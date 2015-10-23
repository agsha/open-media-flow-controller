#include "cse_mgmt.h"

/* following variables are defined as global to make them accessible */
lew_context *mgmt_client_lwc = NULL;
tbool mgmt_client_exiting = false;
md_client_context *cse_mcc;

/* following structure is defined to make development of new daemon to be fast */
struct mgmt_client_details cse_client;
/* external variables */
/* log-level from libmgmtlog */
extern int jnpr_log_level;


int
mgmt_client_lib_init(struct mgmt_client_details *client)
{
    int err = 0, i = 0;
    lew_context *client_lwc = NULL; 

    bail_require(client);

    err = lc_log_init(client->program_name, NULL, LCO_none, 
                      LC_COMPONENT_ID(client->log_id),
                      LOG_DEBUG, LOG_LOCAL4, LCT_SYSLOG);
    bail_error(err);

    err = lc_init();
    bail_error(err);

    /* create lew  */
    err = lew_init(&client_lwc);
    bail_error(err);

    client->mgmt_client_lwc = client_lwc;

    /* register signals */
    /*
     * Register to hear about all the signals we care about.
     * These are automatically persistent, which is fine.
     */
    for (i = 0; i < client->num_signals; ++i) {
	lc_log_debug(LOG_DEBUG, "registering sig %d", client->signals_handled[i]);
	err = lew_event_reg_signal(client_lwc, &(client->event_signals[i]),
		client->signals_handled[i],
	     client->signal_handler, NULL);
	bail_error(err);
    }

    /* register req/action/event/close callback */
    err = nkn_mdc_client_init(client);
    bail_error(err);

bail:
    return err;
}

int
nkn_mdc_client_init(struct mgmt_client_details *client)
{
    int err = 0;
    mdc_wrapper_params *mwp = NULL;
    md_client_context *client_mcc = NULL;
    
    err = mdc_wrapper_params_get_defaults(&mwp);
    bail_error(err);
    
    mwp->mwp_lew_context = client->mgmt_client_lwc;
    mwp->mwp_gcl_client = client->mwp_gcl_client;

    mwp->mwp_session_close_func = client->sess_close_func;
    mwp->mwp_event_request_func = client->sess_event_func;
    mwp->mwp_action_request_func = client->sess_action_func;
    mwp->mwp_query_request_func = client->sess_request_func;

    
    err = mdc_wrapper_init(mwp, &client_mcc);
    bail_error(err);

    err = lc_random_seed_nonblocking_once();
    bail_error(err);

    client->mgmt_client_mcc = client_mcc;

    lc_log_debug(LOG_DEBUG, "initialzed mgmtd conx");
bail:
    mdc_wrapper_params_free(&mwp);
    if (err) {
	lc_log_debug(LOG_ERR,
		_("Unable to connect to the management daemon"));
    }

    return(err);

}

int 
cse_init_mgmt(void)
{
    int err = 0;


    bzero(&cse_client, sizeof(cse_client));

    snprintf(cse_client.program_name, sizeof(cse_client.program_name), 
	    "crawler");

    /* TODO: following way is not good */
    cse_client.signals_handled[0] = SIGHUP;
    cse_client.signals_handled[1] = SIGINT;
    cse_client.signals_handled[2] = SIGPIPE;
    cse_client.signals_handled[3] = SIGTERM;
    cse_client.signals_handled[4] = SIGUSR1;
    cse_client.num_signals = 5;
    cse_client.signal_handler = cse_mgmt_signal_handler;

    cse_client.log_id = LCI_CSED;
    cse_client.sess_close_func = cse_mgmt_handle_close;
    cse_client.sess_event_func = cse_mgmt_handle_event;
    cse_client.sess_action_func = cse_mgmt_handle_action;
    cse_client.sess_request_func = cse_mgmt_handle_request;
    cse_client.mwp_gcl_client = gcl_client_csed;

    err = mgmt_client_lib_init(&cse_client);
    bail_error(err);

    mgmt_client_lwc = cse_client.mgmt_client_lwc;
    cse_mcc = cse_client.mgmt_client_mcc;

    /* TODO : following should be done in generic manner */
    /* register intested events */
    err = mdc_send_action_with_bindings_str_va
        (cse_mcc, NULL, NULL, "/mgmtd/events/actions/interest/add", 
	 2,
         "event_name", bt_name, mdc_event_dbchange
	 ,"match/1/pattern", bt_string, "/nkn/crawler/config/**"
	 ,"match/2/pattern", bt_string, "/nkn/debug/log/all/level"
	);
    bail_error(err);

    err = nkn_cse_cfg_query();
    bail_error(err);

    err =  lew_dispatch(mgmt_client_lwc, NULL, NULL);
    bail_error(err);

    err = cse_mgmt_deinit();
    bail_error(err);

bail:
    return err;
}


