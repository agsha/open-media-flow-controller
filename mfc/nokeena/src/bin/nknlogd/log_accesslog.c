#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>		/* only the defines on windows */
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <math.h>

#include "log_accesslog_pri.h"
#include "nknlogd.h"
#include "accesslog_pub.h"

#include "log_channel.h"
#include "log_accesslog.h"

#include "common.h"
#include "tstr_array.h"
#include "tpaths.h"
#include "file_utils.h"

/*----------------------------------------------------------------------------
 * MACROS/CONSTANTS
 *--------------------------------------------------------------------------*/
// Calculate time used in milliseconds.
// @a - time value (higher)
// @b - time value (lower)
// @diff - diff'ed value usec is always rounded off)
static inline void TIMERDIFF_MILLISEC(struct timeval a, struct timeval b,
	struct timeval *diff)
{
    int d = 0;
    diff->tv_sec  = a.tv_sec  - b.tv_sec;
    diff->tv_usec = a.tv_usec - b.tv_usec;
    d = diff->tv_usec;
    double dd = (d / 1000.0);

    d = (int) (round(dd) * 1000.0);
    diff->tv_usec = d;

    if (diff->tv_usec < 0) {
	    --(diff->tv_sec);
	    (diff->tv_usec) += 1000000;
    }
    return;
}

#define FAIL		(0x0)
#define SIZE_OK		(0x1)
#define RESP_CODE_OK	(0x2)
#define VERDICT_OK	(SIZE_OK | RESP_CODE_OK)

/*----------------------------------------------------------------------------
 * EXTERN FUNCTIONS
 *--------------------------------------------------------------------------*/
extern void accesslog_file_exit(struct channel *chan);
extern void accesslog_file_init(struct channel *chan);

/*----------------------------------------------------------------------------
 * GLOBAL FUNCTIONS
 *--------------------------------------------------------------------------*/
int itoa10(int n, char *s);
int utoa10(unsigned int n, char *s);
int ltoa10(long int n, char *s);
int ultoa10(unsigned long n, char *s);
void acclog_write(struct channel *chan, char *p, int len);
int check_log_restrictions(struct channel *chan, uint16_t resp_code,
			   uint32_t size);

/*----------------------------------------------------------------------------
 * PRIVATE FUNCTIONS
 *--------------------------------------------------------------------------*/
static void http_accesslog_write(struct channel *chan, char *psrc,
				 int len);
char *get_ip_str(ip_addr_t *ip_ptr);
static void fix_format_field(logd_file * plf, accesslog_item_t * item);

/*----------------------------------------------------------------------------
 * PRIVATE FUNCTIONS
 *--------------------------------------------------------------------------*/
static void fix_format_field(logd_file * plf, accesslog_item_t * item)
{
    int j;
    char *p;

    // Server will return the string in the same order that we sent out.
    // If it is none, server will fill in "-" for this field
    p = (char *) item + accesslog_item_s;
    for (j = 0; j < plf->ff_used; j++) {
	if (plf->ff_ptr[j].string[0] == 0)
	    continue;
	if (plf->ff_ptr[j].field == FORMAT_STRING)
	    continue;
	plf->ff_ptr[j].pdata = p;
	p += strlen(p) + 1;
    }
}
int check_log_restrictions(struct channel *chan, uint16_t resp_code,
			   uint32_t size)
{
    int filter = VERDICT_OK;	// Assume no filtering is done
    logd_file *plf = (chan->accesslog);

    if (plf->r_size && (size > 0) && (size <= plf->r_size)) {
	filter &= (~SIZE_OK);	// Restricted size.. turn off logging.
	goto bail;
    }

    if ((plf->al_rcode_ct)&&(plf->r_code)) {
	int i;
	for (i = 0; i < MAX_RESP_CODE_FILTER; i++) {
	    if (plf->r_code[i] == resp_code) {
		filter &= (~RESP_CODE_OK);	// restricted response code.
		goto bail;
	    }
	}
    }

  bail:
    return filter;
}

