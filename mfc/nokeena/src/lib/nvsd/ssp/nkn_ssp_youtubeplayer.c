/*
 * Youtube Server Side Player
 *
 */

#include <string.h>
#include <stdlib.h>
#include <openssl/md5.h>
#include <alloca.h>

#include "ssp.h"
#include "ssp_authentication.h"
#include "ssp_queryparams.h"
#include "http_header.h"
#include "nkn_errno.h"
#include "nkn_debug.h"
#include "nkn_common_config.h"
#include "nkn_stat.h"
#include "nkn_ssp_players.h"
#include "nkn_ssp_player_prop.h"

#include "nkn_vpe_media_props_api.h"
#include "nkn_vpe_types.h"
#include "nkn_vpemgr_defs.h"
#include "nkn_cfg_params.h"


extern unsigned long long glob_ssp_hash_failures;
extern unsigned long long glob_ssp_container_flv, glob_ssp_container_mp4,
    glob_ssp_container_3gpp, glob_ssp_container_webm;

#define YT_SSP_FMT_NUM 6 /* we support 6 fmt itag now */
#define TSTR(_p, _l, _r)  char *(_r); ( 1 ? ( (_r) = alloca((_l)+1), memcpy((_r), (_p), (_l)), (_r)[(_l)] = '\0' ) : 0 )

static int
youtube_ssp_transcode_state(con_t *con,
                            const char *hostData, int hostLen,
                            const ssp_config_t *ssp_cfg,
                            const namespace_config_t *cfg,
                            off_t contentLen, int transType, uint64_t* threshold,
                            char transRatioData[][4], int* transRatioLen,
                            char *newUri, int newUriLen, int idLen, int fmtLen,
                            const char *queryId, const char *queryFmt);

static int
s_remap_up_down_url(con_t *con, const char *queryId, int idLen,
                    int curFmt_state,
                    int up_down, int ssp_container_id);

static int
s_youtube_up_down_grade(con_t* con, /* const char *uriData, int uriLen,*/
			const ssp_config_t *ssp_cfg,
			off_t contentLen, const char *dataBuf, int bufLen,
			int* up_down_flag, int* limiting_bitrate,
			char *newUri,
			const char *queryFmt,
			const char *queryId, int idLen);

static int
s_set_youtube_up_down_grade(const ssp_config_t *ssp_cfg,
                            int *up_down_type,
                            int *up_down_flag,
                            int *limiting_bitrate);


