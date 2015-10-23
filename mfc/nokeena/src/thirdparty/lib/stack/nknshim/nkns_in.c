/*-
 * Copyright (c) 1982, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (C) 2001 WIDE Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)in.c	8.4 (Berkeley) 1/9/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/netinet/in.c,v 1.102 2007/10/07 20:44:22 silby Exp $");

#include "opt_carp.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/malloc.h>
#include <sys/priv.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/in_pcb.h>

#include <nknshim/nkns_api.h>

/*
static int
in_ifinit(struct ifnet *ifp, struct in_ifaddr *ia, struct sockaddr_in *sin,
    int scrub);
*/


/*
 * Return 1 if an internet address is for a ``local'' host
 * (one to which we have a connection).  If subnetsarelocal
 * is true, this includes other subnets of the local net.
 * Otherwise, it includes only the directly-connected (sub)nets.
 */
int
in_localaddr(struct in_addr in)
{
	u_long i = ntohl(in.s_addr); 

	if ( (i & vnetmask) == vsubnet) return (1);
	return (0);
}

/*
 * Return 1 if an internet address is for the local host and configured
 * on one of its interfaces.
 */
int
in_localip(struct in_addr in)
{
	u_long i = ntohl(in.s_addr);

	if( i == vifip ) return (1);
	return (0);
}

/*
 * Determine whether an IP address is in a reserved set of addresses
 * that may not be forwarded, or whether datagrams to that destination
 * may be forwarded.
 */
int
in_canforward(struct in_addr in)
{
	register u_long i = ntohl(in.s_addr);
	register u_long net;

	if (IN_EXPERIMENTAL(i) || IN_MULTICAST(i) || IN_LINKLOCAL(i))
		return (0);
	if (IN_CLASSA(i)) {
		net = i & IN_CLASSA_NET;
		if (net == 0 || net == (IN_LOOPBACKNET << IN_CLASSA_NSHIFT))
			return (0);
	}
	return (1);
}

/*
 * Return 1 if the address might be a local broadcast address.
 */
int
in_broadcast(struct in_addr in, struct ifnet *ifp)
{
	if (in.s_addr == INADDR_BROADCAST ||
	    in.s_addr == INADDR_ANY)
		return 1;

	return (0);
}

/*
 * Delete any existing route for an interface.
 */
void
in_ifscrub(struct ifnet *ifp, struct in_ifaddr *ia)
{
	//in_scrubprefix(ia);
}

int
in_control(struct socket *so, u_long cmd, caddr_t data, struct ifnet *ifp,
    struct thread *td)
{
	printf("ERROR:  in_control\n");
	return 1;
}

#ifdef MAXHE_TODO
/*
 * Generic internet control operations (ioctl's).
 * Ifp is 0 if not an interface-specific ioctl.
 */
/* ARGSUSED */
int
in_control(struct socket *so, u_long cmd, caddr_t data, struct ifnet *ifp,
    struct thread *td)
{
	register struct ifreq *ifr = (struct ifreq *)data;
	register struct in_ifaddr *ia = 0, *iap;
	register struct ifaddr *ifa;
	struct in_addr allhosts_addr;
	struct in_addr dst;
	struct in_ifaddr *oia;
	struct in_aliasreq *ifra = (struct in_aliasreq *)data;
	struct sockaddr_in oldaddr;
	int error, hostIsNew, iaIsNew, maskIsNew, s;
	int iaIsFirst;

	iaIsFirst = 0;
	iaIsNew = 0;
	allhosts_addr.s_addr = htonl(INADDR_ALLHOSTS_GROUP);

	switch (cmd) {
	case SIOCALIFADDR:
		if (td != NULL) {
			error = priv_check(td, PRIV_NET_ADDIFADDR);
			if (error)
				return (error);
		}
		if (!ifp)
			return EINVAL;
		return in_lifaddr_ioctl(so, cmd, data, ifp, td);

	case SIOCDLIFADDR:
		if (td != NULL) {
			error = priv_check(td, PRIV_NET_DELIFADDR);
			if (error)
				return (error);
		}
		if (!ifp)
			return EINVAL;
		return in_lifaddr_ioctl(so, cmd, data, ifp, td);

	case SIOCGLIFADDR:
		if (!ifp)
			return EINVAL;
		return in_lifaddr_ioctl(so, cmd, data, ifp, td);
	}

