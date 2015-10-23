/*
 * (C) Copyright 2015 Juniper Networks
 * All rights reserved.
 */

static const char rcsid[] = "$Id: web_logging.c,v 1.27 2008/05/11 17:28:31 gregs Exp $";

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
 *     tms::get-log-line-count [logfile]
 *     tms::get-filtered-log-line-count <filter> [logfile]
 *
 *         returns the number of lines in the syslog
 *
 *     tms::get-log-lines <start-line> <num-of-lines> [logfile]
 *     tms::get-filtered-log-lines <filter> <start-line>
 *                                 <num-of-lines> [logfile]
 *
 *         returns a list of num-of-lines lines starting from start-line.
 *
 *     tms::continuous-log
 *
 *         returns continuous output from the current log
 */

/*-----------------------------------------------------------------------------
 * CONSTANTS
 */

static const char wlog_sys_get_log_lines_cmd_name[] = "tms::get-log-lines";
static const char wlog_sys_get_filtered_log_lines_cmd_name[] =
    "tms::get-filtered-log-lines";
static const char wlog_sys_get_log_line_count_cmd_name[] =
    "tms::get-log-line-count";
static const char wlog_sys_get_filtered_log_line_count_cmd_name[] =
    "tms::get-filtered-log-line-count";
static const char wlog_sys_continuous_log_cmd_name[] = "tms::continuous-log";
static const uint32 wlog_log_buf_size = 16384;
static const char wlog_sys_force_rotate_action[] = "rotate";
static const char wcf_sys_download_action[] = "download-syslog";
static const char wlog_sys_download_prefix[] = "/var/log";

/*-----------------------------------------------------------------------------
 * PROTOTYPES
 */

static int wlog_sys_open_filtered_log(const char *filter, int *ret_fd);
static int wlog_sys_open_archived_log(const char *file_num, int *ret_fd);
static int wlog_sys_open_filtered_archived_log(const char *filter,
                                           const char *file_num, int *ret_fd);

static int wlog_sys_get_log_lines(Tcl_Interp *interpreter,
                              int fd,
                              uint32 start,
                              uint32 end,
                              Tcl_Obj **ret_list);

static int wlog_sys_get_log_lines_cmd(ClientData clientData,
                                  Tcl_Interp *interpreter,
                                  int objc,
                                  Tcl_Obj *CONST objv[]);

static int wlog_sys_continuous_log_cmd(ClientData clientData,
                                   Tcl_Interp *interpreter,
                                   int objc,
                                   Tcl_Obj *CONST objv[]);

static int wlog_sys_get_log_line_count(int fd, uint32 *ret_lines);

static int wlog_sys_get_log_line_count_cmd(ClientData clientData,
                                       Tcl_Interp *interpreter,
                                       int objc,
                                       Tcl_Obj *CONST objv[]);

static web_action_result wlog_sys_force_rotate_action_handler(web_context *ctx,
                                                          void *param);

static web_action_result wlog_sys_download_action_handler(web_context *ctx,
                                                      void *param);

int web_sys_logging_init(const lc_dso_info *info, void *data);

/*-----------------------------------------------------------------------------
 * IMPLEMENTATION
 */

static int
wlog_sys_open_filtered_log(const char *filter, int *ret_fd)
{
    lc_launch_params *launch_params = NULL;
    lc_launch_result result;
    int err = 0;

    bail_null(filter);
    bail_null(ret_fd);

    err = lc_launch_params_get_defaults(&launch_params);
    bail_error(err);

    err = ts_new_str(&(launch_params->lp_path), prog_grep_path);
    bail_error(err);

    err = tstr_array_new_va_str(&(launch_params->lp_argv), NULL, 4,
                                prog_grep_path, "-i", filter, file_syslog_path);
    bail_error(err);

    launch_params->lp_env_opts = eo_preserve_all;
    launch_params->lp_io_origins[lc_io_stdout].lio_opts = lioo_pipe_parent;
    
    err = lc_launch(launch_params, &result);
    bail_error(err);

    *ret_fd = result.lr_child_stdout_fd;

 bail:
    lc_launch_params_free(&launch_params);
    return(err);
}

/* ------------------------------------------------------------------------- */

