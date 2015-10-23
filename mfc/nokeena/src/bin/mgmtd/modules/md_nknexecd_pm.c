/*
 *
 * Filename:  md_nknexecd_pm.c
 * Date:      2012-03-06
 * Author:    Dheeraj Gautam
 *
 * (C) Copyright 2012 Juniper Networks, Inc.
 * All rights reserved.
 *
 *
 */


#include "common.h"
#include "dso.h"
#include "md_mod_reg.h"
#include "md_upgrade.h"
#include "nkn_mgmt_common.h"

#define	    DAEMON_NAME	"nknexecd"
#define     DAEMON_PATH         "/opt/nkn/sbin/"DAEMON_NAME
#define     DAMEON_WORKING_DIR  "/coredump/"DAEMON_NAME

int md_nknexecd_pm_init(const lc_dso_info *info, void *data);
static md_upgrade_rule_array *md_daemon_ug_rules = NULL;

/* ------------------------------------------------------------------------- */
/* In initial values, we are not required to specify values for nodes
 * underneath wildcards for which we want to accept the default.
 * Here we are just specifying the ones where the default is not what
 * we want, or where we don't want to rely on the default not changing.
 */
const bn_str_value md_daemon_pm_initial_values[] = {
    /* .....................................................................
     * ucfltd daemon: basic launch configuration.
     */
    {"/pm/process/"DAEMON_NAME"/launch_enabled", bt_bool, "true"},
    {"/pm/process/"DAEMON_NAME"/auto_launch", bt_bool, "true"},
    {"/pm/process/"DAEMON_NAME"/working_dir", bt_string, DAMEON_WORKING_DIR},
    {"/pm/process/"DAEMON_NAME"/launch_path", bt_string, DAEMON_PATH},
    {"/pm/process/"DAEMON_NAME"/launch_uid", bt_uint16, "0"},
    {"/pm/process/"DAEMON_NAME"/launch_gid", bt_uint16, "0"},
    {"/pm/process/"DAEMON_NAME"/initial_launch_order", bt_int32, NKN_LAUNCH_ORDER_EXECD},
    {"/pm/process/"DAEMON_NAME"/initial_launch_timeout", bt_duration_ms, NKN_LAUNCH_TIMEOUT_EXECD},
    /* .....................................................................
     * Other samara node overrides
     */
    /* NOTE: 45 is maximum limit asked by feature owner, look like
     * pm uses till 75 fds, so start-up was failing daemons
     */

    /* Set open files limit to a large value */
    {"/pm/process/"DAEMON_NAME"/fd_limit", bt_uint32, "100"},
    /* 100 MB = 100 * 1024 * 1024 ; */
    {"/pm/process/"DAEMON_NAME"/memory_size_limit", bt_int64, "104857600" },
    {"/pm/process/"DAEMON_NAME"/description", bt_string, "NKN Execute Daemon"},
    /* don't want to put stderr/stdout to syslog */
    {"/pm/process/"DAEMON_NAME"/log_output", bt_string, "none"},
};


/* ------------------------------------------------------------------------- */
static int
md_daemon_pm_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
    int err = 0;

    err = mdb_create_node_bindings
        (commit, db, md_daemon_pm_initial_values,
         sizeof(md_daemon_pm_initial_values) / sizeof(bn_str_value));
    bail_error(err);

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
int
md_nknexecd_pm_init(const lc_dso_info *info, void *data)
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
    err = mdm_add_module(
	    "nknexecd_pm",			// module_name
	    0,				// module_version
	    "/pm/nokeena/nknexecd",		// root_node_name
	    NULL,				// root_config_name
	    0,				// md_mod_flags
	    NULL,				// side_effects_func
	    NULL,				// side_effects_arg
	    NULL,				// check_func
	    NULL,				// check_arg
	    NULL,				// apply_func
	    NULL,				// apply_arg
	    300,				// commit_order
	    0,				// apply_order
	    md_generic_upgrade_downgrade,	// updown_func
	    &md_daemon_ug_rules,		// updown_arg
	    md_daemon_pm_add_initial,	// initial_func
	    NULL,				// initial_arg
	    NULL,				// mon_get_func
	    NULL,				// mon_get_arg
	    NULL,				// mon_iterate_func
	    NULL,				// mon_iterate_arg
	    &module);			// ret_module
    bail_error(err);

    /* Upgrade processing nodes */
    err = md_upgrade_rule_array_new(&md_daemon_ug_rules);
    bail_error(err);
    ra = md_daemon_ug_rules;

bail:
    return(err);
}