	/*
	 * Find address for this interface, if it exists.
	 *
	 * If an alias address was specified, find that one instead of
	 * the first one on the interface, if possible.
	 */
	if (ifp) {
		dst = ((struct sockaddr_in *)&ifr->ifr_addr)->sin_addr;
		LIST_FOREACH(iap, INADDR_HASH(dst.s_addr), ia_hash)
			if (iap->ia_ifp == ifp &&
			    iap->ia_addr.sin_addr.s_addr == dst.s_addr) {
				ia = iap;
				break;
			}
		if (ia == NULL)
			TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
				iap = ifatoia(ifa);
				if (iap->ia_addr.sin_family == AF_INET) {
					ia = iap;
					break;
				}
			}
		if (ia == NULL)
			iaIsFirst = 1;
	}

	switch (cmd) {

	case SIOCAIFADDR:
	case SIOCDIFADDR:
		if (ifp == 0)
			return (EADDRNOTAVAIL);
		if (ifra->ifra_addr.sin_family == AF_INET) {
			for (oia = ia; ia; ia = TAILQ_NEXT(ia, ia_link)) {
				if (ia->ia_ifp == ifp  &&
				    ia->ia_addr.sin_addr.s_addr ==
				    ifra->ifra_addr.sin_addr.s_addr)
					break;
			}
			if ((ifp->if_flags & IFF_POINTOPOINT)
			    && (cmd == SIOCAIFADDR)
			    && (ifra->ifra_dstaddr.sin_addr.s_addr
				== INADDR_ANY)) {
				return EDESTADDRREQ;
			}
		}
		if (cmd == SIOCDIFADDR && ia == 0)
			return (EADDRNOTAVAIL);
		/* FALLTHROUGH */
	case SIOCSIFADDR:
	case SIOCSIFNETMASK:
	case SIOCSIFDSTADDR:
		if (td != NULL) {
			error = priv_check(td, PRIV_NET_ADDIFADDR);
			if (error)
				return (error);
		}

		if (ifp == 0)
			return (EADDRNOTAVAIL);
		if (ia == (struct in_ifaddr *)0) {
			ia = (struct in_ifaddr *)
				malloc(sizeof *ia, M_IFADDR, M_WAITOK | M_ZERO);
			if (ia == (struct in_ifaddr *)NULL)
				return (ENOBUFS);
			/*
			 * Protect from ipintr() traversing address list
			 * while we're modifying it.
			 */
			s = splnet();
			ifa = &ia->ia_ifa;
			IFA_LOCK_INIT(ifa);
			ifa->ifa_addr = (struct sockaddr *)&ia->ia_addr;
			ifa->ifa_dstaddr = (struct sockaddr *)&ia->ia_dstaddr;
			ifa->ifa_netmask = (struct sockaddr *)&ia->ia_sockmask;
			ifa->ifa_refcnt = 1;
			TAILQ_INSERT_TAIL(&ifp->if_addrhead, ifa, ifa_link);

			ia->ia_sockmask.sin_len = 8;
			ia->ia_sockmask.sin_family = AF_INET;
			if (ifp->if_flags & IFF_BROADCAST) {
				ia->ia_broadaddr.sin_len = sizeof(ia->ia_addr);
				ia->ia_broadaddr.sin_family = AF_INET;
			}
			ia->ia_ifp = ifp;

			TAILQ_INSERT_TAIL(&in_ifaddrhead, ia, ia_link);
			splx(s);
			iaIsNew = 1;
		}
		break;

	case SIOCSIFBRDADDR:
		if (td != NULL) {
			error = priv_check(td, PRIV_NET_ADDIFADDR);
			if (error)
				return (error);
		}
		/* FALLTHROUGH */

	case SIOCGIFADDR:
	case SIOCGIFNETMASK:
	case SIOCGIFDSTADDR:
	case SIOCGIFBRDADDR:
		if (ia == (struct in_ifaddr *)0)
			return (EADDRNOTAVAIL);
		break;
	}
	switch (cmd) {

	case SIOCGIFADDR:
		*((struct sockaddr_in *)&ifr->ifr_addr) = ia->ia_addr;
		return (0);

	case SIOCGIFBRDADDR:
		if ((ifp->if_flags & IFF_BROADCAST) == 0)
			return (EINVAL);
		*((struct sockaddr_in *)&ifr->ifr_dstaddr) = ia->ia_broadaddr;
		return (0);

	case SIOCGIFDSTADDR:
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0)
			return (EINVAL);
		*((struct sockaddr_in *)&ifr->ifr_dstaddr) = ia->ia_dstaddr;
		return (0);

	case SIOCGIFNETMASK:
		*((struct sockaddr_in *)&ifr->ifr_addr) = ia->ia_sockmask;
		return (0);

	case SIOCSIFDSTADDR:
		if ((ifp->if_flags & IFF_POINTOPOINT) == 0)
			return (EINVAL);
		oldaddr = ia->ia_dstaddr;
		ia->ia_dstaddr = *(struct sockaddr_in *)&ifr->ifr_dstaddr;
		if (ifp->if_ioctl) {
			IFF_LOCKGIANT(ifp);
			error = (*ifp->if_ioctl)(ifp, SIOCSIFDSTADDR,
			    (caddr_t)ia);
			IFF_UNLOCKGIANT(ifp);
			if (error) {
				ia->ia_dstaddr = oldaddr;
				return (error);
			}
		}
		if (ia->ia_flags & IFA_ROUTE) {
			ia->ia_ifa.ifa_dstaddr = (struct sockaddr *)&oldaddr;
			rtinit(&(ia->ia_ifa), (int)RTM_DELETE, RTF_HOST);
			ia->ia_ifa.ifa_dstaddr =
					(struct sockaddr *)&ia->ia_dstaddr;
			rtinit(&(ia->ia_ifa), (int)RTM_ADD, RTF_HOST|RTF_UP);
		}
		return (0);

	case SIOCSIFBRDADDR:
		if ((ifp->if_flags & IFF_BROADCAST) == 0)
			return (EINVAL);
		ia->ia_broadaddr = *(struct sockaddr_in *)&ifr->ifr_broadaddr;
		return (0);

	case SIOCSIFADDR:
		error = in_ifinit(ifp, ia,
		    (struct sockaddr_in *) &ifr->ifr_addr, 1);
		if (error != 0 && iaIsNew)
			break;
		if (error == 0) {
			if (iaIsFirst && (ifp->if_flags & IFF_MULTICAST) != 0)
				in_addmulti(&allhosts_addr, ifp);
			EVENTHANDLER_INVOKE(ifaddr_event, ifp);
		}
		return (0);

	case SIOCSIFNETMASK:
		ia->ia_sockmask.sin_addr = ifra->ifra_addr.sin_addr;
		ia->ia_subnetmask = ntohl(ia->ia_sockmask.sin_addr.s_addr);
		return (0);

	case SIOCAIFADDR:
		maskIsNew = 0;
		hostIsNew = 1;
		error = 0;
		if (ia->ia_addr.sin_family == AF_INET) {
			if (ifra->ifra_addr.sin_len == 0) {
				ifra->ifra_addr = ia->ia_addr;
				hostIsNew = 0;
			} else if (ifra->ifra_addr.sin_addr.s_addr ==
					       ia->ia_addr.sin_addr.s_addr)
				hostIsNew = 0;
		}
		if (ifra->ifra_mask.sin_len) {
			in_ifscrub(ifp, ia);
			ia->ia_sockmask = ifra->ifra_mask;
			ia->ia_sockmask.sin_family = AF_INET;
			ia->ia_subnetmask =
			     ntohl(ia->ia_sockmask.sin_addr.s_addr);
			maskIsNew = 1;
		}
		if ((ifp->if_flags & IFF_POINTOPOINT) &&
		    (ifra->ifra_dstaddr.sin_family == AF_INET)) {
			in_ifscrub(ifp, ia);
			ia->ia_dstaddr = ifra->ifra_dstaddr;
			maskIsNew  = 1; /* We lie; but the effect's the same */
		}
		if (ifra->ifra_addr.sin_family == AF_INET &&
		    (hostIsNew || maskIsNew))
			error = in_ifinit(ifp, ia, &ifra->ifra_addr, 0);
		if (error != 0 && iaIsNew)
			break;

		if ((ifp->if_flags & IFF_BROADCAST) &&
		    (ifra->ifra_broadaddr.sin_family == AF_INET))
			ia->ia_broadaddr = ifra->ifra_broadaddr;
		if (error == 0) {
			if (iaIsFirst && (ifp->if_flags & IFF_MULTICAST) != 0)
				in_addmulti(&allhosts_addr, ifp);
			EVENTHANDLER_INVOKE(ifaddr_event, ifp);
		}
		return (error);

	case SIOCDIFADDR:
		/*
		 * in_ifscrub kills the interface route.
		 */
		in_ifscrub(ifp, ia);
		/*
		 * in_ifadown gets rid of all the rest of
		 * the routes.  This is not quite the right
		 * thing to do, but at least if we are running
		 * a routing process they will come back.
		 */
		in_ifadown(&ia->ia_ifa, 1);
		EVENTHANDLER_INVOKE(ifaddr_event, ifp);
		error = 0;
		break;

	default:
		if (ifp == 0 || ifp->if_ioctl == 0)
			return (EOPNOTSUPP);
		IFF_LOCKGIANT(ifp);
		error = (*ifp->if_ioctl)(ifp, cmd, data);
		IFF_UNLOCKGIANT(ifp);
		return (error);
	}

	/*
	 * Protect from ipintr() traversing address list while we're modifying
	 * it.
	 */
	s = splnet();
	TAILQ_REMOVE(&ifp->if_addrhead, &ia->ia_ifa, ifa_link);
	TAILQ_REMOVE(&in_ifaddrhead, ia, ia_link);
	if (ia->ia_addr.sin_family == AF_INET) {
		LIST_REMOVE(ia, ia_hash);
		/*
		 * If this is the last IPv4 address configured on this
		 * interface, leave the all-hosts group.
		 * XXX: This is quite ugly because of locking and structure.
		 */
		oia = NULL;
		IFP_TO_IA(ifp, oia);
		if (oia == NULL) {
			struct in_multi *inm;

			IFF_LOCKGIANT(ifp);
			IN_MULTI_LOCK();
			IN_LOOKUP_MULTI(allhosts_addr, ifp, inm);
			if (inm != NULL)
				in_delmulti_locked(inm);
			IN_MULTI_UNLOCK();
			IFF_UNLOCKGIANT(ifp);
		}
	}
	IFAFREE(&ia->ia_ifa);
	splx(s);

	return (error);
}

