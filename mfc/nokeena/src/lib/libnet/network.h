#ifndef __NETWORK__H
#define __NETWORK__H

#include <sys/queue.h>
#include <sys/socket.h>
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
#include <net/if.h>
#include <netinet/tcp.h>
#include "nkn_lockstat.h"

#ifdef LIB_NET
#include "network_int.h"
#else /* !LIB_NET */
#include "nkn_defs.h"
#include "network_hres_timer.h"
#include "nkn_lockstat.h"
#endif /* !LIB_NET */

/*
 * Comment the following line to disable fd event trace.
 */
#define SOCKFD_TRACE	0
#define DBG_EVENT_TRACE	0
#define DBG_MUTEX_TRACE	0
#define DBG_FLOW_TRACE	0

/*
 * Network Manager table is the global table for storing socket information.
 * This table is indexed by socket id.
 * This table is linked with nkn_max_client configuration field.
 * Every operation on any table entry should get this entries' mutex first.
 */
typedef int (* NM_func)(int sockfd, void * data);
typedef int (* NM_func_timeout)(int sockfd, void * data, double timeout);
typedef int (* NM_func_timer)(int sockfd, void * data, net_timer_type_t type);

#define NMF_IN_USE	0x0000000000000001
#define NMF_IN_TIMEOUTQ 0x0000000000000002	// In Timeout Queue
#define NMF_IN_EPOLL	0x0000000000000004	// added in Epoll
#define NMF_IN_SBQ 	0x0000000000000008	// In Session Bandwidth Queue
#define NMF_USE_LICENSE	0x0000000000000010	// Use License
#define NMF_IN_EPOLLIN	0x0000000000000020	// added in Epollin
#define NMF_IN_EPOLLOUT	0x0000000000000040	// added in Epollout
#define NMF_NO_FD_UNEPOLL 0x0000000000000080	// Didn't unregister fd from epoll.
#define NMF_RST_WAIT	0x0000000000000100	// Wait to data go out for RST feature
#define NMF_TIMEOUTQ_LOOP 0x0000000000000200	// TIMEOUT Q loop
#define NMF_IS_IPV6	 0x0000000000000400	// FD is of the type IPv6
#define NMF_SUSPENDED	 0x0000000000000800	// Epoll events suspeneded

#define NMF_HAVE_LOCK	0x1000000000000000	// Mark state of LOCK

#define MAX_TOQ_SLOT 	300			// Timeout Queue slot

struct nm_threads ;
#ifdef SOCKFD_TRACE
struct nm_trace ;
#endif
typedef struct network_mgr {
	uint64_t flag;
	nkn_fd_type_t fd_type;
	uint32_t incarn;		// Updated on socket close
	int32_t  fd;
	int32_t accepted_thr_num;
	struct nm_threads * pthr;	// which thread this nm is running on

	/* Timer */
	time_t          last_active;    // Last active time
	LIST_ENTRY(network_mgr) toq_entries;	// In Timeout queue
	LIST_ENTRY(network_mgr) sbq_entries;	// For session bandwidth queue

        /* Mutex to pretec this epoll group */
        pthread_mutex_t mutex;		// Mutex to protect this structure only.

	/* callback functions */
	void * private_data;		// caller private data
	NM_func f_epollin;
	NM_func f_epollout;
	NM_func f_epollerr;
	NM_func f_epollhup;
	NM_func_timeout f_timeout;
	NM_func_timer f_timer;
	int timeout_slot;		// each slot is 5 seconds
#ifdef SOCKFD_TRACE
	struct nm_trace * p_trace;
#endif
} network_mgr_t;

#define NM_SET_FLAG(x, f)	(x)->flag |= (f);
#define NM_UNSET_FLAG(x, f)	(x)->flag &= ~(f);
#define NM_CHECK_FLAG(x, f)	((x)->flag & (f))

LIST_HEAD(nm_timeout_queue, network_mgr);
LIST_HEAD(nm_sess_bw_queue, network_mgr);

/*
 * This structure is defined for each running epoll thread.
 * Each network epoll thread owns one structure.
 * Any operation on timeout and bandwidth queues should get table mutex first.
 */
typedef struct nm_threads {
        pthread_t pid;          // thread id
	int num;		// This thread number, counting from 0
	int send_thread_mutex_num;

        // epoll control management variables
        AO_t cur_fds;     	// currently total open fds in this thread
        unsigned int max_fds;   // Maxixum allowed total fds in this thread
        int epfd;


        // fd timeout slots.
        int cur_toq;
        struct nm_timeout_queue toq_head[MAX_TOQ_SLOT];

	// fd session bandwidth queue
	/* 
	 * we provided two sbq queue for swap purpose to break a loop 
	 * Sometime in timeout session, we could insert itself back to
	 * the sbq.
	 */
	int cleanup_sbq;	// flag time to clean up the SB Queue
	int cur_sbq;
        struct nm_sess_bw_queue sbq_head[2];

	pthread_mutex_t nm_mutex; 	// protect nm_timeout_queue and nm_sess_bw_queue

        // events used for this epoll thread
        struct epoll_event *events;
	unsigned int epoll_event_cnt;

} nm_threads_t;

#ifdef SOCKFD_TRACE

#define NUM_EVENT_TRACE		128
#define NUM_LOCK_TRACE		16
#define NUM_FLOW_TRACE		128
typedef struct nm_trace {
#ifdef DBG_EVENT_TRACE
	pthread_mutex_t mutex;
#ifdef DBG_FLOW_TRACE
	uint32_t f_idx;
	uint16_t f_event[NUM_FLOW_TRACE*3];
#else
	uint32_t cur_event;
	uint8_t event[NUM_EVENT_TRACE*2];
#endif
#endif
#ifdef DBG_MUTEX_TRACE
	uint32_t lock_idx;
	int32_t lock_event[NUM_LOCK_TRACE];
#endif
} nm_trace_t;

extern network_mgr_t * gnm;
extern int debug_fd_trace;

