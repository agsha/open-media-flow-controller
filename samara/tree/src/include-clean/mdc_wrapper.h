/*
 *
 * mdc_wrapper.h
 *
 *
 *
 */

#ifndef __MDC_WRAPPER_H_
#define __MDC_WRAPPER_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "md_client.h"
#include "libevent_wrapper.h"

/*
 * XXX/EMT: TO DO:
 *
 *   - Integrate more tightly with mdc_backlog.h?
 */

/** \defgroup mdc_wrapper LibMdc_wrapper: md_client wrapper API: helping 
 * mgmtd clients establish a connection with mgmtd */

/**
 * \file mdc_wrapper.h High-level wrapper for callers of md_client API
 * who use libevent; manages opening of the mgmtd connection, sending
 * queries, and waiting for responses.
 * \ingroup mdc_wrapper
 *
 * This API is for mgmtd clients who use libevent to dispatch events
 * (watch for fd activity, signals, and timers), and who use the
 * md_client API for semi-blocking communication with mgmtd.  The
 * mdc_wrapper API automates common procedures for connecting to
 * mgmtd, and provides the "management message handler" callback that
 * the md_client API requires.  It requires libevent because the
 * management message handler needs to be able to reenter the event
 * loop while waiting for the reply to the request sent to mgmtd.
 *
 * Normally libmdc_wrapper will handle initializing the GCL and
 * establishing a connection to mgmtd.  But alternately, you can
 * provide your own GCL handle and GCL session, and it will skip that
 * part of initialization, and just perform its other functions.
 *
 * The basic procedure for using this library is:
 *
 *   - Call mdc_wrapper_params_get_defaults() to get a mdc_wrapper_params
 *     data structure, and fill it out as desired.
 *
 *   - Call mdc_wrapper_init().  This initiates a connection to mgmtd
 *     (unless you have provided a GCL handle and GCL session), initializes 
 *     the md_client library, and returns an ::md_client_context.
 *
 *   - Use this md_client context as normal with the rest of the md_client 
 *     API.  The management message handler provided by the wrapper will 
 *     be used to send messages and receive replies.
 *
 *   - When you decide to exit, or break your mgmtd connection for some
 *     reason, call mdc_wrapper_disconnect().  If libmdc_wrapper opened
 *     the mgmtd connection for you, it will now be broken; but if you
 *     supplied a GCL session, it will not be disturbed.  In any case,
 *     this will cancel all outstanding requests, and remove all events
 *     registered by this library, so you can fall out of your main event
 *     loop.  Then call mdc_wrapper_deinit() from your final 
 *     deinitialization.  Do not call mdc_wrapper_deinit() while still in 
 *     the event loop, as it will free data structures that may still be 
 *     referenced.
 *
 *   - All of the md_client API is open to you, except for mdc_init() and
 *     mdc_deinit(), which are wrapped by this library and should not be
 *     called by you.
 *
 * The mangement message handler provided by this library does handle
 * multiple outstanding requests.  This is in contrast to the handlers
 * implemented by many of Samara's infrastructure daemons before the
 * advent of libmdc_wrapper, which would log an error and return
 * failure if someone tried to send a request while another was
 * already outstanding.  However, there are some inherent pitfalls with
 * multiple outstanding requests that you should be aware of.
 *
 * Md_client is a blocking API, meaning that its calls to make
 * requests do not return until the response has been received.  The
 * management message handler in this wrapper API reenters the event
 * loop while waiting for the reply, both to watch for activity on the
 * GCL fd itself to receive the reply, and also to continue processing
 * other events while we are waiting.  If an event occurs during this
 * time interval that triggers your code to send another request message,
 * there will be multiple outstanding requests, and we will reenter
 * the event loop again.  The tricky part is that the replies can only
 * be returned in a LIFO manner.  For example, the following could be
 * the call stack with two requests outstanding:
 * <pre>
 *   9. event_dispatch_once()
 *   8. mdc_wrapper mgmt message handler
 *   7. mdc API to send mgmt message (request #2)
 *   6. your event handler (event #2)
 *   5. event_dispatch_once() (dispatch events one at a time until a
 *      reply is received)
 *   4. mdc_wrapper mgmt message handler
 *   3. mdc API to send mgmt message (request #1)
 *   2. your event handler (event #1)
 *   1. event_dispatch()
 *   0. main()</pre>
 *
 * Because of the nature of the C call stack, we must return the reply
 * to request #2 before we return the reply to request #1.  So if the
 * reply to request #1 arrives first, we save it in a buffer.  When
 * the reply to request #2 arrives, we return to the caller.  Back in
 * frame 4, after processing the event that triggered us to send
 * request #2, we notice that a reply to request #1 is now available, and
 * we return from that too.
 *
 * This can lead to some nonintuitive behavior.  e.g. a fast-running
 * request may not return for a long time, even with mgmtd never
 * blocked, if a longer-running request is sent AFTER the first
 * request, but before the reply to the first request is received.
 * This also leaves you vulnerable to starvation, since if new
 * messages keep getting sent before the previous one receives a
 * reply, you will never be able to complete the first request and the
 * stack will continue growing, potentially ad infinitum.
 *
 * There are two ways to deal with potential starvation.  The easiest
 * is to use a timeout.  The mdc wrapper API supports a timeout (the
 * mdc_wrapper_params::mwp_timeout field) which will trigger as
 * soon as any request has been pending for the specified period of
 * time.  See the description of that field for further details.
 *
 * The other way of preventing starvation is to avoid having multiple
 * requests outstanding.  When you receive an event that might trigger
 * you to send another message, save it and do not process it until
 * you have received a reply to your first message.  You would not
 * need to do this with events that would never send GCL requests,
 * such as monitoring node handlers that only need to send a reply:
 * those should still be handled immediately.  If such an event is
 * tied directly to a specific libevent event (such as waiting for
 * activity on stdin, for an interactive console application) you
 * could just remove that event from libevent while waiting for the
 * response, using mwp_request_start_func to remove it, and
 * mwp_request_complete_func to re-add it.  If the event shares a
 * libevent event with other non-backloggable events (such as the GCL
 * fd, which can often produce either kind of message, and in any case
 * must remain registered to receive the reply to the outstanding
 * request), you must receive the message yourself immediately, and
 * use a backlogging mechanism.  mdc_backlog.h implements one for you.
 * 
 * Please note also that the management message handler provided by
 * this library does NOT wait for event responses by default.
 * (This can be overridden by setting mwp_synchronous_events option).
 * In the default behavior, md_client calls to send event messages
 * will return immediately, as soon as the message is turned over to
 * the GCL.  This may not even mean the request has been sent yet!
 * Please see mdc_wrapper_params::mwp_request_complete_func for more
 * on this.  Also note that if you are using this library, you should
 * only send event messages with the mdc API, since otherwise our
 * tracking of the number of outstanding events will be wrong (if you
 * send one directly, we will still be notified of the responses, but
 * will not have known about the request).
 *
 * If you set mwp_synchronous_events, then event requests and
 * responses are treated just like other request types.
 *
 * (This API is not part of libgcl because from it requires that the
 * caller link with libevent, and there are clients of libgcl that do
 * not use libevent whom we do not want to disturb.)
 */

