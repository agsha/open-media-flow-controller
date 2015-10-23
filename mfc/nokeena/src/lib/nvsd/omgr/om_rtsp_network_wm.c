#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <atomic_ops.h>
#include <netinet/ip.h>
#include <stdint.h>
#include <sys/prctl.h>
#include <uuid/uuid.h>

/* NKN includes */
#include "nkn_debug.h"
#include "nkn_errno.h"
#include "nkn_stat.h"
#include "nkn_cod.h"
#include "nkn_util.h"
#include "network.h"
#include "nkn_cfg_params.h"
#include "rtsp_live.h"
#include "rtsp_session.h"
#include "rtsp_server.h"
#include "rtsp_header.h"
#include "nkn_mediamgr_api.h"

#include "rtcp_util.h"

NKNCNT_EXTERN(rtsp_shared_live_session, uint64_t)
NKNCNT_EXTERN(rtsp_alloc_sess_bw_err, uint64_t)
NKNCNT_EXTERN(rtp_tot_udp_packets_fwd, uint64_t)
NKNCNT_EXTERN(rtcp_tot_udp_packets_fwd, uint64_t)
NKNCNT_EXTERN(rtsp_err_namespace_not_found, uint64_t)
NKNCNT_EXTERN(cur_open_rtsp_socket, AO_t)
NKNCNT_EXTERN(rtsp_tot_size_from_origin, AO_t)
NKNCNT_EXTERN(rtsp_tot_size_from_cache, AO_t)
NKNCNT_EXTERN(rtsp_tot_size_to_cache, AO_t)
NKNCNT_EXTERN(rtcp_tot_udp_sr_packets_sent, uint64_t)
NKNCNT_EXTERN(rtcp_tot_udp_rr_packets_sent, uint64_t)


/*
 * external variables and functions
 */
extern int rtsp_player_idle_timeout;
extern int rtsp_origin_idle_timeout;
extern pthread_mutex_t counter_mutex;

extern int rl_get_udp_sockfd(uint16_t port, int setLoopback, int reuse_port, struct in_addr *IfAddr);


/*
 * local variables and functions
 */

//static pthread_mutex_t ses_bw_mutex = PTHREAD_MUTEX_INITIALIZER;
extern int rtsp_server_port;
extern uint64_t glob_rtsp_tot_bytes_sent;
extern uint64_t glob_tot_video_delivered;
//static pthread_mutex_t g_ps_mutex = PTHREAD_MUTEX_INITIALIZER; 
//static pthread_mutex_t rtp_port_mutex = PTHREAD_MUTEX_INITIALIZER;
//static pthread_mutex_t rtcp_port_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Prototypes
 * nomenclature: rl_ prefix indicates relay
 *               rtp_ prefix indicates rtp connection handlers
 *               rtcp_ prefix indicates rtcp connection handlers
 *               rl_ps denotes MFD <--> Origin connections
 *               rl_player denotes MFD <--> Viewer connections
 */
extern pthread_mutex_t rtp_port_mutex;
extern uint16_t rtp_udp_port; /* starting port number for RTP channels port assignment */
extern uint16_t rtcp_udp_port; /* starting port number for RTCP channels port assignment */


int rl_player_form_sdp(rtsp_con_player_t * pplayer, char *sdp, int buflen);
int rl_player_sendout_resp(rtsp_con_player_t * pplayer, char * p, int len);
int rl_player_write_streamminglog(rtsp_con_player_t * pplayer);
int rl_ps_sendout_request(rtsp_con_ps_t * pps);

int orp_write_sdp_to_tfm(rtsp_con_player_t * pplayer);
int orp_send_data_to_tfm_udp(rtsp_con_player_t * pplayer, char *buf, int len, int type, int track);
int orp_send_data_to_tfm_tcp(rtsp_con_player_t * pplayer, char *buf, int len, int type, int track);

int rtp_player_epollin(int sockfd, void *private_data);
int rtp_player_epollout(int sockfd, void *private_data);
int rtp_player_epollhup(int sockfd, void *private_data);
int rtp_player_epollerr(int sockfd, void *private_data);
int rtcp_player_epollin(int sockfd, void *private_data);
int rtcp_player_epollout(int sockfd, void *private_data);
int rtcp_player_epollhup(int sockfd, void *private_data);
int rtcp_player_epollerr(int sockfd, void *private_data);
int rtp_ps_epollin(int sockfd, void *private_data);
int rtp_ps_epollout(int sockfd, void *private_data);
int rtp_ps_epollhup(int sockfd, void *private_data);
int rtp_ps_epollerr(int sockfd, void *private_data);
int rtcp_ps_epollin(int sockfd, void *private_data);
int rtcp_ps_epollout(int sockfd, void *private_data);
int rtcp_ps_epollhup(int sockfd, void *private_data);
int rtcp_ps_epollerr(int sockfd, void *private_data);

int rl_player_sendout_inline_data(rtsp_con_player_t * pplayer, 
				char *buf, int len, int type, int track);
int rl_player_sendout_udp_data(rtsp_con_player_t * pplayer, 
				char *buf, int len, int type, int track);
int rl_player_sendout_udp_as_inline_data(rtsp_con_player_t * pplayer, 
				char *buf, int len, int type, int track);
int rl_player_sendout_inline_as_udp_data(rtsp_con_player_t * pplayer, 
				char *buf, int len, int type, int track);


int rl_player_wmp_set_callbacks( rtsp_con_player_t * pplayer );
int rl_ps_wms_set_callbacks( rtsp_con_ps_t * pps );

int rl_player_wmp_ret_status_resp(rtsp_con_player_t * pplayer, char * hdr, int hdr_len);
int rl_player_wmp_ret_options (rtsp_con_player_t * pplayer);
int rl_player_wmp_ret_describe(rtsp_con_player_t * pplayer);
int rl_player_wmp_ret_setup(rtsp_con_player_t * pplayer);
int rl_player_wmp_ret_pause(rtsp_con_player_t * pplayer);
int rl_player_wmp_ret_play(rtsp_con_player_t * pplayer);
int rl_player_wmp_ret_get_parameter(rtsp_con_player_t * pplayer);
int rl_player_wmp_ret_set_parameter(rtsp_con_player_t * pplayer);
int rl_player_wmp_ret_teardown(rtsp_con_player_t * pplayer);

char *rl_ps_wms_send_method_options(rtsp_con_ps_t * pps);
char *rl_ps_wms_send_method_describe(rtsp_con_ps_t * pps);
char *rl_ps_wms_send_method_setup(rtsp_con_ps_t * pps);
char *rl_ps_wms_send_method_play(rtsp_con_ps_t * pps);
char *rl_ps_wms_send_method_pause(rtsp_con_ps_t * pps);
char *rl_ps_wms_send_method_get_parameter(rtsp_con_ps_t * pps);
char *rl_ps_wms_send_method_set_parameter(rtsp_con_ps_t * pps);
char *rl_ps_wms_send_method_teardown(rtsp_con_ps_t * pps);

char * rl_ps_wms_buildup_req_head(rtsp_con_ps_t * pps, char * add_hdr, int add_hdr_size);
char * rl_ps_wms_recv_method_options(rtsp_con_ps_t * pps);
char * rl_ps_wms_recv_method_describe(rtsp_con_ps_t * pps);
char * rl_ps_wms_recv_method_setup(rtsp_con_ps_t * pps);
char * rl_ps_wms_recv_method_play(rtsp_con_ps_t * pps);
char * rl_ps_wms_recv_method_pause(rtsp_con_ps_t * pps);
char * rl_ps_wms_recv_method_get_parameter(rtsp_con_ps_t * pps);
char * rl_ps_wms_recv_method_set_parameter(rtsp_con_ps_t * pps);
char * rl_ps_wms_recv_method_teardown(rtsp_con_ps_t * pps);

int rl_player_connect_to_svr(rtsp_con_player_t * pplayer);
void rl_player_closed(rtsp_con_player_t * pplayer, int locks_held);
int rl_ps_closed(rtsp_con_ps_t * pps, rtsp_con_player_t * no_callback_for_this_pplayer, int locks_held);
int32_t rl_ps_parse_sdp_info( char **p_sdp_data, rtsp_con_ps_t *pps);
uint32_t rl_get_sample_rate(rtsp_con_ps_t *pps, char * trackID);
void rl_set_channel_id(rtsp_con_ps_t *pps, char * trackID, uint8_t channel_id);
int rl_get_track_num(rtsp_con_ps_t *pps, char * trackID);

/* Helper Functions */
void rtsplive_keepaliveTask(rtsp_con_ps_t * pps);


/* If hdr_len is non zero, hdr is appended to the response. 
 * Else trailing \r\n is added. If hdr is passed it the
 * responisibilty of thr caaling function to add the addtion trailing \r\n.
 */
int rl_player_wmp_ret_status_resp(rtsp_con_player_t * pplayer, char * hdr, int hdr_len)
{
#define OUTBUF(_data, _datalen) { \
	memcpy((char *)&pplayer->prtsp->cb_respbuf[pplayer->prtsp->cb_resplen], \
		(_data), (_datalen)); \
	pplayer->prtsp->cb_resplen += (_datalen); \
}

	rtsp_con_ps_t *pps;
	const char *name;
	int namelen;
	const char *data;
	int datalen;
	u_int32_t attrs;
	int hdrcnt;
	int nth;
	int token;
	int rv;
	int i;
	char strbuf[1024];

	if(pplayer == NULL) return FALSE;
	pps = pplayer->pps;
	/*
	 * pps could be NULL when we cannot establish a socket to Origin Server.
	 */

	for ( i = 0 ; i < RTSP_STATUS_MAX ; i++ ) {
		if ( pplayer->prtsp->status == rtsp_status_map[i].i_status_code )
			break;
	}
	if (i == RTSP_STATUS_MAX) {
		return FALSE;
	}

	//
        // Build up the RTSP response
	//
	pplayer->prtsp->cb_resplen=0;

	// Adding the RTSP code line
	rv = snprintf(strbuf, sizeof(strbuf), 
			"RTSP/1.0 %d %s\r\n",
			rtsp_status_map[i].i_status_code, 
			rtsp_status_map[i].reason);
	OUTBUF(strbuf, rv);
#if 0
	if(phttp->subcode) {
        	len = snprintf(p, phttp->cb_max_buf_size-phttp->res_hdlen, 
			       " %d", phttp->subcode);
        	p += len; phttp->res_hdlen += len;
	}
#endif // 0

	/* Add CSEQ Header */
	rv = snprintf(strbuf, sizeof(strbuf), 
			"CSeq: %s\r\n",
			pplayer->prtsp->cseq);
	OUTBUF(strbuf, rv);

	/* Add Session Header only if setup was done and server has
	 * given session id
	 */
	if (CHECK_PLAYER_FLAG(pplayer, PLAYERF_SETUP_DONE) && 
			pplayer->session) {
		rv = snprintf(strbuf, sizeof(strbuf), 
			"Session: %llu;timeout=%d\r\n",
			pplayer->session, 
			rtsp_player_idle_timeout); 
		OUTBUF(strbuf, rv);
	}

	/* Add Server Header */
	//rv = snprintf(strbuf, sizeof(strbuf), 
	//		"Server: %s\r\n",
	//		rl_rtsp_server_str);
	//OUTBUF(strbuf, rv);

	/* Add Date Header */
	rv = snprintf(strbuf, sizeof(strbuf), 
			"Date: %s\r\n",
			nkn_get_datestr(NULL));
	OUTBUF(strbuf, rv);

	if (pps) {
		// Add known headers
		for (token = 0; token < RTSP_HDR_MAX_DEFS; token++) {
			// Don't forward the following headers
			switch(token) {
			case RTSP_HDR_CSEQ:
			//case RTSP_HDR_RANGE: // Don't forward Range
			case RTSP_HDR_RTP_INFO:
			case RTSP_HDR_TRANSPORT:
			case RTSP_HDR_SESSION:
			case RTSP_HDR_DATE:
			//case RTSP_HDR_SERVER:
			case RTSP_HDR_CONTENT_LENGTH:
			//case RTSP_HDR_CONTENT_BASE:
			//case RTSP_HDR_EXPIRES:

			case RTSP_HDR_X_METHOD:
			case RTSP_HDR_X_PROTOCOL:
			case RTSP_HDR_X_HOST:
			case RTSP_HDR_X_PORT:
			case RTSP_HDR_X_URL:
			case RTSP_HDR_X_ABS_PATH:
			case RTSP_HDR_X_VERSION:
			case RTSP_HDR_X_VERSION_NUM:
			case RTSP_HDR_X_STATUS_CODE:
			case RTSP_HDR_X_REASON:
			case RTSP_HDR_X_QT_LATE_TOLERANCE:
			case RTSP_HDR_X_NKN_REQ_REAL_DEST_IP:
				continue;

			default:
				break;
			}

			rv = get_known_header(&pps->rtsp.hdr, token, &data, 
					&datalen, &attrs, &hdrcnt);
			if (!rv) {
				//INC_OUTBUF(rtsp_known_headers[token].namelen+datalen+1);
				OUTBUF(rtsp_known_headers[token].name, 
					rtsp_known_headers[token].namelen);
				OUTBUF(": ", 2);
				OUTBUF(data, datalen);
			} 
			else {
				continue;
			}
#if 1
			for (nth = 1; nth < hdrcnt; nth++) {
				rv = get_nth_known_header(&pps->rtsp.hdr, 
					token, nth, &data, &datalen, &attrs);
				if (!rv) {
					//INC_OUTBUF(datalen+3);
					OUTBUF(",", 1);
					OUTBUF(data, datalen);
				}
			}
#endif // 0
			OUTBUF("\r\n", 2);
		}

#if 1
		// Add unknown headers
		nth = 0;
		while ((rv = get_nth_unknown_header(&pps->rtsp.hdr, nth, &name, 
				&namelen, &data, &datalen, &attrs)) == 0) {
			nth++;
			//INC_OUTBUF(namelen+datalen+3);
			OUTBUF(name, namelen);
			OUTBUF(":", 1);
			OUTBUF(data, datalen);
			OUTBUF("\r\n", 2);
		}
#endif   
	}

	// Add passed in header
	if (hdr && hdr_len) {
		OUTBUF(hdr, hdr_len);
	}

	if (!hdr_len)
		OUTBUF("\r\n", 2);
