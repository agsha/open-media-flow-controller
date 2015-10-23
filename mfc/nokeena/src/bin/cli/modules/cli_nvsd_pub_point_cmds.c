/*
 * Filename:    cli_nvsd_pub_point_cmds.c
 * Date:        2009/08/15
 * Author:      Dhruva Narasimhan
 *
 * (C) Copyright 2008-2009, Nokeena Networks Inc.
 *  All rights reserved.
 */

#include "common.h"
#include "climod_common.h"
#include "clish_module.h"
#include "cli_module.h"
#include "nkn_defs.h"


int 
cli_nvsd_pub_point_cmds_init(
	const lc_dso_info *info,
	const cli_module_context *context);




int
cli_nvsd_pub_enter_prefix_mode( void *data,
				const cli_command *cmd,
				const tstr_array *cmd_line,
				const tstr_array *params);

int
cli_nvsd_pub_point_show( void *data,
				const cli_command *cmd,
				const tstr_array *cmd_line,
				const tstr_array *params);


int
cli_nvsd_pub_point_cfg_start_time( void *data,
				const cli_command *cmd,
				const tstr_array *cmd_line,
				const tstr_array *params);

static int 
cli_nvsd_pub_point_revmap_pub_server(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply);

int
cli_nvsd_pub_point_pub_server_reset( void *data,
				const cli_command *cmd,
				const tstr_array *cmd_line,
				const tstr_array *params);
/*---------------------------------------------------------------------------*/

