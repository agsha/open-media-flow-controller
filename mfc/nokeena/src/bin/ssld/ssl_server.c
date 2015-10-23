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

#ifdef HTTP2_SUPPORT
#include "server_common.h"
#include "server_spdy.h"
#include "server_http2.h"
#include "ssl_server_proto.h"

extern int enable_spdy;
extern int enable_http2;

#endif /* HTTP2_SUPPORT */

NKNCNT_DEF(tot_ssl_sockets, AO_t, "", "Total accepted ssl socket")
NKNCNT_DEF(cur_open_ssl_sockets, AO_t, "", "Current app sockets")
NKNCNT_DEF(tot_size_from_ssl, AO_t, "", "total received bytes from ssl client")
NKNCNT_DEF(tot_size_sent_ssl, AO_t, "", "total bytes sent to client")
NKNCNT_DEF(tot_size_http_sent, AO_t, "" , "total bytes sent to nvsd")
NKNCNT_DEF(tot_size_http_received, AO_t, "", "total bytes received from http")
NKNCNT_DEF(tot_handshake_failure, AO_t, "", "Total handshake failure cases")
NKNCNT_DEF(tot_handshake_done, AO_t, "", "Total handshake Success cases")
NKNCNT_DEF(tot_http_setup_err_cnt, AO_t, "", "Total HTTP setup erroro")
NKNCNT_DEF(tot_cert_setup_err_cnt, AO_t, "", "Total Certificate CTX setup erroro")
NKNCNT_DEF(tot_ssl_con_malloc_cnt, AO_t, "", "Total SSL_con malloc count")
NKNCNT_DEF(tot_ssl_ctx_cnt, AO_t, "", "Total SSL ctx  count")
NKNCNT_DEF(tot_ssl_hdr_malloc_cnt, AO_t, "", "Total SSL_con header malloc count")
NKNCNT_DEF(tot_vhost_cert_ctx_cnt, AO_t, "", "Total Virtual Host SSL CTX count")
NKNCNT_DEF(tot_http_timeout, AO_t, "", "Total http timeout")
NKNCNT_DEF(tot_ssl_timeout, AO_t, "", "Total ssl timeout")


extern char * ssl_ca_list;
extern int ssl_client_auth;
extern char * dhfile;
extern ssl_cert_node_data_t lstSSLCerts [NKN_MAX_SSL_CERTS];
extern ssl_vhost_node_data_t lstSSLVhost[NKN_MAX_SSL_HOST];
extern int ssl_license_enable ;

static pthread_mutex_t ssl_default_ctx_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t ssl_interface_lock = PTHREAD_MUTEX_INITIALIZER;

SSL_CTX * default_ctx = NULL;
const char *default_cert = NULL;
const char *default_key = NULL;
const char *default_pass = NULL;
const char *default_cipher = NULL;

typedef struct {
	char *servername;
	char *cert_name;
	char *key_name;
	char *passphrase;
	char *ciphers;
	SSL_CTX *ssl_ctx;
	pthread_mutex_t ctx_lock; 
}vhost_ssl_ctx_t;

vhost_ssl_ctx_t vhost_ssl_ctx[NKN_MAX_SSL_HOST];

SSL_CTX *ssl_vh_ctx[NKN_MAX_SSL_HOST] ;
BIO *bio_err=0;
static const char *pass;

static int s_server_session_id_context = 1;
const char * http_server_ip="127.0.0.1";
int http_server_port=80;
static int init_start = 0;

int ssl_listen_intfcnt = 0;
char ssl_listen_intfname[MAX_NKN_INTERFACE][16];
extern network_mgr_t * gnm;
int nkn_ssl_serverport[MAX_PORTLIST] ;

void ssl_interface_init(void);
void ssl_interface_deinit(void);

void nkn_setup_interface_parameter(int i);
int ssl_accept(ssl_con_t * ssl_con);

int ssl_exit_err(const char * string);
SSL_CTX * ssl_initialize_ctx(const  char * certfile, const char *keyfile, const char *password);
static DH * load_dh_params(const char * file);
static int ssl_password_cb(char *buf,int num, int rwflag, void *userdata);

SSL_CTX * ssl_server_init(const char *cert_name, const char *key_name, const char *passphrase, const char *servername, const char *ciphers);
void ssl_vhost_ssl_ctx_init(ssl_vhost_node_data_t *vhost);
void ssl_vhost_ssl_ctx_destroy(ssl_vhost_node_data_t *vhost);
SSL_CTX * ssl_get_valid_vhost(void);
void ssl_reinit_ssl_ctx_client_auth(void);

static int ssl_epollin(int fd, void *private_date);
static int ssl_epollout(int fd, void *private_data);
static int ssl_epollerr(int fd, void *private_data);
static int ssl_epollhup(int fd, void *private_data);
static int ssl_timeout(int fd, void * private_data, double timeout);

static int http_epollin(int fd, void *private_date);
static int http_epollout(int fd, void *private_data);
static int http_epollerr(int fd, void *private_data);
static int http_epollhup(int fd, void *private_data);
static int http_timeout(int fd, void * private_data, double timeout);

static int forward_request_to_nvsd(ssl_con_t * ssl_con);
static int forward_response_to_ssl(ssl_con_t * http_con);
void close_conn(int fd);
static unsigned char dh512_p[]={
        0xDA,0x58,0x3C,0x16,0xD9,0x85,0x22,0x89,0xD0,0xE4,0xAF,0x75,
        0x6F,0x4C,0xCA,0x92,0xDD,0x4B,0xE5,0x33,0xB8,0x04,0xFB,0x0F,
        0xED,0x94,0xEF,0x9C,0x8A,0x44,0x03,0xED,0x57,0x46,0x50,0xD3,
        0x69,0x99,0xDB,0x29,0xD7,0x76,0x27,0x6B,0xA2,0xD3,0xD4,0x12,
        0xE2,0x18,0xF4,0xDD,0x1E,0x08,0x4C,0xF6,0xD8,0x00,0x3E,0x7C,
        0x47,0x74,0xE8,0x33,
        };
static unsigned char dh512_g[]={
        0x02,
        };

static DH *get_dh512(void)
        {
        DH *dh=NULL;

        if ((dh=DH_new()) == NULL) return(NULL);
        dh->p=BN_bin2bn(dh512_p,sizeof(dh512_p),NULL);
        dh->g=BN_bin2bn(dh512_g,sizeof(dh512_g),NULL);
        if ((dh->p == NULL) || (dh->g == NULL))
                return(NULL);
        return(dh);
        }

static DH *load_dh_params(const char * file)
{
        DH *ret=0;
        BIO *bio;

        /* "openssl dhparam -out dh_param_1024.pem -2 1024" */
        /* "openssl dhparam -out dh_param_512.pem -2 512" */
        if( (bio=BIO_new_file(file, "r")) == NULL ) {
                DBG_LOG(MSG, MOD_SSL, "Couldn't open DH file %s\r\n", file);
                ERR_print_errors(bio_err);
                //exit(0);
		return NULL;
        }

        ret=PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
        BIO_free(bio);
	return ret;
}

static RSA *tmp_rsa_cb(SSL *s, int is_export, int keylength)
{
        BIGNUM *bn = NULL;
        static RSA *rsa_tmp=NULL;
	DBG_LOG(MSG, MOD_SSL, "Using temp RSA\n");
        if (!rsa_tmp && ((bn = BN_new()) == NULL))
                BIO_printf(bio_err,"Allocation error in generating RSA key\n");


        if (!rsa_tmp && bn) {
                if(!BN_set_word(bn, RSA_F4) || 
			((rsa_tmp = RSA_new()) == NULL) ||
                        !RSA_generate_key_ex(rsa_tmp, keylength, bn, NULL)) {
                        if(rsa_tmp) RSA_free(rsa_tmp);
                        rsa_tmp = NULL;
                }
                BN_free(bn);
        }
        return(rsa_tmp);
}

