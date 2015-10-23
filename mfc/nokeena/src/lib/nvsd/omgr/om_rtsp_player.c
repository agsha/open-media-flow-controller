/*
 * OM RTSP Player (orp) module
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <stdint.h>
#include <sys/prctl.h>
#include <uuid/uuid.h>

/* NKN includes */
#include "network.h"
#include "nkn_debug.h"
#include "nkn_errno.h"
#include "nkn_stat.h"
#include "nkn_cod.h"
#include "rtsp_live.h"
#include "rtsp_server.h"
#include "rtsp_header.h"
#include "rtsp_vpe_nkn_fmt.h"
#include "nkn_vpemgr_defs.h"
#include "vfs_defs.h"


NKNCNT_DEF(rtsp_orp_num_object_req, uint64_t, "", "req objects by internal player")
NKNCNT_DEF(rtsp_orp_num_object_complete, uint64_t, "", "complete objects download")
NKNCNT_DEF(rtsp_orp_req_same_object, uint64_t, "", "req same object")
NKNCNT_DEF(rtsp_orp_num_conversion_started, uint64_t, "", "num of conversion started")
NKNCNT_DEF(rtsp_orp_num_conversion_skipped, uint64_t, "", "num of conversion skipped")
NKNCNT_DEF(rtsp_orp_cache_age_less_than_duration, uint64_t, "", "cache age less than duration")
NKNCNT_DEF(rtsp_orp_num_fake_player, uint64_t, "", "cache age less than duration")
NKNCNT_DEF(rtsp_orp_create_fake_player, uint64_t, "", "cache age less than duration")
NKNCNT_DEF(rtsp_orp_close_fake_player, uint64_t, "", "cache age less than duration")

NKNCNT_DEF(rtsp_orp_cache_buffer_overflow, uint64_t, "", "cache buffer overflow")
NKNCNT_DEF(rtsp_orp_data_ready_non_play, uint64_t, "", "orp_data_ready_non_play");

#define LIVE_VIDEO_CACHEABLE_DURATION 600

static pthread_mutex_t orp_mutex = PTHREAD_MUTEX_INITIALIZER;
static orp_fetch_uri_t * g_orp_list = NULL;

extern int glob_rtsp_format_convert_enabled;
extern struct vfs_funcs_t vfs_funs;

extern int rl_player_process_rtsp_request(int sockfd, rtsp_con_player_t * pplayer, int locks_held);
extern rtsp_con_player_t * rtsp_new_pplayer(nkn_interface_t * pns, unsigned long long fOurSessionId);
extern void rl_player_closed(rtsp_con_player_t * pplayer, int locks_held);
extern int rl_ps_closed(rtsp_con_ps_t * pps, rtsp_con_player_t * no_callback_for_this_pplayer, int locks_held);
extern int trylock_pl( rtsp_con_player_t * pplayer, int locks_held);


int orp_fsm(rtsp_con_player_t * pplayer, int locks_held);
int orp_write_sdp_to_tfm(rtsp_con_player_t * pplayer);
int orp_send_data_to_tfm_udp(rtsp_con_player_t * pplayer, char *buf, int len, int type, int track);
int orp_send_data_to_tfm_tcp(rtsp_con_player_t * pplayer, char *buf, int len, int type, int track);

int nkn_copy_orp_data(void *in_proto_data, void **out_proto_data);
void nkn_free_orp_data(void *proto_data);

void orp_teardown_state(void* clientData);
void orp_teardown(void* clientData, int locks_held);
int orp_close(rtsp_con_player_t * pplayer, int lock_held);
void *orp_add_new_video(nkn_interface_t * pns, uint32_t dip, uint16_t dport, 
		char * uri, char *cache_uri,
		namespace_token_t nstoken, char *vfs_filename);
int orp_create_fake_pplayer(orp_fetch_uri_t * porp);


orp_fetch_uri_t *orp_look_up_uri(char * uri);
orp_fetch_uri_t *orp_look_up_physid(char * physid);
void orp_add_to_list(orp_fetch_uri_t *porp);
orp_fetch_uri_t *orp_remove_from_list_uri(char * uri);
orp_fetch_uri_t *orp_free_from_list(orp_fetch_uri_t *porp);


void orp_teardown_state(void* clientData)
{
	rtsp_con_player_t * pplayer = (rtsp_con_player_t *)clientData;
	struct network_mgr * pnm_ps = NULL;
	int locks = 0;

	if (!pplayer->prtsp) return;

	if (pplayer->fpl_state <= RL_FPL_STATE_PLAYING) {
		pplayer->fpl_state = RL_FPL_STATE_SEND_TEARDOWN;
		if (pplayer->pps)
			pnm_ps = &gnm[pplayer->pps->rtsp_fd];
		if (pnm_ps) { 
			pthread_mutex_lock(&pnm_ps->mutex); 
			SET_LOCK(locks, LK_PS_GNM);
		}
		orp_fsm(pplayer, locks);
		if (pnm_ps) pthread_mutex_unlock(&pnm_ps->mutex);
	}
	return;
}

