/*
 *
 * Filename:  nkn_mgmt_namespace.c
 * Date:      2008/11/23
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008-10 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

/* Standard Headers */
#include <pthread.h>

/* Samara Headers */
#include "common.h"
#include "md_client.h"
#include "license.h"
#include "ttime.h"

/* nvsd Headers */
#include "nkn_defs.h"
#include "nvsd_mgmt.h"
#include "nvsd_mgmt_namespace.h"
#include "nkn_mgmt_defs.h"
#include "nkn_namespace.h"
#include "nkn_mediamgr_api.h"
#include "nkn_regex.h"
#include "nkn_cod.h"
#include "nkn_attr.h"
#include "nkn_stat.h"
#include "nkn_cfg_params.h"
#include "nkn_diskmgr2_api.h"
#include "nvsd_mgmt_pe.h"
#include "nvsd_mgmt_lib.h"
#include "nkn_http.h"
#include "nvsd_mgmt_inline.h"
#include "nkn_dpi.h"

#ifdef PRELOAD_UF_DATA
#include "nkn_namespace_utils.h"
#endif

extern unsigned int jnpr_log_level;

/* Local Macros */
#define	REPEAT_HDR	25
// Number of line so repeat the header
#define MAX_RAM_CACHE_URI_SIZE	(MAX_URI_SIZE + 128)	// MAX URI size +
// buffer for other
// fields
#define GB_TO_BYTES		(1024*1024*1024)
#define KB_TO_BYTES		1024

/* enums */
enum fqdn_operation {
    eFQDN_None,
    eFQDN_add,
    eFQDN_delete,
};

extern url_filter_data_t* nvsd_mgmt_get_url_map(const char *filter_name);


/* Local Function Prototypes */
int
nvsd_namespace_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings);
int
nvsd_mgmt_remove_pe_srvr(namespace_node_data_t * pstNamespace,
	const char *pe_name);

void
nvsd_update_namespace_accesslog_profile_format(const char *profile_name,
	const char *profile_format,
	void *ns_node_data);

int
nsvd_mgmt_ns_is_obj_cached(const char *cp_namespace,
	const char *cp_uid,
	const char *incoming_uri, char **ret_cache_name);

int
get_policy_srvr(const char *pe_name, policy_srvr_obj_t ** pe_obj);

static int
nvsd_namespace_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value, void *data);
static int
nvsd_namespace_delete_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array *
	bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value,
	void *data);
static int
nvsd_uri_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value, void *data);
static int
nvsd_uri_delete_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value, void *data);

static int
nvsd_origin_fetch_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value, void *data);
static int
nvsd_origin_fetch_cfg_delete_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array *
	bndgs_name_components,
	const tstring *
	bndgs_name_last_part,
	const tstring * bndgs_value,
	void *data);

static int
nvsd_origin_fetch_cache_age_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array *
	bndgs_name_components,
	const tstring *
	bndgs_name_last_part,
	const tstring * bndgs_value,
	void *data);

static int
nvsd_origin_request_header_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array *
	bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value,
	void *data);

static int
nvsd_client_response_header_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array *
	bndgs_name_components,
	const tstring *
	bndgs_name_last_part,
	const tstring * bndgs_value,
	void *data);
static int
nvsd_client_response_header_delete_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array *
	bndgs_name_components,
	const tstring *
	bndgs_name_last_part,
	const tstring * bndgs_value,
	void *data);

void remove_virtual_player_in_namespace(const char *t_virtual_player);
void remove_server_map_in_namespace(const char *t_server_map);
void remove_pub_point_in_namespace(const char *t_pub_point);

static int
nvsd_ns_http_server_map_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array *
	bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value,
	void *data);

static int
nvsd_ns_http_server_map_handle_delete_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array *
	bndgs_name_components,
	const tstring *
	bndgs_name_last_part,
	const tstring * bndgs_value,
	void *data);

static int
nvsd_ns_cluster_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value, void *data);

static int
nvsd_ns_cluster_handle_delete_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value, void *data);

static int
nvsd_pe_srvr_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value, void *data);
static int
nvsd_filter_handle_delete_change(const bn_binding_array *arr, uint32 idx,
                                    const bn_binding *binding,
                                    const tstring *bndgs_name,
                                    const tstr_array *bndgs_name_components,
                                    const tstring *bndgs_name_last_part,
                                    const tstring *bndgs_value,
                                    void *data);
static int
nvsd_filter_handle_change(const bn_binding_array *arr, uint32 idx,
			   const bn_binding *binding,
			   const tstring *bndgs_name,
			   const tstr_array *bndgs_name_components,
			   const tstring *bndgs_name_last_part,
			   const tstring *bndgs_value,
			   void *data);

url_filter_data_t listFilterMaps [NKN_MAX_FILTER_MAPS] ;


static int
nvsd_pe_srvr_delete_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value, void *data);

static int
nvsd_ns_cache_index_header_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array *
	bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value,
	void *data);

static int
nvsd_ns_cache_index_header_handle_delete_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array *
	bndgs_name_components,
	const tstring *
	bndgs_name_last_part,
	const tstring * bndgs_value,
	void *data);

char *get_namespace_precedence_list(void);
static int compare_domain(namespace_node_data_t * curr_ns,
	namespace_node_data_t * changed_ns);

static int compare_domain_regex(namespace_node_data_t * curr_ns,
	namespace_node_data_t * changed_ns);

static int
nvsd_mgmt_namespace_clear_current_match(namespace_node_data_t * pNs);

static pub_point_data_t *getPubPoint(namespace_node_data_t * pNamespace,
	const char *pub_point_name);

int ns_fqdn_fill_holes(namespace_node_data_t *ns_node_data);
int update_fqdn_list(namespace_node_data_t *ns_node_data, const char *fqdn,
	int operation);
int find_fqdn_index(const namespace_node_data_t *ns_node_data, const char *fqdn,
	int *fqdn_index);
int ns_fqdn_find_empty_slot(const namespace_node_data_t *ns_node_data,
	int *empty_index);
int ns_check_overlap_fqdns(namespace_node_data_t *ns_first,
	namespace_node_data_t *ns_second);
int ns_match_regex_domain(namespace_node_data_t *ns_regex,
	namespace_node_data_t *ns_domain);
int is_ns_domain_any(namespace_node_data_t *ns_node_data, int *result);

static int
insert_namespace(const char *ns, int *return_idx);

int
search_namespace(const char *ns, int *return_idx);

static void
insert_list_by_preference(namespace_node_data_t * pointer, int *return_idx);

static int
delete_namespace(int n_index);

static inline void
set_namespace(int n_index, namespace_node_data_t * pointer);

static void
update_namespace_list_by_precedence(int n_index);

static void
free_members(namespace_node_data_t * namespace);

/* Extern Variables */
extern md_client_context *nvsd_mcc;
extern policy_srvr_t *nvsd_mgmt_get_pe_srvr(const char *cp_name);

/* extern declarations */
extern int
search_ressource_mgr(const char *cpResourcePool, int *ret_idx);

/* Local Variables */
const char namespace_config_prefix[] = "/nkn/nvsd/namespace";
const char uri_config_prefix[] = "/nkn/nvsd/uri";
const char origin_fetch_config_prefix[] = "/nkn/nvsd/origin_fetch/config";
const char origin_fetch_content_type_config_prefix[] =
"/nkn/nvsd/origin_fetch/config/*/cache-age/content-type";
const char origin_fetch_expired_delivery_prefix[] =
"/nkn/nvsd/origin_fetch/config/*/serve-expired/enable";
const char namespace_origin_req_config_prefix[] =
"/nkn/nvsd/namespace/*/origin-request";
const char namespace_client_resp_config_prefix[] =
"/nkn/nvsd/namespace/*/client-response";
const char namespace_servermap_config_prefix[] =
"/nkn/nvsd/namespace/*/origin-server/http/server-map";
const char namespace_cluster_config_prefix[] = "/nkn/nvsd/namespace/*/cluster";
const char namespace_pe_config_prefix[] = "/nkn/nvsd/namespace/*/policy/server";
const char ns_cache_index_prefix[] =
"/nkn/nvsd/origin_fetch/config/*/cache-index";
const char namespace_filter_config_prefix[] = "/nkn/nvsd/namespace/*/client-request";

static namespace_node_data_t *pstNamespace = NULL;
static tbool dynamic_change = false;
static tbool immediate_update = false;

/* The struct namespace_list is defined in nvsd_mgmt_inline.h */
struct namespace_list g_lstNamespace;

/*
 * counter to track any deletion/addition of namespsaces. whenever a
 * namespace is added/deleted, this counter will track that operation.
 * This is monotonous increasing coutner. Everytime a addition/deletion
 * happens, counter will be increased by 1.
 *
 * Usage of Counter: This counter will be used by other daemons(agentd,
 * dashboard) to refresh their namespace names list. This will provide an
 * optimization over refreshing the namespaces name list every time.
 *
 * Its value provides is only used to tell that there is a change and used to
 * compare with previous value of it. If two values are different, then
 * daemons will refresh their list of namespace list.
 *
 * Note: this counter is updated when namespace counters are added/deleted
 * instead of configuration. namespace counters are created only when there
 * is traffic is coming to box.
 */
uint32_t glob_namespaces_changed = 0;

static pthread_mutex_t namespace_node_data_lock = PTHREAD_MUTEX_INITIALIZER;

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_mgmt_namespace_lock()
 *	purpose : set a pthread_mutex to block till a unlock call is made
 */
    inline void
nvsd_mgmt_namespace_lock()
{
    pthread_mutex_lock(&namespace_node_data_lock);
}	/* end of nvsd_mgmt_namespace_lock() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_mgmt_namespace_unlock()
 *	purpose : unlock the pthread_mutex 
 */
    inline void
nvsd_mgmt_namespace_unlock()
{
    pthread_mutex_unlock(&namespace_node_data_lock);
}	/* end of nvsd_mgmt_namespace_unlock() */

    void
nvsd_update_namespace_accesslog_profile_format(const char *profile_name,
	const char *profile_format,
	void *ns_node_data)
{
    int i;
    char record_history[] = "%e";
    char resp_header[] = "}o";
    char resp_md5_chksum_header[] = "X-NKN-MD5-Checksum";
    int record_cache_hit_history = 0;
    int resp_header_configured = 0;
    int resp_md5_chksum_configured = 0;
    namespace_node_data_t *curr_ns = (namespace_node_data_t *) ns_node_data;

    if (strstr(profile_format, record_history)) {
	record_cache_hit_history = 1;
    }

    if (strstr(profile_format, resp_header)) {
	resp_header_configured = 1;
	if (strstr(profile_format, resp_md5_chksum_header)) {
	    resp_md5_chksum_configured = 1;
	}
    }

    if (curr_ns != NULL) {
	/*
	 * If this function is called from namespace change event,
	 * there is no need to call nvsd_mgmt_namespace_lock(),
	 * nvsd_mgmt_namespace_unlock() and nkn_namespace_config_update()
	 */
	if (strcmp(curr_ns->access_log.name, profile_name) == 0) {
	    curr_ns->access_log.al_record_cache_history =
		record_cache_hit_history;
	    curr_ns->access_log.al_resp_header_configured =
		resp_header_configured;
	    curr_ns->access_log.al_resp_hdr_md5_chksum_configured =
		resp_md5_chksum_configured;
	}
	return;
    }

    nvsd_mgmt_namespace_lock();
    for (i = 0; i < g_lstNamespace.namespace_counter; i++) {
	curr_ns = get_namespace(i);
	if (strcmp(curr_ns->access_log.name, profile_name) == 0) {
	    curr_ns->access_log.al_record_cache_history =
		record_cache_hit_history;
	    curr_ns->access_log.al_resp_header_configured =
		resp_header_configured;
	    curr_ns->access_log.al_resp_hdr_md5_chksum_configured =
		resp_md5_chksum_configured;
	}
    }
    nvsd_mgmt_namespace_unlock();
    nkn_namespace_config_update(NULL);

    return;
}

/* ------------------------------------------------------------------------- */
int
nvsd_namespace_uf_cfg_query(const bn_binding_array *bindings)
{
    int err =0 ;
    lc_log_basic(LOG_NOTICE, "starting ns_uf");
    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    namespace_filter_config_prefix, NULL,
	    nvsd_filter_handle_change, NULL);
    bail_error(err);

bail:
    return err;
}

/* ------------------------------------------------------------------------- */
/*
 *	funtion : nvsd_namespace_cfg_query()
 *	purpose : query for namespace config parameters
 */
    int
nvsd_namespace_cfg_query(const bn_binding_array * bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    lc_log_basic(jnpr_log_level,
	    "nvsd namespace module mgmtd query initializing ..");

    /*
     * Initialize the Namespace list 
     */

    g_lstNamespace.namespace_counter = 0;

    /*
     * Bind to get NAMESPACE 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    namespace_config_prefix,
	    NULL,
	    nvsd_namespace_cfg_handle_change,
	    &rechecked_licenses);

    bail_error(err);

    /*
     * Bind to get URI-PREFIX 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    uri_config_prefix,
	    NULL,
	    nvsd_uri_cfg_handle_change,
	    &rechecked_licenses);

    bail_error(err);

    /*
     * Bind to get ORIGIN-FETCH 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    origin_fetch_config_prefix,
	    NULL,
	    nvsd_origin_fetch_cfg_handle_change,
	    &rechecked_licenses);

    bail_error(err);

    /*
     * Bind to get ORIGIN-FETCH/CACHE-AGE/CONTENT-TYPE 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    origin_fetch_content_type_config_prefix,
	    NULL,
	    nvsd_origin_fetch_cache_age_handle_change,
	    &rechecked_licenses);

    bail_error(err);

    /*
     * bind to the ORIGIN-REQUEST/HEADER/ADD 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    namespace_origin_req_config_prefix,
	    NULL,
	    nvsd_origin_request_header_handle_change,
	    &rechecked_licenses);

    bail_error(err);

    /*
     * bind to the CLIENT-RESPONSE/HEADER/ADD 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    namespace_client_resp_config_prefix,
	    NULL,
	    nvsd_client_response_header_handle_change,
	    &rechecked_licenses);

    bail_error(err);

    /*
     * bind to the CLIENT-RESPONSE/HEADER/DELETE 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    namespace_client_resp_config_prefix,
	    NULL,
	    nvsd_client_response_header_delete_handle_change,
	    &rechecked_licenses);

    bail_error(err);

    /*
     * Server-map 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    namespace_servermap_config_prefix,
	    NULL,
	    nvsd_ns_http_server_map_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    /*
     * Cluster 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    namespace_cluster_config_prefix,
	    NULL,
	    nvsd_ns_cluster_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    /*
     * include headers in cache-index 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    origin_fetch_content_type_config_prefix,
	    NULL,
	    nvsd_ns_cache_index_header_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    /*
     * Policy server 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    namespace_pe_config_prefix,
	    NULL,
	    nvsd_pe_srvr_handle_change,
	    &rechecked_licenses);
    bail_error(err);

bail:
    return err;

}	/* end of nvsd_namespace_cfg_query() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_namespace_module_cfg_handle_change()
 *	purpose : handler for config changes for namespace module
 */
    int
nvsd_namespace_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    /*
     * Reset the flag 
     */
    dynamic_change = false;
    immediate_update = false;

    /*
     * Lock first 
     */
    nvsd_mgmt_namespace_lock();

    /*
     * Call the callbacks for new bindings 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    namespace_config_prefix,
	    NULL,
	    nvsd_namespace_cfg_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    uri_config_prefix,
	    NULL,
	    nvsd_uri_cfg_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    origin_fetch_config_prefix,
	    NULL,
	    nvsd_origin_fetch_cfg_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    err = mgmt_lib_mdc_foreach_binding_prequeried(old_bindings,
	    "",
	    NULL,
	    nvsd_origin_fetch_cfg_delete_handle_change,
	    &rechecked_licenses);

    bail_error(err);

    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    origin_fetch_content_type_config_prefix,
	    NULL,
	    nvsd_origin_fetch_cache_age_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    // TODO:
    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    namespace_origin_req_config_prefix,
	    NULL,
	    nvsd_origin_request_header_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    namespace_client_resp_config_prefix,
	    NULL,
	    nvsd_client_response_header_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    namespace_client_resp_config_prefix,
	    NULL,
	    nvsd_client_response_header_delete_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    namespace_cluster_config_prefix,
	    NULL,
	    nvsd_ns_cluster_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    namespace_pe_config_prefix,
	    NULL,
	    nvsd_pe_srvr_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    err = mgmt_lib_mdc_foreach_binding_prequeried(old_bindings,
	    namespace_filter_config_prefix,
	    NULL,
	    nvsd_filter_handle_delete_change,
	    NULL);
    bail_error(err);

    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    namespace_filter_config_prefix,
	    NULL,
	    nvsd_filter_handle_change,
	    NULL);
    bail_error(err);

    err = mgmt_lib_mdc_foreach_binding_prequeried(old_bindings,
	    namespace_pe_config_prefix,
	    NULL,
	    nvsd_pe_srvr_delete_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    /*
     * Call the callbacks for old bindings 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(old_bindings,
	    namespace_servermap_config_prefix,
	    NULL,
	    nvsd_ns_http_server_map_handle_delete_change,
	    &rechecked_licenses);
    bail_error(err);

    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    namespace_servermap_config_prefix,
	    NULL,
	    nvsd_ns_http_server_map_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    err = mgmt_lib_mdc_foreach_binding_prequeried(old_bindings,
	    namespace_cluster_config_prefix,
	    NULL,
	    nvsd_ns_cluster_handle_delete_change,
	    &rechecked_licenses);
    bail_error(err);

    err = mgmt_lib_mdc_foreach_binding_prequeried(old_bindings,
	    namespace_config_prefix,
	    NULL,
	    nvsd_namespace_delete_cfg_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    err = mgmt_lib_mdc_foreach_binding_prequeried(old_bindings,
	    uri_config_prefix,
	    NULL,
	    nvsd_uri_delete_cfg_handle_change,
	    &rechecked_licenses);
    bail_error(err);
    /*
     * Call the callbacks for old bindings 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(old_bindings,
	    ns_cache_index_prefix,
	    NULL,
	    nvsd_ns_cache_index_header_handle_delete_change,
	    &rechecked_licenses);
    bail_error(err);

    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    ns_cache_index_prefix,
	    NULL,
	    nvsd_ns_cache_index_header_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    /*
     * Unlock when leaving 
     */
    nvsd_mgmt_namespace_unlock();

    /*
     * If relevant node has changed then indicate to the module 
     */
    if (dynamic_change)
	nkn_namespace_config_update(NULL);

    /*
     * Now if immediate update needed then call the trigger function 
     */
    if (immediate_update)
	immediate_namespace_update();

    return err;
bail:
    if (err) {
	/*
	 * Unlock when leaving 
	 */
	nvsd_mgmt_namespace_unlock();
    }
    return err;
}	/* end of nvsd_namespace_module_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_namespace_cfg_handle_change()
 *	purpose : handler for config changes for namespace module
 */
    static int