#undef OUTBUF

	pplayer->prtsp->cb_respbuf[pplayer->prtsp->cb_resplen] = 0;
	rl_player_sendout_resp(pplayer, 
			pplayer->prtsp->cb_respbuf, 
			pplayer->prtsp->cb_resplen);
	pplayer->prtsp->cb_resplen = 0;
	pplayer->prtsp->cb_respoffsetlen = 0;
	rl_player_write_streamminglog(pplayer);
	return TRUE;
}

int rl_player_wmp_ret_options (rtsp_con_player_t * pplayer)
{
	if (!pplayer) return FALSE;
	pplayer->prtsp->status = 200;
	return rl_player_wmp_ret_status_resp(pplayer, NULL, 0);
}

int rl_player_wmp_ret_describe(rtsp_con_player_t * pplayer)
{
	// replace RTSP: Content-Base, Content-Length
	// replace SDP: a=control:
	rtsp_con_ps_t * pps;
	int sdplen; 
	struct in_addr in;
	char sdp[RTSP_BUFFER_SIZE + 4];
	int bytesused;
	char outbuf[RTSP_BUFFER_SIZE + 4];
	int outbufsz = RTSP_BUFFER_SIZE;
	
	//Since this function could be called from either the publisher or the player we
	//need to ensure that we check the state before sending a describe.
	//if (pplayer->method >= METHOD_SENT_DESCRIBE_RESP) return TRUE;

	if (!pplayer) return FALSE;
	pps = pplayer->pps;
	in.s_addr = pplayer->own_ip_addr; //rtsp.pns->if_addr;

	sdplen = rl_player_form_sdp(pplayer, sdp, RTSP_BUFFER_SIZE);

	/*
	 * Buildup the response header
	 */
	bytesused = 0;

	/*
	 * Disable IP spoofing by adding Content-base to point to MFD interface IP.
	 */
#if 0
	if (rtsp_tproxy_enable || pplayer->player_type == RL_PLAYER_TYPE_RELAY) {
		bytesused += snprintf(&outbuf[bytesused],
				outbufsz - bytesused,
				"Content-Base: rtsp://%s:%d%s/\r\n",
				inet_ntoa(in),
				rtsp_server_port,
				pps->uri );
	}
#endif
	
	bytesused += snprintf(&outbuf[bytesused],
			  outbufsz - bytesused,
			  "Content-Length: %d\r\n\r\n",
			  sdplen);
  
	pplayer->prtsp->status = 200;

	/* 
	 * Work around for nload issue, send sdp info in same packet
	 */
	if ((bytesused + sdplen) <= (RTSP_BUFFER_SIZE-1024)) {
		memcpy(outbuf + bytesused, sdp, sdplen);
		rl_player_wmp_ret_status_resp(pplayer, outbuf, bytesused + sdplen);
	}
	else {
		rl_player_wmp_ret_status_resp(pplayer, outbuf, bytesused);
		rl_player_sendout_resp(pplayer, sdp, sdplen);
	}

	return TRUE;
}

int rl_player_wmp_ret_setup(rtsp_con_player_t * pplayer)
{
	rtsp_con_ps_t * pps;
	int trackID;
	int bytesused;
	char outbuf[1024];
	int outbufsz = 1024;
	const char *data;
	int datalen;
	u_int32_t attrs;
	int hdrcnt;
	int rv;
	char * p;
	struct in_addr ifAddr;
	int len;
	
	pplayer->prtsp->status = RTSP_STATUS_400;
	if (!pplayer) return FALSE;
	pps = pplayer->pps;
	// extract trackID
	rv = get_known_header(&pplayer->prtsp->hdr, RTSP_HDR_X_ABS_PATH, &data,
				&datalen, &attrs, &hdrcnt);
	if (rv || !datalen) { return FALSE; }
	//p = strstr(data, "track");
	//if (!p) { 
	//	p = (char *) data; 
	//}
	for (trackID = 0; trackID < pplayer->pps->sdp_info.num_trackID; trackID++) {
		len = strlen(pplayer->pps->sdp_info.track[trackID].trackID);
		if (pplayer->pps->sdp_info.track[trackID].flag_abs_url)
			p = (char *) data;
		else
			p = (char *) data + (datalen - len);
		if (strncmp(pplayer->pps->sdp_info.track[trackID].trackID, 
				p, 
				len) == 0)
			break;
	}
	if (trackID == pplayer->pps->sdp_info.num_trackID) {
		pplayer->prtsp->status = RTSP_STATUS_462;	// Destination unreachable
		return FALSE;
	}

	if (pplayer->prtsp->streamingMode == RTP_UDP) {
		pplayer->rtp[trackID].peer_udp_port = pplayer->prtsp->clientRTPPortNum;
		pplayer->rtcp[trackID].peer_udp_port = pplayer->prtsp->clientRTCPPortNum;
		if (pplayer->pps->rtsp.streamingMode == RTP_UDP) {
			if( pps->pplayer_list->player_type == RL_PLAYER_TYPE_FAKE_PLAYER ) {
				pplayer->send_data = orp_send_data_to_tfm_udp;
			}
			else {
				pplayer->send_data = rl_player_sendout_udp_data;
			}
		}
		else if (pplayer->pps->rtsp.streamingMode == RTP_TCP) {
			if( pps->pplayer_list->player_type == RL_PLAYER_TYPE_FAKE_PLAYER ) {
				pplayer->send_data = orp_send_data_to_tfm_tcp;
			}
			else {
				pplayer->send_data = rl_player_sendout_inline_as_udp_data;
			}
		}
		else {
			/* Not supported currently */
			pplayer->prtsp->status = RTSP_STATUS_461;	// Unsupported transport
			return FALSE;
		}

		// we should open our UDP port first with stream server
		pthread_mutex_lock(&rtp_port_mutex);
		/*
		 * BUG: TBD, for T-Proxy, we need to set right UDP in iptables */
		//ifAddr.s_addr = pplayer->rtp[trackID].own_ip_addr;
		ifAddr.s_addr = pplayer->prtsp->pns->if_addrv4;
		while(1) {
			rtp_udp_port += 2;
			if (rtp_udp_port < 1024)
				rtp_udp_port = 1026;
			pplayer->rtp[trackID].udp_fd = rl_get_udp_sockfd(htons(rtp_udp_port), 0, 0, &ifAddr);
			if(pplayer->rtp[trackID].udp_fd != -1) break;
		}
		pplayer->rtp[trackID].own_udp_port = rtp_udp_port;
		//pthread_mutex_unlock(&rtp_port_mutex);
		NM_set_socket_nonblock(pplayer->rtp[trackID].udp_fd);	

		register_NM_socket(pplayer->rtp[trackID].udp_fd,
				(void *)pplayer,
				rtp_player_epollin,
				rtp_player_epollout,
				rtp_player_epollerr,
				rtp_player_epollhup,
				NULL,
				NULL,
				0,
				USE_LICENSE_FALSE,
				FALSE);

		NM_add_event_epollin(pplayer->rtp[trackID].udp_fd);

		// we should open our UDP port first with stream server
		//pthread_mutex_lock(&rtcp_port_mutex);
		rtcp_udp_port = rtp_udp_port + 1;
		while(1) {
			pplayer->rtcp[trackID].udp_fd = rl_get_udp_sockfd(htons(rtcp_udp_port), 0, 0, &ifAddr);
			if(pplayer->rtcp[trackID].udp_fd != -1) break;
			rtcp_udp_port += 2;
			if (rtcp_udp_port < 1024)
				rtcp_udp_port = 1027;
		}
		pplayer->rtcp[trackID].own_udp_port = rtcp_udp_port;
		pthread_mutex_unlock(&rtp_port_mutex);
		NM_set_socket_nonblock(pplayer->rtcp[trackID].udp_fd);

		register_NM_socket(pplayer->rtcp[trackID].udp_fd,
				(void *)pplayer,
				rtcp_player_epollin,
				rtcp_player_epollout,
				rtcp_player_epollerr,
				rtcp_player_epollhup,
				NULL,
				NULL,
				0,
				USE_LICENSE_FALSE,
				FALSE);

		NM_add_event_epollin(pplayer->rtcp[trackID].udp_fd);

	/*	if (pplayer->rtcp[trackID].udp_fd >= g_rl_maxfd) {
			g_rl_maxfd = pplayer->rtcp[trackID].udp_fd + 1;
		}*/
	} else if (pplayer->prtsp->streamingMode == RTP_TCP){
		if (pplayer->pps->rtsp.streamingMode == RTP_TCP) {
			/* Server mode is also TCP, use server's channel ID's.
			 */
			pplayer->rtp[trackID].own_channel_id = pps->rtp[trackID].own_channel_id;
			pplayer->rtcp[trackID].own_channel_id = pps->rtcp[trackID].own_channel_id;
			pplayer->send_data = rl_player_sendout_inline_data;
		}
		else if (pplayer->pps->rtsp.streamingMode == RTP_UDP) {
			/* Server mode UDP, use player's channel ID's. And set
			 * the send function to udp_to_inline conversion function.
			 */
			pplayer->rtp[trackID].own_channel_id = pplayer->prtsp->rtpChannelId;
			pplayer->rtcp[trackID].own_channel_id = pplayer->prtsp->rtcpChannelId;
			pplayer->send_data = rl_player_sendout_udp_as_inline_data;
		}
		else {
			/* Not supported currently */
			pplayer->prtsp->status = RTSP_STATUS_461;	// Unsupported transport
			return FALSE;
		}

	} else {
		pplayer->prtsp->status = RTSP_STATUS_461;	// Unsupported transport
		return FALSE;
	}

	/*
	 * Buildup the response header
	 */
	// Build up the request line
	bytesused = 0;

	// Build up the Transport
	if (pplayer->prtsp->streamingMode == RTP_TCP) {
		bytesused += snprintf(&outbuf[bytesused], outbufsz-bytesused,
				"Transport: RTP/AVP/TCP;unicast;interleaved=%d-%d\r\n",
				pplayer->rtp[trackID].own_channel_id,
				pplayer->rtcp[trackID].own_channel_id);
	}
	else {
		bytesused += snprintf(&outbuf[bytesused], outbufsz-bytesused,
				"Transport: RTP/AVP;unicast;client_port=%d-%d;server_port=%d-%d\r\n",
				pplayer->rtp[trackID].peer_udp_port,
				pplayer->rtcp[trackID].peer_udp_port,
				pplayer->rtp[trackID].own_udp_port,
				pplayer->rtcp[trackID].own_udp_port );
	}

	bytesused += snprintf(&outbuf[bytesused], outbufsz - bytesused,
			"\r\n" );

	SET_PLAYER_FLAG(pplayer, PLAYERF_SETUP_DONE);
	pplayer->rtp[trackID].setup_done_flag = TRUE;
	pplayer->rtcp[trackID].setup_done_flag = TRUE;

	pplayer->prtsp->status = 200;
	return rl_player_wmp_ret_status_resp(pplayer, outbuf, bytesused);
}

int rl_player_wmp_ret_pause(rtsp_con_player_t * pplayer)
{
	if (!pplayer) return FALSE;
	pplayer->pause = TRUE;
	pplayer->prtsp->status = 200;
	return rl_player_wmp_ret_status_resp(pplayer, NULL, 0);
}

