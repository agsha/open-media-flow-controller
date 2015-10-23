/*
 * Filename :   cli_nvsd_server_map_cmds.c
 * Date :       2009/03/09
 * Author :     Dhruva Narasimhan
 *
 * (C) Copyright 2008, 2009, Nokeena Networks Inc.
 * All rights reserved.
 *
 */


/*----------------------------------------------------------------------------
 * HEADER FILES
 *---------------------------------------------------------------------------*/
#include "common.h"
#include "climod_common.h"
#include "nkn_defs.h"

cli_expansion_option cli_server_map_proto_opts[] = {
    {"http", N_("Use HTTP protocol"), (void*) "http"},
    {"nfs", N_("Use NFS protocol"), (void*) "nfs"},
    {NULL, NULL, NULL}
};

#if 0
cli_expansion_option cli_server_map_format_type_opts[] = {
    {"host-origin-map", N_("Use a HTTP origin map as the format type"), (void*) "1"},
    {"cluster-map", N_("Use a HTTP cluster map as the format type"), (void*) "2"},
    {"origin-escalation-map", N_("Use a HTTP origin escalation map as the format type"), (void*) "3"},
    {"nfs-map", N_("Use a NFS map as the format type"), (void*) "4"},
    {"origin-round-robin-map", N_("Use a Origin Round Robin map as the format type"), (void*) "5"},
    {NULL, NULL, NULL}
};
#endif

extern int
cli_std_exec(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params);

int 
cli_nvsd_server_map_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int 
cli_nvsd_server_map_enter_mode(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);


int
cli_server_map_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_sm_name_help(
        void *data, 
        cli_help_type help_type,
        const cli_command *cmd, 
        const tstr_array *cmd_line,
        const tstr_array *params, 
        const tstring *curr_word,
        void *context);
int
cli_sm_name_completion(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions);

