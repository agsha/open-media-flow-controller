/*
 *
 * Filename:  md_nvsd_server_map.c
 * Date:      2009/03/09
 * Author:    Dhruva Narasimhan
 *
 * (C) Copyright 2008,2009 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

#include <strings.h>
#include <limits.h>
#include "common.h"
#include "dso.h"
#include "mdb_db.h"
#include "md_utils.h"
#include "md_mod_commit.h"
#include "md_mod_reg.h"
#include "proc_utils.h"
#include "file_utils.h"
#include "md_main.h"
#include "nkn_defs.h"
#include "nkn_mgmt_defs.h"

md_commit_forward_action_args md_server_map_fwd_args =
{
    GCL_CLIENT_NVSD, true, N_("Request failed; MFD subsystem did not respond"),
    mfat_blocking
#ifdef PROD_FEATURE_I18N_SUPPORT
    , GETTEXT_DOMAIN
#endif
};

/* ------------------------------------------------------------------------- */

enum {
        STATE_UNKNOWN = 0,
        STATE_VALID = 1,
        STATE_INVALID   = 2
};


int md_nvsd_server_map_init(const lc_dso_info *info, void *data);

static int
md_nvsd_server_map_get_mon_nodes(
                md_commit *commit,
                const mdb_db *db,
                const char *node_name,
                const bn_attrib_array *node_attribs,
                bn_binding **ret_binding,
                uint32 *ret_node_flags,
                void *arg);

static int
md_nvsd_server_map_async_action(const char *name,
                         const char *config);

static int
md_nvsd_server_map_refresh_async_comp(lc_child_completion_handle *cmpl_hand,
        pid_t pid, int wait_status, tbool completed,
        void *arg);

static int md_nvsd_server_map_add_initial(md_commit *commit, mdb_db *db, void *arg);

static int md_nvsd_server_map_commit_check(md_commit *commit,
                                  const mdb_db *old_db,
                                  const mdb_db *new_db,
                                  mdb_db_change_array *change_list, void *arg);

static int md_nvsd_server_map_commit_apply(md_commit *commit,
                                  const mdb_db *old_db,
                                  const mdb_db *new_db,
                                  mdb_db_change_array *change_list, void *arg);

static int md_nvsd_server_map_commit_side_effects( md_commit *commit,
				const mdb_db *old_db, mdb_db *inout_new_db,
				mdb_db_change_array *change_list, void *arg);

static int
md_nvsd_server_map_handle_action(md_commit *commit,
                         const mdb_db *db,
                         const char *action_name,
                         bn_binding_array *params,
                         void *cb_arg);

static int
md_nvsd_server_map_create_timer(md_commit *commit,
                         const mdb_db *db,
                         const char *action_name,
                         bn_binding_array *params,
                         void *cb_arg);

static int
md_nvsd_server_map_cleanup_temp_files(const char *smap,
		const char *file_name,
		tbool delete_map);

static md_upgrade_rule_array *md_nvsd_server_map_ug_rules = NULL;

/*
 * This module registers a timer event for every server-map that is
 * created and has a valid URL & refresh interval.
 */
typedef struct server_map_timer_data_st {
	int			index; 		/* Index holder : Never reset or modify */
	char 			*name;
	char 			*url;
	lew_event		*poll_event;
	lew_event_handler 	callback;
	void 			*context_data; /* Points to iteself */
	lt_dur_ms		poll_frequency;
	uint32			type;
        uint32                  version;
        time_t                  last_action_time;
        char                    *file_path;
        uint32                  state;
} server_map_timer_data_t;

typedef struct servermap_refresh_event_data_st {
        time_t             action_trig_time;
        char               *smap_name;
        lc_launch_result   *lr;
        uint32             version;
        char               *uri;
        char               *file;
} servermap_refresh_event_data_t;


static server_map_timer_data_t	server_map_timers[NKN_MAX_SERVER_MAPS];

int md_nvsd_server_map_poll_timeout_handler(int fd,
					short event_type,
					void *data,
					lew_context *ctxt);

static int md_nvsd_server_map_fetch_file(server_map_timer_data_t *context, int32 *ret_code);

/* Called when server-map is set and ready to be polled. And also called
 * from the timer expiry handler
 */
int md_nvsd_server_map_set_poll_timer(server_map_timer_data_t *context);

/* Called when a server-map is deleted from the config */
int md_nvsd_server_map_delete_poll_timer(server_map_timer_data_t *context);

static int md_nvsd_server_map_context_allocate(const char *name, lt_dur_ms poll_frequency, const char *url, uint32 type, server_map_timer_data_t **ret_context);
static int md_nvsd_server_map_context_free(server_map_timer_data_t *context);
static int md_nvsd_server_map_context_find(const char *name, server_map_timer_data_t **ret_context);
static int md_nvsd_server_map_context_init(void);
static int md_nvsd_server_map_process_file(server_map_timer_data_t *context, int32 *ret_code, tstring **msg, md_commit *commit);
int parse_url(const char *url, char **ret_hostname, char **ret_file);

static const char prog_fetch_smap_path[] = "/opt/nkn/bin/fetch_smap";
static const char prog_mapxml_path[] = "/opt/nkn/bin/MapXML";

static const char path_smap_files[] = "/nkn/tmp/smap";

/* BZ 4836 : Add format type node strings */
static const char *format_type_map[] = {
	"UNDEFINED",
	"HostOriginMap",
	"ClusterMap",
	"OriginEscalationMap",
	"nfsMap",
	"OriginRoundRobinMap"
};
static int
md_nvsd_server_map_validate_name(const char *name,
			tstring **ret_msg);
/* ------------------------------------------------------------------------- */
int
md_nvsd_server_map_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    /*
     * XXX: change this to reflect the name of your module, and the
     * root of the subtree the module owns.
     */
    err = mdm_add_module("nvsd-server-map", 8,
	    "/nkn/nvsd/server-map", NULL,
	    modrf_all_changes,
	    md_nvsd_server_map_commit_side_effects, NULL,
	    md_nvsd_server_map_commit_check, NULL,
	    md_nvsd_server_map_commit_apply, NULL,
	    0, 0,
	    md_generic_upgrade_downgrade,
	    &md_nvsd_server_map_ug_rules,
	    md_nvsd_server_map_add_initial, NULL,
	    NULL, NULL, NULL, NULL,
	    &module);
    bail_error(err);

#ifdef PROD_FEATURE_I18N_SUPPORT
    err = mdm_module_set_gettext_domain(module, GETTEXT_DOMAIN);
    bail_error(err);
