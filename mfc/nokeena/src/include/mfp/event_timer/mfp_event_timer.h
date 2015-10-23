#ifndef MFP_EVENT_TIMER_H
#define MFP_EVENT_TIMER_H

#include <stdint.h>
#include <sys/time.h>
#include <pthread.h>
#include <glib.h>

#ifdef __cpluplus
extern "C" {
#endif

struct event_timer;

typedef void (*et_thread_conf_fptr)(uint32_t thr_id);

typedef void* (*mem_alloc_fptr)(uint32_t count, uint32_t size);

typedef void (*timed_event_hdlr_fptr)(void* ctx);

typedef void* (*start_event_timer_fptr)(void* et);

typedef int32_t (*add_timed_event_fptr)(struct event_timer* et,
		struct timeval const*, timed_event_hdlr_fptr hdlr, void* ctx);

typedef int32_t (*add_timed_event_ms_fptr)(struct event_timer* et,
		uint32_t ms_diff, timed_event_hdlr_fptr hdlr, void* ctx);

typedef void (*delete_event_timer_fptr)(struct event_timer*);

typedef struct event_timer {

	GTree* events;

	int32_t stop_flag;
	int32_t start_flag;
	int32_t timeout_avl;
	struct timeval curr_ev_ts;

	pthread_mutex_t lock;
	pthread_cond_t cond;
	pthread_t id;

	mem_alloc_fptr mem_alloc;
	et_thread_conf_fptr thread_conf_cb;

	start_event_timer_fptr start_event_timer;
	add_timed_event_fptr add_timed_event;
	add_timed_event_ms_fptr add_timed_event_ms;
	delete_event_timer_fptr delete_event_timer;
} event_timer_t;


event_timer_t* createEventTimer(mem_alloc_fptr mem_alloc, 
		et_thread_conf_fptr);

#ifdef __cpluplus
}
#endif
#endif
