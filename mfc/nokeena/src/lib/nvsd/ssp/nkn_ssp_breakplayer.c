/*
 * Break Server Side Player
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
#include "nkn_vpe_mp4_parser.h"
#include "nkn_vpe_mp4_seek_api.h"
#include "nkn_vpe_bitio.h"
#include "nkn_ssp_player_prop.h"

extern unsigned long long glob_ssp_hash_failures;
extern unsigned long long glob_ssp_container_flv, glob_ssp_container_mp4;
#if 0
#define MP4_INIT_BYTE_RANGE_LEN (32768) //32KB of data for moov atom size

int query_mp4_seek(con_t *con, const char *queryName, const char *uriData, int uriLen, off_t contentLen, const char *dataBuf, int bufLen);
void writeMDAT_Header(char *p_src, uint64_t mdat_size);
#endif
/////////////////////////////////////////////////////
// Break.com Server Side Player (Break SSP Type 1) //
/////////////////////////////////////////////////////
int
breakSSP(con_t *con, const char *uriData, int uriLen, const char *hostData, int hostLen,  const ssp_config_t *ssp_cfg, off_t contentLen, const char *dataBuf, int bufLen)
{
    const char *queryData, *querySeek=NULL;
    int remapLen=0, seekLen=0;
    int64_t initBufSize=0; // steadyRate=0;
    int rv, queryrv;
    //    int autoAFR=0;

    http_cb_t *phttp = &con->http;
    queryData = NULL;

    // Obtain the relative URL length stripped of all query params
    if ( (remapLen = findStr(uriData, "?", uriLen)) == -1 ) {
	remapLen = uriLen;
        DBG_LOG(WARNING, MOD_SSP, "URL does not contain any query params");
    }

    //Remap to URI to serve video
    add_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI, uriData, remapLen);

    // Verify the hash for break video URL. If successful then proceed, else reject connection
    if (ssp_cfg->hash_verify.enable){
        if (verifyMD5Hash(hostData, uriData, hostLen, &phttp->hdr, ssp_cfg->hash_verify) != 0) { // hashes dont match
            glob_ssp_hash_failures++;
            DBG_LOG(WARNING, MOD_SSP, "Hash Verification Failed. Closing connection (Error: %d)", NKN_SSP_HASHCHECK_FAIL );
	    if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
		DBG_TRACE("Hash authentication failed for virtual player: %s" , ssp_cfg->player_name);
	    }
            rv = -NKN_SSP_HASHCHECK_FAIL;  //Close/Refuse connection
            goto exit;
        }
	else {
	    if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
                DBG_TRACE("Hash authentication sucsessful for virtual player: %s" , ssp_cfg->player_name);
            }
	}
    }

    //Check for seek request
    get_query_arg_by_name(&phttp->hdr, ssp_cfg->seek.uri_query, strlen(ssp_cfg->seek.uri_query), &querySeek, &seekLen);

    if (ssp_cfg->seek.enable && ssp_cfg->seek.uri_query && querySeek != NULL) {

	if (ssp_cfg->seek.activate_tunnel && atoi(querySeek) > 0) { // Force tunnel logic for seek requests
	    delete_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI);
	    rv = -NKN_SSP_FORCE_TUNNEL_PATH;
	    goto exit;
	}

	if( (memcmp(uriData + remapLen - 4, ".flv", 4) == 0) || (memcmp(uriData + remapLen - 4, ".FLV", 4) == 0) ){ // FLV video file

	    queryrv = query_flv_seek(con, ssp_cfg->seek.uri_query, ssp_cfg->seek.query_length);
	    if ( queryrv < 0 ){
		rv = queryrv;
		goto exit;
	    }
	}
	else if ( (memcmp(uriData + remapLen - 4, ".mp4", 4) == 0) || (memcmp(uriData + remapLen - 4, ".MP4", 4) == 0) ){ // MP4 video file

	    queryrv = query_mp4_seek(con, ssp_cfg->seek.uri_query, contentLen, dataBuf, bufLen, 1); // timeUnits is 1 unit
	    if ( queryrv < 0 || queryrv == 1 || queryrv == 2) { // error or WAIT_MORE_DATA or SEND_TO_SSP
		rv = queryrv;
		goto exit;
	    }
	}
    }

    // Document the requested container format and update counters
    if( (memcmp(uriData + remapLen - 4, ".flv", 4) == 0) || (memcmp(uriData + remapLen - 4, ".FLV", 4) == 0) ){
        con->ssp.ssp_container_id = CONTAINER_TYPE_FLV;
        glob_ssp_container_flv++;
    }
    else if ( (memcmp(uriData + remapLen - 4, ".mp4", 4) == 0) || (memcmp(uriData + remapLen - 4, ".MP4", 4) == 0) ){
        con->ssp.ssp_container_id = CONTAINER_TYPE_MP4;
        glob_ssp_container_mp4++;
    }
    else {
        DBG_LOG(MSG, MOD_SSP, "Requested container format not supported. Passing the file through (escape ssp): %d", NKN_SSP_CONTAINER_FRMT_NOT_SUPP);
	rv = SSP_SEND_DATA_OTW;
	goto exit;
    }

    // Set the fast start buffer size
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
                    DBG_LOG(WARNING, MOD_SSP, "Fast start buffer in time units not supported");
                }
                break;
            default:
                break;
        }

        if (initBufSize > 0) {
            // Set the initial client buffer size for fast start
            con->max_faststart_buf = initBufSize;
            DBG_LOG(MSG, MOD_SSP, "Fast start buffer set to %ld bytes", con->max_faststart_buf );
	    if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
                DBG_TRACE("Fast start buffer size set to %ld bytes" , con->max_faststart_buf);
            }
        }
        else if (initBufSize < 0) {
	    DBG_LOG(MSG, MOD_SSP, "Fast start not enabled");
            if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
                DBG_TRACE("Fast start not enabled");
            }
            rv = initBufSize;
            goto exit;
        }
    }

#if 0
    //Throttle to AFR (Param for BW throttling)
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
                autoAFR = 1; // this will skip setting AFR from Virtual player mode
                break;

            case PARAM_ENABLE_TYPE_SIZE:
            case PARAM_ENABLE_TYPE_TIME:
                break;

        }

	rv = setAFRate(con, ssp_cfg, steadyRate);
        if (rv < 0)
            goto exit;

    } else { // AFR is disabled, so if CMB is specified set AFR to this value
	if (ssp_cfg->con_max_bandwidth > 0) {
	    con->max_bandwidth = ssp_cfg->con_max_bandwidth;
	}
    }
#else
    // Throttle to Rate Control (Param for BW throttling)
    rv = ssp_set_rc(con, ssp_cfg);
    if (rv < 0) {
	goto exit;
    }
#endif
    con->ssp.ssp_activated = TRUE;
    rv = SSP_SEND_DATA_OTW;

 exit:
    return rv;

}

#if 0
/*
 *
 * Query for MP4 Seek/Scrub using Time Offsets
 *
 */
