/*
 *
 * Filename:  watchdog_config.c
 * Date:      2010/03/25 
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008-2010 Ankeena Networks, Inc.  
 * All rights reserved.
 *
 */

#include "md_client.h"
#include "watchdog_main.h"
#include "license.h"
#include "proc_utils.h"
#include "bnode.h"
#include "file_utils.h"
#include <string.h>
#include <stdlib.h>
#include "watchdog.h"

#define PROBE_LISTEN_INTERFACE "lo"

static const char *watchdog_locale_binding = "/system/locale/global";
//static const char *watchdog_probe_listen_interface = "/web/httpd/listen/interface/lo";
static const char *watchdog_httpd = "/web/httpd";
static const char *watchdog_watchdog_binding = "/nkn/watch_dog/config";
static const char *watchdog_network_thread_num = "/nkn/nvsd/network/config/threads";
static const char *watchdog_hung_count_num = "/pm/process/nvsd/liveness/hung_count";
static const char *watchdog_grace_period = "/pm/failure/liveness/grace_period";
static const char *watchdog_network_param = "/nkn/nvsd/network/config";
static const char *watchdog_mfc_probe = "/nkn/nvsd/namespace/mfc_probe";
static const char *watchdog_nvsd_http_cfg = "/nkn/nvsd/http/config";
static const char *watchdog_nvsd_sys_coredump = "/nkn/nvsd/system/config/coredump";

int watchdog_set_watcdog_ns_origin(const char *intf, tbool *mgmt_intf_set);

/* Global */
extern nvsd_live_check_t* watchdog_get_nvsd_alarm(const char *name);
static nvsd_live_check_t* watchdog_get_nvsd_alarm_from_binding(bn_binding *alarm_binding);
extern int probe_dbg_msg;

extern uint32_t nvsd_mem_soft_limit;
extern uint32_t nvsd_mem_hard_limit;
extern char *wd_nvsd_pid;
/* ------------------------------------------------------------------------- */
int
watchdog_config_query(void)
{
    int err = 0;
    bn_binding_array *bindings = NULL;
    tbool rechecked_licenses = false;

    /* Also Monitor watch-dog nodes */
    err = mdc_get_binding_children(watchdog_mcc,
	    NULL, NULL, true, &bindings, false, true,
	    watchdog_watchdog_binding);
    bail_error_null(err, bindings);
    err = bn_binding_array_foreach(bindings, watchdog_config_handle_change,
	    &rechecked_licenses);
    bail_error(err);
    bn_binding_array_free(&bindings);


    /* Get the network thread number config */
    err = mdc_get_binding_children(watchdog_mcc,
	    NULL, NULL, true, &bindings, true, true,
	    watchdog_network_thread_num);
    bail_error_null(err, bindings);
    err = bn_binding_array_foreach(bindings, watchdog_config_handle_change,
	    &rechecked_licenses);
    bail_error(err);
    bn_binding_array_free(&bindings);

    /*Read the coredump enable status */
    err = mdc_get_binding_children(watchdog_mcc,
	    NULL, NULL, true, &bindings, true, true,
	    watchdog_nvsd_sys_coredump);
    bail_error_null(err, bindings);
    err = bn_binding_array_foreach(bindings, watchdog_config_handle_change,
	    &rechecked_licenses);
    bail_error(err);
    bn_binding_array_free(&bindings);



    /* Get the hung-count number */
    err = mdc_get_binding_children(watchdog_mcc,
	    NULL, NULL, true, &bindings, true, true,
	    watchdog_hung_count_num);
    bail_error_null(err, bindings);
    err = bn_binding_array_foreach(bindings, watchdog_config_handle_change,
	    &rechecked_licenses);
    bail_error(err);
    bn_binding_array_free(&bindings);

    /* Get the grace_period number */
    err = mdc_get_binding_children(watchdog_mcc,
	    NULL, NULL, true, &bindings, true, true,
	    watchdog_grace_period);
    bail_error_null(err, bindings);
    err = bn_binding_array_foreach(bindings, watchdog_config_handle_change,
	    &rechecked_licenses);
    bail_error(err);
    bn_binding_array_free(&bindings);

    /* Get the syn-cookie values */
    err = mdc_get_binding_children(watchdog_mcc,
	    NULL, NULL, true, &bindings, true, true,
	    watchdog_network_param);
    bail_error_null(err, bindings);
    err = bn_binding_array_foreach(bindings, watchdog_config_handle_change,
	    &rechecked_licenses);
    bail_error(err);
    bn_binding_array_free(&bindings);

    /* Get the web-hhtpd values */
    /* TODO - move the handle change specific things in anew function */
    err = mdc_get_binding_children(watchdog_mcc,
	    NULL, NULL, true, &bindings, true, true,
	    watchdog_httpd);
    bail_error_null(err, bindings);
    err = bn_binding_array_foreach(bindings, watchdog_config_handle_change,
	    &rechecked_licenses);
    bail_error(err);
    bn_binding_array_free(&bindings);

    /* if no interface has been allocated, use "eth0" */
    if (watchdog_config.mgmt_intf == NULL) {
	/* this variable is unused */
	tbool is_mgmtintf_set = false;
	lc_log_basic(LOG_INFO, "using eth0 for watchdog");
	watchdog_set_watcdog_ns_origin("eth0", &is_mgmtintf_set);
	if(watchdog_config.mgmt_intf)
	    safe_free(watchdog_config.mgmt_intf);

	watchdog_config.mgmt_intf = strdup("eth0");
    }
    /* Get the probe namespace active */
    err = mdc_get_binding_children(watchdog_mcc,
	    NULL, NULL, true, &bindings, true, true,
	    watchdog_mfc_probe);
    bail_error_null(err, bindings);
    err = bn_binding_array_foreach(bindings, watchdog_config_handle_change,
	    &rechecked_licenses);
    bail_error(err);
    bn_binding_array_free(&bindings);

    /* Get the nvsd listen port */
    err = mdc_get_binding_children(watchdog_mcc,
	    NULL, NULL, true, &bindings, true, true,
	    watchdog_nvsd_http_cfg);
    bail_error_null(err, bindings);
    err = bn_binding_array_foreach(bindings, watchdog_config_handle_change,
	    &rechecked_licenses);
    bail_error(err);


bail:
    bn_binding_array_free(&bindings);
    return(err);
}


