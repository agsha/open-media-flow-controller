/*
 * cmm_output_fqueue.c - Output FQueue interfaces
 */
#include "cmm_output_fqueue.h"
#include "cmm_misc.h"
#include <unistd.h>
#include <pthread.h>
#include <poll.h>
#include <strings.h>
#include <assert.h>

/*
 * Global configuration data
 */
const char *output_queue_filename = NODESTATUS_QUEUE_FILE;
int enqueue_retries = 2;
int enqueue_retry_interval_secs = 1;

/*
 * Global runtime data
 */
pthread_t fqueue_output_thread_id;
int fqueue_output_init_complete = 0;

/*
 * Local global data
 */
pthread_mutex_t output_requestq_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t output_requestq_cv;
static int output_requestq_thread_wait = 1;
SIMPLEQ_HEAD(output_requestq, queue_element);
static struct output_requestq output_requestq_head;
static int num_output_memq;

static int dump_fqueue_element(fqueue_element_t *fq_el, char *attrbuf, 
			       int attrbuf_size)
{
    int n;
    int num_attrs = fq_el->u.e.num_attrs;
    int rv;
    const char *l_name;
    int namelen;
    const char *data;
    int datalen;
    int bytes = 0;
    char namebuf[FQUEUE_ELEMENTSIZE];
    char valuebuf[FQUEUE_ELEMENTSIZE];
    int retval = 0;

    for (n = 0; n < num_attrs; n++) {
        rv = get_attr_fqueue_element(fq_el, 
		((n == 0) ? T_ATTR_PATHNAME : 
			(n == 1) ? T_ATTR_URI : T_ATTR_NAMEVALUE),
		n-2, &l_name, &namelen, &data, &datalen);
	if (!rv) {
	    if (strncasecmp(l_name, "http_header", 11) == 0) {
	    	rv = snprintf(&attrbuf[bytes], attrbuf_size-bytes,
			      "[http_header]: ... ");
	    } else {
	    	memcpy((void *)namebuf, l_name, namelen);
	    	namebuf[namelen] = '\0';
	    	memcpy((void *)valuebuf, data, datalen);
	    	valuebuf[datalen] = '\0';
	    	rv = snprintf(&attrbuf[bytes], attrbuf_size-bytes,
			      "[%s]:%s ", namebuf, valuebuf);
	    }
	    if (rv >= attrbuf_size-bytes) {
		attrbuf[attrbuf_size-1] = '\0';
		retval = 1;
		break;
	    } else {
	    	bytes += rv;
	    }
	}
    }
    return retval;
}

/*
 *******************************************************************************
 * init_output_fqueue() - Initialization
 *******************************************************************************
 */
void init_output_fqueue()
{
    SIMPLEQ_INIT(&output_requestq_head);
}

/*
 *******************************************************************************
 * fqueue_output_request_handler() - Process output fqueue_element_t request
 *******************************************************************************
 */
void *fqueue_output_request_handler(void *arg)
{
    int rv;
    fhandle_t fh;
    queue_element_t *qe = NULL;
    int retries;
    char attrbuf[2048];

    // Open output fqueue
    do {
    	fh = open_queue_fqueue(output_queue_filename);
	if (fh < 0) {
	    DBG("Unable to open output queuefile [%s]",
	    	output_queue_filename);
	    sleep(10);
	}
    } while (fh < 0);

    fqueue_output_init_complete = 1;

    pthread_mutex_lock(&output_requestq_mutex);
    while (1) {
    	SIMPLEQ_FOREACH(qe, &output_requestq_head, queue)
	{
	    SIMPLEQ_REMOVE(&output_requestq_head, qe, queue_element, queue);
	    break;
	}
	if (qe) {
	    num_output_memq--;
	    pthread_mutex_unlock(&output_requestq_mutex);
	    if (DBG_ON) {
		dump_fqueue_element(&qe->fq_element, attrbuf, 
				    sizeof(attrbuf));
	    }
	    retries = enqueue_retries;
	    while (retries--) {
	    	rv = enqueue_fqueue_fh_el(fh, &qe->fq_element);
		if (!rv) {
		    DBG("<== enqueue_fqueue_fh_el() success, "
		    	"qe=%p attr_bytes=%hd num_attrs=%hhd attrs=[%s]",
			qe, qe->fq_element.u.e.attr_bytes_used,
			qe->fq_element.u.e.num_attrs, attrbuf);
		    break;
		} else if (rv == -1) {
		    if (retries) {
		    	sleep(enqueue_retry_interval_secs);
		    } else {
		    	DBG("<== Dropping request, rv=%d "
			    "qe=%p attr_bytes=%hd num_attrs=%hhd attrs=[%s]",
			    rv, qe, qe->fq_element.u.e.attr_bytes_used,
			    qe->fq_element.u.e.num_attrs, attrbuf);
			g_output_fq_retry_drops++;
			break;
		    }
		} else {
		    DBG("<== enqueue_fqueue_fh_el(fd=%d) failed rv=%d, "
		    	"Dropping fqueue_element_t "
		    	"qe=%p attr_bytes=%hd num_attrs=%hhd attrs=[%s]",
			fh, rv, qe, qe->fq_element.u.e.attr_bytes_used,
			qe->fq_element.u.e.num_attrs, attrbuf);
		    g_output_fq_err_drops++;
		    break;
		}
	    }
	    CMM_free(qe);
	    qe = NULL;
	    pthread_mutex_lock(&output_requestq_mutex);
	} else {
	    output_requestq_thread_wait = 1;
	    pthread_cond_wait(&output_requestq_cv, &output_requestq_mutex);
	    output_requestq_thread_wait = 0;
	}
    }
    assert(!"fqueue_output_request_handler() invalid return");
    return NULL;
}

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
int enqueue_output_fqueue(queue_element_t *qe) 
{
    struct pollfd pfd;

    while (1) {
    	// Enqueue queue_element_t to output fqueue thread
    	pthread_mutex_lock(&output_requestq_mutex);
    	if (num_output_memq < output_memq_limit) {
	    SIMPLEQ_INSERT_TAIL(&output_requestq_head, qe, queue);
	    num_output_memq++;
	    if (output_requestq_thread_wait) {
	    	pthread_cond_signal(&output_requestq_cv);
	    }
	    pthread_mutex_unlock(&output_requestq_mutex);
	    return 0;
    	} else {
	    pthread_mutex_unlock(&output_requestq_mutex);
	    poll(&pfd, 0, 100);
	    g_output_memq_stalls++;
    	}
    }
}

/*
 * End of cmm_output_fqueue.c
 */