nvsd_namespace_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value, void *data)
{
    const tstring *name = NULL;
    const char *t_namespace = NULL;
    tstr_array *name_parts = NULL;
    tbool *rechecked_licenses_p = data;
    tbool change_in_precedence = false;
    pub_point_data_t *pPubPoint = NULL;
    const char *t_pub_point = NULL;
    char *acclog_name = NULL;
    char *vip = NULL;
    int err = 0, ret;
    int ns_idx = -1;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);

    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(bndgs_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /*
     * Check if this is our node 
     */
    if (bn_binding_name_pattern_match(ts_str(name), "/nkn/nvsd/namespace/**")) {

	/*-------- Get the Namespace ------------*/
	bn_binding_get_name_parts(binding, &name_parts);
	bail_error_null(err, name_parts);

	t_namespace = tstr_array_get_str_quick(name_parts, 3);
	bail_error(err);

	lc_log_basic(jnpr_log_level, "Read .../nkn/nvsd/namespace as : \"%s\"",
		t_namespace);

	/*
	 * Get the namespace entry in the global array 
	 */
	// commenting out since new logic added

	if (insert_namespace(t_namespace, &ns_idx) != 0)
	    goto bail;

	pstNamespace = get_namespace(ns_idx);

	if (NULL == pstNamespace->namespace) {
	    pstNamespace->namespace =
		nkn_strdup_type(t_namespace, mod_mgmt_charbuf);
	}
    } else {
	/*
	 * This is not the namespace node hence leave 
	 */
	goto bail;
    }

    if (bn_binding_name_pattern_match(ts_str(name),
		"/nkn/nvsd/namespace/*/pub-point/**")) {
	/*
	 * Get the pub point 
	 */
	t_pub_point = tstr_array_get_str_quick(name_parts, 5);

	bail_error(err);
	lc_log_basic(jnpr_log_level,
		"Read .../nkn/nvsd/namespace/%s/pub-point as : \"%s\"",
		t_namespace, t_pub_point);

	pPubPoint = getPubPoint(pstNamespace, t_pub_point);
	if (pPubPoint == NULL) {
	    int i = 0;

	    /*
	     * New pub point. Find an empty slot in the array 
	     */
	    for (i = 0; i < MAX_PUB_POINT; i++) {
		if (pstNamespace->rtsp_live_pub_point[i].name == NULL) {
		    pPubPoint = &pstNamespace->rtsp_live_pub_point[i];
		    break;
		}
	    }

	    if (i == MAX_PUB_POINT) {
		/*
		 * No free slot - This should NOT happen as the max_wc_count
		 * on the mgmtd module should allow more than MAX_PUB_POINT
		 * names. 
		 */
		goto bail;
	    }
	    pPubPoint->name = nkn_strdup_type(t_pub_point, mod_mgmt_charbuf);
	}
    }

    /*
     * Get the value of UID 
     */
    if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 2, "*", "uid")) {
	// if
	// (bn_binding_name_pattern_match(ts_str(name),"/nkn/nvsd/namespace/*/uid"))
	char *t_uid = NULL;
	int i;

	(void) i;

	err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &t_uid);
	bail_error(err);

	lc_log_basic(jnpr_log_level, "Read .../namespace/%s/uid as : \"%s\"",
		t_namespace, (t_uid) ? (t_uid) : "");

	if (!t_uid) {
	    /*
	     * UID from db will be NULL on a delete 
	     */
	    err = DM2_mgmt_ns_delete_namespace(pstNamespace->uid + 1);
	    if (err) {
		lc_log_basic(LOG_NOTICE, "Error deleting namespace stats");
		/*
		 * resetting the error, don't want inturrpt processing 
		 */
		err = 0;
	    }
	}
	/*
	 * Set uid entry 
	 */
	if (NULL != pstNamespace->uid)
	    safe_free(pstNamespace->uid);

	if (t_uid) {
	    pstNamespace->uid = nkn_strdup_type(t_uid, mod_mgmt_charbuf);
	    safe_free(t_uid);
	    // DM2 doesnt need the "/" in the UID
	    if (nkn_system_inited) {
		err = DM2_mgmt_ns_add_namespace(pstNamespace, ns_idx);
		bail_error(err);
	    }
	} else {
	    pstNamespace->uid = NULL;
	}
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 3, "*",
		"pinned_object",
		"auto_pin")) {
	tbool t_cache_auto_pin;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_cache_auto_pin);
	bail_error(err);
	pstNamespace->origin_fetch.object.cache_pin.cache_auto_pin =
	    t_cache_auto_pin;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 3, "*",
		"match", "precedence")) {
	uint32 t_precedence = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_precedence);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../namespace/%s/match/precedence as : \"%d\"",
		t_namespace, t_precedence);
	//pstNamespace->precedence = t_precedence;

	//move_namespace_in_list_by_precedence(ns_idx);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 2, "*",
		"cluster-hash")) {
	uint32 t_ch_type = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_ch_type);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../namespace/%s/cluster-hash as : \"%d\"",
		t_namespace, t_ch_type);
	pstNamespace->hash_url = t_ch_type;
    }

    /*
     * ! added domain/host and domain/host regex 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 3, "*",
		"domain", "host")) {
#if 0
	tstring *t_domain = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_domain);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../namespace/%s/domain/host as : \"%s\"",
		t_namespace, (t_domain) ? ts_str(t_domain) : "");

	if (NULL != pstNamespace->domain) {
	    safe_free(pstNamespace->domain);
	    // backward compatibility
	    safe_free(pstNamespace->uri_prefix[0].domain);
	}

	if (t_domain && ts_length(t_domain) > 0) {
	    pstNamespace->domain = nkn_strdup_type(ts_str(t_domain),
		    mod_mgmt_charbuf);
	    // backward compatibility
	    pstNamespace->uri_prefix[0].domain =
		nkn_strdup_type(ts_str(t_domain), mod_mgmt_charbuf);
	} else {
	    pstNamespace->domain = NULL;
	    // backward compatibility
	    pstNamespace->uri_prefix[0].domain = NULL;
	}
	move_namespace_in_list_by_precedence(ns_idx);
	ts_free(&t_domain);
#endif
    } else  if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 3,
		"*", "fqdn-list", "*")) {
	int operation  =  eFQDN_None;
	const char *domain_value = NULL;

	//lc_log_basic(LOG_NOTICE, "value: %s, last_part: %s",
		//ts_str_maybe_empty(bndgs_value), ts_str(bndgs_name_last_part));
	if (0 == strcmp(ts_str_maybe_empty(bndgs_value), ""))  {
	    /* domain has been deleted, now search and free mem */
	    operation = eFQDN_delete;
	} else {
	    operation = eFQDN_add;
	}
	if (0 == strcmp(ts_str_maybe_empty(bndgs_name_last_part), "any")) {
	    domain_value = "*";
	} else {
	    domain_value = ts_str(bndgs_name_last_part);
	}
	err = update_fqdn_list(pstNamespace, domain_value,
		operation);
	bail_error(err);

	err = ns_fqdn_fill_holes(pstNamespace);
	bail_error(err);

    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 2,"*",
		"ns-precedence")) {
	uint32 precedence = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &precedence);
	bail_error(err);

	pstNamespace->precedence = precedence;

	update_namespace_list_by_precedence(ns_idx);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 3,"*",
						    "domain", "regex")) {
    /* Get the value of URI-PREFIX DOMAIN REGEX */
    //else if (bn_binding_name_pattern_match(ts_str(name),
    //"/nkn/nvsd/namespace/*/domain/regex"))
	tstring *t_regex = NULL;
	char err_buffer[512];

	err = bn_binding_get_tstr(binding, ba_value, bt_regex, NULL, &t_regex);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../namespace/%s/domain/regex as : \"%s\"",
		t_namespace, t_regex ? ts_str(t_regex) : "");

	if (NULL != pstNamespace->domain_regex) {
	    safe_free(pstNamespace->domain_regex);
	    nkn_regex_ctx_free(pstNamespace->domain_comp_regex);
	    pstNamespace->domain_regex = NULL;
	    pstNamespace->domain_comp_regex = NULL;
	    // backward compatibility
	    safe_free(pstNamespace->uri_prefix[0].domain_regex);
	}

	if (t_regex && ts_length(t_regex) > 0) {
	    pstNamespace->domain_regex =
		nkn_strdup_type(ts_str(t_regex), mod_mgmt_charbuf);

	    // backward compatibility
	    pstNamespace->uri_prefix[0].domain_regex =
		nkn_strdup_type(ts_str(t_regex), mod_mgmt_charbuf);
	    pstNamespace->domain_comp_regex = nkn_regex_ctx_alloc();
	    if (!pstNamespace->domain_comp_regex) {
		// need to change this to lc_log
		lc_log_basic(LOG_INFO, "regex comp node allocation failed");
		err = lc_err_unexpected_null;
		bail_error(err);
	    }
	    if ((ret = nkn_regcomp(pstNamespace->domain_comp_regex,
			    pstNamespace->domain_regex,
			    err_buffer, sizeof (err_buffer))) != 0) {
		err_buffer[sizeof (err_buffer) - 1] = '\0';
		lc_log_basic(jnpr_log_level,
			"nkn comp failed ret = %d regex = %s error = %s",
			ret, pstNamespace->domain_regex, err_buffer);
		err = -1;
		goto bail;
	    }
	} else {
	    pstNamespace->uri_prefix[0].domain_regex = NULL;
	    safe_free(pstNamespace->domain_regex);
	    nkn_regex_ctx_free(pstNamespace->domain_comp_regex);
	    pstNamespace->domain_regex = NULL;
	    pstNamespace->domain_comp_regex = NULL;
	    // backward compatibility
	    safe_free(pstNamespace->uri_prefix[0].domain_regex);

	}
	//move_namespace_in_list_by_precedence(ns_idx);
	ts_free(&t_regex);
    }

    /*
     * Get the value of proxy_mode 
     */
    /*
     * ! need to remove the below proxy-mode pattern match instead 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 2, "*",
		"proxy-mode")) {
	char *t_proxy_mode = NULL;

	err = bn_binding_get_str(binding,
		ba_value, bt_string, NULL, &t_proxy_mode);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../namespace/%s/proxy-mode as : \"%s\"",
		t_namespace, t_proxy_mode ? (t_proxy_mode) : "");

	if (t_proxy_mode && strncmp(t_proxy_mode, "mid-tier", 8) == 0) {
	    pstNamespace->proxy_mode = PROXY_MID_TIER;
	} else if (t_proxy_mode && strncmp(t_proxy_mode, "forward", 7) == 0) {
	    pstNamespace->proxy_mode = PROXY_FORWARD;
	} else if (t_proxy_mode && strncmp(t_proxy_mode, "virtual", 7) == 0) {
	    pstNamespace->proxy_mode = PROXY_VIRTUAL_DOMAIN;
	} else if (t_proxy_mode && strncmp(t_proxy_mode,
		    "transparent", 11) == 0) {
	    pstNamespace->proxy_mode = PROXY_TRANSPARENT;
	} else {
	    pstNamespace->proxy_mode = PROXY_REVERSE;	// default
	}

	safe_free(t_proxy_mode);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 2, "*",
		"ip_tproxy")) {
	char *t_ipaddr = NULL;
	in_addr_t ipaddr;

	err = bn_binding_get_str(binding,
		ba_value, bt_ipv4addr, NULL, &t_ipaddr);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"............ read IP Address as : %s\n", t_ipaddr);
	ipaddr = inet_addr(t_ipaddr);

	// Network byte order
	pstNamespace->tproxy_ipaddr = (ipaddr);

	safe_free(t_ipaddr);
    }

    /*
     * Get the value of VIRTUAL PLAYER 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 2, "*",
		"virtual_player")) {
	char *t_virtual_player = NULL;
	int i = 0;

	(void) i;
	err = bn_binding_get_str(binding,
		ba_value, bt_string, NULL, &t_virtual_player);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../namespace/%s/virtual_player as : \"%s\"",
		t_namespace, t_virtual_player ? (t_virtual_player) : "");

	/*
	 * Set virtual player entry 
	 */
	if (NULL != pstNamespace->virtual_player_name)
	    safe_free(pstNamespace->virtual_player_name);

	if (t_virtual_player)
	    pstNamespace->virtual_player_name =
		nkn_strdup_type(t_virtual_player, mod_mgmt_charbuf);
	else
	    pstNamespace->virtual_player_name = NULL;

	/*
	 * Now if the virtual player has been created then set the ptr to it 
	 */
	pstNamespace->virtual_player =
	    nvsd_mgmt_get_virtual_player(t_virtual_player);

	if (t_virtual_player)
	    safe_free(t_virtual_player);
    }

    /*
     * Get the value of STATUS 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 3, "*",
		"status", "active")) {
	tbool t_active;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_active);
	bail_error(err);
	pstNamespace->active = t_active;

	// Call the DM2 API here to let them know a new namespace
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"delivery", "proto", "http")) {
    //else if (bn_binding_name_pattern_match(ts_str(name),
	//"/nkn/nvsd/namespace/*/delivery/proto/http"))
	tbool t_http;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_http);
	bail_error(err);
	pstNamespace->proto_http = t_http;
    }
    /*
     * Get the value of URI-PREFIX DELIVERY PROTO RTMP 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"delivery", "proto",
		"rtmp")) {
	tbool t_rtmp;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_rtmp);
	bail_error(err);
	pstNamespace->proto_rtmp = t_rtmp;
    }
    /*
     * Get the value of URI-PREFIX DELIVERY PROTO RTSP 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"delivery", "proto",
		"rtsp")) {
	tbool t_rtsp;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_rtsp);
	bail_error(err);
	pstNamespace->proto_rtsp = t_rtsp;
    }
    /*
     * Get the value of URI-PREFIX DELIVERY PROTO RTP 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"delivery", "proto",
		"rtp")) {
	tbool t_rtp;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_rtp);
	bail_error(err);
	pstNamespace->proto_rtp = t_rtp;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 3, "*",
		"match", "type")) {
	uint8 t_type = 0;

	err = bn_binding_get_uint8(binding, ba_value, NULL, &t_type);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../namespace/%s/match/type as : \"%d\"",
		t_namespace, t_type);

	pstNamespace->ns_match.type = t_type;

    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"match", "uri",
		"uri_name")) {
	tstring *t_uri = NULL;

	err = bn_binding_get_tstr(binding, ba_value, bt_uri, NULL, &t_uri);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../namespace/%s/match/uri/uri_name as : \"%s\"",
		t_namespace, (t_uri) ? ts_str(t_uri) : "");

	if (NULL != pstNamespace->ns_match.match.uri.uri_prefix) {
	    safe_free(pstNamespace->ns_match.match.uri.uri_prefix);
	    // backward compatibility
	    safe_free(pstNamespace->uri_prefix[0].uri_prefix);
	}

	if (t_uri && ts_length(t_uri) > 0) {
	    pstNamespace->ns_match.match.uri.uri_prefix =
		nkn_strdup_type(ts_str(t_uri), mod_mgmt_charbuf);
	    // backward compatibility
	    pstNamespace->uri_prefix[0].uri_prefix =
		nkn_strdup_type(ts_str(t_uri), mod_mgmt_charbuf);
	} else {
	    pstNamespace->ns_match.match.uri.uri_prefix = NULL;
	    // backward compatibility
	    pstNamespace->uri_prefix[0].uri_prefix = NULL;
	}

	ts_free(&t_uri);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"match", "uri",
		"regex")) {
	tstring *t_uri_regex = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_regex, NULL, &t_uri_regex);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../namespace/%s/match/uri/regex as : \"%s\"",
		t_namespace, (t_uri_regex) ? ts_str(t_uri_regex) : "");

	if (NULL != pstNamespace->ns_match.match.uri.uri_prefix_regex)
	    safe_free(pstNamespace->ns_match.match.uri.uri_prefix_regex);

	if (t_uri_regex && ts_length(t_uri_regex) > 0) {
	    pstNamespace->ns_match.match.uri.uri_prefix_regex =
		nkn_strdup_type(ts_str(t_uri_regex), mod_mgmt_charbuf);
	} else
	    pstNamespace->ns_match.match.uri.uri_prefix_regex = NULL;

	ts_free(&t_uri_regex);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"match", "header",
		"name")) {
	tstring *t_header_name = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_header_name);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../namespace/%s/match/header/name as : \"%s\"",
		t_namespace, (t_header_name) ? ts_str(t_header_name) : "");

	if (NULL != pstNamespace->ns_match.match.header.name)
	    safe_free(pstNamespace->ns_match.match.header.name);

	if (t_header_name && ts_length(t_header_name) > 0) {
	    pstNamespace->ns_match.match.header.name =
		nkn_strdup_type(ts_str(t_header_name), mod_mgmt_charbuf);
	} else
	    pstNamespace->ns_match.match.header.name = NULL;
	ts_free(&t_header_name);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"match", "header",
		"value")) {
	tstring *t_header_value = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_header_value);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../namespace/%s/match/header/value as : \"%s\"",
		t_namespace,
		(t_header_value) ? ts_str(t_header_value) : "");

	if (NULL != pstNamespace->ns_match.match.header.value)
	    safe_free(pstNamespace->ns_match.match.header.value);

	if (t_header_value && ts_length(t_header_value) > 0) {
	    pstNamespace->ns_match.match.header.value =
		nkn_strdup_type(ts_str(t_header_value), mod_mgmt_charbuf);
	} else
	    pstNamespace->ns_match.match.header.value = NULL;
	ts_free(&t_header_value);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"match", "header",
		"regex")) {
	tstring *t_header_regex = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_regex, NULL, &t_header_regex);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../namespace/%s/match/header/regex as : \"%s\"",
		t_namespace,
		(t_header_regex) ? ts_str(t_header_regex) : "");

	if (NULL != pstNamespace->ns_match.match.header.regex)
	    safe_free(pstNamespace->ns_match.match.header.regex);

	if (t_header_regex && ts_length(t_header_regex) > 0) {
	    pstNamespace->ns_match.match.header.regex =
		nkn_strdup_type(ts_str(t_header_regex), mod_mgmt_charbuf);
	} else
	    pstNamespace->ns_match.match.header.regex = NULL;

	ts_free(&t_header_regex);

    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"match", "query-string",
		"name")) {
	tstring *t_qstring_name = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_qstring_name);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../namespace/%s/match/query-string/name as : \"%s\"",
		t_namespace,
		(t_qstring_name) ? ts_str(t_qstring_name) : "");

	if (NULL != pstNamespace->ns_match.match.qstring.name)
	    safe_free(pstNamespace->ns_match.match.qstring.name);

	if (t_qstring_name && ts_length(t_qstring_name) > 0) {
	    pstNamespace->ns_match.match.qstring.name =
		nkn_strdup_type(ts_str(t_qstring_name), mod_mgmt_charbuf);
	} else
	    pstNamespace->ns_match.match.qstring.name = NULL;

	ts_free(&t_qstring_name);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"match", "query-string",
		"value")) {
	tstring *t_qstring_value = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_qstring_value);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../namespace/%s/match/query-string/value as : \"%s\"",
		t_namespace,
		(t_qstring_value) ? ts_str(t_qstring_value) : "");

	if (NULL != pstNamespace->ns_match.match.qstring.value)
	    safe_free(pstNamespace->ns_match.match.qstring.value);

	if (t_qstring_value && ts_length(t_qstring_value) > 0) {
	    pstNamespace->ns_match.match.qstring.value =
		nkn_strdup_type(ts_str(t_qstring_value), mod_mgmt_charbuf);
	} else
	    pstNamespace->ns_match.match.qstring.value = NULL;

	ts_free(&t_qstring_value);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"match", "query-string",
		"regex")) {
	tstring *t_qstring_regex = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_regex, NULL, &t_qstring_regex);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../namespace/%s/match/query-string/regex as : \"%s\"",
		t_namespace,
		(t_qstring_regex) ? ts_str(t_qstring_regex) : "");

	if (NULL != pstNamespace->ns_match.match.qstring.regex)
	    safe_free(pstNamespace->ns_match.match.qstring.regex);

	if (t_qstring_regex && ts_length(t_qstring_regex) > 0) {
	    pstNamespace->ns_match.match.qstring.regex =
		nkn_strdup_type(ts_str(t_qstring_regex), mod_mgmt_charbuf);
	} else
	    pstNamespace->ns_match.match.qstring.regex = NULL;

	ts_free(&t_qstring_regex);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"match", "virtual-host",
		"ip")) {
	tstring *t_fqdn = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_ipv4addr, NULL, &t_fqdn);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../namespace/%s/match/virtualhost/ip as : \"%s\"",
		t_namespace, (t_fqdn) ? ts_str(t_fqdn) : "");

	if (NULL != pstNamespace->ns_match.match.virhost.fqdn)
	    safe_free(pstNamespace->ns_match.match.virhost.fqdn);

	if (t_fqdn && ts_length(t_fqdn) > 0) {
	    pstNamespace->ns_match.match.virhost.fqdn =
		nkn_strdup_type(ts_str(t_fqdn), mod_mgmt_charbuf);
	} else
	    pstNamespace->ns_match.match.virhost.fqdn = NULL;

	ts_free(&t_fqdn);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"match", "virtual-host",
		"port")) {
	uint16 t_port = 0;

	err = bn_binding_get_uint16(binding, ba_value, NULL, &t_port);

	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../namespace/%s/match/virtualhost/port as : \"%d\"",
		t_namespace, t_port);

	pstNamespace->ns_match.match.virhost.port = t_port;
    }

    /*
     * Get the value of URI-PREFIX ORIGIN-SERVER NFS HOST 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 3, "*",
		"origin-server", "type")) {
	uint8 t_type = 0;

	err = bn_binding_get_uint8(binding, ba_value, NULL, &t_type);

	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../namespace/%s/origin-server/type as : \"%d\"",
		t_namespace, t_type);

	pstNamespace->origin_server.proto = t_type;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"origin-server", "nfs",
		"host")) {
	const char *cp_nfs_host = NULL;
	tstring *t_nfs_host = NULL;

	/*-------- Get the nfs host  ------------*/
	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_nfs_host);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../namespace/%s/origin-server/nfs/host as : \"%s\"",
		t_namespace, (cp_nfs_host) ? ts_str(t_nfs_host) : "");

	if (NULL != pstNamespace->origin_server.nfs.host) {
	    safe_free(pstNamespace->origin_server.nfs.host);
	    // backward compatibility
	    safe_free(pstNamespace->uri_prefix[0].origin_server_nfs[0].host);
	}

	if ((NULL != t_nfs_host) && ts_length(t_nfs_host) > 0) {
	    pstNamespace->origin_server.nfs.host =
		nkn_strdup_type(ts_str(t_nfs_host), mod_mgmt_charbuf);
	    // backward compatibility
	    pstNamespace->uri_prefix[0].origin_server_nfs[0].host =
		nkn_strdup_type(ts_str(t_nfs_host), mod_mgmt_charbuf);
	} else {
	    pstNamespace->origin_server.nfs.host = NULL;
	    // backward compatibility
	    pstNamespace->uri_prefix[0].origin_server_nfs[0].host = NULL;
	}

	// backward compatibility
	/*
	 * Set the NFS enum only if host entry is not empty 
	 */
	if (cp_nfs_host && ('\0' != cp_nfs_host[0])) {
	    // backward compatibility
	    // - Set from origin-server/type
	}

	ts_free(&t_nfs_host);
    }

    /*
     * Get the value of URI-PREFIX ORIGIN-SERVER NFS PORT 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"origin-server", "nfs",
		"port")) {
	uint16 t_port = 0;

	err = bn_binding_get_uint16(binding, ba_value, NULL, &t_port);

	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../namespace/%s/origin-server/nfs/port as : \"%d\"",
		t_namespace, t_port);
	pstNamespace->origin_server.nfs.port = t_port;
	// backward compatibility
	pstNamespace->uri_prefix[0].origin_server_nfs[0].port = t_port;
    }

    /*
     * Get the value of URI-PREFIX ORIGIN-SERVER NFS LOCAL_CACHE FLAG 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"origin-server", "nfs",
		"local-cache")) {
	tbool t_local_cache;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_local_cache);
	bail_error(err);
	pstNamespace->origin_server.nfs.local_cache = t_local_cache;
	// backward compatibility
	pstNamespace->uri_prefix[0].origin_server_nfs[0].local_cache =
	    t_local_cache;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"origin-server", "http",
		"absolute-url")) {
	tbool t_absolute_url;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_absolute_url);
	bail_error(err);

	pstNamespace->origin_server.absolute_url = t_absolute_url;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"origin-server", "http",
		"ip-version")) {
	tstring *t_sversion = NULL;

	/*
	 * t_sversion will be allocated, so free this memory 
	 */
	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_sversion);
	bail_error(err);

	if (t_sversion && (0 == strcmp(ts_str(t_sversion), "force-ipv4"))) {
	    pstNamespace->origin_server.ip_version = FORCE_IPV4;
	} else if (t_sversion &&
		(0 == strcmp(ts_str(t_sversion), "force-ipv6"))) {
	    pstNamespace->origin_server.ip_version = FORCE_IPV6;
	} else {
	    pstNamespace->origin_server.ip_version = FOLLOW_CLIENT;
	}
	ts_free(&t_sversion);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"origin-server", "http",
		"idle-timeout")) {
	uint32 t_idle_timeout = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_idle_timeout);

	bail_error(err);
	lc_log_basic(jnpr_log_level,
		"Read .../namespace/%s/origin-server/http/idle-timeout as : \"%d\"",
		t_namespace, t_idle_timeout);
	pstNamespace->origin_server.http_idle_timeout = t_idle_timeout;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 5, "*",
		"origin-server", "http",
		"follow", "header")) {
	tstring *t_header = NULL;
	const char *header = NULL;

	/*-------- Get the nfs host  ------------*/
	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_header);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../namespace/%s/origin-server/http/follow/header as : \"%s\"",
		t_namespace, header ? ts_str(t_header) : "");

	if (pstNamespace->origin_server.follow.dest_header)
	    safe_free(pstNamespace->origin_server.follow.dest_header);

	if ((NULL != t_header) && ts_length(t_header) > 0) {
	    pstNamespace->origin_server.follow.dest_header =
		nkn_strdup_type(ts_str(t_header), mod_mgmt_charbuf);
	} else {
	    pstNamespace->origin_server.follow.dest_header = NULL;
	}
	ts_free(&t_header);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 6,"*",
						    "origin-server", "http",
						    "follow", "aws", "access-key")) {
	tstring	 *t_access_key = NULL;

	err = bn_binding_get_tstr(binding,
				  ba_value, bt_string, NULL,
				  &t_access_key);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
	   "Read .../namespace/%s/origin-server/http/follow/aws/access-key as : \"%s\"",
		     t_namespace, t_access_key ? ts_str(t_access_key): "" );

	if (pstNamespace->origin_server.follow.aws.access_key)
	    safe_free(pstNamespace->origin_server.follow.aws.access_key);

	if ((NULL != t_access_key) && ts_length(t_access_key) > 0){
	    pstNamespace->origin_server.follow.aws.access_key =
		nkn_strdup_type(ts_str(t_access_key), mod_mgmt_charbuf);
	} else {
	    pstNamespace->origin_server.follow.aws.access_key = NULL;
	}
	ts_free(&t_access_key);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 6,"*",
						    "origin-server", "http",
						    "follow", "aws", "secret-key")) {
	tstring	 *t_secret_key = NULL;

	err = bn_binding_get_tstr(binding,
				  ba_value, bt_string, NULL,
				  &t_secret_key);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
	   "Read .../namespace/%s/origin-server/http/follow/aws/secret-key as : \"%s\"",
		     t_namespace, t_secret_key ? ts_str(t_secret_key): "" );

	if (pstNamespace->origin_server.follow.aws.secret_key)
	    safe_free(pstNamespace->origin_server.follow.aws.secret_key);

	if ((NULL != t_secret_key) && ts_length(t_secret_key) > 0){
	    pstNamespace->origin_server.follow.aws.secret_key =
		nkn_strdup_type(ts_str(t_secret_key), mod_mgmt_charbuf);
	} else {
	    pstNamespace->origin_server.follow.aws.secret_key = NULL;
	}
	ts_free(&t_secret_key);
	if (pstNamespace->origin_server.follow.aws.access_key &&
		pstNamespace->origin_server.follow.aws.secret_key) {
	    pstNamespace->origin_server.follow.aws.active = 1;
	} else {
	    pstNamespace->origin_server.follow.aws.active = 0;
	}
    }
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 5,"*",
						    "origin-server", "http",
						    "follow", "dns-query")) {
	tstring	 *t_dns_query = NULL;

	err = bn_binding_get_tstr(binding,
				  ba_value, bt_string, NULL,
				  &t_dns_query);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
	   "Read .../namespace/%s/origin-server/http/follow/dns-query as : \"%s\"",
		     t_namespace, t_dns_query ? ts_str(t_dns_query): "" );

	if (pstNamespace->origin_server.follow.dns_query)
	    safe_free(pstNamespace->origin_server.follow.dns_query);

	if ((NULL != t_dns_query) && ts_length(t_dns_query) > 0){
	    pstNamespace->origin_server.follow.dns_query =
		nkn_strdup_type(ts_str(t_dns_query), mod_mgmt_charbuf);
	} else {
	    pstNamespace->origin_server.follow.dns_query = NULL;
	}
	ts_free(&t_dns_query);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"origin-server", "nfs",
		"server-map")) {
	char *t_server_map = NULL;

	err = bn_binding_get_str(binding,
		ba_value, bt_string, NULL, &t_server_map);
	bail_error(err);

	if (pstNamespace->origin_server.nfs_server_map != NULL) {
	    pstNamespace->origin_server.nfs_server_map = NULL;
	    safe_free(pstNamespace->origin_server.nfs_server_map_name);
	}

	if ((NULL != t_server_map) && strlen(t_server_map) > 0) {
	    pstNamespace->origin_server.nfs_server_map =
		nvsd_mgmt_get_server_map(t_server_map);
	    pstNamespace->origin_server.nfs_server_map_name =
		nkn_strdup_type(t_server_map, mod_mgmt_charbuf);

	    // backward compatibility
	    pstNamespace->uri_prefix[0].server_map =
		nvsd_mgmt_get_server_map(t_server_map);

	    // backward compatibility
	    pstNamespace->uri_prefix[0].server_map_name =
		nkn_strdup_type(t_server_map, mod_mgmt_charbuf);

	    /*
	     * Set the immediate update flag only if namespace is active 
	     */
	    if (pstNamespace->active)
		immediate_update = true;
	} else {
	    pstNamespace->origin_server.nfs_server_map = NULL;
	    pstNamespace->origin_server.nfs_server_map_name = NULL;
	}

	if (t_server_map) {
	    safe_free(t_server_map);
	}
    }

    /*
     * Get the value of URI-PREFIX ORIGIN-SERVER HTTP HOST 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 5, "*",
		"origin-server", "http",
		"host", "name")) {
	char *t_http_host = NULL;
	int j = 0;

	/*-------- Get the http host  ------------*/
	err = bn_binding_get_str(binding,
		ba_value, bt_string, NULL, &t_http_host);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../namespace/%s/origin-server/http/host/name as : \"%s\"",
		t_namespace, (t_http_host) ? (t_http_host) : "");

	if ((NULL != pstNamespace->origin_server.http.host)
		&& (NULL != pstNamespace->uri_prefix[0].origin_server_http[j].host)) {
	    /*
	     * This is not the same host as the old one 
	     */
	    safe_free(pstNamespace->origin_server.http.host);
	    pstNamespace->origin_server.http.host = NULL;

	    // backward compatibility
	    safe_free(pstNamespace->uri_prefix[0].origin_server_http[j].host);
	    pstNamespace->uri_prefix[0].origin_server_http[j].host = NULL;
	}

	if ((NULL == pstNamespace->origin_server.http.host)
		&& (NULL != t_http_host)
		&& (strlen(t_http_host) > 0)) {

	    pstNamespace->origin_server.http.host =
		nkn_strdup_type(t_http_host, mod_mgmt_charbuf);

	    // backward compatibility
	    pstNamespace->uri_prefix[0].origin_server_http[j].host =
		nkn_strdup_type(t_http_host, mod_mgmt_charbuf);
	}

	if (t_http_host) {
	    safe_free(t_http_host);
	}
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 5, "*",
		"origin-server", "http",
		"host", "port")) {
	uint16 t_port = 0;

	err = bn_binding_get_uint16(binding, ba_value, NULL, &t_port);

	bail_error(err);
	lc_log_basic(jnpr_log_level,
		"Read .../namespace/%s/origin-server/http/host/port as : \"%d\"",
		t_namespace, t_port);
	pstNamespace->origin_server.http.port = t_port;

	// backward compatibility
	pstNamespace->uri_prefix[0].origin_server_http[0].port = t_port;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 6,"*",
						    "origin-server", "http",
						    "host", "aws", "access-key")) {
	tstring	 *t_access_key = NULL;

	err = bn_binding_get_tstr(binding,
				  ba_value, bt_string, NULL,
				  &t_access_key);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
	   "Read .../namespace/%s/origin-server/http/host/aws/access-key as : \"%s\"",
		     t_namespace, t_access_key ? ts_str(t_access_key): "" );

	if (pstNamespace->origin_server.http.aws.access_key) {
	    safe_free(pstNamespace->origin_server.http.aws.access_key);
	    pstNamespace->origin_server.http.aws.access_key = NULL;
	}

	if (pstNamespace->uri_prefix [0].origin_server_http[0].aws.access_key) {
	    safe_free(pstNamespace->uri_prefix[0].origin_server_http[0].aws.access_key);
	    pstNamespace->uri_prefix[0].origin_server_http[0].aws.access_key = NULL;
	}

	if ((NULL != t_access_key) && ts_length(t_access_key) > 0){
	    pstNamespace->origin_server.http.aws.access_key =
		nkn_strdup_type(ts_str(t_access_key), mod_mgmt_charbuf);
	    pstNamespace->uri_prefix[0].origin_server_http[0].aws.access_key =
		nkn_strdup_type(ts_str(t_access_key), mod_mgmt_charbuf);
	}
	ts_free(&t_access_key);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 6,"*",
						    "origin-server", "http",
						    "host", "aws", "secret-key")) {
	tstring	 *t_secret_key = NULL;

	err = bn_binding_get_tstr(binding,
				  ba_value, bt_string, NULL,
				  &t_secret_key);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
	   "Read .../namespace/%s/origin-server/http/host/aws/secret-key as : \"%s\"",
		     t_namespace, t_secret_key ? ts_str(t_secret_key): "" );

	if (pstNamespace->origin_server.http.aws.secret_key) {
	    safe_free(pstNamespace->origin_server.http.aws.secret_key);
	    pstNamespace->origin_server.http.aws.secret_key = NULL;
	}

	if (pstNamespace->uri_prefix [0].origin_server_http[0].aws.secret_key) {
	    safe_free(pstNamespace->uri_prefix[0].origin_server_http[0].aws.secret_key);
	    pstNamespace->uri_prefix[0].origin_server_http[0].aws.secret_key = NULL;
	}

	if ((NULL != t_secret_key) && ts_length(t_secret_key) > 0){
	    pstNamespace->origin_server.http.aws.secret_key =
		nkn_strdup_type(ts_str(t_secret_key), mod_mgmt_charbuf);
	    pstNamespace->uri_prefix[0].origin_server_http[0].aws.secret_key =
		nkn_strdup_type(ts_str(t_secret_key), mod_mgmt_charbuf);
	}
	ts_free(&t_secret_key);

	if (pstNamespace->origin_server.http.aws.access_key &&
		    pstNamespace->origin_server.http.aws.secret_key) {
		pstNamespace->origin_server.http.aws.active = 1;
	} else {
		pstNamespace->origin_server.http.aws.active = 0;
	}
    }
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 5,"*",
						    "origin-server", "http",
						    "host", "dns-query")) {
	tstring	 *t_dns_query = NULL;

	err = bn_binding_get_tstr(binding,
				  ba_value, bt_string, NULL,
				  &t_dns_query);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
	   "Read .../namespace/%s/origin-server/http/host/dns-query as : \"%s\"",
		     t_namespace, t_dns_query ? ts_str(t_dns_query): "" );

	if (pstNamespace->origin_server.http.dns_query) {
	    safe_free(pstNamespace->origin_server.http.dns_query);
	    pstNamespace->origin_server.http.dns_query = NULL;
	}

	if (pstNamespace->uri_prefix [0].origin_server_http[0].dns_query) {
	    safe_free(pstNamespace->uri_prefix[0].origin_server_http[0].dns_query);
	    pstNamespace->uri_prefix[0].origin_server_http[0].dns_query = NULL;
	}

	if ((NULL != t_dns_query) && ts_length(t_dns_query) > 0){
	    pstNamespace->origin_server.http.dns_query =
		nkn_strdup_type(ts_str(t_dns_query), mod_mgmt_charbuf);
	    pstNamespace->uri_prefix[0].origin_server_http[0].dns_query =
		nkn_strdup_type(ts_str(t_dns_query), mod_mgmt_charbuf);
	}
	ts_free(&t_dns_query);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 5, "*",
		"origin-server", "http",
		"follow", "dest-ip")) {
	tbool t_follow_dest_ip = false;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_follow_dest_ip);
	bail_error(err);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 5, "*",
		"origin-server", "http",
		"follow",
		"use-client-ip")) {
	tbool t_use_client_ip = false;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_use_client_ip);
	bail_error(err);

	pstNamespace->origin_server.http_use_client_ip = t_use_client_ip;
    }

    /*
     * Get the value of RTSP HOST NAME 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 5, "*",
		"origin-server", "rtsp",
		"host", "name")) {
	char *t_rtsp_host = NULL;

	/*
	 * Get the rtsp host 
	 */
	err = bn_binding_get_str(binding,
		ba_value, bt_string, NULL, &t_rtsp_host);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../namespace/%s/origin-server/rtsp/host/name as : \"%s\"",
		t_namespace, (t_rtsp_host) ? t_rtsp_host : "");

	if (NULL != pstNamespace->origin_server.rtsp.host) {
	    safe_free(pstNamespace->origin_server.rtsp.host);
	    pstNamespace->origin_server.rtsp.host = NULL;
	}

	if ((NULL == pstNamespace->origin_server.rtsp.host)
		&& (NULL != t_rtsp_host)
		&& (strlen(t_rtsp_host) > 0)) {
	    pstNamespace->origin_server.rtsp.host =
		nkn_strdup_type(t_rtsp_host, mod_mgmt_charbuf);
	}

	if (t_rtsp_host) {
	    safe_free(t_rtsp_host);
	}
    }

    /*
     * Get the value of RTSP HOST PORT 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 5, "*",
		"origin-server", "rtsp",
		"host", "port")) {
	uint16 t_port = 0;

	err = bn_binding_get_uint16(binding, ba_value, NULL, &t_port);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../namespace/%s/origin-server/rtsp/host/port as : \"%d\"",
		t_namespace, t_port);

	pstNamespace->origin_server.rtsp.port = t_port;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 5, "*",
		"origin-server", "rtsp",
		"host", "transport")) {
	char *t_rtsp_transport = NULL;

	/*
	 * Get the rtsp host 
	 */
	err = bn_binding_get_str(binding,
		ba_value, bt_string, NULL, &t_rtsp_transport);
	bail_error(err);

	if (t_rtsp_transport) {
	    lc_log_basic(jnpr_log_level,
		    "Read .../namespace/%s/origin-server/rtsp/host/transport as : \"%s\"",
		    t_namespace, t_rtsp_transport);

	    if (!strcmp(t_rtsp_transport, "rtp-udp")) {
		pstNamespace->origin_server.rtsp.transport = RTSP_TRANSPORT_UDP;
	    } else if (!strcmp(t_rtsp_transport, "rtp-rtsp")) {
		pstNamespace->origin_server.rtsp.transport =
		    RTSP_TRANSPORT_RTSP;
	    } else {
		pstNamespace->origin_server.rtsp.transport =
		    RTSP_TRANSPORT_CLIENT;
	    }
	}
	if (t_rtsp_transport)
	    safe_free(t_rtsp_transport);
    }

    /*
     * Get the value of RTSP ALTERNATE HTTP PORT 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 6, "*",
		"origin-server", "rtsp",
		"alternate", "http",
		"port")) {
	uint16 t_port = 0;

	err = bn_binding_get_uint16(binding, ba_value, NULL, &t_port);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../namespace/%s/origin-server/rtsp/alternate/http/port as : \"%d\"",
		t_namespace, t_port);

	pstNamespace->origin_server.rtsp.alternate_http_port = t_port;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 5, "*",
		"origin-server", "rtsp",
		"follow", "dest-ip")) {
	tbool t_follow_dest_ip = false;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_follow_dest_ip);
	bail_error(err);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 5, "*",
		"origin-server", "rtsp",
		"follow",
		"use-client-ip")) {
	tbool t_use_client_ip = false;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_use_client_ip);
	bail_error(err);

	pstNamespace->origin_server.rtsp_use_client_ip = t_use_client_ip;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"pub-point", "*",
		"active")) {
	tbool is_active = false;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &is_active);
	bail_error(err);

	pPubPoint->enable = is_active;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"pub-point", "*",
		"mode")) {
	char *mode = NULL;

	err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &mode);
	bail_error(err);

	if (mode) {
	    if (strcmp(mode, "on-demand") == 0) {
		pPubPoint->mode = PUB_SERVER_MODE_ON_DEMAND;
	    } else if (strcmp(mode, "immediate") == 0) {
		pPubPoint->mode = PUB_SERVER_MODE_IMMEDIATE;
	    } else if (strcmp(mode, "start-time") == 0) {
		pPubPoint->mode = PUB_SERVER_MODE_TIMED;
	    }
	} else {
	    pPubPoint->mode = PUB_SERVER_MODE_NONE;
	}
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"pub-point", "*",
		"sdp")) {
	char *t_sdp = NULL;

	err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &t_sdp);
	bail_error(err);

	if (NULL != pPubPoint->sdp_static) {
	    safe_free(pPubPoint->sdp_static);
	    pPubPoint->sdp_static = NULL;
	}

	if (t_sdp && (strlen(t_sdp) > 1)) {
	    pPubPoint->sdp_static = nkn_strdup_type(t_sdp, mod_mgmt_charbuf);
	}
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"pub-point", "*",
		"start-time")) {
	char *t_start_time = NULL;

	err = bn_binding_get_str(binding,
		ba_value, bt_string, NULL, &t_start_time);
	bail_error(err);

	if (NULL != pPubPoint->start_time) {
	    safe_free(pPubPoint->start_time);
	    pPubPoint->start_time = NULL;
	}

	if (t_start_time && (strlen(t_start_time) > 1)) {
	    pPubPoint->start_time =
		nkn_strdup_type(t_start_time, mod_mgmt_charbuf);
	}
	safe_free(t_start_time);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"pub-point", "*",
		"end-time")) {
	char *t_end_time = NULL;

	err = bn_binding_get_str(binding,
		ba_value, bt_string, NULL, &t_end_time);
	bail_error(err);

	if (NULL != pPubPoint->end_time) {
	    safe_free(pPubPoint->end_time);
	    pPubPoint->end_time = NULL;
	}

	if (t_end_time && (strlen(t_end_time) > 1)) {
	    pPubPoint->end_time = nkn_strdup_type(t_end_time, mod_mgmt_charbuf);
	}
	safe_free(t_end_time);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 5, "*",
		"pub-point", "*",
		"caching", "enable")) {
	tbool t_enable = false;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_enable);
	bail_error(err);

	pPubPoint->caching_enabled = t_enable;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 6, "*",
		"pub-point", "*",
		"caching", "format",
		"chunked-ts")) {
	tbool t_enable = false;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_enable);
	bail_error(err);

	pPubPoint->cache_format_chunked_ts = t_enable;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 6, "*",
		"pub-point", "*",
		"caching", "format",
		"frag-mp4")) {
	tbool t_enable = false;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_enable);
	bail_error(err);

	pPubPoint->cache_format_frag_mp4 = t_enable;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 6, "*",
		"pub-point", "*",
		"caching", "format",
		"moof")) {
	tbool t_enable = false;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_enable);
	bail_error(err);

	pPubPoint->cache_format_moof = t_enable;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 6, "*",
		"pub-point", "*",
		"caching", "format",
		"chunk-flv")) {
	tbool t_enable = false;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_enable);
	bail_error(err);

	pPubPoint->cache_format_chunk_flv = t_enable;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 3, "*",
		"pub-point", "*")) {
	char *t_name = NULL;

	err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &t_name);
	bail_error(err);

	if (pPubPoint && (pPubPoint->name != NULL)) {
	    safe_free(pPubPoint->name);
	    pPubPoint->name = NULL;
	}

	if (t_name && pPubPoint && (strlen(t_name) > 0)) {
	    pPubPoint->name = nkn_strdup_type(t_name, mod_mgmt_charbuf);
	}
	safe_free(t_name);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 6, "*",
		"resource-pool", "limit",
		"http", "max-session",
		"count")) {
	uint32_t t_count = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_count);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../namespace/%s/resource-pool/limit/http/max-session/count as : \"%d\"",
		t_namespace, t_count);

	pstNamespace->http_max_conns = t_count;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 6, "*",
		"resource-pool", "limit",
		"http", "max-session",
		"retry-after")) {
	uint32_t t_count = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_count);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../namespace/%s/resource-pool/limit/http/max-session/retry-afteras : \"%d\"",
		t_namespace, t_count);

	pstNamespace->http_retry_after_sec = t_count;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 6, "*",
		"resource-pool", "limit",
		"rtsp", "max-session",
		"count")) {
	uint32_t t_count = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_count);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../namespace/%s/resource-pool/limit/rtsp/max-session/count as : \"%d\"",
		t_namespace, t_count);

	pstNamespace->rtsp_max_conns = t_count;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 6, "*",
		"resource-pool", "limit",
		"rtsp", "max-session",
		"retry-after")) {
	uint32_t t_count = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_count);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../namespace/%s/resource-pool/limit/rtsp/max-session/retry-afteras : \"%d\"",
		t_namespace, t_count);

	pstNamespace->rtsp_retry_after_sec = t_count;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 3, "*",
		"policy", "file")) {
	char *t_file = NULL;
	char *t_file_full_path = NULL;

	err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &t_file);
	bail_error(err);

	if (t_file)
	    t_file_full_path =
		smprintf("/nkn/policy_engine/%s.tcl.com", t_file);

	if (NULL != pstNamespace->policy_engine.policy_file) {
	    safe_free(pstNamespace->policy_engine.policy_file);
	    pstNamespace->policy_engine.policy_file = NULL;
	}

	if (t_file && (strlen(t_file) > 1) && t_file_full_path) {
	    pstNamespace->policy_engine.policy_file =
		nkn_strdup_type(t_file_full_path, mod_mgmt_charbuf);
	}
	safe_free(t_file);
	safe_free(t_file_full_path);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 3, "*",
		"policy", "time_stamp")) {
	/*
	 * Do nothing here as of now 
	 */
	/*
	 * Ideally this time stamp should be used to revlaidate the tcl
	 * interpretter context 
	 */

    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 2, "*",
		"resource_pool")) {
	char *t_pool = NULL;
	int ret_idx = 0;
	int unbind_flag = 0;

	err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &t_pool);
	bail_error(err);

	/*
	 * if there is a resource pool bind event then the state changes
	 * possible for this node are a. global to a resource pool if there
	 * is a resource pool unbind event then the stte changes possible for 
	 * this node are a. change from a resource pool to the default global 
	 * pool the code fragment here first determines if the state changes
	 * from a resource pool to the global pool and then increments/
	 * decrements the availability counter (num_ns_bound) 
	 */
	if (t_pool)
	    unbind_flag = !(strcmp(t_pool, "global_pool"));

	if (t_pool && !unbind_flag) {
	    /*
	     * bind event: decrement global pool bind count increment the
	     * current resource pool bind count 
	     */
	    if (pstNamespace->resource_pool) {
		/*
		 * check ascertains if this is the startup case where the
		 * resource pool is essentially unsassigned 
		 */
		pstNamespace->resource_pool->num_ns_bound--;
	    }
	    pstNamespace->resource_pool = nvsd_mgmt_get_resource_mgr(t_pool);
	    if (!search_ressource_mgr(t_pool, &ret_idx)) {
		pstNamespace->rp_idx = ret_idx;
	    }
	    if (pstNamespace->resource_pool)
		pstNamespace->resource_pool->num_ns_bound++;
	    if (nkn_system_inited) {
		err = DM2_mgmt_ns_modify_namespace(pstNamespace->uid + 1,
			ns_idx);
		bail_error(err);
	    }
	} else if (unbind_flag) {
	    /*
	     * unbind event: decrement existing resource pool availability
	     * and increment global pool bind count 
	     */
	    if (pstNamespace->resource_pool) {
		/*
		 * check ascertains if this is the startup case where the
		 * resource pool is essentially unsassigned 
		 */
		pstNamespace->resource_pool->num_ns_bound--;
	    }
	    /*
	     * set the global resource pool availability 
	     */
	    pstNamespace->resource_pool = nvsd_mgmt_get_resource_mgr(t_pool);
	    if (!search_ressource_mgr(t_pool, &ret_idx)) {
		pstNamespace->rp_idx = ret_idx;
	    }
	    if (pstNamespace->resource_pool)
		pstNamespace->resource_pool->num_ns_bound++;
	}

	safe_free(t_pool);
    }

    /*
     * As a part of resource pool,we dont delete the namespace, rather we set
     * this field to indicate that the resource pool is deleted
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 2, "*",
		"deleted")) {
	tbool t_deleted;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_deleted);
	bail_error(err);
	pstNamespace->deleted = t_deleted;
    }

    /*------------------------ Dynamic-URI configs---------------------*/
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"client-request",
		"dynamic-uri", "regex")) {
	tstring *t_regex = NULL;

	err = bn_binding_get_tstr(binding, ba_value, bt_regex, NULL, &t_regex);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read ../nkn/nvsd/namespace/%s/client-request/dynamic-uri/regex as : \"%s\"",
		t_namespace, ts_str_maybe_empty(t_regex));

	if (NULL != pstNamespace->dynamic_uri.regex) {
	    safe_free(pstNamespace->dynamic_uri.regex);
	    pstNamespace->dynamic_uri.regex = NULL;
	}

	if (t_regex && ts_length(t_regex) > 0) {
	    pstNamespace->dynamic_uri.regex =
		nkn_strdup_type(ts_str(t_regex), mod_mgmt_charbuf);
	} else {
	    safe_free(pstNamespace->dynamic_uri.regex);
	    pstNamespace->dynamic_uri.regex = NULL;
	}

	ts_free(&t_regex);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"client-request",
		"dynamic-uri",
		"map-string")) {
	tstring *t_map_string = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_map_string);
	bail_error(err);
	lc_log_basic(jnpr_log_level,
		"Read ../nkn/nvsd/namespace/%s/client-request/dynamic-uri/map-string as : \"%s\"",
		t_namespace, ts_str_maybe_empty(t_map_string));

	if (NULL != pstNamespace->dynamic_uri.map_string)
	    safe_free(pstNamespace->dynamic_uri.map_string);

	if (NULL != t_map_string) {
	    pstNamespace->dynamic_uri.map_string =
		nkn_strdup_type(ts_str(t_map_string), mod_mgmt_charbuf);
	} else
	    pstNamespace->dynamic_uri.map_string = NULL;

	ts_free(&t_map_string);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"client-request",
		"dynamic-uri",
		"no-match-tunnel")) {
	tbool t_no_match_tunnel;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_no_match_tunnel);
	bail_error(err);
	pstNamespace->dynamic_uri.no_match_tunnel = t_no_match_tunnel;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"client-request",
		"geo-service",
		"service")) {
	tstring *t_service = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_service);
	bail_error(err);
	if (t_service && ts_equal_str(t_service, "quova", false)) {
	    pstNamespace->geo_service.service = QUOVA;
	} else if (t_service && ts_equal_str(t_service, "maxmind", false)) {
	    pstNamespace->geo_service.service = MAXMIND;
	} else {
	    pstNamespace->geo_service.service = NONE;
	}
	ts_free(&t_service);

    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"client-request",
		"geo-service",
		"api-url")) {
	tstring *t_url = NULL;

	err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &t_url);
	bail_error(err);
	if (NULL != pstNamespace->geo_service.api_url)
	    safe_free(pstNamespace->geo_service.api_url);

	if (t_url && (ts_length(t_url) > 0)) {
	    pstNamespace->geo_service.api_url =
		nkn_strdup_type(ts_str(t_url), mod_mgmt_charbuf);
	} else {
	    pstNamespace->geo_service.api_url = NULL;
	}
	ts_free(&t_url);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"client-request",
		"geo-service",
		"failover-mode")) {
	tbool t_failover;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_failover);
	bail_error(err);
	pstNamespace->geo_service.failover_mode = t_failover;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"client-request",
		"geo-service",
		"lookup-timeout")) {
	uint32 t_time;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_time);
	bail_error(err);
	pstNamespace->geo_service.lookup_timeout = t_time;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"client-request",
		"dynamic-uri",
		"tokenize-string")) {
	tstring *t_tokenize_str = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_tokenize_str);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read ../nkn/nvsd/namespace/%s/client-request/dynamic-uri/tokenize-string as : \"%s\"",
		t_namespace, ts_str_maybe_empty(t_tokenize_str));

	if (NULL != pstNamespace->dynamic_uri.tokenize_str)
	    safe_free(pstNamespace->dynamic_uri.tokenize_str);

	if (NULL != t_tokenize_str) {
	    pstNamespace->dynamic_uri.tokenize_str =
		nkn_strdup_type(ts_str(t_tokenize_str), mod_mgmt_charbuf);
	} else
	    pstNamespace->dynamic_uri.tokenize_str = NULL;

	ts_free(&t_tokenize_str);
    }

    /*------------------------ Seek-URI configs---------------------*/
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"client-request",
		"seek-uri", "regex")) {
	tstring *t_regex = NULL;

	err = bn_binding_get_tstr(binding, ba_value, bt_regex, NULL, &t_regex);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read ../nkn/nvsd/namespace/%s/client-request/seek-uri/regex as : \"%s\"",
		t_namespace, ts_str_maybe_empty(t_regex));

	if (NULL != pstNamespace->seek_uri.regex) {
	    safe_free(pstNamespace->seek_uri.regex);
	    pstNamespace->seek_uri.regex = NULL;
	}

	if (t_regex && ts_length(t_regex) > 0) {
	    pstNamespace->seek_uri.regex =
		nkn_strdup_type(ts_str(t_regex), mod_mgmt_charbuf);
	} else {
	    safe_free(pstNamespace->seek_uri.regex);
	    pstNamespace->seek_uri.regex = NULL;
	}

	ts_free(&t_regex);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"client-request",
		"seek-uri",
		"map-string")) {
	tstring *t_map_string = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_map_string);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read ../nkn/nvsd/namespace/%s/client-request/seek-uri/map-string as : \"%s\"",
		t_namespace, ts_str_maybe_empty(t_map_string));

	if (NULL != pstNamespace->seek_uri.map_string)
	    safe_free(pstNamespace->seek_uri.map_string);

	if (NULL != t_map_string) {
	    pstNamespace->seek_uri.map_string =
		nkn_strdup_type(ts_str(t_map_string), mod_mgmt_charbuf);
	} else
	    pstNamespace->seek_uri.map_string = NULL;

	ts_free(&t_map_string);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"client-request",
		"seek-uri", "enable")) {
	tbool t_seek_uri_enable;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_seek_uri_enable);
	bail_error(err);
	pstNamespace->seek_uri.seek_uri_enable = t_seek_uri_enable;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 3, "*",
		"client-request",
		"secure")) {
	tbool t_secure;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_secure);
	bail_error(err);
	pstNamespace->client_req.secure = t_secure;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 3, "*",
		"client-request",
		"plain-text")) {
	tbool t_nonsecure;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_nonsecure);
	bail_error(err);
	pstNamespace->client_req.non_secure = t_nonsecure;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 3, "*",
		"origin-request",
		"secure")) {
	tbool t_secure;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_secure);
	bail_error(err);
	pstNamespace->origin_req.secure = t_secure;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 7, "*",
		"client-request", "method", "head",
		"cache-miss", "action", "tunnel")) {
	tbool t_head_tunnel;

	pstNamespace->client_request.http_head_to_get = 0;
	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_head_tunnel);
	bail_error(err);
	if (!t_head_tunnel) {
                pstNamespace->client_request.http_head_to_get = 1;
        }
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 9, "*",
		"client-request", "method", "head",
		"cache-miss", "action", "convert-to", "method", "get")) {
	tbool t_head_to_get;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_head_to_get);
	bail_error(err);
	pstNamespace->client_request.http_head_to_get = t_head_to_get;
    } else  if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 5, "*",
						"client-request", "header",
						"accept", "**")) {
	const char *t_temp_str;
	uint32 t_index;
	client_request_header_t *pst_accept_header_map;

	t_temp_str = tstr_array_get_str_quick (name_parts, 7);
	if (!t_temp_str) goto bail;
	t_index = atoi(t_temp_str);
	t_index--;
	if (MAX_CLIENT_REQUEST_HEADERS <= t_index) goto bail;

	pst_accept_header_map = &(pstNamespace->client_request.accept_headers[t_index]);
	if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 6, "*", "client-request", "header", "accept", "*", "name")) {
	    tstring *t_header_name = NULL;

	    err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &t_header_name);
	    bail_error(err);
	    lc_log_basic(jnpr_log_level, "Read .../nkn/nvsd/namespace/%s/client-request/header/accept/%d/name as : \"%s\"",
			     t_namespace, (t_index + 1), (ts_str(t_header_name)) ? ts_str(t_header_name) : "");
	    if (pst_accept_header_map->name) {
		safe_free(pst_accept_header_map->name);
		pst_accept_header_map->name = NULL;
	    }
	    if (t_header_name) {
		pst_accept_header_map->name = nkn_strdup_type (ts_str(t_header_name), mod_mgmt_charbuf);
	    }
	    ts_free(&t_header_name);
	}
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 6, "*",
		    "client-request", "dns", "mismatch", "action", "tunnel")) {
	tbool t_dns_resp_mismatch;

	err = bn_binding_get_tbool(binding,
			ba_value, NULL,
			&t_dns_resp_mismatch);
	bail_error(err);
	pstNamespace->client_request.dns_resp_mismatch = t_dns_resp_mismatch;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 5, "*",
		"resource", "cache",
		"sata", "capacity")) {
	uint64 t_sata_cache_limit = 0;

	err = bn_binding_get_uint64(binding,
		ba_value, NULL, &t_sata_cache_limit);
	bail_error(err);

	pstNamespace->media_cache.sata.max_disk_usage = t_sata_cache_limit;

	if (pstNamespace->uid && nkn_system_inited) {
	    err = DM2_mgmt_ns_set_tier_max_disk_usage(pstNamespace->uid + 1,
		    "sata",
		    pstNamespace->media_cache.
		    sata.max_disk_usage);

	    if (-ENOENT == err) {
		lc_log_basic(LOG_NOTICE, "No cache tier found - SATA");
	    } else if (-EINVAL == err) {
		lc_log_basic(LOG_NOTICE, "Invalid value : %ld",
			t_sata_cache_limit);
	    }
	    err = 0;
	}
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 5, "*",
		"resource", "cache",
		"ssd", "capacity")) {
	uint64 t_ssd_cache_limit = 0;

	err = bn_binding_get_uint64(binding,
		ba_value, NULL, &t_ssd_cache_limit);
	bail_error(err);

	pstNamespace->media_cache.ssd.max_disk_usage = t_ssd_cache_limit;

	if (pstNamespace->uid && nkn_system_inited) {
	    err = DM2_mgmt_ns_set_tier_max_disk_usage(pstNamespace->uid + 1,
		    "ssd",
		    pstNamespace->media_cache.
		    ssd.max_disk_usage);

	    if (-ENOENT == err) {
		lc_log_basic(LOG_NOTICE, "No cache tier found - SSD");
	    } else if (-EINVAL == err) {
		lc_log_basic(LOG_NOTICE, "Invalid value : %ld",
			t_ssd_cache_limit);
	    }
	    err = 0;
	}
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 5, "*",
		"resource", "cache",
		"sas", "capacity")) {
	uint64 t_sas_cache_limit = 0;

	err = bn_binding_get_uint64(binding,
		ba_value, NULL, &t_sas_cache_limit);
	bail_error(err);

	pstNamespace->media_cache.sas.max_disk_usage = t_sas_cache_limit;
	if (pstNamespace->uid && nkn_system_inited) {
	    err = DM2_mgmt_ns_set_tier_max_disk_usage(pstNamespace->uid + 1,
		    "sas",
		    pstNamespace->media_cache.
		    sas.max_disk_usage);

	    if (-ENOENT == err) {
		lc_log_basic(LOG_NOTICE, "No cache tier found - SAS");
	    } else if (-EINVAL == err) {
		lc_log_basic(LOG_NOTICE, "Invalid value : %ld",
			t_sas_cache_limit);
	    }
	    err = 0;
	}
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"mediacache", "sas",
		"readsize")) {
	uint32 t_sas_read_size = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_sas_read_size);
	bail_error(err);

	pstNamespace->media_cache.sas.read_size = t_sas_read_size * 1024;
	if (pstNamespace->uid && nkn_system_inited) {
	    err = DM2_mgmt_ns_set_tier_read_size(pstNamespace->uid + 1, "sas",
		    pstNamespace->media_cache.sas.
		    read_size);

	    if (-ENOENT == err) {
		lc_log_basic(LOG_NOTICE, "No cache tier found - SAS");
	    } else if (-EINVAL == err) {
		lc_log_basic(LOG_NOTICE, "Invalid value : %d", t_sas_read_size);
	    }
	    err = 0;
	}
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"mediacache", "sata",
		"readsize")) {
	uint32 t_sata_read_size = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_sata_read_size);
	bail_error(err);

	pstNamespace->media_cache.sata.read_size = t_sata_read_size * 1024;

	if (pstNamespace->uid && nkn_system_inited) {
	    err = DM2_mgmt_ns_set_tier_read_size(pstNamespace->uid + 1, "sata",
		    pstNamespace->media_cache.sata.
		    read_size);

	    if (-ENOENT == err) {
		lc_log_basic(LOG_NOTICE, "No cache tier found - SATA");
	    } else if (-EINVAL == err) {
		lc_log_basic(LOG_NOTICE, "Invalid value : %d",
			t_sata_read_size);
	    }
	    err = 0;
	}
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"mediacache", "ssd",
		"readsize")) {
	uint32 t_ssd_read_size = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_ssd_read_size);
	bail_error(err);

	pstNamespace->media_cache.ssd.read_size = t_ssd_read_size * 1024;

	if (pstNamespace->uid && nkn_system_inited) {
	    err = DM2_mgmt_ns_set_tier_read_size(pstNamespace->uid + 1, "ssd",
		    pstNamespace->media_cache.ssd.
		    read_size);

	    if (-ENOENT == err) {
		lc_log_basic(LOG_NOTICE, "No cache tier found - SSD");
	    } else if (-EINVAL == err) {
		lc_log_basic(LOG_NOTICE, "Invalid value : %d", t_ssd_read_size);
	    }
	    err = 0;
	}
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"mediacache", "sas",
		"free_block_thres")) {
	uint32 t_sas_free_block_thres = 0;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL, &t_sas_free_block_thres);
	bail_error(err);

	pstNamespace->media_cache.sas.free_block_thres = t_sas_free_block_thres;

	if (pstNamespace->uid && nkn_system_inited) {
	    err =
		DM2_mgmt_ns_set_tier_block_free_threshold(pstNamespace->uid + 1,
			"sas",
			pstNamespace->
			media_cache.sas.
			free_block_thres);

	    if (-ENOENT == err) {
		lc_log_basic(LOG_NOTICE, "No cache tier found - SAS");
	    } else if (-EINVAL == err) {
		lc_log_basic(LOG_NOTICE, "Invalid value : %d",
			t_sas_free_block_thres);
	    }
	    err = 0;
	}
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"mediacache", "sata",
		"free_block_thres")) {
	uint32 t_sata_free_block_thres = 0;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL, &t_sata_free_block_thres);
	bail_error(err);

	pstNamespace->media_cache.sata.free_block_thres =
	    t_sata_free_block_thres;

	if (pstNamespace->uid && nkn_system_inited) {
	    err =
		DM2_mgmt_ns_set_tier_block_free_threshold(pstNamespace->uid + 1,
			"sata",
			pstNamespace->
			media_cache.sata.
			free_block_thres);

	    if (-ENOENT == err) {
		lc_log_basic(LOG_NOTICE, "No cache tier found - SATA");
	    } else if (-EINVAL == err) {
		lc_log_basic(LOG_NOTICE, "Invalid value : %d",
			t_sata_free_block_thres);
	    }
	    err = 0;
	}
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"mediacache", "ssd",
		"free_block_thres")) {
	uint32 t_ssd_free_block_thres = 0;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL, &t_ssd_free_block_thres);
	bail_error(err);

	pstNamespace->media_cache.ssd.free_block_thres = t_ssd_free_block_thres;

	if (pstNamespace->uid && nkn_system_inited) {
	    err =
		DM2_mgmt_ns_set_tier_block_free_threshold(pstNamespace->uid + 1,
			"ssd",
			pstNamespace->
			media_cache.ssd.
			free_block_thres);

	    if (-ENOENT == err) {
		lc_log_basic(LOG_NOTICE, "No cache tier found - SSD");
	    } else if (-EINVAL == err) {
		lc_log_basic(LOG_NOTICE, "Invalid value : %d",
			t_ssd_free_block_thres);
	    }
	    err = 0;
	}
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"mediacache", "sas",
		"group_read_enable")) {
	tbool t_sas_group_read_enable = false;

	err = bn_binding_get_tbool(binding,
		ba_value, NULL, &t_sas_group_read_enable);
	bail_error(err);

	pstNamespace->media_cache.sas.group_read_enable =
	    t_sas_group_read_enable;

	if (pstNamespace->uid && nkn_system_inited) {
	    err = DM2_mgmt_ns_set_tier_group_read(pstNamespace->uid + 1, "sas",
		    (pstNamespace->media_cache.
		     sas.
		     group_read_enable) ? 1 : 0);

	    if (-ENOENT == err) {
		lc_log_basic(LOG_NOTICE, "No cache tier found - SAS");
	    } else if (-EINVAL == err) {
		lc_log_basic(LOG_NOTICE, "Invalid value");
	    }
	    err = 0;
	}
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"mediacache", "sata",
		"group_read_enable")) {
	tbool t_sata_group_read_enable = false;

	err = bn_binding_get_tbool(binding,
		ba_value, NULL, &t_sata_group_read_enable);
	bail_error(err);

	pstNamespace->media_cache.sata.group_read_enable =
	    t_sata_group_read_enable;

	if (pstNamespace->uid && nkn_system_inited) {
	    err = DM2_mgmt_ns_set_tier_group_read(pstNamespace->uid + 1,
		    "sata",
		    (pstNamespace->media_cache.
		     sata.
		     group_read_enable) ? 1 : 0);

	    if (-ENOENT == err) {
		lc_log_basic(LOG_NOTICE, "No cache tier found - SATA");
	    } else if (-EINVAL == err) {
		lc_log_basic(LOG_NOTICE, "Invalid value");
	    }
	    err = 0;
	}
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4, "*",
		"mediacache", "ssd",
		"group_read_enable")) {
	tbool t_ssd_group_read_enable = false;

	err = bn_binding_get_tbool(binding,
		ba_value, NULL, &t_ssd_group_read_enable);
	bail_error(err);

	pstNamespace->media_cache.ssd.group_read_enable =
	    t_ssd_group_read_enable;

	if (pstNamespace->uid && nkn_system_inited) {
	    err = DM2_mgmt_ns_set_tier_group_read(pstNamespace->uid + 1,
		    "ssd",
		    (pstNamespace->media_cache.
		     ssd.
		     group_read_enable) ? 1 : 0);

	    if (-ENOENT == err) {
		lc_log_basic(LOG_NOTICE, "No cache tier found - SSD");
	    } else if (-EINVAL == err) {
		lc_log_basic(LOG_NOTICE, "Invalid value");
	    }
	    err = 0;
	}
    }

    /*
     * Read the accesslog profile name that this Namespace uses 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 2, "*",
		"accesslog")) {
	node_name_t node_name;
	tstring *acclog_format = NULL;

	err = bn_binding_get_str(binding,
		ba_value, bt_string, NULL, &acclog_name);
	bail_error(err);

	if (pstNamespace->access_log.name != NULL) {
	    safe_free(pstNamespace->access_log.name);
	    pstNamespace->access_log.name = NULL;
	}

	if (acclog_name) {
	    pstNamespace->access_log.name =
		nkn_strdup_type(acclog_name, mod_mgmt_charbuf);
	    snprintf(node_name, sizeof (node_name),
		    "/nkn/accesslog/config/profile/%s/format", acclog_name);
	    err = mdc_get_binding_tstr(nvsd_mcc, NULL, NULL, NULL,
		    &acclog_format, node_name, NULL);
	    bail_error(err);
	    if (acclog_format) {
		nvsd_update_namespace_accesslog_profile_format(acclog_name,
			ts_str
			(acclog_format),
			pstNamespace);
		ts_free(&acclog_format);
	    }
	}
	safe_free(acclog_name);
	acclog_name = NULL;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 3, "*",
		"vip", "*")) {
	err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &vip);
	bail_error(err);

	if (vip) {
	    pstNamespace->vip_obj.vip[pstNamespace->vip_obj.n_vips] =
		nkn_strdup_type(vip, mod_mgmt_charbuf);
	    pstNamespace->vip_obj.n_vips++;
	    assert(pstNamespace->vip_obj.n_vips <= NKN_MAX_VIPS);
	}

    }

    /*
     * Set the flag so that we know this is a dynamic change 
     */
    dynamic_change = true;

