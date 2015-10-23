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
#include "log_accesslog_pri.h"
#include "nknlogd.h"
#include "log_channel.h"

#include "common.c"
#include "log_accesslog.h"

struct sockaddr_in dip;
socklen_t dlen = sizeof(dip);
struct sockaddr_in srv;
static void log_set_socket_active(log_network_mgr_t * pnm);
extern void el_handle_new_send_socket(log_network_mgr_t * pnm);
extern void sl_handle_new_send_socket(log_network_mgr_t * pnm);

void logsvr_init(log_network_mgr_t ** listener_pnm);

extern pthread_mutex_t nm_free_queue_lock;
extern int g_log_network_thread_wait;

static int log_max_fds = MAX_FDS;	// Maxixum allowed total fds in this thread
extern int log_port_listen;


static int srv_shutdown = 0;
static int elog_epfd;
static struct epoll_event *elog_events;

/*
 * init functions
 */
void log_init(void);
void
log_init(void)
{
    elog_events = calloc(MAX_FDS, sizeof(struct epoll_event));
    if(elog_events == NULL ){
	complain_error_msg(1, "Memory allocation failed!");
	exit(EXIT_FAILURE);
    }
    elog_epfd = epoll_create(MAX_FDS);
    if(elog_epfd == -1 ){
	complain_error_errno(1, "epoll create failed!");
	exit(EXIT_FAILURE);
    }
}

