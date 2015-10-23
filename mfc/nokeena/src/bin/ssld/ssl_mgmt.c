#include <ctype.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>
#include <limits.h>

#include "md_client.h"
#include "mdc_wrapper.h"
#include "bnode.h"
#include "signal_utils.h"
#include "libevent_wrapper.h"
#include "random.h"
#include "dso.h"

#include "ssl_defs.h"
#include "ssld_mgmt.h"
#include "nkn_mgmt_defs.h"

///////////////////////////////////////////////////////////////////////////////
int update_from_db_done = 0;
int ssl_client_auth = 0;
int ssl_server_auth = 0;
int ssl_license_enable = 0;
char *ssl_dhfile = NULL;
char *ssl_ciphers = NULL;
const char *program_name = "ssld";
lew_context *ssld_mgmt_lew = NULL;
md_client_context *mgmt_mcc = NULL;
tbool mgmt_exiting = false;
int ssl_idle_timeout = SSL_DEFAULT_IDLE_TIMEOUT;
//Thilak - is the below needed
int mgmt_init_done = 0;
tbool ssld_client_auth_enabled;
tbool ssld_server_auth_enabled;
static pthread_mutex_t upload_mgmt_log_lock;

/* Local Function Prototypes */
ssl_cert_node_data_t * ssld_mgmt_get_cert(const char *cp_name);
ssl_key_node_data_t * ssld_mgmt_get_key(const char *cp_name);

static const char ssl_cfg_nds_prefix[] = "/nkn/ssld/config";
static const char ssl_cert_prefix[] = "/nkn/ssld/config/certificate";
static const char ssl_defaults[] = "/nkn/ssld/config/default";
static const char ssl_key_prefix[] = "/nkn/ssld/config/key";
static const char ssl_client_auth_prefix[] = "/nkn/ssld/config/client-req/client_auth";
static const char ssl_client_req_prefix[] = "/nkn/ssld/config/client-req";
static const char ssl_origin_req_prefix[] = "/nkn/ssld/config/origin-req";
static const char ssl_server_auth_prefix[] = "/nkn/ssld/config/origin-req/server_auth";
static const char ssl_cipher_list[] = "/nkn/ssld/config/origin-req/cipher";
static const char ssl_vhost_prefix[] = "/nkn/ssld/config/virtual_host";
static const char ssl_delivery_prefix[] = "/nkn/ssld/delivery/config";
static const char http_config_server_port_prefix[]   = "/nkn/nvsd/http/config/server_port";
static const char ssl_license_status[] = "/nkn/mfd/licensing/config/ssl_licensed";
static const char ssl_license_active[] = "/license/feature/SSL/status/active";
static const char ssl_network_threads[] = "/nkn/ssld/config/network/threads";
static const char network_config_prefix[] = "/nkn/nvsd/network/config";
static const char network_idle_timeout[] = "/nkn/nvsd/network/config/timeout";
static const char conn_pool_config_prefix[] = "/nkn/ssld/delivery/config/conn-pool/origin";
static const char conn_pool_origin_enable_binding[] = "/nkn/ssld/delivery/config/conn-pool/origin/enabled";
static const char conn_pool_origin_timeout_binding[] = "/nkn/ssld/delivery/config/conn-pool/origin/timeout";
static const char conn_pool_origin_max_conn_binding[] = "/nkn/ssld/delivery/config/conn-pool/origin/max-conn";

tstring *http_server_port_list = NULL;
tstring *server_port_list = NULL;
tstring *server_interface_list = NULL;
//tstring *req_method_list = NULL;

ssl_cert_node_data_t lstSSLCerts[NKN_MAX_SSL_CERTS];
ssl_key_node_data_t lstSSLKeys[NKN_MAX_SSL_KEYS];
ssl_vhost_node_data_t lstSSLVhost[NKN_MAX_SSL_HOST];

///////////////////////////////////////////////////////////////////////////////
extern int http_server_port;
extern int NM_tot_threads;
extern int nkn_cfg_tot_socket_in_pool;
extern int cp_enable;
extern int cp_idle_timeout;
///////////////////////////////////////////////////////////////////////////////
int ssld_mgmt_event_registeration_init(void);
int ssld_mgmt_handle_event_request(gcl_session *session,
		    bn_request  **inout_request, void *arg);
int ssld_mgmt_handle_session_close(int fd,
		    fdic_callback_type cbt, void *vsess, void *gsec_arg);
int ssld_config_handle_set_request(bn_binding_array *old_bindings, bn_binding_array *new_bindings);
int ssld_mgmt_module_cfg_handle_change(bn_binding_array *old_bindings, bn_binding_array *new_bindings);
int ssld_deinit(void);
int ssld_mgmt_init(int init_value);
void ssld_mgmt_thrd_init(void);
int ssld_mgmt_initiate_exit(void);

void *ssld_mgmt_thrd(void *arg);

static int ssld_mgmt_handle_action_request(gcl_session *sess,
                                bn_request **inout_action_request,
                                void *arg); 

int ssld_key_cfg_handle_change(const bn_binding_array *arr,
			uint32 idx, bn_binding *binding, void *data);
int ssld_key_delete_cfg_handle_change(const bn_binding_array *arr,
			    uint32 idx, bn_binding *binding, void *data);

//int ssld_default_cfg_handle_change(const bn_binding_array *arr,
//			uint32 idx, bn_binding *binding, void *data);

int ssld_cert_cfg_handle_change(const bn_binding_array *arr,
		    uint32 idx, bn_binding *binding, void *data);
int ssld_cert_delete_cfg_handle_change(const bn_binding_array *arr,
		    uint32 idx, bn_binding *binding, void *data);

int ssld_vhost_cfg_handle_change(const bn_binding_array *arr,
		    uint32 idx, bn_binding *binding, void *data);
int ssld_delivery_cfg_handle_change(const bn_binding_array *arr,
			uint32 idx, bn_binding *binding, void *data);
int ssld_vhost_delete_cfg_handle_change(const bn_binding_array *arr,
		    uint32 idx, bn_binding *binding, void *data);
int nvsd_ssl_cfg_server_port(const bn_binding_array *arr,
		    uint32 idx, bn_binding *binding, void *data);
int
nvsd_http_cfg_server_port(const bn_binding_array *arr, uint32 idx,
                        bn_binding *binding, void *data);
int nvsd_ssl_cfg_server_interface(const bn_binding_array *arr,
		    uint32 idx, bn_binding *binding, void *data);

int ssld_client_auth_cfg_handle_change(const bn_binding_array *arr,
		    uint32 idx, bn_binding *binding, void *data);
int ssld_origin_req_cfg_handle_change(const bn_binding_array *arr,
		    uint32 idx, bn_binding *binding, void *data);
#if 0
int ssld_origin_cipher_cfg_handle_change(const bn_binding_array *arr,
		    uint32 idx, bn_binding *binding, void *data);
#endif
int ssld_license_cfg_handle_change(const bn_binding_array *arr,
		    uint32 idx, bn_binding *binding, void *data);
int ssld_network_cfg_handle_change(const bn_binding_array *arr,
		    uint32 idx, bn_binding *binding, void *data);
int ssld_conn_pool_cfg_handle_change(const bn_binding_array *arr,
		    uint32 idx, bn_binding *binding, void *data);
static ssl_cert_node_data_t * get_ssl_cert_element(const char *cpsslcert);
int vhost_ctx_cert_reinit(ssl_cert_node_data_t *p_cert_data);
int vhost_ctx_key_reinit(ssl_key_node_data_t *p_key_data);

void ssl_license_cfg(tbool t_license);
void ssl_network_thread_cfg(tstring *t_nw_threads); 
/* Is the below needed */
int log_basic(const char *fmt, ...)
		__attribute__ ((format (printf, 1, 2)));
