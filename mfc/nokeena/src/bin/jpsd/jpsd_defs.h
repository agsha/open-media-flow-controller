/*
 * @file jpsd_defs.h
 * @brief
 * jpsd_defs.h - declarations for common functions
 *
 * Yuvaraja Mariappan, Dec 2014
 *
 * Copyright (c) 2015, Juniper Networks, Inc.
 * All rights reserved.
 *
 */

#ifndef _JPSD_DEFS_H
#define _JPSD_DEFS_H

#define TRUE	1
#define FALSE	0

extern int NM_tot_threads;

extern void config_and_run_counter_update(void);
extern void jpsd_timer_start(void);
extern void NM_init(void);
extern void NM_start(void);
extern void NM_wait(void);


extern void get_cacheable_ip_list(void);

extern void diameter_timer(void);
extern void diameter_timer_queue_init(void);
extern void jpsd_tdf_start(void);

#endif // _JPSD_DEFS_H

