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
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <stdint.h>
#include <alloca.h>
#include <sys/queue.h>
#include "log_accesslog_pri.h"
#include "nknlogd.h"
#include "log_channel.h"

#include "common.c"
#include "log_accesslog.h"


#define SERVER_STRING	    "Server (%s) @ %p"
#define CLIENT_STRING	    "Client (%s) @ %p"


extern void accesslog_file_exit(struct channel *chan);

extern void accesslog_file_init(struct channel *chan);
static int log_max_fds = MAX_FDS;	// Maxixum allowed total fds in this thread
static int srv_shutdown = 0;

void unlink_svr_array(struct channel *c);

static int log_epfd;
static struct epoll_event *events;

void uds_server_log_init(void);

int channel_close(struct channel *c);

struct channel *channel_alloc(int clifd, char *name);
int log_create_accesslog_server(const char *name, void *plf);
int log_delete_accesslog_server(const char *name);

int create_loop_control(void);
int setnonblock(int fd);
static void __flush(void *key, void *value, void *user);
int uds_ctl_epollin(struct socku *sku);


static struct channel *svr_array[MAX_FDS] = { 0 };


int setnonblock(int fd)
{
    int opt = 0;

    opt = fcntl(fd, F_GETFL);
    if (opt < 0) {
	complain_error_errno(1, "fcntl failed");
	return -errno;
    }

    opt = (opt | O_NONBLOCK);
    if (fcntl(fd, F_SETFL, opt) < 0) {
	complain_error_errno(1, "fcntl failed");
	return -errno;
    }

    return 0;
}


/*
 * init functions
 */
void uds_server_log_init(void)
{
    log_epfd = epoll_create(MAX_FDS);
    if(log_epfd == -1 ){
	complain_error_errno(1, "epoll create failed!");
	exit(EXIT_FAILURE);
    }

    events = calloc(log_max_fds, sizeof(struct epoll_event));
    if(events == NULL ){
	complain_error_msg(1, "Memory allocation failed!");
	exit(EXIT_FAILURE);
    }

}

int create_loop_control(void)
{
    const char *u = "/vtmp/nknlogd/accesslog/uds-control";
    int fd;
    int ret = 0;
    int l;
    size_t addr_len;
    struct sockaddr_un addr;
    struct channel *c = NULL;
    int i;
    int err;

    fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) {
	complain_error_errno(1, "socket creation failed");
	return -errno;
    }

    unlink(u);
    addr.sun_family = AF_UNIX;
    l = sprintf(addr.sun_path, "%s", u);
    addr_len = sizeof(addr.sun_family) + l;

    if (bind(fd, (struct sockaddr *) &addr, addr_len) != 0) {
	complain_error_errno(1, "bind failed");
	ret = -errno;
	goto bail;
    }

    err = chmod(u, 00777);
    c = log_channel_new("control");
    if (!c) {
	ret = -ENOMEM;
	goto bail;
    }

    c->sku_fd = fd;
    c->epollin = uds_ctl_epollin;
    c->epollout = NULL;
    c->epollerr = NULL;
    c->epollhup = NULL;
    c->sku_event.events = EPOLLIN;
    c->sku_event.data.ptr = c;
    snprintf(c->info, 64, SERVER_STRING, "control", c);
    pthread_mutex_init(&c->sku_lock, NULL);
    memcpy(&(c->addr.uds.udsaddr), &addr, addr_len);

    usleep(10000);
    setnonblock(c->sku_fd);

    if ((ret =
	 epoll_ctl(log_epfd, EPOLL_CTL_ADD, c->sku_fd,
		   &c->sku_event)) < 0) {
	complain_error_errno(1, "epoll_ctl failed");
	ret = -errno;
	goto bail;
    }

    c->state |= cs_INEPOLL;

    for (i = 0; i < 128; i++) {
	if (svr_array[i] == NULL) {
	    svr_array[i] = c;
	    break;
	}
    }

    return ret;

  bail:
    unlink(u);
    safe_close(fd);
    return ret;
}


static void __flush(void *key, void *value, void *user)
{
    char *k = (char *) key;
    struct channel *ch = (struct channel *) value;
    time_t now;
    logd_file * plf = NULL;
    int reinit = 0;
    int ret = 0;
    /* Force a file flush for all registered profiles */
    if ((ch) && (ch->accesslog) && (ch->accesslog->fp)) {
	ret = fflush(ch->accesslog->fp);
	plf = (ch->accesslog);
	log_write_report_err(ret , plf);
	now = time(NULL);
    }

    if ((plf != NULL)&&(plf->fp) && (plf->rt_interval)){
	reinit = align_to_rotation_interval(plf, now);
	if(reinit){
	    accesslog_file_exit(ch);
	    accesslog_file_init(ch);
	}
    }
}

