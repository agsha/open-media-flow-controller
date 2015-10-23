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
#ifndef NKN_LOCMGR2_PHYSID_H
#define NKN_LOCMGR2_PHYSID_H

#include <stdint.h>
#include <glib.h>
#include "nkn_diskmgr2_local.h"

typedef struct DM2_physid_head {
    pthread_rwlock_t ph_rwlock;
    GList	     *ph_ext_list;
    uint64_t	     ph_physid;
} DM2_physid_head_t;

#if 0
void nkn_locmgr2_physid_tbl_print(void);
void nkn_locmgr2_physid_tbl_init(void);
int nkn_locmgr2_physid_tbl_insert(const uint64_t *key, const void *value);
void *nkn_locmgr2_physid_tbl_get(const uint64_t *key);
void nkn_locmgr2_physid_tbl_cleanup(void);
#endif

DM2_physid_head_t *dm2_extent_get_by_physid(const uint64_t *in_physid,
					    dm2_cache_type_t *ct);
DM2_physid_head_t *dm2_extent_get_by_physid_rlocked(const uint64_t *in_physid,
						    dm2_cache_type_t *ct);
DM2_physid_head_t *dm2_extent_get_by_physid_wlocked(const uint64_t *in_physid,
						    dm2_cache_type_t *ct);
int dm2_extent_insert_by_physid(const uint64_t *in_physid,
				DM2_extent_t *in_extent,
				dm2_cache_type_t *ct);

int dm2_physid_tbl_ext_remove(const uint64_t *physid_key,
			      const DM2_extent_t *ext_data,
			      dm2_cache_type_t *ct,
			      int *ph_empty);
DM2_physid_head_t *dm2_physid_tbl_get(const uint64_t *key,
				      dm2_cache_type_t *ct);
int dm2_physid_tbl_ext_insert(const uint64_t *key, const void *value,
			      dm2_cache_type_t *ct);
int dm2_physid_tbl_init(dm2_cache_type_t *ct);
int dm2_uri_physid_cnt(GList *uri_ext_list);
#endif /* NKN_LOCMGR2_PHYSID_H */
