#ifndef _RTSP_SERVER_LIVE__HH
#define _RTSP_SERVER_LIVE__HH

/*
 * RTCP macros
 */
#define RTCP_SENDER_SIZE 28
#define RTCP_RR_SIZE 32
#define RTCP_VERSION 2
#define RTCP_RR_PT 201
#define RTCP_SR_PT 200
#define RTCP_SDES_PT 202
#define RTCP_BYE_PT 203
#define RTCP_MFD_SSRC 0x01020304
#define RTCP_SDES_SIZE 20
#define RTCP_CNAME_SIZE 12
#define RTCP_MFD_CNAME 0x4D464420
#define RTCP_BYE_SIZE 8



#define MAX_QUEUED_PKTS 64

#define RL_BROADCAST_STATE_RTP 1
#define RL_BROADCAST_STATE_RTCP 2
#define RL_BROADCAST_STATE_RTSP 3
#define RL_BROADCAST_STATE_PAUSE 4
#define SET_STATE_FLAG(x, flag) x->ps_state |= (1 << (flag -1))
#define RESET_STATE_FLAG(x, flag) x->ps_state &= ~(1 << (flag-1))
#define ISSET_STATE_FLAG(x, flag) (x->ps_state & (1 << (flag-1)))


#define TRACK_TYPE_AUDIO     1
#define TRACK_TYPE_VIDEO     2
#define TRACK_TYPE_UNKNOWN   3

#define RTP_MTU_SIZE 2048

#define RL_DATA_NONE	0
#define RL_DATA_RTP	1
#define RL_DATA_RTCP	2
#define RL_DATA_INLINE	3

#define LK_PS_GNM	0x0001
#define LK_PS		0x0002
#define LK_PL_GNM	0x0010
#define LK_PL		0x0020

#define SET_LOCK(x, f) (x) |= (f)
#define UNSET_LOCK(x, f) (x) &= ~(f)
#define CHECK_LOCK(x, f) ((x) & (f))

#include <sys/queue.h>
#include "rtsp_server.h"
#include "nkn_defs.h"

typedef enum rl_bw_query_result {
	BWS_DROP_PACKET = 0,
	BWS_SEND_PACKET,
	BWS_QUEUE_PACKET
} rl_bw_query_result_t;

typedef struct rl_bw_smooth_ctx {
} rl_bw_smooth_ctx_t;


#if 0
typedef enum rl_ps_state {
	RL_PS_STATE_INIT_DESCRIBE_RECV,
	RL_PS_STATE_INIT,
	RL_PS_STATE_READY,
	RL_PS_STATE_PLAYING
} rl_ps_state_t;
#endif // 0

typedef enum rl_player_type {
	RL_PLAYER_TYPE_UNKOWN,
	RL_PLAYER_TYPE_LIVE,
	RL_PLAYER_TYPE_VOD,
	RL_PLAYER_TYPE_RELAY,
	RL_PLAYER_TYPE_FAKE_PLAYER,
} rl_player_type_t;


typedef struct rtp_stt{
    int  num_pkts;
    int  seq_num;
    int  ts;
    int  time_scale;
    int start_seq_num;
    uint32_t jitter;
    int exp_seq_num;
    int num_pkts_lost;
    struct timeval start_time;
    int transit;
    int rtcp_pkts;
}rtcp_rtp_state_t;

typedef struct rtsp_live_stream_track_t {
	int32_t		udp_fd;		// socket Id

	uint16_t	own_udp_port;	// MFD UDP port, host order
	uint16_t	peer_udp_port;	// peer UDP port, host order
	uint16_t	peer_rtsp_port;	// peer RTSP port, host order

	uint8_t         first_packet_flag; //set when the first RTP/RTCP packet is seen
	uint8_t		setup_done_flag;
	uint32_t        sample_rate; //sampling rate for the RTP track/ '0' for RTCP track
	char		*trackID; //the tracks are indexe with a track id
	//uint32_t	sdp_track_num; //track index in sdp_info
	
	nkn_iovec_t     resend_buf[MAX_QUEUED_PKTS];
	uint32_t        num_resend_pkts;
	int32_t		cb_bufLen;
	uint32_t	own_channel_id;
	uint32_t	peer_channel_id;
	rtcp_rtp_state_t rtp;

} rls_track_t;


//#define MAX_RL_CB_BUFF	(4*1024)
//#define MAX_TRACK	2 	// only Audio/Video by now

