/*
 *
 * Filename:  nkn_mgmt_http.c
 * Date:      2008/11/23 
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.  
 * All rights reserved.
 *
 *
 */

#include <unistd.h>

/* Samara Headers */
#include "md_client.h"
#include "license.h"

/* NVSD headers */
#include "nkn_defs.h"
#include "nkn_cfg_params.h"
#include "nkn_common_config.h"
#include "nvsd_mgmt_lib.h"

/* Local Function Prototypes */
int nvsd_http_cfg_query(const bn_binding_array * bindings);

int nvsd_http_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings);
int nvsd_http_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * b_value, void *data);

int nvsd_http_cfg_server_port(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * b_value, void *data);

int nvsd_http_content_type_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array *
	bndgs_name_components,
	const tstring *
	bndgs_name_last_part,
	const tstring * b_value,
	void *data);

int nvsd_http_content_suppress_resp_header_cfg_handle_change(const
	bn_binding_array *
	arr, uint32 idx,
	const bn_binding *
	binding,
	const tstring *
	bndgs_name,
	const tstr_array *
	bndgs_name_components,
	const tstring *
	bndgs_name_last_part,
	const tstring *
	b_value,
	void *data);

int nvsd_http_content_add_resp_header_cfg_handle_change(const bn_binding_array *
	arr, uint32 idx,
	const bn_binding *
	binding,
	const tstring *
	bndgs_name,
	const tstr_array *
	bndgs_name_components,
	const tstring *
	bndgs_name_last_part,
	const tstring * b_value,
	void *data);
int nvsd_http_content_set_resp_header_cfg_handle_change(const bn_binding_array *
	arr, uint32 idx,
	const bn_binding *
	binding,
	const tstring *
	bndgs_name,
	const tstr_array *
	bndgs_name_components,
	const tstring *
	bndgs_name_last_part,
	const tstring * b_value,
	void *data);
int nvsd_http_content_allowed_type_cfg_handle_change(const bn_binding_array *
	arr, uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array *
	bndgs_name_components,
	const tstring *
	bndgs_name_last_part,
	const tstring * b_value,
	void *data);
int nvsd_http_delete_resp_header_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array *
	bndgs_name_components,
	const tstring *
	bndgs_name_last_part,
	const tstring * b_value,
	void *data);
int nvsd_http_cfg_server_interface(const bn_binding_array * arr, uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * b_value, void *data);

int
nvsd_http_cfg_allow_req_method(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * b_value, void *data);

int nvsd_http_delete_req_method_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array *
	bndgs_name_components,
	const tstring *
	bndgs_name_last_part,
	const tstring * b_value,
	void *data);

int
nvsd_http_filter_cfg_handle_change(const bn_binding_array *arr,
		uint32 idx,const bn_binding *binding,
		const tstring *bndgs_name,
		const tstr_array *bndgs_name_components,
		const tstring *bndgs_name_last_part,
		const tstring *b_value, void *data);


/* Extern Variables */
extern md_client_context *nvsd_mcc;
extern int nkn_http_trace_enable;
extern int nkn_http_reval_meth_get;
extern int nkn_http_pers_conn_nums;
extern int nkn_cfg_tot_socket_in_pool;
extern int nkn_cfg_dpi_uri_mode;
extern int nkn_http_ipv6_enable;

const char http_config_prefix[] = "/nkn/nvsd/http/config";

char http_config_server_port_prefix[] = "/nkn/nvsd/http/config/server_port";
const char http_config_server_interface_prefix[] =
"/nkn/nvsd/http/config/interface";

char http_config_content_type_prefix[] = "/nkn/nvsd/http/config/content/type";
char http_content_suppress_resp_header_prefix[] =
"/nkn/nvsd/http/config/response/header/supress";
char http_content_add_resp_header_prefix[] =
"/nkn/nvsd/http/config/response/header/append";
char http_content_set_resp_header_prefix[] =
"/nkn/nvsd/http/config/response/header/set";
char http_content_allowed_type_prefix[] =
"/nkn/nvsd/http/config/content/allowed_type";
char http_config_allow_req_method_prefix[] =
"/nkn/nvsd/http/config/allow_req/method";
char http_config_allow_all_req_method_prefix[] =
"/nkn/nvsd/http/config/allow_req/all";
char http_config_filter_mode_prefix[] = "/nkn/nvsd/http/config/filter";

