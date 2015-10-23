/*
 *
 * Filename:    md_pacifica_resiliency_pm.c
 * Date:        12/22/2014
 * Author:      Jeya ganesh babu J
 *
 * (C) Copyright 2014 Juniper Networks, Inc.
 * All rights reserved/
 *
 */

/*----------------------------------------------------------------------------
 * md_pacifica_resiliency_pm.c: shows how the md_pacifica_resiliency_pm.c module is added to
 * the initial PM database.
 *
 * This module does not register any nodes of its own. Its sole purpose
 * is to add/change nodes in the initial database.
 *---------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
 * HEADER FILES
 *---------------------------------------------------------------------------*/
#include "common.h"
#include "dso.h"
#include "md_mod_reg.h"
#include "md_upgrade.h"
#include "nkn_mgmt_common.h"

/*----------------------------------------------------------------------------
 * PREPROCESSOR MACROS/CONSTANTS
 *---------------------------------------------------------------------------*/
#define NKN_PACIFICA_RESILIENCY_PATH    "/opt/nkn/bin/pacifica_resiliency.py"
#define NKN_PACIFICA_RESILIENCY_WORKING_DIR    "/coredump/pacifica_resiliency"


/*----------------------------------------------------------------------------
 * PRIVATE VARIABLE DECLARATIONS
 *---------------------------------------------------------------------------*/
static md_upgrade_rule_array *md_pacifica_resiliency_pm_ug_rules = NULL;

static const bn_str_value md_pacifica_resiliency_pm_initial_values[] = {
    /*
     * nknpacifica_resiliency : basic launch configuration.
     */
    {"/pm/process/nknpacifica_resiliency/launch_enabled", bt_bool, "true"},
    {"/pm/process/nknpacifica_resiliency/auto_launch", bt_bool, "true"},
    {"/pm/process/nknpacifica_resiliency/delete_trespassers", bt_bool, "false"},
    {"/pm/process/nknpacifica_resiliency/working_dir", bt_string,
	NKN_PACIFICA_RESILIENCY_WORKING_DIR},
    {"/pm/process/nknpacifica_resiliency/launch_path", bt_string,
	NKN_PACIFICA_RESILIENCY_PATH},
    {"/pm/process/nknpacifica_resiliency/launch_uid", bt_uint16, "0"},
    {"/pm/process/nknpacifica_resiliency/launch_gid", bt_uint16, "0"},

    {"/pm/process/nknpacifica_resiliency/initial_launch_order", bt_int32,
	NKN_LAUNCH_ORDER_PACIFICA_RESILIENCY},
    {"/pm/process/nknpacifica_resiliency/initial_launch_timeout", bt_duration_ms,
	NKN_LAUNCH_TIMEOUT_PACIFICA_RESILIENCY  },

};

/*----------------------------------------------------------------------------
 * FUNCTION DECLARATIONS
 *---------------------------------------------------------------------------*/
int md_pacifica_resiliency_pm_init(const lc_dso_info *info, void *data);


static int md_pacifica_resiliency_initial(md_commit *commit, mdb_db *db, void *arg)
{
    int err = 0;

    err = mdb_create_node_bindings(
	    commit,
	    db,
	    md_pacifica_resiliency_pm_initial_values,
	    sizeof(md_pacifica_resiliency_pm_initial_values)/ sizeof(bn_str_value));
    bail_error(err);

bail:
    return err;
}

int
md_pacifica_resiliency_pm_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    tbool exist = false;
    md_module *module = NULL;
    md_reg_node *node = NULL;

    err = lf_test_is_regular("/etc/pacifica_hw", &exist);
    if (err || !exist) {
	/* This is not pacifica platform
	 * return
	 */
	err = 0;
	goto bail;
    }

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
                    "pacifica_resiliency",
                    1,
                    "/pm/nokeena/pacifica_resiliency",
                    NULL,
                    modrf_namespace_unrestricted,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    200,
                    0,
                    md_generic_upgrade_downgrade,
                    NULL,
                    md_pacifica_resiliency_initial,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    &module);
    bail_error(err);

bail:
    return err;
}

