/*-
 * Copyright (c) 1995 Terrence R. Lambert
 * All rights reserved.
 *
 * Copyright (c) 1982, 1986, 1989, 1991, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)init_main.c	8.9 (Berkeley) 1/21/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/kern/init_main.c,v 1.283.2.2 2007/12/14 13:41:08 rrs Exp $");

#include "opt_ddb.h"
#include "opt_init_path.h"
#include "opt_mac.h"
#include "opt_printf.h"
#include "opt_compat.h"
#include "opt_inet6.h"
#include "opt_inet.h"
#include "opt_carp.h"


#include <sys/param.h>
#include <machine/endian.h>
#include <sys/kernel.h>
#include <sys/exec.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/vnode.h>
#include <sys/sysent.h>
#include <sys/reboot.h>
#include <sys/sched.h>
#include <sys/sx.h>
#include <sys/sysproto.h>
#include <sys/vmmeter.h>
#include <sys/unistd.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/domain.h>
#include <sys/time.h>
#include <sys/eventhandler.h>
#include <sys/random.h>
#include <sys/jail.h>

#include <machine/cpu.h>

#include <security/audit/audit.h>
#include <security/mac/mac_framework.h>

/*
#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_param.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/uma.h>
#include <vm/uma_int.h>
#include <vm/uma_dbg.h>
#include <vm/pmap.h>
*/
//#include <sys/copyright.h>
#include <sys/uio.h>

#include <sys/msgbuf.h>
#include <sys/malloc.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/stddef.h>
#include <sys/sysctl.h>
#include <sys/tty.h>
#include <sys/syslog.h>
#include <sys/cons.h>
#include <sys/ctype.h>
#include <sys/protosw.h>
#include <sys/sx.h>



#include <ddb/ddb.h>
#include <ddb/db_sym.h>

#include <net/netisr.h>
#include <net/if.h>
#include <net/if_clone.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/radix.h>
#include <net/route.h>
#include <net/pfil.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_encap.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/sbuf.h>
#include <sys/bus.h>
#include <sys/mbuf.h>
#include <sys/systm.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/kernel.h>
#include <sys/sockio.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <sys/domain.h>
#include <sys/jail.h>
#include <machine/stdarg.h>

#include <net/if.h>
#include <net/if_clone.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/radix.h>
#include <net/route.h>

#if defined(INET) || defined(INET6)
/*XXX*/
#include <netinet/in.h>
#include <netinet/in_var.h>
#ifdef INET6
#include <netinet6/in6_var.h>
#include <netinet6/in6_ifattach.h>
#endif
#endif
#ifdef INET
#include <netinet/if_ether.h>
#endif
#ifdef DEV_CARP
#include <netinet/ip_carp.h>
#endif

#include <security/mac/mac_framework.h>

#include <nknshim/nkns_linux.h>
#include <nknshim/nkns_osl.h>

struct sysctl_oid_list sysctl__net_children;
struct sysctl_oid_list sysctl__kern_children;
struct sysctl_oid_list sysctl__security_bsd_children;


//int	bootverbose;
struct msgbuf *msgbufp = 0;

int        jail_socket_unixiproute_only;

struct malloc_type M_IFADDR[1];
struct malloc_type M_IOV[1];
struct malloc_type M_TEMP[1];
struct malloc_type M_RTABLE[1];

time_t     time_second;
time_t     time_uptime;
int tick;                        /* usec per tick (1000000 / hz) */
int hz;                          /* system clock's frequency */
int psratio;                     /* ratio: prof / stat */
int stathz;                      /* statistics clock's frequency */
int profhz;                      /* profiling clock's frequency */
int profprocs;                   /* number of process's profiling */
int ticks;
int log_open;

int curcpu=0;

//int securelevel;         /* system security level (see init(8)) */
int cold;                /* nonzero if we are doing a cold boot */
int rebooting;           /* boot() has been called. */
int kstack_pages;        /* number of kernel stack pages */
int nswap;               /* size of swap space */
u_int nselcoll;          /* select collisions since boot */
struct mtx sellock;      /* select lock variable */
long physmem;            /* physical memory */
long realmem;            /* 'real' memory */
char *rootdevnames[2];   /* names of possible root devices */
int maxusers;            /* system tune hint */

int maxfiles;            /* kernel limit on number of open files */
int maxfilesperproc;     /* per process limit on number of open files */
int openfiles;           /* (fl) actual number of open files */

struct msgbuf consmsgbuf; /* Message buffer for constty. */
struct tty *constty;    /* Temporary virtual console. */
long tk_cancc;
long tk_nin;
long tk_nout;
long tk_rawcc;

struct mtx Giant;
//struct mtx blocked_lock;

struct ifnethead ifnet;
struct ifindex_entry *ifindex_table;
int ifqmaxlen;
struct ifnet *loif;     /* first loopback interface */
int if_index;


int kdb_active;                  /* Non-zero while in debugger. */
int debugger_on_panic;           /* enter the debugger on panic. */
struct kdb_dbbe *kdb_dbbe;       /* Default debugger backend or NULL. */
struct trapframe *kdb_frame;     /* Frame to kdb_trap(). */
struct pcb *kdb_thrctx;          /* Current context. */
struct thread *kdb_thread;       /* Current thread. */

const char *panicstr=NULL;     /* panic message */

