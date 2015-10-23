#include <math.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/queue.h>
#define USE_SIGNALS 1
#include <time.h> // for "strftime()" and "gmtime()"
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <atomic_ops.h>


#define USE_SPRINTF

// NKN generic defs
#include "nvsd_mgmt.h"
#include "nkn_debug.h"
#include "nkn_defs.h"
#include "nkn_cfg_params.h"
#include "nkn_sched_api.h"
#include "network.h"
#include "server.h"
#include "nkn_util.h"
#include "nkn_stat.h"
#include "nkn_trace.h"
#include "http_header.h"
#include "nkn_http.h"
#include "nkn_cod.h"
#include "nkn_mediamgr_api.h"
#include "rtsp_func.h"
#include "rtsp_server.h"
#include "rtsp_session.h"
#include "rtsp_header.h"
#include "rtsp_header.h"
#include "rtsp_live.h"
#include "nkn_rtsched_api.h"
#include "nvsd_resource_mgr.h"

typedef struct tag_rtsp_om_task {
	char *filename;
	namespace_token_t ns_token;
	rtsp_cb_t *con;

        cache_response_t * cptr;
	//rtsp_con_ps_t *pps;
        nkn_task_id_t nkn_taskid;
        pthread_mutex_t mutex;
}rtsp_om_task_t;	

rtsp_om_task_t * create_rtsp_om_task(char *path, rtsp_cb_t *prtsp);
static void rtsp_mgr_output(nkn_task_id_t id);
static void rtsp_mgr_input(nkn_task_id_t id);
static void rtsp_mgr_cleanup(nkn_task_id_t id);


static void rtsp_describe_handler_cleanup(nkn_task_id_t id);
static void rtsp_describe_handler_input(nkn_task_id_t id); 
static void rtsp_describe_handler_output(nkn_task_id_t id);
#if 0
static void rtsp_om_post_sched_a_task(rtsp_om_task_t *pfs, 
				      off_t offset_requested, 
				      off_t length_requested);
#endif
static void reclaim_rtsp_session_states(rtsp_cb_t * prtsp);
static int rtsp_epollin(int sockfd, void *private_data);
static int rtsp_epollerr(int sockfd, void * private_data);
static int rtsp_epollhup(int sockfd, void * private_data);
static int rtsp_epollout(int sockfd, void * private_data);
static int rtsp_timeout(int sockfd, void *private_data, double timeout);
void nkn_release_prtsp(struct rtsp_cb * prtsp);
void delete_rtsp_session_from_rtsp_list(rtsp_cb_t * prtsp, rtsp_session* serverMediaSession);
extern int rtsp_make_relay(int fd, uint32_t cip, uint32_t dst_ip, 
		nkn_interface_t * pns,
		char * reqbuf, int reqlen, unsigned long long fOurSessionId,
		char * abs_uri, char *cache_uri,
		namespace_token_t nstoken,
		char *vfs_filename);
extern uint32_t tproxy_find_org_dst_ip(int sockfd, struct nkn_interface * pns);
extern int make_up_sdp_string(char * porg_sdp, char * sdp_buf, int sdp_buflen, uint32_t dst_ip);
extern int RTCP_createNew(rtsp_cb_t * prtsp,
                    int streamNum,
                    char * cname,
                    unsigned short serverRTPPort);
extern void free_RTCPInstance(struct streamState * pss);

void handleCmd_unsupportedService(rtsp_cb_t * prtsp);
static void init_rtsp_interleave(rtsp_cb_t* prtsp);
static int process_rtsp_interleave(rtsp_cb_t* prtsp);
void handleCmd_unsupportedMediaCodec(rtsp_cb_t * prtsp) ;
void handleCmd_failStateMachine(rtsp_cb_t * prtsp);
#define RTPINFO_INCLUDE_RTPTIME 1

/********************************************************************************
 *                         COUNTER DEFINITIONS                   
 *******************************************************************************/

/* HIT COUNT & CACHE STATS COUNTER */
NKNCNT_EXTERN(tot_video_delivered_with_hit, AO_t)
extern uint64_t glob_tot_video_delivered;
NKNCNT_DEF(rtsp_tot_bytes_sent, uint64_t, "", "Total number RTSP/RTP bytes sent via RTSP origin")
NKNCNT_DEF(rtsp_tot_vod_bytes_sent, uint64_t, "", "Total RTSP/RTP bytes served from vod")

/* CONNECTION & SESSION COUNTERS */
NKNCNT_DEF(cur_open_rtsp_socket, AO_t, "", "Total currently open RTSP sockets")
NKNCNT_DEF(rtsp_err_bad_method, uint64_t, "", "BAD rtsp method")
NKNCNT_DEF(rtsp_tot_mp4_session, AO_t, "", "Total MPEG-4 session")
NKNCNT_DEF(rtsp_tot_unknown_session, AO_t, "", "Total unknown RTSP session")

NKNCNT_DEF(rtsp_fd_reassigned_err, AO_t, "", "Total FD reassigned error")

/* RTSP PROTOCOL SPECIFIC COUNTERS include,
 * 1. STATUS CODES
 * 2. COMMANDS
 */
NKNCNT_DEF(rtsp_tot_status_resp_100, AO_t, "Messages", "Total 100 response");
NKNCNT_DEF(rtsp_tot_status_resp_200, AO_t, "Messages", "Total 200 response");
NKNCNT_DEF(rtsp_tot_status_resp_201, AO_t, "Messages", "Total 201 response");
NKNCNT_DEF(rtsp_tot_status_resp_250, AO_t, "Messages", "Total 250 response");
NKNCNT_DEF(rtsp_tot_status_resp_300, AO_t, "Messages", "Total 300 response");
NKNCNT_DEF(rtsp_tot_status_resp_301, AO_t, "Messages", "Total 301 response");
NKNCNT_DEF(rtsp_tot_status_resp_302, AO_t, "Messages", "Total 302 response");
NKNCNT_DEF(rtsp_tot_status_resp_303, AO_t, "Messages", "Total 303 response");
NKNCNT_DEF(rtsp_tot_status_resp_304, AO_t, "Messages", "Total 304 response");
NKNCNT_DEF(rtsp_tot_status_resp_305, AO_t, "Messages", "Total 305 response");
NKNCNT_DEF(rtsp_tot_status_resp_400, AO_t, "Messages", "Total 400 response");
NKNCNT_DEF(rtsp_tot_status_resp_401, AO_t, "Messages", "Total 401 response");
NKNCNT_DEF(rtsp_tot_status_resp_402, AO_t, "Messages", "Total 402 response");
NKNCNT_DEF(rtsp_tot_status_resp_403, AO_t, "Messages", "Total 403 response");
NKNCNT_DEF(rtsp_tot_status_resp_404, AO_t, "Messages", "Total 404 response");
NKNCNT_DEF(rtsp_tot_status_resp_405, AO_t, "Messages", "Total 405 response");
NKNCNT_DEF(rtsp_tot_status_resp_406, AO_t, "Messages", "Total 406 response");
NKNCNT_DEF(rtsp_tot_status_resp_407, AO_t, "Messages", "Total 407 response");
NKNCNT_DEF(rtsp_tot_status_resp_408, AO_t, "Messages", "Total 408 response");
NKNCNT_DEF(rtsp_tot_status_resp_410, AO_t, "Messages", "Total 410 response");
NKNCNT_DEF(rtsp_tot_status_resp_411, AO_t, "Messages", "Total 411 response");
NKNCNT_DEF(rtsp_tot_status_resp_412, AO_t, "Messages", "Total 412 response");
NKNCNT_DEF(rtsp_tot_status_resp_413, AO_t, "Messages", "Total 413 response");
NKNCNT_DEF(rtsp_tot_status_resp_414, AO_t, "Messages", "Total 414 response");
NKNCNT_DEF(rtsp_tot_status_resp_415, AO_t, "Messages", "Total 415 response");
NKNCNT_DEF(rtsp_tot_status_resp_451, AO_t, "Messages", "Total 451 response");
NKNCNT_DEF(rtsp_tot_status_resp_452, AO_t, "Messages", "Total 452 response");
NKNCNT_DEF(rtsp_tot_status_resp_453, AO_t, "Messages", "Total 453 response");
NKNCNT_DEF(rtsp_tot_status_resp_454, AO_t, "Messages", "Total 454 response");
NKNCNT_DEF(rtsp_tot_status_resp_455, AO_t, "Messages", "Total 455 response");
NKNCNT_DEF(rtsp_tot_status_resp_456, AO_t, "Messages", "Total 456 response");
NKNCNT_DEF(rtsp_tot_status_resp_457, AO_t, "Messages", "Total 457 response");
NKNCNT_DEF(rtsp_tot_status_resp_458, AO_t, "Messages", "Total 458 response");
NKNCNT_DEF(rtsp_tot_status_resp_459, AO_t, "Messages", "Total 459 response");
NKNCNT_DEF(rtsp_tot_status_resp_460, AO_t, "Messages", "Total 460 response");
NKNCNT_DEF(rtsp_tot_status_resp_461, AO_t, "Messages", "Total 461 response");
NKNCNT_DEF(rtsp_tot_status_resp_462, AO_t, "Messages", "Total 462 response");
NKNCNT_DEF(rtsp_tot_status_resp_500, AO_t, "Messages", "Total 500 response");
NKNCNT_DEF(rtsp_tot_status_resp_501, AO_t, "Messages", "Total 501 response");
NKNCNT_DEF(rtsp_tot_status_resp_502, AO_t, "Messages", "Total 502 response");
NKNCNT_DEF(rtsp_tot_status_resp_503, AO_t, "Messages", "Total 503 response");
NKNCNT_DEF(rtsp_tot_status_resp_504, AO_t, "Messages", "Total 504 response");
NKNCNT_DEF(rtsp_tot_status_resp_505, AO_t, "Messages", "Total 505 response");
NKNCNT_DEF(rtsp_tot_status_resp_551, AO_t, "Messages", "Total 551 response");
 
/* PARSER STATS COUNTERS */
NKNCNT_DEF(rtsp_tot_rtsp_parse_error, uint64_t, "", "Total number of RTSP parse errors")

/* COUNTERS FOR DEBUG PURPOSES */
uint64_t glob_async_flag_not_reset_options_failed_count = 0;
uint64_t glob_async_flag_not_reset_task_failed_count = 0;
uint64_t glob_async_flag_not_reset_desc_failed_count = 0;
NKNCNT_DEF(rtsp_OPTIONS_async_call, uint64_t, "", "Total number of Async OPTIONS call")
NKNCNT_DEF(rtsp_TEARDOWN_async_call, uint64_t, "", "Total number ofAsync TEARDOWN call")
AO_t glob_rtsp_cmd_when_task_in_prog = 0;

extern int rtsp_server_port;

int glob_rtsp_format_convert_enabled = NKN_FALSE;
int rtsp_player_idle_timeout = 60;
int rtsp_origin_idle_timeout = 60;
struct rtsp_cb_queue g_rtsp_cb_queue_head;
extern struct vfs_funcs_t vfs_funs;
static char const* const libNameStr = "nvsd media server";
char const* const libVersionStr = "08/30/2009";
static GHashTable *g_rtsp_cb_origin_tbl; //hash table containing the origin server list
static pthread_mutex_t rtsp_origin_tbl_lock; 
extern struct rtsp_so_libs * rtsp_get_so_libname(char * ext);
extern char rl_rtsp_server_str[];
static pthread_mutex_t debug_counter_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t rtsp_cb_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
/*
 * 1. Make copy the request.
 * 2. Free the local prtsp structure.
 * 3. make the RTSP tunnel.
 */
int tunnel_RTSP_request(rtsp_cb_t * prtsp, char *vfs_filename);
int tunnel_RTSP_request(rtsp_cb_t * prtsp, char *vfs_filename)
{
	char abs_uri[1024];
	char reqbuf[RTSP_REQ_BUFFER_SIZE];
	int reqlen;
	int sockfd;
	nkn_interface_t * pns;
	uint32_t src_ip;
	uint32_t dst_ip;
	unsigned long long fOurSessionId;
	int hostlen;
	size_t cacheurilen;
	//const namespace_config_t * ns_cfg;
	char *cache_uri = NULL;
	int rv;
	char host[2048];
	char *p_host = host;
	uint16_t port;
	char *cache_name = NULL;
	namespace_token_t nstoken;

	sockfd = prtsp->fd;
	pthread_mutex_lock(&gnm[sockfd].mutex);
	/* prtsp seems to be invalid and socket already closed 
	 */
	if (gnm[sockfd].fd == -1 || gnm[sockfd].fd_type != RTSP_FD || 
			gnm[sockfd].private_data != prtsp) {
		nkn_release_prtsp(prtsp);
		pthread_mutex_unlock(&gnm[sockfd].mutex);
		return 0;
	}
	
	/* Make copy the request and other necessary key data */
	RTSP_GET_ALLOC_ABS_URL(&prtsp->hdr, prtsp->urlPreSuffix);
	strcpy(abs_uri, prtsp->urlPreSuffix);
	memcpy(reqbuf, prtsp->cb_reqbuf, prtsp->cb_reqlen + prtsp->rtsp_hdr_len);
	reqlen = prtsp->cb_reqlen + prtsp->rtsp_hdr_len;
	pns = prtsp->pns;
	src_ip = prtsp->src_ip;
	dst_ip = prtsp->dst_ip;
	fOurSessionId = prtsp->fOurSessionId;

	if (!VALID_NS_TOKEN(prtsp->nstoken)) {
		prtsp->nstoken = rtsp_request_to_namespace(&prtsp->hdr);
	}
	nstoken = prtsp->nstoken;
	prtsp->nsconf =  namespace_to_config(prtsp->nstoken);
	if ((prtsp->nsconf == NULL) || 
			((prtsp->nsconf->http_config->origin.select_method != OSS_RTSP_CONFIG) && 
			(prtsp->nsconf->http_config->origin.select_method != OSS_RTSP_DEST_IP))) {
		pthread_mutex_unlock(&gnm[sockfd].mutex);
		return -1;
	}

	hostlen = 0;
	rv = request_to_origin_server(REQ2OS_CACHE_KEY, 0, &prtsp->hdr, 
				      prtsp->nsconf, 0, 0, &p_host, sizeof(host),
				      &hostlen, &port, 0, 1);
	release_namespace_token_t(prtsp->nstoken);
	if (rv) {
	    DBG_LOG(MSG, MOD_HTTP, 
		    "request_to_origin_server() failed, rv=%d", rv);
		pthread_mutex_unlock(&gnm[sockfd].mutex);
	    return -1;
	}

	cache_uri = 
	    nstoken_to_uol_uri(prtsp->nstoken, abs_uri,
			       strlen(abs_uri), host, hostlen, &cacheurilen);
	if (!cache_uri) {
	    DBG_LOG(MSG, MOD_HTTP, "nstoken_to_uol_uri(token=0x%lx) failed",
	    	    prtsp->nstoken.u.token);
	    /* if unable to get cache_uri, do not cache, but still tunnel.
	     */
	    //return -1;
	}

	/* Unregister from epoll loop and release the prtsp structure*/
	/* Notice: Don't close the socket */
	// NOTE: unregister_NM_socket(fd, TRUE) deletes the fd from epoll loop.
	//unregister_NM_socket(prtsp->fd, TRUE);
	assert(gnm[sockfd].fd_type == RTSP_FD);
	nkn_release_prtsp(prtsp);
	/* change fd type from RTSP to RTSP_OM
	 */
	gnm[sockfd].fd_type = RTSP_OM_FD;

	/* reset reqeust and response buffers */
	//prtsp->cb_reqlen = prtsp->cb_reqlen - prtsp->cb_reqoffsetlen;
	//prtsp->cb_reqoffsetlen = 0;
	//prtsp->cb_resplen = 0;
	//prtsp->cb_respoffsetlen = 0;
	if (cache_uri) {
		cache_name = nkn_malloc_type(cacheurilen +8, mod_rtsp_tunnel_cache_name);
		strcpy(cache_name, cache_uri);
		strcat(cache_name, ".nknc" );
		free(cache_uri);
	}

	/* Create the RTSP tunnel */
	rtsp_make_relay(sockfd, src_ip, dst_ip, pns, reqbuf, reqlen, fOurSessionId, 
		abs_uri, cache_name, nstoken, vfs_filename);
	free(cache_name);
	pthread_mutex_unlock(&gnm[sockfd].mutex);
	return 0;
}

/* ****************************************************************
 * RTSP server functions.
 * ****************************************************************/
void init_rtsp_session(void);
void init_rtsp_session(void)
{
	nkn_task_register_task_type(TASK_TYPE_RTSP_SVR,
				    &rtsp_mgr_input,
				    &rtsp_mgr_output,
				    &rtsp_mgr_cleanup);

	nkn_rttask_register_task_type(
			TASK_TYPE_RTSP_DESCRIBE_HANDLER,
			&rtsp_describe_handler_input,
			&rtsp_describe_handler_output,
			&rtsp_describe_handler_cleanup);

	g_rtsp_cb_origin_tbl = g_hash_table_new(g_str_hash, g_str_equal);
	pthread_mutex_init(&rtsp_origin_tbl_lock, NULL);

        LIST_INIT(&g_rtsp_cb_queue_head);
}

void end_rtsp_server(void);
void end_rtsp_server()
{
	struct rtsp_cb * prtsp;
	while( !LIST_EMPTY(&g_rtsp_cb_queue_head) ) {
		prtsp = (&g_rtsp_cb_queue_head)->lh_first;
		nkn_free_prtsp(prtsp);
	}
}



char *rtspURL(rtsp_session const *serverMediaSession, rtsp_cb_t *prtsp);
char *rtspURL(rtsp_session const *serverMediaSession, rtsp_cb_t *prtsp)
{
        const char *data;
        char rtsp_mrl[256];
        int len;
        u_int32_t attrs;
        int hdrcnt;
	int clientSocket;
	struct sockaddr_in ourAddress;
	char urlBuffer[256]; // more than big enough for "rtsp://<ip-address>:<port>/"
	unsigned int namelen = sizeof(ourAddress);
	unsigned int port;

	clientSocket = prtsp->fd;

        /* bz 4517: the domain should be constructed from the parsed RTSP url and
	 * not the interface ip address as used before this fix
         */
        get_known_header(&prtsp->hdr, RTSP_HDR_X_HOST,
                         &data, &len, &attrs, &hdrcnt);
        if(len > 256) {
                return NULL;
        }
        memcpy(rtsp_mrl, data, len);
        rtsp_mrl[len] = '\0';
	
        getsockname(clientSocket, (struct sockaddr*)&ourAddress, &namelen);

        port = ntohs(ourAddress.sin_port);
        if (port != 554)
                snprintf(urlBuffer, 256, "rtsp://%s:%d/%s", rtsp_mrl, port, serverMediaSession->fStreamName);
        else
                snprintf(urlBuffer, 256, "rtsp://%s/%s",rtsp_mrl, serverMediaSession->fStreamName);

	return nkn_strdup_type(urlBuffer, mod_rtsp_rl_urlp_strdup);
}

void free_rtsp_session(rtsp_session * psm) {

	if(psm->fStreamName) free(psm->fStreamName);
	if(psm->fInfoSDPString) free(psm->fInfoSDPString);
	if(psm->fDescriptionSDPString) free(psm->fDescriptionSDPString);
	if(psm->fMiscSDPLines) free(psm->fMiscSDPLines);
	free(psm);
}

