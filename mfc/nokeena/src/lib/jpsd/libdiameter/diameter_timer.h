/*
 * @file diameter_timer.h
 * @brief
 * diameter_timer.h - declarations for diameter timer functions
 *
 * Yuvaraja Mariappan, Dec 2014
 *
 * Copyright (c) 2015, Juniper Networks, Inc.
 * All rights reserved.
 *
 */

#ifndef _DIAMETER_TIMER_H
#define _DIAMETER_TIMER_H

#include "sys/jnx/types.h"
#include "jnx/diameter_avp_common_defs.h"
#include "jnx/diameter_base_api.h"

uint32_t diameter_create_timer(uint16_t type,
			void *private_data, diameter_timer_handle *h);

uint32_t diameter_destroy_timer(diameter_timer_handle h);

uint32_t diameter_start_timer(diameter_timer_handle h,
			uint32_t timeout, uint32_t jitter_percent);

uint32_t diameter_restart_timer(diameter_timer_handle h, uint32_t timeout);

uint32_t diameter_stop_timer(diameter_timer_handle h);

uint32_t diameter_get_remaining_time(diameter_timer_handle h,
			uint32_t *remaining_time);

void diameter_timer(void);

void diameter_timer_queue_init(void);

#endif /* !_DIAMETER_TIMER_H */