struct thread thread0;           /* Primary thread in proc0. */

int ncallout = 1000; //16 + maxproc + maxfiles;




void panic(const char *cfmt, ...) 
{
/*
        int retval;
        va_list ap;
	char buf[1024];

        va_start(ap, cfmt);
        retval = sprintf(buf, cfmt, ap);
        buf[retval] = '\0';
        va_end(ap);

	printf("PANIC: %s\n", buf);
*/
	printf("PANIC: %s\n", cfmt);

	nkn_exit (1);

	// Never reach here
}

int uiomove(void *cp, int n, struct uio *uio)
{
	struct iovec *iov;
	u_int cnt;
	int error = 0;

	KASSERT(uio->uio_rw == UIO_READ || uio->uio_rw == UIO_WRITE,
	    ("uiomove: mode"));
	KASSERT(uio->uio_segflg != UIO_USERSPACE || uio->uio_td == curthread,
	    ("uiomove proc"));
	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
	    "Calling uiomove()");

	while (n > 0 && uio->uio_resid) {
		iov = uio->uio_iov;
		cnt = iov->iov_len;
		if (cnt == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			continue;
		}
		if (cnt > n)
			cnt = n;

		switch (uio->uio_segflg) {

		case UIO_USERSPACE:
#ifdef MAXHE_TODO
			if (ticks - PCPU_GET(switchticks) >= hogticks)
				uio_yield();
#endif // MAXHE_TODO
			if (uio->uio_rw == UIO_READ)
				error = copyout(cp, iov->iov_base, cnt);
			else
				error = copyin(iov->iov_base, cp, cnt);
			if (error)
				goto out;
			break;

		case UIO_SYSSPACE:
			if (uio->uio_rw == UIO_READ)
				bcopy(cp, iov->iov_base, cnt);
			else
				bcopy(iov->iov_base, cp, cnt);
			break;
		case UIO_NOCOPY:
			break;
		}
		iov->iov_base = (char *)iov->iov_base + cnt;
		iov->iov_len -= cnt;
		uio->uio_resid -= cnt;
		uio->uio_offset += cnt;
		cp = (char *)cp + cnt;
		n -= cnt;
	}
out:
	return (error);
}


int     priv_check_cred(struct ucred *cred, int priv, int flags)
{
	DBG_OSL(("WARNING: priv_check_cred always returns 0 means OK\n"));
	return 0;
}
int     priv_check(struct thread *td, int priv)
{
	return priv_check_cred(NULL, priv, 0);
}
int jailed(struct ucred *cred)
{
	DBG_OSL(("WARNING: jailed. No ip is in jail in this design\n"));
	return 0;
}
u_int32_t prison_getip(struct ucred *cred)
{
	DBG_OSL(("ERROR: prison_getip\n"));
	return 0;
}
int prison_ip(struct ucred *cred, int flag, u_int32_t *ip)
{
	DBG_OSL(("ERROR: prison_ip\n"));
	return 0;
}
void prison_remote_ip(struct ucred *cred, int flags, u_int32_t *ip)
{
	DBG_OSL(("ERROR: prison_remote_ip\n"));
}

/////////////////////////////////////////////////////////////
// functions from uma_core.c
/////////////////////////////////////////////////////////////

int         tputchar(int c, struct tty *tp)
{
	DBG_OSL(("ERROR: tputchar\n"));
	return 0;
}

int         ttycheckoutq(struct tty *tp, int wait)
{
	DBG_OSL(("ERROR: ttycheckoutq\n"));
	return 0;
}



/*
 * pfil_run_hooks() runs the specified packet filter hooks.
 */
int
pfil_run_hooks(struct pfil_head *ph, struct mbuf **mp, struct ifnet *ifp,
    int dir, struct inpcb *inp)
{
	int rv = 0;
	DBG_OSL(("ERROR:  pfil_head_register\n"));

	return (rv);
}

/*
 * pfil_head_register() registers a pfil_head with the packet filter
 * hook mechanism.
 */
int
pfil_head_register(struct pfil_head *ph)
{
	DBG_OSL(("ERROR:  pfil_head_register\n"));
	return (0);
}

/*
 * pfil_head_unregister() removes a pfil_head from the packet filter
 * hook mechanism.
 */
int
pfil_head_unregister(struct pfil_head *ph)
{
	DBG_OSL(("ERROR:  pfil_head_unregister\n"));
	return (0);
}

/*
 * pfil_head_get() returns the pfil_head for a given key/dlt.
 */
struct pfil_head *
pfil_head_get(int type, u_long val)
{
	struct pfil_head *ph=NULL;

	DBG_OSL(("ERROR:  pfil_head_get\n"));

	return (ph);
}

/*
 * pfil_add_hook() adds a function to the packet filter hook.  the
 * flags are:
 *	PFIL_IN		call me on incoming packets
 *	PFIL_OUT	call me on outgoing packets
 *	PFIL_ALL	call me on all of the above
 *	PFIL_WAITOK	OK to call malloc with M_WAITOK.
 */
int
pfil_add_hook(int (*func)(void *, struct mbuf **, struct ifnet *, int, struct inpcb *),
    void *arg, int flags, struct pfil_head *ph)
{
	int err=0;

	DBG_OSL(("ERROR:  pfil_add_hook\n"));

	return err;
}

