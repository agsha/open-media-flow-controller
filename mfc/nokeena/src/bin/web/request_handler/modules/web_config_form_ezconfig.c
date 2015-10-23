/*
 *
 * Filename:  web_config_form_ezconfig.c
 * Date:      2008/12/26
 * Author:    Chitradevi R
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.  
 * All rights reserved.
 *
 *
 */

#include <string.h>
#include "common.h"
#include "dso.h"
#include "web.h"
#include "web_config_form.h"
#include "web_internal.h"
#include "md_client.h"

/*-----------------------------------------------------------------------------
 * CONSTANTS
 */

static const char wcf_action_name[] = "config-form-ezconfig";
static const char wcf_hostname[] = "/system/hostname";
static const char wcf_domainname[] = "/resolver/domain_search";
static const char wcf_dns[] = "/resolver/nameserver";
static const char wcf_field_fqdn[] = "f_fqdn";
static const char wcf_field_dns[] = "f_dns";
static const char wcf_id_fqdn[] = "fqdn";
static const char wcf_id_dns[] = "dns";
static const char wcf_btn_apply[] = "apply";
static const char wcf_del_delimiter_char = ' ';

/*-----------------------------------------------------------------------------
 * PROTOTYPES
 */

static int wcf_handle_apply(void);
static web_action_result wcf_ezconfig_form_handler(web_context *ctx,
                                                void *param);

int web_config_form_ezconfig_init(const lc_dso_info *info, void *data);

/*-----------------------------------------------------------------------------
 * IMPLEMENTATION
 */

static int
wcf_handle_apply(void)
{
    const bn_response *resp = NULL;
    bn_binding *binding = NULL;
    bn_request *req = NULL;
    const tstring *fqdn_value = NULL;
    tstring *fqdn_fixed = NULL;
    const tstring *dns_value = NULL;
    tstring *dns_fixed = NULL;
    tstring *msg = NULL;
    char *node_name = NULL;
    char *gid_str = NULL;
    uint32 gid = 0;
    uint32 code = 0;
    int err = 0;
    tstr_array *commands = NULL;
    tstr_array *domain_array = NULL;
    const tstring *command = NULL;
    uint32 num_commands = 0, i =0;
    tstring *domain_name = NULL;
    tstring *hostname = NULL;
    int required = 0 ;

    err = web_get_request_param_str(g_web_data, wcf_field_fqdn, ps_post_data,
                                    &fqdn_value);
    bail_error(err);
    if (fqdn_value == NULL || ts_num_chars(fqdn_value) <= 0) {
        err = ts_new_str(&msg, _("No Host Name specified."));
        bail_error(err);
        code = 1;
        goto check_error;
    }
    
    err = ts_dup(fqdn_value, &fqdn_fixed);
    bail_error(err);

    err = ts_trim_whitespace(fqdn_fixed);
    bail_error(err);

    fqdn_value = fqdn_fixed;

    if (fqdn_value == NULL || ts_num_chars(fqdn_value) <= 0) {
        err = ts_new_str(&msg, _("No Host Name specified."));
        bail_error(err);
        code = 1;
        goto check_error;
    }

    /* Create the ezconfig index */
    node_name = smprintf("%s",wcf_hostname);
    bail_null(node_name);

    err = ts_tokenize(fqdn_value,'.',0,0,
                      ttf_ignore_leading_separator |
                      ttf_ignore_trailing_separator |
                      ttf_single_empty_token_null,&commands);
    bail_error(err);

    if (commands != NULL ) {
        err = tstr_array_length(commands,&num_commands);
        bail_error(err);
    }
        
    if ( num_commands < 3 )
    {
        err = ts_new_str(&msg,_("Enter Fully Qualified Domain Name.ex. a.b.c format"));
        bail_error(err);
        goto check_error;
    }
    err = tstr_array_new(&domain_array, NULL);
    bail_error(err);
 
    hostname = tstr_array_get_quick(commands,0);
    for ( i = 1 ; i < num_commands ; i++) {
        command = tstr_array_get_quick(commands,i);
        bail_null(commands);
        err = tstr_array_append(domain_array,command);
        bail_error(err);

    }

    err = ts_join_tokens(domain_array,'.','\0' ,'\0',0,&domain_name);
    bail_error(err);

    err = mdc_create_binding(web_mcc, NULL, NULL, bt_hostname, 
    				ts_str(hostname), node_name);
    bail_error(err);

    err = mdc_array_append_single
		(web_mcc,
		"/resolver/domain_search",
		"domainname",
		bt_hostname,
		ts_str(domain_name),
		true,NULL,&code,&msg);
    bail_error(err);

   /*! Add option to include DNS server */
    err = web_get_request_param_str(g_web_data, wcf_field_dns, ps_post_data,
                                    &dns_value);
    bail_error(err);

    if (dns_value == NULL || ts_num_chars(dns_value) <= 0) {
        if ( required ) {
            err = ts_new_str(&msg, _("No DNS server name specified."));
            bail_error(err);
             code = 1;
        } 
        goto check_error;
    }
    
    err = ts_dup(dns_value, &dns_fixed);
    bail_error(err);

    err = ts_trim_whitespace(dns_fixed);
    bail_error(err);

    dns_value = dns_fixed;

    if (dns_value == NULL || ts_num_chars(dns_value) <= 0) {
        goto check_error;
    }

    if (code != 0 ) {  goto check_error; }

    err = mdc_array_append_single
		(web_mcc,
		"/resolver/nameserver",
		"address",
		bt_ipv4addr,
		ts_str(dns_value),
		true,NULL,&code,&msg);
    bail_error(err);
   

 check_error:
    err = web_set_msg_result(g_web_data, code, msg);
    bail_error(err);

    if (!code) {
        err = web_clear_post_data(g_web_data);
        bail_error(err);
    }

 bail:
    bn_binding_free(&binding);
    bn_request_msg_free(&req);
    ts_free(&fqdn_fixed);
    ts_free(&msg);
    safe_free(gid_str);
    safe_free(node_name);
    tstr_array_free(&commands);
    tstr_array_free(&domain_array);
    //ts_free(&hostname);
    ts_free(&domain_name);

    return(err);
}

static web_action_result
wcf_ezconfig_form_handler(web_context *ctx, void *param)
{
    int err = 0;

    if (web_request_param_exists_str(g_web_data, wcf_btn_apply, ps_post_data)) {
        err = wcf_handle_apply();
        bail_error(err);
    } else {
        err = web_process_form_buttons(g_web_data);
        bail_error(err);
    }

 bail:
    if (err) {
        return(war_error);
    } else {
        return(war_ok);
    }
}

int
web_config_form_ezconfig_init(const lc_dso_info *info, void *data)
{
    int err = 0;

    /*
     * the context contains useful information including a pointer
     * to the TCL interpreter so that modules can register their own
     * TCL commands.
     */

    /*
     * register all commands that are handled by this module.
     */
    err = web_register_action_str(g_rh_data, wcf_action_name, NULL,
                                  wcf_ezconfig_form_handler);
    bail_error(err);

 bail:
    return(err);
}
