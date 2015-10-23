#include "agentd_mgmt.h"
#include "agentd_op_cmds_base.h"
#include "agentd_utils.h"

extern int jnpr_log_level;
extern md_client_context * agentd_mcc;

/* ---- Function prototype for initializing / deinitializing module ---- */

int 
agentd_virtual_player_cmds_init(
        const lc_dso_info *info,
        void *context);
int
agentd_virtual_player_cmds_deinit(
        const lc_dso_info *info,
        void *context);

/* ---- Configuration custom handler prototype ---- */ 

int
ns_virtual_player_yahoo_set_default(agentd_context_t *context, agentd_binding *br_cmd, void *cb_arg,
    char **cli_buff);

/* ---- Operational command handler prototype and structure definition ---- */ 


/*---------------------------------------------------------------------------*/
int 
agentd_virtual_player_cmds_init(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;
    /* Translation table array for configuration commands */
    static  mfc_agentd_nd_trans_tbl_t virtual_player_cmds_trans_tbl[] =  {
        #include "../translation/rules_virtual_player.inc"
        #include "../translation/rules_virtual_player_delete.inc"
        TRANSL_ENTRY_NULL
    };

    /* Translation table array for operational commands */

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    err = agentd_register_cmds_array (virtual_player_cmds_trans_tbl);
    bail_error (err);

bail:
    return err;
}

int
agentd_virtual_player_cmds_deinit(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;
    lc_log_debug (jnpr_log_level, "agentd_virtual_player_cmds_deinit called");

bail:
    return err;
}

int
ns_virtual_player_yahoo_set_default(agentd_context_t *context, agentd_binding *br_cmd, void *cb_arg,
    char **cli_buff)
{
    int err = 0;
    node_name_t p = {0};
    node_name_t s = {0};
    const char *vp_name = NULL;
    uint16 ret_code = 0;
    tstring *ret_msg = NULL;

    //vp_name is the second argument in the br_cmd.
    vp_name = br_cmd->arg_vals.val_args[1];

    snprintf(p, sizeof(p), "/nkn/nvsd/virtual_player/config/%s", vp_name);
    snprintf(s, sizeof(s), "%s/type", p);
    err = agentd_append_req_msg (context, s, SUBOP_MODIFY, bt_uint32, "3");
    bail_error(err);
    snprintf(s, sizeof(s), "%s/req_auth/digest", p);
    err = agentd_append_req_msg (context, s, SUBOP_MODIFY, bt_string, "md-5");
    bail_error(err);
    snprintf(s, sizeof(s), "%s/req_auth/stream_id/uri_query", p);
    err = agentd_append_req_msg(context, s, SUBOP_MODIFY, bt_string, "streamid");
    bail_error(err);
    snprintf(s, sizeof(s), "%s/req_auth/auth_id/uri_query", p);
    err = agentd_append_req_msg (context, s, SUBOP_MODIFY, bt_string, "authid");
    bail_error(err);
    snprintf(s, sizeof(s), "%s/req_auth/secret/value", p);
    err = agentd_append_req_msg(context, s, SUBOP_MODIFY, bt_string, "ysecret");
    bail_error(err);
    snprintf(s, sizeof(s), "%s/req_auth/time_interval", p);
    err = agentd_append_req_msg (context, s, SUBOP_MODIFY, bt_uint32, "15");
    bail_error(err);
    snprintf(s, sizeof(s), "%s/req_auth/match/uri_query", p);
    err = agentd_append_req_msg (context, s, SUBOP_MODIFY, bt_string, "ticket");
    bail_error(err);
    snprintf(s, sizeof(s), "%s/req_auth/active", p);
    err = agentd_append_req_msg (context, s, SUBOP_MODIFY, bt_bool, "true");
    bail_error(err);
    snprintf(s, sizeof(s), "%s/health_probe/uri_query", p);
    err = agentd_append_req_msg (context, s, SUBOP_MODIFY, bt_string, "no-cache");
    bail_error(err);
    snprintf(s, sizeof(s), "%s/health_probe/match", p);
    err = agentd_append_req_msg (context, s, SUBOP_MODIFY, bt_string, "1");
    bail_error(err);
    snprintf(s, sizeof(s), "%s/health_probe/active", p);
    err = agentd_append_req_msg (context, s, SUBOP_MODIFY, bt_bool, "true");
    bail_error(err);

bail:
    return err;
}