/*
 * pfil_remove_hook removes a specific function from the packet filter
 * hook list.
 */
int
pfil_remove_hook(int (*func)(void *, struct mbuf **, struct ifnet *, int, struct inpcb *),
    void *arg, int flags, struct pfil_head *ph)
{
	int err = 0;

	DBG_OSL(("ERROR:  pfil_remove_hook\n"));
	return err;
}

void     wakeup(void *chan) 
{
	// We should do nothing here.
	// In fact, this function better not called by anybody.
	DBG_OSL(("ERROR:  wakeup() called. Recommended not be called by anybody\n"));
}

void     wakeup_one(void *chan) 
{
	DBG_OSL(("ERROR:  wakeup_one\n"));
}

void   selwakeuppri(struct selinfo *sip, int pri)
{
	DBG_OSL(("ERROR:  selwakeuppri\n"));
	return;
}

void      sessrele(struct session * sess)
{
	DBG_OSL(("ERROR:  sessrele\n"));
}

int      cr_cansee(struct ucred *u1, struct ucred *u2)
{
	DBG_OSL(("ERROR:   cr_cansee\n"));
	return (0);
}

int      cr_canseesocket(struct ucred *cred, struct socket *so)
{
	DBG_OSL(("ERROR:   cr_canseesocket\n"));
	return (0);
}


/*
void tunable_int_init(void *data)
{
	DBG_OSL(("ERROR:   tunable_int_init\n"));
}
*/

void    sx_sysinit(void *arg)
{
	DBG_OSL(("ERROR:   sx_sysinit\n"));
}

void    sx_init_flags(struct sx *sx, const char *description, int opts)
{
	DBG_OSL(("ERROR:   sx_init_flags\n"));
}

void    sx_destroy(struct sx *sx)
{
	DBG_OSL(("ERROR:   sx_destroy\n"));
}

int     _sx_slock(struct sx *sx, int opts, const char *file, int line)
{
	DBG_OSL(("ERROR:   _sx_slock\n"));
	return 0;
}

int     _sx_xlock(struct sx *sx, int opts, const char *file, int line)
{
	DBG_OSL(("ERROR:   _sx_xlock\n"));
	return 0;
}

int     _sx_try_slock(struct sx *sx, const char *file, int line)
{
	DBG_OSL(("ERROR:   _sx_try_slock\n"));
	return 0;
}

int     _sx_try_xlock(struct sx *sx, const char *file, int line)
{
	DBG_OSL(("ERROR:   _sx_try_xlock\n"));
	return 0;
}

void    _sx_sunlock(struct sx *sx, const char *file, int line)
{
	DBG_OSL(("ERROR:   _sx_sunlock\n"));
}

void    _sx_xunlock(struct sx *sx, const char *file, int line)
{
	DBG_OSL(("ERROR:   _sx_xunlock\n"));
}

int     _sx_try_upgrade(struct sx *sx, const char *file, int line)
{
	DBG_OSL(("ERROR:   _sx_try_upgrade\n"));
	return 0;
}

void    _sx_downgrade(struct sx *sx, const char *file, int line)
{
	DBG_OSL(("ERROR:   _sx_downgrade\n"));
}

int     _sx_xlock_hard(struct sx *sx, uintptr_t tid, int opts,
            const char *file, int line)
{
	DBG_OSL(("ERROR:   _sx_xlock_hard\n"));
	return 0;
}

int     _sx_slock_hard(struct sx *sx, int opts, const char *file, int line)
{
	DBG_OSL(("ERROR:   _sx_slock_hard\n"));
	return 0;
}

void    _sx_xunlock_hard(struct sx *sx, uintptr_t tid, const char *file, int
            line)
{
	DBG_OSL(("ERROR:   _sx_xunlock_hard\n"));
}

void    _sx_sunlock_hard(struct sx *sx, const char *file, int line)
{
	DBG_OSL(("ERROR:   _sx_sunlock_hard\n"));
}

void     knote(struct knlist *list, long hint, int islocked)
{
	DBG_OSL(("ERROR:   knote\n"));
}
void     knlist_add(struct knlist *knl, struct knote *kn, int islocked)
{
	DBG_OSL(("ERROR:   knlist_add\n"));
}
void     knlist_remove(struct knlist *knl, struct knote *kn, int islocked)
{
	DBG_OSL(("ERROR:   knlist_remove\n"));
}
void     knlist_remove_inevent(struct knlist *knl, struct knote *kn)
{
	DBG_OSL(("ERROR:   knlist_remove_inevent\n"));
}
int      knlist_empty(struct knlist *knl)
{
	DBG_OSL(("ERROR:   knlist_empty\n"));
	return 0;
}
void     knlist_init(struct knlist *knl, void *lock,
    void (*kl_lock)(void *), void (*kl_unlock)(void *),
    int (*kl_locked)(void *))
{
	DBG_OSL(("ERROR:   knlist_init\n"));
}
void     knlist_destroy(struct knlist *knl)
{
	DBG_OSL(("ERROR:   knlist_destroy\n"));
}
void     knlist_cleardel(struct knlist *knl, struct thread *td,
        int islocked, int killkn)
{
	DBG_OSL(("ERROR:   knlist_cleardel\n"));
}
void     knote_fdclose(struct thread *p, int fd)
{
	DBG_OSL(("ERROR:   knote_fdclose\n"));
}
int      kqfd_register(int fd, struct kevent *kev, struct thread *p, int waitok)
{
	DBG_OSL(("ERROR:   kqfd_register\n"));
	return 0;
}