static int http_copy_pdata(char * pin, char * pout)
{
  /*
   * non-printable and other special characters in %r, %i and %o are escaped 
   * using \xhh sequences, where hh stands for the hexadecimal representation 
   * of the raw byte. 
   * Exceptions from this rule are " and \, which are escaped by prepending a 
   * backslash, and all whitespace characters, which are written in their 
   * C-style notation (\n, \t, etc). 
   */
   int in, out;

   in = 0;
   out = 0;
   while (pin[in] != '\0') {
	if (pin[in] == '\"') {
	   pout[out] = '\\'; out++;
	   pout[out] = '\"'; out++;
	}
	else if (pin[in] == '\\') {
	   pout[out] = '\\'; out++;
	   pout[out] = '\\'; out++;
	}
	else {
	   pout[out] = pin[in]; out++;
	}
	in ++;
    }
   return out;
}

static void http_accesslog_write(struct channel *chan, char *psrc, int len)
{
    int i, j;
    char *p;
    const char *value;
    char logbuf[LOCAL_MAX_BUFSIZE];
    struct in_addr in;
    accesslog_item_t *item;
    uint32_t obj_size;
    int resp_code;
    unsigned int l = 0;
    int offset;
    int err = 0;
    const char * query_string = NULL;
    char uri[16384];
    tstring * q_str = NULL;
    logd_file *plf = (chan->accesslog);

    item = (accesslog_item_t *) psrc;
    fix_format_field(plf, item);
    obj_size = item->out_bytes;
    resp_code = item->status;

    int filter_verdict = check_log_restrictions(chan, resp_code, obj_size);
    if (filter_verdict != VERDICT_OK) {
	return;
    }

  LOG:
    p = &logbuf[0];

    for (j = 0; j < plf->ff_used; j++) {
	value = NULL;

	switch (plf->ff_ptr[j].field) {

	    /*
	     * Whatever configured, copy as is.
	     */
	case FORMAT_STRING:
	    i = 0;
	    while (plf->ff_ptr[j].string[i] != '\0') {
		    p[i] = plf->ff_ptr[j].string[i];
		    i++;
	    }
	    p += i;
	    //p += sprintf(p, "%s", plf->ff_ptr[j].string);
	    break;

	    /*
	     * Time stamp
	     */
	case FORMAT_TIMESTAMP:
	    /* cache the generated timestamp */
	    //[02/Oct/2008:18:54:52 -0700]
	    {
		struct tm tm;
		localtime_r(&(item->end_ts.tv_sec), &tm);
		l = strftime(p, 255, "[%d/%b/%Y:%H:%M:%S %z]", &tm);
		p += l;
		break;
	    }

	    /*
	     * Elapsted Time
	     */
	case FORMAT_TIME_USED_SEC:
	    if (item->start_ts.tv_sec != 0) {	//parser error
		p += sprintf(p, "%ld",
			     item->end_ts.tv_sec - item->start_ts.tv_sec);
	    } else {
		strcpy(p, "0.0");
		p += 3;;
	    }
	    break;
	case FORMAT_TIME_USED_MS:
	    if (item->start_ts.tv_sec != 0) {	//parser error
		struct timeval diff = { 0, 0 };
		// Calculate time used in milliseconds.
		TIMERDIFF_MILLISEC(item->end_ts, item->start_ts, &diff);
		p += sprintf(p, "%ld",
			     diff.tv_sec * 1000 + (diff.tv_usec / 1000));
	    } else {
		strcpy(p, "0.0");
		p += 3;
	    }
	    break;
	case FORMAT_REQUEST_IN_TIME:
	    p += sprintf(p, "%ld.%ld", item->start_ts.tv_sec,
			 item->start_ts.tv_usec / 1000);
	    break;
	case FORMAT_FBYTE_OUT_TIME:
	    p += sprintf(p, "%ld.%ld", item->ttfb_ts.tv_sec,
			 item->ttfb_ts.tv_usec / 1000);
	    break;
	case FORMAT_LBYTE_OUT_TIME:
	    p += sprintf(p, "%ld.%ld", item->end_ts.tv_sec,
			 item->end_ts.tv_usec / 1000);
	    break;
	case FORMAT_LATENCY:
	    if ((item->ttfb_ts.tv_sec != 0)
		&& (item->start_ts.tv_sec != 0)) {
		struct timeval diff = { 0, 0 };
		TIMERDIFF_MILLISEC(item->ttfb_ts, item->start_ts, &diff);

		p += sprintf(p, "%ld",
			     diff.tv_sec * 1000 + (diff.tv_usec / 1000));
	    } else {
		strcpy(p, "0.0");
		p += 3;
	    }
	    break;
	case FORMAT_DATA_LENGTH_MSEC:
	    if (item->ttfb_ts.tv_sec != 0) {
		struct timeval diff = { 0, 0 };
		TIMERDIFF_MILLISEC(item->end_ts, item->ttfb_ts, &diff);

		p += sprintf(p, "%ld",
			     diff.tv_sec * 1000 + (diff.tv_usec / 1000));
	    } else {
		strcpy(p, "0.0");
		p += 3;
	    }
	    break;

	    /*
	     * These fields have been sent over to server in the configuration handshake time
	     * so all we need to do here is pick up the data.
	     */
	case FORMAT_FILENAME:
	case FORMAT_REMOTE_USER:
	case FORMAT_REQUEST_HEADER:
	case FORMAT_RESPONSE_HEADER:
	case FORMAT_SERVER_NAME:
	    //case FORMAT_URL:
	case FORMAT_COOKIE:
	    if (plf->ff_ptr[j].pdata && plf->ff_ptr[j].pdata[0] != '\0') {
	    	p += http_copy_pdata(plf->ff_ptr[j].pdata, p);
	    } else {
		*p = '-';
		p++;
	    }
	    break;

        case FORMAT_QUERY_STRING:
	    offset = http_copy_pdata(plf->ff_ptr[j].pdata, uri);
	    if (offset == 0) goto free_str;
	    uri[offset] = '\0';

	    err = ts_new_str(&q_str, uri);
	    if(err)
		goto free_str;

            err = ts_find_char(q_str, 0, '?', &offset);
            if(err)
                goto free_str;

            if (offset != -1 ) {
		// query string found. strip entire query
                query_string = ts_str_nth((const tstring *)q_str,
			(unsigned int)(offset + 1));
		if (query_string != NULL) {
		    strcpy(p, query_string);//TODO :: strcpy should be modified to strlcpy
		    p += strlen(p);
		} // string written to log buffer
             } else {
        free_str:
                *p = '-';
                 p++;
             }
	    ts_free(&q_str); // BZ 11293 - MUST free q_str lest we leak!
            query_string = NULL;
            break;

	case FORMAT_HTTP_HOST:
	    if (plf->ff_ptr[j].pdata && plf->ff_ptr[j].pdata[0] != '\0') {
	    	p += http_copy_pdata(plf->ff_ptr[j].pdata, p);
	    } else {
		*p = '-';
		p++;
	    }
	    break;
	case FORMAT_REQUEST_LINE:
	    if (plf->ff_ptr[j].pdata && plf->ff_ptr[j].pdata[0] != '\0') {
	    	p += http_copy_pdata(plf->ff_ptr[j].pdata, p);
	    } else {
		*p = '-';
		p++;
	    }
	    break;
	case FORMAT_NAMESPACE_NAME:
	    if (plf->ff_ptr[j].pdata && plf->ff_ptr[j].pdata[0] != '\0') {
	    	p += http_copy_pdata(plf->ff_ptr[j].pdata, p);
	    } else {
		*p = '-';
		p++;
	    }
	    break;
	case FORMAT_HOTNESS:
	    if (plf->ff_ptr[j].pdata && plf->ff_ptr[j].pdata[0] != '\0') {
	    	p += http_copy_pdata(plf->ff_ptr[j].pdata, p);
	    } else {
		*p = '-';
		p++;
	    }
	    break;
	case FORMAT_CACHE_HIT:
	case FORMAT_ORIGIN_SIZE:
	case FORMAT_CACHE_HISTORY:
	    if (plf->ff_ptr[j].pdata && plf->ff_ptr[j].pdata[0] != '\0') {
	    	p += http_copy_pdata(plf->ff_ptr[j].pdata, p);
	    } else {
		*p = '-';
		p++;
	    }
	    break;
	case FORMAT_URL:
	    if (plf->ff_ptr[j].pdata && plf->ff_ptr[j].pdata[0] != '\0') {
	    	p += http_copy_pdata(plf->ff_ptr[j].pdata, p);
	    } else {
		*p = '-';
		p++;
	    }
	    break;

	    /*
	     * HTTP transaction response code.
	     */
	case FORMAT_PEER_STATUS:
	case FORMAT_STATUS:
	    p += itoa10(item->status, p);
	    break;

	    /*
	     * HTTP transaction response subcode.
	     */
	case FORMAT_STATUS_SUBCODE:
	    p += itoa10(item->subcode, p);
	    break;

	    /*
	     * Bytes counter information
	     */
	case FORMAT_BYTES_OUT:
	    p += ltoa10(item->out_bytes + item->resp_hdr_size, p);
	    break;
	case FORMAT_BYTES_IN:
	    p += ltoa10(item->in_bytes, p);
	    break;
	case FORMAT_BYTES_OUT_NO_HEADER:
	    p += ltoa10(item->out_bytes, p);
	    break;

	    /*
	     * All fields are set in the flag
	     */
	case FORMAT_REQUEST_METHOD:
	    if (CHECK_ALITEM_FLAG(item, ALF_METHOD_GET)) {
		strcpy(p, "GET");
		p += 3;
	    } else if (CHECK_ALITEM_FLAG(item, ALF_METHOD_HEAD)) {
		strcpy(p, "HEAD");
		p += 4;
	    } else if (CHECK_ALITEM_FLAG(item, ALF_METHOD_POST)) {
		strcpy(p, "POST");
		p += 4;
	    } else if (CHECK_ALITEM_FLAG(item, ALF_METHOD_CONNECT)) {
		strcpy(p, "CONNECT");
		p += 7;
	    } else if (CHECK_ALITEM_FLAG(item, ALF_METHOD_OPTIONS)) {
		strcpy(p, "OPTIONS");
		p += 7;
	    } else if (CHECK_ALITEM_FLAG(item, ALF_METHOD_PUT)) {
		strcpy(p, "PUT");
		p += 3;
	    } else if (CHECK_ALITEM_FLAG(item, ALF_METHOD_DELETE)) {
		strcpy(p, "DELETE");
		p += 6;
	    } else if (CHECK_ALITEM_FLAG(item, ALF_METHOD_TRACE)) {
		strcpy(p, "TRACE");
		p += 5;
	    } else {
		strcpy(p, "UNKNOWN");
		p += 7;
	    }
	    break;
	case FORMAT_REQUEST_PROTOCOL:
	    value = CHECK_ALITEM_FLAG(item, ALF_HTTP_10) ?
		"HTTP/1.0" : (CHECK_ALITEM_FLAG(item, ALF_HTTP_11) ?
			      "HTTP/1.1" : "HTTP/0.9");
	    break;
	case FORMAT_CONNECTION_STATUS:
	    value = CHECK_ALITEM_FLAG(item, ALF_CONNECTION_KEEP_ALIVE) ?
		"Keep-Alive" : "Close";
	    break;
	case FORMAT_CONNECTION_TYPE:
	    value = CHECK_ALITEM_FLAG(item, ALF_SECURED_CONNECTION) ?
		"https" : "http";
	    break;
	    /*
	     * Server side information
	     */
	case FORMAT_SERVER_PORT:
	    p += sprintf(p, "%d", item->dst_port);
	    break;
	case FORMAT_REMOTE_ADDR:
	case FORMAT_REMOTE_HOST:
	    //in.s_addr=item->src_ip;
	    //p += sprintf(p, "%s", inet_ntoa(in));
	    p += sprintf(p, "%s", get_ip_str(&(item->src_ipv4v6)));
	    break;
	case FORMAT_PEER_HOST:
	case FORMAT_LOCAL_ADDR:
	    //in.s_addr=item->dst_ip;
	    //p += sprintf(p, "%s", inet_ntoa(in));
	    p += sprintf(p, "%s", get_ip_str(&(item->dst_ipv4v6)));
	    break;
	case FORMAT_REVALIDATE_CACHE:
	    if (CHECK_ALITEM_FLAG(item, ALF_RETURN_REVAL_CACHE)) {
		strcpy(p, "Revalid");
		p += 7;
	    } else {
		strcpy(p, "-");
		p += 1;
	    }
	    break;
       case FORMAT_ORIGIN_SERVER_NAME:
           if (plf->ff_ptr[j].pdata && plf->ff_ptr[j].pdata[0] != '\0') {
	    	p += http_copy_pdata(plf->ff_ptr[j].pdata, p);
           } else {
               *p = '-';
               p++;
           }
           break;
	case FORMAT_COMPRESSION_STATUS:
	    if (CHECK_ALITEM_FLAG(item, ALF_RETURN_OBJ_GZIP)) {
		strcpy(p, "gzip");
		p += 4;
	    } else if(CHECK_ALITEM_FLAG(item, ALF_RETURN_OBJ_DEFLATE)){
		strcpy(p, "deflate");
		p += 7;
	    } else{
		strcpy(p, "-");
		p += 1;
	    }
	    break;

	case FORMAT_EXPIRED_DEL_STATUS:
	    if (CHECK_ALITEM_FLAG(item, ALF_EXP_OBJ_DEL)) {
		strcpy(p, "expired"); p += 7;
	    } else {
		strcpy(p, "-"); p += 1;
	    }
	    break;
	    /*
	     * To be implemented fields
	     */
	    //case FORMAT_RFC_931_AUTHENTICATION_SERVER:
	    //case FORMAT_REMOTE_IDENT:
	    //case FORMAT_ENV:
	default:
	    *p = '-';
	    p++;
	    break;
	}

	if (value) {
	    strcpy(p, value);
	    p += strlen(p);
	}
    }

    *p = '\n';
    p++;

    acclog_write(chan, logbuf, p - logbuf);
}


