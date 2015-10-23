/* File name:    cli_nkn_logexport.c
 * Date:        2012-03-26
 * Author:      Madhumitha Baskaran
 *
 *
 */


#include <climod_common.h>
#include "file_utils.h"
#include "nkn_defs.h"

static const char file_logexport_path[] = "/var/logexport/chroot/home/LogExport";


int
cli_logexport_show(void *data, const cli_command *cmd,
                   const tstr_array *cmd_line,
                   const tstr_array *params);

int
cli_nkn_logexport_cmds_init(const lc_dso_info *info,
			    const cli_module_context *context);
int
cli_nkn_logexport_cmds_init(const lc_dso_info *info,
		            const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;
    UNUSED_ARGUMENT(info);

    if (context->cmc_mgmt_avail == false) {
        goto bail;
    }

    CLI_CMD_NEW;
    cmd->cc_words_str      = "show logging export";
    cmd->cc_flags          = ccf_terminal|ccf_acl_inheritable|ccf_shell_required;
    cli_acl_add(cmd,                tacl_sbm_logcfg);
    cli_acl_opers_exec(cmd,         tao_query);
    cli_acl_set_modes(cmd,          cms_enable);
    cmd->cc_capab_required = ccp_query_basic_curr;
    cmd->cc_help_descr	   = N_("Display configuration for exporting "
				"logs from the log export directory ");
    cmd->cc_exec_callback  = cli_logexport_show;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "logging export";
    cmd->cc_help_descr     = N_("Parameters for exporting log files "
				"from the log export directory");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "logging export purge";
    cmd->cc_help_descr     = N_("Parameters for removing files "
				"exported to the log export directory");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "logging export purge criteria";
    cmd->cc_help_descr     = N_("Specify the purge criteria of files under "
				"logexport directory");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "logging export purge criteria frequency";
    cmd->cc_help_descr     = N_("Interval for removing log files from the "
				"logexport directory");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "logging export purge criteria frequency *";
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_set_basic_curr;
    cmd->cc_help_exp_hint  = N_("<Purge frequency <1-24> in hrs "
				"0 disable purge>");
    cmd->cc_help_exp       = N_("Frequency ");
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_name      = "/nkn/logexport/config/purge/criteria/interval";
    cmd->cc_exec_type      = bt_uint32;
    cmd->cc_exec_value     = "$1$";
    cmd->cc_revmap_type    = crt_auto;
    cmd->cc_revmap_order   = cro_logging;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "logging export purge criteria size-pct";
    cmd->cc_help_descr     = N_("Percentage of partition size for"
                            " removing files in the log export directory");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "logging export purge criteria size-pct *";
    cmd->cc_flags          = ccf_terminal ;
    cmd->cc_capab_required = ccp_set_basic_curr;
    cmd->cc_exec_operation = cxo_set;
    cmd->cc_exec_name      = "/nkn/logexport/config/purge/criteria/threshold_size_thou_pct";
    cmd->cc_exec_type      = bt_uint32;
    cmd->cc_help_exp       = N_("<percentage>");
    cmd->cc_help_exp_hint  = N_("Percentage of log export partition size"
				"(truncated to three digits after decimal point)");
    cmd->cc_exec_value     = "$1$";
    cmd->cc_exec_value_multiplier = 1000;
    cmd->cc_revmap_type    = crt_auto;
    cmd->cc_revmap_order   = cro_logging;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str      = "logging export purge force";
    cmd->cc_help_descr     = N_("Remove all the files under "
				"the log export directory");
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_set_priv_curr;
    cli_acl_set_modes(cmd,          cms_enable);
    cmd->cc_exec_operation = cxo_action;
    cmd->cc_exec_action_name = "/nkn/logexport/actions/force_purge";
    CLI_CMD_REGISTER;

bail:
    return(err);
}


int
cli_logexport_show(void *data, const cli_command *cmd,
                   const tstr_array *cmd_line,
                   const tstr_array *params)
{
    int err = 0;
    bn_binding_array *bindings = NULL;
    const bn_binding *binding = NULL;
    const bn_binding *threshold_size_binding = NULL;
    const bn_binding *threshold_interval = NULL;
    uint64 threshold_size = 0, part_size = 0;
    uint32 threshold_size_thou_pct = 0;
    uint32 interval = 0;
    tstring *ts_part_size = NULL;

    err = mdc_get_binding_children
	  (cli_mcc,
	  NULL, NULL, false, &bindings, false, true, "/nkn/logexport");
    bail_error(err);

    err = bn_binding_array_get_binding_by_name
          (bindings,
	  "/nkn/logexport/config/purge/criteria/threshold_size_thou_pct",
	  &threshold_size_binding);
    bail_error(err);

    err = bn_binding_get_uint32
          (threshold_size_binding, ba_value, NULL, &threshold_size_thou_pct);
    bail_error(err);

    /*
     * Technically we should get this value from /system/fs/mount/...
     * but either hardwiring a reference to /var or determining which
     * of the partitions underneath there goes with file_syslog_path
     * both seem distasteful.
     */
    err = lf_read_file_tstr("/config/nkn/log-export-size", 32, &ts_part_size);
    bail_error(err);

    if (!ts_part_size) {
	part_size = 0;
    } else
	part_size = strtoll(ts_str(ts_part_size), NULL, 10);
    // part_size is in KBytes,convert to bytes
    threshold_size = threshold_size_thou_pct *( part_size * 1024) / 1000 / 100;

    err = cli_printf
	(_("Logexport purge size threshold-pct: %.3f%% of partition "
	   "(%.2lf MiBytes)\n"),
	 (float)threshold_size_thou_pct / 1000,
	 (double)threshold_size / 1000 / 1000);
    bail_error(err);

    err = bn_binding_array_get_binding_by_name
          (bindings,
	  "/nkn/logexport/config/purge/criteria/interval",
	  &threshold_interval);
    bail_error(err);

    err = bn_binding_get_uint32
	  (threshold_interval, ba_value, NULL, &interval);
    bail_error(err);

    err = cli_printf(_("Logexport purge interval: %d hr\n"),
	              interval );
    bail_error(err);

bail:
    bn_binding_array_free(&bindings);
    ts_free(&ts_part_size);
    return err;
}