/* Local Variables */
static boolean dynamic_node_change = NKN_FALSE;

static const char http_rate_control_binding[] =
"/nkn/nvsd/http/config/rate_control";
static const char http_max_req_size_binding[] =
"/nkn/nvsd/http/config/max_http_req_size";
static const char http_conn_pool_origin_enable_binding[] =
"/nkn/nvsd/http/config/conn-pool/origin/enabled";
static const char http_conn_pool_origin_timeout_binding[] =
"/nkn/nvsd/http/config/conn-pool/origin/timeout";
static const char http_conn_pool_origin_max_conn_binding[] =
"/nkn/nvsd/http/config/conn-pool/origin/max-conn";

const char http_origin_reval_meth_get_binding[] =
"/nkn/nvsd/http/config/reval_meth_get";

tstring *server_port_list = NULL;
tstring *server_interface_list = NULL;


/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_http_cfg_query()
 *	purpose : query for http config parameters
 */
int
nvsd_http_cfg_query(const bn_binding_array * bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    lc_log_basic(LOG_DEBUG, "nvsd http module mgmtd query initializing ..");

    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    http_config_server_port_prefix,
	    NULL,
	    nvsd_http_cfg_server_port,
	    &rechecked_licenses);
    bail_error(err);

    /*
     * We should have read all the ports that need to be opened for listening
     * on. Send the port list to the config module 
     */
    if (server_port_list != NULL) {
	nkn_http_portlist_callback((char *) ts_str(server_port_list));
	ts_free(&server_port_list);
    }

    /*----------- HTTP REQ-METHOD--------------*/
    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    http_config_allow_req_method_prefix,
	    NULL,
	    nvsd_http_cfg_allow_req_method,
	    &rechecked_licenses);
    bail_error(err);

    /*----------- HTTP LISTEN INTERFACES -----------*/
    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    http_config_server_interface_prefix,
	    NULL,
	    nvsd_http_cfg_server_interface,
	    &rechecked_licenses);
    bail_error(err);

    /*
     * We should have read all the interfaces that need to be opened for
     * listening on. Sent the interface list to the config module. 
     */
    if (server_interface_list != NULL) {
	nkn_http_interfacelist_callback((char *) ts_str(server_interface_list));
	ts_free(&server_interface_list);
    }

    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    http_config_prefix,
	    NULL,
	    nvsd_http_cfg_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    /*
     * Bind to get CONTENT TYPE 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    http_config_content_type_prefix,
	    NULL,
	    nvsd_http_content_type_cfg_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    /*
     * Bind to get CONTENT SUPPRESS RESP HEADER 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    http_content_suppress_resp_header_prefix,
	    NULL,
	    nvsd_http_content_suppress_resp_header_cfg_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    /*
     * Bind to get CONTENT ADD RESP HEADER 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    http_content_add_resp_header_prefix,
	    NULL,
	    nvsd_http_content_add_resp_header_cfg_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    /*
     * Bind to get CONTENT SET RESP HEADER 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    http_content_set_resp_header_prefix,
	    NULL,
	    nvsd_http_content_set_resp_header_cfg_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    /*
     * Bind to get CONTENT ALLOWED TYPE 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    http_content_allowed_type_prefix,
	    NULL,
	    nvsd_http_content_allowed_type_cfg_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    /*
     * Bind to get filter-nodes
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    http_config_filter_mode_prefix,
	    NULL,
	    nvsd_http_filter_cfg_handle_change,
	    NULL);
    bail_error(err);

bail:
    ts_free(&server_port_list);
    ts_free(&server_interface_list);
    return err;

}	/* end of nvsd_http_cfg_query() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_http_module_cfg_handle_change()
 *	purpose : handler for config changes for http module
 */
