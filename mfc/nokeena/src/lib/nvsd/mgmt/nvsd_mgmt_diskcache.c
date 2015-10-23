/*
 *
 * Filename:  nkn_mgmt_diskcache.c
 * Date:      2008/11/23
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

/* Standard Headers */
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <sys/types.h>
#include <dirent.h>
#include  <sys/stat.h>

/* Samara Headers */
#include "md_client.h"
#include "license.h"

#include "nkn_defs.h"
#include "nkn_cfg_params.h"
#include "nvsd_mgmt.h"
#include "nkn_diskmgr2_api.h"
#include "nkn_namespace.h"
#include "nkn_mediamgr_api.h"
#include "nkn_mgmt_defs.h"

/* Macros */
#define	PROBE_NEWDISKS_STR	"*"
#define	SSD_STR		"SSD"
#define	SAS_STR		"SAS"
#define	SATA_STR	"SATA"
#define	TIER_1_STR	"Tier-1"
#define	TIER_2_STR	"Tier-2"
#define	TIER_3_STR	"Tier-3"
#define	UNKNOWN_TIER_STR	"Tier_Unk"

/* Local Function Prototypes */
int nvsd_diskcache_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings);

int nvsd_diskcache_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	bn_binding * binding, void *data);
int nvsd_diskcache_disk_id_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	bn_binding * binding, void *data);
static dm2_mgmt_db_info_t *get_diskcache_element(const char *cpDiskid);
static const dm2_mgmt_disk_status_t *get_diskstatus_from_diskid(const char
	*t_diskid);
char *get_uids_in_disk(void);
static gint compare_disk_entries(gconstpointer elem1, gconstpointer elem2);

/* Extern Variables */
extern md_client_context *nvsd_disk_mcc;

/* Local Variables */
static const char diskcache_config_prefix[] = "/nkn/nvsd/diskcache/config";
static const char diskcache_config_disk_id_prefix[] =
"/nkn/nvsd/diskcache/config/disk_id";
static tbool b_new_disk = false;
static pthread_mutex_t diskcache_node_data_lock = PTHREAD_MUTEX_INITIALIZER;

GList *lstDiskCache = NULL;

tbool glob_cmd_rate_limit = false;

/* ------------------------------------------------------------------------- */

/*
 *	function : nvsd_mgmt_diskcache_lock()
 *	purpose : set a pthread_mutex to block till a unlock call is made
 */
void
nvsd_mgmt_diskcache_lock(void)
{
    pthread_mutex_lock(&diskcache_node_data_lock);
}	/* end of nvsd_mgmt_diskcache_lock() */

/* ------------------------------------------------------------------------- */

/*
 *	function : nvsd_mgmt_diskcache_unlock()
 *	purpose : unlock the pthread_mutex
 */
void
nvsd_mgmt_diskcache_unlock(void)
{
    pthread_mutex_unlock(&diskcache_node_data_lock);
}	/* end of nvsd_mgmt_diskcache_unlock() */

/* ------------------------------------------------------------------------- */

/*
 *	function : nvsd_diskcache_cfg_query()
 *	purpose : query for diskcache config parameters
 */
int
nvsd_diskcache_cfg_query(void
	)
{
    int err = 0;
    bn_binding_array *bindings = NULL;
    tbool rechecked_licenses = false;

    lc_log_basic(LOG_DEBUG,
	    "nvsd diskcache module mgmtd query initializing ..");
    err = mdc_get_binding_children(nvsd_disk_mcc, NULL,
	    NULL, true, &bindings, false, true, diskcache_config_prefix);
    bail_error_null(err, bindings);

    err = bn_binding_array_foreach(bindings,
	    nvsd_diskcache_cfg_handle_change,
	    &rechecked_licenses);
    bail_error(err);
    bn_binding_array_free(&bindings);

    /*
     * Bind to get DISK_ID
     */
    err = mdc_get_binding_children(nvsd_disk_mcc,
	    NULL,
	    NULL,
	    true,
	    &bindings,
	    false,
	    true, diskcache_config_disk_id_prefix);
    bail_error_null(err, bindings);

    err = bn_binding_array_foreach(bindings,
	    nvsd_diskcache_disk_id_cfg_handle_change,
	    &rechecked_licenses);
    bail_error(err);
    bn_binding_array_free(&bindings);

    /*
     * Sort the disk list 
     */
    if (lstDiskCache)
	lstDiskCache = g_list_sort(lstDiskCache, compare_disk_entries);

bail:
    bn_binding_array_free(&bindings);

    return err;
}	/* end of nvsd_diskcache_cfg_query() */

/* ------------------------------------------------------------------------- */

/*
 *	function : nvsd_diskcache_module_cfg_handle_change()
 *	purpose : handler for config changes for diskcache module
 */
int
nvsd_diskcache_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    UNUSED_ARGUMENT(old_bindings);

    /*
     * Lock first 
     */
    nvsd_mgmt_diskcache_lock();

    /*
     * Call the callbacks 
     */
    err = bn_binding_array_foreach(new_bindings,
	    nvsd_diskcache_cfg_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    err = bn_binding_array_foreach(new_bindings,
	    nvsd_diskcache_disk_id_cfg_handle_change,
	    &rechecked_licenses);
    bail_error(err);

bail:
    /*
     * Unlock when leaving 
     */
    nvsd_mgmt_diskcache_unlock();
    return err;

}	/* end of nvsd_diskcache_module_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	function : nvsd_diskcache_cfg_handle_change()
 *	purpose : handler for config changes for diskcache module
 */
