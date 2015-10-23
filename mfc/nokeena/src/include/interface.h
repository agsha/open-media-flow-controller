
#ifndef __INTERFACE__H__
#define __INTERFACE__H__

#include <assert.h>
#include <stdint.h>
#include <pthread.h>
#include <atomic_ops.h>
#include <arpa/inet.h>

#include "nkn_defs.h"
#define MAX_PORTLIST	64

/*
 * TBD: 
 *    someday I need to separate interface structure information from server control block.
 */

// In a 2-byte value, LSB is PNS index and MSB is IPv6 index

/*
 * Each interface launches one server socket.
 * this design simplifies the ABR calculation and can add more server-based counters.
 *
 * server control block
 *
 */

#define MAX_IPV6_IP         32
#define MAX_NKN_INTERFACE      32
#define MAX_NKN_ALIAS      128

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
    ipv6_if_t if_addrv6[MAX_IPV6_IP]; //network order
    uint32_t if_subnet;     // network order
    uint32_t if_netmask;    // network order
    uint32_t if_gw;    	// network order
    uint32_t ifv6_ip_cnt; // Number of IPv6 addresses configured 

    /* server information */
    uint16_t port[MAX_PORTLIST];	// server listens on this port
    uint16_t listenfd[MAX_PORTLIST][64];// socket id
    AO_t tot_sessions;	// currently open connections.
    AO_t max_allowed_sessions;	// calculated based on AFR
    AO_t max_allowed_bw;	// Max allowed BW for this session for AFR calculation.

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
extern nkn_interface_t interface[MAX_NKN_INTERFACE];
extern nkn_interface_t *if_alias_list;
//extern char http_listen_intfname[MAX_NKN_INTERFACE][16];
extern char **http_listen_intfname;
extern int http_listen_intfcnt;

extern char rtsp_listen_intfname[MAX_NKN_INTERFACE][16];
extern int rtsp_listen_intfcnt;

extern uint16_t glob_tot_interface;

extern int interface_init(void);
extern void interface_display(void);
extern struct nkn_interface * interface_find_by_dstip(uint32_t dstip);
extern uint8_t * interface_find_mac_for_dstip(uint32_t ip, char * ifname);


#endif // __INTERFACE__H__