/*
 * Managment daemon clients not yet converted to use this wrapper,
 * and why not:
 *
 * Should convert:
 *   - CLI (more complex case since libcli manages md_client; also 
 *     need to re-form mgmtd connection rather than just exiting)
 *   - Sched (also due to libcli)
 *   - RGP (also due to libcli, plus RGP had its own md_client abstraction)
 *   - rendv_client (shares GCL handle with libremgcl session)
 *   - PM (needs to re-form mgmtd connection, not just exit)
 *   - Libweb (has custom mgmt message handler that adds binding to request)
 *
 * Probably won't convert:
 *   - SNMP (does not use libevent)
 *   - Wsmd (uses libevent directly, not libevent wrapper)
 *   - Clustering (???)
 */

#include "md_client.h"

/**
 * A function to be called by the library either before sending a
 * request message, or after completing a request (usually by
 * receiving a response, but this could also be because we gave up on
 * the request before getting a response).
 *
 * \param mcc Pointer to md_client library context with which the request
 * is being made.
 *
 * \param request The request message which is either about to be sent,
 * or which we have just completed.  (Exception: if this is being called
 * for an event response, and mwp_synchronous_events is not set, then 
 * we no longer have the original request, so this will be NULL.)
 * The function may modify the request if it wants;
 * e.g. the Web request handler could add a cookie to an outgoing request.
 * There is not much point in modifying a request on the back end (when
 * the response has been received, and we're wrapping up the exchange).
 *
 * \param request_complete If true, this means the request specified
 * has completed (either by getting a response or being cancelled),
 * and we are being called as
 * mdc_wrapper_params::mwp_request_complete_func.  If false, this
 * means the request specified is about to be sent, and we are being
 * called as mdc_wrapper_params::mwp_request_start_func.
 *
 * \param response If request_complete is false, this will be NULL.
 * If request_complete is true, this will contain the response we received,
 * if any.  If the request was cancelled, it will still be NULL.
 * The function may modify the response if it wants to.
 *
 * \param request_idx The index number of this request on the request
 * stack.  In other words, this is the number of other requests that
 * were already outstanding before this request was begun.  This is
 * mainly useful to detect whether a request is the first one (i.e.
 * request_idx == 0), in case you need to add or remove events from
 * libevent.  NOTE: this will be -1 in the case of an event response,
 * since we do not correlate event responses with event requests, we
 * do not know which request it went with.
 *
 * \param arg The argument specified in either
 * mdc_wrapper_params::mwp_request_start_data or
 * mdc_wrapper_params::mwp_request_complete_data, depending on why
 * this function is being called.
 *
 * XXXX/EMT: the Web UI can now be migrated to use libmdc_wrapper,
 * since the cookie can be added with a request start func.
 */
