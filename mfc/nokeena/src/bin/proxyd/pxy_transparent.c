#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/netfilter_ipv4.h>
#include <string.h>
#include <errno.h>

#include <libpxy_common.h>

#include "pxy_defs.h"
#include "nkn_debug.h"
#include "nkn_assert.h"
#include "pxy_server.h"
#include "pxy_interface.h"
#include "pxy_network.h"


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

uint32_t tproxy_find_org_dst_ip(int sockfd, struct pxy_nkn_interface * pns);


static int __pxy_set_transparent_mode_tosocket(int fd, in_addr_t src_ip);

/*
 * Function to TPROXY config params
 */
int pxy_set_transparent_mode_tosocket(int fd, con_t * con)
{
	if ((con->tproxy_ip == 0) || (con->src_ip == 0)) {
		/*
		 * if socket source IP address is not provided
		 * e.g. AM internal request
		 * we will use MFD interface IP address to make a socket.
		 */
		glob_tp_no_client_ip++;
		return FALSE;
	}

	return (__pxy_set_transparent_mode_tosocket(
		    fd, con->src_ip));
}



uint32_t tproxy_find_org_dst_ip(int sockfd, struct pxy_nkn_interface * pns)
{
	struct sockaddr_in dip;
	socklen_t dlen = sizeof(dip);

	/* Get real destination IP */
	memset(&dip, 0, dlen);
	if (getsockname(sockfd, (struct sockaddr *) &dip, &dlen) == -1) {
		return pns->if_addr;
	}

	return dip.sin_addr.s_addr;
}

/* fd - socket fd
 * src_ip - source ip to spoof to. (Always network byte order!!)
 *
 * Unlike the previous versions (tproxy2), the route interface
 * is selected based on normal routing rules.
 */
static int __pxy_set_transparent_mode_tosocket(int fd, in_addr_t src_ip)
{
    struct sockaddr_in sin;
    int val = 0;
    int err = 0;

    val = 1;
    err = setsockopt(fd, SOL_IP, IP_TRANSPARENT, &val, sizeof(val));
    if (err < 0) {
	glob_tp_err_set_src_ip_1++;
	DBG_LOG(SEVERE, MOD_PROXYD,
		"Failed to set socket to IP_TRANSPARENT (errno = %d)", errno);
	return FALSE;
    }
    val = 1;
    err = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    if (err == -1) {
	glob_tp_err_set_src_ip_3++;
	DBG_LOG(WARNING, MOD_PROXYD,
		"Failed to set REUSEADDR on tproxy socket. errno = %d", errno);
	return FALSE;
    }

    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = src_ip;
    sin.sin_port = htons(0);
    if (bind(fd, (struct sockaddr *) &sin, sizeof(sin)) < 0) 
    {
	/* Try thrice in the hope of getting a free port.. otherwise giveup
	 */
	glob_tp_err_set_src_ip_2++;
	DBG_LOG(WARNING, MOD_PROXYD,
		"Failed to bind TPROXY socket to local address. errno = %d", errno);
	return FALSE;
    }
    return TRUE;
}
