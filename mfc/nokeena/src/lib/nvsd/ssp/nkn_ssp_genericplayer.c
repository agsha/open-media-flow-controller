/*
 * Nokeena Generic Server Side Player
 *
 */

#include <string.h>
#include <stdlib.h>
#include <openssl/md5.h>
#include <alloca.h>

#include "ssp.h"
#include "http_header.h"
#include "nkn_common_config.h"
#include "nkn_stat.h"
#include "nkn_ssp_players.h"
#include "nkn_errno.h"
#include "nkn_debug.h"
#include "ssp_authentication.h"
#include "nkn_ssp_player_prop.h"
#include "ssp_queryparams.h"
//#include "nkn_vpe_media_props_api.h"
#include "nkn_vpe_types.h"
#include "nkn_vpemgr_defs.h"
#include "nkn_cfg_params.h"

extern unsigned long long glob_ssp_container_flv, glob_ssp_container_mp4, glob_ssp_container_asf,
    glob_ssp_container_webm, glob_ssp_container_3gpp, glob_ssp_container_3gp2;

static int
generic_ssp_transcode_state(con_t *con, const char *uriData,
                            int uriLen, const char *hostData,
                            int hostLen, off_t contentLen,
                            int remapLen, int transType, uint64_t* threshold,
                            char transRatioData[][4], int* transRatioLen);
