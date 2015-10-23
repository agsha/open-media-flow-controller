/*
 * network_hres_timer.h 
 *
 * High resolution timer (subsecond resolution) support for 
 * network_mgr_t objects.
 * Interface provides support for specific types of timers (net_timer_type_t)
 * per network_mgr_t object.
 */
#ifndef _NETWORK_HRES_TIMER_H_
#define _NETWORK_HRES_TIMER_H_

#include "nkn_defs.h"

typedef enum {
    TT_ONESHOT = 0,
    TT_MAX // Always last
} net_timer_type_t;

/*
 * net_set_timer() 
 *	fhd - (I) see net_fd_handle_t definition
 *	interval_msecs - (I) Timer interval in milliseconds
 *	type - (I) see net_timer_type_t definition
 *
 *	Returns: 0 => Success
 *
 *	Notes:
 *	1) Caller has acquired the gnm[fd].mutex prior to the call
 *	2) If a net_time_type_t is pending, it is implicitly canceled
 *	   prior to handling the new set request.
 *	3) Callout made via network_mgr_t.f_timer (see network.h)
 *	   Returns: 0 => Success
 */
int 
net_set_timer(net_fd_handle_t fhd, int interval_msecs, net_timer_type_t type);

/*
 * net_cancel_timer() 
 *	fhd - (I) see net_fd_handle_t definition
 *	net_time_type_t - (I) see net_timer_type_t definition
 *
 *	Returns: 0   => Success
 *		 < 0 => Retry
 *
 *	Notes:
 *	1) Caller has acquired the gnm[fd].mutex prior to the call
 */
int net_cancel_timer(net_fd_handle_t fhd, net_timer_type_t type);

/*
 * int network_hres_timer_init()
 *
 * 	Subsystem initialization.
 *
 *	Returns: 0 => Success
 */
int network_hres_timer_init(void);

#endif /* _NETWORK_HRES_TIMER_H_ */

/*
 * End of network_hres_timer.h
 */