/////////////////////////////////////////////////////////
// Youtube.com Server Side Player (Youtube SSP Type 5) //
// including the transcoder
/////////////////////////////////////////////////////////
int
youtubeSSP(con_t *con, const char *uriData, int uriLen, const char *hostData, int hostLen, const ssp_config_t *ssp_cfg, const namespace_config_t *cfg, off_t contentLen, const char *dataBuf, int bufLen)
{
    const char *queryIdTmp=NULL, *queryFmtTmp=NULL, *querySeekTmp=NULL;
    char* queryFmt_alloca = NULL;
    int idLen=0, fmtLen=0, seekLen=0;
    int remapLen=0;
    int rv=0, queryrv=0;

    char *newUri;
    int newUriLen = 0;
    char uriPrefix[] = "/yt_video_id_";

    http_cb_t *phttp = &con->http;

    /* ssp transocde parameters */
    char transRatioData[MAX_SSP_BW_FILE_TYPES][4];
    int transRatioLen[MAX_SSP_BW_FILE_TYPES];
    uint64_t threshold[MAX_SSP_BW_FILE_TYPES];
    /* use bit operation to decide what kind of video file will be transcode */
    int transType = 0x0; /* FLV 0x0001 MP4 0x0002 */

    /*ssp up/downgrade parameters */
    int up_down_type = 0x0; /* FLV 0x0001 MP4 0x0002 */
    int up_down_flag[MAX_SSP_BW_FILE_TYPES]; /* 0: downgrade 1: upgrade*/
    int limiting_bitrate[MAX_SSP_BW_FILE_TYPES]; /*Kbps*/

    /*
     *  FLV itag bitrate(Mbps)  MP4 itag bitrate(Mbps) WebM itag bitrate(Mbps)
     *        35 480P 1.25          38 3072P 5.00           46 1080P ?
     *        34 360P 0.625         37 1080P 4.00           45  720P 2.00
     *         6 270P               22  720P 3.00           44  480P 1.00
     *         5 240P 0.25          18  360P 0.65           43  360P 0.65
     */

    /*
     * FLV Downgrade limiting_bitrate 500kbps,  35 will downgrade to 34
     * MP4 Upgade    limiting_bitrate 3200kbps, 18 will upgrade to 22,
     *                                          22 will stay the same
     * MP4 Downgrade limiting_bitrate 1000kbps, 37 will downgrade to 22,
     *                                          22 & 18 will stay the same
     */

    // Obtain the relative URL length stripped of all query params
    if ((remapLen = findStr(uriData, "?", uriLen)) == -1 ) {
	remapLen = uriLen;
	DBG_LOG(WARNING, MOD_SSP, "URL does not contain any query params");
    }

    /* Perform a URI rewrite to map the requested object via the query param to
       our internal object naming policy for youtube's videos
       Original URI is present in MIME_HDR_X_NKN_URI
       Remapped URI using id is present in MIME_HDR_X_NKN_REMAPPED_URI
       The new object will be named using the video id w/o any file extensions
    */
    if (ssp_cfg->video_id.enable &&
	ssp_cfg->video_id.video_id_uri_query &&
	ssp_cfg->video_id.format_tag) {
	get_query_arg_by_name(&phttp->hdr, ssp_cfg->video_id.video_id_uri_query, strlen(ssp_cfg->video_id.video_id_uri_query), &queryIdTmp, &idLen);
	get_query_arg_by_name(&phttp->hdr, ssp_cfg->video_id.format_tag, strlen(ssp_cfg->video_id.format_tag), &queryFmtTmp, &fmtLen);
    }
    else {
	DBG_LOG(WARNING, MOD_SSP, "Type 5 player, cache name configuration is disabled in CLI.\
                                  Without a video id and format tag, we cannot cache or process the requests.\
                                  Tunnel connection (Error: %d)", NKN_SSP_FORCE_TUNNEL_PATH);

	if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
	    DBG_TRACE("Type 5 player, cache name configuration is disabled in CLI for virtual player: %s" , ssp_cfg->player_name);
	}

	rv = -NKN_SSP_FORCE_TUNNEL_PATH;  //Tunnel connection
	goto exit;
    }
    TSTR(queryIdTmp, idLen, queryId);
    TSTR(queryFmtTmp, fmtLen, queryFmt);
    if ( strncmp(queryId, "", 1) == 0 ) {
	queryId = NULL;
    }
    if ( strncmp(queryFmt, "", 1) == 0 ) {
	queryFmt = NULL;
    }

    if (queryId == NULL) {
	DBG_LOG(WARNING, MOD_SSP, "Video ID missing in request. Just tunnel the request through");
	rv = -NKN_SSP_FORCE_TUNNEL_PATH;;
	goto exit;
    }

    if (queryFmt == NULL) {
	DBG_LOG(WARNING, MOD_SSP, "Video TAG/FMT missing in request. Just tunnel the request through");
	rv = -NKN_SSP_FORCE_TUNNEL_PATH;;
	goto exit;
    }

    /* strndupa is only available if GNU CC is used */
    queryFmt_alloca = (char*) alloca(fmtLen + 1);
    if (queryFmt_alloca == NULL) {
	DBG_LOG(WARNING, MOD_SSP, "Fail to copy Video TAG/FMT to local string. Just tunnel the request through");
	rv = -NKN_SSP_FORCE_TUNNEL_PATH;;
	goto exit;
    }
    queryFmt_alloca[fmtLen] = '\0';
    memcpy(queryFmt_alloca, queryFmt, fmtLen);

    // Generate internal cache name using ID & TAG/FMT
    newUriLen = strlen(uriPrefix) + idLen + strlen("_fmt_") + fmtLen + 1;
    newUri = (char *)alloca(newUriLen);
    memset(newUri, 0, newUriLen);
    memcpy(newUri, uriPrefix, strlen(uriPrefix));
    strncat(newUri,queryId, idLen);
    strcat(newUri,"_fmt_");
    strncat(newUri, queryFmt, fmtLen);
    newUri[newUriLen -1] = '\0';

    /*
     * Obtain the container format type from querystring
     * Up/Downgrade can only be carried for same format
     */
    /* atoi can only work correctly for string with NULL terminator */
    switch (atoi(queryFmt_alloca)) {

	case 18: // MP4 container (480x270) - Medium quality
        case 22: // MP4 container (1280x720) - 720p quality
        case 37: // MP4 container (1920x1080) - 1080p quality
	case 38: // MP4 container               3072p quality
	    if (con->ssp.ssp_container_id == 0) {
		glob_ssp_container_mp4++;
	    }
            con->ssp.ssp_container_id = CONTAINER_TYPE_MP4;
            break;
	case 36: // 3GPP container 320x240
            if (con->ssp.ssp_container_id == 0) {
                glob_ssp_container_3gpp++;
            }
	    con->ssp.ssp_container_id = CONTAINER_TYPE_3GPP;
	    break;
	case 5: // FLV container (320x240) - Looks like this is streaming http
	case 6: // FLV container  270p
	case 34: // FLV container (320x240 to 640x480) - Normal quality
	case 35: // FLV container (854x480) - High quality 480p
            if (con->ssp.ssp_container_id == 0) {
                glob_ssp_container_flv++;
            }
	    con->ssp.ssp_container_id = CONTAINER_TYPE_FLV;
	    break;

	case 43: // WebM 360p
        case 44: // WebM 480p
        case 45: // WebM 720p
	case 46: // WebM 1080p
            if (con->ssp.ssp_container_id == 0) {
                glob_ssp_container_webm++;
            }
            con->ssp.ssp_container_id = CONTAINER_TYPE_WEBM;
            break;

	default:
	    DBG_LOG(WARNING, MOD_SSP, "unknown Video Tag. Just tunnel the request through");
	    rv = -NKN_SSP_FORCE_TUNNEL_PATH;;
	    goto exit;
	    break;
    } // switch (atio(queryFmt))

    if (ssp_cfg->bandwidth_opt.enable == 1) {
        /* config ssp transocde parameters from CLI */
        ssp_set_transcode(ssp_cfg, &transType, threshold,
                          transRatioData, transRatioLen);

	/*config youtube up_down_grade parameters*/
	s_set_youtube_up_down_grade(ssp_cfg, &up_down_type,
                                    up_down_flag, limiting_bitrate);
	/*
	 * Youtube up/downgrade and transcode will not co-exist
	 * when both are configed, transcode will be depreciated.
	 */
	transType = transType & (~ up_down_type);
    }

    if (ssp_cfg->bandwidth_opt.enable == 1 && /* feature on */
	con->ssp.ssp_container_id <= 2 && /* only for FLV and MP4 type*/
	(up_down_type & con->ssp.ssp_container_id) &&  /* file type */
	(up_down_flag[con->ssp.ssp_container_id - 1] == 1 || /* upgrade or */
	 up_down_flag[con->ssp.ssp_container_id - 1] == 0)) { /* downgrade*/
	if (atoi(queryFmt_alloca) == 18 || atoi(queryFmt_alloca) == 22 ||
	    atoi(queryFmt_alloca) == 37 || /* MP4 */
	    atoi(queryFmt_alloca) ==  5 || atoi(queryFmt_alloca) == 34 ||
	    atoi(queryFmt_alloca) ==35) { /* FLV */
	    /* up_down_grade main loop */
	    rv = s_youtube_up_down_grade(con, ssp_cfg, contentLen,
					 dataBuf, bufLen,
					 up_down_flag,
					 limiting_bitrate,
					 newUri, queryFmt, queryId, idLen);
	    if ( rv < 0 || rv == SSP_WAIT_FOR_DATA || rv == SSP_SEND_DATA_BACK) {
		goto exit;
	    }
	} else { // fmt 38 and fmt 6 is not supported for up/down grade
	    /* Map original internal cache name to REMAPPED_URI */
	    add_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI,
			     newUri, strlen(newUri));
	    DBG_LOG(MSG, MOD_SSP, "Cache name : %s", newUri );
 	}
    } else if (ssp_cfg->bandwidth_opt.enable == 1 &&
	       (transType & con->ssp.ssp_container_id)) { /* file type */
	/* transcoding feature is turned on */
	rv = youtube_ssp_transcode_state(con, hostData,
                                         hostLen, ssp_cfg, cfg, contentLen,
                                         transType, threshold, transRatioData,
                                         transRatioLen, newUri, newUriLen,
                                         idLen, fmtLen, queryId, queryFmt);
        if ( rv < 0 || rv == SSP_WAIT_FOR_DATA || rv == SSP_SEND_DATA_BACK) {
            goto exit;
	}
    } else { /* no transcode, no up/downgrade */
	/* Map original internal cache name to REMAPPED_URI */
	add_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI,
                         newUri, strlen(newUri));
	DBG_LOG(MSG, MOD_SSP, "Cache name : %s", newUri );
    }

    /* Bitrate Detection for auto rate control or fast start time */
    if ((ssp_cfg->rate_control.active && /* rate control*/
	 ssp_cfg->rate_control.active_param == PARAM_ENABLE_TYPE_AUTO) ||
        (ssp_cfg->fast_start.enable && /* fast start time*/
	 ssp_cfg->fast_start.active_param == PARAM_ENABLE_TYPE_TIME)) {
        /* do we need the seek config?*/
        if (con->ssp.attr_state != 50)
	{
            rv = ssp_detect_media_attr(con, ssp_cfg, contentLen, dataBuf, bufLen);
            if ( rv < 0 || rv == SSP_WAIT_FOR_DATA || rv == SSP_SEND_DATA_BACK) {
                goto exit;
            }
	    if (rv == 0) {
		/*
		 * rv == 0 means that we finish the detect of media attr
		 * we should bypass ssp_detect_media_attr loop for the seek loop
		 */
		con->ssp.attr_state = 50;
	    }
        }
    }


    // Seek handling
    if (ssp_cfg->seek.enable && ssp_cfg->seek.uri_query) { // Is CLI configured

	get_query_arg_by_name(&phttp->hdr, ssp_cfg->seek.uri_query, strlen(ssp_cfg->seek.uri_query), &querySeekTmp, &seekLen);
	TSTR(querySeekTmp, seekLen, querySeek);
	if ( strncmp(querySeek, "", 1) == 0 ) {
            querySeek = NULL;
        }

	if (querySeek != NULL) {
	    if (ssp_cfg->seek.activate_tunnel && atoi(querySeek) > 0) { // Force tunnel logic for seek requests
		delete_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI);
		con->ssp.attr_state = 0;
		rv = -NKN_SSP_FORCE_TUNNEL_PATH;
		goto exit;
	    }

	    if ( ssp_cfg->seek.flv_off_type == NULL || ssp_cfg->seek.mp4_off_type == NULL ) { // BZ 8464
		DBG_LOG(ERROR, MOD_SSP,
                    "CLI Seek Config nodes for virtual player are not set"
			"Will tunnel the request");
		delete_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI);
		con->ssp.attr_state = 0;
		rv = -NKN_SSP_FORCE_TUNNEL_PATH;
		goto exit;
	    }

	    switch (con->ssp.ssp_container_id) {
		case CONTAINER_TYPE_MP4:
		case CONTAINER_TYPE_3GPP:
		case CONTAINER_TYPE_3GP2:
		    if ( strncmp(ssp_cfg->seek.mp4_off_type, "time-secs", \
				 strlen("time-secs")) == 0) {
			//timeUnits is 1 unit
			queryrv = query_mp4_seek(con, ssp_cfg->seek.uri_query, \
                                                 contentLen, dataBuf, bufLen, 1);
		    } else if ( strncmp(ssp_cfg->seek.mp4_off_type, "time-msec", \
					strlen("time-msec")) == 0) {
			//timeUnits is 1000 unit
			queryrv = query_mp4_seek(con, ssp_cfg->seek.uri_query, \
						 contentLen, dataBuf,  bufLen, 1000);
		    }
                    rv = queryrv;
                    goto exit;
		    break;

		case CONTAINER_TYPE_FLV:
		    if (strncmp(ssp_cfg->seek.flv_off_type, "byte-offset",
				strlen("byte-offset")) == 0) {
			queryrv = query_flv_seek(con, ssp_cfg->seek.uri_query,
						 ssp_cfg->seek.query_length);

		    } else if (strncmp(ssp_cfg->seek.flv_off_type, "time-secs",
				       strlen("time-secs")) == 0) {
			queryrv = query_flv_timeseek(con,
						     ssp_cfg->seek.uri_query,
						     contentLen, dataBuf, bufLen, 1);

		    } else if (strncmp(ssp_cfg->seek.flv_off_type, "time-msec",
				       strlen("time-msec")) == 0) {
			queryrv = query_flv_timeseek(con, ssp_cfg->seek.uri_query,
						     contentLen, dataBuf, bufLen, 1000);
		    }
                    rv = queryrv;
                    goto exit;
                    break;

                default:
                    break;
            }
        }
    } else {
 	// Generate NKN_SEEK_URI equivalent for origin fetch
	generate_om_seek_uri(con, ssp_cfg->seek.uri_query);
    }


