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

#include "nkn_cfg_params.h"
#include "nkn_debug.h"
#include "server.h"
#include "network.h"
#include "nkn_stat.h"
#include "nkn_assert.h"
#include "nkn_lockstat.h"

#define F_FILE_ID	LT_NETWORK

NKNCNT_DEF(err_wrong_fd_in_epoll, uint64_t, "", "error fd in epoll")
NKNCNT_DEF(err_closed_fd_in_epoll, uint64_t, "", "closed fd in epoll")
NKNCNT_DEF(license_used, AO_t, "", "Total license used")
NKNCNT_DEF(socket_err_epoll, uint64_t, "", "Total sockets closed due to epoll error")
NKNCNT_DEF(socket_hup_epoll, uint32_t, "", "Total sockets closed due to epoll error")
NKNCNT_DEF(socket_accumulate_tot, AO_t, "", "Total accepted sockets")
NKNCNT_DEF(socket_accumulate_tot_ipv6, AO_t, "", "Total accepted sockets for ipv6")
NKNCNT_DEF(socket_cur_open, AO_t, "", "Total currently open sockets")
NKNCNT_DEF(socket_cur_open_ipv6, AO_t, "", "Total currently open sockets for ipv6")
NKNCNT_DEF(socket_tot_timeout, uint64_t, "", "Total Timeout")
NKNCNT_DEF(socket_tot_imm_timeout, uint64_t, "", "Total Immediate Timeout")
NKNCNT_DEF(socket_in_epoll, AO_t, "", "Total sockets in epoll")
NKNCNT_DEF(register_bad_fd, uint64_t, "", "Total sockets cannot be registered in NM")
NKNCNT_DEF(run_out_of_license, uint64_t, "", "Run out of license")
NKNCNT_DEF(err_epoll_add, uint64_t, "", "Total failure in epoll_add")
NKNCNT_DEF(err_epoll_del, uint64_t, "", "Total failure in epoll_del")
NKNCNT_DEF(err_epoll_exist, uint64_t, "", "Total failure in epoll_ctl returns EXIST")
NKNCNT_DEF(err_bad_timer_entry, uint64_t, "", "num of bad entries inserted in timer slots");
NKNCNT_DEF(warn_register_in_use, uint32_t, "", "num of register err when flag is still set");
NKNCNT_DEF(err_timeout_f_timeout_NULL, uint32_t, "", "BUG 4704");
NKNCNT_DEF(err_unsent_data_rst_close, uint32_t, "", "num of rst close when unsent data is in socket");
NKNCNT_DEF(err_bad_timer_entry_1, uint64_t, "", "num of bad entries inserted in timer slots, invalid gnm case");
NKNCNT_DEF(err_bad_timer_entry_2, uint64_t, "", "num of bad entries inserted in timer slots, not in Q case");
NKNCNT_DEF(err_bad_timer_entry_3, uint64_t, "", "num of bad entries inserted in timer slots, not in Q case");
NKNCNT_DEF(sbq_items_processed, uint64_t, "", "num of bad entries inserted in timer slots, not in Q case");
extern uint16_t glob_tot_svr_sockets;
extern uint16_t glob_tot_svr_sockets_ipv6;

int epoll_wait_timeout = 1; // 1 second
AO_t next_epthr = 0;
network_mgr_t * gnm = NULL;	// global network manager array
extern pthread_key_t namespace_key;
extern volatile sig_atomic_t srv_shutdown;
extern int check_out_hmoq(int num_of_epoll);
extern void check_out_cp_queue(int num_of_epoll);
extern void check_out_fp_queue(int num_of_epoll);
extern void check_out_tmrq(int num_of_epoll);
extern int nkn_choose_cpu(int cpu);
//extern void nkn_setup_interface_parameter(int i);
extern void nkn_setup_interface_parameter(nkn_interface_t *);
extern void check_out_pe_req_queue(int num_of_epoll);

//static pthread_mutex_t network_mutex = PTHREAD_MUTEX_INITIALIZER;

nkn_lockstat_t nm_lockstat[MAX_EPOLL_THREADS];
static nkn_mutex_t fd_type_mutex;
int debug_fd_trace = 1;
static uint64_t glob_socket_close_fd_without_lock;
/*
 * This global variable will specify how many epoll thread will run
 */