int      kqueue_add_filteropts(int filt, struct filterops *filtops)
{
	DBG_OSL(("ERROR:   kqueue_add_filteropts\n"));
	return 0;
}
int      kqueue_del_filteropts(int filt)
{
	DBG_OSL(("ERROR:   kqueue_del_filteropts\n"));
	return 0;
}


void    change_egid(struct ucred *newcred, gid_t egid)
{
	DBG_OSL(("ERROR:   change_egid\n"));
}
void    change_euid(struct ucred *newcred, struct uidinfo *euip)
{
	DBG_OSL(("ERROR:   change_euid\n"));
}
void    change_rgid(struct ucred *newcred, gid_t rgid)
{
	DBG_OSL(("ERROR:   change_rgid\n"));
}
void    change_ruid(struct ucred *newcred, struct uidinfo *ruip)
{
	DBG_OSL(("ERROR:   change_ruid\n"));
}
void    change_svgid(struct ucred *newcred, gid_t svgid)
{
	DBG_OSL(("ERROR:   change_svgid\n"));
}
void    change_svuid(struct ucred *newcred, uid_t svuid)
{
	DBG_OSL(("ERROR:   change_svuid\n"));
}
void    crcopy(struct ucred *dest, struct ucred *src)
{
	DBG_OSL(("ERROR:   crcopy\n"));
}
struct ucred    *crdup(struct ucred *cr)
{
	DBG_OSL(("ERROR:   crdup\n"));
	return NULL;
}
void    cred_update_thread(struct thread *td)
{
	DBG_OSL(("ERROR:   cred_update_thread\n"));
}
void    crfree(struct ucred *cr)
{
	DBG_OSL(("ERROR:   crfree\n"));
}
struct ucred    *crget(void)
{
	DBG_OSL(("ERROR:   crget\n"));
	return NULL;
}
struct ucred    *crhold(struct ucred *cr)
{
	DBG_OSL(("ERROR:   crhold\n"));
	return NULL;
}
int     crshared(struct ucred *cr)
{
	DBG_OSL(("ERROR:   crshared\n"));
	return 0;
}
void    cru2x(struct ucred *cr, struct xucred *xcr)
{
	DBG_OSL(("ERROR:   cru2x\n"));
}
int     groupmember(gid_t gid, struct ucred *cred)
{
	DBG_OSL(("ERROR:   groupmember\n"));
	return 0;
}

void rt_missmsg(int type, struct rt_addrinfo *rtinfo, int flags, int error)
{
	DBG_OSL(("ERROR:   rt_missmsg\n"));
}
void rt_newaddrmsg(int cmd, struct ifaddr *ifa, int error, struct rtentry *rt)
{
	DBG_OSL(("ERROR:   rt_newaddrmsg\n"));
}


void rn_init(void)
{
	DBG_OSL(("ERROR:   rn_init\n"));
}

void pgsigio(struct sigio ** sig, int signum, int checkctty)
{
	DBG_OSL(("ERROR:   pgsigio\n"));
}



void    in_rtqdrain(void)
{
	DBG_OSL(("ERROR:   in_rtqdrain\n"));
}
int     in_ifadown(struct ifaddr *ifa, int i)
{
	DBG_OSL(("ERROR:   in_ifadown\n"));
	return 0;
}
/*
void    in_ifscrub(struct ifnet * ifn, struct in_ifaddr * ifa)
{
	DBG_OSL(("ERROR:   in_ifscrub\n"));
}
*/
struct  mbuf    *ip_fastforward(struct mbuf * m)
{
	DBG_OSL(("ERROR:   ip_fastforward\n"));
	return NULL;
}


int     if_simloop(struct ifnet *ifp, struct mbuf *m, int af, int hlen)
{
	DBG_OSL(("ERROR:   if_simloop\n"));
	return 0;
}

struct  ifnet *ifunit(const char * s)
{
	DBG_OSL(("ERROR:    ifunit\n"));
	return NULL;
}

void      getmicrotime(struct timeval *tvp)
{
	DBG_OSL(("ERROR:    getmicrotime\n"));
}

void      getmicrouptime(struct timeval *tvp)
{
	DBG_OSL(("ERROR:    getmicrouptime\n"));
}

int
if_addmulti(struct ifnet *ifp, struct sockaddr *sa,
    struct ifmultiaddr **retifma)
{
	DBG_OSL(("ERROR:    if_addmulti\n"));
	return 0;
}

int
if_delmulti(struct ifnet *ifp, struct sockaddr *sa)
{
	DBG_OSL(("ERROR:    if_delmulti\n"));
	return 0;
}

void if_delmulti_ifma(struct ifmultiaddr *ifma)
{
	DBG_OSL(("ERROR:     if_delmulti_ifma\n"));
}


/*
 * Locate an interface based on a complete address.
 */
/*ARGSUSED*/
struct ifaddr *
ifa_ifwithaddr(struct sockaddr *addr)
{
	printf("ERROR:  ifa_ifwithaddr\n");
	return NULL;
}

