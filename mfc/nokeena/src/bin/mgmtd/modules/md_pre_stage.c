/*
 * Filename : md_pre_stage.c
 * Created :  2008/12/29
 * Author :   Dhruva Narasimhan
 *
 * (C) Copyright 2008, Nokeena Networks
 * All rights reserved.
 */


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



int
md_pre_stage_init(
        const lc_dso_info *info,
        void *data);



/*----------------------------------------------------------------------------
 * PRIVATE FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
int
md_pre_stage_init(
        const lc_dso_info *info,
        void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;

    err = mdm_add_module(
            "pre_stage",
            1,
            "/nkn/nvsd/pre_stage",
            NULL,
            modrf_namespace_unrestricted,
            NULL, NULL,
            NULL, NULL,
            NULL, NULL,
            200,
            0,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            &module);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/pre_stage/ftpd/default/url";
    node->mrn_value_type        = bt_uri;
    node->mrn_initial_value     = "/";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Default URI to use when pre-staging a file";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/pre_stage/ftpd/default/attribute/*";
    node->mrn_value_type        = bt_string;
    node->mrn_node_reg_flags    = mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Default Attrbute name";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/pre_stage/ftpd/default/attribute/*/value";
    node->mrn_value_type        = bt_string;
    node->mrn_initial_value     = "";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Default Attribute value to use when pre-staging a file";
    err = mdm_add_node(module, &node);
    bail_error(err);


bail:
    return err;
}
