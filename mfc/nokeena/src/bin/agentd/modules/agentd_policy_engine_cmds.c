#include "agentd_mgmt.h"
#include "agentd_utils.h"

extern int jnpr_log_level;
extern md_client_context * agentd_mcc;

#define POL_SCRIPT_LOC_VJX "/var/tmp/"
#define POL_SCRIPT_LOC_MFC "/nkn/policy_engine/"
#define VJX_SCP_URL "scp://root:n1keenA@10.84.77.10"

/* ---- Function prototype for initializing / deinitializing module ---- */

int 
agentd_policy_engine_cmds_init(
        const lc_dso_info *info,
        void *context);
int
agentd_policy_engine_cmds_deinit(
        const lc_dso_info *info,
        void *context);

/* ---- Configuration custom handler prototype ---- */ 

int 
agentd_set_ns_policy_binding (agentd_context_t * context,
              agentd_binding *abinding, void *cb_arg, char **cli_buff);

int 
agentd_del_ns_policy_binding (agentd_context_t * context,
              agentd_binding *abinding, void *cb_arg, char **cli_buff);

static int 
agentd_create_policy (agentd_context_t * context,
              agentd_binding *abinding, void *cb_arg, char ** cli_buff);

static int 
agentd_delete_policy (agentd_context_t * context,
              agentd_binding *abinding, void *cb_arg, char ** cli_buff);

int
agentd_rm_policy_file (void* context, agentd_binding *abinding, void *cb_arg);

/* ---- Operational command handler prototype and structure definition ---- */ 


/*---------------------------------------------------------------------------*/
int 
agentd_policy_engine_cmds_init(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;
    /* Translation table array for configuration commands */
    static  mfc_agentd_nd_trans_tbl_t policy_engine_cmds_trans_tbl[] =  {
        #include "../translation/rules_policy_engine.inc"
        #include "../translation/rules_policy_engine_delete.inc"
        TRANSL_ENTRY_NULL
    };

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    err = agentd_register_cmds_array (policy_engine_cmds_trans_tbl);
    bail_error (err);

bail:
    return err;
}

int
agentd_policy_engine_cmds_deinit(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;
    lc_log_debug (jnpr_log_level, "agentd_policy_engine_cmds_deinit called");

bail:
    return err;
}

/*---------------------------------------------------------------------------*/

/* This function fetches the policy file from remote location and 
   sets the policy configuration nodes */

static int
agentd_create_policy (agentd_context_t * context,
              agentd_binding *abinding, void *cb_arg, char ** cli_buff) {
    int err = 0;
    temp_buf_t remote_url = {0}, local_filename = {0}, db_file_mode_str = {0};
    uint32 ret_code = 0;
    tstring * ret_msg = NULL;
    node_name_t node_name = {0};

    /* args[2] has the policy name. 
    * Note: Policy file will be available in VJX in the same name 
    * with .tcl extension  in POL_SCRIPT_LOC_VJX directory 
    */
    const char *policy_name = abinding->arg_vals.val_args[2];
    bail_null (policy_name);

    /* Fetch the policy file from VJX */
    snprintf(remote_url, sizeof(remote_url), "%s%s%s.tcl", 
                           VJX_SCP_URL, POL_SCRIPT_LOC_VJX, policy_name);

    snprintf(local_filename, sizeof(local_filename), "%s.tcl.com", policy_name);

    snprintf (db_file_mode_str, sizeof(db_file_mode_str), "%d", 0600);

    /* Send request to fetch file */
    lc_log_debug (jnpr_log_level, "Fetching policy file from URL: %s", remote_url);

    err = mdc_send_action_with_bindings_str_va (agentd_mcc, &ret_code, &ret_msg,
                   "/file/transfer/download", 5,
                   "remote_url", bt_string, remote_url,
                   "local_dir", bt_string, POL_SCRIPT_LOC_MFC,
                   "local_filename", bt_string, local_filename,
                   "allow_overwrite", bt_bool, lc_bool_to_str(true),
                   "mode", bt_uint16, db_file_mode_str);

    bail_error(err);

    if (ret_code != 0) {
        lc_log_basic (LOG_ERR, "Fetching policy file returned - Code: %d, Message: %s",
                             ret_code, ts_str(ret_msg));
        set_ret_code(context, ret_code);
        set_ret_msg(context, ts_str(ret_msg));
        err = 1;
        bail_error (err);
    }
    /* Set policy nodes */
    lc_log_debug (jnpr_log_level, "Setting policy nodes");
    snprintf (node_name, sizeof(node_name), "/nkn/nvsd/policy_engine/config/policy/script/%s",
                                             policy_name);
    err = agentd_append_req_msg (context, node_name, bsso_modify, bt_string, policy_name);
    bail_error (err);

    /* Committing the policy script straightaway.
       Assumption is that the policy file is a valid script. 
       If not valid, the policy engine should handle it appropriately 
    */

    snprintf (node_name, sizeof(node_name), "/nkn/nvsd/policy_engine/config/policy/script/%s/commited",
                                            policy_name);
    err = agentd_append_req_msg (context, node_name, bsso_modify, bt_bool, "true");
    bail_error (err);

bail:
    ts_free (&ret_msg);
    return err;
}