#endif /* PROD_FEATURE_I18N_SUPPORT */

    /*
     * XXX: replicate this as many times as you have nodes to register.
     * This is for static data nodes; see below for other node types.
     *
     * Make sure to only set the registration flags to either
     * mrf_flags_reg_config_literal or mrf_flags_reg_config_wildcard
     * depending on what kind of node you have.
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/server-map/config/*";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_limit_wc_count_max = NKN_MAX_SERVER_MAPS;
    node->mrn_limit_str_max_chars = MAX_SERVER_MAP_NAMELEN;
    node->mrn_limit_str_charlist =  CLIST_ALPHANUM "_";
    node->mrn_bad_value_msg =	   "Invalid Name/Maximum server-maps configured";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Server-Map";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/server-map/config/*/protocol";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Server-Map Protocol";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/server-map/config/*/file-url";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Server-Map file URL";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/server-map/config/*/map-status";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "false";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Server-Map refresh status";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/server-map/config/*/refresh";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value =      "0";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Server-Map Refresh interval, seconds";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/server-map/config/*/file-binary";
    node->mrn_value_type =         bt_string;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Server-Map binary, compiled, file";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/server-map/config/*/format-type";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value =      "0";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Server-Map binary, compiled, file";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/server-map/config/*/node-monitoring/heartbeat/interval";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value =      "100";
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  3600000;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Node Monitoring heartbeat interval in milliseconds";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/server-map/config/*/node-monitoring/heartbeat/connect-timeout";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value =      "100";
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  3600000;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Node Monitoring heartbeat connect timeout in milliseconds";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/server-map/config/*/node-monitoring/heartbeat/read-timeout";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value =      "100";
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  3600000;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Node Monitoring heartbeat read timeout in milliseconds";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/server-map/config/*/node-monitoring/heartbeat/allowed-fails";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value =      "3";
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  UINT_MAX;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Node Monitoring heartbeat allowed failure count";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/server-map/monitor/*/state";
    node->mrn_value_type =         bt_uint32;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Monitoring node which will identify the state of server-map";
    node->mrn_mon_get_func =       md_nvsd_server_map_get_mon_nodes;
    node->mrn_mon_get_arg =        (void*)("state");
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/server-map/monitor/*/map-bin-file";
    node->mrn_value_type =         bt_string;
    node->mrn_node_reg_flags =     mrf_flags_reg_monitor_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Monitoring node which will identify the map binary file";
    node->mrn_mon_get_func =       md_nvsd_server_map_get_mon_nodes;
    node->mrn_mon_get_arg =        (void*)("map-bin-file");
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_action(module, &node, 2);
    bail_error(err);
    node->mrn_name = 		"/nkn/nvsd/server-map/actions/refresh-force";
    node->mrn_node_reg_flags =  mrf_flags_reg_action;
    node->mrn_cap_mask = 	mcf_cap_action_basic;
    node->mrn_action_func = 	md_nvsd_server_map_handle_action;
    node->mrn_action_arg = 	NULL;
    node->mrn_actions[0]->mra_param_name = "name";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_actions[1]->mra_param_name = "config";
    node->mrn_actions[1]->mra_param_type = bt_string;
    node->mrn_description = 	"";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err  = mdm_new_action(module, &node, 2);
    bail_error(err);
    node->mrn_name = 		"/nkn/nvsd/server-map/actions/updated-map";
    node->mrn_node_reg_flags =  mrf_flags_reg_action;
    node->mrn_cap_mask = 	mcf_cap_node_basic;
    node->mrn_action_func = 	md_commit_forward_action;
    node->mrn_action_arg = 	&md_server_map_fwd_args;
    node->mrn_actions[0]->mra_param_name = "name";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_actions[1]->mra_param_name = "file-name";
    node->mrn_actions[1]->mra_param_type = bt_string;
    node->mrn_description = 	"";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err  = mdm_new_action(module, &node, 1);
    bail_error(err);
    node->mrn_name = 		"/nkn/nvsd/server-map/actions/create-timer";
    node->mrn_node_reg_flags =  mrf_flags_reg_action;
    node->mrn_cap_mask = 	mcf_cap_node_basic;
    node->mrn_action_async_nonbarrier_start_func = 	md_nvsd_server_map_create_timer;
    node->mrn_action_arg = 	&md_server_map_fwd_args;
    node->mrn_actions[0]->mra_param_name = "name";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_description = 	"Create a timer for the server-map";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_event(module, &node, 2);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/server-map/notify/map-status";
    node->mrn_node_reg_flags = mrf_flags_reg_event;
    node->mrn_cap_mask = mcf_cap_event_privileged;
    node->mrn_description = "";
    node->mrn_events[0]->mre_binding_name = "sever_map_name";
    node->mrn_events[0]->mre_binding_type = bt_string;
    node->mrn_events[1]->mre_binding_name = "status";
    node->mrn_events[1]->mre_binding_type = bt_bool;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_event(module, &node, 3);
    bail_error(err);
    node->mrn_name = "/nkn/nvsd/server-map/notify/map-status-change";
    node->mrn_node_reg_flags = mrf_flags_reg_event;
    node->mrn_cap_mask = mcf_cap_event_privileged;
    node->mrn_description = "";
    node->mrn_events[0]->mre_binding_name = "sever_map_name";
    node->mrn_events[0]->mre_binding_type = bt_string;
    node->mrn_events[1]->mre_binding_name = "status";
    node->mrn_events[1]->mre_binding_type = bt_uint32;
    node->mrn_events[2]->mre_binding_name = "file_name";
    node->mrn_events[2]->mre_binding_type = bt_string;
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = md_upgrade_rule_array_new(&md_nvsd_server_map_ug_rules);
    bail_error(err);
    ra = md_nvsd_server_map_ug_rules;

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type = 	MUTT_REPARENT;
    rule->mur_name = 		"/nkn/nvsd/server_map/*";
    rule->mur_reparent_self_value_follows_name = true;
    rule->mur_reparent_graft_under_name = "/nkn/nvsd/server_map/config/*";
    rule->mur_reparent_new_self_name = NULL;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/server_map/config/*/file-binary";
    rule->mur_new_value_type = 	bt_string;
    rule->mur_new_value_str = 	"";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/server_map/config/*/format-type";
    rule->mur_new_value_type = 	bt_uint32;
    rule->mur_new_value_str = 	"0";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);


    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type = 	MUTT_CLAMP;
    rule->mur_name = 		"/nkn/nvsd/server_map/config/*/refresh";
    rule->mur_lowerbound = 		0;
    rule->mur_upperbound = 		86400; // 1 day = 24 hours
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 5, 6);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/server-map/config/*/node-monitoring/heartbeat/interval";
    rule->mur_new_value_type = 	bt_uint32;
    rule->mur_new_value_str = 	"100";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 5, 6);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/server-map/config/*/node-monitoring/heartbeat/connect-timeout";
    rule->mur_new_value_type = 	bt_uint32;
    rule->mur_new_value_str = 	"100";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 5, 6);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/server-map/config/*/node-monitoring/heartbeat/read-timeout";
    rule->mur_new_value_type = 	bt_uint32;
    rule->mur_new_value_str = 	"100";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 5, 6);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/server-map/config/*/node-monitoring/heartbeat/allowed-fails";
    rule->mur_new_value_type = 	bt_uint32;
    rule->mur_new_value_str = 	"3";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type = 	MUTT_ADD;
    rule->mur_name = 		"/nkn/nvsd/server-map/config/*/map-status";
    rule->mur_new_value_type = 	bt_bool;
    rule->mur_new_value_str = 	"false";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 7, 8);
    rule->mur_upgrade_type = 	MUTT_RESTRICT_CHARS;
    rule->mur_name = 		"/nkn/nvsd/server-map/config/*";
    rule->mur_replace_char =	'_';
    rule->mur_allowed_chars =   CLIST_ALPHANUM "_";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 7, 8);
    rule->mur_upgrade_type = 	MUTT_RESTRICT_LENGTH;
    rule->mur_name = 		"/nkn/nvsd/server-map/config/*";
    rule->mur_len_max =		MAX_SERVER_MAP_NAMELEN;
    rule->mur_allowed_chars	= CLIST_ALPHANUM "_";
    MD_ADD_RULE(ra);

bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_nvsd_server_map_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
    int err = 0;

    /*
     * XXX: create initial wildcard instances, or compute initial
     * values of any nodes, here.  If you have a lot of initial nodes
     * with static values, consider using mdb_create_node_bindings()
     * with an array of bn_str_value structs, rather than writing a
     * lot of code.
     */

bail:
    return(err);
}

/* ------------------------------------------------------------------------- */
static int md_nvsd_server_map_commit_side_effects( md_commit *commit,
				const mdb_db *old_db, mdb_db *inout_new_db,
				mdb_db_change_array *change_list, void *arg)
{
    int err = 0;
    int num_changes = 0;
    int i = 0;
    bn_binding *binding = NULL;
    tstr_array *name_parts = NULL;
    const mdb_db_change *change = NULL;
    char *abs_file_name = NULL;
    char *filename = NULL;
    char *url = NULL;
    const char *t_smap = NULL;
    tstring *t_file_url = NULL;
    const bn_attrib *new_value = NULL;
    server_map_timer_data_t *context = NULL;

    num_changes = mdb_db_change_array_length_quick(change_list);
    for(i = 0; i < num_changes; i++)
    {
	change = mdb_db_change_array_get_quick(change_list, i);
	bail_null(change);

	if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6, "nkn", "nvsd", "server-map", "config", "*", "file-url")
		&& ((mdct_add == change->mdc_change_type)
		    || (mdct_modify == change->mdc_change_type)))
	{
	    err = mdb_get_node_binding(commit, inout_new_db, ts_str(change->mdc_name), 0, &binding);
	    bail_error_null(err, binding);

	    err = bn_binding_get_name_parts(binding, &name_parts);
	    bail_error_null(err, name_parts);

	    t_smap = tstr_array_get_str_quick(name_parts, 4);
	    bail_null(t_smap);

	    new_value = bn_attrib_array_get_quick(change->mdc_new_attribs, ba_value);

	    /* If no value then continue */
	    if (!new_value) continue;

	    /* Get the URL */
	    err = bn_attrib_get_tstr(new_value, NULL, bt_string, NULL, &t_file_url);
	    bail_error_null(err, t_file_url);

	    err = parse_url(ts_str(t_file_url), &url, &filename);
	    if((err == lc_err_bad_path) || (err != 0))
	    {
		err = md_commit_set_return_status_msg_fmt(commit, 1,
			_("Invalid Path : '%s'\n"), ts_str(t_file_url));
		bail_error(err);
		goto bail;
	    }
	}
    }

