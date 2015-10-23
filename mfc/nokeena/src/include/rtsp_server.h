#ifndef _RTSP_SERVER_HH
#define _RTSP_SERVER_HH

#include <sys/queue.h>
#include "interface.h"
#include "rtsp_session.h"
#include "nkn_namespace.h"
#include "nkn_sched_api.h"
#include "nkn_mediamgr_api.h"
#include "rtsp_def.h"

#define MAX_SESSION 10
#define TRACKID_LEN 32
struct rtsp_con_publish_svr;
struct rtsp_cb;

struct rtsp_so_libs {
	struct rtsp_so_libs * next;
	int    enable;
	char * ext;
	char * libname;
	void * handle;
	pthread_mutex_t mutex;
	struct rtsp_so_exp_funcs_t * pfunc;
};

#if 0
typedef enum rtsp_om_response_type {
	RESPONSE_NOT_CACHEABLE,
	RESPONSE_CACHEABLE_VIA_TFM,
	DATA_NOT_CACHEABLE,
	DATA_CACHEABLE_VIA_TFM
}rtsp_om_response_type_t;
#endif // 0

typedef enum rtsp_om_cache_control {
	RTSP_CACHE_CONTROL_NO_CACHE,
	RTSP_CACHE_CONTROL_NO_TRANSFORM,
	RTSP_CACHE_CONTROL_PUBLIC,
	RTSP_CACHE_CONTROL_PRIVATE,
	RTSP_CACHE_CONTROL_MUST_REVALIDATE,
	RTSP_CACHE_CONTROL_MAX_AGE
}rtsp_om_cache_control_t;

#define RTSP_PARAM_STRING_MAX 100

// A data structure used for optional user/password authentication:

/*
 * connection control block
 */

#define RCONF_FIRST_TASK	0x0000000000000001
#define RCONF_CANCELD		0x0000000000000004
#define RCONF_TASK_POSTED	0x0000000000000008
#define RCONF_RECV_DATA		0x0000000000000010
#define RCONF_SEND_DATA		0x0000000000000020
#define RTSP_RTP_DELIMITER 	0x24

#define RTSP_REQ_BUFFER_SIZE	(4*1024)
#define RTSP_BUFFER_SIZE	(16*1024)
#define RTP_BUFFER_SIZE		(2*1024)
//#define RTSP_PARAM_STRING_MAX	64

typedef enum {
	RPS_OK = 0,
	RPS_ERROR,
	RPS_NEED_MORE_DATA,
	RPS_NEED_MORE_SPACE,
	RPS_INLINE_DATA,
	RPS_RTSP_RESP_HEADER,
	RPS_RTSP_REQ_HEADER,
	RPS_NOT_SUPPORTED,
	RPS_NEED_CONTENT,
}RTSP_PARSE_STATUS ;

typedef  RTSP_PARSE_STATUS rtsp_parse_status_t;

typedef enum {
	METHOD_BAD = 0,
	METHOD_OPTIONS = 1,
	METHOD_DESCRIBE = 2,
	METHOD_SETUP = 3,
	METHOD_TEARDOWN = 4,
	METHOD_PLAY = 5,
	METHOD_PAUSE = 6,
	METHOD_GET_PARAMETER = 7,
	METHOD_ANNOUNCE = 8,
	METHOD_RECORD = 9,
	METHOD_REDIRECT = 10,
	METHOD_SET_PARAM = 11,
	METHOD_BROADCAST_PAUSE = 12,
	//METHOD_SENT_DESCRIBE_RESP = 13,
	//METHOD_SENT_SETUP_RESP = 14,
	//METHOD_SENT_PLAY_RESP = 15,
	//METHOD_SENT_TEARDOWN_RESP = 16,
	//METHOD_RECV_RTP_DATA,
	METHOD_UNKNOWN = 20,
} RTSP_METHOD;

typedef enum StreamingMode {
	RTP_UDP,
	RTP_TCP,
	RAW_UDP,
	RAW_TCP
} StreamingMode;

typedef enum {
	RSTATE_INIT = 0,
	RSTATE_RCV_OPTIONS = 1,
	RSTATE_RCV_DESCRIBE = 2,
	RSTATE_RCV_SETUP = 3,
	RSTATE_RCV_TEARDOWN = 4,
	RSTATE_RCV_PLAY = 5,
	RSTATE_RCV_PAUSE = 6,
	RSTATE_RCV_GET_PARAMETER = 7,
	RSTATE_ASYNC_HANDLER = 8,
	RSTATE_RELAY
} RTSP_REQ_STATE;


struct streamState {
	struct rtsp_cb * prtsp;
	void * subsession;
	void * streamToken;

	/* RTCP handler */
	int rtcp_udp_fd;
	uint16_t rtcpPortNum;
	int isTimeout;
	int startRtcp;
};

/* The below macro suffices all headers
 * separate definitins provided for backward compatibility */
#define RTSP_FLAG_SET_HDR(flag, hdr_id) ((flag) = (flag)|(0x01L<<(hdr_id)))
#define RTSP_IS_FLAG_SET(flag, hdr_id) ((flag)&(0x01L<<(hdr_id)))

