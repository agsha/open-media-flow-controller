/*
 *
 * mdc_backlog.h
 *
 *
 *
 */

#ifndef __MDC_BACKLOG_H_
#define __MDC_BACKLOG_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "gcl.h"
#include "bnode.h"
#include "bnode_proto.h"
#include "fdintrcb.h"

/**
 * \file mdc_backlog.h API for managing a backlog of pending events.
 * \ingroup gcl
 *
 * The general idea of this API is that among the events an application
 * may receive, some of them may not be convenient to handle immediately.
 * You may wish to backlog these events, and then handle them at a later
 * time.  Usually this comes up because you're fine handling an event when
 * it occurs in your OUTER dispatch loop; however, often an application
 * will REENTER its dispatch loop after sending a mgmtd request, to await
 * its reply.  So if something happens in the INNER dispatch loop, and
 * processing it may interfere with the outer event already in progress,
 * you'll want to backlog it.

 * The main scenarios are:
 *
 *   - Processing the inner event may modify global variables in a way
 *     that would confuse the outer event processing in progress.
 *
 *   - Processing the inner event may send a mgmtd request, and the
 *     application does not want to have more than one mgmtd request
 *     outstanding at a time.  This was more important when mdc_backlog
 *     was originally created, since then we did not have mdc_wrapper,
 *     and the md_client API was most commonly used with a mgmtd message
 *     handler which did not tolerate multiple outstanding requests.
 *     Now mdc_wrapper does handle multiple outstanding requests, so
 *     this is a less relevant scenario.
 *
 * These triggering events would often be GCL requests received from
 * mgmtd, and the APIs below try to make this common case easy.  But they
 * may also be other kinds of events, like timers -- the APIs are general
 * enough to backlog anything (you pass it a 'void *'), and you are
 * free to ignore the gcl_session and bn_request parameters it takes.

 * The core idea is that you keep the library apprised as to whether or
 * not you are ready to handle these events, and the library makes sure
 * that you are only called to handle these events when you are ready.
 *
 * To use this library:
 *
 * <ol>
 *
 * <li>Call mdc_bl_init() before any of the other functions.
 *
 * <li>Call mdc_bl_set_ready() with \c true as soon as you are ready to
 *     start processing requests, presumably during initialization.
 *
 * <li>Whenever an event occurs (you receive a GCL request, etc.) whose
 *     handling would involve doing whatever the backlog is intended to
 *     protect (e.g. sending a mgmtd message), pass it to
 *     mdc_bl_backlog_or_handle_message_takeover().  In the callback you
 *     pass to this function, handle the event normally, with the assumption
 *     that it is safe to do so (e.g. no other messages are outstanding).
 *
 * <li>Maintain the API's idea of whether or not you are ready.  If you are
 *     protecting against multiple outstanding requests, this would mean 
 *     that in your md_client mgmt message handler, call mdc_bl_set_ready()
 *     with \c false before reentering the event loop, and then
 *     call it back with \c true after receiving the reply and before
 *     returning.  This will ensure that you do not get called back to
 *     handle a backlogged request while another message is outstanding.
 *
 * <li>Call mdc_bl_deinit() when you're shutting down.  This is 
 *     particularly important if you are using libevent, because otherwise,
 *     the backlog mechanism's fd will still be installed in the watch list,
 *     and libevent's dispatch function won't return so you can fall out of
 *     the main loop and exit.
 *
 * </ol>
 */

/**
 * Opaque handle to track context of backlog mechanism.
 * Client gets one and just passes it back to other APIs.
 */
typedef struct mdc_bl_context mdc_bl_context;

/**
 * Initialize backlog queue.  Your initial status will be assumed to be
 * "not ready", so you will have to call mdc_bl_set_ready() when you
 * become ready.
 *
 * We need an fdic_handle (which you can get from fdic_libevent_init()
 * and fdic_init() if you are using libevent) because we create a pipe
 * and use it to wake ourselves up when it is time to process events
 * from the backlog queue.
 */