int
nvsd_diskcache_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx, bn_binding * binding, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    tbool *rechecked_licenses_p = data;
    tstr_array *name_parts = NULL;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    if (bn_binding_name_pattern_match
	    (ts_str(name), "/nkn/nvsd/diskcache/config/**")) {
	bn_binding_get_name_parts(binding, &name_parts);
	bail_error(err);
    } else {
	/*
	 * This is not the server_map node hence leave 
	 */
	goto bail;
    }

    /*
     * Get the value of the EVICT THREAD FREQUENCY 
     */
    if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 1, "evict_thread_freq")) {

	uint32 t_evict_thread_freq = 0;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL, &t_evict_thread_freq);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../diskcache/config/evict_thread_freq as : \"%d\"",
		t_evict_thread_freq);

	/*
	 * Set the global 
	 */
	mm_evict_thread_time_sec = t_evict_thread_freq;
    }

    /*
     * Get the value of FREE-BLOCK THRESHOLD INTERNAL SAS 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "free-block", "threshold", "internal", "sas")) {
	int8_t t_sas_free_blk_threshold_pct = 0;

	err = bn_binding_get_int8(binding,
		ba_value, NULL,
		&t_sas_free_blk_threshold_pct);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../diskcache/config/free-block/threshold/internal/sas : \"%d\"",
		t_sas_free_blk_threshold_pct);
    }

    /*
     * Get the value of FREE-BLOCK THRESHOLD INTERNAL SATA 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "free-block", "threshold", "internal",
	     "sata")) {
	int8_t t_sata_free_blk_threshold_pct = 0;

	err = bn_binding_get_int8(binding,
		ba_value, NULL,
		&t_sata_free_blk_threshold_pct);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../diskcache/config/free-block/threshold/internal/sata : \"%d\"",
		t_sata_free_blk_threshold_pct);
    }

    /*
     * Get the value of FREE-BLOCK THRESHOLD INTERNAL SSD 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "free-block", "threshold", "internal", "ssd")) {
	int8_t t_ssd_free_blk_threshold_pct = 0;

	err = bn_binding_get_int8(binding,
		ba_value, NULL,
		&t_ssd_free_blk_threshold_pct);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../diskcache/config/free-block/threshold/internal/ssd : \"%d\"",
		t_ssd_free_blk_threshold_pct);
    }

    /*
     * Get the value of block-sharing THRESHOLD INTERNAL SAS 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "block-sharing", "threshold", "internal",
	     "sas")) {
	int8_t t_sas_blk_sharing_threshold_pct = 0;

	err = bn_binding_get_int8(binding,
		ba_value, NULL,
		&t_sas_blk_sharing_threshold_pct);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../diskcache/config/block-sharing/threshold/internal/sas : \"%d\"",
		t_sas_blk_sharing_threshold_pct);

	/*
	 * Set the global 
	 */
	err = DM2_mgmt_set_block_share_threshold("sas", (int32_t)
		t_sas_blk_sharing_threshold_pct);
	if (-ENOENT == err) {
	    lc_log_basic(LOG_NOTICE, "No cache tier found - SAS");
	} else if (-EINVAL == err) {
	    lc_log_basic(LOG_NOTICE, "Invalid value : %d",
		    t_sas_blk_sharing_threshold_pct);
	}
	err = 0;
    }

    /*
     * Get the value of block-sharing THRESHOLD INTERNAL SATA 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "block-sharing", "threshold", "internal",
	     "sata")) {
	int8_t t_sata_blk_sharing_threshold_pct = 0;

	err = bn_binding_get_int8(binding,
		ba_value, NULL,
		&t_sata_blk_sharing_threshold_pct);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../diskcache/config/block-sharing/threshold/internal/sata : \"%d\"",
		t_sata_blk_sharing_threshold_pct);

	/*
	 * Set the global 
	 */
	err = DM2_mgmt_set_block_share_threshold("sata", (int32_t)
		t_sata_blk_sharing_threshold_pct);
	if (-ENOENT == err) {
	    lc_log_basic(LOG_NOTICE, "No cache tier found - SATA");
	} else if (-EINVAL == err) {
	    lc_log_basic(LOG_NOTICE, "Invalid value : %d",
		    t_sata_blk_sharing_threshold_pct);
	}
	err = 0;
    }

    /*
     * Get the value of block-sharing THRESHOLD INTERNAL SSD 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "block-sharing", "threshold", "internal",
	     "ssd")) {
	int8_t t_ssd_blk_sharing_threshold_pct = 0;

	err = bn_binding_get_int8(binding,
		ba_value, NULL,
		&t_ssd_blk_sharing_threshold_pct);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../diskcache/config/block-sharing/threshold/internal/ssd : \"%d\"",
		t_ssd_blk_sharing_threshold_pct);

	/*
	 * Set the global 
	 */
	err = DM2_mgmt_set_block_share_threshold("ssd", (int32_t)
		t_ssd_blk_sharing_threshold_pct);
	if (-ENOENT == err) {
	    lc_log_basic(LOG_NOTICE, "No cache tier found - SSD");
	} else if (-EINVAL == err) {
	    lc_log_basic(LOG_NOTICE, "Invalid value : %d",
		    t_ssd_blk_sharing_threshold_pct);
	}
	err = 0;
    }
    // Get the value of media-cache group-read for sas
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "sas", "group-read", "enable")) {
	tbool t_group_read_sas_enable = false;

	err = bn_binding_get_tbool(binding,
		ba_value, NULL, &t_group_read_sas_enable);
	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../diskcache/config/sas/group-read/enable : \"%d\"",
		t_group_read_sas_enable);
    }
    // Get the value of media-cache group-read for sata
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "sata", "group-read", "enable")) {
	tbool t_group_read_sata_enable = false;

	err = bn_binding_get_tbool(binding,
		ba_value, NULL, &t_group_read_sata_enable);
	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../diskcache/config/sata/group-read/enable : \"%d\"",
		t_group_read_sata_enable);
    }
    // Get the value of media-cache group-read for ssd
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "ssd", "group-read", "enable")) {
	tbool t_group_read_ssd_enable = false;

	err = bn_binding_get_tbool(binding,
		ba_value, NULL, &t_group_read_ssd_enable);
	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../diskcache/config/ssd/group-read/enable : \"%d\"",
		t_group_read_ssd_enable);
    }
    // Get the value for cli_cmd rate limiting and update in the global
    // variable.
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 2, "cmd", "rate-limit")) {
	err =
	    bn_binding_get_tbool(binding, ba_value, NULL, &glob_cmd_rate_limit);
	bail_error(err);
    }
    /*
     * Get the value of the admission threshold for SATA 
     */
    else if (bn_binding_name_pattern_match(ts_str(name),
		"/nkn/nvsd/diskcache/config/sata_adm_thres")) {
	uint32 t_sata_adm_thres = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_sata_adm_thres);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../diskcache/config/sata_adm_thres as : \"%d\"",
		t_sata_adm_thres);

	/*
	 * Set the global 
	 */
	nkn_sac_per_thread_limit[SATADiskMgr_provider] = t_sata_adm_thres;
    }
    /*
     * Get the value of the admission threshold for SATA 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 1, "sas_adm_thres")) {
	uint32 t_sas_adm_thres = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_sas_adm_thres);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../diskcache/config/sas_adm_thres as : \"%d\"",
		t_sas_adm_thres);

	/*
	 * Set the global 
	 */
	nkn_sac_per_thread_limit[SAS2DiskMgr_provider] = t_sas_adm_thres;
    }
    /*
     * Get the value of the admission threshold for SATA 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 1, "ssd_adm_thres")) {
	uint32 t_ssd_adm_thres = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_ssd_adm_thres);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../diskcache/config/ssd_adm_thres as : \"%d\"",
		t_ssd_adm_thres);

	/*
	 * Set the global 
	 */
	nkn_sac_per_thread_limit[SolidStateMgr_provider] = t_ssd_adm_thres;
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "watermark", "internal", "ssd", "high")) {
	uint16 t_watermark_ssd_high = 0;

	err = bn_binding_get_uint16(binding,
		ba_value, NULL, &t_watermark_ssd_high);
	bail_error(err);

	err = DM2_mgmt_set_internal_water_mark(SSD_STR,
		t_watermark_ssd_high, -1);
	bail_error(err);
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "watermark", "internal", "ssd", "low")) {
	uint16 t_watermark_ssd_low = 0;

	err = bn_binding_get_uint16(binding,
		ba_value, NULL, &t_watermark_ssd_low);
	bail_error(err);

	err = DM2_mgmt_set_internal_water_mark(SSD_STR, -1,
		t_watermark_ssd_low);
	bail_error(err);
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "watermark", "internal", "sata", "high")) {
	uint16 t_watermark_sata_high = 0;

	err = bn_binding_get_uint16(binding,
		ba_value, NULL, &t_watermark_sata_high);
	bail_error(err);

	err = DM2_mgmt_set_internal_water_mark(SATA_STR,
		t_watermark_sata_high, -1);
	bail_error(err);
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "watermark", "internal", "sata", "low")) {
	uint16 t_watermark_sata_low = 0;

	err = bn_binding_get_uint16(binding,
		ba_value, NULL, &t_watermark_sata_low);
	bail_error(err);

	err = DM2_mgmt_set_internal_water_mark(SATA_STR, -1,
		t_watermark_sata_low);
	bail_error(err);
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "watermark", "internal", "sas", "high")) {
	uint16 t_watermark_sas_high = 0;

	err = bn_binding_get_uint16(binding,
		ba_value, NULL, &t_watermark_sas_high);
	bail_error(err);

	err = DM2_mgmt_set_internal_water_mark(SAS_STR,
		t_watermark_sas_high, -1);
	bail_error(err);
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "watermark", "internal", "sas", "low")) {
	uint16 t_watermark_sas_low = 0;

	err = bn_binding_get_uint16(binding,
		ba_value, NULL, &t_watermark_sas_low);
	bail_error(err);

	err = DM2_mgmt_set_internal_water_mark(SAS_STR, -1,
		t_watermark_sas_low);
	bail_error(err);
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "watermark", "internal", "dict", "high")) {

	uint16 t_watermark_dict_high = 0;

	err = bn_binding_get_uint16(binding,
		ba_value, NULL, &t_watermark_dict_high);
	bail_error(err);

	err = DM2_mgmt_set_dict_evict_water_mark(t_watermark_dict_high, -1);
	bail_error(err);
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "watermark", "internal", "dict", "low")) {

	uint16 t_watermark_dict_low = 0;

	err = bn_binding_get_uint16(binding,
		ba_value, NULL, &t_watermark_dict_low);
	bail_error(err);

	err = DM2_mgmt_set_dict_evict_water_mark(-1, t_watermark_dict_low);
	bail_error(err);
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "internal", "sas", "percent-disks")) {
	uint16 t_prcnt = 0;

	err = bn_binding_get_uint16(binding, ba_value, NULL, &t_prcnt);
	bail_error(err);

	err = DM2_mgmt_set_internal_percentage(SAS_STR, t_prcnt);
	bail_error(err);
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "internal", "sata", "percent-disks")) {
	uint16 t_prcnt = 0;

	err = bn_binding_get_uint16(binding, ba_value, NULL, &t_prcnt);
	bail_error(err);

	err = DM2_mgmt_set_internal_percentage(SATA_STR, t_prcnt);
	bail_error(err);
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "internal", "ssd", "percent-disks")) {
	uint16 t_prcnt = 0;

	err = bn_binding_get_uint16(binding, ba_value, NULL, &t_prcnt);
	bail_error(err);

	err = DM2_mgmt_set_internal_percentage(SSD_STR, t_prcnt);
	bail_error(err);
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "internal", "watermark", "sata",
	     "evict_time")) {
	lt_time_sec t_evict_time;
	struct tm evict_tm;

	err = bn_binding_get_time_sec(binding, ba_value, NULL, &t_evict_time);
	bail_error(err);

	/*
	 * The time value is the seconds elapsed from midnight and is
	 * actually the localtime. Using the '_utc' ensures that no timezone
	 * conversion is done and we get the duration from midnight (which is 
	 * the localtime ;) ) 
	 */
	lt_time_sec_to_tm_utc(t_evict_time, &evict_tm);
	err = DM2_mgmt_set_time_evict_config(SATA_STR, -1,
		evict_tm.tm_hour, evict_tm.tm_min);
	bail_error(err);
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "internal", "watermark", "sas", "evict_time")) {
	lt_time_sec t_evict_time;
	struct tm evict_tm;

	err = bn_binding_get_time_sec(binding, ba_value, NULL, &t_evict_time);
	bail_error(err);

	/*
	 * The time value is the seconds elapsed from midnight and is
	 * actually the localtime. Using the '_utc' ensures that no timezone
	 * conversion is done and we get the duration from midnight (which is 
	 * the localtime ;) ) 
	 */
	lt_time_sec_to_tm_utc(t_evict_time, &evict_tm);
	err = DM2_mgmt_set_time_evict_config(SAS_STR, -1,
		evict_tm.tm_hour, evict_tm.tm_min);
	bail_error(err);
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "internal", "watermark", "ssd", "evict_time")) {
	lt_time_sec t_evict_time;
	struct tm evict_tm;

	err = bn_binding_get_time_sec(binding, ba_value, NULL, &t_evict_time);
	bail_error(err);

	/*
	 * The time value is the seconds elapsed from midnight and is
	 * actually the localtime. Using the '_utc' ensures that no timezone
	 * conversion is done and we get the duration from midnight (which is 
	 * the localtime ;) ) 
	 */
	lt_time_sec_to_tm_utc(t_evict_time, &evict_tm);
	err = DM2_mgmt_set_time_evict_config(SSD_STR, -1,
		evict_tm.tm_hour, evict_tm.tm_min);
	bail_error(err);
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "internal", "watermark", "sata",
	     "time_low_pct")) {
	uint16_t t_time_sata_lwm;

	err = bn_binding_get_uint16(binding, ba_value, NULL, &t_time_sata_lwm);
	bail_error(err);

	err = DM2_mgmt_set_time_evict_config(SATA_STR, t_time_sata_lwm, -1, -1);
	bail_error(err);
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "internal", "watermark", "sas",
	     "time_low_pct")) {
	uint16_t t_time_sas_lwm;

	err = bn_binding_get_uint16(binding, ba_value, NULL, &t_time_sas_lwm);
	bail_error(err);

	err = DM2_mgmt_set_time_evict_config(SAS_STR, t_time_sas_lwm, -1, -1);
	bail_error(err);
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "internal", "watermark", "ssd",
	     "time_low_pct")) {
	uint16_t t_time_ssd_lwm;

	err = bn_binding_get_uint16(binding, ba_value, NULL, &t_time_ssd_lwm);
	bail_error(err);

	err = DM2_mgmt_set_time_evict_config(SSD_STR, t_time_ssd_lwm, -1, -1);
	bail_error(err);
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 1, "write-size")) {
	uint32_t t_size;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_size);
	bail_error(err);

	err = DM2_mgmt_set_write_size_config(t_size);
	bail_error(err);

    }

