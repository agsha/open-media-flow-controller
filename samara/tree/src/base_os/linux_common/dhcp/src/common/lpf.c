/* lpf.c

   Linux packet filter code, contributed by Brian Murrel at Interlinx
   Support Services in Vancouver, B.C. */

/*
 * Copyright (c) 2004,2007,2009 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996-2003 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *   Internet Systems Consortium, Inc.
 *   950 Charter Street
 *   Redwood City, CA 94063
 *   <info@isc.org>
 *   https://www.isc.org/
 */

#include "dhcpd.h"
#if defined (USE_LPF_SEND) || defined (USE_LPF_RECEIVE)
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <errno.h>

#include <asm/types.h>
#include <linux/filter.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <netinet/in_systm.h>
#include "includes/netinet/ip.h"
#include "includes/netinet/udp.h"
#include "includes/netinet/if_ether.h"
#include <net/if.h>
#include <ifaddrs.h>

#ifndef PACKET_AUXDATA
#define PACKET_AUXDATA 8

struct tpacket_auxdata
{
	__u32		tp_status;
	__u32		tp_len;
	__u32		tp_snaplen;
	__u16		tp_mac;
	__u16		tp_net;
};
#endif

/* Reinitializes the specified interface after an address change.   This
   is not required for packet-filter APIs. */

/* Default broadcast address for IPoIB */
static unsigned char default_ib_bcast_addr[20] = {
 	0x00, 0xff, 0xff, 0xff,
	0xff, 0x12, 0x40, 0x1b,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff
};

#ifdef USE_LPF_SEND
void if_reinitialize_send (info)
	struct interface_info *info;
{
}
#endif

#ifdef USE_LPF_RECEIVE
void if_reinitialize_receive (info)
	struct interface_info *info;
{
}
#endif

/* Called by get_interface_list for each interface that's discovered.
   Opens a packet filter for each interface and adds it to the select
   mask. */

int if_register_lpf (info)
	struct interface_info *info;
{
	int sock;
	union {
		struct sockaddr_ll ll;
		struct sockaddr common;
	} sa;
	struct ifreq ifr;
	int type;
	int protocol;

	/* Make an LPF socket. */
	get_hw_addr(info);

	if (info->hw_address.hbuf[0] == HTYPE_INFINIBAND) {
		type = SOCK_DGRAM;
		protocol = ETHERTYPE_IP;
	} else {
		type = SOCK_RAW;
		protocol = ETH_P_ALL;
	}

	if ((sock = socket(PF_PACKET, type, htons((short)protocol))) < 0) {
		if (errno == ENOPROTOOPT || errno == EPROTONOSUPPORT ||
		    errno == ESOCKTNOSUPPORT || errno == EPFNOSUPPORT ||
		    errno == EAFNOSUPPORT || errno == EINVAL) {
			log_error ("socket: %m - make sure");
			log_error ("CONFIG_PACKET (Packet socket) %s",
				   "and CONFIG_FILTER");
			log_error ("(Socket Filtering) are enabled %s",
				   "in your kernel");
			log_fatal ("configuration!");
		}
		log_fatal ("Open a socket for LPF: %m");
	}

	memset (&ifr, 0, sizeof ifr);
	strncpy (ifr.ifr_name, (const char *)info -> ifp, sizeof ifr.ifr_name);
	if (ioctl (sock, SIOCGIFINDEX, &ifr))
		log_fatal ("Failed to get interface index: %m");

	/* Bind to the interface name */
	memset (&sa, 0, sizeof sa);
	sa.ll.sll_family = AF_PACKET;
	sa.ll.sll_protocol = htons(protocol);
	sa.ll.sll_ifindex = ifr.ifr_ifindex;
	if (bind (sock, &sa.common, sizeof sa)) {
		if (errno == ENOPROTOOPT || errno == EPROTONOSUPPORT ||
		    errno == ESOCKTNOSUPPORT || errno == EPFNOSUPPORT ||
		    errno == EAFNOSUPPORT || errno == EINVAL) {
			log_error ("socket: %m - make sure");
			log_error ("CONFIG_PACKET (Packet socket) %s",
				   "and CONFIG_FILTER");
			log_error ("(Socket Filtering) are enabled %s",
				   "in your kernel");
			log_fatal ("configuration!");
		}
		log_fatal ("Bind socket to interface: %m");
	}

	return sock;
}
#endif /* USE_LPF_SEND || USE_LPF_RECEIVE */

