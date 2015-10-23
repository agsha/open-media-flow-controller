#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <limits.h>
#include <linux/netfilter_ipv4.h>


#include "nkn_debug.h"
#include "nkn_errno.h"
#include "server.h"
#include "nkn_om.h"
#include "om_defs.h"
#include "om_fp_defs.h"
#include "logging.h"
#include "nkn_stat.h"
#include "rtsp_live.h"
#include "rtsp_server.h"
#include "rtsp_header.h"


#include "om_port_mapper.h"

/* We cant change the headers on a Centos distro. So this
 * constant may or may not be available from the standard
 * kernel headers.
 */
#if !defined(IP_TRANSPARENT)
#define IP_TRANSPARENT	(19)
#endif


NKNCNT_DEF(tp_no_client_ip, int32_t, "", "tproxy no client ip")
NKNCNT_DEF(tp_err_set_src_ip, int32_t, "", "failed to set src ip")
NKNCNT_DEF(tp_err_set_src_ip_1, int32_t, "", "failed to set src ip")
NKNCNT_DEF(tp_err_set_src_ip_2, int32_t, "", "failed to set src ip")
NKNCNT_DEF(tp_err_set_src_ip_3, int32_t, "", "failed to set src ip")

extern int rtsp_tproxy_enable;
int rtsp_set_transparent_mode_tosocket(int fd, rtsp_con_ps_t * pps);
uint32_t tproxy_find_org_dst_ip(int sockfd, struct nkn_interface * pns);


static int __cp_set_transparent_mode_tosocket(int fd, in_addr_t src_ip,
	in_addr_t dest_ip, uint16_t dport, uint16_t *__port);



extern pmapper_t    pmap_ctxt;

/*
 * Function to TPROXY config params
 */
int cp_set_transparent_mode_tosocket(int fd, cpcon_t * cpcon)
{
	int err = 0;
	if (cpcon->src_ipv4v6.family == AF_INET6) {
	        DBG_LOG(SEVERE, MOD_OM, "IPv6 transparency is not yet supported");
		return FALSE;
	}
	if (IPv4(cpcon->src_ipv4v6) == 0) {
		/*
		 * if socket source IP address is not provided
		 * e.g. AM internal request
		 * we will use MFD interface IP address to make a socket.
		 */
		glob_tp_no_client_ip++;
		return TRUE;
	}

	err = __cp_set_transparent_mode_tosocket(
		fd, IPv4(cpcon->src_ipv4v6), IPv4(cpcon->remote_ipv4v6), cpcon->remote_port,
		&(cpcon->src_port));

	return err;
}

int rtsp_set_transparent_mode_tosocket(int fd, rtsp_con_ps_t * pps)
{
	return (__cp_set_transparent_mode_tosocket(
		    fd,
		    pps->pplayer_list->peer_ip_addr, 0, 0, NULL));
}


uint32_t tproxy_find_org_dst_ip(int sockfd, struct nkn_interface * pns)
{
	struct sockaddr_in dip;
	socklen_t dlen = sizeof(dip);

	// If T-proxy is not enabled, return interface IP address
	if (rtsp_tproxy_enable == 0) {
		return pns->if_addrv4;
	}

	/* Get real destination IP */
	memset(&dip, 0, dlen);
	if (getsockname(sockfd, (struct sockaddr *) &dip, &dlen) == -1) {
		return pns->if_addrv4;
	}

	return dip.sin_addr.s_addr;
}

/* fd - socket fd
 * src_ip - source ip to spoof to. (Always network byte order!!)
 *
 * Unlike the previous versions (tproxy2), the route interface
 * is selected based on normal routing rules.
 */
static int __cp_set_transparent_mode_tosocket(int fd, in_addr_t src_ip,
	in_addr_t dest_ip, uint16_t dport, uint16_t *__port)
{
    struct sockaddr_in sin;
    int val = 0;
    int err = 0;
    uint16_t port = 0;

    val = 1;
    err = setsockopt(fd, SOL_IP, IP_TRANSPARENT, &val, sizeof(val));
    if (err < 0) {
	glob_tp_err_set_src_ip_1++;
	DBG_LOG(SEVERE, MOD_OM,
		"Failed to set socket to IP_TRANSPARENT (errno = %d)", errno);
	return FALSE;
    }
    val = 1;
    err = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    if (err == -1) {
	glob_tp_err_set_src_ip_3++;
	DBG_LOG(WARNING, MOD_OM,
		"Failed to set REUSEADDR on tproxy socket. errno = %d", errno);
	return FALSE;
    }

    /* Get a free port */
    if ((err = om_pmapper_get_port(pmap_ctxt, src_ip, dest_ip, dport, &port)) == -ENOPORT) {
	glob_tp_err_set_src_ip_2++;
	DBG_LOG(WARNING, MOD_OM,
		"Failed to bind TPROXY socket to local address. errno = %d", errno);
	return FALSE;
    }

    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = src_ip;
    sin.sin_port = htons(port);
    if (bind(fd, (struct sockaddr *) &sin, sizeof(sin)) < 0)
    {
	/* Try thrice in the hope of getting a free port.. otherwise giveup
	 */
	glob_tp_err_set_src_ip_2++;
	DBG_LOG(WARNING, MOD_OM,
		"Failed to bind TPROXY socket to local address. errno = %d", errno);
	/* return back the port */
	if ((err = om_pmapper_put_port(pmap_ctxt, src_ip, port)) != 0) {
	    DBG_LOG(SEVERE, MOD_OM, "om_pmapper_put_port() failed for port : %d", port);
	}
	return FALSE;
    }

    if (__port != NULL) {
	*__port = port;
    }
    return TRUE;
}

int cp_release_transparent_port(cpcon_t *cpcon)
{
    if (cpcon && cpcon->src_port) {
	om_pmapper_put_port(pmap_ctxt, IPv4(cpcon->src_ipv4v6), cpcon->src_port);
    }

    return 0;
}
