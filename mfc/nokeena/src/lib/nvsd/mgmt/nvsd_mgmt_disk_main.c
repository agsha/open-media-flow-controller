/*
 *
 * Filename:  nvsd_mgmt_disk_main.c
 * Date:      2011/01/23 
 * Author:    Varsha Rajagopalan
 *
 * (C) Copyright 2011 Juniper Networks, Inc.  
 * All rights reserved.
 * 
 *
 * This file is copied and modified from nvsd_mgmt_main.c
 * The file has function calls related to diskcache action handling.
 * It has been introduced as a fix for BUG 7322.
 *
 */

/* Standard Headers */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#include <glib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/prctl.h>

/* Samara Headers */
#include "md_client.h"
#include "mdc_wrapper.h"
#include "libevent_wrapper.h"
#include "random.h"
#include "dso.h"
#include "mdc_backlog.h"

/* nvsd Headers */
#include "nkn_defs.h"
#include "nvsd_mgmt.h"
#include "nkn_stat.h"
#include "nkn_common_config.h"
#include "nkn_am_api.h"
#include "nkn_cfg_params.h"
#include "nkn_regex.h"
#include "cache_mgr.h"

/* Samara Variable */
lew_context *nvsd_disk_lwc = NULL;
md_client_context *nvsd_disk_mcc = NULL;
tbool nvsd_disk_exiting = false;
mdc_bl_context *nvsd_disk_blc = NULL;

static lew_event *nvsd_disk_retry_event = NULL;

/* Local Variables */
static GThread *mgmt_thread;

int
nvsd_disk_retry_handler(int fd, short event_type, void *data
	, lew_context * ctxt);
int nvsd_disk_mgmt_thread_init(void);
int nvsd_disk_mdc_init(void);
int nvsd_disk_mgmt_initiate_exit(void);
static int nvsd_disk_mgmt_handle_session_close(int fd,
	fdic_callback_type cbt,
	void *vsess, void *gsec_arg);

static int nvsd_disk_mgmt_handle_event_request(gcl_session * sess,
	bn_request ** inout_request,
	void *arg);

static int nvsd_disk_mgmt_handle_query_request(gcl_session * sess,
	bn_request **
	inout_query_request, void *arg);
static int nvsd_disk_mgmt_handle_action_request(gcl_session * sess,
	bn_request **
	inout_action_request,
	void *arg);

static int
nvsd_disk_mgmt_recv_action_request(gcl_session * sess,
	bn_request ** inout_request, void *arg);

static int
nvsd_disk_mgmt_recv_event_request(gcl_session * sess,
	bn_request ** inout_request, void *arg);

static int
nvsd_disk_mgmt_recv_query_request(gcl_session * sess,
	bn_request ** inout_request, void *arg);

/* Extern Funtions */
extern int nvsd_diskcache_module_cfg_handle_change(bn_binding_array *
	old_bindings,
	bn_binding_array *
	new_bindings);
extern char *get_uids_in_disk(void);

int nvsd_disk_mgmt_thrd_initd = 0;

int
nvsd_disk_mgmt_req_start(md_client_context * mcc,
	bn_request * request,
	tbool request_complete,
	bn_response * response, int32 idx, void *arg);

int
nvsd_disk_mgmt_req_complete(md_client_context * mcc,
	bn_request * request,
	tbool request_complete,
	bn_response * response, int32 idx, void *arg);

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_disk_mgmt_thread_hdl ()
 *	purpose : thread handler for the management module
 */
static void
nvsd_disk_mgmt_thread_hdl(void)
{
    /*
     * Set the thread name 
     */
    prctl(PR_SET_NAME, "nvsd-disk-mgmt", 0, 0, 0);
    int err = 0;

    /*
     * Call the main loop of samara 
     */
    lc_log_debug(LOG_DEBUG,
	    _("nvsd_disk_mgmt_thread_hdl () - before lew_dispatch"));

    err = nvsd_disk_mgmt_init();
    bail_error(err);

    nvsd_disk_mgmt_thrd_initd = 1;

    err = lew_dispatch(nvsd_disk_lwc, NULL, NULL);
    bail_error(err);

    /*
     * de-init to be called 
     */
bail:
    if (!nvsd_disk_mgmt_thrd_initd) {
	lc_log_debug(LOG_NOTICE, ("error: initing nvsd disk mgmt thread"));
	exit(1);
    }
    return;
}	/* end of nvsd_disk_mgmt_thread_hdl() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_disk_mgmt_thread_init ()
 *	purpose : thread handler for the management module
 */
