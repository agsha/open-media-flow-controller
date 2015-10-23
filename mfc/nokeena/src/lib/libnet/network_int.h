/*
 * network_int.h -- Internal library definitions
 */
#ifndef NETWORK_INT_H
#define NETWORK_INT_H

#include <stdint.h>
#include <atomic_ops.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "memalloc.h"

#define TRUE 1
#define FALSE 0

#define UNUSED_ARGUMENT(x) (void )x

enum dlevel_enum {
    ALARM = 0,
    SEVERE = 1,
    ERROR = 2,
    WARNING = 3,
    MSG = 4,
    MSG1 = 4,
    MSG2 = 5,
    MSG3 = 6,
};

typedef enum {
    TT_ONESHOT = 0,
    TT_MAX // Always last
} net_timer_type_t;

typedef enum {
    FREE_FD = 0,
    NETWORK_FD,
    CLIENT_FD,
    SERVER_FD,
    OM_FD,
    DM2_FD,
    RTSP_FD,
    RTSP_UDP_FD,
    RTSP_OM_FD,
    RTSP_OM_UDP_FD,
} nkn_fd_type_t;

typedef struct net_fd_handle {
    uint32_t incarn;
    int32_t fd;
} net_fd_handle_t;

#define MAX_GNM (512*1024)

#define MOD_NETWORK     0x0000000000000001
#define MOD_HTTP 	0x0000000000000002

#define KiBYTES 1024

extern int (*pnet_dbg_log)(int, long, const char *, ...);

#define DBG_LOG(_dlevel, _dmodule, _fmt, ...) { \
	if ((_dlevel) <= *libnet_log_level) { \
	    (*pnet_dbg_log)((_dlevel), (_dmodule), ""_fmt"\n", ##__VA_ARGS__); \
	} \
}

#define DBG_ERR(_dlevel, _fmt, ...)
#define NKN_ASSERT(_e)

#define MAX_PORTLIST 64
#define MAX_IPV6_IP  32
#define MAX_NKN_INTERFACE 32

#define nkn_mon_add 	loc_nkn_mon_add
#define nkn_mon_delete	loc_nkn_mon_delete
#define nkn_mon_get	loc_nkn_mon_get
#define nkn_mon_reset	loc_nkn_mon_reset

typedef enum nkn_if_type {
    NKN_IF_PHYSICAL = 0,
    NKN_IF_BOND,
    NKN_IF_ALIAS,
    NKN_IF_LO,
    NKN_IF_UNK
} nkn_if_type_t;

typedef struct ipv6_if {
    struct in6_addr if_addrv6;
    uint16_t port[MAX_PORTLIST];
    uint16_t listenfdv6[MAX_PORTLIST];
    AO_t if_status; // 0 = delete/disabled, 1 = add/enabled
    AO_t if_listening; // indicates currently listening
} ipv6_if_t;

typedef struct nkn_interface {
    struct nkn_interface *next, *prev;
    /* interface information */
    char 	if_name[16];
    int64_t if_credit;   	// left over in last sec in Bytes/Sec
    int64_t if_totbytes_sent;// total byte sent
    int64_t if_bandwidth;   // in Bytes/Sec
    int32_t if_mtu;         // in Bytes
    char 	if_mac[6];
    uint32_t if_addrv4;       // network order
    char     if_addrv4_numeric[NI_MAXHOST];
    ipv6_if_t if_addrv6[MAX_IPV6_IP]; //network order
    struct if_addr_numeric_list {
    	char     addr[NI_MAXHOST];
    } if_addrv6_numeric[MAX_IPV6_IP];
    uint32_t if_subnet;     // network order
    uint32_t if_netmask;    // network order
    uint32_t if_gw;    	// network order
    uint32_t ifv6_ip_cnt; // Number of IPv6 addresses configured 

    /* server information */
    uint16_t port[MAX_PORTLIST];	// server listens on this port
    uint16_t listenfd[MAX_PORTLIST];// socket id
    AO_t tot_sessions;	// currently open connections.
    AO_t max_allowed_sessions;	// calculated based on AFR
    AO_t max_allowed_bw; // Max allowed BW for this session for AFR calculation.

    AO_t if_refcnt; // Refcnt by con_t 
    nkn_if_type_t if_type; // 0 = physical/bond, 1 = alias, 2 = loopback
    AO_t if_status; // 0 = down/delete, 1 = up/enabled
    AO_t if_listening; // indicates interface listening status

    /* network driver information */
    int tot_pkt;		// Total packet in the MMAP
    unsigned long rx_begin_offset;	// Packet location in MMAP
    unsigned long rx_end_offset;	// Packet location in MMAP
    unsigned long tx_begin_offset;	// Packet location in MMAP
    unsigned long tx_end_offset;	// Packet location in MMAP
    pthread_mutex_t ioctl_mutex;	// mutex for managing MMAP buffer.

    /* HTTP delivery protocol listen enable status from cli*/
    int32_t enabled;		// 1 = enabled, 0 = disabled

} nkn_interface_t;

