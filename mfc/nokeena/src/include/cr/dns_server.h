/**
 * @file   dns_server.h
 * @author Sunil Mukundan <smukundan@cmbu-build03>
 * @date   Thu Mar  1 04:21:27 2012
 * 
 * @brief  definitions for the DNS TCP/UDP listeners
 * 
 * 
 */
#ifndef _DNS_SERVER_
#define _DNS_SERVER_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <net/if.h> //if_nameindex()
#include <sys/socket.h>
#include <arpa/inet.h> //inet_ntop()
#include <sys/types.h>
#include <sys/ioctl.h>
#include <aio.h>
#include <inttypes.h>
#include <errno.h>

// nkn common defs
#include "nkn_defs.h"
#include "nkn_debug.h"
#include "event_dispatcher.h"
#include "entity_data.h"

#include "dns_common_defs.h"
#include "cr_common_intf.h"
#include "dns_udp_handler.h"
#include "dns_tcp_handler.h"
#include "common/nkn_free_list.h"
#include "dns_common_defs.h"
#include "dns_con.h"

#ifdef __cplusplus
extern "C" {
#endif

#if 0 //keeps emacs happy
}
#endif

int32_t dns_start_udp_server(struct sockaddr_in **gl_ip_if_addr,
			     event_disp_t *disp);
int32_t dns_start_tcp_server(struct sockaddr_in **gl_ip_if_addr,
			     event_disp_t *disp, 
			     obj_list_ctx_t *con_list); 
int32_t dns_set_socket_non_blocking(int fd);

#ifdef __cplusplus
}
#endif

#endif //_DNS_SERVER_