int
nvsd_http_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    /*
     * Call the callbacks 
     */
    dynamic_node_change = NKN_TRUE;
    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    http_config_prefix,
	    NULL,
	    nvsd_http_cfg_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    http_config_content_type_prefix,
	    NULL,
	    nvsd_http_content_type_cfg_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    http_content_suppress_resp_header_prefix,
	    NULL,
	    nvsd_http_content_suppress_resp_header_cfg_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    http_content_add_resp_header_prefix,
	    NULL,
	    nvsd_http_content_add_resp_header_cfg_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    http_content_set_resp_header_prefix,
	    NULL,
	    nvsd_http_content_set_resp_header_cfg_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    http_content_allowed_type_prefix,
	    NULL,
	    nvsd_http_content_allowed_type_cfg_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    http_config_allow_req_method_prefix,
	    NULL,
	    nvsd_http_cfg_allow_req_method,
	    &rechecked_licenses);
    bail_error(err);

    err = mgmt_lib_mdc_foreach_binding_prequeried(old_bindings,
	    "/nkn/nvsd/http/config/response",
	    NULL,
	    nvsd_http_delete_resp_header_cfg_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    err = mgmt_lib_mdc_foreach_binding_prequeried(old_bindings,
	    http_config_allow_req_method_prefix,
	    NULL,
	    nvsd_http_delete_req_method_cfg_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    http_config_filter_mode_prefix,
	    NULL,
	    nvsd_http_filter_cfg_handle_change,
	    NULL);
    bail_error(err);

bail:
    dynamic_node_change = NKN_FALSE;
    return err;

}	/* end of nvsd_http_module_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_http_cfg_handle_change()
 *	purpose : handler for config changes for http module
 */
