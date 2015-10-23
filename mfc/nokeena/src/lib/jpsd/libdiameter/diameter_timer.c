/*
 * @file diameter_timer.c
 * @brief
 * diameter_timer.c - definations for diameter timer functions.
 *
 * Yuvaraja Mariappan, Dec 2014
 *
 * Copyright (c) 2015, Juniper Networks, Inc.
 * All rights reserved.
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <atomic_ops.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/prctl.h>
#include <sys/queue.h>
#include <sys/uio.h>
#include <sys/socket.h>

#include "nkn_memalloc.h"
#include "nkn_stat.h"

#include "diameter_timer.h"

#define DIAMETER_MAX_TOQ_SLOT 300


NKNCNT_DEF(dia_timer_tot, uint64_t, "", "Total Timers");
NKNCNT_DEF(dia_timer_cur, uint64_t, "", "Total Active Timers");

static int diameter_cur_toq;

static pthread_mutex_t diameter_timer_mutex;

typedef struct diameter_timer_s {
	uint8_t active;
	uint16_t type;
	void *private_data;
	LIST_ENTRY(diameter_timer_s) toq_entry;
} diameter_timer_t;

LIST_HEAD(diameter_timeout_queue, diameter_timer_s);

static struct diameter_timeout_queue diameter_toq_head[DIAMETER_MAX_TOQ_SLOT];

static uint32_t diameter_register_timer(diameter_timer_handle h,
			uint32_t timeout, int locked)
{
	int timeout_slot;
	int slot;
	diameter_timer_t *timeout_entry = (diameter_timer_t *)h;

	/* Time out slot is 2 sec */
	timeout_slot = (timeout / 2000) + 1;
	if (timeout_slot < 3)
		timeout_slot = 3;

	if (timeout_slot > DIAMETER_MAX_TOQ_SLOT)
		timeout_slot = DIAMETER_MAX_TOQ_SLOT - 2;

	if (locked == 0)
		pthread_mutex_lock(&diameter_timer_mutex);

	slot = (diameter_cur_toq + timeout_slot) % DIAMETER_MAX_TOQ_SLOT;
	LIST_INSERT_HEAD(&(diameter_toq_head[slot]), timeout_entry, toq_entry);

	if (locked == 0)
		pthread_mutex_unlock(&diameter_timer_mutex);

	timeout_entry->active = 1;
	glob_dia_timer_cur++;

	return DIAMETER_CB_OK;
}

uint32_t diameter_create_timer(uint16_t type,
			void *private_data, diameter_timer_handle *h)
{
	diameter_timer_t *timer_entry = (diameter_timer_t *)
		nkn_malloc_type(sizeof(diameter_timer_t), mod_diameter_t);

	if (timer_entry == NULL)
		return DIAMETER_CB_ERROR;

	timer_entry->active = 0;
	timer_entry->type = type;
	timer_entry->private_data = private_data;

	*h = timer_entry;

	glob_dia_timer_tot++;

	return DIAMETER_CB_OK;
}

uint32_t diameter_destroy_timer(diameter_timer_handle h)
{
	diameter_timer_t *timeout_entry = (diameter_timer_t *)h;

	if (timeout_entry->active)
		diameter_stop_timer(h);

	free(h);

	glob_dia_timer_tot--;

	return DIAMETER_CB_OK;
}

uint32_t diameter_start_timer(diameter_timer_handle h,
			uint32_t timeout, uint32_t jitter_percent)
{
	(void) jitter_percent;
	return diameter_register_timer(h, timeout, 0);

}

uint32_t diameter_restart_timer(diameter_timer_handle h, uint32_t timeout)
{
	diameter_timer_t *timeout_entry = (diameter_timer_t *)h;
	int ret;

	pthread_mutex_lock(&diameter_timer_mutex);
	LIST_REMOVE(timeout_entry, toq_entry);
	ret = diameter_register_timer(h, timeout, 1);
	pthread_mutex_unlock(&diameter_timer_mutex);

	return ret;
}

uint32_t diameter_stop_timer(diameter_timer_handle h)
{
	diameter_timer_t *timeout_entry = (diameter_timer_t *)h;

	pthread_mutex_lock(&diameter_timer_mutex);
	LIST_REMOVE(timeout_entry, toq_entry);
	pthread_mutex_unlock(&diameter_timer_mutex);
	timeout_entry->active = 0;

	glob_dia_timer_cur--;

	return DIAMETER_CB_OK;
}

uint32_t diameter_get_remaining_time(diameter_timer_handle h,
			uint32_t *remaining_time)
{
	(void) h;
	(void) remaining_time;

	return DIAMETER_CB_OK;
}

void diameter_timer(void)
{
	int timeout_toq;
	diameter_timer_t *timeout_entry;

	pthread_mutex_lock(&diameter_timer_mutex);
	timeout_toq = (diameter_cur_toq + 1) % DIAMETER_MAX_TOQ_SLOT;
	while (!LIST_EMPTY(&diameter_toq_head[timeout_toq])) {

		timeout_entry =
			LIST_FIRST(&diameter_toq_head[timeout_toq]);
		LIST_FIRST(&diameter_toq_head[timeout_toq]) =
			LIST_NEXT(timeout_entry, toq_entry);
		LIST_REMOVE(timeout_entry, toq_entry);

		pthread_mutex_unlock(&diameter_timer_mutex);
		diameter_timer_elapsed(timeout_entry->type,
					timeout_entry->private_data);
		pthread_mutex_lock(&diameter_timer_mutex);

	}
	diameter_cur_toq = timeout_toq;
	pthread_mutex_unlock(&diameter_timer_mutex);

	return;
}

void diameter_timer_queue_init(void)
{
	int i;

	for (i = 0; i < DIAMETER_MAX_TOQ_SLOT; i++) {
		LIST_INIT(&diameter_toq_head[i]);
	}

	return;
}
