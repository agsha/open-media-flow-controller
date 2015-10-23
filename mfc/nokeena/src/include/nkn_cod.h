#ifndef NKN_COD_H
#define NKN_COD_H

#include <sys/types.h>
#include <sys/syscall.h>
#include <stdint.h>

#include "nkn_defs.h"

/* API for Cache Object Descriptors */

/*
 * A COD represents a specific version of a cache object that is currently
 * in uss within nvsd.  The COD provides the following functionality.
 *	- Object version support.  Multiple versions of an object can be
 *	  in play in nvsd, especially for large objects, across the expiry
 *	  boundary.
 *      - Compact representation of a URI (saves each module from having
 *	  to store/compare a large string name.
 *
 * The usage model is for a module to get a COD when it wants to access
 * a cached object.  This is a reference counted object and must be closed
 * to release the underlying resource.
 */

typedef enum {
	NKN_COD_STATUS_NULL,
	NKN_COD_STATUS_OK,
	NKN_COD_STATUS_EXPIRED,
	NKN_COD_STATUS_INVALID,
	NKN_COD_STATUS_STALE,
	NKN_COD_STATUS_STREAMING
} nkn_cod_status_t;

/*
 * Cod Owners
 */

typedef enum {
    NKN_COD_FILE_MGR,
    NKN_COD_BUF_MGR,
    NKN_COD_MEDIA_MGR,
    NKN_COD_TFM_MGR,
    NKN_COD_DISK_MGR,
    NKN_COD_NW_MGR,
    NKN_COD_VFS_MGR,
    NKN_COD_GET_MGR,
    NKN_COD_ORIGIN_MGR,
    NKN_COD_NS_MGR,
    NKN_COD_COD_MGR,
    NKN_COD_AM_MGR,
    NKN_COD_CIM_MGR,
    NKN_COD_MAX_MGR
}nkn_cod_owner_t;

/*
 * Initialize the COD module
 */
void nkn_cod_init(int entries);

/*
 * Get a COD (reference counted) for the specified cache object.  Returns
 * NKN_COD_NULL if there are no more entries available.
 * A COD that is marked streaming becomes non-shareable until it is marked
 * complete.  Therefore, a COD open on the object will fail and return NULL.
 */
nkn_cod_t nkn_cod_open(const char *cn, size_t length, nkn_cod_owner_t cown);

/*
 * Get a duplicate (reference counted) COD.
 */
nkn_cod_t nkn_cod_dup(nkn_cod_t, nkn_cod_owner_t cown);

/* Close a COD previously opened */
void nkn_cod_close(nkn_cod_t, nkn_cod_owner_t cown);

/*
 * Get pointer to the name of the object.  Optionally, the length of the name
 * can also be retrieved (if lenp is non-null).  Returns non-zero error code
 * if the COD is not valid.  The returned pointer is only valid while the COD
 * is ref counted.
 */
int nkn_cod_get_cn(nkn_cod_t cod, char **cnp, int *lenp);

/*
 * Get pointer to the name of the object.
 * Returns the null string if the COD is not valid.
 */
const char *nkn_cod_get_cnp(nkn_cod_t cod);

/*
 * Atomically test and set the version ID for the COD.  Returns zero if the
 * COD has an existing version that matches the specified one or if the COD
 * is updated with the specified version.  Errors returned are:
 * NKN_COD_VERSION_MISMATCH: existing version is different from the
 *	specified one.
 * NKN_COD_INVALID_COD:  the COD is no longer valid.
 */
#define NKN_COD_SET_ORIGIN	1	// caller is origin provider
#define NKN_COD_SET_STREAMING	2	// object is being streamed
int nkn_cod_test_and_set_version(nkn_cod_t, nkn_objv_t, int flags);

/*
 * Mark a COD that was previously set as streaming to be now complete
 * Returns 0 on success or one of the following error:
 * NKN_COD_INVALID_COD:  the COD is no longer valid or not marked streaming.
 */
int nkn_cod_set_complete(nkn_cod_t);

/*
 * Set the new version ID for the COD.  This is used by an Origin provider
 * to establish the new version for an expired COD.  This ensures that
 * subsequent request to the object will go to the OM until the new
 * version is locally cached.
 */
void nkn_cod_set_new_version(nkn_cod_t cod, nkn_objv_t ver);

/*
 * Get the version ID for the COD.  Returns a null version if not set and
 * invalid if the COD is not valid.
 */
int nkn_cod_get_version(nkn_cod_t, nkn_objv_t *);

/*
 * Returns COD status - valid/expired/invalid/stale/streaming
 */
nkn_cod_status_t nkn_cod_get_status(nkn_cod_t);

/*
 * Mark COD as expired.
 */
void nkn_cod_set_expired(nkn_cod_t);

/*
 * Get the next cached entry that is held in the RAM cache.  This routine is
 * intended to get a sequential walk of COD entries.  The first invocation
 * can be done with cod set to NKN_COD_NULL.  After that, subsequent calls
 * should use the next_cod returned by the previous call.  The routine
 * gets a reference count for the entry returned.  It must be released by the
 * caller after use using nkn_cod_close().
 * If a non-null prefix string is specified, only entries
 * matching it will be returned.  This can be used to limit the entries to
 * a Namespace or a specific container.  The length of the prefix string
 * (minux the null) should be specified via plen.
 * Returns 0 on success.  After all entries are listed, next is set to
 * NKN_COD_NULL. On failure, one of the following error codes:
 *	EINVAL - Invalid COD entry.
 */
int nkn_cod_get_next_entry(nkn_cod_t cod, char *prefix, int plen,
			   nkn_cod_t *next, char **uri, int *urilen,
			   nkn_cod_owner_t cown);

int nkn_cod_ingest_attach(nkn_cod_t cod, void *ingest_ptr, off_t ingest_offset);

void nkn_cod_update_client_offset(nkn_cod_t cod, off_t client_offset);

off_t nkn_cod_get_client_offset(nkn_cod_t cod);

/* Get number of active clients other than MM */
int nkn_cod_push_ingest_get_clients(nkn_cod_t cod);

/* Push ingest flag set/clear/check functions */
void nkn_cod_clear_push_in_progress(nkn_cod_t cod);

void nkn_cod_set_push_in_progress(nkn_cod_t cod);

int nkn_cod_is_push_in_progress(nkn_cod_t cod);

void nkn_cod_clear_ingest_in_progress(nkn_cod_t cod);

void nkn_cod_set_ingest_in_progress(nkn_cod_t cod);

int nkn_cod_is_ingest_in_progress(nkn_cod_t cod);

void *
nkn_cod_update_hit_offset(nkn_cod_t cod, off_t client_offset,
			  void *objp, uint32_t *am_hits,
			  uint32_t *update, const char **uri);
void nkn_buf_hit_ts(void *objp, uint32_t *am_hits, uint32_t *update);

nkn_buffer_t *nkn_cod_attr_lookup(nkn_cod_t cod);
#endif	/* NKN_COD_H */
