/*
 *
 * Filename:    md_cache_meta_evictd_pm.c
 * Date:        2011-01-06
 * Author:      Manikandan Vengatachalam
 *
 * (C) Copyright 2011 Juniper Networks, Inc.
 * All rights reserved
 *
 */

/*----------------------------------------------------------------------------
 * md_cache_meta_evictd_pm.c: shows how the cache_meta_evictd module is added to
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
#define NKN_CACHE_META_EVICTD_PATH          "/opt/nkn/bin/cache_meta_evictd.sh"
#define NKN_CACHE_META_EVICTD_WORKING_DIR   "/coredump/cache_meta_evictd"
#define DEFAULT_SERVER_IP       "127.0.0.1"
#define DEFAULT_SERVER_PORT     "80"


/*----------------------------------------------------------------------------
 * PRIVATE VARIABLE DECLARATIONS
 *---------------------------------------------------------------------------*/

static const bn_str_value md_cache_meta_evictd_pm_initial_values[] = {
        /*
         * nkncache_evictd : basic launch configuration.
         */
        {"/pm/process/nkncache_meta_evictd/launch_enabled", bt_bool, "true"},
        {"/pm/process/nkncache_meta_evictd/auto_launch", bt_bool, "true"},
        {"/pm/process/nkncache_meta_evictd/delete_trespassers", bt_bool, "false"},
        {"/pm/process/nkncache_meta_evictd/working_dir", bt_string, NKN_CACHE_META_EVICTD_WORKING_DIR},
        {"/pm/process/nkncache_meta_evictd/launch_path", bt_string, NKN_CACHE_META_EVICTD_PATH},
        {"/pm/process/nkncache_meta_evictd/launch_uid", bt_uint16, "0"},
        {"/pm/process/nkncache_meta_evictd/launch_gid", bt_uint16, "0"},

        {"/pm/process/nkncache_meta_evictd/initial_launch_order", bt_int32, NKN_LAUNCH_ORDER_CACHE_META_EVICTD},
        {"/pm/process/nkncache_meta_evictd/initial_launch_timeout", bt_duration_ms, NKN_LAUNCH_TIMEOUT_CACHE_META_EVICTD},
 };

/*----------------------------------------------------------------------------
 * FUNCTION DECLARATIONS
 *---------------------------------------------------------------------------*/
int md_cache_meta_evictd_pm_init(const lc_dso_info *info, void *data);


static int md_nkn_pm_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
        int err = 0;

        err = mdb_create_node_bindings(
                        commit,
                        db,
                        md_cache_meta_evictd_pm_initial_values,
                        sizeof(md_cache_meta_evictd_pm_initial_values)/ sizeof(bn_str_value));
        bail_error(err);

bail:
        return err;
}

int
md_cache_meta_evictd_pm_init(const lc_dso_info *info, void *data)
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
                    "cache_meta_evictd",
                    1,
                    "/pm/nokeena/cache_meta_evictd",
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
                    md_nkn_pm_add_initial,
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

