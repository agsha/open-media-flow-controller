/*
 *
 * libevent_wrapper.h
 *
 *
 *
 */

#ifndef __LIBEVENT_WRAPPER_H_
#define __LIBEVENT_WRAPPER_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "ttime.h"
#include "event_external.h"

/** \defgroup libevent LibEvent: event processing library */

/* ------------------------------------------------------------------------- */
/** \file libevent_wrapper.h Wrapper functions for libevent.
 * \ingroup libevent
 *
 * The advantages to using this wrapper vs. using libevent directly are:
 *
 *   - It exposes a less error-prone API.  For example:
 *       - With libevent you may not call event_set() on an event that
 *         is still pending without removing it first, and it does not
 *         catch attempts to do so; they just corrupt its data structures.
 *         The wrapper API takes care of this automatically.
 *       - Libevent adds your timer length onto the current time to
 *         get the expiration time of the event.  It does not catch it
 *         if this overflows and results in a time in the past.
 *         The wrapper API clamps the timer length at the maximum 
 *         possible without overflowing.
 *
 *   - It keeps track of all events registered.  This can:
 *       - Facilitate switching "event contexts" easily, for instance
 *         if you need to call libevent reentrantly with some events
 *         disabled.
 *       - Make it easy to unregister all events at once for when you
 *         are ready to exit.
 *
 *   - It shortens most common tasks, making code more expressive.
 *     For example, to reschedule an event with a timer, it 
 *     previously took five calls, to event_pending(), event_del(),
 *     event_set(), lt_time_ms_to_timeval(), and event_add(); these
 *     are replaced with a single call to the wrapper API.
 *
 *   - It works with the native time types from ttime.h, rather than
 *     with struct timevals.
 *
 *   - All of its function and type names start with a common prefix,
 *     making it easier to search for in your source code.
 *
 *   - It contains precautions to work around bug 11816, which affects
 *     timers set around the time of an abrupt change in system time.
 *
 * Usage notes for libevent wrapper:
 *
 *   - The caller must hold pointers to lew_event structures.  They must 
 *     start out NULL.  A pointer to one of these pointers is passed to
 *     the lew_event_reg...() functions.  The first time it is passed,
 *     an event structure is allocated and stored there.  Subsequently,
 *     the existing event structure is reused.  When the event is
 *     canceled, the structure is preserved, but when the event is
 *     deleted, the structures is deallocated and the caller's own
 *     pointer set to NULL, since the wrapper keeps pointers to
 *     pointers.
 *
 *   - An event may be persistent or not.  Not being persistent means that
 *     it remains registered until it is triggered once; otherwise it
 *     remains registered until explicitly cancelled.  Signal events
 *     are always persistent, timers are never persistent, and 
 *     file descriptor events may or may not be persistent according to
 *     a parameter passed in when they are registered.  Note that even
 *     though a signal or file descriptor event may be persistent, the
 *     timer does not automatically reset itself.  If the timer for a 
 *     persistent event goes off, the event will remain registered, but
 *     without a timer.  Note that when a nonpersistent event is
 *     triggered and not renewed, its structure is not deallocated
 *     automatically.
 *
 *   - An event pointer may be in one of three states.  The wrapper
 *     correctly detects which state it is in, and handles it 
 *     accordingly when the client tries to set an event using it
 *     with one of the lew_event_reg...() functions.
 *       - Unallocated.  Pointer is NULL.  It should be in this case
 *         initially, and can return to this case if an event is 
 *         explicitly freed.  Wrapper allocates an event structure,
 *         zeros it out, calls event_set() and event_add().
 *       - Allocated and pending.  Wrapper detects this with
 *         event_pending(), then calls event_del(), event_set(),
 *         and event_add().
 *       - Allocated and not pending.  An event structure can get to
 *         this state by not being persistent, and then firing once.
 *         Wrapper simply calls event_set() and event_add().
 *
 *   - The event handler functions see the same interface as before,
 *     with one exception: they are passed a lew_context instead of
 *     an event_context.  This is because programs using the libevent
 *     wrapper need a libevent context to do anything else with events
 *     in the same context.  This is not as crucial to single-threaded
 *     programs, which can always just use a global to hold the context,
 *     but could be useful to multithreaded ones.  In order to change
 *     the interface this way, the libevent wrapper does contain an
 *     intermediary event notification callback between libevent and
 *     the callback provided by the caller.
 *     Their first parameter is the fd number, the signal number, or
 *     zero depending on why the callback was called.  Their second
 *     parameter is one of EV_READ, EV_WRITE, EV_SIGNAL, or
 *     EV_TIMEOUT, to indicate why it was called.  The third parameter
 *     is the same opaque data that was provided when the event was
 *     registered.  And the fourth parameter is to the libevent
 *     wrapper context as previously mentioned.
 *
 *   - Signal handlers are not installed until you first enter your 
 *     event loop (with lew_dispatch() or a variant thereof).
 *     So you will miss any signals that occur between when you
 *     register to handle them and when you enter the event loop.
 *     This is not a function of the wrapper; it is true of raw libevent
 *     as well.  See bug 10804.
 *
 *   - If multiple signals of the same type are received very close 
 *     together, your handler function will be called at least once,
 *     but not necessarily as many times as the signal was received.
 *     This is not a function of the wrapper; it is true of raw 
 *     libevent as well.  It is largely unavoidable, as the kernel also
 *     does some consolidation of signals, though not as much as
 *     libevent does.
 */