#ifdef DBG_EVENT_TRACE
#ifdef DBG_FLOW_TRACE
static inline void
dbg_nm_flow_trace(int fd, int file, int line_num)
{
	int idx;

	if (debug_fd_trace && fd > 0 && fd < MAX_GNM && gnm[fd].p_trace) {
		pthread_mutex_lock(&gnm[fd].p_trace->mutex);
		gnm[fd].p_trace->f_idx++;
		idx = (gnm[fd].p_trace->f_idx % NUM_FLOW_TRACE) * 3;
		gnm[fd].p_trace->f_event[idx]   = (uint16_t)(nkn_cur_ts & 0x00ffff);
		gnm[fd].p_trace->f_event[idx+1] = (uint16_t)file;
		gnm[fd].p_trace->f_event[idx+2] = (uint16_t)line_num;
		pthread_mutex_unlock(&gnm[fd].p_trace->mutex);
	}
}

#define NM_FLOW_TRACE(_fd)		{dbg_nm_flow_trace((_fd), (F_FILE_ID), (__LINE__));}
#define NM_EVENT_TRACE(_fd, _event)	{dbg_nm_flow_trace((_fd), (F_FILE_ID), (__LINE__));}
#else // DBG_FLOW_TRACE
static inline void
dbg_nm_event_trace(int fd, int event)
{
	if (debug_fd_trace && gnm[fd].p_trace) {
		pthread_mutex_lock(&gnm[fd].p_trace->mutex);
		gnm[fd].p_trace->cur_event++;
		gnm[fd].p_trace->event[(gnm[fd].p_trace->cur_event%NUM_EVENT_TRACE)*2] = 
			(uint8_t)(gnm[fd].p_trace->cur_event & 0x00ff);
		gnm[fd].p_trace->event[(gnm[fd].p_trace->cur_event%NUM_EVENT_TRACE)*2+1] = 
			(uint8_t)((event)& 0x00ff);
		pthread_mutex_unlock(&gnm[fd].p_trace->mutex);
	}
}

#define NM_EVENT_TRACE(_fd, _event)	{dbg_nm_event_trace((_fd), (_event));}
#define NM_FLOW_TRACE(_fd)		{}
#endif // DBG_FLOW_TRACE
#else  // DBG_EVENT_TRACE
#define NM_EVENT_TRACE(_fd, _event)	{}
#define NM_FLOW_TRACE(_fd)		{}
#endif //DBG_EVENT_TRACE

#ifdef DBG_MUTEX_TRACE
static inline void
dbg_nm_lock_trace(network_mgr_t * pnm, int lock, int file, int event)
{
	if (debug_fd_trace && pnm->p_trace) {
		//pthread_mutex_lock(&pnm->p_trace->mutex); 
		//if (event > lt_file_num_lines[f]) {
		//if (event > 0) assert(0);
		//assert(f > 7);
		pnm->p_trace->lock_idx++;
		pnm->p_trace->lock_event[(pnm->p_trace->lock_idx%NUM_LOCK_TRACE)] = 
			(lock << 24) | (file << 16) | event;
		//pthread_mutex_unlock(&pnm->p_trace->mutex); 
	}
}

#define NM_TRACE_LOCK(pnm, _file)	{dbg_nm_lock_trace((pnm), 1, (_file), (__LINE__)); \
	NM_SET_FLAG(pnm, NMF_HAVE_LOCK); \
}
#define NM_TRACE_UNLOCK(pnm, _file)	{dbg_nm_lock_trace((pnm), 0, (_file), (__LINE__)); \
	NM_UNSET_FLAG(pnm, NMF_HAVE_LOCK); \
}
#else
#define NM_TRACE_LOCK(pnm, _file)	{NM_SET_FLAG(pnm, NMF_HAVE_LOCK);}
#define NM_TRACE_UNLOCK(pnm, _file)	{NM_UNSET_FLAG(pnm, NMF_HAVE_LOCK);}
#endif

#else
#define NM_EVENT_TRACE(_fd, _event)	{}
#define NM_TRACE_LOCK(pnm, _file)	{NM_SET_FLAG(pnm, NMF_HAVE_LOCK);}
#define NM_TRACE_UNLOCK(pnm, _file)	{NM_UNSET_FLAG(pnm, NMF_HAVE_LOCK);}
#define NM_FLOW_TRACE(_fd)		{}
#endif

#define MAX_EPOLL_THREADS       32


#define LB_FD		0
#define LB_ROUNDROBIN	1
#define LB_STICK	2	/* 1 network thread bound to 1 network port */

#define USE_LICENSE_FALSE	0
#define USE_LICENSE_TRUE	1
#define USE_LICENSE_ALWAYS_TRUE	2

#if 0
/*
 * The following function is defined for backup purpose.
 * just in case nkn_replace_func() does not work.
 */