bail:
    tstr_array_free(&name_parts);
    return err;

}	/* end of nvsd_diskcache_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	function : nvsd_diskcache_disk_id_cfg_handle_change()
 *	purpose : handler for config changes for diskcache module
 */
int
nvsd_diskcache_disk_id_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	bn_binding * binding, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    tstr_array *name_parts = NULL;
    const char *t_disk_id = NULL;
    uint32 ret_idx = 0;
    tbool *rechecked_licenses_p = data;
    dm2_mgmt_db_info_t *pstDiskCache = NULL;

    (void) ret_idx;
    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /*
     * Check if this is our node 
     */
    if (bn_binding_name_pattern_match(ts_str(name),
		"/nkn/nvsd/diskcache/config/disk_id/**")) {

	/*-------- Get the DISK_ID ------------*/
	bn_binding_get_name_parts(binding, &name_parts);
	bail_error(err);
	t_disk_id = tstr_array_get_str_quick(name_parts, 5);

	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../diskcache/config/disk_id as : \"%s\"",
		t_disk_id);

	pstDiskCache = get_diskcache_element(t_disk_id);
	if (NULL == pstDiskCache)
	    goto bail;
    } else {
	/*
	 * not relevant to us, exit from function 
	 */
	goto bail;
    }

    /*
     * Get the value of the BAY NUMBER 
     */
    if (bn_binding_name_parts_pattern_match_va(name_parts, 5, 2, "*", "bay")) {
	uint16 t_disk_id_bay;

	err = bn_binding_get_uint16(binding, ba_value, NULL, &t_disk_id_bay);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../diskcache/config/disk_id/%s/bay as : \"%d\"",
		t_disk_id, t_disk_id_bay);
    }

    /*
     * Get the value of the ACTIVATED flag 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 5, 2, "*", "activated")) {
	tbool t_disk_id_activated;

	err = bn_binding_get_tbool(binding,
		ba_value, NULL, &t_disk_id_activated);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../diskcache/config/disk_id/%s/activated as : \"%s\"",
		t_disk_id, t_disk_id_activated ? "TRUE" : "FALSE");
	if (pstDiskCache)
	    pstDiskCache->mi_activated = t_disk_id_activated;
    }

    /*
     * Get the value of the CACHE_ENABLED flag 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 5, 2, "*", "cache_enabled")) {
	tbool t_disk_id_cache_enabled;

	err = bn_binding_get_tbool(binding,
		ba_value, NULL, &t_disk_id_cache_enabled);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../diskcache/config/disk_id/%s/cache_enabled as : \"%s\"",
		t_disk_id, t_disk_id_cache_enabled ? "TRUE" : "FALSE");
	if (pstDiskCache)
	    pstDiskCache->mi_cache_enabled = t_disk_id_cache_enabled;
    }

    /*
     * Get the value of the AUTO_REFORMAT flag 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 5, 2, "*", "auto_reformat")) {
	tbool t_disk_id_auto_reformat;

	err = bn_binding_get_tbool(binding,
		ba_value, NULL, &t_disk_id_auto_reformat);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../diskcache/config/disk_id/%s/auto_reformat as : \"%s\"",
		t_disk_id, t_disk_id_auto_reformat ? "TRUE" : "FALSE");
	if (pstDiskCache)
	    pstDiskCache->mi_auto_reformat = t_disk_id_auto_reformat;
    }

    /*
     * Get the value of the COMMENTED OUT flag 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 5, 2, "*", "commented_out")) {
	tbool t_commented_out;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_commented_out);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../diskcache/config/disk_id/%s/commented_out as : \"%s\"",
		t_disk_id, t_commented_out ? "TRUE" : "FALSE");
	if (pstDiskCache)
	    pstDiskCache->mi_commented_out = t_commented_out;
    }
    /*
     * Get the value of the FS PARTITION type 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 5, 2, "*", "fs_partition")) {
	char *t_fs_partition = NULL;
	int i;

	(void) i;

	err = bn_binding_get_str(binding,
		ba_value, bt_string, NULL, &t_fs_partition);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../diskcache/config/disk_id/%s/fs_partition as : \"%s\"",
		t_disk_id, t_fs_partition);
	if (t_fs_partition && pstDiskCache) {
	    strncpy(pstDiskCache->mi_fs_partition, t_fs_partition,
		    (DM2_MAX_FSDEV - 1));
	    safe_free(t_fs_partition);
	}

    }

    /*
     * Get the value of the MISSING AFTER BOOT flag 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 5, 2, "*", "missing_after_boot")) {
	tbool t_missing_after_boot;

	err = bn_binding_get_tbool(binding,
		ba_value, NULL, &t_missing_after_boot);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../diskcache/config/disk_id/%s/missing_after_boot as : \"%s\"",
		t_disk_id, t_missing_after_boot ? "TRUE" : "FALSE");
	if (pstDiskCache) {
	    pstDiskCache->mi_missing_after_boot = t_missing_after_boot;

	    if (b_new_disk && (!t_missing_after_boot))
		err = DM2_mgmt_TM_DB_update_msg(PROBE_NEWDISKS_STR,
			DM2_MGMT_MOD_ACTIVATE_FINISH,
			(void *) pstDiskCache, NULL);
	}

    }

    /*
     * Get the value of the RAW PARTITION type 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 5, 2, "*", "raw_partition")) {
	char *t_raw_partition = NULL;
	int i;

	(void) i;

	err = bn_binding_get_str(binding,
		ba_value, bt_string, NULL, &t_raw_partition);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../diskcache/config/disk_id/%s/raw_partition as : \"%s\"",
		t_disk_id, t_raw_partition);
	if (t_raw_partition && pstDiskCache) {
	    strncpy(pstDiskCache->mi_raw_partition,
		    t_raw_partition, (DM2_MAX_RAWDEV - 1));
	    safe_free(t_raw_partition);
	}
    }

    /*
     * Get the value of the SET SECTOR count 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 5, 2, "*", "set_sector_count")) {
	uint64 t_set_sector_count;

	err = bn_binding_get_uint64(binding,
		ba_value, NULL, &t_set_sector_count);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../diskcache/config/disk_id/%s/set_sector_count as : \"%ld\"",
		t_disk_id, t_set_sector_count);
	if (pstDiskCache)
	    pstDiskCache->mi_set_sector_cnt = t_set_sector_count;
    }

    /*
     * Get the value of the SUB_PROVIDER type 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 5, 2, "*", "sub_provider")) {
	char *t_sub_provider = NULL;
	int i;

	(void) i;

	err = bn_binding_get_str(binding,
		ba_value, bt_string, NULL, &t_sub_provider);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../diskcache/config/disk_id/%s/sub_provider as : \"%s\"",
		t_disk_id, t_sub_provider);
	if (t_sub_provider && pstDiskCache) {
	    strncpy(pstDiskCache->mi_provider, t_sub_provider,
		    (DM2_MAX_TIERNAME - 1));
	    safe_free(t_sub_provider);
	}
    }

    /*
     * Get the value of the DISK_NAME type 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 5, 2, "*", "block_disk_name")) {
	char *t_block_disk_name = NULL;
	int i;

	(void) i;

	err = bn_binding_get_str(binding,
		ba_value, bt_string, NULL, &t_block_disk_name);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../diskcache/config/disk_id/%s/block_disk_name as : \"%s\"",
		t_disk_id, t_block_disk_name);
	if (t_block_disk_name && pstDiskCache) {
	    strncpy(pstDiskCache->mi_diskname, t_block_disk_name,
		    (DM2_MAX_DISKNAME - 1));
	    safe_free(t_block_disk_name);
	}
    }

    /*
     * Get the value of the STAT_NAME type 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 5, 2, "*", "stat_size_name")) {
	char *t_stat_size_name = NULL;
	int i;

	(void) i;

	err = bn_binding_get_str(binding,
		ba_value, bt_string, NULL, &t_stat_size_name);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../diskcache/config/disk_id/%s/stat_size_name as : \"%s\"",
		t_disk_id, t_stat_size_name);
	if (t_stat_size_name && pstDiskCache) {
	    strncpy(pstDiskCache->mi_statname, t_stat_size_name,
		    (DM2_MAX_STATNAME - 1));
	    safe_free(t_stat_size_name);
	}
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 5, 2, "*", "owner")) {
	char *owner = NULL;

	err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &owner);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../diskcache/config/disk_id/%s/owner as : \"%s\"",
		t_disk_id, owner);
	if (owner) {
	    strlcpy(pstDiskCache->mi_owner, owner,
		    sizeof (pstDiskCache->mi_owner));
	    safe_free(owner);
	}
    }

bail:
    tstr_array_free(&name_parts);
    return err;

}	/* end of * nvsd_diskcache_disk_id_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	function : get_diskcache_element ()
 *	purpose : get the element in the list for the given diskid
 *		if not found then create a new entry and return
 */
