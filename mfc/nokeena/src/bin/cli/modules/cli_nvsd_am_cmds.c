/*
 * Filename :   cli_nvsd_am_cmds.c
 * Date     :   2008/12/07
 * Author   :   Dhruva Narasimhan
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 */

/*----------------------------------------------------------------------------
 * HEADER FILES
 *---------------------------------------------------------------------------*/
#include "common.h"
#include "climod_common.h"
#include "clish_module.h"
#include "proc_utils.h"
#include "file_utils.h"
#include "tpaths.h"
#include "nkn_defs.h"

/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
int 
cli_nvsd_am_cmds_init(
        const lc_dso_info   *info,
        const cli_module_context *context);

int 
cli_nvsd_analytics_show(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int 
cli_nvsd_am_show_tier(
	void *data,
	const cli_command *cmd,
	const tstr_array *cmd_line,
	const tstr_array *params);

int 
cli_nvsd_am_show_rank(
	void *data,
	const cli_command *cmd,
	const tstr_array *cmd_line,
	const tstr_array *params);

int 
cli_nvsd_am_cmds_init(
        const lc_dso_info   *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    /*----------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics";
    cmd->cc_help_descr =        N_("Configure Analytics parameters");
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no analytics";
    cmd->cc_help_descr =        N_("Set default Analytics parameters");
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    /*----------------------------------------------------------------------*/


    /*----------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics cache-promotion";
    cmd->cc_help_descr =        N_("Enable cache promotion");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no analytics cache-promotion";
    cmd->cc_help_descr =        N_("Reset the cache promotion values");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics cache-promotion enable";
    cmd->cc_help_descr =        N_("Enable cache promotion");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/am/config/cache_promotion_enabled";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_exec_value =        "true";
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_analytics;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics cache-promotion disable";
    cmd->cc_help_descr =        N_("Disable cache promotion");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/am/config/cache_promotion_enabled";
    cmd->cc_exec_type =         bt_bool;
    cmd->cc_exec_value =        "false";
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_analytics;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics cache-promotion size-threshold";
    cmd->cc_help_descr =        N_("Configure size threshold");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no analytics cache-promotion size-threshold";
    cmd->cc_help_descr =        N_("Set default cache size threshold (0)");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/am/config/cache_promotion/size_threshold";
    cmd->cc_exec_type =         bt_uint32;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics cache-promotion size-threshold *";
    cmd->cc_help_exp =          N_("<size in bytes>");
    cmd->cc_help_exp_hint =     N_("bytes");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/am/config/cache_promotion/size_threshold";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_analytics;

    CLI_CMD_REGISTER;

#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics cache-promotion hit-threshold";
    cmd->cc_help_descr =        N_("Configure hit threshold");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no analytics cache-promotion hit-threshold";
    cmd->cc_help_descr =        N_("Set default cache hit threshold (10)");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/am/config/cache_promotion/hit_threshold";
    cmd->cc_exec_type =         bt_uint32;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics cache-promotion hit-threshold *";
    cmd->cc_help_exp =          N_("<number>");
    cmd->cc_help_exp_hint =     N_("number");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/am/config/cache_promotion/hit_threshold";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_analytics;

    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics cache-promotion hit-increment";
    cmd->cc_help_descr =        N_("Configure hit increment");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no analytics cache-promotion hit-increment";
    cmd->cc_help_descr =        N_("Set default cache hit increment (100)");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/am/config/cache_promotion/hit_increment";
    cmd->cc_exec_type =         bt_uint32;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics cache-promotion hit-increment *";
    cmd->cc_help_exp =          N_("<number>");
    cmd->cc_help_exp_hint =     N_("number");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/am/config/cache_promotion/hit_increment";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_analytics;

    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics cache-promotion hotness-increment";
    cmd->cc_help_descr =        N_("Configure hotness increment");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no analytics cache-promotion hotness-increment";
    cmd->cc_help_descr =        N_("Set default hotness increment (100)");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/am/config/cache_promotion/hotness_increment";
    cmd->cc_exec_type =         bt_uint32;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics cache-promotion hotness-increment *";
    cmd->cc_help_exp =          N_("<number>");
    cmd->cc_help_exp_hint =     N_("number");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/am/config/cache_promotion/hotness_increment";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_analytics;

    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics cache-promotion hotness-threshold";
    cmd->cc_help_descr =        N_("Configure hotness threshold");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no analytics cache-promotion hotness-threshold";
    cmd->cc_help_descr =        N_("Set default cache hotness threshold (3)");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/am/config/cache_promotion/hotness_threshold";
    cmd->cc_exec_type =         bt_uint32;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics cache-promotion hotness-threshold *";
    cmd->cc_help_exp =          N_("<number>");
    cmd->cc_help_exp_hint =     N_("number");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/am/config/cache_promotion/hotness_threshold";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_analytics;
    CLI_CMD_REGISTER;

    /*----------------------------------------------------------------------*/


    /*----------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics cache-evict";
    cmd->cc_help_descr =        N_("Configure Cache Eviction");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics cache-evict wait-time";
    cmd->cc_help_descr =        N_("Set cache eviction timeout");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics cache-evict wait-time *";
    cmd->cc_help_exp =          N_("<number>");
    cmd->cc_help_exp_hint =     N_("Timeout in seconds");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_priv_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/am/config/cache_evict_aging_time";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_duration;
    cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics cache-evict wait-time 10";
    cmd->cc_help_descr =        N_("Disable timeout, set to 0 seconds");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_priv_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/am/config/cache_evict_aging_time";
    cmd->cc_exec_value =        "10";
    cmd->cc_exec_type =         bt_duration;
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;
    /*----------------------------------------------------------------------*/
#endif

    /*----------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics cache-ingest";
    cmd->cc_help_descr =        N_("Configure Cache Ingest");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no analytics cache-ingest";
    cmd->cc_help_descr =        N_("Set default Cache Ingest options");
    CLI_CMD_REGISTER;

#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics cache-ingest hit-threshold";
    cmd->cc_help_descr =        N_("Configure Cache ingest hit threshold "
                                    "parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no analytics cache-ingest hit-threshold";
    cmd->cc_help_descr =        N_("Set default Cache ingest hit threshold "
                                    "parameters (3)");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_name =         "/nkn/nvsd/am/config/cache_ingest_hits_threshold";
    cmd->cc_exec_type =         bt_uint32;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics cache-ingest hit-threshold 1";
    cmd->cc_help_descr =        N_("Default hit threshold - 1");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_priv_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/am/config/cache_ingest_hits_threshold";
    cmd->cc_exec_value =        "1";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_none;
    //cmd->cc_revmap_order =      cro_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics last-evict-time-diff";
    cmd->cc_help_descr =        N_("Last Eviction time diff");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics last-evict-time-diff 1";
    cmd->cc_help_descr =        N_("Default last eviction time difference"
                                    " - 1 second");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_priv_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_type =         bt_duration_sec;
    cmd->cc_exec_name =         "/nkn/nvsd/am/config/cache_ingest_last_eviction_time_diff";
    cmd->cc_exec_value =        "1";
    cmd->cc_exec_callback =     cli_nvsd_am_set_ingest_threshold;
    cmd->cc_revmap_type =       crt_none;
    //cmd->cc_revmap_order =      cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics cache-ingest hit-threshold *";
    cmd->cc_help_exp  =         N_("<number>");
    cmd->cc_help_exp_hint =     N_("Threshold number of hits");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/am/config/cache_ingest_hits_threshold";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_analytics;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics cache-ingest size-threshold";
    cmd->cc_help_descr =        N_("Configure Cache ingest size threshold "
                                    "parameter");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no analytics cache-ingest size-threshold";
    cmd->cc_help_descr =        N_("Set default Cache ingest size threshold "
                                    "parameter (0)");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/am/config/cache_ingest_size_threshold";
    cmd->cc_exec_type =         bt_uint32;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics cache-ingest size-threshold *";
    cmd->cc_help_exp  =         N_("<bytes>");
    cmd->cc_help_exp_hint =     N_("Ingest into tier-1 cache at or below this size");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/am/config/cache_ingest_size_threshold";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_analytics;
    CLI_CMD_REGISTER;
#endif

    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics cache-ingest queue-depth";
    cmd->cc_help_descr =        N_("Configure Cache ingest queue-depth "
                                    "parameter");
    cmd->cc_flags =             ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics cache-ingest queue-depth *";
    cmd->cc_help_exp  =         N_("<Depth of the queue>");
    cmd->cc_help_exp_hint =     N_("Queue-depth in units");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/am/config/cache_ingest_queue_depth";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint32;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics cache-ingest object-timeout";
    cmd->cc_help_descr =        N_("Configure Cache ingest object-timeout "
                                    "parameter");
    cmd->cc_flags =             ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics cache-ingest object-timeout *";
    cmd->cc_help_exp  =         N_("<Timeout value of the object>");
    cmd->cc_help_exp_hint =     N_("Timeout in seconds");
    cmd->cc_flags =             ccf_terminal| ccf_ignore_length ;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/am/config/cache_ingest_object_timeout";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint32;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics packing-policy";
    cmd->cc_help_descr =        N_("Configure packing-policy options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no analytics packing-policy";
    cmd->cc_help_descr =        N_("Configure packing-policy: namespace-based");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/am/config/cache_ingest_policy";
    cmd->cc_exec_value =        "2";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_analytics;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics packing-policy hybrid";
    cmd->cc_help_descr =        N_("Configure packing-policy: hybrid");
    cmd->cc_flags =             ccf_terminal; 
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/am/config/cache_ingest_policy";
    cmd->cc_exec_value =        "3";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_analytics;
    CLI_CMD_REGISTER;

    #if 0
    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics last-evict-time-diff *";
    cmd->cc_help_exp =          N_("<seconds>");
    cmd->cc_help_exp_hint =     N_("Time difference in seconds");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_priv_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_type =         bt_duration_sec;
    cmd->cc_exec_name =         "/nkn/nvsd/am/config/cache_ingest_last_eviction_time_diff";
    cmd->cc_exec_value =        "$1$";
    //cmd->cc_exec_callback =     cli_nvsd_am_set_ingest_threshold;
    cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;
    /*----------------------------------------------------------------------*/

    /*----------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =         "clear analytics";
    cmd->cc_help_descr =        N_("Clear analytics..");
    cmd->cc_flags =             ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "clear analytics longtail";
    cmd->cc_help_descr =        N_("Clear long tails");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_action_priv_curr;
    cmd->cc_exec_operation =    cxo_action;
    cmd->cc_exec_action_name =  "/nkn/nvsd/am/actions/longtail";
    cmd->cc_exec_name =         "longtail";
    cmd->cc_exec_type =         bt_int32;
    cmd->cc_exec_value =        "0";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "clear analytics longtail *";
    cmd->cc_help_exp =          N_("<number>");
    cmd->cc_help_exp_hint =     N_("longtail number");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_action_priv_curr;
    cmd->cc_exec_operation =    cxo_action;
    cmd->cc_exec_action_name =  "/nkn/nvsd/am/actions/longtail";
    cmd->cc_exec_name =         "longtail";
    cmd->cc_exec_type =         bt_int32;
    cmd->cc_exec_value =        "$1$";
    CLI_CMD_REGISTER;
          /* Bug# 4000 */
    err = cli_unregister_command("analytics");
    bail_error(err);

    err = cli_unregister_command("no analytics");
    bail_error(err);
#endif
    /*----------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =         "show analytics";
    cmd->cc_help_descr =        N_("Display Analytics manager");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_action_priv_curr;
    cmd->cc_exec_callback =     cli_nvsd_analytics_show;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show analytics rank";
    cmd->cc_help_descr =        N_("Display analytics rank");
    cmd->cc_flags =             ccf_terminal | ccf_hidden ;
    cmd->cc_capab_required =    ccp_action_basic_curr;
    cmd->cc_exec_callback =     cli_nvsd_am_show_rank;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show analytics rank *";
    cmd->cc_help_exp =          N_("<number>");
    cmd->cc_help_exp_hint =     N_("Rank Number");
    cmd->cc_flags =             ccf_terminal ;
    cmd->cc_capab_required =    ccp_action_basic_curr;
    cmd->cc_exec_callback =     cli_nvsd_am_show_rank;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show analytics tier";
    cmd->cc_help_descr =        N_("Display analytics tiers");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_action_basic_curr;
    cmd->cc_exec_callback =     cli_nvsd_am_show_tier;
    CLI_CMD_REGISTER;


    /*----------------------------------------------------------------------*/

    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics memory-limit";
    cmd->cc_help_descr =        N_("Set the memory-limit.");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics memory-limit auto";
    cmd->cc_help_descr =        N_("Set the memory-limit automatically using the inbuilt algorithm");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/am/config/memory_limit";
    cmd->cc_exec_value =        "0";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics memory-limit *";
    cmd->cc_help_exp  =         N_("<Mbytes>");
    cmd->cc_help_exp_hint =     N_("Set the memory-limit. Size in MiB.");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/am/config/memory_limit";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_analytics;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "analytics cache-ingest ignore-hotness-check";
    cmd->cc_help_descr =        N_("Set ignore hotness check during ingestion");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/am/config/cache_ingest_ignore_hotness_check";
    cmd->cc_exec_value =        "1";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_revmap_type =       crt_auto;
    cmd->cc_revmap_order =      cro_analytics;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no analytics cache-ingest ignore-hotness-check";
    cmd->cc_help_descr =        N_("Set default to enable hotness check during ingestion");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_reset;
    cmd->cc_exec_name =         "/nkn/nvsd/am/config/cache_ingest_ignore_hotness_check";
    cmd->cc_exec_type =         bt_uint32;
    CLI_CMD_REGISTER;

    /*----------------------------------------------------------------------*/

    err = cli_revmap_ignore_bindings_va(11,"/nkn/nvsd/am/config/cache_evict_aging_time",
	    "/nkn/nvsd/am/config/cache_ingest_hits_threshold",
	    "/nkn/nvsd/am/config/cache_ingest_last_eviction_time_diff",
	    "/nkn/nvsd/am/config/cache_promotion/hit_increment",
	    "/nkn/nvsd/am/config/cache_promotion/hit_threshold",
	    "/nkn/nvsd/am/config/cache_promotion/hotness_increment",
	    "/nkn/nvsd/am/config/cache_ingest_size_threshold",
	    "/nkn/nvsd/am/config/cache_promotion/hotness_threshold",
	    "/nkn/nvsd/am/config/cache_ingest_queue_depth",
	    "/nkn/nvsd/am/config/cache_ingest_object_timeout",
	    "/nkn/nvsd/am/config/cache_ingest_ignore_hotness_check");
    bail_error(err);
bail:
    return err;
}

int 
cli_nvsd_analytics_show(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    char *r = NULL;
    char *cache_promotion_node_name = NULL;
    tbool t_cache_promotion = false; 
    bn_binding *binding = NULL;
    uint32 policy_type = 0;
    uint32 mem_limit = 0;
    uint32 AM_auto_mem_limit = 0;
    uint32 cache_promotion_size_threshold = 0;
    uint32 code = 0;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    r = smprintf("/nkn/nvsd/am/config");
    bail_null(r);

    cli_printf(_(
                "Analytics Configuration:\n"));

    cache_promotion_node_name = smprintf("%s/cache_promotion_enabled",r);
    bail_null(cache_promotion_node_name);

    err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL, 
		    &t_cache_promotion, cache_promotion_node_name, NULL);
    bail_error(err);
    err = cli_printf_query(
		_("   Cache Promotion                      : %s\n"), 
		t_cache_promotion ? "Enabled" : "Disabled");
    bail_error(err);

    /* Cache Promotion Object size threshold show */
    err = mdc_get_binding(cli_mcc,&code,NULL,false,&binding,
		    "/nkn/nvsd/am/config/cache_promotion/size_threshold" ,NULL);

    if (binding ) {
	err = bn_binding_get_uint32(binding,ba_value,NULL,
				    &cache_promotion_size_threshold);
	bail_error(err);
	bn_binding_free(&binding);
    }

    if(cache_promotion_size_threshold)
	err = cli_printf_query(
		_("   Cache Promotion Size threshold       : %d bytes\n"),
		cache_promotion_size_threshold);
    else
	err = cli_printf_query(
		_("   Cache Promotion Size threshold       : 0 "
		"(All objects are promoted) \n"));
    bail_error(err);

#if 0
    cli_printf_query(_(
                "   Cache Promotion Hotness Threshold    : #%s/cache_promotion/hotness_threshold#\n"), r);
    cli_printf_query(_(
                "   Cache Ingest Size Threshold          : #%s/cache_ingest_size_threshold#\n"), r);
#endif
    err = mdc_get_binding(cli_mcc,&code,NULL,false,&binding,
		    "/nkn/nvsd/am/config/cache_ingest_policy" ,NULL);
    bail_error(err);                                                                                                                                                               
    if (binding ) {
	err = bn_binding_get_uint32(binding,ba_value,NULL,&policy_type);
	bail_error(err);
	bn_binding_free(&binding);
    }

    err = cli_printf(
                _("   Cache ingest policy                  : %s\n"),
                (policy_type == 3) ? "Hybrid" : "Namespace-Based");

    err = mdc_get_binding(cli_mcc,&code,NULL,false,&binding,
		    "/nkn/nvsd/am/config/memory_limit" ,NULL);
    bail_error(err);
    if (binding ) {
	err = bn_binding_get_uint32(binding,ba_value,NULL,&mem_limit);
	bail_error(err);
	bn_binding_free(&binding);
    }

    if(mem_limit == 0)
    {
        err = mdc_get_binding(cli_mcc,&code,NULL,false,&binding,
    		    "/nkn/nvsd/am/monitor/cal_am_mem_limit" ,NULL);
        bail_error(err);
        if (binding ) {
    	err = bn_binding_get_uint32(binding,ba_value,NULL,&AM_auto_mem_limit);
    	bail_error(err);
    	bn_binding_free(&binding);
        }

    	cli_printf(_("   Memory Limit                         : AUTO( %d MiB)\n"), AM_auto_mem_limit);
    }
    else
    {
    	cli_printf_query(_("   Memory Limit                           : #%s/memory_limit# MiB\n"), r);
    }
bail:
    safe_free(r);
    safe_free(cache_promotion_node_name);
    return err;
}



