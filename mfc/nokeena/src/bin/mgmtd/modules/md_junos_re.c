/*
 *
 * Filename:  md_junos_re.c
 * Date:      12/22/2014
 * Author:    Jeya ganesh babu J
 *
 * (C) Copyright 2014 Juniper Networks, Inc.
 * All rights reserved.
 *
 *
 */

#include "common.h"
#include "dso.h"
#include "mdb_db.h"
#include "md_utils.h"
#include "md_mod_commit.h"
#include "md_mod_reg.h"
#include "md_upgrade.h"
#include "proc_utils.h"



/* ------------------------------------------------------------------------- */

int md_nvsd_mm_init(const lc_dso_info *info, void *data);

static md_upgrade_rule_array *md_junos_re_ug_rules = NULL;

/* ------------------------------------------------------------------------- */
int
md_junos_re_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    /*
     * Creating the Nokeena specific module
     */
    err = mdm_add_module("junos-re", 1,
                         "/nkn/junos-re", "/nkn/junos-re/auth",
			 0,
                         NULL, NULL,
                         NULL, NULL,
                         NULL, NULL,
                         0, 0,
                         md_generic_upgrade_downgrade,
			 &md_junos_re_ug_rules,
                         NULL, NULL,
                         NULL, NULL, NULL, NULL,
			 &module);
    bail_error(err);

    /*
     * Junos Re authentication - /nkn/junos-re/auth/username
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/junos-re/auth/username";
    node->mrn_value_type        = bt_string;
    node->mrn_initial_value     = "root";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "junos-re username";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * Junos Re authentication - /nkn/junos-re/auth/password
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/junos-re/auth/password";
    node->mrn_value_type        = bt_string;
    node->mrn_initial_value     = "Juniper12";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "junos-re password";
    err = mdm_add_node(module, &node);
    bail_error(err);

bail:
    return(err);
}

