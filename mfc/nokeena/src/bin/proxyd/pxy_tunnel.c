#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <stdint.h>
#include <errno.h>
#include <netdb.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <pthread.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include "pxy_defs.h"
#include "nkn_debug.h"
#include "nkn_assert.h"
#include "pxy_server.h"
#include "pxy_interface.h"
#include "pxy_network.h"

NKNCNT_DEF(pxy_tot_size_from_client,     AO_t, "", "size from client")
NKNCNT_DEF(pxy_tot_size_to_origin_fd,    AO_t, "", "size sent from client fd to origin fd")
NKNCNT_DEF(pxy_tot_size_to_origin_svr,   AO_t, "", "size sent to origin server")
NKNCNT_DEF(pxy_tot_size_from_origin_svr, AO_t, "", "size got from origin server")
NKNCNT_DEF(pxy_tot_size_to_client_fd,    AO_t, "", "size forwarded to client fd from origin fd")
NKNCNT_DEF(pxy_tot_size_from_l4proxy,    AO_t, "", "size delivered to client")

NKNCNT_DEF(pxy_cur_open_sockets, AO_t, "", "cur open socket")

static pthread_mutex_t pxy_tr_socket_mutex = PTHREAD_MUTEX_INITIALIZER;
extern network_mgr_t * gnm;
extern int http_idle_timeout;
extern int use_client_ip;
void close_conn(int fd);


/*
 *
 */
static int forward_data_to_peer(con_t * con)
{
	con_t * peer_con;
	int ret, len;
	char * p;

	peer_con = gnm[con->peer_fd].private_data;
	pxy_NM_set_socket_active(&gnm[con->fd]);

	len = con->cb_totlen - con->cb_offsetlen;
	while(len) {

                DBG_LOG(MSG, MOD_PROXYD, "offset=%d http.cb_totlen=%d\n",
                             con->cb_offsetlen, con->cb_totlen);

		p = &con->cb_buf[con->cb_offsetlen];
		ret = send(con->peer_fd, p, len, 0);
		if( ret==-1 ) {
			if(errno == EAGAIN) {
				pxy_NM_add_event_epollout(con->peer_fd);
				pxy_NM_del_event_epoll(con->fd);
				return TRUE;
			}
			return FALSE;
		} 
		con->cb_offsetlen += ret;
		len -= ret;
                if (CHECK_CON_FLAG(con, CONF_CLIENT_SIDE)) {
		    glob_pxy_tot_size_from_l4proxy += ret;
                } else {
		    glob_pxy_tot_size_to_origin_svr += ret;
                }
	}

	pxy_NM_add_event_epollin(con->fd);
	con->cb_offsetlen = 0;
	con->cb_totlen = 0;
	return TRUE;
}


/* ***************************************************
 * The following functions handles client side socket.
 * *************************************************** */

static int pxy_http_epollin(int fd, void * private_data)
{
	con_t * con = (con_t *)private_data;
	int rlen;
	int ret;

	rlen = MAX_CB_BUF - con->cb_totlen;
	ret = recv(fd, &con->cb_buf[con->cb_totlen], rlen, 0);
	if(ret <= 0) {
                DBG_LOG(MSG, MOD_PROXYD, "[closing fd:%d]recv returned %d." , fd, ret) ;
                close_conn(fd);
		return TRUE;
	}
	con->cb_totlen += ret;

        if (CHECK_CON_FLAG(con, CONF_CLIENT_SIDE)) {
            glob_pxy_tot_size_from_client += ret;
        } else {
            glob_pxy_tot_size_from_origin_svr += ret;
        }

        DBG_LOG(MSG, MOD_PROXYD, 
                     "[fd:%d]recv returned %d., fwding data to peer" ,
                     fd, ret) ;

	forward_data_to_peer(con);
	return TRUE;
}

