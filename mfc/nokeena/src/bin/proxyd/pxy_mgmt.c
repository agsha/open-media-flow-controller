/*
 * Filename :   pxy_mgmt.c
 * Date:        2011-05-02
 * Author:      Dhruva Narasimhan
 *
 * (C) Copyright 2011, Nokeena Networks, Inc.
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

#include "pxy_defs.h"
#include "pxy_mgmt.h"
#include "pxy_interface.h"
#include "nkn_cfg_params.h"

//#include "nknlogd.h"


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

static int pxy_handle_signal(
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
int pxy_mgmt_process(int init_value)
{
	int err = 0;
	uint32 i = 0;

	err = lc_log_init(program_name, NULL, LCO_none,
	    LC_COMPONENT_ID(LCI_PROXYD),
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
	    			pxy_handle_signal, NULL);
		bail_error(err);
	}


	err = pxy_mgmt_event_registeration_init();
	printf("pass %d\n",err);
	bail_error(err);

	if (init_value)
	{
		err = pxy_l4proxy_query_init();
		bail_error(err);
	}	

	pxy_http_cfg_query() ;

bail:
	if (err) // registration failed
	{
		/* Safe to call all the exits */
		pxy_mgmt_initiate_exit ();

		/* Ensure we set the flag back */
		mgmt_exiting = false;
	}

	/* Enable the flag to indicate config init is done */
	update_from_db_done = 1;

	return err;
}



///////////////////////////////////////////////////////////////////////////////
int pxy_mgmt_event_registeration_init(void)
{
        int err = 0;
        mdc_wrapper_params *mwp = NULL;

        err = mdc_wrapper_params_get_defaults(&mwp);
        bail_error(err);

        mwp->mwp_lew_context = mgmt_lew;
        mwp->mwp_gcl_client = gcl_client_proxyd;

        mwp->mwp_session_close_func = pxy_mgmt_handle_session_close;
        mwp->mwp_event_request_func = pxy_mgmt_handle_event_request;
       
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
                        "match/1/pattern", bt_string, "/nkn/l4proxy/config/**");
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
int pxy_mgmt_handle_session_close(
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
	pthread_mutex_lock (&upload_mgmt_log_lock);
        err = pxy_mgmt_initiate_exit();
	pthread_mutex_unlock (&upload_mgmt_log_lock);
        bail_error(err);

bail:
        return err;
}

///////////////////////////////////////////////////////////////////////////////
int pxy_mgmt_handle_event_request(
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

                err = pxy_config_handle_set_request(old_bindings, new_bindings);
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
pxy_l4proxy_query_init (void)
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
                        l4proxy_prefix);
        bail_error_null(err, bindings);

        err = bn_binding_array_foreach(bindings, pxy_l4proxy_cfg_handle_change,
                        &rechecked_licenses);
        bail_error(err);
bail:
        bn_binding_array_free(&bindings);
        return err;
}

int
pxy_l4proxy_cfg_handle_change(const bn_binding_array *arr, uint32 idx,
                        bn_binding *binding, void *data)
{
        int err = 0;
        const tstring *name = NULL;
        tbool *rechecked_licenses_p = data;
        tstring *str = NULL;

        bail_null(rechecked_licenses_p);

        err = bn_binding_get_name(binding, &name);
        bail_error(err);


        if (ts_equal_str(name, "/nkn/l4proxy/config/enable", false)) {
            tbool proxy_enabled = 0;

            err = bn_binding_get_tbool(binding,
                                ba_value, NULL,
                                &proxy_enabled);

            bail_error(err);
            l4proxy_enabled = proxy_enabled;

            lc_log_basic(LOG_NOTICE, "Read .../l4proxy/config/enable as : \"%d\"",
                                proxy_enabled);
	} else if (ts_equal_str(name, "/nkn/l4proxy/config/cache-fail/action", false)) {
	    uint32 cache_fail_action = 0;

	    err = bn_binding_get_uint32(binding, ba_value, 
                                        NULL, 
                                       &cache_fail_action);
	    bail_error(err);
	    /* 0 is tunnel, 1 == reject */
	    pxy_cache_fail_reject = cache_fail_action ;

	    lc_log_basic(LOG_NOTICE, "Read .../l4proxy/config/cache-fail/action as : \"%d\"",
		    cache_fail_action);
	} else if (ts_equal_str(name, "/nkn/l4proxy/config/tunnel-list/enable", false)) {
	    tbool pxy_tunn_all = 0;
                err = bn_binding_get_tbool(binding,
                                ba_value, NULL,
                                &pxy_tunn_all);

                bail_error(err);
                pxy_tunnel_all = pxy_tunn_all;

                lc_log_basic(LOG_NOTICE, "Read .../l4proxy/config/tunel-list/enable as : \"%d\"",
                                pxy_tunn_all);

#if 0 /* DWL/DBL not supported in this phase of l4proxy support */
	} else if (ts_equal_str(name, l4proxy_use_client_ip_binding, false)) {
                uint32 t_use_client_ip = 0;

                err = bn_binding_get_uint32(binding,
                                ba_value, NULL,
                                &t_use_client_ip);

                bail_error(err);
                use_client_ip = t_use_client_ip;

                //nm_lb_policy = policy;
                lc_log_basic(LOG_DEBUG, "Read .../l4proxy/use_client_ip as : \"%d\"",
                                t_use_client_ip);
	}
        else if (ts_equal_str(name, l4proxy_del_ip_binding, false)) {
                tstring * mod = NULL;
                char strmod[256];

                err = bn_binding_get_tstr(binding,
                                ba_value, bt_string, NULL,
                                &mod);

                bail_error(err);
                snprintf(strmod, 255, "%s", ts_str(mod));
		del_ip_list(strmod);

                //nm_lb_policy = policy;
                lc_log_basic(LOG_DEBUG, "Read .../l4proxy/add_white_ip as : \"%s\"",
                                strmod);
	}
	else if (ts_equal_str(name, l4proxy_add_cacheable_ip_binding, false)) {
                tstring * mod = NULL;
                char strmod[256];

                err = bn_binding_get_tstr(binding,
                                ba_value, bt_string, NULL,
                                &mod);

                bail_error(err);
                snprintf(strmod, 255, "%s", ts_str(mod));
		add_cacheable_ip_list(strmod);

                //nm_lb_policy = policy;
                lc_log_basic(LOG_DEBUG, "Read .../l4proxy/add_cacheable_ip as : \"%s\"",
                                strmod);
	}
	else if (ts_equal_str(name, l4proxy_add_block_ip_binding, false)) {
                tstring * mod = NULL;
                char strmod[256];

                err = bn_binding_get_tstr(binding,
                                ba_value, bt_string, NULL,
                                &mod);

                bail_error(err);
                snprintf(strmod, 255, "%s", ts_str(mod));
		add_block_ip_list(strmod);

                //nm_lb_policy = policy;
                lc_log_basic(LOG_DEBUG, "Read .../l4proxy/add_block_ip as : \"%s\"",
                                strmod);
#endif  /* This Section not supported now. */
        }


bail:
        ts_free(&str);
        return err;

}


