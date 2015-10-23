/*
 * Filename:    md_mfdlog.c
 * Date:        2009/01/09
 * Author:      Dhruva Narasimhan
 *
 * (C) Copyright 2008-2009 Nokeena Networks, Inc.
 * All rights reserved.
 */


#include "common.h"
#include "dso.h"
#include "mdb_db.h"
#include "md_utils.h"
#include "md_mod_commit.h"
#include "md_mod_reg.h"
#include "md_upgrade.h"


int
md_mfdlog_init(
        const lc_dso_info *info,
        void *data);

static md_upgrade_rule_array *md_mfdlog_ug_rules = NULL;

int
md_mfdlog_init(
        const lc_dso_info *info,
        void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    err = mdm_add_module("mfdlog", 1,
            "/nkn/mfdlog", "/nkn/mfdlog/config",
            0,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            0,
            0,
            md_generic_upgrade_downgrade,
            &md_mfdlog_ug_rules,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            &module);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/mfdlog/config/interface";
    node->mrn_value_type =          bt_string;
    node->mrn_initial_value =       "lo";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Interface name";
    err = mdm_add_node(module, &node);
    bail_error(err);


    /* This node is never used since the CLI only supports TCP. This is
     * merely a placeholder incase we support many more protocols!
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/mfdlog/config/proto";
    node->mrn_value_type =          bt_string;
    node->mrn_initial_value =       "tcp";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Protocol";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/mfdlog/config/port";
    node->mrn_value_type =          bt_uint16;
    node->mrn_initial_value =       "7957";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Port to read on";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/mfdlog/config/field-length";
    node->mrn_value_type =          bt_uint16;
    node->mrn_initial_value =       "256";
    node->mrn_limit_num_max_int =   256;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Maximum field length";
    err = mdm_add_node(module, &node);
    bail_error(err);


bail:
    return err;
}


