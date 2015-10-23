/*
 *
 * Filename:  md_nvsd_mm.c
 * Date:      2008/12/18
 * Author:    Dhruva Narasimhan
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
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

static md_upgrade_rule_array *md_nvsd_mm_ug_rules = NULL;

/* ------------------------------------------------------------------------- */
int
md_nvsd_mm_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    /*
     * Creating the Nokeena specific module
     */
    err = mdm_add_module("nvsd-mm", 1,
                         "/nkn/nvsd/mm", "/nkn/nvsd/mm/config",
			 0,
                         NULL, NULL,
                         NULL, NULL,
                         NULL, NULL,
                         0, 0,
                         md_generic_upgrade_downgrade,
			 &md_nvsd_mm_ug_rules,
                         NULL, NULL,
                         NULL, NULL, NULL, NULL,
			 &module);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/mm/config/provider/threshold/high";
    node->mrn_value_type        = bt_uint32;
    node->mrn_limit_num_min_int = 0;
    node->mrn_limit_num_max_int = 100;
    node->mrn_initial_value_int = 95;
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Media Manager - Provider Threshold High";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/mm/config/provider/threshold/low";
    node->mrn_value_type        = bt_uint32;
    node->mrn_limit_num_min_int = 0;
    node->mrn_limit_num_max_int = 100;
    node->mrn_initial_value_int = 90;
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Media Manager - Provider Threshold Low";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/mm/config/evict_thread_time";
    node->mrn_value_type        = bt_uint32;
    node->mrn_limit_num_min_int = 60;
    node->mrn_limit_num_max_int = 86400;
    node->mrn_initial_value_int = 3600;
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Media Manager - Eviction thread time";
    err = mdm_add_node(module, &node);
    bail_error(err);

bail:
    return(err);
}

