/*
 * libnknalog.c -- Generic client access log interface
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/queue.h>
#include <sys/epoll.h>
#include <sys/prctl.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "libnknalog.h"
#include "nknlog_pub.h"

// #define UNUSED_ARGUMENT(_x) (void)_x
#define MAX_EPOLL_FDS 16
#define MAX_ACCESSLOG_BUFSIZE (16 * 1024 * 1024)

#define ALI_RD_LOCK(_ali) \
    do { \
    	pthread_rwlock_rdlock(&(_ali)->lock); \
    } while(0)

#define ALI_RD_UNLOCK(_ali) \
    do { \
    	pthread_rwlock_unlock(&(_ali)->lock); \
    } while(0)

#define ALI_WR_LOCK(_ali) \
    do { \
    	pthread_rwlock_wrlock(&(_ali)->lock); \
    } while(0)

#define ALI_WR_UNLOCK(_ali) \
    do { \
    	pthread_rwlock_unlock(&(_ali)->lock); \
    } while(0)

typedef struct alog_stat {
    uint64_t tot_logged;
    uint64_t err_buf_overflow;
    uint64_t tot_dropped;
} alog_stat_t;

typedef struct alog_info {
    char *name;
    pthread_rwlock_t lock;
    int fd;
    int in_use;
    int32_t channelid;
    int32_t num_al_list;
    char *al_list[32];
    int32_t al_resp_header_configured;
    int32_t al_record_cache_history;
    char *al_buf;
    uint32_t al_buf_start;
    uint32_t al_buf_end;
    alog_stat_t stats;
} alog_info_t;

static int thr_is_running = 0;
static pthread_t thr_pid = 0;
static alog_info_t alog_info_data;
char alog_data_buf[MAX_ACCESSLOG_BUFSIZE];
char alog_handshake_buf[1024 * 1024];

uint64_t glob_alog_tot_logged;
uint64_t glob_alog_err_buf_overflow;


static 
int alog_info_add(alog_info_t *ali, NLP_accesslog_channel *plog, char *p)
{
    int i = 0;

    if (plog->num_of_hdr_fields >= 32) {
	return 0;
    }

    ali->channelid = plog->channelid;
    ali->num_al_list = plog->num_of_hdr_fields;
    ali->al_resp_header_configured = 0;
    ali->al_record_cache_history = 0;

    for (i = 0; i < plog->num_of_hdr_fields; i++) {
	ali->al_list[i] = strdup(p);

	if(ali->al_list[i] == NULL){
	    return 0;
	}
	p += (strlen(p) + 1);

	switch (*(int *)(&ali->al_list[i][0]))
	{
	case 0x7365722e: /* ".res" */
	    ali->al_resp_header_configured = 1;
	    break;
	case 0x67726f2e: /* ".org" */
	case 0x7468632e: /* ".cht" */
	    ali->al_record_cache_history = 1;
	    break;
	}
    }
    return 1;
}

static 
void alog_info_freeall(alog_info_t *ali)
{
    int i;

    if (ali->in_use == 0) {
	return;
    }

    for (i = 0; i < ali->num_al_list; i++) {
        if (ali->al_list[i]) {
            free(ali->al_list[i]);
	}
    }
}

static 
int alog_handshake_accesslog(alog_info_t *ali)
{
    NLP_client_hello *req = NULL;
    NLP_server_hello *resp = NULL;
    int ret = 0;
    int len = 0; 
    int i = 0;
    NLP_accesslog_channel *plog;
    char *p = NULL;

    /* Send request to server */
    req = (NLP_client_hello *)alog_handshake_buf;
    strcpy(req->sig, SIG_SEND_ACCESSLOG);
    if (send(ali->fd, alog_handshake_buf, sizeof(NLP_client_hello), 0) <= 0) {
	return errno;
    }

    /* Read response from server */
    ret = recv(ali->fd, alog_handshake_buf, sizeof(NLP_server_hello), 0);
    if (ret < (int) sizeof(NLP_server_hello)) {
	return errno;
    }

    resp = (NLP_server_hello *)alog_handshake_buf;
    len = resp->lenofdata;

    if (len != 0) {
	ret = recv(ali->fd, &alog_handshake_buf[sizeof(NLP_server_hello)], 
		   len, 0);
	if (ret < len) {
	    return errno;
	}

	p = &alog_handshake_buf[sizeof(NLP_server_hello)];
    	ALI_WR_LOCK(ali);
	alog_info_freeall(ali);

	for (i = 0; i < resp->numoflog; i++) {
	    plog = (NLP_accesslog_channel *) p;

	    p += sizeof(NLP_accesslog_channel);

	    if (!alog_info_add(ali, plog, p)) {
		return -1;
	    }
	    p += plog->len;
	}
    	ALI_WR_UNLOCK(ali);
    }
    return 0;
}

