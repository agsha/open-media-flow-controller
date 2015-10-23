/*
 * (C) Copyright 2008-9 Ankeena Networks, Inc
 * (C) Copyright 2010-2012 Juniper Networks, Inc
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
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#ifdef HAVE_FLOCK
#  include <sys/file.h>
#endif
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <sys/user.h>
#include <glib.h>
#include <mntent.h>
#include <sys/mount.h>
#include <strings.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/shm.h>

#include "zvm_api.h"

#include "nkn_defs.h"
#include "nkn_cod.h"
#include "nkn_namespace_stats.h"
#include "nkn_diskmgr2_local.h"
#include "nkn_diskmgr2_disk_api.h"
#include "nkn_hash_map.h"
#include "nkn_locmgr2_extent.h"
#include "nkn_locmgr2_uri.h"
#include "nkn_locmgr2_physid.h"
#include "nkn_locmgr2_container.h"
#include "cache_mgr.h"
#include "nkn_cfg_params.h"
#include "nkn_debug.h"
#include "nkn_util.h"
#include "nkn_stat.h"
#include "nkn_am_api.h"
#include "nkn_assert.h"
#include "nkn_opts.h"		// likely/unlikely macros
#include "nkncnt_client.h"

#include "diskmgr2_locking.h"
#include "diskmgr2_common.h"
#include "diskmgr2_util.h"
#include "diskmgr2_locking.h"
#include "smart_attr.h"
#include "nkn_nknexecd.h"

#include "mdc_wrapper.h"


#define ROUNDDOWN(x, y)  (((x)/(y))*(y))

/* Max count of URI locks - TODO should be calculated
 * based on number of disks and PUT/GET/STAT thread counts */
#define DM2_MAX_URI_LOCKS 1024
#define DM2_SMALL_READ_DIV 8

typedef struct DM2_blk_map {
    nkn_buffer_t *bp;
    void	 *cp;
    DM2_extent_t *ext;
    uint32_t	 mem_page_size;
    uint32_t	 data_len;
    int		 num_pages;
    int		 is_hole;
} DM2_blk_map_t;

AO_t *dm2_signal_disk_change_to_mgmt;
int dm2_show_locks = 0;
int dm2_verify_attr_on_update = 1;
int dm2_perform_put_chksum = 1;
int dm2_perform_get_chksum = 1;
int dm2_assert_for_disk_inconsistency = 1;
int glob_dm2_throttle_writes = 0;
int glob_dm2_small_write_enable;
int glob_dm2_small_write_min_size;

nkncnt_client_ctx_t dm2_smart_ctx;
int dm2_ram_drive_only = 0;
int dm2_print_mem = 0;
int dm2_stop_preread = 0;
static int dm2_block_share_prcnt[NKN_MM_MAX_CACHE_PROVIDERS];
static int dm2_internal_high_watermark[NKN_MM_MAX_CACHE_PROVIDERS];
static int dm2_internal_low_watermark[NKN_MM_MAX_CACHE_PROVIDERS];
static int dm2_internal_time_low_watermark[NKN_MM_MAX_CACHE_PROVIDERS] = {-1};
static int dm2_evict_hour[NKN_MM_MAX_CACHE_PROVIDERS] = {-1};
static int dm2_evict_min[NKN_MM_MAX_CACHE_PROVIDERS] = {-1};
static int dm2_internal_num_percent[NKN_MM_MAX_CACHE_PROVIDERS];

int glob_dm2_dict_int_evict_hwm = 100;
int glob_dm2_dict_int_evict_lwm = 99;
int glob_dm2_dict_int_evict_in_progress = 0;

nkn_provider_type_t glob_dm2_lowest_tier_ptype;

extern int g_dm2_unit_test;
int glob_dm2_optimal_packing_prcnt = 25;

extern md_client_context *nvsd_mcc;
/*
 * static global variables
 */
/* XXXmiken: Needs to be protected by lock when adding types dynamically */
dm2_cache_type_t g_cache2_types[DM2_MAX_CACHE_TYPE];
GStaticMutex dm2_cache2_types_mutex = G_STATIC_MUTEX_INIT;

static int dm2_get_pages(dm2_disk_block_t *db, off_t partial_len,
			 off_t *page_ret, uint32_t *numpages_ret, char *str);
static int dm2_return_pages_to_free_list(dm2_disk_block_t *blk_map,
					 dm2_bitmap_t *bm, off_t max_pg,
					 dm2_cache_info_t *ci,
					 int *freed_bitmap_blocks);

static uint32_t dm2_evict_internal_high_water_mark(int ptype);
static uint32_t dm2_evict_internal_low_water_mark(int ptype);
static int32_t dm2_evict_internal_time_low_water_mark(int ptype);
static int32_t	dm2_ct_evict_hour_threshold(int ptype);
static int32_t	dm2_ct_evict_min_threshold(int ptype);
static int dm2_uri_end_puts(char *put_uri, dm2_cache_type_t *ct,
			    ns_stat_token_t ns_stoken);
static uint32_t dm2_ct_internal_block_share_threshold(int ptype);
static uint32_t dm2_evict_internal_disk_percentage(int ptype);
static int dm2_attr_update_read_cont(dm2_cache_type_t *ct, char* uri);
#if 0
static void dm2_cont_head_free_empty_containers(dm2_container_head_t *cont_head,
						dm2_cache_type_t     *ct,
						char		     *uri);
#endif
static void dm2_put_err_uri_cleanup(DM2_uri_t	     *uri_head,
				    int		     *cont_wlocked,
				    const char	     *put_uri,
				    dm2_cache_type_t *ct);

/*
 * Global statistics
 */
int64_t glob_dm2_num_cache_types,
    glob_dm2_file_open_err,
    glob_dm2_file_lseek_err,
    glob_dm2_file_read_err,
    NOZCC_DECL(glob_dm2_stat_cnt),
    glob_dm2_stat_busy_err,
    glob_dm2_stat_err,
    NOZCC_DECL(glob_dm2_stat_warn),
    NOZCC_DECL(glob_dm2_stat_not_found),
    NOZCC_DECL(glob_dm2_stat_sys_cnt),
    glob_dm2_preread_disabled,
    glob_dm2_preread_stat_cnt,
    NOZCC_DECL(glob_dm2_quick_stat_not_found),
    glob_dm2_quick_put_not_found,
    glob_dm2_stat_invalid_input_err,
    glob_dm2_stat_uri_chksum_err,
    glob_dm2_stat_uri_invalid_err,
    glob_dm2_stat_time0_cnt,

    glob_dm2_get_buf_alloc_size_mismatch_cnt,
    NOZCC_DECL(glob_dm2_get_cnt),
    glob_dm2_get_err,
    NOZCC_DECL(glob_dm2_get_success),
    NOZCC_DECL(glob_dm2_get_attr_read_cnt),
    glob_dm2_get_attr_read_err,
    glob_dm2_attr_read_cod_null_err,
    glob_dm2_attr_read_alloc_failed_err,
    glob_dm2_get_invalid_input_err,
    glob_dm2_get_physid_mismatch_cnt,
    glob_dm2_get_not_found_cnt,		// normal not found
    glob_dm2_get_not_found_err,		// error not found
    NOZCC_DECL(glob_dm2_put_cnt),
    NOZCC_DECL(glob_dm2_put_err),
    glob_dm2_put_bad_uri_err,
    glob_dm2_put_bad_offset_err,
    glob_dm2_put_eexist_warn,
    glob_dm2_put_try_again_cnt,
    glob_dm2_put_try_again_err,

    NOZCC_DECL(glob_dm2_api_server_busy_err),// sub errcod NKN_SERVER_BUSY 10003
    glob_dm2_api_server_busy_dict_full_err,

/* Function comments */
    glob_dm2_put_basename_too_long_err,
    glob_dm2_put_ci_disabled_try_again_cnt,
    glob_dm2_put_ci_disabled_loop_err,
    glob_dm2_put_cache_pin_reset_promote_cnt,
    glob_dm2_put_cache_pin_reset_size_cnt,
    glob_dm2_put_cache_pin_reset_pin_disabled_cnt,
    glob_dm2_put_delete_expired_uol_err,
    glob_dm2_put_empty_version_err,
    glob_dm2_put_end_trans_no_obj_warn,
    glob_dm2_put_expired_err,
    glob_dm2_put_expired_cnt,
    glob_dm2_put_expiry_cnt,
    NOZCC_DECL(glob_dm2_put_extra_blks_resvd_cnt),
    glob_dm2_put_ext_append_cnt,
    glob_dm2_put_find_free_block_err,
    glob_dm2_put_find_free_block_race,
    glob_dm2_put_find_obj_in_cache_err,
    glob_dm2_put_full_block_unavl_err,
    glob_dm2_put_free_page_alloc_fail_err,
    glob_dm2_put_free_block_alloc_fail_err,
    glob_dm2_put_free_page_list_unaligned_err,
    glob_dm2_put_free_page_list_empty_err,
    glob_dm2_put_get_new_freespace_device_err,
    glob_dm2_put_illegal_component_err,
    glob_dm2_put_invalid_attrsize_err,
    glob_dm2_put_invalid_content_len_err,
    glob_dm2_put_invalid_input_err,
    glob_dm2_put_invalid_offset_err,
    glob_dm2_put_invalid_slash_err,
    glob_dm2_put_invalid_total_attr_size_err,
    glob_dm2_put_invalid_uri_err,
    glob_dm2_put_mismatched_len_err,
    glob_dm2_put_no_lead_slash_err,
    glob_dm2_put_null_cod_err,
    glob_dm2_put_null_uri_err,
    glob_dm2_put_same_block_not_enough_cnt,
    glob_dm2_put_too_many_dir_level_err,
    glob_dm2_put_unaligned_attr_err,
    glob_dm2_put_unexpected_attr_err,
    glob_dm2_put_write_attr_trunc_cnt,
    glob_dm2_put_write_attr_trunc_err,
    glob_dm2_put_write_content_err,
    glob_dm2_put_zero_len_err,
    glob_dm2_put_mkdir_err,
    glob_dm2_put_no_meta_space_err,
    glob_dm2_put_resv_fail_cnt,
    glob_dm2_put_create_mem_ext_err,
    glob_dm2_put_del_disk_ext_err,
    glob_dm2_put_ext_physid_remove_err,
    glob_dm2_put_remove_mem_ext_err,

    glob_dm2_put_dm2_get_pages_err,
    NOZCC_DECL(glob_dm2_put_dm2_get_pages_warn),
    glob_dm2_put_dm2_write_attr_dup,
    glob_dm2_put_soft_reject_cnt,
    glob_dm2_put_hard_reject_cnt,

    glob_dm2_delete_cnt,
    glob_dm2_delete_err,
    glob_dm2_delete_not_found_warn,
    glob_dm2_delete_uri_cnt,
    glob_dm2_delete_invalid_input_err,
    glob_dm2_delete_free_uri_null_physid_warn,
    glob_dm2_delete_skip_preread_warn,
    glob_dm2_delete_bad_uri_skip_evict,

    NOZCC_DECL(glob_dm2_attr_update_cnt),
    glob_dm2_attr_update_hotness_cnt,
    glob_dm2_attr_update_expiry_cnt,
    glob_dm2_attr_update_sync_cnt,
    glob_dm2_attr_update_content_length_mismatch_warn,
    glob_dm2_attr_update_hotval_zero_cnt,
    glob_dm2_attr_update_err,
    glob_dm2_attr_update_disk_magic_err,
    glob_dm2_attr_update_disk_version_err,
    glob_dm2_attr_update_expired_cnt,
    glob_dm2_attr_update_magic_err,
    glob_dm2_attr_update_not_init_warn,
    glob_dm2_attr_update_open_err,
    glob_dm2_attr_update_pread_dad_err,
    glob_dm2_attr_update_pread_attr_err,
    glob_dm2_attr_update_pwrite_attr_err,
    glob_dm2_attr_update_uri_err,
    glob_dm2_attr_update_version_err,
    glob_dm2_attr_update_invalid_input_err,
    glob_dm2_attr_update_ptype_err,
    NOZCC_DECL(glob_dm2_attr_update_uri_not_found_warn),
    NOZCC_DECL(glob_dm2_raw_open_cnt),
    glob_dm2_attr_update_log_hotness,
    glob_dm2_attr_update_cache_pin_reset_pin_disabled_cnt,
    glob_dm2_attr_update_cache_pin_reset_size_cnt,
    glob_dm2_attr_update_cache_pin_enospc_err,
    glob_dm2_attr_update_skip_bad_uri,
    glob_dm2_raw_open_err,
    NOZCC_DECL(glob_dm2_raw_close_cnt),
    glob_dm2_raw_close_err,
    NOZCC_DECL(glob_dm2_raw_read_cnt),
    glob_dm2_raw_read_err,
    NOZCC_DECL(glob_dm2_raw_read_bytes),
    NOZCC_DECL(glob_dm2_raw_seek_cnt),
    glob_dm2_raw_seek_err,
    NOZCC_DECL(glob_dm2_raw_write_cnt),
    glob_dm2_raw_write_err,
    NOZCC_DECL(glob_dm2_raw_write_bytes),

    NOZCC_DECL(glob_dm2_bitmap_open_cnt),
    glob_dm2_bitmap_open_err,
    glob_dm2_bitmap_close_cnt,
    glob_dm2_bitmap_close_err,
    glob_dm2_bitmap_read_cnt,
    glob_dm2_bitmap_read_err,
    glob_dm2_bitmap_read_bytes,
    NOZCC_DECL(glob_dm2_bitmap_write_cnt),
    glob_dm2_bitmap_write_err,
    NOZCC_DECL(glob_dm2_bitmap_write_bytes),

    NOZCC_DECL(glob_dm2_container_open_cnt),
    glob_dm2_container_open_err,
    NOZCC_DECL(glob_dm2_container_close_cnt),
    glob_dm2_container_close_err,
    glob_dm2_container_read_cnt,
    glob_dm2_container_read_err,
    glob_dm2_container_read_bytes,
    NOZCC_DECL(glob_dm2_container_write_cnt),
    glob_dm2_container_write_err,
    NOZCC_DECL(glob_dm2_container_write_bytes),
    glob_dm2_container_freed_cnt,
    glob_dm2_container_free_lookup_err,
    glob_dm2_container_free_not_found_err,
    NOZCC_DECL(glob_dm2_container_run_cnt),
    glob_dm2_container_skip_free_cnt,
    glob_dm2_container_rmdir_cnt,
    glob_dm2_container_rmdir_failed_cnt,

    NOZCC_DECL(glob_dm2_attr_open_cnt),
    glob_dm2_attr_open_err,
    NOZCC_DECL(glob_dm2_attr_close_cnt),
    glob_dm2_attr_close_err,
    NOZCC_DECL(glob_dm2_attr_write_cnt),
    glob_dm2_attr_write_err,
    NOZCC_DECL(glob_dm2_attr_write_bytes),
    glob_dm2_attr_read_cnt,
    glob_dm2_attr_read_err,
    glob_dm2_attr_read_bytes,
    glob_dm2_attr_read_setid_cnt,

    glob_dm2_attr_delete_err,
    glob_dm2_attr_delete_read_err,
    glob_dm2_attr_delete_short_read_err,
    glob_dm2_attr_delete_repeated_delete_cnt,
    glob_dm2_attr_delete_bad_magic_err,
    glob_dm2_attr_delete_bad_version_err,
    glob_dm2_attr_delete_basename_mismatch_err,
    glob_dm2_attr_delete_write_err,
    glob_dm2_attr_delete_short_write_err,
    glob_dm2_attr_delete_no_attr_err,
    glob_dm2_attr_delete_open_err,
    glob_dm2_attr_delete_big_obj_cnt,
    glob_dm2_attr_delete_eio_override,

    glob_dm2_ext_delete_err,
    glob_dm2_ext_delete_read_err,
    glob_dm2_ext_delete_short_read_err,
    glob_dm2_ext_delete_repeated_delete_cnt,
    glob_dm2_ext_delete_bad_magic_err,
    glob_dm2_ext_delete_bad_version_err,
    glob_dm2_ext_delete_basename_mismatch_err,
    glob_dm2_ext_delete_write_err,
    glob_dm2_ext_delete_short_write_err,
    glob_dm2_ext_delete_eio_override,

    glob_dm2_misc_open_cnt,
    glob_dm2_misc_open_err,
    glob_dm2_misc_read_err,
    glob_dm2_misc_close_cnt,
    glob_dm2_misc_close_err,

    glob_dm2_move_files,
    glob_dm2_other_files,

    glob_dm2_cleanup,
    glob_dm2_num_caches,

    glob_dm2_alloc_uri_head,
    glob_dm2_alloc_uri_head_uri_name,
    glob_dm2_alloc_uri_head_open_dblk,
    glob_dm2_free_uri_head,
    glob_dm2_free_uri_head_uri_name,
    glob_dm2_uri_create_race,

    glob_dm2_uri_remove_disk_cnt,
    glob_dm2_uri_head_rlock,
    glob_dm2_uri_head_runlock,
    glob_dm2_uri_head_remove_from_cache_err,
    glob_dm2_cont_head_rlock,
    glob_dm2_cont_head_runlock,
    glob_dm2_get_checksum_err,

    glob_dm2_expired_cnt,
    glob_dm2_stat_expired_cnt,
    glob_dm2_ext_evict_total_cnt,
    glob_dm2_ext_evict_buf_get_failed_cnt,
    glob_dm2_ext_evict_delete_cnt,

    NOZCC_DECL(glob_dm2_ns_preread_not_pinned_cnt),
    glob_dm2_ns_preread_pinned_expired_cnt,
    glob_dm2_ns_preread_pinned_cnt,
    glob_dm2_ns_preread_not_lowest_tier_cnt,
    glob_dm2_ns_preread_lowest_tier_cnt,
    glob_dm2_ns_preread_not_cache_enabled_cnt,
    glob_dm2_ns_preread_cache_enabled_cnt,
    glob_dm2_ns_put_namespace_enospc_err,
    glob_dm2_ns_put_cache_pin_enospc_err,
    glob_dm2_ns_put_cache_pin_err,
    glob_dm2_ns_put_namespace_free_err,
    glob_dm2_ns_put_resv_fail_cnt,
    glob_dm2_ns_delete_namespace_free_err,

    glob_dm2_cod_null_cnt,
    glob_dm2_mem_overflow,
    glob_dm2_cont_head_free_empty_containers,

    glob_dm2_sata_tier_avl,
    glob_dm2_sas_tier_avl,
    glob_dm2_ssd_tier_avl,

    glob_dm2_sata_preread_done,
    glob_dm2_sas_preread_done,
    glob_dm2_ssd_preread_done;

uint64_t glob_dm2_mem_max_bytes;
uint64_t glob_dm2_init_done;
uint64_t glob_dm2_put_race_without_uri;

AO_t glob_dm2_alloc_total_bytes;
uint64_t glob_dm2_duplicate_block_added_cnt;

void dm2_print_bitmap_free_list(int index);
void dm2_print_uri(DM2_uri_t *uri_head);
void dm2_print_free_bitmap(int ct_index, int ci_index, int sort);


/* Delete this code when real function is written */
extern void nvsd_disk_mgmt_force_offline(char *, char *);
void
nvsd_disk_mgmt_force_offline(char *serial_num,
			     char *mgmt_name)
{
    int err = 0;
    bn_request *req = NULL;
    gcl_session *sess;

    DBG_DM2S("%s %s", serial_num, mgmt_name);

    err = bn_event_request_msg_create(&req,
				     "/nkn/nvsd/diskcache/events/disk_failure");
    bail_error(err);

    err = bn_event_request_msg_append_new_binding(req, 0,
						  "disk_name", bt_string,
						  mgmt_name, NULL);
    bail_error(err);

    err = bn_event_request_msg_append_new_binding(req, 0,
						  "disk_sno", bt_string,
						  serial_num, NULL);
    bail_error(err);

    if (nvsd_mcc) {
	sess = mdc_wrapper_get_gcl_session(nvsd_mcc);
	bail_require(sess);
	if (sess) {
	    err = bn_request_msg_send(sess, req);
	    bail_error(err);
	}
    }

bail:
    return;
}

static void
dm2_inc_raw_write_cnt(dm2_cache_type_t	*ct,
		      dm2_cache_info_t	*ci,
		      int		num)
{
    glob_dm2_raw_write_cnt += num;
    NKN_ASSERT(ct && ci);
    ct->ct_dm2_raw_write_cnt += num;
    ci->ci_dm2_raw_write_cnt += num;
}	/* dm2_inc_raw_write_cnt */


static void
dm2_inc_raw_write_bytes(dm2_cache_type_t *ct,
			dm2_cache_info_t *ci,
			size_t		 nbytes)
{
    glob_dm2_raw_write_bytes += nbytes;
    NKN_ASSERT(ct && ci);
    ct->ct_dm2_ingest_bytes += nbytes;
    ct->ct_dm2_raw_write_bytes += nbytes;
    ci->ci_dm2_raw_write_bytes += nbytes;
}	/* dm2_inc_raw_write_bytes */


static void
dm2_inc_raw_read_cnt(dm2_cache_type_t	*ct,
		     dm2_cache_info_t	*ci,
		     int		num)
{
    glob_dm2_raw_read_cnt += num;
    NKN_ASSERT(ct && ci);
    ct->ct_dm2_raw_read_cnt += num;
    ci->ci_dm2_raw_read_cnt += num;
}	/* dm2_inc_raw_read_cnt */


static void
dm2_inc_raw_read_bytes(dm2_cache_type_t *ct,
		       dm2_cache_info_t *ci,
		       size_t		 nbytes)
{
    glob_dm2_raw_read_bytes += nbytes;
    NKN_ASSERT(ct && ci);
    ct->ct_dm2_raw_read_bytes += nbytes;
    ci->ci_dm2_raw_read_bytes += nbytes;
}	/* dm2_inc_raw_read_bytes */


static void
dm2_inc_bitmap_write_cnt(dm2_cache_info_t *ci,
			 int		  num)
{
    glob_dm2_bitmap_write_cnt += num;
    NKN_ASSERT(ci);
    ci->ci_dm2_bitmap_write_cnt += num;
}	/* dm2_inc_bitmap_write_cnt */


static void
dm2_inc_bitmap_write_bytes(dm2_cache_info_t *ci,
			   size_t	     nbytes)
{
    glob_dm2_bitmap_write_bytes += nbytes;
    NKN_ASSERT(ci);
    ci->ci_dm2_bitmap_write_bytes += nbytes;
}	/* dm2_inc_bitmap_write_bytes */

void
dm2_inc_container_write_bytes(dm2_cache_type_t *ct,
			      dm2_cache_info_t *ci,
			      size_t	       nbytes)
{
    NKN_ASSERT(ct && ci);
    ct->ct_dm2_container_write_bytes += nbytes;
    ct->ct_dm2_container_write_cnt++;
    ci->ci_dm2_cont_write_bytes += nbytes;
    ci->ci_dm2_cont_write_cnt++;
}	/* dm2_inc_container_write_bytes */

void
dm2_inc_container_read_bytes(dm2_cache_type_t *ct,
			     dm2_cache_info_t *ci,
			     size_t	       nbytes)
{
    NKN_ASSERT(ct && ci);
    ct->ct_dm2_container_read_bytes += nbytes;
    ci->ci_dm2_cont_read_bytes += nbytes;
    ci->ci_dm2_cont_read_cnt++;
}	/* dm2_inc_container_read_bytes */

void
dm2_inc_attribute_write_bytes(dm2_cache_type_t *ct,
			      dm2_cache_info_t *ci,
			      size_t	       nbytes)
{
    NKN_ASSERT(ct && ci);
    ct->ct_dm2_attrfile_write_bytes += nbytes;
    ct->ct_dm2_attrfile_write_cnt++;
    ci->ci_dm2_attr_write_bytes += nbytes;
    ci->ci_dm2_attr_write_cnt++;
}	/* dm2_inc_attribute_write_bytes */

void
dm2_inc_attribute_read_bytes(dm2_cache_type_t *ct,
			     dm2_cache_info_t *ci,
			     size_t	       nbytes)
{
    NKN_ASSERT(ct && ci);
    ct->ct_dm2_attrfile_read_bytes += nbytes;
    ct->ct_dm2_attrfile_read_cnt++;
    ci->ci_dm2_attr_read_bytes += nbytes;
    ci->ci_dm2_attr_read_cnt++;
}	/* dm2_inc_attribute_read_bytes */

static void
dm2_inc_put_cnt(dm2_cache_type_t *ct,
		dm2_cache_info_t *ci)
{
    glob_dm2_put_cnt++;
    if (ct)
	ct->ct_dm2_put_cnt++;
    if (ci)
	ci->ci_dm2_put_cnt++;
}	/* dm2_inc_put_cnt */

static void
dm2_inc_put_err(dm2_cache_type_t *ct)
{
    glob_dm2_put_err++;
    if (ct)
	ct->ct_dm2_put_err++;
}	/* dm2_inc_put_err */

void
dm2_uri_log_state(DM2_uri_t *uri_head,
		  int uri_op,
		  int uri_flags,
		  int uri_errno,
		  int uri_wowner,
		  uint32_t uri_log_ts) {

    dm2_uri_lock_t *ulock = uri_head->uri_lock;
    int idx = ulock->uri_log_idx;

    if (uri_op)
	ulock->uri_log[idx].uri_lastop = uri_op;

    if (uri_wowner)
	ulock->uri_log[idx].uri_wowner = uri_wowner;

    if (uri_flags)
	ulock->uri_log[idx].uri_flags = uri_flags;

    if (uri_errno)
	ulock->uri_log[idx].uri_errno = uri_errno;

    if (uri_log_ts)
	ulock->uri_log[idx].uri_log_ts = (uri_log_ts & 0xFFFF);
}   /* dm2_uri_log_state */

void
dm2_uri_log_bump_idx(DM2_uri_t *uri_head) {

    if (uri_head->uri_lock->uri_log_idx == DM2_URI_LOG_OP_MAX - 1)
	uri_head->uri_lock->uri_log_idx = 0;
    else
	uri_head->uri_lock->uri_log_idx++;
}   /* dm2_uri_log_bump_idx */

void
dm2_uri_log_dump(DM2_uri_t *uri_head) {

    int i, idx = uri_head->uri_lock->uri_log_idx;
    FILE *dbg_fp = fopen("/nkn/tmp/dm2_debug.out", "w+");

    assert(dbg_fp);
    for (i=idx+1;i<DM2_URI_LOG_OP_MAX; i++) {
	fprintf(dbg_fp, "%d. TS: %d, OP: %d, Flag: %d, errno: %d \n",
		 i,
		 uri_head->uri_lock->uri_log[i].uri_log_ts,
		 uri_head->uri_lock->uri_log[i].uri_lastop,
		 uri_head->uri_lock->uri_log[i].uri_flags,
		 uri_head->uri_lock->uri_log[i].uri_errno);
    }
    for (i=0;i<idx+1; i++) {
	fprintf(dbg_fp, "%d. TS: %d, OP: %d, Flag: %d, errno: %d \n",
		 i,
		 uri_head->uri_lock->uri_log[i].uri_log_ts,
		 uri_head->uri_lock->uri_log[i].uri_lastop,
		 uri_head->uri_lock->uri_log[i].uri_flags,
		 uri_head->uri_lock->uri_log[i].uri_errno);
    }
    fflush(dbg_fp);
    fclose(dbg_fp);
}   /* dm2_uri_log_dump */

/*
 * Register free space and total counters
 */
void
dm2_register_disk_counters(dm2_cache_info_t *ci,
			   uint32_t *ptype_ptr)
{
    int i, hotval;
    char buf[128];

    if (ci->cnt_registered)
	return;
    ci->cnt_registered = 1;

    nkn_mon_add(DM2_PREREAD_STAT_CNT_MON, ci->mgmt_name,
		&ci->ci_preread_stat_cnt,
		sizeof(ci->ci_preread_stat_cnt));
    nkn_mon_add(DM2_PREREAD_STAT_OPT_MON, ci->mgmt_name,
		&ci->ci_preread_stat_opt,
		sizeof(ci->ci_preread_stat_opt));
    nkn_mon_add(DM2_PREREAD_Q_DEPTH_MON, ci->mgmt_name,
		&ci->ci_dm2_preread_q_depth,
		sizeof(ci->ci_dm2_preread_q_depth));
    nkn_mon_add(DM2_PREREAD_DONE_MON, ci->mgmt_name,
		&ci->ci_preread_done, sizeof(ci->ci_preread_done));
    snprintf(buf, sizeof(buf), "disk.%s.preread.queue_depth",
	     ci->mgmt_name);
    nkn_mon_add(buf, "dictionary", &ci->ci_dm2_preread_q_depth,
		sizeof(ci->ci_dm2_preread_q_depth));

    nkn_mon_add(DM2_FREE_PAGES_MON, ci->mgmt_name, &ci->bm.bm_num_page_free,
		sizeof(ci->bm.bm_num_page_free));
    nkn_mon_add(DM2_TOTAL_PAGES_MON, ci->mgmt_name, &ci->set_cache_page_cnt,
		sizeof(ci->set_cache_page_cnt));

    nkn_mon_add(DM2_FREE_BLOCKS_MON, ci->mgmt_name, &ci->bm.bm_num_blk_free,
		sizeof(ci->bm.bm_num_blk_free));
    nkn_mon_add(DM2_TOTAL_BLOCKS_MON, ci->mgmt_name, &ci->set_cache_blk_cnt,
		sizeof(ci->set_cache_blk_cnt));
    nkn_mon_add(DM2_FREE_RESV_BLOCKS_MON, ci->mgmt_name,
		&ci->bm.bm_num_resv_blk_free,
		sizeof(ci->bm.bm_num_resv_blk_free));

    nkn_mon_add(DM2_RAW_READ_CNT_MON, ci->mgmt_name, &ci->ci_dm2_raw_read_cnt,
		sizeof(ci->ci_dm2_raw_read_cnt));
    nkn_mon_add(DM2_RAW_READ_BYTES_MON, ci->mgmt_name,
		&ci->ci_dm2_raw_read_bytes,
		sizeof(ci->ci_dm2_raw_read_bytes));
    nkn_mon_add(DM2_RAW_WRITE_CNT_MON, ci->mgmt_name, &ci->ci_dm2_raw_write_cnt,
		sizeof(ci->ci_dm2_raw_write_cnt));
    nkn_mon_add(DM2_RAW_WRITE_BYTES_MON, ci->mgmt_name,
		&ci->ci_dm2_raw_write_bytes,
		sizeof(ci->ci_dm2_raw_write_bytes));

    nkn_mon_add(DM2_STATE_MON, ci->mgmt_name, &ci->state_overall,
		sizeof(ci->state_overall));

    nkn_mon_add(DM2_PROVIDER_TYPE_MON, ci->mgmt_name, ptype_ptr,
		sizeof(*ptype_ptr));

    nkn_mon_add(DM2_PUT_CNT_MON, ci->mgmt_name, &ci->ci_update_cnt,
		sizeof(ci->ci_update_cnt));
    nkn_mon_add(DM2_URI_HASH_CNT_MON, ci->mgmt_name,
		&ci->ci_uri_hash_table_total_cnt,
		sizeof(ci->ci_uri_hash_table_total_cnt));

    nkn_mon_add(DM2_NO_META_MON, ci->mgmt_name,
		&ci->no_meta, sizeof(ci->no_meta));
    nkn_mon_add(DM2_NO_META_ERR_MON, ci->mgmt_name,
		&ci->ci_dm2_put_no_meta_space_err,
		sizeof(ci->ci_dm2_put_no_meta_space_err));
    nkn_mon_add(DM2_FREE_META_BLKS_MON, ci->mgmt_name,
		&ci->ci_dm2_free_meta_blks_cnt,
		sizeof(ci->ci_dm2_free_meta_blks_cnt));
    nkn_mon_add(DM2_RM_ATTRFILE_CNT_MON, ci->mgmt_name,
		&ci->ci_dm2_remove_attrfile_cnt,
		sizeof(ci->ci_dm2_remove_attrfile_cnt));
    nkn_mon_add(DM2_RM_CONTFILE_CNT_MON, ci->mgmt_name,
		&ci->ci_dm2_remove_contfile_cnt,
		sizeof(ci->ci_dm2_remove_contfile_cnt));
    nkn_mon_add(DM2_URI_LOCK_IN_USE_CNT_MON, ci->mgmt_name,
		&ci->ci_dm2_uri_lock_in_use_cnt,
		sizeof(ci->ci_dm2_uri_lock_in_use_cnt));

    nkn_mon_add(DM2_EXT_EVICT_DELS_MON, ci->mgmt_name,
		&ci->ci_dm2_ext_evict_total_cnt,
		sizeof(ci->ci_dm2_ext_evict_total_cnt));
    nkn_mon_add(DM2_EXT_EVICT_FAILS_MON, ci->mgmt_name,
		&ci->ci_dm2_ext_evict_fail_cnt,
		sizeof(ci->ci_dm2_ext_evict_fail_cnt));

    nkn_mon_add("dm2_ext_append_cnt", ci->mgmt_name,
		&ci->ci_dm2_ext_append_cnt,
		sizeof(ci->ci_dm2_ext_append_cnt));
    /* Write throttling reject counters */
    nkn_mon_add("write_throttle.hard_rejects", ci->mgmt_name,
		&ci->ci_hard_rejects,
		sizeof(ci->ci_hard_rejects));
    nkn_mon_add("write_throttle.soft_rejects", ci->mgmt_name,
		&ci->ci_soft_rejects,
		sizeof(ci->ci_soft_rejects));

    /* Name the evict bucket counters based on the hotness values
     * Hotness 0 - 128 will have unique buckets
     * 129 - 2G-1 will be grouped into power of two buckets
     * The counter name will have the highest hotness value in the bucket
     */
    for (i=0; i<DM2_MAX_EVICT_BUCKETS; i++) {
	if (i <= DM2_UNIQ_EVICT_BKTS) {
	    snprintf(buf, 128, "dm2_evict_bkt_%d_entries", i);
	} else {
	    hotval = 1 << (i - DM2_UNIQ_EVICT_BKTS + DM2_MAP_UNIQ_BKTS);
	    snprintf(buf, 128, "dm2_evict_bkt_%d_entries", hotval - 1);
	}
        nkn_mon_add(buf, ci->mgmt_name,
		    &ci->evict_buckets[i].num_entries,
		    sizeof(ci->evict_buckets[i].num_entries));
    }
}	/* dm2_register_disk_counters */


static int
dm2_get_provider_type(const char* tier_name)
{
    assert(tier_name != NULL);
    if (!strcasecmp(tier_name, DM2_TYPE_SATA)) {
	return SATADiskMgr_provider;
    } else if (!strcasecmp(tier_name, DM2_TYPE_SAS)) {
	return SAS2DiskMgr_provider;
    } else if (!strcasecmp(tier_name, DM2_TYPE_SSD)) {
	return SolidStateMgr_provider;
    } else {
	return -1;
    }
}


dm2_cache_type_t *
dm2_get_cache_type(int i)
{
    return &g_cache2_types[i];
}   /* dm2_get_cache_type */

static int
dm2_count_number_valid_caches(dm2_cache_type_t *ct)
{
    GList *cptr;
    int cnt = 0;
    dm2_cache_info_t *ci;

    dm2_ct_info_list_rwlock_rlock(ct);
    for (cptr = ct->ct_info_list; cptr; cptr = cptr->next) {
	ci = (dm2_cache_info_t *)cptr->data;
	if (ci->valid_cache)
	    cnt++;
    }
    dm2_ct_info_list_rwlock_runlock(ct);
    return cnt;
}	/* dm2_count_number_valid_caches */


uint32_t
dm2_get_blocksize(const dm2_cache_info_t *ci)
{
    dm2_bitmap_header_t *bmh;

    bmh = (dm2_bitmap_header_t *)ci->bm.bm_buf;
    return bmh->u.bmh_disk_blocksize;
}	/* dm2_get_blocksize */


uint32_t
dm2_get_pagesize(const dm2_cache_info_t *ci)
{
    dm2_bitmap_header_t *bmh;

    bmh = (dm2_bitmap_header_t *)ci->bm.bm_buf;
    return bmh->u.bmh_disk_pagesize;
}   /* dm2_get_pagesize */


int
dm2_cache_container_stat(const char *uri,
			 dm2_cache_info_t *ci)
{
    char	container_pathname[PATH_MAX];
    char	*temp, *uri_dir;
    struct stat sb;
    int		len;

    if (ci->valid_cache == 0) {
	return -ENOENT;
    }

    strcpy(container_pathname, ci->ci_mountpt);
    len = strlen(container_pathname);

    if (container_pathname[len-1] == '/') {
	if (uri[0] == '/')
	    container_pathname[len-1] = '\0';
    } else {
	if (uri[0] != '/') {
	    container_pathname[len] = '/';
	    container_pathname[len+1] = '\0';
	}
    }
    uri_dir = alloca(strlen(uri)+1);
    strcpy(uri_dir, uri);
    temp = dm2_uri_dirname(uri_dir, 1);

    strcat(container_pathname, uri_dir);
    /*
     * chop off basename to find directory name.  uri doesn't need to
     * have a slash in it.
     */
    strcat(container_pathname, NKN_CONTAINER_NAME);
    glob_dm2_stat_sys_cnt++;
    if (stat(container_pathname, &sb) == 0) {
	/* If the container file is not a regular file, we probably found
	 * a directory */
	if (! S_ISREG(sb.st_mode))
	    return -ENOENT;
	else
	    return 0;
    } else
	return -ENOENT;
}	/* dm2_cache_container_stat */


dm2_cache_type_t *
dm2_find_cache_list(const char *type,
		    const int  num_cache_types)
{
    int i;

    for (i = 0; i < num_cache_types; i++) {
	if (!strcmp(g_cache2_types[i].ct_name, type)) {
	    return (&g_cache2_types[i]);
	}
    }
    return NULL;
}	/* dm2_find_cache_list */

uint64_t
dm2_find_raw_dev_size(const char *stat_name)
{
    FILE *fp;
    size_t size = 0;
    int ret;

    glob_dm2_misc_open_cnt++;
    if ((fp = fopen(stat_name, "r")) == NULL) {
	DBG_DM2S("Failed to open sizefile for partition %s: %d",
		 stat_name, errno);
	glob_dm2_misc_open_err++;
	return 0;
    }
    if ((ret = fscanf(fp, "%ld", &size)) != 1) {
	DBG_DM2S("Failed to read sizefile for partition %s: %d/%d",
		 stat_name, ret, errno);
	glob_dm2_misc_read_err++;
	fclose(fp);
	return 0;
    }
    glob_dm2_misc_close_cnt++;
    fclose(fp);
    return size;
}	/* dm2_find_raw_dev_size */

void
dm2_find_a_disk_slot(GList	*disk_slot_list,
		     const char *serial_num,
		     int	*slot,
		     int	*disk_found)
{
    GList	    *dsptr;
    dm2_disk_slot_t *ds;

    /*
     * If the list is NULL, then we don't have slot information.  In this
     * case, we mark the drive as found; and other code will assign
     * incrementing slot ids.
     */
    if (disk_slot_list == NULL) {
	*disk_found = 1;	/* by definition, disk is found */
	return;
    }

    dsptr = g_list_first(disk_slot_list);
    while (dsptr) {
	ds = (dm2_disk_slot_t *)dsptr->data;
	if (!strcasecmp(serial_num, ds->ds_serialnum)) {
	    /* Found it */
	    *slot = ds->ds_slot;
	    *disk_found = 1;
	    return;
	}
	dsptr = dsptr->next;
    }
    *disk_found = 0;
    return;
}	/* dm2_find_a_disk_slot */


void
dm2_get_device_disk_slots(GList **disk_slot_list_ret)
{
    nknexecd_retval_t exec_reply;
    dm2_disk_slot_t   *ds;
    char	      command[NKNEXECD_MAXCMD_LEN];
    char	      reply_basename[NKNEXECD_MAXBASENAME_LEN];
    char	      serialnum[DM2_MAX_SERIAL_NUM];
    char	      devname[DM2_MAX_DISKNAME];
    char	      mountpt[DM2_MAX_MOUNTPT];
    char	      junk[DM2_MAX_DISKNAME];
    GList	      *disk_slot_list = NULL;
    FILE	      *fp;
    int		      ret, slot;

    /*
     * We use -C option so that we can reuse old findslot output if
     * mount-new is not run after previous findslot command. This will
     * reduce the number of sas2ircu calls when multiple drives are
     * pushed in at once and mount-new is issued.
     */
    snprintf(command, NKNEXECD_MAXCMD_LEN, "%s -a -C", DM2_FINDSLOT_CMD);
    memset(&exec_reply, 0, sizeof(exec_reply));
    strcpy(reply_basename, "findslot");
    nknexecd_run(command, reply_basename, &exec_reply);
    if (exec_reply.retval_reply_code != 0) {
	nkn_print_file_to_syslog(exec_reply.retval_stderrfile, 10);
	return;
    }
    if ((fp = fopen(exec_reply.retval_stdoutfile, "r")) == NULL) {
	DBG_DM2S("Failed to open slot file: %s", exec_reply.retval_stdoutfile);
	return;
    }

    /* Strip off header */
    ret = fscanf(fp, "%s %s %s %s", junk, devname, mountpt, serialnum);
    if (ret != 4) {
	goto file_err;
    }
    while ((ret = fscanf(fp, "%d %s %s %s",
			 &slot, devname, mountpt, serialnum)) == 4) {
	if ((ds = dm2_calloc(1, sizeof(*ds), mod_dm2_disk_slot_t)) == NULL) {
	    DBG_DM2S("Calloc failed: size=%ld", sizeof(*ds));
	    goto file_err;
	}
	/* No point calling strncpy.  corruption would already happen if the
	 * string were too long */
	strcpy(ds->ds_serialnum, serialnum);
	strcpy(ds->ds_devname, devname);
	ds->ds_slot = slot;
	disk_slot_list = g_list_append(disk_slot_list, ds);
    }

 file_err:
    if (fp)
	fclose(fp);
    *disk_slot_list_ret = disk_slot_list;
    return;
}	/* dm2_get_device_disk_slots */


void
dm2_free_disk_slot_list(GList *disk_slot_list)
{
    GList *dsptr;

    if (disk_slot_list == NULL)
	return;

    dsptr = g_list_first(disk_slot_list);
    while (dsptr) {
	free((dm2_disk_slot_t *)dsptr->data);
	dsptr = dsptr->next;
    }
    g_list_free(disk_slot_list);
    return;
}	/* dm2_free_disk_slot_list */

/*
 * Since we control the mount points, there should never be a trailing /
 */
static int
dm2_search_proc_mounts(const char *devname,
		       const char *mountpt,
		       char	  *dev_mountpt,
		       int	  *sameline,
		       int	  *devfound,
		       int	  *mountfound)
{
    FILE	  *mnt;
    struct mntent *fs;
    int		  ret = 0;

    if ((mnt = setmntent("/proc/mounts", "r")) == 0) {
	ret = -errno;
	DBG_DM2S("Failed to open mount file (/proc/mounts): %d", errno);
	return ret;
    }
    while ((fs = getmntent(mnt)) != NULL) {
	if (!strcmp(fs->mnt_dir, mountpt)) {
	    *mountfound = 1;
	    if (!strncmp(fs->mnt_fsname, devname, strlen(devname))) {
		*sameline = 1;
		*devfound = 1;
		break;
	    }
	}
	/* Match the base part of devname w/o the partition number */
	if (!strncmp(fs->mnt_fsname, devname, strlen(devname))) {
	    *devfound = 1;
	    strcpy(dev_mountpt, fs->mnt_dir);
	}
	if (*mountfound && *devfound)
	    break;
    }
    endmntent(mnt);
    return 0;
}	/* dm2_search_proc_mounts */


int
dm2_mgmt_is_disk_mounted(const char *devname,
	       const char *mountpt,
	       int	  *mounted)
{
    char dev_mountpt[DM2_MAX_MOUNTPT];
    int sameline = 0, devfound = 0, mountfound = 0, ret;

    ret = dm2_search_proc_mounts(devname, mountpt, dev_mountpt,
				 &sameline, &devfound, &mountfound);
    if (ret != 0) {
	DBG_DM2S("Cannot mount cache for %s: /proc/mounts unavailable",
		 devname);
	NKN_ASSERT(0);	// Should never happen
	return -ENOENT;
    }

    if (mountfound && devfound)
	*mounted = 1;
    else
	*mounted = 0;

    return 0;
}	/* dm2_mgmt_is_disk_mounted */


/*
 * If we can't unmount all resources, then we can't guarantee data integrity.
 */
int
dm2_clean_mount_points(const char *devname,
		       const char *mountpt,
		       const int  umount_all,
		       int	  *mounted)
{
    char dev_mountpt[DM2_MAX_MOUNTPT];
    int sameline = 0, devfound = 0, mountfound = 0, ret;

    ret = dm2_search_proc_mounts(devname, mountpt, dev_mountpt,
				 &sameline, &devfound, &mountfound);
    if (ret != 0) {
	DBG_DM2S("Cannot mount cache for %s: /proc/mounts unavailable",
		 devname);
	NKN_ASSERT(0);	// Should never happen
	return -ENOENT;
    }
    if (sameline && !umount_all) {
	*mounted = 1;
	return 0;
    }
    if (mountfound) {
	DBG_DM2M("Unmounting %s", mountpt);
	if (dm2_umount_fs(mountpt) < 0) {
	    ret = -errno;
	    DBG_DM2S("umount failed for (%s): %d", mountpt, errno);
	    return ret;
	}
    }
    if (devfound && !sameline) {
	DBG_DM2M("Unmounting %s", dev_mountpt);
	if (dm2_umount_fs(dev_mountpt) < 0) {
	    ret = -errno;
	    DBG_DM2S("umount failed for (%s) (%s): %d",
		     devname, dev_mountpt, errno);
	    return ret;
	}
    }
    return 0;
}	/* dm2_clean_mount_points */

void
dm2_setup_am_thresholds(uint32_t max_io_throughput,
			int32_t hotness_threshold,
			nkn_provider_type_t ptype,
			int8_t sub_ptype __attribute((unused)))
{
    AM_pk_t pk;
    char    tmp[MAX_URI_SIZE];

    pk.provider_id = ptype;
    snprintf(tmp, 64, "%d_%d", ptype, 0);
    pk.name = tmp;

    AM_init_provider_thresholds(&pk, max_io_throughput, hotness_threshold);
}	/* dm2_setup_am_threshholds */


int
dm2_get_am_thresholds(int      ptype,
		      int      *cache_use_percent,
		      uint32_t *max_io_throughput,
		      int      *hotness,
		      char     *ctype)
{
    switch (ptype) {
	case SAS2DiskMgr_provider:
	    if (ctype)
		strcpy(ctype, DM2_TYPE_SAS);
	    if (cache_use_percent)
		*cache_use_percent = 95;
	    if (max_io_throughput)
		*max_io_throughput = 70000000;  // 70 MB/sec
	    if (hotness)
		*hotness = am_cache_promotion_hotness_threshold*2;
	    break;

	case SATADiskMgr_provider:
	    if (ctype)
		strcpy(ctype, DM2_TYPE_SATA);
	    if (cache_use_percent)
		*cache_use_percent = 95;
	    if (max_io_throughput)
		*max_io_throughput = 50000000;  // 50 MB/sec
	    if (hotness)
		*hotness = am_cache_promotion_hotness_threshold;
	    break;

	case SolidStateMgr_provider:
	    if (ctype)
		strcpy(ctype, DM2_TYPE_SSD);
	    if (cache_use_percent)
		*cache_use_percent = 95;
	    if (max_io_throughput)
		*max_io_throughput = 200000000; // 240 MB/sec
	    if (hotness)
		*hotness = am_cache_promotion_hotness_threshold*6;
	    break;

	default:
	    return -1;
    }
    return 0;
}

/*
 *  This function will be called from the mgmt when the
 *  am_cache_promotion_hotness_threshold is changed through CLI.
 */
void
DM2_mgmt_update_am_thresholds(void)
{
    nkn_provider_type_t ptype;
    int ptype_idx;
    int cache_use_percent, hotness;
    uint32_t max_io_throughput;

    /* Since the hotness threshold changed, update the tier specific values */
    ptype = SATADiskMgr_provider;
    if ((ptype_idx = dm2_cache_array_map_ret_idx(ptype)) >= 0) {
	dm2_get_am_thresholds(ptype, &cache_use_percent,
			      &max_io_throughput, &hotness, NULL);
	dm2_setup_am_thresholds(max_io_throughput, hotness, ptype, 0);
    }

    ptype = SAS2DiskMgr_provider;
    if ((ptype_idx = dm2_cache_array_map_ret_idx(ptype)) >= 0) {
	dm2_get_am_thresholds(ptype, &cache_use_percent,
			      &max_io_throughput, &hotness, NULL);
	dm2_setup_am_thresholds(max_io_throughput, hotness, ptype, 0);
    }

    ptype = SolidStateMgr_provider;
    if ((ptype_idx = dm2_cache_array_map_ret_idx(ptype)) >= 0){
	dm2_get_am_thresholds(ptype, &cache_use_percent,
			      &max_io_throughput, &hotness, NULL);
	dm2_setup_am_thresholds(max_io_throughput, hotness, ptype, 0);
    }

    return;
}   /* DM2_mgmt_update_am_thresholds */

void
dm2_ct_update_thresholds(dm2_cache_type_t *ct)
{
    ct->ct_internal_high_water_mark =
	dm2_evict_internal_high_water_mark(ct->ct_ptype);
    ct->ct_internal_low_water_mark =
	dm2_evict_internal_low_water_mark(ct->ct_ptype);
    ct->ct_internal_time_low_water_mark =
	 dm2_evict_internal_time_low_water_mark(ct->ct_ptype);
    ct->ct_share_block_threshold =
	dm2_ct_internal_block_share_threshold(ct->ct_ptype);
    ct->ct_evict_hour =
	dm2_ct_evict_hour_threshold(ct->ct_ptype);
    ct->ct_evict_min =
	dm2_ct_evict_min_threshold(ct->ct_ptype);
    ct->ct_num_evict_disk_prcnt =
	dm2_evict_internal_disk_percentage(ct->ct_ptype);
    return;
}   /* dm2_ct_update_thresholds	*/


static int
dm2_check_disk_life(dm2_cache_info_t *ci)
{
    glob_item_t *attr9 = NULL, *attr100 = NULL;
    char *disk, cntr[128];
    uint64_t drive_size_gb, soft_limit_gb, hard_limit_gb, pe_cycles;
    uint64_t attr9_val;
    uint64_t attr100_val;

#define ATTR9_INCR  64
#define DESIRED_LIFE_TIME (7*365.25*24*60*60L)
#define HW_LIFE_TIME (3*365.25*24*60*60L)

    if (ci->ci_throttle == 0)
	return DM2_DISK_NO_THROTTLE;

    disk = strrchr(ci->disk_name, '/');
    disk++;

    snprintf(cntr, 128,"%s.%s", disk, POWER_ON_SECS_9);
    nkncnt_client_get_exact_match(&dm2_smart_ctx, cntr, &attr9);
    attr9_val = attr9->value;

    snprintf(cntr, 128,"%s.%s", disk, GB_ERASED_100);
    nkncnt_client_get_exact_match(&dm2_smart_ctx, cntr, &attr100);
    attr100_val = attr100->value;

    pe_cycles = ci->ci_pe_cycles;
    drive_size_gb = ci->ci_drive_size_gb;

    soft_limit_gb = (pe_cycles * drive_size_gb / DESIRED_LIFE_TIME) * attr9_val;
    hard_limit_gb = ((pe_cycles * drive_size_gb / HW_LIFE_TIME) * attr9_val)
	- ATTR9_INCR;

    if (attr100_val > hard_limit_gb) {
	DBG_DM2W("Disk %s reached hard limit, attr100: %ld, hard_limit_gb: %ld",
		  ci->mgmt_name, attr100_val, hard_limit_gb);
	return DM2_DISK_HARD_THROTTLE;
    }

    if (attr100_val > soft_limit_gb) {
	DBG_DM2W("Disk %s reached soft limit, attr100: %ld, soft_limit_gb: %ld",
		  ci->mgmt_name, attr100_val, soft_limit_gb);
	return DM2_DISK_SOFT_THROTTLE;
    }

    DBG_DM2M("Disk %s is within throttle limits, attr100: %ld"
	     " soft_limit_gb: %ld hard_limit_gb: %ld", ci->mgmt_name,
	     attr100_val, soft_limit_gb, hard_limit_gb);

    return DM2_DISK_NO_THROTTLE;
}   /* dm2_check_disk_life */

void
dm2_print_uri(DM2_uri_t *uri_head)
{
    GList	 *eptr;
    DM2_extent_t *ext;

    eptr = g_list_first(uri_head->uri_ext_list);
    printf("URI name: %s\n", uri_head->uri_name);
    printf("  expiry:	%d\n", uri_head->uri_expiry);
    printf("  content_len:	%ld\n", (long)uri_head->uri_content_len);
    printf("  resv len:	%ld\n", (long)uri_head->uri_resv_len);
    while (eptr) {
	ext = (DM2_extent_t *)eptr->data;
	printf("   uol: offset=%ld  length=%d  phys_start=%ld\n",
	       ext->ext_offset, ext->ext_length,
	       ext->ext_start_sector);
	eptr = eptr->next;
    }
}	/* dm2_print_uri */


void
dm2_make_container_name(const char *mountpt,
			const char *uri_dir,
			char	   *container_pathname)
{
    int cplen;

    strcpy(container_pathname, mountpt);
    cplen = strlen(container_pathname);
    if (uri_dir[0] != '/' && container_pathname[cplen-1] != '/')
	strcat(container_pathname, "/");
    else if (uri_dir[0] == '/' && container_pathname[cplen-1] == '/')
	container_pathname[cplen-1] = '\0';
    strcat(container_pathname, uri_dir);
    if (strlen(uri_dir) > 0)
	strcat(container_pathname, "/");
    strcat(container_pathname, NKN_CONTAINER_NAME);
}	/* dm2_make_container_name */


/*
 * Return 1 if there is a full block available
 *
 * This function is inherently racey!
 */
static int
dm2_full_block_available(dm2_bitmap_t *bm)
{
    if (bm->bm_free_blk_list)
	return 1;

    return 0;
}	/* dm2_full_block_available */

static void
dm2_put_cleanup_uri_disk_block(DM2_uri_t *uri_head)
{
    if (uri_head->uri_open_dblk == NULL)
	return;

    /* Return the unused pages back */
    (void)dm2_return_unused_pages(NULL, uri_head);

    /* Free the uri disk block memory, we can allocate again if needed */
    dm2_free(uri_head->uri_open_dblk, sizeof(dm2_disk_block_t), DM2_NO_ALIGN);
    uri_head->uri_open_dblk = NULL;
    return;
}   /* dm2_put_cleanup_uri_disk_block */

/*
 * Return all the unused pages in the given container as indicated by
 * what is in the cont->c_open_blk structure.  The bitmap indicates
 * precisely which pages are free.
 */
int
dm2_return_unused_pages(dm2_container_t *cont,
			DM2_uri_t	*uri_head)
{
    dm2_disk_block_t	*blk_map, *open_dblk;
    dm2_bitmap_header_t *bmh;
    dm2_bitmap_t	*bm;
    dm2_cache_info_t	*ci = NULL;
    off_t		pagestart;
    uint32_t		numpages, blksz, max_pg, macro_ret;
    int			ret = 0, cont_wlocked = 0, freed_blocks;

    if (cont)
	dm2_cont_rwlock_wlock(cont, &cont_wlocked, cont->c_dev_ci->mgmt_name);
    blk_map = alloca(sizeof(dm2_disk_block_t));
    memset(blk_map, 0, sizeof(blk_map));

    if (uri_head && uri_head->uri_open_dblk) {
	open_dblk = uri_head->uri_open_dblk;
	ci = uri_head->uri_container->c_dev_ci;
    }
    else if (cont) {
	open_dblk = &cont->c_open_dblk;
	ci = cont->c_dev_ci;
    }

    assert(ci);
    bm = &ci->bm;
    PTHREAD_MUTEX_LOCK(&bm->bm_mutex);

    bmh = (dm2_bitmap_header_t *)bm->bm_buf;
    max_pg = ci->set_cache_page_cnt;
    if (open_dblk->db_num_pages == 0) {
	/* Not yet initialized, so nothing to free */
	goto end;
    }
    blk_map->db_disk_page_sz = bmh->u.bmh_disk_pagesize;
    blk_map->db_num_free_disk_pages = 0;
    blk_map->db_flags |= DM2_DB_ACTIVE;
    blksz = bmh->u.bmh_disk_blocksize;
    /* At this point in time, we should only be running this loop once */
    while (dm2_get_pages(open_dblk, blksz, &pagestart, &numpages,
			 ci->mgmt_name) == 0) {
	blk_map->db_start_page = pagestart;
	blk_map->db_num_pages = numpages;

	ret = dm2_return_pages_to_free_list(blk_map, bm, max_pg, ci,
					    &freed_blocks);
	if (ret < 0) {
	    /*
	     * There is little point in continuing here because a failure
	     * indicates a drive issue.  Close down this cache now.
	     */
	    goto end;
	}
    }

 end:
    PTHREAD_MUTEX_UNLOCK(&bm->bm_mutex);
    if (cont)
	dm2_cont_rwlock_wunlock(cont, &cont_wlocked, cont->c_dev_ci->mgmt_name);
    return ret;
}	/* dm2_return_unused_pages */


static void
dm2_calc_resv_blks(nkn_attr_t *attr,
		   uint64_t   block_size,
		   uint64_t   *resv_blks,
		   int	      shift)
{
    if (nkn_attr_is_streaming(attr))
	*resv_blks = ROUNDUP_PWR2(DM2_STREAMING_OBJ_CONTENT_LEN, block_size);
    else
	*resv_blks = ROUNDUP_PWR2(attr->content_length, block_size);

    *resv_blks >>= shift;
    return;
}	/* dm2_calc_resv_blks */

/* This function should be called with 'ct_dm2_select_disk_mutex' held */
static void
dm2_put_select_new_disk(MM_put_data_t    *put,
			dm2_cache_type_t *ct,
			char		 *put_uri,
			dm2_cache_info_t **best_dev_ci_ret)
{
    dm2_cache_info_t    *ci = NULL, *best_dev_ci = *best_dev_ci_ret;
    GList               *cptr = NULL;
    dm2_bitmap_header_t *bmh = NULL;
    uint64_t		resv_blks = 0;
    int			block_avl = 0;

    dm2_ct_info_list_rwlock_rlock(ct);
    for (cptr = ct->ct_info_list; cptr; cptr = cptr->next) {
	ci = (dm2_cache_info_t *)cptr->data;
	DBG_DM2M("Checking disk %s, URI %s, update_cnt %ld, resv_blks %ld, "
		 "free_blks %ld select_cnt %ld", ci->mgmt_name, put_uri,
                 ci->ci_update_cnt, AO_load(&ci->bm.bm_num_resv_blk_free),
		 ci->bm.bm_num_blk_free, AO_load(&ci->ci_select_cnt));
	if (ci->valid_cache == 0)	// I/O Error
	    continue;
	if (ci->ci_disabling)		// Causes I/O error to disable cache
	    continue;

	/*
	 * For multi-block requests, we need to make sure there is enough
	 * space for the entire object.
	 */
	if (put->attr) {
	    bmh = (dm2_bitmap_header_t *)ci->bm.bm_buf;
	    dm2_calc_resv_blks(put->attr, (uint64_t)bmh->u.bmh_disk_blocksize,
			       &resv_blks, ci->bm.bm_byte2block_bitshift);
	    if (resv_blks > AO_load(&ci->bm.bm_num_resv_blk_free)) {
		DBG_DM2M("[cache_type=%s] Resv blks (%ld) unavailable in "
			 "disk %s", ct->ct_name, resv_blks, ci->mgmt_name)
		continue;
	    }
	}
	/*
	 * Let's forget about trying to fill in partial blocks.  This
	 * should be fine for now because we free entire blocks during
	 * eviction.
	 */
	block_avl = dm2_full_block_available(&ci->bm);
	if (!block_avl) {
	    DBG_DM2S("[cache_type=%s] Disk %s does not have free full blocks",
		     ct->ct_name, ci->mgmt_name);
	    glob_dm2_put_full_block_unavl_err++;
	    continue;
	}

	if (best_dev_ci == NULL) {
	    best_dev_ci = ci;
	    DBG_DM2M("[cache_type=%s] Select Disk %s, URI - %s",
		     ct->ct_name, ci->mgmt_name, put_uri);
	    continue;
	}
	/* Pick the device with the fewest updaters */
	if (AO_load(&ci->ci_update_cnt)< AO_load(&best_dev_ci->ci_update_cnt)) {
	    best_dev_ci = ci;
	    DBG_DM2M("[cache_type=%s] Select Disk %s based on load. URI - %s",
		     ct->ct_name, ci->mgmt_name, put_uri);
	    continue;
	}

	/* Skip the device that was selected for ingest very recently */
	if (AO_load(&ci->ci_select_cnt)< AO_load(&best_dev_ci->ci_select_cnt)) {
	    best_dev_ci = ci;
	    DBG_DM2M("[cache_type=%s] Select Disk %s based on alloc. URI - %s",
		     ct->ct_name, ci->mgmt_name, put_uri);
	    continue;
	}

	/* If update cnts are same, then choose disk with most free space */
	/* We use the resv blocks to select the disk to get an even spread
	 * of ingests. The difference between disks with 'free_blks' would be
	 * of very small order of blocks and thus selecting based on 'free_blks'
	 * would not give a good spread */
	if (AO_load(&ci->ci_update_cnt) == AO_load(&best_dev_ci->ci_update_cnt)
	    && (AO_load(&ci->bm.bm_num_resv_blk_free) >
		AO_load(&best_dev_ci->bm.bm_num_resv_blk_free))) {
		best_dev_ci = ci;
		DBG_DM2M("[cache_type=%s] Select Disk %s based on space."
			 " URI - %s", ct->ct_name, ci->mgmt_name, put_uri);
		continue;
	}
    }
    *best_dev_ci_ret = best_dev_ci;
    dm2_ct_info_list_rwlock_runlock(ct);

    return;
}   /* dm2_put_select_new_disk */

/*
 * If the total length of the URI is greater than a block, then choose a new
 * block for this URI.  It doesn't matter
 *
 * Return the cache with the largest amount of free space available.
 * This should keep all caches equally filled which is what we want when
 * trying to do a performance test or are filling in content from a provider
 * which has a relatively flat name space (Break, for example).  Once we
 * choose a cache, we attempt to stay with that cache for about 20MiB before
 * we switch.
 *
 * Later, we will try to get 2MiB of free space from this device.  It's
 * possible that the device we return has more total space and no free
 * 2MiB chunk but other devices do have that amount.
 *
 * If we need to reserve 2MiB and the current open block for the container has
 * left over unused pages, then we need to return the unused pages before we
 * can allocate the full 2MiB from this cache.
 */
static int
dm2_get_new_freespace_dev_ch_rl_cont_wl_ci_rl(
    MM_put_data_t	 *put,
    char		 *put_uri,
    int			 *same_block,
    dm2_cache_type_t	 *ct,
    dm2_cache_info_t	 **free_space_dev_ci_ret,
    dm2_container_head_t **ch_ret,
    int			 *ch_locked_ret,
    int			 *cont_wlocked_ret,
    int			 *ci_rlocked_ret)
{
    dm2_container_head_t *cont_head = NULL;
    char		uri_dir[PATH_MAX];
    dm2_container_t	*cont = NULL;
    GList		*cptr = NULL, *cont_ptr = NULL;
    dm2_bitmap_t	*bm = NULL;
    dm2_cache_info_t	*best_dev_ci = NULL;
    char		*temp = NULL;
    int			ret, not_found, macro_ret;
    int			cont_wlocked = 0, ch_locked = DM2_CONT_HEAD_UNLOCKED;
    int			ci_try_again_cnt = 0, free_prcnt = -1;

    NKN_ASSERT(*free_space_dev_ci_ret == NULL);

    /* Find uri_dir */
    strcpy(uri_dir, put_uri);
    temp = dm2_uri_dirname(uri_dir, 0);

 ci_disabled_try_again:
    if (ci_try_again_cnt > 20) {
	/* Looks like we are looping forever, exit here now */
	ret = -ENOSPC;
	glob_dm2_put_ci_disabled_loop_err++;
	goto get_freespace_error;
    }
    NKN_MUTEX_LOCKR(&ct->ct_dm2_select_disk_mutex);
    cont_head = dm2_conthead_get_by_uri_dir_rlocked(uri_dir, ct, &ch_locked,
						    NULL, DM2_BLOCKING);
    if (cont_head && cont_head->ch_cont_list) {
	for (cptr = cont_head->ch_cont_list; cptr; cptr = cptr->next) {
	    cont = (dm2_container_t *)cptr->data;
	    if (cont->c_dev_ci->valid_cache == 0) {
		cont->c_blk_allocs = 0;			// not used anymore?
		continue;				// bad drive
	    }
	    bm = &cont->c_dev_ci->bm;
	    if (DM2_VIRGIN_CONT(cont)) {
		/*
		 * Virgin container to be used later.  Disk block information
		 * has not been filled in yet.
		 */
		continue;
	    }
	    if (cont->c_dev_ci->ci_disabling) {
		/*
		 * Disk has been disabled, so no new URIs can be placed here.
		 * The dm2_cache_info_t obj has been removed from the
		 * ci_info_list but the container has yet been removed from
		 * the container list.
		 */
		continue;
	    }

	    /* dirty read */
	    if (!DM2_CONT_OPEN_BLOCK_NOSHARE(cont) &&
		!DM2_CONT_OPEN_BLOCK_DELETED(cont) &&
		!nkn_attr_is_streaming(put->attr)  &&
		((cont->c_open_dblk.db_num_free_disk_pages >=
		Byte2DP_cont(put->attr->content_length, cont)) ||
		ct->ct_share_block_threshold)) {

		dm2_cont_rwlock_wlock(cont, &cont_wlocked, put_uri);
		if (!DM2_CONT_OPEN_BLOCK_NOSHARE(cont) &&
		    !DM2_CONT_OPEN_BLOCK_DELETED(cont) &&
		    !nkn_attr_is_streaming(put->attr)) {

		    if (ct->ct_share_block_threshold) {
			/* Calculate the occupancy of the open block */
			free_prcnt =
			    (cont->c_open_dblk.db_num_free_disk_pages* 100)/
			     cont->c_open_dblk.db_num_pages;
		    }

		    /* If the occupancy is less than the configured value and
		     * we are within the 'run' limits (number of continuous
		     * blocks used per container) and the object size is within
		     * a block size, share the block */
		    if (((free_prcnt >= ct->ct_share_block_threshold) &&
			(cont->c_run < 10) &&
			(put->attr->content_length <
			 (cont->c_open_dblk.db_num_pages *
			  cont->c_open_dblk.db_disk_page_sz))) ||
		       (cont->c_open_dblk.db_num_free_disk_pages >=
			Byte2DP_cont(put->attr->content_length, cont))) {

			/*
			 * Request will fit in this block but no URI is defined,
			 * so we aren't necessarily defining a new block.  We
			 * disregard the uol.length here because we don't want
			 * wierd sharing scenarios when we get a small PUT for
			 * a large object.
			 */
			*same_block = 1;
			best_dev_ci = cont->c_dev_ci;
			DBG_DM2M("[cache_type=%s] Select Disk %s based on cont."
				 " URI - %s", ct->ct_name,
				 best_dev_ci->mgmt_name, put_uri);
			cont->c_new_space = DM2_CONT_NEW_SPACE_SAME_BLOCK;
			if (ct->ct_share_block_threshold &&
			    (Byte2DP_cont(put->attr->content_length, cont) >
			    cont->c_open_dblk.db_num_free_disk_pages)) {
				DBG_DM2W("[Disk:%s] Sharing Block(uri:%s)",
				    best_dev_ci->mgmt_name, put_uri);
				glob_dm2_container_run_cnt++;
				cont->c_run++;
			}

#if 0
			DBG_DM2S("SAME BLOCK: %p %d %ld %s %s", cont,
			       cont->c_open_dblk.db_num_free_disk_pages,
			       Byte2DP_cont(put->attr->content_length, cont),
			       best_dev_ci->mgmt_name, put_uri);
#endif
			goto get_freespace_runlock_done;
			/* The URI for this container may not be created */
		    } else {
			cont->c_run = 0;
			dm2_cont_rwlock_wunlock(cont, &cont_wlocked,
						put_uri);
		    }
		} else {
		    cont->c_run = 0;
		    dm2_cont_rwlock_wunlock(cont, &cont_wlocked, put_uri);
		}
	    }
	}
	/*
	 * It's possible that none of the existing containers will work, so
	 * we just choose the best device below.  If that ends up choosing
	 * a new device, then we'll need to create a new container on that
	 * device.
	 */
    }

    dm2_put_select_new_disk(put, ct, put_uri, &best_dev_ci);

    if (cont_head && cont_head->ch_cont_list) {
	for (cont_ptr = cont_head->ch_cont_list; cont_ptr;
			cont_ptr = cont_ptr->next) {
	    cont = (dm2_container_t *)cont_ptr->data;
	    /* this is a hack. make sure we have a open block that is filled */
	    if ((cont->c_dev_ci == best_dev_ci) &&
		(cont->c_open_dblk.db_num_pages > 0)) {
		dm2_cont_rwlock_wlock(cont, &cont_wlocked, put_uri);
		if (!DM2_CONT_OPEN_BLOCK_NOSHARE(cont) &&
		    !DM2_CONT_OPEN_BLOCK_DELETED(cont) &&
		    !nkn_attr_is_streaming(put->attr)  &&
		    cont->c_open_dblk.db_num_free_disk_pages >=
		    Byte2DP_cont(put->attr->content_length, cont)) {

		    *same_block = 1;
		    best_dev_ci = cont->c_dev_ci;
		    DBG_DM2M("[cache_type=%s] Select Disk %s based on cont. "
			     "URI %s", ct->ct_name, best_dev_ci->mgmt_name,
			     put_uri);
		    cont->c_new_space = DM2_CONT_NEW_SPACE_SAME_BLOCK;
		    goto get_freespace_runlock_done;
		    /* The URI for this container may not be created */
		} else {
		    dm2_cont_rwlock_wunlock(cont, &cont_wlocked,
					put_uri);
		}
	    }
	}
    }

    cont = NULL;
    not_found = 1;
    if (!cont_head)
	cont_head = dm2_conthead_get_by_uri_dir_rlocked(uri_dir, ct,
							&ch_locked, NULL,
							DM2_BLOCKING);
    //DBG_DM2S("Picked CI=%s cont_head=%p", best_dev_ci->mgmt_name, cont_head);
    if (cont_head) {
	for (cptr = cont_head->ch_cont_list; cptr; cptr = cptr->next) {
	    cont = (dm2_container_t *)cptr->data;
	    if (cont->c_dev_ci->ci_disabling)
		continue;
	    if (best_dev_ci == cont->c_dev_ci) {
		/* Only malloc or write can fail. gets/releases cont rwlock */
		if ((ret = dm2_return_unused_pages(cont, NULL)) < 0) {
		    NKN_MUTEX_UNLOCKR(&ct->ct_dm2_select_disk_mutex);
		    goto get_freespace_error;
		}
		dm2_cont_rwlock_wlock(cont, &cont_wlocked, put_uri);
		cont->c_blk_allocs = NKN_CONTAINER_MAX_BLK_ALLOC;// Not used?
		cont->c_new_space = DM2_CONT_NEW_SPACE_NEW_DISK;
		not_found = 0;
		break;
	    }
	}
	if (not_found)
	    cont = NULL;
    }
    /* We may not lock a container because it may not be created yet */

 get_freespace_runlock_done:
    NKN_MUTEX_UNLOCKR(&ct->ct_dm2_select_disk_mutex);
    *free_space_dev_ci_ret = best_dev_ci;
    *cont_wlocked_ret = cont_wlocked;

    if (best_dev_ci) {
	dm2_ci_dev_rwlock_rlock(best_dev_ci);
	if (best_dev_ci->ci_disabling)
	    goto disk_disabled_exit;
	*ci_rlocked_ret = 1;
	DBG_DM2M("[cache_type=%s] Select Disk %s to ingest URI %s",
		 ct->ct_name, best_dev_ci->mgmt_name, put_uri);
    }
    if (cont_head) {
	*ch_ret = cont_head;
	*ch_locked_ret = ch_locked;
    }
    assert(cont == NULL || *cont_wlocked_ret);
    return 0;

 disk_disabled_exit:
    DBG_DM2S("[cache_type=%s] Disk %s disabled, selecting disk again",
	     ct->ct_name, best_dev_ci->mgmt_name);
    dm2_ci_dev_rwlock_runlock(best_dev_ci);
    *free_space_dev_ci_ret = NULL;
    best_dev_ci = NULL;
    if (*cont_wlocked_ret)
	dm2_cont_rwlock_wunlock(cont, &cont_wlocked, put_uri);
    if (cont_head)
	dm2_conthead_rwlock_runlock(cont_head, ct, &ch_locked);
    glob_dm2_put_ci_disabled_try_again_cnt++;
    ci_try_again_cnt++;
    goto ci_disabled_try_again;

 get_freespace_error:
    if (cont_head)
	dm2_conthead_rwlock_runlock(cont_head, ct, &ch_locked);
    glob_dm2_put_get_new_freespace_device_err++;
    return ret;
}	/* dm2_get_new_freespace_dev_ch_rl_cont_wl_ci_rl */


static inline int
dm2_get_bit_mask(off_t start_page)
{
    int mod_value = start_page % NBBY;
    int mask[] = { 0x1, 0x3, 0x7, 0xf, 0x1f, 0x3f, 0x7f, 0xff };

    return mask[mod_value];
}	/* dm2_get_bit_mask */


/*
 * Given a blk_map, reserve or free the pages based on the value of
 * 'reserve_pages'.  Need to update the disk and the incore free page
 * list.
 */
static int
dm2_update_bitmap(dm2_bitmap_t	   *bm,
		  dm2_disk_block_t *blk_map,
		  int		   reserve_pages,
		  dm2_cache_info_t *ci)
{
    dm2_bitmap_header_t *header;
    off_t	byte0, byteN, byte0sector, byteNsector, write_offset;
    char	*bitbuf, *byte0bitbuf;
    uint32_t	num_pages_modified = blk_map->db_num_pages;
    uint32_t	bit_shift, set_bits, num_pages_bits, sector_bytes;
    int		i, nbytes, ret;

    header = (dm2_bitmap_header_t *)bm->bm_buf;
    bitbuf = (char *)(header+1);
    byte0 = blk_map->db_start_page / NBBY;
    //   bit_shift = blk_map->db_start_page % NBBY;
    bit_shift = blk_map->db_start_page & 0x7;
    byteN = (blk_map->db_start_page + blk_map->db_num_pages - 1) / NBBY;
    bitbuf += byte0;
    for (i = byte0; i <= byteN; i++) {
	set_bits = dm2_get_bit_mask(7 - bit_shift);
	if (8 - bit_shift > num_pages_modified) {
	    num_pages_bits = dm2_get_bit_mask(num_pages_modified - 1);
	    if (reserve_pages)
		*bitbuf |= ((set_bits & num_pages_bits) << bit_shift);
	    else
		*bitbuf &= ~((set_bits & num_pages_bits) << bit_shift);
	    num_pages_modified = 0;
	} else {
	    if (reserve_pages)
		*bitbuf |= (set_bits << bit_shift);
	    else
		*bitbuf &= ~(set_bits << bit_shift);
	    num_pages_modified -= (8 - bit_shift);
	}
	bitbuf++;
	bit_shift = 0;
    }

    /* lseek to correct sector */
    byte0sector = byte0 / DEV_BSIZE;
    byteNsector = byteN / DEV_BSIZE;
    write_offset = DM2_BITMAP_HEADER_SZ + (DEV_BSIZE * byte0sector);

    sector_bytes = DEV_BSIZE * (byteNsector - byte0sector + 1);
    byte0bitbuf = bm->bm_buf + write_offset;
    /* XXXmiken checksums? stored in a different file? */
    dm2_inc_bitmap_write_cnt(ci, 1);
    nbytes = pwrite(bm->bm_fd, byte0bitbuf, sector_bytes, write_offset);
    ret = -errno;
#if 0
    printf("bitmap write at offset=%lu len=%d\n", write_offset, sector_bytes);
#endif
    if ((uint32_t)nbytes != sector_bytes) {
	glob_dm2_bitmap_write_err++;
	if (nbytes == -1) {
	    NKN_ASSERT(ret != -EBADF);
	    DBG_DM2S("IO ERROR:[name=%s] FreeBitmap (%s,%s) write at %p "
                     "for len=%u at offset=%lu in fd=%d failed: %d",
                     ci->mgmt_name, bm->bm_fname, bm->bm_devname,
                     byte0bitbuf, sector_bytes, write_offset, bm->bm_fd,
                     -ret);
	    return ret;
	} else {
	    DBG_DM2S("IO ERROR:[name=%s] FreeBitmap (%s,%s) write at %p"
                     " for len=%u at offset=%lu in fd=%d failed: %d",
                     ci->mgmt_name, bm->bm_fname, bm->bm_devname,
                     byte0bitbuf, sector_bytes, write_offset, bm->bm_fd,
		     -ret);
	    return -EINVAL;
	}
    }
    dm2_inc_bitmap_write_bytes(ci, sector_bytes);

    return 0;
}	/* dm2_update_bitmap */


/*
 * We are trying to delete a specific URI.  While removing this from the URI
 * hash table, we need to see if the URI happens to be located on a
 * currently opened disk block for the container where the URI is located.
 * We should not need to get a lock on the extent list
 */
int
dm2_matching_physid(DM2_uri_t *uri_head)
{
    off_t cont_blkno, ext_blkno;
    GList	 *ext_obj;
    DM2_extent_t *ext;

    /* container not initialized, so can't have matching blkno */
    if (! DM2_DISK_BLOCK_INITED(&uri_head->uri_container->c_open_dblk))
	return 0;

    cont_blkno = uri_head->uri_container->c_open_dblk.db_start_page /
	(uri_head->uri_container->c_blksz /
	 uri_head->uri_container->c_open_dblk.db_disk_page_sz);
    for (ext_obj = uri_head->uri_ext_list; ext_obj; ext_obj = ext_obj->next) {
	ext = (DM2_extent_t *)ext_obj->data;
	ext_blkno = ext->ext_physid & NKN_PHYSID_SECTOR_MASK;
	if (cont_blkno == ext_blkno) {
	    return 1;
	}
    }
    return 0;
}	/* dm2_matching_physid */


static int
dm2_get_block_from_free_blk_list(dm2_bitmap_t *bm,
				 DM2_uri_t *uri_head,
				 uint32_t pagesize,
				 ns_stat_token_t ns_stoken,
				 nkn_provider_type_t ptype,
				 int	uri_disk_block,
				 dm2_disk_block_t **blk_map_ret)
{
    off_t	     blknum = (off_t) bm->bm_free_blk_list->data;
    dm2_disk_block_t *blk_map;
    unsigned char    *free_blk_map;
    int		     blk_idx;

    /* Don't need to zero entire object if we assign most fields */
    if (uri_disk_block)
	blk_map = uri_head->uri_open_dblk;
    else
	blk_map = &uri_head->uri_container->c_open_dblk;

    dm2_db_mutex_lock(blk_map);

    /* Point to the entry in the block bitmap */
    free_blk_map = (unsigned char *)&bm->bm_free_blk_map[blknum/8];
    blk_idx = blknum%8;

    blk_map->db_num_free_disk_pages = bm->bm_pages_per_block;
    blk_map->db_start_page = blknum*bm->bm_pages_per_block;
    blk_map->db_num_pages = bm->bm_pages_per_block;
    blk_map->db_flags = DM2_DB_ACTIVE;
    memset(blk_map->db_used, 0, sizeof(blk_map->db_used));
    bm->bm_num_page_free -= bm->bm_pages_per_block;
    bm->bm_num_blk_free--;
    /* remove the entry from the list and mark it as used in the blk_map */
    bm->bm_free_blk_list = g_list_delete_link(bm->bm_free_blk_list,
					      bm->bm_free_blk_list);
    *free_blk_map &= ~(1 << blk_idx);
    blk_map->db_disk_page_sz = pagesize;	// must be last
    dm2_ns_add_disk_usage(ns_stoken, ptype, 1);
    dm2_db_mutex_unlock(blk_map);

    *blk_map_ret = blk_map;
    return 0;
}	/* dm2_get_block_from_free_blk_list */


static int
cmp_search_ext_list(gconstpointer obj,
		    gconstpointer user_data)
{
    DM2_extent_t *ext = (DM2_extent_t *)obj;
    uint64_t *physid = (uint64_t *)user_data;

    if (ext->ext_physid == *physid)
	return 0;
    else if (ext->ext_physid < *physid)
	return -1;
    else
	return 1;
}	/* cmp_search_ext_list */

static int
dm2_set_same_uri_block(MM_put_data_t *put,
		       DM2_uri_t *uri_head,
		       const dm2_cache_type_t *ct,
		       DM2_extent_t **append_ext_ret)
{
    dm2_container_t *cont = uri_head->uri_container;
    dm2_cache_info_t *ci = cont->c_dev_ci;
    dm2_disk_block_t *db = uri_head->uri_open_dblk;
    GList *found_ext;
    DM2_extent_t    *ext;
    uint64_t blksz = cont->c_blksz;
    uint64_t blknum, physid;
    int same_block = 0;

    assert(db);

    /* It's possible for this db to not be initialized yet */
    if (! DM2_DISK_BLOCK_INITED(db))
	return 0;

    if (db->db_num_free_disk_pages == 0)
	return 0;

    /* TODO: Validate that this block can hold the new PUT request data by
     */

    /*
     * We shouldn't need to lock the db here because this routine is only called
     * when we are choosing a disk for a large URI which will not be sharing
     * the disk block.  On the other hand, the lock should not cause a problem
     * either.
     */
    dm2_db_mutex_lock(db);
    blknum = (db->db_start_page * db->db_disk_page_sz) / blksz;
    physid = nkn_physid_create(ct->ct_ptype, ci->list_index, blknum);
    found_ext = g_list_find_custom(uri_head->uri_ext_list, &physid,
				   cmp_search_ext_list);
    if (found_ext) {
	ext = (DM2_extent_t *)found_ext->data;
	if (ext->ext_offset + ext->ext_length == put->uol.offset) {
	    same_block = 1;
	    *append_ext_ret = ext;
	}
    }
    dm2_db_mutex_unlock(db);

    return same_block;
}	/* dm2_set_same_uri_block */



static int
dm2_set_same_block(DM2_uri_t *uri_head,
		   const dm2_cache_type_t *ct,
		   MM_put_data_t *put)
{
    dm2_container_t *cont = uri_head->uri_container;
    dm2_cache_info_t *ci = cont->c_dev_ci;
    dm2_disk_block_t *db = &cont->c_open_dblk;
    uint64_t blksz = cont->c_blksz;
    uint64_t blknum;
    uint64_t physid;
    GList *found_ext;

    /* It's possible for this db to not be initialized yet */
    if (! DM2_DISK_BLOCK_INITED(db))
	return 0;

    if (db->db_num_free_disk_pages == 0)
	return 0;
    /*
     * This check is added so as to avoid splitting of the incoming put of
     * full block size (2MB for SAS/SATA and 256KB for SSD). We should not
     * share the current block for full block size PUTs
     * Splitting of full writes will cause duplicate block issue: PR 906911
     * For Silverlight objects which are currently 1.1MB, this check will
     * fail and current block will be shared.Thus data will be aligned.
     */
    if (put->uol.length == (db->db_num_pages * db->db_disk_page_sz)) {
	return 0;
    }

    /*
     * For small writes(256KB) in a 2MB block, the above check would fail
     * So to avoid sharing of the block (PR 906911), I have added this
     * check
     */
    if (glob_dm2_small_write_enable &&
       ((put->uol.length % DM2_DISK_BLKSIZE_256K) == 0)) {
	return 0;
    }
    if (!ct->ct_share_block_threshold &&
	(db->db_num_free_disk_pages <
	Byte2DP_cont(uri_head->uri_content_len, uri_head->uri_container))) {
	    (void)dm2_return_unused_pages(uri_head->uri_container, NULL);
	    glob_dm2_put_same_block_not_enough_cnt++;
	    return 0;
    }

    /*
     * We shouldn't need to lock the db here because this routine is only called
     * when we are choosing a disk for a large URI which will not be sharing
     * the disk block.  On the other hand, the lock should not cause a problem
     * either.
     */
    dm2_db_mutex_lock(db);
    blknum = (db->db_start_page * db->db_disk_page_sz) / blksz;
    physid = nkn_physid_create(ct->ct_ptype, ci->list_index, blknum);
    found_ext = g_list_find_custom(uri_head->uri_ext_list, &physid,
				   cmp_search_ext_list);
    dm2_db_mutex_unlock(db);
    return (found_ext != NULL);
}	/* dm2_set_same_block */

static int
dm2_choose_disk_with_uri(MM_put_data_t    *put,
			 char		  *put_uri,  // const
			 dm2_cache_type_t *ct,
			 DM2_uri_t	  *uri_head,
			 int		  *same_block_ret,
			 DM2_extent_t	  **append_ext,
			 int		  *uri_cont_wlocked_ret)
{
    dm2_bitmap_header_t	*bm_header;
    dm2_cache_info_t	*free_space_dev_ci;
    ns_stat_token_t	ns_stoken = put->ns_stoken;
    uint64_t		resv_blks = 0, block_size = 0, resv_len = 0;
    uint64_t		blocks_reqd, blocks_used;
    int			ret = 0, ns_resv_ret = 0;

    NKN_ASSERT(uri_head && uri_head->uri_container);

    /*
     * Once a specific container is chosen, stay with it for this
     * URI.  If we run out, then return ENOSPC for now.  If we are here,
     * then we used up the last block, so we need to allocate a new one.
     */
    free_space_dev_ci = uri_head->uri_container->c_dev_ci;
    uri_head->uri_container->c_blk_allocs--;	// not used anymore?
    bm_header = (dm2_bitmap_header_t *)free_space_dev_ci->bm.bm_buf;
    block_size = bm_header->u.bmh_disk_blocksize;

    if (uri_head->uri_resvd == 0) {
	/*
	 * If this is a partial URI, uri_resv_len would contain
	 * the amount of data that is yet to be written to the disk.
	 * Do the reservation now for the partial URI
	 */
	NKN_ASSERT((int64_t)uri_head->uri_resv_len > 0);
	resv_blks = ROUNDUP_PWR2(uri_head->uri_resv_len, block_size);
	resv_blks >>= free_space_dev_ci->bm.bm_byte2block_bitshift;
    } else {
	/*
	 * Check if we have sufficient reservation. If a previous PUT
	 * did not use the allocated block fully, we may have to adjust
	 * the reservation for this URI.
	 */

	resv_len = uri_head->uri_content_len;
	if (uri_head->uri_obj_type & NKN_OBJ_STREAMING) {
	    if (resv_len <= DM2_STREAMING_OBJ_CONTENT_LEN)
		resv_len = DM2_STREAMING_OBJ_CONTENT_LEN;
	    else
		resv_len = ROUNDUP_PWR2(resv_len,DM2_STREAMING_OBJ_CONTENT_LEN);
	}

	blocks_reqd = ROUNDUP_PWR2(resv_len, block_size);
	blocks_reqd >>= free_space_dev_ci->bm.bm_byte2block_bitshift;

	blocks_used = dm2_uri_physid_cnt(uri_head->uri_ext_list);
	if (blocks_used >= blocks_reqd) {
	    /* OK, Need more .. Reserve one more block for now */
	    glob_dm2_put_extra_blks_resvd_cnt++;
	    if (uri_head->uri_obj_type & NKN_OBJ_STREAMING)
		resv_blks = DM2_STREAMING_OBJ_CONTENT_LEN/block_size;
	    else
		resv_blks = 1;

	    uri_head->uri_resv_len += resv_blks * block_size;
	}
    }

    if (resv_blks) {
	/* Do the reservation here, if available*/
	ns_resv_ret= dm2_ns_resv_disk_usage(ns_stoken, ct->ct_ptype, resv_blks);
	if (ns_resv_ret) {
	    glob_dm2_ns_put_resv_fail_cnt++;
	    DBG_DM2S("[cache_type=%s] Namespace size limit reached",
		     ct->ct_name);
	    ret = -ENOSPC;
	    goto no_locks;
	}

	if (resv_blks >
	    AO_load(&free_space_dev_ci->bm.bm_num_resv_blk_free)) {
	    DBG_DM2S("[cache_type=%s] Can not find free space on disk %s",
		     ct->ct_name, free_space_dev_ci->mgmt_name);
	    dm2_ns_sub_disk_usage(ns_stoken, ct->ct_ptype, resv_blks, 1);
	    ret = -ENOSPC;
	    goto no_locks;
	}

	AO_fetch_and_add(&free_space_dev_ci->bm.bm_num_resv_blk_free,
			 -resv_blks);

	/* Mark that some reservation is done */
	if (uri_head->uri_resvd == 0)
	    uri_head->uri_resvd = 1;
    }

    /* If the PUT is continuing for an object, allocate the memory
     * for the disk_block in the uri_head */
    if (glob_dm2_small_write_enable &&
	(uri_head->uri_content_len >= NKN_MAX_BLOCK_SZ) &&
	uri_head->uri_open_dblk == NULL) {
	    uri_head->uri_open_dblk = dm2_calloc(1, sizeof(dm2_disk_block_t),
						 mod_dm2_disk_block_t);
	    if (uri_head->uri_open_dblk == NULL) {
		DBG_DM2S("[cache_type=%s] dm2_calloc failed: %ld",
			 ct->ct_name, sizeof(dm2_disk_block_t));
		NKN_ASSERT(uri_head->uri_open_dblk);
	    } else {
		glob_dm2_alloc_uri_head_open_dblk++;
	    }
    }

    if (uri_head->uri_open_dblk)
	*same_block_ret = dm2_set_same_uri_block(put, uri_head, ct,
						 append_ext);
    else
	*same_block_ret = dm2_set_same_block(uri_head, ct, put);
    dm2_cont_rwlock_wlock(uri_head->uri_container, uri_cont_wlocked_ret,
			  put_uri);

 no_locks:
    return ret;
}	/* dm2_choose_disk_with_uri */


static int
dm2_choose_disk_without_uri_ci_rl(MM_put_data_t	    *put,
				  char		    *put_uri,
				  dm2_cache_type_t  *ct,
				  DM2_uri_t	    **uri_head_ret,
				  int		    *same_block_ret,
				  dm2_cache_info_t  **free_space_dev_ci_ret,
				  int		    *uri_cont_wlocked_ret)
{
    DM2_uri_t		 *uri_head = NULL;
    dm2_cache_info_t	 *free_space_dev_ci = NULL;
    dm2_container_head_t *cont_head = NULL;
    dm2_container_t	 *cont = NULL;
    dm2_bitmap_header_t  *bm_header;
    uint64_t		 resv_blks = 0;
    int			 stat_mutex_locked, cont_wlocked, same_block,
			 valid_cont_found, cont_head_locked, uri_head_wlocked,
			 uri_cont_wlocked, ci_rlocked, select_disk = 0;
    int			 ret = 0, preread_good = 0, resv_ret = 0, ns_resv_ret;
    int			 uri_disk_block = 0, macro_ret = 0;

    stat_mutex_locked = cont_wlocked = same_block = valid_cont_found = 0;
    uri_head_wlocked = uri_cont_wlocked = ci_rlocked = 0;
    cont_head_locked = DM2_CONT_HEAD_UNLOCKED;

    if (dm2_count_number_valid_caches(ct) == 0) {
	DBG_DM2W("[cache_type=%s] No valid caches exist", ct->ct_name);
	ret = -NKN_DM2_EMPTY_CACHE_TIER;
	goto no_locks;
    }
    dm2_ct_stat_mutex_lock(ct, &stat_mutex_locked, put_uri);
    if (dm2_uri_head_lookup_by_uri(put_uri, cont_head,
				   &cont_head_locked, ct, 0) != -ENOENT) {
	glob_dm2_put_find_free_block_race++;
	DBG_DM2S("PUT race: %s", put_uri);
	ret = -EAGAIN;
	goto unlock_stat_mutex;
    }

    if (ct->ct_tier_preread_stat_opt) {
	/* As long as ct_tier_preread_stat_opt is set and preread is done,
	 * we know that there are no directories which are not found, so
	 * we can drop the stat lock because no disk reads are necessary. */
	dm2_ct_stat_mutex_unlock(ct, &stat_mutex_locked, put_uri);
	preread_good = 1;

    }

    if (glob_dm2_small_write_enable &&
	(put->attr->content_length >= NKN_MAX_BLOCK_SZ)) {
	/* We will have to allocate disk blocks on the uri_head */
	uri_disk_block = 1;
	NKN_MUTEX_LOCKR(&ct->ct_dm2_select_disk_mutex);
	dm2_put_select_new_disk(put, ct, put_uri, &free_space_dev_ci);
	NKN_MUTEX_UNLOCKR(&ct->ct_dm2_select_disk_mutex);
	if (free_space_dev_ci)
	    dm2_ci_dev_rwlock_rlock(free_space_dev_ci);
    } else {
	ret = dm2_get_new_freespace_dev_ch_rl_cont_wl_ci_rl(put, put_uri,
							    &same_block,
							    ct,
							    &free_space_dev_ci,
							    &cont_head,
							    &cont_head_locked,
							    &cont_wlocked,
							    &ci_rlocked);
	if (ret < 0)
	    goto unlock_stat_mutex;
    }
    /* Not locked when container is not created */

    if (free_space_dev_ci == NULL) {
	DBG_DM2S("[cache_type=%s] Can not find free space", ct->ct_name);
	ret = -ENOSPC;
	goto unlock_cont_head_lock;
    }
    if (same_block)
	DBG_DM2M3("[cache_type=%s] Reusing disk block for URI=%s",
		  ct->ct_name, put_uri);

    AO_fetch_and_add1(&free_space_dev_ci->ci_select_cnt);
    select_disk = 1;
    /*
     * Assume that the uri_head is not in the cache and that the object
     * is not in the cache.  If the container is present, it will have
     * already been read into the cache.
     */
    ret = dm2_get_contlist(put_uri, 1, free_space_dev_ci, ct,
			   &valid_cont_found, &cont_head, &cont,
			   &cont_wlocked, &cont_head_locked);
    if (ret < 0)
	goto unlock_cont_wlock;
    /* cont may not be wlocked if we found a container already present */

    if (valid_cont_found == 0) {
	/* Since we are creating this, it shouldn't ever be not found */
	NKN_ASSERT(0);	/* This can happen when we run out of fds */
	ret = -ENOENT;
	goto unlock_cont_head_lock;
    }

    if (preread_good == 0) {
	if ((ret = dm2_read_contlist(cont_head, 0, ct)) < 0)
	    goto unlock_cont_head_lock;
    }

    /* Get the uri_head along with ci rlocked */
    uri_head = dm2_uri_head_get_by_uri_wlocked(put_uri, cont_head,
					       &cont_head_locked, ct, 0,
					       DM2_RLOCK_CI);
    if (uri_head == NULL) {
	ret = dm2_create_uri_head_wlocked(put_uri, cont, ct,
					  uri_disk_block, &uri_head);
	if (!preread_good)
	    dm2_ct_stat_mutex_unlock(ct, &stat_mutex_locked, put_uri);

	if (ret == -ENOMEM) {
	    goto unlock_cont_head_lock;
	} else if (ret != 0) {
	    /* Race to create uri_head with -EINVAL: loop above */
	    if (ret == -EINVAL)
		ret = -EAGAIN;
	    goto unlock_cont_head_lock;
	}
	/*
	 * Now that we have found a free device and successfully created
	 * the uri_head, we should be able to reserve blocks for the URI.
	 * Earlier the reservation was done in
	 * 'dm2_get_new_freespace_dev_cont_wlocked' but that would cause
	 * a reservation leak if anything failed in the sequence of events
	 * up to this point (Eg., malloc failure, mkdir failure etc).
	 */
	if (!same_block) {
	    bm_header = (dm2_bitmap_header_t *)free_space_dev_ci->bm.bm_buf;
	    dm2_calc_resv_blks(put->attr,
			       (uint64_t)bm_header->u.bmh_disk_blocksize,
			       &resv_blks,
			       free_space_dev_ci->bm.bm_byte2block_bitshift);
	    ns_resv_ret = dm2_ns_resv_disk_usage(put->ns_stoken, ct->ct_ptype,
						 resv_blks);
	    if (ns_resv_ret) {
		DBG_DM2S("[cache_type=%s] Namespace reservation limit "
			 "reached", ct->ct_name);
		glob_dm2_ns_put_resv_fail_cnt++;
		if (cont_head_locked != DM2_CONT_HEAD_UNLOCKED)
		    dm2_conthead_rwlock_unlock(cont_head, ct,&cont_head_locked);
		dm2_put_err_uri_cleanup(uri_head, &cont_wlocked, put_uri, ct);
		if (stat_mutex_locked)
		    dm2_ct_stat_mutex_unlock(ct, &stat_mutex_locked, put_uri);
		ret = -ENOSPC;
		goto no_locks;

	    }
	    resv_ret = AO_fetch_and_add(
			     &free_space_dev_ci->bm.bm_num_resv_blk_free,
			     -resv_blks);
	    if (resv_ret - (int)resv_blks < 0) {
		AO_fetch_and_add(&free_space_dev_ci->bm.bm_num_resv_blk_free,
				 resv_blks);

		dm2_ns_sub_disk_usage(put->ns_stoken, ct->ct_ptype,
				      resv_blks, 1);
		DBG_DM2S("[cache_type=%s] Unable to reserve enough space."
			 "disk:%s uri:%s blocks:%ld", ct->ct_name,
			 free_space_dev_ci->mgmt_name, put_uri, resv_blks);
		glob_dm2_put_resv_fail_cnt++;
		/*
		 * No reservation done, undo the uri insertion and free it up.
		 * Unlock the cont_head, URI, ci_dev, cont and stat_mutex locks
		 * The ci & cont are also unlocked as we free up the uri.
		 */
		if (cont_head_locked != DM2_CONT_HEAD_UNLOCKED)
		    dm2_conthead_rwlock_unlock(cont_head, ct,&cont_head_locked);
		dm2_put_err_uri_cleanup(uri_head, &cont_wlocked, put_uri, ct);
		if (stat_mutex_locked)
		    dm2_ct_stat_mutex_unlock(ct, &stat_mutex_locked, put_uri);
		ret = -ENOSPC;
		goto no_locks;
	    }
	}
	uri_head->uri_expiry = put->attr->cache_expiry;
	uri_head->uri_time0 = put->attr->cache_time0;
	/* Routine returns bit set, not 1 or 0 */
	if (ct->ct_ptype == glob_dm2_lowest_tier_ptype)
	    uri_head->uri_cache_pinned = nkn_attr_is_cache_pin(put->attr);
	uri_head->uri_cache_create = (put->attr->origin_cache_create +
				      put->attr->cache_correctedage);
	if (nkn_attr_is_streaming(put->attr)) {
	    uri_head->uri_content_len = 0;
	    uri_head->uri_resv_len = DM2_STREAMING_OBJ_CONTENT_LEN;
	    uri_head->uri_obj_type |= NKN_OBJ_STREAMING;
	    /* Increment the number of streaming PUTs that can be active
	     * simultaneously */
	    AO_fetch_and_add1(&ct->ct_dm2_streaming_curr_cnt);
	} else {
	    uri_head->uri_content_len = put->attr->content_length;
	    uri_head->uri_resv_len = uri_head->uri_content_len;
	}
	uri_head->uri_resvd = 1;
	uri_head_wlocked = 1;
    } else {
	/*
 	 * Race happens in this case. This part of code
 	 * is executed when there is a race in DM2 PUT
 	 */
	glob_dm2_put_race_without_uri++;
	if (!preread_good)
	    dm2_ct_stat_mutex_unlock(ct, &stat_mutex_locked, put_uri);
	uri_head_wlocked = 1;
	if (cont != uri_head->uri_container) {
	    /*
	     * The URI existed but wasn't read in yet, and the free space
	     * code chose a device which is different from the device
	     * where the URI exists.
	     */
	    dm2_cont_rwlock_wunlock(cont, &cont_wlocked,
				    uri_head->uri_name);
	    dm2_cont_rwlock_wlock(uri_head->uri_container,
				  &uri_cont_wlocked,
				  uri_head->uri_name);
	    if (ci_rlocked) {
		dm2_ci_dev_rwlock_runlock(free_space_dev_ci);
		ci_rlocked = 0;
	    }
	    /* 
 	     * Before replacing dev_ci already selected dev_ci 
	     * needs to be decremented.
	     */
	    if (free_space_dev_ci &&
		(select_disk == 1)) {
		AO_fetch_and_sub1(&free_space_dev_ci->ci_select_cnt);
	    }
	    free_space_dev_ci = uri_head->uri_container->c_dev_ci;
	    if (free_space_dev_ci) {
		AO_fetch_and_add1(&free_space_dev_ci->ci_select_cnt);
		select_disk = 1;
	    }
	}
    }

    NKN_ASSERT((uri_head->uri_container->c_rflags & DM2_CONT_RWFLAG_WLOCK) != 0);
    *uri_head_ret = uri_head;
    dm2_conthead_rwlock_unlock(cont_head, ct, &cont_head_locked);

    *free_space_dev_ci_ret = free_space_dev_ci;
    *uri_cont_wlocked_ret = uri_cont_wlocked || cont_wlocked;
    *same_block_ret = same_block;
    if (select_disk)
	AO_fetch_and_sub1(&free_space_dev_ci->ci_select_cnt);
    return 0;

 unlock_cont_head_lock:
    if (cont_head_locked != DM2_CONT_HEAD_UNLOCKED)
	dm2_conthead_rwlock_unlock(cont_head, ct, &cont_head_locked);
 unlock_cont_wlock:
    if ((uri_cont_wlocked ^ cont_wlocked) != 1) {
	DBG_DM2W("[cache_type=%s] URI=%s uri_cont_wlocked=%d cont_wlocked=%d",
		 ct->ct_name, put_uri, uri_cont_wlocked, cont_wlocked);
    }
    if (uri_cont_wlocked)
	dm2_cont_rwlock_wunlock(uri_head->uri_container, &uri_cont_wlocked,
				put_uri);
    if (cont_wlocked)
	dm2_cont_rwlock_wunlock(cont, &cont_wlocked, put_uri);
    if (uri_head)
	dm2_uri_head_rwlock_wunlock(uri_head, ct, DM2_NOLOCK_CI);
 unlock_stat_mutex:
    if (ci_rlocked && free_space_dev_ci)
	dm2_ci_dev_rwlock_runlock(free_space_dev_ci);
    if (stat_mutex_locked)
	dm2_ct_stat_mutex_unlock(ct, &stat_mutex_locked, put_uri);
 no_locks:
    if (select_disk)
	AO_fetch_and_sub1(&free_space_dev_ci->ci_select_cnt);
    return ret;
}	/* dm2_choose_disk_without_uri_ci_rl */


/*
 * Find a free block (some number of consecutive disk pages).  Update the
 * on-disk free disk page bitmap.
 *
 * Notes:
 * - uri_cont_wlocked_ret will be set if the uri_head_ret->uri_container is
 *   write locked.  This should only be the case on a successful return from
 *   this function.  In the "return 0" case, we set this value to the boolean
 *   OR of uri_cont_wlocked & cont_wlocked.  This must be done because there
 *   are potentially two different containers which must be managed here and
 *   we need to handle the case of unlocking two different containers in the
 *   error case.
 */
static int
dm2_find_free_block(MM_put_data_t    *put,
		    char	     *put_uri,
		    dm2_cache_type_t *ct,
		    DM2_uri_t	     **uri_head_ret,
		    int		     *uri_cont_wlocked_ret,
		    DM2_extent_t     **append_ext,
		    dm2_cache_info_t **bad_ci_ret)
{
    dm2_bitmap_t         *bm = NULL;
    dm2_bitmap_header_t  *bm_header;
    dm2_disk_block_t     *blk_map = NULL;
    DM2_uri_t            *uri_head = *uri_head_ret;
    dm2_cache_info_t     *free_space_dev_ci;
    int                  valid_cont_found, ret, same_block, no_share;
    int                  enough_space, unaligned;
    int                  uri_head_rlocked, bm_locked, pagesize;
    int			 macro_ret;
    int			 uri_cont_wlocked, uri_disk_block = 0;

    valid_cont_found = ret = same_block = enough_space = unaligned = 0;
    bm_locked = uri_head_rlocked = 0;
    no_share = uri_cont_wlocked = 0;

    if (uri_head)
	NKN_ASSERT(uri_head->uri_lock_type == DM2_URI_WLOCKED);

    /*
     * Do not share the block (use existing block or allow others to use
     *	a new block) if:
     *
     * 1. If uri_head is found, then this is not the first PUT.
     * 2. If this is the first PUT and
     *    a. complete object is equal to or larger than 2MB.
     *    b. complete object is smaller than 2MB and this PUT is
     *       smaller than the complete object.
     * 3. If this is not the first PUT and optimal sharing is required
     *    share the block
     */
    if ((uri_head && !ct->ct_share_block_threshold) ||
	(put->attr && !nkn_attr_is_streaming(put->attr) &&
	 ((put->attr->content_length >= NKN_MAX_BLOCK_SZ) ||
	  (put->attr->content_length > (uint64_t)put->uol.length))))
	    no_share = 1;

    if (uri_head && uri_head->uri_container) {
	/* The ci should have been rlocked in 'find_obj_in_cache' if
	 * it was already present */
	ret = dm2_choose_disk_with_uri(put, put_uri, ct, uri_head,
				       &same_block, append_ext,
				       &uri_cont_wlocked);
	if (ret)
	    goto no_locks;
	free_space_dev_ci = uri_head->uri_container->c_dev_ci;
    } else {
	ret = dm2_choose_disk_without_uri_ci_rl(put, put_uri, ct, &uri_head,
						&same_block, &free_space_dev_ci,
						&uri_cont_wlocked);
	if (ret)
	    goto no_locks;
	*uri_head_ret = uri_head;
    }
    NKN_ASSERT(uri_head != NULL);
    /* We should always have a uri_head and a container now */

    if (uri_head->uri_open_dblk)
	uri_disk_block = 1;

    ret = dm2_check_disk_life(free_space_dev_ci);
    switch (ret) {
	case DM2_DISK_NO_THROTTLE:
	    ret = 0;
	    break;
	case DM2_DISK_SOFT_THROTTLE:
	    /* Dont allow any fresh ingests other than crawl ingests */
	    if (put->attr && !(put->flags & MM_FLAG_CIM_INGEST)) {
		ret = -EPERM;
		glob_dm2_put_soft_reject_cnt++;
		free_space_dev_ci->ci_soft_rejects++;
		DBG_DM2W("[cache_type:%s] Disk : %s reached soft throttle "
			 "limit. Rejecting PUT for uri %s", ct->ct_name,
			 free_space_dev_ci->mgmt_name, put_uri);
		goto unlock_uri;
	    }
	    break;
	case DM2_DISK_HARD_THROTTLE:
	    /* Dont allow any ingests */
	    ret = -EPERM;
	    glob_dm2_put_hard_reject_cnt++;
	    free_space_dev_ci->ci_hard_rejects++;
	    DBG_DM2W("[cache_type:%s] Disk : %s reached hard throttle "
		     "limit. Rejecting PUT for uri %s", ct->ct_name,
		     free_space_dev_ci->mgmt_name, put_uri);
	    goto unlock_uri;
	default:
	    assert(0);
	    break;
    }

    /*
     * If we are trying to use the leftovers from a previous open block,
     * we can just return here because we haven't changed any freelists.
     */
    if (same_block) {
	goto find_free_block_done;
    }

    bm = &free_space_dev_ci->bm;
    PTHREAD_MUTEX_LOCK(&bm->bm_mutex);
    bm_locked = 1;
    bm_header = (dm2_bitmap_header_t *)bm->bm_buf;
    pagesize = bm_header->u.bmh_disk_pagesize;

    /*
     * Get a block from free block list if available; otherwise, go to
     * the free page list
     */
    NKN_ASSERT((uri_head->uri_container->c_rflags & DM2_CONT_RWFLAG_WLOCK)!= 0);
    if (bm->bm_free_blk_list) {
	/* Free block list has single blocks */
	ret = dm2_get_block_from_free_blk_list(bm, uri_head, pagesize,
					       put->ns_stoken, ct->ct_ptype,
					       uri_disk_block, &blk_map);
	if (ret < 0) {
	    glob_dm2_put_free_block_alloc_fail_err++;
	    goto unlock_uri;
	}
	DBG_DM2M3("[cache_type=%s] Using pagenum=%ld for %s", ct->ct_name,
		  blk_map->db_start_page, free_space_dev_ci->mgmt_name);
    } else {
	ret = -ENOSPC;
	goto unlock_uri;
    }
    if (no_share)
	blk_map->db_flags |= DM2_DB_NOSHARE;

    if ((ret = dm2_update_bitmap(bm, blk_map, 1, free_space_dev_ci)) < 0) {
	DBG_DM2S("[cache_type=%s] Failure updating bitmap: %d",
		 ct->ct_name, ret);
	if (ret == -EIO)
	    *bad_ci_ret = free_space_dev_ci;
	goto unlock_uri;
    }

    PTHREAD_MUTEX_UNLOCK(&bm->bm_mutex);
 find_free_block_done:
    *uri_cont_wlocked_ret = uri_cont_wlocked;
    return 0;

 unlock_uri:
    if (uri_cont_wlocked)
	dm2_cont_rwlock_wunlock(uri_head->uri_container,
				&uri_cont_wlocked,
				uri_head->uri_name);
    if (bm && bm_locked)
	PTHREAD_MUTEX_UNLOCK(&bm->bm_mutex);

 no_locks:
    glob_dm2_put_find_free_block_err++;
    assert(ret != 0);
    return ret;
}	/* dm2_find_free_block */

static void
dm2_modify_disk_extent(const MM_put_data_t       *put,
		       const DM2_uri_t		 *uri_head,
		       const off_t		 partial_length,
		       const uint32_t		 *ext_v2_checksum,
			     int		 csum_idx,
			     int		 num_idx,
		       const DM2_extent_t	 *old_ext,
		       void			 *dext_in)
{
    uint32_t		objsz;
    DM2_disk_extent_v2_t *dext_v2 = NULL;
    int			 i;

    objsz = uri_head->uri_container->c_obj_sz;
    memset(dext_in, 0, objsz);
    NKN_ASSERT(objsz == DEV_BSIZE);	// Protect against mem corruption

    dext_v2 = (DM2_disk_extent_v2_t *)dext_in;
    if (dm2_perform_put_chksum) {
	/* Technically 0 is a valid checksum */
	dext_v2->dext_header.dext_flags |= DM2_DISK_EXT_FLAG_CHKSUM;
	for (i = 0; i < 8; i++)
	    dext_v2->dext_ext_checksum[i] =old_ext->ext_csum.ext_v2_checksum[i];
	for (i = csum_idx; i < (csum_idx + num_idx); i++)
	    dext_v2->dext_ext_checksum[i] = ext_v2_checksum[i];
    }
    dext_v2->dext_header.dext_magic = DM2_DISK_EXT_MAGIC;
    dext_v2->dext_header.dext_version = DM2_DISK_EXT_VERSION_V2;
    dext_v2->dext_length = old_ext->ext_length + partial_length;
    if (partial_length != put->uol.length)
	DBG_DM2M("DM2_put: wrlen=%lu  uollen=%lu",
		 partial_length, put->uol.length);
    dext_v2->dext_offset = old_ext->ext_offset;
    dext_v2->dext_start_sector = old_ext->ext_start_sector;// byte to sector #
    /*
     * Since the start sector can be zero, add 1 to the dev_id; so we don't
     * get zero physids.
     */
    dext_v2->dext_physid = old_ext->ext_physid;
    dext_v2->dext_start_attr = 0;
    strcpy(dext_v2->dext_basename, dm2_uh_uri_basename(uri_head));
    /*
     * Since we have done a memset over the entire sector, we should
     * perform our checksum over the entire sector because this will
     * allow us to checksum an older (potentially smaller) version of
     * this object.
     */
#if 0
    /* For now, we don't checksum this because it is only a sector */
    dext_v2->dext_my_checksum =
	do_csum64_aligned_aligned((unsigned char *)&dext_v2->dext_magic,
				  objsz - sizeof(dext_v2->dext_my_checksum));
#endif

    return;
}   /* dm2_modify_disk_extent */

/*
 * Create an on-disk extent
 */
static void
dm2_create_disk_extent(const MM_put_data_t	 *put,
		       const DM2_uri_t		 *uri_head,
		       const off_t		 raw_offset,
		       const off_t		 put_offset,
		       const off_t		 partial_length,
		       const uint32_t		 *ext_v2_checksum,
		       const nkn_provider_type_t ptype,
		       void			 *dext_in)
{
    uint32_t		blksz;
    uint32_t		objsz;
    dm2_cache_info_t	*dev_ci = uri_head->uri_container->c_dev_ci;
    DM2_disk_extent_v2_t *dext_v2 = NULL;
    int			 i;

    objsz = uri_head->uri_container->c_obj_sz;
    memset(dext_in, 0, objsz);
    NKN_ASSERT(objsz == DEV_BSIZE);	// Protect against mem corruption

    dext_v2 = (DM2_disk_extent_v2_t *)dext_in;
    if (dm2_perform_put_chksum) {
	/* Technically 0 is a valid checksum */
	dext_v2->dext_header.dext_flags |= DM2_DISK_EXT_FLAG_CHKSUM;
	for (i = 0; i < 8; i++)
	    dext_v2->dext_ext_checksum[i] = ext_v2_checksum[i];
    }
    dext_v2->dext_header.dext_magic = DM2_DISK_EXT_MAGIC;
    dext_v2->dext_header.dext_version = DM2_DISK_EXT_VERSION_V2;
    dext_v2->dext_length = partial_length;
    if (partial_length != put->uol.length)
	DBG_DM2M("DM2_put: wrlen=%lu  uollen=%lu",
		 partial_length, put->uol.length);
    /* Need to add put_offset to handle case of single put crossing blocks */
    dext_v2->dext_offset = put->uol.offset + put_offset;
    dext_v2->dext_start_sector = NKN_Byte2S(raw_offset);// byte to sector #
    /*
     * Since the start sector can be zero, add 1 o the dev_id; so we don't
     * get zero physids.
     */
    blksz = uri_head->uri_container->c_blksz;
    /* XXXmiken: the ext_physid field is probably not needed.  For now,
     * I'm only doing a check on DM2_get */
    dext_v2->dext_physid =
	nkn_physid_create(ptype, dev_ci->list_index,
			  NKN_SECTOR_TO_BLOCK(dext_v2->dext_start_sector,
			  blksz));
    dext_v2->dext_start_attr = 0;
    strcpy(dext_v2->dext_basename, dm2_uh_uri_basename(uri_head));
    /*
     * Since we have done a memset over the entire sector, we should
     * perform our checksum over the entire sector because this will
     * allow us to checksum an older (potentially smaller) version of
     * this object.
     */
#if 0
    /* For now, we don't checksum this because it is only a sector */
    dext_v2->dext_my_checksum =
	do_csum64_aligned_aligned((unsigned char *)&dext_v2->dext_magic,
				  objsz - sizeof(dext_v2->dext_my_checksum));
#endif

    return;
}	/* dm2_create_disk_extent */


/*
 * Given a set of bits that will change in the bitmap, check if there are
 * entire free blocks now within that range.  If a bit is zero, it is not
 * used.  Hence, a value of 0 indicates a completely empty block or 64
 * consecutive free bits/pages.
 */
static void
dm2_update_free_list(dm2_bitmap_t	*bm,
		     dm2_disk_block_t	*blk_map,
		     char		*mgmt_name,
		     int		*freed_blocks)
{
    dm2_bitmap_header_t *header;
    off_t		byte0, byteN, block0, blockN, i;
    uint64_t		*bitbuf;
    unsigned char	*free_blk_map;
    int			blk_idx;

    header = (dm2_bitmap_header_t *)bm->bm_buf;
    bitbuf = (uint64_t *)(header+1);
    byte0 = blk_map->db_start_page / NBBY;
    byteN = (blk_map->db_start_page + blk_map->db_num_pages - 1) / NBBY;
    block0 = byte0 / 8;
    blockN = byteN / 8;
    *freed_blocks = 0;
    for (i = block0; i <= blockN; i += 8) { // 8 = NumBytesPer64bits
	if (*(bitbuf+i) == (uint64_t)0) {
	    DBG_DM2M2("[disk_name=%s] Free block num: %ld", mgmt_name, i);
	    /* Point to the entry in the block bitmap */
	    free_blk_map = (unsigned char *)&bm->bm_free_blk_map[i/8];
	    blk_idx = i%8;
	    if (*free_blk_map & ( 1 << blk_idx)) {
		/* We should not be inserting duplicates into the free list
		 * as in PUT we will end up fetching the same block from the
		 * free list */
		DBG_DM2M("disk_name=%s] Duplicate block: %ld", mgmt_name, i);
		glob_dm2_duplicate_block_added_cnt++;
		/* Added counter as part of workaround for PR 906911 */
		continue;
	    }

	    /* add the block to the list and mark it as available */
	    bm->bm_free_blk_list = g_list_prepend(bm->bm_free_blk_list,
						  (void *)i);
	    *free_blk_map |= (1 << blk_idx);
	    bm->bm_num_page_free += bm->bm_pages_per_block;
	    bm->bm_num_blk_free++;
	    AO_fetch_and_add1(&bm->bm_num_resv_blk_free);
	    (*freed_blocks)++;
	}
    }
}	/* dm2_update_free_list */


static int
dm2_return_pages_to_free_list(dm2_disk_block_t	*blk_map,
			      dm2_bitmap_t	*bm,
			      off_t		max_pg __attribute((unused)),
			      dm2_cache_info_t  *ci,
			      int		*freed_blocks)
{
    int	ret = 0;

    DBG_DM2M("FREE PAGES: name=%s start=%ld end=%ld",
	     bm->bm_fname, blk_map->db_start_page,
	     blk_map->db_start_page + blk_map->db_num_pages - 1);

    if ((ret = dm2_update_bitmap(bm, blk_map, 0, ci)) < 0) {
	DBG_DM2S("FreeBitmap (%s,%s) update failed",
		 bm->bm_fname, bm->bm_devname);
	NKN_ASSERT(0);
	/*
	 * There is no point in continuing here because a failure indicates
	 * a drive issue.  Close down this cache now.
	 */
	goto error;
    }
    dm2_update_free_list(bm, blk_map, ci->mgmt_name, freed_blocks);

 error:
    return ret;
}	/* dm2_return_pages_to_free_list */


void
dm2_free_ext_list(GList *ext_list)
{
    GList *ext_obj;

    for (ext_obj = ext_list; ext_obj; ext_obj = ext_obj->next)
	dm2_free(ext_obj->data, sizeof(DM2_extent_t), DM2_NO_ALIGN);
    g_list_free(ext_list);
}	/* dm2_free_ext_list */


static int
dm2_free_uri_head_space(DM2_uri_t	 *uri_head,
			dm2_cache_type_t *ct,
			ns_stat_token_t	 ns_stoken)
{
    dm2_disk_block_t	*blk_map;
    dm2_bitmap_header_t *bmh;
    dm2_bitmap_t	*bm;
    GList		*elist;		// elist = Extent list
    DM2_extent_t	*ext;
    dm2_cache_info_t	*ci;
    off_t		max_pg;
    int			ret, macro_ret, freed_bitmap_blocks;
    int			list_ret = 0, num_free_cache_pin_pgs = 0;
    uint64_t		physid;
    DM2_physid_head_t   *physid_head;
    uint64_t		blocks_freed = 0, blocks_resvd = 0;

    blk_map = alloca(sizeof(dm2_disk_block_t));
    bm = &uri_head->uri_container->c_dev_ci->bm;
    PTHREAD_MUTEX_LOCK(&bm->bm_mutex);

    bmh = (dm2_bitmap_header_t *)bm->bm_buf;
    ci = uri_head->uri_container->c_dev_ci;
    max_pg = ci->set_cache_page_cnt;
    /* Let's skip locking the uri_ext_list because the uri_head is write
     * locked and no one can update the uri_ext_list right now except us */
    for (elist = uri_head->uri_ext_list; elist; elist = elist->next) {
	ext = (DM2_extent_t *)elist->data;

	physid = ext->ext_physid;
	memset(blk_map, 0, sizeof(blk_map));
	blk_map->db_disk_page_sz = bmh->u.bmh_disk_pagesize;
	blk_map->db_num_free_disk_pages =
	    Byte2DP_pgsz(ext->ext_length, blk_map->db_disk_page_sz);
	blk_map->db_start_page =
	    ext->ext_start_sector / (blk_map->db_disk_page_sz / DEV_BSIZE);
	if (uri_head->uri_cache_pinned)
	    num_free_cache_pin_pgs = blk_map->db_num_free_disk_pages;
	if (blk_map->db_num_free_disk_pages < bm->bm_pages_per_block) {
	    /*
	     * Check if the physid has some data left, else free the
	     * entire block. This case would happen when a block that is
	     * partially filled is deleted after a restart. As the
	     * occupancy details of an open block are stored in memory
	     * and are never flushed to disk, the partial block would
	     * thus be leaking (bug 2252). So after a restart, if
	     * we find a physid with no extents, we free the entire block.
	     */
	    physid_head = dm2_extent_get_by_physid(&physid, ct);
	    if (physid_head == NULL) {
		glob_dm2_delete_free_uri_null_physid_warn++;
		/*
		 * The only time physid_head should be NULL is when we are
		 * deleting an object which got an error during PUT.
		 * Otherwise, this is not a normal event and should not be
		 * tolerated.
		 */
	    } else if (physid_head->ph_ext_list == NULL) {
		/* Reset start_page to start_page of the block */
		blk_map->db_start_page = (nkn_physid_to_sectornum(physid) *
					 bm->bm_pages_per_block);
		blk_map->db_num_free_disk_pages = bm->bm_pages_per_block;
	    }
	}
	blk_map->db_num_pages = blk_map->db_num_free_disk_pages;
	blk_map->db_flags |= DM2_DB_ACTIVE;

	freed_bitmap_blocks = 0;
	ret = dm2_return_pages_to_free_list(blk_map, bm, max_pg, ci,
					    &freed_bitmap_blocks);
	if (ret < 0) {
	    /*
	     * There is little point in continuing here because a failure
	     * indicates a drive issue.  Close down this cache now.  By
	     * returning without unlocking the uri_head, we essentially
	     * just leak this memory.
	     */
	    list_ret = ret;
	    goto unlock;
	}
	blocks_freed++;
    }

    PTHREAD_MUTEX_UNLOCK(&bm->bm_mutex);

    if (uri_head->uri_resvd) {
	/*
	 * Check that the resvd blocks are all freed. For URIs that
	 * exceeded the reservation, all the blocks would have been freed above.
	 * URIs which were made partial due to an explicit 'end trans' would
	 * have been already freed of their unused blocks and will not fall
	 * here.
	 * URIs that are partial(no end trans) because of expiry/eviction/CLI
	 * deletion before a full ingest or may be due to a missed 'end trans'
	 * from the upper layer will need to be cleared of their reservation
	 * here.
	 */
	blocks_resvd = ROUNDUP_PWR2(uri_head->uri_content_len,
				    (uint64_t)bmh->u.bmh_disk_blocksize);
	blocks_resvd >>= bm->bm_byte2block_bitshift;

	if (blocks_freed < blocks_resvd) {
	    AO_fetch_and_add(&bm->bm_num_resv_blk_free,
			     (blocks_resvd - blocks_freed));
	    if (!ns_is_stat_token_null(ns_stoken))
		dm2_ns_sub_disk_usage(ns_stoken, ct->ct_ptype,
				      (blocks_resvd - blocks_freed), 1);
	}
	uri_head->uri_resvd = 0;
    }

    dm2_uri_tbl_steal_and_wakeup(uri_head, ct);
    dm2_uri_head_free(uri_head, ct, 1);
    return list_ret;

 unlock:
    PTHREAD_MUTEX_UNLOCK(&bm->bm_mutex);;
    return list_ret;
}	/* dm2_free_uri_head_space */


/*
 * The first extent is the good extent, but trailing extents need to be marked
 * as DONE, so they can be reused.
 */
static void
dm2_write_large_disk_extent_magic(void     *dext_ptr,
				  uint32_t size)
{
    DM2_disk_extent_header_t *dhdr = (DM2_disk_extent_header_t *)dext_ptr;
    unsigned int i;

    /* Don't zero out the first extent since it is already filled in */
    memset(((char *)dext_ptr)+DM2_DISK_EXT_SIZE, 0, size-DM2_DISK_EXT_SIZE);
    for (i=DM2_DISK_EXT_SIZE; i<size; i+=DM2_DISK_EXT_SIZE) {
	/* Only magic number matters for slot re-use */
	dhdr = (DM2_disk_extent_header_t *)(((char *)dext_ptr)+i);
	dhdr->dext_magic = DM2_DISK_EXT_DONE;
	dhdr->dext_version = DM2_DISK_EXT_VERSION_V2;
    }
}	/* dm2_write_large_disk_extent_magic */


/*
 * We just wrote out a large chunk of mostly DONE extents because the ext slot
 * Q was empty.  Hence, we can assume that the queue is still empty here.
 */
static void
dm2_add_extra_extents(dm2_container_t	   *cont,
		      uint32_t		   size,
		      off_t		   loff)
{
    unsigned int i;

    loff += DEV_BSIZE;	// Skip first extent
    for (i=DEV_BSIZE; i<size; i += DEV_BSIZE, loff += DEV_BSIZE) {
	dm2_ext_slot_push_tail(cont, loff);
    }
}	/* dm2_add_extra_extents */


/*
 * Return the size to grow this container given its current size.
 * Assume that c_cont_sz is accurate, and we don't need to issue a system call
 * to determine the size of the container file.
 */
static int
dm2_grow_container_size(dm2_container_t *cont)
{
    int size = 512;

    /* SSDs are not IOP limited */
    if (cont->c_dev_ci->ci_cttype->ct_ptype == SolidStateMgr_provider)
	return DM2_DISK_EXT_SIZE;

    NKN_ASSERT(cont->c_cont_sz >= 8*1024);
    if (cont->c_cont_sz == 8*1024)	//* only container head is written
	size = 4*1024;
    if (cont->c_cont_sz == 12*1024)	// (8+4)*1024
	size = 8*1024;
    if (cont->c_cont_sz == 20*1024)	// (12+8)*1024
	size = 16*1024;
    if (cont->c_cont_sz >= 36*1024)	// (20+16)*1024
	size = 32*1024;
    NKN_ASSERT(size <= NKN_CONTAINER_ALLOC_CHUNK_SZ);
    return size;
}	/* dm2_grow_container_size */


static int
dm2_write_disk_extent(DM2_uri_t		*uri_head,
		      void		*dext,
		      DM2_extent_t	*ext)
{
    dm2_container_t *cont = uri_head->uri_container;
    dm2_cache_info_t *ci  = cont->c_dev_ci;
    dm2_cache_type_t *ct  = ci->ci_cttype;
    char	    *name = cont->c_dev_ci->mgmt_name;
    char	    *contpath;
    off_t	    loff;
    int		    write_len, nbytes, cfd, new_extent = 1;
    int		    ret = 0, appending = 0;

    NKN_ASSERT(cont->c_rwlock.__data.__writer == gettid());
    DM2_GET_CONTPATH(cont, contpath);

    if ((cfd = dm2_open(contpath, O_WRONLY, 0)) == -1) {
	ret = -errno;
	NKN_ASSERT(ret != -EMFILE);
	DBG_DM2S("Container open failed (%s): %d", contpath, ret);
	glob_dm2_container_open_err++;
	goto error_unlock;
    }
    nkn_mark_fd(cfd, DM2_FD);
    glob_dm2_container_open_cnt++;

    if (ext->ext_cont_off == DM2_EXT_INIT_CONT_OFF) {
	if ((loff = dm2_ext_slot_pop_head(cont)) == -1) {
	    /* Appending to container; grow by chunk to reduce fragmentation */
	    loff = cont->c_cont_sz;
	    appending = 1;
	    write_len = dm2_grow_container_size(cont);
	    dm2_write_large_disk_extent_magic(dext, write_len);
	} else {
	    write_len = DM2_DISK_EXT_SIZE;
#ifdef SLOT_DEBUG
	    DBG_DM2S("Using ext slot %ld at (%s) (%s)",
		     loff, contpath, uri_head->uri_name);
#endif
	}
    } else {
	/* Appending to existing extent */
	loff = ext->ext_cont_off * DM2_DISK_EXT_SIZE;
	new_extent = 0;
	write_len = DM2_DISK_EXT_SIZE;
    }
    glob_dm2_container_write_cnt++;
    nbytes = pwrite(cfd, dext, write_len, loff);
    if (nbytes != write_len) {
	ret = -errno;
	DBG_DM2S("IO ERROR:[name=%s] [contname=%s] Incomplete extent "
		 "write @ %ld: expected=%d got=%d in fd=%d errno=%d",
		 name, contpath, loff, write_len, nbytes, cfd, ret);
	NKN_ASSERT(nbytes != -1 || ret != EBADF);
	glob_dm2_container_write_err++;
	goto error_unlock;
    }
    glob_dm2_container_write_bytes += write_len;
    dm2_inc_container_write_bytes(ct, ci, write_len);
    ext->ext_cont_off = NKN_Byte2S(loff);	// write is done now

    if (appending) {
	cont->c_cont_sz += write_len;
	cont->c_num_exts += write_len / DM2_DISK_EXT_SIZE;
	dm2_add_extra_extents(cont, write_len, loff);
    }
    if (new_extent)
	AO_fetch_and_add1(&cont->c_good_ext_cnt);

 error_unlock:
    if (cfd != -1) {
	int ret2;
	glob_dm2_container_close_cnt++;
	if ((ret2 = nkn_close_fd(cfd, DM2_FD)) != 0) {
	    DBG_DM2S("[contname=%s] Close error for container file: %d",
		     contpath, errno);
	    glob_dm2_container_close_err++;
	}
    }
    return ret;
}	/* dm2_write_disk_extent */


static int
dm2_delete_disk_extent(DM2_uri_t    *uri_head,
		       void	    *dext,
		       DM2_extent_t *ext)
{
    dm2_container_t *cont = uri_head->uri_container;
    DM2_disk_extent_header_t *dhdr = (DM2_disk_extent_header_t *)dext;
    char	    *name = cont->c_dev_ci->mgmt_name;
    char	    *contpath;
    int		    nbytes, cfd, ret = 0;

    NKN_ASSERT(cont->c_rwlock.__data.__writer == gettid());

    DM2_GET_CONTPATH(cont, contpath);

    if ((cfd = dm2_open(contpath, O_WRONLY, 0)) == -1) {
	ret = -errno;
	NKN_ASSERT(ret != -EMFILE);
	DBG_DM2S("Container open failed (%s): %d", contpath, ret);
	glob_dm2_container_open_err++;
	goto error_unlock;
    }
    nkn_mark_fd(cfd, DM2_FD);
    glob_dm2_container_open_cnt++;

    glob_dm2_container_write_cnt++;
    dhdr->dext_magic = DM2_DISK_EXT_DONE;	// Indicate DONE
    nbytes = pwrite(cfd, dext, DM2_DISK_EXT_SIZE,
		    (off_t)ext->ext_cont_off * DM2_DISK_EXT_SIZE);
    if (nbytes != DM2_DISK_EXT_SIZE) {
	ret = -errno;
	DBG_DM2S("IO ERROR:[name=%s] [contname=%s] Incomplete extent "
                 "write @ %d: expected=512 got=%d n fd=%d errno=%d",
                 name, contpath,
		 ext->ext_cont_off * DM2_DISK_EXT_SIZE, nbytes, cfd, ret);
	NKN_ASSERT(nbytes != -1 || ret != EBADF);
	glob_dm2_container_write_err++;
	goto error_unlock;
    }
    glob_dm2_container_write_bytes += DM2_DISK_EXT_SIZE;
    AO_fetch_and_sub1(&cont->c_good_ext_cnt);

 error_unlock:
    if (cfd != -1) {
	int ret2;
	glob_dm2_container_close_cnt++;
	if ((ret2 = nkn_close_fd(cfd, DM2_FD)) != 0) {
	    DBG_DM2S("[contname=%s] Close error for container file: %d",
		     contpath, errno);
	    glob_dm2_container_close_err++;
	}
    }
    return ret;
}	/* dm2_delete_disk_extent */


static int
dm2_create_mem_extent(const void *dext_in,
		      DM2_extent_t **ext_ret)
{
    DM2_extent_t *ext;
    DM2_disk_extent_v2_t *dext_v2 = NULL;
    int i;

    if (*ext_ret == NULL) {
	if ((ext = dm2_calloc(1, sizeof(DM2_extent_t),
			      mod_DM2_extent_t)) == NULL) {
	    DBG_DM2S("Calloc failed: %zd", sizeof(DM2_extent_t));
	    return -ENOMEM;
	}
    } else {
	ext = *ext_ret;
    }

    dext_v2 = (DM2_disk_extent_v2_t *)dext_in;
    ext->ext_offset = dext_v2->dext_offset;
    ext->ext_length = dext_v2->dext_length;
    ext->ext_physid = dext_v2->dext_physid;		// 64-bits
    for (i = 0; i < 8; i++)
	ext->ext_csum.ext_v2_checksum[i] = dext_v2->dext_ext_checksum[i];
    ext->ext_start_sector = dext_v2->dext_start_sector;
    ext->ext_start_attr = 0;	// XXXmiken: not done yet
    ext->ext_flags |= DM2_EXT_FLAG_INUSE;
    ext->ext_flags |= (dext_v2->dext_header.dext_flags &
		       DM2_DISK_EXT_FLAG_CHKSUM) ? DM2_EXT_FLAG_CHKSUM : 0;
    ext->ext_cont_off = DM2_EXT_INIT_CONT_OFF;
    ext->ext_version = DM2_DISK_EXT_VERSION_V2;

    *ext_ret = ext;
    return 0;
}	/* dm2_create_mem_extent */


static void
dm2_modify_mem_extent(const void *dext_in,
		      DM2_extent_t **ext_ret)
{
    DM2_extent_t *ext = *ext_ret;
    DM2_disk_extent_v2_t *dext_v2 = NULL;
    int i;

    dext_v2 = (DM2_disk_extent_v2_t *)dext_in;
    ext->ext_offset = dext_v2->dext_offset;
    ext->ext_length = dext_v2->dext_length;
    ext->ext_physid = dext_v2->dext_physid;		// 64-bits
    for (i = 0; i < 8; i++)
	ext->ext_csum.ext_v2_checksum[i] = dext_v2->dext_ext_checksum[i];
    ext->ext_start_sector = dext_v2->dext_start_sector;
    ext->ext_start_attr = 0;	// XXXmiken: not done yet
    ext->ext_flags |= DM2_EXT_FLAG_INUSE;
    ext->ext_flags |= (dext_v2->dext_header.dext_flags &
		       DM2_DISK_EXT_FLAG_CHKSUM) ? DM2_EXT_FLAG_CHKSUM : 0;

    *ext_ret = ext;
    return;
}	/* dm2_modify_mem_extent */

static void
dm2_insert_mem_extent(DM2_uri_t *uri_head,
		      DM2_extent_t *in_ext)
{
    GList *ext_list;

    ext_list = g_list_insert_sorted(uri_head->uri_ext_list, in_ext,
				    cmp_smallest_dm2_extent_offset_first);
    uri_head->uri_ext_list = ext_list;
}	/* dm2_insert_mem_extent */


static void
dm2_delete_mem_extent(DM2_uri_t *uri_head,
		      DM2_extent_t *in_ext)
{
    NKN_ASSERT(g_list_find(uri_head->uri_ext_list, in_ext));
    uri_head->uri_ext_list = g_list_remove(uri_head->uri_ext_list, in_ext);
}	/* dm2_delete_mem_extent */


static int
dm2_grow_attribute_size(off_t seek_off,
			int *num_chunks)
{
    /*
     * If the file was truncated somehow, we add only one attribute
     * at a time until we get to one of the known sizes
     */
    *num_chunks = 1;

    if (seek_off == 0)				// Empty file
	*num_chunks = 2;
    if (seek_off == 2*DM2_ATTR_TOT_DISK_SIZE)	// 2 attrs
	*num_chunks = 4;
    if (seek_off == 6*DM2_ATTR_TOT_DISK_SIZE)	// 2+4 attrs
	*num_chunks = 8;
    if (seek_off >= 14*DM2_ATTR_TOT_DISK_SIZE)	// 6+8 attrs
	*num_chunks = 16;

    NKN_ASSERT(*num_chunks <= DM2_ATTR_MAX_CHUNKS);
    return *num_chunks * DM2_ATTR_TOT_DISK_SIZE;
}	/* dm2_grow_attribute_size */


static int
dm2_write_attr(DM2_uri_t     *uh,
	       MM_put_data_t *put)
{
    char		 *attr_pathname, *bufptr;
    off_t		 seek_off, at_off, new_seek_off;
    size_t		 at_len;
    dm2_container_t	 *cont = uh->uri_container;
    dm2_cache_info_t	 *ci = cont->c_dev_ci;
    dm2_cache_type_t	 *ct = ci->ci_cttype;
    DM2_disk_attr_desc_t *dad = NULL, *dead_dad;
    struct iovec	 iov[2];
    int			 afd = -1, nbytes = 0, ret = 0, appending = 0;
    int			 mutex_locked = 0, write_len, i, num_chunks;
    int			 alloc_bytes = 0;

    at_len = uh->uri_at_len;
    at_off = uh->uri_at_off;

    /* If attr already written, nothing to do */
    if (put->attr && (put->flags & MM_FLAG_DM2_ATTR_WRITTEN))
	return 0;

    if (put->attr && ((at_off != 0 && at_off != DM2_ATTR_INIT_OFF) ||
		      (at_len != 0 && at_len != DM2_ATTR_INIT_LEN))) {
	/*
	 * The caller of DM2_put has already passed an nkn_attr_t object
	 * with this URI.
	 */
	DBG_DM2S("Error: URI (%s) already has attributes set: "
		 "old_off=%ld old_len=%ld", uh->uri_name, at_off, at_len);
	glob_dm2_put_dm2_write_attr_dup++;
	return -EINVAL;
    }
    if (put->attr == NULL)	/* Nothing to write */
	return 0;

    NKN_ASSERT(((uint64_t)put->attr % DEV_BSIZE) == 0);
    NKN_ASSERT((am_decode_update_time(&put->attr->hotval) != 0) ||
	       (put->attr->hotval == 0));
    if (put->attr->hotval == 0) {
	/* Most attributes are empty.  Writing a time will ensure that BM
	 * doesn't need to update the attribute immediately. */
	AM_init_hotness(&put->attr->hotval);
    }
    if (am_decode_update_time(&put->attr->hotval) == 0)
	DBG_DM2S("Illegal value for hotval: 0x%lx", put->attr->hotval);
    NKN_ASSERT(am_decode_update_time(&put->attr->hotval) != 0);

    alloc_bytes = DM2_ATTR_MAX_CHUNKS * DM2_ATTR_TOT_DISK_SIZE;
    if (dm2_posix_memalign((void *)&dad, DEV_BSIZE, alloc_bytes,
			   mod_DM2_disk_attr_desc_t)){
	DBG_DM2S("Memalign failed: %u", alloc_bytes);
	return -ENOMEM;
    }
    memset(dad, 0, alloc_bytes);

    dm2_attr_mutex_lock(cont);
    mutex_locked = 1;
    DM2_GET_ATTRPATH(cont, attr_pathname);

    if ((ret = dm2_open_attrpath(attr_pathname)) < 0) {
	DBG_DM2S("[name=%s] Failed to create attribute file (%s): %d",
		 ci->mgmt_name, attr_pathname, errno);
	goto free_buf;
    }
    afd = ret;
    ret = 0;

    if ((seek_off = dm2_attr_slot_pop_head(cont)) == -1) {
	if ((seek_off = lseek(afd, 0, SEEK_END)) < 0) {
	    ret = -errno;
	    DBG_DM2S("[name=%s] Failed to lseek to end of file (%s): %d",
		     ci->mgmt_name, attr_pathname, errno);
	    goto free_buf;
	}
	/* Truncate to attribute aligned length */
	if ((seek_off % DM2_ATTR_TOT_DISK_SIZE) != 0) {
	    glob_dm2_put_write_attr_trunc_cnt++;
	    new_seek_off = ROUNDDOWN(seek_off, DM2_ATTR_TOT_DISK_SIZE);
	    if ((ret = ftruncate(afd, new_seek_off)) != 0) {
		ret = -errno;
		DBG_DM2S("[name=%s] Failed to truncate file (%s): %d",
			 ci->mgmt_name, attr_pathname, errno);
		glob_dm2_put_write_attr_trunc_err++;
		goto free_buf;
	    }
	    if ((seek_off = lseek(afd, 0, SEEK_END)) < 0) {
		ret = -errno;
		DBG_DM2S("[name=%s] Failed to lseek to new end of file (%s): %d",
			 ci->mgmt_name, attr_pathname, ret);
		goto free_buf;
	    }
	    assert(seek_off == new_seek_off);
	}
	appending = 1;
    } else {
#ifdef SLOT_DEBUG
	DBG_DM2S("Using attr slot %ld at (%s)(%s)",
		 seek_off, attr_pathname, uh->uri_name);
#endif
	if ((new_seek_off = lseek(afd, seek_off, SEEK_SET)) == -1) {
	    ret = -errno;
	    DBG_DM2S("[name=%s] Failed to lseek to %ld (%s): %d",
		     ci->mgmt_name, seek_off, attr_pathname, ret);
	    goto free_buf;
	}
	NKN_ASSERT(new_seek_off == seek_off);
    }
    NKN_ASSERT(seek_off % DM2_ATTR_TOT_DISK_SIZE == 0);

    uh->uri_at_off = NKN_Byte2S(seek_off);
    uh->uri_at_len = put->attr->total_attrsize;
    uh->uri_blob_at_len = put->attr->blob_attrsize;
//    NKN_ASSERT(put->attr->total_attrsize == (size_t)DM2_MAX_ATTR_DISK_SIZE);
    dad->dat_magic = DM2_ATTR_MAGIC;
    dad->dat_version = DM2_ATTR_VERSION;
    strncpy(dad->dat_basename, dm2_uh_uri_basename(uh), NAME_MAX);
    dad->dat_basename[NAME_MAX] = '\0';

    /* Calculate checksum */
    put->attr->na_checksum = 0;
    put->attr->na_checksum =
	do_csum64_iterate_aligned((uint8_t *)put->attr,
				  put->attr->total_attrsize, 0);

    glob_dm2_attr_write_cnt++;
    if (appending) {
	/* Setup possible extra GONE entries */
	memcpy(((char *)dad)+DEV_BSIZE, put->attr, DM2_MAX_ATTR_DISK_SIZE);
	write_len = dm2_grow_attribute_size(seek_off, &num_chunks);
	bufptr = ((char *)dad) + DM2_ATTR_TOT_DISK_SIZE;
	for (i=1; i<num_chunks; i++) {
	    dead_dad = (DM2_disk_attr_desc_t *)bufptr;
	    dead_dad->dat_magic = DM2_ATTR_MAGIC_GONE;
	    dead_dad->dat_version = DM2_ATTR_VERSION;
	    bufptr += DM2_ATTR_TOT_DISK_SIZE;
	}
	nbytes = write(afd, dad, write_len);
    } else {
	/* This skips the above bcopy */
	write_len = DM2_ATTR_TOT_DISK_SIZE;
	iov[0].iov_base = dad;
	iov[0].iov_len = DEV_BSIZE;
	iov[1].iov_base = put->attr;
	iov[1].iov_len = DM2_MAX_ATTR_DISK_SIZE;

	nbytes = writev(afd, iov, 2);
    }
    if (nbytes == -1) {
	ret = -errno;
	DBG_DM2S("[name=%s] [attrfname=%s] Attribute write failed for URI "
		 "%s: %d", ci->mgmt_name, attr_pathname, uh->uri_name,-ret);
	glob_dm2_attr_write_err++;
	NKN_ASSERT(ret != -EBADF);
	/* ENOSPC means no data was written */
	goto free_buf;
    }
    if (nbytes != write_len) {
	DBG_DM2S("[name=%s] [attrfname=%s] Short write of attribute for URI "
		 "%s: %d", ci->mgmt_name, attr_pathname, uh->uri_name, nbytes);
	glob_dm2_attr_write_err++;
	glob_dm2_attr_write_bytes += nbytes;
	ret = -EINVAL;
	goto truncate_file;
    }

    glob_dm2_attr_write_bytes += write_len;
    dm2_inc_attribute_write_bytes(ct, ci, write_len);
    if (appending) {
	for (i=1; i<num_chunks; i++) {
	    dm2_attr_slot_push_tail(cont, seek_off +(i*DM2_ATTR_TOT_DISK_SIZE));
	}
    }

    /* writing the file means that the contents are already cached */
    cont->c_rflags |= DM2_CONT_RFLAG_ATTRS_READ;

 free_buf:
    if (afd != -1) {
	glob_dm2_attr_close_cnt++;
	nkn_close_fd(afd, DM2_FD);
    }
    if (mutex_locked)
	dm2_attr_mutex_unlock(cont);
    if (dad)
	dm2_free(dad, alloc_bytes, DEV_BSIZE);
    return ret;

 truncate_file:
    /*
     * We have a partially written attribute.  This must be the case of
     * attribute file extension because overwriting previously written
     * data should never produce a short write.  Overwriting data could
     * only produce EIO.
     */
    if (ftruncate(afd, seek_off) != 0) {
	DBG_DM2S("[name=%s] Failed to truncate file (%s) to %ld: %d",
		 ci->mgmt_name, attr_pathname, seek_off, errno);
	glob_dm2_put_write_attr_trunc_err++;
    }
    goto free_buf;
}	/* dm2_write_attr */


/*
 * Find a consecutive run of free pages in the given block.  It is OK to not
 * fulfull the entire request.
 */
static int
dm2_get_pages(dm2_disk_block_t	*db,
	      off_t		partial_len,
	      off_t		*page_ret,
	      uint32_t		*numpages_ret,
	      char		*str __attribute((unused)))
{
    uint8_t  *byteptr;
    off_t    page;
    uint32_t full_pages, numpages;
    int      b, B;
    uint8_t  bits8;

    page = numpages = 0;
    if (! DM2_DISK_BLOCK_INITED(db)) {
	/* Not initialized yet, may be we are racing on a block transition */
	glob_dm2_put_dm2_get_pages_warn++;
	return -EAGAIN;
    }

    dm2_db_mutex_lock(db);
    full_pages = roundup(partial_len, db->db_disk_page_sz)/db->db_disk_page_sz;
    NKN_ASSERT(db->db_num_pages % 8 == 0);

    page = db->db_start_page;
    byteptr = &db->db_used[0];
    for (B = 0; B < (int)db->db_num_pages/8; byteptr++, B++) {
	bits8 = *(uint8_t *)byteptr;
	if (bits8 == 0xff) {
	    page += 8;
	    continue;
	}

	for (b = 0; b < 8; b++) {
	    if ((bits8 & 0x1) == 1) {
		if (numpages > 0)	// free bits must be continuous
		    break;
		/* page used; goto next one */
		page++;
		bits8 >>= 1;
	    } else
		break;
	}
	if (b == 8)
	    continue;

	for (; b < 8; b++) {
	    if (((bits8 & 0x1) == 0) && (numpages < full_pages)) {
		numpages++;
		*byteptr |= (1 << b);
		bits8 >>= 1;
		db->db_num_free_disk_pages--;
	    } else {
		break;
	    }
	}
	/* save updates and move to next byte */
	if (numpages >= full_pages)
	    break;
    }
#if 0
    DBG_DM2W("BLOCK: pagestart=%zd freepages=%d disk=%s",
	     db->db_start_page, db->db_num_free_disk_pages, str);

    if (numpages < full_pages)
	DBG_DM2W("Did not return full pages at pageoff=%zd: needed=%d got=%d",
		 page, full_pages, numpages);
#endif
    *page_ret = page;
    *numpages_ret = numpages;

    if (numpages == 0) {
	glob_dm2_put_dm2_get_pages_warn++;
	dm2_db_mutex_unlock(db);
	return -EAGAIN;
    }
    dm2_db_mutex_unlock(db);
    return 0;
}	/* dm2_get_pages */


/*
 * Only check for metadata full condition every 16 PUT calls.  If we hit
 * ENOSPC, then check every PUT call until we delete space.  Hopefully, we
 * should never hit this condition because we are now deleting empty
 * container and attribute files.
 *
 * We could feasibly check all PUTs every 16 requests if we return -ENOSPC
 * when ci->no_meta is true inbetween checks.
 */
int
dm2_check_meta_fs_space(dm2_cache_info_t  *ci)
{
    struct statfs    stbuf;
    int		     ret;

    if (!ci->no_meta && (ci->ci_dm2_put_statfs_chk_cnt++ & 0xF) != 0)
	return 0;

    if (statfs(ci->ci_mountpt, &stbuf) < 0) {
	ret = errno;
	DBG_DM2S("[MNTPT=%s] statfs error: %d", ci->ci_mountpt, errno);
	return -ret;
    }
    ci->ci_dm2_free_meta_blks_cnt = stbuf.f_bfree;

    /*
     * Ensure that we dont fill up the meta fs.Leave 20MB worth of blocks
     * free in ext3 f/s.
     */
    if (stbuf.f_bfree && (stbuf.f_bfree < DM2_META_FS_THRESHOLD)) {
	glob_dm2_put_no_meta_space_err++;
	ci->ci_dm2_put_no_meta_space_err++;
	ci->no_meta = 1;
	return -ENOSPC;
    }
    ci->no_meta = 0;
    return 0;
}	/* dm2_check_meta_fs_space */


/*
 * There is an iovec field in the MM_put_data_t object.  Assume that the
 * lengths of each iovec are sector aligned or the direct I/O write will
 * fail.
 *
 * uri_head should be readlocked.
 */
static int
dm2_write_content(MM_put_data_t	   *put,
		  dm2_cache_type_t *ct,
		  off_t		   *put_offset,
		  DM2_uri_t	   *uri_head,
		  DM2_extent_t	   *append_ext)
{
    void		*dext_space;
    void		*dext = 0;
    DM2_extent_t	*ext = 0;
    dm2_disk_block_t	*db;
    struct iovec	*myiov;
    dm2_cache_info_t	*dev_ci;
    char		*chk_base_addr;
    off_t		page, partial_len, raw_offset, raw_length, off;
    off_t		write_length, done, useful_len;
    uint32_t		numpages, ext_v2_checksum[8];
    int			nbytes, ret, ret2, i, vec, nvecs, raw_fd = -1;
    int			chk_idx = 0, chk_len = 0, block_size;
    int			chk_size, chk_iov_len, calc_chk_size, chk_base;
    int			num_csum = 0;
    uint64_t		uri_resv_len, uri_content_len;

    dev_ci = uri_head->uri_container->c_dev_ci;
    block_size = dm2_get_blocksize(dev_ci);
    if (uri_head->uri_open_dblk)
	db = uri_head->uri_open_dblk;
    else
	db = &uri_head->uri_container->c_open_dblk;

    NKN_ASSERT((db->db_flags & DM2_DB_ACTIVE) == DM2_DB_ACTIVE);
    for (i = 0; i < put->iov_data_len; i++) {
	NKN_ASSERT(((uint64_t)put->iov_data[i].iov_base & (4096-1)) == 0);
    }

    if ((ret = dm2_check_meta_fs_space(dev_ci)) < 0) {
	/* Not enough space left in metadata f/s, so reject operation */
	goto close_fd;
    }

    /* Get aligned space from stack: Since write is 512 bytes allocate it. */
    dext_space = alloca(NKN_CONTAINER_ALLOC_CHUNK_SZ + DEV_BSIZE);
    dext = (DM2_disk_extent_t *)roundup(((uint64_t)dext_space), DEV_BSIZE);

    // Make a local copy of the request iovec with the remaining length
    myiov = alloca(sizeof(struct iovec) * put->iov_data_len);
    done = *put_offset;
    // Copy the first partial entry starting at the remaining length
    for (i = 0; i < put->iov_data_len; i++) {
	if (put->iov_data[i].iov_len > done) {
	    myiov[0].iov_len = put->iov_data[i].iov_len - done;
	    myiov[0].iov_base = put->iov_data[i].iov_base + done;
	    partial_len = myiov[0].iov_len;
	    break;
	} else
	    done -= put->iov_data[i].iov_len;
    }
    assert(i < put->iov_data_len);
    i++;
    // Copy remaining entries
    for (vec=1; i < put->iov_data_len; i++, vec++) {
	myiov[vec].iov_base = put->iov_data[i].iov_base;
	myiov[vec].iov_len = put->iov_data[i].iov_len;
	partial_len += myiov[vec].iov_len;
    }
    nvecs = vec;

    NKN_ASSERT(((off_t)myiov[0].iov_base & 0xfff) == 0);
    // aligned for direct i/o
    NKN_ASSERT(partial_len > 0);
    glob_dm2_raw_open_cnt++;
    if ((raw_fd = dm2_open(dev_ci->raw_devname, O_WRONLY, 0)) == -1) {
	ret = -errno;
	NKN_ASSERT(ret != -EMFILE);
	DBG_DM2S("Open failed for raw %s: %d", dev_ci->raw_devname, ret);
	glob_dm2_raw_open_err++;
	goto close_fd;
    }
    nkn_mark_fd(raw_fd, DM2_FD);
    ret = dm2_get_pages(db, partial_len, &page, &numpages, dev_ci->mgmt_name);
    if (ret < 0) {
	/*
	 * what to do?  Get new block.  There are no free pages which means
	 * that we should have caught this problem in an ealier routine.
	 * If multiple PUT threads race and think the open block has enough
	 * pages to satisfy their request, they can all race and hand out the
	 * same disk pages because we don't grab a lock.
	 */
	DBG_DM2S("[uri=%s] num free blocks=%ld  num resv free blocks=%ld "
		 "partial_len=%ld ret=%d", uri_head->uri_name,
		 dev_ci->bm.bm_num_blk_free,
		 dev_ci->bm.bm_num_resv_blk_free, partial_len, ret);
	NKN_ASSERT(0);
	glob_dm2_put_dm2_get_pages_err++;
	goto close_fd;
    } else {
#if 0
	DBG_DM2W("[uri=%s] got space page=%ld len=%d",
		 uri_head->uri_name, page, numpages);
#endif
    }
    raw_offset = page * db->db_disk_page_sz;
    raw_length = numpages * db->db_disk_page_sz;

    /* Write data */
    off = lseek(raw_fd, raw_offset, SEEK_SET);
    if (off != raw_offset) {
	DBG_DM2S("lseek failed: expected=%lu got=%lu/%d",
		 raw_offset, off, errno);
	NKN_ASSERT(off == raw_offset);
	ret = -EIO;
	goto close_fd;
    }

    /* Cannot write more than # pages returned */
    /* Truncate the iovec if the # pages is less than we need */
    if (raw_length < partial_len) {
	off_t delta = partial_len - raw_length;

	i = nvecs-1;
	while (delta) {
	    if (myiov[i].iov_len > (size_t)delta) {
		myiov[i].iov_len -= delta;
		break;
	    }
	    delta -= myiov[i].iov_len;
	    assert(i);
	    i--;
	}
	nvecs = i+1;
	partial_len = raw_length;
    }
    write_length = roundup(partial_len, 4096);
    myiov[nvecs-1].iov_len += (write_length - partial_len);
    useful_len = partial_len;

    /* Calculate checksum */
    if (dm2_perform_put_chksum) {
	chk_size = (block_size / DM2_SMALL_READ_DIV);
	if (append_ext)
	    chk_idx = (page - db->db_start_page)/DM2_SMALL_READ_DIV;
	else
	    chk_idx = 0;
	chk_len = 0;
	memset(ext_v2_checksum, 0, 8 * sizeof(uint32_t));
	if (nvecs == 1) {
	    chk_iov_len = myiov[0].iov_len;
	    chk_base = 0;
	    while (chk_iov_len > 0) {
		if (chk_iov_len > chk_size)
		    calc_chk_size = chk_size;
		else
		    calc_chk_size = chk_iov_len;
		chk_base_addr = (char *)myiov[0].iov_base + chk_base;
		ext_v2_checksum[chk_idx] =
		    do_csum32_iterate_aligned((void *)chk_base_addr,
					      calc_chk_size,
					      ext_v2_checksum[chk_idx]);
		chk_iov_len -= calc_chk_size;
		chk_base += calc_chk_size;
		chk_len += calc_chk_size;
		if (chk_len >= chk_size) {
		    chk_idx++;
		    num_csum++;
		    chk_len = 0;
		}
	    }
	} else {
	    for (i = 0; i <= nvecs-1; i++) {
		NKN_ASSERT(((uint64_t)myiov[i].iov_len & (512-1)) == 0);
		ext_v2_checksum[chk_idx] =
		    do_csum32_iterate_aligned(myiov[i].iov_base,
					      myiov[i].iov_len,
					      ext_v2_checksum[chk_idx]);
		chk_len += myiov[i].iov_len;
		if (chk_len >= chk_size) {
		    chk_idx++;
		    num_csum++;
		    chk_len = 0;
		}
	    }
	}
    }

    dm2_inc_raw_write_cnt(ct, dev_ci, 1);
    nbytes = writev(raw_fd, myiov, nvecs);

    /* XXXmiken: write should only fail with EIO and should never be short */
    if (nbytes == -1) {
	ret = -errno;
	DBG_DM2S("[name=%s] RAW WRITE failed at %zd: %d",
		 dev_ci->mgmt_name, raw_offset, errno);
	glob_dm2_raw_write_err++;
	NKN_ASSERT(nbytes == write_length);
	goto close_fd;
    } else if (nbytes != write_length) {
	/* This should never happen */
	DBG_DM2S("[name=%s] RAW SHORT WRITE at %zd",
		 dev_ci->mgmt_name, raw_offset);
	dm2_inc_raw_write_bytes(ct, dev_ci, nbytes);
	glob_dm2_raw_write_err++;
	NKN_ASSERT(nbytes == write_length);
	ret = -EIO;
	goto close_fd;
    } else {
	dm2_inc_raw_write_bytes(ct, dev_ci, nbytes);
	/* Account the disk usage for this PUT interms of number of pages */
	put->namespace_actual_usage = raw_length;
	if (uri_head->uri_cache_pinned)
	    put->cache_pin_actual_usage += raw_length;
    }
    NKN_ASSERT(nbytes == write_length);

    /*
     * The raw data is now written to the raw partition.
     */
    if (!append_ext) {
	/* Update disk extents */
	dm2_create_disk_extent(put, uri_head, raw_offset, *put_offset,
			       partial_len, ext_v2_checksum,
			       ct->ct_ptype, dext);
	/*
	 * Failure to create memory extents will mean we can't access the data.
	 * However, the data on disk will still be consistent.
	 */
	if ((ret = dm2_create_mem_extent(dext, &ext)) < 0) {
	    glob_dm2_put_create_mem_ext_err++;
	    goto close_fd;
	}
	/* ext->ext_cont_off still not set */

	dm2_insert_mem_extent(uri_head, ext);	// can't fail

	ret = dm2_write_disk_extent(uri_head, dext, ext);
	if (ret < 0)	// Leave as 2 lines, so can set ret value
	    goto remove_mem_extent;
	/*
	 * At this point when we get an error, we don't free ext so we can
	 * delete the disk extent on a delete.  By leaving the insert ext into
	 * physid hash after this point, we insure that a failure when writing
	 * the disk extent will cause only a delete of the object
	 *
	 * Insert into physid -> extent hash table
	 */
	ext->ext_uri_head = uri_head;
	ext->ext_read_size =
	    block_size / (DM2_SMALL_READ_DIV * DM2_EXT_RDSZ_MULTIPLIER);
	ret = dm2_extent_insert_by_physid(&ext->ext_physid, ext, ct);
	if (ret < 0) {		// Leave as 2 lines, so can set ret value
	    /* Only error is ENOMEM */
	    goto delete_disk_extent;
	}
    } else {
	/* Create the new disk extent with the latest uol details */
	dm2_modify_disk_extent(put, uri_head, partial_len,
			       ext_v2_checksum, (chk_idx - num_csum),
			       num_csum, append_ext, dext);

	/* Write the extent at the same offset as before. If the write
	 * fails, we should update the memory extent so as to keep the
	 * previous extent intact */
	ret = dm2_write_disk_extent(uri_head, dext, append_ext);
	if (ret < 0)
	    goto close_fd;

	/* Modify the meory extent since the disk write passed */
	dm2_modify_mem_extent(dext, &append_ext);

	glob_dm2_put_ext_append_cnt++;
	dev_ci->ci_dm2_ext_append_cnt++;
    }

    partial_len -= nbytes;
    *put_offset += nbytes;

    if (put->attr) {
	/* Write out attributes & update cache pinning stats */
	ret = dm2_write_attr(uri_head, put);
	if (ret < 0)
	    goto remove_ext_from_physid_tbl;
	/* No errors past this point, so no need to ever delete the attr */

	/* Copy the object version from the PUT->attr */
	memcpy(&uri_head->uri_version, &put->attr->obj_version,
	       sizeof(uri_head->uri_version));
    }

#if 0
    printf("URI=%s resv_len=%d len=%ld rawoffset=%ld uol_off=%ld uol_len=%ld\n",
	   uri_head->uri_name, uri_head->uri_resv_len,
	   useful_len, raw_offset, put->uol.offset, put->uol.length);
#endif
    uri_head->uri_resv_len -= useful_len;
    if ((int64_t)uri_head->uri_resv_len < 0) {
	uri_resv_len = uri_head->uri_resv_len;
	uri_content_len = uri_head->uri_content_len;
	DBG_DM2S("[cache_type=%s] Too much data for URI=%s, resv_len=%ld "
		 "contlen=%ld",
		 ct->ct_name, uri_head->uri_name, uri_resv_len,
		 uri_content_len);
    }

    /* 'ext' stays linked to the URI, dext is on stack */
    if (raw_fd != -1) {
	if ((ret2 = nkn_close_fd(raw_fd, DM2_FD)) != 0) {
	    DBG_DM2S("[cache_type=%s] Close error for raw device(%s): %d",
		     ct->ct_name, dev_ci->raw_devname, errno);
	    glob_dm2_raw_close_err++;
	}
	glob_dm2_raw_close_cnt++;
    }
    return 0;		// SUCCESS

 remove_ext_from_physid_tbl:
    if ((ret2 = dm2_physid_tbl_ext_remove(&ext->ext_physid, ext,
					  ct, NULL)) != 0) {
	DBG_DM2S("[cache_name=%s][uri=%s] Can not find physid: 0x%lx (ret=%d)",
		 dev_ci->mgmt_name, uri_head->uri_name, ext->ext_physid, ret2);
	glob_dm2_put_ext_physid_remove_err++;
	assert(0);	// Catch memory corruption
    }

 delete_disk_extent:
    if ((ret2 = dm2_delete_disk_extent(uri_head, (void *)dext, ext)) != 0) {
	DBG_DM2S("[cache_name=%s][uri=%s] Failed to mark disk extent as unused"
		 " during error handling (ret=%d)",
		 dev_ci->mgmt_name, uri_head->uri_name, ret2);
	if (ret2 == -EMFILE) {
	    uri_head->uri_bad_delete = 1;
	    NKN_ASSERT(0);
	    goto remove_mem_extent;
	}
	if (ret2 != -EIO)
	    assert(0);	// How can this happen?
	else
	    ret = -EIO;
	glob_dm2_put_del_disk_ext_err++;
    }

 remove_mem_extent:
    dm2_delete_mem_extent(uri_head, ext);
    if (ext) {
	glob_dm2_put_remove_mem_ext_err++;
	ext->ext_csum.ext_checksum = 0xdeadbabe;
	dm2_free(ext, sizeof(*ext), DM2_NO_ALIGN);
    }

 close_fd:
    if (raw_fd != -1) {
	if ((ret2 = nkn_close_fd(raw_fd, DM2_FD)) != 0) {
	    DBG_DM2S("[cache_type=%s] Close error for raw device(%s): %d",
		     ct->ct_name, dev_ci->raw_devname, errno);
	    glob_dm2_raw_close_err++;
	}
	glob_dm2_raw_close_cnt++;
    }
    glob_dm2_put_write_content_err++;
    return ret;
}	/* dm2_write_content */


static int
dm2_do_read_block(const int	   raw_fd,
		  const off_t	   in_seek_offset,
		  struct iovec	   *iov,
		  const int	   in_iovcnt,
		  dm2_cache_info_t *ci,
		  dm2_cache_type_t *ct)
{
    off_t nbytes;
    off_t totbytes = 0;
    int   i, ret = 0;

    for (i = 0; i < in_iovcnt; i++)
	totbytes += iov[i].iov_len;

    glob_dm2_raw_seek_cnt++;
    nbytes = lseek(raw_fd, in_seek_offset, SEEK_SET);
    if (nbytes == in_seek_offset) {
	dm2_inc_raw_read_cnt(ct, ci, 1);
	nbytes = readv(raw_fd, iov, in_iovcnt);
	if (nbytes == totbytes) {
	    dm2_inc_raw_read_bytes(ct, ci, nbytes);
	} else if (nbytes < 0) {
	    ret = -EIO;
	    glob_dm2_raw_read_err++;
	    DBG_DM2S("[name=%s] RAW READ at %ld (expected bytes=%ld) ERROR: %d",
		     ci->mgmt_name, in_seek_offset, totbytes, errno);
	    goto read_error;
	} else {
	    glob_dm2_raw_read_err++;
	    dm2_inc_raw_read_bytes(ct, ci, nbytes);
	    DBG_DM2S("[name=%s] RAW SHORT READ at %ld: expected=%ld got=%ld",
		     ci->mgmt_name, in_seek_offset, totbytes, nbytes);
	    ret = -EIO;
	    goto read_error;
	}
    } else if (nbytes == -1) {
	ret = -EIO;
	DBG_DM2S("[name=%s] RAW SEEK (expected bytes=%ld) ERROR: %d",
		 ci->mgmt_name, in_seek_offset, errno);
	glob_dm2_raw_seek_err++;
	goto seek_error;
    } else {
	ret = -EIO;
	DBG_DM2S("[name=%s] RAW SHORT SEEK: expected=%ld got=%ld",
		 ci->mgmt_name, in_seek_offset, nbytes);
	glob_dm2_raw_seek_err++;
	goto seek_error;
    }
    return 0;

 read_error:
 seek_error:
    return ret;
}	/* dm2_do_read_block */


static int
dm2_read_block(MM_get_resp_t	*get,
	       DM2_uri_t	*uri_head,
	       DM2_extent_t     *seek_ext,
	       int		read_full_ext,
	       dm2_cache_type_t *ct,
	       struct iovec     *iov,
	       int iov_cnt )
{
    dm2_cache_info_t *ci;
    off_t	 seek_offset;
    off_t	 sector_offset = 0;//offset into the block to start reading
    uint32_t	 blksz, read_size;
    int		 ret, raw_fd;

    blksz = uri_head->uri_container->c_blksz;
    NKN_ASSERT(blksz != 0);
    if (seek_ext) {
	/* since we need to just read a single URI, the offset/length should
	 * exactly be set to that particular extent */
	if (read_full_ext) {
	    seek_offset = seek_ext->ext_start_sector * DEV_BSIZE;
	} else {
	    /* Calculate the read offset aligning to the start of the
	     * chunk from which the checksum can be verified */
	    if ((get->in_uol.offset - seek_ext->ext_offset) > 0) {
		/* If the extent length is not greater than the read_size,
		 * we just have a single checksum chunk and the caclculation
		 * below might give a negative offset. So if the extent length
		 * is not greater than read-size, then 'sector_offset = 0'
		 */
		read_size = seek_ext->ext_read_size * DM2_EXT_RDSZ_MULTIPLIER;
		if (seek_ext->ext_length > read_size) {
		    sector_offset = get->in_uol.offset - seek_ext->ext_offset;
		    if (sector_offset % read_size)
			sector_offset = (sector_offset/read_size) * read_size;
		}
	    }

	    /* We cant have a negative value for sector_offset */
	    if (sector_offset < 0)
		NKN_ASSERT(0);

	    seek_offset = (seek_ext->ext_start_sector * DEV_BSIZE) +
			   sector_offset;
	}
    } else {
	seek_offset = nkn_physid_to_byteoff(get->physid2, blksz);
    }

    glob_dm2_raw_open_cnt++;
    ci = uri_head->uri_container->c_dev_ci;
    /* need to have private fd because do_read_block does lseek */
    if ((raw_fd = dm2_open(ci->raw_devname, O_RDONLY, 0)) == -1) {
	ret = -errno;
	NKN_ASSERT(ret != -EMFILE);
	DBG_DM2S("Failed to open raw devname for cache (%s): %d",
		 ci->raw_devname, ret);
	ci->valid_cache = 0;
	glob_dm2_raw_open_err++;
	return ret;
    }
    nkn_mark_fd(raw_fd, DM2_FD);
    ci->valid_cache = 1;
    ret = dm2_do_read_block(raw_fd, seek_offset, iov, iov_cnt,
			    uri_head->uri_container->c_dev_ci, ct);
    glob_dm2_raw_close_cnt++;
    nkn_close_fd(raw_fd, DM2_FD);
    return ret;
}	/* dm2_read_block */


static int
dm2_invalid_slash(const char *uri)
{
    int len;
    char *uri_cp;

    len = strlen(uri);
    NKN_ASSERT(len < PATH_MAX);
    if (uri[len-1] == '/') {
	uri_cp = alloca(len+1);
	strcpy(uri_cp, uri);
	uri_cp[len-1] = '\0';
	if (strrchr(uri_cp, '/') == uri_cp)
	    return 1;
    }
    return 0;
}	/* dm2_invalid_slash */


/*
 * Verify the contents of the PUT request.
 *
 * Most of these checks are fast and are done here, so the rest of the code
 * can assume good input.  Eventually, the strstr calls may need to be removed.
 *
 * When (end_trans != 0), nothing but the cod and flag are valid.
 */
static int
dm2_put_check(const MM_put_data_t *put,
	      char **uri_ret)
{
    char *uri;
    int end_trans = put->flags & MM_FLAG_DM2_END_PUT;
    int ret = 0;

    if (put->uol.cod == NKN_COD_NULL) {
	DBG_DM2S("Invalid PUT UOL cod: NULL");
	glob_dm2_put_null_cod_err++;
	ret = -EINVAL;
	goto dm2_put_check_error;
    }
    uri = (char *)nkn_cod_get_cnp(put->uol.cod);
    if (uri == NULL || *uri == '\0') {
	DBG_DM2S("URI not found from cod: NULL");
	glob_dm2_put_null_uri_err++;
	ret = -NKN_DM2_BAD_URI;
	goto dm2_put_check_error;
    }
    if (uri[0] != '/') {
	DBG_DM2S("[URI=%s] Illegal URI from cod: uri[0]=%d", uri, uri[0]);
	glob_dm2_put_no_lead_slash_err++;
	ret = -NKN_DM2_BAD_URI;
	goto dm2_put_check_error;
    }
    if (dm2_invalid_slash(uri)) {
	DBG_DM2M2("[URI=%s] Illegal URI from cod", uri);
	glob_dm2_put_invalid_slash_err++;
	ret = -NKN_DM2_BAD_URI;
	goto dm2_put_check_error;
    }

    if (end_trans) {
	/* Only cod and flag are valid */
	goto uri_checks;
    }

    if (put->attr && (put->flags & MM_FLAG_DM2_ATTR_WRITTEN)) {
	DBG_DM2S("[URI=%s] Illegal put flag: %x",
		 uri, put->flags);
	glob_dm2_put_unexpected_attr_err++;
	ret = -NKN_DM2_INTERNAL_ISSUE;
	goto dm2_put_check_error;
    }

    if (put->attr && ((uint64_t)put->attr % DEV_BSIZE != 0)) {
	DBG_DM2S("[URI=%s] Illegal attribute buffer: %p",
		 uri, put->attr);
	glob_dm2_put_unaligned_attr_err++;
	ret = -NKN_DM2_INTERNAL_ISSUE;
	goto dm2_put_check_error;
    }
    if (put->attr && (put->attr->total_attrsize % 8 != 0)) {
	DBG_DM2S("[URI=%s] Illegal attribute size: %d",
		 uri, put->attr->total_attrsize);
	glob_dm2_put_invalid_attrsize_err++;
	ret = -NKN_DM2_INVALID_ATTR_SIZE;
	goto dm2_put_check_error;
    }
    if (put->attr && put->attr->content_length <= 0 &&
	    !nkn_attr_is_streaming(put->attr)) {
	DBG_DM2S("[URI=%s] Valid attr object but invalid content_length", uri);
	glob_dm2_put_invalid_content_len_err++;
	ret = -NKN_DM2_INVALID_URI_CONT_LEN;
	goto dm2_put_check_error;
    }
    if (put->uol.length == 0) {
	/* This is legal, but then there is nothing to do */
	DBG_DM2S("[URI=%s] Data length=%zd.  Nothing to write",
		 uri, put->uol.length);
	glob_dm2_put_zero_len_err++;
	NKN_ASSERT(put->uol.length != 0);
	return 0;
    }
    if ((put->uol.offset % CM_MEM_PAGE_SIZE) != 0) {
	DBG_DM2S("[URI=%s] Invalid PUT request: uol_off=0x%lx",
		 uri, put->uol.offset);
	glob_dm2_put_invalid_offset_err++;
	NKN_ASSERT((put->uol.offset & CM_MEM_PAGE_SIZE) == 0);
	ret = -NKN_DM2_INVALID_URI_OFFSET;
	goto dm2_put_check_error;
    }

    if (put->attr && !nkn_attr_is_streaming(put->attr) &&
	((uint64_t)put->uol.offset + put->uol.length >
					put->attr->content_length)) {
	DBG_DM2S("[URI=%s] Mismatched UOL request + attributes: uol.off=%ld "
		 "uol.len=%ld  attr.cont_len=%ld", uri,
		 put->uol.offset, put->uol.length, put->attr->content_length);
	glob_dm2_put_mismatched_len_err++;
	return -EINVAL;
    }

    if (put->attr && put->attr->total_attrsize > DM2_MAX_ATTR_DISK_SIZE) {
	DBG_DM2S("[URI=%s] Invalid total attribute length=%d/%d",
		 uri, put->attr->total_attrsize, put->attr->blob_attrsize);
	DBG_CACHELOG("[URI=%s] Invalid total attribute length=%d/%d",
		     uri, put->attr->total_attrsize, put->attr->blob_attrsize);
	glob_dm2_put_invalid_total_attr_size_err++;
	ret = -NKN_DM2_ATTR_TOO_LARGE;
	goto dm2_put_check_error;
    }

    if (put->attr &&
	put->attr->cache_expiry != NKN_EXPIRY_FOREVER &&
	put->attr->cache_expiry < nkn_cur_ts) {

	/* Ignore expiry during PUT. This is done to
	 * cache the max-age=0 or no-cache objects
	 */
	if (!(put->flags & MM_PUT_IGNORE_EXPIRY)) {

	    if (put->attr->cache_expiry + 10*60 < nkn_cur_ts) {
		DBG_DM2S("[URI=%s] Attempting to write object which has already "
			"expired: expiry=%ld now=%ld",
			uri, put->attr->cache_expiry, nkn_cur_ts);
	    } else {
		DBG_DM2M3("[URI=%s] Attempting to write object which has already "
			"expired: expiry=%ld now=%ld",
			uri, put->attr->cache_expiry, nkn_cur_ts);
	    }
	    glob_dm2_put_expired_err++;
	    ret = -NKN_DM2_URI_EXPIRED;
	    goto dm2_put_check_error;
	} else {
	    glob_dm2_put_expired_cnt++;
	}
    }

    if (put->attr && NKN_OBJV_EMPTY(&put->attr->obj_version)) {
	DBG_DM2S("[URI=%s] Attempting to write object with no version", uri);
	glob_dm2_put_empty_version_err++;
	ret = -NKN_DM2_WRONG_URI_VERSION;
	goto dm2_put_check_error;
    }

 uri_checks:
    /*
     * don't allow URIs with "container" or "attributes-2" as a directory name
     */
    if (strstr(uri, "/./") || strstr(uri, "/../") ||
		strstr(uri, "/container/") || strstr(uri, "/attributes-2/")) {
	DBG_DM2M2("[URI=%s] Illegal component in URI", uri);
	glob_dm2_put_illegal_component_err++;
	ret = -NKN_DM2_BAD_URI;
	goto dm2_put_check_error;
    }

    if (strlen(dm2_uri_basename((char *)uri)) > NAME_MAX) {
	DBG_DM2S("[URI=%s] Basename too long", uri);
	glob_dm2_put_basename_too_long_err++;
	ret = -NKN_DM2_BAD_URI;
	goto dm2_put_check_error;
    }
    *uri_ret = (char *)uri;
    return 0;

 dm2_put_check_error:
    return ret;
}	/* dm2_put_check */


int
dm2_common_delete_uol(dm2_cache_type_t *ct,
		       nkn_cod_t        cod,
		       int		ptype,
		       int		sub_ptype,
		       char		*uri,
		       dm2_cache_info_t **bad_ci_ret,
		       int		delete_type)
{
    MM_delete_resp_t	del_request;
    int			ret = 0;

    memset(&del_request, 0, sizeof(del_request));
    del_request.in_ptype = ptype;
    del_request.in_sub_ptype = sub_ptype;
    del_request.in_uol.cod = nkn_cod_dup(cod, NKN_COD_DISK_MGR);
    if (del_request.in_uol.cod == NKN_COD_NULL) {
	DBG_DM2S("[cache_type=%s] COD allocation failed (URI=%s)",
		     ct->ct_name, uri);
	glob_dm2_cod_null_cnt++;
	glob_dm2_api_server_busy_err++;
	return -NKN_SERVER_BUSY;
    }
    del_request.dm2_no_vers_check = 0;
    del_request.dm2_local_delete = 1;
    if (delete_type == DM2_DELETE_EXPIRED_UOL) {
	glob_dm2_expired_cnt++;
	del_request.dm2_is_mgmt_locked = 1;	// PUT already grabs this lock
	del_request.dm2_del_reason = NKN_DEL_REASON_EXPIRED;
    } else {
	glob_dm2_attr_delete_big_obj_cnt++;
	del_request.dm2_is_mgmt_locked = 0;	// UP_ATTR doesn't grab lock
	del_request.dm2_del_reason = NKN_DEL_REASON_ATTR2BIG;
    }
    ret = DM2_type_delete(&del_request, ct, bad_ci_ret);
    nkn_cod_close(del_request.in_uol.cod, NKN_COD_DISK_MGR);
    return ret;
}	/* dm2_common_delete_uol */

int
dm2_uri_end_puts(char		  *put_uri,
		 dm2_cache_type_t *ct,
		 ns_stat_token_t  ns_stoken)
{
    DM2_uri_t		 *uri_head = NULL;
    dm2_container_head_t *cont_head = NULL;
    int			 ch_locked = DM2_CONT_HEAD_UNLOCKED;
    dm2_bitmap_t	 *bm = NULL;
    dm2_bitmap_header_t  *bm_header = NULL;
    uint64_t		 resvd_blks, blocks_used, resv_len;

    uri_head = dm2_uri_head_get_by_uri_wlocked(put_uri, cont_head, &ch_locked,
					       ct, 0, DM2_RLOCK_CI);
    if (uri_head == NULL)
	return -ENOENT;

    if (uri_head->uri_resvd == 0) {
	DBG_DM2W("[cache_type=%s] Duplicate end trans for URI %s",
		 ct->ct_name, put_uri);
	goto dm2_uri_end_puts_unlock;
    }

    bm_header =
	(dm2_bitmap_header_t *)uri_head->uri_container->c_dev_ci->bm.bm_buf;
    bm = (dm2_bitmap_t *)&uri_head->uri_container->c_dev_ci->bm;

    resv_len = uri_head->uri_content_len;

    /* If the object is streaming type, we would have allocated blocks for
     * DM2_STREAMING_OBJ_CONTENT_LEN and that needs to be accounted */
    if (uri_head->uri_obj_type & NKN_OBJ_STREAMING) {
	if (resv_len <= DM2_STREAMING_OBJ_CONTENT_LEN)
	    resv_len = DM2_STREAMING_OBJ_CONTENT_LEN;
	else
	    resv_len = ROUNDUP_PWR2(resv_len, DM2_STREAMING_OBJ_CONTENT_LEN);
    }

    resvd_blks = ROUNDUP_PWR2(resv_len,
		    (uint64_t)bm_header->u.bmh_disk_blocksize);
    resvd_blks >>= bm->bm_byte2block_bitshift;

    blocks_used = dm2_uri_physid_cnt(uri_head->uri_ext_list);

    /* Free up the unused blocks that were reserved earlier */
    if (blocks_used < resvd_blks) {
        AO_fetch_and_add(&bm->bm_num_resv_blk_free, (resvd_blks - blocks_used));
	dm2_ns_sub_disk_usage(ns_stoken, ct->ct_ptype,
			      (resvd_blks - blocks_used), 1);

	DBG_DM2M("[cache_type=%s] Freeing up %ld blocks for partial URI (%s)",
		 ct->ct_name, (resvd_blks - blocks_used), put_uri);
    }

    /* If this is a streaming object, reduce the streaming curr_cnt */
    if (uri_head->uri_obj_type & NKN_OBJ_STREAMING) {
	AO_fetch_and_sub1(&ct->ct_dm2_streaming_curr_cnt);
	uri_head->uri_obj_type &= ~NKN_OBJ_STREAMING;
    }

    /* Cleanup the open disk block and free up the memory for the same */
    dm2_put_cleanup_uri_disk_block(uri_head);

    dm2_cache_log_add(NULL, put_uri, uri_head, ct);
    /* Mark that there is no reservation for this URI */
    uri_head->uri_resvd = 0;
    ct->ct_dm2_end_put_cnt++;

 dm2_uri_end_puts_unlock:
    dm2_uri_head_rwlock_unlock(uri_head, ct, DM2_RUNLOCK_CI);
    return 0;

}   /* dm2_uri_end_puts */


static void
dm2_put_err_uri_cleanup(DM2_uri_t	 *uri_head,
			int		 *cont_wlocked,
			const char	 *put_uri,
			dm2_cache_type_t *ct)
{
    dm2_container_t *cont = NULL;
    dm2_cache_info_t *ci = NULL;
    /*
     * This is the first PUT for this URI and we did not succeed with the
     * PUT.  Since we now free containers, we cannot let this uri_head
     * remain in the hash table.  If the failed PUT already had
     * successfully PUT an extent, then we don't need to do the cleanup
     */
    if (uri_head == NULL)
	return;

    /* Addedndum: When there is no disk space, we would not have any
     * containers that are locked. Commenting the NKN_ASSERT below as it is
     * possible to have this function called without locking a container
     */
    // NKN_ASSERT(*cont_wlocked);

    cont = uri_head->uri_container;
    ci = cont->c_dev_ci;

    /* Free up the open disk block and the memory for the same
     * as the PUT failed. We do not know if the PUT would continue */
    if (uri_head->uri_open_dblk)
	dm2_put_cleanup_uri_disk_block(uri_head);

    if (uri_head->uri_ext_list == NULL) {
	uri_head->uri_initing = 0;	// debug
	uri_head->uri_put_err = 1;	// debug
	dm2_uri_tbl_steal_and_wakeup(uri_head, ct);
	if (*cont_wlocked)
	    dm2_cont_rwlock_wunlock(cont, cont_wlocked, put_uri);
	dm2_ci_dev_rwlock_runlock(ci);
	/*
	 * After destroying the uri_head rwlock, anyone waiting on that
	 * lock will be woken up.  Lookups will return ENOENT from the
	 * lookup
	 */
	dm2_uri_head_free(uri_head, ct, 1);
    } else {
	/* unlock the uri_head, as it is not freed */
	if (*cont_wlocked)
	    dm2_cont_rwlock_wunlock(cont, cont_wlocked, put_uri);
	dm2_uri_head_rwlock_unlock(uri_head, ct, DM2_RUNLOCK_CI);
    }
}	/* dm2_put_err_uri_cleanup */


/*
 * Perform active checks
 */
static int
dm2_put_active_checks(dm2_cache_type_t *ct,
		      MM_put_data_t *put,
		      char *put_uri)
{
    char ns_uuid[NKN_MAX_UID_LENGTH];
    uint64_t max_pin_bytes;
    ns_stat_token_t ns_stoken;
    int  ret, enabled;

    put->ns_stoken = NS_NULL_STAT_TOKEN;
    if ((ret = dm2_uridir_to_nsuuid(put_uri, ns_uuid)) != 0)
	return ret;
    strncpy(put->ns_uuid, ns_uuid, NKN_MAX_UID_LENGTH);	// Save away

    /* Not needed here but be optimistic that we won't encounter errors */
    ns_stoken = dm2_ns_get_stat_token_from_uri_dir(put_uri);
    if (ns_is_stat_token_null(ns_stoken)) {
	DBG_DM2S("[URI=%s] Cannot get stat token", put_uri);
	return -NKN_STAT_NAMESPACE_EINVAL;
    }
    put->ns_stoken = ns_stoken;

    if (ct->ct_ptype != glob_dm2_lowest_tier_ptype)
	return 0;

    if (put->attr && nkn_attr_is_cache_pin(put->attr)) {
	if (put->flags & MM_FLAG_DM2_PROMOTE) {
	   DBG_DM2M("[cache_type=%s] Reset cache pin, promote for URI:%s",
		    ct->ct_name, put_uri);
	   glob_dm2_put_cache_pin_reset_promote_cnt++;
	   nkn_attr_reset_cache_pin(put->attr);
	}
    }
    /*
     * Clear cache pin bit if the size of this object is greater than
     * the maximum allowed for this namespace.
     */
    put->cache_pin_enabled = DM2_CACHE_PIN_INITIALIZER;
    if (put->attr && nkn_attr_is_cache_pin(put->attr)) {
	ret = ns_stat_get_cache_pin_enabled_state(put->ns_uuid, &enabled);
	if (ret != 0)
	    return -ret;

	put->cache_pin_enabled = enabled;
	if (enabled == 0) {	/* cache pinning not enabled */
	    /* Reset cache pin bit as we cannot pin this object */
	    glob_dm2_put_cache_pin_reset_pin_disabled_cnt++;
	    nkn_attr_reset_cache_pin(put->attr);
	    return 0;
	}

	ret = ns_stat_get_cache_pin_max_obj_size(put->ns_uuid, &max_pin_bytes);
	if (ret != 0)
	    return -ret;

	/* if max_pin_bytes == 0, then there is no maximum set */
	if (max_pin_bytes != 0 && (max_pin_bytes < put->attr->content_length)) {
	    DBG_DM2W("[URI=%s] Attempt ingest without pinned attribute", put_uri);
	    glob_dm2_put_cache_pin_reset_size_cnt++;
	    nkn_attr_reset_cache_pin(put->attr);
	}
    }
    return 0;
}	/* dm2_put_active_checks */

/*
 * Put data into the cache
 *
 * if (data is already in cache)
 *	find out current location
 * else
 *	get free space
 * Put data at location
 */
static int
DM2_type_put(MM_put_data_t    *put,
	     video_types_t    video_type __attribute((unused)),
	     dm2_cache_type_t *ct,
	     dm2_cache_info_t **bad_ci_ret)
{
    DM2_uri_t	*uri_head = NULL;
    char	*put_uri = NULL;
    DM2_extent_t *append_ext = NULL;
    off_t	put_offset;
    int		found = 0;
    int		ret = 0;
    int		put_inc = 0;
    int		try_count = 0;
    int		cont_wlocked = 0;

    put->ns_stoken = NS_NULL_STAT_TOKEN;
    if ((ret = dm2_put_check(put, &put_uri)) < 0)	// check inputs
	goto dm2_put_check_error;

    /* Assign put->ns_stoken */
    if ((ret = dm2_put_active_checks(ct, put, put_uri)) < 0)
	goto dm2_put_check_error;

    /* Check if this PUT is to close the URI.  If so, free up the
     * unused reservation. This URI will be partial in the disk */
    if (put->flags & MM_FLAG_DM2_END_PUT) {
	if ((ret = dm2_uri_end_puts(put_uri, ct, put->ns_stoken)) < 0) {
	    glob_dm2_put_end_trans_no_obj_warn++;
	    DBG_DM2W("[cache_type=%s] End trans for obj (%s) not in cache: %d",
		     ct->ct_name, put_uri, ret);
	}
	goto dm2_put_end;
    }

 try_again:
    uri_head = NULL;
    ret = dm2_find_obj_in_cache(put, put_uri, ct, &uri_head,
				bad_ci_ret, &found);
    if (ret < 0){
	if (ret == -EAGAIN) {
	    if (try_count++ > 5) {	// There's a bug.  Just exit
		glob_dm2_put_try_again_err++;
		NKN_ASSERT(try_count < 5);
		goto dm2_put_check_error;
	    }
	    goto try_again;
	} else if (ret == -ENOCSI) {
	}
	DBG_DM2S("[cache_type=%s] Looking for obj (%s) in cache: %d",
		 ct->ct_name, put_uri, ret);
	glob_dm2_put_find_obj_in_cache_err++;
	goto dm2_put_unlock;
    }

    if (found) {
	/*
	 * We define this as a case that we do not need to handle because
	 * nvsd only ingests full objects, so this can only happen after a
	 * crash when the file manager attempts to recache a video.  When
	 * this happens, the ingest engine must be able to handle EEXIST.
	 */
	if (uri_head->uri_bad_delete) {
	    /* If the current object is bad, we can't add to it or replace it */
	    DBG_DM2W("[cache_type=%s] Cached object (%s) is bad: disallow "
		     "new ingest", ct->ct_name, put_uri);
	    glob_dm2_put_bad_uri_err++;
	    ret = -EINVAL;
	    goto dm2_put_eexist;
	} else if (found == NKN_DM2_PARTIAL_EXTENT_EEXIST) {
	    DBG_DM2M("[cache_type=%s] Object (%s) mostly already in cache: "
		     "off=%ld len=%ld", ct->ct_name, put_uri, put->uol.offset,
		     put->uol.length);
	    ret = -NKN_DM2_PARTIAL_EXTENT_EEXIST;
	    goto dm2_put_eexist;
	} else {
	    DBG_DM2M("[cache_type=%s] Object (%s) already in cache: off=%ld "
		     "len=%ld", ct->ct_name, put_uri, put->uol.offset,
		     put->uol.length);
	    glob_dm2_put_eexist_warn++;
	    ret = -EEXIST;
	    goto dm2_put_eexist;
	}
    }
    if ((ret = dm2_ns_put_calc_space_resv(put, put_uri, ct->ct_ptype)) != 0)
	goto dm2_put_unlock;

    put_offset = 0;
    while (put_offset < put->uol.length) {
	append_ext = NULL;
	ret = dm2_find_free_block(put, put_uri, ct, &uri_head,
				  &cont_wlocked, &append_ext, bad_ci_ret);
	if (ret != 0) {
	    if (ret == -EAGAIN) {
		if (try_count++ > 5) { // There's a bug.  Just exit
		    glob_dm2_put_try_again_err++;
		    goto dm2_put_unlock;
		}
		if (uri_head)
		    dm2_uri_head_rwlock_unlock(uri_head, ct, DM2_RUNLOCK_CI);
		glob_dm2_put_try_again_cnt++;
		goto try_again;
	    }
	    if (ret == -ENOCSI) {
	    } else if (ret == -EMLINK) {
		DBG_DM2E("[cache_type=%s] Could not get free block: %d "
			 "[URI=%s]", ct->ct_name, ret, put_uri);
	    } else {
		DBG_DM2S("[cache_type=%s] Could not get free block: %d "
			 "[URI=%s]", ct->ct_name, ret, put_uri);
	    }
	    goto dm2_put_unlock;
	}
	NKN_ASSERT(cont_wlocked);	// parallel ingests can race
	dm2_inc_write_cnt_on_cache_dev(uri_head->uri_container->c_dev_ci);
	DBG_DM2M("[cache_type=%s] Put URI: %s off=%lu len=%lu"
		 " cont=%s/%s%s/container 1stP=%d", ct->ct_name,
		 put_uri, put->uol.offset+put_offset,
		 put->uol.length - put_offset, NKN_DM_MNTPT,
		 uri_head->uri_container->c_dev_ci->mgmt_name,
		 uri_head->uri_container->c_uri_dir, put_inc);
	if (put_inc++ == 0)
	    dm2_inc_put_cnt(ct, uri_head->uri_container->c_dev_ci);

	/*
	 * Write the data and Meta-data
	 */
	if ((ret = dm2_write_content(put, ct, &put_offset,
				     uri_head, append_ext)) < 0) {
	    if (ret == -EIO)
		*bad_ci_ret = uri_head->uri_container->c_dev_ci;
	    dm2_dec_write_cnt_on_cache_dev(uri_head->uri_container->c_dev_ci);
	    goto dm2_put_unlock;
	}
	/* Mark that the attr is written for this PUT request */
	if (put->attr && !(put->flags & MM_FLAG_DM2_ATTR_WRITTEN))
	    put->flags |= MM_FLAG_DM2_ATTR_WRITTEN;

	dm2_dec_write_cnt_on_cache_dev(uri_head->uri_container->c_dev_ci);
	assert((AO_load(&uri_head->uri_container->c_good_ext_cnt) != 0));
	dm2_cont_rwlock_wunlock(uri_head->uri_container, &cont_wlocked,put_uri);
    }

    /* update the content length for the streaming objects */
    if (uri_head->uri_obj_type & NKN_OBJ_STREAMING) {
	uri_head->uri_content_len = put->uol.offset + put->uol.length;
	put->out_content_len = uri_head->uri_content_len;
    }

    dm2_cache_log_add(put, put_uri, uri_head, ct);
    if (put->attr)
	dm2_ns_update_total_objects(put->ns_stoken,
				   uri_head->uri_container->c_dev_ci->mgmt_name,
				   ct, 1);

    uri_head->uri_initing = 0;			// debug
    assert(uri_head->uri_container->c_dev_ci);
    assert(uri_head->uri_container->c_magicnum == NKN_CONTAINER_MAGIC);
    dm2_uri_log_state(uri_head, DM2_URI_OP_PUT, 0, 0,
		      gettid(), nkn_cur_ts);
    if (put->attr) {
        uri_head->uri_hotval = put->attr->hotval;
	uri_head->uri_orig_hotness = am_decode_hotness(&uri_head->uri_hotval);
	dm2_evict_add_uri(uri_head, am_decode_hotness(&uri_head->uri_hotval));
    }

    dm2_ns_put_calc_space_actual_usage(put, put_uri, uri_head, ct->ct_ptype);
    dm2_uri_head_rwlock_unlock(uri_head, ct, DM2_RUNLOCK_CI);
    ns_stat_free_stat_token(put->ns_stoken);
    return 0;

 dm2_put_unlock:
    if (put->attr && uri_head) {
	dm2_uri_log_state(uri_head, DM2_URI_OP_PUT, 0, ret,
			  gettid(), nkn_cur_ts);
	dm2_ns_put_calc_space_actual_usage(put, put_uri, uri_head,
					   ct->ct_ptype);
	dm2_put_err_uri_cleanup(uri_head, &cont_wlocked, put_uri, ct);
	dm2_inc_put_err(ct);
	ns_stat_free_stat_token(put->ns_stoken);
	return ret;
    }

    /* Free up the open disk block and the memory for the same
     * as the PUT failed. We do not know if the PUT would continue */
    if (uri_head && uri_head->uri_open_dblk) {
	dm2_put_cleanup_uri_disk_block(uri_head);
    }

 dm2_put_eexist:
    if (uri_head)
	dm2_uri_log_state(uri_head, DM2_URI_OP_PUT, 0, ret,
			  gettid(), nkn_cur_ts);
    if (uri_head)
	dm2_uri_head_rwlock_unlock(uri_head, ct, DM2_RUNLOCK_CI);

    if (uri_head && cont_wlocked)
	dm2_cont_rwlock_wunlock(uri_head->uri_container, &cont_wlocked,put_uri);

 dm2_put_check_error:
    dm2_inc_put_err(ct);
    if (uri_head) {
	uri_head->uri_initing = 0;	// debug
	uri_head->uri_put_err = 1;	// debug
	dm2_ns_put_calc_space_actual_usage(put, put_uri, uri_head,
					   ct->ct_ptype);
    }

 dm2_put_end:
    ns_stat_free_stat_token(put->ns_stoken);
    return ret;
}	/* DM2_type_put */


int
DM2_put(MM_put_data_t *put,
	video_types_t video_type __attribute((unused)))
{
    uint64_t	     init_ticks;
    dm2_cache_type_t *ct;
    dm2_cache_info_t *ci, *bad_ci = NULL;
    GList            *ci_list;
    uint64_t         percent_full;
    size_t           tot_avail = 0, tot_free = 0;
    int		     ret = 0, idx, dec_counter = 0;
    int		     ct_locked = 0;

    if (put == NULL) {
	DBG_DM2S("put == NULL");
	NKN_ASSERT(put);
	ret = -EINVAL;
	goto error;
    }

    if (put->sub_ptype < 0 || put->sub_ptype > glob_dm2_num_cache_types) {
	DBG_DM2S("Illegal sub_ptype: %d", put->sub_ptype);
	NKN_ASSERT(put->sub_ptype >= 0 &&
		   put->sub_ptype > glob_dm2_num_cache_types);
	ret = -EINVAL;
	goto error;
    }

    idx = dm2_cache_array_map_ret_idx(put->ptype);
    if (idx < 0) {
	DBG_DM2S("Illegal ptype: %d", put->ptype);
	ret = -EINVAL;
	goto error;
    }
    init_ticks = LATENCY_START();

    ct = &g_cache2_types[idx];
    if (ct->ct_tier_preread_done == 0) {
	/* Pre-read thread still working */
	ret = -NKN_SERVER_BUSY;
        glob_dm2_api_server_busy_err++;
	ct->ct_dm2_put_not_ready++;
	goto error;
    }
    AO_fetch_and_add1(&ct->ct_dm2_put_curr_cnt);
    dec_counter = 1;

    dm2_ct_rwlock_rlock(ct);
    ct_locked = 1;
    ret = DM2_type_put(put, video_type, ct, &bad_ci);

    dm2_ct_info_list_rwlock_rlock(ct);
    for (ci_list = ct->ct_info_list; ci_list; ci_list = ci_list->next) {
	ci = (dm2_cache_info_t *)ci_list->data;
	tot_avail += ci->set_cache_blk_cnt;
	tot_free += ci->bm.bm_num_blk_free;
    }
    dm2_ct_info_list_rwlock_runlock(ct);
    if (tot_avail > 0)
	percent_full = ((tot_avail - tot_free) * 100) / tot_avail;
    else
	percent_full = 0;
    DBG_DM2M("[cache_type=%s] percent_full=%ld  total_free=%ld  "
	     "total_available=%ld", ct->ct_name, percent_full,
	     tot_free, tot_avail);
    LATENCY_END(&ct->ct_dm2_put_latency, init_ticks);

 error:
    if (ct_locked)
	dm2_ct_rwlock_runlock(ct);
    put->errcode = (ret < 0 ? -ret : ret);
    if (ret == -EIO && bad_ci) {
	DBG_DM2S("Automatic 'Cache Disable' due to apparent drive failure: "
		 "name=%s serial_num=%s type=%s",
		 bad_ci->mgmt_name, bad_ci->serial_num,
		 bad_ci->ci_cttype->ct_name);
	dm2_mgmt_cache_disable(bad_ci);
	nvsd_disk_mgmt_force_offline(bad_ci->serial_num, bad_ci->mgmt_name);
    }
    if (dec_counter)
	AO_fetch_and_sub1(&ct->ct_dm2_put_curr_cnt);
    return ret;
}	/* DM2_put */


static int
dm2_stat_check(const nkn_uol_t *uol,
	       char **uri_ret)
{
    int ret = 0;
    char *uri = NULL;

    if (uol->cod == NKN_COD_NULL) {
	DBG_DM2S("Invalid STAT UOL cod: NULL");
	ret = -EINVAL;
	goto dm2_stat_error;
    }
    uri = (char *)nkn_cod_get_cnp(uol->cod);
    if (uri == NULL || *uri == '\0') {
	DBG_DM2S("URI not found from cod: NULL");
	ret = -EINVAL;
	goto dm2_stat_error;
    }
    if (uri[0] != '/') {
	DBG_DM2S("[URI=%s] Illegal URI from cod: uri[0]=%d", uri, uri[0]);
	ret = -EINVAL;
	goto dm2_stat_error;
    }
    if (dm2_invalid_slash(uri)) {
	DBG_DM2M2("[URI=%s] Illegal URI from cod", uri);
	ret = -ERANGE;
	goto dm2_stat_error;
    }

    if (strstr(uri, "/./") || strstr(uri, "/../")) {
	DBG_DM2M2("[URI=%s] Illegal component in URI", uri);
	ret = -EINVAL;
	goto dm2_stat_error;
    }
    *uri_ret = uri;
    return 0;

 dm2_stat_error:
    *uri_ret = uri;
    glob_dm2_stat_invalid_input_err++;
    return ret;
}	/* dm2_stat_check */


/*
 * See if any containers are still unread.  If none are unread and we had
 * a URI head hash miss, then we know that the object can not live on disk
 * if all containers are read into memory.
 */
int
dm2_quick_uri_cont_stat(char *uri,
			dm2_cache_type_t *ct,
			int is_blocking)
{
    dm2_container_head_t *cont_head;
    char		 *uri_dir, *fake;
    GList		 *cont_list, *cont_obj;
    dm2_container_t	 *cont;
    int			 uri_len = strlen(uri);
    int			 ret = 0, err_ret = 0;
    int			 ch_locked = DM2_CONT_HEAD_UNLOCKED;

    uri_dir = alloca(uri_len+1);
    strcpy(uri_dir, uri);	//* OK to strcpy since used strlen for size
    fake = dm2_uri_dirname(uri_dir, 0);

    cont_head = dm2_conthead_get_by_uri_dir_rlocked(uri_dir, ct, &ch_locked,
						    &err_ret, is_blocking);
    if (cont_head) {
	/* If a disk was enabled in this ct after the preread got done, it
	 * would not have been read in yet. We will know this by checking
	 * ct disk enable time against the last container read time in the
	 * cont_head. If the disk enable time is latest, the requested URI
	 * could be there in the enabled disk, so we cant return a ENOENT
	 */
	if (ct->ct_dm2_last_ci_enable_ts > cont_head->ch_last_cont_read_ts) {
	    /* We have a disk enabled and is still unread */
	    ret = 0;
	    goto unlock;
	}
	cont_list = cont_head->ch_cont_list;
	for (cont_obj = cont_list; cont_obj; cont_obj = cont_obj->next) {
	    cont = (dm2_container_t *)cont_obj->data;
	    if (! NKN_CONT_EXTS_READ(cont)) {
		/* One container must still be read in */
		ret = 0;
		goto unlock;
	    }
	}
	ret = -ENOENT;
    } else {
	ret = err_ret;	// might be present
    }

 unlock:
    if (cont_head)
	dm2_conthead_rwlock_runlock(cont_head, ct, &ch_locked);
    return ret;
}	/* dm2_quick_uri_cont_stat */

/*
 * Return information about whether a URI, specified by the uol, is present.
 * Let the calling application know about the URI by returning a physid
 * which is the location of the information in the cache.
 */
static int
DM2_type_stat(nkn_uol_t	       uol,
	      dm2_cache_type_t *ct,
	      MM_stat_resp_t   *reply)
{
    off_t		tot_len = 0;
    off_t		unavail_offset = 0, *unavail_offset_p=NULL;
    dm2_bitmap_header_t	*bmh;
    DM2_uri_t		*uri_head;
    DM2_extent_t	*ext;
    dm2_container_head_t *cont_head = NULL;
    dm2_container_t	*cont;				// not used
    char		*uri = NULL;
    dm2_cache_info_t	*free_space_dev_ci;
    int			valid_cont_found = 0;
    int			mutex_locked = 0;
    int			uri_rlocked = 0;
    int			ret = 0, is_blocking = DM2_NONBLOCKING;
    int			cont_head_locked = DM2_CONT_HEAD_UNLOCKED;
    int			cont_wlocked = 0, errcode = 0;
    int                 object_with_hole = 1;
    uint32_t		preread = -1;

    glob_dm2_stat_cnt++;
    ct->ct_dm2_stat_cnt++;

    preread = reply->in_flags & MM_FLAG_PREREAD;

    if (preread)
	is_blocking = DM2_BLOCKING;

    if ((ret = dm2_stat_check(&uol, &uri)) < 0)
	goto dm2_stat_error;

    DBG_DM2M2("[cache_type=%s] Lookup URI=%s off=%ld len=%ld",
	      ct->ct_name, uri, uol.offset, uol.length);

    if (reply->in_flags & MM_FLAG_TRACE_REQUEST)
	DBG_TRACELOG("[cache_type=%s] Lookup URI=%s off=%ld len=%ld",
		     ct->ct_name, uri, uol.offset, uol.length);


 stat_loop:
    mutex_locked = 0;
    uri_rlocked = 0;
    /* Get the uri_head along with the ci rlocked. If we are unable to get the
     * uri_head because of any lock contention, the errcode will state it */
    uri_head = dm2_uri_head_get_by_uri_rlocked(uri, cont_head,
					       &cont_head_locked, ct,
					       is_blocking, DM2_RLOCK_CI,
					       &errcode);
    if (uri_head == NULL) {

	if (errcode) {
	    /* We will fall in here, if we are unable to lock the URI
	     * even if it exists */
	    ret =  -NKN_SERVER_BUSY;
	    glob_dm2_api_server_busy_err++;
	    glob_dm2_stat_busy_err++;
	    goto dm2_stat_error;
	}

	ret = dm2_quick_uri_cont_stat(uri, ct, is_blocking);
	if (ret == -ENOENT) {
	    glob_dm2_quick_stat_not_found++;
	    goto dm2_stat_not_found;
	} else if (ret == -EBUSY) {
	    /* We will fall in here if we are unable to lock the cont_head */
	    ret =  -NKN_SERVER_BUSY;
	    glob_dm2_api_server_busy_err++;
	    glob_dm2_stat_busy_err++;
	    goto dm2_stat_error;
	}

	if (ct->ct_tier_preread_done == 0 && preread == 0) {
	    /* Pre-read thread still working */
	    ret = -NKN_SERVER_BUSY;
	    glob_dm2_api_server_busy_err++;
	    ct->ct_dm2_stat_not_ready++;
	    goto dm2_stat_error;
	}
	if (ct->ct_tier_preread_done && ct->ct_tier_preread_stat_opt) {
	    /* As long as ct_tier_preread_stat_opt is set and preread is done,
	     * we know that there are no directories which are not found */
	    ct->ct_dm2_stat_opt_not_found++;
	    goto dm2_stat_not_found;
	}

	if (preread == 0 && ct->ct_tier_preread_done && dm2_dictionary_full()) {
	    /*
	     * There is no need to read in containers if we have no room.
	     * The below code will do the proper memory check in
	     * dm2_get_containers(), but it also does a stat() which causes
	     * slowdown.
	     */
	    ret = -NKN_SERVER_BUSY;
	    glob_dm2_api_server_busy_dict_full_err++;
	    ct->ct_dm2_stat_dict_full++;
	    goto dm2_stat_error;
	}

	/* This is really a lock to do a disk read of containers */
	if (!preread)
	    dm2_ct_stat_mutex_lock(ct, &mutex_locked, uri);
	if ((ret = dm2_uri_head_lookup_by_uri(uri, cont_head,
					      &cont_head_locked, ct, 0)) < 0) {
	    if (ret == -EBUSY)
		goto dm2_stat_error;
	    NKN_ASSERT(ret == -ENOENT);
	} else {
	    if (!preread)
		dm2_ct_stat_mutex_unlock(ct, &mutex_locked, uri);
	    goto stat_loop;
	}
	/* not in memory, check out disk cache */

	ret = dm2_get_contlist(uri, 0, NULL, ct, &valid_cont_found, &cont_head,
			       &cont, &cont_wlocked, &cont_head_locked);
	if (ret < 0)
	    goto dm2_stat_error;
	NKN_ASSERT(cont_wlocked == 0);	/* Should never be creating anything */

	if (valid_cont_found == 0)
	    goto dm2_stat_not_found;

	if ((ret = dm2_read_contlist(cont_head, preread, ct)) < 0)
	    goto dm2_stat_error;

	if ((ret = dm2_read_container_attrs(cont_head, &cont_head_locked,
					    preread, ct)) < 0)
	    goto dm2_stat_error;

	uri_head = dm2_uri_head_get_by_uri_rlocked(uri, cont_head,
						   &cont_head_locked, ct,
						   DM2_NONBLOCKING,
						   DM2_RLOCK_CI, NULL);
	if (uri_head == NULL)
	    goto dm2_stat_not_found;

	dm2_conthead_rwlock_unlock(cont_head, ct, &cont_head_locked);
	if (!preread)
	    dm2_ct_stat_mutex_unlock(ct, &mutex_locked, uri);
    }
    uri_rlocked = 1;

    dm2_uri_log_state(uri_head, DM2_URI_OP_STAT, 0, 0, 0, nkn_cur_ts);

    if (!(reply->in_flags & MM_FLAG_ALLOW_PARTIALS) &&
	(uri_head->uri_obj_type & NKN_OBJ_STREAMING) &&
	(uri_head->uri_resvd)) {
	/*
	 * For streaming objects, do not serve objects if they are
	 * partial in the disk.
	 */
	DBG_DM2M2("STAT: Partial URI=%s", uri_head->uri_name);
	ct->ct_dm2_stat_uri_partial_cnt++;
	goto dm2_stat_not_found;
    }

    if (uri_head->uri_chksum_err) {
	/* The URI is bad, so return as if we don't have it in cache */
	glob_dm2_stat_uri_chksum_err++;
	goto dm2_stat_not_found;
    }

    if (uri_head->uri_invalid) {
	glob_dm2_stat_uri_invalid_err++;
	goto dm2_stat_not_found;
    }
    /*
     * Has this uri expired?
     * When the attributes cannot be found, uri_expiry == 0 which will cause
     * the extents to be deleted by expiring the URI
     */
    if (!(reply->in_flags & MM_FLAG_IGNORE_EXPIRY) &&
	((int)uri_head->uri_expiry) != NKN_EXPIRY_FOREVER &&
	uri_head->uri_expiry <= nkn_cur_ts) {		// time_t is signed
	DBG_DM2M2("STAT: URI Expired=%s current=%ld read=%d",
		  uri_head->uri_name, nkn_cur_ts, uri_head->uri_expiry);
	glob_dm2_stat_expired_cnt++;
	goto dm2_stat_not_found;
    }
    free_space_dev_ci = uri_head->uri_container->c_dev_ci;

    /* The object may not be fully created */
    if (uri_head->uri_ext_list == NULL) {
	DBG_DM2M2("NULL EXT_LIST");
	goto dm2_stat_not_found;
    }
    NKN_ASSERT(uri_head->uri_ext_list);
    if (uri_head->uri_bad_delete)
	goto dm2_stat_not_found;

    /* Allow STAT to pass only if the attribute is written/read in */
    if (uri_head->uri_at_off == DM2_ATTR_INIT_OFF) {
	DBG_DM2M2("STAT: No attributes for URI %s", uri_head->uri_name);
	goto dm2_stat_not_found;
    }

    /* Check that we have the same version */
    ret = nkn_cod_test_and_set_version(uol.cod, uri_head->uri_version, 0);
    NKN_ASSERT(ret != NKN_COD_INVALID_COD);
    if (ret != 0) {
	ct->ct_dm2_stat_cod_mismatch_warn++;
	goto dm2_stat_not_found;
    }

    if(reply->in_flags & MM_FLAG_MM_ING_REQ)
	unavail_offset_p = &unavail_offset;

    ext = dm2_find_extent_by_offset(uri_head->uri_ext_list, uol.offset,
				    uol.length, &tot_len, unavail_offset_p,
				    &object_with_hole);
    if (ext == NULL)
	goto dm2_stat_not_found;

    /* We have a good URI but we are told to not serve this object yet */
    if (nkn_cur_ts < uri_head->uri_time0) {
	glob_dm2_stat_time0_cnt++;
	ret = -NKN_FOUND_BUT_NOT_VALID_YET;
	goto dm2_stat_error;
    }

    NKN_ASSERT((ext->ext_flags & DM2_EXT_FLAG_INUSE) == DM2_EXT_FLAG_INUSE);
    reply->unavail_offset = unavail_offset;
    reply->tot_content_len = tot_len;
    reply->content_len = uri_head->uri_content_len;
    bmh = (dm2_bitmap_header_t *)free_space_dev_ci->bm.bm_buf;
    reply->media_blk_len = bmh->u.bmh_disk_blocksize;
    reply->physid2 = ext->ext_physid;
    /* Tell BM that DM2 will allocate the required buffers for GET */
    reply->out_flags |= MM_OFLAG_NOBUF;
    if((reply->in_flags & MM_FLAG_MM_ING_REQ) && object_with_hole)
	reply->out_flags |= MM_OFLAG_OBJ_WITH_HOLE;
    reply->attr_size = uri_head->uri_blob_at_len;
    // Hack until we remove DM
    //    memset(reply->physid, 0, NKN_MAX_FILE_NAME_SZ);
    //    *(uint64_t *)(&reply->physid[0]) = ext->ext_physid;

    DBG_DM2M2("[cache_type=%s] Found URI: %s  off=%ld  len=%ld  totlen=%ld",
	      ct->ct_name, uri, uol.offset, uol.length, tot_len);
    if (reply->in_flags & MM_FLAG_TRACE_REQUEST) {
	DBG_TRACELOG("[cache_type=%s] Found URI: %s off=%ld len=%ld "
		     "totlen=%ld", ct->ct_name, uri, uol.offset,
		     uol.length, tot_len);
    }
    if (cont_head_locked != DM2_CONT_HEAD_UNLOCKED)
	dm2_conthead_rwlock_unlock(cont_head, ct, &cont_head_locked);
    if (cont_wlocked)
	dm2_cont_rwlock_wunlock(cont, &cont_wlocked, uri);
    if (uri_rlocked)
	dm2_uri_head_rwlock_runlock(uri_head, ct, DM2_RUNLOCK_CI);

    assert(uri_head->uri_container->c_dev_ci);
    return 0;

 dm2_stat_not_found:
    glob_dm2_stat_not_found++;
    ct->ct_dm2_stat_not_found++;
    reply->tot_content_len = 0;
    reply->media_blk_len = 0;
    if (uri_rlocked)
	dm2_uri_log_state(uri_head, DM2_URI_OP_STAT, 0, -ENOENT, 0, nkn_cur_ts);
    if (uri_rlocked)
	dm2_uri_head_rwlock_runlock(uri_head, ct, DM2_RUNLOCK_CI);
    if (cont_wlocked)
	dm2_cont_rwlock_wunlock(cont, &cont_wlocked, uri);
    if (cont_head_locked != DM2_CONT_HEAD_UNLOCKED)
	dm2_conthead_rwlock_unlock(cont_head, ct, &cont_head_locked);
    /* Freeing empty containers here would race against a DELETE that
     * also tries to free the container. So comment the below lines
     * until a protocol is defined for multiple paths to free a container */
    //if (cont_head)
    //    dm2_cont_head_free_empty_containers(cont_head, ct, uri);
    if (mutex_locked)
	dm2_ct_stat_mutex_unlock(ct, &mutex_locked, uri);
    DBG_DM2M2("[cache_type=%s] Stat URI not found: %s", ct->ct_name, uri);
    if (reply->in_flags & MM_FLAG_TRACE_REQUEST)
	DBG_TRACELOG("[cache_type=%s] Stat URI not found: %s",
		     ct->ct_name, uri);
    return -ENOENT;

 dm2_stat_error:
    if (ret == -NKN_SERVER_BUSY && !ct->ct_tier_preread_done) {
	/* If we are pre-reading, increment a warning counter
	 * instead of the stat error counter */
	glob_dm2_stat_warn++;
	ct->ct_dm2_stat_warn++;
    } else {
	glob_dm2_stat_err++;
	ct->ct_dm2_stat_err++;
    }
    reply->tot_content_len = 0;
    reply->media_blk_len = 0;
    if (uri_rlocked)
	dm2_uri_log_state(uri_head, DM2_URI_OP_STAT, 0, ret, 0, nkn_cur_ts);
    if (uri_rlocked)
	dm2_uri_head_rwlock_runlock(uri_head, ct, DM2_RUNLOCK_CI);
    if (cont_wlocked)
	dm2_cont_rwlock_wunlock(cont, &cont_wlocked, uri);
    if (cont_head_locked != DM2_CONT_HEAD_UNLOCKED)
	dm2_conthead_rwlock_unlock(cont_head, ct, &cont_head_locked);
    /* Freeing empty containers here would race against a DELETE that
     * also tries to free the container. So comment the below lines
     * until a protocol is defined for multiple paths to free a container */
    //if (cont_head)
    //	dm2_cont_head_free_empty_containers(cont_head, ct, uri);
    if (mutex_locked)
	dm2_ct_stat_mutex_unlock(ct, &mutex_locked, uri);
    /* Special handling of trailing '/' */
    if (ret == -ERANGE)
	ret = -EINVAL;
    else if (ret == -NKN_SERVER_BUSY) {
	DBG_DM2M("[cache_type=%s] Stat URI error=%d: %s",
		 ct->ct_name, -ret, uri);
    } else {
	DBG_DM2E("[cache_type=%s] Stat URI error=%d: %s",
		 ct->ct_name, -ret, uri);
	if (reply->in_flags & MM_FLAG_TRACE_REQUEST)
	    DBG_TRACELOG("[cache_type=%s] Stat Failure URI: %s",
			 ct->ct_name, uri);
    }
    return ret;
}	/* DM2_type_stat */


int
DM2_stat(const nkn_uol_t uol,
	 MM_stat_resp_t  *stat_op)
{
    uint64_t	     init_ticks;
    dm2_cache_type_t *ct;
    int64_t	     old_stat_thr_cnt, old_pr_stat_thr_cnt = 0;
    int		     ret = 0, idx, preread = 0;
    int		     dec_counter = 0;

    if (stat_op == NULL) {
	ret = -EINVAL;
	goto error;
    }

    if (stat_op->sub_ptype < 0 ||
	stat_op->sub_ptype > glob_dm2_num_cache_types) {
	DBG_DM2S("Illegal sub_ptype: %d", stat_op->sub_ptype);
	ret = -EINVAL;
	goto error;
    }

    idx = dm2_cache_array_map_ret_idx(stat_op->ptype);
    if (idx < 0) {
	DBG_DM2S("Illegal ptype: %d", stat_op->ptype);
	ret = -EINVAL;
	goto error;
    }
    init_ticks = LATENCY_START();

    ct = &g_cache2_types[idx];
    old_stat_thr_cnt = AO_fetch_and_add1(&ct->ct_dm2_stat_curr_cnt);

    preread = stat_op->in_flags & MM_FLAG_PREREAD;
    if (preread)
	old_pr_stat_thr_cnt = AO_fetch_and_add1(&ct->ct_dm2_pr_stat_curr_cnt);
    /*
     * Limit the number of dm2 stat threads to 90% of the available
     * stat threads. Preread threads should not be accounted for this
     * SAC. Also, the SAC check should not be done for preread STAT's
     * so as to allow preread to complete fully.
     * g_nkn_num_stat_threads - Number of threads per scheduler thread
     * glob_sched_num_core_threads - Number of scheduler threads
     *
     */
    if (!preread) {
	if ((old_stat_thr_cnt - old_pr_stat_thr_cnt) >=
	    ((g_nkn_num_stat_threads * glob_sched_num_core_threads * 90)/100)) {
	    /* This will act as a SAC */
	    AO_fetch_and_sub1(&ct->ct_dm2_stat_curr_cnt);
	    ret = -NKN_SERVER_BUSY;
	    goto error;
	}
    }
    dec_counter = 1;

    ret = DM2_type_stat(uol, ct, stat_op);
    if (ret == -ENOCSI &&
	(stat_op->in_flags & MM_FLAG_PREREAD) != MM_FLAG_PREREAD) {
	/*
	 * By returning ENOENT, the higher level will continue the ingest for
	 * a normal call of DM2_stat().  When pre-read calls this routine, it
	 * wants to know that we have a full dictionary, so it can stop the
	 * preread process.
	 */
	ret = -ENOENT;
    }
    LATENCY_END(&ct->ct_dm2_stat_latency, init_ticks);

 error:
    stat_op->mm_stat_ret = (ret < 0 ? -ret : ret);
    if (dec_counter)
	AO_fetch_and_sub1(&ct->ct_dm2_stat_curr_cnt);
    if (preread)
	AO_fetch_and_sub1(&ct->ct_dm2_pr_stat_curr_cnt);
    return ret;
}	/* DM2_stat */


static int
dm2_get_check(MM_get_resp_t *get,
	      char **uri_ret)
{
    char *uri;
    int ret = 0;

    if (get == NULL) {
	DBG_DM2S("get == NULL");
	NKN_ASSERT(get != NULL);
	ret = -EINVAL;
	goto dm2_get_check_error;
    }
    if (get->physid2 == 0 || get->in_uol.length <= 0) {
	DBG_DM2S("Input argument problem: physid=0x%lx, len=%ld",
		 get->physid2, get->in_uol.length);
	NKN_ASSERT(get->physid != 0);
	NKN_ASSERT(get->in_uol.length > 0);
	/*
	 * A zero physid is illegal (unless searching by uri) because
	 * a provider id is part of physid2 and providers are > 0
	 */
	ret = -EINVAL;
	goto dm2_get_check_error;
    }
    if (get->in_uol.cod == NKN_COD_NULL) {
	DBG_DM2S("Invalid GET UOL cod: NULL");
	ret = -EINVAL;
	NKN_ASSERT(get->in_uol.cod != NKN_COD_NULL);
	goto dm2_get_check_error;
    }

    uri = (char *)nkn_cod_get_cnp(get->in_uol.cod);
    if (uri == NULL || *uri == '\0') {
	DBG_DM2S("URI not found from cod: NULL");
	NKN_ASSERT(uri != NULL);
	ret = -EINVAL;
	goto dm2_get_check_error;
    }
    if (uri[0] != '/') {
	DBG_DM2S("[URI=%s] Illegal URI from cod", uri);
	ret = -EINVAL;
	goto dm2_get_check_error;
    }
    if (dm2_invalid_slash(uri)) {
	ret = -EINVAL;
	goto dm2_get_check_error;
    }

    if (strstr(uri, "/./") || strstr(uri, "/../")) {
	DBG_DM2M2("[URI=%s] Illegal URI component", uri);
	ret = -EINVAL;
	goto dm2_get_check_error;
    }
    *uri_ret = uri;
    return 0;

 dm2_get_check_error:
    glob_dm2_get_invalid_input_err++;
    return ret;
}	/* dm2_get_check */


static int
dm2_verify_checksum(MM_get_resp_t *get,
		    DM2_blk_map_t *bmap,
		    const char	  *get_uri,
		    const char	  *ext_uri,
		    int		  disk_block_size,
		    int		  read_full_ext,
		    int		  idx,
		    int		  num_pages)
{
#define CHKSUM_ROUNDOFF_LEN 4096 //shld be the min disk page size supported
    uint64_t	    checksum, uol_length;
    uint32_t	    checksum32 = 0, read_size;
    off_t	    chunk_start = 0, chunk_diff = 0;
    DM2_extent_t    *ext = bmap[idx].ext;
    char	    *uri = ext->ext_uri_head->uri_name;
    int		    j, chk_idx = 0, read_length = 0;
    int		    proc_len = 0, count;

    if ((ext->ext_flags & DM2_EXT_FLAG_CHKSUM) == 0) {
	DBG_DM2S("Checksum NOT PRESENT");
	return 0;
    }

    checksum = 0;
    read_length = ext->ext_read_size * DM2_EXT_RDSZ_MULTIPLIER;

    if (read_length == disk_block_size) {
	for (j = idx; j < idx + num_pages; j++) {
	    uol_length = ROUNDUP_PWR2(bmap[j].data_len, CHKSUM_ROUNDOFF_LEN);
	    checksum =
		do_csum64_iterate_aligned(bmap[j].cp, uol_length, checksum);
	}

	if (checksum != ext->ext_csum.ext_checksum) {
	    DBG_DM2E("[URI=%s] [mgmt=%s] Checksum mismatch: expected=0x%lx "
		     "got=0x%lx", uri,
		     ext->ext_uri_head->uri_container->c_dev_ci->mgmt_name,
		     ext->ext_csum.ext_checksum, checksum);
	    DBG_DM2E("e_offset=%ld  u_off=%ld  u_len=%ld",
		     ext->ext_start_sector, get->in_uol.offset,
		     get->in_uol.length);
	    glob_dm2_get_checksum_err++;
	    /* Force BM to delete all bad buffers */
	    nkn_buffer_purge(&get->in_uol);
	    return -EINVAL;
	} else {
	    DBG_DM2M3("[URI=%s] Checksum OK: expected=0x%lx got=0x%lx",
		      uri, ext->ext_csum.ext_checksum, checksum);
	    return 0;
	}
    } else {

	/* point to the appropriate checksum, based on the read offset in the
	 * GET request. This should be done only if a particular extent was read
	 * on the URI that was requested */
	if (!strcmp(get_uri, ext_uri) && !read_full_ext) {
	    /* make sure to point to the start of the chunk that can be
	     * checksum verified */
	    chunk_diff = get->in_uol.offset - ext->ext_offset;
	    read_size = ext->ext_read_size * DM2_EXT_RDSZ_MULTIPLIER;
	    if (chunk_diff % read_size)
		chunk_diff = (chunk_diff/read_size) * read_size;
	    chunk_start = chunk_diff +  ext->ext_offset;
	    chk_idx = (chunk_start - ext->ext_offset ) / read_length;
	} else {
	    chk_idx = 0;
	}

	while (num_pages > 0) {
	    checksum32 = 0;
	    count = 0;
	    for (j = idx; j < idx + num_pages; j++) {
		uol_length = ROUNDUP_PWR2(bmap[j].data_len,CHKSUM_ROUNDOFF_LEN);
		checksum32 =
		  do_csum32_iterate_aligned(bmap[j].cp, uol_length, checksum32);
		proc_len += uol_length;
		count++;
		if (proc_len == read_length)
		    break;
	    }

	    if (checksum32 != ext->ext_csum.ext_v2_checksum[chk_idx]) {
		DBG_DM2S("[URI=%s] [mgmt=%s] Checksum mismatch: expected=0x%x "
			 "got=0x%x", uri,
			 ext->ext_uri_head->uri_container->c_dev_ci->mgmt_name,
			 ext->ext_csum.ext_v2_checksum[chk_idx], checksum32);
		DBG_DM2S("e_offset=%ld  u_off=%ld  u_len=%ld",
			 ext->ext_start_sector, get->in_uol.offset,
			 get->in_uol.length);
		glob_dm2_get_checksum_err++;
		NKN_ASSERT(0);
		/* Force BM to delete all bad buffers */
		nkn_buffer_purge(&get->in_uol);
		return -EINVAL;
	    } else {
		DBG_DM2M3("[URI=%s] Checksum OK: expected=0x%x got=0x%x",
			  uri, ext->ext_csum.ext_v2_checksum[chk_idx], checksum32);
	    }
	    num_pages = num_pages - count;
	    idx = j + 1;
	    proc_len = 0;
	    chk_idx++;
	}
    }
    return 0;
}	/* dm2_verify_checksum */

static int
dm2_get_alloc_buffer(DM2_blk_map_t *bmap,
		     int	   *idx_bmap,
		     uint32_t	   page_size)
{
    void	 *content_ptr, *ptr_vobj;
    uint32_t	 mem_page_size = 0;

    /* We should not be allocating more than 64 buffers,
     * if so thene is a bug in DM2 */
    if (*idx_bmap < 0 || *idx_bmap >= MM_MAX_OUT_BUFS) {
	NKN_ASSERT(0);
	return -EINVAL;
    }
    ptr_vobj = nkn_buffer_alloc(NKN_BUFFER_DATA, page_size, 0);
    if (ptr_vobj == NULL) {
	/* Buffer allocation failure */
	return -ENOMEM;
    }
    content_ptr = nkn_buffer_getcontent(ptr_vobj);
    if (content_ptr == NULL) {
	NKN_ASSERT(content_ptr);
	return -ENOMEM;
    }
    mem_page_size = nkn_buffer_get_bufsize(ptr_vobj);
    /* If we get a buffer of different size than requested, we still would use
     * it for the size we want. Hopefully, "nkn_buffer_alloc" should not give
     * back a buffer of samller size than requestd */
    if (mem_page_size != page_size)
	glob_dm2_get_buf_alloc_size_mismatch_cnt++;
    bmap[*idx_bmap].bp = ptr_vobj;
    bmap[*idx_bmap].cp = content_ptr;
    bmap[*idx_bmap].mem_page_size = page_size;;
    return 0;
}	/* dm2_get_alloc_buffer */


static int
dm2_ext_list_to_bmap(GList	    *ph_ext_list,
		     DM2_blk_map_t  *bmap,
		     struct iovec   *iov,
		     int	    *bmap_cnt,
		     DM2_extent_t   *seek_ext,
		     off_t	    in_uol_offset,
		     int	    read_full_ext,
		     uint32_t	    disk_block_size,
		     uint32_t	    disk_page_size)
{
    GList	 *ext_obj;
    DM2_extent_t *ext;
    off_t	 start_sec = 0, next_sec = 0;
    uint32_t	 read_length = 0, read_size;
    int		 num_32k_pages, num_4k_pages, rem_bytes, num_hole_pages;
    int		 idx_gca, idx_lca, idx_bmap;
    int		 ret = 0, tmp_ext_start = 0;
    int		 sec_per_disk_page = disk_page_size/DEV_BSIZE;

    /*
     * Walk the extent list and generate the block map.
     * If there exists a hole in the block, we still have to allocate buffers
     * and read the contents. It shall be ignored later.
     * If the 'disk_page_size' is 32K, then all the buffers needed for reading
     * should be 32K.
     *
     * If the 'disk_page_size' is 4K, use a 4K buffer for objects <= 4K.
     * Else, use 32K buffers and truncate the 'iov_len' at 4K boundary on
     * the last 32K buffer. If the last part of the object fits in a 4K buffer
     * use a 4K buffer.
     */
    *bmap_cnt = 0;
    idx_gca = 0; idx_lca = 0; idx_bmap = 0;
    start_sec = -1; next_sec = -1;
    for (ext_obj = ph_ext_list; ext_obj; ext_obj = ext_obj->next) {
	num_32k_pages = num_4k_pages = num_hole_pages = 0;
	ext = (DM2_extent_t *) ext_obj->data;
	if (start_sec == -1) {
	    /* 'next_sec' is the start of the block to start with */
	    start_sec =(ext->ext_start_sector & ~(disk_block_size/DEV_BSIZE-1));
	    next_sec = start_sec;
	}
	if (seek_ext && seek_ext != ext) {
	    continue;
	} else if (seek_ext) {
	    next_sec = ext->ext_start_sector;
	    if (read_full_ext)
		read_length = ext->ext_length;
	    else {
		read_size = ext->ext_read_size * DM2_EXT_RDSZ_MULTIPLIER;
		/* If the GET is for an offset within the extent, calculate the
		 * read_offset and length accordingly */
		if ((in_uol_offset - seek_ext->ext_offset) > 0) {
		    /* If the extent length is smaller than read_size and the
		     * 'in_uol_offset' points to an offset inside the extent,
		     * the modulo with read_size will yield a negative offset
		     * that is worng */
		    if (ext->ext_length > read_size) {
			tmp_ext_start = in_uol_offset - seek_ext->ext_offset;
			if (tmp_ext_start % read_size)
				tmp_ext_start = (tmp_ext_start/read_size) * read_size;
		    }
		}
		/* We cant have a negative value for tmp_ext_start */
		if (tmp_ext_start < 0)
		    NKN_ASSERT(0);

		next_sec = next_sec + (tmp_ext_start / DEV_BSIZE);
		if ((seek_ext->ext_length - tmp_ext_start) < read_size)
		    read_length = seek_ext->ext_length - tmp_ext_start;
		else
		    read_length = read_size;
	    }
	} else {
	    read_length =  ext->ext_length;
	}

	if (!seek_ext && (next_sec != ext->ext_start_sector)) {
	    num_hole_pages = ((ext->ext_start_sector - next_sec) * DEV_BSIZE /
			       disk_page_size);

	    /* a hole has been found, fill up the pages now
	     * we will use the native disk_page_size for these buffers */
	    while (num_hole_pages > 0) {
		ret = dm2_get_alloc_buffer(bmap, &idx_bmap, disk_page_size);
		if (ret)
		    goto dm2_bmap_end;
		/* mark that these buffers are holes */
		bmap[idx_bmap].data_len = 0;
		bmap[idx_bmap].is_hole = 1;
		bmap[idx_bmap].ext = NULL;
		iov[idx_bmap].iov_base =  bmap[idx_bmap].cp;
		iov[idx_bmap].iov_len = disk_page_size;
		num_hole_pages--;
		idx_bmap++;
	    }
	}

	/* find where the next extent should start from the ext_length */
	next_sec = ext->ext_start_sector +
		     (read_length/disk_page_size) * sec_per_disk_page;
	if (read_length % disk_page_size)
	    next_sec += sec_per_disk_page;

	/* find how many 32K/4K pages are required for this extent */
	num_32k_pages = read_length / DM2_DISK_PAGESIZE_32K;
	rem_bytes = read_length % DM2_DISK_PAGESIZE_32K;
	if (disk_page_size == DM2_DISK_PAGESIZE_32K ) {
	    if (rem_bytes) {
		if (seek_ext && rem_bytes <= DM2_DISK_PAGESIZE_4K)
		    num_4k_pages++;
		else
		    num_32k_pages++;
	    }
	} else if (rem_bytes > DM2_DISK_PAGESIZE_4K) {
	    num_32k_pages++;
	} else if (rem_bytes) {
	    num_4k_pages++;
	}

	/* update the total pages required to bmap for later use */
	bmap[idx_bmap].num_pages = num_32k_pages + num_4k_pages;

	/* allocate the necessary 32K buffers */
	while (num_32k_pages > 0) {
	    ret = dm2_get_alloc_buffer(bmap, &idx_bmap, DM2_DISK_PAGESIZE_32K);
	    if (ret)
		goto dm2_bmap_end;
	    iov[idx_bmap].iov_base =  bmap[idx_bmap].cp;

	    /* if this is the last 32k buffer and no 4k buffers are needed
	     * the remaining content should fit in this last 32k buffer */
	    if (num_32k_pages == 1 && !num_4k_pages)
		bmap[idx_bmap].data_len = read_length;
	    else
		bmap[idx_bmap].data_len = DM2_DISK_PAGESIZE_32K;

	    if (disk_page_size == DM2_DISK_PAGESIZE_4K &&
		num_32k_pages == 1 && !num_4k_pages ) {
		/* if disk_page_size is 4K, truncate iov_len to a 4K boundary
		 * on the last 32K buffer */
		iov[idx_bmap].iov_len =
		    ROUNDUP_PWR2(read_length, disk_page_size);
	    } else {
		iov[idx_bmap].iov_len = DM2_DISK_PAGESIZE_32K;
	    }
	    read_length -= DM2_DISK_PAGESIZE_32K;
	    bmap[idx_bmap].is_hole = 0;
	    bmap[idx_bmap].ext = ext;
	    num_32k_pages--;
	    idx_bmap++;
	}

	/* allocate the necessary 4K buffers */
	while (num_4k_pages > 0) {
	    ret = dm2_get_alloc_buffer(bmap, &idx_bmap, DM2_DISK_PAGESIZE_4K);
	    if (ret)
		goto dm2_bmap_end;
	    iov[idx_bmap].iov_base =  bmap[idx_bmap].cp;
	    iov[idx_bmap].iov_len = DM2_DISK_PAGESIZE_4K;

	    /* for the last page, set the data_len to the occupancy bytes */
	    if (num_4k_pages == 1)
		bmap[idx_bmap].data_len = read_length;
	    else
		bmap[idx_bmap].data_len = DM2_DISK_PAGESIZE_4K;
	    read_length -= DM2_DISK_PAGESIZE_4K;
	    bmap[idx_bmap].is_hole = 0;
	    bmap[idx_bmap].ext = ext;
	    num_4k_pages--;
	    idx_bmap++;
	}
    }

 dm2_bmap_end:
    *bmap_cnt = idx_bmap;
    return ret;
}	/* dm2_ext_list_to_bmap */

/*
 * It should be safe for GET to skip grabbing a ci_dev read lock because the
 * only lookup mechanism here is a URI lookup.  The URI will either be found
 * or not found.  If it is waiting on a read lock and the object is deleted,
 * the lookup will eventually fail.
 */
static int
DM2_type_get(MM_get_resp_t    *get,
	     dm2_cache_type_t *ct)
{
    char	      ns_uuid[NKN_MAX_UID_LENGTH];
    ns_stat_token_t   ns_stoken = NS_NULL_STAT_TOKEN;
    nkn_uol_t	      uol, del_uol;
    DM2_physid_head_t *physid_head;
    GList	      *ph_ext_list, *ext_obj;
    DM2_extent_t      *ext, *seek_ext = NULL;
    void	      *ptr_vobj;
    DM2_uri_t	      *uri_head = NULL;
    uint64_t	      read_offset, chunk_diff;
    char	      *get_uri = NULL, *ext_uri;
    char	      *ext_base_addr, *fake;
    struct iovec      iovp_mem[MM_MAX_OUT_BUFS];
    struct iovec      *iov = iovp_mem;
    dm2_container_head_t *cont_head = NULL;
    DM2_blk_map_t     bmap[MM_MAX_OUT_BUFS];
    int		      num_pages, z, read_single = 0, ret = 0;
    int		      group_read = 0, small_read = 0;
    int		      uri_rlocked = 0, ext_rlocked = 0, read_full_ext = 1;
    int		      idx_bmap = 0, idx_setid = 0, idx_outbuf = 0;
    int		      cont_head_locked = DM2_CONT_HEAD_UNLOCKED;
    uint32_t	      read_length, read_size, ext_rdsz;
    uint32_t	      block_size, disk_page_size, mem_page_size;

    glob_dm2_get_cnt++;
    ct->ct_dm2_get_cnt++;

    if ((ret = dm2_get_check(get, &get_uri)) < 0)
	goto dm2_get_error;

    if ((ret = dm2_uridir_to_nsuuid(get_uri, ns_uuid)) != 0)
        goto dm2_get_error;

    /* Not needed here but be optimistic that we won't encounter errors */
    ns_stoken = dm2_ns_get_stat_token_from_uri_dir(get_uri);
    if (ns_is_stat_token_null(ns_stoken)) {
        DBG_DM2S("[URI=%s] Cannot get stat token", get_uri);
        ret = -NKN_STAT_NAMESPACE_EINVAL;
        goto dm2_get_error;
    }

    /* Get the group read state for thie namespace */
    ret = ns_stat_get_tier_group_read_state(ns_uuid, ct->ct_ptype, &group_read);
    if (ret) {
        DBG_DM2S("[cache_type=%s] Cannot get group read state, URI:%s",
		 ct->ct_name, get_uri);
        goto dm2_get_error;
    }

    /* Get the read size for this namespace */
    ret = ns_stat_get_tier_read_size(ns_uuid, ct->ct_ptype, &read_size);
    if (ret) {
        DBG_DM2S("[cache_type=%s] Cannot get read size, URI:%s",
		 ct->ct_name, get_uri);
        goto dm2_get_error;
    }

    if (read_size == 0) {
	DBG_DM2S("[cache_type=%s] Invalid read size for namespace %s",
		 ct->ct_name, ns_uuid);
	goto dm2_get_error;
    }

    ext_uri = alloca(MAX_URI_SIZE+1);
    strcpy(ext_uri, get_uri);
    fake = dm2_uri_dirname(ext_uri, 1);
    ext_base_addr = ext_uri + strlen(ext_uri);

    if (get->in_flags & MM_FLAG_TRACE_REQUEST)
	DBG_TRACELOG("[cache_type=%s] GET uri=%s off=%ld len=%ld", ct->ct_name,
		     get_uri, get->in_uol.offset, get->in_uol.length);

    // XXXmiken: TBD: Search by UOL to get the PHYSID and then search by PHYSID
    /* Get the uri_head along with the ci rlocked */
    uri_head = dm2_uri_head_get_by_uri_rlocked(get_uri, cont_head,
					       &cont_head_locked, ct,
					       DM2_BLOCKING, DM2_RLOCK_CI,NULL);
    if (uri_head == NULL) {
	ret = -ENOENT;
	DBG_DM2S("[cache_type=%s] URI not found: %s", ct->ct_name, get_uri);
	glob_dm2_get_not_found_cnt++;
	goto dm2_get_error;
    }
    uri_rlocked = 1;
    if (uri_head->uri_bad_delete) {
	ret = -ENOENT;
	DBG_DM2W("[cache_type=%s] URI BAD => not found: %s",
		 ct->ct_name, get_uri);
	glob_dm2_get_not_found_cnt++;
	goto dm2_get_unlock;
    }
    ret = nkn_cod_test_and_set_version(get->in_uol.cod,
					uri_head->uri_version, 0);
    NKN_ASSERT(ret != NKN_COD_INVALID_COD);
    if (ret != 0) {
	/* A 'COD Mismatch' is possible if the object has changed in the
	 * origin after DM2 returned a stat success. Reducing the severity
	 * of this message since the test case in bug 4939, changes objects
	 * in the origin */
	DBG_DM2E("[cache_type=%s] URI cod mismatch: 0x%lx/0x%lx (URI=%s)",
		 ct->ct_name, uri_head->uri_version.objv_l[0],
		 uri_head->uri_version.objv_l[1], uri_head->uri_name);
	ct->ct_dm2_get_cod_mismatch_warn++;
	ret = -ret;
	goto dm2_get_unlock;
    }

    /* Get the block size and disk size for this disk. Note that these
     * are expressed in bytes */
    block_size = dm2_get_blocksize(uri_head->uri_container->c_dev_ci);
    disk_page_size = dm2_get_pagesize(uri_head->uri_container->c_dev_ci);

    /* If the read size matches blocksize, we have to read the full extent
     * If the config value is incorrect, it is better to crash than return error
     * or else we will just be rejecting GETs and never notice the issue at all
     */
    /* In a standard system with default config, the read_size will always be
     * equal to the block size. In case of SSD its 256K, SAS/SATA is 2MB.
     * If a SAS/SATA drive is formated with small block size of 256K, then
     * read_size will still be 2MB as its a namespace configuration and
     * the block_size is 256K. The additional check of
     * read_size == NKN_MAX_BLOCK_SZ will take care of the above mentioned case.
     */
    if ((read_size == block_size) || (read_size == NKN_MAX_BLOCK_SZ))
	small_read = 0;
    else if (read_size == (block_size / DM2_SMALL_READ_DIV))
	small_read = 1;
    else
	assert(0);

    physid_head = dm2_extent_get_by_physid_rlocked(&get->physid2, ct);
    if (physid_head == NULL) {
	DBG_DM2S("Could not find extent");
	ret = -ENOENT;
	glob_dm2_get_not_found_cnt++;
	goto dm2_get_unlock;
    }
    ext_rlocked = 1;

    /* extent_list should be head, but this doesn't hurt */
    ph_ext_list = g_list_first(physid_head->ph_ext_list);
    /*
     * Since multiple objects could map to the same physid, we need to find
     * the exact extent, so we get the correct uri_head which then allows
     * us to pass back the correct attributes later on.
     */
    for (ext_obj = ph_ext_list; ext_obj; ext_obj = ext_obj->next) {
	ext = (DM2_extent_t *) ext_obj->data;
	if (!strcmp(dm2_uh_uri_basename(ext->ext_uri_head),
		    dm2_uri_basename(get_uri))) {
	    if (uri_head != (void *)ext->ext_uri_head) {
		/*
		 * If these objs don't match, then ext_uri_head is not locked.
		 * Since we need to grab locks in order, just return and let BM
		 * retry mechanism grab locks in the correct order.
		 */
		ret = -EAGAIN;
		glob_dm2_get_physid_mismatch_cnt++;
		ct->ct_dm2_get_physid_mismatch_cnt++;
		goto dm2_get_unlock;
	    }
	    if (group_read == DM2_GROUP_READ_DISABLE) {
		/* If we have to read just a single or part of the URI,
		 * remember the corresponding extent. Make sure we have the
		 * correct ext when we have a multi-extent URI. */
		if (! (get->in_uol.offset >= ext->ext_offset &&
		       get->in_uol.offset < ext->ext_offset+ext->ext_length))
		    continue;
		seek_ext = ext;
		read_single = 1;
		if (small_read &&
		    (seek_ext->ext_uri_head->uri_content_len >
			    (block_size / DM2_SMALL_READ_DIV)))
		    read_full_ext = 0;
	    } else if (small_read &&
		       (ext->ext_uri_head->uri_content_len >= block_size)) {
		seek_ext = ext;
		read_full_ext = 0;
	    }
	    break;
	}
    }
    if (ext_obj == NULL) {	// Only if we didn't find uri
	ret = -ENOENT;
	glob_dm2_get_not_found_err++;
	goto dm2_get_unlock;
    }

    if (!(get->in_flags & MM_FLAG_IGNORE_EXPIRY) &&
	((int)uri_head->uri_expiry) != NKN_EXPIRY_FOREVER &&
	uri_head->uri_expiry <= nkn_cur_ts) {		// time_t is signed
	DBG_DM2M2("[cache_type=%s] GET expired_URI=%s current=%ld read=%d",
		  ct->ct_name, uri_head->uri_name, nkn_cur_ts,
		  uri_head->uri_expiry);
	ret = -ENOENT;
	goto dm2_get_unlock;
    }

    /*
     * Step 1: Walk the 'physid_ext_list' and generate the blk_map that
     * reflects the page usage in the block.
     */
    memset(bmap, 0, sizeof(bmap));
    if ((ret = dm2_ext_list_to_bmap(ph_ext_list, bmap, iov, &idx_bmap,
				    seek_ext, get->in_uol.offset,
				    read_full_ext, block_size,
				    disk_page_size))< 0) {
	goto dm2_get_unlock;
    }

    /* Step 2: Execute the readv system call */
    if ((ret = dm2_read_block(get, uri_head, seek_ext, read_full_ext, ct,
			      iov, idx_bmap))< 0) {
	goto dm2_get_unlock;
    }

    /* Step 3: Read in the attributes for the requested object */
    if (get->in_attr) {
	glob_dm2_get_attr_read_cnt++;
	if (get->in_flags & MM_FLAG_TRACE_REQUEST)
	    DBG_TRACELOG("[cache_type=%s] GET uri=%s ATTR Read",
			 ct->ct_name, get_uri);
	if ((ret = dm2_read_single_attr(get, ct->ct_ptype, uri_head,
					ph_ext_list, ext_uri,
					read_single)) < 0) {
	    glob_dm2_get_attr_read_err++;
	    goto dm2_get_unlock;
	}
    }

    /* Step 4: Match up every disk page to a memory page */
    for (idx_setid = 0; idx_setid < idx_bmap;) {

	/* setid only the buffers that have data read into them,
	 * if the buffer is marked a hole, do not process it. But
	 * it will be sent to BM as an 'anonymous' buffer and BM will
	 * release it */
	if (bmap[idx_setid].is_hole) {
	    get->out_data_buffers[idx_outbuf] = bmap[idx_setid].bp;
	    idx_outbuf++;
	    bmap[idx_setid].bp = NULL;
	    idx_setid++;
	    continue;
	}
	ext = (DM2_extent_t *) bmap[idx_setid].ext;
	ext_rdsz = ext->ext_read_size * DM2_EXT_RDSZ_MULTIPLIER;
	strcpy(ext_base_addr, dm2_uh_uri_basename(ext->ext_uri_head));

	if (read_full_ext) {
	    read_offset = ext->ext_offset;
	    read_length = ext->ext_length;
	} else {
	    /* We would have read from the start of the checksum'able
	     * chunk, so account the read_offset accordingly */
	    chunk_diff = get->in_uol.offset - ext->ext_offset;
	    if (chunk_diff % ext_rdsz)
		chunk_diff = (chunk_diff/ext_rdsz) * ext_rdsz;
	    read_offset = chunk_diff + ext->ext_offset;

	    if (ext->ext_length < ext_rdsz)
		read_length = ext->ext_length;
	    else
		read_length = ext_rdsz;
	}
	assert(read_length > 0);

	num_pages = bmap[idx_setid].num_pages;
	assert(num_pages);

	/* The checksum mismatched, so we delete the URI */
	if (likely(dm2_perform_get_chksum) &&
	    dm2_verify_checksum(get, bmap, get_uri, ext_uri, block_size,
				read_full_ext, idx_setid, num_pages)) {
	    uri_head->uri_chksum_err = 1;	// Force expiration
	    if (!strcmp(get_uri, ext_uri))
		del_uol.cod = nkn_cod_dup(get->in_uol.cod, NKN_COD_DISK_MGR);
	    else
		del_uol.cod = nkn_cod_open(ext_uri, strlen(ext_uri),
					   NKN_COD_DISK_MGR);
	    if (del_uol.cod != NKN_COD_NULL) {
		/* No need to set version in cod */
		del_uol.offset = del_uol.length = 0;
		nkn_buffer_purge(&del_uol);
		nkn_cod_close(del_uol.cod, NKN_COD_DISK_MGR);
	    }
	    ret = -ENOENT;
	    goto dm2_get_unlock;
	}
	if (!strcmp(get_uri, ext_uri)) {
	    uol.cod = get->in_uol.cod;
	} else if ((uol.cod =
		    nkn_cod_open(ext_uri, strlen(ext_uri),
				 NKN_COD_DISK_MGR)) == NKN_COD_NULL) {
	    /* This condition can happen if the object is streaming */
	    DBG_DM2W("[cache_type=%s] COD allocation failed (URI=%s) "
		     "(REQ_URI=%s)", ct->ct_name, ext_uri, get_uri);
	    glob_dm2_cod_null_cnt++;
	    goto next_ext;
	}
	ret = nkn_cod_test_and_set_version(uol.cod,
				   ext->ext_uri_head->uri_version, 0);
	NKN_ASSERT(ret != NKN_COD_INVALID_COD);
	if (ret != 0) {
	    ct->ct_dm2_get_cod_mismatch_warn++;
	    for (z = idx_setid; z < idx_setid+num_pages; z++) {
		get->out_data_buffers[idx_outbuf] = bmap[z].bp;
		idx_outbuf++;
	    }
	    goto next_ext;
	}

	for (z = idx_setid; z < idx_setid+num_pages; z++) {
	    if (read_length <= 0) {
		/* We should be exiting normally always and should not
		 * be reaching here unless some weird thing happens.
		 * break out to avoid an infinte loop */
		NKN_ASSERT(0);
		break;
	    }
	    ptr_vobj = bmap[z].bp;
	    assert(ptr_vobj);
	    mem_page_size = bmap[z].mem_page_size;

	    uol.offset = read_offset;
	    if (read_length >= mem_page_size) {
		uol.length = mem_page_size;
		read_offset += mem_page_size;
		read_length -= mem_page_size;
	    } else {
		uol.length = read_length;
		read_offset += uol.length;
		read_length = 0;
	    }
	    /* make this buffer available to BM */
	    nkn_buffer_setid(ptr_vobj, &uol, ct->ct_ptype,
			    (group_read)? NKN_BUFFER_GROUPREAD: 0);
	    get->out_data_buffers[idx_outbuf] = ptr_vobj;
	    idx_outbuf++;
	}
    next_ext:
	idx_setid += num_pages;
	if (uol.cod != NKN_COD_NULL && uol.cod != get->in_uol.cod)
	    nkn_cod_close(uol.cod, NKN_COD_DISK_MGR);
    }

    /* Send back the number of data buffers that were needed for this GET*/
    get->out_num_data_buffers = idx_outbuf;
    DBG_DM2M("[cache_type=%s] GET uri=%s off=%ld len=%ld expiry=%d",
	     ct->ct_name, get_uri, get->in_uol.offset,
	     get->in_uol.length, uri_head->uri_expiry);
    if (get->in_flags & MM_FLAG_TRACE_REQUEST)
	DBG_TRACELOG("[cache_type=%s] GET Succes uri=%s", ct->ct_name, get_uri);
    if (uri_rlocked && uri_head)
	dm2_uri_head_rwlock_runlock(uri_head, ct, DM2_RUNLOCK_CI);
    if (ext_rlocked)
	dm2_physid_rwlock_runlock(physid_head);
    glob_dm2_get_success++;
    ns_stat_free_stat_token(ns_stoken);
    return 0;

 dm2_get_unlock:
    if (uri_head)
        dm2_uri_log_state(uri_head, DM2_URI_OP_GET, 0, ret, 0, nkn_cur_ts);
    if (uri_rlocked && uri_head)
	dm2_uri_head_rwlock_runlock(uri_head, ct, DM2_RUNLOCK_CI);
    if (ext_rlocked)
	dm2_physid_rwlock_runlock(physid_head);

    /* Free all the allocated buffers, as the GET did not succeed */
    for (z = 0; z < idx_bmap; z++) {
	if (bmap[z].bp) {
	    nkn_buffer_release(bmap[z].bp);
	    bmap[z].bp = NULL;
	}
	/* hole assigned buffers need to be freed in error case */
	if (bmap[z].is_hole && get->out_data_buffers[z]) {
	    nkn_buffer_release(get->out_data_buffers[z]);
	}
	get->out_data_buffers[z] = NULL;
    }
    get->out_num_data_buffers = 0;

 dm2_get_error:
    ns_stat_free_stat_token(ns_stoken);
    DBG_DM2S("[cache_type=%s] GET uri=%s off=%ld len=%ld expiry=%d: %d",
	     ct->ct_name, get_uri, get->in_uol.offset,
	     get->in_uol.length, uri_head ? uri_head->uri_expiry : 0, ret);
    if (get->in_flags & MM_FLAG_TRACE_REQUEST)
	DBG_TRACELOG("[cache_type=%s] GET Failure uri=%s",
		      ct->ct_name, get_uri);
    glob_dm2_get_err++;
    ct->ct_dm2_get_err++;
    return ret;
}	/* DM2_type_get */


int
DM2_get(MM_get_resp_t *get)
{
    uint64_t	     init_ticks;
    dm2_cache_type_t *ct;
    int		     ret = 0, idx;
    int		     dec_counter = 0;

    if (get == NULL) {
	ret = -EINVAL;
	goto error;
    }

    if (get->in_sub_ptype < 0 || get->in_sub_ptype > glob_dm2_num_cache_types) {
	DBG_DM2S("Illegal sub_ptype: %d", get->in_sub_ptype);
	ret = -EINVAL;
	goto error;
    }

    idx = dm2_cache_array_map_ret_idx(get->in_ptype);
    if (idx < 0) {
	DBG_DM2S("Illlegal ptype: %d", get->in_ptype);
	ret = -EINVAL;
	goto error;
    }
    init_ticks = LATENCY_START();

    ct = &g_cache2_types[idx];
    AO_fetch_and_add1(&ct->ct_dm2_get_curr_cnt);
    dec_counter = 1;

    ret = DM2_type_get(get, ct);
    LATENCY_END(&ct->ct_dm2_get_latency, init_ticks);

 error:
    get->err_code = (ret < 0 ? -ret : ret);
    if (dec_counter)
	AO_fetch_and_sub1(&ct->ct_dm2_get_curr_cnt);
    return ret;
}	/* DM2_get */


void
DM2_cleanup(void)
{
    glob_dm2_cleanup++;
#if 0 // XXXmiken
    nkn_locmgr2_uri_tbl_cleanup();
    nkn_locmgr2_physid_tbl_cleanup();
    nkn_locmgr2_container_tbl_cleanup();
#endif
}	/* DM2_cleanup */


/*
 * Return 0: if OK to delete
 * Return 1: if not OK to delete
 */
static int
dm2_eviction_check(char		    *uri,
		   MM_delete_resp_t *del,
		   dm2_cache_type_t *ct)
{
    nkn_buffer_t *buf;
    nkn_attr_t	 *attr;
    int32_t	 disk_temp, bm_temp;

    if (del->evict_flag == 0)		// for normal deletes, don't do checks
	return 0;

    glob_dm2_ext_evict_total_cnt++;
    ct->ct_ext_evict_total_call_cnt++;	// Every call adds to total

    if ((buf = nkn_buffer_get(&del->in_uol, NKN_BUFFER_ATTR)) == NULL) {
	DBG_DM2M("[cache_type=%s] External Evict URI: %s BM lookup failed => "
		 "forcing eviction", ct->ct_name, uri);
	ct->ct_ext_evict_object_delete_cnt++;	// Delete will be attempted
	glob_dm2_ext_evict_delete_cnt++;
	glob_dm2_ext_evict_buf_get_failed_cnt++;
	/* This item is very cold, so delete it */
	return 0;
    }

    /* Check if temperature has changed from disk value */
    attr = nkn_buffer_getcontent(buf);
    disk_temp = am_decode_hotness(&del->evict_hotness);
    bm_temp = am_decode_hotness(&attr->hotval);
    if (disk_temp != bm_temp) {
	nkn_buffer_release(buf);
	DBG_DM2M("[cache_type=%s] External Evict URI: %s temp changed (disk=%d "
		 "bm=%d) => no eviction", ct->ct_name, uri, disk_temp, bm_temp);
	return 1;
    }
    nkn_buffer_release(buf);


    ct->ct_ext_evict_object_delete_cnt++;	// Delete will be attempted
    glob_dm2_ext_evict_delete_cnt++;
#if 0
    /* Internal eviction does not depend on AM, so these lines are not needed */
    if (disk_temp < am_decode_hotness(&ct->ct_am_hot_evict_candidate))
	ct->ct_am_hot_evict_candidate = del->evict_hotness;
    if (del->evict_trans_flag == NKN_EVICT_TRANS_STOP) {
	// Call AM function to force AM based eviction
	// Do we need a check for the corresponding START?
	ct->ct_am_hot_evict_threshold = ct->ct_am_hot_evict_candidate;
    }
#endif
    DBG_DM2M("[cache_type=%s] External Evict URI: %s temp unchanged=%d => done",
	     ct->ct_name, uri, disk_temp);
    return 0;
}	/* dm2_eviction_check */


/*
 * Figure out if the URI which we are about to delete is in the currently
 * open block for this container.  If so, then delete the unused pages.
 */
static void
dm2_free_current_container_block(DM2_uri_t *uri_head)
{
    DM2_extent_t *ext;
    off_t	 offset;

    offset = uri_head->uri_container->c_open_dblk.db_start_page *
	uri_head->uri_container->c_open_dblk.db_disk_page_sz;
    ext = dm2_find_extent_by_offset2(uri_head->uri_ext_list, offset);
    if (ext) {
	dm2_return_unused_pages(uri_head->uri_container, NULL);
    }
}	/* dm2_free_current_container_block */


static int
dm2_delete_check(const MM_delete_resp_t *delete,
		 char **uri_ret)
{
    char *uri = NULL;
    int ret = 0;

    if (delete == NULL) {
	DBG_DM2S("ERROR: NULL delete ptr");
	ret = -EINVAL;
	goto dm2_delete_check_error;
    }
    uri = (char *)nkn_cod_get_cnp(delete->in_uol.cod);
    if (uri == NULL || *uri == '\0' || *uri != '/') {
	DBG_DM2S("[URI=%s] ERROR: Bad URI: cod=%ld",
		 *uri != '\0' ? uri : "NULL", delete->in_uol.cod);
	ret = -EINVAL;
	goto dm2_delete_check_error;
    }

    if (dm2_invalid_slash(uri)) {
	DBG_DM2M2("[URI=%s] Illegal URI: cod=%ld", uri, delete->in_uol.cod);
	ret =-EINVAL;
	goto dm2_delete_check_error;
    }

    if (strstr(uri, "/./") || strstr(uri, "/../")) {
	DBG_DM2M2("[URI=%s] Illegal component in URI: cod=%ld",
		  uri, delete->in_uol.cod);
	ret = -EINVAL;
	goto dm2_delete_check_error;
    }
    *uri_ret = uri;
    return 0;

 dm2_delete_check_error:
    glob_dm2_delete_invalid_input_err++;
    *uri_ret = uri;
    return ret;
}	/* dm2_delete_check */


/*
 * - Allocate memory and copy uri_name to be deleted to save_del_uri_name
 * - Allocate memory for URI in save_uri_name (to be used later)
 * - Copy mgmt_name of the URI to be deleted
 */
#define DM2_DELETE_ALLOC_URIS(save_del_uri_name, \
			      save_uri_name, \
			      save_mgmt_name, \
			      save_uri_name_len, \
			      del_uri_head, \
			      this_uri_name, \
			      this_uri_name_len) \
{ \
    this_uri_name_len = strlen(this_uri_name); \
    save_uri_name_len = this_uri_name_len + NAME_MAX; \
    save_del_uri_name = alloca(save_uri_name_len); \
    save_uri_name = alloca(save_uri_name_len); \
    strncpy(save_del_uri_name, this_uri_name, this_uri_name_len); \
    save_del_uri_name[this_uri_name_len] = '\0'; \
    strncpy(save_mgmt_name, del_uri_head->uri_container->c_dev_ci->mgmt_name, \
	    DM2_MAX_MGMTNAME); \
}


static int
dm2_purge_buffers_if_same(char* uri_name,
			  DM2_uri_t *uh,
			  dm2_cache_type_t *ct,
			  int no_vers_check,
			  int local_delete)
{
    nkn_uol_t uol;
    int ret = 0;

    /* Purge URI from BM, including URIs which share the same block */
    uol.cod = nkn_cod_open(uri_name, strlen(uri_name), NKN_COD_DISK_MGR);
    if (uol.cod == NKN_COD_NULL) {
	DBG_DM2M("[cache_type=%s] COD allocation failed (URI=%s)",
		     ct->ct_name, uri_name);
	/* If cod allocation fails, we go ahead and delete the URI in disk
	   as BM might have evicted this URI already */
	glob_dm2_cod_null_cnt++;
	return 0;
    }
    /*
     * no_vers_check - 1, force delete the URI in BM
     *		     - 0, delete the URI in BM only if the versions match
     */
    if (!no_vers_check) {
	ret = nkn_cod_test_and_set_version(uol.cod, uh->uri_version, 0);
	NKN_ASSERT(ret != NKN_COD_INVALID_COD);
	if (ret != 0) {
	    ct->ct_dm2_delete_cod_mismatch_warn++;
	    /* skip deleting this URI in BM */
	    goto end;
	}
    }

    /* Don't purge BM if Dm2 generated the DELETE from PUT */
    if (!local_delete)
        nkn_buffer_purge(&uol);		// there's no return code
end:
    nkn_cod_close(uol.cod, NKN_COD_DISK_MGR);
    uol.cod = NKN_COD_NULL;
    return 0;
}	/* dm2_purge_buffers_if_same */


/*
 * Remove the container and attribute files from the filesystem, given a ptr
 * to the container obj.  Currently, we don't call this routine to cleanup
 * files, so we expect both to exist.
 */
void
dm2_container_attrfile_remove(dm2_container_t *free_cont)
{
    char *attrpath, *contpath;
    int  ret;

    DM2_GET_CONTPATH(free_cont, contpath);
    DM2_GET_ATTRPATH(free_cont, attrpath);

    /*
     * Should we ignore ENOENT?  Since attribute files are created after
     * container files, we delete attribute files first
     */
    if (remove(attrpath) < 0) {
	ret = errno;
	DBG_DM2S("[PATH=%s] remove() failed: %d", attrpath, ret);
	if (ret != ENOENT) {
	    NKN_ASSERT(0);
	    return;
	}
    } else {
	free_cont->c_dev_ci->ci_dm2_remove_attrfile_cnt++;
    }

    if (remove(contpath) < 0) {
	ret = errno;
	DBG_DM2S("[PATH=%s] remove() failed: %d", contpath, ret);
	if (ret != ENOENT) {
	   NKN_ASSERT(0);
	   return;
	}
    } else {
	free_cont->c_dev_ci->ci_dm2_remove_contfile_cnt++;
    }
}	/* dm2_container_attrfile_remove */


/*
 * Destroy a container memory object
 */
void
dm2_container_destroy(dm2_container_t  *free_cont,
		      dm2_cache_type_t *ct)
{
    char *del_dir_path = NULL, *tmp = NULL;
    int ret, len, rmdir_err;
    off_t   offset;

    if (free_cont->c_uri_dir) {
        DBG_DM2M("[cache_type=%s] Container Destroy: %s",
		 ct->ct_name, free_cont->c_uri_dir);
    }
    assert(free_cont->c_magicnum != NKN_CONTAINER_MAGIC_FREE);

    /* clear the attr/ext slot queues */
    while (1) {
	offset = dm2_attr_slot_pop_head(free_cont);
	if (offset == -1)
	    break;
    }
    while (1) {
	offset = dm2_ext_slot_pop_head(free_cont);
	if (offset == -1)
	    break;
    }

    dm2_attr_mutex_destroy(free_cont);
    dm2_slot_mutex_destroy(free_cont);
    dm2_db_mutex_destroy(&free_cont->c_open_dblk);

    dm2_cont_rwlock_destroy(free_cont);

    /* Short circuit here, if we dont have the path filled in yet */
    if (free_cont->c_uri_dir == NULL || free_cont->c_dev_ci == NULL)
	goto container_free;

    /* Form the full path of the dir to remove */
    DM2_FORM_CONT_DIR(free_cont, del_dir_path);
    len = strlen(del_dir_path);

    if (del_dir_path[len-1] == '/')
	del_dir_path[len-1] = '\0';

    /* Delete all the empty directories, rmdir will not remove directories
     * that are not empty */
    while (1) {
	ret = rmdir(del_dir_path);
	if (ret) {
	    rmdir_err = errno;
	    if (rmdir_err != ENOTEMPTY &&
		rmdir_err != EBUSY ) {
		DBG_DM2W("[cache_type=%s] rmdir '%s' failed, errno:%d",
			 ct->ct_name, del_dir_path, rmdir_err);
		glob_dm2_container_rmdir_failed_cnt++;
	    }
	    break;
	}
	glob_dm2_container_rmdir_cnt++;
	tmp = strrchr(del_dir_path, '/');
	*tmp = '\0';
    }

    /* c_uri_dir points to ch->ch_uri_dir, make it NULL */
    free_cont->c_uri_dir = NULL;

 container_free:
    /* Free the container */
    free_cont->c_magicnum = NKN_CONTAINER_MAGIC_FREE;
    glob_dm2_container_freed_cnt++;
    dm2_free(free_cont, free_cont->c_cont_mem_sz, DM2_NO_ALIGN);
}	/* dm2_container_destroy */


/*
 * We grab a write lock on the cont_head to make sure no one can be accessing
 * this object while we delete the container from the list.  If another
 * thread manages to put a new object into our container before we try to
 * delete it, we must skip the deletion.  Because we hold the cont_head
 * write lock, we should also be able to delete the physical container
 * and attribute files because no one should be attempting to stat these
 * files while we hold the write lock.
 */
static void
dm2_container_find_and_free(dm2_container_t  *free_cont,
			    dm2_cache_type_t *ct,
			    char	     *del_uri)
{
    dm2_container_head_t *cont_head;
    dm2_container_t	 *cont;
    GList		 *cont_list, *gcont_ptr;
    int			 cont_wlocked = 0, ch_locked = DM2_CONT_HEAD_UNLOCKED;
    int			 found = 0;

    cont_head = dm2_conthead_get_by_uri_dir_wlocked(free_cont->c_uri_dir,
						    ct, &ch_locked);
    if (cont_head == NULL) {
	NKN_ASSERT(cont_head);
	DBG_DM2S("container lookup failed: %s", free_cont->c_uri_dir);
	glob_dm2_container_free_lookup_err++;
	return;
    }
    cont_list = cont_head->ch_cont_list;
    for (gcont_ptr = cont_list; gcont_ptr; gcont_ptr = gcont_ptr->next) {
	cont = (dm2_container_t *)gcont_ptr->data;
	if (cont == free_cont) {
	    found = 1;
	    break;
	}
    }
    if (!found) {
	dm2_conthead_rwlock_wunlock(cont_head, ct, &ch_locked);
	DBG_DM2S("[cache_type=%s] Container not found in list: %s (head=%s)",
		 ct->ct_name, free_cont->c_uri_dir, cont_head->ch_uri_dir);
	glob_dm2_container_free_not_found_err++;
	return;
    }

    dm2_cont_rwlock_wlock(free_cont, &cont_wlocked, del_uri);
    if (AO_load(&free_cont->c_good_ext_cnt) == 0) {
	/* Remove the container from the cont_list */
	cont_head->ch_cont_list =
	    g_list_remove(cont_head->ch_cont_list, free_cont);

	dm2_container_attrfile_remove(free_cont);
	dm2_container_destroy(free_cont, ct);
	/*
	 * Now that we have removed the container, we can possibly delete
	 * the cont_head memory object.  We should have already deleted the
	 * physical objects.  Once we return from this function, the object
	 * no longer exists and as such no lock is possible.
	 */
	if (cont_head->ch_cont_list == NULL) {
	    if (dm2_conthead_free(cont_head, ct, &ch_locked) != 0)
		dm2_conthead_rwlock_wunlock(cont_head, ct, &ch_locked);
	} else
	    dm2_conthead_rwlock_wunlock(cont_head, ct, &ch_locked);
    } else {
	/* We cannot free the container, as someone ingested an object now */
	DBG_DM2M("[cache_type=%s] Skipping Container Delete: %s",
		 ct->ct_name, free_cont->c_uri_dir);
	glob_dm2_container_skip_free_cnt++;
	dm2_cont_rwlock_wunlock(cont, &cont_wlocked, del_uri);
	/* If error found, then need cont->c_uri_dir */
	dm2_conthead_rwlock_wunlock(cont_head, ct, &ch_locked);
    }
}	/* dm2_container_find_and_free */


#if 0
/* Not needed for now until we add back call sites */
static void
dm2_cont_head_free_empty_containers(dm2_container_head_t *cont_head,
				    dm2_cache_type_t	 *ct,
				    char		 *del_uri)
{
    dm2_container_t *cont;
    GList	    *cptr, *rm_list = NULL;
    int		    cont_wlocked = 0, ret = 0;
    int		    ch_locked = DM2_CONT_HEAD_UNLOCKED;

    dm2_conthead_rwlock_wlock(cont_head, ct, &ch_locked);
    /*
     * Build up list to remove, because you can't remove from a list which
     * is being traversed.
     */
    for (cptr = cont_head->ch_cont_list; cptr; cptr = cptr->next) {
	cont = (dm2_container_t *)cptr->data;
	dm2_cont_rwlock_wlock(cont, &cont_wlocked, del_uri);
	if (AO_load(&cont->c_good_ext_cnt) == 0)
	    rm_list = g_list_prepend(rm_list, cont);
	else
	    dm2_cont_rwlock_wunlock(cont, &cont_wlocked, del_uri);
    }
    /* The containers to be freed are wlocked */
    for (cptr = rm_list; cptr; cptr = cptr->next) {
	cont = (dm2_container_t *)cptr->data;

	cont_head->ch_cont_list = g_list_remove(cont_head->ch_cont_list, cont);
	glob_dm2_cont_head_free_empty_containers++;
	dm2_container_attrfile_remove(cont);
	dm2_container_destroy(cont, ct);
    }

    /* Free up the temp list */
    if (g_list_length(rm_list) > 0) {
        g_list_free(rm_list);
    }
    if (cont_head->ch_cont_list == NULL) {
	ret = dm2_conthead_free(cont_head, ct, &ch_locked);
	if (ret)
	    dm2_conthead_rwlock_wunlock(cont_head, ct, &ch_locked);
    } else {
	dm2_conthead_rwlock_wunlock(cont_head, ct, &ch_locked);
    }
}	/* dm2_cont_head_free_empty_containers */
#endif

int
dm2_do_delete(MM_delete_resp_t *delete,
	      dm2_cache_type_t *ct,
	      DM2_uri_t	       *del_uh,
	      dm2_cache_info_t **bad_ci_ret)
{
    char		 ns_uuid[NKN_MAX_UID_LENGTH];
    DM2_uri_t		 *uh;
    GList		 *uh_obj, *uri_del_list = NULL, *uri_unlock_list = NULL;
    dm2_container_t	 *free_cont = NULL;
    dm2_cache_info_t	 *ci = NULL;
    char		 *save_del_uri_name = NULL,
			 *save_uri_name = NULL,
			 *save_uri_base = NULL,
			 *del_uri_dir = NULL,
			 *del_uri_name = NULL;
    char		 save_mgmt_name[DM2_MAX_MGMTNAME];
    nkn_hot_t		 uri_hotval;
    int			 final_ret = 0, ret = 0, save_uri_name_len, obj_pin = 0;
    int			 delete_pinned = 0, save_uri_base_len = 0;
    int		         del_uri_name_len, orig_hotness;
    uint32_t		 block_size = 0, cache_age = 0, bf_tld = 0;
    int32_t		 occupancy = 0, uri_ext_cnt, ao_ret;
    int32_t		 delete_reason;
    int8_t		 free_pages = 1, flush_block = 0, shutdown_ci;
    int8_t		 no_vers_check = 0, evict_flag = 0, local_delete;
    int8_t		 good_nstoken = 0;
    ns_stat_token_t	 ns_stoken;

    ci = del_uh->uri_container->c_dev_ci;
    block_size = dm2_get_blocksize(ci);

    /* Internal eviction will not fill in the 'MM_delete_resp_t' and
     * a COD will not be involved */
    if (delete) {
	ret = nkn_cod_test_and_set_version(delete->in_uol.cod,
					   del_uh->uri_version, 0);
	no_vers_check = delete->dm2_no_vers_check;
	evict_flag = delete->evict_flag;
	local_delete = delete->dm2_local_delete;
	delete_reason = delete->dm2_del_reason;
    } else {
	evict_flag    = 1;
	no_vers_check = 1;
	local_delete  = 0; //dont expect the object to be in BM during eviction
	delete_reason = ct->ct_dm2_evict_reason;
    }

    if (!evict_flag && del_uh->uri_cache_pinned) {
	/* If the delete request is from CLI (not from eviction or internal)
	 * delete the objects even though if its pinned. If the delete request
	 * is not for a pinned object, do not delete shared pinned objects */
	delete_pinned = 1;
    }

    /* Since the uh->uri_name does not store the full uri string, we have to
     * reconstruct the same. First, lets store the URI container string */
    del_uri_name = alloca(MAX_URI_SIZE);
    del_uri_dir = alloca(MAX_URI_SIZE);

    dm2_uh_uri_dirname(del_uh, del_uri_dir, 1);

    if ((ret = dm2_uridir_to_nsuuid(del_uri_dir, ns_uuid)) != 0) {
	DBG_DM2S("Can not get Namespace ID: %s", del_uri_dir);
	dm2_uri_head_rwlock_wunlock(del_uh, ct, DM2_NOLOCK_CI);
        return ret;
    }

    ns_stoken = dm2_ns_get_stat_token_from_uri_dir(del_uri_dir);
    if ((ret = ns_is_stat_token_null(ns_stoken))) {
	DBG_DM2W("Stats not collected for namespace - can not get token: %s",
		 del_uri_dir);
    } else {
	good_nstoken = 1;
    }

    /* Get the block free threshold for this namespace */
    ret = ns_stat_get_tier_block_free_threshold(ns_uuid, ct->ct_ptype, &bf_tld);
    if (ret == ENOENT) {
	bf_tld = -1;
    } else if (ret) {
	/* Not currently used */
	dm2_uri_head_rwlock_wunlock(del_uh, ct, DM2_NOLOCK_CI);
	DBG_DM2S("Can not get block free threshold: %s", del_uri_dir);
	if (good_nstoken)
	    ns_stat_free_stat_token(ns_stoken);
	return ret;
    }
    /*
     * Get the list of URIs that might need to be deleted from this
     * block. If there are more than one URI in this block, try to delete
     * only the requested URI. If the block occupancy falls below the
     * threshold set, delete all the URIs in this block to free it up.
     */
    if (del_uh->uri_content_len < block_size)
	dm2_get_uri_del_list(ct, del_uh, &occupancy, &uri_del_list,
			     delete_pinned, &obj_pin);
    else
	dm2_get_uri_del_list(ct, del_uh, NULL, &uri_del_list,
			     delete_pinned, &obj_pin);

    if (uri_del_list == NULL && obj_pin) {
	/* We cannot delete any objects as atleast one object is pinned */
	final_ret = -EPERM;
	goto do_delete_exit;
    }

    /*
     * Dont delete shared objects if the delete request is for a pinned object.
     * If namespace is deleted, bf_tld == -1 => delete everything.
     */
    if (bf_tld == -1 ||
	((del_uh->uri_cache_pinned == 0) &&
	 ((occupancy * 100) / block_size <= (uint32_t)bf_tld)))
	flush_block = 1;

    DM2_DELETE_ALLOC_URIS(save_del_uri_name, save_uri_name, save_mgmt_name,
			  save_uri_name_len, del_uh,
			  dm2_uh_uri_basename(del_uh), del_uri_name_len);
    if (uri_del_list == NULL && !obj_pin) {
	/*
	 * List will be NULL when a PUT error causes the extent to be missing
	 * from the physid extent list.  The extent should still be placed
	 * on the del_uh->uri_ext_list.  See bug 2745 for a specific example.
	 */
	if ((ret = dm2_free_uri_head_space(del_uh, ct, ns_stoken)) < 0) {
	    /* If this operation fails, there may be an internal
	     * inconsistency.  Only bugs or memory allocation errors or
	     * I/O errors should cause a failure. */
	    DBG_DM2S("[cache_type=%s] Failure while freeing uri "
		     "head/updating free map for %s (%s): %d",
		     ct->ct_name, save_uri_name, save_del_uri_name, -ret);
	    final_ret = ret;
	    del_uh->uri_bad_delete = 1;
	    dm2_evict_delete_uri(del_uh,am_decode_hotness(&del_uh->uri_hotval));
	    glob_dm2_delete_bad_uri_skip_evict++;
	    DBG_DM2S("[cache_type=%s] Removed URI %s/%s (disk:%s) from "
		     "evict list", ct->ct_name, del_uri_dir, save_del_uri_name,
		     ci->mgmt_name);
	    uri_unlock_list = g_list_prepend(uri_unlock_list, del_uh);
	}
    }
    for (uh_obj = uri_del_list; uh_obj; uh_obj = uh_obj->next) {
	shutdown_ci = 0;
	free_pages = 1;
	uh = (DM2_uri_t *)uh_obj->data;
	save_uri_base = dm2_uh_uri_basename(uh);
	save_uri_base_len = strlen(save_uri_base);
	strncpy(save_uri_name, save_uri_base, save_uri_base_len);
	save_uri_name[save_uri_base_len] = '\0';

	if (!flush_block &&
		strncmp(save_uri_name, save_del_uri_name, save_uri_name_len)) {
	    /* Skip if not explicit delete of uh->uri_name and we aren't
	     * flushing entire block */
	    uh->uri_deleting = 0;
	    dm2_uri_head_rwlock_unlock(uh, ct, DM2_NOLOCK_CI);
	    continue;
	}

	strcpy(del_uri_name, del_uri_dir);
	strcat(del_uri_name, save_uri_name);

       /*
	* dm2_no_vers_check is set when deleting an object from CLI.
	* BM will delete the expired object and version will mismatch
	* when a new object has already been loaded in BM.
	*/
	ret = dm2_purge_buffers_if_same(del_uri_name, uh,
					ct, no_vers_check,
					local_delete);
	if (ret != 0) {
	    uh->uri_deleting = 0;
	    dm2_uri_head_rwlock_unlock(uh, ct, DM2_NOLOCK_CI);
	    continue;
	}

	glob_dm2_delete_uri_cnt++;

	if (!local_delete) {
	    /* Delete AM entry, only if the DELETE is from external source
	     * or eviction */
	    if ((ret = AM_delete_obj(NKN_COD_NULL, del_uri_name)) < 0) {
		/* This object isn't necessarily in AM, so we can't print
		 * an error all the time */
		DBG_DM2W("[cache_type=%s] AM Delete failed for %s: %d",
			 ct->ct_name, del_uri_name, ret);
	    }
	}

	/* Get the total extents used up by this URI */
	uri_ext_cnt = dm2_uri_physid_cnt(uh->uri_ext_list);

	if ((ret = dm2_stampout_attr(uh)) < 0) {
	    DBG_DM2S("[cache_type=%s] Failure while deleting disk attribute "
		     "for %s (%s): %d", ct->ct_name, del_uri_name,
		     save_del_uri_name, -ret);
	    final_ret = ret;
	    uh->uri_bad_delete = 1;
	    /*
 	     * Disable the drive only if IO error happens (EIO)
 	     */
	    if (ret == -EIO)
		shutdown_ci = 1;
	    /* try to move forward even though we got an error */
	}

	if ((ret = dm2_stampout_extents(uh)) < 0) {
	    DBG_DM2S("[cache_type=%s] Failure while deleting disk extents for"
		     " %s (%s): %d", ct->ct_name, del_uri_name,
		     save_del_uri_name, -ret);
	    final_ret = ret;
	    /* try to move forward even though we got an error.  If all
	     * disk extents can not be deleted, we cannot free up the disk
	     * blocks */
	    free_pages = 0;
	    uh->uri_bad_delete = 1;
	    /*
 	     * Disable the drive only if IO error happens (EIO)
 	     */
	    if (ret == -EIO)
		shutdown_ci = 1;
	} else {
	    ao_ret = AO_fetch_and_add(&uh->uri_container->c_good_ext_cnt,
				      -uri_ext_cnt);
	    if ((ao_ret - uri_ext_cnt) == 0)
		free_cont = uh->uri_container;
	}

	if (free_pages &&
	    (ret = dm2_delete_physid_extents(uh, ct, ns_stoken)) < 0) {
	    /* If this operation fails, there is an internal inconsistency.
	     * Only bugs should cause a failure. */
	    DBG_DM2S("[cache_type=%s] Failure while deleting mem extents for"
		     " %s (%s): %d", ct->ct_name, del_uri_name,
		     save_del_uri_name, -ret);
	    assert(0);
	    final_ret = ret;
	    uh->uri_bad_delete = 1;
	    /* try to move forward even though we got an error */
	}

	DBG_DM2M("[cache_type=%s] Deleted URI: %s evictf=0x%x",
		 ct->ct_name, del_uri_name, evict_flag);

	if (free_pages && uh->uri_bad_delete == 0)
	    dm2_free_current_container_block(uh);
	else if (dm2_matching_physid(uh))
	    uh->uri_container->c_open_dblk.db_flags |= DM2_DB_DELETE;

	ct->ct_dm2_delete_bytes += uh->uri_content_len;
	if (evict_flag)
	    ct->ct_dm2_evict_bytes += uh->uri_content_len;
	uri_hotval = 0; //reset previous value
	uri_hotval = uh->uri_hotval;
	orig_hotness = uh->uri_orig_hotness;
	cache_age = nkn_cur_ts - uh->uri_cache_create;

	if (uh->uri_bad_delete == 0) {
	    if ((ret = dm2_free_uri_head_space(uh, ct, ns_stoken)) < 0) {
		/* If this operation fails, there may be an internal
		 * inconsistency.  Only bugs or memory allocation errors or
		 * I/O errors should cause a failure. */
		DBG_DM2S("[cache_type=%s] Failure while freeing uri "
			 "head/updating free map for %s (%s): %d",
			 ct->ct_name, del_uri_name, save_del_uri_name, -ret);
		final_ret = ret;
		uh->uri_bad_delete = 1;
		dm2_evict_delete_uri(uh,am_decode_hotness(&uh->uri_hotval));
		glob_dm2_delete_bad_uri_skip_evict++;
		uri_unlock_list = g_list_prepend(uri_unlock_list, uh);
		AO_fetch_and_add(&uh->uri_container->c_good_ext_cnt,
				 uri_ext_cnt);
		free_cont = NULL;
		continue;
	    }
	    uh = NULL;
	} else {
	    /* Remove this URI from th evict list, as we will never be able to
	     * delete it and if it stays in the list, eviction will just loop
	     * on this URI
	     */
	    dm2_evict_delete_uri(uh, am_decode_hotness(&uh->uri_hotval));
	    glob_dm2_delete_bad_uri_skip_evict++;
	    DBG_DM2S("[cache_type=%s] Removed URI %s (disk:%s) from evict list",
		     ct->ct_name, del_uri_name, ci->mgmt_name);
	    uri_unlock_list = g_list_prepend(uri_unlock_list, uh);
	    if (bad_ci_ret && shutdown_ci) {
		*bad_ci_ret = uh->uri_container->c_dev_ci;
		(*bad_ci_ret)->ci_disabling = 1;
	    }
	}

	if (! strncmp(save_uri_name, save_del_uri_name, save_uri_name_len)) {
	    dm2_cache_log_delete(save_uri_name, del_uri_dir, save_mgmt_name,
				orig_hotness, uri_hotval, cache_age,
				delete_reason, ct);
	} else {
	    dm2_cache_log_delete(save_uri_name, del_uri_dir, save_mgmt_name,
				 orig_hotness, uri_hotval, cache_age,
				 (delete_reason << 16) |
				 NKN_DEL_REASON_SHARED, ct);
	}

	if (good_nstoken)
	    dm2_ns_update_total_objects(ns_stoken, save_mgmt_name, ct, 0);

	/* increment ci counter for external eviction alone */
	if (delete && evict_flag && ci)
	    ci->ci_dm2_ext_evict_total_cnt++;

	if (uh && uh->uri_bad_delete) {
	    /* We did not free the uri_head and so reset the good_ext_cnt
	     * in the container, also the container cannot be freed anymore */
	    AO_fetch_and_add(&uh->uri_container->c_good_ext_cnt, uri_ext_cnt);
	    free_cont = NULL;
	}
    }
    if (g_list_length(uri_unlock_list) > 0) {
	/* Unlock any URI heads which are still write locked before returning */
	dm2_unlock_uri_del_list(ct, uri_unlock_list);
	g_list_free(uri_unlock_list);
    }
    if (uri_del_list)
        g_list_free(uri_del_list);

    /* Do we need to free a container and possibly the container head */
    if (free_cont)
	dm2_container_find_and_free(free_cont, ct, del_uri_name);

 do_delete_exit:
    if (good_nstoken)
	ns_stat_free_stat_token(ns_stoken);
    return final_ret;
}   /* dm2_do_delete */


int
DM2_type_delete(MM_delete_resp_t *delete,
		dm2_cache_type_t *ct,
		dm2_cache_info_t **bad_ci_ret)
{
    dm2_container_head_t *cont_head = NULL;
    DM2_uri_t		 *uri_head;
    GList		 *uri_del_list = NULL, *uri_unlock_list = NULL;
    dm2_container_t	 *cont, *uri_cont;
    dm2_cache_info_t	 *ci = NULL;
    char		 *del_uri = NULL;
    int			 ret = 0;
    int			 mutex_locked_stat = 0;
    int			 valid_cont_found = 0, delete_pinned = 0;
    int			 cont_wlocked = 0, ci_rlocked = 0, macro_ret = 0;
    int			 cont_head_locked = DM2_CONT_HEAD_UNLOCKED;
    int8_t		 mutex_locked_delete = 0;
    int8_t		 num_looped = 0, ct_locked = 0;

    glob_dm2_delete_cnt++;
    if ((ret = dm2_delete_check(delete, &del_uri)) != 0)
	goto dm2_delete_error;

    if (dm2_eviction_check(del_uri, delete, ct))
	return -NKN_DM2_EVICTION_SKIPPED;

    if (ct->ct_tier_preread_done == 0) {
	/* Pre-read thread still working */
	ret = -NKN_SERVER_BUSY;
	glob_dm2_api_server_busy_err++;
	ct->ct_dm2_delete_not_ready++;
	goto dm2_delete_preread_exit;
    }

    if (delete->dm2_is_mgmt_locked == 0) {
	dm2_ct_rwlock_rlock(ct);
	ct_locked = 1;
    }

 delete_loop:
    /* Get the uri_head along with ci rlocked */
    uri_head = dm2_uri_head_get_by_uri_wlocked(del_uri, cont_head,
					       &cont_head_locked, ct, 1,
					       DM2_RLOCK_CI);
    if (uri_head == NULL) {
	 if (ct->ct_tier_preread_done && ct->ct_tier_preread_stat_opt) {
            /* As long as ct_tier_preread_stat_opt is set and preread is done,
             * we know that there are no directories which are not read */
            ct->ct_dm2_delete_opt_not_found++;
            goto dm2_delete_not_found;
        }

	dm2_ct_stat_mutex_lock(ct, &mutex_locked_stat, del_uri);
	if ((ret = dm2_uri_head_lookup_by_uri(del_uri, cont_head,
					      &cont_head_locked,
					      ct, 0)) < 0) {
	    if (ret == -EBUSY)
		goto dm2_delete_error;
	    NKN_ASSERT(ret == -ENOENT);
	} else {
	    /* this case is the race & should almost never be hit */
	    dm2_ct_stat_mutex_unlock(ct, &mutex_locked_stat, del_uri);
	    num_looped++;
	    goto delete_loop;
	}

	/*
	 * If we can not get a container list, then we can not read in
	 * the containers which means we can not figure out if this
	 * URI exists.  If we can not read in all the containers or we
	 * can not read in the attributes, we should still be able to
	 * delete this object; however, this is also a sign that one
	 * of the caches is corrupted or there is some bug.
	 */
	ret = dm2_get_contlist(del_uri, 0, NULL, ct, &valid_cont_found,
			       &cont_head, &cont, &cont_wlocked,
			       &cont_head_locked);
	if (ret < 0)
	    goto dm2_delete_error;
	NKN_ASSERT(cont_wlocked == 0);	/* create can not happen here */

	if (valid_cont_found == 0)
	    goto dm2_delete_not_found;

	if ((ret = dm2_read_contlist(cont_head, 0, ct)) < 0)
	    goto dm2_delete_error;

	ret = dm2_read_container_attrs(cont_head, &cont_head_locked, 0, ct);
	if (ret < 0)
	    goto dm2_delete_error;

	uri_head = dm2_uri_head_get_by_uri_wlocked(del_uri, cont_head,
						   &cont_head_locked,
						   ct, 1, DM2_RLOCK_CI);
	if (uri_head == NULL)
	    goto dm2_delete_not_found;

	dm2_conthead_rwlock_unlock(cont_head, ct, &cont_head_locked);
	dm2_ct_stat_mutex_unlock(ct, &mutex_locked_stat, del_uri);
    }
    NKN_ASSERT(uri_head);
    NKN_ASSERT(uri_head->uri_deleting);
    NKN_ASSERT(mutex_locked_stat == 0);
    ci = uri_head->uri_container->c_dev_ci;
    /* Remember the container pointer, as this will be needed to do a lookup
     * on the specific disk uri hash table. The pointer is not used for anything
     * else and no lock is grabbed on it
     */
    uri_cont = uri_head->uri_container;
    dm2_uri_log_state(uri_head, DM2_URI_OP_DEL, 0, 0,
		      gettid(), nkn_cur_ts);

    /* We have to grab the delete mutex on the disk before
     * we do anything else. We cannot be holding any other locks
     * while we are grabbing the delete lock. Release the uri wlock
     * here and will have to re-lookup the CH & the uri after the delete
     * mutex is grabbed. */
    dm2_uri_head_rwlock_wunlock(uri_head, ct, DM2_RUNLOCK_CI);
    uri_head = NULL;
    cont_head = NULL;
    NKN_MUTEX_LOCKR(&ci->ci_dm2_delete_mutex);
    mutex_locked_delete = 1;

    dm2_ci_dev_rwlock_rlock(ci);
    ci_rlocked = 1;

    /* Since we know the disk the object exists, lookup on that disk alone */
    uri_head = dm2_uri_head_get_by_ci_uri_wlocked(del_uri, cont_head,
						  &cont_head_locked, uri_cont,
						  ci, ct, 1);
    if (uri_head == NULL)
	goto dm2_delete_not_found;

    dm2_uri_log_state(uri_head, DM2_URI_OP_DEL, 0, 0,
		      gettid(), nkn_cur_ts);

    if (uri_head->uri_bad_delete) {
	DBG_DM2M("[cache_type=%s] DELETE rejected for bad URI: %s",
		 ct->ct_name, uri_head->uri_name);
	ret = -EPERM;
	ct->ct_dm2_delete_bad_uri_err++;
	dm2_uri_head_rwlock_wunlock(uri_head, ct, 0);
	goto dm2_delete_error;
    }

    /* If the object is pinned and the del request is not for eviction
     * delete the pinned object */
    if (delete && !delete->evict_flag)
	    delete_pinned = 1;

    if (!delete_pinned && uri_head->uri_cache_pinned) {
	DBG_DM2M("[cache_type=%s] DELETE skipped for pinned URI: %s",
		 ct->ct_name, uri_head->uri_name);
	ret = -EPERM;
	dm2_uri_head_rwlock_wunlock(uri_head, ct, 0);
	goto dm2_delete_error;
    }

    ret = dm2_do_delete(delete, ct, uri_head, bad_ci_ret);
    /* cont_head == NULL */

    NKN_ASSERT(!cont_wlocked);
    if (cont_wlocked)			// Should never happen
	dm2_cont_rwlock_wunlock(cont, &cont_wlocked, del_uri);
    if (cont_head_locked != DM2_CONT_HEAD_UNLOCKED)	// Should never happen
	dm2_conthead_rwlock_unlock(cont_head, ct, &cont_head_locked);
    if (ci_rlocked)
	dm2_ci_dev_rwlock_runlock(ci);
    if (mutex_locked_delete)
	NKN_MUTEX_UNLOCKR(&ci->ci_dm2_delete_mutex);
    if (ct_locked)
	dm2_ct_rwlock_runlock(ct);
    return ret;

 dm2_delete_preread_exit:
    glob_dm2_delete_skip_preread_warn++;
    DBG_DM2M("[cache_type=%s] Delete skipped for URI (%s): %d",
	     ct->ct_name, del_uri, ret);
    return ret;

 dm2_delete_not_found:
    glob_dm2_delete_not_found_warn++;
    if (delete->evict_flag)
	ct->ct_ext_evict_object_enoent_cnt++;
    assert(g_list_length(uri_unlock_list) == 0);
    g_list_free(uri_del_list);
    if (cont_wlocked)
	dm2_cont_rwlock_wunlock(cont, &cont_wlocked, del_uri);
    if (cont_head_locked != DM2_CONT_HEAD_UNLOCKED)
	dm2_conthead_rwlock_unlock(cont_head, ct, &cont_head_locked);
    if (mutex_locked_stat)
	dm2_ct_stat_mutex_unlock(ct, &mutex_locked_stat, del_uri);
#if 0
    /* For now, let's just leak this until there is a better solution to
     * handle cont_head delete races */
    if (cont_head)
	dm2_cont_head_free_empty_containers(cont_head, ct, del_uri);
#endif
    if (ci_rlocked)
	dm2_ci_dev_rwlock_runlock(ci);
    if (mutex_locked_delete)
	NKN_MUTEX_UNLOCKR(&ci->ci_dm2_delete_mutex);
    if (ct_locked)
	dm2_ct_rwlock_runlock(ct);
    DBG_DM2M("[cache_type=%s] Delete URI not found: %s", ct->ct_name, del_uri);
    return -ENOENT;

 dm2_delete_error:
    glob_dm2_delete_err++;
    if (delete->evict_flag)
	ct->ct_ext_evict_object_delete_fail_cnt++;
    if (delete->evict_flag && ci)
        ci->ci_dm2_ext_evict_fail_cnt++;
    assert(g_list_length(uri_unlock_list) == 0);
    g_list_free(uri_del_list);
    if (cont_wlocked)
	dm2_cont_rwlock_wunlock(cont, &cont_wlocked, del_uri);
    if (cont_head_locked != DM2_CONT_HEAD_UNLOCKED)
	dm2_conthead_rwlock_unlock(cont_head, ct, &cont_head_locked);
    if (mutex_locked_stat)
	dm2_ct_stat_mutex_unlock(ct, &mutex_locked_stat, del_uri);
    if (ci_rlocked)
	dm2_ci_dev_rwlock_runlock(ci);
    if (mutex_locked_delete)
	NKN_MUTEX_UNLOCKR(&ci->ci_dm2_delete_mutex);
    if (ct_locked)
	dm2_ct_rwlock_runlock(ct);
    DBG_DM2E("[cache_type=%s] Delete URI (%s) error: %d",
	     ct->ct_name, del_uri, ret);
    return ret;
}	/* DM2_type_delete */


/*
 * Delete a specific URI.  Doing this may involve the deletion of other URIs
 * which share common disk blocks.
 */
int
DM2_delete(MM_delete_resp_t *delete)
{
    uint64_t	     init_ticks;
    dm2_cache_type_t *ct;
    dm2_cache_info_t *bad_ci = NULL;
    int		     ret = 0, idx, first_ret = 1;
    int		     dec_counter = 0;

    if (delete == NULL) {
	ret = -EINVAL;
	goto error;
    }

    if (delete->in_sub_ptype < 0 ||
	delete->in_sub_ptype > glob_dm2_num_cache_types) {
	DBG_DM2S("Illegal sub_ptype: %d", delete->in_sub_ptype);
	ret = -EINVAL;
	goto error;
    }

    idx = dm2_cache_array_map_ret_idx(delete->in_ptype);
    if (idx < 0) {
	DBG_DM2S("Illegal ptype: %d", delete->in_ptype);
	ret = -EINVAL;
	goto error;
    }
    init_ticks = LATENCY_START();

    ct = &g_cache2_types[idx];
    AO_fetch_and_add1(&ct->ct_dm2_delete_curr_cnt);
    dec_counter = 1;
    delete->dm2_local_delete = 0; //external delete
    /*
     * In a normal case, this loop should terminate with -ENOENT returned
     * because we eventually run out of objects to delete.  However, if we
     * get an error != -ENOENT, we need to terminate and assume that nothing
     * else can be deleted.
     */
    while (ret == 0) {
	ret = DM2_type_delete(delete, ct, &bad_ci);
	if (first_ret == 1)
	    first_ret = ret;
    }

    LATENCY_END(&ct->ct_dm2_delete_latency, init_ticks);
 error:
    delete->out_ret_code = (first_ret < 0 ? -first_ret : first_ret);
    if (ret == -EIO && bad_ci) {
	DBG_DM2S("Automatic 'Cache Disable' due to apparent drive failure: "
		 "name=%s serial_num=%s type=%s",
		 bad_ci->mgmt_name, bad_ci->serial_num,
		 bad_ci->ci_cttype->ct_name);
	dm2_mgmt_cache_disable(bad_ci);
	nvsd_disk_mgmt_force_offline(bad_ci->serial_num, bad_ci->mgmt_name);
    }
    if (dec_counter)
	AO_fetch_and_sub1(&ct->ct_dm2_delete_curr_cnt);
    return first_ret;
}	/* DM2_delete */


int
DM2_shutdown(void)
{
    return 0;
}

int
DM2_discover(struct mm_provider_priv *p __attribute((unused)))
{
    return 0;
}


void
DM2_promote_complete(MM_promote_data_t *dm2_data)
{
    /* To make WERROR happy */
    if(!dm2_data)
	return;
    return;
}

void
dm2_ct_reset_block_share_threshold(int ptype)
{
    dm2_block_share_prcnt[ptype] = 0;
    return;
}   /* dm2_ct_reset_block_share_threshold */

static uint32_t
dm2_evict_internal_disk_percentage(int ptype)
{
    switch (ptype) {
	case SolidStateMgr_provider:
	case SAS2DiskMgr_provider:
	case SATADiskMgr_provider:
	    return dm2_internal_num_percent[ptype];

	default:
	    DBG_DM2S("Illegal ptype: %d", ptype);
	    return 100;
    }
}   /* dm2_evict_internal_disk_percentage */

static uint32_t
dm2_ct_internal_block_share_threshold(int ptype)
{
    switch (ptype) {
	/* Block sharing is not required for SSD */
	case SolidStateMgr_provider:
	    return 0;
	case SAS2DiskMgr_provider:
	case SATADiskMgr_provider:
	    return dm2_block_share_prcnt[ptype];

	default:
	    DBG_DM2S("Illegal ptype: %d", ptype);
	    return 25;
    }

}   /* dm2_ct_internal_block_share_threshold */

static uint32_t
dm2_evict_internal_high_water_mark(int ptype)
{
    switch (ptype) {
	case SAS2DiskMgr_provider:
	    return dm2_internal_high_watermark[SAS2DiskMgr_provider];
	case SATADiskMgr_provider:
	    return dm2_internal_high_watermark[SATADiskMgr_provider];
	case SolidStateMgr_provider:
	    return dm2_internal_high_watermark[SolidStateMgr_provider];
	default:
	    DBG_DM2S("Illegal ptype: %d", ptype);
	    return 98;
    }
}	/* dm2_evict_internal_high_water_mark */

static int32_t
dm2_evict_internal_time_low_water_mark(int ptype)
{
    switch (ptype) {
        case SAS2DiskMgr_provider:
            return dm2_internal_time_low_watermark[SAS2DiskMgr_provider];
        case SATADiskMgr_provider:
            return dm2_internal_time_low_watermark[SATADiskMgr_provider];
        case SolidStateMgr_provider:
            return dm2_internal_time_low_watermark[SolidStateMgr_provider];
        default:
            DBG_DM2S("Illegal ptype: %d", ptype);
            return -1;
    }
}       /* dm2_evict_internal_time_low_water_mark */


static uint32_t
dm2_evict_internal_low_water_mark(int ptype)
{
    switch (ptype) {
	case SAS2DiskMgr_provider:
	    return dm2_internal_low_watermark[SAS2DiskMgr_provider];
	case SATADiskMgr_provider:
	    return dm2_internal_low_watermark[SATADiskMgr_provider];
	case SolidStateMgr_provider:
	    return dm2_internal_low_watermark[SolidStateMgr_provider];
	default:
	    DBG_DM2S("Illegal ptype: %d", ptype);
	    return 90;
    }
}	/* dm2_evict_internal_low_water_mark */

static int32_t
dm2_ct_evict_hour_threshold(int ptype)
{
    switch (ptype) {
	case SAS2DiskMgr_provider:
	    return dm2_evict_hour[SAS2DiskMgr_provider];
	case SATADiskMgr_provider:
	    return dm2_evict_hour[SATADiskMgr_provider];
	case SolidStateMgr_provider:
	    return dm2_evict_hour[SolidStateMgr_provider];
	default:
	    DBG_DM2S("Illegal ptype: %d", ptype);
	    return -1;
    }
}	/* dm2_ct_evict_hour_threshold */

static int32_t
dm2_ct_evict_min_threshold(int ptype)
{
    switch (ptype) {
	case SAS2DiskMgr_provider:
	    return dm2_evict_min[SAS2DiskMgr_provider];
	case SATADiskMgr_provider:
	    return dm2_evict_min[SATADiskMgr_provider];
	case SolidStateMgr_provider:
	    return dm2_evict_min[SolidStateMgr_provider];
	default:
	    DBG_DM2S("Illegal ptype: %d", ptype);
	    return -1;
    }
}	/* dm2_ct_evict_min_threshold */



/*
 * Return some stats for the given cache type.
 */
int
DM2_provider_stat(MM_provider_stat_t *pstat)
{
    dm2_cache_type_t *ct;
    dm2_cache_info_t *ci;
    GList	     *ci_obj;
    size_t	     tot_avail, tot_free, tot_resv_free;
    int		     idx, state = 0, ret = 0;
    int		     dec_counter = 0;
    int		     block_size = 0, tmp_block_size;

    if (pstat == NULL) {
	ret = -EINVAL;
	goto error;
    }

    if (pstat->sub_ptype < 0 ||
	pstat->sub_ptype > glob_dm2_num_cache_types) {

	DBG_DM2S("Illegal sub_ptype: %d", pstat->sub_ptype);
	ret = -EINVAL;
	goto error;
    }

    if ((idx = dm2_cache_array_map_ret_idx(pstat->ptype)) < 0) {
	DBG_DM2S("Illegal ptype: %d", pstat->ptype);
	ret = -EINVAL;
	goto error;
    }

    ct = &g_cache2_types[idx];
    AO_fetch_and_add1(&ct->ct_dm2_prov_stat_curr_cnt);
    dec_counter = 1;

    tot_avail = tot_free = tot_resv_free = 0;
    dm2_ct_info_list_rwlock_rlock(ct);
    for (ci_obj = ct->ct_info_list; ci_obj; ci_obj = ci_obj->next) {
	ci = (dm2_cache_info_t *)ci_obj->data;
	tmp_block_size = dm2_get_blocksize(ci);
	if (block_size < tmp_block_size)
	    block_size = tmp_block_size;
	tot_avail += ci->set_cache_blk_cnt;
	tot_free += ci->bm.bm_num_blk_free;
	tot_resv_free += AO_load(&ci->bm.bm_num_resv_blk_free);
	if (ci->state_overall == DM2_MGMT_STATE_CACHE_RUNNING) {
	    state = 1;
	}
    }
    dm2_ct_info_list_rwlock_runlock(ct);
    pstat->weight = ct->ct_weighting;
    pstat->hotness_threshold = ct->ct_am_hot_evict_threshold;
    pstat->high_water_mark = ct->ct_internal_high_water_mark;
    pstat->low_water_mark = ct->ct_internal_low_water_mark;
    pstat->max_attr_size = DM2_MAX_ATTR_DISK_SIZE;
    pstat->block_size = block_size;

    if (tot_avail != 0)
	pstat->percent_full = ((tot_avail - tot_free) * 100) / tot_avail;
    else
	pstat->percent_full = 0;
    DBG_DM2M2("[cache_type=%s] Weight=%d  percent_full=%d  total_free=%ld  "
	     "total_resv_free=%ld total_available=%ld", ct->ct_name,
	     pstat->weight, pstat->percent_full, tot_free, tot_resv_free,
	     tot_avail);

    /* If any of the disks belonging to this tier are enabled,
     * the state is enabled. */
    pstat->caches_enabled = state;
 error:
    pstat->out_ret_code = (ret < 0 ? -ret : ret);
    if (dec_counter)
	AO_fetch_and_sub1(&ct->ct_dm2_prov_stat_curr_cnt);
    return ret;
}	/* DM2_provider_stat */


static int
dm2_attr_update_check(const MM_update_attr_t *update_attr,
		      char		     **uri_ret)
{
    nkn_attr_t *dattr;
    char *uri;
    int ret = 0;

    if (update_attr == NULL || update_attr->in_attr == NULL ||
	update_attr->uol.cod == NKN_COD_NULL ||
	(update_attr->in_attr->na_magic != NKN_ATTR_MAGIC) ||
	update_attr->in_attr->na_version != NKN_ATTR_VERSION) {

	DBG_DM2S("Illegal input to attribute update");
	NKN_ASSERT(update_attr);
	NKN_ASSERT(update_attr->in_attr);
	NKN_ASSERT(update_attr->uol.cod != NKN_COD_NULL);;
	NKN_ASSERT(update_attr->in_attr->na_magic == NKN_ATTR_MAGIC);
	NKN_ASSERT(update_attr->in_attr->na_version == NKN_ATTR_VERSION);

	ret = -EINVAL;
	goto attr_check_error;
    }
    dattr = update_attr->in_attr;
    uri = (char *)nkn_cod_get_cnp(update_attr->uol.cod);
    if (uri == NULL || *uri == '\0') {
	DBG_DM2S("URI not found from cod: NULL");
	NKN_ASSERT(uri != NULL);
	ret = -NKN_DM2_BAD_URI;
	goto attr_check_error;
    }
    if (uri[0] != '/') {
	DBG_DM2S("[URI=%s] Illegal URI from cod", uri);
	NKN_ASSERT(uri[0] == '/');
	ret = -NKN_DM2_BAD_URI;
	goto attr_check_error;
    }

    if (dm2_invalid_slash(uri)) {
	DBG_DM2M2("[URI=%s] Illegal URI from cod", uri);
	ret = -NKN_DM2_BAD_URI;
	goto attr_check_error;
    }

    if (dattr && ((uint64_t)dattr % DEV_BSIZE != 0)) {
	DBG_DM2S("[URI=%s] Illegal attribute buffer: %p", uri, dattr);
	ret = -NKN_DM2_INTERNAL_ISSUE;
	goto attr_check_error;
    }
    if (dattr && (dattr->total_attrsize % 8 != 0)) {
	DBG_DM2S("[URI=%s] Illegal attribute size: %d",
		 uri, dattr->total_attrsize);
	ret = -NKN_DM2_INTERNAL_ISSUE;
	goto attr_check_error;
    }
    if (dattr && dattr->content_length == 0) {
	/* This is legal for a PUT but not for an UPDATE */
	DBG_DM2S("[URI=%s] Valid attribute object but invalid content_length "
		 "of 0", uri);
	ret = -EINVAL;
	goto attr_check_error;
    }
    if (dattr && dattr->total_attrsize > DM2_MAX_ATTR_DISK_SIZE) {
	DBG_DM2W("[URI=%s] Invalid total attribute length=%d/blob size=%d",
		 uri, dattr->total_attrsize, dattr->blob_attrsize);
	DBG_CACHELOG("[URI=%s] Invalid total attribute length=%d/blob size=%d",
		     uri, dattr->total_attrsize, dattr->blob_attrsize);
	glob_dm2_put_invalid_total_attr_size_err++;
	ret = -NKN_DM2_ATTR_TOO_LARGE;
	goto attr_check_error;
    }

    if (strstr(uri, "/./") || strstr(uri, "/../")) {
	DBG_DM2S("[URI=%s] Illegal component in URI", uri);
	ret = -NKN_DM2_BAD_URI;
	goto attr_check_error;
    }
    *uri_ret = uri;
    return 0;

 attr_check_error:
    glob_dm2_attr_update_invalid_input_err++;
    return ret;
}	/* dm2_attr_update_check */


static int
dm2_caches_running(dm2_cache_type_t *ct)
{
    GList *ci_obj;
    dm2_cache_info_t *ci;

    dm2_ct_info_list_rwlock_rlock(ct);
    for (ci_obj = ct->ct_info_list; ci_obj; ci_obj = ci_obj->next) {
	ci = (dm2_cache_info_t *)ci_obj->data;
	if (ci->state_overall == DM2_MGMT_STATE_CACHE_RUNNING) {
	    dm2_ct_info_list_rwlock_runlock(ct);
	    return 1;
	}
    }
    dm2_ct_info_list_rwlock_runlock(ct);
    return 0;
}	/* dm2_caches_running */


static int
dm2_update_attr_version_check(MM_update_attr_t	*update_attr,
			      DM2_uri_t		*uri_head,
			      dm2_cache_type_t	*ct)
{
    uint64_t v0, v1;
    int ret = 0;

    ret = nkn_cod_test_and_set_version(update_attr->uol.cod,
				       uri_head->uri_version, 0);
    NKN_ASSERT(ret != NKN_COD_INVALID_COD);
    if (ret == NKN_COD_VERSION_MISMATCH &&
	((int)uri_head->uri_expiry) != NKN_EXPIRY_FOREVER &&
	uri_head->uri_expiry <= nkn_cur_ts) {
	/* If the object has expired, pretend that it doesn't exist */
	glob_dm2_attr_update_expired_cnt++;
	return -ENOENT;
    }
    if (ret != 0) {
	ct->ct_dm2_update_attr_cod_mismatch_warn++;
	/* Bug 3757: It's not clear that this bug has a legitimate reason
	 * to remove this assert, but we need to get the release out now. */
	//NKN_ASSERT(ret == 0);
	ret = -ret;
	goto end;
    }
    if (! NKN_OBJV_EQ(&update_attr->in_attr->obj_version,
		      &uri_head->uri_version)) {
	v0 = update_attr->in_attr->obj_version.objv_l[0];
	v1 = update_attr->in_attr->obj_version.objv_l[1];
	DBG_DM2S("Versions do not match: 0x%lx/0x%lx  0x%lx/0x%lx", v0, v1,
		 uri_head->uri_version.objv_l[0],
		 uri_head->uri_version.objv_l[1]);
	NKN_ASSERT(NKN_OBJV_EQ(&update_attr->in_attr->obj_version,
			       &uri_head->uri_version));
	ct->ct_dm2_update_attr_version_mismatch_err++;
	ret = -EINVAL;
	goto end;
    }
 end:
    return ret;
}	/* dm2_update_attr_version_check */


/*
 * This function reads in the containers of the object requested.
 * TODO: This function could be used globally in places as required
 *	 to read in containers.
 */
static int
dm2_attr_update_read_cont(dm2_cache_type_t *ct,
			  char* uri)
{
    dm2_container_head_t *cont_head = NULL;
    dm2_container_t	 *cont;
    int			 valid_cont_found = 0;
    int			 mutex_locked = 0;
    int			 ret = 0;
    int			 cont_head_locked = DM2_CONT_HEAD_UNLOCKED;
    int			 cont_wlocked = 0;

    if (ct->ct_tier_preread_done && ct->ct_tier_preread_stat_opt) {
	/* As long as ct_tier_preread_stat_opt is set and preread is done,
         * we know that there are no directories which are not found */
	ct->ct_dm2_attr_update_opt_not_found++;
	return 0;
    }

    dm2_ct_stat_mutex_lock(ct, &mutex_locked, uri);
    /* not in memory, check out disk cache */
    ret = dm2_get_contlist(uri, 0, NULL, ct, &valid_cont_found, &cont_head,
			   &cont, &cont_wlocked, &cont_head_locked);
    if (ret < 0)
	goto dm2_attrup_stat_unlock;
    NKN_ASSERT(cont_wlocked == 0);  /* Should never be creating anything */

    if (valid_cont_found == 0)
	goto dm2_attrup_stat_unlock;

    if ((ret = dm2_read_contlist(cont_head, 0, ct)) < 0)
	goto dm2_attrup_cont_error;

    if ((ret = dm2_read_container_attrs(cont_head, &cont_head_locked, 0, ct)) < 0)
	goto dm2_attrup_cont_error;

dm2_attrup_cont_error:
    if (cont_head_locked != DM2_CONT_HEAD_UNLOCKED)
	dm2_conthead_rwlock_unlock(cont_head, ct, &cont_head_locked);

dm2_attrup_stat_unlock:
    if (mutex_locked)
	dm2_ct_stat_mutex_unlock(ct, &mutex_locked, uri);

    return ret;
}   /*	dm2_attr_update_read_cont   */

static void
dm2_attr_update_hotness(MM_update_attr_t *update_attr,
			dm2_cache_type_t *ct,
			dm2_container_head_t *cont_head,
			int *ch_locked,
			const char* uri)
{
    dm2_cache_info_t *ci;
    dm2_container_t  *cont;
    DM2_uri_t	     *uri_head = NULL;
    GList	     *cont_list, *cptr;
    int	      old_hotval = 0, new_hotval = 0;

    glob_dm2_attr_update_hotness_cnt++;

    cont_list = cont_head->ch_cont_list;
    for (cptr = cont_list; cptr; cptr = cptr->next) {
	/* Walk through each of the 'enabled' disks to see if the URI
	 * is present */
	cont = (dm2_container_t *)cptr->data;
	ci = cont->c_dev_ci;
	if (ci->ci_disabling)
	    continue;

	/* Lock the ci while we are updating attributes */
	dm2_ci_dev_rwlock_rlock(ci);
	uri_head = NULL;

	/* Grab a write lock on the uri_head. We need to serialize
	 * multiple hotness_updates happening on the same object
	 * to ensure that the evict list is not corrupted */
	uri_head = dm2_uri_head_get_by_ci_uri_wlocked(uri, cont_head,
						      ch_locked, cont,
						      cont->c_dev_ci, ct, 0);
	if (uri_head == NULL) {
	    dm2_ci_dev_rwlock_runlock(ci);
	    continue;
	}

	/* Skip if the URI is marked bad after a delete */
	if (uri_head->uri_bad_delete) {
	    glob_dm2_attr_update_skip_bad_uri++;
	    dm2_uri_head_rwlock_wunlock(uri_head, ct, DM2_NOLOCK_CI);
	    dm2_ci_dev_rwlock_runlock(ci);
	    continue;
	}

	old_hotval = am_decode_hotness(&uri_head->uri_hotval);
	new_hotval = am_decode_hotness(&update_attr->in_attr->hotval);

	assert(new_hotval >= 0);
	if (old_hotval != new_hotval) {
	    /* Promote will delete the uri_head from the current evict list
	     * and add it to the new evict list. Each evict list has its own
	     * mutex and there is no global mutex protecting this single API.
	     */
	    dm2_evict_promote_uri(uri_head, old_hotval, new_hotval);
	}

	/* Update the hotness value as sent to dm2 from above */
	uri_head->uri_hotval = update_attr->in_attr->hotval;
	uri_head->uri_orig_hotness = new_hotval;
	if (glob_dm2_attr_update_log_hotness) {
	     dm2_cache_log_attr_hotness_update(uri_head, uri, ct,
					       update_attr->in_attr->hotval);
	}
	dm2_uri_head_rwlock_wunlock(uri_head, ct, DM2_NOLOCK_CI);
	dm2_ci_dev_rwlock_runlock(ci);
    }
}	/* dm2_attr_update_hotness */

static int
dm2_attr_update_active_checks(nkn_provider_type_t ptype,
			      const char	  *attr_uri,
			      nkn_attr_t	  *attr)
{
    char ns_uuid[NKN_MAX_UID_LENGTH];
    uint64_t max_pin_bytes;
    int64_t blocksize, uri_size, old_total, total_space;
    ns_stat_token_t ns_stoken = NS_NULL_STAT_TOKEN;
    int  ret, enabled, err = 0;

    if (ptype != glob_dm2_lowest_tier_ptype)
	return -1;

    if ((ret = dm2_uridir_to_nsuuid(attr_uri, ns_uuid)) != 0)
	return ret;

    /* Not needed here but be optimistic that we won't encounter errors */
    ns_stoken = dm2_ns_get_stat_token_from_uri_dir(attr_uri);
    if (ns_is_stat_token_null(ns_stoken)) {
	DBG_DM2S("[URI=%s] Cannot get stat token", attr_uri);
	return -NKN_STAT_NAMESPACE_EINVAL;
    }
    /*
     * Clear cache pin bit if the size of this object is greater than
     * the maximum allowed for this namespace.
     */
    ret = ns_stat_get_cache_pin_enabled_state(ns_uuid, &enabled);
    if (ret || enabled == 0) {	/* cache pinning not enabled */
	/* Reset cache pin bit as we cannot pin this object */
	glob_dm2_attr_update_cache_pin_reset_pin_disabled_cnt++;
	err = -1;
	goto free_stoken;
    }

    /* if max_pin_bytes == 0, then there is no maximum set */
    ret = ns_stat_get_cache_pin_max_obj_size(ns_uuid, &max_pin_bytes);
    if (ret ||
	(max_pin_bytes != 0 && (max_pin_bytes < attr->content_length))) {
	DBG_DM2W("[URI=%s] Attempt ingest without pinned attribute", attr_uri);
	glob_dm2_attr_update_cache_pin_reset_size_cnt++;
	err = -1;
	goto free_stoken;
    }

    blocksize = dm2_tier_blocksize(ptype);
    uri_size = ROUNDUP_PWR2(attr->content_length, blocksize);
    ret = ns_stat_get(ns_stoken, NS_CACHE_PIN_SPACE_RESV_TOTAL, &total_space);
    if (ret != 0) {
	assert(ret == NKN_STAT_GEN_MISMATCH);
    }

    ret = ns_stat_add(ns_stoken, NS_CACHE_PIN_SPACE_USED_TOTAL, 0, &old_total);
    if (ret != 0) {
	assert(ret == NKN_STAT_GEN_MISMATCH);
    }

    /* total_space == 0 means there is no limit */
    if (total_space != 0 && (old_total+uri_size) > total_space) {
	glob_dm2_attr_update_cache_pin_enospc_err++;
	err = -1;
	goto free_stoken;
    }

 free_stoken:
    ns_stat_free_stat_token(ns_stoken);
    return 0;
}   /* dm2_attr_update_active_checks */


static int
dm2_attr_update_adjust_cache_pin_usage(const char* attr_uri,
				       nkn_attr_t* attr,
				       nkn_provider_type_t ptype,
				       const char* mgmt_name,
				       int sub)
{
    char ns_uuid[NKN_MAX_UID_LENGTH];
    ns_stat_token_t ns_stoken = NS_NULL_STAT_TOKEN;
    ns_stat_category_t cp_cat, ns_cat;
    int64_t blocksize, uri_size;
    int ret, count = 1;

    if ((ret = dm2_uridir_to_nsuuid(attr_uri, ns_uuid)) != 0)
	return ret;

    ns_stoken = dm2_ns_get_stat_token_from_uri_dir(attr_uri);
    if (ns_is_stat_token_null(ns_stoken)) {
	DBG_DM2S("[URI=%s] Cannot get stat token", attr_uri);
	return -NKN_STAT_NAMESPACE_EINVAL;
    }

    blocksize = dm2_tier_blocksize(ptype);
    uri_size = ROUNDUP_PWR2(attr->content_length, blocksize);

    if (sub) {
	uri_size = -uri_size;
	count = -count;
    }

    /* Update Global Accounting */
    ret = ns_stat_add(ns_stoken, NS_CACHE_PIN_SPACE_USED_TOTAL, uri_size, NULL);
    if (ret != 0) {
	assert(ret == NKN_STAT_GEN_MISMATCH);
    }

    ret = ns_stat_add(ns_stoken, NS_CACHE_PIN_OBJECT_COUNT, count, NULL);
    if (ret != 0) {
	assert(ret == NKN_STAT_GEN_MISMATCH);
    }

    /* Update disk level accounting */
    ret = dm2_stat_get_diskstat_cats(mgmt_name, &cp_cat, &ns_cat, NULL);
    if (ret != 0) {
	assert(0);
    }

    ret = ns_stat_add(ns_stoken, cp_cat, uri_size, NULL);
    if (ret != 0) {
	assert(ret == NKN_STAT_GEN_MISMATCH);
    }

    ns_stat_free_stat_token(ns_stoken);
    return 0;
}	/* dm2_attr_update_adjust_cache_pin_usage */


int
DM2_attribute_update(MM_update_attr_t *update_attr)
{
    uint64_t		 init_ticks;
    char		 *attrpath;
    DM2_disk_attr_desc_t *dad;
    dm2_cache_type_t	 *ct;
    dm2_container_t	 *cont;
    dm2_cache_info_t	 *ci = NULL;
    nkn_attr_t		 *dattr;
    DM2_uri_t		 *uri_head;
    dm2_container_head_t *cont_head;
    char		 *uri = NULL, *uri_basename, *uri_dir, *uri_lookup;
    GList                *cont_list, *cptr;
    nkn_attr_t		 *attrcp = NULL;
    dm2_uri_lock_t	 *ulock = NULL;
    int			 ch_locked = DM2_CONT_HEAD_UNLOCKED;
    off_t		 seek_off, nbytes;
    int			 idx, afd = -1, ret = 0, ret_delete = 0;
    int			 uri_found_count = 0, first_ret = 0;
    int			 dec_counter = 0, cp_sub = -1;
    int			 old_hotval = 0, new_hotval = 0;
    uint64_t		 uri_content_len;

    glob_dm2_attr_update_cnt++;

    /* check in_pytpe and get the cache index */
    if ((idx = dm2_cache_array_map_ret_idx(update_attr->in_ptype)) < 0) {
	DBG_DM2S("Illegal ptype: %d", update_attr->in_ptype) ;
	ret = -EINVAL;
	glob_dm2_attr_update_ptype_err++;
	goto error;
    }
    init_ticks = LATENCY_START();

    ct = &g_cache2_types[idx];
    AO_fetch_and_add1(&ct->ct_dm2_attr_update_curr_cnt);
    ct->ct_dm2_attr_update_cnt++;
    dec_counter = 1;

    if ((ret = dm2_attr_update_check(update_attr, &uri)) < 0) {
	/* if the ATTR is too large then delete that object */
	if (ret == -NKN_DM2_ATTR_TOO_LARGE) {
	    uri = (char *)nkn_cod_get_cnp(update_attr->uol.cod);
	    ret_delete = dm2_common_delete_uol(ct, update_attr->uol.cod,
	                                       update_attr->in_ptype,
	                                       0, uri, NULL,
					       DM2_DELETE_BIG_ATTR_UOL);
	    if (ret_delete)
		ret = ret_delete;
	}
	goto error;
    }

    DBG_DM2M("[cache_type=%s] Attribute update: %s", ct->ct_name, uri);

    /* copy the attribute buffer as the original buffer
     * could still be changed by other modules above DM.
     */
    if (update_attr->in_utype != MM_UPDATE_TYPE_HOTNESS) {
	if (nkn_mm_dm2_attr_copy(update_attr->in_attr, &attrcp,
				 mod_dm2_posix_memalign) < 0) {
	    DBG_DM2S("Attribute copy failed");
	    ret = -ENOMEM;
	    goto error;
	}
    }

    uri_dir = alloca(strlen(uri)+1);
    strcpy(uri_dir, uri);   //* OK to strcpy since used strlen for size
    dm2_uri_dirname(uri_dir, 0);
    uri_basename = dm2_uri_basename(uri);
    uri_lookup = alloca(DM2_MAX_BASENAME_ALLOC_LEN);

    cont_head = dm2_conthead_get_by_uri_dir_rlocked(uri_dir, ct, &ch_locked,
						    NULL, DM2_BLOCKING);
    if (cont_head == NULL) {
        if ((ret = dm2_attr_update_read_cont(ct, uri)) != 0) {
	    DBG_DM2W("[cache_type=%s] Container read failed while looking for"
		     " URI %s", ct->ct_name, uri);
	    goto error;
	}

	/* Read the containers to check if the uri_dir is in disk */
	cont_head = dm2_conthead_get_by_uri_dir_rlocked(uri_dir, ct, &ch_locked,
							NULL, DM2_BLOCKING);
	if (cont_head == NULL) {
	    ret = -ENOENT;
	    goto error;
	}
    }

    if (update_attr->in_utype == MM_UPDATE_TYPE_HOTNESS) {
	dm2_attr_update_hotness(update_attr, ct, cont_head, &ch_locked, uri);
	goto attr_update_end;
    }

    cont_list = cont_head->ch_cont_list;
    for (cptr = cont_list; cptr; cptr = cptr->next) {
	/* Walk through each of the 'enabled' disks to see if the URI
	 * is present */
	cont = (dm2_container_t *)cptr->data;
	ci = cont->c_dev_ci;
	if (ci->ci_disabling)
	    continue;

	/* Lock the ci while we are updating attributes */
	dm2_ci_dev_rwlock_rlock(ci);
	uri_head = NULL; ulock = NULL;
	if (!first_ret)
	    first_ret = ret;
	/* This URI shall be in my cache if it exists */
	uri_head = dm2_uri_head_get_by_ci_uri_wlocked(uri, cont_head,
						      &ch_locked, cont,
						      cont->c_dev_ci, ct, 0);
	if (uri_head == NULL) {
	    dm2_ci_dev_rwlock_runlock(ci);
	    continue;
	}
	dm2_uri_log_state(uri_head, DM2_URI_OP_ATTR, 0,0, gettid(), nkn_cur_ts);
	assert(uri_head->uri_lock->uri_nwriters == 1);
	uri_found_count++;

	/* Skip if the URI is marked bad after a delete */
	if (uri_head->uri_bad_delete) {
	    glob_dm2_attr_update_skip_bad_uri++;
	    dm2_uri_log_state(uri_head, DM2_URI_OP_ATTR, 0, 0,
			      gettid(), nkn_cur_ts);
	    dm2_uri_head_rwlock_wunlock(uri_head, ct, DM2_NOLOCK_CI);
	    dm2_ci_dev_rwlock_runlock(ci);
	    continue;
	}

        /* XXXmiken:should not happen anymore since we grab write lock in PUT */
	if (uri_head->uri_at_len == DM2_ATTR_INIT_LEN) {
	    /*
	     * We are racing with a parallel PUT which is ingesting this object
	     * and the attribute object hasn't yet been written to disk.
	     */
	    ret = -NKN_DM2_OBJ_NOT_YET_WRITTEN;;
	    glob_dm2_attr_update_not_init_warn++;
	    dm2_uri_log_state(uri_head, DM2_URI_OP_ATTR, 0, ret,
			      gettid(), nkn_cur_ts);
	    dm2_uri_head_rwlock_wunlock(uri_head, ct, DM2_NOLOCK_CI);
	    dm2_ci_dev_rwlock_runlock(ci);
	    continue;
	}

	ret = dm2_update_attr_version_check(update_attr, uri_head, ct);
	if (ret != 0) {
	    dm2_uri_log_state(uri_head, DM2_URI_OP_ATTR, 0, ret,
			      gettid(), nkn_cur_ts);
	    dm2_uri_head_rwlock_wunlock(uri_head, ct, DM2_NOLOCK_CI);
	    dm2_ci_dev_rwlock_runlock(ci);
	    continue;
	}

	if (uri_head->uri_content_len !=
		    update_attr->in_attr->content_length) {
	    /* If the update is for a regular object with EOD flag set
	     * it is a content length sync update and it is valid */
	    if ((update_attr->in_uflags != MM_UPDATE_FLAGS_EOD) &&
		!nkn_attr_is_streaming(update_attr->in_attr)) {
		uri_head->uri_expiry = nkn_cur_ts - 1;
		if (attrcp)
		    attrcp->cache_expiry = uri_head->uri_expiry;
		glob_dm2_attr_update_content_length_mismatch_warn++;
		uri_content_len = uri_head->uri_content_len;
		DBG_DM2W("[cache_type=%s] Content Length Mismatch, "
			 "expiring URI: %s old_len: %ld, new_len: %ld",
			 ct->ct_name, uri, uri_content_len,
			 update_attr->in_attr->content_length);
	    }
	}

	if (update_attr->in_attr->hotval == 0) {
	    /* If the hotval in an attribute_update operation is zero, then DM2
	     * should initialize the time and bump a counter. */
	    AM_init_hotness(&update_attr->in_attr->hotval);
	    glob_dm2_attr_update_hotval_zero_cnt++;
	}

	if (am_decode_update_time(&update_attr->in_attr->hotval) == 0) {
	    DBG_DM2S("Illegal value for hotval: 0x%lx",
		     update_attr->in_attr->hotval);
	    NKN_ASSERT(am_decode_update_time(&update_attr->in_attr->hotval)
		       != 0);
	}

	NKN_ASSERT(am_decode_update_time(&update_attr->in_attr->hotval) != 0);


	if (ct->ct_ptype == glob_dm2_lowest_tier_ptype) {
	    if (uri_head->uri_cache_pinned && !nkn_attr_is_cache_pin(attrcp)) {
		/* Pinning has been removed for this object */
		uri_head->uri_cache_pinned = 0;
		dm2_evict_add_uri(uri_head,
				  am_decode_hotness(&uri_head->uri_hotval));
		cp_sub = 1;
	    } else if (!uri_head->uri_cache_pinned &&
			nkn_attr_is_cache_pin(attrcp)) {
		/* Object was not pinned before, but now pinning is required */
		if (dm2_attr_update_active_checks(ct->ct_ptype, uri, attrcp)) {
		    nkn_attr_reset_cache_pin(attrcp);
		} else {
		    uri_head->uri_cache_pinned = 1;
		    if (uri_head->uri_evict_list_add)
			dm2_evict_delete_uri(uri_head,
			    am_decode_hotness(&uri_head->uri_hotval));
		    cp_sub = 0;
		}
	    }
	}

	cont = uri_head->uri_container;
	ci = cont->c_dev_ci;
	DM2_GET_ATTRPATH(cont, attrpath);
	if ((ret = dm2_open_attrpath(attrpath)) < 0) {
	    DBG_DM2S("[cache_type=%s] Failed to open attribute file (%s): %d",
		     ct->ct_name, attrpath, errno);
	    glob_dm2_attr_update_open_err++;
	    dm2_uri_log_state(uri_head, DM2_URI_OP_ATTR, 0, ret,
			      gettid(), nkn_cur_ts);
	    dm2_uri_head_rwlock_wunlock(uri_head, ct, DM2_NOLOCK_CI);
	    dm2_ci_dev_rwlock_runlock(ci);
	    continue;
	}
	afd = ret;
	ret = 0;
	NKN_ASSERT(afd != -1);

	ct->ct_dm2_attr_update_write_cnt++;
	if (dm2_verify_attr_on_update) {
	    dad = alloca(3 * DEV_BSIZE);
	    dad = (DM2_disk_attr_desc_t *)roundup((uint64_t)dad, DEV_BSIZE);
	    seek_off = uri_head->uri_at_off * DEV_BSIZE;
	    nbytes = pread(afd, dad, 2*DEV_BSIZE, seek_off);
	    if (nbytes != 2*DEV_BSIZE) {
		ret = -errno;
		DBG_DM2E("IO ERROR:[name=%s] Failed to read URI name "
                         "(%s,%s) offset=%ld: %ld in fd(%d) expected(%zu)"
			 "bytes errno(%d)", ci->mgmt_name, attrpath, uri,
			 seek_off, nbytes, afd, 2*DEV_BSIZE, -ret);
		ret = -EIO;
		glob_dm2_attr_update_pread_dad_err++;
		NKN_ASSERT(nbytes != -1 || ret != -EBADF);
		goto next_ci;
	    }
	    if (dad->dat_magic != DM2_ATTR_MAGIC) {
		DBG_DM2S("IO ERROR:[name=%s] Illegal magic number at "
                         "offset=%ld uri=(%s,%s): expected=0x%x read=0x%x"
                         "in fd(%d) errno(%d)", ci->mgmt_name,
			 seek_off, attrpath, uri, DM2_ATTR_MAGIC,
			 dad->dat_magic, afd, -ret);
		ret = -EIO;
		glob_dm2_attr_update_magic_err++;
		goto next_ci;
	    }
	    if (dad->dat_version != DM2_ATTR_VERSION) {
		DBG_DM2S("IO ERROR:[name=%s] Illegal version number at "
                         "offset=%ld uri=(%s,%s): expected=%d read=%d"
                         " in fd(%d) errno(%d)", ci->mgmt_name,
			 seek_off, attrpath, uri, DM2_ATTR_VERSION,
                         dad->dat_version, afd, -ret);
		ret = -EINVAL;
		glob_dm2_attr_update_version_err++;
		goto next_ci;
	    }
	    if (strncmp(uri_basename, dad->dat_basename, NAME_MAX)) { //no match
		DBG_DM2S("IO ERROR:[name=%s] Illegal URI at offset=%ld "
                         "path=%s: expected=%s read=%s in fd(%d) "
                         "errno(%d)", ci->mgmt_name, seek_off,
			 attrpath, uri, dad->dat_basename, afd, -ret);
		ret = -EINVAL;
		glob_dm2_attr_update_uri_err++;
		goto next_ci;
	    }
	    dattr = (nkn_attr_t *)((uint64_t)dad + DEV_BSIZE);
	    seek_off += DEV_BSIZE;
	    if (dattr->na_magic != NKN_ATTR_MAGIC) {
		DBG_DM2S("IO ERROR:[name=%s] Illegal magic number at "
                         "offset=%ld uri=(%s,%s): expected=0x%x read=0x%x"
                         " in fd(%d) errno(%d)", ci->mgmt_name,
			 seek_off, attrpath, uri, NKN_ATTR_MAGIC,
			 dattr->na_magic, afd, -ret);
		ret = -EINVAL;
		glob_dm2_attr_update_disk_magic_err++;
		goto next_ci;
	    }
	    if (dattr->na_version != NKN_ATTR_VERSION) {
		DBG_DM2S("IO ERROR:[name=%s] Illegal version at "
                         "offset=%ld uri=(%s,%s): expected=0x%x read=0x%x"
                         " in fd(%d) errno(%d)", ci->mgmt_name,
			 seek_off, attrpath, uri, NKN_ATTR_VERSION,
			 dattr->na_version, afd, -ret);
		ret = -EINVAL;
		glob_dm2_attr_update_disk_version_err++;
		goto next_ci;
	    }
	    /* Will perform magic check but not checksum check */
	}
	seek_off = (uri_head->uri_at_off+1) * DEV_BSIZE;
	attrcp->na_checksum = 0;
	attrcp->na_checksum =
	    do_csum64_iterate_aligned((uint8_t *)attrcp,
				      attrcp->total_attrsize, 0);
	nbytes = pwrite(afd, attrcp,
			attrcp->total_attrsize, seek_off);
	if (nbytes == -1) {
	    ret = errno;
	    DBG_DM2S("IO ERROR:[name=%s] Failed to write URI attr (%s,%s)"
		    " offset=%ld  buf=%p: in fd(%d) errno=%d",
		    ci->mgmt_name, attrpath, uri, seek_off, attrcp, afd,
		    ret);
	    NKN_ASSERT(ret != EBADF);
	    ret = -EIO;
	    glob_dm2_attr_update_pwrite_attr_err++;
	    goto next_ci;
	}
	if (nbytes != attrcp->total_attrsize) {
	    DBG_DM2S("IO ERROR:[name=%s] Failed to write URI attr (%s,%s)"
                     "offset=%ld buf=%p: %ld in fd(%d) errno=%d",
                     ci->mgmt_name, attrpath, uri, seek_off, attrcp,
                     nbytes, afd, errno);
	    ret = -EIO;
	    glob_dm2_attr_update_pwrite_attr_err++;
	    goto next_ci;
	}

	if (cp_sub != -1)
	    dm2_attr_update_adjust_cache_pin_usage(uri, attrcp, ct->ct_ptype,
						   ci->mgmt_name, cp_sub);

	/* Update data which DM2 caches in memory */
	uri_head->uri_expiry = update_attr->in_attr->cache_expiry;
	if (update_attr->in_attr->na_flags2 & NKN_OBJ_INVALID)
	    uri_head->uri_invalid = 1;

	if (!uri_head->uri_cache_pinned) {
	    old_hotval = am_decode_hotness(&uri_head->uri_hotval);
	    new_hotval = am_decode_hotness(&update_attr->in_attr->hotval);

	    assert(new_hotval >= 0);
	    if (old_hotval != new_hotval) {
		/* Promote will delete the uri_head from the current evict list
		 * and add it to the new evict list. Each evict list has its own
		 * mutex and there is no global mutex protecting this single API.
		 */
		dm2_evict_promote_uri(uri_head, old_hotval, new_hotval);
	    }

	    /* Update the hotness value as sent to dm2 from above */
	    uri_head->uri_hotval = update_attr->in_attr->hotval;
	    uri_head->uri_orig_hotness = new_hotval;
	}

	// Update cache log only for expiry update and not hotness update
	if (update_attr->in_utype == MM_UPDATE_TYPE_EXPIRY) {
	     glob_dm2_attr_update_expiry_cnt++;
	     dm2_cache_log_attr_update(uri_head, uri, ct);
	} else if (update_attr->in_utype == update_attr->in_utype) {
	     glob_dm2_attr_update_sync_cnt++;
	}

    next_ci:
	dm2_uri_log_state(uri_head, DM2_URI_OP_ATTR, 0, ret,
			  gettid(), nkn_cur_ts);
	assert(uri_head->uri_lock->uri_nwriters == 1);
	dm2_uri_head_rwlock_wunlock(uri_head, ct, DM2_NOLOCK_CI);
	dm2_ci_dev_rwlock_runlock(ci);
	if (afd != -1) {
	    glob_dm2_attr_close_cnt++;
	    nkn_close_fd(afd, DM2_FD);
	    afd = -1;
	}
    }

    if (uri_found_count == 0) {
	first_ret = -ENOENT;
	if (dm2_caches_running(ct)) {
	    DBG_DM2W("[cache_type=%s] Attribute update (URI not found): %s",
		     ct->ct_name, uri);
	    glob_dm2_attr_update_uri_not_found_warn++;
	}
    }
    LATENCY_END(&ct->ct_dm2_attrupdate_latency, init_ticks);

 attr_update_end:
 error:
    if (attrcp)
	free(attrcp); //can't dm2_free() as alloc'd by nkn_attr_copy()
    if (ch_locked != DM2_CONT_HEAD_UNLOCKED)
	dm2_conthead_rwlock_unlock(cont_head, ct, &ch_locked);
    if (ret != 0 && !first_ret)
	first_ret = ret;
    if (first_ret != 0 && first_ret != -ENOENT)
	glob_dm2_attr_update_err++;
    update_attr->out_ret_code = (first_ret < 0 ? -first_ret : first_ret);
    if (dec_counter)
        AO_fetch_and_sub1(&ct->ct_dm2_attr_update_curr_cnt);

    return first_ret;
}	/* DM2_attribute_update */


// Subsumed by DM_delete called via MM_delete
int DM2_my_delete(char *arg);
int
DM2_my_delete(char *arg)
{
    MM_delete_resp_t delete;
    nkn_uol_t *uol = &delete.in_uol;

    memset(&delete, 0, sizeof(delete));
    delete.in_ptype = SAS2DiskMgr_provider;
    delete.in_sub_ptype = 0;
    uol->cod = nkn_cod_open(arg, strlen(arg), NKN_COD_DISK_MGR);
    uol->length = 0;
    uol->offset = 0;
    if (DM2_delete(&delete) != 0 && glob_dm2_num_cache_types > 1) {
	delete.in_ptype = SATADiskMgr_provider;
	DM2_delete(&delete);
    }
    nkn_cod_close(uol->cod, NKN_COD_DISK_MGR);
    return 0;
}

int
DM2_find_cache_running_state(int num,
			     ns_stat_category_t category)
{
    dm2_cache_info_t	*ci;
    GList		*ci_obj;
    int			ct_index;
    int			dc_index;
    nkn_provider_type_t ptype_check;

    if (category == NS_USED_SPACE_SSD)
	ptype_check = SolidStateMgr_provider;
    else if (category == NS_USED_SPACE_SAS)
	ptype_check = SAS2DiskMgr_provider;
    else if (category == NS_USED_SPACE_SATA)
	ptype_check = SATADiskMgr_provider;
    else
	assert(0);

    for (ct_index=0; ct_index < glob_dm2_num_cache_types; ct_index++) {
	dm2_ct_info_list_rwlock_rlock(&g_cache2_types[ct_index]);
	if (g_cache2_types[ct_index].ct_ptype != ptype_check) {
	    dm2_ct_info_list_rwlock_runlock(&g_cache2_types[ct_index]);
	    continue;
	}
	ci_obj = g_cache2_types[ct_index].ct_info_list;
	for (; ci_obj; ci_obj = ci_obj->next) {
	    ci = (dm2_cache_info_t *)ci_obj->data;
	    dc_index = atoi(&ci->mgmt_name[strlen(NKN_DM_DISK_CACHE_PREFIX)]);
	    if (dc_index == num) {
		if (ci->state_overall == DM2_MGMT_STATE_CACHE_RUNNING) {
		    dm2_ct_info_list_rwlock_runlock(&g_cache2_types[ct_index]);
		    return 1;
		} else {
		    dm2_ct_info_list_rwlock_runlock(&g_cache2_types[ct_index]);
		    return 0;
		}
	    }
	}
	ci_obj = g_cache2_types[ct_index].ct_del_info_list;
	for (; ci_obj; ci_obj = ci_obj->next) {
	    ci = (dm2_cache_info_t *)ci_obj->data;
	    dc_index = atoi(&ci->mgmt_name[strlen(NKN_DM_DISK_CACHE_PREFIX)]);
	    if (dc_index == num) {
		dm2_ct_info_list_rwlock_runlock(&g_cache2_types[ct_index]);
		return 0;
	    }
	}
	dm2_ct_info_list_rwlock_runlock(&g_cache2_types[ct_index]);
    }
    return 0;
}	/* DM2_find_cache_running_state */


void
DM2_ns_set_rp_size_for_namespace(ns_stat_category_t cat,
				 uint32_t rp_index,
				 uint64_t total)
{
    rp_type_en rp_type;
    int ret;

    /* This is a deleted namespace with URIs */
    if (rp_index == (uint32_t)-1)
	return;

    rp_type = dm2_nscat_to_rptype(cat);

    //DBG_DM2S("rp_type=%d rp_index=%d total=%ld", rp_type, rp_index, total);
    ret = nvsd_rp_alloc_resource_no_check(rp_type, rp_index, total);
    if (ret == 0) {
	DBG_DM2S("Failed to add namespace space to resource pool: %d/%d/%ld",
		 rp_type, rp_index, total);
    }
}	/* DM2_ns_set_rp_size_for_namespace */


dm2_cache_info_t *
dm2_mgmt_find_disk_cache(const char *mgmt_name,
			 uint32_t   **ptype_ptr)
{
    dm2_cache_info_t *ci;
    GList	     *ci_obj;
    int		     ct_index;

    for (ct_index=0; ct_index < glob_dm2_num_cache_types; ct_index++) {
	dm2_ct_info_list_rwlock_rlock(&g_cache2_types[ct_index]);
	ci_obj = g_cache2_types[ct_index].ct_info_list;
	for (; ci_obj; ci_obj = ci_obj->next) {
	    ci = (dm2_cache_info_t *)ci_obj->data;
	    if (ci->state_overall != DM2_MGMT_STATE_NOT_PRESENT &&
		!strcmp(ci->mgmt_name, mgmt_name)) {
		    dm2_ct_info_list_rwlock_runlock(&g_cache2_types[ct_index]);
		    *ptype_ptr = &g_cache2_types[ct_index].ct_ptype;
		    return ci;
	    }
	}
	ci_obj = g_cache2_types[ct_index].ct_del_info_list;
	for (; ci_obj; ci_obj = ci_obj->next) {
	    ci = (dm2_cache_info_t *)ci_obj->data;
	    if (ci->state_overall != DM2_MGMT_STATE_NOT_PRESENT &&
		!strcmp(ci->mgmt_name, mgmt_name)) {
		    dm2_ct_info_list_rwlock_runlock(&g_cache2_types[ct_index]);
		    *ptype_ptr = &g_cache2_types[ct_index].ct_ptype;
		    return ci;
	    }
	}
	dm2_ct_info_list_rwlock_runlock(&g_cache2_types[ct_index]);
    }
    return NULL;
}	/* dm2_mgmt_find_disk_cache */

int
DM2_mgmt_TM_DB_update_msg(const char		 *mgmt_name,
			  const dm2_mgmt_state_t update_field,
			  const void		 *update_value,
			  char			 *out_buffer)
{
    dm2_mgmt_db_info_t *db_info;
    dm2_cache_info_t *ci = NULL;
    char	     *string_ptr;
    uint64_t	     bits64;
    uint32_t	     bits32, small_block;
    int		     ret = 0;
    uint32_t	     *ptype_ptr = NULL;

    printf("mgmt=%s  update=%x\n", mgmt_name, update_field);
    if (update_value == NULL) {
	DBG_DM2S("NULL update_value");
	return -EINVAL;
    }
    if (mgmt_name == NULL) {
	DBG_DM2S("NULL mgmt_name");
	return -EINVAL;
    }

    bits32 = *(uint32_t *)update_value;	// bool_t is an int32
    bits64 = *(uint64_t *)update_value;
    string_ptr = (char *)update_value;
    db_info = (dm2_mgmt_db_info_t *)update_value;

    if (mgmt_name[0] != '*' &&
	((ci = dm2_mgmt_find_disk_cache(mgmt_name, &ptype_ptr)) == NULL)) {
	/* We can not find a valid 'ci' if the 'mgmt_name' was just added */
	return -NKN_DM2_DISK_CACHE_NOT_FOUND;
    }

    switch (update_field) {
	case DM2_MGMT_MOD_ACTIVATE_START:
	    if (mgmt_name[0] == '*') {
		/* Special case */
		dm2_mgmt_find_new_disks();
	    } else if (bits32 == 1) {
		NKN_ASSERT(ci);
		ret = dm2_mgmt_activate_start(ci);
	    } else {
		NKN_ASSERT(ci);
		ret = dm2_mgmt_deactivate(ci);
	    }
	    break;
	case DM2_MGMT_MOD_ACTIVATE_FINISH:
	    NKN_ASSERT(mgmt_name[0] == '*');
	    ret = dm2_mgmt_activate_finish((dm2_mgmt_db_info_t *)update_value);
	    break;
	case DM2_MGMT_MOD_CACHE_ENABLED:
	    NKN_ASSERT(ci);
	    if (bits32 == 1)
		ret = dm2_mgmt_cache_enable(ci, ptype_ptr);
	    else
		ret = dm2_mgmt_cache_disable(ci);
	    break;
	case DM2_MGMT_MOD_COMMENTED_OUT:
	    break;
	case DM2_MGMT_MOD_MISSING_AFTER_BOOT:
	    break;
	case DM2_MGMT_MOD_SET_SECTOR_CNT:
	    /* Shouldn't happen on production system */
	    NKN_ASSERT(0);
	    break;
	case DM2_MGMT_MOD_RAW_PARTITION:
	    break;
	case DM2_MGMT_MOD_FS_PARTITION:
	    break;
	case DM2_MGMT_MOD_AUTO_REFORMAT:
	    /* Not used */
	    NKN_ASSERT(0);
	    break;
	case DM2_MGMT_MOD_PRE_FORMAT:
	    NKN_ASSERT(ci);
	    if (out_buffer== NULL) {
		DBG_DM2S("NULL out_buffer");
		return -EINVAL;
	    }
	    /* Temp code until db_info ptr always passed XXXmiken */
	    small_block = db_info->mi_small_block_format;
	    ret = dm2_mgmt_pre_format_diskcache(ci, ptype_ptr, small_block,
						out_buffer);
	    /* the out_buffer has the shell command to be executed */
	    break;
	case DM2_MGMT_MOD_POST_FORMAT:
	    NKN_ASSERT(ci);
	    ret = dm2_mgmt_post_format_diskcache(ci);
	    break;
	default:
	    break;
    }
    return ret;
}	/* DM2_mgmt_TM_DB_update_msg */


int
DM2_mgmt_get_diskcache_info(const dm2_mgmt_db_info_t *obj,
			    dm2_mgmt_disk_status_t   *status)
{
    dm2_cache_type_t *ct;
    dm2_cache_info_t *ci;
    GList	     *ci_obj;
    int		     ct_index;

    DBG_DM2M("get DC info: [serial %s] [raw %s] [fs %s] [diskname %s] "
	     "[prov %s] [MP %s] [owner %s]", obj->mi_serial_num,
	     obj->mi_raw_partition, obj->mi_fs_partition,
	     obj->mi_diskname, obj->mi_provider,
	     obj->mi_mount_point, obj->mi_owner);

    for (ct_index=0; ct_index < glob_dm2_num_cache_types; ct_index++) {
	ct = &g_cache2_types[ct_index];
	dm2_ct_info_list_rwlock_rlock(ct);
	ci_obj = ct->ct_info_list;
	for (; ci_obj; ci_obj = ci_obj->next) {
	    ci = (dm2_cache_info_t *)ci_obj->data;
	    if (!strcmp(ci->serial_num, obj->mi_serial_num)) {
		dm2_ct_info_list_rwlock_runlock(&g_cache2_types[ct_index]);
		NKN_ASSERT(obj->mi_commented_out == ci->state_commented_out);
		NKN_ASSERT(obj->mi_missing_after_boot == ci->state_missing);

		strncpy(status->ms_name, ci->mgmt_name, DM2_MAX_MGMTNAME);
		status->ms_state = ci->state_overall;
		status->ms_not_production = ci->state_not_production;
		if (ci->type_unknown)
		    strncpy(status->ms_tier, DM2_TYPE_UNKNOWN,DM2_MAX_TIERNAME);
		else
		    strncpy(status->ms_tier, ct->ct_name, DM2_MAX_TIERNAME);

		DBG_DM2M("DiskCache=%s  State:%s",
			 status->ms_name, dm2_mgmt_print_state(status));
		return 0;
	    }
	}

	ci_obj = ct->ct_del_info_list;
	for (; ci_obj; ci_obj = ci_obj->next) {
	    ci = (dm2_cache_info_t *)ci_obj->data;
	    if (!strcmp(ci->serial_num, obj->mi_serial_num)) {
		dm2_ct_info_list_rwlock_runlock(&g_cache2_types[ct_index]);
		NKN_ASSERT(obj->mi_commented_out == ci->state_commented_out);
		NKN_ASSERT(obj->mi_missing_after_boot == ci->state_missing);

		strncpy(status->ms_name, ci->mgmt_name, DM2_MAX_MGMTNAME);
		status->ms_state = ci->state_overall;
		status->ms_not_production = ci->state_not_production;
		if (ci->type_unknown)
		    strncpy(status->ms_tier, DM2_TYPE_UNKNOWN,DM2_MAX_TIERNAME);
		else
		    strncpy(status->ms_tier, ct->ct_name, DM2_MAX_TIERNAME);

		DBG_DM2M("DiskCache=%s  State:%s",
			 status->ms_name, dm2_mgmt_print_state(status));
		return 0;
	    }
	}
	dm2_ct_info_list_rwlock_runlock(&g_cache2_types[ct_index]);
    }
    return -ENOENT;
}	/* DM2_mgmt_get_diskcache_info */

int
DM2_mgmt_set_time_evict_config(const char *tier_name,
			       const int32_t time_low_water_mark,
			       const int32_t hour,
			       const int32_t min)
{
    dm2_cache_type_t *ct;
    int		     ptype = -1, ptype_idx = -1;

    if (time_low_water_mark > 100 || time_low_water_mark < -1) {
	DBG_DM2S("[cache_type=%s] Bad low water mark: %d",
		 tier_name, time_low_water_mark);
	return -EINVAL;
    }

    if (hour < -1 || hour > 23) {
	DBG_DM2S("[cache_type=%s] Bad hour value: %d", tier_name, hour);
	return -EINVAL;
    }

    if (min < -1 || min > 59) {
	DBG_DM2S("[cache_type=%s] Bad hour value: %d", tier_name, hour);
	return -EINVAL;
    }

    ptype = dm2_get_provider_type(tier_name);
    if (ptype < 0) {
	DBG_DM2S("[cache_type=%s] Invalid cache type", tier_name)
	return -EINVAL;
    }
    /*
     * This array will store the default value
     * as given by the mgmt on startup, as we might not have
     * detected any cache tiers by then.
     */

    if (time_low_water_mark != -1)
        dm2_internal_time_low_watermark[ptype] = time_low_water_mark;
    if (hour != -1)
	dm2_evict_hour[ptype] = hour;
    if (min != -1)
	dm2_evict_min[ptype] = min;

    if (glob_dm2_num_cache_types == 0)
	return 0;

    if ((ptype_idx = dm2_cache_array_map_ret_idx(ptype)) >= 0){
	ct = &g_cache2_types[ptype_idx];
	/* Grab write lock to synchronize two fields */
	dm2_ct_info_list_rwlock_wlock(ct);
	if (time_low_water_mark != -1)
	    ct->ct_internal_time_low_water_mark = time_low_water_mark;
	if (hour != -1)
	    ct->ct_evict_hour = hour;
	if (min != -1)
	    ct->ct_evict_min = min;
	dm2_ct_info_list_rwlock_wunlock(ct);
	return 0;
    }

    return -ENOENT;
}	/* DM2_mgmt_set_time_evict_config */


int
DM2_mgmt_set_internal_percentage(const char *tier_name,
				 const int32_t prcnt)
{
    dm2_cache_type_t *ct;
    int		     ptype = -1, ptype_idx = -1;

    if (prcnt == 0 || prcnt > 100 || prcnt < -1) {
	DBG_DM2S("[cache_type=%s] Bad disk percentage for evict: %d",
		 tier_name, prcnt);
	return -EINVAL;
    }

    ptype = dm2_get_provider_type(tier_name);
    if (ptype < 0) {
	DBG_DM2S("[cache_type=%s] Invalid cache type", tier_name)
	return -EINVAL;
    }

    if (prcnt != -1)
	dm2_internal_num_percent[ptype] = prcnt;

    if (glob_dm2_num_cache_types == 0)
	return 0;

    if ((ptype_idx = dm2_cache_array_map_ret_idx(ptype)) >= 0){
	ct = &g_cache2_types[ptype_idx];
	/* Grab write lock to synchronize two fields */
	dm2_ct_info_list_rwlock_wlock(ct);
	if (prcnt != -1)
	    ct->ct_num_evict_disk_prcnt = prcnt;
	DBG_DM2M("[cache_type=%s] Num disks percentage=%d",
		 tier_name, ct->ct_num_evict_disk_prcnt);
	dm2_ct_info_list_rwlock_wunlock(ct);
	return 0;
    }

    return -ENOENT;
}   /* DM2_mgmt_set_internal_percentage */

int
DM2_mgmt_set_internal_water_mark(const char *tier_name,
				 const int32_t high_water_mark,
				 const int32_t low_water_mark)
{
    dm2_cache_type_t *ct;
    int		     ptype = -1, ptype_idx = -1;

    if (high_water_mark == 0 || high_water_mark > 100 || high_water_mark < -1) {
	DBG_DM2S("[cache_type=%s] Bad high water mark: %d",
		 tier_name, high_water_mark);
	return -EINVAL;
    }
    if (low_water_mark == 0 || low_water_mark > 100 || low_water_mark < -1) {
	DBG_DM2S("[cache_type=%s] Bad low water mark: %d",
		 tier_name, low_water_mark);
	return -EINVAL;
    }
#if 0
    if ((low_water_mark != -1 && high_water_mark != -1) &&
	low_water_mark > high_water_mark) {
	DBG_DM2S("[cache_type=%s] Bad watermarks, low=%d, high=%d",
		 tier_name, low_water_mark, high_water_mark);
	return -EINVAL;
    }
#endif

    ptype = dm2_get_provider_type(tier_name);
    if (ptype < 0) {
	DBG_DM2S("[cache_type=%s] Invalid cache type", tier_name)
	return -EINVAL;
    }
    /*
     * This array will store the default value
     * as given by the mgmt on startup, as we might not have
     * detected any cache tiers by then.
     */

    if (high_water_mark != -1)
	dm2_internal_high_watermark[ptype] = high_water_mark;
    if (low_water_mark != -1)
        dm2_internal_low_watermark[ptype] = low_water_mark;

    if (glob_dm2_num_cache_types == 0)
	return 0;

    if ((ptype_idx = dm2_cache_array_map_ret_idx(ptype)) >= 0){
	ct = &g_cache2_types[ptype_idx];
	/* Grab write lock to synchronize two fields */
	dm2_ct_info_list_rwlock_wlock(ct);
	if (high_water_mark != -1)
	    ct->ct_internal_high_water_mark = high_water_mark;
	if (low_water_mark != -1)
	    ct->ct_internal_low_water_mark = low_water_mark;
	DBG_DM2M("[cache_type=%s] New watermarks, low=%d, high=%d",
		 tier_name, ct->ct_internal_low_water_mark,
		 ct->ct_internal_high_water_mark);
	dm2_ct_info_list_rwlock_wunlock(ct);
	return 0;
    }

    return -ENOENT;
}	/* DM2_mgmt_set_internal_water_mark */


int
DM2_mgmt_set_dict_evict_water_mark(int hwm, int lwm)
{

    if (hwm > 0 && hwm <= 100)
	glob_dm2_dict_int_evict_hwm = hwm;

    if (lwm > 0 && lwm <= 100)
	glob_dm2_dict_int_evict_lwm = lwm;

    return 0;
}   /* DM2_mgmt_set_dict_evict_water_mark */

int
DM2_mgmt_get_internal_water_mark(const char *tier_name,
				 int32_t *high_water_mark,
				 int32_t *low_water_mark)
{
    dm2_cache_type_t *ct;
    int		     ct_index;

    for (ct_index = 0; ct_index < glob_dm2_num_cache_types; ct_index++) {
	ct = &g_cache2_types[ct_index];
	dm2_ct_info_list_rwlock_rlock(ct);
	if (!strcmp(ct->ct_name, tier_name)) {
	    *high_water_mark = ct->ct_internal_high_water_mark;
	    *low_water_mark = ct->ct_internal_low_water_mark;
	    dm2_ct_info_list_rwlock_runlock(&g_cache2_types[ct_index]);
	    return 0;
	}
	dm2_ct_info_list_rwlock_runlock(&g_cache2_types[ct_index]);
    }
    return -ENOENT;
}	/* DM2_mgmt_get_internal_water_mark */

int
DM2_mgmt_set_block_share_threshold(const char* tier_name,
				   const int32_t prcnt)
{
    dm2_cache_type_t *ct;
    int		     ct_index;
    int		     ptype;

    if (prcnt < 0 || prcnt > 100 || (prcnt % 25) != 0) {
	DBG_DM2S("[cache_type=%s] Bad block share threshold: %d",
		 tier_name, prcnt);
	return -EINVAL;
    }

    ptype = dm2_get_provider_type(tier_name);
    if (ptype < 0) {
	DBG_DM2S("[cache_type=%s] Invalid cache type", tier_name)
	return -EINVAL;
    }

    if (ptype == SolidStateMgr_provider) {
	dm2_block_share_prcnt[ptype] = 0;
	DBG_DM2M("[cache_type=%s] Block sharing cannot be enabled", tier_name);
	return 0;
    }
    /*
     * This array will store the default value
     * as given by the mgmt on startup, as we might not have
     * detected any cache tiers by then.
     */
    dm2_block_share_prcnt[ptype] = prcnt;

    /* return success if the caches have not been detected yet,
     * during nvsd startup */
    if (glob_dm2_num_cache_types == 0)
	return 0;

    for (ct_index = 0; ct_index < glob_dm2_num_cache_types; ct_index++) {
	ct = &g_cache2_types[ct_index];
	/* Grab write lock to synchronize two fields */
	dm2_ct_info_list_rwlock_wlock(ct);
	if (!strcasecmp(ct->ct_name, tier_name)) {
	    ct->ct_share_block_threshold = prcnt;
	    DBG_DM2M("[cache_type=%s] New block share threshold is %d\n",
		     tier_name, ct->ct_share_block_threshold);
	    dm2_ct_info_list_rwlock_wunlock(&g_cache2_types[ct_index]);
	    return 0;
	}
	dm2_ct_info_list_rwlock_wunlock(&g_cache2_types[ct_index]);
    }
    return -ENOENT;

}	/* DM2_mgmt_set_block_share_threshold */

int
DM2_mgmt_get_block_share_threshold(const char *tier_name,
				   int32_t  *prcnt)
{
    dm2_cache_type_t *ct;
    int              ct_index;

    for (ct_index = 0; ct_index < glob_dm2_num_cache_types; ct_index++) {
	ct = &g_cache2_types[ct_index];
	dm2_ct_info_list_rwlock_rlock(ct);
	if (!strcasecmp(ct->ct_name, tier_name)) {
	    *prcnt = ct->ct_share_block_threshold;
	    dm2_ct_info_list_rwlock_runlock(&g_cache2_types[ct_index]);
	    return 0;
	}
	dm2_ct_info_list_rwlock_runlock(&g_cache2_types[ct_index]);
    }
    return -ENOENT;


}	/* DM2_mgmt_get_block_share_threshold */

/*
 * resv_bytes = 0 is unlimited.  The upper bound can be checked at runtime
 * against the resource pool reservation size.
 */
int
DM2_mgmt_ns_set_cache_pin_diskspace(const char *ns_uuid,
				    int64_t resv_bytes)
{
    return ns_stat_set_cache_pin_diskspace(ns_uuid, resv_bytes);
}	/* DM2_mgmt_ns_set_cache_pin_diskspace */


/*
 * max_bytes = 0 is unlimited.  There is no upper bound restriction.
 */
int
DM2_mgmt_ns_set_cache_pin_max_obj_size(const char *ns_uuid,
				       int64_t max_bytes)
{
    return ns_stat_set_cache_pin_max_obj_size(ns_uuid, max_bytes);
}	/* DM2_mgmt_ns_set_cache_pin_max_obj_size */


/*
 * There should be no restrictions of when cache pinning can be enabled/
 * disabled.  The lowest tier could be over the cache pin reservation already
 * after enable.
 */
int
DM2_mgmt_ns_set_cache_pin_enabled(const char *ns_uuid,
				  int	     enabled	/* boolean */)
{
    return ns_stat_set_cache_pin_enabled_state(ns_uuid, enabled);
}	/* DM2_mgmt_ns_set_cache_pin_enabled */

int
DM2_mgmt_ns_set_tier_group_read(const char *ns_uuid,
				const char *tier_name,
				int	   enabled	/* boolean */)
{
    nkn_provider_type_t ptype;
    ptype = dm2_get_provider_type(tier_name);
    if (!ptype)
	return -EINVAL;
    return ns_stat_set_tier_group_read_state(ns_uuid, ptype, enabled);
}	/* DM2_mgmt_ns_set_tier_group_read */

int
DM2_mgmt_ns_set_tier_read_size(const char *ns_uuid,
			       const char *tier_name,
			       uint32_t	   read_size)
{
    nkn_provider_type_t ptype;
    ptype = dm2_get_provider_type(tier_name);
    if (!ptype)
	return -EINVAL;
    return ns_stat_set_tier_read_size(ns_uuid, ptype, read_size);
}	/* DM2_mgmt_ns_set_tier_group_read */

int
DM2_mgmt_ns_set_tier_block_free_threshold(const char *ns_uuid,
					  const char *tier_name,
					  uint32_t   prcnt)
{
    nkn_provider_type_t ptype;
    ptype = dm2_get_provider_type(tier_name);
    if (!ptype)
	return -EINVAL;
    return ns_stat_set_tier_block_free_threshold(ns_uuid, ptype, prcnt);
}	/* DM2_mgmt_ns_set_tier_group_read */

int
DM2_mgmt_ns_set_tier_max_disk_usage(const char *ns_uuid,
				    const char *tier_name,
				    size_t     size_in_mb)
{
    nkn_provider_type_t ptype;
    ptype = dm2_get_provider_type(tier_name);
    if (!ptype)
	return -EINVAL;
    return ns_stat_set_tier_max_disk_usage(ns_uuid, ptype, size_in_mb);
}	/* DM2_mgmt_ns_set_tier_max_disk_usage */

/*
 * Used when namespace is created
 */
int
DM2_mgmt_ns_add_namespace(namespace_node_data_t *tm_ns_info,
			  int nth)
{
    ns_tier_entry_t ns_ssd, ns_sas, ns_sata;
    uint64_t cache_pin_max_obj_bytes, cache_pin_resv_bytes;
    int cache_pin_enabled, ret, state;

    NKN_ASSERT((tm_ns_info->active && tm_ns_info->deleted) == 0);
    if (tm_ns_info->active)
	state = NS_STATE_ACTIVE;
    else if (tm_ns_info->deleted)
	state = NS_STATE_DELETED;
    else
	state = NS_STATE_INACTIVE;

    nvsd_mgmt_get_cache_pin_config(tm_ns_info,
				   &cache_pin_max_obj_bytes,
				   &cache_pin_resv_bytes,
				   &cache_pin_enabled);

    /* Get the SSD, SAS & SATA configs from mgmt */
    nvsd_mgmt_ns_ssd_config(tm_ns_info,
			    &ns_ssd.read_size,
			    &ns_ssd.block_free_threshold,
			    &ns_ssd.group_read,
			    &ns_ssd.max_disk_usage);

    nvsd_mgmt_ns_sas_config(tm_ns_info,
			    &ns_sas.read_size,
			    &ns_sas.block_free_threshold,
			    &ns_sas.group_read,
			    &ns_sas.max_disk_usage);

    nvsd_mgmt_ns_sata_config(tm_ns_info,
			     &ns_sata.read_size,
			     &ns_sata.block_free_threshold,
			     &ns_sata.group_read,
			     &ns_sata.max_disk_usage);

    ret = ns_stat_add_namespace(nth, state, tm_ns_info->uid+1,
				cache_pin_max_obj_bytes, cache_pin_resv_bytes,
				cache_pin_enabled, &ns_ssd, &ns_sas,
				&ns_sata, 1 /* lock */);
    return -ret;
}	/* DM2_mgmt_ns_add_namespace */


/*
 * Used when "namespace unbind" or "namespace bind"
 *
 * Not needed for now.
 */
int
DM2_mgmt_ns_modify_namespace(const char *ns_uuid __attribute((unused)),
			     int new_rp_index __attribute((unused)))
{
    return 0;
}	/* DM2_mgmt_ns_add_namespace */


/*
 * Used when namespace is deleted
 */
int
DM2_mgmt_ns_delete_namespace(const char *ns_uuid)
{
    int ret;

    ret = ns_stat_del_namespace(ns_uuid);
    return ret;
}	/* DM2_mgmt_ns_delete_namespace */

int
DM2_mgmt_set_write_size_config(int size)
{
#define DM2_MM_SMALL_WRITE_MIN_SIZE (256 * 1024)
    if (size < 0 || (size > NKN_MAX_BLOCK_SZ) ||
	(size % DM2_MM_SMALL_WRITE_MIN_SIZE)) {
	DBG_DM2S("Invalid write size %d", size);
	return -1;
    }

    if (!size || size == NKN_MAX_BLOCK_SZ) {
	glob_dm2_small_write_enable = 0;
	glob_dm2_small_write_min_size = 0;
	goto update_mm;
    }

    glob_dm2_small_write_enable = 1;
    glob_dm2_small_write_min_size = size;

 update_mm:
    mm_update_write_size(SATADiskMgr_provider, glob_dm2_small_write_min_size);
    mm_update_write_size(SAS2DiskMgr_provider, glob_dm2_small_write_min_size);
    mm_update_write_size(SolidStateMgr_provider, glob_dm2_small_write_min_size);
    return 0;
}   /* DM2_mgmt_set_write_size_config */

void
dm2_mgmt_add_new_ct(dm2_mgmt_db_info_t *disk_info, int num_cache_types)
{
    dm2_cache_type_t *ct;
    char* drive_type = disk_info->mi_provider;

    if (strlen(drive_type) > DM2_MAX_CACHE_TYPE_LEN-1) {
	DBG_DM2S("drive_type too long(%zd) skipping device: %s",
		 strlen(drive_type), drive_type);
	return;
    }

    if (!strcmp(drive_type, DM2_TYPE_UNKNOWN))
	drive_type = (char *)DM2_TYPE_SATA;

    /* find the ct for this drive type */
    ct = dm2_find_cache_list(drive_type, num_cache_types);
    if (ct == NULL) {
	DBG_DM2S("Unable to find the cache type for disk %s",
		  disk_info->mi_diskname);
	return;
    }

    /* set the watermark levels and other thresholds */
    dm2_ct_update_thresholds(ct);

    /* mark that the pre-read is done as new disks would mostly be formatted */
    ct->ct_tier_preread_done = 1;
    dm2_set_glob_preread_state(ct->ct_ptype, 1);

    /* tell MM that there is a new cache type available */
    dm2_register_ptype_mm_am(ct);

    return;
}   /* dm2_mgmt_add_new_ct */

void
dm2_update_tier_preread_state(dm2_cache_type_t *ct)
{
    /* If all the active disks in a tier are 'preread'
     * mark that the preread is complete */
    if (ct->ct_num_active_caches ==
	    AO_load(&ct->ct_num_caches_preread_done)) {
	ct->ct_tier_preread_done = 1;
	dm2_set_glob_preread_state(ct->ct_ptype, 1);
	dm2_cleanup_preread_hash(ct);
	DBG_DM2S("[cache_type=%s] Preread complete in %ld seconds",
		 ct->ct_name, (nkn_cur_ts - ct->ct_dm2_preread_start_ts));
    }

    /* If all the active disks had their 'preread'
     * completed without errors, we can enable the
     * 'stat optimized' mode for the STAT calls
     */
    if (ct->ct_num_active_caches ==
	    AO_load(&ct->ct_num_caches_preread_success)) {
	ct->ct_tier_preread_stat_opt = 1;
    }

    return;
}   /* dm2_update_tier_preread_state */

void
dm2_set_glob_preread_state(nkn_provider_type_t ptype, int state)
{
    /* Set the relevant global preread variables */
    switch (ptype) {
	case SAS2DiskMgr_provider:
	    glob_dm2_sas_preread_done = state;
	    break;
	case SATADiskMgr_provider:
	    glob_dm2_sata_preread_done = state;
	    break;
	case SolidStateMgr_provider:
	    glob_dm2_ssd_preread_done = state;
	    break;
	default:
	break;
    }
    return;
}   /* dm2_set_glob_preread_state */

void
dm2_reset_preread_stop_state(void)
{

    if (glob_dm2_sata_tier_avl && glob_dm2_sata_preread_done != 1)
        return;

    if (glob_dm2_sas_tier_avl && glob_dm2_sas_preread_done != 1)
        return;

    if (glob_dm2_ssd_tier_avl && glob_dm2_ssd_preread_done != 1)
        return;

    dm2_stop_preread = 0;
    return;
}   /* dm2_set_preread_stop_state */

void
DM2_mgmt_stop_preread(void)
{
    dm2_stop_preread++;
}   /* DM2_stop_preread */


void test_me(void);
void
test_me(void)
{
    dm2_mgmt_db_info_t obj;
    dm2_mgmt_disk_status_t status;

    DM2_mgmt_get_diskcache_info(&obj, &status);
}

void my_format_disk(char *dc);
void
my_format_disk(char *dc)
{
    int value = 1;
    char out_value[80];
    /* Buggy code -- need to be fixed */
    DM2_mgmt_TM_DB_update_msg(dc, DM2_MGMT_MOD_PRE_FORMAT, &value, out_value);
    system(out_value);
    DM2_mgmt_TM_DB_update_msg(dc, DM2_MGMT_MOD_POST_FORMAT, &value, out_value);
}

void my_attr_update(char *uri);

void
my_attr_update(char *uri)
{
    nkn_attr_t *attr;
    MM_update_attr_t uattr;

    if (dm2_posix_memalign((void **)&attr, 512, 512, mod_dm2_posix_memalign)) {
	DBG_DM2S("ERROR: memory allocation failed: %d", errno);
	return;
    }
    memset(attr, 0, 512);
    attr->na_magic = NKN_ATTR_MAGIC;
    attr->na_version = NKN_ATTR_VERSION;
    uattr.in_attr = attr;
    uattr.in_ptype = SAS2DiskMgr_provider;
    uattr.uol.cod = nkn_cod_open(uri, strlen(uri), NKN_COD_DISK_MGR);
    uattr.uol.length = 0;
    uattr.uol.offset = 0;
    DM2_attribute_update(&uattr);
    dm2_free(attr, 512, 512);
}

void my_set_int_low_water_mark(char *percent);
void
my_set_int_low_water_mark(char *percent)
{
    DM2_mgmt_set_internal_water_mark("SAS", -1, atoi(percent));
}
void my_set_int_high_water_mark(char *percent);
void
my_set_int_high_water_mark(char *percent)
{
    DM2_mgmt_set_internal_water_mark("SAS", atoi(percent), -1);
}

void my_stat(char *uri);
void
my_stat(char *uri)
{
    nkn_uol_t uol;
    MM_stat_resp_t stat_op;

    uol.cod = nkn_cod_open(uri, strlen(uri), NKN_COD_DISK_MGR);
    stat_op.ptype = 5;
    stat_op.sub_ptype = 0;
    DM2_stat(uol, &stat_op);
}


void my_get(char *uri);
void
my_get(char *uri)
{
    MM_get_resp_t get;

    get.in_ptype = 5;
    get.in_sub_ptype = 0;
    get.in_uol.cod = nkn_cod_open(uri, strlen(uri),
				  NKN_COD_DISK_MGR);
    DM2_get(&get);
}

void make_convert_failed(char *dc);
void
make_convert_failed(char *mgmt_name)
{
    dm2_cache_info_t *ci;
    GList	     *ci_obj;
    int		     ct_index;

    for (ct_index=0; ct_index < glob_dm2_num_cache_types; ct_index++) {
	ci_obj = g_cache2_types[ct_index].ct_info_list;
	for (; ci_obj; ci_obj = ci_obj->next) {
	    ci = (dm2_cache_info_t *)ci_obj->data;
	    if (!strcmp(ci->mgmt_name, mgmt_name)) {
		ci->state_overall = DM2_MGMT_STATE_CONVERT_FAILURE;
		return;
	    }
	}
	ci_obj = g_cache2_types[ct_index].ct_del_info_list;
	for (; ci_obj; ci_obj = ci_obj->next) {
	    ci = (dm2_cache_info_t *)ci_obj->data;
	    if (!strcmp(ci->mgmt_name, mgmt_name)) {
		ci->state_overall = DM2_MGMT_STATE_CONVERT_FAILURE;
		return;
	    }
	}
    }
    return;
}
