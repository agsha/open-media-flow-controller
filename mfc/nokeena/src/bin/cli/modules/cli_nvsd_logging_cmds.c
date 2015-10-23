/*
 * Filename :   cli_nvsd_logging_cmds.c
 * Date     :   2008/12/07
 * Author   :   Dhruva Narasimhan
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 */

/*----------------------------------------------------------------------------
 * HEADER FILES
 *---------------------------------------------------------------------------*/
#include "common.h"
#include "climod_common.h"
#include "clish_module.h"
#include "proc_utils.h"
#include "tpaths.h"
#include "file_utils.h"
#include "nkn_defs.h"


/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
int 
cli_nvsd_logging_cmds_init(
        const lc_dso_info   *info,
        const cli_module_context *context);

enum {
    cid_nkn_log_show_default           = 1 << 0,
    cid_nkn_log_show_files             = 1 << 1,
    cid_nkn_log_invert_filter          = 1 << 2,
    cid_nkn_log_version_enable          = 1 << 3,
};

int 
cli_nvsd_logging_cmds_init(
        const lc_dso_info   *info,
        const cli_module_context *context)
{
    int err = 0;

    // Disabled as part of fix for Bug 968. 
    // Commands that are not signed off should not appear in the CLI
    //

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);

    return err;
}



