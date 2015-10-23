/*
 *
 * Filename:  md_nvsd_network.c
 * Date:      2008/11/14
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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
#include "string.h"
#include "nkn_defs.h"

md_commit_forward_action_args md_dns_cache_fwd_args =
{
    GCL_CLIENT_NVSD, true, N_("Request failed; MFC subsystem did not respond"),
    mfat_blocking
#ifdef PROD_FEATURE_I18N_SUPPORT
    , GETTEXT_DOMAIN
#endif
};
#ifdef USE_MFD_LICENSE
static const char *nkn_mfd_lic_binding = "/nkn/mfd/licensing/config/mfd_licensed";
#endif /* USE_MFD_LICENSE */


/* ------------------------------------------------------------------------- */

int md_nvsd_network_init(const lc_dso_info *info, void *data);

static md_upgrade_rule_array *md_nvsd_network_ug_rules = NULL;

static int md_nvsd_network_commit_check(md_commit *commit,
                                const mdb_db *old_db, const mdb_db *new_db,
                                mdb_db_change_array *change_list, void *arg);

static int md_nvsd_network_commit_apply(md_commit *commit,
                                const mdb_db *old_db, const mdb_db *new_db,
                                mdb_db_change_array *change_list, void *arg);


static int
md_network_commit_side_effects(
        md_commit *commit,
        const mdb_db *old_db,
        mdb_db *inout_new_db,
        mdb_db_change_array *change_list,
        void *arg);

static int
md_network_remove_iptables(md_commit *commit,
                                  const mdb_db *db,
                                  const char *action_name,
                                  bn_binding_array *params,
                                  void *cb_arg);

