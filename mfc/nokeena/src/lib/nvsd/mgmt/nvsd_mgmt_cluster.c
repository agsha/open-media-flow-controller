
/*
 *
 * Filename:  nvsd_mgmt_cluster.c
 * Date:      2010/09/01 
 * Author:    Manikandan Vengatachalam
 *
 * (C) Copyright 2010 Juniper Networks, Inc.  
 * All rights reserved.
 *
 *
 */

/* Standard Headers */
#include <stdio.h>
#include <glib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

/* Samara Headers */
#include "md_client.h"
#include "license.h"

/* nvsd Headers */
#include "nkn_defs.h"
#include "nvsd_mgmt.h"
#include "nkn_namespace.h"
#include "nvsd_mgmt_namespace.h"
#include "nvsd_mgmt_cluster.h"
#include "nkn_mgmt_defs.h"
#include "nvsd_mgmt_lib.h"

extern unsigned int jnpr_log_level;

/* Local Function Prototypes */
int nvsd_mgmt_print_clusters(void
	);
int nvsd_clear_cluster(cluster_node_data_t * cl_map);
int nvsd_cluster_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings);

static int
nvsd_cluster_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value, void *data);

static int
nvsd_cluster_delete_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array *
	bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value,
	void *data);

int nvsd_get_rr_method_from_bns(const bn_binding_array * bindings,
	const char *clname,
	uint32 * rr_method, tbool * found);

int nvsd_get_cl_type_from_bns(const bn_binding_array * bindings,
	const char *clname,
	uint32 * cl_type, tbool * found);
int
get_suffix_element(cluster_node_data_t * cl_node,
	const suffix_name_t suff_name,
	cl7_suffix_map_t ** suffix_map);

/* Local Macros */

/* Extern Variables */
extern md_client_context *nvsd_mcc;

/* Local Variables */

/* logging level, nkn_log_level is 7, LOG_NOTICE is 5 */
static int nkn_log_level = LOG_DEBUG;
const char cluster_config_prefix[] = "/nkn/nvsd/cluster/config";
static cluster_node_data_t *pstClusterMap = NULL;
cluster_node_data_t lstClusterMaps[MAX_CLUSTER_MAPS];
static tbool dynamic_change = false;

/* Extern Functions */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_cluster_cfg_query()
 *	purpose : query for cluster config parameters
 */
