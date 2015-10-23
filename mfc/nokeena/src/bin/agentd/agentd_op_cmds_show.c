/*
 * Filename :   agentd_op_cmds_show.c
 * Date:        29 Nov 2011
 * Author:
 *
 * (C) Copyright 2011, Juniper Networks, Inc.
 * All rights reserved.
 *
 */


#include "md_client.h"
#include "mdc_wrapper.h"
#include "bnode.h"
#include "signal_utils.h"
#include "libevent_wrapper.h"
#include "random.h"
#include "dso.h"
#include <ctype.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>
#include "agentd_mgmt.h"
#include "nkn_cfg_params.h"
#include <glib.h>
#include "agentd_utils.h"
#include "agentd_op_cmds_base.h"
#include "agentd_copied_code.h"
#include "ttime.h"
#include "juniper-media-flow-controller_odl.h"
#include "agentd_cli.h"

/*
    1050 Config request Failed
    1051 Unable to find Translational entry
    1052 lookup failed for operational command
*/
extern md_client_context * agentd_mcc;
extern int jnpr_log_level;

int get_system_version(agentd_context_t *context, void *data)
{
    int err = 0;
    oper_table_entry_t *cmd = (oper_table_entry_t *)data;
    uint32 node_flags = 0;
    uint32 node_subop = 0;
    char *node_name = NULL;
    bn_request *req = NULL;
    bn_response *resp = NULL;
    uint32_t ret_code = 0;
    tstring *ret_msg = NULL;
    int i = 0, num_qry_nodes = 0;
    bn_binding_array *resp_bindings = NULL;
    uint32 num_nodes = 0, nn = 0;
    const bn_binding *b = NULL;
    uint32 nnpa = 0;
    const tstring *bname = NULL;
    bn_type btype = bt_none;
    uint32 btype_flags = 0;
    char *str = NULL;
    char *eth0_ip = NULL;
    char *host = NULL;
    char *version = NULL;
    char *model = NULL;
    uint32 boot_loc = 0;
    uint32 standby_loc = 0;
    char *image[2] = {0};
    const bn_binding *binding = NULL;
    const bn_attrib_array *battribs = NULL;
    char *mgmt_ifname = NULL;
    tstring *t_mgmt_ip = NULL;
    const char *mgmt_ip= NULL;

    lc_log_debug(jnpr_log_level, "got %s", cmd->pattern);

    const char *nd_lst[] = {
  "/nkn/mgmt-if/config/if-name",
  "/net/interface/state/eth0/addr/ipv4/1/ip",
  "/system/hostname",
  "/system/version/release",
  "/system/model",
  "/image/boot_location/booted/location_id",
  "/image/info/installed_image/curr_device/location_id/1/build_prod_version",
  "/image/info/installed_image/curr_device/location_id/2/build_prod_version"};
    node_subop = bqso_get;
    node_flags = 0;
    err = bn_request_msg_create(&req,bbmt_query_request);
    bail_error(err);
    /* Now add the node's characteristics to the message */
    num_qry_nodes = sizeof(nd_lst)/sizeof (const char *);
    for(i = 0;i < num_qry_nodes; i++) {
  err = bn_query_request_msg_append_str(req,
    node_subop,
    node_flags,
    nd_lst[i], NULL);
  bail_error(err);
    }
    //bn_request_msg_dump(NULL, req, jnpr_log_level, "jnpr-agentd-dump");

    err = mdc_send_mgmt_msg_raw(agentd_mcc, req, &(resp));
    if (resp) {
  //bn_response_msg_dump(NULL, resp, jnpr_log_level, "jnpr-agentd-dump");
  //bail_error(err);
    }
    /*Setting the oDL tags in the xml format*/
    if (resp) {
  err = bn_response_get_return(resp, &ret_code, &ret_msg);
  complain_error(err);
  err = bn_response_get_takeover(resp, NULL, NULL, true,
    &resp_bindings, NULL);
  bail_error(err);

  num_nodes = bn_binding_array_length_quick(resp_bindings);
  if (num_nodes == 0) {
      goto bail;
  }
  for (nn = 0; nn < num_nodes; nn++) {
      binding = bn_binding_array_get_quick(resp_bindings, nn);
      if (!binding) {
    continue;
      }
      bname = NULL;
      err = bn_binding_get(binding, &bname, &battribs);
      bail_error_null(err, bname);
      bail_null(battribs);
      err = bn_binding_get_type(binding, ba_value, &btype, &btype_flags);
      bail_error(err);
      if(bn_binding_name_pattern_match(ts_str(bname), "/nkn/mgmt-if/config/if-name")) {
          err = bn_binding_get_str(binding, ba_value, bt_NONE, &btype_flags, &mgmt_ifname);
            if (mgmt_ifname) {
                err = mdc_get_binding_tstr_fmt(agentd_mcc, &ret_code, NULL, NULL, &t_mgmt_ip, NULL,
                       "/net/interface/state/%s/addr/ipv4/1/ip", mgmt_ifname);
                bail_error(err);
		if (!t_mgmt_ip) {
			lc_log_basic (LOG_NOTICE, "Management interface \'%s\' not configured. Returning eth0 ip.",
					mgmt_ifname);
		} else {
                        mgmt_ip = ts_str(t_mgmt_ip);
                }
            }
      }
      else if(bn_binding_name_pattern_match(ts_str(bname), "/net/interface/state/eth0/addr/ipv4/1/ip")) {
    err = bn_binding_get_str(binding, ba_value, bt_NONE, &btype_flags,
      &eth0_ip);
      }
      else if(bn_binding_name_pattern_match(ts_str(bname), "/system/hostname")) {
    err = bn_binding_get_str(binding, ba_value, bt_NONE, &btype_flags,
      &host);
      }
      else if(bn_binding_name_pattern_match(ts_str(bname), "/system/version/release")) {
    err = bn_binding_get_str(binding, ba_value, bt_NONE, &btype_flags,
      &version);
      }
      else if(bn_binding_name_pattern_match(ts_str(bname), "/system/model")) {
    err = bn_binding_get_str(binding, ba_value, bt_NONE, &btype_flags,
      &model);
      }
      else if (bn_binding_name_pattern_match(ts_str(bname), "/image/boot_location/booted/location_id")) {
          err = bn_binding_get_uint32(binding, ba_value, &btype_flags, &boot_loc);
          standby_loc = (boot_loc == 1) ? 2 : 1;
          bail_error(err);
      }
      else if (bn_binding_name_pattern_match(ts_str(bname), "/image/info/installed_image/curr_device/location_id/1/build_prod_version")) {
          err = bn_binding_get_str (binding, ba_value, bt_NONE, &btype_flags, &image[0]);
          bail_error (err);
      } 
      else if (bn_binding_name_pattern_match(ts_str(bname), "/image/info/installed_image/curr_device/location_id/2/build_prod_version")) {
          err = bn_binding_get_str (binding, ba_value, bt_NONE, &btype_flags, &image[1]);
          bail_error (err);
      } 
  }
    /* Construct XML response */
    OP_EMIT_TAG_START(context, ODCI_MFC_SYSTEM_DETAILS);
        OP_EMIT_TAG_VALUE(context, ODCI_MGMT_IP_ADDRESS, (mgmt_ip ? : eth0_ip ? : "0.0.0.0"));
        OP_EMIT_TAG_VALUE(context, ODCI_HOSTNAME, host);
        OP_EMIT_TAG_VALUE(context, ODCI_VERSION, version);
        OP_EMIT_TAG_VALUE(context, ODCI_STANDBY_IMAGE, (standby_loc ? (image[standby_loc-1] ? :"None") : "Invalid standby image location"));
        OP_EMIT_TAG_VALUE(context, ODCI_MODEL, model);
    OP_EMIT_TAG_END(context, ODCI_MFC_SYSTEM_DETAILS);
    }

bail:
    ts_free(&ret_msg);
    ts_free(&t_mgmt_ip);
    safe_free (host);
    safe_free (version);
    safe_free (model);
    safe_free (eth0_ip);
    safe_free (image[0]);
    safe_free (image[1]);
    if (req) bn_request_msg_free (&req);
    if (resp) bn_response_msg_free (&resp);
    bn_binding_array_free (&resp_bindings);
    return err;
}


