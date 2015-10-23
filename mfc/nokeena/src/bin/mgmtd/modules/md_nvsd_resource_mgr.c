/*
 *
 * Filename:  md_nvsd_resource_mgr.c
 * Date:      2010/08/27
 * Author:    Thilak Raj S
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

/*----------------------------------------------------------------------------
 * HEADER FILES
 *---------------------------------------------------------------------------*/
#include "common.h"
#include "dso.h"
#include "md_utils.h"
#include "mdm_events.h"
#include "md_mod_commit.h"
#include "md_mod_reg.h"
#include "md_upgrade.h"
#include "proc_utils.h"
#include "nkn_cntr_api.h"
#include "file_utils.h"
#include "nkn_defs.h"
#include "md_nvsd_resource_mgr.h"
#include "nkn_mgmt_defs.h"
#include "nvsd_resource_mgr.h"
#include <string.h>
#include <strings.h>
#include <cprops/hashtable.h>

#define rm "/nkn/nvsd/resource_mgr/config/"
#define RESERVED_BANDWIDTH 125000
#define RESERVED_NET_CONNS 10

extern unsigned int jnpr_log_level;
float rsrc_bw_ratio = 1;
cp_hashtable *t = NULL;

uint64_t system_actual_bandwidth = 0;
uint64_t system_actual_net_conn = 0;

/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
int md_nvsd_resource_mgr_init(const lc_dso_info *info, void *data);

/* one extra for global_pool, zeroth is reserved is for global-pool */
struct resource_pool_s resource_pools[NKN_MAX_RESOURCE_POOL + 1];
const bn_str_value md_nvsd_rm_initial_values[] = { };

static int
md_nvsd_handle_link_event(md_commit *commit, const mdb_db *db,
		const char* event_name, const bn_binding_array *bindings,
		void *cb_reg_arg, void *cb_arg);
int md_nvsd_send_rp_update(const char *resource);
int update_all_rsrc_pools(float ratio, uint8_t type, uint64_t *aggr_rp);
int update_bandwidth_ratio(md_commit *commit, const mdb_db *db);
int get_config_rsrc(md_commit *commit, const mdb_db *db, uint8_t type, uint64_t *aggr_rp);
int update_network_conn(md_commit *commit, const mdb_db *db);
int update_global_pool_net_conns(md_commit *commit, const mdb_db *db, uint64_t net_conns);
void * rp_copy_val_fn (void *);


int md_lib_send_action_from_module(md_commit *commit, const char *action_name);
static int
md_nvsd_is_ns_active(const char *namespace, const mdb_db *db, tbool *active);
int update_global_pool_bw(md_commit *commit, const mdb_db *db, uint64_t bandwidth);
int md_rsrc_pool_update(md_commit *commit, const mdb_db *db, const mdb_db_change* change);
int md_rp_net_conn_update(md_commit *commit, const mdb_db *db, const mdb_db_change* change);

static int
md_nvsd_rm_get_system( md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg);
static int
md_nvsd_rm_get_allowed( md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg);
static int
md_nvsd_rm_iterate_allowed(md_commit *commit, const mdb_db *db,
                          const char *parent_node_name,
                          const uint32_array *node_attrib_ids,
                          int32 max_num_nodes, int32 start_child_num,
                          const char *get_next_child,
                          mdm_mon_iterate_names_cb_func iterate_cb,
                          void *iterate_cb_arg, void *arg);
static int
md_nvsd_rm_get_rp_name( md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg);
static int
md_nvsd_rm_config_alarm(
		md_commit *commit,
		mdb_db *inout_new_db,
		const char *alarm_name,
		const char *rp_name,
		const char *rp_type,
		uint64_t rising_err,
		uint64_t rising_clear,
		uint64_t falling_err,
		uint64_t falling_clear);

static int
md_nvsd_rm_adjust_alarm(
		md_commit *commit,
		mdb_db *db,
		const char *alarm_name,
		const char *rp_name,
		const char *rp_type,
		uint64_t rising_err,
		uint64_t rising_clear,
		uint64_t falling_err,
		uint64_t falling_clear);

static int
md_nvsd_rm_get_uesd_percentage(
		md_commit *commit,
		const mdb_db *db,
		const char *mon_node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg);
/*----------------------------------------------------------------------------
 * LOCAL FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
int update_global_bandwidth(md_commit *commit, mdb_db *new_bd, uint64_t old_value, uint64_t new_value, tbool calculate_tot_bw);
static md_upgrade_rule_array *md_nvsd_rm_ug_rules = NULL;

int md_lib_rp_get_config_bw(md_commit *commit, const mdb_db *db, uint64_t *tot_config_bw);
static int
md_nvsd_rm_commit_check(md_commit *commit,
		const mdb_db *old_db,
		const mdb_db *new_db,
		mdb_db_change_array *change_list,
		void *arg);
int
md_nvsd_rm_commit_side_effects( md_commit *commit,
		const mdb_db *old_db,
		mdb_db *inout_new_db,
		mdb_db_change_array *change_list,
		void *arg);

static int
md_nvsd_rm_commit_apply(md_commit *commit,
		const mdb_db *old_db,
		const mdb_db *new_db,
		mdb_db_change_array *change_list,
		void *arg);

static int
md_nvsd_rm_add_initial(md_commit *commit,
			mdb_db *db,
			void *arg);

static int
md_nvsd_rm_ns_get_trans(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg);

static int
md_nvsd_rm_ns_get_curr_ses(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg);

static int
md_nvsd_rm_ns_get_disk_stats(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg);

static int
md_nvsd_rm_ns_get_tier_stats(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg);

static int
md_nvsd_rm_ns_get_served_bytes(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg);

static int
md_nvsd_rm_get_counter_mbps(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg);

static int
md_nvsd_rm_get_counter(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg);

static int
md_nvsd_rm_ns_get_curr_bw(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg);


static int
md_nvsd_rm_ns_get_cache_hit_req(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg);

static int
md_nvsd_rm_ns_get_cache_hit_bytes(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg);

static int
md_nvsd_rm_ns_get_transactions(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg);

static int
md_nvsd_rm_ns_get_cache_util(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg);

static int
md_nvsd_rm_iterate_namespace(md_commit *commit,
		const mdb_db *db,
		const char *parent_node_name,
		const uint32_array *node_attrib_ids,
		int32 max_num_nodes, int32 start_child_num,
		const char *get_next_child,
		mdm_mon_iterate_names_cb_func iterate_cb,
		void *iterate_cb_arg, void *arg);

static int
md_nvsd_rm_get_namespace(md_commit *commit, const mdb_db *db,
                      const char *node_name,
                      const bn_attrib_array *node_attribs,
                      bn_binding **ret_binding,
                      uint32 *ret_node_flags, void *arg);

static int
md_nvsd_rm_get_rp(md_commit *commit, const mdb_db *db,
                      const char *node_name,
                      const bn_attrib_array *node_attribs,
                      bn_binding **ret_binding,
                      uint32 *ret_node_flags, void *arg);
static int
md_nvsd_rm_iterate_rp(md_commit *commit, const mdb_db *db,
                          const char *parent_node_name,
                          const uint32_array *node_attrib_ids,
                          int32 max_num_nodes, int32 start_child_num,
                          const char *get_next_child,
                          mdm_mon_iterate_names_cb_func iterate_cb,
                          void *iterate_cb_arg, void *arg);

static int
md_nvsd_rm_get_tier_size(const char* tier,
			uint64 *disksize,
			md_commit *commit,
			const mdb_db *mdb);

static int
md_nvsd_resourece_mgr_get_disk(
    md_commit * commit,
    const mdb_db *db,
    tstr_array **ret_labels);

static int
md_nvsd_rm_ns_get_test(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg);

static int
md_nvsd_rm_ns_get_tier_size(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg);
int (*md_lib_cal_tot_avail_bw)(md_commit *commit, const mdb_db *new_db, uint64_t *tot_bw, tbool count_down_link);
static int
md_resource_pool_upgrade(const mdb_db *old_db, mdb_db *inout_new_db,
                      uint32 from_module_version, uint32 to_module_version,
                      void *arg);
/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION DEFINITIONS
 *---------------------------------------------------------------------------*/

static int
md_resource_pool_upgrade(const mdb_db *old_db, mdb_db *inout_new_db,
                      uint32 from_module_version, uint32 to_module_version,
                      void *arg)
{
    int err = 0;

    err = md_generic_upgrade_downgrade
        (old_db, inout_new_db, from_module_version, to_module_version, arg);
    bail_error(err);

    if ((to_module_version == 3) && (from_module_version == 2)) {
	/* change global pool values to percentage basis */
	err = md_nvsd_rm_config_alarm(NULL, inout_new_db,"rp_global_pool_client_sessions",
		GLOBAL_RSRC_POOL, "client_sessions", 0, 0, 0, 0);
	bail_error(err);

	err = md_nvsd_rm_config_alarm(NULL, inout_new_db,"rp_global_pool_max_bandwidth",
		GLOBAL_RSRC_POOL, "max_bandwidth", 0, 0, 0, 0);
	bail_error(err);
    }
bail:
    return err;
}

