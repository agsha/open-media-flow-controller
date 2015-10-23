/*
 *
 * Filename:  web_config_form_diskcache.c
 * Date:      2008/02/20
 * Author:    Chitra Devi R
 *
 * Nokeena Networks, Inc
 */

#include <string.h>
#include "common.h"
#include "dso.h"
#include "web.h"
#include "web_internal.h"
#include "web_config_form.h"
#include "timezone.h"

/*-----------------------------------------------------------------------------
 * INFO
 *
 * This module contains the following TCL commands:
 *
 *     tms::get-diskids
 *
 *         returns all the diskids
 *
 */

/*-----------------------------------------------------------------------------
 * CONSTANTS
 */

static const char wcf_get_diskids_cmd_name[] = "tms::get-diskids";
static const char wcf_diskaction_name[] = "config-form-diskcache";
static const char wcf_field_diskname[] = "f_diskname";
static const char wcf_field_diskaction[] = "f_diskaction";
static const char wcf_field_activate[] = "f_activate";
static const char wcf_field_cacheable[] = "f_cacheable";
static const char wcf_id_activate[] = "activate";
static const char wcf_id_deactivate[] = "deactivate";
static const char wcf_id_cacheable[] = "cacheable";
static const char wcf_id_notcacheable[] = "notcacheable";
static const char wcf_id_format[] = "format";
static const char wcf_id_repair[] = "repair";


/*-----------------------------------------------------------------------------
 * PROTOTYPES
 */

static int wcf_get_diskids_cmd(ClientData clientData,
                                 Tcl_Interp *interpreter,
                                 int objc,
                                 Tcl_Obj *CONST objv[]);

static int wcf_handle_set(uint32 *ret_code);
static web_action_result wcf_diskcache_form_handler(web_context *ctx,
                                               void *param);

int web_config_form_diskcache_init(const lc_dso_info *info, void *data);

/*-----------------------------------------------------------------------------
 * IMPLEMENTATION
 */

static int
wcf_get_diskids_cmd(ClientData clientData,
                      Tcl_Interp *interpreter,
                      int objc,
                      Tcl_Obj *CONST objv[])
{
    Tcl_Obj *ret_list = NULL;
    Tcl_Obj *disk_obj = NULL;
    uint32 num_zones = 0;
    uint32 len = 0;
    uint32 i = 0;
    int err = 0;
    tstr_array *labels = NULL;
    const char *diskname = NULL;
    

    ret_list = Tcl_NewListObj(0, NULL);
    bail_null(ret_list);
   
    err = mdc_get_binding_children_tstr_array(web_mcc,
            NULL, NULL, &labels,
            "/nkn/nvsd/diskcache/monitor/diskname", NULL);
    bail_error_null(err, labels);
    /* For each disk_id get the Disk Name */
    i = 0;
    diskname = tstr_array_get_str_quick (labels, i++);
    while (NULL != diskname)
    {
        char *node_name = NULL;
        tstring *t_disk_name = NULL;

        if ( diskname != NULL ) {
            len = strlen(diskname);
            disk_obj = Tcl_NewStringObj(diskname, len);
            bail_null(disk_obj);
            err = Tcl_ListObjAppendElement(interpreter, ret_list, disk_obj);
            bail_require(err == TCL_OK);
            err = 0;

            diskname = tstr_array_get_str_quick (labels, i++);
        } 
        else {
            //len = strlen(ts_str(diskname));
            //disk_obj = Tcl_NewStringObj(ts_str(diskname), len);
            //bail_null(disk_obj);
            diskname = tstr_array_get_str_quick (labels, i++);
           continue;
        }
    }

    Tcl_SetObjResult(interpreter, ret_list);
    ret_list = NULL;

 bail:
    tstr_array_free(&labels);
    if (ret_list) {
        Tcl_DecrRefCount(ret_list);
    }
    if (err) {
        return TCL_ERROR;
    } else {
        return TCL_OK;
    }
}

