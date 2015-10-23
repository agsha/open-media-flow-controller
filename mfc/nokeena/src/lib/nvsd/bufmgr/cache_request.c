/*
 * Implementation of the cache request handler
 *
 * Requests that miss the cache are filled using the MM/OM.  This is
 * done by creating a sub-task.  On completion of the sub-task, the 
 * originating task is woken up.  
 * 
 * The source for a given request is specified by a "physid".  Since we can 
 * have multiple originating tasks waiting for the same physid, we use an
 * event queue structure to coordinate the activity. 
 * TBD - Can this be generalized to a libary?
 */
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <glib.h>
#include "nkn_defs.h"
#include "cache_mgr.h"
#include "nkn_sched_api.h"
#include "nkn_mediamgr_api.h"
#include "nkn_buf.h"
#include "nkn_om.h"
#include "cache_mgr_private.h"
#include "nkn_errno.h"
#include "nkn_am_api.h"
#include "nkn_memalloc.h"
#include "nkn_lockstat.h"
#include "nkn_cod.h"
#include "nkn_pseudo_con.h"
#include "nkn_debug.h"
#include "nkn_rtsched_api.h"
#include "nkn_assert.h"
#include "om_defs.h"

#define CRT_POOL	32

static GHashTable *cr_table[CRT_POOL], *cr_table2[CRT_POOL];
static nkn_mutex_t crt_lock[CRT_POOL];
AO_t glob_cr_pending=0;
static int glob_cr_cnt_private=0, glob_cr_cnt_shared=0, glob_cr_cnt_collision=0;
static int glob_mmstat_fail=0;
static int glob_media_get_task_alloc=0, glob_media_stat_task_alloc=0;
static int glob_media_get_task_allocfail=0, glob_media_stat_task_allocfail=0;
static int glob_media_reval_task_alloc=0, glob_media_reval_task_allocfail=0;
static int glob_media_update_task_alloc=0, glob_media_update_task_allocfail=0;
static int glob_mmvalidate_mod=0, glob_mmvalidate_fail=0, glob_mmvalidate_error=0;
static int glob_mmvalidate_bypass = 0;
static int glob_mmvalidate_done=0;
static int glob_sync_attr_task_allocfail=0, glob_sync_attr_task_alloc=0;
static int glob_sync_attr_allocfail=0; 
static int glob_bufmgr_attr_buf_not_allocd = 0;
static int glob_bufmgr_buf_not_allocd = 0;
static int glob_bufmgr_reval_on_expire = 0;
static int glob_bm_attr_update_mismatch=0;
#if 0
static int glob_bufmgr_delete_allocfail = 0;
static int glob_bufmgr_delete_task_allocfail = 0;
static int glob_bufmgr_delete_tasks = 0;
static int glob_bufmgr_delete_tasks_failed = 0;
static int glob_bufmgr_delete_tasks_done = 0;
#endif
static int glob_cr_align_cnt=0;
static int glob_cr_recheck=0;
//uint64_t glob_am_to_dm2_hotness_update = 0;
static nkn_mutex_t rvq_lock;
static nkn_mutex_t syncupdateq_lock;
static nkn_mutex_t hotupdateq_lock;
extern long bm_cfg_cr_max_queue_time;
extern long bm_cfg_cr_max_req;
AO_t glob_nkn_bm_to_dm2_read_task;
AO_t glob_nkn_bm_to_om_read_task;
AO_t glob_nkn_bm_to_om_validate_task;


unsigned long long glob_expire_attrib_resouce_unavail = 0;
static uint64_t glob_bm_queue_timeout_tunnel_req = 0;
static uint64_t glob_bm_queue_timeout_reqout_tunnel_req = 0;
uint64_t glob_cr_ingest_buf_not_avail = 0;

uint64_t nkn_buf_reval_size=16*1024;	// 32 KB min size to reval
int nkn_buf_reval_time=10;		// 10 seconds (default)
int nkn_buf_min_update=30;		// 30 seconds min expiry to update
 // one day - making this a global to be able set by mgmt module
int64_t glob_bm_cfg_attr_sync_interval = 3600 * 24;
static int bm_cr_dorecheck = 1;

uint64_t glob_bufmgr_cache_server_busy_err;   // sub error code counter for NKN_SERVER_BUSY

static nkn_lockstat_t crlockstat;

typedef struct nkn_resource {
	int counter, limit, overflow;
} nkn_resource_t;

static nkn_resource_t attr_hotnessupdate_tasks = {0, 10000, 0};
static nkn_resource_t attr_syncupdate_tasks = {0, 200, 0};
//static nkn_resource_t delete_tasks = {0, 20, 0};

void chunk_mgr_output(nkn_task_id_t taskid);
static void
crv_done(nkn_buf_t *attrbuf, int err, MM_validate_t *mmv);

extern unsigned long long glob_cr_reval_conf_timed_wait;
extern unsigned long long glob_cr_reval_uncond_timed_wait;   

#define CM_RV_CONF_TIMED_WAIT	    0x1 
#define CM_RV_UNCOND_TIMED_WAIT	    0x2
// sync reval always flag
#define CM_RV_SYNC_REVAL	    0x4
// sync reval always noupdate flag
#define CM_RV_CACHE_NOUPDATE	    0x8
// sync reval always force always flag
#define CM_RV_FORCEREVAL_ALWAYS	    0x10
// sync reval always tunnel for modified flag
#define CM_RV_SYNC_REVALTUNNEL	    0x20
// no sync reval -- deliver expired object flag
#define CM_RV_DELIVER_EXP	    0x40

// struct used for parameters and private data for the revalidation tasks.
#define NKN_REVAL_MAGIC 0x12345678
typedef struct {
	uint32_t magic;
	nkn_buf_t *oldbuf, *newbuf;
	namespace_token_t   ns_token;
	int proto_free;
	MM_validate_t mmv;
	MM_update_attr_t mmu;
	int flags;   // used for conditional/unconditional retry
	nkn_task_id_t tid;     // task id used for conditional/unconditional retry
	int retries;
} nkn_reval_t;

// struct used for attribute sync.
#define NKN_SYNC_MAGIC 0x87654321
typedef struct {
	uint32_t magic;
	MM_update_attr_t mmu;
	nkn_buf_t *attrbuf;
} nkn_sync_attr_t;

#ifndef CENTOS_6
// The newer glib has this function.
static inline guint
g_int64_hash(gconstpointer v)
{
    const guint32 w1 = ((*(const uint64_t *)v) & 0xffffffff);
    const guint32 w2 = ((*(const uint64_t *)v) & 0xffffffff00000000) >> 32;

    return (w1 ^ w2);
}
#endif /* not CENTOS_6 */

#ifndef CENTOS_6
// The newer glib has this function.
static inline gboolean
g_int64_equal(gconstpointer v1,
	      gconstpointer v2)
{
    return *((const uint64_t *)v1) == *((const uint64_t *)v2);
}
#endif /* not CENTOS_6 */

/*
 * Routines to limit the number of outstanding tasks
 * Reserve returns zero if a slot is available and non-zero otherwise.
 * Release puts back a reserved slot.
 */
static int
cr_get_syncresource_slot(nkn_resource_t *res)
{
	NKN_MUTEX_LOCK(&syncupdateq_lock);
	if (res->counter == res->limit){
		res->overflow++;
		NKN_MUTEX_UNLOCK(&syncupdateq_lock);
		return 1;
	}
	res->counter++;
	NKN_MUTEX_UNLOCK(&syncupdateq_lock);
	return 0;
}

static void
cr_put_syncresource_slot(nkn_resource_t *res)
{
	NKN_MUTEX_LOCK(&syncupdateq_lock);
	assert(res->counter > 0);
	res->counter--;
	NKN_MUTEX_UNLOCK(&syncupdateq_lock);
}

static int
cr_get_hotresource_slot(nkn_resource_t *res)
{
	NKN_MUTEX_LOCK(&hotupdateq_lock);
	if (res->counter == res->limit){
		res->overflow++;
		NKN_MUTEX_UNLOCK(&hotupdateq_lock);
		return 1;
	}
	res->counter++;
	NKN_MUTEX_UNLOCK(&hotupdateq_lock);
	return 0;
}

static void
cr_put_hotresource_slot(nkn_resource_t *res)
{
	NKN_MUTEX_LOCK(&hotupdateq_lock);
	assert(res->counter > 0);
	res->counter--;
	NKN_MUTEX_UNLOCK(&hotupdateq_lock);
}

/* 
 * Find an existing request based on the physid or allocate a new one.  
 * It is returned with the lock its lock held.  
 * The caller must call the put/remove function to
 * release the lock and/or free the request.
 */
static cache_req_t *
crt_get(cache_req_t *ncr, char *physid, uint64_t *physid2, int *recheck)
{
	cache_req_t *cr = NULL;
	int t_index = ncr->uol.cod % CRT_POOL;
	NKN_MUTEX_LOCK(&crt_lock[t_index]);
	if (!(ncr->flags & CR_FLAG_PRIVATE)) {
		if (*physid2)			// use physid2 if non-zero
		    cr = g_hash_table_lookup(cr_table2[t_index], physid2);
		else
		    cr = g_hash_table_lookup(cr_table[t_index], physid);
	}
	if (cr) {
		NKN_MUTEX_LOCKSTAT(&cr->lock, &crlockstat);
	} else if (bm_cr_dorecheck && recheck &&
	    !(ncr->cp->crflags & CR_RFLAG_NO_CACHE)) {

		// Check to see if buffer was added to the cache while we 
		// did the STAT check.  Doing this under the CRT lock ensures
		// an existing task will not race with us.
		*recheck = 0;
		if (nkn_buf_check(&ncr->uol, NKN_BUFFER_DATA, NKN_BUF_IGNORE_EXPIRY)) {
			*recheck = 1;
			glob_cr_recheck++;
		}	
	}
	NKN_MUTEX_UNLOCK(&crt_lock[t_index]);
	return cr;
}