int
md_nvsd_resource_mgr_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    /*
     * Creating the Nokeena specific module
     */
    err = mdm_add_module("nvsd-resource_mgr", 3,
	    "/nkn/nvsd/resource_mgr", NULL,
	    modrf_all_changes,
	    md_nvsd_rm_commit_side_effects, NULL,
	    md_nvsd_rm_commit_check, NULL,
	    md_nvsd_rm_commit_apply, NULL,
	    0,
	    0,
	    md_resource_pool_upgrade,
	    &md_nvsd_rm_ug_rules,
	    md_nvsd_rm_add_initial,
	    NULL,
	    NULL,
	    NULL,
	    NULL,
	    NULL,
	    &module);
    bail_error(err);

    t = cp_hashtable_create_by_option(COLLECTION_MODE_NOSYNC |
	    COLLECTION_MODE_COPY |
	    COLLECTION_MODE_DEEP,
	    NKN_MAX_RESOURCE_POOL +1,
	    cp_hash_string,
	    (cp_compare_fn) strcmp,
	    (cp_copy_fn) strdup,
	    (cp_destructor_fn) free,
	    (cp_copy_fn)rp_copy_val_fn,
	    (cp_destructor_fn) free);

    bail_null(t);

    err = mdm_get_symbol("md_nvsd_http", "md_lib_cal_tot_avail_bw",
	    (void **)&md_lib_cal_tot_avail_bw);
    bail_error_null(err, md_lib_cal_tot_avail_bw);

    err = md_events_handle_int_interest_add(
	    "/net/interface/events/link_up",
	    md_nvsd_handle_link_event, NULL);
    bail_error(err);

    err = md_events_handle_int_interest_add(
	    "/net/interface/events/link_down",
	    md_nvsd_handle_link_event, NULL);
    bail_error(err);

    // Resouce Mgr Config Nodes
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/resource_mgr/config/*";
    node->mrn_value_type =         bt_string;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_limit_str_max_chars = MAX_RSRC_POOL_NAME;
    node->mrn_limit_str_no_charlist = "/\\*:|`\"?";
    node->mrn_limit_wc_count_max = NKN_MAX_RESOURCE_POOL;
    node->mrn_bad_value_msg =	"Error creating Node";
    node->mrn_description =        "Resource Mgr config nodes";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/resource_mgr/config/*/parent";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      GLOBAL_RSRC_POOL;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "parent resource pool";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =		"/nkn/nvsd/resource_mgr/config/*/client_sessions";
    node->mrn_value_type =		bt_uint64;
    node->mrn_limit_num_min_int =	1;
    node->mrn_initial_value =	"10";
    node->mrn_node_reg_flags =	mrf_flags_reg_config_literal;
    node->mrn_cap_mask =		mcf_cap_node_basic;
    node->mrn_description =		"An entry for each script based policy";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =		"/nkn/nvsd/resource_mgr/config/*/origin_sessions";
    node->mrn_value_type =		bt_uint64;
    node->mrn_initial_value =	"0";
    node->mrn_node_reg_flags =	mrf_flags_reg_config_literal;
    node->mrn_cap_mask =		mcf_cap_node_basic;
    node->mrn_description =		"An entry for each script based policy";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* this node tells the configured max bandwidth */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =		"/nkn/nvsd/resource_mgr/config/*/max_bandwidth";
    node->mrn_value_type =		bt_uint64;
    node->mrn_initial_value =	"125000";
    node->mrn_limit_num_min_int =	1;
    node->mrn_node_reg_flags =	mrf_flags_reg_config_literal;
    node->mrn_cap_mask =		mcf_cap_node_basic;
    node->mrn_description =		"An entry for each script based policy";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* this node is set as side effect of delivery interfaces */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =		"/nkn/nvsd/resource_mgr/total/conf_max_bw";
    node->mrn_value_type =		bt_uint64;
    node->mrn_initial_value =	"0";
    node->mrn_node_reg_flags =	mrf_flags_reg_config_literal;
    node->mrn_cap_mask =		mcf_cap_node_restricted;
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* this node is set as side effect of creation/deletion of resource pool */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =		"/nkn/nvsd/resource_mgr/global_pool/conf_max_bw";
    node->mrn_value_type =		bt_uint64;
    node->mrn_initial_value =	"0";
    node->mrn_node_reg_flags =	mrf_flags_reg_config_literal;
    node->mrn_cap_mask =		mcf_cap_node_restricted|mrf_change_no_notify;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =		"/nkn/nvsd/resource_mgr/config/*/reserved_diskspace_tier1";
    node->mrn_value_type =		bt_uint64;
    node->mrn_initial_value =	"0";
    node->mrn_node_reg_flags =	mrf_flags_reg_config_literal;
    node->mrn_cap_mask =		mcf_cap_node_basic;
    node->mrn_description =		"Cache Tier 1 reservation";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =		"/nkn/nvsd/resource_mgr/config/*/reserved_diskspace_tier2";
    node->mrn_value_type =		bt_uint64;
    node->mrn_initial_value =	"0";
    node->mrn_node_reg_flags =	mrf_flags_reg_config_literal;
    node->mrn_cap_mask =		mcf_cap_node_basic;
    node->mrn_description =		"Cache Tier 2 reservation";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =		"/nkn/nvsd/resource_mgr/config/*/reserved_diskspace_tier3";
    node->mrn_value_type =		bt_uint64;
    node->mrn_initial_value = 	"0";
    node->mrn_node_reg_flags =	mrf_flags_reg_config_literal;
    node->mrn_cap_mask =		mcf_cap_node_basic;
    node->mrn_description =		"Cache Tier 3 reservation";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/resource_mgr/config/*/namespace/*";
    node->mrn_value_type =         bt_string;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_limit_str_max_chars = 16;
    node->mrn_limit_str_no_charlist = "/\\*:|`\"?";
    node->mrn_description =        "List of namespace under resource pool";
    err = mdm_add_node(module, &node);
    bail_error(err);

    //End of Resource mgr config nodes

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =              "/nkn/nvsd/resource_mgr/namespace/*";
    node->mrn_value_type =        bt_string;
    node->mrn_node_reg_flags =    mrf_flags_reg_monitor_wildcard;
    node->mrn_cap_mask =          mcf_cap_node_basic;
    node->mrn_mon_get_func =      md_nvsd_rm_get_namespace;
    node->mrn_mon_iterate_func =  md_nvsd_rm_iterate_namespace;
    node->mrn_description =       "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =              "/nkn/nvsd/resource_mgr/ns/*";
    node->mrn_value_type =        bt_string;
    node->mrn_node_reg_flags =    mrf_flags_reg_monitor_wildcard;
    node->mrn_cap_mask =          mcf_cap_node_basic;
    node->mrn_mon_get_func =      md_nvsd_rm_get_namespace;
    node->mrn_mon_iterate_func =  md_nvsd_rm_iterate_namespace;
    node->mrn_description =       "";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/namespace/*/http/served_bytes";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_ns_get_served_bytes;
    node->mrn_mon_get_arg = (void*)("http");
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/namespace/*/rtsp/served_bytes";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_ns_get_served_bytes;
    node->mrn_mon_get_arg = (void*)("rtsp");
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* Defined these nodes here,as the nodes under /stats/nkn
     * have a lot of sibliling and on asampling makes request to all
     * the siblings
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/namespace/*/http/transactions";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_ns_get_trans;
    node->mrn_mon_get_arg = (void*)("http");
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/namespace/*/rtsp/transactions";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_ns_get_trans;
    node->mrn_mon_get_arg = (void*)("rtsp");
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/ns/*/http/curr_ses";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_ns_get_curr_ses;
    node->mrn_mon_get_arg = (void*)("http");
    node->mrn_description = "Current number of sessions for HTTP";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/ns/*/disk/obj_count";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_ns_get_disk_stats;
    node->mrn_mon_get_arg = (void*)("total_objects");
    node->mrn_description = "Current number of sessions for HTTP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/ns/*/disk/sas/obj_count";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_ns_get_tier_stats;
    node->mrn_mon_get_arg = (void*)(5);
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/ns/*/disk/sata/obj_count";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_ns_get_tier_stats;
    node->mrn_mon_get_arg = (void*)(6);
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/ns/*/disk/ssd/obj_count";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_ns_get_tier_stats;
    node->mrn_mon_get_arg = (void*)(1);
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/ns/*/rtsp/curr_ses";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_ns_get_curr_ses;
    node->mrn_mon_get_arg = (void*)("rtsp");
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/ns/*/http/curr_bw";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_ns_get_curr_bw;
    node->mrn_mon_get_arg = (void*)("http");
    node->mrn_description = "Current number of sessions for HTTP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/ns/*/rtsp/curr_bw";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_ns_get_curr_bw;
    node->mrn_mon_get_arg = (void*)("rtsp");
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/ns/*/http/cache_hit_ratio_on_req";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_ns_get_cache_hit_req;
    node->mrn_mon_get_arg = (void*)("http");
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/ns/*/rtsp/cache_hit_ratio_on_req";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_ns_get_cache_hit_req;
    node->mrn_mon_get_arg = (void*)("rtsp");
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/ns/*/http/cache_hit_ratio_on_bytes";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_ns_get_cache_hit_bytes;
    node->mrn_mon_get_arg = (void*)("http");
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/ns/*/rtsp/cache_hit_ratio_on_bytes";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_ns_get_cache_hit_bytes;
    node->mrn_mon_get_arg = (void*)("rtsp");
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    //Use the sample nodes directly
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/ns/*/http/transactions_per_sec";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_ns_get_transactions;
    node->mrn_mon_get_arg = (void*)("http");
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/ns/*/rtsp/transactions_per_sec";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_ns_get_transactions;
    node->mrn_mon_get_arg = (void*)("rtsp");
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* Monitoring nodes which are to be handled by NVSD to know the available resource */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =              "/nkn/nvsd/resource_mgr/monitor/external/*";
    node->mrn_value_type =        bt_string;
    node->mrn_node_reg_flags =    mrf_flags_reg_monitor_wildcard;
    node->mrn_cap_mask =          mcf_cap_node_basic;
    node->mrn_mon_get_func =      md_nvsd_rm_get_rp;
    node->mrn_mon_iterate_func =  md_nvsd_rm_iterate_rp;
    node->mrn_description =       "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    //Used Resource
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/external/*/used/client_sessions";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_nvsd;
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/external/*/used/origin_sessions";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_nvsd;
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/external/*/used/max_bandwidth";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_nvsd;
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/external/*/used/reserved_diskspace_tier1";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_nvsd;
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/external/*/used/reserved_diskspace_tier2";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_nvsd;
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/external/*/used/reserved_diskspace_tier3";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_nvsd;
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    //Available Resources (max - used)
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/external/*/available/client_sessions";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_nvsd;
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/external/*/available/origin_sessions";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_nvsd;
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/external/*/available/max_bandwidth";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_nvsd;
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/external/*/available/reserved_diskspace_tier1";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_nvsd;
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/external/*/available/reserved_diskspace_tier2";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_nvsd;
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/external/*/available/reserved_diskspace_tier3";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_nvsd;
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);


    //Running Max resources
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/external/*/max/client_sessions";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_nvsd;
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/external/*/max/origin_sessions";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_nvsd;
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/external/*/max/max_bandwidth";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_nvsd;
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/external/*/max/reserved_diskspace_tier1";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_nvsd;
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/external/*/max/reserved_diskspace_tier2";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_nvsd;
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/external/*/max/reserved_diskspace_tier3";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_nvsd;
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);


    /* Monitoring nodes counter style  */

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =              "/nkn/nvsd/resource_mgr/monitor/counter/*";
    node->mrn_value_type =        bt_string;
    node->mrn_node_reg_flags =    mrf_flags_reg_monitor_wildcard;
    node->mrn_cap_mask =          mcf_cap_node_basic;
    node->mrn_mon_get_func =      md_nvsd_rm_get_rp;
    node->mrn_mon_iterate_func =  md_nvsd_rm_iterate_rp;
    node->mrn_description =       "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    //Used Resource
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/counter/*/used/client_sessions";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_get_counter;
    node->mrn_mon_get_arg = (void*)("client.sessions");
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/counter/*/used/origin_sessions";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_get_counter;
    node->mrn_mon_get_arg = (void*)("server.origin.sessions");
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/counter/*/used/max_bandwidth";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_get_counter;
    node->mrn_mon_get_arg = (void*)("client.bw");
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/counter/*/used/bw_mbps";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_get_counter_mbps;
    node->mrn_mon_get_arg = (void*)("client.bw");
    node->mrn_description = "Bandwidth used by resourece pool";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/counter/*/used/reserved_diskspace_tier1";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_get_counter;
    node->mrn_mon_get_arg = (void*)("cache.tier.SAS.bytes");
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/counter/*/used/reserved_diskspace_tier2";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_get_counter;
    node->mrn_mon_get_arg = (void*)("cache.tier.SATA.bytes");
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/counter/*/used/reserved_diskspace_tier3";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_get_counter;
    node->mrn_mon_get_arg = (void*)("cache.tier.SSD.bytes");
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/state/*";
    node->mrn_value_type = bt_string;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_wildcard;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_get_rp_name;
    /* get_arg == 1 is for bandwidth */
    node->mrn_mon_get_arg = (void*)("1");
    node->mrn_mon_iterate_func =  md_nvsd_rm_iterate_allowed;
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/state/*/allowed/bandwidth";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_get_allowed;
    /* get_arg == 1 is for bandwidth */
    node->mrn_mon_get_arg = (void*)("1");
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/state/*/allowed/net_conn";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_get_allowed;
    /* get_arg == 1 is for bandwidth */
    node->mrn_mon_get_arg = (void*)("2");
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* calculate used %age for client sessions */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/state/*/used/p_age/client_sessions";
    node->mrn_value_type = bt_uint16;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_get_uesd_percentage;
    node->mrn_mon_get_arg = (void*)(e_rp_client_sessions);
    node->mrn_description = "Current number of sessions for Resource Pool";
    err = mdm_add_node(module, &node);
    bail_error(err);
    /* calculate used %age for bandwidth */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/state/*/used/p_age/max_bandwidth";
    node->mrn_value_type = bt_uint16;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_get_uesd_percentage;
    node->mrn_mon_get_arg = (void*)(e_rp_client_bandwidth);
    node->mrn_description = "Current %age of used bandwidth for Resource Pool";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/system/bandwidth";
    node->mrn_value_type = bt_string;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_get_system;
    /* get_arg == 1 is for bandwidth */
    node->mrn_mon_get_arg = (void*)("1");
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/system/net_conn";
    node->mrn_value_type = bt_string;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_get_system;
    /* get_arg == 1 is for bandwidth */
    node->mrn_mon_get_arg = (void*)("2");
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    //Running Max resources
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/counter/*/max/client_sessions";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_get_counter;
    node->mrn_mon_get_arg = (void*)("client.sessions");
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/counter/*/max/origin_sessions";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_get_counter;
    node->mrn_mon_get_arg = (void*)("server.origin.sessions");
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/counter/*/max/max_bandwidth";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_get_counter;
    node->mrn_mon_get_arg = (void*)("client.bw");
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/counter/*/max/bw_mbps";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_get_counter_mbps;
    node->mrn_mon_get_arg = (void*)("client.bw");
    node->mrn_description = "Bandwidth used by resourece pool";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/counter/*/max/reserved_diskspace_tier1";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_get_counter;
    node->mrn_mon_get_arg = (void*)("disk_tier1");
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/counter/*/max/reserved_diskspace_tier2";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_get_counter;
    node->mrn_mon_get_arg = (void*)("disk_tier2");
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/monitor/counter/*/max/reserved_diskspace_tier3";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_mon_get_func = md_nvsd_rm_get_counter;
    node->mrn_mon_get_arg = (void*)("disk_tier3");
    node->mrn_description = "Current number of sessions for RTSP";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*Event to send email when we think NVSD might be stuck */
    err = mdm_new_event(module, &node, 0);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/resource_mgr/state/update";
    node->mrn_node_reg_flags = mrf_flags_reg_event;
    node->mrn_cap_mask = mcf_cap_event_privileged;
    node->mrn_description = "Event sent to notify that nvsd might be stuck";
    err = mdm_add_node(module, &node);
    bail_error(err);


    /* Upgrade processing nodes */

    err = md_upgrade_rule_array_new(&md_nvsd_rm_ug_rules);
    bail_error(err);
    ra = md_nvsd_rm_ug_rules;

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/resource_mgr/total/conf_max_bw";
    rule->mur_new_value_type =  bt_uint64;
    rule->mur_new_value_str  =  "0";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/resource_mgr/global_pool/conf_max_bw";
    rule->mur_new_value_type =  bt_uint64;
    rule->mur_new_value_str  =  "0";
    MD_ADD_RULE(ra);

    memset(resource_pools, 0, sizeof(resource_pools));

