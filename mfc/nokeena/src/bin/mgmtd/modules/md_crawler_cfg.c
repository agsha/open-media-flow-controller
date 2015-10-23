/*
 *
 * Filename:  md_crawler_cfg.c
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
#include "mdb_db.h"
#include "md_utils.h"
#include "md_mod_commit.h"
#include "md_mod_reg.h"
#include "md_upgrade.h"
#include "proc_utils.h"
#include "nkn_mgmt_defs.h"
#include "nkn_common_config.h"
#include "nvsd_mgmt_pe.h"
#include "url_utils.h"
#include "ttime.h"

//TODO
// MOve this #defs to crawler header files
#define NKN_MAX_CRAWLER_PROFILES	64
#define NKN_MAX_CRAWLER_PROFILE_STR     16
#define NKN_CRAWLER_PROFILE_ALLOWED_CHARS	" ,;$&@!~%/\\*:)(|'`\"?"

#define NKN_MAX_CRAWLER_FILEEXT		10
#define NKN_MAX_CRAWLER_FILEEXT_STR     16
#define NKN_CRAWLER_FILEEXT_ALLOWED_CHARS   " ,;$&@!~%/\\*:)(|'`\"?"

#define NKN_MAX_AUTO_GEN_STR		10
#define NKN_MAX_AUTO_GEN_OBJ		1

#define NKN_MAX_SRC_FILE_STR		10
#define NKN_MAX_SRC_OBJ		1

/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
int md_crawler_cfg_init(const lc_dso_info *info, void *data);

const bn_str_value md_crawler_cfg_initial_values[] = {

};

/*----------------------------------------------------------------------------
 * LOCAL FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
static int
md_crawler_get_profiles(md_commit *commit, const mdb_db *db,
                      const char *node_name,
                      const bn_attrib_array *node_attribs,
                      bn_binding **ret_binding,
                      uint32 *ret_node_flags, void *arg);

static int
md_crawler_profiler_check_func(md_commit *commit,
		const mdb_db *old_db, const mdb_db *new_db,
		const mdb_db_change_array *change_list,
		const tstring *node_name,
		const tstr_array *node_name_parts,
		mdb_db_change_type change_type,
		const bn_attrib_array *old_attribs,
		const bn_attrib_array *new_attribs, void
		*arg);

static int
md_crawler_iterate_profiles(md_commit *commit, const mdb_db *db,
                          const char *parent_node_name,
                          const uint32_array *node_attrib_ids,
                          int32 max_num_nodes, int32 start_child_num,
                          const char *get_next_child,
                          mdm_mon_iterate_names_cb_func iterate_cb,
                          void *iterate_cb_arg, void *arg);

static int
md_crawler_cfg_commit_check(md_commit *commit,
		const mdb_db *old_db,
		const mdb_db *new_db,
		mdb_db_change_array *change_list, void *arg);

static int
md_crawler_cfg_commit_apply(md_commit *commit,
		const mdb_db *old_db, const mdb_db *new_db,
		mdb_db_change_array *change_list, void *arg);

static int
md_crawler_cfg_add_initial(md_commit *commit, mdb_db *db, void *arg);

static md_upgrade_rule_array *md_crawler_ug_rules = NULL;

static int
md_crawler_upgrade_downgrade(const mdb_db *old_db,
                          mdb_db *inout_new_db,
                          uint32 from_module_version,
                          uint32 to_module_version, void *arg);

/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION DEFINITIONS
 *---------------------------------------------------------------------------*/
