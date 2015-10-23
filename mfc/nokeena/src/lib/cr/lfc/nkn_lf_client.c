/**
 * @file   nkn_lf_client.c
 * @author Sunil Mukundan <smukundan@cmbu-build03>
 * @date   Mon Apr 16 17:22:18 2012
 * 
 * @brief  implementes a lf client instance. caller can create,
 * control (i.e START, PAUSE, STOP the client) and delete the
 * client. uses the event_timer API's to  perform periodically fetch
 * information from MFC. uses the http sync read API's to do blocking
 * reads from MFC over HTTP/XML. the LF API XML content is parsed and
 * the lf_stats_t data type is populated and shared with the
 * caller. the 'lf_stats_t' data type will have some key statistics
 * instrumented in the MFC cache.  
 *
 * Object API
 * ==========
 * 1. create - allows creation of a fully instantiated client object
 *
 * Object methods
 * ==============
 * 1. set_state - used to control the state of the client; allowed
 * states are START, IDLE, STOP, where
 *    START - starts data collection from MFC
 *    IDLE - pauses data collection from MFC
 *    STOP - stops data collection from MFC and destoys the object
 * 2. get_state - retrieves the state of the LF client
 *
 * Object Events
 * =============
 * 1. Data Collection Complete - calls the completion handler
 * registerd by the caller along with the registerd data
 * 
 * Dependencies
 * ============
 * 1. Platform - event_timer, nkn_memalloc, nkn_debug, nkn_log
 * 2. Misc - nkn_http_sync_xxxxx APIs
 */
#include "nkn_lf_client.h"

#include "nkn_memalloc.h"
#include "nkn_debug.h"
/* static definition */
static inline int32_t lf_client_set_state(obj_lf_client_t *lfc,
				  lf_client_state_t state);

static inline lf_client_state_t lf_client_get_state(
		    const obj_lf_client_t *const lfc); 

static int32_t lf_client_read_stats(obj_lf_client_t *lfc);

static void lf_client_updater(void *ctx);

static int32_t lf_client_parse_result(const uint8_t *buf, 
		      size_t buf_size, lf_stats_t *lfs);

static int32_t lf_client_delete(obj_lf_client_t *lfc);

static inline void lf_client_set_uri(
		     obj_lf_client_t *lfc,
		     const char *const uri);

static inline void lf_client_set_update_interval(
			 obj_lf_client_t *lfc, 
			 uint32_t update_interval);

/** 
 * creates a fully instantiated LF client. caller needs to provide a
 * valid event_timer_t context in which the LF client will update its
 * state 
 * 
 * @param interval [in] - update interval in milliseconds
 * @param cb [in] - callback to caller context when an LF update is
 * complete. 
 * @param cb_data [in] - caller data to be provided at update callback
 * time
 * @param lf_src_fqdn [in] - the fqdn for the MFC that is reporting
 * stats via LF API
 * @param lf_listen_port [in] - the LF API port in the MFC that is
 * reporting the stats via LF API
 * @param evt [in] - a valid event_timer_t context in which the lf updates
 * will run 
 * @param out [out] - fully instantiated lf client
 * 
 * @return <0 on error and 0 on success
 */
int32_t
lf_client_create(uint32_t interval, lf_client_update_cb_fnp cb,
		 void *cb_data, const char *lf_src_fqdn,
		 uint16_t lf_listen_port, const event_timer_t *evt,
		 obj_lf_client_t **out) 
{
    int32_t err = 0;
    obj_lf_client_t *lfc = NULL;
    
    assert(out);
    *out = NULL;
    
    lfc = (obj_lf_client_t *)
	nkn_calloc_type(1, sizeof(obj_lf_client_t), 100);
    if (!lfc) {
	err = -ENOMEM;
	goto error;
    }
    pthread_mutex_init(&lfc->lock, NULL);
    lfc->update_cb = cb;
    lfc->update_cb_data = cb_data;
    lfc->update_interval = interval;
    lfc->evt = evt;
    lfc->get_state = lf_client_get_state;
    lfc->set_state = lf_client_set_state;
    lfc->get_update = lf_client_read_stats;
    lfc->set_update_interval = lf_client_set_update_interval;
    lfc->set_uri = lf_client_set_uri;
    lfc->delete = lf_client_delete;
    snprintf(lfc->lf_src.fqdn, 256, "%s", lf_src_fqdn);
    lfc->lf_src.port = lf_listen_port;

    *out = lfc;
    return err;

 error:
    if (lfc) lfc->delete(lfc);
    return err;
}

