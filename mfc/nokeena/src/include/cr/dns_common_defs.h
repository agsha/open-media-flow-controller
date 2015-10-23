#ifndef _CR_DNS_COMMON_DEFS_
#define _CR_DNS_COMMON_DEFS_

#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <net/if.h> //if_nameindex()
#include <sys/socket.h>
#include <arpa/inet.h> //inet_ntop()
#include <sys/types.h>
#include <sys/ioctl.h>
#include <aio.h>
#include <inttypes.h>

#include "cr_common_intf.h"
#include "event_dispatcher.h"
#include "dns_errno.h"
#include "dns_parser.h"
#include "common/nkn_ref_count_mem.h"

#ifdef __cpluplus
extern "C" {
#endif

#if 0 //keeps emacs happy
}
#endif

#define DNS_TCP_PREAMBLE_SIZE (2)

#define HOLD_CFG_OBJ(_p)\
    pthread_mutex_lock(&(_p)->lock);

#define REL_CFG_OBJ(_p)\
    pthread_mutex_unlock(&(_p)->lock);

typedef struct tag_cr_dns_cfg {
    pthread_mutex_t lock;
    uint32_t max_disp_descs; //default = 30000;
    uint32_t max_th_pool_tasks; //default = 100000;
    int32_t max_ip_interfaces; //default = 10;
    uint32_t dns_listen_port; //default = 53;
    uint32_t mtu; //default = 1400
    uint32_t dns_udp_max_pkt_size; //default = 512
    uint32_t dns_qr_buf_size; //default = 2048
    struct sockaddr_in** ip_if_addr; //can have a maximum of
				     //max_ip_interfaces
				     //entries
    uint64_t def_dns_ttl; //defaults to 0 secs
    uint32_t tcp_timeout;
    uint32_t rx_threads;
    uint32_t tx_threads;
    unsigned char debug_assert;
} cr_dns_cfg_t;

#define DNS_MAX_DISP 24
typedef struct tag_dns_disp_mgr {
    event_disp_t *disp[DNS_MAX_DISP];
    pthread_t disp_thread[DNS_MAX_DISP];
    uint32_t curr_idx;
    uint32_t num_disp;
} dns_disp_mgr_t;

typedef enum {
    DNS_UDP,
    DNS_TCP
}dns_tproto_t;

typedef enum tag_nw_state {
    DNS_NW_RECV = 1,
    DNS_NW_SEND = 2,
    DNS_NW_ERR = 4,
    DNS_NW_DONE = 8
}nw_state_t;

#define NW_CTX_GET_STATE(p) \
    p->state

#define NW_CTX_SET_STATE(p,x) \
    p->state |= (x);

#define NW_CTX_CLEAR_STATE(p) \
    p->state = 0;

typedef struct nw_ctx {
    pthread_mutex_t lock;
    nw_state_t state;
    dns_tproto_t  ptype;
    int32_t fd;
    struct sockaddr_in l_addr;
    struct sockaddr_in r_addr;
    const event_disp_t *disp;
    const entity_data_t *ed;
    uint32_t timeo;
} nw_ctx_t;

typedef struct tag_dns_con_ctx { 
    struct timeval last_activity;
    nw_ctx_t *nw_ctx;
    obj_proto_desc_t *proto_obj;
    proto_data_t *qr_buf;
} dns_con_ctx_t;

#ifdef __cplusplus
}
#endif

#endif //_CR_DNS_COMMON_DEFS_
