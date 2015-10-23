/*
 *
 * Filename:  nkn_mgmt_resource_mgr.c
 * Date:      2010/09/23
 * Author:    Thilak Raj S
 *
 * (C) Copyright 2009-10 Juniper  Networks, Inc.
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
#include "net/if.h"
#include "interface.h"
#include "sys/ioctl.h"

/* Samara Headers */
#include "md_client.h"
#include "license.h"

/* nvsd Headers */
#include "nkn_defs.h"
#include "nvsd_mgmt.h"
#include "nkn_namespace.h"
#include "nvsd_mgmt_namespace.h"
#include "nkn_stat.h"
#include "libnknmgmt.h"
#include "nkn_assert.h"
#include "nkn_debug.h"
#include "nkn_mgmt_defs.h"
#include "nvsd_mgmt_lib.h"

/* Local Macros */
#define	HTTP_STR	"http"
#define	NFS_STR		"nfs"
#define CNTR_MAX_STR	64

extern unsigned int jnpr_log_level;
uint8_t refetech_global_pool = 1;
uint8_t refetch_global_pool_conx = 1;
uint64_t global_pool_bandwidth = 0;

/* resource pool header */
#include "nvsd_resource_mgr.h"

#define CHECK_FOR_BAD_RP_IDX(rp_index)                                      \
    do {									    \
	if(rp_index >= NKN_MAX_RESOURCE_POOL || (!(g_lstresrcpool.lstResourcePool[rp_index].name))) {			    \
	    glob_err_rc_resource_idx ++;					    \
	    DBG_LOG(SEVERE, MOD_RESRC_POOL, "BAD resource pool index");         \
	    return 0;							    \
	}									    \
    } while(0)

NKNCNT_DEF(err_rc_over_req, uint64_t, "", "over requrest rc session")
NKNCNT_DEF(err_rc_free_less_zero, uint64_t, "", "free less than 0 rc session")
NKNCNT_DEF(err_rc_resource_type, uint64_t, "", "err resource type")
NKNCNT_DEF(err_rc_resource_type_range, uint64_t, "", "err rc range")
NKNCNT_DEF(err_rc_resource_idx, uint64_t, "", "err rc bad index")

/* Local Function Prototypes */
int get_total_bw(md_client_context * mcc, uint64_t * total_actual_bw);
int get_total_network_conn(md_client_context * mcc, uint64_t * max_conn);
int nvsd_rsrc_update_event(const bn_binding_array * arr,
	uint32 idx, bn_binding * binding, void *data);

int nvsd_resource_mgr_module_cfg_handle_change(bn_binding_array *
	old_bindings,
	bn_binding_array *
	new_bindings);

static int nvsd_resource_mgr_cfg_handle_change(const bn_binding_array *
	arr, uint32 idx,
	const bn_binding * binding,
	const tstring * name,
	const tstr_array *
	name_components,
	const tstring *
	name_last_part,
	const tstring * value,
	void *data);

static int nvsd_resource_mgr_delete_cfg_handle_change(const
	bn_binding_array *
	arr, uint32 idx,
	const bn_binding *
	binding,
	const tstring *
	bn_name,
	const tstr_array *
	name_components,
	const tstring *
	name_last_part,
	const tstring *
	value, void *data);

static resource_pool_t *get_resource_mgr_element(const char
	*cpResourcePool);

int nvsd_rp_adjust_bw(void
	);

static int nvsd_rp_add_delete_nkn_cnt(resource_pool_t * pstResourcePool,
	tbool add);
int get_global_pool(md_client_context * mcc,
	const char *rsrc, uint64_t * global_pool);

int
search_ressource_mgr(const char *cpResourcePool, int *ret_idx);

static inline resource_pool_t *get_rp(int index);

/* Extern Variables */
extern md_client_context *nvsd_mcc;

/* Local Variables */
const char resource_mgr_config_prefix[] = "/nkn/nvsd/resource_mgr/config";
struct resrcpool_list {
    int g_nresrc_pool;
    resource_pool_t lstResourcePool[NKN_MAX_RESOURCE_POOL];
};

static struct resrcpool_list g_lstresrcpool;
static tbool dynamic_change = false;

/* Extern variables */
extern long sess_bandwidth_limit;

/* Extern Functions */
void remove_resource_mgr_in_namespace(const char *t_resource_mgr);

resource_pool_t *p_global_resource_pool = NULL;

typedef struct max_system_resource_st {
    /*
     * sys max containing the max value given through 
     */
    uint64_t rsrc_sys_max[NKN_MAX_RESOURCES];
    /*
     * set_total also adjusted values after total
     * Has the value of cur used(except gloabl pool), which is
     * after mgmt starts up
     */
    uint64_t rsrc_cur_used[NKN_MAX_RESOURCES];
} max_system_resource_t;

static max_system_resource_t global_resource_max;

uint64_t g_rsrc_bw_1sec_val[NKN_MAX_RESOURCE_POOL];

/* ------------------------------------------------------------------------- */

/*
 *	function : nvsd_resource_mgr_cfg_query()
 *	purpose : query for resource_mgr config parameters
 */
