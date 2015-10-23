#include "agentd_mgmt.h"
#include "agentd_op_cmds_base.h"
#include "agentd_utils.h"

extern int jnpr_log_level;
extern md_client_context * agentd_mcc;

/* ---- Function prototype for initializing / deinitializing module ---- */

int 
agentd_media_cache_cmds_init(
        const lc_dso_info *info,
        void *context);
int
agentd_media_cache_cmds_deinit(
        const lc_dso_info *info,
        void *context);

/* ---- Configuration custom handler prototype ---- */ 


/* ---- Operational command handler prototype and structure definition ---- */ 


/*---------------------------------------------------------------------------*/
int 
agentd_media_cache_cmds_init(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;
    /* Translation table array for configuration commands */
    static  mfc_agentd_nd_trans_tbl_t media_cache_cmds_trans_tbl[] =  {
        #include "../translation/rules_media-cache.inc"
        #include "../translation/rules_media-cache_delete.inc"
        TRANSL_ENTRY_NULL
    };

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    err = agentd_register_cmds_array (media_cache_cmds_trans_tbl);
    bail_error (err);

bail:
    return err;
}

int
agentd_media_cache_cmds_deinit(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;
    lc_log_debug (jnpr_log_level, "agentd_media_cache_cmds_deinit called");

bail:
    return err;
}