/*
 * Locate an interface based on the broadcast address.
 */
/* ARGSUSED */
struct ifaddr *
ifa_ifwithbroadaddr(struct sockaddr *addr)
{
	printf("ERROR:  ifa_ifwithbroadaddr\n");
	return NULL;
}

/*
 * Locate the point to point interface with a given destination address.
 */
/*ARGSUSED*/
struct ifaddr *
ifa_ifwithdstaddr(struct sockaddr *addr)
{
	printf("ERROR:  ifa_ifwithdstaddr\n");
	return NULL;
}

/*
 * Find an interface on a specific network.  If many, choice
 * is most specific found.
 */
struct ifaddr *
ifa_ifwithnet(struct sockaddr *addr)
{
	printf("ERROR:  ifa_ifwithnet\n");
	return NULL;
}

/*
 * Find an interface address specific to an interface best matching
 * a given address.
 */
struct ifaddr *
ifaof_ifpforaddr(struct sockaddr *addr, struct ifnet *ifp)
{
	printf("ERROR:  ifaof_ifpforaddr\n");
	return NULL;
}


/*
int in_broadcast(struct in_addr in, struct ifnet *ifp)
{
	DBG_OSL(("ERROR:     in_broadcast\n"));
	return 0;
}
*/

int  in_inithead(void **head, int off);
int  in_inithead(void **head, int off)
{
	DBG_OSL(("ERROR:     in_inithead\n"));
	return 0;
}

u_int16_t   ip_randomid(void)
{
	DBG_OSL(("ERROR:     ip_randomid\n"));
	return 0;
}

eventhandler_tag eventhandler_register(struct eventhandler_list *list,
            const char *name, void *func, void *arg, int priority)
{
	DBG_OSL(("ERROR:     eventhandler_register\n"));
	return NULL;
}
void    eventhandler_deregister(struct eventhandler_list *list,
            eventhandler_tag tag)
{
	DBG_OSL(("ERROR:     eventhandler_deregister\n"));
}
struct eventhandler_list *eventhandler_find_list(const char *name)
{
	DBG_OSL(("ERROR:     eventhandler_find_list\n"));
	return NULL;
}
void    eventhandler_prune_list(struct eventhandler_list *list)
{
	DBG_OSL(("ERROR:     eventhandler_prune_list\n"));
}


/*
int      getenv_int(const char *name, int *data)
{
	nkn_print_trace();
	DBG_OSL(("ERROR:     getenv_int\n"));
	return 0;
}
*/

void    cninit(void)
{
	DBG_OSL(("ERROR:     cninit\n"));
}
void    cninit_finish(void)
{
	DBG_OSL(("ERROR:     cninit_finish\n"));
}
int     cnadd(struct consdev * c)
{
	DBG_OSL(("ERROR:     cnadd\n"));
	return 0;
}
void    cnavailable(struct consdev * c, int i)
{
	DBG_OSL(("ERROR:     cnavailable\n"));
}
void    cnremove(struct consdev * c)
{
	DBG_OSL(("ERROR:     cnremove\n"));
}
void    cnselect(struct consdev * c)
{
	DBG_OSL(("ERROR:     cnselect\n"));
}
int     cncheckc(void)
{
	DBG_OSL(("ERROR:     cncheckc\n"));
	return 0;
}
int     cngetc(void)
{
	DBG_OSL(("ERROR:     cngetc\n"));
	return 0;
}
void    cnputc(int i)
{
	DBG_OSL(("ERROR:     cnputc\n"));
}
void    cnputs(char * c)
{
	DBG_OSL(("ERROR:     cnputs\n"));
}
int     cnunavailable(void)
{
	DBG_OSL(("ERROR:     cnunavailable\n"));
	return 0;
}


void    hashdestroy(void * v, struct malloc_type * m, u_long u)
{
	DBG_OSL(("ERROR:     hashdestroy\n"));
}
/*
 * General routine to allocate a hash table with control of memory flags.
 */
void *
hashinit_flags(int elements, struct malloc_type *type, u_long *hashmask,
    int flags)
{
	long hashsize;
	LIST_HEAD(generic, generic) *hashtbl;
	int i;

	if (elements <= 0)
		panic("hashinit: bad elements");

	/* Exactly one of HASH_WAITOK and HASH_NOWAIT must be set. */
	KASSERT((flags & HASH_WAITOK) ^ (flags & HASH_NOWAIT),
	    ("Bad flags (0x%x) passed to hashinit_flags", flags));

	for (hashsize = 1; hashsize <= elements; hashsize <<= 1)
		continue;
	hashsize >>= 1;

	if (flags & HASH_NOWAIT)
		hashtbl = malloc((u_long)hashsize * sizeof(*hashtbl),
		    type, M_NOWAIT);
	else
		hashtbl = malloc((u_long)hashsize * sizeof(*hashtbl),
		    type, M_WAITOK);

	if (hashtbl != NULL) {
		for (i = 0; i < hashsize; i++)
			LIST_INIT(&hashtbl[i]);
		*hashmask = hashsize - 1;
	}
	return (hashtbl);
}

/*
 * Allocate and initialize a hash table with default flag: may sleep.
 */
