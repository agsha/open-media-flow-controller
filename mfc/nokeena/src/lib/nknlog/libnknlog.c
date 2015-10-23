#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdint.h>
#include <sys/time.h>
#include <string.h>
#include <syslog.h>
#include <strings.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <assert.h>
#include <sys/prctl.h>
#include "nkn_opts.h"
#include "nkn_debug.h"
#include "nknlog_pub.h"
#include "nkn_memalloc.h"
#include "nkn_stat.h"

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t log_pid = -1;

#define MAX_LOG_BUFSIZE	(8*1024*1024)// increased the buffer size from 4MB to 16 MB Bug:10714

//counters to check buffer overflow
NKNCNT_DEF(cac_err_buf_overflow, uint64_t, "", "Num of cac logs dropped")
NKNCNT_DEF(cac_tot_logged, uint64_t, "", "Num of cac logs written")

NKNCNT_DEF(el_err_buf_overflow, uint64_t, "", "Num of err logs dropped")
NKNCNT_DEF(el_tot_logged, uint64_t, "", "Num of err logs written")

//NKNCNT_DEF(al_err_buf_overflow, uint64_t, "", "Num of al logs dropped")
//NKNCNT_DEF(al_tot_logged, uint64_t, "", "Num of al logs written")

NKNCNT_DEF(tr_err_buf_overflow, uint64_t, "", "Num of Tr logs dropped")
NKNCNT_DEF(tr_tot_logged, uint64_t, "", "Num of Tr logs written")

NKNCNT_DEF(str_err_buf_overflow, uint64_t, "", "Num of str logs dropped")
NKNCNT_DEF(str_tot_logged, uint64_t, "", "Num of str logs written")

NKNCNT_DEF(sys_err_buf_overflow, uint64_t, "", "Num of sys logs dropped")
NKNCNT_DEF(sys_tot_logged, uint64_t, "", "Num of syslogs written")

NKNCNT_DEF(cac_err_len_overflow, uint64_t, "", "Num of cac logs dropped")
NKNCNT_DEF(cac_err_decay_len_overflow, uint64_t, "", "Num of cac decay logs dropped")
NKNCNT_DEF(cac_err_add_len_overflow, uint64_t, "", "Num of cac add logs dropped")
NKNCNT_DEF(cac_err_delete_len_overflow, uint64_t, "", "Num of cac delete logs dropped")
NKNCNT_DEF(cac_err_update_len_overflow, uint64_t, "", "Num of cac update logs dropped")

NKNCNT_DEF(cac_err_decay_overflow, uint64_t, "", "Num of cac dec logs dropped")
NKNCNT_DEF(cac_err_add_overflow, uint64_t, "", "Num of cac add logs dropped")
NKNCNT_DEF(cac_err_delete_overflow, uint64_t, "", "Num of cac delete logs dropped")
NKNCNT_DEF(cac_err_update_overflow, uint64_t, "", "Num of cac update logs dropped")
NKNCNT_DEF(cac_tot_logged_add, uint64_t, "", "Num of cac logs added written")
NKNCNT_DEF(cac_tot_logged_delete, uint64_t, "", "Num of cac logs deleted written")
NKNCNT_DEF(cac_tot_logged_decay, uint64_t, "", "Num of cac logs decayed written")
NKNCNT_DEF(cac_tot_logged_update, uint64_t, "", "Num of cac logs updated written")

static char debuglog_buf[2][MAX_LOG_BUFSIZE];
static int debuglog_buf_end = 0;
static int debuglog_cur_id = 0;
static int logfd = -1;
static int epfd = -1;
static int max_fds = 20;
static struct epoll_event * events;
static int next_conn_try_time = 0;
static int thread_exit = 0;
int console_log_level = MSG3;
uint64_t console_log_mod = MOD_ANY;

struct channel_mgr {
    struct channel_mgr *next;
    int32_t type;
    int32_t channelid;
    int32_t level;
    uint64_t mod;
};
static struct channel_mgr *g_channel_mgr = NULL;

uint32_t errorlog_ip = DEF_LOG_IP;
uint16_t errorlog_port = LOGD_PORT;

#define CLOSE_LOGFD() \
{ \
	epoll_ctl(epfd, EPOLL_CTL_DEL, logfd, NULL); \
	shutdown(logfd, 0); \
	close(logfd); \
	logfd = -1; \
}

/* ------------------------------------------------------------------------- */
/*
 *      funtion : nvsd_mgmt_timestamp ()
 *      purpose : API/Wrapper for returning timestamp
 */