int
nvsd_disk_mgmt_thread_init(void)
{
    GError *t_err = NULL;

    /*
     * As a test create a thread here 
     */
    mgmt_thread = g_thread_create_full((GThreadFunc) nvsd_disk_mgmt_thread_hdl,
	    NULL, (128 * 1024), TRUE,
	    FALSE, G_THREAD_PRIORITY_NORMAL, &t_err);
    if (NULL == mgmt_thread) {
	lc_log_basic(LOG_ERR, "Mgmt Thread Creation Failed !!!!");
	g_error_free(t_err);
	return 1;
    }

    return 0;
}	/* end of nvsd_disk_mgmt_thread_init() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_disk_mgmt_init ()
 *	purpose : initializes the mgmt interface to mgmtd
 *		this logic is taken from their template
 */
int
nvsd_disk_mgmt_init(void)
{
    int err = 0;

    bail_error(err);
    err = lew_init(&nvsd_disk_lwc);
    bail_error(err);

    /*
     * Connect/Create GCL sessions, intialize mgmtd interface 
     */
    err = nvsd_disk_mdc_init();
    bail_error(err);

    /*
     * Update Configuration related to Diskcache 
     */
    err = nvsd_diskcache_cfg_query();
    bail_error(err);

bail:
    return (err);

}	/* end of nvsd_mgmt_init() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_disk_mgmt_deinit ()
 *	purpose : do the cleanup activity for exiting
 */
int
nvsd_disk_mgmt_deinit(void)
{
    int err = 0;

    err = mdc_wrapper_deinit(&nvsd_disk_mcc);
    bail_error(err);

    err = lew_deinit(&nvsd_disk_lwc);
    bail_error(err);

bail:
    lc_log_basic(LOG_NOTICE, _("nvsd daemon exiting with code %d"), err);
    lc_log_close();
    return (err);

}	/* end of nvsd_disk_mgmt_deinit() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_disk_mdc_init ()
 *	purpose : initializes the mgmt interface to mgmtd
 *		this logic is taken from their template
 */
int
nvsd_disk_mdc_init(void)
{
    int err = 0;
    mdc_wrapper_params *mwp = NULL;
    fdic_handle *fdich = NULL;

    err = mdc_wrapper_params_get_defaults(&mwp);
    bail_error(err);

    mwp->mwp_lew_context = nvsd_disk_lwc;
    mwp->mwp_gcl_client = gcl_client_disk;

    mwp->mwp_request_start_func = nvsd_disk_mgmt_req_start;
    mwp->mwp_request_complete_func = nvsd_disk_mgmt_req_complete;

    mwp->mwp_session_close_func = nvsd_disk_mgmt_handle_session_close;
    mwp->mwp_event_request_func = nvsd_disk_mgmt_recv_event_request;
    mwp->mwp_action_request_func = nvsd_disk_mgmt_recv_action_request;
    mwp->mwp_query_request_func = nvsd_disk_mgmt_recv_query_request;

    err = mdc_wrapper_init(mwp, &nvsd_disk_mcc);
    bail_error(err);

    err = lc_random_seed_nonblocking_once();
    bail_error(err);

    err = mdc_wrapper_get_fdic_handle(nvsd_disk_mcc, &fdich);
    bail_error_null(err, fdich);

    err = mdc_bl_init(fdich, &nvsd_disk_blc);
    bail_error(err);

    /*
     * Register to receive config changes.  We care about:
     *   1. Any of our own configuration nodes.
     */
#define LICENSE
#ifdef LICENSE
    // 2: Change to the MFD license (When active status changes)
#endif /*LICENSE*/
    err = mdc_send_action_with_bindings_str_va
	(nvsd_disk_mcc, NULL, NULL, "/mgmtd/events/actions/interest/add",
#ifndef LICENSE
	 2,
#else
	 3,
#endif
	 "event_name", bt_name, mdc_event_dbchange, "match/1/pattern",
	 bt_string, "/nkn/nvsd/diskcache/**"
#ifdef LICENSE
	 , "match/2/pattern", bt_string, "/nkn/mfd/licensing/**"
#endif	  /*LICENSE*/
	);
    bail_error(err);

bail:
    mdc_wrapper_params_free(&mwp);
    if (err) {
	lc_log_debug(LOG_ERR, _("Unable to connect to the management daemon"));
    }

    return (err);
}	/* end of nvsd_disk_mdc_init () */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_disk_mgmt_handle_session_close ()
 *	purpose : picked up as is from samara's demo
 */