void *
hashinit(int elements, struct malloc_type *type, u_long *hashmask)
{

	DBG_OSL(("WARNING:     hashinit called\n"));
	return (hashinit_flags(elements, type, hashmask, HASH_WAITOK));
}


void     funsetown(struct sigio **sigiop)
{
	DBG_OSL(("ERROR:     funsetown\n"));
	return;
}

int	falloc(struct thread *td, struct file **resultfp, int *resultfd)
{
	DBG_OSL(("ERROR:     falloc\n"));
	return 0;
}
int	fdalloc(struct thread *td, int minfd, int *result)
{
	DBG_OSL(("ERROR:     fdalloc\n"));
	return 0;
}
void	fdclose(struct filedesc *fdp, struct file *fp, int idx, struct thread *td)
{
	DBG_OSL(("ERROR:     fdclose\n"));
}
int fdrop(struct file *fp, struct thread *td)
{
	DBG_OSL(("ERROR:     fdrop\n"));
	return 0;
}

rlim_t      lim_cur(struct proc *p, int which)
{
	DBG_OSL(("ERROR:     lim_cur\n"));
	return 0;
}

struct uio *cloneuio(struct uio *uiop)
{
	DBG_OSL(("ERROR:     cloneuio\n"));
	return NULL;
}

int ppsratecheck(struct timeval *lasttime, int *curpps, int maxpps)
{
	//DBG_OSL(("ppsratecheck: no packet rate control\n"));
	return 1;
}

void      encap_init(void)
{
	DBG_OSL(("ERROR:     encap_init\n"));
}

void      encap4_input(struct mbuf * m, int i)
{
	DBG_OSL(("ERROR:     encap4_input\n"));
}

int      chgsbsize(struct uidinfo *uip, u_int *hiwat, u_int to,
            rlim_t maxval)
{
	DBG_OSL(("WARNING: any size is OK, no limit\n"));
	*hiwat = to;
	return 1;
}


void    free(void *addr, struct malloc_type *type)
{
	(* nkn_netfree)((void **)&addr);
}
void    *malloc(unsigned long size, struct malloc_type *type, int flags)
{
	return (*nkn_netmalloc)(size);
}
/*
void    *realloc(void *addr, unsigned long size, struct malloc_type *type,
            int flags)
{
	return (*nkn_realloc)(addr, size);
}
*/

void malloc_init(void *data)
{
	// Do not need to do anything
	return;
}

void malloc_uninit(void *data)
{
	// Do not need to do anything
	return;
}

int netisr_queue(int num, struct mbuf *m)
{
	DBG_OSL(("ERROR:     netisr_queue\n"));
	return 0;
}

int read_random(void * bufp, int len)
{
	DBG_OSL(("ERROR:     read_random\n"));
	return 0;
}

void    netisr_register(int i, netisr_t * n, struct ifqueue * ifq, int i2)
{
	DBG_OSL(("ERROR:     netisr_register\n"));
}


void    mtx_init(struct mtx *m, const char *name, const char *type, int opts)
{
	/* Determine lock class and lock flags. */
	//memset((char *)&m->linux_mtx, 0, sizeof(m->linux_mtx));
	//m->linux_mtx = PTHREAD_MUTEX_INITIALIZER;
	DBG_OSL(("ERROR:     mtx_init\n"));
}
void    mtx_destroy(struct mtx *m)
{
	//DBG_OSL(("ERROR:     mtx_destroy\n"));
}
int mtx_lock(struct mtx *m)
{
	//nkn_pthread_mutex_unlock(&m->linux_mtx);
	//DBG_OSL(("ERROR:     mtx_lock\n"));
	return 0;
}
int mtx_lock_spin(struct mtx *m)
{
	//DBG_OSL(("ERROR:     mtx_lock_spin\n"));
	return 0;
}
int mtx_trylock(struct mtx *m)
{
	//nkn_pthread_mutex_trylock(&m->linux_mtx);
	return 0;
}
int mtx_unlock(struct mtx *m)
{
	//nkn_pthread_mutex_unlock(&m->linux_mtx);
	return 0;
}
int mtx_unlock_spin(struct mtx *m)
{
//	DBG_OSL(("ERROR:     mtx_unlock_spin\n"));
	return 0;
}

void    mtx_sysinit(void *arg)
{
	DBG_OSL(("ERROR:     mtx_sysinit\n"));
}

int     _sleep(void *chan, struct lock_object *lock, int pri, const char *wmesg,
            int timo)
{
	DBG_OSL(("ERROR:     _sleep\n"));
	return 0;
}

unsigned long random(void)
{
	return (unsigned long)(*nkn_random)();
}

void DELAY(int n)
{
	nkn_usleep(n);	// sleep in microseconds
}

int
copyiniov(struct iovec *iovp, u_int iovcnt, struct iovec **iov, int error)
{
	u_int iovlen;

	*iov = NULL;
	if (iovcnt > UIO_MAXIOV)
		return (error);
	iovlen = iovcnt * sizeof (struct iovec);
	*iov = malloc(iovlen, M_IOV, M_WAITOK);
	error = copyin(iovp, *iov, iovlen);
	if (error) {
		free(*iov, M_IOV);
		*iov = NULL;
	}
	return (error);
}

pid_t	fgetown(struct sigio **sigiop)
{
	DBG_OSL(("ERROR:     fgetown\n"));
	return 0;
}
int	fsetown(pid_t pgid, struct sigio **sigiop)
{
	DBG_OSL(("ERROR:     fsetown\n"));
	return 0;
}