#ifdef USE_LPF_SEND
void if_register_send (info)
	struct interface_info *info;
{
	/* If we're using the lpf API for sending and receiving,
	   we don't need to register this interface twice. */
#ifndef USE_LPF_RECEIVE
	info -> wfdesc = if_register_lpf (info);
#else
	info -> wfdesc = info -> rfdesc;
#endif
	if (!quiet_interface_discovery)
		log_info ("Sending on   LPF/%s/%s%s%s",
		      info -> name,
		      print_hw_addr (info -> hw_address.hbuf [0],
				     info -> hw_address.hlen - 1,
				     &info -> hw_address.hbuf [1]),
		      (info -> shared_network ? "/" : ""),
		      (info -> shared_network ?
		       info -> shared_network -> name : ""));
}

void if_deregister_send (info)
	struct interface_info *info;
{
	/* don't need to close twice if we are using lpf for sending and
	   receiving */
#ifndef USE_LPF_RECEIVE
	/* for LPF this is simple, packet filters are removed when sockets
	   are closed */
	close (info -> wfdesc);
#endif
	info -> wfdesc = -1;
	if (!quiet_interface_discovery)
		log_info ("Disabling output on LPF/%s/%s%s%s",
		      info -> name,
		      print_hw_addr (info -> hw_address.hbuf [0],
				     info -> hw_address.hlen - 1,
				     &info -> hw_address.hbuf [1]),
		      (info -> shared_network ? "/" : ""),
		      (info -> shared_network ?
		       info -> shared_network -> name : ""));
}
#endif /* USE_LPF_SEND */

#ifdef USE_LPF_RECEIVE
/* Defined in bpf.c.   We can't extern these in dhcpd.h without pulling
   in bpf includes... */
extern struct sock_filter dhcp_bpf_filter [];
extern int dhcp_bpf_filter_len;
extern struct sock_filter dhcp_ib_bpf_filter [];
extern int dhcp_ib_bpf_filter_len;

#if defined (HAVE_TR_SUPPORT)
extern struct sock_filter dhcp_bpf_tr_filter [];
extern int dhcp_bpf_tr_filter_len;
static void lpf_tr_filter_setup (struct interface_info *);
#endif

static void lpf_gen_filter_setup (struct interface_info *);

void if_register_receive (info)
	struct interface_info *info;
{
	int val;

	/* Open a LPF device and hang it on this interface... */
	info -> rfdesc = if_register_lpf (info);

	if (info->hw_address.hbuf[0] != HTYPE_INFINIBAND) {
		val = 1;
		if (setsockopt (info -> rfdesc, SOL_PACKET, PACKET_AUXDATA,
				&val, sizeof val) < 0) {
			if (errno != ENOPROTOOPT)
				log_fatal ("Failed to set auxiliary packet data: %m");
		}
	}

#if defined (HAVE_TR_SUPPORT)
	if (info -> hw_address.hbuf [0] == HTYPE_IEEE802)
		lpf_tr_filter_setup (info);
	else
#endif
		lpf_gen_filter_setup (info);

	if (!quiet_interface_discovery)
		log_info ("Listening on LPF/%s/%s%s%s",
			  info -> name,
			  print_hw_addr (info -> hw_address.hbuf [0],
					 info -> hw_address.hlen - 1,
					 &info -> hw_address.hbuf [1]),
			  (info -> shared_network ? "/" : ""),
			  (info -> shared_network ?
			   info -> shared_network -> name : ""));
}

void if_deregister_receive (info)
	struct interface_info *info;
{
	/* for LPF this is simple, packet filters are removed when sockets
	   are closed */
	close (info -> rfdesc);
	info -> rfdesc = -1;
	if (!quiet_interface_discovery)
		log_info ("Disabling input on LPF/%s/%s%s%s",
			  info -> name,
			  print_hw_addr (info -> hw_address.hbuf [0],
					 info -> hw_address.hlen - 1,
					 &info -> hw_address.hbuf [1]),
			  (info -> shared_network ? "/" : ""),
			  (info -> shared_network ?
			   info -> shared_network -> name : ""));
}

static void lpf_gen_filter_setup (info)
	struct interface_info *info;
{
	struct sock_fprog p;

	memset(&p, 0, sizeof(p));

	if (info->hw_address.hbuf[0] == HTYPE_INFINIBAND) {
		/* Set up the bpf filter program structure. */
		p.len = dhcp_ib_bpf_filter_len;
		p.filter = dhcp_ib_bpf_filter;

		/* Patch the server port into the LPF program...
		   XXX
		   changes to filter program may require changes
		   to the insn number(s) used below!
		   XXX */
		dhcp_ib_bpf_filter[6].k = ntohs ((short)local_port);
	} else {
		/* Set up the bpf filter program structure.
		   This is defined in bpf.c */
		p.len = dhcp_bpf_filter_len;
		p.filter = dhcp_bpf_filter;

		/* Patch the server port into the LPF  program...
		   XXX changes to filter program may require changes
		   to the insn number(s) used below! XXX */
		dhcp_bpf_filter [8].k = ntohs ((short)local_port);
	}