Boolean sm_addSubsession(rtsp_cb_t * prtsp, rtsp_session * psm, void * subsession) {
	int i;
	float ssduration;
	void * psubs;

	// Add this subsession into the queue
	for(i=0; i<MAX_SESSION; i++) {
		if(psm->fSubsessionsHead[i]==NULL) {
			psm->fSubsessionsHead[i]= subsession;
			psm->TrackNumber[i] = ++psm->fSubsessionCounter;
			break;
		}
	}
	if(i==MAX_SESSION) return False; // All full.
	
	// Find out Max/Min duration from all subsessions
        psm->minSubsessionDuration = 0.0;
        psm->maxSubsessionDuration = 0.0;
        for (i=0; i<MAX_SESSION; i++) {
                psubs = psm->fSubsessionsHead[i];
                if(psubs == NULL) break;
                ssduration = (*prtsp->so->pfunc->p_subs_duration)(psubs);
                if (psubs == psm->fSubsessionsHead[0]) { // this is the first subsession
                        psm->minSubsessionDuration = psm->maxSubsessionDuration = ssduration;
                } else if (ssduration < psm->minSubsessionDuration) {
                        psm->minSubsessionDuration = ssduration;
                } else if (ssduration > psm->maxSubsessionDuration) {
                        psm->maxSubsessionDuration = ssduration;
                }
        }

	// Set Max/Min duration into all sessions.
	for(i=0; i<MAX_SESSION; i++) {
                psubs = psm->fSubsessionsHead[i];
                if(psubs == NULL) break;
		(*prtsp->so->pfunc->p_subs_setTrackNumer)(psubs, 
					psm->maxSubsessionDuration,
					psm->minSubsessionDuration,
					psm->TrackNumber[i]);
	}
	return True;
}

float sm_testScaleFactor(rtsp_cb_t * prtsp, rtsp_session * psm, float scale) {
	float minSSScale = 1.0;
	float maxSSScale = 1.0;
	float bestSSScale = 1.0;
	float bestDistanceTo1 = 0.0;
	int i;
	void* subsession;

	for(i=0;i<MAX_SESSION; i++) {
		subsession = psm->fSubsessionsHead[i];
		if(subsession == NULL) break;
		float ssscale = scale;
		ssscale = (*prtsp->so->pfunc->p_subs_testScaleFactor)(subsession, ssscale);
		if (i == 0) { // this is the first subsession
			minSSScale = maxSSScale = bestSSScale = ssscale;
			bestDistanceTo1 = (float)fabs(ssscale - 1.0f);
		} else {
			if (ssscale < minSSScale) {
				minSSScale = ssscale;
			} else if (ssscale > maxSSScale) {
				maxSSScale = ssscale;
			}

			float distanceTo1 = (float)fabs(ssscale - 1.0f);
			if (distanceTo1 < bestDistanceTo1) {
				bestSSScale = ssscale;
				bestDistanceTo1 = distanceTo1;
			}
		}
	}
	if (minSSScale == maxSSScale) {
		scale = minSSScale;
		return scale;
	}

	for(i=0;i<MAX_SESSION; i++) {
		subsession = psm->fSubsessionsHead[i];
		if(subsession == NULL) break;
		float ssscale = bestSSScale;
		ssscale = (*prtsp->so->pfunc->p_subs_testScaleFactor)(subsession, ssscale);
		if (ssscale != bestSSScale) break; // no luck
	}
	if (subsession == NULL) {
		// All subsessions are at the same scale: bestSSScale
		scale = bestSSScale;
		return scale;
	}

	// Still no luck.  Set each subsession's scale to 1:
	for(i=0;i<MAX_SESSION; i++) {
		subsession = psm->fSubsessionsHead[i];
		if(subsession == NULL) break;
		float ssscale = 1;
		ssscale = (*prtsp->so->pfunc->p_subs_testScaleFactor)(subsession, ssscale);
	}
	//scale = 1;
	return scale;
}

char *sm_generate_piecewise_SDPDescription(rtsp_cb_t *prtsp, rtsp_session *psm);
char *sm_generate_piecewise_SDPDescription(rtsp_cb_t *prtsp, rtsp_session *psm)
{
    char *media_sdp[MAX_SESSION];
    int32_t media_sdp_len[MAX_SESSION], md_sdp_len;
    char *header, *range, *ip_addr;
    int32_t header_length, i, max_session = 0, upto_rangelen;
    float duration;
    struct in_addr ipaddr;
    void *subsession;

    for(i =  0; i < MAX_SESSION; i++) {
	subsession = psm->fSubsessionsHead[i];
	if(subsession == NULL) break;
	media_sdp[i] = (char*)(*prtsp->so->pfunc->p_subs_sdpLines)(subsession);
	strncpy(prtsp->trackID[i], (*prtsp->so->pfunc->p_subs_trackId)(subsession), TRACKID_LEN);
	if (media_sdp[i] == NULL) continue; // the media's not available
	++max_session;
	md_sdp_len += media_sdp_len[i] = strlen(media_sdp[i]);
    }

    if (psm->maxSubsessionDuration != psm->minSubsessionDuration) {
	duration = -(psm->maxSubsessionDuration); // because subsession durations differ
    } else {
	duration = psm->maxSubsessionDuration; // all subsession durations are the same
    }
    if (duration == 0.0) {
	range = nkn_strdup_type("a=range:npt=0-\r\n", mod_rtsp_ses_gsdp_strdup);
    } else if (duration > 0.0) {
	char buf[100];
	snprintf(buf, 100, "a=range:npt=0-%.3f\r\n", duration);
	range = nkn_strdup_type(buf, mod_rtsp_ses_gsdp_strdup);
    } else { // subsessions have differing durations, so "a=range:" lines go there
	range = nkn_strdup_type("", mod_rtsp_ses_gsdp_strdup);
    }
    ipaddr.s_addr = prtsp->dst_ip;
    ip_addr = inet_ntoa(ipaddr);
    header_length = 5 + // v-len 
		    60 + strlen(ip_addr) + // o-len
		    4 + strlen(psm->fDescriptionSDPString) + // session-len
		    4 + strlen(psm->fInfoSDPString) + // session-info
		    strlen(range) + // range-len
		    md_sdp_len ; // media-sdp-len

    header = nkn_calloc_type(1, header_length, mod_rtsp_ses_gsd_malloc);

    upto_rangelen = snprintf(header, header_length, "v=0\r\no=- %10ld%10ld %d IN IP4 %s\r\ns=%s\r\ni=%s\r\n%s",
			    psm->fCreationTime.tv_sec, psm->fCreationTime.tv_usec, 1, ip_addr,
			    psm->fDescriptionSDPString , psm->fInfoSDPString, range);

    for ( i = 0; i < max_session; ++i) {
	snprintf(&header[upto_rangelen], media_sdp_len[i]+1, "%s", media_sdp[i]);  
	upto_rangelen += (media_sdp_len[i]);//(media_sdp_len[i] - 1);
	prtsp->nMediaTracks++;
	if(media_sdp[i]) {
	    /* free the sdp data generated by the media codec. the
	     * media codec would never know when we are done, hence it
	     * is the session layer's responsibility to do this 
	     */
	    free(media_sdp[i]);
	}
    } 

    free(range);
    return header;
}

char* sm_generate_whole_SDPDescription(rtsp_cb_t * prtsp, rtsp_session * psm);
char* sm_generate_whole_SDPDescription(rtsp_cb_t * prtsp, rtsp_session * psm) 
{
	char const* sdpLines;
	char * sdp_buf;
	int sdp_buflen;
	void* subsession;
	int i;

	subsession = psm->fSubsessionsHead[0];
	if(subsession == NULL) return NULL;

	sdpLines = (*prtsp->so->pfunc->p_subs_sdpLines)(subsession);
	sdp_buflen = strlen(sdpLines)+100;
	sdp_buf = (char *)nkn_malloc_type(sizeof(char)*sdp_buflen,
					mod_rtsp_ses_gsd_malloc);
	if (sdp_buf == NULL) return NULL;
	make_up_sdp_string((char *)sdpLines, sdp_buf, sdp_buflen, prtsp->dst_ip);
	free((char *)sdpLines);

	for (i=0; i< MAX_SESSION; i++) {
		subsession = psm->fSubsessionsHead[i];
		if(subsession == NULL) break;
		strncpy(prtsp->trackID[i], (*prtsp->so->pfunc->p_subs_trackId)(subsession), TRACKID_LEN);
		prtsp->nMediaTracks++;
	}

	return sdp_buf;
}

///////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////

void delete_rtsp_session(rtsp_cb_t * prtsp, rtsp_session* serverMediaSession);
rtsp_session* find_rtsp_session(rtsp_cb_t * prtsp, char * streamName, int * has_tunneled);

static rtsp_session* create_rtsp_session(rtsp_cb_t * prtsp, char * fileName);
static rtsp_session* create_rtsp_session(rtsp_cb_t * prtsp, char * fileName) 
{
	rtsp_session* psms;
	void * pvideo, * paudio;
	char * pdesc;
	char * extension;
	Boolean ret;
        char modified_filename[256];
        struct in_addr in;
        const char *data;
	const char *port;
	int portlen;
        char rtsp_mrl[256];
	char in_port[256];
        int len, rv;
        u_int32_t attrs;
        int hdrcnt;
	//const int rtsp_def_port = 554;

	extension = strrchr(fileName, '.');
	if (extension == NULL) return NULL;

	if ((!video_ankeena_fmt_enable) && strcmp(extension, ".mp4") != 0) {
		return NULL;
	}

	// Use the file name extension to determine the type of "rtsp_session":
	if (strcmp(extension, ".mp4") == 0) { 
		pdesc = (char *)"MPEG-4 Stream"; 
		AO_fetch_and_add1(&glob_rtsp_tot_mp4_session);
	} else { 
		pdesc = (char *)"unknown"; 
		AO_fetch_and_add1(&glob_rtsp_tot_unknown_session);
	}


	/*
	 * Create the psms structure.
	 */
        psms = (rtsp_session *)nkn_malloc_type(sizeof(rtsp_session), mod_rtsp_ses_nsms_malloc);
        if(!psms) return NULL;
        memset((char *)psms, 0, sizeof(rtsp_session));

        psms->fIsSSM = False;
        psms->fDeleteWhenUnreferenced = False;
        psms->fStreamName = nkn_strdup_type(fileName, mod_rtsp_ses_nsms_strdup);
        psms->fInfoSDPString = nkn_strdup_type(fileName, mod_rtsp_ses_nsms_strdup);
        psms->fDescriptionSDPString = nkn_strdup_type(pdesc, mod_rtsp_ses_nsms_strdup);
	psms->fMiscSDPLines = nkn_strdup_type("", mod_rtsp_ses_nsms_strdup);
        gettimeofday(&psms->fCreationTime, NULL);


	/*
	 * Fill up the prtsp structure.
	 * and create the subsessions.
	 */
	if (prtsp->is_ankeena_fmt) {
		prtsp->so = rtsp_get_so_libname((char *)ANKEENA_FMT_EXT);
	}
	else {
		prtsp->so = rtsp_get_so_libname(extension);
	}
	if (prtsp->so == NULL) {
		free_rtsp_session(psms);
		return NULL;
	}

        in.s_addr = prtsp->dst_ip;

	/* bz 4517: the domain should be constructed from the parsed RTSP url and
	 * not the interface ip address as used before this fix
	 */
        get_known_header(&prtsp->hdr, RTSP_HDR_X_HOST,
			 &data, &len, &attrs, &hdrcnt);
        if(len > 256) {
                return NULL;
        }
        memcpy(rtsp_mrl, data, len);
        rtsp_mrl[len] = '\0';

        rv = get_known_header(&prtsp->hdr, RTSP_HDR_X_PORT,
			      &port, &portlen, &attrs, &hdrcnt);
	/* no port info available in the URI */
	if(!portlen) { 

	    /* read port info from config nodes */
	    if(nkn_rtsp_vod_serverport[0] != 0) { 
		ret = snprintf(modified_filename, 
			       sizeof(modified_filename),
			       ":S:%s:%s", rtsp_mrl, fileName);		
	    } else { 
		/* port info not available in config nodes, resort to default */
		ret = snprintf(modified_filename, 
			       sizeof(modified_filename),
			       ":S:%s:%s", rtsp_mrl, fileName);
	    }
	} else {

	    memcpy(in_port, port, portlen);
	    in_port[portlen] = '\0';

	    /* port info available in URI */
	    ret = snprintf(modified_filename, 
			   sizeof(modified_filename),
			   ":R:%s:%s:%s", rtsp_mrl, in_port, fileName);
	}	

	ret = (*prtsp->so->pfunc->p_new_subsession)(
			modified_filename, 
			prtsp->fd, 
			prtsp->pns->if_addrv4,
			&pvideo, 
			&paudio);
	if(ret == False) {
		//printf("Error=%d\r\n", ret);
		free_rtsp_session(psms);
		return NULL;
	}
	if(pvideo) sm_addSubsession(prtsp, psms, pvideo);
	if(paudio) sm_addSubsession(prtsp, psms, paudio);
	return psms;
}

#if 1
/*
 * return NULL: means we tunneled the request.
 */
rtsp_session* find_rtsp_session(rtsp_cb_t * prtsp, char * streamName, int * has_tunneled) 
{
	rtsp_session* psms = NULL;
	char modified_filename[1024];
	FILE* fid, *fid1;
	const char *data;
	const char *port;
	int portlen;
	char rtsp_mrl[256];
	char in_port[256];
	int len;
	u_int32_t attrs;
	int hdrcnt;
	struct in_addr in;
	int ret, rv;
	char nkn_internal_cache_name[1024];
	char file_name[1024];
	char *p;
	//	const int rtsp_def_port = 554;

	*has_tunneled = FALSE;
	if (!streamName) return NULL;

	fid = (FILE*)-1;
	in.s_addr = prtsp->dst_ip;
	get_known_header(&prtsp->hdr, RTSP_HDR_X_HOST,
			 &data, &len, &attrs, &hdrcnt);
	
	rv = get_known_header(&prtsp->hdr, RTSP_HDR_X_PORT,
			      &port, &portlen, &attrs, &hdrcnt);

	if(len > 256) {
		return NULL;
	}
	memcpy(rtsp_mrl, data, len);
	rtsp_mrl[len] = '\0';

	/* If there is query params, ignore those for cache file name.
	 */
	p = strchr(streamName, '?');
	if (p) {
		strncpy(file_name, streamName, p - streamName);
		file_name[p - streamName] = 0;
	}
	else {
		strcpy(file_name, streamName);
	}
	
	/* no port info in the URL*/
	if(!portlen) { 
		/* read port info from config nodes */
		if(nkn_rtsp_vod_serverport[0] != 0) { 
			ret = snprintf(modified_filename, 
				       sizeof(modified_filename),
				       ":S:%s:%s", rtsp_mrl, file_name);
		} else { 
			/* port info not available in config nodes, resort to default */
			ret = snprintf(modified_filename, 
				       sizeof(modified_filename),
				       ":S:%s:%s", rtsp_mrl, file_name);
		}
	} else {
		memcpy(in_port, port, portlen);
		in_port[portlen] = '\0';
		/* port info available in URI */
		ret = snprintf(modified_filename, 
			   sizeof(modified_filename),
			   ":R:%s:%s:%s", rtsp_mrl, in_port, file_name);
	}
	
	//printf("ret=%d filename=%s\n", ret, modified_filename);

	/* Bz4735: default to NKNC file if it exists; and then check for other files
	 */
	/*
	 * First, check whether the specified "streamName" exists as a local file:
	 */
	//	fid = vfs_funs.pnkn_fopen(modified_filename, "rb");

	/*
	 * Secondly, check whether we already have a "rtsp_session" for this file:
	 */
	LIST_FOREACH(psms, &prtsp->sms_queue_head, entries)
	{
		if(psms->fStreamName && (strcmp(psms->fStreamName, file_name)==0)) {
			fid = (FILE*)-1;
			goto close_fd;
		}
	}

	/*
	 * if file does not exist, check out our native cache format
	 */
	strcpy(nkn_internal_cache_name, modified_filename);
	strcat(nkn_internal_cache_name, ANKEENA_FMT_EXT);
	fid = vfs_funs.pnkn_fopen(nkn_internal_cache_name, "I");
	fid1 = (FILE*)fid;
	if(fid) {
		prtsp->is_ankeena_fmt = 1;
	} else {
		prtsp->is_ankeena_fmt = 0;
	}
	

	/* File exists */
	if (psms == NULL) {
		/* Create a new "rtsp_session" object for streaming from the named file. */
		if(prtsp->is_ankeena_fmt == 1) {
			psms = create_rtsp_session(prtsp, file_name);
		} else { 
		    /*
		     * First, check whether the specified "streamName" exists as a local file:
		     */
		    fid = vfs_funs.pnkn_fopen(modified_filename, "I");
		    if(fid) {
			psms = create_rtsp_session(prtsp, file_name);
		    }
	        }
		if(psms == NULL) {
                        prtsp->mediacodec_error_code = -1;
			vfs_funs.pnkn_fclose(fid);
			goto Tunnel_RTSP;
		}
		LIST_INSERT_HEAD(&prtsp->sms_queue_head, psms, entries);
	}

	/*
	 * Thirdly, we take care of four combinations here about fid and sms.
	 */
	if (fid == NULL) {
		if (psms != NULL) {
			/* "sms" was created for a file that no longer exists. Remove it */
			delete_rtsp_session(prtsp, psms);
		}
		/* If file does not exist, we tunnel over the RTSP transaction */
		goto Tunnel_RTSP;
	} 


	if(fid !=(FILE*) -1) {
	    vfs_funs.pnkn_fclose(fid);
	}

 close_fd:
	/* Increment counter only once for a video. The counter could be wrong
	 * if the player disconnects after DESCRIBE.
	 * BZ 4707
	 */
	if (prtsp->method == METHOD_DESCRIBE) {
	    //AO_fetch_and_add1(&glob_tot_video_delivered_with_hit);
	}
	return psms;

Tunnel_RTSP:
	/* 
	 * Also launch the internal RTSP client to download the full video file.
	 */
	if ((prtsp->method == METHOD_OPTIONS) || (prtsp->method == METHOD_DESCRIBE)) {
		//prtsp->state = RSTATE_RELAY;
		/* prtsp is freed, if tunnelling is Successful.
		 */
		if (tunnel_RTSP_request(prtsp, nkn_internal_cache_name) == 0) {
			*has_tunneled = TRUE;
		}
	}
	return NULL;
}
#else
/*
 * return NULL: means we tunneled the request.
 */
