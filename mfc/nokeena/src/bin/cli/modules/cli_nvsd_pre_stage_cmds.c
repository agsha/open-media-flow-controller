/*
 * Filename :   cli_nvsd_pre_stage_cmds.c
 * Created On:  2008/12/22
 * Author :     Dhruva Narasimhan
 *
 * (C) Copyright 2008, Nokeena Networks Inc.
 * All rights reserved.
 *
 */

/*----------------------------------------------------------------------------
 * HEADER FILES
 *---------------------------------------------------------------------------*/
#include "common.h"
#include "climod_common.h"
#include "tcrypt.h"
#include "nkn_defs.h"



/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
int
cli_nvsd_pre_stage_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);


int
cli_pre_stage_new_user(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int 
cli_pre_stage_enter_mode(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int 
cli_pre_stage_user_disable(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nvsd_pre_stage_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * pre-stage ftp";
    cmd->cc_help_descr =        N_("Configure FTP options");
    cmd->cc_flags =             ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * pre-stage ftp user";
    cmd->cc_help_descr =        N_("Configure FTP user");
    cmd->cc_flags =             ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * pre-stage ftp file-age";
    cmd->cc_help_descr =        N_("Configure time of holding FTP staging area before cleanup");
    cmd->cc_flags =             ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * pre-stage ftp file-age *";
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_help_exp =          N_("<time>");
    cmd->cc_help_exp_hint =     N_("holding time (mins)");
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/namespace/$1$/prestage/file_age";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_uint16;
    cmd->cc_revmap_type = 	crt_auto;
    cmd->cc_revmap_order = 	cro_namespace;
    cmd->cc_revmap_suborder = 	5;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * pre-stage ftp check-time";
    cmd->cc_help_descr =        N_("Configure time for checking FTP staging area for cleanup");
    cmd->cc_flags =             ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * pre-stage ftp check-time *";
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_help_exp =          N_("<time>");
    cmd->cc_help_exp_hint =     N_("checking time (mins)");
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/namespace/$1$/prestage/cron_time";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_uint16;
    cmd->cc_revmap_order = 	cro_namespace;
    cmd->cc_revmap_type = 	crt_auto;
    cmd->cc_revmap_suborder = 	5;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * pre-stage ftp user *";
    cmd->cc_help_exp =          N_("<username>");
    cmd->cc_help_exp_hint =     N_("Create a user with this username");
    cmd->cc_comp_type =         cct_matching_names;
    cmd->cc_comp_pattern =      "/nkn/nvsd/namespace/$1$/prestage/user/*";
    cmd->cc_help_use_comp =     true;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * pre-stage ftp user * password";
    cmd->cc_help_descr =        N_("Set password authentication mechanism");
    cmd->cc_flags =             ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "namespace * pre-stage ftp user * password *";
    cmd->cc_help_exp =          N_("<password>");
    cmd->cc_help_exp_hint =     N_("Password");
    cmd->cc_flags =             ccf_terminal | ccf_sensitive_param;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    //cmd->cc_exec_operation =    cxo_set;
    //cmd->cc_exec_name =         "/nkn/nvsd/namespace/$1$/prestage/user/$2$/password";
    //cmd->cc_exec_value =        "$3$";
    //cmd->cc_exec_type =         bt_string;
    cmd->cc_exec_callback =     cli_pre_stage_new_user;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * pre-stage";
    cmd->cc_help_descr =        N_("Disable/Clear prestage user accounts");
    cmd->cc_flags =             ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * pre-stage ftp";
    cmd->cc_help_descr =        N_("Disable/Clear prestage FTP user account");
    cmd->cc_flags =             ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * pre-stage ftp user";
    cmd->cc_help_descr =        N_("Disable pre-stage FTP user account");
    cmd->cc_flags =             ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no namespace * pre-stage ftp user *";
    cmd->cc_help_exp =          N_("<username>");
    cmd->cc_comp_type =         cct_matching_names;
    cmd->cc_comp_pattern =      "/nkn/nvsd/namespace/$1$/prestage/user/*";
    cmd->cc_help_use_comp =     true;
    cmd->cc_help_exp_hint =     N_("User name");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_exec_callback =     cli_pre_stage_user_disable;
    cmd->cc_capab_required =    ccp_set_rstr_curr;

    //cmd->cc_exec_operation =    cxo_set;
    //cmd->cc_exec_name =         "/nkn/nvsd/namespace/$1$/prestage/user/$2$/password";
    //cmd->cc_exec_value =        "*";
    //cmd->cc_exec_type =         bt_string;
    CLI_CMD_REGISTER;


    err = cli_revmap_ignore_bindings("/nkn/nvsd/pre_stage/ftpd/**");
    bail_error(err);
    
    err = cli_revmap_hide_bindings("/nkn/nvsd/namespace/*/prestage/**");
    bail_error(err);
        
    err = cli_revmap_hide_bindings("/pm/process/ftpd/**");
    bail_error(err);


bail:
    return err;
}

int 
cli_pre_stage_user_disable(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *ns = NULL, *user = NULL;
    char *username = NULL;
    char *bn_name = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    ns = tstr_array_get_str_quick(params, 0);
    bail_null(ns);

    user = tstr_array_get_str_quick(params, 1);
    bail_null(user);

    username = smprintf("%s_ftpuser", ns);
    bail_null(username);

    if (strcmp(user, username) != 0) {
        cli_printf_error(_("Invalid pre-stage username : '%s'"), user);
        goto bail;
    }

    bn_name = smprintf("/nkn/nvsd/namespace/%s/prestage/user/%s_ftpuser/password", 
            ns, ns);
    bail_null(bn_name);

    // Set the node to "*";
    err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_modify, bt_string, "*",
            bn_name);
    bail_error(err);


bail:
    safe_free(username);
    safe_free(bn_name);
    return err;
}



int
cli_pre_stage_new_user(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    const char *user = NULL;
    const char *passwd = NULL;
    const char *ns = NULL;
    char *crypt_passwd = NULL;
    tbool valid = false;
    char *bn_user = NULL, *bn_passwd = NULL;
    char *username = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    ns = tstr_array_get_str_quick(params, 0);
    bail_null(ns);
    
    user = tstr_array_get_str_quick(params, 1);
    bail_null(user);
    
    passwd = tstr_array_get_str_quick(params, 2);
    bail_null(passwd);

    //
    // fix bug - 961
    //  Never allow create of the username - this is autocreated when the 
    //  namespace is created.
    //
    // Guard against users that enter an invalid userid
    //
    username = smprintf("%s_ftpuser", ns);
    bail_null(username);

    bn_user = smprintf("/nkn/nvsd/namespace/%s/prestage/user/%s_ftpuser", ns, ns);
    bail_null(bn_user);

    if (strcmp(user, username) != 0) {
        cli_printf_error(_("Invalid pre-stage username : '%s'"), user);
        goto bail;
    }

    bn_passwd = smprintf("/nkn/nvsd/namespace/%s/prestage/user/%s/password", ns, user);
    bail_null(bn_passwd);

    err = lc_password_validate(passwd, &valid);
    bail_error(err);

    if (!valid) {
        err = cli_printf_error("%s\n", lc_get_password_invalid_error_msg());
        bail_error(err);
        goto bail;
    }

    err = ltc_encrypt_password(passwd, lpa_default, &crypt_passwd);
    bail_error_null(err, crypt_passwd);

#if 0

    err = mdc_create_binding(cli_mcc, NULL, NULL, 
            bt_string, user, bn_user);
    bail_error(err);
#endif

    err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_modify,
            bt_string, crypt_passwd, bn_passwd);
    bail_error(err);

bail:
    safe_free(crypt_passwd);
    safe_free(bn_user);
    safe_free(bn_passwd);
    safe_free(username);
    return err;
}


int 
cli_pre_stage_enter_mode(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(params);

    if (cli_prefix_modes_is_enabled()) {
        err = cli_prefix_enter_mode(cmd, cmd_line);
        bail_error(err);
    }
bail:
    return err;
}