#if 0
    /* Bitrate Detection for auto rate control or fast start time */
    if ((ssp_cfg->rate_control.active && /* rate control*/
	ssp_cfg->rate_control.active_param == PARAM_ENABLE_TYPE_AUTO) ||
	(ssp_cfg->fast_start.enable && /* fast start time*/
	ssp_cfg->fast_start.active_param == PARAM_ENABLE_TYPE_TIME)) {
	/* do we need the seek config?*/
	//if (ssp_cfg->seek.enable && ssp_cfg->seek.uri_query)
	    {
	    rv = ssp_detect_media_attr(con, ssp_cfg, contentLen, dataBuf, bufLen);
	    if ( rv < 0 || rv == SSP_WAIT_FOR_DATA || rv == SSP_SEND_DATA_BACK) {
		goto exit;
	    }
	}
    }
#endif

    con->ssp.ssp_activated = TRUE;
    rv = SSP_SEND_DATA_OTW;

 exit:

    if (rv==SSP_SEND_DATA_OTW) {
	con->max_bandwidth = sess_bandwidth_limit;
	// Set the fast start buffer size
	ssp_set_faststart_size(con, ssp_cfg, contentLen);

	//Throttle to Rate Control (Param for BW throttling)
	rv = ssp_set_rc(con, ssp_cfg);
    }

    if (rv == SSP_SEND_DATA_OTW) {
	CLEAR_HTTP_FLAG(phttp, HRF_SSP_NO_AM_HITS);
    } else {
	SET_HTTP_FLAG(phttp, HRF_SSP_NO_AM_HITS);
    }

    return rv;
}

/*
 * The transcoding state change workflow is:
 * state  0: Try to ftech the lowrate video file, go to state 10;
 * state 10: If the lowrate is in cache, serve the lowrate video,
 *           set state to 20, get out of the state machine
 *           otherwise, Try to fetch the original video file, go to state 15;
 * state 15: Serve the original video file and check the hotness of this
 *           video to trigger the transcode task. set state to 20
 * state 20: do not do anything, just serve the video in REMAPPED_URI,
 */
