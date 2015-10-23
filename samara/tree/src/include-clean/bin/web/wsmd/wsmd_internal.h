/*
 *
 * src/bin/web/wsmd/wsmd_internal.h
 *
 *
 *
 */

#ifndef __WSMD_INTERNAL_H_
#define __WSMD_INTERNAL_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "wsmd_client.h"
#include "common.h"
#include "customer.h"
#include "gcl.h"
#define LIBEVENT_USE_INTERNAL_API
#include "event.h"
#include "event_fdintrcb.h"
#include "bnode.h"
#include "bnode_proto.h"
#include "bnode_types.h"
#include "hash_table.h"

/*-----------------------------------------------------------------------------
 * CONSTANTS
 */

#ifndef WEB_MAX_REMOTE_LOGINS_NONADMIN
#define WEB_MAX_REMOTE_LOGINS_NONADMIN 200
#endif
#ifndef WEB_MAX_REMOTE_LOGINS_ADMIN
#define WEB_MAX_REMOTE_LOGINS_ADMIN 225
#endif

static const int wsmd_max_remote_logins_nonadmin = 
    WEB_MAX_REMOTE_LOGINS_NONADMIN;
static const int wsmd_max_remote_logins_admin = 
    WEB_MAX_REMOTE_LOGINS_ADMIN;

static const int wsmd_session_key_length = 32;

static const char wsmd_pam_service_name_default[] = "wsmd";
static const char wsmd_pam_service_name_prefix[] = "wsmd-";

typedef struct wsmd_session_key {
    uint8 *wsk_key;
    lt_dur_ms wsk_created_utms;
    lt_dur_ms wsk_expiration_utms;
} wsmd_session_key;

TYPED_ARRAY_HEADER_NEW_NONE(wsmd_session_key, struct wsmd_session_key *);


/*-----------------------------------------------------------------------------
 * STRUCTURES
 */

typedef struct wsmd_session {
    tbool ws_active;                       /* is the session alive? */
    tbool ws_config_confirmed;             /* has user confimed sets OK? */
    tbool ws_closed;                       /* if we've already closed session */

    char *ws_user_id;                      /* user id of the session */
    char *ws_local_user_id;                /* local user id of the session */
    char *ws_trusted_auth_info;            /* TRUSTED_AUTH_INFO from PAM */
    char *ws_remote_addr;                  /* remote address logged in from */
    char *ws_service_name;                 /* PAM service name used for auth */
    int32 ws_uid;                          /* uid of this session */
    int32 ws_gid;                          /* gid of this session */

    uint16 ws_session_id;                  /* session id - for convenience */
    uint8 *ws_session_key;                 /* session key */

    /*
     * These four values represent:
     *   - When the session key was originally created (may be later than
     *     when session was created, if key was renewed)
     *   - When the session key will expire
     *   - When the session was originally created
     *   - When there was last activity on the session
     *
     * They are all represented as a number of milliseconds of system
     * uptime (since system boot).
     */
    lt_dur_ms ws_session_key_created_utms;
    lt_dur_ms ws_session_key_expiration_utms;
    lt_dur_ms ws_login_time_utms;
    lt_dur_ms ws_last_activity_utms;

    /*
     * The ws_session_key, ws_session_key_created_utms, and
     * ws_session_key_expiration_utms fields above represent the
     * latest session key, and its expiration time is the expiration
     * time of the whole session.  But here we separately keep track
     * of old session keys, and their expirations and creation times.
     *
     * When we give out a new cookie with a freshly-created session
     * key, it should remain good for the full session timeout period,
     * even after it is renewed.  This is partly for convenience of
     * scripts (so they don't have to deal with being given new
     * cookies), and partly for cases like bug 13342, where not all of
     * our code deals with being given new cookies in the middle of a 
     * request (since usually one gets a new cookie at the beginning,
     * if at all).
     *
     * We also have to keep track of the session key's creation time
     * to make sure we can effectively purge the oldest session key if
     * we hit our maximum number of old keys.  If a session expiration
     * time is in effect, we could always have purged the one that
     * expired soonest, but if there is no expiration time, they'll
     * all remain good forever, and we need this separate field in
     * order to know which to delete first.
     */
    wsmd_session_key_array *ws_old_session_keys;

    int32 ws_msgs_outstanding;             /* num of proxied mgmtd messages */
                                           /* we're waiting on              */

    ht_table *ws_session_data;             /* session data */

#ifdef PROD_FEATURE_CAPABS
    tbool ws_caps_set;                     /* if user capabilities set yet */
    tstr_array *ws_caps;                   /* user capabilities */
#endif
#ifdef PROD_FEATURE_ACLS
    uint32_array *ws_acl_auth_groups;      /* user ACL auth groups */
#endif

    /* the web session's gcl data */
    
    gcl_handle *ws_gclh;                   /* gcl handle for this session */
    gcl_session *ws_gcl_session;           /* gcl session */
    fdic_libevent_context *ws_fdic_ctx;    /* fdic libevent context */
    fdic_handle *ws_fdich;                 /* fdic handle */
} wsmd_session;

