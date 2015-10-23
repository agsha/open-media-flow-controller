/*
 * cb_log.c -- Content Broker log interface
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include "nkn_debug.h"
#include "libnknalog.h"
#include "cb_log.h"

const char *subsystem_str[SS_MAX] =  {
    "SS_UNKNOWN", 
    "SS_CORE", 
    "SS_PTRIE", 
    "SS_HTTP", 
    "SS_NET"
};

static const char *cblevel2dlevel[7] = {
    "ALARM",
    "SEVERE",
    "ERROR",
    "WARNING",
    "MSG",
    "MSG2",
    "MSG3",
};

int log_level = CB_MSG3;
static int use_errlog = 0;
static int use_accesslog = 0;;
static alog_token_t al_token;

int cb_log_var(ss_t ss, cb_log_level level, const char *fmt, va_list ap)
{
    va_list ap2;
    char buf[8192];
    int rv;
    int bytes_used;

    va_copy(ap2, ap);

    if (use_errlog) {
	rv = snprintf(buf, sizeof(buf), "[MOD_CB.%s] [%s] [Level=%d]:", 
		      cblevel2dlevel[level], subsystem_str[ss], level);
	if (rv < (int)sizeof(buf)) {
	    bytes_used = rv; 
	} else {
	    buf[sizeof(buf)-1] = '\0';
	    bytes_used = sizeof(buf);
	}

	rv = vsnprintf(&buf[bytes_used], sizeof(buf) - bytes_used, fmt, ap2);
	if (rv >= (int)(sizeof(buf) - bytes_used)) {
	    buf[sizeof(buf)-1] = '\0';
	}
	va_end(ap2);

	switch(level) {
	case ALARM:
	    log_debug_msg(ALARM, MOD_CB, "%s", buf);
	    break;
	case SEVERE:
	    log_debug_msg(SEVERE, MOD_CB, "%s", buf);
	    break;
	case ERROR:
	    log_debug_msg(ERROR, MOD_CB, "%s", buf);
	    break;
	case WARNING:
	    log_debug_msg(WARNING, MOD_CB, "%s", buf);
	    break;
	case MSG:
	    log_debug_msg(MSG, MOD_CB, "%s", buf);
	    break;
	case MSG2:
	    log_debug_msg(MSG2, MOD_CB, "%s", buf);
	    break;
	case MSG3:
	    log_debug_msg(MSG3, MOD_CB, "%s", buf);
	    break;
	default:
	    log_debug_msg(MSG3, MOD_CB, "%s", buf);
	    break;
	}
	return 0;

    } else {
	printf("[%s] [Level=%d]:", subsystem_str[ss], level);
	vprintf(fmt, ap2);
	va_end(ap2);

	return 0;
    }
}

int cb_log(ss_t ss, cb_log_level level, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    return cb_log_var(ss, level, fmt, ap);
}

int cb_access_log(alog_conn_data_t *cd, token_data_t *td)
{
    if (use_accesslog) {
    	return alog_write(al_token, cd, td);
    } else {
    	return 0;
    }
}

int cb_log_init(int enable_errlog, const char *accesslog_pname)
{
    if (enable_errlog) {
    	log_thread_start();
    	use_errlog = 1;

	al_token = alog_init(accesslog_pname);
	if (al_token) {
	    use_accesslog = 1;
	} else {
	    cb_log(SS_CORE, ALARM, 
	    	   "Unable to establish access log using profile [%s]",
		   accesslog_pname);
	}
    }
    return 0;
}

/*
 * End of cb_log.c
 */