/*
 * XXX/EMT: when events are registered, allow them to be tagged with
 * strings, so we can get a more meaningful listing of what events are
 * registered at a given time.
 */

/*
 * XXX/EMT: should allow the registration of cleanup functions for the
 * 'data' attached to an event.  This may be dynamically allocated.
 * It's easy enough to free it if the timer fires, but if the timer
 * is cancelled before it fires, it's too easy to forget to free it,
 * causing a memory leak.  For now, the caller must call
 * lew_event_get_data() clean it up before deleting the event.
 */

/* ------------------------------------------------------------------------- */
/* Type definitions
 *
 * Note that some of these mirror definitions in event.h and need to
 * be kept in sync.  This is because we don't want anyone else to
 * include event.h, but they need access to a limited set of
 * definitions so they know how to interpret the arguments passed to
 * their callbacks, and so we can declare prototypes of our functions.
 */

typedef struct lew_context lew_context;
typedef struct lew_event lew_event;

typedef int (*lew_event_handler)(int fd, short event_type, void *data,
                                 lew_context *ctxt);

#ifndef EV_TIMEOUT
typedef enum {
    EV_TIMEOUT =        0x01,
    EV_READ =           0x02,
    EV_WRITE =          0x04,
    EV_SIGNAL =         0x08
} lew_event_type;
#else
typedef uint32 lew_event_type;
#endif


/* ------------------------------------------------------------------------- */
/** Initialize libevent and the libevent wrapper.
 *
 * \retval ret_context A context pointer for the libevent wrapper that
 * must be passed back to all subsequent wrapper calls.
 *
 * NOTE: the context is intended to later support cases where the
 * caller may want to reenter the event loop with a different context,
 * and have the wrapper switch between them automatically.
 */
int lew_init(lew_context **ret_context);


/* ------------------------------------------------------------------------- */
/** Get a pointer to the native context pointer used by libevent.
 * Generally the caller should not be doing anything with this, but it
 * is needed for fdic_libevent_init(), which uses libevent natively.
 */
int lew_get_event_context(lew_context *context,
                          event_context **ret_event_context);


/* ------------------------------------------------------------------------- */
/** Backward-compatibility wrapper for lew_event_reg_fd_full(),
 * passing NULL for the event_name and event_handler_name; and -1 for
 * the timer, indicating no timer.
 */
int lew_event_reg_fd(lew_context *context,
                     lew_event **inout_event, int fd, 
                     lew_event_type event_type, tbool persist,
                     lew_event_handler l_event_handler, void *data);


