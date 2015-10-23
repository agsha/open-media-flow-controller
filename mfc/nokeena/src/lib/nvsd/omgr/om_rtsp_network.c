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
// #include <linux/netfilter_ipv4.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <atomic_ops.h>
#include <netinet/ip.h>
#include <stdint.h>
#include <atomic_ops.h>
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
/***************************************************************************
 *                       COUNTER DEFINITIONS
 **************************************************************************/
/* Network counters */
NKNCNT_DEF(rtsp_shared_live_session, uint64_t, "", "shared live session")
NKNCNT_DEF(rtsp_alloc_sess_bw_err, uint64_t, "", "no session bw for rtsp")
NKNCNT_DEF(rtp_tot_udp_packets_fwd, uint64_t, "", "Total RTP/UDP pkts forwarded")
NKNCNT_DEF(rtcp_tot_udp_packets_fwd, uint64_t, "", "Total RTCP pkts forwarded")
NKNCNT_DEF(rtsp_err_namespace_not_found, uint64_t, "", "Error not found pub-point")
NKNCNT_EXTERN(cur_open_rtsp_socket, AO_t)
NKNCNT_DEF(rtsp_tot_size_from_origin, AO_t, "", "size from RTSP tunnel")
NKNCNT_DEF(rtsp_tot_size_from_cache, AO_t, "", "size from RTSP RAM Cache")
NKNCNT_DEF(rtsp_tot_size_to_cache, AO_t, "", "size to RTSP Cache")
NKNCNT_DEF(rtcp_tot_udp_sr_packets_sent, uint64_t, "", "Total RTCP Sender report pkts forwarded")
NKNCNT_DEF(rtcp_tot_udp_rr_packets_sent, uint64_t, "", "Total RTCP Rec'er report pkts forwarded")
NKNCNT_DEF(rtsp_tot_rtp_ps_trylock_busy, AO_t, "", "Lock busy")

/* counters for RTSP status codes */
NKNCNT_EXTERN(rtsp_tot_status_resp_100, AO_t);
NKNCNT_EXTERN(rtsp_tot_status_resp_200, AO_t);
NKNCNT_EXTERN(rtsp_tot_status_resp_201, AO_t);
NKNCNT_EXTERN(rtsp_tot_status_resp_250, AO_t);
NKNCNT_EXTERN(rtsp_tot_status_resp_300, AO_t);
NKNCNT_EXTERN(rtsp_tot_status_resp_301, AO_t);
NKNCNT_EXTERN(rtsp_tot_status_resp_302, AO_t);
NKNCNT_EXTERN(rtsp_tot_status_resp_303, AO_t);
NKNCNT_EXTERN(rtsp_tot_status_resp_304, AO_t);
NKNCNT_EXTERN(rtsp_tot_status_resp_305, AO_t);
NKNCNT_EXTERN(rtsp_tot_status_resp_400, AO_t);//, "Total 400 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_401, AO_t);//, "Total 401 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_402, AO_t);//, "Total 402 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_403, AO_t);//, "Total 403 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_404, AO_t);//, "Total 404 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_405, AO_t);//, "Total 405 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_406, AO_t);//, "Total 406 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_407, AO_t);//, "Total 407 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_408, AO_t);//, "Total 408 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_410, AO_t);//, "Total 410 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_411, AO_t);//, "Total 411 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_412, AO_t);//, "Total 412 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_413, AO_t);//, "Total 413 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_414, AO_t);//, "Total 414 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_415, AO_t);//, "Total 415 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_451, AO_t);//, "Total 451 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_452, AO_t);//, "Total 452 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_453, AO_t);//, "Total 453 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_454, AO_t);//, "Total 454 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_455, AO_t);//, "Total 455 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_456, AO_t);//, "Total 456 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_457, AO_t);//, "Total 457 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_458, AO_t);//, "Total 458 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_459, AO_t);//, "Total 459 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_460, AO_t);//, "Total 460 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_461, AO_t);//, "Total 461 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_462, AO_t);//, "Total 462 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_500, AO_t);//, "Total 500 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_501, AO_t);//, "Total 501 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_502, AO_t);//, "Total 502 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_503, AO_t);//, "Total 503 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_504, AO_t);//, "Total 504 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_505, AO_t);//, "Total 505 response");
NKNCNT_EXTERN(rtsp_tot_status_resp_551, AO_t);//, "Total 551 response");

/*
 * external variables and functions
 */
extern int rtsp_player_idle_timeout;
extern int rtsp_origin_idle_timeout;
extern pthread_mutex_t counter_mutex;

int rl_get_udp_sockfd(uint16_t port, int setLoopback, int reuse_port, struct in_addr *IfAddr);
extern uint32_t dns_domain_to_ip(char * domain);
extern int rtsp_set_transparent_mode_tosocket(int fd, rtsp_con_ps_t * pps);
extern void *orp_add_new_video(nkn_interface_t * pns, uint32_t dip, uint16_t dport, 
		char * uri, char *cache_uri, namespace_token_t nstoken, char *vfs_filename);
extern int orp_fsm(rtsp_con_player_t * pplayer, int locks_held);
extern int orp_write_sdp_to_tfm(rtsp_con_player_t * pplayer);
extern int orp_send_data_to_tfm_udp(rtsp_con_player_t * pplayer, char *buf, int len, int type, int track);
extern int orp_send_data_to_tfm_tcp(rtsp_con_player_t * pplayer, char *buf, int len, int type, int track);
extern void orp_teardown_state(void* clientData);
extern int orp_close(rtsp_con_player_t * pplayer, int lock_held);

extern int rl_player_wmp_set_callbacks( rtsp_con_player_t * pplayer );
extern int rl_ps_wms_set_callbacks( rtsp_con_ps_t * pps );


/*
 * local variables and functions
 */

static pthread_mutex_t ses_bw_mutex = PTHREAD_MUTEX_INITIALIZER;
int rtsp_server_port = 554;
extern uint64_t glob_rtsp_tot_bytes_sent;
extern uint64_t glob_tot_video_delivered;
static pthread_mutex_t g_ps_mutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t rtp_port_mutex = PTHREAD_MUTEX_INITIALIZER;
//static pthread_mutex_t rtcp_port_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t g_ps_list_check_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_ps_list_mutex = PTHREAD_MUTEX_INITIALIZER;
static ps_fetch_list_t *g_ps_list = NULL;

#define MAX_RLCON_PS		4000

/* global declarations */
static rtsp_con_ps_t * g_ps[MAX_RLCON_PS]; /*list of pub-servers */
uint16_t rtp_udp_port = 4444; /* starting port number for RTP channels port assignment */
uint16_t rtcp_udp_port = 4445; /* starting port number for RTCP channels port assignment */
static uint64_t g_ssrc = 122000;

char rl_rtsp_server_str[64] = "Juniper Server (2.0.3)";
char rl_rtsp_player_str[64] = "Juniper Media Player (2.0.3)";

/* Prototypes
 * nomenclature: rl_ prefix indicates relay
 *               rtp_ prefix indicates rtp connection handlers
 *               rtcp_ prefix indicates rtcp connection handlers
 *               rl_ps denotes MFD <--> Origin connections
 *               rl_player denotes MFD <--> Viewer connections
 */

static int rl_player_ret_our_options (rtsp_con_player_t * pplayer);
int rl_player_form_sdp(rtsp_con_player_t * pplayer, char *sdp, int buflen);

static int rl_player_tproxy_ret_status_resp(rtsp_con_player_t * pplayer, char * hdr, int hdr_len);
static int rl_player_tproxy_ret_options (rtsp_con_player_t * pplayer);
static int rl_player_tproxy_ret_describe(rtsp_con_player_t * pplayer);
static int rl_player_tproxy_ret_setup(rtsp_con_player_t * pplayer);
static int rl_player_tproxy_ret_pause(rtsp_con_player_t * pplayer);
static int rl_player_tproxy_ret_play(rtsp_con_player_t * pplayer);
static int rl_player_tproxy_ret_get_parameter(rtsp_con_player_t * pplayer);
static int rl_player_tproxy_ret_set_parameter(rtsp_con_player_t * pplayer);
static int rl_player_tproxy_ret_teardown(rtsp_con_player_t * pplayer);

static int rl_player_live_ret_status_resp(rtsp_con_player_t * pplayer, char * hdr, int hdr_len);
static int rl_player_live_ret_options (rtsp_con_player_t * pplayer);
static int rl_player_live_ret_describe(rtsp_con_player_t * pplayer);
static int rl_player_live_ret_setup(rtsp_con_player_t * pplayer);
static int rl_player_live_ret_pause(rtsp_con_player_t * pplayer);
static int rl_player_live_ret_play(rtsp_con_player_t * pplayer);
static int rl_player_live_ret_get_parameter(rtsp_con_player_t * pplayer);
static int rl_player_live_ret_set_parameter(rtsp_con_player_t * pplayer);
static int rl_player_live_ret_teardown(rtsp_con_player_t * pplayer);
static int rl_player_live_set_callbacks( rtsp_con_player_t * pplayer );
static int rl_player_teardown(rtsp_con_player_t * pplayer, int error, int locks_held);
int rl_player_process_rtsp_request(int sockfd, rtsp_con_player_t * pplayer, int locks_held);

static char *rl_ps_tproxy_send_method_options(rtsp_con_ps_t * pps);
static char *rl_ps_tproxy_send_method_describe(rtsp_con_ps_t * pps);
static char *rl_ps_tproxy_send_method_setup(rtsp_con_ps_t * pps);
static char *rl_ps_tproxy_send_method_play(rtsp_con_ps_t * pps);
static char *rl_ps_tproxy_send_method_pause(rtsp_con_ps_t * pps);
static char *rl_ps_tproxy_send_method_get_parameter(rtsp_con_ps_t * pps);
static char *rl_ps_tproxy_send_method_set_parameter(rtsp_con_ps_t * pps);
static char *rl_ps_tproxy_send_method_teardown(rtsp_con_ps_t * pps);

static char *rl_ps_live_send_method_options(rtsp_con_ps_t * pps);
static char *rl_ps_live_send_method_describe(rtsp_con_ps_t * pps);
static char *rl_ps_live_send_method_setup(rtsp_con_ps_t * pps);
static char *rl_ps_live_send_method_play(rtsp_con_ps_t * pps);
static char *rl_ps_live_send_method_pause(rtsp_con_ps_t * pps);
static char *rl_ps_live_send_method_get_parameter(rtsp_con_ps_t * pps);
static char *rl_ps_live_send_method_set_parameter(rtsp_con_ps_t * pps);
static char *rl_ps_live_send_method_teardown(rtsp_con_ps_t * pps);

static char * rl_ps_tproxy_buildup_req_head(rtsp_con_ps_t * pps, char * add_hdr, int add_hdr_size);
static char * rl_ps_tproxy_recv_method_options(rtsp_con_ps_t * pps);
static char * rl_ps_tproxy_recv_method_describe(rtsp_con_ps_t * pps);
static char * rl_ps_tproxy_recv_method_setup(rtsp_con_ps_t * pps);
static char * rl_ps_tproxy_recv_method_play(rtsp_con_ps_t * pps);
static char * rl_ps_tproxy_recv_method_pause(rtsp_con_ps_t * pps);
static char * rl_ps_tproxy_recv_method_get_parameter(rtsp_con_ps_t * pps);
static char * rl_ps_tproxy_recv_method_set_parameter(rtsp_con_ps_t * pps);
static char * rl_ps_tproxy_recv_method_teardown(rtsp_con_ps_t * pps);

static char * rl_ps_live_buildup_req_head(rtsp_con_ps_t * pps, char * add_hdr, int add_hdr_size);
static char * rl_ps_live_form_req_head(rtsp_con_ps_t * pps, const char * method, char * add_hdr, int add_hdr_size);
static char * rl_ps_live_recv_method_options(rtsp_con_ps_t * pps);
static char * rl_ps_live_recv_method_describe(rtsp_con_ps_t * pps);
static char * rl_ps_live_recv_method_setup(rtsp_con_ps_t * pps);
static char * rl_ps_live_recv_method_play(rtsp_con_ps_t * pps);
static char * rl_ps_live_recv_method_pause(rtsp_con_ps_t * pps);
static char * rl_ps_live_recv_method_get_parameter(rtsp_con_ps_t * pps);
static char * rl_ps_live_recv_method_set_parameter(rtsp_con_ps_t * pps);
static char * rl_ps_live_recv_method_teardown(rtsp_con_ps_t * pps);

static int rl_player_connect_to_svr(rtsp_con_player_t * pplayer);
void rl_player_closed(rtsp_con_player_t * pplayer, int locks_held);
int rl_ps_closed(rtsp_con_ps_t * pps, rtsp_con_player_t * no_callback_for_this_pplayer, int locks_held);
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
int32_t rl_ps_parse_sdp_info( char **p_sdp_data, rtsp_con_ps_t *pps);
uint32_t rl_get_sample_rate(rtsp_con_ps_t *pps, char * trackID);
int32_t rl_init_bw_smoothing(rl_bw_smooth_ctx_t **pprl_bws);
rl_bw_query_result_t rl_query_bw_smoothing(rl_bw_smooth_ctx_t *prl_bws, char *packet, uint32_t len);
int32_t rl_cleanup_bw_smoothing(rl_bw_smooth_ctx_t *prl_bws);
int32_t rl_ps_connect(rtsp_con_ps_t *pps);
static int32_t rl_alloc_sess_bw(nkn_interface_t *pns, int32_t max_allowed_bw);
static int32_t rl_dealloc_sess_bw(nkn_interface_t *pns, int32_t max_allowed_bw);
void rl_set_channel_id(rtsp_con_ps_t *pps, char * trackID, uint8_t channel_id);
int rl_get_track_num(rtsp_con_ps_t *pps, char * trackID);

static void rl_ps_relay_rtp_data(rtsp_con_ps_t *pps);
static int rl_player_validate_rtsp_request(rtsp_con_player_t *pplayer);

/* Helper Functions */
void rtsplive_keepaliveTask(rtsp_con_ps_t * pps);
static rtsp_con_ps_t * rl_create_new_ps(rtsp_cb_t *prtsp, char *uri, char *server, uint16_t port);
int32_t rl_ps_fsm(rtsp_con_ps_t *pps, rtsp_cb_t *prtsp);


/* RTCP helper functions */
int form_rtcp_rr_pkt(rtcp_rtp_state_t,rtcp_spkt_t, rtcp_rpkt_t *,char*);
int read_rtcp_sr_pkt(rtcp_spkt_t*,char*);
int create_rtcp_rr_pkt(rtcp_rpkt_t,char*);
int form_rtcp_sr_pkt(rtcp_spkt_t*,char*);
int create_rtcp_sr_pkt(rtcp_spkt_t, char*);
int byte_read32(char*,int);
int byte_write32(char*,int ,int);
int byte_write24(char* ,int,int);
int byte_write16(char*,int,int);
int rtcp_rtp_state_update( rtcp_rtp_state_t *,char *);
long long delta_timeval( struct timeval*, struct timeval*);
void form_rtcp_sdes(char * ,uint32_t);

rtsp_con_ps_t * rl_ps_list_check_uri(char * uri);
rtsp_con_ps_t * rl_ps_list_check_and_add_uri(char * uri, int urilen, rtsp_con_ps_t * pps);
int rl_ps_list_add_uri(char * uri, int urilen, rtsp_con_ps_t * pps);
int rl_ps_list_remove_uri(char * uri);

int lock_ps( rtsp_con_ps_t * pps, int locks_held);
int lock_pl( rtsp_con_player_t * pplayer, int locks_held);
int trylock_pl( rtsp_con_player_t * pplayer, int locks_held);


/* port network order */
int rl_get_udp_sockfd(uint16_t port, int setLoopback, int reuse_port, struct in_addr *IfAddr)
{
        int fd = socket(AF_INET, SOCK_DGRAM, 0);	// Valgrind Warning: invalid file descriptor 1019 in syscall socket()
        int reuseFlag = reuse_port;

        if (fd < 0) {
                return -1;
        }
	DBG_LOG(MSG, MOD_RTSP, "New Udp fd=%d", fd);
        
        nkn_mark_fd(fd, RTSP_OM_UDP_FD);

        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                        (const char*)&reuseFlag, sizeof reuseFlag) < 0) {
                nkn_close_fd(fd, RTSP_OM_UDP_FD);
                return -1;
        }

#ifdef SO_REUSEPORT
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT,
                        (const char*)&reuseFlag, sizeof reuseFlag) < 0) {
                nkn_close_fd(fd, RTSP_OM_UDP_FD);
                return -1;
        }
#endif

#ifdef IP_MULTICAST_LOOP
        const u_int8_t loop = (u_int8_t)setLoopback;
        if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP,
                        (const char*)&loop, sizeof loop) < 0) {
                nkn_close_fd(fd, RTSP_OM_UDP_FD);
                return -1;
        }
#endif
        //UNUSED_ARGUMENT(IfAddr);
        //uint32_t addr = INADDR_ANY;
        uint32_t addr;
        if ( IfAddr != NULL )
                addr = IfAddr->s_addr;
        else
                addr = INADDR_ANY;
        if (port != 0/* || ReceivingInterfaceAddr != INADDR_ANY*/) {
                //if (port == 0) addr = ReceivingInterfaceAddr;
                MAKE_SOCKADDR_IN(name, addr, port);
                if (bind(fd, (struct sockaddr*)&name, sizeof name) != 0) {
                        nkn_close_fd(fd, RTSP_OM_UDP_FD);
                        return -1;
                }
        }

#if 0
        // Set the sending interface for multicasts, if it's not the default:
        if (SendingInterfaceAddr != INADDR_ANY) {
                struct in_addr laddr;
                laddr.s_addr = SendingInterfaceAddr;

                if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF,
                                (const char*)&laddr, sizeof(laddr)) < 0) {
                        printf("error setting outgoing multicast interface: ");
                        nkn_close_fd(fd, RTSP_FD);
                        return -1;
                }
        }
#endif // 0

        return fd;
}

static int find_shared_live_session(rtsp_con_player_t * pplayer);
static int find_shared_live_session(rtsp_con_player_t * pplayer)
{
	int i;
	rtsp_con_ps_t * pps;
        const namespace_config_t *nsconf;

#if 0
	/*
	 * Support UDP sharing only for now.
	 */
	if (pplayer->prtsp->streamingMode != RTP_UDP) {
		return NULL;
	}
#endif
	RTSP_GET_ALLOC_ABS_URL(&pplayer->prtsp->hdr, pplayer->prtsp->urlPreSuffix);

	if (!VALID_NS_TOKEN(pplayer->prtsp->nstoken)) {
		pplayer->prtsp->nstoken = rtsp_request_to_namespace(&pplayer->prtsp->hdr);
	}
	nsconf = namespace_to_config(pplayer->prtsp->nstoken);
	if (nsconf == NULL) {
		return -1;
	}

	/* Check if cacheable flag is set
	 * Todo: Check if more entries are present and scan all
	 *	 entries and validate mode also.
	 */
	if (nsconf->pub_point_config->entries) {
		if (nsconf->pub_point_config->events[0]->enable) {
			SET_PLAYER_FLAG(pplayer, PLAYERF_LIVE_ENABLED);
			if (nsconf->pub_point_config->events[0]->cacheable) {
				SET_PLAYER_FLAG(pplayer, PLAYERF_LIVE_CACHEABLE);
			}
		}
	}

	/* update cache age from namespace
	 */
	pplayer->cache_age = nsconf->rtsp_config->policies.cache_age_default;
	/*
	 * try to find a live video.
	 * searching existing rtsp_con_ps_t connections.
	 */
	pthread_mutex_lock(&g_ps_mutex);
	for(i=0; i<MAX_RLCON_PS; i++) {
		if (g_ps[i] == NULL) continue;
		pps = g_ps[i];
		if ((CHECK_PPS_FLAG(pps, PPSF_LIVE_VIDEO)) &&
				/*(CHECK_PPS_FLAG(pps, PPSF_VIDEO_PLAYING)) && */
				(strcmp(pps->nsconf_uid, nsconf->ns_uid) == 0) &&
				(strncmp(pps->uri, pplayer->prtsp->urlPreSuffix, 
				strlen(pps->uri)) == 0)) {
			if (pplayer->player_type == RL_PLAYER_TYPE_FAKE_PLAYER) {
				pthread_mutex_unlock(&g_ps_mutex);
				release_namespace_token_t(pplayer->prtsp->nstoken);
				return -2;
			}
			
			/* Is this server is used by the internal player, do not
			 * share it with other players.
			 */
			if (CHECK_PPS_FLAG(pps, PPSF_INTERNAL_PLAYER)) {
				continue;
			}
			/* BUG: we should use trylock here */
			DBG_LOG(MSG, MOD_RTSP, "got one shared live session");
			glob_rtsp_shared_live_session ++;
			pthread_mutex_unlock(&g_ps_mutex);
			
			release_namespace_token_t(pplayer->prtsp->nstoken);

			/* Add into link list */
			pthread_mutex_lock(&pps->rtsp.mutex);
			if (pps->pplayer_list) {
				pplayer->next = pps->pplayer_list->next;
				pps->pplayer_list->next = pplayer;
			}
			else {
				pplayer->next = pps->pplayer_list;
				pps->pplayer_list = pplayer;
			}
			pplayer->pps = pps;
			pthread_mutex_unlock(&pps->rtsp.mutex);
			return 1;
		}
	}

	pthread_mutex_unlock(&g_ps_mutex);
	release_namespace_token_t(pplayer->prtsp->nstoken);
	return -2;
}

/* ==================================================================== */
/*
 * Define network callback functions for 
 * epollin/epollout/epollerr/epollhup 
 */
/* ==================================================================== */
int rl_player_sendout_resp(rtsp_con_player_t * pplayer, char * p, int len);
int rl_player_sendout_resp(rtsp_con_player_t * pplayer, char * p, int len)
{
	int ret, sendlen;

	if(pplayer == NULL) return FALSE;

	UNSET_PLAYER_FLAG(pplayer, PLAYERF_REQUEST_PENDING);
	DBG_LOG(MSG, MOD_RTSP, "Response sent (fd=%d, len=%d):\n<%s>", 
			pplayer->rtsp_fd, len, p);
	if (pplayer->player_type == RL_PLAYER_TYPE_FAKE_PLAYER)
		return TRUE;
	
	sendlen = 0;
	while(len) {
		ret = send(pplayer->rtsp_fd, p, len, 0);
		if( ret==-1 ) {
			if(errno == EAGAIN) {

				/* BUG: to be supported */
				continue;
				NM_add_event_epollout(pplayer->rtsp_fd);
				//NM_del_event_epoll(rlcon->peer->rtsp_fd);
				return TRUE;
			}
			return FALSE;
		} 
		p += ret;
		len -= ret;
		sendlen += ret;

		glob_rtsp_tot_bytes_sent += ret;
		pplayer->prtsp->out_bytes += ret;
	}

	return TRUE;
}


/* Call back function for sending data.
 * Currently supports inline TCP data only. Support for UDP data
 * would be added and also ability to convert from TCP inline to UDP.
 * Or can have diff. functions for TCP, UDP and TCP->UDP.
 */
int rl_player_sendout_inline_data(rtsp_con_player_t * pplayer, 
				char *buf, int len, int type, int track);
int rl_player_sendout_udp_data(rtsp_con_player_t * pplayer, 
				char *buf, int len, int type, int track);
int rl_player_sendout_udp_as_inline_data(rtsp_con_player_t * pplayer, 
				char *buf, int len, int type, int track);
int rl_player_sendout_inline_as_udp_data(rtsp_con_player_t * pplayer, 
				char *buf, int len, int type, int track);

int 
rl_player_sendout_inline_data(rtsp_con_player_t * pplayer, 
				char *buf, 
				int len, 
				int type, 
				int track)
{
	int ret;
	char *p = NULL;
	rls_track_t *pt;

	UNUSED_ARGUMENT(track);

	if (pplayer == NULL || type != RL_DATA_RTP) return FALSE;

	pt = &pplayer->rtp[0];
	p = buf;
	while (len) {
		ret = send(pplayer->rtsp_fd, p, len, 0);
		if ( ret == -1 ) {
			//printf("rtp_data: ret=%d errno=%d len=%d\n", ret, errno, len);
			if (errno == EAGAIN) {
				if (pt->num_resend_pkts < MAX_QUEUED_PKTS &&
						len <= RTP_MTU_SIZE) {
					if (pt->resend_buf[pt->num_resend_pkts].iov_base == NULL) {
						char *ptr;

						ptr = (char*)nkn_malloc_type(RTP_MTU_SIZE, mod_rtsp_type_3); 
						if (ptr == NULL)
							return FALSE;
						pt->resend_buf[pt->num_resend_pkts].iov_base = ptr;
						//pt->resend_buf[pt->num_resend_pkts].iov_len = RTP_MTU_SIZE;
					}

					memcpy(pt->resend_buf[pt->num_resend_pkts].iov_base,
						p,
						len);
					pt->resend_buf[pt->num_resend_pkts].iov_len = len;
					pt->num_resend_pkts++;
					pt->num_resend_pkts = (pt->num_resend_pkts % MAX_QUEUED_PKTS); 
					NM_add_event_epollout(pplayer->rtsp_fd);
				}
				return TRUE;
			}
			return FALSE;
		} 
		glob_rtsp_tot_bytes_sent += ret;
		pplayer->prtsp->out_bytes += ret;
		len -= ret;
		p += ret;
	}
	//	SET_PLAYER_FLAG(pplayer, PLAYERF_KEEP_ALIVE);
	return TRUE;
}

int 
rl_player_sendout_udp_data(rtsp_con_player_t * pplayer, 
			char *buf, 
			int len, 
			int type, 
			int track)
{
	int ret;
	char *p = NULL;
	struct sockaddr_in to;
	socklen_t tolen;
	rls_track_t *pt;

	if (pplayer == NULL || type == RL_DATA_INLINE) return FALSE;

	pt = (type == RL_DATA_RTP) ? &pplayer->rtp[track] : &pplayer->rtcp[track];
	p = buf;
	memset(&to, 0, sizeof(to));
	to.sin_family = AF_INET;
	to.sin_port = htons(pt->peer_udp_port);
	to.sin_addr.s_addr = pplayer->peer_ip_addr;
	tolen = sizeof(to);

	ret = sendto(pt->udp_fd, 
		p,
		len,
		0,
		(struct  sockaddr *)&to,
		tolen);

	if (ret < 0)  {
		//printf("errno=%d ret=%d\n", errno, ret);
		if (errno == EAGAIN) {
			/*BZ 3730:
			 *when EGAIN happens we need to copy the left over packets and 
			 *send it when we get an EPOLLOUT event and __remove__ EPOLLOUT 
			 *event when it is sent out.
			 *CAVEAT: we cannot store more than 64 packets per client
			 */
			if (pt->num_resend_pkts < MAX_QUEUED_PKTS &&
					len <= RTP_MTU_SIZE) {
				if (pt->resend_buf[pt->num_resend_pkts].iov_base == NULL) {
					char *ptr;

					ptr = (char*)nkn_malloc_type(RTP_MTU_SIZE, mod_rtsp_type_3); 
					if (ptr == NULL)
						return FALSE;
					pt->resend_buf[pt->num_resend_pkts].iov_base = ptr;
					//pt->resend_buf[pt->num_resend_pkts].iov_len = RTP_MTU_SIZE;
				}
				memcpy(pt->resend_buf[pt->num_resend_pkts].iov_base,
					p,
				        len);
				pt->resend_buf[pt->num_resend_pkts].iov_len = len;
				pt->num_resend_pkts++;
				pt->num_resend_pkts = (pt->num_resend_pkts % MAX_QUEUED_PKTS); 
				NM_add_event_epollout(pt->udp_fd);
			}
			return TRUE;
		}	
		return FALSE;
	}
	//	SET_PLAYER_FLAG(pplayer, PLAYERF_KEEP_ALIVE);

	if (type == RL_DATA_RTP) {
		glob_rtsp_tot_bytes_sent += ret;
	}
	else {
		glob_rtcp_tot_udp_sr_packets_sent++;
	}
	
	return TRUE;
}