bail:
    tstr_array_free(&name_parts);
    safe_free(vip);
    if (acclog_name)
	safe_free(acclog_name);
    return err;

}	/* end of nvsd_namespace_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_namespace_delete_cfg_handle_change()
 *	purpose : handler for config deletes for namespace
 */
static int
nvsd_namespace_delete_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array *
	bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    const char *t_namespace = NULL;
    const char *t_pp = NULL;
    int ns_idx;
    tstr_array *name_parts = NULL;
    tbool *rechecked_licenses_p = data;
    pub_point_data_t *pPubPoint = NULL;
    char *vip = NULL;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(bndgs_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    if (bn_binding_name_pattern_match(ts_str(name), "/nkn/nvsd/namespace/**")) {
	bn_binding_get_name_parts(binding, &name_parts);
	bail_error_null(err, name_parts);

    } else {
	/*
	 * This is not the namespace node hence leave 
	 */
	goto bail;
    }

    /*
     * Now process the removal of the namespace 
     */
    if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 1, "*")) {

	/*-------- Get the Namespace ------------*/
	t_namespace = tstr_array_get_str_quick(name_parts, 3);

	bail_error(err);
	lc_log_basic(jnpr_log_level, "Read .../nkn/nvsd/namespace as : \"%s\"",
		t_namespace);
	if (search_namespace(t_namespace, &ns_idx) == 0) {
	    /*
	     * TODO check whether to delete char * fields before freeing
	     * namespace namespace_node_data_t *temp = get_namespace(ns_idx); 
	     */

	    delete_namespace(ns_idx);

	}
	// else not possible may be some issue
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 3, 3, "*", "pub-point", "*")) {
	/*-------- Get the Namespace ------------*/
	t_namespace = tstr_array_get_str_quick(name_parts, 3);

	bail_error(err);
	lc_log_basic(jnpr_log_level, "Read .../nkn/nvsd/namespace as : \"%s\"",
		t_namespace);

	t_pp = tstr_array_get_str_quick(name_parts, 5);

	if (search_namespace(t_namespace, &ns_idx) == 0) {
	    pstNamespace = get_namespace(ns_idx);
	    pPubPoint = getPubPoint(pstNamespace, t_pp);

	    if (pPubPoint) {
		if (pPubPoint->name) {
		    safe_free(pPubPoint->name);
		    pPubPoint->name = NULL;
		}
	    }
	}
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 3, 3, "*", "vip", "*")) {
	int i = 0;

	t_namespace = tstr_array_get_str_quick(name_parts, 3);

	err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &vip);
	bail_error_null(err, vip);

	if (search_namespace(t_namespace, &ns_idx) == 0) {
	    pstNamespace = get_namespace(ns_idx);

	    for (i = 0; i < pstNamespace->vip_obj.n_vips; i++) {
		if (strcmp(vip, pstNamespace->vip_obj.vip[i]) == 0) {
		    /*
		     * we have a match free the deleted entry move the array
		     * clear the last index as it is moved 
		     */
		    safe_free(pstNamespace->vip_obj.vip[i]);
		    memmove(&(pstNamespace->vip_obj.vip[i]),
			    &(pstNamespace->vip_obj.vip[i + 1]),
			    ((pstNamespace->vip_obj.n_vips -
			      (i +
			       1)) * sizeof (pstNamespace->vip_obj.vip[i])));
		    pstNamespace->vip_obj.vip[pstNamespace->vip_obj.n_vips -
			1] = NULL;
		    pstNamespace->vip_obj.n_vips--;
		}
	    }
	}
    }

    /*
     * BZ 2995/BZ 3286 - Since we allow single host only, and we also need to 
     * provide for backward compatibility, we free the existing 0th index in
     * the older struct and always set in the 0th index This is no longer
     * required, since this condition is handled in previous set (where value 
     * is NULL) 
     */