bail:
    safe_free(abs_file_name);
    safe_free(url);
    safe_free(filename);
    ts_free(&t_file_url);
    tstr_array_free(&name_parts);
    return err;
}


/* ------------------------------------------------------------------------- */
static int
md_nvsd_server_map_commit_check(md_commit *commit,
                         const mdb_db *old_db,
                         const mdb_db *new_db,
                         mdb_db_change_array *change_list, void *arg)
{
    int err = 0;
    uint32 i = 0;
    uint32 idx = 0;
    uint32 num_ns = 0;
    uint32 ret_idx = 0;
    tbool found = false;
    uint32 num_changes = 0;
    char *node_name = NULL;
    const char *server_map_name = NULL;
    tstring *ns_server_map_name = NULL;
    tstr_array *t_namespace = NULL;
    bn_binding_array *binding_list = NULL;
    tstr_array *t_name_parts= NULL;
    const mdb_db_change *change = NULL;
    char *t_name = NULL;
    const bn_attrib *new_value = NULL;
    char *filename = NULL;
    char *url = NULL;
    char *abs_file = NULL;
    bn_binding *binding = NULL;
    bn_request *req = NULL;
    bn_binding_array *barr = NULL;
    bn_binding *bn_smap_name_binding = NULL;
    tstr_array *t_server_maps = NULL;
    uint32 j = 0;
    tstring *t_smap_name = NULL;
    tstring *t_old_url = NULL;
    tstr_array *t_server_maps_http = NULL;
    tstring *t_smap_name_http = NULL;
    tstring *t_server_map_type = NULL;
    tstring *t_server_format_type = NULL;
    char *format_node_name = NULL;
    uint32 format_type = 0;
    tstring *ret_msg =NULL;
    tstr_array *t_server_maps_cluster = NULL;
    tstring *t_smap_name_cluster = NULL;
    num_changes = mdb_db_change_array_length_quick(change_list);
    for(i = 0; i < num_changes; i++) {
	change = mdb_db_change_array_get_quick(change_list, i);
	bail_null(change);


	if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 5, "nkn", "nvsd", "server-map", "config", "*"))) {

	    const char *t_current_server_map = NULL;
	    /* Validate the server-map name here */
	    t_current_server_map = tstr_array_get_str_quick(change->mdc_name_parts, 4);

	    err = md_nvsd_server_map_validate_name (t_current_server_map, &ret_msg);
	    if (err) {
		err = 0;
		err = md_commit_set_return_status_msg_fmt(
			commit, 1,
			_("error: %s\n"), ts_str(ret_msg));
		ts_free(&ret_msg);

		goto bail;
	    }

	    if (change->mdc_change_type == mdct_delete) {
		server_map_name = tstr_array_get_str_quick(change->mdc_name_parts, 4);
		if (!server_map_name) goto bail;

		/* Get the namespace names from the uri nodes */
		err = mdb_get_matching_tstr_array(commit,
			new_db,
			"/nkn/nvsd/namespace/*/origin-server/nfs/server-map",
			0,
			&t_server_maps);
		bail_error(err);
		if (t_server_maps != NULL) {
		    /* At least 1 or more namespaces found.
		     * Check if this namespace is associated with at least
		     * one namespace. If so, reject the commit
		     */
		    for(j = 0; j < tstr_array_length_quick(t_server_maps); j++) {
			lc_log_basic(LOG_DEBUG, "smap_delete: binding: '%s'", tstr_array_get_str_quick(t_server_maps, j));

			/* Get the value of the node and compare the
			 * value with the server-map name.
			 */
			err = mdb_get_node_value_tstr(NULL,
				new_db,
				tstr_array_get_str_quick(t_server_maps, j),
				0, NULL,
				&t_smap_name);
			bail_error(err);

			if (t_smap_name && ts_equal_str(t_smap_name, server_map_name, false)) {
			    /* Name matched... REJECT the commit */

			    /* Get the namespace name */
			    err = ts_tokenize_str(
				    tstr_array_get_str_quick(t_server_maps, j),
				    '/', '\0', '\0', 0, &t_name_parts);
			    bail_error(err);

			    err = md_commit_set_return_status_msg_fmt(
				    commit, 1,
				    _("Server-map \"%s\" is associated with namespace "
					"\"%s\".\nSorry cannot delete this entry"),
				    server_map_name,
				    tstr_array_get_str_quick(t_name_parts, 4));
			    bail_error(err);

			    tstr_array_free(&t_name_parts);
			    goto bail;
			}
			ts_free(&t_smap_name);
			t_smap_name = NULL;
		    }
		    tstr_array_free(&t_server_maps);
		}
		err = mdb_get_matching_tstr_array(commit,
			new_db,
			"/nkn/nvsd/namespace/*/origin-server/http/server-map/*/map-name",
			0,
			&t_server_maps_http);
		bail_error(err);
		if (t_server_maps_http != NULL) {
		    /* At least 1 or more namespaces found.
		     * Check if this namespace is associated with at least
		     * one namespace. If so, reject the commit
		     */
		    for(j = 0; j < tstr_array_length_quick(t_server_maps_http); j++) {
			lc_log_basic(LOG_DEBUG, "smap_delete: binding: '%s'", tstr_array_get_str_quick(t_server_maps_http, j));

			/* Get the value of the node and compare the
			 * value with the server-map name.
			 */
			err = mdb_get_node_value_tstr(NULL,
				new_db,
				tstr_array_get_str_quick(t_server_maps_http, j),
				0, NULL,
				&t_smap_name_http);
			bail_error(err);

			if (t_smap_name_http && ts_equal_str(t_smap_name_http, server_map_name, false)) {
			    /* Name matched... REJECT the commit */

			    /* Get the namespace name */
			    err = ts_tokenize_str(
				    tstr_array_get_str_quick(t_server_maps_http, j),
				    '/', '\0', '\0', 0, &t_name_parts);
			    bail_error(err);

			    err = md_commit_set_return_status_msg_fmt(
				    commit, 1,
				    _("Server-map \"%s\" is associated with namespace "
					"\"%s\".\nSorry cannot delete this entry"),
				    server_map_name,
				    tstr_array_get_str_quick(t_name_parts, 4));
			    bail_error(err);

			    tstr_array_free(&t_name_parts);
			    goto bail;
			}
			ts_free(&t_smap_name_http);
			t_smap_name_http = NULL;
		    }
		    tstr_array_free(&t_server_maps_http);
		}

		err = mdb_get_matching_tstr_array(commit,
			new_db,
			"/nkn/nvsd/cluster/config/*/server-map",
			0,
			&t_server_maps_cluster);
		bail_error(err);
		if (t_server_maps_cluster != NULL) {
		    /* At least 1 or more namespaces found.
		     * Check if this namespace is associated with at least
		     * one namespace. If so, reject the commit
		     */
		    for(j = 0; j < tstr_array_length_quick(t_server_maps_cluster); j++) {
			lc_log_basic(LOG_DEBUG, "smap_delete: binding: '%s'", tstr_array_get_str_quick(t_server_maps_cluster, j));

			/* Get the value of the node and compare the
			 * value with the server-map name.
			 */
			err = mdb_get_node_value_tstr(NULL,
				new_db,
				tstr_array_get_str_quick(t_server_maps_cluster, j),
				0, NULL,
				&t_smap_name_cluster);
			bail_error(err);

			if (t_smap_name_cluster && ts_equal_str(t_smap_name_cluster, server_map_name, false)) {
			    /* Name matched... REJECT the commit */

			    /* Get the cluster name */
			    err = ts_tokenize_str(
				    tstr_array_get_str_quick(t_server_maps_cluster, j),
				    '/', '\0', '\0', 0, &t_name_parts);
			    bail_error(err);

			    err = md_commit_set_return_status_msg_fmt(
				    commit, 1,
				    _("Server-map \"%s\" is associated with Cluster "
					"\"%s\".\nSorry cannot delete this entry"),
				    server_map_name,
				    tstr_array_get_str_quick(t_name_parts, 5));
			    bail_error(err);

			    tstr_array_free(&t_name_parts);
			    goto bail;
			}
			ts_free(&t_smap_name_cluster);
			t_smap_name_cluster = NULL;
		    }
		    tstr_array_free(&t_server_maps_cluster);
		}
	    }
	}
	else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6, "nkn", "nvsd", "server-map", "config", "*", "file-url")
		&& ((mdct_add == change->mdc_change_type) || (mdct_modify == change->mdc_change_type)))
	{
	    err = mdb_get_node_binding (commit, new_db, ts_str (change->mdc_name), 0, &binding);
	    bn_binding_get_name_parts (binding, &t_name_parts);
	    bail_error(err);

	    server_map_name = tstr_array_get_str_quick (t_name_parts, 4);

	    format_node_name = smprintf("/nkn/nvsd/server-map/config/%s/format-type", server_map_name);
	    bail_null(format_node_name);

	    err = mdb_get_node_value_uint32(commit, new_db,
		    format_node_name, 0,
		    NULL, &format_type);
	    bail_error(err);


	    new_value = bn_attrib_array_get_quick(change->mdc_new_attribs, ba_value);
	    if (new_value) {
		err = bn_attrib_get_str(new_value, NULL, bt_string, NULL, &t_name);
		bail_error_null(err, t_name);

		/* Check if we have a file-url value */
		if ((!strcmp(t_name, ""))){
		    if(mdct_modify == change->mdc_change_type){
			err = md_commit_set_return_status_msg_fmt(
				commit, 1,
				_("Server-map \"%s\" file-url cannot be empty\n "),
				server_map_name);
			bail_error(err);
		    }


		}else{

		    /* Now that a file-uril is given, need to make sure format-type is set */
		    if (format_type == 0) {
			err = md_commit_set_return_status_msg_fmt(commit, 1,
				_("Configure the format-type, before configuring file-url"));
			bail_error(err);
			goto bail;
		    }

		    if (!lc_is_prefix("http://", t_name, true)) {
			err = md_commit_set_return_status_msg_fmt(commit, 1,
				_("bad file-url value - '%s'. url must start with 'http://'."),
				t_name);
			bail_error(err);
			goto bail;
		    }

		    err = parse_url(t_name, &url, &filename);
		    if((err == lc_err_bad_path) || (err != 0))
		    {
			err = md_commit_set_return_status_msg_fmt(commit, 1,
				_("Invalid Path : '%s'\n"),url);
			bail_error(err);
			goto bail;
		    }

		    if (filename == NULL) {
			err = md_commit_set_return_status_msg_fmt(commit, 1,
				_("bad file name."));
			bail_error(err);
			goto bail;
		    }

		    /* BZ 3003
		     * To change the file-url after it has been already
		     * configured, need to fire the create_timer action
		     * again. Since we create the timer only when the
		     * refresh-interval node is changed, a change in file-url
		     * is never consumed!
		     *
		     * Get the old binding value & make sure there is a
		     * change. BTW, if we came upto this point then the
		     * new file-url passed basic sanity checks
		     */
		    node_name = smprintf(
			    "/nkn/nvsd/server-map/config/%s/file-url",
			    server_map_name);
		    bail_null(node_name);

		    err = mdb_get_node_value_tstr(NULL,
			    old_db,
			    node_name,
			    0, NULL,
			    &t_old_url);
		    bail_error(err);

		    if ((NULL == t_old_url) || (ts_length(t_old_url) == 0)) {
			/* Old DB didnt have any config set. So this is
			 * a valid change
			 */
		    }

		}
	    }
	}
	else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6, "nkn", "nvsd", "server-map", "config", "*", "refresh")
		&& ((mdct_add == change->mdc_change_type) || (mdct_modify == change->mdc_change_type)))
	{
	    /* This is called when a new server map is added.
	     * When the user creates a server-map and doesnt set the file-url, the default
	     * value of "" is seen in the file-url.
	     * But, if the map exists, is configured correctly and mgmtd restarted, then
	     * file-url is valid!
	     *
	     * When the server-map is first created, the file-url is set to
	     * an empty string. Only when the user sets the file-url and the
	     * timeout (refresh interval), the timer should be created. However,
	     * we can allocate the context and also create the timer with just
	     * the file-url and a refresh interval of 0 seconds - which
	     * happens as soon as the file-url node is set.
	     */
	    uint32 t_time = 0;
	    new_value = bn_attrib_array_get_quick(change->mdc_new_attribs, ba_value);
	    if (new_value) {
		tstring *t_url = NULL;

		err = mdb_get_node_binding (commit, new_db, ts_str (change->mdc_name), 0, &binding);
		bn_binding_get_name_parts (binding, &t_name_parts);
		bail_error(err);
		server_map_name = tstr_array_get_str_quick (t_name_parts, 4);

		node_name = smprintf("/nkn/nvsd/server-map/config/%s/file-url", server_map_name);
		bail_null(node_name);

		err = bn_attrib_get_uint32(new_value, NULL, NULL, &t_time);
		bail_error(err);

		if (((t_time < 300) || (t_time > 86400)) && (t_time != 0)) {
		    err = md_commit_set_return_status_msg_fmt(commit, 1,
			    _("bad refresh time. value should be 0 or 300 (5 minutes) to 86400 (1 day)."));
		    bail_error(err);

		    goto bail;

		}
		bn_binding_array_free(&barr);
		ts_free(&t_url);
		safe_free(node_name);
	    }
	}
	else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6, "nkn", "nvsd", "server-map", "config", "*", "format-type")
		&& (mdct_modify == change->mdc_change_type))
	{
	    tstr_array *t_smaps_http = NULL;
	    tstring *t_map_name = NULL;
	    const char *smap_name = tstr_array_get_str_quick(change->mdc_name_parts, 4);

	    err = mdb_get_matching_tstr_array(commit,
		    old_db,
		    "/nkn/nvsd/namespace/*/origin-server/http/server-map/*/map-name",
		    0,
		    &t_smaps_http);
	    bail_error(err);
	    if (t_smaps_http != NULL) {
		/* At least 1 or more namespaces found.
		 * Check if this namespace is associated with at least
		 * one namespace. If so, reject the commit
		 */

		for(j = 0; j < tstr_array_length_quick(t_smaps_http); j++) {
		    lc_log_basic(LOG_DEBUG, "smap_format_type_modify: binding: '%s'",
			    tstr_array_get_str_quick(t_smaps_http, j));

		    /* Get the value of the node and compare the
		     * value with the server-map name.
		     */
		    err = mdb_get_node_value_tstr(NULL,
			    new_db,
			    tstr_array_get_str_quick(t_smaps_http, j),
			    0, NULL,
			    &t_map_name);
		    bail_error(err);

		    if (t_map_name && ts_equal_str(t_map_name, smap_name, false)) {
			/* Name matched... REJECT the commit */

			err = md_commit_set_return_status_msg_fmt(
				commit, 1,
				_("Server-map \"%s\" is associated with a namespace. Sorry cannot change the format type"),
				smap_name);

			bail_error(err);

			tstr_array_free(&t_name_parts);
			goto bail;
		    }
		    ts_free(&t_map_name);
		    t_map_name = NULL;
		}

		tstr_array_free(&t_smaps_http);
	    }
	}
    }
    err = md_commit_set_return_status(commit, 0);
    bail_error(err);