/////////////////////////////////////////////////////////////
// Nokeena Generic Server Side Player (Generic SSP Type 0) //
/////////////////////////////////////////////////////////////
int
nknGenericSSP(con_t *con, const char *uriData, int uriLen, const char *hostData, int hostLen, const ssp_config_t *ssp_cfg, off_t contentLen, const char *dataBuf, int bufLen)
{
    const char *queryData;
    int remapLen=0;
    int rv;

    char transRatioData[MAX_SSP_BW_FILE_TYPES][4];
    int transRatioLen[MAX_SSP_BW_FILE_TYPES];
    uint64_t threshold[MAX_SSP_BW_FILE_TYPES];
    /* use bit operation to decide what kind of video file will be transcode */
    int transType = 0x0; // FLV 0x0001 MP4 0x0002

    http_cb_t *phttp = &con->http;
    queryData = NULL;

    SET_HTTP_FLAG(phttp, HRF_SSP_NO_AM_HITS); // BZ 10462
    SET_HTTP_FLAG(phttp, HRF_SSP_INTERNAL_REQ); // BZ 10604

    if (ssp_cfg->bandwidth_opt.enable == 1) {
        /* config ssp transocde parameters from CLI */
        ssp_set_transcode(ssp_cfg, &transType, threshold,
                          transRatioData, transRatioLen);
    }

    // Obtain the relative URL length stripped of all query params
    if ( (remapLen = findStr(uriData, "?", uriLen)) == -1 ) {
	remapLen = uriLen;
        DBG_LOG(WARNING, MOD_SSP, "URL does not contain any query params");
    }

    // Verify the hash for video URL.
    // If successful then proceed, else reject connection
    if (con->ssp.ssp_hash_authed == 0) {
	rv = ssp_auth_md5hash_req(con, ssp_cfg, uriData, uriLen, hostData, hostLen);
	if (rv != 0) {
	    return rv;
	}
    }

    // For HEAD request, bypass SSP
    // BZ: 10161:
    // Data will never be delivered via HEAD. Only response headers are sent
    if (CHECK_HTTP_FLAG(phttp, HRF_METHOD_HEAD)) {
	DBG_LOG(MSG, MOD_SSP,
		"Method HEAD recieved by SSP, bypassing SSP code path, no data delivery");
	CLEAR_HTTP_FLAG(phttp, HRF_SSP_NO_AM_HITS);
	return 0;
    }

    if (con->ssp.transcode_state == 0) {
	/*
	 * use this uriData to obtain the original video file
	 * and fetch media attr
	 * this media attr is only the container type
	 */
	add_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI, uriData, remapLen);

	/*
	 * Fetch the media contianer type
	 */
	rv = ssp_detect_media_type(con, ssp_cfg, contentLen, dataBuf, bufLen);
	if (rv < 0 || rv == SSP_SEND_DATA_BACK) {
	    CLEAR_HTTP_FLAG(phttp, HRF_SSP_NO_AM_HITS);
	    goto exit;
	}

	if (0 == con->ssp.ssp_container_filetype) {
	    DBG_LOG(MSG, MOD_SSP,
		    "Requested container format not supported. Passing the file through anyways: %d",
		    NKN_SSP_CONTAINER_FRMT_NOT_SUPP);
	    //Check for allow download request for this exception
	    if (ssp_cfg->full_download.enable) {
		rv = ssp_full_download_req(con, ssp_cfg);
		if (rv == SSP_SEND_DATA_OTW)
		    goto exit;
	    }
	    goto fmt_not_supp;
	}
    }

    // Activate the callback flag to signal that SSP internal fetch is triggered
    con->ssp.ssp_callback_status = 1;

    /* Transcoding state machine workflow */
    if (ssp_cfg->bandwidth_opt.enable == 1) {
	rv = generic_ssp_transcode_state(con, uriData, uriLen, hostData, hostLen,
					 contentLen, remapLen, transType, threshold,
					 transRatioData, transRatioLen);
        if ( rv < 0 || rv == SSP_WAIT_FOR_DATA || rv == SSP_SEND_DATA_BACK) {
            goto exit;
        }
    } else {
	/*
	 * the bandwidth feature is turned off
	 * Map the original video to REMAPPED_URI
	 */
	add_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI,
			 uriData, remapLen);
    } /* if (transFlag==1) */


    //Check for allow download request
    if (ssp_cfg->full_download.enable) {
	rv = ssp_full_download_req(con, ssp_cfg);
	if (rv == SSP_SEND_DATA_OTW)
	    goto exit;
    }


    /*
     * Bitrate Detection for auto rate control or fast start time
     *
     */
    if ((ssp_cfg->rate_control.active && /* rate control*/
	 ssp_cfg->rate_control.active_param == PARAM_ENABLE_TYPE_AUTO) ||
	(ssp_cfg->fast_start.enable && /* fast start time*/
	 ssp_cfg->fast_start.active_param == PARAM_ENABLE_TYPE_TIME)) {
	if (con->ssp.attr_state != 50) {
	    /*
	     * there is no state 50 by pass code inside ssp_detect_media_attr
	     * So we do bypass here
	     */
	    rv = ssp_detect_media_attr(con, ssp_cfg, contentLen, dataBuf, bufLen);

	    if (rv < 0 || rv == SSP_WAIT_FOR_DATA || rv == SSP_SEND_DATA_BACK) {
		goto exit;
	    }
	    if (rv == 0) {
		/*
		 * rv == 0 means that we finish the detect of media attr
		 * we should bypass ssp_detect_media_attr loop for the ssp_seek_req loop
		 */
		con->ssp.attr_state = 50;
	    }
	}
    }

    /* no token is remapped, use normal SSP code pass*/
    if (!phttp->uri_token ||
	(phttp->uri_token && !phttp->uri_token->token_flag)) {
	// Check for seek request
	if (ssp_cfg->seek.enable && ssp_cfg->seek.uri_query) {

	    rv = ssp_seek_req(con, ssp_cfg, contentLen, dataBuf, bufLen);

	    if ( rv < 0 || rv == SSP_WAIT_FOR_DATA || rv == SSP_SEND_DATA_BACK) {
		goto exit;
	    }
	}

    } else { /* have remapped token */
	/* handle with tokenization, mainly with DYN uri + Resp Cache Idx + SSP */
	if (phttp->uri_token &&
	    CHECK_UTF_FLAG(phttp->uri_token, UTF_HTTP_RANGE_START) &&
	    CHECK_UTF_FLAG(phttp->uri_token, UTF_HTTP_RANGE_END) &&
	    !CHECK_UTF_FLAG(phttp->uri_token, UTF_FLVSEEK_BYTE_START)) {
	    /*
	     * http byte range in token (for netflix now)
	     * the SEEK_URI is generated in Dyn URL
	     * we will only set the byte range value here
	     */
	    if (phttp->uri_token->http_range_end > phttp->uri_token->http_range_start) {
		http_set_seek_request(phttp, phttp->uri_token->http_range_start,
				      phttp->uri_token->http_range_end);
	    } else { /*Invalid condition, just tunnel it*/
		delete_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI);
		delete_known_header(&phttp->hdr, MIME_HDR_X_NKN_SEEK_URI);
		con->ssp.attr_state = 0;
		rv = -NKN_SSP_FORCE_TUNNEL_PATH;
		DBG_LOG(MSG, MOD_SSP, "Invalid byte range token: request is tunneled");
		goto exit;
	    }
#if 0
	} else if (!CHECK_UTF_FLAG(phttp->uri_token, UTF_HTTP_RANGE_START) &&
		   !CHECK_UTF_FLAG(phttp->uri_token, UTF_HTTP_RANGE_END) &&
		   CHECK_UTF_FLAG(phttp->uri_token, UTF_FLVSEEK_BYTE_START)) {
	    /*
	     * flv byte range start in token (for megavideo now)
	     * the SEEK_URI is generated in Dyn URL
	     * we will only set the byte range value here
	     */
	    http_set_seek_request(phttp, phttp->uri_token->flvseek_byte_start, 0);
	    /* currently megavideo only has FLV file */
	    con->ssp.ssp_container_id = CONTAINER_TYPE_FLV;
	    if (phttp->uri_token->flvseek_byte_start == 0) {
		/* start from 0 is not seek, no need to attach FLV header in updateSSP*/
		CLEAR_HTTP_FLAG(phttp, HRF_BYTE_SEEK);
	    }
#endif
	} else {
	    /* Invalid condition, just tunnel it */
	    delete_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI);
	    delete_known_header(&phttp->hdr, MIME_HDR_X_NKN_SEEK_URI);
	    con->ssp.attr_state = 0;
	    rv = -NKN_SSP_FORCE_TUNNEL_PATH;
	    goto exit;
	}
    }

 fmt_not_supp:
    con->max_bandwidth = sess_bandwidth_limit;

    // Set the fast start buffer size
    ssp_set_faststart_size(con, ssp_cfg, contentLen);

    // Throttle to Rate Control (Param for BW throttling)
    rv = ssp_set_rc(con, ssp_cfg);
    if (rv < 0) {
	return rv;
    }

    con->ssp.ssp_activated = TRUE;

 exit:
    if (rv == SSP_SEND_DATA_OTW){
	CLEAR_HTTP_FLAG(phttp, HRF_SSP_INTERNAL_REQ);
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
generic_ssp_transcode_state(con_t *con,
                            const char *uriData,
                            int uriLen,
                            const char *hostData,
                            int hostLen,
                            off_t contentLen,
			    int remapLen,
                            int transType,
                            uint64_t* threshold,
                            char transRatioData[][4],
                            int* transRatioLen)
{
    http_cb_t *phttp = &con->http;
    int rv;
    char *lowrateUri;
    int lowrateUriLen = 0;
    int transcoderv;
    /*
     * use this to decide the complexity of the transcoder
     * if it is not very time sensitive, we can use two pass
     * if it is very time sensitive, we will use one pass and fast preset
     * This function is not supported now
     */
    char transComplexData[] = "1";
    int transComplexLen = strlen(transComplexData);
    char containerType[4]="DEF"; //ramdom value

    /* the transcoding feature is on */
    if (transType & con->ssp.ssp_container_id) {
	switch(con->ssp.transcode_state){
	    case 0:
		/* Generate the name of transcoded version of this video */
		lowrateUriLen = remapLen + strlen("_lowrate") + 1;
		lowrateUri = (char *)alloca(lowrateUriLen);
		memset(lowrateUri, 0, lowrateUriLen);
		memcpy(lowrateUri, uriData,remapLen - 4);//copy the file name
		strcat(lowrateUri,"_lowrate");
		if (con->ssp.ssp_container_filetype == CONTAINER_TYPE_FLV) {
		    strcat(lowrateUri,".flv");
		}else if (con->ssp.ssp_container_filetype == CONTAINER_TYPE_MP4) {
		    strcat(lowrateUri,".mp4");
		}
		lowrateUri[lowrateUriLen - 1] = '\0';

		/* Remap the internal cache name of lowrate file to REMAPPED_URI */
		add_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI,
				 lowrateUri, strlen(lowrateUri));
		DBG_LOG(MSG, MOD_SSP, "state : %d. transcoded  name : %s",
			con->ssp.transcode_state, lowrateUri);
		SET_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
		con->ssp.transcode_state = 10;
		rv = SSP_SEND_DATA_BACK;
		break;
	    case 10:
		if (contentLen == OM_STAT_SIG_TOT_CONTENT_LEN || contentLen == 0) {
		    /* the lowrate file is not in cache */
		    add_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI, uriData, remapLen);
		    con->ssp.transcode_state = 15; // go to state 15
		    CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
		    rv = SSP_SEND_DATA_BACK;
		} else {
		    /* lowrate file is in cache, send out the lowrate video file */
		    con->ssp.transcode_state = 20;
		    rv = SSP_SEND_DATA_OTW;
		}
		break;
	    case 15:
		if (contentLen == OM_STAT_SIG_TOT_CONTENT_LEN || contentLen == 0) {
		    rv = - NKN_SSP_BAD_REMAPPED_URL;
		    /* the orginal file is not in cache */
		    DBG_LOG(MSG, MOD_SSP,
			    "state : %d. can not fetch the original file for transcode. rv = %d",
			    con->ssp.transcode_state, rv );
		} else {
		    uint64_t hotval = 0; // need to initial this value, in case of hotval not fetched
		    nkn_uol_t uol = { 0,0,0 };
		    nkn_attr_t *ap = NULL;
		    uol.cod = con->http.req_cod;
		    nkn_buffer_t *abuf = nkn_buffer_get(&uol, NKN_BUFFER_ATTR);
		    if (abuf) {
			ap = (nkn_attr_t *)nkn_buffer_getcontent(abuf);
			hotval = ap->hotval&0x00000000ffffffff;
			nkn_buffer_release(abuf);
		    }

		    if (hotval > threshold[con->ssp.ssp_container_id - 1]) {
			/* hotness larger than threashold, we will trigger the transcoder */
			if (con->ssp.ssp_container_id == CONTAINER_TYPE_FLV) {
			    strcpy(containerType, "flv");
			} else if (con->ssp.ssp_container_id == CONTAINER_TYPE_MP4 ) {
			    strcpy(containerType, "mp4");
			}

			transcoderv =
				submit_vpemgr_generic_transcode_request( \
							hostData, hostLen, \
							uriData, uriLen, \
							containerType, strlen(containerType), \
							transRatioData[con->ssp.ssp_container_id - 1], \
							transRatioLen[con->ssp.ssp_container_id - 1], \
							transComplexData, transComplexLen);
			if (transcoderv) {
			    transcoderv = -NKN_SSP_PYTHON_PLUGIN_FAILURE;
			    DBG_LOG(WARNING,MOD_SSP,
				    "state : %d. Request to trigger transcode fialed. rv = %d",
				    con->ssp.transcode_state, transcoderv);
			}
		    } /* if (hotval>threashold) */
		    con->ssp.transcode_state = 20;
		    rv = transcoderv;
		} /* if (contentLen == OM_STAT_SIG_TOT_CONTENT_LEN || contentLen == 0) */
		break;
	    case 20:
		/* Just serve the video in REMAPPED_URI */
		rv = SSP_SEND_DATA_OTW;
		break;
	    default:
		/* Just serve the original video */
		DBG_LOG(WARNING, MOD_SSP,
			"Transcoding (State: Default): Not supported state = %d",
			con->ssp.seek_state);
		add_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI,
				 uriData, remapLen);
		rv = SSP_SEND_DATA_OTW;
		break;
	} /* switch(con->ssp.transcode_state){ */
    } else {
	/*
	 * this type video file should not transcode according to CLI
	 * Map the original video to REMAPPED_URI
	 */
	add_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI,
			 uriData, remapLen);
	rv = SSP_SEND_DATA_OTW;
    } /* if (transType & con->ssp.ssp_container_id) */
    return rv;

}