#define NUM_TO_DIGITS(s, n, i, __type__)    \
    (s) = (tempbuf + sizeof(__type__) * 4); \
    *--(s) = '\0';                          \
    do                                      \
        *--(s) = digits[(n) % 10], i++;     \
    while ( ( (n) /= 10 ) != 0)

int itoa10(int n, char *s)
{
    static char digits[] = "0123456789";
    char *p1;
    //register char *end;
    register int i = 0, sign;
    char *s1, tempbuf[sizeof(int) * 4] = {0};


    // no buff??
    if (s == NULL)
	return 0;

    // fast case.. no need to do any
    // compute if we get a 0
    if (n == 0) {
	s[i++] = '0';
	s[i] = '\0';
	return i;
    }
    // check sign and remember it
    if ((sign = n) < 0) {
	n = -n;
    }
    // Convert the number to digits..
    // s1 - points to the beginning of the buffer
    //      where the string starts
    // n - always a positive integer
    // i number of bytes written
    NUM_TO_DIGITS(s1, n, i, int);

    if (sign < 0) {
	*s++ = '-', i++;
    }

    p1 = s;
    for (; *s1 != '\0'; s1++) {
	*p1++ = *s1;
    }

    // return the number of bytes we used.
    return i;
}

int utoa10(unsigned int n, char *s)
{
    static char digits[] = "0123456789";
    char *p1 = NULL;
    register int i = 0;
    char *s1, tempbuf[sizeof(int) * 4] = {0};

    if (s == NULL)
	return 0;

    // fast case.. no need to do any
    // compute if we get a 0
    if (n == 0) {
	s[i++] = '0';
	s[i] = '\0';
	return i;
    }
    // Convert the number to digits..
    // s1 - points to the beginning of the buffer
    //      where the string starts
    // n - always a positive integer
    // i number of bytes written
    NUM_TO_DIGITS(s1, n, i, unsigned int);

    p1 = s;
    for (; *s1 != '\0'; s1++) {
	*p1++ = *s1;
    }

    // return the number of bytes we used.
    return i;
}

