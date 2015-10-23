#include "agentd_mgmt.h"
#include "agentd_op_cmds_base.h"
#include "agentd_utils.h"

extern int jnpr_log_level;
extern md_client_context * agentd_mcc;

/* ---- Function prototype for initializing / deinitializing module ---- */

int 
agentd_geodb_service_cmds_init(
        const lc_dso_info *info,
        void *context);
int
agentd_geodb_service_cmds_deinit(
        const lc_dso_info *info,
        void *context);

/* ---- Configuration custom handler prototype ---- */ 


/* ---- Operational command handler prototype and structure definition ---- */ 

#define POS_MAXMIND_DWNLD_URL 5 /* Position of maxmind download URL string in oper_cmd pattern*/
#define POS_MAXMIND_INSTALL_FILE 5 /* Position of maxmind install filename string */
#define GEO_MAXMIND_FILES_PATH "/nkn/maxmind/downloads" /* Local directory to store maxmind db files */

int agentd_op_req_geodb_download (agentd_context_t * context, void *data);
int agentd_op_req_geodb_install (agentd_context_t * context, void *data);

/*---------------------------------------------------------------------------*/
int 
agentd_geodb_service_cmds_init(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;
    /* Translation table array for configuration commands */
    static  mfc_agentd_nd_trans_tbl_t geodb_service_cmds_trans_tbl[] =  {
        #include "../translation/rules_geodb_service.inc"
        #include "../translation/rules_geodb_service_delete.inc"
        TRANSL_ENTRY_NULL
    };

    /* Translation table array for operational commands */
    oper_table_entry_t op_tbl[] = {
        {"/maxmind/download/mfc-cluster/*/url/*", DUMMY_WC_ENTRY, agentd_op_req_geodb_download, NULL},
        {"/maxmind/install/mfc-cluster/*/filename/*", DUMMY_WC_ENTRY, agentd_op_req_geodb_install, NULL},
        OP_ENTRY_NULL
    };

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    err = agentd_register_cmds_array (geodb_service_cmds_trans_tbl);
    bail_error (err);

    err = agentd_register_op_cmds_array (op_tbl);
    bail_error (err);

bail:
    return err;
}

int
agentd_geodb_service_cmds_deinit(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;
    lc_log_debug (jnpr_log_level, "agentd_geodb_service_cmds_deinit called");

bail:
    return err;
}

int agentd_op_req_geodb_download (agentd_context_t * context, void *data) {
/* This function handles the following command pattern */
// "/maxmind/download/mfc-cluster/*/url/*"
    int err = 0;
    uint32 ret_code = 0;
    tstring * ret_msg = NULL;
    const char * remote_url = NULL;
    tstring * dest_filename = NULL;
    str_value_t db_file_mode_str = {0};
    oper_table_entry_t *cmd = (oper_table_entry_t *)data;

    lc_log_debug (jnpr_log_level, "got %s", cmd->pattern);
    agentd_dump_op_cmd_details (context);
    remote_url = tstr_array_get_str_quick(context->op_cmd_parts, POS_MAXMIND_DWNLD_URL);
    bail_null(remote_url);
    lc_log_debug (jnpr_log_level, "URL: %s", remote_url);

    /* Verify the destination filename. We save the target file in the same name as input file */
    err = lf_path_last(remote_url, &dest_filename);
    bail_error (err);
    if (dest_filename == NULL) {
        lc_log_debug (LOG_ERR, "Destination filename cannot be null");
        err = 1;
        goto bail;
    }
    snprintf(db_file_mode_str, sizeof(db_file_mode_str), "%d", 0600);

    /* Send request to fetch file */
    err = mdc_send_action_with_bindings_str_va (agentd_mcc, &ret_code, &ret_msg,
                   "/file/transfer/download", 4,
                   "remote_url", bt_string, remote_url,
                   "local_dir", bt_string, GEO_MAXMIND_FILES_PATH,
                   "allow_overwrite", bt_bool, lc_bool_to_str(true),
                   "mode", bt_uint16, db_file_mode_str); 

    bail_error(err);
    if (ret_code != 0) {
        lc_log_debug (jnpr_log_level, "mdc_send_action- Ret code: %d, Ret message: %s",
                             ret_code, ts_str(ret_msg));
        set_ret_code(context, ret_code);
        set_ret_msg(context, ts_str(ret_msg));
    }

bail:
    ts_free (&ret_msg);
    ts_free(&dest_filename);

    return err;
}

int agentd_op_req_geodb_install (agentd_context_t * context, void *data) {
/* This function handles the following command pattern */
// "/maxmind/install/mfc-cluster/*/filename/*"
    int err = 0;
    uint32 ret_code = 0;
    tstring * ret_msg = NULL;
    const char * filename = NULL;
    temp_buf_t path_filename = {0};
    oper_table_entry_t *cmd = (oper_table_entry_t *)data;

    lc_log_debug (jnpr_log_level, "got %s", cmd->pattern);
    agentd_dump_op_cmd_details (context);
    filename = tstr_array_get_str_quick(context->op_cmd_parts, POS_MAXMIND_INSTALL_FILE);
    bail_null(filename);
    lc_log_debug (jnpr_log_level, "Name of the file to install: %s", filename);

    snprintf (path_filename, sizeof(path_filename), "%s/%s", GEO_MAXMIND_FILES_PATH,filename);

    /* Send request to install file */
    err = mdc_send_action_with_bindings_str_va (agentd_mcc, &ret_code, &ret_msg,
                   "/nkn/geodb/actions/install", 1,
                   "filename", bt_string, path_filename);
    bail_error(err);

    if (ret_code != 0) {
        lc_log_debug (jnpr_log_level, "mdc_send_action- Ret code: %d, Ret message: %s",
                             ret_code, ts_str(ret_msg));
        set_ret_code(context, ret_code);
        set_ret_msg(context, ts_str(ret_msg));
    }

bail:
    ts_free (&ret_msg);
    return err;
}

