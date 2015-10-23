/*============================================================================
 *
 *
 * Purpose: This file defines the public vcs_workq functions and types.
 *
 * Copyright (c) 2002-2012 Juniper Networks. All rights reserved.
 *
 *
 *============================================================================*/
#ifndef __VCS_WORKQ_H__
#define __VCS_WORKQ_H__

#include <pthread.h>

#define vcs_WORKQUEUE_DEFAULT_MAX_THREADS 32
#define vcs_WORKQUEUE_DEFAULT_TIMEOUT 10

typedef struct vcs_workqueue {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int pipefds[2];
    unsigned int current_threads;
    unsigned int idle_threads;
    unsigned int max_threads;
    unsigned int timeout;
    int shutdown;
    pthread_cond_t shutdown_cond;
} vcs_workq_t;

typedef struct vcs_work_item {
    void (*func)(void *);
    void *arg;
} vcs_work_item_t;

int vcs_workq_init(vcs_workq_t *wq);
void vcs_workq_destroy(vcs_workq_t *wq);
int vcs_workq_submit(vcs_workq_t *wq, void (* func)(void *), void *arg);

#endif /* __VCS_WORKQ_H__ */
