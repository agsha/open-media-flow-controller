/*
 *
 * Filename:  md_nvsd_diskcache.c
 * Date:      2008/11/17b
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

#include "common.h"
#include "dso.h"
#include "mdb_db.h"
#include "md_utils.h"
#include "md_mod_commit.h"
#include "md_mod_reg.h"
#include "md_upgrade.h"
#include "proc_utils.h"
#include "file_utils.h"
#include "nkn_mgmt_defs.h"
#include <sys/mount.h>


/*BUG FIX:7322
 * Adding a new GCL session call for disk related action commands
 */
md_commit_forward_action_args md_diskcache_action_fwd_args =
{
    GCL_CLIENT_DISK, true, N_("Request failed; MFD diskcache action subsystem failed"),
    mfat_nonbarrier_async
#ifdef PROD_FEATURE_I18N_SUPPORT
    , GETTEXT_DOMAIN
#endif
};



/* ------------------------------------------------------------------------- */

static md_upgrade_rule_array *md_nvsd_diskcache_ug_rules = NULL;

int md_nvsd_diskcache_init(const lc_dso_info *info, void *data);
static int md_nvsd_diskcache_add_initial(md_commit *commit, mdb_db *db, void *arg);

static int md_nvsd_diskcache_commit_check(md_commit *commit,
                                const mdb_db *old_db, const mdb_db *new_db,
                                mdb_db_change_array *change_list, void *arg);

static int md_nvsd_diskcache_commit_apply(md_commit *commit,
                                const mdb_db *old_db, const mdb_db *new_db,
                                mdb_db_change_array *change_list, void *arg);

static int md_nvsd_check_node_free_blk_thresh(
		md_commit *commit,
		const mdb_db *old_db,
		const mdb_db *new_db,
		const mdb_db_change_array *change_list,
		const tstring *node_name,
		const tstr_array *node_name_parts,
		mdb_db_change_type change_type,
		const bn_attrib_array *old_attribs,
		const bn_attrib_array *new_attribs,
		void *arg);

int nkn_launch_process(tstring *lp_path, tstr_array *lp_argv, const char *msg);
static int md_nvsd_completion_handler(lc_child_completion_handle *cmpl_hand,
	pid_t pid, int wait_status, tbool completed,
	void *arg);

static int
md_nvsd_diskcache_upgrade_firmware_generic(md_commit *commit,
					   const mdb_db *db,
					   const char *action_name,
					   bn_binding_array *params,
					   void *cb_arg);

static int
md_nvsd_diskcache_upgrade_firware(md_commit *commit,
                                  const mdb_db *db,
                                  const char *action_name,
                                  bn_binding_array *params,
                                  void *cb_arg);

static int
md_nvsd_diskcache_upgrade_firware_finalize(md_commit *commit, const mdb_db *db,
                                  const char *action_name, void *cb_arg,
                                  void *finalize_arg, pid_t pid,
                                  int wait_status, tbool completed);

static int
md_diskcache_ps_owner_count(md_commit *commit, const mdb_db *db,
	const bn_binding *binding, void *arg);
static int
md_nvsd_handle_ps(md_commit *commit,
	const mdb_db *db,
	const char *action_name,
	bn_binding_array *params,
	void *cb_arg);
int
md_diskcache_find_diskname(md_commit *commit,
	const mdb_db *new_db,
	const char *disk_sno,
	tstring **diskname);

static int
md_nvsd_handle_ps_finalize(md_commit *commit, const mdb_db *db,
                                  const char *action_name, void *cb_arg,
                                  void *finalize_arg, pid_t pid,
                                  int wait_status, tbool completed);
