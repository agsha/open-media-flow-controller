/*
 *
 * remgcl.h
 *
 *
 *
 */

#ifndef __REMGCL_H_
#define __REMGCL_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/** \defgroup librgc LibRemGCL: Remote GCL Client Library */

/* ------------------------------------------------------------------------- */
/** \file remgcl.h Remote GCL Client Library
 * \ingroup librgc
 *
 * Use this library to connect to a remote system as a GCL client.
 *
 * <em>CAUTION: this library is mainly designed for use by the CMC in
 * conjunction with RGP.  It is still a semi-internal API subject to
 * change.  Other components should not rely on this API without
 * consulting Tall Maple first.</em>
 *
 * The library currently only supports connecting with SSH but could be
 * expanded to use other protocols in the future.
 *
 * The library forks an SSH client and uses it to log into the remote
 * system using authentication data provided by the caller.  It then 
 * opens a GCL session with the remote system and makes that session
 * available for use by the caller.
 *
 * To use this library:
 *
 *   - Use rgc_create() to create a new remote GCL client record.
 *     Each of these records corresponds to a single GCL session
 *     (using one or more services) on a single remote host.
 *
 *   - Fill out the required fields of the rgc_rec structure.
 *     The comments below describe what each of these fields is
 *     for and when they are needed.
 *
 *   - Call rgc_connect() to connect to the remote host, authenticate,
 *     open a GCL session, and open the "mgmt" and "proxy_mgmt"
 *     services.  This is a synchronous call, and will return when all
 *     of these tasks are completed.  If there is an error, it will
 *     return an appropriate error code.
 *
 *   - If the connection is successful, the rgc_rec structure will
 *     now have a GCL session field you can use.
 *
 *   - When you are done with the connection, call rgc_disconnect().
 *     This will close the GCL session and the connection with the
 *     remote host, leaving everything as it was before you connected.
 *
 *   - When disconnecting, the library sends a SIGTERM to the SSH
 *     client and needs to know when it terminates.  You have two 
 *     options for how its termination is handled:
 *       <ol>
 *       <li> If your program does not launch any other processes
 *          (and thus does not have to listen for SIGCHLD), and only
 *          uses this library for a single connection, it is generally
 *          easiest to let the library handle this, which is the default
 *          behavior.
 *
 *       <li> If your program does handle SIGCHLD and may end up reaping
 *          the SSH client itself (e.g. by calling lc_wait() with a
 *          pid of -1 or 0), or if you use this library to manage
 *          multiple connections, then you should set the rr_handle_sigchld 
 *          field to false.  When you reap the SSH process (the pid of a
 *          given rgc_rec's SSH process is exposed as rr_child_pid),
 *          call rgc_child_died() to report it.
 *
 *          This is because if the library cannot be fairly certain
 *          that the SIGCHLD it received is for the session that
 *          registered the handler, it may end up sleeping too long
 *          waiting to reap its child, which would slow down your
 *          program excessively.
 *       </ol>
 *     Note that this termination handling also applies to cases when
 *     the SSH client exits unexpectedly.  This could happen on 
 *     authentication failure, if the connection is reset, etc.
 */

/*
 * XXX/EMT: does the caller need to do anything from its session
 * close handler?  That is, the function you register with
 * fdic_libevent_set_fd_close_handler().
 * 
 * XXX/EMT: currently this library is designed for use with RGP, which
 * provides both 'proxy_mgmt' and 'mgmt' services.  The 'mgmt'
 * services provided by it are somewhat application-specific (they
 * allow the execution of CLI commands on the remote system), and may
 * not be needed for all applications.  At some point perhaps we
 * should require/allow callers to be responsible for opening their
 * own services.
 */

#include "tstring.h"
#include "fd_mux.h"
#include "gcl.h"
#include "libevent_wrapper.h"


/* ------------------------------------------------------------------------- */
/** Type of authentication to use to log into remote system.
 */
typedef enum {
    rat_none = 0,
    rat_password,
    rat_ssh_rsa2,
    rat_ssh_dsa2
} rgc_auth_type;

static const lc_enum_string_map rgc_auth_type_map[] = {
    { rat_none, "none" },
    { rat_password, "password" },
    { rat_ssh_rsa2, "ssh-rsa2" },
    { rat_ssh_dsa2, "ssh-dsa2" },
    { 0, NULL }
};


/* ------------------------------------------------------------------------- */
/** State of the remote GCL client.
 */
