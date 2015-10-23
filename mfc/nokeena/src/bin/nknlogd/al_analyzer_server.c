#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <time.h>
#include <sys/timeb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <stdint.h>
#include <assert.h>
#include "nknlogd.h"

#include "al_analyzer.h"





static int al_analyzer_epollin(struct log_network_mgr *data);
static int al_analyzer_epollout(struct log_network_mgr *data);
static int al_analyzer_epollhup(struct log_network_mgr *data);
static int al_analyzer_epollerr(struct log_network_mgr *data);
static int al_analyzer_timeout(struct log_network_mgr *data,
			       double timeout);



static int scan_query_params(char *p, char *ns, char *domain, char *uri,
			     char *type, char *count)
{
    char *c;
    if (!p || !*p)
	return 0;

    return 0;
}

static int parse_request_uri(char *uri, data_request_t * p)
{
    char type[64], query[1024];
    char *s1, *s2, *t1, *t2;
    char *sv1, *sv2;
    char *tkn, *val;
    int i = 0;
    char *q = uri;


    i = 0;
    while (*q != '?') {
	if(*q==0) return 0;
	type[i++] = *q++;
    }
    q++; // skip '?'
    type[i] = 0;

    if (strstr(type, "/domain")) {
	p->type = dt_domain;
    } else if (strstr(type, "/uri")) {
	p->type = dt_uri;
    } else if (strstr(type, "/filesize")) {
	p->type = dt_filesize;
    } else if (strstr(type, "/respcode")) {
	p->type = dt_respcode;
    }

    /*
     *
     */
    i = 0;
    while (*q != ' ') {
	if(*q==0) return 0;
	query[i++] = *q++;
    }
    query[i] = 0;

    for (s1 = query; ; s1 = NULL ) {
	tkn = NULL;
	val = NULL;
	t1 = strtok_r(s1, "&", &sv1);
	if (!t1)
	    break;
	for (s2 = t1;; s2 = NULL) {
	    t2 = strtok_r(s2, "=", &sv2);
	    if (!t2)
		break;
	    if (!tkn) {
		tkn = t2;
	    } else if (!val) {
		val = t2;
	    }
	}

	if (tkn && strncmp(tkn, "domain", 6) == 0) {
	    p->domain = val;
	}
	else if (tkn && strncmp(tkn, "thour", 5) == 0) {
	    sscanf(val, "%d", &p->thour);
	}
	else if (tkn && strncmp(tkn, "tmin", 4) == 0) {
	    sscanf(val, "%d", &p->tmin);
	}
	else if (tkn && strncmp(tkn, "depth", 5) == 0) {
	    sscanf(val, "%d", &p->depth);
	}
    }


    return 0;
}



static int al_analyzer_epollin(struct log_network_mgr *pnm)
{
    int clifd;
    int cnt, len;
    struct sockaddr_un addr;
    char buf[1024];
    int t, count;
    data_request_t dr;
    char * p;
    char *rbuf;
    int ret;
    
    
    len = recv(pnm->fd, buf, 1024, 0);
    if (len <= 0) {
	unregister_free_close_sock(pnm);
	return FALSE;
    }
    buf[len] = 0;

    // Spec for query language:
    // /domain      - get top domain list
    // /uri         - get top uri list for specific domain
    // /filesize    - file size distribution
    // /respcode    - response code distribution
    // query params:
    //      thour   -       time (hour)
    //      tmin    -       time (minutes)
    //      depth   -       max number of entries to return
    //      domain  -       domain name
    //	    
    // e.g.
    //      /filesize?thour=0&tmin=20
    //      /respcode?thour=2&tmin=20
    //      /domain?thour=1&tmin=20&depth=20
    //      /uri?thour=1&tmin=20&depth=20&domain=www.google.com
    //
    //

    p = buf;
    p += 4;

    memset(&dr, 0, sizeof(data_request_t));
    if (parse_request_uri(p, &dr) != 0) {
	// error case
	unregister_free_close_sock(pnm);
	return FALSE;
    } 

    len = alan_process_data_request(&dr);

    // Get data into buffer and send out
    // We own the buffer so free it here. 
    //
    rbuf = (char *)malloc(128 + dr.resp_len);
    ret = sprintf(rbuf, "HTTP/1.0 200 OK\r\n"
                         "Content-Length: %d\r\n"
                         "\r\n",
                         dr.resp_len);

    p = &rbuf[ret];
    if(dr.resp_len) {
	strcat(p, dr.resp);
    }
    free(dr.resp);

    len = send(pnm->fd, rbuf, ret + dr.resp_len, 0);
    if (len <= 0) {
	unregister_free_close_sock(pnm);
	free(rbuf);
	return FALSE;
    }

    unregister_free_close_sock(pnm); // Close socket
    free(rbuf);
    return TRUE;
}


static int al_analyzer_epollout(struct log_network_mgr *data)
{
    return FALSE;

}


static int al_analyzer_epollhup(struct log_network_mgr *data)
{
    UNUSED_ARGUMENT(data);
    assert(0);
    return FALSE;
}