bail:
    return(err);
}

int
md_nvsd_rm_commit_side_effects( md_commit *commit,
		const mdb_db *old_db,
		mdb_db *inout_new_db,
		mdb_db_change_array *change_list,
		void *arg) {

	int err = 0;
	uint32 num_changes = 0;
	uint32 i = 0, j = 0;
	uint64_t total_bw_add = 0, total_bw_remove = 0;
	const mdb_db_change *change = NULL;
	const char *resource_pool_name = NULL;
	tstr_array *t_name_parts = NULL;
	tstring *t_parent = NULL;
	const char *resource_node = NULL;
	int update_global_bw = 0;
	tbool one_shot = false, initial = false, calculate_rp_conf = false;
	node_name_t alarm_node = {0};

	num_changes = mdb_db_change_array_length_quick(change_list);
	lc_log_basic(jnpr_log_level, "%s: num_changes - %d",__FUNCTION__, num_changes);
	for(i = 0; i < num_changes; i++) {
		change = mdb_db_change_array_get_quick(change_list, i);
		bail_null(change);

		if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 5, "nkn", "nvsd", "resource_mgr", "config", "**"))) {
		    lc_log_basic(jnpr_log_level, "%s: node- %s", __FUNCTION__, ts_str(change->mdc_name));

			resource_pool_name = tstr_array_get_str_quick(change->mdc_name_parts, 4);
			if (!resource_pool_name) goto bail;

			if(!strcmp(resource_pool_name, GLOBAL_RSRC_POOL)) {
				/* Dont let the user changes the values for teh global pool*/
				err = md_commit_set_return_status_msg_fmt(commit, 1,
						_("Global Pool values cannot be changed .\n"));
				bail_error(err);
			} else {
				//Dont do arithmetic for parent node.
				if((!bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6,
							"nkn", "nvsd", "resource_mgr", "config", "*", "parent")) &&
						(!bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 5,
							"nkn", "nvsd", "resource_mgr", "config", "*")) &&
						(!bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7,
							"nkn", "nvsd", "resource_mgr", "config", "*", "namespace", "*")) ) {

				    uint64_t resource_count = 0;
				    uint64_t parent_avail_resource_count = 0;
				    uint64_t parent_used_resource_count = 0;
				    uint64_t parent_max_resource_count = 0;
				    uint64_t old_resource_count = 0;

				if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6,
					    "nkn", "nvsd", "resource_mgr", "config", "*", "max_bandwidth")) {
				    uint64_t old_value = 0, new_value = 0;
				    update_global_bw = 1;
				    if (change->mdc_change_type == mdct_add) {
					err = mdb_get_node_value_uint64(commit,inout_new_db,
						ts_str(change->mdc_name),0, NULL, &new_value);
					bail_error(err);
					total_bw_add+=new_value;
				    } else if (change->mdc_change_type == mdct_delete) {
					err = mdb_get_node_value_uint64(commit, old_db,
						ts_str(change->mdc_name),0, NULL, &old_value);
					bail_error(err);
					total_bw_remove+=old_value;
				    } else {
					/* modify bandwidth */
					err = mdb_get_node_value_uint64(commit, old_db,
						ts_str(change->mdc_name), 0, NULL, &old_value);
					bail_error(err);
					err = mdb_get_node_value_uint64(commit, inout_new_db,
						ts_str(change->mdc_name),0, NULL, &new_value);
					bail_error(err);
					total_bw_add+=new_value;
					total_bw_remove+=old_value;
				    }
				    lc_log_basic(jnpr_log_level, "%s: node- %s, change_type - %d",
					    __FUNCTION__, ts_str(change->mdc_name), change->mdc_change_type);
				}
				resource_node = tstr_array_get_str_quick(change->mdc_name_parts, 5);
				snprintf(alarm_node, sizeof(alarm_node), "rp_%s_%s",
					resource_pool_name, resource_node);
				if (change->mdc_change_type == mdct_add) {
				    /* add stats nodes */
				    err = md_nvsd_rm_config_alarm(commit, inout_new_db,alarm_node,
					    resource_pool_name, resource_node,
					    0, 0, 0, 0);
				} else if (change->mdc_change_type == mdct_delete) {
				    /* delete stats nodes */
				}
				}
			}
		}//mdct_delete
	    tstr_array_free(&t_name_parts);
	}
	/* TODO: following code is not required, can be removed */
	err = md_commit_get_commit_mode(commit, &one_shot);
	bail_error(err);
	if (one_shot) {
	    goto bail;
	}
	err = md_commit_get_commit_initial(commit, &initial);
	bail_error(err);
	if (initial) {
	    /* use total_bw_remove and total_bw_add */
	    lc_log_basic(jnpr_log_level, "%s: inital confg", __FUNCTION__);
	    calculate_rp_conf = 0;
	} else if (update_global_bw == 1) {
	    /* iterate over all resource pools and sum-up current configu resouce pool bandwidth */
	    calculate_rp_conf = 1;
	} else {
	    goto bail;
	}
	lc_log_basic(jnpr_log_level, "%s: add - %lu, remove %lu", __FUNCTION__, total_bw_add, total_bw_remove);
	//err = update_global_bandwidth(commit, inout_new_db, total_bw_remove, total_bw_add, calculate_rp_conf);
	bail_error(err);
	/* TODO: above code can be removed */
bail:
	tstr_array_free(&t_name_parts);
	ts_free(&t_parent);
	return err;
}
/*----------------------------------------------------------------------------
 * LOCAL FUNCTION DEFINITIONS
 *---------------------------------------------------------------------------*/

int
md_lib_rp_get_config_bw(md_commit *commit, const mdb_db *db, uint64_t *tot_config_bw)
{
    int err = 0, i = 0, num_binding = 0;
    bn_binding_array *bindings = NULL;
    bn_binding *binding = NULL;
    uint64_t rp_conf_bw = 0;
    char tmp_buf[256] = { 0 };
    const tstring *bn_name = NULL;

    err = mdb_iterate_binding(commit, db, "/nkn/nvsd/resource_mgr/config", 0, &bindings);
    bail_error(err);
    lc_log_basic(jnpr_log_level, "%s: got %d nodes ", __FUNCTION__, num_binding);
    num_binding = bn_binding_array_length_quick(bindings);
    for (i=0; i < num_binding; i++) {
	binding = bn_binding_array_get_quick(bindings, i);
	if (binding == NULL) {
	    continue;
	}
	bn_name = bn_binding_get_name_quick(binding);
	if (bn_name == NULL) {
	    continue;
	}
	tmp_buf[0] = '\0';
	snprintf(tmp_buf, sizeof(tmp_buf), "%s/max_bandwidth", ts_str(bn_name));
	rp_conf_bw = 0;
	err = mdb_get_node_value_uint64(commit, db,tmp_buf, 0, NULL, &rp_conf_bw);
	bail_error(err);
	*tot_config_bw += rp_conf_bw;
	lc_log_basic(jnpr_log_level, "%s: node- %s, rp-bw=> %lu, tot-bw-> %lu, ",__FUNCTION__, tmp_buf,rp_conf_bw, *tot_config_bw);
    }

bail:
    bn_binding_array_free(&bindings);
    return err;
}

int
update_global_bandwidth(md_commit *commit, mdb_db *new_db, uint64_t old_value, uint64_t new_value, tbool tot_rp_needed)
{
    int err = 0;
    const char *total_conf_bw_node = "/nkn/nvsd/resource_mgr/total/conf_max_bw";
    const char *glob_conf_bw_node = "/nkn/nvsd/resource_mgr/global_pool/conf_max_bw";
    uint64_t total_conf_bw = 0, global_conf_bw = 0, total_config_bw = 0;
    char global_conf_bw_buf[256] = {0};

    if (tot_rp_needed) {
	err = md_lib_rp_get_config_bw(commit, new_db, &total_config_bw);
	bail_error(err);
	old_value = 0;
	new_value = total_config_bw;
    }
    lc_log_basic(jnpr_log_level, "%s: old- %lu, new-%lu", __FUNCTION__, old_value, new_value );

    /* get current global bandwidth */
    err = mdb_get_node_value_uint64(NULL, new_db,
	    total_conf_bw_node, 0, NULL, &total_conf_bw);
    bail_error(err);

    if (total_conf_bw == 0) {
	goto bail;
    }
    /* get configured global bandwidth */
    err = mdb_get_node_value_uint64(NULL, new_db,
	    glob_conf_bw_node, 0, NULL, &global_conf_bw);
    bail_error(err);
    if ((total_conf_bw + RESERVED_BANDWIDTH)  < (new_value - old_value)) {
	/* don;t set anything */
	goto bail;
    } else {
	global_conf_bw = total_conf_bw - new_value;
    }
    lc_log_basic(jnpr_log_level, "%s: g_conf- %lu", __FUNCTION__, global_conf_bw);

    snprintf(global_conf_bw_buf, sizeof(global_conf_bw_buf), "%lu", global_conf_bw);
    err = mdb_set_node_str(NULL, new_db, bsso_modify, 0, bt_uint64,global_conf_bw_buf,
	    "%s", glob_conf_bw_node);
    bail_error(err);

bail:
    return err;
}

static int
md_nvsd_rm_commit_check(md_commit *commit,
		const mdb_db *old_db,
		const mdb_db *new_db,
		mdb_db_change_array *change_list, void *arg)
{
    int err = 0;
    uint32 num_changes = 0, i = 0, j = 0, gp_init_done = 0;
    const mdb_db_change *change = NULL;
    tstr_array *resource_pool = NULL;
    tstring *t_resource_pool_name = NULL;
    const char *resource_pool_name = NULL;
    tstr_array *t_name_parts = NULL;
    tbool initial = false;
    struct resource_pool_s *rp = NULL;

    num_changes = mdb_db_change_array_length_quick(change_list);

    rp = cp_hashtable_get(t, (void *)GLOBAL_RSRC_POOL);
    if (rp) {
	gp_init_done = 1;
    }
    for(i = 0; i < num_changes; i++) {
	change = mdb_db_change_array_get_quick(change_list, i);
	bail_null(change);

	if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 5, "nkn", "nvsd", "resource_mgr", "config", "*")) &&
		change->mdc_change_type == mdct_delete) {

	    //Dont delee a resource pool associated with namespace
	    resource_pool_name = tstr_array_get_str_quick(change->mdc_name_parts, 4);
	    if (!resource_pool_name) goto bail;

	    err = mdb_get_matching_tstr_array(commit,
		    old_db,
		    "/nkn/nvsd/namespace/*/resource_pool",
		    0,
		    &resource_pool);
	    bail_error(err);

	    if(resource_pool) {
	    } //Dont delete a resource pool which was used values.
	} else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6,
			"nkn", "nvsd", "resource_mgr", "config", "*", "max_bandwidth")
		    || (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6,
			    "nkn", "nvsd", "resource_mgr", "config", "*", "client_sessions")))
		&& (change->mdc_change_type != mdct_delete)) {
	    lc_log_debug(jnpr_log_level, "max-bw, init- %d", gp_init_done);
	    /* check only if global_pool exists */
	    if (gp_init_done) {
		uint64_t old_value = 0, new_value = 0, diff = 0, gp_bandwidth = 0,
			 min_gp_value = 0;
		const char *rsrc_type = tstr_array_get_str_quick(change->mdc_name_parts, 5);
		const char *rsrc = NULL;
		node_name_t node_name = {0};

		/* convert config node name to monitoring node */
		if (0 == strcmp(rsrc_type, "max_bandwidth")) {
		    rsrc = "bandwidth";
		    min_gp_value = RESERVED_BANDWIDTH;
		} else if (0 == strcmp(rsrc_type, "client_sessions")) {
		    rsrc = "net_conn";
		    min_gp_value = RESERVED_NET_CONNS;
		} else {
		    /* should not have happeded */
		    err = 1;
		    goto bail;
		}
		/* get old value */
		err = mdb_get_node_value_uint64(commit,old_db, ts_str(change->mdc_name),
			0, NULL, &old_value);
		bail_error(err);

		/* get new value */
		err = mdb_get_node_value_uint64(commit,new_db, ts_str(change->mdc_name),
			0, NULL, &new_value);
		bail_error(err);
		lc_log_debug(jnpr_log_level, "old- %lu, new- %lu", old_value, new_value);

		if (new_value >  old_value) {
		    resource_pool_name = tstr_array_get_str_quick(change->mdc_name_parts, 4);
		    if (!resource_pool_name) goto bail;

		    snprintf(node_name, sizeof(node_name),
			    "/nkn/nvsd/resource_mgr/monitor/state/global_pool/allowed/%s", rsrc);

		    /* get current global pool bandwidth */
		    err = mdb_get_node_value_uint64(commit,new_db, node_name,
			    0, NULL, &gp_bandwidth);
		    bail_error(err);

		    diff = new_value - old_value;
		    lc_log_debug(jnpr_log_level, "diff- %lu, gp_bw- %lu", gp_bandwidth, diff);
		    if (gp_bandwidth >= (diff + min_gp_value)) {
			/* allowed */
		    } else {
			err = md_commit_set_return_status_msg_fmt(commit, EINVAL,
				"Global Pool doesn't have enough resource");
			bail_error(err);
			goto bail;
		    }
		} else {
		    /* decrease is allowed */
		}
	    } else {
		/* do nothing, could be mgmtd start-up */
	    }
	}
    }

    err = md_commit_set_return_status(commit, 0);
    bail_error(err);

