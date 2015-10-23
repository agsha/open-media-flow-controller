
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "network.h"

extern int ingester_log_level;


int del_uds_sku(struct uds *uds);
extern int setnonblock(int fd);
extern int connect_out(struct in_addr dest, char *request,
		       char *nsname, char *host, char *uri,
		       char *path, int64_t size);

extern int32_t epfd;



struct header {

    int		flags;
    char	type;
    char	_pad;
    short	len;
};

struct msg_data {

    int		nsname_len;
    int		host_len;
    int		path_len;
    int		uri_len;
    int		size_len;
    char	data[1];
};

struct message {

    struct header header;
    char	data[1];
};



int uds_epollin(struct socku *sku);
int uds_epollout(struct socku *sku);
int uds_epollerr(struct socku *sku);
int uds_epollhup(struct socku *sku);

int uds_epollin_svr(struct socku *sku);
int uds_epollout_svr(struct socku *sku);
int uds_epollerr_svr(struct socku *sku);
int uds_epollhup_svr(struct socku *sku);
int add_uds_sku(struct uds *uds);

int create_control_uds(void);

int uds_recv(int fd, struct msg_data **data);
struct uds *uds_alloc(int fd);

int uds_recv(int fd, struct msg_data **data)
{
    struct message *msg;
    struct header header;
    int n, l;

    n = recv(fd, &header, sizeof(unsigned long), 0);
    if (n <= 0) {
	//fd closed.
	return -1;
    }

    msg = malloc(sizeof(struct message) + header.len - 1);
    msg->header = header;

    l = 0;
    while (l != header.len) {

	l = recv(fd, &(msg->data[l]), header.len - l, 0);
	if (l <= 0) {
	    // fd closed
	    free(msg);
	    return -2;
	}
	n += l;
    }

    *data = calloc(1, header.len);

    memcpy((*data), msg->data, header.len);
    free(msg);


    return n;

}

int uds_epollin(struct socku *sku)
{
    struct msg_data	*data = NULL;
    const char *p;
    int n;
    int ret = 0;
    struct uds *uds = uds_sku(sku);
    char nsname[256], host[256], *uri = NULL, *path = NULL;
    char request[2048];
    char size[32], *end_ptr = NULL;
    int64_t nsize;

    // new message in
    //
    uds->rxlen = uds_recv(uds->sku_fd, &data);
    if (uds->rxlen <= 0) {
	ret = 1;
	goto bail;
    }

    // We have 1 complete message. Process this ingest request
    //
    send(uds->sku_fd, "1", 1, 0);

    p = &(data->data[0]);
    strncpy(&nsname[0], p, (size_t) data->nsname_len);
    nsname[data->nsname_len] = 0;
    p += data->nsname_len;

    strncpy(host, p, data->host_len);
    host[data->host_len] = 0;
    p += data->host_len;

    uri = calloc(1, data->uri_len + 1);
    strncpy(uri, p, data->uri_len);
    uri[data->uri_len] = 0;
    p += data->uri_len;

    strncpy(size, p, data->size_len);
    size[data->size_len] = 0;
    p += data->size_len;
    errno=0;
    nsize = strtol(size, &end_ptr, 10);

    path = calloc(1, data->path_len + 1);
    strncpy(path, p, data->path_len);
    path[data->path_len] = 0;
    p += data->path_len;

    // build request
    n = snprintf(request, sizeof(request),
	    "GET %s HTTP/1.1\r\n"
	    "Host: %s\r\n"
	    "Cache-Control: max-age=0\r\n"
	    "User-Agent: curl/7.15.5 (x86_64-redhat-linux-gnu) libcurl/7.15.5 OpenSSL/0.9.8b zlib/1.2.3 libidn/0.6.5\r\n"
	    "Accept: */*\r\n"
	    "Connection: close\r\n"
            "X-NKN-Internal: client=ftp\r\n"
	    "\r\n",
	    uri,
	    host);



    struct in_addr addr = {
	.s_addr = htonl(0x7f000001)
    };
    ret = connect_out(addr, request, nsname, host, uri, path, nsize);

bail:
    uri ? free(uri) : 0;
    path ? free(path) : 0;
    data ? free(data) : 0;
    /* freeup the uds */
    ret = del_uds_sku(uds);

    return ret;
}

int uds_epollout(struct socku *sku)
{
    (void) sku;
    return 0;
}

int uds_epollerr(struct socku *sku)
{
    (void) sku;
    return 0;
}

int uds_epollhup(struct socku *sku)
{
    (void) sku;
    return 0;
}


