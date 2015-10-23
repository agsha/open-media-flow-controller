#include "agentd_mgmt.h"
#include "agentd_op_cmds_base.h"
#include "agentd_utils.h"
#include "tpaths.h"

extern int jnpr_log_level;
extern md_client_context * agentd_mcc;

/* ---- Function prototype for initializing / deinitializing module ---- */

int 
agentd_logging_cmds_init(
        const lc_dso_info *info,
        void *context);
int
agentd_logging_cmds_deinit(
        const lc_dso_info *info,
        void *context);

/* ---- Configuration custom handler prototype ---- */ 

static int
agentd_logging_local_severity (agentd_context_t *context, agentd_binding *br_cmd, void *cb_arg, char **cli_buff);

static int
agentd_logging_trap_severity (agentd_context_t *context, agentd_binding *br_cmd, void *cb_arg, char **cli_buff);

static int
agentd_logging_remote_host (agentd_context_t *context, agentd_binding *br_cmd, void *cb_arg, char **cli_buff);

/* ---- Operational command handler prototype and structure definition ---- */ 

/* ---- Translation table array for configuration commands ---- */

static  mfc_agentd_nd_trans_tbl_t logging_cmds_trans_tbl[] =  {
TRANSL_ENTRY(PREPEND_TRANS_STR"systemloggingexportpurgecriteriafrequency*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/nkn/logexport/config/purge/criteria/interval",
                TYPE_UINT32,
                ND_NORMAL,
                0,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemloggingexportpurgecriteriasize-pecentage*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_MULT_VAL("/nkn/logexport/config/purge/criteria/threshold_size_thou_pct",
                TYPE_UINT32,
                ND_NORMAL,
                0,NULL,
                1000) /* Multiplier value */
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemloggingdisable-log-pull*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/auth/passwd/user/LogTransferUser/enable",
                TYPE_BOOL,
                ND_HARDCODED,
                0,"false")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemloggingexportpurgecriteriafrequency*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/logexport/config/purge/criteria/interval",
        TYPE_UINT32,
        ND_NORMAL,
        1,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemloggingexportpurgecriteriasize-pecentage*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/logexport/config/purge/criteria/threshold_size_thou_pct",
        TYPE_UINT32,
        ND_NORMAL,
        1,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemloggingdisable-log-pull*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/auth/passwd/user/LogTransferUser/enable",
                TYPE_BOOL,
                ND_HARDCODED,
                0,"true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemlogginglocalseverity*")
TRANSL_CUST (agentd_logging_local_severity, "set") 
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemlogginglocalseverity*")
TRANSL_CUST (agentd_logging_local_severity, "reset") 
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemloggingtrapseverity*")
TRANSL_CUST (agentd_logging_trap_severity, "set") 
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemloggingtrapseverity*")
TRANSL_CUST (agentd_logging_trap_severity, "reset") 
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemloggingremote*")
TRANSL_CUST (agentd_logging_remote_host, "remote-only")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemloggingremote*")
TRANSL_NUM_NDS(1)
DEL_TRANSL_NDS_BASIC ("/logging/syslog/action/host/%s",
                   TYPE_HOSTNAME,
                   ND_NORMAL,
                   0, NULL)
TRANSL_END_NDS	
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemloggingremote*severity*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/logging/syslog/action/host/%s/facility/all/min_priority",
	TYPE_STR,
	ND_NORMAL,
	1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemloggingremote*severity*")
TRANSL_CUST (agentd_logging_remote_host, "remote-severity")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY_NULL
};


/*---------------------------------------------------------------------------*/
int 
agentd_logging_cmds_init(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    err = agentd_register_cmds_array (logging_cmds_trans_tbl);
    bail_error (err);

bail:
    return err;
}

int
agentd_logging_cmds_deinit(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;
    lc_log_debug (jnpr_log_level, "agentd_logging_cmds_deinit called");

bail:
    return err;
}

static int
agentd_logging_local_severity (agentd_context_t *context, agentd_binding *br_cmd, void *cb_arg, char **cli_buff) {
    int err = 0;
    char * agentd_logging_local_node_name = NULL;
    char * op_str = NULL;
    str_value_t severity_level = {0};
    subop_t optype = OP_NONE;

    bail_null(cb_arg);

    op_str = (char *)cb_arg;

    /* Create the node name with appropriate escaping as it contains the file name with path */
    err = bn_binding_name_parts_to_name_va
        (true, &agentd_logging_local_node_name, 8, "logging", "syslog",
         "action", "file", file_syslog_path, "facility", "all",
         "min_priority", NULL);
    bail_error_null(err, agentd_logging_local_node_name);

    lc_log_debug (jnpr_log_level, "local syslog severity node: %s", agentd_logging_local_node_name);

    if (strcmp (op_str, "set") == 0 ) {
        optype = SUBOP_MODIFY;
        /* args[1] has the severity level */
        snprintf (severity_level, sizeof(severity_level), "%s", br_cmd->arg_vals.val_args[1]);
    } else {
        /* severity level does not matter for reset operation */
        optype = SUBOP_RESET;
    }

    err =agentd_append_req_msg (context, agentd_logging_local_node_name, optype, TYPE_STR, severity_level);
    bail_error(err);

bail:
    safe_free (agentd_logging_local_node_name);
    return err;
}

