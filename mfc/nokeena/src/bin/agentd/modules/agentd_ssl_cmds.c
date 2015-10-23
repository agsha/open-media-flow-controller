#include "agentd_mgmt.h"
#include "agentd_op_cmds_base.h"
#include "agentd_utils.h"

extern int jnpr_log_level;
extern md_client_context * agentd_mcc;

/* ---- Function prototype for initializing / deinitializing module ---- */

int 
agentd_ssl_cmds_init(
        const lc_dso_info *info,
        void *context);
int
agentd_ssl_cmds_deinit(
        const lc_dso_info *info,
        void *context);

/* ---- Configuration custom handler prototype ---- */ 


/* ---- Operational command handler prototype and structure definition ---- */ 

/* ---------- Translation table array for configuration commands ---------- */
static  mfc_agentd_nd_trans_tbl_t ssl_cmds_trans_tbl[] =  {

TRANSL_ENTRY(PREPEND_TRANS_STR"systemsslserver-authen*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/nkn/ssld/config/origin-req/server_auth",
                TYPE_BOOL,
                ND_HARDCODED,
                0,"true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemsslserver-authen*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/ssld/config/origin-req/server_auth",
                TYPE_BOOL,
                ND_HARDCODED,
                0,"false")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemsslcipher*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/nkn/ssld/config/origin-req/cipher",
		TYPE_STR,
		ND_NORMAL,
		0, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemsslcipher*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/ssld/config/origin-req/cipher",
                TYPE_STR,
                ND_NORMAL,
                0, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY_NULL
};

/*---------------------------------------------------------------------------*/
int 
agentd_ssl_cmds_init(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    err = agentd_register_cmds_array (ssl_cmds_trans_tbl);
    bail_error (err);

bail:
    return err;
}

int
agentd_ssl_cmds_deinit(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;
    lc_log_debug (jnpr_log_level, "agentd_ssl_cmds_deinit called");

bail:
    return err;
}