rtsp_session* find_rtsp_session(rtsp_cb_t * prtsp, char * streamName, int * has_tunneled) 
{
    rtsp_session* psms = NULL;
    char modified_filename[1024];
    FILE *fid, *fid1;
    const char *data;
    const char *port;
    int portlen;
    char rtsp_mrl[256];
    char in_port[256];
    int len;
    u_int32_t attrs;
    int hdrcnt;
    struct in_addr in;
    int ret, rv;
    //const int rtsp_def_port = 554;

    *has_tunneled = FALSE;
    if (!streamName) return NULL;

    in.s_addr = prtsp->dst_ip;
    get_known_header(&prtsp->hdr, RTSP_HDR_X_HOST,
		     &data, &len, &attrs, &hdrcnt);
    
    rv = get_known_header(&prtsp->hdr, RTSP_HDR_X_PORT,
			  &port, &portlen, &attrs, &hdrcnt);

    if(len > 256) {
	return NULL;
    }
    memcpy(rtsp_mrl, data, len);
    rtsp_mrl[len] = '\0';
    
    /* no port info in the URL*/
    if(!portlen) { 

	/* read port info from config nodes */
	if(nkn_rtsp_vod_serverport[0] != 0) { 
	    ret = snprintf(modified_filename, 
			   sizeof(modified_filename),
			   ":S:%s:%s", rtsp_mrl, streamName);
	} else { 
	    /* port info not available in config nodes, resort to default */
	    ret = snprintf(modified_filename, 
			   sizeof(modified_filename),
			   ":S:%s:%s", rtsp_mrl, streamName);
	}
    } else {

	memcpy(in_port, port, portlen);
	in_port[portlen] = '\0';
	/* port info available in URI */
	ret = snprintf(modified_filename, 
		       sizeof(modified_filename),
		       ":R:%s:%s:%s", rtsp_mrl, in_port, streamName);
    }
    
    //printf("ret=%d filename=%s\n", ret, modified_filename);

    /*
     * First, check whether the specified "streamName" exists as a local file:
     */
    fid1 = fid = vfs_funs.pnkn_fopen(modified_filename, "I");

    /*
     * if file does not exist, check out our native cache format
     */
    if(fid == NULL) { 
	char nkn_internal_cache_name[256];

	strcpy(nkn_internal_cache_name, modified_filename);
	strcat(nkn_internal_cache_name, ANKEENA_FMT_EXT);
	fid = vfs_funs.pnkn_fopen(nkn_internal_cache_name, "I");
	prtsp->is_ankeena_fmt = 1;
    }
    else {
	prtsp->is_ankeena_fmt = 0;
    }

    /*
     * Secondly, check whether we already have a "rtsp_session" for this file:
     */
    LIST_FOREACH(psms, &prtsp->sms_queue_head, entries)
	{
	    if(psms->fStreamName && (strcmp(psms->fStreamName, streamName)==0)) {
		break;
	    }
	}

    /*
     * Thirdly, we take care of four combinations here about fid and sms.
     */
    if (fid == NULL) {
	if (psms != NULL) {
	    /* "sms" was created for a file that no longer exists. Remove it */
	    delete_rtsp_session(prtsp, psms);
	}
	/* If file does not exist, we tunnel over the RTSP transaction */
	goto Tunnel_RTSP;
    } 

    /* File exists */
    if (psms == NULL) {
	/* Create a new "rtsp_session" object for streaming from the named file. */
	psms = create_rtsp_session(prtsp, streamName);
	if(psms == NULL) {
	    vfs_funs.pnkn_fclose(fid);
	    if(fid1) {
		/* file is in cache/http or nfs origin and media codec
		   does not support it
		*/
		prtsp->mediacodec_error_code = -1;
		return NULL;
	    }

	    goto Tunnel_RTSP;
	}
	LIST_INSERT_HEAD(&prtsp->sms_queue_head, psms, entries);
    }
    vfs_funs.pnkn_fclose(fid);
    /* Increment counter only once for a video. The counter could be wrong
     * if the player disconnects after DESCRIBE.
     * BZ 4707
     */
    if (prtsp->method == METHOD_DESCRIBE) {
	//AO_fetch_and_add1(&glob_tot_video_delivered_with_hit);
    }
    return psms;

 Tunnel_RTSP:
    /* 
     * Also launch the internal RTSP client to download the full video file.
     */
    if ((prtsp->method == METHOD_OPTIONS) || (prtsp->method == METHOD_DESCRIBE)) {
	pthread_mutex_unlock(&prtsp->mutex);
	prtsp->state = RSTATE_RELAY;
	tunnel_RTSP_request(prtsp);
	*has_tunneled = TRUE;
    }
    return NULL;
}
#endif

rtsp_om_task_t * create_rtsp_om_task(char *path, rtsp_cb_t *prtsp)
{
	char *pfilename, *pdomain;
	char buf[256], domain[256];
	struct in_addr in;
	rtsp_om_task_t *pfs;

	if(*path == ':') {
		int len;

		pdomain = (char *)path +1;
		pfilename = strchr(pdomain, ':');
		if (!pfilename) {
			return NULL;
		}

		len = pfilename - pdomain;
		memcpy(domain, pdomain, len);
		domain[len] = 0;

		// skip ':'
		pfilename ++;
	}
	else {
		in.s_addr = eth0_ip_address;
		snprintf(domain, sizeof(domain), "%s", inet_ntoa(in));

		pfilename = (char *)path;
	}

	pfs = (rtsp_om_task_t *) nkn_malloc_type(sizeof(rtsp_om_task_t), mod_rtsp_om_malloc);
	memset((char *)pfs, 0, sizeof(rtsp_om_task_t));
	pfs->con = prtsp;
	pfs->filename = nkn_strdup_type(pfilename, mod_rtsp_om_strdup);
	//pfs->pps = NULL;

	/*
	 * Just a hack to convert RTSP to HTTP protocol
	 * parameters inside may not matter.
	 */
	//pfs->con->http.flag |= HRF_HTTP_11;
	init_http_header(&pfs->con->hdr, 0, 0);
	strcpy(buf, "GET");
	add_known_header(&pfs->con->hdr, MIME_HDR_X_NKN_METHOD, buf, strlen(buf));
	snprintf(buf, sizeof(buf), "/%s", pfs->filename);
	add_known_header(&pfs->con->hdr, MIME_HDR_X_NKN_URI, buf, strlen(buf));
	add_known_header(&pfs->con->hdr, MIME_HDR_HOST, domain, strlen(domain));
	pfs->ns_token = http_request_to_namespace(&pfs->con->hdr, NULL);
	if(pfs->ns_token.u.token == 0) {
		shutdown_http_header(&pfs->con->hdr);
		free(pfs->con->cb_reqbuf);
		free(pfs->con->cb_respbuf);
		free(pfs->con);
		free(pfs);
		return NULL;
	}

	return pfs;
}

void delete_rtsp_session(rtsp_cb_t * prtsp, rtsp_session* serverMediaSession) 
{
	rtsp_session * sms;

	if (serverMediaSession == NULL) return;
	LIST_FOREACH(sms, &prtsp->sms_queue_head, entries)
	{
		if(sms != serverMediaSession) continue;
		LIST_REMOVE(sms, entries);
#if 0
		for(i=0; i<MAX_SESSION; i++) {
			if(sms->fSubsessionsHead[i]!=NULL) continue;
			pthread_mutex_lock( &prtsp->so->mutex );
			(*prtsp->so->pfunc->p_subs_deleteStream)(sms->fSubsessionsHead[i]);
			pthread_mutex_unlock( &prtsp->so->mutex );
		}
#endif // 0
		free_rtsp_session(sms);
		return;
	}
}

void delete_rtsp_session_from_rtsp_list(rtsp_cb_t * prtsp, rtsp_session* serverMediaSession) 
{
	rtsp_session * sms;
	int i;


	if (serverMediaSession == NULL) return;
	LIST_FOREACH(sms, &prtsp->sms_queue_head, entries)
	{
		if(sms != serverMediaSession) continue;
		LIST_REMOVE(sms, entries);

		for(i=0; i<MAX_SESSION; i++) {
			if(sms->fSubsessionsHead[i]==NULL) continue;
			(*prtsp->so->pfunc->p_subs_deleteStream)(sms->fSubsessionsHead[i],
								 0, NULL);
			if (prtsp->fStreamStates)
				free_RTCPInstance(&prtsp->fStreamStates[i]);
			
		}
		
		free_rtsp_session(sms);
		return;
	}
}

/* ****************************************************************
 * Response of RTSP different requests
 * ****************************************************************/

int handleCmd_DESCRIBE(rtsp_cb_t * prtsp) 
{
	unsigned int sdpDescriptionSize;
	char* sdpDescription = NULL;
	char* prtspURL;
	rtsp_session* session;
	int has_tunneled;

	if ( !(prtsp->state == RSTATE_INIT || prtsp->state == RSTATE_RCV_OPTIONS) ) {
		RTSP_SET_ERROR(prtsp, RTSP_STATUS_400, NKN_RTSP_PVE_REQ_CSEQ);
		prtsp->cb_resplen = 0;
		prtsp->status = 400;
		rtsp_build_response(prtsp, prtsp->status_enum,
				    prtsp->sub_error_code);
		AO_fetch_and_add1(&glob_rtsp_tot_status_resp_400);
		return 0;
	}

	/*
	 * We should really check that the request contains an "Accept:" #####
	 * for "application/sdp", because that's what we're sending back #####
	 */

	/*
	 * Find the namespace name here
	 */
	{
		//const namespace_config_t *nsconf = NULL;

		if (!VALID_NS_TOKEN(prtsp->nstoken)) {
			prtsp->nstoken = rtsp_request_to_namespace(&prtsp->hdr);
			release_namespace_token_t(prtsp->nstoken);
		}
		//nsconf = namespace_to_config(prtsp->nstoken);
		
		prtsp->nsconf = namespace_to_config(prtsp->nstoken);
		if (prtsp->nsconf == NULL) {
			handleCmd_notFound(prtsp);
			return 0;
		}
		
		if (prtsp->nsconf->http_config) {
		    /* check if RTSP delivery is enabled for the namespace */
		    if ((prtsp->nsconf->http_config->response_protocol & DP_RTSTREAM) == 0) {
			handleCmd_unsupportedService(prtsp);
			return 0;
		    }
		}

		/* Requirement 2.1 - 34. 
		 * Check for maximum session count reached in namespace.
		 */
		if (!nvsd_rp_alloc_resource(RESOURCE_POOL_TYPE_CLIENT_SESSION, prtsp->nsconf->rp_index, 1)) {
			/* This is going to return a pre-fabricated 
			 * 503 error message, with no sub-error code.
			 */
			handleCmd_unsupportedService(prtsp);
			return 0;

		}
		AO_fetch_and_add1((volatile AO_t *) &(prtsp->nsconf->stat->g_rtsp_sessions));
	}
	
	char *urlPreSuffix;
	RTSP_GET_ALLOC_ABS_URL(&prtsp->hdr, urlPreSuffix);
	session = find_rtsp_session(prtsp, &(urlPreSuffix[1]), &has_tunneled);
	if (session == NULL) {
		if (has_tunneled == TRUE) {
			return -1;
		} else {
		    if(prtsp->mediacodec_error_code == -1) {
			handleCmd_unsupportedMediaCodec(prtsp);
			return 0;
		    }
		    handleCmd_notFound(prtsp);
		    return 0;
		}
	}

	/*
	 * Then, assemble a SDP description for this session:
	 */
	if (prtsp->is_ankeena_fmt) {
		sdpDescription = sm_generate_whole_SDPDescription(prtsp, session);
	}
	else {
		// Everybody needs to generate whole SDP description
		sdpDescription = sm_generate_piecewise_SDPDescription(prtsp, session);
		//sdpDescription = sm_generateSDPDescription(prtsp, session);
	}

	if (sdpDescription == NULL) {
		/*
		 * This usually means that a file name that was specified for a
		 * "ServerMediaSubsession" does not exist.
		 */
		prtsp->status = 404;
		prtsp->cb_resplen = snprintf(prtsp->cb_respbuf, RTSP_BUFFER_SIZE,
			"RTSP/1.0 404 File Not Found, Or In Incorrect Format\r\n"
			"Server: %s\r\n"
			"CSeq: %s\r\n"
			"Date: %s\r\n"
			"\r\n",
			rl_rtsp_server_str,
			prtsp->cseq,
			nkn_get_datestr(NULL));
		AO_fetch_and_add1(&glob_rtsp_tot_status_resp_404);
		NS_INCR_STATS(prtsp->nsconf, PROTO_RTSP, client, resp_4xx);
		NS_INCR_STATS(prtsp->nsconf, PROTO_RTSP, client, resp_404);
		return 0;
	}
	sdpDescriptionSize = strlen(sdpDescription);

	/*
	 * Also, generate our RTSP URL, for the "Content-Base:" header
	 * (which is necessary to ensure that the correct URL gets used in
	 * subsequent "SETUP" requests).
	 */
	prtspURL = rtspURL(session, prtsp);
	prtsp->content_base = nkn_strdup_type(prtspURL, mod_rtsp_ses_hd_strdup);
	prtsp->status = 200;
	prtsp->cb_resplen = snprintf(prtsp->cb_respbuf, RTSP_BUFFER_SIZE,
		"RTSP/1.0 200 OK\r\n"
		"Server: %s\r\n"
		"CSeq: %s\r\n"
		"Date: %s\r\n"
		"Content-Type: application/sdp\r\n"
		"Content-Base: %s/\r\n"
		"Content-Length: %d\r\n"
		"\r\n"
		"%s",
		rl_rtsp_server_str,
		prtsp->cseq,
		nkn_get_datestr(NULL),
		prtspURL,
		sdpDescriptionSize,
		sdpDescription);
	AO_fetch_and_add1(&glob_rtsp_tot_status_resp_200);
	NS_INCR_STATS(prtsp->nsconf, PROTO_RTSP, client, resp_2xx);
	NS_INCR_STATS(prtsp->nsconf, PROTO_RTSP, client, resp_200);
      	free(sdpDescription);
	free(prtspURL);
	return 0;
}


