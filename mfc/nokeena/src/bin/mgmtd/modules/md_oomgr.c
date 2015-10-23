/*
 *
 * Filename:  md_oomgr.c
 * Date:      2008/11/14
 * Author:    Ramanand Narayanan
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

int md_oomgr_init(const lc_dso_info *info, void *data);

static md_upgrade_rule_array *md_oomgr_ug_rules = NULL;

/* ------------------------------------------------------------------------- */
int
md_oomgr_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    /*
     * Creating the Nokeena specific module
     */
    err = mdm_add_module("oomgr", 1,
                         "/nkn/oomgr", "/nkn/oomgr/config",
			 0,
                         NULL, NULL,
                         NULL, NULL,
                         NULL, NULL,
                         0, 0,
                         md_generic_upgrade_downgrade,
			 &md_oomgr_ug_rules,
                         NULL, NULL,
                         NULL, NULL, NULL, NULL,
			 &module);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/oomgr/config/no_daemon";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "true";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Offline Origin Manager - True if not a daemon";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/oomgr/config/input_queue_filename";
    node->mrn_value_type =         bt_string;
    node->mrn_limit_str_max_chars = 256;
    node->mrn_initial_value =      "/tmp/OOMGR-input.queue";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Offline Origin Manager - Input queue filename";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/oomgr/config/output_queue_filename";
    node->mrn_value_type =         bt_string;
    node->mrn_limit_str_max_chars = 256;
    node->mrn_initial_value =      "/tmp/OOMGR-output.queue";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Offline Origin Manager - Output Queue Filename";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/oomgr/config/cleanup_last_data";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "true";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Offline Origin Manager - Cleanup Last Data";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/oomgr/config/input_queue_maxsize";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  100;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  200;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Offline Origin Manager - Input Queue Maxsize";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/oomgr/config/output_queue_retries";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  2;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  10;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Offline Origin Manager - Output Queue Retries";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/oomgr/config/output_queue_retry_delay";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  10;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  20;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Offline Origin Manager - Output Queue Retry Delay";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/oomgr/config/verbose_mode";
    node->mrn_value_type =         bt_bool;
    node->mrn_initial_value =      "false";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Offline Origin Manager - Output Queue Retries";
    err = mdm_add_node(module, &node);
    bail_error(err);


 bail:
    return(err);
}

