/*
 *
 * Filename:  web_config_form_namespace.c
 * Date:      2008/12/25
 * Author:    Ramanand Narayanan
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
enum {
HTTP=1,
NFS=2,
};
static const char wcf_action_name[] = "config-form-namespace";
static const char wcf_namespace_prefix[] = "/nkn/nvsd/namespace";
static const char wcf_namespace_active[] = "/nkn/nvsd/namespace/%s/status/active";
static const char wcf_virtual_player[] = "/nkn/nvsd/namespace/%s/virtual_player";
static const char wcf_service_prefix[] = "/nkn/nvsd/uri/config";
static const char wcf_uri_node_format[] = "/nkn/nvsd/namespace/%s/match/uri/uri_name";
static const char wcf_domain_node_format[] = "/nkn/nvsd/namespace/%s/domain/host";
static const char wcf_httporiginserver_node_format[] = "/nkn/nvsd/namespace/%s/origin-server/http/host/name";
static const char wcf_nfsoriginserver_node_format[] = "/nkn/nvsd/namespace/%s/origin-server/nfs/host";
static const char wcf_field_namespace[] = "f_namespace";
static const char wcf_field_uri[] = "f_uri";
static const char wcf_field_domain[] = "f_domain";
static const char wcf_field_http[] = "f_http";
static const char wcf_field_nfs[] = "f_nfs";
static const char wcf_field_om_type[] = "f_omtype";
static const char wcf_om_type_http[] = "http";
static const char wcf_om_type_nfs[] = "nfs";
static const char wcf_field_httporiginserver[] = "f_httphostname";
static const char wcf_field_nfsoriginserver[] = "f_nfshostname";
static const char wcf_field_originserver[] = "f_hostname";
static const char wcf_field_virtual_player[] = "f_virtual_player";
static const char wcf_id_namespace[] = "namespace";
static const char wcf_id_uri[] = "uri";
static const char wcf_id_domain[] = "domain";
static const char wcf_id_originserver[] = "hostname";
static const char wcf_id_http[] = "http";
static const char wcf_id_nfs[] = "nfs";
static const char wcf_id_httphostname[] = "httphostname";
static const char wcf_id_nfshostname[] = "nfshostname";
static const char wcf_id_virtual_player[] = "virtual_player";
static const char wcf_btn_add[] = "add";
static const char wcf_del_delimiter_char = ' ';

/*-----------------------------------------------------------------------------
 * PROTOTYPES
 */

static int wcf_handle_add(void);
static web_action_result wcf_namespace_form_handler(web_context *ctx,
                                                void *param);

int web_config_form_namespace_init(const lc_dso_info *info, void *data);

/*-----------------------------------------------------------------------------
 * IMPLEMENTATION
 */

