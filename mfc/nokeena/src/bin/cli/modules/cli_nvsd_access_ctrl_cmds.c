/*
 * Filename :   cli_nvsd_access_ctrl_cmds.c
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
#include "nkn_defs.h"


/*----------------------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *---------------------------------------------------------------------------*/
int
cli_nvsd_access_ctrl_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

enum {
    cli_acl_nw_deny_add = 1,
    cli_acl_nw_permit_add,
    cli_acl_nw_deny_delete,
    cli_acl_nw_permit_delete,
};


int
cli_nvsd_acl_mask_completion(
    void *data,
    const cli_command *cmd,
    const tstr_array *cmd_line,
    const tstr_array *params,
    const tstring *curr_word,
    tstr_array *ret_completions);

int
cli_nvsd_access_ctrl_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
    int err = 0;

    UNUSED_ARGUMENT(info);
    UNUSED_ARGUMENT(context);
#if 0
    CLI_CMD_NEW;
    cmd->cc_words_str =         "network access-control";
    cmd->cc_help_descr =        N_("Configure access control parameters");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "network access-control permit";
    cmd->cc_help_descr =        N_("Permit hosts to connect");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network access-control permit *";
    cmd->cc_help_exp =          N_("<ip address>");
    cmd->cc_help_exp_hint =     N_("Source IP Address");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network access-control permit * *";
    cmd->cc_help_exp =          N_("<mask>");
    cmd->cc_help_exp_hint =     N_("Subnet mask");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_priv_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_magic =             cli_acl_nw_permit_add;
    cmd->cc_exec_callback =     cli_nvsd_access_ctrl_config;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network access-control deny";
    cmd->cc_help_descr =        N_("Deny hosts to connect");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "network access-control deny *";
    cmd->cc_help_exp =          N_("<ip address>");
    cmd->cc_help_exp_hint =     N_("Source IP Address");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "network access-control deny * *";
    cmd->cc_help_exp =          N_("<mask>");
    cmd->cc_help_exp_hint =     N_("Subnet mask");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_priv_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_magic =             cli_acl_nw_deny_add;
    cmd->cc_exec_callback =     cli_nvsd_access_ctrl_config;
    CLI_CMD_REGISTER;

    /*-------------------------------------------------*/

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no network access-control";
    cmd->cc_help_descr =        N_("Clear certain access control configurations");
    CLI_CMD_REGISTER;



    /*-------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =         "no network access-control permit";
    cmd->cc_help_descr =        N_("Clear access control permit configurations");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no network access-control permit *";
    cmd->cc_help_exp =          N_("<ip address>");
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_type =         cct_matching_values;
    cmd->cc_comp_pattern =      "/nkn/nvsd/network/config/access_control/permit/*/address";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no network access-control permit * *";
    cmd->cc_help_exp =          N_("<mask>");
    cmd->cc_help_exp_hint =     N_("Subnet mask");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_callback =     cli_nvsd_acl_mask_completion;
    cmd->cc_capab_required =    ccp_set_priv_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_magic =             cli_acl_nw_permit_delete;
    cmd->cc_exec_callback =     cli_nvsd_access_ctrl_config;
    CLI_CMD_REGISTER;


    /*-------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =         "no network access-control deny";
    cmd->cc_help_descr =        N_("Clear access control deny configurations");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no network access-control deny *";
    cmd->cc_help_exp =          N_("<ip address>");
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_type =         cct_matching_values;
    cmd->cc_comp_pattern =      "/nkn/nvsd/network/config/access_control/deny/*/address";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no network access-control deny * *";
    cmd->cc_help_exp =          N_("<mask>");
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_callback =     cli_nvsd_acl_mask_completion;
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_priv_curr;
    cmd->cc_magic =             cli_acl_nw_deny_delete;
    cmd->cc_exec_callback =     cli_nvsd_access_ctrl_config;
    CLI_CMD_REGISTER;
#endif

    return err;
}


int
cli_nvsd_acl_mask_completion(
    void *data,
    const cli_command *cmd,
    const tstr_array *cmd_line,
    const tstr_array *params,
    const tstring *curr_word,
    tstr_array *ret_completions)
{
    int err = 0;
    bn_binding_array *barr = NULL;
    bn_binding *binding = NULL;
    uint32_array *idx_array = NULL;
    uint32 idx = 0, num_idx = 0;
    char *bname = NULL;
    tstring *mask = NULL;
    tstr_array *masks = NULL;
    tstr_array_options opts;

    UNUSED_ARGUMENT(data);
    UNUSED_ARGUMENT(cmd);

    err = tstr_array_options_get_defaults(&opts);
    bail_error(err);

    opts.tao_dup_policy = adp_delete_old;

    err = tstr_array_new(&masks, &opts);
    bail_error(err);

    bname = smprintf("/nkn/nvsd/network/config/access_control/%s", 
            tstr_array_get_str_quick(cmd_line, 3));
    bail_null(bname);

    err = bn_binding_array_new(&barr);
    bail_error(err);

    err = bn_binding_new_str_autoinval(
            &binding, "address", ba_value, bt_ipv4addr, 0,
            tstr_array_get_str_quick(params, 0));
    bail_error_null(err, binding);

    err = bn_binding_array_append_takeover(barr, &binding);
    bail_error(err);


    /* Get all indices under the root node. */
    err = mdc_array_get_matching_indices(cli_mcc,
            bname, NULL, barr, 0, NULL, 
            &idx_array, NULL, NULL);
    bail_error_null(err, idx_array);

    num_idx = uint32_array_length_quick(idx_array);
    for(idx = 0; idx < num_idx; idx++) {
        /* Filter out all addresses that we dont need.
         * And get only the masks for the relevant ones
         */
        err = mdc_get_binding_tstr_fmt(cli_mcc,
                NULL, NULL, NULL, &mask, NULL, 
                "%s/%d/mask", bname, uint32_array_get_quick(idx_array, idx));
        bail_error(err);

        err = tstr_array_append_str(masks, ts_str(mask));
        bail_error(err);
    }

    err = tstr_array_concat(ret_completions, &masks);
    bail_error(err);

    err = tstr_array_delete_non_prefixed(ret_completions, ts_str(curr_word));
    bail_error(err);


bail:
    safe_free(bname);
    tstr_array_free(&masks);
    bn_binding_free(&binding);
    bn_binding_array_free(&barr);
    return err;

}