static int
wlog_sys_open_archived_log(const char *file_num, int *ret_fd)
{
    lc_launch_params *launch_params = NULL;
    lc_launch_result result;
    char *file_name = NULL;
    int err = 0;
    tbool simple = false;

    bail_null(file_num);
    bail_null(ret_fd);

    err = lf_path_component_is_simple(file_num, &simple);
    bail_error(err);

    /* XXX/EMT: would be better to return friendly error page */
    bail_require_msg(simple, I_("Log file number \"%s\" was invalid"), 
                     file_num);

    file_name = smprintf("%s.%s.gz", file_syslog_path, file_num);
    bail_null(file_name);

    err = lc_launch_params_get_defaults(&launch_params);
    bail_error(err);

    err = ts_new_str(&(launch_params->lp_path), prog_zcat_path);
    bail_error(err);

    err = tstr_array_new_va_str(&(launch_params->lp_argv), NULL, 2,
                                prog_zcat_path, file_name);
    bail_error(err);

    launch_params->lp_env_opts = eo_preserve_all;
    launch_params->lp_io_origins[lc_io_stdout].lio_opts = lioo_pipe_parent;
    
    err = lc_launch(launch_params, &result);
    bail_error(err);

    *ret_fd = result.lr_child_stdout_fd;

 bail:
    lc_launch_params_free(&launch_params);
    safe_free(file_name);
    return(err);
}

/* ------------------------------------------------------------------------- */

static int
wlog_sys_open_filtered_archived_log(const char *filter, const char *file_num,
                                int *ret_fd)
{
    lc_launch_params *launch_params = NULL;
    lc_launch_result result;
    char *file_name = NULL;
    int err = 0;
    tbool simple = false;

    bail_null(file_num);
    bail_null(filter);
    bail_null(ret_fd);

    err = lf_path_component_is_simple(file_num, &simple);
    bail_error(err);

    /* XXX/EMT: would be better to return friendly error page */
    bail_require_msg(simple, I_("Log file number \"%s\" was invalid"), 
                     file_num);

    file_name = smprintf("%s.%s.gz", file_syslog_path, file_num);
    bail_null(file_name);

    err = lc_launch_params_get_defaults(&launch_params);
    bail_error(err);

    err = ts_new_str(&(launch_params->lp_path), prog_zgrep_path);
    bail_error(err);

    err = tstr_array_new_va_str(&(launch_params->lp_argv), NULL, 4,
                                prog_zgrep_path, "-i", filter, file_name);
    bail_error(err);

    launch_params->lp_env_opts = eo_preserve_all;
    launch_params->lp_io_origins[lc_io_stdout].lio_opts = lioo_pipe_parent;
    
    err = lc_launch(launch_params, &result);
    bail_error(err);

    *ret_fd = result.lr_child_stdout_fd;

 bail:
    lc_launch_params_free(&launch_params);
    safe_free(file_name);
    return(err);
}

/* ------------------------------------------------------------------------- */

static int
wlog_sys_get_log_lines(Tcl_Interp *interpreter, int fd,
                   uint32 start, uint32 end, Tcl_Obj **ret_list)
{
    Tcl_Obj *list = NULL;
    Tcl_Obj *line_obj = NULL;
    tstring *line_buf = NULL;
    char buf[wlog_log_buf_size];
    uint32 cur_line = 0;
    uint32 seg_start = 0;
    uint32 i = 0;
    int count = 0;
    int err = 0;

    bail_null(ret_list);

    list = Tcl_NewListObj(0, NULL);
    bail_null(list);

    do {
        if (cur_line + 1 >= end) {
            break;
        }

        errno = 0;
        count = read(fd, buf, wlog_log_buf_size);
        if (count == -1 && errno == EINTR) { continue; }
        bail_require_errno(count >= 0, I_("Reading log file '%s'"),
                           file_syslog_path);

        /* look for a newline inside the buffer */
        seg_start = 0;
        for (i = 0; i < (uint32)count; ++i) {
            if (buf[i] == '\n') {
                if (cur_line + 1 >= start && cur_line + 1 < end) {
                    if (!line_buf) {
                        err = ts_new(&line_buf);
                        bail_error(err);
                    }

                    err = ts_append_str_frag(line_buf, buf, seg_start,
                                             i - seg_start);
                    bail_error(err);

                    line_obj = Tcl_NewStringObj(ts_str(line_buf),
                                                ts_length(line_buf));
                    bail_null(line_obj);

                    err = Tcl_ListObjAppendElement(interpreter, list,
                                                   line_obj);
                    bail_require(err == TCL_OK);
                    err = 0;
                    
                    ts_free(&line_buf);
                }

                seg_start = i + 1;
                ++cur_line;
            }
        }

        if (seg_start < (uint32)count) {
            if (cur_line + 1 >= start && cur_line + 1 < end) {
                if (!line_buf) {
                    err = ts_new(&line_buf);
                    bail_error(err);
                }

                err = ts_append_str_frag(line_buf, buf, seg_start,
                                         (uint32)count - seg_start);
                bail_error(err);
            }
        }
    } while (count > 0);

    *ret_list = list;
    list = NULL;

 bail:
    if (list) {
        Tcl_DecrRefCount(list);
    }
    ts_free(&line_buf);
    return(err);
}

