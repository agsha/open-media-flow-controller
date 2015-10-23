/*
 * Filename :   cli_nvsd_http_cmds.c
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
#include "clish_module.h"
#include "libnkncli.h"
#include "nkn_defs.h"
#include "nkn_mgmt_defs.h"
#include <ctype.h>

#if 0 /* Not sure if this is needed anymore - Ram (July 20th, 2010) */
static cli_help_callback cli_interface_help = NULL;
static cli_completion_callback cli_interface_completion = NULL;
#endif // 0
static int (*cli_interface_validate)(const char *ifname, tbool *ret_valid) = NULL;
extern int
cli_std_exec(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params);
int
cli_nkn_max_arp_entry_set(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params);
enum {
    cli_nvsd_http_if_all = 0,
    cli_nvsd_http_if_list,
};

enum {
    cid_http_port_add = 1,
    cid_http_port_delete = 2
};

enum {
    cid_req_method_delete = 3,
    cid_req_method_add = 4
};

int
cli_nvsd_http_print_restart_msg(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params);

int 
cli_proto_enter_prefix_mode(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int 
cli_nvsd_http_interface_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
int
cli_nvsd_http_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int cli_nvsd_delivery_enter_prefix_mode(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int cli_nvsd_http_show_cmd(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);


int 
cli_nvsd_http_show_content_types_cmd(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int 
cli_nvsd_http_content_type_help(
        void *data,
        cli_help_type help_type,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        void *context);


int 
cli_nvsd_http_content_type_completion(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions);

static int 
cli_nvsd_http_get_content_type_list(
        tstr_array **ret_headers,
        tbool   hide_display);

int 
cli_nvsd_http_header_help(
        void *data,
        cli_help_type help_type,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        void *context);


int 
cli_nvsd_http_header_completion(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions);

static int 
cli_nvsd_http_get_header_list(
        void *data,
        tstr_array **ret_headers,
        tbool   hide_display);


int 
cli_nvsd_http_set_file_ext(void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);
int
cli_nvsd_http_delete_method(const bn_binding_array *bindings,
                            uint32 idx, const bn_binding *binding,
                            const tstring *name,
                            const tstr_array *name_components,
                            const tstring *name_last_part,
                            const tstring *value, void *callback_data);

int
cli_nvsd_http_show_interface(const bn_binding_array *bindings,
                            uint32 idx, const bn_binding *binding,
                            const tstring *name,
                            const tstr_array *name_components,
                            const tstring *name_last_part,
                            const tstring *value, void *callback_data);
int
cli_nvsd_http_content_type_show(const bn_binding_array *bindings,
                            uint32 idx, const bn_binding *binding,
                            const tstring *name,
                            const tstr_array *name_components,
                            const tstring *name_last_part,
                            const tstring *value, void *callback_data);
int
cli_nvsd_http_header_suppress_show(const bn_binding_array *bindings,
                            uint32 idx, const bn_binding *binding,
                            const tstring *name,
                            const tstr_array *name_components,
                            const tstring *name_last_part,
                            const tstring *value, void *callback_data);

int
cli_nvsd_http_header_append_show(const bn_binding_array *bindings,
                            uint32 idx, const bn_binding *binding,
                            const tstring *name,
                            const tstr_array *name_components,
                            const tstring *name_last_part,
                            const tstring *value, void *callback_data);
int
cli_nvsd_http_header_set_show(const bn_binding_array *bindings,
                            uint32 idx, const bn_binding *binding,
                            const tstring *name,
                            const tstr_array *name_components,
                            const tstring *name_last_part,
                            const tstring *value, void *callback_data);

int
cli_nvsd_http_allow_req_method_show(const bn_binding_array *bindings,
                            uint32 idx, const bn_binding *binding,
                            const tstring *name,
                            const tstr_array *name_components,
                            const tstring *name_last_part,
                            const tstring *value, void *callback_data);
int
cli_nvsd_http_server_port_show(const bn_binding_array *bindings,
                            uint32 idx, const bn_binding *binding,
                            const tstring *name,
                            const tstr_array *name_components,
                            const tstring *name_last_part,
                            const tstring *value, void *callback_data);

int cli_nvsd_http_conn_pool_disable(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);
int
cli_nvsd_http_allow_req_methods(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params);
int
nkn_validate_port(const char *param, error_msg_t *err_msg);

int get_existing_indices(bn_binding_array *bindings, uint32_array **indices, tstr_array *t_indices);

int
find_matching_index(uint32 prt, bn_binding_array *bindings, uint32_array *indices, uint32 *ret_idx );


/*----------------------------------------------------------------------------
 * MODULE ENTRY POINT
 *---------------------------------------------------------------------------*/
int
cli_nvsd_http_cmds_init(
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
#if 0 /* Not sure if this is needed - Ram (July 20th, 2010) */
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
    cmd->cc_words_str =             "delivery protocol http";
    cmd->cc_help_descr =            N_("Configure delivery protocol options");
    cmd->cc_flags =                 ccf_terminal | ccf_prefix_mode_root;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_callback =         cli_nvsd_delivery_enter_prefix_mode;
    cmd->cc_revmap_order =          cro_delivery;
    //cmd->cc_revmap_type    =        crt_none;
    //cmd->cc_revmap_names =          "/nkn/nvsd/http/config/*";
    //cmd->cc_revmap_callback =       cli_nvsd_http_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol http";
    cmd->cc_help_descr =            N_("Clear delivery protocol options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http no";
    cmd->cc_help_descr =            N_("Disable/Negate protocol options");
    cmd->cc_req_prefix_count =      3;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http trace";
    cmd->cc_help_descr =            N_("Delivery Protocol Trace Flag");
    //cmd->cc_flags =                 ccf_terminal;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http trace enable";
    cmd->cc_help_descr =            N_("Enable the Delivery Protocol Trace");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_basic_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/http/config/trace/enabled";
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_type =             bt_bool;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http ipv6";
    cmd->cc_help_descr =            N_("Delivery Protocol IPv6 Flag");
    //cmd->cc_flags =                 ccf_terminal;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http ipv6 enable";
    cmd->cc_help_descr =            N_("Enable the Delivery Protocol IPv6");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_basic_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/http/config/ipv6/enabled";
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_delivery;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol http trace";
    cmd->cc_help_descr =            N_("Delivery Protocol Trace Flag");
    //cmd->cc_flags =                 ccf_terminal;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol http trace enable";
    cmd->cc_help_descr =            N_("Disable the Delivery Protocol Trace");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_basic_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/http/config/trace/enabled";
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_type =             bt_bool;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol http ipv6";
    cmd->cc_help_descr =            N_("Delivery Protocol IPv6 Flag");
    //cmd->cc_flags =                 ccf_terminal;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol http ipv6 enable";
    cmd->cc_help_descr =            N_("Disable the Delivery Protocol IPv6");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_basic_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/http/config/ipv6/enabled";
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_type =             bt_bool;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http connection";
    cmd->cc_help_descr =            N_("Configures Delivery Protocol connection Parameters");
    CLI_CMD_REGISTER;
	
    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http connection persistence";
    cmd->cc_help_descr =            N_("Configures Delivery Protocol connection Persistence");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http connection persistence num-requests";
    cmd->cc_help_descr =            N_("Configure no.of Persistent HTTP requests allowed per connection");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http connection persistence num-requests *";
    cmd->cc_help_exp =              N_("Configure between 0 to 100");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_basic_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/http/config/connection/persistence/num-requests";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_type =             bt_uint32;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_delivery;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http connection close";
    cmd->cc_help_descr =            N_("Configures the socket closing option");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http connection close use-reset";
    cmd->cc_help_descr =            N_("Configures the socket closing option as RST");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =	    ccp_set_basic_curr;
    cmd->cc_exec_operation =	    cxo_set;
    cmd->cc_exec_name =		    "/nkn/nvsd/http/config/connection/close/use-reset";
    cmd->cc_exec_value =	    "true";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =	    crt_auto;
    cmd->cc_revmap_order =	    cro_delivery;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol http connection";
    cmd->cc_help_descr =            N_("Configures the socket closing option");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol http connection close";
    cmd->cc_help_descr =            N_("Configures the socket closing option");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol http connection close use-reset";
    cmd->cc_help_descr =            N_("Configures the socket closing option as RST");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =	    ccp_set_basic_curr;
    cmd->cc_exec_operation =	    cxo_set;
    cmd->cc_exec_name =		    "/nkn/nvsd/http/config/connection/close/use-reset";
    cmd->cc_exec_value =	    "false";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =	    crt_auto;
    cmd->cc_revmap_order =	    cro_delivery;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http listen";
    cmd->cc_help_descr =            N_("Configure listen port parameters");
    //cmd->cc_revmap_order =          cro_delivery;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http listen port";
    cmd->cc_help_descr =            N_("Set listening port parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol http listen";
    cmd->cc_help_descr =            N_("Clear listen port parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol http listen port";
    cmd->cc_help_descr =            N_("Reset listening port parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol http listen port *";
    cmd->cc_help_exp =              N_("<port number or range>");
    cmd->cc_help_exp_hint =         N_("for example, listen port 80 81 90-100");
    cmd->cc_help_use_comp =         true;
    cmd->cc_comp_type =             cct_matching_values;
    cmd->cc_comp_pattern =          "/nkn/nvsd/http/config/server_port/*/port";
    cmd->cc_flags =                 ccf_terminal| ccf_allow_extra_params;
    cmd->cc_capab_required =        ccp_set_basic_curr;
    //cmd->cc_exec_operation =        cxo_set;
    //cmd->cc_exec_name      =        "/nkn/nvsd/http/config/server_port";
    //cmd->cc_exec_type      =        bt_uint16;
    //cmd->cc_exec_value     =        "80";
    cmd->cc_exec_callback  =        cli_nvsd_http_print_restart_msg;
    cmd->cc_magic           =       cid_http_port_delete;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http listen port *";
    cmd->cc_flags =                 ccf_terminal | ccf_allow_extra_params;
    cmd->cc_capab_required =        ccp_set_basic_curr;
    cmd->cc_help_exp =              N_("<port>");
    cmd->cc_help_exp_hint =         N_("for example, listen port 80 81 90-100");
    cmd->cc_exec_operation =        cxo_set;
    //cmd->cc_exec_name      =        "/nkn/nvsd/http/config/server_port";
    //cmd->cc_exec_type      =        bt_uint16;
    //cmd->cc_exec_value     =        "$1$";
    cmd->cc_exec_callback  =        cli_nvsd_http_print_restart_msg;
    cmd->cc_magic           =       cid_http_port_add;
    cmd->cc_revmap_order =          cro_delivery;
    cmd->cc_revmap_type =           crt_manual;
    cmd->cc_revmap_names =          "/nkn/nvsd/http/config/server_port/*/port";
    cmd->cc_revmap_cmds =           "delivery protocol http listen port $v$";
    CLI_CMD_REGISTER;



    /*----------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http rate-control";
    cmd->cc_help_descr =            N_("Configure rate control");
    cmd->cc_flags =                 ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http rate-control 300000";
    cmd->cc_help_descr =            N_("Set default value - 300000");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_type =             bt_uint64;
    cmd->cc_exec_name =             "/nkn/nvsd/http/config/rate_control";
    cmd->cc_exec_value =            "300000";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http rate-control *";
    cmd->cc_help_exp =              N_("<number>");
    cmd->cc_help_exp_hint =         N_("Number");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_type =             bt_uint64;
    cmd->cc_exec_name =             "/nkn/nvsd/http/config/rate_control";
    cmd->cc_exec_value =            "$1$";
    CLI_CMD_REGISTER;
    /*----------------------------------------------------------------------*/

    /*----------------------------------------------------------------------*/

    /*----------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http om-conn-limit";
    cmd->cc_help_descr =            N_("Configure OM Connection limit");
    cmd->cc_flags =                 ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http om-conn-limit *";
    cmd->cc_help_exp =              N_("<number>");
    cmd->cc_help_exp_hint =         N_("Max number of connections");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/http/config/om_conn_limit";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_type =             bt_uint32;
    cmd->cc_revmap_type = 		crt_auto;
    cmd->cc_revmap_order = 		cro_delivery;
    CLI_CMD_REGISTER;
    /*----------------------------------------------------------------------*/
#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http header";
    cmd->cc_help_descr =            N_("Configure HTTP header options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http header append";
    cmd->cc_help_descr =            N_("Add a HTTP header");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http header append none";
    cmd->cc_help_descr =            N_("Add no HTTP headers");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_callback =         cli_nvsd_http_hdr_append_none_cb;
    CLI_CMD_REGISTER;
#endif
    /*----------------------------------------------------------------------*/


    /*----------------------------------------------------------------------*/
#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http header";
    cmd->cc_help_descr =            N_("Configure HTTP header options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http header suppress";
    cmd->cc_help_descr =            N_("Remove a HTTP header");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http header suppress none";
    cmd->cc_help_descr =            N_("Remove no HTTP headers");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_callback =         cli_nvsd_http_hdr_supress_none_cb;
    CLI_CMD_REGISTER;
#endif
    /*----------------------------------------------------------------------*/



    /*----------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http file-type";
    cmd->cc_help_descr =            N_("Configure content type options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol http file-type";
    cmd->cc_help_descr =            N_("Clear content type options");
    CLI_CMD_REGISTER;

#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http file-type .html";
    cmd->cc_help_descr =            N_("Default file type - .html");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http file-type .html set";
    cmd->cc_help_descr =            N_("Content MIME type to add");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http file-type .html set content-type";
    cmd->cc_help_descr =            N_("Content MIME type to add");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http file-type .html set content-type text/html";
    cmd->cc_help_descr =            N_("Default content-type for .html - text/html");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_type =             bt_string;
    cmd->cc_exec_name =             "/nkn/nvsd/http/config/content/type/.html/mime";
    cmd->cc_exec_value =            "text/html";
    //cmd->cc_revmap_type =           crt_auto;
    CLI_CMD_REGISTER;
#endif
    /*----------------------------------------------------------------------*/

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http file-type *";
    cmd->cc_help_exp =              N_("<type>");
    cmd->cc_help_exp_hint =         N_("filename type, example html");
    //cmd->cc_help_callback =         cli_nvsd_http_fileext_help;
    //cmd->cc_comp_callback =         cli_nvsd_http_fileext_help_completion;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol http file-type *";
    cmd->cc_help_exp =              N_("<type>");
    cmd->cc_help_exp_hint =         N_("filename type, example html");
    cmd->cc_comp_type =             cct_matching_names;
    cmd->cc_comp_pattern =          "/nkn/nvsd/http/config/content/type/*";
    //cmd->cc_help_callback =         cli_nvsd_http_fileext_help;
    //cmd->cc_comp_callback =         cli_nvsd_http_fileext_help_completion;
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_operation =        cxo_delete;
    cmd->cc_exec_name =             "/nkn/nvsd/http/config/content/type/$1$";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_type =             bt_string;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http file-type * content-type";
    cmd->cc_help_descr =            N_("Content MIME type to add");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http file-type * content-type *";
    cmd->cc_help_exp =              N_("<type>");
    cmd->cc_help_exp_hint =         N_("HTTP Content-Type header value in response. example \"text/html\"");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_callback =         cli_nvsd_http_set_file_ext;
    cmd->cc_revmap_order =          cro_delivery;
    cmd->cc_revmap_type =           crt_manual;
    cmd->cc_revmap_names =          "/nkn/nvsd/http/config/content/type/*/mime";
    cmd->cc_revmap_cmds =           "delivery protocol http file-type $7$ content-type $v$";
    CLI_CMD_REGISTER;
#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http header";
    cmd->cc_help_descr =            N_("Configure HTTP header options");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol http header";
    cmd->cc_help_descr =            N_("Disable/Delete HTTP header options");
    CLI_CMD_REGISTER;
#endif

#if 0 /* Only supporting "set" as per Kumar's CLI doc - April 13th 2009 - Ram */
    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http header append";
    cmd->cc_help_descr =            N_("Add a HTTP header");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol http header append";
    cmd->cc_help_descr =            N_("Delete a HTTP header that is appended"
                                       " in response messages");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http header append *";
    cmd->cc_help_exp =              N_("<header>");
    cmd->cc_help_exp_hint =         N_("HTTP header name");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http header append * *";
    cmd->cc_help_exp =              N_("<value>");
    cmd->cc_help_exp_hint =         N_("Header value to be appended");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_type =             bt_string;
    cmd->cc_exec_name =             "/nkn/nvsd/http/config/response/header/append/$1$/value";
    cmd->cc_exec_value =            "$2$";
    cmd->cc_revmap_order =          cro_delivery;
    cmd->cc_revmap_type =           crt_manual;
    cmd->cc_revmap_names =          "/nkn/nvsd/http/config/response/header/append/*/value";
    cmd->cc_revmap_cmds =           "delivery protocol http header append $8$ $v$";
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol http header append *";
    cmd->cc_help_exp =              N_("<header>");
    cmd->cc_help_exp_hint =         N_("Header to be deleted from list of "
                                       "appended headers in response message");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_operation =        cxo_delete;
    cmd->cc_exec_type =             bt_string;
    cmd->cc_exec_name =             "/nkn/nvsd/http/config/response/header/append/$1$";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_help_callback =         cli_nvsd_http_header_help;
    cmd->cc_help_data =             (void*) "/nkn/nvsd/http/config/response/header/append";
    cmd->cc_comp_callback =         cli_nvsd_http_header_completion;
    cmd->cc_comp_data =             (void*) "/nkn/nvsd/http/config/response/header/append";
    CLI_CMD_REGISTER;
#endif /* 0 */


#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http header suppress";
    cmd->cc_help_descr =            N_("Supress appending header in response");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol http header suppress";
    cmd->cc_help_descr =            N_("Delete a header from the suppress list");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http header suppress *";
    cmd->cc_help_exp =              N_("<header>");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_type =             bt_string;
    cmd->cc_exec_name =             "/nkn/nvsd/http/config/response/header/supress/$1$";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_revmap_order =          cro_delivery;
    cmd->cc_revmap_type =           crt_manual;
    cmd->cc_revmap_names =          "/nkn/nvsd/http/config/response/header/supress/*";
    cmd->cc_revmap_cmds =           "delivery protocol http header suppress $v$";
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol http header suppress *";
    cmd->cc_help_exp =              N_("<header>");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_operation =        cxo_delete;
    cmd->cc_exec_type =             bt_string;
    cmd->cc_exec_name =             "/nkn/nvsd/http/config/response/header/supress/$1$";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_help_callback =         cli_nvsd_http_header_help;
    cmd->cc_help_data =             (void*) "/nkn/nvsd/http/config/response/header/supress";
    cmd->cc_comp_callback =         cli_nvsd_http_header_completion;
    cmd->cc_comp_data =             (void*) "/nkn/nvsd/http/config/response/header/supress";
    CLI_CMD_REGISTER;



    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http header set";
    cmd->cc_help_descr =            N_("Set header in response");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol http header set";
    cmd->cc_help_descr =            N_("Delete a header from the set list");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http header set *";
    cmd->cc_help_exp =              N_("<header>");
    cmd->cc_help_exp_hint =         N_("Header");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http header set * *";
    cmd->cc_help_exp =              N_("<value>");
    cmd->cc_help_exp_hint =         N_("Header value to be set");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_type =             bt_string;
    cmd->cc_exec_name =             "/nkn/nvsd/http/config/response/header/set/$1$/value";
    cmd->cc_exec_value =            "$2$";
    cmd->cc_revmap_order =          cro_delivery;
    cmd->cc_revmap_type =           crt_manual;
    cmd->cc_revmap_names =          "/nkn/nvsd/http/config/response/header/set/*/value";
    cmd->cc_revmap_cmds =           "delivery protocol http header set $8$ $v$";
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol http header set *";
    cmd->cc_help_exp =              N_("<header>");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_operation =        cxo_delete;
    cmd->cc_exec_type =             bt_string;
    cmd->cc_exec_name =             "/nkn/nvsd/http/config/response/header/set/$1$";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_help_callback =         cli_nvsd_http_header_help;
    cmd->cc_help_data =             (void*) "/nkn/nvsd/http/config/response/header/set";
    cmd->cc_comp_callback =         cli_nvsd_http_header_completion;
    cmd->cc_comp_data =             (void*) "/nkn/nvsd/http/config/response/header/set";
    CLI_CMD_REGISTER;
#endif

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http interface";
    cmd->cc_help_descr =            N_("Configure interfaces for http");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol http interface";
    cmd->cc_help_descr =            N_("Reset interfaces for http");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http interface *";
    cmd->cc_help_exp =              N_("<interface name>");
    cmd->cc_help_exp_hint =         N_(" for example eth0 ");
    cmd->cc_flags =                 ccf_terminal | ccf_allow_extra_params;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_use_comp =         true;
    cmd->cc_comp_type =             cct_matching_names;
    cmd->cc_comp_pattern =          "/net/interface/config/*";
    cmd->cc_exec_callback =         cli_nvsd_http_interface_config;
    cmd->cc_magic =                 cli_nvsd_http_if_list;
    cmd->cc_revmap_order =          cro_delivery;
    cmd->cc_revmap_type =           crt_manual;
    cmd->cc_revmap_names =          "/nkn/nvsd/http/config/interface/*";
    cmd->cc_revmap_cmds =           "delivery protocol http interface $v$";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol http interface *";
    cmd->cc_help_exp =              N_("<interface name>");
    cmd->cc_help_exp_hint =         N_(" for example eth0 ");
    cmd->cc_flags =                 ccf_terminal | ccf_allow_extra_params; 
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_help_use_comp =         true;
    cmd->cc_comp_type =             cct_matching_names;
    cmd->cc_comp_pattern =          "/nkn/nvsd/http/config/interface/*";
    cmd->cc_exec_callback =         cli_nvsd_http_interface_config;
    cmd->cc_magic =                 cli_nvsd_http_if_list;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http transparent";
    cmd->cc_help_descr =	    "Configure interfaces for t-proxy";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http transparent *";
    cmd->cc_help_exp =              N_("<interface list>");
    cmd->cc_help_exp_hint =         N_("interface name, for example eth0,eth1,eth2... ");
    cmd->cc_help_use_comp =         true;
    cmd->cc_comp_type =             cct_matching_names;
    cmd->cc_comp_pattern =          "/net/interface/config/*";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http transparent * enable";
    cmd->cc_help_descr =	    "Enables t-proxy on this interface";
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =	    cxo_create;
    cmd->cc_exec_name =		    "/nkn/nvsd/http/config/t-proxy/$1$";
    cmd->cc_exec_value =	    "$1$";
    cmd->cc_exec_type =		    bt_string;
    cmd->cc_revmap_order =          cro_delivery;
    cmd->cc_revmap_type =           crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http transparent * disable";
    cmd->cc_help_descr =	    "Disables t-proxy on this interface";
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =	    cxo_delete;
    cmd->cc_exec_name =		    "/nkn/nvsd/http/config/t-proxy/$1$";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http conn-pool";
    cmd->cc_help_descr =            N_("Configure Connection Pooling parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http conn-pool origin";
    cmd->cc_help_descr =            N_("Configure Origin side Connection pooling parmeters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"delivery protocol http conn-pool max-arp-entry";
    cmd->cc_help_descr = 	N_("Configure maximum number of ARP entries in the ARP Cache Table");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"delivery protocol http conn-pool max-arp-entry *";
    cmd->cc_help_exp = 		N_("<number>");
    cmd->cc_help_exp_hint = 	N_("Number of connections, Default: 1024, [64 - 16384] ");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/http/config/conn-pool/max-arp-entry";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_type =             bt_uint32;
    cmd->cc_exec_callback =         cli_nkn_max_arp_entry_set;
    cmd->cc_revmap_type =           crt_manual;
    cmd->cc_revmap_order =          cro_delivery;
    cmd->cc_revmap_names =          "/nkn/nvsd/http/config/conn-pool/max-arp-entry";
    cmd->cc_revmap_cmds =           "delivery protocol http conn-pool max-arp-entry $v$";
    cmd->cc_revmap_suborder =       2;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http conn-pool origin enable";
    cmd->cc_help_descr =            N_("Enable Origin side connection pooling");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/http/config/conn-pool/origin/enabled";
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_delivery;
    //cmd->cc_revmap_callback =       cli_nvsd_http_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http conn-pool origin disable";
    cmd->cc_help_descr =            N_("Disable Origin side connection pooling");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/http/config/conn-pool/origin/enabled";
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_exec_callback = 		cli_nvsd_http_conn_pool_disable;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_delivery;
    cmd->cc_revmap_suborder =       2;
    CLI_CMD_REGISTER;

    /*
     * this command would enable mfc to revalidate objects using GET request
     * HEAD requests, as some websites doesn't responds to HEAD requests
     */
    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http revalidate-get";
    cmd->cc_help_descr =            N_("Enable Origin side revalidation request type");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/http/config/reval_meth_get";
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_delivery;
    CLI_CMD_REGISTER;

    /*
     * This command would disable the revalidation method as GET. This would
     * again enable MFC to use HEAD method for cache revalidation
     */
    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol http revalidate-get";
    cmd->cc_help_descr =            N_("Reset Origin side revalidation request type to HEAD");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_operation =        cxo_reset;
    cmd->cc_exec_name =             "/nkn/nvsd/http/config/reval_meth_get";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http conn-pool origin timeout";
    cmd->cc_help_descr =            N_("Configure timeout value for Origin side connection pooled sockets");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http conn-pool origin timeout *";
    cmd->cc_help_exp =              N_("<seconds>");
    cmd->cc_help_exp_hint =         N_("Timeout, Default: 90 seconds");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/http/config/conn-pool/origin/timeout";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_type =             bt_uint32;
    cmd->cc_revmap_type =           crt_manual;
    cmd->cc_revmap_order =          cro_delivery;
    cmd->cc_revmap_names =          "/nkn/nvsd/http/config/conn-pool/origin/timeout";
    cmd->cc_revmap_cmds =           "delivery protocol http conn-pool origin timeout $v$";
    cmd->cc_revmap_suborder =       1;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"delivery protocol http conn-pool origin max-conn";
    cmd->cc_help_descr = 	N_("Configure maximum number of connections that can be"
		    " opened to the origin server concurrently");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"delivery protocol http conn-pool origin max-conn *";
    cmd->cc_help_exp = 		N_("<number>");
    cmd->cc_help_exp_hint = 	N_("Number of connections, Default: 4096, max: 128000, 0: demand driven");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/http/config/conn-pool/origin/max-conn";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_type =             bt_uint32;
    cmd->cc_revmap_type =           crt_manual;
    cmd->cc_revmap_order =          cro_delivery;
    cmd->cc_revmap_names =          "/nkn/nvsd/http/config/conn-pool/origin/max-conn";
    cmd->cc_revmap_cmds =           "delivery protocol http conn-pool origin max-conn $v$";
    cmd->cc_revmap_suborder =       2;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http req-length";
    cmd->cc_help_descr =            N_("Configure Request length parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http req-length maximum";
    cmd->cc_help_descr =            N_("Configure Request length value");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http req-length maximum *";
    cmd->cc_help_exp =              N_("<bytes>");
    cmd->cc_help_exp_hint =         N_("Set Maximum Request length value [1 - 32768]");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_type =             bt_uint32;
    cmd->cc_exec_name =             "/nkn/nvsd/http/config/max_http_req_size";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_revmap_order =          cro_delivery;
    cmd->cc_revmap_type =           crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol http req-length";
    cmd->cc_help_descr =            N_("Reset request length parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol http req-length maximum";
    cmd->cc_help_descr =            N_("Reset request length value to default (16384)");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_reset;
    cmd->cc_exec_type =             bt_uint32;
    cmd->cc_exec_name =             "/nkn/nvsd/http/config/max_http_req_size";
    cmd->cc_exec_value =            "16384";
    cmd->cc_revmap_type =           crt_none;
    CLI_CMD_REGISTER;






#if 0 
     CLI_CMD_NEW;
     cmd->cc_words_str =             "delivery protocol http rate-control *";
     cmd->cc_help_exp =              N_("<number>");
     cmd->cc_help_exp_hint =         N_("Number");
     cmd->cc_flags =                 ccf_terminal;
     cmd->cc_capab_required =        ccp_set_priv_curr;
     cmd->cc_exec_operation =        cxo_set;
     cmd->cc_exec_type =             bt_uint64;
     cmd->cc_exec_name =             "/nkn/nvsd/http/config/rate_control";
     cmd->cc_exec_value =            "$1$";
     CLI_CMD_REGISTER;
#endif     
     /*----------------------------------------------------------------------*/

    CLI_CMD_NEW;
    cmd->cc_words_str =             "show delivery";
    cmd->cc_help_descr =            N_("Display delivery configuration");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "show delivery protocol";
    cmd->cc_help_descr =            N_("Display delivery protocol configuration options");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "show delivery protocol http";
    cmd->cc_help_descr =            N_("Display HTTP configuration options");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_query_rstr_curr;
    cmd->cc_exec_callback =         cli_nvsd_http_show_cmd;
    CLI_CMD_REGISTER;



#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str =             "show http";
    cmd->cc_help_descr =            N_("Display http server configuration");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_query_rstr_curr;
    cmd->cc_exec_callback =         cli_nvsd_http_show_cmd;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "show http content-type";
    cmd->cc_help_descr =            N_("Display http registered content-types");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_query_rstr_curr;
    cmd->cc_exec_callback =         cli_nvsd_http_show_content_types_cmd;
    CLI_CMD_REGISTER;
#endif
    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http allow-req";
    cmd->cc_help_descr =            N_("Configure the request methods allowded");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol http  allow-req";
    cmd->cc_help_descr =            N_("Clear the allowed request methods");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol http allow-req all";
    cmd->cc_help_descr =             N_("Disallow all the request methods");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_basic_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name      =        "/nkn/nvsd/http/config/allow_req/all";
    cmd->cc_exec_type      =        bt_bool;
    cmd->cc_exec_value     =        "false";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http allow-req all" ;
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_basic_curr;
    cmd->cc_help_descr =            N_("Allow all the request methods");
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name      =        "/nkn/nvsd/http/config/allow_req/all";
    cmd->cc_exec_type      =        bt_bool;
    cmd->cc_exec_value     =        "true";
    cmd->cc_revmap_order =          cro_delivery;
    cmd->cc_revmap_type =           crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http allow-req method ";
    cmd->cc_help_descr =            N_("Configure the request methods allowed");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol http  allow-req method";
    cmd->cc_help_descr =            N_("Clear the allowed request methods");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol http allow-req method *";
    cmd->cc_help_exp =              N_("<request method>");
    cmd->cc_help_use_comp =         true;
    cmd->cc_comp_type =             cct_matching_values;
    cmd->cc_comp_pattern =          "/nkn/nvsd/http/config/allow_req/method/*";
    cmd->cc_flags =                 ccf_terminal | ccf_allow_extra_params;
    cmd->cc_capab_required =        ccp_set_basic_curr;
    cmd->cc_exec_callback  =        cli_nvsd_http_allow_req_methods;
    cmd->cc_magic           =       cid_req_method_delete;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http allow-req method *";
    cmd->cc_flags =                 ccf_terminal | ccf_allow_extra_params;
    cmd->cc_help_exp_hint =         N_("Add request method(s)");
    cmd->cc_help_use_comp =   	    true;
    cmd->cc_help_exp =              N_("<req-method>");
    cmd->cc_capab_required =        ccp_set_basic_curr;
    cmd->cc_comp_type =             cct_matching_values;
    cmd->cc_comp_pattern =          "/nkn/nvsd/http/config/allow_req/method/*";
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_callback  =        cli_nvsd_http_allow_req_methods;
    cmd->cc_magic           =       cid_req_method_add;
    cmd->cc_revmap_order =          cro_delivery;
    cmd->cc_revmap_type =           crt_manual;
    cmd->cc_revmap_names =          "/nkn/nvsd/http/config/allow_req/method/*";
    cmd->cc_revmap_cmds =           "delivery protocol http allow-req method $v$";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http filter";
    cmd->cc_help_descr =            N_("Keyword directive for enabling URL filtering");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http filter mode";
    cmd->cc_help_descr =            N_("Keyword to specify the way HTTP packets are handled for filtering");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http filter mode proxy";
    cmd->cc_help_descr =            N_("HTTP packets are handled by a HTTP proxy listening on ports configured for HTTP");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =	    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	    cxo_set;
    cmd->cc_exec_name =		    "/nkn/nvsd/http/config/filter/mode";
    cmd->cc_exec_value =	    "proxy";
    cmd->cc_exec_type =             bt_string;
    cmd->cc_revmap_type =	    crt_auto;
    cmd->cc_revmap_order =	    cro_delivery;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol http filter mode packet";
    cmd->cc_help_descr =            N_("System is directed to operate without TCP and HTTP termination with packet mode only");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =	    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	    cxo_set;
    cmd->cc_exec_name =		    "/nkn/nvsd/http/config/filter/mode";
    cmd->cc_exec_value =	    "packet";
    cmd->cc_exec_type =             bt_string;
    cmd->cc_revmap_type =	    crt_auto;
    cmd->cc_revmap_order =	    cro_delivery;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol http filter";
    cmd->cc_help_descr =            N_("Reset Filter mode to default");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =	    ccp_set_rstr_curr;
    cmd->cc_exec_operation =	    cxo_set;
    cmd->cc_exec_name =		    "/nkn/nvsd/http/config/filter/mode";
    cmd->cc_exec_value =	    "cache";
    cmd->cc_exec_type =             bt_string;
    cmd->cc_revmap_type =	    crt_auto;
    cmd->cc_revmap_order =	    cro_delivery;
    CLI_CMD_REGISTER;

    err = cli_revmap_ignore_bindings_va(12, 
            "/nkn/nvsd/http/config/content/type/*",
            "/nkn/nvsd/http/config/content/type/*/allow",
            "/nkn/nvsd/http/config/response/header/append/*",
            "/nkn/nvsd/http/config/response/header/set/*",
	    "/nkn/nvsd/http/config/response/header/supress/*",
            "/nkn/nvsd/http/config/rate_control",
            "/nkn/nvsd/http/config/trace/enabled",
            "/nkn/nvsd/http/config/max_http_req_size",
            "/nkn/nvsd/http/config/server_port/*",
            "/nkn/nvsd/http/config/allow_req/method/*",
            "/nkn/nvsd/http/config/reval_meth_get",
            "/nkn/nvsd/http/config/allow_req/all");


bail:
    return err;
}

/* ------------------------------------------------------------------------- */
int
cli_nvsd_http_print_restart_msg(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params)
{
    int err = 0;
    bn_binding_array *barr = NULL, *bindings = NULL;
    uint32 code = 0, i = 0, k = 0,indices_len = 0, last_idx = 0, tot_ports = 0, filled_idx = 0, free_idx = 0, new_idx=0;
    uint32 num_params = 0;
    const char *param = NULL;
    uint16 j = 0;
    char *p = NULL;
    const char *port = NULL;
    tstring *msg = NULL;
    str_value_t port_num = {0};
    uint32_array *port_numbers = NULL, *indices = NULL;
    error_msg_t err_msg = {0};
    bn_request *req = NULL;
    tstr_array *t_indices = NULL;
    node_name_t node_name = {0};
    uint32 prt = 0,port_idx = 0;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd_line);

    port = tstr_array_get_str_quick(params, 0);
    bail_null(port);

    num_params = tstr_array_length_quick(params);

    err = bn_binding_array_new(&barr);
    bail_error(err);

    err = uint32_array_new(&port_numbers);
    bail_error(err);

    err = bn_set_request_msg_create(&req, 0);
    bail_error(err);

    for (i = 0; i < num_params; i++) {

        param = tstr_array_get_str_quick(params, i);
        bail_null(param);

        code = nkn_validate_port(param, &err_msg);
        if (code) {
	    cli_printf_error("%s",err_msg);
            goto bail;
	}

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
            if ( end < start ) {
                    code =1;
                    cli_printf_error("End value of range is less than the start value");
                    goto bail;
            }
            for (j = start; j <= end; j++) {
                err = uint32_array_append(port_numbers,j);
                bail_error(err);
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

            err = uint32_array_append(port_numbers,start);
            bail_error(err);
        }
    }

    err = mdc_get_binding_children(cli_mcc, &code, NULL, true,
                                &bindings, true, true,
                                "/nkn/nvsd/http/config/server_port");
    if (code )
        goto bail;

    err = get_existing_indices(bindings, &indices, t_indices);
    bail_error(err);

    indices_len = uint32_array_length_quick(indices);
    tot_ports = uint32_array_length_quick(port_numbers);


    switch(cmd->cc_magic) 
    {
    case cid_http_port_add:
        
        err = uint32_array_insert_sorted(indices, 0);
        bail_error(err);

	indices_len++; 
	err = uint32_array_get_max(indices, &last_idx);
	bail_error(err);

	err = uint32_array_get_first_absent(indices, &new_idx);
	bail_error(err);

	for ( k = 0, filled_idx = 0 ; k < tot_ports ; k++, port_idx = 0) {

	    prt = uint32_array_get_quick(port_numbers, k);	
	    err = find_matching_index(prt, bindings, indices, &port_idx);
	    bail_error(err);

	    if (! port_idx ) {
		new_idx += filled_idx;
		for( i = 0 ; i < last_idx ; i++) {
		    free_idx = uint32_array_get_quick(indices,i);
		    if ( new_idx == free_idx)
		 	new_idx++;
		}
	        snprintf(port_num, sizeof(port_num), "%d", prt);
	        snprintf(node_name, sizeof(node_name), "/nkn/nvsd/http/config/server_port/%d/port",new_idx);
	    	   
	        err = bn_set_request_msg_append_new_binding(req, bsso_modify,0,
		        	    node_name, bt_uint16, 0, port_num, NULL);
	        bail_error(err);
		filled_idx++;
	    }
	}

	err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, &code, &msg);
	if ( !code ) {
		cli_printf_error("%s",ts_str(msg));
		goto bail;
	}
	
        break;

    case cid_http_port_delete:
        
	for (i = 0; i < tot_ports ; i++, port_idx=0 ) { 
	    prt = uint32_array_get_quick(port_numbers,i);
	     
	    err = find_matching_index(prt, bindings, indices, &port_idx );
	    bail_error(err);

	    if( port_idx) {
                snprintf(node_name, sizeof(node_name), "/nkn/nvsd/http/config/server_port/%d",
                                        port_idx);

                err = bn_set_request_msg_append_new_binding(req, bsso_delete,0,
                                node_name, bt_uint16, 0, "", NULL);
                bail_error(err);
	    }

        }
        err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, &code, &msg);
        if ( !code ) {
                cli_printf_error("%s",ts_str(msg));
                goto bail;
        }



        break;
    }
    /* Call the standard cli processing */
    //err = cli_std_exec(data, cmd, cmd_line, params);
    //bail_error(err);

    /* Print the restart message */

 bail:
    if (!err && !code) 
        cli_printf("warning: if command successful, please restart mod-delivery service for change to take effect\n");
    ts_free(&msg);
    bn_request_msg_free(&req);
    bn_binding_array_free(&barr);
    bn_binding_array_free(&bindings);
    uint32_array_free(&indices);
    uint32_array_free(&port_numbers);
    tstr_array_free(&t_indices);

    return(err);
}


/*--------------------------------------------------------------------------*/
int
cli_nvsd_http_show_interface(const bn_binding_array *bindings,
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
cli_nvsd_http_content_type_show(const bn_binding_array *bindings,
                            uint32 idx, const bn_binding *binding,
                            const tstring *name,
                            const tstr_array *name_components,
                            const tstring *name_last_part,
                            const tstring *value, void *callback_data)
{
    int err = 0;
    tstring *type = NULL;

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
cli_nvsd_http_header_suppress_show(const bn_binding_array *bindings,
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
cli_nvsd_http_header_append_show(const bn_binding_array *bindings,
                            uint32 idx, const bn_binding *binding,
                            const tstring *name,
                            const tstr_array *name_components,
                            const tstring *name_last_part,
                            const tstring *value, void *callback_data)
{
    int err = 0;
    tstring *header = NULL;

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
cli_nvsd_http_header_set_show(const bn_binding_array *bindings,
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


int
cli_nvsd_http_server_port_show(const bn_binding_array *bindings,
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

    if ((idx != 0) && ((idx % 3) == 0)) {
        cli_printf(_("\n"));
    }

    err = bn_binding_get_uint16(binding, ba_value, NULL, &port);
    bail_error(err);

    err = cli_printf(
            _("     %-5d"), port);
    bail_error(err);


bail:
    return err;

}

/*--------------------------------------------------------------------------*/
int cli_nvsd_http_show_cmd(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    uint32 num_intfs = 0;
    uint32 num_ctypes = 0;
    uint32 num_ports = 0;
    uint32 num_method = 0;
    char *conn_pool_status = NULL;
    tbool t_conn_pool_status = true;
    tbool t_allow_all_method = false;
    char *allow_all_method = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    err = cli_printf(
            _("Delivery Protocol : HTTP\n"));
    bail_error(err);

    err = cli_printf(
            _("Server Ports: \n"));
    bail_error(err);

    err = mdc_foreach_binding(cli_mcc, "/nkn/nvsd/http/config/server_port/*/port", NULL,
            cli_nvsd_http_server_port_show, NULL, &num_ports);
    bail_error(err);
    if (num_ports == 0) {
        cli_printf(
                _("     NONE\n"));
    }

    err = cli_printf(
            _("\nInterfaces:\n"));
    bail_error(err);
    err = mdc_foreach_binding(cli_mcc,
            "/nkn/nvsd/http/config/interface/*", NULL,
            cli_nvsd_http_show_interface, NULL, &num_intfs);
    bail_error(err);

    if (num_intfs == 0) {
        err = cli_printf(
                _("     Default (All Configured interfaces)\n"));
        bail_error(err);
    }

    err = cli_printf(
            _("\nInterfaces with T-proxy enabled: \n"));
    bail_error(err);
    err = mdc_foreach_binding(cli_mcc,
            "/nkn/nvsd/http/config/t-proxy/*", NULL,
            cli_nvsd_http_show_interface, NULL, &num_intfs);
    bail_error(err);

    if (num_intfs == 0) {
        err = cli_printf(
                _("     NONE\n"));
        bail_error(err);
    }
    err = cli_printf(
            _("\nFile Extension to Content-Type Map:\n"));
    bail_error(err);
    err = mdc_foreach_binding(cli_mcc, 
            "/nkn/nvsd/http/config/content/type/*", NULL,
            cli_nvsd_http_content_type_show, NULL, &num_ctypes);
    bail_error(err);
    if( num_ctypes == 0 ) {
        err = cli_printf(
                _("     None\n"));
        bail_error(err);
    }
#if 0	/* Removed due to BZ 1863 */
    err = cli_printf(
            _("\nSuppressed Header Name:\n"));
    bail_error(err);
    err = mdc_foreach_binding(cli_mcc,
            "/nkn/nvsd/http/config/response/header/supress/*", NULL,
            cli_nvsd_http_header_suppress_show, NULL, &num_hdrs);
    bail_error(err);
    if (num_hdrs == 0) {
        err = cli_printf(
                _("     None\n"));
        bail_error(err);
    }
#endif

#if 0 /* The append command has been removed - Ram Apil 14th 2009 */
    err = mdc_foreach_binding(cli_mcc, 
            "/nkn/nvsd/http/config/response/header/append/*", NULL,
            cli_nvsd_http_header_append_show, NULL, &num_hdrs);
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

#if 0 /* Removed due to BZ 1863 - Dhruva */
    err = cli_printf(
                _("\nSet Headers:\n"));
    bail_error(err);
    
    err = mdc_foreach_binding(cli_mcc, 
            "/nkn/nvsd/http/config/response/header/set/*", NULL, 
            cli_nvsd_http_header_set_show, NULL, &num_hdrs);
    bail_error(err);
    if (num_hdrs == 0) {
        err = cli_printf(
                _("     None\n"));
        bail_error(err);
    }
#endif

    /* Show the trace enabled flag */
    cli_printf_query(_(
                "\nTrace Flag Enabled: #/nkn/nvsd/http/config/trace/enabled#\n"));

    /* Show the ipv6 enabled flag */
    cli_printf_query(_(
                "\nIPv6 Enabled: #/nkn/nvsd/http/config/ipv6/enabled#\n"));

    /*Show the max-request size */
    cli_printf_query(_(
		"\nMaximum Request Length: #/nkn/nvsd/http/config/max_http_req_size# bytes\n"));

    /*Show Connection Request*/

    cli_printf_query(_(
                "Connection Persistence Requests       : #/nkn/nvsd/http/config/connection/persistence/num-requests# \n"));
    bail_error(err);

    /*Show Connection close option*/
    cli_printf_query(_(
                "Connection Close use RESET       : #/nkn/nvsd/http/config/connection/close/use-reset# \n"));
    bail_error(err);

    cli_printf_query(_(
                "Origin revalidation[revalidate-get] : #/nkn/nvsd/http/config/reval_meth_get# \n"));
    bail_error(err);

    /*Show Connection  pool status*/
    conn_pool_status = smprintf("/nkn/nvsd/http/config/conn-pool/origin/enabled");
    bail_null(conn_pool_status);

    err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL, 
		    &t_conn_pool_status, conn_pool_status, NULL);
    bail_error(err);
    err = cli_printf_query(
		_("\nOrigin Connection Pooling: %s\n"), 
		t_conn_pool_status ? "Enabled" : "Disabled");
    bail_error(err);
    /*Show connection pool timeout value*/
    cli_printf_query(_(
                "    Connection Pooling Timeout       : #/nkn/nvsd/http/config/conn-pool/origin/timeout# (seconds)\n"));

    /*Show connection pool max-arp-entry value*/
    cli_printf_query(_(
                "    Connection Pooling Max-connection: #/nkn/nvsd/http/config/conn-pool/origin/max-conn# \n"));

    /*Show connection pool max-conn value*/
    cli_printf_query(_(
                "    Maximum No.of ARP entries Allowed: #/nkn/nvsd/http/config/conn-pool/max-arp-entry# \n"));

    err = cli_printf(
            _("\nAllow Request Methods:\n"));
    bail_error(err);

    allow_all_method = smprintf( "/nkn/nvsd/http/config/allow_req/all");
    bail_null(allow_all_method);
    err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL, 
		    &t_allow_all_method, allow_all_method, NULL);
    bail_error(err);
    if( t_allow_all_method) {
            err = cli_printf_query( _("    All request methods are allowed\n")); 
    	    bail_error(err);         
    } else {
	    err = mdc_foreach_binding(cli_mcc, 
        	    "/nkn/nvsd/http/config/allow_req/method/*", NULL,
	    cli_nvsd_http_allow_req_method_show, NULL, &num_method);
	    bail_error(err);
	    if( num_method == 0 ) {
        		err = cli_printf(
		                _("  No new request method allowed\n"));                                  
		        bail_error(err);
	    }
    }
    cli_printf_query("Filter Mode: #/nkn/nvsd/http/config/filter/mode# \n");

bail :
    safe_free(conn_pool_status);
    safe_free(allow_all_method);

    return err;
}

int 
cli_nvsd_http_show_content_types_cmd(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    uint32 num_ctypes = 0;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    err = mdc_foreach_binding(cli_mcc, 
            "/nkn/nvsd/http/config/content/type/*", NULL,
            cli_nvsd_http_content_type_show, NULL, &num_ctypes);
    bail_error(err);
bail:
    return err;
}


/*--------------------------------------------------------------------------*/
int 
cli_nvsd_http_content_type_help(
        void *data,
        cli_help_type help_type,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        void *context)
{
    int err = 0;
    tstr_array *hdrnames = NULL;
    const char *hdrname = NULL;
    tstring *comment = NULL;
    int i = 0, num_names = 0;

    switch(help_type)
    {
    case cht_termination:
        if (cli_prefix_modes_is_enabled() && cmd->cc_help_term_prefix) {
            err = cli_add_termination_help(
                    context, GT_(cmd->cc_help_term_prefix,
                        cmd->cc_gettext_domain));
            bail_error(err);
        }
        else {
            err = cli_add_termination_help(
                    context, GT_(cmd->cc_help_term, cmd->cc_gettext_domain));
            bail_error(err);
        }
        break;

    case cht_expansion:
        if (cmd->cc_help_exp) {
            err = cli_add_expansion_help(
                    context, GT_(cmd->cc_help_exp, cmd->cc_gettext_domain),
                    GT_(cmd->cc_help_exp_hint, cmd->cc_gettext_domain));
            bail_error(err);
        }

        err = tstr_array_new(&hdrnames, NULL);
        bail_error(err);

        err = cli_nvsd_http_content_type_completion(data, cmd, cmd_line, 
                params, curr_word, hdrnames);
        bail_error(err);

        num_names = tstr_array_length_quick(hdrnames);
        for(i = 0; i < num_names; ++i) {
            hdrname = tstr_array_get_str_quick(hdrnames, i);
            bail_null(hdrname);

            ts_free(&comment);
            err = mdc_get_binding_tstr_fmt(
                    cli_mcc, NULL, NULL, NULL, &comment, NULL,
                    "/nkn/nvsd/http/config/content/type/%s/mime", hdrname);
            bail_error(err);

            err = cli_add_expansion_help(context, hdrname, 
                    ts_str_maybe(comment));
            bail_error(err);
        }
        break;
    default:
        bail_force(lc_err_unexpected_case);
        break;
    }

bail:
    tstr_array_free(&hdrnames);
    ts_free(&comment);
    return err;
}

int 
cli_nvsd_http_content_type_completion(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions)
{
    int err = 0;
    tstr_array *hdrs = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    err = cli_nvsd_http_get_content_type_list(&hdrs, true);
    bail_error(err);

    err = tstr_array_concat(ret_completions, &hdrs);
    bail_error(err);

    err = tstr_array_delete_non_prefixed(ret_completions, ts_str(curr_word));
    bail_error(err);


bail:
    tstr_array_free(&hdrs);
    return err;
}

static int 
cli_nvsd_http_get_content_type_list(
        tstr_array **ret_headers,
        tbool   hide_display)
{
    int err = 0;
    tstr_array *hdrs = NULL;
    tstr_array_options opts;
    tstr_array *hdrs_config = NULL;
    uint32 i = 0, num_hdrs = 0;
    const char *hdr = NULL;

    UNUSED_ARGUMENT(hide_display);

    bail_null(ret_headers);
    *ret_headers = NULL;

    err = tstr_array_options_get_defaults(&opts);
    bail_error(err);

    opts.tao_dup_policy = adp_delete_old;

    err = tstr_array_new(&hdrs, &opts);
    bail_error(err);

    err = mdc_get_binding_children_tstr_array(cli_mcc,
            NULL,
            NULL,
            &hdrs_config,
            "/nkn/nvsd/http/config/content/type", NULL);
    bail_error_null(err, hdrs_config);

    err = tstr_array_concat(hdrs, &hdrs_config);
    bail_error(err);

    num_hdrs = tstr_array_length_quick(hdrs);
    for(i = 0; i < num_hdrs; i++) {
        hdr = tstr_array_get_str_quick(hdrs, i);
        bail_null(hdr);
        //lc_log_basic(LOG_NOTICE, "hdr[%d] : %s", i, hdr);

    }

    *ret_headers = hdrs;
    hdrs = NULL;

bail:
    tstr_array_free(&hdrs);
    tstr_array_free(&hdrs_config);

    return err;
}


/*--------------------------------------------------------------------------*/
int 
cli_nvsd_http_header_help(
        void *data,
        cli_help_type help_type,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        void *context)
{
    int err = 0;
    tstr_array *hdrnames = NULL;
    const char *hdrname = NULL;
    tstring *comment = NULL;
    int i = 0, num_names = 0;
    char *node_name = (char *) data;

    switch(help_type)
    {
    case cht_termination:
        if (cli_prefix_modes_is_enabled() && cmd->cc_help_term_prefix) {
            err = cli_add_termination_help(
                    context, GT_(cmd->cc_help_term_prefix,
                        cmd->cc_gettext_domain));
            bail_error(err);
        }
        else {
            err = cli_add_termination_help(
                    context, GT_(cmd->cc_help_term, cmd->cc_gettext_domain));
            bail_error(err);
        }
        break;

    case cht_expansion:
        if (cmd->cc_help_exp) {
            err = cli_add_expansion_help(
                    context, GT_(cmd->cc_help_exp, cmd->cc_gettext_domain),
                    GT_(cmd->cc_help_exp_hint, cmd->cc_gettext_domain));
            bail_error(err);
        }

        err = tstr_array_new(&hdrnames, NULL);
        bail_error(err);

        err = cli_nvsd_http_header_completion(data, cmd, cmd_line, 
                params, curr_word, hdrnames);
        bail_error(err);

        num_names = tstr_array_length_quick(hdrnames);
        for(i = 0; i < num_names; ++i) {
            char p[256];
            hdrname = tstr_array_get_str_quick(hdrnames, i);
            bail_null(hdrname);

            ts_free(&comment);
            snprintf(p, 256, "%s/%%s/value", node_name);
            err = mdc_get_binding_tstr_fmt(
                    cli_mcc, NULL, NULL, NULL, &comment, NULL,
                    p, hdrname);
                    //"/nkn/nvsd/http/config/content/type/%s/mime", hdrname);
            bail_error(err);

            err = cli_add_expansion_help(context, hdrname, 
                    ts_str_maybe(comment));
            bail_error(err);
        }
        break;
    default:
        bail_force(lc_err_unexpected_case);
        break;
    }

bail:
    tstr_array_free(&hdrnames);
    ts_free(&comment);
    return err;
}

int 
cli_nvsd_http_header_completion(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions)
{
    int err = 0;
    tstr_array *hdrs = NULL;

    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    err = cli_nvsd_http_get_header_list(data, &hdrs, true);
    bail_error(err);

    err = tstr_array_concat(ret_completions, &hdrs);
    bail_error(err);

    err = tstr_array_delete_non_prefixed(ret_completions, ts_str(curr_word));
    bail_error(err);


bail:
    tstr_array_free(&hdrs);
    return err;
}

static int 
cli_nvsd_http_get_header_list(
        void *data,
        tstr_array **ret_headers,
        tbool   hide_display)
{
    int err = 0;
    tstr_array *hdrs = NULL;
    tstr_array_options opts;
    tstr_array *hdrs_config = NULL;
    uint32 i = 0, num_hdrs = 0;
    const char *hdr = NULL;
    char *node_name = (char*) data;

    UNUSED_ARGUMENT(hide_display);

    lc_log_basic(LOG_ERR, "node_name : %s", node_name);

    bail_null(ret_headers);
    *ret_headers = NULL;

    err = tstr_array_options_get_defaults(&opts);
    bail_error(err);

    opts.tao_dup_policy = adp_delete_old;

    err = tstr_array_new(&hdrs, &opts);
    bail_error(err);

    err = mdc_get_binding_children_tstr_array(cli_mcc,
            NULL,
            NULL,
            &hdrs_config,
            node_name, NULL);
    bail_error_null(err, hdrs_config);

    err = tstr_array_concat(hdrs, &hdrs_config);
    bail_error(err);

    num_hdrs = tstr_array_length_quick(hdrs);
    for(i = 0; i < num_hdrs; i++) {
        hdr = tstr_array_get_str_quick(hdrs, i);
        bail_null(hdr);
        //lc_log_basic(LOG_NOTICE, "hdr[%d] : %s", i, hdr);

    }

    *ret_headers = hdrs;
    hdrs = NULL;

bail:
    tstr_array_free(&hdrs);
    tstr_array_free(&hdrs_config);

    return err;
}


int 
cli_nvsd_http_set_file_ext(void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    uint32 ret_code = 0;
    const char *value = NULL;
    char *binding_name = NULL;
    char *allow_binding = NULL;
    char *mime_binding = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);

    value = tstr_array_get_str_quick(params, 0);
    bail_null(value);

    binding_name = 
        smprintf("/nkn/nvsd/http/config/content/type/%s", value);
    allow_binding = 
        smprintf("/nkn/nvsd/http/config/content/type/%s/allow", value);
    mime_binding = 
        smprintf("/nkn/nvsd/http/config/content/type/%s/mime", value);

    err = mdc_create_binding( cli_mcc,
            &ret_code, 
            NULL, 
            bt_string,
            value,
            binding_name);
    bail_error(err);
    bail_error(ret_code);

    err = mdc_set_binding(cli_mcc,
            &ret_code,
            NULL,
            bsso_modify,
            bt_bool,
            "true",
            allow_binding);
    bail_error(err);
    bail_error(ret_code);

    value = tstr_array_get_str_quick(params, 1);
    bail_null(value);

    err = mdc_set_binding(cli_mcc,
            &ret_code,
            NULL,
            bsso_modify,
            bt_string, 
            value, 
            mime_binding);
    bail_error(err);
    bail_error(ret_code);

bail:
    safe_free(binding_name);
    safe_free(allow_binding);
    safe_free(mime_binding);
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
cli_nvsd_http_interface_config(
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

        bname = smprintf("/nkn/nvsd/http/config/interface/%s", if_name);
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

int cli_nvsd_http_conn_pool_disable(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;

	err = cli_std_exec(data, cmd, cmd_line, params);
	bail_error(err);

	err = mdc_reset_binding(cli_mcc, NULL, NULL, 
			"/nkn/nvsd/http/config/conn-pool/origin/timeout");
	bail_error(err);

	err = mdc_reset_binding(cli_mcc, NULL, NULL, 
			"/nkn/nvsd/http/config/conn-pool/origin/max-conn");
	bail_error(err);

bail:
	return err;
}

int
cli_nvsd_http_allow_req_methods(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params)
{
    int err = 0;
    bn_binding_array *barr = NULL;
    uint32 code = 0;
    uint32 num_params = 0;
    const char *param = NULL;
    uint32 i = 0;
    char *bnname = NULL;
    tstring *msg = NULL;
    tstring *up_param = NULL;
    uint32 num_method = 0;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd_line);

    num_params = tstr_array_length_quick(params);

    switch(cmd->cc_magic) 
    {
    case cid_req_method_add:
        
        err = bn_binding_array_new(&barr);
        bail_error(err);

        for (i = 0; i < num_params; i++) {

            param = tstr_array_get_str_quick(params, i);
            bail_null(param);

	    err = ts_new_str(&up_param, param);
	    bail_error(err);
	    ts_trans_uppercase(up_param);
	    bnname = smprintf( "/nkn/nvsd/http/config/allow_req/method/%s", ts_str(up_param));
            bail_null(bnname);
	    	    err = mdc_create_binding(cli_mcc, &code, &msg,
				    bt_string,
				    ts_str(up_param),
				    bnname);
#if 0
                   err = mdc_array_append_single(cli_mcc, 
				    "/nkn/nvsd/http/config/allow_req/method", 
				    "method",
				    bt_string,
				    param,
				    true, 
				    NULL,
				    &code,
				    &msg);
#endif
		    bail_error(err);

                    if (code != 0) {
			    cli_printf(_("error : %s\n"), ts_str(msg));
                        break;
                    }

            }

        err = mdc_set_binding(cli_mcc,
                NULL,
                NULL,
                bsso_modify,
                bt_bool,
                "false",
                "/nkn/nvsd/http/config/allow_req/all");
        bail_error(err);

	break;
    case cid_req_method_delete:
        
        err = bn_binding_array_new(&barr);
        bail_error(err);

	    err = mdc_foreach_binding(cli_mcc,
        	    "/nkn/nvsd/http/config/allow_req/method/*", NULL, cli_nvsd_http_delete_method, NULL, &num_method);
	    bail_error(err);

        for (i = 0; i < num_params; i++) {

            param = tstr_array_get_str_quick(params, i);
            bail_null(param);
	    err = ts_new_str(&up_param, param);
	    bail_error(err);
	    ts_trans_uppercase(up_param);
	    bnname = smprintf( "/nkn/nvsd/http/config/allow_req/method/%s", ts_str(up_param));
            bail_null(bnname);
            err = mdc_delete_binding(cli_mcc, NULL, NULL, bnname);
#if 0
                    err = mdc_array_delete_by_value_single(cli_mcc,
                            "/nkn/nvsd/http/config/allow_req/method", false, 
                            "method", bt_string,param, NULL, &code, NULL);
#endif
                    bail_error(err);

            }
	    if( num_method == 1 ) {
	        err = mdc_set_binding(cli_mcc,
	                NULL,
	                NULL,
	                bsso_modify,
	                bt_bool,
	                "true",
	                "/nkn/nvsd/http/config/allow_req/all");
	        bail_error(err);

	    }

        break;
    }
 bail:

    if (err && code) 
    {
	    cli_printf("%s\n",ts_str(msg));
    }
    safe_free(bnname);
    bn_binding_array_free(&barr);
    return(err);
}

int
cli_nvsd_http_allow_req_method_show(const bn_binding_array *bindings,
                            uint32 idx, const bn_binding *binding,
                            const tstring *name,
                            const tstr_array *name_components,
                            const tstring *name_last_part,
                            const tstring *value, void *callback_data)
{
    int err = 0;
    tstring *method = NULL;

    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(name_last_part);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(callback_data);

    err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &method);
    bail_error(err);

    err = cli_printf(
                _("     %s"), ts_str(method));
    bail_error(err);

bail:
    ts_free(&method);
    return err;
}

int
cli_nkn_max_arp_entry_set(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params)
{
    int err = 0;
    tstr_array *cmd_params = NULL;
    tstring* max_arp_entry;
    char* max_arp_node;

    err = cli_std_exec(data, cmd, cmd_line, params);
    bail_error(err);

    max_arp_node = smprintf("/nkn/nvsd/http/config/conn-pool/max-arp-entry");
    bail_null(max_arp_node);

    err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
        			&max_arp_entry, max_arp_node, NULL);
    bail_error_null(err, max_arp_entry);

    err = tstr_array_new_va_str(&cmd_params, NULL, 2, "/opt/nkn/bin/max_arp_entry.sh", ts_str(max_arp_entry));
    bail_error(err);

	err = nkn_clish_run_program_fg("/opt/nkn/bin/max_arp_entry.sh", cmd_params, NULL, NULL, NULL);
    bail_error(err);

  bail:
    return(err);
}

int
cli_nvsd_http_delete_method(const bn_binding_array *bindings,
                            uint32 idx, const bn_binding *binding,
                            const tstring *name,
                            const tstr_array *name_components,
                            const tstring *name_last_part,
                            const tstring *value, void *callback_data)
{
    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(name_last_part);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(callback_data);
	return 0;
}


int
nkn_validate_port(const char *param, error_msg_t *err_msg)
{
    uint32 i = 0, code = 0;
    char *p = NULL;
    int ch = '-';

    /* Check if the port has alphabets/special characters */
    for(i=0; i< strlen(param); i++ ) {
         if ( !isdigit(param[i]) && param[i] != '-') {
             code =1;
             snprintf(*err_msg, sizeof(*err_msg),"Enter only numbers");
             goto bail;
          }
    }
    /* Check whether this is a range */
    if ( NULL != (p = strstr(param, "-"))) {
        /* Check if there is more than one hyphen */
        if ( p != strrchr(param, ch)) {
            code = 1;
            snprintf(*err_msg, sizeof(*err_msg),"Invalid input: Enter either a number or a range");
            goto bail;
        }
    }
bail:
    return code;
}

int get_existing_indices(bn_binding_array *bindings, uint32_array **indices, tstr_array *t_indices)
{
        int err = 0;
        uint32 i = 0 ,indices_len = 0, idx = 0;
        tstring *t_idx = 0;

	err = tstr_array_new(&t_indices,NULL);
	bail_error(err);

        err = bn_binding_array_get_last_name_parts(bindings, &t_indices);
        bail_error(err);

	err = uint32_array_new(indices);
	bail_error(err);

        indices_len = tstr_array_length_quick(t_indices);

        for ( i = 0; i < indices_len ; i++) {
            err =tstr_array_get(t_indices, i, &t_idx);
            bail_error_null(err, t_idx);
            idx = strtoul(ts_str(t_idx), NULL, 10);
        //Ignore if the last component is "port"
	    if ( idx ) {
                err = uint32_array_append(*indices, idx);
                bail_error(err);
	    }
        }

        err = uint32_array_sort(*indices);
        bail_error(err);


bail:
        return err;

}

int
find_matching_index(uint32 prt, bn_binding_array *bindings, uint32_array *indices, uint32 *ret_idx )
{
	int err = 0;
	uint32 i = 0, idx_len = 0, idx = 0, value =0;
	node_name_t bn_name = {0};
	tstring *t_value = NULL;
	
	idx_len = uint32_array_length_quick(indices);
        for(i = 0; i< idx_len ; i++){
	    idx = uint32_array_get_quick(indices,i);
            snprintf(bn_name, sizeof(bn_name),"/nkn/nvsd/http/config/server_port/%d/port",idx);

            err = bn_binding_array_get_first_matching_value_tstr(bindings, bn_name,
                                             0, NULL, NULL, &t_value);
            bail_error(err);
            if ( t_value ) {
                value = strtoul(ts_str(t_value),NULL,10);
                if ( prt == value ) {
                    *ret_idx = idx; 
                    break;
                }
            }
        }
bail:
	ts_free(&t_value);
	return err;
}

