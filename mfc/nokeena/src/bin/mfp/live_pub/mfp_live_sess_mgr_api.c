#include <stdio.h>
#include <sys/time.h>

// mfp includes
#include "mfp_live_accum_ts.h"
#include "mfp_live_event_handler.h"
#include "mfp_live_sess_mgr_api.h"
#include "mfp_live_global.h"
#include "mfp_pmf_parser_api.h"
#include "mfp_mgmt_sess_id_map.h"
#include "mfp_control_pmf_handler.h"
#include "mfp_sl_fmtr_conf.h"
#include "mfp_live_file_pump.h"
#include "thread_pool/mfp_thread_pool.h"
#include "event_timer/mfp_event_timer.h"
#include "mfp_live_accum_create.h"

//nkn includes
#include "nkn_debug.h"
#include "nkn_defs.h"
#include "nkn_stat.h"

#include "nkn_vpe_utils.h"
#include "nkn_vpe_sl_fmtr.h"
#include "nkn_vpe_fmtr.h"

//zvm includes
//#include "zvm_api.h"

#define UDP_BUFSIZE_SEC 2
#define UDP_BUFSIZE_MB 1

uint32_t fruit_init_done = 0;
extern uint64_t glob_mfp_max_sess_supported;
extern file_pump_ctxt_t *file_pump_ctxt;
extern uint32_t glob_mfp_live_stream_timeout;
extern uint32_t glob_enable_session_ha;

extern struct sockaddr_in** glob_net_ip_addr;
/************************************************************
 *  COUNTER DEFS
 ***********************************************************/
NKNCNT_DEF(mfp_live_num_active_sessions, AO_t,\
	"", "total number of conversion sessions started")
NKNCNT_DEF(mfp_live_num_ctrl_sessions, AO_t,\
	"", "total number of control sessions started")
NKNCNT_DEF(mfp_live_num_ses_collision, AO_t,\
	"", "total number of session id collisions")
NKNCNT_DEF(mfp_live_tot_sessions, AO_t,\
	"", "total number of conversion sessions started")
NKNCNT_DEF(mfp_live_tot_err_sessions, AO_t,\
	"", "total number of sessions that exited with errors")
NKNCNT_DEF(mfp_live_num_pmfs_cntr, AO_t,\
	"", "total number of pmfs being processed by this session")
NKNCNT_DEF(mfp_live_tot_pmfs, AO_t,				\
               "", "total number of pmfs handled irrespective of deletion of session")

/********************************************************************
 *                HELPER FUNCTIONS (STATIC)
 *******************************************************************/
static int32_t computeRelatedStreamIdx(stream_param_t const *sp_list,
	int32_t encaps_num,
	int32_t streams_per_encaps);
static void freeRegisteredCtx(void* ctx);

/**
 * starts a live listener session. Does this following sequence of
 * operations
 * a. parses the PMF file and creates a publisher context
 * b. generates a valid session id
 * c. starts the listening session using the event handler API's
 * @param path - path to the PMF file
 */
static void mfp_live_start_listener_session(void *data);

static void registerStrmError(
	uint32_t sess_id, 
	uint32_t idx_off,
	uint32_t *num_strms_registration_failed);

/**
 * initializes the session pipeline. the pipeline basically receives
 * and accumulates data. this function reads the pub context and
 * sessin id creates the listeners and registers the listeners in the
 * dispatcher manager
 * @param sess_id - session id to which this context will be attached
 */
static int32_t initSessionPipeline(uint32_t sess_id);

/**
 * attach a publisher context to a global session id. the function
 * returns the new session id that the publisher context has been
 * attached to
 * @param pub_ctx - publisher context that needs to be attached
 * @return returns the session id (a positive non - zero) integer on
 * success. returns 0 otherwise
 */
static int32_t attachPubContextToSessionId(mfp_publ_t *pub_ctx);

/**
 * setup and FD for multicast recv
 * @param fd - the fd for whom we need to setup multicast receive
 * @param addr - sockaddr structure describing the ip:port tuple
 * @return returns 0 on success and a non-zero number on error
 */

static int8_t multicast_recv_conf(int32_t fd, struct sockaddr_in const* addr,
		struct in_addr const* src_addr,int8_t is_ssm);
/**
 * allocation wrapper for SL sync serializer
 */
static void* syncSerAlloc(uint32_t count, uint32_t size);
   // structure for Sync Serializer error callback ctx