int 
cli_nvsd_pub_point_cmds_init(
	const lc_dso_info *info,
	const cli_module_context *context)
{
	int err = 0;
	cli_command *cmd = NULL;

	UNUSED_ARGUMENT(info);

#ifdef PROD_FEATURE_I18N_SUPPORT
	/* This is pretty much redundant and can be skipped in your
	 * modules, unless you want internationalization support.
	 */
	err = cli_set_gettext_domain(GETTEXT_DOMAIN);
	bail_error(err);
#endif /* PROD_FEATURE_I18N_SUPPORT */

	if (context->cmc_mgmt_avail == false) {
		goto bail;
	}

	CLI_CMD_NEW;
	cmd->cc_words_str =		"pub-point";
	cmd->cc_help_descr = 		N_("Configure a publishing point for live streams");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =		"no pub-point";
	cmd->cc_help_descr = 		N_("Delete a configured publishing point");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"pub-point *";
	cmd->cc_help_exp = 		N_("<name>");
	cmd->cc_help_exp_hint = 	N_("name of the publishing point");
	cmd->cc_help_use_comp = 	true;
	cmd->cc_comp_type = 		cct_matching_names;
	cmd->cc_comp_pattern = 		"/nkn/nvsd/pub-point/config/*";
	cmd->cc_flags = 		ccf_terminal | ccf_prefix_mode_root;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_create;
	cmd->cc_exec_name = 		"/nkn/nvsd/pub-point/config/$1$";
	cmd->cc_exec_value = 		"$1$";
	cmd->cc_exec_type = 		bt_string;
	//cmd->cc_exec_callback = 	cli_nvsd_pub_enter_prefix_mode;
	cmd->cc_revmap_order = 		cro_pub_point;
	cmd->cc_revmap_type = 		crt_auto;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"no pub-point *";
	cmd->cc_help_exp = 		N_("<name>");
	cmd->cc_help_term = 		N_("Delete this publishing point");
	cmd->cc_help_use_comp = 	true;
	cmd->cc_comp_pattern = 		"/nkn/nvsd/pub-point/config/*";
	cmd->cc_comp_type = 		cct_matching_names;
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_delete;
	cmd->cc_exec_name = 		"/nkn/nvsd/pub-point/config/$1$";
	cmd->cc_exec_value = 		"$1$";
	cmd->cc_exec_type = 		bt_string;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"pub-point * no";
	cmd->cc_help_descr = 		N_("Clear/reset publishing point configuration settings");
	cmd->cc_req_prefix_count = 	2;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"pub-point * status";
	cmd->cc_help_descr = 		N_("Activate or Deactivate a publishing point");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"pub-point * status active";
	cmd->cc_help_descr = 		N_("Activate the publishing point");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/pub-point/config/$1$/active";
	cmd->cc_exec_value = 		"true";
	cmd->cc_exec_type = 		bt_bool;
	cmd->cc_revmap_order = 		cro_pub_point;
	cmd->cc_revmap_type = 		crt_auto;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"pub-point * status inactive";
	cmd->cc_help_descr = 		N_("Deactivate the publishing point");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/pub-point/config/$1$/active";
	cmd->cc_exec_value = 		"false";
	cmd->cc_exec_type = 		bt_bool;
	cmd->cc_revmap_order = 		cro_pub_point;
	cmd->cc_revmap_type = 		crt_auto;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"pub-point * sdp-name";
	cmd->cc_help_descr = 		N_("Configure a static SDP file which is retrieved by MFD when the stream is requested");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"pub-point * sdp-name *";
	cmd->cc_help_exp = 		N_("<URL>");
	cmd->cc_help_exp_hint = 	N_("URL from where the SDP file is downloaded");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/pub-point/config/$1$/sdp/static";
	cmd->cc_exec_value = 		"$2$";
	cmd->cc_exec_type = 		bt_uri;
	cmd->cc_revmap_order = 		cro_pub_point;
	cmd->cc_revmap_type = 		crt_auto;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"no pub-point * sdp-name";
	cmd->cc_help_descr = 		N_("Clear current sdp setting for this publishing point");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_reset;
	cmd->cc_exec_name = 		"/nkn/nvsd/pub-point/config/$1$/sdp/static";
	cmd->cc_exec_type = 		bt_uri;
	CLI_CMD_REGISTER;



	CLI_CMD_NEW;
	cmd->cc_words_str = 		"pub-point * event"; 
	cmd->cc_help_descr = 		N_("Configure event paramaters such as event duration etc.");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"pub-point * event duration";
	cmd->cc_help_descr = 		N_("Configure the event duration (to be displayed on the player)");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"pub-point * event duration *";
	cmd->cc_help_exp = 		cli_msg_time;
	cmd->cc_help_exp_hint = 	"";
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/pub-point/config/$1$/event/duration";
	cmd->cc_exec_value = 		"$2$";
	cmd->cc_exec_type = 		bt_string;
	cmd->cc_revmap_order = 		cro_pub_point;
	cmd->cc_revmap_type = 		crt_auto;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"no pub-point * event";
	cmd->cc_help_descr = 		N_("Clear event settings for this publishing point");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"no pub-point * event duration";
	cmd->cc_help_descr = 		N_("Clear event duration for this publishing point");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_reset;
	cmd->cc_exec_name = 		"/nkn/nvsd/pub-point/config/$1$/event/duration";
	cmd->cc_exec_type = 		bt_string;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"pub-point * pub-server";
	cmd->cc_help_descr = 		N_("Configure publishing server parameters");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"pub-point * pub-server *";
	cmd->cc_help_exp = 		N_("<URL>");
	cmd->cc_help_exp_hint = 	N_("URL of the publishing server");
	//cmd->cc_help_use_comp = 	true;
	//cmd->cc_comp_type = 		cct_matching_names;
	//cmd->cc_comp_pattern = 		"/nkn/nvsd/pub-point/config/*/pub-server/uri";
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/pub-point/config/$1$/pub-server/uri";
	cmd->cc_exec_value = 		"$2$";
	cmd->cc_exec_type = 		bt_uri;
	cmd->cc_revmap_order = 		cro_pub_point;
	cmd->cc_revmap_type = 		crt_none;
	cmd->cc_revmap_callback = 	cli_nvsd_pub_point_revmap_pub_server;
	cmd->cc_revmap_names = 		"/nkn/nvsd/pub-point/config/*";
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"pub-point * pub-server * receive-mode";
	cmd->cc_help_descr = 		N_("Configure how MFD gets live data");
	CLI_CMD_REGISTER;
	
	CLI_CMD_NEW;
	cmd->cc_words_str = 		"pub-point * pub-server * receive-mode immediate";
	cmd->cc_help_descr = 		N_("Configure MFD to immediately connect to the "
					"publishing server when this pub-point is made active");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/pub-point/config/$1$/pub-server/uri";
	cmd->cc_exec_value = 		"$2$";
	cmd->cc_exec_type = 		bt_uri;
	cmd->cc_exec_name2 = 		"/nkn/nvsd/pub-point/config/$1$/pub-server/mode";
	cmd->cc_exec_value2 = 		"immediate";
	cmd->cc_exec_type2 = 		bt_string;
	//cmd->cc_revmap_order = 		cro_pub_point;
	//cmd->cc_revmap_type = 		crt_auto;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"pub-point * pub-server * receive-mode on-demand";
	cmd->cc_help_descr = 		N_("Configure MFD to connect to the publishing "
					"server only when the first client connects to MFD");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_set;
	cmd->cc_exec_name = 		"/nkn/nvsd/pub-point/config/$1$/pub-server/uri";
	cmd->cc_exec_value = 		"$2$";
	cmd->cc_exec_type = 		bt_uri;
	cmd->cc_exec_name2 = 		"/nkn/nvsd/pub-point/config/$1$/pub-server/mode";
	cmd->cc_exec_value2 = 		"on-demand";
	cmd->cc_exec_type2 = 		bt_string;
	//cmd->cc_revmap_order = 		cro_pub_point;
	//cmd->cc_revmap_type = 		crt_auto;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"pub-point * pub-server * receive-mode start-time";
	cmd->cc_help_descr = 		N_("Configure MFD will start accepting data from "
					"the specified start time ");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"pub-point * pub-server * receive-mode start-time *";
	cmd->cc_help_exp = 		cli_msg_time;
	cmd->cc_help_term = 		N_("Start looking for live feed from this Start-Time");
	cmd->cc_help_exp_hint = 	N_("Start time of the feed");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_callback = 	cli_nvsd_pub_point_cfg_start_time;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"pub-point * pub-server * receive-mode start-time * end-time";
	cmd->cc_help_descr = 		N_("Configure MFD to look for the stream data upto this end time");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"pub-point * pub-server * receive-mode start-time * end-time *";
	cmd->cc_help_exp = 		cli_msg_time;
	cmd->cc_help_term = 		N_("End live stream at this time");
	cmd->cc_help_exp_hint = 	N_("Start time of the feed");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required =	ccp_set_rstr_curr;
	cmd->cc_exec_callback = 	cli_nvsd_pub_point_cfg_start_time;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"no pub-point * pub-server";
	cmd->cc_help_descr = 		N_("Clear publishing server configuration");
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_set_rstr_curr;
	cmd->cc_exec_operation = 	cxo_reset;
	cmd->cc_exec_name = 		"/nkn/nvsd/pub-point/config/$1$/pub-server/uri";
	cmd->cc_exec_callback = 	cli_nvsd_pub_point_pub_server_reset;
	CLI_CMD_REGISTER;


	CLI_CMD_NEW;
	cmd->cc_words_str = 		"show pub-point";
	cmd->cc_help_descr = 		N_("Show a publish point configuration");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str = 		"show pub-point *";
	cmd->cc_help_exp = 		N_("<name>");
	cmd->cc_help_exp_hint = 	"";
	cmd->cc_help_use_comp = 	true;
	cmd->cc_flags = 		ccf_terminal;
	cmd->cc_capab_required = 	ccp_query_basic_curr;
	cmd->cc_comp_type = 		cct_matching_names;
	cmd->cc_comp_pattern = 		"/nkn/nvsd/pub-point/config/*";
	cmd->cc_exec_callback = 	cli_nvsd_pub_point_show;
	CLI_CMD_REGISTER;


bail:
	return err;
}


