#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <time.h>
#include <limits.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "nkn_debug.h"
#include "nkn_errno.h"
#include "nkn_stat.h"
#include "ssl_network.h"
#include "ssl_server.h"
#include "nkn_lockstat.h"
#include "nkn_assert.h"

char *ssl_shm_mem = NULL;
uint32_t nkn_cfg_tot_socket_in_pool = 0;
int cp_enable =1;
int cp_idle_timeout = 300;

/* socket connection pool. */
#define MAX_SOCKET_POOL 10240
#define CPSP_MAGIC_DEAD 0xdead0003
struct cp_socket_pool {
	uint32_t magic;
	struct cp_socket_pool * next;

	ip_addr_t src_ipv4v6; // network order
	ip_addr_t ipv4v6; // network order
	uint16_t port; // network order
	char * domainname;
	int fd;
};
static struct cp_socket_pool * g_pcp_hashtable[MAX_SOCKET_POOL];
static pthread_mutex_t cp_sp_mutex = PTHREAD_MUTEX_INITIALIZER;

extern network_mgr_t * gnm;

NKNCNT_DEF(ssl_cp_tot_sockets_in_pool_ipv6 , AO_t, "", "Total ssl IPv6 sockets in conn pool")
NKNCNT_DEF(ssl_cp_tot_sockets_in_pool , AO_t, "", "total ssl ipv4 sockets in conn pool")
NKNCNT_DEF(ssl_cp_reused_same_time , AO_t, "", "ssl origin side number times of reused sockets")
NKNCNT_DEF(ssl_cp_tot_closed_sockets_ipv6 , AO_t, "", "total ipv6 conn pool closed sockets")
NKNCNT_DEF(ssl_cp_tot_closed_sockets , AO_t, "", "total ipv4 conn pool closed sockets")
NKNCNT_DEF(ssl_cp_tot_keep_alive_idleout_sockets , AO_t, "", "total keep alive idle timeout sockets")
NKNCNT_DEF(ssl_cp_activate_err_1 , AO_t, "", "activate error 1")
NKNCNT_DEF(ssl_cp_cannot_acquire_mutex , AO_t, "", "can not acquire mutex error counts")
NKNCNT_DEF(ssl_cp_socket_reused , AO_t, "", "reused connection pool sockets count")

void cp_init(void);
void cp_init()
{
	int i;
        int shmid = 0;
        char *shm = NULL;
        uint64_t shm_size = 0;
        key_t shmkey;
        int max_cnt_space = MAX_SHM_SSL_ID_SIZE;
        const char *producer = "/opt/nkn/sbin/nvsd";

	for (i=0; i<MAX_SOCKET_POOL; i++) {
		g_pcp_hashtable[i] = NULL;
	}

        if ((shmkey = ftok(producer, NKN_SHMKEY)) < 0) {
                DBG_LOG(MSG, MOD_SSL, "ftok failed. \
			shared memory between ssld and nvsd could not be established");
                return;
        }

        shm_size = max_cnt_space * sizeof(char);
        shmid = shmget(shmkey, shm_size, IPC_CREAT | 0666);
        if (shmid < 0) {
                DBG_LOG(MSG, MOD_SSL, "shmget error for ssld shatred memory");
                return ;
        }

        shm = shmat(shmid, NULL, 0);
        if (shm == (char *)-1) {
                DBG_LOG(MSG, MOD_SSL, "shmat error for ssld shared memory");
                return ;
        }
	ssl_shm_mem = shm;

	return;
}

/*
 * input: ip/port are in network order.
 */
static uint16_t cpsp_hashtable_func(ip_addr_t* src_ipv4v6,
		    ip_addr_t* ipv4v6, uint16_t port, const char *domianname);
