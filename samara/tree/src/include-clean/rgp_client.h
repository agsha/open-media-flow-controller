/*
 *
 * rgp_client.h
 *
 *
 *
 */

#ifndef __RGP_CLIENT_H_
#define __RGP_CLIENT_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "common.h"

/*-----------------------------------------------------------------------------
 * CONSTANTS
 */

/*
 * Response codes.
 *
 * NOTE: these numbers should be left stable, since they are passed from
 * CMC client to server, which sometimes are not both running the same
 * Samara version.
 */
typedef enum {
    rgp_code_success =               0,
    rgp_code_session_expired =       1,
    rgp_code_proxy_failed =          2,
    rgp_code_query_failed =          3,
    rgp_code_set_failed =            4,
    rgp_code_no_capability =         5,
    rgp_code_cli_exec_failure =      6,
    rgp_code_cli_exec_cancelled =    7,
} rgp_response_code;

static const lc_enum_string_map rgp_response_code_map[] = {
    {rgp_code_success, "success"},
    {rgp_code_session_expired, "session expired"},
    {rgp_code_proxy_failed, "proxy failed"},
    {rgp_code_query_failed, "query failed"},
    {rgp_code_set_failed, "set failed"},
    {rgp_code_no_capability, "insufficient privileges"},
    {rgp_code_cli_exec_failure, "CLI command execution failure"},
    {rgp_code_cli_exec_cancelled, "CLI command execution cancelled"},
    {0, NULL}
};

/*
 * XXX/EMT: the strings below are just #defines because this header
 * file doesn't go with any library that the strings could live in as
 * extern string variables.
 */

/* action names */
#define rgp_cli_exec_action       "/rgp/actions/cli/exec"
#define rgp_cli_cancel_action     "/rgp/actions/cli/cancel"
#define rgp_identify_self_action  "/rgp/actions/identify_self"
#define rgp_goodbye_action        "/rgp/actions/goodbye"
#define rgp_get_info_action       "/rgp/actions/get_info"

/* request/response payload binding names */
#define rgp_action_cli_exec_param_command_id                "command"
#define rgp_action_cli_exec_param_failure_cont_id           "failure_cont"
#define rgp_action_cli_exec_response_param_output_id        "exec_output"
#define rgp_action_cli_exec_response_param_error_string_id  "error_output"

#define rgp_session_var_prefix  "/rgp/session/variable"
#define rgp_capability_prefix   "/rgp/session/capability"

/**
 * Reasons why an established CMC connection between a server and a
 * client (initiated by either side) could be broken.  This enum and
 * its associated maps are used on both the client and the server
 * to report reasons for connection breakage.
 *
 * NOTE: if you add any reasons here, make sure to update (a) the 
 * macros below that distinguish between client and server breaking
 * the connection; and (b) the refs to the possible reasons in nodes.txt.
 */
typedef enum {
    cdr_none = 0,
    cdr_no_connection_yet,
    cdr_failure,
    cdr_server_appl_disabled, 
    cdr_server_config_change,
    cdr_server_requested,
    cdr_server_client_validation,
    cdr_server_timeout,
    cdr_server_shutdown,
    cdr_server_unknown,
    cdr_client_disabled, 
    cdr_client_config_change,
    cdr_client_requested,
    cdr_client_server_validation,
    cdr_client_timeout,
    cdr_client_shutdown,
    cdr_client_unknown,
    cdr_client_already_managed,
} cmc_disconnect_reason;

/**
 * Is a specified reason for disconnection one that was caused by the
 * server?  Using this is better than explicitly testing all of the
 * possibilities yourself, since other reasons may be added to this
 * list, and the macro will be kept in sync.
 */
#define cmc_disconnect_reason_is_server(r)                                    \
    ((r) == cdr_server_appl_disabled || (r) == cdr_server_config_change ||    \
     (r) == cdr_server_requested || (r) == cdr_server_client_validation ||    \
     (r) == cdr_server_timeout || (r) == cdr_server_shutdown ||               \
     (r) == cdr_server_unknown)

/**
 * Is a specified reason for disconnection one that was caused by the
 * client?
 */
#define cmc_disconnect_reason_is_client(r)                                    \
    ((r) == cdr_client_disabled || (r) == cdr_client_config_change ||         \
     (r) == cdr_client_requested || (r) == cdr_client_server_validation ||    \
     (r) == cdr_client_timeout || (r) == cdr_client_shutdown ||               \
     (r) == cdr_client_unknown || (r) == cdr_client_already_managed)

static const lc_enum_string_map cmc_disconnect_reason_map[] = {
    {cdr_none, "none"},
    {cdr_no_connection_yet, "no_connection_yet"},
    {cdr_failure, "failure"},
    {cdr_server_appl_disabled, "server_appl_disabled"},
    {cdr_server_config_change, "server_config_change"},
    {cdr_server_requested, "server_requested"},
    {cdr_server_client_validation, "server_client_validation"},
    {cdr_server_timeout, "server_timeout"},
    {cdr_server_shutdown, "server_shutdown"},
    {cdr_server_unknown, "server_unknown"},
    {cdr_client_disabled, "client_disabled"},
    {cdr_client_config_change, "client_config_change"},
    {cdr_client_requested, "client_requested"},
    {cdr_client_server_validation, "client_server_validation"},
    {cdr_client_timeout, "client_timeout"},
    {cdr_client_shutdown, "client_shutdown"},
    {cdr_client_unknown, "client_unknown"},
    {cdr_client_already_managed, "client_already_managed"},
    {0, NULL}
};

/*
 * XXX/EMT: I18N
 */
static const lc_enum_string_map cmc_disconnect_reason_friendly_map[] = {
    {cdr_none, "None"},
    {cdr_no_connection_yet, "Have not connected yet"},
    {cdr_failure, "Unknown failure"},
    {cdr_server_appl_disabled, "Broken by server: appliance disabled"},
    {cdr_server_config_change, 
     "Broken by server: appliance config changed"},
    {cdr_server_requested, "Broken by server: per explicit request"},
    {cdr_server_client_validation,
     "Broken by server: client validation failed"},
    {cdr_server_timeout, "Broken by server: timeout on appliance"},
    {cdr_server_shutdown, "Broken by server: system shutdown"},
    {cdr_server_unknown, "Broken by server: unknown reason"},
    {cdr_client_disabled, "Broken by client: CMC client feature disabled"},
    {cdr_client_config_change, "Broken by client: configuration changed"},
    {cdr_client_requested, "Broken by client: per explicit request"},
    {cdr_client_server_validation,
     "Broken by client: server validation failed"},
    {cdr_client_timeout, "Broken by client: timeout on server"},
    {cdr_client_shutdown, "Broken by client: system shutdown"},
    {cdr_client_unknown, "Broken by client: unknown reason"},
    {cdr_client_already_managed, "Broken by client: already under CMC "
    "management"},
    {0, NULL}
};

#ifdef __cplusplus
}
#endif

#endif /* __RGP_CLIENT_H_ */
