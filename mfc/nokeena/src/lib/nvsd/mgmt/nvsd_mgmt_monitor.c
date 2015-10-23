/*
 *
 * Filename:  nvsd_mgmt_monitor.c
 * Date:      2009/02/04
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008-2009 Nokeena Networks, Inc.
 * All rights reserved.
 *
 */

#include "md_client.h"
#include "bnode.h"
#include "nvsd_mgmt.h"
#include "nvsd_resource_mgr.h"

/* NVSD headers */

/* This header file has been included for macro definition of
 * UNUSED_ARGUMENT
 * */
#include "nkn_defs.h"
#include "nkn_cfg_params.h"
#include "nvsd_mgmt_namespace.h"
#include "nkn_namespace_stats.h"
#include "nvsd_mgmt_inline.h"

/*
 * A made up number for how many monitoring wildcards to indicate.
 * This is mainly used for performance testing.
 * The five fields per wildcard level are entirely arbitrary.
 */
const uint32 nvsd_num_ext_mon_nodes = 1000;

static int nvsd_mon_handle_get_wildcard(const char *bname,
	const tstr_array * bname_parts,
	uint32 num_parts,
	const tstring * last_name_part,
	bn_binding_array * resp_bindings,
	tbool * ret_done);

static tbool is_dm2_preread_done(mgmt_disk_tier_t tier);
extern int nkn_http_service_inited;

/* Extern Variables */
extern uint32 glob_ram_cachesize;
extern uint32 g_max_ram_cachesize_calculated;
extern uint64_t glob_dm2_init_done;
extern int nvsd_mgmt_thrd_initd;
extern int service_init_flags;

extern int64_t glob_dm2_sas_tier_avl;
extern int64_t glob_dm2_sas_preread_done;

extern int64_t glob_dm2_sata_tier_avl;
extern int64_t glob_dm2_sata_preread_done;

extern int64_t glob_dm2_ssd_tier_avl;
extern int64_t glob_dm2_ssd_preread_done;

extern int
search_namespace(const char *ns, int *return_idx);
extern uint32 g_cal_ram_dictsize_MB;
extern uint32 g_cal_AM_mem_limit;

extern uint64_t glob_bm_min_attrcount;
extern uint64_t g_rsrc_bw_1sec_val[NKN_MAX_RESOURCE_POOL];

static int
mgmt_str_num_compare_func(const void *elem1, const void *elem2);