/*-------------------------------------------------------------------*/

static int
badfo_readwrite(struct file *fp, struct uio *uio, struct ucred *active_cred, int flags, struct thread *td)
{

	return (EBADF);
}

static int
badfo_ioctl(struct file *fp, u_long com, void *data, struct ucred *active_cred, struct thread *td)
{

	return (EBADF);
}

static int
badfo_poll(struct file *fp, int events, struct ucred *active_cred, struct thread *td)
{

	return (0);
}

static int
badfo_kqfilter(struct file *fp, struct knote *kn)
{

	return (EBADF);
}

static int
badfo_stat(struct file *fp, struct stat *sb, struct ucred *active_cred, struct thread *td)
{

	return (EBADF);
}

static int
badfo_close(struct file *fp, struct thread *td)
{

	return (EBADF);
}

struct fileops badfileops = {
	.fo_read = badfo_readwrite,
	.fo_write = badfo_readwrite,
	.fo_ioctl = badfo_ioctl,
	.fo_poll = badfo_poll,
	.fo_kqfilter = badfo_kqfilter,
	.fo_stat = badfo_stat,
	.fo_close = badfo_close,
};


void
critical_enter(void)
{
//	DBG_OSL(("ERROR: critical_enter\n"));
}

void
critical_exit(void)
{
//	DBG_OSL(("ERROR: critical_exit\n"));
}


////////////////////////////////////////////////////////////////////
cpumask_t all_cpus;

int mp_ncpus;
/* export this for libkvm consumers. */
int mp_maxcpus = MAXCPU;

struct cpu_top *smp_topology;
volatile int smp_started;
u_int mp_maxid;

#ifdef MAXHE_TODO
/*
 * Sets the value of the per-cpu variable name to value val.
 */
#define	__PCPU_SET(name, val) {						\
	__pcpu_type(name) __val;					\
	struct __s {							\
		u_char	__b[MIN(sizeof(__pcpu_type(name)), 4)];		\
	} __s;								\
									\
	__val = (val);							\
	if (sizeof(__val) == 1 || sizeof(__val) == 2 ||			\
	    sizeof(__val) == 4) {					\
		__s = *(struct __s *)(void *)&__val;			\
		__asm __volatile("mov %1,%%fs:%0"			\
		    : "=m" (*(struct __s *)(__pcpu_offset(name)))	\
		    : "r" (__s));					\
	} else {							\
		*__PCPU_PTR(name) = __val;				\
	}								\
}

#define	PCPU_GET(member)	__PCPU_GET(pc_ ## member)
#endif // MAXHE_TODO
/*
 * Provide dummy SMP support for UP kernels.  Modules that need to use SMP
 * APIs will still work using this dummy support.
 */
static void
mp_setvariables_for_up(void *dummy)
{
	DBG_OSL(("ERROR: mp_setvariables_for_up\n"));
	mp_ncpus = 1;
	mp_maxid = 1; //PCPU_GET(cpuid);
	all_cpus = 1; //PCPU_GET(cpumask);
	KASSERT(PCPU_GET(cpuid) == 0, ("UP must have a CPU ID of zero"));
}
SYSINIT(cpu_mp_setvariables, SI_SUB_TUNABLES, SI_ORDER_FIRST,
    mp_setvariables_for_up, NULL)


/////////////////////////////////////////////////////////
int
copyin(const void *udaddr, void *kaddr, size_t len)
{
	DBG_OSL(("ERROR: nobody should call copin although it supported now\n"));
	// copy from user space to kernel space
	memcpy(kaddr, udaddr, len);
	return 0;
}

int
copyout(const void *kaddr, void *udaddr, size_t len)
{
	DBG_OSL(("ERROR: nobody should call copyout although it supported now\n"));
	// copy from kernel space to user space
	memcpy(udaddr, kaddr, len);
	return 0;
}

/////////////////////////////////////////////////////////
void *
uma_small_alloc(uma_zone_t zone, int bytes, u_int8_t *flags, int wait);
void *
uma_small_alloc(uma_zone_t zone, int bytes, u_int8_t *flags, int wait)
{
	return nkn_netmalloc(bytes);
}

void
uma_small_free(void *mem, int size, u_int8_t flags);
void
uma_small_free(void *mem, int size, u_int8_t flags)
{
	nkn_netfree((void **)&mem);
}

//void *memcpy(void *dest, const void *src, size_t n);
void *memcpy(void *dest, const void *src, size_t n)
{
	char * d=dest;
	const char * s=src;

	while(n) {
		*d++=*s++;
		n--;
	}
	return dest;
}

void bzero(void *s, size_t n)
{
	char * s1=s;
	while(n) {
		*s1=0;
		n--;
	}
}

void bcopy(const void *src, void *dest, size_t n)
{
	char * d=dest;
	const char * s=src;

	while(n) {
		*d++=*s++;
		n--;
	}
	return ;
}

typedef	const void	*cvp;
typedef	const unsigned char	*ustring;
typedef unsigned long	ul;
typedef const unsigned long	*culp;

/*
 * bcmp -- vax cmpc3 instruction
 */
