/*
 *
 */

#ifndef __GCL_H_
#define __GCL_H_

#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "customer.h"

/**
 * \defgroup gcl LibGCL: General Communication Library
 */

/* ------------------------------------------------------------------------- */
/** \file gcl.h General Communication Library.
 * \ingroup gcl
 *
 * This library implements a message-oriented request/response
 * protocol which is used to communicate between many processes in a
 * Samara-based system, as well as between Samara-based systems in
 * some cases.
 *
 * It has a lower-layer API, documented in this file, on which 
 * various different body protocols can be run.  The most common
 * body protocol, "binding node" (::gbp_proto_bnode), has an API
 * described in bnode.h, bnode_proto.h, and bnode_types.h.
 * The body protocol does not wrap all of the GCL API: there are
 * certain GCL functions, mainly for connection setup and teardown,
 * that you will still need to use.
 */

#include <sys/socket.h>
#include "common.h"
#include "array.h"
#include "tstring.h"
#include "ttime.h"
#include "tbuffer.h"
#include "fdintrcb.h"

typedef struct gcl_frame_header gcl_frame_header;
typedef struct gcl_session gcl_session;
typedef struct gcl_handle gcl_handle;


/* ............................................................................
 * Provider names
 */
#define GCL_PROVIDER_MGMTD "mgmtd"
#define GCL_PROVIDER_WSMD  "wsmd"
#if defined(PROD_FEATURE_CMC_CLIENT) || defined(PROD_FEATURE_CMC_SERVER) || \
    defined(PROD_FEATURE_REMOTE_GCL)
#define GCL_PROVIDER_RGP   "rgp"
#define GCL_PROVIDER_RBMD  "rbmd"
#endif /* PROD_FEATURE_CMC_CLIENT || PROD_FEATURE_CMC_SERVER || 
        * PROD_FEATURE_REMOTE_GCL */
#ifdef PROD_FEATURE_CLUSTER
#define GCL_PROVIDER_GEN   "gen"
#endif

extern const char gcl_provider_mgmtd[];
extern const char gcl_provider_wsmd[];
#if defined(PROD_FEATURE_CMC_CLIENT) || defined(PROD_FEATURE_CMC_SERVER) || \
    defined(PROD_FEATURE_REMOTE_GCL)
extern const char gcl_provider_rgp[];
extern const char gcl_provider_rbmd[];
#endif /* PROD_FEATURE_CMC_CLIENT || PROD_FEATURE_CMC_SERVER || 
        * PROD_FEATURE_REMOTE_GCL */
#ifdef PROD_FEATURE_CLUSTER
extern const char gcl_provider_gen[];
#endif

/* ............................................................................
 * Client names
 *
 * These need to be defined as preprocessor constants too so they
 * can be used as constant initializers for structures
 * (e.g. md_commit_forward_action_args).
 */
#define GCL_CLIENT_MDREQ                "mdreq"
#define GCL_CLIENT_CLI                  "cli"
#ifdef PROD_FEATURE_WIZARD
#define GCL_CLIENT_WIZARD               "wizard"
#endif /* PROD_FEATURE_WIZARD */
#ifdef PROD_FEATURE_FRONT_PANEL
#define GCL_CLIENT_FPD                  "fpd"
#endif /* PROD_FEATURE_FRONT_PANEL */
#ifdef PROD_FEATURE_SCHED
#define GCL_CLIENT_SCHED                "sched"
#endif /* PROD_FEATURE_SCHED */
#define GCL_CLIENT_SNMPD                "snmpd"
#define GCL_CLIENT_STATSD               "statsd"
#define GCL_CLIENT_PM                   "pm"
#define GCL_CLIENT_WEB_CGI_LAUNCHER     "web_cgi_launcher"
#define GCL_CLIENT_WEB_REQUEST_HANDLER  "web_request_handler"
#define GCL_CLIENT_WSMD                 "wsmd"
#define GCL_CLIENT_RGP                  "rgp"
#define GCL_CLIENT_RGP_CMC_SERVER       "rgp_cmc_server"
#define GCL_CLIENT_RGP_INNER_PROXY      "rgp_inner_proxy"
#define GCL_CLIENT_RGP_INNER_CLI        "rgp_inner_cli"
#ifdef PROD_FEATURE_CMC_CLIENT
#define GCL_CLIENT_RENDV_CLIENT         "rendv_client"
#endif /* PROD_FEATURE_CMC_CLIENT */
#ifdef PROD_FEATURE_CMC_SERVER
#define GCL_CLIENT_RBMD                 "rbmd"
#endif /* PROD_FEATURE_CMC_SERVER */
#ifdef PROD_FEATURE_CLUSTER
#define GCL_CLIENT_CLUSTERD             "clusterd"
#define GCL_CLIENT_CCL                  "ccl"
#endif /* PROD_FEATURE_CLUSTER */
#define GCL_CLIENT_PROGRESS             "progress"
#ifdef PROD_FEATURE_XML_GW
#define GCL_CLIENT_XG                   "xg"
#endif /* PROD_FEATURE_XML_GW */
#define GCL_CLIENT_TEST                 "test"
#ifdef PROD_FEATURE_VIRT
#define GCL_CLIENT_TVIRTD               "tvirtd"
#endif /* PROD_FEATURE_VIRT */
#ifdef PROD_FEATURE_ACCOUNTING
#define GCL_CLIENT_ACCTD               "acctd"
#endif /* PROD_FEATURE_ACCOUNTING */

