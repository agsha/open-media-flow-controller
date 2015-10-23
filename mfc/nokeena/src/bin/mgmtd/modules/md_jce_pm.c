/*
 * md_jce_pm.c
 * This file contain details of nodes of all the processes managed under PM.
 * Currently nodes are distribution in many fields. one process per file
 * Current way just adds ~60 files for processes under PM
 */

#include <unistd.h>
#include <fcntl.h>

#include "common.h"
#include "dso.h"
#include "md_mod_reg.h"
#include "md_upgrade.h"
#include "nkn_mgmt_common.h"

#define NKN_JCFG_SERVER_PATH          "/opt/jcf-gw/scripts/jcfg_server"
#define NKN_DPITPROXY_PATH            "/opt/nkn/sbin/http_analyzer"
#define NKN_DPITPROXY_WORKING_DIR     "/coredump/dpitproxy"

static int
md_jce_pm_add_initial(md_commit *commit, mdb_db *db, void *arg);
int md_jce_pm_init(const lc_dso_info *info, void *data);
int check_for_pacifica(void);


static const bn_str_value md_jce_pm_initial_values[] = {
    /*
     * JFG-SERVER: for REST-APIs support
     */
    {"/pm/process/jcfg-server/launch_enabled", bt_bool, "true"},
    {"/pm/process/jcfg-server/auto_launch", bt_bool, "false"},
    {"/pm/process/jcfg-server/launch_path", bt_string, NKN_JCFG_SERVER_PATH},
    {"/pm/process/jcfg-server/launch_uid", bt_uint16, "0"},
    {"/pm/process/jcfg-server/launch_gid", bt_uint16, "0"},
    {"/pm/process/jcfg-server/initial_launch_order", bt_int32, NKN_LAUNCH_ORDER_JCE_GW},

    /*
     * DPI TPROXY: for DPI based tproxy support
     */
    {"/pm/process/dpitproxy", bt_string, "dpitproxy"},
    {"/pm/process/dpitproxy/description", bt_string, "Dpi based tproxy"},
    {"/pm/process/dpitproxy/launch_enabled", bt_bool, "true"},
    {"/pm/process/dpitproxy/auto_launch", bt_bool, "false"},
    {"/pm/process/dpitproxy/auto_relaunch", bt_bool, "true"},
    {"/pm/process/dpitproxy/working_dir", bt_string, NKN_DPITPROXY_WORKING_DIR},
    {"/pm/process/dpitproxy/launch_path", bt_string, NKN_DPITPROXY_PATH},
    {"/pm/process/dpitproxy/launch_params/1", bt_uint8, "1"},
    {"/pm/process/dpitproxy/launch_params/1/param", bt_string, "eth21"},
    {"/pm/process/dpitproxy/launch_params/20", bt_uint8, "20"},
    {"/pm/process/dpitproxy/launch_params/20/param", bt_string, "-c"},
    {"/pm/process/dpitproxy/launch_params/21", bt_uint8, "21"},
    {"/pm/process/dpitproxy/launch_params/21/param", bt_string, "0x4"},
    {"/pm/process/dpitproxy/launch_params/22", bt_uint8, "22"},
    {"/pm/process/dpitproxy/launch_params/22/param", bt_string, "-n"},
    {"/pm/process/dpitproxy/launch_params/23", bt_uint8, "23"},
    {"/pm/process/dpitproxy/launch_params/23/param", bt_string, "3"},
    {"/pm/process/dpitproxy/launch_params/24", bt_uint8, "24"},
    {"/pm/process/dpitproxy/launch_params/24/param", bt_string, "-m"},
    {"/pm/process/dpitproxy/launch_params/25", bt_uint8, "25"},
    {"/pm/process/dpitproxy/launch_params/25/param", bt_string, "1024"},
    {"/pm/process/dpitproxy/launch_params/30", bt_uint8, "30"},
    {"/pm/process/dpitproxy/launch_params/30/param", bt_string, "--"},
    {"/pm/process/dpitproxy/launch_params/31", bt_uint8, "31"},
    {"/pm/process/dpitproxy/launch_params/31/param", bt_string, "-p"},
    {"/pm/process/dpitproxy/launch_params/32", bt_uint8, "32"},
    {"/pm/process/dpitproxy/launch_params/32/param", bt_string, "1"},
    {"/pm/process/dpitproxy/launch_uid", bt_uint16, "0"},
    {"/pm/process/dpitproxy/launch_gid", bt_uint16, "0"},
    {"/pm/process/dpitproxy/launch_priority", bt_int32, "0"},
    {"/pm/process/dpitproxy/launch_pre_delete", bt_string, ""},
    {"/pm/process/dpitproxy/initial_launch_order", bt_int32, NKN_LAUNCH_ORDER_DPITPROXY},
    {"/pm/process/dpitproxy/initial_launch_timeout", bt_duration_ms, "0"},
    {"/pm/process/dpitproxy/reexec_path", bt_string, ""},
    {"/pm/process/dpitproxy/shutdown_order", bt_int32, "100"},
    {"/pm/process/dpitproxy/shutdown_pre_delay", bt_duration_ms, "0"},
    {"/pm/process/dpitproxy/terminate_on_shutdown", bt_bool, "true"},
    {"/pm/process/dpitproxy/term_action", bt_name, ""},
    {"/pm/process/dpitproxy/restart_action", bt_string, "restart_self"},
    {"/pm/process/dpitproxy/kill_timeout", bt_duration_ms, "30000"},
    {"/pm/process/dpitproxy/liveness/enable", bt_bool, "false"},/* XXXX: enable? */
    {"/pm/process/dpitproxy/liveness/type", bt_string, "gcl_client"},
    {"/pm/process/dpitproxy/liveness/data", bt_string, ""},
    {"/pm/process/dpitproxy/liveness/hung_count", bt_uint32, "4"},
    {"/pm/process/dpitproxy/term_signals/normal/1", bt_string, "SIGTERM"},
    {"/pm/process/dpitproxy/term_signals/normal/2", bt_string, "SIGQUIT"},
    {"/pm/process/dpitproxy/term_signals/normal/3", bt_string, "SIGKILL"},
    {"/pm/process/dpitproxy/term_signals/force/1", bt_string, "SIGTERM"},
    {"/pm/process/dpitproxy/term_signals/force/2", bt_string, "SIGQUIT"},
    {"/pm/process/dpitproxy/term_signals/force/3", bt_string, "SIGKILL"},
    {"/pm/process/dpitproxy/term_signals/liveness/1", bt_string, "SIGTERM"},
    {"/pm/process/dpitproxy/term_signals/liveness/2", bt_string, "SIGQUIT"},
    {"/pm/process/dpitproxy/term_signals/liveness/3", bt_string, "SIGKILL"},
    {"/pm/process/dpitproxy/log_output", bt_string, "both"},
    {"/pm/process/dpitproxy/tz_change/action", bt_string, "none"},
    {"/pm/process/dpitproxy/tz_change/signal", bt_string, ""},
    {"/pm/process/dpitproxy/fd_limit", bt_uint32, "0"},
    {"/pm/process/dpitproxy/max_snapshots", bt_uint16, "10"},
    {"/pm/process/dpitproxy/delete_trespassers", bt_bool, "false"},
    {"/pm/process/dpitproxy/memory_size_limit", bt_int64, "-1"},/* RLIM_INFINITY */
    {"/pm/process/dpitproxy/expected_exit_code", bt_int16, "-1"},
    {"/pm/process/dpitproxy/expected_exit_backoff", bt_bool, "false"},
    {"/pm/process/dpitproxy/umask", bt_uint32, "0"},

};

