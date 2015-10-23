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
#include <ctype.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>

#include "log_accesslog_pri.h"
#include "nknlogd.h"




///////////////////////////////////////////////////////////////////////////////
lew_context *mgmt_lew = NULL;
md_client_context *mgmt_mcc = NULL;
gcl_session *sess = NULL;
tbool mgmt_exiting = false;
extern pthread_mutex_t upload_mgmt_log_lock;
static lew_event *mgmt_retry_event = NULL;
tbool lgd_exiting = false;
///////////////////////////////////////////////////////////////////////////////
extern int exit_req;

int update_from_db_done = 0;

volatile int mgmt_thrd_initd = 0;

extern int
action_upload_callback(gcl_session *gcls, bn_response **inout_resp,
		        void *arg);
///////////////////////////////////////////////////////////////////////////////
int mgmt_handle_event_request(gcl_session *session, bn_request  **inout_request,
        void        *arg);
int mgmt_handle_session_close(int fd, fdic_callback_type cbt, void *vsess,
        void *gsec_arg);
int config_handle_set_request(
        bn_binding_array *old_bindings, bn_binding_array *new_bindings);
int module_cfg_handle_change(
        bn_binding_array *old_bindings, bn_binding_array *new_bindings);


int
mgmt_event_registeration_init(void);

int access_log_query_init(void);

int error_log_query_init(void);

int trace_log_query_init(void);

int cache_log_query_init(void);

int stream_log_query_init(void);

int fuse_log_query_init(void);

int mfp_log_query_init(void);

int
log_upload(
    logd_file *plf,
    const char *this,
    const char *that);

int
mgmt_retry_handler(int fd, short event_type, void *data, lew_context *ctxt);
///////////////////////////////////////////////////////////////////////////////
int
main_loop(void)
{
	int err = 0;
	lc_log_debug(LOG_DEBUG, _("main_loop () - before lew_dispatch"));

	err = lew_dispatch (mgmt_lew, NULL, NULL);
	bail_error(err);
bail:
	return err;

}

/* List of signals server can handle */
static const int signals_handled[] = {
            SIGHUP, SIGINT, SIGPIPE, SIGTERM, SIGUSR1
};

#define num_signals_handled (int)(sizeof(signals_handled) / sizeof(int))

/* Libevent handles for server */
static lew_event *event_signals[num_signals_handled];