static int ssl_servername_cb(SSL *s, int *ad, void *arg)
{
	int i;
        tlsextctx * p = (tlsextctx *) arg;
	SSL_CTX *pctx = NULL;
	*ad = SSL_AD_HANDSHAKE_FAILURE;
        const char * servername = SSL_get_servername(s, TLSEXT_NAMETYPE_host_name);

	if(servername != NULL) {

		DBG_LOG(MSG, MOD_SSL, "Virtual Host name %s\n",servername); 
		for(i = 0; i< NKN_MAX_SSL_HOST; i++) 
		{
			if( (vhost_ssl_ctx[i].servername != NULL) &&
				(!strcmp(vhost_ssl_ctx[i].servername, servername))) 
			{
				pctx = vhost_ssl_ctx[i].ssl_ctx;

				if(pctx != NULL) {
					SSL_set_SSL_CTX(s, pctx);
					return SSL_TLSEXT_ERR_OK;
				} 
			}
			
		}
	}
	/* chek if wildcard certificate available*/
	pctx = default_ctx;

	if(pctx != NULL) {
		DBG_LOG(MSG, MOD_SSL, "Using Wildcard virtualhost\n");
		SSL_set_SSL_CTX(s, pctx);
		return SSL_TLSEXT_ERR_OK;
	}	

	DBG_LOG(MSG, MOD_SSL, "Could not resolve to any virtual Host\n");
        return SSL_TLSEXT_ERR_ALERT_FATAL;
}


/* The password code is not thread safe */
static int ssl_password_cb(char *buf,int num, int rwflag, void *userdata)
{
	UNUSED_ARGUMENT(rwflag);	
	UNUSED_ARGUMENT(userdata);	
	if(num<(int) strlen(pass)+1) {
		return(0);
	}

	strcpy(buf,pass);
	return(strlen(pass));
}

#ifdef HTTP2_SUPPORT
int http2_init() 
{
	int n;
	ssl_server_procs_t procs;
	int rv;

	memset(&procs, sizeof(procs), 0);

    	procs.ssl_epollin = ssl_epollin;
    	procs.ssl_epollout = ssl_epollout;
    	procs.ssl_epollerr = ssl_epollerr;
    	procs.ssl_epollhup = ssl_epollhup;
    	procs.ssl_timeout = ssl_timeout;
    	procs.close_conn = close_conn;
    	procs.forward_request_to_nvsd = forward_request_to_nvsd;

	ssl_server_proto_init();
	server_spdy_init(&procs, &server_spdy_procs);
	server_http2_init(&procs, &server_http2_procs);

	if (enable_http2) {
	    for (n = 0; http2_version_list[n].ver_str; n++) {
	    	add_ssl_next_proto_def(&ssl_next_proto_def, 
				       http2_version_list[n].ver_str);
	    }
	}

	if (enable_spdy) {
	    for (n = 0; spdy_version_list[n].ver_str; n++) {
	    	add_ssl_next_proto_def(&ssl_next_proto_def, 
				       spdy_version_list[n].ver_str);
	    }
	}

	rv = tokenize_proto_data(ssl_next_proto_def.vers, 
		    ssl_next_proto_def.vers_len,
		    ssl_next_proto_def.tk_proto,
		    (int)(sizeof(ssl_next_proto_def.tk_proto) /
			  sizeof(token_proto_data_t)),
		    &ssl_next_proto_def.tk_proto_entries);
	if (rv) {
	    DBG_LOG(MSG, MOD_SSL, 
		"http2_init(): tokenize_proto_data() failed rv=%d", rv);
	    return 1;
	}

	return 0; // Success
}
#endif /* HTTP2_SUPPORT */


void ssl_lib_init(void) 
{
	/*
	 * 1. Initialization SSL library.
	 */
	/* Global system initialization*/
	if(!init_start) {
		SSL_library_init();
		CRYPTO_malloc_init(); 
		ERR_load_crypto_strings();
		ERR_load_BIO_strings();

		OpenSSL_add_all_algorithms(); 
		SSL_load_error_strings();
		init_start++;
		bio_err=BIO_new_fp(stderr,BIO_NOCLOSE);
		DBG_LOG(MSG, MOD_SSL, "SSL library Init done\n");
	}

}


SSL_CTX * ssl_initialize_ctx(const char *certfile, const char * keyfile, const char * password)
{
#ifdef USE_SAMARA_SSL
	SSL_METHOD *meth;
#else
	const SSL_METHOD *meth;
#endif
	SSL_CTX * pctx;
    
      
	/* An error write ssl_ctxtext */

	/*
	 * 2. Create our ssl_context
	 */
	meth = SSLv23_server_method();
	pctx=SSL_CTX_new(meth);
	if(pctx == NULL)  {
		DBG_LOG(MSG, MOD_SSL, "Failed to create SSL_CTX , certficate %s key_file %s\n", 
							certfile, keyfile);
		return NULL;
	}

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

	if(!(SSL_CTX_use_PrivateKey_file(pctx, keyfile, SSL_FILETYPE_PEM))) {
		log_ssl_stack_errors();
		SSL_CTX_free(pctx);
		return NULL;
	}

	/*
	 * 3. Load the CAs we trust, root certificates
	 */
	if(!(SSL_CTX_load_verify_locations(pctx, NULL, CA_CERT_PATH))) {
		log_ssl_stack_errors();
	}
    
#ifdef HTTP2_SUPPORT
	SSL_CTX_set_next_protos_advertised_cb(pctx, next_proto_cb, 
					      &ssl_next_proto_def);
#if OPENSSL_VERSION_NUMBER >= 0x10002000L
    	SSL_CTX_set_alpn_select_cb(pctx, alpn_select_proto_cb, 
				   &ssl_next_proto_def);
#endif
#endif /* HTTP2_SUPPORT */

	return pctx;
}
     
static int sslsvr_epollin(int sockfd, void * private_data);
static int sslsvr_epollin(int sockfd, void * private_data)
{
        int clifd;
        int cnt;
        struct sockaddr_storage addr;
        socklen_t addrlen;
        nkn_interface_t * pns = (nkn_interface_t *)private_data;

        /* always returns TRUE for this case */

	for(cnt = 0 ; cnt < 100; cnt++) {
		addrlen=sizeof(struct sockaddr_storage);
		clifd = accept(sockfd, (struct sockaddr *)&addr, &addrlen);
		if (clifd == -1) {
			return TRUE;
		}
		DBG_LOG(MSG, MOD_SSL, "Accepted connection fd = %d\n", clifd);
		//nkn_mark_fd(clifd, NETWORK_FD);

		AO_fetch_and_add1(&glob_tot_ssl_sockets);

#if 1
		struct sockaddr_in *addrptr = (struct sockaddr_in *)&addr;
		if(addrptr->sin_addr.s_addr == 0x0100007F) {
			init_cli_conn(sockfd, clifd, pns, (struct sockaddr_storage *)&addr, 0);//gnm[sockfd].pthr->num);
		} else {
			init_conn(sockfd, clifd, pns, (struct sockaddr_storage *)&addr, 0);//gnm[sockfd].pthr->num);
		}
#endif 

	//	init_conn(sockfd, clifd, pns, (struct sockaddr_storage *)&addr, 0);//gnm[sockfd].pthr->num);
	}
        return TRUE;
}

/*
 * This function cleans up HTTP structure and ssl_con structure
 * It should be called only once for each socket at the time of socket close time
 */
