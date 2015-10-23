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

#include "nkn_debug.h"
#include "nkn_stat.h"
#include "nkn_assert.h"
#include "nkn_lockstat.h"
#include "libpxy_common.h"


NKNCNT_DEF(pxy_err_wrong_fd_in_epoll, uint64_t, "", "error fd in epoll")
NKNCNT_DEF(pxy_license_used, uint64_t, "", "Total license used")
NKNCNT_DEF(pxy_socket_accumulate_tot, AO_t, "", "Total accepted sockets")
NKNCNT_DEF(pxy_socket_err_epoll, uint64_t, "", "Total sockets closed due to epoll error")
NKNCNT_DEF(pxy_socket_hup_epoll, uint32_t, "", "Total sockets closed due to epoll error")
NKNCNT_DEF(pxy_cur_open_all_sockets, AO_t, "", "Total currently open sockets")
NKNCNT_DEF(pxy_socket_tot_timeout, uint64_t, "", "Total Timeout")
NKNCNT_DEF(pxy_socket_tot_imm_timeout, uint64_t, "", "Total Immediate Timeout")
NKNCNT_DEF(pxy_socket_in_epoll, AO_t, "", "Total sockets in epoll")
NKNCNT_DEF(pxy_register_bad_fd, uint64_t, "", "Total sockets cannot be registered in NM")
NKNCNT_DEF(pxy_run_out_of_license, uint64_t, "", "Run out of license")
NKNCNT_DEF(pxy_err_epoll_add, uint64_t, "", "Total failure in epoll_add")
NKNCNT_DEF(pxy_err_epoll_del, uint64_t, "", "Total failure in epoll_del")
NKNCNT_DEF(pxy_err_epoll_exist, uint64_t, "", "Total failure in epoll_ctl returns EXIST")
NKNCNT_DEF(pxy_err_bad_timer_entry, uint64_t, "", "num of bad entries inserted in timer slots");
NKNCNT_DEF(pxy_warn_register_in_use, uint32_t, "", "num of register err when flag is still set");
NKNCNT_DEF(pxy_err_timeout_f_timeout_NULL, uint32_t, "", "BUG 4704");
NKNCNT_DEF(pxy_err_unsent_data_rst_close, uint32_t, "", "num of rst close when unsent data is in socket");
NKNCNT_DEF(pxy_err_bad_timer_entry_1, uint64_t, "", "num of bad entries inserted in timer slots, invalid gnm case");
NKNCNT_DEF(pxy_err_bad_timer_entry_2, uint64_t, "", "num of bad entries inserted in timer slots, not in Q case");
NKNCNT_DEF(pxy_err_bad_timer_entry_3, uint64_t, "", "num of bad entries inserted in timer slots, not in Q case");
NKNCNT_DEF(pxy_gnm_alloc_failure, uint64_t, "", "Allocating global NM list failed");


// 200 was added in nkn_check_cfg() call.
#define SET_NM_THR_MAX_FDS(x) \
        (x)->max_fds = (lib_nkn_max_client-200)/lib_NM_tot_threads; \
        if ((x)->max_fds<100) (x)->max_fds=100; \



/*
 * init functions
 */