/*
 * Initialize an interface's internet address
 * and routing table entry.
 */
static int
in_ifinit(struct ifnet *ifp, struct in_ifaddr *ia, struct sockaddr_in *sin,
    int scrub)
{
	register u_long i = ntohl(sin->sin_addr.s_addr);
	struct sockaddr_in oldaddr;
	int s = splimp(), flags = RTF_UP, error = 0;

	oldaddr = ia->ia_addr;
	if (oldaddr.sin_family == AF_INET)
		LIST_REMOVE(ia, ia_hash);
	ia->ia_addr = *sin;
	if (ia->ia_addr.sin_family == AF_INET)
		LIST_INSERT_HEAD(INADDR_HASH(ia->ia_addr.sin_addr.s_addr),
		    ia, ia_hash);
	/*
	 * Give the interface a chance to initialize
	 * if this is its first address,
	 * and to validate the address if necessary.
	 */
	if (ifp->if_ioctl) {
		IFF_LOCKGIANT(ifp);
		error = (*ifp->if_ioctl)(ifp, SIOCSIFADDR, (caddr_t)ia);
		IFF_UNLOCKGIANT(ifp);
		if (error) {
			splx(s);
			/* LIST_REMOVE(ia, ia_hash) is done in in_control */
			ia->ia_addr = oldaddr;
			if (ia->ia_addr.sin_family == AF_INET)
				LIST_INSERT_HEAD(INADDR_HASH(
				    ia->ia_addr.sin_addr.s_addr), ia, ia_hash);
			return (error);
		}
	}
	splx(s);
	if (scrub) {
		ia->ia_ifa.ifa_addr = (struct sockaddr *)&oldaddr;
		in_ifscrub(ifp, ia);
		ia->ia_ifa.ifa_addr = (struct sockaddr *)&ia->ia_addr;
	}
	if (IN_CLASSA(i))
		ia->ia_netmask = IN_CLASSA_NET;
	else if (IN_CLASSB(i))
		ia->ia_netmask = IN_CLASSB_NET;
	else
		ia->ia_netmask = IN_CLASSC_NET;
	/*
	 * The subnet mask usually includes at least the standard network part,
	 * but may may be smaller in the case of supernetting.
	 * If it is set, we believe it.
	 */
	if (ia->ia_subnetmask == 0) {
		ia->ia_subnetmask = ia->ia_netmask;
		ia->ia_sockmask.sin_addr.s_addr = htonl(ia->ia_subnetmask);
	} else
		ia->ia_netmask &= ia->ia_subnetmask;
	ia->ia_net = i & ia->ia_netmask;
	ia->ia_subnet = i & ia->ia_subnetmask;
	in_socktrim(&ia->ia_sockmask);
#ifdef DEV_CARP
	/*
	 * XXX: carp(4) does not have interface route
	 */
	if (ifp->if_type == IFT_CARP)
		return (0);
#endif
	/*
	 * Add route for the network.
	 */
	ia->ia_ifa.ifa_metric = ifp->if_metric;
	if (ifp->if_flags & IFF_BROADCAST) {
		ia->ia_broadaddr.sin_addr.s_addr =
			htonl(ia->ia_subnet | ~ia->ia_subnetmask);
		ia->ia_netbroadcast.s_addr =
			htonl(ia->ia_net | ~ ia->ia_netmask);
	} else if (ifp->if_flags & IFF_LOOPBACK) {
		ia->ia_dstaddr = ia->ia_addr;
		flags |= RTF_HOST;
	} else if (ifp->if_flags & IFF_POINTOPOINT) {
		if (ia->ia_dstaddr.sin_family != AF_INET)
			return (0);
		flags |= RTF_HOST;
	}
	if ((error = in_addprefix(ia, flags)) != 0)
		return (error);

	return (error);
}