static uint16_t cpsp_hashtable_func(ip_addr_t* src_ipv4v6,
		    ip_addr_t* ipv4v6, uint16_t port, const char *domainname)
{
	// todo ipv6. This needs to be heavily optimized.
	uint16_t ht_index = 0;
	unsigned int lower1_ipv6 = 0;
	unsigned int lower2_ipv6 = 0;
	unsigned int higher1_ipv6 = 0;
	unsigned int higher2_ipv6 = 0;

	if (src_ipv4v6->family == AF_INET6) {
                memcpy(&higher1_ipv6, (unsigned char*)&IPv6((*src_ipv4v6)), sizeof(unsigned int));
                memcpy(&higher2_ipv6, (unsigned char*)&IPv6((*src_ipv4v6))+4, sizeof(unsigned int));
                memcpy(&lower1_ipv6, (unsigned char*)&IPv6((*src_ipv4v6))+8, sizeof(unsigned int));
                memcpy(&lower2_ipv6, (unsigned char*)&IPv6((*src_ipv4v6))+12, sizeof(unsigned int));

                ht_index = (higher1_ipv6 & 0xFFFF0000) >> 16;
                ht_index += (higher1_ipv6 & 0xFFFF);

                ht_index += (higher2_ipv6 & 0xFFFF0000) >> 16;
                ht_index += (higher2_ipv6 & 0xFFFF);

                ht_index += (lower1_ipv6 & 0xFFFF0000) >> 16;
                ht_index += (lower1_ipv6 & 0xFFFF);

                ht_index += (lower2_ipv6 & 0xFFFF0000) >> 16;
                ht_index += (lower2_ipv6 & 0xFFFF);

		ht_index = (IPv4((*src_ipv4v6)) & 0xFFFF0000) >> 16;
		ht_index += IPv4((*src_ipv4v6)) & 0xFFFF;
	} else {
                ht_index = (IPv4((*src_ipv4v6)) & 0xFFFF0000) >> 16;
                ht_index += IPv4((*src_ipv4v6)) & 0xFFFF;
	}

	if (ipv4v6->family == AF_INET6) {
                memcpy(&higher1_ipv6, (unsigned char*)&IPv6((*ipv4v6)), sizeof(unsigned int));
                memcpy(&higher2_ipv6, (unsigned char*)&IPv6((*ipv4v6))+4, sizeof(unsigned int));
                memcpy(&lower1_ipv6, (unsigned char*)&IPv6((*ipv4v6))+8, sizeof(unsigned int));
                memcpy(&lower2_ipv6, (unsigned char*)&IPv6((*ipv4v6))+12, sizeof(unsigned int));

                ht_index += (higher1_ipv6 & 0xFFFF0000) >> 16;
                ht_index += (higher1_ipv6 & 0xFFFF);

                ht_index += (higher2_ipv6 & 0xFFFF0000) >> 16;
                ht_index += (higher2_ipv6 & 0xFFFF);

                ht_index += (lower1_ipv6 & 0xFFFF0000) >> 16;
                ht_index += (lower1_ipv6 & 0xFFFF);

                ht_index += (lower2_ipv6 & 0xFFFF0000) >> 16;
                ht_index += (lower2_ipv6 & 0xFFFF);
	} else {
		ht_index += (IPv4((*ipv4v6)) & 0xFFFF0000) >> 16;
		ht_index += IPv4((*ipv4v6)) & 0xFFFF;
	}

	ht_index += port;

	/* TODO Use domain name to compute hash key */
	return ht_index % MAX_SOCKET_POOL;
}

/*
 * input: ip/port are in network order.
 */