int rl_player_wmp_ret_play(rtsp_con_player_t * pplayer)
{
	rtsp_con_ps_t * pps;
        struct in_addr in;
	int bytesused;
	char outbuf[1024];
	int outbufsz = 1024;
	int i, j;
	//char start[16], end[16];

	if (!pplayer) return FALSE;

	pps = pplayer->pps;
	in.s_addr = pplayer->prtsp->pns->if_addrv4;

	/*
	 * Buildup the response header
	 */
	bytesused = 0;
#if 0
	// Build up the Range: npt
	if (RTSP_IS_FLAG_SET(pps->rtsp.flag, RTSP_HDR_RANGE)) {
		if (CHECK_PPS_FLAG(pps, PPSF_LIVE_VIDEO)) {
			bytesused += snprintf(&outbuf[bytesused], 
						outbufsz-bytesused,
						"Range: npt=now-\r\n");
		}
		else
		{
			start[0] = end[0] = 0;
			if (pps->rtsp.rangeStart >= 0) {
				snprintf( start, 15, "%.3f", 
						pps->rtsp.rangeStart ); 
			}
			if (pps->rtsp.rangeEnd >= 0) {
				snprintf( end, 15, "%.3f", 
						pps->rtsp.rangeEnd ); 
			}
			if (pps->rtsp.rangeStart >= 0 || pps->rtsp.rangeEnd >= 0) {
				bytesused += snprintf(&outbuf[bytesused], 
						outbufsz-bytesused,
						"Range: npt=%s-%s\r\n", 
						start, end);
			}
		}
	}
#endif
	if (RTSP_IS_FLAG_SET(pps->rtsp.flag, RTSP_HDR_SCALE)) {
		bytesused += snprintf(&outbuf[bytesused], outbufsz-bytesused,
					"Scale: %.3f\r\n",
					pps->rtsp.scale);
	}

	// Build up the Content-Base
	if (RTSP_IS_FLAG_SET(pps->rtsp.flag, RTSP_HDR_RTP_INFO)) {
		bytesused += snprintf(&outbuf[bytesused], 
				outbufsz-bytesused, "RTP-Info: ");
		for (i = 0, j=0; i < pps->sdp_info.num_trackID; i++) {
			if (!pplayer->rtp[i].setup_done_flag)
				continue;
			if (pps->sdp_info.track[i].flag_abs_url) {
				bytesused += snprintf(&outbuf[bytesused], 
					outbufsz-bytesused,
					"url=rtsp://%s:%d%s",
					inet_ntoa(in),
					rtsp_server_port,
					pps->sdp_info.track[i].trackID);
			}
			else {
				bytesused += snprintf(&outbuf[bytesused], 
					outbufsz-bytesused,
					"url=rtsp://%s:%d%s/%s",
					inet_ntoa(in),
					rtsp_server_port,
					pps->uri,
					pps->sdp_info.track[i].trackID);
			}

			//if (!CHECK_PPS_FLAG(pps, PPSF_LIVE_VIDEO)) {
				if (pps->rtsp.rtp_info_seq[j] != (uint64_t) ~0L) {
					bytesused += snprintf(&outbuf[bytesused], 
						outbufsz-bytesused,
						";seq=%ld",
						pps->rtsp.rtp_info_seq[j]);
				}
				if (pps->rtsp.rtp_time[j] != (uint64_t) ~0L) {
					bytesused += snprintf(&outbuf[bytesused], 
						outbufsz-bytesused,
						";rtptime=%ld",
						pps->rtsp.rtp_time[j]);
				}
				j++;
			//}
			
			if (i == pps->sdp_info.num_trackID-1) {
				bytesused += snprintf(&outbuf[bytesused], 
					outbufsz-bytesused, "\r\n");
			}
			else {
				bytesused += snprintf(&outbuf[bytesused], 
					outbufsz-bytesused, ", ");
			}
		}
	}
	bytesused += snprintf(&outbuf[bytesused], outbufsz-bytesused,
			"\r\n");

	pplayer->prtsp->status = 200;
	rl_player_wmp_ret_status_resp(pplayer, outbuf, bytesused);
	pplayer->pause = FALSE;
	return TRUE;
}

int rl_player_wmp_ret_get_parameter(rtsp_con_player_t * pplayer)
{
	rtsp_con_ps_t * pps;
	int bytesused;
	char outbuf[RTSP_BUFFER_SIZE + 4];
	int outbufsz = RTSP_BUFFER_SIZE;
	int ret;
	
	if (!pplayer) return FALSE;
	pps = pplayer->pps;
	//if (pps->.method != METHOD_GET_PARAMETER)
	//	return FALSE;

	/* Buildup the response header
	 */
	pplayer->prtsp->status = 200;
	bytesused = 0;

	if (pps->rtsp.cb_resp_content_len) {
		bytesused += snprintf(&outbuf[bytesused],
				  outbufsz - bytesused,
				  "Content-Length: %d\r\n\r\n",
				  pps->rtsp.cb_resp_content_len);
		if ((bytesused + pps->rtsp.cb_resp_content_len) <= (RTSP_BUFFER_SIZE-1024)) {
			memcpy(outbuf + bytesused, 
				pps->rtsp.cb_respbuf + pps->rtsp.cb_respoffsetlen, 
				pps->rtsp.cb_resp_content_len);
			ret = rl_player_wmp_ret_status_resp(pplayer, 
					outbuf, 
					bytesused + pps->rtsp.cb_resp_content_len);
		}
		else {
			rl_player_wmp_ret_status_resp(pplayer, outbuf, bytesused);
			ret = rl_player_sendout_resp(pplayer, 
					pps->rtsp.cb_respbuf + pps->rtsp.cb_respoffsetlen, 
					pps->rtsp.cb_resp_content_len);
		}
		/* Remove content from respbuf. Adjust pointers.
		 */
		pps->rtsp.cb_resplen -= pps->rtsp.cb_resp_content_len;
		if (pps->rtsp.cb_resplen == 0) {
			pps->rtsp.cb_respoffsetlen = 0;
		}
		else {
			pps->rtsp.cb_respoffsetlen += pps->rtsp.cb_resp_content_len;
		}
		pps->rtsp.cb_resp_content_len = 0;
		return ret;
	}
	return rl_player_wmp_ret_status_resp(pplayer, NULL, 0);
}

int rl_player_wmp_ret_set_parameter(rtsp_con_player_t * pplayer)
{
	rtsp_con_ps_t * pps;
	int bytesused;
	char outbuf[RTSP_BUFFER_SIZE + 4];
	int outbufsz = RTSP_BUFFER_SIZE;
	
	if (!pplayer) return FALSE;
	pps = pplayer->pps;
	//if (pps->method != METHOD_SET_PARAM)
	//	return FALSE;

	/* Buildup the response header
	 */
	pplayer->prtsp->status = 200;
	bytesused = 0;

	if (pps->rtsp.cb_resp_content_len) {
		bytesused += snprintf(&outbuf[bytesused],
				  outbufsz - bytesused,
				  "Content-Length: %d\r\n\r\n",
				  pps->rtsp.cb_resp_content_len);
		if ((bytesused + pps->rtsp.cb_resp_content_len) <= (RTSP_BUFFER_SIZE-1024)) {
			memcpy(outbuf + bytesused, 
				pps->rtsp.cb_respbuf + pps->rtsp.cb_respoffsetlen, 
				pps->rtsp.cb_resp_content_len);
			return rl_player_wmp_ret_status_resp(pplayer, 
					outbuf, 
					bytesused + pps->rtsp.cb_resp_content_len);
		}
		else {
			rl_player_wmp_ret_status_resp(pplayer, outbuf, bytesused);
			return rl_player_sendout_resp(pplayer, 
					pps->rtsp.cb_respbuf + pps->rtsp.cb_respoffsetlen, 
					pps->rtsp.cb_resp_content_len);
		}
	}
	return rl_player_wmp_ret_status_resp(pplayer, NULL, 0);
}

int rl_player_wmp_ret_teardown(rtsp_con_player_t * pplayer)
{
	if (!pplayer) return FALSE;
	pplayer->prtsp->status = 200;
	rl_player_wmp_ret_status_resp(pplayer, NULL, 0);
	return TRUE;
}

#if 0
/* Functions for live relay with stream splitting
 */
static int rl_player_live_ret_describe(rtsp_con_player_t * pplayer)
{
	// replace RTSP: Content-Base, Content-Length
	// replace SDP: a=control:
	rtsp_con_ps_t * pps;
	int sdplen; 
	struct in_addr in;
	char sdp[RTSP_BUFFER_SIZE + 4];
	int bytesused;
	char outbuf[RTSP_BUFFER_SIZE + 4];
	int outbufsz = RTSP_BUFFER_SIZE;
	const char *data;
	int datalen;
	u_int32_t attrs;
	int hdrcnt;
	int rv;
	int port;
	
	if (!pplayer) return FALSE;
	pps = pplayer->pps;
	in.s_addr = pplayer->own_ip_addr; //rtsp.pns->if_addr;

	sdplen = rl_player_form_sdp(pplayer, sdp, RTSP_BUFFER_SIZE);

	rv = get_known_header(&pplayer->prtsp->hdr, RTSP_HDR_X_PORT, &data, 
				&datalen, &attrs, &hdrcnt);
	if (!rv && datalen) {
		port = atoi(data);
	}
	else {
		port = rtsp_server_port;
	}

	/*
	 * Buildup the response header
	 */
	bytesused = 0;

	bytesused += snprintf(&outbuf[bytesused],
			outbufsz - bytesused,
			"Content-Base: rtsp://%s:%d%s/\r\n"
			"Content-Type: application/sdp\r\n"
			"Content-Length: %d\r\n\r\n",
			inet_ntoa(in),
			port,
			pps->uri,
			sdplen);
  
	pplayer->prtsp->status = 200;

	/* 
	 * Work around for nload issue, send sdp info in same packet
	 */
	if ((bytesused + sdplen) <= (RTSP_BUFFER_SIZE-1024)) {
		memcpy(outbuf + bytesused, sdp, sdplen);
		rl_player_live_ret_status_resp(pplayer, outbuf, bytesused + sdplen);
	}
	else {
		rl_player_live_ret_status_resp(pplayer, outbuf, bytesused);
		rl_player_sendout_resp(pplayer, sdp, sdplen);
	}

	return TRUE;
}

