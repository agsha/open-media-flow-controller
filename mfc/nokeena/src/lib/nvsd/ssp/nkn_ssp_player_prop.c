/*
 *******************************************************************************
 * nkn_ssp_player_prop.c -- SSP Properties
 *******************************************************************************
 */

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <openssl/md5.h>

#include "nkn_common_config.h"
#include "http_header.h"
#include "ssp.h"
#include "nkn_errno.h"
#include "nkn_debug.h"
#include "nkn_cfg_params.h"
#include "nkn_ssp_player_prop.h"
#include "ssp_authentication.h"
#include "ssp_queryparams.h"
#include "nkn_vpe_mp4_parser.h"
#include "nkn_vpe_mp4_seek_api.h"
#include "nkn_vpe_bitio.h"
#include "nkn_vpe_webm_parser.h"
#include "nkn_vpe_asf_parser.h"

#include "nkn_vpe_media_props_api.h"
#include "nkn_vpe_types.h"
//#define HDR_DATA_SIZE 32768  // It should go to nkn_vpe_media_props_api.h

#define TSTR(_p, _l, _r)  char *(_r); ( 1 ? ( (_r) = alloca((_l)+1), memcpy((_r), (_p), (_l)), (_r)[(_l)] = '\0' ) : 0 )
extern unsigned long long glob_ssp_hash_failures;
extern unsigned long long glob_ssp_container_flv, glob_ssp_container_mp4, glob_ssp_container_asf,
    glob_ssp_container_webm, glob_ssp_container_3gpp, glob_ssp_container_3gp2;
extern unsigned long long glob_ssp_pacing_flv_failure, glob_ssp_pacing_flv_success,
    glob_ssp_pacing_mp4_failure, glob_ssp_pacing_mp4_success,
    glob_ssp_pacing_asf_failure, glob_ssp_pacing_asf_success,
    glob_ssp_pacing_webm_failure, glob_ssp_pacing_webm_success,
    glob_ssp_pacing_3gpp_failure, glob_ssp_pacing_3gpp_success,
    glob_ssp_pacing_3gp2_failure, glob_ssp_pacing_3gp2_success;




static int32_t vpe_check_container_flv(uint8_t *data);
//static int32_t vpe_check_container_mp4(uint8_t *data);
static int32_t vpe_check_container_webm(uint8_t *data);
static int32_t vpe_check_container_asf(uint8_t *data);
static int32_t vpe_check_container_3gpp(uint8_t *data);
static int32_t vpe_check_container_3gp2(uint8_t *data);
static int32_t vpe_check_container_iso_media(uint8_t *data);

// Verify the hash for video URL. If successful then proceed, else reject connection
int
ssp_auth_md5hash_req(con_t *con, const ssp_config_t *ssp_cfg,
		 const char *uriData, int uriLen,
		 const char *hostData, int hostLen)
{
    UNUSED_ARGUMENT(uriLen);

    int rv=0;
    http_cb_t *phttp = &con->http;

    if (ssp_cfg->hash_verify.enable){
	rv = verifyMD5Hash(hostData, uriData, hostLen, &phttp->hdr, ssp_cfg->hash_verify);
	if (rv != 0){
	    // hashes dont match
	    glob_ssp_hash_failures++;
	    DBG_LOG(WARNING, MOD_SSP,
		    "Hash Verification Failed. Closing connection (Error: %d)", rv );

	    if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
		DBG_TRACE("Hash authentication failed for virtual player: %s",
			  ssp_cfg->player_name);
	    }
	}
	else {
	    con->ssp.ssp_hash_authed = 1;
	    if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
		DBG_TRACE("Hash authentication sucsessful for virtual player: %s",
			  ssp_cfg->player_name);
	    }
	} // if (verify
    } //if (ssp_cfg

    return rv;
}

/*
 * Set the SSP transcode parameters from CLI
 *
 */

int ssp_set_transcode(const ssp_config_t *ssp_cfg,
		      int* transType,
		      uint64_t* threshold,
		      char transRatioData[][4],
		      int* transRatioLen)
{
    int rv = 0;
    /* config transscoding parameters */
    for (int i = 0; i < MAX_SSP_BW_FILE_TYPES; i++) {
	/* config FLV */
	if (ssp_cfg->bandwidth_opt.info[i].file_type != NULL &&
	    (strncmp(ssp_cfg->bandwidth_opt.info[i].file_type, "FLV", 3) == 0 ||
	     strncmp(ssp_cfg->bandwidth_opt.info[i].file_type, "flv", 3) == 0)) {
	    /* enable FLV transcoding */
	    *transType = *transType | CONTAINER_TYPE_FLV;
	    /* config transRatio for FLV */
	    if (ssp_cfg->bandwidth_opt.info[i].transcode_comp_ratio != NULL &&
		transRatioData[CONTAINER_TYPE_FLV - 1] != NULL) {
		if (strncmp \
		    (ssp_cfg->bandwidth_opt.info[i].transcode_comp_ratio, \
		     "low", 3) == 0) {
		    strcpy(transRatioData[CONTAINER_TYPE_FLV - 1], "85");
		} else if (strncmp \
			   (ssp_cfg->bandwidth_opt.info[i].transcode_comp_ratio, \
                            "med", 3) == 0) {
		    strcpy(transRatioData[CONTAINER_TYPE_FLV - 1], "65");
		} else if (strncmp \
			   (ssp_cfg->bandwidth_opt.info[i].transcode_comp_ratio, \
                            "high", 4) == 0) {
		    strcpy(transRatioData[CONTAINER_TYPE_FLV - 1], "50");
		} else { /* handle abnormal condition */
		    *transType = *transType & (~(CONTAINER_TYPE_FLV));
		}
		transRatioLen[CONTAINER_TYPE_FLV - 1] =
		    strlen(transRatioData[CONTAINER_TYPE_FLV - 1]);
                threshold[CONTAINER_TYPE_FLV-1] = \
                    ssp_cfg->bandwidth_opt.info[i].hotness_threshold;

	    } else { /* handle abnormal condition */
		*transType = *transType & (~(CONTAINER_TYPE_FLV));
	    }
	}

	/* config MP4 */
	if (ssp_cfg->bandwidth_opt.info[i].file_type != NULL &&
	    (strncmp(ssp_cfg->bandwidth_opt.info[i].file_type, "MP4", 3) == 0 ||
	     strncmp(ssp_cfg->bandwidth_opt.info[i].file_type, "mp4", 3) == 0)) {
	    /* enable MP4 transcoding */
	    *transType = *transType | CONTAINER_TYPE_MP4;
	    /* config transRatio for MP4 */
	    if (ssp_cfg->bandwidth_opt.info[i].transcode_comp_ratio != NULL &&
		transRatioData[CONTAINER_TYPE_MP4 - 1] != NULL) {
		if (strncmp \
		    (ssp_cfg->bandwidth_opt.info[i].transcode_comp_ratio, \
		     "low", 3) == 0) {
		    strcpy(transRatioData[CONTAINER_TYPE_MP4 - 1], "85");
		} else if (strncmp \
			   (ssp_cfg->bandwidth_opt.info[i].transcode_comp_ratio, \
                            "med", 3) == 0) {
		    strcpy(transRatioData[CONTAINER_TYPE_MP4 - 1], "65");
		} else if (strncmp \
			   (ssp_cfg->bandwidth_opt.info[i].transcode_comp_ratio, \
                            "high", 4) == 0) {
		    strcpy(transRatioData[CONTAINER_TYPE_MP4 - 1], "50");
		} else { /* handle abnormal condition */
		    *transType = *transType & (~(CONTAINER_TYPE_MP4));
		}
                transRatioLen[CONTAINER_TYPE_MP4 - 1] = \
                    strlen(transRatioData[CONTAINER_TYPE_MP4 - 1]);
                threshold[CONTAINER_TYPE_MP4 - 1] = \
                    ssp_cfg->bandwidth_opt.info[i].hotness_threshold;

	    } else { /* handle abnormal condition */
		/* disable MP4 transcode */
		*transType = *transType & (~(CONTAINER_TYPE_MP4));
	    }
	}
    } /* for (int i = 0; i < MAX_SSP_BW_FILE_TYPES; i++) */

    return rv;
}

// Set fast start buffer size
int
ssp_set_faststart_size(con_t *con, const ssp_config_t *ssp_cfg, off_t contentLen)
{
    int initBufSize=0;
    int rv=0;

    if (ssp_cfg->fast_start.enable) {
        switch (ssp_cfg->fast_start.active_param) {
            case PARAM_ENABLE_TYPE_QUERY:
                if (ssp_cfg->fast_start.uri_query) {
                    initBufSize = query_fast_start_size(con, ssp_cfg->fast_start.uri_query);
                }
                break;
            case PARAM_ENABLE_TYPE_SIZE:
                if (ssp_cfg->fast_start.size > 0) {
                    initBufSize = ssp_cfg->fast_start.size; //Bytes
                }
                break;
            case PARAM_ENABLE_TYPE_TIME:
                if (ssp_cfg->fast_start.time > 0) { // Time takes third preference
		    /* kbps -> BYTES*/
		    initBufSize = ssp_cfg->fast_start.time * con->ssp.ssp_bitrate * KBYTES/8;
		    /* limit by the video file size */
		    initBufSize = (initBufSize < contentLen ? initBufSize : contentLen);
                }
                break;
            default:
                break;
        } // switch

        if (initBufSize >= 0) {
            // Set the initial client buffer size for fast start
            con->max_faststart_buf = initBufSize;
            DBG_LOG(MSG, MOD_SSP, "Fast start buffer set to %ld bytes", con->max_faststart_buf );
            if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
                DBG_TRACE("Fast start buffer size set to %ld bytes" , con->max_faststart_buf);
            }
	    rv = 0;
        }
        else if (initBufSize < 0) {
            DBG_LOG(MSG, MOD_SSP, "Fast start not enforced %d", initBufSize);
            if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
                DBG_TRACE("Fast start not enforced %d", initBufSize);
            }
            rv = initBufSize;
        }

    } //if (ssp_cfg

    return rv;
}

