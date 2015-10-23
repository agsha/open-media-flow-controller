#ifndef __LOG_CHANNEL_H__
#define __LOG_CHANNEL_H__


#include <sys/epoll.h>
#include <atomic_ops.h>
#include <sys/un.h>

#include "nknlogd.h"


#define	UDS_PROVIDER_LOGD	    "uds-provider-logd"
#define UDS_CLIENT_ACCESSLOG	    "uds-client-accesslog"
#define UDS_CLIENT_ERRORLOG	    "uds-client-errorlog"
#define UDS_CLIENT_CACHELOG	    "uds-client-cachelog"

#define MAX_CHANNELS	    (128)

struct channel;


struct acclog_profile {

        uint64_t max_filesize;
        int max_fileid;
	int rt_interval;  //roration interval in hours
	char * filename;
	/* Accesslog */
	//format_field_t *ff_ptr;
	int ff_used;
	int ff_size;
	uint32_t r_size;	//in k-bytes
	int32_t *r_code;	//resp code 200 206
	char * remote_url;
	char * remote_pass;
	char *format;
};



typedef int (*NM_callback)(struct channel *);
typedef int (*NM_timeout)(struct channel*, double);
//typedef int (*NM_timer)(struct channel*, net_timer_type_t);
typedef void (*NM_destructor)(struct channel*);

enum channel_type {
    UNKNOWNLOG = 0,
    ACCESSLOG = 1,
    ERRORLOG,
    STREAMLOG,
    PUBLISHLOG,
    CACHELOG,
    CRAWLLOG
};


struct socku {
    int                         _fd;
    enum channel_type           _type;
    struct epoll_event          _event; // {data.ptr = this}
    AO_t                        _refcnt;
    pthread_mutex_t		_mutex;

    int (*_epollin)(struct socku	*);
    int (*_epollout)(struct socku	*);
    int (*_epollerr)(struct socku	*);
    int (*_epollhup)(struct socku	*);

#define	    sku_fd	_sku._fd
#define	    sku_state	_sku._state
#define	    sku_lock	_sku._mutex
#define	    sku_type	_sku._type
#define	    sku_event	_sku._event
#define	    epollin	_sku._epollin
#define	    epollerr	_sku._epollerr
#define	    epollout	_sku._epollout
#define	    epollhup	_sku._epollhup
    //NM_timeout          skuc_timeout;
    //NM_timer            skuc_timer;

    /* Should never be called by anyone other than NM.
    *      * This is private for all practical purposes
    *           */
    NM_destructor       __destructor;
};

static inline int sku_get(struct socku	*sku) {
    pthread_mutex_lock(&(sku->_mutex));
    return 0;
}

static inline int  sku_put(struct socku *sku) {
    pthread_mutex_unlock(&(sku->_mutex));
    return 0;
}


struct uds {
    struct socku    _sku;

    struct sockaddr_un	sip;
    struct sockaddr_un	dip;
    //struct epoll_event event;
};



enum channel_state {
    cs_CLOSED = 0,
    cs_HELO =		1 << 1,
    cs_ONLINE =		1 << 2,

    cs_INEPOLL =	1 << 3,
    cs_MARK_DELETE =	1 << 4,
    cs_INUSE =		1 << 5,
};

struct al_data {
    char buf[MAX_BUFSIZE];
    uint32_t offsetlen;
    uint32_t totlen;
};


struct channel;


struct channel {
    struct socku	    _sku;


    /* What addressing scheme we use */
    union {

	/* For legacy log applications that still prefer to connect over
	 * local loopback address, using TCP
	 */
	struct {
	    struct sockaddr_in saddr;
	    struct sockaddr_in daddr;
	} inet;

	/* For newer applications that would like to use UDS instead */
	struct {
	    struct sockaddr_un  udsaddr;
	} uds;
    } addr;

    enum channel_state	    state;
    int			    index;
    char		    info[64];

    char		    *name;
    struct logd_file_t	    *accesslog;

    struct al_data	data;

    struct channel	    *parent;
    struct channel	    *child;

};


extern int log_channel_init(void);
extern int log_channel_add(const char *name, struct channel *channel);
extern int log_channel_delete(const char *name, struct channel **channel);
extern int log_channel_get(const char *name, struct channel **channel);
extern int log_channel_for_each(void (*iterator)(void *, void *, void *), void *user);
extern void log_channel_free(struct channel *c);
extern struct channel *log_channel_new(const char *name);
extern void isempty_name_channel(const char *name, struct channel **channel, const char *func_name);



static inline void log_channel_set_type(struct channel* c, enum channel_type type, void *p)
{
    c->sku_type = type;
    (void) p;
}

static inline enum channel_type log_channel_get_type(struct channel* c)
{
    return c->sku_type;
}

#endif