extern int (*f_accept)(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
extern int (*f_bind)(int sockfd, const struct sockaddr *my_addr, socklen_t addrlen);
extern int (*f_connect)(int sockfd, const struct sockaddr *serv_addr, socklen_t addrlen);
extern int (*f_getpeername)(int s, struct sockaddr *name, socklen_t *namelen);
extern int (*f_getsockname)(int s, struct sockaddr *name, socklen_t *namelen);
extern int (*f_getsockopt)(int s, int level, int optname, void *optval, socklen_t *optlen);
extern int (*f_setsockopt)(int s, int level, int optname, const void *optval, socklen_t optlen);
extern int (*f_listen)(int sockfd, int backlog);
extern ssize_t (*f_recv)(int s, void *buf, size_t len, int flags);
extern ssize_t (*f_recvfrom)(int s, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen);
extern ssize_t (*f_recvmsg)(int s, struct msghdr *msg, int flags);
extern ssize_t (*f_send)(int s, const void *buf, size_t len, int flags);
extern ssize_t (*f_sendto)(int  s,  const  void  *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen);
extern ssize_t (*f_sendmsg)(int s, const struct msghdr *msg, int flags);
extern int (*f_shutdown)(int s, int how);
extern int (*f_socket)(int domain, int type, int protocol);
extern int (*f_sockatmark)(int fd);
extern int (*f_socketpair)(int d, int type, int protocol, int sv[2]);
extern int (*f_epoll_create)(int size);
extern int (*f_epoll_ctl)(int epfd, int op, int fd, struct epoll_event *event);
extern int (*f_epoll_wait)(int epfd, struct epoll_event * events, int maxevents, int timeout);
extern ssize_t (*f_readv)(int fd, const struct iovec *vector, int count);
extern ssize_t (*f_writev)(int fd, const struct iovec *vector, int count);
extern int (*f_close)(int fd);
//extern int (*f_fcntl)(int fd, int cmd);
#endif // 0


/*
 *	Defines for fd trace
 */
#define FT_INIT					0x00
#define FT_REGISTER_NM_SOCKET			0x01
#define FT_UNREGISTER_NM_SOCKET			0x02
#define FT_SUSPEND_NM_SOCKET			0x03
#define FT_RESUME_NM_SOCKET			0x04
#define FT_NKN_CLOSE_FD				0x05
#define FT_NM_CLOSE_SOCKET			0x06

#define FT_INIT_CONN				0x10
#define FT_HTTP_EPOLLIN				0x11
#define FT_HTTP_EPOLLOUT			0x12
#define FT_HTTP_EPOLLERR			0x13
#define FT_HTTP_EPOLLHUP			0x14
#define FT_HTTP_TIMER				0x15
#define FT_HTTP_TIMEOUT				0x16
#define FT_HTTP_MGR_OUTPUT			0x17
#define FT_HTTP_TRY_TO_SENDOUT_DATA		0x18
#define FT_CLOSE_CONN				0x19

#define FT_FP_MAKE_TUNNEL			0x30
#define FT_FPHTTP_EPOLLIN			0x31
#define FT_FPHTTP_EPOLLOUT			0x32
#define FT_FPHTTP_EPOLLERR			0x33
#define FT_FPHTTP_EPOLLHUP			0x34
#define FT_CP_TUNNEL_CLIENT_DATA_TO_SERVER	0x35
#define FT_FP_FORWARD_CLIENT_TO_SERVER		0x36
#define FT_FP_FORWARD_SERVER_TO_CLIENT		0x37
#define FT_FP_CONNECTED				0x38
#define FT_FP_CONNECT_FAILED			0x39
#define FT_FP_SEND_HTTPREQ			0x31
#define FT_FP_HTTP_HEADER_RECEIVED		0x3b
#define FT_FP_HTTP_BODY_RECEIVED		0x3c
#define FT_FP_CLOSE_PROXY_SOCKET		0x3d
#define FT_FP_INTERNAL_CLOSED			0x3e

#define FT_FP_ADD_EVENT				0x50
#define FT_FP_ADD_EVENT_OP_CLOSE		0x50
#define FT_FP_ADD_EVENT_OP_EPOLLIN		0x51
#define FT_FP_ADD_EVENT_OP_EPOLLOUT		0x52
#define FT_FP_ADD_EVENT_OP_EPOLLERR		0x53
#define FT_FP_ADD_EVENT_OP_TUNNEL_READY		0x54
#define FT_FP_ADD_EVENT_OP_CLOSE_CALLBACK	0x55
#define FT_FP_ADD_EVENT_OP_TIMER_CALLBACK	0x56

#define FT_FP_ADD_EVENT_REPLACE				0x5d
#define FT_FP_ADD_REPLACE_WITH_CLOSE			0x5e
#define FT_FP_REMOVE_EVENT				0x5f
#define FT_CHECK_OUT_FP_QUEUE				0x60
#define FT_CHECK_OUT_FP_QUEUE_OP_CLOSE			0x60
#define FT_CHECK_OUT_FP_QUEUE_OP_EPOLLIN		0x61
#define FT_CHECK_OUT_FP_QUEUE_OP_EPOLLOUT		0x62
#define FT_CHECK_OUT_FP_QUEUE_OP_EPOLLERR		0x63
#define FT_CHECK_OUT_FP_QUEUE_OP_TUNNEL_READY		0x64
#define FT_CHECK_OUT_FP_QUEUE_OP_CLOSE_CALLBACK		0x65
#define FT_CHECK_OUT_FP_QUEUE_OP_TIMER_CALLBACK		0x66

#define FT_TASK_NEW					0x70
#define FT_TASK_CLEANUP					0x71
#define FT_TASK_CANCEL					0x72
#define FT_TASK_RESUME					0x73
#define FT_TASK_TIMEOUT					0x74

#define FT_CP_SEND_DATA_TO_TUNNEL			0x80
#define FT_CP_EPOLLIN					0x81
#define FT_CP_EPOLLOUT					0x82
#define FT_CP_EPOLLERR					0x83
#define FT_CP_EPOLLHUP					0x84
#define FT_CP_TIMER					0x85
#define FT_CP_TIMEOUT					0x86
#define FT_CP_INTERNAL_CLOSE_CONN			0x87
#define FT_CP_ADD_INTO_POOL				0x88
#define FT_CP_ACTIVATE_SOCKET_HANDLER			0x89
#define FT_CP_SYN_SENT					0x8a
#define FT_CP_GET_SENT_CONTINUE_AFTER_CALLBACK		0x8b
#define FT_CP_GET_SENT					0x8c
#define FT_CP_HTTP_BODY					0x8d
#define FT_CP_EPOLL_RM_FM_POOL				0x8e

#define FT_CP_ADD_EVENT					0x90
#define FT_CP_ADD_EVENT_OP_CLOSE			0x90
#define FT_CP_ADD_EVENT_OP_EPOLLIN			0x91
#define FT_CP_ADD_EVENT_OP_EPOLLOUT			0x92
#define FT_CP_ADD_EVENT_OP_EPOLLERR			0x93
#define FT_CP_ADD_EVENT_OP_TUNNEL_READY			0x94
#define FT_CP_ADD_EVENT_OP_CLOSE_CALLBACK		0x95
#define FT_CP_ADD_EVENT_OP_TIMER_CALLBACK		0x96

#define FT_CHECK_OUT_CP_QUEUE				0x98
#define FT_CP_REMOVE_EVENT				0x9f

#define FT_OM_INIT_CLIENT				0xa0
#define FT_OM_DO_SOCKET_WORK				0xa1
#define FT_CP_CALLBACK_CONNECTED			0xa2
#define FT_CP_CALLBACK_CONNECT_FAILED			0xa3
#define FT_CP_CALLBACK_SEND_HTTPREQ			0xa4
#define FT_CP_CALLBACK_HTTP_HEADER_RECEIVED		0xa5
#define FT_CP_CALLBACK_HTTP_BODY_RECEIVED		0xa6
#define FT_CP_CALLBACK_CLOSE_SOCKET			0xa7
#define FT_CP_CALLBACK_TIMER				0xa8
#define FT_OM_CLOSE_CONN				0xa9
#define FT_OM_DO_SOCKET_VALIDATE_WORK			0xaa

#define LT_NETWORK		1
#define LT_SERVER		2
#define LT_OM_API		3
#define LT_OM_CONNPOLL		4
#define LT_OM_NETWORK		5
#define LT_OM_TUNNEL		6
#define LT_AUTH_HELPER		7

#define F_FILE_ID	LT_NETWORK

extern struct nm_threads g_NM_thrs[MAX_EPOLL_THREADS];
extern nkn_lockstat_t nm_lockstat[MAX_EPOLL_THREADS];

extern AO_t next_epthr;
extern AO_t glob_socket_accumulate_tot;
extern AO_t glob_socket_accumulate_tot_ipv6;
extern AO_t glob_cur_open_all_sockets;
extern AO_t glob_socket_cur_open_ipv6;
extern AO_t glob_socket_cur_open;
extern AO_t glob_socket_in_epoll;
extern AO_t glob_license_used;

extern uint32_t glob_warn_register_in_use;
extern uint32_t glob_err_timeout_f_timeout_NULL;
extern uint32_t glob_err_unsent_data_rst_close;

extern uint64_t glob_run_out_of_license;
extern uint64_t glob_register_bad_fd;
extern uint64_t glob_err_epoll_exist;
extern uint64_t glob_err_epoll_add;
extern uint64_t glob_err_epoll_del;
extern uint64_t glob_err_bad_timer_entry;
extern uint64_t glob_err_bad_timer_entry_1;
extern uint64_t glob_err_bad_timer_entry_2;
extern uint64_t glob_err_bad_timer_entry_3;
extern uint64_t glob_socket_tot_timeout;

extern int (* so_shutdown)(int s, int how);
extern void NM_set_socket_active(network_mgr_t * pnm);

/*
 * Strictly speaking: 
 *     This function returns FALSE only when we run out of license.
 *     The only callers who require a new license are 
 *     init_conn() for HTTP and init_rtsp_conn() for RTSP.
 */
static inline int
register_NM_socket(int fd, void * private_data, NM_func f_epollin,
		   NM_func f_epollout,
		   NM_func f_epollerr,
		   NM_func f_epollhup,
		   NM_func_timer f_timer,
		   NM_func_timeout f_timeout,
		   int timeout,	// in seconds, timeout func callbacked at [timeout, timeout+5) seconds
		   int useLicense,
		   int has_locked)
{
	network_mgr_t * pnm;	// global network manager array
	//static int next_epthr=0;
	int num;

	if( (fd<0) || (fd>=MAX_GNM) ) {
		glob_register_bad_fd ++;
		return FALSE;
	}

	NM_EVENT_TRACE(fd, FT_REGISTER_NM_SOCKET);
	pnm = &gnm[fd];
	DBG_LOG(MSG, MOD_NETWORK, "fd=%d (%d), type=%d", fd, gnm[fd].fd, gnm[fd].fd_type);
#if 0
	if((nm_lb_policy==LB_ROUNDROBIN) || (f_timeout==NULL)) {
		pthread_mutex_lock(&network_mutex);
		num=next_epthr;
		pthread_mutex_unlock(&network_mutex);
	} else /* if(nm_lb_policy == LB_STICK) */ {
		num=pnm->accepted_thr_num;
	}
#endif // 0
	if (nm_lb_policy == LB_FD) {
		num = fd % NM_tot_threads;	// Stick to the same network thread.
	} else {
		num = AO_fetch_and_add1(&next_epthr) % NM_tot_threads;
	}
	if((useLicense == USE_LICENSE_TRUE) &&
	   (AO_load(&g_NM_thrs[num].cur_fds) >= g_NM_thrs[num].max_fds))
	{
		glob_run_out_of_license++;
		return FALSE;
	}

	if (has_locked == FALSE) {
		pthread_mutex_lock(&pnm->mutex);
		NM_TRACE_LOCK(pnm, LT_NETWORK);
	}

	if ( !NM_CHECK_FLAG(pnm, NMF_IN_USE) ) {
		/*
		 * TBD: BUG 1074:
		 * If (fd == -1 and private_data==NULL) we only reset flag.
		 * why pnm->flag is unset is unknown.
		 */
		glob_warn_register_in_use++;
		if ((pnm->fd == -1) && (pnm->private_data == NULL)) {
			pnm->flag = pnm->flag & (NMF_HAVE_LOCK | NMF_IS_IPV6);
		}
	}

	pnm->fd = fd;
	pnm->private_data = private_data;
	pnm->f_epollin = f_epollin;
	pnm->f_epollout = f_epollout;
	pnm->f_epollerr = f_epollerr;
	pnm->f_epollhup = f_epollhup;
	pnm->f_timer = f_timer;
	pnm->f_timeout = f_timeout;
	timeout = (timeout / 2) + 1;	// timeout callback at [timeout, timeout+2] seconds
	if(timeout < 3) timeout = 3;	// min timeout is 6 seconds.
	if(timeout > MAX_TOQ_SLOT) timeout = MAX_TOQ_SLOT-2;	// -2 to avoid insert the same slot of itself
	pnm->timeout_slot = timeout; // timeout_slot is [3, MAX_TOQ_SLOT]
	pnm->pthr = &g_NM_thrs[num];
	NM_SET_FLAG(pnm, NMF_IN_USE);

	if (NM_CHECK_FLAG(pnm, NMF_NO_FD_UNEPOLL)) {
		/*
		 * Network Optimization to reduce some system calls.
		 * e.g. under the following case:
		 * when unregister and immediate register back a socket,
		 * we can save two system calls epoll_ctl(DEL) and epoll_ctl(ADD)
		 *
		 * This fd was in the epoll loop.
		 * We do not need to do anything.
		 */
		NM_UNSET_FLAG(pnm, NMF_NO_FD_UNEPOLL);
	}
	else {
	    // Remove from Timeout Queue as pnm->pthr could be changed
	    // If pnm->pthr is changed, it would not be removed from
	    //    existing timeout Q.
	    if (pnm->pthr && nm_lb_policy == LB_ROUNDROBIN) {
		NKN_MUTEX_LOCKSTAT(&pnm->pthr->nm_mutex, &nm_lockstat[pnm->pthr->num]);
		if (NM_CHECK_FLAG(pnm, NMF_IN_TIMEOUTQ)) {
			LIST_REMOVE(pnm, toq_entries);
			NM_UNSET_FLAG(pnm, NMF_IN_TIMEOUTQ);
		}
		NKN_MUTEX_UNLOCKSTAT(&pnm->pthr->nm_mutex);
	    }

#if 0
	   // pick up an epoll thread.
	   if((nm_lb_policy==LB_ROUNDROBIN) || (f_timeout==NULL)) {
		/* round robin policy */
		pthread_mutex_lock(&network_mutex);
		pnm->pthr = &g_NM_thrs[num];
		next_epthr++;
		if(next_epthr>=NM_tot_threads) next_epthr=0;
		pthread_mutex_unlock(&network_mutex);
	   } else {
		/* stick policy: 1 network thread bound to 1 network port */
		pnm->pthr = &g_NM_thrs[pnm->accepted_thr_num];
	   }
#endif // 0
	}

        // To make sure the connection link list is integrated one.
        //printf("init_conn: LIST_INSERT_HEAD( con, caq_entries)\n");
	NM_set_socket_active(pnm);

	if(useLicense != USE_LICENSE_FALSE) {
		NM_SET_FLAG(pnm, NMF_USE_LICENSE);
		AO_fetch_and_add1(&pnm->pthr->cur_fds);
		AO_fetch_and_add1(&glob_license_used);
	}

	if (has_locked == FALSE) {
		NM_TRACE_UNLOCK(pnm, LT_NETWORK);
		pthread_mutex_unlock(&pnm->mutex);
	}

	if (NM_CHECK_FLAG(pnm, NMF_IS_IPV6)) {
                AO_fetch_and_add1(&glob_socket_accumulate_tot_ipv6);
                AO_fetch_and_add1(&glob_socket_cur_open_ipv6);
        } else {
                AO_fetch_and_add1(&glob_socket_accumulate_tot);
                AO_fetch_and_add1(&glob_socket_cur_open);
        }
	return TRUE;
}

/*
 * epoll_in/out/err operation function
 */

static inline int
NM_add_fd_to_event(int fd, uint32_t epollev)
{
	struct epoll_event ev;
	int op;

	op = (NM_CHECK_FLAG(&gnm[fd], NMF_IN_EPOLL)) ?
		EPOLL_CTL_MOD : EPOLL_CTL_ADD;

	/* initialized to make valgrind happy only */
	memset((char *)&ev, 0, sizeof(struct epoll_event));

	ev.events = epollev;
	ev.data.fd = fd;
	if(epoll_ctl(gnm[fd].pthr->epfd, op, fd, &ev) < 0)
	{
		if(errno == EEXIST) {
			glob_err_epoll_exist++;
			return TRUE;
		}
		DBG_LOG(SEVERE, MOD_NETWORK, "Failed to add fd (%d), errno=%d", fd, errno);
		DBG_ERR(SEVERE, "Failed to add fd (%d), errno=%d", fd, errno);
		// BUG TBD:
		// sometime errno could return 9 (EBADF) error for valid epfd and fd
		// I don't know what root cause it is so far.
		//
		// I removed this assert.
		// We close this connection only and we don't want to crash the process.
		// BUG 429: is opened for this bug.
		glob_err_epoll_add++;
		//assert(0);
		return FALSE;
	}
	if( !NM_CHECK_FLAG(&gnm[fd], NMF_IN_EPOLL) ) {
		AO_fetch_and_add1(&glob_socket_in_epoll);
		NM_SET_FLAG(&gnm[fd], NMF_IN_EPOLL);
	}
	return TRUE;
}

static inline int
NM_add_event_epollin(int fd)
{
	if( NM_CHECK_FLAG(&gnm[fd], NMF_IN_EPOLLIN) ) {
		return TRUE;
	}
	if( NM_add_fd_to_event(fd, EPOLLIN) == FALSE ) {
		return FALSE;
	}
	NM_SET_FLAG(&gnm[fd], NMF_IN_EPOLLIN);
	NM_UNSET_FLAG(&gnm[fd], NMF_IN_EPOLLOUT);
	return TRUE;
}

static inline int
NM_add_event_epollout(int fd)
{
	if( NM_CHECK_FLAG(&gnm[fd], NMF_IN_EPOLLOUT) ) {
		return TRUE;
	}
	if( NM_add_fd_to_event(fd, EPOLLOUT) == FALSE ) {
		return FALSE;
	}
	NM_UNSET_FLAG(&gnm[fd], NMF_IN_EPOLLIN);
	NM_SET_FLAG(&gnm[fd], NMF_IN_EPOLLOUT);
	return TRUE;
}

static inline int
NM_add_event_epoll_out_err_hup(int fd)
{
	if (NM_add_fd_to_event(fd, EPOLLOUT | EPOLLERR | EPOLLHUP) == FALSE) {
		    return FALSE;
	}

	return TRUE;
}

static inline int
NM_del_event_epoll_inout(int fd)
{
	if( NM_add_fd_to_event(fd, EPOLLERR | EPOLLHUP) == FALSE ) {
		return FALSE;
	}
	NM_UNSET_FLAG(&gnm[fd], NMF_IN_EPOLLIN|NMF_IN_EPOLLOUT);
	return TRUE;
}

static inline int
NM_del_event_epoll(int fd)
{
	if( !NM_CHECK_FLAG(&gnm[fd], NMF_IN_EPOLL) ) {
		return TRUE;
	}

	if(epoll_ctl(gnm[fd].pthr->epfd, EPOLL_CTL_DEL, fd, NULL) < 0) {
		// This function call may return negative
		// when del_event_epoll() is called twice.
		glob_err_epoll_del++;
		return TRUE;
	}

	AO_fetch_and_sub1(&glob_socket_in_epoll);
	NM_UNSET_FLAG(&gnm[fd], NMF_IN_EPOLL);
	NM_UNSET_FLAG(&gnm[fd], NMF_IN_EPOLLIN|NMF_IN_EPOLLOUT);
	return TRUE;
}

/*
 * epoll_in/out/err operation function
 */

static inline int
NM_set_socket_nonblock(int fd)
{
        int opts;
	int on = 1;

	if(net_use_nkn_stack) {
		/*
		 * For user space TCP stack, we cannot set to nonblocking socket today.
		 */
		return TRUE;
	}
        opts = O_RDWR; //fcntl(fd, F_GETFL);
        if(opts < 0) {
		return FALSE;
        }
        opts = (opts | O_NONBLOCK);
        if(fcntl(fd, F_SETFL, opts) < 0) {
		return FALSE;
        }

	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &on, sizeof(on));

	return TRUE;
}

