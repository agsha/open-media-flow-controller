/*
 * Filename :   cli_geodbd_cmds.c
 * Date     :   23 May 2011
 * Author   :   Kiran Desai
 *
 * (C) Copyright 2011 Juniper Networks, Inc.
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
#include "nkn_defs.h"
#include "nkn_mgmt_defs.h"
#include "cli_module.h"
/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
/*Geoip- service  application*/
#define GEO_MAXMIND_FILES_PATH "/nkn/maxmind/downloads"
#define GEO_QUOVA_FILES_PATH "/nkn/quova/downloads"
#define MAXMIND_INSTALLFILE_PATH "/opt/nkn/bin/geodb_update"

static int
cli_geoip_image_fetch(void *data, const cli_command *cmd,
                const tstr_array *cmd_line,
                const tstr_array *params);

static int
cli_geoip_image_fetch_finish(const char *password, clish_password_result result,
                       const cli_command *cmd,
                       const tstr_array *cmd_line, const tstr_array *params,
                       void *data, tbool *ret_continue);
static int
cli_geoip_image_fetch_internal(const char *url, const char *password,
                         const char *filename, int dbType);
int
cli_geodbd_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

static int cli_geodbd_show(
    void *data,
    const cli_command *cmd,
    const tstr_array *cmd_line,
    const tstr_array *params);