static int rl_player_live_ret_setup(rtsp_con_player_t * pplayer)
{
	rtsp_con_ps_t * pps;
	int trackID;
	int bytesused;
	char outbuf[1024];
	int outbufsz = 1024;
	const char *data;
	int datalen;
	u_int32_t attrs;
	int hdrcnt;
	int rv;
	char * p;
	struct in_addr ifAddr;
	int len;
	
	pplayer->prtsp->status = RTSP_STATUS_400;
	if (!pplayer) return FALSE;
	pps = pplayer->pps;
	// extract trackID
	rv = get_known_header(&pplayer->prtsp->hdr, RTSP_HDR_X_ABS_PATH, &data,
				&datalen, &attrs, &hdrcnt);
	if (rv || !datalen) { return FALSE; }

	for (trackID = 0; trackID < pplayer->pps->sdp_info.num_trackID; trackID++) {
		len = strlen(pplayer->pps->sdp_info.track[trackID].trackID);
		if (pplayer->pps->sdp_info.track[trackID].flag_abs_url)
			p = (char *) data;
		else
			p = (char *) data + (datalen - len);
		if (strncmp(pplayer->pps->sdp_info.track[trackID].trackID, 
				p, 
				len) == 0)
			break;
	}
	if (trackID == pplayer->pps->sdp_info.num_trackID) {
		pplayer->prtsp->status = RTSP_STATUS_462;	// Destination unreachable
		return FALSE;
	}

	if (pplayer->prtsp->streamingMode == RTP_UDP) {
		pplayer->rtp[trackID].peer_udp_port = pplayer->prtsp->clientRTPPortNum;
		pplayer->rtcp[trackID].peer_udp_port = pplayer->prtsp->clientRTCPPortNum;
		if (pplayer->pps->rtsp.streamingMode == RTP_UDP) {
			if( pps->pplayer_list->player_type == RL_PLAYER_TYPE_FAKE_PLAYER ) {
				pplayer->send_data = orp_send_data_to_tfm_udp;
			}
			else {
				pplayer->send_data = rl_player_sendout_udp_data;
			}
		}
		else if (pplayer->pps->rtsp.streamingMode == RTP_TCP) {
			if( pps->pplayer_list->player_type == RL_PLAYER_TYPE_FAKE_PLAYER ) {
				pplayer->send_data = orp_send_data_to_tfm_tcp;
			}
			else {
				pplayer->send_data = rl_player_sendout_inline_as_udp_data;
			}
		}
		else {
			/* Not supported currently */
			pplayer->prtsp->status = RTSP_STATUS_461;	// Unsupported transport
			return FALSE;
		}

		// we should open our UDP port first with stream server
		pthread_mutex_lock(&rtp_port_mutex);
		/*
		 * BUG: TBD, for T-Proxy, we need to set right UDP in iptables */
		//ifAddr.s_addr = pplayer->rtp[trackID].own_ip_addr;
		ifAddr.s_addr = pplayer->prtsp->pns->if_addr;
		while(1) {
			rtp_udp_port += 2;
			if (rtp_udp_port < 1024)
				rtp_udp_port = 1026;
			pplayer->rtp[trackID].udp_fd = rl_get_udp_sockfd(htons(rtp_udp_port), 0, 0, &ifAddr);
			if(pplayer->rtp[trackID].udp_fd != -1) break;
		}
		pplayer->rtp[trackID].own_udp_port = rtp_udp_port;
		//pthread_mutex_unlock(&rtp_port_mutex);
		NM_set_socket_nonblock(pplayer->rtp[trackID].udp_fd);	

		register_NM_socket(pplayer->rtp[trackID].udp_fd,
				(void *)pplayer,
				rtp_player_epollin,
				rtp_player_epollout,
				rtp_player_epollerr,
				rtp_player_epollhup,
				NULL,
				NULL,
				0,
				USE_LICENSE_FALSE,
				FALSE);

		NM_add_event_epollin(pplayer->rtp[trackID].udp_fd);

		// we should open our UDP port first with stream server
		//pthread_mutex_lock(&rtcp_port_mutex);
		rtcp_udp_port = rtp_udp_port + 1;
		while(1) {
			pplayer->rtcp[trackID].udp_fd = rl_get_udp_sockfd(htons(rtcp_udp_port), 0, 0, &ifAddr);
			if(pplayer->rtcp[trackID].udp_fd != -1) break;
			rtcp_udp_port += 2;
			if (rtcp_udp_port < 1024)
				rtcp_udp_port = 1027;
		}
		pplayer->rtcp[trackID].own_udp_port = rtcp_udp_port;
		pthread_mutex_unlock(&rtp_port_mutex);
		NM_set_socket_nonblock(pplayer->rtcp[trackID].udp_fd);

		register_NM_socket(pplayer->rtcp[trackID].udp_fd,
				(void *)pplayer,
				rtcp_player_epollin,
				rtcp_player_epollout,
				rtcp_player_epollerr,
				rtcp_player_epollhup,
				NULL,
				NULL,
				0,
				USE_LICENSE_FALSE,
				FALSE);

		NM_add_event_epollin(pplayer->rtcp[trackID].udp_fd);

	/*	if (pplayer->rtcp[trackID].udp_fd >= g_rl_maxfd) {
			g_rl_maxfd = pplayer->rtcp[trackID].udp_fd + 1;
		}*/
	} else if (pplayer->prtsp->streamingMode == RTP_TCP){
		if (pplayer->pps->rtsp.streamingMode == RTP_TCP) {
			/* Server mode is also TCP, use server's channel ID's.
			 */
			pplayer->rtp[trackID].own_channel_id = pps->rtp[trackID].own_channel_id;
			pplayer->rtcp[trackID].own_channel_id = pps->rtcp[trackID].own_channel_id;
			pplayer->send_data = rl_player_sendout_inline_data;
			DBG_LOG(MSG, MOD_RTSP, "Player TCP, Server TCP, Track=%d id=%d-%d", 
				trackID, pplayer->rtp[trackID].own_channel_id,
				pplayer->rtcp[trackID].own_channel_id); 
		}
		else if (pplayer->pps->rtsp.streamingMode == RTP_UDP) {
			/* Server mode UDP, use player's channel ID's. And set
			 * the send function to udp_to_inline conversion function.
			 */
			pplayer->rtp[trackID].own_channel_id = pplayer->prtsp->rtpChannelId;
			pplayer->rtcp[trackID].own_channel_id = pplayer->prtsp->rtcpChannelId;
			pplayer->send_data = rl_player_sendout_udp_as_inline_data;
			DBG_LOG(MSG, MOD_RTSP, "Player TCP, Server UDP, Track=%d id=%d-%d", 
				trackID, pplayer->rtp[trackID].own_channel_id,
				pplayer->rtcp[trackID].own_channel_id); 
		}
		else {
			/* Not supported currently */
			pplayer->prtsp->status = RTSP_STATUS_461;	// Unsupported transport
			return FALSE;
		}

	} else {
		pplayer->prtsp->status = RTSP_STATUS_461;	// Unsupported transport
		return FALSE;
	}

	/*
	 * Buildup the response header
	 */
	// Build up the request line
	bytesused = 0;
	
	// Build up the Transport
	if (pplayer->prtsp->streamingMode == RTP_TCP) {
		bytesused += snprintf(&outbuf[bytesused], outbufsz-bytesused,
				"Transport: RTP/AVP/TCP;unicast;interleaved=%d-%d\r\n",
				pplayer->rtp[trackID].own_channel_id,
				pplayer->rtcp[trackID].own_channel_id);
	}
	else {
		bytesused += snprintf(&outbuf[bytesused], outbufsz-bytesused,
				"Transport: RTP/AVP;unicast;client_port=%d-%d;server_port=%d-%d\r\n",
				pplayer->rtp[trackID].peer_udp_port,
				pplayer->rtcp[trackID].peer_udp_port,
				pplayer->rtp[trackID].own_udp_port,
				pplayer->rtcp[trackID].own_udp_port );
	}

	/* For QT player and QTSS server combination pass the late tolernace 
	 * header from server to player
	 */
	if (pps && CHECK_PPS_FLAG(pps, PPSF_SERVER_QTSS) && 
			CHECK_PLAYER_FLAG(pplayer, PLAYERF_PLAYER_QT) && 
			(pps->rtsp.qt_late_tolerance != 0)) {
		bytesused += snprintf(&outbuf[bytesused], outbufsz - bytesused, 
				"%s: late-tolerance=%.6f\r\n",
				rtsp_known_headers[RTSP_HDR_X_QT_LATE_TOLERANCE].name,
				pps->rtsp.qt_late_tolerance);
	}

	bytesused += snprintf(&outbuf[bytesused], outbufsz - bytesused,
			"Cache-Control: no-cache\r\n\r\n" );

	SET_PLAYER_FLAG(pplayer, PLAYERF_SETUP_DONE);

	pplayer->prtsp->status = 200;
	return rl_player_live_ret_status_resp(pplayer, outbuf, bytesused);
}

static int rl_player_live_ret_play(rtsp_con_player_t * pplayer)
{
	rtsp_con_ps_t * pps;
	struct in_addr in;
	int bytesused;
	char outbuf[1024];
	int outbufsz = 1024;
	int i;
	char start[16], end[16];
	const char *data;
	int datalen;
	u_int32_t attrs;
	int hdrcnt;
	int rv;
	int port;

	if (!pplayer) return FALSE;

	pps = pplayer->pps;
	in.s_addr = pplayer->prtsp->pns->if_addr;

	/*
	 * Buildup the response header
	 */
	bytesused = 0;

	if (CHECK_PPS_FLAG(pps, PPSF_LIVE_VIDEO)) {
		bytesused += snprintf(&outbuf[bytesused], 
					outbufsz-bytesused,
					"Range: npt=now-\r\n");
	}
	
	// Build up the Range: npt
	if (RTSP_IS_FLAG_SET(pps->rtsp.flag, RTSP_HDR_RANGE)) {
		start[0] = end[0] = 0;
		if (pps->rtsp.rangeStart >= 0) {
			snprintf( start, 15, "%.3f", 
					pps->rtsp.rangeStart ); 
		}
		if (pps->rtsp.rangeEnd >= 0) {
			snprintf( end, 15, "%.3f", 
					pps->rtsp.rangeEnd ); 
		}
		if (pps->rtsp.rangeStart >= 0 || pps->rtsp.rangeEnd >= 0) {
			bytesused += snprintf(&outbuf[bytesused], 
					outbufsz-bytesused,
					"Range: npt=%s-%s\r\n", 
					start, end);
		}
	}

	if (RTSP_IS_FLAG_SET(pps->rtsp.flag, RTSP_HDR_SCALE)) {
		bytesused += snprintf(&outbuf[bytesused], outbufsz-bytesused,
					"Scale: %.3f\r\n",
					pps->rtsp.scale);
	}

	// Build up the Content-Base
	if (RTSP_IS_FLAG_SET(pps->rtsp.flag, RTSP_HDR_RTP_INFO) || CHECK_PPS_FLAG(pps, PPSF_LIVE_VIDEO)) {
		/* Send correct port number in RTP-Info header.
		 */
		rv = get_known_header(&pplayer->prtsp->hdr, RTSP_HDR_X_PORT, &data, 
					&datalen, &attrs, &hdrcnt);
		if (!rv && datalen) {
			port = atoi(data);
		}
		else {
			port = rtsp_server_port;
		}
		
		bytesused += snprintf(&outbuf[bytesused], 
				outbufsz-bytesused, "RTP-Info: ");
		for (i = 0; i < pps->sdp_info.num_trackID; i++) {
			if (pps->sdp_info.track[i].flag_abs_url) {
				bytesused += snprintf(&outbuf[bytesused], 
					outbufsz-bytesused,
					"url=rtsp://%s:%d%s",
					inet_ntoa(in),
					port,
					pps->sdp_info.track[i].trackID);
			}
			else {
				bytesused += snprintf(&outbuf[bytesused], 
					outbufsz-bytesused,
					"url=rtsp://%s:%d%s/%s",
					inet_ntoa(in),
					port,
					pps->uri,
					pps->sdp_info.track[i].trackID);
			}

			if (!CHECK_PPS_FLAG(pps, PPSF_LIVE_VIDEO)) {
				if (pps->rtsp.rtp_info_seq[i] != (uint64_t) ~0L) {
					bytesused += snprintf(&outbuf[bytesused], 
						outbufsz-bytesused,
						";seq=%ld",
						pps->rtsp.rtp_info_seq[i]);
				}
				if (pps->rtsp.rtp_time[i] != (uint64_t) ~0L) {
					bytesused += snprintf(&outbuf[bytesused], 
						outbufsz-bytesused,
						";rtptime=%ld",
						pps->rtsp.rtp_time[i]);
				}
			}
			
			if (i == pps->sdp_info.num_trackID-1) {
				bytesused += snprintf(&outbuf[bytesused], 
					outbufsz-bytesused, "\r\n");
			}
			else {
				bytesused += snprintf(&outbuf[bytesused], 
					outbufsz-bytesused, ",");
			}
		}
	}
	bytesused += snprintf(&outbuf[bytesused], outbufsz-bytesused,
			"Cache-Control: no-cache\r\n\r\n");

	pplayer->prtsp->status = 200;
	rl_player_live_ret_status_resp(pplayer, outbuf, bytesused);
	pplayer->pause = FALSE;
	return TRUE;
}
#endif

int rl_player_wmp_set_callbacks( rtsp_con_player_t * pplayer )
{
	pplayer->ret_options	   = rl_player_wmp_ret_options;
	pplayer->ret_describe	   = rl_player_wmp_ret_describe;
	pplayer->ret_setup	   = rl_player_wmp_ret_setup;
	pplayer->ret_play	   = rl_player_wmp_ret_play;
	pplayer->ret_pause	   = rl_player_wmp_ret_pause;
	pplayer->ret_teardown	   = rl_player_wmp_ret_teardown;
	pplayer->ret_get_parameter = rl_player_wmp_ret_get_parameter;
	pplayer->ret_set_parameter = rl_player_wmp_ret_set_parameter;
	return TRUE;
}

int rl_ps_wms_set_callbacks( rtsp_con_ps_t * pps )
{
	pps->send_options 	= rl_ps_wms_send_method_options;
	pps->send_describe 	= rl_ps_wms_send_method_describe;
	pps->send_setup 	= rl_ps_wms_send_method_setup;
	pps->send_play 		= rl_ps_wms_send_method_play;
	pps->send_pause 	= rl_ps_wms_send_method_pause;
	pps->send_teardown 	= rl_ps_wms_send_method_teardown;
	pps->send_get_parameter = rl_ps_wms_send_method_get_parameter;
	pps->send_set_parameter = rl_ps_wms_send_method_set_parameter;

	pps->recv_options 	= rl_ps_wms_recv_method_options;
	pps->recv_describe 	= rl_ps_wms_recv_method_describe;
	pps->recv_setup 	= rl_ps_wms_recv_method_setup;
	pps->recv_play 		= rl_ps_wms_recv_method_play;
	pps->recv_pause 	= rl_ps_wms_recv_method_pause;
	pps->recv_teardown 	= rl_ps_wms_recv_method_teardown;
	pps->recv_get_parameter = rl_ps_wms_recv_method_get_parameter;
	pps->recv_set_parameter = rl_ps_wms_recv_method_set_parameter;
	return TRUE;
}


#define OUTBUF(_data, _datalen) { \
    if (((_datalen)+bytesused) >= outbufsz) { \
   	DBG_LOG(MSG, MOD_RTSP, "datalen(%d)+bytesused(%d) >= outbufsz(%d)",  \
		(_datalen), bytesused, outbufsz); \
	outbuf[outbufsz-1] = '\0'; \
	return outbuf; \
    } \
    memcpy((void *)&outbuf[bytesused], (_data), (_datalen)); \
    bytesused += (_datalen); \
}

#define OUTBUF_PTR (&outbuf[bytesused])
#define OUTBUF_REM_LEN (outbufsz-bytesused)
#define INC_OUTBUF_LEN(_datalen) { \
    if (((_datalen)+bytesused) >= outbufsz) { \
   	DBG_LOG(MSG, MOD_RTSP, "datalen(%d)+bytesused(%d) >= outbufsz(%d)",  \
		(_datalen), bytesused, outbufsz); \
	outbuf[outbufsz-1] = '\0'; \
	return outbuf; \
    } \
    bytesused += (_datalen); \
}

#if 0
#define INC_OUTBUF(_datalen) { \
	if (((_datalen)+bytesused) >= outbufsz ) { \
		if (outbufsz < 16*1024) { \
			*outbufsz_p = outbufsz * 2; \
			outbufsz = *outbufsz_p; \
			outbuf_p = (char *)nkn_realloc_type(*outbuf_p, outbufsz, mod_om_ocon_t); \
			outbuf = *outbuf_p; \
			DBG_LOG(MSG, MOD_RTSP, "datalen(%d)+bytesused(%d) >= outbufsz(%d)",  \
			(_datalen), bytesused, outbufsz); \
			} \
		else { \
			DBG_LOG(MSG, MOD_RTSP, "datalen(%d)+bytesused(%d) >= outbufsz(%d)",  \
			(_datalen), bytesused, outbufsz); \
			outbuf[outbufsz-1] = '\0'; \
			return outbuf; \
		} \
	} \
}
#else // 0
#define INC_OUTBUF(_datalen) { \
}
#endif // 0

