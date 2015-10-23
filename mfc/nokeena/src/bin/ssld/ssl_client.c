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
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "ssl_defs.h"
#include "nkn_debug.h"
#include "nkn_stat.h"
#include "nkn_assert.h"
#include "nkn_lockstat.h"
#include "ssl_server.h"
#include "ssl_interface.h"
#include "ssl_network.h"
#include "ssld_mgmt.h"
#include "openssl/ssl.h"
#include "nkn_ssl.h"
#include "openssl/x509v3.h"

NKNCNT_DEF(origin_tot_ssl_sockets, AO_t, "", "Total connected ssl origin side socket")
NKNCNT_DEF(origin_tot_http_sockets, AO_t, "", "Current app sockets")
NKNCNT_DEF(origin_cur_open_ssl_sockets, AO_t, "", "Current origin side ssl  sockets")
NKNCNT_DEF(origin_tot_size_from_ssl, AO_t, "", "total received bytes from ssl origin")
NKNCNT_DEF(origin_tot_size_sent_ssl, AO_t, "", "total bytes sent to origin")
NKNCNT_DEF(origin_tot_size_http_sent, AO_t, "" , "total bytes sent to nvsd")
NKNCNT_DEF(origin_tot_size_http_received, AO_t, "", "total bytes received from http")
NKNCNT_DEF(origin_tot_handshake_failure, AO_t, "", "Total handshake failure cases")
NKNCNT_DEF(origin_tot_handshake_success, AO_t, "", "Total handshake Success cases")
NKNCNT_DEF(origin_tot_cert_setup_err_cnt, AO_t, "", "Total Certificate CTX setup erroro")
NKNCNT_DEF(origin_tot_ssl_con_malloc_cnt, AO_t, "", "Total SSL_con malloc count")
NKNCNT_DEF(origin_cur_ssl_con_malloc_cnt, AO_t, "", "Total SSL_con malloc count")
NKNCNT_DEF(origin_tot_http_timeout, AO_t, "", "Total http timeout")
NKNCNT_DEF(origin_tot_ssl_timeout, AO_t, "", "Total ssl timeout")
NKNCNT_DEF(origin_tot_server_auth_failures, AO_t, "", "Total ssl server auth failure cases")
NKNCNT_DEF(ssl_unread_data_close, AO_t, "", "Total ssl close with pending untread data")


BIO *bio_cli_err=0;

static int s_server_session_id_context = 1;
int ssl_cli_server_port=443;
static int init_start = 0;
extern int ssl_server_auth;
extern char *ssl_ciphers;
int ssl_cli_listen_intfcnt = 0;
char ssl_cli_listen_intfname[MAX_NKN_INTERFACE][16];
extern network_mgr_t * gnm;
int nkn_ssl_cli_serverport[MAX_PORTLIST] ;

void ssl_cli_interface_init(void);
void nkn_setup_interface_parameter(int i);
int ssl_cli_accept(ssl_con_t * ssl_con);

int ssl_cli_exit_err(const char * string);
SSL_CTX * ssl_cli_initialize_ctx(const  char * certfile, const char *keyfile, const char *password);

SSL_CTX * ssl_cli_server_init(const char *cert_name, const char *key_name, const char *passphrase, const char *servername, const char *ciphers);

static int ssl_connect(int fd, ssl_con_t *ssl_con);

static int http_client_epollin(int fd, void *private_date);
static int http_client_epollout(int fd, void *private_data);
static int http_client_epollerr(int fd, void *private_data);
static int http_client_epollhup(int fd, void *private_data);
static int http_client_timeout(int fd, void * private_data, double timeout);

static int forward_request_to_ssl(ssl_con_t * ssl_con);
static int forward_response_to_nvsd(ssl_con_t * http_con);

int ssl_check_cert(SSL * ssl, char * host);

SSL_CTX * ssl_cli_initialize_ctx(const char *certfile, const char * keyfile, const char * password)
{
#ifdef USE_SAMARA_SSL
	SSL_METHOD *meth;
#else
	const SSL_METHOD *meth;
#endif
	SSL_CTX * pctx;
    
	/*
	 * 1. Initialization SSL library.
	 */
	/* Global system initialization*/
	if(!init_start) {
		SSL_library_init();

		SSL_load_error_strings();
		init_start++;
		bio_cli_err=BIO_new_fp(stderr,BIO_NOCLOSE);
		DBG_LOG(MSG, MOD_SSL, "SSL library Init done\n");


	
	}
      
	/* An error write ssl_cli_ctxtext */

	/*
	 * 2. Create our ssl_context
	 */
	meth = SSLv23_client_method();
	pctx=SSL_CTX_new(meth);
	if(pctx == NULL)  {
		DBG_LOG(MSG, MOD_SSL, "Failed to create SSL_CTX , certficate %s key_file %s\n", 
							certfile, keyfile);
		return NULL;
	}

	/*
	 * 3. Load the CAs we trust, root certificates
	 */
	if(!(SSL_CTX_load_verify_locations(pctx, NULL, CA_CERT_PATH))) {
		log_ssl_stack_errors();
	}

	if(!certfile)
		return pctx;

	/*
	 * 3. Load our keys and certificates
	 *    if it is password protected, use password.
	 */
	
	if(!(SSL_CTX_use_certificate_chain_file(pctx, certfile))) {
		log_ssl_stack_errors();
		SSL_CTX_free(pctx);
		return NULL;
	}

	if (password != NULL) {
		SSL_CTX_set_default_passwd_cb_userdata(pctx, (void *)password);
	}

	if(!keyfile)
		return pctx;


	if(!(SSL_CTX_use_PrivateKey_file(pctx, keyfile, SSL_FILETYPE_PEM))) {
		log_ssl_stack_errors();
		SSL_CTX_free(pctx);
		return NULL;
	}

    
	return pctx;
}
     

/*
 * This function cleans up HTTP structure and ssl_con structure
 * It should be called only once for each socket at the time of socket close time
 */