bail:
    safe_free(node_name);
    bn_request_msg_free(&req);
    bn_binding_array_free(&binding_list);
    tstr_array_free(&t_name_parts);
    tstr_array_free(&t_namespace);
    ts_free(&ns_server_map_name);
    tstr_array_free(&t_server_maps);
    safe_free(t_name);
    bn_binding_free(&binding);
    ts_free(&t_smap_name);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_nvsd_server_map_commit_apply(md_commit *commit,
                         const mdb_db *old_db, const mdb_db *new_db,
                         mdb_db_change_array *change_list, void *arg)
{
    int err = 0;
    int32 num_changes = 0;
    mdb_db_change *change = NULL;
    int i = 0;
    const char *t_name = NULL;
    uint32 poll_frequency = 0;
    char *node_name = NULL;
    server_map_timer_data_t *context = NULL;
    tstring *t_url = NULL;
    uint32 type = 0;
    char *url_node_name = NULL;
    const bn_attrib *new_value = NULL;
    bn_request *req = NULL;
    tstring *map_status = NULL;
    tstring *ret_msg = NULL;


    num_changes = mdb_db_change_array_length_quick(change_list);
    for(i = 0; i < num_changes; i++) {
	change = mdb_db_change_array_get_quick(change_list, i);
	bail_null(change);


	if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 5, "nkn", "nvsd", "server-map", "config", "*"))
	{
	    if (mdct_add == change->mdc_change_type){
		t_name = tstr_array_get_str_quick(change->mdc_name_parts, 4);
		err = md_nvsd_server_map_context_allocate(t_name,
			0, "", 0, &context);
		bail_error(err);
	    }
	}
	else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6, "nkn", "nvsd", "server-map", "config", "*", "file-url"))
	{
	    if ((mdct_modify == change->mdc_change_type) || (mdct_add == change->mdc_change_type))
	    {
		/* REMOVED -
		 * We use an action to create the timer now. The action is
		 * fired from the CLI
		 */

		/* Set a timer event here */
		t_name = tstr_array_get_str_quick(change->mdc_name_parts, 4);

		new_value = bn_attrib_array_get_quick(change->mdc_new_attribs, ba_value);
		if(new_value) {
		    err = bn_attrib_get_tstr(new_value, NULL, bt_string, NULL, &t_url);
		    bail_error(err);
		}

		if(strcmp(ts_str(t_url),"")!=0){
		    url_node_name = smprintf("/nkn/nvsd/server-map/config/%s/refresh", t_name);
		    bail_null(url_node_name);

		    node_name = smprintf("/nkn/nvsd/server-map/config/%s/format-type", t_name);
		    bail_null(node_name);

		    err = md_nvsd_server_map_context_find(t_name, &context);
		    bail_error(err);

		    err = mdb_get_node_value_uint32(commit, new_db, url_node_name, 0, NULL, &poll_frequency);
		    bail_error(err);

		    err = mdb_get_node_value_uint32(commit, new_db,
			    node_name, 0, NULL, &type);
		    bail_error(err);

		    poll_frequency = poll_frequency * 1000; /* convert to millseconds */


		    if (NULL != context) {
			/* already existing server-map. We need to delete the timer
			 * and create a new one
			 */
			err = md_nvsd_server_map_delete_poll_timer(context);
			bail_error(err);

			err = md_nvsd_server_map_context_free(context);
			bail_error(err);

		    } else {
			/* New server-map. fall-through to create a new timer */
		    }
		    err = md_nvsd_server_map_context_allocate(t_name, poll_frequency, ts_str(t_url), type, NULL);
		    bail_error(err);

		    err = md_nvsd_server_map_async_action(context->name, "1");
		    bail_error(err);
		}
	    }
	    else if (mdct_delete == change->mdc_change_type) {
		t_name = tstr_array_get_str_quick(change->mdc_name_parts, 4);

		node_name = smprintf("/nkn/nvsd/server-map/config/%s/refresh", t_name);
		bail_null(node_name);

		err = md_nvsd_server_map_context_find(t_name, &context);
		bail_error(err);


		if (NULL != context) {
		    /* already existing server-map. We need to delete the timer
		     * and create a new one
		     */
		    err = md_nvsd_server_map_delete_poll_timer(context);
		    bail_error(err);

		    err = md_nvsd_server_map_context_free(context);
		    bail_error(err);

		    err = md_nvsd_server_map_cleanup_temp_files(t_name, NULL, true);
		    bail_error(err);

		}
	    }
	}

	else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6, "nkn", "nvsd", "server-map", "config", "*", "format-type")
		&& (mdct_modify == change->mdc_change_type))
	{
	    t_name = tstr_array_get_str_quick(change->mdc_name_parts, 4);
	    bail_null(t_name);

	    err = mdb_set_node_str(NULL, (mdb_db *)new_db, bsso_reset, 0,
		    bt_bool,
		    "false",
		    "/nkn/nvsd/server-map/config/%s/map-status",
		    t_name);
	    bail_error(err);

	    /* Reset the refresh */
	    err = mdb_set_node_str(NULL, (mdb_db *)new_db, bsso_reset, 0,
		    bt_uint32,
		    "0",
		    "/nkn/nvsd/server-map/config/%s/refresh",
		    t_name);
	    bail_error(err);

	    /* Reset the file-url */
	    err = mdb_set_node_str(NULL, (mdb_db *)new_db, bsso_reset, 0,
		    bt_string,
		    "",
		    "/nkn/nvsd/server-map/config/%s/file-url",
		    t_name);
	    bail_error(err);

	    err = md_nvsd_server_map_context_find(t_name, &context);
	    bail_error(err);


	    if (NULL != context) {
		context->state = STATE_INVALID;
		err = md_nvsd_server_map_delete_poll_timer(context);
		bail_error(err);

	    }

	}
	else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6, "nkn", "nvsd", "server-map", "config", "*", "refresh")
		&& (mdct_modify == change->mdc_change_type))
	{
	    t_name = tstr_array_get_str_quick(change->mdc_name_parts, 4);
	    bail_null(t_name);

	    new_value = bn_attrib_array_get_quick(change->mdc_new_attribs, ba_value);
	    if(new_value) {
		err = bn_attrib_get_uint32(new_value, NULL, NULL, &poll_frequency);
		bail_error(err);
	    }


	    err = md_nvsd_server_map_context_find(t_name, &context);
	    bail_error(err);

	    if (NULL != context) {
		err = md_nvsd_server_map_delete_poll_timer(context);
		bail_error(err);
		if(poll_frequency > 0) {
		    context->poll_frequency = poll_frequency * 1000;
		    err = md_nvsd_server_map_set_poll_timer(context);
		    bail_error(err);
		}
	    }

	}
	else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 6, "nkn", "nvsd", "server-map", "config", "*", "map-status")
		&& (mdct_modify == change->mdc_change_type))
	{


	}
    }