#if 0 // set_afr is replaced by set_rc
// Set Assured Flow Rate
int
ssp_set_afr(con_t *con, const ssp_config_t *ssp_cfg)
{
    int64_t steadyRate=0;
    int rv=0;
    float arf_overhead = 0;
    if (ssp_cfg->assured_flow.enable) {
        switch (ssp_cfg->assured_flow.active_param) {
            case PARAM_ENABLE_TYPE_QUERY:
                if (ssp_cfg->assured_flow.uri_query) {
                    steadyRate = query_throttle_to_afr(con, ssp_cfg->assured_flow.uri_query); //bytes/sec
                }
                break;

            case PARAM_ENABLE_TYPE_RATE:
                if(ssp_cfg->assured_flow.rate > 0) {
                    steadyRate = ssp_cfg->assured_flow.rate;
                }
                break;

            case PARAM_ENABLE_TYPE_AUTO:
		if (ssp_cfg->assured_flow.overhead == NULL) {
		    /* 1.25 is the overhead */
		    arf_overhead = 1.25 / 1.2; /* 1.2 is the TCP/IP over head*/
		} else {
		    arf_overhead = atof(ssp_cfg->assured_flow.overhead);
		    if (arf_overhead >=1.25 && arf_overhead <=2) {/* valid range*/
			arf_overhead = arf_overhead / 1.2;
		    } else {
			arf_overhead = 1.25 / 1.2;
		    }
		}
		if (ssp_cfg->assured_flow.use_mbr == 1 /*mbr*/) {
		    if (con->ssp.ssp_bitrate != 0) {
			steadyRate = con->ssp.ssp_bitrate * 1000/8; /*kbps ->Bps*/
			steadyRate = (uint64_t)(arf_overhead * (float)steadyRate);

			if (ssp_cfg->con_max_bandwidth > 0) {
			    con->max_bandwidth = ssp_cfg->con_max_bandwidth < (uint64_t)steadyRate ? \
				ssp_cfg->con_max_bandwidth : (uint64_t)steadyRate;
			} else {
			    con->max_bandwidth = con->max_client_bw < (uint64_t)steadyRate ? \
				con->max_client_bw : (uint64_t)steadyRate;
			}
			con->max_bandwidth = (uint64_t)(1.2 * (float)con->max_bandwidth);
			DBG_LOG(MSG, MOD_SSP,
				"use_mbr for assure flow: con->max_bandwidth = %llu",
				con->max_bandwidth);
			return 0;
		    } else {
			steadyRate = 0;
		    }
		} else { /* afr */
		    DBG_LOG(MSG, MOD_SSP, "no mbr for assure flow");
		    if (con->ssp.ssp_bitrate != 0) {
			steadyRate = con->ssp.ssp_bitrate * 1000/8; /*kbps ->Bps*/
			/* rate fatcor 1.25 decomposite to 1.05*1.2, 1.2 is in setAFRRate */
			steadyRate = (uint64_t)(arf_overhead * (float)steadyRate);
		    } else {
			steadyRate = 0;
		    }
		}
                break;

            case PARAM_ENABLE_TYPE_SIZE:
            case PARAM_ENABLE_TYPE_TIME:
                break;

        } // switch(

        rv = setAFRate(con, ssp_cfg, steadyRate);
        if (rv < 0)
            return rv;
    } else {
	// AFR is disabled, so if CMB is specified set AFR to this value
        if (ssp_cfg->con_max_bandwidth > 0) {
            con->max_bandwidth = ssp_cfg->con_max_bandwidth;
        }
	rv = 0;
    }

    return rv;
}

#else
/* Set rate control */
#define MIN_RC(a,b) (((a)<(b))?(a):(b))
int
ssp_set_rc(con_t *con, const ssp_config_t *ssp_cfg) {
    int64_t steadyRate=0;
    int rv=0;
    if (ssp_cfg->rate_control.active) {
        switch (ssp_cfg->rate_control.active_param) {
            case PARAM_ENABLE_TYPE_QUERY:
                if (ssp_cfg->rate_control.qstr) {
		    /* read the value from query */
                    steadyRate = query_throttle_to_rc(con, ssp_cfg->rate_control.qstr); //bytes/sec

		    /* multiply the rate_unit to convert to bytes/sec*/
		    steadyRate = steadyRate * ssp_cfg->rate_control.qrate_unit;

		    /* multiply the burst_factor*/
		    steadyRate = (uint64_t) (ssp_cfg->rate_control.burst_factor * (float)steadyRate);
                }
                break;

            case PARAM_ENABLE_TYPE_RATE:
		if(ssp_cfg->rate_control.static_rate > 0) {
                    steadyRate = ssp_cfg->rate_control.static_rate * KBYTES/8;
                }

		steadyRate = (uint64_t) (ssp_cfg->rate_control.burst_factor* (float)steadyRate);
		break;

            case PARAM_ENABLE_TYPE_AUTO:

		if (con->ssp.ssp_bitrate > 0) { /* has a detected bitrate for video file*/
		    steadyRate = con->ssp.ssp_bitrate * KBYTES/8; /*kbps ->Bps*/
		    steadyRate = (uint64_t) (ssp_cfg->rate_control.burst_factor* (float)steadyRate);
		    //pacing counter
		    switch (con->ssp.ssp_container_id) {
			case CONTAINER_TYPE_FLV:
			    glob_ssp_pacing_flv_success++;
			    break;
			case CONTAINER_TYPE_MP4:
			    glob_ssp_pacing_mp4_success++;
			    break;
			case CONTAINER_TYPE_WEBM:
			    glob_ssp_pacing_webm_success++;
			    break;
			case CONTAINER_TYPE_ASF:
			    glob_ssp_pacing_asf_success++;
			    break;
			case CONTAINER_TYPE_3GPP:
			    glob_ssp_pacing_3gpp_success++;
			    break;
			case CONTAINER_TYPE_3GP2:
			    glob_ssp_pacing_3gp2_success++;
			    break;
		    }
		} else {
		    steadyRate = 0;
		    //pacing counter
                    switch (con->ssp.ssp_container_id) {
                        case CONTAINER_TYPE_FLV:
                            glob_ssp_pacing_flv_failure++;
                            break;
                        case CONTAINER_TYPE_MP4:
                            glob_ssp_pacing_mp4_failure++;
                            break;
			case CONTAINER_TYPE_WEBM:
                            glob_ssp_pacing_webm_failure++;
                            break;
                        case CONTAINER_TYPE_ASF:
                            glob_ssp_pacing_asf_failure++;
                            break;
			case CONTAINER_TYPE_3GPP:
                            glob_ssp_pacing_3gpp_failure++;
                            break;
			case CONTAINER_TYPE_3GP2:
                            glob_ssp_pacing_3gp2_failure++;
                            break;
                    }
		}
                break;

            case PARAM_ENABLE_TYPE_SIZE:
            case PARAM_ENABLE_TYPE_TIME:
                break;

        } // switch(

	if (ssp_cfg->rate_control.scheme == VP_RC_MBR /* MBR */) {
	    if (steadyRate > 0) {
		/* multipy the TCP/IP overhead 1.2 */
		steadyRate = (int64_t) (1.2 * (float)steadyRate);
		if (ssp_cfg->con_max_bandwidth > 0) {
		    con->max_bandwidth = MIN_RC(ssp_cfg->con_max_bandwidth, (uint64_t)steadyRate);
		} else {
		    con->max_bandwidth = MIN_RC(con->max_client_bw, (uint64_t)steadyRate);
		}
	    } else { /* fail to find the steadyRate */
		if (ssp_cfg->con_max_bandwidth > 0) {
		    con->max_bandwidth = ssp_cfg->con_max_bandwidth;
		} else if (sess_bandwidth_limit > 0){
		    con->max_bandwidth = con->max_client_bw;
		}
	    }

	    DBG_LOG(MSG, MOD_SSP,
		    "use_mbr for rate control: con->max_bandwidth = %lu",
		    con->max_bandwidth);
	    rv = 0;

	} else { /* AFR */
	    DBG_LOG(MSG, MOD_SSP, "use afr for rate control");
	    rv = setAFRate(con, ssp_cfg, steadyRate);
	    if (rv < 0) {
		return rv;
	    }
	}
    } else {
        // Rate Control is disabled, so if CMB is specified, set to this value
        if (ssp_cfg->con_max_bandwidth > 0) {
            con->max_bandwidth = ssp_cfg->con_max_bandwidth;
        }
        rv = 0;
    }

    return rv;
}
#undef MIN_RC

#endif


// Check & enforce if full file download is requested
int
ssp_full_download_req(con_t *con, const ssp_config_t *ssp_cfg)
{
    int rv = -1, queryrv;

    if (ssp_cfg->full_download.always) {
	setDownloadRate(con, ssp_cfg);
	rv = SSP_SEND_DATA_OTW;
	goto exit;
    }
    else {
	if (ssp_cfg->full_download.uri_query &&
	    strncmp(ssp_cfg->full_download.uri_query, "", 1) != 0 && ssp_cfg->full_download.matchstr) { // URI QUERY
	    queryrv = query_full_download(con, ssp_cfg->full_download.uri_query,
					  ssp_cfg->full_download.matchstr);
	    if (!queryrv) {
		setDownloadRate(con, ssp_cfg);
		rv = SSP_SEND_DATA_OTW;
		goto exit;
	    }
	    else{
		DBG_LOG(MSG, MOD_SSP,
			"Download enabled, but query param is missing"
			"Will enforce afr instead if specified");
	    }
	} // uri_query
	else if (ssp_cfg->full_download.uri_hdr && ssp_cfg->full_download.matchstr){ // URI HEADER
	    queryrv = header_full_download(con, ssp_cfg->full_download.uri_hdr,
					   ssp_cfg->full_download.matchstr);
	    if (!queryrv) {
		setDownloadRate(con, ssp_cfg);
		rv = SSP_SEND_DATA_OTW;
		goto exit;
	    }
	    else{
		DBG_LOG(MSG, MOD_SSP,
			"Download enabled, but uri header param is missing"
			"Will enforce afr instead if specified");
	    }
	} // uri_header
    }

 exit:
    return rv;
}


//////////////////////////////////////////////////////////////
int32_t
vpe_detect_media_type(uint8_t *data)
{
    if (vpe_check_container_flv(data)) {
	DBG_LOG(MSG, MOD_SSP,
                    "ssp_mprops_detect: FLV container");
	return CONTAINER_TYPE_FLV;
    }

    if(vpe_check_container_iso_media(data)) {
	//detect whether it is 3GPP
	if (vpe_check_container_3gpp(data)) {
	    DBG_LOG(MSG, MOD_SSP,
		    "ssp_mprops_detect: 3GPP container");
	    return CONTAINER_TYPE_3GPP;
	}
	//detect whether it is 3GP2
        if (vpe_check_container_3gp2(data)) {
            DBG_LOG(MSG, MOD_SSP,
                    "ssp_mprops_detect: 3GP2 container");
            return CONTAINER_TYPE_3GP2;
	}

	//all the rest are MP4
	DBG_LOG(MSG, MOD_SSP,
		"ssp_mprops_detect: MP4 container");
	return CONTAINER_TYPE_MP4;
    }

    if(vpe_check_container_webm(data)) {
	DBG_LOG(MSG, MOD_SSP,
                "ssp_mprops_detect: WebM container");
	return CONTAINER_TYPE_WEBM;
    }

    if (vpe_check_container_asf(data)) {
        DBG_LOG(MSG, MOD_SSP,
                "ssp_mprops_detect: ASF container");
        return CONTAINER_TYPE_ASF;
    }


    DBG_LOG(WARNING, MOD_SSP,
	    "container frmt not support, SSP_SEND_DATA_OTW");
    return  NKN_SSP_CONTAINER_FRMT_NOT_SUPP;
}

static int32_t
vpe_check_container_flv(uint8_t *data)
{
  char signature[] = {'F','L','V'};
  return (!memcmp(data, signature, sizeof(signature)));
}

static int32_t
vpe_check_container_iso_media(uint8_t *data)
{
 char signature[2][4] = { {'f', 't', 'y', 'p'},
                           {'m', 'd', 'a', 't'} };

  if( !memcmp(data + 4, signature[0], 4) ||
      !memcmp(data + 4, signature[1], 4) ) {
    return 1;
  }

  return 0;
}