#define RLMAGIC_ALIVE   0x11114520
#define RLMAGIC_DEAD    0xdead4520

/*
 * This structure is facing to video player.
 * One video player can only have one linked publish server.
 */
struct rtsp_con_publish_svr;

#include "rtsp_vpe_nkn_fmt.h" 


#define PLAYERF_CONNECTED	0x0000000000000001
#define PLAYERF_KEEP_ALIVE	0x0000000000000002
#define PLAYERF_FOUND_LIVE	0x0000000000000004
#define PLAYERF_SETUP_DONE	0x0000000000000008
#define PLAYERF_TEARDOWN	0x0000000000000010
#define PLAYERF_REQUEST_PENDING 0x0000000000000020
#define PLAYERF_CLOSE		0x0000000000000040
#define PLAYERF_TPROXY_MODE	0x0000000000000080
#define PLAYERF_SEND_BUFFER	0x0000000000000100
#define PLAYERF_LIVE_CACHEABLE	0x0000000000000200
#define PLAYERF_LIVE_ENABLED	0x0000000000000400
#define PLAYERF_INTERNAL	0x0000000000000800
#define PLAYERF_SRVR_TIMEOUT	0x0000000000001000
#define PLAYERF_CLOSE_TASK	0x0000000000002000

/* Flags for mutex locks
 */
#define PLAYERF_PS_GNM_LOCK	0x0001000000000000
#define PLAYERF_PS_LOCK		0x0002000000000000
#define PLAYERF_PL_GNM_LOCK	0x0010000000000000
#define PLAYERF_PL_LOCK		0x0020000000000000

/* Flags based on user agent, for any player specfic actions
 */
#define PLAYERF_PLAYER_QT	0x0100000000000000
#define PLAYERF_PLAYER_WMP	0x0200000000000000

typedef enum rl_fpl_state {
	RL_FPL_STATE_INIT,
	RL_FPL_STATE_SEND_OPTIONS,
	RL_FPL_STATE_WAIT_OPTIONS,
	RL_FPL_STATE_SEND_DESCRIBE,
	RL_FPL_STATE_WAIT_DESCRIBE,
	RL_FPL_STATE_SEND_SETUP,
	RL_FPL_STATE_WAIT_SETUP,
	RL_FPL_STATE_SEND_PLAY,
	RL_FPL_STATE_WAIT_PLAY,
	RL_FPL_STATE_START_PLAY,
	RL_FPL_STATE_PLAYING,
	RL_FPL_STATE_SEND_TEARDOWN,
	RL_FPL_STATE_WAIT_TEARDOWN,
	RL_FPL_STATE_ERROR,
	RL_FPL_STATE_ERROR_CLOSE
} rl_fpl_state_t;

typedef struct rtsp_con_player {
	uint32_t        magic;
	uint64_t	flag;

	int		cur_trackId;	// for RTSP player
	char *		uri;		// for RTSP player
	rl_fpl_state_t  fpl_state;
	rl_fpl_state_t  fpl_last_state;
	//FILE * 		tfm_fd;
	//char 		tfm_filename[256];
	TaskToken	fteardown_task;
	pthread_mutex_t buf_mutex;
	char 		*out_buffer;
	int		ob_bufsize;
	int		ob_datasz;
	int		ob_rdoff;
	int		ob_wroff;
	uint64_t	ob_tot_len;
	uint64_t	ob_tot_written;
	void 		*pseekbox;
	int		ob_len;
	MM_get_resp_t * p_mm_task;	// XXX:
	nkn_uol_t 	uol;		// XXX:
	struct timeval	start_time;
	uint32_t	elapsed_time;
	int		req_cseq;
	int		cache_age;

	/* mime header */
	mime_header_t hdr;

	int		pause;
	//int		g_players_index;
	uint64_t	ssrc;
	unsigned long long session;	// our session id
	struct rtsp_con_player * next;
	rl_bw_smooth_ctx_t *rl_player_bws_ctx;
	int32_t		max_allowed_bw;
	int32_t         rtsp_fd;
	rls_track_t	rtp[MAX_TRACK];
	rls_track_t	rtcp[MAX_TRACK];
	uint32_t	peer_ip_addr; 	// peer IP address, network order
	uint32_t	own_ip_addr; 	// MFD IP address, network order
	time_t		rtp_last_ts; //Contains the last time stamp for the associated rtp session
	struct rtsp_cb	*prtsp;
	rl_player_type_t player_type;
	struct rtsp_con_publish_svr * pps; // link to publish server
	TaskToken	fkeep_alive_task;
	TaskToken	close_task;
	int (*send_data)(struct rtsp_con_player *pplayer, char *buf, int len, int type, int track);
	int (*ret_options)(struct rtsp_con_player *pplayer);
	int (*ret_describe)(struct rtsp_con_player *pplayer);
	int (*ret_setup)(struct rtsp_con_player *pplayer);
	int (*ret_play)(struct rtsp_con_player *pplayer);
	int (*ret_pause)(struct rtsp_con_player *pplayer);
	int (*ret_teardown)(struct rtsp_con_player *pplayer);
	int (*ret_get_parameter)(struct rtsp_con_player *pplayer);
	int (*ret_set_parameter)(struct rtsp_con_player *pplayer);
} rtsp_con_player_t;

