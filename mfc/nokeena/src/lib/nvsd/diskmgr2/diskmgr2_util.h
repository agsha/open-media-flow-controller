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
#ifndef DISKMGR2_UTIL_H
#define DISKMGR2_UTIL_H

#include "nkn_mediamgr_api.h"
#include "nkn_diskmgr2_disk_api.h"
#include "nkn_diskmgr2_local.h"

void dm2_cache_log_add(MM_put_data_t *put, char* put_uri, DM2_uri_t *uri_head,
		       dm2_cache_type_t *ct);
void dm2_cache_log_delete(const char *uri_name, const char *uri_dir,
			  const char *mgmt_name, int uri_orig_hotness,
			  nkn_hot_t uri_hotval, uint32_t cache_age,
			  uint32_t del_reason, const dm2_cache_type_t *ct);
void dm2_cache_log_attr_update(DM2_uri_t *uri_head, char *uri,
			       dm2_cache_type_t *ct);
void dm2_cache_log_attr_hotness_update(DM2_uri_t *uri_head, const char* uri,
                                  dm2_cache_type_t *ct, uint64_t  hotval);
void dm2_cache_log_decay(DM2_uri_t *uri_head, dm2_cache_type_t *ct,
                    nkn_hot_t   old_hotval, nkn_hot_t   new_hotval);
void dm2_cache_log_disk_format(const char *mgmt_name, dm2_cache_type_t *ct);
void dm2_dec_write_cnt_on_cache_dev(dm2_cache_info_t *ci);
void dm2_inc_write_cnt_on_cache_dev(dm2_cache_info_t *ci);
int dm2_tier_blocksize(nkn_provider_type_t ptype);
int dm2_tier_pagesize(nkn_provider_type_t ptype);
void dm2_set_lowest_tier(nkn_provider_type_t ptype);
void dm2_update_lowest_tier(void);
nkn_provider_type_t dm2_nscat_to_ptype(ns_stat_category_t category);
ns_stat_category_t dm2_ptype_to_nscat(nkn_provider_type_t ptype);
ns_stat_category_t dm2_ptype_to_ns_maxuse_cat(nkn_provider_type_t ptype);
rp_type_en dm2_nscat_to_rptype(ns_stat_category_t cat);
rp_type_en dm2_ptype_to_rptype(nkn_provider_type_t ptype);

char* dm2_get_time_single_token(time_t t);

#endif /* DISKMGR2_UTIL_H */
