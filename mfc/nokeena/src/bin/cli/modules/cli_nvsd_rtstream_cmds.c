/*
 * Filename :   cli_nvsd_rtstream_cmds.c
 * Date     :   2008/12/02
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
#include "nkn_defs.h"

#define DEFAULT_SERVER_PORT "554"

#if 0 /* Looks like this is not needed anymore - Ram (20th July 2010) */
static cli_help_callback cli_interface_help = NULL;
static cli_completion_callback cli_interface_completion = NULL;
#endif // 0
static int (*cli_interface_validate)(const char *ifname, tbool *ret_valid) = NULL;


enum {
    cli_nvsd_rtstream_if_all = 0,
    cli_nvsd_rtstream_if_list,
};
enum {
    cid_default_server_port = 1,
    cid_configure_server_port = 2
};

enum {
    cid_rtstream_port_add = 1,
    cid_rtstream_port_delete = 2
};


extern int
cli_std_exec(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params);


int
cli_nvsd_rtstream_set_vod_server_port(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params);

int 
cli_proto_enter_prefix_mode(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int 
cli_nvsd_rtstream_interface_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
int
cli_nvsd_rtstream_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int
cli_nvsd_rtstream_listen_port_vod_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int
cli_nvsd_rtstream_listen_port_live_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int cli_nvsd_delivery_enter_prefix_mode(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int cli_nvsd_rtstream_show_cmd(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);


int 
cli_nvsd_rtstream_show_content_types_cmd(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int 
cli_nvsd_rtstream_content_type_help(
        void *data,
        cli_help_type help_type,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        void *context);


int 
cli_nvsd_rtstream_content_type_completion(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions);

int 
cli_nvsd_rtstream_header_help(
        void *data,
        cli_help_type help_type,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        void *context);


int 
cli_nvsd_rtstream_header_completion(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions);

int 
cli_nvsd_rtstream_set_file_ext(void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nvsd_rtstream_show_interface(const bn_binding_array *bindings,
                            uint32 idx, const bn_binding *binding,
                            const tstring *name,
                            const tstr_array *name_components,
                            const tstring *name_last_part,
                            const tstring *value, void *callback_data);
int
cli_nvsd_rtstream_content_type_show(const bn_binding_array *bindings,
                            uint32 idx, const bn_binding *binding,
                            const tstring *name,
                            const tstr_array *name_components,
                            const tstring *name_last_part,
                            const tstring *value, void *callback_data);
int
cli_nvsd_rtstream_header_suppress_show(const bn_binding_array *bindings,
                            uint32 idx, const bn_binding *binding,
                            const tstring *name,
                            const tstr_array *name_components,
                            const tstring *name_last_part,
                            const tstring *value, void *callback_data);

int
cli_nvsd_rtstream_header_append_show(const bn_binding_array *bindings,
                            uint32 idx, const bn_binding *binding,
                            const tstring *name,
                            const tstr_array *name_components,
                            const tstring *name_last_part,
                            const tstring *value, void *callback_data);
int
cli_nvsd_rtstream_header_set_show(const bn_binding_array *bindings,
                            uint32 idx, const bn_binding *binding,
                            const tstring *name,
                            const tstr_array *name_components,
                            const tstring *name_last_part,
                            const tstring *value, void *callback_data);

int
cli_nvsd_rtstream_live_port_config(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params);

int
cli_nvsd_rtstream_vod_port_config(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params);
int
cli_nvsd_rtsp_server_port_show(const bn_binding_array *bindings,
		uint32 idx, const bn_binding *binding,
		const tstring *name,
		const tstr_array *name_components,
		const tstring *name_last_part,
		const tstring *value, void *callback_data);
/*----------------------------------------------------------------------------
 * MODULE ENTRY POINT
 *---------------------------------------------------------------------------*/
int
cli_nvsd_rtstream_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;
    void *callback = NULL;

#ifdef PROD_FEATURE_I18N_SUPPORT
    err = cli_set_gettext_domain(GETTEXT_DOMAIN);
    bail_error(err);
#endif

    if (context->cmc_mgmt_avail == false) {
        goto bail;
    }

    /* Must get the help and completion callbacks from
     * cli_interface_cmd
     */

#if 0 /* Looks like this is not needed anymore - Ram (20th July 2010) */
    err = lc_dso_module_get_symbol(info->ldi_context, 
            "cli_interface_cmds", "cli_interface_help", 
            &callback);
    bail_error_null(err, callback);

    cli_interface_help = (cli_help_callback)(callback);

    err = lc_dso_module_get_symbol(info->ldi_context, 
            "cli_interface_cmds", "cli_interface_completion", 
            &callback);
    bail_error_null(err, callback);
    cli_interface_completion = (cli_completion_callback)(callback);

#endif // 0
    err = lc_dso_module_get_symbol(info->ldi_context, 
            "cli_interface_cmds", "cli_interface_validate", 
            &callback);
    bail_error_null(err, callback);
    cli_interface_validate = (callback);


    /* Parent command is defined in cli_nvsd_delivery_cmds.c */

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol rtsp";
    cmd->cc_help_descr =            N_("Configure delivery protocol options");
    cmd->cc_flags =                 ccf_terminal | ccf_prefix_mode_root;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_callback =         cli_nvsd_delivery_enter_prefix_mode;
    cmd->cc_revmap_order =          cro_delivery;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol rtsp";
    cmd->cc_help_descr =            N_("Clear delivery protocol options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol rtsp no";
    cmd->cc_help_descr =            N_("Disable/Negate protocol options");
    cmd->cc_req_prefix_count =      3;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol rtsp trace";
    cmd->cc_help_descr =            N_("Delivery Protocol Trace Flag");
    cmd->cc_flags = 		    ccf_unavailable;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol rtsp trace enable";
    cmd->cc_help_descr =            N_("Enable the Delivery Protocol Trace");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_basic_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/rtstream/config/trace/enabled";
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_type =             bt_bool;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol rtsp trace";
    cmd->cc_help_descr =            N_("Delivery Protocol Trace Flag");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol rtsp trace enable";
    cmd->cc_help_descr =            N_("Disable the Delivery Protocol Trace");
    cmd->cc_flags =                 ccf_terminal | ccf_allow_extra_params;
    cmd->cc_capab_required =        ccp_set_basic_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/rtstream/config/trace/enabled";
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_type =             bt_bool;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol rtsp listen";
    cmd->cc_help_descr =            N_("Configure listen port parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol rtsp listen";
    cmd->cc_help_descr =            N_("Clear listen port parameters");
    CLI_CMD_REGISTER;


    err = cli_nvsd_rtstream_listen_port_vod_cmds_init(info, context);
    bail_error(err);

    //err = cli_nvsd_rtstream_listen_port_live_cmds_init(info, context);
    //bail_error(err);


    /*----------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol rtsp rate-control";
    cmd->cc_help_descr =            N_("Configure rate control");
    cmd->cc_flags =                 ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol rtsp rate-control 30000";
    cmd->cc_help_descr =            N_("Set default value - 30000");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_type =             bt_uint64;
    cmd->cc_exec_name =             "/nkn/nvsd/rtstream/config/rate_control";
    cmd->cc_exec_value =            "30000";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol rtsp rate-control *";
    cmd->cc_help_exp =              N_("<number>");
    cmd->cc_help_exp_hint =         N_("Number");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_type =             bt_uint64;
    cmd->cc_exec_name =             "/nkn/nvsd/rtstream/config/rate_control";
    cmd->cc_exec_value =            "$1$";
    CLI_CMD_REGISTER;
    /*----------------------------------------------------------------------*/

    /*----------------------------------------------------------------------*/

    /*----------------------------------------------------------------------*/
#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol rtsp om-conn-limit";
    cmd->cc_help_descr =            N_("Configure OM Connection limit");
    cmd->cc_flags =                 ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol rtsp om-conn-limit *";
    cmd->cc_help_exp =              N_("<number>");
    cmd->cc_help_exp_hint =         N_("Max number of connections");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/rtstream/config/om_conn_limit";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_type =             bt_uint32;
    CLI_CMD_REGISTER;
#endif

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol rtsp interface";
    cmd->cc_help_descr =            N_("Configure interfaces for RTSP");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol rtsp interface";
    cmd->cc_help_descr =            N_("Reset interfaces for RTSP");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol rtsp interface *";
    cmd->cc_help_exp =              N_("<interface name>");
    cmd->cc_help_exp_hint =         N_(" for example eth0 ");
    cmd->cc_flags =                 ccf_terminal | ccf_allow_extra_params;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_use_comp =         true;
    cmd->cc_comp_type =             cct_matching_names;
    cmd->cc_comp_pattern =          "/net/interface/config/*";
    cmd->cc_exec_callback =         cli_nvsd_rtstream_interface_config;
    cmd->cc_magic =                 cli_nvsd_rtstream_if_list;
    cmd->cc_revmap_order =          cro_delivery;
    cmd->cc_revmap_type =           crt_manual;
    cmd->cc_revmap_names =          "/nkn/nvsd/rtstream/config/interface/*";
    cmd->cc_revmap_cmds =           "delivery protocol rtsp interface $v$";
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol rtsp interface *";
    cmd->cc_help_exp =              N_("<interface name>");
    cmd->cc_help_exp_hint =         N_(" for example eth0 ");
    cmd->cc_flags =                 ccf_terminal | ccf_allow_extra_params; 
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_use_comp =         true;
    cmd->cc_comp_type =             cct_matching_names;
    cmd->cc_comp_pattern =          "/nkn/nvsd/rtstream/config/interface/*";
    cmd->cc_exec_callback =         cli_nvsd_rtstream_interface_config;
    cmd->cc_magic =                 cli_nvsd_rtstream_if_list;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol rtsp origin-idle-timeout";
    cmd->cc_help_descr =            N_("Configure Origin Idle timeout");
    cmd->cc_flags =                 ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol rtsp origin-idle-timeout *";
    cmd->cc_help_exp =              N_("<seconds>");
    cmd->cc_help_exp_hint =         N_("Origin side idle timeout, default - 60 seconds");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/rtstream/config/om_idle_timeout";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_type =             bt_uint32;
    cmd->cc_revmap_type = 	    crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol rtsp player-idle-timeout";
    cmd->cc_help_descr =            N_("Configure OM Idle timeout");
    cmd->cc_flags =                 ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol rtsp player-idle-timeout *";
    cmd->cc_help_exp =              N_("<seconds>");
    cmd->cc_help_exp_hint =         N_("Player side timeout, default - 60 seconds");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/rtstream/config/player_idle_timeout";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_type =             bt_uint32;
    cmd->cc_revmap_type = 	    crt_none;
    CLI_CMD_REGISTER;

    /* "show delivery protocol" is registerd elsewhere */

    CLI_CMD_NEW;
    cmd->cc_words_str =             "show delivery protocol rtsp";
    cmd->cc_help_descr =            N_("Display RTSP configuration options");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_query_rstr_curr;
    cmd->cc_exec_callback =         cli_nvsd_rtstream_show_cmd;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str = 	"delivery protocol rtsp rtsp-origin";
    cmd->cc_help_descr = 	N_("Configure global RTSP origin specific parameters");
    cmd->cc_flags = 		ccf_hidden;
    CLI_CMD_REGISTER;
   
    CLI_CMD_NEW;
    cmd->cc_words_str = 	"delivery protocol rtsp rtsp-origin format";
    cmd->cc_help_descr = 	N_("Configure format parameters for file fetched from RTSP origin");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"delivery protocol rtsp rtsp-origin format conversion";
    cmd->cc_help_descr = 	N_("Configure format conversion parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"delivery protocol rtsp rtsp-origin format conversion enable";
    cmd->cc_help_descr = 	N_("Enable format conversion of fetched file to local format");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_operation = 	cxo_set;
    cmd->cc_exec_name = 	"/nkn/nvsd/rtstream/config/rtsp-origin/format/convert/enable";
    cmd->cc_exec_value = 	"true";
    cmd->cc_exec_type = 	bt_bool;
    cmd->cc_revmap_type = 	crt_none;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"delivery protocol rtsp rtsp-origin format conversion disable";
    cmd->cc_help_descr = 	N_("Disable format conversion of fetched file to local format");
    cmd->cc_flags = 		ccf_terminal;
    cmd->cc_capab_required = 	ccp_set_rstr_curr;
    cmd->cc_exec_operation = 	cxo_set;
    cmd->cc_exec_name = 	"/nkn/nvsd/rtstream/config/rtsp-origin/format/convert/enable";
    cmd->cc_exec_value = 	"false";
    cmd->cc_exec_type = 	bt_bool;
    cmd->cc_revmap_type = 	crt_none;
    CLI_CMD_REGISTER;

    err = cli_revmap_ignore_bindings_va(10, 
            "/nkn/nvsd/rtstream/config/content/type/*",
            "/nkn/nvsd/rtstream/config/content/type/*/allow",
            "/nkn/nvsd/rtstream/config/response/header/append/*",
            "/nkn/nvsd/rtstream/config/response/header/set/*",
            "/nkn/nvsd/rtstream/config/rate_control",
            "/nkn/nvsd/rtstream/config/trace/enabled",
            "/nkn/nvsd/rtstream/config/max_rtstream_req_size",
	    "/nkn/nvsd/rtstream/config/player_idle_timeout",
	    "/nkn/nvsd/rtstream/config/om_idle_timeout",
	    "/nkn/nvsd/rtstream/config/rtsp-origin/format/convert/enable");

bail:
    return err;
}


/*--------------------------------------------------------------------------*/
int
cli_nvsd_rtstream_set_vod_server_port(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params)
{
    int err = 0;
    const char *c_port = NULL;
    char *bn_server_port = NULL;
    uint32 code =0;

    /* Call the standard cli processing */
    err = cli_std_exec(data, cmd, cmd_line, params);
    bail_error(err);

    bn_server_port = smprintf("/nkn/nvsd/rtstream/config/server_port");
    bail_null(bn_server_port);

    switch(cmd->cc_magic){
    	case cid_default_server_port:
		c_port = DEFAULT_SERVER_PORT;
		break;
	case cid_configure_server_port:
    		c_port = tstr_array_get_str_quick(params, 0);
	        bail_null(c_port);
		break;
    }
    err = mdc_set_binding(cli_mcc, &code, NULL, bsso_modify, bt_uint16,
              c_port, bn_server_port);
    bail_error(err);
    if(code){
	cli_printf_error (" Error:Invalid Port Number.The server port value was not set \n");
    	goto bail;
    }
    /* Print the restart message */
    err = cli_printf("warning: if command successful, please restart mod-delivery service for change to take effect\n");
    bail_error(err);

 bail:
    safe_free(bn_server_port);
    return(err);
}

int
cli_nvsd_rtstream_show_interface(const bn_binding_array *bindings,
                            uint32 idx, const bn_binding *binding,
                            const tstring *name,
                            const tstr_array *name_components,
                            const tstring *name_last_part,
                            const tstring *value, void *callback_data)
{
    int err = 0;
    tstring *interface = NULL;

    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(name_last_part);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(callback_data);

    err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &interface);
    bail_error(err);

    err = cli_printf(
                _("     Interface: %s\n"), ts_str(interface));
    bail_error(err);

bail:
    ts_free(&interface);
    return err;
}

/*--------------------------------------------------------------------------*/
int
cli_nvsd_rtstream_content_type_show(const bn_binding_array *bindings,
                            uint32 idx, const bn_binding *binding,
                            const tstring *name,
                            const tstr_array *name_components,
                            const tstring *name_last_part,
                            const tstring *value, void *callback_data)
{
    int err = 0;
    tstring *type = NULL;
    //tstring *mime = NULL;

    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(name_last_part);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(callback_data);

    err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &type);
    bail_error(err);

    err = cli_printf(
                _("     %s"), ts_str(type));
    bail_error(err);

    cli_printf_query(
                _(" => #%s/mime#\n"), ts_str(name));

bail:
    ts_free(&type);
    return err;
}

/*--------------------------------------------------------------------------*/
int
cli_nvsd_rtstream_header_suppress_show(const bn_binding_array *bindings,
                            uint32 idx, const bn_binding *binding,
                            const tstring *name,
                            const tstr_array *name_components,
                            const tstring *name_last_part,
                            const tstring *value, void *callback_data)
{
    int err = 0;
    tstring *header = NULL;

    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(name_last_part);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(callback_data);

    err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &header);
    bail_error(err);

    err = cli_printf(
                _("     %s\n"), ts_str(header));
    bail_error(err);

bail:
    ts_free(&header);
    return err;
}

/*--------------------------------------------------------------------------*/
int
cli_nvsd_rtstream_header_append_show(const bn_binding_array *bindings,
                            uint32 idx, const bn_binding *binding,
                            const tstring *name,
                            const tstr_array *name_components,
                            const tstring *name_last_part,
                            const tstring *value, void *callback_data)
{
    int err = 0;
    tstring *header = NULL;
    //tstring *value = NULL;

    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(name_last_part);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(callback_data);

    if (idx == 0) {
        err = cli_printf(
                _("\nAppended Headers:\n"));
        bail_error(err);
    }
    
    err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &header);
    bail_error(err);

    err = cli_printf(
                _("     Header: %s"), ts_str(header));
    bail_error(err);

    err = cli_printf_query(
                _(" => #%s/value#\n"), ts_str(name));
    bail_error(err);


bail:
    return err;
}

/*--------------------------------------------------------------------------*/
int
cli_nvsd_rtstream_header_set_show(const bn_binding_array *bindings,
                            uint32 idx, const bn_binding *binding,
                            const tstring *name,
                            const tstr_array *name_components,
                            const tstring *name_last_part,
                            const tstring *value, void *callback_data)
{
    int err = 0;
    tstring *header = NULL;

    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(name_last_part);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(callback_data);

    err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &header);
    bail_error(err);

    err = cli_printf(
                _("     %s"), ts_str(header));
    bail_error(err);

    err = cli_printf_query(
                _(" => #%s/value#\n"), ts_str(name));
    bail_error(err);


bail:
    return err;
}

/*--------------------------------------------------------------------------*/
int cli_nvsd_rtstream_show_cmd(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    uint32 num_intfs = 0;
    uint32 num_ports = 0;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    err = cli_printf(
            _("Delivery Protocol : rtstream\n"));
    bail_error(err);

    err = cli_printf(
            _("Server Ports:\n"));
    bail_error(err);
    
    err = mdc_foreach_binding(cli_mcc, "/nkn/nvsd/rtstream/config/vod/server_port/*/port", NULL,
		    cli_nvsd_rtsp_server_port_show, NULL, &num_ports);
    bail_error(err);

    if( num_ports == 0 ) {
	    cli_printf( _("     None\n"));
    }

    err = cli_printf(
            _("\nListen Interfaces:\n"));
    bail_error(err);
    err = mdc_foreach_binding(cli_mcc,
            "/nkn/nvsd/rtstream/config/interface/*", NULL,
            cli_nvsd_rtstream_show_interface, NULL, &num_intfs);
    bail_error(err);

    if (num_intfs == 0) {
        err = cli_printf(
                _("      Default (All Configured interfaces)\n"));
        bail_error(err);
    }
#if 0	
    cli_printf_query( _("Origin-Manager Idle Timeout: "
			    " #/nkn/nvsd/rtstream/config/om_idle_timeout# (seconds) \n"));

    cli_printf_query( _("Player Idle Timeout: "
			    " #/nkn/nvsd/rtstream/config/player_idle_timeout# (seconds) \n"));
#endif

#if 0 /* The append command has been removed - Ram Apil 14th 2009 */
    err = mdc_foreach_binding(cli_mcc, 
            "/nkn/nvsd/rtstream/config/response/header/append/*", NULL,
            cli_nvsd_rtstream_header_append_show, NULL, &num_hdrs);
    bail_error(err);
    if (num_hdrs == 0) {
        err = cli_printf(
                _("\nAppended Headers:\n"));
        bail_error(err);
        err = cli_printf(
                _("     No Append Headers.\n"));
        bail_error(err);
    }
#endif /* 0 */

bail :

    return err;
}

int 
cli_proto_enter_prefix_mode(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(params);

    if(cli_prefix_modes_is_enabled()) {
        err = cli_prefix_enter_mode(cmd, cmd_line);
        bail_error(err);
    }
    else {
        err = cli_print_incomplete_command_error(cmd, cmd_line);
        bail_error(err);
    }
bail:
    return err;

}


int 
cli_nvsd_rtstream_interface_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    tbool valid = false;
    uint32 idx = 0, num_parms = 0;
    tbool delete_cmd = false;
    const char *if_name = NULL;
    char *bname = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);

    num_parms = tstr_array_length_quick(params);
    for(idx = 0; idx < num_parms; ++idx) {
        err = cli_interface_validate(tstr_array_get_str_quick(params, idx), &valid);
        bail_error(err);

        if (!valid) {
            goto bail;
        }
        else {
            valid = false;
        }
    }

    delete_cmd = (strcmp(tstr_array_get_str_quick(cmd_line, 0), "no") == 0) ? 
            true : false;

    /* If we reached up to this line, then all params are valid interfaces
     */
    num_parms = tstr_array_length_quick(params);
    for(idx = 0; idx < num_parms; idx++) {
        if_name = tstr_array_get_str_quick(params, idx);
        bail_null(if_name);

        bname = smprintf("/nkn/nvsd/rtstream/config/interface/%s", if_name);
        bail_null(bname);

        /* Create the node */
        if (!delete_cmd) {
            err = mdc_create_binding(cli_mcc, NULL, NULL,
                    bt_string,
                    if_name,
                    bname);
            bail_error(err);
        }
        /* Delete the node */
        else {
            err = mdc_delete_binding(cli_mcc, NULL, NULL, bname);
            bail_error(err);
        }

        safe_free(bname);
    }

    err = cli_printf("warning: if command successful, please restart mod-delivery service for change to take effect\n");
    bail_error(err);