typedef struct sess_cb_ctx {

	uint32_t sess_id;
} sess_cb_ctx_t;

static void sessErrorCallback(void* ctx);

static void sessResyncCallback(void* ctx); 


/**
 * wrapper to post a task to the thread pool
 * @param microseconds - the execution delay in us
 * @param handler - the function to be exectued by the thread pool
 * @param ctx - context to the passed to the handler function
 */
static uint64_t mfpTaskPost(int64_t microseconds,
			    taskHandler handler,
			    void* ctx); 

/**
 * wrapper to post a delayed Event
 * @param s_val - the delay in seconds (measured from 1970) populated
 * in a timeval structure
 * @param task_hdlr - the handler function to be called
 * @param ctx - context to the handler
 */
static void schedTimedEvent(struct timeval const* s_val, 
			    taskHandler  task_hdlr, 
			    void* ctx); 

/******************************************************************
 *                EXTERN DECLARATIONS
 *****************************************************************/
extern int32_t fruit_formatter_init(mfp_publ_t *, uint32_t,
				    register_formatter_error_t, 
				    int32_t);
extern event_timer_t* ev_timer;
extern mfp_thread_pool_t* task_processor;

/*********************************************************************
 * Implements following publisher context API's
 * 1. mfp_live_schedule_live_session - starts a live listener session
 * that will parse a PMF, create a session id and start the event pump
 *********************************************************************/

/**
 * schedules a live listener session. Does this following sequence of
 * operations
 * 1. if the scheduling is set to now then we trigger a session
 * immediately
 * 2. if this is a scehduled event then we sceduled a delayed event;
 * further if the duration is set then we create a pseudo STOP Control
 * PMF and the scehdule a delayed event for the Control PMF
 * @param path - path to the PMF file
 * @return returns 0 on success and a non-zero negative number on error
 */
int32_t mfp_live_schedule_live_session(char *path)
{
    int32_t rv = 0;
    mfp_publ_t *pub = NULL, *ctrl_pub = NULL;
    struct timeval *tv = NULL;
    mfp_live_sess_mgr_cont_t *smc1 = NULL, *smc2 = NULL;
    
    AO_fetch_and_add1(&glob_mfp_live_tot_sessions);
    rv = readPublishContextFromFile(path, &pub);
    if (rv) {
	AO_fetch_and_add1(&glob_mfp_live_tot_err_sessions);
	DBG_MFPLOG ("0", ERROR, MOD_MFPLIVE, "Error Parsing PMF-"
		" err code: %d", rv);
	goto error;
    }

    if (pub->src_type != LIVE_SRC &&
	pub->src_type != CONTROL_SRC) {
	AO_fetch_and_add1(&glob_mfp_live_tot_err_sessions);
	DBG_MFPLOG(pub->name, ERROR, MOD_MFPLIVE, "Incompatible PMF"
		   " type - err code: %d", -E_MFP_PMF_INCOMPAT);
	rv = -E_MFP_PMF_INCOMPAT;
	goto error;
    }
    smc1 = (mfp_live_sess_mgr_cont_t *)					\
	nkn_calloc_type(1, sizeof(mfp_live_sess_mgr_cont_t),
			mod_mfp_live_sess_mgr_cont);
    if (!smc1) {
	rv = -E_MFP_NO_MEM;
	goto error;
    }
    
    smc1->pub = pub;
    if (pub->ssi.st_time) {
	struct timeval tv1;

	memset(&tv1, 0, sizeof(struct timeval));
	gettimeofday(&tv1, NULL);
	tv1.tv_sec += pub->ssi.st_time;
	DBG_MFPLOG(pub->name, MSG, MOD_MFPLIVE, "scheduling live"
		   " session to start with duration %ld (sec) after"
		   " %ld (sec)",  pub->ssi.duration, pub->ssi.st_time);
	schedTimedEvent(&tv1, mfp_live_start_listener_session,
			smc1);

	/** if duration is present then schedule a STOP control PMF
	 * corresponding to the duration of the scheduled session. 
	 * a. create an empty CONTROL publish context
	 * b. set the status STOP with the name tag copied from the
	 *    existing publish context
	 */
	if (pub->ssi.duration) {
	    uint64_t min_duration = 0;

	    min_duration = (pub->ssi.duration <= 60) * 60;
	    smc2 = (mfp_live_sess_mgr_cont_t *)			\
		nkn_calloc_type(1, sizeof(mfp_live_sess_mgr_cont_t),
				mod_mfp_live_sess_mgr_cont);
	    if (!smc2) {
		rv = -E_MFP_NO_MEM;
		goto error;
	    }
	    ctrl_pub = newPublishContext(1, CONTROL_SRC);
	    if (!ctrl_pub) {
		rv = -E_MFP_NO_MEM;
		goto error;
	    }
	    rv = initControlPMF(ctrl_pub, (const char *)pub->name,
				PUB_SESS_STATE_STOP); 
	    if (rv) {
		goto error;
	    }
	    smc2->pub = ctrl_pub;
	    tv1.tv_sec += (pub->ssi.duration + min_duration);
	    AO_fetch_and_add1(&glob_mfp_live_tot_sessions);
	    DBG_MFPLOG(pub->name, MSG, MOD_MFPLIVE, "scheduling session"
		       " stop after %ld (sec)", pub->ssi.duration + min_duration);
	    schedTimedEvent(&tv1, mfp_live_start_listener_session, smc2);
	}
	
    } else {
	/**
	 * trigger a session immediately. this means that either the
	 * shehduling was set to now or there was an absence of the
	 * ScheduleInfo tag
	 */
	mfp_live_start_listener_session((void *)smc1);
    }

    return rv;

  error:
    if (tv) free(tv);
    if (pub) pub->delete_publish_ctx(pub);
    if (ctrl_pub) ctrl_pub->delete_publish_ctx(ctrl_pub);
    if (smc1) free(smc1);
    if (smc2) free(smc2);
    return rv;
}

