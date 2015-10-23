#ifndef __LIBPXY_COMMON_H__
#define __LIBPXY_COMMON_H__

#include <assert.h>
#include <stdint.h>
#include <pthread.h>
#include <signal.h>
#include <atomic_ops.h>
#include <sys/queue.h>

#include <nkn_lockstat.h>
#include <interface.h>
#include <pxy_defs.h>

#ifndef MAX_PORTLIST
#define MAX_PORTLIST           64
#endif

#ifndef MAX_NKN_INTERFACE
#define MAX_NKN_INTERFACE      32
#endif

#ifndef MAX_TOQ_SLOT 
#define MAX_TOQ_SLOT 300
#endif

#define NM_SET_FLAG(x, f)         (x)->flag |= (f);
#define NM_UNSET_FLAG(x, f)       (x)->flag &= ~(f);
#define NM_CHECK_FLAG(x, f)       ((x)->flag & (f))

/* Global NM flags(pnm->flag) */
#define NMF_IN_USE                0x0000000000000001
#define NMF_IN_TIMEOUTQ           0x0000000000000002    // In Timeout Queue
#define NMF_IN_EPOLL              0x0000000000000004    // added in Epoll
#define NMF_IN_SBQ                0x0000000000000008    // In Session Bandwidth Queue
#define NMF_IN_EPOLLIN            0x0000000000000020    // added in Epollin
#define NMF_IN_EPOLLOUT           0x0000000000000040    // added in Epollout
#define NMF_NO_FD_UNEPOLL         0x0000000000000080    // Didn't unregister fd from epoll.
#define NMF_RST_WAIT              0x0000000000000100    // Wait to data go out for RST feature
#define NMF_TYPE_INTERFACE        0x0000000000000200    // pnm->private_data is of type 'pxy_nkn_interface_t *'
#define NMF_TYPE_CON_T            0x0000000000000400    // pnm->private_data will be con_t* when this flag is set.

extern volatile sig_atomic_t pxy_srv_shutdown; 
/*
 * Each interface launches one server socket.
 * this design simplifies the ABR calculation and can add more server-based counters.
 *
 * server control block
 *
 */

typedef struct pxy_nkn_interface {
        /* interface information */
        char    if_name[16];
        int32_t if_bandwidth;   // in Bytes/Sec
        int32_t if_mtu;         // in Bytes
        char    if_mac[6];
        uint32_t if_addr;       // network order
        uint32_t if_subnet;     // network order
        uint32_t if_netmask;    // network order
        uint32_t if_gw;         // network order

        /* server information */
        uint16_t port[MAX_PORTLIST];    // server listens on this port
        uint16_t listenfd[MAX_PORTLIST];// socket id
        AO_t tot_sessions;      // currently open connections.
        AO_t max_allowed_sessions;      // calculated based on AFR
        AO_t max_allowed_bw;    // Max allowed BW for this session for AFR calculation.

        /* network driver information */
        int tot_pkt;            // Total packet in the MMAP
        unsigned long rx_begin_offset;  // Packet location in MMAP
        unsigned long rx_end_offset;    // Packet location in MMAP
        unsigned long tx_begin_offset;  // Packet location in MMAP
        unsigned long tx_end_offset;    // Packet location in MMAP
        pthread_mutex_t ioctl_mutex;    // mutex for managing MMAP buffer.

        /* HTTP delivery protocol listen enable status from cli*/
        int32_t enabled;                // 1 = enabled, 0 = disabled

} pxy_nkn_interface_t;


/*
 * Network Manager table is the global table for storing socket information.
 * This table is indexed by socket id.
 * This table is linked with nkn_max_client configuration field.
 * Every operation on any table entry should get this entries' mutex first.
 */
typedef int (* NM_func)(int sockfd, void * data);
typedef int (* NM_func_timeout)(int sockfd, void * data, double timeout);

struct nm_threads ;
typedef struct pxy_network_mgr {
        uint64_t        flag;
        uint32_t        incarn;                 // Updated on socket close
        int32_t         fd;
        int32_t         accepted_thr_num;
        struct nm_threads * pthr;               // which thread this nm is running on

        /* Timer */
        time_t          last_active;            // Last active time
        LIST_ENTRY(pxy_network_mgr) toq_entries;        // In Timeout queue

        /* Mutex to pretec this epoll group */
        pthread_mutex_t mutex;                  // Mutex to protect this structure only.

        /* callback functions */
        void *          private_data;           // caller private data
        NM_func         f_epollin;
        NM_func         f_epollout;
        NM_func         f_epollerr;
        NM_func         f_epollhup;
        NM_func_timeout f_timeout;
        int             timeout_slot;           // each slot is 5 seconds
} network_mgr_t;