	if (setsockopt (info -> rfdesc, SOL_SOCKET, SO_ATTACH_FILTER, &p,
			sizeof p) < 0) {
		if (errno == ENOPROTOOPT || errno == EPROTONOSUPPORT ||
		    errno == ESOCKTNOSUPPORT || errno == EPFNOSUPPORT ||
		    errno == EAFNOSUPPORT) {
			log_error ("socket: %m - make sure");
			log_error ("CONFIG_PACKET (Packet socket) %s",
				   "and CONFIG_FILTER");
			log_error ("(Socket Filtering) are enabled %s",
				   "in your kernel");
			log_fatal ("configuration!");
		}
		log_fatal ("Can't install packet filter program: %m");
	}
}

#if defined (HAVE_TR_SUPPORT)
static void lpf_tr_filter_setup (info)
	struct interface_info *info;
{
	struct sock_fprog p;

	memset(&p, 0, sizeof(p));

	/* Set up the bpf filter program structure.    This is defined in
	   bpf.c */
	p.len = dhcp_bpf_tr_filter_len;
	p.filter = dhcp_bpf_tr_filter;

        /* Patch the server port into the LPF  program...
	   XXX changes to filter program may require changes
	   XXX to the insn number(s) used below!
	   XXX Token ring filter is null - when/if we have a filter 
	   XXX that's not, we'll need this code.
	   XXX dhcp_bpf_filter [?].k = ntohs (local_port); */

	if (setsockopt (info -> rfdesc, SOL_SOCKET, SO_ATTACH_FILTER, &p,
			sizeof p) < 0) {
		if (errno == ENOPROTOOPT || errno == EPROTONOSUPPORT ||
		    errno == ESOCKTNOSUPPORT || errno == EPFNOSUPPORT ||
		    errno == EAFNOSUPPORT) {
			log_error ("socket: %m - make sure");
			log_error ("CONFIG_PACKET (Packet socket) %s",
				   "and CONFIG_FILTER");
			log_error ("(Socket Filtering) are enabled %s",
				   "in your kernel");
			log_fatal ("configuration!");
		}
		log_fatal ("Can't install packet filter program: %m");
	}
}
#endif /* HAVE_TR_SUPPORT */
#endif /* USE_LPF_RECEIVE */

#ifdef USE_LPF_SEND
ssize_t send_packet_ib(interface, packet, raw, len, from, to, hto)
	struct interface_info *interface;
	struct packet *packet;
	struct dhcp_packet *raw;
	size_t len;
	struct in_addr from;
	struct sockaddr_in *to;
	struct hardware *hto;
{
	unsigned ibufp = 0;
	double ih [1536 / sizeof (double)];
	unsigned char *buf = (unsigned char *)ih;
	ssize_t result;

	union sockunion {
		struct sockaddr sa;
		struct sockaddr_ll sll;
		struct sockaddr_storage ss;
	} su;

	assemble_udp_ip_header (interface, buf, &ibufp, from.s_addr,
				to->sin_addr.s_addr, to->sin_port,
				(unsigned char *)raw, len);
	memcpy (buf + ibufp, raw, len);

	memset(&su, 0, sizeof(su));
	su.sll.sll_family = AF_PACKET;
	su.sll.sll_protocol = htons(ETHERTYPE_IP);

	if (!(su.sll.sll_ifindex = if_nametoindex(interface->name))) {
		errno = ENOENT;
		log_error ("send_packet_ib: %m - failed to get if index");
		return -1;
	}

	su.sll.sll_hatype = htons(HTYPE_INFINIBAND);
	su.sll.sll_halen = sizeof(interface->bcast_addr);
	memcpy(&su.sll.sll_addr, interface->bcast_addr, 20);

	result = sendto(interface->wfdesc, buf, ibufp + len, 0,
			&su.sa, sizeof(su));

	if (result < 0)
		log_error ("send_packet_ib: %m");

	return result;
}

ssize_t send_packet (interface, packet, raw, len, from, to, hto)
	struct interface_info *interface;
	struct packet *packet;
	struct dhcp_packet *raw;
	size_t len;
	struct in_addr from;
	struct sockaddr_in *to;
	struct hardware *hto;
{
	unsigned hbufp = 0, ibufp = 0;
	double hh [16];
	double ih [1536 / sizeof (double)];
	unsigned char *buf = (unsigned char *)ih;
	int result;
	int fudge;