int
cli_nvsd_server_map_show_list(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nvsd_server_map_show_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_nvsd_server_map_create_timer(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);



static int 
cli_server_map_name_validate(
        const char *server_map,
        tbool *ret_valid);


int 
cli_server_map_get_names(
        tstr_array **ret_names,
        tbool hide_display);


int 
cli_nvsd_server_map_show_one(
        const bn_binding_array *bindings,
        uint32 idx,
        const bn_binding *binding,
        const tstring *name,
        const tstr_array *name_components, 
        const tstring *name_last_part,
        const tstring *value,
        void *cb_data);


static int 
cli_nvsd_server_map_revmap(
        void *data, const cli_command *cmd,
        const bn_binding_array *bindings, const char *name,
        const tstr_array *name_parts, const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply);

int
cli_nvsd_smap_nfs_print_msg(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);


int 
cli_nvsd_server_map_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;

    UNUSED_ARGUMENT(info);

#ifdef PROD_FEATURE_I18N_SUPPORT
    err = cli_set_gettext_domain(GETTEXT_DOMAIN);
    bail_error(err)
#endif
    if (context->cmc_mgmt_avail == false) {
        goto bail;
    }

    CLI_CMD_NEW;
    cmd->cc_words_str =             "server-map";
    cmd->cc_help_descr =            N_("Configure server maps");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "server-map *";
    cmd->cc_help_exp =              N_("<name>");
    cmd->cc_help_exp_hint =         N_("Server map name");
    cmd->cc_help_term_prefix =      
        N_("Create a new server map or change configuration for an existing server map");
    cmd->cc_help_use_comp =         true;
    cmd->cc_comp_type =             cct_matching_names;
    cmd->cc_comp_pattern =          "/nkn/nvsd/server-map/config/*";
    cmd->cc_flags =                 ccf_terminal | ccf_prefix_mode_root;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_callback =         cli_nvsd_server_map_enter_mode;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_revmap_type =           crt_none;
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =             "server-map * format-type";
    cmd->cc_help_descr =            N_("Configure/Modify format-type. "
											"While modifying \"file-url\" value will be reset");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "server-map * format-type host-origin-map";
    cmd->cc_help_descr =            N_("Host-origin type server-map");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/server-map/config/$1$/format-type";
    cmd->cc_exec_value =            "1";
    cmd->cc_exec_type =             bt_uint32;
//    cmd->cc_revmap_type =           crt_auto;
//    cmd->cc_revmap_order =          cro_servermap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "server-map * format-type cluster-map";
    cmd->cc_help_descr =            N_("cluster type server-map");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/server-map/config/$1$/format-type";
    cmd->cc_exec_value =            "2";
    cmd->cc_exec_type =             bt_uint32;
//    cmd->cc_revmap_type =           crt_auto;
//    cmd->cc_revmap_order =          cro_servermap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "server-map * format-type origin-escalation-map";
    cmd->cc_help_descr =            N_("origin-escalation server-map");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/server-map/config/$1$/format-type";
    cmd->cc_exec_value =            "3";
    cmd->cc_exec_type =             bt_uint32;
//    cmd->cc_revmap_type =           crt_auto;
//    cmd->cc_revmap_order =          cro_servermap;
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =             "server-map * format-type nfs-map";
    cmd->cc_help_descr =            N_("NFS type server-map is not supported");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_callback = 	    cli_nvsd_smap_nfs_print_msg;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "server-map * format-type origin-round-robin-map";
    cmd->cc_help_descr =            N_("Define server-map to send requests to origin in round robin mode");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/server-map/config/$1$/format-type";
    cmd->cc_exec_value =            "5";
    cmd->cc_exec_type =             bt_uint32;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "server-map * node-monitoring";
    cmd->cc_help_descr =            N_("Configures the node monitoring parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "server-map * node-monitoring heartbeat";
    cmd->cc_help_descr =            N_("Configure the heartbeat parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "server-map * node-monitoring heartbeat interval";
    cmd->cc_help_descr =            N_("Configure the heartbeat interval");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "server-map * node-monitoring heartbeat interval *";
    cmd->cc_help_exp =              N_("<Configure the heartbeat interval>");
    cmd->cc_help_exp_hint =         N_("time in msec");
    cmd->cc_flags =                 ccf_terminal|ccf_ignore_length;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/server-map/config/$1$/node-monitoring/heartbeat/interval";
    cmd->cc_exec_value =            "$2$";
    cmd->cc_exec_type =             bt_uint32;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_servermap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "server-map * node-monitoring heartbeat connect-timeout";
    cmd->cc_help_descr =            N_("Configure the connect timeout");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "server-map * node-monitoring heartbeat connect-timeout *";
    cmd->cc_help_exp =              N_("<Configure the connect timeout>");
    cmd->cc_help_exp_hint =         N_("time in msec");
    cmd->cc_flags =                 ccf_terminal|ccf_ignore_length;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/server-map/config/$1$/node-monitoring/heartbeat/connect-timeout";
    cmd->cc_exec_value =            "$2$";
    cmd->cc_exec_type =             bt_uint32;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_servermap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "server-map * node-monitoring heartbeat read-timeout";
    cmd->cc_help_descr =            N_("Configure the heartbeat read timeout");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "server-map * node-monitoring heartbeat read-timeout *";
    cmd->cc_help_exp =              N_("<Configure the read timeout>");
    cmd->cc_help_exp_hint =         N_("time in msec");
    cmd->cc_flags =                 ccf_terminal|ccf_ignore_length;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/server-map/config/$1$/node-monitoring/heartbeat/read-timeout";
    cmd->cc_exec_value =            "$2$";
    cmd->cc_exec_type =             bt_uint32;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_servermap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "server-map * node-monitoring heartbeat allowed-fails";
    cmd->cc_help_descr =            N_("Configure the heartbeat allowed filures");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "server-map * node-monitoring heartbeat allowed-fails *";
    cmd->cc_help_exp =              N_("<Configure the allowed heartbeat failure count>");
    cmd->cc_help_exp_hint =         N_("No.of Failures");
    cmd->cc_flags =                 ccf_terminal|ccf_ignore_length;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/server-map/config/$1$/node-monitoring/heartbeat/allowed-fails";
    cmd->cc_exec_value =            "$2$";
    cmd->cc_exec_type =             bt_uint32;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_servermap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no server-map";
    cmd->cc_help_descr =            N_("Reset/Clear server map configuration");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no server-map *";
    cmd->cc_help_exp =              N_("<name>");
    cmd->cc_help_exp_hint =         N_("Server map name");
    cmd->cc_comp_pattern =          "/nkn/nvsd/server-map/config/*";
    cmd->cc_comp_type =             cct_matching_names;
    cmd->cc_help_use_comp =         true;
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_delete;
    cmd->cc_exec_name =             "/nkn/nvsd/server-map/config/$1$";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_type =             bt_string;
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "server-map * no";
    cmd->cc_help_descr =            N_("Clear/remove server-map settings");
    cmd->cc_req_prefix_count =       2;
    cmd->cc_flags =                  ccf_hidden;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"no server-map * foo";
    cmd->cc_help_descr = 	N_("Dummy command definition to get rid of annoying warning in syslog");
    cmd->cc_flags = 		ccf_terminal | ccf_hidden;
    cmd->cc_capab_required =        ccp_action_rstr_curr;
    cmd->cc_exec_operation =        cxo_action;
    cmd->cc_exec_action_name =      "/nkn/nvsd/server-map/actions/refresh-force";
    cmd->cc_exec_name =             "name";
    cmd->cc_exec_type =             bt_string;
    cmd->cc_exec_value =            "$1$";
    CLI_CMD_REGISTER;
		    

    CLI_CMD_NEW;
    cmd->cc_words_str =             "server-map * file-url";
    cmd->cc_help_descr =            N_("Configure File URL");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "server-map * file-url *";
    cmd->cc_help_exp =              N_("<URL>");
    cmd->cc_help_exp_hint =         N_("URL of file");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/nvsd/server-map/config/$1$/file-url";
    cmd->cc_exec_type =             bt_string;
    cmd->cc_exec_value =            "$2$";
    cmd->cc_exec_name2 =            "/nkn/nvsd/server-map/config/$1$/refresh";
    cmd->cc_exec_type2 =            bt_uint32;
    cmd->cc_exec_value2 =           "300";
    CLI_CMD_REGISTER;
    

    CLI_CMD_NEW;
    cmd->cc_words_str =             "server-map * file-url * refresh-interval";
    cmd->cc_help_descr =            N_("Refresh time for mapping file");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "server-map * file-url * refresh-interval *";
    cmd->cc_help_exp =              N_("<seconds>");
    cmd->cc_help_exp_hint =         N_("Refresh time, in seconds");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =            "/nkn/nvsd/server-map/config/$1$/file-url";
    cmd->cc_exec_type =            bt_string;
    cmd->cc_exec_value =           "$2$";
    cmd->cc_exec_name2 =             "/nkn/nvsd/server-map/config/$1$/refresh";
    cmd->cc_exec_type2 =             bt_uint32;
    cmd->cc_exec_value2 =            "$3$";
    cmd->cc_revmap_names =          "/nkn/nvsd/server-map/config/*";
    cmd->cc_revmap_callback =       cli_nvsd_server_map_revmap;
    cmd->cc_revmap_order =          cro_servermap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "server-map * refresh-force";
    cmd->cc_help_descr =            N_("Force a refresh of the server map pointed to by the file-url");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_action_priv_curr;
    cmd->cc_exec_operation =        cxo_action;
    cmd->cc_exec_action_name =      "/nkn/nvsd/server-map/actions/refresh-force";
    cmd->cc_exec_name =             "name";
    cmd->cc_exec_type =             bt_string;
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_name2 =             "config";
    cmd->cc_exec_type2 =             bt_string;
    cmd->cc_exec_value2 =            "0";
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str = 	    "show server-map";
    cmd->cc_help_descr =	    N_("Display Server map configurations");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "show server-map list";
    cmd->cc_help_descr =            N_("Display the list of server maps configured");
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_query_priv_curr;
    cmd->cc_exec_callback =         cli_nvsd_server_map_show_list;
    CLI_CMD_REGISTER;
    
    CLI_CMD_NEW;
    cmd->cc_words_str =             "show server-map *";
    cmd->cc_help_exp = 		    N_("<server-map name>");
    cmd->cc_help_exp_hint =         "";
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_query_priv_curr;
    cmd->cc_exec_callback =         cli_nvsd_server_map_show_config;
    cmd->cc_help_callback =         cli_sm_name_help;
    cmd->cc_comp_callback =         cli_sm_name_completion;
    CLI_CMD_REGISTER;

    err = cli_revmap_ignore_bindings_va(2,
    		"/nkn/nvsd/server-map/config/*/map-status",
		"/nkn/nvsd/server-map/config/*/format-type");
bail:
    return err;
}


static int 
cli_nvsd_server_map_revmap(
        void *data, const cli_command *cmd,
        const bn_binding_array *bindings, const char *name,
        const tstr_array *name_parts, const char *value,
        const bn_binding *binding,
        cli_revmap_reply *ret_reply)
{
    int err = 0;
    //const char *c_name = NULL;
    tstring *rev_cmd = NULL;
    tstring *t_file = NULL;
    tstring *t_refresh = NULL;
    tstring *t_proto = NULL;
    tstring *t_format_type = NULL;
    const char *str_format_type = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(name_parts);
    UNUSED_ARGUMENT(binding);
    
    //err = tstr_array_get_str(name_parts, 2, &c_name);
    //bail_error_null(err, c_name);

    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
            NULL, &t_file, "%s/file-url", name);
    bail_error_null(err, t_file);

    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
            NULL, &t_refresh, "%s/refresh", name);
    bail_error_null(err, t_refresh);

    err = bn_binding_array_get_value_tstr_by_name_fmt(bindings,
		    NULL, &t_format_type, "%s/format-type", name);
    bail_error_null(err ,t_format_type);

    /* Give the reverse map for both format-type and file-url with
       format-type coming above file-url */

    if ( ts_num_chars(t_file) > 0 ) {
	    if(!strcmp(ts_str(t_format_type),"1")) {
		    str_format_type = "host-origin-map";
	    }else if(!strcmp(ts_str(t_format_type),"2")) {
		    str_format_type = "cluster-map";
	    } else if(!strcmp(ts_str(t_format_type),"3")) {
		    str_format_type = "origin-escalation-map";
	    }else if(!strcmp(ts_str(t_format_type), "5")) {
		    str_format_type = "origin-round-robin-map";
	    }
	    bail_null(str_format_type);
	    if (cli_prefix_modes_revmap_is_enabled()) {
                err = ts_new_sprintf(&rev_cmd, "server-map %s format-type %s \n  file-url %s refresh-interval %s", 
                        value, str_format_type, ts_str(t_file), ts_str(t_refresh));
            }
            else {
                err = ts_new_sprintf(&rev_cmd, "server-map %s format-type %s\n"
                                           "   server-map %s file-url %s refresh-interval %s", 
                        value, str_format_type, value,  ts_str(t_file), ts_str(t_refresh));
            }
            bail_error(err);
    err = tstr_array_append_str(ret_reply->crr_cmds, ts_str(rev_cmd));
    bail_error(err);
    }


#define CONSUME_REVMAP_NODES(c) \
    {\
        err = tstr_array_append_sprintf(ret_reply->crr_nodes, c, name);\
        bail_error(err);\
    }

    CONSUME_REVMAP_NODES("%s");
    CONSUME_REVMAP_NODES("%s/file-url");
    CONSUME_REVMAP_NODES("%s/refresh");
    CONSUME_REVMAP_NODES("%s/protocol");
    CONSUME_REVMAP_NODES("%s/format-type");
    CONSUME_REVMAP_NODES("%s/file-binary");

bail:
    ts_free(&rev_cmd);
    ts_free(&t_file);
    ts_free(&t_refresh);
    ts_free(&t_proto);
    return err;
}


