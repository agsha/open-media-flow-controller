#ifndef CACHE_MANAGER_PUBLIC_H
#define CACHE_MANAGER_PUBLIC_H

/* 
 * External API for the Buffer manager (aka Cache manager) module.
 *
 * There are two main parts of this API:
 * 1.  Operations on the buffer object - set identity, hold/release (for
 *     reference counting), get the data pointer, etc.
 * 2.  The cache_response structure used for the GET task used to get
 *     a set of buffers for a given object segment (uri, offset, length).
 */

#include <sys/types.h>
#include "nkn_defs.h"
#include "nkn_attr.h"
#include "nkn_sched_api.h"
#include "nkn_namespace.h"
#include <stdio.h>

#define CACHE_RESP_REQ_MAGIC   0xbabababa
#define CACHE_RESP_RESP_MAGIC  0xdeadbeaf

/* 
 * The page size number of bytes associated with each buffer.  This is the
 * basic unit of managing cache/buffer memory in nvsd.
 */
#define NKN_INIT_ATTR_SIZE (512)
#define CM_MEM_MIN_PAGE_SIZE (2 * NKN_INIT_ATTR_SIZE)
#define CM_MEM_MIN_NONSMALLATTR_PAGE_SIZE (8 * NKN_INIT_ATTR_SIZE)
#define CM_MEM_PAGE_SIZE (32* CM_MEM_MIN_PAGE_SIZE)
#define CM_MEM_PAGE_MASK (CM_MEM_PAGE_SIZE - 1)

/* 
 * Maximum number of buffers that can be returned via the cache_response
 * structure below.
 */
#define MAX_CM_IOVECS 64

/* In/out params required for the GET task */
#define CR_RFLAG_GETATTR	1
#define CR_RFLAG_NO_CACHE	2
#define CR_RFLAG_FIRST_REQUEST	4
#define CR_RFLAG_TRACE		0x10
#define CR_RFLAG_INTERNAL	0x20	// Internal request (do not count as hit)
#define CR_RFLAG_NO_DATA	0x40	// No data is requested (only attributes)
#define CR_RFLAG_NO_REVAL	0x80	// Do no revalidate expired data
#define CR_RFLAG_REVAL		0x100	// Force revalidate
#define CR_RFLAG_SEP_REQUEST	0x200   // Force a separate cache request 
					// instead of sharing with existing
					// requests.
#define CR_RFLAG_BM_ONLY	0x400	// Request from BM only. Dont go to origin
#define CR_RFLAG_CACHE_ONLY	0x800	// Request only from local
					// cache. Dont go to origin
#define CR_RFLAG_NO_AMHITS	0x1000	// Force revalidate
#define CR_RFLAG_FUSE_HITS	0x2000  // FUSE injestion only to SATA provider
#define CR_RFLAG_RET_REVAL	0x4000  // This object has been revalidated. Return flag
#define CR_RFLAG_EOD_CLOSE	0x8000  // EOD flag to check invalid length 
#define CR_RFLAG_SYNC_REVALWAYS 0x10000 // sync revalidation always needs to be used
#define CR_RFLAG_NFS_BYPASS     0x20000 // NFS bypass
#define CR_RFLAG_CLIENT_INTERNAL     0x40000 // Requested by an internal client through 127.0.0.x
#define CR_RFLAG_IGNORE_COUNTERSTAT 0x80000 // Ignore stats  
#define CR_RFLAG_ING_ONLYIN_RAM	0x100000 // cluster L7 support -- ingest only in ram flag 
#define CR_RFLAG_POLICY_REVAL	0x200000 // Policy Revalidation
#define CR_RFLAG_PREFETCH_REQ   0x400000 // Pre-fetch request
#define CR_RFLAG_CRAWL_REQ      0x800000 // Crawl request
#define CR_RFLAG_MFC_PROBE_REQ 	0x1000000 
#define CR_RFLAG_DELIVER_EXP	0x2000000