struct nm_threads g_NM_thrs[MAX_EPOLL_THREADS];
int NM_send_threads = 0;
int nm_hdl_send_and_receive = 1;

void dbg_dump_fds_in_epoll(void);
void dbg_dump_gnm(void);

		// 200 was added in nkn_check_cfg() call.
#define SET_NM_THR_MAX_FDS(x) \
	(x)->max_fds = (nkn_max_client-200)/NM_tot_threads; \
	(x)->max_fds *= 2; \
	if ((x)->max_fds<100) (x)->max_fds=100; \


/*
 * init functions
 */
void NM_init()
{
	int i, j;
	struct nm_threads * pnmthr;
	char mutex_stat[15];

        /*
         * initialization
         */
	NKN_MUTEX_INIT(&fd_type_mutex, NULL, "fd-type-mutex");

        gnm = (network_mgr_t *)nkn_calloc_type(MAX_GNM, sizeof(network_mgr_t),
						mod_nm_network_mgr_t);
        if (gnm == NULL) {
		DBG_LOG(SEVERE, MOD_NETWORK, "malloc failure%s", "");
		DBG_ERR(SEVERE, "malloc failure%s", "");
		exit(1);
        }
	for(i=0; i<MAX_GNM; i++)
	{
		gnm[i].fd = -1;	// initialize to -1
		gnm[i].incarn = 1;	// initialize to 1
                gnm[i].fd_type = 0;
	}

	/*
	 * Create epoll threads for running epoll ctl.
	 */
	for(i=0; i<NM_tot_threads; i++)
	{
		// Set to zero
		memset((char *)&g_NM_thrs[i], 0, sizeof(struct nm_threads));

		// Initialize each member

		pnmthr=&g_NM_thrs[i];

		pnmthr->num = i;

		// This line is not meeded because anyway 
		// PTHREAD_MUTEX_INITIALIZER is defined as
		// { { 0, 0, 0, 0, 0, 0, { 0, 0 } } }
		//pnmthr->nm_mutex = PTHREAD_MUTEX_INITIALIZER;

		SET_NM_THR_MAX_FDS(pnmthr);

		// Each active fd will be in time out list.
		// Timeout is separated into groups.
		// If timed out, the whole group will time out.
		for(j=0; j<MAX_TOQ_SLOT; j++) {
			LIST_INIT( &pnmthr->toq_head[j] );
		}
		LIST_INIT( &pnmthr->sbq_head[0] );
		LIST_INIT( &pnmthr->sbq_head[1] );
		pnmthr->cur_sbq = 0;


		/* 
		 * big waste of memory. Each thread events holds 64K size.
		 * this design is for re-init the convenience of nkn_max_client.
		 */
		pnmthr->events = nkn_calloc_type(MAX_GNM, sizeof(struct epoll_event),
						mod_nm_epoll_event_t);
		assert( pnmthr->events != NULL);

		pnmthr->epfd = epoll_create(pnmthr->max_fds);
		assert( pnmthr->epfd != -1);

		// Initialize mutex
		snprintf(mutex_stat, sizeof(mutex_stat), "nm-mutex-%d", i);
		NKN_LOCKSTAT_INIT(&nm_lockstat[i], mutex_stat);
		pthread_mutex_init(&pnmthr->nm_mutex, NULL);
		snprintf(mutex_stat, sizeof(mutex_stat), "epoll-mutex-%d", i);
	}
}

void NM_reinit_callback(void);
void NM_reinit_callback()
{
	int i;
	struct nm_threads * pnmthr;

	/* NM threads has not been initialized yet */
	if (gnm == NULL) return;

	/* 
	 *  NOTICE:
	 *    1. This function is introduced to change configuration
	 *       without restarting nvsd.
	 *    2. We only reinit nkn_max_client, sess_assured_flow_rate
	 *    3. This function does not re-init NM_tot_threads variable.
	 */
	for(i=0; i<NM_tot_threads; i++) {
		pnmthr=&g_NM_thrs[i];

	    	NKN_MUTEX_LOCKSTAT(&pnmthr->nm_mutex, &nm_lockstat[i]);

		/* When nkn_max_client is changed */
		SET_NM_THR_MAX_FDS(pnmthr);

	    	NKN_MUTEX_UNLOCKSTAT(&pnmthr->nm_mutex);
	}

	/*
	 * When sess_assured_flow_rate or nkn_max_client is changed 
	 */
	for(i=0; i<glob_tot_interface; i++)
	{
		//nkn_setup_interface_parameter(i);
		nkn_setup_interface_parameter(&interface[i]);
	}

}


