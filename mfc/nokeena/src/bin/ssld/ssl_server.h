
#ifndef __SERVER__H__
#define __SERVER__H__

#include <assert.h>
#include <sys/queue.h>
#include "ssl_interface.h"
#include "nkn_ssl.h"

#ifdef HTTP2_SUPPORT
#include <spdylay/spdylay.h>
#include <nghttp2/nghttp2.h>
#include "proto_http/proto_http.h"
#endif

#define TIMER_INTERVAL	5

/*
 * socket handling
 */

#define SOCKET_CLOSED 		0
#define SOCKET_WAIT_EPOLLOUT_EVENT 1
#define SOCKET_WAIT_EPOLLIN_EVENT 2
#define SOCKET_WAIT_CM_DATA	3
#define SOCKET_DATA_SENTOUT	4
#define SOCKET_TIMER_EVENT	5
#define SOCKET_NO_ACTION	6	// Used by SSP during outbound path (send_to_ssp)
#define CPMAGIC_ALIVE	0xabcdef12
#define CPMAGIC_IN_POOL		0x12345678

/*
 * connection control block
 */

#define CONF_CLIENT_SIDE	0x0000000000000001
#define CONF_CANCELD		0x0000000000000004
#define CONF_SYN_SENT		0x0000000000000040
#define CONF_SSL_SENT           0x0000000000000080
#define CONF_SSL_HANDSHAKE      0x0000000000000100
#define CONF_TASK_TIMEOUT	0x0000000000000200
#define CONF_SSL                0x0000000000000400
#define CONF_SSL_READY          0x0000000000000800
#define CONF_SSL_ACCEPT         0x0000000000001000
#define CONF_HTTP_READY         0x0000000000002000
#define CONF_HTTP_CLOSE         0x0000000000004000
#define CONF_SSL_CONNECT	0x0000000000008000 
#define CPF_USE_KA_SOCKET	0x0000000000010000
#define CPF_IS_IPV6		0x0000000000020000

#ifdef HTTP2_SUPPORT
#define CONF_SPDY3_1		0x0000000000100000
#define CONF_HTTP2		0x0000000000200000
#endif

#define CON_MAGIC_FREE          0x123456789deadbee
#define CON_MAGIC_USED          0x1111111111111111

#define MAX_CB_BUF	(1024 * 16)

#ifdef HTTP2_SUPPORT

typedef struct ssl_hello_data {
#define HD_INIT 0
#define HD_DONE 1
#define HD_READ_HEADER 2
#define HD_READ_HELLO 3
#define HD_PROCESS_HELLO 4
    int state;
    unsigned char *buf;
    int buflen;
    int avail;
    int consumed;
    int hellosize;
} ssl_hello_data_t;

typedef struct ng_proto_ctx {

#define FL_WANT_READ (1 << 0)
#define FL_WANT_WRITE (1 << 1)
#define FL_HTTPIN_SUSPENDED (1 << 2)

    uint64_t flags;
    ssl_hello_data_t hd;
    union {
    	struct s_spdy {
	    spdylay_session *sess;
	    token_data_t http_token_data;
	    token_data_t http_resp_token_data;
	    off_t http_resp_content_len;
	    int current_streamid; // only allow one stream
	    int response_hdrs_sent;
	} spdy;

	struct s_http2 {
	    nghttp2_option *options;
	    nghttp2_session *sess;
	    token_data_t http_token_data;
	    uint64_t http_request_options;
	    token_data_t http_resp_token_data;
	    off_t http_resp_content_len;
	    int current_streamid; // only allow one stream
	    int response_hdrs_sent;

	    uint64_t pseudo_header_map;
	    int last_header_is_pseudo;
	    int request_hdr_cnt;
	} http2;
    } u;
} ng_proto_ctx_t;
#endif /* HTTP2_SUPPORT */


/*
 * con_t NM_func_timer event types
 */
typedef struct ssl_con_t{
	uint64_t	magic;
	uint64_t 	con_flag;

	/* socket control block */
        int32_t 	fd;
        int32_t 	peer_fd;
	uint16_t 	reserved1;
	uint16_t 	src_port;
	uint16_t	dest_port;
	uint8_t 	ip_family;
        off_t 		sent_len;

	char 		cb_buf[MAX_CB_BUF];
        int32_t 	cb_totlen;      // last data byte in cb_buf
        int32_t 	cb_offsetlen;   // first data byte in cb_buf
	int 		ssl_fid;

	time_t   	nkn_cur_ts;	// The time to calculate max_send_size

	/* SSL control block */
	SSL * ssl;
	BIO * io, * ssl_bio;
	BIO * sbio;
	int ssl_err;
	const char *servername;
	char *domainname;
	int ssl_hdr_sent;
	/* server information */
	struct nkn_interface * pns;
	        /* ip structures */
        ip_addr_t       src_ipv4v6;
        ip_addr_t       dest_ipv4v6;
	ip_addr_t 	remote_src_ipv4v6;
	uint16_t	remote_src_port;

#ifdef HTTP2_SUPPORT
	ng_proto_ctx_t ctx;
#endif /* HTTP2_SUPPORT */

} ssl_con_t;

#define SET_CON_FLAG(x, f) (x)->con_flag |= (f)
#define UNSET_CON_FLAG(x, f) (x)->con_flag &= ~(f)
#define CHECK_CON_FLAG(x, f) ((x)->con_flag & (f))


struct fd_struct
{
        int listen_fd;
        struct sockaddr_in client_addr;
        struct sockaddr_in dst_addr;
	
};

void init_conn(int svrfd, int ssl_fd, nkn_interface_t * pns, struct sockaddr_storage* addr, int thr_num);
void init_cli_conn(int svrfd, int http_fd, nkn_interface_t * pns, struct sockaddr_storage * addr, int thr_num);
int cp_activate_socket_handler(int fd, ssl_con_t * con);

ssl_con_t * cp_open_socket_handler(ssl_con_t *httpcon);

ssl_con_t * create_new_sslsvr_con(int cli_fd);

void close_cli_conn(int fd, int nolock);
int ssl_origin_epollin(int fd, void *private_date);
int ssl_origin_epollout(int fd, void *private_data);
int ssl_origin_epollerr(int fd, void *private_data);
int ssl_origin_epollhup(int fd, void *private_data);
int ssl_origin_timeout(int fd, void * private_data, double timeout);
int cpsp_add_into_pool(int fd, ssl_con_t * con);
#endif // __SERVER__H__