/* ------------------------------------------------------------------------- */
int
md_nvsd_diskcache_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    /*
     * Creating the Nokeena specific module
     */
    err = mdm_add_module("nvsd-diskcache", 19,
                         "/nkn/nvsd/diskcache", "/nkn/nvsd/diskcache/config",
                         0,
                         NULL, NULL,
                         md_nvsd_diskcache_commit_check, NULL,
                         md_nvsd_diskcache_commit_apply, NULL,
                         0, 0,
                         md_generic_upgrade_downgrade,
                         &md_nvsd_diskcache_ug_rules,
                         md_nvsd_diskcache_add_initial, NULL,
                         NULL, NULL, NULL, NULL,
                         &module);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/evict_thread_freq";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  30;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Evict thread's wakeup frequency in secs";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/sata_adm_thres";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  12;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  1250;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "media-cache cache-tier for sata";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/ssd_adm_thres";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  1250;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  1250;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "media-cache cache-tier for ssd";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/sas_adm_thres";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  20;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  1250;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "media-cache cache-tier for sas";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/free-block/threshold/internal/sas";
    node->mrn_value_type =         bt_int8;
    node->mrn_initial_value_int =  50;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  100;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_check_node_func =     md_nvsd_check_node_free_blk_thresh;
    node->mrn_description =        "Block free threshold for SAS";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/free-block/threshold/internal/sata";
    node->mrn_value_type =         bt_int8;
    node->mrn_initial_value_int =  50;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  100;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_check_node_func =     md_nvsd_check_node_free_blk_thresh;
    node->mrn_description =        "Block free threshold for SATA";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/free-block/threshold/internal/ssd";
    node->mrn_value_type =         bt_int8;
    node->mrn_initial_value_int =  50;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  100;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_check_node_func =     md_nvsd_check_node_free_blk_thresh;
    node->mrn_description =        "Block free threshold for SSD";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/block-sharing/threshold/internal/sas";
    node->mrn_value_type =         bt_int8;
    node->mrn_initial_value_int =  25;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  100;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Block sharing threshold for SAS";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/block-sharing/threshold/internal/sata";
    node->mrn_value_type =         bt_int8;
    node->mrn_initial_value_int =  25;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  100;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Block sharing threshold for SATA";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/block-sharing/threshold/internal/ssd";
    node->mrn_value_type =         bt_int8;
    node->mrn_initial_value_int =  0;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  0;
    node->mrn_bad_value_msg	 = 	   "error: Can not configure block-sharing threshold for SSD";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Block sharing threshold for SSD";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/disk_id/*";
    node->mrn_value_type =         bt_string;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Disk IDs that are being managed by the Disk Manager";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/disk_id/*/bay";
    node->mrn_value_type =         bt_uint16;
    node->mrn_initial_value_int =  1;
    node->mrn_limit_num_min_int =  1;
    node->mrn_limit_num_max_int =  16;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Bay Number for the Disk ID";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/disk_id/*/activated";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "false";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Flag to indicate if the Disk is activated";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/disk_id/*/cache_enabled";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "false";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Flag to indicate if the cache for the Disk is activated";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/disk_id/*/auto_reformat";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "false";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Flag to indicate if the Disk can be auto reformatted on write error";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/disk_id/*/raw_partition";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Raw partition";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/disk_id/*/fs_partition";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "FS partition";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/disk_id/*/set_sector_count";
    node->mrn_value_type =         bt_uint64;
    node->mrn_initial_value_int =  0;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Sector count";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/disk_id/*/sub_provider";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Sub provider name";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/disk_id/*/commented_out";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "false";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Commented out";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/disk_id/*/missing_after_boot";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "true";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Missing after boot";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/disk_id/*/block_disk_name";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Disk Name";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/disk_id/*/stat_size_name";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Stat Size Name";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/disk_id/*/firmware_version";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Firmware version";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/disk_id/*/owner";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "DM2";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "who is owner of this disk (PS/DM2)";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/sas/group-read/enable";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "true";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Group read SATA enable";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/sata/group-read/enable";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "true";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Group read SAS enable";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/ssd/group-read/enable";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "false";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Group read SSD enable";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/cmd/rate_limit";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "true";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Rate limit CLI action requests";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/internal/sas/percent-disks";
    node->mrn_value_type =         bt_uint16;
    node->mrn_initial_value_int =  100;
    node->mrn_limit_num_min_int =  1;
    node->mrn_limit_num_max_int =  100;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Percentage of SAS disks for eviction";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/internal/sata/percent-disks";
    node->mrn_value_type =         bt_uint16;
    node->mrn_initial_value_int =  100;
    node->mrn_limit_num_min_int =  1;
    node->mrn_limit_num_max_int =  100;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Percentage of SATA disks for eviction";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/internal/ssd/percent-disks";
    node->mrn_value_type =         bt_uint16;
    node->mrn_initial_value_int =  100;
    node->mrn_limit_num_min_int =  1;
    node->mrn_limit_num_max_int =  100;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Percentage of SSD disks for eviction";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/watermark/internal/sas/high";
    node->mrn_value_type =         bt_uint16;
    node->mrn_initial_value_int =  90;
    node->mrn_limit_num_min_int =  1;
    node->mrn_limit_num_max_int =  100;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "High Watermark value for SAS";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/watermark/internal/sas/low";
    node->mrn_value_type =         bt_uint16;
    node->mrn_initial_value_int =  85;
    node->mrn_limit_num_min_int =  1;
    node->mrn_limit_num_max_int =  100;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Low Watermark value for SAS";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/watermark/internal/sata/high";
    node->mrn_value_type =         bt_uint16;
    node->mrn_initial_value_int =  90;
    node->mrn_limit_num_min_int =  1;
    node->mrn_limit_num_max_int =  100;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "High Watermark value for SATA";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/watermark/internal/sata/low";
    node->mrn_value_type =         bt_uint16;
    node->mrn_initial_value_int =  85;
    node->mrn_limit_num_min_int =  1;
    node->mrn_limit_num_max_int =  100;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Low Watermark value for SATA";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/watermark/internal/ssd/high";
    node->mrn_value_type =         bt_uint16;
    node->mrn_initial_value_int =  95;
    node->mrn_limit_num_min_int =  2;
    node->mrn_limit_num_max_int =  100;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "High Watermark value for SSD";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/watermark/internal/ssd/low";
    node->mrn_value_type =         bt_uint16;
    node->mrn_initial_value_int =  90;
    node->mrn_limit_num_min_int =  1;
    node->mrn_limit_num_max_int =  100;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Low Watermark value for SSD";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/watermark/internal/dict/high";
    node->mrn_value_type =         bt_uint16;
    node->mrn_initial_value_int =  100;
    node->mrn_limit_num_min_int =  2;
    node->mrn_limit_num_max_int =  100;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "High Watermark value for dictionary based eviction";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/watermark/internal/dict/low";
    node->mrn_value_type =         bt_uint16;
    node->mrn_initial_value_int =  99;
    node->mrn_limit_num_min_int =  1;
    node->mrn_limit_num_max_int =  100;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Low Watermark value for dictionary based eviction";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/watermark/external/sas/high";
    node->mrn_value_type =         bt_uint16;
    node->mrn_initial_value_int =  99;
    node->mrn_limit_num_min_int =  1;
    node->mrn_limit_num_max_int =  100;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "High Watermark value for SAS";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/watermark/external/sas/low";
    node->mrn_value_type =         bt_uint16;
    node->mrn_initial_value_int =  90;
    node->mrn_limit_num_min_int =  1;
    node->mrn_limit_num_max_int =  100;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Low Watermark value for SAS";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/watermark/external/sata/high";
    node->mrn_value_type =         bt_uint16;
    node->mrn_initial_value_int =  99;
    node->mrn_limit_num_min_int =  1;
    node->mrn_limit_num_max_int =  100;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "High Watermark value for SATA";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/watermark/external/sata/low";
    node->mrn_value_type =         bt_uint16;
    node->mrn_initial_value_int =  90;
    node->mrn_limit_num_min_int =  1;
    node->mrn_limit_num_max_int =  100;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Low Watermark value for SATA";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/watermark/external/ssd/high";
    node->mrn_value_type =         bt_uint16;
    node->mrn_initial_value_int =  99;
    node->mrn_limit_num_min_int =  2;
    node->mrn_limit_num_max_int =  100;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "High Watermark value for SSD";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/watermark/external/ssd/low";
    node->mrn_value_type =         bt_uint16;
    node->mrn_initial_value_int =  90;
    node->mrn_limit_num_min_int =  1;
    node->mrn_limit_num_max_int =  100;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Low Watermark value for SSD";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/internal/watermark/sata/time_low_pct";
    node->mrn_value_type =         bt_uint16;
    node->mrn_initial_value_int =  0;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  100;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/internal/watermark/sata/evict_time";
    node->mrn_value_type =         bt_time;
    node->mrn_initial_value =	   "23:00";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/internal/watermark/sas/time_low_pct";
    node->mrn_value_type =         bt_uint16;
    node->mrn_initial_value_int =  0;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  100;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/internal/watermark/sas/evict_time";
    node->mrn_value_type =         bt_time;
    node->mrn_initial_value =	   "23:00";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/internal/watermark/ssd/time_low_pct";
    node->mrn_value_type =         bt_uint16;
    node->mrn_initial_value_int =  0;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  100;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/internal/watermark/ssd/evict_time";
    node->mrn_value_type =         bt_time;
    node->mrn_initial_value =      "23:00";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 0);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/actions/upgrade-firmware";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_async_start_func       = md_nvsd_diskcache_upgrade_firware;
    node->mrn_description =        "Upgrade firmware on Intel SSD drive";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 0);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/actions/upgrade-firmware-generic";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_async_start_func       = md_nvsd_diskcache_upgrade_firmware_generic;
    node->mrn_description =        "Upgrade firmware for SSD drives";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 2);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/diskcache/actions/pre_format";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_async_nonbarrier_start_func       = md_commit_forward_action;
    node->mrn_action_arg        = &md_diskcache_action_fwd_args;
    node->mrn_actions[0]->mra_param_name = "diskname";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_actions[1]->mra_param_name = "blocks";
    node->mrn_actions[1]->mra_param_type = bt_string;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 1);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/diskcache/actions/post_format";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_async_nonbarrier_start_func       = md_commit_forward_action;
    node->mrn_action_arg        = &md_diskcache_action_fwd_args;
    node->mrn_actions[0]->mra_param_name = "diskname";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_action(module, &node, 1);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/diskcache/actions/repair";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_async_nonbarrier_start_func       = md_commit_forward_action;
    node->mrn_action_arg        = &md_diskcache_action_fwd_args;
    node->mrn_actions[0]->mra_param_name = "name";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 2);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/diskcache/actions/activate";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_async_nonbarrier_start_func       = md_commit_forward_action;
    node->mrn_action_arg        = &md_diskcache_action_fwd_args;
    node->mrn_actions[0]->mra_param_name = "name";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_actions[1]->mra_param_name = "activate";
    node->mrn_actions[1]->mra_param_type = bt_bool;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 2);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/diskcache/actions/cacheable";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_async_nonbarrier_start_func       = md_commit_forward_action;
    node->mrn_action_arg        = &md_diskcache_action_fwd_args;
    node->mrn_actions[0]->mra_param_name = "name";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_actions[1]->mra_param_name = "cacheable";
    node->mrn_actions[1]->mra_param_type = bt_bool;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 0);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/diskcache/actions/newdisk";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_async_nonbarrier_start_func       = md_commit_forward_action;
    node->mrn_action_arg        = &md_diskcache_action_fwd_args;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 0);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/diskcache/actions/show-internal-watermarks";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_async_nonbarrier_start_func       = md_commit_forward_action;
    node->mrn_action_arg        = &md_diskcache_action_fwd_args;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 0);
    bail_error(err);
    node->mrn_name		= "/nkn/nvsd/diskcache/actions/pre-read-stop";
    node->mrn_node_reg_flags	= mrf_flags_reg_action;
    node->mrn_cap_mask		= mcf_cap_action_basic;
    node->mrn_action_async_nonbarrier_start_func = md_commit_forward_action;
    node->mrn_action_arg	= &md_diskcache_action_fwd_args;
    node->mrn_description	= "Action to abort Pre-read threads";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 2);
    bail_error(err);
    node->mrn_name		= "/nkn/nvsd/diskcache/actions/ps-format";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_async_start_func       = md_nvsd_handle_ps;
    node->mrn_action_arg	= (void *)"format";
    node->mrn_description	= "format the disk to persistent-store";
    node->mrn_actions[0]->mra_param_name = "diskname";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_actions[1]->mra_param_name = "disk_id";
    node->mrn_actions[1]->mra_param_type = bt_string;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 3);
    bail_error(err);
    node->mrn_name		= "/nkn/nvsd/diskcache/actions/mount-xfs";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_async_start_func       = md_nvsd_handle_ps;
    node->mrn_action_arg	= (void *)"mount";
    node->mrn_description	= "mount the disk xfs";
    node->mrn_actions[0]->mra_param_name = "diskname";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_actions[1]->mra_param_name = "disk_id";
    node->mrn_actions[1]->mra_param_type = bt_string;
    /* if true, mount disk. else unmount disk */
    node->mrn_actions[2]->mra_param_name = "mount";
    node->mrn_actions[2]->mra_param_type = bt_bool;
    err = mdm_add_node(module, &node);
    bail_error(err);
    /*
     * Monitor nodes
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/diskcache/monitor/disk_id/*";
    node->mrn_value_type 	= bt_string;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_wildcard;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_disk;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/diskcache/monitor/disk_id/*/diskname";
    node->mrn_value_type 	= bt_string;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_disk;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/diskcache/monitor/disk_id/*/disktype";
    node->mrn_value_type 	= bt_string;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_disk;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/diskcache/monitor/disk_id/*/tier";
    node->mrn_value_type 	= bt_string;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_disk;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/diskcache/monitor/disk_id/*/diskstate";
    node->mrn_value_type 	= bt_string;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_disk;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/diskcache/monitor/diskname/*";
    node->mrn_value_type 	= bt_string;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_wildcard;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_disk;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/diskcache/monitor/diskname/*/disk_id";
    node->mrn_value_type 	= bt_string;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_disk;
    node->mrn_description 	= "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/pre-load/enable";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "true";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "controls enabling/disabling of cache dictionary";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/diskcache/config/write-size";
    node->mrn_value_type =         bt_uint32;
    /* 0 is the default value, meaning all writes will be on block-sizes
     * max value is 2M as it is the max block size that we support */
    node->mrn_initial_value_int =  0;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  2097152;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Minimum size of data to buffer before writing to disk (in bytes)";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_event(module, &node, 2);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/diskcache/events/disk_failure";
    node->mrn_node_reg_flags = mrf_flags_reg_event;
    node->mrn_cap_mask = mcf_cap_event_privileged;
    node->mrn_description = "A disk failure has happened";
    node->mrn_events[0]->mre_binding_name = "disk_name";
    node->mrn_events[0]->mre_binding_type = bt_string;
    node->mrn_events[1]->mre_binding_name = "disk_sno";
    node->mrn_events[1]->mre_binding_type = bt_string;
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* Upgrade processing nodes */

    err = md_upgrade_rule_array_new(&md_nvsd_diskcache_ug_rules);
    bail_error(err);
    ra = md_nvsd_diskcache_ug_rules;

    /*! Added in module version 2
     */
    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/disk_id/*/block_disk_name";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/disk_id/*/stat_size_name";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/free-block/threshold/internal/sas";
    rule->mur_new_value_type =  bt_int8;
    rule->mur_new_value_str =   "50";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/free-block/threshold/internal/sata";
    rule->mur_new_value_type =  bt_int8;
    rule->mur_new_value_str =   "50";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/free-block/threshold/internal/ssd";
    rule->mur_new_value_type =  bt_int8;
    rule->mur_new_value_str =   "50";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/sas/group-read/enable";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "true";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/sata/group-read/enable";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "true";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/ssd/group-read/enable";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "true";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/ssd/group-read/enable";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_modify_cond_value_str = "true";
    rule->mur_new_value_str =   "false";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = false;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 5, 6);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/cmd/rate_limit";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "true";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/sata_adm_thres";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "0";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/sas_adm_thres";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "0";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/ssd_adm_thres";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "0";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 7, 8);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/block-sharing/threshold/internal/sas";
    rule->mur_new_value_type =  bt_int8;
    rule->mur_new_value_str =   "25";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 7, 8);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/block-sharing/threshold/internal/sata";
    rule->mur_new_value_type =  bt_int8;
    rule->mur_new_value_str =   "25";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 7, 8);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/block-sharing/threshold/internal/ssd";
    rule->mur_new_value_type =  bt_int8;
    rule->mur_new_value_str =   "0";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 8, 9);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/watermark/internal/sas/high";
    rule->mur_new_value_type =  bt_uint16;
    rule->mur_new_value_str =   "98";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 8, 9);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/watermark/internal/sas/low";
    rule->mur_new_value_type =  bt_uint16;
    rule->mur_new_value_str =   "90";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 8, 9);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/watermark/internal/sata/high";
    rule->mur_new_value_type =  bt_uint16;
    rule->mur_new_value_str =   "98";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 8, 9);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/watermark/internal/sata/low";
    rule->mur_new_value_type =  bt_uint16;
    rule->mur_new_value_str =   "90";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 8, 9);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/watermark/internal/ssd/high";
    rule->mur_new_value_type =  bt_uint16;
    rule->mur_new_value_str =   "99";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 8, 9);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/watermark/internal/ssd/low";
    rule->mur_new_value_type =  bt_uint16;
    rule->mur_new_value_str =   "92";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 8, 9);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/pre-load/enable";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 9, 10);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/pre-load/enable";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "true";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 10, 11);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/sata_adm_thres";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "12";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 10, 11);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/sas_adm_thres";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "20";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 10, 11);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/ssd_adm_thres";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "1250";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 11, 12);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/watermark/external/sas/high";
    rule->mur_new_value_type =  bt_uint16;
    rule->mur_new_value_str =   "90";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 11, 12);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/watermark/external/sas/low";
    rule->mur_new_value_type =  bt_uint16;
    rule->mur_new_value_str =   "85";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 11, 12);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/watermark/external/sata/high";
    rule->mur_new_value_type =  bt_uint16;
    rule->mur_new_value_str =   "90";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 11, 12);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/watermark/external/sata/low";
    rule->mur_new_value_type =  bt_uint16;
    rule->mur_new_value_str =   "85";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 11, 12);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/watermark/external/ssd/high";
    rule->mur_new_value_type =  bt_uint16;
    rule->mur_new_value_str =   "90";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 11, 12);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/watermark/external/ssd/low";
    rule->mur_new_value_type =  bt_uint16;
    rule->mur_new_value_str =   "85";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 12, 13);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/disk_id/*/owner";
    rule->mur_new_value_type =  bt_string;
    /* by default all disks will be dm2 */
    rule->mur_new_value_str =   "DM2";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 13, 14);
    rule->mur_upgrade_type =    MUTT_DELETE;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/free-block/threshold/internal/sas";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 13, 14);
    rule->mur_upgrade_type =    MUTT_DELETE;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/free-block/threshold/internal/sata";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 13, 14);
    rule->mur_upgrade_type =    MUTT_DELETE;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/free-block/threshold/internal/ssd";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 13, 14);
    rule->mur_upgrade_type =    MUTT_DELETE;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/sas/group-read/enable";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 13, 14);
    rule->mur_upgrade_type =    MUTT_DELETE;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/sata/group-read/enable";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 13, 14);
    rule->mur_upgrade_type =    MUTT_DELETE;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/ssd/group-read/enable";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 14, 15);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/internal/sata/percent-disks";
    rule->mur_new_value_type =  bt_uint16;
    rule->mur_new_value_str =   "100";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 14, 15);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/internal/sas/percent-disks";
    rule->mur_new_value_type =  bt_uint16;
    rule->mur_new_value_str =   "100";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 14, 15);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/internal/ssd/percent-disks";
    rule->mur_new_value_type =  bt_uint16;
    rule->mur_new_value_str =   "100";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 15, 16);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/write-size";
    rule->mur_new_value_type =  bt_uint32;
    /* by default all disks will be dm2 */
    rule->mur_new_value_str =   "0";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 16, 17);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/internal/watermark/sata/time_low_pct";
    rule->mur_new_value_type =  bt_uint16;
    /* by default all disks will be dm2 */
    rule->mur_new_value_str =   "0";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 16, 17);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/internal/watermark/sata/evict_time";
    rule->mur_new_value_type =  bt_time;
    /* by default all disks will be dm2 */
    rule->mur_new_value_str =   "23:00";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 16, 17);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/internal/watermark/sas/time_low_pct";
    rule->mur_new_value_type =  bt_uint16;
    /* by default all disks will be dm2 */
    rule->mur_new_value_str =   "0";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 16, 17);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/internal/watermark/sas/evict_time";
    rule->mur_new_value_type =  bt_time;
    /* by default all disks will be dm2 */
    rule->mur_new_value_str =   "23:00";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);


    MD_NEW_RULE(ra, 16, 17);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/internal/watermark/ssd/time_low_pct";
    rule->mur_new_value_type =  bt_uint16;
    /* by default all disks will be dm2 */
    rule->mur_new_value_str =   "0";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 16, 17);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/internal/watermark/ssd/evict_time";
    rule->mur_new_value_type =  bt_time;
    /* by default all disks will be dm2 */
    rule->mur_new_value_str =   "23:00";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 17, 18);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/watermark/internal/dict/high";
    rule->mur_new_value_type =  bt_uint16;
    rule->mur_new_value_str =   "100";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 17, 18);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/watermark/internal/dict/low";
    rule->mur_new_value_type =  bt_uint16;
    rule->mur_new_value_str =   "99";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 18, 19);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/diskcache/config/disk_id/*/firmware_version";
    rule->mur_new_value_type =  bt_string;
    rule->mur_new_value_str =   "";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
const bn_str_value md_nvsd_diskcache_initial_values[] = {
};


/* ------------------------------------------------------------------------- */
static int
md_nvsd_diskcache_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
    int err = 0;
    bn_binding *binding = NULL;

    err = mdb_create_node_bindings
        (commit, db, md_nvsd_diskcache_initial_values,
         sizeof(md_nvsd_diskcache_initial_values) / sizeof(bn_str_value));
    bail_error(err);

 bail:
    bn_binding_free(&binding);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_nvsd_diskcache_commit_check(md_commit *commit,
                     const mdb_db *old_db,
                     const mdb_db *new_db,
                     mdb_db_change_array *change_list, void *arg)
{
    int err = 0, num_changes=0;
    const mdb_db_change *change = NULL;
    tstring *t_owner = NULL;

    num_changes = mdb_db_change_array_length_quick(change_list);

    for (int i = 0; i < num_changes; i++)
    {
	change = mdb_db_change_array_get_quick(change_list, i );
	bail_null(change);

	if ((bn_binding_name_pattern_match(ts_str(change->mdc_name),
			"/nkn/nvsd/diskcache/config/watermark/external/sas/high"))
		&& (mdct_modify == change->mdc_change_type)) {

	    uint16 int_sas_high =0;
	    uint16 ext_sas_low = 0;
	    uint16 ext_sas_high =0;

	    err = mdb_get_node_value_uint16(commit, new_db,
		    "/nkn/nvsd/diskcache/config/watermark/external/sas/low", 0,
		    NULL, &ext_sas_low);
	    bail_error(err);

	    err = mdb_get_node_value_uint16(commit, new_db,
		    "/nkn/nvsd/diskcache/config/watermark/internal/sas/high", 0,
		    NULL, &int_sas_high);
	    bail_error(err);

	    err = mdb_get_node_value_uint16(commit, new_db,
		    ts_str(change->mdc_name), 0,
		    NULL, &ext_sas_high);
	    bail_error(err);

	    if(ext_sas_low > ext_sas_high){

		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("Bad watermark value. Low watermark level Should not be greater than high watermark level\n"));
		bail_error(err);
		goto bail;
	    }

	    if (int_sas_high >= ext_sas_high)
	    {
		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("Bad Watermark value. External High Watermark level for SAS should be higher than Internal High Watermark level for SAS\n"));
		bail_error(err);

		goto bail;

	    }

	}
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 8, 
			"nkn", "nvsd", "diskcache", "config", "watermark", "external", "sata", "high"))
		&& (mdct_modify == change->mdc_change_type)) {

	    uint16 int_sata_high =0;
	    uint16 ext_sata_high =0;
	    uint16 ext_sata_low =0;

	    err = mdb_get_node_value_uint16(commit, new_db,
		    "/nkn/nvsd/diskcache/config/watermark/external/sata/low", 0,
		    NULL, &ext_sata_low);
	    bail_error(err);

	    err = mdb_get_node_value_uint16(commit, new_db,
		    "/nkn/nvsd/diskcache/config/watermark/internal/sata/high", 0,
		    NULL, &int_sata_high);
	    bail_error(err);

	    err = mdb_get_node_value_uint16(commit, new_db,
		    ts_str(change->mdc_name), 0,
		    NULL, &ext_sata_high);
	    bail_error(err);

	    if(ext_sata_low > ext_sata_high){

		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("Bad watermark value. Low watermark level Should not be greater than high watermark level\n"));
		bail_error(err);
		goto bail;
	    }

	    if (int_sata_high >= ext_sata_high)
	    {
		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("Bad Watermark value. External High Watermark level for SATA should be higher than Internal High Watermark level for SATA\n"));
		bail_error(err);

		goto bail;

	    }

	}else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 8, 
			"nkn", "nvsd", "diskcache", "config", "watermark", "external", "ssd", "high"))
		&& (mdct_modify == change->mdc_change_type)) {

	    uint16 int_ssd_high =0;
	    uint16 ext_ssd_high =0;
	    uint16 ext_ssd_low = 0;

	    err = mdb_get_node_value_uint16(commit, new_db,
		    "/nkn/nvsd/diskcache/config/watermark/external/ssd/low", 0,
		    NULL, &ext_ssd_low);
	    bail_error(err);

	    err = mdb_get_node_value_uint16(commit, new_db,
		    "/nkn/nvsd/diskcache/config/watermark/internal/ssd/high", 0,
		    NULL, &int_ssd_high);
	    bail_error(err);

	    err = mdb_get_node_value_uint16(commit, new_db,
		    ts_str(change->mdc_name), 0,
		    NULL, &ext_ssd_high);
	    bail_error(err);

	    if(ext_ssd_low > ext_ssd_high){

		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("Bad watermark value. Low watermark level Should not be greater than high watermark level\n"));
		bail_error(err);
		goto bail;
	    }

	    if (int_ssd_high >= ext_ssd_high)
	    {
		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("Bad Watermark value. External High Watermark level for SSD should be higher than Internal High Watermark level for SSD\n"));
		bail_error(err);

		goto bail;

	    }

	}else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 8, 
			"nkn", "nvsd", "diskcache", "config", "watermark", "internal", "sas", "high"))
		&& (mdct_modify == change->mdc_change_type)) {

	    uint16 int_sas_high =0;
	    uint16 int_sas_low = 0;
	    uint16 ext_sas_high =0;

	    err = mdb_get_node_value_uint16(commit, new_db,
		    "/nkn/nvsd/diskcache/config/watermark/external/sas/high", 0,
		    NULL, &ext_sas_high);
	    bail_error(err);

	    err = mdb_get_node_value_uint16(commit, new_db,
		    ts_str(change->mdc_name), 0,
		    NULL, &int_sas_high);
	    bail_error(err);

	    err = mdb_get_node_value_uint16(commit, new_db,
		    "/nkn/nvsd/diskcache/config/watermark/internal/sas/low", 0,
		    NULL, &int_sas_low);
	    bail_error(err);

	    if(int_sas_low > int_sas_high){

		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("Bad watermark value. Low watermark level Should not be greater than high watermark level\n"));
		bail_error(err);
		goto bail;
	    }

	    if (int_sas_high >= ext_sas_high)
	    {
		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("Bad Watermark value. External High Watermark level for SAS should be higher than Internal High Watermark level for SAS\n"));
		bail_error(err);

		goto bail;

	    }

	}
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 8, 
			"nkn", "nvsd", "diskcache", "config", "watermark", "internal", "sata", "high"))
		&& (mdct_modify == change->mdc_change_type)) {

	    uint16 int_sata_high =0;
	    uint16 int_sata_low = 0;
	    uint16 ext_sata_high =0;

	    err = mdb_get_node_value_uint16(commit, new_db,
		    "/nkn/nvsd/diskcache/config/watermark/external/sata/high", 0,
		    NULL, &ext_sata_high);
	    bail_error(err);

	    err = mdb_get_node_value_uint16(commit, new_db,
		    ts_str(change->mdc_name), 0,
		    NULL, &int_sata_high);
	    bail_error(err);

	    err = mdb_get_node_value_uint16(commit, new_db,
		    "/nkn/nvsd/diskcache/config/watermark/internal/sata/low", 0,
		    NULL, &int_sata_low);
	    bail_error(err);

	    if(int_sata_low > int_sata_high){

		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("Bad watermark value. Low watermark level Should not be greater than high watermark level\n"));
		bail_error(err);
		goto bail;
	    }

	    if (int_sata_high >= ext_sata_high)
	    {
		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("Bad Watermark value. External High Watermark level for SATA should be higher than Internal High Watermark level for SATA\n"));
		bail_error(err);

		goto bail;

	    }

	}else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 8, 
			"nkn", "nvsd", "diskcache", "config", "watermark", "internal", "ssd", "high"))
		&& (mdct_modify == change->mdc_change_type)) {

	    uint16 int_ssd_high =0;
	    uint16 int_ssd_low = 0;
	    uint16 ext_ssd_high =0;

	    err = mdb_get_node_value_uint16(commit, new_db,
		    "/nkn/nvsd/diskcache/config/watermark/external/ssd/high", 0,
		    NULL, &ext_ssd_high);
	    bail_error(err);

	    err = mdb_get_node_value_uint16(commit, new_db,
		    ts_str(change->mdc_name), 0,
		    NULL, &int_ssd_high);
	    bail_error(err);

	    err = mdb_get_node_value_uint16(commit, new_db,
		    "/nkn/nvsd/diskcache/config/watermark/internal/ssd/low", 0,
		    NULL, &int_ssd_low);
	    bail_error(err);

	    if(int_ssd_low > int_ssd_high){

		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("Bad watermark value. Low watermark level Should not be greater than high watermark level\n"));
		bail_error(err);
		goto bail;
	    }

	    if (int_ssd_high >= ext_ssd_high)
	    {
		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("Bad Watermark value. External High Watermark level for SSD should be higher than Internal High Watermark level for SSD\n"));
		bail_error(err);

		goto bail;

	    }

	}

	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 8, 
			"nkn", "nvsd", "diskcache", "config", "watermark", "external", "sas", "low"))
		&& (mdct_modify == change->mdc_change_type)) {

	    uint16 ext_sas_low = 0;
	    uint16 ext_sas_high =0;

	    err = mdb_get_node_value_uint16(commit, new_db,
		    "/nkn/nvsd/diskcache/config/watermark/external/sas/high", 0,
		    NULL, &ext_sas_high);
	    bail_error(err);

	    err = mdb_get_node_value_uint16(commit, new_db,
		    ts_str(change->mdc_name), 0,
		    NULL, &ext_sas_low);
	    bail_error(err);

	    if(ext_sas_low > ext_sas_high){

		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("Bad watermark value. Low watermark level Should not be greater than high watermark level\n"));
		bail_error(err);
		goto bail;
	    }

	}
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 8, 
			"nkn", "nvsd", "diskcache", "config", "watermark", "external", "sata", "low"))
		&& (mdct_modify == change->mdc_change_type)) {

	    uint16 ext_sata_high =0;
	    uint16 ext_sata_low =0;

	    err = mdb_get_node_value_uint16(commit, new_db,
		    "/nkn/nvsd/diskcache/config/watermark/external/sata/high", 0,
		    NULL, &ext_sata_high);
	    bail_error(err);

	    err = mdb_get_node_value_uint16(commit, new_db,
		    ts_str(change->mdc_name), 0,
		    NULL, &ext_sata_low);
	    bail_error(err);

	    if(ext_sata_low > ext_sata_high){

		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("Bad watermark value. Low watermark level Should not be greater than high watermark level\n"));
		bail_error(err);
		goto bail;
	    }
	}else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 8, 
			"nkn", "nvsd", "diskcache", "config", "watermark", "external", "ssd", "low"))
		&& (mdct_modify == change->mdc_change_type)) {

	    uint16 ext_ssd_high =0;
	    uint16 ext_ssd_low = 0;

	    err = mdb_get_node_value_uint16(commit, new_db,
		    "/nkn/nvsd/diskcache/config/watermark/external/ssd/high", 0,
		    NULL, &ext_ssd_high);
	    bail_error(err);

	    err = mdb_get_node_value_uint16(commit, new_db,
		    ts_str(change->mdc_name), 0,
		    NULL, &ext_ssd_low);
	    bail_error(err);

	    if(ext_ssd_low > ext_ssd_high){

		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("Bad watermark value. Low watermark level Should not be greater than high watermark level\n"));
		bail_error(err);
		goto bail;
	    }

	}else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 8,
			"nkn", "nvsd", "diskcache", "config", "watermark", "internal", "sas", "low"))
		&& (mdct_modify == change->mdc_change_type)) {

	    uint16 int_sas_high =0;
	    uint16 int_sas_low = 0;

	    err = mdb_get_node_value_uint16(commit, new_db,
		    ts_str(change->mdc_name), 0,
		    NULL, &int_sas_low);
	    bail_error(err);

	    err = mdb_get_node_value_uint16(commit, new_db,
		    "/nkn/nvsd/diskcache/config/watermark/internal/sas/high", 0,
		    NULL, &int_sas_high);
	    bail_error(err);

	    if(int_sas_low > int_sas_high){

		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("Bad watermark value. Low watermark level Should not be greater than high watermark level\n"));
		bail_error(err);
		goto bail;
	    }

	}
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 8, 
			"nkn", "nvsd", "diskcache", "config", "watermark", "internal", "sata", "low"))
		&& (mdct_modify == change->mdc_change_type)) {

	    uint16 int_sata_high =0;
	    uint16 int_sata_low = 0;

	    err = mdb_get_node_value_uint16(commit, new_db,
		    ts_str(change->mdc_name), 0,
		    NULL, &int_sata_low);
	    bail_error(err);

	    err = mdb_get_node_value_uint16(commit, new_db,
		    "/nkn/nvsd/diskcache/config/watermark/internal/sata/high", 0,
		    NULL, &int_sata_high);
	    bail_error(err);

	    if(int_sata_low > int_sata_high){

		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("Bad watermark value. Low watermark level Should not be greater than high watermark level\n"));
		bail_error(err);
		goto bail;
	    }

	}else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 8, 
			"nkn", "nvsd", "diskcache", "config", "watermark", "internal", "ssd", "low"))
		&& (mdct_modify == change->mdc_change_type)) {

	    uint16 int_ssd_high =0;
	    uint16 int_ssd_low = 0;

	    err = mdb_get_node_value_uint16(commit, new_db,
		    ts_str(change->mdc_name), 0,
		    NULL, &int_ssd_low);
	    bail_error(err);

	    err = mdb_get_node_value_uint16(commit, new_db,
		    "/nkn/nvsd/diskcache/config/watermark/internal/ssd/high", 0,
		    NULL, &int_ssd_high);
	    bail_error(err);

	    if(int_ssd_low > int_ssd_high){

		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("Bad watermark value. Low watermark level Should not be greater than high watermark level\n"));
		bail_error(err);
		goto bail;
	    }


	}else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 8, 
			"nkn", "nvsd", "diskcache", "config", "watermark", "internal", "dict", "high"))
		&& (mdct_modify == change->mdc_change_type)) {

	    uint16 int_hwm = 0;
	    uint16 int_lwm = 0;

	    err = mdb_get_node_value_uint16(commit, new_db,
		    ts_str(change->mdc_name), 0,
		    NULL, &int_hwm);
	    bail_error(err);

	    err = mdb_get_node_value_uint16(commit, new_db,
		    "/nkn/nvsd/diskcache/config/watermark/internal/dict/low", 0,
		    NULL, &int_lwm);
	    bail_error(err);

	    if(int_lwm > int_hwm){

		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("Bad watermark value. Low watermark level Should not be greater than high watermark level\n"));
		bail_error(err);
		goto bail;
	    }

	}else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 8, 
			"nkn", "nvsd", "diskcache", "config", "watermark", "internal", "dict", "low"))
		&& (mdct_modify == change->mdc_change_type)) {

	    uint16 int_hwm = 0;
	    uint16 int_lwm = 0;

	    err = mdb_get_node_value_uint16(commit, new_db,
		    ts_str(change->mdc_name), 0,
		    NULL, &int_lwm);
	    bail_error(err);

	    err = mdb_get_node_value_uint16(commit, new_db,
		    "/nkn/nvsd/diskcache/config/watermark/internal/dict/high", 0,
		    NULL, &int_hwm);
	    bail_error(err);

	    if(int_lwm > int_hwm){

		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("Bad watermark value. Low watermark level Should not be greater than high watermark level\n"));
		bail_error(err);
		goto bail;
	    }

	}else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7,
			"nkn", "nvsd", "diskcache", "config" ,"disk_id", "*", "owner"))
		&& (mdct_modify == change->mdc_change_type)) {
	    uint32_t num_ps_owners = 0;
	    node_name_t node_name = {0};
	    tbool initial = false, enabled = false;

	    /* don't do anything in case this is initial mode */
	    err = md_commit_get_commit_initial(commit, &initial);
	    bail_error(err);
	    if (initial)
		continue;
	    /* check if the disk is cached disabled and status inactive */
	    snprintf(node_name, sizeof(node_name),
		    "/nkn/nvsd/diskcache/config/disk_id/%s/activated",
		    tstr_array_get_str_quick(change->mdc_name_parts, 5));
	    err = mdb_get_node_value_tbool(commit, new_db, node_name,
		    0, NULL, &enabled);
	    bail_error(err);

	    if (enabled) {
		err = md_commit_set_return_status_msg_fmt(commit, EINVAL,
			"disk must be inactive.");
		bail_error(err);
		goto bail;
	    }
	    err = mdb_foreach_binding_cb(commit, new_db,
		    "/nkn/nvsd/diskcache/config/disk_id/*/owner",
		    0, md_diskcache_ps_owner_count, &num_ps_owners);
	    bail_error(err);

	    if (num_ps_owners > 1) {
		/* this is disallowed */
		err = md_commit_set_return_status_msg_fmt(commit, EINVAL,
			"only one disk is allowed as persistent-store");
		bail_error(err);
		goto bail;
	    }
	} else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7,
			"nkn", "nvsd", "diskcache", "config", "disk_id", "*", "owner"))
		&& (mdct_add == change->mdc_change_type)) {
	    /* check that we have only one disk as persistent store */
	    uint32_t num_ps_owners = 0;

	    ts_free(&t_owner);
	    err = mdb_get_node_value_tstr(commit, new_db,
		    ts_str(change->mdc_name), 0, NULL, &t_owner);
	    bail_error(err);

	    if (t_owner == NULL || (strcmp(ts_str(t_owner), "PS"))) {
		/* not a persistent-disk, we are not interested */
		continue;
	    }

	    err = mdb_foreach_binding_cb(commit, new_db,
		    "/nkn/nvsd/diskcache/config/disk_id/*/owner",
		    0, md_diskcache_ps_owner_count, &num_ps_owners);
	    bail_error(err);

	    if (num_ps_owners > 1) {
		/* this is disallowed */
		err = md_commit_set_return_status_msg_fmt(commit, EINVAL,
			"only one disk is allowed as persistent-store");
		bail_error(err);
		goto bail;
	    }
	}
    }

    err = md_commit_set_return_status(commit, 0);
    bail_error(err);

