/* 
 * Filename: mfp_pmf_processor.c
 * Date: 23/Jun/2011
 *
 * Implements the PMF processing routines which perform the following
 * functionality.
 * a. Directory Watch: accept a PMF/set of PMFs by watching a
 * directory for PMFs 
 * b. PMF Queuing/Processing: process a list of these queued PMFs
 *
 * PMF Queuing/Processing
 * ----------------------
 * Implements a worker thread processing events out of a queue by
 * draining the queue periodically
 * 1. A TAILQ which queues a list of PMF's added via the directory
 * watcher interface
 * 2. A worker thread that processes these PMFs
 *
 * The above two are encapsulated into a data type called the
 * pmf_proc_mgr_t whose object is an instance of PMF
 * Queuing/Processing feature.
 * 
 * API's implemented for the aforementioned functionalities
 * 1. create_pmf_processor_mgr - creates an instance of a pmf
 *    processor
 * 
 * Directory Watch
 * ---------------
 * the directory watch scans a subscribed directory for new files and
 * feeds them into the PMF Processing Queue. The directory watcher
 * uses the inotify API's available as a part of the linux kernel to
 * perform the directory scanning.
 *
 * The core functionalities include
 * 1. adding a directory to be scanned and be able to add new files to
 * the PMF Processing Queue
 * 2. removing a diretory that is being scanned
 *
 * The implementation includes uses a data type dir_watch_th_obj_t
 * that stores the parameters describing a watch directory including
 * the name, status, locks etc. There is also a global list containing
 * a list of all the dir_watch_th_obj_t objects. This aids in being
 * able to query the status of the watch directories and provides
 * capability to remove watch directories
 *
 * API's implemented for aforementioned functionalities
 * 1. create_dir_watch_obj - creates and initializes a directory watch
 * handle; inputs include the directory name, the pmf_proc_mgr to whom
 * the events need to be sent
 * 2. create_dir_watch_thread - starts the directory watching
 * 3. stop_dir_watch - stops the directory watch
 */
#include <sys/inotify.h>

// mfp includes 
#include "mfp_pmf_processor.h"
#include "mfp_publ_context.h"
#include "mfp_pmf_parser_api.h"
#include "mfp_live_sess_mgr_api.h"
#include "mfp_err.h"

// nkn includes
#include "nkn_memalloc.h"
#include "nkn_debug.h"

/***************************************************************
 *                DEFINITIONS
 **************************************************************/
/** \def Max number of watch dir FD's */
#define N_FD 1024
#define INOTIFY_EVENT_SIZE sizeof(struct inotify_event)
#define EVENT_BUFLEN ((N_FD * INOTIFY_EVENT_SIZE) + 1024)
extern pmf_proc_mgr_t *g_pmgr;
static void * dir_watch_th(void *param);
pthread_mutex_t watch_dir_list_mutex;

/**
 * initializes all the global resources for the PMF processing sub-system
 */
void
init_mfp_pmf_processor(void)
{
        LIST_INIT(&g_watch_dir_list);
	pthread_mutex_init(&watch_dir_list_mutex, NULL);
}

/**
 * creates a instance of the PMF processor
 * @param proc - the session start handler function
 * @return returns a valid address pointing the instance of PMF
 * processor, NULL on error
 */
pmf_proc_mgr_t*
create_pmf_processor_mgr(fnp_start_session proc)
{
	pmf_proc_mgr_t *pmgr;
	
	pmgr = (pmf_proc_mgr_t*)nkn_calloc_type(1,
						sizeof(pmf_proc_mgr_t), 
						mpd_mfp_pmf_proc_mgr_t);
	if (!pmgr) {
		return NULL;
	}

	pmgr->start_session = proc;
	TAILQ_INIT(&pmgr->pmf_process_queue_head);
	pthread_mutex_init(&pmgr->pmf_process_queue_lock, NULL);

	return pmgr;
}

/**
 * creates a directory watch thread and starts the watcher
 * @param dwto - a fully initialized directory watch object
 * @return returns the thread id
 */
