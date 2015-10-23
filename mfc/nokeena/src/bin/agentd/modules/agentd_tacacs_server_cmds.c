#include "agentd_mgmt.h"

extern int jnpr_log_level;
extern md_client_context * agentd_mcc;
extern agentd_array_move_context tacacs_server_ctxt; //Context for TACACS server array

#define TACACS_SERVER_HOST_INDEX_POS 2
#define TACACS_SERVER_ROOT_NODE     "/tacacs/server"

/* ---- Function prototype for initializing / deinitializing module ---- */

int 
agentd_tacacs_server_cmds_init(
        const lc_dso_info *info,
        void *context);
int
agentd_tacacs_server_cmds_deinit(
        const lc_dso_info *info,
        void *context);

/* ---- Configuration custom handler prototype ---- */ 

static int 
agentd_configure_tacacs_host (agentd_context_t * context, agentd_binding *br_cmd, void *cb_arg, char **cli_buff);
static int 
agentd_delete_tacacs_host (agentd_context_t * context, agentd_binding *br_cmd, void *cb_arg, char **cli_buff);

/* ---- Operational command handler prototype and structure definition ---- */ 


/* --- Translation table array for configuration commands --- */

static  mfc_agentd_nd_trans_tbl_t tacacs_server_cmds_trans_tbl[] =  {

TRANSL_ENTRY(PREPEND_TRANS_STR"systemauthenticationtacacs-serverhost*")
TRANSL_CUST (agentd_configure_tacacs_host, "address")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemauthenticationtacacs-serverhost*")
TRANSL_CUST (agentd_delete_tacacs_host, "address")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemauthenticationtacacs-serverhost*key*")
TRANSL_CUST (agentd_configure_tacacs_host, "key")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemauthenticationtacacs-serverhost*key*")
TRANSL_CUST (agentd_delete_tacacs_host, "key")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemauthenticationtacacs-serverhost*timeout*")
TRANSL_CUST (agentd_configure_tacacs_host, "timeout")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemauthenticationtacacs-serverhost*timeout*")
TRANSL_CUST (agentd_delete_tacacs_host, "timeout")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY_NULL
};

/*---------------------------------------------------------------------------*/
int 
agentd_tacacs_server_cmds_init(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    err = agentd_register_cmds_array (tacacs_server_cmds_trans_tbl);
    bail_error (err);

bail:
    return err;
}

int
agentd_tacacs_server_cmds_deinit(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;
    lc_log_debug (jnpr_log_level, "agentd_tacacs_server_cmds_deinit called");

bail:
    return err;
}

/* ---- Configuration custom handler definitions ---- */

