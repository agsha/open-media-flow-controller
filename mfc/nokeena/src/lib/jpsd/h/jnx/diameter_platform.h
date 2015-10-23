/*
 * @file diameter_platfrom.h
 * @brief
 * diameter_platfrom.h - the only place-holder for all platform specific
 * declarations and includes.
 *
 * Yuvaraja Mariappan, Dec 2014
 *
 * Copyright (c) 2015, Juniper Networks, Inc.
 * All rights reserved.
 *
 */

#ifndef _DIAMETER_PLATFORM_H
#define _DIAMETER_PLATFORM_H

#include <sys/jnx/types.h>
#include <jnx/getput.h>

#include "nkn_debug.h"

#define DIAMETER_SYSTEM_LOG(group, level, fmt, ...) \
{ \
        if (DO_DBG_LOG(MSG, MOD_JPSD)) {  \
                log_debug_msg(MSG, MOD_JPSD, \
                        "[MOD_JPSD.MSG] %s:%d: "fmt"\n",   \
                       __FUNCTION__,                            \
                       __LINE__, ##__VA_ARGS__);                \
        } \
}

#define atomic_u32_set(p,v)         (AO_int_store(p,v))
#define atomic_u32_inc(p)           (AO_int_fetch_and_add1(p))
#define atomic_u32_inc_return(p)    (AO_int_fetch_and_add1(p)+1)
#define atomic_u32_dec_return(p)    (AO_int_fetch_and_sub1(p)-1)
#define atomic_u32_dec(p)           (AO_int_fetch_and_sub1(p))
#define atomic_u32_add_return(x,p)  (AO_int_fetch_and_add(p,x)+x)
#define atomic_u32_read(x)          (*x)
#define atomic_u32_sub_return(x,p)  (AO_int_fetch_and_add(p,-x)-x)
#define atomic_u32_sub(x,p)         (AO_int_fetch_and_add(p,-x))

void diameter_log(int group, int level, const char* format, ...);

#endif /* !_DIAMETER_PLATFORM_H */
