#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/prctl.h>
#include <dlfcn.h>
#include <uuid/uuid.h>
#include <sys/epoll.h>
#include <poll.h>


#include "nkn_debug.h"
#include "server.h"
#include "network.h"
#include "nkn_defs.h"
#include "nkn_stat.h"
#include "nvsd_mgmt.h"
#include "nkn_cfg_params.h"
#include "rtsp_func.h"
#include "rtsp_server.h"
#include "rtsp_session.h"
#include "rtsp_vpe_nkn_fmt.h"


extern uint16_t glob_tot_svr_sockets;
extern void init_rtsp_session(void);
extern void init_DelayQueue(void);
//extern void init_rtsp_livesvr(void);
extern void nkn_rtsp_live_init(void);

static pthread_mutex_t cfg_so_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct rtsp_so_libs * g_rtsp_so = NULL;
struct rtsp_so_libs * rtsp_get_so_libname(char * ext);

char rtsp_listen_intfname[MAX_NKN_INTERFACE][16];
int rtsp_listen_intfcnt = 0;


/*
 * ===============> server socket <===============================
 * The following functions are designed for server socket.
 * we need to provde epollin/epollout/epollerr/epollhup functions
 */

static int rtspsvr_epollin(int sockfd, void * private_data)
{
	static unsigned long long fSessionIdCounter = 0;
	int clifd;
	int cnt;
	struct sockaddr_in addr;
	socklen_t addrlen;
	nkn_interface_t * pns = (nkn_interface_t *)private_data;
	uuid_t uid;

	UNUSED_ARGUMENT(sockfd);

	/* always returns TRUE for this case */

	for(cnt=0; ; cnt++) {

		//printf("cnt=%d\n", cnt);
		addrlen=sizeof(struct sockaddr_in);
		clifd = accept(sockfd, (struct sockaddr *)&addr, &addrlen);
		if (clifd == -1) {
			return TRUE;
		}
		nkn_mark_fd(clifd, RTSP_FD);

		// valid socket
		if(NM_set_socket_nonblock(clifd) == FALSE) {
			DBG_LOG(MSG, MOD_RTSP, 
				"Failed to set socket %d to be nonblocking socket.",
				clifd);
			nkn_close_fd(clifd, RTSP_FD);
			return TRUE;
		}

		// Bind this socket into this accepted interface 
		NM_bind_socket_to_interface(clifd, pns->if_name);

		DBG_LOG(MSG, MOD_RTSP, "rtspsvr_epollin: accepted socket %d from %s\n", 
			clifd, inet_ntoa(addr.sin_addr));

		uuid_generate(uid);
		fSessionIdCounter=((unsigned long long)((*(uint32_t *)&uid[0]) ^ (*(uint32_t *)&uid[4])) << 32 ) | 
			          ((unsigned long long)((*(uint32_t *)&uid[8]) ^ (*(uint32_t *)&uid[12])) );
		if(nkn_new_prtsp(fSessionIdCounter, clifd, addr, pns) == NULL) {
			nkn_close_fd(clifd, RTSP_FD);
			return TRUE;
		}
	}
	return TRUE;
}

static int rtspsvr_epollout(int sockfd, void * private_data)
{
	UNUSED_ARGUMENT(private_data);
	DBG_LOG(SEVERE, MOD_RTSP, "epollout should never called. sockid=%d", sockfd);
	assert(0); // should never called
	return FALSE;
}

static int rtspsvr_epollerr(int sockfd, void * private_data)
{
	UNUSED_ARGUMENT(private_data);
	DBG_LOG(SEVERE, MOD_RTSP, "epollerr should never called. sockid=%d", sockfd);
	assert(0); // should never called
	return FALSE;
}

static int rtspsvr_epollhup(int sockfd, void * private_data)
{
	UNUSED_ARGUMENT(private_data);
	DBG_LOG(SEVERE, MOD_RTSP, "epollhup should never called. sockid=%d", sockfd);
	assert(0); // should never called
	return FALSE;
}


