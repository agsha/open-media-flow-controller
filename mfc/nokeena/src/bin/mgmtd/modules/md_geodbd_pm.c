/*
 *
 * Filename:  md_geodbd_pm.c
 * Date:      23 May 2011
 * Author:    Kiran Desai
 *
 * (C) Copyright 2011 Juniper Networks, Inc.
 * All rights reserved.
 *
 *
 */

/* ------------------------------------------------------------------------- */
/* md_geodbd_pm.c: shows how to add new instances of wildcard nodes to
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

#define	GEODBD_PATH	 "/opt/nkn/sbin/geodbd"
#define GEODBD_WORKING_DIR "/coredump/geodbd"

int md_geodbd_pm_init(const lc_dso_info *info, void *data);
static md_upgrade_rule_array *md_geodbd_pm_ug_rules = NULL;

/* ------------------------------------------------------------------------- */
/* In initial values, we are not required to specify values for nodes
 * underneath wildcards for which we want to accept the default.
 * Here we are just specifying the ones where the default is not what
 * we want, or where we don't want to rely on the default not changing.
 */
const bn_str_value md_geodbd_pm_initial_values[] = {
    /* .....................................................................
     * geodbd daemon: basic launch configuration.
     */
    {"/pm/process/geodbd/launch_enabled", bt_bool, "true"},
    {"/pm/process/geodbd/auto_launch", bt_bool, "true"},
    {"/pm/process/geodbd/working_dir", bt_string, GEODBD_WORKING_DIR},
    {"/pm/process/geodbd/launch_path", bt_string, GEODBD_PATH},
    {"/pm/process/geodbd/launch_uid", bt_uint16, "0"},
    {"/pm/process/geodbd/launch_gid", bt_uint16, "0"},
    {"/pm/process/geodbd/kill_timeout", bt_duration_ms, "12000"},
    {"/pm/process/geodbd/term_signals/force/1", bt_string, "SIGKILL"},
    {"/pm/process/geodbd/term_signals/normal/1", bt_string, "SIGKILL"},
    {"/pm/process/geodbd/environment/set/PYTHONPATH", bt_string, "PYTHONPATH"},
    {"/pm/process/geodbd/environment/set/PYTHONPATH/value", bt_string, "${PYTHONPATH}:/nkn/plugins"},
    {"/pm/process/geodbd/initial_launch_order", bt_int32, NKN_LAUNCH_ORDER_ADNSD},
    {"/pm/process/geodbd/initial_launch_timeout", bt_duration_ms, NKN_LAUNCH_TIMEOUT_ADNSD},
    {"/pm/process/geodbd/description", bt_string, "GEODBD"},
    {"/pm/process/geodbd/environment/set/LD_LIBRARY_PATH", bt_string, "LD_LIBRARY_PATH"},
    {"/pm/process/geodbd/environment/set/LD_LIBRARY_PATH/value", bt_string, "/opt/nkn/lib:/lib/nkn"},

    /* Set additional library paths that need to be saved in case of a core dump */
    {"/pm/process/geodbd/save_paths/1", bt_uint8, "1"},
    {"/pm/process/geodbd/save_paths/1/path", bt_string, "/lib/nkn"},
    {"/pm/process/geodbd/save_paths/2", bt_uint8, "2"},
    {"/pm/process/geodbd/save_paths/2/path", bt_string, "/opt/nkn/lib"},

};


/* ------------------------------------------------------------------------- */
static int
md_geodbd_pm_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
    int err = 0;

    err = mdb_create_node_bindings
        (commit, db, md_geodbd_pm_initial_values,
         sizeof(md_geodbd_pm_initial_values) / sizeof(bn_str_value));
    bail_error(err);

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
int
md_geodbd_pm_init(const lc_dso_info *info, void *data)
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
        ("geodbd_pm", 0, "/nkn/geodbd", NULL,
         modrf_namespace_unrestricted,
         NULL, NULL, NULL, NULL, NULL, NULL, 300, 0,
         md_generic_upgrade_downgrade,
	 &md_geodbd_pm_ug_rules,
         md_geodbd_pm_add_initial, NULL,
         NULL, NULL, NULL, NULL, &module);
    bail_error(err);

    /* Upgrade processing nodes */
    err = md_upgrade_rule_array_new(&md_geodbd_pm_ug_rules);
    bail_error(err);
    ra = md_geodbd_pm_ug_rules;

 bail:
    return(err);
}