bail:
    safe_free(node_name);
    ts_free(&ret_msg);
    safe_free(url_node_name);
    bn_request_msg_free(&req);
    ts_free(&map_status);
    ts_free(&t_url);
    return(err);
}

/* ------------------------------------------------------------------------- */
static int
md_nvsd_server_map_async_action(const char *name,
                         const char *config)
{
    int err = 0;
    const bn_binding *binding = NULL;
    int32 ret_code = 0;
    server_map_timer_data_t *context = NULL;
    tstring *msg = NULL;
    lc_launch_params *lp = NULL;
    char version_str[32] = {0};
    char *uri = NULL;
    char *file = NULL;
    servermap_refresh_event_data_t *edata = NULL;
    char force_refresh[4] = {0};

    /* XXX: add action handling here */

    err = md_nvsd_server_map_context_find(name, &context);
    bail_error(err);

    if (context == NULL) {
	goto bail;
    }
    edata = malloc(sizeof(servermap_refresh_event_data_t));
    bail_null(edata);

    err = parse_url(context->url, &uri, &file);
    bail_error(err);

    bail_null(uri);
    bail_null(file);

    edata->lr = malloc(sizeof(lc_launch_result));
    bail_null(edata->lr);
    memset(edata->lr, 0, sizeof(lc_launch_result));


    err = lc_launch_params_get_defaults(&lp);
    bail_error(err);

    err = ts_new_str(&(lp->lp_path), "/opt/nkn/bin/fetch_smap");
    bail_error(err);

    sprintf(version_str,"%d",context->version);

    // Added to fix PR 805962
    // If the previous fetch was a failure we have to do a forceful refresh
    // to avoid the issue mentioned above

    if((strcmp(config,"0")==0)&&(context->state != STATE_VALID)){
	strlcpy(force_refresh, "1", sizeof(force_refresh));
    } else {
	strlcpy(force_refresh, config, sizeof(force_refresh));
    }

    /*
     * /root/fetch_uf_list -t url -r 2 -c abc -f abc
     * -d cmbu-build04.juniper.net/data/users/dgautam -l sample-uf_data.txt
     */
    err = tstr_array_new_va_str(&(lp->lp_argv), NULL, 7, "/opt/nkn/bin/fetch_smap",
	    context->name, uri, file, format_type_map[context->type],
	    version_str, force_refresh);
    bail_error(err);

    lp->lp_wait = false;
    lp->lp_env_opts = eo_preserve_all;
    lp->lp_log_level = LOG_INFO;

    lp->lp_io_origins[lc_io_stdout].lio_opts = lioo_capture_nowait;
    lp->lp_io_origins[lc_io_stderr].lio_opts = lioo_capture_nowait;
    edata->action_trig_time = time(NULL);
    edata->smap_name = strdup(context->name);
    edata->uri = strdup(uri);
    edata->file = strdup(file);
    edata->version = context->version;

    context->version ++;

    err = lc_launch(lp, edata->lr);
    bail_error(err);

    err = lc_child_completion_register
	(md_cmpl_hand, edata->lr->lr_child_pid,
	 md_nvsd_server_map_refresh_async_comp, (void *)edata);
    bail_error(err);

bail:
    lc_launch_params_free(&lp);
    return err;
}