typedef enum {
    rs_none = 0,

    /**
     * We are not connected to the remote system.  A new rgc_rec
     * starts in this state, and also returns to this state when the
     * connection is broken.
     */
    rs_disconnected,

    /**
     * We are in the process of connecting to the client and opening
     * a GCL session.  This state is only seen by callers who use 
     * rgc_connect_nonblock(), since rgc_connect() does not return
     * until we have either finished connecting, or failed.
     */
    rs_connecting,

    /**
     * The SSH client is running, we are authenticated, and we have
     * opened a GCL session with the remote host.
     */
    rs_connected,

    /**
     * We are trying to disconnect, and are in the process of
     * terminating the SSH client.  We have sent it a SIGTERM, but
     * it has not terminated yet.
     */
    rs_term_sent,

    /**
     * We are trying to disconnect, and are in the process of
     * terminating the SSH client.  It did not respond to our SIGTERM
     * within our timeout period, so we sent it a SIGKILL, and are
     * now waiting for it to terminate.
     */
    rs_kill_sent
} rgc_state;


static const lc_enum_string_map rgc_state_type_map[] = {
    { rs_none, "none" },
    { rs_disconnected, "disconnected" },
    { rs_connecting, "connecting" },
    { rs_connected, "connected" },
    { rs_term_sent, "term_sent" },
    { rs_kill_sent, "kill_sent" },
    { 0, NULL }
};

#define rgc_state_to_string(val) lc_enum_to_string(rgc_state_type_map, val)

typedef struct rgc_rec rgc_rec;

/* ------------------------------------------------------------------------- */
/** Callback to be called when the connection state is changed.
 *
 * \param data The opaque data pointer given in rr_state_change_data to
 * be passed back to this function.
 *
 * \param rgc The remgcl record whose connection state has changed.
 * At the point your function is called its rr_state field HAS
 * been updated to the new state, but in other respects it may be 
 * at any point during the transition so you should not rely on
 * particulars (e.g. during a voluntary termination, you may be called
 * before or after we send the SIGTERM to the ssh process).
 *
 * \param old_state The old connection state before the change.
 *
 * \param new_state The new connection state after the change.
 * This is redundant with rgc->rr_state, but is provided separately
 * to make it more explicit that the new state is being referenced.
 *
 * \param requested Indicates whether the connection state change was
 * caused by a direct voluntary request from the caller.  If not,
 * if it was closed spontaneously, or we closed it due to some other
 * failure.  Some callers might use this information, for example, 
 * to decide whether to schedule a retry of the connection.
 */
typedef int (*rgc_state_change_callback)(void *data, rgc_rec *rgc,
                                         rgc_state old_state,
                                         rgc_state new_state,
                                         tbool requested);

/*
 * If you add any reasons here, make sure to update the reference to
 * them in nodes.txt as well.
 */
typedef enum {
    rfr_none = 0,

    /**
     * None of the below conditions were detected.
     */
    rfr_unknown,

    /**
     * Some piece of configuration required to connect was missing
     * entirely.  This error might be generated by libremgcl, or by a
     * potential caller of it before even trying.
     */
    rfr_missing_config,

    /**
     * We were given a hostname (rather than an IP address) to connect
     * with, and were not able to resolve it to an address.
     * SSH printed: "Name or service not known".
     */
    rfr_dns_lookup_failure,

    /**
     * The address we tried to connect with was not reachable.
     * SSH printed: "No route to host".
     */
    rfr_no_route_to_host,

    /**
     * The port to which we tried to connect was not open.
     * SSH printed: "Connection refused".
     */
    rfr_connection_refused,

    /**
     * The ssh client was not happy with the host key on the remote
     * system.  Either we have a known host entry and it does not
     * match the key the remote system sent us; or we do not have a
     * known host entry, and have been configured not to automatically
     * accept new host keys.
     * SSH printed: "Host key verification failed".
     */
    rfr_host_key_mismatch,

    /**
     * We were not able to authenticate to the remote system.
     * SSH printed: "Permission denied".
     */
    rfr_auth_failure,

    /**
     * We waited too long without trying to open a connection.  This
     * is a somewhat vague error, since it doesn't actually tell you
     * how far we got (e.g. did we manage to log in successfully).
     * SSH has not printed anything; our call to open the GCL session
     * returned with a timeout.
     */
    rfr_timeout,
} rgc_failure_reason;


/*
 * Map a failure reason enum to a string most compactly representing
 * that value.
 */
