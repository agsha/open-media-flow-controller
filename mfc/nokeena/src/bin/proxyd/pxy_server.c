#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>
#include <time.h>
#include <limits.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <linux/netfilter_ipv4.h>

#include "pxy_defs.h"
#include "nkn_debug.h"
#include "nkn_stat.h"
#include "nkn_assert.h"
#include "nkn_lockstat.h"
#include "pxy_server.h"
#include "pxy_interface.h"
#include "pxy_network.h"

#ifndef IP_TRANSPARENT
#define IP_TRANSPARENT 19
#endif


NKNCNT_DEF(pxy_tot_blocked_sockets, AO_t, "", "Total blocked socket")
NKNCNT_DEF(pxy_tot_cacheable_sockets, AO_t, "", "Total cacheable socket")
NKNCNT_DEF(pxy_tot_tunnel_sockets, AO_t, "", "Total proxy socket")

int           http_idle_timeout = 60;
int           http_listen_intfcnt = 0;
char          http_listen_intfname[MAX_NKN_INTERFACE][16];
static struct sockaddr_un unix_socket_name;
extern network_mgr_t * gnm;
int           nkn_http_serverport[MAX_PORTLIST] ;

extern void pxy_proxy_this_fd(int cli_fd, void * private_data,
		int svr_fd, 
		struct sockaddr_in * p_s_addr, socklen_t s_addrlen,
		struct sockaddr_in * p_d_addr, socklen_t d_addrlen);


/* **************************************************
 * functions for transferring fd from proxyd to nvsd.
 * ************************************************* */
static int unix_socket_fd = -1;

static int 
pxy_open_unix_fd (const char * path)
{
  	unix_socket_name.sun_family = AF_UNIX;
  	strcpy(unix_socket_name.sun_path, path);
  	unix_socket_fd = socket(PF_UNIX, SOCK_DGRAM, 0);
  	return (unix_socket_fd == -1) ? 0 : 1;
}

static int 
pxy_transfer_fd_to_nvsd (int cli_fd, void * private_data,
		int svr_fd, 
		struct sockaddr_in * p_s_addr, socklen_t s_addrlen,
		struct sockaddr_in * p_d_addr, socklen_t d_addrlen)
{
	struct fd_struct local_fd_struct;
	struct msghdr msg;
  	struct cmsghdr *cmsg;
  	struct iovec vec; //in-band-data 
	int fd; 
	struct fd_struct * ptr;
  	char ccmsg[CMSG_SPACE(sizeof(fd))] = {0,};
        int  sendlen = 0;


       /*
        * Before transfering the fd to nvsd, check whether nvsd is 
        * in fine condition.
        */

	local_fd_struct.listen_fd = svr_fd;
	memcpy(&local_fd_struct.dst_addr, p_d_addr, d_addrlen);
	memcpy(&local_fd_struct.client_addr, p_s_addr, s_addrlen);
        local_fd_struct.accepted_thr_num = gnm[svr_fd].accepted_thr_num ;

        DBG_LOG(MSG, MOD_PROXYD, "got a connection on listenfd=%d and client fd=%d\n", svr_fd, cli_fd);

	fd = cli_fd;
	ptr = &local_fd_struct;
  
  	msg.msg_name = (struct sockaddr*)&unix_socket_name;
  	msg.msg_namelen = sizeof(unix_socket_name);

  	vec.iov_base = ptr;
  	vec.iov_len = sizeof(struct fd_struct);
  	msg.msg_iov = &vec;
  	msg.msg_iovlen = 1;

  	msg.msg_control = ccmsg;
       /* Since we are sending only one fd at a time now, msg_controllen is same as
        * length of single ancilliary object
        */
        msg.msg_controllen = CMSG_LEN(sizeof(fd));

  	cmsg = CMSG_FIRSTHDR(&msg);
  	cmsg->cmsg_level = SOL_SOCKET;//out-of-band data
  	cmsg->cmsg_type = SCM_RIGHTS;
  	cmsg->cmsg_len = CMSG_LEN(sizeof(fd));
  	*(int*)CMSG_DATA(cmsg) = fd;

  	msg.msg_flags = 0;