/* ------------------------------------------------------------------------- */
static int
md_nvsd_server_map_handle_action(md_commit *commit,
                         const mdb_db *db,
                         const char *action_name,
                         bn_binding_array *params,
                         void *cb_arg)
{
    int err = 0;
    char *name = NULL;
    const bn_binding *binding = NULL;
    int32 ret_code = 0;
    server_map_timer_data_t *context = NULL;
    tstring *msg = NULL;
    lc_launch_params *lp = NULL;
    char version_str[32] = {0};
    char *uri = NULL;
    char *file = NULL;
    servermap_refresh_event_data_t *edata = NULL;
    char *config = NULL;

    /* XXX: add action handling here */

    err = bn_binding_array_get_binding_by_name(params, "name", &binding);
    bail_error_null(err, binding);

    err = bn_binding_get_str(binding, ba_value, bt_string, 0, &name);
    bail_error_null(err, name);

    err = bn_binding_array_get_binding_by_name(params, "config", &binding);
    bail_error_null(err, binding);

    err = bn_binding_get_str(binding, ba_value, bt_string, 0, &config);
    bail_error(err);

    err = md_nvsd_server_map_async_action(name, config);
    bail_error(err);



bail:
    return err;
}

static int
md_nvsd_server_map_get_mon_nodes(
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
    const char *var_name = (const char *) arg;
    server_map_timer_data_t *context = NULL;

    bail_null(arg);

    // split the node name and get the smap name.
    err = bn_binding_name_to_parts_va(node_name, false, 1, 4, &t_str);
    bail_error_null(err, t_str);

    err = md_nvsd_server_map_context_find(ts_str(t_str), &context);
    bail_error_null(err, context);

    if(strcmp(var_name, "state")==0){
	//Fill in state
	err = bn_binding_new_uint32(ret_binding,
		node_name, ba_value,
		0, context->state);
	bail_error(err);
    } else if(strcmp(var_name, "map-bin-file")==0) {
	err = bn_binding_new_str_autoinval(ret_binding,
		node_name, ba_value, bt_string,
		0, context->file_path);
	bail_error(err);
    }

bail:
    ts_free(&t_str);
    return err;
}

static int
send_server_map_status_change_event(md_commit *commit, char *smap_name,
                            uint32 status, char *filename)
{
    int err = 0;
    bn_request *req = NULL;
    char state[32] = {0};
    tstring *ret_msg = NULL;
    uint16_t ret_code = 0;

    err = bn_event_request_msg_create(&req,
	    "/nkn/nvsd/server-map/notify/map-status-change");
    bail_error(err);

    err = bn_event_request_msg_append_new_binding (req, 0, "server_map_name",
	    bt_string, smap_name, NULL);
    bail_error(err);

    snprintf(state, 32, "%d", status);

    err = bn_event_request_msg_append_new_binding (req, 0, "status",
	    bt_uint32, (char *)&state, NULL);
    bail_error(err);

    err = bn_event_request_msg_append_new_binding (req, 0, "file_name",
	    bt_string, filename, NULL);
    bail_error(err);

    err = md_commit_handle_event_from_module
	(commit, req, &ret_code, &ret_msg, NULL, NULL);
    bail_error(err);

bail:
    bn_request_msg_free(&req);
    ts_free(&ret_msg);
    return err;
}

static int
md_nvsd_server_map_refresh_async_comp(lc_child_completion_handle *cmpl_hand,
        pid_t pid, int wait_status, tbool completed,
        void *arg)
{
    int err = 0;
    servermap_refresh_event_data_t *edata = arg;
    lc_launch_result *lr = edata->lr;
    const tstring *output = NULL;
    tstring *container = NULL;
    int32 offset1 = 0, offset2 = 0;
    bn_binding *binding = NULL;
    server_map_timer_data_t *context = NULL;
    int res = 0;

    bail_null(lr);

    if(completed) {
	lr->lr_child_pid = -1;
	res = lr->lr_exit_status;
	lr->lr_exit_status = wait_status;
    }

    err = lc_launch_complete_capture(lr);
    bail_error(err);

    err = md_nvsd_server_map_context_find(edata->smap_name, &context);
    bail_error_null(err,context);

    if((context->last_action_time > edata->action_trig_time)){
	//Action executed last already completed. This
	//callback is for action triggered before that
	//So ignore this
	return 0;
    } else {
	context->last_action_time = edata->action_trig_time;
    }

    output = lr->lr_captured_output;
    bail_null(output);

    if(strstr(ts_str(output),"RETURN=0")){
	//Successfully refreshed
	lc_log_basic(LOG_INFO, _("Refresh completed for server-map %s.Status %d"),
		edata->smap_name, 0);
	context->state = STATE_VALID;
	snprintf(context->file_path, 1024, "/nkn/tmp/smap/%s/%s.%d.bin",
		edata->smap_name, edata->file, edata->version);
	err = send_server_map_status_change_event(NULL, context->name,
		context->state, context->file_path);
	bail_error(err);

    } else if (strstr(ts_str(output),"RETURN=1")){
	//File fetch failed
	lc_log_basic(LOG_INFO, _("Refresh completed for server-map %s.Status %d"),
		edata->smap_name, 1);
	context->state = STATE_INVALID;
	err = send_server_map_status_change_event(NULL, context->name,
		context->state, context->file_path);
	bail_error(err);

    } else if (strstr(ts_str(output),"RETURN=2")){
	//No change in file
	//and hence no change in state
	lc_log_basic(LOG_INFO, _("Refresh completed for server-map %s.Status %d"),
		edata->smap_name, 2);
    }else if (strstr(ts_str(output),"RETURN=3")){
	//File parsing failed
	lc_log_basic(LOG_INFO, _("Refresh completed for server-map %s.Status %d"),
		edata->smap_name, 3);
	context->state = STATE_INVALID;
	err = send_server_map_status_change_event(NULL, context->name,
		context->state, context->file_path);
	bail_error(err);

    }

bail:
    if (lr) {
	lc_launch_result_free_contents(lr);
	safe_free(lr);
    }
    free(edata);
    ts_free(&container);
    bn_binding_free(&binding);
    return err;
}



/* ------------------------------------------------------------------------- */
int md_nvsd_server_map_set_poll_timer(server_map_timer_data_t *context)
{
    int err = 0;

    err = lew_event_reg_timer(md_lwc,
	    &(context->poll_event),
	    (context->callback),
	    (void *) (context),
	    (context->poll_frequency));
    bail_error(err);

bail:
    return err;
}