/** 
 * destruction of the lf client object. SHOULD NEVER be called
 * directly as there is a chance that there will be outstanding
 * scheduled events which may then operate on a stale object. use the
 * 'set_state' method to set the state to STOP to trigger a delete. 
 * 
 * @param lfc [in] - a valid lf client instance
 * 
 * @return 
 */
static int32_t
lf_client_delete(obj_lf_client_t *lfc)
{
    if (lfc) {
	assert(lfc->state == LF_CLIENT_STOP);
	free(lfc);
    }
    return 0;
}

/** 
 * handles the state changes for the @obj_lf_client_t instance
 * valid state changes are
 * IDLE -> RUNNING - where the LF API reads are scheduled on the event
 * time attached to the object
 * RUNNING->STOP - stop the LF API client by not scheuduling any
 * further LF API reads. maps to use cases where the object needs to
 * be deleted e.g. config plane events to remove a cache etc
 * RUNNING->IDLE - pauses the LF API client. maps to use cases where
 * the cache goes down and the caller wants to restart after a
 * cool-off period
 * IDLE->STOP - stop a paused LF API client
 * NOTE:
 * 1. only a stopped client can be deleted
 * 2. no state change is permitted when the object is in
 * LF_CLIENT_PROGRESS state
 * 
 * @param lfc [in/out] - a valid instance of @obj_lf_client_t. the
 * object is updated after this function is executed
 * @param state [in] - state change requested
 * 
 * @return ignore for this API
 */
static inline int32_t
lf_client_set_state(obj_lf_client_t *lfc, lf_client_state_t state) 
{
    assert(lfc);

    int32_t err = 0;
    lf_client_state_t curr_state = lfc->state;
    event_timer_t *evt = (event_timer_t *)lfc->evt;

    pthread_mutex_lock(&lfc->lock);
    switch(state) {
	case LF_CLIENT_IDLE:
	    DBG_LOG(MSG, MOD_NETWORK, "LFC state IDLE");
	    assert(curr_state == LF_CLIENT_RUNNING);
	    lfc->state = state;
	    break;
	case LF_CLIENT_RUNNING:
	    assert(curr_state == LF_CLIENT_IDLE);
	    DBG_LOG(MSG, MOD_NETWORK, "LFC state RUNNING");
	    lfc->state = state;
	    evt->add_timed_event_ms(evt, 
		 lfc->update_interval, lf_client_updater, lfc); 
	    break;
	case LF_CLIENT_STOP:
	    assert( (curr_state == LF_CLIENT_IDLE || 
		     curr_state == LF_CLIENT_RUNNING) ); 
	    DBG_LOG(MSG, MOD_NETWORK, "LFC state STOP");
	    lfc->state = state;
	    break;
	default:
	    assert(0);
    }
    pthread_mutex_unlock(&lfc->lock);
    
    return err;
}

static inline lf_client_state_t
lf_client_get_state(const obj_lf_client_t *const lfc)
{
    assert(lfc);
    lf_client_state_t s;
    pthread_mutex_t *lock = &((obj_lf_client_t *)lfc)->lock;
    pthread_mutex_lock(lock);
    s = lfc->state;
    pthread_mutex_unlock(lock);

    return s;
}

/** 
 * set a new update interval to an existing LFC object. this change is
 * thread safe
 * 
 * @param lfc - existing LFC object
 * @param update_interval - new update interval
 */
static inline void
lf_client_set_update_interval(obj_lf_client_t *lfc, 
			      uint32_t update_interval)
{
    pthread_mutex_t *lock = &((obj_lf_client_t *)lfc)->lock;
    pthread_mutex_lock(lock);
    lfc->update_interval = update_interval;
    pthread_mutex_unlock(lock);
    
    return;
}

/** 
 * set a new uri to an existing LFC object. this change is thread safe
 * 
 * @param lfc - existing LFC object
 * @param uri - new uri to be updated
 */
static inline void
lf_client_set_uri(obj_lf_client_t *lfc,
		  const char *const uri)
{
    pthread_mutex_t *lock = &((obj_lf_client_t *)lfc)->lock;
    pthread_mutex_lock(lock);
    snprintf(lfc->lf_src.fqdn, 256, "%s", uri);	     
    pthread_mutex_unlock(lock);
    
    return;

}

