/*
  Nokeena Networks proprietary
  Author: Vikram Venkataragahvan
  07/01/2008
*/

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include "nkn_rtsched_task.h"
#include <sys/types.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <atomic_ops.h>
#include "nkn_slotapi.h"
#include "nkn_lockstat.h"
#include <sys/prctl.h>
#include "nkn_memalloc.h"

nkn_slot_mgr *nkn_rtsched_deadline_mgr[8];

/* We can have deadlines to upto 60 seconds */
#define S_DEADLINE_CHUNK 100 /* In milliseconds */
#define NKN_DEADLINE_MAX_SLOTS 600 /* 600 * 100 milliseconds = 60 seconds */

/* 8 different deadline queues can be defined */
GAsyncQueue *nkn_glob_rtsched_runnable_q_array[NKN_DEADLINE_MAX_SLOTS];
GAsyncQueue *nkn_glob_rtsched_main_task_q;
uint32_t nkn_glob_rtsched_deadline_q_count[NKN_DEADLINE_MAX_SLOTS];
AO_t glob_rtsched_main_task_q_count = 0;
GAsyncQueue *nkn_glob_rtsched_cleanup_q;
static pthread_mutex_t nkn_rttask_q_mutex;

AO_t glob_rtsched_runnable_q_pushes = 0;
AO_t glob_rtsched_runnable_q_pops = 0;
AO_t glob_rtsched_cleanup_q_pushes = 0;
uint64_t glob_rtsched_cleanup_q_pops = 0;
// disabling worker thread
//long long glob_rtsched_worker_threads = 3;
int rtsched_dbg_dup_rttask_check = 0;
static void
s_dbg_find_duplicate_rttask(GAsyncQueue *in_queue, nkn_task_id_t tid);

struct rtsched_queue
{
    GMutex *mutex;
    GCond *cond;
    GQueue *queue;
    GDestroyNotify item_free_func;
    guint waiting_threads;
    gint32 ref_count;
};

void rtsched_queue_dump_counters (void);
void nkn_rttask_q_cleanup (void);
void nkn_rttask_q_insert_sorted (GAsyncQueue *in_async_queue,
			       struct nkn_task *in_ntask);

static inline double
tv_diff (struct timeval *tv1, struct timeval *tv2)
{
    return ((tv2->tv_sec - tv1->tv_sec)
	    + (tv2->tv_usec - tv1->tv_usec) / 1e6);
}


void rtsched_queue_dump_counters(void)
{
    printf("\nglob_rtsched_runnable_q_pushes = %ld",
	    AO_load(&glob_rtsched_runnable_q_pushes));
    printf("\nglob_rtsched_runnable_q_pops = %ld",
	    AO_load(&glob_rtsched_runnable_q_pops));
    printf("\nglob_rtsched_cleanup_q_pushes = %ld",
	    AO_load(&glob_rtsched_cleanup_q_pushes));
    printf("\nglob_rtsched_cleanup_q_pops = %ld", glob_rtsched_cleanup_q_pops);
}

#if 0 // if needed worker thread can be enabled
struct head_entry {
	void *task_data;
	void (*task_func) (void *);
	TAILQ_ENTRY(head_entry) entries;
};

static TAILQ_HEAD(tailhead, head_entry) *head_queue;
static pthread_mutex_t *head_queue_lock;
static pthread_cond_t *head_queue_cond;
static pthread_t *thread_list;
static int queue_thr_index[8];
static void (*fp)(void *arg) = NULL; 
static int worker_threads;

void setFunc ( void (*p)(void *arg));
void setFunc ( void (*p)(void *arg))
{
	fp = p;
}

static void *worker_thread(void *arg);
int init_worker_threads(int num);
int init_worker_threads(int num)
{
	int i = 0;
	worker_threads = num;
	head_queue = nkn_calloc_type(num,
			sizeof(TAILQ_HEAD(tailhead, head_entry)),
			mod_rtsched_nkn_task);
	head_queue_lock = nkn_calloc_type(num, sizeof(pthread_mutex_t),
			mod_rtsched_nkn_task);
	head_queue_cond = nkn_calloc_type(num, sizeof(pthread_cond_t),
			mod_rtsched_nkn_task);
	thread_list = nkn_calloc_type(num, sizeof(pthread_t),
			mod_rtsched_nkn_task);
	for ( ; i < num ; ++i) {
		TAILQ_INIT(&head_queue[i]);
		pthread_mutex_init(&head_queue_lock[i], NULL);
		queue_thr_index[i] = i;
		pthread_create(&thread_list[i], NULL,
			worker_thread , (void *) &queue_thr_index[i]);
	}

	return 0;
}


