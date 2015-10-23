#ifndef NKN_BUF_H
#define NKN_BUF_H
#include <glib.h>

#include "cache_mgr.h"
#include "nkn_attr.h"
#include "nkn_defs.h"
#include <sys/queue.h>
#include "nkn_page.h"

/* 
 * A buffer provides a generic container for both pages and objects for
 * managing free lists, reference counting etc.
 */

// A buffer ID is a string composed of the COD for attributes and the
// the COD plus offset for data.
#define TIME32_MASK	(0xFFFFFFFF)
#define TIME40_MASK	(0xFFFFFFFFFF)
#define MAXBUFID	32	// max uri plus offset
#define NKN_BUF_DOWNHITS    1

#define GRANULAR_TIMESTAMP  (100)
#define DISCARD_STATIC_COST (1 * GRANULAR_TIMESTAMP)
#define MAX_DYN_LRU_SCALE   (80 * GRANULAR_TIMESTAMP)
#define MIN_DYN_LRU_SCALE   (1 * GRANULAR_TIMESTAMP)

/* Buffer status flags */
#define NKN_BUF_ID		0x1	/* Buffer has an identity, i.e. uol is valid */
#define NKN_BUF_CACHE		0x2	/* Buffer is in the cache */
#define NKN_BUF_ATTR_FLIST	0x4	/* Buffer is in attr free list */
#define NKN_BUF_GROUPLIST	0x8	/* Buffer is in group-read list */
#define NKN_BUF_FREE_FLIST	0x10	/* Buffer is in free list */

typedef struct nkn_buf {
	pthread_mutex_t lock;
	nkn_uol_t id;		/* identity */
	TAILQ_ENTRY(nkn_buf) entries;
	TAILQ_ENTRY(nkn_buf) attrentries;
//	nkn_pageid_t pageid;	/* underlying page */ -- not needed as we are not freeing to pool
	void *buffer;		/* underlying buffer */
        uint64_t use_ts     : 40;
        uint64_t flags      :  8;          /* status flags */
        uint64_t type       :  4; /* type of buffer */
        uint64_t ftype	    :  4; /* type of buffer */
        uint64_t flist      :  3;          /* free list the buffer is on */
        uint64_t flist0     :  5;          /* free list the buffer is on */
        uint64_t discard_ts : 40;
        uint64_t bufsize    : 16;
        uint64_t discard_cnt:  8;
	int64_t endhit;
        nkn_provider_type_t provider;   /* provider of the data */
        int ref;                /* reference count */
        int use;                /* use count - number of hits */
        uint32_t create_ts;     /* timestamp at which setid is done*/
} nkn_buf_t;

/*
 * An object encapsulates a resource in the cache (named by URI).
 * Each object has attributes and a set of buffers (see below).
 * While pages can be individually inserted and evicted, the object
 * controls object wide actions like expiry and purge.
 */

/* Object status flags */
#define NKN_OBJ_VALID		1	/* Attributes are valid */
#define NKN_OBJ_MODIFIED 	2	/* Object has been modified by origin */
#define NKN_OBJ_DIRTY		4	/* needs to be written to tiers if buffer is dirty*/

struct nkn_page;	/* forward decl */

struct cm_priv;                // forward declaration - see below
TAILQ_HEAD(nkn_revalq, cm_priv);

typedef struct {
	nkn_buf_t buf;		/* the encapsulating buffer container */
	struct nkn_revalq revalq;	/* Queue of revalidation requestors */
				/* MUST BE FIRST TO MAKE ADDR SAME AS BUFFER */
	struct nkn_page *pglist;	/* buffers for this object. */
	struct nkn_page *page0;
	uint64_t bytes_cached;	/* Number of cached bytes in RAM */
	uint64_t orig_hotval;	/* hotval from provider */
	uint32_t reval_ts;	/* time at which to do revalidation */
	uint32_t am_ts;           /* time at which am hits were reported */
	int pagecnt;		/* number of pages for this object */
	uint16_t am_hits;            /* pending hits to report */
	uint8_t flags;
	uint8_t validate_pending;	/* validate is pending */
} nkn_obj_t;
#define NKN_BUF_LIST_OFFSET (&((nkn_obj_t *)0)->buflist)
#define NKN_BUF_LIST_TO_OBJP(blp) ((nkn_obj_t *)((off_t)blp - NKN_BUF_LIST_OFFSET))

#define AM_MAX_HITS (64000)

/*
 * A page encapsulates a contiguous and aligned chunk of an object's 
 * content in memory.
 */
typedef struct nkn_page {
	nkn_buf_t buf;		/* the encapsulating buffer container */
				/* MUST BE FIRST TO MAKE ADDR SAME AS BUFFER */
	char hashid[MAXBUFID];	/* buffer hash identity */
	nkn_obj_t *obj;		/* object association */
	struct nkn_page *nextpage;	/* next page for same object */
	struct nkn_page *prevpage;	/* prev page for same object */
} nkn_page_t;

/*
 * We support variable page sizes (of preconfigured values) between a min
 * and max.
 */
#define NKN_BUF_MIN_PAGE_SIZE   CM_MEM_MIN_PAGE_SIZE	
#define NKN_BUF_MAX_PAGE_SIZE	CM_MEM_PAGE_SIZE;

/* macro defined for attach/detach */
#define NKN_BUF_NODETACH_FROM_COD   0
#define NKN_BUF_DETACH_FROM_COD	    1


