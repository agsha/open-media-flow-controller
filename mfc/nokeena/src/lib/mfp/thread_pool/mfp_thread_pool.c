#include "thread_pool/mfp_thread_pool.h"
#include <sys/prctl.h>
static void* workerThreadMain(void*);


static void deleteMfpThreadPool(mfp_thread_pool_t*);
static void addWorkItemToPool(mfp_thread_pool_t*, thread_pool_task_t*);
static uint32_t getTPQueueFillLen(mfp_thread_pool_t*);

static void deleteMfpThreadPoolTask(thread_pool_task_t*);


/* These functions encapsulate the delete and unlock functions.
Reason: pthread_cleanup_push requires void fn(void*) type    */
static void mfpDeleteTaskTP(void* arg);
static void mfpTPUnlockMutex(void* arg);

tp_thread_conf_fptr tp_thread_conf_hdlr = NULL;

static void* tp_malloc(uint32_t num); 

static void* tp_calloc(uint32_t num, uint32_t size); 

tp_malloc_fptr tp_malloc_hdlr = tp_malloc;
tp_calloc_fptr tp_calloc_hdlr = tp_calloc;

uint32_t tp_thr_count = 0;

void initThreadPoolLib(tp_malloc_fptr malloc_hdlr, tp_calloc_fptr calloc_hdlr,
			            tp_thread_conf_fptr thread_conf_hdlr) {
	if (malloc_hdlr != NULL)
		tp_malloc_hdlr = malloc_hdlr;
	if (calloc_hdlr != NULL)
		tp_calloc_hdlr = calloc_hdlr;
	tp_thread_conf_hdlr = thread_conf_hdlr;
}


mfp_thread_pool_t* newMfpThreadPool(uint32_t num_worker_threads, 
		uint32_t max_work_queue_len) {

	if ((num_worker_threads == 0) || (max_work_queue_len == 0))
		return NULL;
	mfp_thread_pool_t* thread_pool =
		tp_calloc_hdlr(1, sizeof(mfp_thread_pool_t));
	if (thread_pool == NULL)
		return NULL;
	thread_pool->worker_threads = NULL;
	thread_pool->num_worker_threads = num_worker_threads;
	thread_pool->max_work_queue_len = max_work_queue_len;
	thread_pool->queue_fill_len = 0;
	thread_pool->worker_threads =
		tp_calloc_hdlr(num_worker_threads, sizeof(pthread_t));
	if (thread_pool->worker_threads == NULL) {
		free(thread_pool);
		return NULL;
	}
	pthread_mutex_init(&thread_pool->thread_pool_lock, NULL);
	pthread_cond_init(&thread_pool->task_posted, NULL);
	pthread_cond_init(&thread_pool->task_taken, NULL);

	TAILQ_INIT(&thread_pool->work_item_queue);

	thread_pool->add_work_item = addWorkItemToPool;
	thread_pool->get_queue_fill_len = getTPQueueFillLen;
	thread_pool->delete_thread_pool = deleteMfpThreadPool;

	uint32_t i = 0;
	for (; i < num_worker_threads; i++)
		pthread_create(thread_pool->worker_threads + i, NULL,
				workerThreadMain, (void*)thread_pool);
	return thread_pool;
}


static void deleteMfpThreadPool(mfp_thread_pool_t* thread_pool) {

	int32_t actual_thread_state;
	uint32_t i = 0;
	pthread_mutex_lock(&thread_pool->thread_pool_lock);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &actual_thread_state);
	pthread_cleanup_push(mfpTPUnlockMutex,
			(void*)&thread_pool->thread_pool_lock);

	for (; i < thread_pool->num_worker_threads; i++)
		pthread_cancel(thread_pool->worker_threads[i]);

	thread_pool_task_t* work_item = NULL;
	/*TAILQ_FOREACH(work_item, &thread_pool->work_item_queue, queue_ctxt) {

	  TAILQ_REMOVE(&thread_pool->work_item_queue,
	  work_item, queue_ctxt);
	  work_item->delete_thread_pool_task(work_item);
	  printf("deleting from queue\n");
	  }*/
	while (1) {
		work_item = TAILQ_FIRST(&thread_pool->work_item_queue);
		if (work_item == NULL)
			break;
		TAILQ_REMOVE(&thread_pool->work_item_queue,
				work_item, queue_ctxt);
		work_item->delete_thread_pool_task(work_item);
	}
	pthread_cleanup_pop(1);

	for (i = 0; i < thread_pool->num_worker_threads; i++)
		pthread_join(thread_pool->worker_threads[i], NULL);

	pthread_mutex_destroy(&thread_pool->thread_pool_lock);
	pthread_cond_destroy(&thread_pool->task_posted);
	pthread_cond_destroy(&thread_pool->task_taken);

	free(thread_pool->worker_threads);
	free(thread_pool);
	pthread_setcanceltype(actual_thread_state, NULL);
}


