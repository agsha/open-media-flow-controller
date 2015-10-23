/*
 *
 * Filename:  md_nvsd_http.c
 * Date:      2008/10/21
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
#include "mdm_events.h"
#include "nkn_defs.h"
#include "file_utils.h"
#include "nkn_mgmt_defs.h"

extern unsigned int jnpr_log_level;

md_commit_forward_action_args md_http_fwd_args =
{
    GCL_CLIENT_NVSD, true, N_("Request failed; MFD http subsystem failed"),
    mfat_nonbarrier_async
#ifdef PROD_FEATURE_I18N_SUPPORT
    , GETTEXT_DOMAIN
#endif
};

typedef struct ipt_rule_match_params {
	str_value_t intf_name;
	str_value_t proto;
	uint16_t dport_min;
	uint16_t dport_max;
	str_value_t tgt;
}ipt_match_params_t;

#define IPT_COMPLAIN_STATUS(status, err_msg)                 \
    do {                                                     \
	if(status) {                                         \
	    err = 1;                                         \
	    lc_log_debug(LOG_NOTICE, err_msg);               \
	    goto bail;                                       \
	}                                                    \
    }while(0)


/* ------------------------------------------------------------------------- */
int md_lib_cal_tot_avail_bw(md_commit *commit, const mdb_db *new_db, uint64_t *tot_bw, tbool count_down_link);
int md_nvsd_http_init(const lc_dso_info *info, void *data);

static md_upgrade_rule_array *md_nvsd_http_ug_rules = NULL;

static int md_nvsd_http_add_initial(md_commit *commit, mdb_db *db, void *arg);

static int md_nvsd_http_commit_check(md_commit *commit,
                                const mdb_db *old_db, const mdb_db *new_db,
                                mdb_db_change_array *change_list, void *arg);

static int md_nvsd_http_commit_apply(md_commit *commit,
                                const mdb_db *old_db, const mdb_db *new_db,
                                mdb_db_change_array *change_list, void *arg);

static int
md_nvsd_http_upgrade_downgrade(const mdb_db *old_db,
                              mdb_db *inout_new_db,
                              uint32 from_module_version,
                              uint32 to_module_version,
                              void *arg);


int
md_nvsd_http_side_effects( md_commit *commit,
		const mdb_db *old_db,
		mdb_db *inout_new_db,
		mdb_db_change_array *change_list,
		void *arg);
static int
md_nvsd_handle_intf_change_event(md_commit *commit, const mdb_db *db,
		const char* event_name, const bn_binding_array *bindings,
		void *cb_reg_arg, void *cb_arg);
typedef int
(*update_global_bandwidth)(md_commit *commit, mdb_db *new_db, uint64_t old_value, uint64_t new_value, tbool tot_rp_needed);
update_global_bandwidth md_nvsd_update_global_bw = NULL;

static int ip_table_ref_cnt = 0;

static int
md_nvsd_http_add_divert_chain(md_commit *commit,const  mdb_db *db);

static int
md_nvsd_http_delete_divert_chain(md_commit *commit , const mdb_db *db);


static int
md_nvsd_http_get_rule_matches(md_commit *commit ,const mdb_db *db,const char *rule_num,
		ipt_match_params_t *match_params);

static int
md_nvsd_http_ipt_has_TPXY_rules(md_commit *commit, const mdb_db *db,
			    ipt_match_params_t *match_params, tbool *is_existing);

static int
md_nvsd_http_ipt_get_rules(md_commit *commit ,const mdb_db *db, const char *match_node,
			    		const char *match_val, tstr_array **match_rules);

static int
md_nvsd_http_ipt_DIVERT_rules(md_commit *commit, const mdb_db *db, tbool *is_existing);

static tbool
md_nvsd_http_is_rule_matching(ipt_match_params_t *match1, ipt_match_params_t *match2);

static int
md_nvsd_http_synch_ipt(md_commit *commit,const mdb_db *db);

static int
md_nvsd_http_get_intf_ip(md_commit *commit, const mdb_db *db, const char *intf_name,
								    char **ip_address);

/* ------------------------------------------------------------------------- */
int
md_nvsd_http_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    /*
     * Creating the Nokeena specific module
     */
    err = mdm_add_module("nvsd-http", 19,
                         "/nkn/nvsd/http", "/nkn/nvsd/http/config",
			 modrf_all_changes,
                         md_nvsd_http_side_effects, NULL,
                         md_nvsd_http_commit_check, NULL,
                         md_nvsd_http_commit_apply, NULL,
                         0, 0,
                         md_nvsd_http_upgrade_downgrade,
			 &md_nvsd_http_ug_rules,
                         md_nvsd_http_add_initial, NULL,
                         NULL, NULL, NULL, NULL,
			 &module);
    bail_error(err);

    err = mdm_get_symbol("md_nvsd_resource_mgr", "update_global_bandwidth",
                         (void **)&md_nvsd_update_global_bw);
    bail_error_null(err, md_nvsd_update_global_bw);

    // BZ 2608 - Allow multipl listen ports
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/http/config/server_port/*";
    node->mrn_value_type =         bt_uint16;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_limit_wc_count_max = 64;
    node->mrn_limit_wc_count_min = 0;
    node->mrn_bad_value_msg =      N_("error: Cannot set more than 64 ports");
    node->mrn_description =        "HTTP server's listening port";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/http/config/server_port/*/port";
    node->mrn_value_type =         bt_uint16;
    node->mrn_initial_value_int =  0;
    node->mrn_limit_num_min =      "0";
    node->mrn_limit_num_max =      "65535";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "HTTP server's listening port";
    err = mdm_add_node(module, &node);
    bail_error(err);
    // End BZ 2608


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/http/config/rate_control";
    node->mrn_value_type =         bt_uint64;
    node->mrn_initial_value_int =  300000;
    node->mrn_limit_num_min_int =  1000;
    node->mrn_limit_num_max_int =  1000000;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "HTTP GET rate control to avoid HTTP attack";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* Added in module version 3
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/http/config/om_conn_limit";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  0;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "HTTP - to limit the # connection toward origin";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/http/config/max_http_req_size";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  16384;
    node->mrn_limit_num_min_int =  1;
    node->mrn_limit_num_max_int =  32768;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "HTTP server's Maximum HTTP Request Size";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/http/config/content/type/*";
    node->mrn_value_type =         bt_string;
    node->mrn_limit_str_max_chars = 16;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "HTTP server's Content Type Name";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/http/config/content/type/*/mime";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_limit_str_max_chars = 256;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "HTTP server's Content Type MIME String";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/http/config/content/type/*/allow";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "false";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "HTTP server's Content Type MIME - Allowed or not Allowed";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/http/config/response/header/supress/*";
    node->mrn_value_type =         bt_string;
    node->mrn_limit_str_max_chars = 64;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "HTTP server's Suppress Response Header";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/http/config/response/header/append/*";
    node->mrn_value_type =         bt_string;
    node->mrn_limit_str_max_chars = 64;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "HTTP server's Add Response Header";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/http/config/response/header/append/*/value";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_limit_str_max_chars = 256;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "HTTP server's Add Response Header";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/http/config/content/allowed_type/*";
    node->mrn_value_type =         bt_string;
    node->mrn_limit_str_max_chars = 64;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "HTTP server's Content Allowed Type";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/http/config/response/header/set/*";
    node->mrn_value_type =         bt_string;
    node->mrn_limit_str_max_chars = 64;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "HTTP server's Set Response Header";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/http/config/response/header/set/*/value";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_limit_str_max_chars = 256;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "HTTP server's Set Response Header";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/http/config/interface/*";
    node->mrn_value_type =          bt_string;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/http/config/trace/enabled";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Delivery Protocol Trace Enabled";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/http/config/ipv6/enabled";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Delivery Protocol IPv6 Enabled";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/http/config/conn-pool/max-arp-entry";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  1024;
    node->mrn_limit_num_min_int =  64;
    node->mrn_limit_num_max_int =  16384;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Maximum Number of ARP cache table entries supported";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/http/config/conn-pool/origin/enabled";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "true";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Delivery Protocol Connection Pool Origin enabled";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/http/config/conn-pool/origin/timeout";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  90;
    node->mrn_limit_num_min_int =  1;
    node->mrn_limit_num_max_int =  86400;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Delivery protocol Connection Pool Origin Timeout";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/http/config/conn-pool/origin/max-conn";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  4096;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  128000;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Delivery protocol Connection Pool Origin - "
	    			"maximum number of connections to open "
				"concurrently to a single origin server";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/http/config/t-proxy/*";
    node->mrn_value_type =         bt_string;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_limit_wc_count_max = 64;
    node->mrn_limit_wc_count_min = 0;
    node->mrn_bad_value_msg =      N_("Cannot add more than 64 interfaces for t-proxy");
    node->mrn_description =        "interface for t-proxy";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/http/config/allow_req/method/*";
    node->mrn_value_type =         bt_string;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_limit_wc_count_max = 16;
    node->mrn_limit_wc_count_min = 0;
    node->mrn_bad_value_msg =      N_("error: Cannot set more than 16 request methods");
    node->mrn_description =        "HTTP allow request methods";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/http/config/allow_req/all";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "true";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "All request methods are disabled";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/http/config/connection/persistence/num-requests";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value =       "32";
    node->mrn_limit_num_min =       "0";
    node->mrn_limit_num_max =       "100";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Connection persistence requests count";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/http/config/connection/close/use-reset";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "socket Connection close option";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * mgmtd node to control enabling/disabling of revalidation of cache using GET method
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/http/config/reval_meth_get";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "false";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "enable cache revalidation using GET method";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/http/config/filter/mode";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "cache";
    node->mrn_limit_str_choices_str = ",cache,proxy,packet";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Mode of MFC: caching | proxy | packet (dpi)";
    node->mrn_bad_value_msg =	   "Invalid Mode";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_event(module, &node, 1);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/http/notify/delivery_intf_add";
    node->mrn_node_reg_flags = mrf_flags_reg_event;
    node->mrn_cap_mask = mcf_cap_event_privileged;
    node->mrn_description = "";
    node->mrn_events[0]->mre_binding_name = "interface";
    node->mrn_events[0]->mre_binding_type = bt_string;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_event(module, &node, 1);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/http/notify/delivery_intf_del";
    node->mrn_node_reg_flags = mrf_flags_reg_event;
    node->mrn_cap_mask = mcf_cap_event_privileged;
    node->mrn_description = "";
    node->mrn_events[0]->mre_binding_name = "interface";
    node->mrn_events[0]->mre_binding_type = bt_string;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = md_events_handle_int_interest_add(
		    "/net/interface/events/state_change",
		    md_nvsd_handle_intf_change_event, NULL);
    bail_error(err);

     /* Upgrade processing nodes */

    err = md_upgrade_rule_array_new(&md_nvsd_http_ug_rules);
    bail_error(err);
    ra = md_nvsd_http_ug_rules;

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type = MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/http/config/trace/enabled";
    rule->mur_new_value_type = bt_bool;
    rule->mur_new_value_str = "false";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/http/config/om_conn_limit";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "0";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/nkn/nvsd/http/config/max_http_req_size";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "16384";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 5, 6);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/nkn/nvsd/http/config/rate_control";
    rule->mur_new_value_type =  bt_uint64;
    rule->mur_new_value_str =   "300000";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/http/config/conn-pool/origin/enabled";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "true";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/http/config/conn-pool/origin/timeout";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "300";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type =    MUTT_CLAMP;
    rule->mur_name =            "/nkn/nvsd/http/config/max_http_req_size";
    rule->mur_lowerbound =  	1;
    rule->mur_upperbound =   	32768;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 7, 8);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/http/config/conn-pool/origin/max-conn";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "256";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 8, 9);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/http/config/allow_req/all";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 9, 10);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/http/config/conn-pool/max-arp-entry";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "1024";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 10, 11);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =             "/nkn/nvsd/http/config/connection/persistence/num-requests";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "0";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 11, 12);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =              "/nkn/nvsd/http/config/connection/close/use-reset";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 12, 13);
    rule->mur_upgrade_type =    MUTT_ADD;
    rule->mur_name =            "/nkn/nvsd/http/config/reval_meth_get";
    rule->mur_new_value_type =  bt_bool;
    rule->mur_new_value_str =   "false";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 13, 14);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/nkn/nvsd/http/config/conn-pool/origin/max-conn";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "4096";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 13, 14);
    rule->mur_upgrade_type =    MUTT_CLAMP;
    rule->mur_name =            "/nkn/nvsd/http/config/conn-pool/origin/max-conn";
    rule->mur_lowerbound =      0;
    rule->mur_upperbound =      128000;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 15, 16);
    rule->mur_upgrade_type =    MUTT_CLAMP;
    rule->mur_name =            "/nkn/nvsd/http/config/max_http_req_size";
    rule->mur_lowerbound =  	1;
    rule->mur_upperbound =   	8192; /*bug 8181 */
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 16, 17);
    rule->mur_upgrade_type =    MUTT_CLAMP;
    rule->mur_name =            "/nkn/nvsd/http/config/max_http_req_size";
    rule->mur_lowerbound =  	1;
    rule->mur_upperbound =   	32768; /*bug 8388 */
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 16, 17);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =            "/nkn/nvsd/http/config/max_http_req_size";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "16384"; /*bug 8388 */
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 17, 18);
    rule->mur_upgrade_type = MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/http/config/ipv6/enabled";
    rule->mur_new_value_type = bt_bool;
    rule->mur_new_value_str = "false";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 17, 18);
    rule->mur_upgrade_type =    MUTT_CLAMP;
    rule->mur_name =            "/nkn/nvsd/http/config/rate_control";
    rule->mur_lowerbound =      1000;
    rule->mur_upperbound =      1000000;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 18, 19);
    rule->mur_upgrade_type = MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/http/config/filter/mode";
    rule->mur_new_value_type = bt_string;
    rule->mur_new_value_str = "cache";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);
bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
const bn_str_value md_nvsd_http_initial_values[] = {
    {"/nkn/nvsd/http/config/content/type/html/mime",  bt_string, "text/html"},
    {"/nkn/nvsd/http/config/content/type/html/allow", bt_bool, "false"},
    {"/nkn/nvsd/http/config/server_port/1", bt_uint16, "1"},
    {"/nkn/nvsd/http/config/server_port/1/port", bt_uint16, "80"}, // Default HTTP port
};


/* ------------------------------------------------------------------------- */
static int
md_nvsd_http_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
    int err = 0;
    bn_binding *binding = NULL;

    err = mdb_create_node_bindings
        (commit, db, md_nvsd_http_initial_values,
         sizeof(md_nvsd_http_initial_values) / sizeof(bn_str_value));
    bail_error(err);

 bail:
    bn_binding_free(&binding);
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
md_nvsd_http_commit_check(md_commit *commit,
                     const mdb_db *old_db,
                     const mdb_db *new_db,
                     mdb_db_change_array *change_list, void *arg)
{
    int err = 0;
    uint32 i = 0;
    uint32 num_changes = 0;
    const mdb_db_change *change = NULL;
    tstr_array *binding_parts= NULL;
    char intf_node_name[256] = {0};
    const char  *t_intf_name = NULL;
    tbool intf_valid = 0;
    tstring *interface = NULL;


    num_changes = mdb_db_change_array_length_quick(change_list);
    for(i = 0; i < num_changes; i++) {
	change = mdb_db_change_array_get_quick(change_list, i);
	bail_null(change);

	if ((bn_binding_name_parts_pattern_match_va (change->mdc_name_parts, 0, 9, "nkn", "nvsd", "http", "config", "response", "header", "set", "*", "value"))
		&& (mdct_modify == change->mdc_change_type))
	{
	    err = md_commit_set_return_status_msg( commit, 1, _("Header is already set"));
	    bail_error(err);
	    goto bail;
	}
	if ((bn_binding_name_parts_pattern_match_va (change->mdc_name_parts, 0, 6, "nkn", "nvsd", "http", "config", "t-proxy", "*"))
		&& ((mdct_add == change->mdc_change_type)
		    || (mdct_delete == change->mdc_change_type)))
	{
	    err = bn_binding_name_to_parts(ts_str(change->mdc_name), &binding_parts, NULL);
	    bail_error(err);
	    /*  "/nkn/nvsd/http/config/t-proxy/eth0" */
	    t_intf_name = tstr_array_get_str_quick (binding_parts, 5);
	    bail_null(t_intf_name);
	    if ((safe_strcmp("lo", t_intf_name) == 0)
		    || lc_is_prefix("lo:", t_intf_name, false)) {
		err = md_commit_set_return_status_msg( commit, EINVAL,
			"loopback interface is not allowed for T-proxy");
		bail_error(err);
		goto bail;
	    }
	    /* Check for host interface in configuration database */
	    snprintf(intf_node_name, sizeof(intf_node_name),
		    "/net/interface/config/%s", t_intf_name);

	    err = mdb_get_node_value_tstr(commit, new_db, intf_node_name, 0,
		    &intf_valid, &interface);

	    if (!intf_valid) {
		err = md_commit_set_return_status_msg_fmt(commit, ENOENT,
			"%s: No such interface exists on this host", t_intf_name);
		bail_error(err);
		goto bail;
	    }
	}
    }

    err = md_commit_set_return_status(commit, 0);
    bail_error(err);

bail:
    ts_free(&interface);
    tstr_array_free(&binding_parts);
    return(err);
}



static int
md_nvsd_handle_ipfilter(md_commit *commit, const mdb_db *db,
		 const bn_binding *binding, void *arg, int add)
{
    int err = 0, status = 0, free_intf_name = 0;
    char *intf_name = arg, bn_name[256], *ip_address = NULL;
    char port[32] = {0};
    const tstring *bn_string;
    uint16 port_num = 0;
    bn_binding *ip_binding = NULL;
    tbool is_existing = false;

    bail_null(binding);

    bn_binding_dump(jnpr_log_level,__FUNCTION__ , binding);
    bn_string = bn_binding_get_name_quick(binding);
    bail_null(bn_string);

    if (bn_binding_name_pattern_match(ts_str(bn_string),
		"/nkn/nvsd/http/config/t-proxy/*")) {
	/* arg is port number */
	port_num = *(uint16 *)arg;
	err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &intf_name);
	bail_error_null(err, intf_name);
	free_intf_name = 1;

    } else if (bn_binding_name_pattern_match(ts_str(bn_string),
		"/nkn/nvsd/http/config/server_port/*/port")) {
	/* arg is interface name */
	intf_name = arg;
	err = bn_binding_get_uint16(binding, ba_value, NULL, &port_num);
	bail_error(err);
    } else {
	goto bail;
    }

    lc_log_basic(jnpr_log_level, "%s: got port 2 - %d", __FUNCTION__, port_num);
    lc_log_basic(jnpr_log_level, "%s: got interface - %s",__FUNCTION__, intf_name);

    if (0 == strcmp(intf_name, "lo") || (0 == port_num)) {
	/* we don't want to set on loop back interface or port number zero*/
	goto bail;
    }
    snprintf(port, sizeof(port), "%d", port_num);
    snprintf(bn_name, sizeof(bn_name), "/net/interface/state/%s/addr/ipv4/1/ip", intf_name);

    err = mdb_get_node_binding(commit, db, bn_name, 0, &ip_binding);
    bail_error(err);

    if (ip_binding == NULL) {
	/* we don't have ip address set for this interface, ignore this */
	err = 0;
	goto bail;
    }
    err = bn_binding_get_str(ip_binding, ba_value, bt_ipv4addr, NULL, &ip_address);
    bail_error(err);

    if (ip_address == NULL) {
	/* we don't have ip address set for this interface */
	err = 0;
	goto bail;
    }

    lc_log_basic(jnpr_log_level, "%s: got ip addres - %s", __FUNCTION__, ip_address);
    if (add) {

	/* check if rule is already present */
	ipt_match_params_t ip_match_params;
	memset(&ip_match_params, 0 ,sizeof(ipt_match_params_t));

	snprintf(ip_match_params.intf_name,sizeof(ip_match_params.intf_name),
		"%s",intf_name);
	ip_match_params.dport_min = port_num;
	ip_match_params.dport_max = port_num;

	snprintf(ip_match_params.tgt,sizeof(ip_match_params.tgt),
		"%s","TPROXY");

	snprintf(ip_match_params.proto,sizeof(ip_match_params.proto),
		"%s","tcp");

	err = md_nvsd_http_ipt_has_TPXY_rules(commit, db, &ip_match_params, &is_existing);
	bail_error(err);

	//Dont add extra ones
	if(is_existing)
	    goto bail;

	err = lc_launch_quick_status(&status, NULL, true, 19 ,
		"/sbin/iptables", "-t", "mangle", "-A", "PREROUTING",
		"-i", intf_name, "-p", "tcp",
		"--dport", port, "-j", "TPROXY",
		"--tproxy-mark", "0x1/0x1", "--on-port", port,
		"--on-ip", ip_address);

	lc_log_basic(jnpr_log_level, "%s: got status - %d", __FUNCTION__, status);
	bail_error(err);
    } else { //Delete
	err = lc_launch_quick_status(&status, NULL, false, 19 ,
		"/sbin/iptables", "-t", "mangle", "-D", "PREROUTING",
		"-i", intf_name, "-p", "tcp",
		"--dport", port, "-j", "TPROXY",
		"--tproxy-mark", "0x1/0x1", "--on-port", port,
		"--on-ip", ip_address);
	bail_error(err);
    }
    if (status) {
	err = 1;
	goto bail;
    }