static inline int
NM_get_tcp_info(int fd, void *tcp_con_info)
{
	int tcp_info_size = sizeof(struct tcp_info);

	return getsockopt(fd, SOL_TCP, TCP_INFO,
			(void *)tcp_con_info, (socklen_t *)&tcp_info_size);

}

static inline int
NM_get_tcp_sendq_pending_bytes(int fd, void *pending_bytes)
{
	return ioctl(fd, SIOCOUTQ, pending_bytes);
}

static inline int
NM_get_tcp_recvq_pending_bytes(int fd, void *pending_bytes)
{
	return ioctl(fd, SIOCINQ, pending_bytes);
}

/*
 * ***  IMPORTANT NOTES  ****:
 * This function requires ROOT account priviledge.
 */
static inline int
NM_bind_socket_to_interface(int fd, char * p_interface)
{
	struct ifreq minterface;

	/* Initialized only to make valgrind happy. */
	memset((char *)&minterface, 0, sizeof(struct ifreq));

	strncpy(minterface.ifr_ifrn.ifrn_name, p_interface, IFNAMSIZ);
	if (setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE,
			(char *)&minterface, sizeof(minterface)) < 0) {
		return FALSE;
	}
	return TRUE;
}

/*
 * Caller should get the mutex before calling this function
 */