bail:
    safe_free(bname);
    return err;
}


int cli_nvsd_delivery_enter_prefix_mode(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(params);

    if(cli_prefix_modes_is_enabled()) {
        err = cli_prefix_enter_mode(cmd, cmd_line);
        bail_error(err);
    }
    else {
        err = cli_print_incomplete_command_error(cmd, cmd_line);
        bail_error(err);
    }
bail:
    return err;

}


int
cli_nvsd_rtstream_listen_port_vod_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
	int err = 0;
	cli_command *cmd = NULL;

	UNUSED_ARGUMENT(info);
	UNUSED_ARGUMENT(context);

	CLI_CMD_NEW;
	cmd->cc_words_str =             "delivery protocol rtsp listen port";
	cmd->cc_help_descr =            N_("Set listening port parameters");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =             "no delivery protocol rtsp listen port";
	cmd->cc_help_descr =            N_("Reset listening port parameters");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =             "delivery protocol rtsp listen port *";
	cmd->cc_flags =                 ccf_terminal | ccf_allow_extra_params;
	cmd->cc_capab_required =        ccp_set_basic_curr;
	cmd->cc_help_exp =              N_("<port>");
	cmd->cc_help_exp_hint =         N_("The RTSP Port(s) used to deliver "
					"the video, for example 554 8554 8000-8010");
	cmd->cc_exec_callback  =        cli_nvsd_rtstream_vod_port_config;
	cmd->cc_magic	=	    	cid_rtstream_port_add;
	cmd->cc_revmap_order =          cro_delivery;
	cmd->cc_revmap_type =           crt_manual;
	cmd->cc_revmap_names =          "/nkn/nvsd/rtstream/config/vod/server_port/*/port";
	cmd->cc_revmap_cmds =           "delivery protocol rtsp listen port $v$";
	CLI_CMD_REGISTER;


	CLI_CMD_NEW;
	cmd->cc_words_str =             "no delivery protocol rtsp listen port *";
	cmd->cc_flags =                 ccf_terminal | ccf_allow_extra_params;
	cmd->cc_capab_required =        ccp_set_basic_curr;
	cmd->cc_help_exp =              N_("<port>");
	cmd->cc_help_exp_hint =         N_("The RTSP Port(s) used to deliver "
					"the video, for example, 554 8000-8010");
	cmd->cc_exec_callback  =        cli_nvsd_rtstream_vod_port_config;
	cmd->cc_magic	=	    	cid_rtstream_port_delete;
	cmd->cc_revmap_type =           crt_none;
	CLI_CMD_REGISTER;

	err = cli_revmap_ignore_bindings_va(1, 
			"/nkn/nvsd/rtstream/config/vod/server_port/*");
	bail_error(err);
