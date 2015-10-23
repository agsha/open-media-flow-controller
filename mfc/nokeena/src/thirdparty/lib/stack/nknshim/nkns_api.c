#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/kern/init_main.c,v 1.283.2.2 2007/12/14 13:41:08 rrs Exp $");

#include "opt_ddb.h"
#include "opt_init_path.h"
#include "opt_mac.h"
#include "opt_printf.h"


#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/exec.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sx.h>
#include <sys/sysproto.h>
#include <sys/vmmeter.h>
#include <sys/unistd.h>
#include <sys/malloc.h>
#include <sys/domain.h>
#include <sys/time.h>
#include <sys/sockio.h>
#include <sys/msgbuf.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/sx.h>
#include <sys/mbuf.h>
#include <sys/filio.h>
#include <sys/socketvar.h>

#include <net/netisr.h>
#include <net/if.h>
#include <net/if_clone.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_var.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <nknshim/nkns_osl.h>
#include <nknshim/nkns_api.h>

#define ssize_t long 

int nkn_errno;

static pthread_mutex_t global_mutex = PTHREAD_MUTEX_INITIALIZER; 

#define my_nkn_pthread_mutex_lock(x) \
{ \
	nkn_pthread_mutex_lock(x); \
	/*printf("lock: %s: %d\n", __FUNCTION__, __LINE__);*/ \
}
#define my_nkn_pthread_mutex_unlock(x) \
{ \
	/*printf("unlock: %s: %d .....\n\n", __FUNCTION__, __LINE__);*/ \
	nkn_pthread_mutex_unlock(x); \
}

void inline nkn_crash(void) ;
void inline nkn_crash(void) 
{
	char * p=NULL;
	printf("crashed: %s:%d", __FUNCTION__, __LINE__);
	*p='1';
}

void nkn_init_api_mutex(void);
void nkn_init_api_mutex(void)
{
//	pthread_mutex_init(&global_mutex, NULL); 
}

/*
NOTICE: the defination is different between FreeBSD and Linux 
for this structure So we need to be very careful.
*/

//Defined in FREEBSD:
struct sockaddr_in_freebsd {
        uint8_t sin_len;
        sa_family_t     sin_family;
        in_port_t       sin_port;
        struct  in_addr sin_addr;
        char    sin_zero[8];
};

//Defined in Linux:
struct sockaddr_in_linux {
  sa_family_t           sin_family;
  unsigned short int    sin_port;
  struct in_addr        sin_addr;

  // Pad to size of `struct sockaddr'.
  //unsigned char         __pad[__SOCK_SIZE__ - sizeof(short int) -
  //                      sizeof(unsigned short int) - sizeof(struct in_addr)];
};

static void sockaddr_in_linux_to_freebsd(
	struct sockaddr_in_linux * l,
	struct sockaddr_in_freebsd * f,
	int addrlen);
static void sockaddr_in_linux_to_freebsd(
	struct sockaddr_in_linux * l,
	struct sockaddr_in_freebsd * f,
	int addrlen)
{
	f->sin_family = l->sin_family;
	f->sin_port = l->sin_port;
	f->sin_addr.s_addr = l->sin_addr.s_addr;

	f->sin_len = addrlen;
}
	
static void sockaddr_in_freebsd_to_linux(
	struct sockaddr_in_freebsd * f,
	struct sockaddr_in_linux * l);
static void sockaddr_in_freebsd_to_linux(
	struct sockaddr_in_freebsd * f,
	struct sockaddr_in_linux * l)
{
	l->sin_family = f->sin_family;
	l->sin_port = f->sin_port;
	l->sin_addr.s_addr = f->sin_addr.s_addr;
}
	

//-----------------------------------------------
#define MAX_SOCKFD	64000
static struct socket * all_sockfds[MAX_SOCKFD+1] = { NULL };
static int nkn_nextsockfd=1;

static struct socket * CONVERT_ID_TO_SOCKET(int x) 
{ 
	if(all_sockfds[x]==NULL) {
		//printf("CONVERT_ID_TO_SOCKET return 0 for x=%d\n", x); 
		return NULL;
		//nkn_crash();

	} 
	return all_sockfds[x];
}

static void DEL_SOCKET_BY_ID(int x) { 
	if(all_sockfds[x]==NULL) { 
		printf("DEL_SOCKET_BY_ID: already zero\n"); 
		return;
	} 
	all_sockfds[x]=NULL; 
	//printf("DEL_SOCKET_BY_ID: x=%d\n", x); 
}

static int GET_ID_FOR_SOCKET(struct socket * so);
static int GET_ID_FOR_SOCKET(struct socket * so)
{
        int i;
        if(so==NULL) {
                printf("GET_ID_FOR_SOCKET: s==NULL, return -1\n");
                return -1;
        }
        for(i=1;i<MAX_SOCKFD;i++,nkn_nextsockfd++)
        {
                if(nkn_nextsockfd >= MAX_SOCKFD) nkn_nextsockfd=1;
                if(all_sockfds[nkn_nextsockfd]==NULL) {
                        all_sockfds[nkn_nextsockfd]=so;
			so->nkn_s=nkn_nextsockfd;
			so->pnknevents=NULL;
			so->pnknwaitevents=NULL;
			so->nkn_where = 0;
			so->tot_accepted_len=1;	// Skip SYN+ACK packet
                        //printf("GET_ID_FOR_SOCKET: x=%d\n", nkn_nextsockfd);
			i = nkn_nextsockfd;
                        nkn_nextsockfd++;
                        return i;
                }
        }
        printf("GET_ID_FOR_SOCKET: socket slot full. crash for s=0x%lx\n", (long)so);
        nkn_crash();
        return -1; // crash
}

#if 0
static int CONVERT_SOCKET_TO_ID(struct socket * so);
static int CONVERT_SOCKET_TO_ID(struct socket * so)
{
	if(so==NULL) return -1; // crash
	
        return so->nkn_s;
}
#endif // 0



// ----------------------------------------------
// Interface/IP address/Routing table setup functions
// ----------------------------------------------
// This is the IP address we are using
unsigned long vifip;	// In host order
unsigned long vnetmask;	// In host order
unsigned long vsubnet;	// In host order

int nkn_add_ifip(unsigned long ifip, unsigned long netmask, unsigned long subnet)
{
	//nkn_pthread_mutex_init(&global_mutex, NULL);
	//return 0;

	vifip = ifip;
	vnetmask = netmask;
	vsubnet = subnet;

	/*
	struct ifnet ifp;
	struct if_laddrreq iflr;

	memset((char *)&ifp, 0, sizeof(ifp));
	ifp->if_flags |= IFF_UP;
	
	in_control(NULL, SIOCALIFADDR, (caddr_t)&iflr, &ifp, NULL);
	*/

	return 1;
}