static inline void
NM_close_socket(int fd)
{
	network_mgr_t * pnm;

	if(fd == -1) {
		return;
	}

	NM_EVENT_TRACE(fd, FT_NM_CLOSE_SOCKET);
	pnm = &gnm[fd];
	if( NM_CHECK_FLAG(pnm, NMF_IN_USE) ) {

		// Cleanup all queues
		NKN_MUTEX_LOCKSTAT(&pnm->pthr->nm_mutex, &nm_lockstat[pnm->pthr->num]);
		if (NM_CHECK_FLAG(pnm, NMF_IN_TIMEOUTQ)) {
			LIST_REMOVE(pnm, toq_entries);
		}
		if(NM_CHECK_FLAG(pnm, NMF_IN_SBQ) ) {
			LIST_REMOVE(pnm, sbq_entries);
		}
		NKN_MUTEX_UNLOCKSTAT(&pnm->pthr->nm_mutex);

		DBG_LOG(MSG, MOD_NETWORK, "close(fd=%d)", fd);
		if( NM_CHECK_FLAG(pnm, NMF_USE_LICENSE) ) {
			AO_fetch_and_sub1(&pnm->pthr->cur_fds);
			AO_fetch_and_sub1(&glob_license_used);
		}
	        if (NM_CHECK_FLAG(pnm, NMF_IS_IPV6)) {
			AO_fetch_and_sub1(&glob_socket_cur_open_ipv6);
		} else {
			AO_fetch_and_sub1(&glob_socket_cur_open);
		}

	}
	else {
		// Do not close if the socket is already marked not
		// in use.
		// TBD.  Why do we call the close again?
		DBG_LOG(MSG, MOD_NETWORK, "!NMF_IN_USE fd=%d", fd);
		return;
	}
	//NM_del_event_epoll(fd);
        pnm->fd   = -1;
	pnm->incarn++;
	//pnm->flag = 0;	// Unset all flags
	pnm->flag = pnm->flag & NMF_HAVE_LOCK;
	pnm->private_data = NULL;
	if(net_use_nkn_stack) {
		so_shutdown(fd, 2);
	} else { /*shutdown(fd, SHUT_WR);*/
		if (close_socket_by_RST_enable) {
			/* Abortive close logic */
			int pending_bytes;
			int ret;
			struct linger L;
			L.l_onoff = 1;
			L.l_linger = 0;
			setsockopt(fd, SOL_SOCKET, SO_LINGER, (char *) &L, sizeof(L));
			ret = NM_get_tcp_sendq_pending_bytes(fd, &pending_bytes);
			if (ret == 0 && pending_bytes > 0) {
				glob_err_unsent_data_rst_close++;
				DBG_LOG(MSG, MOD_NETWORK, 
					"unsent bytes %d in fd=%d before rst", pending_bytes, fd);
			}
		}
		nkn_close_fd(fd, NETWORK_FD);
	}
}