bail:
    tstr_array_free(&t_name_parts);
    tstr_array_free(&resource_pool);
    ts_free(&t_resource_pool_name);
    return(err);
}


int
md_rp_net_conn_update(md_commit *commit, const mdb_db *db, const mdb_db_change* change)
{
    const char *rp_name = NULL;
    struct resource_pool_s *rp = NULL;
    struct resource_pool_s rp_pool;
    int err = 0;
    uint64_t net_conns = 0;

    bail_null(change);

    // /nkn/nvsd/resource_mgr/config/*/max_bandwidth
    rp_name = tstr_array_get_str_quick(change->mdc_name_parts, 4);
    bail_null(rp_name);

    err = mdb_get_node_value_uint64(commit, db, ts_str(change->mdc_name),
	    0, NULL, &net_conns);
    bail_error(err);

    switch(change->mdc_change_type) {
	case mdct_add:
	    rp = cp_hashtable_get(t, (void *)rp_name);
	    if (rp) {
		rp->connection = net_conns;
		rp->config_conn = net_conns;
	    } else {
		bzero(&rp_pool, sizeof(rp_pool));
		strlcpy(rp_pool.name, rp_name, sizeof(rp_pool.name));
		rp_pool.config_conn = net_conns;
		rp_pool.connection = net_conns;
		rp = cp_hashtable_put(t, (void *)rp_name, &rp_pool);
	    }
	    break;

	case mdct_modify:
	    rp = cp_hashtable_get(t, (void *)rp_name);
	    bail_null(rp);
	    rp->connection = net_conns;
	    rp->config_conn = net_conns;
	    break;

	case mdct_delete:
	    rp = cp_hashtable_remove(t, (void *)rp_name);
	    break;

	default:
	    err = 1;
	    bail_error(err);
	    break;
    }

bail:
    return err;
}

int
md_rsrc_pool_update(md_commit *commit, const mdb_db *db, const mdb_db_change* change)
{
    const char *rp_name = NULL;
    struct resource_pool_s *rp = NULL;
    struct resource_pool_s rp_pool;
    int err = 0;
    uint64_t bandwidth = 0;

    bail_null(change);

    // /nkn/nvsd/resource_mgr/config/*/max_bandwidth
    rp_name = tstr_array_get_str_quick(change->mdc_name_parts, 4);
    bail_null(rp_name);

    err = mdb_get_node_value_uint64(commit, db, ts_str(change->mdc_name),
	    0, NULL, &bandwidth);
    bail_error(err);

    switch(change->mdc_change_type) {
	case mdct_add:
	    rp = cp_hashtable_get(t, (void *)rp_name);
	    if (rp) {
		rp->config_bw = bandwidth;
		rp->bandwidth = bandwidth;
	    } else {
		bzero(&rp_pool, sizeof(rp_pool));
		strlcpy(rp_pool.name, rp_name, sizeof(rp_pool.name));
		rp_pool.config_bw = bandwidth;
		rp_pool.bandwidth = bandwidth;
		rp = cp_hashtable_put(t, (void *)rp_name, &rp_pool);
	    }
	    break;

	case mdct_modify:
	    rp = cp_hashtable_get(t, (void *)rp_name);
	    bail_null(rp);
	    rp->config_bw = bandwidth;
	    rp->bandwidth = bandwidth;
	    break;

	case mdct_delete:
	    rp = cp_hashtable_remove(t, (void *)rp_name);
	    break;

	default:
	    err = 1;
	    bail_error(err);
	    break;
    }

bail:
    return err;
}

int
update_all_rsrc_pools(float ratio, uint8_t type, uint64_t *aggr_rp)
{
    int err = 0, i = 0;
    long count = 0;
    char **keys = NULL;
    struct resource_pool_s *rp = NULL, rp_pool;

    count = cp_hashtable_count(t);
    lc_log_debug(jnpr_log_level, "got count - %ld", count);

    *aggr_rp = 0;

    if (count > 0) {
	/* global pool is ready added into hash table */
	keys = (char **)cp_hashtable_get_keys(t);
	if (keys == NULL) {
	    lc_log_debug(jnpr_log_level, "ERROR: No keys");
	    goto bail;
	}

	for (i = 0; i < count; i++) {
	    lc_log_debug(jnpr_log_level, "got key %d %s", i, keys[i]);
	    rp = cp_hashtable_get(t, (void *)keys[i]);
	    bail_null(rp);

	    if (0 == strcmp(keys[i], GLOBAL_RSRC_POOL)) {
		continue;
	    }
	    switch (type) {
		case 1: /* bandwidth */
		    rp->bandwidth = (uint64_t) (rp->config_bw * ratio);
		    lc_log_debug(jnpr_log_level, "cfg- %lu, actual-%lu, ratio - %g",
			    rp->bandwidth, rp->config_bw, ratio);
		    if (rp->bandwidth > rp->config_bw) {
			rp->bandwidth = rp->config_bw;
		    }
		    *aggr_rp += rp->bandwidth;
		    break;

		case 2: /* network connections */
		    rp->connection = (uint64_t) (rp->config_conn * ratio);
		    lc_log_debug(jnpr_log_level, "cfg- %lu, actual-%lu, ratio - %g",
			    rp->config_conn, rp->connection, ratio);
		    if (rp->connection > rp->config_conn) {
			rp->connection = rp->config_conn;
		    }
		    *aggr_rp += rp->connection;
		    break;
	    }
	}
    }

bail:
    return err;
}

int
get_config_rsrc(md_commit *commit, const mdb_db *db, uint8_t type, uint64_t *aggr_rp)
{
    int err = 0, i = 0;
    long count = 0;
    char **keys = NULL;
    struct resource_pool_s *rp = NULL;

    count = cp_hashtable_count(t);
    lc_log_debug(jnpr_log_level, "got count - %ld", count);

    *aggr_rp= 0;

    if (count > 0) {
	/* global pool is ready added into hash table */
	keys = (char **)cp_hashtable_get_keys(t);
	if (keys == NULL) {
	    lc_log_debug(jnpr_log_level, "ERROR: No keys");
	    goto bail;
	}
	for (i = 0; i < count; i++) {
	    if (0 == strcmp(keys[i], GLOBAL_RSRC_POOL)) {
		continue;
	    }
	    rp = cp_hashtable_get(t, (void *)keys[i]);
	    bail_null(rp);
	    lc_log_debug(jnpr_log_level, "got key %d %s, bw- %lu, conn- %lu",
		    i, keys[i], rp->config_bw, rp->config_conn);
	    switch(type) {
		case 1: /* bandwidth */
		    *aggr_rp += rp->config_bw;
		    break;
		case 2:
		    *aggr_rp += rp->config_conn;
		    break;
	    }
	}
    }

    lc_log_debug(jnpr_log_level, "total config rp - %lu", *aggr_rp);

bail:
    return err;
}

int
update_network_conn(md_commit *commit, const mdb_db *db)
{
    int err = 0;
    uint64_t config_total = 0, global_net_conns = 0,
	     actual_rp = 0, allowed_conns = 0;
    uint32_t max_conns = 0;

    float ratio = 1;

    lc_log_debug(jnpr_log_level, "calculate network connections" );

    /* 2 is for bandwidth */
    err = get_config_rsrc(commit, db, 2, &config_total);
    bail_error(err);

    err = mdb_get_node_value_uint32(commit,db,
	    "/nkn/nvsd/network/config/max_connections",0, NULL, &max_conns);
    bail_error(err);

    allowed_conns = max_conns;
    lc_log_debug(jnpr_log_level, "actual - %lu, req= %lu", allowed_conns, config_total);

    if (allowed_conns >= config_total + RESERVED_NET_CONNS) {
	/* we have extra bandwidth, make every body happy */
	global_net_conns = allowed_conns - config_total;
	ratio = 1.0;
	err = update_all_rsrc_pools(ratio, 2, &actual_rp);
    } else {
	/* we have to apply ratio on all resource pools */
	ratio =	(float)allowed_conns/(config_total + RESERVED_NET_CONNS);
	lc_log_debug(jnpr_log_level, "ratio = %g", ratio);

	err = update_all_rsrc_pools(ratio, 2, &actual_rp);
	bail_error(err);

	global_net_conns = allowed_conns - actual_rp;
    }

    lc_log_debug(jnpr_log_level, "act- %lu, act-rp- %lu, glob- %lu",
	    allowed_conns, actual_rp, global_net_conns);

    err = update_global_pool_net_conns(commit,db, global_net_conns);
    system_actual_net_conn = allowed_conns;

    lc_log_debug(jnpr_log_level, "ratio - %g, actual - %lu", ratio, system_actual_net_conn);

    err = md_nvsd_send_rp_update("connection");
    bail_error(err);

bail:
    return err;
}

int
update_bandwidth_ratio(md_commit *commit, const mdb_db *db)
{
    int err = 0;
    uint64_t actual_bw = 0, config_total_bw = 0, global_bw = 0, actual_rp_bw = 0;

    float ratio = 1;

    lc_log_debug(jnpr_log_level, "calculate ratio" );
    err = md_lib_cal_tot_avail_bw(commit, db, &actual_bw , true);
    bail_error(err);

    actual_bw = (actual_bw/10) * (GBYTES/800);
    lc_log_debug(jnpr_log_level, "actual - %lu", actual_bw);

    err = get_config_rsrc(commit, db, 1, &config_total_bw);
    bail_error(err);

    lc_log_debug(jnpr_log_level, "actual - %lu, req= %lu", actual_bw, config_total_bw);
    if (actual_bw >= config_total_bw + RESERVED_BANDWIDTH) {
	/* we have extra bandwidth, make every body happy */
	global_bw = actual_bw - config_total_bw;
	ratio = 1.0;
	err = update_all_rsrc_pools(ratio, 1, &actual_rp_bw);
    } else {
	/* we have to apply ratio on all resource pools */
	ratio =	(float) actual_bw/(config_total_bw + RESERVED_BANDWIDTH);
	lc_log_debug(jnpr_log_level, "ratio = %g", ratio);

	err = update_all_rsrc_pools(ratio, 1,&actual_rp_bw);
	bail_error(err);

	global_bw = actual_bw - actual_rp_bw;
    }

    lc_log_debug(jnpr_log_level, "act- %lu, act-rp- %lu, glob- %lu",
	    actual_bw, actual_rp_bw, global_bw);
    err = update_global_pool_bw(commit,db, global_bw);
    system_actual_bandwidth = actual_bw;

    lc_log_debug(jnpr_log_level, "ratio - %g, actual - %lu", ratio, system_actual_bandwidth);

    err = md_nvsd_send_rp_update("bandwidth");
    bail_error(err);

bail:
    return err;
}


int
update_global_pool_net_conns(md_commit *commit, const mdb_db *db, uint64_t net_conns)
{
    int err = 0;
    struct resource_pool_s rp_pool, *rp = NULL;

    lc_log_debug(jnpr_log_level, "adding global pool with %lu ", net_conns);

    rp = cp_hashtable_get(t, (void *)GLOBAL_RSRC_POOL);
    if (rp) {
	rp->connection = net_conns;
    } else {
	bzero(&rp_pool, sizeof(rp_pool));
	strlcpy(rp_pool.name, GLOBAL_RSRC_POOL, sizeof(rp_pool.name));
	/* config_conns is not valid for global pool */
	rp_pool.connection =  net_conns;
	rp = cp_hashtable_put(t, (void *)GLOBAL_RSRC_POOL, &rp_pool);
	bail_null(rp);
    }

    lc_log_debug(jnpr_log_level, "added global pool - %lu", rp->connection);

bail:
    return err;
}