int 
cse_mgmt_deinit(void)
{
    int err = 0;

    err = mdc_wrapper_deinit(&cse_client.mgmt_client_mcc);
    bail_error(err);

    err = lew_deinit(&cse_client.mgmt_client_lwc);
    bail_error(err);

    err = cse_exit_cleanup();
    bail_error(err);

bail:
    lc_log_basic(LOG_NOTICE, "csed daemon exiting with code %d", err);
    lc_log_close();
    return(err);
}

int
cse_mgmt_initiate_exit()
{
    int err = 0, i = 0;

    mgmt_client_exiting = true;

    lc_log_debug(LOG_DEBUG, "EXITING");
    for (i = 0; i < cse_client.num_signals; ++i) {
        err = lew_event_delete(cse_client.mgmt_client_lwc,
		&(cse_client.event_signals[i]));
        bail_error(err);
    }

    err = mdc_wrapper_disconnect(cse_client.mgmt_client_mcc, true);
    bail_error(err);

    err = lew_escape_from_dispatch(cse_client.mgmt_client_lwc, true);
    bail_error(err);

bail:
    return err;

}
int 
cse_mgmt_signal_handler(int signum, short event_type, void *data,
                                 lew_context *ctxt)
{
    int err = 0;

    UNUSED_ARGUMENT(event_type);
    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(ctxt);

    lc_log_debug(LOG_DEBUG, "got signal %d", signum);
    switch(signum) {
	case SIGHUP:
	case SIGINT:
	case SIGTERM:
	    cse_mgmt_initiate_exit();
	    break;
	case SIGPIPE:
	    /* ignored */
	    break;
	case SIGUSR1:
	    /* ignored */
	    break;
	default:
	    break;
    }

    return err;
}

int 
cse_mgmt_handle_close(int fd, fdic_callback_type cbt,
				void *vsess, void *gsec_arg )
{
    int err = 0;
    UNUSED_ARGUMENT(fd);
    UNUSED_ARGUMENT(cbt);
    UNUSED_ARGUMENT(vsess);
    UNUSED_ARGUMENT(gsec_arg);

    lc_log_debug(LOG_DEBUG, "closing connection");
    if (mgmt_client_exiting) {
	goto bail;
    }
    err = cse_mgmt_initiate_exit();
    bail_error(err);

bail:
    return err;
}
int
cse_mgmt_handle_event(gcl_session *sess, bn_request **inout_request, 
				void *arg )
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

    err = bn_request_get_takeover(*inout_request, &msg_type, NULL,
	    false, &bindings, NULL, NULL);
    bail_error(err);

    bail_require(msg_type == bbmt_event_request);

    err = bn_event_request_msg_get_event_name(*inout_request, &event_name);
    bail_error_null(err, event_name);

    lc_log_basic(LOG_DEBUG, "Received event: %s", ts_str(event_name));

    if (ts_equal_str(event_name, mdc_event_dbchange, false)) {
	err = mdc_event_config_change_notif_extract(bindings, &old_bindings, &new_bindings, NULL);
	bail_error(err);
	bn_binding_array_dump("NEW-BINDINGS", new_bindings, LOG_DEBUG);
	bn_binding_array_dump("OLD-BINDINGS", old_bindings, LOG_DEBUG);
	err = nkn_cse_module_cfg_handle_change(old_bindings,new_bindings);
	bail_error(err);
    }

bail:
    bn_binding_array_free(&bindings);
    bn_binding_array_free(&old_bindings);
    bn_binding_array_free(&new_bindings);
    ts_free(&event_name);
    return err;
}



int
cse_mgmt_handle_request(gcl_session *sess, bn_request **inout_request, 
				void *arg )
{
  int err = 0;

  UNUSED_ARGUMENT(arg);

  bail_null(*inout_request);

  lc_log_debug(LOG_DEBUG, "handling request");

  err = mdc_dispatch_mon_request(sess, *inout_request,
		  nkn_cse_mon_handle_get,
	  NULL, nkn_cse_mon_handle_iterate, NULL);
  bail_error(err);

bail:
    return err;
}

int
cse_mgmt_handle_action(gcl_session *sess, bn_request **inout_request, 
				void *arg )
{
    int err = 0;
    UNUSED_ARGUMENT(sess);
    UNUSED_ARGUMENT(inout_request);
    UNUSED_ARGUMENT(arg);

    lc_log_debug(LOG_DEBUG, "handling action");
    goto bail;
bail:
    return err;
}
