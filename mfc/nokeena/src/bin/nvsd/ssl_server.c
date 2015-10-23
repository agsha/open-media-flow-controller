#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

#include "server.h"
#include "network.h"
#include "nkn_defs.h"
#include "nkn_debug.h"


NKNCNT_DEF(ssl_tot_socket, uint64_t, "", "total ssl socket")

#define PASSWORD "password"
#define SSL_PORT_str	":4433"
#define BUF_SIZE	8096
#define HTTP_PORT       80

int ssl_svrfd;
extern int ssl_port;
extern char * ssl_keyfile;
extern char * ssl_ca_list;
extern int ssl_client_auth;
extern char * ssl_ciphers;
extern char * ssl_dhfile;
SSL_CTX * ssl_ctx = NULL;
BIO *bio_err=0;
static const char *pass;

int ssl_exit_err(const char * string);
SSL_CTX * ssl_initialize_ctx(char *keyfile, const char *password);
void ssl_destroy_ctx(void);
static void load_dh_params(const char * file);
void generate_eph_rsa_key(void);
static int ssl_password_cb(char *buf,int num, int rwflag, void *userdata);
static int ssl_tcp_listen(void);
extern void init_conn(int svrfd, int sockfd, nkn_interface_t * pns, struct sockaddr_in * addr, int thr_num);

static void load_dh_params(const char * file)
{
        DH *ret=0;
        BIO *bio;

        /* "openssl dhparam -out dh_param_1024.pem -2 1024" */
        /* "openssl dhparam -out dh_param_512.pem -2 512" */
        if( (bio=BIO_new_file(file, "r")) == NULL ) {
                DBG_LOG(MSG, MOD_SSL, "Couldn't open DH file %s\r\n", file);
                ERR_print_errors(bio_err);
                exit(0);
        }

        ret=PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
        BIO_free(bio);
        if( SSL_CTX_set_tmp_dh(ssl_ctx, ret) < 0 ) {
                DBG_LOG(MSG, MOD_SSL, "Couldn't set DH parameters");
                ERR_print_errors(bio_err);
                exit(0);
        }
}

