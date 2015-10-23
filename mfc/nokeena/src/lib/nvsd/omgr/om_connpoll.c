#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <stdint.h>

#include "queue.h"
#include "nkn_debug.h"
#include "nkn_errno.h"
#include "nkn_stat.h"
#include "network.h"
#include "server.h"
#include "nkn_cfg_params.h"
#include "nkn_tunnel_defs.h"
#include "nkn_om.h"
#include "om_defs.h"
#include "om_fp_defs.h"
#include "nkn_om.h"
#include "nkn_lockstat.h"
#include "nkn_assert.h"
#include "nkn_ssl.h"

#define F_FILE_ID	LT_OM_CONNPOLL

#define MAX_CHUNK_HDR_SIZE	32
#define MAX_CHUNK_EXT_HDR_SIZE 1024

NKNCNT_DEF(cp_close_overwrite_closeback, int32_t, "", "")
NKNCNT_DEF(cp_cannot_acquire_mutex, int32_t, "", "mutex busy")
NKNCNT_DEF(err_cp_magic_not_alive, int32_t, "", "error")
NKNCNT_DEF(cp_event_replacement, int32_t, "", "replace event type")
NKNCNT_DEF(cp_err_unknown_2, int64_t, "", "unknown error 2")
NKNCNT_DEF(cp_reused_same_time, int32_t, "", "reused exactly same time")
NKNCNT_DEF(cp_socket_reused, int64_t, "", "total reused sockets")
NKNCNT_DEF(cp_tot_sockets_in_pool, AO_t, "", "total reused sockets")
NKNCNT_DEF(cp_tot_sockets_in_pool_ipv6, AO_t, "", "total reused sockets for ipv6")
NKNCNT_DEF(cp_tot_cur_open_sockets, AO_t, "", "total open sockets")
NKNCNT_DEF(cp_tot_cur_open_sockets_ipv6, AO_t, "", "total open sockets for ipv6")
NKNCNT_DEF(cp_tot_keep_alive_idleout_sockets, int64_t, "", "idled out cp sockets")
NKNCNT_DEF(cp_tot_closed_sockets, AO_t, "", "total closed cp sockets")
NKNCNT_DEF(cp_tot_closed_sockets_ipv6, AO_t, "", "total closed cp sockets for ipv6")
NKNCNT_DEF(om_http_tot_200_resp, uint64_t, "", "OM 200 response")
NKNCNT_DEF(om_http_tot_206_resp, uint64_t, "", "OM 206 response")
NKNCNT_DEF(om_http_tot_304_resp, uint64_t, "", "OM 304 response")
NKNCNT_DEF(om_http_tot_400_resp, uint64_t, "", "OM 400 response")
NKNCNT_DEF(om_http_tot_404_resp, uint64_t, "", "OM 404 response")
NKNCNT_DEF(om_http_tot_414_resp, uint64_t, "", "OM 414 response")
NKNCNT_DEF(om_http_tot_416_resp, uint64_t, "", "OM 416 response")
NKNCNT_DEF(om_http_tot_500_resp, uint64_t, "", "OM 500 response")
NKNCNT_DEF(om_http_tot_501_resp, uint64_t, "", "OM 501 response")
NKNCNT_DEF(om_http_resp_parse_err, uint64_t, "", "OM http response parse error")
NKNCNT_DEF(cp_gdb_no_callback_1, uint32_t, "", "gdb")
NKNCNT_DEF(cp_gdb_no_callback_2, uint32_t, "", "gdb")
NKNCNT_DEF(cp_gdb_no_callback_3, uint32_t, "", "gdb")
NKNCNT_DEF(cp_gdb_no_callback_4, uint32_t, "", "gdb")
NKNCNT_DEF(cp_gdb_insert_event, uint32_t, "", "gdb")
NKNCNT_DEF(cp_gdb_ignore_event_2, uint32_t, "", "gdb")
NKNCNT_DEF(cp_gdb_ignore_event_3, uint32_t, "", "gdb")
NKNCNT_DEF(cp_activate_err_1, uint32_t, "", "activate error 1")
NKNCNT_DEF(cp_activate_err_2, uint32_t, "", "activate error 2")
NKNCNT_DEF(cp_activate_err_3, uint32_t, "", "activate error 3")
NKNCNT_DEF(cp_activate_err_4, uint32_t, "", "activate error 4")
NKNCNT_DEF(cp_activate_err_5, uint32_t, "", "activate error 5")
NKNCNT_DEF(cp_activate_err_6, uint32_t, "", "activate error 6")
NKNCNT_DEF(cp_fpcon_closed_1, uint32_t, "", "fpcon has been freed when calling fp_connected")
NKNCNT_DEF(cp_fpcon_closed_2, uint32_t, "", "fpcon has been freed when calling fp_send_http")
NKNCNT_DEF(cp_fpcon_closed_3, uint32_t, "", "fpcon has been freed when calling fp_connected")
NKNCNT_DEF(cp_fpcon_closed_4, uint32_t, "", "fpcon has been freed when calling fp_send_http")
NKNCNT_DEF(http_resp_parse_err_longheader, uint64_t, "", "long reponse header errors")
NKNCNT_DEF(origin_recv_tot_bytes, AO_t, "", "total bytes recvd from origin ")
NKNCNT_DEF(cp_activate_err_addrnotavail, uint64_t, "", "EADDRNOTAVAIL errors on a connect()")
NKNCNT_DEF(origin_send_tot_request, AO_t, "", "total requests sent to origin")
NKNCNT_DEF(origin_recv_tot_response, AO_t, "", "total responses recvd from origin")
NKNCNT_DEF(om_http_www_authentication_ntlm, uint32_t, "", "OM WWW authentication NTLM")

uint64_t nkn_cfg_tot_socket_in_pool = 0;
uint32_t ssl_listen_port = 443;
#define NO_DEBUG_HELPER 0
#if NO_DEBUG_HELPER
#define dbg_helper_connect_sock(errnum, state, _cpcon, _fn, _ln) \
    do { \
	char _sip[32], _dip[32]; \
	struct in_addr __dip = { .s_addr = ((cpcon_t*)(_cpcon))->remote_ip }; \
	struct in_addr __sip = { .s_addr = ((cpcon_t*)(_cpcon))->src_ip}; \
	snprintf(_sip, 32, "%s", inet_ntoa(__sip)); \
	snprintf(_dip, 32, "%s", inet_ntoa(__dip)); \
	log_debug_msg(SEVERE, MOD_CP, "[SEVERE.MOD_CP] %s:%d: connect() : [%8s] source = %s:%u, dest = %s:%u (errno = %d)\n", \
		_fn, _ln, state, _sip, ((cpcon_t*)(cpcon))->src_port, _dip, 80, errnum); \
	if (errno == EADDRNOTAVAIL) { \
	    glob_cp_activate_err_addrnotavail++; \
	} \
    } while (0)
#else /* NO_DEBUG_HELPER */
#define dbg_helper_connect_sock(errnum, state, _cpcon, _fn, _ln) \
    do { \
	if (errno == EADDRNOTAVAIL) \
	    glob_cp_activate_err_addrnotavail++; \
    } while (0)

#endif
extern char *ssl_shm_mem;
/* DNS lookup */
extern void call_om_close_conn(void * ocon); // exceptional case to be called in file.
extern int cp_tunnel_client_data_to_server(void * private_data);
extern int cp_tunnel_server_data_to_client(void * private_data);



static void cp_internal_close_conn(int fd, cpcon_t * cpcon);
static int cp_epollout(int fd, void * private_data);

void http_accesslog_write(con_t * con);
//static pthread_mutex_t tr_socket_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ==================================================================== */
/*
 * Finite State Machine
 *

        CP_FSM_FREE
            |
            | non-blocking connect, SYN packet sent
            |
        CP_FSM_SYN_SENT
            |
            | After receive SYN+ACK packet, sent out GET request
            |
        CP_FSM_GET_SENT
            |
            | epollin event, received the HTTP header response.
            | parse the complete HTTP header response.
	    | if parse succeeded
            |
	 IF EGAIN--- yes -----	CP_FSM_GET_SENT_EAGAIN
	    |                          |
	    | no                       |
	    |                          |
            |----------------<---------|
            |
        CP_FSM_HTTP_BODY  <== continue in this state until all data received.
            |
            | receive epollin event, receiving all data.
            |
        CP_FSM_CLOSE
            |
            | After socket get closed and task gets cleaned.
            |
        CP_FSM_FREE
 */

typedef enum {
	FR_OK = 14,		// OK.
	FR_ASK_TO_CLOSE = 10, 	// ask caller to call cp_internal_close_conn() 
				// and not reuse the socket
	FR_CPCON_CLOSED = 86,	// cp_internal_close_conn() has been called before return.
				// socket may be reused
} fsm_ret;

static fsm_ret cp_syn_sent(int fd, cpcon_t * cpcon);
static fsm_ret cp_get_sent(int fd, cpcon_t * cpcon);
static fsm_ret cp_get_sent_eagain(int fd, cpcon_t *cpcon);
static fsm_ret cp_http_body(int fd, cpcon_t * cpcon);
static fsm_ret cp_close(int fd, cpcon_t * cpcon);
static fsm_ret cp_crash(int fd, cpcon_t * cpcon);

static struct cp_fsm_structure {
	int state;
	fsm_ret (*fn) (int fd, cpcon_t * cpcon);
} cpfsm[] = {
	{ CP_FSM_FREE, cp_crash },
	{ CP_FSM_SYN_SENT, cp_syn_sent },
        { CP_FSM_GET_SENT, cp_get_sent },
	{ CP_FSM_GET_SENT_EAGAIN, cp_get_sent_eagain },
        { CP_FSM_HTTP_BODY, cp_http_body },
	{ CP_FSM_CLOSE, cp_close }
};
int cp_fsm(int fd, cpcon_t * cpcon);


#define MAX_CP_TRACKER (409600)

struct cp_tracker {
	struct cp_tracker *next;
	ip_addr_t ipv4v6;
	unsigned int unresp_connections;
};

static struct cp_tracker * g_pcp_tracker[MAX_CP_TRACKER];
static pthread_mutex_t cp_tracker_mutex[MAX_CP_TRACKER];

static uint32_t cp_tracker_hash_func(ip_addr_t *ipv4v6);
static int cp_tracker_del_unresp_connections(uint32_t ct_ht_index, cpcon_t *cpcon);
static int cp_tracker_check_and_add_unresp_connections(uint32_t ct_ht_index, cpcon_t *cpcon);


static int om_http_get_chunklen(char * p, int * totlen);
static int om_http_get_chunklen(char * p, int * totlen)
{
        int chunksize=0;
	int found;
	int ext_hdr_len = 0;
	int extra_header = 0;
	int num_digits = 0;

	if(!p || !totlen) 
		return -1;

        // If leading by CRLF, we skip it.
	while (*p=='\r' || *p=='\n') { 
		(*totlen)--;  
		p++; 
		if (*totlen<=0) 
			return -1;
	}

        // Since chuncked header in in hex and occupied the whole line.
        // This logic here is to convert to dec.
	while (*totlen>0) {
		if(*p>='0' && *p<='9') 
			chunksize += *p-'0';
		else if(*p>='a' && *p<='f') 
			chunksize += *p-'a'+10;
		else if(*p>='A' && *p<='F') 
			chunksize += *p-'A'+10;
		else { 
			chunksize >>= 4; 
			break; 
		}
		(*totlen)--; 
		p++; 
		if (*totlen<=0) 
			return -1;
		chunksize <<= 4;
		num_digits++;
	}

	if (num_digits == 0) {
		return -2;
	}

	// skip to the end of this line
	found = 0;
	if (*p == ';') extra_header = 1;
	while (*totlen>0) {
		(*totlen)--; 
		p++; 
		ext_hdr_len++;
		if (*totlen<=0) {
			/* not enough data to detect end of line 
			 * BZ 3438
			 * BUG 7560: temperorily fix. Need to understand more
			 * why cp_timeout is not called.
			 */
			if (extra_header) {
				return (ext_hdr_len > MAX_CHUNK_EXT_HDR_SIZE) ? -2 : -1;
			} else {
				return (ext_hdr_len > MAX_CHUNK_HDR_SIZE) ? -2 : -1;
			}
		}
		if( *p=='\n' ) { 
			found++; 
			break; 
		}
	}
	if(found == 0) 
		return -1;
	(*totlen)--; 
	p++; 
	if (*totlen<=0) 
		return -1;

        return chunksize;
}



/* ==================================================================== */

/*
 * For non-blocking connection, This function get called when SYN+ACK has been received.
 */
static fsm_ret cp_syn_sent(int fd, cpcon_t * cpcon)
{
	int ret;
	int retlen;

	NM_EVENT_TRACE(fd, FT_CP_SYN_SENT);
	retlen=sizeof(ret);
	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *)&ret, 
		       (socklen_t *)&retlen)) {
		DBG_LOG(MSG, MOD_CP, "getsockopt(fd=%d) failed, errno=%d", 
			fd, errno);
		return FR_ASK_TO_CLOSE;
	}
	if(ret) {
		DBG_LOG(MSG, MOD_CP, "connect(fd=%d) failed, ret=%d", fd, ret);
		return FR_ASK_TO_CLOSE;
	}

	DBG_LOG(MSG, MOD_CP, "connect(fd=%d) succeeded", fd);
	if(cpcon->cp_connected) {
                /*The fpcon might be freed at this time, check if there is any use
                in proceeding further*/
		if (cpcon->type ==  CP_APP_TYPE_TUNNEL) {
                        fpcon_t * fpcon  = (fpcon_t *)cpcon->private_data;
                        if (fpcon &&
                           (fpcon->magic == FPMAGIC_ALIVE) &&
                           (fpcon->cp_sockfd == fd)) {
                               ret = (*(cpcon->cp_connected)) (cpcon->private_data);
                       }
                       else {
                                glob_cp_fpcon_closed_1++;
                                return FR_ASK_TO_CLOSE;
                       }
               	}
               	else {
                       ret = (*(cpcon->cp_connected)) (cpcon->private_data);
               	}
		switch (ret) {
		case -1:
			return FR_ASK_TO_CLOSE;
		default:
			break;
		}
	}

	if (cpcon->nsconf) {
		NS_INCR_STATS(cpcon->nsconf, PROTO_HTTP, server, active_conns);
		NS_INCR_STATS(cpcon->nsconf, PROTO_HTTP, server, conns);
	}

        if (CHECK_CPCON_FLAG(cpcon, CPF_CONNECT_HANDLE)
                        && CHECK_CPCON_FLAG(cpcon, CPF_METHOD_CONNECT)) {
                /* We dont need to send anything to origin server as we are
                 * handling CONNECT, and we just act as relay here-after */
                cpcon->expected_len = OM_STAT_SIG_TOT_CONTENT_LEN;
                CPCON_SET_STATE(cpcon, CP_FSM_HTTP_BODY);
                return FR_OK;
        }

	/*
	 * Connection has been established.
	 * Time to send out GET request.
	 */
	if(cpcon->cp_send_httpreq) {
		/*The fpcon might be freed at this time, check if there is any use
               	in proceeding further*/
               	if (cpcon->type ==  CP_APP_TYPE_TUNNEL) {
                       fpcon_t * fpcon  = (fpcon_t *)cpcon->private_data;
                       if (fpcon &&
                          (fpcon->magic == FPMAGIC_ALIVE) &&
                          (fpcon->cp_sockfd == fd)) {
                               ret = (*(cpcon->cp_send_httpreq)) (cpcon->private_data);
                       }
                       else {
                               glob_cp_fpcon_closed_2++;
                               return FR_ASK_TO_CLOSE;
                       }
               	}
               	else {
                       ret = (*(cpcon->cp_send_httpreq)) (cpcon->private_data);
               	}
		if(ret == -1) {
			DBG_LOG(MSG, MOD_CP, "cp_syn_sent: cp_send_httpreq return %d", ret);
			return FR_ASK_TO_CLOSE;
		}
		else if(ret == 1) {
			// EAGAIN
			DBG_LOG(MSG, MOD_CP, "cp_syn_sent: cp_send_httpreq return %d", ret);
			return FR_OK;
		}
		else if(ret == 0) {
		}
		// Increment no. of requests sent to origin
		AO_fetch_and_add1(&glob_origin_send_tot_request);
	}

	/* Set up next state */
	SET_CPCON_FLAG(cpcon, CPF_GET_ALREADY_SENT);
	CPCON_SET_STATE(cpcon, CP_FSM_GET_SENT);

	return FR_OK;
}

