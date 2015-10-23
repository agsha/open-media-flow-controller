#include "mfp_event_timer.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <string.h>

//// struct used locally with GTree {
struct event_info;

typedef void (*delete_event_info_fptr)(struct event_info* e_info);

typedef struct event_info {

	void* ctx;
	timed_event_hdlr_fptr timed_event_hdlr;

	delete_event_info_fptr delete_event_info;
} event_info_t;

static event_info_t* createEventInfo(void* ctx,
		timed_event_hdlr_fptr timed_event_hdlr,
		mem_alloc_fptr mem_alloc);

static void deleteEventInfo(event_info_t* e_info);
//// }

static void* startEventTimer(void* ctx); 

static void stopEventTimer(void* ctx); 

static int32_t addTimedEvent(event_timer_t* et, struct timeval const* e_time,
		        timed_event_hdlr_fptr hdlr, void* ctx);

static int32_t addTimedEvent_MS(event_timer_t* et, uint32_t ms_diff,
		timed_event_hdlr_fptr hdlr, void* ctx); 

static void deleteEventTimer(struct event_timer*);

static int32_t diffTimevalToMs(struct timeval const*, struct timeval const *);

static struct timeval* newTimevalKey(struct timeval const*);

static gboolean updateTimeout(gpointer key,
		gpointer value, gpointer data);

static void* eventTimerAlloc(uint32_t count, uint32_t size);

// Callbacks registered with glib structs
static gint timevalCompare(gconstpointer, gconstpointer, gpointer);
static void deleteTimeval(gpointer);


event_timer_t* createEventTimer(mem_alloc_fptr mem_alloc, 
		et_thread_conf_fptr thread_conf_hdlr) {

	if (mem_alloc == NULL)
		mem_alloc = eventTimerAlloc;
	event_timer_t* et = mem_alloc(1, sizeof(event_timer_t));
	if (et == NULL)
		return NULL;
	pthread_condattr_t a;

	et->mem_alloc = mem_alloc;
	et->start_flag = 0;
	et->stop_flag = -1;
	et->timeout_avl = -1;
	et->curr_ev_ts.tv_sec = 0;
	et->curr_ev_ts.tv_usec = 0;

	pthread_mutex_init(&et->lock, NULL);
	pthread_condattr_init(&a);
	pthread_condattr_setclock(&a, CLOCK_MONOTONIC);
	pthread_cond_init(&et->cond, &a);

	et->mem_alloc = mem_alloc;
	et->thread_conf_cb = thread_conf_hdlr;
    et->events = g_tree_new_full(timevalCompare, NULL, deleteTimeval, NULL);

	et->start_event_timer = startEventTimer;
	et->add_timed_event = addTimedEvent;
	et->add_timed_event_ms = addTimedEvent_MS;
	et->delete_event_timer = deleteEventTimer;

	return et;
}


static void* startEventTimer(void* ctx) {

	event_timer_t* et = (event_timer_t*)ctx;
	if (et->thread_conf_cb != NULL)
		et->thread_conf_cb(0);
	et->id = pthread_self();
	et->start_flag = 1;
	struct timeval tv;
	struct timespec ts, ts1;
	
	prctl(PR_SET_NAME, "event-timer-th", 0, 0, 0);	
	pthread_mutex_lock(&et->lock);
	while (1) {

		if (et->stop_flag == 1)
			break;
		event_info_t* e_info = NULL;
		et->timeout_avl = -1;
		g_tree_traverse(et->events, updateTimeout, G_IN_ORDER, et);
		if (et->timeout_avl >= 0) {
		    //gettimeofday(&tv, NULL);
			clock_gettime(CLOCK_MONOTONIC, &ts1);
			tv.tv_sec = ts1.tv_sec;
			tv.tv_usec = ts1.tv_nsec/1000;
			uint32_t diff =
			    diffTimevalToMs(&et->curr_ev_ts, 
					    &tv);
			//printf("diff: %u\n", diff);
			if (diff <= 0) {
				e_info = g_tree_lookup(et->events, &et->curr_ev_ts);
				g_tree_remove(et->events, &et->curr_ev_ts);
			}
		}
		pthread_mutex_unlock(&et->lock);
		if (e_info != NULL) {
			e_info->timed_event_hdlr(e_info->ctx);
			e_info->delete_event_info(e_info);
		}
		pthread_mutex_lock(&et->lock);
		et->timeout_avl = -1;
		g_tree_traverse(et->events, updateTimeout, G_IN_ORDER, et);
		if (et->timeout_avl != -1) {
			ts.tv_sec = et->curr_ev_ts.tv_sec;  
			ts.tv_nsec = (et->curr_ev_ts.tv_usec * 1000); 
#if 0
			printf("timed wait %ld(s) %ld(us)\n", 
			   et->curr_ev_ts.tv_sec,
			   et->curr_ev_ts.tv_usec);
#endif
			pthread_cond_timedwait(&et->cond, &et->lock, &ts);
			//printf("rc: %d\n", rc);
		} else {
			//printf("No events: cond_wait\n");
			pthread_cond_wait(&et->cond, &et->lock);
		}
	}
	pthread_mutex_unlock(&et->lock);
	return NULL;
}


