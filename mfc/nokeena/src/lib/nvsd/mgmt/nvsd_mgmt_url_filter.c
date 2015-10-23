#include <fcntl.h>
#include <sys/mman.h>
/* nvsd Headers */
#include "nkn_defs.h"
#include "nkn_debug.h"
#include "nvsd_mgmt.h"
#include "nkn_namespace.h"
#include "nvsd_mgmt_namespace.h"
#include "nkn_mgmt_defs.h"
#include "nvsd_mgmt_lib.h"
#include "nvsd_mgmt_inline.h"

#ifdef PRELOAD_UF_DATA
#include "nkn_namespace_utils.h"
#endif


/* Extern Variables */
extern unsigned int jnpr_log_level;
extern md_client_context *nvsd_mcc;
extern int search_namespace(const char *ns, int *return_idx);

static tbool	dynamic_change = false ;
int uf_config_changed = 0;
static namespace_node_data_t	*pstNamespace = NULL;
url_filter_data_t list_filter_map[NKN_MAX_FILTER_MAPS] ;
static url_filter_data_t *pstUrlFilterMap = NULL;
const char url_filter_map_config_prefix[] = "/nkn/nvsd/url-filter/config";


void nvsd_mgmt_uf_update_ns_cfg(void);
int
nvsd_mgmt_uf_update_ns(url_filter_data_t *uf_data);
int
parse_url(const char *url, char **ret_hostname, char **ret_file);

static url_filter_data_t *
get_url_filter_element(const char *filter_name);
url_filter_data_t*
nvsd_mgmt_get_url_map(const char *filter_name);
static int
nvsd_url_filter_cfg_handle_change(const bn_binding_array *arr, uint32 idx,
	const bn_binding *binding,
	const tstring *bndgs_name,
	const tstr_array *bndgs_name_components,
	const tstring *bndgs_name_last_part,
	const tstring *bndgs_value,
	void *data);


static int
nvsd_url_filter_delete_handle_change(const bn_binding_array *arr, uint32 idx,
	const bn_binding *binding,
	const tstring *bndgs_name,
	const tstr_array *bndgs_name_components,
	const tstring *bndgs_name_last_part,
	const tstring *bndgs_value,
	void *data);


    int
nvsd_url_filter_cfg_query(const bn_binding_array *bindings)
{
    int err = 0;

    /* Initialize the filter-map list */
    memset (list_filter_map, 0, NKN_MAX_FILTER_MAPS * sizeof (url_filter_data_t));

    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    url_filter_map_config_prefix, NULL,
	    nvsd_url_filter_cfg_handle_change, NULL);
    bail_error(err);


bail:
    return err;
}
    static int