/* ------------------------------------------------------------------------- */
int
nvsd_mon_handle_get(const char *bname,
	const tstr_array * bname_parts,
	void *data, bn_binding_array * resp_bindings)
{
    int err = 0;
    bn_binding *binding = NULL;
    uint32 num_parts = 0;

    UNUSED_ARGUMENT(data);

    /*
     * Check if the node is of interest to us 
     */
    if ((!lc_is_prefix("/nkn/nvsd/buffermgr/monitor", bname, false)) &&
	    (!lc_is_prefix("/nkn/nvsd/am/monitor", bname, false)) &&
	    (!lc_is_prefix("/nkn/nvsd/resource_mgr/ns", bname, false)) &&
	    (!lc_is_prefix("/nkn/nvsd/resource_mgr/monitor", bname, false)) &&
	    (!lc_is_prefix("/nkn/nvsd/services/monitor/state/nvsd",
			   bname, false))) {
	/*
	 * Not for us 
	 */
	goto bail;
    }

    num_parts = tstr_array_length_quick(bname_parts);

    if (!strcmp("/nkn/nvsd/buffermgr/monitor/cachesize", bname)) {
	err = bn_binding_new_parts_uint32
	    (&binding, bname, bname_parts, true, ba_value, 0, glob_ram_cachesize);
	bail_error(err);
    } else if (!strcmp
	    ("/nkn/nvsd/buffermgr/monitor/max_cachesize_calculated", bname)) {
	err =
	    bn_binding_new_parts_uint32(&binding, bname, bname_parts, true,
		    ba_value, 0,
		    g_max_ram_cachesize_calculated);
	bail_error(err);
    } else if (!strcmp("/nkn/nvsd/buffermgr/monitor/attributecount", bname)) {
	err = bn_binding_new_parts_uint32
	    (&binding, bname, bname_parts, true,
	     ba_value, 0, (uint32_t) glob_bm_min_attrcount);
	bail_error(err);
    } else if (bn_binding_name_parts_pattern_match(bname_parts, true,
		"/nkn/nvsd/resource_mgr/ns/*/cache_tier1_util")
	    || bn_binding_name_parts_pattern_match(bname_parts, true,
		"/nkn/nvsd/resource_mgr/ns/*/cache_tier2_util")
	    || bn_binding_name_parts_pattern_match(bname_parts, true,
		"/nkn/nvsd/resource_mgr/ns/*/cache_tier3_util")) {
	/*
	 * Get the namespace node data,get the nsuid,
	 Call ns_stat_get_stat_token,
	 call ns_stat_get

	 We should be getting cache utilization per namespace.
	 compute the percentage acroos the reserved space for the resource pool
	 associated with the namespace.
	 */
	namespace_node_data_t *pstNamespace = NULL;
	int ns_idx = 0;
	ns_stat_token_t ns_stoken = NS_NULL_STAT_TOKEN;
	uint64_t res_max = 0;
	int64_t counter_val = 0;
	uint64_t cache_ratio = 0;
	const char *ns_namespace = NULL;
	const char *str_res_type = NULL;
	ns_stat_category_t ns_stat_category;
	rp_type_en rp_type;

	str_res_type = tstr_array_get_str_quick(bname_parts, num_parts - 1);
	bail_null(str_res_type);

	ns_namespace = tstr_array_get_str_quick(bname_parts, num_parts - 2);
	bail_null(ns_namespace);

	err = search_namespace(ns_namespace, &ns_idx);
	bail_error(err);

	pstNamespace = get_namespace(ns_idx);

	if (pstNamespace && pstNamespace->uid)
	    ns_stoken = ns_stat_get_stat_token(pstNamespace->uid + 1);

	if (!strcmp(str_res_type, "cache_tier1_util")) {
	    rp_type = RESOURCE_POOL_TYPE_UTIL_TIER1;
	    ns_stat_category = NS_USED_SPACE_SSD;
	} else if (!strcmp(str_res_type, "cache_tier2_util")) {
	    rp_type = RESOURCE_POOL_TYPE_UTIL_TIER2;
	    ns_stat_category = NS_USED_SPACE_SAS;
	} else if (!strcmp(str_res_type, "cache_tier3_util")) {
	    rp_type = RESOURCE_POOL_TYPE_UTIL_TIER3;
	    ns_stat_category = NS_USED_SPACE_SATA;
	} else {
	    lc_log_basic(LOG_NOTICE, "Invalid resource Type");
	    if (ns_is_stat_token_null(ns_stoken))
		ns_stat_free_stat_token(ns_stoken);
	    goto bail;
	}

	err = ns_stat_get(ns_stoken, ns_stat_category, &counter_val);
	res_max =
	    AO_load(&pstNamespace->resource_pool->resources[rp_type - 1].max);

	if (res_max) {
	    cache_ratio = ((uint64_t) counter_val / res_max) * 100;
	}
	ns_stat_free_stat_token(ns_stoken);

	err = bn_binding_new_parts_uint64
	    (&binding, bname, bname_parts, true, ba_value, 0, cache_ratio);
	bail_error(err);
    } else if (!strcmp("/nkn/nvsd/buffermgr/monitor/cal_max_dict_size", bname)) {
	err = bn_binding_new_parts_uint32
	    (&binding, bname, bname_parts, true,
	     ba_value, 0, g_cal_ram_dictsize_MB);
	bail_error(err);
    } else if (!strcmp("/nkn/nvsd/am/monitor/cal_am_mem_limit", bname)) {
	err = bn_binding_new_parts_uint32
	    (&binding, bname, bname_parts, true,
	     ba_value, 0, g_cal_AM_mem_limit);
	bail_error(err);
    } else if (bn_binding_name_parts_pattern_match(bname_parts, true,
		"/nkn/nvsd/resource_mgr/monitor/external/**")) {
	const char *rsrc_pool = NULL;
	const char *rsrc_name = NULL;
	const char *get_type = NULL;
	resource_pool_t *rp = NULL;
	rp_type_en rp_type;
	uint64_t res_used;
	uint64_t res_max;

	rsrc_pool = tstr_array_get_str_quick(bname_parts, num_parts - 3);
	rsrc_name = tstr_array_get_str_quick(bname_parts, num_parts - 1);
	get_type = tstr_array_get_str_quick(bname_parts, num_parts - 2);

	bail_null(rsrc_pool);
	bail_null(rsrc_name);
	bail_null(get_type);

	lc_log_basic(LOG_DEBUG,
		"monitor request for rsrc mgr pool = %s, rsrc_name = %s, get_type = %s",
		rsrc_pool, rsrc_name, get_type);

	if (!strcmp(rsrc_name, "client_sessions")) {
	    rp_type = RESOURCE_POOL_TYPE_CLIENT_SESSION;

	} else if (!strcmp(rsrc_name, "origin_sessions")) {
	    rp_type = RESOURCE_POOL_TYPE_ORIGIN_SESSION;

	} else if (!strcmp(rsrc_name, "max_bandwidth")) {
	    rp_type = RESOURCE_POOL_TYPE_BW;

	} else if (!strcmp(rsrc_name, "reserved_diskspace_tier1")) {
	    rp_type = RESOURCE_POOL_TYPE_UTIL_TIER1;

	} else if (!strcmp(rsrc_name, "reserved_diskspace_tier2")) {
	    rp_type = RESOURCE_POOL_TYPE_UTIL_TIER2;

	} else if (!strcmp(rsrc_name, "reserved_diskspace_tier3")) {
	    rp_type = RESOURCE_POOL_TYPE_UTIL_TIER3;
	} else {
	    lc_log_basic(LOG_NOTICE, "Invalid Resource Type");
	}

	if (rp_type > RESOURCE_POOL_MAX)
	    goto bail;

	rp = nvsd_mgmt_get_resource_mgr(rsrc_pool);

	if (rp && (rp->resources) && (strcmp(get_type, "available") == 0)) {

	    res_used = AO_load(&rp->resources[rp_type - 1].used);
	    res_max = AO_load(&rp->resources[rp_type - 1].max);

	    err = bn_binding_new_parts_uint64
		(&binding, bname, bname_parts, true,
		 ba_value, 0, res_max - res_used);
	    bail_error(err);

	} else if (rp && (rp->resources) && (strcmp(get_type, "used") == 0)) {
	    // For Bw get the value from g_rsrc_bw_1_sec
	    if (rp_type == RESOURCE_POOL_TYPE_BW) {
		res_used = g_rsrc_bw_1sec_val[rp->index];
	    } else {
		res_used = AO_load(&rp->resources[rp_type - 1].used);
	    }

	    err = bn_binding_new_parts_uint64
		(&binding, bname, bname_parts, true, ba_value, 0, res_used);
	    bail_error(err);
	} else if (rp && (rp->resources) && (strcmp(get_type, "max") == 0)) {

	    res_used = AO_load(&rp->resources[rp_type - 1].max);

	    err = bn_binding_new_parts_uint64
		(&binding, bname, bname_parts, true, ba_value, 0, res_used);
	    bail_error(err);
	}

    } else if (!strcmp("/nkn/nvsd/services/monitor/state/nvsd/global", bname)) {

	/*
	 * Based on what is to be used for nvsd delivery readiness send the
	 * state 
	 */
	const char *ret_msg = NULL;

	if (nkn_system_inited && nkn_http_service_inited) {
	    if ((service_init_flags & PREREAD_INIT)) {
		tbool ret = true;
		mgmt_disk_tier_t tier;

		for (tier = MGMT_TIER_SATA; tier < MGMT_TIER_MAX; tier++) {
		    ret &= is_dm2_preread_done(tier);
		}
		ret_msg = ret ? ("Ready") : ("Initializing");
	    } else {
		ret_msg = "Ready";
	    }
	} else {
	    ret_msg = "Initializing";
	}

	err = bn_binding_new_parts_str(&binding, bname, bname_parts, true,
		ba_value, bt_string, 0, ret_msg);
	bail_error(err);
    } else if (!strcmp("/nkn/nvsd/services/monitor/state/nvsd/mgmt", bname)) {

	/*
	 * Based on what is to be used for nvsd delivery readiness send the
	 * state 
	 */
	const char *ret_msg = NULL;

	if (nvsd_mgmt_thrd_initd) {
	    ret_msg = "Ready";
	} else {
	    ret_msg = "Initializing";
	}

	err = bn_binding_new_parts_str(&binding, bname, bname_parts, true,
		ba_value, bt_string, 0, ret_msg);
	bail_error(err);
    } else if (!strcmp ("/nkn/nvsd/services/monitor/state/nvsd/preread/global"
		, bname)) {
	/*
	 * Based on what is to be used for nvsd delivery readiness send the
	 * state 
	 */
	const char *ret_msg = NULL;
	tbool ret = true;
	mgmt_disk_tier_t tier;

	for (tier = MGMT_TIER_SATA; tier < MGMT_TIER_MAX; tier++) {
	    ret &= is_dm2_preread_done(tier);
	}

	if (ret) {
	    ret_msg = "Ready";
	} else {
	    ret_msg = "Initializing";
	}

	if (!glob_dm2_init_done)
	    ret_msg = "Initializing";

	err = bn_binding_new_parts_str(&binding, bname, bname_parts, true,
		ba_value, bt_string, 0, ret_msg);
	bail_error(err);
    } else if (!strcmp("/nkn/nvsd/services/monitor/state/nvsd/preread/sas"
		, bname)) {

	/*
	 * Based on what is to be used for nvsd delivery readiness send the
	 * state 
	 */
	const char *ret_msg = NULL;

	if (is_dm2_preread_done(MGMT_TIER_SAS)) {
	    ret_msg = "Done";
	} else {
	    ret_msg = "Reading";
	}

	if (!glob_dm2_sas_tier_avl)
	    ret_msg = "Hardware not present";

	err = bn_binding_new_parts_str(&binding, bname, bname_parts, true,
		ba_value, bt_string, 0, ret_msg);
	bail_error(err);
    } else if (!strcmp("/nkn/nvsd/services/monitor/state/nvsd/preread/ssd"
		, bname)) {

	/*
	 * Based on what is to be used for nvsd delivery readiness send the
	 * state 
	 */
	const char *ret_msg = NULL;

	if (is_dm2_preread_done(MGMT_TIER_SSD)) {
	    ret_msg = "Done";
	} else {
	    ret_msg = "Reading";
	}

	if (!glob_dm2_ssd_tier_avl)
	    ret_msg = "Hardware not present";

	err = bn_binding_new_parts_str(&binding, bname, bname_parts, true,
		ba_value, bt_string, 0, ret_msg);
	bail_error(err);
    } else if (!strcmp ("/nkn/nvsd/services/monitor/state/nvsd/preread/sata"
		, bname)) {

	/*
	 * Based on what is to be used for nvsd delivery readiness send the
	 * state 
	 */
	const char *ret_msg = NULL;

	if (is_dm2_preread_done(MGMT_TIER_SATA)) {
	    ret_msg = "Done";
	} else {
	    ret_msg = "Reading";
	}

	if (!glob_dm2_sata_tier_avl)
	    ret_msg = "Hardware not present";

	err = bn_binding_new_parts_str(&binding, bname, bname_parts, true,
		ba_value, bt_string, 0, ret_msg);
	bail_error(err);
    } else if (!strcmp("/nkn/nvsd/services/monitor/state/nvsd/network", bname)) {

	/*
	 * Based on what is to be used for nvsd delivery readiness send the
	 * state 
	 */

	const char *ret_msg = NULL;

	if (nkn_http_service_inited) {
	    ret_msg = "Ready";
	} else {
	    ret_msg = "Initializing";
	}

	err = bn_binding_new_parts_str(&binding, bname, bname_parts, true,
		ba_value, bt_string, 0, ret_msg);
	bail_error(err);

    }

    if (binding) {
	err = bn_binding_array_append_takeover(resp_bindings, &binding);
	bail_error(err);
    }

bail:
    return (err);
}