int handleCmd_SETUP(rtsp_cb_t * prtsp) 
{
	char * streamName = &prtsp->urlPreSuffix[1];
	char const* trackId = prtsp->urlSuffix;
	unsigned int i;
	unsigned streamNum;
	char *full_url;
	int has_tunneled;
	uint16_t clientRTPPort;
	uint16_t clientRTCPPort;
	int tcpSocketNum;
	uint32_t destinationAddress;
	u_int8_t destinationTTL;
	uint16_t serverRTPPort;
	uint16_t serverRTCPPort;
	uint32_t ssrc;
	struct in_addr destinationAddr;
	char* destAddrStr;
	struct sockaddr_in sourceAddr; 
	unsigned namelen;
	char* sourceAddrStr;
	void * subsession;


	RTSP_GET_ALLOC_URL(&prtsp->hdr, full_url);
	if (strncmp(full_url, prtsp->content_base, strlen(prtsp->content_base)) != 0) {
	    //free(full_url);
		handleCmd_notFound(prtsp);
		return 0;
	}
	
	/* bz 4367:We assume that by the time SETUP command is encountered we have decided that 
	 * the file is in the cache (since decision to tunnel/relay happens in DESCRIBE/OPTIONS
	 * command itself). At this state VFS calls were safe to be made in the Network
	 * thread itself. In this specific instance Codenomicon sends URI's that are different
	 * to that sent in DESCRIBE. This causes a deadlock in the NETWORK thread as VFS tries to 
	 * open connections to the ORIGIN in the same thread
	 * Returned error if the DESCRIBE URI's and SETUP URI's are different. this case is valid
	 * since the SDP is provided by MFD only and we do not support other methods to fetch SDP 
	 * and start a session with SETUP command
	*/
	i = 0;
	for(i = 0; i < prtsp->nMediaTracks; i++) {
		char track_url[1024];
		if(prtsp->content_base[strlen(prtsp->content_base) - 1] == '/') {
			snprintf(track_url, sizeof(track_url), 
				"%s%s", prtsp->content_base, prtsp->trackID[i]);
		} else {
			snprintf(track_url, sizeof(track_url), 
				"%s/%s", prtsp->content_base, prtsp->trackID[i]);
		}
		if(strcmp(track_url, full_url)==0) {
			break;
		}
	}
	//free(full_url);
	if(i == prtsp->nMediaTracks) {
		handleCmd_notFound(prtsp);
		return 0;
	}

	// Check whether we have existing session state, and, if so, whether it's
	// for the session that's named in "streamName".  (Note that we don't
	// support more than one concurrent session on the same client connection.) #####
	if ( (prtsp->fOurrtsp_session != NULL) &&
	     (strcmp(streamName, prtsp->fOurrtsp_session->fStreamName) != 0) ) 
	{
		prtsp->fOurrtsp_session = NULL;
	}

	if (prtsp->fOurrtsp_session == NULL) {
		// Set up this session's state.

		// Look up the "rtsp_session" object for the specified stream:
		if (streamName[0] == '\0') {
			streamName = prtsp->urlSuffix;
			trackId = NULL;
		}
		prtsp->fOurrtsp_session = find_rtsp_session(prtsp, streamName, &has_tunneled);
		if (prtsp->fOurrtsp_session == NULL) {
			if (has_tunneled == TRUE) {
			    return -1;
			} else {
				handleCmd_notFound(prtsp);
				return 0;
			}
		}

		++prtsp->fOurrtsp_session->fReferenceCount;

		// Set up our array of states for this session's subsessions (tracks):
		reclaim_rtsp_session_states(prtsp);
		for(prtsp->fNumStreamStates=0, i=0; 
			prtsp->fNumStreamStates<MAX_SESSION; 
			++i, ++prtsp->fNumStreamStates) {
				if(prtsp->fOurrtsp_session->fSubsessionsHead[i]==NULL) 
					break;
		}
    		prtsp->fStreamStates = (struct streamState *)nkn_malloc_type(prtsp->fNumStreamStates * sizeof(struct streamState), mod_rtsp_ses_hs_malloc);
		//iter.reset();
		for (i = 0; i < prtsp->fNumStreamStates; ++i) {
			prtsp->fStreamStates[i].subsession = NULL;
			prtsp->fStreamStates[i].streamToken = NULL;
			prtsp->fStreamStates[i].rtcp_udp_fd = -1;
			prtsp->fStreamStates[i].rtcpPortNum = 0;
			prtsp->fStreamStates[i].isTimeout = FALSE;
			prtsp->fStreamStates[i].prtsp = prtsp;
		}
	}

	// Look up information for the specified subsession (track):
	subsession = NULL;
	if (trackId != NULL && trackId[0] != '\0') { // normal case
		for (streamNum = 0; streamNum < prtsp->fNumStreamStates; ++streamNum) {
			subsession = prtsp->fOurrtsp_session->fSubsessionsHead[streamNum];
			if (subsession != NULL && strcmp(trackId, (*prtsp->so->pfunc->p_subs_trackId)(subsession)) == 0) 
				break;
		}
		if (streamNum >= prtsp->fNumStreamStates) {
			// The specified track id doesn't exist, so this request fails:
			handleCmd_notFound(prtsp);
			return 0;
		}
	} else {
		if (prtsp->fNumStreamStates != 1) {
			handleCmd_bad(prtsp);
			return 0;
		}
		streamNum = 0;
		subsession = prtsp->fOurrtsp_session->fSubsessionsHead[streamNum];
	}
	// ASSERT: subsession != NULL
	prtsp->fStreamStates[streamNum].subsession = subsession;

	/*
	 * Look for a "Transport:" header in the request string,
	 * to extract client parameters:
	 */
	if ((prtsp->streamingMode == RTP_TCP) && (prtsp->rtpChannelId == 0xFF)) {
		/*
		 * TCP streaming was requested, but with no "interleaving=" fields.
		 * (QuickTime Player sometimes does this.)  Set the RTP and RTCP 
		 * channel ids to proper values:
		 */
		prtsp->rtpChannelId = prtsp->fTCPStreamIdCount; 
		prtsp->rtcpChannelId = prtsp->fTCPStreamIdCount+1;
	}
	prtsp->fTCPStreamIdCount += 2;

	clientRTPPort = htons(prtsp->clientRTPPortNum);
	clientRTCPPort = htons(prtsp->clientRTCPPortNum);

	/*
	 * Next, check whether a "Range:" header is present in the request.
	 * This isn't legal, but some clients do this to combine "SETUP" and "PLAY":
	 */
	prtsp->fStreamAfterSETUP = (prtsp->flag & HAS_RANGE_HEAD) ||
			    (prtsp->flag & HAS_PLAYNOW_HEAD);

	// Then, get server parameters from the 'subsession':
	tcpSocketNum = (prtsp->streamingMode == RTP_TCP) ? prtsp->fd : -1;
	destinationAddress = 0;
	destinationTTL = 255;
	if (prtsp->destinationAddressStr[0] != 0) {
		// Use the client-provided "destination" address.
		// Note: This potentially allows the server to be used in denial-of-service
		// attacks, so don't enable this code unless you're sure that clients are
		// trusted.
		destinationAddress = inet_addr(prtsp->destinationAddressStr);
	}
	// Also use the client-provided TTL.
	destinationTTL = prtsp->destinationTTL;
	serverRTPPort = 0;
	serverRTCPPort = 0;
	(prtsp->fStreamStates[streamNum]).streamToken =
	 	(*prtsp->so->pfunc->p_subs_getStreamParameters)(subsession, 
				prtsp->fOurSessionId, prtsp->src_ip,
				&clientRTPPort, &clientRTCPPort,
				tcpSocketNum, prtsp->rtpChannelId, prtsp->rtcpChannelId,
				&destinationAddress, &destinationTTL, &prtsp->fIsMulticast,
				&serverRTPPort, &serverRTCPPort,
				&ssrc,
				(prtsp->fStreamStates[streamNum]).streamToken);

	/*
	 * MAXHE: launch the RTCP server at this time.
	 */
	if(prtsp->streamingMode == RTP_UDP) {
		char cname[1024];
		struct in_addr in;
	
		in.s_addr=prtsp->dst_ip;
		snprintf(&cname[0], 1024, "admin@%s", inet_ntoa(in));
		RTCP_createNew(prtsp, streamNum, &cname[0], serverRTPPort);
		serverRTCPPort = (prtsp->fStreamStates[streamNum]).rtcpPortNum;
	}

	prtsp->need_session_id = TRUE;
	destinationAddr.s_addr = destinationAddress;
	destAddrStr = nkn_strdup_type(inet_ntoa(destinationAddr), mod_rtsp_ses_hs_strdup);
	namelen = sizeof(sourceAddr);
	getsockname(prtsp->fd, (struct sockaddr*)&sourceAddr, &namelen);
	sourceAddrStr = nkn_strdup_type(inet_ntoa(sourceAddr.sin_addr), mod_rtsp_ses_hs_strdup);
	if (prtsp->fIsMulticast) {
	switch (prtsp->streamingMode) {
	case RTP_UDP:
		prtsp->status = 200;
		prtsp->cb_resplen = snprintf(prtsp->cb_respbuf, RTSP_BUFFER_SIZE,
			"RTSP/1.0 200 OK\r\n"
			"Server: %s\r\n"
			"CSeq: %s\r\n"
			"Date: %s\r\n"
			"Transport: RTP/AVP;multicast;destination=%s;source=%s;port=%d-%d;ttl=%d\r\n"
			"Session: %llu;timeout=%d\r\n"
			"\r\n",
			rl_rtsp_server_str,
			prtsp->cseq,
			nkn_get_datestr(NULL),
			destAddrStr, sourceAddrStr, 
			ntohs(serverRTPPort), ntohs(serverRTCPPort), destinationTTL,
			prtsp->fOurSessionId,
			rtsp_player_idle_timeout);
		AO_fetch_and_add1(&glob_rtsp_tot_status_resp_200);
		NS_INCR_STATS(prtsp->nsconf, PROTO_RTSP, client, resp_2xx);
		NS_INCR_STATS(prtsp->nsconf, PROTO_RTSP, client, resp_200);
		break;
	case RTP_TCP:
		// multicast streams can't be sent via TCP
		handleCmd_unsupportedTransport(prtsp);
		break;
	case RAW_UDP:
		prtsp->status = 200;
		prtsp->cb_resplen = snprintf(prtsp->cb_respbuf, RTSP_BUFFER_SIZE,
			"RTSP/1.0 200 OK\r\n"
			"Server: %s\r\n"
			"CSeq: %s\r\n"
			"Date: %s\r\n"
			"Transport: %s;multicast;destination=%s;source=%s;port=%d;ttl=%d\r\n"
			"Session: %llu;timeout=%d\r\n"
			"\r\n",
			rl_rtsp_server_str,
			prtsp->cseq,
			nkn_get_datestr(NULL),
			prtsp->streamingModeString, destAddrStr, sourceAddrStr, 
			ntohs(serverRTPPort), destinationTTL,
			prtsp->fOurSessionId, rtsp_player_idle_timeout);
		AO_fetch_and_add1(&glob_rtsp_tot_status_resp_200);
		NS_INCR_STATS(prtsp->nsconf, PROTO_RTSP, client, resp_2xx);
		NS_INCR_STATS(prtsp->nsconf, PROTO_RTSP, client, resp_200);
		break;
	case RAW_TCP:
		// multicast streams can't be sent via TCP
		handleCmd_unsupportedTransport(prtsp);
		break;
	}
	} else {
	switch (prtsp->streamingMode) {
	case RTP_UDP: 
		prtsp->status = 200;
		prtsp->cb_resplen = snprintf(prtsp->cb_respbuf, RTSP_BUFFER_SIZE,
			"RTSP/1.0 200 OK\r\n"
			"Server: %s\r\n"
			"CSeq: %s\r\n"
			"Date: %s\r\n"
			"Transport: RTP/AVP;unicast;destination=%s;source=%s;client_port=%d-%d;server_port=%d-%d;ssrc=%08x\r\n"
			"Session: %llu;timeout=%d\r\n"
			"\r\n",
			rl_rtsp_server_str,
			prtsp->cseq,
			nkn_get_datestr(NULL),
			destAddrStr, sourceAddrStr, 
			ntohs(clientRTPPort), ntohs(clientRTCPPort), 
			ntohs(serverRTPPort), ntohs(serverRTCPPort),
			ssrc,
			prtsp->fOurSessionId, rtsp_player_idle_timeout);
		AO_fetch_and_add1(&glob_rtsp_tot_status_resp_200);
		NS_INCR_STATS(prtsp->nsconf, PROTO_RTSP, client, resp_2xx);
		NS_INCR_STATS(prtsp->nsconf, PROTO_RTSP, client, resp_200);
		break;
	case RTP_TCP:
		prtsp->status = 200;
		prtsp->cb_resplen = snprintf(prtsp->cb_respbuf, RTSP_BUFFER_SIZE,
			"RTSP/1.0 200 OK\r\n"
			"Server: %s\r\n"
			"CSeq: %s\r\n"
			"Date: %s\r\n"
			"Transport: RTP/AVP/TCP;unicast;destination=%s;source=%s;interleaved=%d-%d;ssrc=%08x\r\n"
			"Session: %llu;timeout=%d\r\n"
			"\r\n",
			rl_rtsp_server_str,
			prtsp->cseq,
			nkn_get_datestr(NULL),
			destAddrStr, sourceAddrStr, prtsp->rtpChannelId, prtsp->rtcpChannelId,
			ssrc,
			prtsp->fOurSessionId, rtsp_player_idle_timeout);
		AO_fetch_and_add1(&glob_rtsp_tot_status_resp_200);
		NS_INCR_STATS(prtsp->nsconf, PROTO_RTSP, client, resp_2xx);
		NS_INCR_STATS(prtsp->nsconf, PROTO_RTSP, client, resp_200);
		break;
	case RAW_UDP:
		prtsp->status = 200;
		prtsp->cb_resplen = snprintf(prtsp->cb_respbuf, RTSP_BUFFER_SIZE,
			"RTSP/1.0 200 OK\r\n"
			"Server: %s\r\n"
			"CSeq: %s\r\n"
			"Date: %s\r\n"
			"Transport: %s;unicast;destination=%s;port=%d;source=%s;client_port=%d\r\n"
			"Session: %llu;timeout=%d\r\n"
			"\r\n",
			rl_rtsp_server_str,
			prtsp->cseq,
			nkn_get_datestr(NULL),
			prtsp->streamingModeString, destAddrStr, ntohs(clientRTPPort), 
			sourceAddrStr, ntohs(serverRTPPort),
			prtsp->fOurSessionId, rtsp_player_idle_timeout);
		AO_fetch_and_add1(&glob_rtsp_tot_status_resp_200);
		NS_INCR_STATS(prtsp->nsconf, PROTO_RTSP, client, resp_2xx);
		NS_INCR_STATS(prtsp->nsconf, PROTO_RTSP, client, resp_200);
		break;
	case RAW_TCP:
		prtsp->status = 200;
		prtsp->cb_resplen = snprintf(prtsp->cb_respbuf, RTSP_BUFFER_SIZE,
			"RTSP/1.0 200 OK\r\n"
			"Server: %s\r\n"
			"CSeq: %s\r\n"
			"Date: %s\r\n"
			"Transport: %s;unicast;interleaved=%d-%d\r\n"
			"Session: %llu;timeout=%d\r\n"
			"\r\n",
			rl_rtsp_server_str,
			prtsp->cseq,
			nkn_get_datestr(NULL),
			prtsp->streamingModeString, prtsp->rtpChannelId, prtsp->rtcpChannelId,
			prtsp->fOurSessionId, rtsp_player_idle_timeout);
		AO_fetch_and_add1(&glob_rtsp_tot_status_resp_200);
		NS_INCR_STATS(prtsp->nsconf, PROTO_RTSP, client, resp_2xx);
		NS_INCR_STATS(prtsp->nsconf, PROTO_RTSP, client, resp_200);
		break;
	}
	}
	free( destAddrStr ); 
	free( sourceAddrStr ); 
	return 0;
}

void handleCmd_withinSession(rtsp_cb_t * prtsp, RTSP_METHOD method) 
{
	void* subsession;
	char *query;

	// This will either be:
	// - a non-aggregated operation, if "urlPreSuffix" is the session (stream)
	//   name and "urlSuffix" is the subsession (track) name, or
	// - a aggregated operation, if "urlSuffix" is the session (stream) name,
	//   or "urlPreSuffix" is the session (stream) name, and "urlSuffix"
	//   is empty.
	// First, figure out which of these it is:

	if (prtsp->fOurrtsp_session == NULL) { 
		// There wasn't a previous SETUP!
		handleCmd_failStateMachine(prtsp);
		return;
	}
	/* Check for query param in URL. If query param is stored with
	 * file name later, this code has to change. For now, ignore
	 * query params in URL.
	 */
	query = strchr(prtsp->urlPreSuffix, '?');
	if ((prtsp->urlSuffix[0] != '\0') &&
	    (strcmp(prtsp->fOurrtsp_session->fStreamName, prtsp->urlPreSuffix) == 0)) {
		// Non-aggregated operation.
		// Look up the media subsession whose track id is "urlSuffix":
		int i;
		for(i=0;i<MAX_SESSION;i++) {
			subsession = prtsp->fOurrtsp_session->fSubsessionsHead[i];
			if(subsession == NULL) break;
			if (strcmp((*prtsp->so->pfunc->p_subs_trackId)(subsession), prtsp->urlSuffix) == 0)
				break;
		}
		if (subsession == NULL) { // no such track!
			handleCmd_notFound(prtsp);
			return;
		}
	} else if (strcmp(prtsp->fOurrtsp_session->fStreamName, prtsp->urlSuffix) == 0 ||
		strcmp(prtsp->fOurrtsp_session->fStreamName, &prtsp->urlPreSuffix[1]) == 0 ||
		(query && strncmp(prtsp->fOurrtsp_session->fStreamName, &prtsp->urlPreSuffix[1], 
		query - prtsp->urlPreSuffix - 1) == 0)) {
		// Aggregated operation
		subsession = NULL;
	} else { // the request doesn't match a known stream and/or track at all!
		handleCmd_notFound(prtsp);
		return;
	}

	prtsp->subsession = subsession;

	if (method == METHOD_TEARDOWN) {
	        handleCmd_TEARDOWN(prtsp);
	} else if (method == METHOD_PLAY) {
		handleCmd_PLAY(prtsp);
	} else if (method == METHOD_PAUSE) {
		handleCmd_PAUSE(prtsp);
	} else if (method == METHOD_GET_PARAMETER) {
		handleCmd_GET_PARAMETER(prtsp);
	}
}

void handleCmd_PLAY(rtsp_cb_t * prtsp) 
{
	int ret = 0;
	char* prtspURL = NULL;
	unsigned rtspURLSize;
	char buf[100];
	char* scaleHeader = NULL;
	char* rangeHeader = NULL;
	char* rtpInfo = NULL;
	float tmp;
	float duration;

	prtspURL = rtspURL(prtsp->fOurrtsp_session, prtsp);
	rtspURLSize = strlen(prtspURL);
	
	// Try to set the stream's scale factor to this value:
	if (prtsp->subsession == NULL) { // aggregate op
		prtsp->scale = sm_testScaleFactor(prtsp, prtsp->fOurrtsp_session, prtsp->scale);
	} else {
		prtsp->scale = (*prtsp->so->pfunc->p_subs_testScaleFactor)(prtsp->subsession, prtsp->scale);
	}
	if ((prtsp->flag & HAS_SCALE_HEAD) == 0) {
		buf[0] = '\0'; // Because we didn't see a Scale: header, don't send one back
	} else {
	    /* setting the scale header to 1; since we dont support trick play in any codec
	     * the interface today is such that the testScaleFactor and setStreamScale cannot
	     * discern between PLAY and TRICKPLAY modes. Till we change that or there is a 
	     * need to support TRICKPLAY mode; we can just leave the response as '1'
	     */
	    snprintf(buf, 100, "Scale: %f\r\n", 1.0 /*prtsp->scale*/);
	}
	scaleHeader = nkn_strdup_type(buf, mod_rtsp_ses_hp_strdup);

	// Use this information, plus the stream's duration (if known), to create
	// our own "Range:" header, for the response:
	if (prtsp->fOurrtsp_session->maxSubsessionDuration != 
		prtsp->fOurrtsp_session->minSubsessionDuration) {
		tmp = -(prtsp->fOurrtsp_session->maxSubsessionDuration);
	} else {
		tmp = prtsp->fOurrtsp_session->maxSubsessionDuration;
	}

	//BZ 3158
	//The following code block has a callback to the mediacodec implemented without locks protecting it,
	//float duration = (prtsp->subsession == NULL) /*aggregate op*/
	//	? tmp
	//	: (*prtsp->so->pfunc->p_subs_duration)(prtsp->subsession);
	//end of BZ 3158

	duration = tmp;
	if (prtsp->subsession != NULL) {
	    duration = (*prtsp->so->pfunc->p_subs_duration)(prtsp->subsession);
	}

	if (duration < 0.0) {
		// We're an aggregate PLAY, but the subsessions have different durations.
		// Use the largest of these durations in our header
		duration = -duration;
	}
	
	/* added case where we cap the rangeEnd to the duration even if the rangeEnd is zero. 
	 * Even if the Range line in the request asks us to play till the end we need to 
	 * respond with the duration of the video sequence, since some players like VLC 
	 * look for this to populate their seekbars and time duration information
	 * Typically VLC range lines are of the form,
	 * Request Line from VLC
	 * ---------------------
	 * Range: 0-\r\n
	 *
	 * Expected Response (this is what other RTSP servers like DARWIN etc respond with)
	 *--------------------------------------------------------------------------------
	 * Range: 0-DURATION\r\n
	 *
	 * BUT, MFD responds with
	 * Range: 0-\r\n 
	 */

	/* If requested range is not within vaild range return error
	 */
	if (((duration > 0) && (prtsp->rangeStart > duration)) || 
	    ((prtsp->rangeEnd > 0) && ((prtsp->rangeEnd >
					(duration+0.1/*account for
				        rounding off errors */)) ||  
				       (prtsp->rangeStart > prtsp->rangeEnd)))) {
		prtsp->status = 457;
		prtsp->cb_resplen = snprintf(prtsp->cb_respbuf, RTSP_BUFFER_SIZE,
			"RTSP/1.0 457 Invalid Range\r\n"
			"Server: %s\r\n"
			"CSeq: %s\r\n"
			"Date: %s\r\n"
			"Session: %llu;timeout=%d\r\n"
			"\r\n",
			rl_rtsp_server_str,
			prtsp->cseq,
			nkn_get_datestr(NULL),
			prtsp->fOurSessionId,
			rtsp_player_idle_timeout);
		free(scaleHeader);
		AO_fetch_and_add1(&glob_rtsp_tot_status_resp_457); 
		NS_INCR_STATS(prtsp->nsconf, PROTO_RTSP, client, resp_4xx);
		return;
	}

	if (prtsp->rangeEnd <= 0.0 || prtsp->rangeEnd > duration) 
		prtsp->rangeEnd = duration;
	if (prtsp->rangeStart < 0.0) {
		prtsp->rangeStart = 0.0;
	} else if (prtsp->rangeEnd > 0.0 && prtsp->scale > 0.0 && prtsp->rangeStart > prtsp->rangeEnd) {
		prtsp->rangeStart = prtsp->rangeEnd;
	}

	  // Cra "RTP-Info:" line.  It will get filled in from each subsession's state:
	char const* rtpInfoFmt =
		"%s" // "RTP-Info:", plus any preceding rtpInfo items
		"%s" // comma separator, if needed
		"url=%s/%s"
		";seq=%d"
#ifdef RTPINFO_INCLUDE_RTPTIME
		";rtptime=%u"
#endif
		;
	unsigned rtpInfoFmtSize = strlen(rtpInfoFmt);
	unsigned i, numRTPInfoItems = 0;
	float playTime = -1;
	float playTime1 = -1;
	rtpInfo = nkn_strdup_type("RTP-Info: ", mod_rtsp_ses_hp_strdup);

	// Do any required seeking/scaling on each subsession, before starting streaming:
	for (i = 0; i < prtsp->fNumStreamStates; ++i) {
                if ((prtsp->fStreamStates[i].subsession) &&
                        ((prtsp->subsession == NULL) ||
                         (prtsp->subsession == prtsp->fStreamStates[i].subsession))) {
			if (prtsp->flag & HAS_RANGE_HEAD) {
				(*prtsp->so->pfunc->p_subs_seekStream)(
					prtsp->fStreamStates[i].subsession, prtsp->fOurSessionId,
					prtsp->fStreamStates[i].streamToken,
					prtsp->rangeStart, prtsp->rangeEnd );
			}
			if (prtsp->flag & HAS_SCALE_HEAD) {
				(*prtsp->so->pfunc->p_subs_setStreamScale)(
					prtsp->fStreamStates[i].subsession, prtsp->fOurSessionId,
					prtsp->fStreamStates[i].streamToken,
					prtsp->scale);
			}

			playTime1 = (*prtsp->so->pfunc->p_subs_getPlayTime)(
				prtsp->fStreamStates[i].subsession, prtsp->fOurSessionId,
				prtsp->fStreamStates[i].streamToken );
			//if (playTime1 > 1) {
			//	printf( "PT=%f s=%d %s\r\n", playTime1, i, prtspURL );
			//}
			if ( playTime1 != -1 ) {
				if (playTime == -1)
					playTime = playTime1;
				else
					playTime = playTime < playTime1 ? playTime : playTime1;
			}
		}
	}

	// Now, start streaming:
	for (i = 0; i < prtsp->fNumStreamStates; ++i) {
		if ((prtsp->fStreamStates[i].subsession) &&
				((prtsp->subsession == NULL) ||
				(prtsp->subsession == prtsp->fStreamStates[i].subsession))) {
			unsigned short rtpSeqNum = 0;
			unsigned rtpTimestamp = 0;
			ret = (*prtsp->so->pfunc->p_subs_startStream)(prtsp->fStreamStates[i].subsession, 
						prtsp->fOurSessionId,
						prtsp->fStreamStates[i].streamToken,
						(TaskFunc*)rtsp_set_timeout,
						(void *)prtsp,
						&rtpSeqNum,
						&rtpTimestamp);
			prtsp->fStreamStates[i].isTimeout = TRUE; // enabling stream timeout
			if (ret < 0) goto error_return;
			
			char const *urlSuffix = (*prtsp->so->pfunc->p_subs_trackId)(prtsp->fStreamStates[i].subsession);
			
			char* prevRTPInfo = rtpInfo;
			unsigned rtpInfoSize = rtpInfoFmtSize
				+ strlen(prevRTPInfo)
				+ 1
				+ rtspURLSize + strlen(urlSuffix)
				+ 5 /*max unsigned short len*/
#ifdef RTPINFO_INCLUDE_RTPTIME
				+ 10 /*max unsigned (32-bit) len*/
#endif
				+ 2 /*allows for trailing \r\n at final end of string*/;
			rtpInfo = (char *)nkn_malloc_type(sizeof(char)*rtpInfoSize, mod_rtsp_ses_hp_malloc);
			snprintf(rtpInfo, sizeof(char)*rtpInfoSize, rtpInfoFmt,
				prevRTPInfo,
				numRTPInfoItems++ == 0 ? "" : ",",
				prtspURL, urlSuffix,
				rtpSeqNum
#ifdef RTPINFO_INCLUDE_RTPTIME
				,rtpTimestamp
#endif
			);
			free(prevRTPInfo);
		}
	}
	if (numRTPInfoItems == 0) {
		rtpInfo[0] = '\0';
	} else {
		unsigned rtpInfoLen = strlen(rtpInfo);
		rtpInfo[rtpInfoLen] = '\r';
		rtpInfo[rtpInfoLen+1] = '\n';
		rtpInfo[rtpInfoLen+2] = '\0';
	}

	if (((prtsp->flag & HAS_RANGE_HEAD) == 0) && (playTime == -1)) {
	//if( (prtsp->flag & HAS_RANGE_HEAD) == 0 ) {
	    buf[0] = '\0'; // Because we didn't see a Range: header, don't send one back
	    if ( (prtsp->flag & HAS_SCALE_HEAD) ) {
		rtpInfo[0] = '\0'; // Only when scale response is sent, we omit the rtpinfo in response; QTSS behaves similarly
	    }
	} else if (prtsp->rangeEnd == 0.0 && prtsp->scale >= 0.0) {
	    snprintf(buf, 100, "Range: npt=%.3f-\r\n", (playTime == -1) ? prtsp->rangeStart : playTime );
	} else {
		snprintf(buf, 100, "Range: npt=%.3f-%.3f\r\n", (playTime == -1) ? prtsp->rangeStart : playTime, prtsp->rangeEnd);
	}
	rangeHeader = nkn_strdup_type(buf, mod_rtsp_ses_hp_strdup);

	// Fill in the response:
	prtsp->status = 200;
	prtsp->cb_resplen = snprintf(prtsp->cb_respbuf, RTSP_BUFFER_SIZE,
		"RTSP/1.0 200 OK\r\n"
		"Server: %s\r\n"
		"CSeq: %s\r\n"
		"Date: %s\r\n"
		"%s"
		"%s"
		"Session: %llu;timeout=%d\r\n"
		"%s"
		"\r\n",
		rl_rtsp_server_str,
		prtsp->cseq,
		nkn_get_datestr(NULL),
		scaleHeader,
		rangeHeader,
		prtsp->fOurSessionId,
		rtsp_player_idle_timeout,
		rtpInfo);

	prtsp->isPlaying = TRUE;
	
	//Clear the range head and rtpinfo flags after the first play request
	//We dont need to send range unless it is present in the request. 
	//Similarly we dont need to send rtpinfo unless range is present in the request
	prtsp->flag &= ~(HAS_RANGE_HEAD);
	prtsp->flag &= ~(HAS_RTPINFO_HEAD);

	if (rtpInfo) free(rtpInfo); 
	if (rangeHeader) free(rangeHeader);
	if (scaleHeader) free(scaleHeader); 
	if (prtspURL) free(prtspURL);
	return;
	
error_return:
	prtsp->status = 500;
	prtsp->cb_resplen = snprintf(prtsp->cb_respbuf, RTSP_BUFFER_SIZE,
		"RTSP/1.0 500 Internal Server Error\r\n"
		"Server: %s\r\n"
		"CSeq: %s\r\n"
		"Date: %s\r\n"
		"Session: %llu;timeout=%d\r\n"
		"\r\n",
		rl_rtsp_server_str,
		prtsp->cseq,
		nkn_get_datestr(NULL),
		prtsp->fOurSessionId,
		rtsp_player_idle_timeout);
	AO_fetch_and_add1(&glob_rtsp_tot_status_resp_500); 
	NS_INCR_STATS(prtsp->nsconf, PROTO_RTSP, client, resp_5xx);
	prtsp->fSessionIsActive = FALSE;

	if (rtpInfo) free(rtpInfo); 
	if (rangeHeader) free(rangeHeader);
	if (scaleHeader) free(scaleHeader); 
	if (prtspURL) free(prtspURL);
	return;
}