void close_conn(int fd)
{
	int peer_fd;
	ssl_con_t * ssl_con, * peer_con;

	if (fd <= 0) return;
	ssl_con = (ssl_con_t *)gnm[fd].private_data;
	if (ssl_con == 0) return;
	peer_fd = ssl_con->peer_fd;

	DBG_LOG(MSG, MOD_SSL, "close_conn: errno=%d", errno);
        if (fd > 0) 
        {
		if (ssl_con->ssl) 
		{
			int r;

			/* close SSL client side socket */
			SSL_set_shutdown(ssl_con->ssl, SSL_SENT_SHUTDOWN|SSL_RECEIVED_SHUTDOWN);
#if 0
			r = SSL_shutdown(ssl_con->ssl);
			if(!r){
				/*
				 * If we called SSL_shutdown() first then
				 * we always get return value of '0'. In
				 * this case, try again, but first send a
				 * TCP FIN to trigger the other side's
				 * close_notify
				 */
				shutdown(ssl_con->fd, 1);
				r=SSL_shutdown(ssl_con->ssl);
			}
#endif
			SSL_free(ssl_con->ssl);
			ssl_con->ssl = NULL;
			AO_fetch_and_sub1(&glob_tot_ssl_ctx_cnt);
			AO_fetch_and_sub1(&glob_cur_open_ssl_sockets);
#ifdef HTTP2_SUPPORT
			server_close_conn(ssl_con);
#endif
		}

		NM_close_socket(fd);
		if(peer_fd > 0 ) {
			gnm[peer_fd].peer_fd = -1;
		}
		free(ssl_con);
		AO_fetch_and_sub1(&glob_tot_ssl_con_malloc_cnt);
        }

        if (peer_fd > 0)
        {
		peer_con = (ssl_con_t *)gnm[peer_fd].private_data;
		if(peer_con == NULL) { 
			return ;
		}
		if  (peer_con->ssl)
		{
			int r;

			/* close SSL client side socket */
			SSL_set_shutdown(peer_con->ssl, SSL_SENT_SHUTDOWN|SSL_RECEIVED_SHUTDOWN);
#if 0
			SSL_shutdown(peer_con->ssl);
			if(!r){
				/*
				 * If we called SSL_shutdown() first then
				 * we always get return value of '0'. In
				 * this case, try again, but first send a
				 * TCP FIN to trigger the other side's
				 * close_notify
				 */
				shutdown(peer_con->fd, 1);
				r=SSL_shutdown(peer_con->ssl);
			}
#endif
			SSL_free(peer_con->ssl);
			peer_con->ssl = NULL;
			AO_fetch_and_sub1(&glob_tot_ssl_ctx_cnt);
			AO_fetch_and_sub1(&glob_cur_open_ssl_sockets);
		}
		NM_close_socket(peer_fd);
		gnm[fd].peer_fd = -1;
		free(peer_con);
		AO_fetch_and_sub1(&glob_tot_ssl_con_malloc_cnt);
        }
}


/* ***************************************************
 * The following functions handles server side socket.
 * *************************************************** */