int 
rl_player_sendout_udp_as_inline_data(rtsp_con_player_t * pplayer, 
				char *buf, 
				int len, 
				int type, 
				int track)
{
	int ret;
	char *p = NULL;
	rls_track_t *pt;

	/* Forward only RTP data for now. Need to confirm
	 * if RTCP data is also needs to be send in inline mode.
	 */
	if (pplayer == NULL || type != RL_DATA_RTP) return FALSE;

	pt = (type == RL_DATA_RTP) ? &pplayer->rtp[track] : &pplayer->rtcp[track];
	p = buf - 4;
	*p = '$';
	*(p+1) = pt->own_channel_id;
	*((uint16_t *)(p + 2)) = htons(len);
	len += 4;

	//p = buf;
	while (len) {
		ret = send(pplayer->rtsp_fd, p, len, 0);
		if ( ret == -1 ) {
			if (errno == EAGAIN) {
				/* TODO:
				 * Use iovec array as in RTP/UDP, to store multiple 
				 * packets instead of just one packet.
				 */
				if (pplayer->prtsp->rtp_data.rtp_buf_len ==0) {
					pplayer->prtsp->rtp_data.rtp_buf_len = len;
					pplayer->prtsp->rtp_data.rtp_buf = pplayer->prtsp->rtp_data.data_buf;
					pplayer->prtsp->rtp_data.data_len = len;
					memcpy( pplayer->prtsp->rtp_data.data_buf, p, len);
				}
				NM_add_event_epollout(pplayer->rtsp_fd);
				return TRUE;
			}
			return FALSE;
		} 
		glob_rtsp_tot_bytes_sent += ret;
		pplayer->prtsp->out_bytes += ret;
		len -= ret;
		p += ret;
	}
	//	SET_PLAYER_FLAG(pplayer, PLAYERF_KEEP_ALIVE);
	return TRUE;
}

int 
rl_player_sendout_inline_as_udp_data(rtsp_con_player_t * pplayer, 
				char *buf, 
				int len, 
				int type, 
				int track)
{
	int ret;
	char *p = NULL;
	struct sockaddr_in to;
	socklen_t tolen;
	rls_track_t *pt = NULL;
	uint32_t channel_id;
	int pkt_len, i;

	UNUSED_ARGUMENT(track);

	if (pplayer == NULL || type != RL_DATA_RTP) return FALSE;

	p = buf;
	if (*p != '$') return FALSE;
	channel_id = *(p+1);
	pkt_len = ntohs( *((uint16_t *)(p + 2)) );
	if (pkt_len != (len - 4)) return FALSE;
	//if (channel_id != track) return FALSE;

	for (i = 0; i < pplayer->pps->sdp_info.num_trackID ; i++) {
		if (pplayer->pps->rtp[i].own_channel_id == channel_id) {
			pt = &pplayer->rtp[i];
			break;
		}
		if (pplayer->pps->rtcp[i].own_channel_id == channel_id) {
			pt = &pplayer->rtcp[i];
			break;
		}
	}
	if (pt == NULL) return FALSE;
	
	memset(&to, 0, sizeof(to));
	to.sin_family = AF_INET;
	to.sin_port = htons(pt->peer_udp_port);
	to.sin_addr.s_addr = pplayer->peer_ip_addr;
	tolen = sizeof(to);

	ret = sendto(pt->udp_fd, 
		p + 4,
		pkt_len,
		0,
		(struct  sockaddr *)&to,
		tolen);

	if (ret < 0)  {
		//printf("errno=%d ret=%d\n", errno, ret);
		if (errno == EAGAIN) {
			/*BZ 3730:
			 *when EGAIN happens we need to copy the left over packets and 
			 *send it when we get an EPOLLOUT event and __remove__ EPOLLOUT 
			 *event when it is sent out.
			 *CAVEAT: we cannot store more than 64 packets per client
			 */
			if (pt->num_resend_pkts < MAX_QUEUED_PKTS &&
					len <= RTP_MTU_SIZE) {
				if (pt->resend_buf[pt->num_resend_pkts].iov_base == NULL) {
					char *ptr;

					ptr = (char*)nkn_malloc_type(RTP_MTU_SIZE, mod_rtsp_type_3); 
					if (ptr == NULL)
						return FALSE;
					pt->resend_buf[pt->num_resend_pkts].iov_base = ptr;
					//pt->resend_buf[pt->num_resend_pkts].iov_len = RTP_MTU_SIZE;
				}
				memcpy(pt->resend_buf[pt->num_resend_pkts].iov_base,
					p + 4,
				        pkt_len);
				pt->resend_buf[pt->num_resend_pkts].iov_len = pkt_len;
				pt->num_resend_pkts++;
				pt->num_resend_pkts = (pt->num_resend_pkts % MAX_QUEUED_PKTS); 
				NM_add_event_epollout(pt->udp_fd);
			}
			return TRUE;
		}	
		return FALSE;
	}
	//SET_PLAYER_FLAG(pplayer, PLAYERF_KEEP_ALIVE);
	
	if (type == RL_DATA_RTP) {
		glob_rtsp_tot_bytes_sent += ret;
	}
	else {
		glob_rtcp_tot_udp_sr_packets_sent++;
	}
	return TRUE;
}

int rl_player_write_streamminglog(rtsp_con_player_t * pplayer);
int rl_player_write_streamminglog(rtsp_con_player_t * pplayer)
{
	gettimeofday(&(pplayer->prtsp->end_ts), NULL);
	if(!CHECK_PLAYER_FLAG(pplayer, PLAYERF_INTERNAL))
		rtsp_streaminglog_write(pplayer->prtsp);
#if 0
	pplayer->prtsp->cb_reqlen = 0;
	pplayer->prtsp->cb_resplen = 0;
	pplayer->prtsp->cb_reqoffsetlen = 0;
	pplayer->prtsp->cb_respoffsetlen = 0;
#endif
	mime_hdr_shutdown(&pplayer->prtsp->hdr);

	return TRUE;
}

/* return new_sdplen */
int make_up_sdp_string(char * porg_sdp, char * sdp_buf, int sdp_buflen, uint32_t dst_ip);
int make_up_sdp_string(char * porg_sdp, char * sdp_buf, int sdp_buflen, uint32_t dst_ip)
{
	char *ps, *pd, *p;
	int len, sdplen;
	struct in_addr in;
	int full_control_url = 0;
	
        in.s_addr = dst_ip;

	/* 
	 * Buildup the SDP string first 
	 */
	if (!porg_sdp) return 0;

	// replace C=IN IP4
	ps = porg_sdp;
	pd = sdp_buf;
	sdplen = 0;
	p = strstr(ps, "o=");
	if (p) {
		p = strstr(p, "IN IP4 ");
		if(!p) return 0;
		p += 7; // sizeof("IN IP4 ")
		len = p-ps;
		memcpy(pd, ps, len);
		ps += len;
		pd += len;
		sdplen += len;
		
		/* If address is 0.0.0.0 or 127.0.0.1, do not replace it */
		if ( (strncmp(ps, "127.0.0.1", 9)!=0) &&
		     (strncmp(ps, "0.0.0.0", 7)!=0) ) {
			len = snprintf(pd, sdp_buflen-sdplen, 
				"%s",
				inet_ntoa(in));
			pd += len;
			sdplen += len;
			// skip c=IN IP4 line
			while(*ps!='\n' && *ps!='\r' && *ps!=' ') ps++;
		}
	}
	
	// replace C=IN IP4
	p = strstr(ps, "c=IN IP4 ");
	if (!p) return 0;
	p += 9; // sizeof("c=IN IP4 ")
	len = p-ps;
	memcpy(pd, ps, len);
	ps += len;
	pd += len;
	sdplen += len;
	
	/* If address is 0.0.0.0 or 127.0.0.1, do not replace it */
	if ( (strncmp(ps, "127.0.0.1", 9)!=0) &&
	     (strncmp(ps, "0.0.0.0", 7)!=0) ) {
		len = snprintf(pd, sdp_buflen-sdplen, 
			"%s",
			inet_ntoa(in));
		pd += len;
		sdplen += len;
		// skip c=IN IP4 line
		while(*ps!='\n' && *ps!='\r' && *ps!=' ') ps++;
	}
	
	while(1) {
		// replace a=control:rtsp://xxxx:8080/xx
		p = strstr(ps, "a=control:rtsp://");
		if (!p) {
			len = strlen(ps);
			memcpy(pd, ps, len);
			ps += len;
			pd += len;
			sdplen += len;
			*pd = 0;
			break;
		}
		p += 17; // sizeof("a=control:rtsp://")
		len = p-ps;
		memcpy(pd, ps, len);
		ps += len;
		pd += len;
		sdplen += len;

		len = snprintf(pd, sdp_buflen-sdplen, 
				"%s",
				inet_ntoa(in));
		pd += len;
		sdplen += len;
		full_control_url = 1;

		while(*ps!='/' && *ps!='\n' && *ps!='\r' && *ps!=' ') ps++;
	}
	if (sdplen != pd - sdp_buf) return 0;

	return sdplen;
}

int rl_player_form_sdp(rtsp_con_player_t * pplayer, char *sdp, int buflen)
{
	//Since this function could be called from either the publisher or the player we
	//need to ensure that we check the state before sending a describe.
	//if (pplayer->method >= METHOD_SENT_DESCRIBE_RESP) return TRUE;

	if (!pplayer || !pplayer->pps) return 0;
	return make_up_sdp_string(pplayer->pps->sdp_string, sdp, buflen, pplayer->own_ip_addr);
}


/* If hdr_len is non zero, hdr is appended to the response. 
 * Else trailing \r\n is added. If hdr is passed it the
 * responisibilty of thr caaling function to add the addtion trailing \r\n.
 */
static int rl_player_tproxy_ret_status_resp(rtsp_con_player_t * pplayer, char * hdr, int hdr_len)
{
#define OUTBUF(_data, _datalen) { \
	assert(((_datalen)+pplayer->prtsp->cb_resplen) < RTSP_BUFFER_SIZE); \
	memcpy((char *)&pplayer->prtsp->cb_respbuf[pplayer->prtsp->cb_resplen], \
		(_data), (_datalen)); \
	pplayer->prtsp->cb_resplen += (_datalen); \
}

	rtsp_con_ps_t *pps;
 	//const char *name;
	//int namelen;
	const char *data;
	int datalen;
	u_int32_t attrs;
	int hdrcnt;
	//int nth;
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
		case RTSP_HDR_RANGE: // Don't forward Range
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

		} else {
	    		continue;
		}
#if 0
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

#if 0
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

static int rl_player_ret_our_options (rtsp_con_player_t * pplayer)
{
	/* 
	 * RealPlayer sends OPTIONS without uri in the respuest,
	 * So we return the value directly.
	 */
	const char * hdr = 
		"Public: OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, GET_PARAMETER\r\n\r\n";
        pplayer->prtsp->status = 200;
	return rl_player_live_ret_status_resp(pplayer, (char *)hdr, strlen(hdr));
}

static int rl_player_tproxy_ret_options (rtsp_con_player_t * pplayer)
{
	if (!pplayer) return FALSE;
        pplayer->prtsp->status = 200;
	return rl_player_tproxy_ret_status_resp(pplayer, NULL, 0);
}
	
static int rl_player_tproxy_ret_describe(rtsp_con_player_t * pplayer)
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
	in.s_addr=pplayer->own_ip_addr; //rtsp.pns->if_addr;

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
		rl_player_tproxy_ret_status_resp(pplayer, outbuf, bytesused + sdplen);
	}
	else {
		rl_player_tproxy_ret_status_resp(pplayer, outbuf, bytesused);
		rl_player_sendout_resp(pplayer, sdp, sdplen);
	}

	return TRUE;
}

static int rl_player_tproxy_ret_setup(rtsp_con_player_t * pplayer)
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

		/* If it is internal player, no need to open RTP/RTCP ports
		 */
		if (!CHECK_PLAYER_FLAG(pplayer, PLAYERF_INTERNAL)) {
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
				pplayer->rtp[trackID].udp_fd = rl_get_udp_sockfd(
						htons(rtp_udp_port), 0, 0, &ifAddr);
				if(pplayer->rtp[trackID].udp_fd != -1) break;
			}
			pplayer->rtp[trackID].own_udp_port = rtp_udp_port;
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

			rtcp_udp_port = rtp_udp_port + 1;
			while(1) {
				pplayer->rtcp[trackID].udp_fd = rl_get_udp_sockfd(
						htons(rtcp_udp_port), 0, 0, &ifAddr);
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
		}
		else {
			pthread_mutex_lock(&rtp_port_mutex);
			rtp_udp_port += 2;
			if (rtp_udp_port < 1024)
				rtp_udp_port = 1026;
			pplayer->rtp[trackID].udp_fd = -1;
			pplayer->rtp[trackID].own_udp_port = rtp_udp_port;

			rtcp_udp_port = rtp_udp_port + 1;
			pplayer->rtcp[trackID].udp_fd = -1;
			pplayer->rtcp[trackID].own_udp_port = rtcp_udp_port;
			pthread_mutex_unlock(&rtp_port_mutex);
		}
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
			"Cache-Control: no-cache\r\n\r\n" );

	SET_PLAYER_FLAG(pplayer, PLAYERF_SETUP_DONE);

        pplayer->prtsp->status = 200;
	return rl_player_tproxy_ret_status_resp(pplayer, outbuf, bytesused);
}

static int rl_player_tproxy_ret_pause(rtsp_con_player_t * pplayer)
{
	if (!pplayer) return FALSE;
	pplayer->pause = TRUE;
        pplayer->prtsp->status = 200;
	return rl_player_tproxy_ret_status_resp(pplayer, NULL, 0);
}

static int rl_player_tproxy_ret_play(rtsp_con_player_t * pplayer)
{
	rtsp_con_ps_t * pps;
        struct in_addr in;
	int bytesused;
	char outbuf[1024];
	int outbufsz = 1024;
	int i;
	char start[16], end[16];

	if (!pplayer) return FALSE;

	pps = pplayer->pps;
	in.s_addr = pplayer->prtsp->pns->if_addrv4;

	/*
	 * Buildup the response header
	 */
	bytesused = 0;

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

	if (RTSP_IS_FLAG_SET(pps->rtsp.flag, RTSP_HDR_SCALE)) {
		bytesused += snprintf(&outbuf[bytesused], outbufsz-bytesused,
					"Scale: %.3f\r\n",
					pps->rtsp.scale);
	}

	// Build up the Content-Base
	if (RTSP_IS_FLAG_SET(pps->rtsp.flag, RTSP_HDR_RTP_INFO)) {
		bytesused += snprintf(&outbuf[bytesused], 
				outbufsz-bytesused, "RTP-Info: ");
		for (i = 0; i < pps->sdp_info.num_trackID; i++) {
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
	rl_player_tproxy_ret_status_resp(pplayer, outbuf, bytesused);
	pplayer->pause = FALSE;
	return TRUE;
}

static int rl_player_tproxy_ret_get_parameter(rtsp_con_player_t * pplayer)
{
	rtsp_con_ps_t * pps;
	int bytesused;
	char outbuf[RTSP_BUFFER_SIZE + 4];
	int outbufsz = RTSP_BUFFER_SIZE;
	
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
			return rl_player_tproxy_ret_status_resp(pplayer, 
					outbuf, 
					bytesused + pps->rtsp.cb_resp_content_len);
		}
		else {
			rl_player_tproxy_ret_status_resp(pplayer, outbuf, bytesused);
			return rl_player_sendout_resp(pplayer, 
					pps->rtsp.cb_respbuf + pps->rtsp.cb_respoffsetlen, 
					pps->rtsp.cb_resp_content_len);
		}
	}
	return rl_player_tproxy_ret_status_resp(pplayer, NULL, 0);
}

static int rl_player_tproxy_ret_set_parameter(rtsp_con_player_t * pplayer)
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
			return rl_player_tproxy_ret_status_resp(pplayer, 
					outbuf, 
					bytesused + pps->rtsp.cb_resp_content_len);
		}
		else {
			rl_player_tproxy_ret_status_resp(pplayer, outbuf, bytesused);
			return rl_player_sendout_resp(pplayer, 
					pps->rtsp.cb_respbuf + pps->rtsp.cb_respoffsetlen, 
					pps->rtsp.cb_resp_content_len);
		}
	}
	return rl_player_tproxy_ret_status_resp(pplayer, NULL, 0);
}

static int rl_player_tproxy_ret_teardown(rtsp_con_player_t * pplayer)
{
	if (!pplayer) return FALSE;
        pplayer->prtsp->status = 200;
	rl_player_tproxy_ret_status_resp(pplayer, NULL, 0);
	return TRUE;
}

/* Functions for live relay with stream splitting
 */
static int rl_player_live_ret_status_resp(rtsp_con_player_t * pplayer, char * hdr, int hdr_len)
{
#define OUTBUF(_data, _datalen) { \
	assert(((_datalen)+pplayer->prtsp->cb_resplen) < RTSP_BUFFER_SIZE); \
	memcpy((char *)&pplayer->prtsp->cb_respbuf[pplayer->prtsp->cb_resplen], \
		(_data), (_datalen)); \
	pplayer->prtsp->cb_resplen += (_datalen); \
}

	rtsp_con_ps_t *pps;
	//const char *name;
	//int namelen;
	//const char *data;
	//int datalen;
	//u_int32_t attrs;
	//int hdrcnt;
	//int nth;
	//int token;
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
			pplayer->session && 
			(pplayer->prtsp->status == 200)) {
		rv = snprintf(strbuf, sizeof(strbuf), 
			"Session: %llu;timeout=%d\r\n",
			pplayer->session, 
			rtsp_player_idle_timeout); 
		OUTBUF(strbuf, rv);
	}

	/* Add Server Header */
	rv = snprintf(strbuf, sizeof(strbuf), 
			"Server: %s\r\n",
			rl_rtsp_server_str);
	OUTBUF(strbuf, rv);

	/* Add Date Header */
	rv = snprintf(strbuf, sizeof(strbuf), 
			"Date: %s\r\n",
			nkn_get_datestr(NULL));
	OUTBUF(strbuf, rv);

	if( pplayer->player_type != RL_PLAYER_TYPE_FAKE_PLAYER ) {
	    switch(pplayer->prtsp->status) {
		case 403:
		    rv = snprintf(strbuf, sizeof(strbuf), 
				  "%s\r\n",
				  "Connection: close");
		    OUTBUF(strbuf, rv);
		    AO_fetch_and_add1(&glob_rtsp_tot_status_resp_403);
		    break;
		case 200:
		    AO_fetch_and_add1(&glob_rtsp_tot_status_resp_200);
		    break;
		case 461:
		    AO_fetch_and_add1(&glob_rtsp_tot_status_resp_461);
		    break;
		case 462:
		    AO_fetch_and_add1(&glob_rtsp_tot_status_resp_462);
		    break;
		case 454:
		    AO_fetch_and_add1(&glob_rtsp_tot_status_resp_454);
		    break;
		case 457:
		    AO_fetch_and_add1(&glob_rtsp_tot_status_resp_457);
		    break;
		case 413:
		    AO_fetch_and_add1(&glob_rtsp_tot_status_resp_413);
		    break;
		case 404:
		    AO_fetch_and_add1(&glob_rtsp_tot_status_resp_404);
		    break;
		case 455:
		    AO_fetch_and_add1(&glob_rtsp_tot_status_resp_455);
		    break;
		case 503:
		    AO_fetch_and_add1(&glob_rtsp_tot_status_resp_503);
		    break;
	    }
	}
