/*
 *
 * Filename:    md_geodbd.c
 * Date:        2011-10-30
 * Author:      Varsha Rajagopalan
 *
 * (C) Copyright 2011 Juniper Networks, Inc.
 * All rights reserved/
 *
 */

/*----------------------------------------------------------------------------
 * md_geodbd.c: defines the action nodes for installing the geodbd service.
 *
 *---------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
 * HEADER FILES
 *---------------------------------------------------------------------------*/
#include "common.h"
#include "dso.h"
#include "file_utils.h"
#include "md_utils.h"
#include "md_mod_reg.h"
#include "md_mod_commit.h"
#include "mdb_db.h"
#include "mdb_dbbe.h"
#include "array.h"
#include "tpaths.h"
#include "proc_utils.h"
#include "url_utils.h"

/*----------------------------------------------------------------------------
 * PREPROCESSOR MACROS/CONSTANTS
 *---------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * PRIVATE VARIABLE DECLARATIONS
 *---------------------------------------------------------------------------*/
md_commit_forward_action_args md_geodbd_fwd_args =
{
    GCL_CLIENT_GEODBD, true, N_("Request failed; MFD subsystem 'geodbd' did not respond"),
    mfat_nonbarrier_async
#ifdef PROD_FEATURE_I18N_SUPPORT
    , GETTEXT_DOMAIN
#endif
};
static md_upgrade_rule_array *md_geodbd_ug_rules = NULL;

/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
int
md_geodbd_init(
        const lc_dso_info *info,
        void *data);

/*----------------------------------------------------------------------------
 * PRIVATE FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------
 * MODULE ENTRY POINT
 *---------------------------------------------------------------------------*/
int
md_geodbd_init(const lc_dso_info *info, void *data)
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
	    "geodbd",                           // module_name
	    1,                                  // module_version
	    "/nkn/geodb",                   // root_node_name
	    NULL,                               // root_config_name
	    modrf_namespace_unrestricted,       // md_mod_flags
	    NULL,			       // side_effects_func
	    NULL,                               // side_effects_arg
	    NULL,		                // check_func
	    NULL,                               // check_arg
	    NULL,		                 // apply_func
	    NULL,                               // apply_arg
	    200,                                // commit_order
	    0,                                  // apply_order
	    md_generic_upgrade_downgrade,       // updown_func
	    &md_geodbd_ug_rules,		// updown_arg
	    NULL,                               // initial_func
	    NULL,                               // initial_arg
	    NULL,                               // mon_get_func
	    NULL,                               // mon_get_arg
	    NULL,                               // mon_iterate_func
	    NULL,                               // mon_iterate_arg
	    &module);                           // ret_module
    bail_error(err);

#ifdef PROD_FEATURE_I18N_SUPPORT
    err = mdm_module_set_gettext_domain(module, GETTEXT_DOMAIN);
    bail_error(err);
#endif /* PROD_FEATURE_I18N_SUPPORT */
    /*
     *   Action nodes
     */
    err = mdm_new_action(module, &node, 1);
    bail_error(err);
    node->mrn_name              = "/nkn/geodb/actions/install";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_async_nonbarrier_start_func        = md_commit_forward_action;
    node->mrn_action_arg        = &md_geodbd_fwd_args;
    node->mrn_actions[0]->mra_param_name = "filename";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_actions[0]->mra_param_name = "destination";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 1);
    bail_error(err);
    node->mrn_name              = "/nkn/geodb/actions/info_display";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_async_nonbarrier_start_func        = md_commit_forward_action;
    node->mrn_action_arg        = &md_geodbd_fwd_args;
    node->mrn_actions[0]->mra_param_name = "app_name";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*Define an event for raising after installation*/
    err = mdm_new_event(module, &node, 0);
    bail_error(err);
    node->mrn_name = "/nkn/geodb/events/installation";
    node->mrn_node_reg_flags = mrf_flags_reg_event;
    node->mrn_cap_mask = mcf_cap_event_privileged;
    node->mrn_description = "Event to notify on geodb installation";
    err = mdm_add_node(module, &node);
    bail_error(err);

bail:
    return err;
}
/*----------------------------------------------------------------------------
 * END OF FILE
 *---------------------------------------------------------------------------*/