typedef int (*mdc_wrapper_request_func)
    (md_client_context *mcc, bn_request *request, 
     tbool request_complete, bn_response *response,
     int32 request_idx, void *arg);


/**
 * Structure to be passed to mdc_wrapper_init() to initialize it.  Don't
 * allocate your own copy; call mdc_wrapper_params_get_defaults() to
 * allocate and prepopulate one for you.
 */
typedef struct mdc_wrapper_params {
    /**
     * MANDATORY: libevent wrapper context to use to manage GCL events.
     */
    lew_context *mwp_lew_context;

    /**
     * @name To initialize your own GCL handle and session
     */
    /*@{*/

    /**
     * Should we initialize the GCL?  Defaults to 'true'.  If 'false',
     * the 'mwp_gcl_handle' field is mandatory.
     */
    tbool mwp_init_gcl;

    /**
     * If mwp_init_gcl is 'false', this is the GCL handle we should use.
     */
    gcl_handle *mwp_gcl_handle;

    /** 
     * Should we open a session with a provider?  Defaults to 'true',
     * and in this state, the 'mwp_gcl_client' field is mandatory.
     * If 'false', the 'mwp_gcl_session' field is mandatory instead.
     *
     * Note: this may not be 'false' if mwp_init_gcl is 'true'.
     */
    tbool mwp_open_session;

    /**
     * If mwp_open_session is 'false', this is the GCL session we
     * should use.
     */ 
    gcl_session *mwp_gcl_session;

    /*@}*/

    /**
     * @name To have the library initialize the GCL for you
     */

    /*@{*/

    /**
     * GCL client name (e.g.\ gcl_client_cli in gcl.h).  This is
     * mandatory if mwp_open_session is 'true'; and ignored if it
     * is 'false'.
     */
    const char *mwp_gcl_client;

    /**
     * Option flags for the client session.
     *
     * This field is rarely used.  If used at all, it would probably
     * be for ::gof_not_provider.
     */
    gs_option_flags_bf mwp_gcl_client_options;

    /**
     * Username to use when opening the session.  This defaults to NULL,
     * and should generally be left that way for noninteractive processes.
     * This should only be set by a process directly representing a user.
     */
    const char *mwp_gcl_username;

    /**
     * Provider to which to connect.  Defaults to mgmtd
     * (gcl_provider_mgmtd).  This is ignored if mwp_open_session
     * is 'false'.
     */
    const char *mwp_provider;

    /**
     * Primary service to open.  Defaults to the management service
     * (bap_service_mgmt).  This is ignored if mwp_open_session is
     * 'false'.
     *
     * By "primary" service, we mean the one which will be opened
     * last, and thus will be the default chosen for any messages
     * which do not specify a service using
     * bn_request_set_service_id().
     *
     * Callers which want to open more than one service can specify
     * the remainder of them in mwp_other_services.
     */
    bap_service_id mwp_service;

    /**
     * Other services to open.  Each element should be a member of
     * bap_service_id.  This array comes pre-allocated (empty) from
     * mdc_wrapper_params_get_defaults().
     *
     * This is ignored if mwp_open_session is 'false'.
     */
    uint32_array *mwp_other_services;

    /**
     * Optional GCL handle ID.  This is filled in for ggo_handle_id in
     * the gcl_global_options structure we use for gcl_init().  This
     * string will be prepended to every GCL log message pertaining
     * to this session.
     */
    const char *mwp_gcl_handle_id;

    /*@}*/

    /**
     * Timeout on how long any request can run for before returning to
     * the caller.
     *
     * The semantics of this timeout are unusual in that the timeout
     * on one message can affect the processing of other messages that
     * have not been outstanding for as long as the timeout specifies.
     * When multiple request messages are outstanding, the timeout
     * will always fire on the one at the bottom of the stack,
     * i.e. the one that was sent first.  But in order to return to
     * its caller, all subsequent requests must be cancelled, even if
     * they were not outstanding for very long at all.
     *
     * When the timeout fires, we mark all outstanding requests
     * "complete" whether a response has been received yet or not.
     * And until we empty the stack entirely (i.e. return a response
     * to the caller of the first request), any subsequent requests we
     * receive will be immediately marked complete and answered
     * emptyhanded.
     * 
     * This mechanism attempts to limit how long we can be starved by
     * additional requests coming in before we can respond to the old
     * ones.  Note that it is beyond our control to return immediately
     * to the caller we timed out for, since the event handlers that
     * made subsequent requests must still finish running, and the
     * only thing we can do to speed them up is to fail their existing
     * requests, and refuse to initiate any new requests.
     *
     * Some of the requests pending may already have received a
     * response (which we could not return yet due to the C stack),
     * and those will be answered normally.  Those that did not
     * receive a response yet will get back a NULL response, as well
     * as lc_err_io_timeout in the return value.
     *
     * The timeout does not apply to event requests, as those return
     * immediately without awaiting the response.  (Exception: if
     * mwp_synchronous_events is set, they behave like other requests)
     *
     * The default is zero, which indicates no timeout.
     */
    lt_dur_ms mwp_timeout;

    /**
     * Maximum number of outstanding requests, or -1 for no limit.
     * Pathologically bad cases, where a new request continually gets
     * sent out before we receive the response to all of the previous
     * ones, can lead to stack overflow, as we will add a few more 
     * stack frames for every new request.  Setting a maximum here
     * limits this case, and will start failing requests if there
     * are too many other requests already outstanding.
     *
     * This solves a different problem than the timeout.  Even with a
     * timeout, this problem could occur if the requests came very
     * quickly.  And even with a maximum, we still need a timeout to
     * prevent starvation, if we simply end up hovering around the
     * maximum forever.
     *
     * The default is 2000;
     */
    int32 mwp_max_outstanding_reqs;
    
    /**
     * @name GCL request callbacks
     *
     * This API handles response messages automatically, but still 
     * passes request messages directly through to your callbacks.
     * Configure those callbacks here.
     */
    /*@{*/
    bn_msg_request_callback_func mwp_query_request_func;
    void *mwp_query_request_data;
    bn_msg_request_callback_func mwp_set_request_func;
    void *mwp_set_request_data;
    bn_msg_request_callback_func mwp_action_request_func;
    void *mwp_action_request_data;
    bn_msg_request_callback_func mwp_event_request_func;
    void *mwp_event_request_data;
    /*@}*/

    /**
     * @name GCL response callbacks
     *
     * Handlers to be called when we receive a response we do not
     * recognize (by its request ID).  Typically this will happen when
     * a caller calls bn_request_msg_send() directly, because they
     * want asynchronous message handling.
     *
     * If a caller does not plan to send any messages directly,
     * handlers need not be provided here.
     *
     * Note: unless mwp_synchronous_events is set, the case of event
     * responses is special, because even our handling of them is
     * asynchronous, and we do not track request IDs of the responses
     * for which we are awaiting a reply.  If an event response
     * callback is provided here, it will be called for EVERY event
     * response received, whether the original event request was sent
     * through the md_client API or not.  If mwp_synchronous_events is 
     * set, then this paragraph does not apply.
     */
    /*@{*/
    bn_msg_response_callback_func mwp_query_response_func;
    void *mwp_query_response_data;
    bn_msg_response_callback_func mwp_set_response_func;
    void *mwp_set_response_data;
    bn_msg_response_callback_func mwp_action_response_func;
    void *mwp_action_response_data;
    bn_msg_response_callback_func mwp_event_response_func;
    void *mwp_event_response_data;
    /*@}*/

    /**
     * @name Other runtime callbacks
     */
    /*@{*/

    /**
     * Function to be called right before we send each request to
     * mgmtd.  One possible application of this would be to unregister
     * some events from libevent if you do not want to have to process
     * them while waiting for the response.  e.g. a console
     * application might unregister STDIN_FILENO so it doesn't get
     * distracted by further user input.  It could still leave the
     * SIGINT signal registered so a Ctrl+C could still interrupt the
     * transaction.
     */
    mdc_wrapper_request_func mwp_request_start_func;

    /** Data to be passed to mwp_request_start_func whenever it is called */
    void *mwp_request_start_data;

    /**
     * Function to be called just as we are "completing" a request.
     *
     * In the case of query, set, and action requests, which we
     * process synchronously, this would be right before we return the
     * response to the caller (which might be nothing, if the request
     * was cancelled before a response was received).
     *
     * Note that the library automatically logs a warning if an error
     * response is received, so you do not have to register a custom
     * callback just to do that.  (XXX/EMT: could offer a flag that
     * prevents library from doing this)
     *
     * The following two points are true by default, but NOT if the
     * mwp_synchronous_events option is set:
     * 
     *   - In the case of an event request, this would be as soon as the
     *     event response is received.  This will often be after the call
     *     making the request has returned, but not always.  In some cases,
     *     the GCL may send the event and process the response all before 
     *     the request handler has a chance to return.  Note that since we
     *     do not save event requests we have sent, the request will not be
     *     passed to this function (unlike with the other three kinds of 
     *     requests).
     *
     *   - Normally the sender of an event does not care about event
     *     responses, but clients who might send events near to shutdown
     *     time should note that the functions that send events do not
     *     guarantee that the event has even been sent (let alone
     *     acknowledged) by the time they return.  If the socket buffers
     *     are already full, it might just queue it up in a GCL buffer, so
     *     the event response is the only way you can be certain that an
     *     event has been sent and properly received.  So often a client's
     *     exit handler will call mdc_wrapper_get_num_outstanding_events()
     *     to see if there are any events to which we have not received
     *     replies, and if so, leave it to this event response function to
     *     continue exiting when the last expected event response is
     *     received (possibly subject to a timeout, which would be
     *     registered as a separate libevent timer).
     *
     * A common use for this callback in the case of non-event
     * messages is to process the return code and message in some
     * centralized way, rather than relying on the caller who made the
     * request to do so.  For example, the CLI and Wizard
     * infrastructures automatically print any response message or 
     * error code they get back, rather than requiring the modules to
     * do this.
     */
    mdc_wrapper_request_func mwp_request_complete_func;

    /** Data to be passed to mwp_request_complete_func whenever it is called */
    void *mwp_request_complete_data;

    /**
     * Function to be called to handle supplemental user-visible
     * messages.  See ::mdc_user_msg_handler_func_ptr for details.
     */
    mdc_user_msg_handler_func_ptr mwp_user_msg_handler_func;

    /** Data to be passed to mwp_user_msg_handler_func whenever it is called */
    void *mwp_user_msg_handler_data;

    /**
     * Function to call when the mgmtd session is closed (whether we
     * closed it ourselves or not).  Only heeded if mwp_init_gcl
     * is 'true'.
     */
    fdic_event_callback_func mwp_session_close_func;

    /** Data to be passed to mwp_session_close_func whenever it is called */
    void *mwp_session_close_data;

    /*@}*/

    /** Should we log a warning if an error response code is received? */
    tbool mwp_log_error_response;

    /**
     * Should we treat event requests the same as other requests; i.e.
     * send them synchronously, track outstanding requests, and wait
     * for their responses?
     */
    tbool mwp_synchronous_events;

} mdc_wrapper_params;