/** 
 * obtains cache status using the LF API specifcation. uses the
 * nkn_http_sync_* API's to do HTTP reads. Once the LF API XML is
 * read, it is parsed for stats required and a fully populated
 * lf_stats_t is passed with the registed 'ON COMPLETE' event callback
 * 
 * ASSUMPTIONS:
 * the LF API response will not exceed 32KB
 * 
 * NOTE:
 * 1. the HTTP reads are synchronous and blocking but this will be run
 * in the event timer context and generally should not block other
 * threads
 * 2. HTTP 'keep alive' is not used since LF API does not support it
 * 3. the @ls_stats_t data will be destoyed after the 'ON COMPLETE'
 * event callback returns. for any staggered processing of this data
 * the caller needst to keep a copy of the same
 * 
 * @param lfc [in] - a valid @obj_lf_client_t object
 * 
 * @return 
 */
static int32_t 
lf_client_read_stats(obj_lf_client_t *lfc)
{
    assert(lfc);

    nkn_http_reader_t *hr = NULL;
    int32_t err = 0;
    size_t to_read = 32*1024, rv = 0, buf_size = 32*1024;
    uint8_t buf[32*1024];
    lf_stats_t lfs;

    pthread_mutex_lock(&lfc->lock);
    DBG_LOG(MSG, MOD_NETWORK, "LFC scheduled collection from  LFD "
	    "with uri %s", lfc->lf_src.fqdn);
    hr = nkn_http_sync_open(lfc->lf_src.fqdn, "f", 0);
    if (!hr) {
	DBG_LOG(MSG, MOD_NETWORK, "LFC error connecting to LFD with " 
		"uri %s [err=%d]", lfc->lf_src.fqdn, -errno);		
	pthread_mutex_unlock(&lfc->lock);
	goto error;
    }
    pthread_mutex_unlock(&lfc->lock);
	
    rv = nkn_http_sync_read(buf, 1, to_read, hr);
    buf_size = rv;
    
    nkn_http_sync_close(hr);

    if (buf_size) {
	err = lf_client_parse_result(buf, buf_size, &lfs);
	if (err) {
	    goto error;
	}
    } else {
	DBG_LOG(MSG, MOD_NETWORK, "LFC error collection data from "
		"LFD with uri %s, data size read from LFD is %ld", 
		lfc->lf_src.fqdn, buf_size);

	err = -1;
	goto error;
    }

    if (lfc->update_cb) {
	lfc->update_cb(lfc->update_cb_data, &lfs, sizeof(lf_stats_t));
    }
   
    DBG_LOG(MSG, MOD_NETWORK, "LFC data collection complete from LFD "
	    "with uri %s", lfc->lf_src.fqdn);
    return err;

 error:
    /* we may not have been able to reach the LF API source which means
     * we simply mark the status as LF_UNREACHABLE and call back the
     * complete handler
     */
    if (err != -E_LF_DATA) {
	lfs.status = LF_UNREACHABLE;
    }
    if (lfc->update_cb) {
	lfc->update_cb(lfc->update_cb_data, &lfs,
		       sizeof(lf_stats_t));
    }
    
    return err;
    
}

static int32_t
lf_client_parse_result(const uint8_t *buf, size_t buf_size,
		       lf_stats_t *lfs)
{
    xml_read_ctx_t *xml_ctx = NULL;
    double *cpu_load = &lfs->cpu_load;
    xmlXPathObject *xpath_obj = NULL;
    int32_t err= 0;
    
    xml_ctx = init_xml_read_ctx((uint8_t *)buf, buf_size);
    if (!xml_ctx) {
	err = -E_LF_DATA;
	goto error;
    }

    /* register the LF API namespace before XPath query */
    const char *prefix = "xmlns";
    const char *ns_name = "http://www.juniper.net/mfc/schema/lf";
    err = xmlXPathRegisterNs(xml_ctx->xpath, (xmlChar*)prefix, 
			     (xmlChar*)ns_name);
    if (err) {
	err = -E_LF_DATA;
	goto error;
    }

    /* check cache status */
    lfs->status = CACHE_DOWN;
    xpath_obj = xmlXPathEvalExpression(
		       (xmlChar*)"//xmlns:na", xml_ctx->xpath);
    if (xpath_obj && xpath_obj->nodesetval &&
	xpath_obj->nodesetval->nodeNr) {
	lfs->status = CACHE_UP;
    } else {
	err = -E_LF_DATA;
	goto error;
    }
    xmlXPathFreeObject(xpath_obj);
    xpath_obj = NULL;

    /* check CPU usage */
    xpath_obj = xmlXPathEvalExpression(
       (xmlChar*)"//xmlns:na/xmlns:http/xmlns:cpuUsage", xml_ctx->xpath);
    if (xpath_obj && xpath_obj->nodesetval &&
	xpath_obj->nodesetval->nodeNr) {
	xmlNode *n = xpath_obj->nodesetval->nodeTab[0];
	*cpu_load = 
	    atof((char*)n->children->content);
    } else {
	err = -E_LF_DATA;
	*cpu_load = -1.0;
    }
    
 error:
    if (xpath_obj) xmlXPathFreeObject(xpath_obj);
    if (xml_ctx) xml_cleanup_ctx(xml_ctx);
    return err;
}