int
cli_nvsd_pub_enter_prefix_mode( void *data,
				const cli_command *cmd,
				const tstr_array *cmd_line,
				const tstr_array *params)
{
	int err = 0;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(params);

	return err;
}

int
cli_nvsd_pub_point_cfg_start_time( void *data,
				const cli_command *cmd,
				const tstr_array *cmd_line,
				const tstr_array *params)
{
	int err = 0;
	char *name = NULL;
	char *b_name = NULL;
	const char *start_time = NULL;
	const char *end_time = NULL;
	const char *t_mode = NULL;
	const char *t_name = NULL;
	const char *t_uri = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);

	t_name = tstr_array_get_str_quick(params, 0);
	bail_null(t_name);

	t_uri = tstr_array_get_str_quick(params, 1);
	bail_null(t_uri);


	/* This can be null */
	t_mode = tstr_array_get_str_quick(cmd_line, 5);

	if (t_mode) {
		/* Check if start-time is given. if so, then extract args.
		 * For all other cases, the default action executes 
		 * and we dont come here
		 */
		if (safe_strcmp(t_mode, "start-time") == 0) {
			start_time = tstr_array_get_str_quick(params, 2);
			bail_null(start_time);

			/* Check whether end-time was also given */
			if (tstr_array_length_quick(params) > 3) {
				/* end-time is also given */
				end_time = tstr_array_get_str_quick(params, 3);
				bail_null(end_time);
			}
		}
	}

	name = smprintf("/nkn/nvsd/pub-point/config/%s", t_name);
	bail_null(name);
	err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_create, bt_string, t_name, name);
	bail_error(err);

	b_name = smprintf("%s/pub-server/uri", name);
	bail_null(b_name);

	err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_modify, bt_uri, t_uri, b_name);
	bail_error(err);
	safe_free(b_name);


	b_name = smprintf("%s/pub-server/mode", name);
	bail_null(b_name);

	err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_modify, bt_string, t_mode, b_name);
	bail_error(err);
	safe_free(b_name);

	b_name = smprintf("%s/pub-server/start-time", name);
	bail_null(b_name);

	err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_modify, bt_string, start_time, b_name);
	bail_error(err);
	safe_free(b_name);


	if (end_time) {
		b_name = smprintf("%s/pub-server/end-time", name);
		bail_null(b_name);

		err = mdc_set_binding(cli_mcc, NULL, NULL, bsso_modify, bt_string, end_time, b_name);
		bail_error(err);
		safe_free(b_name);
	}