/* ------------------------------------------------------------------------- */
/** Backward compatibility wrapper for lew_event_reg_fd_full(),
 * passing NULL for the event_name and event_handler_name.
 */
int lew_event_reg_fd_timeout(lew_context *context, 
                             lew_event **inout_event, int fd, 
                             lew_event_type event_type, tbool persist,
                             lew_event_handler l_event_handler, void *data,
                             lt_dur_ms timer_length);


/* ------------------------------------------------------------------------- */
/** Register to be called back when activity on a file descriptor is
 * seen.  You can also optionally attach a timer to this event.
 * If the timer expires without any activity being seen on the fd,
 * the handler is called anyway.
 *
 * \param context The libevent wrapper context.
 *
 * \param inout_event A pointer to a pointer to an event structure to use.
 * A new structure will be allocated if it points to a NULL pointer.
 *
 * \param event_name An optional string to identify this event.
 * This is only for debugging purposes; it will show up in debug logs
 * regarding this event, but is otherwise ignored.  NULL is allowed.
 *
 * \param fd The file descriptor to listen for activity on.  It must be
 * nonnegative.
 *
 * \param event_type Specifies whether we are interested in readability,
 * writability, or both.  It can be EV_READ, EV_WRITE, or both ORed together.
 *
 * \param persist Indicates whether or not this event should remain
 * registered after it is triggered once.  Note that this only applies
 * to the triggering of the fd event -- even if 'persist' is set, 
 * after the timer goes off, the event will be cancelled until it is 
 * rescheduled.
 *
 * \param event_handler_name An optional string to identify the event
 * handler, usually exactly the C name of the function.
 *
 * \param event_handler Function to be called when this event is triggered.
 *
 * \param data Opaque data to be passed to the event handler.
 *
 * \param timer_length Number of milliseconds in the future that the
 * timer, if any, should fire.  0 means the timer will fire
 * immediately.  Pass -1 to specify no timer.  Note that this will
 * replace any timer previously registered with this event (or
 * cancel it, if -1 is passed).  Note also that even if the event is
 * persistent, the timer will only fire once until the event is
 * rescheduled; i.e. persistence only applies to the fd portion of the
 * event.  Finally, note that the timer is not automatically reset when
 * the event fires for the fd, so it cannot really be considered a 
 * "timeout" unless you reset the timer every time activity is seen on
 * the fd.
 */
int lew_event_reg_fd_full(lew_context *context, 
                          lew_event **inout_event, const char *event_name,
                          int fd, lew_event_type event_type, tbool persist,
                          const char *event_handler_name,
                          lew_event_handler event_handler, void *data,
                          lt_dur_ms timer_length);


/* ------------------------------------------------------------------------- */
/** Register to hear when a signal occurs.  This event is permanent
 * until cancelled.  If the event was previously scheduled with a
 * timer that is still active, this cancels the timer.
 *
 * \param context The libevent wrapper context.
 *
 * \param inout_event A pointer to a pointer to an event structure to use.
 * A new structure will be allocated if it points to a NULL pointer.
 *
 * \param signum The number of the signal to listen for.
 *
 * \param l_event_handler Function to be called when this event is triggered.
 *
 * \param data Opaque data to be passed to the event handler.
 */
int lew_event_reg_signal(lew_context *context,
                         lew_event **inout_event, int signum,
                         lew_event_handler l_event_handler, void *data);


/* ------------------------------------------------------------------------- */
/** Register to hear when a signal occurs.  This is the same as
 * lew_event_reg_signal() except that you can also specify a timer.
 * If the signal does not occur within the specified time interval,
 * the callback will be called anyway.
 *
 * \param context The libevent wrapper context.
 *
 * \param inout_event A pointer to a pointer to an event structure to use.
 * A new structure will be allocated if it points to a NULL pointer.
 *
 * \param signum The number of the signal to listen for.
 *
 * \param l_event_handler Function to be called when this event is triggered.
 *
 * \param data Opaque data to be passed to the event handler.
 *
 * \param timer_length Number of milliseconds before libevent should give up 
 * waiting and call the handler anyway.  Even if the event is persistent,
 * the timer will only fire once until the event is rescheduled.
 * Note that the timer is not automatically reset when the event fires
 * for the signal, so it cannot really be considered a "timeout"
 * unless you reset the timer every time the signal fires.
 */