int
pxy_module_cfg_handle_change(
        bn_binding_array *old_bindings,
        bn_binding_array *new_bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    /*! error log handle change callback */
    err = bn_binding_array_foreach (new_bindings, pxy_l4proxy_cfg_handle_change,
                                        &rechecked_licenses);
    bail_error(err);


bail:
    return err;
}


///////////////////////////////////////////////////////////////////////////////
int pxy_config_handle_set_request(
        bn_binding_array *old_bindings,
        bn_binding_array *new_bindings)
{
        int err = 0;
        err = pxy_module_cfg_handle_change(old_bindings, new_bindings);
        bail_error(err);
bail:
        return err;
}


///////////////////////////////////////////////////////////////////////////////


/* ------------------------------------------------------------------------- */
/*
 *	funtion : pxy_http_cfg_query()
 *	purpose : query for http config parameters
 */
int
pxy_http_cfg_query(void)
{
        int err = 0;
        bn_binding_array *bindings = NULL;
        tbool rechecked_licenses = false;
        

        lc_log_basic(LOG_DEBUG, "nvsd http module mgmtd query initializing ..");

	err = mdc_get_binding_children(mgmt_mcc,
			NULL,
			NULL,
			true,
			&bindings,
			false,
			true,
			http_config_server_port_prefix);
	bail_error_null(err, bindings);

	err = bn_binding_array_foreach(bindings, pxy_http_cfg_server_port,
			&rechecked_licenses);
	bail_error(err);

	bn_binding_array_free(&bindings);

	/* We should have read all the ports that need to be opened for listening
	 * on. Send the port list to the config module
	 */
	if( server_port_list != NULL ) {
		pxy_http_portlist_callback((char*) ts_str(server_port_list));
		ts_free(&server_port_list);
	}
	/*----------- HTTP REQ-METHOD--------------*/

	/*----------- HTTP LISTEN INTERFACES -----------*/
	err = mdc_get_binding_children(mgmt_mcc,
			NULL,
			NULL,
			true,
			&bindings,
			false,
			true,
			http_config_server_interface_prefix);
	bail_error_null(err, bindings);

	err = bn_binding_array_foreach(bindings, pxy_http_cfg_server_interface,
			&rechecked_licenses);

        bail_error(err);
        bn_binding_array_free(&bindings);

	/* We should have read all the interfaces that need to be opened for listening 
	 * on. Sent the interface list to the config module.
	 */
	if (server_interface_list != NULL) {
		pxy_http_interfacelist_callback((char*) ts_str(server_interface_list));
		ts_free(&server_interface_list);
	}

        err = mdc_get_binding_children(mgmt_mcc,
                        NULL,
                        NULL,
                        true,
                        &bindings,
                        false,
                        true,
                        http_config_prefix);
        bail_error_null(err, bindings);

        err = bn_binding_array_foreach(bindings, pxy_http_cfg_handle_change, 
                        &rechecked_licenses);
        bail_error(err);
        bn_binding_array_free(&bindings);

bail:
        bn_binding_array_free(&bindings);
	ts_free(&server_port_list);
	ts_free(&server_interface_list);
        return err;

} /* end of pxy_http_cfg_query() */


