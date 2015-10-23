/*
 *
 * Filename:  web_fuselog.c
 *
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
 * INFO
 *
 * This module contains the following TCL commands:
 *
 *     tms::get-fuselog-line-count
 *
 *         returns the number of lines in the fuselog
 *
 *     tms::get-fuselog-lines <start-line> <num-of-lines> 
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

static const char wlog_get_fuselog_lines_cmd_name[] = "tms::get-fuselog-lines";
static const char wlog_get_fuselog_line_count_cmd_name[] =
    "tms::get-fuselog-line-count";
static const char wlog_continuous_fuselog_cmd_name[] = "tms::continuous-fuselog";
static const char wcf_fuselog_download[] = "fuselog-download";
static const uint32 wlog_log_buf_size = 16384;

static char *file_fuselog_path = NULL;
static const char file_fuselog_index[] = ".accesslog.fileid";
static const char path_prefix_node[] = "/nkn/accesslog/config/path"; //"/nkn/fuselog/config/path";
static const char file_name_node[] = "/nkn/fuselog/config/filename";
/*-----------------------------------------------------------------------------
 * PROTOTYPES
 */

static int wlog_get_fuselog_lines(Tcl_Interp *interpreter,
                              int fd,
                              uint32 start,
                              uint32 end,
                              Tcl_Obj **ret_list);

static int wlog_get_fuselog_lines_cmd(ClientData clientData,
                                  Tcl_Interp *interpreter,
                                  int objc,
                                  Tcl_Obj *CONST objv[]);

static int wlog_continuous_fuselog_cmd(ClientData clientData,
                                   Tcl_Interp *interpreter,
                                   int objc,
                                   Tcl_Obj *CONST objv[]);

static int wlog_get_fuselog_line_count(int fd, uint32 *ret_lines);

static int wlog_get_fuselog_line_count_cmd(ClientData clientData,
                                       Tcl_Interp *interpreter,
                                       int objc,
                                       Tcl_Obj *CONST objv[]);
static web_action_result wr_fuselog_download_handler(web_context *ctx,
                                                void *param);
static int 
web_get_fuselog_name(void);

int web_fuselog_init(const lc_dso_info *info, void *data);

/*-----------------------------------------------------------------------------
 * IMPLEMENTATION
 */

/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */

static int
wlog_get_fuselog_lines(Tcl_Interp *interpreter, int fd,
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
    char *cp;

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
                           file_fuselog_path);

        while (( cp = memchr(buf, '\0' , count )) != NULL )
            *cp = ' ';

        while (( cp = memchr(buf, '<' , count )) != NULL )
            *cp = '[';

        while (( cp = memchr(buf, '>' , count )) != NULL )
            *cp = ']';


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
wlog_get_fuselog_lines_cmd(ClientData clientData,
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
        err = web_get_fuselog_name();
        bail_error(err);
        
        fd = open(file_fuselog_path, O_RDONLY);
        if (fd < 0 ) {
            bail_error(err);
            goto bail;
        }

        //bail_require_errno(fd >= 0, I_("Opening log file '%s'"),
        //                   file_fuselog_path);
    }

    err = wlog_get_fuselog_lines(interpreter, fd, start_line, end_line, &list);
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
wlog_get_fuselog_line_count(int fd, uint32 *ret_lines)
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
wlog_get_fuselog_line_count_cmd(ClientData clientData,
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
        web_get_fuselog_name();
        bail_error(err);
        fd = open(file_fuselog_path, O_RDONLY);
    } 

    if (fd >= 0) {
        err = wlog_get_fuselog_line_count(fd, &lines);
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
wlog_continuous_fuselog_cmd(ClientData clientData,
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
    char *cp;

    bail_require(objc == 1);

    errno = 0;
    web_get_fuselog_name();
    bail_error(err);
    fd = open(file_fuselog_path, O_RDONLY);
    bail_require_errno(fd >= 0, I_("Opening log file '%s'"), file_fuselog_path);

    errno = 0;
    offset = lseek(fd, 0, SEEK_END);
    bail_require_errno(offset >= 0,
                       I_("Seeking inside log file '%s'"), file_fuselog_path);

    do {
        errno = 0;
        count = read(fd, buf, wlog_log_buf_size - 1);
        if (count == -1 && errno == EINTR) { continue; }
        bail_require_errno(count >= 0, I_("Reading log file"));
      
        while (( cp = memchr(buf, '\0' , count )) != NULL )
            *cp = ' ';

        while (( cp = memchr(buf, '<' , count )) != NULL )
            *cp = '[';

        while (( cp = memchr(buf, '>' , count )) != NULL )
            *cp = ']';

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
static int 
web_get_fuselog_name(void)
{
    int err = 0;
    int count = 0;
    char * tok = NULL;
    char *index = NULL;
    tstring *path_prefix = NULL;
    tstring *file_name = NULL;
    int fd = -1;
    char buf[16] = "1";
    FILE *fp = NULL;
    int aid = 0, eid = 0, tid = 0, cid = 0;

    /* Now get the Accesslog Path*/
    err = mdc_get_binding_tstr (web_mcc, NULL, NULL, NULL,
                            &path_prefix,path_prefix_node , NULL);
    bail_error_null(err, path_prefix);

    /* Now get the fuselog filename*/
    err = mdc_get_binding_tstr (web_mcc, NULL, NULL, NULL,
                            &file_name, file_name_node, NULL);
    bail_error_null(err, file_name);

    safe_free(file_fuselog_path);
    file_fuselog_path = smprintf("%s/%s", ts_str(path_prefix), ts_str(file_name));
    bail_null(file_fuselog_path);

 bail :
     return err;
}

static web_action_result
wr_fuselog_download_handler(web_context *ctx, void *param)
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
    err = web_get_fuselog_name();
    bail_error(err);
    
    if (file_fuselog_path) {
        err = lf_path_last(file_fuselog_path, &dump_name);
        bail_error_null(err, dump_name);

        err = lf_path_parent(file_fuselog_path, &dump_dir);
        bail_error(err);

        err = web_send_raw_file_with_fileinfo
            (file_fuselog_path, "application/octet-stream", 
                                    ts_str(dump_name), 0);
        if (err ) {
            err = ts_new_sprintf(&msg, _("Could not download file %s" ),
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
web_fuselog_init(const lc_dso_info *info, void *data)
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
        ctx->wctx_interpreter, (char *)wlog_get_fuselog_lines_cmd_name,
        wlog_get_fuselog_lines_cmd, NULL, NULL);
    bail_null(cmd);

    cmd = Tcl_CreateObjCommand(
        ctx->wctx_interpreter, (char *)wlog_get_fuselog_line_count_cmd_name,
        wlog_get_fuselog_line_count_cmd, NULL, NULL);
    bail_null(cmd);

    cmd = Tcl_CreateObjCommand(
        ctx->wctx_interpreter, (char *)wlog_continuous_fuselog_cmd_name,
        wlog_continuous_fuselog_cmd, NULL, NULL);
    bail_null(cmd);

    /*! Register all actions handled by this module */ 
    err = web_register_action_str(g_rh_data, wcf_fuselog_download, NULL,
                                 wr_fuselog_download_handler);
    bail_error(err);

 bail:
    return(err);
}