static dm2_mgmt_db_info_t *
get_diskcache_element(const char *cpDiskid)
{
    dm2_mgmt_db_info_t *pstElement;
    GList *pList;

    /*
     * Sanity test 
     */
    if (NULL == cpDiskid)
	return (NULL);

    /*
     * Start with first element and search for the match 
     */
    pList = g_list_first(lstDiskCache);

    while (NULL != pList) {
	pstElement = (dm2_mgmt_db_info_t *) pList->data;
	if (NULL == pstElement) {
	    /*
	     * Should not happen 
	     */
	    return (NULL);
	}

	/*
	 * See if it matches 
	 */
	if (0 == strcmp(pstElement->mi_serial_num, cpDiskid))
	    return (pstElement);

	/*
	 * Get the next element 
	 */
	pList = g_list_next(pList);
    }

    /*
     * We did not find a match hence create a new entry 
     */
    pstElement = (dm2_mgmt_db_info_t *) nkn_calloc_type(1,
	    sizeof (dm2_mgmt_db_info_t),
	    mod_mgmt_dm2_mgmt_db_info_t);
    if (NULL == pstElement)
	return (NULL);			// Memory allocation problem

    strncpy(pstElement->mi_serial_num, cpDiskid, (DM2_MAX_SERIAL_NUM - 1));

    /*
     * Now add this entry to the GList 
     */
    lstDiskCache = g_list_append(lstDiskCache, (gpointer) pstElement);

    /*
     * Return the new element 
     */
    return (pstElement);
}	/* end of get_diskcache_element() */

