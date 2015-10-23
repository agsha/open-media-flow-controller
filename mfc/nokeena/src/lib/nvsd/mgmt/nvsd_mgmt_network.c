/*
 *
 * Filename:  nkn_mgmt_network.c
 * Date:      2008/11/23 
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.  
 * All rights reserved.
 *
 *
 */

/* Samara Headers */
#include "md_client.h"
#include "license.h"

/* NVSD headers */
#include "nkn_defs.h"
#include "nvsd_mgmt.h"
#include "nkn_cfg_params.h"
#include "network.h"
#include "nvsd_resource_mgr.h"
#include "nkn_memalloc.h"
#include "libnknmgmt.h"
#include "om_port_mapper.h"
#include "nvsd_mgmt_lib.h"

extern unsigned int jnpr_log_level;

/* Local Function Prototypes */
int nvsd_network_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings);

int nvsd_network_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * b_value, void *data);

/* Extern Variables */
extern md_client_context *nvsd_mcc;
extern void NM_reinit_callback(void
	);
struct om_portmap_conf om_pmap_config;

const char network_config_prefix[] = "/nkn/nvsd/network/config";

/* Local Variables */
static const char network_threads_binding[] =
"/nkn/nvsd/network/config/threads";
static const char network_policy_binding[] = "/nkn/nvsd/network/config/policy";
static const char network_stack_binding[] = "/nkn/nvsd/network/config/stack";
static const char network_timeout_binding[] =
"/nkn/nvsd/network/config/timeout";
static const char network_afr_binding[] =
"/nkn/nvsd/network/config/assured_flow_rate";
static const char network_afr_fs_binding[] =
"/nkn/nvsd/network/config/afr_fast_start";
static const char network_max_bandwidth_binding[] =
"/nkn/nvsd/network/config/max_bandwidth";
static const char network_maxconnections_binding[] =
"/nkn/nvsd/network/config/max_connections";
static const char network_portmapper_binding[] =
"/nkn/nvsd/network/config/max_connections/pmapper-disable";
static const char network_tunnel_only_binding[] =
"/nkn/nvsd/network/config/tunnel-only";
static const char network_resolver_adns_binding[] =
"/nkn/nvsd/network/config/resolver/async-enable";
static const char network_resolver_cache_timeout_binding[] =
"/nkn/nvsd/network/config/resolver/cache-timeout";
static const char network_resolver_adnsd_enabled_binding[] =
"/nkn/nvsd/network/config/resolver/adnsd-enabled";
static const char network_origin_queue_delay_binding[] =
"/nkn/nvsd/network/config/connection/origin/queue/max_delay";
static const char network_origin_queue_num_binding[] =
"/nkn/nvsd/network/config/connection/origin/queue/max_num";
static const char network_bond_if_cfg_binding[] =
"/nkn/nvsd/network/config/bond_if_cfg_enable";

/* Failover use-dns-response config*/
static const char network_origin_fo_use_dns_binding[] =
"/nkn/nvsd/network/config/connection/origin/failover/use_dns_response";
static const char network_origin_connect_timeout_binding[] =
"/nkn/nvsd/network/config/connection/origin/connect/timeout";

static const char portmapper_port_range_min[] =
"/nkn/nvsd/network/config/connection/transparent-proxy/port_range_min";
static const char portmapper_port_range_max[] =
"/nkn/nvsd/network/config/connection/transparent-proxy/port_range_max";


static const char network_origin_max_unresponsive_conns_binding[] = "/nkn/nvsd/network/config/connection/origin/max-unresponsive-conns";
/* Extern Functions */
extern void update_interface_bandwidth(const char *if_name, uint32_t if_speed);

/* glob variable */
uint32_t glob_network_num_threads = 0;

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_network_cfg_query()
 *	purpose : query for network config parameters
 */
int
nvsd_network_cfg_query(const bn_binding_array * bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    lc_log_basic(LOG_DEBUG, "nvsd network module mgmtd query initializing ..");

    err = mgmt_lib_mdc_foreach_binding_prequeried(bindings,
	    network_config_prefix,
	    NULL,
	    nvsd_network_cfg_handle_change,
	    &rechecked_licenses);
    bail_error(err);

bail:
    return err;

}	/* end of nvsd_network_cfg_query() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_network_module_cfg_handle_change()
 *	purpose : handler for config changes for network module
 */