void close_cli_conn(int fd, int nolock)
{
	int peer_fd;
	ssl_con_t * ssl_con, * peer_con;

	if (fd == -1) return;

	ssl_con = (ssl_con_t *)gnm[fd].private_data;
	if (ssl_con == 0) return;

	peer_fd = ssl_con->peer_fd;

	DBG_LOG(MSG, MOD_SSL, "close_cli_conn: fd %d errno=%d", fd, errno);
        if (fd > 0) 
        {

	    unsigned int incarn = gnm[fd].incarn;
	    int locked;
		
	    network_mgr_t *pnm = &gnm[fd];
	    if(nolock == 0) {
		    locked = pthread_mutex_trylock(&gnm[fd].mutex);
	    } else {
		    locked = -1;
	    }

	    if(((locked == 0) || NM_CHECK_FLAG(pnm, NMF_IN_TIMEOUTQ) || nolock )&& incarn == gnm[fd].incarn) 
	    {
		if (ssl_con && ssl_con->ssl) 
		{
			int r;
			int pending_bytes;

			r = NM_get_tcp_recvq_pending_bytes(ssl_con->fd, &pending_bytes);
			if(r == 0 && pending_bytes > 0) {
				glob_ssl_unread_data_close++;

				// Read and discard all the messages
        			r = SSL_read(ssl_con->ssl, &ssl_con->cb_buf[0], pending_bytes>MAX_CB_BUF?MAX_CB_BUF:pending_bytes);
			}
			/* close SSL client side socket */
			SSL_set_shutdown(ssl_con->ssl, SSL_SENT_SHUTDOWN|SSL_RECEIVED_SHUTDOWN);
#if 0
			if(!r){
				/*
				 * If we called SSL_shutdown() first then
				 * we always get return value of '0'. In
				 * this case, try again, but first send a
				 * TCP FIN to trigger the other side's
				 * close_notify
				 */
				shutdown(ssl_con->fd, 1);
				r=SSL_set_shutdown(ssl_con->ssl, SSL_SENT_SHUTDOWN|SSL_RECEIVED_SHUTDOWN);
			}
#endif
			SSL_free(ssl_con->ssl);
			ssl_con->ssl = NULL;
			AO_fetch_and_sub1(&glob_origin_cur_open_ssl_sockets);
		}


		if(ssl_con->domainname) free(ssl_con->domainname);

		NM_close_socket(fd);
		AO_fetch_and_sub1(&glob_origin_cur_ssl_con_malloc_cnt);
		if(peer_fd > 0 ) {
                        gnm[peer_fd].peer_fd = -1;
                }

		free(ssl_con);


	    }

	    if(locked == 0) pthread_mutex_unlock(&gnm[fd].mutex);
	}

        if (peer_fd > 0)
        {
	    peer_con = (ssl_con_t *)gnm[peer_fd].private_data;

	    if(peer_con == NULL) return;

	    unsigned int incarn = gnm[peer_fd].incarn;
	    int locked;
	    network_mgr_t *pnm = &gnm[peer_fd];

	    if(nolock == 0) {
	    	locked = pthread_mutex_trylock(&gnm[peer_fd].mutex);
	    } else {
		locked = -1;
	    }

	    if(((locked == 0) || NM_CHECK_FLAG(pnm, NMF_IN_TIMEOUTQ) || nolock )&& incarn == gnm[peer_fd].incarn)
	    { 
		if  (peer_con && peer_con->ssl)
		{
			int r;

			/* close SSL client side socket */
			SSL_set_shutdown(peer_con->ssl, SSL_SENT_SHUTDOWN|SSL_RECEIVED_SHUTDOWN);
#if 0
			if(!r){
				/*
				 * If we called SSL_shutdown() first then
				 * we always get return value of '0'. In
				 * this case, try again, but first send a
				 * TCP FIN to trigger the other side's
				 * close_notify
				 */
				shutdown(peer_con->fd, 1);
				r=SSL_set_shutdown(peer_con->ssl, SSL_SENT_SHUTDOWN|SSL_RECEIVED_SHUTDOWN);
			}
#endif
			SSL_free(peer_con->ssl);
			peer_con->ssl = NULL;
			AO_fetch_and_sub1(&glob_origin_cur_open_ssl_sockets);
		}

		if(peer_con->domainname) free(peer_con->domainname);

		NM_close_socket(peer_fd);
		gnm[fd].peer_fd = -1;
		AO_fetch_and_sub1(&glob_origin_cur_ssl_con_malloc_cnt);
		free(peer_con);

	
    	    }

      	    if(locked == 0)
		pthread_mutex_unlock(&gnm[peer_fd].mutex);
        }
}


/* ***************************************************
 * The following functions handles server side socket.
 * *************************************************** */