/* ------------------------------------------------------------------------- */
int
md_nvsd_network_init(const lc_dso_info *info, void *data)
{
    int err = 0;
    md_module *module = NULL;
    md_reg_node *node = NULL;
    md_upgrade_rule *rule = NULL;
    md_upgrade_rule_array *ra = NULL;

    /*
     * Creating the Nokeena specific module
     */
    err = mdm_add_module("nvsd-network", 27,
	    "/nkn/nvsd/network", "/nkn/nvsd/network/config",
	    modrf_all_changes,
	    md_network_commit_side_effects, NULL,
	    md_nvsd_network_commit_check, NULL,
	    md_nvsd_network_commit_apply, NULL,
	    0, 0,
	    md_generic_upgrade_downgrade,
	    &md_nvsd_network_ug_rules,
	    NULL, NULL,
	    NULL, NULL, NULL, NULL,
	    &module);
    bail_error(err);

    /*
     * Setup config node - /nkn/nvsd/network/config/tunnel-only
     *  - Configure network tunnel-only debug feature
     *  - Default value is false
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/network/config/tunnel-only";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "false";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "network tunnel-only";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * Setup config node - /nkn/nvsd/network/config/threads
     *  - Configure network running threads
     *  - Default value is 0
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/network/config/threads";
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value_int = 0;
    node->mrn_limit_num_min_int = 0;
    node->mrn_limit_num_max_int = 16;
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "network threads";
    err = mdm_add_node(module, &node);
    bail_error(err);


    /*
     * Setup config node - /nkn/nvsd/network/config/policy
     *  - Configure network running threads
     *  - Default value is 0 ; round robin
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/network/config/policy";
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value_int = 0;
    node->mrn_limit_num_min_int = 0;
    node->mrn_limit_num_max_int = 1;
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "network threads policy";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * Setup config node - /nkn/nvsd/network/config/timeout
     *  - Configure network socket timeout
     *  - Default value is 60 seconds
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/network/config/timeout";
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value_int = 60;
    node->mrn_limit_num_min_int = 6;
    node->mrn_limit_num_max_int = 300;
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "network socket idle timed out";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * Setup config node - /nkn/nvsd/network/config/max_connections
     *  - Configure network socket max connections
     *  - Default value is 64000
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/network/config/max_connections";
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value_int = 10;
    node->mrn_limit_num_min_int = 10;
    node->mrn_limit_num_max_int = 512*1024;
    node->mrn_bad_value_msg     = N_("Value must be between 10 and 524288");
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "network socket max connections";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * Setup config node - /nkn/nvsd/network/config/stack
     *  - Configure network stack
     *  - Default value is linux kernel stack
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/network/config/stack";
    node->mrn_value_type        = bt_string;
    node->mrn_initial_value     = "linux_kernel_stack";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "use linux kernel stack";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * Setup config node - /nkn/nvsd/network/config/assured_flow_rate
     *  - Configure session assured flow rate
     *  - Default value is 0, disabled
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/network/config/assured_flow_rate";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  0;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  24000;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Session Assured Flow Rate (Kbits/sec)";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * Setup config node - /nkn/nvsd/network/config/afr_fast_start
     *  - Configure session assured flow rate fast start
     *  - Default value is 0, disabled
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/network/config/afr_fast_start";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  0;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  30;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Session Assured Flow Rate Fast Start (sec)";
    err = mdm_add_node(module, &node);
    bail_error(err);


    /*
     * Setup config node - /nkn/nvsd/network/config/max_bandwidth
     *  - Configure network s
     *  - Default value is 0. disabled
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/network/config/max_bandwidth";
    node->mrn_value_type =         bt_uint32;
    node->mrn_initial_value_int =  200;
    node->mrn_limit_num_min_int =  0;
    node->mrn_limit_num_max_int =  24000;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_basic;
    node->mrn_description =        "Session Max Bandwidth (KBits/sec)";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/network/config/access_control/permit/*";
    node->mrn_value_type =          bt_uint32;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =            mcf_cap_node_restricted;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/network/config/access_control/permit/*/address";
    node->mrn_value_type =          bt_ipv4addr;
    node->mrn_initial_value =       "0.0.0.0";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_restricted;
    node->mrn_description =         "IP Address";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/network/config/access_control/permit/*/mask";
    node->mrn_value_type =          bt_ipv4addr;
    node->mrn_initial_value =       "0.0.0.0";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_restricted;
    node->mrn_description =         "Subnet mask";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/network/config/access_control/deny/*";
    node->mrn_value_type =          bt_uint32;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_wildcard;
    node->mrn_cap_mask =            mcf_cap_node_restricted;
    node->mrn_description =         "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/network/config/access_control/deny/*/address";
    node->mrn_value_type =          bt_ipv4addr;
    node->mrn_initial_value =       "0.0.0.0";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_restricted;
    node->mrn_description =         "IP Address";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/network/config/access_control/deny/*/mask";
    node->mrn_value_type =          bt_ipv4addr;
    node->mrn_initial_value =       "0.0.0.0";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_restricted;
    node->mrn_description =         "Subnet mask";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/network/config/max_connections/no";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value =       "0";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_restricted;
    node->mrn_description =         "invertion internal use";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/network/config/max_bandwidth/no";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value =       "0";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_restricted;
    node->mrn_description =         "invertion internal use";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     * Setup config node - /nkn/nvsd/network/config/syn-cookie
     *  - Configure syncookie
     *  - Default value is 1. Enabled
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/network/config/syn-cookie";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "true";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "network syn-cookie";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/network/config/syn-cookie/half_open_conn";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value =       "1024";
    node->mrn_limit_num_min_int =   1;
    node->mrn_limit_num_max_int =   16000;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_restricted;
    node->mrn_description =         "network half-open connections";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/network/config/tcp_max_orphans";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value =       "131072";
    node->mrn_limit_num_min_int =   1024;
    node->mrn_limit_num_max_int =   524288;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_restricted;
    node->mrn_description =         "network tcp max_orphans";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/network/config/tcp_max_tw_buckets";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value =       "184320";
    node->mrn_limit_num_min_int =   1024;
    node->mrn_limit_num_max_int =   1048576;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_restricted;
    node->mrn_description =         "network tcp max_tw_buckets";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/network/config/tcp_fin_timeout";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value =       "20";
    node->mrn_limit_num_min_int =   5;
    node->mrn_limit_num_max_int =   120;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_restricted;
    node->mrn_description =         "network tcp fin_timeout";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/network/config/tcp_mem/low";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value =       "196608";
    node->mrn_limit_num_min_int =   98304;
    node->mrn_limit_num_max_int =   393216;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_restricted;
    node->mrn_description =         "network tcp memory low";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/network/config/tcp_mem/high";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value =       "262144";
    node->mrn_limit_num_min_int =   131072;
    node->mrn_limit_num_max_int =   524288;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_restricted;
    node->mrn_description =         "network tcp memory high";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/network/config/tcp_mem/maxpage";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value =       "1572864";
    node->mrn_limit_num_min_int =   196608;
    node->mrn_limit_num_max_int =   6291456;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_restricted;
    node->mrn_description =         "network tcp memory maxpage";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/network/config/tcp_wmem/min";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value =       "4096";
    node->mrn_limit_num_min_int =   4096;
    node->mrn_limit_num_max_int =   32768;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_restricted;
    node->mrn_description =         "network tcp write min memory";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/network/config/tcp_wmem/default";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value =       "65536";
    node->mrn_limit_num_min_int =   32768;
    node->mrn_limit_num_max_int =   1048576;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_restricted;
    node->mrn_description =         "network tcp write default memory";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/network/config/tcp_wmem/max";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value =       "16777216";
    node->mrn_limit_num_min_int =   65536;
    node->mrn_limit_num_max_int =   16777216;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_restricted;
    node->mrn_description =         "network tcp write max memory";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/network/config/tcp_rmem/min";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value =       "4096";
    node->mrn_limit_num_min_int =   4096;
    node->mrn_limit_num_max_int =   32768;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_restricted;
    node->mrn_description =         "network tcp read min memory";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/network/config/tcp_rmem/default";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value =       "87380";
    node->mrn_limit_num_min_int =   32768;
    node->mrn_limit_num_max_int =   1048576;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_restricted;
    node->mrn_description =         "network tcp read default memory";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/network/config/tcp_rmem/max";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value =       "16777216";
    node->mrn_limit_num_min_int =   65536;
    node->mrn_limit_num_max_int =   16777216;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_restricted;
    node->mrn_description =         "network tcp read max memory";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/network/config/ip_conntrack_tcp_timeout_time_wait";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value =       "30";
    node->mrn_limit_num_min_int =   5;
    node->mrn_limit_num_max_int =   120;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_restricted;
    node->mrn_description =         "network tcp conntrack time-wait";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/network/config/ip_conntrack_tcp_timeout_close_wait";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value =       "10";
    node->mrn_limit_num_min_int =   0;
    node->mrn_limit_num_max_int =   300;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_restricted;
    node->mrn_description =         "network tcp conntrack timeout closewait";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/network/config/resolver/async-enable";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "true";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_restricted;
    node->mrn_description =         "Flag to enable/disable async-dns resolver feature";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/network/config/resolver/adnsd-enabled";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_restricted;
    node->mrn_description =         "Flag to enable/disable adns daemon";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/network/config/resolver/cache-timeout";
    node->mrn_value_type =          bt_int32;
    node->mrn_initial_value =       "-100";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_restricted;
    node->mrn_description =         "timeout value for every dns cache entry; "
	"-1 = random timeouts, 0 = never timeout, "
	">0 timeout in seconds";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 0);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/network/actions/get_dns_cache";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_func       = md_commit_forward_action;
    node->mrn_action_arg        = &md_dns_cache_fwd_args;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 1);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/network/actions/get_dns_cache_fqdn";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_func       = md_commit_forward_action;
    node->mrn_action_arg        = &md_dns_cache_fwd_args;
    node->mrn_actions[0]->mra_param_name = "domain";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 0);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/network/actions/del_dns_cache_all";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_func       = md_commit_forward_action;
    node->mrn_action_arg        = &md_dns_cache_fwd_args;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 1);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/network/actions/del_dns_cache_entry";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_func       = md_commit_forward_action;
    node->mrn_action_arg        = &md_dns_cache_fwd_args;
    node->mrn_actions[0]->mra_param_name = "domain";
    node->mrn_actions[0]->mra_param_type = bt_string;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_action(module, &node, 0);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/network/actions/remove_iptables";
    node->mrn_node_reg_flags    = mrf_flags_reg_action;
    node->mrn_cap_mask          = mcf_cap_action_basic;
    node->mrn_action_async_nonbarrier_start_func        = md_network_remove_iptables;
    node->mrn_description = "";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/network/config/ip_conntrack_max_entries";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value =       "1048576";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_restricted;
    node->mrn_description =         "Max conntrack entries that can be held";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/network/config/path-mtu-discovery";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "true";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_restricted;
    node->mrn_description =         "network tcp path_mtu_discovery";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/network/config/bond_if_cfg_enable";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_restricted;
    node->mrn_description =         "Flag to enable/disable bond interface configuration";
    err = mdm_add_node(module, &node);
    bail_error(err);

    /*
     *Node to hold the number of seconds after which the BM will tunnel the requests
     *to the origin,if no response from HTTP OM comes.
     *
     */
    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/network/config/connection/origin/queue/max_delay";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value =       "3";
    node->mrn_limit_num_min_int =   0;
    node->mrn_limit_num_max_int =   120;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_restricted;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/network/config/connection/origin/queue/max_num";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value =       "2";
    node->mrn_limit_num_min_int =   0;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_restricted;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/network/config/connection/origin/failover/use_dns_response";
    node->mrn_value_type            = bt_bool;
    node->mrn_initial_value         = "false";
    node->mrn_node_reg_flags        = mrf_flags_reg_config_literal;
    node->mrn_cap_mask              = mcf_cap_node_restricted;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/network/config/connection/origin/connect/timeout";
    node->mrn_value_type =          bt_uint32;
    node->mrn_initial_value =       "10";
    node->mrn_limit_num_min_int =   6;
    node->mrn_limit_num_max_int =   120;
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_restricted;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =                "/nkn/nvsd/network/config/ip-forward";
    node->mrn_value_type =          bt_bool;
    node->mrn_initial_value =       "false";
    node->mrn_node_reg_flags =      mrf_flags_reg_config_literal;
    node->mrn_cap_mask =            mcf_cap_node_basic;
    node->mrn_description =         "Flag to enable/disable ip forward configuration";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/network/config/max_connections/pmapper-disable";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "false";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Enables/Disables port mapper module";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node (module, &node);
    bail_error (err);
    node->mrn_name =               "/nkn/nvsd/network/config/connection/transparent-proxy/port_range_min";
    node->mrn_value_type =         bt_uint16;
    node->mrn_initial_value_int =  D_TP_PORT_RANGE_MIN_DEF;
    node->mrn_limit_num_min_int =  D_TP_PORT_RANGE_MIN1;
    node->mrn_limit_num_max_int =  D_TP_PORT_RANGE_MIN2;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_restricted;
    node->mrn_description =        "T-proxy source port range min";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name =               "/nkn/nvsd/network/config/connection/transparent-proxy/port_range_max";
    node->mrn_value_type =         bt_uint16;
    node->mrn_initial_value_int =  D_TP_PORT_RANGE_MAX_DEF;
    node->mrn_limit_num_min_int =  D_TP_PORT_RANGE_MAX1;    /* Set the max possible */
    node->mrn_limit_num_max_int =  D_TP_PORT_RANGE_MAX2;
    node->mrn_node_reg_flags =     mrf_flags_reg_config_literal;
    node->mrn_cap_mask =           mcf_cap_node_restricted;
    node->mrn_description =        "T-proxy source port range max";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/network/config/dynamic-routing";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "false";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Enables/Disables bgpd,zebra(dynamic routing) module";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name 		= "/nkn/nvsd/network/config/igmp-version";
    node->mrn_value_type	= bt_uint32;
    node->mrn_initial_value 	= "2";
    node->mrn_limit_num_min_int = 1;
    node->mrn_limit_num_max_int = 3;
    node->mrn_node_reg_flags	= mrf_flags_reg_config_literal;
    node->mrn_cap_mask 		= mcf_cap_node_restricted;
    node->mrn_description       = "IGMP Version Number";
    err = mdm_add_node(module, &node);
    bail_error(err);


    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/network/config/ip-tables";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "false";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    node->mrn_description       = "Removes the ip table modules";
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/network/config/connection/origin/max-unresponsive-conns";
    node->mrn_value_type        = bt_uint32;
    node->mrn_initial_value     = "0";
    node->mrn_limit_num_min_int = 0;
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = mdm_new_node(module, &node);
    bail_error(err);
    node->mrn_name              = "/nkn/nvsd/network/config/tcp/timestamp";
    node->mrn_value_type        = bt_bool;
    node->mrn_initial_value     = "true";
    node->mrn_node_reg_flags    = mrf_flags_reg_config_literal;
    node->mrn_cap_mask          = mcf_cap_node_restricted;
    err = mdm_add_node(module, &node);
    bail_error(err);

    err = md_upgrade_rule_array_new(&md_nvsd_network_ug_rules);
    bail_error(err);
    ra = md_nvsd_network_ug_rules;

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/max_connections/no";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "0";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 1, 2);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/max_bandwidth/no";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "0";
    MD_ADD_RULE(ra);


    MD_NEW_RULE(ra, 2, 3);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/tunnel-only";
    rule->mur_new_value_type = bt_bool;
    rule->mur_new_value_str =  "false";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 3, 4);
    rule->mur_upgrade_type =   MUTT_CLAMP;
    rule->mur_name =           "/nkn/nvsd/network/config/max_connections";
    rule->mur_lowerbound =      10;
    rule->mur_upperbound =      256*1024;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 4, 5);
    rule->mur_upgrade_type =   MUTT_MODIFY;
    rule->mur_name =           "/nkn/nvsd/network/config/threads";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "0";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 5, 6);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/syn-cookie";
    rule->mur_new_value_type = bt_bool;
    rule->mur_new_value_str =  "true";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 5, 6);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/syn-cookie/half_open_conn";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "1024";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/tcp_max_orphans";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "131072";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/tcp_max_tw_buckets";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "16384";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/tcp_fin_timeout";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "20";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/tcp_rmem/min";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "4096";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/tcp_rmem/default";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "87380";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/tcp_rmem/max";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "16777216";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/tcp_wmem/min";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "4096";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/tcp_wmem/default";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "65536";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/tcp_wmem/max";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "16777216";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/tcp_mem/low";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "196608";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/tcp_mem/high";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "262144";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 6, 7);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/tcp_mem/maxpage";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "6291456";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 7, 8);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/ip_conntrack_tcp_timeout_time_wait";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "30";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 8, 9);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/resolver/async-enable";
    rule->mur_new_value_type = bt_bool;
    rule->mur_new_value_str =  "true";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 8, 9);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/resolver/cache-timeout";
    rule->mur_new_value_type = bt_int32;
    rule->mur_new_value_str =  "2047";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 8, 9);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/resolver/adnsd-enabled";
    rule->mur_new_value_type = bt_bool;
    rule->mur_new_value_str =  "false";
    MD_ADD_RULE(ra);


    MD_NEW_RULE(ra, 9, 10);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/ip_conntrack_tcp_timeout_close_wait";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "10";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 10, 11);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/bond_if_cfg_enable";
    rule->mur_new_value_type = bt_bool;
    rule->mur_new_value_str =  "false";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 11, 12);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =	       "/nkn/nvsd/network/config/connection/origin/queue/max_delay";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "0";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 12, 13);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =	       "/nkn/nvsd/network/config/connection/origin/queue/max_num";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "0";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 13, 14);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/path-mtu-discovery";
    rule->mur_new_value_type = bt_bool;
    rule->mur_new_value_str =  "true";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 14, 15);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/ip_conntrack_max_entries";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "1048576";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 15, 16);
    rule->mur_upgrade_type =   MUTT_CLAMP;
    rule->mur_name =           "/nkn/nvsd/network/config/max_connections";
    rule->mur_lowerbound =      10;
    rule->mur_upperbound =      512*1024;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 16, 17);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/ip-forward";
    rule->mur_new_value_type = bt_bool;
    rule->mur_new_value_str =  "false";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 17, 18);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/max_connections/pmapper-disable";
    rule->mur_new_value_type = bt_bool;
    rule->mur_new_value_str =  "false";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 18, 19);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/dynamic-routing";
    rule->mur_new_value_type = bt_bool;
    rule->mur_new_value_str =  "false";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 19, 20);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/ip-tables";
    rule->mur_new_value_type = bt_bool;
    rule->mur_new_value_str =  "false";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 20, 21);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/igmp-version";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "2";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 23, 24);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/connection/transparent-proxy/port_range_min";
    rule->mur_new_value_type = bt_uint16;
    rule->mur_new_value_str =  D_TP_PORT_RANGE_MIN_DEF_STR;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 23, 24);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/connection/transparent-proxy/port_range_max";
    rule->mur_new_value_type = bt_uint16;
    rule->mur_new_value_str =  D_TP_PORT_RANGE_MAX_DEF_STR;
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 22, 23);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/connection/origin/failover/use_dns_response";
    rule->mur_new_value_type = bt_bool;
    rule->mur_new_value_str =  "false";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 22, 23);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/connection/connection/origin/connect/timeout";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "10";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 24, 25);
    rule->mur_upgrade_type =   MUTT_MODIFY;
    rule->mur_name =           "/nkn/nvsd/network/config/resolver/cache-timeout";
    rule->mur_new_value_type = bt_int32;
    rule->mur_new_value_str =  "-100";
    rule->mur_modify_cond_value_str = "2047";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 25, 26);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/connection/origin/max-unresponsive-conns";
    rule->mur_new_value_type = bt_uint32;
    rule->mur_new_value_str =  "0";
    MD_ADD_RULE(ra);

    MD_NEW_RULE(ra, 26, 27);
    rule->mur_upgrade_type =   MUTT_ADD;
    rule->mur_name =           "/nkn/nvsd/network/config/tcp/timestamp";
    rule->mur_new_value_type = bt_bool;
    rule->mur_new_value_str =  "true";
    MD_ADD_RULE(ra);
