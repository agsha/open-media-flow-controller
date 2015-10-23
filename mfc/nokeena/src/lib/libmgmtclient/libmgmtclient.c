
/*
 *
 * Filename:  libmgmtclient.c
 * Date:      2012/04/17
 * Author:    Thilak Raj S
 *
 * (C) Copyright 2010-2011 Juniper Networks, Inc.
 * All rights reserved.
 *
 */

#include "libmgmtclient.h"


/* static declarations */
static int
libmgmt_client_req_complete(md_client_context *mcc, bn_request *request,
        tbool request_complete, bn_response *response, int32 idx,
        void *arg);

static int
libmgmt_client_req_start(md_client_context *mcc, bn_request *request,
        tbool request_complete, bn_response *response, int32 idx,
        void *arg);

static int
libmgmt_client_recv_query_request(gcl_session *sess,
                                bn_request **inout_request,
                                void *arg );

static int
libmgmt_client_recv_event_request(gcl_session *sess,
                                bn_request **inout_request,
                                void *arg );
static int
libmgmt_client_recv_action_request(gcl_session *sess,
                                bn_request **inout_request,
                                void *arg );
static int
libmgmt_mdc_client_init(mgmt_client_obj_t *client);




/* Public declarations */

/* Static Definitions */







/* Public Definitions */
int
libmgmt_client_init(mgmt_client_obj_t *client)
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
	     client->cb.mgmt_signal_func, NULL);
	bail_error(err);
    }

    /* register req/action/event/close callback */
    err = libmgmt_mdc_client_init(client);
    bail_error(err);

bail:
    return err;
}