static const lc_enum_string_map rgc_failure_reason_map[] = {
    {rfr_none, "none"},
    {rfr_unknown, "unknown"},
    {rfr_missing_config, "missing_config"},
    {rfr_dns_lookup_failure, "dns_lookup_failure"},
    {rfr_no_route_to_host, "no_route_to_host"},
    {rfr_connection_refused, "connection_refused"},
    {rfr_host_key_mismatch, "host_key_mismatch"},
    {rfr_auth_failure, "auth_failure"},
    {rfr_timeout, "timeout"},
    {0, NULL}
};


/*
 * Map a failure reason enum to the user-visible string we should
 * display.
 *
 * XXX/EMT: I18N.
 */
static const lc_enum_string_map rgc_failure_reason_friendly_map[] = {
    {rfr_none, "None"},
    {rfr_unknown, "Unknown"},
    {rfr_missing_config, "Missing configuration"},
    {rfr_dns_lookup_failure, "DNS lookup failure"},
    {rfr_dns_lookup_failure, "Temporary failure in name resolution"},
    {rfr_no_route_to_host, "No route to host"},
    {rfr_connection_refused, "Connection refused"},
    {rfr_host_key_mismatch, "Host key verification failed"},
    {rfr_auth_failure, "Authentication failure"},
    {rfr_timeout, "Timeout"},
    {0, NULL}
};


/*
 * Map a string we might see on the ssh client's stderr to the failure
 * reason enum that it would indicate.
 *
 * XXX #dep/parse: ssh
 */
static const lc_enum_string_map rgc_failure_reason_indicators[] = {
    {rfr_dns_lookup_failure, "Name or service not known"},
    {rfr_no_route_to_host, "No route to host"},
    {rfr_connection_refused, "Connection refused"},
    {rfr_host_key_mismatch, "Host key verification failed"},
    {rfr_auth_failure, "Permission denied"},
    {0, NULL}
};


/* ------------------------------------------------------------------------- */
/** Remote GCL client record.  Each of these records corresponds to one
 * GCL session with one remote host.  Once the session is opened, 
 * however, you may open any number of GCL services with the other end.
 */
struct rgc_rec {
    /* ..................................................................... */
    /** @name Configuration
     * These fields are provided by the caller before initiating the
     * connection.
     */
    /*@{*/

    /**
     * Hostname or IP address of remote host.  This is passed on to the
     * SSH client without review.  Has no default, so must be filled in.
     */
    tstring *rr_addr;

    /**
     * Address family for SSH connection to the remote host.  It can be
     * AF_INET, which will force using only IPv4 addresss, AF_INET6
     * which will force using only IPv6 addresss, or AF_UNSPEC which
     * means address usage is unconstrained.  Defaults to AF_UNSPEC.
     */
    uint16 rr_addr_family;

    /**
     * Port of remote host to which we should connect.  Defaults to 22,
     * the standard ssh port.
     */
    uint16 rr_port;

    /**
     * Type of authentication to use with the remote system.
     * This field is mandatory; as for the other rr_auth_... fields,
     * only ones corresponding to the auth type need to be set.
     */
    rgc_auth_type rr_auth_type;

    tstring *rr_auth_password_username;
    tstring *rr_auth_password_password;
    tstring *rr_auth_ssh_dsa2_username;
    tstring *rr_auth_ssh_dsa2_identity_path;
    tstring *rr_auth_ssh_rsa2_username;
    tstring *rr_auth_ssh_rsa2_identity_path;

    /**
     * Control if the ssh connection does strict hostkey checking
     */
    tbool rr_ssh_hostkey_strict;

    /**
     * Specify if we should ignore the user's known hosts file, and
     * only respect the global one.  Defaults to false.
     */
    tbool rr_ssh_knownhosts_globalonly;

    /**
     * Optionally override the path to the user's known hosts file.
     * Defaults to NULL, meaning no override, and ssh's default is
     * to use ~/.ssh/known_hosts.  If rr_ssh_knownhosts_globalonly
     * is set, this field is ignored.
     */
    tstring *rr_ssh_knownhosts_user_override_path;

