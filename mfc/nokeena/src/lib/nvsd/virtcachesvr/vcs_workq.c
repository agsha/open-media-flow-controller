/*============================================================================
 *
 *
 * Purpose: This file implements vcs_workq functions.
 *
 * Copyright (c) 2002-2012 Juniper Networks. All rights reserved.
 *
 *
 *============================================================================*/
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#include "vcs_workq.h"
#include "nkn_defs.h"

#if defined(DEBUG_WQ)
#include <stdio.h>
#define DEBUG_PRINT(fmt, args ...) do {				\
	fprintf(stderr, "[%s] "fmt, __FUNCTION__, ## args);	\
    } while (0)
#else
#define DEBUG_PRINT(fmt, args ...)
#endif

#define WORKQUEUE_READ_PIPE 0
#define WORKQUEUE_WRITE_PIPE 1

static inline void
vcs_workq_lock(vcs_workq_t *wq)
{
    pthread_mutex_lock(&wq->mutex);
}

static inline void
vcs_workq_unlock(vcs_workq_t *wq)
{
    pthread_mutex_unlock(&wq->mutex);
}

int
vcs_workq_init(vcs_workq_t *wq)
{
    int rc, flags;

    memset(wq, 0, sizeof(vcs_workq_t));

    rc = pipe(wq->pipefds);
    if (rc < 0) {
	DEBUG_PRINT("pipe() failed: %s\n", strerror(errno));
	return rc;
    }

    flags = fcntl(wq->pipefds[WORKQUEUE_READ_PIPE], F_GETFL);
    if (flags < 0) goto error;

    flags |= O_NONBLOCK;
    rc = fcntl(wq->pipefds[WORKQUEUE_READ_PIPE], F_SETFL, flags);
    if (rc < 0) {
	DEBUG_PRINT("fcntl(F_SETFL) failed: %s\n", strerror(errno));
	goto error;
    }

    wq->max_threads = vcs_WORKQUEUE_DEFAULT_MAX_THREADS;
    wq->timeout = vcs_WORKQUEUE_DEFAULT_TIMEOUT;

    pthread_mutex_init(&wq->mutex, NULL);
    pthread_cond_init(&wq->cond, NULL);
    pthread_cond_init(&wq->cond, NULL);

    return 0;

error:
    rc = errno;
    close(wq->pipefds[WORKQUEUE_READ_PIPE]);
    close(wq->pipefds[WORKQUEUE_WRITE_PIPE]);
    errno = rc;
    return -1;
}

void
vcs_workq_destroy(vcs_workq_t *wq)
{
    int rc = 0;

    vcs_workq_lock(wq);
    wq->shutdown = 1;
    pthread_cond_broadcast(&wq->cond);
    close(wq->pipefds[WORKQUEUE_READ_PIPE]);
    close(wq->pipefds[WORKQUEUE_WRITE_PIPE]);
    while (wq->current_threads > 0 && rc == 0) {
	rc = pthread_cond_wait(&wq->shutdown_cond, &wq->mutex);
    }
    vcs_workq_unlock(wq);
}

static int
vcs_workq_wait(vcs_workq_t *wq, vcs_work_item_t *item)
{
    int rc;

    while (1) {
	if (wq->shutdown)
	    return -1;

	/* This read is atomic as long as sizeof(vcs_work_item_t) <= PIPE_BUF */
	rc = read(wq->pipefds[WORKQUEUE_READ_PIPE], item, sizeof(vcs_work_item_t));
	if (rc < 0 && errno != EWOULDBLOCK) {
	    DEBUG_PRINT("read() failed: %s\n", strerror(errno));
	    return errno;
	}
	if (rc == sizeof(vcs_work_item_t))
	    break;

	if (wq->timeout) {
	    struct timespec ts;
	    clock_gettime(CLOCK_REALTIME, &ts);
	    ts.tv_sec += wq->timeout;
	    rc = pthread_cond_timedwait(&wq->cond, &wq->mutex, &ts);
	    if (rc == ETIMEDOUT) {
		DEBUG_PRINT("timeout.\n");
		return rc;
	    }
	} else {
	    rc = pthread_cond_wait(&wq->cond, &wq->mutex);
	}
	if (rc != 0) {
	    DEBUG_PRINT("%s\n", strerror(rc));
	    return rc;
	}
    }
    return 0;
}

static void *
vcs_workq_worker(void *arg)
{
    vcs_workq_t *wq = (vcs_workq_t *)arg;

    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

    while (1) {
	int rc = 0;
	vcs_work_item_t item;
	
	vcs_workq_lock(wq);

	rc = vcs_workq_wait(wq, &item);
	if (rc != 0)
	    break;

	wq->idle_threads--;
	vcs_workq_unlock(wq);

	item.func(item.arg);

	vcs_workq_lock(wq);
	wq->idle_threads++;
	vcs_workq_unlock(wq);
    }

    wq->idle_threads--;
    wq->current_threads--;
    DEBUG_PRINT("thread exiting: current=%d\n", wq->current_threads);
    pthread_cond_signal(&wq->shutdown_cond);
    vcs_workq_unlock(wq);

    return NULL;
}

int
vcs_workq_submit(vcs_workq_t *wq, void (* func)(void *), void *arg)
{
    int rc;
    vcs_work_item_t item;

    if (wq == NULL || func == NULL) {
	errno = EINVAL;
	return -1;
    }

    item.func = func;
    item.arg = arg;

    vcs_workq_lock(wq);
    if (wq->idle_threads == 0 && wq->current_threads < wq->max_threads) {
	pthread_t t;
	sigset_t set, oldset;

	sigfillset(&set);
	sigprocmask(SIG_BLOCK, &set, &oldset);

	pthread_create(&t, NULL, vcs_workq_worker, wq); /* best-effort */
	wq->idle_threads++;
	wq->current_threads++;
	DEBUG_PRINT("thread created: current=%d\n", wq->current_threads);

	sigprocmask(SIG_SETMASK, &oldset, NULL);
    }
    vcs_workq_unlock(wq);
    
    /* This write is guaranteed to be atomic. */
    rc = write(wq->pipefds[WORKQUEUE_WRITE_PIPE], &item, sizeof(item));
    if (rc < 0) return rc;

    pthread_cond_signal(&wq->cond);

    return 0;
}