static int
agentd_logging_trap_severity (agentd_context_t *context, agentd_binding *br_cmd, void *cb_arg, char **cli_buff) {
    int err = 0;
    char * op_str = NULL;
    str_value_t severity_level = {0};
    subop_t optype = OP_NONE;
    node_name_t parent = {0}, host_priority_node = {0};
    tstr_array * host_arr = NULL;
    uint32 num_hosts = 0;

    bail_null (cb_arg);
    op_str = (char *)cb_arg;

    if (strcmp (op_str, "set") == 0 ) {
        optype = SUBOP_MODIFY;
        /* args[1] has the severity level */
        snprintf (severity_level, sizeof(severity_level), "%s", br_cmd->arg_vals.val_args[1]);
    } else {
        optype = SUBOP_RESET;
        /* severity level does not matter for reset operation */
    }

    lc_log_debug (jnpr_log_level, "severity level: %s, operation: %d", severity_level, optype);

    /* Add request to modify the global severity level for remote logging */
    err = agentd_append_req_msg (context, "/logging/syslog/defaults/action/host/facility/all/min_priority",
                                optype, TYPE_STR, severity_level);
    bail_error(err);

    /* Add requests to modify the severity level for each configured remote host */
    snprintf(parent, sizeof(parent), "/logging/syslog/action/host");
    err = mdc_get_binding_children_tstr_array(agentd_mcc, NULL, NULL, &host_arr, parent, NULL);
    bail_error_null (err, host_arr);

    num_hosts = tstr_array_length_quick (host_arr);
    lc_log_debug (jnpr_log_level, "Number of remote syslog servers configured: %d", num_hosts);

    for (uint32 i = 0; i < num_hosts; i++) {
        const char * host = tstr_array_get_str_quick (host_arr, i);
        lc_log_debug (jnpr_log_level, "Setting/Resetting severity of syslog server \'%s\'",host);

        snprintf (host_priority_node, sizeof(host_priority_node), "%s/%s/facility/all/min_priority",
                                        parent,host);
        err = agentd_append_req_msg (context, host_priority_node, optype, TYPE_STR, severity_level);
        bail_error (err);
    }

bail:
    tstr_array_free (&host_arr);
    return err;
}

static int
agentd_logging_remote_host (agentd_context_t *context, agentd_binding *br_cmd, void *cb_arg, char **cli_buff) {
    int err = 0;
    str_value_t host = {0};
    char *op = NULL;
    tstring * default_severity = NULL;
    node_name_t host_node = {0};
 
    bail_null(cb_arg);
    op = (char *) cb_arg;
 
    /* args[1] contains the host ip */
    snprintf(host, sizeof(host), "%s", br_cmd->arg_vals.val_args[1]);
    lc_log_debug (jnpr_log_level, "Remote syslog server: %s", host);

    if (strcmp (op, "remote-only") == 0) {
        lc_log_debug (jnpr_log_level, "Creating host node for the remote syslog server: %s", host);
        snprintf (host_node, sizeof(host_node), "/logging/syslog/action/host/%s", host);
        err = agentd_append_req_msg (context, host_node, SUBOP_MODIFY, 
                                 TYPE_HOSTNAME, host);
        bail_error (err);
    }
    /* Set the severity of this host to the default configured severity of all hosts */
    err = agentd_mdc_get_value_tstr_fmt (&default_severity, "/logging/syslog/defaults/action/host/facility/all/min_priority");
    bail_error (err);

    lc_log_debug (jnpr_log_level, "Default severity of all syslog hosts: %s", ts_str(default_severity));
    snprintf(host_node, sizeof(host_node), "/logging/syslog/action/host/%s/facility/all/min_priority",
                                            host);
    err =  agentd_append_req_msg (context, host_node, SUBOP_MODIFY, TYPE_STR, ts_str(default_severity));
    bail_error(err);
                                         
bail:
    ts_free (&default_severity);
    return err;
}