void libpxy_NM_init(int lib_NM_tot_threads,
                 int lib_nkn_max_client,
                 network_mgr_t **pgnm,
                 struct nm_threads * p_NM_thrs,
                 nkn_lockstat_t *pnm_lockstat,
                 nkn_mutex_t *fd_type_mutex_p)
{
	int i, j;
	struct nm_threads * pnmthr;
	char mutex_stat[15];

        /*
         * initialization
         */
	NKN_MUTEX_INIT(fd_type_mutex_p, NULL, "fd-type-mutex");

        *pgnm = (network_mgr_t *)calloc(MAX_GNM, sizeof(network_mgr_t));
        if (*pgnm == NULL) {
		DBG_LOG(SEVERE, MOD_PROXYD, "malloc failure%s", "");
		DBG_ERR(SEVERE, "malloc failure%s", "");
                glob_pxy_gnm_alloc_failure++ ;
		exit(1);
        }
	for(i=0; i<MAX_GNM; i++)
	{
		(*pgnm)[i].fd = -1;	// initialize to -1
	        (*pgnm)[i].incarn = 1;	// initialize to 1
	}

	/*
	 * Create epoll threads for running epoll ctl.
	 */
	for(i=0; i<lib_NM_tot_threads; i++)
	{
		// Set to zero
		memset((char *)&p_NM_thrs[i], 0, sizeof(struct nm_threads));

		// Initialize each member

		pnmthr=&p_NM_thrs[i];

		pnmthr->num = i;

		SET_NM_THR_MAX_FDS(pnmthr);

		// Each active fd will be in time out list.
		// Timeout is separated into groups.
		// If timed out, the whole group will time out.
		for(j=0; j<MAX_TOQ_SLOT; j++) {
			LIST_INIT( &pnmthr->toq_head[j] );
		}

		/* 
		 * big waste of memory. Each thread events holds 64K size.
		 * this design is for re-init the convenience of nkn_max_client.
		 */
		pnmthr->events = calloc(MAX_GNM, sizeof(struct epoll_event));
		assert( pnmthr->events != NULL);

		pnmthr->epfd = epoll_create(pnmthr->max_fds);
		assert( pnmthr->epfd != -1);

		// Initialize mutex
		snprintf(mutex_stat, sizeof(mutex_stat), "nm-mutex-%d", i);
		NKN_LOCKSTAT_INIT(&pnm_lockstat[i], mutex_stat);
		pthread_mutex_init(&pnmthr->nm_mutex, NULL);
		snprintf(mutex_stat, sizeof(mutex_stat), "epoll-mutex-%d", i);
	}
	DBG_LOG(SEVERE, MOD_PROXYD, "LIBPROXY: created '%d' NM threads", lib_NM_tot_threads);
}


/*
 * timer functions
 * timer function will be called once every 5 seconds
 */

