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
#include <limits.h>
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
#include "ssl_network.h"

NKNCNT_DEF(err_wrong_fd_in_epoll, uint64_t, "", "error fd in epoll")
NKNCNT_DEF(license_used, uint64_t, "", "Total license used")
NKNCNT_DEF(socket_accumulate_tot, uint64_t, "", "Total accepted sockets")
NKNCNT_DEF(socket_err_epoll, uint64_t, "", "Total sockets closed due to epoll error")
NKNCNT_DEF(socket_hup_epoll, uint32_t, "", "Total sockets closed due to epoll error")
NKNCNT_DEF(cur_open_all_sockets, uint64_t, "", "Total currently open sockets")
NKNCNT_DEF(socket_tot_timeout, uint64_t, "", "Total Timeout")
NKNCNT_DEF(socket_tot_imm_timeout, uint64_t, "", "Total Immediate Timeout")
NKNCNT_DEF(socket_in_epoll, uint64_t, "", "Total sockets in epoll")
NKNCNT_DEF(register_bad_fd, uint64_t, "", "Total sockets cannot be registered in NM")
NKNCNT_DEF(run_out_of_license, uint64_t, "", "Run out of license")
NKNCNT_DEF(err_epoll_add, uint64_t, "", "Total failure in epoll_add")
NKNCNT_DEF(err_epoll_del, uint64_t, "", "Total failure in epoll_del")
NKNCNT_DEF(err_epoll_exist, uint64_t, "", "Total failure in epoll_ctl returns EXIST")
NKNCNT_DEF(warn_register_in_use, uint32_t, "", "num of register err when flag is still set");
NKNCNT_DEF(err_timeout_f_timeout_NULL, uint32_t, "", "BUG 4704");
NKNCNT_DEF(err_unsent_data_rst_close, uint32_t, "", "num of rst close when unsent data is in socket");
NKNCNT_DEF(err_bad_timer_entry, uint64_t, "", "num of bad entries inserted in timer slots, invalid incarn case");
NKNCNT_DEF(err_bad_timer_entry_1, uint64_t, "", "num of bad entries inserted in timer slots");
NKNCNT_DEF(err_bad_timer_entry_2, uint64_t, "", "num of bad entries inserted in timer slots, invalid gnm case");

int NM_tot_threads = 2;
int debug_fd_trace = 1;
int nkn_max_client = MAX_GNM;
network_mgr_t *gnm = NULL;	// global network manager array
static nkn_mutex_t fd_type_mutex;
static pthread_mutex_t network_mutex = PTHREAD_MUTEX_INITIALIZER;

struct nm_threads g_NM_thrs[MAX_EPOLL_THREADS];
nkn_lockstat_t nm_lockstat[MAX_EPOLL_THREADS];

extern uint16_t glob_tot_svr_sockets;

extern volatile sig_atomic_t srv_shutdown;
extern void check_out_hmoq(int num_of_epoll);
extern void check_out_cp_queue(int num_of_epoll);
extern void check_out_fp_queue(int num_of_epoll);
extern void check_out_tmrq(int num_of_epoll);
extern int nkn_choose_cpu(int cpu);
extern void nkn_setup_interface_parameter(int i);
extern int (* so_shutdown)(int s, int how);
AO_t next_epthr = 0;
// 200 was added in nkn_check_cfg() call.
#define SET_NM_THR_MAX_FDS(x) \
        (x)->max_fds = (nkn_max_client-200)/NM_tot_threads; \
        if ((x)->max_fds<100) (x)->max_fds=100; \

/*
 * init functions
 */