int query_mp4_seek(con_t *con, const char *queryName, const char *uriData, int uriLen, off_t contentLen, const char *dataBuf, int bufLen)
{
    const char *queryData;
    bitstream_state_t *bs;
    int queryLen=0;
    int queryNameLen=0, remapLen=0;

    int64_t timeOffset=0, bytesWritten = 0;

    uint64_t moov_offset=0, moov_size=0, mdat_offset=0;
    moov_t *moov = NULL;
    int rv=0, traks = 0, seek_ret = 0;
    bs = NULL;
    http_cb_t *phttp = &con->http;

    queryData = NULL;
    queryNameLen = strlen(queryName);
    remapLen = remapLen;

    get_query_arg_by_name(&phttp->hdr, queryName, queryNameLen, &queryData, &queryLen);

    if(queryData){ //Seek query was found

        queryData != NULL ? (timeOffset = atoi(queryData)) : (timeOffset = 0); // in seconds
	DBG_LOG(MSG, MOD_SSP, "Entered mp4 seek time = %d mp4-hdr-size %d", timeOffset, con->ssp.mp4_header_size);

	if (timeOffset == 0) {
	    rv = SSP_SEND_DATA_OTW;
            goto exit;
	} else	if (timeOffset < 0){
            DBG_LOG(WARNING, MOD_SSP, "Seek time offset parameter is negative. %s=%ld (Error: %d)", queryName, timeOffset, NKN_SSP_BAD_QUERY_OFFSET);
            rv = -NKN_SSP_BAD_QUERY_OFFSET;  //Close connection
            goto exit;
	}

	if ( contentLen == OM_STAT_SIG_TOT_CONTENT_LEN ) { // file was not found
	    DBG_LOG(WARNING, MOD_SSP, "MP4Seek: Requested file for seek was not found, re-try and then just forward to client. Disable seek");
	    if (CHECK_HTTP_FLAG(phttp, HRF_BYTE_RANGE)) {
                CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
                phttp->brstart = 0;
                phttp->brstop = 0;
            }

            rv = SSP_SEND_DATA_OTW;
            goto exit;
        } else if ( !con->ssp.mp4_seek_state && contentLen == 0 && dataBuf == NULL ) { // First entry, to initiate internal loop-back for MP4 header fetch
	    DBG_LOG(MSG, MOD_SSP, "Requested time offset for MP4 seek is %d seconds", timeOffset);

	    con->ssp.mp4_seek_state = 1; // Persistent state variable
	    generate_om_seek_uri(con, uriData, uriLen, queryName); // Create the SEEK URI for OM

	    if (CHECK_HTTP_FLAG(phttp, HRF_BYTE_RANGE)) { // Clear any byte range requests sent by flash player
                CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
                phttp->brstart = 0;
                phttp->brstop = 0;
            }

	    DBG_LOG(MSG, MOD_SSP, "MP4Seek:  First request to fetch the file internally to parse and re-base the headers");
	    rv = SSP_SEND_DATA_BACK;
	    goto exit;
	} else if ( con->ssp.mp4_seek_state == 1 ||
		    con->ssp.mp4_seek_state == 2 ||
		    con->ssp.mp4_seek_state == 3) { // Second entry, to determine how much we need to get entire MP4 header

	    if ( bufLen < contentLen && con->ssp.ssp_partial_content_len < contentLen && bufLen > 0) {

		// Do the buffered read
		if(!con->ssp.mp4_bufered_data) {
		    con->ssp.mp4_header = (uint8_t *)nkn_calloc_type(MP4_INIT_BYTE_RANGE_LEN, sizeof(char),  mod_ssp_mp4_rebased_seekheader_t);
		    if (con->ssp.mp4_header == NULL) {
			DBG_LOG(MSG, MOD_HTTP, "MP4Seek: Failed to allocate space for MP4 header");
			CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
			rv = SSP_SEND_DATA_OTW;
			goto exit;
		    }
		    con->ssp.mp4_bufered_data = bio_init_bitstream(con->ssp.mp4_header, MP4_INIT_BYTE_RANGE_LEN);
		    con->ssp.mp4_header_size = MP4_INIT_BYTE_RANGE_LEN;
		}
		bytesWritten = bio_aligned_write_block(con->ssp.mp4_bufered_data, (uint8_t*)dataBuf, bufLen);

		if( (bio_get_pos(con->ssp.mp4_bufered_data) > con->ssp.mp4_header_size) ) {
		    switch(con->ssp.mp4_seek_state) {
			case 1:
			    bs = (bitstream_state_t*)con->ssp.mp4_bufered_data;
			    mp4_get_moov_info((uint8_t*)bs->data, MP4_INIT_BYTE_RANGE_LEN, &moov_offset, &moov_size);
			    if(moov_offset == 0 || moov_size == 0) {
				DBG_LOG(WARNING, MOD_HTTP, "MP4Seek: Corrupted moov offset or size. Will revert to serving full file instead");
				CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
				rv = SSP_SEND_DATA_OTW;
				goto exit;
			    }
			    //con->ssp.mp4_header = (uint8_t *)nkn_realloc_type(con->ssp.mp4_header, moov_offset + moov_size + 8, mod_ssp_mp4_rebased_seekheader_t);
			    con->ssp.mp4_header_size = moov_offset + moov_size + 8;
			    //bio_reinit_bitstream(bs, con->ssp.mp4_header, moov_offset + moov_size + 8);

			    if ( bytesWritten < 0 ) {
				con->ssp.mp4_header = (uint8_t *)nkn_realloc_type(con->ssp.mp4_header, bufLen + bs->size, mod_ssp_mp4_rebased_seekheader_t);
				if (con->ssp.mp4_header == NULL) {
				    DBG_LOG(MSG, MOD_HTTP, "MP4Seek: Failed to allocate seek headers");
				    CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
				    rv = SSP_SEND_DATA_OTW;
				    goto exit;
				}
				bio_reinit_bitstream(bs, con->ssp.mp4_header, bufLen + bs->size);
				bio_aligned_seek_bitstream(bs, con->ssp.ssp_partial_content_len, SEEK_SET);
				bytesWritten = bio_aligned_write_block(con->ssp.mp4_bufered_data, (uint8_t*)dataBuf, bufLen);
			    }

			    con->ssp.mp4_seek_state = 2;
			    break;
			case 2:
			    bs = (bitstream_state_t*)con->ssp.mp4_bufered_data;
			    mp4_get_moov_info((uint8_t*)bs->data, MP4_INIT_BYTE_RANGE_LEN, &moov_offset, &moov_size);

			    moov = mp4_init_moov((uint8_t *)bs->data+moov_offset, moov_offset, moov_size);
			    traks = mp4_populate_trak(moov);

			    if(traks > 2 || traks <= 0) {
				DBG_LOG(WARNING, MOD_HTTP, "MP4Seek: Only 2 media tracks are supported (Tracks in file = %d). Will revert to serving full file instead", traks);
                                CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
                                rv = SSP_SEND_DATA_OTW;
                                goto exit;
			    }

			    mdat_offset = moov_offset + moov_size + (seek_ret = mp4_seek_trak(moov, traks, timeOffset));

			    if(seek_ret < 0) {
				DBG_LOG(WARNING, MOD_HTTP, "MP4Seek: Invalid request for seek, maybe outside the timeline (seektime = %d)", timeOffset);
                                CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
                                rv = SSP_SEND_DATA_OTW;
                                goto exit;
			    }
			    // write mdat headers at the end of MOOV box
			    writeMDAT_Header((char*)(bs->data + moov_size - 8), phttp->content_length - mdat_offset - 8);
			    con->ssp.mp4_header = (uint8_t *)nkn_realloc_type(con->ssp.mp4_header, moov_offset + moov_size + 8, mod_ssp_mp4_rebased_seekheader_t);
			    if (con->ssp.mp4_header == NULL) {
				DBG_LOG(MSG, MOD_HTTP, "MP4Seek: Failed to allocate seek headers");
				CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
				rv = SSP_SEND_DATA_OTW;
				goto exit;
			    }
			    con->ssp.mp4_mdat_offset = mdat_offset;
			    con->ssp.mp4_header_size = moov_offset + moov_size + 8;
			    con->ssp.mp4_seek_state = 3;

			    goto escapemp4;
			    break;
		    }
		} else { // looped buffered read
		    bs = (bitstream_state_t*)con->ssp.mp4_bufered_data;
		    if ( bytesWritten < 0 ) {
			con->ssp.mp4_header = (uint8_t *)nkn_realloc_type(con->ssp.mp4_header, bufLen + bs->size, mod_ssp_mp4_rebased_seekheader_t);
			if (con->ssp.mp4_header == NULL) {
			    DBG_LOG(MSG, MOD_HTTP, "MP4Seek: Failed to allocate seek headers");
			    CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
			    rv = SSP_SEND_DATA_OTW;
			    goto exit;
			}
			bio_reinit_bitstream(bs, con->ssp.mp4_header, bufLen + bs->size);
			bio_aligned_seek_bitstream(bs, con->ssp.ssp_partial_content_len, SEEK_SET);
			bytesWritten = bio_aligned_write_block(con->ssp.mp4_bufered_data, (uint8_t*)dataBuf, bufLen);
			//bio_aligned_seek_bitstream(bs, bufLen, SEEK_SET);
		    }
		}

		con->ssp.ssp_partial_content_len += bufLen;
		/*		    if(con->ssp.mp4_seek_state == 3) {
		  printf("partial content len %d. http content length %ld\n", con->ssp.ssp_partial_content_len, phttp->content_length);
		  if(con->ssp.ssp_partial_content_len == phttp->content_length) { 
		  mdat_offset = con->ssp.mp4_mdat_offset;
		  goto escapemp4;
		  }

		  }*/

		rv = SSP_WAIT_FOR_DATA;
                goto exit;
	    }

	escapemp4: // All data required for re-basing has been obtained
	    // Now set the correct SEEK_RANGE_REQUEST, generate the pre-pend header and SEND_OTW
	    if (CHECK_HTTP_FLAG(phttp, HRF_BYTE_RANGE)) {
		CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
		phttp->brstart = 0;
		phttp->brstop = 0;
	    }
	    // This is now a HRF_SEEK_REQUEST to generate valid 206 response
	    //http_set_seek_request(phttp, mdat_offset+8, mdat_offset+8 + (phttp->content_length - mdat_offset - 8 - 1));
	    http_set_seek_request(phttp, mdat_offset+8, 0);

	    con->ssp.ssp_activated = TRUE;
	    rv = SSP_SEND_DATA_OTW;
	    goto exit;
	}
    }//end if queryData

 exit:
    return rv;
}

void
writeMDAT_Header(char *p_src, uint64_t mdat_size)
{

	int32_t mdat_id = 0x6d646174;

	memcpy(p_src, &mdat_size, 4);
	memcpy(p_src + 4, &mdat_id, 4);

}

#endif