int
bcmp(b1, b2, length)
	const void *b1, *b2;
	register size_t length;
{
#if BYTE_ORDER == LITTLE_ENDIAN
	/*
	 * The following code is endian specific.  Changing it from
	 * little-endian to big-endian is fairly trivial, but making
	 * it do both is more difficult.
	 *
	 * Note that this code will reference the entire longword which
	 * includes the final byte to compare.  I don't believe this is
	 * a problem since AFAIK, objects are not protected at smaller
	 * than longword boundaries.
	 */
	int	shl, shr, len = length;
	ustring	p1 = b1, p2 = b2;
	ul	va, vb;

	if (len == 0)
		return (0);

	/*
	 * align p1 to a longword boundary
	 */
	while ((long)p1 & (sizeof(long) - 1)) {
		if (*p1++ != *p2++)
			return (1);
		if (--len <= 0)
			return (0);
	}

	/*
	 * align p2 to longword boundary and calculate the shift required to
	 * align p1 and p2
	 */
	shr = (long)p2 & (sizeof(long) - 1);
	if (shr != 0) {
		p2 -= shr;			/* p2 now longword aligned */
		shr <<= 3;			/* offset in bits */
		shl = (sizeof(long) << 3) - shr;

		va = *(culp)p2;
		p2 += sizeof(long);

		while ((len -= sizeof(long)) >= 0) {
			vb = *(culp)p2;
			p2 += sizeof(long);
			if (*(culp)p1 != (va >> shr | vb << shl))
				return (1);
			p1 += sizeof(long);
			va = vb;
		}
		/*
		 * At this point, len is between -sizeof(long) and -1,
		 * representing 0 .. sizeof(long)-1 bytes remaining.
		 */
		if (!(len += sizeof(long)))
			return (0);

		len <<= 3;		/* remaining length in bits */
		/*
		 * The following is similar to the `if' condition
		 * inside the above while loop.  The ?: is necessary
		 * to avoid accessing the longword after the longword
		 * containing the last byte to be compared.
		 */
		return ((((va >> shr | ((shl < len) ? *(culp)p2 << shl : 0)) ^
		    *(culp)p1) & ((1L << len) - 1)) != 0);
	} else {
		/* p1 and p2 have common alignment so no shifting needed */
		while ((len -= sizeof(long)) >= 0) {
			if (*(culp)p1 != *(culp)p2)
				return (1);
			p1 += sizeof(long);
			p2 += sizeof(long);
		}

		/*
		 * At this point, len is between -sizeof(long) and -1,
		 * representing 0 .. sizeof(long)-1 bytes remaining.
		 */
		if (!(len += sizeof(long)))
			return (0);

		return (((*(culp)p1 ^ *(culp)p2)
			 & ((1L << (len << 3)) - 1)) != 0);
	}
#else
	register char *p1, *p2;

	if (length == 0)
		return(0);
	p1 = (char *)b1;
	p2 = (char *)b2;
	do
		if (*p1++ != *p2++)
			break;
	while (--length);
	return(length);
#endif
}

int uiomoveco(void *cp, int n, struct uio *uio, int disposable)
{
	return 0;
}

static void nkn_socow_mbuf_ext_do_free(void * ext_buf, void * ext_args);
static void nkn_socow_mbuf_ext_do_free(void * ext_buf, void * ext_args)
{
	struct mbuf *m0=(struct mbuf *)ext_args;
	nkn_netfree((void **)&(m0->m_ext.ref_cnt));
}

int nkn_socow_setup(struct mbuf *m0, struct uio *uio, int space);
int nkn_socow_setup(struct mbuf *m0, struct uio *uio, int space)
{
	struct iovec *iov;

	iov = uio->uio_iov;

	// Setup M_EXT flag
	// We always use M_EXT
	m0->m_flags |= M_EXT;

	if(iov->iov_len > NKN_MTU) {
		 m0->m_len = NKN_MTU;
	} else {
		m0->m_len = iov->iov_len;
	}
	if(m0->m_len>space) m0->m_len=space;

	//printf("=====socow_setup=====> iov->iov_base=0x%lx len=%d\n", 
	//		(long)iov->iov_base, m0->m_len);
	m0->m_ext.ext_buf = (char *)iov->iov_base;
	m0->m_data = m0->m_ext.ext_buf;

	// BUG: we don't know yet how to manage memory.
	// But I use the function m0->m_ext.ext_free to free ref_cnt buffer.
	// Try to free m0->m_ext.ref_cnt
	m0->m_ext.ext_free = nkn_socow_mbuf_ext_do_free;
	m0->m_ext.ext_args = (void *)m0;
	m0->m_ext.ext_size = NKN_MTU;
	m0->m_ext.ext_type = EXT_EXTREF;
	m0->m_ext.ref_cnt = (u_int *)nkn_netmalloc(sizeof(u_int));
	*(m0->m_ext.ref_cnt) = 1;

	iov->iov_base = (char *)iov->iov_base + m0->m_len;
	iov->iov_len -= m0->m_len;
	uio->uio_resid -= m0->m_len;
	uio->uio_offset += m0->m_len;
	if (iov->iov_len == 0) {
		uio->uio_iov++;
		uio->uio_iovcnt--;
	}
	//printf("socow_setup: iov->iov_len=%ld\n", iov->iov_len);

	return(m0->m_len);
}