/**
 * Prototype for callback function to be used in the register function
 * below when waiting for a request response.

 * \return 0 on success, error code on failure.
 */
typedef int (*wsmd_outer_request_callback_func)(const bn_response *response,
                                            wsmd_session *session,
                                            const bn_request *sent_request,
                                            gcl_session *orig_gcl_session,
                                            bn_msg_type orig_msg_type,
                                            uint32 orig_msg_id,
                                            uint32 orig_transaction_id,
                                            bap_service_id orig_service_id);

typedef struct wsmd_outer_request {
    wsmd_outer_request_callback_func wor_callback_func; /* callback func */
    uint16 wor_ws_session_id;              /* wsmd session id */
    bn_request *wor_sent_request;          /* dup of request we sent */

    int_ptr wor_orig_gs_id;                /* original gcl session id */
    bn_msg_type wor_orig_msg_type;         /* If !bbmt_none, next fields ok */
    uint32 wor_orig_request_msg_id;        /* Fields from original request */
    uint32 wor_orig_transaction_id;
    bap_service_id wor_orig_service_id;
} wsmd_outer_request;

/*-----------------------------------------------------------------------------
 * GLOBALS
 */

extern lt_dur_sec wsmd_session_renewal_threshold;
extern lt_dur_sec wsmd_session_timeout;
extern lt_dur_sec wsmd_inactivity_timeout;
extern tstr_array *wsmd_outer_peer_names;
extern tbool wsmd_httpd_conf_http_enabled;
extern tbool wsmd_httpd_conf_https_enabled;
extern tbool wsmd_httpd_conf_redirect_to_https_enabled;
extern tbool wsmd_httpd_sessions_secure_only;


/*-----------------------------------------------------------------------------
 * SESSION
 */

/**
 * Initialize the session system.
 * \return 0 on success, error code on failure.
 */
int wsmd_init_sessions(void);

/**
 * Deinitialize the session system.
 * \return 0 on success, error code on failure.
 */
void wsmd_deinit_sessions(void);

/**
 * Generate a session cookie for the given session id.
 * ret_session_cookie is NULL on error.
 * \param session the session.
 * \param ret_new_cookie the new cookie.
 * \param ret_cookie_hdr the resulting session cookie.
 * \return 0 on success, error code on failure.
 */
int wsmd_generate_auth_cookie_hdr(const wsmd_session *session,
                                  char **ret_new_cookie,
                                  char **ret_cookie_hdr);

/**
 * Generate a logout cookie for the given session id.
 * ret_session_cookie is NULL on error.
 * \param session the session.
 * \param ret_session_cookie the resulting session cookie.
 * \return 0 on success, error code on failure.
 */
int wsmd_generate_deauth_cookie_hdr(const wsmd_session *session,
                                    char **ret_cookie_hdr);

/*
 * Check an array of new_bindings extracted from a dbchange event, to see
 * whether we can safely send the cookie "Secure" attribute.  This attribute
 * tells the browser it can only use our authentication cookie on a secure
 * channel (https), but this will break unencrypted http users, so only set
 * this flag if http has been disabled.
 */
int wsmd_recheck_httpd_secure_only(const bn_binding_array *bindings);

/**
 * Validate the given cookie and if good, return a pointer to the
 * session.
 * ret_session is NULL on error.
 * The returned session should never be freed.
 * \param cookie the cookie.
 * \param update_activity indicates whether to update last activity time
 * \param ret_session the session or NULL if not valid.
 * \param ret_new_cookie the new cookie.
 * \param ret_cookie_hdr contains cookie new header if
 * session was going to expire.
 * \return 0 on success, error code on failure.
 */
int wsmd_validate_session_cookie(const char *cookie,
                                 tbool update_activity,
                                 wsmd_session **ret_session,
                                 char **ret_new_cookie,
                                 char **ret_cookie_hdr);