void orp_teardown(void* clientData, int locks_held)
{
	rtsp_con_player_t * pplayer = (rtsp_con_player_t *)clientData;
	struct in_addr in;
	//char cmd[1024];
	orp_fetch_uri_t * porp;
	nkn_attr_t *pnknattr;
	nkn_buffer_t *attrbuf;
	rvnf_seek_box_t *rvnf_sb;
	int ret;
	char *hostname;
	int packets_lost = 0;
	char *uri;
	int release_attrbuf = 0;
	int locks = locks_held;

	/* BUG: Should get mutex first */
	if (!pplayer->prtsp) return;

	if (CHECK_PLAYER_FLAG(pplayer, PLAYERF_TEARDOWN) || CHECK_PLAYER_FLAG(pplayer, PLAYERF_CLOSE)) {
		return;
	}
	if (!CHECK_LOCK(locks_held, LK_PL)) {
		pthread_mutex_lock(&pplayer->prtsp->mutex);
		SET_LOCK(locks, LK_PL);
	}
	SET_PLAYER_FLAG(pplayer, PLAYERF_TEARDOWN);
	DBG_LOG(MSG, MOD_RTSP, "Internal player teardown (pps fd=%d player fd=%d", 
		pplayer->pps ? pplayer->pps->rtsp_fd : -1, 
		pplayer->rtsp_fd);

	/* Unschedule teardown task first
	 */
	if (pplayer->fteardown_task) {
		nkn_unscheduleDelayedTask(pplayer->fteardown_task);
		pplayer->fteardown_task = NULL;
	}

	pthread_mutex_lock(&pplayer->buf_mutex);
	if (CHECK_PLAYER_FLAG(pplayer, PLAYERF_SEND_BUFFER)) {
		if ((pplayer->ob_wroff + sizeof(rvnf_seek_box_t)) <
				(32 * CM_MEM_PAGE_SIZE)) {
			rvnf_sb = (rvnf_seek_box_t *) &pplayer->out_buffer[pplayer->ob_wroff];
			/* Store end time in the last seek box also so that
			 * duration could be found for live streams.
			 */
			rvnf_sb->end_time = pplayer->elapsed_time;
			rvnf_sb->this_box_data_size = 0;
			pplayer->ob_wroff   += sizeof(rvnf_seek_box_t);
			pplayer->ob_datasz  += sizeof(rvnf_seek_box_t);
			pplayer->ob_tot_len += sizeof(rvnf_seek_box_t); 
			//printf( "SDP tot_len=%ld data_size=%d rd_off=%d wr_off=%d\n",
			//	pplayer->ob_tot_len, pplayer->ob_datasz, 
			//	pplayer->ob_rdoff, pplayer->ob_wroff );
		}
		else {
			assert(0);
		}
		if(pplayer->p_mm_task->in_attr) {
			attrbuf = pplayer->p_mm_task->in_attr;
		} else {
			attrbuf = nkn_buffer_get(&pplayer->uol, NKN_BUFFER_ATTR);
			release_attrbuf = 1;
		}
		if (attrbuf) {
			pnknattr = (nkn_attr_t *) nkn_buffer_getcontent(attrbuf);
			pnknattr->content_length = pplayer->ob_tot_len;
			nkn_attr_reset_streaming(pnknattr);
			DBG_LOG(MSG, MOD_RTSP, "Update content length=%ld",
				pnknattr->content_length );
		}
		if (pplayer->pseekbox) {
			rvnf_sb = (rvnf_seek_box_t *) pplayer->pseekbox;
			rvnf_sb->this_box_data_size = pplayer->ob_len;
			rvnf_sb->end_time           = pplayer->elapsed_time;
		}
		if (pplayer->uol.cod) {
			nkn_cod_close(pplayer->uol.cod, NKN_COD_ORIGIN_MGR);
			pplayer->uol.cod = NKN_COD_NULL;
		}
		ret = RTSP_OM_get_ret_task(pplayer->p_mm_task, 
				pplayer->out_buffer + pplayer->ob_rdoff,
				pplayer->ob_datasz, &pplayer->ob_tot_written);
		//pplayer->ob_tot_written += ret;
		pplayer->ob_rdoff  += ret;
		pplayer->ob_datasz -= ret;
		if (pplayer->ob_datasz == 0) {
			pplayer->ob_rdoff = 0;
			pplayer->ob_wroff = 0;
		}
		UNSET_PLAYER_FLAG(pplayer, PLAYERF_SEND_BUFFER);

		/* if attrbuf was got using nkn_buffer_get
		 * release the buffer to avoid memory leak in long run
		 */
		if (attrbuf && release_attrbuf) {
			release_attrbuf = 0;
			nkn_buffer_release(attrbuf);
		}
		//pplayer->p_mm_task->in_proto_data = NULL;
	}
	pthread_mutex_unlock(&pplayer->buf_mutex);

	/* Store required info before calling rl_player_process_rtsp_request
	 * as the pplayer and pps could be freed, by the time it returns.
	 */
	uri = alloca(strlen(pplayer->uri)+2);
	strcpy(uri, pplayer->uri);
	glob_rtsp_orp_num_object_complete++;
	porp = orp_remove_from_list_uri(uri);	// porp->pmutex is locked

	if (pplayer->pps) {
		packets_lost = pplayer->pps->rtp[0].rtp.num_pkts_lost + pplayer->pps->rtp[1].rtp.num_pkts_lost;
		hostname = alloca(strlen(pplayer->pps->hostname)+2);
		strcpy(hostname, pplayer->pps->hostname);
		in.s_addr = pplayer->own_ip_addr;
		pplayer->prtsp->cb_reqlen = snprintf(&pplayer->prtsp->cb_reqbuf[0],
			RTSP_REQ_BUFFER_SIZE,
			"TEARDOWN rtsp://%s%s/ RTSP/1.0\r\n"
			"CSeq: 6\r\n"
			"Session: %llu\r\n"
			"User-Agent: VLC media player (LIVE555 Streaming Media v2010.01.07)\r\n"
			"\r\n",
			inet_ntoa(in),
			pplayer->uri,
			pplayer->session);
		/* If packets are lost do not start conversion
		 */
		if (packets_lost) {
			DBG_LOG(MSG, MOD_RTSP, "Packets lost=%d (pps fd=%d player fd=%d) URI: %s", 
				packets_lost,
				pplayer->pps ? pplayer->pps->rtsp_fd : -1, 
				pplayer->rtsp_fd,
				uri);
			glob_rtsp_orp_num_conversion_skipped++;
		}
		//pthread_mutex_unlock(&pplayer->prtsp->mutex);
		rl_player_process_rtsp_request(pplayer->prtsp->fd, pplayer, locks);
	}
	else {
		orp_close(pplayer, locks);
		packets_lost = 100;
	}

	if (porp) {
		if (!packets_lost && glob_rtsp_format_convert_enabled == NKN_TRUE) {
			////KARTHIK
			char kuri[256];
			strcpy(kuri,"/nkn/mnt/fuse");
			strcat(kuri, porp->uri);
			submit_vpemgr_rtsp_request( hostname, strlen(hostname), kuri, strlen(kuri));
			glob_rtsp_orp_num_conversion_started++;
		}
#if 0
		/*
		 * free porp structure
		 */
		//pthread_mutex_lock(porp->pmutex);
		pmutex = porp->pmutex;
		free(porp->uri); 
		free(porp->cache_uri); 
		porp->uri = NULL;
		porp->cache_uri  = NULL;
		porp->pplayer = NULL;
		porp->pmutex  = NULL;
		porp->magic   = RLMAGIC_DEAD;
		free(porp);
		glob_rtsp_orp_num_fake_player--;
		pthread_mutex_unlock(pmutex);
		free(pmutex);
#endif
	}
}

int orp_close(rtsp_con_player_t * pplayer, int locks_held)
{
	orp_fetch_uri_t * porp;
	char *uri;
	int locks = locks_held;

	if (!pplayer->prtsp) return -1;
	if (CHECK_PLAYER_FLAG(pplayer, PLAYERF_TEARDOWN) || CHECK_PLAYER_FLAG(pplayer, PLAYERF_CLOSE)) {
		return -1;
	}
	locks = trylock_pl(pplayer, locks_held);

	SET_PLAYER_FLAG(pplayer, PLAYERF_TEARDOWN);
	if (pplayer->fpl_state < RL_FPL_STATE_WAIT_TEARDOWN)
		pplayer->fpl_state = RL_FPL_STATE_WAIT_TEARDOWN;
	if (pplayer->fteardown_task) {
		nkn_unscheduleDelayedTask(pplayer->fteardown_task);
		pplayer->fteardown_task = NULL;
	}
	DBG_LOG(MSG, MOD_RTSP, "Internal player close (pps fd=%d player fd=%d)  state=%d", 
		pplayer->pps ? pplayer->pps->rtsp_fd : -1, 
		pplayer->rtsp_fd, pplayer->fpl_state);

	if (pplayer->uri) {
		uri = alloca(strlen(pplayer->uri)+2);
		strcpy(uri, pplayer->uri);
	}
	else {
		uri = alloca(2);
		*uri = 0;
	}

	pthread_mutex_lock(&pplayer->buf_mutex);
	if (CHECK_PLAYER_FLAG(pplayer, PLAYERF_SEND_BUFFER)) {
		if (pplayer->uol.cod) {
			nkn_cod_close(pplayer->uol.cod, NKN_COD_ORIGIN_MGR);
			pplayer->uol.cod = NKN_COD_NULL;
		}
		RTSP_OM_get_ret_task(pplayer->p_mm_task, 
				NULL,
				0, &pplayer->ob_tot_written);
		//pplayer->ob_tot_written = pplayer->ob_tot_len;
		UNSET_PLAYER_FLAG(pplayer, PLAYERF_SEND_BUFFER);
		//pplayer->p_mm_task->in_proto_data = NULL;
	}
	pthread_mutex_unlock(&pplayer->buf_mutex);

	/*
	 * free porp structure
	 */
	porp = orp_remove_from_list_uri(uri);	// porp->pmutex is locked
#if 0
	if (porp) {
		//pthread_mutex_lock(porp->pmutex);
		pmutex = porp->pmutex;
		free(porp->uri); 
		free(porp->cache_uri); 
		porp->uri = NULL;
		porp->cache_uri  = NULL;
		porp->pplayer = NULL;
		porp->pmutex  = NULL;
		porp->magic   = RLMAGIC_DEAD;
		free(porp);
		glob_rtsp_orp_num_fake_player--;
		pthread_mutex_unlock(pmutex);
		free(pmutex);
	}
#endif
	if (pplayer->pps && pplayer->pps->pplayer_list == pplayer && 
			pplayer->pps->pplayer_list->next == NULL) {
		DBG_LOG(MSG, MOD_RTSP, "Last player, close ps connection %p", pplayer->pps ); 
		rl_ps_closed(pplayer->pps, pplayer, locks);
	}
	//pthread_mutex_unlock(&pplayer->prtsp->mutex);
	if (pplayer) {
		DBG_LOG(MSG, MOD_RTSP, "Free/close internal RTSP player %p", pplayer ); 
		glob_rtsp_orp_close_fake_player++;
		rl_player_closed(pplayer, locks);
	}
	return 0;
}