char * rl_ps_wms_buildup_req_head(rtsp_con_ps_t * pps, char * add_hdr, int add_hdr_size)
{
	rtsp_con_player_t * pplayer;
	char strbuf[1024];
	int rv;
	int token;
	int nth;
	const char *name;
	int namelen;
	const char *data;
	int datalen;
	u_int32_t attrs;
	int hdrcnt;
	int bytesused = 0;
	char *outbuf;
	int outbufsz;

	if (!pps) return NULL;
	pplayer = pps->pplayer_list;
	outbuf = pps->rtsp.cb_reqbuf;
	outbufsz = RTSP_REQ_BUFFER_SIZE;
	pps->rtsp.cb_reqoffsetlen = 0;
	if (!pplayer) {
		DBG_LOG(MSG, MOD_RTSP, "pplayer==NULL");
		return 0;
	}
	rv = get_known_header(&pplayer->prtsp->hdr, RTSP_HDR_X_METHOD, 
			&data, &datalen, &attrs, &hdrcnt);
	if (rv) {
		DBG_LOG(MSG, MOD_RTSP, "No RTSP method found");
		return 0;
	}

	// Method
	OUTBUF(data, datalen);
	OUTBUF(" ", 1);

	if ((rv = get_known_header(&pplayer->prtsp->hdr, RTSP_HDR_X_ABS_PATH, 
				&data, &datalen, &attrs, &hdrcnt))) {
		DBG_LOG(MSG, MOD_RTSP, "No URI");
		return 0;
#if 0
		/* BZ 3909: for tproxy case, we should give preference to 
		 * X_NKN_ABS_URL_HOST over X_NKN_URI, if the former exists
		 */
		if ( (ocon->oscfg.nsc->http_config->origin.u.http.use_client_ip)
			&& !(rv = get_known_header(&pplayer->prtsp->hdr, 
						RTSP_HDR_X_ABS_PATH,
						&data, &datalen, &attrs, 
						&hdrcnt))) {
			OUTBUF("rtsp://", 7);
			OUTBUF(data, datalen);
		}
    		rv = get_known_header(&pplayer->prtsp->hdr, MIME_HDR_X_NKN_URI, 
				&data, &datalen, &attrs, &hdrcnt);
    		if (rv) {
			DBG_LOG(MSG, MOD_RTSP, "No URI");
			return 0;
    		}
#endif // 0
	}
	// URI
	rv = snprintf(strbuf, sizeof(strbuf), "rtsp://%s", pps->hostname);
	OUTBUF(strbuf, rv);
	OUTBUF(data, datalen);
	OUTBUF(" ", 1);

	// RTSP version
	if ((rv = get_known_header(&pplayer->prtsp->hdr, RTSP_HDR_X_VERSION, 
				&data, &datalen, &attrs, &hdrcnt))) {
		DBG_LOG(MSG, MOD_RTSP, "No VERSION");
		return 0;
	}
	OUTBUF(data, datalen);
	OUTBUF("\r\n", 2);

	rv = get_known_header(&pplayer->prtsp->hdr, RTSP_HDR_USER_AGENT, &data, 
			&datalen, &attrs, &hdrcnt);
	if (!rv) {
		OUTBUF(rtsp_known_headers[RTSP_HDR_USER_AGENT].name, 
			rtsp_known_headers[RTSP_HDR_USER_AGENT].namelen);
		OUTBUF(": ", 2);
		OUTBUF(data, datalen);
		OUTBUF("\r\n", 2);
	}
	
	/* Add CSEQ Header */
	rv = snprintf(strbuf, sizeof(strbuf), 
			"CSeq: %d\r\n",
			pps->cseq++);
	OUTBUF(strbuf, rv);

	/* Add session Header */
	if (pps->session) { //VLC doesnt re-start sesssions anyway!
		rv = snprintf(strbuf, sizeof(strbuf), 
			"Session: %s\r\n",
			pps->session);
		OUTBUF(strbuf, rv);
	}
#if 0
	/* Add User Agent */
	rv = snprintf(strbuf, sizeof(strbuf), 
			"User-Agent: %s\r\n",
			rl_rtsp_player_str);
	OUTBUF(strbuf, rv);
#endif

	// Add known headers
	for (token = 0; token < RTSP_HDR_MAX_DEFS; token++) {
		// Don't forward the following headers
		switch(token) {
		case RTSP_HDR_CSEQ:
		case RTSP_HDR_RANGE: // Don't forward Range
		case RTSP_HDR_TRANSPORT:
		case RTSP_HDR_SESSION:
		case RTSP_HDR_USER_AGENT:
		case RTSP_HDR_CONTENT_LENGTH:

		case RTSP_HDR_X_METHOD:
		case RTSP_HDR_X_PROTOCOL:
		case RTSP_HDR_X_HOST:
		case RTSP_HDR_X_PORT:
		case RTSP_HDR_X_URL:
		case RTSP_HDR_X_ABS_PATH:
		case RTSP_HDR_X_VERSION:
		case RTSP_HDR_X_VERSION_NUM:
		case RTSP_HDR_X_STATUS_CODE:
		case RTSP_HDR_X_REASON:
		case RTSP_HDR_X_QT_LATE_TOLERANCE:
		case RTSP_HDR_X_NKN_REQ_REAL_DEST_IP:
			continue;
		default:
			break;
		}

        	rv = get_known_header(&pplayer->prtsp->hdr, token, &data, 
				&datalen, &attrs, &hdrcnt);
		if (!rv) {
			INC_OUTBUF(rtsp_known_headers[token].namelen+datalen+1);
			OUTBUF(rtsp_known_headers[token].name, 
				rtsp_known_headers[token].namelen);
			OUTBUF(": ", 2);
			OUTBUF(data, datalen);

		} else {
			continue;
		}
		for (nth = 1; nth < hdrcnt; nth++) {
			rv = get_nth_known_header(&pplayer->prtsp->hdr, 
				token, nth, &data, &datalen, &attrs);
			if (!rv) {
				INC_OUTBUF(datalen+3);
				OUTBUF(",", 1);
				OUTBUF(data, datalen);
			}
		}
		OUTBUF("\r\n", 2);
	}

	// Add unknown headers
	nth = 0;
	while ((rv = get_nth_unknown_header(&pplayer->prtsp->hdr, nth, &name, 
			&namelen, &data, &datalen, &attrs)) == 0) {
		nth++;
		INC_OUTBUF(namelen+datalen+3);
		OUTBUF(name, namelen);
		OUTBUF(": ", 2);
		OUTBUF(data, datalen);
		OUTBUF("\r\n", 2);
	}

	/* Add passed in headers */
	if (add_hdr) {
		OUTBUF(add_hdr, add_hdr_size);
	}

	OUTBUF("\r\n", 2);
	pps->rtsp.cb_reqlen = bytesused;
	outbuf[pps->rtsp.cb_reqlen] = 0;
	return outbuf;
}

char * rl_ps_wms_send_method_options(rtsp_con_ps_t * pps)
{
	if (!pps) return NULL;
	pps->method = METHOD_OPTIONS;
	rl_ps_wms_buildup_req_head(pps, NULL, 0);
	rl_ps_sendout_request(pps);
	return NULL;
}

char * rl_ps_wms_send_method_describe(rtsp_con_ps_t * pps)
{
	if (!pps) return NULL;
	pps->method = METHOD_DESCRIBE;
	rl_ps_wms_buildup_req_head(pps, NULL, 0);
	rl_ps_sendout_request(pps);
	return NULL;
}


char * rl_ps_wms_send_method_setup(rtsp_con_ps_t * pps)
{
	// the request uri example:
	// SETUP rtsp://172.19.172.151:8080/vlc.sdp/trackID=0 RTSP/1.0
	int trackID;
	int bytesused = 0;
	char outbuf[1024];
	int outbufsz = sizeof(outbuf);
	struct in_addr ifAddr;
	//const char *data;
	int32_t datalen, hdrcnt, rv;
	//uint32_t attrs;

	if (!pps) return NULL;
	pps->method = METHOD_SETUP;
	datalen = hdrcnt = rv = 0;
	RTSP_GET_ALLOC_URL_COMPONENT(&pps->pplayer_list->prtsp->hdr, 
			pps->pplayer_list->prtsp->urlPreSuffix, 
			pps->pplayer_list->prtsp->urlSuffix);
	trackID = rl_get_track_num(pps, pps->pplayer_list->prtsp->urlSuffix);
	//trackID = pps->cur_trackID;;

	// we should open our UDP port first with stream server
	//i = rtp_udp_port;

	// Force it to TCP, for testing 
	//pps->pplayer_list->prtsp->streamingMode = RTP_TCP;
	//pps->rtsp.streamingMode = RTP_TCP;

	pps->rtsp.streamingMode = pps->pplayer_list->prtsp->streamingMode;
	if (pps->rtsp.streamingMode == RTP_UDP) {
		pthread_mutex_lock(&rtp_port_mutex);
		ifAddr.s_addr = pps->own_ip_addr;
		while(1) {
			rtp_udp_port += 2;
			if (rtp_udp_port < 1024)
				rtp_udp_port = 1026;
			/* For origin side, interface is not known, 
			 * don't pass IP address to bind
			 * BZ 4248
			 */
			pps->rtp[trackID].udp_fd = rl_get_udp_sockfd(htons(rtp_udp_port), 0, 0, NULL);
			if(pps->rtp[trackID].udp_fd != -1) break;
		}
		pps->rtp[trackID].own_udp_port = rtp_udp_port;
		//pthread_mutex_unlock(&rtp_port_mutex);

		NM_set_socket_nonblock(pps->rtp[trackID].udp_fd);

		register_NM_socket(pps->rtp[trackID].udp_fd,
				   (void *)pps,
				   rtp_ps_epollin,
				   rtp_ps_epollout,
				   rtp_ps_epollerr,
				   rtp_ps_epollhup,
				   NULL,
				   NULL,
				   0,
				   USE_LICENSE_FALSE,
				   FALSE);
		NM_add_event_epollin(pps->rtp[trackID].udp_fd);

		// we should open our UDP port first with stream server
		//pthread_mutex_lock(&rtcp_port_mutex);
		rtcp_udp_port = rtp_udp_port + 1;
		while (1) {
			/* For origin side, interface is not known, 
			 * don't pass IP address to bind
			 * BZ 4248
			 */
			pps->rtcp[trackID].udp_fd = rl_get_udp_sockfd(htons(rtcp_udp_port), 0, 0, NULL);
			if (pps->rtcp[trackID].udp_fd != -1) break;
			rtcp_udp_port += 2;
			if (rtcp_udp_port < 1024)
				rtcp_udp_port = 1027;
		}
		pps->rtcp[trackID].own_udp_port = rtcp_udp_port;
		pthread_mutex_unlock(&rtp_port_mutex);

		NM_set_socket_nonblock(pps->rtcp[trackID].udp_fd);

		register_NM_socket(pps->rtcp[trackID].udp_fd,
				   (void *)pps,
				   rtcp_ps_epollin,
				   rtcp_ps_epollout,
				   rtcp_ps_epollerr,
				   rtcp_ps_epollhup,
				   NULL,
				   NULL,
				   0,
				   USE_LICENSE_FALSE,
				   FALSE);
	
		NM_add_event_epollin(pps->rtcp[trackID].udp_fd);

	}
	else {
		pps->rtp[trackID].own_channel_id = pps->pplayer_list->prtsp->rtpChannelId;
		pps->rtcp[trackID].own_channel_id = pps->pplayer_list->prtsp->rtcpChannelId;
	}

	// Build up the Transport
	if (pps->rtsp.streamingMode == RTP_TCP) {
		bytesused += snprintf(&outbuf[bytesused], outbufsz-bytesused,
				"Transport: RTP/AVP/TCP;unicast;interleaved=%d-%d",
				pps->rtp[trackID].own_channel_id,
				pps->rtcp[trackID].own_channel_id);
	}
	else {
		bytesused += snprintf(&outbuf[bytesused], outbufsz-bytesused,
				"Transport: RTP/AVP;unicast;client_port=%d-%d",
				pps->rtp[trackID].own_udp_port,
				pps->rtcp[trackID].own_udp_port);
	}
	if (strlen(pps->pplayer_list->prtsp->ssrc)) {
		bytesused += snprintf(&outbuf[bytesused], outbufsz-bytesused,
				";ssrc=%s", pps->pplayer_list->prtsp->ssrc);
	}
	if (strlen(pps->pplayer_list->prtsp->mode)) {
		bytesused += snprintf(&outbuf[bytesused], outbufsz-bytesused,
				";mode=%s", pps->pplayer_list->prtsp->mode);
	}
	bytesused += snprintf(&outbuf[bytesused], outbufsz-bytesused, "\r\n");

	rl_ps_wms_buildup_req_head(pps, outbuf, bytesused);
	rl_ps_sendout_request(pps);
	return NULL;
}

