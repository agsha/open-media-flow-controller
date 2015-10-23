/**
 * @file   lf_na_connector.c
 * @author Sunil Mukundan <smukundan@cmbu-build03>
 * @date   Tue Mar 13 17:14:53 2012
 * 
 * @brief  Native Applications (NA) metrics collection and statistics
 * computation.
 * Object Model:
 * implements the callbacks for the lf_na_connector_t object which in
 * turn enumerates and creates a application specific connector object
 * for every application
 * the 'HTTP' application has a lf_na_http_metrics_t object which
 * implements all the lf_na_connector_t callbacks. it also maintains
 * the data collection and statistics computation states. for results
 * sharing it has a circular FIFO buffer (depth 5) which it shares
 * with a reference count with the presentation layer 
 * 
 */
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <pthread.h>
#include <stdint.h>

// nkn defs
#include "nkn_memalloc.h"
#include "nkn_defs.h"
#include "nkn_stat.h"

// lf defs
#include "lf_na_connector.h"
#include "lf_common_defs.h"
#include "lf_proc_utils.h"
#include "lf_err.h"
#include "lf_dbg.h"

#define LF_NA_NO_TOKEN (-1)

const uint32_t section_id = LF_NA_SECTION;
const char *disk_tier_name[/*lf_na_http_disk_tier_t*/] = {
    "ssd", "sas", "sata"
};

const char 
*disk_tier_get_q_counter_name[/*lf_na_http_disk_tier_t*/] = {
    "glob_mm_ssd_get_queue_cnt",
    "glob_mm_sas_get_queue_cnt",
    "glob_mm_sata_get_queue_cnt"
};

const char 
*disk_tier_num_get_threads_counter_name[/*lf_na_http_disk_tier_t*/] = {
    "glob_mm_ssd_num_get_threads",
    "glob_mm_sas_num_get_threads",
    "glob_mm_sata_num_get_threads"
};

const char 
*disk_tier_get_q_sac_counter_name[/*lf_na_http_disk_tier_t*/] = {
    "glob_mm_ssd_sac_limit",
    "glob_mm_sas_sac_limit",
    "glob_mm_sata_sac_limit"
};

const char *disk_tier_avl_counter_name[/*lf_na_http_disk_tier_t*/] = { 
    "glob_dm2_ssd_tier_avl", 	/* LF_NA_HTTP_DISK_TIER_SSD */
    "glob_dm2_sas_tier_avl",	/* LF_NA_HTTP_DISK_TIER_SAS */
    "glob_dm2_sata_tier_avl"	/* LF_NA_HTTP_DISK_TIER_SATA */
};
const char *disk_tier_preread_counter_name[/*lf_na_http_disk_tier_t*/] = { 
    "glob_dm2_ssd_preread_done", 	/* LF_NA_HTTP_DISK_TIER_SSD */
    "glob_dm2_sas_preread_done",	/* LF_NA_HTTP_DISK_TIER_SAS */
    "glob_dm2_sata_preread_done"	/* LF_NA_HTTP_DISK_TIER_SATA */
};
   
extern uint32_t g_lfd_processor_count;
extern lf_app_metrics_intf_t *g_app_intf[LF_MAX_SECTIONS * LF_MAX_APPS];
static int32_t lf_na_http_metrics_create(void *inp, void **out);
static int32_t lf_na_http_metrics_update(void *app_conn, 
					 void *app_metrics_ctx);
static int32_t lf_na_http_metrics_hold(void *app_metrics_ctx);
static int32_t lf_na_http_metrics_release(void *app_metrics_ctx);
static int32_t lf_na_http_metrics_get_out_size(void *app_metrics_ctx,
					       uint32_t *out_size);
static void lf_na_http_metrics_cleanup(void *app_metrics_ctx);
static int32_t lf_na_http_metrics_copy(const void *app_metrics_ctx, 
				       void *const out);

lf_app_metrics_intf_t lf_na_http_metrics_cb = {
    .create = lf_na_http_metrics_create,
    .cleanup = lf_na_http_metrics_cleanup,
    .update = lf_na_http_metrics_update,
    .get_out_size = lf_na_http_metrics_get_out_size,
    .release = lf_na_http_metrics_release,
    .copy = lf_na_http_metrics_copy,
    .hold = lf_na_http_metrics_hold
};

static uint32_t lf_na_get_app_count(const void *ctx);
static int32_t lf_na_get_snapshot(void *const nac,
				  void **out, void **ref_id);
static int32_t lf_na_get_filter_snapshot(void *const ctx,
					 void **const filter,
					 void **out, 
					 void **ref_id);
static int32_t lf_na_release_snapshot(void *lfm,
				      void *ref_id);
static int32_t lf_na_get_free_token(lf_na_connector_t *nac);
static int32_t lf_na_update_all_app_metrics(uint64_t sample_duration,
					    const void *ctx); 
static int32_t lf_na_connect(const char *const app_name,
			     nkncnt_client_ctx_t *out);

/* HTTP APP: RP STATISTICS COMPUTATION */
static int32_t lf_na_http_metrics_get_rp_list(
				      lf_na_http_metrics_ctx_t *hm,
				      nkncnt_client_ctx_t *nc,
				      cp_vector *rp_list);
static int32_t lf_na_http_strip_rp_name(const char *str, 
					uint32_t base_name_len,
					char **out);
static inline int32_t lf_na_http_fill_rp_stats(
		       lf_na_http_metrics_ctx_t *hm,
		       nkncnt_client_ctx_t *nc);

/* HTTP APP: SERVICE STATE AND CPU USAGE */
static inline int8_t lf_na_http_detect_service_state(
			     nkncnt_client_ctx_t *nc); 
static int32_t lf_na_http_calc_pid_list_lf(lf_na_http_metrics_ctx_t *hm,
					   struct pid_stat_list_t *head,
					   double *out);

/* HTTP APP: DISK STATISTICS COMPUTATION */
static inline int32_t lf_na_http_fill_disk_io_params(
				nkncnt_client_ctx_t *nc, 
				lf_na_http_metrics_ctx_t *hm);
static inline int32_t lf_na_http_fill_disk_get_q_sac(
			     nkncnt_client_ctx_t *nc, 
			     lf_na_http_metrics_ctx_t *hm);
static inline int8_t lf_na_http_get_tier_preread_state(
	       nkncnt_client_ctx_t *nc, lf_na_http_disk_tier_t tier);
static inline int8_t lf_na_http_fill_preread_state(
	   nkncnt_client_ctx_t *nc, lf_na_http_metrics_ctx_t *hm);
static inline int32_t lf_na_http_fill_disk_usage(
			 lf_na_http_metrics_ctx_t *hm);

/* HTTP APP: RAM CACHE USAGE COMPUTATION */
static inline int32_t lf_na_http_fill_ram_cache_usage(nkncnt_client_ctx_t *nc,
					      lf_na_http_metrics_ctx_t *hm);

