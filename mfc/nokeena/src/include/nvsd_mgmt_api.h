#ifndef __NVSD_MGMT_API__H
#define __NVSD_MGMT_API__H

/*
 *
 * Filename:  nvsd_mgmt_api.h
 * Date:      2009/02/23 
 * Author:    Ramanand Narayanan
 *
 * (C) Copyright 2008-09 Nokeena Networks, Inc.  
 * All rights reserved.
 *
 *
 */

/* Include Files */
#include <glib.h>

/* Macros to log using Samara's Logging API */
/* These names and values are in syslog.h, and here only for reference */
#if 0
enum {
	LOG_first = 0,
	LOG_EMERG        = 0, /* system is unusable */
	LOG_ALERT        = 1, /* action must be taken immediately */
	LOG_CRIT         = 2, /* critical conditions */
	LOG_ERR          = 3, /* error conditions */
	LOG_WARNING      = 4, /* warning conditions */
	LOG_NOTICE       = 5, /* normal but significant condition */
	LOG_INFO         = 6, /* informational */
	LOG_DEBUG        = 7, /* debug-level messages */
	LOG_last
};
#endif
/* Use one of the above LOG_?? for loglevel in the macro */
#define SYS_LOG(loglevel, fmt, ...)			\
    {							\
	lc_log_basic (loglevel, fmt, ##__VA_ARGS__); 	\
    }

/* Types */

/* Used by ret_str to DM2 - max length is defined as 
 * "script_fullpath(40) + 1 + raw_dev(40) + 1 + fs_dev(40) + 1 + blksz(4) + 1 +
 *  pgsz(2) + 1 +percent(3)" */
#define	MAX_FORMAT_CMD_LEN  256

/* Extern Function Prototypes */
extern GList* nvsd_mgmt_get_disklist_locked (void);
extern void nvsd_mgmt_diskcache_lock(void);
extern void nvsd_mgmt_diskcache_unlock(void);

/****************************************************************
 * This is a common entry for both om_dns and cli, so we can 
 * change in only one place if the dns info that nvsd populates
 * increases. This is for a single DNS info
 ****************************************************************/  
#define MAX_DNS_INFO 128

#endif // __NVSD_MGMT_API__H