int
cli_nvsd_am_show_tier(
	void *data,
	const cli_command *cmd,
	const tstr_array *cmd_line,
	const tstr_array *params)
{
    int err = 0;
    uint32 ret_err = 0;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    err = mdc_send_action
            (cli_mcc, &ret_err, NULL, "/nkn/nvsd/am/actions/tier");
    bail_error(err);

    if (ret_err != 0) {
        cli_printf(_("Error in executing command.\n"));
        goto bail;
    } 

#if 0
    tmp_filename = (char*) file_out;
    // This is where the file needs to be dumped
    if (clish_paging_is_enabled()) {
        err = tstr_array_new_va_str(&argv, NULL, 9, prog_cli_pager_path,
                "-i", 
                "-M",
                "-e", 
                "+G", 
                "-X",
                "-d",
                "--",
                tmp_filename);
        bail_error(err);

        err = clish_run_program_fg(prog_cli_pager_path, argv, NULL,
                cli_nvsd_am_show_terminate, tmp_filename);
        bail_error(err);
    } 
    else {
        cat_prog = prog_zcat_path;
        err = tstr_array_new_va_str(&argv, NULL, 3, cat_prog, "--",
                tmp_filename);
        bail_error(err);

        err = clish_run_program_fg(cat_prog, argv, NULL,
                cli_nvsd_am_show_terminate, tmp_filename);
        bail_error(err);
    }
#endif /* 0 */


bail:
    return err;

}