/**
 * Insert a new session record into the session list.
 * The returned session should never be freed.
 * \param service_name the PAM service name used for authentication.
 * \param user_id the user id.
 * \param remote_addr the remote address logged in from
 * \param local_user_id the local user id.
 * \param trusted_auth_info the TRUSTED_AUTH_INFO string from PAM
 * \param ret_session_id the new session id or NULL if not needed.
 * \param ret_session the new session or NULL if not needed.
 * \return 0 on success, error code on failure.
 */
int wsmd_register_session(const char *service_name,
                          const char *user_id, const char *local_user_id,
                          const char *trusted_auth_info,
                          const char *remote_addr,
                          uint16 *ret_session_id, wsmd_session **ret_session);

/**
 * Find the session record for the given session id.
 * The returned session SHOULD NOT be freed.
 * \param session_id the session id.
 * \param ret_session the session or NULL if not found.
 * \return 0 on success, error code on failure.
 */
int wsmd_get_session(uint16 session_id, wsmd_session **ret_session);

/**
 * Get the list of all web session ids.
 * The returned array MUST be freed by the caller.
 * \param ret_sid_list uint16 array of all web session ids.
 * \return 0 on success, error code on failure.
 */
int wsmd_get_all_session_ids(uint16_array **ret_sid_list);

/**
 * Return the number of sessions currently in our tables.  There may be
 * some inactive sessions in this count.
 */
int32 wsmd_session_get_count(void);

/**
 * Free a single session.
 * \param session_id the session to free.
 * \return 0 on success, error code on failure.
 */
int wsmd_unregister_session(uint16 session_id);

/**
 * Free all sessions.
 * \return 0 on success, error code on failure.
 */
int wsmd_unregister_all_sessions(void);

/**
 * Check all sessions to see if any need to be timed out.
 * \return 0 on success, error code on failure.
 */
int wsmd_check_sessions(void);

typedef int (*wsmd_iterate_sessions_func)(wsmd_session *sess, void *data);

/**
 * Call a specified callback for every session.
 */
int wsmd_iterate_sessions(wsmd_iterate_sessions_func callback, void *data);


/*-----------------------------------------------------------------------------
 * INNER
 */

/**
 * Callback handler for the md_client library to send messages.
 * \param data callback data.
 * \param session the session used to construct the message.
 * \param request the request.
 * \param ret_response the response.
 * \return 0 on success, error code on failure.
 */
int wsmd_mdc_msg_handler(void *data, gcl_session *session,
                         bn_request *request, bn_response **ret_response);

int wsmd_handle_inner_request(gcl_session *gcls, 
                          bn_request **inout_request, void *arg);
/**
 * Callback handler for the inner connection responses between the
 * session manager and the management daemon.
 * \param gcls the GCL session.
 * \param resp the response.
 * \param arg callback argument.
 * \return 0 on success, error code on failure.
 */
int wsmd_handle_inner_response(gcl_session *gcls, 
                               bn_response **inout_resp,
                               void *arg);

/**
 * Callback handler for the inner connection event request between the
 * session manager and the management daemon.
 * \param gcls the GCL session.
 * \param request the request.
 * \param arg callback argument.
 * \return 0 on success, error code on failure.
 */
int wsmd_handle_inner_event_request(gcl_session *gcls, 
                                    bn_request **inout_request,
                                    void *arg);

/**
 * Callback handler for proxy connection responses between the
 * session manager and the management daemon.
 * \param gcls the GCL session.
 * \param resp the response.
 * \param arg callback argument.
 * \return 0 on success, error code on failure.
 */
int wsmd_handle_proxy_response(gcl_session *gcls, 
                               bn_response **inout_resp,
                               void *arg);

/**
 * Load the wsmd settings from the mgmtd.
 * \return 0 on success, error code on failure.
 */
int wsmd_load_wsmd_settings(void);

/**
 * Load the wsmd settings from the given binding array.
 * \param bindings the binding array.
 * \return 0 on success, error code on failure.
 */
int wsmd_load_wsmd_settings_from_bindings(const bn_binding_array *bindings);

/*-----------------------------------------------------------------------------
 * OUTER
 */

/**
 * Initialize "outer"-related data structures: GCL request lookup table,
 * and array of current peer names.

 * \return 0 on success, error code on failure.
 */
int wsmd_outer_init(void);

/**
 * De-initialize "outer"-related data structures.
 * \return 0 on success, error code on failure.
 */
int wsmd_outer_deinit(void);

/**
 * Register a particular request and call the callback when the
 * message arrives.
 * \param session the web session.
 * \param inout_sent_request the binding request we sent.
 * \param orig_gcl_session the original gcl session.
 * \param orig_request the orignal gcl request
 * \param param callback parameter.
 * \return 0 on success, error code on failure.
 */
