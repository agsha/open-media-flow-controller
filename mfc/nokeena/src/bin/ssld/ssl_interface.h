
#ifndef __INTERFACE__H__
#define __INTERFACE__H__

#include <assert.h>
#include <stdint.h>
#include <pthread.h>
#include <atomic_ops.h>
#include <netdb.h>

#define MAX_PORTLIST	64

/*
 * TBD: 
 *    someday I need to separate interface structure information from server control block.
 */


/*
 * Each interface launches one server socket.
 * this design simplifies the ABR calculation and can add more server-based counters.
 *
 * server control block
 *
 */

#define MAX_IPV6_IP         32
#define MAX_NKN_INTERFACE      128

typedef struct ipv6_if {
    struct in6_addr if_addrv6;
    uint16_t port[MAX_PORTLIST];
    uint16_t listenfdv6[MAX_PORTLIST];
} ipv6_if_t;

typedef struct nkn_interface {
	/* interface information */
        char 	if_name[16];
        int32_t if_bandwidth;   // in Bytes/Sec
        int32_t if_mtu;         // in Bytes
        char 	if_mac[6];
        uint32_t if_addrv4;       // network order
        ipv6_if_t if_addrv6[MAX_IPV6_IP]; //network order
        uint32_t if_subnet;     // network order
        uint32_t if_netmask;    // network order
        uint32_t if_gw;    	// network order
        uint32_t ifv6_ip_cnt; // Number of IPv6 addresses configured 

	/* server information */
	uint16_t port[MAX_PORTLIST];	// server listens on this port
	uint16_t listenfd[MAX_PORTLIST];// socket id
	AO_t tot_sessions;	// currently open connections.
	AO_t max_allowed_sessions;	// calculated based on AFR
	AO_t max_allowed_bw;	// Max allowed BW for this session for AFR calculation.

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
extern nkn_interface_t interface[MAX_NKN_INTERFACE];
extern char ssl_listen_intfname[MAX_NKN_INTERFACE][16];
extern int ssl_listen_intfcnt;
extern int nkn_ssl_serverport[MAX_PORTLIST];

extern uint16_t glob_tot_interface;

extern int interface_init(void);
extern void interface_display(void);
extern struct nkn_interface * interface_find_by_dstip(uint32_t dstip);
extern uint8_t * interface_find_mac_for_dstip(uint32_t ip, char * ifname);

#endif // __INTERFACE__H__