int
cli_server_map_config(
        void *data,
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
cli_nvsd_server_map_enter_mode(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    tbool valid = false;
    char *binding_name = NULL;
    uint32 code = 0;
    tstring *msg = NULL;

    UNUSED_ARGUMENT(data);

    err = cli_server_map_name_validate(tstr_array_get_str_quick(params, 0),
            &valid);
    //bail_error(err);

    // If we are running in EXEC mode, then we need to check
    // that the user didnt give a bigus server-map.
    //
    if ( !cli_have_capabilities(ccp_set_rstr_curr) && !valid ) {
        cli_printf_error(_("Unknown server-map '%s'"),
                tstr_array_get_str_quick(params, 0));
        err = 0;
        goto bail;
    }
    // If we are running in EXEC mode, and namespace name is valid
    // DO NOT allow to enter into prefix mode.
    else if ( !cli_have_capabilities(ccp_set_rstr_curr) && valid ) {
        err = cli_print_incomplete_command_error(cmd, cmd_line);
        bail_error(err);

        goto bail;
    }
    if(!valid) {
        // Node doesnt exist.. create it

        binding_name = smprintf("/nkn/nvsd/server-map/config/%s", 
                tstr_array_get_str_quick(params, 0));
        bail_null(binding_name);

        err = mdc_create_binding(cli_mcc,
                &code,
                &msg,
                bt_string,
                tstr_array_get_str_quick(params, 0),
                binding_name);
        bail_error(err);
        if(code != 0) {
            goto bail;
        }

        safe_free(binding_name);
        valid = true;
    }

    if (cli_prefix_modes_is_enabled()) {
        err = cli_server_map_name_validate(tstr_array_get_str_quick(params, 0),
                &valid);
        bail_error(err);
    
        if (valid) {
            err = cli_prefix_enter_mode(cmd, cmd_line);
            bail_error(err);
        }
    }
    else {
        err = cli_print_incomplete_command_error(cmd, cmd_line);
        bail_error(err);
    }

bail:
    safe_free(binding_name);
    return err;
}

static int 
cli_server_map_name_validate(
        const char *server_map,
        tbool *ret_valid)
{
    int err = 0;
    tstr_array *ns = NULL;
    uint32 idx = 0;

    bail_null(ret_valid);
    *ret_valid = false;

    err = cli_server_map_get_names(&ns, false);
    bail_error(err);

    err = tstr_array_linear_search_str(ns, server_map, 0, &idx);
    if (lc_err_not_found != err) {
        *ret_valid = true;
        err = 0;
    }
    else {
        // This condition doesnt exist since
        //  - if name does not exist, we create it
        //  - if name exists then this is never hit
        //err = cli_printf_error(_("Unknown server_map - %s\n"), server_map);
        //bail_error(err);
    }
bail:
    tstr_array_free(&ns);
    return err;
}

int 
cli_server_map_get_names(
        tstr_array **ret_names,
        tbool hide_display)
{
    int err = 0;
    tstr_array *names = NULL;
    tstr_array_options opts;
    tstr_array *names_config = NULL;
    char *bn_name = NULL;
    bn_binding *display_binding = NULL;

    UNUSED_ARGUMENT(hide_display);

    bail_null(ret_names);
    *ret_names = NULL;

    err = tstr_array_options_get_defaults(&opts);
    bail_error(err);

    opts.tao_dup_policy = adp_delete_old;

    err = tstr_array_new(&names, &opts);
    bail_error(err);

    err = mdc_get_binding_children_tstr_array(cli_mcc,
            NULL, NULL, &names_config, 
            "/nkn/nvsd/server-map/config", NULL);
    bail_error_null(err, names_config);

    err = tstr_array_concat(names, &names_config);
    bail_error(err);

    *ret_names = names;
    names = NULL;
bail:
    tstr_array_free(&names);
    tstr_array_free(&names_config);
    safe_free(bn_name);
    bn_binding_free(&display_binding);
    return err;
}


int 
cli_nvsd_server_map_show_one(
        const bn_binding_array *bindings,
        uint32 idx,
        const bn_binding *binding,
        const tstring *name,
        const tstr_array *name_components, 
        const tstring *name_last_part,
        const tstring *value,
        void *cb_data)
{
    int err = 0;
    tstring *fmt_type = NULL;
    bn_binding *status_binding = NULL;
    uint32 state = 0;

    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(name_last_part);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(cb_data);

    cli_printf_query(
            _("Server-map : #%s# \n"), ts_str(name));

    err = mdc_get_binding_tstr_fmt(cli_mcc, NULL, NULL, NULL, &fmt_type, NULL, "%s/format-type", ts_str(name));
    bail_error(err);

    if (ts_equal_str(fmt_type, "1", true)) {
        cli_printf(
                _("    Format-Type : host-origin-map\n"));
    }
    else if (ts_equal_str(fmt_type, "2", true)) {
        cli_printf(
                _("    Format-Type : cluster-map\n"));
    }
    else if (ts_equal_str(fmt_type, "3", true)) {
       cli_printf(
                _("    Format-Type : origin-escalation-map\n"));
    }
    else if (ts_equal_str(fmt_type, "4", true)) {
           cli_printf(
                _("    Format-Type : nfs-map\n"));
    }
    else if (ts_equal_str(fmt_type, "5", true)) {
           cli_printf(
                _("    Format-Type : origin-round-robin-map\n"));
    }else {
        cli_printf(
                _("    Format-Type : None\n"));
    }

    cli_printf_query(
            _("    Map File : #%s/file-url# \n"), ts_str(name));
    cli_printf_query(
            _("    Refresh Interval : #%s/refresh# (seconds)\n"), ts_str(name));
    cli_printf(_("\n"));
#if 0
    if (!ts_equal_str(fmt_type, "1", true)) {
#endif
        err = mdc_get_binding_fmt(cli_mcc, NULL, NULL, false, &status_binding, NULL,
            "/nkn/nvsd/server-map/monitor/%s/state", ts_str(name_last_part));
        bail_error_null(err, status_binding);
        err = bn_binding_get_uint32(status_binding, ba_value, 0, &state);
        bail_error(err);
        if(state == 1){
            cli_printf(
                _("    Map File Status : Valid\n"));
        } else if(state == 2){
            cli_printf(
                _("    Map File Status : Invalid\n"));
        } else {
            cli_printf(
                _("    Map File Status : Unknown\n"));
        }
    	cli_printf(_("\n"));
#if 0
    }
#endif

bail:
    ts_free(&fmt_type);
    bn_binding_free(&status_binding);
    return err;
}

int
cli_nvsd_server_map_show_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
	int err = 0;
	uint32 ret_matches = 0;
	const char *server_map = NULL;
	char *server_map_binding = NULL;
	char *smap_ft_node = NULL;
	tstring *smap_ft = NULL;

	UNUSED_ARGUMENT(data);
    	UNUSED_ARGUMENT(cmd);
    	UNUSED_ARGUMENT(cmd_line);

	server_map = tstr_array_get_str_quick(params, 0);
	bail_null(server_map);

	server_map_binding = smprintf("/nkn/nvsd/server-map/config/%s",server_map);
	bail_null(server_map_binding);

	err = mdc_foreach_binding(cli_mcc, server_map_binding, NULL, 
	cli_nvsd_server_map_show_one, NULL, &ret_matches);
	bail_error(err);

	if (ret_matches == 0) {
	cli_printf(
		_("No Server maps configured."));
	}
	else{
		smap_ft_node = smprintf("/nkn/nvsd/server-map/config/%s/format-type", server_map);
		bail_null(smap_ft_node);

		err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL,
			    &smap_ft, smap_ft_node, NULL);
		bail_error(err);

		if(((atoi(ts_str(smap_ft))) == 2) || ((atoi(ts_str(smap_ft))) == 3) || ((atoi(ts_str(smap_ft))) == 5))
		{
			err = cli_printf(
					_("  Node Monitoring Parameters:\n"));
			bail_error(err);
			err = cli_printf_query(
					_("    Heartbeat interval Value : #/nkn/nvsd/server-map/config/%s/node-monitoring/heartbeat/interval#\n"),
					server_map);
			bail_error(err);

			err = cli_printf_query(
					_("    Heartbeat connect Timeout Value : #/nkn/nvsd/server-map/config/%s/node-monitoring/heartbeat/connect-timeout#\n"),
					server_map);
			bail_error(err);

			err = cli_printf_query(
					_("    Heartbeat Read Timeout Value : #/nkn/nvsd/server-map/config/%s/node-monitoring/heartbeat/read-timeout#\n"),
					server_map);
			bail_error(err);

			err = cli_printf_query(
					_("    Heartbeat allowed failure count : #/nkn/nvsd/server-map/config/%s/node-monitoring/heartbeat/allowed-fails#\n\n"),
					server_map);
			bail_error(err);
		}
	}