int uds_ctl_epollin(struct socku *sku)
{
    struct channel *c = (struct channel *) sku;
    char buf[32];
    int ret;

    /* This channel is triggerred from the timer. At every
     * n-th second, we induce a flush for all open profiles.
     */
    ret = recv(c->sku_fd, buf, 1, 0);
    if(ret < 0)
    {
	complain_error_errno(1, "recv failed");
    }


    log_channel_for_each(__flush, NULL);
    return 0;
}

/* ***********************************************************************
 * log_main is an infinite loop. It never exits unless somehow hits Ctrl-C.
 * ***********************************************************************/
int nkn_launch_logd_server(void);
int nkn_launch_logd_server(void)
{
    int i;
    int ret;
    int res;
    struct channel *c = NULL;


    create_loop_control();

    // An infinite loop
    while (1) {
	if (srv_shutdown == 1)
	    break;		// we are shutting down

	res = epoll_wait(log_epfd, events, log_max_fds, 10);

	if (res == -1) {
	    complain_error_errno(1, "epoll_wait failed");
	    // Can happen due to -pg
	    if (errno == EINTR)
		continue;
	    goto bail;
	    //exit(10);
	} else if (res == 0) {
	    // time out case
	    //
	    // Check if there is any element in the freelist. If so, get
	    // and free the element
	    //empty_flist();
	    continue;
	}

	for (i = 0; i < res; i++) {

	    struct socku *sku = NULL;
	    sku = (struct socku *) events[i].data.ptr;
	    ret = 0;
	    if (sku == NULL) continue;

	    if (events[i].events & EPOLLIN) {
		ret = (sku->_epollin) ? sku->_epollin(sku) : 0;
	    }
	    if ((ret == 0) && events[i].events & EPOLLOUT) {
		ret = (sku->_epollout) ? sku->_epollout(sku) : 0;
	    }

	    if ((ret == 0) && events[i].events & EPOLLERR) {
		ret = (sku->_epollerr) ? sku->_epollerr(sku) : 0;
	    }

	    if ((ret == 0) && events[i].events & EPOLLHUP) {
		ret = (sku->_epollhup) ? sku->_epollhup(sku) : 0;
	    }
	    if (!ret) {
		sku_put(sku);
	    }
	}
    }

  bail:
    printf("nkn_launch_logd_server: unexpected exited\n");
    return 0;
}


int log_create_accesslog_server(const char *name, void *plf)
{
#define MY_MASK         01777

    int ret;
    int i;
    struct stat stats;
    int uds_fd;
    struct sockaddr_un addr;
    unsigned int addr_len;
    char *uds_path = alloca(strlen(LOG_UDS_PATH) + 8 + strlen(name));
    int err, n;
    struct channel *c = NULL;
    char svr_name[128];
    char profile[32];

    uds_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (uds_fd < 0) {
	complain_error_errno(1, "socket creation failed");
	return -1;
    }

    ret = lstat(LOG_UDS_PATH, &stats);
    if(ret < 0)
    {
	complain_error_errno(1, "lstat failed");
    }
    if (ret && errno == ENOENT) {
	// doesnt exist. create one
	if ((ret = mkdir(LOG_UDS_ROOT, MY_MASK)) != 0) {
	    perror("mkdir");
	}
	if ((ret = mkdir(LOG_UDS_PATH, MY_MASK)) != 0) {
	    perror("mkdir");
	}
    } else {
	if (lstat(LOG_UDS_PATH, &stats) == 0) {
	    if (!S_ISDIR(stats.st_mode) && !S_ISREG(stats.st_mode)) {
		ret = -4;
		goto bail;
	    }
	} else {
	    ret = -5;
	    goto bail;
	}
    }

    err = chmod(LOG_UDS_PATH, MY_MASK);
    sprintf(uds_path, "%s/%s", LOG_UDS_PATH, name);
    unlink(uds_path);
    addr.sun_family = AF_UNIX;
    n = sprintf(addr.sun_path, "%s", uds_path);
    addr_len = sizeof(addr.sun_family) + n;

    if (bind(uds_fd, (struct sockaddr *) &addr, addr_len) != 0) {
	complain_error_errno(1, "bind failed");
	ret = -6;
	goto bail;
    }

    if (listen(uds_fd, 256) != 0) {
	complain_error_errno(1, "listen failed");
	ret = -7;
	goto bail;
    }

    err = chmod(uds_path, 00777);

    snprintf(svr_name, 128, "server-%s", name);
    sscanf(name, "uds-acclog-%31s", profile);
    log_channel_get(profile, &c);
    if (!c) {
	ret = -8;
	goto bail;
    }

    c->sku_fd = uds_fd;

    /* New connection on UDS. use the handshake handlers */
    c->epollin = uds_epollin_svr;
    c->epollout = uds_epollout_svr;
    c->epollerr = uds_epollerr_svr;
    c->epollhup = uds_epollhup_svr;
    c->sku_event.events = EPOLLIN;
    c->sku_event.data.ptr = c;
    c->parent = NULL;
    c->child = NULL;
    c->accesslog = plf;
    pthread_mutex_init(&c->sku_lock, NULL);

    memcpy(&(c->addr.uds.udsaddr), &addr, addr_len);

    usleep(10000);

    setnonblock(c->sku_fd);

    if ((ret =
	 epoll_ctl(log_epfd, EPOLL_CTL_ADD, c->sku_fd,
		   &c->sku_event)) < 0) {
	complain_error_errno(1, "epoll_ctl failed");
	goto bail;
    }

    for (i = 0; i < 128; i++) {
	if (svr_array[i] == NULL) {
	    svr_array[i] = c;
	    break;
	}
    }

    return 0;

  bail:
    safe_close(uds_fd);
    return ret;
}