bail:
    safe_free(ip_address);
    if (free_intf_name) {
	safe_free(intf_name);
    }
    bn_binding_free(&ip_binding);

    return err;
}


static int
md_nvsd_http_ipt_has_TPXY_rules(md_commit *commit, const mdb_db *db,
	ipt_match_params_t *match_params, tbool *is_existing) {
    int err = 0;
    tstr_array *tproxy_rules = NULL;
    uint16_t i = 0;
    ipt_match_params_t ret_match_params;

    memset(&ret_match_params, 0 ,sizeof(ipt_match_params_t));

    err = md_nvsd_http_ipt_get_rules(commit, db,
	    "/iptables/state/table/mangle/chain/PREROUTING/rule/*/target/name",
	    match_params->tgt, &tproxy_rules);
    bail_error(err);

    for(i = 0; i < tstr_array_length_quick(tproxy_rules); i++ ) {
	const char *rule_num = NULL;
	rule_num = tstr_array_get_str_quick(tproxy_rules, i);
	bail_null(rule_num);

	err = md_nvsd_http_get_rule_matches(commit, db, rule_num,
		&ret_match_params);
	bail_error(err);

	*is_existing = md_nvsd_http_is_rule_matching(match_params,
		&ret_match_params);

	if(*is_existing)
	    break;
    }

bail:
    tstr_array_free(&tproxy_rules);
    return err;
}

static tbool
md_nvsd_http_is_rule_matching(ipt_match_params_t *match1, ipt_match_params_t *match2) {

    if(match1->intf_name &&
	    (strcmp(match1->intf_name,match2->intf_name))) {
	return false;

    }

    if(match1->proto &&
	    (strcmp(match1->proto,match2->proto))) {
	return false;

    }

    if(match1->tgt &&
	    (strcmp(match1->tgt,match2->tgt))) {
	return false;

    }

    if(match1->dport_min != match2->dport_min)
	return false;

    if(match1->dport_max != match2->dport_max)
	return false;

    return true;

}

static int
md_nvsd_http_ipt_DIVERT_rules(md_commit *commit, const mdb_db *db, tbool *is_existing) {
    int err = 0;
    tstr_array *divert_accept_rules = NULL;
    tstr_array *divert_mark_rules = NULL;

    err = md_nvsd_http_ipt_get_rules(commit, db,
	    "/iptables/state/table/mangle/chain/DIVERT/rule/*/target/name",
	    "ACCEPT", &divert_accept_rules);
    bail_error(err);

    err = md_nvsd_http_ipt_get_rules(commit, db,
	    "/iptables/state/table/mangle/chain/DIVERT/rule/*/target/name",
	    "MARK",	&divert_mark_rules);
    bail_error(err);

    if( tstr_array_length_quick(divert_accept_rules) > 0 && tstr_array_length_quick(divert_mark_rules) > 0) {
	*is_existing = true;
    }

bail:
    tstr_array_free(&divert_accept_rules);
    tstr_array_free(&divert_mark_rules);
    return err;
}

static int
md_nvsd_http_ipt_get_rules(md_commit *commit ,const mdb_db *db, const char *match_node,
	const char *match_val, tstr_array **match_rules) {
    int err = 0;
    tstr_array *ipt_tgt = NULL;
    uint16_t i = 0;
    const char *binding_name = NULL;
    tstring *t_tgt_name = NULL;

    err = mdb_get_matching_tstr_array(commit,
	    db,
	    match_node,
	    0,
	    &ipt_tgt);
    bail_error(err);

    err = tstr_array_new (match_rules , NULL);
    bail_error(err);

    for( i = 0; i < tstr_array_length_quick(ipt_tgt) ; i++) {
	tstr_array *node_parts = NULL;
	const char *rule_num = NULL;
	binding_name = tstr_array_get_str_quick(
		ipt_tgt, i);
	bail_null(binding_name);


	if(match_val) {
	    err = mdb_get_node_value_tstr(commit, db, binding_name, 0,
		    NULL, &t_tgt_name);
	    bail_error_null(err, t_tgt_name);


	    if(ts_equal_str(t_tgt_name, match_val, false)) {
		/*Get the rule num
		 * Call function to see if it has the same intf_name, port and iP
		 */

		err = lf_path_get_components (binding_name, &node_parts, NULL, NULL);
		bail_error(err);

		rule_num = tstr_array_get_str_quick(node_parts, 7);
		bail_null(rule_num);

		err = tstr_array_append_str(*match_rules, rule_num);
		bail_error (err);

	    }
	} else {
	    err = lf_path_get_components (ts_str(t_tgt_name), &node_parts, NULL, NULL);
	    bail_error(err);

	    rule_num = tstr_array_get_str_quick(node_parts, 7);
	    bail_null(rule_num);

	    err = tstr_array_append_str(*match_rules, rule_num);
	    bail_error (err);
	}
	tstr_array_free(&node_parts);
	ts_free(&t_tgt_name);
    }

bail:
    ts_free(&t_tgt_name);
    tstr_array_free(&ipt_tgt);
    return err;
}

static int
md_nvsd_http_get_rule_matches(md_commit *commit ,const mdb_db *db, const char *rule_num,
						    ipt_match_params_t *match_params) {
    int err = 0;
    node_name_t tmp_nd = {0};
    tstring *t_intf_name = NULL;
    tstring *t_proto = NULL;
    tstring *t_tgt = NULL;

    snprintf(tmp_nd, sizeof(tmp_nd),
	    "/iptables/state/table/mangle/chain/PREROUTING/rule/%s/match/intf_in/name_pattern",
	    rule_num);

    err = mdb_get_node_value_tstr(commit, db, tmp_nd, 0,
	    NULL, &t_intf_name);
    bail_error(err);
    if(t_intf_name)
	snprintf(match_params->intf_name, sizeof(match_params->intf_name),
		"%s", ts_str(t_intf_name));

    snprintf(tmp_nd, sizeof(tmp_nd),
	    "/iptables/state/table/mangle/chain/PREROUTING/rule/%s/match/protocol",
	    rule_num);

    err = mdb_get_node_value_tstr(commit, db, tmp_nd, 0,
	    NULL, &t_proto);
    bail_error(err);

    if(t_proto)
	snprintf(match_params->proto, sizeof(match_params->proto),
		"%s", ts_str(t_proto));

    snprintf(tmp_nd, sizeof(tmp_nd),
	    "/iptables/state/table/mangle/chain/PREROUTING/rule/%s/match/dst/port_max",
	    rule_num);

    err = mdb_get_node_value_uint16(commit, db, tmp_nd, 0,
	    NULL, &match_params->dport_min);
    bail_error(err);

    snprintf(tmp_nd, sizeof(tmp_nd),
	    "/iptables/state/table/mangle/chain/PREROUTING/rule/%s/match/dst/port_min",
	    rule_num);

    err = mdb_get_node_value_uint16(commit, db, tmp_nd, 0,
	    NULL, &match_params->dport_max);
    bail_error(err);

    snprintf(tmp_nd, sizeof(tmp_nd),
	    "/iptables/state/table/mangle/chain/PREROUTING/rule/%s/target/name",
	    rule_num);

    err = mdb_get_node_value_tstr(commit, db, tmp_nd, 0,
	    NULL, &t_tgt);
    bail_error(err);

    if(t_tgt)
	snprintf(match_params->tgt, sizeof(match_params->tgt),
		"%s", ts_str(t_tgt));

bail:
    ts_free(&t_intf_name);
    ts_free(&t_proto);
    ts_free(&t_tgt);
    return err;
}