bail:
	return err;
}

int
cli_nvsd_rtstream_listen_port_live_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
	int err = 0;
	cli_command *cmd = NULL;

	UNUSED_ARGUMENT(info);
	UNUSED_ARGUMENT(context);

	CLI_CMD_NEW;
	cmd->cc_words_str =             "delivery protocol rtsp listen live";
	cmd->cc_help_descr =            N_("Set listening port parameters");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =             "delivery protocol rtsp listen live port";
	cmd->cc_help_descr =            N_("Set listening port parameters");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =             "no delivery protocol rtsp listen live port";
	cmd->cc_help_descr =            N_("Reset listening port parameters");
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =             "no delivery protocol rtsp listen live port";
	cmd->cc_help_descr =            N_("Set default port - 554");
	//cmd->cc_flags =                 ccf_terminal;
	//cmd->cc_capab_required =        ccp_set_basic_curr;
	//cmd->cc_exec_callback  =        cli_nvsd_rtstream_set_vod_server_port;
	//cmd->cc_magic	   =	    	cid_default_server_port;
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =             "delivery protocol rtsp listen live port *";
	cmd->cc_flags =                 ccf_terminal | ccf_allow_extra_params;
	cmd->cc_capab_required =        ccp_set_basic_curr;
	cmd->cc_help_exp =              N_("<port>");
	cmd->cc_help_exp_hint =         N_("for example, listen port 80 81 90-100");
	cmd->cc_exec_operation =        cxo_set;
	cmd->cc_exec_callback  =        cli_nvsd_rtstream_live_port_config;
	cmd->cc_magic           =       cid_rtstream_port_add;
	cmd->cc_revmap_order =          cro_delivery;
	cmd->cc_revmap_type =           crt_none;