/**
 * starts a live listener session. Does this following sequence of
 * operations
 * a. parses the PMF file and creates a publisher context
 * b. generates a valid session id
 * c. starts the listening session using the event handler API's
 * @param path - path to the PMF file
 * @return returns 0 on success, -ve integer on error
 */
static void mfp_live_start_listener_session(void *data)
{
    int32_t *rv;
    mfp_publ_t *pub;
    int32_t sess_id;
    mgmt_sess_state_t *sess_obj = NULL;
    mfp_live_sess_mgr_cont_t *smc;

    smc = (mfp_live_sess_mgr_cont_t*)(data);
    rv = &smc->err;
    pub = smc->pub;

    /* add to the mgmt session map */
    if (pub->src_type != CONTROL_SRC) {

	/* attach the pub context to a global session
	 * id
	 */
	sess_id = attachPubContextToSessionId(pub);
	if (sess_id < 0) {
	    *rv = -E_MFP_INVALID_SESS;
	    DBG_MFPLOG ("0", ERROR, MOD_MFPLIVE, "no free session id"
		    " (total concurrent sessions possible %ld) -"
		    " error code: %d" ,
		    glob_mfp_max_sess_supported, *rv);
	    /* no session id, calling the cleanup routine directly */
	    pub->delete_publish_ctx(pub);
	    AO_fetch_and_add1(&glob_mfp_live_tot_err_sessions);
	    free(smc);
	    return;
	}
	sess_obj = mfp_mgmt_sess_tbl_acq((char *)pub->name);
	/**
	 * SCTE DEMO: if there is an existing sess obj for a session
	 * name and the session is not active then we will attach it
	 * to a new session id. Previously we used to return a session
	 * id collision error at this point
	 */
	if (sess_obj) {
	    if (sess_obj->status == PUB_SESS_STATE_REMOVE) {
		mfp_mgmt_sess_replace(sess_obj, sess_id);
	    } else {
		*rv = -E_MFP_INVALID_SESS;
		DBG_MFPLOG (pub->name, ERROR, MOD_MFPLIVE, "Session ID"
			" Collision");
		AO_fetch_and_add1(&glob_mfp_live_num_ses_collision);
		mfp_mgmt_sess_tbl_rel((char *)pub->name);
		free(smc);
		return;
	    }
	    mfp_mgmt_sess_tbl_rel((char *)pub->name);
	} else {
	    mfp_mgmt_sess_tbl_add(sess_id, (char*)pub->name);
	}
    } else {
	AO_fetch_and_add1(&glob_mfp_live_num_ctrl_sessions);
	AO_fetch_and_sub1(&glob_mfp_live_tot_sessions);
	*rv = handleControlPMF(pub, live_state_cont, LIVE_SRC);
	pub->delete_publish_ctx(pub);
	free(smc);
	return;
    }

    /* initialize the session pipeline */
    AO_fetch_and_add1(&glob_mfp_live_num_active_sessions);
    *rv = initSessionPipeline(sess_id);
    if (*rv < 0) {
	DBG_MFPLOG (pub->name, ERROR, MOD_MFPLIVE, "Unable to initialize"
		" session pipeline - error code: %d" ,
		*rv);
	mfp_mgmt_sess_unlink((char*)pub->name, PUB_SESS_STATE_REMOVE,
		*rv);
	live_state_cont->remove(live_state_cont, sess_id);
	AO_fetch_and_add1(&glob_mfp_live_tot_err_sessions);
	AO_fetch_and_sub1(&glob_mfp_live_num_active_sessions);
	free(smc);
	return;
    }

    DBG_MFPLOG (pub->name, MSG, MOD_MFPLIVE, "Session Started Succesfully");

    free(smc);
    return;
}

