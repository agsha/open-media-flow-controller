/*
 *
 * wsmd_client.h
 *
 *
 *
 */

#ifndef __WSMD_CLIENT_H_
#define __WSMD_CLIENT_H_


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

/* environment variable set to indicate the user was auto-logged out */
static const char wsmd_session_timed_out_env[] = "AUTO_LOGOUT";

/*
 * Environment variable set by the CGI launcher to the name of the user
 * as provided.  It is not set by Apache because as far as it is
 * concerned, all of our requests are unauthenticated.
 */
static const char wsmd_remote_user_env[] = "REMOTE_USER";

/*
 * Environment variable set by the CGI launcher to the mapped (local)
 * user name.  This will be different from REMOTE_USER only when using
 * mapped users via remote authentication like RADIUS or TACACS+ .
 */
static const char wsmd_remote_auth_user_env[] = "REMOTE_AUTH_USER";

/* session cookie name */
static const char wsmd_session_cookie[] = "session";

/* response codes */
enum {
    wsmd_code_success = 0,
    wsmd_code_auth_failed,
    wsmd_code_deauth_failed,
    wsmd_code_validate_failed,
    wsmd_code_session_timedout,
    wsmd_code_proxy_failed,
    wsmd_code_query_failed,
    wsmd_code_set_failed,
    wsmd_code_session_expired,
    wsmd_code_no_capability,
    wsmd_code_mgmt_unavailable,
};

/* action names */
static const char wsmd_auth_action[] =
    "/wsm/auth/login/basic";
static const char wsmd_deauth_action[] =
    "/wsm/auth/logout/basic";
static const char wsmd_validate_action[] =
    "/wsm/auth/validate/basic";
static const char wsmd_config_confirmed_get_action[] =
    "/wsm/flags/config_confirmed/get";
static const char wsmd_config_confirmed_set_action[] =
    "/wsm/flags/config_confirmed/set";

/* request/response payload binding names */
static const char wsmd_service_name[] = "service_name";
static const char wsmd_user_id[] = "user_id";
static const char wsmd_local_user_id[] =
    "local_user_id";
static const char wsmd_session_id[] = "session_id";
static const char wsmd_password[] = "password";
static const char wsmd_remote_addr[] = "remote_addr";
static const char wsmd_cookie_hdr[] = "set_cookie";
static const char wsmd_cookie[] = "cookie";
static const char wsmd_update_activity[] = "update_activity";
static const char wsmd_session_var_prefix[] =
    "/wsm/session/variable";
static const char wsmd_capability_prefix[] =
    "/wsm/session/capability";
static const char wsmd_acl_auth_prefix[] =
    "/wsm/session/acl/auth_groups";


#ifdef __cplusplus
}
#endif

#endif /* __WSMD_CLIENT_H_ */