nvsd_url_filter_cfg_handle_change(const bn_binding_array *arr, uint32 idx,
	const bn_binding *binding,
	const tstring *bndgs_name,
	const tstr_array *bndgs_name_components,
	const tstring *bndgs_name_last_part,
	const tstring *bndgs_value,
	void *data)
{
    int err = 0;
    const tstring *name = NULL;
    const char *t_url_filter_map = NULL;
    tstr_array *name_parts = NULL;

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /* Check if this is our node */
    if (bn_binding_name_pattern_match (ts_str(name), "/nkn/nvsd/url-filter/config/id/**"))
    {
	uint32 ret_flags = 0;
	err = bn_binding_get_type(binding, ba_value, NULL, &ret_flags);
	bail_error(err);

	if (ret_flags & btf_deleted)  {
	    /* binding has been deleted, old_bindings would handle this */
	    goto bail;
	}
	uf_config_changed++;
	/*-------- Get the ServerMap ------------*/
	bn_binding_get_name_parts (binding, &name_parts);
	bail_error(err);
	t_url_filter_map= tstr_array_get_str_quick (name_parts, 5);

	bail_error(err);
	lc_log_basic(LOG_DEBUG, "Read .../nkn/nvsd/server-map/config as : \"%s\"",
		t_url_filter_map);

	/* Get the server_map entry in the global array */
	pstUrlFilterMap= get_url_filter_element(t_url_filter_map);
	if (!pstUrlFilterMap)
	    goto bail;

	if (NULL == pstUrlFilterMap->uf_map_name)
	    pstUrlFilterMap->uf_map_name = nkn_strdup_type (t_url_filter_map, mod_mgmt_charbuf);
    } else {
	/* This is not the server_map node hence leave */
	goto bail;
    }
    if (bn_binding_name_parts_pattern_match_va(name_parts, 5, 2,"*",
		"type")) {
	char *filter_type= NULL;
	err = bn_binding_get_str(binding,
		ba_value, bt_string, NULL,
		&filter_type);
	bail_error(err);

	lc_log_basic(LOG_NOTICE, "filter-map :%s, Type: %s", pstUrlFilterMap->uf_map_name, filter_type);

	if(filter_type ) {
	    if (0 == strcmp(filter_type, "url")) {
		pstUrlFilterMap->uf_type = UF_URL;
	    } else if (0 == strcmp(filter_type, "iwf")) {
		pstUrlFilterMap->uf_type = UF_IWF;
	    } else if (0 == strcmp(filter_type, "calea")) {
		pstUrlFilterMap->uf_type = UF_CALEA;
	    } else {
		pstUrlFilterMap->uf_type = UF_UNDEF;
	    }
	    safe_free(filter_type);
	}
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 5, 2,"*",
		"file-url")) {
	char *file_url= NULL, *ret_file = NULL;
	char *file_path = NULL;

	safe_free(pstUrlFilterMap->uf_local_file);
#ifdef PRELOAD_UF_DATA
	delete_url_filter_trie(pstUrlFilterMap->uf_trie_data);
	pstUrlFilterMap->uf_trie_data = 0;
#endif

	err = bn_binding_get_str(binding,
		ba_value, bt_string, NULL,
		&file_url);
	bail_error(err);

	if (file_url) {
	    safe_free(file_url);
	    node_name_t node_name = {0};
	    bn_binding *fpath_binding = NULL;

	    snprintf(node_name, sizeof(node_name),
		    "/nkn/nvsd/url-filter/mon/id/%s/file-path",
		    pstUrlFilterMap->uf_map_name);

	    err = mdc_get_binding(nvsd_mcc, NULL, NULL, false, &fpath_binding, node_name,
		    NULL);
	    bail_error(err);

	    bn_binding_get_str(fpath_binding, ba_value,0 , NULL, &file_path);
	    lc_log_basic(LOG_NOTICE, "file-name : %s", file_path?: "FILE-PATH-NULL");
	    DBG_LOG(MSG, MOD_URL_FILTER, "uf-name: %s, file-name : %s",
		    pstUrlFilterMap->uf_map_name, file_path?: "FILE-PATH-NULL");

	    if (file_path && strcmp(file_path, "")) {
		pstUrlFilterMap->uf_local_file = nkn_strdup_type(file_path,
			mod_mgmt_charbuf);
		lc_log_basic(LOG_NOTICE, "local_file: %s for %s",
			pstUrlFilterMap->uf_local_file, pstUrlFilterMap->uf_map_name);
#ifdef PRELOAD_UF_DATA
	    	pstUrlFilterMap->uf_trie_data = 
		    new_url_filter_trie(pstUrlFilterMap->uf_local_file,
				    	pstUrlFilterMap->ns_name ? 
					pstUrlFilterMap->ns_name : "NO-NS", 
					&err);
		if (err) {
		    lc_log_basic(LOG_NOTICE, "new_url_filter_trie() failed, "
			     "rv=%d ns %s, file: %s", 
			     err,
			     pstUrlFilterMap->ns_name ? 
			     	pstUrlFilterMap->ns_name : "NO-NS",
			     pstUrlFilterMap->uf_local_file);
		    err = 0;
		}
#endif
	    } else {
		safe_free(pstUrlFilterMap->uf_local_file);
#ifdef PRELOAD_UF_DATA
		delete_url_filter_trie(pstUrlFilterMap->uf_trie_data);
		pstUrlFilterMap->uf_trie_data = 0;
#endif
	    }
	}
    }