bail:                                                                         
	return err;
}

int
cli_nvsd_server_map_show_list(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    uint32 ret_matches = 0;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    err = mdc_foreach_binding(cli_mcc, "/nkn/nvsd/server-map/config/*", NULL, 
            cli_nvsd_server_map_show_one, NULL, &ret_matches);
    bail_error(err);

    if (ret_matches == 0) {
        cli_printf(
                _("No Server maps configured."));
    }

bail:
    return err;
}

int
cli_sm_name_help(
        void *data, 
        cli_help_type help_type,
        const cli_command *cmd, 
        const tstr_array *cmd_line,
        const tstr_array *params, 
        const tstring *curr_word,
        void *context)
{
    int err = 0;
    tstr_array *names = NULL;
    const char *namespace = NULL;
    int i = 0, num_names = 0;


    switch(help_type)
    {
    case cht_termination:
        if(cli_prefix_modes_is_enabled() && cmd->cc_help_term_prefix) {
            err = cli_add_termination_help(context, 
                    GT_(cmd->cc_help_term_prefix, cmd->cc_gettext_domain));
            bail_error(err);
        }
        else {
            err = cli_add_termination_help(context,
                    GT_(cmd->cc_help_term, cmd->cc_gettext_domain));
            bail_error(err);
        }
        break;

    case cht_expansion:
        if(cmd->cc_help_exp) {
            err = cli_add_expansion_help(context,
                    GT_(cmd->cc_help_exp, cmd->cc_gettext_domain),
                    GT_(cmd->cc_help_exp_hint, cmd->cc_gettext_domain));
            bail_error(err);
        }

        err = tstr_array_new(&names, NULL);
        bail_error(err);

        err = cli_sm_name_completion(data, cmd, cmd_line, params, 
                curr_word, names);
        bail_error(err);

        num_names = tstr_array_length_quick(names);
        for(i = 0; i < num_names; ++i) {
            namespace = tstr_array_get_str_quick(names, i);
            bail_null(namespace);
            
            err = cli_add_expansion_help(context, namespace, NULL);
            bail_error(err);
        }
        break;
    
    default:
        bail_force(lc_err_unexpected_case);
        break;
    }
bail:
    tstr_array_free(&names);
    return err;
}

