/*
 * Filename :   agentd_copied_code.c
 * Date:        30 Nov 2011
 * Author:
 *
 * (C) Copyright 2011, Juniper Networks, Inc.
 * All rights reserved.
 *
 */

/*
 TODO:Cleanup the unnecessary headers from inclusion
 */
#include "md_client.h"
#include "mdc_wrapper.h"
#include "bnode.h"
#include "signal_utils.h"
#include "libevent_wrapper.h"
#include "random.h"
#include "dso.h"
#include <ctype.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>
#include "agentd_mgmt.h"
#include "nkn_cfg_params.h"
#include <glib.h>

#include "common.h"
#include <climod_common.h>
#include <dso.h>
#include <env_utils.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "tstring.h"
#include "random.h"
#include "cli_module.h"
#include "clish_module.h"
#include "file_utils.h"
#include "proc_utils.h"
#include "libnkncli.h"
#include "nkn_defs.h"
#include <unistd.h>

#include "agentd_copied_code.h"
#include "agentd_op_cmds_base.h"

/*
 #define AGENTD_SET_ERROR
 #define cli_printf_error	AGENTD_SET_ERROR
 */

extern md_client_context * agentd_mcc;

#ifndef G_LOG
extern int jnpr_log_level;
#endif /*G_LOG*/

/* Removed samples total_cache_byte_count, total_disk_byte_count, total_origin_byte_count,Perportbytes, peroriginbytes, perdiskbytes.*/

const char* sample_clr_list[] =
       { "total_bytes", "num_of_connections", "cache_byte_count", "origin_byte_count",
        "disk_byte_count", "http_transaction_count",
        NULL };

int md_clear_nvsd_samples(const char* p_clusterName)
{
    int err = 0;
    int i = 0;

    lc_log_debug(jnpr_log_level, "restarging  nvsd %s", p_clusterName);

    while (sample_clr_list[i] != NULL)
    {
        err = mdc_send_action_with_bindings_str_va(agentd_mcc, NULL, NULL,
                "/stats/actions/clear/sample", 1, "id", bt_string,
                sample_clr_list[i]);
        bail_error(err);
        i++;
    }
    err
            = mdc_send_action_with_bindings_str_va(agentd_mcc, NULL, NULL,
                    "/pm/actions/restart_process", 1, "process_name",
                    bt_string, "nvsd");
    bail_error(err);

    err
            = mdc_send_action_with_bindings_str_va(agentd_mcc, NULL, NULL,
                    "/pm/actions/restart_process", 1, "process_name",
                    bt_string, "ssld");
    bail_error(err);

bail: return (err);
}

