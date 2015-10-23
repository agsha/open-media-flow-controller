/*
 * (C) Copyright 2010 Juniper Networks, Inc
 *
 * Core modeler header file
 *
 * Author: Hrushikesh Paralikar
 *
 */

#ifndef _NKN_CORE_ENGINE_H
#define _NKN_CORE_ENGINE_H

#include <stdint.h>
#include <sys/queue.h>
#include "nkn_am_api.h"
#include "nkn_bm_api.h"
#include "nkn_global_defs.h"

//#define MAX_HOT_VAL 65536

typedef struct am_transfer_data am_transfer_data;
void (*put_in_sched_queue_ptr)(uri_entry_t *);
void (*schedule_get_ptr)(void*);



int scheduler_stop_processing;

TAILQ_HEAD( ,uri_entry_s) sched_queue;


/* Mutex to access the queue */
pthread_mutex_t sched_queue_mutex;

pthread_t logger;
pthread_mutex_t logger_mutex;
pthread_cond_t logger_cond;
void put_in_sched_queue(uri_entry_t *);
void schedule_get(void *);
void logger_thread(void *);
void deref_fetched_buffers(uri_entry_t *);
void copy_to_transfer_object(uri_entry_t *, am_transfer_data_t *);
void ref_buffers_and_copy(uri_entry_t *,am_transfer_data_t *);
void copy_to_stat_resp(uri_entry_t *, DM_stat_resp_t *);
void *signal_handler_fn(void *);
#endif