static int cpsp_epoll_rm_fm_pool(int fd, void * private_data);
static int cpsp_epoll_rm_fm_pool(int fd, void * private_data)
{
	uint16_t ht_index;
	struct cp_socket_pool * psp;
	struct cp_socket_pool * del_psp;
	ssl_con_t *con = (ssl_con_t *)private_data;

	/*
	 * Try to find it from keep-alive connection pool first.
	 */
	pthread_mutex_lock(&cp_sp_mutex);
        ht_index = cpsp_hashtable_func(&(con->src_ipv4v6),
                                        &(con->dest_ipv4v6),
                                        con->dest_port, con->domainname);

	psp = g_pcp_hashtable[ht_index];
	if (!psp) {
		goto notfound;
	}

	if (psp->fd == fd) {
		g_pcp_hashtable[ht_index] = psp->next;
		if (CHECK_CON_FLAG(con, CPF_IS_IPV6)) {
			AO_fetch_and_sub1(&glob_ssl_cp_tot_sockets_in_pool_ipv6);
		} else {
			AO_fetch_and_sub1(&glob_ssl_cp_tot_sockets_in_pool);
		}
		free(psp);
		pthread_mutex_unlock(&cp_sp_mutex);
		goto done;
	}
	while (psp->next) {
		if (psp->next->fd == fd) {
			del_psp = psp->next;
			psp->next = del_psp->next;
			if (CHECK_CON_FLAG(con, CPF_IS_IPV6)) {
				AO_fetch_and_sub1(&glob_ssl_cp_tot_sockets_in_pool_ipv6);
			} else {
				AO_fetch_and_sub1(&glob_ssl_cp_tot_sockets_in_pool);
			}
			free(del_psp);
			pthread_mutex_unlock(&cp_sp_mutex);
			goto done;
		}
		psp = psp->next;
	}
	/* follow up  notfound */
notfound:
	/*
	 * BUG 3604
	 *
	 * This case could happen like this.
	 * 1. cp_open_socket_handler() called and hold cp_sp_mutex locker.
	 * 2. epoll event happens and calls cpsp_epoll_rm_fm_pool().
	 *    cpsp_epoll_rm_fm_pool() wait for cp_sp_mutex locker.
	 * 3. cp_open_socket_handler() deleted this psp from connection pool hash table,
	 *    released the cp_sp_mutex locker.
	 * 4. Then we cannot find this socket fd from connection pool hash table.
	 *
	 * How about this sockfd?
	 * It should be taken care of by cp_epollin/cp_epollout etc APIs.
	 */
	glob_ssl_cp_reused_same_time ++;
	pthread_mutex_unlock(&cp_sp_mutex);
	/* then follow down */
done:
	con->magic = CPSP_MAGIC_DEAD;
	if (CHECK_CON_FLAG(con, CPF_IS_IPV6)) {
		AO_fetch_and_add1(&glob_ssl_cp_tot_closed_sockets_ipv6);
		DBG_LOG(MSG, MOD_SSL, "socket %d in connection pool deleted, \
			cp_tot_socketsipv6=%ld", fd, glob_ssl_cp_tot_sockets_in_pool_ipv6);
	} else {
		AO_fetch_and_add1(&glob_ssl_cp_tot_closed_sockets);
		DBG_LOG(MSG, MOD_SSL, "socket %d in connection pool deleted, \
			cp_tot_sockets_in_pool=%ld", fd, glob_ssl_cp_tot_sockets_in_pool);
	}

	close_cli_conn(fd, 0);

	return TRUE;
}

static int cpsp_timeout(int fd, void * private_data, double timeout);
static int cpsp_timeout(int fd, void * private_data, double timeout)
{
	UNUSED_ARGUMENT(timeout);
	glob_ssl_cp_tot_keep_alive_idleout_sockets++;
	cpsp_epoll_rm_fm_pool(fd, private_data);
	return TRUE;
}