int
nvsd_disk_mgmt_handle_session_close(int fd,
	fdic_callback_type cbt,
	void *vsess, void *gsec_arg)
{
    int err = 0;

    UNUSED_ARGUMENT(fd);
    UNUSED_ARGUMENT(cbt);
    UNUSED_ARGUMENT(vsess);
    UNUSED_ARGUMENT(gsec_arg);
    /*
     * If we're exiting, this is expected and we shouldn't do anything.
     * Otherwise, it's an error, so we should complain and reestablish
     * the connection.
     */
    if (nvsd_disk_exiting) {
	goto bail;
    }

    lc_log_basic(LOG_ERR, _("Lost connection to mgmtd; reconnecting"));

    if (!nvsd_disk_mgmt_thrd_initd)
	goto bail;

    /*
     * Reconnect to mgmtd 
     */
    /*
     * Handle Re-connect by triggering an timer event 
     */
    err = lew_event_reg_timer(nvsd_disk_lwc, &nvsd_disk_retry_event,
	    nvsd_disk_retry_handler, NULL, 5000);
    bail_error(err);

bail:
    return (err);

}	/* end of nvsd_disk_mgmt_handle_session_close() */

int
nvsd_disk_retry_handler(int fd,
	short event_type, void *data, lew_context * ctxt)
{
    int err = 0;

    UNUSED_ARGUMENT(fd);
    UNUSED_ARGUMENT(event_type);
    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(ctxt);

    err = nvsd_disk_mdc_init();
    bail_error(err);

bail:
    return err;

}

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_disk_mgmt_handle_action_request ()
 *	purpose : picked up as is from samara's demo
 */