// ----------------------------------------------
// Timer functions
// ----------------------------------------------
void nkn_100ms_timer(void * tv)
{
	//struct timeval tv1, tv2;

	my_nkn_pthread_mutex_lock(&global_mutex);
	ticks++;
        //nkn_gettimeofday(&tv1, NULL);
	softclock(NULL);
        //nkn_gettimeofday(&tv2, NULL);
	//if(tv2.tv_usec-tv1.tv_usec>10)
        //printf("softclock: tv1=%ld tv2=%ld delta=%ld\n", tv1.tv_usec, tv2.tv_usec, tv2.tv_usec-tv1.tv_usec);

	my_nkn_pthread_mutex_unlock(&global_mutex);
}

void nkn_1s_timer(void *tv)
{
}

// ----------------------------------------------
// /dev/tun0 packet input function
// ----------------------------------------------

void nkn_mbuf_ext_do_free(void * ext_buf, void * ext_args);
void nkn_mbuf_ext_do_free(void * ext_buf, void * ext_args)
{
	free(ext_buf, M_TEMP);
	//printf("nkn_mbuf_ext_free: buf=0x%x\n", (int)ext_buf);
}

static void nkn_mbuf_ext_free(void * ext_buf, void * ext_args);
static void nkn_mbuf_ext_free(void * ext_buf, void * ext_args)
{
	nkn_netfree( (void **)&ext_buf );
}

int nkn_ip_input(char * p, int len)
{
	struct mbuf * m;
	u_int * pref_cnt;
	//struct timeval tv1, tv2;
	char * pcopy;

        //nkn_gettimeofday(&tv1, NULL);
	my_nkn_pthread_mutex_lock(&global_mutex);
        //nkn_gettimeofday(&tv2, NULL);
        //printf("nkn_ip_input: (1) tv1=%ld tv2=%ld\n", tv1.tv_usec, tv2.tv_usec);

	pcopy=(char *)nkn_netmalloc(len+8); // add 4 for reference counter
	if(!pcopy) {
		my_nkn_pthread_mutex_unlock(&global_mutex);
		return 0;
	}
	memcpy(pcopy, p, len);
	pref_cnt = (u_int *)(pcopy+len);
	*pref_cnt = 1;
	
	//printf("nkn_ip_input: (1) called\n");

	MGET(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
	{
		printf("ERROR: nkn_ip_input(): cannot get send buffer.\n");
		nkn_netfree((void **)&pcopy);
		my_nkn_pthread_mutex_unlock(&global_mutex);
		//printf("my_nkn_pthread_mutex_unlock: (1)\n");
		return 0;
	}
	m->m_flags = M_PKTHDR | M_EOR;

	m->m_flags |= M_EXT;
	m->m_ext.ext_buf = (caddr_t)pcopy;
	m->m_data = m->m_ext.ext_buf;
	m->m_ext.ext_free = nkn_mbuf_ext_free;
	m->m_ext.ext_args = NULL;
	m->m_ext.ext_size = len;
	m->m_ext.ext_type = EXT_EXTREF;
	m->m_ext.ref_cnt = pref_cnt;

	m->m_len = len;
	m->m_pkthdr.len = len;
	m->m_pkthdr.rcvif = NULL;
	m->m_pkthdr.csum_flags = 0;
	m_tag_init(m);


	//printf("nkn_ip_input: (2) called\n");
	//printf("nkn_ip_input: m=0x%lx m_len=%d\n", (long)m, m->m_len);
	ip_input(m);
	//printf("nkn_ip_input: (3) called\n");
        //nkn_gettimeofday(&tv2, NULL);
        //printf("nkn_ip_input: (2) tv1=%ld tv2=%ld\n", tv1.tv_usec, tv2.tv_usec);
	my_nkn_pthread_mutex_unlock(&global_mutex);

	return 1;
}

void     nkn_debug_verbose(int verbose)
{
	nkn_verbose=verbose;
}

// ----------------------------------------------
// socket API functions
// ----------------------------------------------

static int nkn_kern_accept(struct thread *td, int s, struct sockaddr **name,
    socklen_t *namelen, struct socket **retso);
static int nkn_kern_accept(struct thread *td, int s, struct sockaddr **name,
    socklen_t *namelen, struct socket **retso)
{
	struct sockaddr *sa = NULL;
	int error=0;
	struct socket *head, *so=NULL;

	if (name) {
		*name = NULL;
		if (*namelen < 0) {
			return (EINVAL);
		}
	}

	//printf("nkn_kern_accept: called\n");
	head=CONVERT_ID_TO_SOCKET(s);
	if ((head->so_options & SO_ACCEPTCONN) == 0) {
		error = EINVAL;
		goto done;
	}
	//printf("nkn_kern_accept: called 1\n");

	ACCEPT_LOCK();
	while (TAILQ_EMPTY(&head->so_comp) && head->so_error == 0) {
		if (head->so_rcv.sb_state & SBS_CANTRCVMORE) {
			head->so_error = ECONNABORTED;
			//printf("nkn_kern_accept: called 1.6\n");
			break;
		}
		// MAXHE: BUG here.
		//printf("head->so_timeo=%d head->so_error = %d\n", head->so_timeo, head->so_error);
		//my_nkn_pthread_mutex_unlock(&global_mutex);
		//nkn_usleep(100);//head->so_timeo * 1000); // microsecond
		//my_nkn_pthread_mutex_lock(&global_mutex);
		error=1; // Some error code>0
		ACCEPT_UNLOCK();
		goto noconnection;
	}
	//printf("nkn_kern_accept: called 2\n");
	if (head->so_error) {
		error = head->so_error;
		head->so_error = 0;
		ACCEPT_UNLOCK();
		goto noconnection;
	}
	//printf("nkn_kern_accept: called 4\n");
	so = TAILQ_FIRST(&head->so_comp);
	KASSERT(!(so->so_qstate & SQ_INCOMP), ("accept1: so SQ_INCOMP"));
	KASSERT(so->so_qstate & SQ_COMP, ("accept1: so not SQ_COMP"));
	//printf("nkn_kern_accept: called 5\n");

	/*
	 * Before changing the flags on the socket, we have to bump the
	 * reference count.  Otherwise, if the protocol calls sofree(),
	 * the socket will be released due to a zero refcount.
	 */
	SOCK_LOCK(so);			/* soref() and so_state update */
	soref(so);			/* file descriptor reference */

	TAILQ_REMOVE(&head->so_comp, so, so_list);
	head->so_qlen--;
	so->so_state |= (head->so_state & SS_NBIO);
	so->so_qstate &= ~SQ_COMP;
	so->so_head = NULL;
	//printf("queue length: %d\n", head->so_qlen);

	//printf("nkn_kern_accept: called 9\n");
	SOCK_UNLOCK(so);
	ACCEPT_UNLOCK();

	/* connection has been removed from the listen queue */
	KNOTE_UNLOCKED(&head->so_rcv.sb_sel.si_note, 0);

	sa = 0;
	error = soaccept(so, &sa);
	if (error) {
		/*
		 * return a namelen of zero for older code which might
		 * ignore the return value from accept.
		 */
		if (name)
			*namelen = 0;
		goto noconnection;
	}
	if (sa == NULL) {
		if (name)
			*namelen = 0;
		goto done;
	}
	if (name) {
		/* check sa_len before it is destroyed */
		if (*namelen > sa->sa_len)
			*namelen = sa->sa_len;
		*name = sa;
		sa = NULL;
	}
noconnection:
	if (sa)
		FREE(sa, M_SONAME);

	/*
	 * Release explicitly held references before returning.  We return
	 * a reference on nfp to the caller on success if they request it.
	 */
done:
	//printf("nkn_kern_accept: done\n");
	if (error == 0) {
		*retso = so;
	} 
	return (error);
}

int     nkn_geterrno(void)
{
	return nkn_errno;
}

// accept will return sockfd id.
int     nkn_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
	struct sockaddr_in_freebsd * sa_freebsd;
	struct sockaddr * sa;
	int error, s;
	struct socket * retso;

	my_nkn_pthread_mutex_lock(&global_mutex);
	//printf("Calling nkn_accept global_mutex=%p\n", &global_mutex);
	//printf("nkn_accept: 0 \n");

	// Not use the following so far
	if (addr == NULL) {
		retso=NULL;
		error = nkn_kern_accept(NULL, sockfd, NULL, NULL, &retso);
		if (error) {
			nkn_errno=error;
			//printf("Calling nkn_accept error\n");
			my_nkn_pthread_mutex_unlock(&global_mutex);
			return (-1);
		}
		//printf("Calling nkn_kern_accept done\n");
		s = GET_ID_FOR_SOCKET(retso);
		my_nkn_pthread_mutex_unlock(&global_mutex);
		return s;
	}

	//printf("nkn_accept: 1 addrlen=0x%lx\n", (long)addrlen);
	
	error = nkn_kern_accept(NULL, sockfd, &sa, addrlen, &retso);

	//printf("nkn_accept: 3 error=%d\n", error);
	/*
	 * return a namelen of zero for older code which might
	 * ignore the return value from accept.
	 */
	if (error) {
		my_nkn_pthread_mutex_unlock(&global_mutex);
		return (-1);
	}

	if (error == 0 && sa != NULL) {
		sa_freebsd=(struct sockaddr_in_freebsd *)sa;
		sockaddr_in_freebsd_to_linux(sa_freebsd,
				(struct sockaddr_in_linux *)addr);
		*addrlen = sa_freebsd->sin_len;
	}
	free(sa, M_SONAME);
	s = GET_ID_FOR_SOCKET(retso);
	my_nkn_pthread_mutex_unlock(&global_mutex);
	//printf("nkn_accept: 4 s=%d\n", s);
	return s;
}

