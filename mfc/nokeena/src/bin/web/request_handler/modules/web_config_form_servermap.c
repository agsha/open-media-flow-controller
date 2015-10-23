/*
 *
 * Filename:  web_config_form_servermap.c
 * Date: 
 * Author:    Dhivyamanohary R
 *
 * Juniper Networks, Inc
 */

#include <string.h>
#include "common.h"
#include "dso.h"
#include "web.h"
#include "web_internal.h"
#include "web_config_form.h"
#include "timezone.h"

/*-----------------------------------------------------------------------------
 * CONSTANTS
 */

static const char wcf_servermap_name[] = "config-form-action-servermap";
static const char wcf_field_name[] = "f_name";

static const char wcf_get_mapids_cmd_name[] = "tms::get-mapids";

/*-----------------------------------------------------------------------------
 * PROTOTYPES
 */
static int wcf_get_mapids_cmd(ClientData clientData,
	Tcl_Interp *interpreter,
	int objc,
	Tcl_Obj *CONST objv[]);
static int wcf_handle_set(uint32 *ret_code);
static web_action_result wcf_servermap_form_handler(web_context *ctx,
	void *param);
int web_config_form_servermap_init(const lc_dso_info *info, void *data);

/*-----------------------------------------------------------------------------
 * IMPLEMENTATION
 */
static int
wcf_get_mapids_cmd(ClientData clientData,
	Tcl_Interp *interpreter,
	int objc,
	Tcl_Obj *CONST objv[])
{
    Tcl_Obj *ret_list = NULL;
    Tcl_Obj *sm_obj = NULL;
    uint32 len = 0;
    uint32 i = 0;
    int err = 0;
    tstr_array *labels_config = NULL;
    const char *t_disk_id = NULL;
    
    ret_list = Tcl_NewListObj(0, NULL);
    bail_null(ret_list);
    
    err = mdc_get_binding_children_tstr_array(web_mcc,
	    NULL, NULL, &labels_config,
	    "/nkn/nvsd/server-map/config", NULL);
    bail_error_null(err, labels_config);
    /* For each map names*/
    /* Loop the labels_config array and get all names.*/
    for(; i < tstr_array_length_quick(labels_config); i++) 
    {
	// Get a name from i-th location in the labels_cnfig array
	t_disk_id = tstr_array_get_str_quick(labels_config, i);
	// Get length of name
	len = strlen(t_disk_id);
	// Create a tcl string object
	sm_obj = Tcl_NewStringObj(t_disk_id, len); 
	bail_null(sm_obj);
	// append the string object into a tcl object.
	err = Tcl_ListObjAppendElement(interpreter, ret_list, sm_obj);
	bail_require(err == TCL_OK);
    }
    
    // set the tcl name list as return.
    Tcl_SetObjResult(interpreter, ret_list);
    ret_list = NULL;

bail:
    tstr_array_free(&labels_config);
    if (ret_list) {
	Tcl_DecrRefCount(ret_list);
    }
    if (err) {
	return TCL_ERROR;
    } else {
	return TCL_OK;
    }
}


/* called when user clicks something */
static int
wcf_handle_set(uint32 *ret_code)
{
    static const char wcf_field_smapname[] = "f_smap";
    const bn_response *resp = NULL;
    bn_request *req = NULL;
    bn_binding *binding = NULL;
    const tstring *servermap_value = NULL;
    const char *c_servermap_name=NULL;
    tstring *ret_msg = NULL;
    uint32 ret_err=0;
    tstring *msg = NULL;
    uint32 code = 0;
    int err = 0;
    
    if (ret_code) {
	*ret_code = 0;
    }
    
    err = web_get_request_param_str(g_web_data, wcf_field_smapname, ps_post_data,
	    &servermap_value);
    bail_error(err);
    
    err = bn_action_request_msg_create(&req, "/nkn/nvsd/server-map/actions/refresh-force");
    bail_error(err);
    
    err = bn_action_request_msg_append_new_binding(
	    req, 0, "name", bt_string,ts_str(servermap_value), NULL);
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
wcf_servermap_form_handler(web_context *ctx, void *param)
{
    uint32 code = 0;
    int err = 0;
    
    err = wcf_handle_set(&code);
    bail_error(err);
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
web_config_form_servermap_init(const lc_dso_info *info, void *data)
{
    Tcl_Command cmd = NULL;
    web_context *ctx = NULL;
    int err = 0;
    
    bail_null(data);
    
    /*
     * * the context contains useful information including a pointer
     * * to the TCL interpreter so that modules can register their own
     * * TCL commands.
     * */
    ctx = (web_context *)data;
    
    /*
     * * register all commands that are handled by this module.
     * */
    err = web_register_action_str(g_rh_data, wcf_servermap_name, NULL,
	    wcf_servermap_form_handler);
    bail_error(err);

    /*register TCL commands */
    cmd = Tcl_CreateObjCommand(
	    ctx->wctx_interpreter, (char *)wcf_get_mapids_cmd_name,
	    wcf_get_mapids_cmd, NULL, NULL);
    bail_null(cmd);
    
bail:
    return(err);
}