int log_debug(const char *fmt, ...)
		__attribute__ ((format (printf, 1, 2)));

int ssld_cfg_query(void);
static int ssld_mgmt_main_loop(void);

/* List of signals server can handle */
static const int signals_handled[] = {
            SIGHUP, SIGINT, SIGPIPE, SIGTERM, SIGUSR1
};

#define num_signals_handled (int)(sizeof(signals_handled) / sizeof(int))

/* Libevent handles for server */
static lew_event *event_signals[num_signals_handled];

extern void catcher(int sig);
extern void ssl_reinit_ssl_ctx_client_auth(void);
extern void nkn_ssl_portlist_callback(char * ssl_port_list_str);
extern void ssl_vhost_ssl_ctx_init(ssl_vhost_node_data_t *vhost);
extern void ssl_vhost_ssl_ctx_destroy(ssl_vhost_node_data_t *vhost);
extern void ssl_interface_init(void);
extern void ssl_interface_deinit(void);
///////////////////////////////////////////////////////////////////////////////
static int ssld_mgmt_main_loop(void)
{
	int err = 0;
	lc_log_debug(LOG_DEBUG, _("ssld_mgmt_main_loop () - before lew_dispatch"));

	err = lew_dispatch (ssld_mgmt_lew, NULL, NULL);
	bail_error(err);
bail:
	return err;
}


static int handle_signal(int signum,
        short ev_type, void *data, lew_context *ctxt)
{
    (void) ev_type;
    (void) data;
    (void) ctxt;

    catcher(signum);
    return 0;
}

///////////////////////////////////////////////////////////////////////////////
int ssld_mgmt_init(int init_value)
{
	int err = 0;
	uint32 i = 0;

	/* Thilak - Using acceslog name here is wrong */
	err = lc_log_init(program_name, NULL, LCO_none,
	    LC_COMPONENT_ID(LCI_SSLD),
	    LOG_DEBUG, LOG_LOCAL4, LCT_SYSLOG);
	bail_error(err);

	err = lew_init(&ssld_mgmt_lew);
	bail_error(err);

	/*
	* Register to hear about all the signals we care about.
	* These are automatically persistent, which is fine.
	*/
	for (i = 0; i < (int) num_signals_handled; ++i) {
		err = lew_event_reg_signal
	    			(ssld_mgmt_lew, &(event_signals[i]), signals_handled[i],
	    			handle_signal, NULL);
		bail_error(err);
	}

	err = ssld_mgmt_event_registeration_init();
	printf("pass %d\n",err);
	bail_error(err);

	err = ssld_cfg_query();
	bail_error(err);

bail:
	if (err) {// registration failed
		/* Safe to call all the exits */
		ssld_mgmt_initiate_exit ();

		/* Ensure we set the flag back */
		mgmt_exiting = false;
	}

	if(NM_tot_threads <= 0 )
               NM_tot_threads = 2;


	/* Enable the flag to indicate config init is done */
	update_from_db_done = 1;

	return err;
}

///////////////////////////////////////////////////////////////////////////////
int ssld_mgmt_event_registeration_init(void)
{
        int err = 0;
        mdc_wrapper_params *mwp = NULL;

        err = mdc_wrapper_params_get_defaults(&mwp);
        bail_error(err);

        mwp->mwp_lew_context = ssld_mgmt_lew;
        mwp->mwp_gcl_client = gcl_client_ssld;

        mwp->mwp_session_close_func = ssld_mgmt_handle_session_close;
        mwp->mwp_event_request_func = ssld_mgmt_handle_event_request;
        mwp->mwp_action_request_func = ssld_mgmt_handle_action_request;

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
                        6,
                        "event_name", bt_name, mdc_event_dbchange_cleartext,
                        "match/1/pattern", bt_string, "/nkn/ssld/config/**",
			"match/1/pattern", bt_string, "/nkn/ssld/delivery/config/**",
			"match/1/pattern", bt_string, "/nkn/mfd/licensing/config/ssl_licensed",
			"match/1/pattern", bt_string, "/nkn/nvsd/network/config/**",
			"match/1/pattern", bt_string, "/nkn/ssld/delivery/config/conn-pool/origin/**");
        bail_error(err);

	err = mdc_send_action_with_bindings_str_va(
                        mgmt_mcc,
                        NULL,
                        NULL,
                        "/mgmtd/events/actions/interest/add",
			1,
			"event_name", bt_name, "/license/events/key_state_change");
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
int ssld_mgmt_handle_session_close(int fd,
        fdic_callback_type cbt, void *vsess, void *gsec_arg)
{
        int err = 0;
        gcl_session *gsec_session = vsess;

        if (mgmt_exiting) {
                goto bail;
        }

        lc_log_basic(LOG_ERR, _("Lost connection to mgmtd; exiting"));
	pthread_mutex_lock (&upload_mgmt_log_lock);
        err = ssld_mgmt_initiate_exit();
	pthread_mutex_unlock (&upload_mgmt_log_lock);
        bail_error(err);

bail:
        return err;
}