static ssl_con_t * create_new_svr_con(int cli_fd)
{
	int http_fd;
	ssl_con_t * http_con, * ssl_con;
	struct sockaddr_in srv;
	int ret;


	DBG_LOG(MSG, MOD_SSL, "create_new_http_con: called fd=%d", cli_fd);
	ssl_con = gnm[cli_fd].private_data;

	http_con = (ssl_con_t *)calloc(1, sizeof(ssl_con_t));
	if(!http_con) {
		return NULL;
	}
	AO_fetch_and_add1(&glob_tot_ssl_con_malloc_cnt);
	http_con->magic = CON_MAGIC_USED;
        http_con->fd = -1;
	http_con->pns = ssl_con->pns;
	
	//memcpy(&http_con->src_ipv4v6, &ssl_con->src_ipv4v6, sizeof(ip_addr_t));
	http_con->src_ipv4v6.family = ssl_con->src_ipv4v6.family ;	
	if(ssl_con->src_ipv4v6.family == AF_INET) {
		IPv4(http_con->src_ipv4v6) = IPv4(ssl_con->src_ipv4v6);
	} else {
		memcpy(&IPv6(http_con->src_ipv4v6), &IPv6(ssl_con->src_ipv4v6), sizeof(struct in6_addr));
	}
	http_con->src_port = ssl_con->src_port;
	http_con->dest_ipv4v6.family = AF_INET;
	IPv4(http_con->dest_ipv4v6) = inet_addr(http_server_ip);//IPv4(ssl_con->dest_ipv4v6);
	http_con->dest_port = http_server_port ? http_server_port : 80 ;

	DBG_LOG(MSG, MOD_SSL, "http_con=%p cli_fd=%d", http_con, cli_fd);

        if( (http_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                DBG_LOG(WARNING, MOD_SSL, "FP Failed to create fd, errno=%d.", errno);
		return NULL;
        }
	http_con->fd = http_fd;

	// Set to non-blocking socket. so it is non-blocking connect.
	NM_set_socket_nonblock(http_fd);

	if(register_NM_socket(http_con->fd,
                        (void *)http_con,
                        http_epollin,
                        http_epollout,
                        http_epollerr,
                        http_epollhup,
                        http_timeout,
                        ssl_idle_timeout,
			gnm[cli_fd].accepted_thr_num) == FALSE)
	{
		close(http_con->fd);
		AO_fetch_and_sub1(&glob_tot_ssl_con_malloc_cnt);
		free(http_con);
		return NULL;
	}

	/* Now time to make a socket connection. */
        bzero(&srv, sizeof(srv));
        srv.sin_family = AF_INET;
	srv.sin_addr.s_addr = IPv4(http_con->dest_ipv4v6);
        srv.sin_port = htons(http_con->dest_port);

	ret = connect(http_fd, (struct sockaddr *)&srv, sizeof(struct sockaddr));
	if(ret < 0)
	{
		if(errno == EINPROGRESS) {
			DBG_LOG(MSG, MOD_SSL, "connect(0x%x) fd=%d in progress", 
						IPv4(http_con->dest_ipv4v6), http_fd);
			if (NM_add_event_epollout(http_fd)==FALSE) {
				free(http_con);
				DBG_LOG(MSG, MOD_SSL, "failed to add epollout event fd = %d", http_con->fd); 
				AO_fetch_and_sub1(&glob_tot_ssl_con_malloc_cnt);
				NM_close_socket(http_fd);
				return NULL;
			}
			SET_CON_FLAG(http_con, CONF_SYN_SENT);
			gnm[http_con->fd].peer_fd = cli_fd;
			gnm[cli_fd].peer_fd = http_con->fd;
			ssl_con->peer_fd = http_fd;
        		http_con->peer_fd = cli_fd;
			return http_con;
		}
		DBG_LOG(MSG, MOD_SSL, "connect(0x%x) fd=%d failed, errno=%d", 
					IPv4(http_con->dest_ipv4v6), http_fd, errno);
		free(http_con);
		AO_fetch_and_sub1(&glob_tot_ssl_con_malloc_cnt);
		NM_close_socket(http_fd);
		return NULL;
	}

	SET_CON_FLAG(http_con, CONF_HTTP_READY);

	gnm[http_con->fd].peer_fd = cli_fd;
	gnm[cli_fd].peer_fd = http_con->fd;
	ssl_con->peer_fd = http_fd;
        http_con->peer_fd = cli_fd;
	/* sent data in case we have data there */
	ssl_con = (ssl_con_t *)gnm[http_con->peer_fd].private_data;
	if (forward_request_to_nvsd(ssl_con) == FALSE) {
		DBG_LOG(MSG, MOD_SSL, "forward_request_to_nvsd failed ssl_fd %d http_fd %d", 
					ssl_con->fd, http_con->fd);
		return NULL;
	}

	/*
	 * Socket has been established successfully by now.
	 */
	return http_con;
}

int ssl_accept(ssl_con_t * ssl_con)
{
        int r_code;
	int ret= 0 ;
        unsigned long es;
        //struct epoll_event ev;
	ssl_con_t * http_con;
	SSL_CTX *pctx_def = NULL;
	SSL_CTX *pctx = NULL;
	
	const char *servername = NULL;

        if (ssl_con->ssl == NULL)
        {
		pctx = ssl_get_valid_vhost();
		if(!pctx) {
			DBG_LOG(WARNING, MOD_SSL, "No valid Virtual Host Config fd=%d", ssl_con->fd);
			AO_fetch_and_add1(&glob_tot_cert_setup_err_cnt);
			AO_fetch_and_sub1(&glob_cur_open_ssl_sockets);
			close_conn(ssl_con->fd);
			return TRUE;
		}

                ssl_con->ssl=SSL_new(pctx);
		if(!ssl_con->ssl) {
			DBG_LOG(WARNING, MOD_SSL, "Error in setting new ssl ctx fd=%d", ssl_con->fd);
			AO_fetch_and_add1(&glob_tot_cert_setup_err_cnt);
			AO_fetch_and_sub1(&glob_cur_open_ssl_sockets);
			close_conn(ssl_con->fd);
			return TRUE;
		}
		AO_fetch_and_add1(&glob_tot_ssl_ctx_cnt);
//		pthread_mutex_unlock(&ssl_default_ctx_lock);
#ifdef HTTP2_SUPPORT
		ssl_con->sbio=BIO_ssld_new_socket(ssl_con->fd, BIO_NOCLOSE);
#else
                ssl_con->sbio=BIO_new_socket(ssl_con->fd, BIO_NOCLOSE);
#endif
                SSL_set_bio(ssl_con->ssl, ssl_con->sbio, ssl_con->sbio);
		/* Set SSL to work in server mode */
                SSL_set_accept_state(ssl_con->ssl);

        }

#ifdef HTTP2_SUPPORT
	r_code = process_client_hello(ssl_con, &ret);
	switch (r_code) {
	case PCH_RET_CONTINUE: // no action, proceed with SSL_accept()
	    break;

	case PCH_RET_NEED_READ:
	    NM_add_event_epollin(ssl_con->fd);
	    SET_CON_FLAG(ssl_con, CONF_SSL_ACCEPT);
	    return FALSE;

	case PCH_RET_NEED_WRITE:
	    NM_add_event_epollout(ssl_con->fd);
	    SET_CON_FLAG(ssl_con, CONF_SSL_ACCEPT);
	    return FALSE;

	case PCH_RET_CLOSE_CONN:
	    close_conn(ssl_con->fd);
	    return TRUE;

	case PCH_RET_TCP_TUNNEL:
	    assert(!"Implement TCP tunnel");
	    break;

	case PCH_RET_ERROR:
	    DBG_LOG(WARNING, MOD_SSL, 
	    	    "process_client_hello() error rv=%d", ret);
	    close_conn(ssl_con->fd);
	    return TRUE;

	default:
	    DBG_LOG(WARNING, MOD_SSL, 
	    	    "Invalid process_client_hello() return rv=%d, "
		    "abort request", r_code);
	    close_conn(ssl_con->fd);
	    return TRUE;
	}
#endif

        r_code = SSL_accept(ssl_con->ssl);
        if (r_code == 1) {
#ifdef HTTP2_SUPPORT
		ret = setup_http2_con(ssl_con);
		if (ret == 0) {
			re_register_socket(ssl_con, ssl_con->fd);
		} else if (ret > 0) {
			DBG_LOG(WARNING, MOD_SSL, 
				"setup_http2_con() failed, rv=%d", ret);
			close_conn(ssl_con->fd);
			return TRUE;
		} else {
			DBG_LOG(MSG, MOD_SSL, 
				"Unknown HTTP2 version, assume HTTP/1.x "
				"ssl_con=%p", ssl_con);
		}
#endif
		ssl_con->servername = SSL_get_servername(ssl_con->ssl, TLSEXT_NAMETYPE_host_name);
                DBG_LOG(MSG, MOD_SSL, "SSL socket connected");
		UNSET_CON_FLAG(ssl_con, CONF_SSL_ACCEPT);

		/*
		 * Set up peer socket.
		 */
		http_con = create_new_svr_con(ssl_con->fd);
		if(http_con == NULL) {
			AO_fetch_and_add1(&glob_tot_http_setup_err_cnt);
			DBG_LOG(WARNING, MOD_SSL, "failed to set up http context fd = R%d", ssl_con->fd);
			//unregister_NM_socket(ssl_con->fd);
			close_conn(ssl_con->fd);
			//free(ssl_con);
			return TRUE;
		}
		DBG_LOG(MSG, MOD_TUNNEL, "Tunnel %d <--> %d established",
			ssl_con->fd, http_con->fd);

		/*
		 * Setup the relation ship
		 */
		SET_CON_FLAG(ssl_con, CONF_SSL_READY);
		AO_fetch_and_add1(&glob_tot_handshake_done);

                return TRUE;
        }
	ssl_con->servername = SSL_get_servername(ssl_con->ssl, TLSEXT_NAMETYPE_host_name);
        ret = SSL_get_error(ssl_con->ssl, r_code);
        DBG_LOG(MSG, MOD_SSL, "ssl_accept: r_code=%d , ssl_get_error %d", r_code, ret);
        switch(ret)
        {
                case SSL_ERROR_WANT_WRITE:
			NM_add_event_epollout(ssl_con->fd);
                        break;
                case SSL_ERROR_WANT_READ:
			NM_add_event_epollin(ssl_con->fd);
                        break;
		case SSL_ERROR_SSL:
                case SSL_ERROR_SYSCALL:
                default:
			log_ssl_stack_errors();
			DBG_LOG(WARNING, MOD_SSL, "Virtual Host Name %s", ssl_con->servername);
 
			AO_fetch_and_add1(&glob_tot_handshake_failure);
			close_conn(ssl_con->fd);
			return TRUE;
        }
	SET_CON_FLAG(ssl_con, CONF_SSL_ACCEPT);
        return FALSE;
}

void init_conn(int svrfd, int ssl_fd, nkn_interface_t * pns, struct sockaddr_storage * addr, int thr_num)
{

	struct ssl_con_t *ssl_con ;
	struct sockaddr_storage svraddr;
        socklen_t addrlen = sizeof(struct sockaddr_in);

	/*
	 * Set up this client socket
	 */

	addrlen = sizeof(struct sockaddr_storage);
	getsockname(svrfd, (struct sockaddr *)&svraddr, &addrlen);
	if (NM_set_socket_nonblock(ssl_fd) == FALSE) {
		DBG_LOG(MSG, MOD_SSL,
				 "Failed to set socket %d to be nonblocking ssocket.", ssl_fd);
		close(ssl_fd);
		return;
	}

	ssl_con = (struct ssl_con_t *)calloc(sizeof(struct ssl_con_t ), 1);
	if(!ssl_con) {
		DBG_LOG(WARNING, MOD_SSL, "SSL con malloc failed\n");
		close(ssl_fd);
		return;
	}
	AO_fetch_and_add1(&glob_tot_ssl_con_malloc_cnt);
#if 0 
	if (!default_cert || !default_key) {
		close(ssl_fd);
		return ;
	}
#endif

	ssl_con->magic = CON_MAGIC_USED;
	ssl_con->fd = ssl_fd;
	ssl_con->peer_fd = -1;
	ssl_con->pns = pns;
	if(addr->ss_family == AF_INET) {
		struct sockaddr_in *addrptr = (struct sockaddr_in *)addr;;
		IPv4(ssl_con->src_ipv4v6) = addrptr->sin_addr.s_addr;
		ssl_con->src_ipv4v6.family = AF_INET;
		ssl_con->src_port = addrptr->sin_port;
	} else {
		struct sockaddr_in6 *addrptrv6 = (struct sockaddr_in6*)addr;

		ssl_con->src_ipv4v6.family = AF_INET6;
		ssl_con->src_port = addrptrv6->sin6_port;
		memcpy(&IPv6(ssl_con->src_ipv4v6), &addrptrv6->sin6_addr , sizeof(struct in6_addr));
		
	}
	if(svraddr.ss_family == AF_INET) {
		struct sockaddr_in *addrptr = (struct sockaddr_in *)&svraddr;;
		ssl_con->dest_ipv4v6.family = AF_INET;
		IPv4(ssl_con->dest_ipv4v6) = addrptr->sin_addr.s_addr; //inet_addr(http_server_ip);
		ssl_con->dest_port = addrptr->sin_port; //http_server_port;
	} else if(svraddr.ss_family == AF_INET6) {
		struct sockaddr_in6 *addrptrv6 = (struct sockaddr_in6*)addr;
		ssl_con->dest_ipv4v6.family = AF_INET6;
		ssl_con->dest_port = addrptrv6->sin6_port;
		memcpy(&IPv6(ssl_con->dest_ipv4v6), &addrptrv6->sin6_addr, sizeof(struct in6_addr));
		
	}
	SET_CON_FLAG(ssl_con, CONF_CLIENT_SIDE);

	DBG_LOG(MSG, MOD_SSL, "ssl_con =%p fd=%d", ssl_con, ssl_fd);
	SET_CON_FLAG(ssl_con, CONF_SSL);

	if(register_NM_socket(ssl_fd,
				(void *)ssl_con,
				ssl_epollin,
				ssl_epollout,
				ssl_epollerr,
				ssl_epollhup,
				ssl_timeout,
				ssl_idle_timeout,
				-1) == FALSE)
	{
		DBG_LOG(MSG, MOD_SSL, "SSL Registration error");
		free(ssl_con);
		AO_fetch_and_sub1(&glob_tot_ssl_con_malloc_cnt);
		close(ssl_fd);
		return; 
	}

	AO_fetch_and_add1(&glob_cur_open_ssl_sockets);
	SET_CON_FLAG(ssl_con, CONF_SSL_ACCEPT);
	NM_add_event_epollin(ssl_con->fd);
	//ssl_accept(ssl_con);
	return;
}


/*
 * ===============> server socket <===============================
 * The following functions are designed for server socket.
 * we need to provde epollin/epollout/epollerr/epollhup functions
 */

static int sslsvr_epollout(int sockfd, void * private_data)
{
	UNUSED_ARGUMENT(private_data);

	DBG_LOG(SEVERE, MOD_SSL, "epollout should never called. sockid=%d", sockfd);
	assert(0); // should never called
	return FALSE;
}

static int sslsvr_epollerr(int sockfd, void * private_data)
{
	UNUSED_ARGUMENT(private_data);

	DBG_LOG(SEVERE, MOD_SSL, "epollerr should never called. sockid=%d", sockfd);
	//assert(0); // should never called
	return FALSE;
}

static int sslsvr_epollhup(int sockfd, void * private_data)
{
	DBG_LOG(SEVERE, MOD_SSL, "epollhup should never called. sockid=%d", sockfd);
	return sslsvr_epollerr(sockfd, private_data);
}

// should be called with ssl_interface_lock mutex locked.
static int ssl_if6_init(nkn_interface_t *pns) {
    struct sockaddr_in6 srv_v6;
    int j;
    uint32_t ipv6_cnt;
    int val, ret;
    int listenfdv6;
    network_mgr_t *pnm = NULL;

    if(!pns) {
        return -1;
    }

    for(ipv6_cnt = 0; ipv6_cnt < pns->ifv6_ip_cnt; ipv6_cnt++) {
        for(j = 0; j < MAX_PORTLIST; j++) {
            if(0 == nkn_ssl_serverport[j]) {
                continue;
            }

            if ((listenfdv6 = socket(AF_INET6, SOCK_STREAM, 0)) < 0) {
                DBG_LOG(SEVERE, MOD_SSL, "Failed to create IPv6 socket. errno = %d", errno);
                continue;
            }
            val = 1;
            ret = setsockopt(listenfdv6, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
            if (-1 == ret) {
                DBG_LOG(SEVERE, MOD_SSL, "Failed to set ipv6 socket options. errno = %d", errno);
                close(listenfdv6);
                continue;
            }

            memset(&srv_v6, 0, sizeof(srv_v6));
            srv_v6.sin6_family = AF_INET6;
            memcpy(&srv_v6.sin6_addr, &pns->if_addrv6[ipv6_cnt].if_addrv6, sizeof(struct in6_addr));
            srv_v6.sin6_port = htons(nkn_ssl_serverport[j]);

            if (bind(listenfdv6, (struct sockaddr *)&srv_v6, sizeof(srv_v6)) < 0) {
                DBG_LOG(SEVERE, MOD_SSL, "Failed to bind ipv6 socket to port %d (errno=%d)", nkn_ssl_serverport[j],
                        errno);
                close(listenfdv6);
                continue;
            }
            DBG_LOG(SEVERE, MOD_SSL, "SSLD: [ifcnt: %d, j: %d] bind to server socket succ to port %d, listenfdv6: %d",
            ipv6_cnt, j, ntohs(srv_v6.sin6_port), listenfdv6);

            // We will use non-blocking accept listen socket
            NM_set_socket_nonblock(listenfdv6);

            // Bind this socket into this accepted interface
            NM_bind_socket_to_interface(listenfdv6, pns->if_name);

            listen(listenfdv6, 10000);
            pns->if_addrv6[ipv6_cnt].port[j] = nkn_ssl_serverport[j];
            pns->if_addrv6[ipv6_cnt].listenfdv6[j] = listenfdv6;

            if (register_NM_socket(listenfdv6,
                pns,
                sslsvr_epollin,
                sslsvr_epollout,
                sslsvr_epollerr,
                sslsvr_epollhup,
                NULL,
                0,
                -1) == FALSE) {
                close(listenfdv6);
                continue;
            }
            NM_add_event_epollin(listenfdv6);
            pnm = &gnm[listenfdv6];
            gnm[listenfdv6].accepted_thr_num = gnm[listenfdv6].pthr->num;
            NM_SET_FLAG(pnm, NMF_IS_IPV6);
        }
    }

    return 0;
}

void ssl_interface_init(void)
{
	struct sockaddr_in srv;
	int ret;
	int val;
	int i, j;
	int listenfd;
	int listen_all = 1;
        struct sockaddr_in dip;
        socklen_t dlen;
	network_mgr_t * pnm;



	/* Check if any interface is enabled else listen on all
	 * interface 
	 */

   pthread_mutex_lock(&ssl_interface_lock);

   if(ssl_license_enable == FALSE)  {
        pthread_mutex_unlock(&ssl_interface_lock);
        return;
   }


   for(i=0; i<ssl_listen_intfcnt; i++)
   {
        for(j=0; j< glob_tot_interface; j++)
        {
                if(strcmp(interface[j].if_name, ssl_listen_intfname[i]) == 0)
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
                      ( strcmp(interface[i].if_name, "lo") !=0 )) {
              continue;
	}
	for (j=0; j<MAX_PORTLIST; j++)
	{
	  if(nkn_ssl_serverport[j] == 0) {
		continue;
	  }

        if( (listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
		DBG_LOG(SEVERE, MOD_SSL, "failed to create a socket errno=%d", errno);
		continue;
        }

        bzero(&srv, sizeof(srv));
        srv.sin_family = AF_INET;
        srv.sin_addr.s_addr = interface[i].if_addrv4;
        srv.sin_port = htons(nkn_ssl_serverport[j]);

	val = 1;
        ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
        if (ret == -1) {
		DBG_LOG(SEVERE, MOD_SSL, "failed to set socket option errno=%d", errno);
		close(listenfd);
		continue;
        }

        val = 1;

        if( bind(listenfd, (struct sockaddr *) &srv, sizeof(srv)) < 0)
        {
		DBG_LOG(SEVERE, MOD_SSL, 
			"failed to bind server socket to port %d (errno=%d)", 
			nkn_ssl_serverport[j], errno);
		close(listenfd);
		continue;
        }

#if 0
       /*
        *  Code added to workaround a bug in current bind() implementation,
        *  which does not return a -1 on a second bind on the same port.
        */
        if ((getsockname(listenfd, (__SOCKADDR_ARG)&dip, &dlen) == -1)
            || (srv.sin_port != dip.sin_port)) {
            DBG_LOG(SEVERE, MOD_HTTP,
                            "SSL:[i:%d, j:%d] Socket bound to an ephermeral port."
                            " No need to use that. Requested port:%d, bound to:%d",
                            i, j, nkn_ssl_serverport[j], ntohs(dip.sin_port));
            close(listenfd);
            continue;
        }
#endif
        DBG_LOG(SEVERE, MOD_SSL, "SSLD:[i:%d, j:%d] bind to server socket succ. to port %d (errno=%d), listenfd:%d",
                                i, j, ntohs(srv.sin_port), errno, listenfd);

	// We will use nonblocking accept listen socket
	NM_set_socket_nonblock(listenfd);

	// Bind this socket into this accepted interface 
	NM_bind_socket_to_interface(listenfd, interface[i].if_name);

	listen(listenfd, 10000);

	interface[i].port[j] = nkn_ssl_serverport[j];
	interface[i].listenfd[j] = listenfd;

	if(register_NM_socket(listenfd, 
			&interface[i],
			sslsvr_epollin,
			sslsvr_epollout,
			sslsvr_epollerr,
			sslsvr_epollhup,
			NULL,
			0,
			-1) == FALSE)
	{
		// will this case ever happen at all?
		interface[i].listenfd[j] = -1;
		close(listenfd);
		continue;
	}
	NM_add_event_epollin(listenfd);

	gnm[listenfd].accepted_thr_num = gnm[listenfd].pthr->num;
	}
    ssl_if6_init(&interface[i]);
   }
   pthread_mutex_unlock(&ssl_interface_lock);

}

void ssl_interface_deinit(void)
{
        int i = 0;
        int j = 0;
        int fd = 0 ;
        network_mgr_t *pnm = NULL;
        if(ssl_license_enable == TRUE) {
                return;
        }
        pthread_mutex_lock(&ssl_interface_lock);
        for(i=0; i < glob_tot_interface; i++) {
                for(j=0;j<MAX_PORTLIST; j++) {
                        fd = interface[i].listenfd[j];
			if(fd<= 0) continue;
                        pnm = &gnm[fd];
                        pthread_mutex_lock(&pnm->mutex);
                        NM_close_socket(fd);
                        pthread_mutex_unlock(&pnm->mutex);
                }
        }
        pthread_mutex_unlock(&ssl_interface_lock);
}

#if 1
void ssl_reinit_ssl_ctx_client_auth(void)
{
        int i = 0;
        for(i = 0 ; i < NKN_MAX_SSL_HOST; i++)
        {
                if(vhost_ssl_ctx[i].servername != NULL) {
                        pthread_mutex_lock(&vhost_ssl_ctx[i].ctx_lock);
			if(ssl_client_auth ==  1 ) {
				SSL_CTX_set_verify(vhost_ssl_ctx[i].ssl_ctx,
				   SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
				   0);
			
			} else
			{
				SSL_CTX_set_verify(vhost_ssl_ctx[i].ssl_ctx, SSL_VERIFY_NONE, 0);
			}
                        pthread_mutex_unlock(&vhost_ssl_ctx[i].ctx_lock);

                        return;
                }
        }
}
#endif
void ssl_vhost_ssl_ctx_destroy(ssl_vhost_node_data_t *vhost)
{
	int i;
	ssl_cert_node_data_t *cert_data;
	if (vhost == NULL || vhost->name == NULL)
                return;

        cert_data = vhost->p_ssl_cert;

	for(i = 0 ; i < NKN_MAX_SSL_HOST; i++)
        {
                if((vhost_ssl_ctx[i].servername != NULL)&& 
		   (strcmp(vhost_ssl_ctx[i].servername ,vhost->name)==0)) {
			DBG_LOG(MSG, MOD_SSL, "Removed virtual host %s", vhost->name);
			pthread_mutex_lock(&vhost_ssl_ctx[i].ctx_lock);
			if(strcmp(vhost_ssl_ctx[i].servername, "wildcard") == 0 ) {
				pthread_mutex_lock(&ssl_default_ctx_lock);
				default_cipher = NULL; 
				default_key = NULL; 
				default_cert = NULL;
				default_pass = NULL;
				default_ctx = NULL;
				pthread_mutex_unlock(&ssl_default_ctx_lock);
			}
			free(vhost_ssl_ctx[i].servername);
			free(vhost_ssl_ctx[i].cert_name);
			free(vhost_ssl_ctx[i].key_name);
			free(vhost_ssl_ctx[i].ciphers);
			free(vhost_ssl_ctx[i].passphrase);

 			vhost_ssl_ctx[i].servername = NULL;
 			vhost_ssl_ctx[i].cert_name = NULL;
 			vhost_ssl_ctx[i].key_name = NULL;
 			vhost_ssl_ctx[i].ciphers = NULL;
 			vhost_ssl_ctx[i].passphrase = NULL;

			if(vhost_ssl_ctx[i].ssl_ctx) {
				SSL_CTX_free(vhost_ssl_ctx[i].ssl_ctx);
				vhost_ssl_ctx[i].ssl_ctx = NULL;
			}
			pthread_mutex_unlock(&vhost_ssl_ctx[i].ctx_lock);
			AO_fetch_and_sub1(&glob_tot_vhost_cert_ctx_cnt);
                        return;
                }
        }

}

SSL_CTX * ssl_get_valid_vhost(void)
{
	int i = 0 ;
	SSL_CTX *pctx;
	for ( i = 0 ; i < NKN_MAX_SSL_HOST; i++) {
		if(vhost_ssl_ctx[i].ssl_ctx != NULL) {
		pctx = vhost_ssl_ctx[i].ssl_ctx;

			if(!pctx) {
				continue;
			}

			return pctx;
		}
	}
	return NULL;

}
void ssl_vhost_ssl_ctx_init(ssl_vhost_node_data_t *vhost)
{
	int i = 0;
	ssl_cert_node_data_t *cert_data;	
	ssl_key_node_data_t *key_data;
	if (vhost == NULL || vhost->name == NULL)
		return;

	cert_data = vhost->p_ssl_cert;
	key_data = vhost->p_ssl_key;

	for(i = 0 ; i < NKN_MAX_SSL_HOST; i++) 
	{
		if(vhost_ssl_ctx[i].ssl_ctx == NULL) {
		 	pthread_mutex_lock(&vhost_ssl_ctx[i].ctx_lock);
			if(key_data->key_name)
				vhost_ssl_ctx[i].key_name = strdup(key_data->key_name);

			if(cert_data->cert_name)
				vhost_ssl_ctx[i].cert_name = strdup(cert_data->cert_name);


			if(key_data->passphrase)
				vhost_ssl_ctx[i].passphrase = strdup(key_data->passphrase);

			if(vhost->name)
				vhost_ssl_ctx[i].servername = strdup(vhost->name);

			if(vhost->cipher)
				vhost_ssl_ctx[i].ciphers = strdup(vhost->cipher);


			vhost_ssl_ctx[i].ssl_ctx = ssl_server_init(
				vhost_ssl_ctx[i].cert_name,
				vhost_ssl_ctx[i].key_name, 
				vhost_ssl_ctx[i].passphrase, 
				vhost_ssl_ctx[i].servername, 
				vhost_ssl_ctx[i].ciphers);
			if(strcmp(vhost_ssl_ctx[i].servername, "wildcard") == 0 ) {
				pthread_mutex_lock(&ssl_default_ctx_lock);
				default_ctx = vhost_ssl_ctx[i].ssl_ctx;
				default_cipher = vhost_ssl_ctx[i].ciphers;
				default_key = vhost_ssl_ctx[i].key_name;
				default_cert = vhost_ssl_ctx[i].cert_name;
				default_pass = vhost_ssl_ctx[i].passphrase ;
				pthread_mutex_unlock(&ssl_default_ctx_lock);
			}
			AO_fetch_and_add1(&glob_tot_vhost_cert_ctx_cnt);
			pthread_mutex_unlock(&vhost_ssl_ctx[i].ctx_lock);
			return;
		}
	}

}
SSL_CTX * ssl_server_init(const char *cert_name, const char *key_name, const char *passphrase, const char *servername, const char *ciphers)
{
	SSL_CTX *ssl_ctx;
	EC_KEY *ecdh=NULL;
	DH *dh = NULL; 

	tlsextctx tlsextcbp = {NULL, NULL, SSL_TLSEXT_ERR_ALERT_WARNING};

	if (cert_name != NULL && key_name != NULL && 
			passphrase != NULL) 
	{
		ssl_ctx = ssl_initialize_ctx(cert_name, key_name, passphrase);
	} else {
	
        	DBG_LOG(MSG, MOD_SSL, "Couldn't set certificate context");
		return NULL;
	}
	if(ssl_ctx == NULL) {
		return NULL;
	}
	if (dhfile && (dhfile[0]!=0)) {
		dh = load_dh_params(dhfile);
	} 
	else if(cert_name) {
		dh = load_dh_params(cert_name);
	}
	

	if(dh == NULL)
		dh = get_dh512();

	if(dh != NULL) { 
        	SSL_CTX_set_tmp_dh(ssl_ctx, dh);
		DH_free(dh);
	}


	ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
	if(ecdh == NULL)
	{
		DBG_LOG(MOD_SSL, WARNING, "unable to create curve (nistp256)");
	} else {
		 SSL_CTX_set_tmp_ecdh(ssl_ctx, ecdh);
  	 	 EC_KEY_free(ecdh);
	}

	SSL_CTX_set_tmp_rsa_callback(ssl_ctx, tmp_rsa_cb);

	SSL_CTX_set_session_id_context(ssl_ctx,
		(void*)&s_server_session_id_context,
		sizeof(s_server_session_id_context)); 
   
	/* 2. Set our cipher list */
	if(ciphers && (ciphers[0]!=0)){
		if(!SSL_CTX_set_cipher_list(ssl_ctx, ciphers)) 
		{
			DBG_LOG(MSG, MOD_SSL, "Couldn't set Cipher set");
			ERR_print_errors(bio_err);
			return ssl_ctx;
		}
	} else {
		if(!SSL_CTX_set_cipher_list(ssl_ctx, "DEFAULT")) 
		{
			DBG_LOG(MSG, MOD_SSL, "Couldn't set Cipher set");
			ERR_print_errors(bio_err);
			return ssl_ctx;
		}
	}
	/* 3. Check supported client authentication */
	if(ssl_client_auth == 1) {
		SSL_CTX_set_verify(ssl_ctx,
				   SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
				   0);

	}

	tlsextcbp.biodebug = bio_err;
	if(servername != NULL && strlen(servername) != 0) {
		tlsextcbp.servername =  servername; 
	}
	
        SSL_CTX_set_tlsext_servername_callback(ssl_ctx, ssl_servername_cb);
        SSL_CTX_set_tlsext_servername_arg(ssl_ctx, &tlsextcbp);

    
	return ssl_ctx;
}


/* **********************************************************
 * Callback functions from epoll_wait loop.
 * ***********************************************************/
static int ssl_epollin(int fd, void *private_data)
{
	ssl_con_t * ssl_con = (ssl_con_t *)private_data;
	int rlen;
	int ret;

	DBG_LOG(MSG, MOD_SSL, "fd=%d", fd);
	if (CHECK_CON_FLAG(ssl_con, CONF_SSL_ACCEPT)) {
		if (ssl_accept(ssl_con) == FALSE) {
			DBG_LOG(MSG, MOD_SSL, "fd=%d ssl_accept returns FALSE", fd);
			return TRUE;
		}
			DBG_LOG(MSG, MOD_SSL, "fd=%d ssl_accept returns TRUE", fd);
		return TRUE;
	}

	if(!CHECK_CON_FLAG(ssl_con, CONF_SSL_READY))  {
		return TRUE;
	}

	rlen = MAX_CB_BUF - ssl_con->cb_totlen ;
	if(rlen == 0) {
		/* No more space */
		if (NM_del_event_epoll(fd)==FALSE) {
			close_conn(fd);
		}
		return TRUE;
	}
        ret = SSL_read(ssl_con->ssl, &ssl_con->cb_buf[ssl_con->cb_totlen], rlen);
	DBG_LOG(MSG, MOD_SSL, "fd=%d recv=%d bytes", fd, ret);
	if (ret>0) {
                DBG_LOG(MSG, MOD_SSL, "SSL_ERROR_NONE");
                AO_fetch_and_add(&glob_tot_size_from_ssl , ret);
                ssl_con->cb_totlen += ret;
                //ssl_con->cb_buf[ssl_con->cb_totlen]=0;
                //DBG_LOG(MSG, MOD_SSL, "ret=%d res=%s", ret, ssl_con->cb_buf);

                if (forward_request_to_nvsd(ssl_con) == FALSE) {
                        close_conn(fd);
                        return TRUE;
                }

                return TRUE;
        }


	ret = SSL_get_error(ssl_con->ssl, ret);
	DBG_LOG(MSG, MOD_SSL, "SSL_get_error ret=%d", ret);
	switch(ret) {
		case SSL_ERROR_ZERO_RETURN:
			// Peer close the SSL socket
			close_conn(fd);
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
			close_conn(fd);
			return TRUE;
	}

	return TRUE;
}

static int ssl_epollout(int fd, void *private_data)
{
	ssl_con_t * ssl_con = (ssl_con_t *)private_data;
	ssl_con_t * http_con;

	DBG_LOG(MSG, MOD_SSL, "fd=%d called", fd);
	UNUSED_ARGUMENT(fd);

	http_con = (ssl_con_t *)gnm[ssl_con->peer_fd].private_data;
	if (forward_response_to_ssl(http_con) == FALSE) {
		close_conn(fd);
		return TRUE;
	}

	return TRUE;
}
static int ssl_epollerr(int fd, void *private_data)
{
	DBG_LOG(MSG, MOD_SSL, "fd=%d called", fd);
	UNUSED_ARGUMENT(private_data);
	
	ssl_con_t *ssl_con = (ssl_con_t *)private_data;
	if(!ssl_con->ssl) {
		AO_fetch_and_sub1(&glob_cur_open_ssl_sockets);
	}
	close_conn(fd);
	return TRUE;
}

static int ssl_epollhup(int fd, void *private_data)
{
	DBG_LOG(MSG, MOD_SSL, "fd=%d called", fd);
	return ssl_epollerr(fd, private_data);
}

static int ssl_timeout(int fd, void * private_data, double timeout)
{
	int ret = TRUE;
	ssl_con_t *ssl_con = (ssl_con_t *)private_data;
	unsigned int incarn;
	DBG_LOG(MSG, MOD_SSL, "fd=%d called", fd);
	UNUSED_ARGUMENT(timeout);
	AO_fetch_and_add1(&glob_tot_ssl_timeout);
	if (gnm[fd].peer_fd > 0 ) {
		network_mgr_t *pnm = &gnm[ssl_con->peer_fd];
		incarn = pnm->incarn;
		pthread_mutex_lock(&pnm->mutex);
		if(incarn == pnm->incarn) {
			ret = ssl_epollerr(fd, private_data);
		}
		pthread_mutex_unlock(&pnm->mutex);
	} else if (CHECK_CON_FLAG(ssl_con, CONF_SSL_ACCEPT)) {
		// Peer fd is zero when handshake in progress
		ret = ssl_epollerr(fd, private_data);
	}
	return ret;
}

static int forward_response_to_ssl(ssl_con_t * http_con)
{
	ssl_con_t * ssl_con;
	int ret, len;
	char * p;
	DBG_LOG(MSG, MOD_SSL, "fd=%d called", http_con->fd);

	ssl_con = gnm[http_con->peer_fd].private_data;
	NM_set_socket_active(&gnm[ssl_con->fd]);

#ifdef HTTP2_SUPPORT
	switch (CHECK_CON_FLAG(ssl_con, (CONF_HTTP2 | CONF_SPDY3_1))) {
	case CONF_HTTP2:
	    return server_http2_procs.forward_response_to_ssl(http_con);
	    break;
	case CONF_SPDY3_1:
	    return server_spdy_procs.forward_response_to_ssl(http_con);
	    break;
	default:
	    break;
	}
#endif

	len = http_con->cb_totlen - http_con->cb_offsetlen;
	while(len) {

		p = &http_con->cb_buf[http_con->cb_offsetlen];
		ret = SSL_write(ssl_con->ssl, p, len);
		if (ret>0) {
			http_con->cb_offsetlen += ret;
			http_con->sent_len += ret;
			len -= ret;
			AO_fetch_and_add(&glob_tot_size_sent_ssl, ret);
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
			log_ssl_stack_errors();
			close_conn(http_con->fd);
			return FALSE;
		}
	}
	if(CHECK_CON_FLAG(http_con, CONF_HTTP_CLOSE)) {
                close_conn(http_con->fd);
		return TRUE;
	}
        if (NM_add_event_epollin(ssl_con->fd)==FALSE) {
                close_conn(http_con->fd);
                return TRUE;
        }
        if (NM_add_event_epollin(http_con->fd)==FALSE) {
                close_conn(http_con->fd);
                return TRUE;
        }
	http_con->cb_offsetlen = 0;
	http_con->cb_totlen = 0;
	return TRUE;
}

static int forward_request_to_nvsd(ssl_con_t * ssl_con)
{
	ssl_con_t * http_con;
	int ret, len;
	char * p;
	DBG_LOG(MSG, MOD_SSL, "fd=%d called", ssl_con->fd);

	http_con = gnm[ssl_con->peer_fd].private_data;
	NM_set_socket_active(&gnm[http_con->fd]);

send_again:
	len = ssl_con->cb_totlen - ssl_con->cb_offsetlen;
#if 1
	if(!ssl_con->ssl_hdr_sent) {
		
		ssl_con_hdr_t *hdr = (ssl_con_hdr_t *)malloc(sizeof(ssl_con_hdr_t));
		if(!hdr) {
			DBG_LOG(WARNING, MOD_SSL, "SSL header Malloc failed\n");
			return FALSE;
		}
		AO_fetch_and_add1(&glob_tot_ssl_hdr_malloc_cnt);
		int hdr_len = sizeof(ssl_con_hdr_t);
		int vhost_len = 0;
		hdr->magic = HTTPS_REQ_IDENTIFIER;
		memcpy(&hdr->src_ipv4v6, &ssl_con->src_ipv4v6, sizeof(ip_addr_t)); 
		memcpy(&hdr->dest_ipv4v6, &ssl_con->dest_ipv4v6, sizeof(ip_addr_t));;
		hdr->src_port = ssl_con->src_port;
		hdr->dest_port = ssl_con->dest_port;

		if(ssl_con->servername != NULL) {
			vhost_len = strlen(ssl_con->servername) ;
			if(vhost_len > 0 && 
				vhost_len < MAX_VIRTUAL_DOMAIN_LEN) {
		 
				strncpy(hdr->virtual_domain ,
					ssl_con->servername, 
					vhost_len);
			}
		}

		while(hdr_len) {
			ret = send(http_con->fd, hdr, hdr_len, 0);
			if( ret==-1 ) {
				DBG_LOG(MSG, MOD_SSL, "forward_request_to_nvsd: ERROR ret=-1 errno=%d", errno);
				if(errno == EAGAIN) {
					NM_add_event_epollout(http_con->fd);
					NM_del_event_epoll(ssl_con->fd);
					return TRUE;
				}
				AO_fetch_and_sub1(&glob_tot_ssl_hdr_malloc_cnt);
				free(hdr);
				return FALSE;
			}	 
			hdr_len -= ret;
		}

		ssl_con->ssl_hdr_sent = 1;
		AO_fetch_and_sub1(&glob_tot_ssl_hdr_malloc_cnt);
		free(hdr);
	}
#endif
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
			return FALSE;
		} 
		ssl_con->cb_offsetlen += ret;
		ssl_con->sent_len += ret;
		len -= ret;
		AO_fetch_and_add(&glob_tot_size_http_sent, ret);
	}

	ssl_con->cb_offsetlen = 0;
	ssl_con->cb_totlen = 0;

	if(SSL_pending(ssl_con->ssl)) {
		int rlen = MAX_CB_BUF - ssl_con->cb_totlen ;
        	ret = SSL_read(ssl_con->ssl, &ssl_con->cb_buf[ssl_con->cb_totlen], rlen);
		DBG_LOG(MSG, MOD_SSL, "fd=%d recv=%d bytes", ssl_con->fd, ret);
		if (ret>0) {
	                AO_fetch_and_add(&glob_tot_size_from_ssl, ret);
        	        ssl_con->cb_totlen += ret;
                	goto send_again;
                }

		ret = SSL_get_error(ssl_con->ssl, ret);
		DBG_LOG(MSG, MOD_SSL, "SSL_get_error ret=%d", ret);
		switch(ret) {
			case SSL_ERROR_ZERO_RETURN:
				// Peer close the SSL socket
				close_conn(ssl_con->fd);
				return TRUE;
			case SSL_ERROR_WANT_WRITE:
				NM_add_event_epollout(ssl_con->fd);
				break;
			case SSL_ERROR_WANT_READ:
				NM_add_event_epollin(ssl_con->fd);
				break;
			case SSL_ERROR_SYSCALL:
			default:
				log_ssl_stack_errors();
				DBG_LOG(MSG, MOD_SSL, "r_code=%d", ret);
				close_conn(ssl_con->fd);
				return TRUE;
		}

		return TRUE;
        }

	if (NM_add_event_epollin(ssl_con->fd)==FALSE) {
		close_conn(ssl_con->fd);
		return TRUE;
	}
	if (NM_add_event_epollin(http_con->fd)==FALSE) {
		close_conn(ssl_con->fd);
		return TRUE;
	}

	DBG_LOG(MSG, MOD_SSL, "forward_request_to_nvsd: send complete");
	return TRUE;
}


/* ***************************************************
 * The following functions handles client side socket.
 * *************************************************** */

static int http_epollin(int fd, void * private_data)
{
	ssl_con_t * http_con = (ssl_con_t *)private_data;
	int rlen;
	int ret;

	DBG_LOG(MSG, MOD_SSL, "fd=%d", fd);
	rlen = MAX_CB_BUF - http_con->cb_totlen;
	if(rlen == 0) {
		/* No more space */
		if (NM_del_event_epoll(fd)==FALSE) {
			close_conn(fd);
		}
		return TRUE;
	}
	ret = recv(fd, &http_con->cb_buf[http_con->cb_totlen], rlen, 0);
	DBG_LOG(MSG, MOD_SSL, "fd=%d recv=%d", fd, ret);

	if(ret < 0) {
		if ((ret==-1) && (errno==EAGAIN)) {
			return TRUE;
		}
		close_conn(fd);
		return TRUE;
	} else if(ret == 0 ) {
		/* close httpcon->fd after sending the remaining data */
		SET_CON_FLAG(http_con, CONF_HTTP_CLOSE);

	}
	http_con->cb_totlen += ret;
	AO_fetch_and_add(&glob_tot_size_http_received ,ret);

        if (forward_response_to_ssl(http_con) == FALSE) {
                close_conn(fd);
                return TRUE;
        }

	return TRUE;
}

static int http_epollout(int fd, void * private_data)
{
	ssl_con_t * http_con = (ssl_con_t *)private_data;
	ssl_con_t * ssl_con;
	int ret;
	int retlen;

	DBG_LOG(MSG, MOD_SSL, "http_epollout: fd=%d", fd);
	if (CHECK_CON_FLAG(http_con, CONF_SYN_SENT)) {

		UNSET_CON_FLAG(http_con, CONF_SYN_SENT);
		retlen=sizeof(ret);
		if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *)&ret, 
		       (socklen_t *)&retlen)) {
			DBG_LOG(MSG, MOD_SSL, "getsockopt(fd=%d) failed, errno=%d", fd, errno);
			close_conn(fd);
			return TRUE;
		}
		if(ret) {
			DBG_LOG(MSG, MOD_SSL, "connect(fd=%d) failed, ret=%d", fd, ret);
			close_conn(fd);
			return TRUE;
		}

		SET_CON_FLAG(http_con, CONF_HTTP_READY);
		DBG_LOG(MSG, MOD_SSL, "fd=%d connection established", fd);
		// Fall through in case we have already some data
	}

        /* sent data */
        ssl_con = (ssl_con_t *)gnm[http_con->peer_fd].private_data;
        if (forward_request_to_nvsd(ssl_con) == FALSE) {
                close_conn(fd);
                return TRUE;
        }

	return TRUE;
}

