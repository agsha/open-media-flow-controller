#ifndef __NKN__API__
#define __NKN__API__

#include "nknshim/nkns_osl.h"

enum EPOLL_EVENTS
  {
    EPOLLIN = 0x001,
#define EPOLLIN EPOLLIN
    EPOLLPRI = 0x002,
#define EPOLLPRI EPOLLPRI
    EPOLLOUT = 0x004,
#define EPOLLOUT EPOLLOUT
    EPOLLRDNORM = 0x040,
#define EPOLLRDNORM EPOLLRDNORM
    EPOLLRDBAND = 0x080,
#define EPOLLRDBAND EPOLLRDBAND
    EPOLLWRNORM = 0x100,
#define EPOLLWRNORM EPOLLWRNORM
    EPOLLWRBAND = 0x200,
#define EPOLLWRBAND EPOLLWRBAND
    EPOLLMSG = 0x400,
#define EPOLLMSG EPOLLMSG
    EPOLLERR = 0x008,
#define EPOLLERR EPOLLERR
    EPOLLHUP = 0x010,
#define EPOLLHUP EPOLLHUP
    EPOLLONESHOT = (1 << 30),
#define EPOLLONESHOT EPOLLONESHOT
    EPOLLET = (1 << 31)
#define EPOLLET EPOLLET
  };

/* Valid opcodes ( "op" parameter ) to issue to epoll_ctl().  */
#define EPOLL_CTL_ADD 1 /* Add a file decriptor to the interface.  */
#define EPOLL_CTL_DEL 2 /* Remove a file decriptor from the interface.  */
#define EPOLL_CTL_MOD 3 /* Change file decriptor epoll_event structure.  */

/* should be aligned by 12 bytes */

#pragma pack(1)

typedef union epoll_data {
        void *ptr;
        int fd;
        __uint32_t u32;
        __uint64_t u64;
} epoll_data_t;

struct epoll_event {
        __uint32_t events;      /* Epoll events */
        epoll_data_t data;      /* User data variable */
};
#pragma pack()

typedef void (*nkn_data_sent_callback) (char * p, int len);

extern  unsigned long vifip;  	// host order
extern  unsigned long vnetmask; // host order
extern  unsigned long vsubnet;  // host order

extern int nkn_errno;

void 	nkn_100ms_timer(void * tv);
void 	nkn_1s_timer(void * tv);
int 	nkn_add_ifip(unsigned long ifip, unsigned long netmask, unsigned long subnet);
int     nkn_ip_input(char * p, int len);
void    nkn_debug_verbose(int verbose);

int     nkn_geterrno(void);
int     nkn_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int     nkn_bind(int sockfd, struct sockaddr *my_addr, socklen_t addrlen);
int     nkn_connect(int sockfd, struct sockaddr *serv_addr, socklen_t addrlen);
int     nkn_getpeername(int s, struct sockaddr *name, socklen_t *namelen);
int     nkn_getsockname(int s, struct sockaddr *name, socklen_t *namelen);
int     nkn_getsockopt(int s, int level, int optname, void *optval, socklen_t *optlen);
int     nkn_listen(int sockfd, int backlog);

ssize_t nkn_recv(int s, void *buf, size_t len, int flags);
ssize_t nkn_read(int s, void *buf, size_t len);
ssize_t nkn_readv(int fd, struct iovec *vector, int count);
ssize_t nkn_recvmsg(int s, struct msghdr *msg, int flags);
ssize_t nkn_recvfrom(int s, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen);

ssize_t nkn_send(int s, void *buf, size_t len, int flags);
ssize_t nkn_writev(int fd, struct iovec *vector, int count);
ssize_t nkn_write(int s, void *buf, size_t len);
ssize_t nkn_send_fn(int s, void *buf, size_t len, int flags, nkn_data_sent_callback fn);
ssize_t nkn_writev_fn(int fd, struct iovec *vector, int count, nkn_data_sent_callback fn);
ssize_t nkn_write_fn(int s, void *buf, size_t len, nkn_data_sent_callback fn);
ssize_t nkn_sendmsg(int s, const struct msghdr *msg, int flags);
ssize_t nkn_sendto(int s, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen);
int     nkn_setsockopt(int s, int level, int optname, void *optval, socklen_t optlen);
int     nkn_shutdown(int s, int how);
int     nkn_close(int s);
int     nkn_socket(int domain, int type, int protocol);
int     nkn_sockatmark(int s);
int     nkn_socketpair(int d, int type, int protocol, int sv[2]);
int	nkn_ioctl(int s, unsigned long cmd, void * data);

int 	nkn_epoll_create(int size);
int 	nkn_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
int 	nkn_epoll_wait(int epfd, struct epoll_event * events, int maxevents, int timeout);
void 	nkn_epoll_close(int epfd);
struct protoent * nkn_getprotobyname(char *name);

#endif // __NKN__API__