int
nvsd_resource_mgr_cfg_query(const bn_binding_array * bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;
    uint64_t sys_conf_val = 0, glob_conf_val = 0, glob_conf_net_conx = 0;

    lc_log_basic(LOG_DEBUG,
	    "nvsd resource_mgr module mgmtd query initializing ..");

    /*
     * Initialize the Resource Pool list 
     */
    memset(g_lstresrcpool.lstResourcePool, 0,
	    NKN_MAX_RESOURCE_POOL * sizeof (resource_pool_t));

    /*
     * Init the global Resource Pool Here 
     */
    p_global_resource_pool = &g_lstresrcpool.lstResourcePool[0];

    /*
     * Set the Global pool here 
     */
    p_global_resource_pool->name =
	nkn_strdup_type(GLOBAL_RSRC_POOL, mod_mgmt_charbuf);
    g_lstresrcpool.g_nresrc_pool = 1;

    /*
     * Add counters for global resource pool 
     */
    nvsd_rp_add_delete_nkn_cnt(p_global_resource_pool, true);

    /*
     * get actual system bandwidth 
     */
    err = get_total_bw(nvsd_mcc, &sys_conf_val);
    bail_error(err);

    global_resource_max.rsrc_sys_max[RESOURCE_POOL_TYPE_BW - 1] = sys_conf_val;

    /*
     * get actual network connection 
     */
    err = get_total_network_conn(nvsd_mcc, &sys_conf_val);
    bail_error(err);

    global_resource_max.rsrc_sys_max[RESOURCE_POOL_TYPE_CLIENT_SESSION - 1] =
	sys_conf_val;

    glob_conf_val = 0;
    err = get_global_pool(nvsd_mcc, "bandwidth", &glob_conf_val);
    bail_error(err);

    glob_conf_net_conx = 0;
    err = get_global_pool(nvsd_mcc, "net_conn", &glob_conf_net_conx);
    bail_error(err);

    lc_log_debug(jnpr_log_level, "got sys bw=> %lu, global-rp bw=> %lu",
	    sys_conf_val, glob_conf_val);
    if (p_global_resource_pool) {
	AO_store(&p_global_resource_pool->resources[RESOURCE_POOL_TYPE_BW - 1].
		max, glob_conf_val);
	AO_store(&p_global_resource_pool->
		resources[RESOURCE_POOL_TYPE_CLIENT_SESSION - 1].max,
		glob_conf_net_conx);
	global_pool_bandwidth = glob_conf_val;
	refetech_global_pool = 0;
	refetch_global_pool_conx = 0;
    }

    /*
     * Bind to get Resource Pool list 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    resource_mgr_config_prefix,
	    NULL,
	    nvsd_resource_mgr_cfg_handle_change,
	    &rechecked_licenses);
    bail_error(err);

bail:
    refetech_global_pool = 1;
    refetch_global_pool_conx = 1;
    return err;
}								/* end of nvsd_resource_mgr_cfg_query() */

/* ------------------------------------------------------------------------- */

/*
 *	function : nvsd_resource_mgr_module_cfg_handle_change()
 *	purpose : handler for config changes for resource_mgr module
 */
int
nvsd_resource_mgr_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    /*
     * Reset the flag 
     */
    dynamic_change = false;

    /*
     * NOTE - no need to have lock, as it doesn't change any namespace config 
     */
    /*
     * Call the new bindings callbacks 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    resource_mgr_config_prefix,
	    NULL,
	    nvsd_resource_mgr_cfg_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    /*
     * Call the old bindings callbacks 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(old_bindings,
	    resource_mgr_config_prefix,
	    NULL,
	    nvsd_resource_mgr_delete_cfg_handle_change,
	    &rechecked_licenses);
    bail_error(err);

    /*
     * If relevant node has changed then indicate to the module 
     */
    if (dynamic_change)
	nkn_namespace_config_update(NULL);

bail:
    refetech_global_pool = 1;
    refetch_global_pool_conx = 1;
    return err;
}	/* end of * nvsd_resource_mgr_module_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	function : get_resource_mgr_element ()
 *	purpose : get the element in the array for the given resource_mgr,
 *		if not found then return the first free element
 */
static resource_pool_t *
get_resource_mgr_element(const char *cpResourcePool)
{
    int i;
    int free_entry_index = -1;

    for (i = 0; i < NKN_MAX_RESOURCE_POOL; i++) {
	if (NULL == g_lstresrcpool.lstResourcePool[i].name) {
	    /*
	     * Save the first free entry 
	     */
	    if (-1 == free_entry_index)
		free_entry_index = i;

	    /*
	     * Empty entry hence continue 
	     */
	    continue;
	} else if (0 ==
		strcmp(cpResourcePool,
		    g_lstresrcpool.lstResourcePool[i].name)) {
	    /*
	     * Found match hence return this entry 
	     */
	    return (&(g_lstresrcpool.lstResourcePool[i]));
	}
    }

    /*
     * Now that we have gone thru the list and no match return the
     * free_entry_index Fill the index here,This would be used in
     * nvsd_mgmt_monitor for bw 
     */
    if (-1 != free_entry_index) {
	g_lstresrcpool.g_nresrc_pool++;
	g_lstresrcpool.lstResourcePool[free_entry_index].index =
	    free_entry_index;
	return (&(g_lstresrcpool.lstResourcePool[free_entry_index]));
    }

    /*
     * No match and no free entry 
     */
    return (NULL);
}	/* end of get_resource_mgr_element () */

/* ------------------------------------------------------------------------- */

/*
 *	function : nvsd_resource_mgr_cfg_handle_change()
 *	purpose : handler for config changes for resource_mgr module
 */