int     nkn_bind(int s, struct sockaddr *my_addr, socklen_t addrlen)
{
	int ret;
	struct socket *so;
	//printf("s=%d so=0x%lx\n", s, (long)so);

	my_nkn_pthread_mutex_lock(&global_mutex);
	so = CONVERT_ID_TO_SOCKET(s);

	my_addr->sa_len = addrlen;
	ret = sobind(so, my_addr, NULL);
	//printf("Calling nkn_bind\n");
	my_nkn_pthread_mutex_unlock(&global_mutex);
	return ret;
}

int     nkn_connect(int s, struct sockaddr *serv_addr, socklen_t addrlen)
{
	int error=-1, ret;
	struct socket *so;
	struct sockaddr_in_freebsd sa_freebsd;

	my_nkn_pthread_mutex_lock(&global_mutex);
	so = CONVERT_ID_TO_SOCKET(s);

	sockaddr_in_linux_to_freebsd((struct sockaddr_in_linux *)serv_addr,
					&sa_freebsd, addrlen);
	

	//printf("Calling nkn_connect\n");

	if (so->so_state & SS_ISCONNECTING) {
		nkn_errno = EALREADY;
		goto done1;
	}

	//printf("nkn_connect: here 1 family=%d\n", ((struct sockaddr_in *)serv_addr)->sin_family);
	error = soconnect(so, (struct sockaddr *)&sa_freebsd, NULL);
	//printf("nkn_connect: here 2 error=%d\n", error);
	if (error) {
		nkn_errno=error;
		error=-1;
		goto done1;
	}
	if ((so->so_state & SS_NBIO) && (so->so_state & SS_ISCONNECTING)) {
		nkn_errno = EINPROGRESS;
		error=-1;
		goto done1;
	}
	SOCK_LOCK(so);
	while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0) {
		printf("ERROR: so->so_timeo=%d should not be zero\n", so->so_timeo);
		ret = nkn_usleep(so->so_timeo * 1000);
		if (ret) { 
			so->so_state &= ~SS_ISCONNECTING;
		}
	}

	// Socket has been connected.

	if (error == 0) {
		error = so->so_error;
		so->so_error = 0;
	}
	SOCK_UNLOCK(so);
	//printf("nkn_connect: here 4 error=%d\n", error);
done1:
	// recover back sockaddr structure
	// sockaddr_in_freebsd_to_linux(&sa_freebsd,
	//			(struct sockaddr_in_linux *)serv_addr);

	//printf("nkn_connect: here 5 error=%d\n", error);
	my_nkn_pthread_mutex_unlock(&global_mutex);
	return (error);
}

static int
nkn_kern_getpeername(struct thread *td, int fd, struct sockaddr **sa,
    socklen_t *alen);
static int
nkn_kern_getpeername(struct thread *td, int s, struct sockaddr **sa,
    socklen_t *alen)
{
	struct socket *so;
	socklen_t len;
	int error;

	my_nkn_pthread_mutex_lock(&global_mutex);
	so = CONVERT_ID_TO_SOCKET(s);
	if (*alen < 0) {
		my_nkn_pthread_mutex_unlock(&global_mutex);
		return (EINVAL);
	}

	if ((so->so_state & (SS_ISCONNECTED|SS_ISCONFIRMING)) == 0) {
		error = ENOTCONN;
		goto done;
	}
	*sa = NULL;
	error = (*so->so_proto->pr_usrreqs->pru_peeraddr)(so, sa);
	if (error)
		goto bad;
	if (*sa == NULL)
		len = 0;
	else
		len = MIN(*alen, (*sa)->sa_len);
	*alen = len;
bad:
	if (error && *sa) {
		free(*sa, M_SONAME);
		*sa = NULL;
	}
done:
	my_nkn_pthread_mutex_unlock(&global_mutex);
	return (error);
}

/*
 * getpeername() - Get name of peer for connected socket.
 */
int     nkn_getpeername(int s, struct sockaddr *name, socklen_t *namelen)
{
	struct sockaddr_in_freebsd * sa_freebsd;
	struct sockaddr * sa;
	int error;


	my_nkn_pthread_mutex_lock(&global_mutex);
	//printf("Calling nkn_getpeername\n");

	// do the real work
	error = nkn_kern_getpeername(NULL, s, &sa, namelen);
	if (error) {
		my_nkn_pthread_mutex_unlock(&global_mutex);
		return (error);
	}

	// convert from FreeBSD to Linux
	sa_freebsd=(struct sockaddr_in_freebsd *)sa;
	sockaddr_in_freebsd_to_linux(sa_freebsd,
				(struct sockaddr_in_linux *)name);
	*namelen = sa_freebsd->sin_len;
	free(sa, M_SONAME); // sa was malloced inside pru_peeraddr() call.
	my_nkn_pthread_mutex_unlock(&global_mutex);
	return (error);
}