/* Maximum entries to be processed in a single call to
 * NM_timer_for_sbq. This ensures, we are not spending
 * too much time in this function at a time.
 * The rest of the entries will be processed in the
 * subsequent iteration of the NM loop.
 */
#define SBQ_PROCESS_ITEMS_MAX 500

/*
 * This function will be called within network thread.
 * So we will not run into the timer runs out of time problem
 * and distribute the load into different CPU cores.
 */
static void NM_timer_for_sbq(struct nm_threads * pnmthr)
{
	network_mgr_t * pnm;
	con_t * httpcon;
	int cur_sbq, other_sbq;
	int sbq_processed = 0; /* In this call, process a maximum of 500 entries */
	int sbq_max_to_be_processed;


	NKN_MUTEX_LOCKSTAT(&pnmthr->nm_mutex, &nm_lockstat[pnmthr->num]);
	/*
	 * cleanup the SBQ if there are data in the queue.
	 */
	cur_sbq = (pnmthr->cur_sbq==0) ? 1 : 0; //pick up the other queue
	if( pnmthr->cleanup_sbq == 1 ) {
		// Because we need to swap the SBQ, let's clean up the current sbq now.
		sbq_max_to_be_processed = 0x7FFFFFFF;
	}
	else {
		// Don't block epollout event, let's only process up to 500 sockets
		// from SBQ
		sbq_max_to_be_processed = SBQ_PROCESS_ITEMS_MAX;
	}
	while( !LIST_EMPTY(&pnmthr->sbq_head[cur_sbq]) )
	{
		/*
		 * Get the first entry in the queue, and
		 * send data out or post a new scheduler task.
		 */
		pnm = LIST_FIRST(&pnmthr->sbq_head[cur_sbq]);
		NKN_MUTEX_UNLOCKSTAT(&pnmthr->nm_mutex);

		pthread_mutex_lock(&pnm->mutex);
		NM_TRACE_LOCK(pnm, LT_NETWORK);
		httpcon = (con_t *)(pnm->private_data);
		if( NM_CHECK_FLAG(pnm, NMF_IN_USE) ) {
			NKN_MUTEX_LOCKSTAT(&pnm->pthr->nm_mutex, &nm_lockstat[pnmthr->num]);
		    	if( NM_CHECK_FLAG(pnm, NMF_IN_SBQ) ) {
				LIST_REMOVE( pnm, sbq_entries);
				NM_UNSET_FLAG( pnm, NMF_IN_SBQ );
			}
			NKN_MUTEX_UNLOCKSTAT(&pnm->pthr->nm_mutex);
                        /*
                        * PR-769965
                        * The problem was that we were calling http_try_to_send_data(),
                        * and inside that we were also adding to sbq.
                        * The sbq could kick off the same second and send again
                        * Avoiding all this, by just adding to another sbq if this pnm
                        * was active the same second.
                        * We only send data once a second and dont burst during the start.
                        */
                        if (nkn_cur_ts != httpcon->nkn_cur_ts) {
                                http_try_to_sendout_data(httpcon);
                        } else {
                                other_sbq = (cur_sbq==0) ? 1 : 0;
                                NKN_MUTEX_LOCKSTAT(&pnm->pthr->nm_mutex, &nm_lockstat[pnmthr->num]);
                                LIST_INSERT_HEAD( &(pnm->pthr->sbq_head[other_sbq]), pnm, sbq_entries);
                                NM_SET_FLAG(pnm, NMF_IN_SBQ);
                                NKN_MUTEX_UNLOCKSTAT(&pnm->pthr->nm_mutex);
                        }
		}
		NM_TRACE_UNLOCK(pnm, LT_NETWORK);
		pthread_mutex_unlock(&pnm->mutex);

		NKN_MUTEX_LOCKSTAT(&pnmthr->nm_mutex, &nm_lockstat[pnmthr->num]);
		// Get the lock back for this while loop
		sbq_processed++ ;
		glob_sbq_items_processed ++ ;
		if(sbq_processed > sbq_max_to_be_processed) {
			break;
		}
	}
	if (pnmthr->cleanup_sbq) {
		pnmthr->cur_sbq = (pnmthr->cur_sbq==0) ? 1 : 0; //swap the sbq
		pnmthr->cleanup_sbq = 0;
	}
	NKN_MUTEX_UNLOCKSTAT(&pnmthr->nm_mutex);
}



