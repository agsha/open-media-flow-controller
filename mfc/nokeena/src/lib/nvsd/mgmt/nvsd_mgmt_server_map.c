/*
 *
 * Filename:  nkn_mgmt_server_map.c
 * Date:      2009/04/03 
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008-09 Nokeena Networks, Inc.  
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
#include "nvsd_mgmt_lib.h"

/* Local Macros */
#define	HTTP_STR	"http"
#define	NFS_STR		"nfs"

/* Local Function Prototypes */
int nvsd_server_map_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings);

extern int nvsd_mgmt_thrd_initd;

static int
nvsd_server_map_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value, void *data);

static int
nvsd_server_map_delete_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array *
	bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value,
	void *data);

int read_binary_servermap_to_buf(const char *t_server_map,
	const char *t_bin_filename);

int
nvsd_handle_smap_status_change(const char *t_server_map,
	const char *t_bin_filename,
	const char *status);

/* Extern Variables */
extern md_client_context *nvsd_mcc;

/* Local Variables */
const char server_map_config_prefix[] = "/nkn/nvsd/server-map/config";
static server_map_node_data_t *pstServerMap = NULL;
server_map_node_data_t lstServerMaps[NKN_MAX_SERVER_MAPS];
static tbool dynamic_change = false;

/* Extern Functions */
void remove_server_map_in_namespace(const char *t_server_map);

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_server_map_cfg_query()
 *	purpose : query for server_map config parameters
 */
int
nvsd_server_map_cfg_query(const bn_binding_array * bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    lc_log_basic(LOG_DEBUG,
	    "nvsd server_map module mgmtd query initializing ..");

    /*
     * Initialize the ServerMap list 
     */
    memset(lstServerMaps, 0,
	    NKN_MAX_SERVER_MAPS * sizeof (server_map_node_data_t));

    /*
     * Bind to get SERVER_MAP 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    server_map_config_prefix,
	    NULL,
	    nvsd_server_map_cfg_handle_change,
	    &rechecked_licenses);

    bail_error(err);

bail:
    return err;
}	/* end of nvsd_server_map_cfg_query() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_server_map_module_cfg_handle_change()
 *	purpose : handler for config changes for server_map module
 */
int
nvsd_server_map_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    /*
     * Reset the flag 
     */
    dynamic_change = false;

    /*
     * Lock first 
     */
    nvsd_mgmt_namespace_lock();

    /*
     * Call the new bindings callbacks 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    server_map_config_prefix,
	    NULL,
	    nvsd_server_map_cfg_handle_change,
	    &rechecked_licenses);

    bail_error(err);

    /*
     * Call the old bindings callbacks 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(old_bindings,
	    server_map_config_prefix,
	    NULL,
	    nvsd_server_map_delete_cfg_handle_change,
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

    return err;

bail:
    if (err) {
	/*
	 * Unlock on error condition and while leaving 
	 */
	nvsd_mgmt_namespace_unlock();
    }
    return err;
}	/* end of * nvsd_server_map_module_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : get_server_map_element ()
 *	purpose : get the element in the array for the given server_map, 
 *		if not found then return the first free element
 */
static server_map_node_data_t *
get_server_map_element(const char *cpServerMap)
{
    int i;
    int free_entry_index = -1;

    for (i = 0; i < NKN_MAX_SERVER_MAPS; i++) {
	if (NULL == lstServerMaps[i].name) {
	    /*
	     * Save the first free entry 
	     */
	    if (-1 == free_entry_index)
		free_entry_index = i;

	    /*
	     * Empty entry hence continue 
	     */
	    continue;
	} else if (0 == strcmp(cpServerMap, lstServerMaps[i].name)) {
	    /*
	     * Found match hence return this entry 
	     */
	    return (&(lstServerMaps[i]));
	}
    }

    /*
     * Now that we have gone thru the list and no match return the
     * free_entry_index 
     */
    if (-1 != free_entry_index)
	return (&(lstServerMaps[free_entry_index]));

    /*
     * No match and no free entry
     */
    return (NULL);
}	/* end of get_server_map_element () */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_server_map_cfg_handle_change()
 *	purpose : handler for config changes for server_map module
 */