int orp_fsm(rtsp_con_player_t * pplayer, int locks_held)
{
	struct in_addr in;
	int64_t tot_npt;
	int ret;

	if (!pplayer) return -1;
	pplayer->prtsp->fd = -1;
	in.s_addr = pplayer->own_ip_addr;

	while (1) {
	DBG_LOG(MSG, MOD_RTSP, "pplayer=%p State=%d (pps fd=%d)", 
		pplayer,
		pplayer->fpl_state,
		pplayer->pps ? pplayer->pps->rtsp_fd : -1 );
	pplayer->fpl_last_state = pplayer->fpl_state;
	switch (pplayer->fpl_state) {
	case RL_FPL_STATE_INIT:
		pplayer->fpl_state = RL_FPL_STATE_SEND_OPTIONS;
		break;

	case RL_FPL_STATE_SEND_OPTIONS:
		pplayer->prtsp->cb_reqlen = snprintf(&pplayer->prtsp->cb_reqbuf[0], 
			RTSP_REQ_BUFFER_SIZE,
			"OPTIONS rtsp://%s%s RTSP/1.0\r\n"
			"CSeq: %d\r\n"
			"User-Agent: VLC media player (LIVE555 Streaming Media v2010.01.07)\r\n"
			"\r\n",
			inet_ntoa(in),
			pplayer->uri,
			++pplayer->req_cseq);
		pplayer->fpl_state = RL_FPL_STATE_WAIT_OPTIONS;
		ret = rl_player_process_rtsp_request(pplayer->prtsp->fd, pplayer, locks_held);
		if (ret == 0 || ret == 2) return 1;
		if (ret == -1) pplayer->fpl_state = RL_FPL_STATE_ERROR;
		break;

	case RL_FPL_STATE_WAIT_OPTIONS:
		if (pplayer->prtsp->status == 200) {
			pplayer->fpl_state = RL_FPL_STATE_SEND_DESCRIBE;
		}
		else {
			pplayer->fpl_state = RL_FPL_STATE_ERROR;
		}
		break;

	case RL_FPL_STATE_SEND_DESCRIBE:
		if (pplayer->pps && CHECK_PPS_FLAG(pplayer->pps, PPSF_LIVE_VIDEO) &&
				!CHECK_PLAYER_FLAG(pplayer, PLAYERF_LIVE_CACHEABLE)) {
			DBG_LOG(MSG, MOD_RTSP, "Non-Cacheable Live stream (pps fd=%d)", 
				pplayer->pps->rtsp_fd );
			pplayer->fpl_state = RL_FPL_STATE_ERROR;
			break;
		}
		pplayer->prtsp->cb_reqlen = snprintf(&pplayer->prtsp->cb_reqbuf[0],
			RTSP_REQ_BUFFER_SIZE,
			"DESCRIBE rtsp://%s%s RTSP/1.0\r\n"
			"CSeq: %d\r\n"
			"Accept: application/sdp\r\n"
			"User-Agent: VLC media player (LIVE555 Streaming Media v2010.01.07)\r\n"
			"\r\n",
			inet_ntoa(in),
			pplayer->uri,
			++pplayer->req_cseq);
		pplayer->fpl_state = RL_FPL_STATE_WAIT_DESCRIBE;
		ret = rl_player_process_rtsp_request(pplayer->prtsp->fd, pplayer, locks_held);
		if (ret == 0 || ret == 2) return 1;
		if (ret == -1) pplayer->fpl_state = RL_FPL_STATE_ERROR;
		break;

	case RL_FPL_STATE_WAIT_DESCRIBE:
		if (pplayer->prtsp->status == 200) {
			tot_npt = (int64_t)(atof(pplayer->pps->sdp_info.npt_stop) + 0.5);
			if (CHECK_PPS_FLAG(pplayer->pps, PPSF_LIVE_VIDEO)) {
				if (!CHECK_PLAYER_FLAG(pplayer, PLAYERF_LIVE_CACHEABLE) ||
						pplayer->cache_age < LIVE_VIDEO_CACHEABLE_DURATION) {
					pplayer->fpl_state = RL_FPL_STATE_ERROR;
					break;
				}
			}
			else {
				/* If cache cache is less than video duration, do not get the
				 * video from origin
				 */
				if (pplayer->cache_age <= tot_npt)
				{
					DBG_LOG(MSG, MOD_RTSP, "Cache age is less than video duration,"
						"what is the point in caching this ? (%s)", 
						pplayer->uri);
					glob_rtsp_orp_cache_age_less_than_duration++;
					pplayer->fpl_state = RL_FPL_STATE_ERROR;
					break;
				}
			}
			pplayer->fpl_state = RL_FPL_STATE_SEND_SETUP;
			//mime_hdr_copy_headers(&pplayer->hdr, &pplayer->pps->rtsp.hdr);
		}
		else {
			pplayer->fpl_state = RL_FPL_STATE_ERROR;
		}
		break;

	case RL_FPL_STATE_SEND_SETUP:
		if (pplayer->cur_trackId >= pplayer->pps->sdp_info.num_trackID) {
			pplayer->fpl_state = RL_FPL_STATE_WAIT_SETUP;
			break;
		}
		if (pplayer->cur_trackId == 0) {
			pplayer->prtsp->cb_reqlen = snprintf(&pplayer->prtsp->cb_reqbuf[0],
				RTSP_REQ_BUFFER_SIZE,
				"SETUP rtsp://%s%s/%s RTSP/1.0\r\n"
				"CSeq: %d\r\n"
				"Transport: RTP/AVP;unicast;client_port=2144-2145\r\n"
				//"Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n"
				"User-Agent: VLC media player (LIVE555 Streaming Media v2010.01.07)\r\n"
				"\r\n",
				inet_ntoa(in),
				pplayer->uri,
				pplayer->pps->sdp_info.track[pplayer->cur_trackId].trackID,
				++pplayer->req_cseq);
			pplayer->cur_trackId++;
		}
		else {
			pplayer->prtsp->cb_reqlen = snprintf(&pplayer->prtsp->cb_reqbuf[0],
				RTSP_REQ_BUFFER_SIZE,
				"SETUP rtsp://%s%s/%s RTSP/1.0\r\n"
				"CSeq: 4\r\n"
				"Transport: RTP/AVP;unicast;client_port=2146-2147\r\n"
				//"Transport: RTP/AVP/TCP;unicast;interleaved=2-3\r\n"
				"Session: %llu\r\n"
				"User-Agent: VLC media player (LIVE555 Streaming Media v2010.01.07)\r\n"
				"\r\n",
				inet_ntoa(in),
				pplayer->uri,
				pplayer->pps->sdp_info.track[pplayer->cur_trackId].trackID,
				pplayer->session);
			pplayer->cur_trackId++;
		}
		pplayer->fpl_state = RL_FPL_STATE_WAIT_SETUP;
		ret = rl_player_process_rtsp_request(pplayer->prtsp->fd, pplayer, locks_held);
		if (ret == 0 || ret == 2) return 1;
		if (ret == -1) pplayer->fpl_state = RL_FPL_STATE_ERROR;
		break;

	case RL_FPL_STATE_WAIT_SETUP:
		if (pplayer->prtsp->status != 200) {
			pplayer->fpl_state = RL_FPL_STATE_ERROR;
			break;
		}
		if (pplayer->cur_trackId < pplayer->pps->sdp_info.num_trackID) {
			pplayer->fpl_state = RL_FPL_STATE_SEND_SETUP;
		}
		else {
			pplayer->fpl_state = RL_FPL_STATE_SEND_PLAY;
		}
		break;

	case RL_FPL_STATE_SEND_PLAY:
		pplayer->prtsp->cb_reqlen = snprintf(&pplayer->prtsp->cb_reqbuf[0],
			RTSP_REQ_BUFFER_SIZE,
			"PLAY rtsp://%s%s/ RTSP/1.0\r\n"
			"CSeq: 5\r\n"
			"Session: %llu\r\n"
			"Range: npt=0.000-\r\n"
			"User-Agent: VLC media player (LIVE555 Streaming Media v2010.01.07)\r\n"
			"\r\n",
			inet_ntoa(in),
			pplayer->uri,
			pplayer->session);
		gettimeofday(&pplayer->start_time, NULL);
		pplayer->fpl_state = RL_FPL_STATE_WAIT_PLAY;
		ret = rl_player_process_rtsp_request(pplayer->prtsp->fd, pplayer, locks_held);
		if (ret == 0 || ret == 2) return 1;
		if (ret == -1) pplayer->fpl_state = RL_FPL_STATE_ERROR;
		break;

	case RL_FPL_STATE_WAIT_PLAY:
		if (pplayer->prtsp->status == 200) {
			pplayer->fpl_state = RL_FPL_STATE_START_PLAY;
		}
		else {
			pplayer->fpl_state = RL_FPL_STATE_SEND_TEARDOWN;
		}
		break;

	case RL_FPL_STATE_START_PLAY:
		tot_npt = atof(pplayer->pps->sdp_info.npt_stop) * 1000000; // ms
		//printf("tot_npt=%ld\n", tot_npt);
		if (tot_npt < 10000) {
			tot_npt = LIVE_VIDEO_CACHEABLE_DURATION * 1000000; // record live stream for 10 mins
		}
		if (tot_npt > 0 && !pplayer->fteardown_task) {
			pplayer->fteardown_task = nkn_scheduleDelayedTask(
					tot_npt, orp_teardown_state, pplayer);
		}

		char *cod_uri;
		int cod_urilen;
		nkn_objv_t objv;
		nkn_attr_t *pnknattr;
	
		ret = nkn_cod_get_cn(pplayer->uol.cod, &cod_uri, &cod_urilen);
		if (ret) {
			DBG_LOG(MSG, MOD_RTSP, 
				"nkn_cod_get_cn() failed, rv=%d", ret);
			pplayer->p_mm_task->err_code = NKN_OM_INV_OBJ_VERSION;
			//return -1;
			pplayer->fpl_state = RL_FPL_STATE_ERROR;
			break;
		}
		mime_header_t hdr;
		char buf[256];
		const char *data;
		u_int32_t attrs;
		int len;
		int hdrcnt;
		int rv;
		int nonmat_type;
		
		init_http_header(&hdr, 0, 0);
		strcpy(buf, "GET");
		add_known_header(&hdr, MIME_HDR_X_NKN_METHOD, buf, strlen(buf));
		add_known_header(&hdr, MIME_HDR_X_NKN_URI, pplayer->uri, strlen(pplayer->uri));

		rv = get_known_header(&pplayer->hdr, RTSP_HDR_X_HOST,
				  &data, &len, &attrs, &hdrcnt);
		if (!rv) add_known_header(&hdr, MIME_HDR_HOST, data, len);
		
		rv = get_known_header(&pplayer->hdr, RTSP_HDR_LAST_MODIFIED,
				  &data, &len, &attrs, &hdrcnt);
		if (!rv) add_known_header(&hdr, MIME_HDR_LAST_MODIFIED, data, len);
		
		rv = get_known_header(&pplayer->hdr, RTSP_HDR_DATE,
				  &data, &len, &attrs, &hdrcnt);
		if (!rv) add_known_header(&hdr, MIME_HDR_DATE, data, len);
		
		ret = compute_object_version(&hdr, cod_uri, cod_urilen, 0, &objv, &nonmat_type);
		if (ret) {
			DBG_LOG(MSG, MOD_RTSP, 
				"compute_object_version() failed, rv=%d cod=0x%lx uri=%s",
				ret, pplayer->uol.cod, cod_uri);
			pplayer->p_mm_task->err_code = NKN_OM_INV_OBJ_VERSION;
			//return -1;
			pplayer->fpl_state = RL_FPL_STATE_ERROR;
			break;
		}

		ret = nkn_cod_test_and_set_version(pplayer->uol.cod, objv,
				NKN_COD_SET_ORIGIN | NKN_COD_SET_STREAMING);
		if (ret) {
			DBG_LOG(MSG, MOD_RTSP, 
				"Cod set version error, ret=%d cod=0x%lx uri=%s",
				ret, pplayer->uol.cod, cod_uri);
			pplayer->p_mm_task->err_code = NKN_OM_INV_OBJ_VERSION;
			//return -1;
			pplayer->fpl_state = RL_FPL_STATE_ERROR;
			break;
		}
	
		if (pplayer->p_mm_task->in_attr) {
			ret = mime_header2nkn_buf_attr(&hdr, 0,
					    pplayer->p_mm_task->in_attr,
					    nkn_buffer_set_attrbufsize,
					    nkn_buffer_getcontent);
			if (ret) {
			    DBG_LOG(MSG, MOD_OM,
				    "mime_header2nkn_buf_attr(mime_header_t) "
				    "failed rv=%d", ret);
				//goto tunnel_response;
				//return -1;
				pplayer->fpl_state = RL_FPL_STATE_ERROR;
				break;
			}

			pnknattr = (nkn_attr_t *) nkn_buffer_getcontent(
					    pplayer->p_mm_task->in_attr);

#if 0
			    pnknattr->cache_create =
					ocon->request_response_time -
					object_corrected_initial_age;
			    if (pnknattr->cache_create < 0) {
				    pnknattr->cache_create =
					ocon->request_response_time;
			    }
			    pnknattr->cache_expiry = object_expire_time;
			    pnknattr->content_length = ocon->content_length;
			    pnknattr->obj_version = objv;
#endif

			/* Todo: update with expiry time from namespace
			 */
#if 0
			if (object_expire_time == NKN_EXPIRY_FOREVER) {
				object_expire_time = nkn_cur_ts +
					ocon->oscfg.policies->cache_age_default;
			}
#endif
			pnknattr->cache_expiry = nkn_cur_ts + pplayer->cache_age;
#if 0
			if (CHECK_PLAYER_FLAG(pplayer, PLAYERF_LIVE_CACHEABLE)) {
				pnknattr->cache_expiry = nkn_cur_ts + 60*30;
			}
			else {
				pnknattr->cache_expiry = nkn_cur_ts + 60*60*8;
			}
#endif
			pnknattr->obj_version = objv;
		}

		pplayer->fpl_state = RL_FPL_STATE_PLAYING;
		return 1;

	case RL_FPL_STATE_PLAYING:
		return 1;

	case RL_FPL_STATE_SEND_TEARDOWN:
		pplayer->fpl_state = RL_FPL_STATE_WAIT_TEARDOWN;
		orp_teardown(pplayer, locks_held);
		return 1;

	case RL_FPL_STATE_WAIT_TEARDOWN:
		return 1;

	case RL_FPL_STATE_ERROR:
		pplayer->fpl_state = RL_FPL_STATE_ERROR_CLOSE;
		orp_close(pplayer, locks_held);
		return -1;
		
	case RL_FPL_STATE_ERROR_CLOSE:
		return 1;

	default:
		return -1;
	}
	}
	return 1;
}

