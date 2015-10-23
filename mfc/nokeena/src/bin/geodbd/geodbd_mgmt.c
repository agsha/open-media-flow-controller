/*
 * Filename :   geodbd_mgmt.c
 * Date:        04 July 2011
 * Author:      Muthukumar
 *
 * (C) Copyright 2011, Juniper Networks, Inc.
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
#include <unistd.h>

#include "geodbd_mgmt.h"
#include "nkn_cfg_params.h"

static int main_loop(void);
int geodbd_info_display( const char *app_name, tstring **ret_output);
///////////////////////////////////////////////////////////////////////////////
static int main_loop(void)
{
	int err = 0;
	lc_log_debug(LOG_DEBUG, _("main_loop () - before lew_dispatch"));

	err = lew_dispatch (geodbd_lew, NULL, NULL);
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
extern void catcher(int sig);

static int geodbd_handle_signal(
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
int geodbd_mgmt_process(int init_value)
{
	int err = 0;
	uint32 i = 0;

	err = lc_log_init(program_name, NULL, LCO_none,
	    LC_COMPONENT_ID(LCI_GEODBD),
	    LOG_DEBUG, LOG_LOCAL4, LCT_SYSLOG);
	bail_error(err);

	err = lew_init(&geodbd_lew);
	bail_error(err);

	/*
	* Register to hear about all the signals we care about.
	* These are automatically persistent, which is fine.
	*/
	for (i = 0; i < (int) num_signals_handled; ++i) {
		err = lew_event_reg_signal
	    			(geodbd_lew, &(event_signals[i]), signals_handled[i],
	    			geodbd_handle_signal, NULL);
		bail_error(err);
	}


	err = geodbd_mgmt_event_registration_init();
	printf("pass %d\n",err);
	bail_error(err);

	if (init_value)
	{
		err = geodbd_query_init();
		bail_error(err);
	}	

bail:
	if (err) // registration failed
	{
		/* Safe to call all the exits */
		geodbd_mgmt_initiate_exit ();

		/* Ensure we set the flag back */
		geodbd_exiting = false;
	}

	/* Enable the flag to indicate config init is done */
	update_from_db_done = 1;

	return err;
}



///////////////////////////////////////////////////////////////////////////////
int geodbd_mgmt_event_registration_init(void)
{
        int err = 0;
        mdc_wrapper_params *mwp = NULL;

        err = mdc_wrapper_params_get_defaults(&mwp);
        bail_error(err);

        mwp->mwp_lew_context = geodbd_lew;
        mwp->mwp_gcl_client = gcl_client_geodbd;

        mwp->mwp_session_close_func = geodbd_mgmt_handle_session_close;
        mwp->mwp_event_request_func = geodbd_mgmt_handle_event_request;
	mwp->mwp_action_request_func = geodbd_mgmt_handle_action_request;
       
        err = mdc_wrapper_init(mwp, &geodbd_mcc);
        bail_error(err);

        err = lc_random_seed_nonblocking_once();
        bail_error(err);


	/*! accesslog binding registeration */
        err = mdc_send_action_with_bindings_str_va(
                        geodbd_mcc,
                        NULL,
                        NULL,
                        "/mgmtd/events/actions/interest/add",
                        2,
                        "event_name", bt_name, mdc_event_dbchange,
                        "match/1/pattern", bt_string, "/nkn/geodbd/config/**",
			"match/2/pattern", bt_string, "/nkn/geodb/actions/**");
			bail_error(err);


bail:
        mdc_wrapper_params_free(&mwp);
        if (err) {
                lc_log_debug(LOG_ERR, 
                                _("Unable to connect to the management daemon"));
        }
        return err;
}



///////////////////////////////////////////////////////////////////////////////
int geodbd_mgmt_handle_session_close(
        int fd,
        fdic_callback_type cbt,
        void *vsess,
        void *gsec_arg)
{
        int err = 0;
        gcl_session *gsec_session = vsess;

        if ( geodbd_exiting) {
                goto bail;
        }

        lc_log_basic(LOG_ERR, _("Lost connection to mgmtd; exiting"));
	pthread_mutex_lock (&upload_mgmt_geodbd_lock);
        err = geodbd_mgmt_initiate_exit();
	pthread_mutex_unlock (&upload_mgmt_geodbd_lock);
        bail_error(err);

bail:
        return err;
}

