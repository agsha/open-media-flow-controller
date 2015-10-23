/*
 * Filename :   agentd_op_cmds_base.c
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
/*
    1050 Config request Failed
    1051 Unable to find Translational entry
    1052 lookup failed for operational command
*/
extern md_client_context * agentd_mcc;

extern int jnpr_log_level;

int op_table_size = 0; /* Holds the number of commands registered in operation_cmd_table */

oper_table_entry_t operation_cmd_table[MAX_OP_TBL_ENTRIES] = {
    {"/system-info/mfc-cluster/*",DUMMY_WC_ENTRY , get_system_version, NULL},
    {"/purge-contents/mfc-cluster/*/service-instance/*/uri-pattern/*", DUMMY_WC_ENTRY, agentd_op_req_purge_contents, NULL},
    {"/interfaces/mfc-cluster/*/member-node/*/*",DUMMY_WC_ENTRY, agentd_show_interface_details, NULL},
    {"/interfaces/mfc-cluster/*/member-node/*/brief/*",DUMMY_WC_ENTRY, agentd_show_interface_details, NULL},
    {"/interfaces/mfc-cluster/*/member-node/*/configured/*",DUMMY_WC_ENTRY, agentd_show_interface_details, NULL},
    {"/interfaces/mfc-cluster/*/member-node/*",DUMMY_WC_ENTRY, agentd_show_interface_details, NULL},
    {"/interfaces/mfc-cluster/*/member-node/*/brief",DUMMY_WC_ENTRY, agentd_show_interface_details, NULL},
    {"/interfaces/mfc-cluster/*/member-node/*/configured",DUMMY_WC_ENTRY, agentd_show_interface_details, NULL},
    {"/bonds/mfc-cluster/*/member-node/*",DUMMY_WC_ENTRY, agentd_show_bond_details, NULL},
    {"/bonds/mfc-cluster/*/member-node/*/*",DUMMY_WC_ENTRY, agentd_show_bond_details, NULL},
    {"/revalidate-contents/mfc-cluster/*/service-instance/*/*",DUMMY_WC_ENTRY, agentd_op_req_revalidate_contents, NULL},
    {"/restart-service/mfc-cluster/*/service-name/*", DUMMY_WC_ENTRY, agentd_op_req_restart_process, NULL},
    {"/stop-service/mfc-cluster/*/service-name/*", DUMMY_WC_ENTRY, agentd_op_req_stop_process, NULL},
    {"/service-info/mfc-cluster/*/service-name/*", DUMMY_WC_ENTRY, agentd_show_service_info, NULL},
    {"/running-config/mfc-cluster/*", DUMMY_WC_ENTRY, agentd_show_running_config, NULL},
    {"/config-version/mfc-cluster/*", DUMMY_WC_ENTRY, agentd_show_config_version, NULL },
    {"/schema-version/mfc-cluster/*", DUMMY_WC_ENTRY, agentd_show_schema_version, NULL },
    {"/purge-contents/mfc-cluster/*/service-instance/*/uri-pattern/*/domain/*", DUMMY_WC_ENTRY, agentd_op_req_purge_contents, NULL},
    {"/purge-contents/mfc-cluster/*/service-instance/*/uri-pattern/*/port/*", DUMMY_WC_ENTRY, agentd_op_req_purge_contents, NULL},
    {"/purge-contents/mfc-cluster/*/service-instance/*/uri-pattern/*/domain/*/port/*", DUMMY_WC_ENTRY, agentd_op_req_purge_contents, NULL},
};


static int agentd_get_oper_function(agentd_context_t *context, char *oper_cmd,  oper_table_entry_t **oper_func, int *ret_code);

int agentd_process_oper_request(agentd_context_t *context, int *ret_code)
{
    int err = 0;
    oper_table_entry_t *oper_func = NULL;

    /*Do the look up on the command, if matches, then get the function pointer*/
    err = agentd_get_oper_function(context, context->oper_cmd, &oper_func,
            ret_code);
    if (!oper_func)
    {
        lc_log_basic(LOG_NOTICE, _("Lookup for operation-cmd [%s] failed"),
                context->oper_cmd);
        goto bail;
    }

    /*call the oper_func*/
    err = (oper_func->func)(context, oper_func->callback_data);
    bail_error(err);

    bail: return err;
}