int get_namespace_counters(agentd_context_t *context, void *data)
{
    int err = 0;
bail:
    return err;
}


int get_interface_details(agentd_context_t *context, void *data)
{
    int err = 0;
    oper_table_entry_t *cmd = (oper_table_entry_t *)data;
    node_name_t pattern = {0};
    const char *if_name = "*";
    tstr_array *intf_arr = NULL;
    uint32 length = 0;

    tstring *t_ipv4addr;

    lc_log_debug(jnpr_log_level, "got %s", cmd->pattern);
    /*TODO : This command can be used if a specific interface name is given*/
    snprintf(pattern, sizeof(pattern), "/net/interface/config");
    err = mdc_get_binding_children_tstr_array(agentd_mcc,
      NULL, NULL, &intf_arr,
      pattern, NULL);
    bail_error_null(err, intf_arr);

    length = tstr_array_length_quick(intf_arr);
    for(uint32 i = 0; i< length; i++)
    {
        t_ipv4addr = NULL;
        const char *interface = tstr_array_get_str_quick(intf_arr, i);

        err = mdc_get_binding_tstr_fmt(agentd_mcc, NULL, NULL, NULL,
        &t_ipv4addr, NULL,
        "/net/interface/state/%s/addr/ipv4/1/ip", interface);
        bail_error(err);

        if(!t_ipv4addr) {
          ts_new(&t_ipv4addr);
          ts_append_str(t_ipv4addr, "0.0.0.0");
        }
        OP_EMIT_TAG_START(context, ODCI_INTERFACE);
            OP_EMIT_TAG_VALUE(context, ODCI_NAME, interface);
            OP_EMIT_TAG_VALUE(context, ODCI_IP_ADDRESS, ts_str(t_ipv4addr));
        OP_EMIT_TAG_END(context, ODCI_INTERFACE);
        ts_free(&t_ipv4addr);
    }

bail:
    ts_free(&t_ipv4addr);
    tstr_array_free(&intf_arr);
    return err;
}

