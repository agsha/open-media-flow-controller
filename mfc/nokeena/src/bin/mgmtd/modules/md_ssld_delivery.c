/*
 *
 * Filename:  md_ssld_delivery.c
 * Date:      2011/05/23
 * Author:    Thilak Raj S
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

/*----------------------------------------------------------------------------
 * HEADER FILES
 *---------------------------------------------------------------------------*/
#include "common.h"
#include "dso.h"
#include "mdb_db.h"
#include "md_utils.h"
#include "md_mod_commit.h"
#include "md_mod_reg.h"
#include "md_upgrade.h"
#include "proc_utils.h"
#include "nkn_mgmt_defs.h"
#include "file_utils.h"
#include "ssld_mgmt.h"
/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
int md_ssld_delivery_init(const lc_dso_info *info, void *data);
/*----------------------------------------------------------------------------
 * LOCAL FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
static md_upgrade_rule_array *md_ssld_delivery_ug_rules = NULL;

static int
md_ssld_del_cfg_add_initial(md_commit *commit, mdb_db *db, void *arg);

/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION DEFINITIONS
 *---------------------------------------------------------------------------*/
const bn_str_value md_ssld_def_delivery_port[] = {
    {"/nkn/ssld/delivery/config/server_port/443", bt_uint16, "443"},
};

int
md_ssld_delivery_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    /*
     * Creating the Nokeena specific module
     */
    err = mdm_add_module("ssld_delivery", 2,
	    "/nkn/ssld/delivery", "/nkn/ssld/delivery/config",
	    0,
	    NULL, NULL,
	    NULL, NULL,
	    NULL, NULL,
	    0,
	    0,
	    md_generic_upgrade_downgrade,
	    &md_ssld_delivery_ug_rules,
	    md_ssld_del_cfg_add_initial,
	    NULL,
	    NULL,
	    NULL,
	    NULL,
	    NULL,
	    &module);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/ssld/delivery/config/server_port/*";
    node->mrn_value_type =         bt_uint16;
    //	node->mrn_limit_str_charlist =  "0123456789";
    node->mrn_limit_wc_count_max = NKN_SSLD_DELIVERY_MAX_PORTS;
    node->mrn_bad_value_msg =      "Error creating Node";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "";
    //	node->mrn_limit_num_max =       "65535";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/ssld/delivery/config/server_intf/*";
    node->mrn_value_type =         bt_string;
    //	node->mrn_limit_str_charlist =  "0123456789";
    node->mrn_limit_wc_count_max = NKN_SSLD_DELIVERY_MAX_INTF;
    node->mrn_bad_value_msg =      "Cannot set more than 64	ports";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "";
    //	node->mrn_limit_num_max =       "65535";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/ssld/delivery/config/conn-pool/origin/enabled";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "true";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Delivery Protocol Connection Pool Origin enabled";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/ssld/delivery/config/conn-pool/origin/timeout";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  90;
    node->mrn_limit_num_min_int =  1;
    node->mrn_limit_num_max_int =  86400;	
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Delivery protocol Connection Pool Origin Timeout";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/ssld/delivery/config/conn-pool/origin/max-conn";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  4096;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  128000;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Delivery protocol Connection Pool Origin - "
	"maximum number of connections to open "
	"concurrently to a single origin server";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = md_upgrade_rule_array_new(&md_ssld_delivery_ug_rules);
    bail_error(err);
    ra = md_ssld_delivery_ug_rules;

    /*! Added in module version 2
     */
    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type              =       MUTT_ADD;
    rule->mur_name                      =       "/nkn/ssld/delivery/config/conn-pool/origin/enabled";
    rule->mur_new_value_type            =       bt_bool;
    rule->mur_new_value_str             =       "true";
    rule->mur_cond_name_does_not_exist  =       true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type              =       MUTT_ADD;
    rule->mur_name                      =       "/nkn/ssld/delivery/config/conn-pool/origin/timeout";
    rule->mur_new_value_type            =       bt_uint32;
    rule->mur_new_value_str             =       "90";
    rule->mur_cond_name_does_not_exist  =       true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type              =       MUTT_ADD;
    rule->mur_name                      =       "/nkn/ssld/delivery/config/conn-pool/origin/max-conn";
    rule->mur_new_value_type            =       bt_uint32;
    rule->mur_new_value_str             =       "4096";
    rule->mur_cond_name_does_not_exist  =       true;
    MD_ADD_RULE(ra);
bail:
    return(err);
}

static int
md_ssld_del_cfg_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
    int err = 0;
    bn_binding *binding = NULL;

    err = mdb_create_node_bindings
	(commit, db, md_ssld_def_delivery_port,
	 sizeof(md_ssld_def_delivery_port) / sizeof(bn_str_value));
    bail_error(err);

bail:
    bn_binding_free(&binding);
    return(err);
}

