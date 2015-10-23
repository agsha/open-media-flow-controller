/*
 * Filename :   adnsd_mgmt.c
 * Date:        23 May 2011
 * Author:      Kiran Desai
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

#include "adnsd_mgmt.h"
#include "nkn_cfg_params.h"

static int main_loop(void);

///////////////////////////////////////////////////////////////////////////////
static int main_loop(void)
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
extern void catcher(int sig);

static int adnsd_handle_signal(
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
int adnsd_mgmt_process(int init_value)
{
	int err = 0;
	uint32 i = 0;

	err = lc_log_init(program_name, NULL, LCO_none,
	    LC_COMPONENT_ID(LCI_NKNACCESSLOG),
	    LOG_DEBUG, LOG_LOCAL4, LCT_SYSLOG);
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
	    			adnsd_handle_signal, NULL);
		bail_error(err);
	}


	err = adnsd_mgmt_event_registration_init();
	printf("pass %d\n",err);
	bail_error(err);

	if (init_value)
	{
		err = adnsd_query_init();
		bail_error(err);
	}	

bail:
	if (err) // registration failed
	{
		/* Safe to call all the exits */
		adnsd_mgmt_initiate_exit ();

		/* Ensure we set the flag back */
		mgmt_exiting = false;
	}

	/* Enable the flag to indicate config init is done */
	update_from_db_done = 1;

	return err;
}



///////////////////////////////////////////////////////////////////////////////
int adnsd_mgmt_event_registration_init(void)
{
        int err = 0;
        mdc_wrapper_params *mwp = NULL;

        err = mdc_wrapper_params_get_defaults(&mwp);
        bail_error(err);

        mwp->mwp_lew_context = mgmt_lew;
        mwp->mwp_gcl_client = gcl_client_adnsd;

        mwp->mwp_session_close_func = adnsd_mgmt_handle_session_close;
        mwp->mwp_event_request_func = adnsd_mgmt_handle_event_request;
       
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
                        2,
                        "event_name", bt_name, mdc_event_dbchange,
                        "match/1/pattern", bt_string, "/nkn/adnsd/config/**");
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
int adnsd_mgmt_handle_session_close(
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

        lc_log_basic(LOG_ERR, _("Lost connection to mgmtd; exiting"));
	pthread_mutex_lock (&upload_mgmt_adnsd_lock);
        err = adnsd_mgmt_initiate_exit();
	pthread_mutex_unlock (&upload_mgmt_adnsd_lock);
        bail_error(err);

bail:
        return err;
}

///////////////////////////////////////////////////////////////////////////////
int adnsd_mgmt_handle_event_request(
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

                err = adnsd_config_handle_set_request(old_bindings, new_bindings);
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
adnsd_query_init (void)
{
        int err = 0;
        bn_binding_array *bindings = NULL;
        tbool rechecked_licenses = false;


        lc_log_basic(LOG_DEBUG, "nvsd error log query initializing ..");
        err = mdc_get_binding_children(mgmt_mcc,
                        NULL,
                        NULL,
                        true,
                        &bindings,
                        false,
                        true,
                        adnsd_prefix);
        bail_error_null(err, bindings);

        err = bn_binding_array_foreach(bindings, adnsd_cfg_handle_change,
                        &rechecked_licenses);
        bail_error(err);
bail:
        bn_binding_array_free(&bindings);
        return err;
}

int
adnsd_cfg_handle_change(const bn_binding_array *arr, uint32 idx,
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
adnsd_module_cfg_handle_change(
        bn_binding_array *old_bindings,
        bn_binding_array *new_bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    /*! error log handle change callback */
    err = bn_binding_array_foreach (new_bindings, adnsd_cfg_handle_change,
                                        &rechecked_licenses);
    bail_error(err);


bail:
    return err;
}


///////////////////////////////////////////////////////////////////////////////
int adnsd_config_handle_set_request(
        bn_binding_array *old_bindings,
        bn_binding_array *new_bindings)
{
        int err = 0;
        err = adnsd_module_cfg_handle_change(old_bindings, new_bindings);
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

int adnsd_mgmt_initiate_exit(void)
{
    int err = 0;
    int i;
    
    mgmt_exiting = true;

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
int adnsd_deinit(void)
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


void * adnsd_mgmt_func(void * arg)
{
        int err = 0;

        int start = 1;
        /* Connect/create GCL sessions, initialize mgmtd i/f
        */
        while (1)
        {

                mgmt_exiting = false;
                printf("mgmt connect: initialized\n");
                if (start == 1) {
                        err = adnsd_mgmt_process(1);
                        bail_error(err);
                } else {
                        while ((err = adnsd_mgmt_process(0)) != 0) {
                                adnsd_deinit();
                                sleep(2);
                        }
                }

                err = main_loop();
                complain_error(err);

                err = adnsd_deinit();
                complain_error(err);
                printf("mgmt connect: de-initialized\n");
                start = 0;
        }

bail:
        printf("adnsd_mgmt_func: exit\n");
        return NULL;
}



void adnsd_mgmt_init(void)
{
	pthread_t mgmtid;

	pthread_mutex_init(&upload_mgmt_adnsd_lock, NULL);
	pthread_create(&mgmtid, NULL, adnsd_mgmt_func, NULL);

	return;
}