#define HAS_AUTHENTICATE_HEAD	(0x01L<<RTSP_HDR_AUTHORIZATION)
#define HAS_CSEQ_HEAD		(0x01L<<RTSP_HDR_CSEQ)
#define HAS_RANGE_HEAD		(0x01L<<RTSP_HDR_RANGE)
#define HAS_SCALE_HEAD		(0x01L<<RTSP_HDR_SCALE)
#define HAS_RTPINFO_HEAD	(0x01L<<RTSP_HDR_RTP_INFO)
#define HAS_TRANSPORT_HEAD	(0x01L<<RTSP_HDR_TRANSPORT)
#define HAS_PLAYNOW_HEAD	(0x01L<<RTSP_HDR_X_PLAY_NOW)

#define RTSP_MAGIC_FREE	0xfeee0034
#define RTSP_MAGIC_DEAD	0xdead0033
#define RTSP_MAGIC_LIVE	0x00000032

typedef struct rtp_d_t{
	uint32_t	channel_id;
	int		rtp_buf_len;
	char *		rtp_buf;
	int		required_len;
	int		data_len;
	char 		data_buf[RTSP_BUFFER_SIZE + 4];
}rtp_data_t;

// RTSP interleave related info
typedef enum { SIGNALLING, INTERLEAVE_HDR, DATA } RTSP_INTERLEAVE_STATE ;

typedef struct rtsp_interleave {
        uint16_t to_read;
        RTSP_INTERLEAVE_STATE interleave_state;
} rtsp_interleave_t;


//struct ServerMediaSession_t;
// RTSP client session
typedef struct rtsp_cb {
	uint32_t magic;
	uint64_t flag;
	RTSP_REQ_STATE state;

	// For link list
	LIST_ENTRY(rtsp_cb) entries;

	/* media codecs control block */
	struct rtsp_so_libs * so;
	int is_ankeena_fmt;
	int mediacodec_error_code;

	/* server information */
	struct nkn_interface * pns;

	/* RTSP transaction information */
	struct timeval start_ts;	// RTSP begin timestamp
	struct timeval end_ts;		// RTSP end timestamp
	uint64_t in_bytes;	// bytes delivered to client
	uint64_t out_bytes;	// bytes delivered to client

	/* network socket information */
	int fd;
	uint32_t src_ip;
	uint32_t dst_ip;
	//struct sockaddr_in fClientAddr;
	char cb_reqbuf[RTSP_REQ_BUFFER_SIZE + 4];
	int  cb_reqlen;
	int  cb_reqoffsetlen;
	int  cb_req_content_len;
	char cb_respbuf[RTSP_BUFFER_SIZE + 4];
	int  cb_resplen;
	int  cb_respoffsetlen;
	int  cb_resp_content_len;
	int  rtsp_hdr_len;

	/* store namespace info */
	namespace_token_t nstoken;
	const namespace_config_t *nsconf;

	/* RTSP return information */
	int status;

	/* RTSP Status info 
	 * enum and sub error code */
	rtsp_status_t status_enum;
	int sub_error_code;
	int isPlaying;

	TaskToken fLivenessCheckTask;
	TaskToken fAsyncTaskToken;
	uint32_t fAsyncTask;
	unsigned char fResponseBuffer[RTSP_BUFFER_SIZE + 4];
	Boolean fIsMulticast, fSessionIsActive, fStreamAfterSETUP;
	unsigned char fTCPStreamIdCount; // used for (optional) RTP/TCP

	/* mime header */
	mime_header_t hdr;

	/* Session Management */
	struct SMS_queue sms_queue_head;
	//struct ServerMediaSession_t * fOurServerMediaSession;
	struct rtsp_session_t * fOurrtsp_session;
	unsigned long long fOurSessionId;
	int need_session_id;
	unsigned int fNumStreamStates;
	struct streamState  * fStreamStates;
	struct rtsp_con_publish_svr *pps;
	void * subsession;

	/* parser result */
	RTSP_METHOD method;
	char cmdName[RTSP_PARAM_STRING_MAX];
	char cseq[RTSP_PARAM_STRING_MAX];
	/* Pointers allocated in process request
	 * using alloca. cannot use these pointers
	 * outside rtsp_process_response function
	 * char *urlPreSuffix;
	 * char *urlSuffix;
	 */
	char *urlPreSuffix;
	char *urlSuffix;
	float qt_late_tolerance;
	//char rtsp_url[RTSP_PARAM_STRING_MAX];

	double rangeStart, rangeEnd;
	float  scale;

	StreamingMode streamingMode;
	char streamingModeString[RTSP_PARAM_STRING_MAX];
	char destinationAddressStr[RTSP_PARAM_STRING_MAX];
	u_int8_t destinationTTL;
	uint16_t clientRTPPortNum; // if UDP
	uint16_t clientRTCPPortNum; // if UDP
	uint16_t serverRTPPortNum; // if UDP
	uint16_t serverRTCPPortNum; // if UDP
	char session[32];
	char ssrc[16];
	char mode[32];
	char trackID[MAX_SESSION][TRACKID_LEN];
	uint32_t nMediaTracks;
	unsigned char rtpChannelId; // if TCP
	unsigned char rtcpChannelId; // if TCP
	uint64_t rtp_info_seq[MAX_SESSION];  // Up to two trackID
	uint64_t rtp_time[MAX_SESSION];
	uint32_t reclaimSuccess;

	rtsp_om_cache_control_t cacheControl;
	time_t expires;
	time_t curr_date;
	uint32_t cacheMaxAge;
	/* only for origin connections
	 */
	//rtsp_om_response_type_t resp_type;
	nkn_task_id_t nkn_tsk_id;

	// Describe task id
	nkn_task_id_t describe_nkn_task_id;

	char *username;
	char *realm;
	char *nonce;
	char *uri;
	char *response;
	char *content_base;
	int partialRTPDataRcvd;
	rtp_data_t rtp_data;
	pthread_mutex_t mutex;

        // to  maintain state about RTSP signalling/data interleaving
        rtsp_interleave_t rtsp_intl;
} rtsp_cb_t;