bail:
    tstr_array_free(&name_parts);
    safe_free(vip);
    return err;

}	/* end of nvsd_namespace_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_uri_cfg_handle_change()
 *	purpose : handler for config changes for uri
 */

static int
nvsd_uri_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    const char *t_namespace = NULL;
    int ns_idx;
    tstr_array *name_parts = NULL;
    tbool *rechecked_licenses_p = data;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);

    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(bndgs_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /*
     * Check if this is our node 
     */
    if (bn_binding_name_pattern_match(ts_str(name), "/nkn/nvsd/uri/config/**")) {

	/*-------- Get the Namespace ------------*/
	bn_binding_get_name_parts(binding, &name_parts);
	bail_error(err);
	t_namespace = tstr_array_get_str_quick(name_parts, 4);

	bail_error(err);
	lc_log_basic(jnpr_log_level, "Read .../nkn/nvsd/uri as : \"%s\"",
		t_namespace);
	if (search_namespace(t_namespace, &ns_idx) != 0) {
	    lc_log_basic(LOG_NOTICE,
		    "Failed to find the namespace entry for : \"%s\"",
		    t_namespace);
	    goto bail;
	}
	pstNamespace = get_namespace(ns_idx);
    } else {
	/*
	 * This is not the uri node hence leave 
	 */
	goto bail;
    }

    /*
     * Set the flag so that we know this is a dynamic change 
     */
    dynamic_change = true;

bail:
    tstr_array_free(&name_parts);
    return err;

}	/* end of nvsd_uri_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_uri_delete_cfg_handle_change()
 *	purpose : handler for config deletes for uri
 */
static int
nvsd_uri_delete_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    const char *t_namespace = NULL;
    int ns_idx;
    tstr_array *name_parts = NULL;
    tbool *rechecked_licenses_p = data;
    uri_prefix_data_t *pstUriPrefix = NULL;

    (void) pstUriPrefix;
    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(bndgs_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /*
     * Check if this is our node 
     */
    if (bn_binding_name_pattern_match(ts_str(name), "/nkn/nvsd/uri/config/**")) {

	/*-------- Get the Namespace ------------*/
	bn_binding_get_name_parts(binding, &name_parts);
	bail_error(err);
	t_namespace = tstr_array_get_str_quick(name_parts, 4);

	bail_error(err);
	lc_log_basic(jnpr_log_level, "Read .../nkn/nvsd/uri as : \"%s\"",
		t_namespace);
	if (search_namespace(t_namespace, &ns_idx) != 0) {
	    lc_log_basic(LOG_NOTICE,
		    "Failed to find the namespace entry for : \"%s\"",
		    t_namespace);
	    goto bail;
	}

	pstNamespace = get_namespace(ns_idx);
    } else {
	/*
	 * This is not the uri node hence leave 
	 */
	goto bail;
    }

bail:
    tstr_array_free(&name_parts);
    return err;

}	/* end of nvsd_uri_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_origin_fetch_cfg_handle_change()
 *	purpose : handler for config changes for origin_fetch policy
 */

static int
nvsd_origin_fetch_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    const char *t_namespace = NULL;
    int ns_idx;
    tstr_array *name_parts = NULL;
    tbool *rechecked_licenses_p = data;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(bndgs_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /*
     * Check if this is our node 
     */
    if (bn_binding_name_pattern_match
	    (ts_str(name), "/nkn/nvsd/origin_fetch/config/**")) {

	/*-------- Get the Namespace ------------*/
	err = bn_binding_get_name_parts(binding, &name_parts);
	bail_error(err);
	t_namespace = tstr_array_get_str_quick(name_parts, 4);

	bail_error(err);
	lc_log_basic(jnpr_log_level,
		"Read .../nkn/nvsd/origin_fetch/config as : \"%s\"",
		t_namespace);
	if (search_namespace(t_namespace, &ns_idx) != 0)
	    goto bail;

	pstNamespace = get_namespace(ns_idx);
    } else {
	/*
	 * This is not the origin-policy node hence leave 
	 */
	goto bail;
    }

    /*
     * Get the value of CACHE-DIRECTIVE NO-CACHE 
     */
    if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 3, "*",
		"cache-directive", "no-cache")) {
	tstring *t_no_cache = NULL;
	const char *no_cache = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_no_cache);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../origin_fetch/config/%s/cache-directive/no_cache as : \"%s\"",
		t_namespace, no_cache ? ts_str(t_no_cache) : "");

	if (NULL != pstNamespace->origin_fetch.cache_directive_no_cache)
	    safe_free(pstNamespace->origin_fetch.cache_directive_no_cache);

	if (NULL != t_no_cache) {
	    pstNamespace->origin_fetch.cache_directive_no_cache =
		nkn_strdup_type(ts_str(t_no_cache), mod_mgmt_charbuf);
	} else
	    pstNamespace->origin_fetch.cache_directive_no_cache = NULL;

	ts_free(&t_no_cache);
    }

    /*
     * Get the value of CACHE-AGE 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 3, "*",
		"cache-age", "default")) {
	uint32 t_cache_age = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_cache_age);

	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../origin_fetch/config/%s/cache-age as : \"%d\"",
		t_namespace, t_cache_age);

	pstNamespace->origin_fetch.cache_age = t_cache_age;
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "serve-expired", "enable")) {
	tbool enable;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &enable);
	bail_error(err);
	pstNamespace->origin_fetch.enable_expired_delivery = enable;
    }
    /*
     * Get the value of CUSTOM-HEADER 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 3, "*",
		"cache-control",
		"custom-header")) {
	tstring *t_custom_hdr = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_custom_hdr);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../origin_fetch/config/%s/cache-control/custom-header as : \"%s\"",
		t_namespace, ts_str_maybe_empty(t_custom_hdr));

	if (NULL != pstNamespace->origin_fetch.custom_cache_control)
	    safe_free(pstNamespace->origin_fetch.custom_cache_control);

	if (NULL != t_custom_hdr) {
	    pstNamespace->origin_fetch.custom_cache_control =
		nkn_strdup_type(ts_str(t_custom_hdr), mod_mgmt_charbuf);
	    ts_free(&t_custom_hdr);
	} else
	    pstNamespace->origin_fetch.custom_cache_control = NULL;

    }
    /*
     * Get the value of regex
     */

    else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 3, "*",
                "vary-header",
                "pc")) {
        tstring *t_pc_regex = NULL;

        err = bn_binding_get_tstr(binding,
                ba_value, bt_string, NULL, &t_pc_regex);
        bail_error(err);

        lc_log_basic(jnpr_log_level,
                "Read .../origin_fetch/config/%s/vary-header/pc as : \"%s\"",
                t_namespace, ts_str_maybe_empty(t_pc_regex));

        if (NULL != pstNamespace->user_agent_regex[VARY_HEADER_PC])
            safe_free(pstNamespace->user_agent_regex[VARY_HEADER_PC]);

        if (NULL != t_pc_regex ) {
            pstNamespace->user_agent_regex[VARY_HEADER_PC]  =
                nkn_strdup_type(ts_str(t_pc_regex), mod_mgmt_charbuf);
            ts_free(&t_pc_regex);
        } else {
            pstNamespace->user_agent_regex[VARY_HEADER_PC]= NULL;
	}
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 3, "*",
                "vary-header",
                "tablet")) {
        tstring *t_tablet_regex = NULL;

        err = bn_binding_get_tstr(binding,
                ba_value, bt_string, NULL, &t_tablet_regex);
        bail_error(err);

        lc_log_basic(jnpr_log_level,
                "Read .../origin_fetch/config/%s/vary-header/tablet as : \"%s\"",
                t_namespace, ts_str_maybe_empty(t_tablet_regex));

        if (NULL != pstNamespace->user_agent_regex[VARY_HEADER_TABLET])
            safe_free(pstNamespace->user_agent_regex[VARY_HEADER_TABLET]);

        if (NULL != t_tablet_regex ) {
            pstNamespace->user_agent_regex[VARY_HEADER_TABLET]  =
                nkn_strdup_type(ts_str(t_tablet_regex), mod_mgmt_charbuf);
            ts_free(&t_tablet_regex);
        } else {
            pstNamespace->user_agent_regex[VARY_HEADER_TABLET]= NULL;
	}
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 3, "*",
                "vary-header",
                "phone")) {
        tstring *t_phone_regex = NULL;

        err = bn_binding_get_tstr(binding,
                ba_value, bt_string, NULL, &t_phone_regex);
        bail_error(err);

        lc_log_basic(jnpr_log_level,
                "Read .../origin_fetch/config/%s/vary-header/phone as : \"%s\"",
                t_namespace, ts_str_maybe_empty(t_phone_regex));
        if (NULL != pstNamespace->user_agent_regex[VARY_HEADER_PHONE])
            safe_free(pstNamespace->user_agent_regex[VARY_HEADER_PHONE]);

        if (NULL != t_phone_regex ) {
            pstNamespace->user_agent_regex[VARY_HEADER_PHONE]  =
                nkn_strdup_type(ts_str(t_phone_regex), mod_mgmt_charbuf);
            ts_free(&t_phone_regex);
        } else {
            pstNamespace->user_agent_regex[VARY_HEADER_PHONE]= NULL;
	}
    }
    /*
     * Get the value of s-maxage config 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 3, "*",
		"cache-control",
		"s-maxage-ignore")) {
	tbool ignore;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &ignore);
	bail_error(err);
	pstNamespace->origin_fetch.ignore_s_maxage = ignore;
    }

    /*
     * Get the value of PARTIAL-CONTENT 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 2, "*",
		"partial-content")) {
	tbool t_partial_content;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_partial_content);
	bail_error(err);
	pstNamespace->origin_fetch.partial_content = t_partial_content;
    }

    /*
     * Get the value of DATE_HEADER MODIFY 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 3, "*",
		"date-header", "modify")) {
	tbool t_date_header_modify;

	err = bn_binding_get_tbool(binding,
		ba_value, NULL, &t_date_header_modify);
	bail_error(err);
	pstNamespace->origin_fetch.date_header_modify = t_date_header_modify;
    }

    /*
     * Get the value of CACHE REVALIDATE 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 2, "*",
		"cache-revalidate")) {
	tbool t_cache_revalidate;

	err = bn_binding_get_tbool(binding,
		ba_value, NULL, &t_cache_revalidate);
	bail_error(err);
	pstNamespace->origin_fetch.cache_reval.cache_revalidate =
	    t_cache_revalidate;
    }

    /*
     * Get the value of USE-DATE-HEADER 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 3, "*",
		"cache-revalidate",
		"use-date-header")) {
	tbool t_use_date_header;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_use_date_header);
	bail_error(err);
	pstNamespace->origin_fetch.use_date_header = t_use_date_header;
    }

    /*
     * Get the value of FLUSH-INTERMEDIATE-CACHES 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 3, "*",
		"cache-revalidate",
		"flush-intermediate-cache")) {
	tbool t_flush_cache;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_flush_cache);
	bail_error(err);
	pstNamespace->origin_fetch.flush_intermediate_caches = t_flush_cache;
    }
    /* Get the value of client-header-inherit */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 3,"*",
		"cache-revalidate", "client-header-inherit"))
    //else if (bn_binding_name_pattern_match(ts_str(name), "/nkn/nvsd/origin_fetch/config/*/cache-revalidate/client-header-inherit"))
    {
	tbool t_client_header_inherit;

	err = bn_binding_get_tbool(binding,
			ba_value, NULL,
			&t_client_header_inherit);
	bail_error(err);
	pstNamespace->origin_fetch.cache_reval.use_client_headers = t_client_header_inherit;
    }
    /*
     * Get the value of CACHE-AGE-THRESHOLD 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 4, "*",
		"content-store", "media",
		"cache-age-threshold")) {
	uint32 t_cache_age_threshold = 0;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL, &t_cache_age_threshold);

	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../origin_fetch/config/%s/content-store/media/cache-age-threshold as : \"%d\"",
		t_namespace, t_cache_age_threshold);

	pstNamespace->origin_fetch.content_store_cache_age_threshold =
	    t_cache_age_threshold;
    }
    /*
     * Get the value of URI-DEPTH-THRESHOLD 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 4, "*",
		"content-store", "media",
		"uri-depth-threshold")) {
	uint32 t_uri_depth_threshold = 0;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL, &t_uri_depth_threshold);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../origin_fetch/config/%s/content-store/media/uri-depth-threshold as : \"%d\"",
		t_namespace, t_uri_depth_threshold);

	pstNamespace->origin_fetch.content_store_uri_depth_threshold =
	    t_uri_depth_threshold;
    }

    /*
     * Get the value of OBJECT-SIZE-THRESHOLD 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 4, "*",
		"content-store", "media",
		"object-size-threshold")) {
	uint32 t_object_size_threshold = 0;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL, &t_object_size_threshold);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../origin_fetch/config/%s/content-store/media/object-size-threshold as : \"%d\"",
		t_namespace, t_object_size_threshold);

	pstNamespace->origin_fetch.store_cache_min_size =
	    t_object_size_threshold;
    }
    /*
     * Get the value of CACHE-FILL 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 2, "*",
		"cache-fill")) {
	uint32 t_cache_fill = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_cache_fill);
	bail_error(err);
	lc_log_basic(jnpr_log_level,
		"Read .../origin_fetch/config/%s/content-store/media/cache-fill as : \"%d\"",
		t_namespace, t_cache_fill);

	pstNamespace->origin_fetch.cache_fill = t_cache_fill;
    }
    /*
     * Get the value of INGEST-POLICY 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 2, "*",
		"ingest-policy")) {
	uint32 t_ingest_policy = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_ingest_policy);
	bail_error(err);
	lc_log_basic(jnpr_log_level,
		"Read .../origin_fetch/config/%s/content-store/media/ingest-policy as : \"%d\"",
		t_namespace, t_ingest_policy);

	pstNamespace->origin_fetch.ingest_policy = t_ingest_policy;
    }
    /*
     * Get the value of OFFLINE FETCH SIZE 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 4, "*",
		"offline", "fetch", "size")) {
	uint32 t_offline_fetch_size = 0;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL, &t_offline_fetch_size);

	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../origin_fetch/config/%s/offline/fetch/size as : \"%d\"",
		t_namespace, t_offline_fetch_size);

	pstNamespace->origin_fetch.offline_fetch_size = t_offline_fetch_size;
    }

    /*
     * Get the value of SMOOTH-FLOW 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 4, "*",
		"offline", "fetch",
		"smooth-flow")) {
	tbool t_smooth_flow;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_smooth_flow);
	bail_error(err);
	pstNamespace->origin_fetch.offline_fetch_smooth_flow = t_smooth_flow;
    }

    /*
     * Get the value of CONVERT-HEAD 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 3, "*",
		"convert", "head")) {
	tbool t_convert_head;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_convert_head);
	bail_error(err);
	pstNamespace->origin_fetch.convert_head = t_convert_head;
    }

    /*
     * Get the value of SET-COOKIE CACHEABLE 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 4, "*",
		"header", "set-cookie",
		"cache")) {
	tbool t_set_cookie;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_set_cookie);
	bail_error(err);
	pstNamespace->origin_fetch.is_set_cookie_cacheable = t_set_cookie;
    }
    /*
     * Get the value of SET-COOKIE CACHEABLE with no-revalidation 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 5, "*",
		"header", "set-cookie",
		"cache", "no-revalidation")) {
	tbool t_no_revalidation;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_no_revalidation);
	bail_error(err);
	pstNamespace->origin_fetch.cacheable_no_revalidation =
	    t_no_revalidation;
    }
    /*
     * Get the value of HOST-HEADER INHERIT 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 3, "*",
		"host-header", "inherit")) {
	tbool t_is_inheritable;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_is_inheritable);
	bail_error(err);

	pstNamespace->origin_fetch.is_host_header_inheritable =
	    t_is_inheritable;
    }
    /*
     * Get the value of HOST-HEADER 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 3, "*",
		"host-header",
		"header-value")) {
	tstring *t_custom_hdr = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_custom_hdr);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../origin_fetch/config/%s/host-header/header-value as : \"%s\"",
		t_namespace, ts_str_maybe_empty(t_custom_hdr));

	if (NULL != pstNamespace->origin_fetch.host_header)
	    safe_free(pstNamespace->origin_fetch.host_header);

	if (NULL != t_custom_hdr) {
	    pstNamespace->origin_fetch.host_header =
		nkn_strdup_type(ts_str(t_custom_hdr), mod_mgmt_charbuf);
	    ts_free(&t_custom_hdr);
	} else
	    pstNamespace->origin_fetch.host_header = NULL;

    }

    /*
     * Get the value of QUERY CACHE ENABLE 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 4, "*",
		"query", "cache", "enable")) {
	tbool t_is_cacheable = false;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_is_cacheable);
	bail_error(err);

	/*
	 * Bug 3610 - mgmtd uses this flag to indicate as following TRUE -
	 * allow query to be cached FALSE - disallow query to be cached NVSD 
	 * uses this flag in the exact opposite sense TRUE - DIS-allow query
	 * to be cached FALSE - allow query to be cached 
	 */
	pstNamespace->origin_fetch.disable_cache_on_query = !(t_is_cacheable);
    }
    /*
     * Get the value of QUERY STRIP 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 3, "*",
		"query", "strip")) {
	tbool t_allow_strip = false;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_allow_strip);
	bail_error(err);

	pstNamespace->origin_fetch.strip_query = t_allow_strip;
    }
    /*
     * Get the value of COOKIE CACHE ENABLE 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 4, "*",
		"cookie", "cache",
		"enable")) {
	tbool t_is_cacheable = false;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_is_cacheable);
	bail_error(err);

	pstNamespace->origin_fetch.disable_cache_on_cookie = !(t_is_cacheable);
    }
    /*
     * Get the value for max-age 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 4, "*",
		"client-request",
		"cache-control", "max_age")) {
	uint32 t_max_age_value = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_max_age_value);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../origin-fetch/config/%s/client_request/cache-control/max-age/ value as : \"%d\"",
		t_namespace, t_max_age_value);

	pstNamespace->origin_fetch.client_req_max_age = t_max_age_value;
    }
    /*
     * Get the value for action 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 4, "*",
		"client-request",
		"cache-control", "action")) {
	tstring *t_action = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_action);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../origin-fetch/config/%s/client_request/cache-control/action/ value as : \"%s\"",
		t_namespace, t_action ? ts_str(t_action) : "");

	if (t_action && ts_equal_str(t_action, "serve-from-origin", false))
	    pstNamespace->origin_fetch.client_req_serve_from_origin = true;
	else
	    pstNamespace->origin_fetch.client_req_serve_from_origin = false;

	ts_free(&t_action);
    }

    /*------------------------ HANDLE RTSP SPECIFIC ORIGIN FETCH PARAMATERES ---------------------*/
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 4, "*",
		"rtsp", "cache-directive",
		"no-cache")) {
	tstring *t_no_cache = NULL;
	const char *no_cache = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_no_cache);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read ../origin_fetch/config/%s/rtsp/cache-directive/no-cache as : \"%s\"",
		t_namespace, no_cache ? ts_str(t_no_cache) : "");

	if (NULL != pstNamespace->rtsp_origin_fetch.cache_directive_no_cache)
	    safe_free(pstNamespace->rtsp_origin_fetch.cache_directive_no_cache);

	if (NULL != t_no_cache) {
	    pstNamespace->rtsp_origin_fetch.cache_directive_no_cache =
		nkn_strdup_type(ts_str(t_no_cache), mod_mgmt_charbuf);
	} else
	    pstNamespace->rtsp_origin_fetch.cache_directive_no_cache = NULL;

	ts_free(&t_no_cache);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 4, "*",
		"rtsp", "cache-age",
		"default")) {
	uint32 t_cache_age = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_cache_age);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../origin_fetch/config/%s/rtsp/cache-age as : \"%d\"",
		t_namespace, t_cache_age);

	pstNamespace->rtsp_origin_fetch.cache_age = t_cache_age;

    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 5, "*",
		"rtsp", "content-store",
		"media",
		"cache-age-threshold")) {
	uint32 t_cache_age_threshold = 0;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL, &t_cache_age_threshold);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../origin_fetch/config/%s/rtsp/content-store"
		"/media/cache-age-threshold as : \"%d\"",
		t_namespace, t_cache_age_threshold);

	pstNamespace->rtsp_origin_fetch.content_store_cache_age_threshold =
	    t_cache_age_threshold;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 4, "*",
		"object", "revalidate",
		"time_barrier")) {

	char *time_buf = NULL;
	time_t time_barrier;

	err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &time_buf);
	bail_error(err);

	if (time_buf) {

	    lc_log_basic(jnpr_log_level,
		    "Read .../nkn/nvsd/origin_fetch/config/%s/"
		    "object/revalidate/time_barrier as :%s",
		    t_namespace, time_buf ? time_buf : " ");

	    time_barrier = atol(time_buf);
	    lc_log_basic(jnpr_log_level, "time_barrier here is %u",
		    (unsigned int) time_barrier);

	    pstNamespace->origin_fetch.cache_barrier_revalidate.reval_time =
		time_barrier;
	}

	safe_free(time_buf);

    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 4, "*",
		"object", "revalidate",
		"reval_type")) {
	char *type = NULL;

	err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &type);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../nkn/nvsd/origin_fetch/config/%s/"
		"object/revalidate/reval_type as : %s",
		t_namespace, type ? type : "");

	if (type) {
	    if (!strcmp(type, "all")) {
		pstNamespace->origin_fetch.cache_barrier_revalidate.reval_type =
		    NS_OBJ_REVAL_ALL;
	    } else if (!strcmp(type, "all_inline")) {
		pstNamespace->origin_fetch.cache_barrier_revalidate.reval_type =
		    NS_OBJ_REVAL_ALL;
		pstNamespace->origin_fetch.cache_barrier_revalidate.
		    reval_trigger = NS_OBJ_REVAL_INLINE;
	    } else if (!strcmp(type, "all_offline")) {
		pstNamespace->origin_fetch.cache_barrier_revalidate.reval_type =
		    NS_OBJ_REVAL_ALL;
		pstNamespace->origin_fetch.cache_barrier_revalidate.
		    reval_trigger = NS_OBJ_REVAL_OFFLINE;
	    } else {
		pstNamespace->origin_fetch.cache_barrier_revalidate.reval_type =
		    NS_OBJ_REVAL_NONE;
	    }
	}

	safe_free(type);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 2, "*",
		"detect_eod_on_close")) {
	tbool eod_on_origin_close;

	err = bn_binding_get_tbool(binding,
		ba_value, NULL, &eod_on_origin_close);
	bail_error(err);
	pstNamespace->origin_fetch.eod_on_origin_close = eod_on_origin_close;
    }
    /*
     * cache - Pinning 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 4, "*",
		"object", "cache_pin",
		"cache_size")) {
	uint32 t_cache_size = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_cache_size);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read ... /nkn/nvsd/origin_fetch/config/%s/object/cache_pin/cache_size as : \"%d\"",
		t_namespace, t_cache_size);

	/* convert GB into bytes and store */

	pstNamespace->origin_fetch.object.cache_pin.cache_resv_bytes = (uint64_t) t_cache_size *GB_TO_BYTES;
	/*
	 * calling the DM2 API here
	 */
	if (pstNamespace->uid)
	    if (nkn_system_inited) {
		err = DM2_mgmt_ns_set_cache_pin_diskspace(pstNamespace->uid + 1,
			pstNamespace->
			origin_fetch.object.
			cache_pin.
			cache_resv_bytes);
		bail_error(err);
	    }

    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 4, "*",
		"object", "cache_pin",
		"max_obj_size")) {
	uint32 t_max_obj_size = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_max_obj_size);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read ... /nkn/nvsd/origin_fetch/config/%s/"
		"object/cache_pin/max_obj_size as : \"%d\"",
		t_namespace, t_max_obj_size);

	/* convert KB into bytes and store */
	pstNamespace->origin_fetch.object.cache_pin.max_obj_bytes = (uint64_t) t_max_obj_size *KB_TO_BYTES;

	/*
	 * calling the DM2 API here 
	 */
	if (pstNamespace->uid && nkn_system_inited)
	    err = DM2_mgmt_ns_set_cache_pin_max_obj_size(pstNamespace->uid + 1,
		    pstNamespace->
		    origin_fetch.object.
		    cache_pin.
		    max_obj_bytes);
	bail_error(err);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 4, "*",
		"object", "cache_pin",
		"enable")) {
	tbool t_cache_pin_ena;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_cache_pin_ena);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../nkn/nvsd/origin_fetch/config/%s/object/cache_pin/enable as : \"%d\"",
		t_namespace, t_cache_pin_ena);

	pstNamespace->origin_fetch.object.cache_pin.cache_pin_ena =
	    t_cache_pin_ena;

	/*
	 * calling the DM2 API here 
	 */
	if (pstNamespace->uid && nkn_system_inited)
	    err =
		DM2_mgmt_ns_set_cache_pin_enabled(pstNamespace->uid + 1,
			t_cache_pin_ena);
	bail_error(err);

    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 4, "*",
		"object", "cache_pin",
		"pin_header_name")) {
	tstring *t_pin_header = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_pin_header);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read ... /nkn/nvsd/origin_fetch/config/%s/"
		"object/cache_pin/pin_header_name value as : \"%s\"",
		t_namespace, t_pin_header ? ts_str(t_pin_header) : "");

	if (NULL != pstNamespace->origin_fetch.object.cache_pin.pin_header)
	    safe_free(pstNamespace->origin_fetch.object.cache_pin.pin_header);

	if (NULL != t_pin_header && (!ts_equal_str(t_pin_header, " ", false))) {
	    pstNamespace->origin_fetch.object.cache_pin.pin_header =
		nkn_strdup_type(ts_str(t_pin_header), mod_mgmt_charbuf);
	    ts_free(&t_pin_header);
	} else
	    pstNamespace->origin_fetch.object.cache_pin.pin_header = NULL;

    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "object", "validaity", "start_header_name")) {
	tstring *t_start_header = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_start_header);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read ... /nkn/nvsd/origin_fetch/config/%s/"
		"object/validity/start_header_name value as : \"%s\"",
		t_namespace, t_start_header ? ts_str(t_start_header) : "");

	if (NULL != pstNamespace->origin_fetch.object.start_header)
	    safe_free(pstNamespace->origin_fetch.object.start_header);

	if (NULL != t_start_header
		&& (!ts_equal_str(t_start_header, " ", false))) {
	    pstNamespace->origin_fetch.object.start_header =
		nkn_strdup_type(ts_str(t_start_header), mod_mgmt_charbuf);
	    ts_free(&t_start_header);
	} else
	    pstNamespace->origin_fetch.object.start_header = NULL;
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 5, "*", "object", "cache_pin", "validaity",
	     "end_header_name")) {
	tstring *t_end_header = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_end_header);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read ... /nkn/nvsd/origin_fetch/config/%s/"
		"object/cache_pin/validity/end_header_name value as : \"%s\"",
		t_namespace, t_end_header ? ts_str(t_end_header) : "");

	if (NULL != pstNamespace->origin_fetch.object.cache_pin.end_header)
	    safe_free(pstNamespace->origin_fetch.object.cache_pin.end_header);

	if (NULL != t_end_header && (!ts_equal_str(t_end_header, " ", false))) {
	    pstNamespace->origin_fetch.object.cache_pin.end_header =
		nkn_strdup_type(ts_str(t_end_header), mod_mgmt_charbuf);
	    ts_free(&t_end_header);
	} else
	    pstNamespace->origin_fetch.object.cache_pin.end_header = NULL;

    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 2, "*", "redirect_302")) {
	tbool t_enable = false;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_enable);
	bail_error(err);

	pstNamespace->origin_fetch.redirect_302 = t_enable;
    }

    /*
     * Get value of ORIGIN-FETCH/ * /CLIENT-REQUEST/TUNNEL-ALL 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "client-request", "tunnel-all")) {
	tbool tunnel_all = false;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &tunnel_all);
	bail_error(err);
	pstNamespace->origin_fetch.tunnel_all = tunnel_all;
    }

    /*
     * Get value of ORIGIN_FETCH/ * /CACHE-INDEX/DOMAIN 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "cache-index", "domain-name")) {
	tbool exclude = false;
	tstring *t_exclude = NULL;

	/*
	 * t_exclude will be allocated, so free this memory 
	 */
	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_exclude);
	bail_error(err);

	if (t_exclude && (0 == strcmp(ts_str(t_exclude), "include"))) {
	    /*
	     * either namespace is delated or domain-name set to "exclude" 
	     */
	    exclude = false;
	} else {
	    exclude = true;
	}
	pstNamespace->http_cache_index.exclude_domain = exclude;
	snprintf(pstNamespace->http_cache_index.ldn_name,
		sizeof (pstNamespace->http_cache_index.ldn_name),
		"ldn-%s", t_namespace);

	ts_free(&t_exclude);
    }

    /*
     * Get value of ORIGIN_FETCH/ * /RTSP/CACHE-INDEX/DOMAIN 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "rtsp", "cache-index", "domain-name")) {
	tstring *t_exclude = NULL;
	tbool exclude = false;

	/*
	 * t_exclude will be allocated, so free this memory 
	 */
	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_exclude);
	bail_error(err);

	if (t_exclude && (0 == strcmp(ts_str(t_exclude), "include"))) {
	    exclude = false;
	} else {
	    /*
	     * either namespace is delated or domain-name set to "exclude" 
	     */
	    exclude = true;
	}
	pstNamespace->rtsp_cache_index.exclude_domain = exclude;
	snprintf(pstNamespace->rtsp_cache_index.ldn_name,
		sizeof (pstNamespace->rtsp_cache_index.ldn_name),
		"ldn-%s", t_namespace);

	ts_free(&t_exclude);
    }
    /*
     * Get value of minimum object size threshold for caching 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 2, "*", "obj-thresh-min")) {
	uint32 t_obj_size_threshold_min = 0;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL, &t_obj_size_threshold_min);
	bail_error(err);
	pstNamespace->origin_fetch.cache_obj_size_min =
	    t_obj_size_threshold_min;
    }
    /*
     * Get value of maximum object size threshold for caching 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 2, "*", "obj-thresh-max")) {
	uint32 t_obj_size_threshold_max = 0;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL, &t_obj_size_threshold_max);
	bail_error(err);
	pstNamespace->origin_fetch.cache_obj_size_max =
	    t_obj_size_threshold_max;
    }
    /*
     * Get the value of TUNNEL_OVERRIDE - auth-header 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "tunnel-override", "request",
	     "auth-header")) {
	tbool t_auth_header;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_auth_header);
	bail_error(err);
	pstNamespace->tunnel_override.auth_header = t_auth_header;
    }
    /*
     * Get the value of TUNNEL_OVERRIDE - cookie-header 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "tunnel-override", "request",
	     "cookie-header")) {
	tbool t_cookie_header;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_cookie_header);
	bail_error(err);
	pstNamespace->tunnel_override.cookie = t_cookie_header;
    }
    /*
     * Get the value of TUNNEL_OVERRIDE - query-stringr 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "tunnel-override", "request",
	     "query-string")) {
	tbool t_query_string;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_query_string);
	bail_error(err);
	pstNamespace->tunnel_override.query_string = t_query_string;
    }
    /*
     * Get the value of TUNNEL_OVERRIDE - cache-control 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "tunnel-override", "request",
	     "cache-control")) {
	tbool t_cache_control;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_cache_control);
	bail_error(err);
	pstNamespace->tunnel_override.cache_control = t_cache_control;
    }
    /*
     * Get the value of TUNNEL_OVERRIDE - cache-control-no-transform 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "tunnel-override", "response",
	     "cc-notrans")) {
	tbool t_cc_notrans;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_cc_notrans);
	bail_error(err);
	pstNamespace->tunnel_override.cc_notrans = t_cc_notrans;
    }
    /*
     * Get the value of TUNNEL_OVERRIDE - Object-Expired 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "tunnel-override", "response",
	     "obj-expired")) {
	tbool t_obj_expired;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_obj_expired);
	bail_error(err);
	pstNamespace->tunnel_override.obj_expired = t_obj_expired;
    }
    /*
     * Get the value of CACHE_REVALIDATE/METHOD 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "cache-revalidate", "method")) {
	tstring *t_method = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_method);
	bail_error(err);

	if (t_method && (ts_equal_str(t_method, "get", true))) {
	    pstNamespace->origin_fetch.cache_reval.method = REVAL_METHOD_GET;
	} else {
	    pstNamespace->origin_fetch.cache_reval.method = REVAL_METHOD_HEAD;
	}
	ts_free(&t_method);
    }
    /*
     * Get the value of CACHE-REVALIDATE/VALIDATE-HEADER 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "cache-revalidate", "validate-header")) {
	tbool validate_header = false;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &validate_header);
	bail_error(err);
	pstNamespace->origin_fetch.cache_reval.validate_header =
	    validate_header;
    }
    /*
     * Get the value of CACHE-REVALIDATE/VALIDATE-HEADER/HEADER-NAME 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "cache-revalidate", "validate-header",
	     "header-name")) {
	tstring *t_header = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_header);
	bail_error(err);

	if (NULL != pstNamespace->origin_fetch.cache_reval.header_name) {
	    safe_free(pstNamespace->origin_fetch.cache_reval.header_name);
	}

	if (t_header && ts_length(t_header) > 0) {
	    pstNamespace->origin_fetch.cache_reval.header_name =
		nkn_strdup_type(ts_str(t_header), mod_mgmt_charbuf);
	} else {
	    pstNamespace->origin_fetch.cache_reval.header_name = NULL;
	}

	/*
	 * found from valgrind 
	 */
	ts_free(&t_header);
    }
    /*
     * Get the value of etag ignore 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 5, "*", "object", "correlation", "etag", "ignore")) {
	tbool t_etag_ignore = false;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_etag_ignore);
	bail_error(err);
	pstNamespace->origin_fetch.object_correlation_etag_ignore =
	    t_etag_ignore;
    }
    /*
     * Get the value of validatorsall ignore 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 5, "*", "object", "correlation", "validatorsall",
	     "ignore")) {
	tbool t_validatorsall_ignore = false;

	err = bn_binding_get_tbool(binding,
		ba_value, NULL, &t_validatorsall_ignore);
	bail_error(err);
	pstNamespace->origin_fetch.object_correlation_validatorsall_ignore =
	    t_validatorsall_ignore;
    }
    /*
     * Get the value of aggressive threshold 
     */

      else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 3, "*",
                "header", "vary")) {
        tbool t_enable = false;

        err = bn_binding_get_tbool(binding, ba_value, NULL, &t_enable);
        bail_error(err);

        lc_log_basic(jnpr_log_level,
                "Read .../nkn/nvsd/origin_fetch/config/%s/header/vary as : \"%d\"",
                t_namespace, t_enable );
	pstNamespace->vary_enable = t_enable;
      }
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "cache-fill", "client-driven",
	     "aggressive_threshold")) {
	uint32 t_agg_threshold = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_agg_threshold);
	bail_error(err);
	pstNamespace->origin_fetch.client_driven_agg_threshold =
	    t_agg_threshold;
    }
    /*
     * Get the value of CACHE-INGEST hotness-threshold 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "cache-ingest", "hotness-threshold")) {
	uint32 t_hotness_threshold = 0;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL, &t_hotness_threshold);

	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../origin_fetch/config/%s/cache-ingest/hotness-threshold as : \"%d\"",
		t_namespace, t_hotness_threshold);

	pstNamespace->origin_fetch.cache_ingest.hotness_threshold =
	    t_hotness_threshold;
    }
    /*
     * Get the value of cache ingest incabaple byte-range origin 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "cache-ingest", "byte-range-incapable"))
	// /nkn/nvsd/origin_fetch/config/*/cache-ingest/byte-range-incapable
    {
	tbool t_inc_byte_ran_origin = false;

	err = bn_binding_get_tbool(binding,
		ba_value, NULL, &t_inc_byte_ran_origin);
	bail_error(err);
	pstNamespace->origin_fetch.cache_ingest.incapable_byte_ran_origin =
	    t_inc_byte_ran_origin;

    }
    /*
     * Get the value of CACHE-INGEST size-threshold 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "cache-ingest", "size-threshold")) {
	uint32 t_size_threshold = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_size_threshold);

	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../origin_fetch/config/%s/cache-ingest/size-threshold as : \"%d\"",
		t_namespace, t_size_threshold);

	pstNamespace->origin_fetch.cache_ingest.size_threshold =
	    t_size_threshold;
    }
    /*
     * Get the value of CONNECT METHOD ACTION 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 5, "*", "client-request", "method", "connect",
	     "handle")) {
	tbool t_connect_handle = false;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_connect_handle);
	bail_error(err);
	pstNamespace->origin_fetch.client_req_connect_handle = t_connect_handle;
    }
    /*
     * Get value of cache-index/include-header 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "cache-index", "include-header")) {
	tbool t_include_header = false;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_include_header);
	bail_error(err);
	pstNamespace->http_cache_index.include_header = t_include_header;
    }
    /*
     * Get value of cache-index/include-object 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 3, "*", "cache-index", "include-object")) {
	tbool t_include_object = false;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_include_object);
	bail_error(err);
	pstNamespace->http_cache_index.include_object = t_include_object;
    }
    /*
     * Get the value of CACHE-index.../chksumbytes 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "cache-index", "include-object",
	     "chksum-bytes")) {
	uint32 t_checksum_bytes = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_checksum_bytes);
	bail_error(err);
	pstNamespace->http_cache_index.checksum_bytes = t_checksum_bytes;
    }
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 4, "*", "cache-index", "include-object", "chksum-offset"))
	//	else if (bn_binding_name_pattern_match(ts_str(name), "/nkn/nvsd/origin_fetch/config/*/cache-index/include-object/chksum-offset"))
    {
	uint32 t_checksum_offset = 0;

	err = bn_binding_get_uint32(binding,
	           ba_value, NULL,
	           &t_checksum_offset);
        bail_error(err);

	lc_log_basic(LOG_DEBUG, "Read .../origin_fetch/config/%s/cache-index/include-object/chksum-offset as : \"%d\"",
					t_namespace, t_checksum_offset);

	pstNamespace->http_cache_index.checksum_offset = t_checksum_offset;
    }
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 4, "*", "cache-index", "include-object", "from-end"))
	//	else if (bn_binding_name_pattern_match(ts_str(name), "/nkn/nvsd/origin_fetch/config/*/cache-index/include-object/chksum-offset"))
    {
	tbool t_from_end = 0;

	err = bn_binding_get_tbool(binding,
	           ba_value, NULL,
	           &t_from_end);
        bail_error(err);

	lc_log_basic(LOG_DEBUG, "Read .../origin_fetch/config/%s/cache-index/include-object/chksum-offset as : \"%d\"",
					t_namespace, t_from_end);

	pstNamespace->http_cache_index.checksum_from_end = t_from_end;
    }
    /*
     * Get the compression status 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "object", "compression", "enable")) {

	tbool t_enabled;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_enabled);
	bail_error(err);

	/*
	 * store the flag in the global structure 
	 */
	pstNamespace->origin_fetch.compression_status = t_enabled;

    }
    /*
     * Get the value of object size min threshold for compression 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "object", "compression",
	     "obj_range_min")) {
	uint64 t_min_bytes = 0;

	err = bn_binding_get_uint64(binding, ba_value, NULL, &t_min_bytes);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../origin_fetch/config/%s/object/compression/obj_range_min as : \"%lu\"",
		t_namespace, t_min_bytes);

	pstNamespace->origin_fetch.compress_obj_min_size = t_min_bytes;
    }
    /*
     * Get the value of object size max threshold for compression 
     */
    else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "object", "compression",
	     "obj_range_max")) {
	uint64 t_max_bytes = 0;

	err = bn_binding_get_uint64(binding, ba_value, NULL, &t_max_bytes);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../origin_fetch/config/%s/object/compression/obj_range_max as : \"%lu\"",
		t_namespace, t_max_bytes);

	pstNamespace->origin_fetch.compress_obj_max_size = t_max_bytes;
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 5, "*", "object", "compression", "file_types",
	     "*")) {
	tstring *t_type = NULL;
	int i = 0;

	err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &t_type);
	bail_error(err);

	if (t_type) {
	    for (i = 0; i < MAX_NS_COMPRESS_FILETYPES; i++) {
		if (pstNamespace->origin_fetch.compress_file_types[i].type ==
			NULL
			&& pstNamespace->origin_fetch.compress_file_types[i].
			allowed == 0) {
		    pstNamespace->origin_fetch.compress_file_types[i].allowed =
			1;
		    pstNamespace->origin_fetch.compress_file_types[i].type_len =
			strlen(ts_str(t_type));
		    pstNamespace->origin_fetch.compress_file_types[i].type =
			(char *) nkn_strdup_type(ts_str(t_type),
				mod_mgmt_charbuf);
		    pstNamespace->origin_fetch.compress_file_type_cnt++;
		    break;

		}
	    }
	} else {
	    const char *t_bindtype = tstr_array_get_str_quick(name_parts, 8);

	    bail_error_null(err, t_bindtype);
	    for (i = 0; i < MAX_NS_COMPRESS_FILETYPES; i++) {
		if (pstNamespace->origin_fetch.compress_file_types[i].type
			&&
			(strcmp
			 (pstNamespace->origin_fetch.compress_file_types[i].type,
			  t_bindtype) == 0)) {
		    free(pstNamespace->origin_fetch.compress_file_types[i].
			    type);
		    pstNamespace->origin_fetch.compress_file_types[i].type =
			NULL;
		    pstNamespace->origin_fetch.compress_file_types[i].allowed =
			0;
		    pstNamespace->origin_fetch.compress_file_types[i].type_len =
			0;
		    pstNamespace->origin_fetch.compress_file_type_cnt--;
		    break;

		}
	    }
	}

	if (t_type)
	    ts_free(&t_type);

    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "object", "compression", "algorithm")) {
	tstring *t_name = NULL;

	err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &t_name);
	bail_error(err);
	if (t_name && !strcmp(ts_str(t_name), "deflate")) {
	    pstNamespace->origin_fetch.compression_algorithm = COMPRESS_DEFLATE;
	} else if (t_name && !strcmp(ts_str(t_name), "gzip")) {
	    pstNamespace->origin_fetch.compression_algorithm = COMPRESS_GZIP;
	} else {
	    pstNamespace->origin_fetch.compression_algorithm = COMPRESS_UNKNOWN;
	}

	if (t_name)
	    ts_free(&t_name);

    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "cache", "response", "*")) {
	uint32 t_resp_code = 0;
	int i = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_resp_code);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../origin_fetch/config/%s/cache/response/%u as : \"%u\"",
		t_namespace, t_resp_code, t_resp_code);
	for (i = 0; i < MAX_UNSUCCESSFUL_RESPONSE_CODES; i++) {
	    if ((pstNamespace->origin_fetch.unsuccess_response_cache[i].
			status == 0) && (t_resp_code > 0)) {
		pstNamespace->origin_fetch.unsuccess_response_cache[i].status =
		    1;
		pstNamespace->origin_fetch.unsuccess_response_cache[i].code =
		    t_resp_code;
		break;
	    }
	}
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 5, "*", "cache", "response", "*", "age")) {
	uint32_t resp_code = 0;
	int i = 0;
	const char *t_resp_code = NULL;
	uint32_t t_resp_age = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_resp_age);
	bail_error(err);
	t_resp_code = tstr_array_get_str_quick(name_parts, 7);
	if (t_resp_code) {
	    resp_code = atoi(t_resp_code);
	}

	lc_log_basic(LOG_DEBUG,
		"Read .../origin_fetch/config/%s/cache/response/%s/age as : \"%u\"",
		t_namespace, t_resp_code, t_resp_age);
	for (i = 0; i < MAX_UNSUCCESSFUL_RESPONSE_CODES; i++) {
	    if ((pstNamespace->origin_fetch.unsuccess_response_cache[i].
			status == 1)
		    && (pstNamespace->origin_fetch.unsuccess_response_cache[i].
			code == resp_code)) {

		pstNamespace->origin_fetch.unsuccess_response_cache[i].age
		    = t_resp_age;
		break;
	    }
	}
    } else if (bn_binding_name_parts_pattern_match_va (name_parts, 4,2 , "*", "fetch-page")) {
	    tbool fetch_page;

	    err = bn_binding_get_tbool(binding,
		    ba_value, NULL, &fetch_page);
	    bail_error(err);
	    pstNamespace->origin_fetch.bulk_fetch = fetch_page;

	} else if (bn_binding_name_parts_pattern_match_va (name_parts, 4,2 , "*", "fetch-page-count")) {
	    uint32_t fetch_page_count = 0;

            err = bn_binding_get_uint32(binding,
                   ba_value, NULL, &fetch_page_count);
            bail_error(err);

	    /* assigning u32 to u8 */
	    pstNamespace->origin_fetch.bulk_fetch_pages = fetch_page_count;
    }

    /*
     * Set the flag so that we know this is a dynamic change 
     */
    dynamic_change = true;

