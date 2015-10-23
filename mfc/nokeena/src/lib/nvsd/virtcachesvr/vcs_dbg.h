/*============================================================================
 *
 *
 * Purpose: This file defines the virt_cache logging macros.
 *
 * Copyright (c) 2002-2012 Juniper Networks. All rights reserved.
 *
 *
 *============================================================================*/
#ifndef __VCS_DBG_H__
#define __VCS_DBG_H__

#include "nkn_debug.h"

// SEVERE level message
#define DBG_VCS(fmt, ...) DBG_LOG(SEVERE, MOD_VCS, fmt, ##__VA_ARGS__)

// ERROR level message
#define DBG_VCE(fmt, ...) DBG_LOG(ERROR, MOD_VCS, fmt, ##__VA_ARGS__)

// WARNING level message
#define DBG_VCW(fmt, ...) \
    DBG_LOG(WARNING, MOD_VCS, fmt, ##__VA_ARGS__);

// MSG level message
#define DBG_VCM(fmt, ...) \
    DBG_LOG(MSG, MOD_VCS, fmt, ##__VA_ARGS__);

// MSG3 level message
#define DBG_VCM3(fmt, ...) \
    DBG_LOG(MSG3, MOD_VCS, "(%s) "fmt, __FUNCTION__, ##__VA_ARGS__);

// Out Of Memory message
#define DBG_OOM() DBG_LOG_MEM_UNAVAILABLE(MOD_VCS)

#endif /* __VCS_DBG_H__ */