ssl_con_t * create_new_sslsvr_con(int cli_fd)
{
	int ssl_fd;
	ssl_con_t * http_con, * ssl_con;
	struct sockaddr_in srv;
	int ret;


	DBG_LOG(MSG, MOD_SSL, "create_new_sslsvr_con: called fd=%d", cli_fd);
	http_con = gnm[cli_fd].private_data;

	ssl_con = (ssl_con_t *)calloc(1, sizeof(ssl_con_t));
	if(!ssl_con) {
		return NULL;
	}

	AO_fetch_and_add1(&glob_origin_cur_ssl_con_malloc_cnt);
	ssl_con->magic = CPMAGIC_ALIVE;
        ssl_con->fd = -1;

	ssl_con->pns = http_con->pns;
	if(http_con->domainname) 
		ssl_con->domainname = strdup(http_con->domainname) ;

#if 1
	ssl_con->src_ipv4v6.family = http_con->remote_src_ipv4v6.family;
	if(ssl_con->src_ipv4v6.family == AF_INET) {
		IPv4(ssl_con->src_ipv4v6) = IPv4(http_con->remote_src_ipv4v6);
	} else {
		memcpy(&IPv6(ssl_con->src_ipv4v6), &IPv6(http_con->remote_src_ipv4v6), sizeof(struct in6_addr));
	}

	ssl_con->src_port = http_con->remote_src_port;

	ssl_con->dest_ipv4v6.family = http_con->dest_ipv4v6.family;
	if(ssl_con->dest_ipv4v6.family == AF_INET) {
		IPv4(ssl_con->dest_ipv4v6) = IPv4(http_con->dest_ipv4v6);
	} else {
		memcpy(&IPv6(ssl_con->dest_ipv4v6), &IPv6(http_con->dest_ipv4v6), sizeof(struct in6_addr));
	}

	ssl_con->dest_port = http_con->dest_port ; //ssl_cli_server_port ? ssl_cli_server_port : 443 ;


#endif

        if( (ssl_fd = socket(http_con->dest_ipv4v6.family, SOCK_STREAM, 0)) < 0) {
                DBG_LOG(WARNING, MOD_SSL, "FP Failed to create fd, errno=%d.", errno);
		AO_fetch_and_sub1(&glob_origin_cur_ssl_con_malloc_cnt);
		free(ssl_con);
		return NULL;
        }

	ssl_con->fd = ssl_fd;
	pthread_mutex_lock(&gnm[ssl_fd].mutex);	

	ssl_con->ssl_fid = http_con->ssl_fid;
	// Set to non-blocking socket. so it is non-blocking connect.
	NM_set_socket_nonblock(ssl_fd);

	if(register_NM_socket(ssl_con->fd,
                        (void *)ssl_con,
                        ssl_origin_epollin,
                        ssl_origin_epollout,
                        ssl_origin_epollerr,
                        ssl_origin_epollhup,
                        ssl_origin_timeout,
                        ssl_idle_timeout,
			gnm[cli_fd].accepted_thr_num) == FALSE)
	{
		close(ssl_con->fd);
		AO_fetch_and_sub1(&glob_origin_cur_ssl_con_malloc_cnt);
		free(ssl_con);
		pthread_mutex_unlock(&gnm[ssl_fd].mutex);	
		return NULL;
	}

	

	if(http_con->dest_ipv4v6.family == AF_INET) {
		/* Now time to make a socket connection. */
	        bzero(&srv, sizeof(srv));
        	srv.sin_family = AF_INET;
		srv.sin_addr.s_addr = IPv4(http_con->dest_ipv4v6);
        	srv.sin_port = http_con->dest_port;
		ret = connect(ssl_fd, (struct sockaddr *)&srv, sizeof(struct sockaddr));
	} else {
		struct sockaddr_in6 srv_v6;
		bzero(&srv_v6, sizeof(srv_v6));
		srv_v6.sin6_family = AF_INET6;

		memcpy(&srv_v6.sin6_addr.s6_addr, &IPv6(http_con->dest_ipv4v6), sizeof(struct in6_addr));
		srv_v6.sin6_port = http_con->dest_port;
		ret = connect(ssl_fd, (struct sockaddr *)&srv_v6, sizeof(srv_v6));
	}
	if(ret < 0)
	{
		if(errno == EINPROGRESS) {
			DBG_LOG(MSG, MOD_SSL, "connect(0x%x) fd=%d in progress", 
						IPv4(http_con->dest_ipv4v6), ssl_fd);
			if (NM_add_event_epollout(ssl_fd)==FALSE) {
				// NOTE: unregister_NM_socket(fd, TRUE) deletes
				// the fd from epoll loop.
				unregister_NM_socket(ssl_con->fd);
				free(ssl_con);
				DBG_LOG(MSG, MOD_SSL, "failed to add epollout event fd = %d", http_con->fd); 
				AO_fetch_and_sub1(&glob_origin_cur_ssl_con_malloc_cnt);
				close(ssl_fd);
				pthread_mutex_unlock(&gnm[ssl_fd].mutex);	
				return NULL;
			}
			SET_CON_FLAG(ssl_con, CONF_SYN_SENT);
			http_con->peer_fd = ssl_fd;
        		ssl_con->peer_fd = cli_fd;
			AO_fetch_and_add1(&glob_origin_cur_open_ssl_sockets);
			AO_fetch_and_add1(&glob_origin_tot_ssl_sockets);
			return ssl_con;
		}
		DBG_LOG(MSG, MOD_SSL, "connect(0x%x) fd=%d failed, errno=%d", 
					IPv4(ssl_con->dest_ipv4v6), ssl_fd, errno);
		unregister_NM_socket(ssl_con->fd);
		free(ssl_con);
		AO_fetch_and_sub1(&glob_origin_cur_ssl_con_malloc_cnt);
		close(ssl_fd);
		pthread_mutex_unlock(&gnm[ssl_fd].mutex);	
		return NULL;
	}

	AO_fetch_and_add1(&glob_origin_cur_open_ssl_sockets);
	AO_fetch_and_add1(&glob_origin_tot_ssl_sockets);
#if 0
	SET_CON_FLAG(ssl_con, CONF_SSL_CONNECT);

#endif
	http_con->peer_fd = ssl_fd;
        ssl_con->peer_fd = cli_fd;

	if(ssl_connect(ssl_fd, ssl_con) == FALSE) {
		pthread_mutex_unlock(&gnm[ssl_fd].mutex);	
		return NULL;
	}
#if 0
	/* sent data in case we have data there */
	http_con = (ssl_con_t *)gnm[ssl_con->peer_fd].private_data;
	if (forward_response_to_nvsd(http_con) == FALSE) {
		DBG_LOG(MSG, MOD_SSL, "forward_request_to_nvsd failed ssl_fd %d http_fd %d", 
					ssl_con->fd, http_con->fd);
		close_cli_conn(http_con->fd);
		return NULL;
	}
#endif

	/*
	 * Socket has been established successfully by now.
	 */
	return ssl_con;
}
static int ssl_connect(int fd, ssl_con_t *ssl_con)
{
        int ret;
	ssl_con_t * http_con;
	SSL_CTX *ssl_ctx;

        /* Connect the SSL socket */
        if (ssl_con->ssl==NULL) {
		pthread_mutex_lock(&cipher_lock);
		ssl_ctx = ssl_cli_server_init(NULL, NULL, NULL, NULL, ssl_ciphers);
		pthread_mutex_unlock(&cipher_lock);

                ssl_con->ssl=SSL_new(ssl_ctx);
                ssl_con->sbio=BIO_new_socket(fd, BIO_NOCLOSE);
                SSL_set_bio(ssl_con->ssl, ssl_con->sbio, ssl_con->sbio);
                SET_CON_FLAG(ssl_con, CONF_SSL_HANDSHAKE);
		if(ssl_ctx)
			SSL_CTX_free(ssl_ctx);
        }

	if(!ssl_con->ssl) {
		DBG_LOG(WARNING, MOD_SSL, "SSL Could not ger client context\n");
		AO_fetch_and_sub1(&glob_origin_cur_open_ssl_sockets);
		AO_fetch_and_add1(&glob_origin_tot_cert_setup_err_cnt);
		close_cli_conn(fd, 0);
		return FALSE;
	}

	if(ssl_con->domainname == NULL || strlen (ssl_con->domainname) <= 0 ||
			(!SSL_set_tlsext_host_name(ssl_con->ssl, ssl_con->domainname))) {
		DBG_LOG(MSG, MOD_SSL, "Could not set Host name %s", 
			ssl_con->domainname? ssl_con->domainname: "NULL");
		AO_fetch_and_sub1(&glob_origin_cur_open_ssl_sockets);
		AO_fetch_and_add1(&glob_origin_tot_cert_setup_err_cnt);
		close_cli_conn(fd, 0);
		return FALSE;
	}

        ret = SSL_connect(ssl_con->ssl);
        if(ret == 1) {
                // SSL successfully established
                DBG_LOG(MSG, MOD_SSL, "SSL socket connected");
                UNSET_CON_FLAG(ssl_con, CONF_SSL_HANDSHAKE);
                SET_CON_FLAG(ssl_con, CONF_SSL_READY);

                if( ssl_server_auth /*require_server_auth*/) {
                        if (ssl_check_cert(ssl_con->ssl, ssl_con->domainname)==FALSE) {
				AO_fetch_and_add1(&glob_origin_tot_server_auth_failures);
                                close_cli_conn(fd, 0);
                                return FALSE;
                        }
                }

		AO_fetch_and_add1(&glob_origin_tot_handshake_success);

		
                http_con = (ssl_con_t *)gnm[ssl_con->peer_fd].private_data;

	
	        forward_request_to_ssl(http_con);
           	return TRUE;
        }
        else if(ret ==0) {
		// SSL failed to be established.
		AO_fetch_and_add1(&glob_origin_tot_handshake_failure);
		log_ssl_stack_errors();
        	ret = SSL_get_error(ssl_con->ssl, ret);
	        DBG_LOG(MSG, MOD_SSL, "ret=%d", ret);
                close_cli_conn(fd, 0);
                return FALSE;
        }

        ret = SSL_get_error(ssl_con->ssl, ret);
        DBG_LOG(MSG, MOD_SSL, "ret=%d", ret);
       switch(ret)
        {
                case SSL_ERROR_WANT_WRITE:
                        //NM_add_event_epollout(fd);
                        break;
                case SSL_ERROR_WANT_READ:
                        NM_add_event_epollin(fd);
                        break;
                default:
			log_ssl_stack_errors() ;
			AO_fetch_and_add1(&glob_origin_tot_handshake_failure);
                        close_cli_conn(fd, 0);
                        return FALSE;
        }
        return TRUE;
}


