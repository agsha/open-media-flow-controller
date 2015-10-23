/*
 *
 * Filename:  md_nvsd_am.c
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

int md_nvsd_am_init(const lc_dso_info *info, void *data);

static md_upgrade_rule_array *md_nvsd_am_ug_rules = NULL;


md_commit_forward_action_args md_am_fwd_args =
{
    GCL_CLIENT_NVSD, true, N_("Request failed; MFD subsystem not running"), mfat_blocking
#ifdef PROD_FEATURE_I18N_SUPPORT
        , GETTEXT_DOMAIN
#endif
};

/* ------------------------------------------------------------------------- */
int
md_nvsd_am_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    /*
     * Creating the Nokeena specific module
     */
    err = mdm_add_module("nvsd-am", 6,
                         "/nkn/nvsd/am", "/nkn/nvsd/am/config",
			 0,
                         NULL, NULL,
                         NULL, NULL,
                         NULL, NULL,
                         0, 0,
                         md_generic_upgrade_downgrade,
			 &md_nvsd_am_ug_rules,
                         NULL, NULL,
                         NULL, NULL, NULL, NULL,
			 &module);
    bail_error(err);

    /*
     * Setup config node - /nkn/nvsd/am/config/cache_promotion_enabled
     *  - Configure the boolean to enable/disable cache_promotion
     *  - Default value is "false"
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/am/config/cache_promotion_enabled";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "false";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Analytics Manager : Flag to enable/disable cache promotion";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * Setup config node - /nkn/nvsd/am/config/cache_promotion_enabled
     *  - Configure the boolean to enable/disable cache_promotion
     *  - Default value is "false"
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/am/config/cache_promotion/size_threshold";
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value     = "0";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Analytics Manager : Maximum size of the object that can be promoted";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * Setup config node - /nkn/nvsd/am/config/cache_ingest_hits_threshold
     *  - Configure the threshold on the number of hits for cache ingest
     *  - Default value is 10 hits
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/am/config/cache_ingest_hits_threshold";
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value     = "3";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Analytics Manager : Number of hits after which cache ingest should happen";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * Setup config node - /nkn/nvsd/am/config/cache_ingest_size_threshold
     *  - Configure the threshold on the size of file for cache ingest
     *  - Default value is 0 meaning size threshold disabled
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/am/config/cache_ingest_size_threshold";
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value     = "0";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Analytics Manager : Size in bytes below which cache ingest should happen";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * Setup config node - /nkn/nvsd/am/config/cache_ingest_last_eviction_time_diff
     *  - Configure the difference in time between the last eviction and when the video
     *  - should be ingested into cache. This can be set in seconds.
     *  - Default value is 60 seconds
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/am/config/cache_ingest_last_eviction_time_diff";
    node->mrn_value_type        = bt_duration_sec;
    node->mrn_initial_value     = "1";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Analytics Manager : Time difference between last eviction and when the vieo should be ingested into the cache. (Seconds)";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * Setup config node - /nkn/nvsd/am/config/cache_evict_aging_time
     *  - Configure the time interval after which aging happens on the videos in cache.
     *  - Default value is 3600 seconds (1 hour)
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/am/config/cache_evict_aging_time";
    node->mrn_value_type        = bt_duration_sec;
    node->mrn_initial_value     = "10";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Analytics Manager : Time interval after which aging happens on the videos in cache (Seconds)";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/am/config/cache_promotion/hotness_increment";
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value_int =  100;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  100000;
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Analytics Manager : Hotness increment";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/am/config/cache_promotion/hotness_threshold";
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value_int =  3;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  100000;
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Analytics Manager : Hotness threshold";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/am/config/cache_promotion/hit_increment";
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value_int =  100;
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Analytics Manager : Hit increment";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/am/config/cache_promotion/hit_threshold";
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value_int =  10;
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Analytics Manager : Hit threshold";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/am/config/cache_ingest_queue_depth";
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value_int = 200000;
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Analytics Manager : Ingest Queue Depth";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/am/config/cache_ingest_object_timeout";
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value_int = 0;
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Analytics Manager : Ingest object timeout in Secs";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/am/config/cache_ingest_policy";
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value_int = 3;
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Analytics Manager : Ingest policy - auto/namespace-based/dynamic";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/am/config/memory_limit";
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value     = "0";
    node->mrn_limit_num_min_int = 0;
    node->mrn_limit_num_max_int = 2048;
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Analytics Manager : memory limit";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/am/monitor/cal_am_mem_limit";
    node->mrn_value_type 	= bt_uint32;
    node->mrn_node_reg_flags    = mrf_flags_reg_monitor_ext_literal;
    node->mrn_cap_mask          = mcf_cap_node_basic;
    node->mrn_extmon_provider_name = gcl_client_nvsd;
    node->mrn_description 	= "Calculated AM memory limit";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/am/config/cache_ingest_ignore_hotness_check";
    node->mrn_value_type 	= bt_uint32;
    node->mrn_initial_value     = "0";
    node->mrn_limit_num_min_int = 0;
    node->mrn_limit_num_max_int = 1;
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description 	= "Ignore hotness check during ingest";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * Action nodes for the analytics manager
     */
    err = mdm_new_action(module, &node, 1);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/am/actions/rank";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_func       = md_commit_forward_action;
    node->mrn_action_arg        = &md_am_fwd_args;
    node->mrn_actions[0]->mra_param_name = "rank";
    node->mrn_actions[0]->mra_param_type = bt_int32;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 0);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/am/actions/tier";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_func       = md_commit_forward_action;
    node->mrn_action_arg        = &md_am_fwd_args;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_action(module, &node, 1);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/am/actions/longtail";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_func       = md_commit_forward_action;
    node->mrn_action_arg        = &md_am_fwd_args;
    node->mrn_actions[0]->mra_param_name = "longtail";
    node->mrn_actions[0]->mra_param_type = bt_int32;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /* Upgrade processing nodes */

    err = md_upgrade_rule_array_new(&md_nvsd_am_ug_rules);
    bail_error(err);
    ra = md_nvsd_am_ug_rules;

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type = MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/am/config/cache_ingest_size_threshold";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str = "0";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type = MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/am/config/cache_ingest_queue_depth";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str = "200000";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type = MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/am/config/cache_ingest_object_timeout";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str = "0";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type = MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/am/config/cache_ingest_policy";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str = "3";
    /* Only add if the node does not exist */
    rule->mur_cond_name_does_not_exist = true;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type = MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/am/config/memory_limit";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str = "0";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type = MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/am/config/cache_promotion/size_threshold";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str = "0";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 5, 6);
    rule->mur_upgrade_type = MUTT_ADD;
    rule->mur_name = "/nkn/nvsd/am/config/cache_ingest_ignore_hotness_check";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str = "0";
    MD_ADD_RULE(ra);

 bail:
    return(err);
}

