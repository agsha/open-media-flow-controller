#include "common.h"
#include "climod_common.h"
#include "clish_module.h"
#include "nkn_defs.h"
#include "nkn_mgmt_defs.h"


int
cli_config_files_cmds_init(
	const lc_dso_info *info,
	const cli_module_context *context);

int
cli_config_files_cmds_init(
	const lc_dso_info *info,
	const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    if (context->cmc_mgmt_avail == false) {
	goto bail;
    }


    CLI_CMD_NEW;
    cmd->cc_words_str =         "configuration files";
    cmd->cc_flags =		ccf_hidden;
    cmd->cc_help_descr =        N_("Manipulate Internal configuration files");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "configuration files *";
    cmd->cc_help_exp =          N_("<file-name>");
    cmd->cc_help_exp_hint =     N_("File-name to be modified");
    cmd->cc_comp_type =         cct_matching_names;
    cmd->cc_comp_pattern =      "/nkn/cfg-files/config/files/*";
    cmd->cc_help_use_comp =         true;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "configuration files * param";
    cmd->cc_help_descr =         N_("Add/Modify a paramter");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "configuration files * param *";
    cmd->cc_help_exp =          N_("<param-name>");
    cmd->cc_help_exp_hint =     N_("Parameter to be add/modified");
    cmd->cc_comp_type =         cct_matching_names;
    cmd->cc_comp_pattern =      "/nkn/cfg-files/config/files/$1$/params/*";
    cmd->cc_help_use_comp =         true;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "configuration files * param * value";
    cmd->cc_help_descr =         N_("Change the value the paramter");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "configuration files * param * value *";
    cmd->cc_capab_required =	ccp_set_rstr_curr;
    cmd->cc_help_exp =          N_("<value>");
    cmd->cc_help_exp_hint =     N_("Value of parameter to be add/modified");
    cmd->cc_flags =		ccf_terminal;
    cmd->cc_exec_operation =	cxo_set;
    cmd->cc_exec_name =         "/nkn/cfg-files/config/files/$1$/params/$2$/value";
    cmd->cc_exec_value =        "$3$";
    cmd->cc_exec_type =         bt_string;
    cmd->cc_revmap_type =	crt_auto;
    cmd->cc_revmap_order =	cro_config_files;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no configuration files";
    cmd->cc_help_descr =         N_("Remove a internal configuration file");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no configuration files * ";
    cmd->cc_capab_required =	ccp_set_rstr_curr;
    cmd->cc_help_exp =          N_("<file-name>");
    cmd->cc_help_exp_hint =     N_("File name to be removed");
    cmd->cc_comp_type =         cct_matching_names;
    cmd->cc_comp_pattern =      "/nkn/cfg-files/config/files/*";
    cmd->cc_help_use_comp =         true;
    cmd->cc_flags =		ccf_terminal;
    cmd->cc_exec_operation =	cxo_delete;
    cmd->cc_exec_name =         "/nkn/cfg-files/config/files/$1$";
    cmd->cc_revmap_type =	crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no configuration files * param";
    cmd->cc_help_descr =         N_("Remove a parameter from internal configuration file");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no configuration files * param *";
    cmd->cc_capab_required =	ccp_set_rstr_curr;
    cmd->cc_help_exp =          N_("<param-name>");
    cmd->cc_help_exp_hint =     N_("Parameter to be removed");
    cmd->cc_comp_type =         cct_matching_names;
    cmd->cc_comp_pattern =      "/nkn/cfg-files/config/files/$1$/params/*";
    cmd->cc_help_use_comp =         true;
    cmd->cc_flags =		ccf_terminal;
    cmd->cc_exec_operation =	cxo_delete;
    cmd->cc_exec_name =         "/nkn/cfg-files/config/files/$1$/params/$2$";
    cmd->cc_revmap_type =	crt_none;
    CLI_CMD_REGISTER;

    err = cli_revmap_ignore_bindings("/nkn/cfg-files/config/files/*");
    bail_error(err);
    err = cli_revmap_ignore_bindings("/nkn/cfg-files/config/files/*/params/*");
    bail_error(err);


bail:
    return err;
}