bail:
    return(err);
}



static int
md_network_commit_side_effects(
        md_commit *commit,
        const mdb_db *old_db,
        mdb_db *inout_new_db,
        mdb_db_change_array *change_list,
        void *arg)
{

    int err = 0;
    tbool found = false;
    tbool t_license = false;
    int i = 0, num_changes = 0;
    tstring *t_inverse_max_connection = NULL;
    const mdb_db_change *change = NULL;

    num_changes = mdb_db_change_array_length_quick (change_list);
    for (i = 0; i < num_changes; i++)
    {
	change = mdb_db_change_array_get_quick (change_list, i);
	bail_null (change);

	/* Check if it is namespace creation */
	if ((strcmpi(ts_str(change->mdc_name), "/nkn/nvsd/network/config/max_connections/no") == 0)
		&& (mdct_modify == change->mdc_change_type))
	{
	    const char *t_max_connections = NULL;
	    err = mdb_get_node_value_tstr(commit, inout_new_db, "/nkn/nvsd/network/config/max_connections/no", 0, &found,
		    &t_inverse_max_connection);
	    bail_error(err);
	    // if zero dont do anything
	    if (ts_equal_str(t_inverse_max_connection,"1",false))
	    {
#ifndef USE_MFD_LICENSE
		t_max_connections = VALID_LICENSE_MAXCONN ;
#else /*USE_MFD_LICENSE*/
		/* First get the license node */

		err = mdb_get_node_value_tbool(commit, inout_new_db, nkn_mfd_lic_binding,
			0, &found, &t_license);
		bail_error(err);
		if (t_license)
		{
		    t_max_connections = VALID_LICENSE_MAXCONN ;
		} else {
		    t_max_connections = INVALID_LICENSE_MAXCONN ;
		}
#endif /*USE_MFD_LICENSE*/
		err = mdb_set_node_str(commit, inout_new_db, bsso_modify, 0,
			bt_uint32, t_max_connections, "/nkn/nvsd/network/config/max_connections");
		bail_error(err);
	    }
	} else if ((strcmpi(ts_str(change->mdc_name), "/nkn/nvsd/network/config/max_bandwidth/no") == 0)
		&& (mdct_modify == change->mdc_change_type))
	{
	    const char *t_max_bandwidth = NULL;
	    err = mdb_get_node_value_tstr(commit, inout_new_db, "/nkn/nvsd/network/config/max_bandwidth/no", 0, &found,
		    &t_inverse_max_connection);
	    bail_error(err);
	    // if zero dont do anything
	    if (ts_equal_str(t_inverse_max_connection,"1",false))
	    {
#ifndef USE_MFD_LICENSE
		t_max_bandwidth = VALID_LICENSE_MAXBW;
#else /*USE_MFD_LICENSE*/

		/* First get the license node */
		err = mdb_get_node_value_tbool(commit, inout_new_db, nkn_mfd_lic_binding,
			0, &found, &t_license);
		bail_error(err);
		if (t_license)
		{
		    t_max_bandwidth = VALID_LICENSE_MAXBW;
		} else {
		    t_max_bandwidth = INVALID_LICENSE_MAXBW;
		}
#endif /* USE_MFD_LICENSE */
		err = mdb_set_node_str(commit, inout_new_db, bsso_modify, 0,
			bt_uint32, t_max_bandwidth, "/nkn/nvsd/network/config/max_bandwidth");
		bail_error(err);
	    }
	}else if (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 3, "iptables", "config", "enable")
		&& ((mdct_modify == change->mdc_change_type)||(mdct_add == change->mdc_change_type)) ){
	    tbool t_ipfilter_enable = false;

	    err = mdb_get_node_value_tbool(commit, inout_new_db, "/iptables/config/enable", 0, 0, &t_ipfilter_enable);
	    bail_error(err);

	    if (t_ipfilter_enable == true) {
		err = mdb_set_node_str(commit, inout_new_db, bsso_modify, 0, bt_bool, "true",
			"/nkn/nvsd/network/config/ip-tables");
		bail_error(err);
	    }
	}
    }
