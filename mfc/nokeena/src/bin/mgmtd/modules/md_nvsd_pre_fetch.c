/*
 *
 * Filename:    md_nvsd_pre_fetch.c
 * Date:        16/09/2010
 * Author:      Ramalingam Chandran
 *
 * (C) Copyright 2010 Juniper Networks, Inc.
 * All rights reserved/
 *
 */

/*----------------------------------------------------------------------------
 *md_nkn_prefetch.c
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
#include "tpaths.h"
#include "md_upgrade.h"


/*----------------------------------------------------------------------------
 * PREPROCESSOR MACROS/CONSTANTS
 *---------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * PRIVATE VARIABLE DECLARATIONS
 *---------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
int
md_nvsd_pre_fetch_init(
        const lc_dso_info *info,
        void *data);


/*----------------------------------------------------------------------------
 * MODULE ENTRY POINT
 *---------------------------------------------------------------------------*/
int
md_nvsd_pre_fetch_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    err = mdm_add_module(
	    "mgmt-prefetch",                           // module_name
	    2,                                  // module_version
	    "/nkn/prefetch",                   // root_node_name
	    "/nkn/prefetch/config",            // root_config_name
	    0,                  // md_mod_flags
	    NULL,                               // side_effects_func
	    NULL,                               // side_effects_arg
	    NULL,      // check_func
	    NULL,                               // check_arg
	    NULL,      // apply_func
	    NULL,                               // apply_arg
	    0,                                 // commit_order
	    0,                                  // apply_order
	    md_generic_upgrade_downgrade,       // updown_func
	    NULL,                               // updown_arg
	    NULL,                               // initial_func
	    NULL,                               // initial_arg
	    NULL,                               // mon_get_func
	    NULL,                               // mon_get_arg
	    NULL,                               // mon_iterate_func
	    NULL,				// mon_iterate_arg
	    &module);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/prefetch/config/*";
    node->mrn_value_type =         bt_string;
    node->mrn_limit_str_max_chars = 16;
    node->mrn_initial_value =      "";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_limit_str_no_charlist = "/\\:*|`\"?";
    node->mrn_description       = "Prefetch Job name";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/prefetch/config/*/time";
    node->mrn_initial_value_int =   0;
    node->mrn_value_type        = bt_time_sec;
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_description       = "Prefetch Job time";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/prefetch/config/*/date";
    node->mrn_initial_value_int =   0;
    node->mrn_value_type        = bt_date;
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_description       = "Prefetch Job time";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*The status node will take the following state as status - immediate, scheduled, completed*/
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/prefetch/config/*/status";
    node->mrn_value_type        = bt_string;
    node->mrn_initial_value     = "";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_description       = "Prefetch Job status";
    err = mdm_add_node(module, &node);
    bail_error(err);

bail:
    return err;
}

/*----------------------------------------------------------------------------
 * END OF FILE
 *---------------------------------------------------------------------------*/

