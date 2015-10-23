/*
 *
 * Filename:  cli_watchdog_cmds.c
 * Date:      2011/08/21
 * Author:    Thilak Raj S
 *
 * (C) Copyright 2011 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

#include <dso.h>
#include <env_utils.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "tstring.h"
#include "random.h"
#include "cli_module.h"
#include "clish_module.h"
#include "file_utils.h"
#include "proc_utils.h"
#include "libnkncli.h"
#include <climod_common.h>
#include "nkn_defs.h"

static int
cli_wd_show(void *data, const cli_command *cmd,
            const tstr_array *cmd_line, const tstr_array *params);
int
cli_watchdog_cmds_init(const lc_dso_info *info,
                     const cli_module_context *context);

static int
cli_reg_nkn_watchdog_cmds(const lc_dso_info *info,
                    const cli_module_context *context);

static int
cli_wd_alarm_show(void *data, const cli_command *cmd,
            const tstr_array *cmd_line, const tstr_array *params);

static const char*
cli_get_wd_action(const char* action);

#define PROBE_CHECK(name, func_name, threshold, freq, is_hard_alarm, enabled, gdb_cmds)                    \
    CLI_CMD_NEW;   \
    cmd->cc_words_str      = "watch-dog alarm "name; \
   cmd->cc_help_descr      = N_("Set watch-dog "name" alarm's configuration"); \
    CLI_CMD_REGISTER;   \
 \
    CLI_CMD_NEW; \
    cmd->cc_words_str      = "watch-dog alarm "name" enable"; \
    cmd->cc_flags          = ccf_terminal; \
    cmd->cc_help_descr      = N_("Enable watch-dog "name" alarm"); \
    cmd->cc_capab_required = ccp_set_basic_curr; \
    cmd->cc_exec_operation = cxo_reset; \
    cmd->cc_exec_name      = "/nkn/watch_dog/config/nvsd/alarms/"name"/enable"; \
    cmd->cc_revmap_type    = crt_none; \
    CLI_CMD_REGISTER; \
\
    CLI_CMD_NEW; \
    cmd->cc_words_str      = "watch-dog alarm "name" disable"; \
    cmd->cc_flags          = ccf_terminal; \
    cmd->cc_help_descr      = N_("Disable watch-dog "name" alarm"); \
    cmd->cc_capab_required = ccp_set_basic_curr; \
    cmd->cc_exec_operation = cxo_set; \
    cmd->cc_exec_name      = "/nkn/watch_dog/config/nvsd/alarms/"name"/enable"; \
    cmd->cc_exec_type      = bt_bool; \
    cmd->cc_exec_value     = "false"; \
    cmd->cc_revmap_type =    crt_auto; \
    cmd->cc_revmap_order =   cro_watchdog; \
    CLI_CMD_REGISTER; \
\
    CLI_CMD_NEW;   \
    cmd->cc_words_str      = "watch-dog alarm "name" threshold"; \
    cmd->cc_help_descr      = N_("Set watch-dog "name" alarm's threshold"); \
    CLI_CMD_REGISTER;   \
 \
    CLI_CMD_NEW; \
    cmd->cc_words_str      = "watch-dog alarm "name" threshold *"; \
    cmd->cc_flags          = ccf_terminal; \
    cmd->cc_help_exp       = N_("<threshold>"); \
    cmd->cc_help_exp_hint  = N_("Set watch-dog "name" alarm's threshold"); \
    cmd->cc_capab_required = ccp_set_basic_curr; \
    cmd->cc_exec_operation = cxo_set; \
    cmd->cc_exec_name      = "/nkn/watch_dog/config/nvsd/alarms/"name"/threshold"; \
    cmd->cc_exec_type      = bt_uint32; \
    cmd->cc_exec_value     = "$1$"; \
    cmd->cc_revmap_type =    crt_auto; \
    cmd->cc_revmap_order =   cro_watchdog; \
    CLI_CMD_REGISTER; \
\
    CLI_CMD_NEW;   \
    cmd->cc_words_str      = "watch-dog alarm "name" poll-frequency"; \
    cmd->cc_help_descr      = N_("Set watch-dog "name" alarm's poll-frequency"); \
    CLI_CMD_REGISTER;   \
\
    CLI_CMD_NEW; \
    cmd->cc_words_str      = "watch-dog alarm "name" poll-frequency *"; \
    cmd->cc_flags          = ccf_terminal; \
    cmd->cc_exec_value_multiplier = 1000; \
    cmd->cc_help_exp       = N_("<poll-freq>"); \
    cmd->cc_help_exp_hint  = N_("Set watch-dog "name" alarms poll-frequency"); \
    cmd->cc_capab_required = ccp_set_basic_curr; \
    cmd->cc_exec_operation = cxo_set; \
    cmd->cc_exec_name      = "/nkn/watch_dog/config/nvsd/alarms/"name"/poll_freq"; \
    cmd->cc_exec_type      = bt_uint32; \
    cmd->cc_exec_value     = "$1$"; \
    cmd->cc_revmap_type =    crt_auto; \
    cmd->cc_revmap_order =   cro_watchdog; \
    CLI_CMD_REGISTER; \
\
    CLI_CMD_NEW;   \
    cmd->cc_words_str      = "watch-dog alarm "name" action"; \
   cmd->cc_help_descr      = N_("Set an action in an event of a watchdog "name" alarm."); \
    CLI_CMD_REGISTER;   \
\
    CLI_CMD_NEW; \
    cmd->cc_words_str      = "watch-dog alarm "name" action restart"; \
    cmd->cc_flags          = ccf_terminal; \
    cmd->cc_help_descr      = N_("Configure watch-dog to restart process in an event of the "name" alarm."); \
    cmd->cc_capab_required = ccp_set_basic_curr; \
    cmd->cc_exec_operation = cxo_reset; \
    cmd->cc_exec_name      = "/nkn/watch_dog/config/nvsd/alarms/"name"/action"; \
    cmd->cc_revmap_type    = crt_none; \
     CLI_CMD_REGISTER;   \
\
    CLI_CMD_NEW; \
    cmd->cc_words_str      = "watch-dog alarm "name" action log"; \
    cmd->cc_flags          = ccf_terminal; \
    cmd->cc_help_descr      = N_("Configure watch-dog to only log about "name" alarm."); \
    cmd->cc_capab_required = ccp_set_basic_curr; \
    cmd->cc_exec_operation = cxo_set; \
    cmd->cc_exec_name      = "/nkn/watch_dog/config/nvsd/alarms/"name"/action"; \
    cmd->cc_exec_type      = bt_uint64; \
    cmd->cc_exec_value     = "2"; \
   cmd->cc_revmap_type =    crt_auto; \
    cmd->cc_revmap_order =   cro_watchdog; \
    CLI_CMD_REGISTER; \
\
    CLI_CMD_NEW; \
    cmd->cc_words_str      = "show watch-dog alarm "name""; \
    cmd->cc_flags          = ccf_terminal; \
    cmd->cc_exec_callback  = cli_wd_alarm_show; \
    cmd->cc_capab_required = ccp_query_basic_curr; \
    cmd->cc_help_descr     = N_("Display watch-dog "name" alarm configuration."); \
    cmd->cc_revmap_type    = crt_none; \
    CLI_CMD_REGISTER;

    const char* wd_revmap_nds[] = {
				    "/nkn/watch_dog/config/**",
				    "/nkn/nvsd/watch_dog/config/restart"};



int
cli_watchdog_cmds_init(const lc_dso_info *info,
                     const cli_module_context *context)
{
	int err = 0;

	if (context->cmc_mgmt_avail == false) {
		goto bail;
	}


	err = cli_reg_nkn_watchdog_cmds(info, context);
	bail_error(err);

bail:
	return (err);
}

static const char*
cli_get_wd_action(const char* action)
{

    if(strcmp("1", action) == 0)
	return "Restart";
    else if (strcmp("2", action) == 0)
	return "Log";
    else
	return NULL;

}


static int
cli_wd_alarm_show(void *data, const cli_command *cmd,
            const tstr_array *cmd_line, const tstr_array *params)
{
    int err = 0;
    tstring *wd_enable = NULL;
    tstring *wd_action = NULL;
    tstring *wd_thresh = NULL;
    tstring *wd_poll = NULL;
    uint32_t wd_poll_freq = 0;
    const char *alarm_name = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    alarm_name = tstr_array_get_str_quick(cmd_line, 3);
    bail_null(alarm_name);

    cli_printf("Watch-Dog Alarm Config:\n");
    /* Check based on the node */


    err = mdc_get_binding_tstr_fmt(cli_mcc,
	    NULL, NULL, NULL, &wd_enable, NULL,
	    "/nkn/watch_dog/config/nvsd/alarms/%s/enable", alarm_name);
    bail_error_null(err, wd_enable);

    err = mdc_get_binding_tstr_fmt(cli_mcc,
	    NULL, NULL, NULL, &wd_action, NULL,
	    "/nkn/watch_dog/config/nvsd/alarms/%s/action", alarm_name);
    bail_error_null(err, wd_action);

    err = mdc_get_binding_tstr_fmt(cli_mcc,
	    NULL, NULL, NULL, &wd_thresh, NULL,
	    "/nkn/watch_dog/config/nvsd/alarms/%s/threshold", alarm_name);
    bail_error_null(err, wd_thresh);


    err = mdc_get_binding_tstr_fmt(cli_mcc,
		    NULL, NULL, NULL, &wd_poll, NULL,
		    "/nkn/watch_dog/config/nvsd/alarms/%s/poll_freq", alarm_name);
    bail_error_null(err, wd_poll);

    wd_poll_freq = strtoul(ts_str(wd_poll), NULL, 10);

    err = cli_printf("  Watch-dog Alarm Enabled           : %s\n", (strcmp("true", ts_str(wd_enable)) == 0)? "Enabled": "Disabled");
    bail_error(err);

    err = cli_printf("  Watch-dog Alarm Action            : %s\n", cli_get_wd_action(ts_str(wd_action)));
    bail_error(err);

    err = cli_printf("  Watch-dog Alarm Threshold         : %s\n", ts_str(wd_thresh));
    bail_error(err);

    err = cli_printf("  Watch-dog Alarm Poll-Frequency    : %u(secs)\n", (wd_poll_freq/1000));
    bail_error(err);