/* HTTP APP: GATHER STATS FOR A LIST OF NAMESPACES */
static int32_t lf_na_populate_filter_counter_list(
				  struct cp_vec_list_t *counter_list, 
				  struct str_list_t *filter_list,
				  nkncnt_client_ctx_t *nc);

/* HTTP APP: VIP STATISTICS COMPUTATION */
static int32_t lf_na_http_metrics_get_vip_list(
			       lf_na_http_metrics_ctx_t *hm,
			       nkncnt_client_ctx_t *nc,
			       cp_vector *vip_list);
int32_t lf_na_http_fill_vip_stats(lf_na_http_metrics_ctx_t *hm,
				  nkncnt_client_ctx_t *nc);

/* CP TRIE CALLBACKS */
static void *trie_copy_func(void *nd);
static void trie_destruct_func(void *nd);

int32_t
lf_na_connector_create(
      const lf_connector_input_params_t *const inp,
      void **out)
{
    int32_t err = 0;
    uint32_t i, j;
    lf_na_connector_t *nac;
    lf_app_metrics_intf_t *app_metrics_intf = NULL;
    uint8_t *buf = NULL;
    struct iovec *vec = NULL;
    uint32_t out_size = 0;

    assert(inp);
    if (inp->sampling_interval < LF_MIN_SAMPLING_RES ||
	inp->window_len < LF_MIN_WINDOW_LEN ||
	inp->sampling_interval > LF_MAX_SAMPLING_RES ||
	inp->window_len > LF_MAX_WINDOW_LEN ||
	inp->num_tokens > LF_MAX_CLIENT_TOKENS) {
	err = -E_LF_INVAL;
	goto error;
    }

    nac = (lf_na_connector_t*)
	nkn_calloc_type(1, sizeof(lf_na_connector_t),
			100);
    if (!nac) {
	err = -E_LF_NO_MEM;
	goto error;
    }

    nac->num_apps = inp->app_list->num_apps;
    nac->get_app_count = lf_na_get_app_count;
    nac->sampling_interval = inp->sampling_interval;
    nac->window_len = inp->window_len;
    nac->num_tokens = inp->num_tokens;
    nac->update_all_app_metrics = lf_na_update_all_app_metrics;
    nac->get_snapshot = lf_na_get_snapshot;
    nac->get_filter_snapshot = lf_na_get_filter_snapshot;
    nac->release_snapshot = lf_na_release_snapshot;
    nac->curr_token = LF_NA_NO_TOKEN;
    nac->ext_window_len = inp->ext_window_len;
    pthread_mutex_init(&nac->token_ref_cnt_lock, NULL);

    for (i = 0; i < nac->num_apps; i++) {
	err = lf_na_connect((const char *)inp->app_list->name[i],
			    &nac->nc[i]);
	if (err) {
	    goto error;
	}
	
	/* create the metrics objects */
	app_metrics_intf = lf_app_metrics_intf_query(g_app_intf,
						     LF_NA_SECTION, i);
	err = app_metrics_intf->create(nac,
				       &nac->app_specific_metrics[i]);
	if (err) {
	    goto error;
	}
	app_metrics_intf->get_out_size(nac->app_specific_metrics[i],
				       &out_size);

	/* allocate and mark output buffers */
	assert(out_size);
	buf = (uint8_t*)nkn_malloc_type(nac->num_tokens * out_size, 100);
	if (!buf) {
	    err = -E_LF_NO_MEM;
	    goto error;
	}
	vec = (struct iovec *)
	    nkn_calloc_type(nac->num_tokens, sizeof(struct iovec), 100);
	if (!vec) {
	    err = -E_LF_NO_MEM;
	    goto error;
	}
	for (j = 0; j < nac->num_tokens; j++) {
	    vec[j].iov_base = buf + (j * out_size);
	    vec[j].iov_len = out_size;
	    nac->token_ref_cnt[j] = LF_NA_NO_TOKEN;
	}
	nac->out[i] = vec;
    }

    *out = nac;
    return err;
 error:
    lf_na_connector_cleanup(nac);
    return err;
}

int32_t 
lf_na_connector_cleanup(lf_na_connector_t *nac)
{
    uint32_t i;
    lf_app_metrics_intf_t *app_metrics_intf = NULL;
    
    if (nac) {
	for (i = 0; i < nac->num_apps; i++) {
	    /* create the metrics objects */
	    app_metrics_intf = lf_app_metrics_intf_query(g_app_intf,
							 LF_NA_SECTION, i);
	    assert(app_metrics_intf);
	    if (app_metrics_intf->cleanup) 
		app_metrics_intf->cleanup(nac->app_specific_metrics[i]);
	    
	    if(nac->out[i]) {
		if (nac->out[i][0].iov_base) {
		    free(nac->out[i][0].iov_base);
		}
	    }
	}
	free(nac);
    }

    return 0;
}

/**
 * returns a snapshot of the counter set filtered based on the filter
 * provided
 * @ctx [in] - the na connector context
 * @filter [in] - the set of resource pool, namespaces whose counter
 * set need to be returned. the data organization for this is as
 * follows. @filter is an array of 2 lists, one each for resource
 * pools and namespaces respectively. the list is a string list of
 * type str_list_t. if either the namespace or resource pool filters
 * are absent, then their corresponding entry in the array needs to be
 * marked as NULL.
 * @out [out] - the output data which is organized as follows. the
 * output consists of a array of two lists; one each for resource pool
 * and namespaces. the list is of type cp_vector which itself is a
 * list of g_items (nkncnt type)
 * out_list[rp_filters/ns_filters] = {cp_vector[filter(i)] = {g_items[filters(i)]}}
 * out = {out_list[rp_filters], out_list[ns_filter]}
 */