static int handle_signal(
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
int mgmt_init(int init_value)
{
	int err = 0;
	uint32 i = 0;

	err = lc_log_init(program_name, NULL, LCO_none | LCO_NO_THREAD_ID,
	    LC_COMPONENT_ID(LCI_NKNLOGD),
	    LOG_DEBUG, LOG_LOCAL4, LCT_SYSLOG);
	bail_error(err);

	err = lc_init();
	bail_error(err);

	err = lew_init(&mgmt_lew);
	bail_error(err);

	/*
	* Register to hear about all the signals we care about.
	* These are automatically persistent, which is fine.
	*/
	for (i = 0; i < (int) num_signals_handled; ++i) {
		err = lew_event_reg_signal
	    			(mgmt_lew, &(event_signals[i]), signals_handled[i],
	    			handle_signal, NULL);
		bail_error(err);
	}


	err = mgmt_event_registeration_init();
	printf("pass %d\n",err);
	bail_error(err);

	if (init_value)
	{

		err = access_log_query_init();
		bail_error(err);

		err = error_log_query_init();
		bail_error(err);

		err = trace_log_query_init();
		bail_error(err);

		err = cache_log_query_init();
		bail_error(err);

		err = stream_log_query_init();
		bail_error(err);

		err = fuse_log_query_init();
		bail_error(err);

		err = mfp_log_query_init();
		bail_error(err);
	}
bail:
	if (err) // registration failed
	{
		/* Safe to call all the exits */
		mgmt_initiate_exit ();

		/* Ensure we set the flag back */
		mgmt_exiting = false;
	}

	/* Enable the flag to indicate config init is done */
	update_from_db_done = 1;

	if(!err){
	    mgmt_thrd_initd = 1;
	}


	return err;
}

///////////////////////////////////////////////////////////////////////////////
int
mgmt_event_registeration_init(void)
{
        int err = 0;
        mdc_wrapper_params *mwp = NULL;


        err = mdc_wrapper_params_get_defaults(&mwp);
        bail_error(err);

        mwp->mwp_lew_context = mgmt_lew;
        mwp->mwp_gcl_client = gcl_client_nknaccesslog;

        mwp->mwp_session_close_func = mgmt_handle_session_close;
        mwp->mwp_event_request_func = mgmt_handle_event_request;

        err = mdc_wrapper_init(mwp, &mgmt_mcc);
        bail_error(err);

        err = lc_random_seed_nonblocking_once();
        bail_error(err);


	/*! accesslog binding registeration */
        err = mdc_send_action_with_bindings_str_va(
                        mgmt_mcc,
                        NULL,
                        NULL,
                        "/mgmtd/events/actions/interest/add",
                        9,
                        "event_name", bt_name, mdc_event_dbchange,
                        "match/1/pattern", bt_string, "/nkn/accesslog/config/profile/**",
			"match/2/pattern", bt_string, "/nkn/errorlog/config/**",
			"match/3/pattern", bt_string, "/nkn/tracelog/config/**",
			"match/4/pattern", bt_string, "/nkn/cachelog/config/**",
			"match/5/pattern", bt_string, "/nkn/streamlog/config/**",
			"match/6/pattern", bt_string, "/nkn/fuselog/config/**",
			"match/7/pattern", bt_string, "/nkn/mfplog/config/**",
			"match/8/pattern", bt_string, "/nkn/nvsd/system/config/debug/**");
	bail_error(err);

	sess = mdc_wrapper_get_gcl_session(mgmt_mcc);
	bail_null(sess);

	err = bn_set_session_callback_action_response_msg(sess,
		action_upload_callback,
		NULL);
	bail_error(err);
bail:
	mdc_wrapper_params_free(&mwp);
	if (err) {
	    lc_log_debug(LOG_ERR,
		    _("Unable to connect to the management daemon"));
	}
        return err;
}


int
mgmt_retry_handler(int fd, short event_type, void *data, lew_context *ctxt)
{
    int err = 0;

    UNUSED_ARGUMENT(fd);
    UNUSED_ARGUMENT(event_type);
    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(ctxt);

    err  = mgmt_event_registeration_init();
    bail_error(err);

bail:
    return err;
}

///////////////////////////////////////////////////////////////////////////////
int mgmt_handle_session_close(
        int fd,
        fdic_callback_type cbt,
        void *vsess,
        void *gsec_arg)
{
        int err = 0;
        gcl_session *gsec_session = vsess;

        if ( mgmt_exiting) {
                goto bail;
        }


	lc_log_basic(LOG_ERR, _("Lost connection to mgmtd; reconnecting"));

    /* If we haven't connected to mgmtd even once
     * bail out
     */
      if(!mgmt_thrd_initd)
          goto bail;


	/* Reconnect to mgmtd */
	err = lew_event_reg_timer(mgmt_lew, &mgmt_retry_event,
		mgmt_retry_handler,
		NULL, 5000);
	bail_error(err);


/*	pthread_mutex_lock (&upload_mgmt_log_lock);
        err = mgmt_initiate_exit();
	pthread_mutex_unlock (&upload_mgmt_log_lock);
        bail_error(err);
*/

bail:
        return err;
}

///////////////////////////////////////////////////////////////////////////////
int mgmt_handle_event_request(
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

                err = config_handle_set_request(old_bindings, new_bindings);
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
int config_handle_set_request(
        bn_binding_array *old_bindings,
        bn_binding_array *new_bindings)
{
        int err = 0;
        err = module_cfg_handle_change(old_bindings, new_bindings);
        bail_error(err);
bail:
        return err;
}

///////////////////////////////////////////////////////////////////////////////
int log_basic(const char *fmt, ...)
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

int log_debug(const char *fmt, ...)
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


int
mgmt_initiate_exit(void)
{
    int err = 0;
    int i;

    mgmt_exiting = true;
    lgd_exiting = true;

    for(i = 0; i < num_signals_handled; ++i) {
        err = lew_event_delete(mgmt_lew, &(event_signals[i]));
	event_signals[i] = NULL; // Needs to be NULL for the case of retry
        bail_error(err);
    }

    err = mdc_wrapper_disconnect(mgmt_mcc, false);
    bail_error(err);

    err = lew_escape_from_dispatch(mgmt_lew, true);
    bail_error(err);

bail:
    return err;
}


///////////////////////////////////////////////////////////////////////////////
int deinit(void)
{
    int err = 0;

    err = mdc_wrapper_deinit(&mgmt_mcc);
    bail_error(err);
    mgmt_mcc = NULL;

    err = lew_deinit(&mgmt_lew);
    bail_error(err);

    mgmt_lew = NULL;

bail:
    return err;
}

void log_write_report_err(int ret, logd_file *plf)
{
    int err = 0;

    if (ret == EOF && errno == ENOSPC) {
	if (plf->type == TYPE_ACCESSLOG) {
	    lc_log_basic(LOG_NOTICE, "No space left on partition,"
		    "Failed to write accesslog for profile %s",
		    plf->profile);
	} else {
	    lc_log_basic(LOG_NOTICE, "No space left on partition,"
		    "Failed to write log file %s",
		    plf->filename);
	}

	err = mdc_send_event_with_bindings_str_va
	    (mgmt_mcc, NULL, NULL, "/nkn/nknlogd/events/logwritefailed", 1,
	     "logfile", bt_string,
	     plf->filename);
	complain_error(err);

    }

bail:
    return;

}