/** 
 * the scehduled event handler; if the client is IDLE/PROCESSING a
 * previous LF API read then it does nothing. if the client is RUNNING
 * then it triggers a LF API read
 * 
 * @param ctx [in] - user defined context passed to the event timer
 */
static void
lf_client_updater(void *ctx)
{
    assert(ctx);
    
    obj_lf_client_t *lfc = NULL;
    lf_client_state_t curr_state;
    event_timer_t *evt = NULL;
    int32_t err = 0;

    lfc = (obj_lf_client_t *)ctx;
    evt = (event_timer_t *)lfc->evt;

    curr_state = lfc->get_state(lfc);
    
    switch(curr_state) {
	case LF_CLIENT_IDLE:
	case LF_CLIENT_PROGRESS:
	    evt->add_timed_event_ms(evt,
		 lfc->update_interval, lf_client_updater, lfc);
	    break;
	case LF_CLIENT_RUNNING:
	    err = lfc->get_update(lfc);
	    evt->add_timed_event_ms(evt,
		 lfc->update_interval, lf_client_updater, lfc);
	    break;					 
	case LF_CLIENT_STOP:
	    lfc->delete(lfc);
	    break;
    }		
    
}

#ifdef LF_CLIENT_UT

#include "nkn_mem_counter_def.h"

int nkn_mon_add(const char *name, const char *instance, void *obj,
		uint32_t size)
{   
    return 0;
}

int32_t 
lf_update_complete(void *ctx, const lf_stats_t *const stats, 
		   uint32_t stats_size)
{
    printf("stats done: cpu load = %2.2f\n", stats->cpu_load);
    return 0;
}

int32_t 
lf_update_complete_ut03(void *ctx, const lf_stats_t *const stats, 
		   uint32_t stats_size)
{
    
    lf_cache_status_t *t = (lf_cache_status_t *)ctx;
    
    *t = stats->status;

    printf("stats done: cpu load = %2.2f\n", stats->cpu_load);
    return 0;
}

/* Verfies basic functionality create, execute and delete a
 * obj_lf_client instance
 * 
 * creates NUM_LF_CLIENTS LF API clients and runs then at 500ms
 * intervals. After running for 10s we stop and exit this test
 */
int32_t
LFC_UT_01(void)
{
#define NUM_LF_CLIENTS 200
    obj_lf_client_t *lfc[NUM_LF_CLIENTS] = {NULL};
    int32_t err = 0, i = 0;
    event_timer_t *evt = NULL;
    pthread_t evt_th; 

    evt = createEventTimer(NULL, NULL);
    pthread_create(&evt_th, NULL, evt->start_event_timer, evt);
    sleep(1);

    for (i = 0; i < NUM_LF_CLIENTS; i++) {
	err = lf_client_create(500, lf_update_complete, NULL, 
	       "http://10.102.0.153:2012/lf", 2012, evt, &lfc[i]);
	if (err) {
	    goto error;
	}
	lfc[i]->set_state(lfc[i], LF_CLIENT_RUNNING);
    }
 
   /* run for 10s */
    sleep(10);
    printf("LFC_UT_01: done running for 10s\nStopping the event timer "
	   "and cleaning up the lf clients\n");
 error:
    for (i = 0; i < NUM_LF_CLIENTS; i++) {
	if (lfc[i]) 
	    lfc[i]->set_state(lfc[i], LF_CLIENT_STOP);
    }
    //evt->delete_event_timer(evt);
    //pthread_join(evt_th, NULL);
    return err;
#undef NUM_LF_CLIENTS
}