static int32_t
lf_na_get_filter_snapshot(void *const ctx,
			  void **const filter,
			  void **out, 
			  void **ref_id)
{
    int32_t err = 0;
    lf_na_connector_t *nac = NULL;
    struct str_list_t **head, *rp_filter_list = NULL, 
	*ns_filter_list = NULL;
    struct cp_vec_list_t *cpv_list_rp = NULL, *cpv_list_ns = NULL;
    lf_na_filter_out_t *filter_out = NULL;
    nkncnt_client_ctx_t *nc = NULL;

    assert(ctx);
    assert(out);
    assert(filter);
    assert(ref_id);

    nac = (lf_na_connector_t *)ctx;
    head = (struct str_list_t **)filter;
    rp_filter_list = head[LF_NA_RP_FILTER_ID];
    ns_filter_list = head[LF_NA_NS_FILTER_ID];
    nc = nac->nc;

    filter_out = (lf_na_filter_out_t *)
	nkn_calloc_type(1, sizeof(lf_na_filter_out_t), 100);
    if (!filter_out) {
	err = -E_LF_NO_MEM;
	goto error;
    }

    /* initializing the cp_vector list for resource pool filtered
     * counter set and populating the counter set
     */
    cpv_list_rp = (struct cp_vec_list_t *)
	nkn_calloc_type(1, sizeof(struct cp_vec_list_t), 100);
    if (!cpv_list_rp) {
	err = -E_LF_NO_MEM;
	goto error;
    }
    TAILQ_INIT(cpv_list_rp);
    err = lf_na_populate_filter_counter_list(cpv_list_rp, 
					     rp_filter_list, 
					     nac->nc);
    if (err) {
	goto error;
    }

    /** initializing the cp_vector list for namespce filtered counter
     * and populating the counter set       
     */
    cpv_list_ns = (struct cp_vec_list_t *)
	nkn_calloc_type(1, sizeof(struct cp_vec_list_t), 100);
    if (!cpv_list_ns) {
	err = -E_LF_NO_MEM;
	goto error;
    }
    TAILQ_INIT(cpv_list_ns);
    err = lf_na_populate_filter_counter_list(cpv_list_ns, 
					     ns_filter_list, 
					     nac->nc);
    if (err) {
	goto error;
    }

    /* setup output counter list */
    filter_out->counter_name_offset = 
	(char *)(nc->shm +  sizeof(nkn_shmdef_t) + 
		 sizeof(glob_item_t) * nc->max_cnts);
    filter_out->filter_out[LF_NA_RP_FILTER_ID] = cpv_list_rp;
    filter_out->filter_out[LF_NA_NS_FILTER_ID] = cpv_list_ns;
    *out = filter_out;
    
    return err;
 error:
    if (cpv_list_rp) lf_cleanup_cp_vec_list(cpv_list_rp);
    if (cpv_list_ns) lf_cleanup_cp_vec_list(cpv_list_ns);
    return err;
}

/**
 * populates a the vector list with a filtered counter set
 * @counter_list [in/out] - the vector list which needs to be populated
 * with a counter set for each vector
 * @filter_list [in] - filter list of counter names
 * @nc [in] - the nkncnt client context which holds the counter set state
 * @return returns 0 on success and a non zero negative value on error
 */
static int32_t
lf_na_populate_filter_counter_list(struct cp_vec_list_t *counter_list, 
				   struct str_list_t *filter_list,
				   nkncnt_client_ctx_t *const nc)
{
    cp_vector *matches = NULL;
    char str[64] = {0};
    str_list_elm_t *elm = NULL;
    int32_t err = 0;

    if (!filter_list) {
	/* nothing to do */
	return err;
    }
    TAILQ_FOREACH(elm, filter_list, entries) {
	cp_vec_list_elm_t *cpv_list_elm = NULL;
	snprintf(str, 64, "%s", elm->name);
	err = nkncnt_client_get_submatch(nc, str, &matches);
	if (err) {
	    goto error;
	}
	cpv_list_elm = (cp_vec_list_elm_t *)
	    nkn_calloc_type(1, sizeof(cp_vec_list_elm_t), 100);
	if (!cpv_list_elm) {
	    err = -E_LF_NO_MEM;
	    goto error;
	}
	cpv_list_elm->v = matches;
	TAILQ_INSERT_TAIL(counter_list, cpv_list_elm, entries);
    }
 error:
    return err;    
}

int32_t
lf_cleanup_cp_vec_list(struct cp_vec_list_t *head)
{
    cp_vec_list_elm_t *elm;
    TAILQ_FOREACH(elm, head, entries) {
	if(elm->v) cp_vector_destroy(elm->v);
    }
    if (head) free(head);

    return 0;
}


static int32_t
lf_na_get_snapshot(void *const ctx,
		   void **out, void **ref_id)
{
    int32_t err = 0, free_token = LF_NA_NO_TOKEN;
    uint32_t i;
    uint64_t *token_no = NULL;
    lf_na_connector_t *nac = NULL;

    assert(ctx);
    assert(out);
    assert(ref_id);

    nac = (lf_na_connector_t *)ctx;
    token_no = (uint64_t *)
	nkn_calloc_type(1, sizeof(uint64_t), 100);
    if (!token_no) {
	err = -E_LF_NO_MEM;
	goto error;
    }

    pthread_mutex_lock(&nac->token_ref_cnt_lock);
    free_token = nac->curr_token;
    if (free_token < 0) {
	pthread_mutex_unlock(&nac->token_ref_cnt_lock);
	err = -E_LF_NA_NO_FREE_TOKEN;
	goto error;
    }
    for (i = 0; i < nac->num_apps; i++) {
	(nac->token_ref_cnt[free_token])++;
	*out = nac->out[i][free_token].iov_base;
	*token_no = free_token;
	*ref_id = token_no;
    }
    pthread_mutex_unlock(&nac->token_ref_cnt_lock);

 error:
    return err;
}

static int32_t
lf_na_release_snapshot(void *ctx,
		       void *ref_id)
{
    uint64_t *free_token = NULL;
    lf_na_connector_t *nac = NULL;

    nac = (lf_na_connector_t *)ctx;

    if (nac && ref_id) {
	free_token = (uint64_t*)ref_id;
	pthread_mutex_lock(&nac->token_ref_cnt_lock);
	(nac->token_ref_cnt[*free_token])--;
	assert(nac->token_ref_cnt[*free_token] >= 0);
	pthread_mutex_unlock(&nac->token_ref_cnt_lock);
	free(free_token);
    }
    
    return 0;
}

static uint32_t
lf_na_get_app_count(const void *ctx) 
{
    lf_na_connector_t *nac = NULL;

    assert(ctx);
    nac = (lf_na_connector_t *)ctx;
    
    return nac->num_apps;
}

static int32_t 
lf_na_update_all_app_metrics(uint64_t sample_duration, 
			     const void *ctx)
{
    lf_na_connector_t *nac = NULL;
    lf_app_metrics_intf_t *app_metrics_intf = NULL;
    int32_t err = 0, free_token = LF_NA_NO_TOKEN;
    uint32_t i;

    assert(ctx);
    nac = (lf_na_connector_t *)ctx;

    for (i = 0; i < nac->num_apps; i++) {
	app_metrics_intf = lf_app_metrics_intf_query(g_app_intf,
						     LF_NA_SECTION, i);
	app_metrics_intf->hold(nac->app_specific_metrics[i]);
	err = app_metrics_intf->update(&nac->nc[i],
			 nac->app_specific_metrics[i]);
	if (err) {
	    app_metrics_intf->release(nac->app_specific_metrics[i]);
	    continue;
	}
	pthread_mutex_lock(&nac->token_ref_cnt_lock);
	free_token = lf_na_get_free_token(nac);
	if (free_token < 0) {
	    pthread_mutex_unlock(&nac->token_ref_cnt_lock);
	    app_metrics_intf->release(nac->app_specific_metrics[i]);
	    err = -E_LF_NA_NO_FREE_TOKEN;
	    goto error;
	}
	pthread_mutex_unlock(&nac->token_ref_cnt_lock);
	app_metrics_intf->copy(nac->app_specific_metrics[i],
			       nac->out[i][free_token].iov_base);
	app_metrics_intf->release(nac->app_specific_metrics[i]);
    }

 error:
    return err;
}