bail:
    ts_free(&t_owner);
    return(err);
}

static int
md_diskcache_ps_owner_count(md_commit *commit, const mdb_db *db,
	const bn_binding *binding, void *arg)
{
    int err = 0;
    uint32_t *num_count = 0;
    char *owner = NULL;

    bail_require(arg);
    num_count = (uint32_t *)arg;

    err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &owner);
    bail_error(err);
    if (owner && (0 == strcmp(owner, "PS"))) {
	/* counter number of persistent disks */
    	*num_count = *num_count + 1;
    }

bail:
    safe_free(owner);
    return err;
}

/* ------------------------------------------------------------------------- */
static int
md_nvsd_diskcache_commit_apply(md_commit *commit,
                     const mdb_db *old_db, const mdb_db *new_db,
                     mdb_db_change_array *change_list, void *arg)
{
    int err = 0, num_changes = 0, i = 0 ;
    const mdb_db_change *change = NULL;
    tbool enabled = false, is_exist = false;
    tstring *t_owner = NULL, *t_disk_name = NULL,
	    *lp_path = NULL;
    tstr_array *lp_argv = NULL;

    num_changes = mdb_db_change_array_length_quick(change_list);
    for (i = 0; i < num_changes; i++)
    {
	change = mdb_db_change_array_get_quick(change_list, i );
	bail_null(change);

	if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6,
			"nkn", "nvsd", "diskcache", "config", "pre-load", "enable"))
		&& (mdct_modify == change->mdc_change_type)) {
	    err = mdb_get_node_value_tbool(commit, new_db,
		    ts_str(change->mdc_name), 0, NULL, &enabled);
	    bail_error(err);

	    if (enabled) {
		lf_test_exists(DICTIONARY_PRELOAD_FILE, &is_exist);
		if (is_exist) {
		    lf_remove_file(DICTIONARY_PRELOAD_FILE);
		}
	    } else {
		lf_touch_file(DICTIONARY_PRELOAD_FILE,
			S_IRUSR | S_IWUSR |
			S_IRGRP | S_IWGRP |
			S_IROTH | S_IWOTH);
	    }
	} else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7,
			"nkn", "nvsd", "diskcache", "config", "disk_id", "*", "owner"))
		&& (mdct_modify == change->mdc_change_type)) {
	    node_name_t node_name = {0};
	    tbool initial = false;

	    /* don't do anything in case this is initial mode */
	    err = md_commit_get_commit_initial(commit, &initial);
	    bail_error(err);
	    if (initial)
		continue;

	    err = mdb_get_node_value_tstr(commit, new_db,
		    ts_str(change->mdc_name), 0, NULL, &t_owner);
	    bail_error_null(err, t_owner);

	    if (ts_equal_str(t_owner, "", false)) {
		/* owner field is not set, ignoreing for now */
		ts_free(&t_owner);
		continue;
	    }
	    snprintf(node_name, sizeof(node_name),
		    "/nkn/nvsd/diskcache/config/disk_id/%s/block_disk_name",
		    tstr_array_get_str_quick(change->mdc_name_parts, 5));

	    err = mdb_get_node_value_tstr(commit, new_db,
		    node_name, 0, NULL, &t_disk_name);
	    bail_error_null(err, t_disk_name);

	    if (ts_equal_str(t_disk_name, "", false)) {
		ts_free(&t_disk_name);
		ts_free(&t_owner);
		continue;
	    }

	    if (ts_equal_str(t_owner, "PS", false)) {
		/* not doing anything here */
	    } else if (ts_equal_str(t_owner, "DM2", false)) {
		/* call delete_ps_disk.sh /dev/sda */
		err = ts_new_str(&lp_path, "/opt/nkn/bin/delete_ps_disk.sh");
		bail_error(err);
		err = tstr_array_new_va_str(&lp_argv, NULL, 2,
			"/opt/nkn/bin/delete_ps_disk.sh", ts_str(t_disk_name));
		bail_error(err);

		err = nkn_launch_process(lp_path, lp_argv, "delete-ps");
		bail_error(err);
	    } else {
		/* MUST NOT HAVE HAPPENED, ignoring as of now */
	    }
	    ts_free(&t_disk_name);
	    ts_free(&t_owner);
	} else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7, 
			"nkn", "nvsd", "diskcache", "config", "disk_id", "*", "owner"))
		&& (mdct_add == change->mdc_change_type)) {
	    /* for start-up */
	    node_name_t node_name = {0};

	    err = mdb_get_node_value_tstr(commit, new_db,
		    ts_str(change->mdc_name), 0, NULL, &t_owner);
	    bail_error_null(err, t_owner);

	    if (ts_equal_str(t_owner, "", false)) {
		/* owner field is not set, ignoreing for now */
		ts_free(&t_owner);
		continue;
	    }
	    if (ts_equal_str(t_owner, "PS", false)) {
		char device_name[256] = {0};
		file_path_t mount_path = {0};
		tstring *diskname = NULL;

		/* check if disk is mounted and mount disk if not mounted */
		snprintf(node_name, sizeof(node_name),
			"/nkn/nvsd/diskcache/config/disk_id/%s/block_disk_name",
			tstr_array_get_str_quick(change->mdc_name_parts, 5));

		err = mdb_get_node_value_tstr(commit, new_db,
			node_name, 0, NULL, &t_disk_name);
		bail_error_null(err, t_disk_name);

		if (ts_equal_str(t_disk_name, "", false)) {
		    /* not a valid device name */
		    ts_free(&t_disk_name);
		    ts_free(&t_owner);
		    continue;
		}

		/* make /dev/sdb1 */
		snprintf(device_name, sizeof(device_name),
			"%s1",ts_str(t_disk_name));

		/* make  /nkn/pers/ps_1 ,
		 * ps_1 is hardcoded now, as we don't support multiplei
		 * ps-device
		 */
		snprintf(mount_path, sizeof(mount_path),
			"/nkn/pers/ps_1");

		err = lf_ensure_dir(mount_path, 0755);
		bail_error(err);

		/*  mount(src, target, fstype, flags, data); */
		err = mount(device_name, mount_path, "xfs", 0, NULL);
		if (err == -1) {
		    if (errno == EBUSY) {
			/* device is already mounted, ignore */
			lc_log_basic(LOG_INFO,
				"disk %s is already mounted", device_name);
		    } else {
			lc_log_basic(LOG_NOTICE,
				"error in mounting %s disk", device_name);
			err = md_commit_set_return_status_msg_fmt
			    (commit, errno, "error in mounting %s disk",
			     device_name);
			bail_error(err);
		    }
		}
		/* no bail_error() here */
	    }
	}
    }