int ltoa10(long int n, char *s)
{
    static char digits[] = "0123456789";
    char *p1, *p2;
    register long int i = 0, sign;
    char *s1, tempbuf[sizeof(long int) * 4];

    // no buff??
    if (s == NULL)
	return 0;

    // fast case.. no need to do any
    // compute if we get a 0
    if (n == 0) {
	s[i++] = '0';
	s[i] = '\0';
	return i;
    }
    // check sign and remember it
    if ((sign = n) < 0) {
	n = -n;
    }
    // Convert the number to digits..
    // s1 - points to the beginning of the buffer
    //      where the string starts
    // n - always a positive integer
    // i number of bytes written
    NUM_TO_DIGITS(s1, n, i, long);

    if (sign < 0) {
	*s++ = '-', i++;
    }

    p1 = s;
    for (; *s1 != '\0'; s1++) {
	*p1++ = *s1;
    }

    // return the number of bytes we used.
    return i;
}


int ultoa10(unsigned long n, char *s)
{
    static char digits[] = "0123456789";
    char *p1, *p2;
    register long int i = 0;
    char *s1, tempbuf[sizeof(long int) * 4];

    if (s == NULL)
	return 0;

    // fast case.. no need to do any
    // compute if we get a 0
    if (n == 0) {
	s[i++] = '0';
	s[i] = '\0';
	return i;
    }
    // Convert the number to digits..
    // s1 - points to the beginning of the buffer
    //      where the string starts
    // n - always a positive integer
    // i number of bytes written
    NUM_TO_DIGITS(s1, n, i, unsigned long);

    p1 = s;
    for (; *s1 != '\0'; s1++) {
	*p1++ = *s1;
    }

    // return the number of bytes we used.
    return i;
}
static __thread char ip_str[INET6_ADDRSTRLEN + 1];
char *get_ip_str(ip_addr_t *ip_ptr)
{
    ip_str[INET6_ADDRSTRLEN] = 0;

    if (ip_ptr == NULL ||
        (ip_ptr->family != AF_INET && ip_ptr->family != AF_INET6)){
        strncpy(ip_str, "invalid IP", INET6_ADDRSTRLEN);
    } else if (ip_ptr->family == AF_INET) {
        inet_ntop(AF_INET, &(ip_ptr->addr.v4),
                  ip_str, INET_ADDRSTRLEN);
    } else {
        inet_ntop(AF_INET6, &(ip_ptr->addr.v6),
                  ip_str, INET6_ADDRSTRLEN);
    }

    return ip_str;
}