/* ------------------------------------------------------------------------- */
int
nvsd_mon_handle_iterate(const char *bname,
	const tstr_array * bname_parts,
	void *data, tstr_array * resp_names)
{
    int err = 0, req_data = 0;
    const tstring *type = NULL;

    UNUSED_ARGUMENT(data);

    /*
     * 1 is for disknames 
     * 2 is for disk_id 
     */
    /*
     * SAMPLE node: /nkn/nvsd/diskcache/monitor/disk_id 
     */
    type = tstr_array_get_quick(bname_parts, 4);
    bail_null(type);

    if (0 == strcmp(ts_str_maybe_empty(type), "disk_id")) {
	req_data = 2;
    } else if (0 == strcmp(ts_str_maybe_empty(type), "diskname")) {
	req_data = 1;
    } else {
	lc_log_basic(LOG_NOTICE, "unsupported request recvd: %s", bname);
	goto bail;
    }
    err = get_all_disknames(resp_names, req_data);
    bail_error(err);

    if (req_data == 1) {
	/*
	 * only needs to sort disknames 
	 */
	array_options opts;

	memset(&opts, 0, sizeof (opts));
	err = tstr_array_options_get(resp_names, &opts);
	bail_error(err);
	opts.ao_elem_compare_func = mgmt_str_num_compare_func;
	err = tstr_array_options_set(resp_names, &opts, NULL);
	bail_error(err);
	err = tstr_array_sort(resp_names);
	bail_error(err);
    }

bail:
    return (err);
}