/* ------------------------------------------------------------------------- */

static int
wlog_sys_get_log_lines_cmd(ClientData clientData,
                       Tcl_Interp *interpreter,
                       int objc,
                       Tcl_Obj *CONST objv[])
{
    Tcl_Obj *list = NULL;
    const char *file_num = NULL;
    const char *line_str = NULL;
    const char *num_lines_str = NULL;
    uint32 start_line = 0;
    uint32 end_line = 0;
    uint32 num_lines = 0;
    int fd = -1;
    int err = 0;

    bail_require(objc == 3 || objc == 4);

    line_str = Tcl_GetString(objv[1]);
    bail_null(line_str);
    start_line = (uint32)atoi(line_str);

    num_lines_str = Tcl_GetString(objv[2]);
    bail_null(num_lines_str);
    num_lines = (uint32)atoi(num_lines_str);
    end_line = start_line + num_lines;

    if (objc == 3) {
        errno = 0;
        fd = open(file_syslog_path, O_RDONLY);
        bail_require_errno(fd >= 0, I_("Opening log file '%s'"),
                           file_syslog_path);
    } else {
        file_num = Tcl_GetString(objv[3]);
        bail_null(file_num);

        err = wlog_sys_open_archived_log(file_num, &fd);
        bail_error(err);
    }

    err = wlog_sys_get_log_lines(interpreter, fd, start_line, end_line, &list);
    bail_error(err);

    Tcl_SetObjResult(interpreter, list);

 bail:
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
    if (err) {
        return TCL_ERROR;
    } else {
        return TCL_OK;
    }
}

/* ------------------------------------------------------------------------- */

static int
wlog_sys_get_filtered_log_lines_cmd(ClientData clientData,
                                Tcl_Interp *interpreter,
                                int objc,
                                Tcl_Obj *CONST objv[])
{
    Tcl_Obj *list = NULL;
    const char *filter = NULL;
    const char *file_num = NULL;
    const char *line_str = NULL;
    const char *num_lines_str = NULL;
    uint32 start_line = 0;
    uint32 end_line = 0;
    uint32 num_lines = 0;
    int fd = -1;
    int err = 0;

    bail_require(objc == 4 || objc == 5);

    filter = Tcl_GetString(objv[1]);
    bail_null(filter);

    line_str = Tcl_GetString(objv[2]);
    bail_null(line_str);
    start_line = (uint32)atoi(line_str);

    num_lines_str = Tcl_GetString(objv[3]);
    bail_null(num_lines_str);
    num_lines = (uint32)atoi(num_lines_str);
    end_line = start_line + num_lines;

    if (objc == 4) {
        err = wlog_sys_open_filtered_log(filter, &fd);
        bail_error(err);
    } else {
        file_num = Tcl_GetString(objv[4]);
        bail_null(file_num);

        err = wlog_sys_open_filtered_archived_log(filter, file_num, &fd);
        bail_error(err);
    }

    err = wlog_sys_get_log_lines(interpreter, fd, start_line, end_line, &list);
    bail_error(err);

    Tcl_SetObjResult(interpreter, list);

 bail:
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
    if (err) {
        return TCL_ERROR;
    } else {
        return TCL_OK;
    }
}

/* ------------------------------------------------------------------------- */