/* ------------------------------------------------------------------------- */
int
watchdog_config_handle_change(const bn_binding_array *arr, uint32 idx,
                        bn_binding *binding, void *data)
{
    int err = 0;
    const tstring *name = NULL;
    tstr_array *name_parts = NULL;
    const char *t_mfd_index_str = NULL;
    uint32_t mfd_index;
    uint32 retcode = 0;
    tstring *retmsg = NULL;
    tstring *t_status = NULL;
    int32 status = 0;
    tstring *ret_output = NULL;
    tstring *t_value = NULL;
    tbool *rechecked_licenses_p = data;
    tstring *t_mfcprobe_host = NULL;
    tstring *t_mfcprobe_port = NULL;

    err = bn_binding_get_name(binding, &name);
    bail_error(err);

   if (bn_binding_name_pattern_match (ts_str(name), "/nkn/watch_dog/config/nvsd/restart")) {
	lc_log_basic(LOG_DEBUG, "watch_dog restart");

	err = bn_binding_get_tstr(binding, ba_value, bt_bool, 0,&t_status);
	bail_error(err);

	if(!strcmp("true", ts_str(t_status))) {
	    watchdog_config.restart_nvsd = true;
	    lc_log_basic(LOG_DEBUG,"resttting as true");
	}
	else {
	    watchdog_config.restart_nvsd = false;
	    lc_log_basic(LOG_DEBUG, "settting as false");
	}
    }
    else if (bn_binding_name_pattern_match (ts_str(name), "/nkn/watch_dog/config/debug/enable")) {
	tbool is_dbg_enabled = false;

	err = bn_binding_get_tbool(binding, ba_value, 0, &is_dbg_enabled);
	bail_error(err);

	if(is_dbg_enabled) {
	    probe_dbg_msg = LOG_NOTICE;
	} else {
	    probe_dbg_msg = LOG_DEBUG;
	}
    }
    else if (bn_binding_name_pattern_match (ts_str(name), "/nkn/watch_dog/config/debug/gdb_ena")) {
	tbool is_gdb_enabled = false;

	err = bn_binding_get_tbool(binding, ba_value, 0, &is_gdb_enabled);
	bail_error(err);

	watchdog_config.is_gdb_ena = is_gdb_enabled;
    }
    else if (bn_binding_name_pattern_match (ts_str(name), "/nkn/watch_dog/config/nvsd/mem/softlimit")) {
	uint32_t soft_limit = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &soft_limit);
	bail_error(err);

	nvsd_mem_soft_limit = soft_limit;

    }
    else if (bn_binding_name_pattern_match (ts_str(name), "/nkn/watch_dog/config/nvsd/mem/hardlimit")) {
	uint32_t hard_limit = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &hard_limit);
	bail_error(err);

	nvsd_mem_hard_limit = hard_limit;

    }

    else if (bn_binding_name_pattern_match (ts_str(name), "/nkn/watch_dog/config/nvsd/alarms/*/enable")) {
	tbool is_enabled = false;
	nvsd_live_check_t *probe_alarm = NULL;

	/*Get the check Function */
	probe_alarm = watchdog_get_nvsd_alarm_from_binding(binding);
	bail_null(probe_alarm);

	err = bn_binding_get_tbool(binding, ba_value, 0, &is_enabled);
	bail_error(err);

	probe_alarm->enabled = is_enabled;
    }
    else if (bn_binding_name_pattern_match (ts_str(name), "/nkn/watch_dog/config/nvsd/alarms/*/threshold")) {
	uint32 threshold = 0;
	nvsd_live_check_t *probe_alarm = NULL;

	/*Get the check Function */
	probe_alarm = watchdog_get_nvsd_alarm_from_binding(binding);
	bail_null(probe_alarm);

	err = bn_binding_get_uint32(binding, ba_value, NULL, &threshold);
	bail_error(err);

	probe_alarm->threshold = threshold;

    }
    else if (bn_binding_name_pattern_match (ts_str(name), "/nkn/watch_dog/config/nvsd/alarms/*/poll_freq")) {
	uint32_t poll_freq = 0;
	nvsd_live_check_t *probe_alarm = NULL;

	/*Get the check Function */
	probe_alarm = watchdog_get_nvsd_alarm_from_binding(binding);
	bail_null(probe_alarm);

	err = bn_binding_get_uint32(binding, ba_value, NULL, &poll_freq);
	bail_error(err);

	probe_alarm->poll_time = poll_freq;

    }
    else if (bn_binding_name_pattern_match (ts_str(name), "/nkn/watch_dog/config/nvsd/alarms/*/action")) {
	uint64_t action = 0;
	nvsd_live_check_t *probe_alarm = NULL;

	/*Get the check Function */
	probe_alarm = watchdog_get_nvsd_alarm_from_binding(binding);
	bail_null(probe_alarm);

	err = bn_binding_get_uint64(binding, ba_value, NULL, &action);
	bail_error(err);

	probe_alarm->wd_action = action;

    }
    else if (bn_binding_name_pattern_match (ts_str(name), "/nkn/nvsd/network/config/threads")) {

	uint32 num_threads = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &num_threads);
	bail_error(err);
	lc_log_basic(LOG_DEBUG, "network thread change = %u", num_threads);

	/* Default is two */
	if(num_threads == 0)
	{
	    watchdog_config.num_network_threads = 2;
	} else {
	    watchdog_config.num_network_threads = num_threads;
	}
    }
    else if (bn_binding_name_pattern_match (ts_str(name), "/pm/process/nvsd/liveness/hung_count")) {

	uint32 num_hung_count = 0;

	err= bn_binding_get_uint32(binding, ba_value, NULL, &num_hung_count);
	bail_error(err);
	lc_log_basic(LOG_DEBUG, "hung_count = %u", num_hung_count);

	watchdog_config.hung_count = num_hung_count;
    }
    else if (bn_binding_name_pattern_match (ts_str(name), "/pm/failure/liveness/grace_period")) {

	lt_time_sec grace_period = 0;

	err= bn_binding_get_duration_sec(binding, ba_value, NULL, &grace_period);
	bail_error(err);

	watchdog_config.grace_period = grace_period;
    }

    else if (bn_binding_name_pattern_match (ts_str(name), "/nkn/nvsd/network/config/max_connections")) {

	uint32 num_lic_sock = 0;

	err= bn_binding_get_uint32(binding, ba_value, NULL, &num_lic_sock);
	bail_error(err);
	lc_log_basic(LOG_DEBUG, "num_lic_sock = %u", num_lic_sock);

	watchdog_config.num_lic_sock = num_lic_sock;
    }
    else if (bn_binding_name_pattern_match (ts_str(name), "/web/httpd/http/enable")) {

	tbool is_enabled = false;

	err = bn_binding_get_tbool(binding, ba_value, 0, &is_enabled);
	bail_error(err);

	watchdog_config.is_httpd_enabled = is_enabled;
    }
    else if (bn_binding_name_pattern_match (ts_str(name), "/web/httpd/listen/enable")) {

	tbool is_enabled = false;

	err = bn_binding_get_tbool(binding, ba_value, 0, &is_enabled);
	bail_error(err);

	watchdog_config.is_httpd_listen_enabled = is_enabled;

    }
    else if (bn_binding_name_pattern_match (ts_str(name), "/web/httpd/http/port")) {

	uint16 httpd_listen_port = 0;
	char *str_listen_port = NULL;

	err = bn_binding_get_uint16(binding, ba_value, NULL, &httpd_listen_port);
	bail_error(err);

	if(watchdog_config.httpd_listen_port)
	    safe_free(watchdog_config.httpd_listen_port);

	str_listen_port = smprintf("%u", httpd_listen_port);
	watchdog_config.httpd_listen_port = str_listen_port;

	err = mdc_set_binding(watchdog_mcc, NULL, NULL,
		bsso_modify, bt_uint16,
		str_listen_port, "/nkn/nvsd/namespace/mfc_probe/origin-server/http/host/port");
	lc_log_basic(LOG_NOTICE, "setting the port for probe namepace as %s\n", str_listen_port);
	bail_error(err);
	//Dont free here,used in config
	//safe_free(str_listen_port);
    }
    else if (bn_binding_name_pattern_match (ts_str(name), "/web/httpd/listen/interface/**")) {

	tstr_array *mgmt_intfs = NULL;
	const char *mgmt_first_intf = NULL;
	tbool is_mgmtintf_set = false;
	uint32_t i = 0;

	/* make it to false here ,whenever we get a change
	 * if a mgmt int if found to be valid,
	 * watchdog_set_watcdog_ns_origin will set the is mgmtinft is found
	 * in the watch dog config.
	 */
	watchdog_config.has_mgmt_intf = false;

	/* get the listen interface and conifgure the ip address of the interface
	 * as the origin for the mfc_probe namespace
	 */


	err = mdc_get_binding_children_tstr_array(watchdog_mcc,
		NULL, NULL, &mgmt_intfs,
		"/web/httpd/listen/interface", NULL);
	bail_error(err);

	for( i = 0; i< tstr_array_length_quick(mgmt_intfs) ; i++){

	    mgmt_first_intf = tstr_array_get_str_quick(mgmt_intfs, i);
	    bail_null(mgmt_first_intf);

	    watchdog_set_watcdog_ns_origin(mgmt_first_intf, &is_mgmtintf_set);
	    if(watchdog_config.mgmt_intf)
		safe_free(watchdog_config.mgmt_intf);

	    watchdog_config.mgmt_intf = strdup(mgmt_first_intf);
	    if(is_mgmtintf_set) {
		break;
	    }
	}
	/*
	 * if no interfaces are configured, then httpd will listen on all
	 * interfaces. So using "eth0".
	 */
	if (tstr_array_length_quick(mgmt_intfs) == 0) {
	    lc_log_basic(LOG_INFO, "using eth0 for watchdog");
	    watchdog_set_watcdog_ns_origin("eth0", &is_mgmtintf_set);
	    if(watchdog_config.mgmt_intf)
		safe_free(watchdog_config.mgmt_intf);

	    watchdog_config.mgmt_intf = strdup("eth0");
	}
	tstr_array_free(&mgmt_intfs);
    } else if(bn_binding_name_pattern_match (ts_str(name), "/nkn/nvsd/namespace/mfc_probe/status/active")) {
	/* when namespace is made inactive dont send probe */
	tbool is_enabled = false;

	err = bn_binding_get_tbool(binding, ba_value, 0, &is_enabled);
	bail_error(err);

	watchdog_config.is_httpd_enabled = is_enabled;
    } else if(bn_binding_name_pattern_match (ts_str(name), "/nkn/nvsd/namespace/mfc_probe/origin-server/http/host/name")) {

	err = bn_binding_get_tstr(binding, ba_value, bt_string, 0, &t_mfcprobe_host);
	bail_error(err);

	if(watchdog_config.probe_o_host)
	    safe_free(watchdog_config.probe_o_host);
	if(t_mfcprobe_host)
	    watchdog_config.probe_o_host = strdup(ts_str(t_mfcprobe_host));


    } else if(bn_binding_name_pattern_match (ts_str(name), "/nkn/nvsd/namespace/mfc_probe/origin-server/http/host/port")) {

	err = bn_binding_get_tstr(binding, ba_value, bt_uint16, 0, &t_mfcprobe_port);
	bail_error(err);

	if(watchdog_config.probe_o_port)
	    safe_free(watchdog_config.probe_o_port);
	if(t_mfcprobe_port)
	    watchdog_config.probe_o_port = strdup(ts_str(t_mfcprobe_port));

    }
    else if(bn_binding_name_pattern_match (ts_str(name), "/nkn/nvsd/namespace/mfc_probe/domain/host")) {

	tstring *t_domain = NULL;

	err = bn_binding_get_tstr(binding, ba_value, bt_string, 0, &t_domain);
	bail_error(err);

	if(watchdog_config.domain)
	    safe_free(watchdog_config.domain);
	if(t_domain)
	    watchdog_config.domain = strdup(ts_str(t_domain));

	ts_free(&t_domain);
    }  else if(bn_binding_name_pattern_match (ts_str(name), "/nkn/nvsd/http/config/server_port/*/port")) {
	uint16 nvsd_listen_port = 0;

	err = bn_binding_get_uint16(binding, ba_value, NULL, &nvsd_listen_port);
	bail_error(err);

	watchdog_config.nvsd_listen_port = nvsd_listen_port;

	if(watchdog_config.probe_uri)
	    safe_free(watchdog_config.probe_uri);

	watchdog_config.probe_uri = smprintf("http://127.0.0.1:%d/mfc_probe/mfc_probe_object.dat", nvsd_listen_port);

    } else if(bn_binding_name_pattern_match (ts_str(name), "/nkn/nvsd/system/config/coredump")) {
	uint32 core_dump_enable = 0;

	err = bn_binding_get_uint32(binding, ba_value, NULL, &core_dump_enable);
	bail_error(err);

	watchdog_config.coredump_enabled = core_dump_enable;
    }