fsm_ret cp_get_sent_continue_after_callback(int fd, cpcon_t * cpcon);
fsm_ret cp_get_sent_continue_after_callback(int fd, cpcon_t * cpcon)
{
	NM_EVENT_TRACE(fd, FT_CP_GET_SENT_CONTINUE_AFTER_CALLBACK);
	CPCON_SET_STATE(cpcon, CP_FSM_HTTP_BODY);

       /*
        * Prior to the fix for BZ-3862, there was an assumption 
        * that a response from origin has only one header followed
        * by response body, which is not true. An origin server can send
        * multiple information headers(1xx) before a 200 response is
        * sent to MFC. The cb_offsetlen below is used to track the
        * point of buffer until where the header parsing is successfully
        * completed.  
        */  
        cpcon->http.cb_offsetlen += cpcon->http.http_hdr_len;
        cpcon->expected_len -= cpcon->http.http_hdr_len;

	if(cpcon->expected_len == 0) {
		// end of transaction.
		// For chunked response, MUST expected_len != 0 here
		if (!CHECK_CPCON_FLAG(cpcon, CPF_TE_CHUNKED)) {
			// end of transaction . content_length = 0 case.
			SET_CPCON_FLAG(cpcon, CPF_HTTP_END_DETECTED);
			cp_internal_close_conn(fd, cpcon);
			return FR_CPCON_CLOSED;
		}
	}

	if(cpcon->http.cb_totlen == cpcon->http.cb_offsetlen) {
		// No more data
		cpcon->http.cb_totlen = 0;
		cpcon->http.cb_offsetlen = 0;
        	return FR_OK;
	}

	// Some extra data in the same packet.
	return cp_http_body(fd, cpcon); 
}

/* Incase of parse error in the response path copy_resp_buf()
   function is used to copy the content from origin server
   response buffer to client response buffer */
static inline int cp_copy_resp_buf(cpcon_t *cpcon, con_t *httpcon)
{
	int len = 0;

	if (httpcon == NULL || cpcon == NULL) {
		return FALSE;
	}

	len = httpcon->http.cb_max_buf_size - httpcon->http.cb_totlen;
	httpcon->http.resp_buf = (char *)nkn_malloc_type(
			    cpcon->http.http_hdr_len, mod_om_resp_buf_t);
	if (httpcon->http.resp_buf == NULL) {
		DBG_LOG(MSG, MOD_CP, "malloc failed");
		return FALSE;
	}

	memmove(httpcon->http.resp_buf, cpcon->http.cb_buf +
		    cpcon->http.cb_offsetlen, cpcon->http.http_hdr_len);
	httpcon->http.resp_buflen = cpcon->http.http_hdr_len;
	httpcon->http.res_hdlen = cpcon->http.http_hdr_len;

	return TRUE;
}

static inline int cp_make_special_tunnel(int fd, cpcon_t *cpcon)
{
	con_t *httpcon;
	int ret;

	SET_CPCON_FLAG(cpcon, CPF_CONN_CLOSE);
	CLEAR_HTTP_FLAG(&cpcon->http, HRF_CONNECTION_KEEP_ALIVE);
	CLEAR_HTTP_FLAG(&cpcon->http, HRF_PARSE_DONE);
	DBG_LOG(MSG, MOD_CP, "http response parse error: packet_length=%d", cpcon->http.cb_totlen);
	if (!CHECK_HTTP_FLAG(&cpcon->http, HRF_HTTP_09)) {
		glob_om_http_resp_parse_err++;
	}

	if (cpcon->type ==  CP_APP_TYPE_TUNNEL) {
		fpcon_t *fpcon = (fpcon_t *)cpcon->private_data;
		if (fpcon && (fpcon->magic == FPMAGIC_ALIVE) && (fpcon->cp_sockfd == fd)) {
			cpcon->http.content_length = 0;
                        httpcon =  fpcon->httpcon;
                        cpcon->http.http_hdr_len =
                                cpcon->http.cb_totlen - cpcon->http.cb_offsetlen;
                        cpcon->expected_len =  OM_STAT_SIG_TOT_CONTENT_LEN;
                        if (cp_copy_resp_buf(cpcon, httpcon) == TRUE) {
				return 2;
                        }
                } else {
                        glob_cp_fpcon_closed_3++;
                        return -1;
                }
        } else {
                ret = (*(cpcon->cp_http_header_parse_error))(cpcon->private_data);
                switch (ret) {
                case 1:
                        // Forwarding to tunnel path
                        cpcon->cp_httpbody_received = NULL;
                        cpcon->cp_closed = NULL;
                        glob_cp_gdb_no_callback_1 ++;
                        SET_CPCON_FLAG(cpcon, CPF_WAIT_FOR_TUNNEL);
                        assert(cpcon->http.cb_totlen > 0);
                        cpcon->http.http_hdr_len =
                                cpcon->http.cb_totlen - cpcon->http.cb_offsetlen;
                        cpcon->expected_len = OM_STAT_SIG_TOT_CONTENT_LEN;
                        httpcon = ((omcon_t *) (cpcon->private_data))->httpreqcon;
                        if (cp_copy_resp_buf(cpcon , httpcon) == TRUE) {
                                DBG_LOG(MSG, MOD_CP, "forwarding to tunnel path");
                                call_om_close_conn(cpcon->private_data);
                                return 1;
                        }
                        break;
                case -1:
                default:
                        break;
                }
        }
        cpcon->http.cb_totlen = 0;
        cpcon->http.cb_offsetlen = 0;
	return -1;
}

static inline int cp_check_special_limitation(cpcon_t *cpcon)
{
	con_t *httpcon;
	omcon_t *ocon;
	fpcon_t *fpcon;

	if (cpcon->type == CP_APP_TYPE_TUNNEL) {
		fpcon = (fpcon_t *)cpcon->private_data;
		httpcon =  fpcon->httpcon;
	} else {
		ocon = (omcon_t *)cpcon->private_data;
		httpcon = ocon->httpreqcon;
	}

	if (CHECK_HTTP_FLAG(&cpcon->http, HRF_WWW_AUTHENTICATE_NTLM)) {
		if (cpcon->type == CP_APP_TYPE_TUNNEL) {
			if (fpcon && (fpcon->magic == FPMAGIC_ALIVE)) {
				httpcon->http.tunnel_reason_code =
						NKN_TR_RES_WWW_AUTHENTICATE_NTLM;
			}
		} else {
			if (OM_CHECK_FLAG(ocon, OMF_DO_GET) && ocon->p_mm_task) {
				ocon->p_mm_task->err_code = NKN_OM_WWW_AUTHENTICATE_NTLM;
			}
		}

		cpcon->http.cb_offsetlen = 0;
		glob_om_http_www_authentication_ntlm++;
		SET_CPCON_FLAG(cpcon, CPF_SPECIAL_TUNNEL);

		return 1;
	}

	if (cpcon->http.respcode == 101) {
		if (CHECK_HTTP_FLAG2(&httpcon->http, HRF2_CONNECTION_UPGRADE) &&
		    CHECK_HTTP_FLAG2(&httpcon->http, HRF2_UPGRADE_WEBSOCKET) &&
		    CHECK_HTTP_FLAG2(&cpcon->http, HRF2_CONNECTION_UPGRADE) &&
		    CHECK_HTTP_FLAG2(&cpcon->http, HRF2_UPGRADE_WEBSOCKET)) {
			if (cpcon->type != CP_APP_TYPE_TUNNEL) {
				ocon->p_mm_task->err_code = NKN_OM_CONNECTION_UPGRADE;
			}
			cpcon->http.cb_offsetlen = 0;
			SET_CPCON_FLAG(cpcon, CPF_SPECIAL_TUNNEL);
			return 1;
		}
	}

	return 0;
}

//
// After we sent out GET request, we receive epollin event and read data from socket
//
static fsm_ret cp_get_sent(int fd, cpcon_t * cpcon)
{
	int len;
	int ret = HPS_ERROR;
	int content_length;

	NM_EVENT_TRACE(fd, FT_CP_GET_SENT);
	if (CHECK_CPCON_FLAG(cpcon, CPF_WAIT_FOR_TUNNEL)) {
		NM_del_event_epoll(fd);
		return FR_OK;
	}

        //Parse the HTTP response. Skip it for HTTP 0.9
	if (!CHECK_HTTP_FLAG(&cpcon->http, HRF_HTTP_09)) {
		SET_HTTP_FLAG(&cpcon->http, HRF_HTTP_RESPONSE);
		if (!CHECK_HTTP_FLAG(&cpcon->http, HRF_PARSE_DONE)) {
			ret = http_parse_header(&cpcon->http);
		} else {
			//This case should not happen... 
			//wrong state entry can cause this
			assert(0);
		}
	}

        if (ret == HPS_NEED_MORE_DATA) {
		if (CHECK_CPCON_FLAG(cpcon, CPF_ORIGIN_CLOSE)) {
			ret = HPS_ERROR;
		} else {
			return FR_OK;
		}
        }

	if (ret == HPS_ERROR || cp_check_special_limitation(cpcon)) {
		ret = cp_make_special_tunnel(fd, cpcon);
		switch (ret) {
		case 1:
			return FR_OK;
		case 2:
			goto normal_path;
		case -1:
		default:
			return FR_ASK_TO_CLOSE;
		}
        }
	//printf("cp_get_sent: content_length=%ld\n", cpcon->http.content_length);

	/*
	 * Following codes are used to extract information from HTTP response
	 * so we could make a decision if the socket is reuseable or not.
	 *
	 * Socket is reuseable when:
	 *     1. Content-Length: <num>
	 *     2. Transfer-Encoding: chunked
	 * and 
	 *     Connection: Keep-Alive is provided.
	 */
	/*
	 * RFC 2616: 4.4
4.4 Message Length

   The transfer-length of a message is the length of the message-body as
   it appears in the message; that is, after any transfer-codings have
   been applied. When a message-body is included with a message, the
   transfer-length of that body is determined by one of the following
   (in order of precedence):

   1.Any response message which "MUST NOT" include a message-body (such
     as the 1xx, 204, and 304 responses and any response to a HEAD
     request) is always terminated by the first empty line after the
     header fields, regardless of the entity-header fields present in
     the message.

   2.If a Transfer-Encoding header field (section 14.41) is present and
     has any value other than "identity", then the transfer-length is
     defined by use of the "chunked" transfer-coding (section 3.6),
     unless the message is terminated by closing the connection.

   3.If a Content-Length header field (section 14.13) is present, its
     decimal value in OCTETs represents both the entity-length and the
     transfer-length. The Content-Length header field MUST NOT be sent
     if these two lengths are different (i.e., if a Transfer-Encoding
     header field is present). If a message is received with both a
     Transfer-Encoding header field and a Content-Length header field,
     the latter MUST be ignored.

   4.If the message uses the media type "multipart/byteranges", and the
     ransfer-length is not otherwise specified, then this self-
     elimiting media type defines the transfer-length. This media type
     UST NOT be used unless the sender knows that the recipient can arse
     it; the presence in a request of a Range header with ultiple byte-
     range specifiers from a 1.1 client implies that the lient can parse
     multipart/byteranges responses.

       A range header might be forwarded by a 1.0 proxy that does not
       understand multipart/byteranges; in this case the server MUST
       delimit the message using methods defined in items 1,3 or 5 of
       this section.

   5.By the server closing the connection. (Closing the connection
     cannot be used to indicate the end of a request body, since that
     would leave no possibility for the server to send back a response.)

	 */
	if ( CHECK_HTTP_FLAG(&cpcon->http, HRF_TRANSCODE_CHUNKED) ) {
		cpcon->expected_len = cpcon->http.http_hdr_len;
		SET_CPCON_FLAG(cpcon, CPF_TE_CHUNKED);
		/* If content length is also present, ignore content
		 * length, and delete it. BZ 2220
		 */
		if (CHECK_HTTP_FLAG(&cpcon->http, HRF_CONTENT_LENGTH)) {
			mime_header_t *hdr = &cpcon->http.hdr;
			cpcon->http.content_length = 0;
			delete_known_header(hdr, MIME_HDR_CONTENT_LENGTH);
			CLEAR_HTTP_FLAG(&cpcon->http, HRF_CONTENT_LENGTH);
		}
	}
	else if( CHECK_HTTP_FLAG(&cpcon->http, HRF_CONTENT_LENGTH) &&
		 !CHECK_CPCON_FLAG(cpcon, CPF_METHOD_HEAD)) {
		/* total http body + http response header length */
		cpcon->expected_len = cpcon->http.content_length + cpcon->http.http_hdr_len;
#if 0
		printf("content-length, expected_len=%ld content_lenth=%ld http_hdr_len=%d\n",  
			cpcon->expected_len, 
			cpcon->http.content_length,
			cpcon->http.http_hdr_len);
#endif // 0
	}
	else if( (cpcon->http.respcode == 304) ||
		 (cpcon->http.respcode == 204) ||
		 //(cpcon->http.respcode == 100) ||  // BUG 6393
		 CHECK_CPCON_FLAG(cpcon, CPF_METHOD_HEAD) ) {
		/*
		 * According to RFC, the following response MUST not have body
		 *	1) HEAD response 
		 *	2) 304 response
		 */
		cpcon->expected_len = cpcon->http.http_hdr_len;
	}
	else {
		cpcon->expected_len = OM_STAT_SIG_TOT_CONTENT_LEN;
	}

	if (CHECK_HTTP_FLAG(&cpcon->http, HRF_CONNECTION_KEEP_ALIVE) ||
	    (CHECK_HTTP_FLAG(&cpcon->http, HRF_HTTP_11) && 
	    !is_known_header_present(&cpcon->http.hdr, MIME_HDR_CONNECTION))) {
		SET_CPCON_FLAG(cpcon, CPF_CONN_KEEP_ALIVE);
	}
	else {
		SET_CPCON_FLAG(cpcon, CPF_CONN_CLOSE);
	}

#if 0 
       /* Just needed for debugging. Hence commenting out this block now. */
        {     
            int    via_listsz = 4096;
            char * via_list   = alloca(via_listsz);
      
            via_list[0] = '\0';
            concatenate_known_header_values(&cpcon->http.hdr,
                                            MIME_HDR_CONNECTION,
                                            via_list, via_listsz) ;
            DBG_LOG(MSG, MOD_HTTP,
                    "OM-response: Connection header values: '%s'\n", 
                    via_list);
        }
#endif
       /* 
        * Remove the unknown headers which are from the connection-token
        * list. For eg:- if there are 2 headers like,
        *                "Connection: foo-bar\r\n" \
        *                "foo-bar: value-1\r\n"
        * Then MFD should not fwd header "foo-bar: value-1" back to client.
        * BUG: 3428.
        */    
        http_hdr_block_connection_token(&cpcon->http.hdr) ;

	/* Update counters */
	switch(cpcon->http.respcode) {
		case 200: glob_om_http_tot_200_resp++; break;
		case 206: glob_om_http_tot_206_resp++; break;
		case 304: glob_om_http_tot_304_resp++; break;
		case 400: glob_om_http_tot_400_resp++; break;
		case 404: glob_om_http_tot_404_resp++; break;
		case 414: glob_om_http_tot_414_resp++; break;
		case 416: glob_om_http_tot_416_resp++; break;
		case 500: glob_om_http_tot_500_resp++; break;
		case 501: glob_om_http_tot_501_resp++; break;
		default:  break;
	}
	if (cpcon->nsconf) {
		if (cpcon->http.respcode >= 200 && cpcon->http.respcode < 300) {
			NS_INCR_STATS((cpcon->nsconf), PROTO_HTTP, server, resp_2xx);
		} else if (cpcon->http.respcode >= 300 && cpcon->http.respcode < 400) {
			NS_INCR_STATS((cpcon->nsconf), PROTO_HTTP, server, resp_3xx);
		} else if (cpcon->http.respcode >= 400 && cpcon->http.respcode < 500) {
			NS_INCR_STATS((cpcon->nsconf), PROTO_HTTP, server, resp_4xx);
		} else if (cpcon->http.respcode >= 500 && cpcon->http.respcode < 600) {
			NS_INCR_STATS((cpcon->nsconf), PROTO_HTTP, server, resp_5xx);
		}
		NS_INCR_STATS((cpcon->nsconf), PROTO_HTTP, server, responses);
        }

	SET_HTTP_FLAG(&cpcon->http, HRF_PARSE_DONE);

normal_path:

	// Increment no. of responses received from origin
	AO_fetch_and_add1(&glob_origin_recv_tot_response);

	/* It is a response to HEAD request, no body is expected */
	if( (cpcon->http.respcode == 304) ||
	    (cpcon->http.respcode == 204) ||
	    //(cpcon->http.respcode == 100) ||   // BUG6393
	    CHECK_CPCON_FLAG(cpcon, CPF_METHOD_HEAD) ) {
		cpcon->expected_len = cpcon->http.http_hdr_len;
		content_length = 0;
	}
	else {
		content_length = cpcon->http.content_length;
	}
	
	len = cpcon->http.http_hdr_len;
	//printf("cp_get_sent: http.cb_offsetlen=%d http.cb_totlen=%d http.http_hdr_len=%d\n",
	//			cpcon->http.cb_offsetlen, cpcon->http.cb_totlen, len);
	cpcon->http.cb_offsetlen = 0;
	if (cpcon->cp_http_header_received) {
        	char * p = &cpcon->http.cb_buf[cpcon->http.cb_offsetlen];
        	if (cpcon->type ==  CP_APP_TYPE_TUNNEL) {
                	fpcon_t * fpcon  = (fpcon_t *)cpcon->private_data;
                       	if (fpcon &&
                           (fpcon->magic == FPMAGIC_ALIVE) &&
                           (fpcon->cp_sockfd == fd)) {
				ret = (*(cpcon->cp_http_header_received)) 
					(cpcon->private_data, p, len, content_length);
                        }
                        else {
                        	glob_cp_fpcon_closed_4++;
                                return FR_ASK_TO_CLOSE;
                        }
                }
		else {
			ret = (*(cpcon->cp_http_header_received)) (cpcon->private_data, p, len, content_length);
		}
	}
	else {
		ret = -1; // why not set cp_http_header_received by OM/Tunnel ?
	}
	switch (ret) {
	case -1: /* Socket close */
		cpcon->http.cb_totlen = 0; 
		return FR_ASK_TO_CLOSE;
	case -2: /* EAGAIN */
		// EAGAIN state is set only for tunnel case
		CPCON_SET_STATE(cpcon, CP_FSM_GET_SENT_EAGAIN);	
		return FR_OK;
	case -3: /* Fail to get Mutex locker */
		return FR_OK;
	case 1:
		/*
		 * BUG 3479.
		 *
		 * The HTTP response is non-cacheable response.
		 * Use tunnel to send data back.
		 * However for any reason, if HTTP does not callback fp_make_tunnel()
		 * to pick up this cpcon, we will timeout the cpcon.
		 * By that time, we should not callback any httpbody_received and
		 * socket close functions.
		 */
		cpcon->cp_httpbody_received = NULL;
		cpcon->cp_closed = NULL;
		glob_cp_gdb_no_callback_2 ++;
		SET_CPCON_FLAG(cpcon, CPF_WAIT_FOR_TUNNEL);
		assert(cpcon->http.cb_totlen > 0);
		/* BUG 3742: we only can return task at this point */
		call_om_close_conn(cpcon->private_data);
		return FR_OK;

	case 2:
		/* BZ 3442 , If the returned value is 2 , 
		 * just close the session and add socket to conn pool
		 */
		SET_CPCON_FLAG(cpcon, CPF_HTTP_END_DETECTED);
		cpcon->expected_len = 0;
		cpcon->cp_httpbody_received = NULL;
		cp_internal_close_conn(fd, cpcon);
		return FR_CPCON_CLOSED; 
	case 3:
	default: /* successfully done */
		/* if(ret==0) */
		return cp_get_sent_continue_after_callback(fd, cpcon);
	}
}