bail:
    tstr_array_free(&name_parts);
    return err;

}	/* end of * nvsd_origin_fetch_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_origin_fetch_cfg_delete_handle_change()
 *	purpose : handler delete config changes for origin_fetch policy
 */

static int
nvsd_origin_fetch_cfg_delete_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array *
	bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value,
	void *data)
{
    int err = 0;
    const tstring *name = NULL;
    const char *t_namespace = NULL;
    int ns_idx;
    tstr_array *name_parts = NULL;
    tbool *rechecked_licenses_p = data;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(bndgs_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /*
     * Check if this is our node 
     */
    if (bn_binding_name_pattern_match
	    (ts_str(name), "/nkn/nvsd/origin_fetch/config/**")) {

	/*-------- Get the Namespace ------------*/
	err = bn_binding_get_name_parts(binding, &name_parts);
	bail_error(err);
	t_namespace = tstr_array_get_str_quick(name_parts, 4);

	bail_error(err);
	lc_log_basic(jnpr_log_level,
		"Read .../nkn/nvsd/origin_fetch/config as : \"%s\"",
		t_namespace);
	if (search_namespace(t_namespace, &ns_idx) != 0)
	    goto bail;

	pstNamespace = get_namespace(ns_idx);
    } else {
	/*
	 * This is not the origin-policy node hence leave 
	 */
	goto bail;
    }

    /*
     * Get the value of CACHE-DIRECTIVE NO-CACHE 
     */
    if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 5, "*", "object", "compression", "file_types", "*")) {
	tstring *t_type = NULL;
	int i = 0;

	err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &t_type);
	bail_error(err);
	for (i = 0; i < MAX_NS_COMPRESS_FILETYPES; i++) {
	    if (pstNamespace->origin_fetch.compress_file_types[i].type
		    &&
		    (strcmp
		     (pstNamespace->origin_fetch.compress_file_types[i].type,
		      ts_str(t_type)) == 0)) {
		free(pstNamespace->origin_fetch.compress_file_types[i].type);
		pstNamespace->origin_fetch.compress_file_types[i].type = NULL;
		pstNamespace->origin_fetch.compress_file_types[i].allowed = 0;
		pstNamespace->origin_fetch.compress_file_types[i].type_len = 0;
		pstNamespace->origin_fetch.compress_file_type_cnt--;
		break;

	    }
	}
	ts_free(&t_type);
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 4, "*", "cache", "response", "*")) {
	uint32 t_resp_code = 0;
	int i = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_resp_code);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../origin_fetch/config/%s/cache/response/%u as : \"%u\"",
		t_namespace, t_resp_code, t_resp_code);

	for (i = 0; i < MAX_UNSUCCESSFUL_RESPONSE_CODES; i++) {
	    if ((pstNamespace->origin_fetch.unsuccess_response_cache[i].
			status == 1)
		    && (pstNamespace->origin_fetch.unsuccess_response_cache[i].
			code == t_resp_code)) {

		pstNamespace->origin_fetch.unsuccess_response_cache[i].status =
		    0;
		pstNamespace->origin_fetch.unsuccess_response_cache[i].code = 0;
		pstNamespace->origin_fetch.unsuccess_response_cache[i].age = 0;
		break;
	    }
	}
    }
bail:
    tstr_array_free(&name_parts);
    return err;
}

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_origin_fetch_cache_age_handle_change()
 *	purpose : handler for config changes for content-type under origin_fetch policy
 */

static int
nvsd_origin_fetch_cache_age_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array *
	bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value,
	void *data)
{
    int i = 0;
    int err = 0;
    int ns_idx = 0;
    const tstring *name = NULL;
    const char *t_temp_str = NULL;
    const char *t_namespace = NULL;
    tbool *rechecked_licenses_p = data;
    uint32 t_index = 0;
    tstr_array *name_parts = NULL;
    cache_age_content_type_t *pst_cache_age_map;

    (void) i;
    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(bndgs_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /*
     * Check if this is our node 
     */
    if (bn_binding_name_pattern_match
	    (ts_str(name),
	     "/nkn/nvsd/origin_fetch/config/*/cache-age/content-type/**")) {

	/*---------------- Get the Namespace ------------*/
	err = bn_binding_get_name_parts(binding, &name_parts);
	bail_error(err);
	t_namespace = tstr_array_get_str_quick(name_parts, 4);

	lc_log_basic(jnpr_log_level,
		"Read .../nkn/nvsd/origin_fetch/config as : \"%s\"",
		t_namespace);
	if (search_namespace(t_namespace, &ns_idx) != 0)
	    goto bail;
	pstNamespace = get_namespace(ns_idx);
    } else {
	/*
	 * This is not the node we are interested in.. hence bail 
	 */
	goto bail;
    }

    /*
     * Get the index first 
     */
    t_temp_str = tstr_array_get_str_quick(name_parts, 7);
    if (!t_temp_str)
	goto bail;				// sanity check

    t_index = atoi(t_temp_str);

    /*
     * Since node index starts with 1, decrement by 1 for array index 
     */
    t_index--;
    if (MAX_CONTENT_TYPES <= t_index)
	goto bail;

    /*
     * Now get a pointer to the cache_age_map array entry 
     */
    pst_cache_age_map = &(pstNamespace->origin_fetch.cache_age_map[t_index]);

    /*
     * Get the value of CACHE-AGE/CONTENT-TYPE/[] 
     */
    if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 5, "*", "cache-age", "content-type", "*", "type")) {
	tstring *t_content_type = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_content_type);
	bail_error(err);

	/*
	 * Check if value already exists, if so free it 
	 */
	if (pst_cache_age_map->content_type) {
	    safe_free(pst_cache_age_map->content_type);
	    pst_cache_age_map->content_type = NULL;
	}

	/*
	 * In case of delete the value comes as NULL hence, check for NULL 
	 */
	if (t_content_type) {
	    lc_log_basic(jnpr_log_level,
		    "Read .../origin-fetch/config/%s/catche-age/content-type/%d/type as : \"%s\"",
		    t_namespace, t_index,
		    (ts_str(t_content_type)) ? ts_str(t_content_type) :
		    "");
	    pst_cache_age_map->content_type =
		nkn_strdup_type(ts_str(t_content_type), mod_mgmt_charbuf);
	    /*
	     * Since this is an add, set the index + 1 
	     */
	    pstNamespace->origin_fetch.content_type_count = t_index + 1;
	}
	ts_free(&t_content_type);
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 5, "*", "cache-age", "content-type", "*", "value")) {
	    uint32 t_cache_age_value = 0;

	    err = bn_binding_get_uint32(binding,
		    ba_value, NULL, &t_cache_age_value);

	    bail_error(err);

	    lc_log_basic(jnpr_log_level,
		    "Read .../origin-fetch/config/%s/catche-age/content-type/%d/value as : \"%d\"",
		    t_namespace, t_index, t_cache_age_value);

	    pst_cache_age_map->cache_age = t_cache_age_value;
	}

bail:
    tstr_array_free(&name_parts);
    return err;
}	/* end of nvsd_origin_fetch_cache_age_handle_change */

/* ------------------------------------------------------------------------- */
/*
 *	funtion : nvsd_mgmt_get_nth_namespace()
 *	purpose : api for getting the nth namespace entry.  All namespaces
 *		are considered in this function, including ones which have
 *		been deleted.
 */
namespace_node_data_t *
nvsd_mgmt_get_nth_namespace(int nth)
{
    namespace_node_data_t *temp = NULL;

    /*
     * Sanity Check 
     */
    if (nth < 0)
	return (NULL);
    if (nth >= g_lstNamespace.namespace_counter)
	return (NULL);

    lc_log_basic(LOG_INFO, "Request received for active namespace #%d", nth);

    /*
     * Find the nth namespace 
     */
    temp = get_namespace(nth);
    return temp;
}	/* end of nvsd_mgmt_get_nth_namespace */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_mgmt_get_nth_namespace()
 *	purpose : api for getting the nth active namespace entry.  Inactive
 *		namespaces are completely ignored.
 */
namespace_node_data_t *
nvsd_mgmt_get_nth_active_namespace(int nth)
{
    int i, j;

    /*
     * Sanity Check 
     */
    if (nth < 0)
	return (NULL);
    if (nth >= g_lstNamespace.namespace_counter)
	return (NULL);

    lc_log_basic(LOG_INFO, "Request received for active namespace #%d", nth);

    /*
     * Find the nth active namespace 
     */
    namespace_node_data_t *temp = NULL;

    i = j = 0;
    while (j < g_lstNamespace.namespace_counter) {
	temp = get_namespace(j);
	if (temp && temp->active) {
	    int has_invalid_smap = 0;

	    /*
	     * check if servermap is present and if they have any server-map
	     * nodes with refresh status as false and no binary data 
	     */

	    if (temp->origin_server.http_smap_count > 0) {
		int idx;

		for (idx = 0; idx < temp->origin_server.http_smap_count; idx++) {
		    server_map_node_data_t *smap = NULL;

		    smap = temp->origin_server.http_server_map[idx];
		    if ((smap->state != 1) ||
			    !smap->binary_server_map || !smap->binary_size) {
			has_invalid_smap = 1;
			break;
		    }
		}
	    }

	    if (temp->num_clusters > 0) {
		int idx;

		for (idx = 0; idx < temp->num_clusters; idx++) {
		    server_map_node_data_t *smap = NULL;

		    smap = temp->clusterMap[idx]->serverMap;
		    if ((smap->state != 1) ||
			    !smap->binary_server_map || !smap->binary_size) {
			has_invalid_smap = 1;
			break;
		    }
		}
	    }

	    if (has_invalid_smap) {
		++j;
		continue;
	    }

	    if (i == nth)
		return temp;
	    else
		++i;
	}
	j++;
    }
    /*
     * No match 
     */
    return (NULL);
}	/* end of * nvsd_mgmt_get_nth_active_namespace() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_mgmt_get_cache_pin_config()
 *	purpose : api for getting the cache pin configuration information
 *		from a namespace_node_data_t object.  Don't want other
 *		modules to see the indirection calls.
 */
void
nvsd_mgmt_get_cache_pin_config(namespace_node_data_t * tm_ns_info,
	uint64_t * max_obj_bytes,
	uint64_t * resv_bytes, int *pin_enabled)
{
    *max_obj_bytes = tm_ns_info->origin_fetch.object.cache_pin.max_obj_bytes;
    *resv_bytes = tm_ns_info->origin_fetch.object.cache_pin.cache_resv_bytes;
    *pin_enabled = tm_ns_info->origin_fetch.object.cache_pin.cache_pin_ena;
}	/* end of nvsd_mgmt_get_cache_pin_config */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_mgmt_ns_sas_config()
 *	purpose : api for getting the namespace specific sas cache-tier 
 *		configuration information
 *		from a namespace_node_data_t object.  Don't want other
 *		modules to see the indirection calls.
 */
void
nvsd_mgmt_ns_sas_config(namespace_node_data_t * tm_ns_info,
	uint32_t * read_size,
	uint32_t * free_block_threshold,
	int *group_read_enable, size_t * size_in_mb)
{
    *read_size = tm_ns_info->media_cache.sas.read_size;
    *free_block_threshold = tm_ns_info->media_cache.sas.free_block_thres;
    *group_read_enable =
	(tm_ns_info->media_cache.sas.group_read_enable) ? 1 : 0;
    *size_in_mb = tm_ns_info->media_cache.sas.max_disk_usage;
}	/* end of nvsd_mgmt_ns_sas_config */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_mgmt_ns_sata_config()
 *	purpose : api for getting the namespace specific sata cache-tier 
 *		configuration information
 *		from a namespace_node_data_t object.  Don't want other
 *		modules to see the indirection calls.
 */