int
nvsd_http_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * b_value, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    tbool *rechecked_licenses_p = data;
        tstring *t_filter_mode = NULL;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(b_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /*-------- RATE CONTROL  ------------*/
    if (ts_equal_str(name, http_rate_control_binding, false)) {
	uint64 t_rate_control = 0;

	err = bn_binding_get_uint64(binding, ba_value, NULL, &t_rate_control);

	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../http/config/rate_control as : \"%ld\"",
		t_rate_control);

	/*
	 * Set the global 
	 */
	max_get_rate_limit = t_rate_control;
    }

    /*-------- MAX HTTP request size ------------*/
    else if (ts_equal_str(name, http_max_req_size_binding, false)) {
	uint32 t_http_req_size = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_http_req_size);

	bail_error(err);
	max_http_req_size = t_http_req_size;
	lc_log_basic(LOG_DEBUG,
		"Read .../http/config/max_http_req_size as : \"%d\"",
		t_http_req_size);
    }

    /*-------- DELIVERY PROTOCOL revalidation method is get or not ------------*/
    else if (ts_equal_str(name, http_origin_reval_meth_get_binding, false)) {
	tbool t_reval_meth_get = false;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_reval_meth_get);

	bail_error(err);
	nkn_http_reval_meth_get = t_reval_meth_get;
    }

    /*-------- DELIVERY PROTOCOL TRACE ENABLED ------------*/
    else if (ts_equal_str(name, "/nkn/nvsd/http/config/trace/enabled", false)) {
	tbool t_trace_enabled = false;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_trace_enabled);

	bail_error(err);
	nkn_http_trace_enable = t_trace_enabled;
    }

    /*-------- DELIVERY PROTOCOL HTTP IPv6  ENABLED ------------*/
    else if (ts_equal_str(name, "/nkn/nvsd/http/config/ipv6/enabled", false)) {
	tbool t_ipv6_enabled = false;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_ipv6_enabled);

	bail_error(err);
	nkn_http_ipv6_enable = t_ipv6_enabled;
    }

    /*-------- CONNECTION PERSISTENCE REQUEST------------*/
    else if (ts_equal_str
	    (name, "/nkn/nvsd/http/config/connection/persistence/num-requests",
	     false)) {
	uint32 t_conn_nums = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_conn_nums);

	bail_error(err);
	nkn_http_pers_conn_nums = t_conn_nums;
    }

    /*-------- CONNECTION CLOSE RST OPTION------------*/
    else if (ts_equal_str
	    (name, "/nkn/nvsd/http/config/connection/close/use-reset",
	     false)) {
	tbool t_conn_close_rst = false;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_conn_close_rst);

	bail_error(err);
	close_socket_by_RST_enable = t_conn_close_rst;
    }

    /*------- CONNECTION-POOL ORIGIN ENABLED -----------*/
    else if (ts_equal_str(name, http_conn_pool_origin_enable_binding, false)) {
	tbool t_conn_pool_enabled = false;

	err = bn_binding_get_tbool(binding,
		ba_value, NULL, &t_conn_pool_enabled);
	bail_error(err);
	cp_enable = t_conn_pool_enabled;
    }

    /*------- CONNECTION-POOL ORIGIN TIMEOUT -----------*/
    else if (ts_equal_str(name, http_conn_pool_origin_timeout_binding, false)) {

	uint32 t_cpool_timeout = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_cpool_timeout);
	bail_error(err);

	cp_idle_timeout = t_cpool_timeout;
	lc_log_basic(LOG_DEBUG,
		"Read .../http/config/conn-pool/origin/timeout as : \"%d\"",
		t_cpool_timeout);
    }

    /*------ CONNECTION-POOL ORIGIN MAX-CONNECTIONS ---------*/
    else if (ts_equal_str(name, http_conn_pool_origin_max_conn_binding, false)) {

	uint32 t_cpool_max_conn = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_cpool_max_conn);
	bail_error(err);

	nkn_cfg_tot_socket_in_pool = t_cpool_max_conn;
	lc_log_basic(LOG_DEBUG,
		"Read .../http/config/conn-pool/origin/max-conn as : \"%d\"",
		t_cpool_max_conn);
    }

    /*----- HTTP ALLOW ALL REQUEST METHOD--------*/
    else if (ts_equal_str(name, http_config_allow_all_req_method_prefix, false)) {
	tbool t_allow_all_req_method = false;

	err = bn_binding_get_tbool(binding,
		ba_value, NULL, &t_allow_all_req_method);
	bail_error(err);
	nkn_http_set_all_req_method_callback((int) t_allow_all_req_method);
    } else
	if (bn_binding_name_parts_pattern_match_va
		(bndgs_name_components, 4, 1, "om_conn_limit")) {
	    uint32 t_cpool_max_conn = 0;

	    err = bn_binding_get_uint32(binding, ba_value, NULL, &t_cpool_max_conn);
	    bail_error(err);
	    om_connection_limit = t_cpool_max_conn;
	/*----- URI FILTER MODE --------*/
	} else if (ts_equal_str(name, "/nkn/nvsd/http/config/filter/mode", false)) {
                err = bn_binding_get_tstr(binding,
                                ba_value, bt_string, NULL,
                                &t_filter_mode);
		bail_error(err);
		if (!strcmp(ts_str(t_filter_mode), "packet")) {
			nkn_cfg_dpi_uri_mode = 1;
		} else {
			nkn_cfg_dpi_uri_mode = 0;
		}
	}

bail:
    return err;
}	/* end of nvsd_http_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_http_content_type_cfg_handle_change()
 *	purpose : handler for config changes for contenty type of http module
 */
int
nvsd_http_cfg_server_port(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * b_value, void *data)
{
    int err = 0;
    tbool *rechecked_licenses_p = data;
    uint16 serv_port = 0;
    tstr_array *name_parts = NULL;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);

    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(b_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name_parts(binding, &name_parts);
    bail_error(err);

    if (!bn_binding_name_parts_pattern_match_va(name_parts, 5, 2, "*", "port"))
	goto bail;

    err = bn_binding_get_uint16(binding, ba_value, NULL, &serv_port);
    bail_error(err);

    if (NULL == server_port_list)
	err = ts_new_sprintf(&server_port_list, "%d ", serv_port);
    else
	err = ts_append_sprintf(server_port_list, "%d ", serv_port);

    bail_error(err);
bail:
    tstr_array_free(&name_parts);
    return err;

}	/* end of nvsd_http_cfg_server_port () */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_http_content_type_cfg_handle_change()
 *	purpose : handler for config changes for contenty type of http module
 */