/*
 *  This function is called during tunnel egain case
 *
 */
static fsm_ret cp_get_sent_eagain(int fd, cpcon_t *cpcon)
{
	int ret;
	// check for tunnel type
	if (cpcon->type != CP_APP_TYPE_TUNNEL)
		return FR_OK;
	ret = cp_tunnel_server_data_to_client(cpcon->private_data);
	if (ret == -1) {
		NM_FLOW_TRACE(fd);
		cpcon->http.cb_totlen = 0;
		return FR_ASK_TO_CLOSE;
	} else if (ret == -2) {
		// EAGAIN --  already added to fhttp_epollout
		// or mutex failure -- already added to 
		return FR_OK;
	} else {
		//sent header so move to body state
		return  cp_get_sent_continue_after_callback(fd, cpcon);
	}
	return FR_OK;
}			

/*
 * infinite loop to receive HTTP body.
 */
static fsm_ret cp_http_body(int fd, cpcon_t * cpcon)
{
	char *p;
	int ret, unprocessed_datalen;
	int len, orglen;
	int shouldexit, go_get_sent_state = 0;
        fpcon_t *fpcon  = (fpcon_t *)cpcon->private_data;
	http_cb_t *phttp = &cpcon->http;
        con_t *httpcon = NULL;

	NM_EVENT_TRACE(fd, FT_CP_HTTP_BODY);
 	if(cpcon->type != CP_APP_TYPE_TUNNEL) {
	    omcon_t *ocon = cpcon->private_data;
            //Check some ocon sanity by comparing the fds.
            if(((ocon->fd == fd)) && 
	    	(ocon->comp_status == 1) && 
		(OM_CHECK_FLAG(ocon, (OMF_COMPRESS_TASKPOSTED|OMF_COMPRESS_TASKRETURN))))  {
	    	return FR_OK;
	    }
	}
       /*
        * If the response is a tunnled one and the response
        * code is 100, check whether any header exist in the
        * body part. If so call cp_get_sent to parse the header.
        */
        if ((cpcon->http.respcode == 100) && (cpcon->type == CP_APP_TYPE_TUNNEL)) {
                if (fpcon) {
                        httpcon = fpcon->httpcon;
                        if (httpcon) {
                                http_accesslog_write(httpcon) ;
                                /*
                                * Resetting the response header length to 0,
                                * without which the 2nd response header detail
                                * updation to httpcon->http will not happen in
                                * 'http_build_res_common'.
                                */
                                httpcon->http.res_hdlen = 0;
                                if (CHECK_HTTP_FLAG(&httpcon->http, HRF_100_CONTINUE)) {
                                        go_get_sent_state = 1;
                                }
                        }
                }
                /*
                * Header present, start parsing the header.
                */
                if (go_get_sent_state) {
                        unprocessed_datalen = phttp->cb_totlen - phttp->cb_offsetlen;
                        if (unprocessed_datalen > 0 ) {
                                phttp->cb_totlen -= phttp->cb_offsetlen;
                                memmove(&phttp->cb_buf[0], &phttp->cb_buf[phttp->cb_offsetlen], phttp->cb_totlen);
                                phttp->cb_offsetlen = 0;
                        } else {
                                phttp->cb_totlen = 0;
                                phttp->cb_offsetlen = 0;
                        }
                        phttp->http_hdr_len = 0;
			httpcon->sent_len = 0;
			reset_http_header(&cpcon->http.hdr, 0 ,0);
                        CPCON_SET_STATE(cpcon, CP_FSM_GET_SENT);
			CLEAR_HTTP_FLAG(&cpcon->http, HRF_PARSE_DONE);
			/* Since we've updated the state to process header the HTTP 
			 * side response buffer needs to be freed to enable it for 
			 * re-init and for further use.
			 */
			if (httpcon->http.resp_buf) {
				free(httpcon->http.resp_buf);
				httpcon->http.resp_buf = 0;
			}

                        return cp_get_sent(fd, cpcon);
                }
        }

	/*
	 * Loop until all data has been consumed by application.
	 */
	while(1) {
		unprocessed_datalen = cpcon->http.cb_totlen - cpcon->http.cb_offsetlen;
		if( unprocessed_datalen <= 0) break;
		p = &cpcon->http.cb_buf[cpcon->http.cb_offsetlen];

		if(CHECK_CPCON_FLAG(cpcon, CPF_TE_CHUNKED)) {
		   /*
		    * When chunked response and expected_len == 0,
		    * We need to calculate the chunked size and store
		    * chunked size + chunked line length 
		    * into expected_len memory location.
		    */
		   if( cpcon->expected_len == 0) {
		   	int totlen = unprocessed_datalen;
			// find out the chunked size.
			if (CHECK_CPCON_FLAG(cpcon, CPF_TE_CHUNKED_TRAIL)) {
				/* look into Chunked Trail yet */
				if(totlen<2) return FR_OK; // need more data
				if ((*p=='\r') && (*(p+1)=='\n')) {
					cpcon->expected_len = 2;
					UNSET_CPCON_FLAG(cpcon, CPF_TE_CHUNKED);
					UNSET_CPCON_FLAG(cpcon, CPF_TE_CHUNKED_TRAIL);
				}
				else {
					char * p2;
					len = totlen;
					p2 = p;
					while(len>0) {
						if(*p2=='\n') { break; }
						p2++; len--;
					}
					if (len==0) {
						/* BZ 3438
						 * if trailer header end is not found
						 * within cb_max_buf_size, close connection.
						 */
						if (totlen == cpcon->http.cb_max_buf_size)
							return FR_ASK_TO_CLOSE;
						else
							return FR_OK; // need more data
					}
					p2++; len--;
					cpcon->expected_len = p2 - p;
				}
			}
			else {
				/* not look into Chunked Trail yet */
				ret = om_http_get_chunklen(p, &totlen);
				if(ret == -1) {
					/* not enough data to get chunked size */
					return FR_OK;
				}
				else if(ret == -2) {
					/* not enough data to detect end of line 
					 * BZ 3438
					 */
					return FR_ASK_TO_CLOSE;
				} else if(ret == 0) {
					// end of transaction.
					SET_CPCON_FLAG(cpcon, CPF_TE_CHUNKED_TRAIL);
				}
				cpcon->expected_len = ret
						+ cpcon->http.cb_totlen - cpcon->http.cb_offsetlen
						- totlen;
				/* Add 2 for \r\n after the chunk data.
				 * In case of last chunk and no trailer, handle it here
				 * to avoid sending last packet with just \r\n.
				 * BZ 2391
				 */
				if (ret)
					cpcon->expected_len += 2;
				else {
					if ((*(p+3)=='\r') && (*(p+4)=='\n')) {
						cpcon->expected_len += 2;
						UNSET_CPCON_FLAG(cpcon, CPF_TE_CHUNKED);
						UNSET_CPCON_FLAG(cpcon, CPF_TE_CHUNKED_TRAIL);
					}
				}
			}
		   }
		}

		/*
		 * Forwarding the data to application.
		 */
		len = (unprocessed_datalen > cpcon->expected_len) ?
			cpcon->expected_len : unprocessed_datalen;
		if(len == 0) { return FR_OK; }
		if(len < 0) { return FR_ASK_TO_CLOSE; }
		shouldexit = 0;
		orglen = len;
		if(cpcon->cp_httpbody_received) {
			ret = (*(cpcon->cp_httpbody_received)) (cpcon->private_data, p, &len, &shouldexit);
		}
		else {
			ret = -1; // why not set cp_http_header_received by OM/Tunnel ?
		}
		if(ret == -1) {
			if (CHECK_CPCON_FLAG(cpcon, CPF_REFETCH_CLOSE)) {
				cpcon->cp_httpbody_received = NULL;
				DBG_LOG(MSG, MOD_CP, " Cache index refetch response body"
						" close, cpcon=%p", cpcon);
			} 
			return FR_ASK_TO_CLOSE; 
		}
		else if(ret == -2) {
			/* EAGAIN */
			NM_del_event_epoll(fd);
		}
#if 0
 	        if(cpcon->type != CP_APP_TYPE_TUNNEL) {
			omcon_t *ocon = cpcon->private_data;
			if((ocon->comp_status == 1) && 
				(OM_CHECK_FLAG(ocon, (OMF_COMPRESS_TASKPOSTED|OMF_COMPRESS_TASKRETURN))))  {
	    			return FR_OK;
			}
		}
#endif
		/*
		 * house keeping....
		 */
		cpcon->http.cb_offsetlen += orglen - len;
		cpcon->expected_len -= orglen - len;
		if( (cpcon->expected_len == 0) && !CHECK_CPCON_FLAG(cpcon, CPF_TE_CHUNKED) )  {
			DBG_LOG(MSG, MOD_CP, "cp_httpbody_received: cpcon->expected_len=%ld", 
					cpcon->expected_len);
			// end of transaction.
			SET_CPCON_FLAG(cpcon, CPF_HTTP_END_DETECTED);
			cp_internal_close_conn(fd, cpcon);
			return FR_CPCON_CLOSED;
		}
		if (shouldexit) {
			/* OM abort the reading */
			return FR_ASK_TO_CLOSE;
		}
		if (len || (ret == -2)) {
			/* OM cannot read more data from socket */
			return FR_OK;
		}
	}
	return FR_OK;
}