typedef struct ip_addr {
    uint8_t    family;
    union {
        struct  in_addr v4;
        struct  in6_addr v6;
    } addr;
} ip_addr_t;

typedef struct http_control_block {
    char *cb_buf;
    int32_t cb_max_buf_size;
    int32_t cb_totlen; // total this length data in cb_buf
    const char *remote_ipv4v6_numeric;
    ip_addr_t remote_ipv4v6; // this server IP address returns the response.
    uint16_t remote_port;    // this server port returns the response.
} http_cb_t;

typedef struct struct_con_t{
	uint64_t	magic;
	uint64_t 	con_flag;
	uint64_t	pe_action;

	/* socket control block */
        int32_t 	fd;
        int32_t 	reserved0;
	uint16_t 	reserved1;
	uint16_t 	src_port;
	uint16_t 	src_port_hs; // host order
	uint16_t	dest_port;
	uint8_t		ip_family;

	/* HTTP control block */
	http_cb_t 	http;

	/* server information */
	struct nkn_interface * pns;
    	/* VIP pns */
	struct nkn_interface * v_pns;

	/* ip structures */
	ip_addr_t       src_ipv4v6;
	char 		src_ipv4v6_numeric[NI_MAXHOST];
	ip_addr_t       dest_ipv4v6;

#if 0
	struct {
		uint8_t num_ips;
		uint8_t num_ip_index;
	} os_failover;

	/* Network feature configuration */
	uint64_t max_bandwidth;     	// Session bandwidth: Bytes/sec
	uint64_t min_afr;               // Assured Flow Rate: Bytes/sec
	uint64_t max_client_bw;         // Detected client (ISP) Bandwidth: Bytes/sec
	uint64_t max_allowed_bw;        // Allowed Session Bandwidth (Data Center): Bytes/sec
	uint64_t max_send_size;         // Max Sent Size in this second
	uint64_t bandwidth_send_size;   // AFR, allowed bandwidth send
	uint64_t max_faststart_buf;	// Initial Buffer Size for Fast Start (Bytes)
	time_t   nkn_cur_ts;		// The time to calculate max_send_size

	/* IP TOS setting */
	int32_t		ip_tos;

	/* HTTP control block */
	int32_t		num_of_trans;
	http_cb_t 	http;

	/* Server Side Player control block */
    	char                    session_id[MAX_NKN_SESSION_ID_SIZE];
   	nkn_ssp_params_t	ssp;

	/* server information */
	struct nkn_interface * pns;
    /* VIP pns */
	struct nkn_interface * v_pns;

	/* Scheduler control block */
	nkn_task_id_t	nkn_taskid;

	/* Module type field to indicate whether to copy this or not*/
	nkn_module_t	module_type;

	/* http_timer() state data */
	tmrq_event_type_t timer_event;

	/* ip structures */
	ip_addr_t       src_ipv4v6;
	ip_addr_t       dest_ipv4v6;

	int ns_wait_intervals; // Awaiting namespace init complete intervals

	/* GeoIP info */
	geo_info_t ginf;

	/* URL category */
	ucflt_info_t uc_inf;
	
	struct {
		ip_addr_t ipv4v6[MAX_ORIG_IP_CNT];
		int count;
		ip_addr_t last_ipv4v6;
	}origin_servers;

        /*MD5 checksum calculation for delivery(client) side*/
        http_resp_md5_checksum_t *md5 ;
#endif
} con_t;


extern int http_try_to_sendout_data(con_t *con);
extern void nkn_mark_fd(int fd, nkn_fd_type_t type);
extern int nkn_close_fd(int fd, nkn_fd_type_t type);
extern void nkn_mem_create_thread_pool(const char *pname);

extern int NM_tot_threads;
extern int nkn_max_client;
extern uint16_t glob_tot_interface;
extern int nm_lb_policy;
extern int net_use_nkn_stack;
extern int close_socket_by_RST_enable;
extern nkn_interface_t interface[MAX_NKN_INTERFACE];
extern time_t nkn_cur_ts;
extern int *libnet_log_level;

#endif /* NETWORK_INT_H */

/*
 * End of network_int.h
 */