int lew_event_reg_signal_timeout(lew_context *context, 
                                 lew_event **inout_event, int signum,
                                 lew_event_handler l_event_handler, void *data,
                                 lt_dur_ms timer_length);


/* ------------------------------------------------------------------------- */
/** Backward compatibility wrapper around lew_event_reg_timer_full().
 * passing NULL for the event_name and event_handler_name.
 */
int lew_event_reg_timer(lew_context *context, lew_event **inout_event,
                        lew_event_handler l_event_handler, void *data,
                        lt_dur_ms timer_length);


/* ------------------------------------------------------------------------- */
/** Register to be called after a specified time interval.  Note that
 * this sets a single, non-persistent timer.  If you want the timer to
 * be reset each time it goes off, reset it yourself in your event
 * handler.
 *
 * \param context The libevent wrapper context.
 *
 * \param inout_event A pointer to a pointer to an event structure to use.
 * If this points to an event structure that is already allocated, we will
 * cancel that event (if necessary), and reuse that structure.  If this
 * points to NULL, a new event structure will be allocated, and this pointer
 * updated to point to it.  If NULL is passed for this parameter, a new
 * event structure will be allocated and tracked by libevent, and automatically
 * freed after the timer goes off and its handler has returned.
 *
 * \param event_name An optional string to identify this event.
 * This is only for debugging purposes; it will show up in debug logs
 * regarding this event, but is otherwise ignored.  NULL is allowed.
 *
 * \param event_handler_name An optional string to identify the event
 * handler, usually exactly the C name of the function.
 *
 * \param event_handler Function to be called when this event is triggered.
 *
 * \param data Opaque data to be passed to the event handler.
 *
 * \param timer_length The number of milliseconds before this event
 * should be triggered.
 */
int lew_event_reg_timer_full(lew_context *context, lew_event **inout_event,
                             const char *event_name,
                             const char *event_handler_name,
                             lew_event_handler event_handler, void *data,
                             lt_dur_ms timer_length);


/* ------------------------------------------------------------------------- */
/** Returns the 'void *' data field in the event structure, which was
 * provided the last time this event was registered with one of the
 * lew_event_reg_...() functions.  This can be used to free any
 * memory owned by the pointer before deleting the event.
 *
 * NOTE: do not free the data if you are not just about to delete
 * the event, as it will leave a dangling pointer in the event
 * structure.
 *
 * \param context The libevent wrapper context.
 *
 * \param event A pointer to a pointer to an event structure to
 * use.  The event pointer will never be modified; the extra level of
 * indirection is only for consistency with the rest of the API.
 *
 * \param ret_data Returns the data field registered with the
 * specified event.
 */
int lew_event_get_data(lew_context *context, const lew_event **event,
                       void **ret_data);


/* ------------------------------------------------------------------------- */
/** Tells whether or not the specified event is still active and
 * waiting to occur.  This will always be true for signals and file
 * descriptors which were registered to be persistent; even if they
 * were registered with a timer that has gone off and not been reset,
 * the signal or fd itself is still pending.  For timer and file
 * descriptors that were registered as non-persistent, this will be
 * true only if it has not been triggered yet.
 *
 * \param context The libevent wrapper context.
 *
 * \param event The event whose status to check.  NULL may be passed;
 * this is treated as a non-pending event.  The event will not be
 * changed; the extra level of indirection is only for consistency
 * with the rest of the API.
 *
 * \retval ret_is_pending Specifies whether or not the event is 
 * pending.
 *
 * \retval ret_timer_expiration If the event has an active timer, this
 * is set to the absolute time at which the timer will expire.  If it
 * does not, this is set to zero.
 */
int lew_event_is_pending(lew_context *context, const lew_event **event, 
                         tbool *ret_is_pending,
                         lt_time_ms *ret_timer_expiration);