  	if ((sendlen = sendmsg(unix_socket_fd, &msg, 0)) != -1) {
		close(fd);
                DBG_LOG(MSG, MOD_PROXYD, 
                        "Fwding fd '%d' to nvsd success. send length: %d\n",
                        fd, sendlen) ;
		return TRUE;
	}

        DBG_LOG(MSG, MOD_PROXYD, "Fwding fd '%d' to nvsd failed. Error:%s\n",
                     fd, strerror(errno)) ;
  	return FALSE;
}

static void 
pxy_close_unix_fd (void)
{
  	if (unix_socket_fd != -1) {
		close(unix_socket_fd);
		unix_socket_fd = -1;
	}
}



/*
 * ===============> server socket <===============================
 * The following functions are designed for server socket.
 * we need to provde epollin/epollout/epollerr/epollhup functions
 */

static int 
pxy_httpsvr_epollin (int sockfd, void * private_data)
{
	int clifd;
	int cnt;
	struct sockaddr_in s_addr;
	socklen_t s_addrlen;
	struct sockaddr_in d_addr;
	socklen_t d_addrlen;
	l4proxy_action ipact;
	int ret;


	/* always returns TRUE for this case */
	for(cnt=0; ; cnt++) {

		s_addrlen=sizeof(struct sockaddr_in);
		clifd = accept(sockfd, (struct sockaddr *)&s_addr, &s_addrlen);
		if (clifd == -1) {
			return TRUE;
		}
		d_addrlen=sizeof(struct sockaddr_in);
		memset(&d_addr, 0, d_addrlen);

                if (getsockname(clifd, (struct sockaddr *) &d_addr, &d_addrlen) == -1) {
			printf("errno=%d\n", errno);
			close(clifd);
                        return TRUE;
                }

		ipact = get_l4proxy_action(d_addr.sin_addr.s_addr);
		DBG_LOG(MSG, MOD_PROXYD, 
                             "PROXY: dest ip=0x%x action=%d, accept returned client fd=%d", 
                             d_addr.sin_addr.s_addr, ipact, clifd);

		switch(ipact) {
		case L4PROXY_ACTION_REJECT:
			AO_fetch_and_add1( &glob_pxy_tot_blocked_sockets );
			close(clifd);
			break;

		case L4PROXY_ACTION_WHITE:
			AO_fetch_and_add1( &glob_pxy_tot_cacheable_sockets );
			ret = pxy_transfer_fd_to_nvsd(clifd, private_data, 
					sockfd, &s_addr, s_addrlen, &d_addr, d_addrlen);
			if (ret) {
				break;
			}

			// Others. Fall through the Tunnel code path.
		case L4PROXY_ACTION_TUNNEL:
		default:
			AO_fetch_and_add1( &glob_pxy_tot_tunnel_sockets );
			pxy_proxy_this_fd(clifd, private_data, 
					sockfd, &s_addr, s_addrlen, &d_addr, d_addrlen);
			break;
		}

	}
	return TRUE;
}

static int 
pxy_httpsvr_epollout (int sockfd, void * private_data)
{
        pxy_nkn_interface_t *intf = (pxy_nkn_interface_t *)private_data ;

        DBG_LOG(SEVERE, MOD_PROXYD, "sockid=%d, intf:%s, tot_sess:%lu, tot_packt:%u ",
                                  sockfd, intf->if_name, intf->tot_sessions, intf->tot_pkt);
        return TRUE;
}

static int 
pxy_httpsvr_epollerr (int sockfd, void * private_data)
{
        pxy_nkn_interface_t *intf = (pxy_nkn_interface_t *)private_data ;

        DBG_LOG(SEVERE, MOD_PROXYD, "sockid=%d, intf:%s, tot_sess:%lu, tot_packt:%u ",
                                  sockfd, intf->if_name, intf->tot_sessions, intf->tot_pkt);
        return TRUE;
}

static int 
pxy_httpsvr_epollhup (int sockfd, void * private_data)
{
	return pxy_httpsvr_epollerr(sockfd, private_data);
}