void unlink_svr_array(struct channel *c)
{
    int i;
    for (i = 0; i < 128; i++) {
	if (svr_array[i] == c) {
	    svr_array[i] = NULL;
	    return;
	}
    }
}

int log_delete_accesslog_server(const char *name)
{
    int i;
    char *uds_path = alloca(strlen(LOG_UDS_PATH) + 8 + strlen(name));
    struct channel *c = NULL;
    int len = 0;

    bzero(uds_path, strlen(LOG_UDS_PATH) + 8 + strlen(name));
    len = sprintf(uds_path, "%s/%s", LOG_UDS_PATH, name);

    for (i = 0; i < 128; i++) {
	if (svr_array[i] && (safe_strcmp(svr_array[i]->addr.uds.udsaddr.sun_path, uds_path) == 0)) {
	    c = svr_array[i];
	    svr_array[i] = NULL;
	    break;
	}
    }
    if (c) {
	channel_close(c);
	log_channel_free(c);
	unlink(uds_path);
    }
    return 0;
}


int channel_close(struct channel *c)
{
    if (!c)
	return -1;

    pthread_mutex_lock(&c->sku_lock);
    safe_close(c->sku_fd);
    c->epollin = NULL;
    c->epollout = NULL;
    c->epollhup = NULL;
    c->epollerr = NULL;
    c->state = 0;
    pthread_mutex_unlock(&c->sku_lock);
    return 0;
}

struct channel *channel_alloc(int clifd, char *name)
{
    int ret = 0;
    struct channel *c = NULL;

    char client_name[256];



    c = log_channel_new(name);
    if (!c)
	return NULL;

    c->sku_fd = clifd;
    c->epollin = al_handshake_epollin;
    c->epollout = al_handshake_epollout;
    c->epollhup = al_handshake_epollhup;
    c->epollerr = al_handshake_epollerr;
    pthread_mutex_init(&c->sku_lock, NULL);
    c->sku_event.events = EPOLLIN;
    c->sku_event.data.ptr = c;
    c->state |= cs_HELO;

    snprintf(c->info, 64, CLIENT_STRING, name, c);
    setnonblock(c->sku_fd);

    if ((ret =
	 epoll_ctl(log_epfd, EPOLL_CTL_ADD, c->sku_fd,
		   &c->sku_event)) < 0) {
	complain_error_errno(1, "epoll_ctl failed");
	safe_close(clifd);
	return NULL;
    }
    c->state |= cs_INEPOLL;

    return c;
}



int uds_epollin_svr(struct socku *sku)
{
    // new message in
    struct sockaddr_un addr;
    unsigned int addr_len = 0;
    int clifd, ret = 0;
    struct channel *chan;
    struct channel *c = NULL;
    char cl_name[64];

    clifd = accept(sku->_fd, (struct sockaddr *) (&addr), &addr_len);
    if (clifd < 0){
	complain_error_errno(1, "accept failed");
	return 0;
    }

    c = (struct channel *) sku;

    /* Connect request. Create a channel */
    chan = channel_alloc(clifd, c->name);
    if (!chan) {
	lc_log_basic(LOG_ERR, "failed to create a new channel.");
    }

    chan->parent = c;
    chan->accesslog = c->accesslog;
    c->child = chan;


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


/*----------------------------------------------------------------------------
 * Handshake handlers. 
 * These are called once-and-only-once when a client connects to us.
 * We expect to recieve a HELO message, after which we will send the
 * client a format string. 
 */
int al_handshake_epollin(struct socku *sku)
{
    struct channel *chan = (struct channel *) sku;
    char buf[1024];
    uint32_t len;
    NLP_client_hello *pnlpch;

    len = recv(chan->sku_fd, buf, 1024, 0);
    if (len <= 0 || (len != sizeof(NLP_client_hello))) {
	complain_error_errno(1, "recv  failed");
	return -errno;
    }

    if (!(chan->state & cs_HELO)) {
	return -2;
    }

    pnlpch = (NLP_client_hello *) buf;
    if (strcmp(pnlpch->sig, SIG_SEND_ACCESSLOG) != 0) {
	return -1;
    } else {
	/* Negotiate format string to client */
	al_handle_new_send_socket(chan);
    }

    return 0;
}

int al_handshake_epollout(struct socku *sku)
{
    return 0;
}

int al_handshake_epollerr(struct socku *sku)
{
    return 0;
}
int al_handshake_epollhup(struct socku *sku)
{
    return 0;
}