typedef struct cache_response {
    unsigned int     magic;
    int     length_response;  // size of response
    nkn_uol_t   uol;
    nkn_client_data_t	in_client_data;	/* requesting protocol */
    int         req_max_age;	/* BUG 5889: pass over max-age value */
    int		in_rflags;	// request flags - see above for defintions)
    void 	*in_proto_data; /* requesting protocol data (specific to the
				 * the protocol type).
				 */
    out_proto_data_t out_proto_resp; /* protocol specific ret data */
    namespace_token_t ns_token; /* namespace for request */

    uint64_t nscfg_contlen;	// content length config from 

    nkn_attr_t *attr;		// attributes for the object, if requested
				// via flags (see above)
    nkn_iovec_t content[64]; // actual content
    int     num_nkn_iovecs;
    int     errcode;  // Error code
    nkn_provider_type_t provider;	/* content provider */
    // other fields may be added for convenience of cache manager
} cache_response_t;

/*
 * Each iovec element in the content array returned in the cache_response
 * struct above has an underlying buffer that is reference counted.
 * Typically, the all buffers are release by BM upon completion of the task.
 * In order to reduce buffer occupancy time, especially for slow connections,
 * the following API is provided to release the iovecs incrementally as they
 * are (e.g. sent over the network).  This API must be called atmost once
 * for each vector and no access to the content should be made to the content
 * after this call.
 */
void nkn_cr_content_done(nkn_task_id_t id, int iov_num);



/*
 * Buffer types
 */
typedef enum {
	NKN_BUFFER_DATA,	/* data buffer */
	NKN_BUFFER_ATTR		/* attribute buffer */
} nkn_buffer_type_t;

/*
 * Buffer statistics
 */
typedef struct {
	int hits;		// number of accesses recorded
	uint64_t bytes_cached;	// number of cached bytes.  For a data
			 	// buffer, valid bytes within the page.
				// For an attribute buffer, bytes
				// cached for the entire object.
} nkn_buffer_stat_t;

/* 
 * Set the identity (uri, offset length) for the buffer.
 * Constraints:
 *    - the buffer must not have an any existing identity.
 *    - the offset must be page aligned (i.e. multiple of CM_MEM_PAGE_SIZE)
 *      for the page to be cacheable.  The exception to this rule is for
 *      partial pages.  For example, if there is a byte range request to the
 *      origin, the first page coming back may not be page aligned.  That
 * 	partial page can have an offset that is not page aligned and a length
 *	upto the next page boundary.  Buffer manager will attempt to 
 * 	find an exisint page with the remaining content and merge the content.
 *  	The partial page will be marked non-cacheable.  The subsequent pages 
 *	should be page aligned to enable caching.
 *    - the length must be greater than 0 and less than CM_MEM_PAGE_SIZE)
 *    - offset and length are not interpreted for attribute buffers
 * Flags (see below):
 */
#define NKN_BUFFER_SETID_NOCACHE 0x1	/* Do not cache - i.e. discard when 
					   ref count goes to zero */
#define NKN_BUFFER_UPDATE	 0x2	/* Update the contents of an existing
					 * cached buffer (if any).  Only
					 * supported for attributes. */
#define NKN_BUFFER_GROUPREAD     0x4	/* group read flag */
void nkn_buffer_setid(nkn_buffer_t *,  nkn_uol_t *, 
		      nkn_provider_type_t provider, u_int32_t flags);

/* Check if the buffer is in cache or not */
/*
 * Caution: This does not check whether the buffer is valid or not
 * It assumes that the buffer is valid and is ref counted
 */
int nkn_buffer_incache(nkn_buffer_t *bp);

/* Get the identity associated with the buffer. */
void nkn_buffer_getid(nkn_buffer_t *, nkn_uol_t *);

/*
 * Get / Set the provider for the specified buffer.  The interfaces
 * work only for a buffer that already has an identity.  The Set is intended
 * for supporting a change in provider (e.g. cache promote/demote).
 */
nkn_provider_type_t nkn_buffer_get_provider(nkn_buffer_t *);
void nkn_buffer_set_provider(nkn_buffer_t *, nkn_provider_type_t);