static int
youtube_ssp_transcode_state(con_t *con,
			    const char *hostData,
			    int hostLen,
			    const ssp_config_t *ssp_cfg,
			    const namespace_config_t *cfg,
			    off_t contentLen,
			    int transType,
			    uint64_t* threshold,
			    char transRatioData[][4],
			    int* transRatioLen,
			    char *newUri,
			    int newUriLen,
			    int idLen,
			    int fmtLen,
			    const char *queryId,
			    const char *queryFmt)
{
    http_cb_t *phttp = &con->http;
    int rv;
    char *lowrateUri;
    int lowrateUriLen = 0;
    const char *nsuriPrefix = cfg->sm.up.uri_prefix; // "/videoplayback"
    char lowrateuriIdPrefix[] = "_yt_id_";
    /*
     * use this to decide the complexity of the transcoder
     * if it is not very time sensitive, we can use two pass
     * if it is very time sensitive, we will use one pass and fast preset
     * This function is not supported now
     */
    char transComplexData[] = "1";
    int transComplexLen = strlen(transComplexData);
    int transcoderv;

    /* transcoding feature is turned on */
    if (transType & con->ssp.ssp_container_id) {
	/* this video file can be transcoded according to CLI */
	switch (con->ssp.transcode_state) {
	    case 0:
		/* Generate name of the transcoded version of this video */
		lowrateUriLen = strlen(lowrateuriIdPrefix)
		    + idLen + strlen("_fmt_") + fmtLen + 1 + strlen("_lowrate");
		if (nsuriPrefix) {
		    lowrateUriLen += strlen(nsuriPrefix);
		}
		lowrateUri = (char *)alloca(lowrateUriLen);
		memset(lowrateUri, 0, lowrateUriLen);
		if (nsuriPrefix) {
		    memcpy(lowrateUri, nsuriPrefix, strlen(nsuriPrefix));
		}
		strcat(lowrateUri, lowrateuriIdPrefix);
		strncat(lowrateUri, queryId, idLen);
		strcat(lowrateUri, "_fmt_");
		strncat(lowrateUri, queryFmt, fmtLen);
		strcat(lowrateUri, "_lowrate");
		lowrateUri[lowrateUriLen -1] = '\0';

		/* ReMap the lowrate file internal cache name to REMAPPED_URI */
		add_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI,
				 lowrateUri, strlen(lowrateUri));
		DBG_LOG(MSG, MOD_SSP, "Transcode: lowrate video : %s. state %d.",
			lowrateUri, con->ssp.transcode_state);

		SET_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
		con->ssp.transcode_state = 10;
		rv = SSP_SEND_DATA_BACK;
		break;
	    case 10:
		if (contentLen == OM_STAT_SIG_TOT_CONTENT_LEN || contentLen == 0){
		    /*
		     * the lowrate file is not in cache
		     * remap the original file internal cache name to REMAPPED_URI
		     */
		    add_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI,
				     newUri, strlen(newUri));
		    con->ssp.transcode_state = 15;
		    rv = SSP_SEND_DATA_BACK;
		    /* fetch only from the buffer */
		    SET_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
		    //CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
		} else {
		    /* the lowrate video is in cache, serve the low rate one */
		    con->ssp.transcode_state = 20;
		    rv = SSP_SEND_DATA_OTW;
		}
		break;
	    case 15:
		if (contentLen == OM_STAT_SIG_TOT_CONTENT_LEN || contentLen == 0) {
		    /* the original video file is not in cache */
		    DBG_LOG(MSG, MOD_SSP,
			    "Transcode: original video is not in cache, "
			    "will handle it from om. state %d.",
			    con->ssp.transcode_state);
		    CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
		    con->ssp.transcode_state = 20;
		    rv = SSP_SEND_DATA_OTW;
		} else {
		    /* the original video file is in cache */
		    uint64_t hotval = 0;
		    nkn_uol_t uol = {0,0,0};
		    nkn_attr_t *ap = NULL;
		    uol.cod = con->http.req_cod;
		    nkn_buffer_t *abuf = nkn_buffer_get(&uol, NKN_BUFFER_ATTR);

		    if (abuf) {
			ap = (nkn_attr_t *) nkn_buffer_getcontent(abuf);
			hotval = ap->hotval&0x00000000ffffffff;
			nkn_buffer_release(abuf);
		    }

		    if (hotval > threshold[con->ssp.ssp_container_id - 1]) {
			/* hotness larger than threshold */
			if (ssp_cfg->seek.enable &&
			    ssp_cfg->seek.uri_query &&
			    ssp_cfg->video_id.video_id_uri_query &&
			    ssp_cfg->video_id.format_tag) {
			    /* need the seek.uri_query for obtain the full video file */
                                transcoderv = \
				    submit_vpemgr_youtube_transcode_request( \
						hostData, hostLen,
						ssp_cfg->video_id.video_id_uri_query,
						strlen(ssp_cfg->video_id.video_id_uri_query),
						ssp_cfg->video_id.format_tag,
						strlen(ssp_cfg->video_id.format_tag),
						newUri, newUriLen,
						nsuriPrefix, nsuriPrefix ? strlen(nsuriPrefix) : 0,
						transRatioData[con->ssp.ssp_container_id - 1],
						transRatioLen[con->ssp.ssp_container_id - 1],
						transComplexData, transComplexLen);
			}

			if (transcoderv) {
			    transcoderv = -NKN_SSP_PYTHON_PLUGIN_FAILURE;
			    DBG_LOG(WARNING, MOD_SSP,
				    "state : %d. Request to trigger transcode failed. rv =%d",
				    con->ssp.transcode_state, transcoderv);
			}
		    } /* if (hotval>threadshold) */
		    CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
		    con->ssp.transcode_state = 20;
		    rv = SSP_SEND_DATA_OTW;
		} /* if (contentLen == OM_STAT_SIG_TOT_CONTENT_LEN || contentLen == 0) */
		break;
	    case 20:
		/* Just serve the video in REMAPPED_URI */
		rv = SSP_SEND_DATA_OTW;
		break;

	    default:
		DBG_LOG(WARNING, MOD_SSP,
			"Transcoding (State: Default): Not supported state = %d",
			con->ssp.seek_state);
		add_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI,
				 newUri, strlen(newUri));
		DBG_LOG(MSG, MOD_SSP, "Cache name : %s", newUri);
		rv = SSP_SEND_DATA_OTW;
		break;
	} /* switch (con->ssp.transcode_state) */
    } else {
	/*
	 * this videe file is not transcoded according to CLI
	 *  Map the original to REMAPPED_URI
	 */
	add_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI,
			 newUri, strlen(newUri));
	DBG_LOG(MSG, MOD_SSP, "Cache name : %s", newUri);
	rv = SSP_SEND_DATA_OTW;
    } /* if (transType & con->ssp.ssp_container_id) */

    return rv;
}


/*
 * the up/down grade state machine loop
 *
 *
 */