net_fd_handle_t NM_fd_to_fhandle(int fd)
{
	net_fd_handle_t fdh;

	fdh.incarn = gnm[fd].incarn;
	fdh.fd = fd;
	return fdh;
}


void dbg_dump_fds_in_epoll(void)
{
	int i;
	for(i=0;i<MAX_GNM;i++) {
		if(NM_CHECK_FLAG(&gnm[i], NMF_IN_EPOLL))
			printf("%d\n", i);
	}
}


/* one timeout slot is 5 seconds 
 * Since there is a check of 10 seconds as the min http timeout,
 * the diff time if < 10 seconds will create issue. 
 * Now keeping the threshold to 10 prohibts the api to provide 
 * timeouts less than 10 seconds.
 * */
void NM_change_socket_timeout_time (network_mgr_t * pnm, int next_timeout_slot)
{
	int slot;

	if(pnm->f_timeout == NULL) {
		return;
	}

	pnm->last_active = nkn_cur_ts;
	
	// Timeout Queue operation
	NKN_MUTEX_LOCKSTAT(&pnm->pthr->nm_mutex, &nm_lockstat[pnm->pthr->num]);
	if (NM_CHECK_FLAG(pnm, NMF_IN_TIMEOUTQ)) {
		LIST_REMOVE(pnm, toq_entries);
	}
	next_timeout_slot = (next_timeout_slot > 5 ? next_timeout_slot : 5);
	slot = (pnm->pthr->cur_toq + next_timeout_slot) % MAX_TOQ_SLOT;
	LIST_INSERT_HEAD(&(pnm->pthr->toq_head[slot]), pnm, toq_entries);
	NM_SET_FLAG(pnm, NMF_IN_TIMEOUTQ);
	NKN_MUTEX_UNLOCKSTAT(&pnm->pthr->nm_mutex);
}

/*
 * The following two APIs are designed to mark a fd usage.
 *
 * nkn_mark_fd() should be called after fd is opened.
 * nkn_close_fd() should be called instead of close(fd) directly.
 *
 * If nkn_mark_fd() is called for a fd, nkn_close_fd() MUST be called to cleanup type.
 * otherwise next usage of same fd will be asserted.
 *
 * At nkn_close_fd() time, if type does not match, it will be asserted.
 */

void nkn_mark_fd(int fd, nkn_fd_type_t type)
{
	if (fd<0 || fd>=MAX_GNM) { assert(0); }
	DBG_LOG(MSG, MOD_NETWORK, "fd=%d (%d), type=%d (%d)", fd, gnm[fd].fd, type, gnm[fd].fd_type);
	NKN_MUTEX_LOCK(&fd_type_mutex);
	assert(gnm[fd].fd_type == 0);
	gnm[fd].fd_type = type;
	NKN_MUTEX_UNLOCK(&fd_type_mutex);
}

int nkn_close_fd(int fd, nkn_fd_type_t type)
{
	int ret;

	if (fd<0 || fd>=MAX_GNM) { assert(0); }
	if (type == NETWORK_FD) { 
		NM_EVENT_TRACE(fd, FT_NKN_CLOSE_FD); 
		if (NM_CHECK_FLAG(&gnm[fd], NMF_HAVE_LOCK) == 0) {
			//assert(0);
			glob_socket_close_fd_without_lock++;
		}
		NM_UNSET_FLAG(&gnm[fd], NMF_HAVE_LOCK)
	}
	DBG_LOG(MSG, MOD_NETWORK, "fd=%d (%d), type=%d (%d)", fd, gnm[fd].fd, type, gnm[fd].fd_type);
	NKN_MUTEX_LOCK(&fd_type_mutex);
	assert(gnm[fd].fd_type == type);
	gnm[fd].fd_type = 0;
	NKN_MUTEX_UNLOCK(&fd_type_mutex);
	ret = close(fd); // close happens inside this function
	NKN_ASSERT(ret == 0 || errno != EBADF);
	return(ret);
}