static int
md_nvsd_http_delete_divert_chain(md_commit *commit, const mdb_db *db)
{

    int status = 0;
    int err = 0;

    err = lc_launch_quick_status(&status, NULL, false, 11 ,
	    "/sbin/iptables", "-t", "mangle", "-D", "PREROUTING",
	    "-p", "icmp","-m","socket", "-j", "DIVERT");
    bail_error(err);
    IPT_COMPLAIN_STATUS(status, "Deleting PREROUTING icmp rule failed");

    err = lc_launch_quick_status(&status, NULL, false, 11 ,
	    "/sbin/iptables", "-t", "mangle", "-D", "PREROUTING",
	    "-p", "tcp","-m","socket", "-j", "DIVERT");
    bail_error(err);
    IPT_COMPLAIN_STATUS(status, "Deleting PREROUTING tcp rule failed");

    err = lc_launch_quick_status(&status, NULL, false, 5,
	    "/sbin/iptables", "-t", "mangle", "-F", "DIVERT");
    bail_error(err);
    IPT_COMPLAIN_STATUS(status, "Deleting DIVERT chain failed");

    err = lc_launch_quick_status(&status, NULL, false, 5,
	    "/sbin/iptables", "-t", "mangle", "-X", "DIVERT");
    bail_error(err);
    IPT_COMPLAIN_STATUS(status, "Deleting Divert chain failed");

    /*   Remove below rules
	 /sbin/iptables -t mangle -D PREROUTING -p tcp -m socket -j DIVERT
	 /sbin/iptables -t mangle -F DIVERT
	 /sbin/iptables -t mangle -X DIVERT
     */

bail:
    return err;

}

static int
md_nvsd_http_add_divert_chain(md_commit *commit, const mdb_db *db)
{
    int status = 0;
    int err = 0;
    tbool is_existing = false;

    err = md_nvsd_http_ipt_DIVERT_rules(commit ,db, &is_existing);
    bail_error(err);

    if(is_existing)
	goto bail;

    err = lc_launch_quick_status(&status, NULL, false, 5,
	    "/sbin/iptables", "-t", "mangle", "-N", "DIVERT");
    bail_error(err);
    IPT_COMPLAIN_STATUS(status, "Adding Divert chain failed");

    err = lc_launch_quick_status(&status, NULL, false, 11 ,
	    "/sbin/iptables", "-t", "mangle", "-A", "PREROUTING",
	    "-p", "icmp","-m","socket", "-j", "DIVERT");
    bail_error(err);
    IPT_COMPLAIN_STATUS(status, "Adding PREROUTING ICMP rule failed");

    err = lc_launch_quick_status(&status, NULL, false, 11 ,
	    "/sbin/iptables", "-t", "mangle", "-A", "PREROUTING",
	    "-p", "tcp","-m","socket", "-j", "DIVERT");
    bail_error(err);
    IPT_COMPLAIN_STATUS(status, "Adding PREROUTING tcp rule failed");


    err = lc_launch_quick_status(&status, NULL, false, 9 ,
	    "/sbin/iptables", "-t", "mangle", "-A", "DIVERT",
	    "-j", "MARK","--set-mark","1");
    bail_error(err);
    IPT_COMPLAIN_STATUS(status, "Adding Mark rule failed");

    err = lc_launch_quick_status(&status, NULL, false, 7,
	    "/sbin/iptables", "-t", "mangle", "-A", "DIVERT",
	    "-j", "ACCEPT");
    bail_error(err);
    IPT_COMPLAIN_STATUS(status, "Adding Divert accpet rule failed");

    /*   Add below rules
	 /sbin/iptables -t mangle -N DIVERT
	 /sbin/iptables -t mangle -A PREROUTING -p tcp -m socket -j DIVERT
	 /sbin/iptables -t mangle -A DIVERT -j MARK --set-mark 1
	 /sbin/iptables -t mangle -A DIVERT -j ACCEPT
     */
bail:
    return err;
}

static int
md_nvsd_delete_ipfilter(md_commit *commit, const mdb_db *db,
		 const bn_binding *binding, void *arg)
{
    int err = 0;
    int status = 0;

    ip_table_ref_cnt--;

    if(ip_table_ref_cnt == 0 ) {
	err = md_nvsd_http_delete_divert_chain(commit, db);
	bail_error(err);
    }

    err = md_nvsd_handle_ipfilter(commit, db, binding, arg, 0);
    bail_error(err);

bail:
    return(err);
}
static int
md_nvsd_add_ipfilter(md_commit *commit, const mdb_db *db,
		 const bn_binding *binding, void *arg)
{
    int err = 0;
    int status = 0;
    if(ip_table_ref_cnt == 0 ) {
	err = md_nvsd_http_add_divert_chain(commit, db);
	bail_error(err);
    }
    ip_table_ref_cnt++;
    err = md_nvsd_handle_ipfilter(commit, db, binding, arg, 1);
    bail_error(err);

bail:
    return(err);
}

static int
md_nvsd_intf_ipfilter(md_commit *commit, const mdb_db *db,
		const bn_binding *interface, const bn_binding *port,int add, int delete)
{
    int err = 0;
    char *intf_name = NULL;
    const char *node = NULL;
    uint16 port_num = 0;
    void * cb_arg = NULL;
    mdb_iterate_binding_cb_func iterate_cb;

    if ( interface) {
	bn_binding_dump(jnpr_log_level, "md_nvsd_intf_ipfilter-1", interface);
	err = bn_binding_get_str(interface, ba_value, bt_string, NULL, &intf_name);
	bail_error_null(err, intf_name);

	cb_arg = (void *)intf_name;
	node = "/nkn/nvsd/http/config/server_port/*/port";
	if (add) {
	    iterate_cb = md_nvsd_add_ipfilter;
	} else {
	    iterate_cb = md_nvsd_delete_ipfilter;
	}
    } else if (port) {
	bn_binding_dump(jnpr_log_level, "md_nvsd_intf_ipfilter-2 ", port);
	err = bn_binding_get_uint16(port, ba_value, NULL, &port_num);
	bail_error(err);
	node = "/nkn/nvsd/http/config/t-proxy/*";
	cb_arg = &port_num;
	if (add) {
	    iterate_cb = md_nvsd_add_ipfilter;
	} else if (delete) {
	    iterate_cb = md_nvsd_delete_ipfilter;
	}
    } else {
	err = 1;
	bail_error(err);
    }
    err = mdb_foreach_binding_cb (commit, db, node, 0, iterate_cb, cb_arg);
    bail_error(err);

bail:
    safe_free(intf_name);
    return(err);
}


int
md_nvsd_http_synch_ipt(md_commit *commit,const mdb_db *db) {

    int err = 0;
    tstr_array *tproxy_rules = NULL;
    uint16_t i = 0;
    ipt_match_params_t match_params;
    tbool is_tproxy_configured = false;
    tstr_array *ts_port = NULL;
    tstr_array *ts_intf = NULL;
    tbool is_existing = false;
    char *ip_address = NULL;

    memset(&match_params, 0 ,sizeof(ipt_match_params_t));

    snprintf(match_params.tgt, sizeof(match_params.tgt), "%s",
			"TPROXY");

    err = md_nvsd_http_ipt_get_rules(commit, db,
			"/iptables/state/table/mangle/chain/PREROUTING/rule/*/target/name",match_params.tgt,
	    &tproxy_rules);
    bail_error(err);

    err = mdb_get_matching_tstr_array_attrib
	(commit, db,"/nkn/nvsd/http/config/server_port/*/port" , 0, ba_value,
	 &ts_port);
    bail_error(err);

    err = mdb_get_matching_tstr_array_attrib
	(commit, db, "/nkn/nvsd/http/config/t-proxy/*", 0, ba_value,
	 &ts_intf);
    bail_error(err);

    for(i = 0; i < tstr_array_length_quick(tproxy_rules); i++ ) {
	const char *rule_num = NULL;
	tbool port_found = true, intf_found = true;
	ipt_match_params_t ret_match_params;
	rule_num = tstr_array_get_str_quick(tproxy_rules, i);
	bail_null(rule_num);

	memset(&ret_match_params, 0 ,sizeof(ipt_match_params_t));

	err = md_nvsd_http_get_rule_matches(commit, db, rule_num, &ret_match_params);
	bail_error(err);

	/* check if the rule is present in the config as well */

	if( ts_port) {
	    str_value_t dport = {0};
	    uint32_t ret_index1 = (uint32_t) -1;

	    snprintf(dport ,sizeof(dport), "%d", ret_match_params.dport_min);
	    lc_log_basic(jnpr_log_level, "the dport to be seached is %s", dport);
	    err = tstr_array_linear_search_str(ts_port, dport, 0, &ret_index1);

	    if(err == lc_err_not_found) {
		port_found = false;
	    } else {
		bail_error(err);
	    }
	}

	if( ts_intf) {
	    uint32_t ret_index1 = (uint32_t)-1;
	    err = tstr_array_linear_search_str(ts_intf, ret_match_params.intf_name, 0, &ret_index1);
	    lc_log_basic(jnpr_log_level, "the intf to be seached is %s", ret_match_params.intf_name);

	    if(err == lc_err_not_found) {
		intf_found = false;
	    } else {
		bail_error(err);
	    }

	}

	if(port_found && intf_found) {
	    is_tproxy_configured = true;
	}

	/* delete the rule*/
	if(port_found == false || intf_found == false) {
	    int status = 0;
	    str_value_t dport = {0};

	    err = md_nvsd_http_get_intf_ip(commit, db, ret_match_params.intf_name, &ip_address);
	    bail_error(err);

	    lc_log_basic(jnpr_log_level, "Clearing rule: PREROUTING, intf : %s,"
				    "dport : %s,",ret_match_params.intf_name,
				    dport);

	    snprintf(dport, sizeof(dport), "%d", ret_match_params.dport_min);
	    err = lc_launch_quick_status(&status, NULL, false, 19 ,
		    "/sbin/iptables", "-t", "mangle", "-D", "PREROUTING",
		    "-i",ret_match_params.intf_name, "-p", "tcp",
		    "--dport", dport, "-j", "TPROXY",
		    "--tproxy-mark", "0x1/0x1", "--on-port", dport,
		    "--on-ip", ip_address);
	    bail_error(err);

	    IPT_COMPLAIN_STATUS(status, "Clearing unwanted rule failed");
	    //Complain on wrong status
	    safe_free(ip_address);


	}

    }

    err = md_nvsd_http_ipt_DIVERT_rules(commit ,db, &is_existing);
    bail_error(err);

    if(is_existing && !is_tproxy_configured) {
	lc_log_basic(LOG_NOTICE, "Divert rules existing and tprooxy is not configured.Clearing Divert rules");
	ip_table_ref_cnt--;
        if(ip_table_ref_cnt == 0 ) {
		err = md_nvsd_http_delete_divert_chain(commit, db);
		bail_error(err);
	}
    }



bail:
    tstr_array_free(&tproxy_rules);
    tstr_array_free(&ts_port);
    tstr_array_free(&ts_intf);
    safe_free(ip_address);
    return err;
}