#define MAX_TIMESTAMP_LENTH	50
static char *
log_mgmt_timestamp(char *sz_ts)
{
    struct timeval  tval;
    time_t	    t_timep;
    char	    year[6];
    int		    retval;

    /* Sanity test */
    if (!sz_ts) return(NULL);

    /* Get the timeval value */
    retval = gettimeofday(&tval, NULL);
    if (0 == retval) {
	/* Now create the timestamp string */
	t_timep = tval.tv_sec;
	sz_ts[0] = '[';
	sz_ts[1] = '\0';
	if (NULL != asctime_r(localtime(&t_timep), &sz_ts[1])) {
	    char *cp;

	    /* Replace the \n with \0 */
	    cp = strchr(sz_ts, '\n');
	    if (cp) *cp = '\0';

	    cp = strrchr(sz_ts, ' ');
	    strncpy(year, cp+1, 6);
	    year[5] = '\0';	// safety

	    /* Fill-in millisecond & year
	     * period = 1
	     * millisecond time = 3
	     * space = 1
	     * year = 4
	     * end bracket = 1
	     * eof-of-line = 1 */
	    snprintf(cp, 11, ".%03d %s]", (int)tval.tv_usec/1000, year);
	} else
	    strcpy(sz_ts, "<gettime failed>");
    } else {
	/* gettimeofday has failed */
	strcpy(sz_ts, "<gettime failed>");
    }

    return sz_ts;
}	/* end of nvsd_mgmt_timestamp() */