static int
mgmt_str_num_compare_func(const void *elem1, const void *elem2)
{
    const tstring *const *p1 = (const tstring * const *) elem1;
    const tstring *const *p2 = (const tstring * const *) elem2;
    int disk1 = 0, disk2 = 0;

    if (!ts_has_prefix_str(*p1, "dc_", false) ||
	    !ts_has_prefix_str(*p2, "dc_", false)) {
	/*
	 * if name is not starting with "dc_", no need to compare 
	 */
	return ts_cmp(elem1, elem2);
    }

    sscanf(ts_str_maybe_empty(*p1), "dc_%d", &disk1);
    sscanf(ts_str_maybe_empty(*p2), "dc_%d", &disk2);

    if (disk1 == disk2) {
	return (0);
    } else if (disk1 < disk2) {
	return (-1);
    } else {
	return (1);
    }
    /*
     * must never come here 
     */
    return ts_cmp(elem1, elem2);
}

/* ------------------------------------------------------------------------- */
static int
nvsd_mon_handle_get_wildcard(const char *bname,
	const tstr_array * bname_parts,
	uint32 num_parts,
	const tstring * last_name_part,
	bn_binding_array * resp_bindings, tbool * ret_done)
{
    int err = 0;
    bn_binding *binding = NULL;
    bn_type btype = bt_NONE;

    bail_null(last_name_part);
    bail_null(ret_done);
    *ret_done = false;

    /*
     * We assume the caller has verified that bname_parts begins
     * with "/nkn/nvsd/diskcache/monitor/<wc>", so we need only check if
     * we're at the wildcard number of parts and return the last name part
     * as the value.
     */

    if (num_parts == 6) {
	btype = bt_string;
    } else {
	goto bail;
    }

    err = bn_binding_new_parts_str(&binding, bname, bname_parts, true,
	    ba_value, btype, 0, ts_str(last_name_part));
    bail_error(err);

    err = bn_binding_array_append_takeover(resp_bindings, &binding);
    bail_error(err);
    *ret_done = true;

bail:
    return (err);
}

