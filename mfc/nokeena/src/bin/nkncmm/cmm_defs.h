/*
 * cmm_defs.h - Fundamental definitions
 */

#ifndef CMM_DEFS_H
#define CMM_DEFS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <sys/queue.h>
#include <sys/time.h>
#include "fqueue.h"
#include "nkn_cmm_request.h"

#include "curl/curl.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int (*add_SM)(CURLM *mch, const cmm_node_config_t *config,
		     const char *ns_name, int ns_name_strlen);
extern int (*cancel_SM)(const char *node_handle, int node_handle_strlen);

/*
 * Statistics
 */
extern long g_add_req_new;
extern long g_add_req_new_sched_fail;
extern long g_add_req_updt;
extern long g_add_req_updt_sched;
extern long g_add_req_updt_sched_fail;
extern long g_add_req_updt_setcfg_fail;

extern long g_cancel_req;
extern long g_cancel_req_sched_fail;
extern long g_cancel_req_inv_hdl;

extern long g_active_SM;
extern long g_canceled_SM;
extern long g_stale_SM;
extern long g_op_other;
extern long g_op_success;
extern long g_op_timeout;
extern long g_op_unexpected;
extern long g_op_online;
extern long g_op_offline;
extern long g_initiate_req_failed;
extern long g_last_initiate_fail_status;
extern long g_invalid_sm_incarn;

extern long g_nsc_req_waits;
extern long g_nsc_hits;
extern long g_nsc_stale_misses;
extern long g_nsc_misses;
extern long g_nsc_entries_flushed;

extern long g_getshm_data_failed;
extern long g_getshm_data_retry_failed;
extern long g_getshm_data_retry_success;
extern long g_getshm_lm_failed;
extern long g_getshm_lm_retry_failed;
extern long g_getshm_lm_retry_success;

extern long g_input_memq_stalls;
extern long g_input_memq_msg_limit;
extern long g_output_memq_stalls;

extern long g_output_fq_retry_drops;
extern long g_output_fq_err_drops;

/*
 * Global configuration data
 */
extern int cmm_debug_output;
extern FILE *cmm_debug_output_file;

extern int cmm_debug_memlog;
extern int cmm_memlog_size_bytes;
extern int cmm_memlog_bytes_used;
extern char *cmm_memlog;
extern pthread_mutex_t cmm_memlog_mutex;
extern pthread_mutex_t cmm_fd_mutex;

extern const char *input_queue_filename;
extern int input_queuefile_maxsize;

extern const char *output_queue_filename;
extern int enqueue_retries;
extern int enqueue_retry_interval_secs;

extern long nodeonline_interval_msecs;
extern long stale_sm_timeout_msecs;
extern int send_config_interval_secs;
extern int enable_connectionpool;
extern int input_memq_limit;
extern int output_memq_limit;

#if 1
#define DBG(_fmt, ...) { \
	if (cmm_debug_output || cmm_debug_memlog) { \
	    FILE *f; \
	    struct timeval tv; \
	    struct tm ttm; \
	    char timebuf[64]; \
	    gettimeofday(&tv, NULL); \
	    gmtime_r(&tv.tv_sec, &ttm); \
	    snprintf(timebuf, sizeof(timebuf), "%02d:%02d:%02d.%ld ", \
		     ttm.tm_hour, ttm.tm_min, ttm.tm_sec, tv.tv_usec); \
	    if (cmm_debug_output) { \
	    	if (!cmm_debug_output_file) { \
		    pthread_mutex_lock(&cmm_fd_mutex); \
		    if (!cmm_debug_output_file) { \
		    	cmm_debug_output_file = \
			    fopen("/nkn/tmp/nkn_cmm.dbg", "w"); \
		    } \
		    pthread_mutex_unlock(&cmm_fd_mutex); \
		} \
	    	f = cmm_debug_output_file ? cmm_debug_output_file : stdout; \
	    	fprintf(f, "%s[%s:%d]: ", timebuf, __FUNCTION__, __LINE__); \
	    	fprintf(f, (_fmt), __VA_ARGS__); \
	    	fprintf(f, "\n"); \
	    	fflush(f); \
	    } else { \
	    	int bytes; \
		pthread_mutex_lock(&cmm_memlog_mutex); \
		while (1) { \
		if ((cmm_memlog_size_bytes - cmm_memlog_bytes_used) < 1024) { \
		    cmm_memlog_bytes_used = 0; \
		} \
		bytes = snprintf(&cmm_memlog[cmm_memlog_bytes_used],  \
			     cmm_memlog_size_bytes - cmm_memlog_bytes_used, \
			     "%s[%s:%d]: ", timebuf, __FUNCTION__, __LINE__); \
		cmm_memlog_bytes_used += bytes; \
		if ((cmm_memlog_size_bytes - cmm_memlog_bytes_used) < 1024) { \
		    cmm_memlog_bytes_used = 0; \
		    continue; \
		} \
		bytes = snprintf(&cmm_memlog[cmm_memlog_bytes_used],  \
			     cmm_memlog_size_bytes - cmm_memlog_bytes_used, \
			     (_fmt), __VA_ARGS__); \
		cmm_memlog_bytes_used += bytes; \
		if ((cmm_memlog_size_bytes - cmm_memlog_bytes_used) < 1024) { \
		    cmm_memlog_bytes_used = 0; \
		    continue; \
		} \
		cmm_memlog[cmm_memlog_bytes_used] = '\n'; \
		cmm_memlog_bytes_used++; \
		cmm_memlog[cmm_memlog_bytes_used] = '\0'; \
		break; \
		} /* End while */ \
		pthread_mutex_unlock(&cmm_memlog_mutex); \
	    } \
	}  else { \
	    if (cmm_debug_output_file) { \
		pthread_mutex_lock(&cmm_fd_mutex); \
		if (cmm_debug_output_file) { \
		    fclose(cmm_debug_output_file); \
		    cmm_debug_output_file = NULL; \
		} \
		pthread_mutex_unlock(&cmm_fd_mutex); \
	    } \
	} \
}
#define DBG_ON cmm_debug_output
#else
#define DBG_ON cmm_debug_output
#define DBG(_fmt, ...)
#endif

#define TSTR(_p, _l, _r)  char *(_r); ( 1 ? ( (_r) = alloca((_l)+1), \
	     memcpy((_r), (_p), (_l)), (_r)[(_l)] = '\0' ) : 0 )

#define MAX_LINE_SIZE		4096
#define CMM_QUEUE_FILE 		"/tmp/CMM.queue"
#define NODESTATUS_QUEUE_FILE 	"/tmp/NODESTATUS.queue"

typedef struct queue_element {
    fqueue_element_t fq_element;
    SIMPLEQ_ENTRY(queue_element) queue;
} queue_element_t;

#ifdef __cplusplus
}
#endif

#endif /* CMM_DEFS_H */

/*
 * End of cmm_defs.h
 */