	if (!strcmp (interface -> name, "fallback"))
		return send_fallback (interface, packet, raw,
				      len, from, to, hto);

	if (interface->hw_address.hbuf[0] == HTYPE_INFINIBAND) {
		return send_packet_ib(interface, packet, raw, len, from,
				      to, hto);
	}

	if (hto == NULL && interface->anycast_mac_addr.hlen)
		hto = &interface->anycast_mac_addr;

	/* Assemble the headers... */
	assemble_hw_header (interface, (unsigned char *)hh, &hbufp, hto);
	fudge = hbufp % 4;	/* IP header must be word-aligned. */
	memcpy (buf + fudge, (unsigned char *)hh, hbufp);
	ibufp = hbufp + fudge;
	assemble_udp_ip_header (interface, buf, &ibufp, from.s_addr,
				to -> sin_addr.s_addr, to -> sin_port,
				(unsigned char *)raw, len);
	memcpy (buf + ibufp, raw, len);

	result = write (interface -> wfdesc, buf + fudge, ibufp + len - fudge);
	if (result < 0)
		log_error ("send_packet: %m");
	return result;
}
#endif /* USE_LPF_SEND */

#ifdef USE_LPF_RECEIVE
ssize_t receive_packet_ib (interface, buf, len, from, hfrom)
	struct interface_info *interface;
	unsigned char *buf;
	size_t len;
	struct sockaddr_in *from;
	struct hardware *hfrom;
{
	int length = 0;
	int offset = 0;
	unsigned char ibuf [1536];
	unsigned bufix = 0;
	unsigned paylen;

	length = read(interface->rfdesc, ibuf, sizeof(ibuf));

	if (length <= 0)
		return length;

	offset = decode_udp_ip_header(interface, ibuf, bufix, from,
				       (unsigned)length, &paylen, 0);

	if (offset < 0)
		return 0;

	bufix += offset;
	length -= offset;

	if (length < paylen)
		log_fatal("Internal inconsistency at %s:%d.", MDL);

	/* Copy out the data in the packet... */
	memcpy(buf, &ibuf[bufix], paylen);

	return (ssize_t)paylen;
}

ssize_t receive_packet (interface, buf, len, from, hfrom)
	struct interface_info *interface;
	unsigned char *buf;
	size_t len;
	struct sockaddr_in *from;
	struct hardware *hfrom;
{
	int length = 0;
	int offset = 0;
	int nocsum = 0;
	unsigned char ibuf [1536];
	unsigned bufix = 0;
	unsigned paylen;
	unsigned char cmsgbuf[CMSG_LEN(sizeof(struct tpacket_auxdata))];
	struct iovec iov = {
		.iov_base = ibuf,
		.iov_len = sizeof ibuf,
	};
	struct msghdr msg = {
		.msg_iov = &iov,
		.msg_iovlen = 1,
		.msg_control = cmsgbuf,
		.msg_controllen = sizeof(cmsgbuf),
	};
	struct cmsghdr *cmsg;

	if (interface->hw_address.hbuf[0] == HTYPE_INFINIBAND) {
		return receive_packet_ib(interface, buf, len, from, hfrom);
	}

	length = recvmsg (interface -> rfdesc, &msg, 0);
	if (length <= 0)
		return length;

	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		if (cmsg->cmsg_level == SOL_PACKET &&
		    cmsg->cmsg_type == PACKET_AUXDATA) {
			struct tpacket_auxdata *aux = (void *)CMSG_DATA(cmsg);
			nocsum = aux->tp_status & TP_STATUS_CSUMNOTREADY;
		}
	}

	bufix = 0;
	/* Decode the physical header... */
	offset = decode_hw_header (interface, ibuf, bufix, hfrom);

	/* If a physical layer checksum failed (dunno of any
	   physical layer that supports this, but WTH), skip this
	   packet. */
	if (offset < 0) {
		return 0;
	}

	bufix += offset;
	length -= offset;

	/* Decode the IP and UDP headers... */
	offset = decode_udp_ip_header (interface, ibuf, bufix, from,
				       (unsigned)length, &paylen, nocsum);

	/* If the IP or UDP checksum was bad, skip the packet... */
	if (offset < 0)
		return 0;

	bufix += offset;
	length -= offset;

	if (length < paylen)
		log_fatal("Internal inconsistency at %s:%d.", MDL);

	/* Copy out the data in the packet... */
	memcpy(buf, &ibuf[bufix], paylen);
	return paylen;
}

int can_unicast_without_arp (ip)
	struct interface_info *ip;
{
	return 1;
}