bail:
    ts_free(&wd_enable);
    ts_free(&wd_poll);
    ts_free(&wd_thresh);
    ts_free(&wd_action);

    return err;

}

static int cli_reg_nkn_watchdog_cmds(const lc_dso_info *info,
                    const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    CLI_CMD_NEW;
    cmd->cc_words_str      = "watch-dog";
    cmd->cc_help_descr     = N_("Configure parameters for service availablity monitoring");
/*    cmd->cc_flags          = ccf_hidden; bug# 8032 */
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "watch-dog threshold";
    cmd->cc_help_descr     = N_("Threshold determines the number of continuous failures needed for watchdog to trigger a restart");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 	   = "watch-dog threshold *";
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_set_basic_curr;
    cmd->cc_help_exp       = N_("<count>");
    cmd->cc_help_exp_hint  = N_("Specify the threshold count");
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_name      = "/pm/process/nvsd/liveness/hung_count";
    cmd->cc_exec_type      = bt_uint32;
    cmd->cc_exec_value     = "$1$";
    cmd->cc_revmap_type    = crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "watch-dog  grace-period";
    cmd->cc_help_descr     = N_("Specify the duration after which the delivery engine should be monitored after a restart");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "watch-dog  grace-period *";
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_set_basic_curr;
    cmd->cc_help_exp       = N_("<secs>");
    cmd->cc_help_exp_hint  = N_("Specify the grace-period duration in seconds");
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_name      = "/pm/failure/liveness/grace_period";
    cmd->cc_exec_type      = bt_duration_sec;
    cmd->cc_exec_value     = "$1$";
    cmd->cc_revmap_type    = crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "watch-dog  poll-freq";
    cmd->cc_help_descr     = N_("Specify the interval at which the delivery engine should be monitored");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "watch-dog  poll-freq *";
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_set_basic_curr;
    cmd->cc_help_exp       = N_("<secs>");
    cmd->cc_help_exp_hint  = N_("Specify the polling-frequency in seconds");
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_name      = "/pm/failure/liveness/poll_freq";
    cmd->cc_exec_type      = bt_duration_sec;
    cmd->cc_exec_value     = "$1$";
    cmd->cc_revmap_type    = crt_none;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str      = "watch-dog  restart";
    cmd->cc_help_descr     = N_("Enable/Disable restart of delivery engine on watchdog detecting a failure");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "watch-dog restart enable";
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_help_descr      = N_("Enable restart of delivery engine on watchdog probe failure");
    cmd->cc_capab_required = ccp_set_basic_curr;
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_name      = "/nkn/watch_dog/config/nvsd/restart";
    cmd->cc_exec_type      = bt_bool;
    cmd->cc_exec_value     = "true";
    cmd->cc_revmap_type    = crt_auto;
    cmd->cc_revmap_order =   cro_watchdog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "watch-dog  debug";
    cmd->cc_flags          = ccf_hidden;
    cmd->cc_help_descr     = N_("Enable watch-dog debug settings");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "watch-dog debug enable";
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_help_descr      = N_("");
    cmd->cc_capab_required = ccp_set_basic_curr;
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_name      = "/nkn/watch_dog/config/debug/enable";
    cmd->cc_exec_type      = bt_bool;
    cmd->cc_exec_value     = "true";
    cmd->cc_revmap_type    = crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "watch-dog  memory";
    cmd->cc_flags          = ccf_hidden;
    cmd->cc_help_descr     = N_("Tune memory related settings for a monitored process");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "watch-dog  memory nvsd";
    cmd->cc_help_descr     = N_("Tune memory related settings for nvsd");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "watch-dog  memory nvsd soft-limit";
    cmd->cc_help_descr     = N_("Set the memory limit for which a warning should be thrown");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "watch-dog memory nvsd soft-limit *";
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_help_exp       = N_("<MB>");
    cmd->cc_help_exp_hint  = N_("Set the soft-limit size");
    cmd->cc_capab_required = ccp_set_basic_curr;
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_name      = "/nkn/watch_dog/config/nvsd/mem/softlimit";
    cmd->cc_exec_type      = bt_uint32;
    cmd->cc_exec_value     = "$1$";
    cmd->cc_revmap_type    = crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "watch-dog  memory nvsd hard-limit";
    cmd->cc_help_descr     = N_("set the memory limit for which a watchdog alarm is raised");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "watch-dog memory nvsd hard-limit *";
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_help_exp       = N_("<MB>");
    cmd->cc_help_exp_hint  = N_("Set the hard-limit size");
    cmd->cc_capab_required = ccp_set_basic_curr;
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_name      = "/nkn/watch_dog/config/nvsd/mem/hardlimit";
    cmd->cc_exec_type      = bt_uint32;
    cmd->cc_exec_value     = "$1$";
    cmd->cc_revmap_type    = crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "watch-dog debug disable";
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_help_descr      = N_("Set watch-dog to create core");
    cmd->cc_capab_required = ccp_set_basic_curr;
    cmd->cc_exec_operation = cxo_reset;
    cmd->cc_exec_name      = "/nkn/watch_dog/config/debug/enable";
    cmd->cc_exec_type      = bt_bool;
    cmd->cc_revmap_type    = crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "watch-dog alarm";
    cmd->cc_help_descr     = N_("Specify the monitoring parameters for a watch-dog alarm");
    CLI_CMD_REGISTER;

#define PROBE_CHECKS
#include "watchdog.inc"
#undef PROBE_CHECKS

    CLI_CMD_NEW;
    cmd->cc_words_str      = "no watch-dog";
    cmd->cc_help_descr     = N_("Restore default watch-dog settings");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "no watch-dog restart";
    cmd->cc_help_descr     = N_("Restore default watch-dog restart settings");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "no watch-dog restart enable";
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_help_descr      = N_("Do not restart delivery engine on watchdog probe failure");
    cmd->cc_capab_required = ccp_set_basic_curr;
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_name      = "/nkn/watch_dog/config/nvsd/restart";
    cmd->cc_exec_type      = bt_bool;
    cmd->cc_exec_value     = "false";
    cmd->cc_revmap_type    = crt_auto;
    cmd->cc_revmap_order   = cro_watchdog;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show watch-dog";
    cmd->cc_help_descr =        N_("Display watch-dog configuration");
    cmd->cc_flags =             ccf_terminal ;
    cmd->cc_capab_required =    ccp_query_basic_curr;
    cmd->cc_exec_callback =     cli_wd_show;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "show watch-dog alarm";
    cmd->cc_help_descr =        N_("Display watch-dog alarm configuration");
    CLI_CMD_REGISTER;
    err = cli_revmap_ignore_bindings_arr(wd_revmap_nds);
    bail_error(err);


bail:
    return err;
}

static int
cli_wd_show(void *data, const cli_command *cmd,
            const tstr_array *cmd_line, const tstr_array *params)
{

    int err = 0;
    
    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    cli_printf("Watch-Dog Config:\n");
    /* Check based on the node */
    err = cli_printf_query
	(_(
	   "Watch-dog Enabled   : "
	   "#/pm/process/nvsd/liveness/enable#\n"

	   "Restart Enabled     : "
	   "#/nkn/watch_dog/config/nvsd/restart#\n"

	   "Threshold           : "
	   "#/pm/process/nvsd/liveness/hung_count#\n"

	   "Polling Interval    : "
	   "#/pm/failure/liveness/poll_freq# sec\n"

	   "Polling Grace Period: "
	   "#/pm/failure/liveness/grace_period# sec\n")
	);
    bail_error(err);
bail:
    return err;
}
