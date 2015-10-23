/*
 * Unit test for the Media Manager module.
 * Copyright 2008, Nokeena Networks Inc.
 */
#include <sys/types.h>
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

#define URL_FILE "urls"
#define MAXREQ 1000000
struct req {
	char *url;
	off_t off, len;
};
static struct req *reqs;
static int num_urls = 0;

static void req_reserve(void);
static void req_release(void);

#define MINTASKS 50
#define MAXTASKS 100
pthread_cond_t cv;
pthread_mutex_t lock;
static int task_wait=0, num_tasks=0, max_tasks=MAXTASKS, min_tasks=MINTASKS;

#define MAX_BUFS_PER_TASK 16

/* buffers to pass to MM_get tasks */
typedef struct {
	int err;
	nkn_uol_t uol;
	void *page;
} mm_buf_t;


static mm_buf_t * mm_buf_alloc(void);
void http_mgr_input(nkn_task_id_t tid);
void http_mgr_output(nkn_task_id_t tid);
void http_mgr_cleanup(nkn_task_id_t tid);
void chunk_mgr_input(nkn_task_id_t tid);
void chunk_mgr_output(nkn_task_id_t tid);
void chunk_mgr_cleanup(nkn_task_id_t tid);
void cm_set_video_obj_uol(void *p, nkn_uol_t *id);
void cm_set_video_obj_content_available(void *p);
void cache_manager_init(void);
void * cm_get_video_obj_content_ptr(void *p);
void cm_set_video_obj_errcode(void *p, int code);
void c_sleeper_q_tbl_init(void);
void start_request(void) ;

extern void nkn_task_free_task(nkn_task_id_t a_in_task_id);

static mm_buf_t *
mm_buf_alloc(void)
{
	mm_buf_t *bp;

	bp = calloc(1, sizeof(mm_buf_t));
	if (bp == NULL)
		err(1, "Failed to allocate buf structure\n");

	bp->page = malloc(CM_MEM_PAGE_SIZE);
	if (bp == NULL)
		err(1, "Failed to allocate buf page\n");
	return bp;
}

static void
mm_buf_free(mm_buf_t *bp)
{
	if (bp->uol.uri)
		free(bp->uol.uri);
	free(bp->page);
	free(bp);
}

static void **
mm_bufarray_alloc(int n)
{
	int i;
	mm_buf_t **bpp = calloc(n, sizeof(mm_buf_t *));
	
	if (bpp == NULL)
		err(1, "Faiked to allocate buf array\n");
	for (i=0; i<n; i++) {
		bpp[i] = mm_buf_alloc();
	}
	return (void **)bpp;
}

static void
mm_bufarray_free(void **p, int n)
{
	int i;
	mm_buf_t **bpp = (mm_buf_t **)p;
	
	for (i=0; i<n; i++)
		mm_buf_free(bpp[i]);
	free(bpp);
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
	err(0, "stub handler called\n");
}

void 
http_mgr_cleanup(nkn_task_id_t tid)
{
	err(0, "stub handler called\n");
}

/* Simulated Chunk Manager */
void 
chunk_mgr_input(nkn_task_id_t tid)
{
	err(0, "stub handler called\n");
}

void 
chunk_mgr_output(nkn_task_id_t tid)
{
	MM_get_resp_t *getr;
	int i, found=0;

	getr = (MM_get_resp_t *)nkn_task_get_data(tid);

	/* Do some validation on the buffers received */
	assert(getr->out_num_content_objects <= getr->in_num_content_objects);
	for (i = 0; i < (int)getr->out_num_content_objects; i++) {
		 mm_buf_t *bp = (mm_buf_t *)getr->in_content_array[i];

		if (bp->uol.uri &&
		    !strncmp(getr->in_uol.uri, bp->uol.uri, NKN_MAX_FILE_NAME_SZ) && 
		    ((getr->in_uol.offset%CM_MEM_PAGE_SIZE) == (bp->uol.offset%CM_MEM_PAGE_SIZE)))
			found++;
		assert(bp->uol.length <= CM_MEM_PAGE_SIZE);
	}
	mm_bufarray_free(getr->in_content_array, getr->in_num_content_objects);
	free(getr);
	nkn_task_free_task(tid);
	req_release();
}

void 
chunk_mgr_cleanup(nkn_task_id_t tid)
{
	err(0, "stub handler called\n");
}

void cache_manager_init(void) {}

void
cm_set_video_obj_uol(void *p, nkn_uol_t *id)
{
	mm_buf_t *bp = (mm_buf_t *)p;

	bp->uol.uri = strdup(id->uri);
	if ( bp->uol.uri == NULL)
		err(1, "Failed to dup uri\n");
	bp->uol.offset = id->offset;
	bp->uol.length = id->length;
}

void
cm_set_video_obj_content_available(void *p)
{
}

void *
cm_get_video_obj_content_ptr(void *p)
{
	mm_buf_t *bp = (mm_buf_t *)p;

	return bp->page;
}

void
cm_set_video_obj_errcode(void *p, int code)
{
	mm_buf_t *bp = (mm_buf_t *)p;

	bp->err = code;
}

void
c_sleeper_q_tbl_init(void)
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
	int ret, num_bufs, r = rand() % num_urls;
	struct req *myreq = &reqs[r];
	nkn_task_id_t tid;

	nkn_uol_t uol;
	MM_stat_resp_t resp;
	MM_get_resp_t *getr;

	req_reserve();

	uol.uri = myreq->url;
	uol.offset = myreq->off;
	uol.length = myreq->len;

	ret = DM_stat(uol, &resp);
	if (ret < 0) {
		printf("DM_stat failed for %s\n", uol.uri);
		req_release();
		return;
	}

	/* Allocate buffer needed to do the get */
	num_bufs = resp.media_blk_len / CM_MEM_PAGE_SIZE;
	if ((num_bufs > MAX_BUFS_PER_TASK) || (num_bufs < 1)) {
		printf("Bad blocksize %d for URL %s\n", resp.media_blk_len, uol.uri);
		req_release();
		return;
	}

	getr = calloc(1, sizeof(MM_get_resp_t));	
	if (getr == NULL)
		err(1, "Failed to allocated get resp\n");

	strncpy(getr->physid, resp.physid, NKN_MAX_FILE_NAME_SZ);
	getr->in_uol = uol;
	getr->in_num_content_objects = num_bufs;
	getr->in_content_array = mm_bufarray_alloc(num_bufs);

	tid = nkn_task_create_new_task(
		TASK_TYPE_DISK_MANAGER, 
		TASK_TYPE_CHUNK_MANAGER,
		TASK_ACTION_INPUT,
		DM_OP_READ,
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
read_url_file(char *fn)
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
	int ret;

	/* Process args, including getting the URL list */
	read_url_file(URL_FILE);

	/* Initialize modules - Scheduler and MM */
	g_thread_init(NULL);
	ret = nkn_scheduler_init();
	if (ret < 0)
		err(0, "Failed to initialize scheduler\n");
	DM_init();

	/* Register our test module - using Chunk Mgr for now
	ret = nkn_task_register_task_type(mmt_mod, mmt_input, mmt_output, mmt_cleanup);
	if (ret)
		err(0, "failed to register mmt module\n");
	*/

	pthread_mutex_init(&lock, NULL);
	pthread_cond_init(&cv, NULL);

	/* Issue Requests */
	while (1) {
		start_request();
	}
	return 0;
}