bail:
	safe_free(name);
	safe_free(b_name);
	return err;
}

int
cli_nvsd_pub_point_show( void *data,
				const cli_command *cmd,
				const tstr_array *cmd_line,
				const tstr_array *params)
{
	int err = 0;
	const char *name = NULL;
	char *pp = NULL;
	tstring *t_name = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);

	/* Validate the name here */
	name = tstr_array_get_str_quick(params, 0);
	bail_null(name);

	pp = smprintf("/nkn/nvsd/pub-point/config/%s", tstr_array_get_str_quick(params, 0));
	bail_null(pp);

	err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL, &t_name, pp, NULL);
	bail_error(err);

	if ( !t_name || !ts_equal_str(t_name, name, false) ) {
		cli_printf_error(_("error : Unknown pub-point name \"%s\"\n"), name);
		goto bail;
	}


	err = cli_printf_query(_(
				"Publish Point : \"#%s#\"\n"), pp);
	bail_error(err);

	err = cli_printf_query(_(
				"    Active            : #%s/active#\n"), pp);
	bail_error(err);

	err = cli_printf_query(_(
				"    Event Duration     : #%s/event/duration#\n"), pp);
	bail_error(err);

	err = cli_printf_query(_(
				"    SDP Name           : #%s/sdp/static#\n"), pp);
	bail_error(err);

	err = cli_printf_query(_(
				"    Publish Server     : #%s/pub-server/uri#\n"), pp);
	bail_error(err);

	err = cli_printf_query(_(
				"        Receive Mode   : #%s/pub-server/mode#\n"), pp);
	bail_error(err);

	err = cli_printf_query(_(
				"        Start Time     : #%s/pub-server/start-time#\n"), pp);
	bail_error(err);

	err = cli_printf_query(_(
				"        End Time       : #%s/pub-server/end-time#\n"), pp);
	bail_error(err);

