#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "mfp_thread_pool.h"

#define THREADPOOL_USER_ALLOC

static void taskArgCleaner(void*);
static void* newTaskArg(uint32_t);
static void testTaskHandler(void* arg);

// Global variables
mfp_thread_pool_t* thread_pool = NULL;

static void exitClean(int signal_num);
static void exitClean(int signal_num) {
	printf(" Caught CTRL-C\n");
	//thread_pool->delete_thread_pool(thread_pool);
	exit(1);
}



int main() {

	struct sigaction action_cleanup;
	memset(&action_cleanup, 0, sizeof(struct sigaction));
	action_cleanup.sa_handler = exitClean; 
	action_cleanup.sa_flags = 0;
	sigaction(SIGINT, &action_cleanup, NULL);
	sigaction(SIGTERM, &action_cleanup, NULL);

	thread_pool = newMfpThreadPool(10, 1000);//10 threads 1000 tasks

	uint32_t i = 0;
	for (; i < 200; i++) {

		thread_pool_task_t* work_item =
			newThreadPoolTask(testTaskHandler,
				newTaskArg(i), taskArgCleaner);
		thread_pool->add_work_item(thread_pool, work_item);
	}
	sleep(30);
	thread_pool->delete_thread_pool(thread_pool);
	return 0;
}


static void taskArgCleaner(void* arg) {

	uint32_t* val_ptr = (uint32_t*)arg;
	free(val_ptr);
}


static void* newTaskArg(uint32_t val) {

	uint32_t* arg = malloc(sizeof(uint32_t));
	*arg = val;
	return (void*)arg;
}


static void testTaskHandler(void* arg) {

	uint32_t* val_ptr = (uint32_t*)arg;
	printf("Value : %d\n", *val_ptr);
	sleep(2);
}


void* threadpool_calloc_custom(int32_t num, int32_t size) {

	return calloc(num, size);
}


void* threadpool_malloc_custom(int32_t size) {

	return malloc(size);
}