void log_channel_peer_shut(struct channel *chan);
void log_channel_peer_shut(struct channel *chan)
{
    safe_close(chan->sku_fd);
    if (chan->name) {
	free(chan->name);
	chan->name = NULL;
    }
    /* Clean reference to config */
    chan->state = 0;
    chan->info[0] = 0;
    //Have to make the parent child,accesslog,data as null,else we might work on stale data PR796660
    memset(chan->data.buf,0,MAX_BUFSIZE);
    chan->data.offsetlen = 0;
    chan->data.totlen = 0;
    chan->parent = NULL;
    chan->child = NULL;
    chan->accesslog = NULL;
    sku_put((struct socku *) (chan));
    return;
}


void acclog_uds_close_unregister(struct channel *chan);
void acclog_uds_close_unregister(struct channel *chan)
{
    chan->epollin = NULL;
    chan->epollout = NULL;
    chan->epollerr = NULL;
    chan->epollhup = NULL;

    //printf("%s : closing socket: %s\n", __FUNCTION__, chan->name);

    log_channel_peer_shut(chan);
}

int acclog_uds_epollin(struct socku *sku)
{
    struct channel *chan = (struct channel *) sku;
    struct al_data *plogb = (struct al_data *) &(chan->data);
    NLP_data *pdata = NULL;
    int len;

    errno = 0; // set to 0 so we can debug what recv() returns on error
    len = recv(chan->sku_fd, &plogb->buf[plogb->offsetlen + plogb->totlen],
	       MAX_BUFSIZE - plogb->offsetlen - plogb->totlen, 0);

    if (len <= 0) {
	// Socket closed..
	acclog_uds_close_unregister(chan);
	return -1;
    }

    plogb->totlen += len;

    while (1) {
	if (plogb->totlen == 0) {
	    plogb->offsetlen = 0;
	    return 0;
	}

	pdata = (NLP_data *) & plogb->buf[plogb->offsetlen];
	if (plogb->totlen < (pdata->len + sizeof(NLP_data))) {
	    // Need more data
	    if (plogb->offsetlen > MAX_BUFSIZE / 2) {
		memcpy(&plogb->buf[0], &plogb->buf[plogb->offsetlen],
		       plogb->totlen);
		plogb->offsetlen = 0;
	    }
	    return 0;
	}

	if (chan->accesslog->channelid != pdata->channelid) {
            accesslog_file_exit(chan);
            accesslog_file_init(chan);

	    acclog_uds_close_unregister(chan);
	    return -1;
	}
	/* Protect config so it doesnt disappear from under us */
	pthread_mutex_lock(&(chan->accesslog->lock));
	http_accesslog_write(chan,
			     &plogb->buf[plogb->offsetlen +
					sizeof(NLP_data)], pdata->len);
	pthread_mutex_unlock(&(chan->accesslog->lock));

	plogb->offsetlen += (sizeof(NLP_data) + pdata->len);
	plogb->totlen -= (sizeof(NLP_data) + pdata->len);
    }


    return 0;
}