static int32_t
lf_na_get_free_token(lf_na_connector_t *nac)
{
    int32_t rv = 0, found = 0;
    uint32_t i;

    for (i = 0; i < nac->num_tokens; i++) {
	if (!nac->token_ref_cnt[i] ||
	    nac->token_ref_cnt[i] == LF_NA_NO_TOKEN) {
	    nac->token_ref_cnt[i] = 0;
	    rv = i;
	    found = 1;
	    LF_LOG(LOG_NOTICE, LFD_NA_CONNECTOR,
				"NA Connector: token number %d\n", i);
	    break;
	}
    }
    if (!found)
	rv = -1;

    nac->curr_token = rv;
    return rv;
}

static int32_t
lf_na_connect(const char *const producer,
	      nkncnt_client_ctx_t *out)
{
    key_t shmkey;
    int shmid, max_cnt_space;    
    char *shm = NULL;
    int32_t err = 0;
    uint64_t shm_size = 0;

    if (strcmp(producer,"/opt/nkn/sbin/nvsd")==0) {
	shmkey=NKN_SHMKEY;
	max_cnt_space = MAX_CNTS_NVSD;
    }
    else if (strcmp(producer,"/opt/nkn/sbin/ssld")==0) {
	shmkey=NKN_SSL_SHMKEY;
	max_cnt_space = MAX_CNTS_SSLD;

    }
    else if( (!strstr(producer,"live_mfpd")) ||
	     (!strstr(producer, "file_mfpd"))) {
	if ((shmkey=ftok(producer,NKN_SHMKEY))<0) {
	    char errbuf[256];
	    snprintf(errbuf, sizeof(errbuf), "ftok failed, %s may not have a shared counter in this machine", producer);
	    perror(errbuf);
	    err = -E_LF_NA_CONN;
	    goto error;
	}
	max_cnt_space = MAX_CNTS_MFP;
    } else {
	if((shmkey=ftok(producer,NKN_SHMKEY))<0) {
	    char errbuf[256];
	    snprintf(errbuf, sizeof(errbuf), "ftok failed, %s may not have a shared counter in this machine", producer);
	    perror(errbuf);
	    err = -E_LF_NA_CONN;
	    goto error;
	}
	max_cnt_space = MAX_CNTS_OTHERS;
    }

    shm_size = (sizeof(nkn_shmdef_t)+max_cnt_space*(sizeof(glob_item_t)+30));
    shmid = shmget(shmkey, shm_size, 0666);
    if (shmid < 0) {
	char errbuf[256];
	snprintf(errbuf, sizeof(errbuf), "shmget error, %s may not have a shared counter in this machine", producer);
	perror(errbuf);
	err = -E_LF_NA_CONN;
	goto error;
    }
    
    shm = shmat(shmid, NULL, 0);
    if (shm == (char *)-1) {
	char errbuf[256];
	snprintf(errbuf, sizeof(errbuf), "shmat error, %s may not be running in this machine", producer);
	perror(errbuf);
	err = -E_LF_NA_CONN;
	goto error;
    }

    err = nkncnt_client_init(shm, max_cnt_space, out);
    if (err) {
	goto error;
    }

 error:
    return err;
}

static int32_t
lf_na_http_metrics_create(void *inp, void **out)
{
    int32_t err = 0, num_slots = 0, i = 0, num_slots1 = 0;
    int32_t num_slots2 = 0;
    lf_na_http_metrics_ctx_t *hm = NULL;
    lf_na_connector_t *nac = NULL;
    char counter_name[256] = {0};
    
    nac = (lf_na_connector_t *)(inp);
    hm = (lf_na_http_metrics_ctx_t *)
	nkn_calloc_type(1, sizeof(lf_na_http_metrics_ctx_t),
			100);
    if (!hm) {
	err = -E_LF_NO_MEM;
	goto error;
    }
    
    num_slots = (nac->window_len / nac->sampling_interval) +  1;
    for (i = 0; i < LF_NA_HTTP_MAX_RP; i++) {
	ma_init(num_slots, 1, &hm->bw_use[i]);
    }    
    hm->gm.size = \
	sizeof(lf_na_http_glob_metrics_t) + \
	(LF_NA_HTTP_MAX_RP * sizeof(lf_na_http_rp_metrics_t)) + \
	(LF_NA_HTTP_MAX_VIP * sizeof(lf_na_http_vip_metrics_t));
    ma_init(num_slots, 1, &hm->http_conn_rate);
    hm->prev_tps = 0xffffffff;
    pthread_mutex_init(&hm->lock, NULL);
    
    /* compute the number of slots for average for external MFC
     * consumers e.g. SNMP etc. Init the moving average calc for the
     * same
     */
    num_slots2 = 
	(nac->ext_window_len / nac->sampling_interval) + 1;
    ma_init(num_slots2, 1, &hm->ext_lf);
    nkn_mon_add("na.http.lf", NULL, (void *)&hm->gm.ext_lf, 
		sizeof(hm->gm.ext_lf));

    for(i = 0; i < LF_NA_HTTP_DISK_TIER_OTH; i++) {
	snprintf(counter_name, 256, "na.http.disk.%s.queue_cnt",
		 disk_tier_name[i]);
	nkn_mon_add(counter_name, NULL, 
		    (void*)&hm->disk_get_q_count[i], 
		    sizeof(hm->disk_get_q_count[i]));
	snprintf(counter_name, 256, "na.http.disk.%s.sac_limit", 
		 disk_tier_name[i]);
	nkn_mon_add(counter_name, NULL,
		    (void *)&hm->disk_get_q_sac[i],
		    sizeof(hm->disk_get_q_sac[i]));
	snprintf(counter_name, 256, "na.http.disk.%s.lf",
		 disk_tier_name[i]);
	nkn_mon_add(counter_name, NULL, 
		    (void*)&hm->gm.disk_lf[i],
		    sizeof(hm->gm.disk_lf[i]));
		 
    }
		    
    /* init the load factor computation metrics */
    num_slots1 = (1000/ nac->sampling_interval) + 1;
    hm->sampling_interval = nac->sampling_interval;
    hm->cpu_clk_freq = lf_get_cpu_clk_freq();

    *out = hm;
    return err;

 error:
    lf_na_http_metrics_cleanup(hm);
    return err;
}

