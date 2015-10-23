/*
 *
 * Filename:  web_config_form_nsconfig.c
 * Date:      2009/12/11
 * Author:    Varsha R
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
static const char wcf_action_name[] = "config-form-delivery";
static const char wcf_delivery_prefix[] = "/nkn/nvsd/http/delivery";

static const char wcf_field_listenport[] = "f_listenport";
static const char wcf_field_ratecontrol[] = "f_ratecontrol";  

static const char wcf_btn_add[] = "add";
static const char wcf_btn_intfadd[] = "intfadd";
/*-----------------------------------------------------------------------------
 * PROTOTYPES
 */

static int wcf_handle_add(void);
static int wcf_handle_intfadd(void);
static web_action_result wcf_delivery_form_handler(web_context *ctx,
                                                void *param);

int web_config_form_delivery_init(const lc_dso_info *info, void *data);
int web_nvsd_http_add_interface(const bn_binding_array *bindings,
		uint32 idx, const bn_binding *binding,
		const tstring *name,
		const tstr_array *name_components,
		const tstring *name_last_part,
		const tstring *value, void *callback_data);

/*-----------------------------------------------------------------------------
 * IMPLEMENTATION
 */

static int
wcf_handle_add(void)
{
    const bn_response *resp = NULL;
    bn_binding *binding = NULL;
    bn_request *req = NULL;
    bn_binding_array *barr = NULL;
    const tstring *namespace_value = NULL;
    tstring *msg = NULL;
    tstring *namespace_fixed = NULL;
    char *node_name = NULL;
    uint32 ret_err = 0;
    int err = 0;
    const tstring *port_value = NULL;
    char *bn_delivery_port = NULL;
    char *bn_rate_control = NULL;
    const tstring *rate_control = NULL;
    const char *port = NULL;
    const char *param = NULL;
    tstring *tparam = NULL;
    char *port_num = NULL;
    char *p = NULL;
    uint32 i = 0;
    uint32 num_params = 0;
    uint16 j = 0;
    int32 ret_offset = 0;
    int32 start_offset = 0;
    int32 string_length = 0;

    err = web_get_request_param_str(g_web_data, wcf_field_listenport, ps_post_data,
                                    &port_value);
    bail_error(err);
    if (port_value == NULL ) {
        err = ts_new_str(&msg, _("No listen port value specified."));
        bail_error(err);
        ret_err = 1;
        goto check_error;
    }
    err = bn_binding_array_new(&barr);
    bail_error(err);
    string_length = ts_length(port_value);
    while((start_offset < string_length)&&(ret_offset != -1)){	
    	err  = ts_find_char(port_value,start_offset,' ',&ret_offset);
    	if(ret_offset != -1)
    	{
	        err = ts_substr(port_value,start_offset,ret_offset-start_offset,&tparam);
		bail_error(err);
		param = ts_str(tparam);
	        bail_null(param);
    		start_offset = ret_offset+1;
    	}else {
	        err = ts_substr(port_value,start_offset,string_length-start_offset,&tparam);
		bail_error(err);
		param = ts_str(tparam);
	        bail_null(param);
	}
               if ( NULL != (p = strstr(param, "-"))) {
    	/* p contains pointer to start of the next segment */
	tbool dup_deleted = false;
        int32 start = strtol(param, &p, 10);
        int32 end = strtol(p+1, NULL, 10);
		/*
		 * If any of the value is negative give out an error
		 */
	if( start < 0 || start > 65535 || end < 0 || end > 65535 ) {
        	err = ts_new_str(&msg, _("error:Out of range value given."
				"Enter values between 0 to 65535"));
	        bail_error(err);
        	ret_err = 1;
	        goto check_error;
	}

        for (j = start; j <= end; j++) {
	    port_num = smprintf("%d", j);
	    err = mdc_array_append_single(web_mcc, 
			    "/nkn/nvsd/http/config/server_port", 
			    "port",
			    bt_uint16,
			    port_num,
			    true, 
			    &dup_deleted,
			    &ret_err,
			    &msg);
           if (ret_err != 0) {
                    break;
            }
	   safe_free(port_num);
        }
           if (ret_err != 0) {
         	    goto check_error;
           }
       }else {
	     int32 start = strtol(param, NULL, 10);
		/*
		 * If any of the value is negative give out an error
		 */
		if( start < 0 || start > 65535 ) {
        		err = ts_new_str(&msg, _("error:Out of range value given."
					"Enter values between 0 to 65535"));
		        bail_error(err);
        		ret_err = 1;
	        	goto check_error;
		}
            
            err = bn_binding_new_uint16(&binding, "port", ba_value, 0, start);
            bail_error(err);

	    err = bn_binding_array_append_takeover(barr, &binding);
            bail_error(err);

	    err = mdc_array_append(web_mcc, 
                        "/nkn/nvsd/http/config/server_port", 
                        barr, true, 0, NULL, NULL, &ret_err, NULL);
	    bail_error(err);

            if (ret_err != 0) {
                    goto check_error;
            }
       }
    }

 check_error:
    err = web_set_msg_result(g_web_data, ret_err,msg);
    bail_error(err);

    if (!ret_err) {
        err = web_clear_post_data(g_web_data);
        bail_error(err);
    }

 bail:
    bn_binding_free(&binding);
    bn_request_msg_free(&req);
    ts_free(&namespace_fixed);
    ts_free(&msg);
    safe_free(node_name);
    return(err);
}
int
web_nvsd_http_add_interface(const bn_binding_array *bindings,
		uint32 idx, const bn_binding *binding,
		const tstring *name,
		const tstr_array *name_components,
		const tstring *name_last_part,
		const tstring *value, void *callback_data)
{
	int err = 0;
	tstring *interface = NULL;
	char *field_intf = NULL;
        char *bn_interface = NULL;
    	const tstring *interface_type = NULL;
	err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &interface);
	bail_error(err);
	field_intf = smprintf("f_%s",ts_str(interface));
	bail_null(field_intf);
    	err = web_get_request_param_str(g_web_data, field_intf, ps_post_data,
                                    &interface_type);
    	bail_error(err);
	if( (ts_equal_str(interface_type,"on",true))){
	    bn_interface = smprintf("/nkn/nvsd/http/config/interface/%s",ts_str(interface));
	    bail_null(bn_interface);
	    err = mdc_set_binding(web_mcc,NULL,NULL,bsso_create,
			    bt_string,ts_str(interface),bn_interface);
	    bail_error(err);

    }