extern void * nkn_rtscheduleDelayedTask(int64_t microseconds,
                                void  (*proc) ( void *),
                                void* clientData);
#if 0
void add_to_worker_thread(void (*func) ( void *), void *arg)
{
	static unsigned int rr_counter = 0;
	static unsigned int rrr_counter = 0;
	static unsigned int selector = 0;
	int current = (++rr_counter) % (2 * worker_threads);
	struct head_entry  *h_entry = calloc(1, sizeof(struct head_entry));
	h_entry->task_data = arg;
	h_entry->task_func = func;
	pthread_mutex_lock (&head_queue_lock[selector]);
	if (selector == 0)
		glob_rtsch_worker_thread_0++;
	else 
		glob_rtsch_worker_thread_1++;
	TAILQ_INSERT_TAIL(&head_queue[selector], h_entry, entries);
	pthread_cond_signal(&head_queue_cond[selector]);

    	//nkn_rtscheduleDelayedTask(30000, func, arg);
	pthread_mutex_unlock (&head_queue_lock[selector]);
	if (current % 2  == 0)
		selector  = (++rrr_counter) % (worker_threads);
}
#endif
long long glob_rtsched_worker_thread_0;
long long glob_rtsched_worker_thread_1;
void add_to_worker_thread(void (*func) ( void *), void *arg)
{
	static unsigned int rr_counter = 0;
	int current = (++rr_counter) % (worker_threads);
	struct head_entry  *h_entry = nkn_calloc_type(1,
				      sizeof(struct head_entry),
				      mod_rtsched_nkn_task);
	h_entry->task_data = arg;
	h_entry->task_func = func;
	pthread_mutex_lock (&head_queue_lock[current]);
	TAILQ_INSERT_TAIL(&head_queue[current], h_entry, entries);
	pthread_cond_signal(&head_queue_cond[current]);
	pthread_mutex_unlock (&head_queue_lock[current]);
}



static void *worker_thread(void *arg)
{
     int queue_index = *(int*)arg;
     struct head_entry *task_entry;
     prctl(PR_SET_NAME, "nvsd-rt-work", 0, 0, 0);
     while (1) {
		pthread_mutex_lock(&head_queue_lock[queue_index]);
		if ((task_entry = head_queue[queue_index].tqh_first) != NULL) {
			TAILQ_REMOVE(&head_queue[queue_index], task_entry, entries);
			pthread_mutex_unlock(&head_queue_lock[queue_index]);
			(*task_entry->task_func)(task_entry->task_data);
			free(task_entry);
		} else {
			pthread_cond_wait(&head_queue_cond[queue_index], &head_queue_lock[queue_index]);
			pthread_mutex_unlock(&head_queue_lock[queue_index]);
		}
     }		
}
#endif

/*
 * Description: This function initializes an array of queues.
 */
void
nkn_rttask_q_init()
{
    int i;
    int ret_val = -1;

    nkn_glob_rtsched_main_task_q = g_async_queue_new();
    AO_store(&glob_rtsched_main_task_q_count, 0);
    for(i = 0; i < NKN_DEADLINE_MAX_SLOTS; i++) {
	/* At the moment, we are not using an array of queues for the
	   runnable queue. Commmenting out for now. */
	//nkn_glob_rtsched_runnable_q_array[i] = g_queue_new();
	//assert(nkn_glob_rtsched_runnable_q_array[i]);
	nkn_glob_rtsched_deadline_q_count[i] = 0;
    }
    nkn_glob_rtsched_cleanup_q = g_async_queue_new();

    ret_val = pthread_mutex_init(&nkn_rttask_q_mutex, NULL);
    if(ret_val < 0) {
        DBG_LOG(SEVERE, MOD_TFM, "Sched Mutex not created. "
		"Severe Scheduler failure");
        return;
    }

    assert(glob_rtsched_num_core_threads <= 8);
    for (i = 0; i < glob_rtsched_num_core_threads; i++) {
	    nkn_rtsched_deadline_mgr[i] = slot_init(1000000, 1, 1);
    }
    //disabling worker thread
    //init_worker_threads(glob_rtsched_worker_threads);
    return;
}

void
nkn_rttask_q_cleanup (void)
{
    /* TBD: Need to free Async queues */
    /* This function is not getting called today */
    nkn_glob_rtsched_main_task_q = NULL;
    nkn_glob_rtsched_cleanup_q = NULL;
}

/**
 * Description: This function pops the next function from queues
 * based on a 'deadline' if the task state is RUNNABLE.
 * If the task state is not RUNNABLE
 */