void
nvsd_mgmt_ns_sata_config(namespace_node_data_t * tm_ns_info,
	uint32_t * read_size,
	uint32_t * free_block_threshold,
	int *group_read_enable, size_t * size_in_mb)
{
    *read_size = tm_ns_info->media_cache.sata.read_size;
    *free_block_threshold = tm_ns_info->media_cache.sata.free_block_thres;
    *group_read_enable =
	(tm_ns_info->media_cache.sata.group_read_enable) ? 1 : 0;
    *size_in_mb = tm_ns_info->media_cache.sata.max_disk_usage;
}	/* end of nvsd_mgmt_ns_sata_config */

/* ------------------------------------------------------------------------- */

/*
 *	function : nvsd_mgmt_ns_ssd_config()
 *	purpose : api for getting the namespace specific ssd cache-tier
 *		configuration information
 *		from a namespace_node_data_t object.  Don't want other
 *		modules to see the indirection calls.
 */
void
nvsd_mgmt_ns_ssd_config(namespace_node_data_t * tm_ns_info,
	uint32_t * read_size,
	uint32_t * free_block_threshold,
	int *group_read_enable, size_t * size_in_mb)
{
    *read_size = tm_ns_info->media_cache.ssd.read_size;
    *free_block_threshold = tm_ns_info->media_cache.ssd.free_block_thres;
    *group_read_enable =
	(tm_ns_info->media_cache.ssd.group_read_enable) ? 1 : 0;
    *size_in_mb = tm_ns_info->media_cache.ssd.max_disk_usage;
}	/* end of nvsd_mgmt_ns_ssd_config */

/* ------------------------------------------------------------------------- */
/*
 *	funtion : remove_virtual_player_in_namespace ()
 *	purpose : api for removing the virtual player reference in the
 *	namespace
 */
void
remove_virtual_player_in_namespace(const char *t_virtual_player)
{
    int i;

    /*
     * Sanity Check 
     */
    if (!t_virtual_player)
	return;

    namespace_node_data_t *temp = NULL;

    /*
     * Iterate thru the Namespace list 
     */
    for (i = 0; i < g_lstNamespace.namespace_counter; ++i) {
	temp = get_namespace(i);
	if (temp && temp->virtual_player_name &&
		(strcmp(t_virtual_player, temp->virtual_player_name) == 0)) {
	    safe_free(temp->virtual_player_name);
	    temp->virtual_player_name = NULL;
	    temp->virtual_player = NULL;
	}
    }

    return;
}	/* end of remove_virtual_player_in_namespace() */

/* ------------------------------------------------------------------------- */
/*
 *	funtion : remove_pub_point_in_namespace()
 *	purpose : api for removing the publish point reference in the
 *	namespace
 */
void
remove_pub_point_in_namespace(const char *t_pub_point)
{
    int i = 0;
    namespace_node_data_t *temp = NULL;

    /*
     * Sanity Check
     */
    if (!t_pub_point)
	return;

    /*
     * Iterate thru the Namespace list
     */
    for (i = 0; i < g_lstNamespace.namespace_counter; i++) {
	temp = get_namespace(i);
	if (temp != NULL) {
	    if (temp->pub_point_name &&
		    (0 == strcmp(t_pub_point, temp->pub_point_name))) {
		free(temp->pub_point_name);
		temp->pub_point_name = NULL;
		temp->pub_point = NULL;
	    }
	}
    }

    return;
}

/* ------------------------------------------------------------------------- */
/*
 *	funtion : nvsd_mgmt_namespace_action_hdlr ()
 *	purpose : the api to the namespace module to handle the various
 *		actions to give the cache path.
 */
int
nsvd_mgmt_ns_is_obj_cached(const char *cp_namespace,
	const char *cp_uid,
	const char *incoming_uri, char **ret_cache_name)
{
    int incoming_uri_len = 0;
    nkn_cod_t cod, next;
    int ret = 0;

    cod = NKN_COD_NULL;
    next = NKN_COD_NULL;
    char *uri = NULL;
    int urilen;
    int plen = strlen(cp_uid);

    UNUSED_ARGUMENT(cp_namespace);
    UNUSED_ARGUMENT(cp_uid);

    incoming_uri_len = strlen(incoming_uri);
    do {
	// Thilak - should this be char * ?
	ret = nkn_cod_get_next_entry(cod, (char *) cp_uid, plen, &next, &uri,
		    &urilen, NKN_COD_NS_MGR);
	if (ret)
	    break;

	if (uri && urilen && (urilen > incoming_uri_len)) {
	    *ret_cache_name = strncmp(uri + (urilen - incoming_uri_len),
		    incoming_uri,
		    incoming_uri_len) ? nkn_strdup_type("",
			mod_mgmt_charbuf)
			: nkn_strdup_type(uri, mod_mgmt_charbuf);
	    break;
	}
	cod = next;
	nkn_cod_close(next, NKN_COD_NS_MGR);
    } while (next != NKN_COD_NULL);

    return 0;
}

/* ------------------------------------------------------------------------- */
/*
 *	funtion : nvsd_mgmt_namespace_action_hdlr ()
 *	purpose : the api to the namespace module to handle the various
 *		actions from northbound interfaces
 */
int
nsvd_mgmt_ns_get_ramcache_list(const char *cp_namespace,
	const char *cp_uid, const char *cp_filename)
{
    int fd;
    int ns_idx = 0;
    int object_count = 0;
    nkn_cod_t cod, next;
    int ret, plen = 0;
    nkn_uol_t uol = { 0, 0, 0 };
    char cp_buf[512];
    char cp_ext_buf[MAX_RAM_CACHE_URI_SIZE];
    char cp_expiry_time[26];
    const char *cp_url;
    time_t curr_time;

    /*
     * Sanity 
     */
    if (!cp_namespace || !cp_uid || !cp_filename)
	return (-1);
    memset(cp_buf, 0, 512);
    memset(cp_expiry_time, 0, 26);

    /*
     * Get the namespace entry in the global array 
     */
    if (search_namespace(cp_namespace, &ns_idx) != 0) {
	/*
	 * Bail out 
	 */
	return (-1);
    }

    /*
     * Open the filename provided 
     */
    fd = open(cp_filename, O_CREAT | O_WRONLY,
	    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd == -1)
	return (-1);

    /*
     * Print the header 
     */
    curr_time = time(NULL);
    snprintf(cp_buf, sizeof (cp_buf), "Generated on : %s\n%s%s\n%s\n",
	    ctime(&curr_time), "Objects in cache for namespace : ",
	    cp_namespace,
	    "------------------------------------------------------"
	    "--------------------------------- ");
    write(fd, cp_buf, strlen(cp_buf));

    /*
     * Now read the ramcache and write it into the file 
     */
    cod = NKN_COD_NULL;
    next = NKN_COD_NULL;
    plen = strlen(cp_uid);

    do {
	nkn_buffer_t *abuf;

	ret =
	    nkn_cod_get_next_entry(cod, (char *) cp_uid, plen, &next, NULL,
		    NULL, NKN_COD_NS_MGR);
	if (ret)
	    break;
	cod = next;
	uol.cod = cod;
	abuf = nkn_buffer_get(&uol, NKN_BUFFER_ATTR);
	if (abuf) {
	    nkn_buffer_stat_t stbuf;
	    nkn_attr_t *ap;

	    nkn_buffer_getstats(abuf, &stbuf);
	    ap = (nkn_attr_t *) nkn_buffer_getcontent(abuf);

	    /*
	     * Get the expiry time without the trailing \n 
	     */
	    strncpy(cp_expiry_time, ctime(&ap->cache_expiry), 24);

	    /*
	     * Get the URL and skip UID part with the underscore 
	     */
	    cp_url = nkn_cod_get_cnp(cod);
	    if (cp_url == NULL || *cp_url == '\0')
		continue;

	    if (cp_url && (strlen(cp_url) > strlen(cp_uid)))
		cp_url += (strlen(cp_uid) + 1);

	    /*
	     * Check if the bytes cached is more than 0 
	     */
	    /*
	     * There could be object with 0 bytes cached meaning RAM cache
	     * knows the attributes but does not have the data yet and those
	     * we do not want to show 
	     */
	    if (stbuf.bytes_cached) {
		/*
		 * For every REPEAT_HDR lines add a header line 
		 */
		if (0 == object_count % REPEAT_HDR) {
		    snprintf(cp_buf, sizeof (cp_buf),
			    "*%5s %7s %10s %s %24s %20s\n", "Loc", "Pinned",
			    "Size (KB)", "Compressed", "Expiry        ",
			    "          URL          ");
		    /*
		     * Reverting the cache pinning changes since it is not
		     * going to be part of R2.1 
		     */
		    /*
		     * Enabling the cache-pinning for bug 8344
		     */
		    write(fd, cp_buf, strlen(cp_buf));
		    snprintf(cp_buf, sizeof (cp_buf),
			    "*-------------------------------"
			    "-----------------------------------"
			    "--------------------\n");
		    write(fd, cp_buf, strlen(cp_buf));
		}

		/*
		 * Create the line to be written 
		 */
		char type;

		if ((ap->na_flags & NKN_OBJ_COMPRESSED)
			&& (ap->encoding_type == HRF_ENCODING_DEFLATE)) {
		    type = 'D';
		} else if ((ap->na_flags & NKN_OBJ_COMPRESSED)
			&& (ap->encoding_type == HRF_ENCODING_GZIP)) {
		    type = 'G';
		} else {
		    type = '-';
		}
		snprintf(cp_ext_buf, sizeof (cp_ext_buf),
			" %5s %4s    %10ld    %4c    %24s  %s\n",
			"RAM", "N", (stbuf.bytes_cached / 1024),
			type, cp_expiry_time, cp_url);
		write(fd, cp_ext_buf, strlen(cp_ext_buf));
		object_count++;
	    }

	    nkn_buffer_release(abuf);
	}
	nkn_cod_close(next, NKN_COD_NS_MGR);
    } while (next != NKN_COD_NULL);

    /*
     * Now close the fd 
     */
    close(fd);
    return (object_count);
}	/* nsvd_mgmt_ns_get_ramcache_list () */

/* ------------------------------------------------------------------------- */
/*
 *	funtion : remove_server_map_in_namespace ()
 *	purpose : api for removing the virtual player reference in the
 *	namespace
 */
void
remove_server_map_in_namespace(const char *t_server_map)
{
    int i, j;

    /*
     * Sanity Check 
     */
    if (!t_server_map)
	return;
    /*
     * Iterate thru the Namespace list 
     */
    namespace_node_data_t *temp = NULL;

    for (i = 0; i < g_lstNamespace.namespace_counter; i++) {
	temp = get_namespace(i);
	if (temp != NULL) {
	    if (temp->origin_server.proto == OSF_HTTP_SERVER_MAP) {
		if (temp->origin_server.http_server_map[0] &&
			strcmp(temp->origin_server.http_server_map[0]->name,
			    t_server_map) == 0) {
		    temp->origin_server.http_server_map[0] = NULL;
		    safe_free(temp->origin_server.http_server_map_name[0]);
		    temp->origin_server.http_server_map_name[0] = NULL;
		}
	    } else if ((temp->origin_server.proto == OSF_NFS_SERVER_MAP)) {

		if (temp->origin_server.nfs_server_map &&
			strcmp(temp->origin_server.nfs_server_map->name,
			    t_server_map) == 0) {
		    temp->origin_server.nfs_server_map = NULL;
		    safe_free(temp->origin_server.nfs_server_map_name);
		    temp->origin_server.nfs_server_map_name = NULL;
		}

	    } else if (temp->origin_server.proto == OSF_HTTP_NODE_MAP) {
		for (j = 0; j < temp->origin_server.http_smap_count; j++) {
		    if (temp->origin_server.http_server_map[j] &&
			    strcmp(temp->origin_server.http_server_map[j]->name,
				t_server_map) == 0) {
			temp->origin_server.http_server_map[j] = NULL;
			safe_free(temp->origin_server.http_server_map_name[j]);
			temp->origin_server.http_server_map_name[j] = NULL;
		    }
		}
	    }
	}
    }
    return;
}	/* end of remove_server_map_in_namespace () */

/* ------------------------------------------------------------------------- */
/*
 *	funtion : addable_ns_uid()
 *	purpose : check if the uid is valid and not already in the list
 */
tbool
addable_ns_uid(const char *uid2chk, const char *lst_nsuids)
{
    int i;
    char *cp_uid;
    tbool retval = false;

    (void) i;
    (void) retval;

    /*
     * Sanity 
     */
    if (!uid2chk || !lst_nsuids)
	return false;

    /*
     * First check the entries that we know for sure to skip 
     */
    if (!strcmp(uid2chk, ".") || !strcmp(uid2chk, "..") ||
	    !strcmp(uid2chk, "lost+found"))
	return false;

    /*
     * Now check if the uid already exists in the list 
     */
    cp_uid = strstr(lst_nsuids, uid2chk);
    if (cp_uid)
	return false;			// the uid already exists in the list

    /*
     * Seems fine to add the uid 
     */
    return true;

}	/* end of addable_ns_uid () */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : configured_ns_uid()
 *	purpose : check if this is a UID of a configured namespace
 */
tbool
configured_ns_uid(const char *uid2chk)
{
    int i;
    char *cp_uid;
    tbool retval = false;

    /*
     * Sanity 
     */
    if (!uid2chk)
	return false;

    namespace_node_data_t *temp = NULL;

    /*
     * Loop thru the namespace array 
     */
    for (i = 0; i < g_lstNamespace.namespace_counter; ++i) {
	if ((temp = get_namespace(i)) == NULL)
	    continue;			// this should never happen

	/*
	 * Get a pointer to UID and skip the leading fwd slash 
	 */
	cp_uid = temp->uid;
	if (!cp_uid)
	    continue;

	/*
	 * Skip the leading fwd slash 
	 */
	cp_uid++;

	if (!strcmp(cp_uid, uid2chk)) {
	    retval = true;
	    break;				/* Jump out of the loop */
	}

    }

    return retval;
}	/* end of configured_ns_uid () */

/* get_namespace() -- Get the namespace pointer for the given index.
 * This inline function is defined in nvsd_mgmt_inline.h
 *     inline namespace_node_data_t * get_namespace(int n_index)
 */

/*! set the namespace pointer for the given n_index */
static inline void
set_namespace(int n_index, namespace_node_data_t * pointer)
{
    if (n_index < 0 && n_index >= NKN_MAX_NAMESPACES) {
	lc_log_basic(LOG_INFO, "invalid index for set_namespace %d", n_index);
	return;
    }
    g_lstNamespace.pNamespace[n_index] = pointer;
}

static int
nvsd_mgmt_namespace_clear_current_match(namespace_node_data_t * pNs)
{
    int err = 0;

    switch (pNs->ns_match.type) {
	case MATCH_NONE:
	    // nothing to free here
	    break;
	case MATCH_URI_NAME:
	case MATCH_URI_REGEX:
	    if (pNs->ns_match.match.uri.uri_prefix)
		free(pNs->ns_match.match.uri.uri_prefix);
	    if (pNs->ns_match.match.uri.uri_prefix_regex)
		free(pNs->ns_match.match.uri.uri_prefix_regex);
	    break;
	case MATCH_HEADER_NAME:
	case MATCH_HEADER_REGEX:
	    if (pNs->ns_match.match.header.name)
		free(pNs->ns_match.match.header.name);
	    if (pNs->ns_match.match.header.value)
		free(pNs->ns_match.match.header.value);
	    if (pNs->ns_match.match.header.regex)
		free(pNs->ns_match.match.header.regex);
	    break;
	case MATCH_QUERY_STRING_NAME:
	case MATCH_QUERY_STRING_REGEX:
	    if (pNs->ns_match.match.qstring.name)
		free(pNs->ns_match.match.qstring.name);
	    if (pNs->ns_match.match.qstring.value)
		free(pNs->ns_match.match.qstring.value);
	    if (pNs->ns_match.match.qstring.regex)
		free(pNs->ns_match.match.qstring.regex);
	    break;
	case MATCH_VHOST:
	    if (pNs->ns_match.match.virhost.fqdn) {
		free(pNs->ns_match.match.virhost.fqdn);
		pNs->ns_match.match.virhost.port = 0;
	    }
	    break;

	default:
	    break;

    }

    switch (pNs->rtsp_ns_match.type) {
	case MATCH_NONE:
	    break;
	case MATCH_RTSP_URI_NAME:
	    if (pNs->rtsp_ns_match.match.uri.uri_prefix)
		free(pNs->rtsp_ns_match.match.uri.uri_prefix);
	    if (pNs->rtsp_ns_match.match.uri.uri_prefix_regex)
		free(pNs->rtsp_ns_match.match.uri.uri_prefix_regex);
	    break;
	case MATCH_RTSP_VHOST:
	    if (pNs->rtsp_ns_match.match.virhost.fqdn) {
		free(pNs->rtsp_ns_match.match.virhost.fqdn);
		pNs->rtsp_ns_match.match.virhost.port = 0;
	    }
	    break;
	default:
	    break;
    }
    goto bail;

bail:
    return err;
}

static void
free_members(namespace_node_data_t * namespace)
{
    int i, j;
    /*
     * ! check for namespace if not return 
     */
    if (!namespace)
	return;

    if (namespace->namespace)
	free(namespace->namespace);

    if (namespace->uid)
	free(namespace->uid);

    if (namespace->domain)
	free(namespace->domain);

    if (namespace->domain_regex) {
	free(namespace->domain_regex);
	nkn_regex_ctx_free(namespace->domain_comp_regex);
    }

    if (namespace->virtual_player_name) {
	free(namespace->virtual_player_name);
	namespace->virtual_player = NULL;
    }
    for (i = 0;i < namespace->num_domain_fqdns; i++) {
	safe_free(namespace->domain_fqdns[i]);

    }
    if (namespace->domain_regex){
	free(namespace->domain_regex);
	nkn_regex_ctx_free(namespace->domain_comp_regex);
    }
    for (i = 0; i < namespace->vip_obj.n_vips; i++) {
	safe_free(namespace->vip_obj.vip[i]);
    }

    /*
     * ! match clean up for the given namespace 
     */
    nvsd_mgmt_namespace_clear_current_match(namespace);

    /*
     * ! origin server clean up for the given namespace 
     */
    switch (namespace->origin_server.proto) {
	case OSF_NONE:
	    break;
	case OSF_HTTP_HOST:
	case OSF_HTTP_PROXY_HOST:
	    i = 0;
	    while (i < MAX_HTTP_ORIGIN_SERVERS) {
		if (NULL != namespace->origin_server.http.host) {
		    safe_free(namespace->origin_server.http.host);
		}
		if (namespace->origin_server.http.aws.active) {
			safe_free(namespace->origin_server.http.aws.access_key);
			safe_free(namespace->origin_server.http.aws.secret_key);
			namespace->origin_server.http.aws.active = 0;
		}
		if (namespace->origin_server.http.dns_query) {
			safe_free(namespace->origin_server.http.dns_query);
		}
		++i;
	    }
	    break;
	case OSF_HTTP_ABS_URL:
	    break;
	case OSF_HTTP_FOLLOW_HEADER:
	    if (namespace->origin_server.follow.dest_header)
		free(namespace->origin_server.follow.dest_header);
	    if (namespace->origin_server.follow.aws.active) {
		free(namespace->origin_server.follow.aws.access_key);
		free(namespace->origin_server.follow.aws.secret_key);
		namespace->origin_server.follow.aws.active = 0;
	    }
	    if (namespace->origin_server.follow.dns_query) {
		free(namespace->origin_server.follow.dns_query);
	    }
	    break;
	case OSF_HTTP_DEST_IP:
	    break;
	case OSF_HTTP_SERVER_MAP:
	    if (namespace->origin_server.http_server_map[0]) {
		namespace->origin_server.http_server_map[0] = NULL;
		safe_free(namespace->origin_server.http_server_map_name[0]);
	    }
	    break;
	case OSF_NFS_SERVER_MAP:
	    if (namespace->origin_server.nfs_server_map) {
		namespace->origin_server.nfs_server_map = NULL;
		safe_free(namespace->origin_server.nfs_server_map_name);
	    }
	    break;
	case OSF_NFS_HOST:
	    if (namespace->origin_server.nfs.host) {
		safe_free(namespace->origin_server.nfs.host);
	    }
	case OSF_RTSP_HOST:
	    if (namespace->origin_server.rtsp.host) {
		safe_free(namespace->origin_server.rtsp.host);
	    }
	    break;

	case OSF_RTSP_DEST_IP:
	    // Do nothing
	    break;
	case OSF_HTTP_NODE_MAP:
	    for (j = 0; j < namespace->origin_server.http_smap_count; j++) {
		if (namespace->origin_server.http_server_map[j]) {
		    namespace->origin_server.http_server_map[j] = NULL;
		    safe_free(namespace->origin_server.http_server_map_name[j]);
		    namespace->origin_server.http_server_map_name[j] = NULL;
		}
	    }
	    break;
    }
#ifdef PRELOAD_UF_DATA
    	if (namespace->url_filter.uf_trie_data) {
	    delete_url_filter_trie(namespace->url_filter.uf_trie_data);
	    namespace->url_filter.uf_trie_data = 0;
    	}
#endif

}

/*! delete namespace pointer from the available namespace list*/
static int
delete_namespace(int n_index)
{
    int i;
    char statname[1024];
    namespace_node_data_t *temp = get_namespace(n_index);

    if (!temp) {
	lc_log_basic(LOG_INFO, "empty namespace for deleting index=%d", n_index);
	return -1;
    }

    snprintf(statname, sizeof (statname), "glob_namespace_bkup_%s_gets",
	    temp->namespace);
    statname[sizeof (statname) - 1] = '\0';
    nkn_mon_delete(statname, NULL);
    /*
     * ! first free the members with in the namespace 
     */
    free_members(temp);
    /*
     * ! free the namespace 
     */
    free(temp);
    /*
     * ! set the index position to null value 
     */
    set_namespace(n_index, NULL);
    /*
     * ! if the index is not last then move up one position 
     */
    for (i = n_index + 1; i < g_lstNamespace.namespace_counter; ++i) {
	temp = get_namespace(i);
	set_namespace(i - 1, temp);
    }
    /*
     * Set the last index to NULL after we move 
     */
    set_namespace(i - 1, NULL);
    --g_lstNamespace.namespace_counter;
    return 0;
}

/*! insert namespace pointer based on precedence */
static void
insert_list_by_preference(namespace_node_data_t * new_ns, int *return_idx)
{
    int i = g_lstNamespace.namespace_counter;
#if 0
    namespace_node_data_t *curr_ns = NULL;
    for (i =0; i < g_lstNamespace.namespace_counter; ++i)
    {
	curr_ns = get_namespace(i);
	if (curr_ns){
	    /*! if domain regex of the current node contains the
	      incoming namespace domain then match the precedence */
	    if (curr_ns->domain_regex &&
		    (strlen(curr_ns->domain_regex)> 0) &&
		    (nkn_regexec(curr_ns->domain_comp_regex,
				 new_ns->domain, NULL, 0) == 0)){
		/*! if domain is "*" any then directly compare
		  precedence */
		if (new_ns->precedence < curr_ns->precedence)
		    break;
	    }
	    else {
		/*! if domain of the current node contains the incoming
		  namespace domain then match the precedence */
		if (curr_ns->domain  && new_ns->domain
			&& strlen(curr_ns->domain) > 0){
		    /*! if domain is "*" any then directly compare
		      precedence */
		    if ((strcmp(curr_ns->domain, "*") == 0) ||
			    ((strcmp(new_ns->domain, "*") == 0))){
			if (new_ns->precedence < curr_ns->precedence)
			    break;
		    } else if (strcmp(curr_ns->domain,new_ns->domain) == 0){
			if (new_ns->precedence < curr_ns->precedence)
			    break;
		    }
		}
	    }
	}
    }
#endif
    /*! if the namespace comparison is success then
      set incoming namespace to current position and move
      the remaining one step down */
    if (i != (g_lstNamespace.namespace_counter )){
#if 0
	namespace_node_data_t *curr_ns_curr_ns = curr_ns;
	set_namespace(i, new_ns);
	*return_idx = i;
	while (1){
	    ++i;
	    curr_ns = get_namespace(i);
	    if (curr_ns == NULL)
		break;
	    set_namespace(i, curr_ns_curr_ns);
	    curr_ns_curr_ns = curr_ns;
	}
	set_namespace(i, curr_ns_curr_ns);
#endif
    } else {
	/*! if the namespace is not in the current
	  comparison list then set the new namespace
	  at the end */
	set_namespace(i, new_ns);
	*return_idx = i;
    }
    ++g_lstNamespace.namespace_counter;
}

/*! search the namespace in the available namespace list */
int
search_namespace(const char *ns, int *return_idx)
{
    int i;
    namespace_node_data_t *temp = NULL;

    for (i = 0; i < g_lstNamespace.namespace_counter; ++i) {
	temp = get_namespace(i);
	if (temp != NULL) {
	    if (strcmp(ns, temp->namespace) == 0) {
		*return_idx = i;
		return 0;
	    }
	}
    }
    return -1;
}

/*! insert namespace first searches for the specific namespace
  if it is there then the index is returned else
  insertion for the namespace is done and the corresponding index
  is returned
 */
static int
insert_namespace(const char *ns, int *return_idx)
{
    namespace_node_data_t *new_ns = NULL;
    char statname[1024];

    /*
     * ! search the namespace if available then return the index 
     */
    if (search_namespace(ns, return_idx) == 0) {
	return 0;
    }

    /*
     * ! before allocating namespace check for max limit 
     */
    if ((g_lstNamespace.namespace_counter) == NKN_MAX_NAMESPACES) {
	lc_log_basic(LOG_INFO, "namespace limit exceeded");
	return -1;
    }

    /*
     * ! allocate the new namespace data 
     */
    new_ns = nkn_calloc_type(1, sizeof (namespace_node_data_t), mod_mgmt_charbuf);	// get_next_empty_namespace_slot();
    if (new_ns == NULL) {
	lc_log_basic(LOG_INFO, "namespace node allocation failed");
	return -1;
    }
    /*
     * ! set the default values to the newly created namespace 
     */
    new_ns->namespace = nkn_strdup_type(ns, mod_mgmt_charbuf);
    new_ns->domain = nkn_strdup_type("*", mod_mgmt_charbuf);
    new_ns->domain_regex = NULL;
    new_ns->precedence = 0;
    /*
     * ! insert the newly created namespace based on the precedence value and 
     * domain value 
     */
    insert_list_by_preference(new_ns, return_idx);
    /*
     * ! add the backup-counter for namespace request count 
     */
    snprintf(statname, sizeof (statname), "glob_namespace_bkup_%s_gets", ns);
    statname[sizeof (statname) - 1] = '\0';
    nkn_mon_add(statname, NULL, &new_ns->tot_request_bkup_count,
	    sizeof (new_ns->tot_request_bkup_count));

    return 0;
}

/*! compare two namespace domain and precedence values
  if domain value matches then compare the precence value
  if changed namespace precedence < current namespace precedence
  then we need to insert and move the rest down
 */
static int
compare_domain(namespace_node_data_t * curr_ns,
	namespace_node_data_t * changed_ns)
{
    int err = 0, result = 0, changed_ns_result = 0;

    if (curr_ns->domain_regex && strlen(curr_ns->domain_regex) > 0) {
	/*
	 * ! if domain value is any then directly compare the precedence
	 * value 
	 */
	err = is_ns_domain_any(curr_ns, &result);
	if (result == 0 ) {
	    if (changed_ns->precedence < curr_ns->precedence)
		goto bail;
	} else if (curr_ns->domain_regex
		&& curr_ns->domain_comp_regex
		&& (0 == ns_match_regex_domain(curr_ns, changed_ns))) {

	    if (changed_ns->precedence < curr_ns->precedence)
		goto bail;
	}
    } else if (curr_ns->num_domain_fqdns > 0) {
	/*! if domain value is any then directly compare the
	  precedence value
	 */
	err = is_ns_domain_any(curr_ns, &changed_ns_result);
	err = is_ns_domain_any(changed_ns, &result);
	if ((changed_ns_result == 0)|| (result == 0)) {

	    if (changed_ns->precedence < curr_ns->precedence)
		goto bail;

	} else if (ns_check_overlap_fqdns(curr_ns,changed_ns) == 0) {
	//} else if (strcmp(curr_ns->domain,changed_ns->domain) == 0) {
	    /* ns_check_overlap_fqdns() == 0 means, there is overlapping */
	    if (changed_ns->precedence < curr_ns->precedence)
		goto bail;
	}
    }
    /*
     * Bad match !!
     */
    err = -1;
bail:
    return err;
}

/*! compare two namespace domain regex and precedence values
  if domain regex value matches then compare the precence value
  if changed namespace precedence < current namespace precedence
  then we need to insert and move the rest down
 */