/*
 * Caller should get the mutex before calling this function
 *
 * unregister_NM_socket is the same as NM_close_socket
 * except not closing the socket.
 *
 * field remove_fd_from_epoll is for code optimization.
 * when FALSE, does not remove fd from epoll loop and 
 * keep this pnm NMF_IN_USE flag bit set.
 */
static inline int
unregister_NM_socket(int fd, int remove_fd_from_epoll)
{
        network_mgr_t * pnm;

        if(fd == -1) {
                return FALSE;
        }

        assert(fd>=0 && fd<MAX_GNM);
	NM_EVENT_TRACE(fd, FT_UNREGISTER_NM_SOCKET);
	/* Code optimization, we do not need to remove fd from epoll loop */
	if (remove_fd_from_epoll) {
		NM_del_event_epoll(fd);
	}
        pnm = &gnm[fd];
	DBG_LOG(MSG, MOD_NETWORK, "fd=%d (%d), type=%d", fd, gnm[fd].fd, gnm[fd].fd_type);
        if( NM_CHECK_FLAG(pnm, NMF_IN_USE) ) {

		// Cleanup all queues
		NKN_MUTEX_LOCKSTAT(&pnm->pthr->nm_mutex, &nm_lockstat[pnm->pthr->num]);
                if (NM_CHECK_FLAG(pnm, NMF_IN_TIMEOUTQ)) {
                        LIST_REMOVE(pnm, toq_entries);
                }
		if(NM_CHECK_FLAG(pnm, NMF_IN_SBQ) ) {
			LIST_REMOVE(pnm, sbq_entries);
		}
		NKN_MUTEX_UNLOCKSTAT(&pnm->pthr->nm_mutex);

		// close(fd)
		if( NM_CHECK_FLAG(pnm, NMF_USE_LICENSE) ) {
			AO_fetch_and_sub1(&pnm->pthr->cur_fds);
			AO_fetch_and_sub1(&glob_license_used);
			NM_UNSET_FLAG(pnm, NMF_USE_LICENSE);
		}
                if (NM_CHECK_FLAG(pnm, NMF_IS_IPV6)) {
			AO_fetch_and_sub1(&glob_socket_cur_open_ipv6);
                } else {
			AO_fetch_and_sub1(&glob_socket_cur_open);
                }

        }
        pnm->fd = -1;
	if (remove_fd_from_epoll) {
		//pnm->flag         = 0;
		pnm->flag	  = pnm->flag & (NMF_HAVE_LOCK | NMF_IS_IPV6);
		pnm->f_epollin    = NULL;
		pnm->f_epollout   = NULL;
		pnm->f_epollerr   = NULL;
		pnm->f_epollhup   = NULL;
		pnm->f_timeout    = NULL;
		pnm->private_data = NULL;	// clear private data also 
	}
	else {
		// preserve these flag bits only
		pnm->flag &= (NMF_HAVE_LOCK|NMF_IN_USE|NMF_IN_EPOLL|NMF_IN_EPOLLIN|NMF_IN_EPOLLOUT|NMF_IS_IPV6);
		NM_SET_FLAG(pnm, NMF_NO_FD_UNEPOLL);
	}

	return TRUE;
}

