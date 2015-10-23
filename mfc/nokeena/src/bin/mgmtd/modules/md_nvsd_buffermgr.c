/*
 *
 * Filename:  md_nvsd_buffermgr.c
 * Date:      2008/11/19
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

int md_nvsd_buffermgr_init(const lc_dso_info *info, void *data);

static md_upgrade_rule_array *md_nvsd_buffermgr_ug_rules = NULL;

static int md_nvsd_buffermgr_commit_check(md_commit *commit,
                                const mdb_db *old_db, const mdb_db *new_db,
                                mdb_db_change_array *change_list, void *arg);

md_commit_forward_action_args md_buffermgr_fwd_args =
{
    GCL_CLIENT_NVSD, true, N_("Request failed; MFD subsystem not running"), mfat_blocking
#ifdef PROD_FEATURE_I18N_SUPPORT
        , GETTEXT_DOMAIN
#endif
};

/* ------------------------------------------------------------------------- */
int
md_nvsd_buffermgr_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    /*
     * Creating the Nokeena specific module
     */
    err = mdm_add_module("nvsd-buffermgr", 14,
                         "/nkn/nvsd/buffermgr", "/nkn/nvsd/buffermgr/config",
			 0,
                         NULL, NULL,
                         md_nvsd_buffermgr_commit_check, NULL,
                         NULL, NULL,
                         0, 0,
                         md_generic_upgrade_downgrade,
			 &md_nvsd_buffermgr_ug_rules,
                         NULL, NULL,
                         NULL, NULL, NULL, NULL,
			 &module);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/buffermgr/config/max_cachesize";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  0;
    node->mrn_limit_num_min_int =  0;    /* Set the max possible */
    node->mrn_limit_num_max_int =  524288; /* 512 GB  Bug# 10485*/
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Max Buffer Cache Size";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/buffermgr/config/pageratio";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  2;
    node->mrn_limit_num_min_int =  0;    /* Set the max possible */
    node->mrn_limit_num_max_int =  8; /* 4k max value 256 */
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Buffer PageRatio";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/buffermgr/config/attr_sync_interval";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  14400; // This seems to be default configured in  lib/nvsd/bufmgr/cache_request.c
    node->mrn_limit_num_min_int =  1;    /* Set the min possible */
    node->mrn_limit_num_max_int =  UINT32_MAX; /* 32 GB */
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Max Buffer Cache Size";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/buffermgr/config/revalidate_window";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  120;
    node->mrn_limit_num_min_int =  60;
    node->mrn_limit_num_max_int =  UINT32_MAX;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Max Buffer Cache Size";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/buffermgr/config/revalidate_minsize";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  1;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  UINT32_MAX;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Revalidate window minimum size in KB";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/buffermgr/config/max_dictsize";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  0;
    node->mrn_limit_num_min_int =  0;    /* Set the max possible */
    node->mrn_limit_num_max_int =  32768; /* 32 GB */
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Max Cache Dictionary Size";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/buffermgr/config/threadpool_stack_size";
    node->mrn_value_type =         bt_uint64;
    node->mrn_initial_value_int =  262144;
    node->mrn_limit_num_min_int =  0;    /* Use default value */
    node->mrn_limit_num_max_int =  20971520; /* 20 MB */
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Stack Size to be used by threadpool";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/buffermgr/config/lru-scale";
    node->mrn_value_type =         bt_uint16;
    node->mrn_initial_value_int =  3;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  100;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "scale factor to increase the hotness of Least Recently Used Buffer";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/buffermgr/config/attributesize";
    node->mrn_value_type =          bt_uint16;
    node->mrn_initial_value_int     =   1024;
    node->mrn_limit_str_choices_str     = ",1024,1536,2048,2560,3072";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_bad_value_msg =       "error: supported attribute sizes are 1024, 1536, 2048, 2560, 3072";
    node->mrn_description =         "Use 1024/1536/2048/2560/3072 for attribute size";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/buffermgr/config/attributecount";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value_int =   0;
    node->mrn_limit_num_min_int =   0;
    node->mrn_limit_num_max_int =   64000000;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "configure ram cache attribute count";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * Monitor nodes
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/buffermgr/monitor/cachesize";
    node->mrn_value_type 	= bt_uint32;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_nvsd;
    node->mrn_description 	= "Allocated Cachesize in MBytes";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/buffermgr/monitor/max_cachesize_calculated";
    node->mrn_value_type 	= bt_uint32;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_nvsd;
    node->mrn_description 	= "Caculated Cachesize in MBytes";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/buffermgr/monitor/cal_max_dict_size";
    node->mrn_value_type 	= bt_uint32;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_nvsd;
    node->mrn_description 	= "Calculated max dictionary size in MBytes";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/buffermgr/monitor/attributecount";
    node->mrn_value_type 	= bt_uint32;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_nvsd;
    node->mrn_description 	= "ram-cache attribute count";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * Action nodes
     */

    err = mdm_new_action(module, &node, 0);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/buffermgr/actions/object-list";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_func       = md_commit_forward_action;
    node->mrn_action_arg        = &md_buffermgr_fwd_args;
    node->mrn_description = "List the object for debug purpose";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/buffermgr/config/unsuccessful_response/cachesize";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  0;
    node->mrn_limit_num_min_int =  0;    /* Set the min possiblee */
    node->mrn_limit_num_max_int =  20; /* max possible*/
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Max Unsuccessful response caching Buffer Cache Size percentage";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* Upgrade processing nodes */

    err = md_upgrade_rule_array_new(&md_nvsd_buffermgr_ug_rules);
    bail_error(err);
    ra = md_nvsd_buffermgr_ug_rules;

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type = MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/buffermgr/config/revalidate_window";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str = "600";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type = MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/buffermgr/config/revalidate_minsize";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str = "0";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type = MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/buffermgr/config/max_dictsize";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str = "0";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type = MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/buffermgr/config/threadpool_stack_size";
    rule->mur_new_value_type = bt_uint64;
    rule->mur_new_value_str = "262144";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 5, 6);
    rule->mur_upgrade_type = MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/buffermgr/config/pageratio";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str = "0";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =             "/nkn/nvsd/buffermgr/config/pageratio";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "2";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 7, 8);
    rule->mur_upgrade_type =    MUTT_MODIFY;
    rule->mur_name =             "/nkn/nvsd/buffermgr/config/revalidate_minsize";
    rule->mur_new_value_type =  bt_uint32;
    rule->mur_new_value_str =   "1";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 8, 9);
    rule->mur_upgrade_type = MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/buffermgr/config/lru-scale";
    rule->mur_new_value_type = bt_uint16;
    rule->mur_new_value_str = "3";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 9, 10);
    rule->mur_upgrade_type		=   MUTT_ADD;
    rule->mur_name			=   "/nkn/nvsd/buffermgr/config/attributesize";
    rule->mur_new_value_type		=   bt_uint16;
    rule->mur_new_value_str		=   "1024";
    rule->mur_cond_name_does_not_exist	=   true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 9, 10);
    rule->mur_upgrade_type		=   MUTT_ADD;
    rule->mur_name			=   "/nkn/nvsd/buffermgr/config/attributecount";
    rule->mur_new_value_type		=   bt_uint32;
    rule->mur_new_value_str		=   "0";
    rule->mur_cond_name_does_not_exist	=   true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 10, 11);
    rule->mur_upgrade_type		=    MUTT_MODIFY;
    rule->mur_name			=    "/nkn/nvsd/buffermgr/config/attributesize";
    rule->mur_new_value_type		=    bt_uint16;
    rule->mur_new_value_str		=   "1536";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 12, 13);
    rule->mur_upgrade_type =    MUTT_CLAMP;
    rule->mur_name =            "/nkn/nvsd/buffermgr/config/max_cachesize";
    rule->mur_lowerbound =  	0;
    rule->mur_upperbound =   	524288;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 13, 14);
    rule->mur_upgrade_type		=    MUTT_ADD;
    rule->mur_name			=    "/nkn/nvsd/buffermgr/config/unsuccessful_response/cachesize";
    rule->mur_new_value_type		=   bt_uint32;
    rule->mur_new_value_str		=   "0";
    rule->mur_cond_name_does_not_exist	=   true;
    MD_ADD_RULE(ra);
 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