int
update_global_pool_bw(md_commit *commit, const mdb_db *db, uint64_t bandwidth)
{
    int err = 0;
    struct resource_pool_s rp_pool, *rp = NULL;

    lc_log_debug(jnpr_log_level, "adding global pool with %lu ", bandwidth);

    rp = cp_hashtable_get(t, (void *)GLOBAL_RSRC_POOL);
    if (rp) {
	rp->bandwidth = bandwidth;
    } else {
	bzero(&rp_pool, sizeof(rp_pool));
	strlcpy(rp_pool.name, GLOBAL_RSRC_POOL, sizeof(rp_pool.name));
	rp_pool.bandwidth = bandwidth;
	rp = cp_hashtable_put(t, (void *)GLOBAL_RSRC_POOL, &rp_pool);
	bail_null(rp);
    }

    lc_log_debug(jnpr_log_level, "added global pool - %lu", rp->bandwidth );

bail:
    return err;
}


static int
md_nvsd_rm_commit_apply(md_commit *commit,
		const mdb_db *old_db, const mdb_db *new_db,
		mdb_db_change_array *change_list, void *arg)
{
    int err = 0, update_ratio = 0, update_net_conns = 0;
    uint32 num_changes = 0, i = 0;
    mdb_db_change *change = NULL;
    tbool initial  = false, one_shot = false;
    uint64_t actual_bw = 0;

    err = md_commit_get_commit_mode(commit, &one_shot);
    bail_error(err);
    if (one_shot) {
	/* don't do anything during one-shot mode */
	goto bail;
    }

    num_changes = mdb_db_change_array_length_quick(change_list);
    lc_log_debug(jnpr_log_level, "num_changes - %d",num_changes);
    for(i = 0; i < num_changes; i++) {
	change = mdb_db_change_array_get_quick(change_list, i);
	bail_null(change);

	if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6,
		    "nkn", "nvsd", "resource_mgr", "config", "*", "max_bandwidth")) {

	    lc_log_debug(jnpr_log_level, "got level type - %d", change->mdc_change_type);
	    err = md_rsrc_pool_update(commit, new_db, change);
	    bail_error(err);
	    update_ratio = 1;
	} else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6,
		    "nkn", "nvsd", "resource_mgr", "config", "*", "client_sessions")) {
	    err = md_rp_net_conn_update(commit, new_db, change);
	    bail_error(err);
	    update_net_conns = 1;
	} else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 5,
		    "nkn", "nvsd", "network", "config", "max_connections")) {
	    update_net_conns = 1;
	} else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6,
			"nkn", "nvsd", "http", "config", "interface", "*")
		    || bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6,
			"net", "bonding", "config", "*", "interface", "*"))) {
	    update_ratio = 1;
	}
    }

    /* avoid updating it every time */
    err = md_commit_get_commit_initial(commit, &initial);
    bail_error(err);

    if (initial) {
	update_ratio = 1;
	update_net_conns = 1;
    }

    if (update_ratio) {
	err = update_bandwidth_ratio(commit, new_db);
	bail_error(err);
    }
    if (update_net_conns) {
	err = update_network_conn(commit, new_db);
	bail_error(err);
    }

bail:
    return(err);
}

static int
md_nvsd_rm_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
    int err = 0;

    uint32 i = 0;
    uint32 j = 0;
    char *node_pattern = NULL;
    char *node_esc = NULL;
    char *sample_node = NULL;
    bn_binding *binding = NULL;
    tstr_array *src_nodes = NULL;
    const char *src = NULL;
    uint32 num_src = 0 ;

    err = mdb_create_node_bindings
	(commit, db, md_nvsd_rm_initial_values,
	 sizeof(md_nvsd_rm_initial_values) / sizeof(bn_str_value));
    bail_error(err);


    err = md_nvsd_rm_config_alarm(commit, db,"rp_global_pool_client_sessions",
	    GLOBAL_RSRC_POOL, "client_sessions", 0, 0, 0, 0);
    bail_error(err);

    err = md_nvsd_rm_config_alarm(commit, db,"rp_global_pool_max_bandwidth",
	    GLOBAL_RSRC_POOL, "max_bandwidth", 0, 0, 0, 0);
    bail_error(err);


    /* sample node for resource monitoring per namespace */
    for ( i = 0 ; i < sizeof(sample_cfg_entries)/sizeof(struct sample_config) ; i++)
    {
	if (sample_cfg_entries[i].name != NULL)
	{
	    node_pattern = smprintf("/stats/config/sample/%s", sample_cfg_entries[i].name);
	    err = mdb_set_node_str(commit, db, bsso_create, 0, bt_string,sample_cfg_entries[i].name,
		    "%s",node_pattern);
	    bail_error(err);
	    err = mdb_set_node_str(commit, db, bsso_create, 0, bt_string, sample_cfg_entries[i].label,
		    "/stat/nkn/stats/%s/label", sample_cfg_entries[i].name);
	    bail_error(err);
	    err = mdb_set_node_str(commit, db, bsso_create, 0, bt_string, sample_cfg_entries[i].unit,
		    "/stat/nkn/stats/%s/unit", sample_cfg_entries[i].name);
	    bail_error(err);
	    if(sample_cfg_entries[i].enable != NULL)
	    {
		err = mdb_set_node_str(commit,db,bsso_modify, 0, bt_bool, sample_cfg_entries[i].enable,
			"%s/enable", node_pattern);
		bail_error(err);
	    }
	    /*! support multiple patterns */
	    if (sample_cfg_entries[i].node != NULL)
	    {
		err = ts_tokenize_str(sample_cfg_entries[i].node, ',', '\0', '\0', 0, &src_nodes);
		bail_error_null(err, src_nodes);
		num_src = tstr_array_length_quick(src_nodes);
		for ( j = 0 ; j < num_src ; j++)
		{
		    src = tstr_array_get_str_quick(src_nodes, j);
		    bail_null(src);
		    err = bn_binding_name_escape_str(src, &node_esc);
		    bail_error_null(err,node_esc);

		    err = mdb_set_node_str(commit, db, bsso_create, 0, bt_name,
			    src, "%s/node/%s", node_pattern, node_esc);
		    safe_free(node_esc);
		}
		tstr_array_free(&src_nodes);
	    }
	    sample_node = smprintf("%s/interval",node_pattern);
	    err = bn_binding_new_duration_sec(&binding, sample_node, ba_value, 0, sample_cfg_entries[i].interval);
	    bail_error(err);
	    err = mdb_set_node_binding(commit, db, bsso_modify, 0, binding);
	    bail_error(err);
	    bn_binding_free(&binding);
	    safe_free(sample_node);

	    sample_node = smprintf("%s/num_to_keep", node_pattern);
	    err = bn_binding_new_uint32(&binding, sample_node, ba_value,0, sample_cfg_entries[i].num_to_keep);
	    bail_error(err);
	    err = mdb_set_node_binding(commit,db,bsso_modify,0,binding);
	    bail_error(err);
	    bn_binding_free(&binding);
	    safe_free(sample_node);

	    if(sample_cfg_entries[i].sample_method != NULL)
	    {
		err = mdb_set_node_str(commit,db,bsso_modify,0,bt_string,sample_cfg_entries[i].sample_method,
			"%s/sample_method", node_pattern);
		bail_error(err);
	    }
	    if(sample_cfg_entries[i].delta_constraint != NULL)
	    {
		err = mdb_set_node_str(commit,db,bsso_modify, 0, bt_string,sample_cfg_entries[i].delta_constraint,
			"%s/delta_constraint", node_pattern);
		bail_error(err);
	    }

	    sample_node = smprintf("%s/sync_interval",node_pattern);
	    err = bn_binding_new_uint32(&binding,sample_node,ba_value,0,sample_cfg_entries[i].sync_interval);
	    bail_error(err);
	    err = mdb_set_node_binding(commit,db,bsso_modify,0,binding);
	    bail_error(err);
	    bn_binding_free(&binding);
	    safe_free(sample_node);


	    safe_free(node_pattern);
	}

    }

bail:
    bn_binding_free(&binding);
    safe_free(node_pattern);
    safe_free(node_esc);
    safe_free(sample_node);
    tstr_array_free(&src_nodes);
    return(err);
}

static int
md_nvsd_rm_ns_get_curr_ses(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg)
{
    int err = 0;
    tstring *t_str = NULL;
    char *counter_name = NULL;
    uint64_t n = 0;
    const char *var_name = (const char *) arg;

    bail_null(arg);

    // split the node name and get the namespace name.
    err = bn_binding_name_to_parts_va(node_name, false, 1, 4, &t_str);
    bail_error_null(err, t_str);

    counter_name = smprintf("ns.%s.%s.sessions", ts_str(t_str), var_name);
    bail_null(counter_name);

    // read the counter value
    n = nkncnt_get_uint64((char *)
	    counter_name);
    // create a binding to hold
    // the counter value.
    err =
	bn_binding_new_uint64(ret_binding,
		node_name, ba_value,
		0, n);
    bail_error(err);

bail:
    ts_free(&t_str);
    safe_free(counter_name);
    return err;
}

static int
md_nvsd_rm_ns_get_trans(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg)
{
    int err = 0;
    tstring *t_str = NULL;
    char *counter_name = NULL;
    uint64_t n_tps = 0;
    const char *var_name = (const char *) arg;
    tbool active = false;

    bail_null(arg);

    // split the node name and get the namespace name.
    err = bn_binding_name_to_parts_va(node_name, false, 1, 4, &t_str);
    bail_error_null(err, t_str);

    err = md_nvsd_is_ns_active(ts_str(t_str), db, &active);
    /* ignoring return value */
    if (active == false) {
	/* if namespace is inactive, this counter will absent, so respond with zero */
	err = bn_binding_new_uint64(ret_binding,
		node_name, ba_value,
		0, 0);
	bail_error(err);
	goto bail;
    }

    counter_name = smprintf("ns.%s.%s.client.requests", ts_str(t_str), var_name);
    bail_null(counter_name);

    // read the counter value
    n_tps = nkncnt_get_uint64(counter_name);
    //cntr_cache_get_value(counter_name, 1, 0);

    // create a binding to hold
    // the counter value.

    if(n_tps) {
	err = bn_binding_new_uint64(ret_binding,
		node_name, ba_value,
		0, (n_tps/5));
	bail_error(err);
    } else {
	err = bn_binding_new_uint64(ret_binding,
		node_name, ba_value,
		0, 0);
	bail_error(err);
    }

bail:
    ts_free(&t_str);
    safe_free(counter_name);
    return err;


}

static int
md_nvsd_rm_ns_get_served_bytes(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg)
{
    int err = 0;
    tstring *t_str = NULL;
    char *counter_name = NULL;
    uint64_t n_client_out_bytes = 0, n_server_in_bytes = 0;
    const char *var_name = (const char *) arg;
    tbool active = false;

    bail_null(arg);

    // split the node name and get the namespace name.
    err = bn_binding_name_to_parts_va(node_name, false, 1, 4, &t_str);
    bail_error_null(err, t_str);

    err = md_nvsd_is_ns_active(ts_str(t_str), db, &active);
    /* ignoring return value */
    if (active == false) {
	/* if namespace is inactive, the counters will be absent, so respond with zero */
	err = bn_binding_new_uint64(ret_binding,
		node_name, ba_value,
		0, 0);
	bail_error(err);
	goto bail;
    }
    counter_name = smprintf("ns.%s.%s.client.out_bytes", ts_str(t_str), var_name);
    bail_null(counter_name);

    // read the counter value
    n_client_out_bytes = nkncnt_get_uint64(counter_name);
    //cntr_cache_get_value(counter_name, 1, 0);

    safe_free(counter_name);
    counter_name = smprintf("ns.%s.%s.server.in_bytes", ts_str(t_str), var_name);
    bail_null(counter_name);

    // read the counter value
    n_server_in_bytes = nkncnt_get_uint64(counter_name);
    //cntr_cache_get_value(counter_name, 1, 0);


    // create a binding to hold
    // the counter value.
    err = bn_binding_new_uint64(ret_binding,
	    node_name, ba_value,
	    0, n_client_out_bytes + n_server_in_bytes);
    bail_error(err);

bail:
    ts_free(&t_str);
    safe_free(counter_name);
    return err;
}

static int
md_nvsd_rm_ns_mon1(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg)
{
    int err = 0;
    char *counter_name = NULL;
    uint64_t n = 0;
    const char *var_name = (const char *) arg;

    err = bn_binding_new_uint64(ret_binding,
	    node_name, ba_value,
	    0, 38);
    bail_error(err);

bail:
    safe_free(counter_name);
    return err;
}