extern pthread_mutex_t hmoq_mutex[MAX_EPOLL_THREADS];
extern pthread_cond_t hmoq_cond[MAX_EPOLL_THREADS];
static void * NM_main_send_loop( void * arg );
static void * NM_main_send_loop( void * arg )
{
	char name[64];
	int64_t num = (int64_t)arg;
	int processed_entries = 0;
	prctl(PR_SET_NAME, "nvsd-network-send", 0, 0, 0);
	snprintf(name, 64, "nt%d_send_mempool", (int)num);
	nkn_mem_create_thread_pool((const char *)name);
        if (nm_hdl_send_and_receive) {
		int send_thread_mutex_num;
		int start_thread;
		int end_thread;
		int start_thread_bkup;

		start_thread = (NM_tot_threads/NM_send_threads)*(num);
		end_thread = (NM_tot_threads/NM_send_threads)*(num+1);
		send_thread_mutex_num = start_thread;
		start_thread_bkup = start_thread;

                for (start_thread = start_thread_bkup; start_thread < end_thread; start_thread++) {
                        g_NM_thrs[start_thread].send_thread_mutex_num = send_thread_mutex_num;
                }
                // An infinite loop
                while(1) {
                        /*
                         *  check out http_mgr_outpur_q to see if we have
                         *  any pending http_mgr_output queue.
                         *  We only use the first epoll thread to check out.
                         */
                        for (start_thread = start_thread_bkup; start_thread < end_thread; start_thread++) {
                                processed_entries += check_out_hmoq(start_thread);
                        }
                        if (processed_entries == 0) {
                                pthread_mutex_lock(&hmoq_mutex[send_thread_mutex_num]);
                                pthread_cond_wait(&hmoq_cond[send_thread_mutex_num],
							    &hmoq_mutex[send_thread_mutex_num]);
                                pthread_mutex_unlock(&hmoq_mutex[send_thread_mutex_num]);
                        }
                        processed_entries = 0;
                }
        } else {
                while(1) {
                        /*
                         *  check out http_mgr_outpur_q to see if we have
                         *  any pending http_mgr_output queue.
                         *  We only use the first epoll thread to check out.
                         */
                        check_out_hmoq(num);
                        pthread_mutex_lock(&hmoq_mutex[num]);
                        pthread_cond_wait(&hmoq_cond[num], &hmoq_mutex[num]);
			pthread_mutex_unlock(&hmoq_mutex[num]);
                }
        }
	return NULL;
}
/*
 * NM_main is an infinite loop. It never exits unless somehow hits Ctrl-C.
 */
