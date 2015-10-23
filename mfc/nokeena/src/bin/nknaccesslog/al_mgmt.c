/*
 * Filename :   al_monitor.c
 * Date:        2008-11-18
 * Author:      Dhruva Narasimhan
 *
 * (C) Copyright 2008, Nokeena Networks, Inc.
 * All rights reserved.
 *
 */


#include "md_client.h"
#include "mdc_wrapper.h"
#include "bnode.h"
#include "signal_utils.h"
#include "libevent_wrapper.h"
#include "random.h"
#include "dso.h"
#include <glib.h>
#include <ctype.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdarg.h>
#include "accesslog_pri.h"




///////////////////////////////////////////////////////////////////////////////
lew_context *al_lew = NULL;
md_client_context *al_mcc = NULL;
tbool al_exiting = false;


///////////////////////////////////////////////////////////////////////////////
extern char *pass;
extern char *remote_url;
extern int al_exit_req;


///////////////////////////////////////////////////////////////////////////////
int al_mgmt_handle_event_request(gcl_session *session, bn_request  **inout_request,
        void        *arg);
int al_mgmt_handle_session_close(int fd, fdic_callback_type cbt, void *vsess,
        void *gsec_arg);
int al_config_handle_set_request( 
        bn_binding_array *old_bindings, bn_binding_array *new_bindings);
int al_module_cfg_handle_change(
        bn_binding_array *old_bindings, bn_binding_array *new_bindings);

int al_mgmt_handle_action_response(gcl_session *sess, bn_response **inout_response, void *arg);

///////////////////////////////////////////////////////////////////////////////
int
al_main_loop(void)
{
	int err = 0;
	/* Call the main loop of samara */
    lc_log_debug(LOG_DEBUG, _("al_main_loop () - before lew_dispatch"));

	err = lew_dispatch (al_lew, NULL, NULL);
    bail_error(err);
bail:
    return err;

} 

/* List of signals server can handle */
static const int al_signals_handled[] = {
            SIGHUP, SIGINT, SIGPIPE, SIGTERM, SIGUSR1
};

#define al_num_signals_handled (int)(sizeof(al_signals_handled) / sizeof(int))

/* Libevent handles for server */
static lew_event *al_event_signals[al_num_signals_handled];

static int al_handle_signal(
        int signum,
        short ev_type,
        void *data,
        lew_context *ctxt)
{
    (void) ev_type;
    (void) data;
    (void) ctxt;

    catcher(signum);
    return 0;
}



///////////////////////////////////////////////////////////////////////////////
int al_init(void)
{
    int err = 0;
    uint32 i = 0;

    err = lc_log_init(accesslog_program_name, NULL, LCO_none,
            LC_COMPONENT_ID(LCI_NKNACCESSLOG),
            LOG_DEBUG, LOG_LOCAL4, LCT_SYSLOG);
    bail_error(err);

    err = lew_init(&al_lew);
    bail_error(err);


    /*
     * Register to hear about all the signals we care about.
     * These are automatically persistent, which is fine.
     */
    for (i = 0; i < (int) al_num_signals_handled; ++i) {
        err = lew_event_reg_signal
            (al_lew, &(al_event_signals[i]), al_signals_handled[i],
            al_handle_signal, NULL);
        bail_error(err);
    }

    err = al_mgmt_init();
    bail_error(err);

    err = al_cfg_query();
    bail_error(err);

    err = al_serv_init();
    bail_error(err);

bail:
    return err;
}



///////////////////////////////////////////////////////////////////////////////
int
al_mgmt_init(void)
{
        int err = 0;
        mdc_wrapper_params *mwp = NULL;

        err = mdc_wrapper_params_get_defaults(&mwp);
        bail_error(err);

        mwp->mwp_lew_context = al_lew;
        mwp->mwp_gcl_client = gcl_client_nknaccesslog;

        mwp->mwp_session_close_func = al_mgmt_handle_session_close;
        mwp->mwp_event_request_func= al_mgmt_handle_event_request;
       
        err = mdc_wrapper_init(mwp, &al_mcc);
        bail_error(err);

        err = lc_random_seed_nonblocking_once();
        bail_error(err);

        /*
         * Register to receive config changes.  We care about:
         *   1. Any of our own configuration nodes.
         *   2. The system locale, so we can update gettext.
         */
        err = mdc_send_action_with_bindings_str_va(
                        al_mcc,
                        NULL,
                        NULL,
                        "/mgmtd/events/actions/interest/add",
                        2,
                        "event_name", bt_name, mdc_event_dbchange,
                        "match/1/pattern", bt_string, "/nkn/accesslog/config/**");
        bail_error(err);

        err = mdc_send_action_with_bindings_str_va(
                        al_mcc,
                        NULL,
                        NULL,
                        "/mgmtd/events/actions/interest/add",
                        1,
                        "event_name", bt_name, "/mgmtd/notify/dbchange");
        bail_error(err);

        /// Register late so that above calls that are synchronous
        /// can configure the accesslogger.
        //err = bn_set_session_callback_action_response_msg(
                //mdc_wrapper_get_gcl_session(al_mcc),
                //al_mgmt_handle_action_response,
                //NULL);
        //bail_error(err);

bail:
        mdc_wrapper_params_free(&mwp);
        if (err) {
                lc_log_debug(LOG_ERR, 
                                _("Unable to connect to the management daemon"));
        }
        return err;
}



