#ifndef MFP_THREAD_POOL_H
#define MFP_THREAD_POOL_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>

#include "queue.h"

#ifdef __cplusplus
extern "C" {
#endif

	typedef void (*tp_thread_conf_fptr)(uint32_t thr_id);
	typedef void* (*tp_malloc_fptr)(uint32_t num);
	typedef void* (*tp_calloc_fptr)(uint32_t num, uint32_t size);

	/*
	   struct thread_pool_task encapsulates
	   - Procedure to handle this task (task_handler)
	   - Argument to be passed to the task procedure (args)
	   - Procedure to be called on completion : to free the task resources
	   (so that args can be freed appropriately)
	 */
	struct thread_pool_task;
	typedef void (*taskHandler)(void* arg);
	typedef void (*deleteThreadPoolTask)(struct thread_pool_task*);
	typedef void (*deleteTaskArg)(void*);

	typedef struct thread_pool_task {

		taskHandler task_handler;
		void* arg;
		TAILQ_ENTRY(thread_pool_task) queue_ctxt;
		deleteTaskArg delete_task_arg;
		deleteThreadPoolTask delete_thread_pool_task;
	} thread_pool_task_t;
	thread_pool_task_t* newThreadPoolTask(taskHandler,
			void*, deleteTaskArg);


	/*
	   struct mfp_thread_pool encapsulates,
	   - Work Item Queue
	   - Worker thread array
	   - Pool Lock
	   - Condition variables to signal the availability and completion
	   - Interfaces to add work item and delete the thread_pool
	 */

	struct mfp_thread_pool;
	typedef void (*threadPoolIntf)(struct mfp_thread_pool*);
	typedef void (*addWorkItem)(struct mfp_thread_pool*,
			thread_pool_task_t*);
	typedef uint32_t (*getQueueFillLen)(struct mfp_thread_pool*);

	typedef struct mfp_thread_pool {

		TAILQ_HEAD(, thread_pool_task) work_item_queue;
		uint32_t max_work_queue_len;
		uint32_t queue_fill_len;
		pthread_t* worker_threads;
		uint32_t num_worker_threads;

		pthread_mutex_t thread_pool_lock;
		pthread_cond_t task_posted;
		pthread_cond_t task_taken;

		addWorkItem add_work_item;
		getQueueFillLen get_queue_fill_len;
		threadPoolIntf delete_thread_pool;

	} mfp_thread_pool_t;


	void initThreadPoolLib(tp_malloc_fptr tp_malloc, tp_calloc_fptr tp_calloc,
			tp_thread_conf_fptr tp_thread_conf_hdlr);

	mfp_thread_pool_t* newMfpThreadPool(uint32_t num_worker_threads,
			uint32_t max_work_queue_len);

#ifdef __cplusplus
}
#endif

#endif