int agentd_show_service_info (agentd_context_t *context, void *data) {
/* This function handles the following command pattern */
// "/service-info/mfc-cluster/*/service-name/*"
    int err = 0;
    uint32 ret_code = 0;
    tstring * ret_msg = NULL;
    oper_table_entry_t *cmd = (oper_table_entry_t *)data;
    const char * mfc_cluster_name = NULL;
    const char * service_name = NULL;
    char ps_name[MAX_TMP_BUF_LEN] = {0};
    node_name_t node_pat = {0};
    node_name_t uptime_name = {0};
    bn_binding_array * bindings = NULL;
    bn_binding_array * nvsd_bindings = NULL;
    const bn_binding * uptime_binding = NULL;
    char *uptime_str =NULL;
    tstring * status = NULL;
    tstring * failures = NULL;
    tstring * last_term = NULL;
    tstring * glob_status = NULL;
    tstring * mgmt_status = NULL;
    tstring * nw_status = NULL;
    tstring * disk_glob_status = NULL;
    tstring * sas_status = NULL;
    tstring * sata_status = NULL;
    tstring * ssd_status = NULL;
    lt_dur_ms uptime = 0;
    int is_nvsd = 0;

    lc_log_debug(jnpr_log_level, "got %s", cmd->pattern);
    agentd_dump_op_cmd_details (context);

    mfc_cluster_name = tstr_array_get_str_quick(context->op_cmd_parts,POS_CLUSTER_NAME) ;
    service_name = tstr_array_get_str_quick(context->op_cmd_parts, POS_SERVICE_NAME);

    lc_log_debug(jnpr_log_level, "MFC cluster:%s, Service name:%s", mfc_cluster_name, service_name);

    if (strcmp(service_name, "crawler") == 0) {
        snprintf (ps_name, sizeof(ps_name),"%s", "crawler");
    } else if (strcmp(service_name, "nvsd") == 0) {
        snprintf (ps_name, sizeof(ps_name),"%s", "nvsd");
        is_nvsd = 1;
    } else {
        lc_log_debug(LOG_ERR, "Invalid service name");
        err = 1;
        goto bail;
    }

    /* Get the process monitor bindings */
    snprintf(node_pat, sizeof(node_pat), "/pm/monitor/process/%s", ps_name);
    err = mdc_get_binding_children(agentd_mcc, NULL, NULL, true, &bindings,
                                  true, true, node_pat);
    bail_error_null(err, bindings);
    bn_binding_array_dump("PM-MON-BINDINGS", bindings, jnpr_log_level);

    err = agentd_binding_array_get_value_tstr_fmt (bindings, &status,  "/pm/monitor/process/%s/state", ps_name);
    bail_error (err);
    err = agentd_binding_array_get_value_tstr_fmt (bindings, &failures,  "/pm/monitor/process/%s/num_failures", ps_name);
    bail_error (err);
    err = agentd_binding_array_get_value_tstr_fmt (bindings, &last_term,  "/pm/monitor/process/%s/last_terminated/time", ps_name);
    bail_error (err);

    snprintf (uptime_name, sizeof (uptime_name), "/pm/monitor/process/%s/uptime", ps_name);
    err = bn_binding_array_get_binding_by_name (bindings, uptime_name, &uptime_binding);
    bail_error(err);
    if (uptime_binding) {
        err = bn_binding_get_duration_ms (uptime_binding, ba_value, NULL, &uptime);
        bail_error(err);
        err = lt_dur_ms_to_counter_str (uptime, (char **)(&uptime_str));
        bail_error (err);
        lc_log_debug (jnpr_log_level, "Uptime: %ld ms / %s", (int64) uptime, uptime_str);
    }
    /* Get additional status information for nvsd service */
    if (is_nvsd) {
        snprintf(node_pat, sizeof(node_pat), "%s", "/nkn/nvsd/services/monitor/state/nvsd");
        err = mdc_get_binding_children(agentd_mcc, NULL, NULL, true, &nvsd_bindings,
                                          true, true, node_pat);
        bail_error_null(err, bindings);
        bn_binding_array_dump ("NVSD-MON-BINDINGS", nvsd_bindings, jnpr_log_level);

        err = agentd_binding_array_get_value_tstr_fmt (nvsd_bindings, &glob_status, "/nkn/nvsd/services/monitor/state/nvsd/global");
        bail_error (err);
        err = agentd_binding_array_get_value_tstr_fmt (nvsd_bindings, &mgmt_status, "/nkn/nvsd/services/monitor/state/nvsd/mgmt");
        bail_error (err);
        err = agentd_binding_array_get_value_tstr_fmt (nvsd_bindings, &nw_status, "/nkn/nvsd/services/monitor/state/nvsd/network");
        bail_error (err);
        err = agentd_binding_array_get_value_tstr_fmt (nvsd_bindings, &disk_glob_status, "/nkn/nvsd/services/monitor/state/nvsd/preread/global");
        bail_error (err);
        err = agentd_binding_array_get_value_tstr_fmt (nvsd_bindings, &sas_status, "/nkn/nvsd/services/monitor/state/nvsd/preread/sas");
        bail_error (err);
        err = agentd_binding_array_get_value_tstr_fmt (nvsd_bindings, &sata_status, "/nkn/nvsd/services/monitor/state/nvsd/preread/sata");
        bail_error (err);
        err = agentd_binding_array_get_value_tstr_fmt (nvsd_bindings, &ssd_status, "/nkn/nvsd/services/monitor/state/nvsd/preread/ssd");
        bail_error (err);
    }
    OP_EMIT_TAG_START(context, ODCI_MFC_SERVICE_INFO);
        OP_EMIT_TAG_START(context, ODCI_SERVICE);
            OP_EMIT_TAG_VALUE(context, ODCI_SERVICE_NAME, service_name);
    if (is_nvsd) {
            OP_EMIT_TAG_VALUE(context, ODCI_SERVICE_STATUS, ts_str(glob_status)?:"N/A");
            OP_EMIT_TAG_START(context, ODCI_NVSD_INFO);
                OP_EMIT_TAG_VALUE(context, ODCI_MGMT_STATUS, ts_str(mgmt_status)?:"N/A");
                OP_EMIT_TAG_VALUE(context, ODCI_NW_STATUS, ts_str(nw_status)?:"N/A");
                OP_EMIT_TAG_VALUE(context, ODCI_DISK_CACHE_STATUS, ts_str(disk_glob_status)?:"N/A");
                OP_EMIT_TAG_VALUE(context, ODCI_SAS_DICT_GEN, ts_str(sas_status)?:"N/A");
                OP_EMIT_TAG_VALUE(context, ODCI_SATA_DICT_GEN, ts_str(sata_status)?:"N/A");
                OP_EMIT_TAG_VALUE(context, ODCI_SSD_DICT_GEN, ts_str(ssd_status)?:"N/A");
            OP_EMIT_TAG_END(context, ODCI_NVSD_INFO);
    } else {
            OP_EMIT_TAG_VALUE(context, ODCI_SERVICE_STATUS, ts_str(status));
    }
            OP_EMIT_TAG_VALUE(context, ODCI_FAILURE_COUNT, ts_str(failures)?:"N/A");
            OP_EMIT_TAG_VALUE(context, ODCI_LAST_TERMINATED, ts_str(last_term)?:"N/A");
            OP_EMIT_TAG_VALUE(context, ODCI_UPTIME, uptime_str?uptime_str:"N/A");
        OP_EMIT_TAG_END(context, ODCI_SERVICE);
    OP_EMIT_TAG_END(context, ODCI_MFC_SERVICE_INFO);

bail :
    ts_free(&status);
    ts_free(&failures);
    ts_free(&last_term);
    ts_free(&glob_status);
    ts_free(&mgmt_status);
    ts_free(&nw_status);
    ts_free(&disk_glob_status);
    ts_free(&sas_status);
    ts_free(&sata_status);
    ts_free(&ssd_status);
    safe_free(uptime_str);
    bn_binding_array_free (&bindings);
    bn_binding_array_free (&nvsd_bindings);
    return err;
}