/* ------------------------------------------------------------------------- */

/*
 *	function : get_diskid_from_diskname()
 *	purpose : get the disk-id for the given disk name
 */
char *
get_diskid_from_diskname(const char *t_diskname)
{
    int err = 0;
    dm2_mgmt_db_info_t *pstElement;
    GList *pList;
    dm2_mgmt_disk_status_t stDiskStatus;

    /*
     * Sanity test 
     */
    if (NULL == t_diskname)
	return (NULL);

    /*
     * Start with first element and search for the match 
     */
    pList = g_list_first(lstDiskCache);

    while (NULL != pList) {
	pstElement = (dm2_mgmt_db_info_t *) pList->data;
	if (NULL == pstElement) {
	    /*
	     * Should not happen 
	     */
	    return (NULL);
	}

	/*
	 * Get the status struct from DM module 
	 */
	memset(&stDiskStatus, 0, sizeof (dm2_mgmt_disk_status_t));
	err = DM2_mgmt_get_diskcache_info(pstElement, &stDiskStatus);
	if (err != 0) {
	    lc_log_debug(LOG_NOTICE, "DM2: get_diskcache_info failed : %d",
		    err);
	}
	err = 0;

	/*
	 * Check if the names match 
	 */
	if ((DM2_MGMT_STATE_NOT_PRESENT != stDiskStatus.ms_state)
		&& (0 == strcmp(stDiskStatus.ms_name, t_diskname)))
	    return (pstElement->mi_serial_num);	/* Return the disk id */

	/*
	 * Get the next element 
	 */
	pList = g_list_next(pList);
    }

    /*
     * We did not find a match hence NULL 
     */
    return (NULL);
}	/* end of get_diskid_from_diskname() */

/* ------------------------------------------------------------------------- */

/*
 *	function : nvsd_mgmt_get_disklist_locked()
 *	purpose : the api to the diskmanager module to pass the glib list
 *		of disk information read from the config database
 */
GList *
nvsd_mgmt_get_disklist_locked(void)
{
    if (lstDiskCache) {
	/*
	 * As per discussion with Michael locking before returning 
	 */
	nvsd_mgmt_diskcache_lock();

	/*
	 * Now return the list 
	 */
	return (lstDiskCache);
    } else
	return (NULL);

}	/* end of nvsd_mgmt_get_disklist_locked() */

/* ------------------------------------------------------------------------- */

/*
 *	function : nvsd_mgmt_show_watermark_action_hdlr()
 *	purpose : the handler to get the configured watermarks
 */
char *
nvsd_mgmt_show_watermark_action_hdlr()
{
    char *ret_resp;
    int32_t ssd_hi, ssd_lo;
    int32_t sas_hi, sas_lo;
    int32_t sata_hi, sata_lo;

    /*
     * Initialize 
     */
    ssd_hi = ssd_lo = sas_hi = sas_lo = sata_hi = sata_lo = 0;

    /*
     * Get the SSD watermarks 
     */
    DM2_mgmt_get_internal_water_mark(SSD_STR, &ssd_hi, &ssd_lo);
    DM2_mgmt_get_internal_water_mark(SAS_STR, &sas_hi, &sas_lo);
    DM2_mgmt_get_internal_water_mark(SATA_STR, &sata_hi, &sata_lo);
    ret_resp =
	smprintf("Tier\tHi\tLo\nSSD\t%2d\t%2d\nSAS\t%2d\t%2d\nSATA\t%2d\t%2d\n",
		ssd_hi, ssd_lo, sas_hi, sas_lo, sata_hi, sata_lo);

    return (ret_resp);
}	/* end of nvsd_mgmt_show_watermark_action_hdlr() */

/* ------------------------------------------------------------------------- */

/*
 *	function : nvsd_mgmt_diskcache_action_hdlr()
 *	purpose : the api to the diskmanager module to handle the various
 *		actions from northbound interfaces
 */
