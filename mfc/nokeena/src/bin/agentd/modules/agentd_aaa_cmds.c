#include "agentd_mgmt.h"

extern int jnpr_log_level;

/* ---- Function prototype for initializing / deinitializing module ---- */

int 
agentd_aaa_cmds_init(
        const lc_dso_info *info,
        void *context);
int
agentd_aaa_cmds_deinit(
        const lc_dso_info *info,
        void *context);

/* ---- Configuration custom handler prototype ---- */ 

static int 
agentd_set_auth_order_flag (agentd_context_t * context, agentd_binding *br_cmd, void *cb_arg, char **cli_buff);

/* ---- Operational command handler prototype and structure definition ---- */ 


/* --- Translation table array for configuration commands --- */

static  mfc_agentd_nd_trans_tbl_t aaa_cmds_trans_tbl[] =  {

TRANSL_ENTRY(PREPEND_TRANS_STR"systemaaaauthenticationlogindefaultmethod*")
TRANSL_CUST (agentd_set_auth_order_flag, "")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemaaaauthenticationlogindefaultmethod*")
TRANSL_CUST (agentd_set_auth_order_flag, "")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemaaaauthorizationmapdefault-user*")
TRANSL_NUM_NDS(2)
TRANSL_NDS_BASIC("/aaa/author/default-user",
		TYPE_STR,
		ND_NORMAL,
		0,NULL)
TRANSL_NDS_BASIC("/aaa/author/map-order",
		TYPE_STR,
		ND_HARDCODED,
		0, "remote-first")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemaaaauthorizationmapdefault-user*")
TRANSL_NUM_NDS(2)
RESET_TRANSL_NDS_BASIC("/aaa/author/default-user",
                TYPE_STR,
                ND_NORMAL,
                0,NULL)
RESET_TRANSL_NDS_BASIC("/aaa/author/map-order",
                TYPE_STR,
                ND_HARDCODED,
                0, "remote-first")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY_NULL
};

/*---------------------------------------------------------------------------*/
int 
agentd_aaa_cmds_init(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    err = agentd_register_cmds_array (aaa_cmds_trans_tbl);
    bail_error (err);

bail:
    return err;
}

int
agentd_aaa_cmds_deinit(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;
    lc_log_debug (jnpr_log_level, "agentd_aaa_cmds_deinit called");

bail:
    return err;
}

/* ---- Configuration custom handler definitions ---- */

static int
agentd_set_auth_order_flag (agentd_context_t * context, agentd_binding *br_cmd, void *cb_arg, char **cli_buff) {
    int err = 0;

    lc_log_debug (jnpr_log_level, "Setting af_set_auth_order flag");
    context->conf_flags |= AF_SET_AUTH_ORDER;

bail:
    return err;
}