/*
 * The transaction is completely done.
 */
static fsm_ret cp_close(int fd, cpcon_t * cpcon)
{
	UNUSED_ARGUMENT(cpcon);
	DBG_LOG(MSG, MOD_CP, "cp_close(fd=%d)", fd);
	return FR_ASK_TO_CLOSE;
}

/*
 * Non-existing state
 */
static fsm_ret cp_crash(int fd, cpcon_t * cpcon)
{
	UNUSED_ARGUMENT(fd);
	UNUSED_ARGUMENT(cpcon);
	// Never been here. Let's crash
	assert(0);
	return FR_ASK_TO_CLOSE;
}

/*
 * Main switch function
 * After this function is called, caller should not access cpcon any more
 * because cpcon could be freed.
 *
 * return TRUE: success
 * return FALSE: socket closed.
 */
int cp_fsm(int fd, cpcon_t * cpcon)
{
	fsm_ret ret;

	ret = cpfsm[cpcon->state].fn(fd, cpcon);
	switch(ret) {
	case FR_ASK_TO_CLOSE:
		SET_CPCON_FLAG(cpcon, CPF_CONN_CLOSE);
		cp_internal_close_conn(fd, cpcon);
		return FALSE;
	case FR_OK:
		return TRUE;
	case FR_CPCON_CLOSED:
		return FALSE;
	default:
		assert(0);
	}
}

static fsm_ret cp_send_data_to_tunnel(int fd, cpcon_t * cpcon);
static fsm_ret cp_send_data_to_tunnel(int fd, cpcon_t * cpcon)
{
	int ret;

	if (!CHECK_CPCON_FLAG(cpcon, CPF_USED_IN_FPCON)) {
		return FR_OK;
	}

	if (cpcon->state != CP_FSM_GET_SENT) {
		cp_internal_close_conn(fd, cpcon);
		return FR_OK;
	}
	/* 
	 * when socket in the event queue, cp_epollin maybe called and
	 * there sockfd could be removed from epoll loop.
	 */
	NM_EVENT_TRACE(fd, FT_CP_SEND_DATA_TO_TUNNEL);
	if (NM_add_event_epollin(fd)==FALSE) {
		cp_internal_close_conn(fd, cpcon);
		return FR_OK;
	}

	if(cpcon->http.cb_totlen == cpcon->http.cb_offsetlen) {
		/* should never happen due to revision 6293 */
		glob_cp_err_unknown_2 ++;
		cp_internal_close_conn(fd, cpcon);
		return FR_OK;
	}
	cpcon->http.cb_offsetlen = 0;
	if (cpcon->cp_http_header_received) {	// Why is cp_http_header_received NULL ?
		char * p;
		int len;

		p = &cpcon->http.cb_buf[cpcon->http.cb_offsetlen];
		len = cpcon->http.http_hdr_len;
		ret = (*(cpcon->cp_http_header_received)) (cpcon->private_data, p, len, 0);
	} else {
		ret = -1;
	}
	switch (ret) {
	case -1: /* Socket close */
		cpcon->http.cb_totlen = 0; 
		return FR_ASK_TO_CLOSE;
	//case -2: /* EAGAIN */
		/* BUG:why we didn't handlethis case*/
	case -3: /* Fail to get Mutex locker */
		return FR_OK;
	case 1:
		/* should never happen */
		assert(0);
		return FR_OK; 
	default: /* successfully done */
		/* if(ret==0) */
		return cp_get_sent_continue_after_callback(fd, cpcon);
	}
}
/*
 * end of finite state machine
 */

/* ==================================================================== */
/*
 * Connection Pool Socket Close Event Queue
 * The reason to introduce this queue is to avoid other sockets to call cp_internal_close_conn() directly.
 */
static struct cp_sce_q_t {
        struct cp_sce_q_t * next;
	int fd;
	op_type op;
	void *data;
} * g_cpevent_q[MAX_EPOLL_THREADS] =  { NULL };
static pthread_mutex_t csq_mutex[MAX_EPOLL_THREADS] =
{
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER
};
void check_out_cp_queue(int num_of_epoll);

/**
 *
 * caller should own the gnm[fd].mutex lock
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
void cp_add_event(int fd, op_type op, void * data) {
        struct cp_sce_q_t * psceq;
        int num;
        struct network_mgr * pnm;
	cpcon_t * cpcon;

	if(fd == -1) return;
        pnm = &gnm[fd];
	NM_EVENT_TRACE(fd, FT_CP_ADD_EVENT + op - OP_CLOSE);

        if( NM_CHECK_FLAG(pnm, NMF_IN_USE) ) {

		/*
		 * To Do List:
		 * While this cpcon is in this event queue.
		 * we should delete fd from kernel epoll event list.
		 * after exit this event queue, then we insert fd back
		 * into kernel epoll event list.
		 */

		cpcon = (cpcon_t *)(gnm[fd].private_data);
		if(!cpcon || cpcon->magic != CPMAGIC_ALIVE) {
			glob_err_cp_magic_not_alive ++;
			return;
		}

		/*
		 * event replacement
		 */
                if(CHECK_CPCON_FLAG(cpcon, CPF_IN_EVENT_QUEUE)) {
			glob_cp_event_replacement++;
			/* 
			 * BUG 3652
			 * if we are already in the event queue and 
			 * this op==OP_CLOSE || op==OP_CLOSE_CALLBACK
			 * we should replace old op by OP_CLOSE/OP_CLOSE_CALLBACK.
			 */
			if((op==OP_CLOSE) || (op==OP_CLOSE_CALLBACK)) {
				num = pnm->pthr->num;
               		 	pthread_mutex_lock(&csq_mutex[num]);
				psceq = g_cpevent_q[num];
				while(psceq) {
				    if(psceq->fd == fd) {

					if( (psceq->op==OP_CLOSE_CALLBACK) &&
					    (op == OP_CLOSE) ) {
					    /* counter for debugging bug 4881 */
					    glob_cp_close_overwrite_closeback ++;
					}

					if( (psceq->op!=OP_CLOSE) && 
					    (psceq->op!=OP_CLOSE_CALLBACK) ) {
					    /*
					     * BUG 4881
					     * Don't use OP_CLOSE to overwrite
					     * OP_CLOSE_CALLBACK event.
					     */
					    if(op==OP_CLOSE) {
						cpcon->cp_closed = NULL;
						cpcon->cp_httpbody_received = NULL;
						glob_cp_gdb_no_callback_3 ++;
					    }
					    DBG_LOG(MSG, MOD_CP, "fd=%d replace cp event %d by %d", 
								psceq->fd, psceq->op, op);
					    psceq->op = op;
					}
					/* 
					 * return only if the event was replaced.
					 */
					pthread_mutex_unlock(&csq_mutex[num]);
					return;
				    }
				    psceq = psceq->next;
				}
               		 	pthread_mutex_unlock(&csq_mutex[num]);
				glob_cp_gdb_insert_event ++;
				DBG_LOG(MSG, MOD_CP, "fd=%d event %d insert", fd, op);
			}
			else {
				glob_cp_gdb_ignore_event_2 ++;
				DBG_LOG(MSG, MOD_CP, "fd=%d event %d lost (2)", fd, op);
	                        return;
			}
                }

		/*
		 * add a new event
		 */
                psceq = (struct cp_sce_q_t *)nkn_malloc_type(sizeof(struct cp_sce_q_t), mod_om_sceq_q_t);
                if(!psceq) {
			glob_cp_gdb_ignore_event_3 ++;
			DBG_LOG(MSG, MOD_CP, "fd=%d event %d lost (3)", fd, op);
                        return; // We lost the socket here
                }

		SET_CPCON_FLAG(cpcon, CPF_IN_EVENT_QUEUE);
		if(op == OP_CLOSE || op == OP_CLOSE_CALLBACK) {
			CPCON_SET_STATE(cpcon, CP_FSM_CLOSE);
			if (op == OP_CLOSE) {
				cpcon->cp_closed = NULL;
				cpcon->cp_httpbody_received = NULL;
				glob_cp_gdb_no_callback_4 ++;
			}
		}

                num = pnm->pthr->num;
                pthread_mutex_lock(&csq_mutex[num]);
                psceq->next = g_cpevent_q[num];
		psceq->fd = fd;
                psceq->op = op;
                psceq->data = data;
                g_cpevent_q[num] = psceq;
                pthread_mutex_unlock(&csq_mutex[num]);
        }

        return;
}

/* this function should be called only from cp_internal_close_conn() */
static void cp_remove_event(int fd);
static void cp_remove_event(int fd) 
{
        struct cp_sce_q_t * psceq, * psceq_del;
        int num;
        struct network_mgr * pnm;

	NM_EVENT_TRACE(fd, FT_CP_REMOVE_EVENT);
        pnm = &gnm[fd];
	num = pnm->pthr->num;
	pthread_mutex_lock(&csq_mutex[num]);

	psceq = g_cpevent_q[num];
	if(psceq == NULL) {
		/* 
		 * BUG 3664
		 * This could happen if cp_timeout() and
		 * check_out_cp_queue() calls in the same time.
		 * check_out_cp_queue() has already dequeued from
		 * g_cpevent_q[] and wait for pnm->lock.
		 * At this time cp_timeout() is called.
		 */
		pthread_mutex_unlock(&csq_mutex[num]);
		return;
	}

	if(psceq->fd == fd) {
		g_cpevent_q[num] = psceq->next;
		pthread_mutex_unlock(&csq_mutex[num]);
		free(psceq);
		return;
	}

	while(psceq->next) {
		if(psceq->next->fd == fd) {
			psceq_del = psceq->next;
			psceq->next = psceq_del->next;
			free(psceq_del);
			break;
		}
		psceq = psceq->next;
	}
	pthread_mutex_unlock(&csq_mutex[num]);
	return;
}

void check_out_cp_queue(int num_of_epoll) {
        struct cp_sce_q_t * psceq;
	cpcon_t * cpcon;
	struct network_mgr * pnm;
	fsm_ret ret;
	int rv;

again:
	pthread_mutex_lock(&csq_mutex[num_of_epoll]);
        while (g_cpevent_q[num_of_epoll]) {
                psceq = g_cpevent_q[num_of_epoll];
                g_cpevent_q[num_of_epoll] = g_cpevent_q[num_of_epoll]->next;
		pthread_mutex_unlock(&csq_mutex[num_of_epoll]);//need release now

		/*
		 * All network tasks should get mutex locker first.
		 */
		pnm = &gnm[psceq->fd];
		pthread_mutex_lock(&pnm->mutex);
		NM_TRACE_LOCK(pnm, LT_OM_CONNPOLL);
		if( NM_CHECK_FLAG(pnm, NMF_IN_USE) ) {
			NM_EVENT_TRACE(psceq->fd, FT_CHECK_OUT_CP_QUEUE);
			cpcon = (cpcon_t *)(gnm[psceq->fd].private_data);
			if (cpcon && (cpcon->magic == CPMAGIC_ALIVE)) {
			UNSET_CPCON_FLAG(cpcon, CPF_IN_EVENT_QUEUE);
			switch(psceq->op) {
			case OP_CLOSE_CALLBACK:
			case OP_CLOSE:
				cp_internal_close_conn(psceq->fd, cpcon);
				break;
			case OP_EPOLLOUT:
				cp_epollout(psceq->fd, (void *)cpcon);
				break;
			case OP_TUNNEL_READY:
				ret = cp_send_data_to_tunnel(psceq->fd, cpcon);
				switch(ret) {
				case FR_ASK_TO_CLOSE:
					SET_CPCON_FLAG(cpcon, CPF_CONN_CLOSE);
					cp_internal_close_conn(psceq->fd, cpcon);
					break;
				case FR_OK:
				case FR_CPCON_CLOSED:
					break;
				default:
					assert(0);
				}
				break;
			case OP_TIMER_CALLBACK:
				if (cpcon && cpcon->cp_timer) {
					rv = (*cpcon->cp_timer)(
						cpcon->private_data, 
					       	(net_timer_type_t)psceq->data);
					if (rv) { // Close connection
						cp_internal_close_conn(
							psceq->fd, cpcon);
					}
				}
				break;
			default:
				assert(0);
			}
			}
		}
		NM_TRACE_UNLOCK(pnm, LT_OM_CONNPOLL);
		pthread_mutex_unlock(&pnm->mutex);

                free(psceq);
		goto again;
        }
	pthread_mutex_unlock(&csq_mutex[num_of_epoll]);
}


/* ==================================================================== */
/*
 * Connection Pool Interface
 * The following functions are designed for normal http socket.
 * we need to provde epollin/epollout/epollerr/epollhup functions
 */

/*
 * To close connection pool socket, we should call cp_internal_close_socket()
 * cp_epollin() should never return FALSE
 * When cp_epollin() returns FALSE, it will call NM_close_socket() only.
 */