extern const char gcl_client_mdreq[];
extern const char gcl_client_cli[];
#ifdef PROD_FEATURE_WIZARD
extern const char gcl_client_wizard[];
#endif /* PROD_FEATURE_WIZARD */
#ifdef PROD_FEATURE_FRONT_PANEL
extern const char gcl_client_fpd[];
#endif /* PROD_FEATURE_FRONT_PANEL */
#if defined(PROD_FEATURE_CMC_CLIENT) || defined(PROD_FEATURE_CMC_SERVER) || \
    defined(PROD_FEATURE_REMOTE_GCL)
extern const char gcl_client_rgp[];
extern const char gcl_client_rgp_cmc_server[];
extern const char gcl_client_rgp_inner_proxy[];
extern const char gcl_client_rgp_inner_cli[];
#endif
#ifdef PROD_FEATURE_CMC_CLIENT
extern const char gcl_client_rendv_client[];
#endif /* PROD_FEATURE_CMC_CLIENT */
#ifdef PROD_FEATURE_CMC_SERVER
extern const char gcl_client_rbmd[];
#endif /* PROD_FEATURE_CMC_SERVER */
#ifdef PROD_FEATURE_SCHED
extern const char gcl_client_sched[];
#endif /* PROD_FEATURE_SCHED */
extern const char gcl_client_snmpd[];
extern const char gcl_client_statsd[];
extern const char gcl_client_pm[];
extern const char gcl_client_web_cgi_launcher[];
extern const char gcl_client_web_request_handler[];
extern const char gcl_client_wsmd[];
extern const char gcl_client_progress[];
#ifdef PROD_FEATURE_XML_GW
extern const char gcl_client_xg[];
#endif /* PROD_FEATURE_XML_GW */
extern const char gcl_client_test[];

#ifdef PROD_FEATURE_CLUSTER
extern const char gcl_client_clusterd[];
extern const char gcl_client_ccl[];
#endif /* PROD_FEATURE_CLUSTER */
#ifdef PROD_FEATURE_VIRT
extern const char gcl_client_tvirtd[];
#endif /* PROD_FEATURE_VIRT */

#ifdef PROD_FEATURE_ACCOUNTING
extern const char gcl_client_acctd[];
#endif /* PROD_FEATURE_ACCOUNTING */

/* 
 * WARNING: Only enable GCL_PROVIDER_INET for testing, or in a carefully
 * constructed environment.  In customer.h , #define GCL_PROVIDER_INET 1
 * to use this.  Read also about MD_PROVIDER_INET in that file.
 */

/* This port number string can be overridden in customer.h */
#ifndef GCL_PROVIDER_INET_MGMTD_PORT
#define GCL_PROVIDER_INET_MGMTD_PORT "14567"
#endif

extern const char gcl_provider_inet_mgmtd_port[];

/* 
 * If GCL_PROVIDER_INET is set, each of the below can be set to one of:
 *
 *  An IP address:    "169.254.7.1" (IPv4) or "fd01:a9fe::7:1" (IPv6)
 *  Disabled:         "-"
 *  Any IP interface: "*"
 *  Loopback:         "L"
 *
 *  The defaults here leave everything disabled.
 *
 */
