/*
 * Filename :   cli_ssld_delivery_cmds.c
 * Date     :   2011/07/13
 * Author   :   Thilak Raj S
 *
 * (C) Copyright 2011 Nokeena Networks, Inc.
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

enum {
    cid_https_port_add = 1,
    cid_https_port_delete = 2
};

/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
static int (*cli_interface_validate)(const char *ifname, tbool *ret_valid) = NULL;

int
cli_ssld_delivery_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);
int
cli_ssld_config_port(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params);

int 
cli_ssld_https_interface_config(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int
cli_ssld_https_server_port_show(const bn_binding_array *bindings,
                            uint32 idx, const bn_binding *binding,
                            const tstring *name,
                            const tstr_array *name_components,
                            const tstring *name_last_part,
                            const tstring *value, void *callback_data);

int
cli_ssld_https_show_interface(const bn_binding_array *bindings,
                            uint32 idx, const bn_binding *binding,
                            const tstring *name,
                            const tstr_array *name_components,
                            const tstring *name_last_part,
                            const tstring *value, void *callback_data);

int cli_ssld_https_show_cmd(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);

int cli_ssld_https_conn_pool_disable(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params);

/*----------------------------------------------------------------------------
 * MODULE ENTRY POINT
 *---------------------------------------------------------------------------*/
int
cli_ssld_delivery_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;
    cli_command *cmd = NULL;
    void *callback = NULL;


    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);


#ifdef PROD_FEATURE_I18N_SUPPORT
    err = cli_set_gettext_domain(GETTEXT_DOMAIN);
    bail_error(err);