static int
nvsd_server_map_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * bndgs_value, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    const char *t_server_map = NULL;
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
	    (ts_str(name), "/nkn/nvsd/server-map/config/**")) {

	/*-------- Get the ServerMap ------------*/
	bn_binding_get_name_parts(binding, &name_parts);
	bail_error(err);
	t_server_map = tstr_array_get_str_quick(name_parts, 4);

	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../nkn/nvsd/server-map/config as : \"%s\"",
		t_server_map);

	/*
	 * Get the server_map entry in the global array 
	 */
	pstServerMap = get_server_map_element(t_server_map);
	if (!pstServerMap)
	    goto bail;

	if (NULL == pstServerMap->name)
	    pstServerMap->name =
		nkn_strdup_type(t_server_map, mod_mgmt_charbuf);
    } else {
	/*
	 * This is not the server_map node hence leave 
	 */
	goto bail;
    }

    /*
     * Get the FILE-URL 
     */
    if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 2, "*",
		"file-url")) {
	tstring *t_file_url = NULL;
	tstring *state = NULL;
	tstring *filename = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_file_url);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../server-map/config/%s/file_url %d as : \"%s\"",
		t_server_map, nvsd_mgmt_thrd_initd,
		t_file_url ? ts_str(t_file_url) : "");

	if (NULL != pstServerMap->file_url)
	    safe_free(pstServerMap->file_url);

	if (t_file_url) {
	    pstServerMap->file_url =
		nkn_strdup_type(ts_str(t_file_url), mod_mgmt_charbuf);
	    if (!nvsd_mgmt_thrd_initd) {
		err = mdc_get_binding_tstr_fmt(nvsd_mcc, NULL, NULL, NULL,
			&state, NULL,
			"/nkn/nvsd/server-map/monitor/%s/state",
			t_server_map);
		bail_error(err);
		lc_log_basic(LOG_INFO, "Status mon node = %s", ts_str(state));

		pstServerMap->state = atoi(ts_str(state));

		err = mdc_get_binding_tstr_fmt(nvsd_mcc, NULL, NULL, NULL,
			&filename, NULL,
			"/nkn/nvsd/server-map/monitor/%s/map-bin-file",
			t_server_map);
		bail_error(err);
		lc_log_basic(LOG_INFO, "Status file node = %s",
			ts_str(filename));

		if (pstServerMap->state == 1) {
		    err =
			read_binary_servermap_to_buf(t_server_map,
				ts_str(filename));
		    bail_error(err);
		}

	    }
	} else {
	    pstServerMap->file_url = NULL;
	}

	ts_free(&t_file_url);
    }

    /*
     * Get the REFRESH-INTERVAL 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 2, "*",
		"refresh")) {
	uint32 t_refresh = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_refresh);

	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../server-map/config/%s/refresh as : \"%d\"",
		t_server_map, t_refresh);
	pstServerMap->refresh = t_refresh;
    }

    /*
     * Get the FORMAT-TYPE 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 2, "*",
		"format-type")) {
	uint32 t_format_type = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_format_type);

	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../server-map/config/%s/format-type as : \"%d\"",
		t_server_map, t_format_type);
	pstServerMap->format_type = t_format_type;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 2, "*",
		"file-binary")) {

    }
    /*
     * Get the node monitor parameters 
     */
    else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 4, "*",
		"node-monitoring",
		"heartbeat", "interval")) {
	uint32 t_heartbeat_int = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &t_heartbeat_int);

	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../server-map/config/%s/node-monitoring/heartbeat/interval as : \"%d\"",
		t_server_map, t_heartbeat_int);
	pstServerMap->monitor_values.heartbeat_interval = t_heartbeat_int;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 4, "*",
		"node-monitoring",
		"heartbeat",
		"connect-timeout")) {
	uint32 t_hb_conn_timeout = 0;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL, &t_hb_conn_timeout);

	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../server-map/config/%s/node-monitoring/heartbeat/connect-timeout as : \"%d\"",
		t_server_map, t_hb_conn_timeout);
	pstServerMap->monitor_values.connect_timeout = t_hb_conn_timeout;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 4, "*",
		"node-monitoring",
		"heartbeat",
		"allowed-fails")) {
	uint32 t_hb_allowed_fails = 0;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL, &t_hb_allowed_fails);

	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../server-map/config/%s/node-monitoring/heartbeat/allowed_fails as : \"%d\"",
		t_server_map, t_hb_allowed_fails);
	pstServerMap->monitor_values.allowed_fails = t_hb_allowed_fails;
    } else if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 4, "*",
		"node-monitoring",
		"heartbeat",
		"read-timeout")) {
	uint32 t_hb_read_timeout = 0;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL, &t_hb_read_timeout);

	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../server-map/config/%s/node-monitoring/heartbeat/read_timeout as : \"%d\"",
		t_server_map, t_hb_read_timeout);
	pstServerMap->monitor_values.read_timeout = t_hb_read_timeout;
    }

    /*
     * Set the flag so that we know this is a dynamic change 
     */
    dynamic_change = true;