///////////////////////////////////////////////////////////////////////////////
int geodbd_mgmt_handle_event_request(
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

                err = geodbd_config_handle_set_request(old_bindings, new_bindings);
                bail_error(err);
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

int
geodbd_query_init (void)
{
        int err = 0;
        bn_binding_array *bindings = NULL;
        tbool rechecked_licenses = false;


        lc_log_basic(LOG_DEBUG, "geodbd error log query initializing ..");
        err = mdc_get_binding_children(geodbd_mcc,
                        NULL,
                        NULL,
                        true,
                        &bindings,
                        false,
                        true,
                        geodbd_prefix);
        bail_error_null(err, bindings);

        err = bn_binding_array_foreach(bindings, geodbd_cfg_handle_change,
                        &rechecked_licenses);
        bail_error(err);
bail:
        bn_binding_array_free(&bindings);
        return err;
}

int
geodbd_cfg_handle_change(const bn_binding_array *arr, uint32 idx,
                        bn_binding *binding, void *data)
{
        int err = 0;
        const tstring *name = NULL;
        tbool *rechecked_licenses_p = data;
        tstring *str = NULL;

        bail_null(rechecked_licenses_p);

        err = bn_binding_get_name(binding, &name);
        bail_error(err);

/*
        if (ts_equal_str(name, "/nkn/adnsd/config/enable", false)) {
            tbool proxy_enabled = 0;

            err = bn_binding_get_tbool(binding,
                                ba_value, NULL,
                                &adnsd_enabled);

            bail_error(err);
            adnsd_enabled = adnsd_enabled;

            lc_log_basic(LOG_NOTICE, "Read .../l4proxy/config/enable as : \"%d\"",
                                proxy_enabled);
	}
*/
bail:
        ts_free(&str);
        return err;

}


int
geodbd_module_cfg_handle_change(
        bn_binding_array *old_bindings,
        bn_binding_array *new_bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    /*! error log handle change callback */
    err = bn_binding_array_foreach (new_bindings, geodbd_cfg_handle_change,
                                        &rechecked_licenses);
    bail_error(err);


bail:
    return err;
}


///////////////////////////////////////////////////////////////////////////////
int geodbd_config_handle_set_request(
        bn_binding_array *old_bindings,
        bn_binding_array *new_bindings)
{
        int err = 0;
        err = geodbd_module_cfg_handle_change(old_bindings, new_bindings);
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
    n = vsprintf(outstr, fmt, ap);
    lc_log_basic(LOG_NOTICE, "%s", outstr);
    va_end(ap);

    return n;
}

int log_debug(const char *fmt, ...)
{
    int n = 0;
    char outstr[1024];
    va_list ap;

    va_start(ap, fmt);
    n = vsprintf(outstr, fmt, ap);
    lc_log_basic(LOG_INFO, "%s", outstr);
    va_end(ap);

    return n;
}

int geodbd_mgmt_initiate_exit(void)
{
    int err = 0;
    int i;
    
    geodbd_exiting = true;

    for(i = 0; i < num_signals_handled; ++i) {
        err = lew_event_delete(geodbd_lew, &(event_signals[i]));
	event_signals[i] = NULL; // Needs to be NULL for the case of retry
        bail_error(err);
    }

    err = mdc_wrapper_disconnect(geodbd_mcc, false);
    bail_error(err);

    err = lew_escape_from_dispatch(geodbd_lew, true);
    bail_error(err);

bail:
    return err;
}


///////////////////////////////////////////////////////////////////////////////
int geodbd_deinit(void)
{
    int err = 0;

    err = mdc_wrapper_deinit(&geodbd_mcc);
    bail_error(err);
    geodbd_mcc = NULL;

    err = lew_deinit(&geodbd_lew);
    bail_error(err);

    geodbd_lew = NULL;

bail:
    return err;
}


void * geodbd_mgmt_func(void * arg)
{
        int err = 0;

        int start = 1;
        /* Connect/create GCL sessions, initialize mgmtd i/f
        */
        while (1)
        {

                geodbd_exiting = false;
                printf("geodbd connect: initialized\n");
                if (start == 1) {
                        err = geodbd_mgmt_process(1);
                        bail_error(err);
                } else {
                        while ((err = geodbd_mgmt_process(0)) != 0) {
                                geodbd_deinit();
                                sleep(2);
                        }
                }

                err = main_loop();
                complain_error(err);

                err = geodbd_deinit();
                complain_error(err);
                printf("mgmt connect: de-initialized\n");
                start = 0;
        }

bail:
        printf("adnsd_mgmt_func: exit\n");
        return NULL;
}



void geodbd_mgmt_init(void)
{
	pthread_t geodbdid;

	pthread_mutex_init(&upload_mgmt_geodbd_lock, NULL);
	pthread_create(&geodbdid, NULL, geodbd_mgmt_func, NULL);

	return;
}

int
geodbd_mgmt_handle_action_request(gcl_session *sess,
	bn_request **inout_action_request, void *arg)
{
    int err = 0;
    bn_msg_type req_type = bbmt_none;
    bn_binding_array *bindings = NULL;
    tstring *action_name = NULL;
    tstring *t_filename = NULL;
    tstring *t_app_name = NULL;
    tstring *ret_output = NULL;
    bn_request *req = NULL;

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
    if (ts_equal_str (action_name, "/nkn/geodb/actions/install", false)){
	err = bn_binding_array_get_value_tstr_by_name
	    (bindings, "filename", NULL, &t_filename);
	if (!t_filename) {
	    lc_log_basic(LOG_NOTICE, "No filename entered\n");
	    goto bail;
	}
	lc_log_basic(LOG_NOTICE, I_("Got the install action for %s \n"),
		ts_str(t_filename));
	/* Call the  Geodbd function*/
	err = geodbd_install(ts_str(t_filename), &ret_output);
	/* Send response message*/
	err = bn_response_msg_create_send
	    (sess, bbmt_action_response,
	     bn_request_get_msg_id(*inout_action_request),
	     err, ts_str(ret_output), 0);

    /*Raise an event to notify the installation*/
    err = bn_event_request_msg_create(&req, "/nkn/geodb/events/installation");
    bail_error(err);

    err = mdc_send_mgmt_msg(geodbd_mcc, req, false, NULL, NULL, NULL);
    bail_error(err);
    }
    else if (ts_equal_str (action_name, "/nkn/geodb/actions/info_display", false)){

	err = bn_binding_array_get_value_tstr_by_name
	    (bindings, "app_name", NULL, &t_app_name);
	if (!t_app_name) {
	    lc_log_basic(LOG_NOTICE, "No application entered\n");
	    goto bail;
	}
	lc_log_basic(LOG_NOTICE, I_("Got the info_display action for %s \n"),
		ts_str(t_app_name));
	/* Call the  Geodbd function*/
	err = geodbd_info_display(ts_str(t_app_name), &ret_output);
	/* Send response message*/
	err = bn_response_msg_create_send
	    (sess, bbmt_action_response,
	     bn_request_get_msg_id(*inout_action_request),
	     err, ts_str(ret_output), 0);
    }

bail:
    ts_free(&action_name);
    ts_free(&ret_output);
    ts_free(&t_filename);
    bn_binding_array_free(&bindings);
	bn_request_msg_free(&req);

    return err;
}

extern int geodb_install( const char *file_path_gz, char *ret_output);

int geodbd_install( const char *path, tstring **ret_output)
{
    int err = 0;
    char *ret_string = NULL;
    char str[1024];

    geodb_install( path, str);

    ret_string = smprintf("%s", str);
    ts_new_str_takeover(ret_output, &ret_string, -1, -1);

bail:
    return err;

}
extern int geodb_info( const char *name, char *ret_output, int length);

int geodbd_info_display( const char *app_name, tstring **ret_output) {
    int err = 0;
    char *ret_string = NULL;
    char str[1024];

    err = geodb_info( app_name, str, 1024);
    ret_string = smprintf("%s", str);
    ts_new_str_takeover(ret_output, &ret_string, -1, -1);
bail:
    return err;

}


