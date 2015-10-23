/*
 *
 * Filename:    md_irqpin_pm.c
 * Date:        2008-11-13
 * Author:      Amal P
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved/
 *
 */

/*----------------------------------------------------------------------------
 * md_irqpin_pm.c: shows how the md_irqpin_pm.c module is added to
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
#define NKN_IRQPIN_IXGBED_PATH    "/opt/nkn/bin/irqpin_ixgbe.py"
#define NKN_IRQPIN_IXGBED_WORKING_DIR    "/coredump/irqpin_ixgbe"


/*----------------------------------------------------------------------------
 * PRIVATE VARIABLE DECLARATIONS
 *---------------------------------------------------------------------------*/
static md_upgrade_rule_array *md_irqpin_ixgbed_pm_ug_rules = NULL;

static const bn_str_value md_irqpin_ixgbed_pm_initial_values[] = {
    /*
     * nknirqpin_ixgbed : basic launch configuration.
     */
    {"/pm/process/nknirqpin_ixgbed/launch_enabled", bt_bool, "true"},
    {"/pm/process/nknirqpin_ixgbed/auto_launch", bt_bool, "true"},
    {"/pm/process/nknirqpin_ixgbed/delete_trespassers", bt_bool, "false"},
    {"/pm/process/nknirqpin_ixgbed/working_dir", bt_string,
	NKN_IRQPIN_IXGBED_WORKING_DIR},
    {"/pm/process/nknirqpin_ixgbed/launch_path", bt_string,
	NKN_IRQPIN_IXGBED_PATH},
    {"/pm/process/nknirqpin_ixgbed/launch_uid", bt_uint16, "0"},
    {"/pm/process/nknirqpin_ixgbed/launch_gid", bt_uint16, "0"},

    {"/pm/process/nknirqpin_ixgbed/initial_launch_order", bt_int32,
	NKN_LAUNCH_ORDER_IRQPIN_IXGBED},
    {"/pm/process/nknirqpin_ixgbed/initial_launch_timeout", bt_duration_ms,
	NKN_LAUNCH_TIMEOUT_IRQPIN_IXGBED  },

};

/*----------------------------------------------------------------------------
 * FUNCTION DECLARATIONS
 *---------------------------------------------------------------------------*/
int md_irqpin_ixgbed_pm_init(const lc_dso_info *info, void *data);


static int md_irqpin_pm_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
    int err = 0;

    err = mdb_create_node_bindings(
	    commit,
	    db,
	    md_irqpin_ixgbed_pm_initial_values,
	    sizeof(md_irqpin_ixgbed_pm_initial_values)/ sizeof(bn_str_value));
    bail_error(err);

bail:
    return err;
}

int
md_irqpin_ixgbed_pm_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;

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
                    "irqpin_ixgbed",
                    1,
                    "/pm/nokeena/irqpin_ixgbed",
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
                    md_irqpin_pm_add_initial,
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