static int 
cli_geoip_image_install(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

static int
cli_geodbd_application_show(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

enum {
    GEO_MAXMIND = 1,
    GEO_QUOVA = 2
};
int
cli_geodbd_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str =		"geodb-daemon";
    cmd->cc_help_descr =	N_("Configure geodb-daemon options");
    CLI_CMD_REGISTER;
#endif

    CLI_CMD_NEW;
    cmd->cc_words_str =		"show geodb-daemon";
    cmd->cc_flags =             ccf_terminal | ccf_hidden;
    cmd->cc_capab_required =    ccp_query_basic_curr;
    cmd->cc_help_descr = 	N_("Display geodb-daemon configurations");
    cmd->cc_exec_callback =     cli_geodbd_show;
    cmd->cc_revmap_type =       crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 	   = "application maxmind";
    cmd->cc_help_descr     = N_("Application .maxmind. being configured for download.");
    CLI_CMD_REGISTER


    CLI_CMD_NEW;
    cmd->cc_words_str 	   = "application maxmind download";
    cmd->cc_help_descr     = N_("download the application database to install.");
    CLI_CMD_REGISTER

    CLI_CMD_NEW;
    cmd->cc_words_str 	   = "application maxmind download *";
    cmd->cc_flags          = ccf_terminal | ccf_sensitive_param;
    cmd->cc_capab_required = ccp_action_priv_curr;
    cmd->cc_help_exp =          N_("<url>");
    cmd->cc_help_exp_hint =     N_("Specify SCP URL of the database to be downloaded");
    cmd->cc_exec_callback  = cli_geoip_image_fetch;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str 	   = "application maxmind install";
    cmd->cc_help_descr     = N_("Install the database");
    CLI_CMD_REGISTER


    CLI_CMD_NEW;
    cmd->cc_words_str 	   = "application maxmind install *";
    cmd->cc_magic          = cfi_maxmindfiles;
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_action_priv_curr;
    cmd->cc_help_exp       = cli_msg_filename;
    cmd->cc_help_use_comp  = true;
    cmd->cc_comp_callback  = cli_file_completion;
    cmd->cc_exec_callback  = cli_geoip_image_install;
    cmd->cc_magic  =         GEO_MAXMIND;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 	   = "application quova";
    cmd->cc_help_descr     = N_("Application .quova. being configured ");
    cmd->cc_flags          = ccf_hidden;
    CLI_CMD_REGISTER


    CLI_CMD_NEW;
    cmd->cc_words_str 	   = "application quova download";
    cmd->cc_help_descr     = N_("download the database");
    CLI_CMD_REGISTER

    CLI_CMD_NEW;
    cmd->cc_words_str 	   = "application quova download *";
    cmd->cc_flags          = ccf_terminal | ccf_sensitive_param;
    cmd->cc_capab_required = ccp_action_priv_curr;
    cmd->cc_help_exp =          N_("<URL>");
    cmd->cc_help_exp_hint =     N_("scp URL");
    cmd->cc_exec_callback  = cli_geoip_image_fetch;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 	   = "application quova install";
    cmd->cc_help_descr     = N_("Install the quova application");
    CLI_CMD_REGISTER


    CLI_CMD_NEW;
    cmd->cc_words_str 	   = "application quova install *";
    cmd->cc_magic          = cfi_quovafiles;
    cmd->cc_flags          = ccf_terminal;
    cmd->cc_capab_required = ccp_action_priv_curr;
    cmd->cc_help_exp       = cli_msg_filename;
    cmd->cc_help_use_comp  = true;
    cmd->cc_comp_callback  = cli_file_completion;
    cmd->cc_exec_callback  = cli_geoip_image_install;
    cmd->cc_magic  =         GEO_QUOVA;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 	   = "show application";
    cmd->cc_help_descr     = N_("Displays the application information");
    CLI_CMD_REGISTER

    CLI_CMD_NEW;
    cmd->cc_words_str 	   = "show application maxmind";
    cmd->cc_help_descr     = N_("Displays the Maxmind information");
    cmd->cc_flags 		   = ccf_terminal;
    cmd->cc_capab_required = ccp_action_priv_curr;
    cmd->cc_exec_callback  = cli_geodbd_application_show;
    cmd->cc_magic  		   = GEO_MAXMIND;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str 	   = "show application quova";
    cmd->cc_help_descr     = N_("Displays the quova information");
    cmd->cc_flags 		   = ccf_terminal | ccf_hidden;
    cmd->cc_capab_required = ccp_action_priv_curr;
    cmd->cc_exec_callback  = cli_geodbd_application_show;
    cmd->cc_magic  		   = GEO_QUOVA;
    CLI_CMD_REGISTER;

    const char *geodbd_ignore_bindings[] = {
	"/nkn/geodbd/config/*"
    };
    err = cli_revmap_ignore_bindings_arr(geodbd_ignore_bindings);

bail:
    return err;
}

static int cli_geodbd_show(
    void *data,
    const cli_command *cmd,
    const tstr_array *cmd_line,
    const tstr_array *params)
{

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    return 0;
}

static int
cli_geoip_image_fetch(void *data, const cli_command *cmd,
                const tstr_array *cmd_line,
                const tstr_array *params)
{
    int err = 0;
    const char *remote_url = NULL;

    UNUSED_ARGUMENT(data);

    remote_url = tstr_array_get_str_quick(params, 0);
    bail_null(remote_url);

    if (clish_is_shell()) {
        err = clish_maybe_prompt_for_password_url
           (remote_url, cli_geoip_image_fetch_finish, cmd, cmd_line, params, NULL);
        bail_error(err);
    }
    else {
        err = cli_geoip_image_fetch_finish(NULL, cpr_ok, cmd, cmd_line, params, 
                                     NULL, NULL);
        bail_error(err);
    }

 bail:
    return(err);
}
static int
cli_geoip_image_fetch_finish(const char *password, clish_password_result result,
                       const cli_command *cmd,
                       const tstr_array *cmd_line, const tstr_array *params,
                       void *data, tbool *ret_continue)
{
    int err = 0;
    const char *remote_url = NULL;
    const char *filename = NULL;
    int dbType = 0;

    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(ret_continue);
    UNUSED_ARGUMENT(data);

    /*
     * We have nothing to clean up, so we really only care about this
     * if we got a successful password entry.
     */
    if (result != cpr_ok) {
        goto bail;
    }

    remote_url = tstr_array_get_str_quick(params, 0);
    bail_null(remote_url);

    filename = tstr_array_get_str_quick(params, 1);
    /* May be NULL */

#ifdef cli_default_image_filename
    cid = cmd->cc_magic;
    if (cid == cid_image_fetch_default) {
        filename = cli_default_image_filename;
    }
#endif

    err = cli_geoip_image_fetch_internal(remote_url, password, filename, dbType);
    bail_error(err);

 bail:
    return(err);
} /* end of cli_fms_image_fetch_finish() */

static int
cli_geoip_image_fetch_internal(const char *url, const char *password,
                         const char *filename, int dbType)
{
    int err = 0;
    bn_request *req = NULL;
    uint16 db_file_mode = 0;
    char *db_file_mode_str = NULL;

    err = bn_action_request_msg_create(&req, "/file/transfer/download");
    bail_error(err);

    bail_null(url);
    err = bn_action_request_msg_append_new_binding
        (req, 0, "remote_url", bt_string, url, NULL);
    bail_error(err);

    if (password) {
        err = bn_action_request_msg_append_new_binding
            (req, 0, "password", bt_string, password, NULL);
        bail_error(err);
    }

    if(dbType == GEO_QUOVA) {
	    err = bn_action_request_msg_append_new_binding
		(req, 0, "local_dir", bt_string, GEO_QUOVA_FILES_PATH, NULL);
	    bail_error(err);
    }else {
	    err = bn_action_request_msg_append_new_binding
		(req, 0, "local_dir", bt_string, GEO_MAXMIND_FILES_PATH, NULL);
	    bail_error(err);
    }

    if (filename) {
        err = bn_action_request_msg_append_new_binding
            (req, 0, "local_filename", bt_string, filename, NULL);
        bail_error(err);
    }

    err = bn_action_request_msg_append_new_binding
        (req, 0, "allow_overwrite", bt_bool, "true", NULL);
    bail_error(err);

    /*
     * If we're not in the CLI shell, we won't be displaying progress,
     * so there's no need to track it.
     *
     * XXX/EMT: we should make up our own progress ID, and start and
     * end our own operation, and tell the action about it with the
     * progress_image_id parameter.  This is so we can report errors
     * that happen outside the context of the action request.  But
     * it's not a big deal for now, since almost nothing happens
     * outside of this one action, so errors are unlikely.
     */
    if (clish_progress_is_enabled()) {
        err = bn_action_request_msg_append_new_binding
            (req, 0, "track_progress", bt_bool, "true", NULL);
        bail_error(err);
        err = bn_action_request_msg_append_new_binding
            (req, 0, "progress_oper_type", bt_string, "image_download", NULL);
        bail_error(err);
        err = bn_action_request_msg_append_new_binding
            (req, 0, "progress_erase_old", bt_bool, "true", NULL);
        bail_error(err);
    }

    db_file_mode = 0600;
    db_file_mode_str = smprintf("%d", db_file_mode);
    bail_null(db_file_mode_str);

    err = bn_action_request_msg_append_new_binding
        (req, 0, "mode", bt_uint16, db_file_mode_str, NULL);
    bail_error(err);

    if (clish_progress_is_enabled()) {
        err = clish_send_mgmt_msg_async(req);
        bail_error(err);
        err = clish_progress_track(req, NULL, 0, NULL, NULL);
        bail_error(err);
    }
    else {
        err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, NULL, NULL);
        bail_error(err);
    }

 bail:
    safe_free(db_file_mode_str);
    bn_request_msg_free(&req);
    return(err);
} /* end of  cli_fms_image_fetch_internal() */