void init_cli_conn(int svrfd, int http_fd, nkn_interface_t * pns, struct sockaddr_storage * addr, int thr_num)
{

	struct ssl_con_t *http_con ;
	struct sockaddr_in svraddr;
        socklen_t addrlen;

	/*
	 * Set up this client socket
	 */

	getsockname(svrfd, (struct sockaddr *)&svraddr, &addrlen);
	if (NM_set_socket_nonblock(http_fd) == FALSE) {
		DBG_LOG(MSG, MOD_SSL,
				 "Failed to set socket %d to be nonblocking ssocket.", http_fd);
		close(http_fd);
		return;
	}

	http_con = (struct ssl_con_t *)calloc(sizeof(struct ssl_con_t ), 1);
	if(!http_con) {
		DBG_LOG(WARNING, MOD_SSL, "SSL con malloc failed\n");
		close(http_fd);
		return;
	}
	glob_origin_tot_ssl_con_malloc_cnt++;
	AO_fetch_and_add1(&glob_origin_cur_ssl_con_malloc_cnt);

	http_con->magic = CPMAGIC_ALIVE;
	http_con->fd = http_fd;
	http_con->peer_fd = -1;
	http_con->pns = pns;
	if(addr->ss_family == AF_INET) {
		struct sockaddr_in *addrptr = (struct sockaddr_in *)addr;
		IPv4(http_con->src_ipv4v6) = addrptr->sin_addr.s_addr;
		http_con->src_ipv4v6.family = AF_INET;
		http_con->src_port = addrptr->sin_port;
		
	} else {
		struct sockaddr_in6 *addrptrv6 = (struct sockaddr_in6*)addr;
		http_con->src_ipv4v6.family = AF_INET6;
		http_con->src_port = addrptrv6->sin6_port;
		memcpy(&IPv6(http_con->src_ipv4v6), &addrptrv6->sin6_addr, sizeof(struct in6_addr));
	}

	if(svraddr.sin_family == AF_INET) {
		struct sockaddr_in *addrptr = (struct sockaddr_in *)&svraddr;
		http_con->dest_ipv4v6.family = AF_INET;
		IPv4(http_con->dest_ipv4v6) = (addrptr->sin_addr.s_addr);

	} else if(svraddr.sin_family == AF_INET6) {
		struct sockaddr_in6 *addrptrv6 = (struct sockaddr_in6*)&svraddr;
		http_con->dest_ipv4v6.family = AF_INET6;
		http_con->dest_port = addrptrv6->sin6_port;
		memcpy(&IPv6(http_con->dest_ipv4v6), &addrptrv6->sin6_addr, sizeof(struct in6_addr));
	}

	http_con->dest_port = svraddr.sin_port; //http_server_port;
	SET_CON_FLAG(http_con, CONF_CLIENT_SIDE);

	DBG_LOG(MSG, MOD_SSL, "ssl_con =%p fd=%d", http_con, http_fd);
	SET_CON_FLAG(http_con, CONF_SSL);

	if(register_NM_socket(http_fd,
				(void *)http_con,
				http_client_epollin,
				http_client_epollout,
				http_client_epollerr,
				http_client_epollhup,
				http_client_timeout,
				ssl_idle_timeout,
				-1) == FALSE)
	{
		DBG_LOG(MSG, MOD_SSL, "HTTP client Registration error");
		free(http_con);
		AO_fetch_and_sub1(&glob_origin_cur_ssl_con_malloc_cnt);
		close(http_fd);
		return; 
	}

	
	NM_add_event_epollin(http_fd);

	AO_fetch_and_add1(&glob_origin_tot_http_sockets);
	return;
}