static int agentd_get_oper_function(agentd_context_t *context, char *oper_cmd,
        oper_table_entry_t **oper_func, int *ret_code)
{
    int err = 0;
    uint32 i;
    tstr_array *op = NULL;
    char temp_buf[256] = { 0 };

    bail_null(oper_cmd);
    *oper_func = NULL;

    lc_log_debug(jnpr_log_level, "Finding call back for oper command %s",
            oper_cmd);

    err = ts_tokenize_str(oper_cmd, ' ', 0, 0, ttf_ignore_leading_separator
            | ttf_ignore_trailing_separator, &op);
    bail_error_null(err, op);

    context->op_cmd_parts = op;

    if (op)
    {
        for (i = 0; i < sizeof(operation_cmd_table)
                / sizeof(oper_table_entry_t); i++)
        {
            oper_table_entry_t *entry = &operation_cmd_table[i];

            if (bn_binding_name_parts_pattern_match(op, true, entry->pattern))
            {
                /*Found the pattern*/
                *oper_func = &operation_cmd_table[i];
                break;
            }

        }
        if (!(*oper_func))
        {
            lc_log_debug(jnpr_log_level,
                    "Look up failed for Operation command [%s]", oper_cmd);
            snprintf(temp_buf, sizeof(temp_buf), "Operation command [%s]"
                " not found", oper_cmd);
            set_ret_msg(context, temp_buf);
            set_ret_code(context, 1052);
        }
    }

    bail: op = NULL; /*since the ownership is now with the context*/
    return err;
}

void agentd_dump_op_cmd_details(agentd_context_t *context)
{
    const char* tmp = NULL;
    unsigned int length = tstr_array_length_quick(context->op_cmd_parts);
    for (unsigned int i = 0; i <= length; i++)
    {
        tmp = tstr_array_get_str_quick(context->op_cmd_parts, i);
        lc_log_debug(jnpr_log_level, "The cmd part %d |%s|", i, tmp);
    }

}

int
agentd_binding_array_get_value_tstr_fmt (const bn_binding_array *binding_arr, tstring ** target_tstr, const char *format, ...) {
    int err = 0;
    node_name_t str = {0};
    va_list ap;

    bail_null(target_tstr);

    va_start (ap,format);
    vsnprintf(str, sizeof(str), format, ap);
    va_end(ap);

    lc_log_debug( jnpr_log_level, "Getting value for binding %s", str);

    err = bn_binding_array_get_value_tstr_by_name (binding_arr, str, NULL, target_tstr);
    bail_error(err);

    lc_log_debug( jnpr_log_level, "Binding:%s,Value:%s", str, ts_str_maybe_empty(*target_tstr));

bail:
    return err;
}

int
agentd_binding_array_get_value_uint32_fmt (const bn_binding_array *binding_arr, uint32 *target_val, const char *format, ...) {
    int err = 0;
    node_name_t str = {0};
    va_list ap;
    const bn_binding * abinding = NULL;

    va_start (ap,format);
    vsnprintf(str, sizeof(str), format, ap);
    va_end(ap);

    lc_log_debug( jnpr_log_level, "Getting value for binding %s", str);
    err = bn_binding_array_get_binding_by_name (binding_arr, str, &abinding);
    bail_error_null (err, abinding);

    err = bn_binding_get_uint32(abinding, ba_value, NULL, target_val);
    bail_error(err);
    lc_log_debug( jnpr_log_level, "Binding:%s,Value:%d", str, *target_val);

bail:
    return err;
}