void pxy_httpsvr_init (void)
{
	struct sockaddr_in srv;
	int ret;
	int val;
	int i, j;
	int listenfd;
	int listen_all = 1;
        struct sockaddr_in dip;
        socklen_t dlen;



       /*
        * 
        */
	pxy_open_unix_fd(PXY_CLIENT_FD_FWD_PATH);

       /* Attach to the shared memory segment which provide nvsd's
        * health info
        */
        pxy_attach_nvsd_health_info() ;

       /* Check if any interface is enabled else listen on all
        * interface 
        */
        for (i = 0; i < http_listen_intfcnt; i++) {
            for(j = 0; j < glob_tot_interface; j++) {
                if(strcmp(interface[j].if_name, 
                          http_listen_intfname[i]) == 0) {
                        interface[j].enabled = 1;
                        listen_all = 0;
                }
            }
        }

        for(i=0; i<glob_tot_interface; i++) {
            if ( ( strcmp(interface[i].if_name, "lo") ==0 ) || /* proxyd do not listen on loopback */
                 (( listen_all == 0 ) && ( interface[i].enabled != 1 ))) { /* Skip the interfaces which are not enabled.*/
                 DBG_LOG(SEVERE, MOD_PROXYD, "skipping glob_intf[%d]:%s",i,  interface[i].if_name) ;
                 continue;
	    }
 
            for(j=0; j<MAX_PORTLIST; j++)
            {
                if(nkn_http_serverport[j] == 0) {
                    continue;
                }

                if( (listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
         		DBG_LOG(SEVERE, MOD_PROXYD, 
                            "PROXY: failed to create a socket errno=%d", errno);
         		continue;
                }
    
                bzero(&srv, sizeof(srv));
                srv.sin_family = AF_INET;
                srv.sin_addr.s_addr = interface[i].if_addr;
                srv.sin_port = htons(nkn_http_serverport[j]);
        
                val = 1;
                ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, 
                                 &val, sizeof(val));
                if (ret == -1) {
        		DBG_LOG(SEVERE, MOD_PROXYD, 
                            "PROXY: failed to set socket option errno=%d", errno);
        		close(listenfd);
        		continue;
                }
        
                val = 1;
                ret = setsockopt(listenfd, SOL_IP, IP_TRANSPARENT, &val, sizeof(val));
                if (ret == -1) {
                        DBG_LOG(SEVERE, MOD_PROXYD, 
                                "PROXY: failed to set socket option (IP_TRANSPARENT) errno=%d",
                                errno);
        		close(listenfd);
                    continue;
                }
        
                if( bind(listenfd, (struct sockaddr *) &srv, sizeof(srv)) < 0)
                {
        		DBG_LOG(SEVERE, MOD_PROXYD, 
        			"PROXY: failed to bind server socket to port %d (errno=%d)", 
        			nkn_http_serverport[j], errno);
        		close(listenfd);
        		continue;
                } 

               /* Could remove this debug stmt, after the development stage. */
       		DBG_LOG(SEVERE, MOD_PROXYD, "PROXY:[i:%d, j:%d]intf:%s, bind to server socket succ. "
                                          "tried port %d, got:%d. (errno=%d), listenfd:%d", 
                                          i, j,interface[i].if_name, ntohs(srv.sin_port), ntohs(dip.sin_port),  
                                          errno, listenfd);

                // We will use nonblocking accept listen socket
                pxy_NM_set_socket_nonblock(listenfd);
        
        	listen(listenfd, 10000);
        
        	interface[i].port[j] = nkn_http_serverport[j];
        	interface[i].listenfd[j] = listenfd;
        
        	if(pxy_NM_register_socket(listenfd, 
                                NMF_TYPE_INTERFACE,
        			&interface[i],
        			pxy_httpsvr_epollin,
        			pxy_httpsvr_epollout,
        			pxy_httpsvr_epollerr,
        			pxy_httpsvr_epollhup,
        			NULL,
        			0) == FALSE)
        	{
        	    // will this case ever happen at all?
        	    close(listenfd);
                    continue;
        	}
        	pxy_NM_add_event_epollin(listenfd);
        
        	gnm[listenfd].accepted_thr_num = gnm[listenfd].pthr->num;
           }
       }
}