bail:
    ts_free(&t_disk_name);
    ts_free(&t_owner);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int md_nvsd_check_node_free_blk_thresh(
		md_commit *commit,
		const mdb_db *old_db,
		const mdb_db *new_db,
		const mdb_db_change_array *change_list,
		const tstring *node_name,
		const tstr_array *node_name_parts,
		mdb_db_change_type change_type,
		const bn_attrib_array *old_attribs,
		const bn_attrib_array *new_attribs,
		void *arg)
{
    int err = 0;
    uint32 i = 0;
    int8 threshold = 0;
    const bn_attrib *new_val = NULL;
    int valid = 0; /* Always assume that the value is invalid */

    const int valid_values[] = {0, 25, 50, 75, 100};

    /* Check if the value being set is 0, 25, 50, 75 or 100
     * These are the only valid values that we allow.
     */
    new_val = bn_attrib_array_get_quick(new_attribs, ba_value);
    if (new_val) {
	err = bn_attrib_get_int8(new_val, NULL, NULL, &threshold);
	bail_error(err);

	for(i = 0; i < (sizeof(valid_values)/sizeof(int)); i++) {
	    if ( threshold == valid_values[i] ) {
		valid = 1;
		break;
	    }
	}

	if (!valid) {
	    err = md_commit_set_return_status_msg_fmt(commit, 1,
		    _("error: invalid value '%d'"), threshold);
	    bail_error(err);
	    goto bail;
	}
    }

    /* Acceptable change !! */
    err = md_commit_set_return_status(commit, 0);
    bail_error(err);
bail:
    return err;
}