static int
md_nvsd_rm_iterate_namespace(md_commit *commit, const mdb_db *db,
                          const char *parent_node_name,
                          const uint32_array *node_attrib_ids,
                          int32 max_num_nodes, int32 start_child_num,
                          const char *get_next_child,
                          mdm_mon_iterate_names_cb_func iterate_cb,
                          void *iterate_cb_arg, void *arg)
{
    int err = 0;
    uint32 i = 0, num_bindings = 0;
    bn_binding *binding = NULL;
    bn_binding_array *binding_arrays = NULL;
    char *name = NULL;

    err = mdb_iterate_binding(commit, db, "/nkn/nvsd/namespace",
	    0, &binding_arrays);
    bail_error(err);

    err = bn_binding_array_length(binding_arrays, &num_bindings);
    bail_error(err);

    for ( i = 0 ; i < num_bindings ; ++i) {
	err = bn_binding_array_get(binding_arrays, i, &binding);
	bail_error(err);

	safe_free(name);

	err = bn_binding_get_str(binding, ba_value, bt_string, NULL,
		&name);

	err = (*iterate_cb)(commit, db, name, iterate_cb_arg);
	bail_error(err);
	safe_free(name);
    }


bail:
    bn_binding_array_free(&binding_arrays);
    return(err);
}

	static int
md_nvsd_rm_iterate_rp(md_commit *commit, const mdb_db *db,
                          const char *parent_node_name,
                          const uint32_array *node_attrib_ids,
                          int32 max_num_nodes, int32 start_child_num,
                          const char *get_next_child,
                          mdm_mon_iterate_names_cb_func iterate_cb,
                          void *iterate_cb_arg, void *arg)
{
    int err = 0;
    uint32 i = 0, num_bindings = 0;
    bn_binding *binding = NULL;
    bn_binding_array *binding_arrays = NULL;
    char *name = NULL;

    err = mdb_iterate_binding(commit, db, "/nkn/nvsd/resource_mgr/config",
	    0, &binding_arrays);
    bail_error(err);

    err = bn_binding_array_length(binding_arrays, &num_bindings);
    bail_error(err);

    for ( i = 0 ; i < num_bindings ; ++i) {
	err = bn_binding_array_get(binding_arrays, i, &binding);
	bail_error(err);

	safe_free(name);

	err = bn_binding_get_str(binding, ba_value, bt_string, NULL,
		&name);

	err = (*iterate_cb)(commit, db, name, iterate_cb_arg);
	bail_error(err);
	safe_free(name);
    }
    /* Add Global pool here */
    err = (*iterate_cb)(commit, db, GLOBAL_RSRC_POOL, iterate_cb_arg);
    bail_error(err);

bail:
    bn_binding_array_free(&binding_arrays);
    return(err);
}


static int
md_nvsd_rm_get_namespace(md_commit *commit, const mdb_db *db,
                      const char *node_name,
                      const bn_attrib_array *node_attribs,
                      bn_binding **ret_binding,
                      uint32 *ret_node_flags, void *arg)
{
    int err = 0;
    tstr_array *parts = NULL;
    uint32 num_parts = 0;
    const char *namespace = NULL;

    err = bn_binding_name_to_parts(node_name, &parts, NULL);
    bail_error_null(err, parts);

    num_parts = tstr_array_length_quick(parts);
 //   bail_require(num_parts == 4); /* "/stat/nkn/namespace/<namespace>"  */
    namespace = tstr_array_get_str_quick(parts, 4);
    bail_null(namespace);

    /* XXX/EMT validate */

    err = bn_binding_new_str(ret_binding, node_name, ba_value, bt_string,
                             0, namespace);
    bail_error_null(err, *ret_binding);

 bail:
    tstr_array_free(&parts);
    return(err);
}

static int
md_nvsd_rm_get_rp(md_commit *commit, const mdb_db *db,
                      const char *node_name,
                      const bn_attrib_array *node_attribs,
                      bn_binding **ret_binding,
                      uint32 *ret_node_flags, void *arg)
{
    int err = 0;
    tstr_array *parts = NULL;
    uint32 num_parts = 0;
    const char *resource_pool = NULL;

    err = bn_binding_name_to_parts(node_name, &parts, NULL);
    bail_error_null(err, parts);

    num_parts = tstr_array_length_quick(parts);
    //   bail_require(num_parts == 4); /* "/stat/nkn/namespace/<namespace>"  */

    /* should try to use the args to determine instead */
    if(strstr(node_name, "/monitor/external")
	    || strstr(node_name, "/monitor/counter")) {
	resource_pool = tstr_array_get_str_quick(parts, 5);
	bail_null(resource_pool);

    } else {
	resource_pool = tstr_array_get_str_quick(parts, 4);
	bail_null(resource_pool);
    }
    /* XXX/EMT validate */

    err = bn_binding_new_str(ret_binding, node_name, ba_value, bt_string,
	    0, resource_pool);
    bail_error_null(err, *ret_binding);

bail:
    tstr_array_free(&parts);
    return(err);
}


static int
md_nvsd_rm_ns_get_cache_hit_req(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg)
{
    int err = 0;
    tstring *t_str = NULL;
    char *counter_name = NULL;
    uint64_t n_cache_hit = 0, n_cache_miss = 0;
    const char *var_name = (const char *) arg;

    bail_null(arg);

    // split the node name and get the namespace name.
    err = bn_binding_name_to_parts_va(node_name, false, 1, 4, &t_str);
    bail_error_null(err, t_str);

    counter_name = smprintf("ns.%s.%s.client.cache_hits", ts_str(t_str), var_name);
    bail_null(counter_name);

    // read the counter value
    n_cache_hit = nkncnt_get_uint64((char *)counter_name);

    counter_name = smprintf("ns.%s.%s.client.cache_miss", ts_str(t_str), var_name);
    bail_null(counter_name);

    // read the counter value
    n_cache_miss = nkncnt_get_uint64((char *)counter_name);


    // create a binding to hold
    // the counter value.
    if(n_cache_miss || n_cache_hit) {
	err = bn_binding_new_uint64(ret_binding,
		node_name, ba_value,
		0, (((double)n_cache_hit / ( n_cache_hit + n_cache_miss))* 100) );
	bail_error(err);
    } else {
	err = bn_binding_new_uint64(ret_binding,
		node_name, ba_value,
		0, 0);
	bail_error(err);
    }
bail:
    ts_free(&t_str);
    safe_free(counter_name);
    return err;
}

static int
md_nvsd_rm_ns_get_cache_hit_bytes(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg)
{
    int err = 0;
    tstring *t_str = NULL;
    char *counter_name = NULL;
    uint64_t n_bytes_ram = 0, n_bytes_origin = 0, n_bytes_disk = 0, n_bytes_tunnel = 0;
    const char *var_name = (const char *) arg;

    bail_null(arg);

    // split the node name and get the namespace name.
    err = bn_binding_name_to_parts_va(node_name, false, 1, 4, &t_str);
    bail_error_null(err, t_str);

    counter_name = smprintf("ns.%s.%s.client.out_bytes_ram", ts_str(t_str), var_name);
    bail_null(counter_name);

    // read the counter value
    n_bytes_ram = nkncnt_get_uint64((char *)counter_name);

    counter_name = smprintf("ns.%s.%s.client.out_bytes_disk", ts_str(t_str), var_name);
    bail_null(counter_name);

    // read the counter value
    n_bytes_disk = nkncnt_get_uint64((char *)counter_name);

    counter_name = smprintf("ns.%s.%s.client.out_bytes_origin", ts_str(t_str), var_name);
    bail_null(counter_name);

    // read the counter value
    n_bytes_origin = nkncnt_get_uint64((char *)counter_name);

    counter_name = smprintf("ns.%s.%s.client.out_bytes_tunnel", ts_str(t_str), var_name);
    bail_null(counter_name);

    // read the counter value
    n_bytes_tunnel = nkncnt_get_uint64((char *)counter_name);

    // create a binding to hold
    // the counter value.
    if(n_bytes_origin || n_bytes_ram || n_bytes_disk || n_bytes_tunnel) {
	err = bn_binding_new_uint64(ret_binding,
		node_name, ba_value,
		0, (((double)(n_bytes_ram + n_bytes_disk) / ( n_bytes_ram + n_bytes_disk + n_bytes_origin + n_bytes_tunnel))*100) );
	bail_error(err);
    } else {
	err = bn_binding_new_uint64(ret_binding,
		node_name, ba_value,
		0, 0);
	bail_error(err);
    }
bail:
    ts_free(&t_str);
    safe_free(counter_name);
    return err;
}


static int
md_nvsd_rm_ns_get_tier_stats(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg)
{
    int err = 0;
    int i = 0;
    uint64_t prov_type = (uint64_t) arg;
    node_name_t uid = {0};
    tstring *t_ns = NULL;
    tbool found = false;
    tstring *t_uid = NULL;
    counter_name_t cntr_name = {0};
    uint64_t obj_cnt_ns = 0;

    // split the node name and get the namespace name.
    err = bn_binding_name_to_parts_va(node_name, false, 1, 4, &t_ns);
    bail_error_null(err, t_ns);

    snprintf(uid, sizeof(uid), "/nkn/nvsd/namespace/%s/uid", ts_str(t_ns));

    err = mdb_get_node_value_tstr(commit, db, uid, 0, &found, &t_uid);
    bail_error_null(err, t_uid);

    //TODO: need to get a counter represting the number of slots,
    //so that we need not hardcode the value of 64.

    for(i = 1; i <= 64; i++) {

	/*Get the provider type
	 * if it doesn't match with what node wants
	 * Just continue
	 */
	uint64_t provider_type = 0;
	snprintf(cntr_name, sizeof(cntr_name), "dc_%d.dm2_provider_type", i);
	provider_type = nkncnt_get_uint64(cntr_name);

	if(prov_type != provider_type)
	    continue;

	snprintf(cntr_name, sizeof(cntr_name), "ns.%s.cache.disk.dc_%d.total_objects",
		(ts_str(t_uid))+1, i);

	obj_cnt_ns += nkncnt_get_uint64(cntr_name);


    }

    err = bn_binding_new_uint64(ret_binding, node_name, ba_value, 0, obj_cnt_ns);
    bail_error(err);

bail:
    ts_free(&t_ns);
    ts_free(&t_uid);
    return err;

}

static int
md_nvsd_rm_ns_get_disk_stats(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg)
{
    int err = 0;
    int i = 0;
    const char *var_name = (const char *) arg;
    node_name_t uid = {0};
    tstring *t_ns = NULL;
    tbool found = false;
    tstring *t_uid = NULL;
    str_value_t obj_cnt = {0};
    uint64_t obj_cnt_ns = 0;

    // split the node name and get the namespace name.
    err = bn_binding_name_to_parts_va(node_name, false, 1, 4, &t_ns);
    bail_error_null(err, t_ns);

    snprintf(uid, sizeof(uid), "/nkn/nvsd/namespace/%s/uid", ts_str(t_ns));

    err = mdb_get_node_value_tstr(commit, db, uid, 0, &found, &t_uid);
    bail_error_null(err, t_uid);

    for(i = 1; i <= 25; i++) {

	snprintf(obj_cnt, sizeof(obj_cnt), "ns.%s.cache.disk.dc_%d.%s",
		(ts_str(t_uid))+1, i, var_name);

	obj_cnt_ns += nkncnt_get_uint64(obj_cnt);


    }

    err = bn_binding_new_uint64(ret_binding, node_name, ba_value, 0, obj_cnt_ns);
    bail_error(err);

bail:
    ts_free(&t_ns);
    ts_free(&t_uid);
    return err;

}


static int
md_nvsd_rm_ns_get_curr_bw(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg)
{
    int err = 0;
    const char *var_name = (const char *) arg;
    tstring *t_str = NULL;
    uint64 bw = 0;
    uint64 bw_mbps = 0;
    char *bname = NULL;
    tbool found = false;
    tstring *cval = NULL;

    bail_null(arg);

    // split the node name and get the namespace name.
    err = bn_binding_name_to_parts_va(node_name, false, 1, 4, &t_str);
    bail_error_null(err, t_str);

    bname =	smprintf("/stats/state/sample/ns_served_bytes/node/\\/nkn\\/nvsd\\/resource_mgr\\/namespace\\/%s\\/%s\\/served_bytes/last/value",
	    ts_str(t_str),var_name);
    bail_null(bname);

    err = mdb_get_node_value_tstr(commit, db, bname, 0, &found, &cval);
    bail_error(err);

    if(cval) {
	sscanf(ts_str(cval),"%lu",&bw);
	bw_mbps = ((bw/5) * 8);
    }
    err = bn_binding_new_uint64(ret_binding, node_name, ba_value, 0, bw_mbps);
    bail_error(err);


bail:
    safe_free(bname);
    ts_free(&cval);
    ts_free(&t_str);
    return err;
}