int create_control_uds(void)
{
    int ret;
    struct stat stats;
    int uds_fd;
    struct sockaddr_un addr;
    unsigned int addr_len;
    char uds_path[256];
    int err, n;
    struct uds *uds;

#define INGESTER_UDS_ROOT "/vtmp"
#define INGESTER_UDS_PATH "/vtmp/ingester"
#define MY_MASK         01777

    log_to_syslog(ingester_log_level, "Creating Control UDS\n");
    uds_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (uds_fd < 0) {
	return -1;
    }

    ret = lstat(INGESTER_UDS_PATH, &stats);
    if (ret && errno == ENOENT) {
	log_to_syslog(ingester_log_level, "Creating %s\n", INGESTER_UDS_PATH);
	// doesnt exist. create one
	if ((ret = mkdir(INGESTER_UDS_ROOT, MY_MASK)) != 0) {
	    perror("mkdir");
	}
	if ((ret = mkdir(INGESTER_UDS_PATH, MY_MASK)) != 0) {
	    perror("mkdir");
	}
    } else {
	if (lstat(INGESTER_UDS_PATH, &stats) == 0) {
	    if (!S_ISDIR(stats.st_mode) && !S_ISREG(stats.st_mode)) {
		log_to_syslog(ingester_log_level,"%s: ret = -4\n", __FUNCTION__);
		ret = -4;
		goto bail;
	    }
	} else {
	    log_to_syslog(ingester_log_level, "%s: ret = -5\n", __FUNCTION__);
	    ret = -5;
	    goto bail;
	}
    }

    err = chmod(INGESTER_UDS_PATH, MY_MASK);
    snprintf(uds_path, sizeof(uds_path), "%s/uds-ingester-server", INGESTER_UDS_PATH);
    unlink(uds_path);
    addr.sun_family = AF_UNIX;
    n = sprintf(addr.sun_path, "%s", uds_path);
    addr_len = sizeof(addr.sun_family) + n;

    if (bind(uds_fd, (struct sockaddr *) &addr, addr_len) != 0) {
	ret = -6;
	goto bail;
    }

    if (listen(uds_fd, 256) != 0) {
	ret = -7;
	goto bail;
    }

    err = chmod(uds_path, 00777);

    uds = uds_alloc(uds_fd);
    if (uds == NULL) {
	ret = -8;
	goto bail;
    }

    uds->epollin = uds_epollin_svr;
    uds->epollout = uds_epollout_svr;
    uds->epollerr = uds_epollerr_svr;
    uds->epollhup = uds_epollhup_svr;
    uds->event.events = EPOLLIN;
    uds->event.data.ptr = uds;

    usleep(10000);

    setnonblock(uds->sku_fd);

    if ((ret = epoll_ctl(epfd, EPOLL_CTL_ADD, uds->sku_fd, &uds->event)) < 0) {
	free(uds);
	goto bail;
    }

    return 0;

bail:
    close(uds_fd);
    return ret;
}


int del_uds_sku(struct uds *uds)
{
    int ret = 1;

    if ((uds == NULL) || (uds->sku_fd == -1) ) {
	ret = -1;
	goto bail;
    }

    if ((ret = epoll_ctl(epfd, EPOLL_CTL_DEL, uds->sku_fd, NULL)) < 0) {
	log_to_syslog(ingester_log_level,"EPOLL_CTL_DEL failed: %d", ret);
    }

    close(uds->sku_fd);
    uds->sku_fd = -1;

    uds->epollin = 0;
    uds->epollout = 0;
    uds->epollerr = 0;
    uds->epollhup = 0;

    uds_put((struct socku *) uds);
    if (uds) free(uds);

bail:

    return 1;

}

int add_uds_sku(struct uds *uds)
{

    int ret = 0;

    uds_get((struct socku *) uds);

    uds->epollin = uds_epollin;
    uds->epollout = uds_epollout;
    uds->epollerr = uds_epollerr;
    uds->epollhup = uds_epollhup;

    uds->event.events = EPOLLIN;
    uds->event.data.ptr = uds;

    setnonblock(uds->sku_fd);

    if ((ret = epoll_ctl(epfd, EPOLL_CTL_ADD, uds->sku_fd, &uds->event)) < 0) {
	goto bail;
    }

bail:
    uds_put((struct socku *) uds);

    return ret;
}

struct uds *uds_alloc(int fd)
{
    struct uds *uds = 0;

    uds = calloc(1, sizeof(struct uds));
    if (uds == NULL) {
	return NULL;
    }
    uds->sku_fd = fd;

    return (uds);
}


int uds_epollin_svr(struct socku *sku)
{
    // new message in
    struct sockaddr_un	addr;
    unsigned int addr_len = 0;
    int clifd, ret = 0;
    struct uds *uds;

    clifd = accept(sku->_fd, (struct sockaddr *)(&addr), &addr_len);
    if (clifd < 0)
	return 0;

    uds = uds_alloc(clifd);

    if (uds == NULL) {
	return 0;
    }
    ret = add_uds_sku(uds);
    if (ret) {
	free(uds);
    }

    return ret;
}

int uds_epollout_svr(struct socku *sku)
{
    (void) sku;
    return 0;
}

int uds_epollerr_svr(struct socku *sku)
{
    (void) sku;
    return 0;
}

int uds_epollhup_svr(struct socku *sku)
{
    (void) sku;
    return 0;
}

