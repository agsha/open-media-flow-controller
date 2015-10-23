/**
 * @file   dns_con.h
 * @author Sunil Mukundan <smukundan@cmbu-build03>
 * @date   Thu Mar  1 22:22:13 2012
 * 
 * @brief  definitions for the dns_con_ctx_t object and its methods
 * 
 * 
 */
#ifndef _DNS_CON_
#define _DNS_CON_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <netinet/in.h>
#include <net/if.h> //if_nameindex()
#include <sys/socket.h>
#include <arpa/inet.h> //inet_ntop()
#include <sys/types.h>
#include <sys/ioctl.h>
#include <aio.h>
#include <inttypes.h>

// nkn common defs
#include "nkn_defs.h"
#include "nkn_debug.h"
#include "event_dispatcher.h"
#include "entity_data.h"

#include "dns_common_defs.h"
#include "cr_common_intf.h"
#include "dns_udp_handler.h"
#include "dns_proto_handler.h"
#include "dns_errno.h"

#ifdef __cplusplus
extern "C" {
#endif

#if 0 //keeps emacs happy
}
#endif

/* object instantiation and cleanup */
void* dns_con_create(void);
int32_t dns_nw_ctx_create(nw_ctx_t **out); // need to find a better
					   // place for this definition
int32_t dns_proto_data_create(int32_t buf_size, 
      int32_t num_bufs, proto_data_t **out);// need to find a better place
					    // for this definition
int32_t dns_con_tcp_data_recv_handler(dns_con_ctx_t *con);
int32_t dns_con_send_data(dns_con_ctx_t *con);
void dns_con_reset_for_reuse(void *ref);

/* GET SET API's */
inline obj_proto_desc_t *dns_con_get_proto_desc_safe(dns_con_ctx_t *ctx);
inline void dns_con_rel_proto_desc(dns_con_ctx_t *ctx);
inline proto_data_t * dns_con_get_proto_data_safe(dns_con_ctx_t *ctx);
inline void dns_con_rel_proto_data(dns_con_ctx_t *ctx);
inline obj_proto_desc_t * dns_con_get_proto_desc(dns_con_ctx_t *ctx);
inline proto_data_t * dns_con_get_proto_data(dns_con_ctx_t *ctx);
inline nw_ctx_t * dns_con_get_nw_ctx_safe(dns_con_ctx_t *ctx);
inline void dns_con_rel_nw_ctx(dns_con_ctx_t *ctx);
inline nw_ctx_t * dns_con_get_nw_ctx(dns_con_ctx_t *ctx);
inline ref_count_mem_t * dns_con_get_ref(dns_con_ctx_t *ctx);

/* other API's */
#define DNS_CON_LOCK(p) \
    pthread_mutex_lock(&p->lock);
#define DNS_CON_UNLOCK(p) \
    pthread_mutex_unlock(&p->lock);

#ifdef __cplusplus
}
#endif

#endif //_DNS_CON_