void libpxy_NM_timer(int lib_NM_tot_threads,
                  struct nm_threads *p_NM_thrs,
                  network_mgr_t *gnm_loc,
                  nkn_lockstat_t *pnm_lockstat)
{
	network_mgr_t * pnm;
	struct nm_threads * pnmthr;
	int i;
	double timediff;
	int timeout_toq;
        uint32_t        incarn;

	for(i=0; i<lib_NM_tot_threads; i++) {
	    pnmthr=&p_NM_thrs[i];

	    /*
	     * pnmthr->nm_mutex is used to protect toq_head queue.
	     * however f_timeout may try to get this mutex lock.
	     * to avoid this deadlock, NM_timer needs to release this mutex.
	     */
	    NKN_MUTEX_LOCKSTAT(&pnmthr->nm_mutex, &pnm_lockstat[i]);

	    /* Timeout_toq is to be removed */
	    timeout_toq = (pnmthr->cur_toq + 1 ) % MAX_TOQ_SLOT; 
	
            // This NM should be timed out
            while (!LIST_EMPTY(&pnmthr->toq_head[timeout_toq])) {
                pnm = LIST_FIRST(&pnmthr->toq_head[timeout_toq]);
                LIST_FIRST(&pnmthr->toq_head[timeout_toq]) = LIST_NEXT(pnm, toq_entries);
                LIST_REMOVE(pnm, toq_entries);
                NM_UNSET_FLAG(pnm, NMF_IN_TIMEOUTQ);
                incarn = pnm->incarn;
                NKN_MUTEX_UNLOCKSTAT(&pnmthr->nm_mutex);

                pthread_mutex_lock(&pnm->mutex);

                if (incarn != pnm->incarn) {
                    glob_pxy_err_bad_timer_entry_1++;
                    pthread_mutex_unlock(&pnm->mutex);
                    NKN_MUTEX_LOCKSTAT(&pnmthr->nm_mutex, &pnm_lockstat[i]);
                    continue;
                }

                if (pnm->f_timeout == NULL) {
                    glob_pxy_err_timeout_f_timeout_NULL++;
                    DBG_LOG(ERROR, MOD_PROXYD, "(f_timeout==NULL) for fd=%d", pnm->fd);
                    pthread_mutex_unlock(&pnm->mutex);
                    NKN_MUTEX_LOCKSTAT(&pnmthr->nm_mutex, &pnm_lockstat[i]);
                    continue;
                }

                if (NM_CHECK_FLAG(pnm, NMF_IN_USE)) {
                    timediff = difftime(pxy_cur_ts, pnm->last_active);
                    if (timediff >= 4/*http_idle_timeout*/) {
                        if ((pnm->f_timeout(pnm->fd, pnm->private_data, timediff)) == TRUE) {
                            glob_pxy_socket_tot_timeout++;
                            libpxy_NM_close_socket(pnm->fd, gnm_loc, pnm_lockstat);
                        } else {
                            /*Application does not want to timeout socket.*/
                            libpxy_NM_set_socket_active(pnm, pnm_lockstat);
                        }
                    } else {
                        glob_pxy_err_bad_timer_entry_2++;
                        DBG_LOG(ERROR, MOD_PROXYD, "Bad timer entry fd=%d", pnm->fd);
                    }
                } else {
                    glob_pxy_err_bad_timer_entry_3++;
		    DBG_LOG(ERROR, MOD_PROXYD, "Invalid gnm ,Bad timer entry fd=%d", pnm->fd);
                }
                pthread_mutex_unlock(&pnm->mutex);
                NKN_MUTEX_LOCKSTAT(&pnmthr->nm_mutex, &pnm_lockstat[i]);
            } /* while */

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
int libpxy_NM_register_socket(int fd, 
	void * private_data,
        uint32_t pd_type,
	NM_func f_epollin,
	NM_func f_epollout,
	NM_func f_epollerr,
	NM_func f_epollhup,
	NM_func_timeout f_timeout,
	int timeout	,// in seconds, timeout func callbacked at [timeout, timeout+5) seconds
	network_mgr_t * gnm_loc,
        struct nm_threads * p_NM_thrs,
        nkn_lockstat_t *pnm_lockstat )
{
	network_mgr_t * pnm;	// global network manager array
	int num;

	if( (fd<0) || (fd>=MAX_GNM) )
	{
		glob_pxy_register_bad_fd ++;
		return FALSE;
	}

	pnm = &gnm_loc[fd];
	DBG_LOG(MSG, MOD_PROXYD, "[THRD:%d] fd=%d (%d)", getpid(), fd, gnm_loc[fd].fd);
	num=pnm->accepted_thr_num;

	if ( !NM_CHECK_FLAG(pnm, NMF_IN_USE) ) {
		/*
		 * TBD: BUG 1074:
		 * If (fd == -1 and private_data==NULL) we only reset flag.
		 * why pnm->flag is unset is unknown.
		 */
		glob_pxy_warn_register_in_use++;
		if ((pnm->fd == -1) && (pnm->private_data == NULL)) { pnm->flag = 0; }
	}

	pnm->fd = fd;
	pnm->private_data = private_data;
	NM_SET_FLAG(pnm, pd_type);  // type of private_data
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
		/* stick policy: 1 network thread bound to 1 network port */
		pnm->pthr = &p_NM_thrs[pnm->accepted_thr_num];
	}

        // To make sure the connection link list is integrated one.
        //printf("init_conn: LIST_INSERT_HEAD( con, caq_entries)\n");
	libpxy_NM_set_socket_active(pnm, pnm_lockstat);

        AO_fetch_and_add1(&glob_pxy_socket_accumulate_tot);
        AO_fetch_and_add1(&glob_pxy_cur_open_all_sockets);

	return TRUE;
}

/*
 * epoll_in/out/err operation function
 */

static inline int libpxy_NM_add_fd_to_event(int fd, uint32_t epollev,
                                         network_mgr_t * gnm_loc)
{
	struct epoll_event ev;
	int op;

	op = (NM_CHECK_FLAG(&gnm_loc[fd], NMF_IN_EPOLL)) ?
		EPOLL_CTL_MOD : EPOLL_CTL_ADD;

	/* initialized to make valgrind happy only */
	memset((char *)&ev, 0, sizeof(struct epoll_event));

	DBG_LOG(SEVERE, MOD_PROXYD, "LIBPROXY:Adding event(%d) for fd(%d)", epollev, fd);

	ev.events = epollev;
	ev.data.fd = fd;
	if(epoll_ctl(gnm_loc[fd].pthr->epfd, op, fd, &ev) < 0)
	{
		if(errno == EEXIST) {
			glob_pxy_err_epoll_exist++;
			return TRUE;
		}
		DBG_LOG(SEVERE, MOD_PROXYD, "Failed to add fd (%d), errno=%d", fd, errno);
		// BUG TBD:
		// sometime errno could return 9 (EBADF) error for valid epfd and fd
		// I don't know what root cause it is so far.
		//
		// I removed this assert.
		// We close this connection only and we don't want to crash the process.
		// BUG 429: is opened for this bug.
		glob_pxy_err_epoll_add++;
		//assert(0);
		return FALSE;
	}
	if( !NM_CHECK_FLAG(&gnm_loc[fd], NMF_IN_EPOLL) ) {
		AO_fetch_and_add1(&glob_pxy_socket_in_epoll);
		NM_SET_FLAG(&gnm_loc[fd], NMF_IN_EPOLL);
	}
	return TRUE;
}

int libpxy_NM_add_event_epollin(int fd, network_mgr_t * gnm_loc)
{
	if( NM_CHECK_FLAG(&gnm_loc[fd], NMF_IN_EPOLLIN) ) {
		return TRUE;
	}
	if( libpxy_NM_add_fd_to_event(fd, EPOLLIN, gnm_loc) == FALSE ) {
		return FALSE;
	}
	NM_SET_FLAG(&gnm_loc[fd], NMF_IN_EPOLLIN);
	NM_UNSET_FLAG(&gnm_loc[fd], NMF_IN_EPOLLOUT);
	return TRUE;
}

int libpxy_NM_add_event_epollout(int fd, network_mgr_t * gnm_loc)
{
	if( NM_CHECK_FLAG(&gnm_loc[fd], NMF_IN_EPOLLOUT) ) {
		return TRUE;
	}
	if( libpxy_NM_add_fd_to_event(fd, EPOLLOUT, gnm_loc) == FALSE ) {
		return FALSE;
	}
	NM_UNSET_FLAG(&gnm_loc[fd], NMF_IN_EPOLLIN);
	NM_SET_FLAG(&gnm_loc[fd], NMF_IN_EPOLLOUT);
	return TRUE;
}

int libpxy_NM_del_event_epoll_inout(int fd, network_mgr_t * gnm_loc)
{
	if( libpxy_NM_add_fd_to_event(fd, EPOLLERR | EPOLLHUP, gnm_loc) == FALSE ) {
		return FALSE;
	}
	NM_UNSET_FLAG(&gnm_loc[fd], NMF_IN_EPOLLIN|NMF_IN_EPOLLOUT);
	return TRUE;
}

int libpxy_NM_del_event_epoll(int fd, network_mgr_t * gnm_loc)
{
	if( !NM_CHECK_FLAG(&gnm_loc[fd], NMF_IN_EPOLL) ) {
		return TRUE;
	}

	if(epoll_ctl(gnm_loc[fd].pthr->epfd, EPOLL_CTL_DEL, fd, NULL) < 0) {
		// This function call may return negative
		// when del_event_epoll() is called twice.
		glob_pxy_err_epoll_del++;
		return TRUE;
	}

	AO_fetch_and_sub1(&glob_pxy_socket_in_epoll);
	NM_UNSET_FLAG(&gnm_loc[fd], NMF_IN_EPOLL);
	NM_UNSET_FLAG(&gnm_loc[fd], NMF_IN_EPOLLIN|NMF_IN_EPOLLOUT);
	return TRUE;
}


/*
 * epoll_in/out/err operation function
 */

int libpxy_NM_set_socket_nonblock(int fd)
{
        int opts;
	int on = 1;

        opts = O_RDWR; //fcntl(fd, F_GETFL);
        if(opts < 0)
        {
		return FALSE;
        }
        opts = (opts | O_NONBLOCK);
        if(fcntl(fd, F_SETFL, opts) < 0)
        {
		return FALSE;
        }

	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &on, sizeof(on));

	DBG_LOG(SEVERE, MOD_PROXYD, "LIBPROXY: Setting socket fd (%d) to non-blocking.", fd);
	return TRUE;
}


