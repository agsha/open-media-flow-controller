#include "agentd_mgmt.h"
#include "agentd_op_cmds_base.h"
#include "agentd_utils.h"

extern int jnpr_log_level;
extern md_client_context * agentd_mcc;

/* TODO - Handling array context by modules */
extern  agentd_array_move_context sshv2_authkey;   //Context for sshv2 public auth key 

/* ---- Function prototype for initializing / deinitializing module ---- */

int 
agentd_ssh_cmds_init(
        const lc_dso_info *info,
        void *context);
int
agentd_ssh_cmds_deinit(
        const lc_dso_info *info,
        void *context);

/* ---- Configuration custom handler prototype ---- */ 
int
agentd_add_ssh_client_authkey(agentd_context_t *context,
        agentd_binding *br_cmd, void *cb_arg, char **cli_buff);

int
agentd_delete_ssh_client_authkey(agentd_context_t *context,
                agentd_binding *br_cmd, void *cb_arg, char **cli_buff);

/* ---- Operational command handler prototype and structure definition ---- */ 


/*---------------------------------------------------------------------------*/
int 
agentd_ssh_cmds_init(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;
    /* Translation table array for configuration commands */
    static  mfc_agentd_nd_trans_tbl_t ssh_cmds_trans_tbl[] =  {
        #include "../translation/rules_ssh.inc"
        #include "../translation/rules_ssh_delete.inc"
        TRANSL_ENTRY_NULL
    };

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    err = agentd_register_cmds_array (ssh_cmds_trans_tbl);
    bail_error (err);

bail:
    return err;
}

int
agentd_ssh_cmds_deinit(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;
    lc_log_debug (jnpr_log_level, "agentd_ssh_cmds_deinit called");

bail:
    return err;
}

int
agentd_add_ssh_client_authkey(agentd_context_t *context,
        agentd_binding *br_cmd, void *cb_arg, char **cli_buff)
{
    int err = 0;
    node_name_t node_name = {0};
    str_value_t username  = {0};
    bn_binding_array *barr = NULL;
    bn_binding *binding = NULL;
    uint32 code = 0;

    /* args[1] has the username and args[2] has the public auth key */
    snprintf(username, sizeof(username), "%s",  br_cmd->arg_vals.val_args[1]);
    snprintf(node_name, sizeof(node_name),  (char *)cb_arg, username);

    err = bn_binding_array_new(&barr);
    bail_error_null(err, barr);

    err = bn_binding_new_str_autoinval(&binding, "public", ba_value, bt_string, 0,
                br_cmd->arg_vals.val_args[2]);
    bail_error_null(err, binding);

    err = bn_binding_array_append(barr, binding);
    bail_error(err);

    err = agentd_array_append(agentd_mcc, &sshv2_authkey, context->mod_bindings,
            node_name, barr, &code, NULL);
    bail_error(err);

bail:
    bn_binding_array_free(&barr);
    bn_binding_free(&binding);
    return err;
}

int
agentd_delete_ssh_client_authkey(agentd_context_t *context,
                agentd_binding *br_cmd, void *cb_arg, char **cli_buff)
{
    int err = 0;
    node_name_t node_name = {0}, username = {0}, key = {0};
    uint32_array *key_indices = NULL;
    uint32 num_keys = 0;
    node_name_t node_to_delete = {0};

    //args[1] has the user name
    snprintf(username, sizeof(username), "%s",  br_cmd->arg_vals.val_args[1]);
    // args[2] contains the key
    snprintf(key, sizeof(key), "%s", br_cmd->arg_vals.val_args[2]);
    snprintf(node_name, sizeof(node_name), (char *)(cb_arg), username);

    /* Get the index of the particular key */
    err = agentd_ns_mdc_array_get_matching_indices(agentd_mcc,
                        node_name, NULL, "public", key,6,
                        bn_db_revision_id_none,
                        NULL, &key_indices, NULL, NULL);
    bail_error_null(err, key_indices);
    num_keys = uint32_array_length_quick(key_indices);
    if(num_keys == 0){
        /* Ignore. Cannot happen */
    } else {
        snprintf(node_to_delete, sizeof(node_to_delete),"%s/%d",
                    node_name, uint32_array_get_quick(key_indices,0));
        err = agentd_append_req_msg (context, node_to_delete, SUBOP_DELETE, bt_string, "");
        bail_error(err);
    }

bail:
    uint32_array_free(&key_indices);
    return err;
}