    /**
     * What program to run at the other end, and any arguments to pass to
     * it.  These are all in one string, because they are appended together
     * to the 'ssh' command line, and end up as a single argv when the CLI
     * is invoked.
     *
     * We need to end up connected to something that speaks GCL and
     * offers the "mgmt" and "proxy_mgmt" service so we can open a
     * session with it.  Either (a) the user account we log into has
     * such a program as its shell, in which case this field may be
     * empty; or (b) we need to specify the path to one here.
     *
     * This defaults to RGP_RAW_PATH, which is appropriate for raw
     * proxying of mgmtd requests.  CMC callers will need to specify
     * something else, passing other parameters to RGP.
     *
     * NOTE: make sure that whatever you put here is allowed by the
     * cli_exec_allow, defined in customer.h, on the target system.
     */
    tstring *rr_remote_prog_args;

    /**
     * Specifies whether or not libremgcl should install a SIGCHLD
     * handler and attempt to reap its SSH children.  If it does not,
     * the caller must reap them and call rgc_child_died() with the
     * status code.  Defaults to true.
     */
    tbool rr_handle_sigchld;

    /**
     * A function to be called when the state of an rgc session is 
     * changed.  If the caller is just interested in knowing when
     * the connection is formed or broken, this will suffice,
     * though it also provides some additional detail that may or
     * may not be interesting, e.g. distinguishing between when
     * we send a SIGTERM to our ssh client, and when it actually
     * dies so we consider the connection completely broken.
     *
     * Note that this will be called regardless of whether the state
     * change was at the callers request.
     */
    rgc_state_change_callback rr_state_change_func;

    /**
     * Pointer to be passed to rr_state_change_func
     */
    void *rr_state_change_data;

    /**
     * GCL handle: needed to open and manage connection, but library
     * does not take ownership or free later.
     */
    gcl_handle *rr_gclh;

    /**
     * FDIC handle: needed to open and manage connection, but library
     * does not take ownership or free later.
     */
    fdic_handle *rr_fdich;

    const char *rr_gcl_client_name;

    /**
     * Libevent context: needed to open and manage connection, but
     * library does not take ownership or free later.
     */
    lew_context *rr_lwc;

    /*@}*/

    /* ..................................................................... */
    /** @name External State
     * The library exposes this state to the caller.  It is read-only,
     * though the session can be used for sending and receiving messages.
     */
    /*@{*/

    rgc_state rr_state;
    pid_t rr_child_pid;
    gcl_session *rr_gcl_session;

    /**
     * This is set to true when someone requests to close a connection
     * because they don't want the connection open anymore.  It is 
     * false when the connection closes on its own, or if we close
     * it due to some fatal error such as an authentication failure.
     * It is mainly for the consumption of the disconnection function,
     * and has no meaning except during the transition from connected
     * to disconnected.
     */
    tbool rr_voluntary_close;

    /**
     * This attempts to keep track in more detail of why we don't have
     * a connection, if we wanted one.
     */
    rgc_failure_reason rr_cxn_failure_reason;

    /*@}*/

    /* ..................................................................... */
    /** @name Internal State
     * The library maintains this state for its own internal use, and
     * should be ignored by the caller.
     */
    /*@{*/

    int rr_child_password_fds[2];
    int rr_child_stdin_fds[2];
    int rr_child_stdout_fds[2];
    int rr_child_stderr_fds[2];
    int rr_child_mux_fd;
    fdmux_state *rr_child_mux_state;

    /**
     * Watch for the SIGCHLD of our child process.  This is only
     * installed if rr_handle_sigchld is true; otherwise, we depend
     * on the caller to receive SIGCHLD from our child, reap it,
     * and call us.
     */
    lew_event *rr_ev_sigchld;

    /**
     * Timer to tell us when to move to the next phase of termination.
     * We are somewhat simpler than PM.  First we send a SIGTERM, and
     * set this timer.  If the timer goes off, we send a SIGKILL, and
     * set this timer again.  If the timer goes off again, we give up
     * and pretend the child is dead.
     */
    lew_event *rr_ev_termination;

    /*@}*/
};


/* ------------------------------------------------------------------------- */
/** Create a record for a single remote system to connect to.
 */
int rgc_create(rgc_rec **ret_rr);


/* ------------------------------------------------------------------------- */
/** Connect to a remote system.  Enough configuration in the rgc_rec
 * must be filled in for us to establish an connection, authenticate,
 * and open a GCL session.
 *
 * This opens both the "mgmt" service and the "mgmt proxy" service.
 * The former is used to send messages directly to the endpoint
 * we are talking to.  The latter is used to send messages which are
 * intended to be proxied to mgmtd.  The latter is opened last,
 * making it the default service on the GCL session (what you get
 * if you don't set the service ID explicitly).
 *
 * Note: this is a synchronous call which will not return until the 
 * session is established.
 */