struct nkn_task *
nkn_rttask_pop_from_queue(int tnum, nkn_task_state_t tstate)
{
    int             deadline = 0;
    struct nkn_task *ntask = NULL;

    switch(tstate) {
	case TASK_STATE_RUNNABLE:
	    ntask  = 
		(struct nkn_task *)
		rtslot_get_next_item(nkn_rtsched_deadline_mgr[tnum]);

	    assert(ntask);
	    assert(ntask->sched_priv != 0xdeaddead);
	    s_dbg_find_duplicate_rttask(nkn_glob_rtsched_main_task_q,
				      ntask->task_id);
	    ntask->task_state = TASK_STATE_RUNNING;
	    deadline = ntask->deadline_q_idx;

	    ntask->debug_q_status  = DEBUG_Q_STATUS_POP;
	    AO_fetch_and_add1(&glob_rtsched_runnable_q_pops);
	    AO_fetch_and_sub1(&glob_rtsched_main_task_q_count);
	    nkn_rttask_check_deadline_miss(ntask->task_id);
	    return (ntask);
	default:
	    return NULL;
    }
    return NULL;
}

void
nkn_rttask_q_insert_sorted(GAsyncQueue *in_async_queue, struct nkn_task *in_ntask)
{
    GList *list ;
    struct nkn_task *ntask;
    struct rtsched_queue *tmp = (struct rtsched_queue *) in_async_queue;
    GQueue *in_queue = NULL;

    in_queue = tmp->queue;

    assert(in_queue != NULL);

    list = in_queue->head;

    while(list) {
	ntask = (struct nkn_task *) list->data;

	if(ntask->deadline_in_msec > in_ntask->deadline_in_msec) {
	    break;
	}
	list = list->next;
    }

    if(list)
	g_queue_insert_before (in_queue, list, (gpointer)in_ntask);
    else
	g_queue_push_tail (in_queue, (gpointer)in_ntask);
}

#if 0
static gint
s_sort_compare (gconstpointer p1,
		gconstpointer p2,
		gpointer user_data __attribute((unused)))
{
    gint32 id1;
    gint32 id2;

    struct nkn_task *ntask1 = (struct nkn_task *) p1;
    struct nkn_task *ntask2 = (struct nkn_task *) p2;

    id1 = ntask1->deadline_in_msec;
    id2 = ntask2->deadline_in_msec;

    return (id1 > id2 ? +1 : id1 == id2 ? 0 : -1);
}
#endif


/**
 *      - This function pushes a new task into the array of queues.
 *	- The tasks will be prioritized based on a deadline.
 */
void
nkn_rttask_add_to_queue(struct nkn_task *ntask)
{
    int deadline = 0;
    uint32_t next_avail_slot;

    assert(ntask);

    switch (ntask->task_state) {
	case TASK_STATE_RUNNABLE:
	    deadline = ntask->deadline_in_msec;
	    nkn_rttask_set_task_runnable_time(ntask->task_id);
	    assert(ntask->sched_priv != 0xdeaddead);
	    ntask->debug_q_status  = DEBUG_Q_STATUS_PUSH;
	    /* Asynchronous task handling between network
	       thread and scheduler thread.*/
	    rtslot_add(nkn_rtsched_deadline_mgr[ntask->task_id %
		glob_rtsched_num_core_threads],
		ntask->deadline_in_msec, ntask->max_msec, (void *)ntask,
		&next_avail_slot);
	    AO_fetch_and_add1(&glob_rtsched_main_task_q_count);
	    AO_fetch_and_add1(&glob_rtsched_runnable_q_pushes);
	    break;

	case TASK_STATE_CLEANUP:
	    AO_fetch_and_add1(&glob_rtsched_cleanup_q_pushes);
	    g_async_queue_push(nkn_glob_rtsched_cleanup_q, ntask);
	    break;

	default:
	    break;
    }
}



static void
s_dbg_find_duplicate_rttask(GAsyncQueue *in_queue, nkn_task_id_t tid)
{
    GList *list;
    struct nkn_task *ntask = NULL;
    struct rtsched_queue *aqueue = (struct rtsched_queue *)in_queue;
    GQueue *queue = aqueue->queue;

    if(rtsched_dbg_dup_rttask_check == 1) {
	g_return_if_fail (queue != NULL);

	list = queue->head;
	while (list)
	    {
		GList *next = list->next;
		ntask = (struct nkn_task *) list->data;
		if((ntask) && (tid == ntask->task_id)) {
		    printf("\n OOPS OOPS OOPS \n");
		    assert(0);
		}
		list = next;
	    }
    }
    else
	return ;
}
