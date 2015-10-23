/*
 * Filename :   agentd_cli.c
 * Date:        26 July 2012
 * Author:      Lakshmi 
 *
 * (C) Copyright 2011, Juniper Networks, Inc.
 * All rights reserved.
 *
 */

#include "mdc_wrapper.h"
#include "cli_client.h"
#include "agentd_cli.h"

extern int jnpr_log_level;

extern md_client_context * agentd_mcc;

static int 
agentd_cli_mgmt_handler (void *data, gcl_session *session,
			bn_request *request, bn_response **ret_response);

///////////////////////////////////////////////////////////////////////////////

static int 
agentd_cli_mgmt_handler (void *data, gcl_session *session,
			bn_request *request, bn_response **ret_response) {
    int err = 0;
    bn_response *response = NULL;

    if (ret_response) {
        *ret_response = NULL;
    }

    err = mdc_send_mgmt_msg_raw(agentd_mcc, request, &response);
    bail_error (err);

    if (ret_response) {
        err = bn_response_msg_dup(response, ret_response);
        bail_error (err);
    }
bail:
    return (err);
}

int agentd_execute_cli_commands (const tstr_array *commands, tstring **ret_errors, tstring **ret_output,
                                        tbool *ret_success) {
    int err = 0;
    gcl_session * sess = NULL;
    tstring *total_output = NULL, *one_output = NULL;
    uint32 num_cmds = 0, i = 0;
    const char * cmd = NULL;
    tstring *total_errors = NULL, *one_error = NULL;
    tbool success = true, one_success = false;
    tbool line_complete = true;
    int last_char = 0;

    err = mdc_get_gcl_session (agentd_mcc, &sess);
    bail_error (err);

    err = cli_init_outer ();
    bail_error (err);

    err = cli_init_inner (NULL, NULL, agentd_cli_mgmt_handler, NULL, sess);
    bail_error(err);
   
    err = ts_new (&total_output);
    bail_error (err);

    err = ts_new (&total_errors);
    bail_error (err);

    num_cmds = tstr_array_length_quick (commands);
    for (i = 0; i < num_cmds; ++i) {
        cmd = tstr_array_get_str_quick (commands, i );
        bail_null (cmd);
        lc_log_debug (jnpr_log_level, "Cmd to execute:%s\n", cmd);
        err = cli_execute_command_capture_ex (cmd, true, &one_error,
                                              &one_output, &line_complete, 
                                              NULL, &one_success);
        bail_error (err);

        if (one_output) {
            err = ts_append (total_output, one_output);
            bail_error (err);
            ts_free (&one_output);
        }
        if (one_error) {
            if (ts_num_chars (one_error) > 0){
                last_char = ts_get_char(one_error,
                                        ts_num_chars(one_error) - 1);
                if (last_char != '\n' && last_char != '\r') {
                    err = ts_append_char(one_error, '\n');
                    bail_error(err);
                }
                err = ts_append(total_errors, one_error);
                bail_error(err);
                ts_free(&one_error);
                success = false;
                break;
            } else {
                ts_free (&one_error);
            }
        }
        if (!one_success) {
            success = false;
            break;
        }
    }

bail:
    cli_deinit_inner();
    cli_deinit_outer();
    if (ret_output) {
        *ret_output = total_output;
    } else {
        ts_free (&total_output);
    }
    if (ret_errors) {
        *ret_errors = total_errors;
    } else {
        ts_free (&total_errors);
    }
    if (ret_success) {
        *ret_success = success;
    }
    ts_free (&one_output);
    ts_free (&one_error);
    
    return err;
} 