int
nvsd_network_module_cfg_handle_change(bn_binding_array * old_bindings,
	bn_binding_array * new_bindings)
{
    int err = 0;
    tbool rechecked_licenses = false;

    UNUSED_ARGUMENT(old_bindings);

    /*
     * Call the callbacks 
     */
    err = mgmt_lib_mdc_foreach_binding_prequeried(new_bindings,
	    network_config_prefix,
	    NULL,
	    nvsd_network_cfg_handle_change,
	    &rechecked_licenses);
    bail_error(err);
bail:
    return err;

}	/* end of * nvsd_network_module_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_network_cfg_handle_change()
 *	purpose : handler for config changes for network module
 */
int
nvsd_network_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	const bn_binding * binding,
	const tstring * bndgs_name,
	const tstr_array * bndgs_name_components,
	const tstring * bndgs_name_last_part,
	const tstring * b_value, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    uint32_t num_cpus = 0;
    tbool *rechecked_licenses_p = data;
    bn_binding_array *bindings = NULL;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);

    UNUSED_ARGUMENT(bndgs_name);
    UNUSED_ARGUMENT(bndgs_name_components);
    UNUSED_ARGUMENT(bndgs_name_last_part);
    UNUSED_ARGUMENT(b_value);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    /*-------- NETWORK THREADS ------------*/
    if (ts_equal_str(name, network_threads_binding, false)) {
	uint32 threads = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &threads);
	bail_error(err);

	/*
	 * If config value is 0 then it means auto-tune 
	 */
	if (!threads) {
	    /*
	     * Get the number of cores first 
	     */
	    err = mdc_get_binding_children(nvsd_mcc,
		    NULL, NULL, true, &bindings, false,
		    false, "/system/cpu/indiv");
	    bail_error(err);

	    if (bindings) {
		err = bn_binding_array_length(bindings, &num_cpus);
		bail_error(err);
		bn_binding_array_free(&bindings);
	    }

	    /*
	     * 3 network threads for 4 cpus 
	     */
	    if (num_cpus >= 4) {
		threads = (num_cpus / 4) * 3;
		if (nm_hdl_send_and_receive == 0) {
		    threads = threads / 2;
		}
	    } else {
		threads = 2;
	    }
	}

	if (NM_tot_threads <= 32 /* MAX_EPOLL_THREADS */  && NM_tot_threads > 0) {
	    NM_tot_threads = threads;
	    glob_network_num_threads = NM_tot_threads;
	}
	lc_log_basic(LOG_DEBUG, "Read .../network/config/threads as : \"%d\"",
		threads);
    }

    /*-------- NETWORK POLICY ------------*/
    else if (ts_equal_str(name, network_policy_binding, false)) {
	uint32 policy = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &policy);

	bail_error(err);
	nm_lb_policy = policy;
	lc_log_basic(LOG_DEBUG, "Read .../network/config/policy as : \"%d\"",
		policy);
    }

    /*-------- TIMEOUT ------------*/
    else if (ts_equal_str(name, network_timeout_binding, false)) {
	uint32 timeout = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &timeout);

	bail_error(err);
	http_idle_timeout = timeout;
	lc_log_basic(LOG_DEBUG, "Read .../network/config/timeout as : \"%d\"",
		timeout);
    }

    /*-------- MAXCONNECTIONS ------------*/
    else if (ts_equal_str(name, network_maxconnections_binding, false)) {
	uint32 maxconnections = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &maxconnections);

	/*
	 * NOTE - any changes done here should also be done in
	 * get_total_network_conn() 
	 */

	bail_error(err);
	/*
	 * Adjust nkn_max_client to have some buffer room. 
	 */
	nkn_max_client = maxconnections + 200;
	if (nkn_max_client > MAX_GNM) {
	    // Up to 64K connections.
	    nkn_max_client = MAX_GNM;
	}
	lc_log_debug(jnpr_log_level, "max-conn- %d", nkn_max_client);
	// nvsd_rp_set_total(RESOURCE_POOL_TYPE_CLIENT_SESSION,
	// nkn_max_client);
	NM_reinit_callback();
	lc_log_basic(LOG_DEBUG,
		"Read .../network/config/max_connections as : \"%d\"",
		maxconnections);
    } else if (ts_equal_str(name, portmapper_port_range_min, false)) {
	err =
	    bn_binding_get_uint16(binding, ba_value, NULL,
		    &u16_g_port_mapper_min);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../portmapper/config/port_range_min as: \"%hu\"",
		u16_g_port_mapper_min);
    } else if (ts_equal_str(name, portmapper_port_range_max, false)) {
	err =
	    bn_binding_get_uint16(binding, ba_value, NULL,
		    &u16_g_port_mapper_max);
	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../portmapper/config/port_range_max as: \"%hu\"",
		u16_g_port_mapper_max);
    }

    /*-------- ENABLING/DISABLING PORT MAPPER ------------*/
    else if (ts_equal_str(name, network_portmapper_binding, false)) {
	tbool t_pmapper_disable = false;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_pmapper_disable);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../network/config/max_connections/port-mapper-disable as : %d",
		t_pmapper_disable);

	/*
	 * Set the global variable with the value 
	 */
	if (t_pmapper_disable) {
	    om_pmap_config.pmapper_disable = 1;
	} else
	    om_pmap_config.pmapper_disable = 0;
    }

    /*-------- SESSION ASSURED FLOW RATE  ------------*/
    else if (ts_equal_str(name, network_afr_binding, false)) {
	uint32 t_session_assured_flow_rate = 0;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL,
		&t_session_assured_flow_rate);

	bail_error(err);
	sess_assured_flow_rate = (t_session_assured_flow_rate * KBYTES) / 8;
	NM_reinit_callback();
	lc_log_basic(LOG_DEBUG,
		"Read .../network/config/assured_flow_rate as : \"%d\"",
		t_session_assured_flow_rate);
    }

    /*-------- SESSION ASSURED FLOW RATE FAST START  ------------*/
    else if (ts_equal_str(name, network_afr_fs_binding, false)) {
	uint32 t_session_afr_faststart = 0;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL, &t_session_afr_faststart);

	bail_error(err);
	sess_afr_faststart = t_session_afr_faststart;
	lc_log_basic(LOG_DEBUG,
		"Read .../network/config/afr_fast_start as : \"%d\"",
		t_session_afr_faststart);
    }

    /*-------- SESSION MAX BANDWIDTH ------------*/
    else if (ts_equal_str(name, network_max_bandwidth_binding, false)) {
	uint32 t_session_max_bandwidth = 0;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL, &t_session_max_bandwidth);

	bail_error(err);
	sess_bandwidth_limit = (t_session_max_bandwidth * KBYTES) / 8;

	lc_log_basic(LOG_DEBUG,
		"Read .../network/config/max_bandwidth as : \"%d\"",
		t_session_max_bandwidth);
    }

    /*-------- STACK ------------*/
    else if (ts_equal_str(name, network_stack_binding, false)) {
	tstring *stack = NULL;

	err = bn_binding_get_tstr(binding, ba_value, bt_string, NULL, &stack);

	bail_error(err);
	if (strcmp(ts_str(stack), "nokeena_user_space_stack") == 0) {
	    net_use_nkn_stack = 1;
	} else {
	    net_use_nkn_stack = 0;
	}

	lc_log_basic(LOG_DEBUG, "Read .../network/config/stack as : %s",
		ts_str(stack));
	ts_free(&stack);
    }

    /*-------- TUNNEL_ONLY ------------*/
    else if (ts_equal_str(name, network_tunnel_only_binding, false)) {
	tbool tunnel_only;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &tunnel_only);
	bail_error(err);

	lc_log_basic(LOG_DEBUG, "Read .../network/config/tunnel-only as : %d",
		tunnel_only);

	/*
	 * Set the global variable with the value 
	 */
	tunnel_only_enable = tunnel_only;
    }

    /*-------- RESOLVER ADNS ENABLE ------------*/
    else if (ts_equal_str(name, network_resolver_adns_binding, false)) {
	tbool t_adns_enabled;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_adns_enabled);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../network/config/resolver/async-enable as : %d",
		t_adns_enabled);

	/*
	 * Set the global variable with the value 
	 */
	adns_enabled = (t_adns_enabled ? 1 : 0);
    }

    /*-------- RESOLVER CACHE-TIMEOUT ------------*/
    else if (ts_equal_str(name, network_resolver_cache_timeout_binding, false)) {
	int32_t t_cache_timeout;

	err = bn_binding_get_int32(binding, ba_value, NULL, &t_cache_timeout);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../network/config/resolver/cache-timeout as : %d",
		t_cache_timeout);

	/*
	 * Set the global variable with the value 
	 */
	glob_adns_cache_timeout = t_cache_timeout;
    }

    /*-------- RESOLVER ADNS DAEMON ENABLE ------------*/
    else if (ts_equal_str(name, network_resolver_adnsd_enabled_binding, false)) {
	tbool t_adnsd_enabled;

	err = bn_binding_get_tbool(binding, ba_value, NULL, &t_adnsd_enabled);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../network/config/resolver/adnsd-enabled as : %d",
		t_adnsd_enabled);

	/*
	 * Set the global variable with the value 
	 */
	adnsd_enabled = (t_adnsd_enabled ? 1 : 0);
    }

    /*-------- Origin queue max max-delay ------------*/
    else if (ts_equal_str(name, network_origin_queue_delay_binding, false)) {
	uint32_t t_origin_q_max_delay;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL, &t_origin_q_max_delay);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../nkn/nvsd/network/config/connection/origin/queue/max_delay as : %d",
		t_origin_q_max_delay);

	/*
	 * Set the global variable with the value 
	 */
	bm_cfg_cr_max_queue_time = t_origin_q_max_delay;
    } else if (ts_equal_str(name, network_origin_queue_num_binding, false)) {
	/*
	 * This variable control the number of requests for same object buffer
	 * manager will hold till max_delay seconds, after that, requests
	 * will tunneled to origin server.
	 * if max-delay is 0, this parameter has no meaning.
	 */
	uint32_t t_origin_q_max_num;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL, &t_origin_q_max_num);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../network/config/connection/origin/queue/max_num as : %d",
		t_origin_q_max_num);

	/*
	 * Set the global variable with the value 
	 */
	bm_cfg_cr_max_req = t_origin_q_max_num;
    } else if (ts_equal_str(name, network_origin_fo_use_dns_binding, false)) {
	/*
	 * The command specifies to enable Origin Server failover support and
	 * also internally enable multiple IP address lookup and storage support
	 * in DNS cache.
	 */
	tbool t_origin_fo_use_dns;

	err = bn_binding_get_tbool(binding,
		ba_value, NULL, &t_origin_fo_use_dns);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../network/config/connection/origin/failover/use_dns_response as : %d",
		t_origin_fo_use_dns);

	/*
	 * Set the global variable with the value 
	 */
	http_cfg_fo_use_dns = t_origin_fo_use_dns;
    } else if (ts_equal_str(name, network_origin_connect_timeout_binding,
		false)) {
	/*
	 * The command specifies to setup Origin Server Connection timeout value
	 * to be used while establishing connection with the Origin Server.
	 * During connection establishment, the origin connection would wait for
	 * <timeout_value> seconds before timing out and closing the connection.
	 */
	uint32_t t_origin_connect_to;

	err = bn_binding_get_uint32(binding,
		ba_value, NULL, &t_origin_connect_to);
	bail_error(err);
	lc_log_basic(LOG_DEBUG,
		"Read .../network/config/connection/origin/connect/timeout as : %d",
		t_origin_connect_to);

	/*
	 * Set the global variable with the value 
	 */
	http_cfg_origin_conn_timeout = t_origin_connect_to;
    }

    /*-------- BOND IF CONFIG ENABLE ------------*/
    else if (ts_equal_str(name, network_bond_if_cfg_binding, false)) {
	tbool t_bond_if_cfg_enabled;

	err = bn_binding_get_tbool(binding,
		ba_value, NULL, &t_bond_if_cfg_enabled);
	bail_error(err);

	lc_log_basic(LOG_DEBUG,
		"Read .../network/config/bond_if_cfg_enable as : %d",
		t_bond_if_cfg_enabled);

	/*
	 * Set the global variable with the value 
	 */
	bond_if_cfg_enabled = (t_bond_if_cfg_enabled ? 1 : 0);
    }

    else if (ts_equal_str(name, network_origin_max_unresponsive_conns_binding, false)) {
    /*
     * This variable controls the maximum number of unresponsive connnections per origin
     * if max-responsive-connections is 0, it is unlimited.
     */
	uint32_t    t_origin_max_unresponsive_connections;

	err = bn_binding_get_uint32(binding,
			ba_value, NULL,
			&t_origin_max_unresponsive_connections);
	bail_error(err);

	lc_log_basic(LOG_DEBUG, "Read .../network/config/connection/origin/max-unresponsive-conns : %d",
			t_origin_max_unresponsive_connections);

	/* Set the global variable with the value */
	max_unresp_connections = t_origin_max_unresponsive_connections;
    }