void NM_init()
{
	int i, j;
	struct nm_threads *pnmthr;
	char mutex_stat[15];

        /*
         * initialization
         */
	NKN_MUTEX_INIT(&fd_type_mutex, NULL, "fd-type-mutex");

        gnm = (network_mgr_t *)calloc(MAX_GNM, sizeof(network_mgr_t));
        if (gnm == NULL) {
		DBG_LOG(SEVERE, MOD_NETWORK, "malloc failure%s", "");
		DBG_ERR(SEVERE, "malloc failure%s", "");
		exit(1);
        }
	for (i = 0; i < MAX_GNM; i++) {
		gnm[i].fd = -1;		// initialize to -1
		gnm[i].incarn = 1;	// initialize to 1
	}

	/*
	 * Create epoll threads for running epoll ctl.
	 */
	for (i = 0; i < NM_tot_threads; i++) {
		// Set to zero
		memset((char *)&g_NM_thrs[i], 0, sizeof(struct nm_threads));

		// Initialize each member
		pnmthr = &g_NM_thrs[i];
		pnmthr->num = i;
		SET_NM_THR_MAX_FDS(pnmthr);

		// Each active fd will be in time out list.
		// Timeout is separated into groups.
		// If timed out, the whole group will time out.
		for (j = 0; j < MAX_TOQ_SLOT; j++) {
			LIST_INIT(&pnmthr->toq_head[j]);
		}

		/*
		 * big waste of memory. Each thread events holds 64K size.
		 * this design is for re-init the convenience of nkn_max_client.
		 */
		pnmthr->events = calloc(MAX_GNM, sizeof(struct epoll_event));
		assert(pnmthr->events != NULL);

		pnmthr->epfd = epoll_create(pnmthr->max_fds);
		assert( pnmthr->epfd != -1);

		// Initialize mutex
		snprintf(mutex_stat, sizeof(mutex_stat), "nm-mutex-%d", i);
		NKN_LOCKSTAT_INIT(&nm_lockstat[i], mutex_stat);
		pthread_mutex_init(&pnmthr->nm_mutex, NULL);
		snprintf(mutex_stat, sizeof(mutex_stat), "epoll-mutex-%d", i);
	}

	return;
}

/*
 * timer functions
 * timer function will be called once every 5 seconds
 */