int agentd_show_running_config (agentd_context_t * context, void * data) {
/* This function handles the pattern */
//   "/running-config/mfc-cluster/*"
    int err = 0;
    tstring * error = NULL, *output = NULL;
    tbool success = false;
    tstr_array * cmds = NULL;

    err = tstr_array_new_va_str (&cmds, NULL, 3,
                                "enable","configure terminal",
                                 "show running-config");
    bail_error (err);

    err = agentd_execute_cli_commands (cmds, &error, &output, &success);
    bail_error(err);

    if (!success) {
        lc_log_debug (jnpr_log_level, "Error: %s\n", ts_str(error));
    } else {
        lc_log_debug (jnpr_log_level, "Show running output : %s\n", ts_str(output));
        OP_EMIT_TAG_START(context, ODCI_MFC_RUNNING_CONFIG);
            OP_EMIT_TAG_VALUE(context, ODCI_CONFIG, ts_str(output));
        OP_EMIT_TAG_END(context, ODCI_MFC_RUNNING_CONFIG);
    }

bail:
    ts_free (&output);
    ts_free (&error);
    tstr_array_free (&cmds);
    return err;
}

int
agentd_show_config_version(agentd_context_t *context, void *data)
{
    int err = 0;

    OP_EMIT_TAG_START(context, ODCI_MFC_CONFIG_VERSION);
        OP_EMIT_TAG_VALUE(context, ODCI_CONFIG_VERSION, "10");
    OP_EMIT_TAG_END(context, ODCI_MFC_CONFIG_VERSION);

bail:
    return 0;
}

int
agentd_show_schema_version(agentd_context_t *context, void *data)
{
    int err = 0;

    OP_EMIT_TAG_START(context, ODCI_MFC_SCHEMA_VERSION);
        OP_EMIT_TAG_VALUE(context, ODCI_SCHEMA_VERSION, AGENTD_SCHEMA_VERSION);
        OP_EMIT_TAG_VALUE(context, ODCI_MIN_SCHEMA_VERSION, AGENTD_MIN_SCHEMA_VERSION);
    OP_EMIT_TAG_END(context, ODCI_MFC_SCHEMA_VERSION);

bail:
    return 0;
}