static int
nvsd_disk_mgmt_handle_action_request(gcl_session * sess,
	bn_request ** inout_action_request,
	void *arg)
{
    int err = 0, dm2_err = 0;
    bn_msg_type req_type = bbmt_none;
    bn_binding_array *bindings = NULL;
    tstring *action_name = NULL, *t_ret_msg = NULL;
    tstring *t_name = NULL;
    char ret_msg[MAX_FORMAT_CMD_LEN];
    bn_response *resp = NULL;
    char *buff = NULL;
    tstr_array *tok_array = NULL;
    bn_binding *binding = NULL;

    UNUSED_ARGUMENT(arg);

    bail_null(inout_action_request);
    bail_null(*inout_action_request);

    err = bn_request_get
	(*inout_action_request, &req_type, NULL, true, &bindings, NULL, NULL);
    bail_error(err);
    bail_require(req_type == bbmt_action_request);

    err = bn_action_request_msg_get_action_name(*inout_action_request,
	    &action_name);
    bail_error(err);

    lc_log_basic(LOG_INFO, "Received action %s", ts_str(action_name));

    /*
     * Check to see if server initialization is completed 
     */
    if (!nkn_system_inited) {
	lc_log_debug(LOG_WARNING,
		"Server initialization in progress, cannot process  action %s",
		ts_str(action_name));
	err = bn_response_msg_create_send(sess, bbmt_action_response,
		bn_request_get_msg_id
		(*inout_action_request), 1,
		"error : server initialization in progress, please try after sometime",
		0);
	goto bail;
    }

    /*
     * Start processing based on the action 
     */

    /*
     * ACTIVATE action handler 
     */
    if (ts_equal_str
	    (action_name, "/nkn/nvsd/diskcache/actions/activate", false)) {
	tbool t_activate;
	int ret_err = 0;
	char *node_name = NULL;
	const char *t_diskid = NULL;

	/*
	 * Get the name field 
	 */
	err = bn_binding_array_get_value_tstr_by_name
	    (bindings, "name", NULL, &t_name);
	bail_error(err);

	/*
	 * Get the action field 
	 */
	err = bn_binding_array_get_value_tbool_by_name
	    (bindings, "activate", NULL, &t_activate);
	bail_error(err);

	/*
	 * Call the handler 
	 */
	ret_err = nvsd_mgmt_diskcache_action_hdlr(ts_str(t_name)
		, NVSD_DC_ACTIVATE, t_activate, ret_msg);

	/*
	 * Send the response 
	 */
	if (!ret_msg[0])
	    strcpy(ret_msg, "no return message");

	err = bn_response_msg_create_send
	    (sess, bbmt_action_response,
	     bn_request_get_msg_id(*inout_action_request), ret_err ? 1 : 0,
	     _(ret_msg), 0);
	bail_error(err);

	if (!ret_err) {
	    /*
	     * Now get the disk_id for the disk name 
	     */
	    t_diskid = get_diskid_from_diskname(ts_str(t_name));
	    bail_null(t_diskid);

	    /*
	     * Update the node 
	     */
	    node_name =
		smprintf("/nkn/nvsd/diskcache/config/disk_id/%s/activated",
			t_diskid);
	    err = nvsd_disk_mgmt_update_node_bool(node_name, t_activate);
	    safe_free(node_name);
	    bail_error(err);
	}
    }

    /*
     * CACHEABLE action handler 
     */
    else if (ts_equal_str
	    (action_name, "/nkn/nvsd/diskcache/actions/cacheable", false)) {
	tbool t_cacheable;
	int ret_err = 0;
	char *node_name = NULL;
	const char *t_diskid = NULL;

	/*
	 * Get the name field 
	 */
	err = bn_binding_array_get_value_tstr_by_name
	    (bindings, "name", NULL, &t_name);
	bail_error(err);

	/*
	 * Get the action field 
	 */
	err = bn_binding_array_get_value_tbool_by_name
	    (bindings, "cacheable", NULL, &t_cacheable);
	bail_error(err);

	/*
	 * Call the handler 
	 */
	ret_err = nvsd_mgmt_diskcache_action_hdlr(ts_str(t_name),
		NVSD_DC_CACHEABLE, t_cacheable, ret_msg);

	/*
	 * Send the response 
	 */
	if (!ret_msg[0])
	    strcpy(ret_msg, "no return message");

	err = bn_response_msg_create_send
	    (sess, bbmt_action_response,
	     bn_request_get_msg_id(*inout_action_request), ret_err ? 1 : 0,
	     _(ret_msg), 0);
	bail_error(err);
	if (!ret_err) {
	    /*
	     * Now get the disk_id for the disk name 
	     */
	    t_diskid = get_diskid_from_diskname(ts_str(t_name));
	    bail_null(t_diskid);

	    /*
	     * Update the node 
	     */
	    node_name = smprintf("/nkn/nvsd/diskcache/config/"
		    "disk_id/%s/cache_enabled", t_diskid);
	    err = nvsd_disk_mgmt_update_node_bool(node_name, t_cacheable);
	    safe_free(node_name);
	    bail_error(err);
	}
    }

    /*
     * PRE-FORMAT action handler 
     */
    else if (ts_equal_str
	    (action_name, "/nkn/nvsd/diskcache/actions/pre_format", false)) {
	const char *t_diskid = NULL;
	tstring *t_block = NULL;
	const char *c_block = NULL;

	/*
	 * Get the name field 
	 */
	err = bn_binding_array_get_value_tstr_by_name
	    (bindings, "diskname", NULL, &t_name);
	bail_error(err);

	err = bn_binding_array_get_value_tstr_by_name
	    (bindings, "blocks", NULL, &t_block);
	bail_error(err);

	if (NULL == t_block)
	    goto bail;

	c_block = ts_str(t_block);

	t_diskid = get_diskid_from_diskname(ts_str(t_name));

	if (NULL == t_diskid) {
	    err = bn_response_msg_create_send
		(sess, bbmt_action_response,
		 bn_request_get_msg_id(*inout_action_request), 1,
		 _("unknown disk"), 0);
	    bail_error(err);
	} else {
	    /*
	     * Call the handler to get the format command string 
	     */
	    if (0 == strcmp("small-blocks", c_block)) {
		err = nvsd_mgmt_diskcache_action_hdlr(ts_str(t_name),
			    NVSD_DC_FORMAT_BLOCKS, true, ret_msg);
	    } else {
		err =
		    nvsd_mgmt_diskcache_action_hdlr(ts_str(t_name),
			    NVSD_DC_FORMAT, true, ret_msg);
	    }
	    dm2_err = err;
	    /*
	     * Send the response with the error string if error else just a
	     * generic message that formatting will start
	     */
	    err = ts_new_str(&t_ret_msg,
		    dm2_err ? ret_msg : "Disk Formatting ->");
	    bail_error(err);

	    err = bn_action_response_msg_create(&resp,
		    bn_request_get_msg_id
		    (*inout_action_request),
		    dm2_err, t_ret_msg);
	    bail_error(err);

	    err = bn_binding_new_str(&binding, "cmd_str",
		    ba_value, bt_string, 0, ret_msg);
	    bail_error(err);

	    err = bn_action_response_msg_append_binding_takeover(resp,
		    &binding, 0);
	    bail_error(err);

	    err = bn_response_msg_send(sess, resp);
	    bail_error(err);
	}
    }

    /*
     * POST-FORMAT action handler 
     */
    else if (ts_equal_str
	    (action_name, "/nkn/nvsd/diskcache/actions/post_format", false)) {

	/*
	 * Get the name field 
	 */
	err = bn_binding_array_get_value_tstr_by_name
	    (bindings, "diskname", NULL, &t_name);
	bail_error(err);

	/*
	 * Call the diskcache handler to indicate format completed fine 
	 */
	err = nvsd_mgmt_diskcache_action_hdlr(ts_str(t_name),
		    NVSD_DC_FORMAT_RESULT, true, ret_msg);
	bail_error(err);

	err = bn_response_msg_create_send
	    (sess, bbmt_action_response,
	     bn_request_get_msg_id(*inout_action_request),
	     err, "\tDisk Format Complete", 0);
	bail_error(err);
    }

    /*
     * DISK REPAIR action handler 
     */
    else if (ts_equal_str
	    (action_name, "/nkn/nvsd/diskcache/actions/repair", false)) {
	lc_log_debug(LOG_NOTICE, I_("Received action for disk repair"));
	err = bn_response_msg_create_send
	    (sess, bbmt_action_response,
	     bn_request_get_msg_id(*inout_action_request), 1,
	     _("Disk repair not yet implemented"), 0);
	bail_error(err);
    }

    /*
     * NEW DISK DETECTION action handler 
     */
    else if (ts_equal_str
	    (action_name, "/nkn/nvsd/diskcache/actions/newdisk", false)) {
	err = bn_response_msg_create_send(sess, bbmt_action_response,
		bn_request_get_msg_id (*inout_action_request), 0,
		"Message sent to detect for new disks\n"
		"Please check disk state for status", 0);

	/*
	 * Call the handler 
	 */
	nvsd_mgmt_diskcache_action_hdlr(NULL, NVSD_DC_PROBE_NEWDISK, false,
		ret_msg);
    }

    /*
     * SHOW INTERNAL WATERMARKS action handler 
     */
    else if (lc_is_prefix
	    ("/nkn/nvsd/diskcache/actions/show-internal-watermarks",
	     ts_str(action_name), false)) {
	char *ret_resp;

	ret_resp = nvsd_mgmt_show_watermark_action_hdlr();

	/*
	 * Send the response 
	 */
	err = bn_response_msg_create_send
	    (sess, bbmt_action_response,
	     bn_request_get_msg_id(*inout_action_request), 0, ret_resp, 0);

	safe_free(ret_resp);
    }

    /*
     * Handle DISKCACHE/ACTIONS/PRE-READ-STOP 
     */
    else if (ts_equal_str
	    (action_name, "/nkn/nvsd/diskcache/actions/pre-read-stop",
	     false)) {
	err = nvsd_mgmt_diskcache_action_pre_read_stop();
	bail_error(err);

	err = bn_response_msg_create_send(sess,
		bbmt_action_response,
		bn_request_get_msg_id
		(*inout_action_request), err,
		"\tRequest to abort dictionary pre-load sent",
		0);
	bail_error(err);
    }
    /*
     * UNKNOWN action 
     */
    else {
	lc_log_debug(LOG_WARNING, I_("Got unexpected action %s"),
		ts_str(action_name));
	err = bn_response_msg_create_send
	    (sess, bbmt_action_response,
	     bn_request_get_msg_id(*inout_action_request), 1,
	     _("error : action not yet supported"), 0);
	goto bail;
    }

bail:
    safe_free(buff);
    ts_free(&action_name);
    ts_free(&t_name);
    tstr_array_free(&tok_array);
    bn_binding_array_free(&bindings);
    bn_response_msg_free(&resp);
    bn_binding_free(&binding);
    ts_free(&t_ret_msg);
    return (err);
}								/* end of nvsd_mgmt_handle_action_request() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_disk_mgmt_handle_event_request ()
 *	purpose : picked up as is from samara's demo
 */