#define SET_RCON_FLAG(x, f) (x)->flag |= (f)
#define UNSET_RCON_FLAG(x, f) (x)->flag &= ~(f)
#define CHECK_RCON_FLAG(x, f) ((x)->flag & (f))

#define RTSP_SET_ERROR(rtsp_ctxt, status_id, sub_error) \
{\
    rtsp_ctxt->status_enum = status_id;\
    rtsp_ctxt->sub_error_code = sub_error;\
}

LIST_HEAD(rtsp_cb_queue, rtsp_cb);
extern struct rtsp_cb_queue g_rtsp_cb_queue_head;


// The state of each individual session handled by a RTSP server:
rtsp_cb_t * nkn_new_prtsp(unsigned long long sessionId,
		int clientSocket, struct sockaddr_in clientAddr,
		struct nkn_interface * pns);
void nkn_free_prtsp(rtsp_cb_t * prtsp);

Boolean parseRTSPRequestString(rtsp_cb_t * prtsp, char * p, int len);
Boolean parseCSeqHeader(rtsp_cb_t * prtsp, char * p, int len);
Boolean parseRangeHeader(rtsp_cb_t * prtsp, char * p, int len);
Boolean parseScaleHeader(rtsp_cb_t * prtsp, char * p, int len);
Boolean parseSessionHeader(rtsp_cb_t * prtsp, char * p, int len);
Boolean parseSessionHeaderWithoutValidation(rtsp_cb_t * prtsp, char * p, int len);
Boolean parseRTPInfoHeader(rtsp_cb_t * prtsp, char * p, int len);
Boolean parseTransportHeader(rtsp_cb_t * prtsp, char * p, int len);
Boolean parsePlayNowHeader(rtsp_cb_t * prtsp, char * p, int len);
Boolean parseAuthorizationHeader(rtsp_cb_t * prtsp, char * p, int len);
Boolean ParseCmdName(rtsp_cb_t * prtsp, char * p, int len);
Boolean parseRTSP_URL(rtsp_cb_t * prtsp, char * p, int len);

void rtsp_init_buf(rtsp_cb_t * prtsp);
rtsp_parse_status_t rtsp_parse_request(rtsp_cb_t * prtsp);
rtsp_parse_status_t rtsp_parse_origin_request(rtsp_cb_t * prtsp);
rtsp_parse_status_t rtsp_parse_response(rtsp_cb_t * prtsp);
rtsp_parse_status_t rtsp_parse_rtp_data(rtsp_cb_t *prtsp);

void handleCmd_bad(rtsp_cb_t * prtsp);
void handleCmd_notSupported(rtsp_cb_t * prtsp);
void handleCmd_notFound(rtsp_cb_t * prtsp);
void handleCmd_unsupportedTransport(rtsp_cb_t * prtsp);
void handleCmd_OPTIONS_KA(rtsp_cb_t * prtsp, Boolean sessionId);
int handleCmd_DESCRIBE(rtsp_cb_t * prtsp);
int handleCmd_SETUP(rtsp_cb_t * prtsp);
void handleCmd_withinSession(rtsp_cb_t * prtsp, RTSP_METHOD method);
void handleCmd_TEARDOWN(rtsp_cb_t * prtsp);
void handleCmd_PLAY(rtsp_cb_t * prtsp);
void handleCmd_PAUSE(rtsp_cb_t * prtsp);
void handleCmd_GET_PARAMETER(rtsp_cb_t * prtsp);
	
void rtsp_set_timeout(rtsp_cb_t * prtsp);
Boolean isMulticast(rtsp_cb_t * prtsp);
void noteClientLiveness(rtsp_cb_t * prtsp);
void livenessTimeoutTask(rtsp_cb_t * prtsp);

/*
 * for RTSP OM, defined in om_rtsp_api.c
 */
int RTSP_OM_get_ret_error_to_task(MM_get_resp_t *p_mm_task, rtsp_cb_t *prtsp);
int RTSP_OM_get_ret_task(MM_get_resp_t *p_mm_task, char * buf, int len, uint64_t *tot_len);

#endif // _RTSP_SERVER_HH