/**
 * attach a publisher context to a global session id. the function
 * returns the new session id that the publisher context has been
 * attached to
 * @param pub_ctx - publisher context that needs to be attached
 * @return returns the session id (a positive non - zero) integer on
 * success. returns 0 otherwise
 */
static int32_t attachPubContextToSessionId(mfp_publ_t *pub_ctx)
{
    return live_state_cont->insert(live_state_cont,
	    pub_ctx,
	    (ctxFree_fptr)\
	    pub_ctx->delete_publish_ctx);
}

static void registerStrmError(
	uint32_t sess_id, 
	uint32_t idx_off,
	uint32_t *num_strms_registration_failed){

    mfp_publ_t *pub_ctx;
    
    /* retrieve the publisher context from the session id*/
    live_state_cont->acquire_ctx(live_state_cont, sess_id);
    pub_ctx = (mfp_publ_t *)
	live_state_cont->get_ctx(live_state_cont, sess_id);
    live_state_cont->release_ctx(live_state_cont, sess_id);	

    uint32_t* streams_per_encaps = pub_ctx->streams_per_encaps;
    stream_param_t* stream_parm = pub_ctx->stream_parm;	

    stream_parm[idx_off].stream_state = STRM_REGISTRATION_FAILURE;
    streams_per_encaps[0]--;

    DBG_MFPLOG(pub_ctx->name, ALARM, MOD_MFPLIVE, 
    	"sess %u stream %u, STREAM REGISTRATION FAILED",
    	sess_id, idx_off);

    (*num_strms_registration_failed)++;
    
    return;
}

static int32_t createFormatters(
	mfp_publ_t *pub_ctx,
	uint32_t sess_id,
	accum_ts_t *accum){
			
	assert(accum != NULL);

	if(pub_ctx->ms_parm.status == FMTR_ON){
	    int32_t ret;
	    AO_fetch_and_add1(&glob_mfp_live_num_pmfs_cntr);
	    ret = fruit_formatter_init(pub_ctx, sess_id, 
				       accum->error_callback_fn,
				       0);
	    if(ret<0){
	    	if(glob_enable_session_ha){
			pub_ctx->ms_parm.status = FMTR_INIT_FAILURE;
			DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE,
			"sess[%d]: Rx Error (%d) from APPLE formatter INIT",
			sess_id,FMTR_OUTPUT_IO_ERROR);
	    	} else{
			return -E_MFP_FRUIT_INIT;
	    	}
	    }
	}
	

	if (pub_ctx->ss_parm.status  == FMTR_ON){
	    sl_fmtr_publ_type_t type = SL_LIVE;
	    uint32_t dvr_chunk_count = 5;
	    
	    if (pub_ctx->ss_parm.dvr == 1) {
		type = SL_DVR;
		dvr_chunk_count = \
		    pub_ctx->ss_parm.dvr_seconds*1000/ pub_ctx->ss_parm.key_frame_interval;//2000; 
	    }
	    //create sl_fmtr
	    sl_fmtr_t* fmt;
	    fmt = createSLFormatter((char*)pub_ctx->name,
				(char*)pub_ctx->ss_store_url, type,
				    pub_ctx->ss_parm.key_frame_interval,
				    dvr_chunk_count,
					schedTimedEvent, accum->error_callback_fn);
	    if (fmt == NULL) {
		    if(glob_enable_session_ha){
			    pub_ctx->ss_parm.status = FMTR_INIT_FAILURE;
			    DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE,
			    "sess[%d]: Rx Error (%d) from SL formatter INIT",
			    sess_id,FMTR_OUTPUT_IO_ERROR);
		    } else{
			DBG_MFPLOG ("0", ERROR, MOD_MFPLIVE,
				"Silverlight Fmtr init failed");
			return -E_MFP_SL_INIT;
		    }
	    }
	    pub_ctx->sl_publ_fmtr = (void*)(fmt);

	    //create sync ser
	    sess_cb_ctx_t* cb_ctx = mfp_live_calloc(1,
				sizeof(sess_cb_ctx_t));
	    cb_ctx->sess_id = sess_id;
	    pub_ctx->sl_sync_ser = createSyncSerializer(
			pub_ctx->streams_per_encaps[0], 70, mfpTaskPost,
			1, syncSerAlloc, sessErrorCallback,
			sessResyncCallback, (void*)cb_ctx, pub_ctx->name);
   	    if (pub_ctx->sl_sync_ser == NULL) {
		    if(glob_enable_session_ha){
			    pub_ctx->ss_parm.status = FMTR_INIT_FAILURE;
			    DBG_MFPLOG (pub_ctx->name, MSG, MOD_MFPLIVE,
			    "sess[%d]: Rx Error (%d) from SL Syncser INIT",
			    sess_id, FMTR_OUTPUT_IO_ERROR);
		    } else{
			DBG_MFPLOG ("0", ERROR, MOD_MFPLIVE,
				"Silverlight Serial Sync init failed");
			return -E_MFP_SL_INIT;
		    }
	    }
	}
	pub_ctx->formatters_created_flag = 1;
	if(pub_ctx->ms_parm.status == FMTR_INIT_FAILURE
		&& pub_ctx->ss_parm.status == FMTR_INIT_FAILURE){
		//both formatter failed to init
		//no need to start the session
		return -1;
	}
	return 0;
}
	