#if 0
	if (pps) {
	   // Add known headers
	   for (token = 0; token < RTSP_HDR_MAX_DEFS; token++) {
		// Don't forward the following headers
		switch(token) {
		case RTSP_HDR_CSEQ:
		case RTSP_HDR_RANGE: // Don't forward Range
		case RTSP_HDR_RTP_INFO:
		case RTSP_HDR_TRANSPORT:
		case RTSP_HDR_SESSION:
		case RTSP_HDR_DATE:
		case RTSP_HDR_SERVER:
		case RTSP_HDR_CONTENT_LENGTH:
		case RTSP_HDR_CONTENT_BASE:
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

		} else {
	    		continue;
		}
#if 0
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

#if 0
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
#endif
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


static int rl_player_live_ret_options (rtsp_con_player_t * pplayer)
{
	char buf[256];
	int len;
	
	if (!pplayer) return FALSE;
	if (pplayer->pps->optionsString) {
		pplayer->prtsp->status = 200;
		len = snprintf( buf, 255, "%s\r\n\r\n", pplayer->pps->optionsString );
		return rl_player_live_ret_status_resp(pplayer, buf, len);
	}
	else {
		return rl_player_ret_our_options(pplayer);
	}
}
	
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
	
	//Since this function could be called from either the publisher or the player we
	//need to ensure that we check the state before sending a describe.
	//if (pplayer->method >= METHOD_SENT_DESCRIBE_RESP) return TRUE;

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

		if (!CHECK_PLAYER_FLAG(pplayer, PLAYERF_INTERNAL)) {
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
				pplayer->rtp[trackID].udp_fd = rl_get_udp_sockfd(
						htons(rtp_udp_port), 0, 0, &ifAddr);
				if(pplayer->rtp[trackID].udp_fd != -1) break;
			}
			pplayer->rtp[trackID].own_udp_port = rtp_udp_port;
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
			rtcp_udp_port = rtp_udp_port + 1;
			while(1) {
				pplayer->rtcp[trackID].udp_fd = rl_get_udp_sockfd(
						htons(rtcp_udp_port), 0, 0, &ifAddr);
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

			DBG_LOG(MSG, MOD_RTSP, "Rtsp fd=%d, rtp fd=%d, rtcp fd=%d",
				pplayer->rtsp_fd,
				pplayer->rtp[trackID].udp_fd,
				pplayer->rtcp[trackID].udp_fd );

		/*	if (pplayer->rtcp[trackID].udp_fd >= g_rl_maxfd) {
				g_rl_maxfd = pplayer->rtcp[trackID].udp_fd + 1;
			}*/
		}
		else
		{
			pthread_mutex_lock(&rtp_port_mutex);
			rtp_udp_port += 2;
			if (rtp_udp_port < 1024)
				rtp_udp_port = 1026;
			pplayer->rtp[trackID].udp_fd = -1;
			pplayer->rtp[trackID].own_udp_port = rtp_udp_port;

			rtcp_udp_port = rtp_udp_port + 1;
			pplayer->rtcp[trackID].udp_fd = -1;
			pplayer->rtcp[trackID].own_udp_port = rtcp_udp_port;
			pthread_mutex_unlock(&rtp_port_mutex);
		}
	} else if (pplayer->prtsp->streamingMode == RTP_TCP){
		if (pplayer->pps->rtsp.streamingMode == RTP_TCP) {
			/* Server mode is also TCP, use server's channel ID's.
			 */
			pplayer->rtp[trackID].own_channel_id = pps->rtp[trackID].own_channel_id;
			pplayer->rtcp[trackID].own_channel_id = pps->rtcp[trackID].own_channel_id;
			if( pps->pplayer_list->player_type == RL_PLAYER_TYPE_FAKE_PLAYER ) {
				pplayer->send_data = orp_send_data_to_tfm_tcp;
			}
			else {
				pplayer->send_data = rl_player_sendout_inline_data;
			}
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
			if( pps->pplayer_list->player_type == RL_PLAYER_TYPE_FAKE_PLAYER ) {
				pplayer->send_data = orp_send_data_to_tfm_udp;
			}
			else {
				pplayer->send_data = rl_player_sendout_udp_as_inline_data;
			}
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

static int rl_player_live_ret_pause(rtsp_con_player_t * pplayer)
{
	if (!pplayer) return FALSE;
	pplayer->pause = TRUE;
        pplayer->prtsp->status = 200;
	return rl_player_live_ret_status_resp(pplayer, NULL, 0);
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
	in.s_addr = pplayer->prtsp->pns->if_addrv4;

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

static int rl_player_live_ret_get_parameter(rtsp_con_player_t * pplayer)
{
	rtsp_con_ps_t * pps;
	int bytesused;
	char outbuf[RTSP_BUFFER_SIZE + 4];
	int outbufsz = RTSP_BUFFER_SIZE;
	int ret;
	
	if (!pplayer) return FALSE;
	pps = pplayer->pps;

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
			ret = rl_player_live_ret_status_resp(pplayer, 
					outbuf, 
					bytesused + pps->rtsp.cb_resp_content_len);
		}
		else {
			rl_player_live_ret_status_resp(pplayer, outbuf, bytesused);
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
	return rl_player_live_ret_status_resp(pplayer, NULL, 0);
}

static int rl_player_live_ret_set_parameter(rtsp_con_player_t * pplayer)
{
	if (!pplayer) return FALSE;
	pplayer->prtsp->status = 200;
	return rl_player_live_ret_status_resp(pplayer, NULL, 0);
}

static int rl_player_live_ret_teardown(rtsp_con_player_t * pplayer)
{
	if (!pplayer) return FALSE;
	pplayer->prtsp->status = 200;
	rl_player_live_ret_status_resp(pplayer, NULL, 0);
	DBG_LOG(MSG2, MOD_RTSP, "Close player %p", pplayer);
	return TRUE;
}

static int rl_player_live_set_callbacks( rtsp_con_player_t * pplayer )
{
	pplayer->ret_options	= rl_player_live_ret_options;
	pplayer->ret_describe	= rl_player_live_ret_describe;
	pplayer->ret_setup	= rl_player_live_ret_setup;
	pplayer->ret_play	= rl_player_live_ret_play;
	pplayer->ret_pause	= rl_player_live_ret_pause;
	pplayer->ret_teardown	= rl_player_live_ret_teardown;
	pplayer->ret_get_parameter = rl_player_live_ret_get_parameter;
	pplayer->ret_set_parameter = rl_player_live_ret_set_parameter;
	return TRUE;
}

static int rl_player_teardown(rtsp_con_player_t * pplayer, int error, int locks_held)
{
	struct network_mgr * pnm_ps = NULL;
	int locks = locks_held;

	if (pplayer->pps && !CHECK_LOCK(locks_held, LK_PS_GNM)) {
		pnm_ps = &gnm[pplayer->pps->rtsp_fd];
	}

	if (!pplayer) return TRUE;

	SET_PLAYER_FLAG(pplayer, PLAYERF_TEARDOWN);
	if (!pplayer->pps || CHECK_PPS_FLAG(pplayer->pps, PPSF_CLOSE)) {
		DBG_LOG(MSG2, MOD_RTSP, "Close player %p", pplayer);
		rl_player_closed(pplayer, locks);
		return TRUE;
	}

	if (CHECK_PLAYER_FLAG(pplayer, PLAYERF_FOUND_LIVE)) {
		if (pplayer->pps->pplayer_list && !pplayer->pps->pplayer_list->next) {
			/* Send Teardown to origin and response to
			 * Player. Do not wait for origin response.
			 */
			if (error)
				SET_PPS_FLAG(pplayer->pps, PPSF_OWN_REQUEST);
			if (pnm_ps) pthread_mutex_lock(&pnm_ps->mutex);
			pplayer->pps->send_teardown(pplayer->pps);
			if (pnm_ps) pthread_mutex_unlock(&pnm_ps->mutex);
		}
	}
	else {
		if (pnm_ps) {
			pthread_mutex_lock(&pnm_ps->mutex);
			SET_LOCK(locks, LK_PS_GNM);
		}
		if (CHECK_PPS_FLAG(pplayer->pps, PPSF_SETUP_DONE)) {
			/* Send Teardown to origin and response to
			 * Player. Do not wait for origin response.
			 */
			if (error)
				SET_PPS_FLAG(pplayer->pps, PPSF_OWN_REQUEST);
			pplayer->pps->send_teardown(pplayer->pps);
		}
		else {
			/* Need to close origin connecton.
			 * BZ 4441
			 */
			rl_ps_closed(pplayer->pps, pplayer, locks);
		}
		if (pnm_ps) {
			pthread_mutex_unlock(&pnm_ps->mutex);
			UNSET_LOCK(locks, LK_PS_GNM);
		}
	}

	if (error) {
		DBG_LOG(MSG2, MOD_RTSP, "Close player %p", pplayer);
		rl_player_closed(pplayer, locks);
	}
	else {
		pplayer->ret_teardown(pplayer);
		rl_player_closed(pplayer, locks);
	}
	return TRUE;
}

/* Return 	-1 - Session ID not found
 * 		-2 - Invalid Range 
 *		 0 - Success
 */
static int rl_player_validate_rtsp_request(rtsp_con_player_t *pplayer)
{

	/*! Bug 4466 : If any rtsp method is received without session header
	 * after first SETUP method , then error 454 is returned
	 */
	if (CHECK_PLAYER_FLAG(pplayer, PLAYERF_SETUP_DONE)) { 

		if (pplayer->prtsp->fOurSessionId != (unsigned long long) atoll_len(pplayer->prtsp->session, sizeof(pplayer->prtsp->session))) {
			pplayer->prtsp->status = 454;
			return -1;
		}
	}

	/*! Bug 4467 : If rangeStart is greater than total npt,
	 * then  457 is returned
	 */
	if ((pplayer->pps) && (!CHECK_PPS_FLAG(pplayer->pps, PPSF_LIVE_VIDEO))) {

		if (pplayer->pps->pplayer_list && 
				pplayer->pps->pplayer_list->prtsp ) {

		  if (RTSP_IS_FLAG_SET(pplayer->pps->pplayer_list->prtsp->flag, 
					  RTSP_HDR_RANGE) && 
		  	(pplayer->pps->pplayer_list->prtsp->rangeStart > 
			 (float)atof(pplayer->pps->sdp_info.npt_stop))) {
				pplayer->prtsp->status = 457;
				return -2;
			}
		}
	}
	return 0;
}

/* Return 	 2 - Forwared request
 *		 1 - Processed header
 *		 0 - Need more data
 *		-1 - Error
 */
int rl_player_process_rtsp_request(int sockfd, rtsp_con_player_t * pplayer, int locks_held)
{
	struct network_mgr * pnm_ps;
	int ret;
	rtsp_con_ps_t * pps;
	static int c_num = 0;
	int cnt;
	int ret_val = -1;
	int locks = locks_held;
	
	UNUSED_ARGUMENT(sockfd);

	if (!CHECK_LOCK(locks_held,LK_PL)) {
		pthread_mutex_lock(&pplayer->prtsp->mutex);
		SET_LOCK(locks,LK_PL);
	}
	/*
	 * Parse the RTSP request.
	 */
	ret = rtsp_parse_request(pplayer->prtsp);

	if (ret == RPS_NEED_MORE_DATA) {
		// Not enough data.
		/* Check if request buffer is full.
		 * BZ 4254
		 */
		if ((pplayer->prtsp->cb_reqoffsetlen + pplayer->prtsp->cb_reqlen) >= RTSP_REQ_BUFFER_SIZE) {
			pplayer->prtsp->status = 413;
			if (!CHECK_LOCK(locks_held,LK_PL)) pthread_mutex_unlock(&pplayer->prtsp->mutex);
			return -1;
		}
		if (!CHECK_LOCK(locks_held,LK_PL)) pthread_mutex_unlock(&pplayer->prtsp->mutex);
		return 0;
	} else if(ret == RPS_OK) {
		// Does nothing.
	} else {
		/* Error no/status is set in rtsp_parse_request
		 */
		if (!CHECK_LOCK(locks_held,LK_PL)) pthread_mutex_unlock(&pplayer->prtsp->mutex);
		return -1;
	}
		
	DBG_LOG(MSG, MOD_RTSP, "recv method = %d", pplayer->prtsp->method);

	if (rl_player_validate_rtsp_request(pplayer) != 0) {
		if (!CHECK_LOCK(locks_held,LK_PL)) pthread_mutex_unlock(&pplayer->prtsp->mutex);
		return -1;
	}
	/* next five lines should not be here
	 * needs to be done once only ?
	 */
	//time(&(pplayer->prtsp->start_ts));
	pplayer->prtsp->src_ip = pplayer->peer_ip_addr;
	pplayer->prtsp->dst_ip = pplayer->own_ip_addr;
	pplayer->prtsp->out_bytes = 0;
	pplayer->prtsp->in_bytes = pplayer->prtsp->rtsp_hdr_len;
#if 0
	if ( pplayer->prtsp->cb_reqoffsetlen > 0 ) {
		memcpy( pplayer->prtsp->cb_reqbuf, 
			pplayer->prtsp->cb_reqbuf + pplayer->prtsp->cb_reqoffsetlen, 
			pplayer->prtsp->cb_reqlen - pplayer->prtsp->cb_reqoffsetlen );
		pplayer->prtsp->cb_reqlen = pplayer->prtsp->cb_reqlen - pplayer->prtsp->cb_reqoffsetlen;
		pplayer->prtsp->cb_reqoffsetlen = 0;
	}
	else {
		pplayer->prtsp->cb_reqlen = 0;
		pplayer->prtsp->cb_reqoffsetlen = 0;
	}
#endif
	if ((pplayer->prtsp->method == METHOD_OPTIONS) ||
	    (pplayer->prtsp->method == METHOD_DESCRIBE)) { 
		if (!pplayer->pps || !CHECK_PLAYER_FLAG(pplayer, PLAYERF_CONNECTED) ) {
			++c_num;
			pthread_mutex_lock(&g_ps_list_check_mutex);
			RTSP_GET_ALLOC_ABS_URL(&pplayer->prtsp->hdr, pplayer->prtsp->urlPreSuffix);
			pps = rl_ps_list_check_uri(pplayer->prtsp->urlPreSuffix);
			if (pps) {
				pthread_mutex_unlock(&g_ps_list_check_mutex);
				cnt = 0;
				do {
					ret = pthread_mutex_trylock(&pps->mutex);
					if ( ret == 0) {
						pthread_mutex_unlock(&pps->mutex);
						break;
					}
					else if (ret == EBUSY){
						sleep(1);
						pps = rl_ps_list_check_uri(pplayer->prtsp->urlPreSuffix);
						if (!pps)
							break;
					}
					else {
						break;
					}
					cnt++;
				}
				while (ret != 0 && cnt < 10);
				pthread_mutex_lock(&g_ps_list_check_mutex);
			}
			ret = find_shared_live_session(pplayer);
			switch (ret) {
			case 1:
				/* 
				 * Found one session that we can share this pplayer.
				 * link the existing pps with this pplayer.
				 */
				SET_PLAYER_FLAG(pplayer, PLAYERF_FOUND_LIVE);
				SET_PLAYER_FLAG(pplayer, PLAYERF_CONNECTED);
				rl_player_live_set_callbacks(pplayer);
				break;
			
			case -1:
				if (pplayer->prtsp->method == METHOD_OPTIONS) {
					rl_player_ret_our_options(pplayer);
					pthread_mutex_unlock(&g_ps_list_check_mutex);
					if (!CHECK_LOCK(locks_held,LK_PL)) pthread_mutex_unlock(&pplayer->prtsp->mutex);
					return 1;
				}
				else {
					pplayer->prtsp->status = 404;
					pthread_mutex_unlock(&g_ps_list_check_mutex);
					if (!CHECK_LOCK(locks_held,LK_PL)) pthread_mutex_unlock(&pplayer->prtsp->mutex);
					return -1;
				}
			case -2:
				{
				char req_dest_ip_str[64];
				struct sockaddr_in dip;
				socklen_t dlen = sizeof(dip);
				/* Setup DestIP so that we can use it later */
				/* Grab the destination IP here - we dont know yet 
				 * what the origin server will turn out to be. 
				 */
				memset(&dip, 0, dlen);
				dip.sin_addr.s_addr = pplayer->own_ip_addr;
#if 0
				if ((sockfd!=-1) && // Excluding the internal RTSP player
				    (getsockopt(sockfd, SOL_IP, SO_ORIGINAL_DST, &dip, &dlen) == -1)) {
					/* Error!! Dont do anything for now. This will 
					 * fail when we try to lookup the origin
					 * from the namespace
					 */
				}

				/* If destination IP is our interface IP itself */
				if (dip.sin_addr.s_addr == pplayer->prtsp->pns->if_addr) {
					/* Log an error ?? */
				}
#endif // 0
				ret = snprintf(req_dest_ip_str, sizeof(req_dest_ip_str), "%s",
						inet_ntoa(dip.sin_addr));
				mime_hdr_add_known(&pplayer->prtsp->hdr, RTSP_HDR_X_NKN_REQ_REAL_DEST_IP,
						req_dest_ip_str, ret);

				ret = rl_player_connect_to_svr(pplayer);
				if (ret > 0) {
					//SET_PLAYER_FLAG(pplayer, PLAYERF_FOUND_LIVE);
					SET_PLAYER_FLAG(pplayer, PLAYERF_CONNECTED);
					/* release the mutex acquired by rl_player_connect_to_svr */
					pnm_ps = &gnm[pplayer->pps->rtsp_fd];
					pthread_mutex_unlock(&pnm_ps->mutex);
				}
				else {
					pplayer->prtsp->status = 404;
					pthread_mutex_unlock(&g_ps_list_check_mutex);
					if (!CHECK_LOCK(locks_held,LK_PL)) pthread_mutex_unlock(&pplayer->prtsp->mutex);
					return -1;
				}
				}
				break;
			default:
				assert(0);
			}
			pthread_mutex_unlock(&g_ps_list_check_mutex);
		}
	}

	SET_PLAYER_FLAG(pplayer, PLAYERF_REQUEST_PENDING);

	if (!CHECK_LOCK(locks_held,LK_PS_GNM) && pplayer->pps && !CHECK_PLAYER_FLAG(pplayer, PLAYERF_FOUND_LIVE))
		pnm_ps = &gnm[pplayer->pps->rtsp_fd];
	else
		pnm_ps = NULL;

#if 0
	if (pnm_ps) pthread_mutex_lock(&pnm_ps->mutex);
#else
	if (pnm_ps) {
		while (pthread_mutex_trylock(&pnm_ps->mutex) != 0) {
			/* Release player fd gnm lock and reacquire do that
			 * any other thread waiting for player fd lock could
			 * run and avoid dead lock.
			 */
			pthread_mutex_unlock(&gnm[pplayer->rtsp_fd].mutex);
			sleep(0);
			if (pplayer->rtsp_fd != -1) {
				pthread_mutex_lock(&gnm[pplayer->rtsp_fd].mutex);
			}
			else {
				if (!CHECK_LOCK(locks_held,LK_PL)) pthread_mutex_unlock(&pplayer->prtsp->mutex);
				return -1;
			}
		}
		SET_LOCK(locks, LK_PS_GNM);
	}
#endif

	switch(pplayer->prtsp->method) {
	case METHOD_OPTIONS:
		if (!pplayer->pps || !CHECK_PLAYER_FLAG(pplayer, PLAYERF_CONNECTED) ) {
			pplayer->prtsp->status = 200;
			rl_player_ret_our_options(pplayer);
			ret_val = 1;
			goto normal_return;
		}
		/* Check user agent and set flag for QuickTime player
		 */
		//pthread_mutex_lock(&pplayer->prtsp->mutex);
		if (RTSP_IS_FLAG_SET(pplayer->prtsp->flag, RTSP_HDR_USER_AGENT)) {
			unsigned int hdr_attr = 0;
			int hdr_cnt = 0;
			const char *hdr_str = NULL;
			int hdr_len = 0;

			if (0 == mime_hdr_get_known(&pplayer->prtsp->hdr, RTSP_HDR_USER_AGENT, 
					&hdr_str, &hdr_len, &hdr_attr, &hdr_cnt)) {
				if (strncasecmp(hdr_str, "QuickTime", strlen("QuickTime")) == 0) {
					SET_PLAYER_FLAG(pplayer, PLAYERF_PLAYER_QT);
				}
				else if (strncasecmp(hdr_str, "WMPlayer", strlen("WMPlayer")) == 0) {
					SET_PLAYER_FLAG(pplayer, PLAYERF_PLAYER_WMP);
					rl_player_wmp_set_callbacks( pplayer );
					rl_ps_wms_set_callbacks( pplayer->pps );
				}
			}
		}
		//pthread_mutex_unlock(&pplayer->prtsp->mutex);
		if (CHECK_PLAYER_FLAG(pplayer, PLAYERF_FOUND_LIVE) && 
				pplayer->pps->optionsString) {
			pplayer->ret_options(pplayer);
			ret_val = 1;
		}
		else {
			//if (pnm_ps) pthread_mutex_lock(&pnm_ps->mutex);
			if (pplayer->pps) {
				//pthread_mutex_lock(&pplayer->prtsp->mutex);
				pplayer->pps->send_options(pplayer->pps);
				//pthread_mutex_unlock(&pplayer->prtsp->mutex);
				ret_val = 2;
			}
			else {
				pplayer->prtsp->status = 503;
				goto error_return;
			}
		}
		break;

	case METHOD_DESCRIBE:
		if (!pplayer->pps || !CHECK_PLAYER_FLAG(pplayer, PLAYERF_CONNECTED) ) {
			pplayer->prtsp->status = 404;
			goto error_return;
		}
		/* Check user agent and set flag for QuickTime player
		 */
		//pthread_mutex_lock(&pplayer->prtsp->mutex);
		if (RTSP_IS_FLAG_SET(pplayer->prtsp->flag, RTSP_HDR_USER_AGENT)) {
			unsigned int hdr_attr = 0;
			int hdr_cnt = 0;
			const char *hdr_str = NULL;
			int hdr_len = 0;

			if (0 == mime_hdr_get_known(&pplayer->prtsp->hdr, RTSP_HDR_USER_AGENT, 
					&hdr_str, &hdr_len, &hdr_attr, &hdr_cnt)) {
				if (strncasecmp(hdr_str, "QuickTime", strlen("QuickTime")) == 0) {
					SET_PLAYER_FLAG(pplayer, PLAYERF_PLAYER_QT);
				}
				else if (strncasecmp(hdr_str, "WMPlayer", strlen("WMPlayer")) == 0) {
					SET_PLAYER_FLAG(pplayer, PLAYERF_PLAYER_WMP);
					rl_player_wmp_set_callbacks( pplayer );
					rl_ps_wms_set_callbacks( pplayer->pps );
				}
			}
		}
		//pthread_mutex_unlock(&pplayer->prtsp->mutex);
		if (CHECK_PLAYER_FLAG(pplayer, PLAYERF_FOUND_LIVE)) {
			/* Check if live streaming is enabled for this player.
			 * If not return 403 and close the connections.
			 */
			if (!CHECK_PLAYER_FLAG(pplayer, PLAYERF_LIVE_ENABLED)) {
				pplayer->prtsp->status = 403;
				goto error_return;
			}
			else {
				pplayer->ret_describe(pplayer);
				ret_val = 1;
			}
		}
		else {
			//if (pnm_ps) pthread_mutex_lock(&pnm_ps->mutex);
			//pthread_mutex_lock(&pplayer->prtsp->mutex);
			pplayer->pps->send_describe(pplayer->pps);
			//pthread_mutex_unlock(&pplayer->prtsp->mutex);
			ret_val = 2;
		}
		break;

	case METHOD_SETUP:
		if(!pplayer->pps || !CHECK_PLAYER_FLAG(pplayer, PLAYERF_CONNECTED)) {
			goto error_455;
		}
		if (CHECK_PLAYER_FLAG(pplayer, PLAYERF_FOUND_LIVE)) {
			ret = pplayer->ret_setup(pplayer);
			ret_val = (ret == 0) ? -1 : 1;
			break;
		}
		else {
			pplayer->pps->send_setup(pplayer->pps);
			ret_val = 2;
		}
		break;

	case METHOD_PAUSE:
		if(!pplayer->pps || !CHECK_PLAYER_FLAG(pplayer, PLAYERF_CONNECTED)) {
			goto error_455;
		}
		if (CHECK_PLAYER_FLAG(pplayer, PLAYERF_FOUND_LIVE)) {
			pplayer->ret_pause(pplayer);
			ret_val =  1;
		}
		else {
			pplayer->pps->send_pause(pplayer->pps);
			ret_val =  2;
		}
		break;

	case METHOD_PLAY:
		if(!pplayer->pps || !CHECK_PLAYER_FLAG(pplayer, PLAYERF_CONNECTED)) {
			goto error_455;
		}
		if (CHECK_PLAYER_FLAG(pplayer, PLAYERF_FOUND_LIVE)) {
			pplayer->ret_play(pplayer);
			ret_val =  1;
		}
		else {
			pplayer->pps->send_play(pplayer->pps);
			//pplayer->pause = FALSE;
			ret_val =  2;
		}
		break;

	case METHOD_GET_PARAMETER:
		if(!pplayer->pps || !CHECK_PLAYER_FLAG(pplayer, PLAYERF_CONNECTED)) {
			goto error_455;
		}
		if (CHECK_PLAYER_FLAG(pplayer, PLAYERF_FOUND_LIVE)) {
			pplayer->ret_get_parameter(pplayer);
			ret_val = 1;
		}
		else {
			pplayer->pps->send_get_parameter(pplayer->pps);
			ret_val = 2;
		}
		break;

	case METHOD_SET_PARAM:
		if(!pplayer->pps || !CHECK_PLAYER_FLAG(pplayer, PLAYERF_CONNECTED)) {
			goto error_455;
		}
		if (CHECK_PLAYER_FLAG(pplayer, PLAYERF_FOUND_LIVE)) {
			pplayer->ret_set_parameter(pplayer);
			ret_val = 1;
		}
		else {
			pplayer->pps->send_set_parameter(pplayer->pps);
			ret_val = 2;
		}
		break;

	case METHOD_TEARDOWN:
		if (!CHECK_LOCK(locks_held,LK_PL)) {
			pthread_mutex_unlock(&pplayer->prtsp->mutex);
			UNSET_LOCK(locks, LK_PL);
		}
		if (pnm_ps) {
			pthread_mutex_unlock(&pnm_ps->mutex);
			UNSET_LOCK(locks, LK_PS_GNM);
		}
		if(!pplayer->pps && CHECK_PLAYER_FLAG(pplayer, PLAYERF_SRVR_TIMEOUT)) {
			pplayer->ret_teardown(pplayer);
			rl_player_closed(pplayer, locks);
			return 1;
		}
		if(!pplayer->pps || !CHECK_PLAYER_FLAG(pplayer, PLAYERF_CONNECTED)) {
			/* Sanity Check */
			pplayer->prtsp->status = 455;
			return -1;
		}
		rl_player_teardown(pplayer, FALSE, locks);
		return 1;

	case METHOD_UNKNOWN:
	case METHOD_BAD:
	default:
                /* We should return 400 BAD Request here */
		pplayer->prtsp->status = 400;
		ret_val = -1;
        }

normal_return:
	if (!CHECK_LOCK(locks_held,LK_PL)) pthread_mutex_unlock(&pplayer->prtsp->mutex);
	if (pnm_ps) pthread_mutex_unlock(&pnm_ps->mutex);
	/* No need to call any function here */
	return ret_val;

error_455:
	pplayer->prtsp->status = 455;
error_return:
	if (!CHECK_LOCK(locks_held,LK_PL)) pthread_mutex_unlock(&pplayer->prtsp->mutex);
	if (pnm_ps) pthread_mutex_unlock(&pnm_ps->mutex);
	ret_val = -1;
	/* No need to call any function here */
	return ret_val;

}

static int rl_player_epollin(int sockfd, void * private_data);
static int rl_player_epollin(int sockfd, void * private_data)
{
	rtsp_con_player_t * pplayer;
	char * p;
	int len, ret;
	struct network_mgr * pnm_ps = NULL;
	int locks = LK_PL_GNM;

	pplayer = (rtsp_con_player_t *)private_data;
	if (!pplayer) {
		return TRUE;
	}
	if (pplayer->pps && pplayer->pps->rtsp_fd != -1) {
		pnm_ps = &gnm[pplayer->pps->rtsp_fd];
	}

	/* Check if last request was processed. Else wait for the request to be
	 * processed and do not read data from network buffer.
	 */
	if (CHECK_PLAYER_FLAG(pplayer, PLAYERF_REQUEST_PENDING)) {
		/* Disabling this check as this causes network thread to go 100%
		 */
		//return TRUE;
	}
	
	/* Read the data from socket.
	 */
	if ( pplayer->prtsp->cb_reqoffsetlen > 0 ) {
		memcpy( pplayer->prtsp->cb_reqbuf, 
			pplayer->prtsp->cb_reqbuf + pplayer->prtsp->cb_reqoffsetlen, 
			pplayer->prtsp->cb_reqlen );
		pplayer->prtsp->cb_reqoffsetlen = 0;
	}
	 
	p = &pplayer->prtsp->cb_reqbuf[pplayer->prtsp->cb_reqoffsetlen + pplayer->prtsp->cb_reqlen];
	len = RTSP_REQ_BUFFER_SIZE - pplayer->prtsp->cb_reqlen - pplayer->prtsp->cb_reqoffsetlen;
	ret = recv(sockfd, p, len, MSG_DONTWAIT);
	if (ret < 0) {
		DBG_LOG(MSG, MOD_RTSP, "recv(fd=%d) ret=%d errno=%d ", sockfd, ret, errno);
		//printf("recv(fd=%d) ret=%d errno=%d ", sockfd, ret, errno);
		if (ret==-1 && (errno==EAGAIN || errno == EBADF) ) return TRUE;
		/* On error do not send any response to player.
		 * BZ 4441
		 */
		if (pnm_ps) {
			if (pthread_mutex_trylock(&pnm_ps->mutex) != 0)
				return TRUE;
			SET_LOCK(locks, LK_PS_GNM);
		}
		rl_player_teardown(pplayer, TRUE, locks);
		if (pnm_ps) pthread_mutex_unlock(&pnm_ps->mutex);
		return TRUE;
	} 
	else if (ret == 0) {
		//printf("recv(fd=%d) ret=%d errno=%d ", sockfd, ret, errno);
		if (CHECK_PLAYER_FLAG(pplayer, PLAYERF_TEARDOWN))
			return TRUE;
		DBG_LOG(MSG, MOD_RTSP, "recv(fd=%d) ret=%d errno=%d ", sockfd, ret, errno);
		/* On error do not send any response to player.
		 * BZ 4441
		 */
		if (pnm_ps) {
			if (pthread_mutex_trylock(&pnm_ps->mutex) != 0)
				return TRUE;
			SET_LOCK(locks, LK_PS_GNM);
		}
		rl_player_teardown(pplayer, TRUE, locks);
		if (pnm_ps) pthread_mutex_unlock(&pnm_ps->mutex);
		return TRUE;
	}

	pplayer->prtsp->cb_reqlen += ret;
	pplayer->prtsp->cb_reqbuf[pplayer->prtsp->cb_reqoffsetlen + pplayer->prtsp->cb_reqlen] = 0;
	pplayer->prtsp->fd = sockfd;
	SET_PLAYER_FLAG(pplayer, PLAYERF_KEEP_ALIVE);
	/*
	 * Process the RTSP request.
	 * Handle pipe-lined requests. If some data is left after processing
	 * one request, copy it to offset 0 of buffer.
	 */
#if 0
	while (1) {
		ret = rl_player_process_rtsp_request(sockfd, pplayer);
		if (ret == 1) {	/* Processed one request */
#if 0			
			if ( pplayer->prtsp->cb_reqoffsetlen > 0 ) {
				memcpy( pplayer->prtsp->cb_reqbuf, 
					pplayer->prtsp->cb_reqbuf + pplayer->prtsp->cb_reqoffsetlen, 
					pplayer->prtsp->cb_reqlen - pplayer->prtsp->cb_reqoffsetlen );
				pplayer->prtsp->cb_reqlen = pplayer->prtsp->cb_reqlen - pplayer->prtsp->cb_reqoffsetlen;
				pplayer->prtsp->cb_reqoffsetlen = 0;
				return TRUE;
			}
			else {
				pplayer->prtsp->cb_reqlen = 0;
				pplayer->prtsp->cb_reqoffsetlen = 0;
				return TRUE;
			}
#else
			if ( pplayer->prtsp->cb_reqlen == 0 ) {
				return TRUE;
			}
#endif
		}
		else if (ret == -1) {	/* Error */
			break;
		} 
		else {	/* Need more data */
			return TRUE;
		}
	}
#endif

	ret = rl_player_process_rtsp_request(sockfd, pplayer, locks);
	if (ret != -1) {	/* Error */
		return TRUE;
	}

	// fall through

//close_connection:
	/* typically comes here when GET type methods are received */
	pplayer->prtsp->src_ip = pplayer->peer_ip_addr;
	pplayer->prtsp->dst_ip = pplayer->own_ip_addr;
	rl_player_live_ret_status_resp(pplayer, NULL, 0);

	DBG_LOG(MSG, MOD_RTSP, "Teardown Player fd=%d", sockfd);
	rl_player_teardown(pplayer, TRUE, locks);
	return TRUE;
}


static int rl_player_epollout(int sockfd, void * private_data);
static int rl_player_epollout(int sockfd, void * private_data)
{
	/* To be implemented */
	UNUSED_ARGUMENT(sockfd);
	rtsp_con_player_t *pplayer;
	char *p = NULL;
	int ret, len;
	uint32_t i;
	rls_track_t *pt;

	pplayer = (rtsp_con_player_t *)private_data;
	if (!pplayer) {
		return TRUE;
	}

	pt = &pplayer->rtp[0];
	NM_add_event_epollin(pplayer->rtsp_fd);
	for (i = 0; i < pt->num_resend_pkts; i++) { 
		p  = (char *) pt->resend_buf[i].iov_base;
		len = pt->resend_buf[i].iov_len;
		
		while (len) {
			ret = send(pplayer->rtsp_fd, p, len, 0);
			if (ret < 0) 
				break;
			glob_rtsp_tot_bytes_sent += ret;
			pplayer->prtsp->out_bytes += ret;
			len -=ret;
			p +=ret;
		}
		
	}

	pt->num_resend_pkts = 0;
	return TRUE;
}

static int rl_player_epollerr(int sockfd, void * private_data);
static int rl_player_epollerr(int sockfd, void * private_data)
{
	rtsp_con_player_t *pplayer;

	pplayer = (rtsp_con_player_t *)private_data;
	if (!pplayer) {
		return TRUE;
	}

	DBG_LOG(MSG, MOD_RTSP, "Teardown Player fd=%d", sockfd);

	rl_player_teardown(pplayer, TRUE, LK_PL_GNM);
	return TRUE;
}

static int rl_player_epollhup(int sockfd, void * private_data);
static int rl_player_epollhup(int sockfd, void * private_data)
{
	DBG_LOG(MSG, MOD_RTSP, "fd=%d", sockfd);
	return rl_player_epollerr(sockfd, private_data);
}

static int rl_player_timeout(int sockfd, void * private_data, double timeout);
static int rl_player_timeout(int sockfd, void * private_data, double timeout)
{
	rtsp_con_player_t *pplayer;
	struct network_mgr * pnm_ps = NULL;
	int locks = LK_PL_GNM;
	
	UNUSED_ARGUMENT(sockfd);
	UNUSED_ARGUMENT(timeout);
	
	pplayer = (rtsp_con_player_t *)private_data;
	if (!pplayer) {
		return FALSE;
	}
	if (pplayer->pps && pplayer->pps->rtsp_fd != -1) {
		pnm_ps = &gnm[pplayer->pps->rtsp_fd];
	}

	/* bug 4284: if an empty telnet ssession or connect in port 554 timesout
	 * we will not have a pps conext. we just need to close the pplayer context
	 */
	if(!pplayer->pps || CHECK_PLAYER_FLAG(pplayer, PLAYERF_CLOSE)) {
		DBG_LOG(MSG, MOD_RTSP, "Close Player fd=%d", pplayer->rtsp_fd);
		if (pnm_ps) {
			pthread_mutex_lock(&pnm_ps->mutex);
			SET_LOCK(locks, LK_PS_GNM);
		}
		rl_player_closed(pplayer, locks);
		if (pnm_ps) pthread_mutex_unlock(&pnm_ps->mutex);
		return FALSE;
	}

	if (!CHECK_PLAYER_FLAG(pplayer, PLAYERF_KEEP_ALIVE)) {
		DBG_LOG(MSG, MOD_RTSP, "Teardown Player fd=%d", pplayer->rtsp_fd);
		if (pnm_ps) {
			if (pthread_mutex_trylock(&pnm_ps->mutex) != 0)
				return FALSE;
			SET_LOCK(locks, LK_PS_GNM);
		}
		rl_player_teardown(pplayer, TRUE, locks);
		if (pnm_ps) pthread_mutex_unlock(&pnm_ps->mutex);
		return FALSE;

	}
	UNSET_PLAYER_FLAG(pplayer, PLAYERF_KEEP_ALIVE);
	return FALSE;
}

/*
 * Initialize the pplayer structure
 */
rtsp_con_player_t * rtsp_new_pplayer(nkn_interface_t * pns, unsigned long long fOurSessionId);
rtsp_con_player_t * rtsp_new_pplayer(nkn_interface_t * pns, unsigned long long fOurSessionId)
{
	
	rtsp_con_player_t * pplayer;
	int i, j;

	/*
	 * Allocate memory
	 */
	pplayer = (rtsp_con_player_t *)nkn_malloc_type(sizeof(rtsp_con_player_t), mod_rtsp_type_1);
	if(!pplayer) { return NULL; }
	memset(pplayer, 0, sizeof(rtsp_con_player_t));

	if (pns) {
		pns->tot_sessions++;
		AO_fetch_and_add1(&glob_cur_open_rtsp_socket);
	}

	/*
	 * Fill up the data in pplayer
	 */
 	if ( rl_init_bw_smoothing(&pplayer->rl_player_bws_ctx) < 0 ) {
		free(pplayer);
        	return NULL;
	}
	if ( pns && rl_alloc_sess_bw(pns, sess_bandwidth_limit) < 0 ) {
		//Need to log this event
		DBG_LOG(WARNING, MOD_RTSP, 
			"Connection rejected due to insufficient bandwidth");
		glob_rtsp_alloc_sess_bw_err ++;
		free(pplayer);
        	return NULL;
	}

	pplayer->prtsp = (rtsp_cb_t *)nkn_malloc_type(sizeof(rtsp_cb_t), mod_rtsp_type_2);
	if (pplayer->prtsp == NULL) {
		free(pplayer);
		return NULL;
	}
	memset(pplayer->prtsp, 0, sizeof(rtsp_cb_t));
	pplayer->max_allowed_bw = sess_bandwidth_limit;
	pplayer->magic = RLMAGIC_ALIVE;
	pplayer->pps = NULL;
	pplayer->pause = TRUE;
	pplayer->ssrc = g_ssrc++;
	pplayer->session = fOurSessionId;
	pplayer->rtsp_fd = -1;
	pplayer->prtsp->fOurSessionId = pplayer->session;
	pplayer->prtsp->fd = -1;
	pplayer->ret_options 	   = rl_player_live_ret_options;
	pplayer->ret_describe 	   = rl_player_live_ret_describe;
	pplayer->ret_setup	   = rl_player_live_ret_setup;
	pplayer->ret_play	   = rl_player_live_ret_play;
	pplayer->ret_pause	   = rl_player_live_ret_pause;
	pplayer->ret_teardown	   = rl_player_live_ret_teardown;
	pplayer->ret_get_parameter = rl_player_live_ret_get_parameter;
	pplayer->ret_set_parameter = rl_player_live_ret_set_parameter;
	pthread_mutex_init(&pplayer->prtsp->mutex, NULL);

	for(i=0; i<MAX_TRACK; i++) {
		pplayer->rtp[i].udp_fd = -1;

		/* allocate memory for the queued packets */
		for(j = 0; j < MAX_QUEUED_PKTS; j++) {
#if 0
			char *p;

			/*
			 * BUG: we need to check return value of nkn_malloc_type
			 */
			p = (char*)nkn_malloc_type(RTP_MTU_SIZE, mod_rtsp_type_3);
			pplayer->rtp[i].resend_buf[j].iov_base = p; 
			pplayer->rtp[i].resend_buf[j].iov_len = (p == NULL) ? 0 : RTP_MTU_SIZE;
			p = (char*)nkn_malloc_type(RTP_MTU_SIZE, mod_rtsp_type_4);
			pplayer->rtcp[i].resend_buf[j].iov_base = p;
			pplayer->rtcp[i].resend_buf[j].iov_len = (p == NULL) ? 0 : RTP_MTU_SIZE;
#else
			/* Allcoate memory when required. Initialize to NULL. */
			pplayer->rtp[i].resend_buf[j].iov_base = NULL; 
			pplayer->rtp[i].resend_buf[j].iov_len  = 0;
			pplayer->rtcp[i].resend_buf[j].iov_base = NULL;
			pplayer->rtcp[i].resend_buf[j].iov_len  = 0;
#endif
		}
		pplayer->rtcp[i].udp_fd = -1;
	}

	return pplayer;
}

int rtsp_make_relay(int fd, uint32_t src_ip, uint32_t dst_ip,
			nkn_interface_t * pns, 
			char * reqbuf, int reqlen, unsigned long long fOurSessionId,
			char * abs_uri, char *cache_uri,
			namespace_token_t nstoken,
			char *vfs_filename);
int rtsp_make_relay(int fd, uint32_t src_ip, uint32_t dst_ip,
			nkn_interface_t * pns, 
			char * reqbuf, int reqlen, unsigned long long fOurSessionId,
			char * abs_uri, char *cache_uri,
			namespace_token_t nstoken,
			char *vfs_filename)
{
	rtsp_con_player_t * pplayer;
	int err = 0;

	pplayer = rtsp_new_pplayer(pns, fOurSessionId);
	if (!pplayer) {
		err = 1;
		goto error;
	}

	DBG_LOG(MSG, MOD_RTSP, "New rtsp for URI=%s, fd=%d", 
		abs_uri, fd);

	
	pplayer->prtsp->pns = pns;
	pplayer->rtsp_fd = fd;
	pplayer->peer_ip_addr = src_ip;
	pplayer->own_ip_addr = dst_ip;
	gettimeofday(&(pplayer->prtsp->start_ts), NULL);
	pplayer->player_type = RL_PLAYER_TYPE_RELAY;
	SET_PLAYER_FLAG(pplayer, PLAYERF_KEEP_ALIVE);

	/* Set default call back function for sending data.
	 */
	pplayer->send_data = rl_player_sendout_udp_data;

	/*
	 * add into network manager
	 */
	//printf("Publishing Server fd %d\n" , cfd);
	if(register_NM_socket(pplayer->rtsp_fd,
		(void *)pplayer,
		rl_player_epollin,
		rl_player_epollout,
		rl_player_epollerr,
		rl_player_epollhup,
		NULL,
		rl_player_timeout,
		rtsp_player_idle_timeout/2,
		USE_LICENSE_FALSE,
		TRUE) == FALSE)
	{
		err = 2;
		goto error;
	}

	if (NM_add_event_epollin(pplayer->rtsp_fd) == FALSE) {
		err = 3;
		goto error;
	}

	memcpy(pplayer->prtsp->cb_reqbuf, reqbuf, reqlen);
	pplayer->prtsp->cb_reqlen = reqlen;
	pplayer->prtsp->cb_reqbuf[reqlen] = 0;
	pplayer->prtsp->fd = fd;
	pplayer->prtsp->nstoken = nstoken;

	DBG_LOG(MSG, MOD_RTSP, "RTSP relay started (player fd=%d)", fd);
	/*
	 * Process the RTSP request.
	 */
	if (rl_player_process_rtsp_request(fd, pplayer, LK_PL_GNM) != -1) {
		if (video_ankeena_fmt_enable && 
			(!CHECK_PLAYER_FLAG(pplayer, PLAYERF_FOUND_LIVE) ||
			(CHECK_PLAYER_FLAG(pplayer, PLAYERF_FOUND_LIVE) &&
			CHECK_PLAYER_FLAG(pplayer, PLAYERF_LIVE_CACHEABLE)))
			/*&& (dst_ip != pns->if_addr)*/
			&& !CHECK_PLAYER_FLAG(pplayer, PLAYERF_PLAYER_WMP)) {
			orp_add_new_video(pns,
				dst_ip,
                                ntohs(rtsp_server_port),
				abs_uri, cache_uri, nstoken, vfs_filename);
		}
		return TRUE;
	}
	// fall through

	//pthread_mutex_lock(&rlcon_mutex);
	/* typically comes here when GET type methods are received */
	pplayer->prtsp->src_ip = pplayer->peer_ip_addr;
	pplayer->prtsp->dst_ip = pplayer->own_ip_addr;
	rl_player_live_ret_status_resp(pplayer, NULL, 0);

	DBG_LOG(MSG, MOD_RTSP, "Teardown Player fd=%d", pplayer->rtsp_fd);
	rl_player_teardown(pplayer, TRUE, LK_PL_GNM);
	//pthread_mutex_unlock(&rlcon_mutex);
	return TRUE;

#if 0
	/* Establish origin server connection
	 */
	ret = find_shared_live_session(pplayer);
	switch(ret) {
	case 1:
		/* 
		 * Found one session that we can share this pplayer.
		 * link the existing pps with this pplayer.
		 */
		SET_PLAYER_FLAG(pplayer, PLAYERF_FOUND_LIVE);
		SET_PLAYER_FLAG(pplayer, PLAYERF_CONNECTED);
		break;
	case -1:
		goto error;
	case -2:
		{
		char req_dest_ip_str[64];
		struct sockaddr_in dip;
		socklen_t dlen = sizeof(dip);
		/* Setup DestIP so that we can use it later */
		/* Grab the destination IP here - we dont know yet 
		 * what the origin server will turn out to be. 
		 */
		memset(&dip, 0, dlen);
		if (getsockopt(pplayer->rtsp_fd, SOL_IP, SO_ORIGINAL_DST, &dip, &dlen) == -1) {
			/* Error!! Dont do anything for now. This will 
			 * fail when we try to lookup the origin
			 * from the namespace
			 */
		}

		/* If destination IP is our interface IP itself */
		if (dip.sin_addr.s_addr == pplayer->prtsp->pns->if_addr) {
			/* Log an error ?? */
		}
		ret = snprintf(req_dest_ip_str, sizeof(req_dest_ip_str), "%s",
				inet_ntoa(dip.sin_addr));
		mime_hdr_add_known(&pplayer->prtsp->hdr, RTSP_HDR_X_NKN_REQ_REAL_DEST_IP,
				req_dest_ip_str, ret);

		ret = rl_player_connect_to_svr(pplayer);
		if (ret != FALSE) {
			//SET_PLAYER_FLAG(pplayer, PLAYERF_FOUND_LIVE);
			SET_PLAYER_FLAG(pplayer, PLAYERF_CONNECTED);
			/* release the mutex acquired by rl_player_connect_to_svr */
			pnm = &gnm[pplayer->pps->rtsp_fd];
			pthread_mutex_unlock(&pnm->mutex);
		}
		else {
			goto error;
		}
		}
		break;
	default:
		assert(0);
	}

	switch(pplayer->prtsp->method) {
	case METHOD_OPTIONS:
		if (!pplayer->pps || !CHECK_PLAYER_FLAG(pplayer, PLAYERF_CONNECTED) ) {
			pplayer->prtsp->status = 200;
			rl_player_ret_our_options(pplayer);
			return TRUE;
		}
		if (CHECK_PLAYER_FLAG(pplayer, PLAYERF_FOUND_LIVE)) {
			pplayer->ret_options(pplayer);
		}
		else {
			pplayer->pps->send_options(pplayer->pps);
		}
		return TRUE;
	case METHOD_DESCRIBE:
		if (CHECK_PLAYER_FLAG(pplayer, PLAYERF_FOUND_LIVE)) {
			pplayer->ret_describe(pplayer);
		}
		else {
			pplayer->pps->send_describe(pplayer->pps);
		}
		return TRUE;
	default:
		/* should never happen
		 */
		assert(0);
	}
#endif // 0
	return TRUE;

error:
	if(pplayer) {
	    DBG_LOG(MSG, MOD_RTSP, "Close Player fd=%d err=%d", pplayer->rtsp_fd, err);
	    if (gnm[pplayer->rtsp_fd].fd == pplayer->rtsp_fd)
		rl_player_closed(pplayer, LK_PL_GNM);
	} else {
	    DBG_LOG(MSG, MOD_RTSP, "Close Player fd=%d err=%d", fd, err);
	}
	return FALSE;

}


void rl_player_close_task(rtsp_con_player_t *pplayer);
void rl_player_close_task(rtsp_con_player_t *pplayer)
{
	struct network_mgr * pnm_pl = NULL;
	int locks = 0;

	if (pplayer && pplayer->rtsp_fd != -1) {
		pnm_pl = &gnm[pplayer->rtsp_fd];
	}
	if (pnm_pl) { 
		pthread_mutex_lock( &pnm_pl->mutex );
		SET_LOCK(locks, LK_PL_GNM);
	}
	SET_PLAYER_FLAG(pplayer, PLAYERF_CLOSE_TASK);
	rl_player_closed(pplayer, locks);
	if (pnm_pl) pthread_mutex_unlock( &pnm_pl->mutex );
}

/*
 * This function should be called with rlcon_mutex lock held.
 * rl_player_closed will not free pps structure
 */
void rl_player_closed(rtsp_con_player_t * pplayer, int locks_held)
{
	int i, j;
	rtsp_con_ps_t * pps;
	rtsp_con_player_t * pplayer1;
	pthread_mutex_t *pmutex;
	int pl_locked = 0;
	int ps_locked = 0; 
	int locks = locks_held;

	if (!pplayer || pplayer->magic != RLMAGIC_ALIVE) return;

	if (pplayer->close_task && !CHECK_PLAYER_FLAG(pplayer, PLAYERF_CLOSE_TASK)) {
		return;
	}

	/* 
	 * Acquire both locks, if unable to acquire lock, release locks and
	 * schedule delayed task to close player. If already scheduled, let
	 * scheduled task close the player.
	 */
	if (!CHECK_LOCK(locks_held, LK_PL)) {
		if (pthread_mutex_trylock(&pplayer->prtsp->mutex) == 0) {
			pl_locked = 1;
			SET_LOCK(locks, LK_PL);
		}
	}
	pps = pplayer->pps;
	if (pps) {
		if (pthread_mutex_trylock(&pps->rtsp.mutex) == 0) {
			ps_locked = 1;
			SET_LOCK(locks, LK_PS);
		}
	}

	if (!CHECK_LOCK(locks, LK_PL) || (CHECK_LOCK(locks, LK_PL) && pps && !ps_locked)) {
		if (!pplayer->close_task || CHECK_PLAYER_FLAG(pplayer, PLAYERF_CLOSE_TASK)) {
			pplayer->close_task = nkn_rtscheduleDelayedTask( 1 * 1000000, 
					(TaskFunc*)rl_player_close_task, (void *)pplayer);
		}
		UNSET_PLAYER_FLAG(pplayer, PLAYERF_CLOSE_TASK);
		if (pl_locked) pthread_mutex_unlock(&pplayer->prtsp->mutex);
		if (ps_locked) pthread_mutex_unlock(&pps->rtsp.mutex);
		return;
	}

	UNSET_PLAYER_FLAG(pplayer, PLAYERF_CONNECTED);

	//if (pplayer->rtsp_fd != gnm[pplayer->rtsp_fd].fd)
	//	printf("Player close fd=%d gnm fd=%d\r\n", pplayer->rtsp_fd, gnm[pplayer->rtsp_fd].fd);
	if (pplayer->player_type == RL_PLAYER_TYPE_FAKE_PLAYER) {
		DBG_LOG(MSG, MOD_RTSP, "Internal Player Closed (pps fd=%d) Uri=%s",
				pps ? pps->rtsp_fd : -1, 
				pplayer->uri);
	}
	else {
		DBG_LOG(MSG, MOD_RTSP, "Player Closed (pps fd=%d player fd=%d)",
				pps ? pps->rtsp_fd : -1, 
				pplayer->rtsp_fd);
	}
		
	if (pplayer->rtsp_fd != -1) {
		AO_fetch_and_sub1(&glob_cur_open_rtsp_socket);
		if (pplayer->prtsp->pns) pplayer->prtsp->pns->tot_sessions--;
	}
	/* Close this pplayer */
	pplayer->pps = NULL;
	if (pplayer->rtsp_fd != -1) { 
		//network_mgr_t tmp_nm;
		//if (gnm[pplayer->rtsp_fd].fd_type == FREE_FD)
		//	printf("Player close fd=%d FREE_FD mg=%x\r\n", pplayer->rtsp_fd, pplayer->magic);
		// NOTE: unregister_NM_socket(fd, TRUE) deletes the fd from epoll loop.
		//if (pplayer->rtsp_fd != -1 && pplayer->rtsp_fd == gnm[pplayer->rtsp_fd].fd) {
		if (gnm[pplayer->rtsp_fd].fd != -1) {
			//tmp_nm = gnm[pplayer->rtsp_fd];
			unregister_NM_socket(pplayer->rtsp_fd, TRUE);
			//if (gnm[pplayer->rtsp_fd].fd_type == FREE_FD) {
			//	printf("Player close fd=%d FREE_FD mg=%x\r\n", pplayer->rtsp_fd, pplayer->magic);
			//}
			nkn_close_fd(pplayer->rtsp_fd, RTSP_OM_FD); 
		}
		pplayer->rtsp_fd = -1;
	}
	for (i=0; i<MAX_TRACK; i++) {
		if(pplayer->rtp[i].udp_fd != -1) { 
			pmutex = &gnm[pplayer->rtp[i].udp_fd].mutex;
			pthread_mutex_lock(pmutex);
			assert(gnm[pplayer->rtp[i].udp_fd].fd_type == RTSP_OM_UDP_FD);
			// NOTE: unregister_NM_socket(fd, TRUE) deletes the fd from epoll loop.
			unregister_NM_socket(pplayer->rtp[i].udp_fd, TRUE);
			if (pplayer->rtp[i].udp_fd != -1) 
				nkn_close_fd(pplayer->rtp[i].udp_fd, RTSP_OM_UDP_FD); 
			pplayer->rtp[i].udp_fd = -1;
			pthread_mutex_unlock(pmutex);
		}
		if(pplayer->rtcp[i].udp_fd != -1) { 
			pmutex = &gnm[pplayer->rtcp[i].udp_fd].mutex;
			pthread_mutex_lock(pmutex);
			assert(gnm[pplayer->rtcp[i].udp_fd].fd_type == RTSP_OM_UDP_FD);
			// NOTE: unregister_NM_socket(fd, TRUE) deletes the fd from epoll loop.
			unregister_NM_socket(pplayer->rtcp[i].udp_fd, TRUE);
			if(pplayer->rtcp[i].udp_fd != -1) 
				nkn_close_fd(pplayer->rtcp[i].udp_fd, RTSP_OM_UDP_FD); 
			pplayer->rtcp[i].udp_fd = -1;
			pthread_mutex_unlock(pmutex);
		}
		for(j = 0; j < MAX_QUEUED_PKTS; j++) {
			/* free the queued packet buffer */
			if (pplayer->rtp[i].resend_buf[j].iov_base)
				free(pplayer->rtp[i].resend_buf[j].iov_base);
			pplayer->rtp[i].resend_buf[j].iov_base = NULL;
			pplayer->rtp[i].resend_buf[j].iov_len = 0;
			if (pplayer->rtcp[i].resend_buf[j].iov_base)
				free(pplayer->rtcp[i].resend_buf[j].iov_base);
			pplayer->rtcp[i].resend_buf[j].iov_base = NULL;
			pplayer->rtcp[i].resend_buf[j].iov_len = 0;
		}

	}

	/* delete from pps list */
	if (pps) {
		pplayer1 = pps->pplayer_list;
		if(pplayer1 == pplayer) {
			pps->pplayer_list = pplayer->next;
		}
		else {
			while(pplayer1) {
				if(pplayer1->next == pplayer) {
					pplayer1->next = pplayer->next;
					break;
				}
				pplayer1 = pplayer1->next;
			}
		}
	}

	mime_hdr_shutdown(&pplayer->prtsp->hdr);
	rl_cleanup_bw_smoothing(pplayer->rl_player_bws_ctx);
	pplayer->rl_player_bws_ctx = NULL;
	rl_dealloc_sess_bw(pplayer->prtsp->pns, pplayer->max_allowed_bw);
	pplayer->magic = RLMAGIC_DEAD;
	pplayer->prtsp->magic = RLMAGIC_DEAD;

	/* Always unlock, as prtsp is going to be freed.
	 */
	pthread_mutex_unlock(&pplayer->prtsp->mutex);
	free(pplayer->prtsp);
	if (pplayer->uri) free(pplayer->uri);
	if (pplayer->out_buffer) free(pplayer->out_buffer);
	free(pplayer);

	if (ps_locked) pthread_mutex_unlock(&pps->rtsp.mutex);
	
	/* Do not close origin from inside player close
	 */
#if 0
	/* Close this publish server socket if this player is the last player*/
	if(close_pps && pps && pps->pplayer_list == NULL) {
		/* if no player connected any more */
		//rl_ps_send_method_teardown(pps);
		rl_ps_closed(pps, NULL);
	}
#endif
}


/* ==================================================================== */
/*
 * Define network callback functions for 
 * epollin/epollout/epollerr/epollhup 
 */
/* ==================================================================== */

int rl_ps_sendout_request(rtsp_con_ps_t * pps);
int rl_ps_sendout_request(rtsp_con_ps_t * pps)
{
	int ret;
	char * p;

	if (!pps || pps->rtsp_fd == -1) return FALSE;

	DBG_LOG(MSG, MOD_RTSP, "Request (fd=%d, len=%d, off=%d):\n<%s>", 
			pps->rtsp_fd,
			pps->rtsp.cb_reqlen,
			pps->rtsp.cb_reqoffsetlen,
			pps->rtsp.cb_reqbuf);

	/* Send the request packet out. */
	SET_PPS_FLAG(pps, PPSF_REQUEST_PENDING);
	//printf("MFD->PUB\n%s\n", &pps->rtsp.cb_reqbuf[pps->rtsp.cb_reqoffsetlen]);
	while(pps->rtsp.cb_reqlen > 0) {

		p = &pps->rtsp.cb_reqbuf[pps->rtsp.cb_reqoffsetlen];
		ret = send(pps->rtsp_fd, p, pps->rtsp.cb_reqlen, 0);
		if( ret==-1 ) {
			if(errno == EAGAIN) {
				NM_add_event_epollout(pps->rtsp_fd);
				return TRUE;
			}
			return FALSE;
		} 
		pps->rtsp.cb_reqlen -= ret;
		pps->rtsp.cb_reqoffsetlen += ret;
	}

	//printf("EOF: rlcon->offset=%ld\n", rlcon->offset);
	pps->rtsp.cb_reqoffsetlen = 0;
	if (pps->rtsp.cb_resplen == 0)
		pps->rtsp.cb_respoffsetlen = 0;
	return TRUE;
}
#if 0
static int rl_ps_sendout_resp(rtsp_con_ps_t *pps, char * p, int len);
static int rl_ps_sendout_resp(rtsp_con_ps_t *pps, char * p, int len)
{
	int ret, sendlen;

	if (pps == NULL) return FALSE;

	DBG_LOG(MSG, MOD_RTSP, "Response sent to server(fd=%d, len=%d):\n<%s>", 
			pps->rtsp_fd, len, p);

	sendlen = 0;
	while (len) {
		ret = send(pps->rtsp_fd, p, len, 0);
		if (ret == -1) {
			if (errno == EAGAIN) {

				/* BUG: to be supported */
				continue;
				NM_add_event_epollout(pps->rtsp_fd);
				return TRUE;
			}
			return FALSE;
		} 
		p += ret;
		len -= ret;
		sendlen += ret;
	}

	return TRUE;
}
#endif
#define OUTBUF(_data, _datalen) { \
	assert(((_datalen)+bytesused) < outbufsz); \
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

static char * rl_ps_tproxy_buildup_req_head(rtsp_con_ps_t * pps, char * add_hdr, int add_hdr_size)
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
#if 0
	/* Add configured headers
	 * 
	 * For internal genertated requests src_ip is not known.
	 * So do not send X-Forwarded-For header with 0.0.0.0 IP.
	 * BZ 3706
	 *
	 * If X-Forwarded-For: is present in request, append IP
	 * BZ 3691
	 */
	if (ocon->oscfg.nsc->http_config->origin.u.http.send_x_forward_header && 
			httpreqcon->src_ip) {
		char *src_ip_str = alloca(INET_ADDRSTRLEN+1);
		datalen = 0;
		if (x_forward_hdr_no >= 0) {
			get_nth_unknown_header(&httpreqcon->http.hdr, x_forward_hdr_no, 
				&name, &namelen, &data, &datalen, &attrs);
		}
		if (inet_ntop(AF_INET, &httpreqcon->src_ip, src_ip_str, 
			      INET_ADDRSTRLEN+1)) {
		    INC_OUTBUF((int)strlen(src_ip_str)+datalen+20);
		    OUTBUF("X-Forwarded-For:", 16);
		    if (datalen) {
			OUTBUF(data, datalen);
			OUTBUF(", ", 2);
		    }
		    OUTBUF(src_ip_str, (int)strlen(src_ip_str));
		    OUTBUF("\r\n", 2);
		}
	}

	for (nth = 0; 
	     nth < ocon->oscfg.nsc->http_config->origin.u.http.num_headers; 
	     nth++) {
		hd = &ocon->oscfg.nsc->http_config->origin.u.http.header[nth];
		if ((hd->action == ACT_ADD) && hd->name) {
		    INC_OUTBUF(hd->name_strlen+hd->value_strlen+3);
		    OUTBUF(hd->name, hd->name_strlen);
		    OUTBUF(":", 1);
		    if (hd->value) {
		    	OUTBUF(hd->value, hd->value_strlen);
		    }
		    OUTBUF("\r\n", 2);
		}
	}
#endif 

	OUTBUF("\r\n", 2);
	//OUTBUF("", 1);
	pps->rtsp.cb_reqlen = bytesused;
	outbuf[pps->rtsp.cb_reqlen] = 0;
	return outbuf;
}

static char * rl_ps_tproxy_send_method_options(rtsp_con_ps_t * pps)
{
	if (!pps) return NULL;
	pps->method = METHOD_OPTIONS;
	rl_ps_tproxy_buildup_req_head(pps, NULL, 0);
	rl_ps_sendout_request(pps);
	return NULL;
}

static char * rl_ps_tproxy_send_method_describe(rtsp_con_ps_t * pps)
{
	if (!pps) return NULL;
	pps->method = METHOD_DESCRIBE;
	rl_ps_tproxy_buildup_req_head(pps, NULL, 0);
	rl_ps_sendout_request(pps);
	return NULL;
}


static char * rl_ps_tproxy_send_method_setup(rtsp_con_ps_t * pps)
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

	rl_ps_tproxy_buildup_req_head(pps, outbuf, bytesused);
	rl_ps_sendout_request(pps);
	return NULL;
}

static char * rl_ps_tproxy_send_method_play(rtsp_con_ps_t * pps)
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
	rl_ps_tproxy_buildup_req_head(pps, outbuf, bytesused);
	rl_ps_sendout_request(pps);
	//pthread_cond_broadcast(&rlcon_play_cond);
	//pthread_mutex_unlock(&rlcon_mutex);
	return NULL;
}

static char *rl_ps_tproxy_send_method_pause(rtsp_con_ps_t * pps)
{
	if (!pps) return NULL;
	pps->method = METHOD_PAUSE;
	rl_ps_tproxy_buildup_req_head(pps, NULL, 0);
	rl_ps_sendout_request(pps);
	return NULL;
}

static char *rl_ps_tproxy_send_method_get_parameter(rtsp_con_ps_t * pps)
{
	if (!pps) return NULL;
	pps->method = METHOD_GET_PARAMETER;
	rl_ps_tproxy_buildup_req_head(pps, NULL, 0);
	rl_ps_sendout_request(pps);
	return NULL;
}

static char *rl_ps_tproxy_send_method_set_parameter(rtsp_con_ps_t * pps)
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
		rl_ps_tproxy_buildup_req_head(pps, outbuf, len);
		rl_ps_sendout_request(pps);
		 
		memcpy( &pps->rtsp.cb_reqbuf[0],
			&pps->pplayer_list->prtsp->cb_reqbuf[pps->rtsp.rtsp_hdr_len],
			content_length);
		pps->rtsp.cb_reqlen = content_length;
		pps->rtsp.cb_reqoffsetlen = 0;
		rl_ps_sendout_request(pps);
	}
	else {
		rl_ps_tproxy_buildup_req_head(pps, NULL, 0);
		rl_ps_sendout_request(pps);
	}
	return NULL;
}