static int
agentd_configure_tacacs_host (agentd_context_t * context, agentd_binding *br_cmd, void *cb_arg, char **cli_buff) {
    int err = 0;
    str_value_t host_ip = {0}, param_value = {0};
    uint32_array *host_indices = NULL;
    uint32 num_hosts = 0;
    tbool is_param = false;
    bn_type param_type = bt_NONE;
    node_name_t node_pat = {0};
    bn_binding_array * barr = NULL;
    bn_binding * binding = NULL;
    char * binding_name = NULL;
    uint32 code = 0;

    bail_null (cb_arg);
    binding_name = ((char *) cb_arg); 

    if (!strcmp (binding_name, "address")) {
        param_type = bt_ipv4addr;
    } else if (!strcmp (binding_name, "key")) {
        param_type = bt_string;
    } else if (!strcmp (binding_name, "timeout")) {
        param_type = bt_duration_sec;
    } else {
        lc_log_basic (LOG_NOTICE, "Invalid binding name");
        goto bail;
    }

    /* args[1] contains the host ip */
    snprintf (host_ip, sizeof(host_ip), "%s", br_cmd->arg_vals.val_args[1]);

    if (br_cmd->arg_vals.nargs > 2) {
        /* Host settings are configured. args[2] contains the host settings */
        is_param = true;
        snprintf (param_value, sizeof(param_value), "%s", br_cmd->arg_vals.val_args[2]);
    } else {
        /* Host ip should be configured. Set param_value as host_ip */
        snprintf (param_value, sizeof(param_value), "%s", host_ip);
    }

    /* Get tacacs server indices */
    err = agentd_ns_mdc_array_get_matching_indices (agentd_mcc, TACACS_SERVER_ROOT_NODE, NULL,
                           "address", host_ip, TACACS_SERVER_HOST_INDEX_POS,
                           bn_db_revision_id_none,
                           NULL, &host_indices, NULL, NULL);
    bail_error (err);

    num_hosts = uint32_array_length_quick (host_indices);

    if (num_hosts == 0) { /* A new host & its options are being configured */
        err = bn_binding_array_new (&barr);
        bail_error (err);
        err = bn_binding_new_str_autoinval (&binding, binding_name, ba_value, param_type, 0, param_value);
        bail_error_null (err, binding);
        err = bn_binding_array_append (barr, binding);
        bail_error (err);

        if (is_param) {
            lc_log_debug (jnpr_log_level, "Configuring %s for a new host", binding_name);

            err = agentd_array_set (agentd_mcc, &tacacs_server_ctxt, context->mod_bindings, 
                                         TACACS_SERVER_ROOT_NODE, barr, &code, NULL);
            bail_error (err);
        } else { 
            lc_log_debug (jnpr_log_level, "Configuring a new host");

            err = agentd_array_append(agentd_mcc, &tacacs_server_ctxt, context->mod_bindings, 
                                         TACACS_SERVER_ROOT_NODE, barr, &code, NULL);
            bail_error (err);
        }
    } else if (is_param) { /* Host already exists. Configure only its settings*/
        lc_log_debug (jnpr_log_level, "Configuring %s for an existing host", binding_name);
        snprintf (node_pat, sizeof(node_pat), "%s/%d/%s", TACACS_SERVER_ROOT_NODE,
                                            uint32_array_get_quick(host_indices, 0), binding_name);
        err = agentd_append_req_msg (context, node_pat, SUBOP_MODIFY, param_type, param_value);
        bail_error (err);
    } else {
        /* Not a valid case. Ignore */
        lc_log_debug (jnpr_log_level, "Invalid case");
    }

bail:
    bn_binding_array_free (&barr);
    bn_binding_free (&binding);
    uint32_array_free (&host_indices);

    return err;
}

static int
agentd_delete_tacacs_host (agentd_context_t * context, agentd_binding *br_cmd, void *cb_arg, char **cli_buff) {
    int err = 0;
    str_value_t host_ip = {0};
    uint32_array *host_indices = NULL;
    uint32 num_hosts = 0;
    subop_t op = OP_NONE;
    bn_type param_type = bt_NONE;
    node_name_t node_pat = {0};
    char * binding_name = NULL;

    bail_null (cb_arg);
    binding_name = ((char *) cb_arg);

    if (!strcmp (binding_name, "address")) {
        param_type = bt_ipv4addr;
    } else if (!strcmp (binding_name, "key")) {
        param_type = bt_string;
    } else if (!strcmp (binding_name, "timeout")) {
        param_type = bt_duration_sec;
    } else {
        lc_log_basic (LOG_NOTICE, "Invalid binding name");
        goto bail;
    }

    /* args[1] contains the host ip */
    snprintf (host_ip, sizeof(host_ip), "%s", br_cmd->arg_vals.val_args[1]);

    /* Get tacacs server indices */
    err = agentd_ns_mdc_array_get_matching_indices (agentd_mcc, TACACS_SERVER_ROOT_NODE, NULL,
                           "address", host_ip, TACACS_SERVER_HOST_INDEX_POS,
                           bn_db_revision_id_none,
                           NULL, &host_indices, NULL, NULL);
    bail_error (err);

    num_hosts = uint32_array_length_quick (host_indices);
    if (num_hosts == 0) { /* Host doesnt exist*/
        /* This should not happen */
        lc_log_debug (jnpr_log_level, "Invalid tacacs host to delete");
        goto bail;
    }

    if (br_cmd->arg_vals.nargs > 2) {
        /* Host settings are deleted. */
        snprintf(node_pat, sizeof(node_pat), "%s/%d/%s",
                   TACACS_SERVER_ROOT_NODE, uint32_array_get_quick(host_indices, 0), binding_name);
        op = SUBOP_RESET;
    } else {
        snprintf(node_pat, sizeof(node_pat), "%s/%d",
                   TACACS_SERVER_ROOT_NODE, uint32_array_get_quick(host_indices,0));
        op = SUBOP_DELETE;
    }

    err = agentd_append_req_msg (context, node_pat, op, param_type, "");
    bail_error (err);

bail:
    uint32_array_free(&host_indices);
    return err;
}