static int32_t 
lf_na_http_metrics_update(void *app_conn, void *app_metrics_ctx)
{
    lf_na_http_metrics_ctx_t *hm = NULL;
    nkncnt_client_ctx_t *nc = NULL;
    cp_vector *matches = NULL, *vip_matches = NULL;
    int32_t err = 0, pid = -1;
    uint32_t i, j;
    char buf[256] = {0}, pname[16] = {0}; 
    glob_item_t *item = NULL, *item1 = NULL, *item2 = NULL;
    uint64_t cpu_time, tot_cpu_time = 0;
    sample_t s_cpu;
    FILE *f = NULL;
    double lf[LF_NA_HTTP_MOD_MAX];
    int8_t http_ready_flag = 0, sys_ready_flag = 0;

    assert(app_metrics_ctx);
    hm = (lf_na_http_metrics_ctx_t *)app_metrics_ctx;
    
    assert(app_conn);
    nc = (nkncnt_client_ctx_t *)app_conn;

    if (hm->pid) {
	err = lf_get_proc_name(hm->pid, pname);
	if (err  || strcmp(pname, "nvsd")) {
	    hm->pid = 0;
	    hm->is_ready = 0;
	    hm->time_elapsed = 0;
	    err = -E_LF_NA_HTTP_UNAVAIL;
	    goto error;
	}
	/* detect NVSD state */
	hm->is_ready = lf_na_http_detect_service_state(nc);
    } else {
	lf_cleanup_pid_stat_list(hm->mm_pid_list);
	lf_cleanup_pid_stat_list(hm->nw_pid_list);
	lf_cleanup_pid_stat_list(hm->sched_pid_list);
	hm->mm_pid_list = hm->nw_pid_list = \
	    hm->sched_pid_list = NULL;
	lf_find_parent_pid_exact_name_match("nvsd", &pid);
	if (pid == -1) {
	    hm->pid = 0;
	    hm->time_elapsed = 0;
	    hm->is_ready = 0;
	    err = -E_LF_NA_HTTP_UNAVAIL;
	    goto error;
	}
	/* detect NVSD state */
	hm->is_ready = lf_na_http_detect_service_state(nc);
	hm->pid = pid;
    }
    
    hm->time_elapsed += hm->sampling_interval;
    if (!hm->sched_pid_list && hm->is_ready == 1) {
	lf_find_child_pid_name_match(hm->pid, "nvsd-sched",
				     LF_PROC_MATCH_TYPE_EXACT,
				     &hm->sched_pid_list);
    }
    if (!hm->nw_pid_list && hm->is_ready == 1) {
	lf_find_child_pid_name_match(hm->pid, "nvsd-network",
				     LF_PROC_MATCH_TYPE_SUBSTR,
				     &hm->nw_pid_list);
    }
    if (!hm->mm_pid_list && hm->is_ready == 1) {
	lf_find_child_pid_name_match(hm->pid, "mm-",
				     LF_PROC_MATCH_TYPE_SUBSTR,
				     &hm->mm_pid_list);

	/* need to compute the disk queue SAC only once */
	lf_na_http_fill_disk_get_q_sac(nc, hm);
    }
    
    lf_na_http_calc_pid_list_lf(hm, hm->mm_pid_list, 
				&lf[LF_NA_HTTP_MOD_MM]);
    lf_na_http_calc_pid_list_lf(hm, hm->nw_pid_list, 
				&lf[LF_NA_HTTP_MOD_NW]);
    lf_na_http_calc_pid_list_lf(hm, hm->sched_pid_list, 
				&lf[LF_NA_HTTP_MOD_SCHED]);

    hm->gm.lf = 0;
    for (i = 0; i < LF_NA_HTTP_MOD_MAX; i++) {
	if (lf[i] > hm->gm.lf) {
	    hm->gm.lf = lf[i];
	}
    }
    sample_t lf_samp;
    lf_samp.i64 = round(hm->gm.lf);
    ma_move_window(&hm->ext_lf, lf_samp);
    hm->gm.ext_lf = (int64_t)hm->ext_lf.prev_ma;
    

    /* get pre-read state */
    lf_na_http_fill_preread_state(nc, hm);

    /* get disk queue length factor */
    lf_na_http_fill_disk_io_params(nc, hm);

    /* get disk sac */
    lf_na_http_fill_disk_get_q_sac(nc, hm);
    
    /* compute load factor per tier */
    lf_na_http_fill_disk_usage(hm);

    /*compute RAM cache usage */
    lf_na_http_fill_ram_cache_usage(nc, hm);

#if 0
    printf("load factor (time elapsed %lu, mm = %f, nw = %f,"
	   " sched =%f, http=%f) \n",
	   hm->time_elapsed, 
	   lf[LF_NA_HTTP_MOD_MM], lf[LF_NA_HTTP_MOD_NW],
	   lf[LF_NA_HTTP_MOD_SCHED], hm->lf);
#endif

    /* compute TPS */
    item = NULL;
    err = nkncnt_client_get_exact_match(nc,
			"glob_http_tot_transactions", &item);

    if (item) {
	sample_t s;
	if (hm->prev_tps == 0xffffffff) {
	    hm->prev_tps = item->value;
	}
	s.i64 = (item->value - hm->prev_tps)/
	    ((double)hm->sampling_interval/1000);
	hm->prev_tps = item->value;
	ma_move_window(&hm->http_conn_rate, s);
	hm->gm.tps = (uint32_t)hm->http_conn_rate.prev_ma;
    } else {
	hm->gm.tps = 0;
    }
    
    /* get all rp* counters*/
    err = nkncnt_client_get_submatch(nc, "rp.", &matches);
    if (err) {
	goto error;
    }

    /* prune rp* counters to get the unique resource pool names */
    err = lf_na_http_metrics_get_rp_list(hm, nc, matches);
    if (err) {
	goto error;
    }
    
    /* get vip list
     * NOTE: the list 'vip_matcher' can be NULL
     */
    nkncnt_client_get_submatch(nc, "net_port.", &vip_matches);

    /* get VIP list */
    err = lf_na_http_metrics_get_vip_list(hm, nc, vip_matches);
    if (err) {
	goto error;
    }

    /* get RP stats */
    err = lf_na_http_fill_rp_stats(hm, nc);
    if (err) {
	goto error;
    }

    /* get VIP stats */
    err = lf_na_http_fill_vip_stats(hm, nc);
    if (err) {
	goto error;
    }

 error:
    if (matches) cp_vector_destroy(matches);
    if (vip_matches) cp_vector_destroy(vip_matches);
    return err;
}