int
nvsd_mgmt_diskcache_action_hdlr(const char *t_diskname,
	nvsd_mgmt_actions_t en_action,
	tbool t_value, char *ret_msg)
{
    const char *t_diskid = NULL;
    char *node_name = NULL;
    int err = 0;
    dm2_mgmt_db_info_t *pstElement;

    /*
     * Now get the disk_id for the disk name 
     */
    if (en_action != NVSD_DC_PROBE_NEWDISK) {
	/*
	 * Sanity test 
	 */
	if (NULL == t_diskname)
	    return (-1);

	/*
	 * Lock first 
	 */
	nvsd_mgmt_diskcache_lock();

	t_diskid = get_diskid_from_diskname(t_diskname);

	/*
	 * Unlock when leaving 
	 */
	nvsd_mgmt_diskcache_unlock();

	/*
	 * Check validity of diskid 
	 */
	if (NULL == t_diskid) {
	    strcpy(ret_msg, "error: unknown disk");
	    return (-1);
	}
    }

    /*
     * Check the action type 
     */
    switch (en_action) {
	case NVSD_DC_PROBE_NEWDISK:
	    /*
	     * Call the DM module with the action 
	     */
	    err = DM2_mgmt_TM_DB_update_msg(PROBE_NEWDISKS_STR,
		    DM2_MGMT_MOD_ACTIVATE_START,
		    (void *) &t_value, NULL);
	    b_new_disk = true;
	    break;
	case NVSD_DC_ACTIVATE:
	    /*
	     * Call the DM module with the action 
	     */
	    err = DM2_mgmt_TM_DB_update_msg(t_diskname,
		    DM2_MGMT_MOD_ACTIVATE_START,
		    (void *) &t_value, NULL);

	    /*
	     * Update the node 
	     */
	    if (0 == err)
		strcpy(ret_msg, t_value ? "Disk Activated" : "Disk Deactivated");
	    else if (-NKN_DM2_MUST_CACHE_DISABLE == err)
		strcpy(ret_msg, "Must issue 'cache disable' command first");
	    else if (-NKN_DM2_MUST_CLEAR_ERROR == err)
		strcpy(ret_msg,
			"Must first clear error state (please refer to admin guide)");
	    else
		strcpy(ret_msg,
			t_value ? "Disk Activation Failed !!!" :
			"Disk Deactivation Failed !!!");
	    break;
	case NVSD_DC_CACHEABLE:
	    err = DM2_mgmt_TM_DB_update_msg(t_diskname,
		    DM2_MGMT_MOD_CACHE_ENABLED,
		    (void *) &t_value, NULL);

	    /*
	     * Update the node 
	     */
	    if (0 == err)
		strcpy(ret_msg,
			t_value ? "Disk Cache Enabled" : "Disk Cache Disabled");
	    /*
	     * Expected to happen only when enabling cache 
	     */
	    else if (-NKN_DM2_MUST_FORMAT == err)
		strcpy(ret_msg, "Disk Cache Enable Failed - Disk Cache"
			" must be formatted before enabling !!!");
	    else if (-NKN_DM2_MUST_STATUS_ACTIVE == err)
		strcpy(ret_msg, "Must issue 'status active' command first");
	    else if (-NKN_DM2_MUST_CLEAR_ERROR == err)
		strcpy(ret_msg,
			"Must first clear error state (please refer to admin guide)");
	    else
		strcpy(ret_msg,
			t_value ? "Cache Enable Failed !!!" :
			"Cache Disable Failed !!!");
	    break;
	case NVSD_DC_FORMAT:
	    /*
	     * Get the disk information 
	     */
	    pstElement = get_diskcache_element(t_diskname);
	    if (NULL == pstElement) {
		goto bail;
	    }

	    pstElement->mi_small_block_format = 0;

	    /*
	     * Call the DM2 module for the format string 
	     */
	    err = DM2_mgmt_TM_DB_update_msg(t_diskname,
		    DM2_MGMT_MOD_PRE_FORMAT,
		    (void *) pstElement, ret_msg);

	    /*
	     * Check the error code 
	     */
	    if (-NKN_DM2_MUST_CLEAR_ERROR == err)
		strcpy(ret_msg,
			"Must first clear error state (please refer to admin guide)");
	    else if (err) {
		/*
		 * Since we are not using the allocated memory because of
		 * error, free it 
		 */
		strcpy(ret_msg,
			"Unable to format, please check disk status and ensure cache is disabled");
	    }
	    break;

	case NVSD_DC_FORMAT_BLOCKS:

	    /*
	     * Get the disk information 
	     */
	    pstElement = get_diskcache_element(t_diskname);
	    if (NULL == pstElement) {
		goto bail;
	    }

	    pstElement->mi_small_block_format = 1;
	    /*
	     * Call the DM2 module for the format string 
	     */
	    err = DM2_mgmt_TM_DB_update_msg(t_diskname,
		    DM2_MGMT_MOD_PRE_FORMAT,
		    (void *) pstElement, ret_msg);

	    /*
	     * Check the error code 
	     */
	    if (-NKN_DM2_MUST_CLEAR_ERROR == err)
		strcpy(ret_msg,
			"Must first clear error state (please refer to admin guide)");
	    else if (err) {
		/*
		 * Since we are not using the allocated memory because of
		 * error, free it 
		 */
		strcpy(ret_msg,
			"Unable to format, please check disk status and ensure cache is disabled");
	    }
	    break;

	case NVSD_DC_FORMAT_RESULT:
	    err = DM2_mgmt_TM_DB_update_msg(t_diskname,
		    DM2_MGMT_MOD_POST_FORMAT,
		    (void *) &t_value, NULL);
	    break;
	case NVSD_DC_REPAIR:
	    break;
	default:
	    break;
    }

bail:
    safe_free(node_name);
    return err;

}	/* end of nvsd_mgmt_diskcache_action_hdlr() */

/* ------------------------------------------------------------------------- */

/*
 *	function : get_diskstatus_from_diskid()
 *	purpose : get the disk status struct for the given disk id
 */
static const dm2_mgmt_disk_status_t *
get_diskstatus_from_diskid(const char *t_diskid)
{
    dm2_mgmt_db_info_t *pstElement;
    GList *pList;
    static dm2_mgmt_disk_status_t stDiskStatus;
    dm2_mgmt_disk_status_t *ret_val = NULL;
    int found = 0, err;

    /*
     * Sanity test 
     */
    if (NULL == t_diskid)
	return (NULL);

    /*
     * Lock first 
     */
    nvsd_mgmt_diskcache_lock();

    /*
     * Start with first element and search for the match 
     */
    pList = g_list_first(lstDiskCache);

    while (NULL != pList) {
	pstElement = (dm2_mgmt_db_info_t *) pList->data;
	if (NULL == pstElement) {
	    /*
	     * Should not happen 
	     */
	    ret_val = NULL;
	    goto bail;
	}

	/*
	 * See if it matches 
	 */
	if (0 == strcmp(pstElement->mi_serial_num, t_diskid) && found == 0) {
	    /*
	     * Get the status struct from DM module 
	     */
	    memset(&stDiskStatus, 0, sizeof (dm2_mgmt_disk_status_t));
	    err = DM2_mgmt_get_diskcache_info(pstElement, &stDiskStatus);
	    if (err != 0) {
		lc_log_debug(LOG_NOTICE, "DM2: get_diskcache_info failed : %d",
			err);
	    }
	    err = 0;

	    /*
	     * ignore absent disks 
	     */
	    if (stDiskStatus.ms_state != DM2_MGMT_STATE_NOT_PRESENT) {
		ret_val = &stDiskStatus;
	    }
	}

	/*
	 * Get the next element 
	 */
	pList = g_list_next(pList);
    }

bail:
    /*
     * Unlock when leaving 
     */
    nvsd_mgmt_diskcache_unlock();

    /*
     * Return 
     */
    return (ret_val);

}	/* end of get_diskstatus_from_diskid() */

/* ------------------------------------------------------------------------- */

/*
 *	function : get_diskname_from_diskid()
 *	purpose : get the diskname for the given disk id
 */
const char *
get_diskname_from_diskid(const char *t_diskid)
{
    const dm2_mgmt_disk_status_t *pstDiskStatus;

    /*
     * Sanity test 
     */
    if (!t_diskid)
	return (NULL);

    /*
     * Get the disk status struct 
     */
    pstDiskStatus = get_diskstatus_from_diskid(t_diskid);

    /*
     * Check if the names match 
     */
    if (pstDiskStatus && (0 != strcmp(pstDiskStatus->ms_name, "")))
	return (pstDiskStatus->ms_name);	/* Return the disk name */

    /*
     * Error hence return NULL 
     */
    return (NULL);

}	/* end of get_diskname_from_diskid() */

/* ------------------------------------------------------------------------- */

/*
 *	function : get_type_from_diskid()
 *	purpose : get the type for the given disk id
 */
const char *
get_type_from_diskid(const char *t_diskid)
{
    const dm2_mgmt_disk_status_t *pstDiskStatus;
    const char *cpTier;

    (void) cpTier;

    /*
     * Sanity test 
     */
    if (!t_diskid)
	return (NULL);

    /*
     * Get the disk status struct 
     */
    pstDiskStatus = get_diskstatus_from_diskid(t_diskid);

    /*
     * Check if the types match 
     */
    if (pstDiskStatus)
	return (pstDiskStatus->ms_tier);

    /*
     * Error hence return NULL 
     */
    return (NULL);

}	/* end of get_type_from_diskid() */

/* ------------------------------------------------------------------------- */

/*
 *	function : get_tier_from_diskid()
 *	purpose : get the tier for the given disk id
 */