int cpsp_add_into_pool(int fd, ssl_con_t * con)
{
	struct cp_socket_pool * psp;
	uint16_t ht_index;
	char * cb_buf;
	int cb_max_buf_size;

	if (cp_enable == 0) {
		return FALSE;
	}
#if 0

	if (con->ssl_fid >= 0 && (*(ssl_shm_mem+con->ssl_fid) == '0' || *(ssl_shm_mem+con->ssl_fid) == CONN_POOL_SSL_MARKER)) {
	} else {
		assert(0);
	}

#endif
	if (con->ssl_fid < 0 || *(ssl_shm_mem+con->ssl_fid) != CONN_POOL_SSL_MARKER)
		return FALSE;
	/* Check the Configured maximum number of connections that will be
	 * opened to the origin server concurrently. Default is 256. If set to 0, then
	 * there is no limit (demand driven). Maximum allowed value is 2048.
	 */

	if (((AO_load(&glob_ssl_cp_tot_sockets_in_pool) +
		AO_load(&glob_ssl_cp_tot_sockets_in_pool_ipv6)) >= nkn_cfg_tot_socket_in_pool) &&
			(nkn_cfg_tot_socket_in_pool != 0 ))
		return FALSE;

	/*
	 * It is a keep-alive socket. We can reuse the connection.
	 */
	psp = (struct cp_socket_pool *) malloc(sizeof(struct cp_socket_pool));
	if (!psp) {
		return FALSE;
	}

	// Fill in psp structure
	psp->magic = 0;
	memcpy(&(psp->src_ipv4v6), &(con->src_ipv4v6), sizeof(ip_addr_t));
	memcpy(&(psp->ipv4v6), &(con->dest_ipv4v6), sizeof(ip_addr_t));
	psp->port = con->dest_port;
	psp->fd = fd;
	psp->domainname = con->domainname;


	/* When unregister_NM_socket() is followed by register_NM_socket(),
	 * we do not need to delete fd from epoll loop and then add back.
	 * so specify FALSE.  This saves two system calls.
	 */
	unregister_NM_socket(fd);

	/*
	 * Monitoring server closes the connection or timed out.
	 */
	if (register_NM_socket(fd,
		(void *)con,
		cpsp_epoll_rm_fm_pool,
		cpsp_epoll_rm_fm_pool,
		cpsp_epoll_rm_fm_pool,
		cpsp_epoll_rm_fm_pool,
		cpsp_timeout,
		cp_idle_timeout,
		gnm[fd].accepted_thr_num) == FALSE)
	{
		psp->magic = CPSP_MAGIC_DEAD;
		free(psp);
		return FALSE;
	}

	if (NM_add_event_epollin(fd)==FALSE) {
		psp->magic = CPSP_MAGIC_DEAD;
		free(psp);
		return FALSE;
	}

	pthread_mutex_lock(&cp_sp_mutex);
	ht_index = cpsp_hashtable_func(&(con->src_ipv4v6),
			&(con->dest_ipv4v6), con->dest_port, con->domainname);
	psp->next = g_pcp_hashtable[ht_index];
	g_pcp_hashtable[ht_index] = psp;
	if (CHECK_CON_FLAG(con, CPF_IS_IPV6)) {
		AO_fetch_and_add1(&glob_ssl_cp_tot_sockets_in_pool_ipv6);
	} else {
		AO_fetch_and_add1(&glob_ssl_cp_tot_sockets_in_pool);
	}

	assert(con->magic == CPMAGIC_ALIVE);
	con->magic = CPMAGIC_IN_POOL;
	pthread_mutex_unlock(&cp_sp_mutex);

	DBG_LOG(MSG, MOD_SSL, "add socket %d into connection pool", fd);

	return TRUE;
}


/**
 * register network handler into epoll server.
 * starting non-blocking socket connection if not established.
 * enter finite state machine to loop all callback events.
 *
 * caller should acquire the gnm[fd].mutex locker
 *
 * OUTPUT:
 *   return FALSE: error, mutex lock is freed
 *   return TRUE: success.
 */
int cp_activate_socket_handler(int fd, ssl_con_t * con)
{
	struct sockaddr_in srv;
	struct sockaddr_in6 srv_v6;
	int ret;
	network_mgr_t * pnm;

	pnm = &gnm[fd];

	/*
	 * It is a Keep-alive reused socket.
	 * We should not send connect again.
	 */
	if (CHECK_CON_FLAG(con, CPF_USE_KA_SOCKET)) {

		int thr_num =-1;
		if (gnm[fd].peer_fd > 0) {
			thr_num = gnm[gnm[fd].peer_fd].accepted_thr_num;
		} else {
			assert(0);
		}

		DBG_LOG(MSG, MOD_SSL, "reused socket fd=%d", fd);
		UNSET_CON_FLAG(con, CPF_USE_KA_SOCKET);
		/* When unregister_NM_socket() is followed by register_NM_socket(),
	 	* we do not need to delete fd from epoll loop and then add back,
		* so specify FALSE.  This saves two system calls.
	 	*/
		unregister_NM_socket(fd);

		if (register_NM_socket(fd,
			(void *)con,
			ssl_origin_epollin,
			ssl_origin_epollout,
			ssl_origin_epollerr,
			ssl_origin_epollhup,
			ssl_origin_timeout,
			ssl_idle_timeout, // set to 60 seconds timeout
			thr_num) == FALSE)
		{
			glob_ssl_cp_activate_err_1 ++;
			pthread_mutex_unlock(&gnm[fd].mutex);
			return FALSE;
		}

		NM_add_event_epollout(fd);
		pthread_mutex_unlock(&gnm[fd].mutex);
		return TRUE;
	}

	pthread_mutex_unlock(&gnm[fd].mutex);
	return TRUE;
}