static int
log_init_socket(void)
{
    struct sockaddr_in srv;
    int ret;

    // Create a socket for making a connection.
    // here we implemneted non-blocking connection
    if ((logfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	//printf("%s: logfd failed", __FUNCTION__);
	logfd = -1;
	return -1;
    }

    // Now time to make a socket connection.
    bzero(&srv, sizeof(srv));
    srv.sin_family = AF_INET;
    //srv.sin_addr.s_addr = 0x0100007F;
    //srv.sin_port = htons(LOGD_PORT);
    srv.sin_addr.s_addr = errorlog_ip;
    srv.sin_port =  htons(errorlog_port);

    //log_set_socket_nonblock(logfd);

    ret = connect(logfd, (struct sockaddr *)&srv, sizeof(struct sockaddr));
    if (ret < 0) {
	//printf("Failed to connect to nknlogd server\n");
	return -1;
    }
    //printf("Successfully connect to nknlogd server\n");
    return 1;
}

static int
log_recv_data(char *buf, int expectlen)
{
    int ret;

    while (expectlen) {
	ret = recv(logfd, buf, expectlen, 0);
	if (ret <= 0) {
	    return -1;
	}
	buf += ret;
	expectlen -= ret;
    }
    return 1;
}


static int
log_handshake_debuglog(void)
{
    NLP_client_hello *req;
    NLP_server_hello *res;
    NLP_debuglog_channel *plog;
    struct channel_mgr *c_mgr;
    int ret, len, i;
    char buf[4096] = {0};

    /*
     * Send request to nvsd server.
     */
    req = (NLP_client_hello *)buf;
    strcpy(req->sig, SIG_SEND_DEBUGLOG);
    if (send(logfd, buf, sizeof(NLP_client_hello), 0) <= 0) {
	return -1;
    }

    /*
     * Read response from nvsd server.
     */
    ret = log_recv_data(&buf[0], sizeof(NLP_server_hello));
    if (ret < 0) {
	perror("Socket recv.");
	return -1;
    }
    res = (NLP_server_hello *)buf;

    len = res->lenofdata;

    if (len != 0)
    {
	ret = log_recv_data(&buf[sizeof(NLP_server_hello)], len);
	if(ret<0) {
	    perror("Socket recv.");
	    return -1;
	}
	plog = (NLP_debuglog_channel *)&buf[sizeof(NLP_server_hello)];
	for(i=0; i<res->numoflog; i++) {
	    /*
	     * if same type c_mgr does not exist, create one.
	     * Otherwise just update with new configuration.
	     */
	    c_mgr = g_channel_mgr;
	    while(c_mgr) {
		if(c_mgr->type == plog->type) break;
		    c_mgr = c_mgr->next;
	    }
	    if(c_mgr == NULL) {
		c_mgr = (struct channel_mgr *)calloc(1, sizeof(struct channel_mgr));
		if(c_mgr) {
		    c_mgr->next = g_channel_mgr;
		    g_channel_mgr = c_mgr;
		}
	    }
	    if(c_mgr) {
		c_mgr->level = plog->level;
		c_mgr->mod = plog->mod;
		c_mgr->type = plog->type;
		c_mgr->channelid = plog->channelid;
	    }
	    plog ++;
	}
    }
    return 1;
}


static void *
log_thread_func(void *arg __attribute((unused)))
{
    struct channel_mgr *c_mgr;
    int ret;
    char *p;
    int len;
    struct epoll_event ev;
    prctl(PR_SET_NAME, "nvsd-log-func", 0, 0, 0);

    epfd = epoll_create(max_fds);
    assert(epfd != -1);
    events = calloc(max_fds, sizeof(struct epoll_event));
    assert( events != NULL);


again:
    pthread_mutex_lock(&log_mutex);

    if (logfd != -1) {
	CLOSE_LOGFD();
    }

    /* launch the socket  and  complete handshake */
    if ((log_init_socket() < 0) || (log_handshake_debuglog() < 0)) {
	next_conn_try_time = 10; // Try to connect 10 seconds later
	CLOSE_LOGFD();
	pthread_mutex_unlock(&log_mutex);
	sleep(10);	// 10 seconds
	goto again;
    }

    memset((char *)&ev, 0, sizeof(struct epoll_event));
    ev.events = EPOLLIN;
    ev.data.fd = logfd;
    if(epoll_ctl(epfd, EPOLL_CTL_ADD, logfd, &ev) < 0) {
	next_conn_try_time = 10; // Try to connect 10 seconds later
	CLOSE_LOGFD();
	pthread_mutex_unlock(&log_mutex);
	sleep(10);	// 10 seconds
	goto again;
    }

    /* Socket has been successfully established. */
    next_conn_try_time = 0;

    pthread_mutex_unlock(&log_mutex);

    /*
     *  function log_debug_msg() will get mutex lock.
     */
    c_mgr = g_channel_mgr;
    //while (c_mgr) {
    //	log_debug_msg(MSG, MOD_SYSTEM, "[MOD_SYSTEM] nknlogd creates new "
    //		      "channel %d for logging. type=%d, level=%d, mod=0x%lx",
    //		      c_mgr->channelid, c_mgr->type, c_mgr->level, c_mgr->mod);
    //	c_mgr = c_mgr->next;
    //}

    while (1) {
	if (debuglog_buf_end == 0) {
	    /* no data to be sent? we sleep in select */
	    ret = epoll_wait(epfd, events, max_fds, 1000); // 1 second
	    if (ret != 0) {
		/* peer closes the socket */
		CLOSE_LOGFD();
		goto again;
	    }
	    if (thread_exit == 1) {
		CLOSE_LOGFD();
		return NULL;
	    }
	    if (debuglog_buf_end == 0) continue;
	}

	/*
	 * the main idea is: make a copy of data so we avoid holding mutex.
	 */
	pthread_mutex_lock(&log_mutex);
	p = &debuglog_buf[debuglog_cur_id][0];
	len = debuglog_buf_end;
	assert(len < MAX_LOG_BUFSIZE);
	/* swap the debug memory */
	debuglog_cur_id = (debuglog_cur_id+1) % 2;
	debuglog_buf_end = 0;
	pthread_mutex_unlock(&log_mutex);

	while (len) {
	    ret = send(logfd, p, len, 0);
	    if (ret == -1) {
		/* socket closed by peer */
		CLOSE_LOGFD();
		goto again;
	    } else if (ret == 0) {
		/* socket is full, We could not send out more data.
		 * Try again later */
		continue;
	    }

	    p += ret;
	    len -= ret;

	    /* When application has called log_thread_end() */
	    if (thread_exit == 1) {
		CLOSE_LOGFD();
		return NULL;
	    }
	}
    }
}

/*
 * This function will be called by application initially or after socket is
 * disconnected.
 */
void
log_thread_start(void)
{
    static int thr_is_running = 0;	/* 0: thread is not running */

    /* time to launch the sned thread */
    if (thr_is_running == 1) {
	return;
    }
    if (pthread_create(&log_pid, NULL, log_thread_func, NULL) == 0) {
	thr_is_running = 1;
    }
    return;
}


void
log_debug_msg(int dlevel, uint64_t dmodule, const char *fmt, ...)
{
    va_list tAp;
    int32_t len, slen;
    int ret;
    NLP_data *pdata;
    struct channel_mgr *c_mgr;
    char *p;

    if (unlikely(dlevel == ALARM)) {
	char log_str[1024];
	static int syslog_open = 0;

	va_start(tAp, fmt);
	ret = vsnprintf(log_str, 1024, fmt, tAp);
	va_end(tAp);

	if (syslog_open == 0) {
	    openlog("nvsd", LOG_NDELAY,  LOG_SYSLOG);
	    syslog_open = 1;
	}
	syslog(LOG_ALERT, "%s", log_str);
	glob_sys_tot_logged++;
	return;
    }

    /* if nknlogd is not running */
    if (unlikely(logfd == -1)) {
	char buf[100];

	/*
	 * For any reason, if socket does not exist,
	 * We try to launch the network thread again.
	 */
	log_thread_start();

	/*
	 * Let's print it on the screen.
	 */
	if ( (dlevel == SEVERE) ||
		((dmodule & console_log_mod) && (dlevel <= console_log_level)) ) {
	    printf("%s", log_mgmt_timestamp(&buf[0]));
	    va_start(tAp, fmt);
	    ret = vprintf(fmt, tAp);
	    va_end(tAp);
	}
	return;
    }

    /* check each channel */
    c_mgr = g_channel_mgr;

    while (c_mgr) {
	//	printf("log_debug_msg: called\n");
	// All message in TYPE_SYSLOG automatically written to error.log
	if ( ((dlevel == SEVERE) && (c_mgr->type == TYPE_ERRORLOG)) ||
		( (dlevel <= c_mgr->level) && (dmodule & c_mgr->mod)) ) {

	    pthread_mutex_lock(&log_mutex);

	    assert(debuglog_buf_end >=0 && debuglog_buf_end < MAX_LOG_BUFSIZE);
	    len = MAX_LOG_BUFSIZE - debuglog_buf_end - 1;
	    if(len < 5000){
		/* Should be bigger enough for one single log entry */
		pthread_mutex_unlock(&log_mutex);
		if (c_mgr->type == TYPE_ERRORLOG)
		    glob_el_err_buf_overflow++;

		/*		if (c_mgr->type == TYPE_ACCESSLOG)
				glob_al_err_buf_overflow++;
		 */
		if (c_mgr->mod == MOD_TRACE)
		    glob_tr_err_buf_overflow++;

		if (c_mgr->mod == MOD_CACHE){
		    glob_cac_err_buf_overflow++;

		if(strstr(fmt,"ADD"))
		    glob_cac_err_add_overflow++;

		if(strstr(fmt,"DECAY"))
		    glob_cac_err_decay_overflow++;

		if(strstr(fmt,"DELETE"))
	            glob_cac_err_delete_overflow++;

		if(strstr(fmt,"UPDATE_ATTR"))
		    glob_cac_err_update_overflow++;
		}

		if (c_mgr->type == TYPE_STREAMLOG)
		    glob_str_err_buf_overflow++;
		break;
	    }
	    p = &debuglog_buf[debuglog_cur_id][debuglog_buf_end+sizeof(NLP_data)];
	    log_mgmt_timestamp(p);
	    slen = strlen(p);
	    p += slen;
	    len -= sizeof(NLP_data) + slen;
	    va_start(tAp, fmt);
	    ret = vsnprintf(p, len, fmt, tAp);
	    va_end(tAp);
	    /*
	     * It is possible that (ret > len).
	     * It only means that output is trunicated.
	     * We will skip this log message.
	     */
	    if (ret > 0 && ret < len) {
		pdata =(NLP_data *)&debuglog_buf[debuglog_cur_id][debuglog_buf_end];
		pdata->channelid = c_mgr->channelid;
		pdata->len=slen + ret;
		debuglog_buf_end += sizeof(NLP_data) + slen + ret;
		if (c_mgr->type == TYPE_ERRORLOG)
		    glob_el_tot_logged++;

		/*		if (c_mgr->type == TYPE_ACCESSLOG)
				glob_al_tot_logged++;
		 */
		if (c_mgr->mod == MOD_TRACE)
		    glob_tr_tot_logged++;

		if (c_mgr->type == TYPE_STREAMLOG)
		    glob_str_tot_logged++;

		if (c_mgr->mod == MOD_CACHE){
		    glob_cac_tot_logged++;
		if(strstr(fmt,"ADD"))
		    glob_cac_tot_logged_add++;
		if(strstr(fmt, "DECAY"))
	            glob_cac_tot_logged_decay++;
		if(strstr(fmt,"DELETE"))
	            glob_cac_tot_logged_delete++;
                if(strstr(fmt,"UPDATE_ATTR"))
	             glob_cac_tot_logged_update++;
		}

	    }
	    else{

		if (c_mgr->mod == MOD_CACHE){
		    glob_cac_err_len_overflow++;

		    if(strstr(fmt,"ADD"))
			glob_cac_err_add_len_overflow++;
		    if(strstr(fmt, "DECAY"))
			glob_cac_err_decay_len_overflow++;
		    if(strstr(fmt,"DELETE"))
			glob_cac_err_delete_len_overflow++;
		    if(strstr(fmt ,"UPDATE_ATTR"))
			glob_cac_err_update_len_overflow++;
		}
	    }
	    pthread_mutex_unlock(&log_mutex);
	}

	// next channel.
	c_mgr = c_mgr->next;
    }

    return ;

}

void
log_thread_end(void)
{
    void *value_ptr;

    if (logfd == -1) {
	return;
    }

    thread_exit = 1;
    pthread_join(log_pid, &value_ptr);
}