/*
 * ***  IMPORTANT NOTES  ****:
 * This function requires ROOT account priviledge.
 */
int libpxy_NM_bind_socket_to_interface(int fd, char * p_interface)
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
void libpxy_NM_close_socket(int fd, network_mgr_t * gnm_loc,
                         nkn_lockstat_t *pnm_lockstat)
{
	network_mgr_t * pnm;

	if(fd == -1) { 
		return;
	}

	pnm = &gnm_loc[fd];
	if( NM_CHECK_FLAG(pnm, NMF_IN_USE) ) {

		// Cleanup all queues
		NKN_MUTEX_LOCKSTAT(&pnm->pthr->nm_mutex, &pnm_lockstat[pnm->pthr->num]);
		if (NM_CHECK_FLAG(pnm, NMF_IN_TIMEOUTQ)) {
			LIST_REMOVE(pnm, toq_entries);
		}
		NKN_MUTEX_UNLOCKSTAT(&pnm->pthr->nm_mutex);

		DBG_LOG(MSG, MOD_PROXYD, "close(fd=%d)", fd);
        	AO_fetch_and_sub1(&glob_pxy_cur_open_all_sockets);
	}
	else {
		// Do not close if the socket is already marked not
		// in use.
		// TBD.  Why do we call the close again?
		DBG_LOG(MSG, MOD_PROXYD, "!NMF_IN_USE fd=%d", fd);
		return;
	}

       /*
        *  Free only the con_t structure. 
        */
	if( NM_CHECK_FLAG(pnm, NMF_TYPE_CON_T) ) {
            free(pnm->private_data); 
	    pnm->private_data = NULL;
        }
        pnm->fd   = -1;
	pnm->incarn++;
	pnm->flag = 0;	// Unset all flags
	close(fd); 
}