#ifndef GCL_PROVIDER_INET_MGMTD_IPV4ADDR
#define GCL_PROVIDER_INET_MGMTD_IPV4ADDR "-"
#endif

/* Like above, but with an IPv6 address, like "fd01:a9fe::7:1" */
#ifndef GCL_PROVIDER_INET_MGMTD_IPV6ADDR
#define GCL_PROVIDER_INET_MGMTD_IPV6ADDR "-"
#endif

extern const char gcl_provider_inet_mgmtd_ipv4addr[];
extern const char gcl_provider_inet_mgmtd_ipv6addr[];


/* 
 * This is at the interface between the frame and the body protocol message.
 *
 * NOTE: this is mirrored by
 * src/java/packages/com/tallmaple/gcl/GclBodyProto.java, which
 * must be kept in sync.
 *
 * NOTE: gcl_body_proto_map (in gcl.c) must be kept in sync.
 */
typedef enum {
    gbp_none =        0,
    gbp_proto_admin = 1,
    gbp_proto_bnode = 2,
    gbp_proto_gen   = 3,
} gcl_body_proto;

extern const lc_enum_string_map gcl_body_proto_map[];

/**
 * Type of service.  Within a given session that a client has opened
 * with a provider, one or more services may be opened.  A request is
 * directed to one specific service.
 *
 * NOTE: src/java/packages/com/tallmaple/gcl/BapServiceId.java mirrors
 * these definitions, and should be kept in sync.
 *
 * NOTE: bap_service_id_map (in gcl.c) must be kept in sync.
 */
typedef enum {

    /** No service / invalid */
    bap_service_none =       0,

    bap_service_admin =      1,

    /**
     * Management: this is the most common service used, and generally
     * the one that any mgmtd client should select.
     */
    bap_service_mgmt =       2, 

    /**
     * Proxy manangement: the provider will proxy management requests
     * to mgmtd.  For example, wsmd serves as a mgmtd proxy for the 
     * web request handler, so the request handler opens the 
     * proxy management service with wsmd.
     */
    bap_service_proxy_mgmt = 3,

    bap_service_cluster =    4,

    bap_service_LAST
} bap_service_id;

extern const lc_enum_string_map bap_service_id_map[];


/**
 * GCL session option flags: to be passed to client open functions 
 */
typedef enum {
    gof_none             = 0,
    gof_interactive_yes  = 1 << 0,
    gof_interactive_no   = 1 << 1,
    gof_interactive_mask = (1 << 0 | 1 << 1),

    /**
     * Specifies that this instance of a mgmtd client is not the
     * provider to whom external monitoring requests should be
     * forwarded.
     *
     * The two main reasons you might want to use this are:
     *
     *   - If there is more than one instance of your daemon.
     *     They all might need to connect to mgmtd (with the same client
     *     name) to make requests, but only one of them can be the provider
     *     to which mgmtd forwards requests for external query and action
     *     nodes registered to be provided by this client.
     *
     *   - If you have a long period of initialization, and do not want to
     *     appear ready to field requests from mgmtd during this time.
     *     You might connect at first with gof_not_provider; and when 
     *     ready to serve requests, close this session and reopen another
     *     without gof_not_provider.
     */
    gof_not_provider     = 1 << 2,
} gs_option_flags;

/** Bit field of ::gs_option_flags ORed together */
typedef uint32 gs_option_flags_bf;

/**
 * The type of function the GCL calls when a new session becomes
 * "admin ready", meaning that the greeting and greeting response have
 * been exchanged, and the session is ready for services to be opened.
 * This corresponds to the session entering the gas_open_no_service
 * admin state.
 *
 * NOTE: this is currently only called when the service becomes
 * available (with is_avail set to true), not when it goes away.  But
 * you should check is_avail anyway, as this may change in the future.
 */
typedef int (*gcl_session_admin_ready_func)
    (gcl_session *sess, tbool is_avail, tbool is_provider, void *arg);

/**
 * The type of function the GCL calls to tell the client or provider
 * that on a particular session a given body protocol is now available
 * for user messages, or has ceased to be available.  The semantics of
 * when this is are purely defined by the body protocols. 
 *
 * NOTE: this is currently only called when the service becomes
 * available (with is_avail set to true), not when it goes away.  But
 * you should check is_avail anyway, as this may change in the future.
 *
 * NOTE: this is called both on the client who requested the service
 * to be opened, and on the provider who accepted the service open
 * request.  The is_provider parameter tells which case this is.
 */
