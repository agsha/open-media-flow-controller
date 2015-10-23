/*
 * Filename :   agentd_op_cmds_req.c
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
#include "agentd_copied_code.h"
#include "agentd_utils.h"
#include "agentd_op_cmds_base.h"

/*
    1050 Config request Failed
    1051 Unable to find Translational entry
    1052 lookup failed for operational command
*/

extern md_client_context * agentd_mcc;

extern int jnpr_log_level;

int agentd_op_req_purge_contents(agentd_context_t *context, void *data)
{
    int err = 0;
    oper_table_entry_t *cmd = (oper_table_entry_t *)data;
    unsigned int length = 0;
    const char * mfc_cluster_name = NULL;
    const char * instance_name = NULL;
    const char * uri = NULL;
    const char * domain = NULL;
    const char * port = NULL;
    const char * tmp = NULL;
    const char * instance_tag = "service-instance";
    const char * uri_tag = "uri-pattern";
    const char * domain_tag = "domain";
    const char * port_tag = "port";
    temp_buf_t unquoted_uri = {0};
    uint32 ret_code = 0;
    tstring * ret_msg = NULL;

    lc_log_debug(jnpr_log_level, "got %s, cmd-parts-size=%d", cmd->pattern,
	   tstr_array_length_quick(context->op_cmd_parts) );

    length = tstr_array_length_quick(context->op_cmd_parts);

    agentd_dump_op_cmd_details(context);

    mfc_cluster_name = tstr_array_get_str_quick(context->op_cmd_parts,POS_CLUSTER_NAME) ;

    lc_log_debug(jnpr_log_level, "Cluster Name =  %s", mfc_cluster_name);

    for (unsigned int i=POS_CLUSTER_NAME+1 ;i<=length;i++ )
    {
        tmp = tstr_array_get_str_quick(context->op_cmd_parts,i);

        if (tmp== NULL)
            continue;

        lc_log_debug(jnpr_log_level, "The cmd arr checking for |%s|",tmp);

        if( strcmp(tmp,instance_tag) == 0 )
        {
            i++;
            instance_name = tstr_array_get_str_quick(context->op_cmd_parts,i);
        }
        else if( strcmp(tmp,uri_tag) == 0 )
        {
            i++;
            uri = tstr_array_get_str_quick(context->op_cmd_parts,i);
            sscanf (uri, "\"%s\"", unquoted_uri);
            unquoted_uri[strlen(unquoted_uri)-1] = '\0';
        }
        else if( strcmp(tmp,domain_tag) == 0 )
        {
            i++;
            domain = tstr_array_get_str_quick(context->op_cmd_parts,i);
        }
        else if( strcmp(tmp,port_tag) == 0 )
        {
            i++;
            port = tstr_array_get_str_quick(context->op_cmd_parts,i);
        }
        else
        {
            err=1;
            bail_error(err);
        }
    }

    if (instance_name==NULL || uri == NULL )
    {
        
        lc_log_debug(jnpr_log_level, "err : instance: %s url : %s ", 
	   instance_name ,uri );

        err=1;
        bail_error(err);
    }

    lc_log_debug(jnpr_log_level, " Going to invoke tm functions with namespace %s uri-pattern %s", instance_name, unquoted_uri);

    err = mdc_send_action_with_bindings_str_va(
                agentd_mcc, &ret_code, &ret_msg, "/nkn/nvsd/namespace/actions/delete_uri", 2,
                "namespace", bt_string,instance_name,
		"uri-pattern",bt_string,unquoted_uri );
    lc_log_debug (jnpr_log_level, "ret_code = %d, ret_msg = %s", ret_code, ts_str(ret_msg)); 
    if (ret_code != 0) {
        set_ret_code(context, ret_code);
        set_ret_msg(context, ts_str(ret_msg));
        goto bail;
    }
bail:
    return err;
}

