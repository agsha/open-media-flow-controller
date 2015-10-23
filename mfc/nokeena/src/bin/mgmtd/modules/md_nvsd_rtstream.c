/*
 *
 * Filename:  md_nvsd_rtstream.c
 * Date:      2009/07/29
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

int md_nvsd_rtstream_init(const lc_dso_info *info, void *data);

static md_upgrade_rule_array *md_nvsd_rtstream_ug_rules = NULL;

static int md_nvsd_rtstream_add_initial(md_commit *commit, mdb_db *db, void *arg);

/* ------------------------------------------------------------------------- */
int
md_nvsd_rtstream_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    /*
     * Creating the Nokeena specific module
     */
    err = mdm_add_module("nvsd-rtstream", 7,
                         "/nkn/nvsd/rtstream", "/nkn/nvsd/rtstream/config",
			 0,
                         NULL, NULL,
                         NULL, NULL,
                         NULL, NULL,
                         0, 0,
                         md_generic_upgrade_downgrade,
			 &md_nvsd_rtstream_ug_rules,
                         md_nvsd_rtstream_add_initial, NULL,
                         NULL, NULL, NULL, NULL,
			 &module);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/rtstream/config/vod/server_port/*";
    node->mrn_value_type =         bt_uint16;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_limit_wc_count_max = 64;
    node->mrn_limit_wc_count_min = 0;
    node->mrn_bad_value_msg =      N_("error: Cannot set more than 64 ports");
    node->mrn_description =        "RTSP VOD server's listening port";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/rtstream/config/vod/server_port/*/port";
    node->mrn_value_type =         bt_uint16;
    node->mrn_initial_value_int =  0;
    node->mrn_limit_num_min =      "0";
    node->mrn_limit_num_max =      "65535";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "RTSP VOD server's listening port";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/rtstream/config/live/server_port/*";
    node->mrn_value_type =         bt_uint16;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_limit_wc_count_max = 64;
    node->mrn_limit_wc_count_min = 0;
    node->mrn_bad_value_msg =      N_("error: Cannot set more than 64 ports");
    node->mrn_description =        "RTSP LIVE server's listening port";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/rtstream/config/live/server_port/*/port";
    node->mrn_value_type =         bt_uint16;
    node->mrn_initial_value_int =  0;
    node->mrn_limit_num_min =      "0";
    node->mrn_limit_num_max =      "65535";
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "RTSP LIVE server's listening port";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/rtstream/config/rate_control";
    node->mrn_value_type =         bt_uint64;
    node->mrn_initial_value_int =  30000;
    node->mrn_limit_num_min_int =  1000;
    node->mrn_limit_num_max_int =  50000;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "rtstream rate control to avoid rtstream attack";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/rtstream/config/om_idle_timeout";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  60;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  3000;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "rtstream origin side idle timeout";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/rtstream/config/player_idle_timeout";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  60;
    node->mrn_limit_num_min_int =  10;
    node->mrn_limit_num_max_int =  300;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "rtstream player side idle timeout";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/rtstream/config/interface/*";
    node->mrn_value_type =          bt_string;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/rtstream/config/trace/enabled";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Delivery Protocol Trace Enabled";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/rtstream/config/media-encode/*";
    node->mrn_value_type =          bt_uint32;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Media Encode options";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/rtstream/config/media-encode/*/extension";
    node->mrn_value_type =          bt_string;
    node->mrn_initial_value =       "";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Media Encode - Extension (.mpg, etc";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/rtstream/config/media-encode/*/module";
    node->mrn_value_type =          bt_string;
    node->mrn_initial_value =       "";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Media Encode - Extension (libmp4.so, libraw_udp.so, etc";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/rtstream/config/rtsp-origin/format/convert/enable";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Allow format conversion to .nknc or not";
    err = mdm_add_node(module, &node);
    bail_error(err);


    /* Upgrade processing nodes */

    err = md_upgrade_rule_array_new(&md_nvsd_rtstream_ug_rules);
    bail_error(err);
    ra = md_nvsd_rtstream_ug_rules;

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/rtstream/config/om_idle_timeout";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "60";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/rtstream/config/player_idle_timeout";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "60";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =   MUTT_DELETE;
    rule->mur_name =           "/nkn/nvsd/rtstream/config/server_port";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/rtstream/config/vod/server_port/1";
    rule->mur_new_value_type = bt_uint16;
    rule->mur_new_value_str =  "1";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/rtstream/config/vod/server_port/1/port";
    rule->mur_new_value_type = bt_uint16;
    rule->mur_new_value_str =  "554";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/rtstream/config/live/server_port/1";
    rule->mur_new_value_type = bt_uint16;
    rule->mur_new_value_str =  "1";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/rtstream/config/live/server_port/1/port";
    rule->mur_new_value_type = bt_uint16;
    rule->mur_new_value_str =  "8554";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/rtstream/config/media-encode/1";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "1";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/rtstream/config/media-encode/1/extension";
    rule->mur_new_value_type = bt_string;
    rule->mur_new_value_str =  ".mp4";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/rtstream/config/media-encode/1/module";
    rule->mur_new_value_type = bt_string;
    rule->mur_new_value_str =  "libmp4.so.1.0.1";
    MD_ADD_RULE(ra);


    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/rtstream/config/media-encode/2";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "2";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/rtstream/config/media-encode/2/extension";
    rule->mur_new_value_type = bt_string;
    rule->mur_new_value_str =  ".ts";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/rtstream/config/media-encode/2/module";
    rule->mur_new_value_type = bt_string;
    rule->mur_new_value_str =  "libmpeg2ts_h221.so.1.0.1";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =   MUTT_CLAMP;
    rule->mur_name =           "/nkn/nvsd/rtstream/config/player_idle_timeout";
    rule->mur_lowerbound =     10;
    rule->mur_upperbound =     300;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 5, 6);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/rtstream/config/rtsp-origin/format/convert/enable";
    rule->mur_new_value_type = bt_bool;
    rule->mur_new_value_str =  "false";
    MD_ADD_RULE(ra);

bail:
    return(err);
}


