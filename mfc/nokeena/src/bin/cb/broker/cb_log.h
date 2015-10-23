/*
 * cb_log.h -- Content Broker log interface
 */

#ifndef _CB_LOG_H
#define _CB_LOG_H

#include <stdarg.h>
#include "libnknalog.h"

typedef enum {
    SS_UNKNOWN=0,
    SS_CORE,
    SS_PTRIE,
    SS_HTTP,
    SS_NET,
    SS_MAX // Always last
} ss_t; 

typedef enum {
    CB_ALARM=0,
    CB_SEVERE=1,
    CB_ERROR = 2,
    CB_WARNING = 3,
    CB_MSG = 4,
    CB_MSG1 = 4,
    CB_MSG2 = 5,
    CB_MSG3 = 6,
} cb_log_level;

extern int log_level;

int cb_log_init(int, const char *accesslog_pname);
int cb_log_var(ss_t ss, cb_log_level level, const char *fmt, va_list ap);
int cb_log(ss_t ss, cb_log_level level, const char *fmt, ...);
int cb_access_log(alog_conn_data_t *cd, token_data_t *td);

#define CB_LOG(_level, _fmt, ...) { \
    if ((_level) <= log_level) { \
    	cb_log(SS_CORE, (_level), "[%s:%d] "_fmt"\n", \
	       __FUNCTION__, __LINE__, ##__VA_ARGS__); \
    } \
}
#endif /* _CB_LOG_H */
/*
 * End of cb_log.h
 */