/**
 * Allocate a new mdc_wrapper_params structure and populate it with defaults.
 */
int mdc_wrapper_params_get_defaults(mdc_wrapper_params **ret_params);

/**
 * Free the contents of an mdc_wrapper_params structure, and the structure
 * itself.  This does NOT include the ::mwp_lew_context field, which
 * is not owned by this structure, nor does it attempt to free any
 * of the 'void *' fields that are passed back to callback functions.
 * You should free any of those that require it before calling this.
 */
int mdc_wrapper_params_free(mdc_wrapper_params **inout_params);

/**
 * Connect to mgmtd and initialize the md_client library.
 */
int mdc_wrapper_init(const mdc_wrapper_params *params, 
                     md_client_context **ret_mcc);

/**
 * Disconnect from mgmtd, but do not free all of our structures yet.
 * This is decoupled from complete deinitialization because often
 * a caller will decide it wants to disconnect from within an event
 * handler called from the GCL.  So it can close the session, but
 * cannot free all of the structures yet until it falls out of the 
 * event loop.
 *
 * \param mcc Context pointer.
 *
 * \param flush_output Specify whether we should flush our output on
 * the session before closing it.  'false' guarantees that we finish
 * in a non-blocking manner, but any pending messages will be dropped.
 * 'true' may block, and ensures that outgoing messages are sent.
 * NOTE, however, that is still does NOT ensure that mgmtd will 
 * receive those messages.  The only safe way to ensure that is to send
 * the messages synchronously (wait for the response).
 */
