/*
 * Filename:  web_nkn_tcl.c
 */

/*! Copied the logic from web_nkn_tcl.c file */

#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include "common.h"
#include "web.h"
#include "web_internal.h"
#include "tpaths.h"
#include "file_utils.h"
#include "dso.h"
#include "proc_utils.h"
#include "web_config_form.h"
#include "md_client.h"

/*-----------------------------------------------------------------------------
 * INFO
 *
 * This module contains the following TCL commands:
 *
 *     tms::get-nvsd-uptime
 *
 *         returns the NVSD uptime in string format
 *
 */

/*-----------------------------------------------------------------------------
 * CONSTANTS
 */

static const char web_get_nvsd_uptime[] = "tms::get-nvsd-uptime";

/*-----------------------------------------------------------------------------
 * PROTOTYPES
 */

static int web_get_nvsd_uptime_cmd_handler(ClientData clientData,
                                  Tcl_Interp *interpreter,
                                  int objc,
                                  Tcl_Obj *CONST objv[]);

int web_nkn_tcl_init(const lc_dso_info *info, void *data);

/*-----------------------------------------------------------------------------
 * IMPLEMENTATION
 */

/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
int
web_get_nvsd_uptime_cmd_handler(ClientData client_data,
                   Tcl_Interp *interpreter,
                   int objc, Tcl_Obj *CONST objv[])
{
    bn_binding *uptime_binding = NULL;
    char *str_value = NULL;
    lt_dur_ms uptm = 0;
    int err = 0;

    err = mdc_get_binding(web_mcc,
                          NULL, NULL, false, &uptime_binding,
                          "/pm/monitor/process/nvsd/uptime", NULL);
    bail_error(err);

    if (uptime_binding) {
        err = bn_binding_get_duration_ms(uptime_binding, ba_value, 0, &uptm);
        bail_error(err);
    }
    else {
        lc_log_basic(LOG_WARNING, "Could not query system uptime");
    }

    err = lt_dur_ms_to_counter_str(uptm, &str_value);
    bail_error(err);

    Tcl_SetResult(interpreter, str_value, web_tcl_free);
    str_value = NULL;

 bail:
    bn_binding_free(&uptime_binding);
    safe_free(str_value);
    if (err) {
        return TCL_ERROR;
    } else {
        return TCL_OK;
    }
}


/* ------------------------------------------------------------------------- */

int
web_nkn_tcl_init(const lc_dso_info *info, void *data)
{
    Tcl_Command cmd = NULL;
    web_context *ctx = NULL;
    int err = 0;
    errno = 0;

    bail_null(data);
    /*
     * the context contains useful information including a pointer
     * to the TCL interpreter so that modules can register their own
     * TCL commands.
     */
    ctx = (web_context *)data;

    /* register TCL command for viewing logs */
    cmd = Tcl_CreateObjCommand(
        ctx->wctx_interpreter, (char *)web_get_nvsd_uptime,
        web_get_nvsd_uptime_cmd_handler, NULL, NULL);
    bail_null(cmd);


 bail:
    return(err);
}