#endif

    if (context->cmc_mgmt_avail == false) {
        goto bail;
    }

    err = lc_dso_module_get_symbol(info->ldi_context, 
            "cli_interface_cmds", "cli_interface_validate", 
            &callback);

    bail_error_null(err, callback);
    cli_interface_validate = (callback);


    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol https";
    cmd->cc_help_descr =            N_("Configure HTTPS protocol options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol https listen";
    cmd->cc_help_descr =            N_("Configure listen port parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol https listen port";
    cmd->cc_help_descr =            N_("Set listening port parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol https listen";
    cmd->cc_help_descr =            N_("Clear listen port parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol https listen port *";
    cmd->cc_help_exp =              N_("<port number>");
    cmd->cc_help_exp_hint =         N_("for example, listen port 443");
    cmd->cc_help_use_comp =         true;
    cmd->cc_comp_type =             cct_matching_values ;
    cmd->cc_comp_pattern =          "/nkn/ssld/config/delivery/server_port/*";
    cmd->cc_flags =                 ccf_terminal | ccf_allow_extra_params;
    cmd->cc_capab_required =        ccp_set_basic_curr;
    cmd->cc_magic           =       cid_https_port_add;
    cmd->cc_revmap_order =          cro_delivery;
    cmd->cc_revmap_type =           crt_manual;
    cmd->cc_exec_callback  =        cli_ssld_config_port;
    cmd->cc_revmap_names =          "/nkn/ssld/delivery/config/server_port/*";
    cmd->cc_revmap_cmds =           "delivery protocol https listen port $v$";
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol https";
    cmd->cc_help_descr =            N_("Clear https related configuration");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol https listen port";
    cmd->cc_help_descr =            N_("Reset listening port parameters");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol https listen port *";
    cmd->cc_help_exp =              N_("<port number>");
    cmd->cc_help_exp_hint =         N_("for example, listen port 443 444 445-450");
    cmd->cc_help_use_comp =         true;
    cmd->cc_comp_type =             cct_matching_values;
    cmd->cc_comp_pattern =          "/nkn/ssld/delivery/config/server_port/*";
    cmd->cc_flags =                 ccf_terminal | ccf_allow_extra_params;
    cmd->cc_capab_required =        ccp_set_basic_curr;
    cmd->cc_exec_callback  =        cli_ssld_config_port;
    cmd->cc_magic           =       cid_https_port_delete;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol https interface";
    cmd->cc_help_descr =            N_("Set the interface on which https will run");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol https interface *";
    cmd->cc_help_exp =              N_("<Interface name>");
    cmd->cc_help_exp_hint =         N_("for example, eth0");
    cmd->cc_help_use_comp =         true;
    cmd->cc_comp_type =             cct_matching_values ;
    cmd->cc_comp_pattern =          "/nkn/ssld/config/delivery/server_intf/*";
    cmd->cc_flags =                 ccf_terminal | ccf_allow_extra_params;
    cmd->cc_capab_required =        ccp_set_basic_curr;
    cmd->cc_revmap_order =          cro_delivery;
    cmd->cc_revmap_type =           crt_manual;
    cmd->cc_exec_callback  =        cli_ssld_https_interface_config;
    cmd->cc_revmap_names =          "/nkn/ssld/delivery/config/server_intf/*";
    cmd->cc_revmap_cmds =           "delivery protocol https interface $v$";
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol https interface";
    cmd->cc_help_descr =            N_("Clear https listen interfaces");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "no delivery protocol https interface *";
    cmd->cc_help_exp =              N_("<Interface name>");
    cmd->cc_help_exp_hint =         N_("for example, eth0");
    cmd->cc_help_use_comp =         true;
    cmd->cc_comp_type =             cct_matching_values;
    cmd->cc_comp_pattern =          "/nkn/ssld/delivery/config/server_intf/*";
    cmd->cc_flags =                 ccf_terminal | ccf_allow_extra_params;
    cmd->cc_capab_required =        ccp_set_basic_curr;
    cmd->cc_exec_callback  =        cli_ssld_https_interface_config;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol https conn-pool";
    cmd->cc_help_descr =            "Configure connection pooling parameters";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol https conn-pool origin";
    cmd->cc_help_descr =            "Configure origin side connection pooling parmeters";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol https conn-pool origin enable";
    cmd->cc_help_descr =            "Enable origin side connection pooling";
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/ssld/delivery/config/conn-pool/origin/enabled";
    cmd->cc_exec_value =            "true";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_delivery;
    //cmd->cc_revmap_callback =       cli_nvsd_http_revmap;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol https conn-pool origin disable";
    cmd->cc_help_descr =            "Disable origin side connection pooling";
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_priv_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/ssld/delivery/config/conn-pool/origin/enabled";
    cmd->cc_exec_value =            "false";
    cmd->cc_exec_type =             bt_bool;
    cmd->cc_exec_callback = 		cli_ssld_https_conn_pool_disable;
    cmd->cc_revmap_type =           crt_auto;
    cmd->cc_revmap_order =          cro_delivery;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol https conn-pool origin timeout";
    cmd->cc_help_descr =            "Configure timeout value for connections in the origin side connection pool";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =             "delivery protocol https conn-pool origin timeout *";
    cmd->cc_help_exp =              "<seconds>";
    cmd->cc_help_exp_hint =         "Timeout, Default: 90 seconds";
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/ssld/delivery/config/conn-pool/origin/timeout";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_type =             bt_uint32;
    cmd->cc_revmap_type =           crt_manual;
    cmd->cc_revmap_order =          cro_delivery;
    cmd->cc_revmap_names =          "/nkn/ssld/delivery/config/conn-pool/origin/timeout";
    cmd->cc_revmap_cmds =           "delivery protocol http conn-pool origin timeout $v$";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"delivery protocol https conn-pool origin max-conn";
    cmd->cc_help_descr = 	"Configure maximum number of connections that can be"
		    " opened to the origin server concurrently";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str = 	"delivery protocol https conn-pool origin max-conn *";
    cmd->cc_help_exp = 		"<number>";
    cmd->cc_help_exp_hint = 	"Number of connections, Default: 4096, max: 128000, 0: demand driven";
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_set_rstr_curr;
    cmd->cc_exec_operation =        cxo_set;
    cmd->cc_exec_name =             "/nkn/ssld/delivery/config/conn-pool/origin/max-conn";
    cmd->cc_exec_value =            "$1$";
    cmd->cc_exec_type =             bt_uint32;
    cmd->cc_revmap_type =           crt_manual;
    cmd->cc_revmap_order =          cro_delivery;
    cmd->cc_revmap_names =          "/nkn/ssld/delivery/config/conn-pool/origin/max-conn";
    cmd->cc_revmap_cmds =           "delivery protocol https conn-pool origin max-conn $v$";
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =             "show delivery protocol https";
    cmd->cc_help_descr =            "Display HTTPS configuration options";
    cmd->cc_flags =                 ccf_terminal;
    cmd->cc_capab_required =        ccp_query_rstr_curr;
    cmd->cc_exec_callback =         cli_ssld_https_show_cmd;
    CLI_CMD_REGISTER;


bail:
    return err;
}

int cli_ssld_https_conn_pool_disable(void *data,
		const cli_command *cmd,
		const tstr_array *cmd_line,
		const tstr_array *params)
{
	int err = 0;
	uint32 code = 0;	
	tstring *ret_msg = NULL;

	bn_request *req = NULL;

	/*create the request message to set the config nodes*/
	err = bn_set_request_msg_create(&req, 0);
	bail_error_null(err, req);

	err = bn_set_request_msg_append_new_binding(req, bsso_reset, 0,
				"/nkn/ssld/delivery/config/conn-pool/origin/timeout",
				bt_uint32,
				0,
				"90", NULL);
	bail_error(err);

	err = bn_set_request_msg_append_new_binding(req, bsso_reset, 0,
				"/nkn/ssld/delivery/config/conn-pool/origin/max-conn",
				bt_uint32, 
				0, 
				"4096", 
				NULL);
	bail_error(err);

	err = mdc_send_mgmt_msg(cli_mcc, req, false, NULL, &code, &ret_msg);
	bail_error(err);

bail:
	bn_request_msg_free(&req);
	ts_free(&ret_msg);
	return err;
}

int cli_ssld_https_show_cmd(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
    int err = 0;
    uint32 num_intfs = 0;
    uint32 num_ports = 0;
    tbool t_conn_pool_status = true;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    err = cli_printf(
            _("Delivery Protocol : HTTPS\n"));
    bail_error(err);

    err = cli_printf(
            _("Server Ports: \n"));
    bail_error(err);

    err = mdc_foreach_binding(cli_mcc, "/nkn/ssld/delivery/config/server_port/*", NULL,
            cli_ssld_https_server_port_show, NULL, &num_ports);
    bail_error(err);
    if (num_ports == 0) {
        cli_printf(
                _("     NONE\n"));
    }

    err = cli_printf(
            _("\nInterfaces:\n"));
    bail_error(err);
    err = mdc_foreach_binding(cli_mcc,
            "/nkn/ssld/delivery/config/server_intf/*", NULL,
            cli_ssld_https_show_interface, NULL, &num_intfs);
    bail_error(err);

    if (num_intfs == 0) {
        err = cli_printf(
                _("     Default (All Configured interfaces)\n"));
        bail_error(err);
    }
    /*Show Connection  pool status*/

    err = mdc_get_binding_tbool(cli_mcc, NULL, NULL, NULL, 
		    &t_conn_pool_status, 
		    "/nkn/ssld/delivery/config/conn-pool/origin/enabled", NULL);
    bail_error(err);

    err = cli_printf(
		_("\nOrigin Connection Pooling: %s\n"), 
		t_conn_pool_status ? "Enabled" : "Disabled");
    bail_error(err);
    /*Show connection pool timeout value*/
    cli_printf_query(_(
                "    Connection Pooling Timeout       : #e:N.A.~/nkn/ssld/delivery/config/conn-pool/origin/timeout# (seconds)\n"));

    /*Show connection pool max-arp-entry value*/
    cli_printf_query(_(
                "    Connection Pooling Max-connection: #e:N.A.~/nkn/ssld/delivery/config/conn-pool/origin/max-conn# \n"));


bail :
    return err;
}

int
cli_ssld_https_server_port_show(const bn_binding_array *bindings,
                            uint32 idx, const bn_binding *binding,
                            const tstring *name,
                            const tstr_array *name_components,
                            const tstring *name_last_part,
                            const tstring *value, void *callback_data)
{
    int err = 0;
    char *port = 0;

    UNUSED_ARGUMENT(bindings);
    UNUSED_ARGUMENT(name);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(name_last_part);
    UNUSED_ARGUMENT(value);
    UNUSED_ARGUMENT(callback_data);

    if ((idx != 0) && ((idx % 3) == 0)) {
        cli_printf(_("\n"));
    }

    err = bn_binding_get_str(binding, ba_value, bt_uint16, NULL, &port);
    bail_error(err);

    err = cli_printf(
            _("     %-5s"), port);
    bail_error(err);


bail:
    return err;

}

int
cli_ssld_https_show_interface(const bn_binding_array *bindings,
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

int
cli_ssld_https_interface_config(
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

        bname = smprintf("/nkn/ssld/delivery/config/server_intf/%s", if_name);
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

    err = cli_printf("warning: if command successful, please restart mod-ssl service for change to take effect\n");
    bail_error(err);

bail:
    safe_free(bname);
    return err;
}

int
cli_ssld_config_port(void *data, const cli_command *cmd,
			const tstr_array *cmd_line,
			const tstr_array *params)

{
    int err = 0;
    bn_binding_array *barr = NULL;
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
    case cid_https_port_add:
        
        err = bn_binding_array_new(&barr);
        bail_error(err);

        for (i = 0; i < num_params; i++) {

            param = tstr_array_get_str_quick(params, i);
            bail_null(param);

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

			str_value_t port_nd = {0};

			snprintf(port_nd, sizeof(port_nd), "/nkn/ssld/delivery/config/server_port/%s",
					port_num);
			err = mdc_set_binding(cli_mcc,
					&code,
					&msg,
					bsso_create,
					bt_uint16,
					port_num,
					port_nd);
			bail_error(err);


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

		str_value_t port_nd = {0};

		snprintf(port_nd, sizeof(port_nd), "/nkn/ssld/delivery/config/server_port/%s",
				port_num);
		err = mdc_set_binding(cli_mcc,
				&code,
				&msg,
				bsso_create,
				bt_uint16,
				port_num,
				port_nd);
		bail_error(err);
		if (code != 0) {
			cli_printf(_("error : %s\n"), ts_str(msg));
			break;
		}

            }
        }

        break;

    case cid_https_port_delete:
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
			str_value_t port_nd = {0};

			snprintf(port_nd, sizeof(port_nd), "/nkn/ssld/delivery/config/server_port/%s",
					n);

			err = mdc_delete_binding(cli_mcc, &code, NULL, port_nd);

                    safe_free(n);
                }
            }
	    else {
		    uint16 start = strtol(param, NULL, 10);
		    char *n = smprintf("%d", start);;
		    str_value_t port_nd = {0};

		    snprintf(port_nd, sizeof(port_nd), "/nkn/ssld/delivery/config/server_port/%s",
				    n);

		    err = mdc_delete_binding(cli_mcc, &code, NULL, port_nd);

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
        cli_printf("warning: if command successful, please restart mod-ssld service for change to take effect\n");
    bn_binding_array_free(&barr);
    return(err);
}