void addWorkItemToPool(mfp_thread_pool_t* thread_pool,
		thread_pool_task_t* work_item) {

	int32_t actual_thread_state;
	pthread_mutex_lock(&thread_pool->thread_pool_lock);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &actual_thread_state);
	pthread_cleanup_push(mfpTPUnlockMutex,
			&thread_pool->thread_pool_lock);

	while (thread_pool->queue_fill_len == thread_pool->max_work_queue_len)
		//cond_wait is a cancellation point
		pthread_cond_wait(&thread_pool->task_taken,
				&thread_pool->thread_pool_lock);

	TAILQ_INSERT_TAIL(&thread_pool->work_item_queue, work_item, queue_ctxt);
	thread_pool->queue_fill_len++;
	pthread_cond_signal(&thread_pool->task_posted);
	pthread_setcanceltype(actual_thread_state, NULL);
	pthread_cleanup_pop(1);
}


static uint32_t getTPQueueFillLen(mfp_thread_pool_t* thread_pool) {

	return thread_pool->queue_fill_len;
}


static void* workerThreadMain(void* arg) {

	if (tp_thread_conf_hdlr != NULL)
		tp_thread_conf_hdlr(tp_thr_count);
	tp_thr_count++;
	mfp_thread_pool_t* thread_pool = (mfp_thread_pool_t*)arg;
	prctl(PR_SET_NAME, "mfp-tpool-worker", 0, 0, 0);	

	while (1) {
		pthread_mutex_lock(&thread_pool->thread_pool_lock); 
		pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
		thread_pool_task_t* work_item = NULL;
		pthread_cleanup_push(mfpTPUnlockMutex,
				(void *)&thread_pool->thread_pool_lock);

		while (TAILQ_EMPTY(&thread_pool->work_item_queue))
			//cond_wait is a cancellation point
			pthread_cond_wait(&thread_pool->task_posted,
					&thread_pool->thread_pool_lock);
		work_item = TAILQ_FIRST(&thread_pool->work_item_queue);
		TAILQ_REMOVE(&thread_pool->work_item_queue,
				work_item, queue_ctxt);
		thread_pool->queue_fill_len--;
		pthread_cond_signal(&thread_pool->task_taken);
		pthread_cleanup_pop(1);

		pthread_cleanup_push(mfpDeleteTaskTP, work_item);
		pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
		work_item->task_handler(work_item->arg);
		pthread_cleanup_pop(1);
	}
	return NULL;
}


thread_pool_task_t* newThreadPoolTask(taskHandler task_handler,	void* arg,
		deleteTaskArg delete_task_arg) {

	thread_pool_task_t* work_item =
		tp_calloc_hdlr(1, sizeof(thread_pool_task_t));
	if (work_item == NULL)
		return NULL;
	work_item->task_handler = task_handler;
	work_item->arg = arg;
	work_item->delete_task_arg = delete_task_arg;
	work_item->delete_thread_pool_task = deleteMfpThreadPoolTask;
	return work_item;
}


static void deleteMfpThreadPoolTask(thread_pool_task_t* work_item) {

	if (work_item->delete_task_arg != NULL)
		work_item->delete_task_arg(work_item->arg);
	free(work_item);
}


static void mfpTPUnlockMutex(void* arg) {

	pthread_mutex_unlock((pthread_mutex_t*)arg);
}


static void mfpDeleteTaskTP(void* arg) {

	thread_pool_task_t* work_item = (thread_pool_task_t*)arg;
	if (work_item != NULL)
		work_item->delete_thread_pool_task(work_item);
}


static void* tp_malloc(uint32_t num) {

	return malloc(num);
}


static void* tp_calloc(uint32_t num, uint32_t size) {

	return calloc(num, size);
}