/**
 * initializes the session pipeline. the pipeline basically receives
 * and accumulates data. this function reads the pub context and
 * sessin id creates the listeners and registers the listeners in the
 * dispatcher manager
 * @param sess_id - session id to which this context will be attached
 */
static int32_t initSessionPipeline(uint32_t sess_id)
{

    mfp_publ_t *pub_ctx;

    /* retrieve the publisher context from the session id*/
    live_state_cont->acquire_ctx(live_state_cont, sess_id);
    pub_ctx = (mfp_publ_t *)
	live_state_cont->get_ctx(live_state_cont, sess_id);
    live_state_cont->release_ctx(live_state_cont, sess_id);

    // Not complete: Formatter init pending, exit logic, return
    // types

    uint32_t encaps_count = pub_ctx->encaps_count;
    uint32_t* streams_per_encaps = pub_ctx->streams_per_encaps;
    uint32_t num_streams = 0;
    stream_param_t* stream_parm = pub_ctx->stream_parm;
    int32_t fd;
    uint32_t num_strms_registration_failed;
    int32_t ret = 0;
    uint32_t i = 0, j, idx_off, encaps_idx_off;
    pub_ctx->formatters_created_flag = 0;

    AO_fetch_and_add1(&glob_mfp_live_tot_pmfs);
    for (; i < encaps_count; i++) {

	encaps_idx_off = idx_off = (i * MAX_STREAM_PER_ENCAP);

	computeRelatedStreamIdx(pub_ctx->encaps[i], i, streams_per_encaps[i]);

	stream_parm = &pub_ctx->stream_parm[idx_off];
	num_streams = streams_per_encaps[i];
	num_strms_registration_failed = 0;

	for (j = 0; j < num_streams; j++) {

	    accum_ts_t *accum;
	    idx_off = j;
	    stream_parm[idx_off].stream_state = STRM_ACTIVE;
	    stream_parm[idx_off].return_code = 0;

	    //check whether file pump is on /off
	    if(!stream_parm[idx_off].file_source_mode){

		//create and bind sockets
		struct sockaddr_in *addr =&(stream_parm[idx_off].media_src.live_src.to);
		struct in_addr *src_addr =
			&(stream_parm[idx_off].media_src.live_src.source_if);
		fd = socket(AF_INET, SOCK_DGRAM, 0);
		if (fd < 0) {
		    perror("socket() failed: ");
		    stream_parm[idx_off].return_code = -E_MFP_SOCK_CREATE;
		    continue;
		}

		int32_t set_reuse = 1;
		int sbuf_size, est_sbuf_size, new_sbuf_size;
		socklen_t sbuf_len = sizeof(int);
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
			    (char *)&set_reuse,
			    sizeof(set_reuse)) < 0) {
		    perror("setsockopt : reuseaddr: ");
		    close(fd);
		    continue;
		}
		if (getsockopt(fd, SOL_SOCKET, SO_RCVBUF, 
			   (void*)(&sbuf_size), &sbuf_len) < 0) {
		    perror("getsockopt : rcvbuf: ");
		    close(fd);
		    continue;
		}

		est_sbuf_size = (UDP_BUFSIZE_MB * 1000000);
		if (est_sbuf_size > sbuf_size) {
		    new_sbuf_size = est_sbuf_size;
		    DBG_MFPLOG(pub_ctx->name, MSG, MOD_MFPLIVE,
			       "existing RCVBUF size: %d, estimated RCVBUF"
			       " size: %d", sbuf_size, est_sbuf_size);
		    if(setsockopt(fd, SOL_SOCKET, SO_RCVBUF,
			      &new_sbuf_size, sizeof(new_sbuf_size))) {
			DBG_MFPLOG(pub_ctx->name, SEVERE, MOD_MFPLIVE,
				   "unable to set RCVBUF size to %d,"
				   " errno %d", new_sbuf_size, errno);
			close(fd);
			continue;
		    }
		}

		if (stream_parm[idx_off].media_src.live_src.is_multicast == 1) {
			if (multicast_recv_conf(fd, addr, src_addr, 
						stream_parm[idx_off].media_src.live_src.is_ssm) < 0) {
				close(fd);
				registerStrmError(sess_id, idx_off, &num_strms_registration_failed);
				stream_parm[idx_off].return_code =  -E_MFP_MULT_CONF;
				continue;
			}
		} else {
		    if (bind(fd, (struct sockaddr*)addr,
				sizeof(struct sockaddr)) < 0) {
			perror("bind() failed: ");
			stream_parm[idx_off].media_src.live_src.fd = -1;
			close(fd);
			registerStrmError(sess_id, idx_off, &num_strms_registration_failed);
			stream_parm[idx_off].return_code =  -E_MFP_BIND_FAIL;
			continue;
		    }
		}
	    }else{

		//FILE PUMP open
		fd = open((const char *)stream_parm[idx_off].inp_file_name, O_RDONLY, S_IRUSR);
		if(fd < 0){
		    printf("file open %s failed\n", stream_parm[idx_off].inp_file_name);
		    registerStrmError(sess_id, idx_off, &num_strms_registration_failed);
		    stream_parm[idx_off].return_code =  -E_MFP_INVALID_FILE;
		    continue;
		}
	    }
	    stream_parm[idx_off].media_src.live_src.fd = fd;
	    //stream_parm[idx_off].stream_state = STRM_ACTIVE;

	    // Assign correct accumulators to the streams
	    /*
		Note: For now, we have only one accumulator. Later,
		When we add more accumulator support, the below
		assignment will depend on encaps_type, media_type
		and encoding type.
	     */
	    sess_stream_id_t* context_data =
		mfp_live_calloc(1, sizeof(sess_stream_id_t));
	    if (!context_data) {
		return -E_MFP_NO_MEM;
	    }
	    context_data->sess_id = sess_id;
	    context_data->stream_id = idx_off - num_strms_registration_failed +
		(i * MAX_STREAM_PER_ENCAP);
	    gettimeofday(&(context_data->last_seen_at), NULL);

	    switch (stream_parm[encaps_idx_off].encaps_type) {
		case TS:
		    accum = newAccumulatorTS(context_data, LIVE_SRC);
		    if(!accum){
		    	return E_MFP_ACCUM_INIT;
		    }
		    pub_ctx->accum_ctx[idx_off - num_strms_registration_failed] = accum;
		    pub_ctx->accum_intf[idx_off - num_strms_registration_failed] = &(accum->accum_intf);
		    context_data->sess_id = sess_id;
		    context_data->stream_id = idx_off - num_strms_registration_failed +
			(i * MAX_STREAM_PER_ENCAP);
		    gettimeofday(&(context_data->last_seen_at), NULL);
		    if(!pub_ctx->formatters_created_flag){
			    ret = createFormatters(pub_ctx, sess_id, accum);
			    if(ret < 0)
			   	return (ret);
		    }
		    break;
		default:
		    return 0;
	    }

	    if(!stream_parm[idx_off].file_source_mode){

		// register for n/w event and attach appropriate hndlrs
		entity_context_t* entity_ctx = newEntityContext(fd,
			context_data, freeRegisteredCtx,
			mfpLiveEpollinHandler, mfpLiveEpolloutHandler,
			mfpLiveEpollerrHandler, mfpLiveEpollhupHandler,
			mfpLiveTimeoutHandler, disp_mngr);

		entity_ctx->event_flags |= EPOLLIN;
		gettimeofday(&(entity_ctx->to_be_scheduled), NULL);
		entity_ctx->to_be_scheduled.tv_sec += glob_mfp_live_stream_timeout;

		disp_mngr->register_entity(entity_ctx);
		pub_ctx->active_fd_count++;

	    } else{
		//register fd with file pump
		file_pump_ctxt->insert_entry(file_pump_ctxt, 
			stream_parm[idx_off].media_src.live_src.fd, 
			stream_parm[idx_off].bit_rate, 
			stream_parm[idx_off].file_pump_mode,
			pub_ctx, context_data);
	    }


	}
	
	if(num_strms_registration_failed == num_streams){
	     //handle case where all streams failed
	     for (j = 0; j < num_streams; j++) {
	     	if(stream_parm[j].stream_state == STRM_REGISTRATION_FAILURE)
			return stream_parm[j].return_code;
	     }	     
	}
	
    }
    return 1;
}

