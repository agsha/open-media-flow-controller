#include "agentd_mgmt.h"
#include "agentd_op_cmds_base.h"
#include "agentd_utils.h"
#include "proc_utils.h"
#include "agentd_cli.h"

extern int jnpr_log_level;
extern md_client_context * agentd_mcc;
#define NKN_MFA_BACKUP_DIR "/tmp/mfa_backup/"
int
agentd_backup_restore_init(
        const lc_dso_info *info,
        void *context);
int
agentd_backup_restore_deinit(
        const lc_dso_info *info,
        void *context);

int agentd_backup_config(agentd_context_t *context, void *data);
int agentd_restore_config(agentd_context_t *context, void *data);
int agentd_tar_gzip_dir(const char *dir_path);

int
agentd_backup_restore_init(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;

    oper_table_entry_t op_tbl[] = {
        {"/backup-config/mfc-cluster/*", DUMMY_WC_ENTRY, agentd_backup_config , NULL},
        {"/restore-config/mfc-cluster/*", DUMMY_WC_ENTRY, agentd_restore_config , NULL},
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
agentd_backup_restore_deinit(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;

    lc_log_debug (jnpr_log_level, "agentd_backup_restore_deinit called");

bail:
    return err;
}

int
agentd_backup_config(agentd_context_t *context, void *data)
{
    int err = 0;
    file_path_t mfa_backup_files  = {0};
    uint32 ret_code = 0;
    tstring * ret_msg = NULL, *active_db = NULL;
    const char *src_db = NULL;
    tbool exists = false;
    tstring * error = NULL, *output = NULL;
    tbool success = false;
    tstr_array * cmds = NULL;


    err = lf_test_exists(NKN_MFA_BACKUP_DIR, &exists);
    bail_error_errno(err, "Dir: %s", NKN_MFA_BACKUP_DIR);

    if (exists) {
	err = lf_remove_tree(NKN_MFA_BACKUP_DIR);
	bail_error_errno(err, "dir: %s", NKN_MFA_BACKUP_DIR);
    }

    exists = false;
    err = lf_test_exists("/tmp/mfa_backup.tgz", &exists);
    bail_error_errno(err, "File: %s", "/tmp/mfa_backup.tgz");

    if (exists) {
	err = lf_remove_file("/tmp/mfa_backup.tgz");
	bail_error_errno(err, "File: %s","/tmp/mfa_backup.tgz");
    }

    /* generate service files */
    snprintf(mfa_backup_files, sizeof(mfa_backup_files),
	    NKN_MFA_BACKUP_DIR"/mfa_backup_files.tgz");

    err = lf_ensure_dir(NKN_MFA_BACKUP_DIR, 0755);
    bail_error_errno(err, "%s",NKN_MFA_BACKUP_DIR);

    err = mdc_send_action_with_bindings_str_va (agentd_mcc, &ret_code, &ret_msg,
                "/nkn/debug/generate/config_files", 2,
                "file_name", bt_string, mfa_backup_files,
                "cmd", bt_uint16, "1");
    bail_error(err);

    lc_log_debug (jnpr_log_level, "Generate files - Ret code: %d,"
	    "Ret message: %s", ret_code, ts_str(ret_msg));

    /* get active database */
    err = mdc_get_binding_tstr(agentd_mcc, NULL, NULL, false, &active_db,
	    "/mgmtd/db/info/running/db_name", NULL);
    bail_error_null(err, active_db);
    src_db = ts_str(active_db);

    /* copy active database to target database */
    err = mdc_send_action_with_bindings_str_va (agentd_mcc, &ret_code, &ret_msg,
                "/mgmtd/db/copy", 2,
                "from_db_name", bt_string, src_db,
                "to_db_name", bt_string, "mfa_backup_db");
    bail_error(err);

    /* copy the database file to baskup directory */
    err = lf_move_file("/config/db/mfa_backup_db",
	    NKN_MFA_BACKUP_DIR"mfa_backup_db");
    bail_error(err);

    err = agentd_tar_gzip_dir(NKN_MFA_BACKUP_DIR);
    bail_error(err);

    ret_code = 0;
    ts_free(&ret_msg);
    err = mdc_send_action_with_bindings_str_va (agentd_mcc, &ret_code, &ret_msg,
                   "/file/transfer/upload", 3,
                   "remote_url", bt_uri,
		   "scp://root@10.84.77.10/var/tmp/mfa_backup.tgz",
                   "password", bt_string, "n1keenA",
                   "local_path", bt_string, "/tmp/mfa_backup.tgz");
    bail_error(err);

    lc_log_debug (jnpr_log_level, "Generate files - Ret code: %d,"
	    "Ret message: %s", ret_code, ts_str(ret_msg));

    err = tstr_array_new_va_str (&cmds, NULL, 3,
                                "enable","configure terminal",
                                 "show configuration");
    bail_error (err);

    err = agentd_execute_cli_commands (cmds, &error, &output, &success);
    bail_error(err);

    OP_EMIT_TAG_START(context, ODCI_MFC_BACKUP_FILE);
	OP_EMIT_TAG_VALUE(context, ODCI_BACKUP_FILE,
		"/var/tmp/mfa_backup.tgz");
	OP_EMIT_TAG_VALUE(context, ODCI_TEXT_CONFIG, ts_str(output));
    OP_EMIT_TAG_END(context, ODCI_MFC_BACKUP_FILE);

bail:
    ts_free(&ret_msg);
    return err;
}


int
agentd_restore_config(agentd_context_t *context, void *data)
{
    int err = 0;

bail:
    return err;

}

int
agentd_tar_gzip_dir(const char *dir_path)
{
    int err = 0;
    lc_launch_params *lp = NULL;
    lc_launch_result lr = LC_LAUNCH_RESULT_INIT;
    tbool normal_exit = false;

    err = lc_launch_params_get_defaults(&lp);
    bail_error(err);

    err = ts_new_str(&(lp->lp_path), "/bin/tar");
    bail_error(err);

    err = tstr_array_new_va_str(&(lp->lp_argv), NULL, 6, "/bin/tar",
                                "-zcf", "/tmp/mfa_backup.tgz",
				"-C", "/tmp", "mfa_backup");
    bail_error(err);

    //tstr_array_dump(lp->lp_argv, "tar args");
    lp->lp_env_opts = eo_preserve_all;
    lp->lp_log_level = LOG_INFO;
    lp->lp_wait = true;
    lp->lp_priority = PRIO_MAX; /* Run gzip with low priority */

    /* XXX #dep/args: gzip */
    err = lc_launch(lp, &lr);
    bail_error(err);

    err = lc_check_exit_status(lr.lr_exit_status, &normal_exit, 
                               "gzip file %s", dir_path); bail_error(err);

    if (!normal_exit) {
        err = lc_err_generic_error;
    }

 bail:
    lc_launch_params_free(&lp);
    lc_launch_result_free_contents(&lr);
    return(err);
}