static int32_t
vpe_check_container_3gpp(uint8_t *data)
{
    int i;
    //http://www.etsi.org/deliver/etsi_ts/126200_126299/126244/10.00.00_60/ts_126244v100000p.pdf
    char brand[9][4] = { {'3', 'g', 'p', '4'},
			 {'3', 'g', 'p', '5'},
			 {'3', 'g', 'p', '6'},
			 {'3', 'g', 'p', '7'},
			 {'3', 'g', 'p', '8'},
			 {'3', 'g', 'r', '6'},
			 {'3', 'g', 's', '6'},
			 {'3', 'g', 'e', '7'},
			 {'3', 'g', 'g', '6'} };
			// 3gt8 3gt9 are RTP streaming, 3gh9 are http live streaming
    for (i = 0; i < 9; i ++) {
	if (!memcmp(data + 8, brand[i], 4)) {
	    return 1;
	}
    }
    return 0;
}

static int32_t
vpe_check_container_3gp2(uint8_t *data)
{
    int i;
    //http://www.3gpp2.org/Public_html/specs/C.S0050-B_v1.0_070521.pdf
    char brand[3][4] = { {'3', 'g', '2', 'a'},
		       {'3', 'g', '2', 'b'},
		       {'3', 'g', '2', 'c'} };

    for (i = 0; i < 3; i ++) {
	if (!memcmp(data + 8, brand[i], 4)) {
	    return 1;
	}
    }
	return 0;

}

static int32_t
vpe_check_container_webm(uint8_t* data)
{
    uint64_t webm_head_size;
    int rv;
    rv = nkn_vpe_webm_head_parser(data, &webm_head_size);
    if (rv == 0) {
	return 1;
    }
    return 0;
}

static int32_t
vpe_check_container_asf(uint8_t* data)
{
    int rv;
    rv = nkn_vpe_asf_header_guid_check(data);
    if (rv == 0) {
	return 1;
    }
    return 0;
}

#if 1
/*
 * This function can only fetch the container type
 * This function is used to replaced the old ssp_fetch_media_attr()
 */
int
ssp_detect_media_type(con_t *con,
		      const ssp_config_t *ssp_cfg,
		     off_t contentLen,
		      const char *dataBuf,
		      int bufLen)
{
    const char *querySeekTmp=NULL;
    int seekLen=0;
    int rv = 0;

    http_cb_t *phttp = &con->http;

    if (con->ssp.ssp_container_filetype == 0) {
	/* find the seek_query */
	if (ssp_cfg->seek.enable && ssp_cfg->seek.uri_query) {
	    get_query_arg_by_name(&phttp->hdr, ssp_cfg->seek.uri_query,
				  strlen(ssp_cfg->seek.uri_query), &querySeekTmp, &seekLen);
	}
	TSTR(querySeekTmp, seekLen, querySeek);
	if ( strncmp(querySeek, "", 1) == 0 ) {
	    querySeek = NULL;
	}

	if (contentLen == OM_STAT_SIG_TOT_CONTENT_LEN &&
	    !CHECK_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH)) { // File not found
	    DBG_LOG(WARNING, MOD_SSP,
		    "ssp_detect_media_type: "
		    "File not found, re-try and forward to client w/o seeking");
	    if (CHECK_HTTP_FLAG(phttp, HRF_BYTE_RANGE)) {
		CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
                phttp->brstart = 0;
                phttp->brstop  = 0;
	    }

	    /* if client request has byte range request, restore it */
	    if (con->ssp.ssp_video_br_flag) {
		phttp->brstart = con->ssp.client_brstart;
	        phttp->brstop  = con->ssp.client_brstop;
                SET_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
		con->ssp.client_brstart = 0;
                con->ssp.client_brstop  = 0;
	    }
	    rv = -NKN_SSP_FORCE_TUNNEL_PATH;
	    goto exit;
	} else if (contentLen == OM_STAT_SIG_TOT_CONTENT_LEN &&
		   CHECK_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH)) { //File not in cache
 	    DBG_LOG(WARNING, MOD_SSP,
		    "ssp_detect_media_type: "
		    "File is not in cache, tunnel it");
	    delete_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI);
	    delete_known_header(&phttp->hdr, MIME_HDR_X_NKN_SEEK_URI);
	    CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
	    if (CHECK_HTTP_FLAG(phttp, HRF_BYTE_RANGE)) {
		CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
                phttp->brstart = 0;
                phttp->brstop  = 0;
	    }

            /* if client request has byte range request, restore it */
            if (con->ssp.ssp_video_br_flag) {
                phttp->brstart = con->ssp.client_brstart;
                phttp->brstop  = con->ssp.client_brstop;
                SET_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
                con->ssp.client_brstart = 0;
                con->ssp.client_brstop  = 0;
            }
	    rv = -NKN_SSP_FORCE_TUNNEL_PATH;
	    goto exit;
	} else if (contentLen==0) { // first entry this loop
	    if (querySeek == NULL) {
		/* no seek query */
		CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
		DBG_LOG(MSG, MOD_SSP,
                        "ssp_detect_media_type: "
                        "Request first 32KB for no seek query");
	    } else {
		if (atoi(querySeek) == 0) {
		    /* seek query == 0 */
		    CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
		    DBG_LOG(MSG, MOD_SSP,
			    "ssp_detect_media_type: "
			    "Request first 32KB for seek query == 0");
		} else {
		    /*
		     * seek query != 0, still allow to fetch from Origin
		     */
		    CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
		    //SET_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
		    DBG_LOG(MSG, MOD_SSP,
			    "ssp_detect_media_type: "
			    "Request first 32KB for the seek query");
		}
		// Create the SEEK URI for OM
		generate_om_seek_uri(con, ssp_cfg->seek.uri_query);
	    }

	    /* to handle client request with byterange */
	    con->ssp.ssp_video_br_flag = 0;
	    if (CHECK_HTTP_FLAG(phttp, HRF_BYTE_RANGE)) {
		/* store the client request byterange in http header */
		con->ssp.ssp_video_br_flag = 1;
		con->ssp.client_brstart =  phttp->brstart;
		con->ssp.client_brstop  =  phttp->brstop;
		// Clear any byte range requests sent by client
		CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
		phttp->brstart = 0;
		phttp->brstop = 0;
	    }
	    rv = SSP_SEND_DATA_BACK;
	    goto exit;

	} else {
	    uint8_t* p_file_buf;
            p_file_buf = (uint8_t*)dataBuf;
	    /*
	     * contentLen!=0 and contentLen!=OM_STAT_SIG_TOT_CONTENT_LEN
	     * now we may have the first 32KB in hand
	     */
	    if (bufLen > 16) {
		// 16 is ASF guid length, but we have not conside the webM situation
		con->ssp.ssp_container_filetype = vpe_detect_media_type(p_file_buf);
	    }
	    switch (con->ssp.ssp_container_filetype) {
		case CONTAINER_TYPE_FLV:
		    con->ssp.ssp_container_id = CONTAINER_TYPE_FLV;
		    glob_ssp_container_flv++;
		    break;
		case CONTAINER_TYPE_MP4:
		    con->ssp.ssp_container_id = CONTAINER_TYPE_MP4;
		    glob_ssp_container_mp4++;
		    break;
		case CONTAINER_TYPE_ASF:
		    con->ssp.ssp_container_id = CONTAINER_TYPE_ASF;
		    glob_ssp_container_asf++;
		    break;
		case CONTAINER_TYPE_WEBM:
		    con->ssp.ssp_container_id = CONTAINER_TYPE_WEBM;
		    glob_ssp_container_webm++;
		    break;
		case CONTAINER_TYPE_3GPP:
		    con->ssp.ssp_container_id = CONTAINER_TYPE_3GPP;
		    glob_ssp_container_3gpp++;
		    break;
		case CONTAINER_TYPE_3GP2:
		    con->ssp.ssp_container_id = CONTAINER_TYPE_3GP2;
		    glob_ssp_container_3gp2++;
		    break;
		case  NKN_SSP_CONTAINER_FRMT_NOT_SUPP:
		default:
		    break;
	    }
	    if (con->ssp.ssp_container_filetype ==
		NKN_SSP_CONTAINER_FRMT_NOT_SUPP) {
		/* file type is not MP4, FLV, ASF, or WebM */
		DBG_LOG(WARNING, MOD_SSP,
		"ssp_detect_media_type: the file type is not supported");

		con->ssp.ssp_container_filetype = 0;
	    }

	    /* restore the byterange request */
	    if (con->ssp.ssp_video_br_flag) {
		phttp->brstart = con->ssp.client_brstart;
		phttp->brstop  = con->ssp.client_brstop;
		SET_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
		con->ssp.client_brstart = 0;
		con->ssp.client_brstop  = 0;
	    } else { /* client request not using byterange request */
		/* HRF_BYTE_RANGE flag should not set here */
		if (CHECK_HTTP_FLAG(phttp, HRF_BYTE_RANGE)) {
		    CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
		}
		phttp->brstart = 0 ;
		phttp->brstop  = 0;
	    }

	    rv = SSP_SEND_DATA_OTW;
	    goto exit;
	}
    }
 exit:
  return rv;
}

#else

/*
 * This function can only fetch the container type
 * Maybe need to change the name for better understanding
 */
int
ssp_fetch_media_attr(con_t *con, const ssp_config_t *ssp_cfg,
		     off_t contentLen, const char *dataBuf, int bufLen)
{
  const char *querySeekTmp=NULL;
  int seekLen=0;
  int rv = -1;

  http_cb_t *phttp = &con->http;

  if (ssp_cfg->seek.enable && ssp_cfg->seek.uri_query) {
    get_query_arg_by_name(&phttp->hdr, ssp_cfg->seek.uri_query, 
			  strlen(ssp_cfg->seek.uri_query), &querySeekTmp, &seekLen);
  }
  TSTR(querySeekTmp, seekLen, querySeek);
  if ( strncmp(querySeek, "", 1) == 0 ) {
      querySeek = NULL;
  }
  //add a module to read the first 32k bytes of this file to check file type
  //we will use nkn_vpe_media_props_api
  if (con->ssp.ssp_container_filetype == 0) {

    if (contentLen == OM_STAT_SIG_TOT_CONTENT_LEN && 
	!CHECK_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH)) { // File not found

      DBG_LOG(WARNING, MOD_SSP, 
	      "Seek: (2) File not found, re-try and forward to client w/o seeking");

      if (CHECK_HTTP_FLAG(phttp, HRF_BYTE_RANGE)) {
	CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
	phttp->brstart = 0;
	phttp->brstop = 0;
      }
      rv = SSP_DETECT_MEDIA_ATTR_FAIL;
      goto exit;

    } else if ( contentLen==0 ) {
      if ( querySeek != NULL ) {  
	// if no  querySeek, no need to generate the uri for full file
	const char *queryData;
	int queryLen=0;
	int queryNameLen=0;
	queryData = NULL;
	if (ssp_cfg->seek.uri_query) {
	    queryNameLen = strlen(ssp_cfg->seek.uri_query);
	}
	get_query_arg_by_name(&phttp->hdr, ssp_cfg->seek.uri_query, queryNameLen, &queryData, &queryLen);
	TSTR(queryData, queryLen, queryDataTmp);
	if ( strncmp(queryDataTmp, "", 1) == 0 ) {
            queryDataTmp = NULL;
        }
	if ( queryDataTmp ) { //Seek query was found
	  // Create the SEEK URI for OM
	  generate_om_seek_uri(con, ssp_cfg->seek.uri_query);

	  if ( CHECK_HTTP_FLAG(phttp, HRF_BYTE_RANGE) ) {
	    // Clear any byte range requests sent by flash player
	    CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
	    phttp->brstart = 0;
	    phttp->brstop = 0;
	  }
	  DBG_LOG(MSG, MOD_SSP, 
		  "FLVSeek: (1) Request to fetch file internally to parse to seek offset");	
	} // if (queryDataTmp
      } //if ( querySeek != NULL 

      rv = SSP_SEND_DATA_BACK;
      goto exit;

    } else { //contentLen!=0 and contentLen!=OM_STAT_SIG_TOT_CONTENT_LEN

      //now read 8  bytes and decide the file type
      uint8_t* pfileType;
      bufLen = bufLen; // just to avoid a warning
      pfileType = (uint8_t*)alloca(8);
      memcpy(pfileType,(uint8_t*)dataBuf,8);
      con->ssp.ssp_container_filetype=vpe_detect_media_type(pfileType);
      if ( con->ssp.ssp_container_filetype==NKN_SSP_CONTAINER_FRMT_NOT_SUPP ) {
	//Cannot do seek on this filetype
	DBG_LOG(WARNING, MOD_SSP, 
		"SSP container frmt not support, SSP_SEND_DATA_OTW");
	con->ssp.ssp_container_filetype = 0;
	rv = SSP_SEND_DATA_OTW;
	goto exit;
      }

    }
  }
}
#endif