bail:
    ts_free(&t_inverse_max_connection);
    return err;
}



/* ------------------------------------------------------------------------- */
static int
md_nvsd_network_commit_check(md_commit *commit,
                     const mdb_db *old_db,
                     const mdb_db *new_db,
                     mdb_db_change_array *change_list, void *arg)
{
    int err = 0;
    tbool found = false;
    tbool t_license = false;
    tstring *t_max_connections = NULL;
    tstring *t_max_bandwidth = NULL;
    int i = 0, num_changes = 0;
    const mdb_db_change *change = NULL;
    const bn_attrib *new_val = NULL;
    uint32 ui_afr = 0, ui_maxbw = 0;
    int32 status = 0;
    tstring *ret_output = NULL;

    /*
     *  Add the license check
     *  as of March 16th, 2009 license is only controlling
     *  number of concurrent sessions and max bandwidth per session
     *  If no license then sessions is set at 200 and max bandwidth
     *  is set to 200kbps
     */

    /* Since license does not exist, apply the limitations */
    num_changes = mdb_db_change_array_length_quick (change_list);
    for (i = 0; i < num_changes; i++)
    {
	change = mdb_db_change_array_get_quick (change_list, i);
	bail_null (change);

	/* Check if it is network max concurrent sessions */
	if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 5, "nkn", "nvsd", "network", "config", "max_connections"))
		&& (mdct_modify == change->mdc_change_type))
	{
#ifndef USE_MFD_LICENSE
	    err = md_commit_set_return_status(commit, 0);
	    goto bail;
#else /*USE_MFD_LICENSE*/
	    /* First get the license node */
	    err = mdb_get_node_value_tbool(commit, new_db, nkn_mfd_lic_binding,
		    0, &found, &t_license);
	    bail_error(err);

	    if (t_license)
	    {
		/* License exists hence nothing to check */
		err = md_commit_set_return_status(commit, 0);
		goto bail;
	    }

	    err = mdb_get_node_value_tstr(commit, new_db, ts_str(change->mdc_name),
		    0, &found, &t_max_connections);
	    bail_error(err);
	    bail_null(t_max_connections);

	    /* Check if the value is default for no license */
	    if (strcmp (ts_str(t_max_connections), INVALID_LICENSE_MAXCONN))
	    {
		err = md_commit_set_return_status_msg(commit, 1,
			"error: license does not exist to change this parameter");
		bail_error(err);
		goto bail;
	    }
#endif /*USE_MFD_LICENSE*/
	}
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 5, "nkn", "nvsd", "network", "config", "max_bandwidth"))
		&& (mdct_modify == change->mdc_change_type))
	{
#ifndef USE_MFD_LICENSE
	    err = md_commit_set_return_status(commit, 0);
	    goto bail;
#else /*USE_MFD_LICENSE*/
	    /* First get the license node */
	    err = mdb_get_node_value_tbool(commit, new_db, nkn_mfd_lic_binding,
		    0, &found, &t_license);
	    bail_error(err);

	    if (t_license)
	    {
		/* License exists hence nothing to check */
		err = md_commit_set_return_status(commit, 0);
		goto bail;
	    }

	    err = mdb_get_node_value_tstr(commit, new_db, ts_str(change->mdc_name),
		    0, &found, &t_max_bandwidth);
	    bail_error(err);
	    bail_null(t_max_bandwidth);

	    /* Check if the value is default for no license */
	    if (strcmp (ts_str(t_max_bandwidth), INVALID_LICENSE_MAXBW))
	    {
		err = md_commit_set_return_status_msg(commit, 1,
			"error: license does not exist to change this parameter");
		bail_error(err);
		goto bail;
	    }
#endif /*USE_MFD_LICENSE*/
	}
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 5, "nkn", "nvsd", "network", "config", "assured_flow_rate"))
		&& (mdct_modify == change->mdc_change_type))
	{
	    err = mdb_get_node_value_uint32(commit, new_db, "/nkn/nvsd/network/config/max_bandwidth",
		    0, &found, &ui_maxbw);
	    bail_error(err);

	    if (found) {
		/* Get the valud currently being configured
		 */
		new_val = bn_attrib_array_get_quick(change->mdc_new_attribs, ba_value);
		if (!new_val) {
		    lc_log_debug(LOG_NOTICE, "Couldn't get new attributes..");
		    goto bail;
		}
		err = bn_attrib_get_uint32(new_val, NULL, NULL, &ui_afr);
		bail_error(err);

		if ( (ui_maxbw > 0) && (ui_afr > ui_maxbw) ) {
		    err = md_commit_set_return_status_msg(commit, 1,
			    "error: session assured flow rate cannot be more the maximum bandwidth configured.");
		    bail_error(err);
		    goto bail;
		}
	    }
	}
	else if (((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7,
			    "nkn", "nvsd", "network", "config", "connection", "transparent-proxy", "port_range_min")) ||
		    (bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 7,
						   "nkn", "nvsd", "network", "config", "connection", "transparent-proxy", "port_range_max")))
		&& (mdct_modify == change->mdc_change_type))
	{
	    uint16 t_port_range_max = 0;
	    uint16 t_port_range_min = 0;

	    err = mdb_get_node_value_uint16(commit, new_db, "/nkn/nvsd/network/config/connection/transparent-proxy/port_range_max",
		    0, NULL, &t_port_range_max);
	    bail_error(err);

	    err = mdb_get_node_value_uint16(commit, new_db, "/nkn/nvsd/network/config/connection/transparent-proxy/port_range_min",
		    0, NULL, &t_port_range_min);
	    bail_error(err);

	    if ((t_port_range_min > t_port_range_max) || (t_port_range_min == t_port_range_max)) {
		err = md_commit_set_return_status_msg_fmt(commit, 1, _("port-range-max must be larger than port-range-min"));
		bail_error(err);
		goto bail;
	    }
	}
	else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 5, "nkn", "nvsd", "network","config", "ip-tables"))
		&& (mdct_modify == change->mdc_change_type))
	{
	    tbool t_iptable = false;
	    tbool t_ipfilter_enable = false;
	    tstr_array *tproxy_interfaces = NULL;
	    uint32 count = 0;

	    err = mdb_get_node_value_tbool(commit, new_db, ts_str(change->mdc_name), 0, 0, &t_iptable);
	    bail_error(err);

	    if (t_iptable == false){

		err = mdb_get_node_value_tbool(commit, new_db, "/iptables/config/enable", 0, 0, &t_ipfilter_enable);
		bail_error(err);

		if (t_ipfilter_enable == true) {
		    err = md_commit_set_return_status_msg(commit, 1,
			    "IP Tables cannot be disabled, because of IP filtering is enabled.");
		    bail_error(err);
		    goto bail;
		}

		err = mdb_get_matching_tstr_array( NULL,
			(const mdb_db *)new_db,
			"/nkn/nvsd/http/config/t-proxy/*",
			0,
			&tproxy_interfaces);
		bail_error(err);
		bail_null(tproxy_interfaces);

		count = tstr_array_length_quick(tproxy_interfaces);

		if (count > 0) {
		    err = md_commit_set_return_status_msg(commit, 1,
			    "IP Tables cannot be disabled, because of TPROXY is enabled.");
		    bail_error(err);
		    goto bail;
		}
	    }
	    tstr_array_free(&tproxy_interfaces);
	}
    }
    err = md_commit_set_return_status(commit, 0);
    bail_error(err);

