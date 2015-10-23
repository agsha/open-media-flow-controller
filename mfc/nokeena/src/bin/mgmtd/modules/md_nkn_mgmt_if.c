/*
 *
 * Filename:    md_nkn_mgmt_if.c
 * Date:
 * Author:
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved/
 *
 */

/*----------------------------------------------------------------------------
 *md_nkn_mgmt_if.c
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

/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
int
md_nkn_mgmt_if_init(
        const lc_dso_info *info,
        void *data);

int
md_nkn_mgmt_if_check_format(
        md_commit *commit,
        const mdb_db *old_db,
        const mdb_db *new_db,
        const mdb_db_change_array *change_list,
        const tstring *node_name,
        const tstr_array *node_name_parts,
        mdb_db_change_type change_type,
        const bn_attrib_array *old_attribs,
        const bn_attrib_array *new_attribs,
        void *arg);

/*----------------------------------------------------------------------------
 * PRIVATE FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
static md_upgrade_rule_array *md_nkn_mgmt_if_ug_rules = NULL;

static int
md_nkn_mgmt_if_commit_apply(
        md_commit *commit,
        const mdb_db *old_db,
        const mdb_db *new_db,
        mdb_db_change_array *change_list,
        void *arg);

/*----------------------------------------------------------------------------
 * MODULE ENTRY POINT
 *---------------------------------------------------------------------------*/
int
md_nkn_mgmt_if_init(const lc_dso_info *info, void *data)
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
	    "mgmt-if",                           // module_name
	    3,                                  // module_version
	    "/nkn/mgmt-if",                   // root_node_name
	    NULL,                               // root_config_name
	    modrf_namespace_unrestricted,       // md_mod_flags
	    NULL,			         // side_effects_func
	    NULL,                               // side_effects_arg
	    NULL,		                 // check_func
	    NULL,                               // check_arg
	    md_nkn_mgmt_if_commit_apply,             // apply_func
	    NULL,                               // apply_arg
	    200,                                // commit_order
	    0,                                  // apply_order
	    md_generic_upgrade_downgrade,       // updown_func
	    &md_nkn_mgmt_if_ug_rules,           // updown_arg
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
     * Setup config node - /nkn/mgmt-if/config/interface
     *  - Configure eth0 on MAC address
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/mgmt-if/config/interface";
    node->mrn_value_type        = bt_string;
    node->mrn_initial_value     = "00:00:00:00:00:00";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "MAC address for eth0 interface";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * Setup config node - /nkn/mgmt-if/config/if-name
     *  - Configure an interface as management interface
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/mgmt-if/config/if-name";
    node->mrn_value_type        = bt_string;
    node->mrn_initial_value     = "eth0";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Management interface";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/mgmt-if/config/dmi-port";
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value     = "8022";
    node->mrn_limit_num_min	= "1";
    node->mrn_limit_num_max	= "65535";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "DMI SSH Tunnel Listening port";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = md_upgrade_rule_array_new(&md_nkn_mgmt_if_ug_rules);
    bail_error(err);
    ra = md_nkn_mgmt_if_ug_rules;

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type = MUTT_ADD;
    rule->mur_name = "/nkn/mgmt-if/config/if-name";
    rule->mur_new_value_type = bt_string;
    rule->mur_new_value_str = "eth0";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type = MUTT_ADD;
    rule->mur_name = "/nkn/mgmt-if/config/dmi-port";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str = "8022";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

bail:
    return err;
}


/*---------------------------------------------------------------------------*/
static int
md_nkn_mgmt_if_commit_apply(
        md_commit *commit,
        const mdb_db *old_db,
        const mdb_db *new_db,
        mdb_db_change_array *change_list,
        void *arg)
{
    int i,err = 0,num_changes = 0;
    int32 ret_status;
    tbool found=false;
    tstring* mac_addr=NULL;
    const mdb_db_change *change = NULL;

    num_changes = mdb_db_change_array_length_quick (change_list);

    for (i = 0; i < num_changes; i++)
    {
        change = mdb_db_change_array_get_quick (change_list, i);
	bail_null (change);

        if (bn_binding_name_parts_pattern_match_va (change->mdc_name_parts, 0, 4, "nkn", "mgmt-if", "config", "interface")){

            err = mdb_get_node_value_tstr(commit, new_db, ts_str(change->mdc_name), 0, &found, &mac_addr);
            bail_error(err);

            if(found == false)
               bail_error(err);

            if(mac_addr == NULL)
               bail_null(mac_addr);

	    /* Check to see if the mac_addr is the default 00:00:00:00:00:00 */
	    /* If default we need not call eth-reorder.sh */
	    if (!strcmp(ts_str(mac_addr), "00:00:00:00:00:00"))
	    	goto bail;

	    err = lc_launch_quick_status(&ret_status, NULL, true, 4, "/opt/nkn/bin/eth-reorder.sh", "--force", "--eth0",  ts_str(mac_addr));
            bail_error(err);
        }
   }
bail:
        return err;
}

/*----------------------------------------------------------------------------
 * END OF FILE
 *---------------------------------------------------------------------------*/