int
nvsd_disk_mgmt_handle_event_request(gcl_session * sess,
	bn_request ** inout_request, void *arg)
{
    int err = 0;
    bn_msg_type msg_type = bbmt_none;
    bn_binding_array *old_bindings = NULL, *new_bindings = NULL;
    tstring *event_name = NULL;
    bn_binding_array *bindings = NULL;

    UNUSED_ARGUMENT(sess);
    UNUSED_ARGUMENT(arg);

    bail_null(inout_request);
    bail_null(*inout_request);

    err = bn_request_get(*inout_request, &msg_type, NULL, false, 
	    &bindings, NULL, NULL);
    bail_error(err);

    bail_require(msg_type == bbmt_event_request);

    err = bn_event_request_msg_get_event_name(*inout_request, &event_name);
    bail_error_null(err, event_name);

    lc_log_basic(LOG_DEBUG, "Received event: %s", ts_str(event_name));

    if (ts_equal_str(event_name, mdc_event_dbchange, false)) {
	err = mdc_event_config_change_notif_extract(bindings,
		&old_bindings, &new_bindings, NULL);
	bail_error(err);

	err = nvsd_diskcache_module_cfg_handle_change(old_bindings,
		new_bindings);
	bail_error(err);
    } else {
	lc_log_debug(LOG_WARNING, I_("Received unexpected event %s"),
		ts_str(event_name));
    }

bail:
    bn_binding_array_free(&bindings);
    bn_binding_array_free(&old_bindings);
    bn_binding_array_free(&new_bindings);
    ts_free(&event_name);
    return err;
}								/* end of
								 * nvsd_disk_mgmt_handle_event_request() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_disk_mgmt_handle_query_request ()
 *	purpose : picked up as is from samara's demo
 */