void handleCmd_PAUSE(rtsp_cb_t * prtsp) 
{
	unsigned int i;
	for (i = 0; i < prtsp->fNumStreamStates; ++i) {
                if ((prtsp->fStreamStates[i].subsession) &&
                        ((prtsp->subsession == NULL) ||
                         (prtsp->subsession == prtsp->fStreamStates[i].subsession))) {
			(*prtsp->so->pfunc->p_subs_pauseStream)(
				prtsp->fStreamStates[i].subsession,
				prtsp->fOurSessionId,
				prtsp->fStreamStates[i].streamToken);
		}
	}
	prtsp->status = 200;
	prtsp->cb_resplen = snprintf(prtsp->cb_respbuf, RTSP_BUFFER_SIZE,
		"RTSP/1.0 200 OK\r\n"
		"Server: %s\r\n"
		"CSeq: %s\r\n"
		"Date: %s\r\n"
		"Session: %llu;timeout=%d\r\n"
		"\r\n",
		rl_rtsp_server_str,
		prtsp->cseq, nkn_get_datestr(NULL), prtsp->fOurSessionId, rtsp_player_idle_timeout);
}


static char const* allowedCommandNames
	= "OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, GET_PARAMETER";

void handleCmd_bad(rtsp_cb_t * prtsp) {
	// Don't do anything with "cseq", because it might be nonsense
	prtsp->status = 400;
	prtsp->cb_resplen = snprintf(prtsp->cb_respbuf, RTSP_BUFFER_SIZE,
		"RTSP/1.0 400 Bad Request\r\n"
		"Server: %s\r\n"
		"Date: %s\r\n"
		"Allow: %s\r\n"
		"\r\n",
		rl_rtsp_server_str,
		nkn_get_datestr(NULL), allowedCommandNames);
	prtsp->fSessionIsActive = False; // triggers deletion of
					 // ourself after responding
	AO_fetch_and_add1(&glob_rtsp_tot_status_resp_400);
	NS_INCR_STATS(prtsp->nsconf, PROTO_RTSP, client, resp_4xx);
}

void handleCmd_notSupported(rtsp_cb_t * prtsp) {
	prtsp->status = 405;
	prtsp->cb_resplen = snprintf(prtsp->cb_respbuf, RTSP_BUFFER_SIZE,
		"RTSP/1.0 405 Method Not Allowed\r\n"
		"Server: %s\r\n"
		"CSeq: %s\r\n"
		"Date: %s\r\n"
		"Allow: %s\r\n"
		"\r\n",
		rl_rtsp_server_str,
		prtsp->cseq, nkn_get_datestr(NULL), allowedCommandNames);
	prtsp->fSessionIsActive = False; // triggers deletion of
	                                 //ourself after responding
	AO_fetch_and_add1(&glob_rtsp_tot_status_resp_405);
	NS_INCR_STATS(prtsp->nsconf, PROTO_RTSP, client, resp_4xx);

}

void handleCmd_failStateMachine(rtsp_cb_t * prtsp) {
	prtsp->status = 455;
	prtsp->cb_resplen = snprintf(prtsp->cb_respbuf, RTSP_BUFFER_SIZE,
		"RTSP/1.0 455 Method Not Valid in This State\r\n"
		"Server: %s\r\n"
		"CSeq: %s\r\n"
		"Date: %s\r\n"
		"\r\n",
		rl_rtsp_server_str,
		prtsp->cseq, nkn_get_datestr(NULL));
	prtsp->fSessionIsActive = False; // triggers deletion of ourself after responding
	NS_INCR_STATS(prtsp->nsconf, PROTO_RTSP, client, resp_4xx);
}

void handleCmd_notFound(rtsp_cb_t * prtsp) {
	prtsp->status = 404;
	prtsp->cb_resplen = snprintf(prtsp->cb_respbuf, RTSP_BUFFER_SIZE,
		"RTSP/1.0 404 Stream Not Found\r\n"
		"Server: %s\r\n"
		"CSeq: %s\r\n"
		"Date: %s\r\n"
                "Connection: close\r\n"
		"\r\n",
		rl_rtsp_server_str,
		prtsp->cseq, nkn_get_datestr(NULL));
	prtsp->fSessionIsActive = False; // triggers deletion of ourself after responding
	AO_fetch_and_add1(&glob_rtsp_tot_status_resp_404);
	NS_INCR_STATS(prtsp->nsconf, PROTO_RTSP, client, resp_4xx);
	NS_INCR_STATS(prtsp->nsconf, PROTO_RTSP, client, resp_404);
}

void handleCmd_unsupportedTransport(rtsp_cb_t * prtsp) {
	prtsp->status = 461;
	prtsp->cb_resplen = snprintf(prtsp->cb_respbuf, RTSP_BUFFER_SIZE,
		"RTSP/1.0 461 Unsupported Transport\r\n"
		"Server: %s\r\n"
		"CSeq: %s\r\n"
		"Date: %s\r\n"
		"\r\n",
		rl_rtsp_server_str,
		prtsp->cseq, nkn_get_datestr(NULL));
	prtsp->fSessionIsActive = False; // triggers deletion of
	                                 // ourself after responding 
	AO_fetch_and_add1(&glob_rtsp_tot_status_resp_461);
	NS_INCR_STATS(prtsp->nsconf, PROTO_RTSP, client, resp_4xx);
}

void handleCmd_unsupportedMediaCodec(rtsp_cb_t * prtsp) 
{
	prtsp->status = 415;
	prtsp->cb_resplen = snprintf(prtsp->cb_respbuf, RTSP_BUFFER_SIZE,
		"RTSP/1.0 415 Unsupported Media Type\r\n"
		"Server: %s\r\n"
		"CSeq: %s\r\n"
		"Date: %s\r\n"
                "Connection: close\r\n"
		"\r\n",
		rl_rtsp_server_str,
		prtsp->cseq, nkn_get_datestr(NULL));
	prtsp->fSessionIsActive = False; // triggers deletion of
					 // ourself after responding
	AO_fetch_and_add1(&glob_rtsp_tot_status_resp_415);
	NS_INCR_STATS(prtsp->nsconf, PROTO_RTSP, client, resp_4xx);
}

void handleCmd_unsupportedService(rtsp_cb_t * prtsp)
{
	prtsp->status = 503;
	prtsp->cb_resplen = snprintf(prtsp->cb_respbuf, RTSP_BUFFER_SIZE,
		"RTSP/1.0 503 Service Unavailable\r\n"
		"Server: %s\r\n"
		"CSeq: %s\r\n"
		"Date: %s\r\n"
                "Connection: close\r\n"
		"\r\n",
		rl_rtsp_server_str,
		prtsp->cseq, nkn_get_datestr(NULL));
	prtsp->fSessionIsActive = False; // triggers deletion of
					 // ourself after responding
	AO_fetch_and_add1(&glob_rtsp_tot_status_resp_503);
	NS_INCR_STATS(prtsp->nsconf, PROTO_RTSP, client, resp_5xx);
}

void handleCmd_OPTIONS_KA(rtsp_cb_t * prtsp, Boolean sessionId) {
	 // For (OPTIONS used as)Keep Alive, session hdr is included in response
	prtsp->status = 200;
	prtsp->cb_resplen = snprintf(prtsp->cb_respbuf, RTSP_BUFFER_SIZE,
		"RTSP/1.0 200 OK\r\n"
		"Server: %s\r\n"
		"CSeq: %s\r\n"
		"Date: %s\r\n"
		"Public: %s\r\n",
		rl_rtsp_server_str,
		prtsp->cseq, nkn_get_datestr(NULL), allowedCommandNames);
	if (sessionId)
		prtsp->cb_resplen += snprintf(prtsp->cb_respbuf + prtsp->cb_resplen, 
			RTSP_BUFFER_SIZE - prtsp->cb_resplen, 
			"Session: %llu\r\n",
			prtsp->fOurSessionId);
	prtsp->cb_resplen += snprintf(prtsp->cb_respbuf + prtsp->cb_resplen, 
			RTSP_BUFFER_SIZE - prtsp->cb_resplen, "\r\n");
	AO_fetch_and_add1(&glob_rtsp_tot_status_resp_200);
	NS_INCR_STATS(prtsp->nsconf, PROTO_RTSP, client, resp_2xx);
	NS_INCR_STATS(prtsp->nsconf, PROTO_RTSP, client, resp_200);
}

void handleCmd_TEARDOWN(rtsp_cb_t * prtsp) 
{
	prtsp->status = 200;
	prtsp->cb_resplen = snprintf(prtsp->cb_respbuf, RTSP_BUFFER_SIZE,
		"RTSP/1.0 200 OK\r\n"
		"Server: %s\r\n"
		"CSeq: %s\r\n"
		"Date: %s\r\n"
		"Session: %llu;timeout=%d\r\n"
		"\r\n",
		rl_rtsp_server_str,
		prtsp->cseq, nkn_get_datestr(NULL), prtsp->fOurSessionId, rtsp_player_idle_timeout);
	prtsp->fSessionIsActive = False; // triggers deletion of ourself after responding
	glob_tot_video_delivered++;
	AO_fetch_and_add1(&glob_rtsp_tot_status_resp_200);
	NS_INCR_STATS(prtsp->nsconf, PROTO_RTSP, client, resp_2xx);
	NS_INCR_STATS(prtsp->nsconf, PROTO_RTSP, client, resp_200);
}

void handleCmd_GET_PARAMETER(rtsp_cb_t * prtsp) 
{
	// We implement "GET_PARAMETER" just as a 'keep alive',
	// and send back an empty response:
	prtsp->status = 200;
	prtsp->cb_resplen = snprintf(prtsp->cb_respbuf, RTSP_BUFFER_SIZE,
		"RTSP/1.0 200 OK\r\n"
		"Server: %s\r\n"
		"CSeq: %s\r\n"
		"Date: %s\r\n"
		"Session: %llu;timeout=%d\r\n"
		"\r\n",
		rl_rtsp_server_str,
		prtsp->cseq, nkn_get_datestr(NULL), prtsp->fOurSessionId,
		rtsp_player_idle_timeout);
	AO_fetch_and_add1(&glob_rtsp_tot_status_resp_200);
	NS_INCR_STATS(prtsp->nsconf, PROTO_RTSP, client, resp_2xx);
	NS_INCR_STATS(prtsp->nsconf, PROTO_RTSP, client, resp_200);
}

static int rtsp_send_response(rtsp_cb_t *prtsp);
static int rtsp_send_response(rtsp_cb_t *prtsp)
{
	int32_t ret;
	DBG_LOG(MSG, MOD_RTSP, "sending response: <%s>", prtsp->cb_respbuf);
	prtsp->in_bytes = prtsp->rtsp_hdr_len;
	prtsp->out_bytes = prtsp->cb_resplen;
	ret = send(prtsp->fd, prtsp->cb_respbuf, prtsp->cb_resplen, 0);
	glob_rtsp_tot_vod_bytes_sent += ret;
	NS_UPDATE_STATS(prtsp->nsconf, PROTO_RTSP, client, out_bytes, ret);

	return 0;
}

/* ****************************************************************
 * 
 * ****************************************************************/
struct rtsp_cb * nkn_new_prtsp(unsigned long long sessionId,
		int clientSocket, struct sockaddr_in clientAddr,
		struct nkn_interface * pns)
{
	struct rtsp_cb * prtsp;

	prtsp = (struct rtsp_cb *)nkn_malloc_type(sizeof(struct rtsp_cb), mod_rtsp_ses_np_malloc);
	if (!prtsp) 
		return NULL;
	memset((char *)prtsp, 0, sizeof(struct rtsp_cb));
	prtsp->magic = RTSP_MAGIC_LIVE;

	prtsp->so = NULL;//&live_sohandle;
	pthread_mutex_init( &prtsp->mutex, NULL );
	prtsp->fOurSessionId=sessionId;
	prtsp->fd=clientSocket;
	prtsp->mediacodec_error_code = 0;
	//prtsp->fClientAddr=clientAddr;
	prtsp->fIsMulticast=False;
	prtsp->fSessionIsActive=True;
	prtsp->fStreamAfterSETUP=False;
	prtsp->nMediaTracks = 0;
	prtsp->src_ip = clientAddr.sin_addr.s_addr;
	prtsp->dst_ip = tproxy_find_org_dst_ip(clientSocket, pns);

	/* For Streaming log */
	gettimeofday(&(prtsp->start_ts), NULL);
	prtsp->reclaimSuccess = 0;
	prtsp->content_base = NULL;
	prtsp->pns = pns;
	prtsp->fAsyncTaskToken = NULL;
	prtsp->fAsyncTask = 0;
	LIST_INIT(&prtsp->sms_queue_head);

	prtsp->fStreamStates = NULL;
	//nkn_register_epoll_read(prtsp->fd,
	//(BackgroundHandlerProc*)&rtsp_epollin, 
			//(void *)prtsp);
	//rtsp_set_timeout(prtsp);

	pns->tot_sessions++;
	AO_fetch_and_add1(&glob_cur_open_rtsp_socket);

	pthread_mutex_lock(&rtsp_cb_queue_mutex);
	LIST_INSERT_HEAD(&g_rtsp_cb_queue_head, prtsp, entries);
	pthread_mutex_unlock(&rtsp_cb_queue_mutex);

	// RTSP interleaving related init
	init_rtsp_interleave(prtsp);

	/* Register fd after inserting the prtsp in queue
	 * Corner case hit, where this thread was waiting for mutex
	 * to add prtsp into queue, while another thread was trying to
	 * remove it from queue and crashed.
	 */
	register_NM_socket(prtsp->fd,
			   (void *)prtsp,
			   rtsp_epollin,
			   rtsp_epollout,
			   rtsp_epollerr,
			   rtsp_epollhup,
			   NULL,
			   rtsp_timeout,
			   rtsp_player_idle_timeout,
			   USE_LICENSE_FALSE,
			   FALSE);
	NM_add_event_epollin(prtsp->fd);
	return prtsp;
}

