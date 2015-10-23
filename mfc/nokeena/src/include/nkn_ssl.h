
#ifndef __NKN_SSL__H__
#define __NKN_SSL__H__

#include "nkn_defs.h"

#define HTTPS_REQ_IDENTIFIER 0x551abcde
#define MAX_VIRTUAL_DOMAIN_LEN NKN_MAX_FQDN_LENGTH


#define IPv4(x)  x.addr.v4.s_addr
#define IPv6(x)  x.addr.v6.s6_addr
#define MAX_SHM_SSL_ID_SIZE (512*1024)
#define CONN_POOL_SSL_MARKER '5'
typedef struct struct_ssl_con_t{
	uint64_t magic;
	
	/* Client source Ip and destination IP */
	uint16_t 	src_port;
	uint16_t	dest_port;
	ip_addr_t 	src_ipv4v6;
	ip_addr_t 	dest_ipv4v6;

	int		ssl_fid;
	char 		virtual_domain[MAX_VIRTUAL_DOMAIN_LEN];
}ssl_con_hdr_t;

#endif // __NKN_SSL__H__

