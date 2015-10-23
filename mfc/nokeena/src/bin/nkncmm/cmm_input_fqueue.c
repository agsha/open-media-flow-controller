/*
 * cmm_input_fqueue.c - Input FQueue interfaces
 */
#include "cmm_input_fqueue.h"
#include <unistd.h>
#include <pthread.h>
#include <poll.h>
#include <stdint.h>
#include <alloca.h>
#include <assert.h>

#include "fqueue.h"
#include "nkn_cmm_request.h"
#include "cmm_misc.h"

/*
 * Global configuration data
 */
const char *input_queue_filename = CMM_QUEUE_FILE;
int input_queuefile_maxsize = 1024;

/*
 * Global runtime data
 */
pthread_t fqueue_input_thread_id;
int fqueue_input_init_complete;

pthread_mutex_t input_requestq_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t input_requestq_cv;
int input_requestq_thread_wait = 1;

/*
 * Local global data
 */
SIMPLEQ_HEAD(input_requestq, queue_element);
static struct input_requestq input_requestq_head;
static int num_input_memq;

/*
 *******************************************************************************
 * handle_monitor_request() - Deserialize node monitor data and dispatch
 *******************************************************************************
 */
static int handle_monitor_request(CURLM *mch, const fqueue_element_t *qe_l)
{
    int retval = 0;
    int rv;
    const char *data;
    int datalen;
    int nodes;
    cmm_node_config_t cmm_config[MAX_CMM_DATA_ELEMENTS];
    int bytes_used;
    int n;
    const char *ns_name;
    int ns_name_strlen;

    rv = get_nvattr_fqueue_element_by_name(qe_l, REQ_CMM_NAMESPACE, 
					   REQ_CMM_NAMESPACE_STRLEN,
					   &ns_name, &ns_name_strlen);
    if (rv) {
    	ns_name = "???";
    	ns_name_strlen = 3;
    }

    rv = get_nvattr_fqueue_element_by_name(qe_l, REQ_CMM_DATA, 
					   REQ_CMM_DATA_STRLEN,
					   &data, &datalen);
    if (!rv) {
        nodes = 0;
	bytes_used = 0;

  	while ((datalen - bytes_used) > 0) {
	    rv = cmm_request_monitor_data_deserialize(data + bytes_used, 
	    				      datalen - bytes_used, 
					      &cmm_config[nodes],
					      CMM_malloc, CMM_free);
	    if (rv > 0) {
	    	bytes_used += rv;
		nodes++;
	    } else {
	    	DBG("==> nvsd_request_data_deserialize() failed, rv=%d, n=%d",
		    rv, nodes);
		retval = 1;
		goto err_exit;
	    }
	}
    } else {
    	DBG("==> get_nvattr_fqueue_element_by_name(REQ_CMM_DATA) failed, rv=%d",
	    rv);
	retval = 2;
	goto err_exit;
    }
    TSTR(ns_name, ns_name_strlen, nbuf);

    /* Create node monitor SM(s) */
    for (n = 0; n < nodes; n++) {
    	rv = (*add_SM)(mch, &cmm_config[n], ns_name, ns_name_strlen);
	if (!rv) {
	    DBG("==> (*add_SM)(%s:%s) success, rv=%d", nbuf, 
	    	cmm_config[n].node_handle, rv);
	} else {
	    DBG("==> (*add_SM)(%s:%s) failed, rv=%d", 
	    	nbuf, cmm_config[n].node_handle, rv);
	    retval = 3;
	    goto err_exit;
	}
    }

err_exit:

    for (n = 0; n < nodes; n++) {
    	if (cmm_config[n].node_handle) {
	    CMM_free(cmm_config[n].node_handle);
	}
    	if (cmm_config[n].heartbeaturl) {
	    CMM_free(cmm_config[n].heartbeaturl);
	}
    }

    return retval;
}

