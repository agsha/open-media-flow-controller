#include "agentd_mgmt.h"

extern int jnpr_log_level;

/* ---- Function prototype for initializing / deinitializing module ---- */

int 
agentd_stats_alarm_cmds_init(
        const lc_dso_info *info,
        void *context);
int
agentd_stats_alarm_cmds_deinit(
        const lc_dso_info *info,
        void *context);

/* ---- Configuration custom handler prototype ---- */ 


/* ---- Operational command handler prototype and structure definition ---- */ 


/* --- Translation table array for configuration commands --- */

static  mfc_agentd_nd_trans_tbl_t stats_alarm_cmds_trans_tbl[] =  {

TRANSL_ENTRY(PREPEND_TRANS_STR"systemstatsalarm*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC ("/stats/config/alarm/%s/enable",
                TYPE_BOOL,
                ND_HARDCODED,
                0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemstatsalarm*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC ("/stats/config/alarm/%s/enable",
                TYPE_BOOL,
                ND_HARDCODED,
                0, "false")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemstatsalarm*risingerror-threshold*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC ("/stats/config/alarm/%s/rising/error_threshold",
		TYPE_UINT32,
		ND_NORMAL,
		1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemstatsalarm*risingerror-threshold*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC ("/stats/config/alarm/%s/rising/error_threshold",
		TYPE_UINT32,
		ND_HARDCODED,
		0, "0")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemstatsalarm*risingclear-threshold*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC ("/stats/config/alarm/%s/rising/clear_threshold",
                TYPE_UINT32,
                ND_NORMAL,
                1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemstatsalarm*risingclear-threshold*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC ("/stats/config/alarm/%s/rising/clear_threshold",
                TYPE_UINT32,
                ND_HARDCODED,
                0, "0")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemstatsalarm*fallingerror-threshold*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC ("/stats/config/alarm/%s/falling/error_threshold",
                TYPE_UINT32,
                ND_NORMAL,
                1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemstatsalarm*fallingerror-threshold*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC ("/stats/config/alarm/%s/falling/error_threshold",
                TYPE_UINT32,
                ND_HARDCODED,
                0, "0")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemstatsalarm*fallingclear-threshold*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC ("/stats/config/alarm/%s/falling/clear_threshold",
                TYPE_UINT32,
                ND_NORMAL,
                1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemstatsalarm*fallingclear-threshold*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC ("/stats/config/alarm/%s/falling/clear_threshold",
                TYPE_UINT32,
                ND_HARDCODED,
                0, "0")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY_NULL
};

/*---------------------------------------------------------------------------*/
int 
agentd_stats_alarm_cmds_init(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    err = agentd_register_cmds_array (stats_alarm_cmds_trans_tbl);
    bail_error (err);

bail:
    return err;
}

int
agentd_stats_alarm_cmds_deinit(
        const lc_dso_info *info,
        void *context)
{
    int err = 0;
    lc_log_debug (jnpr_log_level, "agentd_stats_alarm_cmds_deinit called");

bail:
    return err;
}