static int pxy_http_epollout(int fd, void * private_data)
{
	con_t * con = (con_t *)private_data;
	int ret;
	int retlen;

	UNUSED_ARGUMENT(fd);
	if (CHECK_CON_FLAG(con, CONF_SYN_SENT)) {

		UNSET_CON_FLAG(con, CONF_SYN_SENT);
		retlen=sizeof(ret);
		if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *)&ret, 
		       (socklen_t *)&retlen)) {
			DBG_LOG(MSG, MOD_PROXYD, "getsockopt(fd=%d) failed, errno=%d", fd, errno);
			close_conn(fd);
			return TRUE;
		}
		if(ret) {
			DBG_LOG(MSG, MOD_PROXYD, "connect(fd=%d) failed, ret=%d", fd, ret);
			close_conn(fd);
			return TRUE;
		}

               /*
                * When one of the fd add to epollin event fail, close fd and peer_fd.
                */
		if ((pxy_NM_add_event_epollin(fd)==FALSE)
		   || (pxy_NM_add_event_epollin(con->peer_fd)==FALSE)) {
			close_conn(fd);
		}
	}
	else {
		/* Resent data */
		forward_data_to_peer(con);
	}

	return TRUE;
}

static int pxy_http_epollerr(int fd, void * private_data)
{
	UNUSED_ARGUMENT(private_data);
	close_conn(fd);
	return TRUE;
}

static int pxy_http_epollhup(int fd, void * private_data)
{
	return pxy_http_epollerr(fd, private_data);
}

static int pxy_http_timeout(int fd, void * private_data, double timeout)
{
	return pxy_http_epollerr(fd, private_data);
}


/*
 * This function cleans up HTTP structure and con structure
 * It should be called only once for each socket at the time of socket close time
 */
void close_conn(int fd)
{
	int peer_fd;
	con_t * con = NULL;

	con = (con_t *)gnm[fd].private_data;
        if (!con) { 
	    DBG_LOG(MSG, MOD_PROXYD, "gnm[fd=%d].private_data NULL. returning", fd);
            return ; 
        }
        if (!CHECK_CON_FLAG(con, CONF_CLIENT_SIDE)) {
	    DBG_LOG(MSG, MOD_PROXYD, "gnm[fd=%d]. Svr side socket.", fd);
            return ; 
        }

	peer_fd = con->peer_fd;

	DBG_LOG(MSG, MOD_PROXYD, "fd=%d peer_fd=%d", fd, peer_fd);

	pxy_NM_close_socket(fd);
	pxy_NM_close_socket(peer_fd);

	AO_fetch_and_sub1(&glob_pxy_cur_open_sockets);
}


/* ***************************************************
 * The following functions handles server side socket.
 * *************************************************** */