static char *rl_ps_tproxy_send_method_teardown(rtsp_con_ps_t * pps)
{
	if (!pps || CHECK_PPS_FLAG(pps, PPSF_TEARDOWN)) return NULL;
	pps->method = METHOD_TEARDOWN;
	SET_PPS_FLAG(pps, PPSF_TEARDOWN);
	rl_ps_tproxy_buildup_req_head(pps, NULL, 0);
	rl_ps_sendout_request(pps);
	return NULL;
}


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
		DBG_LOG(MSG, MOD_RTSP, "No RTSP method found (pps fd=%d) (pl fd=%d)",
			pps->rtsp_fd, pplayer->rtsp_fd);
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
	DBG_LOG(MSG, MOD_RTSP, "Options (pps fd=%d)", pps->rtsp_fd);
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
	DBG_LOG(MSG, MOD_RTSP, "Describe (pps fd=%d)", pps->rtsp_fd);

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
	DBG_LOG(MSG, MOD_RTSP, "Setup (pps fd=%d)", pps->rtsp_fd);
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
		if (pps->transport == rtsp_transport_client) {
			pps->rtsp.streamingMode = pps->pplayer_list->prtsp->streamingMode;
		}
		else if (pps->transport == rtsp_transport_rtsp) {
			pps->rtsp.streamingMode = RTP_TCP;
		}
		else {
			pps->rtsp.streamingMode = RTP_UDP;
		}
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
		DBG_LOG(MSG, MOD_RTSP, "Rtsp fd=%d, rtp fd=%d, rtcp fd=%d",
			pps->rtsp_fd,
			pps->rtp[trackID].udp_fd,
			pps->rtcp[trackID].udp_fd );

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
	DBG_LOG(MSG, MOD_RTSP, "Play (pps fd=%d)", pps->rtsp_fd);

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
	DBG_LOG(MSG, MOD_RTSP, "Pause (pps fd=%d)", pps->rtsp_fd);
	rl_ps_live_buildup_req_head(pps, NULL, 0);
	rl_ps_sendout_request(pps);
	return NULL;
}