int mdc_bl_init(fdic_handle *fdich, mdc_bl_context **ret_backlog_context);

/**
 * Shut down the backlog queue: stage 1 of deinitialization.  Call this
 * when you are ready to start exiting; it removes the pipe from the 
 * fdic mechanism, so it will not generate any new events.
 *
 * This does NOT free any data structures, as it is not safe to do so
 * this early: there may already be pending events from the fd that
 * will still get processed before we fall out of the event loop.
 */
int mdc_bl_shutdown(mdc_bl_context *backlog_context);

/**
 * Destroy a backlog queue: stage 2 of deinitialization.  Do this
 * after calling mdc_bl_shutdown(), and after you have completely
 * fallen out of your event loop.  It frees all data structures 
 * associated with the backlog queue.
 */
int mdc_bl_deinit(mdc_bl_context **inout_backlog_context);

/**
 * Tell the backlogging API whether or not you are ready to process
 * backlogged messages.  When the API is initialized, it is assumed
 * that you are not ready, so call this once with 'true' when you
 * are, and subsequently whenever this status changes.
 *
 * For example, if you only want to have a single message oustanding
 * at a time, you might call this with 'false' from your mgmt message
 * handler (the one passed to mdc_init()) right before you send a
 * message; and again with 'true' when you receive the response from
 * that message.  Your callback function to process a backlogged
 * message will only be called when the last call to this function
 * was with 'true'.
 */
int mdc_bl_set_ready(mdc_bl_context *backlog_context, tbool ready);

/**
 * Add a new event to the backlog queue.  When it comes time to
 * process the backlogged event, the callback_func will be called with
 * the session pointer, the bn_request, and the callback_arg.  This
 * will happen as soon as both (a) the program returns to or reenters
 * its event loop, and is able to detect readability on file
 * descriptors, and call back event handlers; and (b) the last call to
 * mdc_bl_set_ready() was with 'true'.
 *
 * Note that the gcl_session and bn_request fields are geared
 * specifically towards the scenario of backlogging GCL requests,
 * although not every backlogged event has to be a GCL request.
 * To keep the API "pure", it should just have had the 'void *'
 * argument, but these extra arguments try to make it more convenient
 * for the caller, so they don't have to have a wrapper structure.
 * For events that are not GCL requests, these can both be NULL.
 * Similarly, the callback function must take a gcl_session and 
 * bn_request, but it may just expect NULLs and ignore them.
 *
 * \param backlog_context Backlog context pointer returned from
 * mdc_bl_init().
 *
 * \param sess GCL session pointer (optional).
 *
 * \param inout_req GCL request (optional).  If you do pass a request here,
 * you pass a pointer to a pointer, and this function takes over ownership
 * of the request.  Ownership is then passed to your callback when it is
 * called, so it should free the request when it is done.
 *
 * \param callback_func Function to call back when it is time to process
 * the event.
 *
 * \param callback_arg Argument to pass to callback_func.
 */
int mdc_bl_backlog_message_takeover(mdc_bl_context *backlog_context, 
                                    gcl_session *sess,
                                    bn_request **inout_req,
                                    bn_msg_request_callback_func callback_func,
                                    void *callback_arg);

/**
 * If the current ready state as per the last call to
 * mdc_bl_set_ready() is 'false', then pass through to
 * mdc_bl_backlog_message_takeover().  Otherwise, pass through to the
 * callback function immediately.  This is just a small convenience
 * wrapper.  Again, note that 'sess' and 'inout_req' are optional.
 *
 * \retval ret_handled Set to true if the message is passed on to the 
 * callback function, and false if the message is backlogged.
 */
int mdc_bl_backlog_or_handle_message_takeover(mdc_bl_context *backlog_context,
                                              gcl_session *sess, 
                                              bn_request **inout_req,
                                              bn_msg_request_callback_func 
                                              callback_func,
                                              void *callback_arg,
                                              tbool *ret_handled);

#ifdef __cplusplus
}
#endif

#endif /* __MDC_BACKLOG_H_ */