static int
nkn_kern_getsockname(struct thread *td, int s, struct sockaddr **sa,
    socklen_t *alen);
static int
nkn_kern_getsockname(struct thread *td, int s, struct sockaddr **sa,
    socklen_t *alen)
{
	struct socket *so = CONVERT_ID_TO_SOCKET(s);
	socklen_t len;
	int error;

	my_nkn_pthread_mutex_lock(&global_mutex);
	if (*alen < 0)	{
		my_nkn_pthread_mutex_unlock(&global_mutex);
		return (EINVAL);
	}

	*sa = NULL;
	error = (*so->so_proto->pr_usrreqs->pru_sockaddr)(so, sa);
	if (error)
		goto bad;
	if (*sa == NULL)
		len = 0;
	else
		len = MIN(*alen, (*sa)->sa_len);
	*alen = len;
bad:
	if (error && *sa) {
		free(*sa, M_SONAME);
		*sa = NULL;
	}

	my_nkn_pthread_mutex_unlock(&global_mutex);
	return (error);
}

int     nkn_getsockname(int s, struct sockaddr *name, socklen_t *namelen)
{
	struct sockaddr_in_freebsd * sa_freebsd;
	struct sockaddr *sa;
	int error;

	//printf("Calling nkn_getsockname\n");
	my_nkn_pthread_mutex_lock(&global_mutex);

	error = nkn_kern_getsockname(NULL, s, &sa, namelen);
	if (error) {
		my_nkn_pthread_mutex_unlock(&global_mutex);
		return (error);
	}

	sa_freebsd=(struct sockaddr_in_freebsd *)sa;
	sockaddr_in_freebsd_to_linux(sa_freebsd,
				(struct sockaddr_in_linux *)name);
	*namelen = sa_freebsd->sin_len;
	free(sa, M_SONAME);
	my_nkn_pthread_mutex_unlock(&global_mutex);
	return (error);
}

/*
 * Kernel version of getsockopt.
 * optval can be a userland or userspace. optlen is always a kernel pointer.
 */
static int
nkn_kern_getsockopt(
	struct thread *td,
	int s,
	int level,
	int name,
	void *val,
	enum uio_seg valseg,
	socklen_t *valsize);
static int
nkn_kern_getsockopt(td, s, level, name, val, valseg, valsize)
	struct thread *td;
	int s;
	int level;
	int name;
	void *val;
	enum uio_seg valseg;
	socklen_t *valsize;
{
	int error;
	struct socket *so = CONVERT_ID_TO_SOCKET(s);
	struct	sockopt sopt;

	if (val == NULL)
		*valsize = 0;
	if ((int)*valsize < 0)
		return (EINVAL);

	sopt.sopt_dir = SOPT_GET;
	sopt.sopt_level = level;
	sopt.sopt_name = name;
	sopt.sopt_val = val;
	sopt.sopt_valsize = (size_t)*valsize; /* checked non-negative above */
	switch (valseg) {
	case UIO_USERSPACE:
		sopt.sopt_td = td;
		break;
	case UIO_SYSSPACE:
		sopt.sopt_td = NULL;
		break;
	default:
		panic("kern_getsockopt called with bad valseg");
	}

	error = sogetopt(so, &sopt);
	*valsize = sopt.sopt_valsize;
	return (error);
}


int     nkn_getsockopt(int s, int level, int optname, void *optval, socklen_t * optlen)
{
	int	error;

	my_nkn_pthread_mutex_lock(&global_mutex);
	//printf("Calling nkn_getsockopt\n");
	error = nkn_kern_getsockopt(NULL, s, level, optname, optval, UIO_USERSPACE, optlen);
	my_nkn_pthread_mutex_unlock(&global_mutex);
	return (error);
}

int     nkn_listen(int s, int backlog)
{
	int ret;
	struct socket *so;

	my_nkn_pthread_mutex_lock(&global_mutex);
	so = CONVERT_ID_TO_SOCKET(s);
	ret = solisten(so, backlog, NULL);
	//printf("Calling nkn_listen\n");
	my_nkn_pthread_mutex_unlock(&global_mutex);
	return ret;
}

