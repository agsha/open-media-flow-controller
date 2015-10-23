/*
 *
 * fdintrcb.h
 *
 *
 *
 */

#ifndef __FDINTRCB_H_
#define __FDINTRCB_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "typed_array_tmpl.h"
#include "ttime.h"

typedef enum {
    fct_none =    0,
    fct_read =    1,
    fct_write,
    fct_timeout,
    fct_open,
    fct_close
} fdic_callback_type;

/** 
 * The function-type clients are told to call back on read or write
 * activity, or on timeout.
 *
 * \param fd The file descriptor where read or write activity was seen, 
 * unless cbt is fct_timeout.
 *
 * \param cbt The type of event that has occurred.
 *
 * \param per_event_arg The argument provided when this particular event
 * object (fd or timer) was registered.  (NOTE: if you are using
 * event_fdintrcb.h for GCL/libevent integration, this holds a pointer to
 * the gcl_session associated with the fd on which activity was seen.)
 *
 * \param global_arg The argument provided when this callback was registered.
 */
typedef int (*fdic_event_callback_func)
     (int fd, fdic_callback_type cbt, void *per_event_arg, void *global_arg);

typedef struct fdic_handle fdic_handle;


typedef struct fdic_event {
    fdic_handle *fee_fich;

    /** same per event instance (a given fd/timeout) */
    int32 fee_event_id;    

    /** If this event id is not longer needed */
    tbool fee_active;

    tbool fee_uses_fd;
    
    /** An fd to watch */
    int fee_fd;
    
    /**
     * The per-fd-arg is something like a session pointer associated with
     * the fd
     */
    void *fee_per_event_arg;

    tbool fee_fd_on_readable;
    tbool fee_fd_on_writable;
    fdic_event_callback_func fee_fd_readable_callback_func;
    void *fee_fd_readable_callback_arg;
    fdic_event_callback_func fee_fd_writable_callback_func;
    void *fee_fd_writable_callback_arg;

    tbool fee_uses_timeout;

    /** time to wait in ms before callback */
    lt_dur_ms fee_timeout_ms;

    fdic_event_callback_func fee_timeout_callback_func;
    void *fee_timeout_callback_arg;

} fdic_event;

TYPED_ARRAY_HEADER_NEW_NONE(fdic_event, fdic_event *);

/**
 * The function-type fd session owners use to tell clients what fds to watch 
 */
typedef int (*fdic_register_event_callback_func)
     (const fdic_event_array *fevents, void *re_arg);

int fdic_init(fdic_handle **ret_fich,
              fdic_register_event_callback_func register_event_func,
              void *register_event_arg);

int fdic_deinit(fdic_handle **ret_fich);

int fdic_event_dup(const fdic_event *ev, fdic_event **ret_ev);

int fdic_event_dup_for_array(const void *src, void **ret_dest);

int fdic_event_compare_for_array(const void *node1, const void *node2);

/** returns an array of all the current events to deal with */
int fdic_events_query(fdic_handle *fich, 
                      fdic_event_array **ret_events);

int fdic_event_add_fd(fdic_handle *fich, int fd,
                      tbool check_readable, 
                      tbool check_writable, 
                      void *per_event_arg,
                      fdic_event_callback_func fd_readable_callback_func,
                      void *fd_readable_callback_arg,
                      fdic_event_callback_func fd_writable_callback_func,
                      void *fd_writable_callback_arg,
                      int32 *ret_event_id);

int fdic_event_update_fd(fdic_handle *fich,
                         int32 event_id,
                         tbool check_readable, 
                         tbool check_writable);

int fdic_event_delete_fd(fdic_handle *fich, int32 event_id);

int fdic_event_add_timer(fdic_handle *fich, 
                         lt_dur_ms timeout, void *per_event_arg,
                         fdic_event_callback_func timeout_callback_func,
                         void *timout_callback_arg,
                         int32 *ret_event_id);

int fdic_event_update_timer(fdic_handle *fich,
                            int32 event_id,
                            lt_dur_ms timeout);

int fdic_event_delete_timer(fdic_handle *fich,
                            int32 event_id);

#ifdef __cplusplus
}
#endif

#endif /* __FDINTRCB_H_ */