/*
 * acquire mutex locker cp_sp_mutex.
 */
static struct cp_socket_pool *cpsp_pickup_from_pool(ip_addr_t* src_ipv4v6,
		    ip_addr_t* ipv4v6, uint16_t port, char *domainname, uint16_t ht_index)
{
	struct cp_socket_pool * psp, * del_psp;
	del_psp = NULL;
	int src_same = 0;
	int dst_same = 0;
	int domain_same = 0 ;

	psp = g_pcp_hashtable[ht_index];
	if (psp) {
		if (src_ipv4v6->family == AF_INET) {
	    		if (IPv4(psp->src_ipv4v6) == IPv4((*src_ipv4v6))) {
				src_same = 1;
			}
		} else {
			if (memcmp(&(psp->src_ipv4v6), src_ipv4v6, sizeof(ip_addr_t)) == 0) {
				src_same = 1;
			}
		}
		if (ipv4v6->family == AF_INET) {
                        if (IPv4(psp->ipv4v6) == IPv4((*ipv4v6))) {
                                dst_same = 1;
                        }
                } else {
                        if (memcmp(&(psp->ipv4v6), ipv4v6, sizeof(ip_addr_t)) == 0) {
                                dst_same = 1;
                        }
                }


		if (domainname != NULL &&
			psp->domainname != NULL &&
			    strcmp(domainname, psp->domainname)== 0) {
			domain_same = 1;
		}



		if (src_same && dst_same && domain_same && (psp->port == port)) {
			g_pcp_hashtable[ht_index] = psp->next;
			return psp;
		}

	    	while (psp->next) {
			src_same = 0;
			dst_same = 0;
			domain_same = 0;
	                if (src_ipv4v6->family == AF_INET) {
        	                if (IPv4(psp->next->src_ipv4v6) == IPv4((*src_ipv4v6))) {
                	                src_same = 1;
                        	}
               		} else {
                        	if (memcmp(&(psp->next->src_ipv4v6),
					    src_ipv4v6, sizeof(ip_addr_t)) == 0) {
                                	src_same = 1;
                        	}
                	}

                	if (ipv4v6->family == AF_INET) {
                        	if (IPv4(psp->next->ipv4v6) == IPv4((*ipv4v6))) {
                                	dst_same = 1;
                        	}
                	} else {
                        	if (memcmp(&(psp->next->ipv4v6),
					    ipv4v6, sizeof(ip_addr_t)) == 0 ) {
                                	dst_same = 1;
                        	}
                	}

			if (domainname != NULL &&
				psp->next->domainname != NULL &&
				    strcmp(domainname, psp->next->domainname)== 0) {
				domain_same = 1;
			}

			if (src_same && dst_same && domain_same && (psp->next->port == port)) {
				del_psp = psp->next;
				psp->next = del_psp->next;
				return del_psp;
			}
			psp = psp->next;
	    	}
	}

	return NULL;
}