int
nvsd_cluster_cfg_query(const bn_binding_array * bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    lc_log_basic(jnpr_log_level,
	    "nvsd cluster module mgmtd query initializing ..");

    /*
     * Initialize the ServerMap list 
     */
    memset(lstClusterMaps, 0, MAX_CLUSTER_MAPS * sizeof (cluster_node_data_t));

    /*
     * Bind to get CLUSTER 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    cluster_config_prefix,
	    NULL,
	    nvsd_cluster_cfg_handle_change,
	    &rechecked_licenses);

    bail_error(err);

    nvsd_mgmt_print_clusters();
bail:
    return err;
}								/* end of nvsd_cluster_cfg_query() */

int
nvsd_mgmt_print_cluster(cluster_node_data_t * cl);
int
nvsd_mgmt_print_cluster(cluster_node_data_t * cl)
{
    proxy_method_data_t *pd_data = NULL;
    redirect_method_data_t *data = NULL;
    lb_data_t *lb_data = NULL;
    int log_level = nkn_log_level;

    if (cl == NULL) {
	return 0;
    }
    lc_log_basic(log_level, "Cl-%s, smap-%s, type-%d, hash-%d",
	    cl->cluster_name ? : "NULL",
	    cl->server_map_name ? : "NULL", cl->cl_type, cl->hash_url);
    if (cl->cl_type == 1) {
	/*
	 * redirect cluster 
	 */
	if (cl->u.redirect_cluster.method == 1) {
	    data = &cl->u.redirect_cluster.u.redirect_only.rd_data;
	} else if (cl->u.redirect_cluster.method == 2) {
	    data = &cl->u.redirect_cluster.u.redirect_replicate.rd_data;
	    lb_data = &cl->u.redirect_cluster.u.redirect_replicate.lb_data;
	} else {
	    /*
	     * don't know what to do 
	     */
	}
	if (data) {
	    lc_log_basic(log_level, "redir-data=> addr-%s, path-%s, l-redir-%d",
		    data->addr_port ? : "NULL",
		    data->baseRedirectPath ? : "NULL",
		    data->local_redirect);
	}
	if (lb_data) {
	    lc_log_basic(log_level, "lb-redir-data=> replica-%d, thrsh-%d",
		    lb_data->replicate, lb_data->threshold);
	}
    } else if (cl->cl_type == 2) {
	/*
	 * proxy cluster 
	 */
	if (cl->u.proxy_cluster.method == 1) {
	    pd_data = &cl->u.proxy_cluster.u.proxy_only.pd_data;
	} else if (cl->u.proxy_cluster.method == 2) {
	    pd_data = &cl->u.proxy_cluster.u.proxy_replicate.pd_data;
	    lb_data = &cl->u.proxy_cluster.u.proxy_replicate.lb_data;
	} else {
	    /*
	     * don't know what to do 
	     */
	}
	if (pd_data) {
	    lc_log_basic(log_level, "proxy-data=> addr-%s, path-%s, l-redir-%d",
		    pd_data->addr_port ? : "NULL",
		    pd_data->baseRedirectPath ? : "NULL",
		    pd_data->local_redirect);
	}
	if (lb_data) {
	    lc_log_basic(log_level, "lb-proxy-data=> replica-%d, thrsh-%d",
		    lb_data->replicate, lb_data->threshold);
	}
    } else {
	/*
	 * no idea 
	 */
    }
    return 0;
}

int
nvsd_mgmt_print_clusters(void)
{
    int i = 0;

    for (i = 0; i < MAX_CLUSTER_MAPS; i++) {
	if (NULL == lstClusterMaps[i].cluster_name) {
	    /*
	     * do nothing, go to next 
	     */
	    continue;
	} else {
	    /*
	     * print all clusters 
	     */
	    nvsd_mgmt_print_cluster(&lstClusterMaps[i]);
	}
    }
    return 0;
}


/*
 *	funtion : nvsd_cluster_module_cfg_handle_change()
 *	purpose : handler for config changes for cluster module
 */
int
nvsd_cluster_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    /*
     * Reset the flag 
     */
    dynamic_change = false;

    /*
     * Call the new bindings callbacks 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    cluster_config_prefix,
	    NULL,
	    nvsd_cluster_cfg_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    /*
     * Call the old bindings callbacks 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(old_bindings,
	    cluster_config_prefix,
	    NULL,
	    nvsd_cluster_delete_cfg_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    nvsd_mgmt_print_clusters();
    /*
     * If relevant node has changed then indicate to the module 
     */
    if (dynamic_change)
	nkn_namespace_config_update(NULL);

bail:
    return err;

}	/* end of nvsd_cluster_module_cfg_handle_change() */

