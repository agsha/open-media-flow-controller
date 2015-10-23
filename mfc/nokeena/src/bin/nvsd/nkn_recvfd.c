#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <dirent.h>
#include <ctype.h>
#include <glib.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/un.h>
#include <sys/ipc.h>
#include <sys/shm.h> 
#include <sys/epoll.h> 

#include "nvsd_mgmt.h"
#include "nkn_defs.h"
#include "nkn_sched_api.h"
#include "nkn_debug.h"
#include "server.h"
#include "network.h"
#include "nkn_mgmt_agent.h"
#include "nkn_cfg_params.h"
#include "nkn_log_strings.h"
#include "nkn_trace.h"
#include "nkn_am_api.h"
#include "ssp.h"
#include "nkn_namespace.h"
#include "nkn_vpemgr_defs.h"
#include "nkn_cl.h"
#include "nkn_mediamgr_api.h"
#include "nkn_rtsched_api.h"
#include "network_hres_timer.h"

#define MAX_BATCH_FD 64000

NKNCNT_DEF(l4_proxy_fd, AO_t, "", "Number of connections through acceptor")
static int domain_fd;
static int active_intf;
pthread_t acceptor_thread;
int glob_nvsd_health_shmid = -1 ;
nvsd_health_td *nkn_glob_nvsd_health = NULL;


extern void init_conn(int svrfd, int sockfd, nkn_interface_t * pns, 
                      struct sockaddr_in * addr, int thr_num);
void *acceptor(void *arg);

struct fd_struct
{
        int listen_fd;
        int32_t   accepted_thr_num;
        struct sockaddr_in client_addr;
	struct sockaddr_in dest_addr;

};

/*============================================================*/

static void l4proxy_epollin(int clifd, int srvfd, int intf_idx, void* cli_addr, int32_t thr_num);
static void l4proxy_epollin(int clifd, int srvfd, int intf_idx, void* cli_addr, int32_t thr_num)
{
        nkn_interface_t * pns;
        struct sockaddr_in addr;

        DBG_LOG(MSG, MOD_HTTP, "Enter. clifd:%d, srvfd:%d, intf_idx:%d, thr:%d",
                        clifd, srvfd, intf_idx, thr_num);

        memcpy(&addr, (struct sockaddr_in*)cli_addr, sizeof(struct sockaddr_in));
        pns = (nkn_interface_t *)&interface[intf_idx];
        nkn_mark_fd(clifd, NETWORK_FD);

        // max_allowed_sessions is calculated based on ABR

        if (AO_load(&pns->tot_sessions) > pns->max_allowed_sessions) {
                DBG_LOG(MSG, MOD_HTTP, "intf[%s, 0x%p]Total sessions(%ld) exceed max connections:%ld",
                             interface[intf_idx].if_name, &interface[intf_idx],
                             pns->tot_sessions, pns->max_allowed_sessions) ;
                nkn_close_fd(clifd, NETWORK_FD);
                return;
        }

        // valid socket
        if(NM_set_socket_nonblock(clifd) == FALSE) {
                DBG_LOG(MSG, MOD_HTTP,
                        "Failed to set socket %d to be nonblocking socket.",
                        clifd);
                nkn_close_fd(clifd, NETWORK_FD);
                return;
        }

        // Bind this socket into this accepted interface
        if(bind_socket_with_interface) {
                NM_bind_socket_to_interface(clifd, pns->if_name);
        }

        init_conn(srvfd, clifd, pns, &addr, thr_num);
        return;
}


static int open_sock_dgram(const char *path)
{
	struct sockaddr_un unix_socket_name;
	domain_fd = -1;
	memset(&unix_socket_name,0,sizeof(struct sockaddr_un));
  	if (unlink(path) < 0)
	{
    		if (errno != ENOENT)
		{
                	DBG_LOG(SEVERE, MOD_NETWORK,
                        	"Failed to unlink %s",path);
      			return -1;
		}
    	}

  	unix_socket_name.sun_family = AF_UNIX;
  	strcpy(unix_socket_name.sun_path, path);
  	domain_fd = socket(PF_UNIX, SOCK_DGRAM, 0);
  	if (domain_fd == -1)
	{
		DBG_LOG(SEVERE, MOD_NETWORK,
			"Unable to create socket in acceptor");
		return -1;
	}
  	if (bind(domain_fd, (struct sockaddr *)&unix_socket_name, sizeof(unix_socket_name)))
	{
		DBG_LOG(SEVERE, MOD_NETWORK,
			"Unable to bind socket in acceptor");
    		close(domain_fd);
    		return -1;
  	}
	return 0;
}