int
nkn_launch_process(tstring *lp_path, tstr_array *lp_argv, const char *msg)
{
    int err = 0;
    lc_launch_params *lp = NULL;
    lc_launch_result lr;

    if (lp_path == NULL || lp_argv == NULL) {
	err = EINVAL;
	goto bail;
    }

    err = lc_launch_params_get_defaults(&lp);
    bail_error(err);

    lp->lp_path = lp_path;
    lp->lp_argv = lp_argv;

    lp->lp_wait = false;
    lp->lp_env_opts = eo_preserve_all;
    lp->lp_log_level = LOG_DEBUG;

    lp->lp_io_origins[lc_io_stdout].lio_opts = lioo_devnull;
    lp->lp_io_origins[lc_io_stderr].lio_opts = lioo_devnull;

    err = lc_launch(lp, &lr);
    bail_error(err);
    bail_require(lr.lr_child_pid > 0);

    err = lc_child_completion_register
	(md_cmpl_hand, lr.lr_child_pid,
	 md_nvsd_completion_handler, (void *)msg);
    bail_error(err);

bail:
    lc_launch_params_free(&lp);
    return err;
}

static int
md_nvsd_completion_handler(lc_child_completion_handle *cmpl_hand,
	pid_t pid, int wait_status, tbool completed,
	void *arg)
{
    int err = 0;
    const char *msg = arg;

    lc_log_debug(LOG_DEBUG, "%s (pid = %d), done = %d", __FUNCTION__, pid, completed);

    if (!WIFEXITED(wait_status)) {
	lc_log_basic(LOG_NOTICE, "process %s (pid %d) exited abnormally", msg, pid);
	goto bail;
    }

    if (WEXITSTATUS(wait_status) == 0) {
	lc_log_basic(LOG_NOTICE, "process %s (pid %d) has sucessfuly completed", msg, pid);
	goto bail;
    } else {
	lc_log_basic(LOG_NOTICE, "process %s (pid %d) has failed (%d)", msg, pid, WEXITSTATUS(wait_status));
    }


bail:
    return err;
}