/*
 * detect the video file attribute
 * 1. container type (This is used for file extension. It has the same
 * function of ssp_detect_media_type())
 * 2. DataRate of the video file (This is used for AFR)
 * 3. codec type of video and audio (no use now)
 *
 */
#define N_BOX_TO_FIND 3

int
ssp_detect_media_attr(con_t *con, const ssp_config_t *ssp_cfg,
		      off_t contentLen, const char *dataBuf, int bufLen)
{
    const char *querySeekTmp = NULL;
    bitstream_state_t *bs;
    int seekLen = 0;
    int rv = 0;
    int fc_rv; // function call return value
    //    uint64_t moov_offset = 0, moov_size = 0;
    //    uint64_t webm_trak_offset = 0, webm_trak_size = 0;
    http_cb_t *phttp = &con->http;
    vpe_ol_t ol;
    int64_t bytesWritten;
    vpe_media_detector_ctx_t      *p_md;
    vpe_media_props_t             *p_mp;
    nkn_vpe_mp4_find_box_parser_t *fp;
    const char* find_box_name_list[N_BOX_TO_FIND] = {"moov", "mdat", "ftyp"};
    int n_box_to_find = N_BOX_TO_FIND;
    uint8_t *tmp_p;
    off_t brstart, brstop;
    brstart = brstop = -1;

    bs   = NULL;
    p_md = NULL;
    p_mp = NULL;
    fp   = NULL;
    ol.offset = 0; /* need to initial the value in case of paser failed */
    ol.length = 0;

    /* Read first 32KB of the file to detect container format
     * For MP4
     *     1. Find offset, size of ftyp, moov and mdat box.
     *     2. Read ftyp box into buffer, then read
     *        the entire moov box into buffer
     *     3. Once we have the entrie moov box, we can detect the props
     * For WebM
     *     1. Find the offset and size of Segment Information and Tracks
     *        If the last bytes of Segment Information or Tracks is
     *        larger than 32KB, we will SSP_WAIT_FOR_DATA to fetch more
     *        data and make sure to obtain the entire Segment Info and Tracks
     *     2. Once we have entire Segment Info and Tracks, we can detect
     *        the props
     * For FLV
     *     currently, it can only parser onmetada tag within 32KB
     *     And find the "duration" in onmetadata tag
     *     Need extention to handle more situation
     * For ASF/WMV
     *     Read the max_bitrate from header object
     */

    if (con->ssp.attr_state == 0 /*&& contentLen == 0*/) {
	/*
	 * State 0,
	 * if there is query Seek, generate om_seek_uri
	 * SEND_DATA_BACK
	 */
	/* reset value to 0 */
	con->ssp.ssp_bitrate = 0;
	con->ssp.ssp_attr_partial_content_len = 0;
	con->ssp.ssp_video_br_flag            = 0;
	con->ssp.client_brstart               = 0;
	con->ssp.client_brstop                = 0;
	con->ssp.afr_header_size              = 0;
	con->ssp.afr_header                   = NULL;
	con->ssp.afr_buffered_data            = NULL;
	con->ssp.serve_fmt                    = 0;
	/* find the seek query */
	if (ssp_cfg->seek.enable && ssp_cfg->seek.uri_query) {
	    get_query_arg_by_name(&phttp->hdr,
				ssp_cfg->seek.uri_query,
				strlen(ssp_cfg->seek.uri_query),
				&querySeekTmp, &seekLen);
	}
	TSTR(querySeekTmp, seekLen, querySeek);
	if ( strncmp(querySeek, "", 1) == 0 ) {
            querySeek = NULL;
        }

	if (querySeek != NULL) { /* found seek query */
	    /* Create the SEEK URI for OM */
	    generate_om_seek_uri(con, ssp_cfg->seek.uri_query);
	}
	/* if no querySeek, no need to generate om_seek_uri */
	CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
	//SET_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);

	/* to handle client request with byterange */
	con->ssp.ssp_video_br_flag = 0;
	if (CHECK_HTTP_FLAG(phttp, HRF_BYTE_RANGE)) {
	    /* store the byte range requests sent by client*/
	    con->ssp.ssp_video_br_flag = 1;
	    con->ssp.client_brstart = phttp->brstart;
	    con->ssp.client_brstop  = phttp->brstop;

	    /* Clear any byte range requests sent by client */
	    CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
	    phttp->brstart = 0;
	    phttp->brstop  = 0;
	}
	DBG_LOG(MSG, MOD_SSP,
		"ssp_mprops_detect: Request to fetch file internally. "
		"attr_state: %d", con->ssp.attr_state);
	con->ssp.attr_state = 5;
	rv = SSP_SEND_DATA_BACK;
	goto exit;

	/*
	 * Move the check of fetch data availability below the state 0
	 * As the data availability check is only for current fucntion
	 * If the availability check is above the state 0, and previous
	 * function call is exited due to data not in cache, when it comes
	 * to current function, the contentLen is still OM_STAT_SIG_TOT_CONTENT_LEN,
	 * which will cause current funtion exit directly
	 */
    } else if (contentLen == OM_STAT_SIG_TOT_CONTENT_LEN &&
		   !CHECK_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH)) {
	/* File not found */
	DBG_LOG(WARNING, MOD_SSP,
		"ssp_mprops_detect: File not found."
		"Exit the bitrate detection func. attr_state: %d",
		con->ssp.attr_state);
	rv = SSP_DETECT_MEDIA_ATTR_FAIL;
	goto exit;
    } else if (contentLen == OM_STAT_SIG_TOT_CONTENT_LEN &&
               CHECK_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH)) { //not in cache
	DBG_LOG(WARNING, MOD_SSP,
		"ssp_mprops_detect: File is not in cache. "
		"Exit bitrate detection func. attr_state: %d",
		con->ssp.attr_state);
	rv = SSP_DETECT_MEDIA_ATTR_FAIL;
	goto exit;
    } else if (con->ssp.attr_state == 5) {
	/*
	 * State 5
	 * Obtain nkn_attr_priv and check datarate and container format
	 *    1. Check the flag nkn_attr_priv->v_reserved
	 *    2. If flag is set, we can read datarate and container format
	 *       If flag is not set, we will go to next state
	 */
	nkn_uol_t uol = { 0,0,0 };
	nkn_attr_t *ap = NULL;
	uol.cod = con->http.req_cod;
	nkn_buffer_t *abuf = nkn_buffer_get(&uol, NKN_BUFFER_ATTR);
	if (abuf) {
	    ap = (nkn_attr_t *)nkn_buffer_getcontent(abuf);
	    nkn_attr_priv_t *ssp_priv = &ap->na_private;
	    if (ssp_priv->vpe.v_reserved != 0) {
		/* we can read datarate and container type */
		con->ssp.ssp_bitrate      = ssp_priv->vpe.v_video_bit_rate;
		con->ssp.ssp_container_id = ssp_priv->vpe.v_cont_type;
		nkn_buffer_release(abuf);

		DBG_LOG(MSG, MOD_SSP,
			"ssp_mprops_detect: Read bitrate from COD, %d",
			con->ssp.ssp_bitrate);

		/* container type from COD is valid */
		if (con->ssp.ssp_container_id != 0) {
		    /* restore the byterange request */
		    if (con->ssp.ssp_video_br_flag) {
			phttp->brstart = con->ssp.client_brstart;
			phttp->brstop  = con->ssp.client_brstop;
			SET_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
			con->ssp.client_brstart = 0;
			con->ssp.client_brstop  = 0;
		    } else { /* client request not using byterange request */
			/* HRF_BYTE_RANGE flag should not set here */
			CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
			phttp->brstart = 0 ;
			phttp->brstop  = 0;
		    }
		}
                /*
                 * Reset state
		 * This is to handle multiple call of AFR detect in one connection
		 * In up/downgrade, AFR detect is called for each fmt in one connection
		 * But con->ssp.attr_state is for the entire one  connection
		 * Need to rest the state after one call of AFR detect
                 * The keep-alive is handled in update_ssp()
                 */
                con->ssp.attr_state = 0;
		CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
		rv = SSP_SEND_DATA_OTW;
		goto exit;
	    } else {
		/*
		 * We can not read bitrate and container type from COD
		 * because it does not exist in COD.
		 * We will try to parse the content to fetch the prop
		 */
		DBG_LOG(MSG, MOD_SSP,
                        "ssp_mprops_detect: bitrate prop is not in COD. "
			"try to parse video file. attr_state: %d",
			con->ssp.attr_state);
		nkn_buffer_release(abuf);
		con->ssp.attr_state = 10;
		rv = SSP_SEND_DATA_BACK;
		goto exit;
	    }
	} else {
	    /*
	     * We can not read bitrate and container type
	     * Will try to parse the content
	     */
	    DBG_LOG(MSG, MOD_SSP,
                    "ssp_mprops_detect: fail to fetch COD. "
		    "try to parse video file. attr_state: %d",
		    con->ssp.attr_state);
	    con->ssp.attr_state = 10;
	    rv = SSP_SEND_DATA_BACK;
	}
	goto exit;
    } else if (con->ssp.attr_state == 10 || con->ssp.attr_state == 20 ||
	       con->ssp.attr_state == 15 /* 15 is for MP4 only*/) {
	/*
	 * Youtube SSP use the itag query for file type
	 * add a file type detection to check whether it is the same as itag.
	 */
	if (con->ssp.attr_state == 10 && con->ssp.ssp_client_id == 5 &&
	    con->ssp.ssp_attr_partial_content_len == 0) { /* signature is only at the first 32KB data */
	    uint8_t* p_file_buf;
	    p_file_buf = (uint8_t*)dataBuf;
	    if (bufLen > 16) {
		// 16 is ASF guid length, but we have not conside the webM situation
		if (vpe_detect_media_type(p_file_buf) != con->ssp.ssp_container_id) {
		    DBG_LOG(WARNING, MOD_SSP,
			    "ssp_mprops_detect:: Youtube file type detect is different from itag. "
			    "attr_state: %d", con->ssp.attr_state);
		    con->ssp.ssp_container_id = 0;
		    rv = SSP_DETECT_MEDIA_ATTR_FAIL;
		    goto exit;
		}
	    }
	}

	/* start to parse content for different container */
	switch (con->ssp.ssp_container_id) {
	    case CONTAINER_TYPE_FLV:
		/* for FLV, we will only use the first 32K to parse */
		if (con->ssp.ssp_attr_partial_content_len < contentLen && bufLen > 0) {
		    switch (con->ssp.attr_state) {
			case 10:
			    /* Create the mprops detector */
			    p_md = vpe_mprops_create_media_detector((uint8_t*)dataBuf,
								    bufLen,
								    contentLen,
								    con->ssp.ssp_container_id);
			    if (p_md == NULL) {
				DBG_LOG(MSG, MOD_SSP,
					"ssp_mprops_detect: FLV: Failed to allocate media detector ctx. "
					"have to exit. attr_state: %d",
					con->ssp.attr_state);
				rv = SSP_DETECT_MEDIA_ATTR_FAIL;
				goto exit;
			    }

			    /* Compute offset and size of flv onmetadata tag */
			    fc_rv = vpe_mprops_get_metadata_size(p_md, &ol);
			    if (rv != 0 || (ol.offset == 0 || ol.length == 0)) {
				DBG_LOG(MSG, MOD_SSP,
                                        "ssp_mprops_detect: FLV: Failed to get metadata size and pos. "
					"have to exit. attr_state: %d",
					con->ssp.attr_state);
				rv = SSP_DETECT_MEDIA_ATTR_FAIL;
                                goto exit;
			    }

			    /* detect if onmetadata tag is entire in bufLen*/
			    if ((int)(ol.offset + ol.length) >= bufLen) {
				/* larger than bufLen*/
				DBG_LOG(SEVERE, MOD_SSP,
					"ssp_mprops_detect: FLV: onmetadata tag larger than bufLen "
					"is not supported. have to exit. attr_state: %d",
					con->ssp.attr_state);
                                rv = SSP_DETECT_MEDIA_ATTR_FAIL;
				goto exit;
			    }
			    /*
			     * Initialize the meta data buffers
			     * The rv of vpe_mprops_set_meta_data is always 0
			     */
			    fc_rv = vpe_mprops_set_meta_data(p_md, (uint8_t*)dataBuf + ol.offset, ol.length);

			    /* populates the media attributes from the meta data */
			    fc_rv = vpe_mprops_populate_media_attr(p_md);
			    if (fc_rv != 0) {
				DBG_LOG(MSG, MOD_SSP,
					"ssp_mprops_detect: FLV: Failed to detect media attr. "
					"is not supported. have to exit. attr_state: %d",
					con->ssp.attr_state);
				rv = SSP_DETECT_MEDIA_ATTR_FAIL;
				goto exit;
			    }

			    /* Get media properties data */
			    p_mp = vpe_mprops_get_media_attr(p_md);
			    if (p_mp == NULL) {
				DBG_LOG(MSG, MOD_SSP,
					"ssp_mprops_detect: FLV: Failed to obtain media props data. "
					"is not supported. have to exit. attr_state: %d",
					con->ssp.attr_state);
				rv = SSP_DETECT_MEDIA_ATTR_FAIL;
				goto exit;
			    }

			    /* Set the AFR for current connection*/
			    DBG_LOG(MSG, MOD_SSP,
				    "ssp_mprops_detect: FLV: obtain bitrate by parse file, %d",
				    p_mp->v_video_bitrate);
			    con->ssp.ssp_bitrate = p_mp->v_video_bitrate;

			    /* Update the nkn_attr_priv */
			    {
				nkn_uol_t uol = { 0,0,0 };
				nkn_attr_t *ap = NULL;
				uol.cod = con->http.req_cod;
				nkn_buffer_t *abuf = nkn_buffer_get(&uol, NKN_BUFFER_ATTR);
				if (abuf) {
				    ap = (nkn_attr_t *)nkn_buffer_getcontent(abuf);
				    nkn_attr_priv_t *ssp_priv = &ap->na_private;
				    if (ssp_priv->vpe.v_reserved == 0) {
					DBG_LOG(MSG, MOD_SSP,
						"ssp_mprops_detect: FLV: update COD");
					/* we can update datarate and container type */
					ssp_priv->vpe.v_video_bit_rate = con->ssp.ssp_bitrate;
					ssp_priv->vpe.v_cont_type = con->ssp.ssp_container_id;
					ssp_priv->vpe.v_video_codec_type = p_mp->v_video_codec_type;
					ssp_priv->vpe.v_audio_codec_type = p_mp->v_audio_codec_type;
					ssp_priv->vpe.v_reserved = 1;
					nkn_buffer_setmodified(abuf);
				    }
				    nkn_buffer_release(abuf);
				} /* if (abuf) */
			    }

			    /* restore the byterange request */
			    if (con->ssp.ssp_video_br_flag) {
				phttp->brstart = con->ssp.client_brstart;
				phttp->brstop  = con->ssp.client_brstop;
				SET_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
				con->ssp.client_brstart = 0;
				con->ssp.client_brstop  = 0;
			    } else { /* client request not using byterange request */
				/* HRF_BYTE_RANGE flag should not set here */
				CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
				phttp->brstart = 0 ;
				phttp->brstop  = 0;
			    }

			    /* Clean the mprops detector */
			    vpe_mprops_cleanup_media_detector(p_md);

			    con->ssp.ssp_attr_partial_content_len = 0;
			    con->ssp.attr_state = 0;
			    CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
			    rv = SSP_SEND_DATA_OTW; // need to add this?
			    break;
#if 0 /* current FLV parser does not support no duration FLV file*/
			    /* can not support onmetadata tag larger than 32KB */
			    /* need further implementation */
			case 20:
			    // Parse a block of FLV data
			    if((seek_pos = flv_parse_data(con->ssp.fp_state, (uint8_t*)dataBuf, bufLen)) > 0) {
				DBG_LOG(MSG, MOD_SSP, "FLVSeek: Time to Byte translated to %ld", seek_pos);
				break;
			    }
			    con->ssp.ssp_attr_partial_content_len += bufLen;
			    rv = SSP_WAIT_FOR_DATA;
			    goto exit;

			    break;
#endif
		    } //switch
		} //con->ssp.ssp_attr_partial_content_len ...

		break; // end of FLV
	    case CONTAINER_TYPE_MP4:
	    case CONTAINER_TYPE_3GPP:
	    case CONTAINER_TYPE_3GP2:
		switch (con->ssp.attr_state) {
		    case 10:
			if (bufLen > 0 && con->ssp.ssp_attr_partial_content_len < contentLen) {
			    /*
			     * Normally, for video streaming, moov box should
			     * be placed at the first 32KB of MP4 files
			     * But this is not mandatory.
			     * Some MP4 files put the moov box at the end of
			     * file or not at the first 32KB
			     *
			     * State 10:
			     * a new function to detect the pos and size of
			     * moov and mdat box before the processing of moov box
			     */

			    /* only come to the init for the first time */
			    if (con->ssp.mp4_afr_state == NULL) {
				fp = mp4_init_find_box_parser();
				if (!fp) {
				    DBG_LOG(MSG, MOD_SSP,
					    "ssp_mprops_detect: MP4: Unable to init MP4 parser. "
					    "have to exit. attr_state: %d",
					    con->ssp.attr_state);
				    rv = SSP_DETECT_MEDIA_ATTR_FAIL;
				    goto exit;
				}
				con->ssp.mp4_afr_state = (void*)fp; /* store for future loop*/

				/* set which boxes need to find */
				fc_rv = mp4_set_find_box_name(fp, find_box_name_list,
							      n_box_to_find);
				if (fc_rv < 0) {
				    DBG_LOG(MSG, MOD_SSP,
					    "ssp_mprops_detect: MP4: Fail to Set find box name. "
					    "have to exit. attr_state: %d",
					    con->ssp.attr_state);
				    rv = SSP_DETECT_MEDIA_ATTR_FAIL;
				    goto exit;
				}

				/* parser initial the dataBuf*/
				fc_rv = mp4_parse_init_databuf(fp, (uint8_t*)dataBuf, bufLen);
				if (fc_rv < 0) {
				    DBG_LOG(MSG, MOD_SSP,
					    "ssp_mprops_detect: MP4: Fail to find ftyp box, not a Valid MP4. "
					    "have to exit. attr_state: %d",
					    con->ssp.attr_state);
				    rv = SSP_DETECT_MEDIA_ATTR_FAIL;
				    goto exit;
				}
			    } /* if (con->ssp.mp4_fp_state == NULL) */

			    fp = (nkn_vpe_mp4_find_box_parser_t*)con->ssp.mp4_afr_state;

			    /* Loop to parse all the root box of MP4 file */
			    fc_rv = mp4_parse_find_box_info(fp, (uint8_t*)dataBuf, bufLen,
							    &brstart, &brstop);
			    if (fc_rv == n_box_to_find) {
				/* found all the boxes in find_box_name_list */

				// I do not think we need this
				// con->ssp.mp4_fp_state = NULL;

				// shall we need to reset other values?
				con->ssp.ssp_attr_partial_content_len = 0; /* reset value */

				/* check whether box size and offset are valid */
				for (int i = 0; i < n_box_to_find - 1/*do not check ftyp box*/; i++) {
				    if (fp->pbox_list->box_info[i].offset == 0 ||
					fp->pbox_list->box_info[i].size == 0) {
					DBG_LOG(WARNING, MOD_SSP,
						"ssp_mprops_detect: MP4: Corrupted moov/mdat offset or size. "
						"have to exit. attr_state: %d",
						con->ssp.attr_state);
					rv = SSP_DETECT_MEDIA_ATTR_FAIL;
					goto exit;
				    }
				}

				for (int i =0; i < n_box_to_find; i++) {
				    DBG_LOG(MSG, MOD_SSP,
					    "ssp_mprops_detect: MP4: box name = %s, offset = %ld, size = %ld. ",
					    fp->pbox_list->box_info[i].name,
					    fp->pbox_list->box_info[i].offset,
					    fp->pbox_list->box_info[i].size);
				}

				/* must reset this */
				if (CHECK_HTTP_FLAG(phttp, HRF_BYTE_RANGE)) {
				    CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
				    phttp->brstart = 0;
				    phttp->brstop = 0;
				}
				rv = SSP_SEND_DATA_BACK;
				/*
				 * fp will be cleaned at the end of MP4 seek
				 * go to state 15
				 */
				con->ssp.attr_state = 15;
				goto exit;
			    }

			    if (brstart < 0) {
				DBG_LOG(MSG, MOD_SSP,
					"ssp_mprops_detect: MP4: Invalid file "
					"have to exit. attr_state: %d",
					con->ssp.attr_state);
				rv = SSP_DETECT_MEDIA_ATTR_FAIL;
				goto exit;
			    }

			    /* add the parsed bufLen into ssp_partial_content_len */
			    con->ssp.ssp_attr_partial_content_len = brstart;
			    if (con->ssp.ssp_attr_partial_content_len < contentLen) {
				phttp->brstart = brstart;
				phttp->brstop  = contentLen - 1;
				rv = SSP_SEND_DATA_BACK; // load additional data to continue the parse
			    } else {
				DBG_LOG(MSG, MOD_SSP,
                                "ssp_mprops_detect: MP4: Reach the end of file, Fail to find all boxes "
					"have to exit. attr_state: %d",
					con->ssp.attr_state);
				rv = SSP_DETECT_MEDIA_ATTR_FAIL;
			    }
			    goto exit;
			} else {
			    DBG_LOG(MSG, MOD_SSP,
				    "ssp_mprops_detect: MP4: Fail to find all boxes "
				    "have to exit. attr_state: %d",
				    con->ssp.attr_state);
			    rv = SSP_DETECT_MEDIA_ATTR_FAIL;
			    goto exit;
			}
			break;
		    case 15:
			/*
			 * State 15: alloc buffer for whole moov box
			 *           Read the whole moov box into buffer
			 *           ftype box + moov box will be read into buffer
			 */
			fp = (nkn_vpe_mp4_find_box_parser_t*)con->ssp.mp4_afr_state;

			if (bufLen > 0 && con->ssp.ssp_attr_partial_content_len < contentLen) {
			    /* alloc the buffer only for the first time */
			    if (!con->ssp.afr_buffered_data) {
				/* header_size = moov box size + ftype box size*/
				con->ssp.afr_header_size = fp->pbox_list->box_info[0].size +
							   fp->pbox_list->box_info[2].size +
							   16; /* add 16 to prevent valgrind issue*/

				con->ssp.afr_header = (uint8_t *) \
				    nkn_calloc_type(con->ssp.afr_header_size,
						    sizeof(char),
						    mod_ssp_mp4_rebased_seekheader_t);
				if (con->ssp.afr_header == NULL) {
				    DBG_LOG(MSG, MOD_SSP,
					    "ssp_mprops_detect: MP4:: Failed to allocate space for MP4 header. "
					    "have to exit. attr_state: %d",
					    con->ssp.attr_state);
				    rv = SSP_DETECT_MEDIA_ATTR_FAIL;
				    goto exit;
				}
				/* init bitstream */
				con->ssp.afr_buffered_data = bio_init_bitstream(con->ssp.afr_header,
										con->ssp.afr_header_size);
				if (con->ssp.afr_buffered_data == NULL) {
				    DBG_LOG(MSG, MOD_SSP,
					    "ssp_mprops_detect: MP4:: Failed to init data buffer for MP4 header. "
					    "have to exit. attr_state: %d",
					    con->ssp.attr_state);
				    rv = SSP_DETECT_MEDIA_ATTR_FAIL;
				    goto exit;
				}
			    } // if (!con->ssp.buffered_data)

			    /* 1: read the ftyp box into buffer only for the first time */
			    if (bio_get_pos(con->ssp.afr_buffered_data) == 0) {
				int bytes_to_read = fp->pbox_list->box_info[2].size; /* ftyp box size */
				if (fp->pbox_list->box_info[2].offset != 0) {
                                    DBG_LOG(MSG, MOD_SSP,
                                            "ssp_mprops_detect: MP4: Invalid MP4 file. "
                                            "have to exit. attr_state: %d",
                                            con->ssp.attr_state);
                                    rv = SSP_DETECT_MEDIA_ATTR_FAIL;
			            goto exit;
				}
				bytesWritten = bio_aligned_write_block(con->ssp.afr_buffered_data,
								       (uint8_t*)dataBuf,
								       bytes_to_read);
				con->ssp.ssp_attr_partial_content_len += fp->pbox_list->box_info[0].offset;
				phttp->brstart = fp->pbox_list->box_info[0].offset; /* moov box pos*/
				phttp->brstop = contentLen - 1;
				rv = SSP_SEND_DATA_BACK; //
				goto exit;
			    }

			    /* 2: loop to read the whole moov box into buffer */
			    if (bio_get_pos(con->ssp.afr_buffered_data) \
				< con->ssp.afr_header_size) {
				int bytes_left = con->ssp.afr_header_size - 16 -
						 bio_get_pos(con->ssp.afr_buffered_data);
				int bytes_to_read = (bufLen < bytes_left ? bufLen : bytes_left);
				bytesWritten = bio_aligned_write_block(con->ssp.afr_buffered_data,
								       (uint8_t*)dataBuf,
								       bytes_to_read);
				if (bytesWritten < 0) {
				    DBG_LOG(MSG, MOD_SSP,
					    "ssp_mprops_detect: MP4: Failed to read data into MP4 header. "
					    "have to exit. attr_state: %d",
					    con->ssp.attr_state);
				    rv = SSP_DETECT_MEDIA_ATTR_FAIL;
				    goto exit;
				}
				con->ssp.ssp_attr_partial_content_len += bytes_to_read;

				if ((bio_get_pos(con->ssp.afr_buffered_data) + 16) == con->ssp.afr_header_size) {
				    DBG_LOG(MSG, MOD_SSP,
					    "ssp_mprops_detect: MP4: Read ftyp and moov box into buffer. "
					    "go to state 20. attr_state: %d",
					    con->ssp.attr_state);
				    /* only this condition will go to state 20 */
				    con->ssp.attr_state = 20;
				} else if ((con->ssp.ssp_attr_partial_content_len < contentLen) &&
					   (bio_get_pos(con->ssp.afr_buffered_data) + 16) \
					   < con->ssp.afr_header_size) {
				    rv = SSP_WAIT_FOR_DATA;
				    goto exit;
				} else {
				    DBG_LOG(MSG, MOD_SSP,
                                    "ssp_mprops_detect: MP4: Reach the edn of file, Fail to find all boxes. "
					    "have to exit. attr_state: %d",
					    con->ssp.attr_state);
				    rv = SSP_DETECT_MEDIA_ATTR_FAIL;
				    goto exit;
				}
			    } else {
				DBG_LOG(MSG, MOD_SSP,
					"ssp_mprops_detect: MP4: This buffered_data condition should not happen "
					"have to exit. attr_state: %d",
					con->ssp.attr_state);
				rv = SSP_DETECT_MEDIA_ATTR_FAIL;
				goto exit;
			    }

			    /*
			     * now, we should have the entire moov box in buffer
			     * Will not break, go to state 20 directly
			     */
			} else {
                            DBG_LOG(MSG, MOD_SSP,
				    "ssp_mprops_detect: MP4: Fail to find all boxes. "
                                    "have to exit. attr_state: %d",
                                    con->ssp.attr_state);
                            rv = SSP_DETECT_MEDIA_ATTR_FAIL;
                            goto exit;
			}
			/* no break here */
		    case 20:
			/*
			 * We have the entire moov box in afr_buffered_data
			 * We will call nkn_vpe_media_props_api.c to
			 * fetch duration and codec info
			 */
			fp = (nkn_vpe_mp4_find_box_parser_t*)con->ssp.mp4_afr_state;
			bs = (bitstream_state_t*)con->ssp.afr_buffered_data;
			/* Create the mprops detector */
			p_md = vpe_mprops_create_media_detector((uint8_t*)bs->data,
								bio_get_pos(con->ssp.afr_buffered_data),
								contentLen,
								con->ssp.ssp_container_id);
			if (p_md == NULL) {
			    DBG_LOG(MSG, MOD_SSP,
				    "ssp_mprops_detect: MP4: Failed to allocate media detector ctx. "
				    "have to exit. attr_state: %d",
				    con->ssp.attr_state);
			    rv = SSP_DETECT_MEDIA_ATTR_FAIL;
			    goto exit;
			}

			/* Compute offset and size of MP4 moov box */
			fc_rv = vpe_mprops_get_metadata_size(p_md, &ol);
			if (fc_rv != 0 ||(ol.offset == 0 || ol.length == 0)) {
			    DBG_LOG(MSG, MOD_SSP,
				    "ssp_mprops_detect: MP4: Failed to get metadata size and pos. "
				    "have to exit. attr_state: %d",
				    con->ssp.attr_state);
			    rv = SSP_DETECT_MEDIA_ATTR_FAIL;
			}

			/* Initialize the meta data buffers */
			vpe_mprops_set_meta_data(p_md, (uint8_t*)bs->data + ol.offset, ol.length);

			/* populates the media attributes from the meta data */
			fc_rv = vpe_mprops_populate_media_attr(p_md);
			if (fc_rv != 0) {
			    DBG_LOG(MSG, MOD_SSP,
				    "ssp_mprops_detect: MP4: Failed to detect media attr. "
				    "have to exit. attr_state: %d",
				    con->ssp.attr_state);
			    rv = SSP_DETECT_MEDIA_ATTR_FAIL;
                            goto exit;
			}

			/* Get media properties data */
			p_mp = vpe_mprops_get_media_attr(p_md);
			if (p_mp == NULL) {
                            DBG_LOG(MSG, MOD_SSP,
                                    "ssp_mprops_detect: MP4: Failed to obtain media props data. "
                                    "have to exit. attr_state: %d",
				    con->ssp.attr_state);
                            rv = SSP_DETECT_MEDIA_ATTR_FAIL;
                            goto exit;
			}
			/* Set the AFR for current connection*/
			DBG_LOG(MSG, MOD_SSP,
				"ssp_mprops_detect: MP4: obtain bitrate by parse file, %d",
				p_mp->v_video_bitrate);
			con->ssp.ssp_bitrate = p_mp->v_video_bitrate;
			{
			    /* Update the nkn_attr_priv */
			    nkn_uol_t uol = { 0,0,0 };
			    nkn_attr_t *ap = NULL;
			    uol.cod = con->http.req_cod;
			    nkn_buffer_t *abuf = nkn_buffer_get(&uol, NKN_BUFFER_ATTR);
			    if (abuf) {
				ap = (nkn_attr_t *)nkn_buffer_getcontent(abuf);
				nkn_attr_priv_t *ssp_priv = &ap->na_private;
				if (ssp_priv->vpe.v_reserved == 0) {
				    /* we can update datarate and container type */
				    DBG_LOG(MSG, MOD_SSP,
					    "ssp_mprops_detect: MP4: update COD");
				    ssp_priv->vpe.v_video_bit_rate = con->ssp.ssp_bitrate;
				    ssp_priv->vpe.v_cont_type = con->ssp.ssp_container_id;
				    ssp_priv->vpe.v_video_codec_type = p_mp->v_video_codec_type;
				    ssp_priv->vpe.v_audio_codec_type = p_mp->v_audio_codec_type;
				    ssp_priv->vpe.v_reserved = 1;
				    nkn_buffer_setmodified(abuf);
				}
				nkn_buffer_release(abuf);
			    }
			}

			/* free memory and restore state */
			if (fp) {
			    mp4_find_box_parser_cleanup(fp);
			    con->ssp.mp4_afr_state = NULL;
			}

			/* Clean the mprops detector */
			vpe_mprops_cleanup_media_detector(p_md);

			/* Close bitstream bs */
			if (bs) {
			    bio_close_bitstream(bs);
			    bs = NULL;
			    con->ssp.afr_buffered_data = NULL;
			}
			/* Clean the mp4 afr header buffer*/
			con->ssp.afr_header_size = 0;
			if (con->ssp.afr_header) {
			    free(con->ssp.afr_header);
			    con->ssp.afr_header = NULL;
			}
			/* reset state*/
			con->ssp.ssp_attr_partial_content_len = 0;
			con->ssp.attr_state = 0;

			/* restore the byterange request */
			if (con->ssp.ssp_video_br_flag) {
			    phttp->brstart = con->ssp.client_brstart;
			    phttp->brstop  = con->ssp.client_brstop;
			    SET_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
			    con->ssp.client_brstart = 0;
			    con->ssp.client_brstop  = 0;
			} else { /* client request not using byterange request */
			    /* HRF_BYTE_RANGE flag should not set here */
			    CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
			    phttp->brstart = 0 ;
			    phttp->brstop  = 0;
			}
			CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
			rv = SSP_SEND_DATA_OTW;
			goto exit;

			break;
		}

		break; /* end of the MP4  parser */

	    case CONTAINER_TYPE_ASF:
		/* ASF will also share the same work flow with WEBM*/
	    case CONTAINER_TYPE_WEBM:
		//if (is_known_header_present(&phttp->hdr, MIME_HDR_X_NKN_SEEK_URI)) {
		//    delete_known_header(&phttp->hdr, MIME_HDR_X_NKN_SEEK_URI);
		//}
		if (bufLen > 0 && con->ssp.ssp_attr_partial_content_len < contentLen) {
		    /* Start the buffer read */

		    if (!con->ssp.afr_buffered_data) {
			/* alloc memory for mp4 header*/
			con->ssp.afr_header = (uint8_t* ) \
				nkn_calloc_type(MP4_INIT_BYTE_RANGE_LEN,
						sizeof(char),
						mod_ssp_mp4_rebased_seekheader_t);
			if (con->ssp.afr_header == NULL) {
			    DBG_LOG(MSG, MOD_SSP,
				    "ssp_mprops_detect: ASF/WEBM: Failed to allocate memory for header. "
				    "have to exit. attr_state: %d",
				    con->ssp.attr_state);
			    rv = SSP_DETECT_MEDIA_ATTR_FAIL;
			    goto exit;
			}
			con->ssp.afr_buffered_data = \
			    bio_init_bitstream(con->ssp.afr_header,
					       MP4_INIT_BYTE_RANGE_LEN);
			if (con->ssp.afr_buffered_data == NULL) {
			    DBG_LOG(MSG, MOD_SSP,
				    "ssp_mprops_detect: ASF/WEBM: Failed to init data buffer for header. "
				    "have to exit. attr_state: %d",
				    con->ssp.attr_state);
			    rv = SSP_DETECT_MEDIA_ATTR_FAIL;
			    goto exit;
			}
			con->ssp.afr_header_size = MP4_INIT_BYTE_RANGE_LEN;
		    } /* if(!con->ssp.afr_buffered_data) */

		    /* read from dataBuf to buffer */
		    bytesWritten = bio_aligned_write_block(con->ssp.afr_buffered_data,
							   (uint8_t*)dataBuf, bufLen);

		    if ((bio_get_pos(con->ssp.afr_buffered_data) > con->ssp.afr_header_size)) {
			/* we had enough data*/
			switch (con->ssp.attr_state) {
			    case 10:
				bs = (bitstream_state_t*)con->ssp.afr_buffered_data;
				if (con->ssp.ssp_container_id == CONTAINER_TYPE_WEBM) {
				    /* get offset and size of metadata for webm */
				    nkn_vpe_webm_segment_pos((uint8_t*)bs->data,
							     MP4_INIT_BYTE_RANGE_LEN,
							     &ol);
				} else if (con->ssp.ssp_container_id == CONTAINER_TYPE_ASF) {
				    /* get offset and size of header object for ASF */
				    nkn_vpe_asf_header_obj_pos((uint8_t*)bs->data,
							       MP4_INIT_BYTE_RANGE_LEN,
							       &ol);
				}

				if (((ol.offset == 0 || ol.length == 0) && /* not ASF */
				     con->ssp.ssp_container_id != CONTAINER_TYPE_ASF) ||
				    (ol.length == 0 && /* ASF offset will always be 0 */
				     con->ssp.ssp_container_id == CONTAINER_TYPE_ASF)) {
				    DBG_LOG(WARNING, MOD_SSP,
					    "ssp_mprops_detect: ASF/WEBM: Corrupt webm trak/asf header object. "
					    "have to exit. attr_state: %d",
					    con->ssp.attr_state);

				    /* restore the byterange request */
				    if (con->ssp.ssp_video_br_flag) {
					phttp->brstart = con->ssp.client_brstart;
					phttp->brstop  = con->ssp.client_brstop;
					SET_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
					con->ssp.client_brstart = 0;
					con->ssp.client_brstop  = 0;
				    } else { /* client request not using byterange request */
					/* HRF_BYTE_RANGE flag should not set here */
					CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
					phttp->brstart = 0 ;
					phttp->brstop  = 0;
				    }
				    con->ssp.attr_state = 0;
				    rv = SSP_SEND_DATA_OTW;
				    CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
				    bio_close_bitstream(bs);
				    bs = NULL;
				    con->ssp.ssp_attr_partial_content_len = 0;
				    con->ssp.afr_header_size = 0;
				    if (con->ssp.afr_header) {
					free(con->ssp.afr_header);
					con->ssp.afr_header = NULL;
				    }
				    goto exit;
				}

				/* WebM  and ASF */
				con->ssp.afr_header_size = ol.offset + ol.length;

				if (bytesWritten < 0) {
				    tmp_p = (uint8_t *) \
						nkn_realloc_type(con->ssp.afr_header,
								 bufLen + bs->size,
								 mod_ssp_mp4_rebased_seekheader_t);
				    if (tmp_p== NULL) {
					DBG_LOG(MSG, MOD_SSP,
						"ssp_mprops_detect: ASF/WEBM: Failed to allocate seek headers"
						"have to exit. attr_state: %d",
						con->ssp.attr_state);
					rv = SSP_DETECT_MEDIA_ATTR_FAIL;
					if (con->ssp.afr_header) {
					    free(con->ssp.afr_header);
					    con->ssp.afr_header = NULL;
					}
					goto exit;
				    }
				    con->ssp.afr_header = tmp_p;
				    bio_reinit_bitstream(bs, con->ssp.afr_header,
							 bufLen + bs->size);
				    bio_aligned_seek_bitstream(bs,
							       con->ssp.ssp_attr_partial_content_len,
							       SEEK_SET);
				    bytesWritten = bio_aligned_write_block(con->ssp.afr_buffered_data,
									   (uint8_t*)dataBuf, bufLen);
				}

				con->ssp.attr_state = 20;
				break; /* get out of switch (con->ssp.attr_state), and SSP_WAIT_FOR_DATA */
			    case 20:
				/*
				 * We have the entire moov box in buffered_data
				 * We will call nkn_vpe_media_props_api.c to
				 * fetch duration and codec info
				*/
				bs = (bitstream_state_t*)con->ssp.afr_buffered_data;
				/* Create the mprops detector */
				p_md = vpe_mprops_create_media_detector((uint8_t*)bs->data,
									bio_get_pos(con->ssp.afr_buffered_data),
									contentLen,
									con->ssp.ssp_container_id);
				if (p_md == NULL) {
				    DBG_LOG(MSG, MOD_SSP,
					    "ssp_mprops_detect: ASF/WEBM: Fail to allocate media detector ctx."
					    "have to exit. attr_state: %d",
					    con->ssp.attr_state);
				    rv = SSP_DETECT_MEDIA_ATTR_FAIL;
				    goto exit;
				}

				/* Compute offset and size of 
				 *                            WebM Segment
				 *                            ASF Header Object
				 */
				rv = vpe_mprops_get_metadata_size(p_md, &ol);
				if (rv != 0 ||
				    ((ol.offset == 0 || ol.length == 0) && /*non ASF*/
				     con->ssp.ssp_container_id != CONTAINER_TYPE_ASF) ||
				    (ol.length == 0 && /* ASF */
				     con->ssp.ssp_container_id == CONTAINER_TYPE_ASF)) {
				    DBG_LOG(MSG, MOD_SSP,
					    "ssp_mprops_detect: ASF/WEBM: Fail to get metadata size and pos"
					    "have to exit. attr_state: %d",
					    con->ssp.attr_state);
				    rv = SSP_DETECT_MEDIA_ATTR_FAIL;
                                    goto exit;
				}

				/* Initialize the meta data buffers */
				vpe_mprops_set_meta_data(p_md, (uint8_t*)bs->data + ol.offset, ol.length);

				/* populates the media attributes from the meta data */
				rv = vpe_mprops_populate_media_attr(p_md);
				if (rv != 0) {
				    DBG_LOG(MSG, MOD_SSP,
                                            "ssp_mprops_detect: ASF/WEBM: Fail to detect media attr"
					    "have to exit. attr_state: %d",
					    con->ssp.attr_state);
				    rv = SSP_DETECT_MEDIA_ATTR_FAIL;
                                    goto exit;
				}

				/* Get media properties data */
				p_mp = vpe_mprops_get_media_attr(p_md);
				if (p_mp == NULL) {
				    DBG_LOG(MSG, MOD_SSP,
                                            "ssp_mprops_detect: ASF/WEBM: Fail to obtain media props data"
					    "have to exit. attr_state: %d",
					    con->ssp.attr_state);
				    rv = SSP_DETECT_MEDIA_ATTR_FAIL;
                                    goto exit;
				}

				/* Set the AFR for current connection*/
				DBG_LOG(MSG, MOD_SSP,
					"ssp_mprops_detect: ASF/WEBM: obtain bitrate by parse file, %d",
					p_mp->v_video_bitrate);
				con->ssp.ssp_bitrate = p_mp->v_video_bitrate;
				{
				    /* Update the nkn_attr_priv */
				    nkn_uol_t uol = { 0,0,0 };
				    nkn_attr_t *ap = NULL;
				    uol.cod = con->http.req_cod;
				    nkn_buffer_t *abuf = nkn_buffer_get(&uol, NKN_BUFFER_ATTR);
				    if (abuf) {
					ap = (nkn_attr_t *)nkn_buffer_getcontent(abuf);
					nkn_attr_priv_t *ssp_priv = &ap->na_private;
					if (ssp_priv->vpe.v_reserved == 0) {
					    /* we can update datarate and container type */
					    DBG_LOG(MSG, MOD_SSP,
						    "ssp_mprops_detect: ASF/WEBM: update COD");
					    ssp_priv->vpe.v_video_bit_rate = con->ssp.ssp_bitrate;
					    ssp_priv->vpe.v_cont_type = con->ssp.ssp_container_id;
					    ssp_priv->vpe.v_video_codec_type = p_mp->v_video_codec_type;
					    ssp_priv->vpe.v_audio_codec_type = p_mp->v_audio_codec_type;
					    ssp_priv->vpe.v_reserved = 1;
					    nkn_buffer_setmodified(abuf);
					}
					nkn_buffer_release(abuf);
				    }
				}

				/* Clean the mprops detector */
				if (p_md) {
				    vpe_mprops_cleanup_media_detector(p_md);
				    p_md = NULL;
				}
				/* Close bitstream bs */
				if (bs) {
				    bio_close_bitstream(bs);
				    bs = NULL;
				    con->ssp.afr_buffered_data = NULL;
				}

				/* Clean the header buffer*/
				con->ssp.afr_header_size = 0;
				if (con->ssp.afr_header) {
				    free(con->ssp.afr_header);
				    con->ssp.afr_header = NULL;
				}
				/* reset state*/
				con->ssp.ssp_attr_partial_content_len = 0;
				con->ssp.attr_state = 0;

				/* restore the byterange request */
				if (con->ssp.ssp_video_br_flag) {
				    phttp->brstart = con->ssp.client_brstart;
				    phttp->brstop  = con->ssp.client_brstop;
				    SET_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
				    con->ssp.client_brstart = 0;
				    con->ssp.client_brstop  = 0;
				} else { /* client request not using byterange request */
				    /* HRF_BYTE_RANGE flag should not set here */
				    CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
				    phttp->brstart = 0 ;
				    phttp->brstop  = 0;
				}
				CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
				rv = SSP_SEND_DATA_OTW;
				goto exit;
				break;
			} // switch (attr_state)

		    } else {
			/* not enough data, loop to read more from dataBuf*/
			bs = (bitstream_state_t*)con->ssp.afr_buffered_data;
			if (bytesWritten < 0) {
			    tmp_p = (uint8_t *)
					nkn_realloc_type(con->ssp.afr_header, bufLen + bs->size,
							 mod_ssp_mp4_rebased_seekheader_t);
			    if (tmp_p== NULL) {
				DBG_LOG(MSG, MOD_SSP,
					"ssp_mprops_detect: ASF/WEBM: Failed to reallocate memory for header"
					"have to exit. attr_state: %d",
					con->ssp.attr_state);
				rv = SSP_DETECT_MEDIA_ATTR_FAIL;
				if (con->ssp.afr_header) {
				    free(con->ssp.afr_header);
				    con->ssp.afr_header = NULL;
				}
				goto exit;
			    }
			    con->ssp.afr_header = tmp_p;
			    bio_reinit_bitstream(con->ssp.afr_buffered_data, con->ssp.afr_header,
						 bufLen + bs->size);
			    bio_aligned_seek_bitstream(con->ssp.afr_buffered_data,
						       con->ssp.ssp_attr_partial_content_len, SEEK_SET);
			    bytesWritten = bio_aligned_write_block(con->ssp.afr_buffered_data,
								   (uint8_t*)dataBuf, bufLen);
			}
		    } /* end of if ((bio_get_pos(con->ssp.afr_buffered_data) > */

		    con->ssp.ssp_attr_partial_content_len += bufLen;
		    /* for BZ 11022 */
		    if (con->ssp.ssp_attr_partial_content_len < contentLen) {
			/* there is enough data to do another WAIT_FOR_DATA */
			rv = SSP_WAIT_FOR_DATA;
			goto exit;
		    } else {
			DBG_LOG(MSG, MOD_SSP,
				"ssp_mprops_detect: ASF/WEBM: file is too small to do the detection. "
				"have to exit. attr_state: %d",
				con->ssp.attr_state);
			rv = SSP_DETECT_MEDIA_ATTR_FAIL;
			goto exit;
		    }

		} else {
		    DBG_LOG(MSG, MOD_SSP,
			    "ssp_mprops_detect: ASF/WEBM: Failed to load more dataBuf to header. "
			    "have to exit. attr_state: %d",
			    con->ssp.attr_state);
		    rv = SSP_DETECT_MEDIA_ATTR_FAIL;
		    goto exit;
		}
		break;
	} /* end of switch (con->ssp.ssp_container_id) */
    }

 exit:
    if (rv == SSP_DETECT_MEDIA_ATTR_FAIL) {
        /* if client request has byte range request, restore it */
        if (con->ssp.ssp_video_br_flag) {
	    phttp->brstart = con->ssp.client_brstart;
	    phttp->brstop  = con->ssp.client_brstop;
	    SET_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
	    con->ssp.client_brstart = 0;
	    con->ssp.client_brstop  = 0;
	} else { /* client request not using byterange request */
	    /* HRF_BYTE_RANGE flag should not set here */
	    CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
	    phttp->brstart = 0 ;
	    phttp->brstop  = 0;
	}

	/* free memory and restore state */
	if (con->ssp.mp4_afr_state) {
	    mp4_find_box_parser_cleanup(  \
			(nkn_vpe_mp4_find_box_parser_t*)con->ssp.mp4_afr_state);
	    con->ssp.mp4_afr_state = NULL;
	}

	/* Clean the mprops detector */
	if (p_md) {
	    vpe_mprops_cleanup_media_detector(p_md);
	    p_md = NULL;
	}

	/* Close bitstream bs */
        if (con->ssp.afr_buffered_data) {
            bio_close_bitstream(con->ssp.afr_buffered_data);
            con->ssp.afr_buffered_data = NULL;
        }

	/* Clean the afr header buffer*/
        if (con->ssp.afr_header) {
            free(con->ssp.afr_header);
            con->ssp.afr_header = NULL;
        }

        con->ssp.ssp_attr_partial_content_len = 0;
        con->ssp.afr_header_size = 0;

	/*reset state*/
	con->ssp.attr_state = 0;

	CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);

	/* cannot detect AFR, have to use fixed AFR configured per connection */
        con->ssp.ssp_bitrate = 0;

	rv = SSP_SEND_DATA_OTW;

    }
    return rv;
}