int
md_crawler_cfg_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    /*
     * Creating the Nokeena specific module
     */
    err = mdm_add_module("crawler_cfg", 7,
	    "/nkn/crawler", "/nkn/crawler/config",
	    0,
	    NULL, NULL,
	    md_crawler_cfg_commit_check,
	    NULL,
	    md_crawler_cfg_commit_apply,
	    NULL,
	    0,
	    0,
	    md_crawler_upgrade_downgrade,
	    &md_crawler_ug_rules,
	    md_crawler_cfg_add_initial,
	    NULL,
	    NULL,
	    NULL,
	    NULL,
	    NULL,
	    &module);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/crawler/config/profile/*";
    node->mrn_value_type =         bt_string;
    node->mrn_limit_str_max_chars = NKN_MAX_CRAWLER_PROFILE_STR;
    node->mrn_limit_str_no_charlist = NKN_NAME_ILLEGAL_CHARS;
    node->mrn_limit_wc_count_max = NKN_MAX_CRAWLER_PROFILES;
    node->mrn_check_node_func   = md_crawler_profiler_check_func;
    node->mrn_bad_value_msg =       "Error creating crawler profile";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/crawler/config/profile/*/url";
    node->mrn_value_type =         bt_string;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/crawler/config/profile/*/schedule/start";
    node->mrn_value_type =         bt_datetime_sec;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_initial_value_int =  0;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/crawler/config/profile/*/schedule/stop";
    node->mrn_value_type =         bt_datetime_sec;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_initial_value_int =  0;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/crawler/config/profile/*/schedule/refresh";
    node->mrn_value_type =         bt_uint32;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_initial_value_int =  0;
    node->mrn_limit_num_max_int =  43200;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/crawler/config/profile/*/link_depth";
    node->mrn_value_type =         bt_uint8;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_initial_value_int =  10;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  10;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/crawler/config/profile/*/status";
    node->mrn_value_type =         bt_uint8;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_initial_value =      "0";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/crawler/config/profile/*/file_extension/*";
    node->mrn_value_type =         bt_string;
    node->mrn_limit_str_max_chars = NKN_MAX_CRAWLER_FILEEXT_STR;
    node->mrn_limit_str_no_charlist = NKN_NAME_ILLEGAL_CHARS;
    node->mrn_limit_wc_count_max = NKN_MAX_CRAWLER_FILEEXT;
    node->mrn_bad_value_msg =       " Maximum file extensions count reached and/or Invalid character on file extension";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/crawler/config/profile/*/file_extension/*/skip_preload";
    node->mrn_value_type =         bt_bool;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_initial_value =      "false";
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/crawler/config/profile/*/auto_generate";
    node->mrn_value_type =         bt_uint8;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_initial_value_int =  0;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               	"/nkn/crawler/config/profile/*/auto_generate_dest/*";
    node->mrn_value_type =         	bt_string;
    node->mrn_limit_str_max_chars = NKN_MAX_AUTO_GEN_STR;
    node->mrn_limit_wc_count_max = 	NKN_MAX_AUTO_GEN_OBJ;
    node->mrn_bad_value_msg =       "Auto generate dest profile Limit reached";
    node->mrn_node_reg_flags =     	mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           	mcf_cap_node_basic;
    node->mrn_description =        	"";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               	"/nkn/crawler/config/profile/*/auto_generate_src/*";
    node->mrn_value_type =         	bt_string;
    node->mrn_limit_str_max_chars = NKN_MAX_SRC_FILE_STR;
    node->mrn_limit_wc_count_max = 	NKN_MAX_SRC_OBJ;
    node->mrn_bad_value_msg =       "Auto generate dest profile Limit reached";
    node->mrn_node_reg_flags =     	mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           	mcf_cap_node_basic;
    node->mrn_description =        	"";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/crawler/config/profile/*/origin_response_expiry";
    node->mrn_value_type =         bt_bool;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_initial_value =      "false";
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/crawler/config/profile/*/auto_generate_src/*/auto_gen_expiry";
    node->mrn_value_type =         bt_uint64;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_initial_value_int =  315360000;
    node->mrn_limit_num_min_int =  300;
    node->mrn_limit_num_max_int =  3153600000;
    node->mrn_bad_value_msg =      "Invalid expiry time provided.Valid range (300 to 3153600000 seconds)";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* Monitoring Nodes */

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =              "/nkn/crawler/monitor/profile/external/*";
    node->mrn_value_type =        bt_string;
    node->mrn_node_reg_flags =    mrf_flags_reg_monitor_wildcard;
    node->mrn_cap_mask =          mcf_cap_node_basic;
    node->mrn_mon_get_func =      md_crawler_get_profiles;
    node->mrn_mon_iterate_func =  md_crawler_iterate_profiles;
    node->mrn_description =       "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/crawler/monitor/profile/external/*/start_ts";
    node->mrn_value_type = bt_datetime_sec;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_csed;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/crawler/monitor/profile/external/*/end_ts";
    node->mrn_value_type = bt_datetime_sec;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_csed;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/crawler/monitor/profile/external/*/status";
    node->mrn_value_type = bt_uint8;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_csed;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/crawler/monitor/profile/external/*/next_trigger_time";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_csed;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/crawler/monitor/profile/external/*/num_objects_add";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_csed;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/crawler/monitor/profile/external/*/num_objects_add_fail";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_csed;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/crawler/monitor/profile/external/*/num_objects_del";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_csed;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name = "/nkn/crawler/monitor/profile/external/*/num_objects_del_fail";
    node->mrn_value_type = bt_uint64;
    node->mrn_node_reg_flags = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_csed;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/crawler/config/profile/*/xdomain";
    node->mrn_value_type =         bt_bool;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_initial_value =      "true";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_description =        "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* Upgrade processing nodes */
    err = md_upgrade_rule_array_new(&md_crawler_ug_rules);
    bail_error(err);
    ra = md_crawler_ug_rules;

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/crawler/config/profile/*/origin_response_expiry";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/crawler/config/profile/*/auto_generate_src/*/auto_gen_expiry";
    rule->mur_new_value_type =  bt_uint64;
    rule->mur_new_value_str =   "315360000";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =   MUTT_CLAMP;
    rule->mur_name =           "/nkn/crawler/config/profile/*/auto_generate_src/*/auto_gen_expiry";
    rule->mur_lowerbound    =  300;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 5, 6);
    rule->mur_upgrade_type =   MUTT_CLAMP;
    rule->mur_name =           "/nkn/crawler/config/profile/*/auto_generate_src/*/auto_gen_expiry";
    rule->mur_upperbound    =  3153600000;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/crawler/config/profile/*/xdomain";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "true";
    MD_ADD_RULE(ra);

