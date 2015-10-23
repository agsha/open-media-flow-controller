
#ifndef __PXY_SERVER__H__
#define __PXY_SERVER__H__

#include <assert.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include "pxy_interface.h"

#define TIMER_INTERVAL	5

/*
 * socket handling
 */

#define SOCKET_CLOSED 		0
#define SOCKET_WAIT_EPOLLOUT_EVENT 1
#define SOCKET_WAIT_EPOLLIN_EVENT 2
#define SOCKET_WAIT_CM_DATA	3
#define SOCKET_DATA_SENTOUT	4
#define SOCKET_TIMER_EVENT	5
#define SOCKET_NO_ACTION	6	// Used by SSP during outbound path (send_to_ssp)

/*
 * connection control block
 */

#define CONF_CLIENT_SIDE	0x0000000000000001 /* Client side connection */

#define CONF_CANCELD		0x0000000000000004
#define CONF_SYN_SENT		0x0000000000000040
#define CONF_TASK_TIMEOUT	0x0000000000000200

#define CON_MAGIC_FREE          0x123456789deadbee
#define CON_MAGIC_USED          0x1111111111111111

#define MAX_CB_BUF	4096

#define SET_CON_FLAG(x, f) (x)->con_flag |= (f)
#define UNSET_CON_FLAG(x, f) (x)->con_flag &= ~(f)
#define CHECK_CON_FLAG(x, f) ((x)->con_flag & (f))

/*
 * Unix domain socket will be opened by nvsd & proxyd at
 * this path to fwd the client fds to nvsd.
 */
#define PXY_CLIENT_FD_FWD_PATH "/config/nkn/.proxyd"

/*
 * con_t NM_func_timer event types
 */
typedef struct struct_con_t{
	uint64_t	magic;
	uint64_t 	con_flag;

	/* socket control block */
        int32_t 	fd;
        int32_t 	peer_fd;
	uint16_t 	reserved1;
	uint16_t 	src_port;
	uint32_t 	src_ip;
	uint16_t	dest_port;
	uint32_t 	dest_ip;

	uint32_t        tproxy_ip;
        off_t 		sent_len;

	char 		cb_buf[MAX_CB_BUF];
        int32_t 	cb_totlen;      // last data byte in cb_buf
        int32_t 	cb_offsetlen;   // first data byte in cb_buf

	time_t   	nkn_cur_ts;	// The time to calculate max_send_size

	/* server information */
        pxy_nkn_interface_t  *pns ;

} con_t;


/* This has to be in sync with the 'fd_struct' defined in 
 * bin/nvsd/nkn_recvfd.c
 */
struct fd_struct
{
        int       listen_fd;
	int32_t   accepted_thr_num;
        struct    sockaddr_in client_addr;
        struct    sockaddr_in dst_addr;
};

typedef enum {
        L4PROXY_ACTION_NONE   = 0,	// None
        L4PROXY_ACTION_TUNNEL = 1,	// Tunnel the socket in l4proxy
        L4PROXY_ACTION_REJECT = 2,	// when nvsd is down reject connection
        L4PROXY_ACTION_WHITE  = 3,	// forward to nvsd.
        L4PROXY_ACTION_BLACK  = 4,	// block the client socket
} l4proxy_action;

l4proxy_action 
get_l4proxy_action(uint32_t ip);

extern int 
pxy_set_transparent_mode_tosocket(int, con_t *);

extern int
pxy_attach_nvsd_health_info(void);

#endif // __PXY_SERVER__H__

