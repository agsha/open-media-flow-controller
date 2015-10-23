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
#ifndef NKN_LOCMGR2_CONTAINER_H
#define NKN_LOCMGR2_CONTAINER_H

#include <stdint.h>
#include "nkn_diskmgr2_local.h"

int dm2_conthead_remove_cache_info(dm2_cache_info_t *ci, dm2_cache_type_t *ct);
int dm2_conthead_tbl_insert(const char *key, const void *value,
			    dm2_cache_type_t *ct);
dm2_container_head_t *dm2_conthead_get_by_uri_dir(const char *in_dir,
						  dm2_cache_type_t *ct);
dm2_container_head_t *dm2_conthead_get_by_uri_dir_rlocked(const char *in_dir,
							  dm2_cache_type_t *ct,
							  int *ch_locked,
							  int *err_ret,
							  int is_blocking);
dm2_container_head_t *dm2_conthead_get_by_uri_dir_wlocked(const char *in_dir,
							  dm2_cache_type_t *ct,
							  int *ch_locked);
int dm2_conthead_tbl_init(dm2_cache_type_t *ct);

#endif /* NKN_LOCMGR2_CONTAINER_H */