/* Return the request - called after a get to release the lock */
static void
crt_put(cache_req_t *cr)
{
	NKN_MUTEX_UNLOCKSTAT(&cr->lock);
}

/* 
 * Adds a new request to the table.  Returns 0 on success and non-zero if
 * there is already an existing entry in the table.
 */
static int
crt_add(cache_req_t *cr)
{
	int ret = 1;
	cache_req_t *existing;
	int t_index = cr->uol.cod % CRT_POOL;
	if (cr->physid2) {
		NKN_MUTEX_LOCK(&crt_lock[t_index]);
		existing = (cache_req_t *)g_hash_table_lookup(cr_table2[t_index],
					  &cr->physid2);
		if (existing == NULL) {
			g_hash_table_insert(cr_table2[t_index], &cr->physid2, cr);
			cr->crt_index = t_index;
			ret = 0;
		}
		NKN_MUTEX_UNLOCK(&crt_lock[t_index]);
	} else {
		NKN_MUTEX_LOCK(&crt_lock[t_index]);
		existing = (cache_req_t *)g_hash_table_lookup(cr_table[t_index], cr->physid);
		if (existing == NULL) {
			g_hash_table_insert(cr_table[t_index], cr->physid, cr);
			cr->ts = nkn_cur_ts;
			cr->crt_index = t_index;
			ret = 0;
		}
		NKN_MUTEX_UNLOCK(&crt_lock[t_index]);
	}
	if (existing == NULL) {
		AO_fetch_and_add1(&glob_cr_pending);
	}
	return ret;
}

/* Remove the request from the table with entry locked */
static void
crt_remove(cache_req_t *cr)
{
	int t_index = cr->crt_index;
	if (cr->physid2) {
		NKN_MUTEX_LOCK(&crt_lock[t_index]);
		g_hash_table_remove(cr_table2[t_index], &cr->physid2);
		NKN_MUTEX_LOCKSTAT(&cr->lock, &crlockstat);
		NKN_MUTEX_UNLOCK(&crt_lock[t_index]);
	} else {
		NKN_MUTEX_LOCK(&crt_lock[t_index]);
		g_hash_table_remove(cr_table[t_index], cr->physid);
		NKN_MUTEX_LOCKSTAT(&cr->lock, &crlockstat);
		NKN_MUTEX_UNLOCK(&crt_lock[t_index]);
	}
	AO_fetch_and_sub1(&glob_cr_pending);
}
/* 
 * free uncod retry tasks  -- cr lock held
 * walk through list of cache request cm priv list and
 * wakeup the corresponding task with the error passed
 * reset uncod retry state before waking up the tasks
 * current cmpriv task is already running so no wakeup call
 * for current cm priv
 */
void
cr_free_uncond_retry(cache_req_t *cr, int err, cmpriv_t *cp)
{
	cmpriv_t *cur_cp;
	crt_remove(cr);		/* returns with cr locked */
	/* Signal completions */
	while ((cur_cp = cr->cpq.tqh_first)) {
		if (cp != cur_cp) { 
			cur_cp->errcode = err;
		 	TAILQ_REMOVE(&cr->cpq, cur_cp, reqlist);
			cur_cp->flags &= ~CM_PRIV_UNCOND_RETRY_STATE; 
			cur_cp->request_ctxt = cp->request_ctxt;
			cur_cp->out_proto_resp = cp->out_proto_resp;
			cur_cp->delay_ms = cp->delay_ms;
			cur_cp->max_retries = cp->max_retries;
                        // Provider flag set in GET response, not to update this in AM.
                        if (cp->flags & CM_PRIV_NOAM_HITS) {
                            cur_cp->flags |= CM_PRIV_NOAM_HITS;
                        }
			cache_request_done(cur_cp, cr);
		} else {
			TAILQ_REMOVE(&cr->cpq, cp, reqlist);
		}
	}
	NKN_MUTEX_UNLOCKSTAT(&cr->lock)    /* just for symmetry */
	cp->flags &= ~CM_PRIV_UNCOND_RETRY_STATE;
}

static void
cr_free(cache_req_t *cr, int err, cmpriv_t *cp)
{
	int i;

	if (cp &&
		cp->flags & CM_PRIV_UNCOND_RETRY_STATE) {
		// wake all merged requests
		cr_free_uncond_retry(cr, err, cp);		
	}
	if (cr->uol.cod != NKN_COD_NULL)
		nkn_cod_close(cr->uol.cod, NKN_COD_BUF_MGR);
	if (cr->attrbuf)
		nkn_buf_put(cr->attrbuf);
	for (i=0; i<cr->num_bufs; i++)
		nkn_buf_put( cr->bufs[i]);
	free(cr);
}

static int cr_align_request = 1;       // TMP may need to be configurable
static cache_req_t *
cr_alloc(nkn_uol_t *uol, cmpriv_t *cp)
{
	cache_req_t *cr;
	// if uncod retry state then reuse cr 
	if (!(cp->flags & CM_PRIV_UNCOND_RETRY_STATE)) {
		cr = nkn_calloc_type(1, sizeof(cache_req_t),
			mod_bm_cache_req_t);
		if (cr == NULL) {
			return NULL;
		}
		cr->uol.cod = nkn_cod_dup(uol->cod, NKN_COD_BUF_MGR);
	} else {
		cr = cp->cr;
		cr->uol.cod = nkn_cod_dup(uol->cod, NKN_COD_BUF_MGR);
		cr->flags = 0;
		return cr;
	}

#ifdef TMPCOD
	cr->uol.uri = uol->uri;
#endif
	cr->uol.offset = uol->offset;
	cr->uol.length = uol->length;
	// Page align the requested offset to avoid holes during merge in
	// the cache.  BZ 3024.  Should be done only for request coming
	// directly from a client and not follow up requests after a short
	// read (see BZ 3471).
	if (cr_align_request && 
	    (cp->crflags & CR_RFLAG_FIRST_REQUEST) &&
	    !(cp->flags & CM_PRIV_TWICE) &&
	    (uol->offset & (CM_MEM_PAGE_SIZE-1)) && uol->length &&
	    (uol->length < CM_MEM_PAGE_SIZE)) {
		cr->uol.offset = cr->uol.offset & ~(CM_MEM_PAGE_SIZE-1);
		// Adjust length is a specific one was specified, otherwise
		// ask for the whole page - the provider will supply whole
		// or till EOF.
		if (cr->uol.length == 0)
			cr->uol.length = CM_MEM_PAGE_SIZE;
		else
			cr->uol.length = cr->uol.length + 
						(uol->offset - cr->uol.offset);
		glob_cr_align_cnt++;
	}
	cr->ns_token = cp->ns_token;
	cr->encoding_type = cp->encoding_type;
	if (cr->uol.length == 0)
		cr->uol.length = CM_MEM_PAGE_SIZE;
	cr->cp = cp;
	return cr;
}

/* Add fields required for the GET task to MM */
static int
cr_add_getdata(cache_req_t *cr, MM_stat_resp_t *statr)
{
	int i, num_bufs = 0;

	// Allocate data buffers before the attribute buffer since data
	// buffers hold on to attributes.  This way we can free up
	// attributes tied to the data buffers that we may be evicting.
	// Data buffers are not allocated if the provider wants to
	// allocate them on demand.
	if (!(statr->out_flags & MM_OFLAG_NOBUF)) {
		num_bufs = statr->media_blk_len / CM_MEM_PAGE_SIZE;
		if (num_bufs > MAX_BUFS_PER_TASK)
			num_bufs = MAX_BUFS_PER_TASK;
	}
	for (i=0; i<num_bufs; i++) {
		cr->bufs[i] = nkn_buf_alloc(NKN_BUFFER_DATA, 0, 0);
		if (cr->bufs[i] == NULL) {
                        glob_bufmgr_buf_not_allocd ++;
			glob_bufmgr_cache_server_busy_err++;
			return NKN_SERVER_BUSY;
		}
		cr->num_bufs++;
	}

	if (cr->cp->flags & CM_PRIV_GETATTR) {
		cr->attrbuf = nkn_buf_alloc(NKN_BUFFER_ATTR, statr->attr_size, 0);
		if (cr->attrbuf == NULL) {
                        glob_bufmgr_attr_buf_not_allocd ++;
			glob_bufmgr_cache_server_busy_err++;
			return NKN_SERVER_BUSY;
		}
	}

	/* code to avoid strncpy */
	if(statr->physid_len && (statr->physid_len < NKN_PHYSID_MAXLEN)) {
		memcpy(cr->physid, statr->physid, statr->physid_len);
		cr->physid[statr->physid_len] = '\0';
		cr->physid_len = statr->physid_len;
	} else {
		strncpy(cr->physid, statr->physid, NKN_PHYSID_MAXLEN);
		cr->physid_len = 0;
	}
	cr->physid2 = statr->physid2;
	cr->ptype = statr->ptype;
    	cr->ns_token = statr->ns_token;
	cr->tot_content_len = statr->tot_content_len;
	if (cr->cp->flags & CM_PRIV_UNCOND_RETRY_STATE)
		return 0;
 
	TAILQ_INIT(&cr->cpq);
	pthread_mutex_init(&cr->lock, NULL);
	return 0;
}


static void
cr_cp_attach(cache_req_t *cr, cmpriv_t *cp)
{
	TAILQ_INSERT_TAIL(&cr->cpq, cp, reqlist);
	cr->numreq++;
	cp->cr = cr;
}

static MM_get_resp_t *
mmr_alloc(cache_req_t *cr)
{
	MM_get_resp_t *mmr;

	mmr = nkn_calloc_type(1, sizeof(MM_get_resp_t), mod_bm_MM_get_resp_t);
        if (mmr == NULL)
		return NULL;
	/* cr->physid_len will be 0 or it will always be less than NKN_PHYSID_MAXLEN*/
	if(cr->physid_len) {
		memcpy(mmr->physid, cr->physid, cr->physid_len);
		mmr->physid[cr->physid_len] = '\0';
	} else
		strncpy(mmr->physid, cr->physid, NKN_PHYSID_MAXLEN);
	mmr->physid2 = cr->physid2;
	mmr->in_uol = cr->uol;
	mmr->ns_token = cr->ns_token;
	mmr->tot_content_len = cr->tot_content_len;
	mmr->encoding_type = cr->encoding_type;
	return mmr;
}

