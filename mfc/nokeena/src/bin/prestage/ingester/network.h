#ifndef _NETWORK_H_
#define _NETWORK_H_


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <errno.h>
#include <pthread.h>
#include <sys/un.h>
#include "ttime.h"

#define MAX_BUF	    (16384)



enum {
    CONNECTING = 1,
    CONNECTED,
    CLOSING,
    CLOSED
};

struct socku {
    int			_fd;
    int			_state;
    pthread_mutex_t	_mutex;

    int (*_epollin)(struct socku	*);
    int (*_epollout)(struct socku	*);
    int (*_epollerr)(struct socku	*);
    int (*_epollhup)(struct socku	*);
#define	    sku_fd	_sku._fd
#define	    sku_state	_sku._state
#define	    sku_lock	_sku._mutex
#define	    epollin	_sku._epollin
#define	    epollerr	_sku._epollerr
#define	    epollout	_sku._epollout
#define	    epollhup	_sku._epollhup
};

struct network {
    struct socku    _sku;

    struct sockaddr_in sip;
    struct sockaddr_in dip;
    struct epoll_event event;

    char *request;
    char  buf[MAX_BUF];
    char *abs_path;
    char *uri;
    char *host;
    char *ns;
    char *time;
    uint32_t txlen;
    uint64_t rxlen;
    uint32_t buflen;
    uint64_t size;
    uint64_t expected_len;
    uint32_t status;
    int	    header;
    uint32_t req_status;
};


struct uds {
    struct socku    _sku;

    struct sockaddr_un	sip;
    struct sockaddr_un	dip;
    struct epoll_event event;
    char *buf;
    int rxlen;
};




static inline struct network *net_sku(struct socku *sku) {
    return (struct network *) (sku);
}

static inline struct uds *uds_sku(struct socku *sku) {
    return (struct uds*) (sku);
}

static inline int net_get(struct socku	*sku) {
    struct network *net = net_sku(sku);;
    pthread_mutex_lock(&(net->sku_lock));
    return 0;
}

static inline int  net_put(struct socku *sku) {
    struct network *net = net_sku(sku);;
    pthread_mutex_unlock(&(net->sku_lock));
    return 0;
}


static inline int sku_get(struct socku	*sku) {
    pthread_mutex_lock(&(sku->_mutex));
    return 0;
}

static inline int  sku_put(struct socku *sku) {
    pthread_mutex_unlock(&(sku->_mutex));
    return 0;
}


static inline int uds_get(struct socku	*sku) {
    pthread_mutex_lock(&(sku->_mutex));
    return 0;
}

static inline int  uds_put(struct socku *sku) {
    pthread_mutex_unlock(&(sku->_mutex));
    return 0;
}
int log_to_syslog( int level, const char *format, ...)
     __attribute__ ((format (printf, 2, 3)));
#endif