bail:
    ts_free(&t_max_connections);
    ts_free(&t_max_bandwidth);
    ts_free(&ret_output);
    return(err);
}


/* ------------------------------------------------------------------------- */
int
write_proc_fs(const char *proc_file, const char *proc_value);
int
write_proc_fs(const char *proc_file, const char *proc_value)
{
    int err = 0;
    tbool exist = false;
    mode_t proc_fs_mode = JNPR_PROC_FILE_MODE;
    int proc_fd = -1;

    if ((NULL == proc_file) || (NULL == proc_value)) {
	err = 1;
	goto bail;
    }

    err = lf_test_is_regular(proc_file, &exist);

    if (err || !exist) {
	lc_log_basic(LOG_ERR, "Could not read proc file '%s'", proc_file);
	err = lc_err_file_not_found;
	goto bail;
    }

    proc_fd = open(proc_file, O_WRONLY );
    if (proc_fd == -1) {
        err = lc_err_file_not_found;
        goto bail;
    }

    err = lf_write(proc_fd, proc_value, strlen(proc_value));
    if (err == -1) {
	/* there  is some other error */
	lc_log_basic(LOG_ERR, "lf_write() failed");
    } else if (err != (int32)strlen(proc_value)) {
	/* lf_write() didn't wrote all the bytes, that was requested */
	lc_log_basic(LOG_ERR, "lf_write() failed to write %d (%ld) bytes to %s",
		err, strlen(proc_value), proc_file);
	err = 0;
    } else {
	/* else all is well */
	err = 0;
    }

bail:
    return err;
}


