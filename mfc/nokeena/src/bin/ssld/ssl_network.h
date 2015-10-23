#ifndef __NETWORK__H
#define __NETWORK__H

#include <sys/queue.h>
#include <sys/socket.h>
#include "ssl_defs.h"
#include "nkn_lockstat.h"


extern int NM_tot_threads;

//#define SOCKFD_TRACE	1
/*
 * Network Manager table is the global table for storing socket information.
 * This table is indexed by socket id.
 * This table is linked with nkn_max_client configuration field.
 * Every operation on any table entry should get this entries' mutex first.
 */
typedef int (* NM_func)(int sockfd, void * data);
typedef int (* NM_func_timeout)(int sockfd, void * data, double timeout);

#define NMF_IN_USE	0x0000000000000001
#define NMF_IN_TIMEOUTQ 0x0000000000000002	// In Timeout Queue
#define NMF_IN_EPOLL	0x0000000000000004	// added in Epoll
#define NMF_IN_SBQ 	0x0000000000000008	// In Session Bandwidth Queue
#define NMF_IN_EPOLLIN	0x0000000000000020	// added in Epollin
#define NMF_IN_EPOLLOUT	0x0000000000000040	// added in Epollout

#define NMF_IS_IPV6	0x0000000000000400	// FD is og thr type IPv6

#define MAX_TOQ_SLOT 	300			// Timeout Queue slot

struct nm_threads ;
typedef struct network_mgr {
	uint64_t flag;
	uint32_t incarn;		// Updated on socket close
	int32_t  fd;
	int32_t accepted_thr_num;
	int32_t peer_fd;
	struct nm_threads * pthr;	// which thread this nm is running on

	/* Timer */
	time_t          last_active;    // Last active time
	LIST_ENTRY(network_mgr) toq_entries;	// In Timeout queue

        /* Mutex to pretec this epoll group */
        pthread_mutex_t mutex;		// Mutex to protect this structure only.

	/* callback functions */
	void * private_data;		// caller private data
	NM_func f_epollin;
	NM_func f_epollout;
	NM_func f_epollerr;
	NM_func f_epollhup;
	NM_func_timeout f_timeout;
	int timeout_slot;		// each slot is 5 seconds
} network_mgr_t;

#define NM_SET_FLAG(x, f)	(x)->flag |= (f);
#define NM_UNSET_FLAG(x, f)	(x)->flag &= ~(f);
#define NM_CHECK_FLAG(x, f)	((x)->flag & (f))

LIST_HEAD(nm_timeout_queue, network_mgr);

/*
 * This structure is defined for each running epoll thread.
 * Each network epoll thread owns one structure.
 * Any operation on timeout and bandwidth queues should get table mutex first.
 */
typedef struct nm_threads {
        pthread_t pid;          // thread id
	int num;		// This thread number, counting from 0

        // epoll control management variables
        AO_t cur_fds;     	// currently total open fds in this thread
        unsigned int max_fds;   // Maxixum allowed total fds in this thread
        int epfd;

        // fd timeout slots.
        int cur_toq;
        struct nm_timeout_queue toq_head[MAX_TOQ_SLOT];

	pthread_mutex_t nm_mutex; 	// protect nm_timeout_queue and nm_sess_bw_queue

        // events used for this epoll thread
        struct epoll_event *events;
        unsigned int epoll_event_cnt;

} nm_threads_t;

#define MAX_EPOLL_THREADS       16

#define LB_ROUNDROBIN	0
#define LB_STICK	1	/* 1 network thread bound to 1 network port */

#define USE_LICENSE_FALSE	0
#define USE_LICENSE_TRUE	1
#define USE_LICENSE_ALWAYS_TRUE	2

/* 
 * socket is registered by each application module.
 * register_NM_socket() inside handles to acquire/release mutex.
 * multiple application modules can call in the same time.
 */
int register_NM_socket(int fd,
        void * private_data,
        NM_func f_epollin,
        NM_func f_epollout,
        NM_func f_epollerr,
        NM_func f_epollhup,
        NM_func_timeout f_timeout,
        int timeout,  // In seconds
	int thr_num);	// running on this network thread

/* 
 * socket is unregistered by each application module.
 * unregister_NM_socket()/NM_close_socket() inside handle to acquire/release mutex.
 * multiple application modules can call in the same time.
 * The difference between these two functions are close socket only.
 * NM_close_socket() closes the socket.
 */
void NM_close_socket(int fd);
int unregister_NM_socket(int fd);

/*
 * Before calling any following functions, caller should get the gnm.mutex locker first
 */
void NM_change_socket_timeout_time (network_mgr_t * pnm, int next_timeout_slot); // one slot = 5seconds
int NM_set_socket_nonblock(int fd);
int NM_bind_socket_to_interface(int fd, char * p_interface);
int NM_add_event_epollin(int fd);
int NM_add_event_epollout(int fd);
int NM_del_event_epoll(int fd);
int NM_del_event_epoll_inout(int fd);
void NM_set_socket_active(network_mgr_t * pnm);
int NM_get_tcp_info(int fd, void *tcp_con_info);
int NM_get_tcp_sendq_pending_bytes(int fd, void *pending_bytes);
int NM_get_tcp_recvq_pending_bytes(int fd, void *pending_bytes);



/*
 * Mutex is managed within these functions.
 * Caller has no need to acquire the mutex.
 */
void NM_init(void);
void NM_timer(void);
void NM_main(void);

#endif // __NETWORK__H