bail:
    tstr_array_free(&name_parts);
    return err;
}

static url_filter_data_t *
get_url_filter_element(const char *filter_name)
{
    int	i;
    int	free_entry_index = -1;

    for (i = 0; i < NKN_MAX_FILTER_MAPS; i++)
    {
	if (NULL ==  list_filter_map[i].uf_map_name)
	{
	    /* Save the first free entry */
	    if (-1 == free_entry_index)
		free_entry_index = i;

	    /* Empty entry hence continue */
	    continue;
	}
	else if (0 == strcmp (filter_name, list_filter_map[i].uf_map_name))
	{
	    /* Found match hence return this entry */
	    return (&(list_filter_map[i]));
	}
    }

    /* Now that we have gone thru the list and no match 
     * return the free_entry_index */
    if (-1 != free_entry_index)
	return (&(list_filter_map[free_entry_index]));

    /* No match and no free entry */
    return (NULL);

} /* end of get_server_map_element () */


int
nvsd_mgmt_uf_update_ns(url_filter_data_t *uf_data)
{
    int ns_idx = -1;

    if (uf_data == NULL) {
	goto bail;
    }
    if (NULL != uf_data->ns_name) {
	/* find namespace url-filter */
	if (search_namespace(uf_data->ns_name, &ns_idx) == 0) {

	    pstNamespace = get_namespace(ns_idx);
	    if (NULL == pstNamespace) {
		goto bail;
	    }

	    pstNamespace->url_filter.uf_type = uf_data->uf_type;
	    safe_free(pstNamespace->url_filter.uf_local_file);
#ifdef PRELOAD_UF_DATA
	    delete_url_filter_trie(pstNamespace->url_filter.uf_trie_data);
	    pstNamespace->url_filter.uf_trie_data = 0;
#endif

	    if (uf_data->uf_local_file) {
		pstNamespace->url_filter.uf_local_file
		    = nkn_strdup_type(uf_data->uf_local_file, mod_mgmt_charbuf);
#ifdef PRELOAD_UF_DATA
		pstNamespace->url_filter.uf_trie_data = 
		    dup_url_filter_trie(uf_data->uf_trie_data);
#endif
	    }
	    lc_log_basic(LOG_NOTICE, "name: %s, type: %d,  local-file: %s",pstNamespace->namespace,
		    pstNamespace->url_filter.uf_type, pstNamespace->url_filter.uf_local_file?:"NO-UF-MAP");
	    DBG_LOG(MSG, MOD_URL_FILTER, "name: %s, type: %d,  local-file: %s",
		    pstNamespace->namespace, pstNamespace->url_filter.uf_type,
		    pstNamespace->url_filter.uf_local_file?:"NO-UF-MAP");
	}
    }

bail:
    return 0;
}
void
nvsd_mgmt_uf_update_ns_cfg(void)
{
    int	i , ns_idx = -1;
    url_filter_data_t *url_filter_data = NULL;

    /* Now find the matching Virtual Player entry */
    for (i = 0; i < NKN_MAX_FILTER_MAPS; i++)
    {
	if (NULL != list_filter_map [i].uf_map_name) {
	    nvsd_mgmt_uf_update_ns(list_filter_map);
	}
    }

    return;
} /* end of nvsd_mgmt_get_server_map() */

url_filter_data_t*
nvsd_mgmt_get_url_map(const char *filter_name)
{
    int	i;

    /* Sanity Check */
    if (NULL == filter_name) return (NULL);

    lc_log_basic(LOG_NOTICE, "Request received for %s server_map", filter_name);

    /* Now find the matching Virtual Player entry */
    for (i = 0; i < NKN_MAX_FILTER_MAPS; i++)
    {
	if ((NULL != list_filter_map [i].uf_map_name) &&
		(0 == strcmp (filter_name, list_filter_map [i].uf_map_name)))
	    /* Return this entry */
	    return (&(list_filter_map [i]));
    }

    /* No Match */
    return (NULL);

} /* end of nvsd_mgmt_get_server_map() */
    int