int mdc_wrapper_disconnect(md_client_context *mcc, tbool flush_output);

/**
 * Fully deinitialize the md_client library and free all structures.
 * This will implicitly do mdc_wrapper_disconnect() if we are not already
 * disconnected.
 */
int mdc_wrapper_deinit(md_client_context **inout_mcc);

/**
 * Cancel all current outstanding requests, and continue to reject any
 * new requests that come in until we have returned from the
 * bottommost call into the md_client API (i.e. until there are no
 * more pending requests in our queue).  This is the same thing that
 * happens when the timeout expires, except that callers whose
 * requests do not receive a reply get back lc_err_cancelled instead
 * of lc_err_io_timeout.
 *
 * \param mcc Context pointer.
 *
 * \param quiet Should we be quiet while cancelling out of these
 * pending requests, or log errors?  Specifically, this controls
 * whether we return lc_err_cancelled (quiet) or
 * lc_err_cancelled_error (not quiet) from the calls into the 
 * md_client API.
 */
int mdc_wrapper_cancel_all(md_client_context *mcc, tbool quiet);

/**
 * Tell how many requests to mgmtd are currently outstanding.
 * Unless mwp_synchronous_events is set, this number will NOT include
 * outstanding events, as we will not be tracking them individually.
 * That is, how many requests for which we are actually awaiting a
 * response.
 *
 * NOTE: this function will only work if you initialized the md_client
 * library with mdc_wrapper_init().  If you initialized with mdc_init(), it
 * will fail because it is your own mdc_msg_handler_func_ptr function
 * managing the outstanding requests, so the library does not know
 * what's outstanding.
 */