static int
md_nvsd_diskcache_upgrade_firmware_generic(md_commit *commit,
					   const mdb_db *db,
					   const char *action_name,
					   bn_binding_array *params,
					   void *cb_arg)
{
    int err = 0;
    tstring *output = NULL;
    lc_launch_params *lp = NULL;
    lc_launch_result *lr = NULL;
    tstr_array *lp_argv = NULL;
    const bn_binding *binding = NULL;
    char  *option = NULL, *path = NULL;

    lr = malloc(sizeof(*lr));
    bail_null(lr);
    memset(lr, 0, sizeof(*lr));

    err = lc_launch_params_get_defaults(&lp);
    bail_error_null(err, lp);

    err = bn_binding_array_get_binding_by_name(params, "option", &binding);
    bail_error_null(err, binding);

    err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &option);
    bail_error(err);

    err = bn_binding_array_get_binding_by_name(params, "path", &binding);
    bail_error_null(err, binding);
    err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &path);
    bail_error(err);

    err = ts_new_str(&(lp->lp_path), "/opt/nkn/bin/upgrade-firmware-generic.sh");
    bail_error(err);

    err = tstr_array_new_va_str(&lp_argv, NULL, 3,
	    "/opt/nkn/bin/upgrade-firmware-generic.sh" , option, path);

    lp->lp_wait = false;

    lp->lp_argv = lp_argv;

    lp->lp_env_opts = eo_preserve_all;

    lp->lp_log_level = LOG_INFO;

    lp->lp_io_origins[lc_io_stdout].lio_opts = lioo_capture_nowait;
    lp->lp_io_origins[lc_io_stderr].lio_opts = lioo_capture_nowait;

    err = lc_launch(lp, lr);
    bail_error(err);

    err = md_commit_set_completion_proc_async(commit, true);
    bail_error(err);

    err = md_commit_add_completion_proc_state_action(commit,
            lr->lr_child_pid, lr->lr_exit_status,
            lr->lr_child_pid == -1,
            "md_nvsd_diskcache_upgrade_firmware_generic",
            md_nvsd_diskcache_upgrade_firware_finalize, lr);
    lr = NULL;
    bail_error(err);