//---------------------------------------------------------------------
// working functions
//---------------------------------------------------------------------
struct in_aliasreq cfg_ifra;
#define SP_IFNAMSIZ 16
struct sp_cfg_ifnodeop {
//	spcfghead sch;
	char	ifra_name[SP_IFNAMSIZ];		/* if name, e.g. "en0" */
	unsigned long ifra_addr;		// network order
	unsigned long ifra_broadaddr;		// network order
	unsigned long ifra_mask;	// network order
};
extern struct inpcbinfo ripcbinfo;
extern struct inpcbinfo udbinfo;



int sp_addiptoif(struct sp_cfg_ifnodeop * p);
int sp_addiptoif(struct sp_cfg_ifnodeop * p)
{
	//struct in_aliasreq data;
	register struct ifreq *ifr = (struct ifreq *)&cfg_ifra;
	register struct in_ifaddr *ia = 0, *iap;
	register struct ifaddr *ifa;
	struct in_addr dst;
	struct in_ifaddr *oia;
	struct in_aliasreq *ifra = (struct in_aliasreq *)&cfg_ifra;
	int error, hostIsNew, iaIsNew, maskIsNew, s;
	struct ifnet *ifp, * ifp1;


	strcpy(cfg_ifra.ifra_name, p->ifra_name);
	cfg_ifra.ifra_addr.sin_len=sizeof(struct sockaddr_in);
	cfg_ifra.ifra_addr.sin_family=AF_INET;
	cfg_ifra.ifra_addr.sin_addr.s_addr=p->ifra_addr;
	cfg_ifra.ifra_mask.sin_len=sizeof(struct sockaddr_in);
	cfg_ifra.ifra_mask.sin_family=AF_INET;
	cfg_ifra.ifra_mask.sin_addr.s_addr=p->ifra_mask;
	cfg_ifra.ifra_broadaddr.sin_len=sizeof(struct sockaddr_in);
	cfg_ifra.ifra_broadaddr.sin_family=AF_INET;
	cfg_ifra.ifra_broadaddr.sin_addr.s_addr=p->ifra_broadaddr;

/*struct	in_aliasreq {
	char	ifra_name[IFNAMSIZ];		/ * if name, e.g. "en0" * /
	struct	sockaddr_in ifra_addr;
	struct	sockaddr_in ifra_broadaddr;
#define ifra_dstaddr ifra_broadaddr
	struct	sockaddr_in ifra_mask;
};
struct sockaddr_in {
	uint8_t	sin_len;
	sa_family_t	sin_family;
	in_port_t	sin_port;
	struct	in_addr sin_addr;
	char	sin_zero[8];
};

	strcpy(data.ifra_name, "fxp0");
	data.ifra_addr.sin_len=sizeof(struct sockaddr_in);
	data.ifra_addr.sin_family=AF_INET;
	data.ifra_addr.sin_addr.s_addr=0xb20ffb0a;
	data.ifra_mask.sin_len=sizeof(struct sockaddr_in);
	data.ifra_mask.sin_family=AF_INET;
	data.ifra_mask.sin_addr.s_addr=0x00ffffff;
	data.ifra_broadaddr.sin_len=sizeof(struct sockaddr_in);
	data.ifra_broadaddr.sin_family=AF_INET;
	data.ifra_broadaddr.sin_addr.s_addr=0xFF0ffb0a;*/
	 
	/* Create nodes for any already-existing gif interfaces */
	ifp=NULL;
	IFNET_RLOCK();
	TAILQ_FOREACH(ifp1, &ifnet, if_link) {
		if(strcmp(ifp1->if_xname, ifr->ifr_name)==0) {
			ifp=ifp1;
			break;
		}
	}
	IFNET_RUNLOCK();
	if (ifp == NULL)
		return (EADDRNOTAVAIL);

	iaIsNew = 0;

	/*
	 * Find address for this interface, if it exists.
	 *
	 * If an alias address was specified, find that one instead of
	 * the first one on the interface, if possible.
	 */
		dst = ((struct sockaddr_in *)&ifr->ifr_addr)->sin_addr;
		LIST_FOREACH(iap, INADDR_HASH(dst.s_addr), ia_hash)
			if (iap->ia_ifp == ifp &&
			    iap->ia_addr.sin_addr.s_addr == dst.s_addr) {
				ia = iap;
				break;
			}
		if (ia == NULL)
			TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
				iap = ifatoia(ifa);
				if (iap->ia_addr.sin_family == AF_INET) {
					ia = iap;
					break;
				}
			}

		if (ifra->ifra_addr.sin_family == AF_INET) {
			for (oia = ia; ia; ia = TAILQ_NEXT(ia, ia_link)) {
				if (ia->ia_ifp == ifp  &&
				    ia->ia_addr.sin_addr.s_addr ==
				    ifra->ifra_addr.sin_addr.s_addr)
					break;
			}
			if ((ifp->if_flags & IFF_POINTOPOINT)
			    && (ifra->ifra_dstaddr.sin_addr.s_addr
				== INADDR_ANY)) {
				return EDESTADDRREQ;
			}
		}

		if (ia == (struct in_ifaddr *)0) {
			ia = (struct in_ifaddr *)
				malloc(sizeof *ia, M_IFADDR, M_WAITOK | M_ZERO);
			if (ia == (struct in_ifaddr *)NULL)
				return (ENOBUFS);
			/*
			 * Protect from ipintr() traversing address list
			 * while we're modifying it.
			 */
			s = splnet();
			TAILQ_INSERT_TAIL(&in_ifaddrhead, ia, ia_link);

			ifa = &ia->ia_ifa;
			IFA_LOCK_INIT(ifa);
			ifa->ifa_addr = (struct sockaddr *)&ia->ia_addr;
			ifa->ifa_dstaddr = (struct sockaddr *)&ia->ia_dstaddr;
			ifa->ifa_netmask = (struct sockaddr *)&ia->ia_sockmask;
			ifa->ifa_refcnt = 1;
			TAILQ_INSERT_TAIL(&ifp->if_addrhead, ifa, ifa_link);

			ia->ia_sockmask.sin_len = 8;
			ia->ia_sockmask.sin_family = AF_INET;
			if (ifp->if_flags & IFF_BROADCAST) {
				ia->ia_broadaddr.sin_len = sizeof(ia->ia_addr);
				ia->ia_broadaddr.sin_family = AF_INET;
			}
			ia->ia_ifp = ifp;
			splx(s);
			iaIsNew = 1;
		}

		maskIsNew = 0;
		hostIsNew = 1;
		error = 0;
		if (ia->ia_addr.sin_family == AF_INET) {
			if (ifra->ifra_addr.sin_len == 0) {
				ifra->ifra_addr = ia->ia_addr;
				hostIsNew = 0;
			} else if (ifra->ifra_addr.sin_addr.s_addr ==
					       ia->ia_addr.sin_addr.s_addr)
				hostIsNew = 0;
		}
		if (ifra->ifra_mask.sin_len) {
			in_ifscrub(ifp, ia);
			ia->ia_sockmask = ifra->ifra_mask;
			ia->ia_sockmask.sin_family = AF_INET;
			ia->ia_subnetmask =
			     ntohl(ia->ia_sockmask.sin_addr.s_addr);
			maskIsNew = 1;
		}
		if ((ifp->if_flags & IFF_POINTOPOINT) &&
		    (ifra->ifra_dstaddr.sin_family == AF_INET)) {
			in_ifscrub(ifp, ia);
			ia->ia_dstaddr = ifra->ifra_dstaddr;
			maskIsNew  = 1; /* We lie; but the effect's the same */
		}
		if (ifra->ifra_addr.sin_family == AF_INET &&
		    (hostIsNew || maskIsNew))
			error = in_ifinit(ifp, ia, &ifra->ifra_addr, 0);
		if (error != 0 && iaIsNew)
			goto done;

		if ((ifp->if_flags & IFF_BROADCAST) &&
		    (ifra->ifra_broadaddr.sin_family == AF_INET))
			ia->ia_broadaddr = ifra->ifra_broadaddr;
		if (error == 0)
			EVENTHANDLER_INVOKE(ifaddr_event, ifp);
		return (error);