bail:
    return err;
}	/* end of nvsd_network_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/*
 *	funtion : nvsd_interface_up_cfg_handle_change()
 *	purpose : handler for config changes for interface up event
 */
int
nvsd_interface_up_cfg_handle_change(const bn_binding_array * arr,
	uint32 idx,
	bn_binding * binding, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    char *t_intfname = NULL;
    char *node_name = NULL;
    char *bond_intf = NULL;
    uint32_t if_speed = 0;
    tbool *rechecked_licenses_p = data;
    tbool link_up = false;
    tstr_array *alias_intf = NULL;
    uint32_t i = 0;
    const char *if_name = NULL;

    UNUSED_ARGUMENT(arr);
    UNUSED_ARGUMENT(idx);

    bail_null(rechecked_licenses_p);

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

    bn_binding_dump(jnpr_log_level, __FUNCTION__, binding);
    bn_binding_array_dump(__FUNCTION__, arr, jnpr_log_level);

    /*-------- INTERFACE NAME ------------*/
    if (bn_binding_name_pattern_match(ts_str(name), "/net/interface/state/*")) {

	err = bn_binding_get_str(binding,
		ba_value, bt_string, NULL, &t_intfname);
	bail_error_null(err, t_intfname);

	lc_log_basic(LOG_DEBUG, "Read /net/interface/state/* as : \"%s\"",
		t_intfname);

	/*
	 * Now get the link speed 
	 */
	if (nvsd_mgmt_is_bond_intf(t_intfname, nvsd_mcc)) {
	    /*
	     * check to see if the link state is up 
	     */
	    node_name =
		smprintf("/net/interface/state/%s/flags/link_up", t_intfname);
	    err =
		mdc_get_binding_tbool(nvsd_mcc, NULL, NULL, false, &link_up,
			node_name, NULL);
	    bail_error(err);

	    if (link_up) {
		uint32_t bw_Bps =
		    nvsd_mgmt_get_bond_intf_bw(t_intfname, nvsd_mcc);
		uint32_t Mbps = (bw_Bps * 8) / (1000 * 1000);

		if_speed = Mbps;
	    }
	} else {
	    if_speed = nvsd_mgmt_get_interface_link_speed(t_intfname, nvsd_mcc);
	}

	/*
	 * see if its bound to a bond interface,if so get the ifname too and
	 * adjust the bonded interfaces BW. 
	 */
	if (is_bound_to_a_bond(t_intfname, &bond_intf, nvsd_mcc)) {
	    if (bond_intf) {
		uint32_t bw_Bps =
		    nvsd_mgmt_get_bond_intf_bw(bond_intf, nvsd_mcc);
		uint32_t Mbps = (bw_Bps * 8) / (1000 * 1000);

		update_interface_bandwidth(bond_intf, Mbps);

		/*
		 * update the aliased bond interface too 
		 */
		alias_intf = nvsd_mgmt_get_alias_intf(bond_intf, nvsd_mcc);
		if (alias_intf) {
		    for (i = 0; i < tstr_array_length_quick(alias_intf); i++) {
			if_name = tstr_array_get_str_quick(alias_intf, i);
			update_interface_bandwidth(if_name, Mbps);
		    }
		    tstr_array_free(&alias_intf);
		}
		safe_free(bond_intf);
	    }
	}

	/*
	 * Now call the interface function to update the bandwidth 
	 */
	update_interface_bandwidth(t_intfname, if_speed);

	/*
	 * update the BW for all alias interfaces too 
	 */
	alias_intf = nvsd_mgmt_get_alias_intf(t_intfname, nvsd_mcc);
	if (alias_intf) {
	    for (i = 0; i < tstr_array_length_quick(alias_intf); i++) {
		if_name = tstr_array_get_str_quick(alias_intf, i);
		update_interface_bandwidth(if_name, if_speed);
	    }
	    tstr_array_free(&alias_intf);
	}

    }
bail:
    safe_free(t_intfname);
    safe_free(node_name);
    return err;
}	/* end of * nvsd_interface_up_cfg_handle_change() */

/* ------------------------------------------------------------------------- */

/* End of nkn_mgmt_network.c */