static const bn_str_value md_jce_pm_upgrade_adds_1_to_2[] = {

    /*
     * DPI TPROXY: for DPI based tproxy support
     */
    {"/pm/process/dpitproxy", bt_string, "dpitproxy"},
    {"/pm/process/dpitproxy/description", bt_string, "Dpi based tproxy"},
    {"/pm/process/dpitproxy/launch_enabled", bt_bool, "true"},
    {"/pm/process/dpitproxy/auto_launch", bt_bool, "false"},
    {"/pm/process/dpitproxy/auto_relaunch", bt_bool, "false"},
    {"/pm/process/dpitproxy/working_dir", bt_string, NKN_DPITPROXY_WORKING_DIR},
    {"/pm/process/dpitproxy/launch_path", bt_string, NKN_DPITPROXY_PATH},
    {"/pm/process/dpitproxy/launch_params/1", bt_uint8, "1"},
    {"/pm/process/dpitproxy/launch_params/1/param", bt_string, "eth21"},
    {"/pm/process/dpitproxy/launch_uid", bt_uint16, "0"},
    {"/pm/process/dpitproxy/launch_gid", bt_uint16, "0"},
    {"/pm/process/dpitproxy/launch_priority", bt_int32, "0"},
    {"/pm/process/dpitproxy/launch_pre_delete", bt_string, ""},
    {"/pm/process/dpitproxy/initial_launch_order", bt_int32, NKN_LAUNCH_ORDER_DPITPROXY},
    {"/pm/process/dpitproxy/initial_launch_timeout", bt_duration_ms, "0"},
    {"/pm/process/dpitproxy/reexec_path", bt_string, ""},
    {"/pm/process/dpitproxy/shutdown_order", bt_int32, "100"},
    {"/pm/process/dpitproxy/shutdown_pre_delay", bt_duration_ms, "0"},
    {"/pm/process/dpitproxy/terminate_on_shutdown", bt_bool, "true"},
    {"/pm/process/dpitproxy/term_action", bt_name, ""},
    {"/pm/process/dpitproxy/restart_action", bt_string, "restart_self"},
    {"/pm/process/dpitproxy/kill_timeout", bt_duration_ms, "30000"},
    {"/pm/process/dpitproxy/liveness/enable", bt_bool, "false"},/* XXXX: enable? */
    {"/pm/process/dpitproxy/liveness/type", bt_string, "gcl_client"},
    {"/pm/process/dpitproxy/liveness/data", bt_string, ""},
    {"/pm/process/dpitproxy/liveness/hung_count", bt_uint32, "4"},
    {"/pm/process/dpitproxy/term_signals/normal/1", bt_string, "SIGTERM"},
    {"/pm/process/dpitproxy/term_signals/normal/2", bt_string, "SIGQUIT"},
    {"/pm/process/dpitproxy/term_signals/normal/3", bt_string, "SIGKILL"},
    {"/pm/process/dpitproxy/term_signals/force/1", bt_string, "SIGTERM"},
    {"/pm/process/dpitproxy/term_signals/force/2", bt_string, "SIGQUIT"},
    {"/pm/process/dpitproxy/term_signals/force/3", bt_string, "SIGKILL"},
    {"/pm/process/dpitproxy/term_signals/liveness/1", bt_string, "SIGTERM"},
    {"/pm/process/dpitproxy/term_signals/liveness/2", bt_string, "SIGQUIT"},
    {"/pm/process/dpitproxy/term_signals/liveness/3", bt_string, "SIGKILL"},
    {"/pm/process/dpitproxy/log_output", bt_string, "both"},
    {"/pm/process/dpitproxy/tz_change/action", bt_string, "none"},
    {"/pm/process/dpitproxy/tz_change/signal", bt_string, ""},
    {"/pm/process/dpitproxy/fd_limit", bt_uint32, "0"},
    {"/pm/process/dpitproxy/max_snapshots", bt_uint16, "10"},
    {"/pm/process/dpitproxy/delete_trespassers", bt_bool, "true"},
    {"/pm/process/dpitproxy/memory_size_limit", bt_int64, "-1"},/* RLIM_INFINITY */
    {"/pm/process/dpitproxy/expected_exit_code", bt_int16, "-1"},
    {"/pm/process/dpitproxy/expected_exit_backoff", bt_bool, "false"},
    {"/pm/process/dpitproxy/umask", bt_uint32, "0"},

};