done:
	/*
	 * Protect from ipintr() traversing address list while we're modifying
	 * it.
	 */
	s = splnet();
	TAILQ_REMOVE(&ifp->if_addrhead, &ia->ia_ifa, ifa_link);
	TAILQ_REMOVE(&in_ifaddrhead, ia, ia_link);
	LIST_REMOVE(ia, ia_hash);
	IFAFREE(&ia->ia_ifa);
	splx(s);

	return (error);
}


int sp_delipfromif(void);
int sp_delipfromif(void)
{
	//struct in_aliasreq data;
	register struct ifreq *ifr = (struct ifreq *)&cfg_ifra;
	register struct in_ifaddr *ia = 0, *iap;
	register struct ifaddr *ifa;
	struct in_addr dst;
	struct in_ifaddr *oia;
	struct in_aliasreq *ifra = (struct in_aliasreq *)&cfg_ifra;
	int error, iaIsNew, s;
	struct ifnet *ifp, * ifp1;

	/*strcpy(cfg_ifra.ifra_name, p->ifra_name);
	cfg_ifra.ifra_addr.sin_len=sizeof(struct sockaddr_in);
	cfg_ifra.ifra_addr.sin_family=AF_INET;
	cfg_ifra.ifra_addr.sin_addr.s_addr=p->ifra_addr;
	cfg_ifra.ifra_mask.sin_len=sizeof(struct sockaddr_in);
	cfg_ifra.ifra_mask.sin_family=AF_INET;
	cfg_ifra.ifra_mask.sin_addr.s_addr=p->ifra_mask;
	cfg_ifra.ifra_broadaddr.sin_len=sizeof(struct sockaddr_in);
	cfg_ifra.ifra_broadaddr.sin_family=AF_INET;
	cfg_ifra.ifra_broadaddr.sin_addr.s_addr=p->ifra_broadaddr;*/

	/*strcpy(data.ifra_name, "fxp0");
	data.ifra_addr.sin_len=sizeof(struct sockaddr_in);
	data.ifra_addr.sin_family=AF_INET;
	data.ifra_addr.sin_addr.s_addr=0xb20ffb0a;
	data.ifra_mask.sin_len=sizeof(struct sockaddr_in);
	data.ifra_mask.sin_family=AF_INET;
	data.ifra_mask.sin_addr.s_addr=0x00ffffff;
	data.ifra_broadaddr.sin_len=sizeof(struct sockaddr_in);
	data.ifra_broadaddr.sin_family=AF_INET;
	data.ifra_broadaddr.sin_addr.s_addr=0xFF0ffb0a;*/
	 
	/* Create nodes for any already-existing gif interfaces */
	ifp=NULL;
	IFNET_RLOCK();
	TAILQ_FOREACH(ifp1, &ifnet, if_link) {
		if(strcmp(ifp1->if_xname, ifr->ifr_name)==0) {
			ifp=ifp1;
			break;
		}
	}
	IFNET_RUNLOCK();
	if (ifp == 0)
		return (EADDRNOTAVAIL);

	iaIsNew = 0;

	/*
	 * Find address for this interface, if it exists.
	 *
	 * If an alias address was specified, find that one instead of
	 * the first one on the interface, if possible.
	 */
		dst = ((struct sockaddr_in *)&ifr->ifr_addr)->sin_addr;
		LIST_FOREACH(iap, INADDR_HASH(dst.s_addr), ia_hash)
			if (iap->ia_ifp == ifp &&
			    iap->ia_addr.sin_addr.s_addr == dst.s_addr) {
				ia = iap;
				break;
			}
		if (ia == NULL)
			TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
				iap = ifatoia(ifa);
				if (iap->ia_addr.sin_family == AF_INET) {
					ia = iap;
					break;
				}
			}

		if (ifra->ifra_addr.sin_family == AF_INET) {
			for (oia = ia; ia; ia = TAILQ_NEXT(ia, ia_link)) {
				if (ia->ia_ifp == ifp  &&
				    ia->ia_addr.sin_addr.s_addr ==
				    ifra->ifra_addr.sin_addr.s_addr)
					break;
			}
		}
		if ( ia == 0)
			return (EADDRNOTAVAIL);

		if (ia == (struct in_ifaddr *)0) {
			ia = (struct in_ifaddr *)
				malloc(sizeof *ia, M_IFADDR, M_WAITOK | M_ZERO);
			if (ia == (struct in_ifaddr *)NULL)
				return (ENOBUFS);
			/*
			 * Protect from ipintr() traversing address list
			 * while we're modifying it.
			 */
			s = splnet();
			TAILQ_INSERT_TAIL(&in_ifaddrhead, ia, ia_link);

			ifa = &ia->ia_ifa;
			IFA_LOCK_INIT(ifa);
			ifa->ifa_addr = (struct sockaddr *)&ia->ia_addr;
			ifa->ifa_dstaddr = (struct sockaddr *)&ia->ia_dstaddr;
			ifa->ifa_netmask = (struct sockaddr *)&ia->ia_sockmask;
			ifa->ifa_refcnt = 1;
			TAILQ_INSERT_TAIL(&ifp->if_addrhead, ifa, ifa_link);

			ia->ia_sockmask.sin_len = 8;
			ia->ia_sockmask.sin_family = AF_INET;
			if (ifp->if_flags & IFF_BROADCAST) {
				ia->ia_broadaddr.sin_len = sizeof(ia->ia_addr);
				ia->ia_broadaddr.sin_family = AF_INET;
			}
			ia->ia_ifp = ifp;
			splx(s);
			iaIsNew = 1;
		}

		/*
		 * in_ifscrub kills the interface route.
		 */
		in_ifscrub(ifp, ia);
		/*
		 * in_ifadown gets rid of all the rest of
		 * the routes.  This is not quite the right
		 * thing to do, but at least if we are running
		 * a routing process they will come back.
		 */
		in_ifadown(&ia->ia_ifa, 1);
		/*
		 * XXX horrible hack to detect that we are being called
		 * from if_detach()
		 */
		if (ifaddr_byindex(ifp->if_index) == NULL) {
			in_pcbpurgeif0(&ripcbinfo, ifp);
			in_pcbpurgeif0(&udbinfo, ifp);
		}
		EVENTHANDLER_INVOKE(ifaddr_event, ifp);
		error = 0;

	/*
	 * Protect from ipintr() traversing address list while we're modifying
	 * it.
	 */
	s = splnet();
	TAILQ_REMOVE(&ifp->if_addrhead, &ia->ia_ifa, ifa_link);
	TAILQ_REMOVE(&in_ifaddrhead, ia, ia_link);
	LIST_REMOVE(ia, ia_hash);
	IFAFREE(&ia->ia_ifa);
	splx(s);


	cfg_ifra.ifra_name[0]=0;
	cfg_ifra.ifra_addr.sin_addr.s_addr=0;

	return (error);
}

#endif // MAXHE_TODO
