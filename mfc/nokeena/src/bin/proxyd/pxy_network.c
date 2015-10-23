#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <dlfcn.h>
#include <netdb.h>
#include <time.h>
#include <sys/timeb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/prctl.h>
#include <sys/ioctl.h>
#include <linux/unistd.h>
#include <linux/sockios.h>
#ifndef __USE_MISC
#define __USE_MISC
#endif
#include <net/if.h>
#include <netinet/tcp.h>

#include "nkn_debug.h"
#include "nkn_stat.h"
#include "nkn_assert.h"
#include "nkn_lockstat.h"
#include "pxy_network.h"

int NM_tot_threads = 2;

network_mgr_t * gnm = NULL;	// global network manager array

nkn_lockstat_t nm_lockstat[MAX_EPOLL_THREADS];
static nkn_mutex_t fd_type_mutex;
int debug_fd_trace = 1;

NM_main_arg_t g_NM_main_args = {NULL, NULL, NULL};

/*
 * This global variable will specify how many epoll thread will run
 */
struct nm_threads g_NM_thrs[MAX_EPOLL_THREADS];

int nkn_max_client = MAX_GNM;


/*
 * init functions
 */
void pxy_NM_init()
{
        libpxy_NM_init(NM_tot_threads, nkn_max_client, &gnm, &g_NM_thrs[0], 
                      &nm_lockstat[0], &fd_type_mutex) ;
}


/*
 * timer functions
 * timer function will be called once every 5 seconds
 */

void pxy_NM_timer(void)
{
        libpxy_NM_timer(NM_tot_threads, &g_NM_thrs[0], &gnm[0], &nm_lockstat[0]) ;
}

/*
 * Strictly speaking: 
 *     This function returns FALSE only when we run out of license.
 *     The only callers who require a new license are 
 *     init_conn() for HTTP and init_rtsp_conn() for RTSP.
 */
int pxy_NM_register_socket(int fd, 
        uint32_t    type,
	void * private_data,
	NM_func f_epollin,
	NM_func f_epollout,
	NM_func f_epollerr,
	NM_func f_epollhup,
	NM_func_timeout f_timeout,
	int timeout	// in seconds, timeout func callbacked at [timeout, timeout+5) seconds
	)
{

        return libpxy_NM_register_socket(fd, private_data, 
                                          type,
                                          f_epollin,
                                          f_epollout,
                                          f_epollerr,
                                          f_epollhup,
                                          f_timeout,
                                          timeout, 
                                          &gnm[0], 
                                          &g_NM_thrs[0],
                                          &nm_lockstat[0]) ;
}

/*
 * epoll_in/out/err operation function
 */

int pxy_NM_add_event_epollin(int fd)
{
        return libpxy_NM_add_event_epollin(fd, &gnm[0]) ;
}

int pxy_NM_add_event_epollout(int fd)
{
        return libpxy_NM_add_event_epollout(fd, &gnm[0]) ;
}

int pxy_NM_del_event_epoll_inout(int fd)
{
        return libpxy_NM_del_event_epoll_inout(fd, &gnm[0]) ;
}

int pxy_NM_del_event_epoll(int fd)
{
        return libpxy_NM_del_event_epoll(fd, &gnm[0]) ;
}


/*
 * epoll_in/out/err operation function
 */

int pxy_NM_set_socket_nonblock(int fd)
{
        return libpxy_NM_set_socket_nonblock(fd) ;
}


/*
 * ***  IMPORTANT NOTES  ****:
 * This function requires ROOT account priviledge.
 */
int pxy_NM_bind_socket_to_interface(int fd, char * p_interface)
{
        return libpxy_NM_bind_socket_to_interface(fd, p_interface) ;
}

/*
 * Caller should get the mutex before calling this function
 */
void pxy_NM_close_socket(int fd)
{
        libpxy_NM_close_socket(fd, &gnm[0], &nm_lockstat[0]) ;
}

/*
 * Caller should get the mutex before calling this function
 *
 * unregister_NM_socket is the same as NM_close_socket
 * except not closing the socket.
 *
 */
int pxy_NM_unregister_socket(int fd)
{
        return libpxy_NM_unregister_socket(fd, &gnm[0], &nm_lockstat[0]) ;
}

void pxy_NM_set_socket_active(network_mgr_t * pnm)
{
        libpxy_NM_set_socket_active(pnm, &nm_lockstat[0]) ;
}


void pxy_NM_main( void )
{
        g_NM_main_args.gnm_arg = &gnm[0] ;
        g_NM_main_args.nm_thr_arg = &g_NM_thrs[0] ;
        g_NM_main_args.pnm_lockstat_arg = &nm_lockstat[0] ;

        libpxy_NM_main(NM_tot_threads, &g_NM_main_args) ;
}