SSL_CTX * ssl_cli_server_init(const char *cert_name, const char *key_name, const char *passphrase, const char *servername, const char *ciphers)
{
	SSL_CTX *ssl_cli_ctx;

	ssl_cli_ctx = ssl_cli_initialize_ctx(cert_name, key_name, passphrase);
	if(ssl_cli_ctx == NULL) {
		return NULL;
	}
	/* 2. Set our cipher list */
	if(ciphers && (ciphers[0]!=0)){
		if(!SSL_CTX_set_cipher_list(ssl_cli_ctx, ciphers)) 
		{
			DBG_LOG(MSG, MOD_SSL, "Couldn't set Cipher set %s", ciphers);
			ERR_print_errors(bio_cli_err);
			return ssl_cli_ctx;
		}
	} else {
		if(!SSL_CTX_set_cipher_list(ssl_cli_ctx, "DEFAULT")) 
		{
			DBG_LOG(MSG, MOD_SSL, "Couldn't set DEFAULT Cipher set");
			ERR_print_errors(bio_cli_err);
			return ssl_cli_ctx;
		}
	}

	return ssl_cli_ctx;
}


/* *sl_cli_server_init	********************************************************
 * Callback functions from epoll_wait loop.
 * ***********************************************************/
int ssl_origin_epollin(int fd, void *private_data)
{
	ssl_con_t * ssl_con = (ssl_con_t *)private_data;
	int rlen;
	int ret;
	DBG_LOG(MSG, MOD_SSL, "fd=%d", fd);
#if 0
	if (CHECK_CON_FLAG(ssl_con, CONF_SSL_CONNECT)) {
		UNSET_CON_FLAG(ssl_con, CONF_SSL_CONNECT);
		if (ssl_connect(fd, ssl_con) == FALSE) {
			DBG_LOG(MSG, MOD_SSL, "fd=%d ssl_connect returns FALSE", fd);
			return TRUE;
		}
			DBG_LOG(MSG, MOD_SSL, "fd=%d ssl_connect returns TRUE", fd);
		return TRUE;
	}
#endif

	if(CHECK_CON_FLAG(ssl_con, CONF_SYN_SENT)) {
		NM_add_event_epollout(fd);
		return TRUE;
	}
	//if(CHECK_CON_FLAG(ssl_con, CONF_SSL_HANDSHAKE))  {
//		ssl_connect(ssl_con->fd, ssl_con);
	//}
	if(!CHECK_CON_FLAG(ssl_con, CONF_SSL_READY)) {
		ssl_connect(ssl_con->fd, ssl_con);
		return TRUE;
	}

	rlen = MAX_CB_BUF - ssl_con->cb_totlen;
	if(rlen == 0) {
		/* No more space */
		if (NM_del_event_epoll(fd)==FALSE) {
			close_cli_conn(fd, 0);
		}
		return TRUE;
	}

        ret = SSL_read(ssl_con->ssl, &ssl_con->cb_buf[ssl_con->cb_totlen], rlen);
	DBG_LOG(MSG, MOD_SSL, "fd=%d recv=%d bytes", fd, ret);
	if (ret>0) {
                DBG_LOG(MSG, MOD_SSL, "SSL_ERROR_NONE");
                glob_origin_tot_size_from_ssl += ret;
                ssl_con->cb_totlen += ret;

                if (forward_response_to_nvsd(ssl_con) == FALSE) {
                        //close_cli_conn(fd);
                        return TRUE;
                }
		
                return TRUE;
        }


	ret = SSL_get_error(ssl_con->ssl, ret);
	DBG_LOG(MSG, MOD_SSL, "SSL_get_error ret=%d", ret);
	switch(ret) {
		case SSL_ERROR_ZERO_RETURN:
			// Peer close the SSL socket
			close_cli_conn(fd, 0);
			return TRUE;
		case SSL_ERROR_WANT_WRITE:
			NM_add_event_epollout(fd);
			break;
		case SSL_ERROR_WANT_READ:
			NM_add_event_epollin(fd);
			break;
		case SSL_ERROR_SYSCALL:
		default:
			log_ssl_stack_errors();
			DBG_LOG(MSG, MOD_SSL, "r_code=%d", ret);
			close_cli_conn(fd, 0);
			return TRUE;
	}

	return TRUE;
}

int ssl_origin_epollout(int fd, void *private_data)
{
	ssl_con_t * ssl_con = (ssl_con_t *)private_data;
	ssl_con_t * http_con;
	int ret;
	int len ;	
	int http_fd;

	DBG_LOG(MSG, MOD_SSL, "fd=%d called", fd);
	UNUSED_ARGUMENT(fd);

	if (CHECK_CON_FLAG(ssl_con, CONF_SYN_SENT)) {
		UNSET_CON_FLAG(ssl_con, CONF_SYN_SENT);
                len=sizeof(ret);
		
                if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *)&ret,
                       (socklen_t *)&len)) {
                        DBG_LOG(MSG, MOD_SSL, "getsockopt(fd=%d) failed, errno=%d", fd, errno);
                        close_cli_conn(fd, 0);
                        return TRUE;
                }

                if(ret) {
                        DBG_LOG(MSG, MOD_SSL, "connect(fd=%d) failed, ret=%d", fd, ret);
                        close_cli_conn(fd, 0);
                        return TRUE;
                }

                ssl_connect(fd, ssl_con);
                return TRUE;
        }

	http_con = (ssl_con_t *)gnm[ssl_con->peer_fd].private_data;
	forward_request_to_ssl(http_con);

	return TRUE;
}
int ssl_origin_epollerr(int fd, void *private_data)
{
	DBG_LOG(MSG, MOD_SSL, "fd=%d called", fd);
	UNUSED_ARGUMENT(private_data);
	close_cli_conn(fd, 0);
	return TRUE;
}