int
get_suffix_element(cluster_node_data_t * cl_node,
	const suffix_name_t suff_name,
	cl7_suffix_map_t ** suffix_map)
{
    int i = 0, free_entry = -1;
    cl7_suffix_map_t *temp_suffix_map = NULL;

    if (cl_node == NULL || suff_name == NULL) {
	/*
	 * invalid input 
	 */
	lc_log_debug(nkn_log_level, "cl_node or suff_name is NULL");
	return EINVAL;
    }

    for (i = 0; i < MAX_SUFFIX_NUM; i++) {
	temp_suffix_map = &cl_node->suffix_map[i];
	if (0 == temp_suffix_map->valid) {
	    if (free_entry == -1) {
		free_entry = i;
	    }
	    continue;
	}
	if (0 == strcmp(temp_suffix_map->suffix_name, suff_name)) {
	    /*
	     * found it 
	     */
	    *suffix_map = temp_suffix_map;
	    return 0;
	}
    }
    *suffix_map = NULL;
    if (free_entry != -1) {
	*suffix_map = &cl_node->suffix_map[free_entry];
	/*
	 * not setting valid bit 
	 */
    } else {
	return ENOMEM;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */

/*
 *	funtion : get_cluster_element ()
 *	purpose : get the element in the array for the given cluster, 
 *		if not found then return the first free element
 */
cluster_node_data_t *
get_cluster_element(const char *cpClusterMap)
{
    int i;
    int free_entry_index = -1;

    for (i = 0; i < MAX_CLUSTER_MAPS; i++) {
	if (NULL == lstClusterMaps[i].cluster_name) {
	    /*
	     * Save the first free entry 
	     */
	    if (-1 == free_entry_index)
		free_entry_index = i;

	    /*
	     * Empty entry hence continue 
	     */
	    continue;
	} else if (0 == strcmp(cpClusterMap, lstClusterMaps[i].cluster_name)) {
	    /*
	     * Found match hence return this entry 
	     */
	    return (&(lstClusterMaps[i]));
	}
    }

    /*
     * Now that we have gone thru the list and no match return the
     * free_entry_index 
     */
    if (-1 != free_entry_index)
	return (&(lstClusterMaps[free_entry_index]));

    /*
     * No match and no free entry 
     */
    return (NULL);
}	/* end of get_cluster_element () */


/*
 *	funtion : nvsd_cluster_cfg_handle_change()
 *	purpose : handler for config changes for cluster module
 */
static int
nvsd_cluster_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value, void *data)
{
    int err = 0;
    uint32 new_cl_type = 0, new_rr_method = 0;
    tbool cl_type_found = false, rr_method_found = false;
    const tstring *name = NULL;
    const char *t_cluster = NULL;
    tstr_array *name_parts = NULL;
    tstr_array *suffix_name_parts = NULL;
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
	    (ts_str(name), "/nkn/nvsd/cluster/config/**")) {
	uint32 ret_flags = 0;

	err = bn_binding_get_type(binding, ba_value, NULL, &ret_flags);
	bail_error(err);

	if (ret_flags & btf_deleted) {
	    /*
	     * binding has been deleted, old_bindings would handle this 
	     */
	    goto bail;
	}

	/*-------- Get the ServerMap ------------*/
	bn_binding_get_name_parts(binding, &name_parts);
	bail_error(err);
	t_cluster = tstr_array_get_str_quick(name_parts, 4);

	bail_error(err);
	lc_log_basic(jnpr_log_level,
		"Read .../nkn/nvsd/cluster/config as : \"%s\"", t_cluster);

	/*
	 * Get the cluster entry in the global array 
	 */
	pstClusterMap = get_cluster_element(t_cluster);
	if (!pstClusterMap)
	    goto bail;

	if (NULL == pstClusterMap->cluster_name)
	    pstClusterMap->cluster_name =
		nkn_strdup_type(t_cluster, mod_mgmt_charbuf);
    } else {
	/*
	 * This is not the server_map node hence leave 
	 */
	goto bail;
    }

    bn_binding_dump(nkn_log_level, "CLUSTER", binding);
    err = nvsd_get_cl_type_from_bns(arr, t_cluster,
	    &new_cl_type, &cl_type_found);
    bail_error(err);

    err = nvsd_get_rr_method_from_bns(arr, t_cluster, &new_rr_method,
		&rr_method_found);
    bail_error(err);

    lc_log_debug(nkn_log_level, "TYPE:curr-> %d, new-> %d, node->%s",
	    pstClusterMap->cl_type, new_cl_type,
	    cl_type_found ? "found" : "not found");
    lc_log_debug(nkn_log_level, "RR-METHOD: new-> %d, node->%s",
	    new_rr_method, rr_method_found ? "found" : "not found");

    if (cl_type_found) {
	if (new_cl_type != pstClusterMap->cl_type) {
	    /*
	     * this function doesn't frees cluster, only free parameters
	     * memory 
	     */
	    pstClusterMap->cl_type = new_cl_type;
	}
    }

    if (rr_method_found == true) {
	switch (pstClusterMap->cl_type) {
	    case CL_TYPE_REDIRECT:
		pstClusterMap->u.redirect_cluster.method = new_rr_method;
		break;
	    case CL_TYPE_PROXY:
		pstClusterMap->u.proxy_cluster.method = new_rr_method;
		break;
	    default:
		break;
	}
    }

    /*
     * Get the server-map 
     */
    if (bn_binding_name_pattern_match
	    (ts_str(name), "/nkn/nvsd/cluster/config/*/server-map")) {
	tstring *t_server_map = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_server_map);
	bail_error(err);

	lc_log_basic(nkn_log_level,
		"Read .../nkn/nvsd/cluster/config/%s/server-map as : \"%s\"",
		t_cluster, t_server_map ? ts_str(t_server_map) : "");

	if (NULL != pstClusterMap->server_map_name) {
	    safe_free(pstClusterMap->server_map_name);
	}

	if (t_server_map) {
	    pstClusterMap->server_map_name =
		nkn_strdup_type(ts_str(t_server_map), mod_mgmt_charbuf);
	    pstClusterMap->serverMap =
		nvsd_mgmt_get_server_map((char *) ts_str(t_server_map));
	} else {
	    pstClusterMap->server_map_name = NULL;
	    pstClusterMap->serverMap = NULL;
	}

	ts_free(&t_server_map);
    }
    /*
     * Get the CLUSTER TYPE 
     */
    else if (bn_binding_name_pattern_match
	    (ts_str(name), "/nkn/nvsd/cluster/config/*/cluster-type")) {
	uint32 t_cl_type = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_cl_type);

	bail_error(err);

	lc_log_basic(nkn_log_level,
		"Read .../nkn/nvsd/cluster/config/%s/cluster-type as : \"%d\"",
		t_cluster, t_cl_type);
	pstClusterMap->cl_type = t_cl_type;
    }
    /*
     * Get the CLUSTER HASH URL TYPE 
     */
    else if (bn_binding_name_pattern_match
	    (ts_str(name), "/nkn/nvsd/cluster/config/*/cluster-hash")) {
	uint32 t_cl_hash = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_cl_hash);

	bail_error(err);

	lc_log_basic(nkn_log_level,
		"Read .../nkn/nvsd/cluster/config/%s/cluster-hash as : \"%d\"",
		t_cluster, t_cl_hash);
	pstClusterMap->hash_url = t_cl_hash;
    }
    /*
     * Get the REQ ROUTING METHOD 
     */
    else if (bn_binding_name_pattern_match (ts_str(name),
	     "/nkn/nvsd/cluster/config/*/request-routing-method")) {
	uint32 t_rr_method = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_rr_method);

	bail_error(err);

	lc_log_basic(nkn_log_level,
		"Read .../cluster/config/%s/request-routing-methode as : \"%d\"",
		t_cluster, t_rr_method);
	/*
	 * RR-method needs to set based on cluster type 
	 */
	if (pstClusterMap->cl_type == CL_TYPE_REDIRECT) {
	    pstClusterMap->u.redirect_cluster.method = t_rr_method;
	} else if (pstClusterMap->cl_type == CL_TYPE_PROXY) {
	    pstClusterMap->u.proxy_cluster.method = t_rr_method;
	} else {
	    /*
	     * XXX - What should be done here 
	     */
	}
    }
    /*
     * Get the REDIRECT ADDR PORT 
     */
    else if (bn_binding_name_pattern_match
	    (ts_str(name), "/nkn/nvsd/cluster/config/*/redirect-addr-port")) {
	tstring *t_addr_port = NULL;

	if (pstClusterMap->cl_type != CL_TYPE_REDIRECT) {
	    goto bail;
	}
	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_addr_port);
	bail_error(err);

	lc_log_basic(nkn_log_level,
		"Read .../cluster/config/%s/redirect-addr-port as : \"%s\"",
		t_cluster, t_addr_port ? ts_str(t_addr_port) : "");

	if (pstClusterMap->u.redirect_cluster.method ==
		CONSISTENT_HASH_REDIRECT) {
	    if (NULL != pstClusterMap->cnd_redir.rd_data.addr_port)
		safe_free(pstClusterMap->cnd_redir.rd_data.addr_port);

	    if (t_addr_port)
		pstClusterMap->cnd_redir.rd_data.addr_port =
		    nkn_strdup_type(ts_str(t_addr_port), mod_mgmt_charbuf);
	    else
		pstClusterMap->cnd_redir.rd_data.addr_port = NULL;

	    ts_free(&t_addr_port);
	} else if (pstClusterMap->u.redirect_cluster.method ==
		CHASH_REDIRECT_REPLICATE) {
	    if (NULL != pstClusterMap->cnd_redir_repl.rd_data.addr_port)
		safe_free(pstClusterMap->cnd_redir_repl.rd_data.addr_port);

	    if (t_addr_port)
		pstClusterMap->cnd_redir_repl.rd_data.addr_port =
		    nkn_strdup_type(ts_str(t_addr_port), mod_mgmt_charbuf);
	    else
		pstClusterMap->cnd_redir_repl.rd_data.addr_port = NULL;

	    ts_free(&t_addr_port);
	} else {
	    /*
	     * XXX - what to do 
	     */
	}
    }
    /*
     * Get the REDIRECT LOCAL value 
     */
    else if (bn_binding_name_pattern_match
	    (ts_str(name), "/nkn/nvsd/cluster/config/*/redirect-local")) {
	tbool t_redir_local = false;

	if (pstClusterMap->cl_type != CL_TYPE_REDIRECT) {
	    goto bail;
	}

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_redir_local);

	bail_error(err);

	lc_log_basic(nkn_log_level,
		"Read .../cluster/config/%s/redirect-local as : \"%d\"",
		t_cluster, t_redir_local);

	if (pstClusterMap->u.redirect_cluster.method ==
		CONSISTENT_HASH_REDIRECT) {
	    pstClusterMap->cnd_redir.rd_data.local_redirect = t_redir_local;
	} else {
	    pstClusterMap->cnd_redir_repl.rd_data.local_redirect =
		t_redir_local;
	}
    }
    /*
     * Get the REDIRECT BASE PATH 
     */
    else if (bn_binding_name_pattern_match
	    (ts_str(name), "/nkn/nvsd/cluster/config/*/redirect-base-path")) {
	tstring *t_redir_base_path = NULL;

	if (pstClusterMap->cl_type != CL_TYPE_REDIRECT) {
	    goto bail;
	}

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL,
		&t_redir_base_path);
	bail_error(err);

	lc_log_basic(nkn_log_level,
		"Read .../cluster/config/%s/redirect-base-path as : \"%s\"",
		t_cluster,
		t_redir_base_path ? ts_str(t_redir_base_path) : "");

	/*
	 * XXX - this could be a issue here. We might be freeing a
	 * unallocated memory or wrong memory this must be handled 
	 */
	if (pstClusterMap->u.redirect_cluster.method ==
		CONSISTENT_HASH_REDIRECT) {
	    if (NULL != pstClusterMap->cnd_redir.rd_data.baseRedirectPath)
		safe_free(pstClusterMap->cnd_redir.rd_data.baseRedirectPath);

	    if (t_redir_base_path)
		pstClusterMap->cnd_redir.rd_data.baseRedirectPath =
		    nkn_strdup_type(ts_str(t_redir_base_path),
			    mod_mgmt_charbuf);
	    else
		pstClusterMap->cnd_redir.rd_data.baseRedirectPath = NULL;

	    ts_free(&t_redir_base_path);
	} else {
	    if (NULL != pstClusterMap->cnd_redir_repl.rd_data.baseRedirectPath)
		safe_free(pstClusterMap->cnd_redir_repl.rd_data.
			baseRedirectPath);

	    if (t_redir_base_path)
		pstClusterMap->cnd_redir_repl.rd_data.baseRedirectPath =
		    nkn_strdup_type(ts_str(t_redir_base_path),
			    mod_mgmt_charbuf);
	    else
		pstClusterMap->cnd_redir_repl.rd_data.baseRedirectPath = NULL;

	    ts_free(&t_redir_base_path);
	}
    }
    /*
     * Get the proxy LOCAL value 
     */
    else if (bn_binding_name_pattern_match
	    (ts_str(name), "/nkn/nvsd/cluster/config/*/proxy-local")) {
	tbool t_proxy_local = false;

	if (pstClusterMap->cl_type != CL_TYPE_PROXY) {
	    goto bail;
	}

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_proxy_local);
	bail_error(err);

	lc_log_basic(nkn_log_level,
		"Read .../cluster/config/%s/proxy-local as : \"%d\"",
		t_cluster, t_proxy_local);

	if (pstClusterMap->u.proxy_cluster.method == CONSISTENT_HASH_PROXY) {
	    /*
	     * if (rr-method == CH-Proxy) then set values to proxy_only
	     * struture 
	     */
	    pstClusterMap->cnd_proxy.pd_data.local_redirect = t_proxy_local;
	} else if (pstClusterMap->u.proxy_cluster.method ==
		CHASH_PROXY_REPLICATE) {
	    /*
	     * if load-balance is enabled, set values to replicate 
	     */
	    pstClusterMap->cnd_proxy_repl.pd_data.local_redirect =
		t_proxy_local;
	} else {
	    /*
	     * XXX what to do ? 
	     */
	}
    }
    /*
     * Get the PROXY BASE PATH 
     */
    else if (bn_binding_name_pattern_match
	    (ts_str(name), "/nkn/nvsd/cluster/config/*/proxy-base-path")) {
	tstring *t_proxy_base_path = NULL;

	if (pstClusterMap->cl_type != CL_TYPE_PROXY) {
	    goto bail;
	}

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL,
		&t_proxy_base_path);
	bail_error(err);

	lc_log_basic(nkn_log_level,
		"Read .../cluster/config/%s/proxy-base-path as : \"%s\"",
		t_cluster,
		t_proxy_base_path ? ts_str(t_proxy_base_path) : "");

	/*
	 * XXX - this might be a issue here. We may be freeing a unallocated
	 * memory or wrong memory this must be handled 
	 */
	if (pstClusterMap->u.proxy_cluster.method == CONSISTENT_HASH_PROXY) {
	    if (NULL != pstClusterMap->cnd_proxy.pd_data.baseRedirectPath)
		safe_free(pstClusterMap->cnd_proxy.pd_data.baseRedirectPath);

	    if (t_proxy_base_path)
		pstClusterMap->cnd_proxy.pd_data.baseRedirectPath =
		    nkn_strdup_type(ts_str(t_proxy_base_path),
			    mod_mgmt_charbuf);
	    else
		pstClusterMap->cnd_proxy.pd_data.baseRedirectPath = NULL;
	    ts_free(&t_proxy_base_path);
	} else if (pstClusterMap->u.proxy_cluster.method ==
		CHASH_PROXY_REPLICATE) {
	    /*
	     * load-balance is enabled 
	     */
	    if (NULL != pstClusterMap->cnd_proxy.pd_data.baseRedirectPath)
		safe_free(pstClusterMap->cnd_proxy_repl.pd_data.
			baseRedirectPath);

	    if (t_proxy_base_path)
		pstClusterMap->cnd_proxy_repl.pd_data.baseRedirectPath =
		    nkn_strdup_type(ts_str(t_proxy_base_path),
			    mod_mgmt_charbuf);
	    else
		pstClusterMap->cnd_proxy_repl.pd_data.baseRedirectPath = NULL;

	    ts_free(&t_proxy_base_path);
	} else {
	    /*
	     * XXX - what to do 
	     */
	}
    } else if (bn_binding_name_pattern_match
	    (ts_str(name), "/nkn/nvsd/cluster/config/*/proxy-addr-port")) {
	tstring *t_addr_port = NULL;

	if (pstClusterMap->cl_type != CL_TYPE_PROXY) {
	    goto bail;
	}

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_addr_port);
	bail_error(err);

	lc_log_basic(nkn_log_level,
		"Read .../cluster/config/%s/proxy-addr-port as : \"%s\"",
		t_cluster, t_addr_port ? ts_str(t_addr_port) : "");

	if (pstClusterMap->u.proxy_cluster.method == CONSISTENT_HASH_PROXY) {
	    /*
	     * load-balance is disabled 
	     */
	    if (NULL != pstClusterMap->cnd_proxy.pd_data.addr_port)
		safe_free(pstClusterMap->cnd_proxy.pd_data.addr_port);

	    if (t_addr_port)
		pstClusterMap->cnd_proxy.pd_data.addr_port =
		    nkn_strdup_type(ts_str(t_addr_port), mod_mgmt_charbuf);
	    else
		pstClusterMap->cnd_proxy.pd_data.addr_port = NULL;

	    ts_free(&t_addr_port);
	} else if (pstClusterMap->u.proxy_cluster.method ==
		CHASH_PROXY_REPLICATE) {
	    /*
	     * load-balance is enabled 
	     */
	    if (NULL != pstClusterMap->cnd_proxy_repl.pd_data.addr_port)
		safe_free(pstClusterMap->cnd_proxy_repl.pd_data.addr_port);

	    if (t_addr_port)
		pstClusterMap->cnd_proxy_repl.pd_data.addr_port =
		    nkn_strdup_type(ts_str(t_addr_port), mod_mgmt_charbuf);
	    else
		pstClusterMap->cnd_proxy_repl.pd_data.addr_port = NULL;

	    ts_free(&t_addr_port);
	} else {
	    /*
	     * XXX - What to do 
	     */
	}
    } else if (bn_binding_name_pattern_match
	    (ts_str(name), "/nkn/nvsd/cluster/config/*/proxy-cluster-hash")) {
	uint32 t_cl_hash = 0;

	if (pstClusterMap->cl_type != CL_TYPE_PROXY) {
	    goto bail;
	}

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_cl_hash);

	bail_error(err);

	lc_log_basic(nkn_log_level,
		"Read .../nkn/nvsd/cluster/config/%s/proxy-cluster-hash as : \"%d\"",
		t_cluster, t_cl_hash);
	pstClusterMap->hash_url = t_cl_hash;
    }
    /*
     * GET the LOAD THRESHOLD 
     */
    else if (bn_binding_name_pattern_match
	    (ts_str(name), "/nkn/nvsd/cluster/config/*/load-threshold")) {
	uint32 t_lb_threshold = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_lb_threshold);

	bail_error(err);

	lc_log_basic(nkn_log_level,
		"Read .../cluster/config/%s/load-threshold as : \"%d\"",
		t_cluster, t_lb_threshold);

	if (pstClusterMap->cl_type == CL_TYPE_REDIRECT) {
	    pstClusterMap->cnd_redir_repl.lb_data.threshold = t_lb_threshold;
	} else if (pstClusterMap->cl_type == CL_TYPE_PROXY) {
	    pstClusterMap->cnd_proxy_repl.lb_data.threshold = t_lb_threshold;
	} else {
	    /*
	     * XXX - What to do 
	     */
	}
    } else if (bn_binding_name_pattern_match
	    (ts_str(name), "/nkn/nvsd/cluster/config/*/replicate-mode")) {
	uint32 t_lb_repli_mode = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_lb_repli_mode);

	bail_error(err);

	lc_log_basic(nkn_log_level,
		"Read .../cluster/config/%s/replicate-mode as : \"%d\"",
		t_cluster, t_lb_repli_mode);
	if (pstClusterMap->cl_type == CL_TYPE_REDIRECT) {
	    pstClusterMap->cnd_redir_repl.lb_data.replicate = t_lb_repli_mode;
	} else if (pstClusterMap->cl_type == CL_TYPE_PROXY) {
	    pstClusterMap->cnd_proxy_repl.lb_data.replicate = t_lb_repli_mode;
	} else {
	    /*
	     * XXX - what to do 
	     */
	}
    } else if (bn_binding_name_pattern_match
	    (ts_str(name), "/nkn/nvsd/cluster/config/*/suffix/*/action")) {
	/*
	 * "/nkn/nvsd/cluster/config/<cl-name>/suffix/<suffix-name>" will be
	 * implicitly handled 
	 */
	cl7_suffix_map_t *suffix_node = NULL;
	const char *suff_name = NULL;
	uint32 suffix_action = 0;

	bn_binding_get_name_parts(binding, &suffix_name_parts);
	bail_error(err);
	suff_name = tstr_array_get_str_quick(suffix_name_parts, 6);
	bail_error_null(err, suff_name);

	err = get_suffix_element(pstClusterMap, suff_name, &suffix_node);
	if (err || suffix_node == NULL) {
	    /*
	     * invalid input or no all entries full 
	     */
	    lc_log_debug(nkn_log_level, "err in finding suffix (%s) - %d",
		    suff_name, err);
	    goto bail;
	}

	err = bn_binding_get_uint32(binding, ba_value, NULL, &suffix_action);
	bail_error(err);

	suffix_node->action = suffix_action;
	if (suffix_node->valid == 0) {
	    /*
	     * suffix node is unused, this is addition 
	     */
	    strlcpy(suffix_node->suffix_name, suff_name,
		    sizeof (suffix_node->suffix_name));
	    suffix_node->valid = 1;
	    pstClusterMap->suffix_map_entries++;
	} else {
	    /*
	     * suffix already exists, update the action 
	     */
	}
    }

    /*
     * Set the flag so that we know this is a dynamic change 
     */
    dynamic_change = true;