void nkn_free_prtsp(struct rtsp_cb * prtsp) {
	struct rtsp_session_t * sms;

	if ( NULL == prtsp ) {
		assert( 0 );
		return;
	}
	if(prtsp->magic != RTSP_MAGIC_DEAD && prtsp->magic != RTSP_MAGIC_LIVE) {
		return;
	}

	//assert (prtsp->magic == RTSP_MAGIC_LIVE);

	// Turn off any liveness checking:
	//nkn_unscheduleDelayedTask(prtsp->fLivenessCheckTask);
	prtsp->fLivenessCheckTask = NULL;

	// Turn off background read handling:
	DBG_LOG(MSG, MOD_RTSP, "close socket fd=%d", prtsp->fd);
	//nkn_unregister_epoll_read(prtsp->fd);
	// NOTE: unregister_NM_socket(fd, TRUE) deletes the fd from epoll loop.
	if(prtsp->magic == RTSP_MAGIC_LIVE){
	    unregister_NM_socket(prtsp->fd, TRUE);
	    nkn_close_fd(prtsp->fd, RTSP_FD);
	}

	int sockfd = prtsp->fd;
	pthread_mutex_unlock(&gnm[sockfd].mutex);
	reclaim_rtsp_session_states(prtsp);
	pthread_mutex_lock(&gnm[sockfd].mutex);

	while( !LIST_EMPTY(&prtsp->sms_queue_head) ) {
		sms = (&prtsp->sms_queue_head)->lh_first;
		if(!prtsp->reclaimSuccess) {
			pthread_mutex_unlock(&gnm[sockfd].mutex);
			delete_rtsp_session_from_rtsp_list(prtsp, sms);
			pthread_mutex_lock(&gnm[sockfd].mutex);
		} else {  
			delete_rtsp_session(prtsp, sms);
		}
	}

	pthread_mutex_lock(&rtsp_cb_queue_mutex);
	if (!LIST_EMPTY(&g_rtsp_cb_queue_head))
		LIST_REMOVE(prtsp, entries);
	pthread_mutex_unlock(&rtsp_cb_queue_mutex);

	prtsp->pns->tot_sessions --;
	AO_fetch_and_sub1(&glob_cur_open_rtsp_socket);

	prtsp->magic = RTSP_MAGIC_FREE;
	if (prtsp->content_base) free(prtsp->content_base);
	//if (prtsp->urlPreSuffix) {
	//free(prtsp->urlPreSuffix);
	//prtsp->urlPreSuffix = NULL;
	//}
	free(prtsp);
}

void nkn_release_prtsp(struct rtsp_cb * prtsp) 
{
	struct rtsp_session_t * sms;

	if ( NULL == prtsp ) {
		assert( 0 );
		return;
	}
	if(prtsp->magic != RTSP_MAGIC_LIVE) {
		return;
	}

	// Turn off any liveness checking:
	//nkn_unscheduleDelayedTask(prtsp->fLivenessCheckTask);
	prtsp->fLivenessCheckTask = NULL;

	// Turn off background read handling:
	//nkn_unregister_epoll_read(prtsp->fd);
	// NOTE: unregister_NM_socket(fd, TRUE) deletes the fd from 
	//epoll loop.
	if(prtsp->magic == RTSP_MAGIC_LIVE) {
	    unregister_NM_socket(prtsp->fd, TRUE);
	}

	reclaim_rtsp_session_states(prtsp);

	while( !LIST_EMPTY(&prtsp->sms_queue_head) ) {
		sms = (&prtsp->sms_queue_head)->lh_first;
		if(!prtsp->reclaimSuccess) {
			delete_rtsp_session_from_rtsp_list(prtsp, sms);
		} else {  
			delete_rtsp_session(prtsp, sms);
		}
	}

	pthread_mutex_lock(&rtsp_cb_queue_mutex);
	if (!LIST_EMPTY(&g_rtsp_cb_queue_head))
		LIST_REMOVE(prtsp, entries);
	pthread_mutex_unlock(&rtsp_cb_queue_mutex);

	prtsp->pns->tot_sessions --;
	AO_fetch_and_sub1(&glob_cur_open_rtsp_socket);

	prtsp->magic = RTSP_MAGIC_FREE;
	if (prtsp->content_base) free(prtsp->content_base);
	//if (prtsp->urlPreSuffix) {
	//free(prtsp->urlPreSuffix);
	//prtsp->urlPreSuffix = NULL;
	//}
	free(prtsp);
}

static void reclaim_rtsp_session_states(rtsp_cb_t * prtsp) {
	unsigned int i;
	nkn_provider_type_t provider;
	uint32_t from_origin_flag, n_streams_setup;

	from_origin_flag = 0;
	n_streams_setup = 0;

	/* find the number of subsessions that have actually been
	 * SETUP 
	 */
	for(i = 0; i < prtsp->fNumStreamStates; i++) {
	    if(prtsp->fStreamStates[i].subsession) {
		n_streams_setup++;
	    }
	}

	if( n_streams_setup && (n_streams_setup ==
				prtsp->fOurrtsp_session->fSubsessionCounter)){ 
	    /* BZ: 5297
	     * all the streams have been setup and hence we can
	     * cleanup all the setup streams
	     */
	    for (i = 0; i < prtsp->fNumStreamStates; ++i) {
		if (prtsp->fStreamStates[i].subsession != NULL) {
		    provider =
			(*prtsp->so->pfunc->p_subs_getLastProviderType)(prtsp->fStreamStates[i].subsession);
		    if(provider == OriginMgr_provider) {
			from_origin_flag = 1;
		    } 			   
		    prtsp->fStreamStates[i].streamToken = 
			(*prtsp->so->pfunc->p_subs_deleteStream)(
								 prtsp->fStreamStates[i].subsession, 
								 prtsp->fOurSessionId,
								 prtsp->fStreamStates[i].streamToken);
		}
		    
		free_RTCPInstance(&prtsp->fStreamStates[i]); 
		prtsp->reclaimSuccess = 1;
	    }
	    if( prtsp->fNumStreamStates && 
		(!from_origin_flag) ) {
		AO_fetch_and_add1(&glob_tot_video_delivered_with_hit);
		NS_INCR_STATS(prtsp->nsconf, PROTO_RTSP, client, cache_hits);
	    }
	    
	    free(prtsp->fStreamStates); 
	    prtsp->fStreamStates = NULL;
	    prtsp->fNumStreamStates = 0;
	} else {
	    /* BZ:5297
	     * there could be a potential leak in freeing the
	     * subsessions, if the connection closes after one SETUP
	     * request. in this case we need to use the
	     * fOurrtsp_session to cleanup sessions this is the
	     * container for all the initialized states and not the
	     * SETUP
	     */
	    prtsp->reclaimSuccess= 0;
	    prtsp->fNumStreamStates = 0;
	    
	}

}

static void rtsp_vod_async_options_handler(void *private_data);
static void rtsp_vod_async_options_handler(void *private_data)
{
	pthread_mutex_lock(&debug_counter_mutex);
	glob_rtsp_OPTIONS_async_call++;
	pthread_mutex_unlock(&debug_counter_mutex);
	
	rtsp_cb_t *prtsp = (rtsp_cb_t*)private_data;
	int sockfd;
	
	if(prtsp->magic != RTSP_MAGIC_DEAD && prtsp->magic != RTSP_MAGIC_LIVE) {
		return;
	}

	sockfd = prtsp->fd;
	pthread_mutex_lock(&gnm[sockfd].mutex);// Needed: To prevent concurrent exec with rtsp_epollin
	if (gnm[sockfd].private_data != prtsp) {
		AO_fetch_and_add1(&glob_rtsp_fd_reassigned_err);
		prtsp->magic = RTSP_MAGIC_DEAD;
		nkn_free_prtsp(prtsp);
		goto gnm_unlock;
	}

	if(prtsp->magic != RTSP_MAGIC_DEAD && prtsp->magic != RTSP_MAGIC_LIVE) {
	    goto gnm_unlock;
	}
	if(prtsp->magic == RTSP_MAGIC_DEAD) {
	    nkn_free_prtsp(prtsp);
	    goto gnm_unlock;
	}

	int has_tunneled;
	/* check for namespace match here if match fails the skip
	 * return error and if namespace does not match
	 */
	{
	    //const namespace_config_t *nsconf = NULL;
	    if (!VALID_NS_TOKEN(prtsp->nstoken)) {
		prtsp->nstoken = rtsp_request_to_namespace(&prtsp->hdr);
	    }
	    prtsp->nsconf = namespace_to_config(prtsp->nstoken);
	    if (prtsp->nsconf == NULL) {
		handleCmd_notFound(prtsp);
		release_namespace_token_t(prtsp->nstoken);
		goto send_response;
	    }

	    if (prtsp->nsconf->http_config) {
		/* check if RTSP delivery is enabled for the namespace */
		if ((prtsp->nsconf->http_config->response_protocol & DP_RTSTREAM) == 0) {
		    handleCmd_unsupportedService(prtsp);
		    release_namespace_token_t(prtsp->nstoken);
		    goto send_response;
		}
	    }
	}

	char *urlPreSuffix;
	RTSP_GET_ALLOC_ABS_URL(&prtsp->hdr, urlPreSuffix);

	//lock released before find_rtsp_session. Reason:Bug5281
	/* Note: Variables of prtsp used within rtsp_epollin { if AsyncTask=1 { ..(A).. } }
	   SHOULD NOT be accessed within find_rtsp_session.
	   Reason: find_rtsp_session and (A) may execute concurrently */
	pthread_mutex_unlock(&gnm[sockfd].mutex); 
	rtsp_session* session = find_rtsp_session(prtsp,
						  &(urlPreSuffix[1]), &has_tunneled);
	pthread_mutex_lock(&gnm[sockfd].mutex);

	if (has_tunneled == TRUE) {
	    goto gnm_unlock;
	}
	if(prtsp->magic == RTSP_MAGIC_FREE) {
	    goto gnm_unlock;
	}
	if(prtsp->magic == RTSP_MAGIC_DEAD) {
	    nkn_free_prtsp(prtsp);
	    goto gnm_unlock;
	}

	
	if (session != NULL) {
		handleCmd_OPTIONS_KA(prtsp, FALSE);
		if (prtsp->state == RSTATE_INIT) {
			prtsp->state = RSTATE_RCV_OPTIONS;
		}
	} else {
	    if (prtsp->mediacodec_error_code == -1) {
		handleCmd_unsupportedMediaCodec(prtsp);
		goto send_response;
	    } else {
		handleCmd_notFound(prtsp);
		goto send_response;
	    }
		
	}

	if(prtsp->state != RSTATE_RELAY) {
send_response:
		DBG_LOG(MSG, MOD_RTSP, "sending response: <%s>", prtsp->cb_respbuf);
		prtsp->in_bytes = prtsp->rtsp_hdr_len;
		prtsp->out_bytes = prtsp->cb_resplen;
		send(prtsp->fd, prtsp->cb_respbuf, prtsp->cb_resplen, 0);
		
		/* We disable this feature for now */
		if (0 && (prtsp->method == METHOD_SETUP) && prtsp->fStreamAfterSETUP ) {
			// The client has asked for streaming to commence now, rather than after a
			// subsequent "PLAY" command.  So, simulate the effect of a "PLAY" command:
			handleCmd_withinSession(prtsp, METHOD_PLAY);
		}
		
		/* write an entry in streaming log */
		gettimeofday(&(prtsp->end_ts), NULL);
		rtsp_streaminglog_write(prtsp);
	
		/* BZ 4122 & request pipeling support requires the parser
		 * state to be set outside the request processing loop; specifically
		 * in the epollin loop, since we need to copy any leftover data and then 
		 * reset the parser. Hence commenting the reset here.
		 */
		// reset request buffer
		//prtsp->cb_reqlen = 0;
#if 0		
		if (!prtsp->fSessionIsActive) {
			if(prtsp->fOurrtsp_session) {
				prtsp->fOurrtsp_session->fDeleteWhenUnreferenced = True;
			}
			nkn_free_prtsp(prtsp);
			/* bug 2627, free of the context needs to be intimated to the caller
			 * with an appropriate error code so as not reuse the context in the caller's
			 * function
			 */
			return;
		}
#endif
	}
	/* BZ: 4645
	 * parser offsets updated within the parser itself; should not be updated here
	 */
	//prtsp->cb_reqlen = prtsp->cb_reqlen - prtsp->cb_reqoffsetlen;
	//prtsp->cb_reqoffsetlen = 0;
	prtsp->fAsyncTask = 0;
	if (prtsp->status != 200) {
		nkn_free_prtsp(prtsp);
		goto gnm_unlock;
	}
gnm_unlock:
	pthread_mutex_unlock(&gnm[sockfd].mutex);
	return;
}

#if 0
static void rtsp_vod_async_teardown_handler(void *private_data);
static void rtsp_vod_async_teardown_handler(void *private_data)
{
	rtsp_cb_t *prtsp = (rtsp_cb_t*)private_data;
	void* subsession;
	char *query;
	pthread_mutex_t tmp_lock;

	RTSP_GET_ALLOC_ABS_URL(&prtsp->hdr, prtsp->urlPreSuffix);

	pthread_mutex_lock(&debug_counter_mutex);
	glob_rtsp_TEARDOWN_async_call++;
	pthread_mutex_unlock(&debug_counter_mutex);

	// This will either be:
	// - a non-aggregated operation, if "urlPreSuffix" is the session (stream)
	//   name and "urlSuffix" is the subsession (track) name, or
	// - a aggregated operation, if "urlSuffix" is the session (stream) name,
	//   or "urlPreSuffix" is the session (stream) name, and "urlSuffix"
	//   is empty.
	// First, figure out which of these it is:

	if (prtsp->fOurrtsp_session == NULL) { 
		// There wasn't a previous SETUP!
		handleCmd_failStateMachine(prtsp);
		return;
	}
	/* Check for query param in URL. If query param is stored with
	 * file name later, this code has to change. For now, ignore
	 * query params in URL.
	 */
	query = strchr(prtsp->urlPreSuffix, '?');

	if (strcmp(prtsp->fOurrtsp_session->fStreamName, prtsp->urlSuffix) == 0 ||
	    strcmp(prtsp->fOurrtsp_session->fStreamName, &prtsp->urlPreSuffix[1]) == 0 ||
	    (query && strncmp(prtsp->fOurrtsp_session->fStreamName, &prtsp->urlPreSuffix[1], 
			      query - prtsp->urlPreSuffix - 1) == 0)) {
		// Aggregated operation
		subsession = NULL;
	} else { // the request doesn't match a known stream and/or track at all!
		handleCmd_notFound(prtsp);
		return;
	}

	prtsp->subsession = subsession;

	pthread_mutex_lock(&prtsp->mutex);
	tmp_lock = prtsp->mutex;
	if(prtsp->magic == RTSP_MAGIC_DEAD) {
	    nkn_free_prtsp_called_with_lock(prtsp);
	    //	    pthread_mutex_unlock(&tmp_lock);
	    return;
	}

	handleCmd_TEARDOWN(prtsp);

	if(prtsp->state != RSTATE_RELAY) {
	        DBG_LOG(MSG, MOD_RTSP, "sending response: <%s>", prtsp->cb_respbuf);
		prtsp->in_bytes = prtsp->rtsp_hdr_len;
		prtsp->out_bytes = prtsp->cb_resplen;
		send(prtsp->fd, prtsp->cb_respbuf, prtsp->cb_resplen, 0);
		
		/* We disable this feature for now */
		if (0 && (prtsp->method == METHOD_SETUP) && prtsp->fStreamAfterSETUP ) {
			// The client has asked for streaming to commence now, rather than after a
			// subsequent "PLAY" command.  So, simulate the effect of a "PLAY" command:
			handleCmd_withinSession(prtsp, METHOD_PLAY);
		}
		
		/* write an entry in streaming log */
		gettimeofday(&(prtsp->end_ts), NULL);
		rtsp_streaminglog_write(prtsp);
	
		/* BZ 4122 & request pipeling support requires the parser
		 * state to be set outside the request processing loop; specifically
		 * in the epollin loop, since we need to copy any leftover data and then 
		 * reset the parser. Hence commenting the reset here.
		 */
		// reset request buffer
		//prtsp->cb_reqlen = 0;
		if (!prtsp->fSessionIsActive) {
			if(prtsp->fOurrtsp_session) {
				prtsp->fOurrtsp_session->fDeleteWhenUnreferenced = True;
			}
			nkn_free_prtsp_called_with_lock(prtsp);
			//			pthread_mutex_unlock(&tmp_lock);
			/* bug 2627, free of the context needs to be intimated to the caller
			 * with an appropriate error code so as not reuse the context in the caller's
			 * function
			 */
			return;
		}
	}
	/* BZ: 4645
	 * parser offsets updated within the parser itself; should not be updated here
	 */
	//prtsp->cb_reqlen = prtsp->cb_reqlen - prtsp->cb_reqoffsetlen;
	//prtsp->cb_reqoffsetlen = 0;
	prtsp->fAsyncTask = 0;
	if (prtsp->status != 200) {
		int sockfd = prtsp->fd;
		pthread_mutex_lock(&gnm[sockfd].mutex);
		nkn_free_prtsp_called_with_lock(prtsp);
		pthread_mutex_unlock(&gnm[sockfd].mutex);
		return;
	}
	pthread_mutex_unlock(&prtsp->mutex);
	return;
}
#endif

