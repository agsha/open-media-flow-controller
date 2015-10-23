#ifndef CACHE_MANAGER_H
#define CACHE_MANAGER_H
#include "nkn_buf.h"
#include "nkn_sched_api.h"
#include "nkn_defs.h"
#include "nkn_namespace.h"

/* Queue of requestors for the same physid */
struct cm_priv;		// forward declaration
TAILQ_HEAD(request_queue, cm_priv);

/*physid string format definition
 ------------------------------------
 | module_  | cod_    | offset + '\0'|
 | 5 bytes +| 8 byte +|    8 byte +  |
 | 1 byte   | 1 byte  | 1 byte       |
 ------------------------------------
 */
#define NKN_PHYSID_MAXLEN   40
/*
 * Request structure used to cordinate media requests.
 */
#define MAX_BUFS_PER_TASK	64
#define CR_FLAG_STAT_REQ	1
#define CR_FLAG_STAT_DONE	2
#define CR_FLAG_GET_REQ		4
#define CR_FLAG_GET_DONE	8
#define CR_FLAG_PRIVATE		0x10	// Not in the CRT to be shared
typedef struct {
	nkn_buf_t *attrbuf;			/* attribute buffer */
	nkn_buf_t *bufs[MAX_BUFS_PER_TASK];
	uint32_t num_bufs     : 16;		// MAX is MAX_BUFS_PER_TASK
	uint32_t crt_index    : 16;		// crt hash index
	int out_bufs;				/* buffers with data */
	nkn_uol_t uol;
	namespace_token_t ns_token;		/* associated namespace */
	int flags;				/* state of the request */
	nkn_task_id_t id;			/* associated task */
	char physid[NKN_PHYSID_MAXLEN];
	int physid_len;
	uint64_t physid2;
	pthread_mutex_t lock;
	struct request_queue cpq;		/* Queue of requestors */
	int numreq;				/* number of requestors */
	struct cm_priv *cp;
	nkn_provider_type_t ptype;		/* provider from MM */
	time_t	ts;				/* req timestamp */
	off_t	tot_content_len;		/* tot content length */
	uint32_t    encoding_type;		/* encoding type from attribute */
} cache_req_t;

/*
 * CM private structure to support the Get task.  It holds the buffers we have
 * returned to the caller, so that we can release the ref counts after
 * completion.  It also provides the linkage to the MM task we may need
 * to start to fetch a missing buffer.
 */
typedef struct cm_priv {
	int magic;
	nkn_client_data_t in_client_data;	/* client data */
	void	*proto_data;	/* protocol specific data */
	int num_bufs;		/* number of buffers returned */
	int gotlen;		/* total number of bytes returned */
	int flags;		/* request and status flags */
	uint32_t crflags;	/* incoming request flags from cache response */
	int errcode;		/* error encountered */
	out_proto_data_t  out_proto_resp;/* protocol specific ret data */
	int delay_ms;		/* Time to wait before retrying the request */
	int retries;		/* retries count to avoid looping*/
	int max_retries;	/* max_retries provided by provider*/
	uint32_t encoding_type;
	request_context_t   request_ctxt;

	nkn_provider_type_t provider;	/* to support the cache response value */
	nkn_buf_t *attrbuf;			/* attribute buffer returned */
	nkn_buf_t *bufs[MAX_CM_IOVECS];		/* buffers returned */
	nkn_task_id_t mm_task_id;		/* associated task */
	nkn_task_id_t id;			/* associated task */
	cache_req_t *cr;			/* pending cache request */
	namespace_token_t ns_token;		/* associated namespace */
	TAILQ_ENTRY(cm_priv) reqlist;		/* list of shared requests */
	TAILQ_ENTRY(cm_priv) revallist;
	TAILQ_ENTRY(cm_priv) activelist;	/* list of active GET tasks */
	nkn_cod_t tmpcod;	/* temporary cod opened by BM */
	time_t ns_reval_barrier;
} cmpriv_t;
#define CM_PRIV_MAGIC 0xcdefcdef
#define CM_PRIV_REQ_ISSUED	1
#define CM_PRIV_REQ_DONE	2
#define CM_PRIV_OUTPUT		4
#define CM_PRIV_CLEANUP_DONE	8
#define CM_PRIV_TWICE		0x10		/* second request being sent */
#define CM_PRIV_GETATTR		0x20		/* Get attributes */
#define CM_PRIV_REVAL_ISSUED	0x40		/* sent request to revalidate */
#define CM_PRIV_REVAL_DONE	0x80
#define CM_PRIV_CHECK_EXPIRY	0x100		/* expiry check is required */
#define CM_PRIV_CONF_TIMED_WAIT	0x200		/* Task is in conflict
						 * timed wait due
						 * to request from provider
						 * to retry later.
						 */