#define SET_PLAYER_FLAG(x, f) (x)->flag |= (f)
#define UNSET_PLAYER_FLAG(x, f) (x)->flag &= ~(f)
#define CHECK_PLAYER_FLAG(x, f) ((x)->flag & (f))



typedef struct rtcp_rtp_st{
    int32_t expected_seq_num;
    int32_t jitter;
    int32_t delta;
    int32_t expected_prior;
    int32_t received_prior;

}rtcp_rr_state_t;

typedef struct rtcp_r_pkt{
    uint8_t V;
    uint8_t P;
    uint8_t RC;
    uint8_t PT;
    uint16_t length;
    uint32_t SSRC_sender;
    uint32_t SSRC_source;
    uint8_t frac_lost;
    int32_t cum_pkts_lost;
    uint32_t seq_num;
    uint32_t jitter;
    uint32_t LSR;
    uint32_t DLSR;
    rtcp_rr_state_t rtcp_state;
}rtcp_rpkt_t;

typedef struct rtcp_s_pkt{
    uint8_t V;
    uint8_t P;
    uint8_t RC;
    uint8_t PT;
    uint16_t length;
    uint32_t SSRC;
    uint32_t NPT_MSB;
    uint32_t NPT_LSB;
    uint32_t RTP_TS;
    uint32_t pkt_cnt;
    uint32_t octet_pkt_cnt;
}rtcp_spkt_t;


/*
 * This structure is facing to publish server
 * One publish server can have a list of video players.
 */
#define PPSF_LIVE_VIDEO		0x0000000000000001
#define PPSF_VIDEO_PLAYING	0x0000000000000002
#define PPSF_RECV_INLINE_DATA	0x0000000000000004
#define PPSF_KEEP_ALIVE		0x0000000000000008
#define PPSF_OWN_REQUEST	0x0000000000000010
#define PPSF_RECV_CONTENT	0x0000000000000020
#define PPSF_SETUP_DONE		0x0000000000000040
#define PPSF_TEARDOWN		0x0000000000000080
#define PPSF_TPROXY_MODE	0x0000000000000100
#define PPSF_REQUEST_PENDING	0x0000000000000200
#define PPSF_INTERNAL_PLAYER	0x0000000000000400
#define PPSF_IN_URI_LIST	0x0000000000000800
#define PPSF_CLOSE		0x0000000000001000

/* Flags for mutex lock
 */
#define PPSF_PS_GNM_LOCK	0x0001000000000000
#define PPSF_PS_LOCK		0x0002000000000000
#define PPSF_PL_GNM_LOCK	0x0010000000000000
#define PPSF_PL_LOCK		0x0020000000000000


/* Flags based on user agent, for any server specfic actions
 */
#define PPSF_SERVER_QTSS	0x0100000000000000
#define PPSF_SERVER_WMS		0x0200000000000000