static MM_stat_resp_t *
statr_alloc(cache_req_t *cr)
{
	MM_stat_resp_t *statr;

	statr = nkn_calloc_type(1, sizeof(MM_stat_resp_t), mod_bm_MM_stat_resp_t);
        if (statr == NULL)
		return NULL;
	statr->in_uol = cr->uol;
	statr->ns_token = cr->ns_token;
	return statr;
}

static void
mmr_free(MM_get_resp_t *mmr)
{
	free(mmr);
}

/* 
 * Process completion of the request.  This will need to signal completion
 * to all attached cmprivs, remove the request from the table as well as 
 * free memory.  The buffers updated by the request should be put into
 * the cache before signalling the completions.
 */
static void
cr_done(cache_req_t *cr, int err, MM_get_resp_t *getr)
{
	int i, uncon_retry = 0;
	cmpriv_t *cp;
	

	/* Remove request from pool if it was not private */
	if (cr->flags & CR_FLAG_PRIVATE) {
		NKN_MUTEX_LOCKSTAT(&cr->lock, &crlockstat);
		if (err == NKN_MM_UNCOND_RETRY_REQUEST)
			uncon_retry = 1;
	}
	else {
		if (err != NKN_MM_UNCOND_RETRY_REQUEST) {
			crt_remove(cr);		/* returns with cr locked */
		} else {
			NKN_MUTEX_LOCKSTAT(&cr->lock, &crlockstat);
			uncon_retry = 1;
		}
	}

	/* Signal completions */
	while ((cp = cr->cpq.tqh_first)) {
		cp->errcode = err;

		if (cp->errcode == NKN_OM_CI_END_OFFSET_RESPONSE)
			cp->gotlen = getr->tot_content_len;

		if (!uncon_retry) {
		 	TAILQ_REMOVE(&cr->cpq, cp, reqlist);
			cp->flags &= ~CM_PRIV_UNCOND_RETRY_STATE; 
		} else {
			cp->flags |= CM_PRIV_UNCOND_RETRY_STATE; 
		}
		if (getr) {
			cp->request_ctxt = getr->req_ctx;
			cp->out_proto_resp = getr->out_proto_resp;
			cp->delay_ms = getr->out_delay_ms;
			cp->max_retries = getr->max_retries;
                        // Provider flag set in GET response, not to update this in AM.
                        if (getr->out_flags & MM_OFLAG_NOAM_HITS) {
                            cp->flags |= CM_PRIV_NOAM_HITS;
                        }

		}
		// bug 832743 -- Below call invokes another scheduler and below 
		// execited code overlaps with cr acess
		//cache_request_done(cp, cr);
		if (!uncon_retry) {
			cache_request_done(cp, cr);
		} else 
			goto skip_unlock;
			
	}
	NKN_MUTEX_UNLOCKSTAT(&cr->lock)    /* just for symmetry */

skip_unlock:
	/* 
	 * Release the buffers used for the request.  Either they are in cache.
	 * or will go back on the free list.  For non-cacheable buffers,
	 * the requestor should have picked up a reference in the done routine.
	 */
	for (i=0; i<cr->num_bufs; i++) {
		nkn_buf_put_discardid(cr->bufs[i], i);
	}
	if (cr->attrbuf)
		nkn_buf_put(cr->attrbuf);

	if (cr->uol.cod != NKN_COD_NULL)
		nkn_cod_close(cr->uol.cod, NKN_COD_BUF_MGR);

	if (!(uncon_retry)) {
		free(cr);
	} else { // uncond retry case
		cr->num_bufs = 0;
		cr->attrbuf = NULL;
		cr->uol.cod = NKN_COD_NULL;
		cache_request_done(cr->cp, cr);
		NKN_MUTEX_UNLOCKSTAT(&cr->lock)    /* just for symmetry */
	}
}

static int
cr_create_stat_task(cache_req_t *cr, cmpriv_t *cp)
{
	MM_stat_resp_t *statr;
	nkn_task_id_t tid;

	statr = statr_alloc(cr);
	if (statr == NULL) {
		cr_free(cr, ENOMEM, cp);	/* free cr */
		return ENOMEM;
	}
	memcpy(&statr->in_client_data, &cp->in_client_data,
	       sizeof(nkn_client_data_t));
	statr->in_proto_data = cp->proto_data;
	if (cp->crflags & CR_RFLAG_FIRST_REQUEST)
		statr->in_flags |= MM_FLAG_NEW_REQUEST;
	if (cp->crflags & CR_RFLAG_NO_CACHE)
		statr->in_flags |= (MM_FLAG_NO_CACHE | MM_FLAG_REQ_NO_CACHE);
	if (cp->flags & CM_PRIV_NO_CACHE)
		statr->in_flags |= MM_FLAG_NO_CACHE;
	if (cp->crflags & CR_RFLAG_TRACE)
		statr->in_flags |= MM_FLAG_TRACE_REQUEST;
	if (!(cp->flags & CM_PRIV_CHECK_EXPIRY))
		statr->in_flags |= MM_FLAG_IGNORE_EXPIRY;
	if (cp->crflags & CR_RFLAG_CACHE_ONLY)
		statr->in_flags |= MM_FLAG_CACHE_ONLY;

        tid = nkn_task_create_new_task(
                TASK_TYPE_MEDIA_MANAGER,
                TASK_TYPE_CHUNK_MANAGER,
                TASK_ACTION_INPUT,
                MM_OP_STAT,
                (void *)statr,
                sizeof(statr),
                0);                   /* deadline = 100 msecs */

        if (tid < 0) {
		glob_media_stat_task_allocfail++;
		cr_free(cr, ENOMEM, cp);            /* free cr */
		free(statr);
		return ENOMEM;
	}
	glob_media_stat_task_alloc++;
	cp->mm_task_id = tid;
	cr->id = tid;
	cr->flags |= CR_FLAG_STAT_REQ;
	if (cp->crflags & CR_RFLAG_TRACE) {
		DBG_TRACE("Object cod %ld stat req taskid %ld",
		cr->uol.cod, tid);
	}
	nkn_task_set_private(TASK_TYPE_CHUNK_MANAGER, tid, (void *)cr);
	//set the flag
        cp->flags |= CM_PRIV_REQ_ISSUED;
        /* Wait for completion of the MM/OM request */
        nkn_task_set_state(cp->id, TASK_STATE_EVENT_WAIT);

        nkn_task_set_state(tid, TASK_STATE_RUNNABLE);
        return 0;
}

static int
cr_create_get_task(cache_req_t *cr, cmpriv_t *cp)
{
	MM_get_resp_t *getr;
	nkn_task_id_t tid;
	nkn_provider_type_t ptype;

	getr = mmr_alloc(cr);
	if (getr == NULL)
		return ENOMEM;
        getr->in_num_content_objects = cr->num_bufs;
        getr->in_content_array = (void **)&cr->bufs;
	memcpy(&getr->in_client_data, &cp->in_client_data,
	       sizeof(nkn_client_data_t));
	getr->in_proto_data = cp->proto_data;
	getr->in_attr = (void *)cr->attrbuf;
	getr->in_ptype = cr->ptype;
	if (cp->crflags & CR_RFLAG_FIRST_REQUEST)
		getr->in_flags |= MM_FLAG_NEW_REQUEST;
	if (cp->crflags & CR_RFLAG_NO_CACHE)
		getr->in_flags |= (MM_FLAG_NO_CACHE | MM_FLAG_REQ_NO_CACHE);
	if (cp->flags & CM_PRIV_NO_CACHE)
		getr->in_flags |= MM_FLAG_NO_CACHE;
	if (cp->crflags & CR_RFLAG_TRACE)
		getr->in_flags |= MM_FLAG_TRACE_REQUEST;
	if (!(cp->flags & CM_PRIV_CHECK_EXPIRY))
		getr->in_flags |= MM_FLAG_IGNORE_EXPIRY;
	if (cp->flags & CM_PRIV_CONF_TIMED_WAIT_DONE)
		getr->in_flags |= MM_FLAG_CONF_NO_RETRY;
	if (cp->flags & CM_PRIV_UNCOND_RETRY_STATE) {
		getr->req_ctx = cp->request_ctxt;
		if (cp->retries >= cp->max_retries)
			getr->in_flags |= MM_FLAG_UNCOND_NO_RETRY;
	}

        tid = nkn_task_create_new_task(
                TASK_TYPE_MEDIA_MANAGER,
                TASK_TYPE_CHUNK_MANAGER,
                TASK_ACTION_INPUT,
                MM_OP_READ,
                (void *)getr,
                sizeof(getr),
                0);                   /* deadline = 100 msecs */

        if (tid < 0) {
		glob_media_get_task_allocfail++;
		mmr_free(getr);
		return ENOMEM;
	}

	glob_media_get_task_alloc++;
	cp->mm_task_id = tid;
	cr->flags |= CR_FLAG_GET_REQ;
	cr->id = tid;
	ptype = ((cr->ptype >> 24) & 0xff);
        if(ptype > NKN_MM_max_pci_providers)
            AO_fetch_and_add1(&glob_nkn_bm_to_om_read_task);
        else
            AO_fetch_and_add1(&glob_nkn_bm_to_dm2_read_task);

	if (cp->crflags & CR_RFLAG_TRACE) {
		DBG_TRACE("Object cod %ld get req taskid %ld",
		cr->uol.cod, tid);
	}
	nkn_task_set_private(TASK_TYPE_CHUNK_MANAGER, tid, (void *)cr);
        nkn_task_set_state(tid, TASK_STATE_RUNNABLE);
        return 0;
}

/* 
 * Trigger a GET task for MM 
 * Attach to an existing task if it is for the same physid.  Otherwise, 
 * create a new one and add it to the CR table for others to share.
 */
