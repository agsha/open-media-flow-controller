/*
 *	COPYRIGHT: NOKEENA NETWORKS
 *
 * This file implements a form of runtime debugging
 */
#ifndef NKN_DEBUG_H
#define NKN_DEBUG_H

#include <stdio.h>
#include <stdint.h>

enum dlevel_enum {
    ALARM = 0,
    SEVERE = 1,
    ERROR = 2,
    WARNING = 3,
    MSG = 4,
    MSG1 = 4,
    MSG2 = 5,
    MSG3 = 6,
};

#define MOD_NETWORK     0x0000000000000001
#define MOD_HTTP        0x0000000000000002
#define MOD_DM2         0x0000000000000004
#define MOD_MM          0x0000000000000008
#define MOD_BM          0x0000000000000010
#define MOD_CE          0x0000000000000020
#define MOD_SCHED       0x0000000000000040
#define MOD_SSP         0x0000000000000080
#define MOD_HTTPHDRS    0x0000000000000100
#define MOD_FQUEUE      0x0000000000000200
#define MOD_SYSTEM      0x0000000000000400
#define MOD_FM          0x0000000000000800
#define MOD_OM          0x0000000000001000
#define MOD_OOMGR       0x0000000000002000
#define MOD_TCP         0x0000000000004000
#define MOD_TFM         0x0000000000008000
#define MOD_NAMESPACE   0x0000000000010000
#define MOD_NFS         0x0000000000020000
#define MOD_RTSP        0x0000000000040000
#define MOD_AM          0x0000000000080000
#define MOD_MM_PROMOTE  0x0000000000100000
#define MOD_FUSE        0x0000000000200000
#define MOD_VPEMGR      0x0000000000400000
#define MOD_GM          0x0000000000800000
#define MOD_DASHBOARD   0x0000000001000000
#define MOD_CACHELOG	0x0000000002000000
#define MOD_CP		0x0000000004000000
#define MOD_TUNNEL	0x0000000008000000
#define MOD_HRT		0x0000000010000000
#define MOD_CLUSTER	0x0000000020000000
#define MOD_SSL		0x0000000040000000
#define MOD_MFPFILE     0x0000000080000000
#define MOD_MFPLIVE     0x0000000100000000
#define MOD_L7REDIR	0x0000000200000000
#define MOD_EM          0x0000000400000000
#define MOD_RESRC_POOL  0x0000000800000000
#define MOD_AUTHMGR     0x0000001000000000
#define MOD_AUTHHLP     0x0000002000000000
#define MOD_ADNS	0x0000004000000000
#define MOD_PROXYD	0x0000008000000000
#define MOD_GEODB       0x0000010000000000
#define MOD_PEMGR	0x0000020000000000
#define MOD_CSE         0x0000040000000000
#define MOD_CDE         0x0000080000000000
#define MOD_CUE         0x0000100000000000
#define MOD_CCP         0x0000200000000000
#define MOD_CRAWL       0x0000400000000000
#define MOD_CIM         0x0000800000000000
#define MOD_ICCP        0x0001000000000000
#define MOD_UCFLT       0x0002000000000000
#define MOD_NKNEXECD    0x0004000000000000
#define MOD_VCS         0x0008000000000000
#define MOD_COMPRESS    0x0010000000000000
#define MOD_CB    	0x0020000000000000
#define MOD_URL_FILTER 	0x0040000000000000
#define MOD_DPI_TPROXY	0x0080000000000000
#define MOD_DPI_URIF    0x0100000000000000
#define MOD_JPSD	0x0200000000000000


#define MOD_FUSE_LOG	0x2000000000000000
#define MOD_TRACE       0x4000000000000000
#define MOD_CACHE	0x8000000000000000
#define MOD_SYS		0x1000000000000000

#define MOD_CRST        0x0010000000000000
#define MOD_ANY_MOD	0x0FFFFFFFFFFFFFFF
#define MOD_ANY         0xFFFFFFFFFFFFFFFF

/*
 * MACRO definition
 */

/* *************************************************************
 *
 * *************************************************************/

/*
 * These are basic functions used for connecting to nknlogd.
 * log_thread_start() is a blocking function until thread is created.
 * log_debug_msg() is a non-blocking function.
 */
extern void log_thread_start(void);
extern void log_debug_msg(int dlevel, uint64_t dmodule, const char * fmt, ...)
			    __attribute__ ((format (printf, 3, 4)));
extern void log_thread_end(void);

extern uint64_t console_log_mod;
extern int console_log_level;

// Note: For exceptional cases only.
#define DO_DBG_LOG(dlevel, dmodule) \
	(((dmodule) & console_log_mod) && ((dlevel) <= console_log_level))