// Check and enforce seek or scrub request
int
ssp_seek_req(con_t *con, const ssp_config_t *ssp_cfg,
	     off_t contentLen, const char *dataBuf, int bufLen)
{
    const char *querySeekTmp=NULL;
    int seekLen=0;
    int rv=0;

    http_cb_t *phttp = &con->http;

    if (ssp_cfg->seek.uri_query) {
	get_query_arg_by_name(&phttp->hdr, ssp_cfg->seek.uri_query, strlen(ssp_cfg->seek.uri_query),
			      &querySeekTmp, &seekLen);
    }
    TSTR(querySeekTmp, seekLen, querySeek);
    if ( strncmp(querySeek, "", 1) == 0 ) {
	querySeek = NULL;
    }

    if (querySeek != NULL) {

	if (ssp_cfg->seek.activate_tunnel && atoi(querySeek) > 0) { // Force tunnel logic for seek requests
	    delete_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI);
	    SET_HTTP_FLAG(phttp, HRF_SSP_SEEK_TUNNEL);
	    con->ssp.attr_state = 0;
	    rv = -NKN_SSP_FORCE_TUNNEL_PATH;
	    goto exit;
	}

	if ( ssp_cfg->seek.flv_off_type == NULL || ssp_cfg->seek.mp4_off_type == NULL ) {
	    DBG_LOG(ERROR, MOD_SSP,
		    "CLI Seek Config nodes for virtual player are not set"
		    "Will tunnel the request");
	    delete_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI);
	    con->ssp.attr_state = 0;
            rv = -NKN_SSP_FORCE_TUNNEL_PATH;
            goto exit;
	}

	// FLV video file
	if ( con->ssp.ssp_container_filetype == CONTAINER_TYPE_FLV ){
	    if (strncmp(ssp_cfg->seek.flv_off_type, "byte-offset",
			strlen("byte-offset")) == 0) {
		rv = query_flv_seek(con, ssp_cfg->seek.uri_query,
				    ssp_cfg->seek.query_length);

	    } else if (strncmp(ssp_cfg->seek.flv_off_type, "time-secs",
			       strlen("time-secs")) == 0) {
		rv = query_flv_timeseek(con, ssp_cfg->seek.uri_query,
				        contentLen, dataBuf, bufLen, 1);

	    } else if (strncmp(ssp_cfg->seek.flv_off_type, "time-msec",
			       strlen("time-msec")) == 0) {
		rv = query_flv_timeseek(con, ssp_cfg->seek.uri_query,
					contentLen, dataBuf, bufLen, 1000);
	    }
	}
	else if ( con->ssp.ssp_container_filetype == CONTAINER_TYPE_MP4 ||
		  con->ssp.ssp_container_filetype == CONTAINER_TYPE_3GPP ||
		  con->ssp.ssp_container_filetype == CONTAINER_TYPE_3GP2) {
	    // MP4 video file
	    if (strncmp(ssp_cfg->seek.mp4_off_type, "time-secs",
			strlen("time-secs")) == 0) {
		//timeUnits is 1 unit
		rv = query_mp4_seek_for_any_moov(con, ssp_cfg->seek.uri_query,
				    contentLen, dataBuf, bufLen, 1);
	    } else if (strncmp(ssp_cfg->seek.mp4_off_type, "time-msec",
			       strlen("time-msec")) == 0) {
		//timeUnits is 1000 unit
		rv = query_mp4_seek_for_any_moov(con, ssp_cfg->seek.uri_query,
				    contentLen, dataBuf, bufLen, 1000);
	    }
	}
    } // if(querySeek != NULL

 exit:
    return rv;
}