static char *rl_ps_live_send_method_get_parameter(rtsp_con_ps_t * pps)
{
	int len;
	int content_length;
	char outbuf[1024];
	int outbufsz = sizeof(outbuf);

	if (!pps) return NULL;
	pps->method = METHOD_GET_PARAMETER;
	DBG_LOG(MSG, MOD_RTSP, "Get Param (pps fd=%d)", pps->rtsp_fd);

	content_length = pps->pplayer_list->prtsp->cb_req_content_len;
	if (content_length) {
		len = snprintf(outbuf, outbufsz, "Content-Length: %d\r\n", content_length);
		rl_ps_live_buildup_req_head(pps, outbuf, len);
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
			pps->pplayer_list->prtsp->cb_reqoffsetlen += content_length;
		}
		pps->pplayer_list->prtsp->cb_req_content_len = 0;
	}
	else {
		len = snprintf(outbuf, outbufsz, "Content-Length: %d\r\n", content_length);
		rl_ps_live_buildup_req_head(pps, outbuf, len);
		rl_ps_sendout_request(pps);
	}
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

	DBG_LOG(MSG, MOD_RTSP, "Set Param (pps fd=%d)", pps->rtsp_fd);
	/*
	 * Send out SET_PARAMETER Body.
	 */
	content_length = pps->pplayer_list->prtsp->cb_req_content_len;
	if (content_length) {
		len = snprintf(outbuf, outbufsz, "Content-Length: %d\r\n", content_length);
		rl_ps_live_buildup_req_head(pps, outbuf, len);
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
			pps->pplayer_list->prtsp->cb_reqoffsetlen += content_length;
		}
		pps->pplayer_list->prtsp->cb_req_content_len = 0;
	}
	else {
		rl_ps_live_buildup_req_head(pps, NULL, 0);
		rl_ps_sendout_request(pps);
	}
	return NULL;
}