static int
wcf_handle_set(uint32 *ret_code)
{
    const bn_response *resp = NULL;
    bn_request *req = NULL;
    bn_binding *binding = NULL;
    const tstring *diskname_value = NULL;
    const tstring *action_value = NULL;
    const tstring *activate_value = NULL;
    const tstring *cacheable_value = NULL;
    char *node_name =NULL;
    const char *c_diskname=NULL;
    tstring *t_pre_format_str = NULL;
    tstring *ret_msg = NULL;
    int system_retval =0;
    uint32 ret_err=0;
    tstring *msg = NULL;
    uint32 code = 0;
    int err = 0;

    if (ret_code) {
        *ret_code = 0;
    }

    err = web_get_request_param_str(g_web_data, wcf_field_diskname, ps_post_data,
                                    &diskname_value);
    bail_error(err);


    err = web_get_request_param_str(g_web_data, wcf_field_diskaction, ps_post_data,
                                    &action_value);
    bail_error_null(err,action_value);
    
    if ((ts_equal_str(action_value , wcf_id_activate, true) 
	|| (ts_equal_str(action_value , wcf_id_deactivate, true)) )) {
        code = 1;
        err = bn_action_request_msg_create(&req, "/nkn/nvsd/diskcache/actions/activate");
        bail_error(err);

        err = bn_action_request_msg_append_new_binding(
            req, 0, "name", bt_string,ts_str(diskname_value), NULL);
        bail_error(err);
        if ( ts_equal_str(action_value , wcf_id_activate, true) ) {
            err = bn_action_request_msg_append_new_binding(
               req, 0, "activate", bt_bool,"true", NULL);
            bail_error(err);
        }
        else if (ts_equal_str(action_value , wcf_id_deactivate, true)) {
            err = bn_action_request_msg_append_new_binding(
               req, 0, "activate", bt_bool,"false", NULL);
            bail_error(err);
        }
        else
        {
            bail_error(err);
        }
       
        err = web_send_mgmt_msg(g_web_data, req, &resp);
        bail_error_null(err, resp);

        err = bn_response_get(resp, &code, &msg, false, NULL, NULL);
        bail_error(err);
        if (code != 0) {
            goto check_error; 
        } else { 
            err = web_set_msg_result(g_web_data, code, msg);
            bail_error(err);
            ts_free(&msg); 
        }
        
        bn_request_msg_free(&req);
         
    } else if ((ts_equal_str(action_value , wcf_id_cacheable, true) 
	|| (ts_equal_str(action_value , wcf_id_notcacheable, true)) )) {

        err = bn_action_request_msg_create(&req, "/nkn/nvsd/diskcache/actions/cacheable");
        bail_error(err);

        err = bn_action_request_msg_append_new_binding(
            req, 0, "name", bt_string,ts_str(diskname_value), NULL);
        bail_error(err);

        if (ts_equal_str(action_value , wcf_id_cacheable, true)) {
            err = bn_action_request_msg_append_new_binding(
                req, 0, "cacheable", bt_bool,"true", NULL);
            bail_error(err);
        } else if (ts_equal_str(action_value , wcf_id_notcacheable, true)) {
            err = bn_action_request_msg_append_new_binding(
                req, 0, "cacheable", bt_bool,"false", NULL);
            bail_error(err);
        } else {
            bail_error(err);
        }

        err = web_send_mgmt_msg(g_web_data, req, &resp);
        bail_error_null(err, resp);
        
        err = bn_response_get(resp, &code, &msg, false, NULL, NULL);
        bail_error(err);
        if (code != 0) {
            goto check_error; 
        } else { 
            err = web_set_msg_result(g_web_data, code, msg);
            bail_error(err);
            ts_free(&msg); 
        }
    } else if (ts_equal_str(action_value , wcf_id_format, true) ) { 
        c_diskname = ts_str(diskname_value);
        err = bn_action_request_msg_create(&req, "/nkn/nvsd/diskcache/actions/pre_format");
        bail_error(err);

        err = bn_action_request_msg_append_new_binding(
            req, 0, "diskname", bt_string,c_diskname, NULL);
        bail_error(err);
        /* Send the action message to get the returne code first */
        err = web_send_mgmt_msg(g_web_data, req, &resp);
        bail_error_null(err, resp);
        /* Now get the string that could be command or error string */
         node_name = smprintf ("/nkn/nvsd/diskcache/monitor/diskname/%s/pre_format", c_diskname);
        /* Now get the pre_format string */
        err = mdc_get_binding_tstr (web_mcc, NULL, NULL, NULL,
                        &t_pre_format_str, node_name, NULL);
        safe_free (node_name);
        bail_error(err);
           
        if (!t_pre_format_str || !ts_str(t_pre_format_str))
        {
          bail_error(err);
        }  
       
        system_retval = system (ts_str(t_pre_format_str));
        
        /* Send the post-format action to nvsd only if the command was successful */
        if (!system_retval) // 0 means succes else there was error
       { 
        ts_free(&ret_msg);
        err = mdc_send_action_with_bindings_str_va(web_mcc, &code, &ret_msg,
                "/nkn/nvsd/diskcache/actions/post_format", 1,
                "diskname", bt_string, c_diskname);
        bail_error(err);
       }  else {
        err = ts_new_sprintf(&msg, _("%s"),ts_str(t_pre_format_str));
        bail_error(err);       
        err = web_set_msg_result(g_web_data, 1, msg);
        bail_error(err);
        goto bail; 
          //failed to format disk
       }
        ts_free(&t_pre_format_str); 
        if (code != 0) {
            err = ts_new_sprintf(&msg, _("%s"),ts_str(ret_msg));
            bail_error(err);       
            goto check_error; 
        } else { 
            err = ts_new_sprintf(&msg, _("Disk format complete"));
            bail_error(err);
            err = web_set_msg_result(g_web_data, 0, msg);
            bail_error(err);
            ts_free(&msg); 
        }
    } else if (ts_equal_str(action_value , wcf_id_repair, true) ) { 

        err = bn_action_request_msg_create(&req, "/nkn/nvsd/diskcache/actions/repair");
        bail_error(err);

        err = bn_action_request_msg_append_new_binding(
            req, 0, "name", bt_string,ts_str(diskname_value), NULL);
        bail_error(err);

        err = web_send_mgmt_msg(g_web_data, req, &resp);
        bail_error_null(err, resp);
        
        err = bn_response_get(resp, &code, &msg, false, NULL, NULL);
        bail_error(err);

        if (code != 0) {
            goto check_error; 
        } else { 
            err = web_set_msg_result(g_web_data, code, msg);
            bail_error(err);
            ts_free(&msg); 
        }
    } else {
        bail_error(err);
    }
    

 check_error:
    err = web_set_msg_result(g_web_data, code, msg);
    bail_error(err);

    if (ret_code) {
        *ret_code = code;
    }

 bail:
    bn_request_msg_free(&req);
    bn_binding_free(&binding);
    ts_free(&msg);
    return(err);
}