static int cp_epollin(int fd, void * private_data);
static int cp_epollin(int fd, void * private_data)
{
	cpcon_t * cpcon = (cpcon_t *)private_data;
	http_cb_t * phttp = &cpcon->http;
	int n, rlen;
	int ret = FALSE;
	char * p;

	NM_EVENT_TRACE(fd, FT_CP_EPOLLIN);
	if(CHECK_CPCON_FLAG(cpcon, CPF_IN_EVENT_QUEUE)) {
		if(!NM_CHECK_FLAG(&gnm[fd], NMF_IN_EPOLLOUT)) {
			NM_FLOW_TRACE(fd);
			NM_del_event_epoll(fd);
		}
		return TRUE;
	}

	// Reset the offset if needed
	if(phttp->cb_totlen == phttp->cb_offsetlen) {
		phttp->cb_totlen=0;
		phttp->cb_offsetlen=0;
	}
	else if ((phttp->cb_totlen - phttp->cb_offsetlen) <= 16) {
		/* 
		 * For chunked response, few bytes are left at end of buffer
		 * and unable to proceed further without decoding chunk length.
		 * So move the left over bytes if less than 16 to beginning of
		 * buffer and read further data from network.
		 * BZ 6798
		 */

		/* Move the data */
		phttp->cb_totlen -= phttp->cb_offsetlen;
		memmove(&phttp->cb_buf[0],
			&phttp->cb_buf[phttp->cb_offsetlen],
			phttp->cb_totlen);
		phttp->cb_offsetlen=0;
	}

	if ((cpcon->state == CP_FSM_GET_SENT) ||
		(cpcon->state == CP_FSM_GET_SENT_EAGAIN)) {
		rlen = nkn_check_http_cb_buf(phttp);
		if (rlen == -1) {
			NM_FLOW_TRACE(fd);
			cp_internal_close_conn(fd, cpcon);
			DBG_LOG(MSG, MOD_CP,
				"Close socket due to run out of buffer, id=%d",
				fd);
			return TRUE;
		}
		// Temporary hack, Leave space for NULL
		rlen--;
	}
	else {
		rlen = phttp->cb_max_buf_size - phttp->cb_totlen;
	}
	if(rlen == 0) {
		int unprocessed_bytes = phttp->cb_totlen - phttp->cb_offsetlen;
		if (unprocessed_bytes) {
			if(cp_fsm(fd, cpcon)==FALSE) {
				/* Socket has already been closed and cpcon freed inside 
				 * cp_fsm call. Refer to bug 7258.
				 */
				return TRUE;
			}
		}
		if (!NM_CHECK_FLAG(&gnm[fd], NMF_IN_EPOLLOUT) &&
			(unprocessed_bytes == (phttp->cb_totlen - phttp->cb_offsetlen))) {
			NM_FLOW_TRACE(fd);
			NM_del_event_epoll(fd);
		}
		return TRUE;
	}

	p = &cpcon->http.cb_buf[cpcon->http.cb_totlen];
	n = recv(fd, p, rlen, MSG_DONTWAIT);
	if(n <= 0) {
		DBG_LOG(MSG, MOD_CP, "recv(fd=%d) n=%d errno=%d ",
			fd, n, errno);
		if(n==-1 && errno==EAGAIN) {
			NM_FLOW_TRACE(fd);
			return TRUE;
		}
		/*
		 * BUG 3341:
		 * if we have data left in http.cb_buf and Origin server wants to close the socket,
		 * we should leave enough time for HTTP client to come back to fetch the data,
		 * before closing the socket.
		 * how about wait for 5 second time here.
		 */
		if(cpcon->http.cb_totlen) {
			if ((cpcon->state == CP_FSM_GET_SENT) ||
				(cpcon->state == CP_FSM_GET_SENT_EAGAIN)) {
				SET_CPCON_FLAG(cpcon, CPF_ORIGIN_CLOSE);
				cp_fsm(fd, cpcon);
				return TRUE;
			}
			if(!NM_CHECK_FLAG(&gnm[fd], NMF_IN_EPOLLOUT)) {
				NM_FLOW_TRACE(fd);
				NM_del_event_epoll(fd);
			}
			NM_change_socket_timeout_time(&gnm[fd], 1);	
			SET_CPCON_FLAG(cpcon, CPF_CONN_CLOSE);
			NM_FLOW_TRACE(fd);
			return TRUE;
		}
		else {
			NM_FLOW_TRACE(fd);
			CPCON_SET_STATE(cpcon, CP_FSM_CLOSE);
			SET_CPCON_FLAG(cpcon, CPF_CONN_CLOSE);
		}
	} else /* if(n > 0) */ {
		// valid data
		cpcon->http.cb_totlen += n;
		AO_fetch_and_add(&glob_origin_recv_tot_bytes, n);
		if (cpcon->nsconf) {
			NS_UPDATE_STATS(cpcon->nsconf, PROTO_HTTP, server, in_bytes, n);
		}
		if (CHECK_CPCON_FLAG(cpcon, CPF_UNRESP_CONNECTION)) {
			    int32_t ct_ht_index;
			    ct_ht_index = cp_tracker_hash_func(&(cpcon->remote_ipv4v6));
			    pthread_mutex_lock(&cp_tracker_mutex[ct_ht_index]);
			    cp_tracker_del_unresp_connections(ct_ht_index, cpcon);
			    pthread_mutex_unlock(&cp_tracker_mutex[ct_ht_index]);
			    UNSET_CPCON_FLAG(cpcon, CPF_UNRESP_CONNECTION);
		}
	}
	ret = cp_fsm(fd, cpcon);
	if(ret == TRUE) {
		if (!CHECK_CPCON_FLAG(cpcon, CPF_WAIT_FOR_TUNNEL)) {
			if (CHECK_HTTP_FLAG(&cpcon->http, HRF_PARSE_DONE)) {
				if (CHECK_CPCON_FLAG(cpcon, CPF_RESUME_TASK)) {
					NM_FLOW_TRACE(fd);
					cp_internal_close_conn(fd, cpcon);
				}
			}
		}	
	}
	return TRUE;
}

/*
 * To close connection pool socket, we should call cp_internal_close_socket()
 * cp_epollout() should never return FALSE
 * When cp_epollout() returns FALSE, it will call NM_close_socket() only.
 *
 * One funny point is:
 * when application type is CP_APP_TYPE_OM: sentout data is stored in cpcon.
 * when application type is CP_APP_TYPE_TUNNEL: sentout data is stored in httpcon.
 *
 */
static int cp_epollout(int fd, void * private_data)
{
	cpcon_t * cpcon = (cpcon_t *)private_data;
	char buf[10];
	int len, ret;

	NM_EVENT_TRACE(fd, FT_CP_EPOLLOUT);
	if(CHECK_CPCON_FLAG(cpcon, CPF_IN_EVENT_QUEUE)) {
		NM_FLOW_TRACE(fd);
		NM_del_event_epoll(fd);
		return TRUE;
	}

	switch(cpcon->state) {
	case CP_FSM_HTTP_BODY:
		if( (CHECK_CPCON_FLAG(cpcon, CPF_METHOD_NOT_GET)) &&
		    ( cpcon->type == CP_APP_TYPE_TUNNEL) ) {
			goto next;
		}

	case CP_FSM_SYN_SENT:
		if (cp_fsm(fd, cpcon)==FALSE) {
			NM_FLOW_TRACE(fd);
			/* Socket already closed by cp_fsm() function */
			return TRUE;
		}

		if (NM_add_event_epollin(fd)==FALSE) {
			NM_FLOW_TRACE(fd);
			/* epoll call failed, close socket. */
			cp_internal_close_conn(fd, cpcon);
			return TRUE;
		}

		if( CHECK_CPCON_FLAG(cpcon, CPF_METHOD_NOT_GET) ) {
			return TRUE;
		}

		if(cpcon->state == CP_FSM_SYN_SENT) {
			break;
		}

		/* 
		 * Since we have already received the epollout event,
		 * we should call cp_epollin function to recv any data
		 * which is already in the linuex kernel recv buffer.
		 *
		 * If we do not read it, there is a chance we could hang.
		 * e.g. if these data is the last few bytes of transaction,
		 * we will not get any epollin event any more.
		 *
		 * However we only can peek the socket.
		 * If no data in the buffer and we call cp_epollin.
		 * cp_epollin recv() could return 0.
		 */
		len=10;
		if (recv(fd, buf, len, MSG_PEEK) > 0) {
			cp_epollin(fd, private_data);
		}
		break;

	case CP_FSM_GET_SENT:
		if(/* (!CHECK_CPCON_FLAG(cpcon, CPF_METHOD_NOT_GET)) || */ 
		   /* remove because of BUG 6940 */
		    ( cpcon->type != CP_APP_TYPE_TUNNEL) ) {
			break;
		}
next:
		if (NM_add_event_epollin(fd)==FALSE) {
			NM_FLOW_TRACE(fd);
			/* epoll call failed, close socket. */
			cp_internal_close_conn(fd, cpcon);
			return TRUE;
		}

		ret = cp_tunnel_client_data_to_server(cpcon->private_data);
		if(ret == -1) {
			NM_FLOW_TRACE(fd);
			cp_internal_close_conn(fd, cpcon);
			return TRUE;
		}
		else if(ret == -2) {
			/* EAGAIN; */
			cp_add_event(fd, OP_EPOLLOUT, NULL);
			return TRUE;
		}

		if ((cpcon->http.cb_totlen - cpcon->http.cb_offsetlen) > 0) {
			if (cp_fsm(fd, cpcon) == FALSE) {
				NM_FLOW_TRACE(fd);
				return TRUE;
			}
		}

		return TRUE;

	case CP_FSM_GET_SENT_EAGAIN:  // tunnel EAGAIN case
		if (cpcon->type != CP_APP_TYPE_TUNNEL)
			break;
		ret = cp_tunnel_server_data_to_client(cpcon->private_data);
		if (ret == -1) {
			NM_FLOW_TRACE(fd);
			cpcon->http.cb_totlen = 0;
			SET_CPCON_FLAG(cpcon, CPF_CONN_CLOSE);
                	cp_internal_close_conn(fd, cpcon);
		} else if (ret == -2) {
			// EAGAIN --  already added to fhttp_epollout
			// or mutex failure -- already added to 
			break;
		} else {
			//sent header so move to body state
			ret = cp_get_sent_continue_after_callback(fd, cpcon);
		        switch(ret) {
        			case FR_ASK_TO_CLOSE:
                			SET_CPCON_FLAG(cpcon, CPF_CONN_CLOSE);
                			cp_internal_close_conn(fd, cpcon);
					break;
        			case FR_OK:
        			case FR_CPCON_CLOSED:
                			break;
        			default:
                			assert(0);
			}
        	}
		return TRUE;
	default:
		break;
	}

	return TRUE;
}

static int cp_epollerr(int fd, void * private_data);
static int cp_epollerr(int fd, void * private_data)
{
	cpcon_t * cpcon = (cpcon_t *)private_data;

	NM_EVENT_TRACE(fd, FT_CP_EPOLLERR);
	if(CHECK_CPCON_FLAG(cpcon, CPF_IN_EVENT_QUEUE)) {
		// Memory leak here?
		return TRUE;
	}

	DBG_LOG(MSG, MOD_CP, "cp_epollerr: fd=%d state=%d", fd, cpcon->state);
	if (cpcon->state == CP_FSM_SYN_SENT) {
		if(cpcon->cp_connect_failed) {
			(*(cpcon->cp_connect_failed)) (cpcon->private_data, OM_EPOLL_EVENT);
		}
		NM_FLOW_TRACE(fd);
		cp_internal_close_conn(fd, cpcon);
		return TRUE;
	}

	CPCON_SET_STATE(cpcon, CP_FSM_CLOSE);
	cp_fsm(fd, cpcon);
	return TRUE;
}

static int cp_epollhup(int fd, void * private_data);
static int cp_epollhup(int fd, void * private_data)
{
	NM_EVENT_TRACE(fd, FT_CP_EPOLLHUP);
	return cp_epollerr(fd, private_data);
}

static int cp_timer(int fd, void * private_data, net_timer_type_t type);
static int cp_timer(int fd, void * private_data, net_timer_type_t type)
{
	UNUSED_ARGUMENT(private_data);

	NM_EVENT_TRACE(fd, FT_CP_TIMER);
	cp_add_event(fd, OP_TIMER_CALLBACK, (void *)type);
	return 0;
}

static int cp_timeout(int fd, void * private_data, double timeout);
static int cp_timeout(int fd, void * private_data, double timeout)
{
	cpcon_t *cpcon = (cpcon_t *)private_data;

	NM_EVENT_TRACE(fd, FT_CP_TIMEOUT);
	UNUSED_ARGUMENT(fd);
	UNUSED_ARGUMENT(timeout);

	// Check for connect timeout case.
	if (http_cfg_fo_use_dns && (cpcon->state == CP_FSM_SYN_SENT)) {
		if(cpcon->cp_connect_failed) {
			(*(cpcon->cp_connect_failed)) (cpcon->private_data, OM_EPOLL_EVENT);
		}
		NM_FLOW_TRACE(fd);
	}

	SET_CPCON_FLAG(cpcon, CPF_CONN_TIMEOUT);

	/*
	 * TBD: We should be using cp_add_event() to handle the timeout
	 *	since we are _not_ running on a net thread.
	 *
	 * Because it is timeout, 
	 * we also should not put this connection back to connection pool.
	 */
	CPCON_SET_STATE(cpcon, CP_FSM_CLOSE);
	SET_CPCON_FLAG(cpcon, CPF_CONN_CLOSE);
	cp_internal_close_conn(fd, cpcon);
	return TRUE;
}


/* ==================================================================== */
static uint32_t cp_tracker_hash_func(ip_addr_t *ipv4v6)
{
	// todo ipv6. This needs to be heavily optimized.
	uint32_t ct_ht_index = 0;
	unsigned int lower1_ipv6 = 0;
	unsigned int lower2_ipv6 = 0;
	unsigned int higher1_ipv6 = 0;
	unsigned int higher2_ipv6 = 0;

	if (ipv4v6->family == AF_INET6) {

                memcpy(&higher1_ipv6, (unsigned char*)&IPv6((*ipv4v6)), sizeof(unsigned int));
                memcpy(&higher2_ipv6, (unsigned char*)&IPv6((*ipv4v6))+4, sizeof(unsigned int));
                memcpy(&lower1_ipv6, (unsigned char*)&IPv6((*ipv4v6))+8, sizeof(unsigned int));
                memcpy(&lower2_ipv6, (unsigned char*)&IPv6((*ipv4v6))+12, sizeof(unsigned int));

                ct_ht_index += (higher1_ipv6 & 0xFFFFFFFF);

                ct_ht_index += (higher2_ipv6 & 0xFFFFFFFF);

                ct_ht_index += (lower1_ipv6 & 0xFFFFFFFF);

                ct_ht_index += (lower2_ipv6 & 0xFFFFFFFF);
	} else {
		ct_ht_index += (IPv4((*ipv4v6)) & 0xFFFFFFFF);
	}

	return ct_ht_index % MAX_CP_TRACKER;
}

static int cp_tracker_del_unresp_connections(uint32_t ct_ht_index, cpcon_t *cpcon)
{
	struct cp_tracker *pcpt, *pcpt_match;
	ip_addr_t *ipv4v6;
	int found = 0;

	ipv4v6 = &(cpcon->remote_ipv4v6);
	pcpt = g_pcp_tracker[ct_ht_index];

	if (pcpt) {
		if (ipv4v6->family == AF_INET) {
			if (IPv4(pcpt->ipv4v6) == IPv4((*ipv4v6))) {
				pcpt->unresp_connections--;
				found = 1;
			}
		} else {
			if (memcmp(&(pcpt->ipv4v6), ipv4v6, sizeof(ip_addr_t))) {
				pcpt->unresp_connections--;
				found = 1;
			}
		}
		if (found) {
			if (pcpt->unresp_connections == 0) {
				g_pcp_tracker[ct_ht_index] = pcpt->next;
				free(pcpt);
			}
			return TRUE;
		}
	} else {
		return FALSE;
	}
	while (pcpt->next) {
		if (ipv4v6->family == AF_INET) {
			if (IPv4(pcpt->next->ipv4v6) == IPv4((*ipv4v6))) {
				pcpt_match = pcpt->next;
				pcpt->unresp_connections--;
				found = 1;
			}
		} else {
			if (memcmp(&(pcpt->next->ipv4v6), ipv4v6, sizeof(ip_addr_t))) {
				pcpt_match = pcpt->next;
				pcpt->unresp_connections--;
				found = 1;
			}
		}
		if (found) {
			if (pcpt->unresp_connections == 0) {
				pcpt->next = pcpt_match->next;
				free(pcpt_match);
			}
			return TRUE;
		}
		pcpt = pcpt->next;
	}

	return TRUE;
}

