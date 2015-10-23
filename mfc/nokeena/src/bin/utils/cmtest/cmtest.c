/*
 * Unit test for the Cache Manager module.
 * Copyright 2008, Nokeena Networks Inc.
 */
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <err.h>
#include "nkn_defs.h"
#include "cache_mgr.h"
#include "nkn_mediamgr_api.h"
#include "nkn_sched_api.h"
#include "nkn_attr.h"

#define URL_FILE "urls"
#define MAXREQ 1000000
struct req {
	char *url;
	off_t off, len;
};
static struct req *reqs;
static int num_urls = 0;

extern void cache_manager_init(void);

static void req_reserve(void);
static void req_release(void);
void om_get_input(nkn_task_id_t tid);
void om_get_output(nkn_task_id_t tid);
void om_get_cleanup(nkn_task_id_t tid);
int OM_stat(nkn_uol_t uol, MM_stat_resp_t *resp);
void http_mgr_input(nkn_task_id_t tid);
void http_mgr_output(nkn_task_id_t tid);
void http_mgr_cleanup(nkn_task_id_t tid);
void disk_io_req_hdl(gpointer data, gpointer user_data);
void media_mgr_input(nkn_task_id_t tid);
void media_mgr_output(nkn_task_id_t tid);
void media_mgr_cleanup(nkn_task_id_t tid);
void disk_mgr_input(nkn_task_id_t tid);
void disk_mgr_output(nkn_task_id_t tid);
void disk_mgr_cleanup(nkn_task_id_t tid);
void start_request(void);

#define MINTASKS 50
#define MAXTASKS 100
pthread_cond_t cv;
pthread_mutex_t lock;
static int task_wait=0, num_tasks=0, max_tasks=MAXTASKS, min_tasks=MINTASKS;

#define MAX_BUFS_PER_TASK 16

extern unsigned long cm_max_cache_size;

/* Simulate OM */
void 
om_get_input(nkn_task_id_t tid)
{
	err(0, "stub handler called\n");
}

void 
om_get_output(nkn_task_id_t tid)
{
	err(0, "stub handler called\n");
}

void 
om_get_cleanup(nkn_task_id_t tid)
{
	err(0, "stub handler called\n");
}

int 
OM_stat(nkn_uol_t uol, MM_stat_resp_t *resp)
{
	return 0;
}

/* 
 * Temporarily defined here until we take out the hardwired initialization in
 * the scheduler.
 */
void 
http_mgr_input(nkn_task_id_t tid)
{
	err(0, "stub handler called\n");
}

void 
http_mgr_output(nkn_task_id_t tid)
{
	cache_response_t *getr;

	getr = (cache_response_t *)nkn_task_get_data(tid);

	/* Do some validation on the buffers received */
	assert(getr->errcode == 0);
	assert(getr->length_response <= getr->uol.length);
	assert(getr->content[0].iov_base);
	assert(getr->content[0].iov_len);
	if (getr->attr) {
		void *val;
		nkn_attr_len_t len;
		nkn_attr_id_t id;

		id.u.attr_id = 10;
		val = nkn_attr_get(getr->attr, id, &len);
		assert(val);
		id.u.attr_id = 7;
		val = nkn_attr_get(getr->attr, id, &len);
		assert(val);
	}
	free(getr);
	nkn_task_set_action_and_state(tid, TASK_ACTION_CLEANUP,
					TASK_STATE_RUNNABLE);
	req_release();
}

void 
http_mgr_cleanup(nkn_task_id_t tid)
{
	err(0, "stub handler called\n");
}

/* Simulated Disk Manager */
#define MIN_LEN 20345
#define MAX_LEN 1123456789
int DM_stat(nkn_uol_t uol, MM_stat_resp_t *resp)
{
	snprintf(resp->physid, NKN_MAX_FILE_NAME_SZ, "%s", uol.uri);
	resp->tot_content_len = (rand() + MIN_LEN) % MAX_LEN;
	resp->content_len = rand() % CM_MEM_PAGE_SIZE;
	resp->media_blk_len = CM_MEM_PAGE_SIZE; 	/* one page for now */
	return 0;
}

static GAsyncQueue *ioq;
typedef struct {
	nkn_task_id_t id;
} ioq_t;

static void *
ioq_handler(void *ptr)
{
	ioq_t *q;

	while (1) {
		nkn_task_id_t tid;
		MM_get_resp_t *gr;
		nkn_uol_t uol;
		int i;

		q = (ioq_t *)g_async_queue_pop(ioq);

		usleep(10000);
		tid = q->id;
		gr = nkn_task_get_data(tid);

		uol.uri = gr->in_uol.uri;
		uol.offset = gr->in_uol.offset / CM_MEM_PAGE_SIZE;
		uol.length = CM_MEM_PAGE_SIZE;
		for (i=0; i<(int)gr->in_num_content_objects; i++) {
			nkn_buffer_setid(gr->in_content_array[i], &uol, 0, 0);
			uol.offset += CM_MEM_PAGE_SIZE;
		}
		if (gr->in_attr) {
			nkn_attr_t *ap;
			nkn_attr_id_t id;

			ap = (nkn_attr_t *)nkn_buffer_getcontent(gr->in_attr);
			nkn_attr_init(ap);
			ap->content_length = 2345678;
			id.u.attr_id = 7;
			nkn_attr_set(ap, id, (nkn_attr_len_t)14, (void*)"this is attr 7");
			id.u.attr_id = 10;
			nkn_attr_set(ap, id, (nkn_attr_len_t)19, (void*)"attribute number 10");
			nkn_buffer_setid(gr->in_attr, &uol, 0, 0);
		}
		gr->out_num_content_objects = gr->in_num_content_objects;
		nkn_task_set_action_and_state(tid, TASK_ACTION_OUTPUT,
						TASK_STATE_RUNNABLE);

		free(q);
	}
}