///////////////////////////////////////////////////////////////////////////////
int ssld_mgmt_handle_event_request(gcl_session *session,
		    bn_request **inout_request, void *arg)
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

        if (ts_equal_str(event_name, mdc_event_dbchange_cleartext, false)) {
                err = mdc_event_config_change_notif_extract(
                                bindings,
                                &old_bindings,
                                &new_bindings,
                                NULL);
                bail_error(err);

                err = ssld_config_handle_set_request(old_bindings, new_bindings);
                bail_error(err);
	} else if(ts_equal_str(event_name, "/license/events/key_state_change", false)) {
		tbool found = FALSE, t_active = FALSE;
		err = mdc_get_binding_tbool(mgmt_mcc, NULL, NULL, 
				&found, &t_active, 
				"/license/feature/SSL/status/active", NULL);

		bail_error(err);
		if(found) {
			ssl_license_cfg(t_active);
		}

		
	}else {
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

static int ssld_mgmt_handle_action_request(gcl_session *sess,
                                bn_request **inout_action_request,
                                void *arg)
{
    int err = 0;
    bn_msg_type req_type = bbmt_none;
    bn_binding_array *bindings = NULL;
    tstring *action_name = NULL;

    UNUSED_ARGUMENT(arg);

    bail_null(*inout_action_request);

    err = bn_request_get
        (*inout_action_request, &req_type, NULL, true, &bindings, NULL, NULL);
    bail_error(err);
    bail_require(req_type == bbmt_action_request);

    err = bn_action_request_msg_get_action_name(*inout_action_request,
                                                &action_name);
    bail_error(err);

    lc_log_basic(LOG_INFO, "Received action %s", ts_str(action_name));

    if (ts_equal_str(action_name, "/stat/nkn/actions/reset-https-counter", false)) {

            err = bn_response_msg_create_send(sess,
                            bbmt_action_response,
                            bn_request_get_msg_id(*inout_action_request),
                            err, "Request to reset https counter sent", 0);
            bail_error(err);
    }
    else {
        lc_log_basic(LOG_DEBUG, I_("Got unexpected action %s"),
                     ts_str(action_name));
        err = bn_response_msg_create_send
            (sess, bbmt_action_response, bn_request_get_msg_id(*inout_action_request),
             1, _("error : action not yet supported"), 0);
        goto bail;
    }
bail:
    ts_free(&action_name);
    bn_binding_array_free(&bindings);
    return err;
}

int ssld_cfg_query(void)
{
	int err = 0;
	bn_binding_array *bindings = NULL;
	tbool rechecked_licenses = false;
	tstring *t_str = NULL;
	ssl_cert_node_data_t *psslcert = NULL;

	memset(lstSSLCerts, 0, sizeof(lstSSLCerts));
	memset(lstSSLVhost, 0 , sizeof(lstSSLVhost));
	memset(lstSSLKeys, 0, sizeof(lstSSLKeys));

#if 0
	/* Read the defualt ssl settings */
        lc_log_basic(LOG_DEBUG, "ssld cfg query initializing ..");
        err = mdc_get_binding_children_ex(mgmt_mcc,
                        NULL,
                        NULL,
                        true,
                        &bindings,
			bqnf_sel_iterate_subtree,
			ssl_defaults,
			NULL, 0, NULL);
	bail_error_null(err, bindings);

	err = bn_binding_array_foreach(bindings, ssld_default_cfg_handle_change,
			&rechecked_licenses);

	bn_binding_array_free(&bindings);
	bail_error(err);
#endif
	/* SSL license Check */
	tbool t_license;
        err = mdc_get_binding_tbool(mgmt_mcc,
                        NULL,
                        NULL,
                        NULL,
                        &t_license,
			ssl_license_status,
			NULL);
	bail_error(err);
	ssl_license_cfg(t_license);

	tstring *t_nw_threads = NULL;
        err = mdc_get_binding_tstr(mgmt_mcc,
                        NULL,
                        NULL,
                        NULL,
                        &t_nw_threads,
                       ssl_network_threads,
                       NULL);
        bail_error(err);
        ssl_network_thread_cfg(t_nw_threads);
	
	/* read nvsd config settingd like idle timeout */
	err = mdc_get_binding_children_ex(mgmt_mcc,
			NULL,
			NULL,
			true,
			&bindings,
			bqnf_sel_iterate_subtree | bqnf_cleartext,
			network_config_prefix,
			NULL, 0, NULL);
			
	bail_error_null(err, bindings);

	err = bn_binding_array_foreach(bindings, ssld_network_cfg_handle_change,
			&rechecked_licenses);

	bn_binding_array_free(&bindings);
	bail_error(err);

	/* read nvsd connection pool settingd like idle timeout, number of connections */
	err = mdc_get_binding_children_ex(mgmt_mcc,
			NULL,
			NULL,
			true,
			&bindings,
			bqnf_sel_iterate_subtree | bqnf_cleartext,
			conn_pool_config_prefix,
			NULL, 0, NULL);
			
	bail_error_null(err, bindings);

	err = bn_binding_array_foreach(bindings, ssld_conn_pool_cfg_handle_change,
			&rechecked_licenses);

	bn_binding_array_free(&bindings);
	bail_error(err);

	/* read the cert settings */
        lc_log_basic(LOG_DEBUG, "ssld cfg query initializing ..");
        err = mdc_get_binding_children_ex(mgmt_mcc,
                        NULL,
                        NULL,
                        true,
                        &bindings,
			bqnf_sel_iterate_subtree | bqnf_cleartext,
			ssl_cert_prefix,
			NULL, 0, NULL);
	bail_error_null(err, bindings);

	err = bn_binding_array_foreach(bindings, ssld_cert_cfg_handle_change,
			&rechecked_licenses);

	bn_binding_array_free(&bindings);
	bail_error(err);

	/* read the keys config */
        err = mdc_get_binding_children_ex(mgmt_mcc,
                        NULL,
                        NULL,
                        true,
                        &bindings,
			bqnf_sel_iterate_subtree | bqnf_cleartext,
			ssl_key_prefix,
			NULL, 0, NULL);
	bail_error_null(err, bindings);

	err = bn_binding_array_foreach(bindings, ssld_key_cfg_handle_change,
			&rechecked_licenses);

	bn_binding_array_free(&bindings);
	bail_error(err);

	/* read the virtual host */
	err = mdc_get_binding_children(mgmt_mcc,
			NULL,
			NULL,
			true,
			&bindings,
			false,
			true,
			ssl_vhost_prefix);
	bail_error_null(err, bindings);

	err = bn_binding_array_foreach(bindings, ssld_vhost_cfg_handle_change,
			&rechecked_licenses);
	bail_error(err);

	/* read the delivery*/
        err = mdc_get_binding_children(mgmt_mcc,
                        NULL,
                        NULL,
                        true,
                        &bindings,
                        false,
			true,
			ssl_delivery_prefix);
	bail_error_null(err, bindings);

	err = bn_binding_array_foreach(bindings, ssld_delivery_cfg_handle_change,
			&rechecked_licenses);

	bn_binding_array_free(&bindings);
	bail_error(err);

        /* We should have read all the ports that need to be opened for listening
         * on. Send the port list to the config module
         */
        if( server_port_list != NULL ) {
                nkn_ssl_portlist_callback((char*) ts_str(server_port_list));
                ts_free(&server_port_list);
        }

	if(server_interface_list != NULL) {
		nkn_ssl_interfacelist_callback((char *)ts_str(server_interface_list));
		ts_free(&server_interface_list);
	}
	/* Client Authentication */
        err = mdc_get_binding_children(mgmt_mcc,
                        NULL,
                        NULL,
                        true,
                        &bindings,
                        false,
			true,
			ssl_client_req_prefix);
	bail_error_null(err, bindings);

	err = bn_binding_array_foreach(bindings, ssld_client_auth_cfg_handle_change,
			&rechecked_licenses);

	bn_binding_array_free(&bindings);
	bail_error(err);

	/* Server Authentication */
        err = mdc_get_binding_children(mgmt_mcc,
                        NULL,
                        NULL,
                        true,
                        &bindings,
                        false,
			true,
			ssl_origin_req_prefix);
	bail_error_null(err, bindings);

	err = bn_binding_array_foreach(bindings, ssld_origin_req_cfg_handle_change,
			&rechecked_licenses);

	bn_binding_array_free(&bindings);
	bail_error(err);
#if 0
	err = bn_binding_array_foreach(bindings, ssld_origin_cipher_cfg_handle_change,
			&rechecked_licenses);
#endif

	bn_binding_array_free(&bindings);
	bail_error(err);


	err = mdc_get_binding_children(mgmt_mcc,
                        NULL,
                        NULL,
                        true,
                        &bindings,
                        false,
                        true,
                        http_config_server_port_prefix);
        bail_error_null(err, bindings);

        err = bn_binding_array_foreach(bindings, nvsd_http_cfg_server_port,
                        &rechecked_licenses);
        bail_error(err);

        bn_binding_array_free(&bindings);

	if(http_server_port_list != NULL) {
		 ts_free(&http_server_port_list);

	}


bail:
	mgmt_init_done = 1;
        bn_binding_array_free(&bindings);
	ts_free(&server_port_list);
	ts_free(&server_interface_list);
        return err;
}

static ssl_key_node_data_t* get_ssl_key_element(const char *cpsslkey)
{
        int i = 0;
        int free_entry_index  = -1;

        for (i = 0; i < NKN_MAX_SSL_KEYS; i++) {
                if (lstSSLKeys[i].name == NULL) {
                        /* Save the first free entry */
                        if (free_entry_index == -1)
                                free_entry_index = i;

                        /* Empty entry.. hence continue */
                        continue;
                } else if (strcmp(cpsslkey , lstSSLKeys[i].name) == 0) {
                        /* Found match hence return this entry */
                        return (&(lstSSLKeys[i]));
                }
        }

        /* Now that we have gone through the list and no match
         * return the free_entry_index */
        if (free_entry_index != -1)
                return (&(lstSSLKeys [free_entry_index]));

        /* No match and no free entry */
        return (NULL);
}

static ssl_cert_node_data_t* get_ssl_cert_element(const char *cpsslcert)
{
        int i = 0;
        int free_entry_index  = -1;

        for (i = 0; i < NKN_MAX_SSL_CERTS; i++) {
                if (lstSSLCerts[i].name == NULL) {
                        /* Save the first free entry */
                        if (free_entry_index == -1)
                                free_entry_index = i;

                        /* Empty entry.. hence continue */
                        continue;
                } else if (strcmp(cpsslcert , lstSSLCerts[i].name) == 0) {
                        /* Found match hence return this entry */
                        return (&(lstSSLCerts[i]));
                }
        }

        /* Now that we have gone through the list and no match
         * return the free_entry_index */
        if (free_entry_index != -1)
                return (&(lstSSLCerts [free_entry_index]));

        /* No match and no free entry */
        return (NULL);
} /* end of get_publish_point_element () */

ssl_cert_node_data_t * ssld_mgmt_get_cert(const char *cp_name)
{
	int i;

	/* Sanity Check */
	if (cp_name == NULL) return(NULL);

	/* Now find the matching entry */
	for (i = 0; i < NKN_MAX_SSL_CERTS; i++) {
		if ((lstSSLCerts[i].name != NULL) &&
			(strcmp(cp_name, lstSSLCerts[i].name) == 0)) {
			/* Return this entry */
			return (&(lstSSLCerts[i]));
		}
	}

	/* No Match */
	return (NULL);
} /* end of ssld_mgmt_get_cert() */

ssl_key_node_data_t * ssld_mgmt_get_key(const char *cp_name)
{
	int i;

	/* Sanity Check */
	if (cp_name == NULL) return(NULL);

	/* Now find the matching entry */
	for (i = 0; i < NKN_MAX_SSL_KEYS; i++) {
		if ((lstSSLKeys[i].name != NULL) &&
			(strcmp(cp_name, lstSSLKeys[i].name) == 0)) {
			/* Return this entry */
			return (&(lstSSLKeys[i]));
		}
	}

	/* No Match */
	return (NULL);
} /* end of ssld_mgmt_get_key() */


static ssl_vhost_node_data_t * get_ssl_vhost_element(const char *cpsslhost)
{
        int i = 0;
        int free_entry_index  = -1;

        for (i = 0; i < NKN_MAX_SSL_HOST; i++) {
                if (lstSSLVhost[i].name == NULL) {
                        /* Save the first free entry */
                        if (free_entry_index == -1) free_entry_index = i;

                        /* Empty entry.. hence continue */
                        continue;
                } else if (strcmp(cpsslhost, lstSSLVhost[i].name) == 0) {
                        /* Found match hence return this entry */
                        return (&(lstSSLVhost [i]));
                }
        }

        /* Now that we have gone through the list and no match
         * return the free_entry_index */
        if (free_entry_index != -1)
                return (&( lstSSLVhost[free_entry_index]));

        /* No match and no free entry */
        return (NULL);
} /* end of get_publish_point_element () */


int vhost_ctx_key_reinit(ssl_key_node_data_t *p_key_data)
{
        int i = 0;
	if(p_key_data == NULL || p_key_data->name == NULL)
		return FALSE;

        for (i = 0; i < NKN_MAX_SSL_HOST; i++) {
                if ((lstSSLVhost[i].name != NULL) 
			&& (lstSSLVhost[i].p_ssl_key != NULL)
			&& (lstSSLVhost[i].p_ssl_key->name != NULL) 
                	&& (strcmp(lstSSLVhost[i].p_ssl_key->name , p_key_data->name) == 0 )) {
			ssl_vhost_ssl_ctx_destroy(&lstSSLVhost[i]);
			ssl_vhost_ssl_ctx_init(&lstSSLVhost[i]);
			
                }
        }

	return TRUE;
}
int vhost_ctx_cert_reinit(ssl_cert_node_data_t *p_cert_data)
{
        int i = 0;
	if(p_cert_data == NULL || p_cert_data->name == NULL)
		return FALSE;

        for (i = 0; i < NKN_MAX_SSL_HOST; i++) {
                if ((lstSSLVhost[i].name != NULL) 
			&& (lstSSLVhost[i].p_ssl_cert != NULL)
			&& (lstSSLVhost[i].p_ssl_cert->name != NULL)
                	&& (strcmp(lstSSLVhost[i].p_ssl_cert->name , p_cert_data->name) == 0 )) {
			ssl_vhost_ssl_ctx_destroy(&lstSSLVhost[i]);
			ssl_vhost_ssl_ctx_init(&lstSSLVhost[i]);
			
                }
        }

	return TRUE;
}

int ssld_vhost_delete_cfg_handle_change(const bn_binding_array *arr,
			    uint32 idx, bn_binding *binding, void *data)
{
        int err = 0;
        const tstring *name = NULL;
        const char *t_vhost_name = NULL;
        tstr_array *name_parts = NULL;
	ssl_vhost_node_data_t *p_sslvhost = NULL;
        tbool *rechecked_licenses_p = data;

        UNUSED_ARGUMENT(arr);
        UNUSED_ARGUMENT(idx);

        bail_null(rechecked_licenses_p);

        err = bn_binding_get_name(binding, &name);
        bail_error(err);

        /* Check if this is our node */
        if (bn_binding_name_pattern_match (ts_str(name), "/nkn/ssld/config/virtual_host/**")) {
                /* get the cert name */
                err = bn_binding_get_name_parts(binding, &name_parts);
                bail_error(err);

                t_vhost_name = tstr_array_get_str_quick(name_parts, 4);

                lc_log_basic(LOG_DEBUG, "Read ... as : /nkn/ssld/config/virtual_host/\"%s\"",
                                t_vhost_name);

		p_sslvhost = get_ssl_vhost_element(t_vhost_name);
                if (!p_sslvhost)
                        goto bail;

        } else {
                /* This is not a cert node.. hence bail */
                goto bail;
        }
        /* Get the value of passphrase  */
        if (bn_binding_name_pattern_match (ts_str(name), "/nkn/ssld/config/virtual_host/*")) {
		ssl_vhost_ssl_ctx_destroy(p_sslvhost);
		safe_free(p_sslvhost->name);
		if(p_sslvhost->p_ssl_cert) 
			lc_log_basic(LOG_NOTICE, "Virtual host still has a cert obj associated");

		memset(p_sslvhost, 0 ,sizeof(ssl_vhost_node_data_t));
	}

bail:
        tstr_array_free(&name_parts);

        return err;
}

int ssld_delivery_cfg_handle_change(const bn_binding_array *arr,
			uint32 idx, bn_binding *binding, void *data)

{
        int err = 0;
        const tstring *name = NULL;
        tstr_array *name_parts = NULL;
        tbool *rechecked_licenses_p = data;

        UNUSED_ARGUMENT(arr);
        UNUSED_ARGUMENT(idx);

        bail_null(rechecked_licenses_p);

        err = bn_binding_get_name(binding, &name);
        bail_error(err);

	/* Get the value of passphrase  */
	if (bn_binding_name_pattern_match (ts_str(name), "/nkn/ssld/delivery/config/server_port/*")) {
		uint16 t_port;

		err = bn_binding_get_uint16(binding,
				ba_value, NULL,
				&t_port);
		bail_error(err);

	       if (NULL == server_port_list)
        	        err = ts_new_sprintf(&server_port_list, "%d ", t_port);
       	       else
        	        err = ts_append_sprintf(server_port_list, "%d ", t_port);

	} else if (bn_binding_name_pattern_match (ts_str(name), "/nkn/ssld/delivery/config/server_intf/*")) {
		tstring *t_intf = NULL;

		err = bn_binding_get_tstr(binding,
				ba_value, bt_string, NULL,
				&t_intf);
		bail_error(err);

 	        if (NULL == server_interface_list)
        	        err = ts_new_sprintf(&server_interface_list, "%s ", ts_str(t_intf));
       	        else
        	        err = ts_append_sprintf(server_interface_list, "%s ", ts_str(t_intf));
		ts_free(&t_intf);
	}


bail:
        return err;
}

int ssld_vhost_cfg_handle_change(const bn_binding_array *arr,
			uint32 idx, bn_binding *binding, void *data)

{
        int err = 0;
        const tstring *name = NULL;
        const char *t_vhost_name = NULL;
        tstr_array *name_parts = NULL;
	ssl_vhost_node_data_t *p_sslvhost = NULL;
        tbool *rechecked_licenses_p = data;

        UNUSED_ARGUMENT(arr);
        UNUSED_ARGUMENT(idx);

        bail_null(rechecked_licenses_p);

        err = bn_binding_get_name(binding, &name);
        bail_error(err);

        /* Check if this is our node */
        if (bn_binding_name_pattern_match (ts_str(name), "/nkn/ssld/config/virtual_host/**")) {
                /* get the cert name */
                err = bn_binding_get_name_parts(binding, &name_parts);
                bail_error(err);

                t_vhost_name = tstr_array_get_str_quick(name_parts, 4);

                lc_log_basic(LOG_DEBUG, "Read ... as : /nkn/ssld/config/virtual_host/\"%s\"",
                                t_vhost_name);

		p_sslvhost = get_ssl_vhost_element(t_vhost_name);
                if (!p_sslvhost) goto bail;

                if (p_sslvhost->name == NULL)
                        p_sslvhost->name = strdup (t_vhost_name);
        } else {
                /* This is not a cert node.. hence bail */
                goto bail;
        }
        /* Get the value of passphrase  */
	if (bn_binding_name_pattern_match (ts_str(name), "/nkn/ssld/config/virtual_host/*/cert")) {
		char *t_cert = NULL;
		const bn_binding *key_binding = NULL;
		uint32_t ret_idx = 0;
		node_name_t  vhost_key_nd = {0};

		err = bn_binding_get_str (binding,
				ba_value, bt_string, NULL,
				&t_cert);
		bail_error(err);

		lc_log_basic(LOG_DEBUG, "Read ... /nkn/ssld/config/virtual_host/%s/cert \"%s\"",
				t_vhost_name, t_cert);


		snprintf(vhost_key_nd, sizeof(vhost_key_nd), "/nkn/ssld/config/virtual_host/%s/key",
				t_vhost_name);
		err = bn_binding_array_get_first_matching_binding(arr, vhost_key_nd, 0,
				&ret_idx, &key_binding);
		bail_error(err);

		if (t_cert && strlen(t_cert) != 0 ) {
			p_sslvhost->p_ssl_cert = ssld_mgmt_get_cert(t_cert);
			if(p_sslvhost->p_ssl_key != NULL) {
				/*
				 *Re-Init the ssl context here only when certificate alone has changed,
				 */
				if(!key_binding) {
					ssl_vhost_ssl_ctx_destroy(p_sslvhost);
					ssl_vhost_ssl_ctx_init(p_sslvhost);
				}
			}
		} else {
			p_sslvhost->p_ssl_cert = NULL;
			if(!key_binding)
				ssl_vhost_ssl_ctx_destroy(p_sslvhost);
		}

		if (t_cert) safe_free(t_cert);
	} else if (bn_binding_name_pattern_match (ts_str(name), "/nkn/ssld/config/virtual_host/*/key")) {
		char *t_key = NULL;

		err = bn_binding_get_str (binding,
				ba_value, bt_string, NULL,
				&t_key);
		bail_error(err);

		lc_log_basic(LOG_DEBUG, "Read ... /nkn/ssld/config/virtual_host/%s/key \"%s\"",
				t_vhost_name, t_key);

		if (t_key && strlen(t_key) != 0 ) {
			p_sslvhost->p_ssl_key = ssld_mgmt_get_key(t_key);
			if(p_sslvhost->p_ssl_cert != NULL) {
				ssl_vhost_ssl_ctx_destroy(p_sslvhost);
				ssl_vhost_ssl_ctx_init(p_sslvhost);
			}
		} else {
			p_sslvhost->p_ssl_key = NULL;
			ssl_vhost_ssl_ctx_destroy(p_sslvhost);
		}

		if (t_key) safe_free(t_key);
	}
	else if (bn_binding_name_pattern_match (ts_str(name), "/nkn/ssld/config/virtual_host/*/cipher")) {
		char *t_cipher = NULL;

		err = bn_binding_get_str (binding,
				ba_value, bt_string, NULL,
				&t_cipher);
		bail_error(err);

		lc_log_basic(LOG_DEBUG, "Read ... /nkn/ssld/config/virtual_host/%s/cipher \"%s\"",
				t_vhost_name, t_cipher);

		if (t_cipher && strlen(t_cipher) != 0 ) {
		    //Pulgin in cipher changes here
			p_sslvhost->cipher = strdup(t_cipher);
		}
		else if(p_sslvhost->cipher) {
			free(p_sslvhost->cipher);
			p_sslvhost->cipher = NULL;
		}
		if(p_sslvhost->p_ssl_cert != NULL && p_sslvhost->p_ssl_key != NULL) {
			ssl_vhost_ssl_ctx_destroy(p_sslvhost);
			ssl_vhost_ssl_ctx_init(p_sslvhost);
		}

		if (t_cipher) safe_free(t_cipher);
	}


bail:
        tstr_array_free(&name_parts);

        return err;
}

int ssld_key_delete_cfg_handle_change(const bn_binding_array *arr,
			    uint32 idx, bn_binding *binding, void *data)
{
        int err = 0;
        const tstring *name = NULL;
        const char *t_key_name = NULL;
        tstr_array *name_parts = NULL;
	ssl_key_node_data_t *psslkey = NULL;
        tbool *rechecked_licenses_p = data;

        UNUSED_ARGUMENT(arr);
        UNUSED_ARGUMENT(idx);

        bail_null(rechecked_licenses_p);

        err = bn_binding_get_name(binding, &name);
        bail_error(err);

        /* Check if this is our node */
        if (bn_binding_name_pattern_match (ts_str(name), "/nkn/ssld/config/key/**")) {
                /* get the cert name */
                err = bn_binding_get_name_parts(binding, &name_parts);
                bail_error(err);

                t_key_name = tstr_array_get_str_quick(name_parts, 4);

                lc_log_basic(LOG_DEBUG, "Read ... as /nkn/ssld/config/key/: \"%s\"",
                                t_key_name);

		psslkey = get_ssl_key_element(t_key_name);
                if (!psslkey) goto bail;
        } else {
                /* This is not a cert node.. hence bail */
                goto bail;
        }

	if (bn_binding_name_pattern_match(ts_str(name), "/nkn/ssld/config/key/*")) {
		safe_free(psslkey->name);
		safe_free(psslkey->key_name);
		safe_free(psslkey->passphrase);
		memset(psslkey, 0 , sizeof(ssl_key_node_data_t));
	}

bail:
        tstr_array_free(&name_parts);

        return err;
}

int ssld_cert_delete_cfg_handle_change(const bn_binding_array *arr,
			    uint32 idx, bn_binding *binding, void *data)

{
        int err = 0;
        const tstring *name = NULL;
        const char *t_cert_name = NULL;
        tstr_array *name_parts = NULL;
	ssl_cert_node_data_t *psslcert = NULL;
        tbool *rechecked_licenses_p = data;

        UNUSED_ARGUMENT(arr);
        UNUSED_ARGUMENT(idx);

        bail_null(rechecked_licenses_p);

        err = bn_binding_get_name(binding, &name);
        bail_error(err);

        /* Check if this is our node */
        if (bn_binding_name_pattern_match (ts_str(name), "/nkn/ssld/config/certificate/**")) {
                /* get the cert name */
                err = bn_binding_get_name_parts(binding, &name_parts);
                bail_error(err);

                t_cert_name = tstr_array_get_str_quick(name_parts, 4);

                lc_log_basic(LOG_DEBUG, "Read ... as /nkn/ssld/config/certificate/: \"%s\"",
                                t_cert_name);

		psslcert = get_ssl_cert_element(t_cert_name);
                if (!psslcert) goto bail;
        } else {
                /* This is not a cert node.. hence bail */
                goto bail;
        }

	if (bn_binding_name_pattern_match(ts_str(name), "/nkn/ssld/config/certificate/*")) {
		safe_free(psslcert->name);
		safe_free(psslcert->cert_name);
		safe_free(psslcert->passphrase);
		memset(psslcert, 0 , sizeof(ssl_cert_node_data_t));
	}

bail:
        tstr_array_free(&name_parts);

        return err;
}
int ssld_key_cfg_handle_change(const bn_binding_array *arr,
			uint32 idx, bn_binding *binding, void *data)
{
        int err = 0;
        const tstring *name = NULL;
        const char *t_key_name = NULL;
        tstr_array *name_parts = NULL;
	ssl_key_node_data_t *psslkey = NULL;
        tbool *rechecked_licenses_p = data;

        UNUSED_ARGUMENT(arr);
        UNUSED_ARGUMENT(idx);

        bail_null(rechecked_licenses_p);

        err = bn_binding_get_name(binding, &name);
        bail_error(err);

        /* Check if this is our node */
        if (bn_binding_name_pattern_match(ts_str(name), "/nkn/ssld/config/key/**")) {
                /* get the cert name */
                err = bn_binding_get_name_parts (binding, &name_parts);
                bail_error(err);

                t_key_name = tstr_array_get_str_quick(name_parts, 4);

                lc_log_basic(LOG_DEBUG, "Read ... as /nkn/ssld/config/key/: \"%s\"",
                                t_key_name);

		psslkey = get_ssl_key_element(t_key_name);
                if (!psslkey) goto bail;

		if (psslkey->name == NULL) {
			psslkey->name = strdup(t_key_name);
			psslkey->key_name = smprintf ("%s%s",SSLD_KEY_PATH ,t_key_name);
		}
        } else {
                /* This is not a cert node.. hence bail */
                goto bail;
        }
        /* Get the value of passphrase  */
        if (bn_binding_name_pattern_match(ts_str(name), "/nkn/ssld/config/key/*/passphrase")) {
		char *t_passphrase = NULL;

		err = bn_binding_get_str (binding,
				ba_value, bt_string, NULL,
				&t_passphrase);
		bail_error(err);

		lc_log_basic(LOG_DEBUG, "Read ... /nkn/ssld/config/key/%s/passphraseas \"%s\"",
				t_key_name, t_passphrase);

		if (psslkey->passphrase != NULL) free (psslkey->passphrase);

		if (t_passphrase) {
			psslkey->passphrase = strdup(t_passphrase);
		} else {
			psslkey->passphrase = NULL;
		}

		if (t_passphrase) safe_free(t_passphrase);

	
	}else if (bn_binding_name_pattern_match (ts_str(name), "/nkn/ssld/config/key/*/time_stamp")) {
		int32_t time_stamp = 0;

		err = bn_binding_get_int32(binding,ba_value,NULL,&time_stamp);
		bail_error(err);
		vhost_ctx_key_reinit(psslkey);

	} else if (bn_binding_name_pattern_match (ts_str(name), "/nkn/ssld/config/key/*/host/*")) {
	}

bail:
	tstr_array_free(&name_parts);
	return err;
}

#if 0
int ssld_default_cfg_handle_change(const bn_binding_array *arr,
			uint32 idx, bn_binding *binding, void *data)
{
        int err = 0;
        const tstring *name = NULL;
        tstr_array *name_parts = NULL;
        tbool *rechecked_licenses_p = data;

        UNUSED_ARGUMENT(arr);
        UNUSED_ARGUMENT(idx);

        bail_null(rechecked_licenses_p);

        err = bn_binding_get_name(binding, &name);
        bail_error(err);

        /* Check if this is our node */
        if (bn_binding_name_pattern_match(ts_str(name), "/nkn/ssld/config/default/**")) {
	    //Continure further
	    /* Having this block ,as might fill it later */
	     
	} else {
                /* This is not a cert node.. hence bail */
                goto bail;
        }
	/* Get the value of passphrase  */
	if (bn_binding_name_pattern_match(ts_str(name), "/nkn/ssld/config/default/certificate")) {
		char *t_certificate = NULL;

		err = bn_binding_get_str (binding,
				ba_value, bt_string, NULL,
				&t_certificate);
		bail_error(err);

		lc_log_basic(LOG_DEBUG, "Read ... /nkn/ssld/config/default/certificate \"%s\"",
				t_certificate);

		if (t_certificate) {
		    g_ssl_global.cert = strdup(t_certificate);
		} else {
		    g_ssl_global.cert = NULL;
		}

		if (t_certificate) safe_free(t_certificate);

	} else if  (bn_binding_name_pattern_match(ts_str(name), "/nkn/ssld/config/default/key")) {
		char *t_key = NULL;

		err = bn_binding_get_str (binding,
				ba_value, bt_string, NULL,
				&t_key);
		bail_error(err);

		lc_log_basic(LOG_DEBUG, "Read ... /nkn/ssld/config/default/key \"%s\"",
				t_key);
		if (t_key) {
		    g_ssl_global.key = strdup(t_key);
		} else {
		    g_ssl_global.key = NULL;
		}

		if (t_key) safe_free(t_key);
	} else if  (bn_binding_name_pattern_match(ts_str(name), "/nkn/ssld/config/default/cipher")) {
		char *t_cipher = NULL;

		err = bn_binding_get_str (binding,
				ba_value, bt_string, NULL,
				&t_cipher);
		bail_error(err);

		lc_log_basic(LOG_DEBUG, "Read ... /nkn/ssld/config/default/cipher \"%s\"",
				t_cipher);

		if (t_cipher) {
			g_ssl_global.cipher = strdup(t_cipher);
		} else {
			g_ssl_global.cipher = NULL;
		}

		if (t_cipher) safe_free(t_cipher);
	}


	bail:
        tstr_array_free(&name_parts);

        return err;
}

#endif

int ssld_cert_cfg_handle_change(const bn_binding_array *arr,
			uint32 idx, bn_binding *binding, void *data)

{
        int err = 0;
        const tstring *name = NULL;
        const char *t_cert_name = NULL;
        tstr_array *name_parts = NULL;
	ssl_cert_node_data_t *psslcert = NULL;
        tbool *rechecked_licenses_p = data;

        UNUSED_ARGUMENT(arr);
        UNUSED_ARGUMENT(idx);

        bail_null(rechecked_licenses_p);

        err = bn_binding_get_name(binding, &name);
        bail_error(err);

        /* Check if this is our node */
        if (bn_binding_name_pattern_match(ts_str(name), "/nkn/ssld/config/certificate/**")) {
                /* get the cert name */
                err = bn_binding_get_name_parts (binding, &name_parts);
                bail_error(err);

                t_cert_name = tstr_array_get_str_quick(name_parts, 4);

                lc_log_basic(LOG_DEBUG, "Read ... as /nkn/ssld/config/certificate/: \"%s\"",
                                t_cert_name);

		psslcert = get_ssl_cert_element(t_cert_name);
                if (!psslcert) goto bail;

		if (psslcert->name == NULL) {
			psslcert->name = strdup(t_cert_name);
			psslcert->cert_name = smprintf ("%s%s",SSLD_CERT_PATH ,t_cert_name);
		}
        } else {
                /* This is not a cert node.. hence bail */
                goto bail;
        }
        /* Get the value of passphrase  */
        if (bn_binding_name_pattern_match(ts_str(name), "/nkn/ssld/config/certificate/*/passphrase")) {
		char *t_passphrase = NULL;

		err = bn_binding_get_str (binding,
				ba_value, bt_string, NULL,
				&t_passphrase);
		bail_error(err);

		lc_log_basic(LOG_DEBUG, "Read ... /nkn/ssld/config/certificate/%s/passphraseas \"%s\"",
				t_cert_name, t_passphrase);

		if (psslcert->passphrase != NULL) free (psslcert->passphrase);

		if (t_passphrase) {
			psslcert->passphrase = strdup(t_passphrase);
		} else {
			psslcert->passphrase = NULL;
		}

		if (t_passphrase) safe_free(t_passphrase);

	} else if (bn_binding_name_pattern_match (ts_str(name), "/nkn/ssld/config/certificate/*/keyfile")) {
	    /* Use later */
	} else if (bn_binding_name_pattern_match (ts_str(name), "/nkn/ssld/config/certificate/*/time_stamp")) {
		int32_t time_stamp = 0;

		err = bn_binding_get_int32(binding,ba_value,NULL,&time_stamp);
		bail_error(err);
		vhost_ctx_cert_reinit(psslcert);


	} else if (bn_binding_name_pattern_match (ts_str(name), "/nkn/ssld/config/certificate/*/host/*")) {
	} else if (bn_binding_name_pattern_match (ts_str(name), "/nkn/ssld/config/client-req/client_auth")) {
		tbool t_client_auth;

		err = bn_binding_get_tbool(binding,
				ba_value, NULL,
				&t_client_auth);
		bail_error(err);

		if (ssl_client_auth != (int)t_client_auth) {
			ssl_client_auth = t_client_auth;
			ssl_reinit_ssl_ctx_client_auth();
		}
	} 
bail:
        tstr_array_free(&name_parts);

        return err;
}

int ssld_client_auth_cfg_handle_change(const bn_binding_array *arr,
			uint32 idx, bn_binding *binding, void *data)

{
        int err = 0;
        const tstring *name = NULL;
	ssl_cert_node_data_t *psslcert = NULL;
        tbool *rechecked_licenses_p = data;

        UNUSED_ARGUMENT(arr);
        UNUSED_ARGUMENT(idx);

        bail_null(rechecked_licenses_p);

        err = bn_binding_get_name(binding, &name);
        bail_error(err);

	if (bn_binding_name_pattern_match (ts_str(name), "/nkn/ssld/config/client-req/client_auth")) {
		tbool t_client_auth;

		err = bn_binding_get_tbool(binding,
				ba_value, NULL,
				&t_client_auth);
		bail_error(err);

		if (ssl_client_auth != (int)t_client_auth) {
			ssl_client_auth = t_client_auth;
			ssl_reinit_ssl_ctx_client_auth();
		}
 
	}
bail:

        return err;
}

int ssld_origin_req_cfg_handle_change(const bn_binding_array *arr,
			uint32 idx, bn_binding *binding, void *data)

{
        int err = 0;
        const tstring *name = NULL;
	ssl_cert_node_data_t *psslcert = NULL;
        tbool *rechecked_licenses_p = data;

        UNUSED_ARGUMENT(arr);
        UNUSED_ARGUMENT(idx);

        bail_null(rechecked_licenses_p);

        err = bn_binding_get_name(binding, &name);
        bail_error(err);

	if (bn_binding_name_pattern_match (ts_str(name), "/nkn/ssld/config/origin-req/server_auth")) {
		tbool t_server_auth;

		err = bn_binding_get_tbool(binding,
				ba_value, NULL,
				&t_server_auth);
		bail_error(err);

		if (ssl_server_auth != (int)t_server_auth) {
			ssl_server_auth = t_server_auth;
		}
 
	} else if (bn_binding_name_pattern_match (ts_str(name), "/nkn/ssld/config/origin-req/cipher")) {
		char *t_cipher = NULL;

		err = bn_binding_get_str (binding,
				ba_value, bt_string, NULL,
				&t_cipher);
		bail_error(err);

		pthread_mutex_lock(&cipher_lock);
		if(ssl_ciphers) { 
			free(ssl_ciphers); 
			ssl_ciphers = NULL;
		}

		if (t_cipher) {
			ssl_ciphers = strdup(t_cipher);
		} else {
			ssl_ciphers = NULL;
		}

		pthread_mutex_unlock(&cipher_lock);
		if (t_cipher) safe_free(t_cipher);

	}
bail:

        return err;
}
#if 0
int ssld_origin_cipher_cfg_handle_change(const bn_binding_array *arr,
			uint32 idx, bn_binding *binding, void *data)

{
        int err = 0;
        const tstring *name = NULL;
	ssl_cert_node_data_t *psslcert = NULL;
        tbool *rechecked_licenses_p = data;

        UNUSED_ARGUMENT(arr);
        UNUSED_ARGUMENT(idx);

        bail_null(rechecked_licenses_p);

        err = bn_binding_get_name(binding, &name);
        bail_error(err);

	if (bn_binding_name_pattern_match (ts_str(name), "/nkn/ssld/config/origin-req/cipher")) {
		char *t_cipher = NULL;

		err = bn_binding_get_str (binding,
				ba_value, bt_string, NULL,
				&t_cipher);
		bail_error(err);

		pthread_mutex_lock(&cipher_lock);
		if(ssl_ciphers) { 
			free(ssl_ciphers); 
			ssl_ciphers = NULL;
		}

		if (t_cipher) {
			ssl_ciphers = strdup(t_cipher);
		} else {
			ssl_ciphers = NULL;
		}

		pthread_mutex_unlock(&cipher_lock);
		if (t_cipher) safe_free(t_cipher);

	}
bail:

        return err;
}
#endif
int ssld_license_cfg_handle_change(const bn_binding_array *arr,
			uint32 idx, bn_binding *binding, void *data)

{
        int err = 0;
        const tstring *name = NULL;
        tbool *rechecked_licenses_p = data;

        UNUSED_ARGUMENT(arr);
        UNUSED_ARGUMENT(idx);

        bail_null(rechecked_licenses_p);

        err = bn_binding_get_name(binding, &name);
        bail_error(err);

	if (bn_binding_name_pattern_match (ts_str(name), ssl_license_status)) {
		tbool t_license;
	
		err = bn_binding_get_tbool(binding,
				ba_value, NULL,
				&t_license);
		bail_error(err);
		ssl_license_cfg(t_license);
	}
bail:

        return err;
}
int ssld_network_cfg_handle_change(const bn_binding_array *arr,
			uint32 idx, bn_binding *binding, void *data)

{
        int err = 0;
        const tstring *name = NULL;
        tbool *rechecked_licenses_p = data;

        UNUSED_ARGUMENT(arr);
        UNUSED_ARGUMENT(idx);

        bail_null(rechecked_licenses_p);

        err = bn_binding_get_name(binding, &name);
        bail_error(err);

	if (bn_binding_name_pattern_match (ts_str(name), network_idle_timeout)) {
		uint32_t t_socket_timeout;
	
		err = bn_binding_get_uint32(binding,
				ba_value, NULL,
				&t_socket_timeout);
		bail_error(err);
		if(t_socket_timeout > 0 ) 
			ssl_idle_timeout = t_socket_timeout;
		else 
			ssl_idle_timeout = SSL_DEFAULT_IDLE_TIMEOUT;
	}
bail:

        return err;
}
int ssld_conn_pool_cfg_handle_change(const bn_binding_array *arr,
			uint32 idx, bn_binding *binding, void *data)

{
        int err = 0;
        const tstring *name = NULL;
        tbool *rechecked_licenses_p = data;

        UNUSED_ARGUMENT(arr);
        UNUSED_ARGUMENT(idx);

        bail_null(rechecked_licenses_p);

        err = bn_binding_get_name(binding, &name);
        bail_error(err);

	if (bn_binding_name_pattern_match (ts_str(name), conn_pool_origin_max_conn_binding)) {
		uint32 t_cpool_max_conn = 0;

                err = bn_binding_get_uint32(binding,
                                ba_value, NULL,
                                &t_cpool_max_conn);
                bail_error(err);

                nkn_cfg_tot_socket_in_pool = t_cpool_max_conn;


	} else 	if (bn_binding_name_pattern_match (ts_str(name), conn_pool_origin_timeout_binding)) {
		uint32 t_cpool_timeout = 0;
                err = bn_binding_get_uint32(binding,
                                ba_value, NULL,
                                &t_cpool_timeout);
                bail_error(err);

                cp_idle_timeout = t_cpool_timeout;
	} else if (bn_binding_name_pattern_match (ts_str(name), conn_pool_origin_enable_binding)) {
		tbool t_conn_pool_enabled = false;
                err = bn_binding_get_tbool(binding,
                                ba_value, NULL,
                                &t_conn_pool_enabled);
                bail_error(err);
                cp_enable = t_conn_pool_enabled;

	}


bail:

        return err;
}
int
nvsd_http_cfg_server_port(const bn_binding_array *arr, uint32 idx,
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

	if(http_server_port == 0 ) 
		http_server_port = serv_port;

        if (NULL == http_server_port_list)
                err = ts_new_sprintf(&http_server_port_list, "%d ", serv_port);
        else
                err = ts_append_sprintf(http_server_port_list, "%d ", serv_port);

        bail_error(err);
bail:
        return err;

} /* end of nvsd_http_cfg_server_port () */

static ssl_cert_node_data_t * ssld_mgmt_get_cert_obj_(const char *cpCert)
{
	int i;

	/* Sanity Check */
	if (cpCert == NULL) return(NULL);

	/* Now find the matching entry */
	for (i = 0; i < NKN_MAX_SSL_CERTS ; i++) {
		if ((lstSSLCerts[i].name != NULL) &&
		    (strcmp(cpCert, lstSSLCerts[i].name) == 0)) {
			/* Return this entry */
			return (&(lstSSLCerts[i]));
		}
	}

	/* No Match */
	return (NULL);
}

int ssld_mgmt_module_cfg_handle_change(
        bn_binding_array *old_bindings,
        bn_binding_array *new_bindings)
{
	int err = 0;
	tbool rechecked_licenses = false;

	/*default  change callback */
	/* handle only for new bindings */
//	err = bn_binding_array_foreach(new_bindings, ssld_default_cfg_handle_change,
//			    &rechecked_licenses);
//	bail_error(err);

	/*cert  change callback */
	err = bn_binding_array_foreach(new_bindings, ssld_cert_cfg_handle_change,
			    &rechecked_licenses);
	bail_error(err);

	err = bn_binding_array_foreach(old_bindings, ssld_cert_delete_cfg_handle_change,
			    &rechecked_licenses);
	bail_error(err);

	/*key  change callback */
	err = bn_binding_array_foreach(new_bindings, ssld_key_cfg_handle_change,
			    &rechecked_licenses);
	bail_error(err);

	err = bn_binding_array_foreach(old_bindings, ssld_key_delete_cfg_handle_change,
			    &rechecked_licenses);
	bail_error(err);


	/*vhost  change callback */
	err = bn_binding_array_foreach(new_bindings, ssld_vhost_cfg_handle_change,
			    &rechecked_licenses);
	bail_error(err);

	err = bn_binding_array_foreach(old_bindings, ssld_vhost_delete_cfg_handle_change,
			    &rechecked_licenses);
	bail_error(err);


	/*Client authentication  change callback */
	err = bn_binding_array_foreach(new_bindings, ssld_client_auth_cfg_handle_change,
			    &rechecked_licenses);
	bail_error(err);

	/*Server authentication  change callback */
	err = bn_binding_array_foreach(new_bindings, ssld_origin_req_cfg_handle_change,
			    &rechecked_licenses);
	bail_error(err);
#if 0
	/* SSL cipher text change callback*/
	err = bn_binding_array_foreach(new_bindings, ssld_origin_cipher_cfg_handle_change,
			    &rechecked_licenses);
	bail_error(err);
#endif

	/*License change callback */
	err = bn_binding_array_foreach(new_bindings, ssld_license_cfg_handle_change,
			    &rechecked_licenses);
	bail_error(err);

	/*delivery  change callback */
	err = bn_binding_array_foreach(new_bindings, ssld_delivery_cfg_handle_change,
			    &rechecked_licenses);
	bail_error(err);

	err = bn_binding_array_foreach(new_bindings, ssld_network_cfg_handle_change,
			    &rechecked_licenses);
	bail_error(err);

	err = bn_binding_array_foreach(new_bindings, ssld_conn_pool_cfg_handle_change,
			    &rechecked_licenses);
	bail_error(err);
bail:

	return err;
}

///////////////////////////////////////////////////////////////////////////////
int ssld_config_handle_set_request(
        bn_binding_array *old_bindings,
        bn_binding_array *new_bindings)
{
        int err = 0;
        err = ssld_mgmt_module_cfg_handle_change(old_bindings, new_bindings);
        bail_error(err);
bail:
        return err;
}

/* ------------------------------------------------------------------------- */
/* End of nkn_mgmt_ssl.c */
/* ------------------------------------------------------------------------- */
///////////////////////////////////////////////////////////////////////////////

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

int ssld_mgmt_initiate_exit(void)
{
	int err = 0;
	int i;

	mgmt_exiting = true;

	for (i = 0; i < num_signals_handled; ++i) {
		err = lew_event_delete(ssld_mgmt_lew, &(event_signals[i]));
		event_signals[i] = NULL; // Needs to be NULL for the case of retry
		bail_error(err);
	}

	err = mdc_wrapper_disconnect(mgmt_mcc, false);
	bail_error(err);

	err = lew_escape_from_dispatch(ssld_mgmt_lew, true);
	bail_error(err);

bail:
	return err;
}

///////////////////////////////////////////////////////////////////////////////
int ssld_deinit(void)
{
	int err = 0;

	err = mdc_wrapper_deinit(&mgmt_mcc);
	bail_error(err);
	mgmt_mcc = NULL;

	err = lew_deinit(&ssld_mgmt_lew);
	bail_error(err);

	ssld_mgmt_lew = NULL;

bail:
	return err;
}

void * ssld_mgmt_thrd(void * arg)
{
        int err = 0;

        int start = 1;
        /* Connect/create GCL sessions, initialize mgmtd i/f
        */
        while (1) {
                mgmt_exiting = false;
                printf("mgmt connect: initialized\n");
                if (start == 1) {
                        err = ssld_mgmt_init(1);
                        bail_error(err);
                } else {
			/* Thilak - is this else part needed */
                        while ((err = ssld_mgmt_init(0)) != 0) {
                                ssld_deinit();
                                sleep(2);
                        }
                }

                err = ssld_mgmt_main_loop();
                complain_error(err);

                err = ssld_deinit();
                complain_error(err);
                printf("mgmt connect: de-initialized\n");
                start = 0;
        }
bail:
        printf("ssld_mgmt_thrd: exit\n");

        return NULL;
}



void ssld_mgmt_thrd_init(void)
{
	pthread_t mgmtid;

	pthread_mutex_init(&upload_mgmt_log_lock, NULL);
	pthread_create(&mgmtid, NULL, ssld_mgmt_thrd, NULL);

	return;
}

void ssl_license_cfg(tbool t_license) 
{
	if(ssl_license_enable != (int)t_license) {
		ssl_license_enable  = (int)t_license;
		if(t_license == FALSE) {
			ssl_interface_deinit();
		} else {
			ssl_interface_init();
		}
	}
}

void ssl_network_thread_cfg(tstring *t_nw_threads)
{
       if(t_nw_threads == NULL) {
               NM_tot_threads = 2;
       }
       else  {
               NM_tot_threads = atoi(ts_str(t_nw_threads));
       }


}