void init_rtsp_vodsvr(void);
void init_rtsp_vodsvr(void)
{
	struct sockaddr_in srv;
	int ret;
	int val;
	int i, j;
	int rtspsvr_listenfd;
	int listen_all = 1;

	/* Check if any interface is enabled else listen on all
	 * interface 
	 */

        for(i=0; i<rtsp_listen_intfcnt; i++) 
        {
	    for(j=0; j< glob_tot_interface; j++) 	   
	    {
   	        if(strcmp(interface[j].if_name, rtsp_listen_intfname[i]) == 0)
		{
			interface[j].enabled = 1;
		   	listen_all = 0;
		}
	    }
        } 

	for(i=0; i<glob_tot_interface; i++) 
	{
           if ( ( listen_all == 0 ) &&
		      ( interface[i].enabled != 1 ) && 
		      ( strcmp(interface[i].if_name, "lo") !=0 ))
	      continue;

	   for(j=0; j<MAX_PORTLIST; j++) {

		if(nkn_rtsp_vod_serverport[j] == 0) {
			continue;
		}

		if( (rtspsvr_listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		{
			DBG_LOG(SEVERE, MOD_RTSP, "failed to create a socket errno=%d", errno);
			continue;
		}

		bzero(&srv, sizeof(srv));
		srv.sin_family = AF_INET;
		srv.sin_addr.s_addr = interface[i].if_addrv4;
		srv.sin_port = htons(nkn_rtsp_vod_serverport[j]);

		val = 1;
		ret = setsockopt(rtspsvr_listenfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
		if (ret == -1) {
			DBG_LOG(SEVERE, MOD_RTSP, "failed to set socket option errno=%d", errno);
			close(rtspsvr_listenfd);
			continue;
		}

		if( bind(rtspsvr_listenfd, (struct sockaddr *) &srv, sizeof(srv)) < 0)
		{
			DBG_LOG(SEVERE, MOD_RTSP, 
				"failed to bind server socket to port %d (errno=%d)", 
				nkn_rtsp_vod_serverport[j], errno);
			close(rtspsvr_listenfd);
			continue;
		}

		// We will use nonblocking accept listen socket
		NM_set_socket_nonblock(rtspsvr_listenfd);

		// Bind this socket into this accepted interface 
		NM_bind_socket_to_interface(rtspsvr_listenfd, interface[i].if_name);

		listen(rtspsvr_listenfd, 1000);

		if(register_NM_socket(rtspsvr_listenfd, 
			&interface[i],
			rtspsvr_epollin,
			rtspsvr_epollout,
			rtspsvr_epollerr,
			rtspsvr_epollhup,
			NULL,
			NULL, 	// timeout function callback
			0,	// timeout value
			USE_LICENSE_FALSE,
			TRUE) == FALSE)
		{
			// will this case ever happen at all?
			close(rtspsvr_listenfd);
			continue;
		}
		NM_add_event_epollin(rtspsvr_listenfd);

		gnm[rtspsvr_listenfd].accepted_thr_num = gnm[rtspsvr_listenfd].pthr->num;

		glob_tot_svr_sockets ++;
	   }
	}
}



//////////////////////////////////////////////////////////////////
// Task scheduler
//////////////////////////////////////////////////////////////////

#define MAX_EPOLL_EVENTS	(4000 * 5)
#define MAX_RTSP_CONNECTIONS	(4000 * 5)

struct select_handler_ht {
        struct select_handler_ht * next;
        int fd;
        BackgroundHandlerProc * handlerProc;
        void* clientData;
};
static struct select_handler_ht * g_sh_table = NULL;
static pthread_mutex_t sht_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_rtsp_epoll_fd;
static int fMaxNumSockets;
static struct epoll_event *g_rstp_epoll_events = NULL;


/////////////////////////////////////////////////////////////////////////////
void * RTSP_main(void * arg);
void * RTSP_main(void * arg)
{
	int nfds, i;
	struct select_handler_ht *psht;

	UNUSED_ARGUMENT(arg);
	fMaxNumSockets = 0;
	g_rstp_epoll_events = nkn_calloc_type(MAX_EPOLL_EVENTS, sizeof(struct epoll_event), mod_rtsp_sock_malloc);
	//Todo: get MAX connections from some where
	g_rtsp_epoll_fd = epoll_create(MAX_RTSP_CONNECTIONS);

	// Repeatedly loop, handling readble sockets and timed events:
	while (1) {
		memset(g_rstp_epoll_events, 0, MAX_EPOLL_EVENTS * sizeof(struct epoll_event));
		nfds =  epoll_wait(g_rtsp_epoll_fd, g_rstp_epoll_events, MAX_EPOLL_EVENTS, 2); // Time out 2ms
		for (i = 0 ; i < nfds ; i++ ) {
			psht = g_rstp_epoll_events[i].data.ptr;
			if (psht->handlerProc != NULL) {
				if ( g_rstp_epoll_events[i].events & EPOLLIN )
					(*psht->handlerProc)(psht->clientData, SOCKET_READABLE);
#if 0				
				else if ( g_rstp_epoll_events[i].events & EPOLLERR )
					(*psht->handlerProc)(psht->clientData, SOCKET_EXCEPTION);
				else if ( g_rstp_epoll_events[i].events & EPOLLHUP )
					(*psht->handlerProc)(psht->clientData, SOCKET_EXCEPTION);
#endif				
			}
		}
	}

	/* 
	 * thread exits, free all memeories 
	 */
	pthread_mutex_lock(&sht_mutex);
	while(g_sh_table) {
		psht = g_sh_table;
		g_sh_table = psht->next;
		free(psht);
	}
	pthread_mutex_unlock(&sht_mutex);
}

void nkn_doEventLoop(char * watchVariable)
{
	RTSP_main(watchVariable);
}

void nkn_register_epoll_read(int fd,
                                BackgroundHandlerProc* handlerProc,
                                void* clientData)
{
	struct select_handler_ht * psht;
	struct epoll_event ev;

	if (fd < 0) return;
	DBG_LOG(MSG, MOD_RTSP, "fd=%d", fd);

	pthread_mutex_lock(&sht_mutex);
	/*
	 * First, see if there's already a handler for this socket:
	 */
	psht = g_sh_table;
	while (psht) {
		if (psht->fd == fd) {
			break;
		}
		psht = psht->next;
	}

	/*
	 * If no exiting handler found, we will create a new descriptor.
	 */
	if (psht == NULL) {
		psht = (struct select_handler_ht *)nkn_malloc_type(sizeof(struct select_handler_ht), 
			mod_rtsp_soc_rsr_malloc);
		if (!psht) {
			pthread_mutex_unlock(&sht_mutex);
			return;
		}
		psht->next = g_sh_table;
		g_sh_table = psht;

		psht->fd = fd;
	}

	/*
	 * in both cases, we will overwritten the callback function.
	 */
	psht->handlerProc = handlerProc;
	psht->clientData = clientData;

	/*
	 * insert into the epoll loop
	 */
	ev.events = EPOLLIN;
	ev.data.ptr = psht;
	epoll_ctl(g_rtsp_epoll_fd, EPOLL_CTL_ADD, fd, &ev);
	fMaxNumSockets++;
	pthread_mutex_unlock(&sht_mutex);
}

void nkn_unregister_epoll_read(int fd)
{
	struct select_handler_ht * psht, * pshtnext;

	if (fd < 0) return;
	DBG_LOG(MSG, MOD_RTSP, "fd=%d", fd);

	pthread_mutex_lock(&sht_mutex);

	epoll_ctl(g_rtsp_epoll_fd, EPOLL_CTL_DEL, fd, NULL);
	--fMaxNumSockets;

	psht = g_sh_table;
	if (!psht) {
		pthread_mutex_unlock(&sht_mutex);
		return;
	}

	if (psht->fd == fd) {
		g_sh_table = psht->next;
		psht->handlerProc = NULL;
		psht->clientData = NULL;
		free(psht);
		pthread_mutex_unlock(&sht_mutex);
		return;
	}

	while (psht->next) {
		pshtnext = psht->next;
		if (pshtnext->fd == fd) {
			psht->next = pshtnext->next;
			pshtnext->handlerProc = NULL;
			pshtnext->clientData = NULL;
			free(pshtnext);
			pthread_mutex_unlock(&sht_mutex);
			return;
		}
		psht= pshtnext;
	}

	pthread_mutex_unlock(&sht_mutex);
}


////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////
extern struct vfs_funcs_t vfs_funs;

void nkn_rtsp_init(void);
void nkn_rtsp_init(void)
{
	pthread_t rtspid;
	char buf[200];

	/*
	 * Check if the feature is enabled or not
	 */
	if(rtsp_enable == 0) {
                DBG_LOG(MSG, MOD_RTSP, "RTSP is not configured to be enabled");
		return;
	}

	/*
	 * Check if the shared library exists or not.
	 * If yes, we load them.
	 */
	strcpy(buf, ANKEENA_FMT_EXT" libankeena_fmt.so.1.0.1");
	rtsp_cfg_so_callback(buf);
	
        /*
         * create RTSP working thread
         */
        int rv;
        pthread_attr_t attr;
        int stacksize = 128 * KiBYTES;

        rv = pthread_attr_init(&attr);
        if (rv) {
                DBG_LOG(SEVERE, MOD_RTSP,
                        "pthread_attr_init() failed, rv=%d", rv);
                return;
        }
        rv = pthread_attr_setstacksize(&attr, stacksize);
        if (rv) {
                DBG_LOG(SEVERE, MOD_RTSP,
                        "pthread_attr_setstacksize() stacksize=%d failed, "
                        "rv=%d", stacksize, rv);
                return;
        }
        if(pthread_create(&rtspid, &attr, RTSP_main, NULL)) {
                DBG_LOG(MSG, MOD_RTSP, "error in create RTSP_main thread");
                return;
        }

	init_rtsp_session(); // Initialize rtsp sessions
	init_DelayQueue(); // Initialize real-time scheduler
	init_rtsp_vodsvr();  // Initialize VOD server.
	nkn_rtsp_live_init(); // Initialize Live/Relay

        DBG_LOG(MSG, MOD_RTSP, "RTSP is enabled and ready");
}

//////////////////////////////////////////////////////////////////
// The following is for shared library management.
//////////////////////////////////////////////////////////////////

static char * get_next_token (char * pin, int * shift)
{
	char * token;
	char * p = pin;

	if(p==NULL) return NULL;

	// skip the leading space
	while(*p==' '||*p=='\t') {
		if(*p==0) return NULL; // No more token
		p++;
	}
	if(*p==0) return NULL; // No more token

	// starting pointer of token
	token = p;

	// mark the end of token
	while(*p!=' ') {
		if(*p==0) { goto done; }
		p++;
	}
	*p=0; p++;

done:
	*shift = p-pin;
	return token;
}


void rtsp_cfg_so_callback (char * s)
{
	char * ext, * lib;
	int len;
	struct rtsp_so_libs * p;
	char filename[1024];
	struct rtsp_so_exp_funcs_t * (*nkn_vfs_init)(struct vfs_funcs_t * func) = NULL;
        char * error;

	if(!s) return;
	ext = get_next_token(s, &len);
	if(!ext) { return; }
	s += len;
	lib = get_next_token(s, &len);
	if(!lib) { return; }

        p = (struct rtsp_so_libs *)nkn_malloc_type(sizeof(struct rtsp_so_libs), mod_rtsp_vs_cfg_malloc);
        if(!p) return;
	memset((char *)p, 0, sizeof(struct rtsp_so_libs));

	pthread_mutex_lock(&cfg_so_mutex);

        p->ext = nkn_strdup_type(ext, mod_rtsp_vs_cfg_strdup);
        p->libname = nkn_strdup_type(lib, mod_rtsp_vs_cfg_strdup);
	p->enable = 1;
	pthread_mutex_init( &p->mutex, NULL );

	if (strcmp(ext, "default")==0) {
		struct rtsp_so_libs * p2;
		/* Append to the end.
		 * or insert in front of the first "default"
		 */
		if(g_rtsp_so == NULL) {
			g_rtsp_so = p;
			goto done;
		}

		if(strcmp(g_rtsp_so->ext, "default")==0) {
			g_rtsp_so->enable = 0;
			p->next = g_rtsp_so;
			g_rtsp_so = p; // insert before other default
			goto done;
		}

		p2 = g_rtsp_so;
		while(p2->next) { 
			if(strcmp(p2->next->ext, "default")==0) {
				p2->next->enable = 0; // disable it
				// insert
				p->next = p2->next;
				p2->next = p;
				goto done;
			}
			p2= p2->next; 
		}

		// append in the end
		p2->next = p;
		goto done;
	}

	// normal case
	p->next = g_rtsp_so;
	g_rtsp_so = p;

done:
	/*
	 * Pre-load shared library at the configuration time.
	 */
	snprintf(filename, sizeof(filename), "/lib/nkn/%s", p->libname);
	p->handle = dlopen(filename, RTLD_NOW|RTLD_LOCAL/*RTLD_LAZY*/);
	if (!p->handle) {
	    DBG_LOG(MSG, MOD_RTSP, "Cannot load so file %s - error %s", filename, dlerror());
	    pthread_mutex_unlock(&cfg_so_mutex);
	    return;
	}
	nkn_vfs_init = dlsym(p->handle, "nkn_vfs_init");
	if ((error = dlerror()) != NULL)  {
		dlclose(p->handle);
		p->handle = NULL;
		DBG_LOG(MSG, MOD_RTSP, "dlopen %s", error);
		pthread_mutex_unlock(&cfg_so_mutex);
		return;
	}
	p->pfunc = (*nkn_vfs_init)(&vfs_funs);

	pthread_mutex_unlock(&cfg_so_mutex);
}

void rtsp_no_cfg_so_callback(char * ext)
{
        struct rtsp_so_libs * p;

	if(!ext) return;

	pthread_mutex_lock(&cfg_so_mutex);
        p = g_rtsp_so;
        while(p) {
		if(p->enable && strcmp(p->ext, ext) == 0) {
			p->enable = 0;
			pthread_mutex_unlock(&cfg_so_mutex);
			return;
		}
		p = p->next;
	}
	pthread_mutex_unlock(&cfg_so_mutex);
	return;
}

struct rtsp_so_libs * rtsp_get_so_libname(char * ext)
{
        struct rtsp_so_libs * p;

	if(!ext) return NULL;

	pthread_mutex_lock(&cfg_so_mutex);
        p = g_rtsp_so;
        while(p) {
		// extension == NULL: means this is a default shared library.
		if( p->enable && 
		    ((strcmp(p->ext, "default")==0) || (strcmp(ext, p->ext) == 0)) &&
		    (p->handle != NULL) ) {
			// Shared library should have already been loaded.
			pthread_mutex_unlock(&cfg_so_mutex);
			return p;
		}
                p = p->next;
        }

	pthread_mutex_unlock(&cfg_so_mutex);
        return NULL;
}

