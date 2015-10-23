/*
 *
 * Filename:  gmgmthd_mfp_mgmt.c
 * Date:      2011/03/14
 * Author:    Varsha Rajagopalan
 *
 * (C) Copyright 2011 Juniper Networks, Inc.  
 * All rights reserved.
 *
 * The file contains the mfp-gmgmthd  related mgmt/action handling 
 * functions.
 *
 */

#include <pthread.h>
#include <sys/prctl.h>
#include "time.h"
#include "md_client.h"
#include "mdc_wrapper.h"
#include "gmgmthd_main.h"
#include "random.h"
#include "proc_utils.h"
#include "file_utils.h"
#include "clish_module.h"
#include "string.h"
#include "fcntl.h"


// #define DELAY_QUERIES
#define MAX_QUERIES 1000

#define	NS_DEFAULT_PORT		"80"
#define	ALL_STR			"all"

lew_event *mfp_query_timers[MAX_QUERIES];
int mfp_next_query_idx = 0;

/* ------------------------------------------------------------------------- */
/** Local prototypes & definitions
 */
lew_context *mfp_lwc;
md_client_context *mfp_mcc = NULL;
md_client_context *mfp_timer_mcc = NULL;
tbool mfp_exiting = false;
static pthread_t mfp_thread;

static const char *mfp_program_name = "mfp-gmgmthd";
static int mfp_mgmt_handle_session_close(int fd, fdic_callback_type cbt, 
                                        void *vsess, void *gsmfp_arg);

static int mfp_mgmt_handle_event_request(gcl_session *sess,
                                        bn_request **inout_request, void *arg);

static int mfp_mgmt_handle_query_request(gcl_session *sess,
                                        bn_request **inout_query_request,
                                        void *arg);

static int mfp_mgmt_handle_action_request(gcl_session *sess,
                                         bn_request **inout_action_request,
                                         void *arg);

static int handle_shutdown_event(bn_binding_array *);
int unmount_nfs(
	const bn_binding_array *bindings,
	uint32 idx, const bn_binding *binding,
	const tstring *name, const tstr_array *name_components,
	const tstring *name_last_part,
	const tstring *value,
	void *callback_data);
static void *mfp_mgmt_thread_handler_func(void *arg);

/* 
 * MFP PMF generation  related functions
 */
extern int mfp_generate_stream_pmf_file( const char *streamname, tstring **ret_output);

extern int mfp_generate_encap_live_spts_media_profile( FILE *fp,
	const char *streamname, tstring **ret_output);

extern int mfp_generate_encap_file_spts_media_profile( FILE *fp,
	const char *streamname, tstring **ret_output);

extern  int mfp_generate_encap_ssp_media_sbr( FILE *fp,
	const char *streamname, tstring **ret_output);

extern  int mfp_generate_encap_ssp_media_mbr( FILE *fp,
	const char *streamname, tstring **ret_output);

extern int mfp_generate_output_parameter( FILE *fp,
	const char *streamname, tstring **ret_output);
extern  int
mfp_check_if_valid_mount( tstring * t_storage, uint32 *mount_error);

extern  int
mfp_escape_sequence_space_in_address( tstring *t_address);

prefetch_timer_data_t   prefetch_timers[MAX_PREFETCH_JOBS];

extern  int mfp_action_stop( const char *streamname, tstring **ret_output);
extern  int mfp_action_remove( const char *streamname, tstring **ret_output);
extern  int mfp_action_status( const char *streamname, tstring **ret_output);
extern  int mfp_action_restart( const char *streamname, tstring **ret_output);
int mfp_initiate_exit(void);
int mfp_mgmt_init(void);
extern int mfp_status_context_init(void);
extern int
mfp_config_handle_change(const bn_binding_array *arr, uint32 idx,
	                        bn_binding *binding, void *data);