void generate_eph_rsa_key(void)
{
        RSA *rsa;

        rsa=RSA_generate_key(512,RSA_F4,NULL,NULL);

        if (!SSL_CTX_set_tmp_rsa(ssl_ctx,rsa)) {
                DBG_LOG(MSG, MOD_SSL, "Couldn't set RSA key");
                ERR_print_errors(bio_err);
                exit(0);
        }

        RSA_free(rsa);
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


SSL_CTX * ssl_initialize_ctx(char * keyfile, const char * password)
{
	SSL_METHOD *meth;
	SSL_CTX * pctx;
    
	/*
	 * 1. Initialization SSL library.
	 */
	if(!bio_err){
		/* Global system initialization*/
		SSL_library_init();
		SSL_load_error_strings();
      
		/* An error write context */
		bio_err=BIO_new_fp(stderr,BIO_NOCLOSE);
	}

	/*
	 * 2. Create our context
	 */
	meth=SSLv23_method();
	pctx=SSL_CTX_new(meth);

	/*
	 * 3. Load our keys and certificates
	 *    if it is password protected, use password.
	 */
	if(!(SSL_CTX_use_certificate_chain_file(pctx, keyfile))) {
		DBG_LOG(MSG, MOD_SSL, "Can't read certificate file %s", keyfile);
		ERR_print_errors(bio_err);
		exit(0);
	}
	pass=password;
	SSL_CTX_set_default_passwd_cb(pctx, ssl_password_cb);
	if(!(SSL_CTX_use_PrivateKey_file(pctx, keyfile, SSL_FILETYPE_PEM))) {
		DBG_LOG(MSG, MOD_SSL, "Can't read key file %s", keyfile);
		ERR_print_errors(bio_err);
		exit(0);
	}

	/*
	 * 3. Load the CAs we trust, root certificates
	 */
	if(!(SSL_CTX_load_verify_locations(pctx, ssl_ca_list, 0))) {
		DBG_LOG(MSG, MOD_SSL, "Can't read CA list %s", ssl_ca_list);
		ERR_print_errors(bio_err);
		exit(0);
	}
/*
#if (OPENSSL_VERSION_NUMBER < 0x00905100L)
	SSL_CTX_set_verify_depth(pctx,1);
#endif
*/
    
	return pctx;
}
     
void ssl_destroy_ctx(void)
{
	SSL_CTX_free(ssl_ctx);
}


static int sslsvr_epollin(int sockfd, void * private_data);
static int sslsvr_epollin(int sockfd, void * private_data)
{
        int clifd;
        struct sockaddr_in addr;
        socklen_t addrlen;
        nkn_interface_t * pns = (nkn_interface_t *)private_data;

        /* always returns TRUE for this case */

        //printf("cnt=%d\n", cnt);
        addrlen=sizeof(struct sockaddr_in);
        clifd = accept(sockfd, (struct sockaddr *)&addr, &addrlen);
        if (clifd == -1) {
                return TRUE;
        }
        nkn_mark_fd(clifd, NETWORK_FD);

	glob_ssl_tot_socket ++;

	init_conn(sockfd, clifd, pns, &addr, 0);//gnm[sockfd].pthr->num);

        return TRUE;
}

static int sslsvr_epollout(int sockfd, void * private_data);
static int sslsvr_epollout(int sockfd, void * private_data)
{
	UNUSED_ARGUMENT(sockfd);
	UNUSED_ARGUMENT(private_data);
	assert(0);

	return FALSE;
}

static int sslsvr_epollerr(int sockfd, void * private_data);
static int sslsvr_epollerr(int sockfd, void * private_data)
{
	UNUSED_ARGUMENT(sockfd);
	UNUSED_ARGUMENT(private_data);
	assert(0);

	return FALSE;
}

static int sslsvr_epollhup(int sockfd, void * private_data);
static int sslsvr_epollhup(int sockfd, void * private_data)
{
	return sslsvr_epollerr(sockfd, private_data);
}

/* **********************************************************
 * SSL listen server socket
 * ***********************************************************/
static int ssl_tcp_listen(void)
{
	int fd;
	struct sockaddr_in sin;
	int val=1;
    
	if( (fd=socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
		DBG_LOG(MSG, MOD_SSL, "Couldn't open ssl server socket");
		exit(0);
	}
    
	memset(&sin, 0, sizeof(sin));
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_family      = AF_INET;
	sin.sin_port        = htons(ssl_port);
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    
	if( bind(fd, (struct sockaddr *)&sin, sizeof(sin)) < 0 ) {
		DBG_LOG(MSG, MOD_SSL, "ssl server socket couldn't bind port %d", ssl_port);
		ERR_print_errors(bio_err);
		exit(0);
	}
	listen(fd, 5);  

	return(fd);
}


void ssl_server_init(void);
void ssl_server_init(void)
{
	static int s_server_session_id_context = 1;

	// SSL is diabled.
	if(ssl_port == 0) return;

	/* 1. Build our SSL context */
	ssl_ctx = ssl_initialize_ctx(ssl_keyfile, PASSWORD);
	if (ssl_dhfile && (ssl_dhfile[0]!=0)) {
		load_dh_params(ssl_dhfile);
	}

	SSL_CTX_set_session_id_context(ssl_ctx,
		(void*)&s_server_session_id_context,
		sizeof(s_server_session_id_context)); 
    
	/* 2. Set our cipher list */
	if(ssl_ciphers && (ssl_ciphers[0]!=0)){
		SSL_CTX_set_cipher_list(ssl_ctx, ssl_ciphers);
	}
    
	/* 3. Check supported client authentication */
	if(ssl_client_auth == 1) {
		SSL_CTX_set_verify(ssl_ctx,
				   SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
				   0);
	}
    
	/* 4. Launch SSL server socket */
	ssl_svrfd = ssl_tcp_listen();
	nkn_mark_fd(ssl_svrfd, NETWORK_FD);

	/* 
	 * 5. infinite loop to accept a ssl socket and 
	 * fork a process to handle the socket 
	 */
        if(register_NM_socket(ssl_svrfd,
                        (void *)&interface[0], // to be replaced.
                        sslsvr_epollin,
                        sslsvr_epollout,
                        sslsvr_epollerr,
                        sslsvr_epollhup,
                        NULL,
                        NULL,
                        0,
                        USE_LICENSE_FALSE,
                        FALSE) == FALSE)
        {
                // will this case ever happen at all?
                nkn_close_fd(ssl_svrfd, NETWORK_FD);
		ssl_destroy_ctx();
                return;
        }

	if (NM_add_event_epollin(ssl_svrfd) == FALSE) {
		DBG_LOG(MSG, MOD_SSL, "err in adding socket %d", ssl_svrfd);
                nkn_close_fd(ssl_svrfd, NETWORK_FD);
		ssl_destroy_ctx();
		return;
	}
	return;
}

void ssl_server_exit(void);
void ssl_server_exit(void)
{
	/* 6. shutdown server fd and destroy ssl ctx */
	//nkn_close_fd(ssl_svrfd, NETWORK_FD);
	ssl_destroy_ctx();
}