bail:
    tstr_array_free(&name_parts);
    tstr_array_free(&suffix_name_parts);
    return err;

}	/* end of nvsd_cluster_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_cluster_delete_cfg_handle_change()
 *	purpose : handler for config delete changes for cluster module
 */
static int
nvsd_cluster_delete_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    const char *t_cluster = NULL;
    tstr_array *name_parts = NULL;
    tstr_array *suffix_name_parts = NULL;
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
	    (ts_str(name), "/nkn/nvsd/cluster/config/**")) {

	/*-------- Get the Cluster ------------*/
	bn_binding_get_name_parts(binding, &name_parts);
	bail_error(err);
	t_cluster = tstr_array_get_str_quick(name_parts, 4);

	bail_error(err);
	lc_log_basic(nkn_log_level,
		"Read .../nkn/nvsd/cluster/config as : \"%s\"", t_cluster);

	/*
	 * Delete this only if the wildcard change is received 
	 */
	if (bn_binding_name_pattern_match
		(ts_str(name), "/nkn/nvsd/cluster/config/*")) {
	    /*
	     * Get the server_map entry in the global array 
	     */
	    pstClusterMap = get_cluster_element(t_cluster);
	    if (pstClusterMap && (NULL != pstClusterMap->cluster_name)) {
		/*
		 * Free the allocated name 
		 */
		free(pstClusterMap->cluster_name);

		/*
		 * Free the other allocated fields 
		 */
		if (pstClusterMap->server_map_name)
		    safe_free(pstClusterMap->server_map_name);

		nvsd_clear_cluster(pstClusterMap);
		memset(pstClusterMap, 0, sizeof (cluster_node_data_t));
	    }

	    /*
	     * Set the flag so that we know this is a dynamic change 
	     */
	    dynamic_change = true;
	} else if (bn_binding_name_pattern_match(ts_str(name),
		    "/nkn/nvsd/cluster/config/*/suffix/*")) {
	    bn_binding_dump(nkn_log_level, "dgautam", binding);
	    pstClusterMap = get_cluster_element(t_cluster);
	    if (pstClusterMap && (NULL != pstClusterMap->cluster_name)) {
		cl7_suffix_map_t *suffix_node = NULL;
		const char *suff_name = NULL;

		bn_binding_get_name_parts(binding, &suffix_name_parts);
		bail_error(err);
		suff_name = tstr_array_get_str_quick(suffix_name_parts, 6);
		bail_error_null(err, suff_name);

		err =
		    get_suffix_element(pstClusterMap, suff_name, &suffix_node);
		if (err || (suffix_node == NULL)) {
		    lc_log_debug(nkn_log_level, "err in getting %s, %d",
			    suff_name, err);
		    goto bail;
		}
		/*
		 * mark suffix as invalid, this is as good as freeing it 
		 */
		memset(suffix_node, 0, sizeof (*suffix_node));
		pstClusterMap->suffix_map_entries--;

		dynamic_change = true;
	    }
	}
    } else {
	/*
	 * This is not the cluster node hence leave 
	 */
	goto bail;
    }