/* ------------------------------------------------------------------------- */
int
mfp_mgmt_init(void)
{
    int err = 0;
    int i = 0;
    mdc_wrapper_params *mwp = NULL;

    err = mdc_wrapper_params_get_defaults(&mwp);
    bail_error(err);

    mwp->mwp_lew_context = mfp_lwc;
    mwp->mwp_gcl_client = gcl_client_mfp;
    mwp->mwp_session_close_func = mfp_mgmt_handle_session_close;
    mwp->mwp_query_request_func = mfp_mgmt_handle_query_request;
    mwp->mwp_action_request_func = mfp_mgmt_handle_action_request;
    mwp->mwp_event_request_func = mfp_mgmt_handle_event_request;

    err = mdc_wrapper_init(mwp, &mfp_mcc);
    bail_error(err);

    err = lc_random_seed_nonblocking_once();
    bail_error(err);

    /*
     * Register to receive config changes.  We care about:
     *   1. Any of our own configuration nodes.
     *   2. Actions - publish, stop, restart, remove
     */
    err = mdc_send_action_with_bindings_str_va
        (mfp_mcc, NULL, NULL, "/mgmtd/events/actions/interest/add", 3,
	 "event_name", bt_name, mdc_event_dbchange,
	"match/1/pattern", bt_string, "/nkn/nvsd/mfp/actions/**",
	"match/2/pattern", bt_string, "/nkn/nvsd/mfp/status-time");

     bail_error(err);
/*
 * Initialise Timer for getting MFP status 
 */
     err = mfp_status_context_init();

bail:
    mdc_wrapper_params_free(&mwp);
    if (err) {
        lc_log_debug(LOG_ERR, 
                     _("Unable to connect to the management daemon"));
    }
    return(err);
}