static int
cr_trigger_get_task(cache_req_t *cr, MM_stat_resp_t *statr, int *freecr)
{
	int ret, done=0, added=0, uncon_retry = 0;
	cache_req_t *ecr;		// existing cr in the crt
	nkn_provider_type_t ptype;
	/*
	 * Attach to an existing request with the same physid.  Otherwise,
	 * start a new one.  If the request attributes require a separate 
	 * request, we will not attempt to share.
	 */
	if (cr->cp->flags & CM_PRIV_PRIVATE) {
		cr->flags |= CR_FLAG_PRIVATE;
		glob_cr_cnt_private++;
	}
	if (cr->cp->flags & CM_PRIV_UNCOND_RETRY_STATE) {
		uncon_retry = done = 1;	
	}
	while (!done) {
		int recheck = 0;

		ecr = crt_get(cr, statr->physid, &statr->physid2, &recheck);
					/* returns with cr locked */
		if (ecr) {

			/*
			 * if bm_cfg_cr_max_queue_time is configured,
			 * the following code will support it.
			 */
			ptype = ((statr->ptype >> 24) & 0xff);
			if(bm_cfg_cr_max_queue_time &&
				((nkn_cur_ts - ecr->ts) >
				 ((time_t)bm_cfg_cr_max_queue_time)) &&
				(ptype > NKN_MM_max_pci_providers) &&
				(cr->cp->crflags & CR_RFLAG_FIRST_REQUEST)) {
				if (bm_cfg_cr_max_req) {
					if (ecr->numreq > bm_cfg_cr_max_req) {
						glob_bm_queue_timeout_reqout_tunnel_req++;
						/* Return error and this req tunnelled */
						crt_put(ecr);           /* unlocks cr */
						return NKN_BUF_QUEUE_TIMEOUT;
					}
				} else {
					glob_bm_queue_timeout_tunnel_req++;
					/* Return error and this req tunnelled */
					crt_put(ecr);           /* unlocks cr */
					return NKN_BUF_QUEUE_TIMEOUT;
				}
			}
			cr_cp_attach(ecr, cr->cp);
			crt_put(ecr);		/* unlocks cr */
			*freecr = 1;		/* free my own cr */
			glob_cr_cnt_shared++;
			return 0;
		}

		if (recheck)
			return EAGAIN;

		ptype = ((statr->ptype >> 24) & 0xff);
		if((cr->cp->flags & CM_PRIV_BM_ONLY) &&
			(ptype > NKN_MM_max_pci_providers)) {
			glob_cr_ingest_buf_not_avail++;
			return NKN_BUF_NOT_AVAILABLE;
		}

		/* Add in get related fields to the cr */
		if (!added) {
			ret = cr_add_getdata(cr, statr);
			if (ret)
				return ret;
			added++;
		}

		/* 
		 * If the specified length is zero, set it to the content
		 * length or page size.
		 */
		if (cr->uol.length == 0) {
			if (statr->tot_content_len > statr->media_blk_len)
				cr->uol.length = statr->media_blk_len;
			else
				cr->uol.length = statr->tot_content_len;
		}

		/* 
		 * Add to cr table.  This can fail in rare cases where we 
		 * raced with another task.  In this case, we will free the 
		 * one we just allocated and retry the get.
		 */
		if (!(cr->flags & CR_FLAG_PRIVATE)) {
			ret = crt_add(cr);
			if (ret != 0) {
				glob_cr_cnt_collision++;
				continue;
			}
		}

		NKN_MUTEX_LOCKSTAT(&cr->lock, &crlockstat);
		cr_cp_attach(cr, cr->cp);
		NKN_MUTEX_UNLOCKSTAT(&cr->lock);
		done++;
	}

	if (uncon_retry) {
		// fill the buffers this should alread be in crt table
		ret = cr_add_getdata(cr, statr);
		if (ret)
			return ret;
	}
	/* 
	 * We have now added a new request to the table.  We now need to
	 * start a task for the MM.
	 */
	ret = cr_create_get_task(cr, cr->cp);
	if (ret != 0) {
		cr_done(cr, ret, NULL);
		ret = NKN_BUF_TASKFAIL;
	}
	return ret;
}

/* Return a request back that was acquired via get */
void
cache_request_init()
{
	int t_index = 0;
	char name[100];
	for ( ; t_index < CRT_POOL ; ++t_index) {
		cr_table[t_index] = g_hash_table_new(g_str_hash, g_str_equal);
		cr_table2[t_index] = g_hash_table_new(g_int64_hash, g_int64_equal);
		snprintf(name, 100, "crt-mutex-%d", t_index);
		NKN_MUTEX_INIT(&crt_lock[t_index], NULL, name);
	}
	NKN_MUTEX_INIT(&rvq_lock, NULL, "rvq-mutex");
	NKN_MUTEX_INIT(&syncupdateq_lock, NULL, "sync-updateq-mutex");
	NKN_MUTEX_INIT(&hotupdateq_lock, NULL, "hot-updateq-mutex");
	NKN_LOCKSTAT_INIT(&crlockstat, NULL);

	nkn_mon_add("bm.global.obj_hotupdate.task_limit", NULL,
		&attr_hotnessupdate_tasks.limit, sizeof(attr_hotnessupdate_tasks.limit));
	nkn_mon_add("bm.global.obj_hotupdate.task_count", NULL,
		&attr_hotnessupdate_tasks.counter, sizeof(attr_hotnessupdate_tasks.counter));
	nkn_mon_add("bm.global.obj_hotupdate.task_overflow", NULL,
		&attr_hotnessupdate_tasks.overflow, sizeof(attr_hotnessupdate_tasks.overflow));
	nkn_mon_add("bm.global.obj_syncupdate.task_limit", NULL,
		&attr_syncupdate_tasks.limit, sizeof(attr_syncupdate_tasks.limit));
	nkn_mon_add("bm.global.obj_syncupdate.task_count", NULL,
		&attr_syncupdate_tasks.counter, sizeof(attr_syncupdate_tasks.counter));
	nkn_mon_add("bm.global.obj_syncupdate.task_overflow", NULL,
		&attr_syncupdate_tasks.overflow, sizeof(attr_syncupdate_tasks.overflow));
#if 0
	nkn_mon_add("bm.global.obj_delete.task_limit", NULL,
		&delete_tasks.limit, sizeof(delete_tasks.limit));
	nkn_mon_add("bm.global.obj_delete.task_count", NULL,
		&delete_tasks.counter, sizeof(delete_tasks.counter));
	nkn_mon_add("bm.global.obj_delete.task_overflow", NULL,
		&delete_tasks.overflow, sizeof(delete_tasks.overflow));
#endif
}

/*
 * Create a request for the media manager to get the requested content.
 * We start a STAT task and return.  That is followed by a GET task if
 * successful.  In either case, we report back completion asynchronously.
 */
int 
cache_request(nkn_uol_t *uol, cmpriv_t *cp)
{
	cache_req_t *cr;
	int ret;
	cr = cr_alloc(uol, cp);
	if (cr == NULL) {
		return ENOMEM;
	}
	ret = cr_create_stat_task(cr, cp);
	return ret;
}

/* Cancel any pending request associated with the cmpriv structure */
/* Not yet supported */
void
cache_request_cancel(cmpriv_t *cp)
{
	UNUSED_ARGUMENT(cp);
}

static nkn_reval_t *
rv_alloc(nkn_buf_t *attrbuf, namespace_token_t ns_token)
{
	nkn_buf_t *bp;
	nkn_reval_t *rv;
	int use;

	// Allocate second buffer to receive new attributes
	bp = nkn_buf_alloc(NKN_BUFFER_ATTR, nkn_buf_get_bufsize(attrbuf), 0);
	if (bp == NULL)
		return NULL;

	rv = (nkn_reval_t *)nkn_calloc_type(1, sizeof(nkn_reval_t), mod_bm_nkn_reval_t);
	if (rv == NULL) {
		nkn_buf_put(bp);
		return NULL;
	}

	rv->magic = NKN_REVAL_MAGIC;
	rv->newbuf = bp;
	nkn_buf_dup(attrbuf, NKN_BUF_IGNORE_STATS, &use, NULL, NULL, NULL);
	rv->oldbuf = attrbuf;
	rv->ns_token = ns_token;
	return rv;
}

static void
rv_free(nkn_reval_t *rv)
{
	if (VALID_NS_TOKEN(rv->ns_token))
		release_namespace_token_t(rv->ns_token);
	if (rv->oldbuf)
		nkn_buf_put(rv->oldbuf);
	if (rv->newbuf)
		nkn_buf_put(rv->newbuf);
	rv->oldbuf = NULL;
	rv->newbuf = NULL;
	free(rv);
}

/*
 * Create a task for updating attributes to the MM.  Returns 0 on success and
 * non-zero on failure in which case the caller retains ownership of rv.
 * Also, update the buffer cache by setting the ID on the new buffer.
 */

static int
cache_update_attributes(nkn_reval_t *rv, nkn_attr_t *newattr,
			MM_validate_t *mmv)
{
	nkn_task_id_t tid;
	
	nkn_buf_setid(rv->newbuf, &rv->oldbuf->id, 
		      nkn_buf_get_provider(rv->oldbuf),
		      NKN_BUFFER_UPDATE);

	// No need to update attributes if the provider was not a local cache
	// bug 8279 partial object provider will be origin
	if (rv->oldbuf->provider > NKN_MM_max_pci_providers)
		return 1;

	// Do not update in cache providers if about to expire
	if (newattr->cache_expiry <= (nkn_cur_ts + nkn_buf_min_update))
		return 1;

	if (cr_get_syncresource_slot(&attr_syncupdate_tasks)) {
		return 1;
	}

	rv->mmu.uol = rv->oldbuf->id;
	rv->mmu.in_attr = nkn_buf_get_content(rv->newbuf);
	rv->mmu.in_utype = MM_UPDATE_TYPE_EXPIRY;

        tid = nkn_task_create_new_task(
                TASK_TYPE_MEDIA_MANAGER,
                TASK_TYPE_CHUNK_MANAGER,
                TASK_ACTION_INPUT,
                MM_OP_UPDATE_ATTR,
                (void *)&rv->mmu,
                sizeof(rv->mmu),
                100);                   /* deadline = 200 msecs */

        if (tid < 0) {
		glob_media_update_task_allocfail++;
		cr_put_syncresource_slot(&attr_syncupdate_tasks);
		return tid;
	}
	glob_media_update_task_alloc++;
	nkn_task_set_private(TASK_TYPE_CHUNK_MANAGER, tid, (void *)rv);
	crv_done(rv->oldbuf, 0, mmv);
	if (rv->proto_free)
		nkn_free_con_client_data(rv->mmv.in_proto_data);
        nkn_task_set_state(tid, TASK_STATE_RUNNABLE);
	return 0;
}