bail:
	safe_free(pp);
	ts_free(&t_name);
	return err;
}


static int 
cli_nvsd_pub_point_revmap_pub_server(void *data, const cli_command *cmd,
                          const bn_binding_array *bindings, const char *b_name,
                          const tstr_array *name_parts, const char *value,
                          const bn_binding *binding,
                          cli_revmap_reply *ret_reply)
{
	int err = 0;
	//char *b_name = NULL;
	const char *t_name = NULL;
	tstring *t_mode = NULL;
	tstring *t_uri = NULL;
	tstring *t_start_time = NULL;
	tstring *t_end_time = NULL;
	tstring *rev_cmd = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(binding);

	err = tstr_array_get_str (name_parts, 4, &t_name);
	bail_error_null(err, t_name);

	/* Get the pub-server */
	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, 
			NULL, &t_uri, "%s/pub-server/uri", b_name);
	bail_error(err);

	if (ts_length(t_uri) == 0) {
		/* No pub-server configuered.. bail */
		goto revmapped;
	}

	err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, 
			NULL, &t_mode, "%s/pub-server/mode", b_name);
	bail_error(err);

	if (ts_cmp_str(t_mode, "start-time", false)) {
		err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, 
				NULL, &t_start_time, "%s/pub-server/start-time", b_name);
		bail_error(err);
		
		err = bn_binding_array_get_value_tstr_by_name_fmt(bindings, 
				NULL, &t_end_time, "%s/pub-server/end-time", b_name);
		bail_error(err);
	}

	err = ts_new_sprintf(&rev_cmd, "pub-point %s pub-server %s", 
			t_name, ts_str(t_uri));
	bail_error(err);

	if (ts_equal_str(t_mode, "start-time", false)) {
		err = ts_append_sprintf(rev_cmd, " receive-mode start-time %s", 
				ts_str(t_start_time));
		bail_error(err);

		if (ts_length(t_end_time) > 0) {
			err = ts_append_sprintf(rev_cmd, " end-time %s\n",
					ts_str(t_end_time));
			bail_error(err);
		}
		else {
			err = ts_append_str(rev_cmd, "\n");
			bail_error(err);
		}
	}
	else {
		err = ts_append_sprintf(rev_cmd, " receive-mode %s\n",
				ts_str(t_mode));
		bail_error(err);
	}

	err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
	bail_error(err);
revmapped:
	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/pub-server/uri", b_name);
	bail_error(err);
	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/pub-server/mode", b_name);
	bail_error(err);
	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/pub-server/start-time", b_name);
	bail_error(err);
	err = tstr_array_append_sprintf(ret_reply->crr_nodes, "%s/pub-server/end-time", b_name);
	bail_error(err);

bail:
	return err;
}


int
cli_nvsd_pub_point_pub_server_reset( void *data,
				const cli_command *cmd,
				const tstr_array *cmd_line,
				const tstr_array *params)
{
	int err = 0;
	const char *child[] = {"uri", "mode", "start-time", "end-time"};
	const char *name = NULL;
	char *bn_name = NULL;
	uint32 i = 0;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd_line);

	if (cmd->cc_exec_operation != cxo_reset) {
		goto bail;
	}


	name = tstr_array_get_str_quick(params, 0);
	bail_null(name);

	for (i = 0; i < (sizeof(child)/sizeof(const char *)); i++) {

		bn_name = smprintf("/nkn/nvsd/pub-point/config/%s/pub-server/%s", name, child[i]);
		bail_null(bn_name);
	
		err = mdc_reset_binding(cli_mcc, NULL, NULL, bn_name);
		bail_error(err);
		
		safe_free(bn_name);
	}

bail:
	safe_free(bn_name);
	return err;
}
/*
 * vim:ts=8:sw=8:wm=8:noexpandtab
 */