int wsmd_outer_request_register(wsmd_session *session,
                                bn_request **inout_sent_request,
                                gcl_session *orig_gcl_session,
                                const bn_request *orig_request,
                                wsmd_outer_request_callback_func func);

/**
 * Lookup registered request data.
 * \param ws_id the web session id the response was sent on.
 * \param gs_id the gcl session id the response was sent on.
 * \param req_id the req id to look up on.  This must have been sent on
 *               the session specified.
 * \param ret_found whether or not the entry was found.
 * \param ret_session pointer to web session or NULL if not found.
 * \param ret_sent_request pointer to the request we sent on the outer
 *                         session
 * \param ret_orig_gcl_session pointer to the original gcl session the
 *                             request was from, or NULL if not found.
 * \param ret_had_orig_msg whether or not the next three returns are valid
 * \param ret_orig_transaction_id transaction id of the original request
 * \param ret_orig_service_id service id of the original request
 * \param ret_orig_request_msg_id message id of the original request
 * \param ret_func the callback function pointer.
 * \return 0 on success, error code on failure.
 */
int wsmd_outer_request_lookup(uint_ptr ws_id,
                              int_ptr gs_id,
                              uint32 req_id,
                              tbool *ret_found,
                              wsmd_session **ret_session,
                              bn_request **ret_sent_request,
                              gcl_session **ret_orig_gcl_session,

                              bn_msg_type *ret_orig_msg_type,
                              uint32 *ret_orig_request_msg_id,
                              uint32 *ret_orig_transaction_id,
                              bap_service_id *ret_orig_service_id,

                              wsmd_outer_request_callback_func *ret_func);

/**
 * Callback handler for the outer connection requests between CGIs
 * and the session manager.
 * \param gcls the GCL session.
 * \param request the incoming request.
 * \param arg callback argument.
 * \return 0 on success, error code on failure.
 */
int wsmd_handle_outer_msg(gcl_session *gcls, 
                          bn_request **inout_request,
                          void *arg);

/**
 * Callback handler for when an outer connection goes down.
 * \param sess the session that was closed.
 * \param arg callback argument.
 * \return 0 on success, error code on failure.
 */
int wsmd_outer_session_handle_close(int fd, fdic_callback_type cbt, 
                                    void *vsess, void *arg);

int wsmd_send_login_action(wsmd_session *session);


/*-----------------------------------------------------------------------------
 * AUTH
 */

/**
 * Check authentication.
 * \param auth_service the PAM service name.  Pass NULL for default (wsmd).
 * \param user_id the user id.
 * \param password the password.
 * \param remote_addr the ip address the user is logging in from
 * \param ret_local_user_id the local user id after auth.
 * \param ret_trusted_auth_info The contents of the TRUSTED_AUTH_INFO string
 * returned by PAM.
 * \param ret_success true if success, false if failed.
 * \param ret_msg contains failure message if ret_success = false.
 * \return 0 on success, error code on failure.
 */
int wsmd_authenticate_user(const char *auth_service,
                           const char *user_id, const char *password,
                           const char *remote_addr,
                           char **ret_local_user_id,
                           char **ret_trusted_auth_info,
                           tbool *ret_success,
                           char **ret_msg);

/**
 * Deauthenticate all sessions for a specified user ID.
 */
int wsmd_deauth_user(const char *username);


/*-----------------------------------------------------------------------------
 * CAPABILITIES
 */

#ifdef PROD_FEATURE_CAPABS
int wsmd_session_get_capabilities(wsmd_session *session);

int wsmd_get_capabilities(wsmd_session *session, uint32 node_id, 
                          bn_response *resp);

int wsmd_check_login_capabilites(wsmd_session *session, tbool *ret_login_okay,
                                 tstring **ret_error_msg);
#endif

/*-----------------------------------------------------------------------------
 * ACLS
 */

#ifdef PROD_FEATURE_ACLS
int wsmd_session_get_acl_auth(wsmd_session *session);

/*
 * Check an array of new_bindings extracted from a dbchange event, to see
 * if any users have gained or lost ACL roles.  If so, look through our
 * list of open sessions to see which ones might be affected; and make
 * them all requery their auth groups.
 */
int wsmd_recheck_session_auth(const bn_binding_array *bindings);

int wsmd_get_acl_auth(wsmd_session *session, uint32 node_id, 
                      bn_response *resp);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __WSMD_INTERNAL_H_ */