static int
s_youtube_up_down_grade(con_t* con,
			const ssp_config_t *ssp_cfg,
			off_t contentLen,
			const char *dataBuf,
			int bufLen,
			int* up_down_flag,
			int* limiting_bitrate,
			char *newUri,
			const char *queryFmt,
			const char *queryId,
			int idLen)
{
    int       rv = 0;
    http_cb_t *phttp = &con->http;
    int       up_down_flag_fmt;
    int       limiting_bitrate_fmt;

    up_down_flag_fmt = up_down_flag[con->ssp.ssp_container_id - 1];
    limiting_bitrate_fmt = limiting_bitrate[con->ssp.ssp_container_id - 1];



    if (up_down_flag_fmt == 1 || up_down_flag_fmt == 0) {
	if (con->ssp.ssp_container_id == CONTAINER_TYPE_WEBM) {
	    /* WebM cannot use this best effort up/downgrade  */
	    // Map the internal cache name to REMAPPED_URI
	    add_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI,
			     newUri, strlen(newUri));
	    DBG_LOG(MSG, MOD_SSP, "WebM: Cache name : %s", newUri );
	} else if ((con->ssp.ssp_container_id == CONTAINER_TYPE_FLV &&
		   ssp_cfg->seek.flv_off_type == NULL) ||
		   (con->ssp.ssp_container_id == CONTAINER_TYPE_MP4 &&
		    ssp_cfg->seek.mp4_off_type == NULL)) {
	    /* If the seek type is missing, we should not up/downgrade */
	    // Map the internal cache name to REMAPPED_URI
            add_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI,
                             newUri, strlen(newUri));
            DBG_LOG(MSG, MOD_SSP, "No Seek type: Cache name : %s", newUri );
	} else if (con->ssp.ssp_container_id == CONTAINER_TYPE_FLV &&
		   ssp_cfg->seek.flv_off_type != NULL &&
		   (strncmp(ssp_cfg->seek.flv_off_type, "byte-offset",
			    strlen("byte-offset")) == 0)) {
	    /* If FLV uses byte-offset seek, we cannot up/downgrade */
            // Map the internal cache name to REMAPPED_URI
            add_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI,
                             newUri, strlen(newUri));
            DBG_LOG(MSG, MOD_SSP, "FLV byte-offset: Cache name : %s", newUri );
	} else { /* FLV or MP4 with time seek */
	    uint8_t oriFmt_state;
	    /*
	     * For FLV or MP4 file, if client use byte-range request header,
	     * we cannot do up grade or down grade
	     */
	    if (con->ssp.rate_opt_state == 0) {
		con->ssp.ssp_video_br_flag = 0;
		if (CHECK_HTTP_FLAG(phttp, HRF_BYTE_RANGE)) {
		    con->ssp.ssp_video_br_flag = 1;
		}
	    }

	    if (con->ssp.ssp_video_br_flag == 1) {
		add_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI,
				 newUri, strlen(newUri));
		DBG_LOG(MSG, MOD_SSP,
			"MP4/FLV byte-range request, no up/downgrade. "
			"Cache name : %s", newUri);
		rv = SSP_SEND_DATA_OTW;
		goto exit;
	    }

	    switch (atoi(queryFmt)) {
                case 18: /* MP4 container (480x270) - Medium quality */
                    oriFmt_state = up_down_flag_fmt ? 20 : 0;
                    break;
                case 22: /* MP4 container (1280x720) - 720p quality */
                    oriFmt_state = up_down_flag_fmt ? 10 : 10 ;
                    break;
                case 37: /* MP4 container (1920x1080) - 1080p quality */
                    oriFmt_state = up_down_flag_fmt ? 0 : 20 ;
                    break;

                case 5: /* FLV container (320x240) - Looks like this is streaming http */
                    oriFmt_state = up_down_flag_fmt ? 20 : 0;
                    break;
                case 34: /* FLV container (320x240 to 640x480) - Normal quality */
                    oriFmt_state = up_down_flag_fmt ? 10 : 10;
                    break;
                case 35: /* FLV container (854x480) - High quality 480p */
                    oriFmt_state = up_down_flag_fmt ? 0 : 20;
                    break;
                default: /* Just to process some know things */
                    oriFmt_state = 0;
                    break;
	    } /* end of switch (atoi(queryFmt)) */

	    /* the up/downgrade state machine*/
	    switch (con->ssp.rate_opt_state) {
		case 0:
		    /*
		     * FLV: Downgrade Remap fmt  5, Upgrade Remap fmt 35
		     * MP4: Downgrade Remap fmt 18, Upgrade Remap fmt 37
		     */
		    s_remap_up_down_url(con, queryId, idLen,
                                        con->ssp.rate_opt_state,
					up_down_flag_fmt,
					con->ssp.ssp_container_id);
		    con->ssp.rate_opt_state = 10;
                    rv = SSP_SEND_DATA_BACK;
                    goto exit;
		    break;

		case 10:
		    if (contentLen == OM_STAT_SIG_TOT_CONTENT_LEN ||
                        contentLen == 0) { /* not in cache */
                        if (con->ssp.rate_opt_state/10 > oriFmt_state/10) {
                            /* already reach the current video's fmt */
			    CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
                            DBG_LOG(MSG, MOD_SSP,
				    "remapped up/downgrade video not in cache, "
                                    "reached video fmt in uri, have to serve it. state = %d",
				    con->ssp.rate_opt_state);
			    con->ssp.rate_opt_state = 50;
			    rv = SSP_SEND_DATA_OTW;
                            goto exit;
                        } else {
                            /*
                             * FLV: Downgrade Remap fmt 34, Upgrade Remap fmt 34
                             * MP4: Downgrade Remap fmt 22, Upgrade Remap fmt 22
                             */
			    DBG_LOG(MSG, MOD_SSP,
                                    "remapped up/downgrade video not in cache, "
				    "do another remap. state = %d",
                                    con->ssp.rate_opt_state);
                            s_remap_up_down_url(con, queryId, idLen,
						con->ssp.rate_opt_state,
						up_down_flag_fmt,
						con->ssp.ssp_container_id);

                            con->ssp.rate_opt_state = 20;
                            rv = SSP_SEND_DATA_BACK;
                            goto exit;
                        }
                    } else { /* first 32KB in cache*/
			/*
			 * The request up/downgrade video is in cache
			 * We need to check whether entire in video cache for next step
			 */
			if (con->ssp.rate_opt_state/10 > oriFmt_state/10) {
                            /* already reach the current video's fmt */
                            CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
                            DBG_LOG(MSG, MOD_SSP,
                                    "remapped up/downgrade video in cache, "
                                    "reached video fmt in uri, have to serve it. state = %d",
                                    con->ssp.rate_opt_state);
                            con->ssp.rate_opt_state = 50;
                            rv = SSP_SEND_DATA_OTW;
                            goto exit;
                        } else {

			    DBG_LOG(MSG, MOD_SSP,
				    "start to check full file in cache. state = %d",
				    con->ssp.rate_opt_state);
			    SET_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
			    phttp->brstart = contentLen-128;
			    phttp->brstop = 0;
			    SET_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
			    con->ssp.rate_opt_state = 11;
			    rv = SSP_SEND_DATA_BACK;
			    goto exit;
			}
		    }
		    break;

		case 11:
		    if (contentLen == OM_STAT_SIG_TOT_CONTENT_LEN) {
			/* full file not in cache*/

			/* reset the byte range request */
                        if (CHECK_HTTP_FLAG(phttp, HRF_BYTE_RANGE)) {
                            CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
                            phttp->brstart = 0;
                            phttp->brstop = 0;
                        }

			if (con->ssp.rate_opt_state/10 > oriFmt_state/10) {
                            /* already reach the current video's fmt */
                            CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
			    DBG_LOG(MSG, MOD_SSP,
				    "full file not in cache, "
                                    "reached video fmt in uri, have to serve it. state = %d",
                                    con->ssp.rate_opt_state);
                            con->ssp.rate_opt_state = 50;
			    rv = SSP_SEND_DATA_BACK;
                            goto exit;
                        } else {
                            /*
                             * FLV: Downgrade Remap fmt 34, Upgrade Remap fmt 34
                             * MP4: Downgrade Remap fmt 22, Upgrade Remap fmt 22
                             */
			    DBG_LOG(MSG, MOD_SSP,
                                    "remapped up/downgrade video not full file in cache, "
                                    "do another remap. state = %d",
                                    con->ssp.rate_opt_state);
                            s_remap_up_down_url(con, queryId, idLen,
                                                con->ssp.rate_opt_state,
                                                up_down_flag_fmt,
                                                con->ssp.ssp_container_id);
                            con->ssp.rate_opt_state = 20;
                            rv = SSP_SEND_DATA_BACK;
                            goto exit;
                        }
		    } else { /* full file in cache */
                        DBG_LOG(MSG, MOD_SSP,
                                "full file in cache. state = %d",
                                con->ssp.rate_opt_state);
			/* reset the byte range request*/
			if (CHECK_HTTP_FLAG(phttp, HRF_BYTE_RANGE)) {
			    CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
			    phttp->brstart = 0;
			    phttp->brstop = 0;
			}
			con->ssp.rate_opt_state = 12;
			/* re-obtain the video from start */
			rv = SSP_SEND_DATA_BACK;
			goto exit;
		    }
		    break;

		case 12:
		    /*
		     * In state 12, the full file is in cache
		     * We will check the bitrate of this up/downed video
		     * If it satisfies the threshold, we will serve this,
		     * If not, we will try to serve another one
		     */

		    /* Bitrate Detection for this fmt */
		    if (ssp_cfg->seek.enable && ssp_cfg->seek.uri_query) {
			rv = ssp_detect_media_attr(con, ssp_cfg,
						   contentLen, dataBuf,
						   bufLen);
			if (rv < 0 || rv == SSP_WAIT_FOR_DATA || rv == SSP_SEND_DATA_BACK) {
			    goto exit;
			}
		    }

		    if (con->ssp.ssp_bitrate != 0 &&  /* 0 means fail to fetch the bitrate*/
			((con->ssp.ssp_bitrate > limiting_bitrate_fmt && up_down_flag_fmt == 0) || /* downgrade */
			 (con->ssp.ssp_bitrate < limiting_bitrate_fmt && up_down_flag_fmt == 1))) /* upgrade */ {
			/* satisfy the threshold, serve it*/
			DBG_LOG(MSG, MOD_SSP,
				"satisfy the up/down limit rate. state = %d",
				con->ssp.rate_opt_state);

			if (con->ssp.ssp_container_id == CONTAINER_TYPE_FLV) {
			    con->ssp.serve_fmt = up_down_flag_fmt ? 5 : 35;
			    DBG_LOG(MSG, MOD_SSP,
				    "FLV serve fmt 5 for down, 35 for up. state = %d",
				    con->ssp.rate_opt_state);
			} else {
			    con->ssp.serve_fmt = up_down_flag_fmt ? 18 : 37;
			    DBG_LOG(MSG, MOD_SSP,
				    "MP4 serve fmt 18 for down, 37 for up. state = %d",
				    con->ssp.rate_opt_state);
			}
			CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
			con->ssp.rate_opt_state = 50;
			rv = SSP_SEND_DATA_OTW;
			goto exit;
		    } else {
			/* not satisfy the threshould, try another one */
			if (con->ssp.rate_opt_state/10 > oriFmt_state/10) {
			    /* already reach the current video's fmt */
			    DBG_LOG(MSG, MOD_SSP,
				    "Not satisfy the up/down limit rate, "
                                    "reached video fmt in uri, have to serve it. state = %d",
                                    con->ssp.rate_opt_state);
			    if (con->ssp.ssp_container_id == CONTAINER_TYPE_FLV) {
				con->ssp.serve_fmt = up_down_flag_fmt ? 5 : 35;
				DBG_LOG(MSG, MOD_SSP,
					"FLV serve fmt 5 for down, 35 for up. state = %d",
					con->ssp.rate_opt_state);
			    } else {
				con->ssp.serve_fmt = up_down_flag_fmt ? 18 : 37;
				DBG_LOG(MSG, MOD_SSP,
					"MP4 serve fmt 18 for down, 37 for up. state = %d",
					con->ssp.rate_opt_state);
			    }
			    CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
			    con->ssp.rate_opt_state = 50;
			    rv = SSP_SEND_DATA_OTW;
			} else { /* have not reach the current video's fmt*/
			    /*
			     * FLV: Downgrade Remap fmt 34, Upgrade Remap fmt 34
			     * MP4: Downgrade Remap fmt 22, Upgrade Remap fmt 22
			     */
			    DBG_LOG(MSG, MOD_SSP,
				    "Not satisfy the up/down limit rate, "
				    "do another remap. state = %d",
				    con->ssp.rate_opt_state);
			    s_remap_up_down_url(con, queryId, idLen,
						con->ssp.rate_opt_state,
						up_down_flag_fmt,
						con->ssp.ssp_container_id);
			    con->ssp.rate_opt_state = 20;
			    rv = SSP_SEND_DATA_BACK;
			    goto exit;
			}
		    } /* if ((con->ssp.ssp_bitrate > limiting_bitrate && */
                    break;

		case 20:
                    if (contentLen == OM_STAT_SIG_TOT_CONTENT_LEN ||
                        contentLen == 0) { /* not in cache */
                        if (con->ssp.rate_opt_state/10 > oriFmt_state/10) {
                            /* already reach the current video's fmt */
                            CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
                            DBG_LOG(MSG, MOD_SSP,
				    "remapped up/downgrade video not in cache, "
                                    "reached video fmt in uri, have to serve it. state = %d",
				    con->ssp.rate_opt_state);
			    con->ssp.rate_opt_state = 50;
			    rv = SSP_SEND_DATA_OTW;
                            goto exit;
                        } else {
			    /*
                             * FLV: Downgrade Remap fmt 35, Upgrade Remap fmt 5
                             * MP4: Downgrade Remap fmt 37, Upgrade Remap fmt 18
                             */
			    DBG_LOG(MSG, MOD_SSP,
                                    "remapped up/downgrade video not in cache, "
                                    "do another remap. state = %d",
                                    con->ssp.rate_opt_state);
                            s_remap_up_down_url(con, queryId, idLen,
						con->ssp.rate_opt_state,
						up_down_flag_fmt,
						con->ssp.ssp_container_id);

                            con->ssp.rate_opt_state = 30;
                            rv = SSP_SEND_DATA_BACK;
                            goto exit;
                        }
                    } else { /* first 32KB in cache*/
			/*
                         * The request up/downgrade video is in cache
			 * We need to check whether entire in video cache for next step
			 */
			if (con->ssp.rate_opt_state/10 > oriFmt_state/10) {
                            /* already reach the current video's fmt */
			    CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
			    DBG_LOG(MSG, MOD_SSP,
                                    "remapped up/downgrade video in cache, "
                                    "reached video fmt in uri, have to serve it. state = %d",
                                    con->ssp.rate_opt_state);
                            con->ssp.rate_opt_state = 50;
                            rv = SSP_SEND_DATA_OTW;
                            goto exit;
                        } else {
			    DBG_LOG(MSG, MOD_SSP,
				    "start to check full file in cache. state = %d",
				    con->ssp.rate_opt_state);
			    SET_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
			    phttp->brstart = contentLen-128;
			    phttp->brstop = 0;
			    SET_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
			    con->ssp.rate_opt_state = 21;
			    rv = SSP_SEND_DATA_BACK;
			    goto exit;
			}
                    }
                    break;

		case 21:
		    if (contentLen == OM_STAT_SIG_TOT_CONTENT_LEN) {
                        /* full file not in cache*/

                        /* reset the byte range request */
                        if (CHECK_HTTP_FLAG(phttp, HRF_BYTE_RANGE)) {
                            CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
                            phttp->brstart = 0;
                            phttp->brstop = 0;
                        }

                        if (con->ssp.rate_opt_state/10 > oriFmt_state/10) {
			    /* already reach the current video's fmt */
                            CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
                            DBG_LOG(MSG, MOD_SSP,
				    "full file  not in cache, "
                                    "reached video fmt in uri, have to serve it. state = %d",
                                    con->ssp.rate_opt_state);
                            con->ssp.rate_opt_state = 50;
                            rv = SSP_SEND_DATA_BACK;
                            goto exit;
                        } else {
                            /*
			     * FLV: Downgrade Remap fmt 35, Upgrade Remap fmt 5
                             * MP4: Downgrade Remap fmt 37, Upgrade Remap fmt 18
                             */
			    DBG_LOG(MSG, MOD_SSP,
                                    "remapped up/downgrade video not full file in cache, "
                                    "do another remap. state = %d",
                                    con->ssp.rate_opt_state);
                            s_remap_up_down_url(con, queryId, idLen,
                                                con->ssp.rate_opt_state,
                                                up_down_flag_fmt,
                                                con->ssp.ssp_container_id);
                            con->ssp.rate_opt_state = 30;
                            rv = SSP_SEND_DATA_BACK;
                            goto exit;
                        }
                    } else { /* full file in cache */
                        DBG_LOG(MSG, MOD_SSP,
                                "full file in cache. state = %d",
                                con->ssp.rate_opt_state);
                        /* reset the byte range request*/
                        if (CHECK_HTTP_FLAG(phttp, HRF_BYTE_RANGE)) {
                            CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
                            phttp->brstart = 0;
                            phttp->brstop = 0;
                        }
                        con->ssp.rate_opt_state = 22;
                        /* re-obtain the video from start */
                        rv = SSP_SEND_DATA_BACK;
                        goto exit;
                    }
                    break;

		case 22:
		    /*
                     * In state 22, the full file is in cache
                     * We will check the bitrate of this up/downed video
                     * If it satisfies the threshold, we will serve this,
                     * If not, we will try to serve another one
                     */

		    /* Bitrate Detection for this fmt */
		    if (ssp_cfg->seek.enable && ssp_cfg->seek.uri_query) {
			rv = ssp_detect_media_attr(con, ssp_cfg,
						   contentLen, dataBuf,
						   bufLen);
			if (rv < 0 || rv == SSP_WAIT_FOR_DATA || rv == SSP_SEND_DATA_BACK) {
			    goto exit;
			}
		    }

		    if (con->ssp.ssp_bitrate != 0 && /* 0 means fail to fetch the bitrate*/
			((con->ssp.ssp_bitrate > limiting_bitrate_fmt && up_down_flag_fmt == 0) || /* downgrade */
			 (con->ssp.ssp_bitrate < limiting_bitrate_fmt && up_down_flag_fmt == 1))) /* upgrade */ {
			/* satisfy the threshold, serve it*/
			DBG_LOG(MSG, MOD_SSP,
                                "satisfy the up/down limit rate. state = %d",
                                con->ssp.rate_opt_state);
			if (con->ssp.ssp_container_id == CONTAINER_TYPE_FLV){
			    con->ssp.serve_fmt = up_down_flag_fmt ? 34 : 34;
			    DBG_LOG(MSG, MOD_SSP,
				    "FLV serve fmt 34 for down, 34 for up. state = %d",
				    con->ssp.rate_opt_state);
			} else {
			    con->ssp.serve_fmt = up_down_flag_fmt ? 22 : 22;
			    DBG_LOG(MSG, MOD_SSP,
				    "MP4 serve fmt 22 for down, 22 for up. state = %d",
				    con->ssp.rate_opt_state);
			}
			CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
			con->ssp.rate_opt_state = 50;
			rv = SSP_SEND_DATA_OTW;
			goto exit;
		    } else {
			/* not satisfy the threshould, try another one */
			if (con->ssp.rate_opt_state/10 > oriFmt_state/10) {
			    /* already reach the current video's fmt */
			    DBG_LOG(MSG, MOD_SSP,
                                "Not satisfy the up/down limit rate, "
				    "reached video fmt in uri, have to serve it. state = %d",
				    con->ssp.rate_opt_state);
			    if (con->ssp.ssp_container_id == CONTAINER_TYPE_FLV) {
				con->ssp.serve_fmt = up_down_flag_fmt ? 34 : 34;

				DBG_LOG(MSG, MOD_SSP,
					"FLV serve fmt 34 for down, 34 for up. state = %d",
					con->ssp.rate_opt_state);
			    } else {
				con->ssp.serve_fmt = up_down_flag_fmt ? 22 : 22;

				DBG_LOG(MSG, MOD_SSP,
					"MP4 serve fmt 22 for down, 22 for up. state = %d",
					con->ssp.rate_opt_state);
			    }
			    CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
			    con->ssp.rate_opt_state = 50;
			    rv = SSP_SEND_DATA_OTW;
			    goto exit;
			} else { /* have not reach the current video's fmt*/
			    /*
			     * FLV: Downgrade Remap fmt 35, Upgrade Remap fmt 5
			     * MP4: Downgrade Remap fmt 37, Upgrade Remap fmt 18
			     */
			    DBG_LOG(MSG, MOD_SSP,
                                    "Not satisfy the up/down limit rate,"
                                    "do another remap. state = %d",
                                    con->ssp.rate_opt_state);
			    s_remap_up_down_url(con, queryId, idLen,
						con->ssp.rate_opt_state,
						up_down_flag_fmt,
						con->ssp.ssp_container_id);
			    con->ssp.rate_opt_state = 30;
			    rv = SSP_SEND_DATA_BACK;
			    goto exit;
			}
		    } /* if ((con->ssp.ssp_bitrate > limiting_bitrate && */
		    break;

                case 30:
                    if (contentLen == OM_STAT_SIG_TOT_CONTENT_LEN ||
                        contentLen == 0) { /* not in cache */
                        if (con->ssp.rate_opt_state/10 > oriFmt_state/10) {
			    /* already reach the current video's fmt */
                            CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
                            DBG_LOG(MSG, MOD_SSP,
                                    "remapped up/downgrade video not in cache, "
                                    "reached video fmt in uri, have to serve it. state = %d",
                                    con->ssp.rate_opt_state);
			    con->ssp.rate_opt_state = 50;
			    rv = SSP_SEND_DATA_OTW;
                            goto exit;
                        }
                    } else {
                        /* the request video is in cache */
			if (con->ssp.ssp_container_id == CONTAINER_TYPE_FLV){
			    con->ssp.serve_fmt = up_down_flag_fmt ? 35 : 5;
			    DBG_LOG(MSG, MOD_SSP,
				    "reached video fmt in uri, "
				    "FLV serve fmt 35 for down, 5 for up. state = %d",
				    con->ssp.rate_opt_state);
			} else {
			    con->ssp.serve_fmt = up_down_flag_fmt ? 37 : 18;
			    DBG_LOG(MSG, MOD_SSP,
				    "reached video fmt in uri, "
                                    "MP4 serve fmt 37 for down, 18 for up. state = %d",
                                    con->ssp.rate_opt_state);
			}
			CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
                        con->ssp.rate_opt_state = 50;
			rv = SSP_SEND_DATA_OTW;
			goto exit;
                    }
                    break;

                case 50:
                    /* Just serve the video in REMAPPED_URI */
		    rv = SSP_SEND_DATA_OTW;
                    break;
                default:
                    DBG_LOG(WARNING, MOD_SSP,
                            "Changegrade (State: Default): Not supported state = %d",
                            con->ssp.rate_opt_state);
		    CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
                    add_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI,
				     newUri, strlen(newUri));
                    DBG_LOG(MSG, MOD_SSP, "Cache name : %s", newUri );
		    rv = SSP_SEND_DATA_OTW;
		    goto exit;
                    break;
	    } /* switch (con->ssp.rate_opt_state) */
	} /* if (con->ssp.ssp_container_id = CONTAINER_TYPE_WEBM) */
    }

 exit:
    return rv;
}


