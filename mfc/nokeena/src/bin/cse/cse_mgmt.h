#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <signal.h>
#include "signal_utils.h"
#include "md_client.h"
#include "mdc_wrapper.h"
#include "random.h"
#include "libevent_wrapper.h"
#include "nkn_defs.h"
#include "nkn_mgmt_log.h"
#include "libnknmgmt.h"


int cse_init_mgmt(void);

struct mgmt_client_details {
    char program_name[256];
    lc_component_id log_id;
    lew_event_handler signal_handler;
    lew_event *event_signals[16];
    int signals_handled[16];
    uint8_t num_signals;
    fdic_event_callback_func sess_close_func;
    bn_msg_request_callback_func sess_event_func;
    bn_msg_request_callback_func sess_action_func;
    bn_msg_request_callback_func sess_query_func;
    bn_msg_request_callback_func sess_request_func;
    const char *mwp_gcl_client;

    /* TODO:following variables are return data */
    md_client_context *mgmt_client_mcc;
    lew_context *mgmt_client_lwc;
};

int mgmt_client_lib_init(struct mgmt_client_details *client);
int cse_mgmt_signal_handler(int fd, short event_type, void *data,
                                 lew_context *ctxt);
int cse_mgmt_handle_action(gcl_session *sess, bn_request **inout_request, 
	void *arg );
int cse_mgmt_handle_request(gcl_session *sess, bn_request **inout_request, 
	void *arg );
int cse_mgmt_handle_event(gcl_session *sess, bn_request **inout_request, 
	void *arg );
int cse_mgmt_handle_close(int fd, fdic_callback_type cbt,
	void *vsess, void *gsec_arg );
int nkn_mdc_client_init(struct mgmt_client_details *client);
int cse_mgmt_initiate_exit(void);
int nkn_cse_cfg_query(void);
int cse_mgmt_deinit(void);
int cse_exit_cleanup(void);
int nkn_cse_module_cfg_handle_change (bn_binding_array *old_bindings,
					     bn_binding_array *new_bindings);

int cse_mgmt_start_thread(void);
int cse_mgmt_thread_init(void);
int
nkn_cse_mon_handle_get(const char *bname, const tstr_array *bname_parts,
                  void *data, bn_binding_array *resp_bindings);
int
nkn_cse_mon_handle_iterate(const char *bname, const tstr_array *bname_parts,
			void *data, tstr_array *resp_names);