static void rtsp_vod_async_close_handler(void *private_data);
static void rtsp_vod_async_close_handler(void *private_data)
{
	rtsp_cb_t *prtsp = (rtsp_cb_t*)private_data;
	int sockfd = prtsp->fd;
	pthread_mutex_lock(&gnm[sockfd].mutex);// Needed: To prevent concurrent exec with rtsp_epollin

	if (gnm[sockfd].private_data != prtsp) {
		AO_fetch_and_add1(&glob_rtsp_fd_reassigned_err);
		prtsp->magic = RTSP_MAGIC_DEAD;
		nkn_free_prtsp(prtsp);
	} else
		nkn_free_prtsp(prtsp);
	pthread_mutex_unlock(&gnm[sockfd].mutex);
	return;
}

	
static int rtsp_process_request(rtsp_cb_t * prtsp);
static int rtsp_process_request(rtsp_cb_t * prtsp)
{
	// Parse the request string into command name and 'CSeq',
	// then handle the command:
	
	//Need to add a check here to see how we are serving the request
	//It could either be through the origin or through the cache.
  
	switch(prtsp->method) {
		default:
		case METHOD_BAD:
			glob_rtsp_err_bad_method++;
			RTSP_GET_ALLOC_ABS_URL(&prtsp->hdr, prtsp->urlPreSuffix);
			handleCmd_bad(prtsp);
			break;
		case METHOD_UNKNOWN:
			RTSP_GET_ALLOC_ABS_URL(&prtsp->hdr, prtsp->urlPreSuffix);
			handleCmd_notSupported(prtsp);
			break;
		case METHOD_OPTIONS:
			RTSP_GET_ALLOC_ABS_URL(&prtsp->hdr, prtsp->urlPreSuffix);
			if (prtsp->urlPreSuffix[0] == '*' ||
			    prtsp->urlPreSuffix[0] == '\0' /*Real Player Support */) {
				// Uri has not been provided
				handleCmd_OPTIONS_KA(prtsp, FALSE);
				if (prtsp->state == RSTATE_INIT) {
					prtsp->state = RSTATE_RCV_OPTIONS;
				}
			}
			else {
				// Checking if it is keepalive OPTIONS
				if (RTSP_IS_FLAG_SET(prtsp->flag, RTSP_HDR_SESSION)) {
					handleCmd_OPTIONS_KA(prtsp, TRUE);
				} else {
					/* 
					 * If file does not exist, find_rtsp_session will
					 * tunnel the RTSP request. If success, we it returns OK.
					 */
					prtsp->fAsyncTask = 1;
					prtsp->fAsyncTaskToken = 
						nkn_rtscheduleDelayedTask(0, rtsp_vod_async_options_handler, (void*)(prtsp));
					if(prtsp->fAsyncTaskToken == (TaskToken)(-1)) {
					    	AO_fetch_and_add1(&glob_async_flag_not_reset_task_failed_count);
					    	prtsp->fAsyncTask = 0;
					}
				}
			}
			break;
		case METHOD_DESCRIBE:
			RTSP_GET_ALLOC_ABS_URL(&prtsp->hdr, prtsp->urlPreSuffix);

			// 
			// Create and post a task to handle the describe command
			//
			prtsp->describe_nkn_task_id = nkn_rttask_create_new_task(
										 TASK_TYPE_RTSP_DESCRIBE_HANDLER,// dest module id
										 TASK_TYPE_RTSP_DESCRIBE_HANDLER, // src module id
										 TASK_ACTION_INPUT, // task action
										 0, // dst module op
										 prtsp, // data
										 sizeof(*prtsp), // data_len
										 0);  // deadline_in_msec
			if (prtsp->describe_nkn_task_id == -1) {
				/* Failed to create a task. 
				 * return and error and close this 
				 * connection.
				 */
				if ( !(prtsp->state == RSTATE_INIT || prtsp->state == RSTATE_RCV_OPTIONS) ) {
					RTSP_SET_ERROR(prtsp, RTSP_STATUS_400, NKN_RTSP_PVE_REQ_CSEQ);
					prtsp->cb_resplen = 0;
					prtsp->status = 400;
					rtsp_build_response(prtsp,
							    prtsp->status_enum, prtsp->sub_error_code);
					AO_fetch_and_add1(&glob_rtsp_tot_status_resp_400);
					NS_INCR_STATS(prtsp->nsconf, PROTO_RTSP, client, resp_4xx);
					
					return RPS_ERROR;
				}
			}
			prtsp->fAsyncTask = 1;
			nkn_rttask_set_state(prtsp->describe_nkn_task_id, TASK_STATE_RUNNABLE);

			break;
		case METHOD_SETUP:
			/* MFD does not support SETUP request before DESCRIBE.
			 * If received return error 455.
			 */
			if ( !(prtsp->state == RSTATE_RCV_DESCRIBE || prtsp->state == RSTATE_RCV_SETUP) ) {
				RTSP_SET_ERROR(prtsp, RTSP_STATUS_455, NKN_RTSP_PVE_METHOD1);
				prtsp->cb_resplen = 0;
				prtsp->status = 455;
				rtsp_build_response(prtsp, prtsp->status_enum, prtsp->sub_error_code);
				rtsp_send_response(prtsp);
				gettimeofday(&(prtsp->end_ts), NULL);
				rtsp_streaminglog_write(prtsp);
				nkn_free_prtsp(prtsp);
				AO_fetch_and_add1(&glob_rtsp_tot_status_resp_455);
				NS_INCR_STATS(prtsp->nsconf, PROTO_RTSP, client, resp_4xx);
				return RPS_ERROR;
			}
			RTSP_GET_ALLOC_URL_COMPONENT(&prtsp->hdr, prtsp->urlPreSuffix, prtsp->urlSuffix);
			handleCmd_SETUP(prtsp);
			prtsp->state = RSTATE_RCV_SETUP;
			break;
		case METHOD_TEARDOWN:
		    RTSP_GET_ALLOC_ABS_URL(&prtsp->hdr, prtsp->urlPreSuffix);
		    handleCmd_withinSession(prtsp, prtsp->method);
		    prtsp->state = RSTATE_RCV_TEARDOWN;
		    //prtsp->fAsyncTask = 1;
		    //prtsp->fAsyncTaskToken = 
		    //nkn_scheduleDelayedTask(0, rtsp_vod_async_teardown_handler, (void*)(prtsp));
		    //if(prtsp->fAsyncTaskToken == (TaskToken)(-1)) {
		    //AO_fetch_and_add1(&glob_async_flag_not_reset_task_failed_count);
		    //prtsp->fAsyncTask = 0;
		    //}
		    break;
		case METHOD_PLAY:
			RTSP_GET_ALLOC_ABS_URL(&prtsp->hdr, prtsp->urlPreSuffix);
			handleCmd_withinSession(prtsp, prtsp->method);
			prtsp->state = RSTATE_RCV_PLAY;
			break;
		case METHOD_PAUSE:
			RTSP_GET_ALLOC_ABS_URL(&prtsp->hdr, prtsp->urlPreSuffix);
			handleCmd_withinSession(prtsp, prtsp->method);
			prtsp->state = RSTATE_RCV_PAUSE;
			break;
		case METHOD_GET_PARAMETER:
			RTSP_GET_ALLOC_ABS_URL(&prtsp->hdr, prtsp->urlPreSuffix);
			handleCmd_withinSession(prtsp, prtsp->method);
			prtsp->state = RSTATE_RCV_GET_PARAMETER;
			break;
	}

	if (prtsp->nkn_tsk_id == 0 && ((prtsp->method != METHOD_DESCRIBE && prtsp->fAsyncTask != 1))) {
		DBG_LOG(MSG, MOD_RTSP, "sending response: <%s>", prtsp->cb_respbuf);
		prtsp->in_bytes = prtsp->rtsp_hdr_len;
		prtsp->out_bytes = prtsp->cb_resplen;
		send(prtsp->fd, prtsp->cb_respbuf, prtsp->cb_resplen, 0);

		/* We disable this feature for now */
		if (0 && (prtsp->method == METHOD_SETUP) && prtsp->fStreamAfterSETUP ) {
			// The client has asked for streaming to commence now, rather than after a
			// subsequent "PLAY" command.  So, simulate the effect of a "PLAY" command:
			handleCmd_withinSession(prtsp, METHOD_PLAY);
		}

		/* write an entry in streaming log */
		gettimeofday(&(prtsp->end_ts), NULL);
		rtsp_streaminglog_write(prtsp);

		/* BZ 4122 & request pipeling support requires the parser
		 * state to be set outside the request processing loop; specifically
		 * in the epollin loop, since we need to copy any leftover data and then 
		 * reset the parser. Hence commenting the reset here.
		 */
		// reset request buffer
		//prtsp->cb_reqlen = 0;

		if (!prtsp->fSessionIsActive) {
			if(prtsp->fOurrtsp_session) {
				prtsp->fOurrtsp_session->fDeleteWhenUnreferenced = True;
			}
			nkn_free_prtsp(prtsp);
			/* bug 2627, free of the context needs to be intimated to the caller
			 * with an appropriate error code so as not reuse the context in the caller's
			 * function
			 */
			return RPS_ERROR;
		}
	}

	return RPS_OK;
}

static int rtsp_epollin(int sockfd, void* instance) 
{
	if ((&gnm[sockfd])->fd == -1 || gnm[sockfd].fd_type != RTSP_FD) // if asyncHandlers closed, then fd will be -1
		return TRUE;

	rtsp_cb_t *prtsp;
	rtsp_parse_status_t parse_status;
	char* ptr;
	int fRequestBufferBytesLeft;
	int bytesRead;
	char tmp_buf[RTSP_REQ_BUFFER_SIZE];

	UNUSED_ARGUMENT(sockfd);
	prtsp = (rtsp_cb_t *) instance;
	if ((prtsp == NULL)) {
		return TRUE;
	}
	if (prtsp->magic != RTSP_MAGIC_LIVE || prtsp->fd != sockfd) {
		return TRUE;
	}

	/* Relay is being setup. Do not process any more requests
	 */
	if (prtsp->fAsyncTask == 1) {

	    prtsp->magic = RTSP_MAGIC_DEAD; 
	    bytesRead = recv(prtsp->fd, (unsigned char *)tmp_buf, 
			     RTSP_REQ_BUFFER_SIZE, 0);
	    unregister_NM_socket(prtsp->fd, TRUE);
	    nkn_close_fd(prtsp->fd, RTSP_FD);
	    AO_fetch_and_add1(&glob_rtsp_cmd_when_task_in_prog);
	    
	    /*pthread_mutex_lock(&prtsp->mutex);
	    ptr = &prtsp->cb_reqbuf[prtsp->cb_reqlen];
	    fRequestBufferBytesLeft = RTSP_REQ_BUFFER_SIZE - prtsp->cb_reqlen;
	    bytesRead = recv(prtsp->fd, (unsigned char *)ptr, 
			fRequestBufferBytesLeft, 0);
	    if(bytesRead) {
		rv = rtsp_parse_request(prtsp);
		
		}	*/	
	    return TRUE;
	}

	//rtsp_set_timeout(prtsp);
	parse_status = RPS_ERROR;
	if ( prtsp->cb_reqoffsetlen > 0 ) {
		memcpy( prtsp->cb_reqbuf, prtsp->cb_reqbuf + prtsp->cb_reqoffsetlen, 
				prtsp->cb_reqlen );
		prtsp->cb_reqoffsetlen = 0;
	}
	ptr = &prtsp->cb_reqbuf[prtsp->cb_reqlen];
	fRequestBufferBytesLeft = RTSP_REQ_BUFFER_SIZE - prtsp->cb_reqlen;
	bytesRead = recv(prtsp->fd, (unsigned char *)ptr, 
			fRequestBufferBytesLeft, 0);
	if (bytesRead <= 0 || bytesRead >= fRequestBufferBytesLeft) {
		// Either the client socket has died, or the request was too big for us.
		// Terminate this connection:
		DBG_LOG(MSG, MOD_RTSP, 
				"rtsp(%d) read %d bytes (of %d); terminating connection!", 
				prtsp->fd, bytesRead, fRequestBufferBytesLeft);

		prtsp->fAsyncTask = 1;
		prtsp->fAsyncTaskToken = 
		    nkn_rtscheduleDelayedTask(0, rtsp_vod_async_close_handler, (void*)(prtsp));
		if(prtsp->fAsyncTaskToken == (TaskToken)(-1)) {
		    AO_fetch_and_add1(&glob_async_flag_not_reset_task_failed_count);
		    prtsp->fAsyncTask = 0;
		}
		//		nkn_free_prtsp(prtsp);
		//delete this;
		return TRUE;
	}
	prtsp->cb_reqlen += bytesRead;
	while (1) {
//////////////////////
		if (process_rtsp_interleave(prtsp) == 1) // To handle RTSP/data(RTP,RTCP) interleaving over TCP
			break;
//////////////////////
		parse_status = rtsp_parse_request(prtsp);
		if (RPS_OK == parse_status){
			/* Do RTSP Vallidation here */
			parse_status = rtsp_validate_protocol(prtsp);
		}

		if (RPS_OK == parse_status){
			/* Do RTSP MFD Limitation Check here */
			parse_status = rtsp_check_mfd_limitation(prtsp);
		}

		if (RPS_OK == parse_status &&
		    (prtsp->pps == NULL || 
		     prtsp->method == METHOD_GET_PARAMETER ||
		     prtsp->method == METHOD_OPTIONS ||
		     prtsp->method == METHOD_UNKNOWN)) {
			parse_status = rtsp_process_request(prtsp);
			/* bug 2627 the error value from rtsp process request needs to be handled
			 * since there is a possibility of free'ing the context prtps in this function
			 * a reusing this context below
			 */
			if(parse_status == RPS_ERROR) {
			        /* BZ: 5327
				 * no need to increment the counter
				 * here
				 */
				//AO_fetch_and_add1(&glob_rtsp_tot_rtsp_parse_error); 
				return TRUE;
			}

		}

		switch (parse_status){
			case RPS_OK:
				/////////////////////
				init_rtsp_interleave(prtsp);
				/////////////////////
				if (prtsp->method == METHOD_DESCRIBE ||
				    prtsp->fAsyncTask == 1) {
					/* NOTE: Pipelined request handling broken here to accomodate
					 * async tasks for 'file in cache' check
					 */
					return TRUE;
				}
				break;
			case RPS_ERROR:
				AO_fetch_and_add1(&glob_rtsp_tot_rtsp_parse_error);
				prtsp->cb_resplen = 0;
				if (RPS_OK == rtsp_build_response(prtsp, prtsp->status_enum, prtsp->sub_error_code)){
					if (prtsp->status_enum == RTSP_STATUS_400)
						AO_fetch_and_add1(&glob_rtsp_tot_status_resp_400);		
					rtsp_send_response(prtsp);
					
					/* write an entry in streaming log */
					gettimeofday(&(prtsp->end_ts), NULL);
					rtsp_streaminglog_write(prtsp);

					//prtsp->urlPreSuffix = NULL;
					nkn_free_prtsp(prtsp);
					return TRUE;
				}
				break;
			case RPS_NEED_MORE_DATA:
				/* Come back after reading more data */
				/* Check if request buffer is full.
				 * BZ 4254
				 */
				if ((prtsp->cb_reqoffsetlen + prtsp->cb_reqlen) >= RTSP_REQ_BUFFER_SIZE) {
					AO_fetch_and_add1(&glob_rtsp_tot_rtsp_parse_error);
					prtsp->cb_resplen = 0;
					RTSP_SET_ERROR(prtsp, RTSP_STATUS_413, NKN_RTSP_PE_REQ_TOO_LARGE);
					if (RPS_OK == rtsp_build_response(prtsp, prtsp->status_enum, prtsp->sub_error_code)){
						rtsp_send_response(prtsp);
						
						/* write an entry in streaming log */
						gettimeofday(&(prtsp->end_ts), NULL);
						rtsp_streaminglog_write(prtsp);

						//prtsp->urlPreSuffix = NULL;				
						nkn_free_prtsp(prtsp);
						return TRUE;
					}
				}
				return TRUE;
			case RPS_NEED_MORE_SPACE:
				AO_fetch_and_add1(&glob_rtsp_tot_rtsp_parse_error);
				prtsp->cb_resplen = 0;
				if (RPS_OK == rtsp_build_response(prtsp, RTSP_STATUS_500, 0)){
					rtsp_send_response(prtsp);
					//prtsp->urlPreSuffix = NULL;
					nkn_free_prtsp(prtsp);
					return TRUE;
				}
				break;
			case RPS_NOT_SUPPORTED:
				AO_fetch_and_add1(&glob_rtsp_tot_rtsp_parse_error);
				prtsp->cb_resplen = 0;
				if (RPS_OK == rtsp_build_response(prtsp, RTSP_STATUS_501, 0)){
					rtsp_send_response(prtsp);
				}
				break;
			default:
				AO_fetch_and_add1(&glob_rtsp_tot_rtsp_parse_error);
				prtsp->cb_resplen = 0;
				if (RPS_OK == rtsp_build_response(prtsp, RTSP_STATUS_400, 0)){
					AO_fetch_and_add1(&glob_rtsp_tot_status_resp_400);
					rtsp_send_response(prtsp);
					//prtsp->urlPreSuffix = NULL;
					nkn_free_prtsp(prtsp);
					return TRUE;
				}
				break;
		} 
#if 0
		if ( prtsp->cb_reqoffsetlen > 0 ) {
			memcpy( prtsp->cb_reqbuf, prtsp->cb_reqbuf + prtsp->cb_reqoffsetlen, 
					prtsp->cb_reqlen - prtsp->cb_reqoffsetlen );
			prtsp->cb_reqlen = prtsp->cb_reqlen - prtsp->cb_reqoffsetlen;
			prtsp->cb_reqoffsetlen = 0;

		}
		else
			break;
#endif
	}
	return TRUE;
}