nvsd_url_filter_map_cfg_handle_change(bn_binding_array *old_bindings,
	bn_binding_array *new_bindings)
{
    int err = 0;

    /* Reset the flag */
    dynamic_change = false;

    uf_config_changed = 0;
    /* Call the old bindings callbacks */
    err = mgmt_lib_mdc_foreach_binding_prequeried(old_bindings,
	    url_filter_map_config_prefix, NULL,
	    nvsd_url_filter_delete_handle_change,NULL);

    bail_error(err);

    /* Call the new bindings callbacks */
    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    url_filter_map_config_prefix, NULL,
	    nvsd_url_filter_cfg_handle_change, NULL);
    bail_error(err);

    if (uf_config_changed) {
	/*
	 * iterate over all filter-maps and check for namespace association,
	 * if associated, then copy the filter-map details to namespace
	 */
	nvsd_mgmt_namespace_lock();
	nvsd_mgmt_uf_update_ns_cfg();
	nvsd_mgmt_namespace_unlock();
    }


    /* If relevant node has changed then indicate to the module */
    if (dynamic_change)
	nkn_namespace_config_update (NULL);

    return err;

bail:
    if(err){
	/* Unlock  on error condition and while leaving */
    }
    return err;

} /* end of nvsd_server_map_module_cfg_handle_change() */

    static int
nvsd_url_filter_delete_handle_change(const bn_binding_array *arr, uint32 idx,
	const bn_binding *binding,
	const tstring *bndgs_name,
	const tstr_array *bndgs_name_components,
	const tstring *bndgs_name_last_part,
	const tstring *bndgs_value,
	void *data)
{
    int err = 0;
    const tstring *name = NULL;
    const char *t_url_filter_map = NULL;
    tstr_array *name_parts = NULL;

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /* Check if this is our node */
    if (bn_binding_name_pattern_match (ts_str(name), "/nkn/nvsd/url-filter/config/id/**"))
    {
	/*-------- Get the ServerMap ------------*/
	bn_binding_get_name_parts (binding, &name_parts);
	bail_error(err);
	t_url_filter_map= tstr_array_get_str_quick (name_parts, 5);

	bail_error(err);
	lc_log_basic(LOG_DEBUG, "Read .../nkn/nvsd/server-map/config as : \"%s\"",
		t_url_filter_map);

	uf_config_changed++;
	if (bn_binding_name_parts_pattern_match_va(name_parts, 5, 1,  "*"))
	{
	    /* Get the server_map entry in the global array */
	    pstUrlFilterMap = get_url_filter_element(t_url_filter_map);
	    if (pstUrlFilterMap && (NULL != pstUrlFilterMap->uf_map_name)) {
		/* Free the allocated name */
		safe_free(pstUrlFilterMap->uf_map_name);
		safe_free(pstUrlFilterMap->uf_local_file);
#ifdef PRELOAD_UF_DATA
	    	delete_url_filter_trie(pstUrlFilterMap->uf_trie_data);
	    	pstUrlFilterMap->uf_trie_data = 0;
#endif
		safe_free(pstUrlFilterMap->ns_name);
		memset (pstUrlFilterMap, 0, sizeof (url_filter_data_t));
		dynamic_change = true;
	    }
	} else if (bn_binding_name_parts_pattern_match_va(name_parts, 5, 2,"*",
		    "file-url")) {
	    dynamic_change = true;
	    pstUrlFilterMap = get_url_filter_element(t_url_filter_map);
	    safe_free (pstUrlFilterMap->uf_local_file);
#ifdef PRELOAD_UF_DATA
	    delete_url_filter_trie(pstUrlFilterMap->uf_trie_data);
	    pstUrlFilterMap->uf_trie_data = 0;
#endif
	}
    }

bail:
    tstr_array_free(&name_parts);
    return err;
}