int orp_write_sdp_to_tfm(rtsp_con_player_t * pplayer)
{
	//char buf[4096];
	rvnf_header_t * prvnfh;
	rtsp_con_ps_t *pps;
	rvnf_seek_box_t *rvnf_sb;
	//int len, ret;
	//char * p;

	if (!pplayer || !pplayer->pps) return 0;

	pps = pplayer->pps;

	if (!pplayer->out_buffer) {
		pplayer->fpl_state = RL_FPL_STATE_ERROR;
		return 0;
	}

	pthread_mutex_lock(&pplayer->buf_mutex);
	prvnfh = (rvnf_header_t *) &pplayer->out_buffer[pplayer->ob_wroff];
	strcpy(prvnfh->magic, MAGIC_STRING);
	prvnfh->version = RVNFH_VERSION;
	prvnfh->sdp_size = pps->sdp_size;
	memcpy(prvnfh->sdp_string, pps->sdp_string, pps->sdp_size);
	memcpy((char *)&prvnfh->sdp_info, (char *)&pps->sdp_info, sizeof(rl_sdp_info_t));
	pplayer->ob_wroff   += rvnf_header_s + pps->sdp_size;
	pplayer->ob_datasz  += rvnf_header_s + pps->sdp_size;
	pplayer->ob_tot_len += rvnf_header_s + pps->sdp_size;

	rvnf_sb = (rvnf_seek_box_t *) &pplayer->out_buffer[pplayer->ob_wroff];
	rvnf_sb->end_time = 0;
	rvnf_sb->this_box_data_size = 0;
	pplayer->pseekbox    = &pplayer->out_buffer[pplayer->ob_wroff];
	pplayer->ob_wroff   += rvnf_seek_box_s;
	pplayer->ob_datasz  += rvnf_seek_box_s;
	pplayer->ob_tot_len += rvnf_seek_box_s;
	pplayer->ob_len      = 0;
	//printf( "SDP tot_len=%ld data_size=%d rd_off=%d wr_off=%d\n",
	//	pplayer->ob_tot_len, pplayer->ob_datasz, 
	//	pplayer->ob_rdoff, pplayer->ob_wroff );
	pthread_mutex_unlock(&pplayer->buf_mutex);
	return 1;
}

