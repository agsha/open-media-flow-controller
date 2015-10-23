/**
 * @file   crst_mgmt.c
 * @author Sunil Mukundan <smukundan@cmbu-build03>
 * @date   Wed Apr 18 23:31:04 2012
 * 
 * @brief  store daemon's managment interface
 * 
 * 
 */

#include "crst_mgmt.h"
#include "crst_common_defs.h"
#include "libmgmtclient.h"
#include "bnode_proto.h"
#include "md_client.h"

#define MAX_MGMT_QUERY sizeof(cr_store_mgmt_query)/sizeof(mgmt_query_data_t)

/* EXTERN */
extern const char nkn_cr_stored_config_prefix[]; 
extern const char nkn_cr_stored_pop_config_prefix[];
extern const char nkn_cr_stored_domain_config_prefix[];
extern const char nkn_cr_stored_cache_group_name_binding[];
extern const char nkn_cr_stored_cache_entity_config_prefix[];

/* GLOBALS */
const char *config_sequence[]  = {
    "/nkn/cr_dns_store/config/pop",
    "/nkn/cr_dns_store/config/cache-group",
    "/nkn/cr_dns_store/config/cache-entity",
    "/nkn/cr_dns_store/config/cr_domain",
    "/nkn/cr_dns_store/config/global",
    "/nkn/cr_dns_store/config"	/* required to create the table
				   mappings correctly. currently
				   CG to CE mappings are called
				   before the CE is created.
				   threre are other such e.g.*/
};

uint32_t num_config_sequence = 
    sizeof(config_sequence) / sizeof(config_sequence[0]);

int crst_mgmt_handle_action(gcl_session *sess, 
	   bn_request **inout_request, void *arg );
int crst_mgmt_handle_request(gcl_session *sess, 
	    bn_request **inout_request, void *arg );
int crst_mgmt_handle_event(gcl_session *sess, 
	  bn_request **inout_request, void *arg );
int crst_mgmt_handle_close(int fd, fdic_callback_type cbt,
	void *vsess, void *gsec_arg );
int crst_mgmt_handle_query(gcl_session *sess, 
	  bn_request **inout_request, void *arg );
int crst_mgmt_signal_handler(int signum, short event_type, 
			     void *data, lew_context *ctxt);
/* Static */
static int cr_stored_mgmt_get_nodes(
	    mgmt_query_data_t *mgmt_query,
	    bn_binding_array **bindings);

static int32_t crst_load_tm_startup_cfg(void);

/* Globals */
mgmt_client_obj_t g_mc;

mgmt_client_req_cb_t mc_req_hdlr = {
    .mgmt_signal_func = crst_mgmt_signal_handler,
    .sess_close_func = crst_mgmt_handle_close,
    .sess_event_func = crst_mgmt_handle_event,
    .sess_action_func = crst_mgmt_handle_action,
    .sess_query_func = crst_mgmt_handle_query,
    .sess_request_func = crst_mgmt_handle_request
};

mgmt_node_register_info_t crst_register_node_list[] = {
    {"/nkn/cr_dns_store/config/**", MGMT_DB_CHANGE},
    {"/net/interface/events/link_up", MGMT_EVENT_NODE},
    {"/net/interface/events/link_down", MGMT_EVENT_NODE},
    {"/net/interface/events/state_change", MGMT_EVENT_NODE},
};
     
mgmt_query_data_t cr_store_mgmt_query[] = {
	{"/nkn/cr_dns_store/config",NULL},
};

void *
crst_mgmt_init(void *data)
{
    int32_t err = 0;
    int32_t sig_list[] = {SIGHUP, SIGINT, SIGPIPE, SIGTERM, SIGUSR1};
    int32_t num_sig = sizeof(sig_list)/sizeof(sig_list[0]);
    int32_t num_watch_nodes = sizeof(crst_register_node_list)
	/sizeof(crst_register_node_list[0]);
    crst_cfg_t *cfg = NULL;
    
    assert(data);
    cfg = (crst_cfg_t *)data;

    /* set thread name */
    prctl(PR_SET_NAME, "crst-mgmt");

    /* setup basic params */
    err = libmgmt_client_setup("cr_stored", gcl_client_crstored,
	       LCI_CRSTORED, &mc_req_hdlr, sig_list, num_sig, &g_mc);
    bail_error(err);

    /* initialize the mgmt client */
    err = libmgmt_client_init(&g_mc);
    bail_error(err);

    /* register for events */
    err = libmgmt_client_register_change_list(&g_mc,
	      crst_register_node_list, num_watch_nodes);
    bail_error(err);
    
    err = crst_load_tm_startup_cfg();

    /* start the mgmt event loop */
    err = libmgmt_client_start_event_loop(&g_mc);
    bail_error(err);

 bail:
    return NULL;
}