int ssl_origin_epollhup(int fd, void *private_data)
{
	DBG_LOG(MSG, MOD_SSL, "fd=%d called", fd);
	return ssl_origin_epollerr(fd, private_data);
}

int ssl_origin_timeout(int fd, void * private_data, double timeout)
{
	int ret = TRUE;
	ssl_con_t *ssl_con = (ssl_con_t *)private_data;
	unsigned int incarn;
	DBG_LOG(MSG, MOD_SSL, "fd=%d called", fd);
	UNUSED_ARGUMENT(timeout);
	glob_origin_tot_ssl_timeout++;
	if(gnm[fd].peer_fd > 0) {
		network_mgr_t *pnm = &gnm[ssl_con->peer_fd];
		incarn = pnm->incarn;
		pthread_mutex_lock(&pnm->mutex);
		if(incarn == pnm->incarn) {
			close_cli_conn(fd, 1);
			pthread_mutex_unlock(&pnm->mutex);
			return TRUE;
		}
		
		pthread_mutex_unlock(&pnm->mutex);

	}

	close_cli_conn(fd, 1);
	return TRUE;
}

static int forward_request_to_ssl(ssl_con_t * http_con)
{
	ssl_con_t * ssl_con;
	int ret, len;
	char * p;
	int fd ;
	DBG_LOG(MSG, MOD_SSL, "fd=%d called", http_con->fd);

	if(!http_con)
		return TRUE;
	

	fd = http_con->fd;
#if 1
//	if(!CHECK_CON_FLAG(http_con, CONF_SSL_CONNECT)) {
	if(http_con->peer_fd <= 0) {
		//ssl_con = create_new_sslsvr_con(http_con->fd);
		ssl_con = cp_open_socket_handler(http_con);
		if(ssl_con != NULL) {
			SET_CON_FLAG(http_con, CONF_SSL_CONNECT);
			http_con->peer_fd = ssl_con->fd;
			ssl_con->peer_fd = http_con->fd;
			gnm[http_con->fd].peer_fd = ssl_con->fd;
			gnm[ssl_con->fd].peer_fd = http_con->fd;
			ssl_con->ssl_fid = http_con->ssl_fid;
			if(cp_activate_socket_handler(ssl_con->fd, ssl_con) == FALSE) {
				close_cli_conn(fd, 0);
				return TRUE;
			}

		} else {
			close_cli_conn(fd, 0);
			return TRUE;
		}
		
	}

	ssl_con = gnm[http_con->peer_fd].private_data;
	if(!ssl_con) return FALSE;

	NM_set_socket_active(&gnm[ssl_con->fd]);
	if(CHECK_CON_FLAG(ssl_con, CONF_SYN_SENT)) {
		NM_add_event_epollout(ssl_con->fd);
		return TRUE;
	}

	if(!CHECK_CON_FLAG(ssl_con, CONF_SSL_READY)) 
		return TRUE;

	  
#endif

	len = http_con->cb_totlen - http_con->cb_offsetlen;
	while(len) {

		p = &http_con->cb_buf[http_con->cb_offsetlen];
		ret = SSL_write(ssl_con->ssl, p, len);
		if (ret>0) {
			http_con->cb_offsetlen += ret;
			http_con->sent_len += ret;
			len -= ret;
			glob_origin_tot_size_sent_ssl += ret;
			continue;
		}

		//NM_del_event_epollin(httpcon->fd);
		switch(SSL_get_error(ssl_con->ssl, ret)){
		case SSL_ERROR_WANT_WRITE:
			NM_add_event_epollout(ssl_con->fd);
			return TRUE;
		case SSL_ERROR_WANT_READ:
			NM_add_event_epollin(ssl_con->fd);
			return TRUE;
		default:
			DBG_LOG(MSG, MOD_SSL, "r_code=%d", ret);
			log_ssl_stack_errors();
			close_cli_conn(http_con->fd, 0);
			return FALSE;
		}
	}

        if (NM_add_event_epollin(ssl_con->fd)==FALSE) {
                close_cli_conn(ssl_con->fd, 0);
                return TRUE;
        }

        if (NM_add_event_epollin(http_con->fd)==FALSE) {
                close_cli_conn(http_con->fd, 0);
                return TRUE;
        }

	http_con->cb_offsetlen = 0;
	http_con->cb_totlen = 0;
	return TRUE;
}

static int forward_response_to_nvsd(ssl_con_t * ssl_con)
{
	ssl_con_t * http_con;
	int ret, len;
	char * p;
	DBG_LOG(MSG, MOD_SSL, "fd=%d called", ssl_con->fd);

	http_con = gnm[ssl_con->peer_fd].private_data;
	if(!http_con) return FALSE;

	NM_set_socket_active(&gnm[http_con->fd]);

	len = ssl_con->cb_totlen - ssl_con->cb_offsetlen;
	while(len) {

		p = &ssl_con->cb_buf[ssl_con->cb_offsetlen];
		*(p+len) = 0; // To Be Fixed: could memory overwritten
		DBG_LOG(MSG, MOD_SSL, "forward_request_to_nvsd: peer_con->fd=%d, len=%d p=%p", 
				http_con->fd, len, p);
		ret = send(http_con->fd, p, len, 0);
		if( ret==-1 ) {
			DBG_LOG(MSG, MOD_SSL, "forward_request_to_nvsd: ERROR ret=-1 errno=%d", errno);
			if(errno == EAGAIN) {
				NM_add_event_epollout(http_con->fd);
				NM_del_event_epoll(ssl_con->fd);
				return TRUE;
			}

			close_cli_conn(ssl_con->fd, 0);
			return FALSE;
		} 
		ssl_con->cb_offsetlen += ret;
		ssl_con->sent_len += ret;
		len -= ret;
		glob_origin_tot_size_http_sent += ret;
	}

	if (NM_add_event_epollin(ssl_con->fd)==FALSE) {
		close_cli_conn(ssl_con->fd, 0);
		return TRUE;
	}

	if (NM_add_event_epollin(http_con->fd)==FALSE) {
		close_cli_conn(http_con->fd, 0);
		return TRUE;
	}

	DBG_LOG(MSG, MOD_SSL, "forward_request_to_nvsd: send complete");
	ssl_con->cb_offsetlen = 0;
	ssl_con->cb_totlen = 0;
	return TRUE;
}