void NM_timer(void)
{
	network_mgr_t *pnm;
	struct nm_threads *pnmthr;
	int i;
	int ret;
	unsigned int incarn;
	double timediff;
	int timeout_toq;

	for (i = 0; i < NM_tot_threads; i++) {
		pnmthr = &g_NM_thrs[i];

		/*
		* pnmthr->nm_mutex is used to protect toq_head queue.
		* however f_timeout may try to get this mutex lock.
		* to avoid this deadlock, NM_timer needs to release this mutex.
		*/
		NKN_MUTEX_LOCKSTAT(&pnmthr->nm_mutex, &nm_lockstat[i]);

		/* Timeout_toq is to be removed */
		timeout_toq = (pnmthr->cur_toq + 1 ) % MAX_TOQ_SLOT;

		// This NM should be timed out
		while (!LIST_EMPTY(&pnmthr->toq_head[timeout_toq])) {
			pnm = LIST_FIRST(&pnmthr->toq_head[timeout_toq]);
			LIST_FIRST(&pnmthr->toq_head[timeout_toq]) =
						LIST_NEXT(pnm, toq_entries);
			LIST_REMOVE(pnm, toq_entries);
			NM_UNSET_FLAG(pnm, NMF_IN_TIMEOUTQ);
			incarn = pnm->incarn;
			NKN_MUTEX_UNLOCKSTAT(&pnmthr->nm_mutex);

			/*
			* Special case:
			* After getting this mutex lock, this pnm may move to
			* another timeout queue. then we should not call timeout()
			*/
			pthread_mutex_lock(&pnm->mutex);

			if (incarn != pnm->incarn) {
				glob_err_bad_timer_entry++;
				pthread_mutex_unlock(&pnm->mutex);
				NKN_MUTEX_LOCKSTAT(&pnmthr->nm_mutex, &nm_lockstat[i]);
				continue;
			}
			if (NM_CHECK_FLAG(pnm, NMF_IN_USE)) {
				if (pnm->f_timeout == NULL) {
					glob_err_timeout_f_timeout_NULL++;
					DBG_LOG(ERROR, MOD_NETWORK, "(f_timeout==NULL) for fd=%d", pnm->fd);
					pthread_mutex_unlock(&pnm->mutex);
					NKN_MUTEX_LOCKSTAT(&pnmthr->nm_mutex, &nm_lockstat[i]);
					continue;
				}
				timediff = difftime(nkn_cur_ts, pnm->last_active);
				if (timediff >= 4/*ssl_idle_timeout*/) {
					ret = pnm->f_timeout(pnm->fd, pnm->private_data, timediff);
					if (ret == TRUE) {
						glob_socket_tot_timeout++;
					} else {
						/* Application does not want to timeout socket. */
						NM_set_socket_active(pnm);
					}
				} else {
					glob_err_bad_timer_entry_1++;
					DBG_LOG(ERROR, MOD_NETWORK, "Bad timer entry fd=%d", pnm->fd);
				}
			} else {
				glob_err_bad_timer_entry_2++;
				DBG_LOG(ERROR, MOD_NETWORK, "Invalid gnm ,Bad timer entry fd=%d", pnm->fd);
			}

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

/*
 * Strictly speaking:
 *     This function returns FALSE only when we run out of license.
 *     The only callers who require a new license are
 *     init_conn() for HTTP and init_rtsp_conn() for RTSP.
 */
int register_NM_socket(int fd,
	void *private_data,
	NM_func f_epollin,
	NM_func f_epollout,
	NM_func f_epollerr,
	NM_func f_epollhup,
	NM_func_timeout f_timeout,
	int timeout, // in seconds, timeout func callbacked at [timeout, timeout+5) seconds
	int thr_num // Pick up a network thread
	)
{
	network_mgr_t *pnm;	// global network manager array

	if ((fd < 0) || (fd >= MAX_GNM)) {
		glob_register_bad_fd++;
		return FALSE;
	}

	pnm = &gnm[fd];
	DBG_LOG(MSG, MOD_NETWORK, "fd=%d (%d)", fd, gnm[fd].fd);

	if (NM_CHECK_FLAG(pnm, NMF_IN_USE)) {
		/*
		 * TBD: BUG 1074:
		 * If (fd == -1 and private_data==NULL) we only reset flag.
		 * why pnm->flag is unset is unknown.
		 */
		glob_warn_register_in_use++;
		if ((pnm->fd == -1) && (pnm->private_data == NULL)) { pnm->flag = 0; }
	}

	pnm->fd = fd;
	pnm->private_data = private_data;
	pnm->f_epollin = f_epollin;
	pnm->f_epollout = f_epollout;
	pnm->f_epollerr = f_epollerr;
	pnm->f_epollhup = f_epollhup;
	pnm->f_timeout = f_timeout;
	timeout = (timeout / 2) + 1; 	// timeout callback at [timeout, timeout+2] seconds
	if(timeout < 3) timeout = 3;	// min timeout is 6 seconds.
	if(timeout > MAX_TOQ_SLOT) timeout = MAX_TOQ_SLOT-2;	// -2 to avoid insert the same slot of itself
	pnm->timeout_slot = timeout; // timeout_slot is [3, MAX_TOQ_SLOT]
	NM_SET_FLAG(pnm, NMF_IN_USE);
	if (thr_num == -1) {
		pnm->accepted_thr_num = (AO_fetch_and_add1(&next_epthr))%NM_tot_threads;
	}
	else {
		pnm->accepted_thr_num = thr_num;
	}
	pnm->pthr = &g_NM_thrs[pnm->accepted_thr_num];

        // To make sure the connection link list is integrated one.
        //printf("init_conn: LIST_INSERT_HEAD( con, caq_entries)\n");
	NM_set_socket_active(pnm);

        AO_fetch_and_add1(&glob_socket_accumulate_tot);
        AO_fetch_and_add1(&glob_cur_open_all_sockets);

	return TRUE;
}

/*
 * epoll_in/out/err operation function
 */
static inline int NM_add_fd_to_event(int fd, uint32_t epollev)
{
	struct epoll_event ev;
	int op;

	op = (NM_CHECK_FLAG(&gnm[fd], NMF_IN_EPOLL)) ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;

	/* initialized to make valgrind happy only */
	memset((char *)&ev, 0, sizeof(struct epoll_event));

	ev.events = epollev;
	ev.data.fd = fd;

	if (epoll_ctl(gnm[fd].pthr->epfd, op, fd, &ev) < 0) {
		if(errno == EEXIST) {
			glob_err_epoll_exist++;
			return TRUE;
		}
		DBG_LOG(SEVERE, MOD_NETWORK, "Failed to add fd (%d), errno=%d", fd, errno);
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
	if (!NM_CHECK_FLAG(&gnm[fd], NMF_IN_EPOLL)) {
		AO_fetch_and_add1(&glob_socket_in_epoll);
		NM_SET_FLAG(&gnm[fd], NMF_IN_EPOLL);
	}

	return TRUE;
}

int NM_add_event_epollin(int fd)
{
	if (NM_CHECK_FLAG(&gnm[fd], NMF_IN_EPOLLIN)) {
		return TRUE;
	}

	if (NM_add_fd_to_event(fd, EPOLLIN) == FALSE) {
		return FALSE;
	}

	NM_SET_FLAG(&gnm[fd], NMF_IN_EPOLLIN);
	NM_UNSET_FLAG(&gnm[fd], NMF_IN_EPOLLOUT);

	return TRUE;
}

int NM_add_event_epollout(int fd)
{
	if (NM_CHECK_FLAG(&gnm[fd], NMF_IN_EPOLLOUT)) {
		return TRUE;
	}

	if (NM_add_fd_to_event(fd, EPOLLOUT) == FALSE) {
		return FALSE;
	}

	NM_UNSET_FLAG(&gnm[fd], NMF_IN_EPOLLIN);
	NM_SET_FLAG(&gnm[fd], NMF_IN_EPOLLOUT);

	return TRUE;
}

int NM_del_event_epoll_inout(int fd)
{
	if (NM_add_fd_to_event(fd, EPOLLERR | EPOLLHUP) == FALSE) {
		return FALSE;
	}

	NM_UNSET_FLAG(&gnm[fd], NMF_IN_EPOLLIN|NMF_IN_EPOLLOUT);

	return TRUE;
}

int NM_del_event_epoll(int fd)
{
	if (!NM_CHECK_FLAG(&gnm[fd], NMF_IN_EPOLL)) {
		return TRUE;
	}

	if (epoll_ctl(gnm[fd].pthr->epfd, EPOLL_CTL_DEL, fd, NULL) < 0) {
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

int NM_set_socket_nonblock(int fd)
{
        int opts;
	int on = 1;

        opts = O_RDWR; //fcntl(fd, F_GETFL);
        if (opts < 0) {
		return FALSE;
        }

        opts = (opts | O_NONBLOCK);
        if (fcntl(fd, F_SETFL, opts) < 0) {
		return FALSE;
        }

	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &on, sizeof(on));

	return TRUE;
}
int NM_get_tcp_info(int fd, void *tcp_con_info)
{
        int tcp_info_size = sizeof(struct tcp_info);

        return getsockopt(fd, SOL_TCP, TCP_INFO,
                        (void *)tcp_con_info, (socklen_t *)&tcp_info_size);

}

int NM_get_tcp_sendq_pending_bytes(int fd, void *pending_bytes)
{
        return ioctl(fd, SIOCOUTQ, pending_bytes);
}

int NM_get_tcp_recvq_pending_bytes(int fd, void *pending_bytes)
{
        return ioctl(fd, SIOCINQ, pending_bytes);
}


/*
 * ***  IMPORTANT NOTES  ****:
 * This function requires ROOT account priviledge.
 */
int NM_bind_socket_to_interface(int fd, char *p_interface)
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
void NM_close_socket(int fd)
{
	network_mgr_t *pnm;

	if (fd == -1) {
		return;
	}

	pnm = &gnm[fd];
	if (NM_CHECK_FLAG(pnm, NMF_IN_USE)) {

		// Cleanup all queues
		NKN_MUTEX_LOCKSTAT(&pnm->pthr->nm_mutex, &nm_lockstat[pnm->pthr->num]);
		if (NM_CHECK_FLAG(pnm, NMF_IN_TIMEOUTQ)) {
			LIST_REMOVE(pnm, toq_entries);
		}
		NKN_MUTEX_UNLOCKSTAT(&pnm->pthr->nm_mutex);

		DBG_LOG(MSG, MOD_NETWORK, "close(fd=%d)", fd);
        	AO_fetch_and_sub1(&glob_cur_open_all_sockets);
	} else {
		// Do not close if the socket is already marked not
		// in use.
		// TBD.  Why do we call the close again?
		DBG_LOG(MSG, MOD_NETWORK, "!NMF_IN_USE fd=%d", fd);
		return;
	}
	NM_del_event_epoll(fd);
        pnm->fd = -1;
	pnm->incarn++;
	pnm->flag = 0;	// Unset all flags
	pnm->private_data = NULL;
	pnm->accepted_thr_num = -1;
	close(fd);

	return;
}

/*
 * Caller should get the mutex before calling this function
 *
 * unregister_NM_socket is the same as NM_close_socket
 * except not closing the socket.
 *
 */
int unregister_NM_socket(int fd)
{
        network_mgr_t *pnm;

        if (fd == -1) {
                return FALSE;
        }

        assert(fd >= 0 && fd < MAX_GNM);
	/* Code optimization, we do not need to remove fd from epoll loop */
	NM_del_event_epoll(fd);
        pnm = &gnm[fd];
	DBG_LOG(MSG, MOD_NETWORK, "fd=%d (%d)", fd, gnm[fd].fd);
        if (NM_CHECK_FLAG(pnm, NMF_IN_USE)) {
		// Cleanup all queues
		NKN_MUTEX_LOCKSTAT(&pnm->pthr->nm_mutex, &nm_lockstat[pnm->pthr->num]);
                if (NM_CHECK_FLAG(pnm, NMF_IN_TIMEOUTQ)) {
                        LIST_REMOVE(pnm, toq_entries);
                }
		NKN_MUTEX_UNLOCKSTAT(&pnm->pthr->nm_mutex);

                AO_fetch_and_sub1(&glob_cur_open_all_sockets);
        }
        pnm->fd = -1;
	pnm->flag = 0;
	pnm->f_epollin = NULL;
	pnm->f_epollout = NULL;
	pnm->f_epollerr = NULL;
	pnm->f_epollhup = NULL;
	pnm->f_timeout = NULL;
	pnm->private_data = NULL; // clear private data also

	return TRUE;
}

void NM_set_socket_active(network_mgr_t *pnm)
{
	int slot;

	if (pnm->f_timeout == NULL) {
		return;
	}

	pnm->last_active = nkn_cur_ts;

	// Timeout Queue operation
	NKN_MUTEX_LOCKSTAT(&pnm->pthr->nm_mutex, &nm_lockstat[pnm->pthr->num]);
	if (NM_CHECK_FLAG(pnm, NMF_IN_TIMEOUTQ)) {
		LIST_REMOVE(pnm, toq_entries);
	}
	slot = (pnm->pthr->cur_toq + pnm->timeout_slot) % MAX_TOQ_SLOT;
	LIST_INSERT_HEAD(&(pnm->pthr->toq_head[slot]), pnm, toq_entries);
	NM_SET_FLAG(pnm, NMF_IN_TIMEOUTQ);
	NKN_MUTEX_UNLOCKSTAT(&pnm->pthr->nm_mutex);

	return;
}

/*
 * NM_main is an infinite loop. It never exits unless somehow hits Ctrl-C.
 */
static void * NM_main_loop(void *arg)
{
        int i, cpu;
        int res, ret;
	int fd;
	network_mgr_t *pnm;
	struct nm_threads *pnmthr  = (struct nm_threads *)arg;
	char name[64];
	unsigned int incarn;

	prctl(PR_SET_NAME, "ssld-network", 0, 0, 0);

	// cpu = nkn_choose_cpu( pnmthr->num );
	cpu = pnmthr->num;

	snprintf(name, 64, "net_thread%d_cpu%d_tot_sockets", pnmthr->num, cpu);
	nkn_mon_add(name, NULL, &pnmthr->cur_fds, sizeof(pnmthr->cur_fds));

	// An infinite loop
	while (1) {
		if (srv_shutdown == 1) break; // we are shutting down
		res =  epoll_wait(pnmthr->epfd, pnmthr->events, pnmthr->max_fds, 1000);

		if (res == -1) {
			// Can happen due to -pg
			if (errno == EINTR) continue;
			//printf("epoll_wait: res=%d errno=%d\n", res, errno);
			DBG_LOG(SEVERE, MOD_NETWORK,
				"errno=%d epfd=%d max_fds=%d",
				errno, pnmthr->epfd, pnmthr->max_fds);
			DBG_ERR(SEVERE,
                                "errno=%d epfd=%d max_fds=%d",
                                errno, pnmthr->epfd, pnmthr->max_fds);
			exit(10);
		} else if (res == 0) {
			//printf("No active fds, total #=%d\n", max_client);
			// Timeout of epoll_wait()
			continue;
		}

		for (i = 0; i < res; i++) {
			fd = pnmthr->events[i].data.fd;
			pnm = (network_mgr_t *)&gnm[fd];
			incarn = pnm->incarn;
			pthread_mutex_lock(&pnm->mutex);
			if (incarn != pnm->incarn) {
				pthread_mutex_unlock(&pnm->mutex);
				continue;
			}

			if(pnmthr->num != pnm->accepted_thr_num) {
				pthread_mutex_unlock(&pnm->mutex);
				continue;
			}
		
			if (!NM_CHECK_FLAG(pnm, NMF_IN_USE)) {
				/* TBD: We should assert to crash */
				DBG_LOG(MSG, MOD_NETWORK, "structure is in use%s", "");
				glob_err_wrong_fd_in_epoll++;
				NM_SET_FLAG(pnm, NMF_IN_EPOLL);
				NM_del_event_epoll(fd);
				NM_UNSET_FLAG(pnm, NMF_IN_EPOLL);
				pthread_mutex_unlock(&pnm->mutex);
				continue;
			}

			NM_set_socket_active(pnm);

			ret = TRUE;
			if (pnmthr->events[i].events & EPOLLERR) {
				glob_socket_err_epoll++;
				ret=pnm->f_epollerr(fd, pnm->private_data);
				if (ret==FALSE) {
                                        NM_close_socket(fd);
                                }
				pthread_mutex_unlock(&pnm->mutex);
                                continue;
                        }

			if (pnmthr->events[i].events & EPOLLIN) {
				ret=pnm->f_epollin(fd, pnm->private_data);
			} else if (pnmthr->events[i].events & EPOLLOUT) {
				ret=pnm->f_epollout(fd, pnm->private_data);
			} else if (pnmthr->events[i].events & EPOLLHUP) {
				glob_socket_hup_epoll++;
				ret=pnm->f_epollhup(fd, pnm->private_data);
			}


			// if returns false, we close the socket
			if (ret == FALSE) {
				NM_close_socket(fd);
			}

			pthread_mutex_unlock(&pnm->mutex);
		}
	} // while(1); infinite loop

	close(pnmthr->epfd);

	return NULL;
}

void NM_main(void)
{
	int64_t i;
	int rv;
	pthread_attr_t attr;
	int stacksize = 512 * KiBYTES; // 512MB for libketama

	rv = pthread_attr_init(&attr);
	if (rv) {
		DBG_LOG(SEVERE, MOD_NETWORK,
			"pthread_attr_init() failed, rv=%d", rv);
		DBG_ERR(SEVERE,
                        "pthread_attr_init() failed, rv=%d", rv);
		return;
	}
	rv = pthread_attr_setstacksize(&attr, stacksize);
	if (rv) {
		DBG_LOG(SEVERE, MOD_NETWORK,
			"pthread_attr_setstacksize() stacksize=%d failed, "
			"rv=%d", stacksize, rv);
		return;
	}

	for (i = 0; i < NM_tot_threads; i++) {
		pthread_create(&g_NM_thrs[i].pid, &attr, NM_main_loop, &g_NM_thrs[i]);
	}

	// never return until killed.
	pthread_join(g_NM_thrs[0].pid, NULL);

	return;
}
