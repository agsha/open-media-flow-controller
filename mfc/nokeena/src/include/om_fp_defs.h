#ifndef OM_FP_DEFS__H
#define OM_FP_DEFS__H

#include "network_hres_timer.h"

typedef enum {
        OP_CLOSE=1,
        OP_EPOLLIN,
	OP_EPOLLOUT,
	OP_EPOLLERR,
	OP_TUNNEL_READY,
	OP_CLOSE_CALLBACK,
	OP_TIMER_CALLBACK,
} op_type ;

#define CPF_METHOD_NOT_GET	0x0000000000000001
#define CPF_TE_CHUNKED		0x0000000000000002
#define CPF_HTTP_END_DETECTED	0x0000000000000004
#define CPF_CONN_KEEP_ALIVE	0x0000000000000008
#define CPF_CONN_CLOSE		0x0000000000000010
#define CPF_USE_KA_SOCKET	0x0000000000000020
#define CPF_IN_EVENT_QUEUE	0x0000000000000040
#define CPF_USED_IN_FPCON	0x0000000000000080
#define CPF_GET_ALREADY_SENT	0x0000000000000100
#define CPF_TE_CHUNKED_TRAIL	0x0000000000000200
//#define CPF_HTTP_ONE_PACKET	0x0000000000000400
#define CPF_WAIT_FOR_TUNNEL	0x0000000000000800
#define CPF_TRANSPARENT_REQ	0x0000000000001000
#define CPF_FIRST_NEW_SOCKET	0x0000000000002000
#define CPF_METHOD_CONNECT	0x0000000000004000
#define CPF_METHOD_HEAD         0x0000000000008000
#define CPF_CONN_TIMEOUT	0x0000000000010000
#define CPF_RESUME_TASK		0x0000000000020000
#define CPF_CONNECT_HANDLE      0x0000000000040000
#define CPF_REFETCH_CLOSE	0x0000000000080000
#define CPF_IS_IPV6		0x0000000000100000
#define CPF_HTTPS_CONNECT	0x0000000000200000
#define CPF_HTTPS_HDR_SENT	0x0000000000400000
#define CPF_ORIGIN_CLOSE	0x0000000000800000
#define CPF_UNRESP_CONNECTION	0x0000000001000000
#define CPF_SPECIAL_TUNNEL	0x0000000002000000

typedef enum {
	CP_FSM_FREE = 0,
	CP_FSM_SYN_SENT = 1,
	CP_FSM_GET_SENT = 2,
	CP_FSM_GET_SENT_EAGAIN = 3,
	CP_FSM_HTTP_BODY = 4,
	CP_FSM_CLOSE = 5,
} cp_fsm_state;

typedef enum {
	CP_APP_TYPE_OM = 1,
	CP_APP_TYPE_TUNNEL = 2,
} cp_app_type;


#define CPMAGIC_ALIVE	0x11134567
#define CPMAGIC_IN_POOL	0x22276543
#define CPMAGIC_DEAD	0xdead0001

#define OM_EPOLL_EVENT         0x00000001
#define CLIENT_EPOLL_EVENT     0x00000002
typedef struct struct_cp_con {
	uint32_t	magic;
        uint64_t        cp_flag;
        cp_fsm_state	state;
	cp_app_type	type;	// debug purpose only

	ip_addr_t	remote_ipv4v6;
	uint16_t	remote_port;
	ip_addr_t	src_ipv4v6;	// For tproxy case, this is client's ip
				// For reverse case, this is MFD interface IP
	uint16_t	src_port; // Unused so far
	ip_addr_t	tproxy_ipv4v6;
	int		client_fd;
        http_cb_t       http;
        int64_t         expected_len; // expected response data length
	origin_server_cfg_t oscfg; // origin server configuration
	const namespace_config_t *nsconf;// client nsconf for this namespace
	namespace_token_t   ns_token;

	int ssl_fid;
	int comp_len;
        /* **********************************
	 * connection pool callback functions 
	 ************************************ */
        void *          private_data;

	/* Connections handling. */
        int (* cp_connected)(void * private_data);
        void (* cp_connect_failed)(void * private_data, int called_from);

	/*
	 * return -1: error
	 * return 0: successfully done
	 * return 1: callback again, most likely socket returns EAGAIN error.
	 */
        int (* cp_send_httpreq)(void * private_data);

	/*
	 * return -1: means error
	 * return 0: HTTP response Header has been processed
	 * return 1: header handled, but don't continue to call back of 
	 *           cp_httpbody_received for now.
	 * len: the HTTP response header size
	 * content_length: for small object optimization purpose.
	 */
        int (* cp_http_header_received)(void * private_data, char * p, int len, int content_length);

	/*
	 * inform Application that there is a HTTP parser error.
	 */
        int (* cp_http_header_parse_error)(void * private_data);

	/*
	 * return -1: means error
	 * return -2: means EAGAIN
	 * return 0 : success
	 * *len: when input, total byte size. when output: total bytes left.
	 * shouldexit: default 0 before call. means if no more buffer memory
	 */
        int (* cp_httpbody_received)(void * private_data, char * p, int * len, int * shouldexit);

	/*
	 * Connection Pool receives socket closed signal.
	 */
        int (* cp_closed)(void * private_data, int close_httpconn);

	/*
	 * Timer event initiated by user
	 */
	int (* cp_timer)(void * private_data, net_timer_type_t type);
} cpcon_t;