static int
md_nvsd_http_get_intf_ip(md_commit *commit, const mdb_db *db, const char *intf_name,
								char **ip_address) {
    int err = 0;
    str_value_t bn_name = {0};
    *ip_address = NULL;
    bn_binding *ip_binding = NULL;

    snprintf(bn_name, sizeof(bn_name), "/net/interface/state/%s/addr/ipv4/1/ip", intf_name);

    err = mdb_get_node_binding(commit, db, bn_name, 0, &ip_binding);
    bail_error(err);

    if (ip_binding == NULL) {
	/* we don't have ip address set for this interface, ignore this */
	err = 0;
	goto bail;
    }
    err = bn_binding_get_str(ip_binding, ba_value, bt_ipv4addr, NULL, ip_address);
    bail_error(err);

    if (*ip_address == NULL) {
	/* we don't have ip address set for this interface */
	err = 0;
	goto bail;
    }

bail:
    bn_binding_free(&ip_binding);
    return err;

}

/* ------------------------------------------------------------------------- */
static int
md_nvsd_http_commit_apply(md_commit *commit,
                     const mdb_db *old_db, const mdb_db *new_db,
                     mdb_db_change_array *change_list, void *arg)
{
    int err = 0, port_rules_handled = 0;
    int num_changes = 0, i = 0, intf_rules_handled = 0;
    const mdb_db_change *change = NULL;
    bn_binding *binding = NULL;

    static int synch_needed = 0;



    num_changes = mdb_db_change_array_length_quick(change_list);
    for (i = 0; i< num_changes; i++) {
	change = mdb_db_change_array_get_quick(change_list, i);
	if (!change) { continue; }

	/* only interested in add/delete */
	if (change->mdc_change_type == mdct_add) {
	    if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6,
			    "nkn", "nvsd", "http", "config", "t-proxy", "*")
			&& (0 == port_rules_handled))) {
		/* add rule for all listening ports  */
		lc_log_basic(jnpr_log_level, "CHANGE ADD - %s", ts_str(change->mdc_name));
		err = mdb_get_node_binding(commit, new_db, ts_str(change->mdc_name), 0, &binding);
		bail_error(err);
		if (binding) {
		    err = md_nvsd_intf_ipfilter(commit, new_db, binding, NULL, 1 , 0);
		    bn_binding_free(&binding);
		    bail_error(err);
		    intf_rules_handled = 1;
		}
	    }
	    if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7, 
			    "nkn", "nvsd", "http", "config", "server_port", "*", "port"))
		    && (0 == intf_rules_handled)){
		lc_log_basic(jnpr_log_level, "CHANGE ADD - %s", ts_str(change->mdc_name));
		/* add rule for this port for all interfaces */
		err = mdb_get_node_binding(commit, new_db, ts_str(change->mdc_name), 0, &binding);
		bail_error(err);
		if (binding) {
		    err = md_nvsd_intf_ipfilter(commit, new_db, NULL, binding, 1 , 0);
		    bn_binding_free(&binding);
		    bail_error(err);
		    port_rules_handled = 1;
		}
	    }
	    if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6, "nkn", "nvsd", "http", "config", "interface", "*"))){
		tstring *ret_msg = NULL;
		bn_request *req = NULL;
		uint16_t ret_code = 0;

		const char *t_name = tstr_array_get_str_quick(change->mdc_name_parts, 5);
		bail_null(t_name);

		err = bn_event_request_msg_create
		    (&req, "/nkn/nvsd/http/notify/delivery_intf_add");
		bail_error(err);

		err = bn_event_request_msg_append_new_binding
		    (req, 0, "interface", bt_string,
		     t_name, NULL);
		bail_error(err);

		err = md_commit_handle_event_from_module
		    (commit, req, &ret_code, &ret_msg, NULL, NULL);
		if(ret_msg)
		    complain_error_msg(err, "%s", ts_str(ret_msg));

		lc_log_basic(LOG_NOTICE, "Interface %s added to delivery", t_name);

		bn_request_msg_free(&req);
		ts_free(&ret_msg);
	    }

	} else {
	    if (((bn_binding_name_parts_pattern_match_va (change->mdc_name_parts, 0, 6, "nkn", "nvsd", "http", "config", "ipv6", "enabled"))
			&& (mdct_modify == change->mdc_change_type)))
	    {
		err = md_commit_set_return_status_msg(commit, 0,
			"Information:\n\tIf media delivery service has to use IPv6 then\n\tplease restart mod-delivery,mod-ssl using the 'service restart' command");
	    }
	    /* its a deletion */
	    if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6,
			    "nkn", "nvsd", "http", "config", "t-proxy", "*")
			&& (0 == port_rules_handled))) {
		/* delete rule for all listening ports  */
		lc_log_basic(jnpr_log_level, "CHANGE DEL- %s", ts_str(change->mdc_name));
		err = mdb_get_node_binding(commit, old_db, ts_str(change->mdc_name), 0, &binding);
		bail_error(err);
		if (binding) {
		    bn_binding_dump(jnpr_log_level, "md_nvsd_http_commit_apply- 9",binding);
		    err = md_nvsd_intf_ipfilter(commit, old_db, binding, NULL, 0 , 1);
		    bn_binding_free(&binding);
		    bail_error(err);
		    intf_rules_handled = 1;
		}
	    }
	    if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7,
			    "nkn", "nvsd", "http", "config", "server_port", "*", "port"))
		    && (0 == intf_rules_handled)) {
		lc_log_basic(jnpr_log_level, "CHANGE DEL- %s", ts_str(change->mdc_name));
		/* add rule for this port for all interfaces */
		err = mdb_get_node_binding(commit, old_db, ts_str(change->mdc_name), 0, &binding);
		bail_error(err);
		if (binding) {
		    bn_binding_dump(jnpr_log_level, "md_nvsd_http_commit_apply- 7",binding);
		    err = md_nvsd_intf_ipfilter(commit, old_db, NULL, binding, 0 , 1);
		    bn_binding_free(&binding);
		    bail_error(err);
		    port_rules_handled = 1;
		}
	    }
	    if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6, "nkn", "nvsd", "http", "config", "interface", "*"))){
		tstring *ret_msg = NULL;
		bn_request *req = NULL;
		uint16_t ret_code = 0;
		const char *t_name = tstr_array_get_str_quick(change->mdc_name_parts, 5);
		bail_null(t_name);

		err = bn_event_request_msg_create
		    (&req, "/nkn/nvsd/http/notify/delivery_intf_del");
		bail_error(err);

		err = bn_event_request_msg_append_new_binding
		    (req, 0, "interface", bt_string,
		     t_name, NULL);
		bail_error(err);

		err = md_commit_handle_event_from_module
		    (commit, req, &ret_code, &ret_msg, NULL, NULL);
		if(ret_msg)
		    complain_error_msg(err, "%s", ts_str(ret_msg));

		lc_log_basic(LOG_NOTICE, "Interface %s added to delivery", t_name);

		bn_request_msg_free(&req);
		ts_free(&ret_msg);

	    }

	}
    }

    //Try  synching with ip tables here
    if(!synch_needed)
	md_nvsd_http_synch_ipt(commit, new_db);
    synch_needed = 1;

bail:
    return(err);
}