static int
compare_domain_regex(namespace_node_data_t * curr_ns,
	namespace_node_data_t * changed_ns)
{
    int result = 0, err;
    if (curr_ns->domain_regex &&
	    strlen(curr_ns->domain_regex) > 0) {
	if (curr_ns->domain_regex &&
		changed_ns->domain_regex &&
		strcmp(changed_ns->domain_regex, curr_ns->domain_regex) == 0 ) {
		//nkn_regexec(curr_ns->domain_comp_regex,
		    //changed_ns->domain_regex, NULL, 0) == 0) {
	    if (changed_ns->precedence < curr_ns->precedence)
		return 0;
	} else if (curr_ns->domain_regex &&
		changed_ns->domain_regex &&
		nkn_regexec(changed_ns->domain_comp_regex,
		    curr_ns->domain_regex, NULL, 0) == 0) {
	    if (changed_ns->precedence < curr_ns->precedence)
		return 0;
	}
    } else if (curr_ns->num_domain_fqdns > 0) {
	/*! if domain value is any then directly compare the
	  precedence value
	 */
	/* if result = 0 => any is configured */
	err = is_ns_domain_any(curr_ns, &result);
	if (result == 0) {
	//if (strcmp(curr_ns->domain, "*") == 0) {
	    if (changed_ns->precedence < curr_ns->precedence)
		return 0;
	} else if (changed_ns->domain_comp_regex
		&& changed_ns->domain_regex
		&& (ns_match_regex_domain(changed_ns, curr_ns) == 0)) {
		//nkn_regexec(changed_ns->domain_comp_regex, curr_ns->domain, NULL, 0) == 0){
	    if (changed_ns->precedence < curr_ns->precedence)
		return 0;
	}
    }
    return -1;
}

static void
update_namespace_list_by_precedence (int n_index)
{
    int i, precedence = 0, old_index = n_index, new_index = -1;
    namespace_node_data_t *temp_ns = NULL, *temp2 = NULL;
    namespace_node_data_t *changed_namespace = get_namespace(n_index);

    if (!changed_namespace)
	return;

    precedence = changed_namespace->precedence;

    for ( i = 0; i < g_lstNamespace.namespace_counter; ++i) {
	temp_ns = get_namespace(i);
	if (temp_ns == NULL) {
	    continue;
	}
	if (i == n_index) {
	    /* skip comparision with self */
	    continue;
	}
	if (temp_ns->precedence < precedence) {
	    /* this is not our place */
	    continue;
	} else if (temp_ns->precedence == precedence) {
	    /* though two precedence can't be same,
	       still it is good check */
	    /* for now, do nothing */
	} else {
	    /* this is our place */
	    new_index = i;
	    break;
	}
    }
    if (new_index == -1) {
	/* we didn't find a new place, either it is correct place,
	 * or this should be moved to last */
	if ((i == g_lstNamespace.namespace_counter)
		&& (g_lstNamespace.namespace_counter > 1)) {
	    new_index = g_lstNamespace.namespace_counter;
	    for (i = old_index; i < new_index -1; i++) {
		temp_ns = get_namespace(i);
		temp2 = get_namespace(i+1);
		set_namespace(i, temp2);
		set_namespace(i+1, temp_ns);
	    }
	}
    } else if (new_index < old_index) {
	/* move item towards front of list */
	for (i = old_index; i > new_index; i--) {
	    /* exchange i and i-1 */
	    temp_ns = get_namespace(i);
	    temp2 = get_namespace(i-1);
	    set_namespace(i, temp2);
	    set_namespace(i-1, temp_ns);
	}
    } else if (new_index > old_index) {
	/* move item towards end of list */
	for (i = old_index; i < new_index -1; i++) {
	    temp_ns = get_namespace(i);
	    temp2 = get_namespace(i+1);
	    set_namespace(i, temp2);
	    set_namespace(i+1, temp_ns);
	}
    }

    return;
}

#define	MAX_BUFFER	8192
char *
get_namespace_precedence_list(void)
{
    int err = 0;
    uint32_t len;
    char *lst_ns = NULL;
    uint32_t ns_len = 0;
    namespace_node_data_t *temp = NULL;

    (void) err;

    /*
     * Allocate the space 
     */
    lst_ns = nkn_calloc_type(MAX_BUFFER, sizeof (char), mod_mgmt_charbuf);
    if (!lst_ns)
	goto bail;
    len = MAX_BUFFER;
    nvsd_mgmt_namespace_lock();

    /*
     * Iterate thru the Namespace list 
     */
    int i;

    for (i = 0; i < g_lstNamespace.namespace_counter; ++i) {
	temp = get_namespace(i);
	if (temp && temp->active) {

	    ns_len = strlen(temp->namespace);
	    if (len < (ns_len + 1)) {
		nvsd_mgmt_namespace_unlock();
		goto bail;
	    } else
		len -= (ns_len + 1);
	    strncat(lst_ns, temp->namespace, ns_len);
	    strncat(lst_ns, "\n", 1);
	}
    }

    nvsd_mgmt_namespace_unlock();
bail:
    return (lst_ns);
}	/* end of get_namespace_precedence_list() */

int
nvsd_mgmt_remove_pe_srvr(namespace_node_data_t * pNamespace,
	const char *pe_name)
{
    policy_srvr_obj_t *pst_pe_srvr = NULL;
    policy_srvr_obj_t *pst_pe_srvr_prev = NULL;
    int i = 0;

    pst_pe_srvr = pNamespace->policy_engine.policy_srvr_obj;

    while (pst_pe_srvr != NULL) {

	if (pst_pe_srvr->policy_srvr &&
		!strcmp(pst_pe_srvr->policy_srvr->name, pe_name)) {
	    if (i == 0) {
		policy_srvr_obj_t *pst_pe_srvr_tmp = NULL;

		pst_pe_srvr_tmp = pst_pe_srvr;
		pNamespace->policy_engine.policy_srvr_obj =
		    pst_pe_srvr_tmp->nxt;
		free(pst_pe_srvr_tmp);
		return 0;
	    } else if (i < 8) {
		pst_pe_srvr_prev->nxt = pst_pe_srvr->nxt;
		free(pst_pe_srvr);
		return 0;
	    }

	}

	i++;
	pst_pe_srvr_prev = pst_pe_srvr;
	pst_pe_srvr = pst_pe_srvr->nxt;
    }

    return -1;

}

int
get_policy_srvr(const char *pe_name, policy_srvr_obj_t ** policy_obj)
{
    policy_srvr_obj_t *pst_pe_srvr = *policy_obj;
    policy_srvr_obj_t *pst_pe_srvr_prev = NULL;

    int i = 0;

    while (pst_pe_srvr != NULL) {

	if (pst_pe_srvr->policy_srvr && pst_pe_srvr->policy_srvr->name &&
		!strcmp(pst_pe_srvr->policy_srvr->name, pe_name)) {
	    *policy_obj = pst_pe_srvr;
	    return 0;
	}

	i++;
	pst_pe_srvr_prev = pst_pe_srvr;
	pst_pe_srvr = pst_pe_srvr->nxt;
    }

    if (i < 8) {
	policy_srvr_obj_t *new_pe_obj = NULL;

	new_pe_obj =
	    nkn_malloc_type(sizeof (policy_srvr_obj_t), mod_mgmt_charbuf);
	memset(new_pe_obj, 0, sizeof (policy_srvr_obj_t));
	if (pst_pe_srvr_prev)
	    pst_pe_srvr_prev->nxt = new_pe_obj;
	else if (new_pe_obj)
	    pst_pe_srvr = new_pe_obj;
	*policy_obj = new_pe_obj;
	return 0;

    }
    return -1;
}

static int
nvsd_pe_srvr_delete_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value, void *data)
{
    int err = 0;
    int ns_idx = 0;
    const tstring *name = NULL;
    const char *t_namespace = NULL;
    const char *pe_srvr_name = NULL;
    tbool *rechecked_licenses_p = data;
    tstr_array *name_parts = NULL;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(bndgs_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /*
     * Check if this is our node 
     */
    if (bn_binding_name_pattern_match
	    (ts_str(name), "/nkn/nvsd/namespace/*/policy/server/**")) {
	/*
	 * Get the namespace 
	 */
	err = bn_binding_get_name_parts(binding, &name_parts);
	bail_error(err);
	t_namespace = tstr_array_get_str_quick(name_parts, 3);

	pe_srvr_name = tstr_array_get_str_quick(name_parts, 6);

	lc_log_basic(LOG_DEBUG, "Read .../nkn/nvsd/namespace/ as : \"%s\"",
		t_namespace);

	if (search_namespace(t_namespace, &ns_idx) != 0)
	    goto bail;
	pstNamespace = get_namespace(ns_idx);

	if (!pstNamespace)
	    goto bail;
    } else {
	/*
	 * This is not the node we are interested in.. hence bail 
	 */
	goto bail;
    }

    if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 3, 4, "*", "policy", "server", "*")) {
	tstring *t_name = NULL;

	err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &t_name);
	bail_error(err);

	err = nvsd_mgmt_remove_pe_srvr(pstNamespace, pe_srvr_name);

	if (err)
	    lc_log_basic(LOG_NOTICE, "Delete entry not found in server lst");
    }
bail:
    tstr_array_free(&name_parts);
    return err;
}

static int
nvsd_pe_srvr_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value, void *data)
{
    int err = 0;
    int ns_idx = 0;
    const tstring *name = NULL;
    const char *t_namespace = NULL;
    const char *pe_srvr_name = NULL;
    tbool *rechecked_licenses_p = data;
    policy_srvr_obj_t *policy_srvr_obj = NULL;
    tstr_array *name_parts = NULL;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(bndgs_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /*
     * Check if this is our node 
     */
    if (bn_binding_name_pattern_match
	    (ts_str(name), "/nkn/nvsd/namespace/*/policy/server/**")) {
	/*
	 * Get the namespace 
	 */
	err = bn_binding_get_name_parts(binding, &name_parts);
	bail_error(err);
	t_namespace = tstr_array_get_str_quick(name_parts, 3);

	pe_srvr_name = tstr_array_get_str_quick(name_parts, 6);

	lc_log_basic(LOG_DEBUG, "Read .../nkn/nvsd/namespace/ as : \"%s\"",
		t_namespace);

	if (search_namespace(t_namespace, &ns_idx) != 0)
	    goto bail;
	pstNamespace = get_namespace(ns_idx);

	if (!pstNamespace)
	    goto bail;

	policy_srvr_obj = pstNamespace->policy_engine.policy_srvr_obj;

	err = get_policy_srvr(pe_srvr_name, &policy_srvr_obj);

	/*
	 * for the first time create a list 
	 */
	if (pstNamespace->policy_engine.policy_srvr_obj == NULL)
	    pstNamespace->policy_engine.policy_srvr_obj = policy_srvr_obj;

	if (err)
	    goto bail;
    } else {
	/*
	 * This is not the node we are interested in.. hence bail 
	 */
	goto bail;
    }

    /*
     * Check if it is the x-forwarded-for node 
     */
    if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 3, 5, "*", "policy", "server", "*", "failover")) {

	uint16 t_failover = 0;

	err = bn_binding_get_uint16(binding, ba_value, NULL, &t_failover);
	bail_error(err);

	policy_srvr_obj->fail_over = t_failover;
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 3, 5, "*", "policy", "server", "*", "precedence")) {

	uint16 t_precedence = 0;

	err = bn_binding_get_uint16(binding, ba_value, NULL, &t_precedence);

	bail_error(err);

	policy_srvr_obj->precedence = t_precedence;

	// TODO
	/*
	 * When precednece is set sort the list based on precedence 
	 */
	/*
	 * will do it ,if backend needs it Backend has confirmed,Backedn will 
	 * sort the list when it gets it 
	 */
	// sort_pe_srvr_lst(pstNamespace->policy_engine.policy_srvr_obj);
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 3, 6, "*", "policy", "server", "*", "callout",
	     "client_req")) {

	tbool t_enabled;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_enabled);
	bail_error(err);

	/*
	 * store the flag in the global structure 
	 */
	policy_srvr_obj->callouts[CLIENT_REQ] = t_enabled;
    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 3, 6, "*", "policy", "server", "*", "callout",
	     "client_resp")) {
	tbool t_enabled;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_enabled);
	bail_error(err);

	/*
	 * store the flag in the global structure 
	 */
	policy_srvr_obj->callouts[CLIENT_RSP] = t_enabled;

    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 3, 6, "*", "policy", "server", "*", "callout",
	     "origin_req")) {
	tbool t_enabled;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_enabled);
	bail_error(err);

	/*
	 * store the flag in the global structure 
	 */
	policy_srvr_obj->callouts[ORIGIN_REQ] = t_enabled;

    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 3, 6, "*", "policy", "server", "*", "callout",
	     "origin_resp")) {

	tbool t_enabled;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_enabled);
	bail_error(err);

	/*
	 * store the flag in the global structure 
	 */
	policy_srvr_obj->callouts[ORIGIN_RSP] = t_enabled;
    } else if (bn_binding_name_parts_pattern_match_va (name_parts, 3, 4,
		"*", "policy", "server", "*")) {
	tstring *t_name = NULL;
	policy_srvr_t *policy_srvr = NULL;

	err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &t_name);
	bail_error(err);

	policy_srvr = nvsd_mgmt_get_pe_srvr(ts_str_maybe(t_name));
	if (policy_srvr) {
	    policy_srvr_obj->policy_srvr = policy_srvr;
	}
    }

bail:
    return err;
}

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_origin_request_header_handle_change()
 *	purpose : handler for config changes for origin-request header
 */
static int
nvsd_origin_request_header_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array *
	bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value,
	void *data)
{
    int err = 0;
    int ns_idx = 0;
    const tstring *name = NULL;
    const char *t_temp_str = NULL;
    const char *t_namespace = NULL;
    tbool *rechecked_licenses_p = data;
    uint32 t_index = 0;
    tstr_array *name_parts = NULL;
    origin_request_header_add_t *pst_add_header_map = NULL;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(bndgs_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /*
     * Check if this is our node 
     */
    if (bn_binding_name_pattern_match
	    (ts_str(name), "/nkn/nvsd/namespace/*/origin-request/**")) {
	/*
	 * Get the namespace 
	 */
	err = bn_binding_get_name_parts(binding, &name_parts);
	bail_error(err);
	t_namespace = tstr_array_get_str_quick(name_parts, 3);

	lc_log_basic(jnpr_log_level, "Read .../nkn/nvsd/namespace/ as : \"%s\"",
		t_namespace);

	if (search_namespace(t_namespace, &ns_idx) != 0)
	    goto bail;
	pstNamespace = get_namespace(ns_idx);
    } else {
	/*
	 * This is not the node we are interested in.. hence bail 
	 */
	goto bail;
    }

    /*
     * Check if it is the x-forwarded-for node 
     */
    if (bn_binding_name_parts_pattern_match_va (name_parts, 3, 4, "*",
		"origin-request", "x-forwarded-for", "enable")) {
	tbool t_enabled;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_enabled);
	bail_error(err);

	/*
	 * store the flag in the global structure 
	 */
	pstNamespace->origin_request.x_forward_header = t_enabled;

	/*
	 * Do not fall thru but exit the function as we are done 
	 */
	goto bail;
    }

    /*
     * Check if it is the include-orig-ip-port node 
     */
    else if (bn_binding_name_pattern_match(ts_str(name),
		"/nkn/nvsd/namespace/*/origin-request/include-orig-ip-port")) {
	tbool t_enabled;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_enabled);
	bail_error(err);

	/*
	 * store the flag in the global structure 
	 */
	pstNamespace->origin_request.include_orig_ip_port = t_enabled;
	/*
	 * Do not fall thru but exit the function as we are done 
	 */
	goto bail;
    } else if (bn_binding_name_pattern_match (ts_str(name),
	     "/nkn/nvsd/namespace/*/origin-request/connect/timeout")) {
	uint32 con_timeout;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &con_timeout);
	bail_error(err);

	pstNamespace->origin_request.orig_conn_values.conn_timeout =
	    con_timeout;
    } else if (bn_binding_name_parts_pattern_match_va (name_parts, 3, 4, "*",
		"origin-request", "connect", "retry-delay")) {
	uint32 con_retry_delay;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &con_retry_delay);
	bail_error(err);

	pstNamespace->origin_request.orig_conn_values.conn_retry_delay =
	    con_retry_delay;
    } else if (bn_binding_name_parts_pattern_match_va (name_parts, 3, 4,
		"*", "origin-request", "read", "interval-timeout")) {
	uint32 rd_int_timeout;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &rd_int_timeout);
	bail_error(err);

	pstNamespace->origin_request.orig_conn_values.read_timeout =
	    rd_int_timeout;
    } else if (bn_binding_name_parts_pattern_match_va (name_parts, 3, 4,
		"*", "origin-request", "read", "retry-delay")) {
	uint32 rd_retry_delay;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &rd_retry_delay);
	bail_error(err);

	pstNamespace->origin_request.orig_conn_values.read_retry_delay =
	    rd_retry_delay;
    }
    /*
     * Else check if it is NOT the header add node 
     */
    else if (!bn_binding_name_pattern_match(ts_str(name),
		"/nkn/nvsd/namespace/*/origin-request/header/add/**")) {
	/*
	 * SHOULD NOT HAPPEN 
	 */
	goto bail;
    }

    /*
     * It should be the header add node hence get the name and value 
     */
    /*
     * Get the index first 
     */
    t_temp_str = tstr_array_get_str_quick(name_parts, 7);
    if (!t_temp_str)
	goto bail;				// sanity check

    t_index = atoi(t_temp_str);

    /*
     * Since node index starts from 1, decrement by 1 for array index 
     */
    t_index--;
    if (MAX_ORIGIN_REQUEST_HEADERS <= t_index)
	goto bail;

    /*
     * Now get a pointer to the header_add_list entry 
     */
    pst_add_header_map = &(pstNamespace->origin_request.add_headers[t_index]);

    /*
     * Get the value of HEADERS/ADD/[] 
     */
    if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 6,
		"*", "origin-request", "header", "add", "*", "name")) {
	tstring *t_header_name = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_header_name);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../nkn/nvsd/namespace/%s/origin-request/header/add/%d/name as : \"%s\"",
		t_namespace, (t_index + 1),
		(ts_str(t_header_name)) ? ts_str(t_header_name) : "");

	/*
	 * Check if value already exists, if so free it 
	 */
	if (pst_add_header_map->name) {
	    safe_free(pst_add_header_map->name);
	    pst_add_header_map->name = NULL;
	}

	/*
	 * In case of delete, the value comas as a NULL, hence check for NULL 
	 */
	if (t_header_name) {
	    pst_add_header_map->name =
		nkn_strdup_type(ts_str(t_header_name), mod_mgmt_charbuf);
	}

	ts_free(&t_header_name);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 6,
		"*", "origin-request", "header", "add", "*", "value")) {
	tstring *t_value = NULL;

	err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &t_value);
	bail_error(err);

	lc_log_basic(jnpr_log_level, "Read .../nkn/nvsd/namespace/%s/"
		"origin-request/header/add/%d/value as : \"%s\"",
		t_namespace, (t_index + 1),
		(t_value) ? ts_str(t_value) : "");

	if (pst_add_header_map->value) {
	    safe_free(pst_add_header_map->value);
	    pst_add_header_map->value = NULL;
	}

	if (t_value) {
	    pst_add_header_map->value =
		nkn_strdup_type(ts_str(t_value), mod_mgmt_charbuf);
	}

	ts_free(&t_value);
    }

bail:
    tstr_array_free(&name_parts);
    return err;
}	/* end of * nvsd_origin_request_header_handle_change() */

/* ------------------------------------------------------------------------- */
/*
 *	funtion : nvsd_client_response_header_handle_change()
 *	purpose : handler for config changes for client-response header
 */

static int
nvsd_client_response_header_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value,
	void *data)
{
    int err = 0;
    int ns_idx = 0;
    const tstring *name = NULL;
    const char *t_temp_str = NULL;
    const char *t_namespace = NULL;
    tbool *rechecked_licenses_p = data;
    uint32 t_index = 0;
    tstr_array *name_parts = NULL;
    client_response_header_add_t *pst_add_header_map = NULL;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(bndgs_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);
    bail_null(name);

    if (!bn_binding_name_pattern_match (ts_str(name),
		"/nkn/nvsd/namespace/*/client-response/**")) {
	// No need to handle here
	goto bail;
    }

    /*
     * Get the namespace
     */
    err = bn_binding_get_name_parts(binding, &name_parts);
    bail_error(err);
    t_namespace = tstr_array_get_str_quick(name_parts, 3);
    bail_null(t_namespace);

    lc_log_basic(jnpr_log_level, "Read .../nkn/nvsd/namespace/ as : \"%s\"",
	    t_namespace);

    if (search_namespace(t_namespace, &ns_idx) != 0)
	goto bail;
    pstNamespace = get_namespace(ns_idx);

    /*
     * Check if this is our node
     */
    if (bn_binding_name_pattern_match (ts_str(name),
		"/nkn/nvsd/namespace/*/client-response/header/add/**")) {
	// Being handled below
	lc_log_basic(jnpr_log_level, "Read .../nkn/nvsd/namespace/ as : \"%s\"",
		t_namespace);

    } else if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 3, 3, "*", "client-response", "checksum")) {
	uint32 checksum;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &checksum);
	bail_error(err);

	pstNamespace->md5_checksum_len = checksum;

	goto bail;
    } else if (bn_binding_name_parts_pattern_match_va (name_parts, 3, 3,
		"*", "client-response", "dscp")) {
	int32 dscp;

	err = bn_binding_get_int32(binding, ba_value, NULL, &dscp);
	bail_error(err);

	pstNamespace->dscp = dscp;

	goto bail;
    } else {
	/*
	 * This is not the node we are interested in.. hence bail
	 */
	goto bail;
    }

    /*
     * Get the index first 
     */
    t_temp_str = tstr_array_get_str_quick(name_parts, 7);
    if (!t_temp_str)
	goto bail;				// sanity check

    t_index = atoi(t_temp_str);

    /*
     * Since node index starts from 1, decrement by 1 for array index 
     */
    t_index--;
    if (MAX_CLIENT_RESPONSE_HEADERS <= t_index)
	goto bail;

    /*
     * Now get a pointer to the header_add_list entry 
     */
    pst_add_header_map = &(pstNamespace->client_response.add_headers[t_index]);

    /*
     * Get the value of HEADERS/ADD/[] 
     */
    if (bn_binding_name_parts_pattern_match_va (name_parts, 3, 6, "*",
		"client-response", "header", "add", "*", "name")) {
	tstring *t_header_name = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_header_name);
	bail_error(err);

	lc_log_basic(jnpr_log_level,
		"Read .../nkn/nvsd/namespace/%s/client-response/header/add/%d/name as : \"%s\"",
		t_namespace, (t_index + 1),
		(t_header_name) ? ts_str(t_header_name) : "");

	/*
	 * Check if value already exists, if so free it 
	 */
	if (pst_add_header_map->name) {
	    safe_free(pst_add_header_map->name);
	    pst_add_header_map->name = NULL;
	}

	/*
	 * In case of delete, the value comas as a NULL, hence check for NULL 
	 */
	if (t_header_name) {
	    pst_add_header_map->name =
		nkn_strdup_type(ts_str(t_header_name), mod_mgmt_charbuf);
	}

	ts_free(&t_header_name);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 6,
		"*", "client-response", "header", "add", "*", "value")) {
	tstring *t_value = NULL;

	err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &t_value);
	bail_error(err);

	lc_log_basic(jnpr_log_level, "Read .../nkn/nvsd/namespace/%s/"
		"client-response/header/add/%d/valueas : \"%s\"",
		t_namespace, (t_index + 1),
		(t_value) ? ts_str(t_value) : "");

	if (pst_add_header_map->value) {
	    safe_free(pst_add_header_map->value);
	    pst_add_header_map->value = NULL;
	}

	if (t_value) {
	    pst_add_header_map->value =
		nkn_strdup_type(ts_str(t_value), mod_mgmt_charbuf);
	}
	ts_free(&t_value);
    }

bail:
    tstr_array_free(&name_parts);
    return err;
}	/* end of * nvsd_client_response_header_handle_change() */

/* ------------------------------------------------------------------------- */
/*
 *	funtion : nvsd_client_response_header_delete_handle_change()
 *	purpose : handler for config changes for client-response header
 */

static int
nvsd_client_response_header_delete_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value,
	void *data)
{
    int err = 0;
    int ns_idx = 0;
    const tstring *name = NULL;
    const char *t_temp_str = NULL;
    const char *t_namespace = NULL;
    tbool *rechecked_licenses_p = data;
    uint32 t_index = 0;
    tstr_array *name_parts = NULL;
    client_response_header_delete_t *pst_add_header_map = NULL;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(bndgs_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /*
     * Check if this is our node 
     */
    if (bn_binding_name_pattern_match (ts_str(name),
	     "/nkn/nvsd/namespace/*/client-response/header/delete/**")) {
	/*
	 * Get the namespace 
	 */
	err = bn_binding_get_name_parts(binding, &name_parts);
	bail_error(err);
	t_namespace = tstr_array_get_str_quick(name_parts, 3);

	lc_log_basic(jnpr_log_level, "Read .../nkn/nvsd/namespace/ as : \"%s\"",
		t_namespace);

	if (search_namespace(t_namespace, &ns_idx) != 0)
	    goto bail;
	pstNamespace = get_namespace(ns_idx);
    } else {
	/*
	 * This is not the node we are interested in.. hence bail 
	 */
	goto bail;
    }

    /*
     * Get the index first 
     */
    t_temp_str = tstr_array_get_str_quick(name_parts, 7);
    if (!t_temp_str)
	goto bail;				// sanity check

    t_index = atoi(t_temp_str);

    /*
     * Since node index starts from 1, decrement by 1 for array index 
     */
    t_index--;
    if (MAX_CLIENT_RESPONSE_HEADERS <= t_index)
	goto bail;

    /*
     * Now get a pointer to the header_add_list entry 
     */
    pst_add_header_map =
	&(pstNamespace->client_response.delete_headers[t_index]);

    /*
     * Get the value of HEADERS/ADD/[] 
     */
    if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 6, "*",
		"client-response", "header", "delete", "*", "name")) {
	tstring *t_header_name = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_header_name);
	bail_error(err);

	lc_log_basic(jnpr_log_level, "Read .../nkn/nvsd/namespace/%s/"
		"client-response/header/delete/%d/name as : \"%s\"",
		t_namespace, (t_index + 1),
		(ts_str(t_header_name)) ? ts_str(t_header_name) : "");

	/*
	 * Check if value already exists, if so free it 
	 */
	if (pst_add_header_map->name) {
	    safe_free(pst_add_header_map->name);
	    pst_add_header_map->name = NULL;
	}

	/*
	 * In case of delete, the value comas as a NULL, hence check for NULL 
	 */
	if (t_header_name) {
	    pst_add_header_map->name =
		nkn_strdup_type(ts_str(t_header_name), mod_mgmt_charbuf);
	}
	ts_free(&t_header_name);
    }

bail:
    tstr_array_free(&name_parts);
    return err;
}	/* end of * nvsd_client_response_header_delete_handle_change() */

static pub_point_data_t *
getPubPoint(namespace_node_data_t * pNamespace, const char *pub_point_name)
{
    int i = 0;

    for (i = 0; i < MAX_PUB_POINT; i++) {
	if (pNamespace->rtsp_live_pub_point[i].name
		&& (strcmp(pub_point_name,
			pNamespace->rtsp_live_pub_point[i].name) == 0)) {
	    return &pNamespace->rtsp_live_pub_point[i];
	}
    }

    return NULL;
}

/* ------------------------------------------------------------------------- */

static int
nvsd_ns_http_server_map_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value, void *data)
{
    int err = 0;
    int ns_idx = 0;
    int t_smap_count = 0;
    tbool *rechecked_licenses_p = data;
    const tstring *name = NULL;
    char *serv_map = NULL;
    const char *t_namespace = NULL;
    tstr_array *name_parts = NULL;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(bndgs_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /*
     * Check if it is the server-map map-name node
     */
    if (bn_binding_name_pattern_match(ts_str(name),
		"/nkn/nvsd/namespace/*/origin-server/http/"
		"server-map/*/map-name"))
    {
	/*
	 * Get the namespace 
	 */
	err = bn_binding_get_name_parts(binding, &name_parts);
	bail_error(err);
	t_namespace = tstr_array_get_str_quick(name_parts, 3);

	if (search_namespace(t_namespace, &ns_idx) != 0)
	    goto bail;
	pstNamespace = get_namespace(ns_idx);
    } else {
	/*
	 * This is not the node we are interested in.. hence bail 
	 */
	goto bail;
    }

    err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &serv_map);
    bail_error(err);

    if (NULL == serv_map)
	goto bail;

    t_smap_count = pstNamespace->origin_server.http_smap_count;

    if (t_smap_count < MAX_HTTP_SERVER_MAPS) {
	pstNamespace->origin_server.http_server_map[t_smap_count] =
	    nvsd_mgmt_get_server_map(serv_map);
	if (NULL == pstNamespace->origin_server.http_server_map[t_smap_count]) {
	    lc_log_basic(LOG_INFO, "Server-map structure not found for %s",
		    serv_map);
	    goto bail;
	}
	pstNamespace->origin_server.http_server_map_name[t_smap_count] =
	    nkn_strdup_type(serv_map, mod_mgmt_charbuf);
	pstNamespace->origin_server.http_smap_count = t_smap_count + 1;
    }

bail:
    safe_free(serv_map);
    tstr_array_free(&name_parts);
    return err;
}