static int
md_nvsd_rm_get_uesd_percentage(
		md_commit *commit,
		const mdb_db *db,
		const char *mon_node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg)
{
    int err = 0;
    uint64_t counter_value = 0, config_value = 0;
    uint16_t percentage = 0;
    enum resource_type rtype = (enum resource_type) arg;
    tstring *t_str_rpname = NULL;
    node_name_t counter_name = {0}, node_name = {0};

    /* get resource-pool name from */
    // "/nkn/nvsd/resource_mgr/monitor/counter\/*\/used/p_age/client_sessions";
    err = bn_binding_name_to_parts_va(mon_node_name, false, 1, 5, &t_str_rpname);
    bail_error_null(err, t_str_rpname);

    /* get used values */
    switch (rtype) {
	case e_rp_client_sessions:
	    snprintf(counter_name, sizeof(counter_name),
		    "rp.%s.client.sessions_used", ts_str(t_str_rpname));
	    snprintf(node_name, sizeof(node_name),
		    "/nkn/nvsd/resource_mgr/monitor/state/%s/allowed/net_conn",
		    ts_str(t_str_rpname));
	    break;

	case e_rp_client_bandwidth:
	    snprintf(counter_name, sizeof(counter_name),
		    "rp.%s.client.bw_used", ts_str(t_str_rpname));
	    snprintf(node_name, sizeof(node_name),
		    "/nkn/nvsd/resource_mgr/monitor/state/%s/allowed/bandwidth",
		    ts_str(t_str_rpname));
	    break;
	default:
	    err = 1;
	    goto bail;
    }

    /* read the counter value from nvsd */
    counter_value = nkncnt_get_uint64(counter_name);


    /* get configured value */
    err = mdb_get_node_value_uint64(commit, db,
	    node_name,0, NULL, &config_value);
    bail_error(err);

    lc_log_debug(jnpr_log_level, "rp=> %s, cfg=> %ld, curr=> %ld",
	    ts_str(t_str_rpname), config_value, counter_value);

    /* NOTE: multiplying first, then dividing */
    if (config_value) {
	percentage = (uint16_t)((counter_value * 100)/config_value );
    } else {
	percentage = 0;
    }

    err = bn_binding_new_uint16(ret_binding,
	    mon_node_name, ba_value, 0, percentage);
    bail_error(err);

bail:
    ts_free(&t_str_rpname);
    return err;
}

static int
md_nvsd_rm_get_counter_mbps(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg)
{
    int err = 0;
    tstring *t_str_rpname = NULL;
    tstring *t_str_rptype = NULL;
    counter_name_t counter_name = {0};
    uint64_t n_counter = 0;
    const char *var_name = (const char *) arg;

    bail_require(arg);

    // /nkn/nvsd/resource_mgr/monitor/counter/*/used/client_sessions
    // split the node name and get the namespace name.
    err = bn_binding_name_to_parts_va(node_name, false, 1, 5, &t_str_rpname);
    bail_error_null(err, t_str_rpname);

    err = bn_binding_name_to_parts_va(node_name, false, 1, 6, &t_str_rptype);
    bail_error_null(err, t_str_rptype);

    snprintf(counter_name, sizeof(counter_name),
	    "rp.%s.%s_%s", ts_str(t_str_rpname), var_name, ts_str(t_str_rptype));

    // read the counter value
    n_counter = (nkncnt_get_uint64((char *)counter_name)) / 125000;

    // create a binding to hold
    // the counter value.
    err = bn_binding_new_uint64(ret_binding,
	    node_name, ba_value,
	    0, n_counter);
    bail_error(err);

bail:
    ts_free(&t_str_rpname);
    ts_free(&t_str_rptype);
    return err;
}

static int
md_nvsd_rm_get_counter(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg)
{
    int err = 0;
    tstring *t_str_rpname = NULL;
    tstring *t_str_rptype = NULL;
    char *counter_name = NULL;
    uint64_t n_counter = 0;
    const char *var_name = (const char *) arg;

    bail_null(arg);

    // /nkn/nvsd/resource_mgr/monitor/counter/*/used/client_sessions
    // split the node name and get the namespace name.
    err = bn_binding_name_to_parts_va(node_name, false, 1, 5, &t_str_rpname);
    bail_error_null(err, t_str_rpname);

    err = bn_binding_name_to_parts_va(node_name, false, 1, 6, &t_str_rptype);
    bail_error_null(err, t_str_rptype);

    counter_name = smprintf("rp.%s.%s_%s", ts_str(t_str_rpname), var_name, ts_str(t_str_rptype));
    bail_null(counter_name);

    // read the counter value
    n_counter = nkncnt_get_uint64((char *)counter_name);

    // create a binding to hold
    // the counter value.
    err = bn_binding_new_uint64(ret_binding,
	    node_name, ba_value,
	    0, n_counter);
    bail_error(err);

bail:
    ts_free(&t_str_rpname);
    ts_free(&t_str_rptype);
    safe_free(counter_name);
    return err;


}
#if 1
static int
md_nvsd_rm_ns_get_transactions(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg)
{
    int err = 0;
    tstring *t_str = NULL;
    tstring *cval = NULL;
    uint64 trans = 0;
    uint64_t n_req = 0;
    char *bname = NULL;
    tbool found = false;
    const char *var_name = (const char *) arg;

    bail_null(arg);

    // split the node name and get the namespace name.
    err = bn_binding_name_to_parts_va(node_name, false, 1, 4, &t_str);
    bail_error_null(err, t_str);
    bname =	smprintf("/stats/state/sample/ns_transactions/node/\\/nkn\\/nvsd\\/resource_mgr\\/namespace\\/%s\\/%s\\/transactions/last/value",
	    ts_str(t_str),var_name);


    err = mdb_get_node_value_tstr(commit, db, bname, 0, &found, &cval);
    bail_error(err);

    if(cval) {
	sscanf(ts_str(cval),"%lu",&trans);
    }

    err = bn_binding_new_uint64(ret_binding, node_name, ba_value, 0, trans/5);
    bail_error(err);


bail:
    safe_free(bname);
    ts_free(&t_str);
    ts_free(&cval);
    return err;

}
#endif

static int
md_nvsd_rm_ns_get_tier_size(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg)
{
    int err = 0;
    const char *var_name = (const char *) arg;
    uint64 tier_size = 0;

    bail_null(arg);

    err = md_nvsd_rm_get_tier_size(var_name, &tier_size, commit, db);
    bail_error(err);

    if(tier_size) {
	err = bn_binding_new_uint64(ret_binding,
		node_name, ba_value,
		0, tier_size);
	bail_error(err);
    }
bail:
    return err;
}

static int
md_nvsd_rm_ns_get_cache_util(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg)
{
    int err = 0;
    tstring *t_str = NULL;
    char *counter_name = NULL;
    uint64_t n_req = 0;
    const char *var_name = (const char *) arg;
    uint64 tier_size = 0;
    uint64 n_block_size = 0;
    uint64 n_blocks = 0;
    uint64 ns_tier_util = 0;

    bail_null(arg);

    // split the node name and get the namespace name.
    err = bn_binding_name_to_parts_va(node_name, false, 1, 4, &t_str);
    bail_error_null(err, t_str);

    counter_name = smprintf("ns.%s.cache.%s.block_used", ts_str(t_str), var_name);
    bail_null(counter_name);
    n_blocks = nkncnt_get_uint64((char *)counter_name);
    safe_free(counter_name);

    counter_name = smprintf("ns.%s.cache.%s.block_size", ts_str(t_str), var_name);
    bail_null(counter_name);
    n_block_size = nkncnt_get_uint64((char *)counter_name);
    safe_free(counter_name);

    // create a binding to hold
    // the counter value.
    err = md_nvsd_rm_get_tier_size(var_name, &tier_size, commit, db);
    bail_error(err);

    ns_tier_util = n_block_size * n_blocks;

    if(tier_size) {
	err = bn_binding_new_uint64(ret_binding,
		node_name, ba_value,
		0, (((double)(ns_tier_util))/tier_size) * 100);
    } else {
	err = bn_binding_new_uint64(ret_binding,
		node_name, ba_value,
		0, 0);
    }

    bail_error(err);
bail:
    ts_free(&t_str);
    safe_free(counter_name);
    return err;
}

static int
md_nvsd_rm_get_tier_size(const char* tier,
			    uint64 *ptiersize,
			    md_commit *commit,
			    const  mdb_db *db)
{
    int err = 0;
    tstr_array *disk_name_list = NULL;
    uint32 num_disks = 0, idx = 0;
    tstring *t_diskid = NULL;
    char *cp_diskname = NULL;
    char *node_name = NULL;
    tstring *t_tier = NULL;
    uint32 block_size = 0;
    uint32 blocks = 0;
    uint64 disk_size = 0;
    uint64 tier_size = 0;
    bn_binding *binding = NULL;

    err = md_nvsd_resourece_mgr_get_disk(commit, db, &disk_name_list);
    bail_error(err);

    bail_error_null(err, disk_name_list);

    err = tstr_array_length(disk_name_list, &num_disks);
    bail_error(err);

    // Go through all the configured disks to see if we get the  to the desired tier
    for (idx = 0; idx < num_disks; idx++)
    {
	cp_diskname = smprintf("dc_%d",(idx + 1));
	bail_null(cp_diskname);

	err = mdb_get_node_binding_fmt(commit, db, 0 , &binding,
		"/nkn/nvsd/diskcache/monitor/diskname/%s/disk_id",
		cp_diskname);
	bail_error(err);
	if (binding) {
	    err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL,
		    &t_diskid);
	    bail_error_null(err, t_diskid);
	    bn_binding_free(&binding);
	}

	err = mdb_get_node_binding_fmt(commit, db, 0 , &binding,
		"/nkn/nvsd/diskcache/monitor/disk_id/%s/tier",
		ts_str(t_diskid));
	bail_error(err);
	if (binding) {
	    err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL,
		    &t_tier);
	    bail_error_null(err, t_tier);
	    bn_binding_free(&binding);
	}

	if(ts_equal_str(t_tier, tier, false)) {

	    err = mdb_get_node_binding_fmt(commit, db, 0 , &binding,
		    "/stat/nkn/disk/%s/block_size",
		    cp_diskname);
	    bail_error(err);
	    if (binding) {
		err = bn_binding_get_uint32(binding, ba_value, NULL, &block_size);
		bail_error(err);
		bn_binding_free(&binding);
	    }


	    err = mdb_get_node_binding_fmt(commit, db, 0 , &binding,
		    "/stat/nkn/disk/%s/total_blocks",
		    cp_diskname);
	    bail_error(err);
	    if (binding) {
		err = bn_binding_get_uint32(binding, ba_value, NULL, &blocks);
		bail_error(err);
		bn_binding_free(&binding);
	    }

	    disk_size = (uint64_t)blocks * block_size;


	}
	tier_size += disk_size;

    }
    *ptiersize = tier_size;
bail:
    safe_free(node_name);
    safe_free(cp_diskname);
    ts_free(&t_tier);
    ts_free(&t_diskid);
    return err;

}

static int
md_nvsd_resourece_mgr_get_disk(
    md_commit * commit,
    const mdb_db *db,
    tstr_array **ret_labels)
{
    int err = 0;
    tstr_array *labels = NULL, *del_labels = NULL;
    tstr_array_options opts;
    tstr_array *labels_config = NULL;
    uint32 i = 0, num_labels = 0;
    tstring *t_diskid = NULL;
    const char *t_diskid_path = NULL;
    tbool display = false;
    const char *label = NULL;


    tstr_array *lbl_cfg = NULL;

    bail_null(ret_labels);
    *ret_labels = NULL;

    err = tstr_array_options_get_defaults(&opts);
    bail_error(err);

    opts.tao_dup_policy = adp_delete_old;

    err = tstr_array_new(&labels, &opts);
    bail_error(err);

    err = mdb_get_matching_tstr_array(commit, db, "/nkn/nvsd/diskcache/config/disk_id/*",
	    0, &labels_config);
    bail_error_null(err, labels_config);


    /* For each disk_id get the Disk Name */
    i = 0;
    t_diskid_path = tstr_array_get_str_quick (labels_config, i++);
    if(t_diskid_path) {
	err =  lf_path_last(t_diskid_path,&t_diskid);
	bail_error(err);
    }
    while (NULL != t_diskid)
    {
	char *node_name = NULL;
	tstring *t_diskname = NULL;

	node_name = smprintf ("/nkn/nvsd/diskcache/monitor/disk_id/%s/diskname", ts_str(t_diskid));
	ts_free(&t_diskid);
	t_diskid = NULL;
	/* Now get the disk_name */
	err = mdb_get_node_value_tstr(commit, db, node_name, 0, NULL, &t_diskname);
	bail_error(err);

	//err = tstr_array_append_str(labels, ts_str(t_diskname));
	err = tstr_array_insert_str_sorted(labels, ts_str(t_diskname));
	bail_error(err);

	t_diskid_path = tstr_array_get_str_quick (labels_config, i++);
	if(t_diskid_path) {
	    err =  lf_path_last(t_diskid_path,&t_diskid);
	    bail_error(err);
	}
    }

    *ret_labels = labels;
    labels = NULL;
bail:
    ts_free(&t_diskid);
    tstr_array_free(&labels_config);
    return err;

}