typedef int (*gcl_session_service_available_func)
     (gcl_session *sess, gcl_body_proto proto, bap_service_id service,
      tbool is_avail, tbool is_provider, void *arg);


#define gcl_handle_id_max_len 32

/**
 * A structure containing global options to the GCL, to be passed
 * to gcl_init().
 */
typedef struct gcl_global_options {
    /**
     * NOT YET IMPLEMENTED
     */
    tbool ggo_register_event_changes_only;

    /**
     * A function to be called when the session becomes admin ready.
     */
    gcl_session_admin_ready_func ggo_admin_ready_func;

    /**
     * An argument to be passed to ggo_admin_ready_func whenever it is
     * called.
     */
    void *ggo_admin_ready_arg;

    /**
     * A function to be called whenever a new service becomes opened
     * and available for use on any GCL session.  See the use in
     * rgp_outer.c for an example.
     */
    gcl_session_service_available_func ggo_service_avail_func;

    /**
     * An argument to be passed to ggo_service_avail_func whenever
     * it is called.
     */
    void *ggo_service_avail_arg;

    /**
     * Identifying string for this handle.  This is optional, and may be
     * left empty without harm.  It exists only for debugging: it will
     * be used in log messages pertaining to sessions on this handle.
     */
    char ggo_handle_id[gcl_handle_id_max_len];

    /**
     * Maximum number of sessions, above which a GCL provider will not
     * accept() new connections.  -1 means to take the default, 0 means
     * no limit.
     */
    int32 ggo_max_sessions;

} gcl_global_options;


typedef struct gcl_session_options {
    tbool gso_XXX;
} gcl_session_options;

/*
 * NOTE: this is mirrored by
 * src/java/packages/com/tallmaple/gcl/GclSessionState.java, which
 * must be kept in sync.
 *
 * NOTE: gcl_session_state_map (in gcl.c) must be kept in sync.
 */
typedef enum {
    gss_none =         0,

    /** Was initialized or better, now closed */
    gss_closed =       1,

    /** Initialized, but has no state */
    gss_initialized =  2,

    /** Listening for connections (providers only) */
    gss_listening =    3, 

    /** Has socket level connection */
    gss_connected =    4,
} gcl_session_state;

extern const lc_enum_string_map gcl_session_state_map[];

typedef enum {
    gas_none = 0,
    gas_invalid = 1,
    gas_init,
    gas_connected,          /* socket level connection only */
    gas_wait_greeting_response,
    gas_open_no_service,    /* did greeting: server waiting for service open */
    gas_wait_service_open_response,
    gas_service_ready,
    gas_closed
} gbp_admin_state;

/** @name GCL-wide startup and shutdown */

/*@{*/

/**
 * Populate a gcl_global_options structure with the defaults provided
 * by the GCL.
 */
int gcl_options_get_defaults(gcl_global_options *ret_ggo);

/** \cond */
int gcl_session_options_get_provider_defaults(gcl_session_options *ret_gso);
int gcl_session_options_get_client_defaults(gcl_session_options *ret_gso);
/** \endcond */

/**
 * Initialize the GCL.  This function must be called first before
 * most other GCL functionality can be used.
 * \param options Options to the GCL.  Specify NULL for defaults.
 * If not using defaults, call gcl_options_get_defaults() to fill
 * out a structure with defaults before changing any of them.
 * \param fdich File descriptor interest callback handler.  This gives
 * the GCL the means to tell you (or whoever is managing the event loop)
 * when it needs to watch its session file descriptors for readability
 * or writability.
 * \retval  ret_gh GCL handle, to be passed back to many GCL APIs.
 */
int gcl_init(gcl_handle **ret_gh, const gcl_global_options *options,
             fdic_handle *fdich);

/**
 * Deinitialize the GCL.  Closes all GCL sessions, and frees their state. 
 * Does not flush output.  Cleans up all other global gcl state, such 
 * as protocol state.
 *
 * Be careful using this in any context where you might later
 * accidentally try to use any GCL data structures you had saved
 * pointers to, like your session.  Standard practice is to call
 * gcl_shutdown() when you first decide you want to close down the
 * GCL, but then only call gcl_deinit() later after you have
 * completely exited your event loop.
 *
 * \param inout_gh The GCL handle to be deinitialized.  The caller's
 * pointer is set to NULL to avoid accidental use of a dangling pointer.
 */