char * rl_ps_wms_send_method_play(rtsp_con_ps_t * pps)
{
	char npt_buf[ 128 ];
	char start[16], end[16], *p;
	int len;
	int bytesused = 0;
	char outbuf[1024];
	int outbufsz = sizeof(outbuf);
	
	if (!pps) return NULL;
	//pthread_mutex_lock(&rlcon_mutex);
	pps->method = METHOD_PLAY;
	//len = snprintf( npt_buf, 128, "Range: npt=%s-%s", 
	//		pps->sdp_info.npt_start, 
	//		pps->sdp_info.npt_stop );
	p = npt_buf;
	if (CHECK_PPS_FLAG(pps, PPSF_LIVE_VIDEO)) {
		bytesused += snprintf(&outbuf[bytesused], outbufsz-bytesused,
				"Range: npt=0.000-\r\n" ); 
	}
	else
	{
		start[0] = end[0] = 0;
		if (pps->pplayer_list->prtsp->rangeStart >= 0) {
			snprintf( start, 15, "%.3f", 
					pps->pplayer_list->prtsp->rangeStart ); 
		}
		if (pps->pplayer_list->prtsp->rangeEnd >= 0) {
			snprintf( end, 15, "%.3f", 
					pps->pplayer_list->prtsp->rangeEnd ); 
		}
		else {
			strncpy(end, pps->sdp_info.npt_stop, 15);
			end[15] = 0;
		}
		bytesused += snprintf(&outbuf[bytesused], outbufsz-bytesused,
				"Range: npt=%s-%s\r\n",
				start, end);
		if ((pps->pplayer_list->prtsp->rangeStart < 0) &&
				(pps->pplayer_list->prtsp->rangeEnd < 0)) {
			npt_buf[0] = 0;
			len = 0;
		}
#if 0
		if (RTSP_IS_FLAG_SET(pps->pplayer_list->prtsp->flag, RTSP_HDR_SCALE)) {
			len = snprintf( npt_buf + len, 127 - len, 
				"%sScale: %.3f",
				len ? "\r\n" : "",
				pps->pplayer_list->prtsp->scale);
		}
#endif // 0
	}
	rl_ps_wms_buildup_req_head(pps, outbuf, bytesused);
	rl_ps_sendout_request(pps);
	return NULL;
}

char *rl_ps_wms_send_method_pause(rtsp_con_ps_t * pps)
{
	if (!pps) return NULL;
	pps->method = METHOD_PAUSE;
	rl_ps_wms_buildup_req_head(pps, NULL, 0);
	rl_ps_sendout_request(pps);
	return NULL;
}

char *rl_ps_wms_send_method_get_parameter(rtsp_con_ps_t * pps)
{
	int len;
	int content_length;
	char outbuf[1024];
	int outbufsz = sizeof(outbuf);

	if (!pps) return NULL;
	pps->method = METHOD_GET_PARAMETER;

	content_length = pps->pplayer_list->prtsp->cb_req_content_len;
	if (content_length) {
		len = snprintf(outbuf, outbufsz, "Content-Length: %d\r\n", content_length);
		rl_ps_wms_buildup_req_head(pps, outbuf, len);
		rl_ps_sendout_request(pps);
		 
		memcpy( &pps->rtsp.cb_reqbuf[0],
			&pps->pplayer_list->prtsp->cb_reqbuf[pps->pplayer_list->prtsp->cb_reqoffsetlen],
			content_length);
		pps->rtsp.cb_reqlen = content_length;
		pps->rtsp.cb_reqoffsetlen = 0;
		rl_ps_sendout_request(pps);

		pps->pplayer_list->prtsp->cb_reqlen -= content_length;
		if (pps->pplayer_list->prtsp->cb_reqlen == 0) {
			pps->pplayer_list->prtsp->cb_reqoffsetlen = 0;
		}
		else {
			pps->pplayer_list->prtsp->cb_reqoffsetlen -= content_length;
		}
		pps->pplayer_list->prtsp->cb_req_content_len = 0;
	}
	else {
		len = snprintf(outbuf, outbufsz, "Content-Length: %d\r\n", content_length);
		rl_ps_wms_buildup_req_head(pps, outbuf, len);
		rl_ps_sendout_request(pps);
	}
	return NULL;
}

char *rl_ps_wms_send_method_set_parameter(rtsp_con_ps_t * pps)
{
	int len;
	int content_length;
	char outbuf[1024];
	int outbufsz = sizeof(outbuf);

	if (!pps) return NULL;
	pps->method = METHOD_SET_PARAM;

	/*
	 * Send out SET_PARAMETER Body.
	 */
	content_length = pps->pplayer_list->prtsp->cb_req_content_len;
	if (content_length) {
		len = snprintf(outbuf, outbufsz, "Content-Length: %d\r\n", content_length);
		rl_ps_wms_buildup_req_head(pps, outbuf, len);
		rl_ps_sendout_request(pps);
		 
		memcpy( &pps->rtsp.cb_reqbuf[0],
			&pps->pplayer_list->prtsp->cb_reqbuf[pps->pplayer_list->prtsp->cb_reqoffsetlen],
			content_length);
		pps->rtsp.cb_reqlen = content_length;
		pps->rtsp.cb_reqoffsetlen = 0;
		rl_ps_sendout_request(pps);

		pps->pplayer_list->prtsp->cb_reqlen -= content_length;
		if (pps->pplayer_list->prtsp->cb_reqlen == 0) {
			pps->pplayer_list->prtsp->cb_reqoffsetlen = 0;
		}
		else {
			pps->pplayer_list->prtsp->cb_reqoffsetlen -= content_length;
		}
		pps->pplayer_list->prtsp->cb_req_content_len = 0;
	}
	else {
		rl_ps_wms_buildup_req_head(pps, NULL, 0);
		rl_ps_sendout_request(pps);
	}
	return NULL;
}

char *rl_ps_wms_send_method_teardown(rtsp_con_ps_t * pps)
{
	if (!pps || CHECK_PPS_FLAG(pps, PPSF_TEARDOWN)) return NULL;
	pps->method = METHOD_TEARDOWN;
	SET_PPS_FLAG(pps, PPSF_TEARDOWN);
	rl_ps_wms_buildup_req_head(pps, NULL, 0);
	rl_ps_sendout_request(pps);
	return NULL;
}

#if 0
static char * rl_ps_live_buildup_req_head(rtsp_con_ps_t * pps, char * add_hdr, int add_hdr_size)
{
	rtsp_con_player_t * pplayer;
	char strbuf[1024];
	int rv;
    	//int token;
    	//int nth;
    	//const char *name;
    	//int namelen;
	const char *data;
	int datalen;
    	u_int32_t attrs;
    	int hdrcnt;
	int bytesused = 0;
	char *outbuf;
	int outbufsz;

	if (!pps) return NULL;
	pplayer = pps->pplayer_list;
	outbuf = pps->rtsp.cb_reqbuf;
	outbufsz = RTSP_REQ_BUFFER_SIZE;
	pps->rtsp.cb_reqoffsetlen = 0;
	if (!pplayer) {
		DBG_LOG(MSG, MOD_RTSP, "pplayer==NULL");
		return 0;
	}
    	rv = get_known_header(&pplayer->prtsp->hdr, RTSP_HDR_X_METHOD, 
			&data, &datalen, &attrs, &hdrcnt);
    	if (rv) {
		DBG_LOG(MSG, MOD_RTSP, "No RTSP method found");
		return 0;
    	}

    	// Method
    	OUTBUF(data, datalen);
    	OUTBUF(" ", 1);

    	if ((rv = get_known_header(&pplayer->prtsp->hdr, RTSP_HDR_X_ABS_PATH, 
				&data, &datalen, &attrs, &hdrcnt))) {
		DBG_LOG(MSG, MOD_RTSP, "No URI");
		return 0;
	}
	// URI
	rv = snprintf(strbuf, sizeof(strbuf), "rtsp://%s:%d", pps->hostname,
			pps->port);
	OUTBUF(strbuf, rv);
	OUTBUF(data, datalen);
	OUTBUF(" ", 1);

	// RTSP version
	if ((rv = get_known_header(&pplayer->prtsp->hdr, RTSP_HDR_X_VERSION, 
				&data, &datalen, &attrs, &hdrcnt))) {
		DBG_LOG(MSG, MOD_RTSP, "No VERSION");
		return 0;
	}
	OUTBUF(data, datalen);
	OUTBUF("\r\n", 2);

	/* Add CSEQ Header */
	rv = snprintf(strbuf, sizeof(strbuf), 
			"CSeq: %d\r\n",
			pps->cseq++);
	OUTBUF(strbuf, rv);

	/* Add session Header */
	if (pps->session) { //VLC doesnt re-start sesssions anyway!
		rv = snprintf(strbuf, sizeof(strbuf), 
			"Session: %s\r\n",
			pps->session);
		OUTBUF(strbuf, rv);
	}

	/* Add User Agent */
	rv = snprintf(strbuf, sizeof(strbuf), 
			"User-Agent: %s\r\n",
			rl_rtsp_player_str);
	OUTBUF(strbuf, rv);

#if 0
	// Add known headers
	for (token = 0; token < RTSP_HDR_MAX_DEFS; token++) {
		// Don't forward the following headers
		switch(token) {
		case RTSP_HDR_CSEQ:
		case RTSP_HDR_RANGE: // Don't forward Range
		case RTSP_HDR_TRANSPORT:
		case RTSP_HDR_SESSION:
		case RTSP_HDR_USER_AGENT:
		case RTSP_HDR_CONTENT_LENGTH:

		case RTSP_HDR_X_METHOD:
		case RTSP_HDR_X_PROTOCOL:
		case RTSP_HDR_X_HOST:
		case RTSP_HDR_X_PORT:
		case RTSP_HDR_X_URL:
		case RTSP_HDR_X_ABS_PATH:
		case RTSP_HDR_X_VERSION:
		case RTSP_HDR_X_VERSION_NUM:
		case RTSP_HDR_X_STATUS_CODE:
		case RTSP_HDR_X_REASON:
		case RTSP_HDR_X_QT_LATE_TOLERANCE:
		case RTSP_HDR_X_NKN_REQ_REAL_DEST_IP:
			continue;
		default:
			break;
		}

        	rv = get_known_header(&pplayer->prtsp->hdr, token, &data, 
				&datalen, &attrs, &hdrcnt);
		if (!rv) {
			INC_OUTBUF(rtsp_known_headers[token].namelen+datalen+1);
			OUTBUF(rtsp_known_headers[token].name, 
				rtsp_known_headers[token].namelen);
			OUTBUF(": ", 2);
			OUTBUF(data, datalen);

		} else {
			continue;
		}
		for (nth = 1; nth < hdrcnt; nth++) {
			rv = get_nth_known_header(&pplayer->prtsp->hdr, 
				token, nth, &data, &datalen, &attrs);
			if (!rv) {
				INC_OUTBUF(datalen+3);
				OUTBUF(",", 1);
				OUTBUF(data, datalen);
			}
		}
		OUTBUF("\r\n", 2);
	}

	// Add unknown headers
	nth = 0;
	while ((rv = get_nth_unknown_header(&pplayer->prtsp->hdr, nth, &name, 
			&namelen, &data, &datalen, &attrs)) == 0) {
		nth++;
		INC_OUTBUF(namelen+datalen+3);
		OUTBUF(name, namelen);
		OUTBUF(": ", 2);
		OUTBUF(data, datalen);
		OUTBUF("\r\n", 2);
	}
#endif
	/* Add passed in headers */
	if (add_hdr) {
		OUTBUF(add_hdr, add_hdr_size);
	}

	OUTBUF("\r\n", 2);
	//OUTBUF("", 1);
	pps->rtsp.cb_reqlen = bytesused;
	outbuf[pps->rtsp.cb_reqlen] = 0;
	return outbuf;
}

static char * rl_ps_live_form_req_head(rtsp_con_ps_t * pps, const char * method, char * add_hdr, int add_hdr_size)
{
	rtsp_con_player_t * pplayer;
	char strbuf[1024];
	int rv;
	int bytesused = 0;
	char *outbuf;
	int outbufsz;

	if (!pps) return NULL;
	pplayer = pps->pplayer_list;
	outbuf = pps->rtsp.cb_reqbuf;
	outbufsz = RTSP_REQ_BUFFER_SIZE;
	pps->rtsp.cb_reqoffsetlen = 0;
	if (!pplayer) {
		DBG_LOG(MSG, MOD_RTSP, "pplayer==NULL");
		return 0;
	}

	/* Request line */
	rv = snprintf(strbuf, sizeof(strbuf),
			"%s rtsp://%s:%d%s RTSP/1.0\r\n",
			method,
			pps->hostname, 
			pps->port, 
			pps->uri);
	OUTBUF(strbuf, rv);

	/* Add CSEQ Header */
	rv = snprintf(strbuf, sizeof(strbuf), 
			"CSeq: %d\r\n",
			pps->cseq++);
	OUTBUF(strbuf, rv);

	/* Add session Header */
	if (pps->session) { //VLC doesnt re-start sesssions anyway!
		rv = snprintf(strbuf, sizeof(strbuf), 
			"Session: %s\r\n",
			pps->session);
		OUTBUF(strbuf, rv);
	}

	/* Add User Agent */
	rv = snprintf(strbuf, sizeof(strbuf), 
			"User-Agent: %s\r\n",
			rl_rtsp_player_str);
	OUTBUF(strbuf, rv);

	/* Add passed in headers */
	if (add_hdr) {
		OUTBUF(add_hdr, add_hdr_size);
	}

	OUTBUF("\r\n", 2);
	pps->rtsp.cb_reqlen = bytesused;
	outbuf[pps->rtsp.cb_reqlen] = 0;
	return outbuf;



}

