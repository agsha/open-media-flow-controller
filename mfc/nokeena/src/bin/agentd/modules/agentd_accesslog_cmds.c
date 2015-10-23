#include "agentd_mgmt.h"
#include "agentd_op_cmds_base.h"
#include "agentd_utils.h"

extern int jnpr_log_level;
extern md_client_context * agentd_mcc;

/* ---- Function prototype for initializing / deinitializing module ---- */

int 
agentd_accesslog_cmds_init(
        const lc_dso_info *info,
        void *context);
int
agentd_accesslog_cmds_deinit(
        const lc_dso_info *info,
        void *context);

/* ---- Configuration custom handler prototype ---- */ 


/* ---- Operational command handler prototype and structure definition ---- */ 

int agentd_op_show_accesslog_profiles (agentd_context_t *context, void *data);

/*---------------------------------------------------------------------------*/
int 
agentd_accesslog_cmds_init(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;
    /* Translation table array for configuration commands */
    static  mfc_agentd_nd_trans_tbl_t accesslog_cmds_trans_tbl[] =  {
        #include "../translation/rules_accesslog.inc"
        #include "../translation/rules_accesslog_delete.inc"
        TRANSL_ENTRY_NULL
    };

    /* Translation table array for operational commands */
    oper_table_entry_t op_tbl[] = {
        {"/accesslog/mfc-cluster/*/*", DUMMY_WC_ENTRY, agentd_op_show_accesslog_profiles, NULL},
        OP_ENTRY_NULL
    };

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    err = agentd_register_cmds_array (accesslog_cmds_trans_tbl);
    bail_error (err);

    err = agentd_register_op_cmds_array (op_tbl);
    bail_error (err);

bail:
    return err;
}

int
agentd_accesslog_cmds_deinit(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;
    lc_log_debug (jnpr_log_level, "agentd_accesslog_cmds_deinit called");

bail:
    return err;
}

int agentd_op_show_accesslog_profiles (agentd_context_t *context, void *data) {
    int err = 0;
    oper_table_entry_t *cmd = (oper_table_entry_t *) data;
    node_name_t node_base = {0};
    tstr_array *profile_arr = NULL;
    uint32 profile_count = 0;

    agentd_dump_op_cmd_details (context);

    err = mdc_get_binding_children_tstr_array(agentd_mcc , NULL, NULL, &profile_arr, "/nkn/accesslog/config/profile", NULL);
    bail_error_null (err, profile_arr);

    profile_count = tstr_array_length_quick(profile_arr);

    OP_EMIT_TAG_START(context, ODCI_MFC_ACCESSLOG_LIST);
    for (uint32 i = 0; i < profile_count; i++) {
        const char *profile_name = tstr_array_get_str_quick (profile_arr, i);

        OP_EMIT_TAG_START(context, ODCI_ACCESSLOG);
            OP_EMIT_TAG_VALUE(context, ODCI_PROFILER_NAME, profile_name);
        OP_EMIT_TAG_END(context, ODCI_ACCESSLOG);
    }
    OP_EMIT_TAG_END(context, ODCI_MFC_ACCESSLOG_LIST);

bail:
    tstr_array_free (&profile_arr);
    return err;
}