int
nvsd_http_content_type_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array *
	bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * b_value, void *data)
{
    int err = 0;
    char *node_name = NULL;
    tstring *t_content_type_name = NULL;
    tstring *t_content_type_value = NULL;
    tbool t_content_type_allowed;
    tbool *rechecked_licenses_p = data;
    char buf[1024];
    tstr_array *name_parts = NULL;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);

    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(b_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name_parts(binding, &name_parts);
    bail_error(err);

    if (!bn_binding_name_parts_pattern_match_va(name_parts, 6, 1, "*"))
	return err;				/* Not this node hence bail */

    lc_log_basic(LOG_DEBUG, "Yes this is the Node : %s",
	    http_config_content_type_prefix);

    /*-------- CONTENT TYPE ------------*/
    err = bn_binding_get_tstr(binding,
	    ba_value, bt_string, NULL, &t_content_type_name);

    bail_error(err);

    /*
     * It is this node hence proceed 
     */
    lc_log_basic(LOG_DEBUG, "Read .../http/config/content/type as : \"%s\"",
	    ts_str(t_content_type_name));

    if (dynamic_node_change)
	sleep(2);				// Pausing to ensure nodes are properly set

    /*
     * Get the value of this content type 
     */
    node_name = smprintf("/nkn/nvsd/http/config/content/type/%s/mime",
	    ts_str(t_content_type_name));
    err = mdc_get_binding_tstr(nvsd_mcc, NULL, NULL, NULL,
	    &t_content_type_value, node_name, NULL);
    bail_error(err);
    safe_free(node_name);

    lc_log_basic(LOG_DEBUG,
	    "Read .../http/config/content/type/%s/mime as : \"%s\"",
	    ts_str(t_content_type_name), ts_str(t_content_type_value));

    /*
     * Get the value of flag allowed for the content type 
     */
    node_name =
	smprintf("/nkn/nvsd/http/config/content/type/%s/allow",
		ts_str(t_content_type_name));
    err =
	mdc_get_binding_tbool(nvsd_mcc, NULL, NULL, NULL,
		&t_content_type_allowed, node_name, NULL);
    bail_error(err);

    lc_log_basic(LOG_DEBUG,
	    "Read .../http/config/content/type/%s/allow as : \"%s\"",
	    ts_str(t_content_type_name),
	    t_content_type_allowed ? "TRUE" : "FALSE");

    /*
     * Call the callback function with the string read 
     */
    snprintf(buf, sizeof (buf), "%s %s",
	    ts_str(t_content_type_name), ts_str(t_content_type_value));
    http_cfg_content_type_callback(buf);

bail:
    safe_free(node_name);
    ts_free(&t_content_type_name);
    ts_free(&t_content_type_value);
    tstr_array_free(&name_parts);
    return err;
}	/* end of * nvsd_http_content_type_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_http_content_suppress_resp_header_cfg_handle_change()
 *	purpose : handler for config changes for contenty type of http module
 */
int
nvsd_http_content_suppress_resp_header_cfg_handle_change(const bn_binding_array
	* arr, uint32 idx,
	const bn_binding *
	binding,
	const tstring *
	bndgs_name,
	const tstr_array *
	bndgs_name_components,
	const tstring *
	bndgs_name_last_part,
	const tstring *
	b_value, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    tstring *t_content_suppress_resp_header = NULL;
    tbool *rechecked_licenses_p = data;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);

    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(b_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /*
     * Check if this is our node 
     */
    if (!ts_has_prefix_str
	    (name, http_content_suppress_resp_header_prefix, false))
	return err;				/* Not this node hence bail */

    /*-------- Get Content Suppress Response Header ------------*/
    err = bn_binding_get_tstr(binding,
	    ba_value, bt_string, NULL,
	    &t_content_suppress_resp_header);

    bail_error(err);
    lc_log_basic(LOG_DEBUG,
	    "Read .../http/config/response/header/supress as : \"%s\"",
	    ts_str(t_content_suppress_resp_header));

    /*
     * Call the callback function with the string read 
     */
    http_cfg_suppress_response_hdr_callback((char *)
	    ts_str
	    (t_content_suppress_resp_header));

bail:
    ts_free(&t_content_suppress_resp_header);
    return err;
}	/* end of nvsd_http_content_suppress_resp_header_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_http_content_add_resp_header_cfg_handle_change()
 *	purpose : handler for config changes for contenty type of http module
 */