bail:
    tstr_array_free(&name_parts);
    return err;
}	/* end of nvsd_server_map_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_server_map_delete_cfg_handle_change()
 *	purpose : handler for config delete changes for server_map module
 */

static int
nvsd_server_map_delete_cfg_handle_change(const bn_binding_array * arr,
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
    const char *t_server_map = NULL;
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
	    (ts_str(name), "/nkn/nvsd/server-map/config/**")) {

	/*-------- Get the ServerMap ------------*/
	bn_binding_get_name_parts(binding, &name_parts);
	bail_error(err);
	t_server_map = tstr_array_get_str_quick(name_parts, 4);

	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../nkn/nvsd/server-map/config as : \"%s\"",
		t_server_map);

	/*
	 * Delete this only if the wildcard change is received 
	 */
	if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 1, "*")) {
	    /*
	     * Get the server_map entry in the global array 
	     */
	    pstServerMap = get_server_map_element(t_server_map);
	    if (pstServerMap && (NULL != pstServerMap->name)) {
		/*
		 * Free the allocated name 
		 */
		free(pstServerMap->name);

		/*
		 * Free the other allocated fields 
		 */
		if (pstServerMap->binary_server_map)
		    safe_free(pstServerMap->binary_server_map);
		if (pstServerMap->file_url)
		    safe_free(pstServerMap->file_url);

		memset(pstServerMap, 0, sizeof (server_map_node_data_t));

		/*
		 * Now search thr Namespace list and remove this entry 
		 */
		remove_server_map_in_namespace(t_server_map);
	    }

	    /*
	     * Set the flag so that we know this is a dynamic change 
	     */
	    dynamic_change = true;

	}
    } else {
	/*
	 * This is not the server_map node hence leave 
	 */
	goto bail;
    }

bail:
    tstr_array_free(&name_parts);
    return err;
}	/* end of * nvsd_service_delete_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_mgmt_get_server_map()
 *	purpose : get the matching server map structure
 */
server_map_node_data_t *
nvsd_mgmt_get_server_map(char *cpServerMap)
{
    int i;

    /*
     * Sanity Check 
     */
    if (NULL == cpServerMap)
	return (NULL);

    lc_log_basic(LOG_NOTICE, "Request received for %s server_map", cpServerMap);

    /*
     * Now find the matching Virtual Player entry 
     */
    for (i = 0; i < NKN_MAX_SERVER_MAPS; i++) {
	if ((NULL != lstServerMaps[i].name) &&
		(0 == strcmp(cpServerMap, lstServerMaps[i].name)))
	    /*
	     * Return this entry 
	     */
	    return (&(lstServerMaps[i]));
    }

    /*
     * No Match 
     */
    return (NULL);
}   /* end of nvsd_mgmt_get_server_map() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : read_binary_servermap_to_buf()
 *	purpose : read the server map binary file into a an allocated buffer 
 *		to be passed to the inline origin manager logic
 */
