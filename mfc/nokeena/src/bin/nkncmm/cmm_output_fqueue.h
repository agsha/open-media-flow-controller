/*
 * cmm_output_fqueue.h - Output FQueue interfaces
 */

#ifndef CMM_OUTPUT_FQUEUE_H
#define CMM_OUTPUT_FQUEUE_H

#include "cmm_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

extern pthread_t fqueue_output_thread_id;
extern int fqueue_output_init_complete;

/*
 *******************************************************************************
 * init_output_fqueue() - Initialization
 *******************************************************************************
 */
void init_output_fqueue(void);

/*
 *******************************************************************************
 * fqueue_output_request_handler - Thread handler
 *******************************************************************************
 */
void *fqueue_output_request_handler(void *arg);

/*
 ******************************************************************************
 * enqueue_output_fqueue() - Enqueue queue_element_t to output FQueue
 *
 *	enqueue_output_fqueue() owns passed malloc'ed memory upon successful 
 *	return.
 *
 *	Returns: 0 => Success
 ******************************************************************************
 */
int enqueue_output_fqueue(queue_element_t *qe);

#ifdef __cplusplus
}
#endif

#endif /* CMM_OUTPUT_FQUEUE_H */
/*
 * End of cmm_output_fqueue.h
 */