#define DBG_LOG(dlevel, dmodule, fmt, ...)                      \
    {									\
	if (DO_DBG_LOG(dlevel, dmodule)) {	\
		log_debug_msg(dlevel, dmodule, "["#dmodule"."#dlevel"] %s:%d: "fmt"\n",   \
                       __FUNCTION__,				\
                       __LINE__, ##__VA_ARGS__);                \
	}   \
    }

#define DBG_MFPLOG(dId, dlevel, dmodule, fmt, ...)                      \
    {									\
        log_debug_msg(dlevel, dmodule, "["#dmodule"."#dlevel"] %s:%d: ID[%s] "fmt"\n",   \
                       __FUNCTION__,				\
                       __LINE__, dId, ##__VA_ARGS__);                \
    }

#define DBG_TRACE(fmt, ...)						\
    {									\
	log_debug_msg(SEVERE, MOD_TRACE, "[MOD_TRACE] "fmt"\n",		\
		       ##__VA_ARGS__);					\
    }

#define DBG_CACHE(fmt, ...)						\
    {									\
	log_debug_msg(ERROR, MOD_CACHE, fmt"\n",		\
		      ##__VA_ARGS__);					\
    }

#define DBG_CRAWL(fmt, ...)						\
    {									\
	log_debug_msg(ERROR, MOD_CRAWL, fmt"\n",		\
		      ##__VA_ARGS__);					\
    }


#if 1
#define DBG_FUSE(fmt, ...)                                             \
    {                                                                   \
        log_debug_msg(ERROR, MOD_FUSE_LOG, fmt"\n",                \
                      ##__VA_ARGS__);                                   \
    }
#else
#define DBG_FUSE(fmt, ...)                                             \
;
#endif

#define DBG_ERR(dlevel, fmt, ...)                                             \
    {                                                                   \
        log_debug_msg(dlevel, MOD_SYS, "["#dlevel"]" fmt"\n",	\
                      ##__VA_ARGS__);	 \
    }
/* *************************************************************/
#if 0
/* *************************************************************/
#include "nkn_cfg_params.h"

/* Extern Functions */
extern void nvsd_mgmt_syslog (char *fmt, ...) __attribute__((format(printf, 1, 2)));
extern char *nvsd_mgmt_timestamp(char *sz_ts);

/*
 * - We always log messages when level is SEVERE or ERROR.
 * - If dlevel is ALARM, then a syslog entry is generated.
 * - If dmodule is MOD_ANY, then match any module.
 *
 * Otherwise, the dmodule must match
 */
#define DBG_LOG(dlevel, dmodule, fmt, ...)			\
    {								\
	char sz_ts[MAX_TIMESTAMP_LEN];				\
        if (dlevel == ALARM) {					\
	   nvsd_mgmt_syslog ((char*)fmt, ##__VA_ARGS__);	\
	} else if ((dlevel == SEVERE) ||			\
		   ((dmodule & dbg_log_mod) &&			\
		    (dlevel <= dbg_log_level))) {		\
	    if (errorlog_socket) {				\
		nkn_errorlog("%s["#dmodule"] %s:%d: "fmt"\n",	\
		       nvsd_mgmt_timestamp(sz_ts),__FUNCTION__,	\
		       __LINE__, ##__VA_ARGS__);		\
	    } else {						\
		printf("%s["#dmodule"] %s:%d: "fmt"\n",	\
		       nvsd_mgmt_timestamp(sz_ts),__FUNCTION__,	\
		       __LINE__, ##__VA_ARGS__);		\
	   }							\
	}							\
    }

#define DBG_LOG_MEM_UNAVAILABLE(module) DBG_LOG(SEVERE, module, "\n Memory not available");

#define DBG_TRACE(fmt, ...)						\
    {									\
	char sz_ts[MAX_TIMESTAMP_LEN];					\
	if (errorlog_socket) {						\
		nkn_errorlog("%s [MOD_TRACE] "fmt"\n",			\
		       nvsd_mgmt_timestamp(sz_ts), ##__VA_ARGS__);	\
	} else {							\
		printf("%s [MOD_TRACE] "fmt"\n",		\
		       nvsd_mgmt_timestamp(sz_ts), ##__VA_ARGS__);	\
	}								\
    }


extern int errorlog_socket;
extern void nkn_errorlog(const char * fmt, ...);

/* *************************************************************/
#endif // 0
/* *************************************************************/


#define DBG_DM2A(fmt, ...) DBG_LOG(ALARM, MOD_DM2, fmt, ##__VA_ARGS__);
#define DBG_DM2S(fmt, ...) DBG_LOG(SEVERE, MOD_DM2, fmt, ##__VA_ARGS__);
#define DBG_DM2E(fmt, ...) DBG_LOG(ERROR, MOD_DM2, fmt, ##__VA_ARGS__);
#define DBG_DM2W(fmt, ...) DBG_LOG(WARNING, MOD_DM2, fmt, ##__VA_ARGS__);
#define DBG_DM2M(fmt, ...) DBG_LOG(MSG, MOD_DM2, fmt, ##__VA_ARGS__);
#define DBG_DM2M2(fmt, ...) DBG_LOG(MSG2, MOD_DM2, fmt, ##__VA_ARGS__);
#define DBG_DM2M3(fmt, ...) DBG_LOG(MSG3, MOD_DM2, fmt, ##__VA_ARGS__);
#define DBG_LOG_MEM_UNAVAILABLE(module) DBG_LOG(SEVERE, module, "\n Memory not available");
#define DBG_CACHELOG(fmt, ...) DBG_LOG(WARNING, MOD_CACHELOG, fmt, ##__VA_ARGS__);
#define DBG_TRACELOG(fmt, ...) DBG_TRACE(fmt, ##__VA_ARGS__);


#endif /* NKN_DEBUG_H */