bail:
    return(err);
}

/*----------------------------------------------------------------------------
 * LOCAL FUNCTION DEFINITIONS
 *---------------------------------------------------------------------------*/
static int
md_crawler_get_profiles(md_commit *commit, const mdb_db *db,
                      const char *node_name,
                      const bn_attrib_array *node_attribs,
                      bn_binding **ret_binding,
                      uint32 *ret_node_flags, void *arg)
{
    int err = 0;
    tstr_array *parts = NULL;
    uint32 num_parts = 0;
    const char *profile = NULL;

    err = bn_binding_name_to_parts(node_name, &parts, NULL);
    bail_error_null(err, parts);

    num_parts = tstr_array_length_quick(parts);

    profile = tstr_array_get_str_quick(parts, 5);
    bail_null(profile);

    err = bn_binding_new_str(ret_binding, node_name, ba_value, bt_string,
	    0, profile);
    bail_error_null(err, *ret_binding);

bail:
    tstr_array_free(&parts);
    return(err);
}

static int
md_crawler_iterate_profiles(md_commit *commit, const mdb_db *db,
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

    err = mdb_iterate_binding(commit, db, "/nkn/crawler/config/profile",
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
    safe_free(name);
    bn_binding_array_free(&binding_arrays);
    return(err);
}


static int
md_crawler_cfg_commit_check(md_commit *commit,
		const mdb_db *old_db,
		const mdb_db *new_db,
		mdb_db_change_array *change_list, void *arg)
{
    int err = 0;
    int i, num_changes = 0;
    const mdb_db_change *change = NULL;
    const char *srvr_name = NULL;
    const bn_attrib *new_val = NULL;
    tstring *value = NULL;
    char *hostname = NULL;
    char *port = NULL;
    char *ret_path = NULL;
    bn_binding *binding = NULL;
    tstring *start_time = NULL, *stop_time = NULL;
    tstr_array *t_crawler_file_extns = NULL;
    tstring *t_file_extn_name = NULL;

    num_changes = mdb_db_change_array_length_quick (change_list);
    for (i = 0; i < num_changes; i++)
    {

	change = mdb_db_change_array_get_quick (change_list, i);
	bail_null (change);

	if ( (bn_binding_name_parts_pattern_match_va (change->mdc_name_parts, 0, 6, "nkn", "crawler", "config", "profile", "*", "url")))
	{
	    if(mdct_add == change->mdc_change_type ||
		    mdct_modify ==
		    change->mdc_change_type ) {

		new_val = bn_attrib_array_get_quick(change->mdc_new_attribs,
			ba_value);
		bail_null(new_val);

		err = bn_attrib_get_tstr(new_val, NULL, bt_string,
			NULL, &value);
		bail_error_null(err, value);

		if(ts_equal_str(value, "", false)) {
		    err = md_commit_set_return_status_msg(
			    commit, 1,
			    "error: Complete URL cannot be empty,Please configure complete URL.");
		    bail_error(err);
		    goto bail;
		}

		if (!( lc_is_prefix("http://", ts_str(value), true) || lc_is_prefix("https://", ts_str(value), true)) ) {
		    err = md_commit_set_return_status_msg_fmt(commit, 1,
			    _("bad url value - '%s'. url must start with 'http://' or 'https://'"),
			    ts_str(value));
		    bail_error(err);
		    goto bail;
		}
	    }
	}
	else if ( (bn_binding_name_parts_pattern_match_va (change->mdc_name_parts, 0, 7, "nkn", "crawler", "config", "profile", "*", "schedule", "start")))
	{
	    if(mdct_add == change->mdc_change_type ||
		    mdct_modify ==
		    change->mdc_change_type ) {
		node_name_t name = {0};
		lt_time_sec ret_dts_stop = 0, ret_dts_start = 0;
		uint32_t refresh_time = 0;

		const char *profile = tstr_array_get_str_quick(change->mdc_name_parts, 4);

		bail_null(profile);
		snprintf(name, sizeof(name), "/nkn/crawler/config/profile/%s/schedule/stop",
			profile);
		err = mdb_get_node_binding(commit, new_db, name, 0, &binding);
		bail_error_null(err, binding);

		err = bn_binding_get_tstr(binding, ba_value, 0, 0, &stop_time);
		bail_error(err);

		if(stop_time) {
		    err = lt_str_to_datetime_sec(ts_str(stop_time), &ret_dts_stop);
		    bail_error(err);
		}

		snprintf(name, sizeof(name), "/nkn/crawler/config/profile/%s/schedule/refresh",
			profile);
		bn_binding_free(&binding);
		err = mdb_get_node_binding(commit, new_db, name, 0, &binding);
		bail_error_null(err, binding);

		err = bn_binding_get_uint32(binding, ba_value, NULL, &refresh_time);
		bail_error(err);

		new_val = bn_attrib_array_get_quick(change->mdc_new_attribs,
			ba_value);
		bail_null(new_val);

		err = bn_attrib_get_tstr(new_val, NULL, bt_datetime_sec,
			NULL, &value);
		bail_error_null(err, value);


		if(value) {
		    err = lt_str_to_datetime_sec(ts_str(value), &ret_dts_start);
		    bail_error(err);
		}

		if(ret_dts_stop && ret_dts_start && !(ret_dts_stop > ret_dts_start)) {
		    err = md_commit_set_return_status_msg(
			    commit, 1,
			    "error: Start time is greater than stop time");
		    bail_error(err);
		    goto bail;

		}


		if(ret_dts_start && ret_dts_stop && refresh_time && ((uint32_t)((ret_dts_stop - ret_dts_start) / 60) < refresh_time)) {
		    err = md_commit_set_return_status_msg(
			    commit, 1,
			    "error: Incorrect stop time for the given refresh interval ");
		    bail_error(err);
		    goto bail;


		}

		bn_binding_free(&binding);
		ts_free(&stop_time);
		ts_free(&value);

	    }

	}
	else if ( (bn_binding_name_parts_pattern_match_va (change->mdc_name_parts, 0, 7, "nkn", "crawler", "config", "profile", "*", "schedule", "stop")))
	{

	    if(mdct_add == change->mdc_change_type ||
		    mdct_modify ==
		    change->mdc_change_type ) {
		node_name_t name = {0};
		uint32_t refresh_time = 0;

		lt_time_sec ret_dts_stop = 0, ret_dts_start = 0;

		const char *profile = tstr_array_get_str_quick(change->mdc_name_parts, 4);

		bail_null(profile);
		snprintf(name, sizeof(name), "/nkn/crawler/config/profile/%s/schedule/start",
			profile);
		err = mdb_get_node_binding(commit, new_db, name, 0, &binding);
		bail_error_null(err, binding);

		err = bn_binding_get_tstr(binding, ba_value, 0, 0, &start_time);
		bail_error(err);

		if(start_time) {
		    err = lt_str_to_datetime_sec(ts_str(start_time), &ret_dts_start);
		    bail_error(err);
		}

		snprintf(name, sizeof(name), "/nkn/crawler/config/profile/%s/schedule/refresh",
			profile);
		bn_binding_free(&binding);

		err = mdb_get_node_binding(commit, new_db, name, 0, &binding);
		bail_error_null(err, binding);

		err = bn_binding_get_uint32(binding, ba_value, NULL, &refresh_time);
		bail_error(err);

		new_val = bn_attrib_array_get_quick(change->mdc_new_attribs,
			ba_value);
		bail_null(new_val);

		err = bn_attrib_get_tstr(new_val, NULL, bt_datetime_sec,
			NULL, &value);
		bail_error_null(err, value);


		if(value) {
		    err = lt_str_to_datetime_sec(ts_str(value), &ret_dts_stop);
		    bail_error(err);
		}

		/* check if the difference is within limits allowed */
		if(ret_dts_stop && ret_dts_start && !(ret_dts_stop > ret_dts_start)) {
		    err = md_commit_set_return_status_msg(
			    commit, 1,
			    "error: Stop time is less than start time");
		    bail_error(err);
		    goto bail;

		}

		if(ret_dts_start && ret_dts_stop && refresh_time && ((uint32_t)((ret_dts_stop - ret_dts_start) / 60) < refresh_time)) {
		    err = md_commit_set_return_status_msg(
			    commit, 1,
			    "error: Invalid stop time,given the refresh time");
		    bail_error(err);
		    goto bail;


		}


		bn_binding_free(&binding);
		ts_free(&start_time);
		ts_free(&value);

	    }
	}
	else if ( (bn_binding_name_parts_pattern_match_va (change->mdc_name_parts, 0, 7, "nkn", "crawler", "config", "profile", "*", "schedule", "refresh")))
	{

	    node_name_t name = {0};
	    lt_time_sec ret_dts_stop = 0, ret_dts_start = 0;
	    uint32_t refresh_time = 0;
	    const char *profile = tstr_array_get_str_quick(change->mdc_name_parts, 4);

	    bail_null(profile);
	    if(!(mdct_add == change->mdc_change_type ||
			mdct_modify ==
			change->mdc_change_type))
		continue;

	    new_val = bn_attrib_array_get_quick(change->mdc_new_attribs,
		    ba_value);
	    bail_null(new_val);

	    err = bn_attrib_get_uint32(new_val, NULL, NULL, &refresh_time);
	    bail_error(err);

	    if(!((refresh_time == 0) || (refresh_time >= 5))) {
		err = md_commit_set_return_status_msg(
			commit, 1,
			"error: Refresh interval value should be zero or greater than (or equal to)5");
		bail_error(err);
		goto bail;
	    }


	    snprintf(name, sizeof(name), "/nkn/crawler/config/profile/%s/schedule/start",
		    profile);

	    err = mdb_get_node_binding(commit, new_db, name, 0, &binding);
	    bail_error(err);

	    err = bn_binding_get_tstr(binding, ba_value, 0, 0, &start_time);
	    bn_binding_free(&binding);
	    bail_error(err);

	    if(start_time) {
		err = lt_str_to_datetime_sec(ts_str(start_time), &ret_dts_start);
		bail_error(err);
	    }

	    snprintf(name, sizeof(name), "/nkn/crawler/config/profile/%s/schedule/stop",
		    profile);
	    err = mdb_get_node_binding(commit, new_db, name, 0, &binding);
	    bail_error(err);

	    err = bn_binding_get_tstr(binding, ba_value, 0, 0, &stop_time);
	    bail_error(err);

	    if(stop_time) {
		err = lt_str_to_datetime_sec(ts_str(stop_time), &ret_dts_stop);
		bail_error(err);
	    }

	    if(ret_dts_start && ret_dts_stop && refresh_time && ((uint32_t)((ret_dts_stop - ret_dts_start) / 60) < refresh_time)) {
		err = md_commit_set_return_status_msg(
			commit, 1,
			"error: Invalid refresh time");
		bail_error(err);
		goto bail;


	    }
	    ts_free(&start_time);
	    ts_free(&stop_time);
	    bn_binding_free(&binding);
	}
	else if ( (bn_binding_name_parts_pattern_match_va (change->mdc_name_parts, 0, 7, "nkn", "crawler", "config", "profile", "*", "auto_generate_dest", "*")))
	{
	    const char *dest_format = tstr_array_get_str_quick(change->mdc_name_parts, 6);
	    if (dest_format && strcmp(dest_format,"asx")){
		err = md_commit_set_return_status_msg(
			commit, 1,
			"Currently only asx format for destination is supported");
		bail_error(err);
		goto bail;

	    }
	}
	else if ( (bn_binding_name_parts_pattern_match_va (change->mdc_name_parts, 0, 7, "nkn", "crawler", "config", "profile", "*", "auto_generate_src", "*")))
	{
	    const char *src_format = tstr_array_get_str_quick(change->mdc_name_parts, 6);
	    if (src_format && strcmp(src_format,"wmv")){
		err = md_commit_set_return_status_msg(
			commit, 1,
			"Currently only wmv format for source is supported");
		bail_error(err);
		goto bail;
	    }
	}
	else if ( (bn_binding_name_parts_pattern_match_va (change->mdc_name_parts, 0, 7, "nkn", "crawler", "config", "profile", "*", "file_extension", "*")))
	{
	    node_name_t name = {0};

	    if(mdct_delete != change->mdc_change_type) {
		const char *crawler_name = tstr_array_get_str_quick(change->mdc_name_parts, 4);
		const char *file_extn_name = tstr_array_get_str_quick(change->mdc_name_parts, 6);

		if(!(lc_is_prefix(".", file_extn_name, true)))
		{
		    err = md_commit_set_return_status_msg(
			    commit, 1,
			    "File extension name should starts with '.'");
		    bail_error(err);
		    goto bail;
		}

		snprintf(name, sizeof(name), "/nkn/crawler/config/profile/%s/file_extension/*",
			crawler_name);

		err = mdb_get_matching_tstr_array(commit,
			old_db,
			name,
			0,
			&t_crawler_file_extns);
		bail_error(err);

		if (t_crawler_file_extns != NULL) {
		    for(uint32 j = 0; j < tstr_array_length_quick(t_crawler_file_extns); j++) {

			err = mdb_get_node_value_tstr(commit,
				old_db,
				tstr_array_get_str_quick(t_crawler_file_extns, j),
				0, NULL,
				&t_file_extn_name);
			bail_error(err);

			if (t_file_extn_name && ts_equal_str(t_file_extn_name, file_extn_name, true)) {
			    /* Name matched... REJECT the commit */
			    err = md_commit_set_return_status_msg(
				    commit, 1,
				    "File extension already exists");
			    bail_error(err);
			    goto bail;

			}
		    }
		}
	    }
	}
	else if ( (bn_binding_name_parts_pattern_match_va (change->mdc_name_parts, 0, 6, "nkn", "crawler", "config", "profile", "*", "status")))
	{
	    if(mdct_modify == change->mdc_change_type ) {
		node_name_t name = {0};
		tstring *conf_start_time = NULL;
		lt_time_sec ret_start = 0;
		uint8 isActive = 0;

		const char *profile = tstr_array_get_str_quick(change->mdc_name_parts, 4);

		bail_null(profile);

		new_val = bn_attrib_array_get_quick(change->mdc_new_attribs,
			ba_value);
		bail_null(new_val);

		err = bn_attrib_get_uint8(new_val,NULL,NULL, &isActive);
		bail_error(err);

		if(isActive == 0)
		    goto bail;

		snprintf(name, sizeof(name), "/nkn/crawler/config/profile/%s/schedule/start",
			profile);
		err = mdb_get_node_binding(commit, new_db, name, 0, &binding);
		bail_error_null(err, binding);

		err = bn_binding_get_tstr(binding, ba_value, 0, 0, &conf_start_time);
		bail_error(err);

		if(conf_start_time) {
		    err = lt_str_to_datetime_sec(ts_str(conf_start_time), &ret_start);
		    bail_error(err);
		}

		if(ret_start == 0) {
		    err = md_commit_set_return_status_msg(
			    commit, 1,
			    "Configure the Start time before activate the crawler");
		    bail_error(err);
		    goto bail;
		}
		ts_free(&conf_start_time);
	    }
	}
    }

    err = md_commit_set_return_status(commit, 0);
    bail_error(err);

bail:
    safe_free(hostname);
    safe_free(port);
    safe_free(ret_path);
    ts_free(&value);
    ts_free(&start_time);
    ts_free(&stop_time);
    bn_binding_free(&binding);
    return(err);
}

static int
md_crawler_cfg_commit_apply(md_commit *commit,
		const mdb_db *old_db, const mdb_db *new_db,
		mdb_db_change_array *change_list, void *arg)
{
    int err = 0;

    /*
     * Nothing to do.
     */

bail:
    return(err);
}

static int
md_crawler_cfg_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
    int err = 0;
    bn_binding *binding = NULL;


bail:
    bn_binding_free(&binding);
    return(err);
}