int mdc_wrapper_get_num_outstanding_requests(md_client_context *mcc,
                                             int32 *ret_num_reqs);

/**
 * Tell how many event requests we have sent without receiving a
 * corresponding response.  Note that if mwp_synchronous_events is
 * set, this will always be zero, because the outstanding events
 * will be accounted for instead in the return value of 
 * mdc_wrapper_get_num_outstanding_requests(), along with all other
 * request types.
 *
 * This may be particularly useful when you are preparing to exit, and
 * want to make sure that all event messages requested to be sent have
 * actually gone out, and been acknowledged.  If they have not yet,
 * you may want to have registered a callback function for
 * mwp_request_complete_func, and not exit until it gets called for
 * the last event you sent.
 *
 * NOTE: this function will only work if you initialized the md_client
 * library with mdc_wrapper_init().  If you initialized with mdc_init(),
 * it will fail because it is your own mdc_msg_handler_func_ptr
 * function managing the outstanding requests, so the library does not
 * know what's outstanding.
 */
int mdc_wrapper_get_num_outstanding_events(md_client_context *mcc,
                                           int32 *ret_num_events);

/**
 * Get a pointer to the GCL session associated with an md_client_context.
 *
 * NOTE: this function will only work if you initialized the md_client
 * library with mdc_wrapper_init().
 */
gcl_session *mdc_wrapper_get_gcl_session(md_client_context *mcc);

/**
 * Get a pointer to the fdic_handle associated with an md_client_context.
 *
 * NOTE: this function will only work if you initialized the md_client
 * library with mdc_wrapper_init(), and mwp_init_gcl was 'true' (which
 * is the default).
 */
int mdc_wrapper_get_fdic_handle(md_client_context *mcc,
                                fdic_handle **ret_fdich);

#ifdef __cplusplus
}
#endif

#endif /* __MDC_WRAPPER_H_ */