/*
 * Caller should get the mutex before calling this function
 *
 */
static inline int
suspend_NM_socket(int fd)
{
        network_mgr_t * pnm;
	uint64_t flag;

        if(fd == -1) {
                return FALSE;
        }

        assert(fd>=0 && fd<MAX_GNM);

	NM_EVENT_TRACE(fd, FT_SUSPEND_NM_SOCKET);
        pnm = &gnm[fd];
	DBG_LOG(MSG, MOD_NETWORK, "fd=%d (%d), type=%d", fd, gnm[fd].fd, gnm[fd].fd_type);
	if (NM_CHECK_FLAG(pnm, NMF_SUSPENDED)) {
		return TRUE;
	}

        if( NM_CHECK_FLAG(pnm, NMF_IN_USE) ) {
		flag = pnm->flag & (NMF_HAVE_LOCK|NMF_IN_USE|NMF_IN_EPOLLIN|NMF_IN_EPOLLOUT|NMF_USE_LICENSE|NMF_IS_IPV6);

		NM_del_event_epoll(fd);

		// Cleanup all queues
		NKN_MUTEX_LOCKSTAT(&pnm->pthr->nm_mutex, &nm_lockstat[pnm->pthr->num]);
                if (NM_CHECK_FLAG(pnm, NMF_IN_TIMEOUTQ)) {
                        LIST_REMOVE(pnm, toq_entries);
                }
		if(NM_CHECK_FLAG(pnm, NMF_IN_SBQ) ) {
			LIST_REMOVE(pnm, sbq_entries);
		}
		NKN_MUTEX_UNLOCKSTAT(&pnm->pthr->nm_mutex);

		pnm->flag = flag;
		NM_SET_FLAG(pnm, NMF_SUSPENDED);
        }

	return TRUE;
}

/*
 * Caller should get the mutex before calling this function
 *
 */
static inline int
resume_NM_socket(int fd)
{
        network_mgr_t * pnm;

        if(fd == -1) {
                return FALSE;
        }

        assert(fd>=0 && fd<MAX_GNM);
	NM_EVENT_TRACE(fd, FT_RESUME_NM_SOCKET);
        pnm = &gnm[fd];
	if (!NM_CHECK_FLAG(pnm, NMF_SUSPENDED)) {
		return TRUE;
	}
	if(NM_CHECK_FLAG(pnm, NMF_IN_EPOLLIN)) {
		/* Clear NMF_IN_EPOLLIN, so that NM_add_event_epollin will
		 * actually add to fd to epoll
		 */
		NM_UNSET_FLAG(&gnm[fd], NMF_IN_EPOLLIN);
		NM_add_event_epollin(fd);
	}
	else if(NM_CHECK_FLAG(pnm, NMF_IN_EPOLLOUT)) {
		/* Clear NMF_IN_EPOLLOUT, so that NM_add_event_epollout will
		 * actually add to fd to epoll
		 */
		NM_UNSET_FLAG(&gnm[fd], NMF_IN_EPOLLOUT);
		NM_add_event_epollout(fd);
	}

	DBG_LOG(MSG, MOD_NETWORK, "fd=%d (%d), type=%d", fd, gnm[fd].fd, gnm[fd].fd_type);

	NM_set_socket_active(pnm);
	NM_UNSET_FLAG(pnm, NMF_SUSPENDED);

	return TRUE;
}

/*
 * timer functions
 * timer function will be called once every 5 seconds
 */

