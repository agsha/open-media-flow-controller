#include "agentd_mgmt.h"
#include "agentd_op_cmds_base.h"
#include "agentd_utils.h"

extern int jnpr_log_level;
extern md_client_context * agentd_mcc;

/* ---- Function prototype for initializing / deinitializing module ---- */

int 
agentd_snmp_cmds_init(
        const lc_dso_info *info,
        void *context);
int
agentd_snmp_cmds_deinit(
        const lc_dso_info *info,
        void *context);

/* ---- Configuration custom handler prototype ---- */ 
int
agentd_snmp_set_trap_version(agentd_context_t *context,
	agentd_binding *br_cmd, void *cb_arg, char **cli_buff);


/* ---- Operational command handler prototype and structure definition ---- */ 


/*---------------------------------------------------------------------------*/
int 
agentd_snmp_cmds_init(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;
    /* Translation table array for configuration commands */
    static  mfc_agentd_nd_trans_tbl_t snmp_cmds_trans_tbl[] =  {
        #include "../translation/rules_snmp.inc"
        #include "../translation/rules_snmp_delete.inc"
        TRANSL_ENTRY_NULL
    };

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    err = agentd_register_cmds_array (snmp_cmds_trans_tbl);
    bail_error (err);

bail:
    return err;
}

int
agentd_snmp_cmds_deinit(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;
    lc_log_debug (jnpr_log_level, "agentd_snmp_cmds_deinit called");

bail:
    return err;
}

int
agentd_snmp_set_trap_version(agentd_context_t *context,
	agentd_binding *br_cmd, void *cb_arg, char **cli_buff)
{
    int err = 0;
    node_name_t node_name = {0};
    temp_buf_t  version = {0};
    
    snprintf(node_name, sizeof(node_name), "/snmp/trapsink/sink/%s/type", br_cmd->arg_vals.val_args[1]);
    snprintf(version, sizeof(version), "trap-v%s",br_cmd->arg_vals.val_args[2]); 
    err = agentd_append_req_msg (context, node_name, SUBOP_MODIFY, TYPE_STR, version);
    bail_error (err);

bail:
    return err;
}