int acclog_uds_epollout(struct socku *sku)
{
    acclog_uds_close_unregister((struct channel *) sku);
    return -1;
}

int acclog_uds_epollerr(struct socku *sku)
{
    acclog_uds_close_unregister((struct channel *) sku);
    return -1;
}

int acclog_uds_epollhup(struct socku *sku)
{
    acclog_uds_close_unregister((struct channel *) sku);
    return -1;
}


int al_handle_new_send_socket(struct channel *ch)
{
    char buf[1024] = { 0 };
    struct channel *c = NULL;
    logd_file *plf = NULL;
    NLP_accesslog_channel *plog = NULL;
    NLP_server_hello *resp = NULL;
    int numoflog = 1;
    char *p = NULL;
    int j = 0, ret = 0;

    //printf("%s : new socket: %s\n", __FUNCTION__, ch->name);

    resp = (NLP_server_hello *) buf;
    p = (char *) &buf[sizeof(NLP_server_hello)];

    ch->epollin = acclog_uds_epollin;
    ch->epollout = acclog_uds_epollout;
    ch->epollerr = acclog_uds_epollerr;
    ch->epollhup = acclog_uds_epollhup;

    log_channel_get(ch->name, &c);

    plf = (c->accesslog);
    /* Clone the config from parent to our new channel */
    c->accesslog->data = (void *) ch;
    // c - this has the config->accesslog
    plog = (NLP_accesslog_channel *) p;
    plog->channelid = plf->channelid;
    plog->num_of_hdr_fields = 0;
    plf->type = TYPE_ACCESSLOG;

    p += sizeof(NLP_accesslog_channel);
    for (j = 0; j < plf->ff_used; j++) {
	if (plf->ff_ptr[j].string[0] == 0)
	    continue;
	if (plf->ff_ptr[j].field == FORMAT_STRING)
	    continue;
	strcpy(p, plf->ff_ptr[j].string);
	p += (strlen(p) + 1);
	plog->num_of_hdr_fields++;

    }

    plog->len = p - (char *) plog - sizeof(NLP_accesslog_channel);

    resp->numoflog = numoflog;
    resp->lenofdata = p - (char *) (&buf[sizeof(NLP_server_hello)]);

    ret = send(ch->sku_fd, buf, p - (char *) buf, 0);
    complain_require_msg((ret > 0), "log format negotitation failed.. peer seemingly closed.");
    ret = (ret > 0) ? 0 : -1;
    if (!ch->accesslog->fp) {
        accesslog_file_exit(ch);
	accesslog_file_init(ch);
    }

    return 0;
}

