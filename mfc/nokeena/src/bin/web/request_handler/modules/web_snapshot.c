/*
 *
 * Filename:  web_snapshot.c
 *
 * Date:      2010/12/04 
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008-10 Juniper Networks, Inc.  
 * All Rights Reserved.
 *
 */

/*! Copied the logic from web_logging.c file */

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
 * CONSTANTS
 */

static const char wcf_snapshot_file[] = "file-snapshot";

static const char *snapshot_download_prefix= "/coredump/snapshots";
/*-----------------------------------------------------------------------------
 * PROTOTYPES
 */

static web_action_result wr_snapshot_action_handler(web_context *ctx,
						void *param);
static int 
web_get_snapshot_name(void);

int web_snapshot_init(const lc_dso_info *info, void *data);

static int
wcf_handle_file_snapshot_upload(void);
static int
wcf_handle_file_snapshot_delete(void);
static const char wcf_btn_upload[] = "upload";
static const char wcf_btn_delete[] = "delete";
/*-----------------------------------------------------------------------------
 * IMPLEMENTATION
 */

/* ------------------------------------------------------------------------- */
static int
wcf_handle_file_snapshot_upload(void)
{	
    tstring *msg = NULL;
    uint32 code = 0;
    int err = 0;
    tstring *file_snapshot_path = NULL;
    const tstring *file = NULL;
    bn_binding_array *bindings = NULL;
    tstring *dump_path = NULL, *dump_name = NULL, *dump_dir = NULL;
    tbool simple = false, archived = false;


    err = web_get_request_param_str(g_web_data, "f_file", ps_post_data,
                                    &file);
    bail_error_null(err, file);

    /*
     * First make sure the filename doesn't do anything crazy like
     * trying to reference another directory.
     */
    err = lf_path_component_is_simple(ts_str(file), &simple);
    bail_error(err);

    /* XXX/EMT: would be better to return friendly error page */
    bail_require_msg(simple, I_("Filename \"%s\" required for download "
                                "was invalid"), ts_str(file));

    /* Now generate the absolute file path */
    err = lf_path_from_dir_file(snapshot_download_prefix, ts_str(file),
                                &file_snapshot_path);

    err = web_get_msg_result(g_web_data, &code, NULL);
    bail_error(err);

    if (code) {
        goto bail;
    }

    if (file_snapshot_path) {
        err = lf_path_last(ts_str(file_snapshot_path), &dump_name);
        bail_error_null(err, dump_name);

        err = lf_path_parent(ts_str(file_snapshot_path), &dump_dir);
        bail_error(err);

        err = web_send_raw_file_with_fileinfo
            (ts_str(file_snapshot_path), "application/octet-stream",
                                    ts_str(dump_name), 0);
        if (err ) {
            err = ts_new_sprintf(&msg, _("Could not download snapshot %s" ),
                                     ts_str(dump_name));
            code = 1;
            bail_error(err);
        }
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


/* ------------------------------------------------------------------------- */

int
web_snapshot_init(const lc_dso_info *info, void *data)
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

    /*! Register the download actions handler by this module */
    err = web_register_action_str(g_rh_data, wcf_snapshot_file, NULL,
                                 wr_snapshot_action_handler);
    bail_error(err);

 bail:
    return(err);
}

static int
wcf_handle_file_snapshot_delete(void)
{
    tstring *msg = NULL;
    uint32 code = 0;
    int err = 0;
    tstring *file_snapshot_path = NULL;
    const tstring *file = NULL;
    bn_binding_array *bindings = NULL;
    tstring *dump_path = NULL, *dump_name = NULL, *dump_dir = NULL;
    tbool simple = false, archived = false;


    err = web_get_request_param_str(g_web_data, "f_file", ps_post_data,
                                    &file);
    bail_error_null(err, file);

    /*
     * First make sure the filename doesn't do anything crazy like
     * trying to reference another directory.
     */
    err = lf_path_component_is_simple(ts_str(file), &simple);
    bail_error(err);

    /* XXX/EMT: would be better to return friendly error page */
    bail_require_msg(simple, I_("Filename \"%s\" required for download "
                                "was invalid"), ts_str(file));

    /* Now generate the absolute file path */
    err = lf_path_from_dir_file(snapshot_download_prefix, ts_str(file),
                                &file_snapshot_path);

    err = web_get_msg_result(g_web_data, &code, NULL);
    bail_error(err);

    if (code) {
        goto bail;
    }

    if (file_snapshot_path) {
        err = lf_path_last(ts_str(file_snapshot_path), &dump_name);
        bail_error_null(err, dump_name);

        err = lf_path_parent(ts_str(file_snapshot_path), &dump_dir);
        bail_error(err);
	lc_log_basic(LOG_NOTICE, _("Deleting snapshot File:%s\n"), ts_str(dump_name));
	err = mdc_send_action_with_bindings_str_va
	    (web_mcc, NULL, NULL, "/file/delete", 2, "local_dir", bt_string, ts_str(dump_dir),
	     "local_filename", bt_string, ts_str(dump_name));
        if (err ) {
            err = ts_new_sprintf(&msg, _("Could not delete snapshot %s" ),
                                     ts_str(dump_name));
            code = 1;
            bail_error(err);
        }
    }

    err = web_set_msg_result(g_web_data, code, msg);
    bail_error(err);

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
/*  End of web_snapshot.c */
static web_action_result
wr_snapshot_action_handler(web_context *ctx, void *param)
{
    int err = 0;

    if (web_request_param_exists_str(g_web_data, wcf_btn_delete, ps_post_data)){
	    err = wcf_handle_file_snapshot_delete();
	    bail_error(err);
    }else if (web_request_param_exists_str(g_web_data, wcf_btn_upload, ps_post_data)){
	    err = wcf_handle_file_snapshot_upload();
	    bail_error(err);
    }else{
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