static void * NM_main_loop( void * arg )
{
        int i, cpu ;
        int res, ret;
	int fd;
	network_mgr_t * pnm;
	struct nm_threads * pnmthr  = (struct nm_threads *)arg;
	uint32_t incarn;
	char name[64];

	snprintf(name, 64, "nvsd-net-%lu", pnmthr->num);
	prctl(PR_SET_NAME, name, 0, 0, 0);

	// cpu = nkn_choose_cpu( pnmthr->num );
	cpu = pnmthr->num;

	pthread_setspecific(namespace_key, (void *)((pnmthr->num) + 1));

	snprintf(name, 64, "net_thread%lu_cpu%d_tot_sockets", pnmthr->num, cpu);
	nkn_mon_add(name, NULL, &pnmthr->cur_fds, sizeof(pnmthr->cur_fds));

	snprintf(name, 64, "net_thread%lu_cpu%d_sb_queue_size", pnmthr->num, cpu);
	nkn_mon_add(name, NULL, &pnmthr->cur_sbq, sizeof(pnmthr->cur_sbq));
	snprintf(name, 64, "nt%lu_mempool", pnmthr->num);

	nkn_mem_create_thread_pool((const char *)name);

	snprintf(name, 64, "monitor_%lu_network_thread", pnmthr->num);
	nkn_mon_add(name, NULL, &pnmthr->epoll_event_cnt, sizeof(pnmthr->epoll_event_cnt));


	// An infinite loop
	while(1) {

		// check out http_mgr_outpur_q to see if we have any pending http_mgr_output queue.
		// We only use the first epoll thread to check out.
		// check_out_hmoq(pnmthr->num);

		// check out other queues
		check_out_pe_req_queue(pnmthr->num);
		if (nm_hdl_send_and_receive) check_out_hmoq(pnmthr->num);
		check_out_fp_queue(pnmthr->num);
		check_out_cp_queue(pnmthr->num);
		check_out_tmrq(pnmthr->num);

		NM_timer_for_sbq(pnmthr);

		if(srv_shutdown == 1) break; // we are shutting down
		res =  epoll_wait(pnmthr->epfd, pnmthr->events, pnmthr->max_fds, epoll_wait_timeout);

		if(res == -1) {
			// Can happen due to -pg
			if (errno == EINTR)
				continue;
			//printf("epoll_wait: res=%d errno=%d\n", res, errno);
			DBG_LOG(SEVERE, MOD_NETWORK, 
				"errno=%d epfd=%d max_fds=%d", 
				errno, pnmthr->epfd, pnmthr->max_fds);
			DBG_ERR(SEVERE,
                                "errno=%d epfd=%d max_fds=%d",
                                errno, pnmthr->epfd, pnmthr->max_fds);
			exit(10);
		}
		else if(res == 0) {
			//printf("No active fds, total #=%d\n", max_client);
			// Timeout of epoll_wait()
			pnmthr->epoll_event_cnt++;
			continue;
		}

		for(i = 0; i < res; i++)
		{
			fd = pnmthr->events[i].data.fd;
			pnmthr->epoll_event_cnt++;

			pnm = (network_mgr_t *)&gnm[fd];
			incarn = gnm[fd].incarn;
			pthread_mutex_lock(&pnm->mutex);
			NM_TRACE_LOCK(pnm, LT_NETWORK);
			if (incarn != gnm[fd].incarn) {
				glob_err_closed_fd_in_epoll++;
				NM_TRACE_UNLOCK(pnm, LT_NETWORK);
				pthread_mutex_unlock(&pnm->mutex);
				continue;
			}
			if( !NM_CHECK_FLAG(pnm, NMF_IN_USE) ) {
				/* TBD: We should assert to crash */
				DBG_LOG(MSG, MOD_NETWORK, "structure is in use%s", "");
				glob_err_wrong_fd_in_epoll++;
				NM_SET_FLAG(pnm, NMF_IN_EPOLL);
				NM_del_event_epoll(fd);
				NM_UNSET_FLAG(pnm, NMF_IN_EPOLL);
				NM_TRACE_UNLOCK(pnm, LT_NETWORK);
				pthread_mutex_unlock(&pnm->mutex);
				continue;
			}
			NM_set_socket_active(pnm);
			ret=TRUE;

			if (pnmthr->events[i].events & EPOLLERR) {
				glob_socket_err_epoll++;
				ret=pnm->f_epollerr(fd, pnm->private_data);
				if (ret==FALSE) {
					NM_close_socket(fd);
				}
				NM_TRACE_UNLOCK(pnm, LT_NETWORK);
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
			if(ret==FALSE) {
				NM_close_socket(fd);
			}
			NM_TRACE_UNLOCK(pnm, LT_NETWORK);
			pthread_mutex_unlock(&pnm->mutex);
		}

	} // while(1); infinite loop

	close(pnmthr->epfd);

	return NULL;
}

void NM_main( void )
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

	/*
	 * adjust counters: glob_socket_cur_open does not include glob_tot_svr_sockets
	 */
	AO_fetch_and_add(&glob_socket_cur_open, -glob_tot_svr_sockets);
	AO_fetch_and_add(&glob_socket_cur_open_ipv6, -glob_tot_svr_sockets_ipv6);

        if (nm_hdl_send_and_receive) {
                /* One NM_send_thread for 24 NM_tot_threads */
                NM_send_threads = NM_tot_threads/12;
                NM_send_threads += (NM_tot_threads%12)?1:0;

                for(i=0; i<NM_send_threads; i++)
                {
                        pthread_create(&g_NM_thrs[i].pid, &attr, NM_main_send_loop, (void *)i);
                }

                for(i=0; i<NM_tot_threads; i++)
                {
                        pthread_create(&g_NM_thrs[i].pid, &attr, NM_main_loop, &g_NM_thrs[i]);
                }
        } else {
                for(i=0; i<NM_tot_threads; i++)
                {
                        pthread_create(&g_NM_thrs[i].pid, &attr, NM_main_send_loop, (void *)i);
                        pthread_create(&g_NM_thrs[i].pid, &attr, NM_main_loop, &g_NM_thrs[i]);
                }
        }

	// never return until killed.
	pthread_join(g_NM_thrs[0].pid, NULL);
}


