/*
 *
 * Filename:  md_nvsd_tfm.c
 * Date:      2009/05/01
 * Author:    Dhruva Narasimhan
 *
 * (C) Copyright 2008,2009 Nokeena Networks, Inc.
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

int md_nvsd_tfm_init(const lc_dso_info *info, void *data);

static md_upgrade_rule_array *md_nvsd_tfm_ug_rules = NULL;

int md_nvsd_tfm_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    /*
     * Creating the Nokeena specific module
     */
    err = mdm_add_module("nvsd-tfm", 2,
	    "/nkn/nvsd/tfm", "/nkn/nvsd/tfm/config",
	    0,
	    NULL, NULL,
	    NULL, NULL,
	    NULL, NULL,
	    0, 0,
	    md_generic_upgrade_downgrade,
	    &md_nvsd_tfm_ug_rules,
	    NULL, NULL,
	    NULL, NULL, NULL, NULL,
	    &module);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/tfm/config/max_num_files";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value_int =   25;
    node->mrn_limit_num_min_int =   1;
    node->mrn_limit_num_max_int =   1000;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Number of files that can be stored in TFM";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/tfm/config/partial_file/evict_time";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value_int =   300;
    node->mrn_limit_num_min_int =   1;
    node->mrn_limit_num_max_int =   3600;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Time in seconds for eviction of partial files";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/tfm/config/max_file_size";
    node->mrn_value_type =          bt_int32;
    node->mrn_initial_value_int =   10000000;
    node->mrn_limit_num_min_int =   1;
    node->mrn_limit_num_max_int =   250000000;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Max file size";
    err = mdm_add_node(module, &node);
    bail_error(err);


    /* Upgrade/Downgrade rules */
    err = md_upgrade_rule_array_new(&md_nvsd_tfm_ug_rules);
    bail_error(err);
    ra = md_nvsd_tfm_ug_rules;

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type = 		MUTT_ADD;
    rule->mur_name = 			"/nkn/nvsd/tfm/config/max_file_size";
    rule->mur_new_value_type = 		bt_int32;
    rule->mur_new_value_str = 		"10000000";
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =		MUTT_CLAMP;
    rule->mur_name = 			"/nkn/nvsd/tfm/config/max_num_files";
    rule->mur_lowerbound = 		1;
    rule->mur_upperbound = 		1000;
    MD_ADD_RULE(ra);


bail:
    return err;
}