static inline void
NM_timer(void)
{
	network_mgr_t * pnm;
	struct nm_threads * pnmthr;
	int i;
	double timediff;
	int timeout_toq;
	uint32_t incarn;

	for(i=0; i<NM_tot_threads; i++)
	{
	    pnmthr=&g_NM_thrs[i];

	    /*
	     * pnmthr->nm_mutex is used to protect toq_head queue.
	     * however f_timeout may try to get this mutex lock.
	     * to avoid this deadlock, NM_timer needs to release this mutex.
	     */
	    NKN_MUTEX_LOCKSTAT(&pnmthr->nm_mutex, &nm_lockstat[i]);

	    /* Timeout_toq is to be removed */
	    timeout_toq = (pnmthr->cur_toq + 1 ) % MAX_TOQ_SLOT;

	    // This NM should be timed out
	    while( !LIST_EMPTY(&pnmthr->toq_head[timeout_toq]) ) {
		pnm = LIST_FIRST(&pnmthr->toq_head[timeout_toq] );
		LIST_FIRST(&pnmthr->toq_head[timeout_toq] ) = LIST_NEXT(pnm, toq_entries);
		LIST_REMOVE(pnm, toq_entries);
		NM_UNSET_FLAG(pnm, NMF_IN_TIMEOUTQ);
		incarn = pnm->incarn;	// Store incarn
		NKN_MUTEX_UNLOCKSTAT(&pnmthr->nm_mutex);

		/*
		 * Special case:
		 * After getting this mutex lock, this pnm may move to another timeout queue.
		 * then we should not call timeout() 
		 */
		pthread_mutex_lock(&pnm->mutex);
		NM_TRACE_LOCK(pnm, LT_NETWORK);

		/* While waiting for lock, socket could be closed and fd removed
		 * from timer Q. Check if fd is still in timer Q. Also check if the
		 * incarns match.
		 * BZ 7296
		 */
		if( NM_CHECK_FLAG(pnm, NMF_IN_USE) && (incarn != pnm->incarn) ) {
			glob_err_bad_timer_entry_3++;
			NM_TRACE_UNLOCK(pnm, LT_NETWORK);
			pthread_mutex_unlock(&pnm->mutex);

			NKN_MUTEX_LOCKSTAT(&pnmthr->nm_mutex, &nm_lockstat[i]);
			continue;
		}
#if 0
		if (!NM_CHECK_FLAG(pnm, NMF_IN_TIMEOUTQ)) {
			glob_err_bad_timer_entry_2++;
			/* if the same pnm remains in timeout Q with this flag delete it from
			 * from Q
			 */
			if (NM_CHECK_FLAG(pnm, NMF_TIMEOUTQ_LOOP)) {
				NM_UNSET_FLAG(pnm, NMF_TIMEOUTQ_LOOP);
		        /*bug 8177*/
       			if (pnm->f_timeout == NULL) {
                		glob_err_bad_timer_entry_4++;
                		DBG_LOG(ERROR, MOD_NETWORK, "pnm timer entry fd=%d, timeout flag not set", pnm->fd);
                		//NKN_MUTEX_LOCKSTAT(&pnm->pthr->nm_mutex, &nm_lockstat[i]);
                		//LIST_REMOVE(pnm, toq_entries);
				//NKN_MUTEX_UNLOCKSTAT(&pnm->pthr->nm_mutex);
				NM_TRACE_UNLOCK(pnm, LT_NETWORK);
				pthread_mutex_unlock(&pnm->mutex);
				NKN_MUTEX_LOCKSTAT(&pnmthr->nm_mutex, &nm_lockstat[i]);
				continue;
			}
			else {
				NM_SET_FLAG(pnm, NMF_TIMEOUTQ_LOOP);
				NM_TRACE_UNLOCK(pnm, LT_NETWORK);
				pthread_mutex_unlock(&pnm->mutex);

				NKN_MUTEX_LOCKSTAT(&pnmthr->nm_mutex, &nm_lockstat[i]);
				continue;
			}
		}
               
#endif
		/* 
		 * BUG 4704: TBD: I have no idea why (pnm->f_timeout==NULL) is TRUE
		 * Add protection code to avoid crash.
		 */
		if( NM_CHECK_FLAG(pnm, NMF_IN_USE)) {
			if(pnm->f_timeout == NULL) {
				glob_err_timeout_f_timeout_NULL++;
				DBG_LOG(ERROR, MOD_NETWORK, "(f_timeout==NULL) for fd=%d", pnm->fd);
			}
			else {
			timediff = difftime(nkn_cur_ts, pnm->last_active);
			if(timediff>=4/*http_idle_timeout*/) {
				if(pnm->f_timeout(pnm->fd, pnm->private_data, timediff) == TRUE) {
					glob_socket_tot_timeout++;
					NM_close_socket(pnm->fd);
				}
				else {
					/*
					 * Application does not want to timeout socket.
					 */
					NM_set_socket_active(pnm);
				}
			}
			else {
				/*
				 * This issue should be solved in bug 2369
				 * TBD: Here could be memory leak
				 *      first of all, don't understand why this happens.
				 *      with this checkin, this bug should be fixed.
				 *      I still leave here for debug purpose only.
				 */
				glob_err_bad_timer_entry++;
				DBG_LOG(ERROR, MOD_NETWORK, "Bad timer entry fd=%d", pnm->fd);
				//NKN_MUTEX_LOCKSTAT(&pnm->pthr->nm_mutex, &nm_lockstat[i]);
				//LIST_REMOVE(pnm, toq_entries);
				//NM_UNSET_FLAG(pnm, NMF_IN_TIMEOUTQ);
				//NKN_MUTEX_UNLOCKSTAT(&pnm->pthr->nm_mutex);
			}
			}
		} else {
			/* Bug 7296 : If gnm is free'd by some thread , but not yet
			 * removed from toq (why ? bug), then timer thread may end up in infinite
			 * while loop, since it may be accessing wrong memory location
			 * (already free'd one) . So adding this else part to handle such cases
			 * If pnm flag is not valid , remove the entry from toq to avoid infinite loop
			 */
				glob_err_bad_timer_entry_1++;
				DBG_LOG(ERROR, MOD_NETWORK, "Invalid gnm ,Bad timer entry fd=%d", pnm->fd);
				//NKN_MUTEX_LOCKSTAT(&pnm->pthr->nm_mutex, &nm_lockstat[i]);
				//LIST_REMOVE(pnm, toq_entries);
				//NM_UNSET_FLAG(pnm, NMF_IN_TIMEOUTQ);
				//NKN_MUTEX_UNLOCKSTAT(&pnm->pthr->nm_mutex);

		}
		NM_TRACE_UNLOCK(pnm, LT_NETWORK);
		pthread_mutex_unlock(&pnm->mutex);

		NKN_MUTEX_LOCKSTAT(&pnmthr->nm_mutex, &nm_lockstat[i]);
	     }

	     /* 
	      * change to next slot
	      * switch to next Time out queue for next adding.
	      */
	     pnmthr->cur_toq = timeout_toq;
	     NKN_MUTEX_UNLOCKSTAT(&pnmthr->nm_mutex);
	}

	return;
}

#undef F_FILE_ID

/*
 * Before calling any following functions, caller should get the gnm.mutex locker first
 */
void NM_change_socket_timeout_time (network_mgr_t * pnm, int next_timeout_slot); // one slot = 5seconds
net_fd_handle_t NM_fd_to_fhandle(int fd);

/*
 * Mutex is managed within these functions.
 * Caller has no need to acquire the mutex.
 */
void NM_init(void);
void NM_main(void);


#endif // __NETWORK__H