int
nvsd_http_content_add_resp_header_cfg_handle_change(const bn_binding_array *
	arr, uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array *
	bndgs_name_components,
	const tstring *
	bndgs_name_last_part,
	const tstring * b_value,
	void *data)
{
    int err = 0;
    char *node_name = NULL;
    tstring *t_content_add_resp_header = NULL;
    tstring *t_content_add_resp_header_value = NULL;
    tbool *rechecked_licenses_p = data;
    char buf[1024];
    tstr_array *name_parts = NULL;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);

    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(b_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name_parts(binding, &name_parts);
    bail_error(err);

    /*
     * Check if this is our node 
     */
    if (!bn_binding_name_parts_pattern_match_va
	    (bndgs_name_components, 7, 1, "*"))
	return err;				/* Not this node hence bail */

    /*-------- Get Content Add Response Header ------------*/
    err = bn_binding_get_tstr(binding,
	    ba_value, bt_string, NULL,
	    &t_content_add_resp_header);

    bail_error(err);
    lc_log_basic(LOG_DEBUG,
	    "Read .../http/config/response/header/append as : \"%s\"",
	    ts_str(t_content_add_resp_header));

    /*
     * Get the value of this content type 
     */
    node_name =
	smprintf("/nkn/nvsd/http/config/response/header/append/%s/value",
		ts_str(t_content_add_resp_header));
    err =
	mdc_get_binding_tstr(nvsd_mcc, NULL, NULL, NULL,
		&t_content_add_resp_header_value, node_name, NULL);
    bail_error(err);

    lc_log_basic(LOG_DEBUG,
	    "Read .../http/config/response/header/%s/value as : \"%s\"",
	    ts_str(t_content_add_resp_header),
	    ts_str(t_content_add_resp_header_value));

    /*
     * Call the callback function with the string read 
     */
    snprintf(buf, sizeof (buf), "%s %s",
	    ts_str(t_content_add_resp_header),
	    ts_str(t_content_add_resp_header_value));
    http_cfg_add_response_hdr_callback(buf);

bail:
    safe_free(node_name);
    ts_free(&t_content_add_resp_header);
    ts_free(&t_content_add_resp_header_value);
    tstr_array_free(&name_parts);
    return err;
}	/* end of * nvsd_http_content_add_resp_header_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_http_content_set_resp_header_cfg_handle_change()
 *	purpose : handler for config changes for contenty type of http module
 */
int
nvsd_http_content_set_resp_header_cfg_handle_change(const bn_binding_array *
	arr, uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array *
	bndgs_name_components,
	const tstring *
	bndgs_name_last_part,
	const tstring * b_value,
	void *data)
{
    int err = 0;
    char *node_name = NULL;
    tstring *t_content_set_resp_header = NULL;
    tstring *t_content_set_resp_header_value = NULL;
    tbool *rechecked_licenses_p = data;
    char buf[1024];
    tstr_array *name_parts = NULL;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);

    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(b_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name_parts(binding, &name_parts);
    bail_error(err);

    /*
     * Check if this is our node 
     */
    if (!bn_binding_name_parts_pattern_match_va(name_parts, 7, 1, "*"))
	return err;				/* Not this node hence bail */

    /*-------- Get Content Add Response Header ------------*/
    err = bn_binding_get_tstr(binding,
	    ba_value, bt_string, NULL,
	    &t_content_set_resp_header);

    bail_error(err);

    /*
     * Check for null - this happens when the no command is executed 
     */
    if (!t_content_set_resp_header)
	goto bail;

    lc_log_basic(LOG_DEBUG,
	    "Read .../http/config/response/header/set as : \"%s\"",
	    ts_str(t_content_set_resp_header));

    /*
     * Get the value of this content type 
     */
    node_name =
	smprintf("/nkn/nvsd/http/config/response/header/set/%s/value",
		ts_str(t_content_set_resp_header));
    err =
	mdc_get_binding_tstr(nvsd_mcc, NULL, NULL, NULL,
		&t_content_set_resp_header_value, node_name, NULL);
    bail_error(err);

    lc_log_basic(LOG_DEBUG,
	    "Read .../http/config/response/header/%s/value as : \"%s\"",
	    ts_str(t_content_set_resp_header),
	    ts_str(t_content_set_resp_header_value));

    /*
     * Call the callback function with the string read 
     */
    snprintf(buf, sizeof (buf), "%s %s",
	    ts_str(t_content_set_resp_header),
	    ts_str(t_content_set_resp_header_value));
    http_cfg_add_response_hdr_callback(buf);

bail:
    safe_free(node_name);
    ts_free(&t_content_set_resp_header);
    tstr_array_free(&name_parts);
    return err;
}	/* end of * nvsd_http_content_set_resp_header_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_http_content_allowed_type_cfg_handle_change()
 *	purpose : handler for config changes for contenty type of http module
 */
