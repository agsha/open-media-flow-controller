/*
 * cmm_timer.h - Periodic timer
 */

#ifndef CMM_TIMER_H
#define CMM_TIMER_H

#include <time.h>
#include "cmm_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

extern pthread_t timer_thread_id;

/*
 *******************************************************************************
 * timer_handler() - Thread handler
 *******************************************************************************
 */
void *timer_handler(void *arg);

#ifdef __cplusplus
}
#endif

#endif /* CMM_TIMER_H */

/*
 * End of cmm_timer.h
 */