//	cmd->cc_revmap_names =          "/nkn/nvsd/rtstream/config/live/server_port/*/port";
//	cmd->cc_revmap_cmds =           "delivery protocol rtsp listen live port $v$";
	CLI_CMD_REGISTER;

	CLI_CMD_NEW;
	cmd->cc_words_str =             "no delivery protocol rtsp listen live port *";
	cmd->cc_flags =                 ccf_terminal | ccf_allow_extra_params;
	cmd->cc_capab_required =        ccp_set_basic_curr;
	cmd->cc_help_exp =              N_("<port>");
	cmd->cc_help_exp_hint =         N_("for example, 80 81 90-100");
	cmd->cc_exec_operation =        cxo_set;
	cmd->cc_exec_callback  =        cli_nvsd_rtstream_live_port_config;
	cmd->cc_magic	   =	    	cid_default_server_port;
	cmd->cc_revmap_type =           crt_none;
	CLI_CMD_REGISTER;

	bail_error(err);

bail:
	return err;
}


/* ------------------------------------------------------------------------- */
int
cli_nvsd_rtstream_live_port_config(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params)
{
    int err = 0;
    bn_binding_array *barr = NULL;
    bn_binding *binding = NULL;
    uint32 code = 0;
    uint32 num_params = 0;
    const char *param = NULL;
    uint32 i = 0;
    uint16 j = 0;
    char *p = NULL;
    const char *port = NULL;
    char *port_num = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd_line);

    port = tstr_array_get_str_quick(params, 0);
    bail_null(port);

    num_params = tstr_array_length_quick(params);

    //tstr_array_print(params, "param");

    switch(cmd->cc_magic) 
    {
    case cid_rtstream_port_add:
        
        err = bn_binding_array_new(&barr);
        bail_error(err);

        for (i = 0; i < num_params; i++) {

            param = tstr_array_get_str_quick(params, i);
            bail_null(param);

	    tbool dup_deleted = false;
	    tstring *msg = NULL;
            /* Check whether this is a range */
            if ( NULL != (p = strstr(param, "-"))) {
                /* p contains pointer to start of the next segment */
                uint16 start = strtol(param, &p, 10);
                uint16 end = strtol(p+1, NULL, 10);

                for (j = start; j <= end; j++) {

			port_num = smprintf("%d", j);

		    err = mdc_array_append_single(cli_mcc, 
				    "/nkn/nvsd/rtstream/config/live/server_port", 
				    "port",
				    bt_uint16,
				    port_num,
				    true, 
				    &dup_deleted,
				    &code,
				    &msg);
                    if (code != 0) {
			    cli_printf(_("error : %s\n"), ts_str(msg));
                        break;
                    }
		    safe_free(port_num);
                }
                if (code != 0) {
                    break;
                }
            }
            else {
                uint16 start = strtol(param, NULL, 10);
                
		port_num = smprintf("%d", start);

                err = mdc_array_append_single(cli_mcc, 
				    "/nkn/nvsd/rtstream/config/live/server_port", 
				    "port",
				    bt_uint16,
				    port_num,
				    true, 
				    &dup_deleted,
				    &code,
				    &msg);
                    if (code != 0) {
			    cli_printf(_("error : %s\n"), ts_str(msg));
                        break;
                    }
#if 0               
 err = bn_binding_new_uint16(&binding, "port", ba_value, 0, start);
                bail_error(err);

                err = bn_binding_array_append_takeover(barr, &binding);
                bail_error(err);

                err = mdc_array_append(cli_mcc, 
                        "/nkn/nvsd/rtstream/config/server_port", 
                        barr, true, 0, NULL, NULL, &code, NULL);
                bail_error(err);

                if (code != 0) {
                    break;
                }a
#endif
            }
        }

        break;

    case cid_rtstream_port_delete:
        
        err = bn_binding_array_new(&barr);
        bail_error(err);

        for (i = 0; i < num_params; i++) {

            param = tstr_array_get_str_quick(params, i);
            bail_null(param);

            /* Check whether this is a range */
            if ( NULL != (p = strstr(param, "-"))) {
                /* p contains pointer to start of the next segment */
                uint16 start = strtol(param, &p, 10);
                uint16 end = strtol(p+1, NULL, 10);

                for (j = start; j <= end; j++) {
                    char *n = smprintf("%d", j);;

                    err = mdc_array_delete_by_value_single(cli_mcc,
                            "/nkn/nvsd/rtstream/config/live/server_port", true, 
                            "port", bt_uint16, n, NULL, &code, NULL);
                    bail_error(err);

                    safe_free(n);
#if 0
                    err = bn_binding_new_uint16(&binding, "port", ba_value, 0, j);
                    bail_error(err);

                    err = bn_binding_array_append_takeover(barr, &binding);
                    bail_error(err);

                    err = mdc_array_delete_by_value(cli_mcc, 
                            "/nkn/nvsd/rtstream/config/server_port", 
                            true, barr, 0, NULL, NULL, &code, NULL);
                    bail_error(err);
#endif
                }
            }
            else {
                uint16 start = strtol(param, NULL, 10);
                
                err = bn_binding_new_uint16(&binding, "port", ba_value, 0, start);
                bail_error(err);

                err = bn_binding_array_append_takeover(barr, &binding);
                bail_error(err);

                err = mdc_array_delete_by_value_single(cli_mcc, 
                        "/nkn/nvsd/rtstream/config/live/server_port", 
                        true, "port", bt_uint16, param, NULL, &code, NULL);
                bail_error(err);
            }
        }

        break;
    }
    /* Call the standard cli processing */
    //err = cli_std_exec(data, cmd, cmd_line, params);
    //bail_error(err);

    /* Print the restart message */

 bail:

    safe_free(port_num);
    if (!err && !code) 
        cli_printf("warning: if command successful, please restart mod-delivery service for change to take effect\n");
    bn_binding_array_free(&barr);
    return(err);
}