int
nvsd_url_filter_update_map(const bn_binding_array *bindings)
{
    int err =0 ;

    tstring *url_map_name, *file_name, *status, *uf_error = NULL;
    url_filter_data_t *uf_map = NULL;

    err = bn_binding_array_get_value_tstr_by_name
	(bindings, "map_name", NULL, &url_map_name);
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name
	(bindings, "status", NULL, &status);
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name
	(bindings, "file_name", NULL, &file_name);
    bail_error(err);

    err = bn_binding_array_get_value_tstr_by_name
	(bindings, "error", NULL, &uf_error);
    bail_error(err);

    lc_log_debug(LOG_NOTICE,"Recieved URL map status change event for %s,"
	    "new-status: %s, file: %s, ERROR: %s", ts_str(url_map_name), ts_str(status),
	    ts_str(file_name), ts_str(uf_error));

    DBG_LOG(MSG, MOD_URL_FILTER, "Status Change for %s: status: %s, file: %s,"
	    "ERROR: %s", ts_str(url_map_name), ts_str(status),
	    ts_str(file_name), ts_str(uf_error));

    uf_map = nvsd_mgmt_get_url_map(ts_str(url_map_name));
    if (uf_map) {
	/* update the status & file-name */
	uf_map->state = atoi(ts_str(status));
	safe_free(uf_map->uf_local_file);
#ifdef PRELOAD_UF_DATA
	delete_url_filter_trie(uf_map->uf_trie_data);
	uf_map->uf_trie_data = 0;
#endif
	if (file_name && strcmp(ts_str(file_name), "")) {
	    uf_map->uf_local_file = nkn_strdup_type(ts_str(file_name),
		    mod_mgmt_charbuf);
#ifdef PRELOAD_UF_DATA
	    uf_map->uf_trie_data = 
	    	new_url_filter_trie(uf_map->uf_local_file, 
				    uf_map->ns_name ? uf_map->ns_name : "NO-NS",
				    &err);
	    if (err) {
		lc_log_basic(LOG_NOTICE, "new_url_filter_trie() failed, rv=%d "
			     "ns %s, file: %s", 
			     err,
			     uf_map->ns_name ? uf_map->ns_name : "NO-NS",
			     uf_map->uf_local_file);
	    	err = 0;
	    }
#endif
	}
	nvsd_mgmt_namespace_lock();
	nvsd_mgmt_uf_update_ns(uf_map);
	nvsd_mgmt_namespace_unlock();

	nkn_namespace_config_update (NULL);
	lc_log_basic(LOG_NOTICE, "ns %s, file: %s", uf_map->ns_name?:"NO-NS", uf_map->uf_local_file?:"NO-FILE");
    } else {
	lc_log_basic(LOG_WARNING, "update event received for %s filter-map"
		" but it is not configured", ts_str(url_map_name));
	goto bail;
    }
bail:

    ts_free(&url_map_name);
    ts_free(&status);
    ts_free(&file_name);
    ts_free(&uf_error);
    return err;
}

int
parse_url(const char *url, char **ret_hostname, char **ret_file)
{
    int err = 0;
    const char *cl = NULL, *nl = NULL;

    if (ret_hostname) {
	*ret_hostname = NULL;
    }
    if (*ret_file) {
	*ret_file = NULL;
    }
    bail_null(url);

    if (!lc_is_prefix("http://", url, false)) {
	goto bail;
    }

    cl = url + strlen("http://");
    if (!*cl) {
	err = lc_err_bad_path;
	goto bail;
    }

    nl = rindex(cl, '/');
    if (!nl) {
	err = lc_err_bad_path;
	goto bail;
    }

    if (ret_hostname) {
	//*ret_hostname = strldup(cl, nl - cl + 1);
    }
    if (ret_file) {
	*ret_file =nkn_strdup_type(nl+1, mod_mgmt_charbuf);
    }
bail:
    return (err);
}