int gcl_deinit(gcl_handle **inout_gh);

/**
 * "Shut down" the GCL, which is a step short of a full deinitialization.
 * This closes all of the open sessions, without flushing state, but
 * leaves the sessions allocated, and the GCL initialized.  
 */
int gcl_shutdown(gcl_handle *gh);


/**
 * Free all the sessions which have been closed.  Must not be called
 * from a GCL callback function, or descendent of such a function.
 *
 * CAUTION: this is a dangerous function because it frees sessions which
 * you may have your own pointers to, without NULLing out those pointers.
 * You may end up with dangling session pointers after calling this!
 * Make sure you know what you are doing.
 */
int gcl_session_free_closed(gcl_handle *gh);

/*@}*/

/* For debugging */
int gcl_session_dump(gcl_session *sess);
int gcl_session_dump_all(gcl_handle *gclh);

enum {
    /**
     * Number of times to retry connecting to a provider, if retries
     * are requested.
     */
    gcl_session_connect_retry_count = 5,

    /**
     * Amount of time to wait between retries for connecting to a 
     * provider, if retries are requested.
     */
    gcl_session_connect_retry_delay_ms = 1500,

    gcl_session_open_timeout = 7500  /* time to wait in ms for open service */
};

/** 
 * @name GCL client connect functions
 *
 * These are APIs a GCL client can use to establish a session with
 * a GCL provider and prepare the session to be used.  Generally 
 * there are three steps:
 *   -# Create a client session structure, e.g. with 
 *      gcl_session_client_new().  You'll need one of these 
 *      for every session you're going to open.  
 *   -# Establish a connection with a provider, e.g. with
 *      gcl_session_client_connect_by_provider().
 *   -# Open one or more services from the provider, e.g. with
 *      gcl_session_client_open_service().  Multiple services
 *      may be multiplexed over a single session, though commonly
 *      only one is used.
 */

/*@{*/

/**
 * Create a new GCL client session.  Usually followed by a call to
 * gcl_session_client_connect_by_provider() to open a connection to
 * the provider.
 * \param gh GCL handle from gcl_init().
 * \param client_name A string uniquely identifying the binary of the
 * client who is connecting.  It does not have to uniquely identify 
 * the process, since the pid of the client is appended to this to form
 * the complete peer name.  Normally this client name is defined in 
 * gcl.h for base Samara components (e.g. ::gcl_client_cli), or in
 * customer.h for customer components (e.g. ::gcl_client_echod).
 * The name cannot contain '-', and must be < 20 characters long.  If the
 * gof_not_provider client flag is used on the session, the name must 
 * be < 14 characters long.
 * \param local_addr
 * \param local_addr_len
 * \retval ret_session Handle to the newly-created GCL client session.
 */
int gcl_session_client_new(gcl_handle *gh, 
                           const char *client_name,
                           /* local_addr can be NULL for auto */
                           const struct sockaddr *local_addr,
                           uint32 local_addr_len,
                           gcl_session **ret_session);

typedef enum {
    ggid_auto = -1,
    ggid_remote_chooses = -2
} ggid_id_choices;

/**
 * Like gcl_session_client_new(), but offers more options.
 *
 * \param gh GCL handle
 * \param client_name Client name
 * \param client_flags A bit field of ::gs_option_flags ORed together.
 * \param uid The uid we should open the session as, ::ggid_auto to use our
 * current uid, or ::ggid_remote_chooses to allow the provider to choose.
 * You can only specify a different uid if you are uid 0.
 * \param gid The uid we should open the session as, ::ggid_auto to use our
 * current gid, or ::ggid_remote_chooses to allow the provider to choose.
 * You can only specify a different gid if you are uid 0.
 * \param username Remote username, presumably matching the uid param,
 * or NULL for none.
 * \param local_addr 
 * \param local_addr_len
 * \retval ret_session Handle to the newly-created GCL client session.
 */