int32_t  
create_dir_watch_obj(const char *dir_name,
		     pmf_proc_mgr_t *pmgr,
		     dir_watch_th_obj_t **out)
{
        int32_t err = 0;
	dir_watch_th_obj_t *dwto = NULL;

	dwto = (dir_watch_th_obj_t*)				\
	    nkn_calloc_type(1, sizeof(dir_watch_th_obj_t),
			    mod_mfp_dir_watch_obj_t);
	if (!dwto) {
	    err = -E_MFP_NO_MEM;
	    goto error;
	}
	dwto->watch_dir_name = strdup(dir_name);
	dwto->cleanup = cleanup_dir_watch_obj;
	dwto->pmgr = pmgr;
	dwto->fd = inotify_init();
	dwto->stop = 0;
	*out = dwto;
	
	return err;
 error:
	if (dwto) free(dwto);
	*out = NULL;
	return err;
	
}

/** 
 * creates and initializes and object of the type
 * 'dir_watch_th_obj_t' which can be used in calls to the
 * other dir_watch API's
 * @param dir_name - name of the directory that needs to be
 * watched
 * @param pmgr - the worker thread set for processing the
 * events raised in the directory watch
 * @param out - a fully initialized object
 * @return returs 0 on sucess and a non - zero negative number
 * on error
 */
pthread_t  
start_dir_watch(dir_watch_th_obj_t *dwto)
{
        pthread_t tid;
        pthread_create(&tid, NULL, dir_watch_th,
		       (void*)(dwto));

        return tid;
}

/**
 * stops a directory watch
 * @param dir_name - path to the directory for which the
 * directory watch needs to be stopped 
 * @return returns 0 on success and a non-zero negative
 * integer on error
 */
int32_t
stop_dir_watch(const char *dir_name)
{
        int32_t err = 0;
	dir_watch_th_obj_t *dwto;

	pthread_mutex_lock(&watch_dir_list_mutex);
	LIST_FOREACH(dwto, &g_watch_dir_list, entries) {
	    if (dwto) {
		if (!strcmp(dir_name, dwto->watch_dir_name)) {
		    /* remove the entry from the list and then stop the
		     * directory watch. the sequence is important because
		     * stopping the watch will cleanup dwto
		     */
		    LIST_REMOVE(dwto, entries);
		    dwto->stop = 1;
		    inotify_rm_watch(dwto->fd, dwto->wd);
		    DBG_MFPLOG("0", MSG, MOD_MFPFILE, 
			       "removing directory watch for %s", 
			       dwto->watch_dir_name);
		}
	    }
	}
	pthread_mutex_unlock(&watch_dir_list_mutex);

	return err;
}


/**
 * event loop that processes the INOTIFY events for for a particular
 * directory. Processes publishing requests in the form of PMF files
 * @param fd - the fd created with the INOTIFY API
 * @param path - path to the top of of the watch dir, required to
 * construct the absoulte path of every event
 * @return - runs in a thread does not return
 */
static int32_t 
start_dir_watch_event_loop(pmf_proc_mgr_t* pmgr, int fd, char *path)
{
	int32_t n_events, i;
	char event_buf[EVENT_BUFLEN];

	/* blocks on events specified in the INOTIFY API */
	n_events = read(fd, event_buf, EVENT_BUFLEN);

	i = 0;
	/* process fd list */
	while (i < n_events) {
		struct inotify_event *ev;
		ev = (struct inotify_event *)(&event_buf[i]);
		if (ev->len && (ev->mask & IN_CLOSE_WRITE)) {
			/* Hit the event that we are looking for */

			/* add item to queue */
			mfp_add_pmf_to_queue(pmgr, path, ev->name);
		}

		/* process next event */
		i += INOTIFY_EVENT_SIZE + ev->len;
	}

	return 0;
}


/**
 * cleanup for a pmf queue item. to be called after processing every
 * pmf element
 * @param - pmf element to be freed
 */
static void 
mfp_cleanup_pmf_process_queue_item(pmf_process_queue_item_t *item)
{

	if(item) {
		free(item);
	}
}

/** add PMF item to queue
 * @param base_path - path to the watch directory
 * @param relative_path - relative path of the event, that needs to be
 * added to the queue
 * @param lock - positive integer  if this function is called with the
 * queue lock held. will use the queue lock otherwise the execution of
 * this function
 * @return - returns 0 on success, negative value on error
 */