static int32_t
lf_na_http_calc_pid_list_lf(lf_na_http_metrics_ctx_t *hm,
			    struct pid_stat_list_t *head,
			    double *out)
{
    pid_stat_list_elm_t *elm;
    uint64_t tot_time = 0, t;
    double pid_cpu_time = 0.0, lf = 0.0;
    int32_t n_elm = 0;

    if (!head) {
	*out = 0.0;
	return 0;
    }

    TAILQ_FOREACH(elm, head, entries) {
	lf_pid_stat_t *st = &elm->st;
	lf_get_thread_cpu_time(hm->pid, st->pid, st->curr_cpu);
	if (st->first_flag) {
	    lf_save_cpu_time_stat(st->curr_cpu, st->prev_cpu);
	    st->first_flag = 0;
	}
	st->tot_cpu = lf_calc_cpu_time_elapsed(st->curr_cpu,
					       st->prev_cpu);
	lf_save_cpu_time_stat(st->curr_cpu, st->prev_cpu);
	pid_cpu_time = 
	    (100 * (st->tot_cpu)/ hm->cpu_clk_freq)/ 
	    ((double)hm->sampling_interval/1000);
	tot_time += ceil(pid_cpu_time);
	n_elm++;
    }
    
    assert(n_elm);
    lf = (tot_time / n_elm);
    *out = lf;

    return 0;
}

static int32_t
lf_na_http_metrics_get_rp_list(lf_na_http_metrics_ctx_t *hm,
			       nkncnt_client_ctx_t *nc,
			       cp_vector *rp_list)
{
    int32_t err = 0, num_elements = 0, i = 0, num_rp = 0;
    glob_item_t *item = NULL, *item1 = NULL;
    char *varname = (char *)(nc->shm +  sizeof(nkn_shmdef_t) + 
			   sizeof(glob_item_t) * nc->max_cnts);
    char **rp_name = hm->rp_name, *tmp_name = NULL;
    cp_trie *tmp_trie = NULL;

#if 0
    if (hm->rp_cache) {
	cp_trie_destroy(hm->rp_cache);
	hm->rp_cache = cp_trie_create_trie(
		(COLLECTION_MODE_NOSYNC|COLLECTION_MODE_COPY|
		 COLLECTION_MODE_DEEP),
		trie_copy_func, trie_destruct_func);
	if (!hm->rp_cache) {
	    err = -E_NKNCNT_CLIENT_TRIE_INIT;
	    goto error;
	}
    } else {
	hm->rp_cache = cp_trie_create_trie(
		(COLLECTION_MODE_NOSYNC|COLLECTION_MODE_COPY|
		 COLLECTION_MODE_DEEP),
		trie_copy_func, trie_destruct_func);
	if (!hm->rp_cache) {
	    err = -E_NKNCNT_CLIENT_TRIE_INIT;
	    goto error;
	}
    }
#endif

    tmp_trie = cp_trie_create_trie(
		(COLLECTION_MODE_NOSYNC|COLLECTION_MODE_COPY|
		 COLLECTION_MODE_DEEP),
		trie_copy_func, trie_destruct_func);
    if (!tmp_trie) {
	err = -E_NKNCNT_CLIENT_TRIE_INIT;
	goto error;
    }
    
    num_elements = cp_vector_size(rp_list);

    for (i = 0; i < num_elements; i++) {
	item = (glob_item_t *)cp_vector_element_at(rp_list, i);
	lf_na_http_strip_rp_name(varname + item->name, 
				 3, &tmp_name);
	item1 = (glob_item_t*)cp_trie_exact_match(tmp_trie,
					  tmp_name);
	if (!item1) {
	    /* new entry in cache */
	    err = cp_trie_add(tmp_trie, 
			      tmp_name, item);
	    if (err) {
		err = -E_NKNCNT_CLIENT_TRIE_ADD;
		goto error;
	    }
	    if (num_rp >= LF_NA_HTTP_MAX_RP) {
		err = -E_NA_HTTP_EXCEEDS_RP_CNT;
		goto error;
	    } else {
		if (rp_name[num_rp] != NULL)
		    free(rp_name[num_rp]);
		
		rp_name[num_rp] = tmp_name;
		num_rp++;
	    }

	} else {
	    free(tmp_name);
	}
    }

    hm->gm.num_rp = num_rp;
 error:
    if (tmp_trie != NULL)
	cp_trie_destroy(tmp_trie);
    
    return err;
}

static int32_t 
lf_na_http_strip_rp_name(const char *str, 
			 uint32_t base_name_len,
			 char **out)
{
    char *p1  = (char*) (str);
    char *p2 = p1, *p3, *name = NULL;
    int32_t size = 0, err = 0;

    *out = NULL;    
    p3 = (p2 += base_name_len);
    while ( *p2++ != '.' ) {
	    };
    size = (p2 - p3);
    if (size <= 0) {
	err = -E_LF_INVAL;
	goto error;
    }
    name = (char *)nkn_calloc_type(1, size, 100);
    if (!name) {
	err = -E_LF_NO_MEM;
	goto error;
    }
    memcpy(name, p3, size - 1);
    *out = name;
    
    return err;

 error:
    if (name) free(name);
    return err;
}

static int32_t 
lf_na_http_metrics_hold(void *app_metrics_ctx)
{
    lf_na_http_metrics_ctx_t *hm = NULL;

    assert(app_metrics_ctx);
    hm = (lf_na_http_metrics_ctx_t *)app_metrics_ctx;

    pthread_mutex_lock(&hm->lock);
    
    return 0;
}

static int32_t
lf_na_http_metrics_get_out_size(void *app_metrics_ctx,
				uint32_t *out_size)
{
    lf_na_http_metrics_ctx_t *hm = NULL;

    assert(app_metrics_ctx);
    assert(out_size);
    hm = (lf_na_http_metrics_ctx_t *)app_metrics_ctx;

    *out_size = hm->gm.size;
    return 0;
}

static int32_t 
lf_na_http_metrics_copy(const void *app_metrics_ctx, 
			void *const out)
{
    lf_na_http_metrics_ctx_t *hm = NULL;

    assert(app_metrics_ctx);
    assert(out);
    hm = (lf_na_http_metrics_ctx_t *)app_metrics_ctx;

    memcpy(out, hm, hm->gm.size);

    return hm->gm.size;    
}

static int32_t 
lf_na_http_metrics_release(void *app_metrics_ctx)
{
    lf_na_http_metrics_ctx_t *hm = NULL;

    assert(app_metrics_ctx);
    hm = (lf_na_http_metrics_ctx_t *)app_metrics_ctx;

    pthread_mutex_unlock(&hm->lock);
    
    return 0;
}

static void
lf_na_http_metrics_cleanup(void *app_metrics_ctx)
{
    lf_na_http_metrics_ctx_t *hm = NULL;
    uint32_t i;
  
    hm = (lf_na_http_metrics_ctx_t *)app_metrics_ctx;
    
    if (hm) {
	for (i = 0; i < hm->gm.num_rp; i++) {
	    ma_deinit(&hm->bw_use[i]);
	    if (hm->rp_name[i]) free(hm->rp_name[i]);
	}
	if (hm) {
	    free(hm);
	}
    }

}