void acclog_write(struct channel *chan, char *p, int len)
{
    int ret;
    time_t now;
    struct tm ltime;
    logd_file *plf = (chan->accesslog);

    /* Drop logs.. because file isnt open yet */
    if (plf->fp == NULL)
	return;

    while (len) {
	ret = fwrite(p, 1, len, plf->fp);
	if (ret <= 0) {
	    log_write_report_err(ret, plf);
	    return;
	}
	p += ret;
	len -= ret;
	plf->cur_filesize += ret;
    }

    /* Check whether time-interval is hit, if so, close & re-init
     * else, chck if size is hit.
     * If neither, return.
     */

    if ((plf->max_filesize)
	    && (plf->cur_filesize > plf->max_filesize)) {
	goto reinit;
    }

    return;

reinit:
    /* Need to free the lock because we intend changing the config */
    pthread_mutex_unlock(&(plf->lock));
    accesslog_file_exit(chan);
    accesslog_file_init(chan);
    pthread_mutex_lock(&(plf->lock));
}

/*
 * align_to_rotation_interval will align the rotation interval of
 * the files to the configured rotation interval
 * align_to_rotation_interval will be invoked every 2 sec and the
 * significant changes happens when called on every file_init
 * where the mod_val is calculated which specifies when to reinit
 * and this is tracked closesly by cal_rt_interval which gets updated on
 * minute basis
 * Eg: current time =10:03,rt_interval = 5 ,then the reinit should happen
 * at 10:05 and there on the rotation should happen @ every 5 min interval.
 * On intialization(first entry) prev_rt_interval will be 0
 * Assign minute_interval  the current min then calculate mod_val
 * mod_val in this case is 3,which helps in framing file start_time
 * then cal_rt_interval will evaluate when to reinit based on the cur_min
 *
 *
 *
 * rotation will work only if the rotation interval is configured
 * in multiples of 5
 */