static int
md_nvsd_http_upgrade_downgrade(const mdb_db *old_db,
                              mdb_db *inout_new_db,
                              uint32 from_module_version,
                              uint32 to_module_version,
                              void *arg)
{
    int err = 0;
    tbool found = false;
    tstring *port = NULL;
    tstring *method = NULL;
    tstring *method_name = NULL;
    tstr_array *ts_methods = NULL;
    int i = 0;
    int num_methods = 0;
    tstr_array *name_parts = NULL;
    char *node_name = NULL;
    tstring *uppercase_name = NULL;
    tstring *t_req_size = NULL;

    err = md_generic_upgrade_downgrade(old_db, inout_new_db, from_module_version,
	    to_module_version, arg);
    bail_error(err);

    if ( from_module_version == 4 && to_module_version == 5) {
	bn_str_value binding_value[2];

	/* Read the existing server_port value  and move to
	 * the wild card node */
	err = mdb_get_node_value_tstr(NULL, inout_new_db, "/nkn/nvsd/http/config/server_port", 0, &found, &port);
	bail_error(err);


	binding_value[0].bv_name = "/nkn/nvsd/http/config/server_port/1";
	binding_value[0].bv_type = bt_uint16;
	binding_value[0].bv_value = "1";

	binding_value[1].bv_name = "/nkn/nvsd/http/config/server_port/1/port";
	binding_value[1].bv_type = bt_uint16;
	binding_value[1].bv_value = ts_str(port) ;

	err = mdb_create_node_bindings(NULL, inout_new_db,
		binding_value, sizeof(binding_value)/sizeof(bn_str_value));
	bail_error(err);
    }

    if ( from_module_version == 9 && to_module_version == 10) {

	err = mdb_get_matching_tstr_array(NULL,
		inout_new_db,
		"/nkn/nvsd/http/config/allow_req/req_method/*",
		0,
		&ts_methods);
	bail_error(err);

	num_methods = tstr_array_length_quick(ts_methods);
	if (!ts_methods || (num_methods <= 0))
	    goto bail;

	for (i = 0; i < num_methods; i++) {
	    found = false;
	    node_name = smprintf("%s/method", tstr_array_get_str_quick(ts_methods, i));
	    bail_null(node_name);

	    err = mdb_get_node_value_tstr(NULL,
		    inout_new_db,
		    node_name,
		    0,
		    &found,
		    &method_name);
	    bail_error(err);

	    // defensive code.. this should not happen..
	    if(!found || (method_name == NULL) || (ts_length(method_name) == 0)) {
		lc_log_basic(LOG_NOTICE,
			"UG (9 -> 10): method name read as null when node was valid (%s)",
			node_name);
		safe_free(node_name);
		continue;
	    }

	    /* Always convert the name to uppercase while upgrading */
	    err = ts_dup(method_name, &uppercase_name);
	    bail_error(err);

	    err = ts_trans_uppercase(uppercase_name);
	    bail_error(err);

	    err = mdb_reparent_node(NULL,
		    inout_new_db,
		    bsso_reparent,
		    0,
		    node_name, //tstr_array_get_str_quick(ts_methods, i),
		    "/nkn/nvsd/http/config/allow_req/method",
		    ts_str(uppercase_name),
		    true);
	    safe_free(node_name);
	    bail_error(err);

	    ts_free(&uppercase_name);
	}
    }
    if ( from_module_version == 15 && to_module_version == 16) {

	err = mdb_get_node_value_tstr(NULL, inout_new_db, "/nkn/nvsd/http/config/max_http_req_size", 0, &found, &t_req_size);
	bail_error_null(err, t_req_size);

	if (atoi(ts_str(t_req_size)) > 8192){
	    err = mdb_set_node_str(NULL,
		    inout_new_db,
		    bsso_modify,
		    0,
		    bt_uint32,
		    "8192",
		    "%s",
		    "/nkn/nvsd/http/config/max_http_req_size");
	    bail_error(err);
	}
    }



bail:
    tstr_array_free(&ts_methods);
    ts_free(&method_name);
    ts_free(&uppercase_name);
    ts_free(&port);
    safe_free(node_name);
    ts_free(&t_req_size);
    return err;
}

static int
md_nvsd_ev_handle_ipfilter(md_commit *commit, const mdb_db *db,
		char * ip_addr, char *ifname, int add, int delete)
{
    int err = 0, num_ports = 0, status = 0, i = 0;
    char port_string[64] = {0};
    const tstring *port = NULL;
    bn_binding_array *port_array = NULL;
    bn_binding *port_binding = NULL;
    uint16 port_num = 0;

    bail_null(ip_addr);
    bail_null(ifname);

    /* iterate over all listenig ports and take the action */
    err = mdb_iterate_binding(commit, db,
	    "/nkn/nvsd/http/config/server_port",
	    mdqf_sel_iterate_subtree, &port_array);
    bail_error(err);

    num_ports = bn_binding_array_length_quick(port_array);

    lc_log_basic(jnpr_log_level, "%s: NUMBER of pors configured %d", __FUNCTION__, num_ports);
    bn_binding_array_dump(__FUNCTION__, port_array, jnpr_log_level);
    for (i = 0; i< num_ports; i++) {
	err = bn_binding_array_get(port_array, i, &port_binding);
	bail_error_null(err, port_binding);

	port = bn_binding_get_name_quick(port_binding);
	bail_null(port);
	if (bn_binding_name_pattern_match(ts_str(port),
		    "/nkn/nvsd/http/config/server_port/*/port")){
	    /* get the value of port */
	    err = bn_binding_get_uint16(port_binding, ba_value, NULL, &port_num);
	    bail_error(err);
	    snprintf(port_string, sizeof(port_string), "%d", port_num);
	    if (delete) {

		ip_table_ref_cnt--;

		if(ip_table_ref_cnt == 0 ) {
		    err = md_nvsd_http_delete_divert_chain(commit, db);
		    complain_error_msg(err, "error deleting divert rule");
		}

		err = lc_launch_quick_status(&status, NULL, false, 19 ,
			"/sbin/iptables", "-t", "mangle", "-D", "PREROUTING",
			"-i", ifname, "-p", "tcp",
			"--dport", port_string, "-j", "TPROXY",
			"--tproxy-mark", "0x1/0x1", "--on-port", port_string,
			"--on-ip", ip_addr);
		lc_log_basic(jnpr_log_level, "%s: got status delet 2- %d", __FUNCTION__, status);
		bail_error(err);
		if (status ) {
		    err = 1;
		    goto bail;
		}
	    } else if (add) {
		/* delete first */
		tbool is_existing = false;
		ipt_match_params_t ip_match_params;
		memset(&ip_match_params, 0 ,sizeof(ipt_match_params_t));

		snprintf(ip_match_params.intf_name, sizeof(ip_match_params.intf_name),"%s",
			ifname);

		ip_match_params.dport_min = port_num;
		ip_match_params.dport_max = port_num;
		snprintf(ip_match_params.tgt, sizeof(ip_match_params.tgt),"%s",
			"TPROXY");
		snprintf(ip_match_params.proto, sizeof(ip_match_params.proto),"%s",
			"tcp");


		err = md_nvsd_http_ipt_has_TPXY_rules(commit, db, &ip_match_params, &is_existing);
		bail_error(err);



		if(ip_table_ref_cnt == 0 ) {
		    err = md_nvsd_http_add_divert_chain(commit, db);
		    complain_error_msg(err, "error adding divert rule");
		}
		ip_table_ref_cnt++;
		
		if(is_existing)
		    goto bail;

		lc_log_basic(jnpr_log_level, "%s: got status delet 1- %d", __FUNCTION__, status);
		err = lc_launch_quick_status(&status, NULL, false, 19 ,
			"/sbin/iptables", "-t", "mangle", "-A", "PREROUTING",
			"-i", ifname, "-p", "tcp",
			"--dport", port_string, "-j", "TPROXY",
			"--tproxy-mark", "0x1/0x1", "--on-port", port_string,
			"--on-ip", ip_addr);
		lc_log_basic(jnpr_log_level, "%s: got status - %d", __FUNCTION__, status);
		bail_error(err);
		if (status ) {
		    err = 1;
		    goto bail;
		}
	    }
	}
    }
bail:
    bn_binding_array_free(&port_array);
    return err;
}
static int
md_nvsd_handle_intf_change_event(md_commit *commit, const mdb_db *db,
		const char* event_name, const bn_binding_array *bindings,
		void *cb_reg_arg, void *cb_arg)
{
    int err = 0, has_old_ip = 0, has_new_ip = 0;
    char tproxy_if[256] = {0};
    const bn_binding *old_ip_binding = NULL, *new_ip_binding = NULL,
	  *binding = NULL;
    tstring *interface = NULL;
    char *old_ip = NULL, *new_ip = NULL, *ifname = NULL;
    tbool found = false;


    bail_null(event_name);
    bail_require(!strcmp(event_name, "/net/interface/events/state_change"));
    bail_null(bindings);

    bn_binding_array_dump(__FUNCTION__, bindings, jnpr_log_level);

    err = bn_binding_array_get_binding_by_name(bindings, "ifname", &binding);
    bail_error_null(err, binding);

    err = bn_binding_get_str(binding, ba_value, 0, NULL, &ifname);
    bail_error_null(err, ifname);

    snprintf(tproxy_if, sizeof(tproxy_if), "/nkn/nvsd/http/config/t-proxy/%s", ifname);
    err = mdb_get_node_value_tstr(commit, db, tproxy_if, 0, &found, &interface);
    bail_error(err);

    if (found == false) {
	/* interface is not configured for tproxy, so we don't need to do anything */
	lc_log_basic(jnpr_log_level, "%s: %s is not configured for t-proxy", __FUNCTION__, ifname);
	goto bail;
    }

    err = bn_binding_array_get_binding_by_name(bindings, "old/addr/ipv4/1/ip", &old_ip_binding);
    bail_error(err);

    err = bn_binding_array_get_binding_by_name(bindings, "new/addr/ipv4/1/ip", &new_ip_binding);
    bail_error(err);

    if (old_ip_binding == NULL && new_ip_binding == NULL) {
	/* both are NULL, not ip change */
	goto bail;
    }
    if (((old_ip_binding != NULL) && (new_ip_binding == NULL))
	    || ((old_ip_binding == NULL) && (new_ip_binding != NULL))) {
	/* THIS MUST NOT HAVE HAPPENED */
	goto bail;
    }
    if (old_ip_binding != NULL) {
	err = bn_binding_get_str(old_ip_binding, ba_value, 0, NULL, &old_ip);
	bail_error_null(err, old_ip);
    } else {
	lc_log_basic(jnpr_log_level, "%s: OLD BINDING NULL", __FUNCTION__);
    }

    if (new_ip_binding != NULL) {
	err = bn_binding_get_str(new_ip_binding, ba_value, 0, NULL, &new_ip);
	bail_error_null(err, new_ip);
    } else {
	lc_log_basic(jnpr_log_level, "%s: NEW BINDING NULL", __FUNCTION__);
    }

    /* old_ip && new_ip cannot be NULL */
    if (0 == strcmp(old_ip, "0.0.0.0")) {
	lc_log_basic(jnpr_log_level, "%s: NO OLD IPADDR ", __FUNCTION__);
    } else {
	/* we must remove the old_ip rules */
	err = md_nvsd_ev_handle_ipfilter(commit, db, old_ip, ifname, 0, 1);
	bail_error(err);
    }
    if (0 == strcmp(new_ip, "0.0.0.0")) {
	lc_log_basic(jnpr_log_level, "%s: NO NEW IPADDR ", __FUNCTION__);
    } else {
	/* we must add the new_ip rules */
	err = md_nvsd_ev_handle_ipfilter(commit, db, new_ip, ifname, 1, 0);
	bail_error(err);
    }

bail:
    safe_free(ifname);
    safe_free(new_ip);
    safe_free(old_ip);
    ts_free(&interface);
    return err;
}
int
md_nvsd_http_side_effects( md_commit *commit,
		const mdb_db *old_db,
		mdb_db *inout_new_db,
		mdb_db_change_array *change_list,
		void *arg)
{
    int done = 0, i = 0, err =0;
    tbool one_shot = false, initial = false;
    int num_changes = 0, calulate_bandwidth = 0;
    const mdb_db_change *change = NULL;
    char global_conf_bw_buf[256] = {0};
    const char *node_total_conf = "/nkn/nvsd/resource_mgr/total/conf_max_bw";

    err = md_commit_get_commit_mode(commit, &one_shot);
    bail_error(err);
    if (one_shot) {
	/* don't do anything here */
	goto bail;
    }

    err = md_commit_get_commit_initial(commit, &initial);
    bail_error(err);
    if (initial) {
	/* calculate the bandwidth */
	goto bail_6;
    }
    num_changes = mdb_db_change_array_length_quick(change_list);
    for(i = 0; i < num_changes; i++) {
	change = mdb_db_change_array_get_quick(change_list, i);
	bail_null(change);

	if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6, "nkn", "nvsd", "http", "config", "interface", "*"))) {
	    goto bail_6;
	}
	if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6,
		    "nkn", "nvsd", "http", "config", "t-proxy", "*") && (mdct_add == change->mdc_change_type)) {

	    err = mdb_set_node_str(commit, inout_new_db, bsso_modify, 0, bt_bool, "true",
		    "/nkn/nvsd/network/config/ip-tables");
	    bail_error(err);

	}

    }