int gcl_session_client_set_user_new(gcl_handle *gh, 
                                    const char *client_name,
                                    gs_option_flags_bf client_flags,
                                    int32 uid, int32 gid,
                                    const char *username,
                                    /* local_addr can be NULL for auto */
                                    const struct sockaddr *local_addr,
                                    uint32 local_addr_len,
                                    gcl_session **ret_session);

/**
 * Used by programs running as root that want to be a different user, and/or
 * already have an fd they want to use for the session.
 */
int gcl_session_client_full_new(gcl_handle *gh, 
                                const char *client_name,
                                uint32 client_flags,
                                int32 uid, int32 gid,
                                const char *username,
                                /* 
                                 * -1 for auto (local) or an AF_... .
                                 * This is mutually exclusive with
                                 * local_addr, and is used to specify
                                 * the address family, but not bind to a
                                 * local address.
                                 */
                                int local_af_no_addr,
                                /* local_addr can be NULL for auto */
                                const struct sockaddr *local_addr,
                                uint32 local_addr_len,
                                int existing_fd,
                                tbool existing_fd_connected,
                                gcl_session **ret_session);


#if 0 /* NYI */
/*
 * Used when a socket connection is already up, and then a GCL session wants
 * to run over the connection.
 */
int gcl_session_client_new_from_fd(gcl_handle *gh, 
                                   int fd,
                                   tbool fd_is_connected,
                                   gcl_session **ret_session);
#endif /* NYI */

int gcl_session_client_connect_by_sockaddr(gcl_session *sess, 
                                           const struct sockaddr *prov_sa, 
                                           uint32 prov_sa_len,
                                           tbool retry_on_failure);

/**
 * Connect to a GCL provider using a client session that has already
 * been created.
 * \param sess The client session with which to connect.
 * \param provider_name The name of the provider to whom to connect.
 * The Samara standard providers are defined in this file.  By far
 * the most commonly used is gcl_provider_mgmtd.
 * \param retry_on_failure If we initially fail to connect, should
 * we retry?  We will retry ::gcl_session_connect_retry_count times,
 * waiting ::gcl_session_connect_retry_delay_ms milliseconds before
 * each retry attempt.
 */
int gcl_session_client_connect_by_provider(gcl_session *sess, 
                                           const char *provider_name,
                                           tbool retry_on_failure);

/**
 * Called by the client to open a "service".  By default, there are no 
 * services enabled on a connection.  This open call calls back the 
 * registered function when a new service is opened (the one specified
 * in ggo_service_avail_func).
 */
int gcl_session_client_open_service(gcl_session *sess, 
                                    gcl_body_proto body_proto,
                                    bap_service_id service,
                                    const char *lang_tag,
                                    void *service_arg);


/**
 * Like gcl_session_client_open_service(), except this is a blocking
 * call: it does not return until the service is successfully opened,
 * or the request times out.
 */
int gcl_session_client_open_service_blocking(gcl_session *sess, 
                                             gcl_body_proto body_proto,
                                             bap_service_id service,
                                             const char *lang_tag,
                                             void *service_arg,
                                             lt_dur_ms max_wait_ms);

/*@}*/

int gcl_session_get_id(const gcl_session *sess, int_ptr *ret_session_id);

int gcl_session_lookup(gcl_handle *gh, const char *remote_name, 
                       tbool only_find_ready, tbool exclude_client_only,
                       gcl_session **ret_sess);

int gcl_session_id_lookup(gcl_handle *gh, uint32 session_id,
                          gcl_session **ret_sess);

int gcl_session_get_fd(const gcl_session *sess, 
                       int *ret_fd);

int gcl_session_get_handle(const gcl_session *sess,
                           gcl_handle **ret_handle);

/**
 * Run the session state machine until a specified event occurs.  This is
 * provided as a convenience to clients, and so we can implement the
 * blocking service open below.
 */
int
gcl_session_do_blocking_io(gcl_handle *gclh, 
                           gcl_session *sess,
                           tbool until_message_sent,
                           tbool until_message_received,
                           tbool until_session_state_change,
                           tbool until_bp_state_change,
                           gcl_body_proto bp_test,
                           lt_dur_ms max_wait_ms);


int gcl_session_provider_new(gcl_handle *gh, 
                             const char *provider_name,
                             /* local_addr can be NULL for auto */
                             const struct sockaddr *local_addr,
                             uint32 local_addr_len,
                             gcl_session **ret_session);