const bn_str_value md_nvsd_rtstream_initial_values[] = {
    {"/nkn/nvsd/rtstream/config/live/server_port/1", bt_uint16, "1"},
    {"/nkn/nvsd/rtstream/config/live/server_port/1/port", bt_uint16, "8554"}, // Default RTSP LIVE port
    {"/nkn/nvsd/rtstream/config/vod/server_port/1", bt_uint16, "1"},
    {"/nkn/nvsd/rtstream/config/vod/server_port/1/port", bt_uint16, "554"}, // Default RTSP VOD port
    {"/nkn/nvsd/rtstream/config/media-encode/1", bt_uint32, "1"},
    {"/nkn/nvsd/rtstream/config/media-encode/1/extension", bt_string, ".mp4"},
    {"/nkn/nvsd/rtstream/config/media-encode/1/module", bt_string, "libmp4.so.1.0.1"},
    {"/nkn/nvsd/rtstream/config/media-encode/2", bt_uint32, "2"},
    {"/nkn/nvsd/rtstream/config/media-encode/2/extension", bt_string, ".ts"},
    {"/nkn/nvsd/rtstream/config/media-encode/2/module", bt_string, "libmpeg2ts_h221.so.1.0.1"},
};

/* ------------------------------------------------------------------------- */
static int
md_nvsd_rtstream_add_initial(md_commit *commit, mdb_db *db, void *arg)
{
    int err = 0;
    bn_binding *binding = NULL;

    err = mdb_create_node_bindings
        (commit, db, md_nvsd_rtstream_initial_values,
         sizeof(md_nvsd_rtstream_initial_values) / sizeof(bn_str_value));
    bail_error(err);


 bail:
    bn_binding_free(&binding);
    return(err);
}