bail_6:
    if (calulate_bandwidth == 1) {
	uint64_t config_bw = 0, old_total_config_bw = 0;
	/* don't account for interface up-down */
	err = md_lib_cal_tot_avail_bw(commit, inout_new_db, &config_bw, false);
	lc_log_basic(jnpr_log_level, "%s: Done setting total bw, err = %d, vla - %lu", __FUNCTION__, err, config_bw);
	bail_error(err);
	err = mdb_get_node_value_uint64(commit, inout_new_db, node_total_conf, 0, NULL, &old_total_config_bw);
	bail_error(err);
	/* GBYTES = 1000 * 1000 * 1000, config_bw before calculation will contain Mbps */
	config_bw = (config_bw/10) * (GBYTES /800);
	if ((config_bw == old_total_config_bw) && (old_total_config_bw !=0)) {
	    /* don't do anything */
	} else {
	    snprintf(global_conf_bw_buf, sizeof(global_conf_bw_buf), "%lu", config_bw);
	    err = mdb_set_node_str(commit, inout_new_db, bsso_modify, 0, bt_uint64,global_conf_bw_buf, "%s", node_total_conf);
	    bail_error(err);
	    err = (*md_nvsd_update_global_bw)(commit, inout_new_db, 0, 0 , true );
	    bail_error(err);
	}
    }

bail:
    return err;
}

int
is_internal_intf(const char *name, tbool* found);
int
is_internal_intf(const char *name, tbool* found)
{
    if (name == NULL || found == NULL) {
	return EINVAL;
    }
    *found = false;
    if(!strcmp(name, "lo") || !strcmp(name, "sit0") || !strncmp(name, "virbr", 5)) {
	*found = true;
    }
    return 0;
}


int
md_lib_intf_is_bonded(md_commit * commit, const mdb_db *db, const char *intf_name, tbool *found);
int
md_lib_intf_is_bonded(md_commit * commit, const mdb_db *db, const char *intf_name, tbool *found)
{
    int err = 0, i = 0;
    bn_binding_array *bindings = NULL;
    bn_binding *binding = NULL;
    const tstring *bn_string = NULL;
    tstring *ret_val = NULL;
    int num_intf = 0;
    tbool intf_found = false;
    char tmp_buf[256] = {0};

    bail_null(intf_name);
    bail_null(found);

    *found = false;
    err = mdb_iterate_binding(commit, db, "/net/bonding/config", 0, &bindings);
    bail_error(err);
    num_intf = bn_binding_array_length_quick(bindings);
    if (num_intf == 0) {
	/* no bond interfaces, not bound to a bond */
	*found = false;
	err = 0;
	goto bail;
    }
    for (i = 0 ; i < num_intf; i++) {
	binding = bn_binding_array_get_quick(bindings, i);
	bail_null(binding);
	bn_string = bn_binding_get_name_quick(binding);
	bail_null(bn_string);

	tmp_buf[0] = '\0';
	snprintf(tmp_buf, sizeof(tmp_buf), "%s/interface/%s", ts_str(bn_string), intf_name);
	lc_log_basic(jnpr_log_level, "%s: checking %s", __FUNCTION__, tmp_buf);
	err = mdb_get_node_value_tstr(commit, db, tmp_buf, 0, &intf_found, &ret_val);
	ts_free(&ret_val);
	bail_error(err);
	if (intf_found == true) {
	    *found = true;
	    err = 0;
	    goto bail;
	}
    }

bail:
    bn_binding_array_free(&bindings);
    ts_free(&ret_val);
    return err;
}
int
md_lib_is_delivery_intf(md_commit * commit, const mdb_db *db, const char *intf_name, tbool *found);
int
md_lib_is_delivery_intf(md_commit * commit, const mdb_db *db, const char *intf_name, tbool *found)
{

    int err = 0, num_intf = 0;
    bn_binding_array *delivery_intf = NULL;
    tstring *ret_val = NULL;
    tbool intf_found = false;
    char tmp_buf[256] = {0};

    bail_null(intf_name);
    bail_null(found);

    *found = false;
    /* delivery interfaces */
    err = mdb_iterate_binding(commit, db, "/nkn/nvsd/http/config/interface", 0, &delivery_intf);
    bail_error(err);
    num_intf = bn_binding_array_length_quick(delivery_intf);
    if (num_intf == 0) {
	/* all http interfaces are allowed */
	*found = true;
	err = 0;
	goto bail;
    }
    snprintf(tmp_buf, sizeof(tmp_buf),"/nkn/nvsd/http/config/interface/%s",intf_name);
    err = mdb_get_node_value_tstr(commit, db, tmp_buf, 0, &intf_found, &ret_val);
    ts_free(&ret_val);
    bail_error(err);
    if (intf_found == true) {
	*found = true;
	err = 0;
	goto bail;
    }

bail:
    bn_binding_array_free(&delivery_intf);
    ts_free(&ret_val);
    return err;
}
int
md_lib_is_bond_interface(md_commit *commit, const mdb_db *db,
		const char *intf_name, tbool * found);

int
md_lib_is_bond_interface(md_commit *commit, const mdb_db *db,
		const char *intf_name, tbool * found)
{
    int err = 0;
    tstring *ret_val = NULL;
    char tmp_buf[256] = {0};
    tbool intf_found = 0;

    snprintf(tmp_buf, sizeof(tmp_buf),"/net/interface/state/%s/devsource",intf_name);
    err = mdb_get_node_value_tstr(commit, db, tmp_buf, 0, &intf_found, &ret_val);
    bail_error(err);
    if (intf_found == false) {
	*found = false;
	goto bail;
    }
    if (ret_val && (0 == strcmp(ts_str(ret_val), "bond"))) {
	*found = true;
    } else {
	*found = false;
    }
bail:
    ts_free(&ret_val);
    return err;
}

int
md_lib_get_phy_intf_speed(md_commit *commit, const mdb_db *db, const char *intf_name,
		uint64_t *intf_conf_bw,tbool count_down_link);

int
md_lib_get_phy_intf_speed(md_commit *commit, const mdb_db *db, const char *intf_name,
		uint64_t *intf_conf_bw,tbool count_down_link)
{
    int err = 0;
    tstring *if_speed = NULL;
    char tmp_buf[256] = {0};
    tbool intf_found = 0;

    if (count_down_link) {
	tbool link_up = false;
	snprintf(tmp_buf, sizeof(tmp_buf),"/net/interface/state/%s/flags/link_up",intf_name);
	err = mdb_get_node_value_tbool(commit, db, tmp_buf, 0, &intf_found, &link_up);
	bail_error(err);
	if (intf_found == false || link_up == false) {
	    /* the node is not available, so set speed to zero */
	    lc_log_basic(jnpr_log_level, "interface (%s), link down ", intf_name);
	    *intf_conf_bw = 0;
	    goto bail;
	}
    }

    /* XXX - should we check for alias interfaces */
    snprintf(tmp_buf, sizeof(tmp_buf),"/net/interface/state/%s/speed",intf_name);
    err = mdb_get_node_value_tstr(commit, db, tmp_buf, 0, &intf_found, &if_speed);
    bail_error(err);
    if (intf_found == false) {
	/* the node is not available, so set speed to zero */
	*intf_conf_bw = 0;
	goto bail;
    }
    if (if_speed ) {
	/* we have got the speed */
	*intf_conf_bw = strtol(ts_str(if_speed), NULL, 10);
    } else {
	*intf_conf_bw = 0;
    }

    lc_log_basic(jnpr_log_level, "%s: got speed - %lu", __FUNCTION__, *intf_conf_bw);
bail:
    ts_free(&if_speed);
    return err;

}
int
md_lib_get_intf_speed(md_commit *commit, const mdb_db *db, const char *intf_name,
		uint64_t *total_conf_bw,tbool count_down_link);