typedef struct rtsp_con_publish_svr {
	uint32_t        magic;
	uint64_t	flag;

	int		method;
	int		g_ps_index;
	int		qtss_custom_flag;
	uint32_t	cseq;
	uint32_t	heartbeat_cseq;
	//uint32_t	state;
	char * 		streamingModeString;
	char *		session;
	char * 		ssrc;
	char * 		mode;
	char *		rtp_buf;
	char *		uri;
	char *		hostname;	// Origin server Domain
	uint32_t	peer_ip_addr; 	// peer IP address, network order
	uint32_t	own_ip_addr; 	// MFD IP address, network order
	int		port;
	int		cur_trackID; // Only set in the player side
	int		rtp_buf_len;
	uint64_t	rtp_info_seq[MAX_TRACK];
	int32_t         rtsp_fd;	// OM server fd
	rl_bw_smooth_ctx_t *rl_ps_bws_ctx;
	rls_track_t	rtp[MAX_TRACK];
	rls_track_t	rtcp[MAX_TRACK];
	rl_sdp_info_t   sdp_info;	// SDP parsing result
	int32_t 	sdp_size;	// origina sdp string
	char *		sdp_string;	// origina sdp string
	time_t		rtp_last_ts;

	/* For live streaming match */
	struct rtsp_cb	rtsp;
	char * 		nsconf_uid;
	struct rtsp_con_player * pplayer_list; // link to player list
	TaskToken	fkeep_alive_task;
	rtcp_spkt_t	rtcp_s;
	rtcp_rpkt_t	rtcp_r;
	char		rtcp_sdes[RTCP_SDES_SIZE];
	char		rtcp_bye[RTCP_BYE_SIZE];
	uint8_t		ps_state;
	char *		optionsString;
	int 		transport;
	pthread_mutex_t mutex;

	char * (*send_options)(struct rtsp_con_publish_svr *pps);
	char * (*send_describe)(struct rtsp_con_publish_svr *pps);
	char * (*send_setup)(struct rtsp_con_publish_svr *pps);
	char * (*send_play)(struct rtsp_con_publish_svr *pps);
	char * (*send_pause)(struct rtsp_con_publish_svr *pps);
	char * (*send_teardown)(struct rtsp_con_publish_svr *pps);
	char * (*send_get_parameter)(struct rtsp_con_publish_svr *pps);
	char * (*send_set_parameter)(struct rtsp_con_publish_svr *pps);
	
	char * (*recv_options)(struct rtsp_con_publish_svr *pps);
	char * (*recv_describe)(struct rtsp_con_publish_svr *pps);
	char * (*recv_setup)(struct rtsp_con_publish_svr *pps);
	char * (*recv_play)(struct rtsp_con_publish_svr *pps);
	char * (*recv_pause)(struct rtsp_con_publish_svr *pps);
	char * (*recv_teardown)(struct rtsp_con_publish_svr *pps);
	char * (*recv_get_parameter)(struct rtsp_con_publish_svr *pps);
	char * (*recv_set_parameter)(struct rtsp_con_publish_svr *pps);
	
} rtsp_con_ps_t;

#define SET_PPS_FLAG(x, f) (x)->flag |= (f)
#define UNSET_PPS_FLAG(x, f) (x)->flag &= ~(f)
#define CHECK_PPS_FLAG(x, f) ((x)->flag & (f))

#define PORPF_ACTIVE		0x0000000000000001

#define SET_PORP_FLAG(x, f) (x)->flag |= (f)
#define UNSET_PORP_FLAG(x, f) (x)->flag &= ~(f)
#define CHECK_PORP_FLAG(x, f) ((x)->flag & (f))

typedef struct orp_fetch_uri {
	uint32_t		magic;
	struct orp_fetch_uri 	*next;
	uint64_t 		flag;
	uint32_t 		dip; 	// network order, original dest ip
	uint32_t 		dport; // network order, original dest port
	char 			physid[NKN_PHYSID_MAXLEN];
	char 			*uri;
	char 			*cache_uri;
	int 			urilen;
	nkn_interface_t 	*pns;
	rtsp_con_player_t 	*pplayer;
	pthread_mutex_t		*pmutex;
} orp_fetch_uri_t;

typedef struct orp_fetch_om_uri {
	uint64_t flag;
	uint32_t dip; 	// network order, original dest ip
	uint32_t dport; // network order, original dest port
	char physid[NKN_PHYSID_MAXLEN];
	char * uri;
	char * cache_uri;
	int urilen;
} orp_fetch_om_uri_t;

#define SET_ORP_FLAG(x, f) (x)->flag |= (f)
#define UNSET_ORP_FLAG(x, f) (x)->flag &= ~(f)
#define CHECK_ORP_FLAG(x, f) ((x)->flag & (f))

#define ORPF_FETCHING		0x0000000000000001
#define ORPF_START_PLAY		0x0000000000000002
#define ORPF_SEND_BUFFER	0x0000000000000004

typedef struct ps_fetch_list {
	struct ps_fetch_list * next;
	uint64_t flag;
	char * uri;
	int urilen;
	rtsp_con_ps_t * pps;
} ps_fetch_list_t;


#endif // _RTSP_SERVER_LIVE__HH