int
nvsd_http_content_allowed_type_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array *
	bndgs_name_components,
	const tstring *
	bndgs_name_last_part,
	const tstring * b_value,
	void *data)
{
    int err = 0;
    const tstring *name = NULL;
    tstring *t_content_allowed_type = NULL;
    tbool *rechecked_licenses_p = data;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);

    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(b_value);
    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /*
     * Check if this is our node 
     */
    if (!ts_has_prefix_str(name, http_content_allowed_type_prefix, false))
	return err;				/* Not this node hence bail */

    /*-------- Get Content Allowed Type ------------*/
    err = bn_binding_get_tstr(binding,
	    ba_value, bt_string, NULL,
	    &t_content_allowed_type);

    bail_error(err);
    lc_log_basic(LOG_DEBUG,
	    "Read .../http/config/content/allowed_type as : \"%s\"",
	    ts_str(t_content_allowed_type));

    /*
     * Call the callback function with the string read 
     */
    http_cfg_allowed_content_types_callback((char *)
	    ts_str(t_content_allowed_type));

bail:
    ts_free(&t_content_allowed_type);
    return err;
}	/* end of * nvsd_http_content_allowed_type_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_http_delete_resp_header_cfg_handle_change()
 *	purpose : handler for config delete for response header of http module
 */
int
nvsd_http_delete_resp_header_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array *
	bndgs_name_components,
	const tstring *
	bndgs_name_last_part,
	const tstring * b_value,
	void *data)
{
    int err = 0;
    char *node_name = NULL;
    tstring *t_content_set_resp_header = NULL;
    tstring *t_content_suppress_resp_header = NULL;
    tbool *rechecked_licenses_p = data;
    char buf[256];
    tstr_array *name_parts = NULL;

    (void) node_name;
    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);

    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(b_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name_parts(binding, &name_parts);
    bail_error(err);

    /*
     * Check if this is our node 
     */
    if ((bn_binding_name_parts_pattern_match_va(name_parts, 7, 1, "*")) ||
	    (bn_binding_name_parts_pattern_match_va(name_parts, 7, 1, "*"))) {

	/*-------- Get Content Response Header ------------*/
	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL,
		&t_content_set_resp_header);
	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Remove :  .../http/config/response/header/set as : \"%s\"",
		ts_str(t_content_set_resp_header));

	/*
	 * Call the callback function with the string read 
	 */
	memset(buf, 0, sizeof (buf));
	snprintf(buf, sizeof (buf), "%s", ts_str(t_content_set_resp_header));
	http_no_cfg_add_response_hdr_callback(buf);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 7, 1, "*")) {

	/*-------- Get Content Suppress Header ------------*/
	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL,
		&t_content_suppress_resp_header);
	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Remove :  .../http/config/response/header/supress as : \"%s\"",
		ts_str(t_content_suppress_resp_header));

	/*
	 * Call the callback function with the string read 
	 */
	memset(buf, 0, sizeof (buf));
	snprintf(buf, sizeof (buf), "%s",
		ts_str(t_content_suppress_resp_header));
	http_no_cfg_suppress_response_hdr_callback(buf);
    }

bail:
    ts_free(&t_content_set_resp_header);
    ts_free(&t_content_suppress_resp_header);
    tstr_array_free(&name_parts);
    return err;
}	/* end of * nvsd_http_delete_resp_header_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_http_cfg_server_interface()
 *	purpose : handler for config changes for http interface
 */