int can_receive_unicast_unconfigured (ip)
	struct interface_info *ip;
{
	return 1;
}

int supports_multiple_interfaces (ip)
	struct interface_info *ip;
{
	return 1;
}

void maybe_setup_fallback ()
{
	isc_result_t status;
	struct interface_info *fbi = (struct interface_info *)0;
	if (setup_fallback (&fbi, MDL)) {
		if_register_fallback (fbi);
		status = omapi_register_io_object ((omapi_object_t *)fbi,
						   if_readsocket, 0,
						   fallback_discard, 0, 0);
		if (status != ISC_R_SUCCESS)
			log_fatal ("Can't register I/O handle for \"%s\": %s",
				   fbi -> name, isc_result_totext (status));
		interface_dereference (&fbi, MDL);
	}
}

static unsigned char * get_ib_hw_addr(char * name)
{
	struct ifaddrs *ifaddrs;
	struct ifaddrs *ifa;
	struct sockaddr_ll *sll = NULL;
	static unsigned char hw_addr[8];

	if (getifaddrs(&ifaddrs) == -1)
		return NULL;

	for (ifa = ifaddrs; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family != AF_PACKET)
			continue;
		if (ifa->ifa_flags & IFF_LOOPBACK)
			continue;
		if (strcmp(ifa->ifa_name, name) == 0) {
			sll = (struct sockaddr_ll *)(void *)ifa->ifa_addr;
			break;
		}
	}
	if (sll == NULL) {
		freeifaddrs(ifaddrs);
		return NULL;
	}
	memcpy(hw_addr, &sll->sll_addr[sll->sll_halen - 8], 8);
	freeifaddrs(ifaddrs);
	return (unsigned char *)&hw_addr;
}

void
get_hw_addr(struct interface_info *info)
{
	struct hardware *hw = &info->hw_address;
	char *name = info->name;
	struct ifaddrs *ifaddrs;
	struct ifaddrs *ifa;
	struct sockaddr_ll *sll = NULL;
	unsigned char *hw_addr;

	if (getifaddrs(&ifaddrs) == -1)
		log_fatal("Failed to get interfaces");

	for (ifa = ifaddrs; ifa != NULL; ifa = ifa->ifa_next) {

		if (ifa->ifa_addr == NULL)
			continue;

		if (ifa->ifa_addr->sa_family != AF_PACKET)
			continue;

		if (ifa->ifa_flags & IFF_LOOPBACK)
			continue;

		if (strcmp(ifa->ifa_name, name) == 0) {
			sll = (struct sockaddr_ll *)(void *)ifa->ifa_addr;
			break;
		}
	}

	if (sll == NULL) {
		freeifaddrs(ifaddrs);
		log_fatal("Failed to get HW address for %s\n", name);
	}

	switch (sll->sll_hatype) {
		case ARPHRD_ETHER:
			hw->hlen = 7;
			hw->hbuf[0] = HTYPE_ETHER;
			memcpy(&hw->hbuf[1], sll->sll_addr, 6);
			break;
		case ARPHRD_IEEE802:
#ifdef ARPHRD_IEEE802_TR
		case ARPHRD_IEEE802_TR:
#endif /* ARPHRD_IEEE802_TR */
			hw->hlen = 7;
			hw->hbuf[0] = HTYPE_IEEE802;
			memcpy(&hw->hbuf[1], sll->sll_addr, 6);
			break;
		case ARPHRD_FDDI:
			hw->hlen = 17;
			hw->hbuf[0] = HTYPE_FDDI;
			memcpy(&hw->hbuf[1], sll->sll_addr, 16);
			break;
		case ARPHRD_INFINIBAND:
			/* For Infiniband, save the broadcast address and store
			 * the port GUID into the hardware address.
			 */
			if (ifa->ifa_flags & IFF_BROADCAST) {
				struct sockaddr_ll *bll;

				bll = (struct sockaddr_ll *)ifa->ifa_broadaddr;
				memcpy(&info->bcast_addr, bll->sll_addr, 20);
			} else {
				memcpy(&info->bcast_addr, default_ib_bcast_addr,
				       20);
			}

			hw->hlen = 1;
			hw->hbuf[0] = HTYPE_INFINIBAND;
			hw_addr = get_ib_hw_addr(name);
			if (!hw_addr)
				log_fatal("Failed getting %s hw addr", name);
			memcpy (&hw->hbuf [1], hw_addr, 8);
			break;
		default:
			freeifaddrs(ifaddrs);
			log_fatal("Unsupported device type %ld for \"%s\"",
				  (long int)sll->sll_family, name);
	}

	freeifaddrs(ifaddrs);
}
#endif