ssl_con_t * cp_open_socket_handler(ssl_con_t *httpcon)
{
	ssl_con_t * sslcon;
	uint16_t ht_index;
	struct cp_socket_pool * psp, * psp2;
        struct network_mgr * pnm_cp;
	int cnt = 0;
	int fd = 0;

	/*
	 * Try to find it from keep-alive connection pool first.
	 */
	pthread_mutex_lock(&cp_sp_mutex);
	ht_index = cpsp_hashtable_func(&httpcon->remote_src_ipv4v6,
			&httpcon->dest_ipv4v6, httpcon->dest_port, httpcon->domainname);
	psp = cpsp_pickup_from_pool(&httpcon->remote_src_ipv4v6,
			&httpcon->dest_ipv4v6, httpcon->dest_port,
			httpcon->domainname, ht_index);
	if (psp == NULL) {
		pthread_mutex_unlock(&cp_sp_mutex);
		return create_new_sslsvr_con(httpcon->fd);
	}

return_con_from_pool:
	/*
	 * While we are deleting this psp from connection pool,
	 * epoll event could wake up and call cpsp_epoll_rm_fm_pool().
	 * In that case, the socket in connection pool could be invalid.
	 *
	 * Also cpsp_epoll_rm_fm_pool() could hold the gnm[fd].mutex
	 * and wait for cp_sp_mutex lock. Then we maybe deadlock.
	 */
        pnm_cp = &gnm[psp->fd];
        if (pthread_mutex_trylock(&pnm_cp->mutex) != 0) {
		/*
		 * Cannot get lock, try to find second socket in the pool
		 */
		glob_ssl_cp_cannot_acquire_mutex++;
		cnt++;
		psp2 = (cnt < 2) ? (cpsp_pickup_from_pool(&httpcon->remote_src_ipv4v6,
					&httpcon->dest_ipv4v6, httpcon->dest_port,
					httpcon->domainname, ht_index)) : NULL;

		/* recover back into pool */
		psp->next = g_pcp_hashtable[ht_index];
		g_pcp_hashtable[ht_index] = psp;

		if (psp2==NULL) {
			pthread_mutex_unlock(&cp_sp_mutex);
			return create_new_sslsvr_con(httpcon->fd);
		}
		// Try second socket
		psp = psp2;
		goto return_con_from_pool;
        }

        if (!NM_CHECK_FLAG(pnm_cp, NMF_IN_USE) ) {
		/*
		 * while we are waiting for lock,
		 * try to find second socket in the pool
		 */
		cnt++;
		psp2 = (cnt < 2) ? (cpsp_pickup_from_pool(&httpcon->remote_src_ipv4v6,
					&httpcon->dest_ipv4v6, httpcon->dest_port,
					httpcon->domainname, ht_index)) : NULL;

		/* delete this psp */
		AO_fetch_and_sub1(&glob_ssl_cp_tot_sockets_in_pool);
		psp->magic = CPSP_MAGIC_DEAD;
		free(psp);

		if (psp2==NULL) {
			pthread_mutex_unlock(&pnm_cp->mutex);
			pthread_mutex_unlock(&cp_sp_mutex);
			return create_new_sslsvr_con(httpcon->fd);
		}

		// Try second socket
		psp = psp2;
		pthread_mutex_unlock(&pnm_cp->mutex);
		goto return_con_from_pool;
        }

	// Good time to delete psp from pool
	fd = psp->fd;
	if (NM_CHECK_FLAG(pnm_cp, NMF_IS_IPV6)) {
		AO_fetch_and_sub1(&glob_ssl_cp_tot_sockets_in_pool_ipv6);
	} else {
		AO_fetch_and_sub1(&glob_ssl_cp_tot_sockets_in_pool);
	}
	psp->magic = CPSP_MAGIC_DEAD;
	free(psp);
	pthread_mutex_unlock(&cp_sp_mutex);

	sslcon = (ssl_con_t *)gnm[fd].private_data;
	assert(sslcon->magic == CPMAGIC_IN_POOL);
	sslcon->magic = CPMAGIC_ALIVE;

	/* delete for now, this socket will be reused */
	NM_del_event_epoll(fd);

	/* the socket is valid */
	SET_CON_FLAG(sslcon, CPF_USE_KA_SOCKET);
	glob_ssl_cp_socket_reused ++;


	/* don't release this mutex locker here */
//	pthread_mutex_unlock(&pnm_cp->mutex);
	DBG_LOG(MSG, MOD_SSL, "extract socket %d from connection pool.", fd);
	return sslcon;
}