static int
flush_logsvr_epollin(log_network_mgr_t * pnm)
{
    char buf[32];
    int ret;
    logd_file * plf = NULL;
    time_t now1;
    int reinit = 0;
    unsigned int len = 0;
    struct sockaddr_in srv_addr;
    struct tm tm;

    /* changed from now to now1 to chk the drift in crawl log
     * Receiving the time_t now from socket from flush_otherlogs(now)
     * causes drift making the alignment of file improper
     * moving back to getting time and manipulating over here
     * caution:to look out whether nknlogd hits 100% cpu
     */
    now1 = time(NULL);
    len = sizeof(srv_addr);
    /* This channel is triggerred from the timer. At every
     * n-th second, we induce a flush for all open profiles.
     */
    ret = recvfrom(pnm->fd, buf, 1, 0, (struct sockaddr *)&srv_addr,&len);
    if (ret < 0)
    {
	complain_error_errno(1, "recvfrom failed");
    }

    localtime_r(&now1, &tm);
    for (plf = g_lf_head.lh_first; plf != NULL; plf = plf->entries.le_next) {
	if ((plf->fp != NULL) && (plf->fp) && (plf->rt_interval)){
	    reinit = align_to_rotation_interval(plf, now1);
	    if(reinit){
		log_file_exit(plf);
		log_file_init(plf);
	    }
	}
    }

    return 0;

}
int create_flush_control(void);
int
create_flush_control(void)
{
    int ret;
    int val;
    int i;
    int listenfd;
    log_network_mgr_t *listener_pnm = NULL;
    log_network_mgr_t *current_nm = NULL;


    if ((listenfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	perror(" Socket creation failed:");
	return 1;
    }


    bzero(&srv, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_addr.s_addr = 0x0100007F;   //INADDR_ANY;
    srv.sin_port = 0; //bind in any free port

    val = 1;
    ret =
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    if (ret == -1) {
	perror("sectsockt opt failed:");
	return 1;
  }
    if (bind(listenfd, (struct sockaddr *) &srv, sizeof(srv)) < 0) {
	perror("bind failed");
	return 1;
    }

    memset(&dip, 0, dlen);
    if (getsockname(listenfd,(struct sockaddr *)&dip, &dlen) == -1) {
	return 1;
    }
    log_set_socket_nonblock(listenfd);

    if (register_log_socket(listenfd,
		NULL,
		flush_logsvr_epollin,
		NULL,
		NULL,
		NULL, NULL, &current_nm) == FALSE) {
	close(listenfd);
	free(current_nm);
	exit(1);
    }
    listener_pnm = current_nm;
    log_add_event_epollin(current_nm);

    return ret;
}

static inline int
validate_network_manager(log_network_mgr_t * incoming_pnm)
{
    log_network_mgr_t *pnm;
    pthread_mutex_lock(&nm_free_queue_lock);
    if (LIST_EMPTY(&g_nm_to_free_queue)) {
	pthread_mutex_unlock(&nm_free_queue_lock);
	return 1;
    } else {
	/*! loop to compare current incoming log manager to the list of manager */
	for (pnm = g_nm_to_free_queue.lh_first; pnm != NULL;
	     pnm = pnm->entries.le_next) {

	    if (pnm == incoming_pnm) {
		pthread_mutex_unlock(&nm_free_queue_lock);
		return 0;
	    }
	}
    }
    pthread_mutex_unlock(&nm_free_queue_lock);
    return 1;
}

static inline void
clean_up_free_nm_list(void)
{
    log_network_mgr_t *pnm;

    /*! memory freeing done for all the log managers available 
       in the free queue list */
    pthread_mutex_lock(&nm_free_queue_lock);
    while (!LIST_EMPTY(&g_nm_to_free_queue)) {
	pnm = g_nm_to_free_queue.lh_first;
	LIST_REMOVE(pnm, entries);
	free(pnm);
    }
    pthread_mutex_unlock(&nm_free_queue_lock);
}


void *nkn_launch_misc_logd_server(void *arg);
void
*nkn_launch_misc_logd_server(void *arg)
{
    int i;
    int res;
    int fd;
    log_network_mgr_t *pnm = NULL;
    log_network_mgr_t *listener_pnm = NULL;
    log_network_mgr_t *al_listener_pnm = NULL;



    log_init();
    logsvr_init(&listener_pnm);

    /*PR:Fix for 769939 create  a control/management socket connection
     *which receive flush every 2 sec
     */
    create_flush_control();
    if (glob_run_under_pm) {
	while (g_log_network_thread_wait)
	    sleep(2);
    }
    // An infinite loop
    while (1) {
	if (srv_shutdown == 1)
	    break;		// we are shutting down

	res = epoll_wait(elog_epfd, elog_events, log_max_fds, 30);

	if (res == -1) {
	    complain_error_errno(1, "epoll_wait");
	    // Can happen due to -pg
	    if (errno == EINTR)
		continue;
	    exit(10);
	} else if (res == 0) {
	    // time out case
	    continue;
	}


	for (i = 0; i < res; i++) {

	    pnm = (log_network_mgr_t *) elog_events[i].data.ptr;
	    pthread_mutex_lock(&pnm->network_manager_log_lock);
	    /*! if the pnm is in free queue list then skip. it is ready to free !! */
	    if (validate_network_manager(pnm) == 0) {
		pthread_mutex_unlock(&pnm->network_manager_log_lock);
		continue;
	    }
	    log_set_socket_active(pnm);
	    if (elog_events[i].events & EPOLLIN) {
		pnm->f_epollin(pnm);
	    } else if (elog_events[i].events & EPOLLOUT) {
		pnm->f_epollout(pnm);
	    } else if (elog_events[i].events & EPOLLERR) {
		pnm->f_epollerr(pnm);
	    } else if (elog_events[i].events & EPOLLHUP) {
		pnm->f_epollhup(pnm);
	    }
	    pthread_mutex_unlock(&pnm->network_manager_log_lock);
	}
	/*! clear the free queue list */
	clean_up_free_nm_list();

    }				// while(1); infinite loop

    printf("nkn_launch_misc_logd_server: unexpected exited\n");
    close(elog_epfd);
    free(listener_pnm);
    free(al_listener_pnm);
    return arg;


}

/*
 * epoll_in/out/err operation function
 */
int
register_log_socket(int fd,
		    void *private_data,
		    LOG_func f_epollin,
		    LOG_func f_epollout,
		    LOG_func f_epollerr,
		    LOG_func f_epollhup,
		    LOG_func_timeout f_timeout,
		    log_network_mgr_t ** pnm)
{

    log_del_event_epoll(fd);
    if (*pnm == NULL) {

	*pnm = (log_network_mgr_t *) calloc(sizeof(log_network_mgr_t), 1);
	if (!(*pnm))
	    return FALSE;
	memset((char *) (*pnm), 0, sizeof(log_network_mgr_t));
	pthread_mutex_init(&((*pnm)->network_manager_log_lock), NULL);
    }
    (*pnm)->fd = fd;
    (*pnm)->private_data = private_data;
    (*pnm)->f_epollin = f_epollin;
    (*pnm)->f_epollout = f_epollout;
    (*pnm)->f_epollerr = f_epollerr;
    (*pnm)->f_epollhup = f_epollhup;
    (*pnm)->f_timeout = f_timeout;
    LOG_UNSET_FLAG((*pnm), LOGD_NW_MGR_MAGIC_ALIVE);
    log_set_socket_active(*pnm);

    return TRUE;
}

static inline int
log_add_fd_to_event(log_network_mgr_t * pnm,
		    uint32_t epollev)
{
    struct epoll_event ev;
    int op;

    /* initialized to make valgrind happy only */
    memset((char *) &ev, 0, sizeof(struct epoll_event));

    ev.events = epollev;
    ev.data.ptr = pnm;
    if (epoll_ctl(elog_epfd, EPOLL_CTL_ADD, pnm->fd, &ev) < 0) {
	complain_error_errno(1, "epoll_ctl");
	if (errno == EEXIST) {
	    return TRUE;
	}
	return FALSE;
    }
    return TRUE;
}

int
log_add_event_epollin(log_network_mgr_t * pnm)
{
    return log_add_fd_to_event(pnm, EPOLLIN);
}

int
log_add_event_epollout(log_network_mgr_t * pnm)
{
    return log_add_fd_to_event(pnm, EPOLLOUT);
}

int
log_del_event_epoll_inout(log_network_mgr_t * pnm)
{
    return log_add_fd_to_event(pnm, EPOLLERR | EPOLLHUP);
}

int
log_del_event_epoll(int fd)
{
    if (epoll_ctl(elog_epfd, EPOLL_CTL_DEL, fd, NULL) < 0) {
	return FALSE;
    }
    return TRUE;
}

int
log_set_socket_nonblock(int fd)
{
    int opts;
    opts = fcntl(fd, F_GETFL);
    if (opts < 0) {
	complain_error_errno(1, "fcntl");
	return FALSE;
    }
    opts = (opts | O_NONBLOCK);
    if (fcntl(fd, F_SETFL, opts) < 0) {
	complain_error_errno(1, "fcntl");
	return FALSE;
    }
    return TRUE;
}

int
unregister_log_socket(log_network_mgr_t * pnm)
{
    /* This function does not close the socket */
    log_del_event_epoll(pnm->fd);
    return TRUE;
}


static void
log_set_socket_active(log_network_mgr_t * pnm)
{
    if (pnm->f_timeout == NULL)
	return;
    gettimeofday(&pnm->last_active, NULL);
}


void
log_add_nm_to_freelist_queue(log_network_mgr_t * pnm)
{
    pthread_mutex_lock(&nm_free_queue_lock);
    LIST_INSERT_HEAD(&g_nm_to_free_queue, pnm, entries);
    pthread_mutex_unlock(&nm_free_queue_lock);
}



/***************************************************************************
 * Log server socket handling
 ***************************************************************************/

static int
unknown_epollin(log_network_mgr_t * pnm)
{
    char buf[1024];
    unsigned int len;
    NLP_client_hello *pnlpch;

    len = recv(pnm->fd, buf, 1024, 0);
    if (len <= 0) {
	complain_error_errno(1, "recv failed");
	unregister_free_close_sock(pnm);
	return FALSE;
    }
    if (len < sizeof(NLP_client_hello)) {
	unregister_free_close_sock(pnm);
	return FALSE;
    }
    log_del_event_epoll(pnm->fd);

    pnlpch = (NLP_client_hello *) buf;

    /*! currently supported socket signals */
    if (strcmp(pnlpch->sig, SIG_SEND_DEBUGLOG) == 0) {
	el_handle_new_send_socket(pnm);
    } else if (strcmp(pnlpch->sig, SIG_SEND_STREAMLOG) == 0) {
	sl_handle_new_send_socket(pnm);
    } else if (strcmp(pnlpch->sig, SIG_REQ_DEBUGLOG) == 0) {
	//el_handle_new_req_socket(sockfd);
    } else if (strcmp(pnlpch->sig, SIG_REQ_STREAMLOG) == 0) {
	//sl_handle_new_req_socket(sockfd);
    } else {
	printf("invalid log\n");
	unregister_free_close_sock(pnm);
	return FALSE;
    }
    return TRUE;
}

static int
unknown_epollout(log_network_mgr_t * pnm)
{
    unregister_free_close_sock(pnm);
    return FALSE;
}

static int
unknown_epollerr(log_network_mgr_t * pnm)
{
    unregister_free_close_sock(pnm);
    return FALSE;
}

static int
unknown_epollhup(log_network_mgr_t * pnm)
{
    unregister_free_close_sock(pnm);
    return FALSE;
}

static int
unknown_timeout(log_network_mgr_t * pnm, double timeout)
{
    UNUSED_ARGUMENT(timeout);
    unregister_free_close_sock(pnm);
    return FALSE;
}

void
unregister_free_close_sock(log_network_mgr_t * pnm)
{

    /*! 1. unregister the registered socket  and free privated data if there
       2. invalid the log manager
       3. finally add it to the free queue list
     */
    if (!pnm)
	return;
    unregister_async_free_close_sock(pnm);
    LOG_UNSET_FLAG(pnm, LOGD_NW_MGR_MAGIC_ALIVE);
    log_add_nm_to_freelist_queue(pnm);
}

void
unregister_async_free_close_sock(log_network_mgr_t * pnm)
{
    unregister_log_socket(pnm);
    close(pnm->fd);
    if (pnm->private_data != NULL) {
	free(pnm->private_data);
	pnm->private_data = NULL;
    }
}

/***************************************************************************
 * Log server socket handling
 ***************************************************************************/
static int
logsvr_epollin(log_network_mgr_t * pnm)
{
    int clifd;
    int cnt;
    struct sockaddr_in addr;
    socklen_t addrlen;

    addrlen = sizeof(struct sockaddr_in);
    clifd = accept(pnm->fd, (struct sockaddr *) &addr, &addrlen);
    if (clifd == -1) {
	complain_error_errno(1, "accept failed");
	return TRUE;
    }
    // valid socket
    if (log_set_socket_nonblock(clifd) == FALSE) {
	//printf( "Failed to set socket %d to be nonblocking socket.", clifd);
	close(clifd);
	return TRUE;
    }

    log_network_mgr_t *current_nm = NULL;

    if (register_log_socket(clifd,
			    NULL,
			    unknown_epollin,
			    unknown_epollout,
			    unknown_epollerr,
			    unknown_epollhup,
			    unknown_timeout, &current_nm) == FALSE) {
	// will this case ever happen at all?
	close(clifd);
	return TRUE;
    }
    log_add_event_epollin(current_nm);

    return TRUE;
}

void
donotreachme(const char *func_name)
{
     complain_error_msg(1, "Shouldn't reach here from %s!!", func_name);
     exit(EXIT_FAILURE);
}
static int
logsvr_epollout(log_network_mgr_t * pnm)
{
    donotreachme("logsvr_epollout");
    return FALSE;
}

static int
logsvr_epollerr(log_network_mgr_t * pnm)
{
    donotreachme("logsvr_epollerr" );
    return FALSE;
}

static int
logsvr_epollhup(log_network_mgr_t * pnm)
{
    donotreachme("logsvr_epollhup" );
    return FALSE;
}


/*
 * Launch the nknlogd server.
 */
void
logsvr_init(log_network_mgr_t ** listener_pnm)
{
 //   struct sockaddr_in srv;
    int ret;
    int val;
    int i;
    int listenfd;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	//printf("failed to create a socket errno=%d", errno);
	complain_error_errno(1, "socket creation failed");
	exit(1);
    }

    bzero(&srv, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_addr.s_addr = 0x0100007F;	//INADDR_ANY;
    srv.sin_port = htons(log_port_listen);
    val = 1;
    ret =
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    if (ret == -1) {
	exit(1);
    }

    if (bind(listenfd, (struct sockaddr *) &srv, sizeof(srv)) < 0) {
	complain_error_errno(1, "bind failed");
	exit(1);
    }

    log_set_socket_nonblock(listenfd);

    if(listen(listenfd, 20) != 0) {
	complain_error_errno(1, "listen failed");
    }
    log_network_mgr_t *current_nm = NULL;

    if (register_log_socket(listenfd,
			    NULL,
			    logsvr_epollin,
			    logsvr_epollout,
			    logsvr_epollerr,
			    logsvr_epollhup, NULL, &current_nm) == FALSE) {
	close(listenfd);
	free(current_nm);
	exit(1);
    }
    *listener_pnm = current_nm;
    log_add_event_epollin(current_nm);
}

