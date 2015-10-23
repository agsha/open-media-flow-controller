/*
 * Filename :   cli_nvsd_http_cmds.c
 * Date     :   2008/12/15
 * Author   :   Chitra Devi
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
#include "nkn_defs.h"

#include "../../mgmtd/modules/md_nkn_stats.h"


/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
int
cli_nvsd_stats_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int cli_nvsd_scheduler_stats_show_cmd(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);
int
cli_nvsd_threshold_show_cmd(void *data,
                   const cli_command *cmd,
                   const tstr_array *cmd_line,
                   const tstr_array *params);

enum {
    cid_nkn_show_all           = 1 << 0,
    cid_nkn_show_single             = 1 << 1,
};


/*----------------------------------------------------------------------------
 * MODULE ENTRY POINT
 *---------------------------------------------------------------------------*/
int
cli_nvsd_stats_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;

    UNUSED_ARGUMENT(info);

#ifdef PROD_FEATURE_I18N_SUPPORT
    err = cli_set_gettext_domain(GETTEXT_DOMAIN);
    bail_error(err);
#endif

    if (context->cmc_mgmt_avail == false) {
        goto bail;
    }

    err = cli_revmap_hide_bindings("/stat/nkn/stats/**");
    bail_error(err);
    
    err = cli_revmap_hide_bindings("/stat/nkn/graph/**");
    bail_error(err);

bail:
    return err;
}
/*!----------------------Callback function definitions---------------------*/
int
cli_nvsd_scheduler_stats_show_cmd(void *data, const cli_command *cmd,
                   const tstr_array *cmd_line,
                   const tstr_array *params)
{
    int err;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);
    UNUSED_ARGUMENT(cmd_line);
    UNUSED_ARGUMENT(params);

    err = cli_printf_query(_("Scheduler Deadline Misses       : "
                "#/stat/nkn/http/aborts/task_deadline_missed# \n"));
    bail_error(err);

    err = cli_printf_query(_("Scheduler Runnable Queue Pushes : "
                "#/stat/nkn/scheduler/runnable_q_pushes# \n"));
    bail_error(err);

    err = cli_printf_query(_("Scheduler Runnable Queue Pops   : "
                "#/stat/nkn/scheduler/runnable_q_pops# \n"));
    bail_error(err);

    err = cli_printf_query(_("Scheduler Cleanup Queue Pushes  : "
                "#/stat/nkn/scheduler/cleanup_q_pushes# \n"));
    bail_error(err);

    err = cli_printf_query(_("Scheduler Cleanup Queue Pops    : "
                "#/stat/nkn/scheduler/cleanup_q_pops# \n"));
    bail_error(err);

bail:
        return(err);
}

int
cli_nvsd_threshold_show_cmd(void *data,
                   const cli_command *cmd,
                   const tstr_array *cmd_line,
                   const tstr_array *params)
{

    int err = 0;
    const char *countername = NULL;
    tstr_array *countername_array = NULL;
    int num_names = 0 , i = 0;
    uint32 code = 0;
    bn_binding *binding = NULL;
    char *node_pattern = NULL;
    char *node_pattern_esc = NULL;
    char *trigger_type = NULL;
    char *trigger_id = NULL;
    char *outputnode = NULL;
    char *label = NULL;
    char *unit = NULL;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd_line);

    /*! For loop to iterate through all the available namespace */
    if (cmd->cc_magic & cid_nkn_show_all)
    {
        err = mdc_get_binding_children_tstr_array(cli_mcc,
            NULL, NULL, &countername_array,
            "/stats/config/alarm", NULL);
        bail_error_null(err, countername_array);
    }
    else if (cmd->cc_magic & cid_nkn_show_single)
    {
        countername = tstr_array_get_str_quick(params, 0);
        err = tstr_array_new(&countername_array, NULL);
        bail_error(err);
        err = tstr_array_append_str(countername_array,countername);
        bail_error(err);
    }
    else
    {
        goto bail;
    }
    num_names = tstr_array_length_quick(countername_array);
    for(i = 0; i < num_names; ++i) {
        countername = tstr_array_get_str_quick(countername_array, i);
        bail_null(countername);

        /*! TODO : Dynamically create output node names */
        err = mdc_get_binding_fmt
		(cli_mcc,&code,NULL,false,&binding,NULL,
		"/stats/config/alarm/%s/trigger/type",countername );
        bail_error(err);
        if (binding) {
            err = bn_binding_get_str(binding,ba_value,bt_string,NULL,&trigger_type);
            bail_error(err);
            bn_binding_free(&binding);
        }
        /*! Get Label and Unit Info */
        err = mdc_get_binding_fmt
               (cli_mcc,&code,NULL,false,&binding,NULL,
		"/stat/nkn/stats/%s/label",countername);
        bail_error(err);
        if (binding) {
            err = bn_binding_get_str(binding,ba_value,bt_string,NULL,&label);
            bail_error(err);
            bn_binding_free(&binding);
        }
        else
        {
            label = smprintf("%s",countername);
        }

        err = mdc_get_binding_fmt
               (cli_mcc,&code,NULL,false,&binding,NULL,
		"/stat/nkn/stats/%s/unit",countername);
        bail_error(err);
        if (binding) {
            err = bn_binding_get_str(binding,ba_value,bt_string,NULL,&unit);
            bail_error(err);
            bn_binding_free(&binding);
        }
        else
        {
            unit = smprintf("%s"," ");
        }
   

        err = mdc_get_binding_fmt
		(cli_mcc,&code,NULL,false,&binding,NULL,
		"/stats/config/alarm/%s/trigger/id",countername );
        bail_error(err);

        if (binding) {
            err = bn_binding_get_str(binding,ba_value,bt_string,NULL,&trigger_id);
            bail_error(err);
            bn_binding_free(&binding);
        }
        err = mdc_get_binding_fmt
		(cli_mcc,&code,NULL,false,&binding,NULL,
		"/stats/config/alarm/%s/trigger/node_pattern",countername );
        bail_error(err);

        if (binding) {
            err = bn_binding_get_str(binding,ba_value,bt_name,NULL,&node_pattern);
            bail_error(err);
            bn_binding_free(&binding);
        }
        err = bn_binding_name_escape_str(node_pattern,&node_pattern_esc);
        bail_error_null(err,node_pattern_esc);

        outputnode = smprintf("/stats/state/%s/%s/node/%s/last/value",trigger_type,trigger_id,node_pattern_esc);
        err = cli_printf(_("%s %s :\n"),label,unit);
        bail_error(err);
       
        err = cli_printf_query(_("\tCurrent Value   :"
		"#%s#\n"),outputnode);
        bail_error(err);
        err = cli_printf_query(_("\tError Threshold :"
		"#/stats/config/alarm/%s/rising/error_threshold#\n"),countername);
        bail_error(err);
        err = cli_printf_query(_("\tClear Threshold :"
		"#/stats/config/alarm/%s/rising/clear_threshold#\n"),countername);
        bail_error(err);
    }
bail:
    tstr_array_free(&countername_array);
    safe_free(node_pattern_esc);
    safe_free(trigger_id);
    safe_free(trigger_type);
    safe_free(outputnode);
    safe_free(label);
    safe_free(unit);
    return (err);
}