bail:
    tstr_array_free(&name_parts);
    tstr_array_free(&suffix_name_parts);
    return err;

}	/* end of nvsd_cluster_delete_cfg_handle_change() */

int
nvsd_get_cl_type_from_bns(const bn_binding_array * bindings,
	const char *clname, uint32 * cl_type, tbool * found)
{
    int err = 0;
    node_name_t cl_node = { 0 };
    uint32 value = 0;
    const bn_binding *binding = NULL;

    snprintf(cl_node, sizeof (cl_node),
	    "/nkn/nvsd/cluster/config/%s/cluster-type", clname);
    err = bn_binding_array_get_binding_by_name(bindings, cl_node, &binding);
    bail_error(err);
    if (binding == NULL) {
	*found = false;
	goto bail;
    } else {
	err = bn_binding_get_uint32(binding, ba_value, NULL, &value);
	bail_error(err);
	*cl_type = value;
	*found = true;
    }

bail:
    return err;
}

int
nvsd_get_rr_method_from_bns(const bn_binding_array * bindings,
	const char *clname,
	uint32 * rr_method, tbool * found)
{
    int err = 0;
    node_name_t cl_node = { 0 };
    uint32 value = 0;
    const bn_binding *binding = NULL;

    snprintf(cl_node, sizeof (cl_node),
	    "/nkn/nvsd/cluster/config/%s/request-routing-method", clname);
    err = bn_binding_array_get_binding_by_name(bindings, cl_node, &binding);
    bail_error(err);
    if (binding == NULL) {
	*found = false;
	goto bail;
    } else {
	err = bn_binding_get_uint32(binding, ba_value, NULL, &value);
	bail_error(err);
	*rr_method = value;
	*found = true;
    }

bail:
    return err;
}