/*
 * Write the attribte structure back to the MM providers.
 * Get a ref count for the buffer and release it after the task completes
 * method_type crrently supported :
 *   (a) ATTRIBUTE_SYNC - hotness sync internval check is done,
 *       if sync interval had elapsed, the update is sent down as 'SYNC' causing
 *	 the disk providers to write the attributes to disk,
 *	 else the update is sent down as a 'HOTNESS' update which will not
 *	 trigger any IO in the disk providers
 *   (b) ATTRIBUTE_UPDATE - no hotness sync interval check is done
 */
void
cache_write_attributes(nkn_obj_t *attrbuf,
                       nkn_attr_action_t method_type)
{
	nkn_attr_t *attr = attrbuf->buf.buffer;
	time_t old, new;
	nkn_sync_attr_t *sap;
	nkn_task_id_t tid;
	int use;
	int type = MM_UPDATE_TYPE_SYNC;

	// Do not sync for origin providers
	if (attrbuf->buf.provider > NKN_MM_max_pci_providers)
		return;

	// Must be a cached buffer
	if (!(attrbuf->buf.flags & NKN_BUF_CACHE))
		return;

	if (method_type == ATTRIBUTE_SYNC) {
		/* For now, we use a simple time based test which limits the
		 * update rate to the configured interval.
		 * There is a potential race here between the check and update
		 * but it is very unlikely and the consequences are not bad.
		 * For now, we have only ine scheduler thread, so this won't
		 * happen anyway. */
		old = am_decode_update_time((nkn_hot_t *)&attrbuf->orig_hotval);
		new = am_decode_update_time((nkn_hot_t *)&attr->hotval);
		if ((new <= old) || ((new - old) < glob_bm_cfg_attr_sync_interval)) {
		    if (cr_get_hotresource_slot(&attr_hotnessupdate_tasks)) {
			    return;
		    }
		    type = MM_UPDATE_TYPE_HOTNESS;
		} else {
		    if (cr_get_syncresource_slot(&attr_syncupdate_tasks)) {
			    return;
		    }
		    type = MM_UPDATE_TYPE_SYNC;
		}
	} else {
		    if (cr_get_syncresource_slot(&attr_syncupdate_tasks)) {
                            return;
                    }
	}


	sap = (nkn_sync_attr_t *)nkn_calloc_type(1, sizeof(nkn_sync_attr_t), mod_bm_nkn_sync_attr_t);
	if (sap == NULL) {
		glob_sync_attr_allocfail++;
		if (type == MM_UPDATE_TYPE_HOTNESS)
		    cr_put_hotresource_slot(&attr_hotnessupdate_tasks);
		else
		    cr_put_syncresource_slot(&attr_syncupdate_tasks);
		return; 
	}

	sap->magic = NKN_SYNC_MAGIC;
	nkn_buf_dup((nkn_buf_t *)attrbuf, NKN_BUF_IGNORE_STATS, &use, NULL, NULL, NULL);
	sap->attrbuf = (nkn_buf_t *)attrbuf;
	sap->mmu.uol = attrbuf->buf.id;
	sap->mmu.in_attr = nkn_buf_get_content(sap->attrbuf);
	sap->mmu.in_utype = type;

        tid = nkn_task_create_new_task(
                TASK_TYPE_MEDIA_MANAGER,
                TASK_TYPE_CHUNK_MANAGER,
                TASK_ACTION_INPUT,
                MM_OP_UPDATE_ATTR,
                (void *)&sap->mmu,
                sizeof(sap->mmu),
                100);                   /* deadline = 200 msecs */

        if (tid < 0) {
		glob_sync_attr_task_allocfail++;
		free(sap);
		if (type == MM_UPDATE_TYPE_HOTNESS)
		    cr_put_hotresource_slot(&attr_hotnessupdate_tasks);
		else
		    cr_put_syncresource_slot(&attr_syncupdate_tasks);
		nkn_buf_put((nkn_buf_t *)attrbuf);  /* refcnt down on failure*/
		return;
	}
	glob_sync_attr_task_alloc++;
	// orig_hotval needs to be reset only when attribute sync to disk is done
	if (type == MM_UPDATE_TYPE_SYNC)
		attrbuf->orig_hotval = attr->hotval;
	nkn_obj_unset_dirty(attrbuf);
	nkn_task_set_private(TASK_TYPE_CHUNK_MANAGER, tid, (void *)sap);
        nkn_task_set_state(tid, TASK_STATE_RUNNABLE);
	return;
}


/* cache invalidate request sent to all internal providers */
int
provider_cache_invalidate(nkn_uol_t *uol)
{
	nkn_reval_t *rv;
	nkn_task_id_t tid;
	nkn_buf_t *attrbuf; 
	if (cr_get_syncresource_slot(&attr_syncupdate_tasks)) {
	       glob_expire_attrib_resouce_unavail++;
               return 1;
	}
	rv = (nkn_reval_t *)nkn_calloc_type(1, sizeof(nkn_reval_t), mod_bm_nkn_reval_t);
	if (rv == NULL) {
		cr_put_syncresource_slot(&attr_syncupdate_tasks);
		return 1;	
	}
	rv->magic = NKN_REVAL_MAGIC;
	rv->newbuf = NULL;
	attrbuf = nkn_buf_get(uol, NKN_BUFFER_ATTR, NKN_BUF_IGNORE_STATS);   // ref counted
	if (attrbuf == NULL) {
		cr_put_syncresource_slot(&attr_syncupdate_tasks);
		return 1;
	}	
	rv->oldbuf = attrbuf;
	rv->ns_token = NULL_NS_TOKEN;
	rv->mmu.uol = attrbuf->id;
	rv->mmu.in_attr = nkn_buf_get_content(attrbuf);
	rv->mmu.in_utype = MM_UPDATE_TYPE_EXPIRY;
	rv->mmu.in_attr->na_flags2 |= NKN_OBJ_INVALID;

	tid = nkn_task_create_new_task(
                TASK_TYPE_MEDIA_MANAGER,
                TASK_TYPE_CHUNK_MANAGER,
                TASK_ACTION_INPUT,
                MM_OP_UPDATE_ATTR,
                (void *)&rv->mmu,
                sizeof(rv->mmu),
                0);                   /* deadline = 0 msecs */

       	if (tid < 0) {
               glob_media_update_task_allocfail++;
               cr_put_syncresource_slot(&attr_syncupdate_tasks);
	       nkn_buf_put(attrbuf);
               return tid;
       	}
	glob_media_update_task_alloc++;
	nkn_task_set_private(TASK_TYPE_CHUNK_MANAGER, tid, (void *)rv);
	nkn_task_set_state(tid, TASK_STATE_RUNNABLE);
	return 0;
}

static int
cache_expire_provider_attributes(nkn_reval_t *rv,
				 int mmret,
				 MM_validate_t *mmv)
{
       nkn_task_id_t tid;
       if (cr_get_syncresource_slot(&attr_syncupdate_tasks)) {
	       glob_expire_attrib_resouce_unavail++;
               return 1;
       }
       rv->mmu.uol = rv->oldbuf->id;
       rv->mmu.in_attr = nkn_buf_get_content(rv->oldbuf);
       rv->mmu.in_utype = MM_UPDATE_TYPE_EXPIRY;

        tid = nkn_task_create_new_task(
                TASK_TYPE_MEDIA_MANAGER,
                TASK_TYPE_CHUNK_MANAGER,
                TASK_ACTION_INPUT,
                MM_OP_UPDATE_ATTR,
                (void *)&rv->mmu,
                sizeof(rv->mmu),
                100);                   /* deadline = 200 msecs */

        if (tid < 0) {
               glob_media_update_task_allocfail++;
               cr_put_syncresource_slot(&attr_syncupdate_tasks);
               return tid;
       }
       glob_media_update_task_alloc++;
       nkn_task_set_private(TASK_TYPE_CHUNK_MANAGER, tid, (void *)rv);
       crv_done(rv->oldbuf, mmret, mmv);
       if (rv->proto_free)
               nkn_free_con_client_data(rv->mmv.in_proto_data);
       nkn_task_set_state(tid, TASK_STATE_RUNNABLE);
       return 0;

}

#if 0
/*
 * Create a task for deleting a cached object to MM if required.
 */