static int cp_tracker_check_and_add_unresp_connections(uint32_t ct_ht_index, cpcon_t *cpcon)
{
	struct cp_tracker *pcpt, *pcpt_match;
	ip_addr_t *ipv4v6;
	int memory_allocated = 0;

	pcpt = g_pcp_tracker[ct_ht_index];
	ipv4v6 = &(cpcon->remote_ipv4v6);
	if (pcpt) {
		if (ipv4v6->family == AF_INET) {
			if (IPv4(pcpt->ipv4v6) == IPv4((*ipv4v6))) {
				pcpt_match = pcpt;
				goto found;
			}
		} else {
			if (memcmp(&(pcpt->ipv4v6), ipv4v6, sizeof(ip_addr_t))) {
				pcpt_match = pcpt;
				goto found;
			}
		}
	} else {
		goto new_origin;
	}
	while (pcpt->next) {
		if (ipv4v6->family == AF_INET) {
			if (IPv4(pcpt->next->ipv4v6) == IPv4((*ipv4v6))) {
				pcpt_match = pcpt->next;
				goto found;
			}
		} else {
			if (memcmp(&(pcpt->next->ipv4v6), ipv4v6, sizeof(ip_addr_t))) {
				pcpt_match = pcpt->next;
				goto found;
			}
		}
		pcpt = pcpt->next;
	}
new_origin:
	pcpt = (struct cp_tracker *) nkn_malloc_type(sizeof(struct cp_tracker), mod_om_socket_pool_t);
	if (pcpt == NULL) {
		return FALSE;
	}
	memcpy(&(pcpt->ipv4v6), ipv4v6, sizeof(ip_addr_t));
	pcpt->unresp_connections = 0;
	pcpt_match = pcpt;
	memory_allocated = 1;

	pcpt->next = g_pcp_tracker[ct_ht_index];
	g_pcp_tracker[ct_ht_index] = pcpt;
found:
	if ((pcpt_match->unresp_connections + 1) <= max_unresp_connections) {
		pcpt_match->unresp_connections++;
	} else {
		if (memory_allocated) free(pcpt);
		return FALSE;
	}

	return TRUE;
}
/*
 * socket connection pool.
 */
#define MAX_SOCKET_POOL 10240
#define CPSP_MAGIC_DEAD 0xdead0003
struct cp_socket_pool {
	uint32_t magic;
	//struct cp_socket_pool * next;
	TAILQ_ENTRY(cp_socket_pool) entry;

	ip_addr_t src_ipv4v6; // network order
	ip_addr_t ipv4v6; // network order
	uint16_t port; // network order
	int fd;
};
TAILQ_HEAD(cp_socket_pool_head, cp_socket_pool) g_pcp_hashtable[MAX_SOCKET_POOL];
static pthread_mutex_t cp_sp_mutex = PTHREAD_MUTEX_INITIALIZER;

void cp_init(void);
void cp_init()
{
	int i;

	for (i=0; i<MAX_SOCKET_POOL; i++) {
    		TAILQ_INIT(&g_pcp_hashtable[i]);
	}

	for (i=0; i<MAX_CP_TRACKER; i++) {
		g_pcp_tracker[i] = NULL;
		pthread_mutex_init(&cp_tracker_mutex[i], NULL);
	}

	return;
}

/*
 * input: ip/port are in network order.
 */
static uint16_t cpsp_hashtable_func(ip_addr_t* src_ipv4v6, ip_addr_t* ipv4v6, uint16_t port);
static uint16_t cpsp_hashtable_func(ip_addr_t* src_ipv4v6, ip_addr_t* ipv4v6, uint16_t port)
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
	return ht_index % MAX_SOCKET_POOL;
}

/*
 * input: ip/port are in network order.
 */