static int
wlog_sys_get_log_line_count(int fd, uint32 *ret_lines)
{
    char buf[wlog_log_buf_size];
    uint32 lines = 0;
    int count = 0;
    int i = 0;
    int err = 0;

    bail_null(ret_lines);
    *ret_lines = 0;

    do {
        errno = 0;
        count = read(fd, buf, wlog_log_buf_size);
        if (count == -1 && errno == EINTR) { continue; }
        bail_require_errno(count >= 0, I_("Reading log file"));

        for (i = 0; i < count; ++i) {
            if (buf[i] == '\n') {
                ++lines;
            }
        }
    } while (count > 0);

    *ret_lines = lines;

 bail:
    return(err);
}

/* ------------------------------------------------------------------------- */

static int
wlog_sys_get_log_line_count_cmd(ClientData clientData,
                            Tcl_Interp *interpreter,
                            int objc,
                            Tcl_Obj *CONST objv[])
{
    const char *file_num = NULL;
    char *lines_str = NULL;
    uint32 lines = 0;
    int fd = -1;
    int err = 0;

    bail_require(objc == 1 || objc == 2);

    if (objc == 1) {
        errno = 0;
        fd = open(file_syslog_path, O_RDONLY);
    } else {
        file_num = Tcl_GetString(objv[1]);
        bail_null(file_num);

        err = wlog_sys_open_archived_log(file_num, &fd);
        bail_error(err);
    }

    if (fd >= 0) {
        err = wlog_sys_get_log_line_count(fd, &lines);
        bail_error(err);
    }

    lines_str = smprintf("%u", lines);
    bail_null(lines_str);

    Tcl_SetResult(interpreter, lines_str, web_tcl_free);
    lines_str = NULL;

 bail:
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
    safe_free(lines_str);
    if (err) {
        return TCL_ERROR;
    } else {
        return TCL_OK;
    }
}

/* ------------------------------------------------------------------------- */

static int
wlog_sys_get_filtered_log_line_count_cmd(ClientData clientData,
                                     Tcl_Interp *interpreter,
                                     int objc,
                                     Tcl_Obj *CONST objv[])
{
    const char *filter = NULL;
    const char *file_num = NULL;
    char *lines_str = NULL;
    uint32 lines = 0;
    int fd = -1;
    int err = 0;

    bail_require(objc == 2 || objc == 3);

    filter = Tcl_GetString(objv[1]);
    bail_null(filter);

    if (objc == 2) {
        err = wlog_sys_open_filtered_log(filter, &fd);
        bail_error(err);
    } else {
        file_num = Tcl_GetString(objv[2]);
        bail_null(file_num);

        err = wlog_sys_open_filtered_archived_log(filter, file_num, &fd);
        bail_error(err);
    }

    if (fd >= 0) {
        err = wlog_sys_get_log_line_count(fd, &lines);
        bail_error(err);
    }

    lines_str = smprintf("%u", lines);
    bail_null(lines_str);

    Tcl_SetResult(interpreter, lines_str, web_tcl_free);
    lines_str = NULL;

 bail:
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
    safe_free(lines_str);
    if (err) {
        return TCL_ERROR;
    } else {
        return TCL_OK;
    }
}

/* ------------------------------------------------------------------------- */

static int
wlog_sys_continuous_log_cmd(ClientData clientData,
                        Tcl_Interp *interpreter,
                        int objc,
                        Tcl_Obj *CONST objv[])
{
    char buf[wlog_log_buf_size];
    tbool waiting = true;
    int count = 0;
    int offset = 0;
    int fd = -1;
    int err = 0;

    bail_require(objc == 1);

    errno = 0;
    fd = open(file_syslog_path, O_RDONLY);
    bail_require_errno(fd >= 0, I_("Opening log file '%s'"), file_syslog_path);

    errno = 0;
    offset = lseek(fd, 0, SEEK_END);
    bail_require_errno(offset >= 0,
                       I_("Seeking inside log file '%s'"), file_syslog_path);

    do {
        errno = 0;
        count = read(fd, buf, wlog_log_buf_size - 1);
        if (count == -1 && errno == EINTR) { continue; }
        bail_require_errno(count >= 0, I_("Reading log file"));

        if (count > 0) {
            buf[count] = 0;

            if (waiting) {
                web_print_str(g_web_data, "\n");
                waiting = false;
            }

            err = web_print_str(g_web_data, buf);
            if (err) {
                /* pipe died so we can break out of the loop */
                err = 0;
                break;
            }
        } else {
            sleep(1);
            err = web_print_str(g_web_data, ".");
            if (err) {
                /* pipe died so we can break out of the loop */
                err = 0;
                break;
            }
            waiting = true;
        }
    } while (count >= 0); /* keep going until error */

 bail:
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
    if (err) {
        return TCL_ERROR;
    } else {
        return TCL_OK;
    }
}