const char *
get_tier_from_diskid(const char *t_diskid)
{
    const dm2_mgmt_disk_status_t *pstDiskStatus;
    const char *cpTier;

    /*
     * Sanity test 
     */
    if (!t_diskid)
	return (NULL);

    /*
     * Get the disk status struct 
     */
    pstDiskStatus = get_diskstatus_from_diskid(t_diskid);

    /*
     * Check if the tiers match 
     */
    if (pstDiskStatus) {
	cpTier = pstDiskStatus->ms_tier;
	if (!cpTier)
	    return (UNKNOWN_TIER_STR);

	/*
	 * Now check the tier and return the appropriate string 
	 */
	if (!strcmp(cpTier, SSD_STR))
	    return (TIER_1_STR);
	else if (!strcmp(cpTier, SAS_STR))
	    return (TIER_2_STR);
	else if (!strcmp(cpTier, SATA_STR))
	    return (TIER_3_STR);
	else
	    return (UNKNOWN_TIER_STR);
    }

    /*
     * Error hence return NULL 
     */
    return (NULL);
}	/* end of get_tier_from_diskid() */

/* ------------------------------------------------------------------------- */

/*
 *	function : get_state_from_diskid()
 *	purpose : get the state for the given disk id
 */
const char *
get_state_from_diskid(const char *t_diskid)
{
    const dm2_mgmt_disk_status_t *pstDiskStatus;
    const char *cpDiskState = NULL;

    /*
     * Sanity test 
     */
    if (!t_diskid)
	return (NULL);

    /*
     * Get the disk status struct 
     */
    pstDiskStatus = get_diskstatus_from_diskid(t_diskid);

    /*
     * Sanity test on the disk status 
     */
    if (!pstDiskStatus)
	return (NULL);

    /*
     * Return the relevant state info 
     */
    switch (pstDiskStatus->ms_state) {
	case DM2_MGMT_STATE_CACHEABLE:
	    cpDiskState = "disk cacheable, but cache not enabled";
	    break;
	case DM2_MGMT_STATE_CACHE_DISABLING:
	    cpDiskState = "disk disable under progress";
	    break;
	case DM2_MGMT_STATE_INVAL_FORMAT_BEFORE_MOUNT:
	case DM2_MGMT_STATE_FORMAT_UNKNOWN_AFTER_MOUNT:
	    cpDiskState = "disk has wrong format hence not cacheable";
	    break;
	case DM2_MGMT_STATE_DEACTIVATED:
	    cpDiskState = "disk has been deactivated";
	    break;
	case DM2_MGMT_STATE_ACTIVATED:
	    cpDiskState = "disk has been activated";
	    break;
	case DM2_MGMT_STATE_IMPROPER_UNMOUNT:
	case DM2_MGMT_STATE_IMPROPER_MOUNT:
	    cpDiskState = "soft disk error, try to clear";
	    break;

	case DM2_MGMT_STATE_CACHE_RUNNING:
	    cpDiskState = "cache running";
	    break;
	case DM2_MGMT_STATE_NOT_PRESENT:
	    cpDiskState = "disk no longer found (might not be present)";
	    break;
	case DM2_MGMT_STATE_WRONG_VERSION:
	    cpDiskState = "disk has the wrong cache version";
	    break;
	case DM2_MGMT_STATE_CONVERT_FAILURE:
	    cpDiskState = "conversion of disk cache version failed";
	    break;
	default:
	    cpDiskState = "unknown state, please try again a little later";
	    break;
    }

    return (cpDiskState);
}	/* end of get_state_from_diskid() */

/* ------------------------------------------------------------------------- */

/*
 *	function : disk_is_formatable()
 *	purpose : return true if disk is formatable else return false
 */
tbool
disk_is_formatable(const char *t_diskid)
{
    const dm2_mgmt_disk_status_t *pstDiskStatus;

    /*
     * Sanity Test 
     */
    if (!t_diskid)
	return (false);

    pstDiskStatus = get_diskstatus_from_diskid(t_diskid);

    /*
     * Sanity test on the disk status 
     */
    if (!pstDiskStatus)
	return (false);

    /*
     * Return the relevant state info 
     */
    if (DM2_MGMT_STATE_CACHEABLE == pstDiskStatus->ms_state)
	return (true);
    else
	return (false);
}	/* end of disk_is_formatable() */

/* ------------------------------------------------------------------------- */

/*
 *	function : get_uids_in_disk()
 *	purpose : return the list of uids that are in the disks
 */
char *
get_uids_in_disk(void)
{
    int err = 0;
    uint32_t len;
    GList *pList;
    char *cp_mnt_point;
    char *lst_nsuids = NULL;
    temp_buf_t cache_prefix = { 0 };
    temp_buf_t a_ns_uid = { 0 };
    temp_buf_t mount_path = { 0 };
    char *cp_namespace = NULL, *cp_uuid = NULL;
    tstr_array_options opts;
    dm2_mgmt_db_info_t *pstElement;
    const dm2_mgmt_disk_status_t *pstDiskStatus;

    /*
     * 4K can only hold 163 entries, and this function does not know
     * which entries are valid; so it must be prepared to return
     * valid entries plus invalid ones.  Since valid entries can
     * consume 25*256=6400 bytes, this buffer must be larger than 4K.
     * 8K would allow for 71 unattached namespaces but 12K will allow
     * for 235 unattached namespaces to be listed.  The current macro
     * is larger than 12K.
     */
#define MAX_BUFFER_SIZE (2*NKN_MAX_NAMESPACES*NKN_MAX_UID_LENGTH)

    (void) err;
    (void) cp_mnt_point;
    (void) opts;

    /*
     * Allocate the space 
     */
    lst_nsuids = nkn_calloc_type(MAX_BUFFER_SIZE, sizeof (char),
	    mod_mgmt_charbuf);
    if (!lst_nsuids)
	goto bail;
    len = MAX_BUFFER_SIZE;

    /*
     * Now iterate through the disk list 
     */
    /*
     * Start with first element 
     */
    pList = g_list_first(lstDiskCache);

    while (NULL != pList) {
	pstElement = (dm2_mgmt_db_info_t *) pList->data;
	if (pstElement) {
	    /*
	     * Get the disk status struct 
	     */
	    pstDiskStatus =
		get_diskstatus_from_diskid(pstElement->mi_serial_num);
	    if (pstDiskStatus
		    && (DM2_MGMT_STATE_CACHE_RUNNING == pstDiskStatus->ms_state)) {
		struct dirent **namelist;
		int n_entries, n;

		n_entries =
		    scandir(pstElement->mi_mount_point, &namelist, 0,
			    alphasort);
		if (n_entries >= 0) {
		    n = n_entries;
		    while (n--) {
			/*
			 * BUG 6720: Linux XFS doesnt support
			 * dirent:dir_type. So, the condition check for
			 * directory was always failing. Using the stat
			 * command to get the status of the file/directory
			 * Check if the path represents a directory. 
			 */
			struct stat statbuf;

			if (n > 0) {
			    /*
			     * Don't do stat for every ns_name:ns_uid_origin_server:port,
			     * As in the we need only ns_name:ns_uid,
			     */
			    const char *pchr = NULL;

			    /*
			     * "ns_name" can have an underscore, so search
			     * for the ':' first 
			     */
			    pchr = strchr(namelist[n - 1]->d_name, ':');
			    if (pchr == NULL) {
				lc_log_basic(LOG_DEBUG,
					"Skipping as same ns_uid %s:%s",
					namelist[n - 1]->d_name,
					namelist[n]->d_name);
				safe_free(namelist[n]);
				continue;
			    }
			    pchr = strchr(pchr, '_');
			    if (pchr
				    && !strncmp(namelist[n - 1]->d_name,
					namelist[n]->d_name,
					pchr - namelist[n - 1]->d_name)) {
				lc_log_basic(LOG_DEBUG,
					"Skipping as same ns_uid %s:%s",
					namelist[n - 1]->d_name,
					namelist[n]->d_name);
				safe_free(namelist[n]);
				continue;
			    }
			}
			snprintf(mount_path, sizeof (mount_path), "%s/%s",
				pstElement->mi_mount_point,
				namelist[n]->d_name);

			if (stat(mount_path, &statbuf) == -1) {
			    safe_free(namelist[n]);
			    continue;
			}
			/*
			 * Fix: For t-proxy setting we can have multiple
			 * enteries for same namespace -uid with different
			 * host.In that case, the deleted namespace should
			 * list the namespace only once. 
			 */
			snprintf(cache_prefix, sizeof (cache_prefix), "/%s",
				namelist[n]->d_name);

			/*
			 * Now get the namespace and uuid parts 
			 */
			cp_namespace =
			    get_part_from_cache_prefix(cache_prefix,
				    CP_PART_NAMESPACE,
				    strlen(cache_prefix));
			cp_uuid =
			    get_part_from_cache_prefix(cache_prefix,
				    CP_PART_UUID,
				    strlen(cache_prefix));
			/*
			 * Now create the uid string 
			 */
			if (cp_namespace && cp_uuid) {
			    snprintf(a_ns_uid, sizeof (a_ns_uid), "/%s:%s",
				    cp_namespace, cp_uuid);

			    if ((S_ISDIR(statbuf.st_mode)) &&
				    addable_ns_uid(a_ns_uid, lst_nsuids) &&
				    !configured_ns_uid(namelist[n]->d_name)) {
				/*
				 * Check the size to be sure 
				 */
				/*
				 * Size if strlen of d_name + \n and / 
				 */
				if (len <= (strlen(namelist[n]->d_name) + 2)) {
				    for (int i = n; i >= 0; i--) {
					safe_free(namelist[i]);
				    }
				    safe_free(namelist);
				    goto bail;	// We have used up the
				    // allocated size
				} else
				    len -= strlen(namelist[n]->d_name) + 2;

				strncat(lst_nsuids, a_ns_uid, strlen(a_ns_uid));
				strncat(lst_nsuids, "\n", 1);
			    }
			}
			/*
			 * Free the parts 
			 */
			safe_free(cp_namespace);
			safe_free(cp_uuid);
			safe_free(namelist[n]);
		    }
		    safe_free(namelist);
		}
	    }
	}

	/*
	 * Get the next element 
	 */
	pList = g_list_next(pList);
    }

bail:
    safe_free(cp_namespace);
    safe_free(cp_uuid);
    return (lst_nsuids);
}	/* end of get_uids_in_disk () */