bail:
    lc_launch_params_free(&lp);
    if (lr) {
	lc_launch_result_free_contents(lr);
	safe_free(lr);
    }

    safe_free(option);
    safe_free(path);
    return err;
}

static int
md_nvsd_diskcache_upgrade_firware(md_commit *commit,
                                  const mdb_db *db,
                                  const char *action_name,
                                  bn_binding_array *params,
                                  void *cb_arg)
{
    int err = 0;
    tstring *output = NULL;
    lc_launch_params *lp = NULL;
    lc_launch_result *lr = NULL;

    lr = malloc(sizeof(*lr));
    bail_null(lr);
    memset(lr, 0, sizeof(*lr));

    err = lc_launch_params_get_defaults(&lp);
    bail_error_null(err, lp);

    err = ts_new_str(&(lp->lp_path), "/opt/nkn/bin/upgrade_ssd.sh");
    bail_error(err);

    lp->lp_wait = false;

    lp->lp_env_opts = eo_preserve_all;

    lp->lp_log_level = LOG_INFO;

    lp->lp_io_origins[lc_io_stdout].lio_opts = lioo_capture_nowait;
    lp->lp_io_origins[lc_io_stderr].lio_opts = lioo_capture_nowait;

    err = lc_launch(lp, lr);
    bail_error(err);

    err = md_commit_set_completion_proc_async(commit, true);

    err = md_commit_add_completion_proc_state_action(commit,
            lr->lr_child_pid, lr->lr_exit_status,
            lr->lr_child_pid == -1,
            "md_nvsd_diskcache_upgrade_firware",
            md_nvsd_diskcache_upgrade_firware_finalize, lr);
    lr = NULL;
    bail_error(err);

bail:
    lc_launch_params_free(&lp);
    if (lr) {
	lc_launch_result_free_contents(lr);
	safe_free(lr);
    }
    return err;
}


