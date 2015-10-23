/*
 *
 * Filename:  md_l4proxy_pm.c
 * Date:      2011-05-11
 * Author:    Bijoy Chandrasekharan
 *
 * (C) Copyright 2011 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

/* ------------------------------------------------------------------------- */
/* md_l4proxy_pm.c: shows how to add new instances of wildcard nodes to
 * the initial database, and how to override defaults for nodes
 * created by Samara base components.  Also shows how to add a process
 * of yours to be launched and monitored by Process Manager.
 *
 * This module does not register any nodes of its own: it's sole
 * purpose is to add/change nodes in the initial database.  But there
 * would be no problem if we did want to register nodes here.
 */

#include "common.h"
#include "dso.h"
#include "md_mod_reg.h"
#include "md_upgrade.h"
#include "nkn_mgmt_common.h"

#define	PROXYD_PATH	 "/opt/nkn/sbin/proxyd"
#define PROXYD_WORKING_DIR "/coredump/proxyd"
#define PROXYD_CONFIG_FILE "/config/nkn/nkn.conf.default"

int md_l4proxy_pm_init(const lc_dso_info *info, void *data);
static md_upgrade_rule_array *md_proxyd_pm_ug_rules = NULL;

/* ------------------------------------------------------------------------- */
/* In initial values, we are not required to specify values for nodes
 * underneath wildcards for which we want to accept the default.
 * Here we are just specifying the ones where the default is not what
 * we want, or where we don't want to rely on the default not changing.
 */
const bn_str_value md_proxyd_pm_initial_values[] = {
    /* .....................................................................
     * proxyd daemon: basic launch configuration.
     */
    {"/pm/process/proxyd/launch_enabled", bt_bool, "true"},
    {"/pm/process/proxyd/auto_launch", bt_bool, "true"},
    {"/pm/process/proxyd/working_dir", bt_string, PROXYD_WORKING_DIR},
    {"/pm/process/proxyd/launch_path", bt_string, PROXYD_PATH},
    {"/pm/process/proxyd/launch_uid", bt_uint16, "0"},
    {"/pm/process/proxyd/launch_gid", bt_uint16, "0"},
    {"/pm/process/proxyd/kill_timeout", bt_duration_ms, "12000"},
    {"/pm/process/proxyd/term_signals/force/1", bt_string, "SIGKILL"},
    {"/pm/process/proxyd/term_signals/normal/1", bt_string, "SIGKILL"},
    {"/pm/process/proxyd/launch_params/1/param", bt_string, "-f"},
    {"/pm/process/proxyd/launch_params/2/param", bt_string, PROXYD_CONFIG_FILE},
    {"/pm/process/proxyd/launch_params/3/param", bt_string, "-D"},
    {"/pm/process/proxyd/environment/set/PYTHONPATH", bt_string, "PYTHONPATH"},
    {"/pm/process/proxyd/environment/set/PYTHONPATH/value", bt_string, "${PYTHONPATH}:/nkn/plugins"},
    {"/pm/process/proxyd/initial_launch_order", bt_int32, NKN_LAUNCH_ORDER_PROXYD},
    {"/pm/process/proxyd/initial_launch_timeout", bt_duration_ms, NKN_LAUNCH_TIMEOUT_PROXYD},
    /* .....................................................................
     * Other samara node overrides
     */
    /* Set open files limit to a large value */
    {"/pm/process/proxyd/fd_limit", bt_uint32, "64000"},
    {"/pm/process/proxyd/description", bt_string, "L4Proxy"},
    {"/pm/process/proxyd/environment/set/LD_LIBRARY_PATH", bt_string, "LD_LIBRARY_PATH"},
    {"/pm/process/proxyd/environment/set/LD_LIBRARY_PATH/value", bt_string, "/opt/nkn/lib:/lib/nkn"},

    /* Set additional library paths that need to be saved in case of a core dump */
    {"/pm/process/proxyd/save_paths/1", bt_uint8, "1"},
    {"/pm/process/proxyd/save_paths/1/path", bt_string, "/lib/nkn"},
    {"/pm/process/proxyd/save_paths/2", bt_uint8, "2"},
    {"/pm/process/proxyd/save_paths/2/path", bt_string, "/opt/nkn/lib"},

    /* .....................................................................
     * Echo daemon: liveness configuration (optional).
     */
    /* ---- NVSD Liveness ------*/
    {"/pm/process/proxyd/liveness/enable", bt_bool, "false"},
    {"/pm/process/proxyd/liveness/type", bt_string, "gcl_client"},
    {"/pm/process/proxyd/liveness/data", bt_string, "/nkn/proxyd/state/alive"},
    {"/pm/process/proxyd/liveness/hung_count", bt_uint32, "3"},
    /* ---- NVSD Liveness ------  */

    /* ---- NVSD Liveness term signals ----- */
    {"/pm/process/proxyd/term_signals/liveness/1", bt_string, "SIGQUIT"},
    {"/pm/process/proxyd/term_signals/liveness/2", bt_string, "SIGKILL"},
    {"/pm/process/proxyd/term_signals/liveness/3", bt_string, "SIGTERM"},

    /* .....................................................................
     * Global PM config overrides.  Let's pretend proxyd needs 90 seconds
     * to get going before it's safe to test it for liveness, but that it
     * should be tested every 30 seconds thereafter.
     */
};


/* ------------------------------------------------------------------------- */
static int
md_proxyd_pm_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
    int err = 0;

    err = mdb_create_node_bindings
        (commit, db, md_proxyd_pm_initial_values,
         sizeof(md_proxyd_pm_initial_values) / sizeof(bn_str_value));
    bail_error(err);

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
int
md_l4proxy_pm_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    /*
     * Commit order of 200 is greater than the 0 used by most modules,
     * including md_pm.c.  This is to ensure that we can override some
     * of PM's global configuration, such as liveness grace period.
     *
     * We need modrf_namespace_unrestricted to allow us to set nodes
     * from our initial values function that are not underneath our
     * module root.
     */
    err = mdm_add_module
        ("proxyd_pm", 0, "/nkn/proxyd", NULL,
         modrf_namespace_unrestricted,
         NULL, NULL, NULL, NULL, NULL, NULL, 300, 0,
         md_generic_upgrade_downgrade,
	 &md_proxyd_pm_ug_rules,
         md_proxyd_pm_add_initial, NULL,
         NULL, NULL, NULL, NULL, &module);
    bail_error(err);

    /* Upgrade processing nodes */
    err = md_upgrade_rule_array_new(&md_proxyd_pm_ug_rules);
    bail_error(err);
    ra = md_proxyd_pm_ug_rules;

 bail:
    return(err);
}