/*
 *******************************************************************************
 * handle_cancel_request - Deserialize cancel cluster monitor data and dispatch
 *******************************************************************************
 */
static int handle_cancel_request(CURLM *mch, const fqueue_element_t *qe_l)
{
    int retval = 0;
    int rv;
    const char *data;
    int datalen;
    int nodes;
    const char *node_handle[MAX_CMM_DATA_ELEMENTS];
    int node_handle_strlen[MAX_CMM_DATA_ELEMENTS];
    int n;

    rv = get_nvattr_fqueue_element_by_name(qe_l, REQ_CMM_DATA, 
					   REQ_CMM_DATA_STRLEN,
					   &data, &datalen);
    if (!rv) {
    	rv = cmm_request_cancel_data_deserialize(data, datalen, &nodes,
						 node_handle, 
						 node_handle_strlen);
	if (rv) {
	    DBG("==> cmm_request_cancel_data_deserialize() failed, rv=%d", rv);
	    retval = 1;
	    goto err_exit;
	}
    } else {
    	DBG("==> get_nvattr_fqueue_element_by_name(REQ_CMM_DATA) "
	    "failed, rv=%d", rv);
	retval = 2;
	goto err_exit;
    }

    /* Cancel node monitor SM(s) */
    for (n = 0; n < nodes; n++) {
    	rv = (*cancel_SM)(node_handle[n], node_handle_strlen[n]);
	TSTR(node_handle[n], node_handle_strlen[n], tbuf);
	if (!rv) {
	    DBG("==> (*cancel_SM)(%s) success, rv=%d", tbuf, rv);
	} else {
	    DBG("==> (*cancel_SM)(%s) failed, rv=%d", tbuf, rv);
	    retval = 3;
	    goto err_exit;
	}
    }

err_exit:

    return retval;
}

/*
 *******************************************************************************
 * process_input_request() - Dispatch input request to handler
 *	Returns: 0 => Success
 *******************************************************************************
 */
static int process_input_request(CURLM *mch, const fqueue_element_t *qe_l)
{
    int rv;
    const char *data;
    int datalen;

    rv = get_nvattr_fqueue_element_by_name(qe_l, REQ_CMM_MONITOR, 
					   REQ_CMM_MONITOR_STRLEN,
					   &data, &datalen);
    if (!rv) {
    	return handle_monitor_request(mch, qe_l);
    }

    rv = get_nvattr_fqueue_element_by_name(qe_l, REQ_CMM_CANCEL, 
					   REQ_CMM_CANCEL_STRLEN,
					   &data, &datalen);
    if (!rv) {
    	return handle_cancel_request(mch, qe_l);
    }

    return 1; // Invalid request
}

/*
 *******************************************************************************
 * init_input_fqueue() - Initialization
 *******************************************************************************
 */
void init_input_fqueue()
{
    SIMPLEQ_INIT(&input_requestq_head);
}

/*
 *******************************************************************************
 * fqueue_input_request_handler() - Process input fqueue_element_t request
 *				    thread handler.				
 *******************************************************************************
 */