static int
nvsd_resource_mgr_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bn_name,
	const tstr_array * name_components,
	const tstring * name_last_part,
	const tstring * value, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    const char *t_resource_pool = NULL;
    const char *t_resource_type = NULL;
    tstr_array *name_parts = NULL;
    resource_pool_t *pstResourcePool = NULL;
    tbool *rechecked_licenses_p = data;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);

    UNUSED_ARGUMENT(bn_name);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(name_last_part);
    UNUSED_ARGUMENT(value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /*
     * Check if this is our node 
     */
    if (bn_binding_name_pattern_match(ts_str(name),
		"/nkn/nvsd/resource_mgr/config/**")) {

	/*-------- Get the Resource Pool------------*/
	bn_binding_get_name_parts(binding, &name_parts);
	bail_error(err);
	t_resource_pool = tstr_array_get_str_quick(name_parts, 4);

	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../nkn/nvsd/resource_mgr/config as : \"%s\"",
		t_resource_pool);

	/*
	 * Get the resource_mgr entry in the global array 
	 */
	pstResourcePool = get_resource_mgr_element(t_resource_pool);
	if (!pstResourcePool)
	    goto bail;

	if (NULL == pstResourcePool->name) {
	    pstResourcePool->name =
		nkn_strdup_type(t_resource_pool, mod_mgmt_charbuf);
	    /*
	     * Add counters whenever its a new resource pool. 
	     */
	    nvsd_rp_add_delete_nkn_cnt(pstResourcePool, true);
	}
    } else {
	/*
	 * This is not the resource_mgr node hence leave 
	 */
	goto bail;
    }

    /*
     * Get the Parent 
     */
    if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 2,
		"*", "parent")) {
	tstring *t_pool_parent = NULL;
	resource_pool_t *pstParentResourcePool = NULL;

	err = bn_binding_get_tstr(binding,
		ba_value, bt_string, NULL, &t_pool_parent);
	bail_error(err);
	lc_log_basic(LOG_DEBUG, "Read ... config/%s/parent as : \"%s\"",
		t_resource_pool,
		t_pool_parent ? ts_str(t_pool_parent) : "");

	if (t_pool_parent) {
	    pstParentResourcePool =
		get_resource_mgr_element(ts_str(t_pool_parent));
	    pstResourcePool->parent = pstParentResourcePool;
	} else {
	    pstParentResourcePool = get_resource_mgr_element(GLOBAL_RSRC_POOL);
	    pstResourcePool->parent = pstParentResourcePool;
	}
    }
    if (bn_binding_name_parts_pattern_match_va
	    (name_parts, 4, 2, "*", "client_sessions")
	    || bn_binding_name_parts_pattern_match_va(name_parts, 4, 2, "*",
		"origin_sessions")
	    || bn_binding_name_parts_pattern_match_va(name_parts, 4, 2, "*",
		"max_bandwidth")
	    || bn_binding_name_parts_pattern_match_va(name_parts, 4, 2, "*",
		"reserved_diskspace_tier1")
	    || bn_binding_name_parts_pattern_match_va(name_parts, 4, 2, "*",
		"reserved_diskspace_tier2")
	    || bn_binding_name_parts_pattern_match_va(name_parts, 4, 2, "*",
		"reserved_diskspace_tier3")) {
	uint64_t t_resource_count = 0;
	uint64_t t_old_resource_count = 0;
	uint64_t config_rsrc = 0, current_rsrc = 0;
	float ratio = 0;
	rp_type_en rp_type;

	err = bn_binding_get_uint64(binding, ba_value, NULL,
		    &t_resource_count);
	bail_error(err);

	t_resource_type = tstr_array_get_str_quick(name_parts, 5);
	lc_log_debug(jnpr_log_level, "rpp- %lu, type-%s ", t_resource_count,
		t_resource_type);

	if (t_resource_type) {
	    if (strcmp((t_resource_type), "client_sessions") == 0)
		rp_type = RESOURCE_POOL_TYPE_CLIENT_SESSION;
	    else if (strcmp((t_resource_type), "origin_sessions") == 0)
		rp_type = RESOURCE_POOL_TYPE_ORIGIN_SESSION;
	    else if (strcmp((t_resource_type), "max_bandwidth") == 0)
		rp_type = RESOURCE_POOL_TYPE_BW;
	    else if (strcmp((t_resource_type), "reserved_diskspace_tier1")
		    == 0)
		rp_type = RESOURCE_POOL_TYPE_UTIL_TIER1;
	    else if (strcmp((t_resource_type), "reserved_diskspace_tier2")
		    == 0)
		rp_type = RESOURCE_POOL_TYPE_UTIL_TIER2;
	    else if (strcmp((t_resource_type), "reserved_diskspace_tier3")
		    == 0)
		rp_type = RESOURCE_POOL_TYPE_UTIL_TIER3;
	    else {
		rp_type = RESOURCE_POOL_MAX;
	    }
	} else {
	    /*
	     * set as unsupported resource 
	     */
	    rp_type = RESOURCE_POOL_MAX;
	}

	if (rp_type >= RESOURCE_POOL_MAX) {
	    /*
	     * unsupported resource 
	     */
	    goto bail;
	}

	if (rp_type == RESOURCE_POOL_TYPE_BW) {
	    node_name_t rp_node;
	    uint64_t rp_bandwidth;
	    uint64_t global_bw = 0;
	    bn_binding *bw_binding = NULL;

	    if (refetech_global_pool) {
		err = get_global_pool(nvsd_mcc, "bandwidth", &global_bw);
		bail_error(err);
		global_pool_bandwidth = global_bw;
		if (p_global_resource_pool) {
		    AO_store(&p_global_resource_pool->
			    resources[RESOURCE_POOL_TYPE_BW - 1].max,
			    global_pool_bandwidth);
		}
		refetech_global_pool = 0;
	    }

	    /*
	     * get value from state variables 
	     */
	    snprintf(rp_node, sizeof (rp_node),
		    "/nkn/nvsd/resource_mgr/monitor/state/%s/allowed/bandwidth",
		    t_resource_pool);
	    err = mdc_get_binding(nvsd_mcc, NULL, NULL, false, &bw_binding,
			rp_node, NULL);
	    bail_error(err);

	    err = bn_binding_get_uint64(bw_binding, ba_value, NULL,
			&rp_bandwidth);
	    bail_error(err);
	    current_rsrc = rp_bandwidth;
	} else if (rp_type == RESOURCE_POOL_TYPE_CLIENT_SESSION) {
	    node_name_t rp_node;
	    uint64_t rp_conns = 0, global_value = 0;
	    bn_binding *con_binding = NULL;

	    if (refetch_global_pool_conx) {
		err = get_global_pool(nvsd_mcc, "net_conn", &global_value);
		bail_error(err);

		if (p_global_resource_pool) {
		    AO_store(&p_global_resource_pool->
			    resources[RESOURCE_POOL_TYPE_CLIENT_SESSION -
			    1].max, global_value);
		}
		refetch_global_pool_conx = 0;
	    }

	    /*
	     * get value from state variables 
	     */
	    snprintf(rp_node, sizeof (rp_node),
		    "/nkn/nvsd/resource_mgr/monitor/state/%s/allowed/net_conn",
		    t_resource_pool);
	    err = mdc_get_binding(nvsd_mcc, NULL, NULL, false, &con_binding,
		    rp_node, NULL);
	    bail_error(err);

	    err = bn_binding_get_uint64(con_binding, ba_value, NULL,
		    &rp_conns);
	    bail_error(err);

	    current_rsrc = rp_conns;
	} else {
	    current_rsrc = t_resource_count;
	    config_rsrc = t_resource_count;
	}

	lc_log_debug(jnpr_log_level,
		"%s: ratio - %g, conf=> %lu, curr=> %lu", __FUNCTION__,
		ratio, config_rsrc, current_rsrc);

	global_resource_max.rsrc_cur_used[rp_type - 1] += current_rsrc;
	if (nkn_system_inited) {
	    t_old_resource_count =
		AO_load(&pstResourcePool->resources[rp_type - 1].max);
	}
	AO_store(&pstResourcePool->resources[rp_type - 1].max,
		current_rsrc);
	AO_store(&pstResourcePool->resources[rp_type - 1].configmax,
		config_rsrc);

#ifdef NOT_NEEDED
	/*
	 * Adjust the parent resource count here 
	 */
	if (p_global_resource_pool)
	    AO_store(&p_global_resource_pool->resources[rp_type - 1].max,
		    t_parent_resource_count);
#endif
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
 *	function : nvsd_server_map_delete_cfg_handle_change()
 *	purpose : handler for config delete changes for server_map module
 */
static int
nvsd_resource_mgr_delete_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bn_name,
	const tstr_array * name_components,
	const tstring * name_last_part,
	const tstring * value, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    const char *t_resource_mgr = NULL;
    tstr_array *name_parts = NULL;
    tbool *rechecked_licenses_p = data;
    resource_pool_t *pstResourcePool = NULL;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);

    UNUSED_ARGUMENT(bn_name);
    UNUSED_ARGUMENT(name_components);
    UNUSED_ARGUMENT(name_last_part);
    UNUSED_ARGUMENT(value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /*
     * Check if this is our node 
     */
    if (bn_binding_name_pattern_match(ts_str(name),
		"/nkn/nvsd/resource_mgr/config/**")) {

	/*-------- Get the ServerMap ------------*/
	bn_binding_get_name_parts(binding, &name_parts);
	bail_error(err);
	t_resource_mgr = tstr_array_get_str_quick(name_parts, 4);

	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../nkn/nvsd/resource_mgr/config as : \"%s\"",
		t_resource_mgr);

	/*
	 * Delete this only if the wildcard change is received 
	 */
	if (bn_binding_name_parts_pattern_match_va(name_parts, 4, 1, "*")) {
	    /*
	     * Get the server_map entry in the global array 
	     */
	    pstResourcePool = get_resource_mgr_element(t_resource_mgr);
	    if (pstResourcePool && (NULL != pstResourcePool->name)) {
		/*
		 * delete rp counters 
		 */
		nvsd_rp_add_delete_nkn_cnt(pstResourcePool, false);

		/*
		 * Free the allocated name 
		 */
		free(pstResourcePool->name);

		/*
		 * Clear the other allocated fields 
		 */
		memset(pstResourcePool, 0, sizeof (resource_pool_t));
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
}	/* end of * nvsd_resource_mgr_delete_cfg_handle_change */

resource_pool_t *
nvsd_mgmt_get_resource_mgr(const char *cpResourcePool)
{
    int i;

    /*
     * Sanity Check 
     */
    if (NULL == cpResourcePool)
	return (NULL);

    lc_log_basic(LOG_DEBUG, "Request received for %s resource_mgr",
	    cpResourcePool);

    /*
     * Now find the matching entry 
     */
    for (i = 0; i < NKN_MAX_RESOURCE_POOL; i++) {
	if ((NULL != g_lstresrcpool.lstResourcePool[i].name) &&
		(0 == strcmp(cpResourcePool,
			     g_lstresrcpool.lstResourcePool[i].name)))
	    /*
	     * Return this entry 
	     */
	    return (&(g_lstresrcpool.lstResourcePool[i]));
    }

    /*
     * No Match 
     */
    return (NULL);
}	/* end of nvsd_mgmt_get_server_map() */

/*! get the resource pool pointer for the given rp_index */
static inline resource_pool_t *
get_rp(int rp_index)
{
    if ((rp_index < 0) && (rp_index >= g_lstresrcpool.g_nresrc_pool))
	return NULL;
    return &(g_lstresrcpool.lstResourcePool[rp_index]);
}

int
search_ressource_mgr(const char *cpResourcePool, int *ret_idx)
{
    int i;
    resource_pool_t *rp = NULL;

    for (i = 0; i < min(g_lstresrcpool.g_nresrc_pool, NKN_MAX_RESOURCE_POOL);
	    ++i) {
	rp = get_rp(i);
	if (rp != NULL && (rp->name != NULL)) {
	    if (strcmp(cpResourcePool, rp->name) == 0) {
		*ret_idx = i;
		return 0;
	    }
	}
    }
    return -1;
}

/* ------------------------------------------------------------------------- */

/*
 *    function : nvsd_rp_alloc_resource_no_check()
 *    purpose : Allocate the specifed amount of resource from the resource pool.
 *	  This kind of alloc doesn't do a bound check on the max resources.
 *    returns : 1 on success and 0 on failure
 */
int
nvsd_rp_alloc_resource_no_check(rp_type_en resource_type,
	uint32_t rp_index, uint64_t resource_count)
{

    CHECK_FOR_BAD_RP_IDX(rp_index);
    if (resource_type > RESOURCE_POOL_MAX) {
	glob_err_rc_resource_type++;
	DBG_LOG(SEVERE, MOD_RESRC_POOL, "BAD resource type index");
	return 0;
    }
    /*
     * AO_add 
     */
    AO_fetch_and_add(&
	    (g_lstresrcpool.lstResourcePool[rp_index].
	     resources[resource_type - 1].used), resource_count);
    return 1;
}	/* end of nvsd_rp_alloc_resource_no_check */

/*
 *    function : nvsd_rp_alloc_resource()
 *    purpose : Allocate the specifed amount of resource from the resource pool.
 *    returns : 1 on success and 0 on failure
 */
int
nvsd_rp_alloc_resource(rp_type_en resource_type,
	uint32_t rp_index, uint64_t resource_count)
{
    uint64_t res_used;
    uint64_t res_max;
    uint64_t old_res_used;

    CHECK_FOR_BAD_RP_IDX(rp_index);
    if (resource_type > RESOURCE_POOL_MAX) {
	glob_err_rc_resource_type++;
	DBG_LOG(SEVERE, MOD_RESRC_POOL, "BAD resource type index");
	return 0;
    }
    /*
     * First increment the used,and then check if current used is within max,
     * else subtract the resource count and return 0,
     * by this way during max limits we will never go over the max,
     * but a legitimate request within the max might fail temporarily,
     * when a previous request tried to allocate is over the max.
     */

    old_res_used =
	AO_fetch_and_add(&
		(g_lstresrcpool.lstResourcePool[rp_index].
		 resources[resource_type - 1].used), resource_count);

    res_used = old_res_used + resource_count;
    res_max =
	AO_load(&
		(g_lstresrcpool.lstResourcePool[rp_index].
		 resources[resource_type - 1].max));

    if (res_used <= res_max) {
	/*
	 * AO_add 
	 */
	DBG_LOG(MSG, MOD_RESRC_POOL,
		"nvsd_rp_alloc_resource: max = %lu, used = %lu, resource_req = %lu",
		res_max, res_used, resource_count);
	return 1;
    } else {
	AO_fetch_and_add(&
		(g_lstresrcpool.lstResourcePool[rp_index].
		 resources[resource_type - 1].used), -resource_count);
	glob_err_rc_over_req++;
	return 0;
    }
}								/* end of nvsd_rp_alloc_resource */

/*
 *      function : nvsd_rp_free_resource()
 *      purpose : free the specifed amount of resource to the resource pool.
 *	returns : 1 on success and 0 on failure
 */
int
nvsd_rp_free_resource(rp_type_en resource_type,
	uint32_t rp_index, uint64_t resource_count)
{
    uint64_t res_used;
    uint64_t res_max;

    CHECK_FOR_BAD_RP_IDX(rp_index);
    if (resource_type > RESOURCE_POOL_MAX) {
	glob_err_rc_resource_type++;
	DBG_LOG(SEVERE, MOD_RESRC_POOL, "BAD resource type index");
	return 0;
    }

    res_used = AO_load(& (g_lstresrcpool.lstResourcePool[rp_index].
		 resources[resource_type - 1].used));
    res_max = AO_load(& (g_lstresrcpool.lstResourcePool[rp_index].
		 resources[resource_type - 1].max));

    if (res_used > 0) {
	/*
	 * AO_subtract 
	 */
	AO_fetch_and_add(&
		(g_lstresrcpool.lstResourcePool[rp_index].
		 resources[resource_type - 1].used), -resource_count);
	DBG_LOG(MSG, MOD_RESRC_POOL,
		"nvsd_rp_free_resource: max = %lu, used = %lu, resource_req = %lu",
		res_max, res_used, resource_count);
	return 1;
    } else {
	DBG_LOG(SEVERE, MOD_RESRC_POOL,
		"nvsd_rp_free_resource: max = %lu, used = %lu, resource_req = %lu",
		res_max, res_used, resource_count);
	glob_err_rc_free_less_zero++;
	return 0;
    }
    return 0;
}	/* end of nvsd_rp_free_resource */

/*
 *      function : nvsd_rp_get_resource()
 *      purpose : get the resource count from the resource pool.
 *	returns : (uint64)-1 on a error.
 */
uint64_t
nvsd_rp_get_resource(rp_type_en resource_type, uint32_t rp_index)
{
    uint64_t res_used = (uint64) (-1);

    CHECK_FOR_BAD_RP_IDX(rp_index);
    if (resource_type > RESOURCE_POOL_MAX) {
	glob_err_rc_resource_type++;
	DBG_LOG(SEVERE, MOD_RESRC_POOL, "BAD resource type");
	return 0;
    }
    /*
     * AO Operations below 
     */
    res_used = AO_load(& (g_lstresrcpool.lstResourcePool[rp_index].
		 resources[resource_type - 1].used));

    return res_used;
}	/* end of nvsd_rp_get_resource */

/*
 *      function : nvsd_rp_set_resource()
 *      purpose : setthe resource count from the resource pool.
 *	returns : 1 on success and 0 on failure
 */
int
nvsd_rp_set_resource(rp_type_en resource_type,
	uint32_t rp_index, uint64_t resource_count)
{
    CHECK_FOR_BAD_RP_IDX(rp_index);
    if (resource_type > RESOURCE_POOL_MAX) {
	glob_err_rc_resource_type++;
	DBG_LOG(SEVERE, MOD_RESRC_POOL, "BAD resource type");
	return 0;
    }

    AO_store(&
	    (g_lstresrcpool.lstResourcePool[rp_index].
	     resources[resource_type - 1].used), resource_count);

    return 1;
}	/* end of nvsd_rp_set_resource */

void
nvsd_rp_bw_timer_cleanup_1sec(void)
{
    /*
     * Every second reset every bw counters in resource pool to zero.
     */
    nvsd_rp_cleanup_used(RESOURCE_POOL_TYPE_BW);
    return;
}	/* end of nvsd_rp_bw_timer_cleanup_1sec */

void
nvsd_rp_cleanup_used(rp_type_en resource_type)
{
    /*
     * Every second reset every bw counters in resource pool to zero.
     */
    int i;
    resource_t *res;

    if (resource_type > RESOURCE_POOL_MAX) {
	glob_err_rc_resource_type++;
	DBG_LOG(SEVERE, MOD_RESRC_POOL, "BAD resource type");
	return;
    }
    /*
     * Dont iterate through all the resource pools,
     * Itreate through the configured alone,
     * At the same time,we dont decrease g_nresrc_pool,
     * So,do a min of the max and configured.
     */
    /*
     * XXX - DON'T USE MAGIC NUMBERS, HERE 256 
     */
    for (i = 0; i < min(g_lstresrcpool.g_nresrc_pool, NKN_MAX_RESOURCE_POOL);
	    i++) {
	if ((NULL != g_lstresrcpool.lstResourcePool[i].name)) {
	    /*
	     * Index to BW resource and zero the used. 
	     */
	    res =
		&g_lstresrcpool.lstResourcePool[i].resources[resource_type - 1];
	    if (resource_type == RESOURCE_POOL_TYPE_BW) {
		g_rsrc_bw_1sec_val[i] = AO_load(&(res->used));
	    }
	    AO_store(&(res->used), 0);
	}
    }

    return;
}	/* end of nvsd_rp_cleanup */

void
nvsd_rp_bw_update_send(int rp_index, uint64_t byte_sent)
{
    nvsd_rp_alloc_resource(RESOURCE_POOL_TYPE_BW, rp_index, byte_sent);
    return;
}	/* end of nvsd_rp_bw_update_send */

uint64_t
nvsd_rp_bw_query_available_bw(int rp_index)
{
    /*
     * return available bandwidth from resource pool.
     * return -1: means unlimited
     * return 0: no more available bw
     */

    return nvsd_rp_query_available(RESOURCE_POOL_TYPE_BW, rp_index);
}	/* end of nvsd_bw_query_available_bw */

uint64_t
nvsd_rp_query_available(rp_type_en resource_type, uint32_t rp_index)
{
    /*
     * return available bandwidth from resource pool.
     * return -1: means unlimited
     * return 0: no more available bw
     */
    uint64_t res_used;
    uint64_t res_max;

    CHECK_FOR_BAD_RP_IDX(rp_index);
    if (resource_type > RESOURCE_POOL_MAX) {
	glob_err_rc_resource_type++;
	DBG_LOG(SEVERE, MOD_RESRC_POOL, "BAD resource type index");
	return 0;
    }
    /*
     * AO Operations below 
     */
    res_used =
	AO_load(&g_lstresrcpool.lstResourcePool[rp_index].
		resources[resource_type - 1].used);
    res_max =
	AO_load(&g_lstresrcpool.lstResourcePool[rp_index].
		resources[resource_type - 1].max);

    /*
     * We don't return -1 when res_max == 0 Bug - 6968 
     */
    if (res_max > res_used) {
	return res_max - res_used;	// give zero when no bw available
    } else {
	return 0;
    }
}	/* end of nvsd_rp_query_available */

/* calculate total bandwidth configured in the system */
int
get_total_bw(md_client_context * mcc, uint64_t * total_actual_bw)
{
    int err = 0;
    bn_binding *binding = NULL;
    uint64_t total_system_bw = 0;

    bail_null(total_actual_bw);
    bail_null(mcc);

    *total_actual_bw = 0;
    /*
     * get value of global resouce pool 
     */
    err = mdc_get_binding(nvsd_mcc, NULL, NULL, false, &binding,
	    "/nkn/nvsd/resource_mgr/monitor/system/bandwidth",
	    NULL);
    bail_error(err);

    err = bn_binding_get_uint64(binding, ba_value, NULL, &total_system_bw);
    bail_error(err);

    *total_actual_bw = total_system_bw;
    lc_log_debug(jnpr_log_level, "global_pool bw - %lu", *total_actual_bw);

bail:
    bn_binding_free(&binding);
    return err;
}

/*
 * API to set the global pool max values.Could be called by module that
 * owns the resource.
 */
int
nvsd_rp_set_total(rp_type_en resource_type, uint64_t resource_num)
{
    /*
     * return 0 on success, error code from samara on failure.
     */

    /*
     * First Update the backup global max.  Check if the current global
     * max for resource is less than the previous max.  If so, go through
     * the resource pool list and update the value according to the
     * reduction percent
     */
    float32 downratio = 0;
    float32 upratio = 0;
    uint64_t old_max = 0;
    uint64_t new_max = 0;
    uint64_t config_max = 0;
    int i = 0, err = 0;
    uint64_t current_rsrc = 0;
    bn_binding_array *bindings = NULL;

    if (!p_global_resource_pool || !resource_num)
	return 0;

    /*
     * XXX- DON'T USE MAGIC NUMBERS 
     */
    if (resource_type < 1 && resource_type > 7) {
	glob_err_rc_resource_type_range++;
	DBG_LOG(SEVERE, MOD_RESRC_POOL, "BAD resource type index");
	return 0;
    }
    /*
     * When there is a decrease in resource allocation, reduce the
     * resource allocation across all pools
     */
    lc_log_debug(jnpr_log_level, "type- %d, sys_max - %lu, curr - %lu",
	    resource_type,
	    global_resource_max.rsrc_sys_max[resource_type - 1],
	    resource_num);
    if ((resource_type == RESOURCE_POOL_TYPE_BW)
	    || (resource_type == RESOURCE_POOL_TYPE_CLIENT_SESSION)) {
	err = mdc_get_binding_children(nvsd_mcc, NULL, NULL, true, &bindings,
		false, true,
		"/nkn/nvsd/resource_mgr/monitor/state");
	bail_error(err);

	bn_binding_array_dump("debug", bindings, jnpr_log_level);
    }

    if (resource_type == RESOURCE_POOL_TYPE_BW) {
	uint64_t global_bw = 0, sys_conf_val = 0;

	err = get_total_bw(nvsd_mcc, &sys_conf_val);
	bail_error(err);

	global_resource_max.rsrc_sys_max[RESOURCE_POOL_TYPE_BW - 1] =
	    sys_conf_val;

	err = get_global_pool(nvsd_mcc, "bandwidth", &global_bw);
	bail_error(err);

	global_pool_bandwidth = global_bw;
	if (p_global_resource_pool) {
	    AO_store(&p_global_resource_pool->
		    resources[RESOURCE_POOL_TYPE_BW - 1].max,
		    global_pool_bandwidth);
	}

	lc_log_debug(jnpr_log_level, "total- %lu, glob- %lu", sys_conf_val,
		global_bw);

	for (i = 1; i < NKN_MAX_RESOURCE_POOL; i++) {
	    if ((&g_lstresrcpool.lstResourcePool[i])
		    && g_lstresrcpool.lstResourcePool[i].name) {
		node_name_t node = { 0 };
		const bn_binding *binding = NULL;

		// TODO,Atleast make a single query
		snprintf(node, sizeof (node),
			"/nkn/nvsd/resource_mgr/monitor/state/%s/allowed/bandwidth",
			g_lstresrcpool.lstResourcePool[i].name);

		err =
		    bn_binding_array_get_binding_by_name(bindings, node,
			    &binding);
		bail_error(err);

		err =
		    bn_binding_get_uint64(binding, ba_value, NULL,
			    &current_rsrc);
		bail_error(err);

		lc_log_debug(jnpr_log_level, "rp=> %s bw=> %lu",
			g_lstresrcpool.lstResourcePool[i].name,
			current_rsrc);

		AO_store(&g_lstresrcpool.lstResourcePool[i].
			resources[resource_type - 1].max, current_rsrc);
		/*
		 * update the max system max and allocated max 
		 */
		if (i != 0)
		    global_resource_max.rsrc_cur_used[resource_type - 1] +=
			new_max;
	    }
	}
	goto bail;
    } else if (resource_type == RESOURCE_POOL_TYPE_CLIENT_SESSION) {
	uint64_t sys_conf_val = 0, glob_conf_val = 0;

	err = get_total_network_conn(nvsd_mcc, &sys_conf_val);
	bail_error(err);

	global_resource_max.rsrc_sys_max[RESOURCE_POOL_TYPE_CLIENT_SESSION -
	    1] = sys_conf_val;

	err = get_global_pool(nvsd_mcc, "net_conn", &glob_conf_val);
	bail_error(err);

	if (p_global_resource_pool) {
	    AO_store(&p_global_resource_pool->
		    resources[RESOURCE_POOL_TYPE_CLIENT_SESSION - 1].max,
		    glob_conf_val);
	}

	lc_log_debug(jnpr_log_level, "total- %lu, glob- %lu", sys_conf_val,
		glob_conf_val);

	for (i = 1; i < NKN_MAX_RESOURCE_POOL; i++) {
	    if ((&g_lstresrcpool.lstResourcePool[i])
		    && g_lstresrcpool.lstResourcePool[i].name) {
		node_name_t node = { 0 };
		const bn_binding *binding = NULL;

		snprintf(node, sizeof (node),
			"/nkn/nvsd/resource_mgr/monitor/state/%s/allowed/net_conn",
			g_lstresrcpool.lstResourcePool[i].name);

		err =
		    bn_binding_array_get_binding_by_name(bindings, node,
			    &binding);
		bail_error(err);

		err =
		    bn_binding_get_uint64(binding, ba_value, NULL,
			    &current_rsrc);
		bail_error(err);

		lc_log_debug(jnpr_log_level, "rp=> %s conn=> %lu",
			g_lstresrcpool.lstResourcePool[i].name,
			current_rsrc);

		AO_store(&g_lstresrcpool.lstResourcePool[i].
			resources[resource_type - 1].max, current_rsrc);

		/*
		 * update the max system max and allocated max 
		 */
		if (i != 0)
		    global_resource_max.rsrc_cur_used[resource_type - 1] +=
			new_max;
	    }
	}
	goto bail;
    }

    lc_log_debug(jnpr_log_level, "type- %d, corr- %lu, count- %lu",
	    resource_type,
	    global_resource_max.rsrc_sys_max[resource_type - 1],
	    resource_num);

    if (global_resource_max.rsrc_sys_max[resource_type - 1] > resource_num) {
	/*
	 * Adjust across all the resource pools 
	 */
	downratio = (double)
	    global_resource_max.rsrc_sys_max[resource_type - 1] / resource_num;

	DBG_LOG(SEVERE, MOD_RESRC_POOL, "Down shifting resource allocation");
	lc_log_basic(jnpr_log_level, "down shifting resource allocation");

	global_resource_max.rsrc_cur_used[resource_type - 1] = 0;
	global_resource_max.rsrc_sys_max[resource_type - 1] = resource_num;

	for (i = 0; i < NKN_MAX_RESOURCE_POOL; i++) {
	    if ((&g_lstresrcpool.lstResourcePool[i])
		    && g_lstresrcpool.lstResourcePool[i].name) {
		old_max =
		    AO_load(&g_lstresrcpool.lstResourcePool[i].
			    resources[resource_type - 1].max);
		if (downratio) {
		    new_max = old_max / downratio;
		    AO_store(&g_lstresrcpool.lstResourcePool[i].
			    resources[resource_type - 1].max, new_max);
		}
		/*
		 * update the max system max and allocated max 
		 */
		if (i != 0)
		    global_resource_max.rsrc_cur_used[resource_type - 1] +=
			new_max;
	    }
	    lc_log_basic(jnpr_log_level,
		    "%s-2: gl_used- %lu old- %lu, new- %lu", __FUNCTION__,
		    global_resource_max.rsrc_cur_used[resource_type - 1],
		    old_max, new_max);
	}
    }
    /*
     * XXX- does following comments are relevant to else part or old code 
     */
    /*
     * Slap the global rps values after detecting the individual rp's values.
     * Since Mgmtd comes up first before the nvsd comes up,
     * The db will have individual rp values and the check in db is not done,
     * as the monitoring node to get the resource available value is not found,
     * once nvsd comes up and parses the nodes,we would set the global values
     * and global values have to be adjusted according to the individual rp
     * values
     */
    else {
	uint64_t globalrp_res_count = 0;

	lc_log_basic(LOG_DEBUG, "Up shifting resource allocation");
	upratio = (double)
	    resource_num / global_resource_max.rsrc_sys_max[resource_type - 1];

	lc_log_basic(jnpr_log_level, "upration - %g", upratio);
	DBG_LOG(SEVERE, MOD_RESRC_POOL, "Up shifting resource allocation");

	global_resource_max.rsrc_cur_used[resource_type - 1] = 0;
	global_resource_max.rsrc_sys_max[resource_type - 1] = resource_num;

	for (i = 0; i < NKN_MAX_RESOURCE_POOL; i++) {
	    if ((&g_lstresrcpool.lstResourcePool[i])
		    && g_lstresrcpool.lstResourcePool[i].name) {
		old_max =
		    AO_load(&g_lstresrcpool.lstResourcePool[i].
			    resources[resource_type - 1].max);
		config_max = g_lstresrcpool.lstResourcePool[i].
		    resources[resource_type - 1].configmax;
		lc_log_basic(jnpr_log_level,
			"%s: upratio: old- %lu, config_max - %lu",
			__FUNCTION__, old_max, config_max);
		if (upratio) {
		    new_max = old_max * upratio;
		    /*
		     * Assign the new max value if it is less than or equal
		     * to configured max value 
		     */
		    if (old_max == config_max) {
			// Assign the config_max value to new_max
			// do no other change.
			// continue; //Do nothing, the resource allocation is 
			// already at max
			new_max = config_max;
		    }
		    if (new_max <= config_max) {
			AO_store(&g_lstresrcpool.lstResourcePool[i].
				resources[resource_type - 1].max, new_max);
		    } else if (new_max > config_max) {
			AO_store(&g_lstresrcpool.lstResourcePool[i].
				resources[resource_type - 1].max, config_max);
			new_max = config_max;	// new_max is assigned
			// configured value
			// so that it gets added to res_cur_used.
		    }
		    lc_log_basic(jnpr_log_level, "%s: upatio- new_max - %lu",
			    __FUNCTION__, new_max);

		}
		/*
		 * update the max system max and allocated max 
		 */
		if (i != 0)
		    global_resource_max.rsrc_cur_used[resource_type - 1] +=
			new_max;
	    }
	    globalrp_res_count = resource_num -
		global_resource_max.rsrc_cur_used[resource_type - 1];
	    lc_log_basic(jnpr_log_level, "%s: gl_rp = %lu", __FUNCTION__,
		    globalrp_res_count);

	    AO_store(&p_global_resource_pool->resources[resource_type - 1].max,
		    globalrp_res_count);
	}
    }

bail:
    bn_binding_array_free(&bindings);
    return 1;
}	/* end of nvsd_rp_set_total */

uint64_t
nvsd_rp_get_intf_total_bw(void)
{
    int err = 0;
    uint64_t total_bw = 0;

    err = get_tx_bw_total_bps(&total_bw, nvsd_mcc, SERVICE_INTF_HTTP);
    bail_error(err);
bail:
    return (total_bw) / 8;
}	/* end of nvsd_rp_get_intf_total_bw */

int
nvsd_rp_adjust_bw(void)
{
    nvsd_rp_set_total(RESOURCE_POOL_TYPE_BW, 1);
    return 1;
}	/* end fo nvsd_rp_adjust_bw */

static int
nvsd_rp_add_delete_nkn_cnt(resource_pool_t * pResourcePool, tbool add)
{
    char str_name[CNTR_MAX_STR];

    if (!pResourcePool || !pResourcePool->name)
	return -1;

    snprintf(str_name, CNTR_MAX_STR, "rp.%s.client.sessions_used",
	    pResourcePool->name);
    if (add) {
	nkn_mon_add(str_name, NULL,
		&pResourcePool->
		resources[RESOURCE_POOL_TYPE_CLIENT_SESSION - 1].used,
		sizeof (pResourcePool->
		    resources[RESOURCE_POOL_TYPE_CLIENT_SESSION -
		    1].used));
    } else {
	nkn_mon_delete(str_name, NULL);
    }

    snprintf(str_name, CNTR_MAX_STR, "rp.%s.client.sessions_max",
	    pResourcePool->name);
    if (add) {
	nkn_mon_add(str_name, NULL,
		&pResourcePool->
		resources[RESOURCE_POOL_TYPE_CLIENT_SESSION - 1].max,
		sizeof (pResourcePool->
		    resources[RESOURCE_POOL_TYPE_CLIENT_SESSION -
		    1].max));
    } else {
	nkn_mon_delete(str_name, NULL);
    }

    /*
     * Add counter for the resource pool 
     */
    snprintf(str_name, CNTR_MAX_STR, "rp.%s.client.bw_used",
	    pResourcePool->name);

    if (add) {
	nkn_mon_add(str_name, NULL,
		&g_rsrc_bw_1sec_val[pResourcePool->index],
		sizeof (g_rsrc_bw_1sec_val[pResourcePool->index]));
    } else {
	nkn_mon_delete(str_name, NULL);
    }

    snprintf(str_name, CNTR_MAX_STR, "rp.%s.client.bw_max",
	    pResourcePool->name);
    if (add) {
	nkn_mon_add(str_name, NULL,
		&pResourcePool->resources[RESOURCE_POOL_TYPE_BW - 1].max,
		sizeof (pResourcePool->resources[RESOURCE_POOL_TYPE_BW - 1].
		    max));
    } else {
	nkn_mon_delete(str_name, NULL);
    }

    /*
     * Add counter for the resource pool 
     */
    snprintf(str_name, CNTR_MAX_STR, "rp.%s.server.origin.sessions_used",
	    pResourcePool->name);
    if (add) {
	nkn_mon_add(str_name, NULL,
		&pResourcePool->
		resources[RESOURCE_POOL_TYPE_ORIGIN_SESSION - 1].used,
		sizeof (pResourcePool->
		    resources[RESOURCE_POOL_TYPE_ORIGIN_SESSION -
		    1].used));
    } else {
	nkn_mon_delete(str_name, NULL);
    }

    snprintf(str_name, CNTR_MAX_STR, "rp.%s.server.origin.sessions_max",
	    pResourcePool->name);
    if (add) {
	nkn_mon_add(str_name, NULL,
		&pResourcePool->
		resources[RESOURCE_POOL_TYPE_ORIGIN_SESSION - 1].max,
		sizeof (pResourcePool->
		    resources[RESOURCE_POOL_TYPE_ORIGIN_SESSION -
		    1].max));
    } else {
	nkn_mon_delete(str_name, NULL);
    }

    /*
     * Add counter for the resource pool 
     */
    snprintf(str_name, CNTR_MAX_STR, "rp.%s.cache.tier.SAS.bytes_used",
	    pResourcePool->name);
    if (add) {
	nkn_mon_add(str_name, NULL,
		&pResourcePool->resources[RESOURCE_POOL_TYPE_UTIL_TIER2 -
		1].used,
		sizeof (pResourcePool->
		    resources[RESOURCE_POOL_TYPE_UTIL_TIER1 - 1].used));
    } else {
	nkn_mon_delete(str_name, NULL);
    }

    snprintf(str_name, CNTR_MAX_STR, "rp.%s.cache.tier.SAS.bytes_max",
	    pResourcePool->name);
    if (add) {
	nkn_mon_add(str_name, NULL,
		&pResourcePool->resources[RESOURCE_POOL_TYPE_UTIL_TIER2 -
		1].max,
		sizeof (pResourcePool->
		    resources[RESOURCE_POOL_TYPE_UTIL_TIER1 - 1].max));
    } else {
	nkn_mon_delete(str_name, NULL);
    }

    /*
     * Add counter for the resource pool 
     */
    snprintf(str_name, CNTR_MAX_STR, "rp.%s.cache.tier.SATA.bytes_used",
	    pResourcePool->name);
    if (add) {
	nkn_mon_add(str_name, NULL,
		&pResourcePool->resources[RESOURCE_POOL_TYPE_UTIL_TIER3 -
		1].used,
		sizeof (pResourcePool->
		    resources[RESOURCE_POOL_TYPE_UTIL_TIER2 - 1].used));
    } else {
	nkn_mon_delete(str_name, NULL);
    }

    snprintf(str_name, CNTR_MAX_STR, "rp.%s.cache.tier.SATA.bytes_max",
	    pResourcePool->name);
    if (add) {
	nkn_mon_add(str_name, NULL,
		&pResourcePool->resources[RESOURCE_POOL_TYPE_UTIL_TIER3 -
		1].max,
		sizeof (pResourcePool->
		    resources[RESOURCE_POOL_TYPE_UTIL_TIER2 - 1].max));
    } else {
	nkn_mon_delete(str_name, NULL);
    }

    /*
     * Add counter for the resource pool 
     */
    snprintf(str_name, CNTR_MAX_STR, "rp.%s.cache.tier.SSD.bytes_used",
	    pResourcePool->name);
    if (add) {
	nkn_mon_add(str_name, NULL,
		&pResourcePool->resources[RESOURCE_POOL_TYPE_UTIL_TIER1 -
		1].used,
		sizeof (pResourcePool->
		    resources[RESOURCE_POOL_TYPE_UTIL_TIER3 - 1].used));
    } else {
	nkn_mon_delete(str_name, NULL);
    }

    snprintf(str_name, CNTR_MAX_STR, "rp.%s.cache.tier.SSD.bytes_max",
	    pResourcePool->name);
    if (add) {
	nkn_mon_add(str_name, NULL,
		&pResourcePool->resources[RESOURCE_POOL_TYPE_UTIL_TIER1 -
		1].max,
		sizeof (pResourcePool->
		    resources[RESOURCE_POOL_TYPE_UTIL_TIER3 - 1].max));
    } else {
	nkn_mon_delete(str_name, NULL);
    }

    snprintf(str_name, CNTR_MAX_STR, "rp.%s.namespace.bound_num",
	    pResourcePool->name);
    if (add) {
	nkn_mon_add(str_name, NULL,
		&pResourcePool->num_ns_bound,
		sizeof (pResourcePool->num_ns_bound));
    } else {
	nkn_mon_delete(str_name, NULL);
    }

    return 0;
}	/* end of nvsd_rp_add_delete_nkn_cnt */


int
get_global_pool(md_client_context * mcc,
	const char *rsrc, uint64_t * global_pool_rsrc)
{
    int err = 0;
    bn_binding *binding = NULL;
    uint64_t global_pool_value = 0;
    node_name_t node_name = { 0 };

    bail_null(global_pool_rsrc);
    bail_null(mcc);
    bail_null(rsrc);

    *global_pool_rsrc = 0;

    snprintf(node_name, sizeof (node_name),
	    "/nkn/nvsd/resource_mgr/monitor/state/global_pool/allowed/%s",
	    rsrc);

    /*
     * get value of global resouce pool 
     */
    err = mdc_get_binding(nvsd_mcc, NULL, NULL, false, &binding,
	    node_name, NULL);
    bail_error(err);

    err = bn_binding_get_uint64(binding, ba_value, NULL, &global_pool_value);
    bail_error(err);

    if (0 == strcmp(rsrc, "net_conn")) {
	*global_pool_rsrc = global_pool_value;
    } else {
	*global_pool_rsrc = global_pool_value;
    }
    lc_log_debug(jnpr_log_level, "global_pool rsrc %s, - %lu", rsrc,
	    *global_pool_rsrc);

bail:
    bn_binding_free(&binding);
    return err;
}

int
get_total_network_conn(md_client_context * mcc, uint64_t * max_conn)
{
    int err = 0;
    bn_binding *binding = NULL;
    uint32_t max_network_conn = 0;

    bail_null(max_conn);
    bail_null(mcc);

    *max_conn = 0;

    /*
     * get value of global resouce pool 
     */
    err = mdc_get_binding(nvsd_mcc, NULL, NULL, false, &binding,
	    "/nkn/nvsd/network/config/max_connections", NULL);
    bail_error(err);

    err = bn_binding_get_uint32(binding, ba_value, NULL, &max_network_conn);
    bail_error(err);

    *max_conn = max_network_conn;
    if (*max_conn > MAX_GNM) {
	*max_conn = MAX_GNM;
    }

    lc_log_debug(jnpr_log_level, "network conn  - %lu", *max_conn);

bail:
    bn_binding_free(&binding);
    return err;
}

int
nvsd_rsrc_update_event(const bn_binding_array * arr,
	uint32 idx, bn_binding * binding, void *data)
{
    int err = 0;
    char *str = NULL;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);
    UNUSED_ARGUMENT(data);

    bn_binding_dump(jnpr_log_level, __FUNCTION__, binding);

    err = bn_binding_get_str(binding, ba_value, 0, NULL, &str);
    bail_error_null(err, str);

    if (0 == strcmp(str, "connection")) {
	nvsd_rp_set_total(RESOURCE_POOL_TYPE_CLIENT_SESSION, 1);
    } else if (0 == strcmp(str, "bandwidth")) {
	nvsd_rp_set_total(RESOURCE_POOL_TYPE_BW, 1);
    } else {
	lc_log_basic(LOG_NOTICE, "unknown resource-type (%s)", str);
	goto bail;
    }

bail:
    safe_free(str);
    return err;

}
/* ------------------------------------------------------------------------- */
/* End of nkn_mgmt_resource_mgr.c */