/* ------------------------------------------------------------------------- */
int
cli_nvsd_rtstream_vod_port_config(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params)
{
    int err = 0;
    bn_binding_array *barr = NULL;
    bn_binding *binding = NULL;
    uint32 code = 0;
    uint32 num_params = 0;
    const char *param = NULL;
    uint32 i = 0;
    uint16 j = 0;
    char *p = NULL;
    const char *port = NULL;
    char *port_num = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd_line);

    port = tstr_array_get_str_quick(params, 0);
    bail_null(port);

    num_params = tstr_array_length_quick(params);

    //tstr_array_print(params, "param");

    switch(cmd->cc_magic) 
    {
    case cid_rtstream_port_add:
        
        err = bn_binding_array_new(&barr);
        bail_error(err);

        for (i = 0; i < num_params; i++) {

            param = tstr_array_get_str_quick(params, i);
            bail_null(param);

	    tbool dup_deleted = false;
	    tstring *msg = NULL;
            /* Check whether this is a range */
            if ( NULL != (p = strstr(param, "-"))) {
                /* p contains pointer to start of the next segment */
                int32 start = strtol(param, &p, 10);
                int32 end = strtol(p+1, NULL, 10);
		/*
		 * If any of the value is negative give out an error
		 */
		if( start < 0 || start > 65535 || end < 0 || end > 65535) {
			code = 1;
			cli_printf(_("error: %s\n"), "Out of range value given."
						"Enter values between 0 and 65535");
			goto bail;
		}

                for (j = start; j <= end; j++) {

			port_num = smprintf("%d", j);

		    err = mdc_array_append_single(cli_mcc, 
				    "/nkn/nvsd/rtstream/config/vod/server_port", 
				    "port",
				    bt_uint16,
				    port_num,
				    true, 
				    &dup_deleted,
				    &code,
				    &msg);
                    if (code != 0) {
			    cli_printf(_("error : %s\n"), ts_str(msg));
                        break;
                    }
		    safe_free(port_num);
                }
                if (code != 0) {
                    break;
                }
            }
            else {
                int32 start = strtol(param, NULL, 10);
		/*
		 * If any of the value is negative give out an error
		 */
		if( start < 0 || start > 65535 ) {
			code = 1;
			cli_printf(_("error: %s\n"), "Out of range value given."
						"Enter values between 0 and 65535");
			goto bail;
		}
                
		port_num = smprintf("%d", start);

                err = mdc_array_append_single(cli_mcc, 
				    "/nkn/nvsd/rtstream/config/vod/server_port", 
				    "port",
				    bt_uint16,
				    port_num,
				    true, 
				    &dup_deleted,
				    &code,
				    &msg);
                    if (code != 0) {
			    cli_printf(_("error : %s\n"), ts_str(msg));
                        break;
                    }
#if 0               
 err = bn_binding_new_uint16(&binding, "port", ba_value, 0, start);
                bail_error(err);

                err = bn_binding_array_append_takeover(barr, &binding);
                bail_error(err);

                err = mdc_array_append(cli_mcc, 
                        "/nkn/nvsd/rtstream/config/server_port", 
                        barr, true, 0, NULL, NULL, &code, NULL);
                bail_error(err);

                if (code != 0) {
                    break;
                }a
#endif
            }
        }

        break;

    case cid_rtstream_port_delete:
        
        err = bn_binding_array_new(&barr);
        bail_error(err);

        for (i = 0; i < num_params; i++) {

            param = tstr_array_get_str_quick(params, i);
            bail_null(param);

            /* Check whether this is a range */
            if ( NULL != (p = strstr(param, "-"))) {
                /* p contains pointer to start of the next segment */
                uint16 start = strtol(param, &p, 10);
                uint16 end = strtol(p+1, NULL, 10);

                for (j = start; j <= end; j++) {
                    char *n = smprintf("%d", j);;

                    err = mdc_array_delete_by_value_single(cli_mcc,
                            "/nkn/nvsd/rtstream/config/vod/server_port", true, 
                            "port", bt_uint16, n, NULL, &code, NULL);
                    bail_error(err);

                    safe_free(n);
#if 0
                    err = bn_binding_new_uint16(&binding, "port", ba_value, 0, j);
                    bail_error(err);

                    err = bn_binding_array_append_takeover(barr, &binding);
                    bail_error(err);

                    err = mdc_array_delete_by_value(cli_mcc, 
                            "/nkn/nvsd/rtstream/config/server_port", 
                            true, barr, 0, NULL, NULL, &code, NULL);
                    bail_error(err);
#endif
                }
            }
            else {
                uint16 start = strtol(param, NULL, 10);
                
                err = bn_binding_new_uint16(&binding, "port", ba_value, 0, start);
                bail_error(err);

                err = bn_binding_array_append_takeover(barr, &binding);
                bail_error(err);

                err = mdc_array_delete_by_value_single(cli_mcc, 
                        "/nkn/nvsd/rtstream/config/vod/server_port", 
                        true, "port", bt_uint16, param, NULL, &code, NULL);
                bail_error(err);
            }
        }

        break;
    }
    /* Call the standard cli processing */
    //err = cli_std_exec(data, cmd, cmd_line, params);
    //bail_error(err);

    /* Print the restart message */

 bail:

    safe_free(port_num);
    if (!err && !code) 
        cli_printf("warning: if command successful, please restart mod-delivery service for change to take effect\n");
    bn_binding_array_free(&barr);
    return(err);
}

int
cli_nvsd_rtsp_server_port_show(const bn_binding_array *bindings,
		uint32 idx, const bn_binding *binding,
		const tstring *name,
		const tstr_array *name_components,
		const tstring *name_last_part,
		const tstring *value, void *callback_data)
{
	int err = 0;
	uint16 port = 0;

	UNUSED_ARGUMENT(bindings);
	UNUSED_ARGUMENT(name);
	UNUSED_ARGUMENT(name_components);
	UNUSED_ARGUMENT(name_last_part);
	UNUSED_ARGUMENT(value);
	UNUSED_ARGUMENT(callback_data);

	if ((idx != 0) && ((idx % 3) == 0 )) {
		cli_printf(_("\n"));
	}

	err = bn_binding_get_uint16(binding, ba_value, NULL, &port);
	bail_error(err);

	err = cli_printf( _("     %-5d"), port);
	bail_error(err);
bail:
	return err;
}