static int
md_crawler_profiler_check_func(md_commit *commit,
		const mdb_db *old_db, const mdb_db *new_db,
		const mdb_db_change_array *change_list,
		const tstring *node_name,
		const tstr_array *node_name_parts,
		mdb_db_change_type change_type,
		const bn_attrib_array *old_attribs,
		const bn_attrib_array *new_attribs, void
		*arg)
{
    int err = 0;
    char *profile_name = NULL;
    const bn_attrib *attrib = NULL;

    if(change_type == mdct_add) {

	bail_null(new_attribs);
	attrib = bn_attrib_array_get_quick(new_attribs, ba_value);
	bail_null(attrib);

	err = bn_attrib_get_str(attrib, NULL, bt_string, NULL, &profile_name);
	bail_error_null(err, profile_name);

	if(!strcmp(profile_name, "list")) {
	    err = md_commit_set_return_status_msg(commit, 1,
		    "list as a crawler name is not allowed.");
	    bail_error(err);


	}

    }
bail:
    safe_free(profile_name);
    return err;

}


static int
md_crawler_upgrade_downgrade(const mdb_db *old_db,
                          mdb_db *inout_new_db,
                          uint32 from_module_version,
                          uint32 to_module_version, void *arg)
{
    int err = 0;
    const char *crawl_binding_name = NULL;
    const char *crawl_cd_binding_name = NULL;
    tstr_array *crawl_url_bindings = NULL;
    tstr_array *crawl_cd_bindings = NULL;
    tstr_array *binding_parts= NULL;
    tstring *cd_value = NULL;
    tstring *url_value = NULL;
    tstring *old_url_value = NULL;
    tbool cd_found = false;
    tbool url_found = false;
    tbool old_url_found = false;
    unsigned int arr_index ;
    temp_buf_t new_url = {0};
    temp_buf_t old_url = {0};
    temp_buf_t temp_url = {0};
    const char *t_crawler = NULL;
    const char *t_crawler_profile = NULL;
    char *path = NULL;

    err = md_generic_upgrade_downgrade(old_db, inout_new_db, from_module_version,
	    to_module_version, arg);
    bail_error(err);

    if((to_module_version == 3) || (to_module_version == 4)){	

	err = mdb_get_matching_tstr_array( NULL,
		inout_new_db,
		"/nkn/crawler/config/profile/*/client_domain",
		0,
		&crawl_cd_bindings);
	bail_error(err);

	lc_log_basic(LOG_DEBUG, "total crawl_cd_bindings %d", tstr_array_length_quick(crawl_cd_bindings)  );

	/*
	 * Below loop get the domain value from the client_domain node & replace the the domain value in the url node
	 * while upgrading from 11b3 to 12.1 it also includes the protocol check for the url value */

	for ( arr_index = 0 ; arr_index < tstr_array_length_quick(crawl_cd_bindings) ; ++arr_index)
	{
	    crawl_cd_binding_name = tstr_array_get_str_quick(crawl_cd_bindings, arr_index);

	    if(!crawl_cd_binding_name){
		continue;
	    }

	    tstr_array_free(&binding_parts);
	    err = bn_binding_name_to_parts (crawl_cd_binding_name, &binding_parts, NULL);
	    bail_error(err);

	    if(!binding_parts){
		continue;
	    }

	    t_crawler = tstr_array_get_str_quick (binding_parts, 4);

	    if(!t_crawler){
		continue;
	    }

	    ts_free(&cd_value);
	    err = mdb_get_node_value_tstr(NULL, inout_new_db, crawl_cd_binding_name, 0, &cd_found, &cd_value);
	    bail_error(err);

	    if ( cd_found && cd_value && (ts_length(cd_value) > 0) )
	    {
		snprintf(old_url, sizeof(old_url), "/nkn/crawler/config/profile/%s/url", t_crawler);

		ts_free(&old_url_value);
		err = mdb_get_node_value_tstr(NULL, inout_new_db, old_url, 0, &old_url_found, &old_url_value);
		bail_error(err);

		if(!old_url_found)
		    continue;

		snprintf(temp_url, sizeof(temp_url), "%s", ts_str(old_url_value));
		path = NULL;

		if(lc_is_prefix("http://", temp_url, true)){
		    path = strstr(temp_url+7, "/");
		}
		else{
		    path = strstr(temp_url, "/");
		}

		if(path != NULL){
		    if(lc_is_prefix("http://", ts_str(cd_value), true)){
			snprintf(new_url, sizeof(new_url), "%s%s", ts_str(cd_value), path);
		    }
		    else{
			snprintf(new_url, sizeof(new_url), "http://%s%s", ts_str(cd_value), path);
		    }

		    err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
			    0, bt_string, new_url, old_url,
			    t_crawler);
		    bail_error(err);
		}
	    }

	    err = mdb_set_node_str(NULL, inout_new_db, bsso_delete,
		    0, bt_string,
		    "",
		    "/nkn/crawler/config/profile/%s/client_domain",
		    t_crawler);
	    bail_error(err);

	    err = mdb_set_node_str(NULL, inout_new_db, bsso_delete,
		    0, bt_string,
		    "",
		    "/nkn/crawler/config/profile/%s/transport",
		    t_crawler);
	    bail_error(err);
	}

	if((to_module_version == 3) ){

	    err = mdb_get_matching_tstr_array( NULL,
		    inout_new_db,
		    "/nkn/crawler/config/profile/*/url",
		    0,
		    &crawl_url_bindings);
	    bail_error(err);

	    lc_log_basic(LOG_DEBUG, "total crawl_url_bindings %d", tstr_array_length_quick(crawl_url_bindings)  );
	    /*
	     * Below loop check for the protocol info in the url node if not exist it prepends the protocol info
	     * in the url node while upgrading from 11b3 to 12.1*/

	    for ( arr_index = 0 ; arr_index < tstr_array_length_quick(crawl_url_bindings) ; ++arr_index)
	    {
		crawl_binding_name = tstr_array_get_str_quick(
			crawl_url_bindings, arr_index);

		if(!crawl_binding_name){
		    continue;
		}

		tstr_array_free(&binding_parts);
		err = bn_binding_name_to_parts (crawl_binding_name, &binding_parts, NULL);
		bail_error(err);

		if(!binding_parts){
		    continue;
		}

		t_crawler_profile = tstr_array_get_str_quick (binding_parts, 4);

		if(!t_crawler_profile){
		    continue;
		}

		ts_free(&url_value);
		err = mdb_get_node_value_tstr(NULL, inout_new_db, crawl_binding_name, 0, &url_found, &url_value);
		bail_error(err);

		if ( url_found && url_value && (ts_length(url_value) > 0) )
		{
		    if(lc_is_prefix("http://", ts_str(url_value), true)){
			snprintf(new_url, sizeof(new_url), "%s", ts_str(url_value));
		    }
		    else{
			snprintf(new_url, sizeof(new_url), "http://%s", ts_str(url_value));
		    }

		    err = mdb_set_node_str(NULL, inout_new_db, bsso_modify,
			    0, bt_string,
			    new_url,
			    "/nkn/crawler/config/profile/%s/url",
			    t_crawler_profile);
		    bail_error(err);
		}
	    }
	}
    }
bail:
    ts_free(&url_value);
    ts_free(&old_url_value);
    ts_free(&cd_value);
    tstr_array_free(&binding_parts);
    tstr_array_free(&crawl_url_bindings);
    tstr_array_free(&crawl_cd_bindings);
    return err;
}

/* ---------------------------------END OF FILE---------------------------------------- */
