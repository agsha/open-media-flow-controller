#include "md_client.h"
#include <string.h>
#include <strings.h>
#include <assert.h>
#include "mdc_backlog.h"
#include "libevent_wrapper.h"
#include "mdc_wrapper.h"
#include "random.h"
#include "nkn_mgmt_defs.h"

#ifndef UNUSED_ARGUMENT
#define UNUSED_ARGUMENT(x)		(void)x; 
#endif

typedef int (*cfg_handle_change_func) (const bn_binding_array *arr, uint32 idx,
		bn_binding *binding, void *data);
// Should use the above callback later

//typedef int (cfg_query_func) (const bn_binding_array *arr);

typedef struct mgmt_query_data_st {
	const char *query_nd;
	cfg_handle_change_func chg_func;
} mgmt_query_data_t;

typedef struct tag_mgmt_client_req_cb {
    lew_event_handler mgmt_signal_func;
    fdic_event_callback_func sess_close_func;
    bn_msg_request_callback_func sess_event_func;
    bn_msg_request_callback_func sess_action_func;
    bn_msg_request_callback_func sess_query_func;
    bn_msg_request_callback_func sess_request_func;
} mgmt_client_req_cb_t;

typedef enum tag_mgmt_event_register_type {
    MGMT_DB_CHANGE,
    MGMT_EVENT_NODE
} mgmt_event_register_type_t;

typedef struct tag_mgmt_node_register_info {
    const char *node;
    mgmt_event_register_type_t ev_type;
} mgmt_node_register_info_t;

typedef struct mgmt_client_obj_st {
    char program_name[256];
    lc_component_id log_id;
    lew_event *event_signals[16];
    int signals_handled[16];
    uint8_t num_signals;

    mgmt_client_req_cb_t cb;
    const char *mwp_gcl_client;

    /* TODO:following variables are return data */
    md_client_context *mgmt_client_mcc;
    lew_context *mgmt_client_lwc;
    mdc_bl_context *blc;
}mgmt_client_obj_t;

int
libmgmt_client_init(mgmt_client_obj_t *client);

int
libmgmt_client_get_nodes(mgmt_client_obj_t *client ,mgmt_query_data_t *mgmt_query,
			    int n_nodes, bn_binding_array **bindings);

int libmgmt_client_mdc_foreach_binding_prequeried(const bn_binding_array *bindings,
                                            const char *binding_spec, const char *db_name,
                                            mdc_foreach_binding_func callback,
					    void *callback_data);

int32_t libmgmt_client_start_event_loop(mgmt_client_obj_t *mc);    

int32_t libmgmt_client_register_change_list(mgmt_client_obj_t *mc,
				    mgmt_node_register_info_t *nr,
				    int32_t num_nodes);

int32_t libmgmt_client_setup(
	    const char *prog_name,
	    const char *gcl_client_id, 
	    int32_t lc_id,
	    mgmt_client_req_cb_t *mc_req_cb,
	    const int32_t *const signal_list, 
	    uint8_t num_signals, 
	    mgmt_client_obj_t *const mc);

#if 0
    lew_event_handler signal_handler;
    fdic_event_callback_func sess_close_func;
    bn_msg_request_callback_func sess_event_func;
    bn_msg_request_callback_func sess_action_func;
    bn_msg_request_callback_func sess_query_func;
    bn_msg_request_callback_func sess_request_func;
#endif