static int32_t 
mfp_add_pmf_to_queue(pmf_proc_mgr_t* pmgr,
		     char *base_path, char *relative_path)
{
	int32_t rc = 0;
	pmf_process_queue_item_t *pmf_item;

	/* acquire the queue lock if called without a lock */
	pthread_mutex_lock(&pmgr->pmf_process_queue_lock);

	/* allocate for pmf item */
	pmf_item = (pmf_process_queue_item_t*) 
		calloc(1, sizeof(pmf_process_queue_item_t));
	if (!pmf_item) {
		rc = E_MFP_PMF_NO_MEM;
		goto unlock_queue;
	}

	/* construct the full path, the base path should not have a
	   trailing slash 
	*/
	snprintf(pmf_item->pmf_path, 256, "%s/%s",
		 base_path, relative_path);
	
	/* insert into queue */
	TAILQ_INSERT_TAIL(&pmgr->pmf_process_queue_head, pmf_item, entries);
	
unlock_queue:	
	pthread_mutex_unlock(&pmgr->pmf_process_queue_lock);

	return rc;
}

/**
 * Main PMF processing loop
 * Does the following
 * 1. acquires 'queue lock'
 * 2. pops one element from queue
 * 3. unlocks 'queue lock'
 * 4. processes queue element
 * 5. goto #1 till end of queue
 * 6. if end of queue sleep for 1ms and goto #1
 */
void * 
mfp_process_pmf(void *private_data)
{
    pmf_process_queue_item_t *item, *prev = NULL;
	pmf_proc_mgr_t *pmgr;
	int32_t rv;
	
	pmgr = (pmf_proc_mgr_t*)private_data;
	rv = 0;
	
	do {
	    	/* acquire the queue lock */
		pthread_mutex_lock(&pmgr->pmf_process_queue_lock); 

		/* init */
		prev = NULL;
		
		/* loop through all queue elements */
		TAILQ_FOREACH(item, &pmgr->pmf_process_queue_head, entries)
		{
			/* cleanup queue item resources */
    		        if (prev) {
			    mfp_cleanup_pmf_process_queue_item(prev);
		        }

			printf("PMF file name %s\n", item->pmf_path);
			DBG_MFPLOG("0", MSG, MOD_MFPLIVE, "PMF file"
				   " name %s", item->pmf_path);
			/* remove queue element */
			TAILQ_REMOVE(&pmgr->pmf_process_queue_head,
					item, entries);

			rv = pmgr->start_session(item->pmf_path);
			if (rv < 0) {
			    DBG_MFPLOG ("0", ERROR, MOD_MFPLIVE,
					"unable to start the"
					" conversion session -"  
					" error code %d", rv); 
			}
			prev = item;
		}

		if (prev) {
		    mfp_cleanup_pmf_process_queue_item(prev);
		}

		/* unlock the queue when exiting; the queue lock will
		 * always be held when exiting the loop  */
		pthread_mutex_unlock(&pmgr->pmf_process_queue_lock); 

		/* sleep */
		sleep(1);
	} while (1);
}

static int32_t
cleanup_dir_watch_obj(dir_watch_th_obj_t *dwto)
{
        if (dwto) {
	    if (dwto->watch_dir_name) free(dwto->watch_dir_name);
	    free(dwto);
	}
	return 0;
}

static void * 
dir_watch_th(void *param)
{
        dir_watch_th_obj_t *dwto = (dir_watch_th_obj_t*)param;
	char *dir_watch_name = dwto->watch_dir_name;
	pmf_proc_mgr_t *pmgr = dwto->pmgr;
	int fd = dwto->fd;

	dwto->wd = inotify_add_watch(fd, dir_watch_name,
				     IN_CLOSE_WRITE);	
	if (dwto->wd < 0) {
	    DBG_MFPLOG("0", ERROR, MOD_MFPLIVE, "unable to start"
		       " directory watch errno (%d), check directory " 
		       "path", errno);
	    goto cleanup;
	}

	/* succesfully created the inotify object; add to watch dir
	 * list
	 */
	pthread_mutex_lock(&watch_dir_list_mutex);
	LIST_INSERT_HEAD(&g_watch_dir_list, dwto, entries);
	pthread_mutex_unlock(&watch_dir_list_mutex);

	// add the watch folder 
	printf("watching %s for publish events, drop a PMF file in "
	       "this directory to start the publishing workflow\n", 
	       dir_watch_name);

	while (!dwto->stop) {
	    start_dir_watch_event_loop(pmgr, fd, dir_watch_name);
	}

 cleanup:
	if(dwto->cleanup) {
	    dwto->cleanup(dwto);
	}

	return NULL;
}