#define COMPUTE_GLOB_STREAM_ID(_encap_no, _idx)	\
    ((_encap_no*MAX_STREAM_PER_ENCAP) + _idx)
#define COMPUTE_LOCAL_STREAM_ID(_encap_no, _idx)	\
    (_idx - (_encap_no*MAX_STREAM_PER_ENCAP))

/**
 * computes the relationship between the audio and the video
 * streams. for every audio stream it computes the corresponding
 * audio stream and a bi-directional relationship is stored. this
 * function is called if there is no relationship specified explicitly
 * in the PMF file
 * @param sp_list - list of all the stream param contexts in an
 * encapsulation
 * @param encaps_num - the index of the encapsulation that is going to
 * be processed
 * @param streams_per_encaps - number of stream param contexts in a
 * specific encapsulation
 * @return - returns 0 on success and negative integer on error
 */
static int32_t computeRelatedStreamIdx(stream_param_t const *sp_list,
	int32_t encaps_num,
	int32_t streams_per_encaps)
{
    int32_t j, vid_stream_count, aud_stream_count,
    vid_stream_idx[MAX_STREAM_PER_ENCAP],
    aud_stream_idx[MAX_STREAM_PER_ENCAP],
    err, n_elem_per_bucket, idx;
    stream_param_t *sp;

    /* initialization */
    j = 0;
    err = 0;
    vid_stream_count = aud_stream_count = 0;
    n_elem_per_bucket = idx = 0;

    /* Algorithm to partition
     * divide the video streams into buckets. the number of
     * buckets will be equal to the number of audio streams to be
     * mapped to. all the streams in a bucket will be mapped to
     * their corresponding audio stream and vice versa
     */

    /* partition the audio and video streams */
    for (j = 0; j < streams_per_encaps; j++) {
	sp = (stream_param_t*)(&sp_list[j]);
	if(sp->med_type == VIDEO) {
	    vid_stream_idx[vid_stream_count] =
		COMPUTE_GLOB_STREAM_ID(encaps_num, j);
	    vid_stream_count++;
	} else if (sp->med_type == AUDIO) {
	    aud_stream_idx[aud_stream_count] =
		COMPUTE_GLOB_STREAM_ID(encaps_num, j);
	    aud_stream_count++;
	}
    }

    /* May be MUX/ VIDEO ONLY/ AUDIO only in which case there will
     * not be any related stream id, safe to return without any
     * further processing
     */
    if ( !aud_stream_count || !vid_stream_count) {
	return err;
    }

    /* compute the number of video streams that are to be related
     * to each audio stream */
    n_elem_per_bucket = \
			(vid_stream_count + aud_stream_count - 1) / aud_stream_count;

    /* create the mapping */
    for (j = 0; j < vid_stream_count; j++) {
	/* create video to audio mapping */
	idx = vid_stream_idx[j];
	sp = (stream_param_t*)\
	     (&sp_list[COMPUTE_LOCAL_STREAM_ID(encaps_num,idx)]);
	sp->related_idx[sp->n_related_streams] = \
						 //					 aud_stream_idx[j / n_elem_per_bucket];
						 aud_stream_idx[0];
	sp->n_related_streams++;

	/* create audio to video mapping */
	idx = aud_stream_idx[0];//j / n_elem_per_bucket];
	sp = (stream_param_t*)\
	     (&sp_list[COMPUTE_LOCAL_STREAM_ID(encaps_num,idx)]);
	sp->related_idx[sp->n_related_streams] = \
						 vid_stream_idx[j];
	sp->n_related_streams++;
    }

    return err;
}