///////////////////////////////////////////////////////////////////////////////
int al_mgmt_handle_session_close(
        int fd,
        fdic_callback_type cbt,
        void *vsess,
        void *gsec_arg)
{
        int err = 0;
        gcl_session *gsec_session = vsess;

        if ( al_exiting) {
                goto bail;
        }

        lc_log_basic(LOG_ERR, _("Lost connection to mgmtd; exiting"));
        err = al_mgmt_initiate_exit();
        bail_error(err);

bail:
        return err;
}

///////////////////////////////////////////////////////////////////////////////
int al_mgmt_handle_event_request(
        gcl_session *session,
        bn_request  **inout_request,
        void        *arg)
{
        int err = 0;
        bn_msg_type msg_type = bbmt_none;
        bn_binding_array *old_bindings = NULL, *new_bindings = NULL;
        tstring *event_name = NULL;
        bn_binding_array *bindings = NULL;
        uint32 num_callbacks = 0, i = 0;
        void *data = NULL;

        bail_null(inout_request);
        bail_null(*inout_request);

        err = bn_request_get(*inout_request, &msg_type, 
                        NULL, false, &bindings, NULL, NULL);
        bail_error(err);

        bail_require(msg_type == bbmt_event_request);

        err = bn_event_request_msg_get_event_name(*inout_request, &event_name);
        bail_error_null(err, event_name);

        if (ts_equal_str(event_name, mdc_event_dbchange, false)) {
                err = mdc_event_config_change_notif_extract(
                                bindings, 
                                &old_bindings,
                                &new_bindings,
                                NULL);
                bail_error(err);

                err = al_config_handle_set_request(old_bindings, new_bindings);
                bail_error(err);
        } else if (ts_equal_str(event_name, "/mgmtd/notify/dbchange", false)) {
                lc_log_debug(LOG_DEBUG, I_("Received unexpected event %s"), 
                                ts_str(event_name));
                
        }
        else {
                lc_log_debug(LOG_DEBUG, I_("Received unexpected event %s"), 
                                ts_str(event_name));
        }

bail:
        bn_binding_array_free(&bindings);
        bn_binding_array_free(&old_bindings);
        bn_binding_array_free(&new_bindings);
        ts_free(&event_name);

        return err;
}


///////////////////////////////////////////////////////////////////////////////
int al_config_handle_set_request(
        bn_binding_array *old_bindings,
        bn_binding_array *new_bindings)
{
        int err = 0;

        err = al_module_cfg_handle_change(old_bindings, new_bindings);
        bail_error(err);

bail:
        return err;
}

///////////////////////////////////////////////////////////////////////////////
int al_log_basic(const char *fmt, ...)
{
    int n = 0;
    char outstr[1024];
    va_list ap;
    va_start(ap, fmt);


    if (glob_run_under_pm) {
        n = vsprintf(outstr, fmt, ap);
        lc_log_basic(LOG_NOTICE, "%s", outstr);
    } 
    else {
        n = vprintf(fmt, ap);
    }
    va_end(ap);

    return n;
}

int al_log_debug(const char *fmt, ...)
{
    int n = 0;
    char outstr[1024];
    va_list ap;
    va_start(ap, fmt);


    if (glob_run_under_pm) {
        n = vsprintf(outstr, fmt, ap);
        lc_log_basic(LOG_INFO, "%s", outstr);
    } 
    else {
        n = vprintf(fmt, ap);
    }
    va_end(ap);

    return n;
}