static const bn_str_value md_jce_pm_upgrade_adds_3_to_4[] = {
    {"/pm/process/dpitproxy/launch_params/20", bt_uint8, "20"},
    {"/pm/process/dpitproxy/launch_params/20/param", bt_string, "-c"},
    {"/pm/process/dpitproxy/launch_params/21", bt_uint8, "21"},
    {"/pm/process/dpitproxy/launch_params/21/param", bt_string, "0x4"},
    {"/pm/process/dpitproxy/launch_params/22", bt_uint8, "22"},
    {"/pm/process/dpitproxy/launch_params/22/param", bt_string, "-n"},
    {"/pm/process/dpitproxy/launch_params/23", bt_uint8, "23"},
    {"/pm/process/dpitproxy/launch_params/23/param", bt_string, "3"},
    {"/pm/process/dpitproxy/launch_params/24", bt_uint8, "24"},
    {"/pm/process/dpitproxy/launch_params/24/param", bt_string, "-m"},
    {"/pm/process/dpitproxy/launch_params/25", bt_uint8, "25"},
    {"/pm/process/dpitproxy/launch_params/25/param", bt_string, "1024"},
    {"/pm/process/dpitproxy/launch_params/30", bt_uint8, "30"},
    {"/pm/process/dpitproxy/launch_params/30/param", bt_string, "--"},
    {"/pm/process/dpitproxy/launch_params/31", bt_uint8, "31"},
    {"/pm/process/dpitproxy/launch_params/31/param", bt_string, "-p"},
    {"/pm/process/dpitproxy/launch_params/32", bt_uint8, "32"},
    {"/pm/process/dpitproxy/launch_params/32/param", bt_string, "1"},
};