/* NVSD ready?
 * Definition for 'Ready' here is whether NVSD is ready to serve
 * data. this uses two counters namely
 * 1. nvsd.global.system_init_done which indicates if NVSD's
 * core infrastructure (RAM cache alloc, DM2 disk detect, logger,
 * scheduler, counters etc) is ready
 * 2. nvsd.global.http_service_init_done which indicats if NVSD's
 * HTTP service is ready
 * these two events are sequential and (2) happens iff (1) is
 * complete except if some known failure (disk etc) is detected
 * during (1). Since the requirement is to indicate NVSD's
 * readiness to server from cache we will report the boolean
 * AND of (1), (2)
 */
static inline int8_t
lf_na_http_detect_service_state(nkncnt_client_ctx_t *nc)
{
    glob_item_t *item1, *item2;
    int8_t is_ready = 0, err = 0, sys_ready_flag = 0, 
	http_ready_flag = 0;
    item1 = NULL;
    item2 = NULL;
    err = nkncnt_client_get_exact_match(nc,
			"nvsd.global.system_init_done", &item1);
    err = nkncnt_client_get_exact_match(nc,
			"nvsd.global.http_service_init_done", &item2);
    if (item1 && item2) {
	sys_ready_flag = item1->value;
	http_ready_flag = item2->value;
	is_ready = sys_ready_flag & http_ready_flag;
    } else {
	is_ready = -1;
    }

    return is_ready;

}

/** 
 * computes the pre-read state at a disk tier level and fills the
 * appropriate fields in the http_metrics_ctx
 * 
 * @param nc [in] - the nkncnt client context used to search nvsd's
 * counters 
 * @param hm [in/out] - lf_na_http_metrics_ctx_t object which is needs
 * to be populated 
 * 
 * @return returns -1 if DM2 init is incomplete and zero otherwise
 */
static inline int8_t
lf_na_http_fill_preread_state(nkncnt_client_ctx_t *nc, 
			      lf_na_http_metrics_ctx_t *hm)
{
    glob_item_t *item;
    int8_t err = 0;
    int32_t i = 0;

    /* DM2 ready? */
    item = NULL;
    err = nkncnt_client_get_exact_match(nc, 
			"glob_dm2_init_done", &item);
    if (item) {
	if (!item->value) {
	    err = -1;
	    for(i = 0; i < LF_NA_HTTP_DISK_TIER_OTH; i++) {
		hm->gm.preread_state[i] = -1;
	    }
	    goto error;
	}
    } else {
	err = -1;
	for(i = 0; i < LF_NA_HTTP_DISK_TIER_OTH; i++) {
	    hm->gm.preread_state[i] = -1;
	}
	goto error;
    }
    /* fallthrough if DM2 is ready  */

    /* find which cache tiers are available and find preread status */
    for(i = 0; i < LF_NA_HTTP_DISK_TIER_OTH; i++) {
	hm->gm.preread_state[i] = 
	    lf_na_http_get_tier_preread_state(nc, i);
    }
    
 error:
    return err;
}

static inline int8_t
lf_na_http_get_tier_preread_state(nkncnt_client_ctx_t *nc, 
				  lf_na_http_disk_tier_t tier)
{
    glob_item_t *item, *item1;
    int8_t err = 0;

    item = NULL;
    err = nkncnt_client_get_exact_match(nc,
			disk_tier_avl_counter_name[tier], &item);
    if (item && item->value) {
	err = nkncnt_client_get_exact_match(nc,
		    disk_tier_preread_counter_name[tier], &item1);
	if (item1) {
	    err = item1->value;
	} else {
	    err = -1;
	    goto error;
	}
    } else {
	err = -1;
	goto error;
    }

 error:
    return err;
    
}

/** 
 * computes the disk get queue length on a tier level and fills the
 * appropriate members in the lf_na_http_metrics_ctx_t object
 * 
 * @param nc [in] - the nkncnt client context used to search nvsd's
 * counters 
 * @param hm [in/out] - lf_na_http_metrics_ctx_t object which is needs
 * to be populated 
 * @return - N/A
 */
static inline int32_t
lf_na_http_fill_disk_io_params(nkncnt_client_ctx_t *nc,
			  lf_na_http_metrics_ctx_t *hm)
{
    int32_t i, err = 0;
    glob_item_t *item = NULL;

    for(i = 0; i < LF_NA_HTTP_DISK_TIER_OTH; i++) {
	switch(hm->gm.preread_state[i]) {
	    case 1:
		err = nkncnt_client_get_exact_match(nc, 
		    disk_tier_get_q_counter_name[i], &item);
		if (item) {
		    hm->disk_get_q_count[i] = item->value;
		} else {
		    hm->disk_get_q_count[i] = -1; //redundant;
		                            //preread status should
		                            //take care of this
		}
		break;
	    case 0:
		hm->disk_get_q_count[i] = 0;
		break;
	    case -1:
		hm->disk_get_q_count[i] = -1;
		break;
	    default:
		assert(0);
	}
    }
	
    return err;
}

/** 
 * computes the disk get queue SAC on a tier level and fills the
 * appropriate members in the lf_na_http_metrics_ctx_t object
 * 
 * @param nc [in] - the nkncnt client context used to search nvsd's
 * counters 
 * @param hm [in/out] - lf_na_http_metrics_ctx_t object which is needs
 * to be populated 
 * @return - N/A
 */
static inline int32_t
lf_na_http_fill_disk_get_q_sac(nkncnt_client_ctx_t *nc,
			       lf_na_http_metrics_ctx_t *hm)
{
    int32_t i, err = 0;
    glob_item_t *item1 = NULL, *item2 = NULL;

    for(i = 0; i < LF_NA_HTTP_DISK_TIER_OTH; i++) {
	if (hm->gm.preread_state[i] != -1) {
	    err = nkncnt_client_get_exact_match(nc, 
			disk_tier_num_get_threads_counter_name[i], &item1);
	    err = nkncnt_client_get_exact_match(nc, 
			disk_tier_get_q_sac_counter_name[i], &item2);
	    if (item1 && item2) {
		hm->disk_get_q_sac[i] = item1->value * item2->value;
	    } else {
		hm->disk_get_q_sac[i] = -1; //redundant;
				            //preread status should
					    //take care of this
	    }
	} else {
	    hm->disk_get_q_sac[i] = -1;
	}
	
    }
    
    return err;
}

static inline int32_t
lf_na_http_fill_disk_usage(lf_na_http_metrics_ctx_t *hm)
{
    int32_t i = 0;
    for (i = 0; i < LF_NA_HTTP_DISK_TIER_OTH; i++) {
	if (hm->gm.preread_state[i] != -1) {
	    hm->gm.disk_lf[i] =
		((double)hm->disk_get_q_count[i]/
		 hm->disk_get_q_sac[i]) * 100;
	}
    }

    return 0;
}

