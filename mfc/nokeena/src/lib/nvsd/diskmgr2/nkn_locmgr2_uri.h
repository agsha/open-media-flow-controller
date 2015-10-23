/*
 *	COPYRIGHT: NOKEENA NETWORKS
 *
 * This file contains code which implements the Disk Manager
 *
 * Author: Michael Nishimoto
 *
 * Non-obvious coding conventions:
 *	- All functions should begin with dm2_
 *	- All functions have a name comment at the end of the function.
 *	- All log messages use special logging macros
 *
 */
#ifndef NKN_LOCMGR2_URI_H
#define NKN_LOCMGR2_URI_H

#include <stdint.h>
#include "nkn_diskmgr2_local.h"
#include "nkn_diskmgr2_disk_api.h"

/*
 * - The ci lock is grabbed before the dm2_uri* function returns the uri_head
 *   making things atomic. This would avoid the race where the PUT obtains a
 *   uri_head and before the code locks the ci, the cache disable takes out
 *   the ci.
 *
 * - There are some places where in the uri_head is still not available and in
 *   those places the ci device is locked directly.
 *
 * - At places where we read the containers/attributes, since the top level
 *   function locks the device, the dm2_uri* functions do not lock the device
 *   (this is where the NOLOCK_CI) is used.
 *
 * - If dm2_uri* functions are invoked with RLOCK_CI, they use UNRLOCK_CI while
 *   unlocking the uri_head. If these functions do not want to lock the ci,
 *   NOLOCK_CI is used (both while locking & unlocking).
 */


int dm2_uri_head_insert_by_uri(const struct DM2_uri *in_uri_head,
			       dm2_cache_type_t *ct);
DM2_uri_t *dm2_uri_head_get_by_uri(const char *in_uri, dm2_cache_type_t *ct);
DM2_uri_t *dm2_uri_head_get_by_ci_uri_rlocked(const char *in_uri,
					      dm2_container_head_t *ch_in,
					      int *ch_locked_in,
					      dm2_container_t  *cont,
					      dm2_cache_info_t *ci,
					      dm2_cache_type_t *ct);
DM2_uri_t *dm2_uri_head_get_by_ci_uri_wlocked(const char *in_uri,
					      dm2_container_head_t *ch_in,
					      int *ch_locked_in,
					      dm2_container_t  *cont,
					      dm2_cache_info_t *ci,
					      dm2_cache_type_t *ct,
					      int is_delete);
DM2_uri_t *dm2_uri_head_get_by_uri_rlocked(const char *in_uri,
					   dm2_container_head_t *ch,
					   int *ch_lock,
					   dm2_cache_type_t *ct,
					   int is_blocking,
					   int lock_ci, int *err_ret);
DM2_uri_t *dm2_uri_head_get_by_uri_rlocked_for_put(const char *in_uri,
						   dm2_cache_type_t *ct);
DM2_uri_t *dm2_uri_head_get_by_uri_wlocked(const char *in_uri,
					   dm2_container_head_t *ch,
					   int *ch_lock,
					   dm2_cache_type_t *ct,
					   int is_delete, int lock_ci);
DM2_uri_t *dm2_uri_head_get_ci_by_uri_wlocked(const char    *uri_key,
					      dm2_cache_type_t *ct,
					      dm2_container_t  *cont,
					      dm2_cache_info_t *ci,
					      int is_delete);
DM2_uri_t *dm2_uri_head_get_by_uri_wlocked_for_delete(const char *in_uri,
						      dm2_cache_type_t *ct);
int dm2_uri_head_lookup_by_uri(const char *in_uri,
			       dm2_container_head_t *ch, int *ch_lock,
			       dm2_cache_type_t *ct, int assert);
int dm2_uri_head_raw_lookup_by_uri(const char *in_uri, dm2_cache_type_t* ct,
				   dm2_cache_info_t *ci, int assert);
int dm2_uri_head_match_by_uri_head(const DM2_uri_t *uri_head_key,
				   dm2_cache_type_t *ct);
void dm2_uri_head_free(DM2_uri_t *uri_head, dm2_cache_type_t *ct, int lock);
void dm2_uri_head_rwlock_runlock(DM2_uri_t *uri_head, dm2_cache_type_t *ct,
			         int flag);
void dm2_uri_head_rwlock_wunlock(DM2_uri_t *uri_head, dm2_cache_type_t *ct,
				 int flag);
void dm2_uri_head_rwlock_unlock(DM2_uri_t *uri_head, dm2_cache_type_t *ct,
			         int flag);
void dm2_uri_head_rwlock_wlock(DM2_uri_t *uri_head, dm2_cache_type_t *ct,
			       int is_delete);
int dm2_uri_head_shutdown_steal(DM2_uri_t *uh_key, dm2_cache_info_t *ci,
				dm2_cache_type_t *ct);
int dm2_uri_head_verify_empty(dm2_cache_info_t *ci, dm2_cache_type_t *ct);
int dm2_uri_remove_cache_info(dm2_cache_info_t *ci, dm2_cache_type_t *ct);
#if 0
int dm2_uri_tbl_insert(const char *key, const void *value,
		       dm2_cache_type_t *ct);
#endif
int dm2_uri_tbl_init(dm2_cache_info_t *ci);
void dm2_uri_tbl_steal_and_wakeup(DM2_uri_t *uri_head, dm2_cache_type_t *ct);
void dm2_uri_lock_drop(DM2_uri_t *uri_head);
#endif /* NKN_LOCMGR2_URI_H */