int 
crst_mgmt_handle_close(int fd, fdic_callback_type cbt,
		      void *vsess, void *gsec_arg )
{
    return 0;

}

int
crst_mgmt_handle_event(gcl_session *sess, 
	      bn_request **inout_request, void *arg )
{
    int32_t err = 0;
    bn_binding_array *bindings = NULL, *new_bindings = NULL,
	*old_bindings = NULL;
    bn_msg_type msg_type = bbmt_none;
    tstring *event_name = NULL;

    bail_null(inout_request);
    bail_null(*inout_request);
    
    err = bn_request_get(*inout_request, &msg_type, NULL, false, 
			 &bindings, NULL, NULL);
    bail_require(msg_type == bbmt_event_request);
    
    err = bn_event_request_msg_get_event_name(*inout_request,
					      &event_name);
    bail_error(err);
    
    if (ts_equal_str(event_name, mdc_event_dbchange, false)) {
	err = mdc_event_config_change_notif_extract(bindings,
			    &old_bindings, &new_bindings, NULL);
	bail_error(err);

    err = nkn_cr_stored_module_cfg_handle_change(old_bindings,new_bindings);
    bail_error(err);

    }

 bail:
    if (event_name)ts_free(&event_name);
    if (bindings) bn_binding_array_free(&bindings);
    if (new_bindings) bn_binding_array_free(&new_bindings);
    if (old_bindings) bn_binding_array_free(&old_bindings);
    return 0;

}

int
crst_mgmt_handle_request(gcl_session *sess, 
		bn_request **inout_request, void *arg )
{

    return 0;
}

int
crst_mgmt_handle_action(gcl_session *sess, 
	       bn_request **inout_request, void *arg )
{
    return 0;
}

int 
crst_mgmt_handle_query(gcl_session *sess, 
	  bn_request **inout_request, void *arg )
{
	int err = 0;
	bn_binding_array *ret_bindings = NULL;

	err = cr_stored_mgmt_get_nodes(cr_store_mgmt_query, &ret_bindings);
	bail_error(err);

	nkn_cr_stored_cfg_query(ret_bindings, num_config_sequence - 1);

bail:
	bn_binding_array_free(&ret_bindings);
	return(err);
}

int 
crst_mgmt_signal_handler(int signum, short event_type, void *data,
			lew_context *ctxt)
{

    int err = 0;

    UNUSED_ARGUMENT(event_type);
    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(ctxt);

    lc_log_debug(LOG_DEBUG, "got signal %d", signum);
    switch(signum) {
	case SIGINT:
	    printf("got signal SIGINT\n");
	    exit(-1);
	case SIGHUP:
	case SIGTERM:
	    // deinit mgmt
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


/********/
static int
cr_stored_mgmt_get_nodes(mgmt_query_data_t *mgmt_query, bn_binding_array **bindings)
{
	int err = 0;
	bn_request *req = NULL;
	uint32_t i = 0;
	bn_query_subop node_subop = bqso_iterate;
	int node_flags = bqnf_sel_iterate_subtree;
	uint32_t ret_code;
	tstring *ret_msg = NULL;

	err = bn_request_msg_create(&req,bbmt_query_request);
	bail_error(err);

	for(i = 0;i < MAX_MGMT_QUERY; i++) {
		err = bn_query_request_msg_append_str(req,
				node_subop,
				node_flags,
				mgmt_query[i].query_nd, NULL);
		bail_error(err);
	}

	err = mdc_send_mgmt_msg(g_mc.mgmt_client_mcc, req, false, bindings, &ret_code, &ret_msg);
	bail_error(err);

bail:
	bn_request_msg_free(&req);
	ts_free(&ret_msg);
	return err;
}

static int32_t 
crst_load_tm_startup_cfg(void)
{
    int err = 0;
    bn_binding_array *bindings = NULL;
    tbool rechecked_licenses = false;
    uint32_t i;

    lc_log_debug(LOG_DEBUG,
		 "CRST Loading Startup Config ..");

    for (i = 0; i < num_config_sequence; i++) {
	err = mdc_get_binding_children(g_mc.mgmt_client_mcc,
				       NULL,
				       NULL,
				       true,
				       &bindings,
				       false,
				       true,
				       config_sequence[i]);
	bail_error_null(err, bindings);
	err = nkn_cr_stored_cfg_query(bindings, i); 
	bail_error(err);
    }

 bail:
    bn_binding_array_free(&bindings);
    return err;
}