void dbg_dump_gnm(void)
{
	int i;
	for(i=0; i<MAX_GNM; i++)
	{
		if(gnm[i].fd != -1) {
			printf("id=%d, fd=%d\n", i, gnm[i].fd);
		}
	}
}

#ifdef SOCKFD_TRACE

static char lt_file_name[8][32] = {
	"",
	"network.c",		// LT_NETWORK		1
	"server.c",		// LT_SERVER		2
	"om_api.c",		// LT_OM_API		3
	"om_connpoll.c",	// LT_OM_CONNPOLL		4
	"om_network.c",		// LT_OM_NETWORK		5
	"om_tunnel.c",		// LT_OM_TUNNEL		6
	"auth_helper.c"		// LT_AUTH_HELPER		7
};


#ifdef DBG_EVENT_TRACE
#ifdef DBG_FLOW_TRACE


void dbg_dump_ft(int fd);
void dbg_dump_ft(int fd)
{
	int i, k, n;
	
	if (gnm[fd].p_trace == NULL) return;
	
	printf("Cur Idx = %d\n", (uint32_t) gnm[fd].p_trace->f_idx);
	k = gnm[fd].p_trace->f_idx + 1;
	for (i=0; i < NUM_FLOW_TRACE; i++, k++) {
		n = (k % NUM_FLOW_TRACE) * 3;
		printf("%4d %8d %16s %6d\n", i, gnm[fd].p_trace->f_event[n],
			lt_file_name[gnm[fd].p_trace->f_event[n+1]], gnm[fd].p_trace->f_event[n+2]);
	}
}

#else	// DBG_FLOW_TRACE

void dbg_dump_fd_trace(int fd);
void dbg_dump_fd_trace(int fd)
{
	int i, j, k;
	
	if (gnm[fd].p_trace == NULL) return;
	
	printf("Cur Idx = %04x\n", (uint32_t) gnm[fd].p_trace->cur_event);
	k = gnm[fd].p_trace->cur_event + 1;
	for (i=0; i < NUM_EVENT_TRACE;) {
		for (j=0; j < 8; j++, i++, k++) {
			printf("%02x %02x  ", gnm[fd].p_trace->event[k % NUM_EVENT_TRACE * 2], 
				gnm[fd].p_trace->event[k % NUM_EVENT_TRACE*2+1]);
		}
		printf("\n");
	}
}
#endif // DBG_FLOW_TRACE
#endif // DBG_EVENT_TRACE

#ifdef DBG_MUTEX_TRACE

int lt_file_num_lines[8] = {
	0,
	1243,		// LT_NETWORK		1
	2621,		// LT_SERVER		2
	1625,		// LT_OM_API		3
	2128,		// LT_OM_CONNPOLL		4
	4330,		// LT_OM_NETWORK		5
	1684,		// LT_OM_TUNNEL		6
	310		// LT_AUTH_HELPER		7
};

void dbg_dump_lt(int fd);
void dbg_dump_lt(int fd)
{
	int i, k, n, f, l;
	
	if (gnm[fd].p_trace == NULL) return;
	
	printf("Cur Idx = %d\n", (uint32_t) gnm[fd].p_trace->lock_idx);
	k = gnm[fd].p_trace->lock_idx + 1;
	for (i=0; i < NUM_LOCK_TRACE; i++, k++) {
		n = gnm[fd].p_trace->lock_event[k % NUM_LOCK_TRACE];
		f = (n >> 16) & 0x00ff;
		l = n & 0x0000ffff;
		printf("%4d %08x  %16s: %4d %c\n", i, n, lt_file_name[f], l,
			n & 0x01000000 ? 'L': 'U' );
	}
}

void dbg_check_lt(void);
void dbg_check_lt(void)
{
	int i, j, n , f;
	for(i=0; i<MAX_GNM; i++)
	{
		if(gnm[i].p_trace != NULL) {
			for (j=0; j < NUM_LOCK_TRACE; j++) {
				n = gnm[i].p_trace->lock_event[j];
				f = (n >> 16) & 0x00ff;
				if ((n & 0x0000ffff) > lt_file_num_lines[f]) {
					printf( "%d %d %08x\n", i, j, n );
				}
			}
		}
	}
}

#endif // DBG_MUTEX_TRACE

#endif // SOCKFD_TRACE