int
nvsd_disk_mgmt_handle_query_request(gcl_session * sess,
	bn_request ** inout_query_request,
	void *arg)
{
    int err = 0;

    UNUSED_ARGUMENT(arg);

    bail_null(inout_query_request);
    bail_null(*inout_query_request);

    err = mdc_dispatch_mon_request(sess, *inout_query_request,
	    nvsd_disk_mon_handle_get,
	    NULL, nvsd_mon_handle_iterate, NULL);
    bail_error(err);

bail:

    return (err);
}	/* end of * nvsd_disk_mgmt_handle_query_request() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_disk_mgmt_initiate_exit ()
 *	purpose : picked up as is from samara's demo
 */
int
nvsd_disk_mgmt_initiate_exit()
{
    int err = 0;

    /*
     * Some occurrences are only OK if they happen while we're
     * exiting, such as getting NULL back for a mgmt request.
     */
    nvsd_disk_exiting = true;

    /*
     * Need to do this here to remove our mgmt-related events.
     */
    err = mdc_wrapper_disconnect(nvsd_disk_mcc, false);
    bail_error(err);

    /*
     * Calling lew_delete_all_events() is not guaranteed
     * to solve this for us, since the GCL/libevent shim does not use
     * the libevent wrapper.  lew_escape_from_dispatch() will
     * definitely do it, though.
     */
    err = lew_escape_from_dispatch(nvsd_disk_lwc, true);
    bail_error(err);

bail:
    return (err);
}	/* end of nvsd_disk_mgmt_initiate_exit() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_disk_mgmt_update_node_uint32()
 *	purpose : picked up as is from samara's demo
 */