#if 0
int
ssp_detect_media_attr(con_t *con, off_t contentLen, const char *dataBuf, int bufLen)
{
    UNUSED_ARGUMENT(bufLen);
    int rv = -1;
    nkn_uol_t uol = { 0,0,0 };
    nkn_attr_t *ap = NULL;
    nkn_buffer_t *abuf = NULL;
    //    http_cb_t *phttp = &con->http;

    switch (con->ssp.attr_state) {

        case 0:
            con->ssp.attr_state = 10;
            rv = 10; // SSP_SEND_DATA_BACK
            goto exit;
            break;

        case 10:

            if (contentLen == OM_STAT_SIG_TOT_CONTENT_LEN || contentLen == 0) { // File not found
                DBG_LOG(WARNING, MOD_SSP, "(Attr Detect State 10) File not found. Cannot proceed");
                rv = -10;
                goto exit;
            }

            if (dataBuf != NULL) {
                uol.cod = con->http.req_cod;
                abuf = nkn_buffer_get(&uol, NKN_BUFFER_ATTR);
                if (abuf) {
                    ap = (nkn_attr_t *)nkn_buffer_getcontent(abuf);
                    nkn_attr_priv_t *ssp_priv = &ap->na_private;

                    if (ssp_priv->vpe.v_type == NKN_ATTR_VPE) {
                        memcpy(con->ssp.vpe_attr, ssp_priv, sizeof (ssp_priv->vpe));
                        nkn_buffer_release(abuf);
                    }
                }
            }
            break;

        default:
            break;
    }



 exit:
    return rv;
}
#endif
/*
 * End of nkn_ssp_player_prop.c
 */