int orp_send_data_to_tfm_udp(rtsp_con_player_t * pplayer, char *buf, int len, int type, int track)
{
	rvnf_seek_box_t *rvnf_sb;
	int ret;
	char *p;
	int send_size;
	struct timeval cur_time;

	if (type == RL_DATA_RTCP) return 1;
	if (!pplayer || !pplayer->p_mm_task) return 1;

	pthread_mutex_lock(&pplayer->buf_mutex);
	if ((CHECK_PLAYER_FLAG(pplayer, PLAYERF_SEND_BUFFER) && ((int)(pplayer->ob_datasz) > 
			(int)(pplayer->p_mm_task->in_uol.length))) ||
			(pplayer->ob_datasz > (16 * CM_MEM_PAGE_SIZE))) {
		if (CHECK_PLAYER_FLAG(pplayer, PLAYERF_SEND_BUFFER) && 
				pplayer->fpl_state == RL_FPL_STATE_PLAYING) {
			send_size = MIN(pplayer->ob_datasz, (16 * CM_MEM_PAGE_SIZE));
			send_size = MIN(send_size, pplayer->p_mm_task->in_uol.length);

			/* Update seek box
			 */
			rvnf_sb = (rvnf_seek_box_t *) pplayer->pseekbox;
			rvnf_sb->this_box_data_size = pplayer->ob_len;
			rvnf_sb->end_time = pplayer->elapsed_time;

			if (pplayer->uol.cod) {
				nkn_cod_close(pplayer->uol.cod, NKN_COD_ORIGIN_MGR);
				pplayer->uol.cod = NKN_COD_NULL;
			}
			ret = RTSP_OM_get_ret_task(pplayer->p_mm_task, 
					pplayer->out_buffer + pplayer->ob_rdoff,
					send_size, &pplayer->ob_tot_written);
			//pplayer->ob_tot_written += ret;
			pplayer->ob_rdoff  += ret;
			pplayer->ob_datasz -= ret;
			if (pplayer->ob_datasz == 0) {
				pplayer->ob_rdoff = 0;
				pplayer->ob_wroff = 0;
			}
			else { 
				memcpy(pplayer->out_buffer, 
					pplayer->out_buffer + pplayer->ob_rdoff,
					pplayer->ob_datasz );
				pplayer->ob_rdoff = 0;
				pplayer->ob_wroff = pplayer->ob_datasz;
			}
			rvnf_sb = (rvnf_seek_box_t *) &pplayer->out_buffer[pplayer->ob_wroff];
			rvnf_sb->end_time = 0;
			rvnf_sb->this_box_data_size = 0;
			pplayer->pseekbox    = &pplayer->out_buffer[pplayer->ob_wroff];
			pplayer->ob_wroff   += rvnf_seek_box_s;
			pplayer->ob_datasz  += rvnf_seek_box_s;
			pplayer->ob_tot_len += rvnf_seek_box_s;
			pplayer->ob_len      = 0;
			UNSET_PLAYER_FLAG(pplayer, PLAYERF_SEND_BUFFER);
		}
		else {
			glob_rtsp_orp_data_ready_non_play++;
		}
	}
	if ((pplayer->ob_wroff + sizeof(rvnf_seek_box_t) + len + 8) <
			(32 * CM_MEM_PAGE_SIZE)) {
		p = pplayer->out_buffer + pplayer->ob_wroff;
		gettimeofday(&cur_time, NULL);
		pplayer->elapsed_time = (cur_time.tv_sec * 1000 + cur_time.tv_usec 
				/ 1000) - (pplayer->start_time.tv_sec * 1000 + 
				pplayer->start_time.tv_usec / 1000);
		*p = '$';
		*(p+1) = track;
		*((uint16_t *)(p+2)) = (uint16_t) len;
		*((uint32_t *)(p+4)) = pplayer->elapsed_time;
		memcpy( p + 8, buf, len);
		pplayer->ob_wroff   += len + 8;
		pplayer->ob_datasz  += len + 8;
		pplayer->ob_tot_len += len + 8; 
		pplayer->ob_len     += len + 8;
		//printf( "SDP tot_len=%ld data_size=%d rd_off=%d wr_off=%d\n",
		//	pplayer->ob_tot_len, pplayer->ob_datasz, 
		//	pplayer->ob_rdoff, pplayer->ob_wroff );
		//printf( "Track=%2d len=%4d time=%8d Tot_len=%8ld\r\n",
		//	track, len, pplayer->elapsed_time, pplayer->ob_tot_len);
	}
	else {
		//printf( "Error tot_len=%ld data_size=%d rd_off=%d wr_off=%d\n",
		//	pplayer->ob_tot_len, pplayer->ob_datasz, 
		//	pplayer->ob_rdoff, pplayer->ob_wroff );
		glob_rtsp_orp_cache_buffer_overflow++;
	}
	pthread_mutex_unlock(&pplayer->buf_mutex);
	
	return 1;
}