/**
 * Create a new GCL session as a provider, given an fd already open
 * with the client.  For instance, this is used by RGP to become a 
 * provider on a session opened over an ssh connection initiated 
 * by a remote client.
 *
 * \param gh GCL handle
 * \param provider_name Our own provider name.  Should be defined in 
 * gcl.h along with other provider names, e.g. ::gcl_provider_mgmtd.
 * The name cannot contain '-', and must be < 32 characters long.
 * \param fd The file descriptor to use for the session.  Note that 
 * depending on your circumstances, you may need to use the API
 * in fd_mux.h to multiplex two separate fds (one
 * readable and one writeable) through a single one to pass here.
 * e.g. STDIN_FILENO and STDOUT_FILENO.
 * \param fd_is_connected
 * \param fd_is_listening
 * \retval ret_session The newly-created session.
 */
int gcl_session_provider_new_from_fd(gcl_handle *gh, 
                                     const char *provider_name,
                                     int fd,
                                     tbool fd_is_connected,
                                     tbool fd_is_listening,
                                     gcl_session **ret_session);

int gcl_session_provider_enable(gcl_session *session);


/**
 * Used by programs running as root that want to be a different user, and/or
 * already have an fd they want to use for the session.
 */
int gcl_session_provider_full_new(gcl_handle *gh, 
                                  const char *provider_name,
                                  int32 remote_uid,
                                  int32 remote_gid,
                                  const char *remote_username,
                                  int32 remote_interactive,   /* tbool or -1 */
                                  /* local_addr can be NULL for auto */
                                  const struct sockaddr *local_addr,
                                  uint32 local_addr_len,
                                  int existing_fd,
                                  tbool existing_fd_connected,
                                  tbool existing_fd_listening,
                                  gcl_session **ret_session);


/* See gcl_session_get_state() below */

int gcl_session_get_auth(const gcl_session *sess, 
                         tbool *ret_remote_done,
                         int32 *ret_remote_uid, int32 *ret_remote_gid,
                         tstring **ret_remote_username,
                         tbool *ret_remote_interactive,
                         tbool *ret_incoming_done,
                         int32 *ret_incoming_uid, int32 *ret_incoming_gid,
                         tstring **ret_incoming_username,
                         tbool *ret_incoming_interactive);

/**
 * Get information about the origins of the remote and incoming
 * usernames associated with this session.  A 'true' for each of these
 * means the username was synthesized (in the "i:<pid>-<uid>-<gid>"
 * format) because we didn't have a real username.  A 'false' means
 * that the corresponding username was real.
 *
 * XXX/EMT: explain how you'd get a real username.  See env vars
 * checked in gcl_session_new(), but by the time we get here, the
 * username could have come locally from that, or from the other side
 * of the GCL connection.  (At least the "incoming" username could be
 * from the other side.  Not sure what the "remote" username is for.)
 *
 * If either username is NULL or empty for any reason, 'false' will
 * be returned for its flag.
 */
int gcl_session_get_username_synth(const gcl_session *sess,
                                   tbool *ret_remote_username_synth,
                                   tbool *ret_incoming_username_synth);

int gcl_session_get_service_id_open(const gcl_session *sess, 
                                    bap_service_id service_id,
                                    tbool *ret_service_open);

int gcl_session_get_lang_tag(const gcl_session *sess, 
                             char **ret_lang_tag);


int gcl_session_get_session_names(const gcl_session *sess, 
                                  tstring **ret_local_name,
                                  tstring **ret_peer_name);

int gcl_session_get_session_name_ids(const gcl_session *sess, 
                                     tstring **ret_local_name_id,
                                     tstring **ret_peer_name_id);

typedef struct gcl_session_stats {
    /** Count of complete frames sent */
    uint64 gss_sent_frames;

    /** Count of all bytes sent */
    uint64 gss_sent_bytes;

    /** Count of frames waiting to be sent (including partial ones) */
    uint64 gss_pending_outgoing_frames;

    /** Count of all bytes waiting to be sent */
    uint64 gss_pending_outgoing_bytes;

    uint64 gss_received_frames;
    uint64 gss_received_bytes;
    uint64 gss_received_dispatched_frames;
    uint64 gss_received_dispatched_bytes;
} gcl_session_stats;

/**
 * Populate the provided structure with current statistics on the
 * specified GCL session.
 */