int
read_binary_servermap_to_buf(const char *t_server_map,
	const char *t_bin_filename)
{
    int fd;
    int err = 0;
    int ns_idx = 0;
    int file_size = 0;
    char *cp_buf = NULL;
    struct stat st;
    namespace_node_data_t *t_pstNamespace = NULL;

    (void) ns_idx;
    (void) t_pstNamespace;

    /*
     * Sanity test 
     */
    if (!t_server_map || !t_bin_filename) {
	err = 1;
	goto bail;
    }

    /*
     * Now that the file is read, update the server-map datastructure 
     */
    pstServerMap = get_server_map_element(t_server_map);
    if (!pstServerMap) {
	/*
	 * Not good, in any case bail out 
	 */
	err = 1;
	goto bail;
    }

    /*
     * Now assign the name to the global data structure 
     */
    if (NULL == pstServerMap->name)
	pstServerMap->name = nkn_strdup_type(t_server_map, mod_mgmt_charbuf);

    /*
     * First get the size of the binary file 
     */
    err = stat(t_bin_filename, &st);

    /*
     * check if stat failed 
     */
    if (err) {
	/*
	 * Not able to stat means cannot read 
	 */
	/*
	 * Hence, reset global and bail out 
	 */

	/*
	 * Check to see if we need to free 
	 */
	if (pstServerMap->binary_server_map)
	    safe_free(pstServerMap->binary_server_map);
	pstServerMap->binary_server_map = NULL;
	pstServerMap->binary_size = 0;

	goto bail;
    }

    /*
     * Get the file size 
     */
    file_size = st.st_size;

    /*
     * Now fix the size to be a multiple of 512 so as to have it mem aligned 
     * and there by use DIRECT_IO 
     */
    if (file_size < 512)
	file_size = 512;
    else
	file_size = 512 * ((file_size / 512) + 1);

    /*
     * Allocate the memory needed to read the file 
     */
    err = nkn_posix_memalign_type((void **) &cp_buf,
	    (512), file_size, mod_mgmt_posix_memalign);
    if (err) {
	goto bail;
    }

    /*
     * Open the file and read the data 
     */
    fd = open(t_bin_filename, O_RDONLY | O_DIRECT);
    if (fd == -1) {
	err = 1;
	goto bail;
    }

    read(fd, cp_buf, file_size);
    close(fd);

    /*
     * Check to see if we need to free 
     */
    if (pstServerMap->binary_server_map)
	safe_free(pstServerMap->binary_server_map);

    /*
     * Now assign the buffer and set the size in the global 
     */
    pstServerMap->binary_server_map = cp_buf;
    pstServerMap->binary_size = file_size;

    /*
     * Now that the binary server-map has changed alert backend 
     */
    nkn_namespace_config_update(NULL);
bail:
    return (err);
}	/* end of read_binary_servermap_to_buf */

/* ------------------------------------------------------------------------- */

int
nvsd_handle_smap_status_change(const char *t_server_map,
	const char *t_bin_filename, const char *status)
{
    int err = 0;

    lc_log_basic(LOG_INFO, "Got smap event %s %s %s",
	    t_server_map, t_bin_filename, status);

    pstServerMap = get_server_map_element(t_server_map);
    if (!pstServerMap) {
	/*
	 * Not good, in any case bail out 
	 */
	err = 1;
	goto bail;
    }

    /*
     * Now assign the name to the global data structure 
     */
    if (NULL == pstServerMap->name)
	pstServerMap->name = nkn_strdup_type(t_server_map, mod_mgmt_charbuf);

    pstServerMap->state = atoi(status);

    if (strcmp(status, "1") == 0) {
	err = read_binary_servermap_to_buf(t_server_map, t_bin_filename);
	bail_error(err);
    } else {
	nkn_namespace_config_update(NULL);
    }

bail:
    return (err);
}