/* ------------------------------------------------------------------------- */
int md_nvsd_server_map_delete_poll_timer(server_map_timer_data_t *context)
{
    int err = 0;

    err = lew_event_delete(md_lwc, &(context->poll_event));
    complain_error(err);

bail:
    return err;
}

/* ------------------------------------------------------------------------- */
int md_nvsd_server_map_poll_timeout_handler(int fd,
					short event_type,
					void *data,
					lew_context *ctxt)
{
    int err = 0;
    int32 ret_code = 0;
    tstring *msg = NULL;
    bn_request *req = NULL;
    server_map_timer_data_t *context = (server_map_timer_data_t *) (data);

    bail_null(context);


    lc_log_basic(LOG_INFO, _("Timer expired for server-map %s."), context->name);

    err = md_nvsd_server_map_async_action(context->name, "0");
    bail_error(err);
    /* Finally.. set the timer again */
    if ( context->poll_frequency > 0 ) {
	err = md_nvsd_server_map_set_poll_timer(context);
	bail_error(err);
    }
bail:
    ts_free(&msg);
    bn_request_msg_free(&req);
    return err;
}


/* ------------------------------------------------------------------------- */
static int md_nvsd_server_map_fetch_file(server_map_timer_data_t *context, int32 *ret_code)
{
    int err = 0;
    int32 status = 0;
    char *uri = NULL;
    char *file = NULL;

    /* launch to download file */

    err = parse_url(context->url, &uri, &file);
    bail_error(err);

    bail_null(uri);
    bail_null(file);

    if(context->type == 4)
    {
	*ret_code = 3;
	return err;
    }

    err = lc_launch_quick_status(&status, NULL, false, 4,
	    prog_fetch_smap_path,
	    context->name,
	    uri,
	    file);
    bail_error(err);

    *ret_code = WEXITSTATUS(status);

bail:
    safe_free(uri);
    safe_free(file);
    return err;
}

/* ------------------------------------------------------------------------- */
static int md_nvsd_server_map_process_file(server_map_timer_data_t *context, int32 *ret_code, tstring **ret_msg, md_commit *commit)
{
    int err = 0;
    int status = 0;
    char *filename = NULL;
    char *t_input_file = NULL;
    char *t_output_file = NULL;
    bn_request *req = NULL;
    bn_binding *binding = NULL;
    tbuf *t_bin_contents = NULL;
    int32 max_file_length = (1024*1024); /* 1 MegaBytes */
    uint32 format_type = 0;

    if (ret_code)
	*ret_code = 0;
    if (ret_msg)
	*ret_msg = NULL;


    /*
     * "\t-i --input (input file)		[ /nkn/tmp/smap/${smap-name}/${file}.new ]
     * "\t-o --output (output file)		[ /nkn/tmp/smap/${smap-name}/$file.bin ]
     * "\t-t --type (XML root element)\n"
     */
    /* Call external binary to process the file. and get a glob pattern
     * "\t-o --output (output file)		[ /nkn/tmp/smap/${smap-name}/$file.bin ]
     * "\t-t --type (XML root element)\n"
     */
    /* Call external binary to process the file. and get a glob pattern
     */


    err = parse_url(context->url, NULL, &filename);
    bail_error_null(err, filename);

    t_input_file = smprintf("%s/%s/%s", path_smap_files, context->name, filename);
    bail_null(t_input_file);

    t_output_file = smprintf("%s/%s/%s.bin", path_smap_files, context->name, filename);
    bail_null(t_output_file);

    err = lc_launch_quick_status(&status, ret_msg, false, 7,
	    prog_mapxml_path,
	    "-i",
	    t_input_file,
	    "-o",
	    t_output_file,
	    "-t",
	    format_type_map[context->type]);
    bail_error(err);

    if ( WEXITSTATUS(status) == 0 ) {
	err = lf_read_file_tbuf(t_output_file, max_file_length, &t_bin_contents);
	bail_error(err);

	/* Contents read into local buffer.. now send to NVSD */
	/* TODO: */
	if (t_bin_contents == NULL || tb_size(t_bin_contents) <= 0) {
	    lc_log_basic(LOG_ERR, "could not read compiled server-map contents for %s.",
		    context->name);
	    goto bail;
	}

	/* Read succesfuly. send data to nvsd */

	err = bn_action_request_msg_create(&req, "/nkn/nvsd/server-map/actions/updated-map");
	bail_error_null(err, req);

	err = bn_action_request_msg_append_new_binding(req,
		0,
		"name",
		bt_string,
		context->name,
		NULL);
	bail_error(err);

	err = bn_action_request_msg_append_new_binding(req,
		0,
		"file-name",
		bt_string,
		t_output_file,
		NULL);
	bail_error(err);

	err = md_commit_handle_action_from_module(
		commit, req, NULL, NULL, NULL, NULL, NULL);
	bail_error(err);
    }
    else {
	lc_log_basic(LOG_NOTICE, _("Server map '%s' - failed to validate. exit with code (%d)"),
		context->name, WEXITSTATUS(status));
    }

    if (ret_code) {
	*ret_code = (int32) WEXITSTATUS(status);
    }
bail:
    bn_request_msg_free(&req);
    safe_free(filename);
    safe_free(t_output_file);
    safe_free(t_input_file);
    tb_free(&t_bin_contents);
    return err;
}


/* ------------------------------------------------------------------------- */
static int md_nvsd_server_map_context_init(void)
{
    int err = 0;
    int i = 0;

    for (i = 0; i < NKN_MAX_SERVER_MAPS; ++i) {
	memset(&(server_map_timers[i]), 0, sizeof(server_map_timer_data_t));
	server_map_timers[i].index = i;
	server_map_timers[i].context_data = &(server_map_timers[i]);
    }

bail:
    return err;
}

/* ------------------------------------------------------------------------- */
static int md_nvsd_server_map_context_allocate(
		const char *name,
		lt_dur_ms  poll_frequency,
		const char *url,
		uint32 type,
		server_map_timer_data_t **ret_context)
{
    int err = 0;
    int i = 0;
    server_map_timer_data_t *context = NULL;

    if (ret_context)
	*ret_context = NULL;

    bail_null(name);
    for(i = 0; i < NKN_MAX_SERVER_MAPS; ++i) {
	if (server_map_timers[i].name == NULL) {
	    context = &server_map_timers[i];
	    break;
	}
	if (strcmp(server_map_timers[i].name,name) == 0) {
	    context = &server_map_timers[i];
	    break;
	}
    }
    bail_null(context);

    context->name = smprintf("%s", name);
    bail_null(context->name);

    context->url = smprintf("%s", url);
    bail_null(context->url);

    context->callback = md_nvsd_server_map_poll_timeout_handler;
    context->poll_frequency = poll_frequency;
    context->type = type;
    context->version = 0;
    context->last_action_time = 0;
    context->file_path = (char *)malloc(sizeof(char)*1024);
    context->state = STATE_UNKNOWN;

    lc_log_basic(LOG_INFO, _("creating a timer for %s with poll period of %d ms."),
	    name, (unsigned int) poll_frequency);
    if(poll_frequency > 0)
    {
	err = md_nvsd_server_map_set_poll_timer(context);
	bail_error(err);
    }

    if (ret_context)
	*ret_context = context;

bail:
    return err;
}

/* ------------------------------------------------------------------------- */
static int md_nvsd_server_map_context_free(server_map_timer_data_t *context)
{
    int err = 0;

    if(NULL != context){

	safe_free(context->name);
	safe_free(context->url);
	context->callback = NULL;
	context->poll_frequency = 0;
    }
bail:
    return err;
}

/* ------------------------------------------------------------------------- */
static int md_nvsd_server_map_context_find(const char *name, server_map_timer_data_t **ret_context)
{
    int err = 0;
    int i = 0;

    bail_null(ret_context);

    *ret_context = NULL;
    for (i = 0; i < NKN_MAX_SERVER_MAPS; ++i) {
	if ( safe_strcmp(name, server_map_timers[i].name) == 0) {
	    *ret_context = &server_map_timers[i];
	    goto bail;
	}
    }

bail:
    return err;
}