/* ***************************************************
 * The following functions handles client side socket.
 * *************************************************** */

static int http_client_epollin(int fd, void * private_data)
{
	ssl_con_t * http_con = (ssl_con_t *)private_data;
	int rlen;
	int ret;

	rlen = MAX_CB_BUF - http_con->cb_totlen;
	if(rlen == 0) {
		/* No more space */
		if (NM_del_event_epoll(fd)==FALSE) {
			close_cli_conn(fd, 0);
		}
		return TRUE;
	}
	ret = recv(fd, &http_con->cb_buf[http_con->cb_totlen], rlen, 0);

	if(ret < 0) {
		if ((ret==-1) && (errno==EAGAIN)) {
			return TRUE;
		}
		close_cli_conn(fd, 0);
		return TRUE;
	} else if(ret == 0 ) {
		/* close httpcon->fd after sending the remaining data */
		DBG_LOG(MSG, MOD_SSL, "fd=%d recv=%d", fd, ret);
		// TODO add to conn pool 
		if(http_con->peer_fd>0) {
			ssl_con_t *ssl_con = (ssl_con_t *)gnm[http_con->peer_fd].private_data;
			if(ssl_con) {
				pthread_mutex_lock(&gnm[ssl_con->fd].mutex);
				if(cpsp_add_into_pool(ssl_con->fd, ssl_con) == TRUE) {
			  	  gnm[http_con->peer_fd].peer_fd = -1;
				  gnm[fd].peer_fd = -1;
				  http_con->peer_fd = -1;
				  ssl_con->peer_fd = -1;
				}

				 pthread_mutex_unlock(&gnm[ssl_con->fd].mutex);
			}
		}


		close_cli_conn(fd, 0);
		return TRUE;

	}
	http_con->cb_totlen += ret;
	glob_origin_tot_size_http_received +=ret;

	if(!http_con->ssl_hdr_sent) {
		ssl_con_hdr_t *hdr = (ssl_con_hdr_t *)&http_con->cb_buf[0];
		if((unsigned)http_con->cb_totlen >= sizeof(ssl_con_hdr_t)) {
			if(hdr->magic == HTTPS_REQ_IDENTIFIER) {
				http_con->ssl_hdr_sent = 1;
				http_con->dest_ipv4v6.family = hdr->dest_ipv4v6.family;


				http_con->ssl_fid = hdr->ssl_fid;
				if(hdr->dest_ipv4v6.family == AF_INET) {
					IPv4(http_con->dest_ipv4v6) = IPv4(hdr->dest_ipv4v6);
				} else {
					memcpy(&IPv6(http_con->dest_ipv4v6), &IPv6(hdr->dest_ipv4v6), sizeof(struct in6_addr));
				}

				http_con->remote_src_ipv4v6.family = hdr->src_ipv4v6.family;
				if(hdr->src_ipv4v6.family == AF_INET) {
					IPv4(http_con->remote_src_ipv4v6) = IPv4(hdr->src_ipv4v6);
				} else {
					memcpy(&IPv6(http_con->remote_src_ipv4v6), &IPv6(hdr->src_ipv4v6), sizeof(struct in6_addr));
				}

				if(hdr->virtual_domain && strlen(hdr->virtual_domain) > 0)
					http_con->domainname = strdup(hdr->virtual_domain);


				http_con->dest_port = hdr->dest_port;
				http_con->remote_src_port = hdr->src_port;
				http_con->cb_totlen -= sizeof(ssl_con_hdr_t);
				memcpy(&http_con->cb_buf[0], &http_con->cb_buf[sizeof(ssl_con_hdr_t)], http_con->cb_totlen);
			}
		}
		
	}

        if (forward_request_to_ssl(http_con) == FALSE) {
                //close_cli_conn(fd);
                return TRUE;
        }

	return TRUE;
}

static int http_client_epollout(int fd, void * private_data)
{
	ssl_con_t * http_con = (ssl_con_t *)private_data;
	ssl_con_t * ssl_con;
	int ret;
	int retlen;

	DBG_LOG(MSG, MOD_SSL, "http_client_epollout: fd=%d", fd);

        /* sent data */
        ssl_con = (ssl_con_t *)gnm[http_con->peer_fd].private_data;
        if (forward_response_to_nvsd(ssl_con) == FALSE) {
                //close_cli_conn(fd);
                return TRUE;
        }

	return TRUE;
}

static int http_client_epollerr(int fd, void * private_data)
{
	DBG_LOG(MSG, MOD_SSL, "fd=%d called", fd);
	UNUSED_ARGUMENT(private_data);
	return TRUE;
}

static int http_client_epollhup(int fd, void * private_data)
{
	DBG_LOG(MSG, MOD_SSL, "fd=%d called", fd);
	return http_client_epollerr(fd, private_data);
}

static int http_client_timeout(int fd, void * private_data, double timeout)
{
	int ret = TRUE;
	unsigned int incarn;
	ssl_con_t *http_con = (ssl_con_t *)private_data;
	DBG_LOG(MSG, MOD_SSL, "fd=%d called", fd);
	UNUSED_ARGUMENT(timeout);
	glob_origin_tot_http_timeout++;
	if(gnm[fd].peer_fd > 0 ) {
		network_mgr_t *pnm = &gnm[http_con->peer_fd];
		incarn = pnm->incarn;
		pthread_mutex_lock(&pnm->mutex);
		if(incarn == pnm->incarn) {
			close_cli_conn(fd, 1);
			pthread_mutex_unlock(&pnm->mutex);
			return ret;
		}

		pthread_mutex_unlock(&pnm->mutex);
	}

	close_cli_conn(fd, 1);
	return TRUE;
}