int orp_send_data_to_tfm_tcp(rtsp_con_player_t * pplayer, char *buf, int len, int type, int track)
{
	rvnf_seek_box_t *rvnf_sb;
	int ret;
	char *p;
	int send_size;
	struct timeval cur_time;

	UNUSED_ARGUMENT(track);
	if (type == RL_DATA_RTCP) return 1;
	if (!pplayer || !pplayer->p_mm_task) return 1;

	pthread_mutex_lock(&pplayer->buf_mutex);
	if ((int)(pplayer->ob_datasz) > (int)(pplayer->p_mm_task->in_uol.length) ||
			(pplayer->ob_datasz > (16 * CM_MEM_PAGE_SIZE))) {
		if (CHECK_PLAYER_FLAG(pplayer, PLAYERF_SEND_BUFFER) && 
				pplayer->fpl_state == RL_FPL_STATE_PLAYING) {

			send_size = MIN(pplayer->ob_datasz, (16 * CM_MEM_PAGE_SIZE));
			send_size = MIN(send_size, pplayer->p_mm_task->in_uol.length);

			/* Update seek box
			 */
			rvnf_sb = (rvnf_seek_box_t *) pplayer->pseekbox;
			rvnf_sb->this_box_data_size = pplayer->ob_len;
			rvnf_sb->end_time = pplayer->elapsed_time;

			if (pplayer->uol.cod) {
				nkn_cod_close(pplayer->uol.cod, NKN_COD_ORIGIN_MGR);
				pplayer->uol.cod = NKN_COD_NULL;
			}
			ret = RTSP_OM_get_ret_task(pplayer->p_mm_task, 
					pplayer->out_buffer + pplayer->ob_rdoff,
					send_size, &pplayer->ob_tot_written);
			//pplayer->ob_tot_written += ret;
			pplayer->ob_rdoff  += ret;
			pplayer->ob_datasz -= ret;
			if (pplayer->ob_datasz == 0) {
				pplayer->ob_rdoff = 0;
				pplayer->ob_wroff = 0;
			}
			else { 
				memcpy(pplayer->out_buffer, 
					pplayer->out_buffer + pplayer->ob_rdoff,
					pplayer->ob_datasz );
				pplayer->ob_rdoff = 0;
				pplayer->ob_wroff = pplayer->ob_datasz;
			}
			rvnf_sb = (rvnf_seek_box_t *) &pplayer->out_buffer[pplayer->ob_wroff];
			rvnf_sb->end_time = 0;
			rvnf_sb->this_box_data_size = 0;
			pplayer->pseekbox    = &pplayer->out_buffer[pplayer->ob_wroff];
			pplayer->ob_wroff   += rvnf_seek_box_s;
			pplayer->ob_datasz  += rvnf_seek_box_s;
			pplayer->ob_tot_len += rvnf_seek_box_s;
			pplayer->ob_len      = 0;
			UNSET_PLAYER_FLAG(pplayer, PLAYERF_SEND_BUFFER);
		}
		else {
			glob_rtsp_orp_data_ready_non_play++;
		}
	}

	if ((pplayer->ob_wroff + sizeof(rvnf_seek_box_t) + len + 4) <
			(32 * CM_MEM_PAGE_SIZE)) {
		p = pplayer->out_buffer + pplayer->ob_wroff;
		gettimeofday(&cur_time, NULL);
		pplayer->elapsed_time = (cur_time.tv_sec * 1000 + cur_time.tv_usec 
				/ 1000) - (pplayer->start_time.tv_sec * 1000 + 
				pplayer->start_time.tv_usec / 1000);
		*p = '$';
		*(p+1) = track;
		*((uint16_t *)(p+2)) = (uint16_t) len - 4;
		*((uint32_t *)(p+4)) = pplayer->elapsed_time;
		memcpy( p + 8, buf + 4, len - 4);
		pplayer->ob_wroff   += len + 4;
		pplayer->ob_datasz  += len + 4;
		pplayer->ob_tot_len += len + 4;
		pplayer->ob_len     += len + 4;
		//printf( "SDP tot_len=%ld data_size=%d rd_off=%d wr_off=%d\n",
		//	pplayer->ob_tot_len, pplayer->ob_datasz, 
		//	pplayer->ob_rdoff, pplayer->ob_wroff );
	}
	else {
		//printf( "Error tot_len=%ld data_size=%d rd_off=%d wr_off=%d\n",
		//	pplayer->ob_tot_len, pplayer->ob_datasz, 
		//	pplayer->ob_rdoff, pplayer->ob_wroff );
		glob_rtsp_orp_cache_buffer_overflow++;
	}
	pthread_mutex_unlock(&pplayer->buf_mutex);
	
	return 1;
}

/*
 * con structure management.
 */
int orp_create_fake_pplayer(orp_fetch_uri_t * porp)
{
	rtsp_con_player_t * pplayer;
	uuid_t uid;
	unsigned long long fSessionIdCounter;

	uuid_generate(uid);
	fSessionIdCounter = ((unsigned long long)((*(uint32_t *)&uid[0]) ^ 
			(*(uint32_t *)&uid[4])) << 32 ) | 
			((unsigned long long)((*(uint32_t *)&uid[8]) ^ 
			(*(uint32_t *)&uid[12])));

	pplayer = rtsp_new_pplayer(NULL /*porp->pns*/, fSessionIdCounter);
	if (!pplayer) { return FALSE; }

	SET_PLAYER_FLAG(pplayer, PLAYERF_INTERNAL);
	pplayer->prtsp->pns = porp->pns;
	pplayer->rtsp_fd = -1;
	pplayer->peer_ip_addr = -1;
	pplayer->own_ip_addr = porp->dip; //pns->if_addr;
	pplayer->uri = nkn_strdup_type(porp->uri, mod_rtsp_vs_cfg_strdup);
	pplayer->player_type = RL_PLAYER_TYPE_FAKE_PLAYER;
	pplayer->send_data = orp_send_data_to_tfm_udp;
	pplayer->fteardown_task = NULL;

	pplayer->out_buffer = nkn_malloc_type(32 * CM_MEM_PAGE_SIZE, mod_rtsp_outbuf_malloc);
	if (pplayer->out_buffer == NULL) {
		free(pplayer->uri);
		free(pplayer->prtsp);
		free(pplayer);
		porp->pplayer = NULL;
		return FALSE;
	}
	pplayer->ob_bufsize = 32 * CM_MEM_PAGE_SIZE;
	pplayer->ob_datasz  = 0;
	pplayer->ob_rdoff   = 0;
	pplayer->ob_wroff   = 0;
	pplayer->ob_tot_len = 0;
	pplayer->fpl_state  = RL_FPL_STATE_INIT;
	pthread_mutex_init(&pplayer->buf_mutex, NULL);
	porp->pplayer = pplayer;
	glob_rtsp_orp_num_fake_player++;
	glob_rtsp_orp_create_fake_player++;
	return TRUE;
}