ssize_t nkn_recv(int s, void *buf, size_t len, int flags)
{
	struct uio auio;
	socklen_t uio_len;
	int error;
	struct mbuf *m;
	struct socket *so;
	struct sockaddr *fromsa;
	int msg_flags;
	struct iovec iov;


	my_nkn_pthread_mutex_lock(&global_mutex);
	so = CONVERT_ID_TO_SOCKET(s);
	fromsa = 0;
	msg_flags=flags;
	if(!so || !buf || (len<=0)) {
		my_nkn_pthread_mutex_unlock(&global_mutex);
		//printf("my_nkn_pthread_mutex_unlock: (3)\n");
		return (EINVAL);
	}

	iov.iov_base = buf;	/* Base address. */
	iov.iov_len=len;	/* Length. */
	auio.uio_iov = &iov;
	auio.uio_iovcnt = 1;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_rw = UIO_READ;
	auio.uio_td = NULL;
	auio.uio_offset = 0;			/* XXX */
	auio.uio_resid = len;
	uio_len = auio.uio_resid;
	my_nkn_pthread_mutex_unlock(&global_mutex);
		//printf("my_nkn_pthread_mutex_unlock: (4)\n");
	error = soreceive_generic(so, &fromsa, &auio, (struct mbuf **)&m,
	    (struct mbuf **)0,
	    &msg_flags);
	my_nkn_pthread_mutex_lock(&global_mutex);
	if (error) {
		if (auio.uio_resid != (int)uio_len && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
	}
	if (error) {
		error=error * (-1);
		goto out;
	}

	// added by Maxhe
	if(m) {
	   if(len > m->m_len) {
	  	m_copydata(m, 0, m->m_len, buf);
	   	sbdroprecord(&so->so_rcv);
	   	error = m->m_len;
		m_freem(m);
	   }
   	   else {
		m_freem(m);
		my_nkn_pthread_mutex_unlock(&global_mutex);
		//printf("my_nkn_pthread_mutex_unlock: (5)\n");
		return -2; // Not enough space
	   }
	}
out:
	if (fromsa)
		FREE(fromsa, M_SONAME);

	my_nkn_pthread_mutex_unlock(&global_mutex);
	return (error);
}

ssize_t nkn_read(int s, void *buf, size_t len)
{
	return nkn_recv(s, buf, len, 0);
}

ssize_t nkn_readv(int fd, struct iovec *vector, int count)
{
	printf("ERROR: nkn_readv not implemented\n");
	return 0;
}



ssize_t nkn_recvfrom(int s, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen)
{
	printf("ERROR: nkn_recvfrom not implemented\n");
	return 0;
}

ssize_t nkn_recvmsg(int s, struct msghdr *msg, int flags)
{
	printf("ERROR: nkn_recvmsg not implemented\n");
	return 0;
}


int nkn_kern_sendit(
	struct thread *td,
	int s,
	struct msghdr *mp,
	int flags,
	struct mbuf *control,
	enum uio_seg segflg);

int nkn_kern_sendit(td, s, mp, flags, control, segflg)
	struct thread *td;
	int s;
	struct msghdr *mp;
	int flags;
	struct mbuf *control;
	enum uio_seg segflg;
{
	struct uio auio;
	struct iovec *iov;
	struct socket *so;
	int i;
	long len;
	int error;

	so = CONVERT_ID_TO_SOCKET(s);
	if(!so) {
		error = EINVAL;
		return (error);
	}
	auio.uio_iov = mp->msg_iov;
	auio.uio_iovcnt = mp->msg_iovlen;
	auio.uio_segflg = segflg;
	auio.uio_rw = UIO_WRITE;
	auio.uio_td = td;
	auio.uio_offset = 0;			/* XXX */
	auio.uio_resid = 0;
	iov = mp->msg_iov;
	for (i = 0; i < mp->msg_iovlen; i++, iov++) {
		if ((auio.uio_resid += iov->iov_len) < 0) {
			error = EINVAL;
			return (error);
		}
	}
	len = auio.uio_resid;
	//printf("nkn_kern_sendit: len=%ld\n", len);
	if(so==NULL) {
		printf("so==NULL s=%d\n", s);
		error = EINVAL;
		return (error);
	}
	error = sosend(so, mp->msg_name, &auio, 0, control, flags, td);
	//sbdroprecord(&so->so_snd);
	return (error);
}


static void nkn_appendto_eventslist(struct nkn_socket_events ** list, struct nkn_socket_events * pev);
static void nkn_appendto_eventslist(struct nkn_socket_events ** list, struct nkn_socket_events * pev)
{
	struct nkn_socket_events * pev2;

	if(list==NULL) return;

	pev->next=NULL;
	if(*list==NULL) {
		*list=pev;
		return;
	} 

	pev2 = *list;
	while(pev2->next) {
		//printf("nkn_appendto_eventslist: pex2=%p\n", pev2);
		pev2=pev2->next;
	}
	pev2->next=pev;
}

void nkn_release_mem(struct socket * so, u_int32_t seq);
void nkn_release_mem(struct socket * so, u_int32_t seq)
{
	struct nkn_socket_events * pev;
	u_int32_t acked_len = seq-so->ini_seq;

	// BUG: we need to handle more than 4G

	//printf("nkn_release_mem: acked_len=%d seq=%d ini_seq=%d\n", acked_len, seq, so->ini_seq);
	while(1) {
		pev=so->pnknwaitevents;
		if(pev==NULL) {
			//printf("nkn_release_mem: so->pnknwaitevents==NULL\n");
			break;
		}
		if(acked_len < pev->this_accepted_len) {
			//printf("nkn_release_mem: acked_len=%d pev->this_accepted_len=%d\n", 
			// 	acked_len , pev->this_accepted_len);
			break;
		}
		so->pnknwaitevents=pev->next;
		if((pev->event==NKN_SEND_DATA) && (pev->callback!=NULL))
		{
			//printf("nkn_release_mem: acked_len=%d\n", acked_len);
			//printf("callback: porgbuf=%p lorgbuf=%d\n", 
			//		pev->h.d.porgbuf, pev->h.d.lorgbuf);
#ifdef MAXHE_TODO
			pev->callback(pev->h.d.porgbuf, pev->h.d.lorgbuf);
#endif // MAXHE_TODO
		}
		nkn_netfree((void **)&pev);
	}
}

void nkn_release_all(struct socket * so);
void nkn_release_all(struct socket * so)
{
        struct nkn_socket_events * pev;

	//printf("nkn_release_all: called\n");
	// Free the nkn event list
        while(1) {
                pev=so->pnknevents;
                if(pev==NULL) { break; }
                so->pnknevents=pev->next;
                if((pev->event==NKN_SEND_DATA) && pev->callback)
                {
			pev->callback(pev->h.d.porgbuf, pev->h.d.lorgbuf);
                }
                nkn_netfree((void **)&pev);
        }

	// Free the nkn wait event list
        while(1) {
                pev=so->pnknwaitevents;
                if(pev==NULL) { break; }
                so->pnknwaitevents=pev->next;
                if((pev->event==NKN_SEND_DATA) && pev->callback)
                {
			pev->callback(pev->h.d.porgbuf, pev->h.d.lorgbuf);
                }
                nkn_netfree((void **)&pev);
        }

}



#define NKN_ONESIZE     48000
// where: 0 from nkn_send
// where: 1 from TCP stack
void nkn_get_more_data(struct socket * so, int wherecalled);
void nkn_get_more_data(struct socket * so, int wherecalled)
{
        struct msghdr msg;
        struct iovec aiov;
	struct nkn_socket_events * pev;
	char * pbuf;
	int lbuf;
	int error;

        //printf("nkn_get_more_data: called so->nkn_lbuf=%ld\n", so->nkn_lbuf);
        //printf("nkn_get_more_data: called so->so_snd.sb_cc=%d\n", so->so_snd.sb_cc);
        if(so->so_snd.sb_cc > 7000) {
		//printf("nkn_get_more_data: so->so_snd.sb_cc=%d\n", so->so_snd.sb_cc);
                return;
        }
        if(so->pnknevents==NULL) {
                //printf("nkn_get_more_data: no more events\n");
		return;
	}

	switch(so->pnknevents->event) {
	case NKN_SHUTDOWN:
		pev = so->pnknevents;
		so->pnknevents = NULL;
             	soshutdown(so, pev->h.shutdown_how);
             	DEL_SOCKET_BY_ID(so->nkn_s);
		nkn_appendto_eventslist(&(so->pnknwaitevents), pev);
		soclose(so);
             	return;

	case NKN_SEND_DATA:
		pev = so->pnknevents;
		if(pev->h.d.lbuf <= NKN_ONESIZE) {
			// Get all data and free memory
                	so->pnknevents = pev->next;
			//printf("so->pnknevents=%p\n", so->pnknevents);
			pbuf = pev->h.d.pbuf;
			lbuf = pev->h.d.lbuf;
			//printf("BUG: nkn_netfree((void **)&pev) should be enabled here\n");
			nkn_appendto_eventslist(&(so->pnknwaitevents), pev);
		}
		else {
			pbuf = pev->h.d.pbuf;
			lbuf = NKN_ONESIZE;
			pev->h.d.pbuf += lbuf;
			pev->h.d.lbuf -= lbuf;
		}

        	so->nkn_where=wherecalled;

		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		msg.msg_iov = &aiov;
		msg.msg_iovlen = 1;
		msg.msg_control = 0;
		aiov.iov_base = pbuf;
		aiov.iov_len = lbuf;
		error = nkn_kern_sendit(NULL, so->nkn_s, &msg, 0, NULL, UIO_USERSPACE);
		//printf("nkn_kern_sendit called\n");
		return;

	default:
	     	return;
        }
}

ssize_t nkn_send_fn(int s, void *buf, size_t len, int flags, nkn_data_sent_callback fn)
{
        struct socket *so;
	struct nkn_socket_events * pev;

        //printf("nkn_send: called s=%d\n", s);
        my_nkn_pthread_mutex_lock(&global_mutex);

        //printf("nkn_send: (1)\n");
        so = CONVERT_ID_TO_SOCKET(s);
        if(!so) {
                //error = EINVAL;
                my_nkn_pthread_mutex_unlock(&global_mutex);
                return (-1);
        }
        //printf("nkn_send: (2)\n");
	pev=(struct nkn_socket_events *)nkn_netmalloc(sizeof(struct nkn_socket_events));
	if(!pev) {
        	my_nkn_pthread_mutex_unlock(&global_mutex);
                return (-1);
	}
        //printf("nkn_send: (3)\n");
	pev->next=NULL;
	pev->event=NKN_SEND_DATA;
	pev->h.d.pbuf=buf;
	pev->h.d.lbuf=len;
	pev->h.d.porgbuf=buf;
	pev->h.d.lorgbuf=len;
	pev->callback=fn;

	nkn_appendto_eventslist(&(so->pnknevents), pev);
	so->tot_accepted_len += len;
	pev->this_accepted_len=so->tot_accepted_len;

        //printf("nkn_send: pbuf=%p len=%ld\n", buf, len);
        nkn_get_more_data(so, 0);
        my_nkn_pthread_mutex_unlock(&global_mutex);
        return len;
}

ssize_t nkn_send(int s, void *buf, size_t len, int flags)
{
	return nkn_send_fn(s, buf, len, flags, NULL);
}


#if 0
ssize_t nkn_send(int s, void *buf, size_t len, int flags)
{
	struct msghdr msg;
	struct iovec aiov;
	int error;

	my_nkn_pthread_mutex_lock(&global_mutex);

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	msg.msg_control = 0;
	aiov.iov_base = buf;
	aiov.iov_len = len;
	error = nkn_kern_sendit(NULL, s, &msg, flags, NULL, UIO_USERSPACE);
	my_nkn_pthread_mutex_unlock(&global_mutex);
//	printf("my_nkn_pthread_mutex_unlock: (7)\n");
	//nkn_usleep(10); // BUG To be fixed.
	if(error==0) return len;
	nkn_errno=error;
	return (-1);
}
#endif //0

ssize_t nkn_write_fn(int s, void *buf, size_t len, nkn_data_sent_callback fn)
{
	return nkn_send_fn(s, buf, len, 0, fn);
}

ssize_t nkn_write(int s, void *buf, size_t len)
{
	return nkn_send_fn(s, buf, len, 0, NULL);
}

ssize_t nkn_writev_fn(int s, struct iovec *vector, int count, nkn_data_sent_callback fn)
{
	ssize_t size=0;
	int i;
        struct nkn_socket_events * pev;
	struct socket * so;

	my_nkn_pthread_mutex_lock(&global_mutex);

        so = CONVERT_ID_TO_SOCKET(s);
        if(!so) {
                //error = EINVAL;
                my_nkn_pthread_mutex_unlock(&global_mutex);
                return (-1);
        }
	for(i=0; i<count; i++)
	{
		pev=(struct nkn_socket_events *)nkn_netmalloc(sizeof(struct nkn_socket_events));
        	if(!pev) {
                	my_nkn_pthread_mutex_unlock(&global_mutex);
                	return (-1);
        	}
		pev->next=NULL;
        	pev->event=NKN_SEND_DATA;
        	pev->h.d.pbuf=vector->iov_base;
        	pev->h.d.lbuf=vector->iov_len;
        	pev->h.d.porgbuf=vector->iov_base;
        	pev->h.d.lorgbuf=vector->iov_len;
		pev->callback=fn;

		nkn_appendto_eventslist(&(so->pnknevents), pev);
		so->tot_accepted_len += vector->iov_len;
		pev->this_accepted_len=so->tot_accepted_len;

		size += vector->iov_len;
	}

        nkn_get_more_data(so, 0);
        my_nkn_pthread_mutex_unlock(&global_mutex);
        return size;
}

ssize_t nkn_writev(int s, struct iovec *vector, int count)
{
	return nkn_writev_fn(s, vector, count, NULL);
}

ssize_t nkn_sendmsg(int s, const struct msghdr *msg, int flags)
{
	my_nkn_pthread_mutex_lock(&global_mutex);
	//printf("ERROR: Calling nkn_sendmsg\n");
	my_nkn_pthread_mutex_unlock(&global_mutex);
		//printf("my_nkn_pthread_mutex_unlock: (9)\n");
	return 0;
}

ssize_t nkn_sendto(int  s,  const  void *buf, size_t len, int flags, const
       struct sockaddr *to, socklen_t tolen)
{
	my_nkn_pthread_mutex_lock(&global_mutex);
	//printf("ERROR: Calling nkn_sendto\n");
	my_nkn_pthread_mutex_unlock(&global_mutex);
		//printf("my_nkn_pthread_mutex_unlock: (10)\n");
	return 0;
}

static int
nkn_kern_setsockopt(
	struct thread *td,
	int s,
	int level,
	int name,
	void *val,
	enum uio_seg valseg,
	socklen_t valsize);
static int
nkn_kern_setsockopt(td, s, level, name, val, valseg, valsize)
	struct thread *td;
	int s;
	int level;
	int name;
	void *val;
	enum uio_seg valseg;
	socklen_t valsize;
{
	int error;
	struct socket *so;
	struct sockopt sopt;

	so = CONVERT_ID_TO_SOCKET(s);
	if(!so) {
		error = EINVAL;
		return (error);
	}

	if (val == NULL && valsize != 0)
		return (EFAULT);
	if ((int)valsize < 0)
		return (EINVAL);

	sopt.sopt_dir = SOPT_SET;
	sopt.sopt_level = level;
	sopt.sopt_name = name;
	sopt.sopt_val = val;
	sopt.sopt_valsize = valsize;
	switch (valseg) {
	case UIO_USERSPACE:
		sopt.sopt_td = td;
		break;
	case UIO_SYSSPACE:
		sopt.sopt_td = NULL;
		break;
	default:
		panic("kern_setsockopt called with bad valseg");
	}

	error = sosetopt(so, &sopt);
	return(error);
}

int     nkn_setsockopt(int  s, int level, int optname, void *optval, socklen_t optlen)
{
	int ret;
	//printf("Calling nkn_setsockopt\n");
	my_nkn_pthread_mutex_lock(&global_mutex);
	ret = nkn_kern_setsockopt(NULL, s, level, optname, optval, UIO_USERSPACE, optlen);
	my_nkn_pthread_mutex_unlock(&global_mutex);
	return ret;
}

struct protoent * nkn_getprotobyname(char *name)
{
	return NULL;
}

int     nkn_shutdown(int s, int how)
{
	struct socket *so;
        struct nkn_socket_events * pev;

	my_nkn_pthread_mutex_lock(&global_mutex);
	so = CONVERT_ID_TO_SOCKET(s);
	if(!so) {
		//printf("nkn_shutdown: so==NULL\n");
		my_nkn_pthread_mutex_unlock(&global_mutex);
		return -1;
	}

	// Otherwise, we append the events here
        pev=(struct nkn_socket_events *)nkn_netmalloc(sizeof(struct nkn_socket_events));
        if(!pev) {
                my_nkn_pthread_mutex_unlock(&global_mutex);
                return (-1);
        }
	pev->next=NULL;
        pev->event=NKN_SHUTDOWN;
        pev->h.shutdown_how=how;

	// Append the event
	nkn_appendto_eventslist(&(so->pnknevents), pev);
	(so->tot_accepted_len)++;
	pev->this_accepted_len=so->tot_accepted_len;

        nkn_get_more_data(so, 0);
	my_nkn_pthread_mutex_unlock(&global_mutex);
	return (0);
}

int 	nkn_close(int s)
{
        struct socket *so;
        int error;

        my_nkn_pthread_mutex_lock(&global_mutex);
        so = CONVERT_ID_TO_SOCKET(s);
        if(!so) {
                printf("nkn_close: so==NULL\n");
                my_nkn_pthread_mutex_unlock(&global_mutex);
                return -1;
        }
        //printf("Calling nkn_shutdown\n");
        error = soclose(so);
        //printf("Calling nkn_shutdown error=%d\n", error);
        DEL_SOCKET_BY_ID(s);
        my_nkn_pthread_mutex_unlock(&global_mutex);
                //printf("my_nkn_pthread_mutex_unlock: (11)\n");
        return (error);
}

int     nkn_socket(int domain, int type, int protocol)
{
	struct socket *so;
	int error;
	int s;

	printf("====> nkn_socket called\n");

	my_nkn_pthread_mutex_lock(&global_mutex);
	//printf("nkn_socket called\n");
	error = socreate(domain, &so, type, protocol, NULL, NULL);
	if (error) {
		my_nkn_pthread_mutex_unlock(&global_mutex);
		return -1;
	}

	//printf("Calling nkn_socket\n");
	//printf("so=0x%lx\n", (long)so);
	s = GET_ID_FOR_SOCKET(so);
	my_nkn_pthread_mutex_unlock(&global_mutex);
		//printf("my_nkn_pthread_mutex_unlock: (12)\n");
	return s;
}

int     nkn_sockatmark(int s)
{
	printf("ERROR: nkn_sockatmark not implemented\n");
	return 0;
}

int     nkn_socketpair(int d, int type, int protocol, int sv[2])
{
	printf("ERROR: nkn_socketpair not implemented\n");
	return 0;
}

int nkn_soo_ioctl(struct socket *so, u_long cmd, void *data);
int nkn_soo_ioctl(struct socket *so, u_long cmd, void *data)
{
        int error = 0;

#ifndef MAXHE_TODO
			// BUG here
                        so->so_state |= SS_NBIO;
			//printf("so->so_state=%d\n", so->so_state);
			return error;
#endif // MAXHE_TODO

        switch (cmd) {
        case FIONBIO:
                SOCK_LOCK(so);
                if (*(int *)data) {
                        so->so_state |= SS_NBIO;
			printf("so->so_state=%d\n", so->so_state);
		}
                else
                        so->so_state &= ~SS_NBIO;
                SOCK_UNLOCK(so);
                break;

        case FIOASYNC:
                /*
                 * XXXRW: This code separately acquires SOCK_LOCK(so) and
                 * SOCKBUF_LOCK(&so->so_rcv) even though they are the same
                 * mutex to avoid introducing the assumption that they are
                 * the same.
                 */
                if (*(int *)data) {
                        SOCK_LOCK(so);
                        so->so_state |= SS_ASYNC;
                        SOCK_UNLOCK(so);
                        SOCKBUF_LOCK(&so->so_rcv);
                        so->so_rcv.sb_flags |= SB_ASYNC;
                        SOCKBUF_UNLOCK(&so->so_rcv);
                        SOCKBUF_LOCK(&so->so_snd);
                        so->so_snd.sb_flags |= SB_ASYNC;
                        SOCKBUF_UNLOCK(&so->so_snd);
                } else {
                        SOCK_LOCK(so);
                        so->so_state &= ~SS_ASYNC;
                        SOCK_UNLOCK(so);
                        SOCKBUF_LOCK(&so->so_rcv);
                        so->so_rcv.sb_flags &= ~SB_ASYNC;
                        SOCKBUF_UNLOCK(&so->so_rcv);
                        SOCKBUF_LOCK(&so->so_snd);
                        so->so_snd.sb_flags &= ~SB_ASYNC;
                        SOCKBUF_UNLOCK(&so->so_snd);
                }
                break;
        case FIONREAD:
                /* Unlocked read. */
                *(int *)data = so->so_rcv.sb_cc;
                break;

        case FIOSETOWN:
                error = fsetown(*(int *)data, &so->so_sigio);
                break;

        case FIOGETOWN:
                *(int *)data = fgetown(&so->so_sigio);
                break;

        case SIOCSPGRP:
                error = fsetown(-(*(int *)data), &so->so_sigio);
                break;

        case SIOCGPGRP:
                *(int *)data = -fgetown(&so->so_sigio);
                break;

        case SIOCATMARK:
                /* Unlocked read. */
                *(int *)data = (so->so_rcv.sb_state & SBS_RCVATMARK) != 0;
                break;
        default:
#ifdef MAXHE_TODO
                /*
                 * Interface/routing/protocol specific ioctls: interface and
                 * routing ioctls should have a different entry since a
                 * socket is unnecessary.
                 */
                if (IOCGROUP(cmd) == 'i')
                        error = ifioctl(so, cmd, data, td);
                else if (IOCGROUP(cmd) == 'r')
                        error = rtioctl(cmd, data);
                else
                        error = ((*so->so_proto->pr_usrreqs->pru_control)
                            (so, cmd, data, 0, td));
#endif // MAXHE_TODO
                break;
        }
        return (error);
}

int     nkn_ioctl(int s, unsigned long cmd, void * data)
{
	struct socket *so;
	int ret;

	//printf("nkn_ioctl: called\n");
	my_nkn_pthread_mutex_lock(&global_mutex);
	so = CONVERT_ID_TO_SOCKET(s);
	ret=nkn_soo_ioctl(so, cmd, data);
	my_nkn_pthread_mutex_unlock(&global_mutex);
	return ret;
}

////////////////////////////////////////////////////////////////////////////////
// epoll related functions

#define MAX_EPS_SIZE	10
#define MAX_EPOLL_SIZE	102400

static struct eps_fd {
	struct epoll_event * ep;
	int size;
	int max_fd;
} epsfd[MAX_EPS_SIZE];
static int max_epsfd=0;

int nkn_epoll_create(int size)
{
	struct epoll_event * ep=NULL;
	int i;

	my_nkn_pthread_mutex_lock(&global_mutex);
	if(size<=0) {
		printf("nkn_epoll_create: 1\n");
		nkn_errno=EINVAL;
		my_nkn_pthread_mutex_unlock(&global_mutex);
		return -1;
	}

	if(size>MAX_EPOLL_SIZE) {
		printf("nkn_epoll_create: size=%d MAX_EPOLL_SIZE=%d\n", size, MAX_EPOLL_SIZE);
		nkn_errno=ENFILE;
		my_nkn_pthread_mutex_unlock(&global_mutex);
		return -1;
	}

	if(max_epsfd >= MAX_EPS_SIZE-1) {
		printf("nkn_epoll_create: size=%d MAX_EPS_SIZE=%d\n", size, MAX_EPS_SIZE);
		nkn_errno=ENFILE;
		my_nkn_pthread_mutex_unlock(&global_mutex);
		return -1;
	}

	ep=(struct epoll_event *)nkn_netmalloc(size * sizeof(struct epoll_event));
	if( !ep ) {
		nkn_errno=ENOMEM;
		my_nkn_pthread_mutex_unlock(&global_mutex);
		return -1;
	}
	memset(ep, 0, size * sizeof(struct epoll_event));

	for(i=0;i<MAX_EPS_SIZE;i++) {
		if(epsfd[i].ep==NULL) {
			epsfd[i].ep=ep;
			epsfd[i].size=size;
			epsfd[i].max_fd=0;
			if(i>max_epsfd) max_epsfd=i;
			my_nkn_pthread_mutex_unlock(&global_mutex);
			return i;
		}
	}
	nkn_netfree((void **)&ep);
	nkn_errno=ENOMEM;
	my_nkn_pthread_mutex_unlock(&global_mutex);
        return -1;
}

int nkn_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
{
	struct eps_fd * pepsfd;
	struct epoll_event * ep, * ep1;
	int i;
	int ret=-1;

	my_nkn_pthread_mutex_lock(&global_mutex);
	//printf("nkn_epoll_ctl: op=%d \n", op);
	if(epfd<0 || epfd >= MAX_EPS_SIZE) {
		nkn_errno=EBADF;
		goto done;
	}

	//printf("nkn_epoll_ctl: op=%d \n", op);
	pepsfd=&epsfd[epfd];
	if(pepsfd == NULL) {
		nkn_errno=EBADF;
		goto done;
	}

	//printf("nkn_epoll_ctl: op=%d \n", op);
	switch(op) {

	case EPOLL_CTL_ADD:
	if(pepsfd->max_fd >= pepsfd->size-1) {
		nkn_errno=ENOMEM;
		goto done;
	}
	ep = pepsfd->ep + pepsfd->max_fd;
	ep->events = event->events;
	ep->data.fd = fd;
	pepsfd->max_fd++;
	ret=0;
	break;

	case EPOLL_CTL_MOD:
	for(i=0; i<pepsfd->max_fd; i++) {
		ep = pepsfd->ep + i;
		if(ep->data.fd == fd) {
			// got you. make a modification
			ep->events = event->events;
			ret=0;
			break;
		}
	}
	break;

	case EPOLL_CTL_DEL:
	for(i=0; i<pepsfd->max_fd; i++) {
		ep = pepsfd->ep + i;
		if(ep->data.fd == fd) {
			// got you. Let's delete
			for( ; i<pepsfd->max_fd-1; i++) {
				ep = pepsfd->ep + i;
				ep1 = pepsfd->ep + (i+1);
				ep->events = ep1->events;
				ep->data.fd = ep1->data.fd;
			}
			pepsfd->max_fd--;
			ret=0;
			break;
		}
	}
	break;

	default: // Others: not supported yet
	break;
	}

done:
	my_nkn_pthread_mutex_unlock(&global_mutex);
	return ret;
}
	
int nkn_epoll_wait(int epfd, struct epoll_event * events, int maxevents, int timeout)
{
	struct eps_fd * pepsfd;
	int ret=-1;
	struct socket * head;
	struct epoll_event * ep;
	int i;//, *pfd;

	my_nkn_pthread_mutex_lock(&global_mutex);
	if(epfd < 0 || epfd > max_epsfd) {
		nkn_errno=EBADF;
		goto done;
	}

	pepsfd=&epsfd[epfd];
	while(1) {
		ret=0;

		ep = pepsfd->ep;
		//printf("nkn_epoll_wait: pepsfd->max_fd=%d\n", pepsfd->max_fd);
		for(i=0; i<pepsfd->max_fd;i++, ep++) {

			//printf("nkn_epoll_wait: data fd=%d\n", ep->data.fd);
        		head=CONVERT_ID_TO_SOCKET(ep->data.fd);
			if(!head) {
				printf("nkn_epoll_wait: failed to convert fd to socket\n");
				continue; // BUG:
			}
        		if ((head->so_options & SO_ACCEPTCONN) && 
			    (ep->events & EPOLLIN)) {
        			if ( !TAILQ_EMPTY(&head->so_comp) && (head->so_qlen>0) ) {
					//printf("nkn_epoll_wait: listenfd =%d\n", ep->data.fd);
					events->events=EPOLLIN;
					events->data.fd=ep->data.fd;
					ret++;
					events++;
        			}
			}
			else if((head->so_rcv.sb_mb != NULL) &&
				(ep->events & EPOLLIN)) {
					//printf("nkn_epoll_wait: data fd=%d\n", ep->data.fd);
					events->events=EPOLLIN;
					events->data.fd=ep->data.fd;
					ret++;
					events++; 
			}
			else if(ep->events & EPOLLOUT) {
					// We have infinite buffer, 
					// so we always returns if EPOLLOUT is set
					//printf("nkn_epoll_wait: data fd=%d\n", ep->data.fd);
					events->events=EPOLLOUT;
					events->data.fd=ep->data.fd;
					ret++;
					events++; 
			}
		}
		if(ret) {
			//printf("nkn_epoll_wait: return =%d\n", ret);
			goto done;
		}

		//printf("nkn_epoll_wait: nkn_usleep=0x%lx\n", (long)nkn_usleep);
		my_nkn_pthread_mutex_unlock(&global_mutex);
		nkn_usleep(1200);
		my_nkn_pthread_mutex_lock(&global_mutex);
		if(timeout != -1) {
			timeout -= 1200;
			//printf("nkn_epoll_wait: timeout=%d\n", timeout);
			if(timeout < 0) { goto done; }
		}
	}

done:
	//printf("nkn_epoll_wait: returns ret=%d\n", ret);
	my_nkn_pthread_mutex_unlock(&global_mutex);
	return ret;
}


void nkn_epoll_close(int epfd)
{
	my_nkn_pthread_mutex_lock(&global_mutex);
	if(epfd>=0 && epfd < MAX_EPS_SIZE) {
		epsfd[epfd].ep=NULL;
	}
	my_nkn_pthread_mutex_unlock(&global_mutex);
}

