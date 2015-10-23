/*
 *
 * event_fdintrcb.h
 *
 *
 *
 */

#ifndef __EVENT_FDINTRCB_H_
#define __EVENT_FDINTRCB_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "fdintrcb.h"
#include "event_external.h"

/**
 * \file src/include/event_fdintrcb.h
 * \ingroup libevent
 *
 * FDIC/libevent integration.
 *
 * Call fdic_libevent_init() before initializing the GCL.
 * It will give you back a fdic_libevent_context pointer.
 * When you call fdic_init, pass fdic_libevent_callback() as the
 * third parameter (register_event_func), and the context pointer
 * as the fourth parameter (register_event_arg).  Do not call 
 * fdic_libevent_callback() directly.
 *
 * Subsequently, the library will take care of keeping libevent informed
 * whenever a library (like the GCL) wants to add, remove, or modify events
 * (file descriptors or timeouts).
 *
 * After calling fdic_deinit(), call fdic_libevent_deinit() to clean
 * up the data structures used in providing this service.
 *
 * NOTE: that this library does not use the libevent wrapper;
 * it adds events directly to libevent.  It is not thought to have any
 * of the errors the libevent wrapper is designed to avoid; but the
 * caller should be aware of other implications, such as that calling
 * lew_delete_all_events() will not delete events registered
 * through us.
 */

typedef struct fdic_libevent_context fdic_libevent_context;

int fdic_libevent_init(event_context *ev_ctxt, 
                       fdic_libevent_context **ret_context);
int fdic_libevent_callback(const fdic_event_array *events, void *data);
int fdic_libevent_deinit(fdic_libevent_context **inout_context);
int fdic_libevent_set_fd_open_handler(fdic_libevent_context *context,
                                      fdic_event_callback_func open_cb,
                                      void *cb_arg);

/**
 * Set a callback to be called when a file descriptor is closed.
 * If you are using this for GCL/libevent integration, this would be 
 * called when your GCL session (e.g. connection with mgmtd) is broken,
 * whether the breakage was initiated by you or by the other side.
 *
 * NOTE: cb_arg will be passed back to your callback function as the
 * global_arg (not the per-event arg).
 */
int fdic_libevent_set_fd_close_handler(fdic_libevent_context *context,
                                       fdic_event_callback_func close_cb,
                                       void *cb_arg);


#ifdef __cplusplus
}
#endif

#endif /* __EVENT_FDINTRCB_H_ */