void * orp_add_new_video(nkn_interface_t * pns, uint32_t dip, uint16_t dport, 
		char * uri, char *cache_uri,
		namespace_token_t nstoken, char *vfs_filename)
{
	orp_fetch_uri_t * porp;
	int ret;
	static uint64_t rtsp_om_physid = 0;
	FILE* fid;
	
	/* If cache_uri is NULL, do not cache the video
	 */
	if (!uri || !cache_uri) return NULL;
	glob_rtsp_orp_num_object_req++;

	/*
	 * Check if we are fetching the object now?
	 * We do not want to fetch the same object twice.
	 */
	porp = orp_look_up_uri(uri);
	if (porp) {
		glob_rtsp_orp_req_same_object++;
		return NULL;
	}

	/* Check if file is available in cache
	 */
	fid = vfs_funs.pnkn_fopen(vfs_filename, "I");
	if (fid) {
		vfs_funs.pnkn_fclose(fid);
		return NULL;
	}
	
	/*
	 * Launch the internal FAKE player.
	 */
	porp = (orp_fetch_uri_t *)nkn_malloc_type(sizeof(orp_fetch_uri_t), mod_rtsp_rl_rmd_malloc);
	if (!porp) { return NULL; }
	memset((char *)porp, 0, sizeof(orp_fetch_uri_t));
	porp->magic = RLMAGIC_ALIVE;

	porp->pmutex = (pthread_mutex_t *)nkn_malloc_type(sizeof(pthread_mutex_t), mod_rtsp_rl_rmd_malloc);
	pthread_mutex_init(porp->pmutex, NULL);
	pthread_mutex_lock(porp->pmutex);
	porp->pns = pns;
	porp->uri = nkn_strdup_type(uri, mod_rtsp_vs_cfg_strdup);
	porp->urilen = strlen(porp->uri);
	porp->dip = dip;
	porp->dport = dport;
	porp->flag = 0;

	ret = orp_create_fake_pplayer(porp);
	if (ret == FALSE) {
		pthread_mutex_unlock(porp->pmutex);
		free(porp->uri);
		free(porp);
		return NULL;
	}
	/* Set unique id
	 */
	snprintf(&porp->physid[0], sizeof(porp->physid),
		"RTSP_%08ld", rtsp_om_physid);
	rtsp_om_physid++;
	
	SET_ORP_FLAG(porp, ORPF_FETCHING);
	porp->cache_uri = nkn_strdup_type(cache_uri, mod_rtsp_tunnel_cache_name);
	porp->pplayer->prtsp->nstoken = nstoken;
	pthread_mutex_unlock(porp->pmutex);

	orp_add_to_list(porp);

#if 1
	AM_pk_t am_pk;
	am_object_data_t am_object_data;
	orp_fetch_om_uri_t om;

	memset(&om, 0, sizeof(orp_fetch_om_uri_t));
	om.dip       = porp->dip;
	om.dport     = porp->dport;
	om.uri       = porp->uri;
	om.cache_uri = porp->cache_uri;
	om.urilen    = porp->urilen;
	memcpy(om.physid, porp->physid, sizeof(om.physid) );
	
	am_pk.name = porp->cache_uri;
	am_pk.provider_id = RTSPMgr_provider;
	am_pk.key_len = 0;
	am_object_data.client_ipv4v6.family = AF_INET;
	IPv4(am_object_data.client_ipv4v6) = 0;
	am_object_data.client_port = 0;
	am_object_data.client_ip_family = AF_INET;
	am_object_data.origin_ipv4v6.family = AF_INET;
	IPv4(am_object_data.origin_ipv4v6) = dip;
	am_object_data.origin_port = dport;
	am_object_data.proto_data = &om;
	am_object_data.flags = AM_OBJ_TYPE_STREAMING | AM_INGEST_ONLY;
	am_object_data.host_hdr = NULL;
	am_object_data.in_cod = NKN_COD_NULL;
	/* Do a hit count for AM */
	AM_inc_num_hits(&am_pk, 1, 0, NULL, &am_object_data, NULL);
#endif

	return porp;
}

static void * om_rtsp_player_main(void * arg);
static void * om_rtsp_player_main(void * arg)
{
	orp_fetch_uri_t * porp, * porp_next;
	struct network_mgr * pnm_ps = NULL;
	int locks;

	UNUSED_ARGUMENT(arg);
	prctl(PR_SET_NAME, "om_rtsp_player", 0, 0, 0);

	while(1) {
		pthread_mutex_lock(&orp_mutex);
		porp = g_orp_list;
		while (porp) {
			if (!CHECK_PORP_FLAG(porp, PORPF_ACTIVE)) {
				porp_next = porp->next;
				orp_free_from_list(porp);
				porp = porp_next;
				continue;
			}
#if 0
			if (porp->magic != RLMAGIC_ALIVE) {
				/* If orp is already freed, the next pointer would not be
				 * valid, break from loop
				 */
				break;
			}
#endif			
			porp_next = porp->next;
			pthread_mutex_lock(porp->pmutex);
			if (CHECK_ORP_FLAG(porp, ORPF_START_PLAY) && porp->pplayer) {
				UNSET_ORP_FLAG(porp, ORPF_START_PLAY);
				/* Release lock before calling orp_fsm
				 */
				pthread_mutex_unlock(porp->pmutex);

				locks = 0;
				pthread_mutex_lock(&porp->pplayer->prtsp->mutex);
				SET_LOCK(locks, LK_PL);
				if (porp->pplayer->fpl_state == RL_FPL_STATE_INIT) {
					pthread_mutex_unlock(&orp_mutex);
					if (porp->pplayer->pps)
						pnm_ps = &gnm[porp->pplayer->pps->rtsp_fd];
					if (pnm_ps) {
						pthread_mutex_lock(&pnm_ps->mutex); 
						SET_LOCK(locks, LK_PS_GNM);
					}
					orp_fsm(porp->pplayer, locks);
					if (pnm_ps) pthread_mutex_unlock(&pnm_ps->mutex);
					pthread_mutex_lock(&orp_mutex);
				}
				pthread_mutex_unlock(&porp->pplayer->prtsp->mutex);
			}
			else {
				pthread_mutex_unlock(porp->pmutex);
			}
			porp = porp_next;
		}
		pthread_mutex_unlock(&orp_mutex);
		sleep(1);
	}
	return NULL;
}

/* 
 * Create a fake RTSP player.
 * the purpose of this fake RTSP player is for downloading the whole video streaming file
 * and cache it.
 */
static pthread_t rtsp_player_pid;
void RTSP_player_init(void);
void RTSP_player_init(void)
{
	int rv;
	pthread_attr_t attr;
	int stacksize = 128 * KiBYTES;

	rv = pthread_attr_init(&attr);
	if (rv) {
		DBG_LOG(SEVERE, MOD_RTSP, "(2) pthread_attr_init() failed, rv=%d", rv);
		return;
	}
	rv = pthread_attr_setstacksize(&attr, stacksize);
	if (rv) {
		DBG_LOG(SEVERE, MOD_RTSP,
			"(2) pthread_attr_setstacksize() stacksize=%d failed, "
			"rv=%d", stacksize, rv);
		return;
	}

	if(pthread_create(&rtsp_player_pid, &attr, om_rtsp_player_main, NULL)) {
		DBG_LOG(MSG, MOD_RTSP, "error in create om_rtsp_player_main thread");
		return;
	}

	return;
}