int 
cli_nvsd_am_show_rank(
	void *data,
	const cli_command *cmd,
	const tstr_array *cmd_line,
	const tstr_array *params)
{
    int err = 0;
    const char *rank = NULL;
    uint32 ret_err = 0;
    tstr_array *argv = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    rank = tstr_array_get_str_quick(params, 0);

    if (NULL == rank) {
        // Default case
        rank = "-1";
    }

    err = mdc_send_action_with_bindings_str_va
            (cli_mcc, &ret_err, NULL, "/nkn/nvsd/am/actions/rank",
            1, "rank", bt_int32, rank);
    bail_error(err);

    if (ret_err != 0) {
        cli_printf(_("Error in executing command.\n"));
        goto bail;
    } 

#if 0
    tmp_filename = (char*) file_out;
    // This is where the file needs to be dumped
    if (clish_paging_is_enabled()) {
        err = tstr_array_new_va_str(&argv, NULL, 9, prog_cli_pager_path,
                "-i", 
                "-M",
                "-e", 
                "+G", 
                "-X",
                "-d",
                "--",
                tmp_filename);
        bail_error(err);

        err = clish_run_program_fg(prog_cli_pager_path, argv, NULL,
                cli_nvsd_am_show_terminate, tmp_filename);
        bail_error(err);
    } 
    else {
        cat_prog = prog_zcat_path;
        err = tstr_array_new_va_str(&argv, NULL, 3, cat_prog, "--",
                tmp_filename);
        bail_error(err);

        err = clish_run_program_fg(cat_prog, argv, NULL,
                cli_nvsd_am_show_terminate, tmp_filename);
        bail_error(err);
    }
#endif /* 0 */

bail:
    tstr_array_free(&argv);
    return err;
}