static void stopEventTimer(void* ctx) {

	event_timer_t* et = (event_timer_t*)ctx;
	pthread_mutex_lock(&et->lock);
	et->stop_flag = 1;
	pthread_cond_signal(&et->cond);
	pthread_mutex_unlock(&et->lock);
}


static int32_t addTimedEvent(event_timer_t* et, struct timeval const* e_time,
		timed_event_hdlr_fptr hdlr, void* ctx) {

	if (et->start_flag == 0)
		return -1;
	if (et->stop_flag == 0)
		return -2;
	pthread_mutex_lock(&et->lock);
	int32_t rc = -1;
	event_info_t* e_info = createEventInfo(ctx, hdlr, et->mem_alloc);
    struct timeval* key = newTimevalKey(e_time);
	if (key) {
		while (1) {
			event_info_t* tmp_e_info = g_tree_lookup(et->events, key);
			if (!tmp_e_info)
				break;
			key->tv_usec += 1;
		}
		g_tree_insert(et->events, key, e_info);
		rc = 1;
		if (pthread_equal(et->id, pthread_self()) == 0)
			pthread_cond_signal(&et->cond);
	}
	pthread_mutex_unlock(&et->lock);
	return rc;
}


static int32_t addTimedEvent_MS(event_timer_t* et, uint32_t ms_diff,
		timed_event_hdlr_fptr hdlr, void* ctx) {

	if (et->start_flag == 0)
		return -1;
	if (et->stop_flag == 0)
		return -2;
	pthread_mutex_lock(&et->lock);
	int32_t rc = -1;

	struct timeval e_time;
	struct timespec st;
	memset(&e_time, 0, sizeof(struct timeval));
	clock_gettime(CLOCK_MONOTONIC, &st);
	uint32_t us_diff = ms_diff * 1000 + (st.tv_nsec / 1000);
	uint32_t sec_com = us_diff / 1000000;
	uint32_t usec_com = us_diff % 1000000;
	e_time.tv_sec = st.tv_sec + sec_com;
	e_time.tv_usec = usec_com;

	event_info_t* e_info = createEventInfo(ctx, hdlr, et->mem_alloc);
    struct timeval* key = newTimevalKey(&e_time);
	if (key) {
		while (1) {
			event_info_t* tmp_e_info = g_tree_lookup(et->events, key);
			if (!tmp_e_info)
				break;
			key->tv_usec += 1;
		}
		g_tree_insert(et->events, key, e_info);
		rc = 1;
		if (pthread_equal(et->id, pthread_self()) == 0)
			pthread_cond_signal(&et->cond);
	}
	pthread_mutex_unlock(&et->lock);
	return rc;
}



static void deleteEventTimer(struct event_timer* et) {

	stopEventTimer(et);
	g_tree_destroy(et->events);
	pthread_cond_destroy(&et->cond);
	pthread_mutex_destroy(&et->lock);
	free(et);
}


static gint timevalCompare(gconstpointer gtv1, gconstpointer gtv2,
		gpointer user_data) {

	struct timeval* val1 = (struct timeval*)gtv1;
	struct timeval* val2 = (struct timeval*)gtv2;

	if (val1->tv_sec < val2->tv_sec)
		return -1;
	else if (val1->tv_sec > val2->tv_sec)
		return 1;
	else {
		if (val1->tv_usec < val2->tv_usec)
			return -1;
		else if (val1->tv_usec > val2->tv_usec)
			return 1;
		else
			return 0;
	}
}


static struct timeval* newTimevalKey(struct timeval const* val) {

	struct timeval* key = malloc(sizeof(struct timeval));
	if (key) {
		key->tv_sec = val->tv_sec;
		key->tv_usec = val->tv_usec;
	}
	return key;
}


static void deleteTimeval(gpointer ptr) {

	struct timeval* val = (struct timeval*)ptr;
	free(val);
}


static gboolean updateTimeout(gpointer key,
		gpointer value, gpointer data) {

	struct timeval* val = (struct timeval*)key;
	event_timer_t* et = (event_timer_t*)data;
	et->timeout_avl = 1;
	et->curr_ev_ts.tv_sec = val->tv_sec;
	et->curr_ev_ts.tv_usec = val->tv_usec;
	return TRUE;
}


static int32_t diffTimevalToMs(struct timeval const* from,
		struct timeval const * val) {
	//if -ve, return 0
	double d_from = from->tv_sec + ((double)from->tv_usec)/1000000;
	double d_val = val->tv_sec + ((double)val->tv_usec)/1000000;
	double diff = d_from - d_val;
	if (diff < 0)
		return 0;
	return (int32_t)(diff * 1000);
}


static void* eventTimerAlloc(uint32_t count, uint32_t size) {

	return calloc(count, size);
}


/////
static event_info_t* createEventInfo(void* ctx,
		timed_event_hdlr_fptr timed_event_hdlr,
		mem_alloc_fptr mem_alloc) {

	event_info_t* e_info = mem_alloc(1, sizeof(event_info_t));
	if (e_info == NULL)
		return NULL;

	e_info->ctx = ctx;
	e_info->timed_event_hdlr = timed_event_hdlr;
	e_info->delete_event_info = deleteEventInfo;
	return e_info;
}


static void deleteEventInfo(event_info_t* e_info) {

	free(e_info);
}