/*
 * Caller should get the mutex before calling this function
 *
 * unregister_NM_socket is the same as NM_close_socket
 * except not closing the socket.
 *
 */
int libpxy_NM_unregister_socket(int fd, network_mgr_t * gnm_loc,
                             nkn_lockstat_t *pnm_lockstat)
{
        network_mgr_t * pnm;

        if(fd == -1) {
                return FALSE;
        }

        assert(fd>=0 && fd<MAX_GNM);
	/* Code optimization, we do not need to remove fd from epoll loop */
	libpxy_NM_del_event_epoll(fd, gnm_loc);
        pnm = &gnm_loc[fd];
	DBG_LOG(MSG, MOD_PROXYD, "fd=%d (%d)", fd, gnm_loc[fd].fd);
        if( NM_CHECK_FLAG(pnm, NMF_IN_USE) ) {

		// Cleanup all queues
		NKN_MUTEX_LOCKSTAT(&pnm->pthr->nm_mutex, &pnm_lockstat[pnm->pthr->num]);
                if (NM_CHECK_FLAG(pnm, NMF_IN_TIMEOUTQ)) {
                        LIST_REMOVE(pnm, toq_entries);
                }
		NKN_MUTEX_UNLOCKSTAT(&pnm->pthr->nm_mutex);

                AO_fetch_and_sub1(&glob_pxy_cur_open_all_sockets);
        }
       /*
        *  Free only the con_t structure.
        */
        if (NM_CHECK_FLAG(pnm, NMF_TYPE_CON_T) ) {
            free(pnm->private_data);
	    pnm->private_data = NULL;	// clear private data also 
        }
        pnm->fd = -1;
	pnm->flag         = 0;
	pnm->f_epollin    = NULL;
	pnm->f_epollout   = NULL;
	pnm->f_epollerr   = NULL;
	pnm->f_epollhup   = NULL;
	pnm->f_timeout    = NULL;
	close(fd); 

	DBG_LOG(SEVERE, MOD_PROXYD, "LIBPROXY: Unregister NM socket fd (%d).", fd);
	return TRUE;
}

void libpxy_NM_set_socket_active(network_mgr_t * pnm,
                              nkn_lockstat_t *pnm_lockstat)
{
	int slot;

	if(pnm->f_timeout == NULL) {
		return;
	}

	pnm->last_active = pxy_cur_ts;
	
	// Timeout Queue operation
	NKN_MUTEX_LOCKSTAT(&pnm->pthr->nm_mutex, &pnm_lockstat[pnm->pthr->num]);
	if (NM_CHECK_FLAG(pnm, NMF_IN_TIMEOUTQ)) {
		LIST_REMOVE(pnm, toq_entries);
	}
	slot = (pnm->pthr->cur_toq + pnm->timeout_slot) % MAX_TOQ_SLOT;
	LIST_INSERT_HEAD(&(pnm->pthr->toq_head[slot]), pnm, toq_entries);
	NM_SET_FLAG(pnm, NMF_IN_TIMEOUTQ);
	NKN_MUTEX_UNLOCKSTAT(&pnm->pthr->nm_mutex);
}


/*
 * NM_main is an infinite loop. It never exits unless somehow hits Ctrl-C.
 */