static
int alog_init_socket(alog_info_t *ali)
{
    struct sockaddr_un su;
    int addrlen;
    int ret;

    if (ali->fd >= 0) {
	close(ali->fd);
	ali->fd = -1;
    }

    ali->fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (ali->fd < 0) {
    	return -1;
    }

    su.sun_family = AF_UNIX;
    addrlen = snprintf(su.sun_path, sizeof(su.sun_path), "%s/uds-acclog-%s", 
		       LOG_UDS_PATH, ali->name);
    addrlen += sizeof(su.sun_family);

    ret = connect(ali->fd, (struct sockaddr *)&su, addrlen);
    if (ret < 0) {
    	close(ali->fd);
    	ali->fd = -1;
	return -2;
    }
    return 0; // Success
}

static
int alog_send_data(alog_info_t *ali)
{
    int ret = 0;
    char *p = NULL;
    int len = 0;
    int sent = 0;

    ALI_RD_LOCK(ali);
    p = &ali->al_buf[ali->al_buf_start];
    len = ali->al_buf_end - ali->al_buf_start;

    while (len) {
	ret = send(ali->fd, p, len, 0);
	if (ret == -1) {
	    ALI_RD_UNLOCK(ali);
	    return -1;
	} 
	p += ret;
	len -= ret;
	ali->al_buf_start += ret;
	sent += ret;
    }

    ali->al_buf_start = 0;
    ali->al_buf_end = 0;

    ALI_RD_UNLOCK(ali);
    return sent;
}

static 
void *alog_thread_func(void *arg)
{
    int ret;
    alog_info_t *alp;
    int efd = 0;
    struct epoll_event ev;
    struct epoll_event events[MAX_EPOLL_FDS];

    prctl(PR_SET_NAME, "alog", 0, 0, 0);

    if (arg) {
    	alp = (alog_info_t *)arg;
    } else {
    	return 0;
    }

    efd = epoll_create(MAX_EPOLL_FDS);
    if (efd < 0) {
    	return 0;
    }

    while (1) {
	ret = alog_init_socket(alp);
	if (ret < 0) {
	    sleep(10);
	    continue;
	}

	ret = alog_handshake_accesslog(alp);
	if (ret < 0) {
	    sleep(10);
	    continue;
	}

	memset((char *)&ev, 0, sizeof(ev));
	ev.events = EPOLLIN;
	ev.data.fd = alp->fd;
	if (epoll_ctl(efd, EPOLL_CTL_ADD, alp->fd, &ev) < 0) {
	    sleep(10);
	    continue;
	}

	while (1) {
	    ret = alog_send_data(alp);
	    if (!ret) {
	    	ret = epoll_wait(efd, events, MAX_EPOLL_FDS, 1000); // 1 sec
		if (ret) { // server close
		    break;
		}
	    }
	} // end while

    } // end while
}

static 
char *get_parseCookie(char *cookiename, const char *cookieline, 
		      int cookieline_len)
{
    static char buf[128];
    int len;
    char * p, * pend;

    if (cookieline_len > 127) cookieline_len = 127;
    memcpy(buf, cookieline, cookieline_len);
    buf[cookieline_len] = 0;
    len = strlen(cookiename);
    // Find the exact match
    if (strncasecmp(buf, cookiename, len)) return NULL;
    p = &buf[len];
    while (*p != 0 && *p != '=') {
	if (*p != ' ' && *p != '\t') return NULL;
	p++;
    }
    if (*p == 0) return NULL;
    p++;
    while (*p != 0) {
	// Skip the leading spaces
	while (*p ==  ' ' || *p == '\t') p++;
	pend = p;
	while (*pend != 0) {
	    if (*pend == ';') break;
	    pend++;
	}
	*pend = 0;
	return p;
    }
    return NULL;
}