///////////////////////////////////////////////////////////////////////////////
int 
al_log_upload(
    const char *this, 
    const char *that)
{
    int num_req = 0;
    int err = 0;
    uint32 idx = 0;
    //mdc_wrapper_context *mwc = al_mcc->mcc_wrapper_context;
    char *rurl = NULL;
    uint32 ret_code = 0;
    bn_request *action_msg = NULL;
    gcl_session *sess = NULL;

    if (al_mcc == NULL) {
        // Then we are running w/o mgmtd support
        // perhaps standalone mode.
        goto bail;
    }

    if (pass == NULL) {
        goto bail;
    }

    // FIX - 1060
    // If we are shutting down either due to 
    // SIGXXX or mgmtd request, no point in uploading
    // coz the mgmt context may be lost before the upload 
    // finishes. If this happens, then we will dump core.
    if (al_exit_req || al_exiting) {
        goto bail;
    }

    bail_null(this);
    bail_null(that);

    al_log_basic("Uploading file (%s)", this);

    rurl = smprintf("%s/%s", remote_url, that);
    bail_null(rurl);


    // Alocate the action message
    err = bn_action_request_msg_create(&action_msg, "/file/transfer/upload");
    bail_error(err);

    // Set the local directory
    err = bn_action_request_msg_append_new_binding(
            action_msg,
            0,  // node_flags - UNUSED
            "local_dir", bt_string, "/var/log/nkn", NULL);
    bail_error(err);

    err = bn_action_request_msg_append_new_binding(
            action_msg,
            0,
            "local_filename", bt_string, this, NULL);
    bail_error(err);

    err = bn_action_request_msg_append_new_binding(
            action_msg,
            0,
            "remote_url", bt_string, rurl, NULL);
    bail_error(err);

    err = bn_action_request_msg_append_new_binding(
            action_msg,
            0,
            "password", bt_string, pass, NULL);
    bail_error(err);

    sess = mdc_wrapper_get_gcl_session(al_mcc);
    bail_null(sess);

    // Send the message out to mgmtd
    err = bn_request_msg_send(sess, action_msg);
    bail_error(err);

    // Queue in the message if so that we can service the response
    //err = uint32_arry_append(mwc->mwc_req_ids,
            //bn_request_get_msg_id(action_msg));
    //bail_error(err);
//
    //err = array_set(mwc->mwc_responses, idx, NULL);
    //bail_error(err);


    /*
    if (mdc_wrapper_get_num_outstanding_requests(al_mcc, &num_req) == 0) {
        al_log_basic("Outstanding upload requests = %d", num_req);
    }

    err = mdc_send_action_with_bindings_str_va(
            al_mcc, &ret_code, NULL, "/file/transfer/upload", 4,
            "local_dir", bt_string, "/var/log/nkn",
            "local_filename", bt_string, this,
            "remote_url", bt_string, rurl,
            "password", bt_string, pass);
    bail_error(err);

    if (ret_code) {
        // If SCP failed!! Just disable anc leave it to the user to 
        // figure out what to do
        al_log_basic("Disabling AUTO copy of log files. ");
        al_log_basic("WARNING: Log files may be lost/overwritte. Check whether "
                "destination is upload-able to or not!");
        free(pass);
        pass = NULL;
        free(remote_url);
        remote_url = NULL;
    }
    */

bail:
    bn_request_msg_free(&action_msg);
    return err;
}

///////////////////////////////////////////////////////////////////////////////
int
al_mgmt_initiate_exit(void)
{
    int err = 0;
    int i;
    
    al_exiting = true;

    for(i = 0; i < al_num_signals_handled; ++i) {
        err = lew_event_delete(al_lew, &(al_event_signals[i]));
        bail_error(err);
    }

    err = mdc_wrapper_disconnect(al_mcc, false);
    bail_error(err);

    err = lew_escape_from_dispatch(al_lew, true);
    bail_error(err);

bail:
    return err;
}


///////////////////////////////////////////////////////////////////////////////
int al_deinit(void)
{
    int err = 0;

    err = mdc_wrapper_deinit(&al_mcc);
    bail_error(err);

    err = lew_deinit(&al_lew);
    bail_error(err);

bail:
    return err;
}


#if 0
int al_mgmt_handle_action_response(gcl_session *gcls, bn_response **inout_resp, void *arg)
{
    int err = 0;


    bail_null(gcls);
    bail_null(inout_resp);
    bail_null(*inout_resp);
    
    err = bn_response_msg_dump(gcls, *inout_resp, LOG_NOTICE, "act_response");
    bail_error(err);

bail:
    return err;
}
#endif