static int
md_nvsd_rm_ns_get_test(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg)
{
    int err = 0;

    bail_null(arg);

    // split the node name and get the namespace name.

    err = bn_binding_new_uint64(ret_binding,
	    node_name, ba_value,
	    0, 21);

    bail_error(err);
bail:
    return err;
}

static int
md_nvsd_rm_adjust_alarm(
		md_commit *commit,
		mdb_db *db,
		const char *alarm_name,
		const char *rp_name,
		const char *rp_type,
		uint64_t rising_err,
		uint64_t rising_clear,
		uint64_t falling_err,
		uint64_t falling_clear)
{
    int err = 0;
    char *node_pattern = NULL;
    char *str_trigger_percent = NULL;

    if (rp_name != NULL &&
	    ((strcmp(rp_type,"client_sessions") == 0) ||
	     (strcmp(rp_type,"max_bandwidth") == 0))){
	node_pattern = smprintf("/stats/config/alarm/%s",alarm_name);

	str_trigger_percent = smprintf("%ld", rising_err);
	bail_null(str_trigger_percent);

	err = mdb_set_node_str
	    (commit,db,bsso_modify,0,bt_uint64,str_trigger_percent,"%s/rising/error_threshold",node_pattern);
	bail_error(err);

	safe_free(str_trigger_percent);
	str_trigger_percent = smprintf("%ld", falling_err);
	bail_null(str_trigger_percent);

	err = mdb_set_node_str
	    (commit,db,bsso_modify,0,bt_uint64,str_trigger_percent,"%s/falling/error_threshold",node_pattern);
	bail_error(err);

	safe_free(str_trigger_percent);
	str_trigger_percent = smprintf("%ld", rising_clear);
	bail_null(str_trigger_percent);

	err = mdb_set_node_str
	    (commit,db,bsso_modify,0,bt_uint64,str_trigger_percent,"%s/rising/clear_threshold",node_pattern);
	bail_error(err);

	safe_free(str_trigger_percent);
	str_trigger_percent = smprintf("%ld", falling_clear);
	bail_null(str_trigger_percent);

	err = mdb_set_node_str(
		commit,db,bsso_modify,0,bt_uint64, str_trigger_percent ,"%s/falling/clear_threshold",node_pattern);
	bail_error(err);
    }
bail:
    safe_free(str_trigger_percent);
    safe_free(node_pattern);
    return err;
}

static int
md_nvsd_rm_config_alarm(
		md_commit *commit,
		mdb_db *db,
		const char *alarm_name,
		const char *rp_name,
		const char *rp_type,
		uint64_t rising_err,
		uint64_t rising_clear,
		uint64_t falling_err,
		uint64_t falling_clear)
{
    int err = 0;
    char *node_pattern = NULL;
    char *str_trigger_percent = NULL;
    char *rp_node_pattern = NULL;
    bn_binding *binding = NULL;

    bail_null(rp_type);

    if (rp_name != NULL &&
	    ((strcmp(rp_type,"client_sessions") == 0) ||
	     (strcmp(rp_type,"max_bandwidth") == 0))){

	node_pattern = smprintf("/stats/config/alarm/%s",alarm_name);

	err = mdb_get_node_binding_fmt(commit, db, 0, &binding, "%s",node_pattern);
	bail_error(err);
	if(!binding){
	    err = mdb_set_node_str
		(commit,db,bsso_create,0,bt_string,alarm_name,"%s",node_pattern);
	    bail_error(err);

	    err = mdb_set_node_str
		(commit,db,bsso_modify,0,bt_bool,"true","%s/enable",node_pattern);
	    bail_error(err);

	    err = mdb_set_node_str
		(commit,db,bsso_modify,0,bt_bool,"true","%s/disable_allowed",node_pattern);
	    bail_error(err);

	    err = mdb_set_node_str
		(commit,db,bsso_modify,0,bt_bool,"true","%s/ignore_first_value",node_pattern);
	    bail_error(err);

	    err = mdb_set_node_str
		(commit,db,bsso_modify,0,bt_string,"sample","%s/trigger/type",node_pattern);
	    bail_error(err);

	    err = mdb_set_node_str
		(commit,db,bsso_modify,0,bt_string,"resource_pool","%s/trigger/id",node_pattern);
	    bail_error(err);

	    rp_node_pattern = smprintf("/nkn/nvsd/resource_mgr/monitor/state/%s/used/p_age/%s",rp_name, rp_type);
	    bail_null(rp_node_pattern);

	    err = mdb_set_node_str
		(commit,db,bsso_modify,0,bt_name,rp_node_pattern,"%s/trigger/node_pattern",node_pattern);
	    bail_error(err);

	    err = mdb_set_node_str
		(commit,db,bsso_modify,0,bt_bool,"true","%s/rising/enable",node_pattern);
	    bail_error(err);

	    err = mdb_set_node_str
		(commit,db,bsso_modify,0,bt_bool,"true","%s/falling/enable",node_pattern);
	    bail_error(err);

	    err = mdb_set_node_str
		(commit,db,bsso_modify,0,bt_uint64, "80","%s/rising/error_threshold",node_pattern);
	    bail_error(err);

	    err = mdb_set_node_str
		(commit,db,bsso_modify,0,bt_uint64, "20","%s/falling/error_threshold",node_pattern);
	    bail_error(err);

	    err = mdb_set_node_str
		(commit,db,bsso_modify,0,bt_uint64, "75","%s/rising/clear_threshold",node_pattern);
	    bail_error(err);

	    err = mdb_set_node_str(
		    commit,db,bsso_modify,0,bt_uint64, "25","%s/falling/clear_threshold",node_pattern);
	    bail_error(err);

	    err = mdb_set_node_str
		(commit,db,bsso_modify,0,bt_bool,"true","%s/rising/event_on_clear",node_pattern);
	    bail_error(err);

	    err = mdb_set_node_str
		(commit,db,bsso_modify,0,bt_bool,"true","%s/falling/event_on_clear",node_pattern);
	    bail_error(err);

	    //Hope this works
	    err = mdb_set_node_str
		(commit,db,bsso_modify,0,bt_string,"resource_pool_event","%s/event_name_root",node_pattern);
	    bail_error(err);

	    err = mdb_set_node_str
		(commit,db,bsso_modify,0,bt_bool,"true","%s/clear_if_missing",node_pattern);
	    bail_error(err);
	}
    }

bail:
    safe_free(rp_node_pattern);
    safe_free(node_pattern);
    bn_binding_free(&binding);
    return err;
}
static int
md_nvsd_is_ns_active(const char *namespace, const mdb_db *db, tbool *active)
{
    int err = 0;
    tbool is_active = false;
    char buf[256] = { 0 };

    bail_null(namespace);
    bail_null(active);

    snprintf(buf, sizeof(buf), "/nkn/nvsd/namespace/%s/status/active", namespace);

    err = mdb_get_node_value_tbool(NULL, db, buf, 0, NULL, &is_active);
    bail_error(err);

bail:
    *active = is_active;
    return err;
}

int
md_lib_send_action_from_module(md_commit *commit, const char *action_name)
{
	return 0;
}

static int
md_nvsd_rm_get_rp_name(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg)
{
    int err = 0;
    tstring *t_rpname = NULL;
    struct resource_pool_s *rp = NULL;
    uint64_t value = 0;

    lc_log_debug(jnpr_log_level, "node - %s", node_name);

    /* node name, resource pool name */
    // "/nkn/nvsd/resource_mgr/monitor/state/<resource pool name>"
    err = bn_binding_name_to_parts_va(node_name, false, 1, 5, &t_rpname);
    bail_error_null(err, t_rpname);

    err = bn_binding_new_str(ret_binding, node_name, ba_value, bt_string,
	    0, ts_str(t_rpname));
    bail_error_null(err, *ret_binding);

bail:
    ts_free(&t_rpname);
    return err;
}

static int
md_nvsd_rm_get_system( md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg)
{
    int err = 0;
    uint32_t rsrc_type = atoi((char *)arg);
    uint64_t value = 0;

    lc_log_debug(jnpr_log_level, "node - %s", node_name);

    switch (rsrc_type) {
	case 1: /* bandwidth */
	    value = system_actual_bandwidth;
	    break;

	case 2: /* network connction */
	    value = system_actual_net_conn;
	    break;

	default:
	    err = 1;
	    bail_error(err);
	    break;
    }
    lc_log_debug(jnpr_log_level, "tye - %d, value = %lu", rsrc_type, value);
    err = bn_binding_new_uint64(ret_binding, node_name, ba_value, 0, value);
    bail_error(err);

bail:
    return err;
}

static int
md_nvsd_rm_get_allowed(
		md_commit *commit,
		const mdb_db *db,
		const char *node_name,
		const bn_attrib_array *node_attribs,
		bn_binding **ret_binding,
		uint32 *ret_node_flags,
		void *arg)
{
    int err = 0;
    uint32_t rsrc_type = atoi((char *)arg);
    tstring *t_rpname = NULL;
    struct resource_pool_s *rp = NULL;
    uint64_t value = 0;

    lc_log_debug(jnpr_log_level, "node - %s", node_name);
    /* node name, resource pool name */
    // "/nkn/nvsd/resource_mgr/monitor/state/*/allowed/bandwidth"
    err = bn_binding_name_to_parts_va(node_name, false, 1, 5, &t_rpname);
    bail_error_null(err, t_rpname);

    switch (rsrc_type) {
	case 1: /* bandwidth */
	    rp = cp_hashtable_get(t, (void *)ts_str(t_rpname));
	    if (rp == NULL) {
		value = 0;
	    } else {
		lc_log_debug(jnpr_log_level, "bw - %lu",rp->bandwidth);
		value = rp->bandwidth;
	    }
	    break;

	case 2: /* network connection */
	    rp = cp_hashtable_get(t, (void *)ts_str(t_rpname));
	    if (rp == NULL) {
		value = 0;
	    } else {
		lc_log_debug(jnpr_log_level, "net_conn - %lu",rp->connection);
		value = rp->connection;
	    }
	    break;

	default:
	    err = 1;
	    bail_error(err);
	    break;
    }
    lc_log_debug(jnpr_log_level, "tye - %d, value = %lu", rsrc_type, value);
    err = bn_binding_new_uint64(ret_binding, node_name, ba_value, 0, value);
    bail_error(err);

bail:
    ts_free(&t_rpname);
    return err;
}

static int
md_nvsd_rm_iterate_allowed(md_commit *commit, const mdb_db *db,
                          const char *parent_node_name,
                          const uint32_array *node_attrib_ids,
                          int32 max_num_nodes, int32 start_child_num,
                          const char *get_next_child,
                          mdm_mon_iterate_names_cb_func iterate_cb,
                          void *iterate_cb_arg, void *arg)
{
    int err = 0, i = 0;
    long count = 0;
    char **keys = NULL;

    count = cp_hashtable_count(t);
    lc_log_debug(jnpr_log_level, "got count - %ld", count);
    if (count > 0) {
	/* global pool is ready added into hash table */
	keys = (char **)cp_hashtable_get_keys(t);
	if (keys == NULL) {
	    lc_log_debug(jnpr_log_level, "ERROR: No keys");
	    goto bail;
	}
	for (i = 0; i < count; i++) {
	    lc_log_debug(jnpr_log_level, "got key %d %s", i, keys[i]);
	    err = (*iterate_cb)(commit, db, keys[i], iterate_cb_arg);
	    bail_error(err);
	}
    }
bail:
    return err;
}

void * rp_copy_val_fn (void *value)
{
    if (value == NULL) return NULL;

    struct resource_pool_s *rp = malloc(sizeof(struct resource_pool_s));
    if (rp == NULL) return NULL;

    memcpy(rp, value, sizeof(struct resource_pool_s));
    return (void *)rp;
}

static int
md_nvsd_handle_link_event(md_commit *commit, const mdb_db *db,
		const char* event_name, const bn_binding_array *bindings,
		void *cb_reg_arg, void *cb_arg)
{
    int err = 0;

    lc_log_debug(jnpr_log_level, "event-name : %s", event_name);
    bn_binding_array_dump(__FUNCTION__, bindings, jnpr_log_level);

    err = update_bandwidth_ratio(commit, db);
    bail_error(err);

bail:
    return err;
}

int
md_nvsd_send_rp_update(const char *resource)
{
    bn_request *req = NULL;
    int err = 0;

    bail_null(resource);

    err = bn_event_request_msg_create(&req, "/nkn/nvsd/resource_mgr/state/update");
    bail_error(err);

    err = bn_event_request_msg_append_new_binding(req,0,
	    "rsrc_type", bt_string, resource, NULL);
    bail_error(err);

    err = md_commit_handle_event_from_module
	(NULL, req, NULL, NULL, NULL, NULL);
    bail_error(err);

    bn_request_msg_dump(NULL, req, jnpr_log_level, "RSRC EVENT");
bail:
    bn_request_msg_free(&req);
    return err;
}

/* ---------------------------------END OF FILE---------------------------------------- */