static int 
cli_geoip_image_install(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
	int err = 0;
	const char *image_name = NULL;
	char *image_path = NULL;
	uint32 ret_err = 0;
	tstring *ret_msg = NULL;
	

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);

	/* Get the image name */
	image_name = tstr_array_get_str_quick(params, 0);
	bail_null(image_name);

	if(cmd->cc_magic == GEO_MAXMIND) {
	    image_path = smprintf ("%s/%s", GEO_MAXMIND_FILES_PATH, image_name);
	}else {
	    image_path = smprintf ("%s/%s", GEO_QUOVA_FILES_PATH, image_name);
	}
	if(image_path) {
	    err = mdc_send_action_with_bindings_str_va(cli_mcc, &ret_err, &ret_msg,
		    "/nkn/geodb/actions/install", 1,
		    "filename", bt_string, image_path);
	    bail_error(err);
	}
	
	if (ret_err != 0) {
	    cli_printf_error(_("%s"), ts_str(ret_msg));
	}

bail:
    safe_free(image_path);
    ts_free(&ret_msg);
    return err;

} /* end of cli_geoip_image_install() */

static int
cli_geodbd_application_show(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
	int err = 0;
	uint32 ret_err = 0;
	tstring *ret_msg = NULL;


	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(params);

	if(cmd->cc_magic == GEO_MAXMIND) {
	    err = mdc_send_action_with_bindings_str_va(cli_mcc, &ret_err, &ret_msg,
		    "/nkn/geodb/actions/info_display", 1,
		    "app_name", bt_string, "maxmind");
	    bail_error(err);
	}else {
	    err = mdc_send_action_with_bindings_str_va(cli_mcc, &ret_err, &ret_msg,
		    "/nkn/geodb/actions/info_display", 1,
		    "app_name", bt_string, "quova");
	    bail_error(err);
	}

bail:
    ts_free(&ret_msg);
    return err;

} /* end of cli_geodbd_application_show() */