/* Verfies PAUSE and RESTART functionalities
 * 
 * creates NUM_LF_CLIENTS LF API clients and runs then at 500ms
 * intervals. After running for 10s we PAUSE and after another
 * 5seconds we result the clients
 */
int32_t
LFC_UT_02(void)
{
#define NUM_LF_CLIENTS 200
    obj_lf_client_t *lfc[NUM_LF_CLIENTS] = {NULL};
    int32_t err = 0, i = 0;
    event_timer_t *evt = NULL;
    pthread_t evt_th; 

    printf("LFC_UT_02: starting test \n");
    evt = createEventTimer(NULL, NULL);
    pthread_create(&evt_th, NULL, evt->start_event_timer, evt);
    sleep(1);

    for (i = 0; i < NUM_LF_CLIENTS; i++) {
	err = lf_client_create(500, lf_update_complete, NULL, 
	       "http://10.102.0.153:2012/lf", 2012, evt, &lfc[i]);
	if (err) {
	    goto error;
	}
	lfc[i]->set_state(lfc[i], LF_CLIENT_RUNNING);
    }
 
   /* run for 10s */
    sleep(10);
    printf("LFC_UT_02: done running for 10s\n PAUSING the clients\n");
    for (i = 0; i < NUM_LF_CLIENTS; i++) {
	if (lfc[i]) 
	    lfc[i]->set_state(lfc[i], LF_CLIENT_IDLE);
    }
    printf("LFC_UT_02: will resume after 5s....waiting....\n");
    sleep(5);
    printf("LFC_UT_02: done waiting for 5s, restarting the clients "
	   "for another 10s\n");
    for (i = 0; i < NUM_LF_CLIENTS; i++) {
	if (lfc[i]) 
	    lfc[i]->set_state(lfc[i], LF_CLIENT_RUNNING);
    }
    sleep(10);
    printf("LFC_UT_02: done running for 10s\n STOPPING the clients\n");

 error:
    for (i = 0; i < NUM_LF_CLIENTS; i++) {
	if (lfc[i]) 
	    lfc[i]->set_state(lfc[i], LF_CLIENT_STOP);
    }
    //    evt->delete_event_timer(evt);
    //pthread_join(evt_th, NULL);
    return err;
#undef NUM_LF_CLIENTS
}


int32_t
LFC_UT_03(void) 
{
#define NUM_LF_CLIENTS 1
    obj_lf_client_t *lfc[NUM_LF_CLIENTS] = {NULL};
    int32_t err = 0, i = 0;
    lf_cache_status_t *status[NUM_LF_CLIENTS] = {NULL};
    event_timer_t *evt = NULL;
    pthread_t evt_th; 

    printf("LFC_UT_03: starting test \n");
    evt = createEventTimer(NULL, NULL);
    pthread_create(&evt_th, NULL, evt->start_event_timer, evt);
    sleep(1);

    for (i = 0; i < NUM_LF_CLIENTS; i++) {
	status[i] = (lf_cache_status_t *)
	    calloc(1, sizeof(lf_cache_status_t));
	err = lf_client_create(500, lf_update_complete_ut03, status[i], 
	       "http://10.102.0.153:2012/lf", 2012, evt, &lfc[i]);
	if (err) {
	    goto error;
	}
	lfc[i]->set_state(lfc[i], LF_CLIENT_RUNNING);
    }

    printf("run the clients for 10s\n");
    sleep(10);

    printf("LFC_UT_03: stopping the clients \n");
    for (i = 0; i < NUM_LF_CLIENTS; i++) {
	if (lfc[i]) 
	    lfc[i]->set_state(lfc[i], LF_CLIENT_STOP);
    }

    for (i = 0; i < NUM_LF_CLIENTS; i++) {
	printf("cache state of client %d is %d \n", 
	       i, (int32_t)(*status[i]));
    }

 error:
#undef NUM_LF_CLIENTS	    
    return err;
}
	
int32_t
main(int argc, char *argv[]) 
{

    assert(LFC_UT_01() == 0);
    assert(LFC_UT_02() == 0);
    assert(LFC_UT_03() == 0);
    
    return 0;
}


#endif