bail:
    if (err) {
	    return(war_error);
    }else {
	    return(war_ok);
    }

}

static int 
wcf_handle_intfadd(void)
{
    const bn_response *resp = NULL;
    bn_binding *binding = NULL;
    bn_request *req = NULL;
    bn_binding_array *barr = NULL;
    const tstring *namespace_value = NULL;
    tstring *msg = NULL;
    tstring *namespace_fixed = NULL;
    char *node_name = NULL;
    uint32 ret_err = 0;
    int err = 0;
    const tstring *port_value = NULL;
    char *bn_interface = NULL;
    char *bn_rate_control = NULL;
    const tstring *rate_control = NULL;
    const char *port = NULL;
    const char *param = NULL;
    tstring *tparam = NULL;
    char *port_num = NULL;
    char *p = NULL;
    uint32 i = 0;
    uint32 num_params = 0;
    uint16 j = 0;
    int32 ret_offset = 0;
    int32 start_offset = 0;
    int32 string_length = 0;
    const tstring *interface_type = NULL;
    
    err = web_dump_params(g_web_data);
    bail_error(err);

    err = mdc_foreach_binding(web_mcc,
		    "/net/interface/config/*",NULL,
		    web_nvsd_http_add_interface,NULL,&num_params);
    bail_error(err);

bail:
    if (err) {
	    return(war_error);
    }else {
	    return(war_ok);
    }
    

}

static web_action_result
wcf_delivery_form_handler(web_context *ctx, void *param)
{
    int err = 0;

    if (web_request_param_exists_str(g_web_data, wcf_btn_add, ps_post_data)) {
        err = wcf_handle_add();
        bail_error(err);
    }else if (web_request_param_exists_str(g_web_data, wcf_btn_intfadd, ps_post_data)) {
        err = wcf_handle_intfadd();
        bail_error(err);
    }else {

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
web_config_form_delivery_init(const lc_dso_info *info, void *data)
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
                                  wcf_delivery_form_handler);
    bail_error(err);

 bail:
    return(err);
}