static int
md_nvsd_network_commit_apply(md_commit *commit,
                     const mdb_db *old_db, const mdb_db *new_db,
                     mdb_db_change_array *change_list, void *arg)
{
    int err = 0;
    int i = 0, num_changes = 0;
    const mdb_db_change *change = NULL;
    const char *proc_value = "1";
    tbool found = false;
    tbool t_dyn_routing = false;
    tbool t_ip_tables = false, tcp_timestamp = true;
    tstring *ret_output = NULL;
    int32 status = 0;
    bn_request *req = NULL;
    tstring *t_igmp_version = NULL ;

    num_changes = mdb_db_change_array_length_quick (change_list);
    for (i = 0; i < num_changes; i++)
    {
	change = mdb_db_change_array_get_quick (change_list, i);
	bail_null (change);
	if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 5, "nkn", "nvsd", "network", "config", "ip-tables"))
		&& ((mdct_modify == change->mdc_change_type) || (mdct_add == change->mdc_change_type)))
	{
	    err = mdb_get_node_value_tbool(commit, (mdb_db *)new_db, ts_str(change->mdc_name), 0, 0, &t_ip_tables);
	    bail_error(err);

	    if (t_ip_tables == false){
		err = bn_action_request_msg_create(&req, "/nkn/nvsd/network/actions/remove_iptables");
		bail_error(err);

		err = md_commit_handle_action_from_module(commit, req, NULL, NULL, NULL, NULL, NULL);
		bail_error(err);
	    }
	}else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts, 0, 5, "nkn", "nvsd", "network", "config", "igmp-version"))
		&& ((mdct_modify == change->mdc_change_type) || (mdct_add == change->mdc_change_type)))
	{
	    err = mdb_get_node_value_tstr(commit, (mdb_db *)new_db, ts_str(change->mdc_name), 0, 0, &t_igmp_version);
	    bail_error(err);
	    bail_null(t_igmp_version);

	    err = write_proc_fs("/proc/sys/net/ipv4/conf/all/force_igmp_version",
		    (const char *)ts_str(t_igmp_version));
	    bail_error(err);

	} else if ((bn_binding_name_parts_pattern_match_va(change->mdc_name_parts,
			0, 6, "nkn", "nvsd", "network", "config", "tcp", "timestamp"))
		&& ((mdct_modify == change->mdc_change_type) || (mdct_add == change->mdc_change_type))) {

	    err = mdb_get_node_value_tbool(commit, new_db, ts_str(change->mdc_name),
		    0, 0, &tcp_timestamp);
	    bail_error(err);

	    if (tcp_timestamp) {
		proc_value = "1";
	    } else {
		proc_value = "0";
	    }
	    err = write_proc_fs("/proc/sys/net/ipv4/tcp_timestamps", proc_value);
	    bail_error(err);
	}
    }

    err = md_commit_set_return_status(commit, 0);
    bail_error(err);

bail:
    ts_free(&t_igmp_version);
    bn_request_msg_free(&req);
    return(err);
}

static int
md_network_remove_iptables(md_commit *commit,
                                  const mdb_db *db,
                                  const char *action_name,
                                  bn_binding_array *params,
                                  void *cb_arg)
{
    int err = 0;
    int32 status = 0;
    tstring *ret_output = NULL;

    err = lc_launch_quick_status(&status, &ret_output, false, 1, "/opt/nkn/bin/remove_iptables.sh");
    bail_error(err);
    if ((status) && (ret_output != NULL))
    {
	lc_log_basic(LOG_NOTICE, "failed to remove the iptables status:%d\n %s", WEXITSTATUS(status), ts_str(ret_output));
    }

bail:
    ts_free(&ret_output);
    return err;
}