void *fqueue_input_request_handler(void *arg)
{
    int rv;
    fhandle_t fh;
    queue_element_t *qe = NULL;
    pthread_condattr_t cattr;
    struct pollfd pfd;

    // Create input fqueue
    do {
    	rv = create_fqueue(input_queue_filename, input_queuefile_maxsize);
	if (rv) {
	    DBG("Unable to create input queuefile [%s], rv=%d",
		input_queue_filename, rv);
	    sleep(10);
	}
    } while (rv);

    // Open input fqueue
    do {
    	fh = open_queue_fqueue(input_queue_filename);
	if (fh < 0) {
	    DBG("Unable to open input queuefile [%s]",
	    	input_queue_filename);
	    sleep(10);
	}
    } while (fh < 0);

    // Use CLOCK_MONOTONIC for pthread_cond_timedwait
    pthread_condattr_init(&cattr);
    pthread_condattr_setclock(&cattr, CLOCK_MONOTONIC);
    pthread_cond_init(&input_requestq_cv, &cattr);

    fqueue_input_init_complete = 1;

    while (1) {
    	if (!qe) {
	    qe = (queue_element_t *)CMM_calloc(1, sizeof(queue_element_t));
	    if (!qe) { // Should never happen
	    	DBG("CMM_calloc() failed, size=%ld", sizeof(queue_element_t));
		sleep(2);
		continue;
	    }
	}
    	rv = dequeue_fqueue_fh_el(fh, &qe->fq_element, 1);
	if (rv == -1) {
	    continue;
	} else if (rv) {
	    DBG("dequeue_fqueue_fh_el(%s) failed, rv=%d",
		input_queue_filename, rv);
	    poll(&pfd, 0, 100);
	    continue;
	}

	while (1) {
	    // Enqueue queue_element_t to main_event_loop() thread
	    pthread_mutex_lock(&input_requestq_mutex);
	    if (num_input_memq < input_memq_limit) {
	    	SIMPLEQ_INSERT_TAIL(&input_requestq_head, qe, queue);
	    	num_input_memq++;
	    } else {
	    	pthread_mutex_unlock(&input_requestq_mutex);
	    	poll(&pfd, 0, 100);
		g_input_memq_stalls++;
		continue;
	    }
	    if (input_requestq_thread_wait) {
	    	pthread_cond_signal(&input_requestq_cv);
	    }
	    pthread_mutex_unlock(&input_requestq_mutex);
	    qe = NULL;
	    break;
	}
    }
    assert(!"fqueue_input_request_handler() invalid return");
    return NULL;
}

/*
 *******************************************************************************
 * handle_fqueue_requests() - Process input fqueue_element_t(s) in memory queue
 *******************************************************************************
 */
void handle_fqueue_requests(CURLM *mch, int return_locked, 
			    int max_msgs_processed)
{
    int rv;
    queue_element_t *qe;
    int msgs_processed = 0;

    pthread_mutex_lock(&input_requestq_mutex);
    while (msgs_processed < max_msgs_processed) {
    	SIMPLEQ_FOREACH(qe, &input_requestq_head, queue)
	{
	    SIMPLEQ_REMOVE(&input_requestq_head, qe, queue_element, queue);
	    break;
	}
	if (qe) {
	    num_input_memq--;
	    pthread_mutex_unlock(&input_requestq_mutex);
	    rv = process_input_request(mch, &qe->fq_element);
	    if (rv) {
	    	DBG("process_input_request() failed, qe=%p rv=%d", qe, rv);
	    }
	    CMM_free(qe);
	    qe = NULL;
	    msgs_processed++;
	    pthread_mutex_lock(&input_requestq_mutex);
	} else {
	    break;
	}
    }

    if (!return_locked) {
    	pthread_mutex_unlock(&input_requestq_mutex);
    }

    if (msgs_processed >= max_msgs_processed) {
    	// Hit max msg process limit
    	g_input_memq_msg_limit++;
    }
}

/*
 *******************************************************************************
 * unlock_input_fqueue() - Unlock input FQueue
 *******************************************************************************
 */
void unlock_input_fqueue()
{
    pthread_mutex_unlock(&input_requestq_mutex);
}

/*
 *******************************************************************************
 * cond_timedwait_input_fqueue() - pthread_cond_timedwait() on input FQueue
 *******************************************************************************
 */
void cond_timedwait_input_fqueue(const struct timespec *ts)
{
    int rv;

    input_requestq_thread_wait = 1;
    rv = pthread_cond_timedwait(&input_requestq_cv, &input_requestq_mutex, ts);
    input_requestq_thread_wait = 0;
    pthread_mutex_unlock(&input_requestq_mutex);
}

/*
 * End of cmm_input_fqueue.c
 */