static void freeRegisteredCtx(void* ctx)
{

    sess_stream_id_t* id = (sess_stream_id_t*)ctx;
    free(id);
}


static int8_t multicast_recv_conf(int32_t fd, struct sockaddr_in const* addr,
		struct in_addr const* src_addr,int8_t is_ssm)
{

	struct sockaddr_in local_addr;
	local_addr.sin_family = AF_INET;
	local_addr.sin_port = addr->sin_port;
	local_addr.sin_addr.s_addr = addr->sin_addr.s_addr;

	int32_t rc = bind(fd, (struct sockaddr *)&local_addr,
			sizeof(struct sockaddr_in));
	if (rc < 0) {
		perror("bind: ");
		return -2;
	}
	struct ip_mreq m_ip;
	struct ip_mreq_source m_ip_src;
	if (is_ssm == 1) {
		if (src_addr->s_addr == 0)
			return -3;
		m_ip_src.imr_multiaddr.s_addr = addr->sin_addr.s_addr;
		m_ip_src.imr_sourceaddr.s_addr =src_addr->s_addr;
	} else {
		m_ip.imr_multiaddr.s_addr = addr->sin_addr.s_addr;
	}
	if (glob_net_ip_addr == NULL) {

		if (is_ssm == 1) {
			m_ip_src.imr_interface.s_addr = htonl(INADDR_ANY);
			rc = setsockopt(fd, IPPROTO_IP, IP_ADD_SOURCE_MEMBERSHIP,
					&m_ip_src, sizeof(struct ip_mreq_source));
		} else {
			m_ip.imr_interface.s_addr = htonl(INADDR_ANY);
			rc = setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
					&m_ip, sizeof(struct ip_mreq));
		}
		if (rc < 0) {
			perror("setsockopt: add_membership: ");
			return -4;
		}
	} else {
		int32_t i = 0;
		while(glob_net_ip_addr[i] != NULL) {
			if (is_ssm == 1) {
				m_ip_src.imr_interface.s_addr =
					glob_net_ip_addr[i]->sin_addr.s_addr; 
				rc = setsockopt(fd, IPPROTO_IP, IP_ADD_SOURCE_MEMBERSHIP,
						&m_ip_src, sizeof(struct ip_mreq_source));
			} else {
				m_ip.imr_interface.s_addr =
					glob_net_ip_addr[i]->sin_addr.s_addr; 
				rc = setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
						&m_ip, sizeof(struct ip_mreq));
			}
			if (rc < 0) {
				perror("setsockopt: add_membership: ");
				return -4;
			}
			i++;
		}
	}
	return 1;
}