int
nvsd_disk_mgmt_update_node_uint32(const char *cpNode, uint32 new_value)
{
    int err = 0;
    uint32 code = 0;
    char *value_str;

    value_str = smprintf("%d", new_value);
    err = mdc_modify_binding
	(nvsd_disk_mcc, &code, NULL, bt_uint32, value_str, cpNode);

    /*
     * If we sent the message successfully but an error code was
     * returned from mgmtd, treat it as an error here.
     */
    err = code;
    goto bail;

bail:
    safe_free(value_str);
    return (err);
}	/* end of nvsd_disk_mgmt_update_node_uint32() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_disk_mgmt_update_node_bool()
 *	purpose : picked up as is from samara's demo
 */
int
nvsd_disk_mgmt_update_node_bool(const char *cpNode, tbool new_value)
{
    int err = 0;
    uint32 code = 0;
    char *value_str = NULL;

    value_str = smprintf("%s", new_value ? "true" : "false");
    bail_null(value_str);

    err = mdc_modify_binding
	(nvsd_disk_mcc, &code, NULL, bt_bool, value_str, cpNode);
    bail_error(err);

    /*
     * If we sent the message successfully but an error code was
     * returned from mgmtd, treat it as an error here.
     */
    err = code;

bail:
    safe_free(value_str);
    return (err);
}   /* end of nvsd_disk_mgmt_update_node_bool() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_disk_mgmt_update_node_str()
 *	purpose : picked up as is from samara's demo
 */
int
nvsd_disk_mgmt_update_node_str(const char *cpNode,
	const char *new_value, bn_type binding_type)
{
    int err = 0;
    uint32 code = 0;

    err = mdc_modify_binding
	(nvsd_disk_mcc, &code, NULL, binding_type, new_value, cpNode);

    /*
     * If we sent the message successfully but an error code was
     * returned from mgmtd, treat it as an error here.
     */
    err = code;
    goto bail;
bail:
    return (err);

}								/* end of nvsd_disk_mgmt_update_node_str() */

int
nvsd_disk_mgmt_req_complete(md_client_context * mcc,
	bn_request * request,
	tbool request_complete,
	bn_response * response, int32 idx, void *arg)
{
    int err = 0;

    UNUSED_ARGUMENT(mcc);
    UNUSED_ARGUMENT(request);
    UNUSED_ARGUMENT(request_complete);
    UNUSED_ARGUMENT(response);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(arg);

    err = mdc_bl_set_ready(nvsd_disk_blc, true);
    bail_error(err);

bail:
    return err;
}

int
nvsd_disk_mgmt_req_start(md_client_context * mcc,
	bn_request * request,
	tbool request_complete,
	bn_response * response, int32 idx, void *arg)
{
    int err = 0;

    UNUSED_ARGUMENT(mcc);
    UNUSED_ARGUMENT(request);
    UNUSED_ARGUMENT(request_complete);
    UNUSED_ARGUMENT(response);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(arg);

    err = mdc_bl_set_ready(nvsd_disk_blc, false);
    bail_error(err);

bail:
    return err;
}

static int
nvsd_disk_mgmt_recv_event_request(gcl_session * sess,
	bn_request ** inout_request, void *arg)
{
    int err = 0;
    tbool handled = false;

    UNUSED_ARGUMENT(arg);

    err = mdc_bl_backlog_or_handle_message_takeover
	(nvsd_disk_blc, sess, inout_request,
	 nvsd_disk_mgmt_handle_event_request, NULL, &handled);
    bail_error(err);

bail:
    return err;
}

static int
nvsd_disk_mgmt_recv_action_request(gcl_session * sess,
	bn_request ** inout_request, void *arg)
{
    int err = 0;
    tbool handled = false;

    UNUSED_ARGUMENT(arg);

    err = mdc_bl_backlog_or_handle_message_takeover
	(nvsd_disk_blc, sess, inout_request,
	 nvsd_disk_mgmt_handle_action_request, NULL, &handled);
    bail_error(err);

bail:
    return err;
}

static int
nvsd_disk_mgmt_recv_query_request(gcl_session * sess,
	bn_request ** inout_request, void *arg)
{
    int err = 0;
    tbool handled = false;

    UNUSED_ARGUMENT(arg);

    err = mdc_bl_backlog_or_handle_message_takeover
	(nvsd_disk_blc, sess, inout_request,
	 nvsd_disk_mgmt_handle_query_request, NULL, &handled);
    bail_error(err);

bail:
    return err;
}

/* End of nvsd_mgmt_disk_main.c */