static int
wcf_handle_add(void)
{
    const bn_response *resp = NULL;
    bn_binding *binding = NULL;
    bn_request *req = NULL;
    const tstring *om_type_str = NULL;
    const tstring *namespace_value = NULL;
    const tstring *uri_value = NULL;
    const tstring *domain_value = NULL;
    const tstring *http_value = NULL;
    const tstring *nfs_value = NULL;
    const tstring *originserver_value = NULL;
    const tstring *httporiginserver_value = NULL;
    const tstring *nfsoriginserver_value = NULL;
    const tstring *virtual_player_value = NULL;
    tstring *namespace_fixed = NULL;
    tstring *uri_fixed = NULL;
    tstring *domain_fixed = NULL;
    tstring *originserver_fixed = NULL;
    tstring *httporiginserver_fixed = NULL;
    tstring *nfsoriginserver_fixed = NULL;
    tstring *virtual_player_fixed = NULL;
    tstring *msg = NULL;
    char *node_name = NULL;
    char *gid_str = NULL;
    uint32 gid = 0;
    uint32 code = 0;
    int err = 0;
    char *uri_value_esc;
    uint32 om_type = 0;

    /*! Reading Namespace field */
    err = web_get_request_param_str(g_web_data, wcf_field_namespace, ps_post_data,
                                    &namespace_value);
    bail_error(err);
    if (namespace_value == NULL || ts_num_chars(namespace_value) <= 0) {
        err = ts_new_str(&msg, _("No namespace specified for the new entry."));
        bail_error(err);
        code = 1;
        goto check_error;
    }
    err = web_type_check_form_field_str(g_web_data, wcf_id_namespace, &code, &msg);
    bail_error(err);
    if (code) { goto check_error; }
    else { ts_free(&msg); }

    err = ts_dup(namespace_value, &namespace_fixed);
    bail_error(err);

    err = ts_trim_whitespace(namespace_fixed);
    bail_error(err);

    namespace_value = namespace_fixed;
   
    /*! Reading URI field */
    err = web_get_request_param_str(g_web_data, wcf_field_uri, ps_post_data,
                                    &uri_value);
    bail_error(err);

    if (uri_value == NULL || ts_num_chars(uri_value) <= 0) {
        err = ts_new_str(&msg, _("No URI specified for the new Namespace."));
        bail_error(err);
        code = 1;
        goto check_error;
    }

    err = web_type_check_form_field_str(g_web_data, wcf_id_uri,
                                            &code, &msg);
    bail_error(err);
    if (code) { goto check_error; }
    else { ts_free(&msg); }

    err = ts_dup(uri_value, &uri_fixed);
    bail_error(err);

    err = ts_trim_whitespace(uri_fixed);
    bail_error(err);

    uri_value = uri_fixed;
    /*! Reading Domain field */
    err = web_get_request_param_str(g_web_data, wcf_field_domain, ps_post_data,
                                    &domain_value);
    bail_error(err);

    if (domain_value == NULL || ts_num_chars(domain_value) <= 0) {
        err = ts_new_str(&msg, _("No Domain specified for the new Namespace."));
        bail_error(err);
        code = 1;
        goto check_error;
    }

    err = web_type_check_form_field_str(g_web_data, wcf_id_domain,
                                            &code, &msg);
    bail_error(err);
    if (code) { goto check_error; }
    else { ts_free(&msg); }

    err = ts_dup(domain_value, &domain_fixed);
    bail_error(err);

    err = ts_trim_whitespace(domain_fixed);
    bail_error(err);

    domain_value = domain_fixed;

    /*! Escape URI Field Name */
    err = bn_binding_name_escape_str(ts_str(uri_fixed),&uri_value_esc);
    bail_error_null(err,uri_value_esc);

    err = web_get_request_param_str(g_web_data, wcf_field_om_type,
                                    ps_post_data, &om_type_str);
    bail_error_null(err, om_type_str);

    if (ts_equal_str(om_type_str, wcf_om_type_http, true)) {
        om_type |= HTTP;
    }
    else if ( ts_equal_str(om_type_str, wcf_om_type_nfs, true)) {
        om_type |= NFS;
    }
    else {
        bail_force(lc_err_unexpected_case);
    }

    if ( om_type & HTTP ) {
        /*! Read HTTP Origin Server Details */
        err = web_get_request_param_str(g_web_data, wcf_field_httporiginserver, 
                         ps_post_data, &httporiginserver_value);
        bail_error(err);
        if (httporiginserver_value == NULL || ts_num_chars(httporiginserver_value) <= 0) {
            err = ts_new_str(&msg, _("No HTTP Origin Server specified for the new Namespace."));
            bail_error(err);
            code = 1;
            goto check_error;
        }
      
        err = web_type_check_form_field_str(g_web_data, wcf_id_httphostname,
                                            &code, &msg);
        bail_error(err);
        if (code) { goto check_error; }
        else { ts_free(&msg); }

        err = ts_dup(httporiginserver_value, &httporiginserver_fixed);
        bail_error(err);

        err = ts_trim_whitespace(httporiginserver_fixed);
        bail_error(err);
        httporiginserver_value = httporiginserver_fixed;
    }

    if ( om_type & NFS ) {
        /*! Read NFS Origin Server Details */
        err = web_get_request_param_str(g_web_data, wcf_field_nfsoriginserver, 
                         ps_post_data, &nfsoriginserver_value);
        bail_error(err);
        if (nfsoriginserver_value == NULL || ts_num_chars(nfsoriginserver_value) <= 0) {
            err = ts_new_str(&msg, _("No NFS Origin Server specified for the new Namespace."));
            bail_error(err);
            code = 1;
            goto check_error;
        }
      
        err = web_type_check_form_field_str(g_web_data, wcf_id_nfshostname,
                                            &code, &msg);
        bail_error(err);
        if (code) { goto check_error; }
        else { ts_free(&msg); }

        err = ts_dup(nfsoriginserver_value, &nfsoriginserver_fixed);
        bail_error(err);

        err = ts_trim_whitespace(nfsoriginserver_fixed);
        bail_error(err);
        nfsoriginserver_value = nfsoriginserver_fixed;
    }

    /*! Read Virtual Player Configuration  */
    err = web_get_request_param_str(g_web_data, wcf_field_virtual_player, 
                     ps_post_data, &virtual_player_value);
    bail_error(err);

    if (virtual_player_value != NULL && ts_num_chars(virtual_player_value) > 0) {
      
        err = web_type_check_form_field_str(g_web_data, wcf_id_virtual_player,
                                            &code, &msg);
        bail_error(err);
        if (code) { goto check_error; }
        else { ts_free(&msg); }

        err = ts_dup(virtual_player_value, &virtual_player_fixed);
        bail_error(err);

        err = ts_trim_whitespace(virtual_player_fixed);
        bail_error(err);
        virtual_player_value = virtual_player_fixed;
    }

    /* Create the namespace index */
    node_name = smprintf("%s/%s", wcf_namespace_prefix, ts_str(namespace_value));
    bail_null(node_name);

    err = mdc_create_binding(web_mcc, NULL, NULL, bt_string, 
    				ts_str(namespace_value), node_name);
    bail_error(err);
    safe_free(node_name);

    /* Create the uri node */
    node_name = smprintf(wcf_uri_node_format, ts_str(namespace_value));
    bail_null(node_name);

    err = mdc_create_binding(web_mcc, NULL, NULL, bt_uri, 
    				ts_str(uri_value), node_name);
    bail_error(err);
    safe_free(node_name);
    
    /*! Create Domain Name Binding */
    node_name = smprintf(wcf_domain_node_format, ts_str(namespace_value));
    err = mdc_create_binding(web_mcc, NULL, NULL, bt_string,
                                ts_str(domain_value), node_name);
    bail_error(err);
    safe_free(node_name);
#if 0
    /* Create the service index */
    node_name = smprintf("%s/%s", wcf_service_prefix, ts_str(namespace_value));
    bail_null(node_name);

    err = mdc_create_binding(web_mcc, NULL, NULL, bt_string, 
    				ts_str(namespace_value), node_name);
    bail_error(err);
    safe_free(node_name);
#endif
    /* Create the Virtual player binding */
    if ( virtual_player_value != NULL ) {
        node_name = smprintf(wcf_virtual_player, ts_str(namespace_value)); 
        bail_null(node_name);

        err = mdc_create_binding(web_mcc, NULL, NULL, bt_string, 
    				ts_str(virtual_player_value), node_name);
        bail_error(err);
        safe_free(node_name);
    }

    /* Create the HTTP originserver node */
    if ( om_type & HTTP ) {
        node_name = smprintf(wcf_httporiginserver_node_format, 
                     ts_str(namespace_value));
        bail_null(node_name);

        err = mdc_create_binding(web_mcc, NULL, NULL, bt_string, 
     	          ts_str(httporiginserver_value), node_name);
        bail_error(err);
        safe_free(node_name);
    }
    else {
        /* TODO : Delete the existing HTTP Origins */
    }

    if ( om_type & NFS ) {
        node_name = smprintf(wcf_nfsoriginserver_node_format, 
                     ts_str(namespace_value));
        bail_null(node_name);

        err = mdc_create_binding(web_mcc, NULL, NULL, bt_string, 
     		  ts_str(nfsoriginserver_value), node_name);
        bail_error(err);
        safe_free(node_name);
    }
    else {
        /* TODO : Delete the existing NFS Origins */
    }

    /*! Set the Namespce status to active */
    node_name = smprintf(wcf_namespace_active, 
                 ts_str(namespace_value));
    bail_null(node_name);

    err = mdc_create_binding(web_mcc, NULL, NULL, bt_bool, 
 		  "true", node_name);
    bail_error(err);
    safe_free(node_name);

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
    ts_free(&namespace_fixed);
    ts_free(&uri_fixed);
    ts_free(&msg);
    safe_free(gid_str);
    safe_free(node_name);
    return(err);
}

static web_action_result
wcf_namespace_form_handler(web_context *ctx, void *param)
{
    int err = 0;

    if (web_request_param_exists_str(g_web_data, wcf_btn_add, ps_post_data)) {
        err = wcf_handle_add();
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
web_config_form_namespace_init(const lc_dso_info *info, void *data)
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
                                  wcf_namespace_form_handler);
    bail_error(err);

 bail:
    return(err);
}
