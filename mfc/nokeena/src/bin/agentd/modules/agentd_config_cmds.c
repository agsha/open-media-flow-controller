#include "agentd_mgmt.h"
#include "agentd_op_cmds_base.h"

extern int jnpr_log_level;
extern md_client_context * agentd_mcc;

/* ---- Function prototype for initializing / deinitializing module ---- */

int 
agentd_config_cmds_init(
        const lc_dso_info *info,
        void *context);
int
agentd_config_cmds_deinit(
        const lc_dso_info *info,
        void *context);

/* ---- Configuration custom handler prototype ---- */ 


/* ---- Operational command handler prototype and structure definition ---- */ 

int agentd_op_req_reset_config_factory (agentd_context_t *context, void *data);

/*---------------------------------------------------------------------------*/
int 
agentd_config_cmds_init(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;

    /* Translation table array for operational commands */
    oper_table_entry_t op_tbl[] = {
        {"/reset-config/mfc-cluster/*", DUMMY_WC_ENTRY, agentd_op_req_reset_config_factory, NULL},
        OP_ENTRY_NULL
    };

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    err = agentd_register_op_cmds_array (op_tbl);
    bail_error (err);

bail:
    return err;
}

int
agentd_config_cmds_deinit(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;
    lc_log_debug (jnpr_log_level, "agentd_config_cmds_deinit called");

bail:
    return err;
}

/* This function handles the following pattern */
// "/reset-config/mfc-cluster/*"
int agentd_op_req_reset_config_factory(agentd_context_t *context, void *data)
{
    int err = 0;
    oper_table_entry_t *cmd = (oper_table_entry_t *)data;
    const char *mfc_cluster_name = NULL;
    uint32 ret_code = 0;
    tstring * ret_msg = NULL;

    lc_log_debug(jnpr_log_level, "got %s", cmd->pattern);

    mfc_cluster_name =  tstr_array_get_str_quick (context->op_cmd_parts, POS_CLUSTER_NAME);
    bail_null (mfc_cluster_name);

    /* Send action to reset the configuration to factory defaults */
    /* Basic configuration - Licenses, Host keys,
       configuration for network connectivity - interfaces, routes, ARP 
       configuration for dmi connectivity - dmi, web, virt
       should be preserved 
    */
    err = mdc_send_action_with_bindings_str_va (agentd_mcc, &ret_code, &ret_msg,
                                                 "/mgmtd/db/new_in_place", 1,
                                                 "import_id", bt_string, "keep_basic_reach_dmi");
    bail_error (err);
    lc_log_debug (jnpr_log_level, "mdc_send_action- Ret code: %d, Ret message: %s",
                             ret_code, ts_str(ret_msg));
    if (ret_code != 0) goto bail;

bail :
    if (ret_code != 0) {
        set_ret_code (context, ret_code);
        set_ret_msg (context, ts_str(ret_msg));
    }
    return err;
}