/* Post commit handler for removing policy script file from MFC */
int 
agentd_rm_policy_file (void *context, agentd_binding *abinding, void *cb_arg) {
    int err = 0;
    const char *policy_name = (const char *) cb_arg;
    temp_buf_t file_name = {0};

    /* Delete policy script file stored in POL_SCRIPT_LOC_MFC directory */
    /*
    * Note: Policy file will be available in MFC in the same name
    * with .tcl.com extension  in POL_SCRIPT_LOC_MFC directory
    */
    bail_null (policy_name);
    snprintf(file_name, sizeof(file_name), "%s%s.tcl.com", POL_SCRIPT_LOC_MFC, policy_name);
    lc_log_debug(jnpr_log_level, "Deleting policy script : %s", file_name);

    err = lf_remove_file(file_name);
    if (err) {
        lc_log_basic (LOG_ERR, "Deleting policy file \"%s\" failed with errno: %d", file_name, errno);
        complain_error(err);
        err = 0;
    }

bail:
    return err;
}

static int
agentd_delete_policy (agentd_context_t * context,
              agentd_binding *abinding, void *cb_arg, char ** cli_buff) {
    int err = 0;
    node_name_t node_name = {0};
    const char * policy_name = NULL;
    temp_buf_t xpath = {0};
    int pol_cfg_exists = 0;

    /* args[2] has the policy name. */
    if (abinding) {
        policy_name = abinding->arg_vals.val_args[2];
    } else {
        policy_name = (const char *)cb_arg;
    }

    bail_null (policy_name);

    /* Do not delete the policy if it is configured in some other namespace */
    snprintf(xpath, sizeof(xpath), "//service/http/instance[policy-engine=\"%s\"]/name", policy_name);
    err = agentd_xml_node_exists (context, xpath, &pol_cfg_exists);
    bail_error (err);
    if (pol_cfg_exists) {
        lc_log_debug (jnpr_log_level, "Policy \'%s\' not deleted as it is associated with another namespace", 
                         policy_name);
        goto bail;
    }

    /* Add post-commit handler for removing policy script from MFC */
    err = agentd_save_post_handler (context, NULL, ((void *) policy_name), strlen(policy_name), agentd_rm_policy_file);
    bail_error (err);

    /* Delete policy node */
    snprintf(node_name, sizeof(node_name), "/nkn/nvsd/policy_engine/config/policy/script/%s",
                             policy_name);
    lc_log_debug (jnpr_log_level, "Deleting policy node: %s", node_name);

    err = agentd_append_req_msg (context, node_name, bsso_delete, bt_string, policy_name);
    bail_error (err);

bail:
    return err;
}
/* This function creates the policy and associates it to the namespace */

int
agentd_set_ns_policy_binding (agentd_context_t * context,
              agentd_binding *abinding, void *cb_arg, char **cli_buff) {
    int err = 0;
    node_name_t node_name = {0};
    tstring * old_policy = NULL;

    /* args[1] has the namespace name */
    const char *ns_name = abinding->arg_vals.val_args[1];
    /* args[2] has the policy name.  */
    const char *policy_name = abinding->arg_vals.val_args[2];

    bail_null(ns_name);
    bail_null(policy_name);

    /* Delete any policy already associated with the namespace */
    err = agentd_mdc_get_value_tstr_fmt(&old_policy,"/nkn/nvsd/namespace/%s/policy/file", ns_name);
    bail_error(err);
    if (old_policy && ts_nonempty(old_policy)) {
        lc_log_debug (jnpr_log_level, "Deleting previous policy '\%s\' bound to namespace \'%s\'",
                                          ts_str(old_policy), ns_name);
        err = agentd_delete_policy (context, NULL, ((void *)ts_str(old_policy)), cli_buff);
        bail_error (err);
    }

    /* Create policy file & bindings */
    err = agentd_create_policy (context, abinding, cb_arg, cli_buff);
    bail_error (err);

    /* Set namespace's policy node */
    lc_log_debug (jnpr_log_level, "Setting policy binding in namespace");
    snprintf (node_name, sizeof(node_name), "/nkn/nvsd/namespace/%s/policy/file", ns_name);
    err = agentd_append_req_msg (context, node_name, bsso_modify, bt_string, policy_name);
    bail_error (err);

bail:
    ts_free (&old_policy);
    return err;
}

/* This function deletes policy and removes association from namespace */
int 
agentd_del_ns_policy_binding (agentd_context_t * context,
              agentd_binding *abinding, void *cb_arg, char **cli_buff){
    int err = 0;
    node_name_t node_name = {0};

    /* args[1] has the namespace and args[2] has the policy name */
    const char *ns_name = abinding->arg_vals.val_args[1];
    const char *policy_name = abinding->arg_vals.val_args[2];

    bail_null(ns_name);
    bail_null(policy_name);

    /* Delete policy file and bindings */
    err = agentd_delete_policy (context, abinding, cb_arg, cli_buff);
    bail_error (err);

    /* Remove policy binding from namespace */
    lc_log_debug (jnpr_log_level, "Removing policy binding from namespace");
    snprintf(node_name, sizeof(node_name), "/nkn/nvsd/namespace/%s/policy/file", ns_name);
    err = agentd_append_req_msg (context, node_name, bsso_reset, bt_string, "");
    bail_error (err);

bail:
    return err;
}