int align_to_rotation_interval( logd_file *plf, time_t now)
{
    struct tm ltime;
    int reinit = 0;

    if((plf == NULL)||(!plf->rt_interval)|| (!plf->pai)){
	return 0 ;
    }

    aln_interval *pal = plf->pai;
    localtime_r(&now, &ltime);
    pal->cur_rt_interval = plf->rt_interval;
    pal->cur_min = ltime.tm_min;
    if((pal->prev_rt_interval !=
		pal->cur_rt_interval) ){
	pal->prev_rt_interval_ref = pal->prev_rt_interval;
	pal->prev_rt_interval = pal->cur_rt_interval;
	pal->minute_interval = ltime.tm_min;
	//mod_val will help to evaluate the file start time
	if( pal->prev_rt_interval_ref == 0 ){
	    pal->mod_val =
		pal->minute_interval%pal->cur_rt_interval;
	}else {
	    // When  max-duration is changed in between PR 782719
	    pal->mod_val =
		pal->minute_interval%pal->prev_rt_interval_ref;
	}
    }
    if(pal->mod_val){
	// cal_rt_interval will specify the time to reinit the log files based on cur_min
	pal->cal_rt_interval =
	    pal->cur_min%pal->cur_rt_interval;
    }

    //ensure reinit happens only at the rotation interval
    if((pal->cur_min) && ((pal->cur_min%pal->cur_rt_interval) == 0)){
	/*
	 * spurious reinit of files happens in the same minute range when timer
	 * hits for every 2 sec  say cur_min is 10 then for current time is
	 * 10:10:00 without the condition (ltime.tm_sec < 2)
	 * reinit will happen @ 10:10:00,10:10:02,10:10:04..etc
	 */
	if((pal->cal_rt_interval == 0) && (ltime.tm_sec < 2)){

	    pal->file_start_min =
		abs(pal->mod_val - pal->minute_interval);
	    pal->file_end_min =
		pal->file_start_min + pal->cur_rt_interval;
	    pal->mod_val = 0;
	    pal->use_file_time = 1;
	    pal->cal_rt_interval = 1; //set this value so that we dont intialize file start and end time again
	    reinit = 1;

	}
    }//To rotate at every hour when pal->cur min is 0
    else if((pal->cur_min == 0 ) && (ltime.tm_sec < 2)) {

	if(pal->cal_rt_interval == 0){
	    pal->file_start_min =
		abs(pal->mod_val - pal->minute_interval);
	    pal->file_end_min =
		pal->file_start_min + pal->cur_rt_interval;
	    pal->mod_val = 0;
	    pal->use_file_time = 1;
	    pal->cal_rt_interval = 1; //set this value so that we dont intialize file start and end time again
	    reinit = 1;

	}
    }
    return reinit ;
}