#define CM_PRIV_CONF_TIMED_WAIT_DONE	0x400	/* Conflict timed wait done */
#define CM_PRIV_FROM_ORIGIN		0x800	/* Object came from origin and
						 * does not need to be checked
						 * for expiry.

						 */
#define CM_PRIV_NO_CACHE		0x1000
#define CM_PRIV_UNCOND_TIMED_WAIT	0x2000  /* multiple retry support */
#define CM_PRIV_UNCOND_TIMED_WAIT_DONE  0x4000  /* multiple retry waitdone */
#define CM_PRIV_SEP_REQUEST		0x8000	/* Do a seperate request to
						 * Origin
						 */
#define CM_PRIV_BM_ONLY			0x10000	/* Request from BM only 
						 * Do not goto Origin
						 */
/* bug merged wrongly Baffin to cayoosh #define value is different*/
#define CM_PRIV_REVAL_ALL		0x20000

/* Response from Provider, not to update AM */
#define CM_PRIV_NOAM_HITS               0x40000

/* sync reval for reval always */
#define CM_PRIV_SYNC_REVALALWAYS	0x80000

/* sync reval for reval always tunnel modified */
#define CM_PRIV_SYNC_REVALTUNNEL	0x100000

/* sync reval for response setcookie */
#define CM_PRIV_SYNC_REVAL		0x200000

/* sync reval for reval always with queue support */
#define CM_PRIV_SYNCQUEUE_REVALALWAYS	0x400000

/* sync reval for reval always with queue support */
#define CM_PRIV_INTERNAL_ASYNC_DONE	0x800000

#define CM_PRIV_DELIVER_EXP		0x1000000
/* state added to track uncoditional hit case */
#define CM_PRIV_UNCOND_RETRY_STATE	0x2000000

// Set of flags that make a request private
#define CM_PRIV_PRIVATE		(CM_PRIV_CONF_TIMED_WAIT_DONE | \
				 CM_PRIV_UNCOND_TIMED_WAIT_DONE | \
				 CM_PRIV_SEP_REQUEST)


// Force REVAL flags
#define NO_FORCE_REVAL			0x1	// no force revalidation
#define	FORCE_REVAL			0x2	// force revalidation without condition check
#define FORCE_REVAL_COND_CHECK		0x4	// force revalidation with expiry condition check
#define FORCE_REVAL_ALWAYS		0x8	// force revalidate always
#define NO_FORCE_REVAL_EXPIRED		0x10	// deliver content and revalidate
/*
 * Interface to request a UOL to br brought into the cache.  This is done
 * in a non blocking fashion by starting a sub task and then waking up the
 * the parent task (pointed to by the private data).
 * Returns 0 if the request is started and non-zero if there is an error.
 */
int cache_request(nkn_uol_t *uol, cmpriv_t *cp);

/* Cancel a previous request */
void cache_request_cancel(cmpriv_t *cp);

/* Report completion of a request.  Done atomically wrt to a cancel */
void cache_request_done(cmpriv_t *cp, cache_req_t *cr);

/* Free cache request incase of conditional retry */
void
cr_free_uncond_retry(cache_req_t *cr, int err, cmpriv_t *cp);
/* 
 * Revalidate the cache for the specified request if required.  This is done
 * in a non-blocking fashion by sending a task to MM.  The completion handler
 * then takes care of updating the attributes if needed.  The caller does not
 * need to take any further action.  If a cmpriv_t pointer is specified,
 * the completion is reported via a call to cache_revalidation_done()
 */
int cache_revalidate(nkn_buf_t *abuf, namespace_token_t ns_token,
		     cmpriv_t *cp, nkn_client_data_t *in_client_data,
		     uint32_t force_revalidate,
		     void *in_proto_data);

/* cache invalidate request sent to all internal providers */
int
provider_cache_invalidate(nkn_uol_t *uol);

/* Report completion of a revalidation request. */
void cache_revalidation_done(cmpriv_t *cp);

/* 
 * cache write attribute method types
 */
typedef enum {
	ATTRIBUTE_SYNC = 1,		
	ATTRIBUTE_UPDATE
}nkn_attr_action_t;
/*
 * Write attributes back to MM providers using an async task. 
 * The data from the buffer must be copied out.
 */
void cache_write_attributes(nkn_obj_t *attrbuf, 
                            nkn_attr_action_t method_type);


void cache_request_init(void);
#endif
