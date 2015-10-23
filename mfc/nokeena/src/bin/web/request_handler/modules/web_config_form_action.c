/*
 *
 * Filename:  web_config_form_action.c
 * Date:      2009/02/26
 * Author:    Chitra Devi R
 *
 * Nokeena Networks, Inc.  
 * All rights reserved.
 *
 */

#include <string.h>
#include "common.h"
#include "dso.h"
#include "web.h"
#include "web_internal.h"
#include "web_config_form.h"

/*-----------------------------------------------------------------------------
 * CONSTANTS
 */

static const char wma_action_name[] = "config-form-action";
static const char wma_field_action_binding[] = "mfd_action_binding";
static const char wma_field_append_binding[] = "mfd_append_list";

/*-----------------------------------------------------------------------------
 * PROTOTYPES
 */

static web_action_result wma_action_handler(web_context *ctx, void *param);

int web_config_form_action_init(const lc_dso_info *info, void *data);

/*-----------------------------------------------------------------------------
 * IMPLEMENTATION
 */

static web_action_result
wma_action_handler(web_context *ctx, void *param)
{
    const bn_response *resp = NULL;
    bn_request *req = NULL;
    tstring *action = NULL;
    tstring *msg = NULL;
    uint32 code = 0;
    int err = 0;
    tstr_array *fields = NULL;
    const tstring *field = NULL;
    bn_type type = bt_none;
    tstring *field_value = NULL;
    tstring *fields_str = NULL;
    uint32 num_fields = 0;
    uint32 i = 0;

    err = web_get_trimmed_form_field_str(g_web_data, wma_field_action_binding,
                                    &action);
    bail_error(err);

    if (!action || ts_length(action) <= 0) {
        goto bail;
    }
    err = bn_action_request_msg_create(&req, ts_str(action));
    bail_error(err);

    err = web_get_trimmed_form_field_str(g_web_data, wma_field_append_binding,
                                         &fields_str);
    bail_error(err);

    if (fields_str != NULL ) {
        err = ts_tokenize(fields_str, ',', 0, 0,
                      ttf_ignore_leading_separator |
                      ttf_ignore_trailing_separator |
                      ttf_single_empty_token_null, &fields);
        bail_error(err);
        /* go through all the fields */
        if (fields != NULL) {
            err = tstr_array_length(fields, &num_fields);
            bail_error(err);
        }
        for (i = 0; i < num_fields; ++i) {
            field = tstr_array_get_quick(fields, i);
            bail_null(field);
            err = web_get_trimmed_form_field_str(g_web_data, ts_str(field),
                                        &field_value);
            bail_error(err);

            err = web_get_type_for_form_field(g_web_data, field, &type);
            bail_error(err);
            
            err = bn_action_request_msg_append_new_binding(
                        req, 0, ts_str(field), 
                        type, ts_str(field_value), NULL);
            bail_error(err);
         
            ts_free(&field_value);
 
 
        }
        ts_free(&fields_str);
        tstr_array_free(&fields);
    }
   
    err = web_send_mgmt_msg(g_web_data, req, &resp);
    bail_error_null(err, resp);

    err = bn_response_get(resp, &code, &msg, false, NULL, NULL);
    bail_error(err);

    err = web_set_msg_result(g_web_data, code, msg);
    bail_error(err);

    if (!code) {
        err = web_process_form_buttons(g_web_data);
        bail_error(err);
    }

 bail:
    bn_request_msg_free(&req);
    ts_free(&msg);
    if (err) {
        return(war_error);
    } else {
        return(war_ok);
    }
}

int
web_config_form_action_init(const lc_dso_info *info, void *data)
{
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
    err = web_register_action_str(g_rh_data, wma_action_name, NULL,
                                  wma_action_handler);
    bail_error(err);

 bail:
    return(err);
}