/* ------------------------------------------------------------------------- */

/*
 *	function : compare_disk_entries ()
 *	purpose : compare the entries and retrun +ve, 0 or -ve number
 */
static gint
compare_disk_entries(gconstpointer elem1, gconstpointer elem2)
{
    dm2_mgmt_db_info_t *pstElement1 = (dm2_mgmt_db_info_t *) elem1;
    dm2_mgmt_db_info_t *pstElement2 = (dm2_mgmt_db_info_t *) elem2;
    int len1, len2, retval = 0;

    if (!pstElement2)
	return -1;
    if (!pstElement1)
	return 1;

    /*
     * Now compare and return, but need to account for devices past sdz.
     * sdz needs to be sorted before sdaa, so we need to consider length
     * too.
     */
    len1 = strlen(pstElement1->mi_raw_partition);
    len2 = strlen(pstElement2->mi_raw_partition);
    if (len1 == len2) {
	retval = strcmp(pstElement1->mi_raw_partition,
		pstElement2->mi_raw_partition);
    } else if (len1 < len2) {
	/*
	 * Return negative # when string 1 sorts before string 2 
	 */
	return -1;
    } else {
	/*
	 * Return positive # when string 2 sorts before string 1 
	 */
	return 1;
    }

    return retval;
}	/* end of compare_disk_entries () */

/* ------------------------------------------------------------------------- */

/*
 *	function : nvsd_mgmt_diskcache_action_pre_read_stop()
 *	purpose : Set a glob variable to indicate pre-read threads to stop.
 */
int
nvsd_mgmt_diskcache_action_pre_read_stop(void)
{
    int err = 0;

    /*
     * set variable here 
     */
    DM2_mgmt_stop_preread();
    bail_error(err);

bail:
    return err;
}	/* end of nvsd_mgmt_diskcache_action_pre_read_stop() */

int
get_all_disknames(tstr_array * resp_names, int which_data)
{
    int err = 0;
    dm2_mgmt_db_info_t *pstElement;
    GList *pList;
    const char *data = NULL, *disk_name = NULL;
    char diskname[16] = { 0 };
    dm2_mgmt_disk_status_t stDiskStatus;

    /*
     * Lock first 
     */
    nvsd_mgmt_diskcache_lock();

    /*
     * Start with first element and search for the match 
     */
    pList = g_list_first(lstDiskCache);

    while (NULL != pList) {
	pstElement = (dm2_mgmt_db_info_t *) pList->data;
	if (NULL == pstElement) {
	    /*
	     * Should not happen 
	     */
	    err = 1;
	    goto bail;
	}
	data = NULL;
	switch (which_data) {
	    case 1:				/* get disknames */
		if (0 == strcmp(pstElement->mi_mount_point, "")) {
		    /*
		     * no mount point 
		     */
		    break;
		}
		memset(&stDiskStatus, 0, sizeof (dm2_mgmt_disk_status_t));
		err = DM2_mgmt_get_diskcache_info(pstElement, &stDiskStatus);
		if (err != 0) {
		    lc_log_debug(LOG_NOTICE,
			    "DM2: get_diskcache_info failed : %d", err);
		}
		err = 0;

		if (stDiskStatus.ms_state == DM2_MGMT_STATE_NOT_PRESENT) {
		    /*
		     * not present, don't show it 
		     */
		    break;
		}
		/*
		 * mount point contains /nkn/mnt/dc_3 
		 */
		disk_name = strrchr(pstElement->mi_mount_point, '/');
		bail_null(disk_name);

		bzero(diskname, sizeof (diskname));
		strlcpy(diskname, disk_name + 1, sizeof (diskname));
		data = diskname;
		break;
	    case 2:				/* get disk_ids */
		if (0 == strcmp(pstElement->mi_serial_num, "")) {
		    /*
		     * no serial number 
		     */
		    break;
		}
		if (0 == strcmp(pstElement->mi_mount_point, "")) {
		    /*
		     * no mount point 
		     */
		    break;
		}
		data = pstElement->mi_serial_num;
		break;
	    default:
		err = 1;
		goto bail;
		break;
	}
	if (data) {
	    /*
	     * to be freed by called 
	     */
	    err = tstr_array_append_str(resp_names, data);
	    bail_error(err);
	}

	/*
	 * Get the next element 
	 */
	pList = g_list_next(pList);
    }

bail:
    /*
     * Unlock when leaving 
     */
    nvsd_mgmt_diskcache_unlock();

    /*
     * Return 
     */
    return err;

}	/* end of get_diskstatus_from_diskid() */
/* ------------------------------------------------------------------------- */

/* End of nkn_mgmt_diskcache.c */