static int al_analyzer_epollerr(struct log_network_mgr *data)
{
    UNUSED_ARGUMENT(data);
    assert(0);
    return FALSE;
}


static int al_analyzer_timeout(struct log_network_mgr *data,
			       double timeout)
{
    return FALSE;
}












///////////////////////////////////////////////////////////////////////////////
//                      ANALYZER SERVER-SIDE APIS
///////////////////////////////////////////////////////////////////////////////
static int al_analyzer_svr_epollin(struct log_network_mgr *pnm)
{
    int clifd;
    struct sockaddr_un addr;
    unsigned int addr_len = 0;

    clifd = accept(pnm->fd, (struct sockaddr *) &addr, &addr_len);
    if (clifd < 0) {
	return TRUE;
    }
    // valid socket
    if (log_set_socket_nonblock(clifd) == FALSE) {
	close(clifd);
	return TRUE;
    }

    struct log_network_mgr *current_nm = NULL;

    if (register_log_socket(clifd,
			    NULL,
			    al_analyzer_epollin,
			    al_analyzer_epollout,
			    al_analyzer_epollerr,
			    al_analyzer_epollhup,
			    al_analyzer_timeout, &current_nm) == FALSE) {
	close(clifd);
	return TRUE;
    }

    log_add_event_epollin(current_nm);

    return TRUE;
}

static int al_analyzer_svr_epollout(struct log_network_mgr *pnm)
{
    UNUSED_ARGUMENT(pnm);
    assert(0);			// should never called
    return FALSE;
}

static int al_analyzer_svr_epollerr(struct log_network_mgr *pnm)
{
    UNUSED_ARGUMENT(pnm);
    assert(0);			// should never called
    return FALSE;
}

static int al_analyzer_svr_epollhup(struct log_network_mgr *pnm)
{
    UNUSED_ARGUMENT(pnm);
    assert(0);			// should never called
    return FALSE;
}

int al_analyzer_svr_init(log_network_mgr_t ** listener_pnm)
{
    int listenfd = 0;
    int n = 0;
    struct sockaddr_un addr;
    unsigned int addr_len;
    int err = 0;
    struct stat stats;
    char uds_path[256];

    if ((listenfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
	return -1;
    }
#define DEFAULT_ANALYZER_UNIX_ROOT "/vtmp"
#define DEFAULT_ANALYZER_UNIX_PATH "/vtmp/nknlogd-socks"
#define MY_MASK 	01777
    /* test if directory exists. if not create it */
    err = lstat(DEFAULT_ANALYZER_UNIX_PATH, &stats);
    if (err && errno == ENOENT) {
	/* Directory doesnt exist. Create it first */
	//err = mkdir("/vtmp", S_IRWXG);
	if ((err = mkdir(DEFAULT_ANALYZER_UNIX_ROOT, MY_MASK)) != 0){
		fprintf(stderr, "ERROR %d: unable to mkdir %s; %s\n", errno, DEFAULT_ANALYZER_UNIX_ROOT, strerror(errno));
	}
	if ((err = mkdir(DEFAULT_ANALYZER_UNIX_PATH, MY_MASK)) != 0){
		fprintf(stderr, "ERROR %d: unable to mkdir %s; %s\n", errno, DEFAULT_ANALYZER_UNIX_PATH, strerror(errno));
	}
    } else {
	/* directory exists, but has wrong modes set for it !! */
	if (lstat(DEFAULT_ANALYZER_UNIX_PATH, &stats) == 0) {
	    if (!S_ISDIR(stats.st_mode) && !S_ISREG(stats.st_mode)) {
		close(listenfd);
		return -2;
	    }

	} else {
	    close(listenfd);
	    return -2;
	}
    }

    err = chmod(DEFAULT_ANALYZER_UNIX_PATH, MY_MASK);

    snprintf(uds_path, 256, "%s/uds-analyzer-server",
	     DEFAULT_ANALYZER_UNIX_PATH);

    unlink(uds_path);
    addr.sun_family = AF_UNIX;
    n = sprintf(addr.sun_path, "%s", uds_path);
    addr_len = sizeof(addr.sun_family) + n;

    if (bind(listenfd, (struct sockaddr *) &addr, addr_len) != 0) {
	fprintf(stderr, "ERROR %d: unable to bind to %s; %s\n", errno, uds_path, strerror(errno));
	close(listenfd);
	return -3;
    }

    if (listen(listenfd, 5) != 0) {
	close(listenfd);
	return -4;
    }

    err = chmod(uds_path, 00777);
    struct log_network_mgr *current_nm = NULL;

    if (register_log_socket(listenfd,
			    NULL,
			    al_analyzer_svr_epollin,
			    al_analyzer_svr_epollout,
			    al_analyzer_svr_epollerr,
			    al_analyzer_svr_epollhup,
			    NULL, &current_nm) == FALSE) {
	close(listenfd);
	free(current_nm);
	return -5;
    }

    *listener_pnm = current_nm;
    log_add_event_epollin(current_nm);

    return 0;
  error:
    close(listenfd);
    return -6;
}