static md_upgrade_rule_array *md_jce_pm_upgrade_rules = NULL;


static int
md_jce_pm_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
    int err = 0;

    err = mdb_create_node_bindings(
	    commit,
	    db,
	    md_jce_pm_initial_values,
	    sizeof(md_jce_pm_initial_values)/ sizeof(bn_str_value));
    bail_error(err);

bail:
    return err;
}

int
md_jce_pm_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

     /*
     * We need modrf_namespace_unrestricted to allow us to set nodes
     * from our initial values function that are not underneath our
     * module root.
     */
    err = mdm_add_module("jce_pm",
	    5,
	    "/pm/nokeena/jce", NULL,
	    /* we are registering for /pm */
	    modrf_namespace_unrestricted,
	    NULL, NULL,
	    NULL, NULL,
	    NULL, NULL,
	    200, 0,
	    md_generic_upgrade_downgrade,
	    &md_jce_pm_upgrade_rules,
	    md_jce_pm_add_initial, NULL,
	    NULL, NULL,
	    NULL, NULL,
	    &module);
    bail_error(err);

    /* ------------------------------------------------------------------------
     * Upgrade registration
     */

    err = md_upgrade_rule_array_new(&md_jce_pm_upgrade_rules);
    bail_error(err);
    ra = md_jce_pm_upgrade_rules;

    err = md_upgrade_add_add_rules
          (ra, md_jce_pm_upgrade_adds_1_to_2, 
          sizeof(md_jce_pm_upgrade_adds_1_to_2) / sizeof(bn_str_value), 1, 2);
    bail_error(err);
    
    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =        MUTT_MODIFY;
    rule->mur_name =                "/pm/process/dpitproxy/launch_path";
    rule->mur_new_value_type =      bt_string;
    rule->mur_new_value_str =       NKN_DPITPROXY_PATH;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type =	MUTT_MODIFY;
    rule->mur_name =		"/pm/process/dpitproxy/delete_trespassers";
    rule->mur_new_value_type =	bt_bool;
    rule->mur_new_value_str =	"false";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type =        MUTT_MODIFY;
    rule->mur_name =                "/pm/process/dpitproxy/launch_path";
    rule->mur_new_value_type =      bt_string;
    rule->mur_new_value_str =       NKN_DPITPROXY_PATH;
    MD_ADD_RULE(ra);

    err = md_upgrade_add_add_rules
          (ra, md_jce_pm_upgrade_adds_3_to_4, 
          sizeof(md_jce_pm_upgrade_adds_3_to_4) / sizeof(bn_str_value), 3, 4);
    bail_error(err);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =	MUTT_MODIFY;
    rule->mur_name =		"/pm/process/dpitproxy/auto_relaunch";
    rule->mur_new_value_type =	bt_bool;
    rule->mur_new_value_str =	"true";
    MD_ADD_RULE(ra);

bail:
    return err;
}
