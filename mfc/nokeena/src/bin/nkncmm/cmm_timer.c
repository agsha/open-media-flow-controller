/*
 * cmm_timer.c - Periodic timer
 */
#include <unistd.h>
#include "cmm_defs.h"
#include "cmm_timer.h"
#include "cmm_output_fqueue.h"
#include "cmm_misc.h"

pthread_t timer_thread_id;

static int resend_callouts = 1;
static int cmm_timer_init = 0;

static int resend_node_monitor_requests(void)
{
    int rv;
    queue_element_t *qe = 0;

    if (!cmm_timer_init) {
        // At startup, initiate send sequence
        cmm_timer_init = 1;
    	resend_callouts = send_config_interval_secs;
    }

    if (send_config_interval_secs && 
    	(resend_callouts++ >= send_config_interval_secs)) {

	while (1) { /* Begin while */

	qe = (queue_element_t*)CMM_malloc(sizeof(queue_element_t));
	if (qe) {
	    rv = init_queue_element(&qe->fq_element, 0, "", "http://nvsd");
	    if (rv) {
	    	DBG("init_queue_element() failed, rv=%d", rv);
		break;
	    }
	    rv = add_attr_queue_element_len(&qe->fq_element, REQ_NVSD_RESEND,
					    REQ_NVSD_RESEND_STRLEN,
					    "1", 1);
	    if (rv) {
	    	DBG("add_attr_queue_element_len() failed, rv=%d", rv);
		break;
	    }

	    rv = enqueue_output_fqueue(qe);
	    if (!rv) {
	    	qe = NULL;
	    } else {
	    	DBG("enqueue_output_fqueue() failed, rv=%d", rv);
	    }
	} else {
	    DBG("CMM_malloc(sizeof(queue_element_t)) failed, qe=%p", qe);
	}
	break;

	} /* End while */

	if (qe) {
	    CMM_free(qe);
	}
    	resend_callouts = 1;
    }
    return 0;
}

/*
 *******************************************************************************
 * timer_handler() - Thread handler
 *******************************************************************************
 */
void *timer_handler(void *arg)
{
    /* 1 second interval timer */
    while (1) {
    	sleep(1);
	resend_node_monitor_requests();
    }
    return NULL;
}

/*
 * End of cmm_timer.c
 */