int
nvsd_disk_mon_handle_get(const char *bname,
	const tstr_array * bname_parts,
	void *data, bn_binding_array * resp_bindings)
{
    int err = 0;
    tbool done = false;
    tbool ext_mon_node = false;
    bn_binding *binding = NULL;
    uint32 num_parts = 0;
    const tstring *last_name_part = NULL;

    UNUSED_ARGUMENT(data);

    /*
     * Check if the node is of interest to us 
     */
    if ((!lc_is_prefix("/nkn/nvsd/diskcache/monitor", bname, false))) {
	/*
	 * Not for us 
	 */
	goto bail;
    }

    num_parts = tstr_array_length_quick(bname_parts);

    if (bn_binding_name_parts_pattern_match(bname_parts, true,
		"/nkn/nvsd/diskcache/monitor/disk_id/**")) {
	ext_mon_node = true;
    } else if (bn_binding_name_parts_pattern_match(bname_parts, true,
		"/nkn/nvsd/diskcache/monitor/diskname/**")) {
	ext_mon_node = true;
    }

    last_name_part = tstr_array_get_quick(bname_parts, num_parts - 1);

    /*
     * Check to see if it was a wildcard.  If so, just hand it back to
     * them as a value.   XXX: validation
     */
    if ((lc_is_prefix("/nkn/nvsd/diskcache/monitor/disk_id", bname, false)) ||
	    (lc_is_prefix("/nkn/nvsd/diskcache/monitor/diskname"
			  , bname, false))) {
	err = nvsd_mon_handle_get_wildcard(bname, bname_parts, num_parts,
		last_name_part, resp_bindings,
		&done);
	bail_error(err);
	if (done) {
	    goto bail;
	}
    }

    if (ext_mon_node && !strcmp(ts_str(last_name_part), "diskname")) {
	const tstring *diskid_part = NULL;
	const char *t_diskname = NULL;

	/*
	 * Get the diskid part from the node name 
	 */
	diskid_part = tstr_array_get_quick(bname_parts, num_parts - 2);
	bail_null(diskid_part);

	/*
	 * Get the disk name from the diskid 
	 */
	t_diskname = get_diskname_from_diskid(ts_str(diskid_part));
	if (NULL != t_diskname) {
	    /*
	     * Set the node to the disk name 
	     */
	    err = bn_binding_new_parts_str(&binding, bname, bname_parts, true,
		    ba_value, bt_string, 0, t_diskname);
	    bail_error(err);
	} else {
	    err = 0;
	    goto bail;
	}
    }

    if (ext_mon_node && !strcmp(ts_str(last_name_part), "disktype")) {
	const tstring *diskid_part = NULL;
	const char *t_disktype = NULL;

	/*
	 * Get the diskid part from the node name 
	 */
	diskid_part = tstr_array_get_quick(bname_parts, num_parts - 2);
	bail_null(diskid_part);

	/*
	 * Get the disk name from the diskid 
	 */
	t_disktype = get_type_from_diskid(ts_str(diskid_part));
	if (NULL != t_disktype) {
	    /*
	     * Set the node to the disk type 
	     */
	    err = bn_binding_new_parts_str(&binding, bname, bname_parts, true,
		    ba_value, bt_string, 0, t_disktype);
	    bail_error(err);
	} else {
	    err = 0;
	    goto bail;
	}
    }

    if (ext_mon_node && !strcmp(ts_str(last_name_part), "tier")) {
	const tstring *diskid_part = NULL;
	const char *t_tier = NULL;

	/*
	 * Get the diskid part from the node name 
	 */
	diskid_part = tstr_array_get_quick(bname_parts, num_parts - 2);
	bail_null(diskid_part);

	/*
	 * Get the disk name from the diskid 
	 */
	t_tier = get_tier_from_diskid(ts_str(diskid_part));
	if (NULL != t_tier) {
	    /*
	     * Set the node to the tier name 
	     */
	    err = bn_binding_new_parts_str(&binding, bname, bname_parts, true,
		    ba_value, bt_string, 0, t_tier);
	    bail_error(err);
	} else {
	    err = 0;
	    goto bail;
	}
    }

    else if (ext_mon_node && !strcmp(ts_str(last_name_part), "diskstate")) {
	const tstring *diskid_part = NULL;
	const char *t_disk_state = NULL;

	/*
	 * Get the diskid part from the node name 
	 */
	diskid_part = tstr_array_get_quick(bname_parts, num_parts - 2);
	bail_null(diskid_part);

	/*
	 * Get the disk state from the diskid 
	 */
	t_disk_state = get_state_from_diskid(ts_str(diskid_part));

	if (NULL != t_disk_state) {
	    /*
	     * Set the node to the disk state 
	     */
	    err = bn_binding_new_parts_str(&binding, bname, bname_parts, true,
		    ba_value, bt_string, 0,
		    t_disk_state);
	    bail_error(err);
	} else {
	    err = 0;
	    goto bail;
	}
    } else if (ext_mon_node && !strcmp(ts_str(last_name_part), "disk_id")) {
	const tstring *diskname_part = NULL;
	const char *t_disk_id = NULL;

	/*
	 * Get the diskname part from the node name 
	 */
	diskname_part = tstr_array_get_quick(bname_parts, num_parts - 2);
	bail_null(diskname_part);

	/*
	 * Get the disk id from the diskname 
	 */
	t_disk_id = get_diskid_from_diskname(ts_str(diskname_part));
	if (NULL != t_disk_id) {
	    /*
	     * Set the node to the disk id 
	     */
	    err = bn_binding_new_parts_str(&binding, bname, bname_parts, true,
		    ba_value, bt_string, 0, t_disk_id);
	    bail_error(err);
	} else {
	    err = 0;
	    goto bail;
	}
    } else if (ext_mon_node && !strcmp(ts_str(last_name_part), "pre_format")) {
	const tstring *diskname_part = NULL;
	char ret_msg[MAX_FORMAT_CMD_LEN];

	/*
	 * Get the diskname part from the node name 
	 */
	diskname_part = tstr_array_get_quick(bname_parts, num_parts - 2);
	bail_null(diskname_part);

	/*
	 * Get the format command from diskcache module 
	 */
	/*
	 * Call the handler to get the format command string 
	 */
	err = nvsd_mgmt_diskcache_action_hdlr(ts_str(diskname_part),
		NVSD_DC_FORMAT, true, ret_msg);

	/*
	 * Send the response with the format string and the err 
	 */
	err = bn_binding_new_parts_str(&binding, bname, bname_parts, true,
		ba_value, bt_string, 0, ret_msg);
	bail_error(err);
    } else if (ext_mon_node && !strcmp(ts_str(last_name_part), "pre_format_sb")) {
	const tstring *diskname_part = NULL;
	char ret_msg[MAX_FORMAT_CMD_LEN];

	/*
	 * Get the diskname part from the node name 
	 */
	diskname_part = tstr_array_get_quick(bname_parts, num_parts - 2);
	bail_null(diskname_part);

	/*
	 * Get the format command from diskcache module 
	 */
	/*
	 * Call the handler to get the format command string 
	 */
	err = nvsd_mgmt_diskcache_action_hdlr(ts_str(diskname_part),
		NVSD_DC_FORMAT_BLOCKS, true,
		ret_msg);

	/*
	 * Send the response with the format string and the err 
	 */
	err = bn_binding_new_parts_str(&binding, bname, bname_parts, true,
		ba_value, bt_string, 0, ret_msg);
	bail_error(err);
    }
    if (binding) {
	err = bn_binding_array_append_takeover(resp_bindings, &binding);
	bail_error(err);
    }

bail:
    return (err);
}

tbool
is_dm2_preread_done(mgmt_disk_tier_t tier)
{

    tbool ret = true;

    switch (tier) {
	case MGMT_TIER_SATA:
	    ret =
		(glob_dm2_sata_tier_avl) ? (glob_dm2_sata_preread_done ? true :
			false) : true;
	    break;
	case MGMT_TIER_SAS:
	    ret =
		(glob_dm2_sas_tier_avl) ? (glob_dm2_sas_preread_done ? true :
			false) : true;
	    break;
	case MGMT_TIER_SSD:
	    ret =
		(glob_dm2_ssd_tier_avl) ? (glob_dm2_ssd_preread_done ? true :
			false) : true;
	    break;
	default:
	    lc_log_basic(LOG_NOTICE, "Unknown Tier");
	    break;
    }
    return ret;
}