/* ------------------------------------------------------------------------- */

static web_action_result
wlog_sys_force_rotate_action_handler(web_context *ctx, void *param)
{
    tstring *msg = NULL;
    uint32 code = 0;
    int err = 0;

    err = mdc_send_action(web_mcc, 
                          &code, &msg, "/logging/actions/force_rotation");
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

/* ------------------------------------------------------------------------- */

static web_action_result
wlog_sys_download_action_handler(web_context *ctx, void *param)
{
    int err = 0;
    tstring *file_path = NULL;
    const tstring *file = NULL;
    tstr_array *args = NULL;
    tbool simple = false, archived = false;
    //web_dump_params(g_web_data);	
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
    err = lf_path_from_dir_file(wlog_sys_download_prefix, ts_str(file),
                                &file_path);
    
    /* If it's an archived/compressed file, we need to read it diff'tly */
    archived = ts_has_suffix_str(file_path, ".gz", false);

    /* send the file */
    if (archived) {
        err = tstr_array_new_va_str(&args, NULL, 2,
                                    prog_zcat_path, ts_str(file_path));
        bail_error(err);

        //err = web_send_raw_process_output_str(prog_zcat_path, args,
        //                                      "application/octet-stream");
        err = web_send_raw_file_with_fileinfo(ts_str(file_path),
                                    "application/octet-stream", 
                                    ts_str(file), 0);
        bail_error(err);
    } else {
        //err = web_send_raw_file_str(ts_str(file_path),
        //                            "application/octet-stream");
        err = web_send_raw_file_with_fileinfo(ts_str(file_path),
                                    "application/octet-stream", 
                                    ts_str(file), 0);
        bail_error(err);
    }

 bail:
    /*
     * don't free args as ownership for it is taken over by the
     * web_send_raw_output function.
     */
    ts_free(&file_path);
    if (err) {
        return(war_error);
    } else {
        return(war_override_output);
    }
}

/* ------------------------------------------------------------------------- */

int
web_sys_logging_init(const lc_dso_info *info, void *data)
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

    /* register TCL command for viewing logs */
    cmd = Tcl_CreateObjCommand(
        ctx->wctx_interpreter, (char *)wlog_sys_get_log_lines_cmd_name,
        wlog_sys_get_log_lines_cmd, NULL, NULL);
    bail_null(cmd);

    cmd = Tcl_CreateObjCommand(
        ctx->wctx_interpreter, (char *)wlog_sys_get_filtered_log_lines_cmd_name,
        wlog_sys_get_filtered_log_lines_cmd, NULL, NULL);
    bail_null(cmd);

    cmd = Tcl_CreateObjCommand(
        ctx->wctx_interpreter, (char *)wlog_sys_get_log_line_count_cmd_name,
        wlog_sys_get_log_line_count_cmd, NULL, NULL);
    bail_null(cmd);

    cmd = Tcl_CreateObjCommand(
        ctx->wctx_interpreter,
        (char *)wlog_sys_get_filtered_log_line_count_cmd_name,
        wlog_sys_get_filtered_log_line_count_cmd, NULL, NULL);
    bail_null(cmd);

    cmd = Tcl_CreateObjCommand(
        ctx->wctx_interpreter, (char *)wlog_sys_continuous_log_cmd_name,
        wlog_sys_continuous_log_cmd, NULL, NULL);
    bail_null(cmd);

    err = web_register_action_str(g_rh_data, wlog_sys_force_rotate_action, NULL,
                                  wlog_sys_force_rotate_action_handler);
    bail_error(err);

    err = web_register_action_str(g_rh_data, wcf_sys_download_action, NULL,
                                  wlog_sys_download_action_handler);
    bail_error(err);

 bail:
    return(err);
}