int nkn_copy_orp_data(void *in_proto_data, void **out_proto_data)
{
	orp_fetch_om_uri_t *pom_in, *pom_out;

	*out_proto_data = NULL;
	if (!in_proto_data || !out_proto_data) {
		return -1;
	}
	pom_in  = (orp_fetch_om_uri_t *) in_proto_data;

	if (pom_in->physid[0] != 'R' || pom_in->physid[1] != 'T' ||
		pom_in->physid[2] != 'S' || pom_in->physid[3] != 'P' ) {
		return -1;
	}

	if (pom_in->uri == NULL || pom_in->cache_uri == NULL) {
		return -1;
	}
	
	pom_out = (orp_fetch_om_uri_t *) nkn_malloc_type(sizeof(orp_fetch_om_uri_t), mod_rtsp_rl_rmd_malloc);
	memset(pom_out, 0, sizeof(orp_fetch_om_uri_t));
	pom_out->dip       = pom_in->dip;
	pom_out->dport     = pom_in->dport;
	pom_out->uri       = nkn_strdup_type(pom_in->uri, mod_rtsp_rl_rmd_malloc);
	pom_out->cache_uri = nkn_strdup_type(pom_in->cache_uri, mod_rtsp_rl_rmd_malloc);
	pom_out->urilen    = pom_in->urilen;
	memcpy(pom_out->physid, pom_in->physid, sizeof(pom_out->physid) );
	*out_proto_data = pom_out;
	return 0;
}

void nkn_free_orp_data(void *proto_data)
{
	orp_fetch_om_uri_t *pom;

	pom = (orp_fetch_om_uri_t *) proto_data;
	free(pom->uri);
	free(pom->cache_uri);
	memset(pom, 0, sizeof(orp_fetch_om_uri_t));
	free(pom);
}

orp_fetch_uri_t *orp_look_up_uri(char * uri)
{
	orp_fetch_uri_t * porp;
	/*
	 * Check if we are fetching the object now?
	 * We do not want to fetch the same object twice.
	 */
	if (!uri) return NULL;
	
	pthread_mutex_lock(&orp_mutex);
	porp = g_orp_list;
	while (porp) {
		if (strcmp(uri, porp->uri) == 0) {
			pthread_mutex_unlock(&orp_mutex);
			return CHECK_PORP_FLAG(porp, PORPF_ACTIVE) ? porp : NULL;
		}
		porp = porp->next;
	}
	pthread_mutex_unlock(&orp_mutex);
	return NULL;
}

orp_fetch_uri_t *orp_look_up_physid(char * physid)
{
	orp_fetch_uri_t * porp;
	/*
	 * Check if we are fetching the object now?
	 * We do not want to fetch the same object twice.
	 */

	if (!physid) return NULL;
	
	pthread_mutex_lock(&orp_mutex);
	porp = g_orp_list;
	while (porp) {
		if (strcmp(physid, porp->physid) == 0) {
			pthread_mutex_unlock(&orp_mutex);
			return CHECK_PORP_FLAG(porp, PORPF_ACTIVE) ? porp : NULL;
		}
		porp = porp->next;
	}
	pthread_mutex_unlock(&orp_mutex);
	return NULL;
}

int orp_list_num_entries = 0;
int orp_list_add_called = 0;
int orp_list_remove_called = 0;
int orp_list_remove_found = 0;
int orp_list_remove_notfound = 0;

void orp_add_to_list(orp_fetch_uri_t *porp)
{
	pthread_mutex_lock(&orp_mutex);
	orp_list_add_called++;
	porp->next = g_orp_list;
	g_orp_list = porp;
	orp_list_num_entries++;
	pthread_mutex_unlock(&orp_mutex);
}

orp_fetch_uri_t *orp_remove_from_list_uri(char * uri)
{
	orp_fetch_uri_t * porp; //, *prev;

	if (g_orp_list == NULL) {
		return NULL; // Should assert here
	}
	
	pthread_mutex_lock(&orp_mutex);
	orp_list_remove_called++;
	porp = g_orp_list;
	
	if (strncmp(porp->uri, uri, porp->urilen) == 0) {
		//pthread_mutex_lock(porp->pmutex);
		UNSET_PORP_FLAG(porp, PORPF_ACTIVE);
		//g_orp_list = porp->next;
		//porp->next = NULL;
		//orp_list_num_entries--;
		orp_list_remove_found++;
		//printf( "Found URI=%s, %d %d\r\n", uri, orp_list_remove_called, orp_list_remove_found );
		pthread_mutex_unlock(&orp_mutex);
		return porp;
	}
	
	//prev = porp;
	porp = porp->next;
	
	while (porp) {
		if (strncmp(porp->uri, uri, porp->urilen) == 0) {
			//pthread_mutex_lock(porp->pmutex);
			UNSET_PORP_FLAG(porp, PORPF_ACTIVE);
			//prev->next = porp->next;
			//porp->next = NULL;
			//orp_list_num_entries--;
			orp_list_remove_found++;
			//printf( "Found URI=%s, %d %d\r\n", uri, orp_list_remove_called, orp_list_remove_found );
			pthread_mutex_unlock(&orp_mutex);
			return porp;
		}
		//prev = porp;
		porp = porp->next;
	}
	orp_list_remove_notfound++;
	//printf( "Not Found URI=%s, %d %d\r\n", uri, orp_list_remove_called, orp_list_remove_notfound );
	pthread_mutex_unlock(&orp_mutex);
	return NULL;
}

/* Caller needs to lock orp_mutex.
 */  
orp_fetch_uri_t *orp_free_from_list(orp_fetch_uri_t *porp)
{
	orp_fetch_uri_t *cur, *prev, *next;
	pthread_mutex_t *pmutex;

	if (g_orp_list == NULL) {
		return NULL; // Should assert here
	}
	
	cur = g_orp_list;
	if (cur == porp) {
		pthread_mutex_lock(porp->pmutex);
		g_orp_list = porp->next;
		next = porp->next;
		//printf( "Found URI=%s, %d %d\r\n", uri, orp_list_remove_called, orp_list_remove_found );
		goto free_porp;
	}
	
	prev = cur;
	cur = cur->next;
	
	while (cur) {
		if (cur == porp) {
			pthread_mutex_lock(porp->pmutex);
			prev->next = porp->next;
			next = porp->next;
			//printf( "Found URI=%s, %d %d\r\n", uri, orp_list_remove_called, orp_list_remove_found );
			goto free_porp;
		}
		prev = cur;
		cur = cur->next;
	}
	//printf( "Not Found URI=%s, %d %d\r\n", uri, orp_list_remove_called, orp_list_remove_notfound );
	return NULL;

free_porp:
	if (porp) {
		//pthread_mutex_lock(porp->pmutex);
		pmutex = porp->pmutex;
		UNSET_PORP_FLAG(porp, PORPF_ACTIVE);
		free(porp->uri); 
		free(porp->cache_uri); 
		porp->next = NULL;
		porp->uri = NULL;
		porp->cache_uri  = NULL;
		porp->pplayer = NULL;
		porp->pmutex  = NULL;
		porp->magic   = RLMAGIC_DEAD;
		free(porp);
		orp_list_num_entries--;
		glob_rtsp_orp_num_fake_player--;
		pthread_mutex_unlock(pmutex);
		free(pmutex);
	}
	return next;
}

