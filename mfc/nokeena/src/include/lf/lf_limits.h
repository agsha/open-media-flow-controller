#ifndef _LF_LIMITS_H
#define _LF_LIMITS_H

#define LF_MAX_APPS 10
#define LF_MAX_SECTIONS 4 /* system, vm apps, native apps, custom */
#define LF_MAX_CLIENT_TOKENS 10
#define LFD_MAX_FD_LIMIT 20000
#define LFD_NUM_DISP_THREAD 2

#define LF_MIN_SAMPLING_RES (100) /* ms */
#define LF_MAX_SAMPLING_RES (2000) /* ms - may vioalate sampling theorem */
#define LF_MIN_WINDOW_LEN (2000) /* ms */
#define LF_MAX_WINDOW_LEN (3600000) /* 1hr in ms */

#define LF_SYS_MAX_DISKS 64 /* fine till VXA2100 */

#define LF_NA_ENTITY_NAME_LEN 65
#define LF_NA_COUNTER_NAME_LEN 65		      

#endif
