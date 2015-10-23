/*
 *
 * Filename:  web_config_form_techsupport.c
 * Date:      $Date: 2009/02/26
 * Author:   Chitra Devi R
 *
 * (C) Copyright Nokeena Networks , Inc
 *
 *
 */


#include "common.h"
#include "web.h"
#include "web_internal.h"
#include "dso.h"
#include "web_config_form.h"
#include "md_client.h"
#include "tpaths.h"
#include "file_utils.h"

/*-----------------------------------------------------------------------------
 * CONSTANTS
 */

static const char wr_techsupport_action[] = "config-form-techsupport";
static const char wcf_field_processname[] = "f_processname";

/*-----------------------------------------------------------------------------
 * PROTOTYPES
 */

static web_action_result wr_techsupport_action_handler(web_context *ctx,
                                                  void *param);

int web_config_form_techsupport_init(const lc_dso_info *info, void *data);

/*-----------------------------------------------------------------------------
 * IMPLEMENTATION
 */

static web_action_result
wr_techsupport_action_handler(web_context *ctx, void *param)
{
    tstring *msg = NULL;
    uint32 code = 0;
    int err = 0;
    bn_binding_array *bindings = NULL;
    tstring *dump_path = NULL, *dump_name = NULL, *dump_dir = NULL;

    err = web_get_msg_result(g_web_data, &code, NULL);
    bail_error(err);
    
    if (code) {
        goto bail;
    }

    err = mdc_send_action_with_bindings_and_results(web_mcc,
            &code, NULL, "/debug/generate/dump", NULL, &bindings);
    bail_error(err);

    if (bindings) {
        err = bn_binding_array_get_value_tstr_by_name(bindings,
                "dump_path", NULL, &dump_path);
        bail_error(err);
    }

    if (dump_path) {
        err = lf_path_last(ts_str(dump_path), &dump_name);
        bail_error_null(err, dump_name);
    
        err = lf_path_parent(ts_str(dump_path), &dump_dir);
        bail_error(err);

        err = web_send_raw_file_with_fileinfo
        (ts_str(dump_path), "application/octet-stream", ts_str(dump_name), 0);
        bail_error(err);
    }

    err = web_set_msg_result(g_web_data, code, msg);
    bail_error(err);

    if (!code) {
        err = web_process_form_buttons(g_web_data);
        bail_error(err);
    }

 bail:
    bn_binding_array_free(&bindings);
    ts_free(&dump_path);
    ts_free(&dump_name);
    ts_free(&dump_dir);
    ts_free(&msg);
    if (err) {
        return(war_error);
    } else {
        return(war_ok);
    }
}

int
web_config_form_techsupport_init(const lc_dso_info *info, void *data)
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
    err = web_register_action_str(g_rh_data, wr_techsupport_action, NULL,
                                  wr_techsupport_action_handler);
    bail_error(err);

 bail:
    return(err);
}