/* Check that the common name matches the host name*/
int ssl_check_cert(SSL * ssl, char * host)
{
        X509 *peer = NULL;
        char peer_CN[256];
	char *wildcard = NULL;
        char *firstcndot = NULL;
        char *firsthndot = NULL;
	int i = 0 ;
	int idx = -1;
	int len =0 ;

	int ret = SSL_get_verify_result(ssl);
        if(ret!=X509_V_OK) {
                DBG_LOG(MSG, MOD_SSL, "Certificate doesn't verify error %d", ret);
                return FALSE;
        }

        /*
         * Check the cert chain. The chain length
         * is automatically checked by OpenSSL when
         * we set the verify depth in the ctx
         */

        /*Check the common name*/
        peer=SSL_get_peer_certificate(ssl);
	if(peer == NULL)
		return FALSE;

	/* Verify SubjectAltname
	* As per RFC 2818 , if SubjectAltName extension is present ,
	* it should be used as cert's identity .
	*/

	GENERAL_NAMES *subjectAltNames = NULL;
	
	subjectAltNames = X509_get_ext_d2i(peer, NID_subject_alt_name, NULL, &idx );
	while(subjectAltNames != NULL) 
	{

		for (i=0; i< (int )sk_GENERAL_NAME_num(subjectAltNames); i++) {
			GENERAL_NAME *current = sk_GENERAL_NAME_value(subjectAltNames, i);
			
			const char *altptr = (char *)ASN1_STRING_data(current->d.ia5);
			len = (int) ASN1_STRING_length(current->d.ia5);
			switch(current->type) {
			case GEN_DNS:
				memcpy(peer_CN, altptr, len);
				peer_CN[len] = '\0';
				wildcard = strchr(peer_CN, '*');
			        firstcndot = strchr(peer_CN, '.');
			        firsthndot = strchr(host, '.');
			        if(wildcard && firstcndot && (firstcndot - wildcard == 1) &&
			                (!strncasecmp(peer_CN, host, wildcard - peer_CN))  &&
			                firsthndot && !strcasecmp(firstcndot, firsthndot)) {
					GENERAL_NAMES_free(subjectAltNames);
					X509_free(peer);
					return TRUE;
			        } else if(!strcasecmp(peer_CN, host)) {
					GENERAL_NAMES_free(subjectAltNames);
					X509_free(peer);
					return TRUE;
				}
			break;
			case GEN_IPADD:
			{
				unsigned char *p;
			        char oline[256], htmp[5];
				char *host_str = NULL;
				unsigned char hostbuf[sizeof(struct in6_addr)];
				char hline[256];
				p = current->d.ip->data;
				if(current->d.ip->length == 4) {
					snprintf(oline, sizeof(oline), 
						"%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
					host_str = host;
				} else if(current->d.ip->length == 16) {
					oline[0] = 0 ;
					for (i = 0 ; i < 8; i++) {
						snprintf(htmp, sizeof(htmp), "%X", p[0]<<8 | p[1]);
						p+=2;
						strcat(oline, htmp);
						if(i!=7)
							strcat(oline, ":");
					}
				
					hline[0] = 0 ;

					if(inet_pton(AF_INET6, host, hostbuf) <=0) {
					    DBG_LOG(MSG, MOD_SSL, "inet_pton failed %s", host);
					    break;
					}
					
				        p = hostbuf;
					for (i = 0 ; i < 8; i++) {
				            snprintf(htmp, sizeof(htmp), "%X", p[0]<<8 | p[1]);
				            p+=2;
				            strcat(hline, htmp);
				            if(i!=7)
			                      strcat(hline, ":");
					}

					host_str = hline;
		    
				}

				if(!strncmp(oline, host_str, strlen(host_str))) {
					GENERAL_NAMES_free(subjectAltNames);
					X509_free(peer);
					return TRUE;
				}
			}	
			break;
			default:
				break;
			}	
		}

		/* Get the next subject alternative name,
		*  although there should not be more than one . 
		*/
		GENERAL_NAMES_free(subjectAltNames);
		subjectAltNames = X509_get_ext_d2i(peer, NID_subject_alt_name, NULL, &idx);
		
	}

	if (subjectAltNames) GENERAL_NAMES_free(subjectAltNames);


	X509_NAME *name = X509_get_subject_name(peer);

	idx = -1;
	if(name) {
		while((i = X509_NAME_get_index_by_NID(name, NID_commonName, idx)) >=0) {


		idx = i;

		ASN1_STRING *tmp = X509_NAME_ENTRY_get_data(X509_NAME_get_entry(name,idx));
		len = ASN1_STRING_length(tmp);
		memcpy(peer_CN, ASN1_STRING_data(tmp), len);
			
		peer_CN[len] = '\0';	
		
		
        //X509_NAME_get_text_by_NID
          //      (X509_get_subject_name(peer), NID_commonName, peer_CN, 256);



	/* As per RFC following pattersn willbe matched 
	* Names may contain the wildcard 
	* character * which is considered to match any single domain name
	* component or component fragment. E.g., *.a.com matches foo.a.com but
	* not bar.foo.a.com. f*.com matches foo.com but not bar.com. 
	*/
	wildcard = strchr(peer_CN, '*');
        firstcndot = strchr(peer_CN, '.');
        firsthndot = strchr(host, '.');
        if(wildcard && firstcndot && (firstcndot - wildcard == 1) &&
                (!strncasecmp(peer_CN, host, wildcard - peer_CN))  &&
                firsthndot && !strcasecmp(firstcndot, firsthndot)) {
		X509_free(peer);
		return TRUE;
        } else if(!strcasecmp(peer_CN, host)) {
		X509_free(peer);
		return TRUE;
        }
	}
	}

        DBG_LOG(MSG, MOD_SSL, "Common name doesn't match host name %s", host);
        X509_free(peer);
        return FALSE;
}