static int cpsp_epoll_rm_fm_pool(int fd, void * private_data);
static int cpsp_epoll_rm_fm_pool(int fd, void * private_data)
{
	uint16_t ht_index;
	struct cp_socket_pool * psp, *tmp_psp;
	struct cp_socket_pool * del_psp;
	cpcon_t * cpcon = (cpcon_t *)private_data;

	NM_EVENT_TRACE(fd, FT_CP_EPOLL_RM_FM_POOL);
	/*
	 * Try to find it from keep-alive connection pool first.
	 */
	pthread_mutex_lock(&cp_sp_mutex);
        ht_index = cpsp_hashtable_func(&(cpcon->src_ipv4v6),
                                        &(cpcon->remote_ipv4v6),
                                        cpcon->remote_port);

	psp = TAILQ_FIRST(&g_pcp_hashtable[ht_index]);
	if(!psp) {
		goto notfound;
	}
	if(psp->fd == fd) {
		TAILQ_REMOVE(&g_pcp_hashtable[ht_index], psp, entry);
		if (CHECK_CPCON_FLAG(cpcon, CPF_IS_IPV6)) {
			AO_fetch_and_sub1(&glob_cp_tot_sockets_in_pool_ipv6);
		} else {
			AO_fetch_and_sub1(&glob_cp_tot_sockets_in_pool);
		}
		pthread_mutex_unlock(&cp_sp_mutex);
		free(psp);
		goto done;
	}
	TAILQ_FOREACH_SAFE(psp, &g_pcp_hashtable[ht_index], entry, tmp_psp) {
		if(psp->fd == fd) {
			TAILQ_REMOVE(&g_pcp_hashtable[ht_index], psp, entry);
			if (CHECK_CPCON_FLAG(cpcon, CPF_IS_IPV6)) {
				AO_fetch_and_sub1(&glob_cp_tot_sockets_in_pool_ipv6);
			} else {
				AO_fetch_and_sub1(&glob_cp_tot_sockets_in_pool);
			}
			pthread_mutex_unlock(&cp_sp_mutex);
			free(psp);
			goto done;
		}
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
	glob_cp_reused_same_time ++;
	DBG_LOG(MSG, MOD_CP, "already picked up by MM thread");
	pthread_mutex_unlock(&cp_sp_mutex);
	/* then follow down */

done:
	if (CHECK_CPCON_FLAG(cpcon, CPF_TRANSPARENT_REQ)) {
	    cp_release_transparent_port(cpcon);
	}
	NM_close_socket(fd);
	shutdown_http_header(&cpcon->http.hdr);
	cpcon->magic = CPMAGIC_DEAD;
	if (CHECK_CPCON_FLAG(cpcon, CPF_IS_IPV6)) {
		AO_fetch_and_sub1(&glob_cp_tot_cur_open_sockets_ipv6);
		AO_fetch_and_add1(&glob_cp_tot_closed_sockets_ipv6);
		DBG_LOG(MSG, MOD_CP, "socket %d in connection pool deleted, cp_tot_cur_open_ipv6=%ld", 
			fd, glob_cp_tot_cur_open_sockets_ipv6);
	} else {
		AO_fetch_and_sub1(&glob_cp_tot_cur_open_sockets);
		AO_fetch_and_add1(&glob_cp_tot_closed_sockets);
		DBG_LOG(MSG, MOD_CP, "socket %d in connection pool deleted, cp_tot_cur_open=%ld", 
			fd, glob_cp_tot_cur_open_sockets);
	}

	free(cpcon->http.cb_buf);
	if(cpcon->oscfg.ip) free(cpcon->oscfg.ip);
	free(cpcon);

	return TRUE;
}

static int cpsp_timeout(int fd, void * private_data, double timeout);
static int cpsp_timeout(int fd, void * private_data, double timeout)
{
	UNUSED_ARGUMENT(timeout);
	glob_cp_tot_keep_alive_idleout_sockets++;
	cpsp_epoll_rm_fm_pool(fd, private_data);
	return TRUE;
}

static int cpsp_add_into_pool(int fd, cpcon_t * cpcon);
static int cpsp_add_into_pool(int fd, cpcon_t * cpcon)
{
	struct cp_socket_pool * psp;
	uint16_t ht_index;
	char * cb_buf;
	int cb_max_buf_size;
	http_cb_t * phttp;

	if(cp_enable == 0) {
		return FALSE;
	}
	NM_EVENT_TRACE(fd, FT_CP_ADD_INTO_POOL);

	/* Check the Configured maximum number of connections that will be
	 * opened to the origin server concurrently. Default is 256. If set to 0, then
	 * there is no limit (demand driven). Maximum allowed value is 2048.
	 */

	if ( ( (AO_load(&glob_cp_tot_sockets_in_pool) +
			AO_load(&glob_cp_tot_sockets_in_pool_ipv6)) >= nkn_cfg_tot_socket_in_pool) &&
			(nkn_cfg_tot_socket_in_pool != 0 ))
		return FALSE;

	/* 
	 * It is a keep-alive socket. We can reuse the connection.
	 */
	psp = (struct cp_socket_pool *) nkn_malloc_type(sizeof(struct cp_socket_pool), mod_om_socket_pool_t);
	if(!psp) {
		return FALSE;
	}

	// Fill in psp structure
	psp->magic = 0;
	memcpy(&(psp->src_ipv4v6), &(cpcon->src_ipv4v6), sizeof(ip_addr_t));
	memcpy(&(psp->ipv4v6), &(cpcon->remote_ipv4v6), sizeof(ip_addr_t));
	psp->port = cpcon->remote_port;
	psp->fd = fd;

	// reset necessary cpcon structure fields.
	// For bug 8871, retain the CPF_TRANSPARENT_REQ alone, if present.
	cpcon->cp_flag = cpcon->cp_flag & (CPF_TRANSPARENT_REQ | CPF_IS_IPV6);
	cpcon->state = CP_FSM_FREE;

	phttp = &cpcon->http;
	shutdown_http_header(&phttp->hdr);
        cb_buf = phttp->cb_buf;
        cb_max_buf_size = phttp->cb_max_buf_size;
        memset(phttp, 0, sizeof(*phttp));
        phttp->cb_buf = cb_buf;
        phttp->cb_max_buf_size = cb_max_buf_size;
        init_http_header(&phttp->hdr, phttp->cb_buf, phttp->cb_max_buf_size);

	/* When unregister_NM_socket() is followed by register_NM_socket(), 
	 * we do not need to delete fd from epoll loop and then add back. 
	 * so specify FALSE.  This saves two system calls.
	 */
	unregister_NM_socket(fd, FALSE);

	/*
	 * Monitoring server closes the connection or timed out.
	 */
	if(register_NM_socket(fd,
		(void *)cpcon,
		cpsp_epoll_rm_fm_pool,
		cpsp_epoll_rm_fm_pool,
		cpsp_epoll_rm_fm_pool,
		cpsp_epoll_rm_fm_pool,
		NULL,
		cpsp_timeout,
		cp_idle_timeout,
		USE_LICENSE_FALSE,
		TRUE) == FALSE)
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

	if (CHECK_CPCON_FLAG(cpcon, CPF_UNRESP_CONNECTION)) {
		uint32_t ct_ht_index;
		ct_ht_index = cp_tracker_hash_func(&(cpcon->remote_ipv4v6));
		pthread_mutex_lock(&cp_tracker_mutex[ct_ht_index]);
		cp_tracker_del_unresp_connections(ct_ht_index, cpcon);
		pthread_mutex_unlock(&cp_tracker_mutex[ct_ht_index]);
		UNSET_CPCON_FLAG(cpcon, CPF_UNRESP_CONNECTION);
	}

	pthread_mutex_lock(&cp_sp_mutex);
	ht_index = cpsp_hashtable_func(&(cpcon->src_ipv4v6), 
					&(cpcon->remote_ipv4v6), cpcon->remote_port);
	TAILQ_INSERT_TAIL(&g_pcp_hashtable[ht_index], psp, entry);
	if ( CHECK_CPCON_FLAG(cpcon, CPF_IS_IPV6)) {
		AO_fetch_and_add1(&glob_cp_tot_sockets_in_pool_ipv6);
	} else {
		AO_fetch_and_add1(&glob_cp_tot_sockets_in_pool);
	}

	if (cpcon->nsconf) {
		NS_DECR_STATS(cpcon->nsconf, PROTO_HTTP, server, active_conns);
		NS_DECR_STATS(cpcon->nsconf, PROTO_HTTP, server, conns);
	}

	release_namespace_token_t(cpcon->ns_token);

	assert(cpcon->magic == CPMAGIC_ALIVE);
	cpcon->magic = CPMAGIC_IN_POOL;
	pthread_mutex_unlock(&cp_sp_mutex);

	DBG_LOG(MSG, MOD_CP, "add socket %d into connection pool", fd);

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
int cp_activate_socket_handler(int fd, cpcon_t * cpcon)
{
	struct sockaddr_in srv;
	struct sockaddr_in6 srv_v6;
	int ret;
	int http_timeout = http_idle_timeout;
	int32_t ct_ht_index = 0;
	int cp_tracker_mutex_lock = 0;
	network_mgr_t * pnm;

	pnm = &gnm[fd];

	NM_EVENT_TRACE(fd, FT_CP_ACTIVATE_SOCKET_HANDLER);
	if(cpcon->http.nsconf) {
		http_timeout = cpcon->http.nsconf->http_config->origin.http_idle_timeout;
	}
	/*
	 * It is a Keep-alive reused socket.
	 * We should not send connect again.
	 */
	if( CHECK_CPCON_FLAG(cpcon, CPF_USE_KA_SOCKET) ) {

		DBG_LOG(MSG, MOD_CP, "reused socket fd=%d", fd);
		UNSET_CPCON_FLAG(cpcon, CPF_USE_KA_SOCKET);
		/* When unregister_NM_socket() is followed by register_NM_socket(), 
	 	* we do not need to delete fd from epoll loop and then add back,
		* so specify FALSE.  This saves two system calls.
	 	*/
		unregister_NM_socket(fd, FALSE);

		if(register_NM_socket(fd,
			(void *)cpcon,
			cp_epollin,
			cp_epollout,
			cp_epollerr,
			cp_epollhup,
			cp_timer,
			cp_timeout,
			http_timeout,
			USE_LICENSE_FALSE,
			TRUE) == FALSE)
		{
			glob_cp_activate_err_1 ++;
			goto return_FALSE;
		}


		if (max_unresp_connections) {
			ct_ht_index = cp_tracker_hash_func(&(cpcon->remote_ipv4v6));
			pthread_mutex_lock(&cp_tracker_mutex[ct_ht_index]);
			ret = cp_tracker_check_and_add_unresp_connections(ct_ht_index, cpcon);
			cp_tracker_mutex_lock = 1;
			SET_CPCON_FLAG(cpcon, CPF_UNRESP_CONNECTION);
			if (ret == FALSE) {
				glob_cp_activate_err_6++;
				unregister_NM_socket(fd, TRUE);
				goto return_FALSE;
			}
			pthread_mutex_unlock(&cp_tracker_mutex[ct_ht_index]);
		}

		CPCON_SET_STATE(cpcon, CP_FSM_SYN_SENT);
		cp_add_event(fd, OP_EPOLLOUT, NULL);
		return TRUE;
	}

	/*
	 * It is NOT a keep-alive reused socket.
	 */
	DBG_LOG(MSG, MOD_CP, "new socket fd=%d", fd);
	/* Now time to make a socket connection. */
	if(cpcon->http.nsconf && (cpcon->http.nsconf->http_config->origin_request_type & DP_ORIGIN_REQ_SECURE)) {
        	bzero(&srv, sizeof(srv));
        	srv.sin_family = AF_INET;
		srv.sin_addr.s_addr = inet_addr("127.0.0.1");
                srv.sin_port = htons(ssl_listen_port);
		SET_CPCON_FLAG(cpcon, CPF_HTTPS_CONNECT);
	} else {
		if (CHECK_CPCON_FLAG(cpcon, CPF_IS_IPV6)) {
		        bzero(&srv_v6, sizeof(srv_v6));
        		srv_v6.sin6_family = AF_INET6;
	        	memcpy(&srv_v6.sin6_addr, &(cpcon->http.remote_ipv4v6.addr.v6),  
				sizeof(srv_v6.sin6_addr));
        		srv_v6.sin6_port = htons(cpcon->http.remote_port);
		} else {
        		bzero(&srv, sizeof(srv));
        		srv.sin_family = AF_INET;
			srv.sin_addr.s_addr = IPv4(cpcon->http.remote_ipv4v6);
        		srv.sin_port = htons(cpcon->http.remote_port);
		}
	}
	// Set to non-blocking socket. so it is non-blocking connect.
	NM_set_socket_nonblock(fd);

        if(register_NM_socket(fd,
			(void *)cpcon,
			cp_epollin,
			cp_epollout,
			cp_epollerr,
			cp_epollhup,
			cp_timer,
			cp_timeout,
			(http_cfg_fo_use_dns) ? http_cfg_origin_conn_timeout : http_timeout,
			USE_LICENSE_FALSE,
			TRUE) == FALSE)
	{
		glob_cp_activate_err_2 ++;
		goto return_FALSE;
	}
	
	/*
	 * For connection pool reuse, we can only set transparent mode 
	 * to new socket only.
	 */
	//pthread_mutex_lock(&tr_socket_mutex);
	if ( CHECK_CPCON_FLAG(cpcon, CPF_TRANSPARENT_REQ) &&
	     CHECK_CPCON_FLAG(cpcon, CPF_FIRST_NEW_SOCKET) )
	{
		ret = cp_set_transparent_mode_tosocket(fd, cpcon);
		if (ret == FALSE) {
			//pthread_mutex_unlock(&tr_socket_mutex);
			// NOTE: unregister_NM_socket(fd, TRUE) deletes
			// the fd from epoll loop.
			unregister_NM_socket(fd, TRUE);
			glob_cp_activate_err_3 ++;
			goto return_FALSE;
		}
	}
	UNSET_CPCON_FLAG(cpcon, CPF_FIRST_NEW_SOCKET);


	CPCON_SET_STATE(cpcon, CP_FSM_SYN_SENT);
	dbg_helper_connect_sock(0, "opening", cpcon, __FUNCTION__, __LINE__);

	if (http_cfg_fo_use_dns) {

		switch(cpcon->type) {
			case CP_APP_TYPE_OM:
			{
				con_t *request_con;
				request_con = ((omcon_t *) (cpcon->private_data))->httpreqcon;

				request_con->os_failover.num_ip_index++;
				break;
			}
			case CP_APP_TYPE_TUNNEL:
			{
				con_t *request_con;
				request_con = ((fpcon_t *) (cpcon->private_data))->httpcon;

				request_con->os_failover.num_ip_index++;
				break;
			}
			default:
				break;
		}
	}

	if (max_unresp_connections) {
		ct_ht_index = cp_tracker_hash_func(&(cpcon->remote_ipv4v6));
		pthread_mutex_lock(&cp_tracker_mutex[ct_ht_index]);
		ret = cp_tracker_check_and_add_unresp_connections(ct_ht_index, cpcon);
		SET_CPCON_FLAG(cpcon, CPF_UNRESP_CONNECTION);
		cp_tracker_mutex_lock = 1;
		if (ret == FALSE) {
			glob_cp_activate_err_6++;
			unregister_NM_socket(fd, TRUE);
			goto return_FALSE;
		}
		pthread_mutex_unlock(&cp_tracker_mutex[ct_ht_index]);
	}

	if (CHECK_CPCON_FLAG(cpcon, CPF_IS_IPV6) && !(cpcon->http.nsconf->http_config->origin_request_type & DP_ORIGIN_REQ_SECURE)) {
		ret = connect(fd, (struct sockaddr *)&srv_v6, sizeof(srv_v6));
	} else {
		ret = connect(fd, (struct sockaddr *)&srv, sizeof(struct sockaddr));
	}
	//pthread_mutex_unlock(&tr_socket_mutex);
	if(ret < 0)
	{
		if(errno == EINPROGRESS) {
			DBG_LOG(MSG, MOD_CP, "connect(0x%x) fd=%d in progress", 
						IPv4(cpcon->http.remote_ipv4v6), fd);
			if ((NM_add_event_epoll_out_err_hup(fd)==FALSE)) { 
				// NOTE: unregister_NM_socket(fd, TRUE) deletes
				// the fd from epoll loop.
				unregister_NM_socket(fd, TRUE);
				glob_cp_activate_err_4 ++;
				goto return_FALSE;
			}
			return TRUE;
		}

		if (errno == EADDRNOTAVAIL) {
		    dbg_helper_connect_sock(errno, "failed", cpcon, __FUNCTION__, __LINE__);
		}
		DBG_LOG(MSG, MOD_CP, "connect(0x%x) fd=%d failed, errno=%d", 
					IPv4(cpcon->http.remote_ipv4v6), fd, errno);
		if (cpcon->cp_connect_failed) {
			(*(cpcon->cp_connect_failed)) (cpcon->private_data, CLIENT_EPOLL_EVENT);
		}
		// NOTE: unregister_NM_socket(fd, TRUE) deletes
		// the fd from epoll loop.
		unregister_NM_socket(fd, TRUE);
		glob_cp_activate_err_5 ++;
		goto return_FALSE;
	}

	/*
	 * Socket has been established successfully by now.
	 */
	cp_add_event(fd, OP_EPOLLOUT, NULL);
	return TRUE;

return_FALSE:
	if (CHECK_CPCON_FLAG(cpcon, CPF_UNRESP_CONNECTION)) {
		if (cp_tracker_mutex_lock) {
			cp_tracker_del_unresp_connections(ct_ht_index, cpcon);
			pthread_mutex_unlock(&cp_tracker_mutex[ct_ht_index]);
		} else {
			ct_ht_index = cp_tracker_hash_func(&(cpcon->remote_ipv4v6));
			pthread_mutex_lock(&cp_tracker_mutex[ct_ht_index]);
			cp_tracker_del_unresp_connections(ct_ht_index, cpcon);
			pthread_mutex_unlock(&cp_tracker_mutex[ct_ht_index]);
		}
		UNSET_CPCON_FLAG(cpcon, CPF_UNRESP_CONNECTION);
	}
	if (CHECK_CPCON_FLAG(cpcon, CPF_IS_IPV6)) { 
		DBG_LOG(MSG, MOD_CP, "Activate error fd=%d. cp_tot_cur_open_ipv6=%ld", 
				fd, glob_cp_tot_cur_open_sockets_ipv6);
	} else {
		DBG_LOG(MSG, MOD_CP, "Activate error fd=%d. cp_tot_cur_open=%ld", 
				fd, glob_cp_tot_cur_open_sockets);
	}
	NM_TRACE_UNLOCK(pnm, LT_OM_CONNPOLL);
	pthread_mutex_unlock(&pnm->mutex);
	return FALSE;
}

/*
 * acquire mutex locker cp_sp_mutex.
 */
static struct cp_socket_pool * cpsp_pickup_from_pool(
	ip_addr_t* src_ipv4v6,
	ip_addr_t* ipv4v6, uint16_t port, uint16_t ht_index)
{
	struct cp_socket_pool * psp, *tmp_psp;

	int src_same = 0;
	int dst_same = 0;

	psp = TAILQ_FIRST(&g_pcp_hashtable[ht_index]);
	if(psp) {
		if (src_ipv4v6->family == AF_INET) {
	    		if( IPv4(psp->src_ipv4v6) == IPv4((*src_ipv4v6)) ) {
				src_same = 1;
			}
		} else {
			if ( memcmp(&(psp->src_ipv4v6), src_ipv4v6, sizeof(ip_addr_t)) == 0 ) {
				src_same = 1;
			}
		}
		
		if (ipv4v6->family == AF_INET) {
                        if( IPv4(psp->ipv4v6) == IPv4((*ipv4v6)) ) {
                                dst_same = 1;
                        }
                } else {
                        if ( memcmp(&(psp->ipv4v6), ipv4v6, sizeof(ip_addr_t)) == 0 ) {
                                dst_same = 1;
                        }
                }
		if ( src_same && dst_same && (psp->port == port)) {
			TAILQ_REMOVE(&g_pcp_hashtable[ht_index], psp, entry);
			return psp;
		}
		src_same = 0;
		dst_same = 0;	
		TAILQ_FOREACH_SAFE(psp, &g_pcp_hashtable[ht_index], entry, tmp_psp) {			
	                if (src_ipv4v6->family == AF_INET) {
        	                if( IPv4(psp->src_ipv4v6) == IPv4((*src_ipv4v6)) ) {
                	                src_same = 1;
                        	}
               		} else {
                        	if ( memcmp(&(psp->src_ipv4v6), src_ipv4v6, sizeof(ip_addr_t)) == 0 ) {
                                	src_same = 1;
                        	}
                	}

                	if (ipv4v6->family == AF_INET) {
                        	if( IPv4(psp->ipv4v6) == IPv4((*ipv4v6)) ) {
                                	dst_same = 1;
                        	}
                	} else {
                        	if ( memcmp(&(psp->ipv4v6), ipv4v6, sizeof(ip_addr_t)) == 0 ) {
                                	dst_same = 1;
                        	}
                	}
			if ( src_same && dst_same && (psp->port == port)) {
				TAILQ_REMOVE(&g_pcp_hashtable[ht_index], psp, entry);
				return psp;
			}
	    	}
	}

	return NULL;
}

static cpcon_t * cp_new_socket_handler(int * fd, 
	ip_addr_t* src_ipv4v6, ip_addr_t* ipv4v6, uint16_t port,
	cp_app_type app_type,
        void * private_data,
        int (* pcp_connected)(void * private_data),
        void (* pcp_connect_failed)(void * private_data, int called_from),
        int (* pcp_send_httpreq)(void * private_data),
        int (* pcp_http_header_received)(void * private_data, char * p, int len, int content_length),
        int (* pcp_http_header_parse_error)(void * private_data),
        int (* pcp_httpbody_received)(void * private_data, char * p, int * len, int * shouldexit),
        int (* pcp_closed)(void * private_data, int close_httpconn),
        int (* pcp_timer)(void * private_data, net_timer_type_t type))
{
	cpcon_t * cpcon;
	con_t * httpreqcon;
        struct network_mgr * pnm_cp;
        int http_cb_max_buf_size = TYPICAL_HTTP_CB_BUF_OM;
	const namespace_config_t *nsconf;
	if (app_type == CP_APP_TYPE_TUNNEL) {
                nsconf = ((fpcon_t *)private_data)->httpcon->http.nsconf;
        } else {
                nsconf = ((omcon_t *)private_data)->oscfg.nsc;
        }

	/*
	 * Check for max om connections
	 */
        if ( ( (AO_load(&glob_cp_tot_cur_open_sockets)
			 + AO_load(&glob_cp_tot_cur_open_sockets_ipv6)) >= om_connection_limit) &&
                        (om_connection_limit != 0 ))
	{
		DBG_LOG(WARNING, MOD_CP, "cp_new_socket_handler error,	\
			om_connection_limit=%d, but cp_tot_cur_open_sockets=%ld and cp_tot_cur_open_sockets_ipv6=%ld",
			 om_connection_limit, glob_cp_tot_cur_open_sockets, glob_cp_tot_cur_open_sockets_ipv6 );
                return NULL;
	}
	int family = ipv4v6->family;
	if (nsconf && (nsconf->http_config->origin_request_type & DP_ORIGIN_REQ_SECURE)) {
		family  = AF_INET;
	} 
	/* always return a valid fd */
	if( (*fd = socket(family, SOCK_STREAM, 0)) < 0) {
		DBG_LOG(WARNING, MOD_CP, "FP Failed to create sockfd, errno=%d. family=%d", errno, ipv4v6->family);
		return NULL;
	}
	nkn_mark_fd(*fd, NETWORK_FD);

	/*
	 * Didn't find any idle socket in the socket pool.
	 * We need to create one.
	 */
	cpcon = (cpcon_t *)nkn_malloc_type(sizeof(cpcon_t), mod_om_cpcon_t);
	if(!cpcon) {
		nkn_close_fd(*fd, NETWORK_FD);
		*fd = -1;
		return NULL;
	}
	memset((char *)cpcon, 0, sizeof(cpcon_t));
	cpcon->magic = CPMAGIC_ALIVE;
	SET_CPCON_FLAG(cpcon, CPF_FIRST_NEW_SOCKET);
        if (ipv4v6->family == AF_INET6) {
                SET_CPCON_FLAG(cpcon, CPF_IS_IPV6);
        }
	switch (app_type) {
	case CP_APP_TYPE_OM:
		httpreqcon = ((omcon_t *) (private_data))->httpreqcon;
		break;
	case CP_APP_TYPE_TUNNEL:
		httpreqcon = ((fpcon_t *) (private_data))->httpcon;
		break;
	default:
		break;
	}

	cpcon->http.cb_max_buf_size = http_cb_max_buf_size;
        cpcon->http.cb_buf = (char *)nkn_malloc_type(cpcon->http.cb_max_buf_size+1,
                                                        mod_om_network_cb);
        if(!cpcon->http.cb_buf) {
                DBG_LOG(WARNING, MOD_CP, "Run out of memory.");
		nkn_close_fd(*fd, NETWORK_FD);
		cpcon->magic = CPMAGIC_DEAD;
                free(cpcon);
		*fd = -1;
                return NULL;
        }

        init_http_header(&cpcon->http.hdr, cpcon->http.cb_buf,
                         cpcon->http.cb_max_buf_size);

	memcpy(&(cpcon->src_ipv4v6), src_ipv4v6, sizeof(ip_addr_t));
	memcpy(&(cpcon->remote_ipv4v6), ipv4v6, sizeof(ip_addr_t));
	memcpy(&(cpcon->http.remote_ipv4v6), ipv4v6, sizeof(ip_addr_t));

	cpcon->http.remote_port = ntohs(port);
	cpcon->type = app_type;
	cpcon->remote_port = port;
	cpcon->private_data = private_data;
	cpcon->cp_connected = pcp_connected;
	cpcon->cp_connect_failed = pcp_connect_failed;
	cpcon->cp_send_httpreq = pcp_send_httpreq;
	cpcon->cp_http_header_received = pcp_http_header_received;
	cpcon->cp_http_header_parse_error = pcp_http_header_parse_error;
	cpcon->cp_httpbody_received = pcp_httpbody_received;
	cpcon->cp_closed = pcp_closed;
	cpcon->cp_timer = pcp_timer;
        pnm_cp = &gnm[*fd];
        pthread_mutex_lock(&pnm_cp->mutex);
	NM_TRACE_LOCK(pnm_cp, LT_OM_CONNPOLL);
	if (CHECK_CPCON_FLAG(cpcon, CPF_IS_IPV6)) {
		AO_fetch_and_add1(&glob_cp_tot_cur_open_sockets_ipv6);
		NM_SET_FLAG(pnm_cp, NMF_IS_IPV6);
	} else {
		AO_fetch_and_add1(&glob_cp_tot_cur_open_sockets);
	}
#ifdef SOCKFD_TRACE
	if (debug_fd_trace && pnm_cp->p_trace == NULL) {
		pnm_cp->p_trace = nkn_calloc_type(1, sizeof(nm_trace_t), mod_nm_trace_t);
#ifdef DBG_EVENT_TRACE
		pthread_mutex_init(&pnm_cp->p_trace->mutex, NULL);
#endif
	}
#endif
	return cpcon;
}

/**
 * Get connection poll con structure pointer.
 * this function may not have valid socket id.
 *
 *  INPUT:
 *   ip and port in network order.
 * OUTPUT:
 *   return NULL: means error.
 *   return !=NULL: fd is always returned.
 *   when return !=NULL, mutex lock is hold
 */
cpcon_t * cp_open_socket_handler(int * fd, 
	ip_addr_t* src_ipv4v6, ip_addr_t *ipv4v6, uint16_t port,
	cp_app_type app_type,
        void * private_data,
        int (* pcp_connected)(void * private_data),
        void (* pcp_connect_failed)(void * private_data, int called_from),
        int (* pcp_send_httpreq)(void * private_data),
        int (* pcp_http_header_received)(void * private_data, char * p, int len, int content_length),
        int (* pcp_http_header_parse_error)(void * private_data),
        int (* pcp_httpbody_received)(void * private_data, char * p, int * len, int * shouldexit),
        int (* pcp_closed)(void * private_data, int close_httpconn),
        int (* pcp_timer)(void * private_data, net_timer_type_t type))
{
	cpcon_t * cpcon;
	uint16_t ht_index;
	struct cp_socket_pool * psp, * psp2, *temp_psp;
        struct network_mgr * pnm_cp;
	int cnt=0;
	const namespace_config_t *nsconf;
	/*
	 * Try to find it from keep-alive connection pool first.
	 * If It is a ssl origin, we should not use the conn pool
	 */
	if (app_type == CP_APP_TYPE_TUNNEL) {
		nsconf = ((fpcon_t *)private_data)->httpcon->http.nsconf;
	} else {
		nsconf = ((omcon_t *)private_data)->oscfg.nsc;
	}

	if (nsconf && (nsconf->http_config->origin_request_type & DP_ORIGIN_REQ_SECURE)) {
		return cp_new_socket_handler(fd, src_ipv4v6, ipv4v6, port, app_type,
				private_data,
				pcp_connected, pcp_connect_failed,
				pcp_send_httpreq, pcp_http_header_received,
				pcp_http_header_parse_error,
				pcp_httpbody_received,
				pcp_closed,
				pcp_timer);
	}

	pthread_mutex_lock(&cp_sp_mutex);
	ht_index = cpsp_hashtable_func(src_ipv4v6, ipv4v6, port);
	psp = cpsp_pickup_from_pool(src_ipv4v6, ipv4v6, port, ht_index);
	if (psp == NULL) {
		pthread_mutex_unlock(&cp_sp_mutex);
		return cp_new_socket_handler(fd, src_ipv4v6, ipv4v6, port, app_type,
				private_data, 
				pcp_connected, pcp_connect_failed,
				pcp_send_httpreq, pcp_http_header_received,
				pcp_http_header_parse_error,
				pcp_httpbody_received,
				pcp_closed,
				pcp_timer);
	}

return_cpcon_from_pool:
	/* 
	 * While we are deleting this psp from connection pool, 
	 * epoll event could wake up and call cpsp_epoll_rm_fm_pool().
	 * In that case, the socket in connection pool could be invalid.
	 *
	 * Also cpsp_epoll_rm_fm_pool() could hold the gnm[fd].mutex
	 * and wait for cp_sp_mutex lock. Then we maybe deadlock.
	 */
        pnm_cp = &gnm[psp->fd];
        if(pthread_mutex_trylock(&pnm_cp->mutex) != 0) {
		/* 
		 * Cannot get lock, try to find second socket in the pool
		 */
		glob_cp_cannot_acquire_mutex++;
		cnt++;
		psp2 = (cnt < 2) ? cpsp_pickup_from_pool(src_ipv4v6, ipv4v6, port, ht_index) : NULL;

		/* recover back into pool */
		TAILQ_INSERT_TAIL(&g_pcp_hashtable[ht_index], psp, entry);

		if (psp2==NULL) {
			pthread_mutex_unlock(&cp_sp_mutex);
			return cp_new_socket_handler(fd, src_ipv4v6, ipv4v6, port, app_type,
				private_data, 
				pcp_connected, pcp_connect_failed,
				pcp_send_httpreq, pcp_http_header_received,
				pcp_http_header_parse_error,
				pcp_httpbody_received,
				pcp_closed,
				pcp_timer);
		}
		// Try second socket
		psp = psp2;
		goto return_cpcon_from_pool;
        }
        NM_TRACE_LOCK(pnm_cp, LT_OM_CONNPOLL);
        if( !NM_CHECK_FLAG(pnm_cp, NMF_IN_USE) ) {
		/* 
		 * while we are waiting for lock, 
		 * try to find second socket in the pool
		 */
		cnt++;
		psp2 = (cnt < 2) ? cpsp_pickup_from_pool(src_ipv4v6, ipv4v6, port, ht_index) : NULL;

		/* delete this psp */
		AO_fetch_and_sub1(&glob_cp_tot_sockets_in_pool);
		psp->magic = CPSP_MAGIC_DEAD;
                temp_psp = psp;

		if (psp2==NULL) {
			NM_TRACE_UNLOCK(pnm_cp, LT_OM_CONNPOLL);
			pthread_mutex_unlock(&pnm_cp->mutex);
			pthread_mutex_unlock(&cp_sp_mutex);
			free(temp_psp);
			return cp_new_socket_handler(fd, src_ipv4v6, ipv4v6, port, app_type,
				private_data, 
				pcp_connected, pcp_connect_failed,
				pcp_send_httpreq, pcp_http_header_received,
				pcp_http_header_parse_error,
				pcp_httpbody_received,
				pcp_closed,
				pcp_timer);
		}

		// Try second socket
		psp = psp2;
		NM_TRACE_UNLOCK(pnm_cp, LT_OM_CONNPOLL);
		pthread_mutex_unlock(&pnm_cp->mutex);
		free(temp_psp);
		goto return_cpcon_from_pool;
        }

	// Good time to delete psp from pool
	*fd = psp->fd;
	if (NM_CHECK_FLAG(pnm_cp, NMF_IS_IPV6)) {
		AO_fetch_and_sub1(&glob_cp_tot_sockets_in_pool_ipv6);
	} else {
		AO_fetch_and_sub1(&glob_cp_tot_sockets_in_pool);
	}
	psp->magic = CPSP_MAGIC_DEAD;
	pthread_mutex_unlock(&cp_sp_mutex);
	free(psp);

	cpcon = (cpcon_t *)gnm[*fd].private_data;
	assert(cpcon->magic == CPMAGIC_IN_POOL);
	cpcon->magic = CPMAGIC_ALIVE;

	/* delete for now, this socket will be reused */
	NM_del_event_epoll(*fd);

	/* the socket is valid */
	SET_CPCON_FLAG(cpcon, CPF_USE_KA_SOCKET);
	glob_cp_socket_reused ++;

	cpcon->nsconf = NULL;
	memset(&cpcon->ns_token, 0, sizeof(cpcon->ns_token));
	memcpy(&(cpcon->http.remote_ipv4v6), ipv4v6, sizeof(ip_addr_t));
	cpcon->http.remote_port = ntohs(port);
	cpcon->type = app_type;
	memcpy(&(cpcon->remote_ipv4v6), ipv4v6, sizeof(ip_addr_t));
	cpcon->remote_port = port;
	cpcon->private_data = private_data;
	cpcon->cp_connected = pcp_connected;
	cpcon->cp_connect_failed = pcp_connect_failed;
	cpcon->cp_send_httpreq = pcp_send_httpreq;
	cpcon->cp_http_header_received = pcp_http_header_received;
	cpcon->cp_http_header_parse_error = pcp_http_header_parse_error;
	cpcon->cp_httpbody_received = pcp_httpbody_received;
	cpcon->cp_closed = pcp_closed;

	/* don't release this mutex locker here */
	//pthread_mutex_unlock(&pnm_cp->mutex);
	DBG_LOG(MSG, MOD_CP, "extract socket %d from connection pool.", *fd);
	return cpcon;
}

/*
 * ***************** NOTICE *************************
 * This is an internal function.
 * externally function cp_close_socket_handler() should be called.
 *
 * cp_internal_close_conn() could be called through cp_close_socket_handler() event
 * And socket fd is invalid (fd==-1)
 * e.g. cp_activate_socket_handler() returns FALSE
 */
static void cp_internal_close_conn(int fd, cpcon_t * cpcon)
{
	int close_httpconn;

	if(cpcon == NULL) return;
	/*
	 * BUG 4228/4289: something is not right.
	 *
	 * One possible case might be:
	 * One thread calls this functon to free cpcon()
	 * Another thread calls cp_add_event().
	 * then when check_out_cp_queue() wakes up to call this function,
	 * it is accessing freed memory point cpcon.
	 * The following check to avoid above case.
	 */
        if(cpcon->magic != CPMAGIC_ALIVE) return;
        if(gnm[fd].fd != fd) return;
	if(gnm[fd].private_data != (void *)cpcon) return;

	NM_EVENT_TRACE(fd, FT_CP_INTERNAL_CLOSE_CONN);
	/* If we are in the event queue, we should delete from there */
	if(CHECK_CPCON_FLAG(cpcon, CPF_IN_EVENT_QUEUE)) {
		cp_remove_event(fd);
		UNSET_CPCON_FLAG(cpcon, CPF_IN_EVENT_QUEUE);
	}

	if((cpcon->type != CP_APP_TYPE_TUNNEL )) {
        omcon_t *ocon = (omcon_t *)cpcon->private_data;
	//Check some ocon sanity by comparing the fds.
	    if(((ocon->fd == fd)) &&
		(ocon->comp_status ==1) && 
		OM_CHECK_FLAG(ocon, OMF_COMPRESS_TASKPOSTED|OMF_COMPRESS_TASKRETURN)) {
		cp_add_event(fd, OP_CLOSE_CALLBACK, cpcon);
		return;
	    }
	}

#if 0
	if((cpcon->cp_closed == NULL) && (cpcon->type == CP_APP_TYPE_OM)) {
		/*  
		 * BUG 3821:
		 * For CP_APP_TYPE_OM, we always calls back to clean up omcon
		 * Exception: 
		 *     we mark response as tunnel_response.
		 *     but never come back from HTTP server module.
		 *     Then timeout. This assert tries to catch this case.
		 */
		assert(0);
	}
#endif // 0

	/* 
	 * BUG: If we have left over data, send out.
	 * However, we may not be in the right state in FSM
	 */
	if((fd != -1) && /* invalid socket */
	   cpcon->cp_httpbody_received && /* OM set callback function */
	   (cpcon->http.cb_totlen)) {
		char * p;
		int len;
		int shouldexit = 0;

		p = &cpcon->http.cb_buf[cpcon->http.cb_offsetlen];
		len = cpcon->http.cb_totlen - cpcon->http.cb_offsetlen;
		if (len ) {
			(*(cpcon->cp_httpbody_received)) (cpcon->private_data, p, &len, &shouldexit);
		}

		cpcon->http.cb_totlen = 0;
		cpcon->http.cb_offsetlen = 0;
		cpcon->cp_httpbody_received = NULL;
	}
	/* it does not matter any more for returned value */

	/* close server side connection poll socket */

	/* close client side http socket */
	close_httpconn = FALSE;
	if( CHECK_CPCON_FLAG(cpcon, CPF_CONN_CLOSE) ||
	    !CHECK_CPCON_FLAG(cpcon, CPF_HTTP_END_DETECTED) ) {
		close_httpconn = TRUE;
	}
	if(cpcon->cp_closed) {
		int ret;
		/*
		 * BUG: we closed fpcon->httpcon->fd, but didn't get mutex
		 *      pnm = (network_mgr_t *)&gnm[fpcon->httpcon->fd];
		 *      pthread_mutex_lock(&pnm->mutex);
		 */
		ret = (*(cpcon->cp_closed)) ((void *)cpcon->private_data, close_httpconn);
		if (ret == FALSE) {
			cp_add_event(fd, OP_CLOSE_CALLBACK, cpcon);
			return;
		}
		cpcon->cp_closed = NULL;
	}

	/* 
	 * Close the socket without re-use.
	 */
	if( (close_httpconn == TRUE) || CHECK_CPCON_FLAG(cpcon, CPF_HTTPS_CONNECT) ||
	    (cpsp_add_into_pool(fd, cpcon) == FALSE)) {
		if(fd!=-1) {
			if (CHECK_CPCON_FLAG(cpcon, CPF_TRANSPARENT_REQ)) {
			    cp_release_transparent_port(cpcon);

			    dbg_helper_connect_sock(0, "closing", cpcon, __FUNCTION__, __LINE__);
			}
			
			if(CHECK_CPCON_FLAG(cpcon, CPF_HTTPS_CONNECT)) {
				/* Write into shared memory */
			 	if(close_httpconn == TRUE) {
					*(ssl_shm_mem + cpcon->ssl_fid)	= '0'; 
				} else {
					*(ssl_shm_mem + cpcon->ssl_fid)	= CONN_POOL_SSL_MARKER;
				}
			}

			NM_close_socket(fd);
			if (CHECK_CPCON_FLAG(cpcon, CPF_IS_IPV6)) { 
				AO_fetch_and_add1(&glob_cp_tot_closed_sockets_ipv6);
				AO_fetch_and_sub1(&glob_cp_tot_cur_open_sockets_ipv6);
				DBG_LOG(MSG, MOD_CP, "Close the socket %d without re-use, cp_tot_cur_open_ipv6=%ld", 
					fd, glob_cp_tot_cur_open_sockets_ipv6);
			} else {
				AO_fetch_and_add1(&glob_cp_tot_closed_sockets);
				AO_fetch_and_sub1(&glob_cp_tot_cur_open_sockets);
				DBG_LOG(MSG, MOD_CP, "Close the socket %d without re-use, cp_tot_cur_open=%ld", 
					fd, glob_cp_tot_cur_open_sockets);
			}
			if (cpcon->nsconf) {
				NS_DECR_STATS(cpcon->nsconf, PROTO_HTTP, server, active_conns);
				NS_DECR_STATS(cpcon->nsconf, PROTO_HTTP, server, conns);
			}
		}
		if (CHECK_CPCON_FLAG(cpcon, CPF_UNRESP_CONNECTION)) {
			    int32_t ct_ht_index;
			    ct_ht_index = cp_tracker_hash_func(&(cpcon->remote_ipv4v6));
			    pthread_mutex_lock(&cp_tracker_mutex[ct_ht_index]);
			    cp_tracker_del_unresp_connections(ct_ht_index, cpcon);
			    pthread_mutex_unlock(&cp_tracker_mutex[ct_ht_index]);
			    UNSET_CPCON_FLAG(cpcon, CPF_UNRESP_CONNECTION);
		}
		shutdown_http_header(&cpcon->http.hdr);
		free(cpcon->http.cb_buf);
		if(cpcon->http.resp_buf) free(cpcon->http.resp_buf);
		cpcon->magic = CPMAGIC_DEAD;
		if (cpcon->oscfg.ip) free(cpcon->oscfg.ip);
		release_namespace_token_t(cpcon->ns_token);
		free(cpcon);
	}
}