static void sessResyncCallback(void* ctx) {
    printf("Sync Ser: Re sync session\n");
    sess_cb_ctx_t* cb_ctx = (sess_cb_ctx_t*)ctx;
    uint32_t sess_id = cb_ctx->sess_id;
    mfp_publ_t* publ_ctx = live_state_cont->get_ctx(live_state_cont, sess_id);
    publ_ctx->sl_publ_fmtr->reset_state(publ_ctx->sl_publ_fmtr);
}

static uint64_t mfpTaskPost(int64_t microseconds,
		    taskHandler handler, void* ctx) {

	thread_pool_task_t* t_task = newThreadPoolTask(handler, ctx, NULL);
	task_processor->add_work_item(task_processor, t_task);
	return 0;
}

static void schedTimedEvent(struct timeval const* s_val, taskHandler task_hdlr,
		void* ctx) {

	ev_timer->add_timed_event(ev_timer, s_val, task_hdlr, ctx);

}

static void* syncSerAlloc(uint32_t count, uint32_t size) {

	return nkn_calloc_type(count, size, mod_mfp_sync_ser);
}

static void sessErrorCallback(void* ctx) {

	printf("Sync Ser: Cleaning up session\n");
	sess_cb_ctx_t* cb_ctx = (sess_cb_ctx_t*)ctx;
	uint32_t sess_id = cb_ctx->sess_id;
	live_state_cont->acquire_ctx(live_state_cont, sess_id);
	live_state_cont->mark_inactive_ctx(live_state_cont, sess_id);
	live_state_cont->release_ctx(live_state_cont, sess_id);
	free(cb_ctx);
}