int
cli_sm_name_completion(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params,
        const tstring *curr_word,
        tstr_array *ret_completions)
{
    int err = 0;
    tstr_array *names = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    err = cli_server_map_get_names(&names, true);
    bail_error(err);

    err = tstr_array_concat(ret_completions, &names);
    bail_error(err);

    err = tstr_array_delete_non_prefixed(ret_completions, ts_str(curr_word));
    bail_error(err);

bail:
    tstr_array_free(&names);
    return err;
}

int
cli_nvsd_server_map_create_timer(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    bn_request *req = NULL;
    uint32 ret_err = 0;


    /* Call standard CLI processing - setting up nodes etc.. */
    err = cli_std_exec(data, cmd, cmd_line, params);
    bail_error(err);

    /* Fire action to create the timer */
    err = bn_action_request_msg_create(&req, 
            "/nkn/nvsd/server-map/actions/create-timer");
    bail_error(err);

    err = bn_action_request_msg_append_new_binding(req,
            0, "name", bt_string, tstr_array_get_str_quick(params, 0), NULL);
    bail_error(err);

    /* Send the action message */
    err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, &ret_err, NULL);
    bail_error(err);

bail:
    return err;
}

int
cli_nvsd_smap_nfs_print_msg(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
	int err = 0;

    	UNUSED_ARGUMENT(data);
    	UNUSED_ARGUMENT(cmd);
    	UNUSED_ARGUMENT(cmd_line);
    	UNUSED_ARGUMENT(params);

	err = cli_printf_error(_("NFS server map not supported\n"));

	return err;

}