static char *rl_ps_live_send_method_teardown(rtsp_con_ps_t * pps)
{
	if (!pps || CHECK_PPS_FLAG(pps, PPSF_TEARDOWN)) return NULL;

	DBG_LOG(MSG, MOD_RTSP, "Teardown (pps fd=%d)",	pps->rtsp_fd);

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



static char * rl_ps_tproxy_recv_method_options(rtsp_con_ps_t * pps)
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

static char * rl_ps_tproxy_recv_method_describe(rtsp_con_ps_t * pps)
{
	char * psdpstart, * p;

	if (!pps) return NULL;
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

static char * rl_ps_tproxy_recv_method_setup(rtsp_con_ps_t * pps)
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

static char * rl_ps_tproxy_recv_method_play(rtsp_con_ps_t * pps)
{
	int i;

	if (!pps) return NULL;
	/* video streaming is playing now */
	SET_PPS_FLAG(pps, PPSF_VIDEO_PLAYING);
	for (i = 0; i < pps->sdp_info.num_trackID; i++) {
		pps->rtp_info_seq[i] = pps->rtsp.rtp_info_seq[i];
	}

	if (!pps->fkeep_alive_task) {
	    /* BZ:5118
	     * to ensure that our own heart beat does not stop us from timing out
	     * when we there is no activity on the RTP channels and we are supposed
	     * to time out. Here we are adjusting the time period of the keep alive
	     * task to be 90s if it is an internal player type
	     */
	    if(pps->pplayer_list) {
		if(pps->pplayer_list->player_type == RL_PLAYER_TYPE_FAKE_PLAYER) {
		    pps->fkeep_alive_task = nkn_scheduleDelayedTask( 90 * 1000000, 
			     (TaskFunc*)rtsplive_keepaliveTask, (void *)pps);
		} else {
		    pps->fkeep_alive_task = nkn_scheduleDelayedTask( 60 * 1000000, 
			     (TaskFunc*)rtsplive_keepaliveTask, (void *)pps);
		}
	    }
	//pps->fkeep_alive_task = nkn_scheduleDelayedTask( 60 * 1000000, 
	//	     (TaskFunc*)rtsplive_keepaliveTask, (void *)pps);
	}
	return NULL;
}

static char * rl_ps_tproxy_recv_method_pause(rtsp_con_ps_t * pps)
{
	UNUSED_ARGUMENT(pps);
	//rl_player_ret_teardown(pps->pplayer_list);
	//rl_ps_closed(pps);
	return NULL;
}

static char * rl_ps_tproxy_recv_method_get_parameter(rtsp_con_ps_t * pps)
{
	UNUSED_ARGUMENT(pps);
	//rl_player_ret_teardown(pps->pplayer_list);
	//rl_ps_closed(pps);
	return NULL;
}

static char * rl_ps_tproxy_recv_method_set_parameter(rtsp_con_ps_t * pps)
{
	UNUSED_ARGUMENT(pps);
	//rl_player_ret_teardown(pps->pplayer_list);
	//rl_ps_closed(pps);
	return NULL;
}

static char * rl_ps_tproxy_recv_method_teardown(rtsp_con_ps_t * pps)
{
	UNUSED_ARGUMENT(pps);
	//rl_player_ret_teardown(pps->pplayer_list);
	//rl_ps_closed(pps);
	return NULL;
}




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
	    /* BZ:5118
	     * to ensure that our own heart beat does not stop us from timing out
	     * when we there is no activity on the RTP channels and we are supposed
	     * to time out. Here we are adjusting the time period of the keep alive
	     * task to be 90s if it is an internal player type
	     */
	    if(pps->pplayer_list) {
		if(pps->pplayer_list->player_type == RL_PLAYER_TYPE_FAKE_PLAYER) {
		    pps->fkeep_alive_task = nkn_scheduleDelayedTask( 90 * 1000000, 
			     (TaskFunc*)rtsplive_keepaliveTask, (void *)pps);
		} else {
		    pps->fkeep_alive_task = nkn_scheduleDelayedTask( 60 * 1000000, 
			     (TaskFunc*)rtsplive_keepaliveTask, (void *)pps);
		}
	    }
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

static int rl_ps_epollin(int sockfd, void * private_data);
static int rl_ps_epollin(int sockfd, void * private_data)
{
	rtsp_con_ps_t * pps = (rtsp_con_ps_t *)private_data;
	char * p;
	int len, ret;
	struct network_mgr * pnm_pl = NULL;
	rtsp_con_player_t *pplayer, *pplayer_next;
	int trace_code = 0;
	int locks = LK_PS_GNM;
	
	if (!pps || (g_ps[pps->g_ps_index] != pps)) {
		return TRUE;
	}
	
	/*
	 * receive the data from socket.
	 */
	p = &pps->rtsp.cb_respbuf[pps->rtsp.cb_respoffsetlen + pps->rtsp.cb_resplen];
	len = RTSP_BUFFER_SIZE - pps->rtsp.cb_resplen - pps->rtsp.cb_respoffsetlen;
	ret = recv(sockfd, p, len, MSG_DONTWAIT);
	if(ret < 0) {
		DBG_LOG(MSG, MOD_RTSP, "recv(fd=%d) ret=%d errno=%d pub-server state", 
					sockfd, ret, errno);
		if(ret==-1 && errno==EAGAIN) return TRUE;
		rl_ps_closed(pps, NULL, locks);
		return TRUE;
	} 
	else if(ret == 0) {
		DBG_LOG(MSG, MOD_RTSP, "recv(fd=%d) ret=%d errno=%d pub-server state", 
					sockfd, ret, errno);
		rl_ps_closed(pps, NULL, locks);
		return TRUE;
	}

	pps->rtsp.cb_resplen += ret;
	pps->rtsp.cb_respbuf[pps->rtsp.cb_respoffsetlen + pps->rtsp.cb_resplen] = 0;

	/*
	 * Parse the received data
	 */
	if (CHECK_PPS_FLAG(pps, PPSF_RECV_INLINE_DATA)) {
		ret = RPS_NEED_MORE_DATA;
		while (pps->rtsp.cb_resplen > 4) {
			ret = rtsp_parse_rtp_data(&pps->rtsp);
			if (ret == RPS_NEED_MORE_DATA) {
				rl_ps_relay_rtp_data(pps);
			}
			else if (ret == RPS_RTSP_RESP_HEADER) {
				ret = rtsp_parse_response(&pps->rtsp);
				break;
			}
			else if (ret == RPS_RTSP_REQ_HEADER) {
				//ret = rtsp_parse_server_request(&pps->rtsp);
				break;
			}
		}
#if 0
#define UINT unsigned int
		if (pps->rtsp.cb_resplen) {
			char *p1;
			p1 = pps->rtsp.cb_respbuf + pps->rtsp.cb_respoffsetlen;
			printf( "RTP/TCP r    : %c    %4d %4d [%02x %02x %02x %02x]\n", 
				*p1,
				pps->rtsp.cb_resplen, pps->rtsp.cb_respoffsetlen,
				(UINT) *(p1), (UINT) *(p1+1), (UINT) *(p1+2), (UINT) *(p1+3));
		}
#endif
	}
	else if (CHECK_PPS_FLAG(pps, PPSF_RECV_CONTENT)) {
		/* Nothing to do, content received, may be
		 * SDP info.
		 */
		if (pps->rtsp.cb_resplen < pps->rtsp.cb_resp_content_len)
			return TRUE;
		UNSET_PPS_FLAG(pps, PPSF_RECV_CONTENT);
		ret = RPS_OK;
	}
	else {
		ret = rtsp_parse_response(&pps->rtsp);
	}
	

	switch (ret) {
	case RPS_NEED_MORE_DATA:
		return TRUE;
	case RPS_NEED_CONTENT:
		SET_PPS_FLAG(pps, PPSF_RECV_CONTENT);
		return TRUE;
	case RPS_OK:
		/* Follow through */
		break;
	case RPS_INLINE_DATA:
		rtsp_parse_rtp_data(&pps->rtsp);
		rl_ps_relay_rtp_data(pps);
		return TRUE;
	case RPS_ERROR:
	default:
		rl_ps_closed(pps, NULL, locks);
		return TRUE;
	}

	pthread_mutex_lock(&pps->rtsp.mutex);
	SET_LOCK(locks, LK_PS);
	if (pps->pplayer_list && pps->pplayer_list->prtsp->fd != -1) {
		pnm_pl = &gnm[pps->pplayer_list->prtsp->fd];
	}

#if 0
	if (pnm_pl) pthread_mutex_lock(&pnm_pl->mutex);
#else
	if (pnm_pl) {
		while (pthread_mutex_trylock(&pnm_pl->mutex) != 0) {
			/* Release pps fd gnm lock and reacquire do that
			 * any other thread waiting for pps fd lock could
			 * run and avoid dead lock.
			 */
			pthread_mutex_unlock(&gnm[pps->rtsp_fd].mutex);
			sleep(0);
			if (pps->rtsp_fd != -1) {
				pthread_mutex_lock(&gnm[pps->rtsp_fd].mutex);
			}
			else {
				pthread_mutex_unlock(&pps->rtsp.mutex);
				return TRUE;
			}
		}
		SET_LOCK(locks, LK_PL_GNM);
	}
#endif
	
	if (pps->rtsp.status != 200) {
		/* if we get an error status for GET_PARAMETER it is ok, 
		 * all we are interested is a ping here and we are not 
		 * using it for some explicit functionality. Error on any
		 * other method needs to be handled appropriately. BZ 3269
		 */
		/* Return status to player also
		 * BZ 4232
		 */
		/* If status is 451, Not supported for GET_PARAMETER and
		 * SET_PARAMETER, do not close the connection. Just return
		 * the status.
		 * BZ 4439
		 */
		if (pps->pplayer_list) {
			pps->pplayer_list->prtsp->status = pps->rtsp.status;
			rl_player_live_ret_status_resp(pps->pplayer_list, NULL, 0);
		}
		if (pps->method != METHOD_GET_PARAMETER &&
				pps->method != METHOD_SET_PARAM) {
			trace_code = 1;
			goto close_ps;
		}
		trace_code = 101;
		goto unlock_player;
	}


	/*
	 * Process the response 
	 */
	/* Do we need these two lines at all? */
	//pps->rtsp.cb_reqlen = 0;
	//pps->rtsp.cb_reqoffsetlen = 0;
	UNSET_PPS_FLAG(pps, PPSF_REQUEST_PENDING);

	switch (pps->method) {
	case METHOD_OPTIONS:
		/*
		 * If the response is for our own heartbeat request
		 * do not send response to player.
		 */
		pps->recv_options(pps);
		if (!CHECK_PPS_FLAG(pps, PPSF_OWN_REQUEST)) {
			if (pps->pplayer_list && pps->pplayer_list->ret_options)
				pps->pplayer_list->ret_options(pps->pplayer_list);
		}
		else {
			UNSET_PPS_FLAG(pps, PPSF_OWN_REQUEST);
		}
		mime_hdr_shutdown(&pps->rtsp.hdr);
		break;
	case METHOD_DESCRIBE:
		{
			/* Sanity check to read SDP and HEADER in one short */
			const char *data;
			int datalen;
			u_int32_t attrs;
			int hdrcnt;
			int rv;
			int sdplen;

			rv = get_known_header(&pps->rtsp.hdr, RTSP_HDR_CONTENT_LENGTH, 
						&data, &datalen, &attrs, &hdrcnt);
			if(rv || !datalen) {
				trace_code = 2;
				goto close_ps;
			}
			sdplen = atoi(data);
			if (pps->rtsp.cb_resplen < sdplen) {
				/* 
				 * BUG: Not enough data 
				 * We should add this check in front of rtsp parser.
				 */
				mime_hdr_shutdown(&pps->rtsp.hdr);
				trace_code = 102;
				goto unlock_player;
			}
                }
		pps->recv_describe(pps);

		/* Check if live streaming is enabled for this player.
		 * If not return 403 and close the connections.
		 */
		if (CHECK_PPS_FLAG(pps, PPSF_LIVE_VIDEO) && 
				pps->pplayer_list &&
				!CHECK_PLAYER_FLAG(pps->pplayer_list, PLAYERF_LIVE_ENABLED)) {
			pps->pplayer_list->prtsp->status = 403;
			rl_player_live_ret_status_resp(pps->pplayer_list, NULL, 0);
			mime_hdr_shutdown(&pps->rtsp.hdr);
			trace_code = 3;
			goto close_ps;
		}
		
		if (pps->pplayer_list && pps->pplayer_list->ret_describe)
			pps->pplayer_list->ret_describe(pps->pplayer_list);
		mime_hdr_shutdown(&pps->rtsp.hdr);
		pthread_mutex_unlock(&pps->mutex);
		if (CHECK_PPS_FLAG(pps, PPSF_IN_URI_LIST))
			rl_ps_list_remove_uri(pps->uri);
		break;
	case METHOD_SETUP:
		pps->recv_setup(pps);
		if (pps->pplayer_list && pps->pplayer_list->ret_setup && 
				pps->pplayer_list->ret_setup(pps->pplayer_list) == FALSE) {
			rl_player_live_ret_status_resp(pps->pplayer_list, NULL, 0);
			trace_code = 4;
			goto close_ps;
		}
		else {
			mime_hdr_shutdown(&pps->rtsp.hdr);
		}
		break;
	case METHOD_PLAY:
		SET_STATE_FLAG(pps, RL_BROADCAST_STATE_RTSP);

		//if (pps->heartbeat_cseq == (uint32_t) atoi(pps->rtsp.cseq)) {
		//	mime_hdr_shutdown(&pps->rtsp.hdr);
		//	return TRUE;
		//} 

		//pthread_mutex_lock(&rlcon_mutex);
		pps->recv_play(pps);
		if (pps->pplayer_list && pps->pplayer_list->ret_play)
			pps->pplayer_list->ret_play(pps->pplayer_list);
		if (CHECK_PPS_FLAG(pps, PPSF_LIVE_VIDEO) && pps->pplayer_list 
				&& !CHECK_PPS_FLAG(pps, PPSF_SERVER_WMS)) {
			SET_PLAYER_FLAG(pps->pplayer_list, PLAYERF_FOUND_LIVE);
		}
		mime_hdr_shutdown(&pps->rtsp.hdr);
		//pthread_mutex_unlock(&rlcon_mutex);
		break;
	case METHOD_PAUSE:
		pps->recv_pause(pps);
		if (pps->pplayer_list && !CHECK_PLAYER_FLAG(pps->pplayer_list, PLAYERF_FOUND_LIVE)
				&& pps->pplayer_list->ret_pause) {
			pps->pplayer_list->ret_pause(pps->pplayer_list);
		}
		mime_hdr_shutdown(&pps->rtsp.hdr);
		break;
	case METHOD_GET_PARAMETER:
		pps->recv_get_parameter(pps);
		if(pps->pplayer_list && !CHECK_PLAYER_FLAG(pps->pplayer_list, PLAYERF_FOUND_LIVE)
				&& pps->pplayer_list->ret_get_parameter) {
			pps->pplayer_list->ret_get_parameter(pps->pplayer_list);
		}
		mime_hdr_shutdown(&pps->rtsp.hdr);
		break;
	case METHOD_SET_PARAM:
		pps->recv_set_parameter(pps);
		if (pps->pplayer_list && !CHECK_PLAYER_FLAG(pps->pplayer_list, PLAYERF_FOUND_LIVE)
				&& pps->pplayer_list->ret_set_parameter) {
			pps->pplayer_list->ret_set_parameter(pps->pplayer_list);
		}
		mime_hdr_shutdown(&pps->rtsp.hdr);
		break;
	case METHOD_TEARDOWN:
		SET_PPS_FLAG(pps, PPSF_TEARDOWN);
		//SET_PLAYER_FLAG(pps->pplayer_list, PLAYERF_TEARDOWN);
		pps->recv_teardown(pps);
		//rl_player_ret_teardown(pps->pplayer_list);
 
		if( (pps->pplayer_list!=NULL) &&
				(pps->pplayer_list->player_type == RL_PLAYER_TYPE_FAKE_PLAYER) ) {
			pplayer = pps->pplayer_list;
			//pthread_mutex_unlock(&pps->rtsp.mutex);
			orp_fsm(pplayer, locks);
			//pthread_mutex_lock(&pps->rtsp.mutex);
		}
		trace_code = 5;
		goto close_ps;
	}

	if (pnm_pl) { pthread_mutex_unlock(&pnm_pl->mutex);
		UNSET_LOCK(locks, LK_PL_GNM);
	}
	pthread_mutex_unlock(&pps->rtsp.mutex);
	UNSET_LOCK(locks, LK_PS);

	//pthread_mutex_lock(&pps->rtsp.mutex);
	/* No need to call any function here */
	for ( pplayer = pps->pplayer_list ; pplayer ; ) {
		pplayer_next = pplayer->next;
		if (pplayer->player_type == RL_PLAYER_TYPE_FAKE_PLAYER ) {
			// continue for fake player
			if (orp_fsm(pplayer, locks) == -1)
				return TRUE;
			//if (pnm_pl) pthread_mutex_unlock(&pnm_pl->mutex);
			//return TRUE;
		}
		pplayer = pplayer_next;
	}
	//pthread_mutex_unlock(&pps->rtsp.mutex);

	if (pps->pplayer_list && pps->pplayer_list->prtsp->cb_reqlen) {
		//int ret;
	
		if (pnm_pl) {
			if (pthread_mutex_trylock(&pnm_pl->mutex) != 0)
				return TRUE;
		}
		SET_LOCK(locks,LK_PL_GNM);
		ret = rl_player_process_rtsp_request(pps->pplayer_list->prtsp->fd, pps->pplayer_list, locks);
		if (ret != -1) {	/* Error */
			if (pnm_pl) pthread_mutex_unlock(&pnm_pl->mutex);
			return TRUE;
		}
		pps->pplayer_list->prtsp->src_ip = pps->pplayer_list->peer_ip_addr;
		pps->pplayer_list->prtsp->dst_ip = pps->pplayer_list->own_ip_addr;
		rl_player_live_ret_status_resp(pps->pplayer_list, NULL, 0);

		rl_player_teardown(pps->pplayer_list, TRUE, locks);
		if (pnm_pl) pthread_mutex_unlock(&pnm_pl->mutex);
		return TRUE;
	}
	
	return TRUE;

unlock_player:
	pthread_mutex_unlock(&pps->rtsp.mutex);
	if (pnm_pl) pthread_mutex_unlock(&pnm_pl->mutex);
	return TRUE;

close_ps:
	rl_ps_closed(pps, NULL, locks);
	//pthread_mutex_unlock(&pps->rtsp.mutex);
	if (pnm_pl) pthread_mutex_unlock(&pnm_pl->mutex);
	return TRUE;
}

static int rl_ps_epollout(int sockfd, void * private_data);
static int rl_ps_epollout(int sockfd, void * private_data)
{
	rtsp_con_ps_t * pps = (rtsp_con_ps_t *)private_data;
	char *p;
	int ret;

	if (!pps || (g_ps[pps->g_ps_index] != pps)) {
		return TRUE;
	}
	
	DBG_LOG(MSG, MOD_RTSP, "connection established successfully fd=%d", sockfd);
	/* 
	 * non-blocking Socket has been established successfully
	 */
	if (CHECK_PPS_FLAG(pps, PPSF_REQUEST_PENDING)) {
		while (pps->rtsp.cb_reqlen > 0) {
			p = &pps->rtsp.cb_reqbuf[pps->rtsp.cb_reqoffsetlen];
			ret = send(pps->rtsp_fd, p, pps->rtsp.cb_reqlen, 0);
			if (ret == -1) {
				if (errno == EAGAIN) {
					return TRUE;
				}
				return TRUE;
			} 
			else if (ret == 0) {
				return TRUE;
			}
			pps->rtsp.cb_reqlen -= ret;
			pps->rtsp.cb_reqoffsetlen += ret;
		}
		pps->rtsp.cb_reqoffsetlen = 0;
		NM_add_event_epollin(sockfd);
		return TRUE;
	}
	
	switch(pps->method) {
	case METHOD_OPTIONS:
		pps->send_options(pps);
		NM_add_event_epollin(sockfd);
		break;
	case METHOD_DESCRIBE:
		pps->send_describe(pps);
		NM_add_event_epollin(sockfd);
		break;
	case METHOD_TEARDOWN:
		pps->send_teardown(pps);
		NM_add_event_epollin(sockfd);
		break;
	default:
		//assert(0);
		break;
	}
	return TRUE;
}

static int rl_ps_epollerr(int sockfd, void * private_data);
static int rl_ps_epollerr(int sockfd, void * private_data)
{
	rtsp_con_ps_t * pps = (rtsp_con_ps_t *)private_data;
	struct network_mgr * pnm_pl = NULL;
	rtsp_con_player_t * pplayer;
	int locks = LK_PS_GNM;

	if (!pps || (g_ps[pps->g_ps_index] != pps)) {
		return TRUE;
	}
	
	/* 
	 * Socket failed to be established 
	 */
	DBG_LOG(MSG, MOD_RTSP, "fd=%d", sockfd);

	pthread_mutex_lock(&pps->rtsp.mutex);
	SET_LOCK(locks, LK_PS);
	pplayer = pps->pplayer_list;
	if (pplayer && pplayer->prtsp->fd != -1) {
		pnm_pl = &gnm[pplayer->prtsp->fd];
	}
	if (pnm_pl) {
		pthread_mutex_lock(&pnm_pl->mutex);
		SET_LOCK(locks, LK_PL_GNM);
	}
	
	if (pplayer && CHECK_PLAYER_FLAG(pplayer, PLAYERF_REQUEST_PENDING)) {
		pplayer->prtsp->status = 503;
		rl_player_live_ret_status_resp(pplayer, NULL, 0);
	}
	
	if (pplayer && pplayer->player_type == RL_PLAYER_TYPE_FAKE_PLAYER) {
		if (orp_close(pplayer, locks) == -1) {
			pthread_mutex_unlock(&pps->rtsp.mutex);
			UNSET_LOCK(locks, LK_PS);
		}
	}
	else {
		if (rl_ps_closed(pps, NULL, locks) == -1) {
			pthread_mutex_unlock(&pps->rtsp.mutex);
			UNSET_LOCK(locks, LK_PS);
		}
	}
	//pthread_mutex_unlock(&pps->rtsp.mutex);
	if (pnm_pl) pthread_mutex_unlock(&pnm_pl->mutex);
	return TRUE;

}

static int rl_ps_epollhup(int sockfd, void * private_data);
static int rl_ps_epollhup(int sockfd, void * private_data)
{
	return rl_ps_epollerr(sockfd, private_data);
}

static int rl_ps_timeout(int sockfd, void * private_data, double timeout);
static int rl_ps_timeout(int sockfd, void * private_data, double timeout)
{
	rtsp_con_ps_t * pps = (rtsp_con_ps_t *)private_data;
	int locks = LK_PS_GNM;
	
	UNUSED_ARGUMENT(sockfd);
	UNUSED_ARGUMENT(timeout);

	if (!pps || (g_ps[pps->g_ps_index] != pps)) {
		return FALSE;
	}

	if (!CHECK_PPS_FLAG(pps, PPSF_KEEP_ALIVE) && 
			pps->pplayer_list &&
			pps->pplayer_list->player_type == RL_PLAYER_TYPE_FAKE_PLAYER) {
		orp_close(pps->pplayer_list, locks);
		return FALSE;
	}
	
	if (!CHECK_PPS_FLAG(pps, PPSF_KEEP_ALIVE) &&
	    !(ISSET_STATE_FLAG(pps, RL_BROADCAST_STATE_RTSP))) {
		if (CHECK_PPS_FLAG(pps, PPSF_TEARDOWN) || !CHECK_PPS_FLAG(pps, PPSF_SETUP_DONE)) {
			rl_ps_closed(pps, NULL, locks);
			return FALSE;
		}
		else {
			SET_PPS_FLAG(pps, PPSF_OWN_REQUEST);
			if (pps->pplayer_list) 
				SET_PLAYER_FLAG(pps->pplayer_list, PLAYERF_SRVR_TIMEOUT);
			pps->send_teardown(pps);
			NM_change_socket_timeout_time(&gnm[pps->rtsp_fd], 1);
			return FALSE;
		}
	}
	UNSET_PPS_FLAG(pps, PPSF_KEEP_ALIVE);
	RESET_STATE_FLAG(pps, RL_BROADCAST_STATE_RTSP);

	return FALSE;
}



#if 0
static void rl_ps_closed_lock_held(rtsp_con_ps_t * pps)
{
	int i;
	rtsp_con_player_t * pplayer, *tmp;

	if(!pps) return;
	//	pthread_mutex_lock(&rlcon_mutex);
	nkn_unscheduleDelayedTask(pps->fkeep_alive_task);

	pplayer = pps->pplayer_list;
	if(!pplayer) {
		rl_ps_send_method_teardown(pps);
	}

	while (pplayer) {
		tmp = pplayer->next;
		rl_player_close_disconnect_from_ps(pplayer);
		pplayer = tmp;
	}
	if(pps->sdp) free(pps->sdp);
	if(pps->streamingModeString) free(pps->streamingModeString);
	if(pps->session) free(pps->session);
	if(pps->ssrc) free(pps->ssrc);
	if(pps->mode) free(pps->mode);

	/* Close this rlcon */
	if(pps->rtsp_fd != -1) { 
		// NOTE: unregister_NM_socket(fd, TRUE) deletes the fd from epoll loop.
		unregister_NM_socket(pps->rtsp_fd, TRUE);
		nkn_close_fd(pps->rtsp_fd, RTSP_FD); 
		//close(pps->rtsp_fd); 
	}
	for(i=0; i<MAX_TRACK; i++) {
		if(pps->rtp[i].udp_fd != -1) {
			unregister_NM_socket(pps->rtp[i].udp_fd, TRUE); 
			nkn_close_fd(pps->rtp[i].udp_fd, RTSP_FD); 
		}
		if(pps->rtcp[i].udp_fd != -1) {
			unregister_NM_socket(pps->rtcp[i].udp_fd, TRUE); 
			nkn_close_fd(pps->rtcp[i].udp_fd, RTSP_FD); 
		}
	}
	if(pps->rtsp.pns) pps->rtsp.pns->tot_sessions--;
	//We shouldn't set this to NULL as we are not
	//freeing the pps. We just set the magic to RL_MAGIC_DEAD
	/*for(i=0; i<g_cur_rlcon_ps; i++) {
		if(g_ps[i] == pps) {
			g_ps[i] = NULL;
			break;
		}
	}*/
	mime_hdr_shutdown(&pps->rtsp.hdr);
	rl_reset_ps_con(pps);
	pps->magic = RLMAGIC_DEAD;
	
	rl_cleanup_bw_smoothing(pps->rl_ps_bws_ctx);
	pps->rl_ps_bws_ctx = NULL;
	//free(pps);

	//pthread_mutex_unlock(&rlcon_mutex);
}
#endif // 0

/* If ps and pl locks are held, pass FALSE, else pass TRUE to acquire locks
 */
int rl_ps_closed(rtsp_con_ps_t * pps, rtsp_con_player_t * no_callback_for_this_pplayer, int locks_held)
{
	int i;
	rtsp_con_player_t * pplayer, *tmp;
	pthread_mutex_t *pmutex;
	int locks;

	if(!pps || pps->magic != RLMAGIC_ALIVE) return -1;
	if (CHECK_PPS_FLAG(pps, PPSF_CLOSE)) return -1;

	locks = lock_ps(pps, locks_held);
	SET_PPS_FLAG(pps, PPSF_CLOSE);
	pthread_mutex_lock(&g_ps_mutex);
	g_ps[pps->g_ps_index] = NULL;
	pthread_mutex_unlock(&g_ps_mutex);
	nkn_unscheduleDelayedTask(pps->fkeep_alive_task);

	/* In case of error URI remains in the connection URI list.
	 * If flag is set remove it from list, so that next requet
	 * for same URI is not blocked.
	 */
	if (CHECK_PPS_FLAG(pps, PPSF_IN_URI_LIST)) {
		pthread_mutex_unlock(&pps->mutex);
		rl_ps_list_remove_uri(pps->uri);
	}
	
	pplayer = pps->pplayer_list;
	while (pplayer) {
		tmp = pplayer->next;
		if (pplayer != no_callback_for_this_pplayer) {
			trylock_pl(pplayer, locks);
			SET_PLAYER_FLAG(pplayer, PLAYERF_CLOSE);
			pplayer->pps = NULL;
			if (pplayer->rtsp_fd != -1) {
				NM_change_socket_timeout_time(&gnm[pplayer->rtsp_fd], 1);
#if 0
				if (!pplayer->close_task && !CHECK_PLAYER_FLAG(pplayer, PLAYERF_CLOSE_TASK)) {
					pplayer->close_task = nkn_rtscheduleDelayedTask( 1 * 1000000, 
							(TaskFunc*)rl_player_close_task, (void *)pplayer);
				}
#endif
				DBG_LOG(MSG, MOD_RTSP, "Player delayed close fd=%d", pplayer->rtsp_fd);
				pthread_mutex_unlock(&pplayer->prtsp->mutex);
			}
			else if (CHECK_PLAYER_FLAG(pplayer, PLAYERF_INTERNAL)) {
				DBG_LOG(MSG, MOD_RTSP, "Internal Player close fd=%d", pplayer->rtsp_fd);
				/* no unlock, as pplayer would be freed by orp_close
				 */
				if (orp_close(pplayer, locks | LK_PL) == -1) {
					pthread_mutex_unlock(&pplayer->prtsp->mutex);
				}
			}
			else {
				pthread_mutex_unlock(&pplayer->prtsp->mutex);
			}
		}
		else {
			DBG_LOG(MSG, MOD_RTSP, "Player skip close fd=%d", pplayer->rtsp_fd);
			//pthread_mutex_lock(&pplayer->prtsp->mutex);
			pplayer->pps = NULL;
			//pthread_mutex_unlock(&pplayer->prtsp->mutex);
		}
		pplayer = tmp;
	}
	if(pps->sdp_string) free(pps->sdp_string);
	if(pps->streamingModeString) free(pps->streamingModeString);
	if(pps->session) free(pps->session);
	if(pps->ssrc) free(pps->ssrc);
	if(pps->mode) free(pps->mode);
	if(pps->hostname) free(pps->hostname);
	if(pps->uri) free(pps->uri);
	if(pps->nsconf_uid) free(pps->nsconf_uid);
	if(pps->optionsString) free(pps->optionsString);

	DBG_LOG(MSG, MOD_RTSP, "PPS close fd=%d", pps->rtsp_fd);

	/* Close this rlcon */
	if(pps->rtsp_fd != -1) { 
		if (gnm[pps->rtsp_fd].fd != -1) {
			// NOTE: unregister_NM_socket(fd, TRUE) deletes the fd from epoll loop.
			assert(gnm[pps->rtsp_fd].fd_type == RTSP_OM_FD);
			unregister_NM_socket(pps->rtsp_fd, TRUE);
			nkn_close_fd(pps->rtsp_fd, RTSP_OM_FD); 
		}
		pps->rtsp_fd = -1;
	}
	for (i=0; i<MAX_TRACK; i++) {
		if (pps->rtp[i].udp_fd != -1) {
			pmutex = &gnm[pps->rtp[i].udp_fd].mutex;
			pthread_mutex_lock(pmutex);
			assert(gnm[pps->rtp[i].udp_fd].fd_type == RTSP_OM_UDP_FD);
			// NOTE: unregister_NM_socket(fd, TRUE) deletes the fd from epoll loop.
			unregister_NM_socket(pps->rtp[i].udp_fd, TRUE); 
			nkn_close_fd(pps->rtp[i].udp_fd, RTSP_OM_UDP_FD);
			pps->rtp[i].udp_fd = -1;
			pthread_mutex_unlock(pmutex);
		}
		if (pps->rtcp[i].udp_fd != -1) {
			pmutex = &gnm[pps->rtcp[i].udp_fd].mutex;
			pthread_mutex_lock(pmutex);
			assert(gnm[pps->rtcp[i].udp_fd].fd_type == RTSP_OM_UDP_FD);
			// NOTE: unregister_NM_socket(fd, TRUE) deletes the fd from epoll loop.
			unregister_NM_socket(pps->rtcp[i].udp_fd, TRUE); 
			nkn_close_fd(pps->rtcp[i].udp_fd, RTSP_OM_UDP_FD); 
			pps->rtcp[i].udp_fd = -1;
			pthread_mutex_unlock(pmutex);
		}
		//if (pps->sdp_info.track[i].trackID)
		//	free(pps->sdp_info.track[i].trackID);
		if (pps->rtp[i].trackID)
			free(pps->rtp[i].trackID);
#if 0		
		for (j = 0; j < MAX_QUEUED_PKTS; j++) {
			/* free the queued packet buffer */
			free(pps->rtp[i].resend_buf[j].iov_base);
			pps->rtp[i].resend_buf[j].iov_base = NULL;
			pps->rtp[i].resend_buf[j].iov_len = 0;
			free(pps->rtcp[i].resend_buf[j].iov_base);
			pps->rtcp[i].resend_buf[j].iov_base = NULL;
			pps->rtcp[i].resend_buf[j].iov_len = 0;
		}
#endif
	}
	/* Origin side is not be counted 
	 * BZ 4694
	 */
	//if(pps->rtsp.pns) pps->rtsp.pns->tot_sessions--;

	// This peice of code is not required as we set the 
	// magic to RLMAGIC_DEAD

	/*for(i=0; i<g_cur_rlcon_ps; i++) {
		if(g_ps[i] == pps) {
			g_ps[i] = NULL;
			break;
		}
	}*/
	mime_hdr_shutdown(&pps->rtsp.hdr);
	pps->magic = RLMAGIC_DEAD;
	
	rl_cleanup_bw_smoothing(pps->rl_ps_bws_ctx);
	pps->rl_ps_bws_ctx = NULL;
	/* Always unlock, even if lock was not acquired in this function
	 */
	pthread_mutex_unlock(&pps->rtsp.mutex);
	free(pps);
	return 0;
}


/* ==================================================================== */
/*
 * establish socket connection to stream server
 * If successful, the network[pps->rtsp_fd].mutex is hold when returned from this function.
 */
static rtsp_con_ps_t * rl_get_ps_con(rtsp_con_player_t * pplayer);
static rtsp_con_ps_t * rl_get_ps_con(rtsp_con_player_t * pplayer)
{
	char ip_str[64];
	char *ip = NULL;
	rtsp_con_ps_t * pps;
	const namespace_config_t *nsconf = NULL;
	int ip_len = 0;
	int rv;
	uint16_t port;
	int transport = 0;
		
	RTSP_GET_ALLOC_ABS_URL(&pplayer->prtsp->hdr, pplayer->prtsp->urlPreSuffix);

	/* 
	 * For relay get server details from origin sever config
	 */
	if (!VALID_NS_TOKEN(pplayer->prtsp->nstoken)) {
		pplayer->prtsp->nstoken = rtsp_request_to_namespace(&pplayer->prtsp->hdr);
	}
	if (!VALID_NS_TOKEN(pplayer->prtsp->nstoken)) {
		DBG_LOG(WARNING, MOD_RTSP, "Failed to find namespace for rtsp uri %s.", 
			pplayer->prtsp->urlPreSuffix);
		glob_rtsp_err_namespace_not_found++;
		return NULL;
	}

	/* Find origin server for this stream */
	nsconf = namespace_to_config(pplayer->prtsp->nstoken);
	rv = request_to_origin_server(REQ2OS_ORIGIN_SERVER, 
			0, 
			&pplayer->prtsp->hdr, 
			nsconf,
			NULL, 0, 
			&ip, 0, &ip_len,
			&port, 0, 0);
	if (strcmp(ip, "0.0.0.0") == 0) {
		struct in_addr in;
		in.s_addr=pplayer->own_ip_addr; //rtsp.pns->if_addr;
		snprintf(ip_str, 64, "%s", inet_ntoa(in));
		ip = ip_str;
	}

	if (nsconf && (nsconf->http_config->origin.select_method == OSS_RTSP_DEST_IP &&   
			nsconf->http_config->origin.u.rtsp.use_client_ip == NKN_TRUE)) {
		SET_PLAYER_FLAG(pplayer->prtsp, PLAYERF_TPROXY_MODE);
		port = 554; // TODO: must set the URL as <ip>:<port> in rtsp_parser.c
	}
	else {
		UNSET_PLAYER_FLAG(pplayer->prtsp, PLAYERF_TPROXY_MODE);
	}

	/* Get transport config from namespace
	 */
	if (nsconf && (nsconf->http_config->origin.select_method == OSS_RTSP_DEST_IP ||
			nsconf->http_config->origin.select_method == OSS_RTSP_CONFIG)) {
		if (nsconf->http_config->origin.u.rtsp.server)
			transport = nsconf->http_config->origin.u.rtsp.server[0]->transport;
	}

	release_namespace_token_t(pplayer->prtsp->nstoken);
	if (rv) {
		DBG_LOG(WARNING, MOD_RTSP,
			"request_to_origin_server() failed, rv=%d", rv);
		return NULL;
	}

	pps = rl_create_new_ps(pplayer->prtsp, pplayer->prtsp->urlPreSuffix, ip, port);
	if (pps) {
		pps->nsconf_uid = nkn_strdup_type(nsconf->ns_uid, mod_rtsp_rl_setup_strdup);;
		pps->transport = transport;
	}

	if (ip) {
		free(ip);
	}
	
	return pps;

}	

/*
 * return -1: error.
 * return 1: connection established.
 * return 2: connection in the progress to be established.
 *
 * If successful, the network[pps->rtsp_fd].mutex is hold when returned from this function.
 */
static int rl_player_connect_to_svr(rtsp_con_player_t * pplayer)
{
	rtsp_con_ps_t * pps;
	int ret;

	pps = rl_get_ps_con(pplayer);
	if(pps == NULL) {
		DBG_LOG(MSG, MOD_RTSP, 
			"Pub-Server not found for player (fd=%d)", pplayer->rtsp_fd);
		return FALSE;
	}
	/* 
	 * Insert this pplayer into pplayer_list.
	 */
	pthread_mutex_lock(&pps->rtsp.mutex);
	if (pps->pplayer_list) {
		pplayer->next = pps->pplayer_list->next;
		pps->pplayer_list->next = pplayer;
	}
	else {
		pplayer->next = pps->pplayer_list;
		pps->pplayer_list = pplayer;
	}
	pplayer->pps = pps;
	pthread_mutex_unlock(&pps->rtsp.mutex);

	/* If player type is internal player, mark the server too, so that
	 * no other player would be associated with this server.
	 */
	if (pplayer->player_type == RL_PLAYER_TYPE_FAKE_PLAYER) {
		SET_PPS_FLAG(pps, PPSF_INTERNAL_PLAYER);
	} 
	else {
		/* add pps to pps list */
		rl_ps_list_check_and_add_uri(pps->uri, strlen(pps->uri), pps);
		SET_PPS_FLAG(pps, PPSF_IN_URI_LIST);
	}

	if (rtsp_tproxy_enable || CHECK_PLAYER_FLAG(pplayer->prtsp, PLAYERF_TPROXY_MODE)) {
		pplayer->ret_options 	   = rl_player_tproxy_ret_options;
		pplayer->ret_describe 	   = rl_player_tproxy_ret_describe;
		pplayer->ret_setup	   = rl_player_tproxy_ret_setup;
		pplayer->ret_play	   = rl_player_tproxy_ret_play;
		pplayer->ret_pause	   = rl_player_tproxy_ret_pause;
		pplayer->ret_teardown	   = rl_player_tproxy_ret_teardown;
		pplayer->ret_get_parameter = rl_player_tproxy_ret_get_parameter;
		pplayer->ret_set_parameter = rl_player_tproxy_ret_set_parameter;
	}
	else {
		pplayer->ret_options 	   = rl_player_live_ret_options;
		pplayer->ret_describe 	   = rl_player_live_ret_describe;
		pplayer->ret_setup	   = rl_player_live_ret_setup;
		pplayer->ret_play	   = rl_player_live_ret_play;
		pplayer->ret_pause	   = rl_player_live_ret_pause;
		pplayer->ret_teardown	   = rl_player_live_ret_teardown;
		pplayer->ret_get_parameter = rl_player_live_ret_get_parameter;
		pplayer->ret_set_parameter = rl_player_live_ret_set_parameter;
	}

#if 0
	if (CHECK_PPS_FLAG(pplayer->pps, PPSF_LIVE_VIDEO) &&
	    CHECK_PPS_FLAG(pplayer->pps, PPSF_VIDEO_PLAYING)) {
		/*
		 * Live video has already been playing.
		 */
		return TRUE;
	}

	if(pplayer->player_type == RL_PLAYER_TYPE_LIVE) {
		/*
		 * TBD: read live config from CLI and set the transport type
		 * for the pub-server connection
		 */
		pps->use_tcp_transport = 0; /* setting it to RTP/UDP for now */
	}
	else if(pplayer->player_type == RL_PLAYER_TYPE_RELAY) {
		pps->use_tcp_transport = 0;
	}
#endif // 0

	DBG_LOG(MSG, MOD_RTSP, 
		"Pub-Server found for incoming Player (pps fd=%d, player fd=%d) method=%d", 
		pps->rtsp_fd, pplayer->rtsp_fd, pps->method);

#if 0
	switch(pps->method) {
	    case METHOD_UNKNOWN: 
		break; // needs to connect to server.
	    case METHOD_PLAY:
		pthread_mutex_unlock(&rlcon_mutex);
		return 1; //already connected.
	    case METHOD_DESCRIBE:
	    case METHOD_SETUP:
		pthread_mutex_unlock(&rlcon_mutex);
		return 2; // already connected.
	    case METHOD_TEARDOWN:
		// Reject incoming clients till the teardown completes.
	        /*
		* This scenario could occur when all the players have sent their teardown request and
		* we have also relayed the teardown request to the publishing server. But the publishing server
		* is yet to reply back with the teardown message. Till that this this state is maintained.
		* Thus in this state any new client connections that come in are rejected.
		* Returning the 503 error is appropriate in this case as the service is only temporarily unavailable.
		* Since the publishing server is still in TEARDOWN state.
		*/
		pplayer->pps = NULL;
		pplayer->prtsp->status = 503;
		DBG_LOG(WARNING, MOD_RTSP, "Publishing server still in teardown state (fd=%d)", pps->rtsp_fd); 
		rl_player_ret_status_resp(pplayer, pplayer->prtsp->status);
		pthread_mutex_unlock(&rlcon_mutex);
		return -1;
	    default:
		pthread_mutex_unlock(&rlcon_mutex);
		return 2; // will callback once connection is finished.
	}
#endif // 0

	ret = rl_ps_connect(pps);
	if ( (ret < 0) && CHECK_PPS_FLAG(pps, PPSF_IN_URI_LIST)) {
		rl_ps_list_remove_uri(pps->uri);
		pthread_mutex_unlock(&pps->mutex);
		UNSET_PPS_FLAG(pps, PPSF_IN_URI_LIST);
	}
	return ret;
}

/* ==================================================================== */
/*
 * Separate thread for handling RTCP packets and broadcasting RTSP packets/
 */
/* ==================================================================== */

void nkn_rtsp_live_init(void);
void nkn_rtsp_live_init(void)
{
	int i;

	//g_cur_rlcon_ps = 0;
	for(i=0; i<MAX_RLCON_PS; i++) {
		g_ps[i] = NULL;
	}
	
	/* Set the initial RTP port start number to a random value.
	 * It should be even 16 bit number.
	 */
	rtp_udp_port  = (uint16_t)(random() & 0x0fffe);
	if (rtp_udp_port <= 1024)
		rtp_udp_port += 1024;
	rtcp_udp_port = rtp_udp_port + 1;

}


void rtsplive_keepaliveTask(rtsp_con_ps_t * pps) 
{
        //pthread_mutex_lock(&rlcon_mutex);

	/* partial BZ 2960
	 * Issue:
	 * a crash may happen if the pps has been freed and the keep alive task had been scheduled
	 * the task calls back this function with a *dangling* pps pointer (freed somewhere else).
	 * Fix:
	 * we search the global list of pub-server contexts (g_ps) to check if the pps sent in this callback
	 * is valid. if not then we have to return. the unscheduling of this task would have already happened 
	 * during the cleanup
	 */
	if( (g_ps[pps->g_ps_index] == NULL) || (g_ps[pps->g_ps_index] != pps) ) {
		return;
	}
        
	nkn_unscheduleDelayedTask(pps->fkeep_alive_task);
	pps->heartbeat_cseq = pps->cseq;
	//Check whether there has been any rtp or rtcp traffic over the past 60 seconds.
	// If this is not the case the broadcaster has been paused.
	if ( !CHECK_PPS_FLAG(pps, PPSF_KEEP_ALIVE) &&
	     (ISSET_STATE_FLAG(pps, RL_BROADCAST_STATE_RTSP)) &&
	     !(ISSET_STATE_FLAG(pps, RL_BROADCAST_STATE_PAUSE)) ) {
		//Need to start a streaming log write operation over here
		//time(&pps->rtsp.start_ts);
		//pps->method = METHOD_BROADCAST_PAUSE; 
		pps->rtsp.src_ip = pps->peer_ip_addr;
		pps->rtsp.dst_ip = pps->own_ip_addr;
		gettimeofday((&pps->rtsp.end_ts), NULL);
		rtsp_streaminglog_write(&pps->rtsp);
		SET_STATE_FLAG(pps, RL_BROADCAST_STATE_PAUSE);
	}
	//if (CHECK_PPS_FLAG(pps, PPSF_LIVE_VIDEO) && pps->rtsp.streamingMode != RTP_TCP) {
	if ((CHECK_PPS_FLAG(pps, PPSF_LIVE_VIDEO) || CHECK_PPS_FLAG(pps, PPSF_INTERNAL_PLAYER))
			&& !CHECK_PPS_FLAG(pps, PPSF_SERVER_WMS)) {
		SET_PPS_FLAG(pps, PPSF_OWN_REQUEST);
		pps->send_options( pps );
	}
	UNSET_PPS_FLAG(pps, PPSF_KEEP_ALIVE);
	RESET_STATE_FLAG(pps, RL_BROADCAST_STATE_RTSP);
	/* BZ:5118
	 * to ensure that our own heart beat does not stop us from timing out
	 * when we there is no activity on the RTP channels and we are supposed
	 * to time out. Here we are adjusting the time period of the keep alive
	 * task to be 90s if it is an internal player type
	 */
	if(pps->pplayer_list) {
	    if(pps->pplayer_list->player_type == RL_PLAYER_TYPE_FAKE_PLAYER) {
       		pps->fkeep_alive_task = nkn_scheduleDelayedTask( 90 * 1000000, 
			     (TaskFunc*)rtsplive_keepaliveTask, (void *)pps);
	    } else {
		pps->fkeep_alive_task = nkn_scheduleDelayedTask( 60 * 1000000, 
			     (TaskFunc*)rtsplive_keepaliveTask, (void *)pps);
	    }
	}

}

int rtp_player_epollin(int sockfd, void * private_data)
{
	char udp_buf[RTP_BUFFER_SIZE];
	struct sockaddr_in to;
	socklen_t tolen;
	struct sockaddr_in from;
	socklen_t fromlen;
	rtsp_con_player_t *pplayer = NULL;
	rtsp_con_ps_t	*pps = NULL; 
	rls_track_t *prls_track = NULL;
	char *p = NULL;
	int ret;
	int send_len;

	pplayer = (rtsp_con_player_t *)private_data;
	if (!pplayer) {
		return TRUE;
	}

	pps = pplayer->pps;
	if (!pps || (g_ps[pps->g_ps_index] != pps)) {
		return TRUE;
	}
		
	if (pplayer->rtp[0].udp_fd == sockfd) {
		prls_track = &pplayer->pps->rtp[0]; 
	}
	else if (pplayer->rtp[1].udp_fd == sockfd) {
		prls_track = &pplayer->pps->rtp[1];
	}
	else {
		return TRUE;
	}
	
	fromlen = sizeof(struct sockaddr);
	ret = recvfrom(sockfd, 
		udp_buf,
		RTP_BUFFER_SIZE,
		0,
		(struct sockaddr *)&from,
		&fromlen);

	if (ret < 0) {
		if (errno == EAGAIN) return TRUE;
		return TRUE;
	}

	send_len = ret;
	p = udp_buf;

	memset(&to, 0, sizeof(to));
	to.sin_family = AF_INET;
	to.sin_port = htons(prls_track->peer_udp_port);
	to.sin_addr.s_addr = pps->peer_ip_addr;
	tolen = sizeof(to);
	while (send_len) {
		ret = sendto(prls_track->udp_fd,
			p,
			send_len,
			0,
			(struct  sockaddr *)&to,
			tolen);
		if (ret < 0) {
			if (errno == EAGAIN) {
				NM_add_event_epollout(prls_track->udp_fd);
			}
			break;
		}
		glob_rtsp_tot_bytes_sent += ret;
		p += ret;
		send_len -= ret;
		
	}

	//pthread_mutex_unlock(&rlcon_mutex);
		
	return TRUE;

}

int rtp_player_epollout(int sockfd, void * private_data)
{
	/* To be implemented */

	int ret = 0;
	int sendBufLen = 0;
	int tolen = 0;
	unsigned char *p = 0;
	struct sockaddr_in to;
	rtsp_con_player_t *pplayer = NULL;
	rtsp_con_ps_t *pps = NULL;
	rls_track_t *prls_track = NULL;
	uint32_t i =0;
	
	UNUSED_ARGUMENT(sockfd);
	UNUSED_ARGUMENT(private_data);

	pplayer = (rtsp_con_player_t *)private_data;
	if (!pplayer) {
		return TRUE;
	}

	pps = pplayer->pps;
	if (!pps || (g_ps[pps->g_ps_index] != pps)) {
		return TRUE;
	}
		
	if (pplayer->rtp[0].udp_fd == sockfd) {
		prls_track = &pplayer->rtp[0]; 
	}
	else if (pplayer->rtp[1].udp_fd == sockfd) {
		prls_track = &pplayer->rtp[1];
	}
	else {
		return TRUE;
	}

	memset(&to, 0, sizeof(struct sockaddr_in));

	to.sin_family = AF_INET;
	to.sin_port = htons(prls_track->peer_udp_port);
	to.sin_addr.s_addr = pplayer->peer_ip_addr;
	tolen = sizeof(to);

	for(i = 0;i < prls_track->num_resend_pkts; i++) { 
		p  = (unsigned char*)prls_track->resend_buf[i].iov_base;
		sendBufLen = prls_track->resend_buf[i].iov_len;
		
		while (sendBufLen) {
			ret = sendto(prls_track->udp_fd,
				     p,
				     sendBufLen,
				     0,
				     (struct  sockaddr *)&to,
				     tolen);
			
			if (ret < 0) break;
			sendBufLen -=ret;
			p +=ret;
		}
		
	}

	prls_track->num_resend_pkts = 0;
	prls_track->cb_bufLen = 0;
	NM_del_event_epoll(sockfd);
	NM_add_event_epollin(sockfd);
	//pthread_mutex_unlock(&rlcon_mutex);

	return TRUE;
}

int rtp_player_epollerr(int sockfd, void * private_data)
{
	/* To be implemented */

	UNUSED_ARGUMENT(sockfd);
	UNUSED_ARGUMENT(private_data);

	return TRUE;
}

int rtp_player_epollhup(int sockfd, void * private_data)
{
	/* To be implemented */

	UNUSED_ARGUMENT(sockfd);
	UNUSED_ARGUMENT(private_data);

	return TRUE;
}

int rtcp_player_epollin(int sockfd, void * private_data)
{
	char udp_buf[RTP_BUFFER_SIZE];
	struct sockaddr_in to;
	socklen_t tolen;
	struct sockaddr_in from;
	socklen_t fromlen;
	rtsp_con_player_t *pplayer = NULL;
	rtsp_con_ps_t *pps = NULL;
	rls_track_t *prls_track = NULL;
	char *p = NULL;
	int ret;
	int send_len;


	pplayer = (rtsp_con_player_t *)private_data;
	if (!pplayer) {
		return TRUE;
	}

	pps = pplayer->pps;
	if (!pps || (g_ps[pps->g_ps_index] != pps)) {
		return TRUE;
	}

	if (pplayer->rtcp[0].udp_fd == sockfd) {
		prls_track = &pplayer->pps->rtcp[0]; 
	}
	else if (pplayer->rtcp[1].udp_fd == sockfd) {
		prls_track = &pplayer->pps->rtcp[1];
	}
	else {
		return TRUE;
	}
	
	fromlen = sizeof(struct sockaddr);
	ret = recvfrom(sockfd, 
		udp_buf,
		RTP_BUFFER_SIZE,
		0,
		(struct sockaddr *)&from,
		&fromlen);

	if (ret < 0) {
		if (errno == EAGAIN) return TRUE;
		return TRUE;
	}

	SET_PLAYER_FLAG(pplayer, PLAYERF_KEEP_ALIVE);
	send_len = ret;
	p = udp_buf;
	
	memset(&to, 0, sizeof(to));
	to.sin_family = AF_INET;
	to.sin_port = htons(prls_track->peer_udp_port);
	to.sin_addr.s_addr = pps->peer_ip_addr;
	tolen = sizeof(to);
#if 0	
	while (send_len) {
		ret = sendto(prls_track->udp_fd,
			p,
			send_len,
			0,
			(struct  sockaddr *)&to,
			tolen);
		if (ret < 0) {
			if (errno == EAGAIN) {
				NM_add_event_epollout(prls_track->udp_fd);
			}
			break;
		}
		glob_rtsp_tot_bytes_sent += ret;
		send_len -= ret;
		p += ret;
	}
#endif
	return TRUE;

}

int rtcp_player_epollout(int sockfd, void * private_data)
{
	/* To be implemented */
	int ret = 0;
	int sendBufLen = 0;
	int tolen = 0;
	rtsp_con_player_t *pplayer = NULL;
	rtsp_con_ps_t *pps = NULL;
	rls_track_t *prls_track = NULL;
	unsigned char *p = 0;
	struct sockaddr_in to;
	uint32_t i = 0;

	UNUSED_ARGUMENT(sockfd);
	UNUSED_ARGUMENT(private_data);

	pplayer = (rtsp_con_player_t *)private_data;
	if (!pplayer) {
		return TRUE;
	}

	pps = pplayer->pps;
	if (!pps || (g_ps[pps->g_ps_index] != pps)) {
		return TRUE;
	}
		
	if (pplayer->rtcp[0].udp_fd == sockfd) {
		prls_track = &pplayer->rtcp[0]; 
	}
	else if (pplayer->rtcp[1].udp_fd == sockfd) {
		prls_track = &pplayer->rtcp[1];
	}
	else {
		return TRUE;
	}

	memset(&to, 0, sizeof(struct sockaddr_in));

	to.sin_family = AF_INET;
	to.sin_port = htons(prls_track->peer_udp_port);
	to.sin_addr.s_addr = pplayer->peer_ip_addr;
	tolen = sizeof(to);

	for(i = 0;i < prls_track->num_resend_pkts; i++) { 
		p = (unsigned char*)prls_track->resend_buf[i].iov_base;
		sendBufLen = prls_track->resend_buf[i].iov_len;
		
		while (sendBufLen) {
			ret = sendto(prls_track->udp_fd,
				     p,
				     sendBufLen,
				     0,
				     (struct  sockaddr *)&to,
				     tolen);
			if (ret < 0) break;
			sendBufLen -=ret;
			p +=ret;
		}
	}
	
	prls_track->cb_bufLen = 0;
	prls_track->num_resend_pkts = 0;
	NM_del_event_epoll(sockfd);
	NM_add_event_epollin(sockfd);

	return TRUE;
}

int rtcp_player_epollerr(int sockfd, void * private_data)
{
	/* To be implemented */

	UNUSED_ARGUMENT(sockfd);
	UNUSED_ARGUMENT(private_data);

	return TRUE;
}

int rtcp_player_epollhup(int sockfd, void * private_data)
{
	return rtcp_player_epollerr(sockfd, private_data);
}


int rtp_ps_epollin(int sockfd, void *private_data)
{
	char udp_buf[RTP_BUFFER_SIZE + 4];
	struct sockaddr_in from;
	socklen_t fromlen;
	rtsp_con_ps_t *pps = NULL;
	rtsp_con_player_t *pplayer = NULL;
	char *p = NULL;
	int ret, i;
	int recv_len; 

	pps = (rtsp_con_ps_t *)private_data;

	if (!pps || (g_ps[pps->g_ps_index] != pps)) {
		return TRUE;
	}
	
	/* Acquire lock so that player list is not deleted altered while
	 * we are sending data. If unable to obtain lock, return, so that
	 * we can try to read the data in next epollin.
	 */
	if (pthread_mutex_trylock(&pps->rtsp.mutex) != 0) {
		AO_fetch_and_add1(&glob_rtsp_tot_rtp_ps_trylock_busy);
		return TRUE;
	}

	if (pps->rtp[0].udp_fd == sockfd) {
		i = 0;
	}
	else if (pps->rtp[1].udp_fd == sockfd) {
		i = 1;
	}
	else {
		pthread_mutex_unlock(&pps->rtsp.mutex);
		return TRUE;
	}

	fromlen = sizeof(from);
	/* Leave 4 bytes space for inline header, if inline mode would be
	 * used.
	 */
	p = udp_buf + 4;

	ret = recvfrom(sockfd, 
		p,
		RTP_BUFFER_SIZE,
		0,
		(struct sockaddr *)&from,
		&fromlen);

	/* Fetch the time stamp from the udp buffer containing the rtp header
	 */
	if (ret < 0) {
		pthread_mutex_unlock(&pps->rtsp.mutex);
		if (errno == EAGAIN) return TRUE;
		return TRUE;
	}
	pps->rtp[i].rtp.time_scale = pps->rtp[i].sample_rate;
        rtcp_rtp_state_update(&pps->rtp[i].rtp, p);
	
	SET_PPS_FLAG(pps, PPSF_KEEP_ALIVE);
	if (ISSET_STATE_FLAG(pps, RL_BROADCAST_STATE_PAUSE)) {
		RESET_STATE_FLAG(pps, RL_BROADCAST_STATE_PAUSE);
	}
	recv_len = ret;

	/* Do not update count for internal player/cache.
	 * BZ 5149.
	 */
	if (!CHECK_PPS_FLAG(pps, PPSF_INTERNAL_PLAYER)) {
		glob_rtp_tot_udp_packets_fwd ++;
	}

	pplayer = pps->pplayer_list; 
	if (pplayer && (pplayer->pause != TRUE) && 
		(rl_query_bw_smoothing(pplayer->rl_player_bws_ctx, p, recv_len) == BWS_SEND_PACKET)) {
		/*
		 * Forward packet to this pplayer
		 */
		(pplayer->send_data)(pplayer, p, recv_len, RL_DATA_RTP, i);
		if (pplayer->player_type == RL_PLAYER_TYPE_FAKE_PLAYER)
			AO_fetch_and_add(&glob_rtsp_tot_size_to_cache, recv_len);
		else
			AO_fetch_and_add(&glob_rtsp_tot_size_from_origin, recv_len);
	}
	
	/* Loop split so that size from RAM counter can be updated without check.
	 */
	if (pplayer) {
	for (pplayer = pplayer->next; pplayer ; pplayer = pplayer->next) {
		if (pplayer->pause == TRUE) {
			continue;
		}

		if (rl_query_bw_smoothing(pplayer->rl_player_bws_ctx, p, recv_len) != BWS_SEND_PACKET) {
			continue;
		}

		/*
		 * Forward packet to this pplayer
		 */
		(pplayer->send_data)(pplayer, p, recv_len, RL_DATA_RTP, i);
		AO_fetch_and_add(&glob_rtsp_tot_size_from_cache, recv_len);
	}
	}

	pthread_mutex_unlock(&pps->rtsp.mutex);
	return TRUE;
}

int rtp_ps_epollout(int sockfd, void *private_data)
{
	UNUSED_ARGUMENT(sockfd);
	UNUSED_ARGUMENT(private_data);

	return TRUE;
}

int rtp_ps_epollerr(int sockfd, void *private_data)
{
	UNUSED_ARGUMENT(sockfd);
	UNUSED_ARGUMENT(private_data);

	return TRUE;
}

int rtp_ps_epollhup(int sockfd, void *private_data)
{
	return rtp_ps_epollerr(sockfd, private_data);
}

int rtcp_ps_epollin(int sockfd, void *private_data)
{
	char udp_buf[RTP_BUFFER_SIZE];
	struct sockaddr_in to;
	socklen_t tolen;
	struct sockaddr_in from;
	socklen_t fromlen;
	rtsp_con_ps_t *pps = NULL;
	rtsp_con_player_t *pplayer = NULL;
	char *p = NULL;
	int ret, i, recv_len; //send_len, 
	char rtcp_rr_pkt[RTP_BUFFER_SIZE],rtcp_sr_pkt[RTP_BUFFER_SIZE];
	char *rtcp_st_buf;//rtcp_sdes[RTCP_SDES_SIZE];
	int srlen,len,rtcp_bye=0;
	
	pps = (rtsp_con_ps_t *)private_data;
	if (!pps || (pps != g_ps[pps->g_ps_index])) {
		return TRUE;
	}

	/* Acquire lock so that player list is not deleted altered while
	 * we are sending data. If unable to obtain lock, return, so that
	 * we can try to read the data in next epollin.
	 */
	if (pthread_mutex_trylock(&pps->rtsp.mutex) != 0) {
		return TRUE;
	}

	if (pps->rtcp[0].udp_fd == sockfd) {
		i = 0;
	}
	else if (pps->rtcp[1].udp_fd == sockfd) {
		i = 1;
	}
	else {
		pthread_mutex_unlock(&pps->rtsp.mutex);
		return TRUE;
	}

	fromlen = sizeof(from);

	ret = recvfrom(sockfd, 
		udp_buf,
		RTP_BUFFER_SIZE,
		0,
		(struct sockaddr *)&from,
		&fromlen);

	if (ret < 0) {
		pthread_mutex_unlock(&pps->rtsp.mutex);
		if (errno == EAGAIN) return TRUE;
		return TRUE;
	}

	SET_PPS_FLAG(pps, PPSF_KEEP_ALIVE);
	if (ISSET_STATE_FLAG(pps, RL_BROADCAST_STATE_PAUSE)) {
		RESET_STATE_FLAG(pps, RL_BROADCAST_STATE_PAUSE);
	}

	// Send a receiver report to the publisher.
	srlen = ret;
	len = ret;
	ret = 0;
	rtcp_st_buf = rtcp_rr_pkt;
	if (pps->rtp[i].rtp.rtcp_pkts ==0) {
		pps->rtp[i].rtp.rtcp_pkts = 1;
		form_rtcp_sdes(pps->rtcp_sdes,pps->own_ip_addr);
		pps->rtcp_bye[0] = 0x81;
		pps->rtcp_bye[1] = 203;
		pps->rtcp_bye[2] = 0;
		pps->rtcp_bye[3] = 1;
		byte_write32(pps->rtcp_bye,4,RTCP_MFD_SSRC);
	}
	
	rtcp_bye = 0;
	rtcp_pkt_dissect_t rtcp_dissect;
	init_rtcp_pkt_dissect(&rtcp_dissect);
	if (rtcp_dissector((uint8_t*)udp_buf, len, 0, &rtcp_dissect) == 0) {
		if (rtcp_dissect.sr != NULL) {
			read_rtcp_sr_pkt(&pps->rtcp_s, (char*)rtcp_dissect.sr);
			form_rtcp_rr_pkt(pps->rtp[i].rtp, pps->rtcp_s, &pps->rtcp_r, &rtcp_rr_pkt[ret]);
			ret += RTCP_RR_SIZE;
		}
		if (rtcp_dissect.sdes != NULL) {
			memcpy(&rtcp_rr_pkt[ret],pps->rtcp_sdes,RTCP_SDES_SIZE);
			ret += RTCP_SDES_SIZE;
		}
		if (rtcp_dissect.bye != NULL) {
			memcpy(&rtcp_rr_pkt[ret],pps->rtcp_bye,RTCP_BYE_SIZE);
			ret += RTCP_BYE_SIZE;
			rtcp_bye = 1;
		}
	}
	to = from;
	//memset(&to, 0, sizeof(to));
	to.sin_family = AF_INET;
	to.sin_port = htons(pps->rtcp[i].peer_udp_port);
	to.sin_addr.s_addr = pps->peer_ip_addr;
	tolen = sizeof(to);
	sendto(sockfd,
		rtcp_st_buf,
		ret,
		0,
		(struct  sockaddr *)&to,
		tolen);

	//	glob_rtsp_tot_bytes_sent += ret;
	/* Do not update count for internal player/cache.
	 * BZ 5149.
	 */
	if (!CHECK_PPS_FLAG(pps, PPSF_INTERNAL_PLAYER)) {
		glob_rtcp_tot_udp_rr_packets_sent++;
	}
	
	//Form the sender packets
	p = rtcp_sr_pkt + 4;	// Leave space for inline mode header
	form_rtcp_sr_pkt(&pps->rtcp_s,p);
	//add the sdes packet
	memcpy(p + RTCP_SENDER_SIZE, pps->rtcp_sdes, RTCP_SDES_SIZE);
	ret = RTCP_SENDER_SIZE + RTCP_SDES_SIZE;
	if (rtcp_bye){
		memcpy(p + ret, pps->rtcp_bye, RTCP_BYE_SIZE);
		ret += RTCP_BYE_SIZE;
	}

	recv_len = ret;
	pplayer = pps->pplayer_list;
	if (pplayer && (pplayer->pause != TRUE)) {
		(pplayer->send_data)(pplayer, p, recv_len, RL_DATA_RTCP, i);
		if (pplayer->player_type != RL_PLAYER_TYPE_FAKE_PLAYER)
			AO_fetch_and_add(&glob_rtsp_tot_size_from_origin, recv_len);
	}
	if (pplayer) {
	for (pplayer = pplayer->next; pplayer; pplayer = pplayer->next) {
		if (pplayer->pause == TRUE) {
			continue;
		}

		(pplayer->send_data)(pplayer, p, recv_len, RL_DATA_RTCP, i);
		AO_fetch_and_add(&glob_rtsp_tot_size_from_cache, recv_len);
	}
	}
	pthread_mutex_unlock(&pps->rtsp.mutex);
#if 0
	if (rtcp_bye && pps->pplayer_list && 
			(pps->pplayer_list->player_type == RL_PLAYER_TYPE_FAKE_PLAYER) ) {
		if (pps->rtsp_fd == -1 || pthread_mutex_trylock(&gnm[pps->rtsp_fd].mutex) != 0) {
			return TRUE;
		}
		orp_teardown_state((void*)pps->pplayer_list);
		pthread_mutex_unlock(&gnm[pps->rtsp_fd].mutex);
	}
#endif
	//pthread_mutex_unlock(&rlcon_mutex);
	return TRUE;
}

int rtcp_ps_epollout(int sockfd, void *private_data)
{
	UNUSED_ARGUMENT(sockfd);
	UNUSED_ARGUMENT(private_data);

	return TRUE;
}

int rtcp_ps_epollerr(int sockfd, void *private_data)
{
	UNUSED_ARGUMENT(sockfd);
	UNUSED_ARGUMENT(private_data);

	return TRUE;
}

int rtcp_ps_epollhup(int sockfd, void *private_data)
{
	return rtcp_ps_epollerr(sockfd, private_data);
}

/*
 *@brief - parses the SDP data and gleans the sample rate, number of tracks etc and indexes them
 *based on the track ID provided in the SDP data
 *@param p_sdp_data [in] - buffer containing the SDP data
 *@param pps [in] - the pub-server context
 *@return Returns '0' on success and populates the sdp_info substructure in the pub server context
 * returns -1 on error
 */
int32_t 
rl_ps_parse_sdp_info( char **p_sdp_data, rtsp_con_ps_t *pps)
{
	char *p, *p2, *p3, *p4, *e;
	int len;
	int ret;
	int track;
	char dlim[4] = "\r\n";
	int ln;
	rl_sdp_info_t * psdp_info;

	p = *p_sdp_data;
	psdp_info = &pps->sdp_info;
	track = -1;
	ret = 0;

	while (p && *p) {
		e = p;

		ln = strcspn( p, dlim );
		if ( ln == 0 )
			return 0;
		len = ln - 2;
		
		switch (*p) {
		case 'a':
			if ( strncasecmp( p+2, "control:", 8 ) == 0 ) {
				if (track == -1) {
				}
				else {
					char *pdst;
					// ToDo :  check if full URL is given
					// control section for Audio/Video
					pdst = psdp_info->track[track].trackID;
					if (strncmp(p + 10, "rtsp://", 7) == 0) {
						int ul;
						p2 = memchr(p+10+7, '/', len-8-7);
						if (p2) {
							ul = p2 - (p+10+7);
							strncpy( pdst, p2, len-8-7-ul );
							*(pdst + len-8-7-ul) = 0;
						}
						psdp_info->track[track].flag_abs_url = 1;
					}
					else {
						strncpy( pdst, p + 10, len-8 );
						*(pdst + len-8) = 0;
						psdp_info->track[track].flag_abs_url = 0;
					}
				}
			}
			else if ( strncasecmp( p+2, "rtpmap", 6 ) == 0 ) {
				p2 = strchr( p+9, '/' );
				if ( p2 )
					psdp_info->track[track].sample_rate = atoi(p2 + 1); // glean the sample rate
				    
			}
			else if ( strncasecmp( p+2, "range:npt=", 10 ) == 0 ) {
				// e.g. a=range:npt=now-
				// e.g. a=range:npt=0-17.792
				p2 = p + 2 + 10;
				p3 = strchr(p2, '-');
				if (p3) {
					// Copy npt_start
					p4 = &psdp_info->npt_start[0];
					while (p2 < p3) { *p4++ = *p2++; } *p4 = 0;
					// Copy npt_end
					p3++;
					p4 = &psdp_info->npt_stop[0];
					while (*p3!='\r' && *p3!='\n') { *p4++=*p3++; } *p4=0;
				}
			}
			else if ( strncasecmp( p+2, "type:broadcast", 14 ) == 0 ) {
				/* WMS sends the broadcast field for live video */
				SET_PPS_FLAG(pps, PPSF_LIVE_VIDEO);
			}
			break;
		case 'b':
			track = psdp_info->num_trackID - 1;
			memcpy(psdp_info->track[track].b_string, p+2, len);
			psdp_info->track[track].b_string[len]=0;
			break;
		case 'm':
			psdp_info->num_trackID++;
			track = psdp_info->num_trackID - 1;

			// ToDo :  check for type audio, video and ignore others
			if (strncasecmp(p+2, "video", 5)) {
				psdp_info->track[track].track_type = TRACK_TYPE_VIDEO;
			}
			else if (strncasecmp(p+2, "audio", 5)) {
				psdp_info->track[track].track_type = TRACK_TYPE_AUDIO;
			}
			else {
				psdp_info->track[track].track_type = TRACK_TYPE_UNKNOWN;
			}
			memcpy(psdp_info->track[track].m_string, p+2, len);
			psdp_info->track[track].m_string[len]=0;
			break;
		case 's': //SESSION NAME
		case 'v':
		case 'i': // Video Name
		default:
			break;
		}
		p = p + ln;
		while (*p == '\r' || *p == '\n')
			p++;
	}
	assert(psdp_info->num_trackID != 0);

	//some sanity
	if(track > MAX_TRACK-1 || track == 0) {
		//should not be greater than 2 tracks or no tracks
		ret = -1;
	}
	*p_sdp_data = p;

	/* 
	 * if npt_stop has not been specified, and it is open.
	 * we regard this as live streaming. Also if start is 0.
	 */
#if 0
	if (((psdp_info->npt_start[0] == 0)||(atof(psdp_info->npt_start) == 0)
			||(psdp_info->npt_start[0] == 'n'))
			&& (psdp_info->npt_stop[0] == 0)) {
		SET_PPS_FLAG(pps, PPSF_LIVE_VIDEO);
	}
#endif	
	if ((atof(psdp_info->npt_start) == 0) && (psdp_info->npt_stop[0] == 0)) {
		SET_PPS_FLAG(pps, PPSF_LIVE_VIDEO);
	}
	return ret;

}


/*
 * @brief - finds the sampling rate of the given track ID
 * @param pps [in] - the pub server context
 * @param trackID [in] - the track ID whose sample rate is requested for
 * @return returns the sample rate if function succeeds. On failiure returns -1
 */
uint32_t 
rl_get_sample_rate(rtsp_con_ps_t *pps, char * trackID)
{
	rl_sdp_info_t * psdp_info;
	int32_t i;

	psdp_info = &pps->sdp_info;
	i = 0;

	for(i = 0; i < pps->sdp_info.num_trackID;i++) {
		if(strcmp(psdp_info->track[i].trackID, trackID) == 0) {
			//found the trackID whose information is needed
			return psdp_info->track[i].sample_rate;
		}
	}

	return -1; //should not come here
}

int form_rtcp_rr_pkt(rtcp_rtp_state_t rtp,rtcp_spkt_t sender, rtcp_rpkt_t *rr,char* udata){
    int expected_interval,received_interval,lost_interval;
    //    int SSRC_MFD = 0;
    rr->V = RTCP_VERSION;
    rr->P = 0;
    rr->RC = 1;
    rr->PT = RTCP_RR_PT;
    rr->length = RTCP_RR_SIZE/4 - 1;
    rr->SSRC_sender = RTCP_MFD_SSRC ;
    rr->SSRC_source = sender.SSRC;
    /*fraction lost*/
    expected_interval = rtp.seq_num  - rtp.start_seq_num - rr->rtcp_state.expected_prior;
    received_interval = rtp.num_pkts - rr->rtcp_state.received_prior;
    rr->rtcp_state.expected_prior = rtp.seq_num  - rtp.start_seq_num;
    rr->rtcp_state.received_prior = rtp.num_pkts;
    lost_interval = expected_interval - received_interval;
    if (expected_interval == 0 || lost_interval <= 0)
        rr->frac_lost = 0;
    else
        rr->frac_lost = (lost_interval << 8) / expected_interval;
    /*Cumulative Packets Lost*/
    rr->cum_pkts_lost =  rtp.num_pkts_lost;//rtp->seq_num -  rtp->exp_seq_num ;//rtp.seq_num - rtp.start_seq_num - 1;//rtp.num_pkts;
    /*Sequence number and Jitter*/
    rr->seq_num = rtp.seq_num;//-1;
    rr->jitter = rtp.jitter;
    /*LSR and DLSR*/
    rr->LSR = ((sender.NPT_MSB&0xFFFF)<<16)|((sender.NPT_LSB>>16)&0xFFFF);
    rr->DLSR = 0;
    create_rtcp_rr_pkt(*rr,udata);
    return 1;
}

int read_rtcp_sr_pkt(rtcp_spkt_t* sender,char* data){
	/*Sender packet received - read and parse*/
	int pos =0, ret = 0;
	uint16_t len;

	memset(sender, 0, sizeof(rtcp_spkt_t));
	sender->V = (uint8_t)(data[pos]) >> 6;
	sender->P = 0;
	sender->RC = 0;
	pos++;
	sender->PT = data[pos++];
	ret = (int)sender->PT;
	len = data[pos++];
	len = (len << 8) | data[pos++];
	ret |= (((int)(len))<<8);
	if(sender->PT == RTCP_SR_PT){
		sender->length = len;
		sender->SSRC = byte_read32(data,pos);
		pos += 4;
		sender->NPT_MSB =  byte_read32(data,pos);
		pos += 4;
		sender->NPT_LSB  =  byte_read32(data,pos);
		pos += 4;
		sender->RTP_TS =  byte_read32(data,pos);
		pos += 4;
		sender->pkt_cnt =  byte_read32(data,pos);
		pos += 4;
		sender->octet_pkt_cnt =  byte_read32(data,pos);
		pos += 4;
	}
	return ret;
}


int create_rtcp_rr_pkt(rtcp_rpkt_t rr,char *data){
    int pos= 0;
    char word =0;
    word = (rr.V<<6)|(rr.P<<5)|(rr.RC&0x0F);
    data[pos++] = word;
    data[pos++] = rr.PT;
    byte_write16(data,pos,rr.length);
    pos+=2;
    byte_write32(data,pos,rr.SSRC_sender);
    pos+=4;
    byte_write32(data,pos,rr.SSRC_source);
    pos+=4;

    data[pos++] = rr.frac_lost;
    byte_write24(data,pos,rr.cum_pkts_lost);
    pos+=3;
    //    rr.cum_pkts_lost = (rr.cum_pkts_lost&0xFFFFFF)|(rr.frac_lost<<24);
    //byte_write32(data,pos,rr.cum_pkts_lost);
    ///pos+=4;
    //    rr.seq_num = htonl(rr.seq_num);
    byte_write32(data,pos,rr.seq_num);
    pos+=4;
    byte_write32(data,pos,rr.jitter);
    pos+=4;
    byte_write32(data,pos,rr.LSR);
    pos+=4;
    byte_write32(data,pos,rr.DLSR);
    pos+=4;
    return 1;
}


int form_rtcp_sr_pkt(rtcp_spkt_t* sender,char*data){
    // int pos = 0;
    // struct timeval now;
    sender->V = RTCP_VERSION;
    sender->P = 0;
    sender->RC = 0;
    sender->PT = RTCP_SR_PT;
    //gettimeofday( &now, NULL );
    sender->length = (RTCP_SENDER_SIZE/4)-1;
    //sender->NPT_MSB = now.tv_sec;
    //sender->NPT_LSB = now.tv_usec;
    //sender->RTP_TS = rtp.ts;
    create_rtcp_sr_pkt(*sender,data);
    return 1;
}

int create_rtcp_sr_pkt(rtcp_spkt_t sr, char *data){
    int pos= 0;
    char word =0;
    word = (sr.V<<6)|(sr.P<<5)|(sr.RC&0x0F);
    data[pos++] = word;
    data[pos++] = sr.PT;
    byte_write16(data,pos,sr.length);
    pos+=2;
    byte_write32(data,pos,sr.SSRC);
    pos+=4;
    byte_write32(data,pos,sr.NPT_MSB);
    pos+=4;
    byte_write32(data,pos,sr.NPT_LSB);
    pos+=4;
    byte_write32(data,pos,sr.RTP_TS);
    pos+=4;
    byte_write32(data,pos,sr.pkt_cnt);
    pos+=4;
    byte_write32(data,pos,sr.octet_pkt_cnt);
    return 1;
}

int byte_read32(char* buf,int pos){
    return (((buf[pos]&0xFF)<<24)|((buf[pos+1]&0xFF)<<16)|((buf[pos+2]&0xFF)<<8)|(buf[pos+3]&0xFF));
}

int byte_write32(char* data,int pos,int val){
    data[pos] = val>>24;
    data[pos+1] = (val>>16)&0xFF;
    data[pos+2] = (val>>8)&0xFF;
    data[pos+3] = val&0xFF;
    return 1;

}

int byte_write24(char* data,int pos,int val){
    data[pos] = val>>16;
    data[pos+1] = (val>>8)&0xFF;
    data[pos+2] = val&0xFF;
    return 1;
}

int byte_write16(char* data,int pos,int val){
    data[pos] = (val>>8)&0xFF;
    data[pos+1] = val&0xFF;
    return 1;
}



int rtcp_rtp_state_update( rtcp_rtp_state_t *rtp,char *data){

    //    int pos = 0;
    struct timeval now;
    double elapsed;
    double rtp_time;
    uint32_t arrival;
    int transit;
    int d;
    int diff;
    rtp->num_pkts++;
    //rtp->num_pkts_expected = rtp->num_pkts;
    rtp->seq_num = byte_read32(data,0);
    rtp->seq_num &= 0xFFFF;
    rtp->ts = byte_read32(data,4);
    if(rtp->num_pkts == 1){
        rtp->start_seq_num = rtp->seq_num;
	rtp->exp_seq_num = rtp->seq_num;
        gettimeofday( &rtp->start_time, NULL );
    }
    if(rtp->exp_seq_num == rtp->seq_num)
        rtp->exp_seq_num++;
    diff =  rtp->seq_num -  (rtp->exp_seq_num-1);
    if(diff<0)
	diff = 0;
    rtp->num_pkts_lost+= diff;
    
    //else
    //rtp->num_pkts_expected += rtp->seq_num -  rtp->exp_seq_num ;
    //Calculate Jitter from nload

    gettimeofday( &now, NULL );
    elapsed = delta_timeval(&rtp->start_time, &now) / 1000000.0;
    rtp_time = rtp->ts/(double) rtp->time_scale;
    arrival = elapsed * rtp->time_scale;
    transit = arrival - rtp->ts;
    d = transit - rtp->transit;
    rtp->transit = transit;
    if ( d < 0 )
        d = -d;
    rtp->jitter =(int)(((float)(d))/16 + ((float)(rtp->jitter)*15.0/16.0)) ;
    return 1;
}

long long
delta_timeval( struct timeval* start, struct timeval* finish )
{
    long long delta_secs = finish->tv_sec - start->tv_sec;
    long long delta_usecs = finish->tv_usec - start->tv_usec;
    return delta_secs * (long long) 1000000L + delta_usecs;
}

void form_rtcp_sdes(char * data,uint32_t ip_addr){
    int pos = 0;
    data[pos++] = 0x81;
    data[pos++] = RTCP_SDES_PT;
    byte_write16(data,pos,(RTCP_SDES_SIZE/4)-1);
    pos+=2;
    byte_write32(data,pos,RTCP_MFD_SSRC);
    pos+=4;
    //CNAME starts here:
    data[pos++] = 0x01;
    data[pos++] = RTCP_CNAME_SIZE-2;
    byte_write32(data,pos,RTCP_MFD_CNAME);
    pos+=4;
    byte_write32(data,pos,ip_addr);
    pos+=4;
    byte_write16(data,pos,0); //filler
}

#if 0
static int rtcp_send_bye(rtsp_con_player_t *pplayer)
{
	char udp_buf[RTSP_BUFFER_SIZE];
	struct sockaddr_in to;
	socklen_t tolen;
	struct sockaddr_in from;
	socklen_t fromlen;
	rtsp_con_ps_t *pps = NULL;
	rtsp_con_player_t *pplayer = NULL;
	char *p = NULL;
	int ret, i, recv_len; //send_len, 
        char rtcp_rr_pkt[RTSP_BUFFER_SIZE],rtcp_sr_pkt[RTSP_BUFFER_SIZE];
        char *rtcp_st_buf;//rtcp_sdes[RTCP_SDES_SIZE];
        int srlen,len,payload_type,rtcp_payload_len,rtcp_bye=0,rtcp_pos=0;

	if (!pplayer || (g_players[pplayer->g_players_index] != pplayer)) {
		return FALSE;
	}
	pps = pplayer->pps;
	if (!pps || (pps != g_ps[pps->g_ps_index])) {
		return FALSE;
	}

        //Form the sender packets
        p = rtcp_sr_pkt + 4;	// Leave space for inline mode header
        form_rtcp_sr_pkt(&pps->rtcp_s,p);
        //add the sdes packet
        memcpy(p + RTCP_SENDER_SIZE, pps->rtcp_sdes, RTCP_SDES_SIZE);
        ret = RTCP_SENDER_SIZE + RTCP_SDES_SIZE;
        if (rtcp_bye){
            memcpy(p + ret, pps->rtcp_bye, RTCP_BYE_SIZE);
            ret += RTCP_BYE_SIZE;

        }

	recv_len = ret;
	for (pplayer = pps->pplayer_list; pplayer; pplayer = pplayer->next) {
		if (pplayer->pause == TRUE) {
			continue;
		}

		(pplayer->send_data)(pplayer, p, recv_len, RL_DATA_RTCP, i);
	}
	//pthread_mutex_unlock(&rlcon_mutex);
	return TRUE;
}
#endif

int32_t rl_init_bw_smoothing(rl_bw_smooth_ctx_t **pprl_bws)
{
	//Should allocate the bw smoothing state
	UNUSED_ARGUMENT(pprl_bws);
	
	return 0;
}

rl_bw_query_result_t rl_query_bw_smoothing(rl_bw_smooth_ctx_t *prl_bws, char *packet, uint32_t len)
{
	// Take a decision on whether the packet needs to be sent, dropped or queued.
	// The function is currently a stub, the implementation will be done later.
	UNUSED_ARGUMENT(prl_bws);
	UNUSED_ARGUMENT(packet);
	UNUSED_ARGUMENT(len);

	return BWS_SEND_PACKET;
}

int32_t rl_cleanup_bw_smoothing(rl_bw_smooth_ctx_t *prl_bws)
{
	//free bandwidth state related information...

	UNUSED_ARGUMENT(prl_bws);
	
	return 0;
}

int32_t rl_alloc_sess_bw(nkn_interface_t *pns, int32_t max_allowed_bw)
{
	pthread_mutex_lock(&ses_bw_mutex);
	
	if (pns->max_allowed_bw < (max_allowed_bw * 1.2)) {
		//Reject connection due to insufficient bandwidth
		pthread_mutex_unlock(&ses_bw_mutex);
		return -1;
	}	
	pns->max_allowed_bw -= (max_allowed_bw * 1.2);
	pthread_mutex_unlock(&ses_bw_mutex);
	return 0;	
}

int32_t rl_dealloc_sess_bw(nkn_interface_t *pns, int32_t max_allowed_bw)
{
	pthread_mutex_lock(&ses_bw_mutex);
	pns->max_allowed_bw += (max_allowed_bw * 1.2);
	pthread_mutex_unlock(&ses_bw_mutex);
	return 0;
}

int32_t
rl_ps_connect(rtsp_con_ps_t *pps)
{
	struct sockaddr_in srv;
	struct network_mgr * pnm_ps;
	int sfd = 0;
	int ret;
	const namespace_config_t *nsc;

	sfd = pps->rtsp_fd;

	NM_set_socket_nonblock(sfd);

	if(register_NM_socket(sfd,
		(void *)pps,
		rl_ps_epollin,
		rl_ps_epollout,
		rl_ps_epollerr,
		rl_ps_epollhup,
		NULL,
		rl_ps_timeout,
		rtsp_origin_idle_timeout,
		USE_LICENSE_FALSE,
		TRUE) == FALSE) {
		goto return_FALSE;
	}

	nsc = namespace_to_config(pps->pplayer_list->prtsp->nstoken);

	/* Set to transparent proxy */
	if (nsc &&
		(nsc->http_config->origin.select_method == OSS_RTSP_DEST_IP &&   
		 nsc->http_config->origin.u.rtsp.use_client_ip == NKN_TRUE)) {

		if (rtsp_set_transparent_mode_tosocket(sfd, pps) == FALSE) {
			// NOTE: unregister_NM_socket(fd, TRUE) deletes the fd from epoll loop.
			unregister_NM_socket(sfd, TRUE);
			release_namespace_token_t(pps->pplayer_list->prtsp->nstoken);
			goto return_FALSE;
		}
	}
	release_namespace_token_t(pps->pplayer_list->prtsp->nstoken);

	/* Now time to make a socket connection. */
	bzero(&srv, sizeof(srv));
	srv.sin_family = AF_INET;
	srv.sin_addr.s_addr = pps->peer_ip_addr;
	srv.sin_port = htons(pps->port);

	// BUG: We should set to non-blocking socket. so it is non-blocking connect.
	//NM_set_socket_nonblock(cpcon->fd);
	ret = connect(sfd, (struct sockaddr *)&srv, sizeof(struct sockaddr));
	if(ret < 0) {
		if(errno == EINPROGRESS) {
			DBG_LOG(MSG, MOD_RTSP, "connect(0x%x) fd=%d in progress",
                                                pps->peer_ip_addr, sfd);
			if (NM_add_event_epollout(sfd)==FALSE) {
				unregister_NM_socket(sfd, TRUE);
				goto return_FALSE;
                        }
			return 2;
		}
		// NOTE: unregister_NM_socket(fd, TRUE) deletes the fd from epoll loop.
		unregister_NM_socket(sfd, TRUE);
		goto return_FALSE;
	}
	// BUG: This line should be deleted later.
	//NM_set_socket_nonblock(sfd);


#if 0
	// Get the src ip_address through which we have connected
	bzero(&src_addr, sizeof(struct sockaddr_in));
	src_len = sizeof(struct sockaddr_in);
	
	if (getsockname(sfd, (struct sockaddr *)&src_addr,
			(socklen_t *)&src_len) <  0) {
		DBG_LOG(MSG, MOD_RTSP, "getsockname(%s:%d) fd=%d failed", 
			pps->hostname, 
			pps->port, sfd);
		goto return_error;
	}
#endif // 0

	
	DBG_LOG(MSG, MOD_RTSP, "connect(%s:%d) fd=%d succeeded", 
				pps->hostname, 
				pps->port, sfd);
#if 0
	for(i=0; i<MAX_TRACK; i++) {
		pps->rtp[i].own_ip_addr = src_addr.sin_addr.s_addr;
		pps->rtcp[i].own_ip_addr = src_addr.sin_addr.s_addr;
	}
#endif // 0

	/*
	 * Socket has been successfully connected.
	 */
	if(NM_add_event_epollin(sfd) == FALSE) {
		// NOTE: unregister_NM_socket(fd, TRUE) deletes the fd from epoll loop.
		unregister_NM_socket(sfd, TRUE);
		goto return_FALSE;
	}
	//rl_ps_send_method_describe(pps);

	//pps->rtsp_fd = sfd;
	return TRUE;

return_FALSE:
	nkn_close_fd(sfd, RTSP_OM_FD);
	pnm_ps = &gnm[sfd];
	pthread_mutex_unlock(&pnm_ps->mutex);
	pps->rtsp_fd = -1;
	return -1;
}
/* 
 * Common function that could be used either by the live relay code or
 * the Origin Manager code  which creates a new origin side connection 
 *
 * If successful, the network[pps->rtsp_fd].mutex is hold when returned from this function.
 */
	
static rtsp_con_ps_t * rl_create_new_ps(rtsp_cb_t *prtsp, char *uri, char *server, uint16_t port)
{
	int i;
	rtsp_con_ps_t *pps = NULL;
	uint32_t ip;
	int sockfd;
	struct network_mgr * pnm_ps;

	if (!prtsp || !uri || !server || (port==0)) {
		return NULL;
	}

	if (*uri == '*') {
		// Not leading by '/'
		return NULL;
	}

	ip = dns_domain_to_ip(server);
	if (ip == INADDR_NONE) {
		return NULL;
	}
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		DBG_LOG(WARNING, MOD_RTSP, "Failed to create sockfd, errno=%d.", errno);
		return NULL;
	}
	nkn_mark_fd(sockfd, RTSP_OM_FD);

	/*
	 * Create a new RTSP Origin server connection control block.
	 * Reinitizlied the structure to be zero.
	 */
	pps  = (rtsp_con_ps_t *)nkn_malloc_type(sizeof(rtsp_con_ps_t), mod_rtsp_rl_gpc_malloc);
	if(!pps) { 
		nkn_close_fd(sockfd, RTSP_OM_FD);
		return NULL; 
	}
	memset(pps, 0, sizeof(rtsp_con_ps_t));
	pthread_mutex_init( &pps->mutex, NULL );

	/*
	 * found available slot in g_ps.
	 */
	pthread_mutex_lock(&g_ps_mutex);
	for(i=0; i<MAX_RLCON_PS; i++) {
		if(g_ps[i]==NULL) {
			g_ps[i] = pps;
			pps->g_ps_index = i;
			//pthread_mutex_unlock(&g_ps_mutex);
			break;
		}
	}
	if(i == MAX_RLCON_PS) {
		free(pps);
		pthread_mutex_unlock(&g_ps_mutex);
		nkn_close_fd(sockfd, RTSP_OM_FD);
		return NULL;
	}

	if ( rl_init_bw_smoothing(&pps->rl_ps_bws_ctx) < 0) {
		free(pps);
		g_ps[i] = NULL;
		nkn_close_fd(sockfd, RTSP_OM_FD);
		pthread_mutex_unlock(&g_ps_mutex);
		return NULL;
	}
	/*
	 * Fill up the data in prtspcon_publish
	 */
	pps->magic = RLMAGIC_ALIVE;
	pps->flag = 0;	// Clean up all flags
	pps->method = METHOD_UNKNOWN;
	pps->own_ip_addr = prtsp->pns->if_addrv4;
	pps->hostname = nkn_strdup_type(server, mod_rtsp_rl_setup_strdup);
	pps->peer_ip_addr = ip;
	pps->port = port;
        pps->rtsp_fd = sockfd;
	pps->uri = nkn_strdup_type(uri, mod_rtsp_rl_setup_strdup);
	pps->cseq = 1;
	//pps->state = RL_PS_STATE_INIT_DESCRIBE_RECV;
	pthread_mutex_lock(&pps->mutex);

	for(i=0; i<MAX_TRACK; i++) {
		pps->rtp[i].udp_fd = -1;
		pps->rtcp[i].udp_fd = -1;
	}

	if (rtsp_tproxy_enable || CHECK_PPS_FLAG(prtsp, PLAYERF_TPROXY_MODE)) {
		SET_PPS_FLAG(pps, PPSF_TPROXY_MODE);
		pps->send_options 	= rl_ps_tproxy_send_method_options;
		pps->send_describe 	= rl_ps_tproxy_send_method_describe;
		pps->send_setup 	= rl_ps_tproxy_send_method_setup;
		pps->send_play 		= rl_ps_tproxy_send_method_play;
		pps->send_pause 	= rl_ps_tproxy_send_method_pause;
		pps->send_teardown 	= rl_ps_tproxy_send_method_teardown;
		pps->send_get_parameter = rl_ps_tproxy_send_method_get_parameter;
		pps->send_set_parameter = rl_ps_tproxy_send_method_set_parameter;

		pps->recv_options 	= rl_ps_tproxy_recv_method_options;
		pps->recv_describe 	= rl_ps_tproxy_recv_method_describe;
		pps->recv_setup 	= rl_ps_tproxy_recv_method_setup;
		pps->recv_play 		= rl_ps_tproxy_recv_method_play;
		pps->recv_pause 	= rl_ps_tproxy_recv_method_pause;
		pps->recv_teardown 	= rl_ps_tproxy_recv_method_teardown;
		pps->recv_get_parameter = rl_ps_tproxy_recv_method_get_parameter;
		pps->recv_set_parameter = rl_ps_tproxy_recv_method_set_parameter;
	}
	else {
		UNSET_PPS_FLAG(pps, PPSF_TPROXY_MODE);
		pps->send_options 	= rl_ps_live_send_method_options;
		pps->send_describe 	= rl_ps_live_send_method_describe;
		pps->send_setup 	= rl_ps_live_send_method_setup;
		pps->send_play 		= rl_ps_live_send_method_play;
		pps->send_pause 	= rl_ps_live_send_method_pause;
		pps->send_teardown 	= rl_ps_live_send_method_teardown;
		pps->send_get_parameter = rl_ps_live_send_method_get_parameter;
		pps->send_set_parameter = rl_ps_live_send_method_set_parameter;

		pps->recv_options 	= rl_ps_live_recv_method_options;
		pps->recv_describe 	= rl_ps_live_recv_method_describe;
		pps->recv_setup 	= rl_ps_live_recv_method_setup;
		pps->recv_play 		= rl_ps_live_recv_method_play;
		pps->recv_pause 	= rl_ps_live_recv_method_pause;
		pps->recv_teardown 	= rl_ps_live_recv_method_teardown;
		pps->recv_get_parameter = rl_ps_live_recv_method_get_parameter;
		pps->recv_set_parameter = rl_ps_live_recv_method_set_parameter;
	}
	pthread_mutex_unlock(&g_ps_mutex);

	/* return lock */
	pnm_ps = &gnm[pps->rtsp_fd];
	pthread_mutex_lock(&pnm_ps->mutex);

	DBG_LOG(MSG, MOD_RTSP, "New PS for URI=%s, Host=%s, port=%d, fd=%d", 
			pps->uri, pps->hostname, pps->port, pps->rtsp_fd);
	return pps;
}

void rl_set_channel_id(rtsp_con_ps_t *pps, char * trackID, uint8_t channel_id)
{
	int i = 0;

	for (i = 0; i < pps->sdp_info.num_trackID; i++) {
		if (strcmp(pps->sdp_info.track[i].trackID, trackID) == 0) {
			pps->sdp_info.track[i].channel_id = channel_id;
			return;
		}
	}
	
	return;
}

int rl_get_track_num(rtsp_con_ps_t *pps, char * trackID)
{
	int i = 0;

	for (i = 0; i < pps->sdp_info.num_trackID; i++) {
		if (strcmp(pps->sdp_info.track[i].trackID, trackID) == 0) {
			return i;
		}
	}
	return -1;
}

static void rl_ps_relay_rtp_data(rtsp_con_ps_t *pps)
{
	rtsp_con_player_t * pplayer;
	rls_track_t *pt = NULL;
	int i, track;
	int type = RL_DATA_NONE;

	if (pps->rtsp.partialRTPDataRcvd) {
		return;
	}
	
	if (pps->rtsp.rtp_data.rtp_buf_len == 0) {
		return;
	}

	SET_PPS_FLAG(pps, PPSF_KEEP_ALIVE);
	if (ISSET_STATE_FLAG(pps, RL_BROADCAST_STATE_PAUSE)) {
		RESET_STATE_FLAG(pps, RL_BROADCAST_STATE_PAUSE);
	}

	/* Find out track ID from channel ID.
	 */
	for (i = 0; i < pps->sdp_info.num_trackID ; i++) {
		if (pps->rtp[i].own_channel_id == pps->rtsp.rtp_data.channel_id) {
			pt = &pps->rtp[i];
			type = RL_DATA_RTP;
			track = i;
			break;
		}
		if (pps->rtcp[i].own_channel_id == pps->rtsp.rtp_data.channel_id) {
			pt = &pps->rtcp[i];
			type = RL_DATA_RTCP;
			track = i;
			break;
		}
	}

	/* Update RTP stats
	 */
	if (type == RL_DATA_RTP) {
		pps->rtp[i].rtp.time_scale = pps->rtp[i].sample_rate;
		rtcp_rtp_state_update(&pps->rtp[i].rtp, pps->rtsp.rtp_data.rtp_buf + 4);
	} 
	else if (type == RL_DATA_RTCP){
		char *p = NULL;
		int ret, send_len; 
		char rtcp_rr_pkt[RTP_BUFFER_SIZE];
		int len, rtcp_bye=0;

		// Send a receiver report to the publisher.
		len = pps->rtsp.rtp_data.rtp_buf_len - 4;
		p = pps->rtsp.rtp_data.rtp_buf + 4;
		if (pps->rtp[i].rtp.rtcp_pkts == 0) {
			pps->rtp[i].rtp.rtcp_pkts = 1;
			form_rtcp_sdes(pps->rtcp_sdes, pps->own_ip_addr);
			pps->rtcp_bye[0] = 0x81;
			pps->rtcp_bye[1] = 203;
			pps->rtcp_bye[2] = 0;
			pps->rtcp_bye[3] = 1;
			byte_write32(pps->rtcp_bye, 4, RTCP_MFD_SSRC);
		}
		rtcp_bye = 0;
		ret = 4;

		rtcp_pkt_dissect_t rtcp_dissect;
		init_rtcp_pkt_dissect(&rtcp_dissect);
		if (rtcp_dissector((uint8_t*)p, len, 0, &rtcp_dissect) == 0) {
			if (rtcp_dissect.sr != NULL) {
				read_rtcp_sr_pkt(&pps->rtcp_s, (char*)rtcp_dissect.sr);
				form_rtcp_rr_pkt(pps->rtp[i].rtp, pps->rtcp_s, &pps->rtcp_r, &rtcp_rr_pkt[ret]);
				ret += RTCP_RR_SIZE;
			}	
			if (rtcp_dissect.sdes != NULL) {
				memcpy(&rtcp_rr_pkt[ret],pps->rtcp_sdes,RTCP_SDES_SIZE);
				ret += RTCP_SDES_SIZE;
			}
			if (rtcp_dissect.bye != NULL) {
				memcpy(&rtcp_rr_pkt[ret],pps->rtcp_bye,RTCP_BYE_SIZE);
				ret += RTCP_BYE_SIZE;
				rtcp_bye = 1;
			}
		}

		if (ret > 4) {
			send_len = ret;
			rtcp_rr_pkt[0] = '$';
			rtcp_rr_pkt[1] = pps->rtsp.rtp_data.channel_id;
			byte_write16(rtcp_rr_pkt, 2, send_len - 4);
			ret = send(pps->rtsp_fd, rtcp_rr_pkt, send_len, 0);
			if (!CHECK_PPS_FLAG(pps, PPSF_INTERNAL_PLAYER)) {
				glob_rtcp_tot_udp_rr_packets_sent++;
			}
		}
	}
	else {
		return;
	}

	/* Relay data to players
	 */
	pthread_mutex_lock(&pps->rtsp.mutex);
	pplayer = pps->pplayer_list;
	if (pplayer && (pplayer->pause != TRUE)) {
		(pplayer->send_data)(pplayer, 
			pps->rtsp.rtp_data.rtp_buf,
			pps->rtsp.rtp_data.rtp_buf_len, 
			type, 		//RL_DATA_INLINE, 
			track); 	//pps->rtsp.rtp_data.channel_id);
#if 0
		printf( "Inline data len=%d, type=%d, track=%d, data %2x %2x %2x %2x\r\n",
			pps->rtsp.rtp_data.rtp_buf_len, type, track,
			(int) *pps->rtsp.rtp_data.rtp_buf,
			(int) *(pps->rtsp.rtp_data.rtp_buf+1),
			(int) *(pps->rtsp.rtp_data.rtp_buf+2),
			(int) *(pps->rtsp.rtp_data.rtp_buf+3) );
#endif
		if (pplayer->player_type == RL_PLAYER_TYPE_FAKE_PLAYER)
			AO_fetch_and_add(&glob_rtsp_tot_size_to_cache, pps->rtsp.rtp_data.rtp_buf_len);
		else
			AO_fetch_and_add(&glob_rtsp_tot_size_from_origin, pps->rtsp.rtp_data.rtp_buf_len);
	}
	if (pplayer) {
	for (pplayer = pplayer->next; pplayer ; pplayer = pplayer->next) {
		if (pplayer->pause == TRUE) {
			continue;
		}
		(pplayer->send_data)(pplayer, 
			pps->rtsp.rtp_data.rtp_buf,
			pps->rtsp.rtp_data.rtp_buf_len, 
			type, 		//RL_DATA_INLINE, 
			track); 	//pps->rtsp.rtp_data.channel_id);
		AO_fetch_and_add(&glob_rtsp_tot_size_from_cache, pps->rtsp.rtp_data.rtp_buf_len);
	}
	}
	
	pthread_mutex_unlock(&pps->rtsp.mutex);
	return;
}

rtsp_con_ps_t * rl_ps_list_check_uri(char * uri) 
{
	ps_fetch_list_t *ptr;
	
	pthread_mutex_lock(&g_ps_list_mutex);

	ptr = g_ps_list;
	while (ptr) {
		if (strcmp(ptr->uri, uri) == 0) {
			//glob_rtsp_orp_req_same_object++;
			pthread_mutex_unlock(&g_ps_list_mutex);
			return ptr->pps;
		}
		ptr = ptr->next;
	}
	pthread_mutex_unlock(&g_ps_list_mutex);

	return NULL;
}

rtsp_con_ps_t * rl_ps_list_check_and_add_uri(char * uri, int urilen, rtsp_con_ps_t * pps)
{
	ps_fetch_list_t *ptr;
	
	pthread_mutex_lock(&g_ps_list_mutex);

	ptr = g_ps_list;
	while (ptr) {
		if (strcmp(ptr->uri, uri) == 0) {
			//glob_rtsp_orp_req_same_object++;
			pthread_mutex_unlock(&g_ps_list_mutex);
			return ptr->pps;
		}
		ptr = ptr->next;
	}

	ptr = (ps_fetch_list_t *)nkn_malloc_type(sizeof(ps_fetch_list_t), mod_rtsp_ps_list);
	if (!ptr) { 
		pthread_mutex_unlock(&g_ps_list_mutex);
		return NULL; 
	}
	memset((char *)ptr, 0, sizeof(ps_fetch_list_t));
	ptr->uri    = uri;
	ptr->urilen = urilen;
	ptr->pps    = pps;

	ptr->next = g_ps_list;
	g_ps_list = ptr;
	pthread_mutex_unlock(&g_ps_list_mutex);

	return NULL;
}

int rl_ps_list_add_uri(char * uri, int urilen, rtsp_con_ps_t * pps)
{
	ps_fetch_list_t *ptr;

	ptr = (ps_fetch_list_t *)nkn_malloc_type(sizeof(ps_fetch_list_t), mod_rtsp_ps_list);
	if (!ptr) { 
		return -1; 
	}
	memset((char *)ptr, 0, sizeof(ps_fetch_list_t));
	ptr->uri    = uri;
	ptr->urilen = urilen;
	ptr->pps    = pps;

	pthread_mutex_lock(&g_ps_list_mutex);
	ptr->next = g_ps_list;
	g_ps_list = ptr;
	pthread_mutex_unlock(&g_ps_list_mutex);

	return 1;
}

int rl_ps_list_remove_uri(char * uri)
{
	ps_fetch_list_t *ptr, *ptr_next;

	/* Check g_ps_list after acquiring lock.
	 * At times it becomes NULL after lock.
	 * BZ 5775
	 */
	pthread_mutex_lock(&g_ps_list_mutex);
	if (g_ps_list == NULL) {
		pthread_mutex_unlock(&g_ps_list_mutex);
		return 0;
	}
	ptr = g_ps_list;
	
	if (strncmp(ptr->uri, uri, ptr->urilen) == 0) {
		g_ps_list = ptr->next;
		free(ptr);
		pthread_mutex_unlock(&g_ps_list_mutex);
		return 1;
	}
	while (ptr->next) {
		if (strncmp(ptr->next->uri, uri, ptr->urilen) == 0) {
			ptr_next = ptr->next;
			ptr->next = ptr_next->next;
			free(ptr_next);
			pthread_mutex_unlock(&g_ps_list_mutex);
			return 1;
		}
		ptr = ptr->next;
	}
	pthread_mutex_unlock(&g_ps_list_mutex);

	return 0;
}

int lock_ps( rtsp_con_ps_t * pps, int locks_held)
{
	if (!CHECK_LOCK(locks_held, LK_PS)) {
		pthread_mutex_lock(&pps->rtsp.mutex);
		return (locks_held | LK_PS);
	}
	return(locks_held);
}

int lock_pl( rtsp_con_player_t * pplayer, int locks_held)
{
	if (!CHECK_LOCK(locks_held, LK_PL)) {
		pthread_mutex_lock(&pplayer->prtsp->mutex);
		return (locks_held | LK_PL);
	}
	return(locks_held);
}

#if 0
int trylock_ps( rtsp_con_ps_t * pps, int locks_held)
{
	pthread_mutex_t *ppsg_m = NULL, *pps_m = NULL, *pplg_m = NULL;
	
	if (pthread_mutex_trylock(&pplayer->prtsp->mutex) != 0) {
		if (pplayer->pps && CHECK_LOCK(locks_held, LK_PS_GNM)) {
			ppsg_m = &gnm[pplayer->pps->rtsp_fd].mutex;
		}
		if (pplayer->pps && CHECK_LOCK(locks_held, LK_PS)) {
			pps_m = &pplayer->pps->rtsp.mutex;
		}
		if (pplayer->rtsp_fd != -1 && CHECK_LOCK(locks_held, LK_PL_GNM)) {
			pplg_m = &gnm[pplayer->rtsp_fd].mutex;
		}
		while (pthread_mutex_trylock(&pplayer->prtsp->mutex) != 0) {
			if (ppsg_m) pthread_mutex_unlock(ppsg_m);
			if (pps_m) pthread_mutex_unlock(pps_m);
			if (pplg_m) pthread_mutex_unlock(pplg_m);
			sleep(0);
			if (ppsg_m) pthread_mutex_lock(ppsg_m);
			if (pps_m) pthread_mutex_lock(pps_m);
			if (pplg_m) pthread_mutex_lock(pplg_m);
		}
	}
	return locks_held | LK_PL;
}
#endif

int trylock_pl( rtsp_con_player_t * pplayer, int locks_held)
{
	pthread_mutex_t *ppsg_m = NULL, *pps_m = NULL, *pplg_m = NULL;

	if (CHECK_LOCK(locks_held, LK_PL))
		return locks_held;
	if (pthread_mutex_trylock(&pplayer->prtsp->mutex) != 0) {
		if (pplayer->pps && CHECK_LOCK(locks_held, LK_PS_GNM)) {
			ppsg_m = &gnm[pplayer->pps->rtsp_fd].mutex;
		}
		if (pplayer->pps && CHECK_LOCK(locks_held, LK_PS)) {
			pps_m = &pplayer->pps->rtsp.mutex;
		}
		if (pplayer->rtsp_fd != -1 && CHECK_LOCK(locks_held, LK_PL_GNM)) {
			pplg_m = &gnm[pplayer->rtsp_fd].mutex;
		}
		while (pthread_mutex_trylock(&pplayer->prtsp->mutex) != 0) {
			if (ppsg_m) pthread_mutex_unlock(ppsg_m);
			if (pps_m) pthread_mutex_unlock(pps_m);
			if (pplg_m) pthread_mutex_unlock(pplg_m);
			sleep(0);
			if (ppsg_m) pthread_mutex_lock(ppsg_m);
			if (pps_m) pthread_mutex_lock(pps_m);
			if (pplg_m) pthread_mutex_lock(pplg_m);
		}
	}
	return locks_held | LK_PL;
}


