#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "mfp_event_timer.h"

//#define TEST1

static void taskArgCleaner(void*);
static void* newTaskArg(uint32_t);
#ifdef TEST1
static void testTaskHandler(void* arg);
#else
static void testTaskHandler_Rec(void* arg); 
#endif

// Global variables
event_timer_t* et = NULL;
uint64_t test_cnt = 0;

static void exitClean(int signal_num);
static void exitClean(int signal_num) {
	printf(" Caught CTRL-C\n");
	exit(1);
}


int main() {

	struct sigaction action_cleanup;
	memset(&action_cleanup, 0, sizeof(struct sigaction));
	action_cleanup.sa_handler = exitClean; 
	action_cleanup.sa_flags = 0;
	sigaction(SIGINT, &action_cleanup, NULL);
	sigaction(SIGTERM, &action_cleanup, NULL);

	et = createEventTimer(NULL, NULL); 
	pthread_t et_thread;
	pthread_create(&et_thread, NULL, et->start_event_timer, (void*)et);
	sleep(2);

#ifdef TEST1
	uint32_t i = 0;
	for (; i < 10; i++) {

		struct timeval now;
		gettimeofday(&now, NULL);
	//	now.tv_usec = (10 - i) * 10000;
		now.tv_sec += 3;
		uint32_t* val = newTaskArg(i);
		et->add_timed_event(et, &now, testTaskHandler, (void*)val); 
	}

#else

	struct timeval now;
	gettimeofday(&now, NULL);
//	now.tv_usec += 500 * 1000;
	now.tv_sec += 3;
	uint32_t* val = newTaskArg(0);
	et->add_timed_event_ms(et, 5, testTaskHandler_Rec, (void*)val); 

#endif

	pthread_join(et_thread, NULL);
	//sleep(20);
	et->delete_event_timer(et);
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


#ifdef TEST1
static void testTaskHandler(void* arg) {

	uint32_t* val_ptr = (uint32_t*)arg;
	printf("Value : %d\n", *val_ptr);
	taskArgCleaner(arg);
//	sleep(3);
}

#else

static void testTaskHandler_Rec(void* arg) {

	uint32_t* val_ptr = (uint32_t*)arg;
	printf("Value : %d\n", *val_ptr);
	taskArgCleaner(arg);

/*	if (test_cnt == 2)
		return;
		*/
	
	test_cnt++;
	uint32_t* val = newTaskArg(test_cnt);
	et->add_timed_event_ms(et, 500, testTaskHandler_Rec, (void*)val); 

}

#endif