int
md_lib_get_intf_speed(md_commit *commit, const mdb_db *db, const char *intf_name,
		uint64_t *intf_conf_bw,tbool count_down_link)
{
    int err = 0, i =0, j = 0, child_num = 0, num_bindings = 0;
    tbool is_bonded = false, node_found = false;
    uint64_t interface_bw = 0;
    bn_binding_array *bindings= NULL, *child_interface = NULL;
    bn_binding *binding = NULL, *child_binding  =NULL;
    char tmp_buf[256] = {0};
    const tstring *bn_string = NULL, *child_string = NULL;
    tstring *node_val = NULL, *child_val = NULL;

    /* check if this is a bonded interface or not */
    err = md_lib_is_bond_interface(commit, db, intf_name, &is_bonded);
    bail_error(err);

    if (is_bonded == true) {
	/* this is bond interface, we need to sum up all child interfaces */
	err = mdb_iterate_binding(commit, db, "/net/bonding/config", 0, &bindings );
	bail_error(err);
	num_bindings = bn_binding_array_length_quick(bindings);
	if (num_bindings == 0) {
	    /*
	     * this is a error condition, this is bond interface, we have
	     * no bond bindings
	     */
	    *intf_conf_bw = 0;
	    err = 1;
	    goto bail;
	}
	/* get the bonding configuration and sum-up physical interfaces */
	for (i = 0; i < num_bindings; i++) {
	    binding = bn_binding_array_get_quick(bindings, i);
	    if (binding == NULL) {
		continue;
	    }
	    bn_string = bn_binding_get_name_quick(binding);
	    bail_null(bn_string);
	    snprintf(tmp_buf, sizeof(tmp_buf), "%s/name", ts_str(bn_string));
	    err = mdb_get_node_value_tstr(commit, db, tmp_buf, 0, &node_found, &node_val);
	    bail_error(err);
	    if (node_found == false) {
		/* though this must not happen, still we must be handling it */
		ts_free(&node_val);
		continue;
	    } else {
		/* we got the node, find child interfaces */
		tmp_buf[0] = '\0';
		snprintf(tmp_buf, sizeof(tmp_buf), "%s/interface",ts_str(bn_string));
		err = mdb_iterate_binding(commit, db, tmp_buf, 0, &child_interface);
		bail_error(err);
		child_num = bn_binding_array_length_quick(child_interface);
		if (child_num == 0) {
		    /* no interfaces under this bond */
		    *intf_conf_bw = 0;
		    goto bail;
		}
		for (j = 0; j < child_num; j++) {
		    child_binding = bn_binding_array_get_quick(child_interface, j);
		    if (child_binding == NULL) {
			continue;
		    }
		    child_string = bn_binding_get_name_quick(child_binding);
		    bail_null(child_string);
		    err = mdb_get_node_value_tstr(commit, db, ts_str(child_string),
			    0, &node_found, &child_val);
		    bail_error(err);
		    if (node_found == false || child_val == NULL) {
			continue;
		    }
		    err = md_lib_get_phy_intf_speed(commit, db,ts_str(child_val),
			    &interface_bw, count_down_link);
		    bail_error(err);
		    lc_log_basic(jnpr_log_level, "%s: checking- %s, speed-> %lu", __FUNCTION__, ts_str(child_val), interface_bw);
		    *intf_conf_bw += interface_bw;
		}
		/* we have found our interface, so leave now */
		goto bail;
	    }
	}
    } else {
	/* this is a physical interface, get the interface from node */
	err = md_lib_get_phy_intf_speed(commit, db, intf_name,
		&interface_bw, count_down_link);
	bail_error(err);
	*intf_conf_bw = interface_bw;
    }

bail:
    return err;
}


int
md_lib_cal_tot_avail_bw(md_commit *commit, const mdb_db *db, uint64_t *tot_bw, tbool count_down_link)
{
    int err = 0, i = 0, j = 0, child_num = 0;
    tbool found = false, node_found = false, initial = false;
    bn_binding_array *bindings= NULL, *child_interface = NULL;
    bn_binding *binding = NULL, *child_binding  =NULL;
    char *intf_name = NULL;
    uint64_t total_conf_bw = 0, bonds_intf_bw = 0, interface_bw = 0;
    int num_bindings = 0;
    const tstring *bn_string = NULL, *child_string = NULL;
    tstring *node_val = NULL, *child_val = NULL;
    char tmp_buf[256] = {0};

    bail_null(db);

    lc_log_basic(jnpr_log_level, "%s: FXN_CALL ", __FUNCTION__);
    /* get "/net/interface/state/xxx" , no children */
    err = mdb_iterate_binding(commit, db, "/net/interface/state", 0, &bindings);
    bail_error(err);

    bn_binding_array_dump(__FUNCTION__, bindings, jnpr_log_level);
    num_bindings = bn_binding_array_length_quick(bindings);
    for (i = 0; i < num_bindings; i++) {
	binding = bn_binding_array_get_quick(bindings, i);
	bail_null(binding);

	/* get the name of interface /net/interface/state/eth0 = eth0 */
	safe_free(intf_name);
	err = bn_binding_get_str(binding, ba_value, 0, NULL, &intf_name);
	bail_error(err);

	lc_log_basic(jnpr_log_level, "%s: checking %s", __FUNCTION__, intf_name);

	/* Dont consider loopback and sit0 interfaces*/
	err = is_internal_intf(intf_name, &found);
	bail_error(err);
	if (found == true) {
	    continue;
	}
	/* check if interface is a delivery interface */
	err = md_lib_is_delivery_intf(commit, db, intf_name, &found);
	bail_error(err);
	lc_log_basic(jnpr_log_level, "%s: delivery intf %d", __FUNCTION__, found);
	if (found == false) {
	    /* no delivery on this interface */
	    continue;
	}
	/* check if this interface is bound to a bond */
	err = md_lib_intf_is_bonded(commit, db, intf_name, &found);
	bail_error(err);
	lc_log_basic(jnpr_log_level, "%s: bonded - %d", __FUNCTION__, found);
	if (found == true) {
	    /*
	     * this interface is bound to a bond, don't count it,
	     * this will counted with parent bond interface
	     */
	    continue;
	}
	/*
	 * all rejection has been done, now get the bandwidth(bond or physical),
	 * even if it is down, sending true to count it
	 */
	err = md_lib_get_intf_speed(commit, db, intf_name, &total_conf_bw, count_down_link);
	bail_error(err);
	*tot_bw +=total_conf_bw;
    }
    err = md_commit_get_commit_initial(commit, &initial);
    bail_error(err);
    if (initial == false) {
	/* calculate the bandwidth. need to calculate in case of initial only*/
	goto bail;
    }
    bn_binding_array_free(&bindings);
    lc_log_basic(jnpr_log_level, "%s: FXN_CALL bonding", __FUNCTION__);
    /* get "/net/interface/state/xxx" , no children */
    err = mdb_iterate_binding(commit, db, "/net/bonding/config", 0, &bindings);
    bail_error(err);

    bn_binding_array_dump(__FUNCTION__, bindings, jnpr_log_level);
    num_bindings = bn_binding_array_length_quick(bindings);
    for (i = 0; i < num_bindings; i++) {
	binding = bn_binding_array_get_quick(bindings, i);
	if (binding == NULL) {
	    continue;
	}

	bn_string = bn_binding_get_name_quick(binding);
	bail_null(bn_string);
	snprintf(tmp_buf, sizeof(tmp_buf), "%s/name", ts_str(bn_string));
	ts_free(&node_val);
	err = mdb_get_node_value_tstr(commit, db, tmp_buf, 0, &node_found, &node_val);
	bail_error(err);
	if (node_found == false) {
	    /* though this must not happen, still we must be handling it */
	    ts_free(&node_val);
	    continue;
	}
	/* Dont consider loopback and sit0 interfaces*/
	err = is_internal_intf(ts_str(node_val), &found);
	bail_error(err);
	if (found == true) {
	    continue;
	}
	err = md_lib_is_delivery_intf(commit, db, ts_str(node_val), &found);
	bail_error(err);
	lc_log_basic(jnpr_log_level, "%s: delivery intf %d", __FUNCTION__, found);
	if (found == false) {
	    /* no delivery on this interface */
	    continue;
	}
	/* we got the node, find child interfaces */
	tmp_buf[0] = '\0';
	snprintf(tmp_buf, sizeof(tmp_buf), "%s/interface",ts_str(bn_string));
	err = mdb_iterate_binding(commit, db, tmp_buf, 0, &child_interface);
	bail_error(err);
	child_num = bn_binding_array_length_quick(child_interface);
	lc_log_basic(jnpr_log_level, "%s: bond child- %d", __FUNCTION__, child_num);
	if (child_num == 0) {
	    /* no interfaces under this bond */
	    continue;
	}
	for (j = 0; j < child_num; j++) {
	    child_binding = bn_binding_array_get_quick(child_interface, j);
	    if (child_binding == NULL) {
		continue;
	    }
	    child_string = bn_binding_get_name_quick(child_binding);
	    bail_null(child_string);
	    ts_free(&child_val);
	    err = mdb_get_node_value_tstr(commit, db, ts_str(child_string),
		    0, &node_found, &child_val);
	    bail_error(err);
	    if (node_found == false || child_val == NULL) {
		continue;
	    }
	    err = md_lib_get_phy_intf_speed(commit, db,ts_str(child_val),
		    &interface_bw, count_down_link);
	    bail_error(err);
	    lc_log_basic(jnpr_log_level, "%s: checking- %s, speed-> %lu", __FUNCTION__, ts_str(child_val), interface_bw);
	    bonds_intf_bw += interface_bw;
	}
	*tot_bw +=bonds_intf_bw;
    }
    lc_log_basic(jnpr_log_level, "%s: got bw -%lu", __FUNCTION__, *tot_bw);
bail:
    safe_free(intf_name);
    bn_binding_array_free(&bindings);
    return err;
}