static char * rl_ps_live_send_method_options(rtsp_con_ps_t * pps)
{
	if (!pps) return NULL;
	pps->method = METHOD_OPTIONS;
	if (CHECK_PPS_FLAG(pps, PPSF_OWN_REQUEST)) {
		rl_ps_live_form_req_head(pps, "OPTIONS", NULL, 0);
	}
	else {
		rl_ps_live_buildup_req_head(pps, NULL, 0);
	}
	rl_ps_sendout_request(pps);
	return NULL;
}

static char * rl_ps_live_send_method_describe(rtsp_con_ps_t * pps)
{
	int bytesused = 0;
	char outbuf[1024];
	int outbufsz = sizeof(outbuf);

	if (!pps) return NULL;

	pps->method = METHOD_DESCRIBE;
	bytesused += snprintf(&outbuf[bytesused], outbufsz-bytesused,
			"Accept: application/sdp\r\n" ); 

	rl_ps_live_buildup_req_head(pps, outbuf, bytesused);
	rl_ps_sendout_request(pps);
	return NULL;
}


static char * rl_ps_live_send_method_setup(rtsp_con_ps_t * pps)
{
	// the request uri example:
	// SETUP rtsp://172.19.172.151:8080/vlc.sdp/trackID=0 RTSP/1.0
	int trackID;
	int bytesused = 0;
	char outbuf[1024];
	int outbufsz = sizeof(outbuf);
	struct in_addr ifAddr;
	//const char *data;
	int32_t datalen, hdrcnt, rv;
	//uint32_t attrs;

	if (!pps) return NULL;
	pps->method = METHOD_SETUP;
	datalen = hdrcnt = rv = 0;
	trackID = pps->cur_trackID;;

	// we should open our UDP port first with stream server
	//i = rtp_udp_port;

	// Force it to TCP, for testing 
	//pps->pplayer_list->prtsp->streamingMode = RTP_TCP;
	//pps->rtsp.streamingMode = RTP_TCP;

	/* Force origin mode to TCP is the server is WMS
	 */
	if (CHECK_PPS_FLAG(pps, PPSF_SERVER_WMS)) {
		pps->rtsp.streamingMode = RTP_TCP;
	}
	else {
		pps->rtsp.streamingMode = pps->pplayer_list->prtsp->streamingMode;
	}
	if (pps->rtsp.streamingMode == RTP_UDP) {
		pthread_mutex_lock(&rtp_port_mutex);
		ifAddr.s_addr = pps->own_ip_addr;
		while(1) {
			rtp_udp_port += 2;
			if (rtp_udp_port < 1024)
				rtp_udp_port = 1026;
			/* For origin side, interface is not known, 
			 * don't pass IP address to bind
			 * BZ 4248
			 */
			pps->rtp[trackID].udp_fd = rl_get_udp_sockfd(htons(rtp_udp_port), 0, 0, NULL);
			if(pps->rtp[trackID].udp_fd != -1) break;
		}
		pps->rtp[trackID].own_udp_port = rtp_udp_port;
		//pthread_mutex_unlock(&rtp_port_mutex);

		NM_set_socket_nonblock(pps->rtp[trackID].udp_fd);
		
		register_NM_socket(pps->rtp[trackID].udp_fd,
				   (void *)pps,
				   rtp_ps_epollin,
				   rtp_ps_epollout,
				   rtp_ps_epollerr,
				   rtp_ps_epollhup,
				   NULL,
				   NULL,
				   0,
				   USE_LICENSE_FALSE,
				   FALSE);
		NM_add_event_epollin(pps->rtp[trackID].udp_fd);

		// we should open our UDP port first with stream server
		//pthread_mutex_lock(&rtcp_port_mutex);
		rtcp_udp_port = rtp_udp_port + 1;
		while (1) {
			/* For origin side, interface is not known, 
			 * don't pass IP address to bind
			 * BZ 4248
			 */
			pps->rtcp[trackID].udp_fd = rl_get_udp_sockfd(htons(rtcp_udp_port), 0, 0, NULL);
			if (pps->rtcp[trackID].udp_fd != -1) break;
			rtcp_udp_port += 2;
			if (rtcp_udp_port < 1024)
				rtcp_udp_port = 1027;
		}
		pps->rtcp[trackID].own_udp_port = rtcp_udp_port;
		pthread_mutex_unlock(&rtp_port_mutex);

		NM_set_socket_nonblock(pps->rtcp[trackID].udp_fd);

		register_NM_socket(pps->rtcp[trackID].udp_fd,
				   (void *)pps,
				   rtcp_ps_epollin,
				   rtcp_ps_epollout,
				   rtcp_ps_epollerr,
				   rtcp_ps_epollhup,
				   NULL,
				   NULL,
				   0,
				   USE_LICENSE_FALSE,
				   FALSE);
	
		NM_add_event_epollin(pps->rtcp[trackID].udp_fd);

	}
	else {
		pps->rtp[trackID].own_channel_id = trackID * 2; //g_channel_id++;
		pps->rtcp[trackID].own_channel_id = trackID * 2 + 1; //g_channel_id++;
	}

	// Build up the Transport
	if (pps->rtsp.streamingMode == RTP_TCP) {
		bytesused += snprintf(&outbuf[bytesused], outbufsz-bytesused,
				"Transport: RTP/AVP/TCP;unicast;interleaved=%d-%d\r\n",
				pps->rtp[trackID].own_channel_id,
				pps->rtcp[trackID].own_channel_id);
	}
	else {
		bytesused += snprintf(&outbuf[bytesused], outbufsz-bytesused,
				"Transport: RTP/AVP;unicast;client_port=%d-%d\r\n",
				pps->rtp[trackID].own_udp_port,
				pps->rtcp[trackID].own_udp_port);
	}

	/* For QT player and QTSS server combination pass the late tolernace 
	 * header from player to server
	 */
	if (pps && CHECK_PPS_FLAG(pps, PPSF_SERVER_QTSS) && 
			CHECK_PLAYER_FLAG(pps->pplayer_list, PLAYERF_PLAYER_QT) &&
			RTSP_IS_FLAG_SET(pps->pplayer_list->prtsp->flag, 
			RTSP_HDR_X_QT_LATE_TOLERANCE)) {
		bytesused += snprintf(&outbuf[bytesused], outbufsz - bytesused, 
				"%s: late-tolerance=%f\r\n",
				rtsp_known_headers[RTSP_HDR_X_QT_LATE_TOLERANCE].name,
				pps->pplayer_list->prtsp->qt_late_tolerance);
	}

	rl_ps_live_buildup_req_head(pps, outbuf, bytesused);
	rl_ps_sendout_request(pps);
	return NULL;
}

static char * rl_ps_live_send_method_play(rtsp_con_ps_t * pps)
{
	char start[16], end[16];
	int bytesused = 0;
	char outbuf[1024];
	int outbufsz = sizeof(outbuf);

	if (!pps) return NULL;

	pps->method = METHOD_PLAY;
	if (CHECK_PPS_FLAG(pps, PPSF_LIVE_VIDEO)) {
		bytesused += snprintf(&outbuf[bytesused], outbufsz-bytesused,
				"Range: npt=0.000-\r\n" ); 
	}
	else
	{
		if (RTSP_IS_FLAG_SET(pps->pplayer_list->prtsp->flag, RTSP_HDR_RANGE)) {
			start[0] = end[0] = 0;
			if (pps->pplayer_list->prtsp->rangeStart >= 0) {
				snprintf( start, 15, "%.3f", 
						pps->pplayer_list->prtsp->rangeStart ); 
			}
			if (pps->pplayer_list->prtsp->rangeEnd >= 0) {
				snprintf( end, 15, "%.3f", 
						pps->pplayer_list->prtsp->rangeEnd ); 
			}
			else {
				strncpy(end, pps->sdp_info.npt_stop, 15);
				end[15] = 0;
			}
			if (pps->pplayer_list->prtsp->rangeStart >= 0 || 
					pps->pplayer_list->prtsp->rangeEnd >= 0) {
				bytesused += snprintf(&outbuf[bytesused], outbufsz-bytesused,
						"Range: npt=%s-%s\r\n",
						start, end);
			}
		}

		if (RTSP_IS_FLAG_SET(pps->pplayer_list->prtsp->flag, RTSP_HDR_SCALE)) {
			bytesused += snprintf(&outbuf[bytesused], outbufsz-bytesused,
						"Scale: %.3f\r\n",
						pps->pplayer_list->prtsp->scale);
		}
	}
	rl_ps_live_buildup_req_head(pps, outbuf, bytesused);
	rl_ps_sendout_request(pps);
	return NULL;
}

static char *rl_ps_live_send_method_pause(rtsp_con_ps_t * pps)
{
	if (!pps) return NULL;
	pps->method = METHOD_PAUSE;
	rl_ps_live_buildup_req_head(pps, NULL, 0);
	rl_ps_sendout_request(pps);
	return NULL;
}

static char *rl_ps_live_send_method_get_parameter(rtsp_con_ps_t * pps)
{
	if (!pps) return NULL;
	pps->method = METHOD_GET_PARAMETER;
	rl_ps_live_buildup_req_head(pps, NULL, 0);
	rl_ps_sendout_request(pps);
	return NULL;
}

static char *rl_ps_live_send_method_set_parameter(rtsp_con_ps_t * pps)
{
	int len;
	int content_length;
	char outbuf[1024];
	int outbufsz = sizeof(outbuf);

	if (!pps) return NULL;
	pps->method = METHOD_SET_PARAM;

	/*
	 * Send out SET_PARAMETER Body.
	 */
	content_length = pps->pplayer_list->prtsp->cb_req_content_len;
	if (content_length) {
		len = snprintf(outbuf, outbufsz, "Content-Length: %d\r\n\r\n", content_length);
		rl_ps_live_buildup_req_head(pps, outbuf, len);
		rl_ps_sendout_request(pps);
		 
		memcpy( &pps->rtsp.cb_reqbuf[0],
			&pps->pplayer_list->prtsp->cb_reqbuf[pps->rtsp.rtsp_hdr_len],
			content_length);
		pps->rtsp.cb_reqlen = content_length;
		pps->rtsp.cb_reqoffsetlen = 0;
		rl_ps_sendout_request(pps);
	}
	else {
		rl_ps_live_buildup_req_head(pps, NULL, 0);
		rl_ps_sendout_request(pps);
	}
	return NULL;
}

static char *rl_ps_live_send_method_teardown(rtsp_con_ps_t * pps)
{
	if (!pps) return NULL;
	pps->method = METHOD_TEARDOWN;
	SET_PPS_FLAG(pps, PPSF_TEARDOWN);
	if (CHECK_PPS_FLAG(pps, PPSF_OWN_REQUEST)) {
		rl_ps_live_form_req_head(pps, "TEARDOWN", NULL, 0);
		UNSET_PPS_FLAG(pps, PPSF_OWN_REQUEST);
	}
	else {
		rl_ps_live_buildup_req_head(pps, NULL, 0);
	}
	rl_ps_sendout_request(pps);
	return NULL;
}

#endif

char * rl_ps_wms_recv_method_options(rtsp_con_ps_t * pps)
{
	const char *data;
	int datalen;
	u_int32_t attrs;
	int hdrcnt;
	int rv;

	if (!pps) return NULL;
	rv = get_known_header(&pps->rtsp.hdr, RTSP_HDR_PUBLIC, &data,
				&datalen, &attrs, &hdrcnt);
	if(rv || !datalen) { 
		if (pps->optionsString) 
			free(pps->optionsString);
		pps->optionsString = NULL;
	}
	else {
		pps->optionsString = nkn_realloc_type(pps->optionsString, 
				datalen + 10, 
				mod_rtsp_rl_rmd_malloc);
		strcpy(pps->optionsString, "Public: ");
		strncat(pps->optionsString, data, datalen);
	}
	return NULL;
}