md_nvsd_buffermgr_commit_check(md_commit *commit,
                     const mdb_db *old_db,
                     const mdb_db *new_db,
                     mdb_db_change_array *change_list, void *arg)
{
    int err = 0;
    int i, num_changes = 0;
    const mdb_db_change *change = NULL;

    num_changes = mdb_db_change_array_length_quick (change_list);
    for (i = 0; i < num_changes; i++)
    {
	change = mdb_db_change_array_get_quick (change_list, i);
	bail_null (change);

	if ( (bn_binding_name_parts_pattern_match_va (change->mdc_name_parts, 0, 5, "nkn", "nvsd", "buffermgr", "config", "max_cachesize")))
	{
	    uint32 inp_ram_cache_size = 0;
	    uint32 actual_ram_cache_size = 0;
	    bn_binding *binding = NULL;
	    uint32 code = 0;
	    const bn_attrib *new_val = NULL;


	    new_val = bn_attrib_array_get_quick(change->mdc_new_attribs, ba_value);
	    if (new_val) {
		err = bn_attrib_get_uint32(new_val, NULL, NULL, &inp_ram_cache_size);
		bail_error(err);
	    }

	    err = mdb_get_node_value_uint32(commit, old_db,
		    "/nkn/nvsd/buffermgr/monitor/max_cachesize_calculated", 0,
		    NULL, &actual_ram_cache_size);

	    if ((actual_ram_cache_size != 0) && (inp_ram_cache_size > actual_ram_cache_size)) {
		err = md_commit_set_return_status_msg_fmt(commit, 1, _("error: The value is greater than the actual RAM cache size"));
		bail_error(err);
	    }
	}

    }

    err = md_commit_set_return_status(commit, 0);
    bail_error(err);

bail:
    return(err);
}