/* ------------------------------------------------------------------------- */
/*
 *	funtion : pxy_http_module_cfg_handle_change()
 *	purpose : handler for config changes for http module
 */
int
pxy_http_module_cfg_handle_change (bn_binding_array *old_bindings,
				   bn_binding_array *new_bindings)
{
	int err = 0;
	tbool	rechecked_licenses = false;

	/* Call the callbacks */
	err = bn_binding_array_foreach (new_bindings,                
                                        pxy_http_cfg_handle_change,
					&rechecked_licenses);
        bail_error(err);

bail:
	return err;

} /* end of pxy_http_module_cfg_handle_change() */


/* ------------------------------------------------------------------------- */
/*
 *	funtion : pxy_http_cfg_handle_change()
 *	purpose : handler for config changes for http module
 */
int
pxy_http_cfg_handle_change(const bn_binding_array *arr, uint32 idx,
        		bn_binding *binding, void *data)
{
        int err = 0;
        const tstring *name = NULL;
        tbool *rechecked_licenses_p = data;

	UNUSED_ARGUMENT(arr);
	UNUSED_ARGUMENT(idx);

        bail_null(rechecked_licenses_p);

        err = bn_binding_get_name(binding, &name);
        bail_error(err);
        /*-------- RATE CONTROL  ------------*/

bail:
        return err;
} /* end of pxy_http_cfg_handle_change() */


/* ------------------------------------------------------------------------- */
/*
 *	funtion : pxy_http_content_type_cfg_handle_change()
 *	purpose : handler for config changes for contenty type of http module
 */
int 
pxy_http_cfg_server_port(const bn_binding_array *arr, uint32 idx, 
			bn_binding *binding, void *data)
{
	int err = 0;
	tbool *rechecked_licenses_p = data;
	const tstring *name = NULL;
        uint16 serv_port = 0;

	UNUSED_ARGUMENT(arr);
	UNUSED_ARGUMENT(idx);

	bail_null(rechecked_licenses_p);


	err = bn_binding_get_name(binding, &name);
	bail_error(err);

	lc_log_basic (LOG_DEBUG, "Node is : %s", ts_str(name));

	if ( !bn_binding_name_pattern_match(ts_str(name), "/nkn/nvsd/http/config/server_port/*/port"))
		goto bail;

	err = bn_binding_get_uint16(binding, ba_value, NULL, &serv_port);
	bail_error(err);


	if (NULL == server_port_list)
		err = ts_new_sprintf(&server_port_list, "%d ", serv_port);
	else
		err = ts_append_sprintf(server_port_list, "%d ", serv_port);

	bail_error(err);
bail:
	return err;

} /* end of pxy_http_cfg_server_port () */

/* ------------------------------------------------------------------------- */
/*
 *	funtion : pxy_http_cfg_server_interface()
 *	purpose : handler for config changes for http interface
 */
int 
pxy_http_cfg_server_interface(const bn_binding_array *arr, uint32 idx, 
			bn_binding *binding, void *data)
{
	int err = 0;
	tbool *rechecked_licenses_p = data;
	const tstring *name = NULL;
	tstring *t_interface = NULL;

	UNUSED_ARGUMENT(arr);
	UNUSED_ARGUMENT(idx);
	bail_null(rechecked_licenses_p);


	err = bn_binding_get_name(binding, &name);
	bail_error(err);

	lc_log_basic (LOG_DEBUG, "---------------> Node is : %s", ts_str(name));

	if ( !bn_binding_name_pattern_match(ts_str(name), "/nkn/nvsd/http/config/interface/*"))
		goto bail;


	err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &t_interface);
	bail_error(err);


	if (NULL == server_interface_list)
		err = ts_new_sprintf(&server_interface_list, "%s ", ts_str(t_interface));
	else
		err = ts_append_sprintf(server_interface_list, "%s ", ts_str(t_interface));

	bail_error(err);
bail:
	ts_free(&t_interface);
	return err;

} /* end of pxy_http_cfg_server_interface () */


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




int pxy_mgmt_initiate_exit(void)
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
int pxy_deinit(void)
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


void * pxy_mgmt_func(void * arg)
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
                        err = pxy_mgmt_process(1);
                        bail_error(err);
                } else {
                        while ((err = pxy_mgmt_process(0)) != 0) {
                                pxy_deinit();
                                sleep(2);
                        }
                }

                err = main_loop();
                complain_error(err);

                err = pxy_deinit();
                complain_error(err);
                printf("mgmt connect: de-initialized\n");
                start = 0;
        }

bail:
        printf("pxy_mgmt_func: exit\n");
        return NULL;
}



void pxy_mgmt_init(void)
{
	pthread_t mgmtid;

	pthread_mutex_init(&upload_mgmt_log_lock, NULL);
	pthread_create(&mgmtid, NULL, pxy_mgmt_func, NULL);

	return;
}