/* ------------------------------------------------------------------------- */
/** Reschedules a previously registered event.  In general this only
 * makes sense for non-persistent fd events without timers, since
 * persistent events will not need rescheduling, and events with
 * timers should reschedule with lew_event_reschedule_timeout().
 *
 * \param context The libevent wrapper context.
 *
 * \param event The event to reschedule.  This must be an event
 * previously scheduled with one of the lew_event_reg...() functions
 * above.
 */
int lew_event_reschedule(lew_context *context, lew_event **event);


/* ------------------------------------------------------------------------- */
/** Reschedules a previously registered event with a new timer.
 * This may be used on non-persistent events that need to be
 * rescheduled to remain active at all, or it may be used to reset the
 * timer on a persistent event.
 *
 * \param context The libevent wrapper context.
 *
 * \param event The event to reschedule.  This must be an event
 * previously scheduled with one of the lew_event_reg...() functions
 * above.
 *
 * \param timer_length The number of milliseconds before the event should
 * fire regardless of its other characteristics.  Note that if the
 * event is a signal or fd event, it may fire earlier if the fd sees
 * activity or the signal occurs.
 */
int lew_event_reschedule_timeout(lew_context *context, lew_event **event,
                                 lt_dur_ms timer_length);


/* ------------------------------------------------------------------------- */
/** Cancel a previously registered event, but leave its event
 * structure untouched.  After being cancelled the event can be
 * reinstated using lew_event_reschedule...(), or it can be
 * overwritten with one of the lew_event_reg...() functions.
 *
 * \param context The libevent wrapper context.
 *
 * \param inout_event The event to be cancelled.
 */
int lew_event_cancel(lew_context *context, lew_event **inout_event);


/* ------------------------------------------------------------------------- */
/** Eradicate an event altogether: cancel it, deallocate its
 * structure, set all pointers to it to NULL, and remove it from the
 * wrapper's internal data structures.
 * 
 * \param context The libevent wrapper context.
 *
 * \param inout_event The event to be deleted.
 */
int lew_event_delete(lew_context *context, lew_event **inout_event);


/* ------------------------------------------------------------------------- */
/** Wait for events and dispatch them as they arrive.
 * Do not return until no more events are scheduled, or until 
 * lew_escape_from_dispatch() is called.
 *
 * \param context The libevent wrapper context.
 *
 * \retval ret_no_events_left Returns \c false if there are still more events
 * in the queue, or \c true if there aren't.  May be NULL if the caller
 * does not care.
 *
 * \retval ret_escaped Returns \c true if an event handler called
 * lew_escape_from_dispatch() while processing events during this call.
 * May be NULL if the caller does not care.
 */
int lew_dispatch(lew_context *context, tbool *ret_no_events_left,
                 tbool *ret_escaped);


/* ------------------------------------------------------------------------- */
/** Wait for an event.  As soon as one or more occur, dispatch them,
 * and then return immediately without waiting for more.
 *
 * CAUTION: you are NOT guaranteed that only a single event will be
 * dispatched.  If more than one occur simultaneously, they may all be
 * dispatched.
 *
 * \param context The libevent wrapper context.
 *
 * \retval ret_no_events_left Returns \c false if there are still more events
 * in the queue, or \c true if there aren't.  May be NULL if the caller
 * does not care.
 *
 * \retval ret_escaped Returns \c true if an event handler called
 * lew_escape_from_dispatch() while processing events during this call.
 * May be NULL if the caller does not care.
 */
int lew_dispatch_once(lew_context *context, tbool *ret_no_events_left,
                      tbool *ret_escaped);


/* ------------------------------------------------------------------------- */
/** See if any events have occurred since the last check.
 * If so, dispatch them.  If not, return immediately without blocking.
 *
 * \param context The libevent wrapper context.
 *
 * \retval ret_no_events_left Returns \c false if there are still more events
 * in the queue, or \c true if there aren't.  May be NULL if the caller
 * does not care.
 *
 * \retval ret_escaped Returns \c true if an event handler called
 * lew_escape_from_dispatch() while processing events during this call.
 * May be NULL if the caller does not care.
 */
