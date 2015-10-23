/*
 *
 * Filename:  web_restart.c
 * Date:      $Date: 2009/02/24
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

/*-----------------------------------------------------------------------------
 * CONSTANTS
 */

static const char wr_restart_action[] = "restart";
static const char wcf_field_processname[] = "f_processname";

/*-----------------------------------------------------------------------------
 * PROTOTYPES
 */

static web_action_result wr_restart_action_handler(web_context *ctx,
                                                  void *param);

int web_restart_init(const lc_dso_info *info, void *data);

/*-----------------------------------------------------------------------------
 * IMPLEMENTATION
 */

static web_action_result
wr_restart_action_handler(web_context *ctx, void *param)
{
    tstring *msg = NULL;
    const tstring *processname = NULL;
    uint32 code = 0;
    int err = 0;

    err = web_get_request_param_str(g_web_data, wcf_field_processname, 
                       ps_post_data, &processname);
    bail_error_null(err,processname);

    err = mdc_send_action_with_bindings_str_va
        (web_mcc,
        &code, &msg, "/pm/actions/restart_process",1,
        "process_name",bt_string,ts_str(processname));
    bail_error(err);

    err = web_set_msg_result(g_web_data, code, msg);
    bail_error(err);

    if (!code) {
        err = web_process_form_buttons(g_web_data);
        bail_error(err);
    }

 bail:
    ts_free(&msg);
    if (err) {
        return(war_error);
    } else {
        return(war_ok);
    }
}

int
web_restart_init(const lc_dso_info *info, void *data)
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
    err = web_register_action_str(g_rh_data, wr_restart_action, NULL,
                                  wr_restart_action_handler);
    bail_error(err);

 bail:
    return(err);
}