int rgc_connect(rgc_rec *rr);


/* ------------------------------------------------------------------------- */
/** Like rgc_connect(), except does not block.
 *
 * rgc_connect() blocks on opening GCL services over the ssh fd,
 * immediately after forking ssh, so it could effectively block on
 * either establishing the ssh connection, or communication with the
 * remote RGP once that connection is established.
 *
 * This function goes as far as it can without blocking, but returns
 * before trying to open the first GCL service.  You must set both
 * ggo_admin_ready_func and ggo_service_avail_func in the GCL global
 * options used to initialize your GCL handle.  Call
 * rgc_connect_continue() when your admin_ready_func is called (which
 * should happen once, first), and when your service_avail_func is
 * called (which should subsequently happen once for each service
 * we're opening, which is two for now).  After all of the desired
 * services are opened, it will eventually tell you that the
 * connection is complete and ready to use.
 *
 * If there is a direct failure (e.g. a bad greeting from the
 * provider, or the session is closed by the other side), the session
 * will be closed, and your fd close handler will be called.  But if
 * you want to time out on the connection after not making progress
 * for some period, you'll need to set your own timer, and then call
 * rgc_disconnect() when it goes off, to cancel the connection attempt.
 */
int rgc_connect_nonblock(rgc_rec *rr);

/* ------------------------------------------------------------------------- */
/** Continue connection process initiated with rgc_connect_nonblock().
 * Call this whenever your admin_ready_func or service_avail_func is
 * called.  It will return 'true' for connected only once all of the
 * desired services are opened.
 */
int rgc_connect_continue(rgc_rec *rr, tbool *ret_connected);

/* ------------------------------------------------------------------------- */
/** Disconnect from a remote system, or cancel a pending nonblocking
 * connection.  The 'synchronous' parameter specifies whether we wait
 * for the termination before returning; this is only legal if
 * rr_handle_sigchld is set to true.
 *
 * So there are three possible scenarios:
 *
 *   1. rr_handle_sigchld true, synchronous: first, deinstall our
 *      SIGCHLD handler, as it would only be confused by being called
 *      in this case.  Send SIGTERM, wait for termination.  If no
 *      termination in appropriate time, try SIGKILL.  If still no
 *      termination, give up.  When it terminates or we give up,
 *      do post-termination handling.  Then return.  All of the
 *      waiting is done with usleep() from lc_wait().
 *
 *   2. rr_handle_sigchld true, asynchronous: send SIGTERM,
 *      and set a timer.  Then return, and wait for either
 *      our SIGCHLD handler or our timer to fire.  If timer fires,
 *      we send SIGKILL and set timer again.  Post-termination
 *      handling is done after we are notified of termination, or
 *      we give up after a timer fires for the second time.
 *
 *   3. rr_handle_sigchld false: same as case #2, except that 
 *      it is the caller, not our own SIGCHLD handler, that will
 *      tell us when the client terminates.
 */
int rgc_disconnect(rgc_rec *rr, tbool synchronous);


/* ------------------------------------------------------------------------- */
/** Handle the termination of an ssh client associated with a
 * particular session.  If rr_handle_sigchld is true, the library
 * will take care of this and you will not call this function. 
 * But if rr_handle_sigchld is false, when you reap a process that
 * was the child SSH process of this library, call this function
 * to notify the library.
 *
 * NOTE: this function will call your disconnection callback after it
 * is finished, so don't call this function from your disconnection
 * callback, or call your disconnection callback yourself.
 */
int rgc_child_died(rgc_rec *rr);


/* ------------------------------------------------------------------------- */
/** Delete an RGC record structure.
 */
int rgc_delete(rgc_rec **inout_rr);


/* ------------------------------------------------------------------------- */
/** We define this ourselves instead of using gcl_session_open_timeout
 * because we want to give the ssh client itself a bit more of a
 * chance to time out.  e.g. in tests, it took 12 seconds to get the
 * "no route to host" message when the client was simply off the
 * network, and we'd rather have the failure classified as that rather
 * than a timeout in opening the GCL session.
 */
static const lt_dur_ms remgcl_session_open_timeout = 15000;

/*
 * XXX/EMT: if we got some positive feedback about having logged in,
 * that would help us too.
 */


#ifdef __cplusplus
}
#endif

#endif /* __REMGCL_H_ */