static web_action_result
wcf_diskcache_form_handler(web_context *ctx, void *param)
{
    uint32 code = 0;
    int err = 0;

    if (!web_request_param_exists_str(g_web_data, web_btn_cancel,
                                      ps_post_data) &&
        !web_request_param_exists_str(g_web_data, web_btn_reset,
                                      ps_post_data)) {
        err = wcf_handle_set(&code);
        bail_error(err);
    }

    err = web_process_form_buttons(g_web_data);
    bail_error(err);

    if (!code) {
        err = web_clear_post_data(g_web_data);
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
web_config_form_diskcache_init(const lc_dso_info *info, void *data)
{
    Tcl_Command cmd = NULL;
    web_context *ctx = NULL;
    int err = 0;

    bail_null(data);

    /*
     * the context contains useful information including a pointer
     * to the TCL interpreter so that modules can register their own
     * TCL commands.
     */
    ctx = (web_context *)data;

    /*
     * register all commands that are handled by this module.
     */
    err = web_register_action_str(g_rh_data, wcf_diskaction_name, NULL,
                                  wcf_diskcache_form_handler);
    bail_error(err);

    /* register TCL commands */
    cmd = Tcl_CreateObjCommand(
        ctx->wctx_interpreter, (char *)wcf_get_diskids_cmd_name,
        wcf_get_diskids_cmd, NULL, NULL);
    bail_null(cmd);

 bail:
    return(err);
}