int gcl_session_get_stats(const gcl_session *sess,
                          gcl_session_stats *inout_stats);

/**
 * Get the name of the peer connected to a session.  This is 
 * particularly useful in gdb where it's a lot easier to get
 * information if you can get it in the return value instead
 * of assigned to a variable you pass in.
 *
 * The caller is responsible for freeing the return value.
 */
char *
gcl_session_get_peer_name_quick(const gcl_session *sess);

/**
 * @name Session deinitialization
 *
 * When you are done talking to a provider, you don't have to close
 * the services one by one: you just close the entire session.
 */

/*@{*/

/**
 * Close the specified session.  Note that this leaves the session
 * structure allocated, which may be important if you are in the
 * middle of a callback called by the GCL on behalf of the session,
 * or if some of your other code might have pointers to the session
 * structure and try to reference it, if only to find out if it is 
 * still open.
 *
 * \param session The GCL session to be closed.
 *
 * \param flush_output Specifies whether or not we should attempt to
 * flush any pending output on this session before closing.  Note that
 * passing \c true here still does not make this a blocking call; we
 * just make a non-blocking attempt to flush as much as we can.  Also
 * note that even if you pass \c true, it only attempts to send all
 * the data from this end, but even if it is, there is no guarantee
 * that it will be received by the other end.  The only way to
 * guarantee that is to wait for the other end's response.  So this
 * only makes message loss less likely, not impossible.
 */
int gcl_session_close(gcl_session *session, tbool flush_output);

/**
 * Free the specified session structure.  The caller's pointer is set
 * to NULL to prevent references to the dangling pointer.
 *
 * You should call gcl_session_close() explicitly to ensure that the
 * session is closed before calling this function.  If this function
 * is called with a session that is not closed, it will close the
 * session automatically (without flushing output), but log a 
 * warning message about incorrect usage.
 */
int gcl_session_free(gcl_session **inout_session);

/*@}*/

int gcl_session_send_frame_components(gcl_session *sess, 
                                      gcl_body_proto body_proto,
                                      uint16 body_msg_type,
                                      uint32 body_length,
                                      const void *body);

int gcl_session_send_frame_components_tb(gcl_session *sess, 
                                         gcl_body_proto body_proto,
                                         uint16 body_msg_type,
                                         uint32 header_space_left,
                                         tbuf **inout_tb);

int gcl_session_handle_readable(gcl_session *sess);

int gcl_session_get_state(const gcl_session *sess, 
                          gcl_session_state *ret_gss, 
                          gbp_admin_state *ret_gas);

/**
 * CAUTION: this function returns a pointer to a structure within
 * the GCL session structure, so it is not safe for use in
 * multithreaded environments where another thread may subsequently
 * cause the returned structure to be modified, or free the session.
 * But it is OK to use from the sole thread using a session.
 */
struct sockaddr *gcl_get_remote_addr(gcl_session *sess);

uint16 gcl_get_remote_addr_len(gcl_session *sess); 


/** 
 * Limit the transmit bandwith a GCL session will use.  By default, there is
 * no bandwidth limiting on sessions.  If enabled, the kbytes_sec parameter
 * gives a maximum number of 1024 bytes that will be written out by GCL in a
 * given second.  If kbytes_sec is 0, no limiting is done.  There is no long
 * term averaging.  Bandwidth limiting is likely only useful with network
 * connections, as in the CMC server case.  Note that there is no receive
 * limiting, only transmit limiting, so in some cases both sides may want to
 * have a bandwidth limit.
 *
 * Normally GCL sessions write all the available data they can, until the
 * socket would block.  With bandwidth limiting enabled, data is always sent
 * in chunks, which never exceed the smaller of 32 KBytes or kbytes_sec / 2
 * .  kbytes_sec / 2 represents the number of bytes that are allowed in
 * 500ms.  This is done to allow for more fair sharing of the link
 * bandwidth.
 */
int gcl_session_set_bwlimit(gcl_session *sess, tbool enable, 
                            uint32 kbytes_sec);

/**
 * Retrieve the current bandwidth limiting settings.  If the limit is not
 * enabled, the kbytes_sec setting is not used.
 */
int gcl_session_get_bwlimit(gcl_session *sess, tbool *ret_enable, 
                            uint32 *ret_kbytes_sec);



#ifdef __cplusplus
}
#endif

#endif /* __GCL_H_ */