static int receive_fd(struct fd_struct* ptr)
{
	struct msghdr msg;
  	struct iovec iov;
  	int rv;
  	int connfd = -1;
  	char ccmsg[CMSG_SPACE(sizeof(connfd))];
  	struct cmsghdr *cmsg;

  	iov.iov_base = ptr;
  	iov.iov_len = sizeof(struct fd_struct);

  	msg.msg_name = 0;
  	msg.msg_namelen = 0;
  	msg.msg_iov = &iov;
  	msg.msg_iovlen = 1;
  	msg.msg_control = ccmsg;
  	msg.msg_controllen = sizeof(ccmsg); 

  	rv = recvmsg(domain_fd, &msg, 0);
  	if (rv == -1)
	{
		DBG_LOG(MSG, MOD_NETWORK,
			"Failure in recvmsg in acceptor");
    		return -1;
  	}

  	cmsg = CMSG_FIRSTHDR(&msg);
  	if (!cmsg->cmsg_type == SCM_RIGHTS)
	{
		DBG_LOG(MSG, MOD_NETWORK,
			"got control message of unknown type in acceptor:%d",
			cmsg->cmsg_type);
    		return -1;
  	}

        DBG_LOG(MSG, MOD_NETWORK, "Succefully recved fd: %d\n", *(int*)CMSG_DATA(cmsg)) ;
  	return *(int*)CMSG_DATA(cmsg);
}


/* thread function to constantly read unix domain socket
 * for new fd*/
void *acceptor(void *arg)
{
	UNUSED_ARGUMENT(arg);
	int count=0;
	int connfd = 0, epollfd, timeout;
	struct epoll_event ev, events[MAX_BATCH_FD];
	struct fd_struct local_fd_struct;

	epollfd = epoll_create(1); //single unix domain fd
        if (epollfd < 0) {
                DBG_LOG(SEVERE, MOD_NETWORK, "epoll_create() error:%s",strerror(errno));
                assert(0);
        }
	ev.events = 0;
	ev.events |= EPOLLIN;
	ev.data.fd = domain_fd;	
        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, domain_fd, &ev) < 0) {
                DBG_LOG(SEVERE, MOD_ADNS, "%d fd has trouble when adding to epoll:%s\n",
                        domain_fd, strerror(errno));
                assert(0);
        }
	prctl(PR_SET_NAME, "nvsd-acceptor", 0, 0, 0);
        while(1)
        {
		timeout = 3000;
		count = epoll_wait(epollfd, events, MAX_BATCH_FD, timeout);
		if (count < 0) {
			DBG_LOG(SEVERE, MOD_NETWORK, "epoll_wait failed:%s", strerror(errno));
			continue;
		}
		if (count == 0) {
			//Nothing came for 3secs
			continue;
		}
		connfd = receive_fd(&local_fd_struct);
		if(connfd==-1)
		{
			DBG_LOG(SEVERE, MOD_NETWORK, 
				"Failure in receive_fd in acceptor");
			continue;
		}
		AO_fetch_and_add1(&glob_l4_proxy_fd);
		l4proxy_epollin(connfd, local_fd_struct.listen_fd, active_intf, 
				(void*)&local_fd_struct.client_addr,
                                local_fd_struct.accepted_thr_num);
	}
}

int acceptor_init(const char *path);
int acceptor_init(const char *path)
{
	active_intf = -1;
	open_sock_dgram(path);
        if(domain_fd==-1)
        {
		DBG_LOG(SEVERE, MOD_NETWORK, 
				"init failed in acceptor");
                return -1;
        }
        int i;
        for(i=0; i<glob_tot_interface; i++)
        {
                if( strcmp(interface[i].if_name, "eth0") == 0 )
		{
			active_intf=i;
                        break;
		}
        }

	if (pthread_create(&acceptor_thread, NULL, acceptor, NULL) != 0)
	{
                DBG_LOG(SEVERE, MOD_NETWORK,
                        "Failed to create acceptor thread");
                DBG_ERR(SEVERE,"Failed to create acceptor thread");
                return -1;
        }
        DBG_LOG(SEVERE, MOD_NETWORK, "Successfully created acceptor thread: path:%s", path);
	return 0;
}
	
			

/*
 *  Initialize the shared memory which holds the
 *  nvsd health info, which will be used by proxyd.
 */
int
nkn_init_health_info()
{

        glob_nvsd_health_shmid = shmget(PXY_HEALTH_CHECK_KEY,
                       sizeof(nvsd_health_td),
                       IPC_CREAT) ; /* No flags, only read if present */
        if (glob_nvsd_health_shmid == -1) { /*Error case*/
              DBG_LOG(MSG, MOD_NETWORK, "Failed to get shared mem id. Error: %s\n",
                                       strerror(errno));
              return -1;
        }


        /* Attach to the shared memory where the health info will be written */
        nkn_glob_nvsd_health = shmat(glob_nvsd_health_shmid, NULL, 0);
        if (nkn_glob_nvsd_health ==  (void*)-1) { /*Error case*/
              DBG_LOG(MSG, MOD_TUNNEL, "Failed to attach to shared memory"
                                       " for writing nvsd's health info. Error: %s\n",
                                       strerror(errno));
              return -1;
        } 

        /* Good health now...*/
        nkn_glob_nvsd_health->good = TRUE;
        return 0;
}