static int http_epollerr(int fd, void * private_data)
{
	DBG_LOG(MSG, MOD_SSL, "fd=%d called", fd);
	UNUSED_ARGUMENT(private_data);
	close_conn(fd);
	return TRUE;
}

static int http_epollhup(int fd, void * private_data)
{
	DBG_LOG(MSG, MOD_SSL, "fd=%d called", fd);
	return http_epollerr(fd, private_data);
}

static int http_timeout(int fd, void * private_data, double timeout)
{
	int ret = TRUE;
	unsigned int incarn;
	ssl_con_t * http_con = (ssl_con_t *)private_data;
	DBG_LOG(MSG, MOD_SSL, "fd=%d called", fd);
	UNUSED_ARGUMENT(timeout);
	AO_fetch_and_add1(&glob_tot_http_timeout);
	if (gnm[fd].peer_fd > 0 ) {
		network_mgr_t *pnm = &gnm[http_con->peer_fd];
		incarn = pnm->incarn;
		pthread_mutex_lock(&pnm->mutex);
		if(incarn == pnm->incarn) {
			ret = http_epollerr(fd, private_data);
		}
		pthread_mutex_unlock(&pnm->mutex);
	}
	return ret;
}
void log_ssl_stack_errors(void) 
{
	unsigned long l;
	char buf[256];
	char buf2[4096];
     	const char *file,*data;
	int line,flags;

	while((l=ERR_get_error_line_data(&file,&line,&data,&flags)) != 0)
	{
		ERR_error_string_n(l, buf, sizeof buf);
		snprintf(buf2, sizeof(buf2), "%s:%s:%d:%s\n", buf,
	                        file, line, (flags & ERR_TXT_STRING) ? data : "");
		DBG_LOG(WARNING, MOD_SSL, "%s", buf2);
	}
}