static int
md_nvsd_diskcache_upgrade_firware_finalize(md_commit *commit, const mdb_db *db,
                                  const char *action_name, void *cb_arg,
                                  void *finalize_arg, pid_t pid,
                                  int wait_status, tbool completed)
{
    int err = 0;
    lc_launch_result *lr = finalize_arg;
    const tstring *output = NULL;
    tstring *container = NULL;
    int32 offset1 = 0, offset2 = 0;
    bn_binding *binding = NULL;

    bail_null(commit);
    bail_null(lr);

    if(completed) {
	lr->lr_child_pid = -1;
	lr->lr_exit_status = wait_status;
    }

    err = lc_launch_complete_capture(lr);
    bail_error(err);

    err = lc_check_exit_status_full(lr->lr_exit_status, NULL, LOG_INFO,
	    LOG_WARNING, LOG_ERR, "/opt/nkn/bin/upgrade_ssd.sh");
    bail_error(err);

    output = lr->lr_captured_output;
    bail_null(output);

    err = md_commit_set_return_status_msg
	(commit, 0, ts_str(output));
    bail_error(err);

bail:
    if (lr) {
	lc_launch_result_free_contents(lr);
	safe_free(lr);
    }
    ts_free(&container);
    bn_binding_free(&binding);
    return err;
}

static int
md_nvsd_handle_ps(md_commit *commit,
	const mdb_db *db,
	const char *action_name,
	bn_binding_array *params,
	void *cb_arg)
{
    int err = 0;
    const char *action = cb_arg;
    tstring *output = NULL;
    lc_launch_params *lp = NULL;
    lc_launch_result *lr = NULL;
    tstr_array *lp_argv = NULL;
    const char *path = NULL;
    char  *disk_name = NULL, *disk_id = NULL;
    file_path_t mount_disk = {0}, mount_path = {0};
    const bn_binding *binding = NULL;
    tstring *block_disk_name = NULL;
    tbool mount_cmd = false;
    node_name_t node_name = {0};

    err = bn_binding_array_get_binding_by_name(params, "diskname", &binding);
    bail_error_null(err, binding);
    err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &disk_name);
    bail_error(err);

    err = bn_binding_array_get_binding_by_name(params, "disk_id", &binding);
    bail_error_null(err, binding);
    err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &disk_id);
    bail_error(err);

    err = bn_binding_array_get_binding_by_name(params, "mount", &binding);
    bail_error(err);
    if (binding) {
	err = bn_binding_get_tbool(binding, ba_value, NULL, &mount_cmd);
	bail_error(err);
    }

    /* find  the device name (/dev/sda ) */
    snprintf(node_name, sizeof(node_name),
	    "/nkn/nvsd/diskcache/config/disk_id/%s/block_disk_name", disk_id);

    err = mdb_get_node_value_tstr(commit, db,
	    node_name, 0, NULL, &block_disk_name);
    bail_error_null(err, block_disk_name);

    if (0 == strcmp(action, "format")) {
	path = "/opt/nkn/bin/initcache_ps_xfs.sh";
	err = tstr_array_new_va_str(&lp_argv, NULL, 2,
		path, ts_str(block_disk_name));
	bail_error(err);
    } else if (0 == strcmp(action, "mount")) {
	if (mount_cmd) {
	    path = "/bin/mount";
	} else {
	    path = "/bin/umount";
	}

	snprintf(mount_disk, sizeof(mount_disk), "%s1", ts_str(block_disk_name));
	/* no multiple persistent disks, hard-coding ps_1 */
	snprintf(mount_path, sizeof(mount_path), "/nkn/pers/ps_1");

	if (mount_cmd) {
	    /* create the directory */
	    err = lf_ensure_dir(mount_path, 0755);
	    bail_error(err);

	    /* cmd = mount -t xfs /dev/sdb1 /nkn/pers/dc_2  */
	    err = tstr_array_new_va_str(&lp_argv, NULL, 5,
		    path,"-t", "xfs", mount_disk, mount_path);
	} else {
	    err = tstr_array_new_va_str(&lp_argv, NULL, 2,
		    path,mount_path);
	}
	bail_error(err);
    } else {
	goto bail;
    }

    lr = malloc(sizeof(*lr));
    bail_null(lr);
    memset(lr, 0, sizeof(*lr));

    err = lc_launch_params_get_defaults(&lp);
    bail_error_null(err, lp);

    lp->lp_argv = lp_argv;

    err = ts_new_str(&(lp->lp_path), path);
    bail_error(err);

    lp->lp_wait = false;
    lp->lp_env_opts = eo_preserve_all;
    lp->lp_log_level = LOG_INFO;
    lp->lp_io_origins[lc_io_stdout].lio_opts = lioo_capture_nowait;
    lp->lp_io_origins[lc_io_stderr].lio_opts = lioo_capture_nowait;

    err = lc_launch(lp, lr);
    bail_error(err);

    err = md_commit_set_completion_proc_async(commit, true);

    err = md_commit_add_completion_proc_state_action(commit,
            lr->lr_child_pid, lr->lr_exit_status,
            lr->lr_child_pid == -1,
            "md_nvsd_handle_ps",
            md_nvsd_handle_ps_finalize, lr);
    lr = NULL;
    bail_error(err);

bail:
    if (lp) {
	lc_launch_params_free(&lp);
    } else {
	tstr_array_free(&lp_argv);
    }
    if(lr) {
	lc_launch_result_free_contents(lr);
	safe_free(lr);
    }
    safe_free(disk_name);
    safe_free(disk_id);
    return 0;
}
static int
md_nvsd_handle_ps_finalize(md_commit *commit, const mdb_db *db,
                                  const char *action_name, void *cb_arg,
                                  void *finalize_arg, pid_t pid,
                                  int wait_status, tbool completed)
{
    int err = 0, code = 0;
    lc_launch_result *lr = finalize_arg;
    const tstring *output = NULL;

    bail_null(commit);
    bail_null(lr);

    if(completed) {
	lr->lr_child_pid = -1;
	lr->lr_exit_status = wait_status;
    }

    if (WEXITSTATUS(wait_status) == 0) {
	code = 0;
    } else {
	code = EINVAL;
    }

    err = lc_launch_complete_capture(lr);
    bail_error(err);

    output = lr->lr_captured_output;
    bail_null(output);

    err = md_commit_set_return_status_msg
	(commit, code, ts_str(output));
    bail_error(err);

bail:
    if (lr) {
	lc_launch_result_free_contents(lr);
	safe_free(lr);
    }
    return err;
}

/* NOTE: /nkn/nvsd/diskcache/monitor/disk_id/%s/diskname is external
 * monitoring node, will not be available at start-up
 */
int
md_diskcache_find_diskname(md_commit *commit,const mdb_db *new_db,
	const char *disk_sno, tstring **diskname)
{
    int err = 0;
    node_name_t node_name = {0};
    tstring *t_disk_name = NULL;

    snprintf(node_name, sizeof(node_name),
	    "/nkn/nvsd/diskcache/monitor/disk_id/%s/diskname", disk_sno);

    err = mdb_get_node_value_tstr(commit, new_db,
	    node_name, 0, NULL, &t_disk_name);
    bail_error_null(err, t_disk_name);

    *diskname = t_disk_name;
    t_disk_name = NULL;

bail:
    return err;
}