int
nvsd_http_cfg_server_interface(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * b_value, void *data)
{
    int err = 0;
    tbool *rechecked_licenses_p = data;
    const tstring *name = NULL;
    tstring *t_interface = NULL;
    tstr_array *name_parts = NULL;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);
    bail_null(rechecked_licenses_p);

    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(b_value);

    err = bn_binding_get_name_parts(binding, &name_parts);
    bail_error(err);

    lc_log_basic(LOG_DEBUG, "---------------> Node is : %s", ts_str(name));

    if (!bn_binding_name_parts_pattern_match_va(name_parts, 5, 1, "*"))
	goto bail;

    err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &t_interface);
    bail_error(err);

    if (NULL == server_interface_list)
	err =
	    ts_new_sprintf(&server_interface_list, "%s ", ts_str(t_interface));
    else
	err =
	    ts_append_sprintf(server_interface_list, "%s ",
		    ts_str(t_interface));

    bail_error(err);
bail:
    ts_free(&t_interface);
    tstr_array_free(&name_parts);
    return err;

}	/* end of nvsd_http_cfg_server_interface () */

int
nvsd_http_cfg_allow_req_method(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * b_value, void *data)
{
    int err = 0;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);

    UNUSED_ARGUMENT(binding);
    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_last_part);

    if (bn_binding_name_parts_pattern_match_va
	    (bndgs_name_components, 6, 1, "*")) {

	lc_log_basic(LOG_DEBUG, "method:%s ", ts_str(b_value));

	/*
	 * We should have read all the request method that need to allowed 
	 */
	nkn_http_allow_req_method_callback((char *) ts_str(b_value));
    }

    return err;
}	/* end of nvsd_http_cfg_allow_req_method () */

int
nvsd_http_delete_req_method_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array *
	bndgs_name_components,
	const tstring *
	bndgs_name_last_part,
	const tstring * b_value,
	void *data)
{
    int err = 0;
    char *node_name = NULL;
    tstring *t_method = NULL;
    tbool *rechecked_licenses_p = data;
    tstr_array *name_parts = NULL;

    (void) node_name;
    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);

    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(b_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name_parts(binding, &name_parts);
    bail_error(err);

    if (bn_binding_name_parts_pattern_match_va(name_parts, 6, 1, "*")) {
	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_method);
	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Remove :  .../http/config/allow_req/method/ as : \"%s\"",
		ts_str(t_method));

	/*
	 * Call the callback function with the string read 
	 */
	if (t_method != NULL) {
	    nkn_http_delete_req_method_callback((char *) ts_str(t_method));
	}
    }
bail:
    ts_free(&t_method);
    tstr_array_free(&name_parts);
    return err;
}

int
nvsd_http_filter_cfg_handle_change(const bn_binding_array *arr,
		uint32 idx,const bn_binding *binding,
		const tstring *bndgs_name,
		const tstr_array *bndgs_name_parts,
		const tstring *bndgs_name_last_part,
		const tstring *b_value, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    uint32 ret_flags = 0, filter_mode;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);

    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_parts);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(b_value);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /* Check if this is our node */
    if (!ts_has_prefix_str (name, http_config_filter_mode_prefix, false))
	goto bail; /* Not this node hence bail */

    err = bn_binding_get_type(binding, ba_value, NULL, &ret_flags);
    bail_error(err);
    if (ret_flags & btf_deleted)  {
	/* old binding, we don't need to handle this */
	goto bail;
    }

    /* /nkn/nvsd/http/config/proxy/mode */
    if (bn_binding_name_parts_pattern_match_va(bndgs_name_parts, 5, 1, "mode")) {
	if (ts_equal_str(b_value, "cache", true)) {
	    filter_mode = 0;
	} else if (ts_equal_str(b_value, "proxy", true)) {
	    filter_mode = 1;
	} else if (ts_equal_str(b_value, "packet", true)) {
	    filter_mode = 2;
	} else {
	    /* this is an error condition */
	    filter_mode = 3;
	}
	lc_log_basic(LOG_NOTICE, "mode : %s/%d", ts_str(b_value), filter_mode);
    }

bail:
    return err;
}
/* ------------------------------------------------------------------------- */

/* End of nkn_mgmt_http.c */

/* ------------------------------------------------------------------------- */