/* Get the memory pointer for the content for this buffer */
void* nkn_buffer_getcontent(nkn_buffer_t *);

/*
 * function call to write the modified attribute 
 */
void nkn_buffer_setmodified(nkn_buffer_t *bp);

/*
 * function call to write the modified attribute using SYNC
 */
void nkn_buffer_setmodified_sync(nkn_buffer_t *bp);

/* 
 * Lookup a buffer by uol.  Returns a reference counted buffer or null.
 */
nkn_buffer_t *nkn_buffer_get(nkn_uol_t *uol, nkn_buffer_type_t type);

/* 
 * Get an additional reference for this buffer.  This should be used if a 
 * buffer ptr is copied from one task to another and the tasks can be
 * independently completed.  The caller is responsible for releasing the
 * reference via the release call (see below).
 */
void nkn_buffer_hold(nkn_buffer_t *);
void nkn_buffer_release(nkn_buffer_t *);

// cacheable pool/non-cacheable pool
#define CUR_NUM_CACHE		2

// flags option
#define BM_CACHE_ALLOC	    0x0
#define BM_NEGCACHE_ALLOC   0x1
#define BM_MAXCACHE_ALLOC   0xFF
#define BM_PREREAD_ALLOC    0x100
/*
 * Allocate an anonymous (no identity) buffer.  The buffer is returned
 * with a refernce count.  The caller is responsible for setting the identity
 * for the buffer if content is filled in.  The caller must also release the
 * buffer.  Returns NULL is no buffer is available.
 * An optional (non-zero) size can be specified.  Otherwise, the default
 * size associated with the type will be allocated.
 */
nkn_buffer_t *nkn_buffer_alloc(nkn_buffer_type_t, int size, size_t flags);

/*
 * Purge any data and attributes associated with the specifed UOL.
 * The offset and length can be used to specify a range to be purged or set
 * to zero to indicate the whole object.
 */
void nkn_buffer_purge(nkn_uol_t *);

/*
 * Set (change) the provider for an object.  This is intended for use by
 * cache promote/demote.
 * NOTE:  offset and length are currently ignored.  All existing buffers
 * for the object are changed.
 */
void nkn_uol_setprovider(nkn_uol_t *, nkn_provider_type_t provider);
/*
 * Set (change) the provider for an object.  This is intended for use by
 * cache promote/demote.
 */
void nkn_uol_setattribute_provider(nkn_uol_t *, nkn_provider_type_t provider);
/*
 * Get the object provider.
 */
nkn_provider_type_t nkn_uol_getattribute_provider(nkn_uol_t *uol);

/*
 * Get the size of the memory page associated with this buffer
 */
int nkn_buffer_get_bufsize(nkn_buffer_t *);

/*
 * Change the size of the memory page associated with this buffer.  Returns
 * zero on success or one of the following failures.  Note that a change
 * can only be made on an anonymous buffer (i.e. prior to setid being done).
 *   NKN_BUF_INVALID_INPUT - illegal size, buffer already has identity.
 *   NKN_SERVER_BUSY - allocation of new memory page failed.
 */
int nkn_buffer_set_bufsize(nkn_buffer_t *, int, int);

/*
 * Specialized version of nkn_buffer_set_bufsize() for attribute buffers 
 * which is nkn_attr_t aware.
 */
int nkn_buffer_set_attrbufsize(nkn_buffer_t *, int, int);

/* Get stats for specified buffer */
void nkn_buffer_getstats(nkn_buffer_t *, nkn_buffer_stat_t *);

/* Check if the attribute buffer is for a streaming object */
int nkn_buf_is_streaming(nkn_buffer_t *bp);

/* called to update stats */
void nkn_update_combined_stats(void);

/* update debug information for the chunk_mgr -> MM tasks */
void cache_update_debug_data(struct nkn_task *ntask);

/* update the cache request list to a file - called from cli*/
void nkn_bm_req_entries(FILE *fp);

#endif