void
cache_delete(nkn_obj_t *attrbuf, int cond_check);
void
cache_delete(nkn_obj_t *attrbuf, int cond_check)
{
	nkn_task_id_t tid;
	MM_delete_resp_t *del;
	if (cond_check && (attrbuf->buf.provider > NKN_MM_max_pci_providers))
		return;

	if (cr_get_resource_slot(&delete_tasks))
		return;

	del = (MM_delete_resp_t *)nkn_calloc_type(1, sizeof(MM_delete_resp_t), mod_bm_nkn_del_t);
	if (del == NULL) {
		glob_bufmgr_delete_allocfail++;
		cr_put_resource_slot(&delete_tasks);
		return;
	}

	// Set the COD, other fields are defaulted to zero values.
	// Need a ref cnt since the task is async
	del->in_uol.cod = nkn_cod_dup(attrbuf->buf.id.cod, NKN_COD_BUF_MGR);
	assert(del->in_uol.cod != NKN_COD_NULL);
	del->dm2_del_reason = NKN_DEL_REASON_REVAL_FAILED;

        tid = nkn_task_create_new_task(
                TASK_TYPE_MEDIA_MANAGER,
                TASK_TYPE_CHUNK_MANAGER,
                TASK_ACTION_INPUT,
                MM_OP_DELETE,
                (void *)del,
                sizeof(MM_delete_resp_t),
                200);                   /* deadline = 200 msecs */

        if (tid < 0) {
		nkn_cod_close(del->in_uol.cod, NKN_COD_BUF_MGR);
		glob_bufmgr_delete_task_allocfail++;
		cr_put_resource_slot(&delete_tasks);
		free(del);   /* free del */
		return;
	}
	glob_bufmgr_delete_tasks++;
        nkn_task_set_state(tid, TASK_STATE_RUNNABLE);
	return;
	
}
#endif


/* Notify each requestor */
static void
crv_done(nkn_buf_t *attrbuf, int err, MM_validate_t *mmv)
{
	cmpriv_t *cp = NULL;
	int flags = 0;
	nkn_obj_t *objp = (nkn_obj_t *)attrbuf;

	if (mmv) {
		cp = mmv->cpointer;
		flags = mmv->in_flags;
	}
	if (!cp) {
		NKN_MUTEX_LOCK(&rvq_lock);
		if (!(objp->validate_pending)) {
			NKN_MUTEX_UNLOCK(&rvq_lock);
			return;
		}
		if (!(TAILQ_FIRST(&objp->revalq))) {
			// If there are no requestors to notify
			// and the object is modified,
			// we need to expire the buffer
			if (err) {
				nkn_buf_expire(attrbuf);
			}
			if (err == MM_VALIDATE_MODIFIED)
				nkn_buf_set_modified(attrbuf);
		}
		objp->validate_pending = 0;
		while ((cp = (cmpriv_t *)TAILQ_FIRST(&objp->revalq))) {
			TAILQ_REMOVE(&objp->revalq, cp, revallist);
			cp->errcode = err;
			// Copy out_proto_resp for use by higher layers.
			if ( mmv ) {
				cp->out_proto_resp = mmv->out_proto_resp;
			}
			cache_revalidation_done(cp);
		}
		NKN_MUTEX_UNLOCK(&rvq_lock);
	} else {
		cp->errcode = err;
		cp->out_proto_resp = mmv->out_proto_resp;
		cache_revalidation_done(cp);
	}
}


/*
 * Revalidate cache contents for the requested object if required.  This is
 * done by sending a revalidate task to MM under the following conditions.
 *   - the object is larger than a configured value
 *   - the object's expiry time is within a configured window or the object
 *     has already expired.
 *   - a previous revalidation is not pending
 * If cp is non-null, a completion is reported back.
 */
int
cache_revalidate(nkn_buf_t *attrbuf, 
		 namespace_token_t ns_token, 
		 cmpriv_t *cp,
		 nkn_client_data_t *in_client_data,
		 uint32_t force_revalidation,
		 void *in_proto_data)
{
	nkn_reval_t *rv;
	nkn_attr_t *old, *new;
	nkn_obj_t *objp = (nkn_obj_t *)attrbuf;
	const namespace_config_t *nsc;
	int expired = 0;
	int sync_reval = 0;

	// No need to revalidate non-cacheable attributes
	if (!(attrbuf->flags & NKN_BUF_CACHE)) {
		return 1;
	}
	old = (nkn_attr_t *)nkn_buf_get_content(attrbuf);

	/* force reval always will not
           a. check for expiry
	   b. invalidate the expiry
	   c. check for pre-expiry
	*/
	/* force reval will invalidate the cache and will
		   mark the cache as expired
		   force cond reval will check for expiry and will
		   invalidate the cache
		*/
	if ((force_revalidation & FORCE_REVAL) ||
		((force_revalidation & FORCE_REVAL_COND_CHECK)
		&& (old->cache_expiry > nkn_cur_ts))) {
		old->cache_expiry = nkn_cur_ts - 1;
		if (!(force_revalidation & FORCE_REVAL_ALWAYS))
			old->na_flags |= NKN_OBJ_FORCEREVAL;
	}
	if (!(force_revalidation & FORCE_REVAL_ALWAYS)) {

		expired = (old->cache_expiry <= nkn_cur_ts) ? 1 : 0;
		if (!expired && (old->content_length < nkn_buf_reval_size))
			return 1;

		// Check for pre-expiry condition
		if (!expired && ((objp->reval_ts > (nkn_cur_ts & TIME32_MASK)) || 
			(force_revalidation & NO_FORCE_REVAL_EXPIRED)))
			return 1;

		// Prevent further revalidations if already marked modified
		// In the pre-expiry window we can continue to serve and
		// after expiry, the object must be refetched.
		if (objp->flags & NKN_OBJ_MODIFIED)
			return 1;
	} else {
		if (cp && (cp->flags & CM_PRIV_SYNC_REVALALWAYS))
			sync_reval = 1;
	}

	/* check for streaming */
	if (nkn_attr_is_streaming(old))
		return 1;

	nsc = namespace_to_config(ns_token);
	if (nsc) {
		if (!nsc->http_config->policies.cache_revalidate.allow_cache_revalidate) {
			release_namespace_token_t(ns_token);
			return 1;
		}
	} else
		return 1;


	// If completion notification is required, attach to the list of 
	// requestors
	if (!sync_reval) {
		NKN_MUTEX_LOCK(&rvq_lock);
		if (cp) {
			TAILQ_INSERT_TAIL(&objp->revalq, cp, revallist);
		}
		if (objp->validate_pending) {
			NKN_MUTEX_UNLOCK(&rvq_lock);
			release_namespace_token_t(ns_token);
			return 0;
		}
		objp->validate_pending = 1;
		NKN_MUTEX_UNLOCK(&rvq_lock);
		rv = rv_alloc(attrbuf, ns_token);
		if (rv == NULL) {
			// Remove this request and report error on the rest
			if (cp) {
				NKN_MUTEX_LOCK(&rvq_lock);
				TAILQ_REMOVE(&objp->revalq, cp, revallist);
				NKN_MUTEX_UNLOCK(&rvq_lock);
			}
			crv_done(attrbuf, ENOMEM, NULL);
			release_namespace_token_t(ns_token);
			return 1;
		}
		if (force_revalidation & NO_FORCE_REVAL_EXPIRED)
			rv->flags |= CM_RV_DELIVER_EXP;
	} else {
		rv = rv_alloc(attrbuf, ns_token);
		if (rv == NULL) {
			release_namespace_token_t(ns_token);
			return 1;
		}
		rv->mmv.cpointer = cp;
		rv->flags |= CM_RV_SYNC_REVAL;
		if (cp->flags & CM_PRIV_SYNC_REVALTUNNEL)
		    rv->flags |= CM_RV_SYNC_REVALTUNNEL;
	}
	if (force_revalidation & FORCE_REVAL_ALWAYS) {
		expired = (old->cache_expiry <= nkn_cur_ts) ? 1 : 0;
                if (!expired)
			rv->flags |= CM_RV_CACHE_NOUPDATE;
                rv->flags |= CM_RV_FORCEREVAL_ALWAYS;
	}

	if (expired)
		glob_bufmgr_reval_on_expire++;
	rv->mmv.in_uol = attrbuf->id;
	rv->mmv.ns_token = ns_token;


	// store requesting protocol and data to MM validate
	memcpy(&rv->mmv.in_client_data, in_client_data, sizeof(nkn_client_data_t));
	rv->proto_free = 0;
	if (cp)
	{
		rv->mmv.in_proto_data = in_proto_data;
	} else {
		if (nkn_copy_con_client_data(1, in_proto_data,
			&rv->mmv.in_proto_data) == 0) {
			rv->proto_free = 1;
		}
	}

	new = (nkn_attr_t *)nkn_buf_get_content(rv->newbuf);
	// copy the old attributes
	nkn_attr_docopy(old, new, old->total_attrsize);
	rv->mmv.in_attr = old;
	rv->mmv.new_attr = rv->newbuf;

        rv->tid = nkn_task_create_new_task(
                TASK_TYPE_MEDIA_MANAGER,
                TASK_TYPE_CHUNK_MANAGER,
                TASK_ACTION_INPUT,
                MM_OP_VALIDATE,
                (void *)&rv->mmv,
                sizeof(rv->mmv),
                100);                   /* deadline = 200 msecs */

        if (rv->tid < 0) {
		glob_media_reval_task_allocfail++;
		// Remove this request and report error on the rest
		if (cp && !sync_reval) {
			NKN_MUTEX_LOCK(&rvq_lock);
			TAILQ_REMOVE(&objp->revalq, cp, revallist);
			NKN_MUTEX_UNLOCK(&rvq_lock);
		}
		if (!sync_reval)
			crv_done(attrbuf, ENOMEM, NULL);
		if (rv->proto_free)
			nkn_free_con_client_data(rv->mmv.in_proto_data);
		rv_free(rv);
		return 1;
	}

	glob_media_reval_task_alloc++;
	AO_fetch_and_add1(&glob_nkn_bm_to_om_validate_task);
	nkn_task_set_private(TASK_TYPE_CHUNK_MANAGER, rv->tid, (void *)rv);
	if (cp) {
                cp->flags |= CM_PRIV_REVAL_ISSUED;
                /* wait for revalidation */
                nkn_task_set_state(cp->id, TASK_STATE_EVENT_WAIT);
	}
        nkn_task_set_state(rv->tid, TASK_STATE_RUNNABLE);
        return 0;
}

/*
 * Handle completion of STAT task
 * If the stat worked, we create a GET task and return.  Otherwise, we
 * need to report a back a failed completion.
 */