/* macro defined for attribute limits */
#define NKN_BUF_ATTRLIMIT_NORMAL    0
#define NKN_BUF_ATTRLIMIT_CUR	    1
#define NKN_BUF_ATTRLIMIT_OPP	    2

/* Buffer operations */

/* Initialize the buffer system */
void nkn_buf_init(void);

/* 
 * Get:  return a buffer if one exists, null otherwise.  The returned
 * buffer is reference counted and must be returned via a put call.
 */
#define NKN_BUF_IGNORE_EXPIRY		1	// DO not check for expiry
#define NKN_BUF_IGNORE_STATS		2	// Do not update hit/miss stats
#define NKN_BUF_IGNORE_COUNT_STATS	4	// Do not update count stats
nkn_buf_t *nkn_buf_get(nkn_uol_t *uol, nkn_buffer_type_t type, int flags);

nkn_buf_t *nkn_buf_getdata(nkn_uol_t *uol, int flags, int *use,
			   nkn_uol_t *actuol, off_t *endhit);

/*
 * Check for the existence of the buffer.  The check is not precise - it may
 * return a false negative for sake of efficiency.
 * Returns 1 if the buffer exists and 0 otherwise.
 */
int nkn_buf_check(nkn_uol_t *uol, nkn_buffer_type_t type, int flags);

/* 
 * Dup:  Get an additional reference for the specific buffer.
 */
void nkn_buf_dup(nkn_buf_t *bp, int flags, int *use,
		 nkn_uol_t *uol, nkn_uol_t *actuol, off_t *endhit);

/*
 * Put:  return a buffer previously acquired via get.
 */
void nkn_buf_put(nkn_buf_t *bp);

/*
 * Alloc:  Allocate an anonymous buffer.  Returns a buffer or NULL if no
 * buffers are available.  The returned buffer is reference counted and
 * must be returned via a put.  The buffer can be assigned an ID 
 * with the set call.
 * An optional size (non-zero) can be specified.
 */
nkn_buf_t *nkn_buf_alloc(nkn_buffer_type_t, int size, size_t flags);

/*
 * Set/Get:  Set the identity for an anonymous buffer.  Returns 0 on success
 * and non-zero on error (e.g. buffer is not anonymous or id is not valid)
 * The flags are defined as per cm_set... 
 */
int nkn_buf_setid(nkn_buf_t *bp, nkn_uol_t *uol, nkn_provider_type_t provider, u_int32_t flags);
void nkn_buf_getid(nkn_buf_t *bp, nkn_uol_t *uol);

int nkn_buf_incache(nkn_buf_t *bp);

/*
 * Get / Set the provider for the specified buffer.  The interfaces
 * work only for a buffer that already has an identity.  The Set is intended
 * for supporting a change in provider (e.g. cache promote/demote).
 */
nkn_provider_type_t nkn_buf_get_provider(nkn_buf_t *);
void nkn_buf_set_provider(nkn_buf_t *, nkn_provider_type_t);

// Set the modified flag for this object.  Prevents further pre-expiry
// revalidation requests
void nkn_buf_set_modified(nkn_buf_t *bp);

/*
 * Get Content:  Get the content pointer.  Caller must have a ref count
 * on the buffer via the get call.
 */
void *nkn_buf_get_content(nkn_buf_t *bp);

/*
 * Is Cached:  Check if the buffer is cached.  Caller must have a ref count
 * on the buffer.  Returns 1 if cached, 0 otherwise.
 */
int nkn_buf_is_cached(nkn_buf_t *bp);

/*
 * Has expired:  Check if the buffer has expired.
 */
int nkn_buf_has_expired(nkn_buf_t *bp);

/*
 * Check if the specified buffer has expired and purge the corresponding
 * object from the cache.
 */
void nkn_buf_expire(nkn_buf_t *bp);

/*
 * Purge:  purge data and attributes associated with UOL
 */
void nkn_buf_purge(nkn_uol_t *uol);

/*
 * Get the attr of the object associated with this buffer.
 * Returns the size or -1 if not available.
 */
nkn_attr_t *nkn_buf_get_attr(nkn_buf_t *bp);

/*
 * Report bytes read for updating stats
 */
void nkn_buf_update_hitbytes(uint64_t length, int ftype);

/* Get/Set page size in the buffer */
int nkn_buf_get_bufsize(nkn_buf_t *bp);
int
nkn_buf_lock_set_bufsize(nkn_buf_t *bp, int rsize, int pooltype);
int nkn_buf_set_bufsize(nkn_buf_t *bp, int rsize, int pooltype);
int nkn_buf_set_attrbufsize(nkn_buf_t *bp, int entries, int tot_blob_bytes);

/* Get stats for specified buffer */
void nkn_buf_getstats(nkn_buf_t *, nkn_buffer_stat_t *);

/* set dirty flag for object */
void nkn_obj_set_dirty(nkn_obj_t *bp);

/* unset dirty flag fro object */
void nkn_obj_unset_dirty(nkn_obj_t *bp);

void nkn_buf_put_downhits(nkn_buf_t *bp, int flags, off_t endhit);
void nkn_buf_put_discardid(nkn_buf_t *bp, int i);

/* get total transit pages [ BM clients refcnted pages ]*/
uint64_t nkn_buf_get_totaltransit_pages(void);
#endif