static int
libmgmt_mdc_client_init(mgmt_client_obj_t *client)
{
    int err = 0;
    mdc_wrapper_params *mwp = NULL;
    md_client_context *client_mcc = NULL;
    fdic_handle *fdich = NULL;

    err = mdc_wrapper_params_get_defaults(&mwp);
    bail_error(err);

    mwp->mwp_lew_context = client->mgmt_client_lwc;
    mwp->mwp_gcl_client = client->mwp_gcl_client;

    mwp->mwp_session_close_func = client->cb.sess_close_func;

    mwp->mwp_request_start_func = libmgmt_client_req_start;
    mwp->mwp_request_start_data = client;
    mwp->mwp_request_complete_func = libmgmt_client_req_complete;
    mwp->mwp_request_complete_data = client;

    mwp->mwp_event_request_func = libmgmt_client_recv_event_request;
    mwp->mwp_event_request_data = client;
    mwp->mwp_action_request_func = libmgmt_client_recv_action_request;
    mwp->mwp_action_request_data = client;
    mwp->mwp_query_request_func = libmgmt_client_recv_query_request;
    mwp->mwp_query_request_data = client;

    err = mdc_wrapper_init(mwp, &client_mcc);
    bail_error(err);

    err = lc_random_seed_nonblocking_once();
    bail_error(err);

    err = mdc_wrapper_get_fdic_handle(client_mcc, &fdich);
    bail_error_null(err, fdich);

    err = mdc_bl_init(fdich, &client->blc);
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

static int
libmgmt_client_recv_event_request(gcl_session *sess,
                                bn_request **inout_request,
                                void *arg )
{
    int err = 0;
    tbool handled = false;
    mgmt_client_obj_t *client = (mgmt_client_obj_t*)arg;

    err = mdc_bl_backlog_or_handle_message_takeover
        (client->blc, sess, inout_request, client->cb.sess_event_func,
         NULL, &handled);
    bail_error(err);

 bail:
    return err;
}



static int
libmgmt_client_recv_query_request(gcl_session *sess,
                                bn_request **inout_request,
                                void *arg )
{
    int err = 0;
    tbool handled = false;
    mgmt_client_obj_t *client = (mgmt_client_obj_t*)arg;


    err = mdc_bl_backlog_or_handle_message_takeover
        (client->blc, sess, inout_request, client->cb.sess_request_func,
         NULL, &handled);
    bail_error(err);

 bail:
    return err;
}

static int
libmgmt_client_recv_action_request(gcl_session *sess,
                                bn_request **inout_request,
                                void *arg )
{
    int err = 0;
    tbool handled = false;
    mgmt_client_obj_t *client = (mgmt_client_obj_t*)arg;

    err = mdc_bl_backlog_or_handle_message_takeover
        (client->blc, sess, inout_request, client->cb.sess_action_func,
         NULL, &handled);
    bail_error(err);

 bail:
    return err;
}

static int
libmgmt_client_req_start(md_client_context *mcc, bn_request *request,
        tbool request_complete, bn_response *response, int32 idx,
        void *arg)
{
    int err = 0;
    mgmt_client_obj_t *client = (mgmt_client_obj_t*)arg;

    UNUSED_ARGUMENT(mcc);
    UNUSED_ARGUMENT(request);
    UNUSED_ARGUMENT(request_complete);
    UNUSED_ARGUMENT(response);
    UNUSED_ARGUMENT(idx);

    err = mdc_bl_set_ready(client->blc, false);
    bail_error(err);

bail:
    return err;
}

static int
libmgmt_client_req_complete(md_client_context *mcc, bn_request *request,
        tbool request_complete, bn_response *response, int32 idx,
        void *arg)
{
    int err = 0;
    mgmt_client_obj_t *client = (mgmt_client_obj_t*)arg;

    UNUSED_ARGUMENT(mcc);
    UNUSED_ARGUMENT(request);
    UNUSED_ARGUMENT(request_complete);
    UNUSED_ARGUMENT(response);
    UNUSED_ARGUMENT(idx);

    err = mdc_bl_set_ready(client->blc, true);
    bail_error(err);

bail:
    return err;
}

//TODO
/*
 * Get an array of nodes and the callback to be called
 * for each node,Get the binding array for all the nodes,
 * and call the corresponding callbacks.
 * Same method can be used for db change as well,
 * Make the the library user to just register the node set
 * and call back to be called for each node set.
 */
int
libmgmt_client_get_nodes(mgmt_client_obj_t *client, mgmt_query_data_t *mgmt_query, int n_nodes,
			    bn_binding_array **bindings)
{
        int err = 0;
        bn_request *req = NULL;
        int i = 0;
        bn_query_subop node_subop = bqso_iterate;
        int node_flags = bqnf_sel_iterate_subtree;
        uint32_t ret_code;
        tstring *ret_msg = NULL;

	err = bn_request_msg_create(&req,bbmt_query_request);
	bail_error(err);

	for(i = 0;i < n_nodes; i++) {
		err = bn_query_request_msg_append_str(req,
				node_subop,
				node_flags,
				mgmt_query[i].query_nd, NULL);
		bail_error(err);
	}

        err = mdc_send_mgmt_msg(client->mgmt_client_mcc, req, false,
				    bindings, &ret_code, &ret_msg);
        bail_error(err);

bail:
        return err;
}

int libmgmt_client_mdc_foreach_binding_prequeried(const bn_binding_array *bindings,
                                            const char *binding_spec, const char *db_name,
                                            mdc_foreach_binding_func callback, void *callback_data)
{
        int err = 0;
        node_name_t binding_spec_pattern = {0};
        uint32 ret_num_matches = 0;

        snprintf(binding_spec_pattern , sizeof(binding_spec_pattern), "%s/**", binding_spec);
        err = mdc_foreach_binding_prequeried(bindings, binding_spec_pattern, db_name,
                        callback,
                        callback_data, &ret_num_matches);
        bail_error(err);

bail:
    return err;
}

int32_t
libmgmt_client_register_change_list(mgmt_client_obj_t *mc,
				    mgmt_node_register_info_t *nr,
				    int32_t num_nodes)
{
    int32_t i = 0, err = 0, j = 0;
    uint32_t ret_code;
    tstring *ret_msg = NULL;
    md_client_context *mcc = NULL;
    bn_request *req = NULL, *req1 = NULL;
    char pattern[256];

    assert(mc);
    mcc = mc->mgmt_client_mcc;

    err = bn_action_request_msg_create(&req,
       "/mgmtd/events/actions/interest/add");
    bail_error_null(err, req);

    for (i = 0;i < num_nodes; i++) {
	switch(nr[i].ev_type) {
	    case MGMT_DB_CHANGE:
		if (j == 0) {
		    err = bn_action_request_msg_append_new_binding(
				       req, 0, "event_name", bt_name, 
				       mdc_event_dbchange, NULL);
		    bail_error(err);
		}
		snprintf(pattern, 256, "match/%d/pattern", j + 1);
		err = bn_action_request_msg_append_new_binding(
					   req, 0, pattern, bt_string, 
					   nr[i].node, NULL);
		bail_error(err);
		j++;
		break;
	    case MGMT_EVENT_NODE:
		err = mdc_send_action_with_bindings_str_va(
				   mcc, &ret_code, &ret_msg,
				   "/mgmtd/events/actions/interest/add", 1, 
				   "event_name", bt_name, nr[i].node);
		bail_error(err);
#if 0
		err = bn_action_request_msg_append_new_binding(
					   req, 0, "event_name", bt_name, 
					   nr[i].node, NULL);
		bail_error(err);
#endif
		break;
	    default:
		assert(0);
	}
    }

    err = mdc_send_mgmt_msg(mcc, req, false, NULL, NULL, NULL);
    bail_error(err);

 bail:
    bn_request_msg_free(&req);
    lc_log_debug(LOG_DEBUG, "error registering node list, [node=%s] "
		 ",[ev type=%d], [err=%d]", nr[i].node, nr[i].ev_type, err);
    return err;
}
				    
int32_t
libmgmt_client_start_event_loop(mgmt_client_obj_t *mc)
{
    int32_t err = 0;
    
    assert(mc);
    err = lew_dispatch(mc->mgmt_client_lwc, NULL, NULL);
    bail_error(err);

 bail:
    lc_log_debug(LOG_DEBUG, "error starting event management event "
		 "loop [err=%d]", err);
    return err;
}

int32_t
libmgmt_client_setup(const char *prog_name,
	     const char *gcl_client_id, 
	     int32_t lc_id,
	     mgmt_client_req_cb_t *mc_req_cb,
	     const int32_t *const signal_list, 
	     uint8_t num_signals, 
	     mgmt_client_obj_t *const mc)
{
    assert(mc);

    int32_t err = 0;

    /* set to zero */
    bzero(mc, sizeof(mgmt_client_obj_t));
    
    /* set the program name */
    snprintf(mc->program_name, 256, "%s", prog_name);

    /* copy the signal list */
    if (num_signals > 16) {
	err = -ENOMEM;
	goto error;
    }
    mc->num_signals = num_signals;
    memcpy(mc->signals_handled, signal_list, 
	   num_signals * sizeof(int32_t));
    
    /* set log id */
    mc->log_id = lc_id;
    
    /* set client callbacks
     * NOTE: do not change the order of the callbacks in the structure
     * definition, we are going to do a memcpy here
     */    
    memcpy(&mc->cb, mc_req_cb, 
	   sizeof(mgmt_client_req_cb_t));

    /* set gcl client it */
    mc->mwp_gcl_client = gcl_client_id;
 error:
    return err;
    
}