void * libpxy_NM_main_loop( void * arg)
{
        NM_main_arg_t  *arg_loc = (NM_main_arg_t*) arg; 
        int i, cpu;
        int res, ret;
	int fd;
	network_mgr_t * pnm;
	struct nm_threads * pnmthr  = arg_loc->nm_thr_arg ;
        network_mgr_t * gnm_loc     = arg_loc->gnm_arg ;
        nkn_lockstat_t *pnm_lockstat= arg_loc->pnm_lockstat_arg ;
	char name[64];

	prctl(PR_SET_NAME, "proxy-network", 0, 0, 0);

	// cpu = nkn_choose_cpu( pnmthr->num );
	cpu = pnmthr->num;

	snprintf(name, 64, "net_thread%d_cpu%d_tot_sockets", pnmthr->num, cpu);
	nkn_mon_add(name, NULL, &pnmthr->cur_fds, sizeof(pnmthr->cur_fds));


	// An infinite loop
	while(1) {

		if(pxy_srv_shutdown == 1) break; // we are shutting down
		res =  epoll_wait(pnmthr->epfd, pnmthr->events, pnmthr->max_fds, 1000);

		if(res == -1) {
			// Can happen due to -pg
			if (errno == EINTR)
				continue;
			//printf("epoll_wait: res=%d errno=%d\n", res, errno);
			DBG_LOG(SEVERE, MOD_PROXYD, 
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
			continue;
		}

		for(i = 0; i < res; i++)
		{
			fd = pnmthr->events[i].data.fd;

			pnm = (network_mgr_t *)&gnm_loc[fd];

			pthread_mutex_lock(&pnm->mutex);
			if( !NM_CHECK_FLAG(pnm, NMF_IN_USE) ) {
				DBG_LOG(MSG, MOD_PROXYD, "structure is not in use%s", "");
				glob_pxy_err_wrong_fd_in_epoll++;
				NM_SET_FLAG(pnm, NMF_IN_EPOLL);
				libpxy_NM_del_event_epoll(fd, gnm_loc);
				NM_UNSET_FLAG(pnm, NMF_IN_EPOLL);
				pthread_mutex_unlock(&pnm->mutex);
				continue;
			}

			libpxy_NM_set_socket_active(pnm, pnm_lockstat);
			ret=TRUE;
			if (pnmthr->events[i].events & EPOLLIN) {
				ret=pnm->f_epollin(fd, pnm->private_data);
			} 

			else if (pnmthr->events[i].events & EPOLLOUT) {
				ret=pnm->f_epollout(fd, pnm->private_data);
			}
			else if (pnmthr->events[i].events & EPOLLERR) {
				glob_pxy_socket_err_epoll++;
				ret=pnm->f_epollerr(fd, pnm->private_data);
			}
			else if (pnmthr->events[i].events & EPOLLHUP) {
				glob_pxy_socket_hup_epoll++;
				ret=pnm->f_epollhup(fd, pnm->private_data);
			}

			// if returns false, we close the socket
			if(ret==FALSE) {
				libpxy_NM_close_socket(fd, gnm_loc, pnm_lockstat);
			}
			pthread_mutex_unlock(&pnm->mutex);
		}

	} // while(1); infinite loop

	close(pnmthr->epfd);

	return NULL;
}

void libpxy_NM_main (int lib_NM_tot_threads,
                     NM_main_arg_t *p_NM_main_args)
{
        struct nm_threads * p_NM_thrs = p_NM_main_args->nm_thr_arg ;
	int64_t i;
	int rv;
	pthread_attr_t attr;
	int stacksize = 512 * KiBYTES; // 512MB for libketama

	rv = pthread_attr_init(&attr);
	if (rv) {
		DBG_LOG(SEVERE, MOD_PROXYD, 
			"pthread_attr_init() failed, rv=%d", rv);
		DBG_ERR(SEVERE,
                        "pthread_attr_init() failed, rv=%d", rv);
		return;
	}
	rv = pthread_attr_setstacksize(&attr, stacksize);
	if (rv) {
		DBG_LOG(SEVERE, MOD_PROXYD, 
			"pthread_attr_setstacksize() stacksize=%d failed, "
			"rv=%d", stacksize, rv);
		return;
	}

	for(i=0; i<lib_NM_tot_threads; i++)
	{
		pthread_create(&p_NM_thrs[i].pid, &attr, libpxy_NM_main_loop, p_NM_main_args);
	}

	// never return until killed.
	pthread_join(p_NM_thrs[0].pid, NULL);
}