int
DM_init()
{
	pthread_t thr;

	ioq = g_async_queue_new();
	pthread_create(&thr, NULL, ioq_handler, 0);
	return 0;
}

void
disk_io_req_hdl(gpointer data, gpointer user_data)
{
}

void
media_mgr_input(nkn_task_id_t tid)
{
}

void 
media_mgr_output(nkn_task_id_t tid)
{
	err(0, "stub handler called\n");
}

void 
media_mgr_cleanup(nkn_task_id_t tid)
{
}

void 
disk_mgr_input(nkn_task_id_t tid)
{
	ioq_t *q = (ioq_t *)calloc(1, sizeof(ioq_t));

	if (!q)
		err(1, "Failed to allocate io q element\n");
	q->id = tid;
	g_async_queue_push(ioq, q);
	nkn_task_set_state(tid, TASK_STATE_EVENT_WAIT);	
}

void 
disk_mgr_output(nkn_task_id_t tid)
{
	err(0, "stub handler called\n");
}

void 
disk_mgr_cleanup(nkn_task_id_t tid)
{
}

static void
req_release()
{
	pthread_mutex_lock(&lock);
	if (num_tasks <= 0)
		err(0, "Number of tasks went negative\n");
	num_tasks--;
	if ((num_tasks < min_tasks) && task_wait) {
		task_wait = 0;
		pthread_cond_signal(&cv);	
	}
	pthread_mutex_unlock(&lock);	
}

static void
req_reserve()
{
	pthread_mutex_lock(&lock);
	while (num_tasks >= max_tasks) {
		task_wait = 1;
		pthread_cond_wait(&cv, &lock);
	}

	num_tasks++;
	pthread_mutex_unlock(&lock);	
}

void start_request(void)
{
	int r = rand() % num_urls;
	struct req *myreq = &reqs[r];
	nkn_task_id_t tid;

	nkn_uol_t uol;
	cache_response_t *getr;

	req_reserve();

	uol.uri = myreq->url;
	uol.offset = myreq->off;
	uol.length = myreq->len;

	getr = (cache_response_t *)calloc(1, sizeof(cache_response_t));	
	if (getr == NULL)
		err(1, "Failed to allocated cache resp\n");

	getr->magic = CACHE_RESP_REQ_MAGIC;
	getr->uol = uol;
	getr->in_rflags |= CR_RFLAG_GETATTR;

	tid = nkn_task_create_new_task(
		TASK_TYPE_CHUNK_MANAGER, 
		TASK_TYPE_HTTP_SRV,
		TASK_ACTION_INPUT,
		0,
		(void *)getr,
		sizeof(getr),
		100);			/* deadline = 100 msecs */

	if (tid < 0)
		err(0, "Failed to create task\n");

	nkn_task_set_state(tid, TASK_STATE_RUNNABLE);
	return;
}

#define MAXURL 1000
static void
read_url_file(const char *fn)
{
	FILE *fp;
	char url[MAXURL];
	off_t off, len;
	int ret;

	fp = fopen(fn, "r");
	if ( fp == (FILE*) 0 )
		err(1, "Failed to open URL file %s\n", fn);
	
	reqs = calloc(MAXREQ, sizeof(struct req));
	if (reqs == NULL)
		err(1, "Failed to allocate memory for URL list\n");

	while ((ret = fscanf(fp, "%s %ld %ld\n", url, &off, &len)) != EOF) {
		if (ret !=3)
			err(1, "Misformated URL file\n");
		reqs[num_urls].url = strdup(url);
		if (reqs[num_urls].url == NULL)
			err(1, "Failed to allocate URL string\n");
		reqs[num_urls].off = off;
		reqs[num_urls].len = len;
		num_urls++;
		if (num_urls == MAXREQ) {
			warn("URL file too long\n");
			break;
		}
	}
}

int main(int argc, char **argv)
{
	int ret, last, total;
	struct timeval now, prev;

	/* Set max cache size */
	cm_max_cache_size = 256;

	/* Process args, including getting the URL list */
	read_url_file(URL_FILE);

	/* Initialize modules - Scheduler and MM */
	g_thread_init(NULL);
	ret = nkn_scheduler_init(NULL, NKN_SCHED_START_MMTHREADS);
	if (ret < 0)
		err(0, "Failed to initialize scheduler\n");
	cache_manager_init();
	DM_init();

	/* Register our test module - using Chunk Mgr for now
	ret = nkn_task_register_task_type(mmt_mod, mmt_input, mmt_output, mmt_cleanup);
	if (ret)
		err(0, "failed to register mmt module\n");
	*/

	pthread_mutex_init(&lock, NULL);
	pthread_cond_init(&cv, NULL);

	/* Issue Requests */
	gettimeofday(&prev, NULL);
	last = 0;
	total = 0;
	while (1) {
		start_request();
		total++;
                gettimeofday(&now, NULL);
                if (now.tv_sec > prev.tv_sec) {
                        printf("%d reqs\n", total-last);
                        last = total;
                        prev = now;
                }
	}
	return 0;
}