static void
cr_stat_task_done(nkn_task_id_t taskid)
{
	MM_stat_resp_t *statr;
	cache_req_t *cr;
	int ret=0, freecr=0;

	cr = (cache_req_t *)nkn_task_get_private(TASK_TYPE_CHUNK_MANAGER, taskid);
	if (!cr) {
	    NKN_ASSERT(0);
	    return;
	}
	statr = (MM_stat_resp_t *)nkn_task_get_data(taskid);
	cr->flags |= CR_FLAG_STAT_DONE;

	if (statr->mm_stat_ret) {
		glob_mmstat_fail++;
		ret = statr->mm_stat_ret;
	} else {
		if (statr->in_flags & MM_FLAG_TRACE_REQUEST) {
			DBG_TRACE("Object cod %ld stat done taskid %ld",
			cr->uol.cod, taskid);
		}
		ret = cr_trigger_get_task(cr, statr, &freecr); 	
		// cr may get freed if we use an existing one
		if (freecr)
			cr_free(cr, ret, cr->cp);
	}
	if (ret && (ret != NKN_BUF_TASKFAIL)) {
		cr->cp->errcode = ret;
		if (cr->cp->flags & CM_PRIV_UNCOND_RETRY_STATE)
		{
			cr_free_uncond_retry(cr, ret, cr->cp);
		}
		cache_request_done(cr->cp, cr);
		cr_free(cr, ret, NULL);
	}

	free(statr);
	nkn_task_set_action_and_state(taskid, TASK_ACTION_CLEANUP,
					TASK_STATE_RUNNABLE);
}

/*
 * Handle completion of GET task
 */
static void
cr_get_task_done(nkn_task_id_t taskid)
{
	MM_get_resp_t *getr;
	cache_req_t *cr;
	nkn_provider_type_t ptype;
	int ret=0;

	cr = (cache_req_t *)nkn_task_get_private(TASK_TYPE_CHUNK_MANAGER, taskid);
	if (!cr) {
	    NKN_ASSERT(0);
	    return;
	}
	getr = (MM_get_resp_t *)nkn_task_get_data(taskid);
	cr->flags |= CR_FLAG_GET_DONE;
	ret = getr->err_code;
	ptype = ((cr->ptype >> 24) & 0xff);
        if(ptype > NKN_MM_max_pci_providers)
            AO_fetch_and_sub1(&glob_nkn_bm_to_om_read_task);
        else
            AO_fetch_and_sub1(&glob_nkn_bm_to_dm2_read_task);

	// It is possible for a DELETE to take place between a STAT and a 
	// GET.  To account for that, we convert an ENOENT here to an 
	// EAGAIN so that the upper layer has the option to retry the request.
	if (ret == ENOENT)
		ret = EAGAIN;

	/* 
	 * If no buffers were filled, treat this as a failure.
	 */
	if (!ret && !getr->out_num_content_objects &&
		!getr->out_num_data_buffers) {
		// printf("failed to get buffers for %s\n", cr->uol.uri);
		ret = ENOENT;
	}
	if (getr->in_flags & MM_FLAG_TRACE_REQUEST) {
		DBG_TRACE("Object cod %ld get done taskid %ld",
		cr->uol.cod, taskid);
	}
	cr->out_bufs = getr->out_num_content_objects;	

	// Add in any buffers allocated by the provider
	if (getr->out_num_data_buffers) {
		uint32_t i;
		assert(getr->out_num_data_buffers <= MM_MAX_OUT_BUFS);
		for (i=0; i<getr->out_num_data_buffers; i++)
			cr->bufs[cr->num_bufs++] = getr->out_data_buffers[i];
		cr->out_bufs = cr->num_bufs;
	}
	cr_done(cr, ret, getr);	
        nkn_task_set_action_and_state(taskid, TASK_ACTION_CLEANUP,
					TASK_STATE_RUNNABLE);

	mmr_free(getr);	
}

static void
cr_validate_retry(void *prv)
{
	nkn_reval_t *rv;
	nkn_task_id_t tid, cleanuptid;
	cmpriv_t *cp;
	rv = (nkn_reval_t *) prv;

	if (rv->flags & CM_RV_UNCOND_TIMED_WAIT) {
		if (rv->retries >= rv->mmv.max_retries)
			rv->mmv.in_flags = MM_FLAG_UNCOND_NO_RETRY;
	} else if (rv->flags & CM_RV_CONF_TIMED_WAIT) {
		rv->mmv.in_flags = MM_FLAG_CONF_NO_RETRY;
	}

	tid = nkn_task_create_new_task(
                TASK_TYPE_MEDIA_MANAGER,
                TASK_TYPE_CHUNK_MANAGER,
                TASK_ACTION_INPUT,
                MM_OP_VALIDATE,
                (void *)&rv->mmv,
                sizeof(rv->mmv),
                100);    
	if (tid < 0) {
		glob_media_reval_task_allocfail++;
		if (rv->flags & CM_RV_SYNC_REVAL) {
			cp = rv->mmv.cpointer;
			if (cp) {
				cp->errcode = ENOMEM;
				cp->out_proto_resp = rv->mmv.out_proto_resp;
				cache_revalidation_done(cp);
			}
		} else {
			crv_done(rv->oldbuf, ENOMEM, NULL);
		}
		if (rv->proto_free)
			nkn_free_con_client_data(rv->mmv.in_proto_data);
		cleanuptid = rv->tid;	
		rv_free(rv);
		nkn_task_set_action_and_state(cleanuptid, TASK_ACTION_CLEANUP,
						TASK_STATE_RUNNABLE);
		return;
	}
	glob_media_reval_task_alloc++;
	nkn_task_set_private(TASK_TYPE_CHUNK_MANAGER, tid, (void *)rv);
        nkn_task_set_state(tid, TASK_STATE_RUNNABLE);
}

/*
 * Handle completion of VALIDATE task
 * Update the attributes if the validation succeeded.  Otherwise, the buffers
 * and cache will get purged upon expiry.
 */
static void
cr_validate_task_done(nkn_task_id_t taskid)
{
	MM_validate_t *mmv;
	nkn_reval_t *rv;
	MM_validate_ret_codes_t mmret;
	int ret, freerv, debug_flag;
	nkn_attr_t *oattr, *nattr;
	nkn_task_id_t cleanupid;
	nkn_obj_t *obj;
	uint32_t next_avail, flags = 0;
	nkn_cod_t cod;

	rv = (nkn_reval_t *)nkn_task_get_private(TASK_TYPE_CHUNK_MANAGER, taskid);
	if (!rv) {
	    NKN_ASSERT(0);
	    return;
	}
	oattr = (nkn_attr_t *)(nkn_buf_get_content(rv->oldbuf));
	nattr = (nkn_attr_t *)(nkn_buf_get_content(rv->newbuf));
	mmv = (MM_validate_t *)nkn_task_get_data(taskid);
	mmret = mmv->ret_code;
	cod = mmv->in_uol.cod;
	AO_fetch_and_sub1(&glob_nkn_bm_to_om_validate_task);
	if (rv->tid != taskid) {
		nkn_task_set_action_and_state(taskid, TASK_ACTION_CLEANUP,
						TASK_STATE_RUNNABLE);
	}

	// unconditional/ conditional retry
	if (mmret == NKN_MM_CONF_RETRY_REQUEST) {
		mmv->ret_code = 0;
		rv->flags |= CM_RV_CONF_TIMED_WAIT;
		ret = nkn_rttask_delayed_exec(mmv->out_delay_ms, mmv->out_delay_ms,
					      cr_validate_retry, rv, &next_avail);
		if (ret == -1) {
			DBG_LOG(ERROR, MOD_BM, "conditional Reval Event wait failed for %s, %d",
						nkn_cod_get_cnp(mmv->in_uol.cod), mmv->out_delay_ms);
			mmret = MM_VALIDATE_ERROR;
		} else {
			glob_cr_reval_conf_timed_wait++;
			return;
		}
	}

	if (mmret == NKN_MM_UNCOND_RETRY_REQUEST) {
		if (rv->retries == mmv->max_retries) {
			DBG_LOG(ERROR, MOD_BM, "Reval max retries done for %s tries %d",
				nkn_cod_get_cnp(mmv->in_uol.cod), rv->retries);
			mmret = MM_VALIDATE_ERROR;
		} else {
			mmv->ret_code = 0;
			rv->flags |= CM_RV_UNCOND_TIMED_WAIT;
			++rv->retries;
			ret = nkn_rttask_delayed_exec(mmv->out_delay_ms, mmv->out_delay_ms,
					      cr_validate_retry, rv, &next_avail);
			if (ret == -1) {
				DBG_LOG(ERROR, MOD_BM, "un conditional Reval Event wait failed for %s, %d",
						nkn_cod_get_cnp(mmv->in_uol.cod), mmv->out_delay_ms);
				mmret = MM_VALIDATE_ERROR;
			} else {
				glob_cr_reval_uncond_timed_wait++;
				return;
			}
		}
	}
	flags = rv->flags;
	cleanupid = rv->tid;

	if ((mmret != MM_VALIDATE_NOT_MODIFIED) && (flags & CM_RV_SYNC_REVALTUNNEL)) {
		if (!(flags & CM_RV_CACHE_NOUPDATE)) {
			nkn_buffer_purge(&rv->mmv.in_uol);
			nkn_cod_set_expired(rv->mmv.in_uol.cod);
		}
		mmret = MM_FLAG_VALIDATE_TUNNEL;
	}
	// Validation checks to ensure that we can update the attributes
	if (mmret == MM_VALIDATE_NOT_MODIFIED) {
		// Check that the content length of the object has not changed
		if (oattr->content_length != nattr->content_length) {
			glob_bm_attr_update_mismatch++;
			mmret = MM_VALIDATE_MODIFIED;	// treat as error
		}
	
		// If the new attributes have already reached expiry time,
		// we still honor the revalidation success for the pending
		// requests.  This can happen if the Origin server 
		// has a large clock skew relative to us or specifies a
		// very small max-age value.  New requests will have to
		// revalidate again.
		// if (nattr->cache_expiry <= nkn_cur_ts) {
		// 	glob_bm_attr_update_expired++;
		// 	mmret = MM_VALIDATE_ERROR;	// treat as error
		// }
	}

	// If the object is not modified, we can update the attributes and
	// leave it in the cache.  Otherwise, we let it expire based on the
	// existing attributes.
	if (mmret == MM_VALIDATE_NOT_MODIFIED) {
		nattr->cache_reval_time = nkn_cur_ts;
		freerv = 1;
		if (!(flags & CM_RV_CACHE_NOUPDATE))
			freerv = cache_update_attributes(rv, nattr, mmv);
		if (freerv) {	// failed to start task
			crv_done(rv->oldbuf, 0, mmv);
			if (rv->proto_free)
				nkn_free_con_client_data(rv->mmv.in_proto_data);
			rv_free(rv);
		}
		glob_mmvalidate_done++;
	} else {
		freerv = 1;
		if (flags & CM_RV_DELIVER_EXP) {
			mmv->in_flags |= MM_VALIDATE_EXP_DELIVERY;
		}
		if (mmret == MM_VALIDATE_FAIL) {
			glob_mmvalidate_fail++;
			/* Delete the cached object, since this is a
			 * permanent failure, by marking the object in the cache
			 * as expired. To be very safe, mark the expiry time
			 * 1 sec earlier from the current time */
			oattr->cache_expiry = nkn_cur_ts - 1;
			obj =(nkn_obj_t *)rv->oldbuf;
			/* Now mark the object as invalid in attribute so that
			   further requests will treat this as miss.
			*/
			oattr->na_flags2 |= NKN_OBJ_INVALID;
			freerv = cache_expire_provider_attributes(rv, mmret, mmv);
		} else if ((mmret == MM_VALIDATE_MODIFIED) ||
				 (mmret == MM_VALIDATE_ERROR) ||
				 (mmret == MM_VALIDATE_INVALID)) {
			nattr->cache_reval_time = 0;
			int disk_update = 0;
			if (oattr->na_flags & NKN_OBJ_FORCEREVAL) {
				oattr->na_flags &= ~NKN_OBJ_FORCEREVAL;
				flags &= ~CM_RV_FORCEREVAL_ALWAYS;
				freerv = cache_expire_provider_attributes(rv, mmret, mmv);
				disk_update = 1;
			}
			// debug code to find oattr flags
			if (!freerv && (flags & CM_RV_FORCEREVAL_ALWAYS)) {
			    debug_flag = flags;
			    NKN_ASSERT(0);
			}
			if (freerv && (flags & CM_RV_FORCEREVAL_ALWAYS)) {
				nkn_cod_set_expired(cod);
				oattr->cache_expiry = nkn_cur_ts - 1;
				oattr->na_flags2 |= NKN_OBJ_INVALID;
				freerv = cache_expire_provider_attributes(rv, mmret, mmv);
				disk_update = 1;
			}

			if (!disk_update && oattr->cache_expiry <= nkn_cur_ts) {
				if (!(mmret == MM_VALIDATE_ERROR &&
					flags & CM_RV_DELIVER_EXP)) {
					oattr->na_flags2 |= NKN_OBJ_INVALID;
					freerv = cache_expire_provider_attributes(rv, mmret, mmv);
				}
			} 
			if (mmret == MM_VALIDATE_MODIFIED)
				glob_mmvalidate_mod++;
			else {
				glob_mmvalidate_error++;
				if (mmret == MM_VALIDATE_ERROR &&
					(flags & CM_RV_DELIVER_EXP))
					mmret = 0;
			}
			
		} else if (mmret == MM_VALIDATE_BYPASS) {
			// bypass the cache object and tunnel the request
			mmret = MM_FLAG_VALIDATE_TUNNEL;
			glob_mmvalidate_bypass++;
		}
		if (freerv) {
			crv_done(rv->oldbuf, mmret, mmv);
			if (rv->proto_free)
				nkn_free_con_client_data(rv->mmv.in_proto_data);
			rv_free(rv);
		}
	}

	nkn_task_set_action_and_state(cleanupid, TASK_ACTION_CLEANUP,
					TASK_STATE_RUNNABLE);
}