/* ------------------------------------------------------------------------- */
static int
mfp_mgmt_handle_session_close(int fd, fdic_callback_type cbt, 
                             void *vsess, void *gsmfp_arg)
{
    int err = 0;
    gcl_session *gsmfp_session = vsess;

    /*
     * If we're exiting, this is expected and we shouldn't do anything.
     * Otherwise, it's an error, so we should complain and reestablish 
     * the connection.
     */
    if (mfp_exiting) {
        goto bail;
    }

    lc_log_basic(LOG_ERR, _("Lost connection to mgmtd; exiting"));
    err = mfp_initiate_exit();
    bail_error(err);

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
mfp_mgmt_handle_event_request(gcl_session *sess, 
                             bn_request **inout_request,
                             void *arg)
{
    int err = 0;
    bn_msg_type msg_type = bbmt_none;
    bn_binding_array *old_bindings = NULL, *new_bindings = NULL;
    tstring *event_name = NULL;
    bn_binding_array *bindings = NULL;
    uint32 num_callbacks = 0, i = 0;
    void *data = NULL;
    tbool rechecked_licenses = false;
    tbool t_loaded = false;


    bail_null(inout_request);
    bail_null(*inout_request);
    err = bn_request_get(*inout_request, &msg_type, NULL, false, &bindings,
	    NULL, NULL);
    bail_error(err);
    bail_require(msg_type == bbmt_event_request);
    err = bn_event_request_msg_get_event_name(*inout_request, &event_name);
    bail_error_null(err, event_name);
    lc_log_basic(LOG_INFO, "Received event: %s", ts_str(event_name));
    if (ts_equal_str(event_name, mdc_event_dbchange, false)) {
	err = mdc_event_config_change_notif_extract
	    (bindings, &old_bindings, &new_bindings, NULL);
	bail_error(err);
	err = bn_binding_array_foreach(new_bindings, mfp_config_handle_change,
		&rechecked_licenses);
	bail_error(err);
    }


 bail:
    bn_binding_array_free(&bindings);
    bn_binding_array_free(&old_bindings);
    bn_binding_array_free(&new_bindings);
    ts_free(&event_name);
    return(err);
}

/* ------------------------------------------------------------------------- */
static int 
mfp_mgmt_handle_query_request_async(int fd, short event_type, void *data,
                                   lew_context *ctxt)
{
    int err = 0;
    bn_request *req = data;

    bail_null(req);
    
    bn_request_msg_free(&req);

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
mfp_mgmt_handle_query_request(gcl_session *sess,
                             bn_request **inout_query_request, void *arg)
{
    int err = 0;
    lt_dur_ms ms_to_wait = 0;
    bn_request *req_copy = NULL;
    lew_event **timer = NULL;

    bail_null(inout_query_request);
    bail_null(*inout_query_request);
 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
int
mfp_mgmt_handle_action_request(gcl_session *sess,
                              bn_request **inout_action_request, void *arg)
{
    int err = 0;
    bn_msg_type req_type = bbmt_none;
    bn_binding_array *bindings = NULL;
    tstring *action_name = NULL;
    tstring *t_streamname = NULL;
    tstring *ret_output = NULL;
    int32 status = 0;
    char *bn_mfp_status = NULL;
    uint32 ret_err=0;
    tstring *msg = NULL;
    char bn_stop_time[256];

     lt_date date;
     lt_time_sec curr_time;

    tstring  *t_stoptime = NULL;
    char *time_str = NULL;

    /* Standard Samara logic first */
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

    /* Action handling for PUBLISH the XML file for MFP*/
    if (ts_equal_str (action_name, "/nkn/nvsd/mfp/actions/publish", false)){
	int error_flag = 0;
	err = bn_binding_array_get_value_tstr_by_name
	    (bindings, "stream-name", NULL, &t_streamname);
	if (!t_streamname) {
	    lc_log_basic(LOG_NOTICE, "Got  null stream name\n");
	    goto bail;
	}
	lc_log_basic(LOG_NOTICE, I_("Got the publish action for %s stream\n"),
		    		ts_str(t_streamname));
	/* Call the  XML file generator*/
	err = mfp_generate_stream_pmf_file(ts_str(t_streamname), &ret_output);
	if (err) {
		error_flag = 1;
	}
	/* Send response message*/
	err = bn_response_msg_create_send
		(sess, bbmt_action_response,
		 bn_request_get_msg_id(*inout_action_request),
		 error_flag, ts_str(ret_output), 0);
	/* if no error set the status of the session as published */
	if(!error_flag){
	        bn_mfp_status=smprintf("/nkn/nvsd/mfp/config/%s/status",ts_str(t_streamname));		
		bail_null(bn_mfp_status);
	        err = mdc_set_binding(mfp_mcc, &ret_err, &msg,
                        bsso_modify,bt_string,"PUBLISH",
                        bn_mfp_status);
        	bail_error(err);
		if(ret_err) {
  			lc_log_debug(LOG_WARNING, I_("Could not set status for the stream name %s"),
					ts_str(t_streamname));
			err = bn_response_msg_create_send
                           (sess, bbmt_action_response, bn_request_get_msg_id(*inout_action_request),
                        1, _("error : Invalid status"),0);
			goto bail;
		}
	}

    } /* Action - control pmf for stop*/
    else if (ts_equal_str (action_name, "/nkn/nvsd/mfp/actions/stop", false)){
        err = bn_binding_array_get_value_tstr_by_name
            (bindings, "stream-name", NULL, &t_streamname);
        if (!t_streamname) {
            lc_log_basic(LOG_NOTICE, "Got  null stream name\n");
            goto bail;
        }
        lc_log_basic(LOG_NOTICE, I_("Got the stop action for %s stream\n"),
                                ts_str(t_streamname));
        /* Call the  XML file generator*/
        err = mfp_action_stop(ts_str(t_streamname), &ret_output);
        bail_error(err);
        /* Send response message*/
        err = bn_response_msg_create_send
                (sess, bbmt_action_response,
                 bn_request_get_msg_id(*inout_action_request),
                 0, ts_str(ret_output), 0);
        bail_error(err);
#if 1
	/*
	 * After sending the stop PMF ,set the status as stopped
	 */
	/* if no error set the status of the session as published */
	bn_mfp_status=smprintf("/nkn/nvsd/mfp/config/%s/status",ts_str(t_streamname));		
	bail_null(bn_mfp_status);
	err = mdc_set_binding(mfp_mcc, &ret_err, &msg,
		bsso_modify,bt_string,"STOPPED",
		bn_mfp_status);
	bail_error(err);
	if(ret_err) {
	    lc_log_debug(LOG_WARNING, I_("Could not set status for the stream name %s"),
		    ts_str(t_streamname));
	    err = bn_response_msg_create_send
		(sess, bbmt_action_response, bn_request_get_msg_id(*inout_action_request),
		 1, _("error : Invalid status"),0);
	    goto bail;
	}
	lt_time_sec stop_time = lt_curr_time();
	snprintf(bn_stop_time , sizeof(bn_stop_time),"/nkn/nvsd/mfp/config/%s/stop-time", ts_str(t_streamname));		
	err = lt_daytime_sec_to_str(stop_time, &time_str);
	bail_error(err);

	err = mdc_set_binding(mfp_mcc, &ret_err, &msg,
		bsso_modify, bt_time_sec, 
		time_str, bn_stop_time);
	bail_error(err);
#endif
    }
    /*Replicating the above block of code, so in future any changes to any of the actions can be handled independently*/
    /* Action handling for REMOVE the XML file for MFP*/
    else if (ts_equal_str (action_name, "/nkn/nvsd/mfp/actions/remove", false)){
        err = bn_binding_array_get_value_tstr_by_name
            (bindings, "stream-name", NULL, &t_streamname);
        if (!t_streamname) {
            lc_log_basic(LOG_NOTICE, "Got  null stream name\n");
            goto bail;
        }
        lc_log_basic(LOG_NOTICE, I_("Got the remove action for %s stream\n"),
                                ts_str(t_streamname));
        /* Call the  XML file generator*/
        err = mfp_action_remove(ts_str(t_streamname), &ret_output);
        bail_error(err);
        /* Send response message*/
        err = bn_response_msg_create_send
                (sess, bbmt_action_response,
                 bn_request_get_msg_id(*inout_action_request),
                 0, ts_str(ret_output), 0);
        bail_error(err);
    }
    /* Action handling for STATUS the XML file for MFP*/
    else if (ts_equal_str (action_name, "/nkn/nvsd/mfp/actions/status", false)){
        err = bn_binding_array_get_value_tstr_by_name
            (bindings, "stream-name", NULL, &t_streamname);
        if (!t_streamname) {
            lc_log_basic(LOG_NOTICE, "Got  null stream name\n");
            goto bail;
        }
        lc_log_basic(LOG_NOTICE, I_("Got the status action for %s stream\n"),
                                ts_str(t_streamname));
        /* Call the  XML file generator*/
        err = mfp_action_status(ts_str(t_streamname), &ret_output);
        bail_error(err);
        /* Send response message*/
        err = bn_response_msg_create_send
                (sess, bbmt_action_response,
                 bn_request_get_msg_id(*inout_action_request),
                 0, ts_str(ret_output), 0);
        bail_error(err);
     }
    /* Action handling for RESTART the XML file for MFP*/
    else if (ts_equal_str (action_name, "/nkn/nvsd/mfp/actions/restart", false)){
        err = bn_binding_array_get_value_tstr_by_name
            (bindings, "stream-name", NULL, &t_streamname);
        if (!t_streamname) {
            lc_log_basic(LOG_NOTICE, "Got  null stream name\n");
            goto bail;
        }
        lc_log_basic(LOG_NOTICE, I_("Got the restart action for %s stream\n"),
                                ts_str(t_streamname));
	/*Check if atleast a minute has lapsed since stop*/
	lt_time_sec restart_time = lt_curr_time();
	err = lt_daytime_sec_to_str(restart_time, &time_str);
	bail_error(err);

	lt_time_sec stop_time ;
	err = mdc_get_binding_tstr_fmt(mfp_mcc, NULL, NULL, NULL,
		&t_stoptime, NULL,
		"/nkn/nvsd/mfp/config/%s/stop-time",
				ts_str(t_streamname));
	bail_error(err);
	lt_str_to_daytime_sec ( ts_str(t_stoptime), &stop_time);
	bail_error(err);
	lt_str_to_daytime_sec ( time_str, &restart_time);
	bail_error(err);
	if( restart_time - stop_time < 60) {
	    err = bn_response_msg_create_send
		(sess, bbmt_action_response,
		 bn_request_get_msg_id(*inout_action_request),
		 1, "Stopping the session. Please Wait for a minute before restarting the session.", 0);
	    bail_error(err);
	    goto bail;
	}

        /* Call the  XML file generator*/
        err = mfp_action_restart(ts_str(t_streamname), &ret_output);
        bail_error(err);
        /* Send response message*/
        err = bn_response_msg_create_send
                (sess, bbmt_action_response,
                 bn_request_get_msg_id(*inout_action_request),
                 0, ts_str(ret_output), 0);
        bail_error(err);
	/*
	 * After sending the restart PMF ,set the status as PUBLISH
	 */
	/* if no error set the status of the session as published */
	bn_mfp_status=smprintf("/nkn/nvsd/mfp/config/%s/status",ts_str(t_streamname));
	bail_null(bn_mfp_status);
	err = mdc_set_binding(mfp_mcc, &ret_err, &msg,
		bsso_modify,bt_string,"PUBLISH",
		bn_mfp_status);
	bail_error(err);
	if(ret_err) {
	    lc_log_debug(LOG_WARNING, I_("Could not set status for the stream name %s"),
		    ts_str(t_streamname));
	    err = bn_response_msg_create_send
		(sess, bbmt_action_response, bn_request_get_msg_id(*inout_action_request),
		 1, _("error : Invalid status"),0);
	    goto bail;
	}

    }
    else {
        lc_log_debug(LOG_WARNING, I_("Got unexpected action %s"),
                     ts_str(action_name));
        err = bn_response_msg_create_send
            (sess, bbmt_action_response, bn_request_get_msg_id(*inout_action_request),
             1, _("error : action not yet supported"), 0);
        goto bail;
    }

 bail:
    ts_free(&t_streamname);
    ts_free(&t_stoptime);
    ts_free(&action_name);
    ts_free(&ret_output);
    bn_binding_array_free(&bindings);
    safe_free(bn_mfp_status);
    safe_free(time_str);
    return(err);
}

/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- *
 * Purpose : The timer thread is going to be a GCL client and since we do not
 * have thread support within GCL, we are using another GCL client for the timer
 * thread. 
 * (A little over kill but cannot think of a quicker way at this time - Ram)
 */
int
mfp_initiate_exit(void)
{
    int err = 0;
    uint32 num_recs = 0, i = 0;

    /*
     * Some occurrences are only OK if they happen while we're
     * exiting, such as getting NULL back for a mgmt request.
     */
    mfp_exiting = true;


    /*
     * Need to do this here to remove our mgmt-related events.
     */
    err = mdc_wrapper_disconnect(mfp_mcc, false);
    bail_error(err);

    /*
     * Calling lew_delete_all_events() is not guaranteed
     * to solve this for us, since the GCL/libevent shim does not use
     * the libevent wrapper.  lew_escape_from_dispatch() will
     * definitely do it, though.
     */
    err = lew_escape_from_dispatch(mfp_lwc, true);
    bail_error(err);

 bail:
    return(err);
}
int
mfp_init(void)
{
    int err = 0;
    uint32 i = 0;

    /* Don't mess with the modes we choose */
    umask(0);

#if defined(PROD_FEATURE_I18N_SUPPORT)
    err = lc_set_locale_from_config(true);
    complain_error(err);
    err = 0;
#endif

    /* Initialize libevent wrapper */
    err = lew_init(&mfp_lwc);
    bail_error_null(err, mfp_lwc);

    /*
     * Register to hear about all the signals we care about.
     * These are automatically persistent, which is fine.
     */

    err = mfp_mgmt_init();
    bail_error(err);

 bail:
    return(err);
}

/* ------------------------------------------------------------------------- */
/*
 *  *      funtion : nvsd_disk_mgmt_thread_init ()
 *   *      purpose : thread handler for the management module
 *    */
int
mfp_mgmt_thread_init(void)
{
    int err = 0;

        if (pthread_create(&mfp_thread, NULL,
			mfp_mgmt_thread_handler_func, NULL)) {
		lc_log_basic(LOG_INFO, "Failed to create JCC gmgmthd  thread.%s", "");
		return 1;
        }

 bail:
    return(err);

} /* end of nvsd_disk_mgmt_thread_init() */


static void *
mfp_mgmt_thread_handler_func(void *arg)
{
	int err = 0;

	/* Init the context inside the thread */
	err = mfp_init();
	bail_error(err);

	err = lew_dispatch(mfp_lwc, NULL, NULL);
	bail_error(err);

	/* De-init the mfp gcl context inside the mfp thread */
	err = mfp_deinit();
	complain_error(err);

bail:
	return NULL;
}

int
mfp_deinit(void)
{
    int err = 0;

    err = mdc_wrapper_deinit(&mfp_mcc);
    bail_error(err);

    err = lew_deinit(&mfp_lwc);
    bail_error(err);

 bail:
    return(err);
}