bail:
    tstr_array_free(&name_parts);
    ts_free(&retmsg);
    ts_free(&t_status);
    ts_free(&ret_output);
    ts_free(&t_mfcprobe_host);
    ts_free(&t_mfcprobe_port);
    return(err);
}

int watchdog_set_watcdog_ns_origin(const char *intf, tbool *mgmt_intf_set)
{
    char ip_node[256] = {0};
    tstring *intf_ipaddr = NULL;
    int err = 0;

    *mgmt_intf_set = false;
    snprintf(ip_node, sizeof(ip_node), "/net/interface/state/%s/addr/ipv4/1/ip", intf);

    err = mdc_get_binding_tstr(watchdog_mcc, NULL, NULL, NULL,
	    &intf_ipaddr, ip_node, NULL);
    bail_error(err);


    if(intf_ipaddr) {
	if(watchdog_config.mgmt_ip)
	    safe_free(watchdog_config.mgmt_ip);

	watchdog_config.mgmt_ip = strdup(ts_str(intf_ipaddr));

	err = mdc_set_binding(watchdog_mcc, NULL, NULL,
		bsso_modify, bt_string,
		ts_str(intf_ipaddr), "/nkn/nvsd/namespace/mfc_probe/origin-server/http/host/name");
	bail_error(err);
	lc_log_basic(LOG_NOTICE, "Has active mgmt interface");
	watchdog_config.has_mgmt_intf = true;
	*mgmt_intf_set = true;

    }

bail:
    ts_free(&intf_ipaddr);
    return err;

}

static nvsd_live_check_t*
watchdog_get_nvsd_alarm_from_binding(bn_binding *alarm_binding)
{
    int err = 0;
    tstr_array *name_parts = NULL;
    const char *alarm_name = NULL;

    nvsd_live_check_t *probe_alarm = NULL;

    err = bn_binding_get_name_parts (alarm_binding, &name_parts);
    bail_error_null(err, name_parts);

    alarm_name = tstr_array_get_str_quick (name_parts, 5);
    bail_null(alarm_name);

    /*Get the check Function */
    probe_alarm = watchdog_get_nvsd_alarm(alarm_name);
    bail_null(probe_alarm);

bail:
    tstr_array_free(&name_parts);
    return probe_alarm;

}
/* End of watchdog_config.c */