static con_t * create_new_svr_con(int cli_fd)
{
	int                  svr_fd;
	con_t              * svr_con, * cli_con;
	struct sockaddr_in   srv;
	int                  ret;


	cli_con = gnm[cli_fd].private_data;

	svr_con = (con_t *)calloc(1, sizeof(con_t));
	if(!svr_con) {
		return NULL;
	}
	svr_con->magic     = CON_MAGIC_USED;
        svr_con->fd        = -1;
	svr_con->pns       = cli_con->pns;
	svr_con->src_ip    = cli_con->src_ip;
	svr_con->src_port  = cli_con->src_port;
	svr_con->dest_ip   = cli_con->dest_ip;
	svr_con->dest_port = cli_con->dest_port;
	svr_con->tproxy_ip = svr_con->src_ip;//This should be taken from CLI

        if( (svr_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                DBG_LOG(WARNING, MOD_PROXYD, "FP Failed to create fd, errno=%d.", errno);
                free(svr_con) ;
		return NULL;
        }
	svr_con->fd = svr_fd;
	svr_con->peer_fd = cli_fd;

	DBG_LOG(MSG, MOD_PROXYD, "svr_con=%p[fd:%d] cli_con=%p[fd=%d]", 
                               svr_con, svr_con->fd,
                               cli_con, cli_con->fd);

	if(pxy_NM_register_socket(svr_con->fd,
                        NMF_TYPE_CON_T,
                        (void *)svr_con,
                        pxy_http_epollin,
                        pxy_http_epollout,
                        pxy_http_epollerr,
                        pxy_http_epollhup,
                        pxy_http_timeout,
                        http_idle_timeout) == FALSE)
	{
		close(svr_con->fd);
		free(svr_con);
		return NULL;
	}

	/* Now time to make a socket connection. */
        bzero(&srv, sizeof(srv));
        srv.sin_family = AF_INET;
	srv.sin_addr.s_addr = svr_con->dest_ip;
        srv.sin_port = svr_con->dest_port;

	// Set to non-blocking socket. so it is non-blocking connect.
	pxy_NM_set_socket_nonblock(svr_fd);
	pthread_mutex_lock(&pxy_tr_socket_mutex);

	if(use_client_ip) {
		ret = pxy_set_transparent_mode_tosocket(svr_fd, svr_con);
                if (ret == FALSE) {
                        pthread_mutex_unlock(&pxy_tr_socket_mutex);
                        // NOTE: unregister_NM_socket(fd, TRUE) deletes
                        // the fd from epoll loop.
                        pxy_NM_unregister_socket(svr_fd);
			return NULL;
                }
	}

	ret = connect(svr_fd, (struct sockaddr *)&srv, sizeof(struct sockaddr));
	pthread_mutex_unlock(&pxy_tr_socket_mutex);
	if(ret < 0)
	{
		if(errno == EINPROGRESS) {
			DBG_LOG(MSG, MOD_PROXYD, "connect(0x%x) fd=%d in progress", 
						svr_con->dest_ip, svr_fd);
			if (pxy_NM_add_event_epollout(svr_fd)==FALSE) {
				// NOTE: unregister_NM_socket(fd, TRUE) deletes
				// the fd from epoll loop.
				pxy_NM_unregister_socket(svr_con->fd);
				return NULL;
			}
			SET_CON_FLAG(svr_con, CONF_SYN_SENT);
			return svr_con;
		}
		DBG_LOG(MSG, MOD_PROXYD, "connect(0x%x) fd=%d failed, errno=%d", 
					svr_con->dest_ip, svr_fd, errno);
		pxy_NM_unregister_socket(svr_con->fd);
		return NULL;
	}

	/*
	 * Socket has been established successfully by now.
	 */
	return svr_con;
}


void pxy_proxy_this_fd(int cli_fd, void * private_data,
                int svr_fd,
                struct sockaddr_in * p_s_addr, socklen_t s_addrlen,
                struct sockaddr_in * p_d_addr, socklen_t d_addrlen);
void pxy_proxy_this_fd(int cli_fd, void * private_data,
                int svr_fd,
                struct sockaddr_in * p_s_addr, socklen_t s_addrlen,
                struct sockaddr_in * p_d_addr, socklen_t d_addrlen)
{
 	pxy_nkn_interface_t * pns = (pxy_nkn_interface_t *)private_data;
        con_t * cli_con, * svr_con;
	int j;


	/*
	 * Set up this client socket.
	 */
	if(pxy_NM_set_socket_nonblock(cli_fd) == FALSE) {
		DBG_LOG(MSG, MOD_PROXYD, 
			"Failed to set socket %d to be nonblocking socket.",
			cli_fd);
		close(cli_fd);
		return;
	}

	// Bind this socket into this accepted interface 
	pxy_NM_bind_socket_to_interface(cli_fd, pns->if_name);

	cli_con = (con_t *)calloc(1, sizeof(con_t));
	if(!cli_con) {
		close(cli_fd);
		return;
	}
	cli_con->magic = CON_MAGIC_USED;
        cli_con->fd = cli_fd;
	cli_con->pns = pns;
	cli_con->src_ip = p_s_addr->sin_addr.s_addr;
	cli_con->src_port = p_s_addr->sin_port;
	cli_con->dest_ip = p_d_addr->sin_addr.s_addr;
	cli_con->dest_port = p_d_addr->sin_port;
	SET_CON_FLAG(cli_con, CONF_CLIENT_SIDE);

	DBG_LOG(MSG, MOD_PROXYD, "con=%p fd=%d", cli_con, cli_fd);

	if(pxy_NM_register_socket(cli_fd,
                        NMF_TYPE_CON_T,
                        (void *)cli_con,
                        pxy_http_epollin,
                        pxy_http_epollout,
                        pxy_http_epollerr,
                        pxy_http_epollhup,
                        pxy_http_timeout,
                        http_idle_timeout) == FALSE)
	{
		close(cli_fd);
		free(cli_con);
		return;
	}

	/*
	 * Set up peer socket.
	 */
	svr_con = create_new_svr_con(cli_fd);
	if(svr_con == NULL) {
		pxy_NM_unregister_socket(cli_con->fd);
		return;
	}
	DBG_LOG(MSG, MOD_PROXYD, "Tunnel %d <--> %d established",
			cli_con->fd, svr_con->fd);


	/*
	 * Setup the relation ship
	 */
	cli_con->peer_fd = svr_con->fd;

	AO_fetch_and_add1(&glob_pxy_cur_open_sockets);

	return;
}