int lew_dispatch_nonblocking(lew_context *context, tbool *ret_no_events_left,
                             tbool *ret_escaped);


/* ------------------------------------------------------------------------- */
/** If called with suppress as true, we will ignore all registered
 * read FDs until further notice.  All other events -- write FDs,
 * signals, and timers (including those attached to read FD events)
 * will be left unchanged.  If called with suppress as false, we
 * return to normal behavior.
 */
int lew_suppress_read_fds(lew_context *context, tbool suppress);


/* ------------------------------------------------------------------------- */
/** If called from an event handler, this will cause libevent to
 * return from lew_dispatch() as soon as the event handler returns and
 * all other currently pending events are handled.
 *
 * \param context The libevent wrapper context.
 *
 * \param sticky If \c false, we escape from at most one level of
 * event dispatching, and then that's it.  If \c true,
 * we continue escaping until lew_unescape_from_dispatch() is called.
 * This mainly affects cases where a caller has entered an inner event
 * loop, and wants to escape from all levels rather than just the
 * deepest one.
 *
 * NOTE: if you pass \c true for the \c sticky parameter, subsequent
 * calls to md_client APIs will probably fail unless you call
 * lew_unescape_from_dispatch() first.  This is because md_client APIs
 * usually reenter the libevent event loop to wait for a response, but
 * if the \c sticky bit remains set, the dispatch call will return
 * right away, before a response is received.
 */
int lew_escape_from_dispatch(lew_context *context, tbool sticky);


/* ------------------------------------------------------------------------- */
/** Undo a prior call to lew_escape_from_dispatch() which had set the
 * sticky bit.  This will normally not be of interest to callers, 
 * since common practice is to only escape from dispatch when you are
 * ready to exit.
 */
int lew_unescape_from_dispatch(lew_context *context);


/* ------------------------------------------------------------------------- */
/** Tell if the \c sticky bit is set in this event context.
 */
int lew_escape_from_dispatch_is_sticky(lew_context *context,
                                       tbool *ret_sticky);


/* ------------------------------------------------------------------------- */
/** Cancel all pending events, but leave their event strutures intact.
 *
 * This will have the effect of causing lew_dispatch() to return as
 * soon as the currently-executing event handler returns to libevent.
 * It is generally intended to be called when a process is ready to
 * exit.
 *
 * \param context The libevent wrapper context.
 */
int lew_cancel_all_events(lew_context *context);


/* ------------------------------------------------------------------------- */
/** Delete all events, whether pending or not.  The libevent wrapper
 * keeps pointers to all of the pointers to event structures used to
 * register events, so it can remove any pending ones from the event queue,
 * deallocate all of their structures, and set all of the original
 * event pointers to NULL.  This effectively leaves the event context
 * empty, as it was after first being initialized.
 *
 * \param context The libevent wrapper context.
 */
int lew_delete_all_events(lew_context *context);


/* ------------------------------------------------------------------------- */
/** Dump information about all events to the logs.
 *
 * \param log_level Logging level (e.g. LOG_INFO) at which to log
 * the information.
 * \param prefix A string to be logged before each line of information.
 * NULL may be provided to have no prefix.
 * \param context The libevent wrapper context.
 */
int lew_dump_all_events(int log_level, const char *prefix, 
                        lew_context *context);


/* ------------------------------------------------------------------------- */
/** Deinitialize the libevent wrapper.
 *
 * NOTE: do not call this function from an event handler.  You must
 * first completely fall out of your event loop (either by deleting
 * all your events, or by calling lew_escape_from_dispatch()), before
 * deinitializing.
 *
 * \param inout_context The libevent wrapper context.  After calling
 * this function the context is deallocated and is no longer usable
 * until another call to lew_init().
 */
int lew_deinit(lew_context **inout_context);


#ifdef __cplusplus
}
#endif

#endif /* __LIBEVENT_WRAPPER_H_ */