/* ------------------------------------------------------------------------- */
int
parse_url(const char *url, char **ret_hostname, char **ret_file)
{
    int err = 0;
    const char *cl = NULL, *nl = NULL;

    if (ret_hostname) {
	*ret_hostname = NULL;
    }
    if (*ret_file) {
	*ret_file = NULL;
    }
    bail_null(url);

    if (!lc_is_prefix("http://", url, false)) {
	goto bail;
    }

    cl = url + strlen("http://");
    if (!*cl) {
	err = lc_err_bad_path;
	goto bail;
    }

    nl = rindex(cl, '/');
    if (!nl) {
	err = lc_err_bad_path;
	goto bail;
    }

    if (ret_hostname) {
	*ret_hostname = strldup(cl, nl - cl + 1);
    }
    if (ret_file) {
	*ret_file = strdup(nl+1);
    }
bail:
    return (err);
}


/* ------------------------------------------------------------------------- */
/* if the context is already existing, then delete it and create a new timer
 * context. If not, create a new timer context.
 * If the file-url changes, delete the old context and create a new
 * one.
 * If the timer interval changes, then delete the old context and create a new
 * one.
 *
 * In either case, first time download must always be attempted
 */
static int
md_nvsd_server_map_create_timer(md_commit *commit,
                         const mdb_db *db,
                         const char *action_name,
                         bn_binding_array *params,
                         void *cb_arg)
{
    int err = 0;
    server_map_timer_data_t *context = NULL;
    char *t_smap = NULL;
    uint32 new_poll_freq = 0, old_poll_freq = 0;
    char *node_name = NULL;
    tstring *t_url;
    char *url_node_name = NULL;
    const bn_binding *binding = NULL;
    int32 ret_code = 0;
    char *type_node_name = NULL;
    uint32 type = 0;
    tstring *msg = NULL;
    bn_request *req = NULL;

    /* get the server map name */
    err = bn_binding_array_get_binding_by_name(params, "name", &binding);
    bail_error_null(err, &binding);

    err = bn_binding_get_str(binding, ba_value, bt_string, 0, &t_smap);
    bail_error(err);

    /* Get the timer context */
    err = md_nvsd_server_map_context_find(t_smap, &context);
    bail_error(err);

    if (NULL == context) {
	/* First time the timer is being created */
	url_node_name = smprintf("/nkn/nvsd/server-map/config/%s/file-url", t_smap);
	bail_null(url_node_name);

	err = mdb_get_node_value_tstr(commit, db, url_node_name, 0, NULL, &t_url);
	bail_error(err);

	if ((NULL == t_url) || (ts_length(t_url) == 0)) {
	    err = md_commit_set_return_status_msg(commit, 0,
		    _("Bad file name"));
	    bail_error(err);
	    goto bail;
	}

	/* Get the new timer poll value */
	node_name = smprintf("/nkn/nvsd/server-map/config/%s/refresh", t_smap);
	bail_null(node_name);

	err = mdb_get_node_value_uint32(commit, db,
		node_name, 0,
		NULL, &new_poll_freq);
	bail_error(err);

	/* BZ 4836:
	 * get the format type and plug it into the context
	 */
	type_node_name = smprintf("/nkn/nvsd/server-map/config/%s/format-type", t_smap);
	bail_null(type_node_name);

	err = mdb_get_node_value_uint32(commit, db,
		type_node_name, 0,
		NULL, &type);
	bail_error(err);

	lc_log_basic(LOG_INFO, _("Creating timer for server-map '%s' : url = '%s', poll = %d sec"),
		t_smap, ts_str(t_url), new_poll_freq);
	err = md_nvsd_server_map_context_allocate(t_smap,
		(new_poll_freq * 1000), ts_str(t_url), type, &context);
	bail_error(err);
    }
    else {
	/* Timer context already existed..
	 * delete the timer and create a new one
	 */

	url_node_name = smprintf("/nkn/nvsd/server-map/config/%s/file-url", t_smap);
	bail_null(url_node_name);

	err = mdb_get_node_value_tstr(commit, db, url_node_name, 0, NULL, &t_url);
	bail_error(err);

	if (ts_equal_str(t_url, context->url, true)) {
	    /* No change in URL.. only poll frequency changed */
	}
	else {
	    /* URL changed. So change only the URL in the context */
	    safe_free(context->url);
	    context->url = smprintf("%s", ts_str(t_url));
	    lc_log_basic(LOG_INFO, _("server-map '%s' : changing file-url to '%s'"),
		    t_smap, ts_str(t_url));

	    err = md_nvsd_server_map_cleanup_temp_files(t_smap, NULL, false);
	    bail_error(err);
	}

	/* BZ 4836:
	 * get the format type and plug it into the context
	 */
	type_node_name = smprintf("/nkn/nvsd/server-map/config/%s/format-type", t_smap);
	bail_null(type_node_name);

	err = mdb_get_node_value_uint32(commit, db,
		type_node_name, 0,
		NULL, &type);
	bail_error(err);

	/* Get the new timer poll value */
	node_name = smprintf("/nkn/nvsd/server-map/config/%s/refresh", t_smap);
	bail_null(node_name);

	err = mdb_get_node_value_uint32(commit, db,
		node_name, 0,
		NULL, &new_poll_freq);
	bail_error(err);

	if ((new_poll_freq * 1000) != context->poll_frequency)
	{
	    /* Change in poll frequency - Delete the context,
	     * free it and create a new one */
	    err = md_nvsd_server_map_delete_poll_timer(context);
	    bail_error(err);

	    err = md_nvsd_server_map_context_free(context);
	    bail_error(err);

	    err = md_nvsd_server_map_context_allocate(t_smap,
		    (new_poll_freq * 1000), ts_str(t_url), type, NULL);
	    bail_error(err);

	}
    }

    err = md_nvsd_server_map_async_action(context->name, "1");
    bail_error(err);

bail:
    safe_free(type_node_name);
    safe_free(node_name);
    safe_free(url_node_name);
    safe_free(t_smap);
    bn_request_msg_free(&req);
    ts_free(&t_url);
    return err;
}

static int
md_nvsd_server_map_cleanup_temp_files(const char *smap,
		const char *url,
		tbool delete_map)
{
    int err = 0;
    char *smap_dir = NULL;
    tbool is_dir = false;

    bail_null(smap);

    smap_dir = smprintf("/nkn/tmp/smap/%s", smap);
    bail_null(smap_dir);

    err = lf_test_is_dir(smap_dir, &is_dir);
    bail_error(err);

    if( is_dir) {
	err = lf_remove_dir_contents(smap_dir);
	bail_error(err);

	if (delete_map) {
	    err = lf_remove_dir(smap_dir);
	    bail_error(err);
	}

    }

bail:
    safe_free(smap_dir);
    return err;
}
static int
md_nvsd_server_map_validate_name(const char *name,
			tstring **ret_msg)
{
    int err = 0;
    int i = 0;
    const char *p = "/\\*:|`\"?";
    int j = 0;
    int k = 0;
    int l = strlen(p);

    if (strlen(name) == 0) {
	if (ret_msg) {
	    err = ts_new_sprintf(ret_msg,
		    "Bad server-map name.");
	    err = lc_err_not_found;
	    goto bail;
	}
    }

    if (strcmp(name, "list") == 0) {
	if (ret_msg) {
	    err = ts_new_sprintf(ret_msg,
		    "Bad server-map name. Use of reserved keyword 'list' is not allowed");
	}
	err = lc_err_not_found;
	goto bail;

    }

    if (name[0] == '_') {
	if (ret_msg) {
	    err = ts_new_sprintf(ret_msg,
		    "Bad server-map name. "
		    "Name cannot start with a leading underscore ('_')");
	}
	err = lc_err_not_found;
	goto bail;
    }
    k = strlen(name);

    for (i = 0; i < k; i++) {
	for (j = 0; j < l; j++) {
	    if (p[j] == name[i]) {
		if (ret_msg)
		    err = ts_new_sprintf(ret_msg,
			    "Bad server-map name. "
			    "Name cannot contain the characters '%s'", p);
		err = lc_err_not_found;
		goto bail;
	    }
	}
    }

bail:
    return err;
}

/* ------------------------------------------------------------------------- */