/*
 * Generate the up/downgrade URI for current Fmt
 * parm up_down is used to decide up or downgrade
 * up_down 0: downgrade, 1: upgrade
 */
static int
s_remap_up_down_url(con_t *con,
		    const char *queryId,
		    int idLen,
		    int curFmt_state,
		    int up_down,
		    int ssp_container_id)
{
    char fmtStrName[2][YT_SSP_FMT_NUM][3] = {{ "5", "34", "35",
                                              "18", "22", "37"},
                                             {"35", "34",  "5",
					      "37", "22", "18"}};

    int  fmtStrLen[2][YT_SSP_FMT_NUM]     = {{  1,    2,    2,
                                                2,    2,    2},
                                             {  2,    2,    1,
                                                2,    2,    2}};
    char uriPrefix[] = "/yt_video_id_";

    char *changeUri;
    int  changeUriLen;
    http_cb_t *phttp = &con->http;
    int start_no;
    int remap_fmt_no;
    remap_fmt_no = curFmt_state / 10;
    start_no = ssp_container_id == CONTAINER_TYPE_FLV ? 0 : 3;
    /* generate the up/downgrade video URI */
    changeUriLen = strlen(uriPrefix) + idLen + strlen("_fmt_")
	+ fmtStrLen[up_down][remap_fmt_no + start_no] + 1;
    changeUri = (char *)alloca(changeUriLen);
    memset(changeUri, 0, changeUriLen);
    memcpy(changeUri, uriPrefix, strlen(uriPrefix));
    strncat(changeUri,queryId, idLen);
    strcat(changeUri,"_fmt_");
    strcat(changeUri, fmtStrName[up_down][remap_fmt_no + start_no]);

    changeUri[changeUriLen - 1] = '\0';
    /* REMAP downgrade file internal cache name to REMAPPED_URI */
    add_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI,
                     changeUri, strlen(changeUri));
    DBG_LOG(MSG, MOD_SSP,
	    "REMAPPED up/downgrade  name : %s. state = %d",
	    changeUri, curFmt_state);
    SET_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);

    return 0;
}