int alog_write(alog_token_t alog_token, alog_conn_data_t *cd, token_data_t td)
{
    UNUSED_ARGUMENT(td);

    alog_info_t *ali = (alog_info_t *)alog_token;
    int32_t buf_len;
    NLP_data *pdata;
    int i;
    char *p;
    char *ps;
    char *pname;
    struct accesslog_item *item;
    int32_t list_len;
    const char *pvalue;
    int32_t pvalue_len;
    int hdrcnt;
    ProtoHTTPHeaderId token;
    int rv;
    int nth;

    if (!ali) {
    	return 1;
    }
    glob_alog_tot_logged++;

    ALI_WR_LOCK(ali);
    buf_len = MAX_ACCESSLOG_BUFSIZE - ali->al_buf_end;

    if (buf_len < (int) (sizeof(NLP_data) + sizeof(struct accesslog_item))) {
	glob_alog_err_buf_overflow++;
	goto skip_logit;
    }

    pdata = (NLP_data *) &(ali->al_buf[ali->al_buf_end]);
    ps = (char *)(pdata) + sizeof(NLP_data);
    pdata->channelid = ali->channelid;
    buf_len -= (sizeof(NLP_data) + sizeof(struct accesslog_item));
    item = (struct accesslog_item *) ps;

    item->start_ts = cd->start_ts;
    item->end_ts = cd->end_ts;
    item->ttfb_ts = cd->ttfb_ts;
    item->in_bytes = cd->in_bytes;
    item->out_bytes = cd->out_bytes;
    item->resp_hdr_size = cd->resp_hdr_size;
    item->status = cd->status;
    item->subcode = cd->subcode;
    item->dst_port = cd->dst_port;
    memcpy(&(item->dst_ipv4v6), &(cd->dst_ipv4v6), sizeof(ip_addr_t));
    memcpy(&(item->src_ipv4v6), &(cd->src_ipv4v6), sizeof(ip_addr_t));
    item->flag = cd->flag;

    p = ps + sizeof(struct accesslog_item);
    for (i = 0; i < ali->num_al_list; i++) {
	pvalue = NULL;
	list_len = strlen(ali->al_list[i]);

	switch (*(unsigned int *)(&(ali->al_list[i][0]))) {
	case 0x6972752e:
	{
	    if (item->flag & ALF_METHOD_CONNECT) {
		/* It is CONNECT method */
		pvalue = NULL;
		pvalue_len = 0;
	    } else {
		/* It is NOT CONNECT method */
	    	HTTPGetKnownHeader(td, 0, H_X_NKN_URI,
				   &pvalue, &pvalue_len, &hdrcnt);
	    }
	    break;
	}
	case 0x6c71722e:    /* ".rql" */
	{
	    HTTPGetKnownHeader(td, 0, H_X_NKN_REQ_LINE,
	    		       &pvalue, &pvalue_len, &hdrcnt);
	    break;
	}
	case 0x746f682e:	/* ".hot" */
	{
	    // Not supported
	    break;
	}
	//provide namespace info, only name at this point
	case 0x70736e2e:	/* ".nsp" */
	{
	    // Not supported
	    break;
	}
	//find MFC name or servername
	case 0x6d6e732e:	/* ".snm" */
	{
	    char server[256];
	    int ret = gethostname(server, sizeof(server));
	    if(ret != -1){
		pvalue_len = strlen(server);
		pvalue = server;
	    }
	    break;
	}
	//find filename
	case 0x6d6e662e:	/* ".fnm" */
	{
	    pvalue = NULL;
	    pvalue_len = 0;
	    if (!(item->flag & ALF_METHOD_CONNECT)) {
		const char *temp;
		int temp_len, j;
		char c;
		HTTPGetKnownHeader(td, H_X_NKN_URI, 0,
				   &temp, &temp_len, &hdrcnt);
		if (temp_len > 1) {
		    for(j = temp_len - 1; j >= 0; j--) {
			c = temp[j];
			if(c == '/')
			    break;
		    }
		    pvalue_len = temp_len - (j+1); //Don't print /
		    pvalue = temp + j + 1 ;
		}
	    }
	    break;
	}
	/* Record provide name */
	case 0x7469682e:	/* ".hit" */
	{
	    // Not supported
	    break;
	}
	case 0x67726f2e:	/* ".org" */
	{
	    // Not supported
	    break;
	}
	case 0x7468632e:	/* ".cht" */
	{
	    // Not supported
	    break;
	}
	/* Search HTTP request Cookie header. */
	case 0x6b6f632e:	/* ".cok" */
	{
	    pname = &ali->al_list[i][4];
	    /* it is cookie string, use .cok for performance reason */
	    HTTPGetKnownHeader(td, H_COOKIE, 0, &pvalue, &pvalue_len, &hdrcnt);

	    for (nth = 0; nth < hdrcnt; nth++) {
	    	rv = HTTPGetNthKnownHeader(td, 0, H_COOKIE, nth,
					   &pvalue, &pvalue_len);
		if (!rv && pvalue) {
		    pvalue = get_parseCookie(pname, pvalue, pvalue_len);
		    if(pvalue) {
			pvalue_len = strlen(pvalue);
			goto logit;
		    }
		}
	    }
	    break;
	}
        /* Include Origin Server names details */
       	case 0x6e736f2e:    /* ".osn" */
	{
	    // Not supported
	    break;
	}
	case 0x7165722e:	/* ".req" */
	{
	    pname = &ali->al_list[i][4];
	    list_len -= 4;
	    token = -1;
	    token = HTTPHdrStrToHdrToken(pname, list_len);

	    if ((int)token >= 0) {
	    	if (!HTTPGetKnownHeader(td, token, 0, &pvalue, 
					&pvalue_len, &hdrcnt)) {
		    goto logit;
		}
		break;
	    }
	    if (!HTTPGetUnknownHeader(td, 0, pname, list_len,  
				      &pvalue, &pvalue_len)) {
		goto logit;
	    }
	    break;
	}
	case 0x7365722e:	/* ".res" */
	    pname = &ali->al_list[i][4];
	    list_len -= 4;

	    token = -1;
	    token = HTTPHdrStrToHdrToken(pname, list_len);

	    if ((int)token >= 0) {
	    	if (!HTTPGetKnownHeader(td, token, 1, &pvalue, 
				    	&pvalue_len, &hdrcnt)) {
		    goto logit;
	    	}
		break;
	    }

	    if (!HTTPGetUnknownHeader(td, 1, pname, list_len, 
				      &pvalue, &pvalue_len)) {
	    	goto logit;
	    }
	    break;
	}

logit:
	/*
	 * If valid pvalue, copy it, else return "-"
	 */
	if (pvalue) {
	    if(buf_len < pvalue_len+1) {
		glob_alog_err_buf_overflow++;
		goto skip_logit;
	    }
	    buf_len -= pvalue_len+1;
	    memcpy(p, pvalue, pvalue_len);
	    p[pvalue_len] = '\0';
	    p += pvalue_len + 1;
	} else {
	    if(buf_len < 2) {
		glob_alog_err_buf_overflow++;
		goto skip_logit;
	    }
	    buf_len -= 2;
	    *p++ = '-';
	    *p++ = 0;
	}
    }

    pdata->len = p - ps;
    ali->al_buf_end += (pdata->len + sizeof(NLP_data));

skip_logit:

    ALI_WR_UNLOCK(ali);
    return 0;
}

alog_token_t alog_init(const char *alog_name)
{
    if (thr_is_running) {
    	return (alog_token_t)&alog_info_data;
    }

    memset(&alog_info_data, 0, sizeof(alog_info_data));
    alog_info_data.name = strdup(alog_name);
    pthread_rwlock_init(&alog_info_data.lock, NULL);
    alog_info_data.fd = -1;
    alog_info_data.in_use = 1;
    alog_info_data.al_buf = alog_data_buf;

    if (!pthread_create(&thr_pid, NULL, alog_thread_func, 
    		        (void *)&alog_info_data)) {
	thr_is_running = 1;
    	return (alog_token_t)&alog_info_data;
    } else {
    	return (alog_token_t)0;
    }
}

/*
 * End of libnknalog.c
 */