#define SET_CPCON_FLAG(x, f) (x)->cp_flag |= (f)
#define UNSET_CPCON_FLAG(x, f) (x)->cp_flag &= ~(f)
#define CHECK_CPCON_FLAG(x, f) ((x)->cp_flag & (f))

#define CPCON_SET_STATE(x, f) (x)->state = (f)



/*
 * connection pooling exposed APIs.
 */

/**
 * Get connection poll con structure pointer.
 * this function may not have valid socket id.
 *
 *  INPUT:
 *   ip and port in network order.
 * OUTPUT:
 *   return NULL: means error.
 */
cpcon_t * cp_open_socket_handler(int * fd, 
	ip_addr_t* src_ip, ip_addr_t* ipv4v6, uint16_t port,
	cp_app_type type,
	void * private_data,
        int (* pcp_connected)(void * private_data),
        void (* pcp_connect_failed)(void * private_data, int called_from),
        int (* pcp_send_httpreq)(void * private_data),
        int (* pcp_http_header_received)(void * private_data, char * p, int len, int content_length),
        int (* pcp_http_header_parse_error)(void * private_data),
        int (* pcp_httpbody_received)(void * private_data, char * p, int * len, int * shouldexit),
        int (* pcp_closed)(void * private_data, int close_httpconn),
	int (* pcp_timer)(void * private_data, net_timer_type_t type));

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
int cp_activate_socket_handler(int fd, cpcon_t * cpcon);

/**
 *
 * caller should own the gnm[cpcon->fd].mutex lock
 *
 * OP_EPOLLOUT event:
 * Application receives second task and inform connection pool socket
 * to continue to send more data.
 *
 * OP_CLOSE event:
 * If other threads try to close connection poll socket,
 * they should use this function.
 *
 * The design goal is to separate two sockets httpcon from cpcon
 * any socket related operations should be limited to be run by epoll thread.
 *
 * cp_internal_close_conn() is an internal function to socket
 *
 * OP_TUNNEL_CONT event:
 * Tunnel is ready for forwarding data.
 */
void cp_add_event(int fd, op_type op, void * data);



/* *************************************************************
 * Tunnel code path
 * *************************************************************/

/* Transparent proxy  */
//extern int cp_set_transparent_mode_tosocket(cpcon_t * cpcon);

/*
 * Tunnel code path (Forward Proxy) Control Block
 */

#define FPMAGIC_ALIVE	0x22222222
#define FPMAGIC_DEAD	0xdead0002

#define FPF_IN_EVENT_QUEUE	0x0000000000000001
//#define FPF_TRANSPARENT_REQ	0x0000000000000002
#define FPF_RESTART_TUNNEL	0x0000000000000004
#define FPF_USE_DEST_IP		0x0000000000000008

typedef struct struct_fp_con_t {
	uint32_t	magic;
        int32_t         state;
        uint64_t        fp_flag;

        int32_t         cp_sockfd;
        uint32_t        cp_sockfd_incarn;
        uint32_t        httpcon_fd_incarn;
        con_t *         httpcon;
        cpcon_t *       cpcon;
	request_context_t request_ctx;
} fpcon_t;

#define SET_FPCON_FLAG(x, f) (x)->fp_flag |= (f)
#define UNSET_FPCON_FLAG(x, f) (x)->fp_flag &= ~(f)
#define CHECK_FPCON_FLAG(x, f) ((x)->fp_flag & (f))

#define SET_FPCON_STATE(x, f) (x)->state = f

/* incarn: used to verify if cp_sockfd is valid or not */
int fp_make_tunnel(con_t * httpcon, int cp_sockfd, uint32_t incarn, 
		   request_context_t *req_ctx);
extern int cp_set_transparent_mode_tosocket(int fd, cpcon_t * cpcon);
extern int cp_release_transparent_port(cpcon_t *cpcon);

#endif // OM_FP_DEFS__H