char * rl_ps_wms_recv_method_describe(rtsp_con_ps_t * pps)
{
	char * psdpstart, * p;

	if (!pps) return NULL;

	if (RTSP_IS_FLAG_SET(pps->rtsp.flag, RTSP_HDR_SERVER)) {
		unsigned int hdr_attr = 0;
		int hdr_cnt = 0;
		const char *hdr_str = NULL;
		int hdr_len = 0;

		if (0 == mime_hdr_get_known(&pps->rtsp.hdr, RTSP_HDR_SERVER, 
				&hdr_str, &hdr_len, &hdr_attr, &hdr_cnt)) {
			if (strncasecmp(hdr_str, "WMServer", strlen("WMServer")) == 0) {
				SET_PPS_FLAG(pps, PPSF_SERVER_WMS);
			}
			else if (strncasecmp(hdr_str, "QTSS", strlen("QTSS")) == 0) {
				SET_PPS_FLAG(pps, PPSF_SERVER_QTSS);
			}
		}
	}

	/* 
	 * copy sdp string and save
	 */
	p = pps->rtsp.cb_respbuf + pps->rtsp.cb_respoffsetlen;
	psdpstart = strstr(p, "v=");
	if(!psdpstart) return NULL;
	pps->sdp_size = pps->rtsp.cb_resplen;
	pps->sdp_string = (char *)nkn_malloc_type(pps->sdp_size+1, mod_rtsp_rl_rmd_malloc);
	if(!pps->sdp_string) return NULL;
	memcpy(pps->sdp_string, psdpstart, pps->sdp_size);
	pps->sdp_string[pps->sdp_size] = 0;

	p = psdpstart;
	rl_ps_parse_sdp_info(&p, pps);
	pps->rtsp.cb_resplen -= p - psdpstart;
	if (pps->rtsp.cb_resplen == 0) {
		pps->rtsp.cb_respoffsetlen = 0;
	}
	else {
		pps->rtsp.cb_respoffsetlen += p - psdpstart;
	}
	
	if (pps->pplayer_list ) {
		if (pps->pplayer_list->player_type == RL_PLAYER_TYPE_FAKE_PLAYER ) {
			orp_write_sdp_to_tfm(pps->pplayer_list);
		}
	}

	return NULL;
}

char * rl_ps_wms_recv_method_setup(rtsp_con_ps_t * pps)
{
	int trackID;
	//int sdp_track_num;

	if (!pps) return NULL;
	SET_PPS_FLAG(pps, PPSF_SETUP_DONE);

	RTSP_GET_ALLOC_URL_COMPONENT(&pps->pplayer_list->prtsp->hdr, 
			pps->pplayer_list->prtsp->urlPreSuffix, 
			pps->pplayer_list->prtsp->urlSuffix);
	//trackID = pps->cur_trackID;
	trackID = rl_get_track_num(pps, pps->pplayer_list->prtsp->urlSuffix);

	if (pps->rtsp.streamingMode == RTP_TCP) {
		//Obtain the channel id for retreiving rtp data
		pps->rtp[trackID].peer_channel_id = pps->rtsp.rtpChannelId;
		SET_PPS_FLAG(pps, PPSF_RECV_INLINE_DATA);
	}
	else {
		pps->rtp[trackID].peer_udp_port = pps->rtsp.serverRTPPortNum;
	}
	pps->rtp[trackID].trackID = nkn_strdup_type(pps->sdp_info.track[trackID].trackID, 
							mod_rtsp_rl_setup_strdup);

	if (pps->rtsp.streamingMode == RTP_TCP) {
		rl_set_channel_id(pps, pps->rtp[trackID].trackID, pps->rtp[trackID].peer_channel_id);
	}

	if (pps->rtsp.streamingMode == RTP_TCP) {
		//Obtaining the channel id for retreiving rtcp data
		pps->rtcp[trackID].peer_channel_id = pps->rtsp.rtcpChannelId;
	}
	else {
		pps->rtcp[trackID].peer_udp_port = pps->rtsp.serverRTCPPortNum;
	}

	
	pps->rtp[trackID].sample_rate = rl_get_sample_rate(pps, pps->rtp[trackID].trackID);
	if(pps->rtsp.streamingModeString) {
		if(pps->streamingModeString) free(pps->streamingModeString);
		pps->streamingModeString = nkn_strdup_type(pps->rtsp.streamingModeString, 
								mod_rtsp_rl_setup_strdup);
	}
	if(pps->rtsp.ssrc) {
		if(pps->ssrc) free(pps->ssrc);
		pps->ssrc = nkn_strdup_type(pps->rtsp.ssrc, mod_rtsp_rl_setup_strdup);
	}
	if(pps->rtsp.mode) {
		if(pps->mode) free(pps->mode);
		pps->mode = nkn_strdup_type(pps->rtsp.mode, mod_rtsp_rl_setup_strdup);
	}
	if(pps->rtsp.session) {
		if(pps->session) free(pps->session);
		pps->session = nkn_strdup_type(pps->rtsp.session, mod_rtsp_rl_setup_strdup);
	}

	pps->cur_trackID ++;
	return NULL;

}

char * rl_ps_wms_recv_method_play(rtsp_con_ps_t * pps)
{
	int i;

	if (!pps) return NULL;
	/* video streaming is playing now */
	SET_PPS_FLAG(pps, PPSF_VIDEO_PLAYING);
	for (i = 0; i < pps->sdp_info.num_trackID; i++) {
		pps->rtp_info_seq[i] = pps->rtsp.rtp_info_seq[i];
	}

	if (!pps->fkeep_alive_task) {
		pps->fkeep_alive_task = nkn_scheduleDelayedTask( 60 * 1000000, 
			     (TaskFunc*)rtsplive_keepaliveTask, (void *)pps);
	}
	return NULL;
}

char * rl_ps_wms_recv_method_pause(rtsp_con_ps_t * pps)
{
	UNUSED_ARGUMENT(pps);
	//rl_player_ret_teardown(pps->pplayer_list);
	//rl_ps_closed(pps);
	return NULL;
}

char * rl_ps_wms_recv_method_get_parameter(rtsp_con_ps_t * pps)
{
	UNUSED_ARGUMENT(pps);
	//rl_player_ret_teardown(pps->pplayer_list);
	//rl_ps_closed(pps);
	return NULL;
}

char * rl_ps_wms_recv_method_set_parameter(rtsp_con_ps_t * pps)
{
	UNUSED_ARGUMENT(pps);
	//rl_player_ret_teardown(pps->pplayer_list);
	//rl_ps_closed(pps);
	return NULL;
}

char * rl_ps_wms_recv_method_teardown(rtsp_con_ps_t * pps)
{
	UNUSED_ARGUMENT(pps);
	//rl_player_ret_teardown(pps->pplayer_list);
	//rl_ps_closed(pps);
	return NULL;
}



#if 0
static char * rl_ps_live_recv_method_options(rtsp_con_ps_t * pps)
{
	const char *data;
	int datalen;
	u_int32_t attrs;
	int hdrcnt;
	int rv;

	if (!pps) return NULL;
	rv = get_known_header(&pps->rtsp.hdr, RTSP_HDR_PUBLIC, &data,
				&datalen, &attrs, &hdrcnt);
        if(rv || !datalen) { 
        	if (pps->optionsString) 
        		free(pps->optionsString);
        	pps->optionsString = NULL;
        }
        else {
        	pps->optionsString = nkn_realloc_type(pps->optionsString, 
        			datalen + 10, 
        			mod_rtsp_rl_rmd_malloc);
		strcpy(pps->optionsString, "Public: ");
		strncat(pps->optionsString, data, datalen);
        }
	return NULL;
}

static char * rl_ps_live_recv_method_describe(rtsp_con_ps_t * pps)
{
	char * psdpstart, * p;

	if (!pps) return NULL;

	if (RTSP_IS_FLAG_SET(pps->rtsp.flag, RTSP_HDR_SERVER)) {
		unsigned int hdr_attr = 0;
		int hdr_cnt = 0;
		const char *hdr_str = NULL;
		int hdr_len = 0;

		if (0 == mime_hdr_get_known(&pps->rtsp.hdr, RTSP_HDR_SERVER, 
				&hdr_str, &hdr_len, &hdr_attr, &hdr_cnt)) {
			if (strncasecmp(hdr_str, "QTSS", strlen("QTSS")) == 0) {
				SET_PPS_FLAG(pps, PPSF_SERVER_QTSS);
			}
			else if (strncasecmp(hdr_str, "WMServer", strlen("WMServer")) == 0) {
				SET_PPS_FLAG(pps, PPSF_SERVER_WMS);
			}
		}
	}
	
	/* 
	 * copy sdp string and save
	 */
	p = pps->rtsp.cb_respbuf + pps->rtsp.cb_respoffsetlen;
	psdpstart = strstr(p, "v=");
	if(!psdpstart) return NULL;
	pps->sdp_size = pps->rtsp.cb_resplen;
	pps->sdp_string = (char *)nkn_malloc_type(pps->sdp_size+1, mod_rtsp_rl_rmd_malloc);
	if(!pps->sdp_string) return NULL;
	memcpy(pps->sdp_string, psdpstart, pps->sdp_size);
	pps->sdp_string[pps->sdp_size] = 0;

	p = psdpstart;
	rl_ps_parse_sdp_info(&p, pps);
	pps->rtsp.cb_resplen -= p - psdpstart;
	if (pps->rtsp.cb_resplen == 0) {
		pps->rtsp.cb_respoffsetlen = 0;
	}
	else {
		pps->rtsp.cb_respoffsetlen += p - psdpstart;
	}
	
	if (pps->pplayer_list ) {
		if (pps->pplayer_list->player_type == RL_PLAYER_TYPE_FAKE_PLAYER ) {
			orp_write_sdp_to_tfm(pps->pplayer_list);
			mime_hdr_copy_headers(&pps->pplayer_list->hdr, &pps->rtsp.hdr);
		}
	}

	return NULL;
}

static char * rl_ps_live_recv_method_setup(rtsp_con_ps_t * pps)
{
	int trackID;

	if (!pps) return NULL;
	SET_PPS_FLAG(pps, PPSF_SETUP_DONE);
	trackID = pps->cur_trackID;

	if (pps->rtsp.streamingMode == RTP_TCP) {
		//Obtain the channel id for retreiving rtp data
		pps->rtp[trackID].peer_channel_id = pps->rtsp.rtpChannelId;
		SET_PPS_FLAG(pps, PPSF_RECV_INLINE_DATA);
	}
	else {
		pps->rtp[trackID].peer_udp_port = pps->rtsp.serverRTPPortNum;
	}
	pps->rtp[trackID].trackID = nkn_strdup_type(pps->sdp_info.track[trackID].trackID, 
							mod_rtsp_rl_setup_strdup);

	if (pps->rtsp.streamingMode == RTP_TCP) {
		rl_set_channel_id(pps, pps->rtp[trackID].trackID, pps->rtp[trackID].peer_channel_id);
	}

	if (pps->rtsp.streamingMode == RTP_TCP) {
		//Obtaining the channel id for retreiving rtcp data
		pps->rtcp[trackID].peer_channel_id = pps->rtsp.rtcpChannelId;
	}
	else {
		pps->rtcp[trackID].peer_udp_port = pps->rtsp.serverRTCPPortNum;
	}

	
	pps->rtp[trackID].sample_rate = rl_get_sample_rate(pps, pps->rtp[trackID].trackID);
	if(pps->rtsp.streamingModeString) {
		if(pps->streamingModeString) free(pps->streamingModeString);
		pps->streamingModeString = nkn_strdup_type(pps->rtsp.streamingModeString, 
								mod_rtsp_rl_setup_strdup);
	}
	if(pps->rtsp.ssrc) {
		if(pps->ssrc) free(pps->ssrc);
		pps->ssrc = nkn_strdup_type(pps->rtsp.ssrc, mod_rtsp_rl_setup_strdup);
	}
	if(pps->rtsp.mode) {
		if(pps->mode) free(pps->mode);
		pps->mode = nkn_strdup_type(pps->rtsp.mode, mod_rtsp_rl_setup_strdup);
	}
	if(pps->rtsp.session) {
		if(pps->session) free(pps->session);
		pps->session = nkn_strdup_type(pps->rtsp.session, mod_rtsp_rl_setup_strdup);
	}

	pps->cur_trackID ++;
	return NULL;

}

static char * rl_ps_live_recv_method_play(rtsp_con_ps_t * pps)
{
	int i;

	if (!pps) return NULL;
	/* video streaming is playing now */
	SET_PPS_FLAG(pps, PPSF_VIDEO_PLAYING);
	for (i = 0; i < pps->sdp_info.num_trackID; i++) {
		pps->rtp_info_seq[i] = pps->rtsp.rtp_info_seq[i];
	}

	if (!pps->fkeep_alive_task) {
		pps->fkeep_alive_task = nkn_scheduleDelayedTask( 60 * 1000000, 
			     (TaskFunc*)rtsplive_keepaliveTask, (void *)pps);
	}
	return NULL;
}


static char * rl_ps_live_recv_method_pause(rtsp_con_ps_t * pps)
{
	UNUSED_ARGUMENT(pps);
	//rl_player_ret_teardown(pps->pplayer_list);
	//rl_ps_closed(pps);
	return NULL;
}

static char * rl_ps_live_recv_method_get_parameter(rtsp_con_ps_t * pps)
{
	UNUSED_ARGUMENT(pps);
	//rl_player_ret_teardown(pps->pplayer_list);
	//rl_ps_closed(pps);
	return NULL;
}

static char * rl_ps_live_recv_method_set_parameter(rtsp_con_ps_t * pps)
{
	UNUSED_ARGUMENT(pps);
	//rl_player_ret_teardown(pps->pplayer_list);
	//rl_ps_closed(pps);
	return NULL;
}

static char * rl_ps_live_recv_method_teardown(rtsp_con_ps_t * pps)
{
	UNUSED_ARGUMENT(pps);
	//rl_player_ret_teardown(pps->pplayer_list);
	//rl_ps_closed(pps);
	return NULL;
}
#endif