/*
 * Handle completion of UPDATE ATTR task
 * No logic to execute.  Simply free rv.
 */
static void
cr_update_task_done(nkn_task_id_t taskid)
{
	nkn_reval_t *rv;
	

	// Could be one of nkn_reval_t or nkn_sync_attr_t
	rv = (nkn_reval_t *)nkn_task_get_private(TASK_TYPE_CHUNK_MANAGER, taskid);
	if (!rv) {
	    NKN_ASSERT(0);
	    return;
	}
	if (rv->magic == NKN_REVAL_MAGIC) {
		rv_free(rv);
		cr_put_syncresource_slot(&attr_syncupdate_tasks);
	} else if (rv->magic == NKN_SYNC_MAGIC) {
		nkn_sync_attr_t *sap = (nkn_sync_attr_t *)rv;
		
		nkn_buf_put(sap->attrbuf);
		if (sap->mmu.in_utype == MM_UPDATE_TYPE_HOTNESS)
		    cr_put_hotresource_slot(&attr_hotnessupdate_tasks);
		else
		    cr_put_syncresource_slot(&attr_syncupdate_tasks);

		free(sap);
	} else
		assert(0);


	nkn_task_set_action_and_state(taskid, TASK_ACTION_CLEANUP,
					TASK_STATE_RUNNABLE);
}

#if 0
/*
 * Handle completion of DELETE task
 * No logic to execute.  Simply log and free resources.
 */
static void
cr_delete_task_done(nkn_task_id_t taskid)
{
	MM_delete_resp_t *del;

	// Could be one of nkn_reval_t or nkn_sync_attr_t
	del = (MM_delete_resp_t *)nkn_task_get_data(taskid);
	nkn_cod_close(del->in_uol.cod, NKN_COD_BUF_MGR);
	if (del->out_ret_code)
		glob_bufmgr_delete_tasks_failed++;
	else
		glob_bufmgr_delete_tasks_done++;
	free(del);
	cr_put_resource_slot(&delete_tasks);

	nkn_task_set_action_and_state(taskid, TASK_ACTION_CLEANUP,
					TASK_STATE_RUNNABLE);
}
#endif

/*
 * Handler for completion of MM task.  Here we need to return all the buffers
 * to the buffer cache and process completion on the request structure.
 */
void
chunk_mgr_output(nkn_task_id_t taskid)
{
	struct nkn_task *tp;
	op_t op;

	// TODO - replace with call to sched to get op
	tp = nkn_task_get_task(taskid);

	if (!tp) {
	    NKN_ASSERT(0);
	    return;
	}

	op = tp->dst_module_op;

	if (op == MM_OP_STAT)
		cr_stat_task_done(taskid);
	else if (op == MM_OP_READ)
		cr_get_task_done(taskid);
	else if (op == MM_OP_VALIDATE)
		cr_validate_task_done(taskid);
	else if (op == MM_OP_UPDATE_ATTR)
		cr_update_task_done(taskid);
#if 0
	else if (op == MM_OP_DELETE)
		cr_delete_task_done(taskid);
#endif
	else
		assert(0);
}

/*
 * Funciton for updation of debug infomation of MM tasks which have waited for
 * a long time
 */
void
cache_update_debug_data(struct nkn_task *ntask)
{
    op_t op;
    cache_req_t *cr;
    nkn_provider_type_t ptype;

    op = ntask->dst_module_op;
    switch(op) {
	case MM_OP_STAT:
	    glob_nkn_sched_mm_stat_task++;
	    break;
	case MM_OP_READ:
	    cr = (cache_req_t *)ntask->src_module_data;
	    if(cr) {
		ptype = ((cr->ptype >> 24) & 0xff);
		if(ptype > NKN_MM_max_pci_providers)
		    glob_nkn_sched_mm_origin_read_task++;
		else
		    glob_nkn_sched_mm_dm2_read_task++;
	    }
	    break;
	case MM_OP_VALIDATE:
	    glob_nkn_sched_mm_origin_validate_task ++;
	    break;
	case MM_OP_UPDATE_ATTR:
	    glob_nkn_sched_mm_dm2_update_attr_task++;
	    break;
#if 0
	case MM_OP_DELETE:
	    glob_nkn_sched_mm_dm2_delete_task++;
	    break;
#endif
	default:
	    break;
    }
}


static void hash_func(void *key, void *value, void *userdata);
static void hash_func(void *key, void *value, void *userdata)
{
	key = key;
	cache_req_t *cr;
	FILE *fp = (FILE *)userdata;
	if (value) {
		cr = (cache_req_t *)value;
		fprintf(fp, "%s\t%ld\t%ld\t%d\t%ld\n", nkn_cod_get_cnp(cr->uol.cod), cr->uol.offset, cr->uol.length, cr->numreq, cr->physid2);
	}
}

static void hash_func1(void *key, void *value, void *userdata);
static void hash_func1(void *key, void *value, void *userdata)
{
	key = key;
	cache_req_t *cr;
	FILE *fp = (FILE *)userdata;
	if (value) {
		cr = (cache_req_t *)value;
		fprintf(fp, "%s\t%ld\t%ld\t%d\t%s\n", nkn_cod_get_cnp(cr->uol.cod), cr->uol.offset, cr->uol.length, cr->numreq, cr->physid);
	}
}


void nkn_bm_req_entries(FILE *fp)
{
	int t_index;
	if (!fp)
		return;
	fprintf(fp, "uri    \t\t   offset   \t   length   \t  num-requests   \t   physid  \n");
	for (t_index = 0 ; t_index < CRT_POOL ; ++t_index) {
		NKN_MUTEX_LOCK(&crt_lock[t_index]);
		g_hash_table_foreach (cr_table2[t_index], hash_func, fp);
		g_hash_table_foreach (cr_table[t_index], hash_func1, fp);
		NKN_MUTEX_UNLOCK(&crt_lock[t_index]);
	}
	fprintf(fp, "---------------------------------------------------------\n");
	fflush(fp);
}