static int
nvsd_ns_http_server_map_handle_delete_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value,
	void *data)
{
    int err = 0;
    int t_smap_count = 0;
    tbool *rechecked_licenses_p = data;
    const tstring *name = NULL;
    char *serv_map = NULL;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(bndgs_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    if (!bn_binding_name_pattern_match (ts_str(name),
	     "/nkn/nvsd/namespace/*/origin-server/http/server-map/*/map-name"))
	goto bail;

    err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &serv_map);
    bail_error(err);

    if (NULL == serv_map)
	goto bail;

    t_smap_count = pstNamespace->origin_server.http_smap_count;

    if (0 == t_smap_count)
	goto bail;

    for (int i = 0; i < t_smap_count; i++) {
	if (NULL == (pstNamespace->origin_server.http_server_map_name[i]))
	    goto bail;

	if (!strcmp
		(pstNamespace->origin_server.http_server_map_name[i], serv_map)) {
	    if ((i + 1) != t_smap_count) {
		for (int j = i; j < (t_smap_count - 1); j++) {
		    if ((NULL ==
				(pstNamespace->origin_server.http_server_map_name[j]))
			    || (NULL ==
				(pstNamespace->origin_server.
				 http_server_map_name[j + 1]))
			    || (NULL ==
				(pstNamespace->origin_server.http_server_map[j]))
			    || (NULL ==
				(pstNamespace->origin_server.
				 http_server_map[j + 1])))
			goto bail;

		    pstNamespace->origin_server.http_server_map_name[j] =
			pstNamespace->origin_server.http_server_map_name[j + 1];
		    pstNamespace->origin_server.http_server_map[j] =
			pstNamespace->origin_server.http_server_map[j + 1];

		}

		pstNamespace->origin_server.http_server_map_name[t_smap_count -
		    1] = NULL;
		pstNamespace->origin_server.http_server_map[t_smap_count - 1] =
		    NULL;
		pstNamespace->origin_server.http_smap_count = t_smap_count - 1;

		goto bail;
	    } else {
		pstNamespace->origin_server.http_server_map_name[i] = NULL;
		pstNamespace->origin_server.http_server_map[i] = NULL;
		pstNamespace->origin_server.http_smap_count = t_smap_count - 1;

		goto bail;
	    }
	}
    }

bail:
    safe_free(serv_map);
    return err;
}

/* ------------------------------------------------------------------------- */
static int
nvsd_ns_cluster_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value, void *data)
{
    int err = 0;
    int ns_idx = 0;
    int t_cluster_count = 0;
    tbool *rechecked_licenses_p = data;
    const tstring *name = NULL;
    char *cluster = NULL;
    const char *t_namespace = NULL;
    tstr_array *name_parts = NULL;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(bndgs_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /*
     * Check if it is the cluster node 
     */
    if (bn_binding_name_pattern_match(ts_str(name),
		"/nkn/nvsd/namespace/*/cluster/*/cl-name")) {
	/*
	 * Get the namespace
	 */
	err = bn_binding_get_name_parts(binding, &name_parts);
	bail_error(err);
	t_namespace = tstr_array_get_str_quick(name_parts, 3);

	if (search_namespace(t_namespace, &ns_idx) != 0)
	    goto bail;
	pstNamespace = get_namespace(ns_idx);
    } else {
	/*
	 * This is not the node we are interested in.. hence bail 
	 */
	goto bail;
    }

    err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &cluster);
    bail_error(err);

    if (NULL == cluster)
	goto bail;

    t_cluster_count = pstNamespace->num_clusters;

    if (t_cluster_count < MAX_CLUSTERS) {
	pstNamespace->clusterMap[t_cluster_count] =
	    get_cluster_element(cluster);
	if (NULL == pstNamespace->clusterMap[t_cluster_count]) {
	    lc_log_basic(LOG_INFO, "Cluster structure not found for %s",
		    cluster);
	    goto bail;
	}
	pstNamespace->cluster_map_name[t_cluster_count] =
	    nkn_strdup_type(cluster, mod_mgmt_charbuf);
	pstNamespace->num_clusters = t_cluster_count + 1;
    }

bail:
    safe_free(cluster);
    return err;
}

static int
nvsd_ns_cluster_handle_delete_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value, void *data)
{
    int err = 0;
    int t_cluster_count = 0;
    tbool *rechecked_licenses_p = data;
    const tstring *name = NULL;
    char *cluster = NULL;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(bndgs_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    if (!bn_binding_name_pattern_match (ts_str(name),
		"/nkn/nvsd/namespace/*/cluster/*/cl-name"))
	goto bail;

    err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &cluster);
    bail_error(err);

    if (NULL == cluster)
	goto bail;

    t_cluster_count = pstNamespace->num_clusters;

    if (0 == t_cluster_count)
	goto bail;

    for (int i = 0; i < t_cluster_count; i++) {
	if (NULL == (pstNamespace->cluster_map_name[i]))
	    goto bail;

	if (!strcmp(pstNamespace->cluster_map_name[i], cluster)) {
	    if ((i + 1) != t_cluster_count) {
		for (int j = i; j < (t_cluster_count - 1); j++) {
		    if ((NULL == (pstNamespace->cluster_map_name[j])) ||
			    (NULL == (pstNamespace->cluster_map_name[j + 1])) ||
			    (NULL == (pstNamespace->clusterMap[j])) ||
			    (NULL == (pstNamespace->clusterMap[j + 1])))
			goto bail;

		    pstNamespace->cluster_map_name[j] =
			pstNamespace->cluster_map_name[j + 1];
		    pstNamespace->clusterMap[j] =
			pstNamespace->clusterMap[j + 1];
		}

		pstNamespace->cluster_map_name[t_cluster_count - 1] = NULL;
		pstNamespace->clusterMap[t_cluster_count - 1] = NULL;
		pstNamespace->num_clusters = t_cluster_count - 1;

		goto bail;
	    } else {
		pstNamespace->cluster_map_name[i] = NULL;
		pstNamespace->clusterMap[i] = NULL;
		pstNamespace->num_clusters = t_cluster_count - 1;

		goto bail;
	    }
	}
    }

bail:
    safe_free(cluster);
    return err;
}

int
nvsd_ns_reset_counter_values(void)
{
    int err = 0;
    int i;
    uint32_t size = 0;
    char statname[1024];
    namespace_node_data_t *curr_ns = NULL;

    for (i = 0; i < g_lstNamespace.namespace_counter; ++i) {
	uint64_t counter_value = 0;

	curr_ns = get_namespace(i);
	if (curr_ns) {
	    snprintf(statname, sizeof (statname), "glob_namespace_%s_gets",
		    curr_ns->namespace);
	    statname[sizeof (statname) - 1] = '\0';
	    nkn_mon_get(statname, NULL, &counter_value, &size);
	    curr_ns->tot_request_bkup_count = counter_value;
	    lc_log_basic(LOG_NOTICE, "Namespace:%s Counter:%u\n",
		    curr_ns->namespace,
		    (unsigned int) curr_ns->tot_request_bkup_count);
	}
    }

    return err;
}

/* ------------------------------------------------------------------------- */

static int
nvsd_ns_cache_index_header_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value,
	void *data)
{
    int err = 0;
    int ns_idx = 0;
    int t_header_count = 0;
    tbool *rechecked_licenses_p = data;
    const tstring *name = NULL;
    char *header_name = NULL;
    const char *t_namespace = NULL;
    tstr_array *name_parts = NULL;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(bndgs_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    lc_log_basic(LOG_DEBUG, "Node is : %s", ts_str(name));

    /*
     * Check if it is the header include header-name node 
     */
    if (bn_binding_name_pattern_match(ts_str(name),
		"/nkn/nvsd/origin_fetch/config/*/cache-index/"
		"include-headers/*/header-name")) {
	/*
	 * Get the namespace 
	 */
	err = bn_binding_get_name_parts(binding, &name_parts);
	bail_error(err);
	t_namespace = tstr_array_get_str_quick(name_parts, 4);

	if (search_namespace(t_namespace, &ns_idx) != 0)
	    goto bail;
	pstNamespace = get_namespace(ns_idx);
    } else {
	/*
	 * This is not the node we are interested in.. hence bail 
	 */
	goto bail;
    }

    err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &header_name);
    bail_error(err);

    if (NULL == header_name)
	goto bail;

    t_header_count = pstNamespace->http_cache_index.headers_count;

    if (t_header_count < MAX_HEADER_NAMES) {
	pstNamespace->http_cache_index.include_header_names[t_header_count] =
	    nkn_strdup_type(header_name, mod_mgmt_charbuf);
	pstNamespace->http_cache_index.headers_count = t_header_count + 1;
    }

bail:
    safe_free(header_name);
    tstr_array_free(&name_parts);
    return err;
}

static int
nvsd_ns_cache_index_header_handle_delete_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value,
	void *data)
{
    int err = 0;
    int t_header_count = 0;
    tbool *rechecked_licenses_p = data;
    const tstring *name = NULL;
    char *header_name = NULL;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(bndgs_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    lc_log_basic(LOG_DEBUG, "Node is : %s", ts_str(name));

    if (!bn_binding_name_pattern_match(ts_str(name),
		"/nkn/nvsd/origin_fetch/config/*/cache-index"
		"/include-headers/*/header-name"))
	goto bail;

    err = bn_binding_get_str(binding, ba_value, bt_string, NULL, &header_name);
    bail_error(err);

    if (NULL == header_name)
	goto bail;

    t_header_count = pstNamespace->http_cache_index.headers_count;

    if (0 == t_header_count)
	goto bail;

    for (int position = 0; position < t_header_count; position++) {
	if (NULL ==
		(pstNamespace->http_cache_index.include_header_names[position]))
	    goto bail;

	if (!strcmp
		(pstNamespace->http_cache_index.include_header_names[position],
		 header_name)) {
	    if ((position + 1) != t_header_count) {
		for (int j = position; j < (t_header_count - 1); j++) {
		    if ((NULL ==
				(pstNamespace->http_cache_index.
				 include_header_names[j]))
			    || (NULL ==
				(pstNamespace->http_cache_index.
				 include_header_names[j + 1])))
			goto bail;

		    pstNamespace->http_cache_index.include_header_names[j] =
			pstNamespace->http_cache_index.include_header_names[j +
			1];
		}

		pstNamespace->http_cache_index.
		    include_header_names[t_header_count - 1] = NULL;
		pstNamespace->http_cache_index.headers_count =
		    t_header_count - 1;

		goto bail;
	    } else {
		pstNamespace->http_cache_index.include_header_names[position] =
		    NULL;
		pstNamespace->http_cache_index.headers_count =
		    t_header_count - 1;

		goto bail;
	    }
	}
    }

bail:
    safe_free(header_name);
    return err;
}

int
update_fqdn_list(namespace_node_data_t *ns_node_data, const char *fqdn,
	int operation)
{
    int err = 0, fqdn_index = -1;

    bail_require(ns_node_data);
    bail_require(fqdn);

    /* find fqdn index, (index == -1) => not found */
    err = find_fqdn_index(ns_node_data, fqdn, &fqdn_index);
    bail_error(err);

    /* check opertion */
    if (operation == eFQDN_delete) {
	/* free memory */
	safe_free(ns_node_data->domain_fqdns[fqdn_index]);
	ns_node_data->num_domain_fqdns--;
    } else if (operation  == eFQDN_add) {
	/* if already there, may be we re-conected with mgmtd */
	if (fqdn_index >= 0) {
	    /* domain is already in the list, don't update domain_fqdns */
	    /* DO NOTHING */
	} else {
	    /* domain is new to the list */
	    fqdn_index = -1;
	    err = ns_fqdn_find_empty_slot(ns_node_data, &fqdn_index );
	    bail_error(err);
	    ns_node_data->domain_fqdns[fqdn_index] =
		nkn_strdup_type(fqdn, mod_mgmt_charbuf);
	    ns_node_data->num_domain_fqdns++;
	}
    } else {
	/* log error */
    }

bail:
    return err;
}

int
find_fqdn_index(const namespace_node_data_t *ns_node_data, const char *fqdn,
	int *fqdn_index)
{
    int fqdn_location = 0, err = 0;

    bail_require(ns_node_data);
    bail_require(fqdn);
    bail_require(fqdn_index);

    /* zero is valid index */
    *fqdn_index = -1;

    for (fqdn_location = 0;
	    /* there are no holes in array */
	    fqdn_location < ns_node_data->num_domain_fqdns;
	    fqdn_location++) {
	if (ns_node_data->domain_fqdns[fqdn_location]) {
	    if (strcmp(ns_node_data->domain_fqdns[fqdn_location], fqdn)) {
		/* mis match */
		continue;
	    } else {
		/* we got a match, we should not have duplicates */
		*fqdn_index = fqdn_location;
		break;
	    }
	}
    }

bail:
    return err;
}

int
ns_fqdn_find_empty_slot(const namespace_node_data_t *ns_node_data,
	int *empty_index)
{
    int err = 0, fqdn_index = 0;

    *empty_index = -1;

    for (fqdn_index = 0;
	    fqdn_index < NKN_MAX_FQDNS;
	    fqdn_index++) {
	if (ns_node_data->domain_fqdns[fqdn_index] == NULL) {
	    /* found empty slot */
	    *empty_index = fqdn_index;
	    break;
	}
    }


    return err;
}

int
ns_fqdn_fill_holes(namespace_node_data_t *ns_node_data)
{
    int err = 0, fqdn_index = 0, hole_index = -1, move_index = 0;

    bail_require(ns_node_data);

    for (fqdn_index = 0;
	     fqdn_index < ns_node_data->num_domain_fqdns;) {
	if (ns_node_data->domain_fqdns[fqdn_index] == NULL) {
	    hole_index = fqdn_index;
	} else {
	    fqdn_index++;
	    continue;
	}
	move_index = fqdn_index;
	while(1) {
	    if ((move_index + 1) == NKN_MAX_FQDNS) {
		/* we have reached maximum, set last item as NULL */
		ns_node_data->domain_fqdns[NKN_MAX_FQDNS-1] = NULL;
		break;
	    }
	    /* move entries till next hole */
	    ns_node_data->domain_fqdns[move_index] =
		ns_node_data->domain_fqdns[move_index + 1];

	    move_index++;
	}
    }

bail:
    return err;
}
int
is_ns_domain_any(namespace_node_data_t *ns_node_data, int *result)
{
    int err = 0, fqdn_index = 0;

    bail_require(ns_node_data);
    bail_require(result);

    *result = 1;
    err = find_fqdn_index(ns_node_data, "*", &fqdn_index);
    bail_error(err);

    /* index == 0, is valid index */
    if (fqdn_index >= 0) {
	*result = 0;
    }

bail:
    return err;
}


/* return 0 => one of domain matches with regex
   returns 1 => some error or none of domain matched */

int
ns_match_regex_domain(namespace_node_data_t *ns_regex,
	namespace_node_data_t *ns_domain)
{
    int err = 0, i = 0;

    bail_require(ns_regex);
    bail_require(ns_domain);

    if ((ns_regex->domain_comp_regex == NULL)
	    || (ns_regex->domain_regex == NULL)) {
	err = 1;
	bail_error(err);
    }

    if (ns_domain->num_domain_fqdns == 0) {
	/* nothing to match, should it be success OR failure !!! */
	/* marking it as mismatch as of now */
	err = 1;
	bail_error(err);
    }

    for (i = 0; i < ns_domain->num_domain_fqdns; i++) {
	err = nkn_regexec(ns_regex->domain_comp_regex,
		ns_domain->domain_fqdns[i], NULL, 0);
	if (err == 0) {
	    /* regex matches with domain_fqdn[i], so return 0 */
	    break;
	}
    }

bail:
    return err;
}

int
ns_check_overlap_fqdns(namespace_node_data_t *ns_first,
	namespace_node_data_t *ns_second)
{
    int err = 0, fqdn_index = 0, i = 0 ;

    bail_require(ns_first);
    bail_require(ns_second);

    for (i = 0; i < ns_first->num_domain_fqdns; i++) {
	err = find_fqdn_index(ns_second, ns_first->domain_fqdns[i]
		, &fqdn_index);
	bail_error(err);
	if (fqdn_index >= 0) {
	    /* we found a match */
	    goto bail;
	}
    }

    /* if we here , then we didn't found any overlappig */
    err = 1;


bail:
    return err;
}

static int
nvsd_filter_handle_delete_change(const bn_binding_array *arr, uint32 idx,
                                    const bn_binding *binding,
                                    const tstring *bndgs_name,
                                    const tstr_array *name_parts,
                                    const tstring *bndgs_name_last_part,
                                    const tstring *bndgs_value,
                                    void *data)
{
    int err = 0;
    int ns_idx = 0;
    const tstring *name = NULL;
    const char *t_namespace = NULL;
    const char *filter_name = NULL;

    err = bn_binding_get_name(binding, &name);
    bail_error(err);


    /* Check if this is our node, XXX: should be "client-request/uf/ **" */
    if (bn_binding_name_pattern_match(ts_str(name),
		"/nkn/nvsd/namespace/*/client-request/**")) {
	/* our node */
	t_namespace = tstr_array_get_str_quick(name_parts, 3);
	if (search_namespace(t_namespace, &ns_idx) != 0)
	    goto bail;
	pstNamespace = get_namespace(ns_idx);

	if (!pstNamespace)
	    goto bail;
    } else {
	goto bail;
    }

    if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 3,
		"*", "client-request", "filter-map")) {
	if (pstNamespace->url_filter.uf_map_name != NULL) {
	    url_filter_data_t *url_filter_data = NULL;

	    url_filter_data = nvsd_mgmt_get_url_map(pstNamespace->url_filter.uf_map_name);
	    if (url_filter_data && url_filter_data->ns_name) {
		safe_free(url_filter_data->ns_name);
	    }
	    safe_free(pstNamespace->url_filter.uf_local_file);
#ifdef PRELOAD_UF_DATA
	    if (pstNamespace->url_filter.uf_trie_data) {
	    	delete_url_filter_trie(pstNamespace->url_filter.uf_trie_data);
	    	pstNamespace->url_filter.uf_trie_data = 0;
	    }
#endif
	    safe_free(pstNamespace->url_filter.uf_map_name);
	}
	safe_free(pstNamespace->url_filter.ns_name);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4,
		"*", "client-request", "uf", "200-msg")) {
	if (pstNamespace->url_filter.uf_reject_action == UF_REJECT_HTTP_200) {
	    safe_free(pstNamespace->url_filter.u_rej.http_200.response_body);
	}
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4,
		"*", "client-request", "uf", "3xx-fqdn")) {
	if (pstNamespace->url_filter.uf_reject_action == UF_REJECT_HTTP_301) {
	    safe_free(pstNamespace->url_filter.u_rej.http_301.redir_host);
	} else if (pstNamespace->url_filter.uf_reject_action == UF_REJECT_HTTP_302) {
	    safe_free(pstNamespace->url_filter.u_rej.http_302.redir_host);
	}
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4,
		"*", "client-request", "uf", "3xx-uri")) {
	if (pstNamespace->url_filter.uf_reject_action == UF_REJECT_HTTP_301) {
	    safe_free(pstNamespace->url_filter.u_rej.http_301.redir_url);
	} else if (pstNamespace->url_filter.uf_reject_action == UF_REJECT_HTTP_302) {
	    safe_free(pstNamespace->url_filter.u_rej.http_302.redir_url);
	}
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4,
		"*", "client-request", "uf-uri-size", "200-msg")) {
	if (pstNamespace->url_filter.uf_uri_size_reject_action ==
				    UF_REJECT_HTTP_200) {
	    safe_free(pstNamespace->url_filter.uri_size_rej.http_200.response_body);
	}
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4,
		"*", "client-request", "uf-uri-size", "3xx-fqdn")) {
	if (pstNamespace->url_filter.uf_uri_size_reject_action ==
				    UF_REJECT_HTTP_301) {
	    safe_free(pstNamespace->url_filter.uri_size_rej.http_301.redir_host);
	} else if (pstNamespace->url_filter.uf_uri_size_reject_action ==
				    UF_REJECT_HTTP_302) {
	    safe_free(pstNamespace->url_filter.uri_size_rej.http_302.redir_host);
	}
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4,
		"*", "client-request", "uf-uri-size", "3xx-uri")) {
	if (pstNamespace->url_filter.uf_uri_size_reject_action ==
				    UF_REJECT_HTTP_301) {
	    safe_free(pstNamespace->url_filter.uri_size_rej.http_301.redir_url);
	} else if (pstNamespace->url_filter.uf_uri_size_reject_action ==
				    UF_REJECT_HTTP_302) {
	    safe_free(pstNamespace->url_filter.uri_size_rej.http_302.redir_url);
	}
    }

bail:
    return err;
}


static int
nvsd_filter_handle_change(const bn_binding_array *arr, uint32 idx,
                                    const bn_binding *binding,
                                    const tstring *bndgs_name,
                                    const tstr_array *bndgs_name_components,
                                    const tstring *bndgs_name_last_part,
                                    const tstring *bndgs_value,
                                    void *data)
{
    int err = 0;
    int ns_idx = 0;
    const tstring *name = NULL;
    const char *t_namespace = NULL;
    const char *filter_name = NULL;
    tstr_array *name_parts = NULL;

    /* search namespace */
    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /* Check if this is our node, XXX: should be "client-request/uf/ **" */
    if (bn_binding_name_pattern_match(ts_str(name), "/nkn/nvsd/namespace/*/client-request/**"))
    {
	uint32 ret_flags = 0;
	err = bn_binding_get_type(binding, ba_value, NULL, &ret_flags);
	bail_error(err);

	if (ret_flags & btf_deleted)  {
	    /* binding has been deleted, old_bindings would handle this */
	    goto bail;
	}
	/* Get the namespace */
	err = bn_binding_get_name_parts (binding, &name_parts);
	bail_error(err);
	t_namespace = tstr_array_get_str_quick(name_parts, 3);

	lc_log_basic(LOG_DEBUG, "Read .../nkn/nvsd/namespace/ as : \"%s\"",
		t_namespace);

	if (search_namespace(t_namespace, &ns_idx) != 0)
	    goto bail;
	pstNamespace = get_namespace(ns_idx);

	if (!pstNamespace)
	    goto bail;

	//pfilter_map = get_filter_map_element(filter_name );
    } else {
	/* not interested */
	goto bail;
    }

    /* get each binding & configure namespace */
    if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 3,
		"*", "client-request", "filter-map")) {
	char *map_name = NULL;

	err = bn_binding_get_str(binding,
		ba_value, bt_string, NULL,
		&map_name);
	bail_error(err);
	if (pstNamespace->url_filter.uf_map_name != NULL) {
	    safe_free(pstNamespace->url_filter.uf_map_name);
	}
	if (map_name && strlen(map_name) > 0) {
	    url_filter_data_t *url_filter_data = NULL;
	    pstNamespace->url_filter.uf_map_name = nkn_strdup_type(map_name, mod_mgmt_charbuf);
	    url_filter_data = nvsd_mgmt_get_url_map(map_name);
	    if (url_filter_data) {
		url_filter_data->ns_name = nkn_strdup_type(pstNamespace->namespace,mod_mgmt_charbuf);
		pstNamespace->url_filter.uf_type =  url_filter_data->uf_type;
		if (url_filter_data->uf_local_file) {
		    pstNamespace->url_filter.uf_local_file = nkn_strdup_type(url_filter_data->uf_local_file, mod_mgmt_charbuf);
#ifdef PRELOAD_UF_DATA
		    pstNamespace->url_filter.uf_trie_data = 
		    	dup_url_filter_trie(url_filter_data->uf_trie_data);
#endif
		}
		lc_log_basic(LOG_NOTICE, "map-file bin: %s", pstNamespace->url_filter.uf_local_file?:"NO-LOCAL-FILE");
	    } else {
		lc_log_basic(LOG_NOTICE, "association with ns, but map-file details not found");
	    }
	}
	safe_free(map_name);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 3,
		"*", "client-request", "filter-type")) {
	char *filter_type = NULL;

	err = bn_binding_get_str(binding,
		ba_value, bt_string, NULL,
		&filter_type);
	bail_error(err);
	if (filter_type) {
	    if (0 == strcmp(filter_type, "black")) {
		pstNamespace->url_filter.uf_list_type = UF_LT_BLACK_LIST;
	    } else {
		pstNamespace->url_filter.uf_list_type = UF_LT_WHITE_LIST;
	    }
	    safe_free(filter_type);
	}
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 3,
		"*", "client-request", "filter-action")) {
	char *filter_action= NULL;

	err = bn_binding_get_str(binding,
		ba_value, bt_string, NULL,
		&filter_action);
	bail_error(err);
	if (filter_action) {
	    if (0 == strcmp(filter_action, "200")) {
		pstNamespace->url_filter.uf_reject_action =  UF_REJECT_HTTP_200;
	    } else if (0 == strcmp(filter_action, "301")) {
		pstNamespace->url_filter.uf_reject_action =  UF_REJECT_HTTP_301;
	    } else if (0 == strcmp(filter_action, "302")) {
		pstNamespace->url_filter.uf_reject_action =  UF_REJECT_HTTP_302;
	    } else if (0 == strcmp(filter_action, "403")) {
		pstNamespace->url_filter.uf_reject_action =  UF_REJECT_HTTP_403;
	    } else if (0 == strcmp(filter_action, "404")) {
		pstNamespace->url_filter.uf_reject_action =  UF_REJECT_HTTP_404;
	    } else if (0 == strcmp(filter_action, "200")) {
		pstNamespace->url_filter.uf_reject_action =  UF_REJECT_HTTP_200;
	    } else if (0 == strcmp(filter_action, "close")) {
		pstNamespace->url_filter.uf_reject_action =  UF_REJECT_CLOSE;
	    } else if (0 == strcmp(filter_action, "reset")) {
		pstNamespace->url_filter.uf_reject_action = UF_REJECT_RESET;
	    }
	    safe_free(filter_action);
	}
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4,
		"*", "client-request", "uf", "200-msg")) {
	char *msg_200 = NULL;

	err = bn_binding_get_str(binding,
		ba_value, bt_string, NULL,
		&msg_200);
	bail_error(err);

	if (msg_200 && strlen(msg_200) > 0) {
	    pstNamespace->url_filter.u_rej.http_200.response_body = nkn_strdup_type(msg_200, mod_mgmt_charbuf);
	}
	safe_free(msg_200);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4,
		"*", "client-request", "uf", "3xx-fqdn")) {
	char *fqdn_3xx= NULL;

	err = bn_binding_get_str(binding,
		ba_value, bt_string, NULL,
		&fqdn_3xx);
	bail_error(err);

	if (fqdn_3xx && strlen(fqdn_3xx) > 0) {
	    pstNamespace->url_filter.u_rej.http_301.redir_host = nkn_strdup_type(fqdn_3xx, mod_mgmt_charbuf);
	}
	safe_free(fqdn_3xx);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4,
		"*", "client-request", "uf", "3xx-uri")) {
	char *uri_3xx= NULL;

	err = bn_binding_get_str(binding,
		ba_value, bt_string, NULL,
		&uri_3xx);
	bail_error(err);

	if (uri_3xx &&  strlen(uri_3xx) > 0) {
	    pstNamespace->url_filter.u_rej.http_301.redir_url = nkn_strdup_type(uri_3xx, mod_mgmt_charbuf);
	}
	safe_free(uri_3xx);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 3,
		"*", "client-request", "filter-uri-size")) {
	uint32_t filter_uri_size;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &filter_uri_size);
	bail_error(err);
	pstNamespace->url_filter.uf_uri_size = filter_uri_size;
	/* Set the global filter size for Packet mode */
	glob_dpi_uri_filter_size = filter_uri_size;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 3,
		"*", "client-request", "filter-uri-size-action")) {
	char *filter_action= NULL;

	err = bn_binding_get_str(binding,
		ba_value, bt_string, NULL,
		&filter_action);
	bail_error(err);
	if (filter_action) {
	    if (0 == strcmp(filter_action, "200")) {
		pstNamespace->url_filter.uf_uri_size_reject_action =  UF_REJECT_HTTP_200;
	    } else if (0 == strcmp(filter_action, "301")) {
		pstNamespace->url_filter.uf_uri_size_reject_action =  UF_REJECT_HTTP_301;
	    } else if (0 == strcmp(filter_action, "302")) {
		pstNamespace->url_filter.uf_uri_size_reject_action =  UF_REJECT_HTTP_302;
	    } else if (0 == strcmp(filter_action, "403")) {
		pstNamespace->url_filter.uf_uri_size_reject_action =  UF_REJECT_HTTP_403;
	    } else if (0 == strcmp(filter_action, "404")) {
		pstNamespace->url_filter.uf_uri_size_reject_action =  UF_REJECT_HTTP_404;
	    } else if (0 == strcmp(filter_action, "200")) {
		pstNamespace->url_filter.uf_uri_size_reject_action =  UF_REJECT_HTTP_200;
	    } else if (0 == strcmp(filter_action, "close")) {
		pstNamespace->url_filter.uf_uri_size_reject_action =  UF_REJECT_CLOSE;
	    } else if (0 == strcmp(filter_action, "reset")) {
		pstNamespace->url_filter.uf_uri_size_reject_action = UF_REJECT_RESET;
	    }
	    safe_free(filter_action);
	}
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4,
		"*", "client-request", "uf-uri-size", "200-msg")) {
	char *msg_200 = NULL;

	err = bn_binding_get_str(binding,
		ba_value, bt_string, NULL,
		&msg_200);
	bail_error(err);

	if (msg_200 && strlen(msg_200) > 0) {
	    pstNamespace->url_filter.uri_size_rej.http_200.response_body = nkn_strdup_type(msg_200, mod_mgmt_charbuf);
	}
	safe_free(msg_200);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4,
		"*", "client-request", "uf-uri-size", "3xx-fqdn")) {
	char *fqdn_3xx= NULL;

	err = bn_binding_get_str(binding,
		ba_value, bt_string, NULL,
		&fqdn_3xx);
	bail_error(err);

	if (fqdn_3xx && strlen(fqdn_3xx) > 0) {
	    pstNamespace->url_filter.uri_size_rej.http_301.redir_host = nkn_strdup_type(fqdn_3xx, mod_mgmt_charbuf);
	}
	safe_free(fqdn_3xx);
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 3, 4,
		"*", "client-request", "uf-uri-size", "3xx-uri")) {
	char *uri_3xx= NULL;

	err = bn_binding_get_str(binding,
		ba_value, bt_string, NULL,
		&uri_3xx);
	bail_error(err);

	if (uri_3xx &&  strlen(uri_3xx) > 0) {
	    pstNamespace->url_filter.uri_size_rej.http_301.redir_url = nkn_strdup_type(uri_3xx, mod_mgmt_charbuf);
	}
	safe_free(uri_3xx);
    }

bail:
    tstr_array_free(&name_parts);
    return err;

}



/* End of nvsd_mgmt_namespace.c */