int agentd_op_req_revalidate_contents (agentd_context_t *context, void *data) {
/* This function handles the following command pattern */
// "/revalidate-contents/mfc-cluster/*/service-instance/*/*"

    #define MFC_NAME_POS 2
    #define NS_NAME_POS 4
    #define REVAL_TYPE_POS 5

    int err = 0;
    oper_table_entry_t *cmd = (oper_table_entry_t *) data;
    const char * mfc_name = NULL;
    const char * ns_name = NULL;
    const char * reval_type = NULL;
    tstring *t_proxy_mode = NULL;
    uint32 ret_code = 0;
    tstring * ret_msg = NULL;

    lc_log_debug (jnpr_log_level, "got %s", cmd->pattern);
    agentd_dump_op_cmd_details (context);

    mfc_name = tstr_array_get_str_quick(context->op_cmd_parts, MFC_NAME_POS);
    ns_name = tstr_array_get_str_quick(context->op_cmd_parts, NS_NAME_POS);
    reval_type = tstr_array_get_str_quick(context->op_cmd_parts, REVAL_TYPE_POS);

    if (mfc_name == NULL || ns_name == NULL || reval_type == NULL) {
        lc_log_debug(LOG_ERR, "Received NULL arguments");
        err = 1;
        goto bail;
    }
    lc_log_debug(jnpr_log_level, "MFC Name: %s, Serv instance: %s, Revalidate: %s",
                                  mfc_name, ns_name, reval_type);

    /* Check proxy mode of the namespace. We dont allow this for t-proxy namespace */
    err = mdc_get_binding_tstr_fmt (agentd_mcc, NULL, NULL,NULL,
                                     &t_proxy_mode, NULL,
                                     "/nkn/nvsd/namespace/%s/proxy-mode",
                                     ns_name);
    bail_error(err);
    if (!t_proxy_mode) {
        lc_log_debug(LOG_ERR, "Namespace \'%s\' not found", ns_name);
        err = 1;
        goto bail;
    }

    if ((strcmp(ts_str(t_proxy_mode), "transparent") == 0) ||
        (strcmp(ts_str(t_proxy_mode), "virtual") == 0)) {
        lc_log_debug(LOG_ERR, "Operation not permitted for t-proxy instance");
        err = 1 ;
        goto bail;
    }

    err = mdc_send_action_with_bindings_str_va (agentd_mcc, &ret_code, &ret_msg,
                "/nkn/nvsd/namespace/actions/revalidate_cache_timer", 2,
                "namespace", bt_string, ns_name,
                "type", bt_string, "all"); /* We support only revalidation type "all" */
    bail_error(err);
    if (ret_code != 0) {
        lc_log_debug (jnpr_log_level, "mdc_send_action- Ret code: %d, Ret message: %s",
                             ret_code, ts_str(ret_msg));
        set_ret_code(context, ret_code);
        set_ret_msg(context, ts_str(ret_msg));
    }
        
bail:
    ts_free(&t_proxy_mode);
    return err;
}

int agentd_op_req_restart_process(agentd_context_t *context, void *data) {
/* This function handles the following command pattern */
// "/restart-service/mfc-cluster/*/service-name/*"
    int err = 0;
    uint32 ret_code = 0;
    tstring * ret_msg = NULL;
    oper_table_entry_t *cmd = (oper_table_entry_t *)data;
    const char * mfc_cluster_name = NULL;
    const char * service_name = NULL;
    char ps_name[MAX_TMP_BUF_LEN] = {0};

    lc_log_debug(jnpr_log_level, "got %s", cmd->pattern);
    agentd_dump_op_cmd_details (context);

    mfc_cluster_name = tstr_array_get_str_quick(context->op_cmd_parts,POS_CLUSTER_NAME) ;
    service_name = tstr_array_get_str_quick(context->op_cmd_parts, POS_RESTART_SERVICE_NAME);

    lc_log_debug(jnpr_log_level, "MFC cluster:%s, Service name:%s", mfc_cluster_name, service_name);

    if (strcmp(service_name, "crawler") == 0) {
        snprintf (ps_name, sizeof(ps_name),"%s", "crawler");
    } else if (strcmp(service_name, "nvsd") == 0) {
        err = md_clear_nvsd_samples(mfc_cluster_name);
        goto bail;
    } else {
        lc_log_debug(LOG_ERR, "Invalid service name");
        err = 1;
        goto bail;
    }

    err = mdc_send_action_with_bindings_str_va(agentd_mcc, &ret_code, &ret_msg, "/pm/actions/restart_process", 1,
                                "process_name", bt_string, ps_name);
    bail_error(err);
    if (ret_code != 0) {
        lc_log_debug (jnpr_log_level, "mdc_send_action- Ret code: %d, Ret message: %s", 
                             ret_code, ts_str(ret_msg));
        set_ret_code(context, ret_code);
        set_ret_msg(context, ts_str(ret_msg));
    }

bail :
    ts_free(&ret_msg);
    return err;
}

int agentd_op_req_stop_process(agentd_context_t *context, void *data) {
/* This function handles the following command pattern */
// "/stop-service/mfc-cluster/*/service-name/*"
    int err = 0;
    uint32 ret_code = 0;
    tstring * ret_msg = NULL;
    oper_table_entry_t *cmd = (oper_table_entry_t *)data;
    const char * service_name = NULL;
    char ps_name[MAX_TMP_BUF_LEN] = {0};

    lc_log_debug(jnpr_log_level, "got %s", cmd->pattern);
    agentd_dump_op_cmd_details (context);
    service_name = tstr_array_get_str_quick(context->op_cmd_parts, POS_STOP_SERVICE_NAME);

    if (strcmp(service_name, "crawler") == 0) {
        snprintf (ps_name, sizeof(ps_name),"%s", "crawler");
    } else {
        lc_log_debug(LOG_ERR, "Invalid service name");
        err = 1;
        goto bail;
    }

    lc_log_debug(jnpr_log_level, "Service name %s", service_name);
    err = mdc_send_action_with_bindings_str_va(agentd_mcc, &ret_code, &ret_msg, 
	    "/pm/actions/terminate_process", 1,
	    "process_name", bt_string, ps_name);
    bail_error(err);
    if (ret_code != 0) {
        lc_log_debug (jnpr_log_level, "mdc_send_action- Ret code: %d, Ret message: %s",
                             ret_code, ts_str(ret_msg));
        set_ret_code(context, ret_code);
        set_ret_msg(context, ts_str(ret_msg));
    }

bail :
    ts_free(&ret_msg);
    return err;
}