int
agentd_binding_array_get_value_uint8_fmt (const bn_binding_array *binding_arr, uint8 *target_val, const char *format, ...) {
    int err = 0;
    node_name_t str = {0};
    va_list ap;
    const bn_binding * abinding = NULL;

    va_start (ap,format);
    vsnprintf(str, sizeof(str), format, ap);
    va_end(ap);

    lc_log_debug( jnpr_log_level, "Getting value for binding %s", str);
    err = bn_binding_array_get_binding_by_name (binding_arr, str, &abinding);
    bail_error_null (err, abinding);

    err = bn_binding_get_uint8(abinding, ba_value, NULL, target_val);
    bail_error(err);
    lc_log_debug( jnpr_log_level, "Binding:%s,Value:%d", str, *target_val);

bail:
    return err;
}

int agentd_mdc_get_value_uint8_fmt (uint8 *target_val, const char *format, ...) {
    int err = 0;
    node_name_t str = {0};
    va_list ap;
    bn_binding * binding = NULL;
    node_name_t bn_name = {0};

    va_start (ap,format);
    vsnprintf(str, sizeof(str), format, ap);
    va_end(ap);

    lc_log_debug( jnpr_log_level, "Getting value for binding %s", str);
    snprintf(bn_name, sizeof(bn_name), "%s", str);
    err = mdc_get_binding (agentd_mcc, NULL, NULL, false, &binding, bn_name, NULL);
    bail_error (err);

    err = bn_binding_get_uint8(binding, ba_value, NULL, target_val);
    bail_error(err);
    lc_log_debug( jnpr_log_level, "Binding:%s,Value:%d", str, *target_val);

bail:
    bn_binding_free (&binding);
    return err;
}

int agentd_mdc_get_value_tstr_fmt (tstring **target_val, const char *format, ...)
{
    int err = 0;
    node_name_t str = {0};
    va_list ap;
    bn_binding * binding = NULL;
    node_name_t bn_name = {0};

    bail_null(target_val);

    va_start (ap,format);
    vsnprintf(str, sizeof(str), format, ap);
    va_end(ap);

    lc_log_debug( jnpr_log_level, "Getting value for binding %s", str);
    snprintf(bn_name, sizeof(bn_name), "%s", str);
    err = mdc_get_binding (agentd_mcc, NULL, NULL, false, &binding, bn_name, NULL);
    err=0;//  Do not handle error since it could fail for multiple reason. handling these errors break the caller

    if(!binding)
        goto bail;

    err = bn_binding_get_tstr(binding, ba_value, bt_NONE,NULL, target_val);
    err=0;// Do not handle error since it could return null or if the binding has null value

    lc_log_debug( jnpr_log_level, "Binding:%s,Value:%s", str, ts_str_maybe_empty(*target_val) );

bail:
    bn_binding_free (&binding);
    return err;
}

int agentd_register_op_cmds_array (oper_table_entry_t * op_arr){
    int err = 0, i = 0;

    bail_null(op_arr);

    lc_log_debug (jnpr_log_level, "agentd_register_op_cmds_array called");
    if (!op_table_size) {
        err = agentd_update_op_table_size();
        bail_error(err);
    }
    while (op_arr[i].func != NULL) {
        err = agentd_register_op_cmd(&op_arr[i]);
        bail_error (err);
        i++;
    }

bail:
    return err;
}

int agentd_register_op_cmd (oper_table_entry_t* op_entry) {
    int err = 0;

    bail_null(op_entry);
    bail_null (op_entry->func);

    lc_log_basic(LOG_DEBUG, "registering op-cmd: %s", op_entry->pattern);
    if (op_table_size >= MAX_OP_TBL_ENTRIES) {
        lc_log_basic (LOG_ERR, "Cannot register operation command %s. Table size [%d] exceeded",
			op_entry->pattern, MAX_OP_TBL_ENTRIES);
        err = 1;
        bail_error(err);
    }

    memcpy (&operation_cmd_table[op_table_size], op_entry, sizeof(oper_table_entry_t));
    op_table_size++;

bail:
    return err;
}

int agentd_update_op_table_size (void) {
    int err = 0;
    int i = 0;

    for (i=0, op_table_size=0;
	    (i < MAX_OP_TBL_ENTRIES) && (operation_cmd_table[i].func != NULL);
	    i++, op_table_size++);

bail:
    return err;
}