#if 0 
static int rtsp_epollin(int sockfd, void* private_data) 
{
	rtsp_cb_t *prtsp = (rtsp_cb_t *) private_data;
	rtsp_parse_status_t parse_status;
	struct sockaddr_in dummy; // 'from' address, meaningless in this case
	char* ptr;
	int fRequestBufferBytesLeft;
	int bytesRead;

	/*
	 * Read the request.
	 */
	ptr = &prtsp->cb_reqbuf[prtsp->cb_reqlen];
	fRequestBufferBytesLeft = RTSP_BUFFER_SIZE - prtsp->cb_reqlen;
	bytesRead = nkn_readSocket(sockfd, (unsigned char *)ptr, 
			fRequestBufferBytesLeft, &dummy, NULL);
	if (bytesRead <= 0 || bytesRead >= fRequestBufferBytesLeft) {
		// Either the client socket has died, or the request was too big for us.
		// Terminate this connection:
		DBG_LOG(MSG, MOD_RTSP, 
				"rtsp(%d) read %d bytes (of %d); terminating connection!", 
				prtsp->fd, bytesRead, fRequestBufferBytesLeft);
		nkn_free_prtsp(prtsp);
		return TRUE;
	}
	prtsp->cb_reqlen += bytesRead;

	/*
	 * Parse the request
	 */
	parse_status = rtsp_parse_request(prtsp);
	if (parse_status == RPS_NEED_MORE_DATA) {
		// Not enough data.
		return TRUE;
	} else if(parse_status == RPS_OK) {
		// Does nothing.
	} else {
		// We should return an error code.
		nkn_free_prtsp(prtsp);
		return FALSE;
	}

	/* 
	 * Validate the request
	 */
	if ( (rtsp_validate_protocol(prtsp)) != RPS_OK) {
		goto close_RTSP_socket;
	}

	if ( (rtsp_check_mfd_limitation(prtsp)) != RPS_OK) {
		if ((prtsp->method == METHOD_OPTIONS) ||
		    (prtsp->method == METHOD_DESCRIBE)) {
			RTSP_GET_ALLOC_ABS_URL(&prtsp->hdr, prtsp->urlPreSuffix);
			tunnel_RTSP_request(prtsp);
			return TRUE;
		}
		else { goto close_RTSP_socket; }
	}

	/*
	 * Handle this request.
	 */
	// Parse the request string into command name and 'CSeq',
	// then handle the command:
	//Need to add a check here to see how we are serving the request
	//It could either be through the origin or through the cache.
	switch(prtsp->method) {
	default:
	case METHOD_BAD:
		glob_rtsp_err_bad_method++;
		RTSP_GET_ALLOC_ABS_URL(&prtsp->hdr, prtsp->urlPreSuffix);
		handleCmd_bad(prtsp);
		break;
	case METHOD_UNKNOWN:
		RTSP_GET_ALLOC_ABS_URL(&prtsp->hdr, prtsp->urlPreSuffix);
		handleCmd_notSupported(prtsp);
		break;
	case METHOD_OPTIONS:
		RTSP_GET_ALLOC_ABS_URL(&prtsp->hdr, prtsp->urlPreSuffix);
		if (prtsp->urlPreSuffix[0] == '*') {
			// Uri has not been provided
			handleCmd_OPTIONS(prtsp);
			if (prtsp->state == RSTATE_INIT) {
				prtsp->state = RSTATE_RCV_OPTIONS;
			}
		}
		else {
			/* 
			 * If file does not exist, find_rtsp_session will
			 * tunnel the RTSP request. If success, we it returns OK.
			 */
			int has_tunneled;
			if (find_rtsp_session(prtsp, 
					&(prtsp->urlPreSuffix[1]), 
					&has_tunneled)) {
				handleCmd_OPTIONS(prtsp);
				if (prtsp->state == RSTATE_INIT) {
					prtsp->state = RSTATE_RCV_OPTIONS;
				}
			}
			break;
		}
		break;
	case METHOD_DESCRIBE:
		RTSP_GET_ALLOC_ABS_URL(&prtsp->hdr, prtsp->urlPreSuffix);

		// 
		// Create and post a task to handle the describe command
		//
		prtsp->describe_nkn_task_id = nkn_rttask_create_new_task(
				TASK_TYPE_RTSP_DESCRIBE_HANDLER,	// dest module id
				TASK_TYPE_RTSP_DESCRIBE_HANDLER, 	// src module id
				TASK_ACTION_INPUT, 			// task action
				0, 					// dst module op
				prtsp, 					// data
				sizeof(*prtsp), 			// data_len
				0);  					// deadline_in_msec
		if (prtsp->describe_nkn_task_id == -1) {
			/* Failed to create a task. 
			 * return and error and close this 
			 * connection.
			 */
			if ( !(prtsp->state == RSTATE_INIT || prtsp->state == RSTATE_RCV_OPTIONS) ) {
				RTSP_SET_ERROR(prtsp, RTSP_STATUS_400, NKN_RTSP_PVE_REQ_CSEQ);
				prtsp->cb_resplen = 0;
				prtsp->status = 400;
				rtsp_build_response(prtsp, prtsp->status_enum, prtsp->sub_error_code);
				return RPS_ERROR;
			}
			AO_fetch_and_add1(&glob_async_flag_not_reset_task_failed_count);
			AO_fetch_and_add1(&glob_rtsp_tot_status_resp_400);
			prtsp->fAsyncTask = 0;
		}
		nkn_rttask_set_state(prtsp->describe_nkn_task_id, TASK_STATE_RUNNABLE);

		break;
	case METHOD_SETUP:
		RTSP_GET_ALLOC_URL_COMPONENT(&prtsp->hdr, prtsp->urlPreSuffix, prtsp->urlSuffix);
		handleCmd_SETUP(prtsp);
		prtsp->state = RSTATE_RCV_SETUP;
		break;
	case METHOD_TEARDOWN:
		RTSP_GET_ALLOC_ABS_URL(&prtsp->hdr, prtsp->urlPreSuffix);
		handleCmd_withinSession(prtsp, prtsp->method);
		prtsp->state = RSTATE_RCV_TEARDOWN;
		break;
	case METHOD_PLAY:
		RTSP_GET_ALLOC_ABS_URL(&prtsp->hdr, prtsp->urlPreSuffix);
		handleCmd_withinSession(prtsp, prtsp->method);
		prtsp->state = RSTATE_RCV_PLAY;
		break;
	case METHOD_PAUSE:
		RTSP_GET_ALLOC_ABS_URL(&prtsp->hdr, prtsp->urlPreSuffix);
		handleCmd_withinSession(prtsp, prtsp->method);
		prtsp->state = RSTATE_RCV_PAUSE;
		break;
	case METHOD_GET_PARAMETER:
		RTSP_GET_ALLOC_ABS_URL(&prtsp->hdr, prtsp->urlPreSuffix);
		handleCmd_withinSession(prtsp, prtsp->method);
		prtsp->state = RSTATE_RCV_GET_PARAMETER;
		break;
	}

	if (prtsp->nkn_tsk_id == 0 && (prtsp->method != METHOD_DESCRIBE)) {
		DBG_LOG(MSG, MOD_RTSP, "sending response: <%s>", prtsp->cb_respbuf);
		prtsp->in_bytes = prtsp->rtsp_hdr_len;
		prtsp->out_bytes = prtsp->cb_resplen;
		send(prtsp->fd, prtsp->cb_respbuf, prtsp->cb_resplen, 0);

		/* We disable this feature for now */
		if (0 && (prtsp->method == METHOD_SETUP) && prtsp->fStreamAfterSETUP ) {
			// The client has asked for streaming to commence now, rather than after a
			// subsequent "PLAY" command.  So, simulate the effect of a "PLAY" command:
			handleCmd_withinSession(prtsp, METHOD_PLAY);
		}

		/* write an entry in streaming log */
		gettimeofday(&(prtsp->end_ts), NULL);
		rtsp_streaminglog_write(prtsp);

		/* BZ 4122 & request pipeling support requires the parser
		 * state to be set outside the request processing loop; specifically
		 * in the epollin loop, since we need to copy any leftover data and then 
		 * reset the parser. Hence commenting the reset here.
		 */
		// reset request buffer
		//		prtsp->cb_reqlen = 0;

		if (!prtsp->fSessionIsActive) {
			if(prtsp->fOurrtsp_session) {
				prtsp->fOurrtsp_session->fDeleteWhenUnreferenced = True;
			}
			nkn_free_prtsp(prtsp);
			/* bug 2627, free of the context needs to be intimated to the caller
			 * with an appropriate error code so as not reuse the context in the caller's
			 * function
			 */
			return TRUE;
		}
	}

	prtsp->cb_reqlen = 0;
	prtsp->cb_reqoffsetlen = 0;
	return TRUE;

close_RTSP_socket:
	prtsp->cb_resplen = 0;
	if (RPS_OK == rtsp_build_response(prtsp, prtsp->status_enum, prtsp->sub_error_code)){
		rtsp_send_response(prtsp);
	}

	/* write an entry in streaming log */
	gettimeofday(&(prtsp->end_ts), NULL);
	rtsp_streaminglog_write(prtsp);
				
	nkn_free_prtsp(prtsp);
	return TRUE;
}
#endif

static int rtsp_epollout(int sockfd, void * private_data)
{
	UNUSED_ARGUMENT(private_data);
	DBG_LOG(SEVERE, MOD_RTSP, "epollout should never called. sockid=%d", sockfd);
	//assert(0); // should never called
	return TRUE;
}

static int rtsp_epollerr(int sockfd, void * private_data)
{
	rtsp_cb_t *prtsp;

	UNUSED_ARGUMENT(private_data);
	DBG_LOG(SEVERE, MOD_RTSP, "epollerr should never called. sockid=%d", sockfd);
	//assert(0); // should never called

	prtsp = (rtsp_cb_t *) private_data;
	if ((prtsp == NULL)) {
		return TRUE;
	}

	pthread_mutex_lock(&prtsp->mutex);
	prtsp->magic = RTSP_MAGIC_DEAD; 
	unregister_NM_socket(prtsp->fd, TRUE);
	nkn_close_fd(prtsp->fd, RTSP_FD);
	pthread_mutex_unlock(&prtsp->mutex);

	return TRUE;
}

static int rtsp_epollhup(int sockfd, void * private_data)
{
	UNUSED_ARGUMENT(private_data);
	DBG_LOG(SEVERE, MOD_RTSP, "epollhup should never called. sockid=%d", sockfd);
	//assert(0); // should never called
	return TRUE;
}


void rtsp_set_timeout(struct rtsp_cb * prtsp) 
{
	if (rtsp_player_idle_timeout > 0) {
		nkn_unscheduleDelayedTask(prtsp->fLivenessCheckTask);
		prtsp->fLivenessCheckTask = nkn_scheduleDelayedTask(
			rtsp_player_idle_timeout * 1000000,
			(TaskFunc*)livenessTimeoutTask, (void *)prtsp);
	}
}

static int rtsp_timeout(int sockfd, void * private_data, double timeout)
{
	struct streamState * pss;
	struct rtsp_cb *prtsp;
	unsigned int streamNum = 0;
	int rv;

	rv  = TRUE;
	UNUSED_ARGUMENT(sockfd);
	UNUSED_ARGUMENT(timeout);
	prtsp = (struct rtsp_cb*)(private_data);

	if(!prtsp || gnm[sockfd].private_data != prtsp) { 
		return FALSE;
	}

	/* At SETUP timeout, prtsp has been created but the stream timeout has not beeen enabled.
		So, stream state is checked only if SETUP has been crossed */
	if (prtsp->method != METHOD_SETUP) {
		for((streamNum = 0);streamNum < prtsp->fNumStreamStates; ++streamNum) {
			pss = &prtsp->fStreamStates[streamNum];
			if(pss->isTimeout == FALSE) {
				rv = FALSE;
			}
			pss->isTimeout = TRUE;
		}
	}
	if(rv == TRUE) {
		livenessTimeoutTask((struct rtsp_cb*)(prtsp));
	}

	return FALSE;
}

void livenessTimeoutTask(struct rtsp_cb * prtsp) {
	// If this gets called, the client session is assumed to have timed out,
	// so delete it:

	// However, we don't timeout multicast sessions, because to do so would require
	// closing all client sessions that have requested the stream - not just this one.
	// Also, the multicast stream itself would usually not be halted, in any case.
	if (prtsp->fAsyncTask == 1) {
		prtsp->magic = RTSP_MAGIC_DEAD;
		unregister_NM_socket(prtsp->fd, TRUE);
		nkn_close_fd(prtsp->fd, RTSP_FD);
		return;
	}
	if (isMulticast(prtsp)) return;

	DBG_LOG(MSG, MOD_RTSP, 
		"RTSP client session from 0x%x has timed out (due to inactivity)", 
		prtsp->src_ip);
	nkn_free_prtsp(prtsp);
	return;
}

Boolean isMulticast(rtsp_cb_t * prtsp) { 
	return prtsp->fIsMulticast; 
}

/* Fire off an RTSP OM Task, Note this task will only stat for a file
 */
#if 0 
static void rtsp_om_post_sched_a_task(rtsp_om_task_t *pfs, 
				      off_t offset_requested, 
				      off_t length_requested)
{
	cache_response_t *cr;
	uint32_t deadline_in_msec;
	char tmpuri[256];
	int len;

	/* For now do nothing
	 */
	return;
	// build up the cache response structure
	cr = nkn_calloc_type(1, sizeof(*cr),
			     mod_vfs_cache_response_t);

	if(!cr) return;
	cr->magic = CACHE_RESP_REQ_MAGIC;
	len = strlen(pfs->filename)+2;
	snprintf(tmpuri, sizeof(tmpuri), "/%s", pfs->filename);
	//cr->uol.uri = nstoken_to_uol_uri(pfs->ns_token, tmpuri, strlen(tmpuri), NULL, 0);
	cr->uol.cod = nkn_cod_open(tmpuri, NKN_COD_NW_MGR);
	if (cr->uol.cod == NKN_COD_NULL) {
		DBG_LOG(MSG, MOD_RTSP, "nstoken_to_uol_uri(token=0x%lx) failed",
			pfs->ns_token.u.token);
		DBG_FUSE("%s nstoken_to_uol_uri(token=0x%lx) failed", __FUNCTION__, pfs->ns_token.u.token);
		return;
	}
	cr->uol.offset = offset_requested;
	cr->uol.length = length_requested;
	cr->in_client_data.proto = NKN_PROTO_RTSP;
	cr->in_proto_data = pfs->con;

	if(offset_requested * length_requested == 0) {
		cr->in_rflags = CR_RFLAG_GETATTR | CR_RFLAG_FIRST_REQUEST;
		//cr->in_rflags |= CR_RFLAG_NO_DATA;
	}

	/* for the moment disable the CR_RFLAG_GEATTR; only for testing the return path */
	//	cr->in_rflags &= ~CR_RFLAG_GETATTR;

	cr->ns_token = pfs->ns_token;
	pfs->cptr = cr;
	deadline_in_msec = 0;

	// Create a new task
	pfs->nkn_taskid = nkn_task_create_new_task(
					TASK_TYPE_CHUNK_MANAGER,
					TASK_TYPE_RTSP_SVR,
					TASK_ACTION_INPUT,
					0, /* NOTE: Put callee operation if any*/
					cr,
					sizeof(cache_response_t),
					deadline_in_msec);
	assert(pfs->nkn_taskid != -1);
	pfs->con->nkn_tsk_id = pfs->nkn_taskid;

	nkn_task_set_private(TASK_TYPE_RTSP_SVR, pfs->nkn_taskid, (void *)pfs);
	nkn_task_set_state(pfs->nkn_taskid, TASK_STATE_RUNNABLE);

	return;
}
#endif

static void rtsp_mgr_cleanup(nkn_task_id_t id) 
{
	UNUSED_ARGUMENT(id);
        return ;
}

static void rtsp_mgr_input(nkn_task_id_t id) 
{
	UNUSED_ARGUMENT(id);

        return ;
}


static void rtsp_mgr_output(nkn_task_id_t id)
{
	cache_response_t *cr;
	rtsp_om_task_t *pfs;

	cr = NULL;
	pfs = NULL;

	cr = nkn_task_get_data(id);
	pfs = nkn_task_get_private(TASK_TYPE_RTSP_SVR, id);

	nkn_task_set_action_and_state(id, TASK_ACTION_CLEANUP, TASK_STATE_RUNNABLE);


}


static void rtsp_describe_handler_cleanup(nkn_task_id_t id) 
{
	UNUSED_ARGUMENT(id);
        return ;
}

static void rtsp_describe_handler_input(nkn_task_id_t id) 
{
	/* For RTSP methods that are handled using Async Task Handlers, 
	prtsp should be freed only within the AsyncTaskHandlers. Need for 
	cleaning prtsp can be indicated by others(rtsp_epollin) using 
	prtsp->magic = RTSP_MAGIC_DEAD */
	rtsp_cb_t *prtsp = (rtsp_cb_t *) nkn_rttask_get_data(id);
	int sockfd;
	int ret;

	if(prtsp->magic != RTSP_MAGIC_DEAD && prtsp->magic != RTSP_MAGIC_LIVE) {
		return;
	}

	sockfd = prtsp->fd;
	pthread_mutex_lock(&gnm[sockfd].mutex);// Needed: To prevent concurrent exec with rtsp_epollin
	if (gnm[sockfd].private_data != prtsp) {
		AO_fetch_and_add1(&glob_rtsp_fd_reassigned_err);
		prtsp->magic = RTSP_MAGIC_DEAD;
		nkn_free_prtsp(prtsp);
		goto gnm_unlock;
	}

	if(prtsp->magic != RTSP_MAGIC_DEAD && prtsp->magic != RTSP_MAGIC_LIVE) {
	    goto gnm_unlock;
	}
	if(prtsp->magic == RTSP_MAGIC_DEAD) {
	    nkn_free_prtsp(prtsp);
	    goto gnm_unlock;
	}

	pthread_mutex_unlock(&gnm[sockfd].mutex);

	ret = handleCmd_DESCRIBE(prtsp);

	nkn_rttask_set_action(id, TASK_ACTION_CLEANUP);
	nkn_rttask_set_state(id, TASK_STATE_RUNNABLE);

	pthread_mutex_lock(&gnm[sockfd].mutex);
	if (ret == -1) {
	    AO_fetch_and_add1(&glob_async_flag_not_reset_desc_failed_count);
	    goto gnm_unlock;
	}
	if(prtsp->magic == RTSP_MAGIC_FREE) {
	    goto gnm_unlock;
	}
	if(prtsp->magic == RTSP_MAGIC_DEAD) {
	    nkn_free_prtsp(prtsp);
	    goto gnm_unlock;
	}

	if (prtsp->state == RSTATE_INIT || prtsp->state == RSTATE_RCV_OPTIONS)
		prtsp->state = RSTATE_RCV_DESCRIBE;

	if (prtsp->nkn_tsk_id == 0 &&
	    prtsp->state != RSTATE_RELAY) {
		DBG_LOG(MSG, MOD_RTSP, "sending response: <%s>", prtsp->cb_respbuf);
		prtsp->in_bytes = prtsp->rtsp_hdr_len;
		prtsp->out_bytes = prtsp->cb_resplen;
		send(prtsp->fd, prtsp->cb_respbuf, prtsp->cb_resplen, 0);

		/* We disable this feature for now */
		if (0 && (prtsp->method == METHOD_SETUP) && prtsp->fStreamAfterSETUP ) {
			// The client has asked for streaming to commence now, rather than after a
			// subsequent "PLAY" command.  So, simulate the effect of a "PLAY" command:
			handleCmd_withinSession(prtsp, METHOD_PLAY);
		}

		/* write an entry in streaming log */
		gettimeofday(&(prtsp->end_ts), NULL);
		rtsp_streaminglog_write(prtsp);

		/* BZ 4122 & request pipeling support requires the parser
		 * state to be set outside the request processing loop; specifically
		 * in the epollin loop, since we need to copy any leftover data and then 
		 * reset the parser. Hence commenting the reset here.
		 */
		// reset request buffer
		//		prtsp->cb_reqlen = 0;
#if 0
		if (!prtsp->fSessionIsActive) {
			if(prtsp->fOurrtsp_session) {
				prtsp->fOurrtsp_session->fDeleteWhenUnreferenced = True;
			}
			//	pthread_mutex_unlock(&prtsp->mutex);
			nkn_free_prtsp(prtsp);
			/* bug 2627, free of the context needs to be intimated to the caller
			 * with an appropriate error code so as not reuse the context in the caller's
			 * function
			 */
			return ; //RPS_ERROR;
		}
#endif
	}
	/* BZ: 4645
	 * parser offsets updated within the parser itself; should not be updated here
	 */
	//prtsp->cb_reqlen = prtsp->cb_reqlen - prtsp->cb_reqoffsetlen;
	//prtsp->cb_reqoffsetlen = 0;
	prtsp->fAsyncTask = 0;

	if (prtsp->status != 200) {
		nkn_free_prtsp(prtsp);
		goto gnm_unlock;
	}
gnm_unlock:
	pthread_mutex_unlock(&gnm[sockfd].mutex);
        return ;
}


static void rtsp_describe_handler_output(nkn_task_id_t id)
{
	UNUSED_ARGUMENT(id);
        return ;
}

void init_rtsp_interleave(rtsp_cb_t* prtsp) 
{
	// RTSP interleaving related init
	prtsp->rtsp_intl.to_read = 0;
	prtsp->rtsp_intl.interleave_state = INTERLEAVE_HDR;
}

static int process_rtsp_interleave(rtsp_cb_t* prtsp) { // 1 - break , 0 - continue parsing

	int8_t need_read = -1;
	while ((prtsp->rtsp_intl.interleave_state != SIGNALLING) && 
		(need_read == -1) && (prtsp->cb_reqlen > 0)) {
	   switch (prtsp->rtsp_intl.interleave_state) {
		case INTERLEAVE_HDR: // initial state
			if (prtsp->cb_reqbuf[prtsp->cb_reqoffsetlen] == '$') {
				if (prtsp->cb_reqlen >= 4) {
					memcpy(&(prtsp->rtsp_intl.to_read), 
						prtsp->cb_reqbuf+prtsp->cb_reqoffsetlen+2, 2);
					prtsp->rtsp_intl.to_read = ntohs(prtsp->rtsp_intl.to_read);
					prtsp->rtsp_intl.interleave_state = DATA;
					prtsp->cb_reqoffsetlen += 4;
					prtsp->cb_reqlen -= 4;
				} else
					need_read = 1;
			} else 
				prtsp->rtsp_intl.interleave_state = SIGNALLING;
			break;
		case DATA:
			// currently skipping the interleaved data on the RTSP channel
			if (prtsp->cb_reqlen >= prtsp->rtsp_intl.to_read) {
				prtsp->cb_reqoffsetlen += prtsp->rtsp_intl.to_read;
				prtsp->cb_reqlen -= prtsp->rtsp_intl.to_read;
				init_rtsp_interleave(prtsp);
			} else { 
				prtsp->rtsp_intl.to_read -= prtsp->cb_reqlen;
				prtsp->cb_reqoffsetlen += prtsp->cb_reqlen;
				prtsp->cb_reqlen = 0;
			}
			break;
		case SIGNALLING:
			// nothing to be done
			break;
		default:
			// should not be here
			return FALSE;
	   }
	}

	if ((prtsp->rtsp_intl.interleave_state != SIGNALLING) || (need_read ==1))
		return 1;
	else
		return 0;
}

void nkn_updateRTSPCounters(uint64_t bytesSent) {
        glob_rtsp_tot_vod_bytes_sent += bytesSent;
}