int
nvsd_clear_cluster(cluster_node_data_t * cl_map)
{
    redirect_method_data_t *rd_data = NULL;
    proxy_method_data_t *pd_data = NULL;

    if (cl_map->cl_type == 1) {
	/*
	 * free redirect allocations 
	 */
	if (cl_map->u.redirect_cluster.method == 1) {
	    rd_data = &cl_map->cnd_redir.rd_data;
	} else if (cl_map->u.redirect_cluster.method == 2) {
	    rd_data = &cl_map->cnd_redir_repl.rd_data;
	} else {
	    /*
	     * we don't know what to free 
	     */
	}
	if (rd_data != NULL) {
	    if (rd_data->addr_port != NULL) {
		safe_free(rd_data->addr_port);
	    }
	    if (rd_data->baseRedirectPath != NULL) {
		safe_free(rd_data->baseRedirectPath);
	    }
	}
    } else if (cl_map->cl_type == 2) {
	/*
	 * free proxy allocations 
	 */
	if (cl_map->u.proxy_cluster.method == 1) {
	    pd_data = &cl_map->cnd_proxy.pd_data;
	} else if (cl_map->u.proxy_cluster.method == 2) {
	    pd_data = &cl_map->cnd_proxy_repl.pd_data;
	} else {
	    /*
	     * XXX- we don't know what to do here 
	     */
	}
	if (pd_data != NULL) {
	    if (pd_data->addr_port != NULL) {
		safe_free(pd_data->addr_port);
	    }
	    if (pd_data->baseRedirectPath != NULL) {
		safe_free(pd_data->baseRedirectPath);
	    }
	}
    } else {
	/*
	 * cluster type is not set 
	 */
    }
    return 0;
}

/* End of nvsd_mgmt_cluster.c */