/** 
 * computes the RAM cache usage as follows:
 * ram cache usage = buffers in transit
 *                   ------------------ * 100
 *                   total buffers
 * 
 * @param nc [in] - the nkncnt client context
 * @param hm [in/out] - the http metrics context
 * 
 * @return 
 */
static inline int32_t
lf_na_http_fill_ram_cache_usage(nkncnt_client_ctx_t *nc,
				lf_na_http_metrics_ctx_t *hm)
{
    int32_t err = 0;
    glob_item_t *item = NULL;
    uint64_t total = 1, use = 0;

    hm->gm.ram_lf = 0.0;
    if (!hm->is_ready) {
	return 0;
    }
    err = nkncnt_client_get_exact_match(nc, 
		"glob_bm_total_transit_buffers", &item);
    if (item) {
	use = (uint64_t)item->value;
    }

    item = NULL;
    err = nkncnt_client_get_exact_match(nc, 
		"glob_bm_total_buffers", &item);
    if (item) {
	total = (uint64_t)item->value;
    }
    hm->gm.ram_lf = ((double)use/total) * 100;
    
    return 0;
}

static void *
trie_copy_func(void *nd)
{
    UNUSED_ARGUMENT(nd);
    return nd;
}

static void
trie_destruct_func(void *nd)
{
    UNUSED_ARGUMENT(nd);
    return;
}


static int32_t
lf_na_http_metrics_get_vip_list(lf_na_http_metrics_ctx_t *hm,
			       nkncnt_client_ctx_t *nc,
			       cp_vector *vip_list)
{
    int32_t err = 0, num_elements = 0, i = 0, num_vip = 0;
    glob_item_t *item = NULL, *item1 = NULL;
    char *varname = (char *)(nc->shm +  sizeof(nkn_shmdef_t) + 
			   sizeof(glob_item_t) * nc->max_cnts);
    char **vip_name = hm->vip_name, *tmp_name = NULL;
    cp_trie *tmp_trie = NULL;

    if (vip_list) {
	num_elements = cp_vector_size(vip_list);
    }
    
    if (!num_elements) {
	hm->gm.num_vip = 0;
    }
    tmp_trie = cp_trie_create_trie(
		(COLLECTION_MODE_NOSYNC|COLLECTION_MODE_COPY|
		 COLLECTION_MODE_DEEP),
		trie_copy_func, trie_destruct_func);
    if (!tmp_trie) {
	err = -E_NKNCNT_CLIENT_TRIE_INIT;
	goto error;
    }

    
    for (i = 0; i < num_elements; i++) {
	item = (glob_item_t *)cp_vector_element_at(vip_list, i);
	lf_na_http_strip_rp_name(varname + item->name, 
				 9, &tmp_name);
	item1 = (glob_item_t*)cp_trie_exact_match(tmp_trie,
					  tmp_name);
	if (!item1) {
	    /* new entry in cache */
	    err = cp_trie_add(tmp_trie, 
			      tmp_name, item);
	    if (err) {
		err = -E_NKNCNT_CLIENT_TRIE_ADD;
		goto error;
	    }
	    if (num_vip >= LF_NA_HTTP_MAX_VIP) {
		err = -E_NA_HTTP_EXCEEDS_VIP_CNT;
		goto error;
	    } else {
		if (vip_name[num_vip] != NULL)
		    free(vip_name[num_vip]);
		
		vip_name[num_vip] = tmp_name;
		num_vip++;
	    }

	} else {
	    free(tmp_name);
	}
    }

    hm->gm.num_vip = num_vip;
 error:
    if (tmp_trie != NULL)
	cp_trie_destroy(tmp_trie);
    
    return err;
}

static inline int32_t
lf_na_http_fill_rp_stats(lf_na_http_metrics_ctx_t *hm,
			 nkncnt_client_ctx_t *nc)
{
    glob_item_t *item;
    uint32_t i = 0, err = 0;
    char buf[256] = {0};

    item = NULL;
    for (i = 0; i < hm->gm.num_rp; i++) {
	lf_na_http_rp_metrics_t *rpm = &hm->rpm[i];
	sample_t s;
	snprintf(rpm->name, 64, "%s", hm->rp_name[i]);

	rpm->avail = 0;
	snprintf(buf, 256, "rp.%s.namespace.bound_num",
		 hm->rp_name[i]);
	nkncnt_client_get_exact_match(nc, buf, &item);
	if (item) {
	    rpm->avail = (item->value > 0) * 1;
	}
	snprintf(buf, 256, "rp.%s.client.bw_used", hm->rp_name[i]);
	nkncnt_client_get_exact_match(nc, buf, &item);
	if (item) {
	    s.i64 = item->value;
	    ma_move_window(&hm->bw_use[i], s);
	    rpm->bw_use = (uint64_t)hm->bw_use[i].prev_ma;
	}
	
	snprintf(buf, 256, "rp.%s.client.bw_max", hm->rp_name[i]);
	nkncnt_client_get_exact_match(nc, buf, &item);
	if (item) {
	    rpm->bw_max = item->value;
	}
	snprintf(buf, 256, "rp.%s.client.sessions_used",
		 hm->rp_name[i]);
	nkncnt_client_get_exact_match(nc, buf, &item);
	if (item) {
	    rpm->active_conn = item->value;
	}
	snprintf(buf, 256, "rp.%s.client.sessions_max",
		 hm->rp_name[i]);
	nkncnt_client_get_exact_match(nc, buf, &item);
	if (item) {
	    rpm->active_conn_max = item->value;
	}

    }

    return err;

}

int32_t
lf_na_http_fill_vip_stats(lf_na_http_metrics_ctx_t *hm,
			  nkncnt_client_ctx_t *nc)
{
    int32_t err = 0;
    uint32_t i = 0;
    glob_item_t *item = NULL;
    char buf[256] = {0};
    
    for(i = 0; i < hm->gm.num_vip; i++) {
	snprintf(hm->vipm[i].name, 64, "%s", hm->vip_name[i]);
	snprintf(buf, 256, "net_port.%s.vip_addr", hm->vip_name[i]);
	nkncnt_client_get_exact_match(nc, buf, &item);
	if (item) {
	    hm->vipm[i].ip = item->value;
	} else {
	    hm->vipm[i].ip = 0;
	}
	snprintf(buf, 256, "net_port.%s.tot_sessions",
		 hm->vip_name[i]);
	nkncnt_client_get_exact_match(nc, buf, &item);
	if (item) {
	    hm->vipm[i].tot_sessions = item->value;
	} else {
	    hm->vipm[i].tot_sessions = 0;
	}
    }
   
    return err;
}