/*
 * Set the youtube up/down grade paramters from CLI config
 */
static int
s_set_youtube_up_down_grade(const ssp_config_t *ssp_cfg,
			    int *up_down_type,
			    int *up_down_flag,
			    int *limiting_bitrate)
{
    int rv = 0;
    for (int i = 0; i < MAX_SSP_BW_FILE_TYPES; i++) {
	up_down_flag[i] = -1;
    }
    /* config parameters */
    for (int i = 0; i < MAX_SSP_BW_FILE_TYPES; i++) {
        /* config FLV */
        if (ssp_cfg->bandwidth_opt.info[i].file_type != NULL &&
            (strncmp(ssp_cfg->bandwidth_opt.info[i].file_type, "FLV", 3) == 0 ||
             strncmp(ssp_cfg->bandwidth_opt.info[i].file_type, "flv", 3) == 0)) {
            /* enable FLV up/downgrade */
            *up_down_type = *up_down_type | CONTAINER_TYPE_FLV;
            if (ssp_cfg->bandwidth_opt.info[i].switch_rate != NULL) {
                if (strncmp \
                    (ssp_cfg->bandwidth_opt.info[i].switch_rate, \
                     "lower", 5) == 0) {
                    up_down_flag[CONTAINER_TYPE_FLV - 1] = 0; /* downgrade */
                } else if (strncmp \
                           (ssp_cfg->bandwidth_opt.info[i].switch_rate, \
                            "higher", 5) == 0) {
                    up_down_flag[CONTAINER_TYPE_FLV - 1] = 1; /* upgrade */
                } else { /* handle abnormal condition */
                    *up_down_type = *up_down_type & (~(CONTAINER_TYPE_FLV));
                }
            } else { /* handle abnormal condition */
                *up_down_type = *up_down_type & (~(CONTAINER_TYPE_FLV));
            }
                limiting_bitrate[CONTAINER_TYPE_FLV-1] = \
                    ssp_cfg->bandwidth_opt.info[i].switch_limit_rate;
        }

        /* config MP4 */
        if (ssp_cfg->bandwidth_opt.info[i].file_type != NULL &&
            (strncmp(ssp_cfg->bandwidth_opt.info[i].file_type, "MP4", 3) == 0 ||
             strncmp(ssp_cfg->bandwidth_opt.info[i].file_type, "mp4", 3) == 0)) {
            /* enable MP4 up/downgrade */
            *up_down_type = *up_down_type | CONTAINER_TYPE_MP4;
            if (ssp_cfg->bandwidth_opt.info[i].switch_rate != NULL) {
                if (strncmp \
                    (ssp_cfg->bandwidth_opt.info[i].switch_rate, \
                     "lower", 5) == 0) {
		    up_down_flag[CONTAINER_TYPE_MP4 - 1] = 0; /* downgrade */
                } else if (strncmp \
                           (ssp_cfg->bandwidth_opt.info[i].switch_rate, \
                            "higher", 5) == 0) {
		    up_down_flag[CONTAINER_TYPE_MP4 - 1] = 1; /* upgrade */
                } else { /* handle abnormal condition */
                    *up_down_type = *up_down_type & (~(CONTAINER_TYPE_MP4));
                }
            } else { /* handle abnormal condition */
                /* disable MP4 up/down grade */
                *up_down_type = *up_down_type & (~(CONTAINER_TYPE_MP4));
            }
		limiting_bitrate[CONTAINER_TYPE_MP4 - 1] = \
		    ssp_cfg->bandwidth_opt.info[i].switch_limit_rate;
        }
    } /* for (int i = 0; i < MAX_SSP_BW_FILE_TYPES; i++) */

    return rv;
}


