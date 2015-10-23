/*
 * cmm_input_fqueue.h - Input FQueue interfaces
 */

#ifndef CMM_INPUT_FQUEUE_H
#define CMM_INPUT_FQUEUE_H

#include <time.h>
#include "cmm_defs.h"
#include "curl/curl.h"

#ifdef __cplusplus
extern "C" {
#endif

extern pthread_t fqueue_input_thread_id;
extern int fqueue_input_init_complete;

/*
 *******************************************************************************
 * init_input_fqueue() - Initialization
 *******************************************************************************
 */
void init_input_fqueue(void);

/*
 *******************************************************************************
 * fqueue_input_request_handler() - Thread handler
 *******************************************************************************
 */
void *fqueue_input_request_handler(void *arg);

/*
 *******************************************************************************
 * handle_fqueue_requests() - Process input fqueue_element_t(s) in memory queue
 *******************************************************************************
 */
void handle_fqueue_requests(CURLM *mch, int return_locked, 
			    int max_msgs_processed);

/*
 *******************************************************************************
 * unlock_input_fqueue() - Unlock input FQueue
 *******************************************************************************
 */
void unlock_input_fqueue(void);

/*
 *******************************************************************************
 * cond_timedwait_input_fqueue() - pthread_cond_timedwait() on input FQueue
 *******************************************************************************
 */
void cond_timedwait_input_fqueue(const struct timespec *ts);

#ifdef __cplusplus
}
#endif

#endif /* CMM_INPUT_FQUEUE_H */

/*
 * End of cmm_input_fqueue.h
 */