typedef struct NM_main_arg {
        network_mgr_t *gnm_arg ;
        struct nm_threads *nm_thr_arg;
        nkn_lockstat_t *pnm_lockstat_arg ;
} NM_main_arg_t ;

LIST_HEAD(nm_timeout_queue, pxy_network_mgr);

/*
 * This structure is defined for each running epoll thread.
 * Each network epoll thread owns one structure.
 * Any operation on timeout and bandwidth queues should get table mutex first.
 */
typedef struct nm_threads {
        pthread_t     pid;          // thread id
        int           num;          // This thread number, counting from 0

        /* epoll control management variables */
        AO_t          cur_fds;      // currently total open fds in this thread
        unsigned int  max_fds;      // Maxixum allowed total fds in this thread
        int           epfd;

        /* fd timeout slots. */
        int           cur_toq;
        struct        nm_timeout_queue toq_head[MAX_TOQ_SLOT];

        /* protect nm_timeout_queue and nm_sess_bw_queue */
        pthread_mutex_t nm_mutex;

        /* events used for this epoll thread */
        struct        epoll_event *events;
        unsigned int  epoll_event_cnt;

} nm_threads_t;

/*
 * Get Link Aggregation Slave Interface Number
 */
int libpxy_get_link_aggregation_slave_interface_num(char * if_name) ;

void libpxy_interface_display(pxy_nkn_interface_t *interface,
                           uint16_t             lib_tot_interface) ;

/*
 *  Network Interface Management APIs
 */
int libpxy_interface_init(pxy_nkn_interface_t *interface,
                       uint16_t             *lib_tot_interface) ;


void libpxy_NM_init(int lib_NM_tot_threads,
                 int lib_nkn_max_client,
                 network_mgr_t **pgnm,
                 struct nm_threads * p_NM_thrs,
                 nkn_lockstat_t *pnm_lockstat,
                 nkn_mutex_t *fd_type_mutex_p) ;

/*
 * timer functions
 * timer function will be called once every 5 seconds
 */
void libpxy_NM_timer(int lib_NM_tot_threads,
                  struct nm_threads * p_NM_thrs,
                  network_mgr_t * gnm_loc, 
                  nkn_lockstat_t *pnm_lockstat) ;

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
        int timeout     ,// in seconds, timeout func callbacked at [timeout, timeout+5) seconds
        network_mgr_t * gnm_loc,
        struct nm_threads * p_NM_thrs,
        nkn_lockstat_t *pnm_lockstat ) ;

/*
 * epoll_in/out/err operation function
 */

int libpxy_NM_add_event_epollin(int fd, network_mgr_t * gnm_loc) ;
int libpxy_NM_add_event_epollout(int fd, network_mgr_t * gnm_loc) ;
int libpxy_NM_del_event_epoll_inout(int fd, network_mgr_t * gnm_loc);
int libpxy_NM_del_event_epoll(int fd, network_mgr_t * gnm_loc);
int libpxy_NM_set_socket_nonblock(int fd);
/*
 * ***  IMPORTANT NOTES  ****:
 * This function requires ROOT account priviledge.
 */
int libpxy_NM_bind_socket_to_interface(int fd, char * p_interface) ;
/*
 * Caller should get the mutex before calling this function
 */
void libpxy_NM_close_socket(int fd, network_mgr_t * gnm_loc,
                         nkn_lockstat_t *pnm_lockstat) ;
/*
 * Caller should get the mutex before calling this function
 *
 * unregister_NM_socket is the same as NM_close_socket
 * except not closing the socket.
 *
 */
int libpxy_NM_unregister_socket(int fd, network_mgr_t * gnm_loc,
                             nkn_lockstat_t *pnm_lockstat) ;
void libpxy_NM_set_socket_active(network_mgr_t * pnm,
                              nkn_lockstat_t *pnm_lockstat) ;
/*
 * NM_main is an infinite loop. It never exits unless somehow hits Ctrl-C.
 */
void * libpxy_NM_main_loop(void * arg) ;
void libpxy_NM_main (int lib_NM_tot_threads,
                     NM_main_arg_t *p_NM_main_args) ;

#endif /* __LIBPXY_COMMON_H__ */
