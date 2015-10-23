/*
 *******************************************************************************
 * ssp.c -- Server Side Player
 *******************************************************************************
 */

#include <string.h>
#include <stdlib.h>
#include <openssl/md5.h>
#include <alloca.h>
#include <pthread.h>
#include <glib.h>

#include "ssp.h"
#include "nkn_ssp_players.h"
#include "ssp_authentication.h"
#include "ssp_queryparams.h"
#include "http_header.h"
#include "nkn_errno.h"
#include "nkn_cfg_params.h"
#include "nkn_debug.h"
#include "nkn_common_config.h"
#include "nkn_stat.h"
#include "nkn_vpe_metadata.h"
#include "nkn_ssp_sessionbind.h"
#include "nkn_vpe_mp4_seek_api.h"
//#include "nkn_vpe_mp4_parser.h"

#ifndef FLV_HEADER_SIZE
#define FLV_HEADER_SIZE 0x000000000000000D
#endif
#if 0
#define FLV_HD_SIZE_MEGAVIDEO 0x0000000000000009
#endif
const char *FLV_HEADER =  "FLV\x1\x1\0\0\0\x9\0\0\0\x9";
#if 0
const char *FLV_HD_MEGAVIDEO = "FLV\x1\x5\0\0\0\x9";
#endif

unsigned long long glob_ssp_hash_failures;
unsigned long long glob_ssp_force_tunnel;
unsigned long long glob_ssp_container_flv, glob_ssp_container_mp4, glob_ssp_container_asf,
    glob_ssp_container_webm, glob_ssp_container_3gpp, glob_ssp_container_3gp2;
unsigned long long glob_ssp_pacing_flv_failure, glob_ssp_pacing_flv_success,
    glob_ssp_pacing_mp4_failure, glob_ssp_pacing_mp4_success,
    glob_ssp_pacing_asf_failure, glob_ssp_pacing_asf_success,
    glob_ssp_pacing_webm_failure, glob_ssp_pacing_webm_success,
    glob_ssp_pacing_3gpp_failure, glob_ssp_pacing_3gpp_success,
    glob_ssp_pacing_3gp2_failure, glob_ssp_pacing_3gp2_success;


unsigned long long glob_ssp_unrecognized_uri;
unsigned long long ssp_vplayer_type[MAX_VIRTUAL_PLAYER_TYPES];
unsigned long long glob_normalizer_ssp;

///////////////////
// SSP Init Routine
int SSP_init(void)
{
    int i;
    char *strbuf = NULL;
    int strSize=100; // max string size
    char vplayer_cnt_str[] = "vplayer_type";

    strbuf = (char *)alloca(strSize);

    // Registering and initializing counters
    for (i=0; i<MAX_VIRTUAL_PLAYER_TYPES; i++){
    	ssp_vplayer_type[i] = 0;

	memset(strbuf, 0, strSize);
	snprintf(strbuf, strSize, "%s_%d", vplayer_cnt_str, i);

	(void)nkn_mon_add(strbuf, NULL, (void *) &ssp_vplayer_type[i], sizeof(unsigned long long));
    }

    glob_ssp_hash_failures = 0;
    glob_ssp_container_flv = 0;
    glob_ssp_container_mp4 = 0;
    glob_ssp_container_asf = 0;
    glob_ssp_container_webm= 0;
    glob_ssp_container_3gpp = 0;
    glob_ssp_container_3gp2 = 0;
    glob_ssp_pacing_flv_failure = 0;
    glob_ssp_pacing_flv_success = 0;
    glob_ssp_pacing_mp4_failure = 0;
    glob_ssp_pacing_mp4_success = 0;
    glob_ssp_pacing_asf_failure = 0;
    glob_ssp_pacing_asf_success = 0;
    glob_ssp_pacing_webm_failure = 0;
    glob_ssp_pacing_webm_success = 0;
    glob_ssp_pacing_3gpp_failure = 0;
    glob_ssp_pacing_3gpp_success = 0;
    glob_ssp_pacing_3gp2_failure = 0;
    glob_ssp_pacing_3gp2_success = 0;


    glob_ssp_unrecognized_uri = 0;
    glob_ssp_force_tunnel = 0;
    // Initialize the Hash Table for Session Binding
    nkn_ssp_sessbind_init();

  return 0;
}


/*
 * startSSP
 *     Return values:
 *	3 return 200 OK
 *	2 return the data buffer back to SSP and do not send over the wire
 *      1 SSP is still waiting for all the pieces of .txt to arrive
 *	0 Fetch and send data over the wire
 *    < 0 Error
 *	Please see nkn_errno.h for error definitions
 */
#if 0
int
startSSP(con_t 		*con,
	 off_t 		contentLen,
	 const char 	*dataBuf,
	 int 		bufLen)
{
    const char *tmpData;
    char *uriData;
    int uriLen=0, uriCnt=0;
    unsigned int uriAttrs=0;
    int rv=0;

    http_cb_t *phttp = &con->http;

    /* Get a handle on the Header URI, so that SSP can do preliminary
     * investigation to determine customer type, such that specific
     * logic applicable to each customer can be applied henceforth
     */

    // Get Host Header
    //    if (get_known_header(&phttp->hdr , MIME_HDR_HOST, &hostData, &hostLen, &hostAttrs, &hostCnt)) {
    //	return -NKN_SSP_BAD_URL;
    //}

    // Get URI
    if (get_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI, &tmpData, &uriLen, &uriAttrs, &uriCnt)) {// BZ: 8355

	if (get_known_header(&phttp->hdr, MIME_HDR_X_NKN_DECODED_URI, &tmpData, &uriLen, &uriAttrs, &uriCnt)) {
	    if (get_known_header(&phttp->hdr , MIME_HDR_X_NKN_URI, &tmpData, &uriLen, &uriAttrs, &uriCnt)) {
		return -NKN_SSP_BAD_URL;
	    }
	}

    }
    /* copy get_known_header()'s tmpData to a local memory for safety*/
    uriData = (char *)alloca(uriLen + 1);
    memset(uriData, 0, uriLen + 1);
    uriData[0] = '\0';
    strncat(uriData, tmpData, uriLen);

    // Identify Customer
    rv = selectSSP(con, uriData, uriLen, contentLen, dataBuf, bufLen);

    if (rv == -NKN_SSP_FORCE_TUNNEL_PATH) {
	cleanSSP_force_tunnel(con);
    }
    return rv;
}
#endif

int
startSSP(con_t  	*con,
	  off_t 	contentLen,
	  const char 	*dataBuf,
	  int 		bufLen)
{
  int rv=0;
  const char *hostData = NULL;
  int hostLen=0, hostCnt;
  unsigned int hostAttrs=0;
  const namespace_config_t *cfg;
  const ssp_config_t *ssp_cfg;

  const char *tmpData=NULL;
  char *uriData=NULL;
  int uriLen=0, uriCnt=0;
  unsigned int uriAttrs=0;

  http_cb_t *phttp = &con->http;
  phttp->p_ssp_cb = &con->ssp;

  cfg = phttp->nsconf;

  if(cfg == NULL){
      rv = 10;
      DBG_LOG(MSG, MOD_SSP, "namespace_to_config() ret=NULL; rv=%d", rv);
      return rv;
  }

  ssp_cfg = cfg->ssp_config;

  DBG_LOG(MSG2, MOD_SSP, "Virtual Player: Type:  %d has been invoked" , ssp_cfg->player_type);

  // For Tier-2 Nodes, check for presence of Cache Index header. If present bypass all SSP code path
  if (is_known_header_present(&phttp->hdr, MIME_HDR_X_NKN_CLUSTER_TPROXY)) {
      DBG_LOG(WARNING, MOD_SSP, "Bypassing SSP for Tier-2 Cluster Nodes");
      return 0;
  }

  /* Get a handle on the Header URI, so that SSP can do preliminary
   * investigation to determine customer type, such that specific
   * logic applicable to each customer can be applied henceforth
   */
  // Get URI
  if (get_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI, &tmpData, &uriLen, &uriAttrs, &uriCnt)) {// BZ: 8355
      if (get_known_header(&phttp->hdr, MIME_HDR_X_NKN_DECODED_URI, &tmpData, &uriLen, &uriAttrs, &uriCnt)) {
	  if (get_known_header(&phttp->hdr , MIME_HDR_X_NKN_URI, &tmpData, &uriLen, &uriAttrs, &uriCnt)) {
	      rv = -NKN_SSP_BAD_URL;
	      goto exit;
	  }
      }
  }
  /* copy get_known_header()'s tmpData to a local memory for safety*/
  uriData = (char *)alloca(uriLen + 1);
  memset(uriData, 0, uriLen + 1);
  uriData[0] = '\0';
  strncat(uriData, tmpData, uriLen);

  switch (ssp_cfg->player_type) {
      case 0:  // Player type 0 - Generic Nokeena Player
	  if (ssp_cfg->hash_verify.enable &&
	      ssp_cfg->hash_verify.url_type &&
	      strcmp(ssp_cfg->hash_verify.url_type, "absolute-url")==0 ) {
	      if (get_known_header(&phttp->hdr , MIME_HDR_HOST,
				   &hostData, &hostLen, &hostAttrs, &hostCnt)) {         // Get Host Header
		  rv = -NKN_SSP_BAD_URL;
		  goto exit;
	      }
	  } else {
              if (get_known_header(&phttp->hdr , MIME_HDR_HOST,
                                   &hostData, &hostLen, &hostAttrs, &hostCnt)) {         // Get Host Header
                  hostData = NULL;
		  hostLen = 0;
              }
	  }

	  rv = nknGenericSSP(con, uriData, uriLen, hostData, hostLen, ssp_cfg, contentLen, dataBuf, bufLen);

	  con->ssp.ssp_client_id = ssp_cfg->player_type;
	  ssp_vplayer_type[ssp_cfg->player_type]++;
	  if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
	      DBG_TRACE("Virtual Player: %s of Type-%d has been invoked" , ssp_cfg->player_name, ssp_cfg->player_type);
	  }
	  SET_HTTP_FLAG(&con->http, HRF_SSP_CONFIGURED);
	  con->module_type = SSP;
	  glob_normalizer_ssp++;

	  break;

#if 0
      case 1:  // Player type 1 - Break Player
	  if (ssp_cfg->hash_verify.enable &&
	      ssp_cfg->hash_verify.url_type &&
	      strcmp(ssp_cfg->hash_verify.url_type, "absolute-url")==0 ) {
	      if (get_known_header(&phttp->hdr , MIME_HDR_HOST,
				   &hostData, &hostLen, &hostAttrs, &hostCnt)) {         // Get Host Header
		  rv = -NKN_SSP_BAD_URL;
		  goto exit;
	      }
	  } else {
	      hostData = NULL;
	      hostLen=0;
	  }

	  rv = breakSSP(con, uriData, uriLen, hostData, hostLen, ssp_cfg, contentLen, dataBuf, bufLen);

	  con->ssp.ssp_client_id = ssp_cfg->player_type;
	  ssp_vplayer_type[ssp_cfg->player_type]++;
	  if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
	      DBG_TRACE("Virtual Player: %s of Type-%d has been invoked" , ssp_cfg->player_name, ssp_cfg->player_type);
	  }
	  SET_HTTP_FLAG(&con->http, HRF_SSP_CONFIGURED);
	  con->module_type = SSP;
	  glob_normalizer_ssp++;

	  break;

      case 2:  // Player type 2 - Move Player
	  rv = moveSSP(con, uriData, uriLen, ssp_cfg);

	  con->ssp.ssp_client_id = ssp_cfg->player_type;
	  ssp_vplayer_type[ssp_cfg->player_type]++;
	  if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
	      DBG_TRACE("Virtual Player: %s of Type-%d has been invoked" , ssp_cfg->player_name, ssp_cfg->player_type);
	  }
	  SET_HTTP_FLAG(&con->http, HRF_SSP_CONFIGURED);
	  con->module_type = SSP;
	  glob_normalizer_ssp++;

	  break;
#endif

      case 3:  // Player type 3 - Yahoo Player
	  if (ssp_cfg->hash_verify.enable &&
	      ssp_cfg->hash_verify.url_type &&
	      strcmp(ssp_cfg->hash_verify.url_type, "absolute-url")==0 ) {
	      if (get_known_header(&phttp->hdr , MIME_HDR_HOST,
				   &hostData, &hostLen, &hostAttrs, &hostCnt)) {         // Get Host Header
		  rv = -NKN_SSP_BAD_URL;
		  goto exit;
	      }
	  } else {
	      hostData = NULL;
	      hostLen=0;
	  }

	  rv = yahooSSP(con, uriData, uriLen, hostData, hostLen, ssp_cfg);

	  con->ssp.ssp_client_id = ssp_cfg->player_type;
	  ssp_vplayer_type[ssp_cfg->player_type]++;
	  if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
	      DBG_TRACE("Virtual Player: %s of Type-%d has been invoked" , ssp_cfg->player_name, ssp_cfg->player_type);
	  }
	  SET_HTTP_FLAG(&con->http, HRF_SSP_CONFIGURED);
	  con->module_type = SSP;
	  glob_normalizer_ssp++;

	  break;

#if 0
      case 4: // Player type 4 - Nokeena Smoothflow Player
	  // For smoothflow, host header is mandatory to allow ingest/publish to locate the right namespace
	  if (get_known_header(&phttp->hdr , MIME_HDR_HOST,
			       &hostData, &hostLen, &hostAttrs, &hostCnt)) {         // Get Host Header
	      rv = -NKN_SSP_BAD_URL;
	      goto exit;
	  }

	  rv = smoothflowSSP(con, uriData, uriLen, hostData, hostLen, ssp_cfg, contentLen, dataBuf, bufLen);

	  con->ssp.ssp_client_id = ssp_cfg->player_type;
	  ssp_vplayer_type[ssp_cfg->player_type]++;
	  if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
	      DBG_TRACE("Virtual Player: %s of Type-%d has been invoked" , ssp_cfg->player_name, ssp_cfg->player_type);
	  }
	  SET_HTTP_FLAG(&con->http, HRF_SSP_CONFIGURED);
	  con->module_type = SSP;
	  glob_normalizer_ssp++;

	  break;
#endif

      case 5:  // Player type 5 - Youtube Player
	  if (get_known_header(&phttp->hdr , MIME_HDR_HOST,
			       &hostData, &hostLen, &hostAttrs, &hostCnt)) {         // Get Host Header
	      rv = -NKN_SSP_BAD_URL;
	      goto exit;
	  }
	  rv = youtubeSSP(con, uriData, uriLen, hostData, hostLen, ssp_cfg, cfg, contentLen, dataBuf, bufLen);

	  con->ssp.ssp_client_id = ssp_cfg->player_type;
	  ssp_vplayer_type[ssp_cfg->player_type]++;
	  if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
	      DBG_TRACE("Virtual Player: %s of Type-%d has been invoked" , ssp_cfg->player_name, ssp_cfg->player_type);
	  }
	  SET_HTTP_FLAG(&con->http, HRF_SSP_CONFIGURED);
	  con->module_type = SSP;
	  glob_normalizer_ssp++;

	  break;

      case 6:  // Player/Publisher type 6 - Smooth streaming publisher
	  rv = smoothstreamingSSP(con, uriData, uriLen, ssp_cfg, contentLen, dataBuf, bufLen);

	  con->ssp.ssp_client_id = ssp_cfg->player_type;
	  ssp_vplayer_type[ssp_cfg->player_type]++;
	  if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
	      DBG_TRACE("Virtual Player: %s of Type-%d has been invoked" , ssp_cfg->player_name, ssp_cfg->player_type);
	  }

	  SET_HTTP_FLAG(&con->http, HRF_SSP_CONFIGURED);
	  con->module_type = SSP;
	  glob_normalizer_ssp++;

	  break;

      case 7:  // Player/Publisher type 7 - Flash Zeri streaming publisher
	  rv = ssp_zeri_streamingSSP(con, uriData, uriLen, ssp_cfg, contentLen, dataBuf, bufLen);

	  con->ssp.ssp_client_id = ssp_cfg->player_type;
	  ssp_vplayer_type[ssp_cfg->player_type]++;
	  if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
	      DBG_TRACE("Virtual Player: %s of Type-%d has been invoked" , ssp_cfg->player_name, ssp_cfg->player_type);
	  }

	  SET_HTTP_FLAG(&con->http, HRF_SSP_CONFIGURED);
	  con->module_type = SSP;
	  glob_normalizer_ssp++;

	  break;

      default: // This is for fall through when nothing matches
	  glob_ssp_unrecognized_uri++;

	  DBG_LOG(WARNING, MOD_SSP, "No Virtual Player associated with namespace - %s. Forwarding request %s to BM", cfg->namespace, uriData);
	  if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
	      DBG_TRACE("No Virtual Player associated with namespace - %s", cfg->namespace);
	  }

	  if ( sess_assured_flow_rate == 0) {
	      rv = SSP_SEND_DATA_OTW;
	  }
	  else if ( sess_assured_flow_rate < MIN( (signed)con->max_client_bw, (signed) con->max_allowed_bw) ) {
	      con_set_afr(con, sess_assured_flow_rate);
	      DBG_LOG(MSG, MOD_SSP, "AFR Set: %ld", sess_assured_flow_rate);
	      rv = SSP_SEND_DATA_OTW;
	  }
	  else {
	      // Reject connection, since requested rate > available bandwidth
	      DBG_LOG(WARNING, MOD_SSP, "AFR Failed: %ld Unavailable BW", sess_assured_flow_rate);
	      rv =  -NKN_SSP_AFR_INSUFF;
	  }

	  break;
  }

exit:
  if (rv == -NKN_SSP_FORCE_TUNNEL_PATH) {
      cleanSSP_force_tunnel(con);
      glob_ssp_force_tunnel++;
  }

  return rv;
}

/*
 * updateSSP
 * returns nothing
 * functionlity is to update the SSP state for a connection and add headers dynamically
 */
void
updateSSP(con_t *con)
{
    const namespace_config_t *cfg;
    const ssp_config_t *ssp_cfg;

    //Till we complete customer identification & video content identification
    http_cb_t *phttp = &con->http;

    cfg = phttp->nsconf;
    ssp_cfg = cfg->ssp_config;

    switch (con->ssp.ssp_client_id)
	{
	    case 0:  // Player type 0 - Generic Nokeena Player
		if(con->ssp.ssp_activated == TRUE && con->ssp.ssp_container_id == CONTAINER_TYPE_FLV) {

		    con->ssp.ssp_content_length = con->http.content_length;//save content length for future use

		    if(CHECK_HTTP_FLAG(phttp, HRF_BYTE_SEEK) && !CHECK_HTTP_FLAG(phttp, HRF_RES_HEADER_SENT)) {//means this is a scrubbed request

			if (ssp_cfg->seek.flv_off_type &&
			    strncmp(ssp_cfg->seek.flv_off_type, "byte-offset",
				    strlen("byte-offset")) == 0) {
			    if (!phttp->uri_token ||
				 (phttp->uri_token && !phttp->uri_token->token_flag)) {
				//read-only static data, just pass reference
				http_prepend_response_data(phttp, FLV_HEADER, FLV_HEADER_SIZE, 0);
			    }
#if 0
			    else if (!CHECK_UTF_FLAG(phttp->uri_token, UTF_HTTP_RANGE_START) &&
				     !CHECK_UTF_FLAG(phttp->uri_token, UTF_HTTP_RANGE_END) &&
				     CHECK_UTF_FLAG(phttp->uri_token, UTF_FLVSEEK_BYTE_START)) {
				//read-only static data, just pass reference
				http_prepend_response_data(phttp, FLV_HD_MEGAVIDEO, FLV_HD_SIZE_MEGAVIDEO, 0);
			    }
#endif
			} else {
			    http_prepend_response_data(phttp, (char*)con->ssp.header, con->ssp.header_size, 1);
			    con->ssp.header = 0;
			    con->ssp.header_size = 0;
			    con->ssp.seek_state = 0;
			    con->ssp.ssp_partial_content_len = 0;
			    if (con->ssp.fp_state) {
				flv_parser_cleanup(con->ssp.fp_state);
			    }
			    con->ssp.fp_state = NULL;
			}
		    }//end scrubbed request
		    con->ssp.transcode_state = 0;
		    con->ssp.ssp_container_filetype = 0;
		    con->ssp.ssp_activated = FALSE;
		    con->ssp.ssp_hash_authed = 0; /* reset for each request in keep-alive*/

		    /* afr related*/
		    con->ssp.attr_state = 0;
		    con->ssp.ssp_attr_partial_content_len = 0;
		    con->ssp.afr_header_size = 0;
		    con->ssp.afr_header = NULL;
		    con->ssp.afr_buffered_data = NULL;
		    con->ssp.ssp_video_br_flag = 0;
		    con->ssp.client_brstart = 0;
		    con->ssp.client_brstop  = 0;

		    con->ssp.ssp_callback_status = 0;
		}//end ssp activated
		else if (con->ssp.ssp_activated == TRUE &&
			 (con->ssp.ssp_container_id == CONTAINER_TYPE_MP4 ||
			  con->ssp.ssp_container_id == CONTAINER_TYPE_3GPP ||
			  con->ssp.ssp_container_id == CONTAINER_TYPE_3GP2)) {

                    if(CHECK_HTTP_FLAG(phttp, HRF_BYTE_SEEK) && !CHECK_HTTP_FLAG(phttp, HRF_RES_HEADER_SENT)) {//means this is a scrubbed request
			http_prepend_response_data(phttp, (char*)con->ssp.mp4_header, con->ssp.mp4_header_size, 1);
			con->ssp.mp4_header = 0;
			con->ssp.mp4_header_size = 0;
			con->ssp.seek_state = 0;
			con->ssp.ssp_partial_content_len = 0;
			con->ssp.mp4_mdat_offset = 0;
			con->ssp.buffered_data = NULL;
                    }
		    con->ssp.transcode_state = 0;
		    con->ssp.ssp_container_filetype = 0;
                    con->ssp.ssp_activated = FALSE;
		    con->ssp.ssp_hash_authed = 0; /* reset for each request in keep-alive*/

		    /* afr related */
		    con->ssp.attr_state = 0;
		    con->ssp.ssp_attr_partial_content_len = 0;
		    con->ssp.afr_header_size = 0;
		    con->ssp.afr_header = NULL;
		    con->ssp.afr_buffered_data = NULL;
		    con->ssp.ssp_video_br_flag = 0;
		    con->ssp.client_brstart = 0;
		    con->ssp.client_brstop  = 0;
                } else if (con->ssp.ssp_activated == TRUE && con->ssp.ssp_container_id == CONTAINER_TYPE_ASF) {
		    con->ssp.transcode_state        = 0;
		    con->ssp.ssp_container_filetype = 0;
		    con->ssp.ssp_activated          = FALSE;
		    con->ssp.ssp_hash_authed        = 0; /* reset for each request in keep-alive*/

		    /* afr related */
		    con->ssp.attr_state = 0;
		    con->ssp.ssp_attr_partial_content_len = 0;
		    con->ssp.afr_header_size = 0;
		    con->ssp.afr_header = NULL;
		    con->ssp.afr_buffered_data = NULL;
		    con->ssp.ssp_video_br_flag = 0;
		    con->ssp.client_brstart = 0;
		    con->ssp.client_brstop  = 0;

		    con->ssp.ssp_callback_status = 0;
		} else if (con->ssp.ssp_activated == TRUE &&
			   con->ssp.ssp_container_id == CONTAINER_TYPE_WEBM) {
		    con->ssp.transcode_state        = 0;
		    con->ssp.ssp_container_filetype = 0;
		    con->ssp.ssp_activated          = FALSE;
		    con->ssp.ssp_hash_authed        = 0;

		    /* afr related */
		    con->ssp.attr_state = 0;
		    con->ssp.ssp_attr_partial_content_len = 0;
                    con->ssp.afr_header_size = 0;
                    con->ssp.afr_header = NULL;
                    con->ssp.afr_buffered_data = NULL;
                    con->ssp.client_brstart = 0;
                    con->ssp.client_brstop  = 0;
		}
		break;

	    case 1:  // Player type 1 - Break Player
		if(con->ssp.ssp_activated == TRUE && con->ssp.ssp_container_id == CONTAINER_TYPE_FLV) {
		    con->ssp.ssp_content_length = con->http.content_length;//save content length for future use

		    if(CHECK_HTTP_FLAG(phttp, HRF_BYTE_SEEK) && !CHECK_HTTP_FLAG(phttp, HRF_RES_HEADER_SENT)) {//means this is a scrubbed request
			http_prepend_response_data(phttp, FLV_HEADER, FLV_HEADER_SIZE, 0);//read-only static data, just pass reference
		    }//end scrubbed request
		}//end ssp activated
		else if (con->ssp.ssp_activated == TRUE && con->ssp.ssp_container_id == CONTAINER_TYPE_MP4) {

		    if(CHECK_HTTP_FLAG(phttp, HRF_BYTE_SEEK) && !CHECK_HTTP_FLAG(phttp, HRF_RES_HEADER_SENT)) {//means this is a scrubbed request
			    http_prepend_response_data(phttp, (char*)con->ssp.mp4_header, con->ssp.mp4_header_size, 1);
			    con->ssp.mp4_header = 0;
			    con->ssp.mp4_header_size = 0;
			    con->ssp.seek_state = 0;
			    con->ssp.ssp_partial_content_len = 0;
			    con->ssp.mp4_mdat_offset = 0;
			    con->ssp.buffered_data = NULL;
                    }
		    con->ssp.ssp_activated = FALSE;
		}
		break;

	    case 2:  // Player type 2 - Move Player
		// No seek support for Move, since it is inherent in their player operation
		break;

	    case 3:  // Player type 3 - Yahoo Player
		if(con->ssp.ssp_activated == TRUE && con->ssp.ssp_container_id == CONTAINER_TYPE_FLV) {

		    con->ssp.ssp_content_length = con->http.content_length;//save content length for future use

		    if(CHECK_HTTP_FLAG(phttp, HRF_BYTE_SEEK) && !CHECK_HTTP_FLAG(phttp, HRF_RES_HEADER_SENT)) {//means this is a scrubbed request
			http_prepend_response_data(phttp, FLV_HEADER, FLV_HEADER_SIZE, 0);//read-only static data, just pass reference
		    }//end scrubbed request
		}//end ssp activated

		break;

	    case 4:  // Player type 4 - Nokeena Smoothflow Player
		// No exit part update logic for smoothflow yet. Just pass through
                if(con->ssp.ssp_scrub_req){
		    char  *prepend_data = NULL;
		    avcc_config *avcc = con->ssp.private_data;

		    if(con->ssp.ssp_codec_id & VCODEC_ID_AVC){
			prepend_data = (char*)nkn_malloc_type(FLV_HEADER_SIZE + avcc->avcc_data_size,
								mod_ssp_prepend_data_t);
			memcpy(prepend_data, FLV_HEADER, FLV_HEADER_SIZE);
			memcpy(prepend_data + FLV_HEADER_SIZE, avcc->p_avcc_data, avcc->avcc_data_size);
		    	http_prepend_response_data(phttp, prepend_data, FLV_HEADER_SIZE + avcc->avcc_data_size, 1);
		    } else {
		    	http_prepend_response_data(phttp, FLV_HEADER, FLV_HEADER_SIZE, 0); // read-only static data, just pass reference
		    }

		    con->ssp.ssp_scrub_req = 0;
                }

                break;

            case 5:  // Player type 5 - Youtube Player
                if(con->ssp.ssp_activated == TRUE && con->ssp.ssp_container_id == CONTAINER_TYPE_FLV) {
		    //Check for seek request
		    if(CHECK_HTTP_FLAG(phttp, HRF_BYTE_SEEK) && !CHECK_HTTP_FLAG(phttp, HRF_RES_HEADER_SENT)) {
			http_prepend_response_data(phttp, (char*)con->ssp.header, con->ssp.header_size, 1);
			con->ssp.header = 0;
			con->ssp.header_size = 0;
			con->ssp.seek_state = 0;
			con->ssp.ssp_partial_content_len = 0;
			if (con->ssp.fp_state) {
			    flv_parser_cleanup(con->ssp.fp_state);
			}
			con->ssp.fp_state = NULL;
		    }
		    con->ssp.transcode_state = 0;
		    con->ssp.ssp_activated = FALSE;

		    con->ssp.attr_state = 0;
		    con->ssp.rate_opt_state = 0;
		    con->ssp.serve_fmt = 0;
		    con->ssp.ssp_attr_partial_content_len = 0;
		    con->ssp.afr_header_size = 0;
		    con->ssp.afr_header = NULL;
		    con->ssp.afr_buffered_data = NULL;
		    con->ssp.ssp_video_br_flag = 0;
		    con->ssp.client_brstart = 0;
		    con->ssp.client_brstop  = 0;
                }
		else if (con->ssp.ssp_activated == TRUE &&
			 (con->ssp.ssp_container_id == CONTAINER_TYPE_MP4 ||
			  con->ssp.ssp_container_id == CONTAINER_TYPE_3GPP ||
			  con->ssp.ssp_container_id == CONTAINER_TYPE_3GP2)) {
		    // Check for seek request
		    if(CHECK_HTTP_FLAG(phttp, HRF_BYTE_SEEK) && !CHECK_HTTP_FLAG(phttp, HRF_RES_HEADER_SENT)) {
			http_prepend_response_data(phttp, (char*)con->ssp.mp4_header, con->ssp.mp4_header_size, 1);
			con->ssp.mp4_header = 0;
			con->ssp.mp4_header_size = 0;
			con->ssp.seek_state = 0;
			con->ssp.ssp_partial_content_len = 0;
			con->ssp.mp4_mdat_offset = 0;
			con->ssp.buffered_data = NULL;
		    }
		    con->ssp.transcode_state = 0;
		    con->ssp.ssp_activated = FALSE;

		    con->ssp.attr_state = 0;
		    con->ssp.rate_opt_state = 0;
		    con->ssp.serve_fmt = 0;
		    con->ssp.ssp_attr_partial_content_len = 0;
		    con->ssp.afr_header_size = 0;
		    con->ssp.afr_header = NULL;
		    con->ssp.afr_buffered_data = NULL;
		    con->ssp.ssp_video_br_flag = 0;
		    con->ssp.client_brstart = 0;
                    con->ssp.client_brstop  = 0;
		} else if (con->ssp.ssp_activated == TRUE && con->ssp.ssp_container_id == CONTAINER_TYPE_WEBM) {
		    con->ssp.ssp_activated = FALSE;

		    con->ssp.attr_state = 0;
		    con->ssp.rate_opt_state = 0;
		    con->ssp.serve_fmt = 0;
		    con->ssp.ssp_attr_partial_content_len = 0;
		    con->ssp.afr_header_size = 0;
		    con->ssp.afr_header = NULL;
		    con->ssp.afr_buffered_data = NULL;
		    con->ssp.ssp_video_br_flag = 0;
		    con->ssp.client_brstart = 0;
                    con->ssp.client_brstop  = 0;
		}
#ifdef ATTR_UPDATE_UNIT_TEST
		nkn_uol_t uol = { 0,0,0 };
		nkn_attr_t *ap = NULL;
		uol.cod = con->http.req_cod;
		nkn_buffer_t *abuf = nkn_buffer_get(&uol, NKN_BUFFER_ATTR);
		if (abuf) {
			ap = (nkn_attr_t *)nkn_buffer_getcontent(abuf);
			nkn_attr_priv_t *ssp_priv = &ap->na_private;
			ssp_priv->vpe.v_video_bit_rate = 431;
			nkn_buffer_setmodified(abuf);
			nkn_buffer_release(abuf);
		}
#endif

                break;

            case 6:  // Player/Publisher type 6 - Smooth Streaming publisher
                break;

	    case 7: // Player type 7 - Flash zeri streaming publisher
		break;

	    default:
		break;
	}

    return;
}

/*
 * requestdoneSSP
 * Return values:
 * > 0 - Process another request
 * = 0 - Close connection
 */
int requestdoneSSP(con_t *con)
{
    int rv=0;

    if ( atol(con->session_id) ) {
	rv = smoothflowLoopbackRequest(con);
    }

    return rv;
}


// Connection close callout.
int close_conn_ssp(con_t *con)
{
    nkn_ssp_session_t *existing_sobj;

    // Only for connections that have a session_id defined
    if ( atol(con->session_id) ) {
	existing_sobj = nkn_ssp_sess_get(con->session_id);                // returns with sobj locked

	if(existing_sobj) {
	    pthread_mutex_unlock(&existing_sobj->lock);

	    nkn_ssp_sess_remove(existing_sobj);
	    pthread_mutex_unlock(&existing_sobj->lock);
	    sess_obj_free(existing_sobj);

	    DBG_LOG(MSG, MOD_SSP, "Session ID %s removed from table, freed and connection closed", con->session_id);
	}
    }
    if(con->ssp.private_data){
	free(con->ssp.private_data);
        con->ssp.private_data = NULL;
    }

    if (con->ssp.header) {
	free(con->ssp.header);
	con->ssp.header = NULL;
    }
    if (con->ssp.fp_state) {
	flv_parser_cleanup(con->ssp.fp_state);
    }

    if (con->ssp.mp4_header) {
	free(con->ssp.mp4_header);
	con->ssp.mp4_header = NULL;
    }

    return 0;
}

/*
 * BZ 10293
 * If the CACHE_ONLY flag is not turned on, SSP data fetch from Buffer
 * will cause OM to do cache validadation with the Origin.
 * If the Origin return a 302, then the NVSD will go to NKN_OM_NON_CACHEABLE
 * codepath, the ssp codepath will be bypassed.
 * During this bypass, the state and alloced memory in ssp will not be reset or
 * released, which will cause error for the next keep-alive connection and
 * memory leak.
 *
 * In the SSP_FORCE_TUNNEL codepath, we have some clean for seek only.
 *
 * This cleanSSP_force_tunnel() will do state reset and memory release for
 * the tunnel forced by OM or other outside control logic and SSP_FORCE_TUNNEL.
 */
int cleanSSP_force_tunnel(con_t *con)
{
    http_cb_t *phttp = &con->http;
    CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
    /*
     * reset the state and values for each request in keep-alive
     * memory alloced in this function will also be release
     */
    /* flv time seek state reset */
    if (con->ssp.fp_state) {
	flv_parser_cleanup(con->ssp.fp_state);
	con->ssp.fp_state = NULL;
    }
    if (con->ssp.header) {
	free(con->ssp.header);
	con->ssp.header = NULL;
    }
    con->ssp.header_size = 0;


    /* MP4 time seek state reset */
    if (con->ssp.mp4_fp_state) {
	mp4_find_box_parser_cleanup((nkn_vpe_mp4_find_box_parser_t*) con->ssp.mp4_fp_state);
	con->ssp.mp4_fp_state = NULL;
    }

    if (con->ssp.buffered_data) {
	bio_close_bitstream(con->ssp.buffered_data);
	con->ssp.buffered_data = NULL;
    }
    if (con->ssp.mp4_header) {
	free(con->ssp.mp4_header);
	con->ssp.mp4_header = NULL;
    }

    con->ssp.seek_state = 0;
    con->ssp.ssp_partial_content_len = 0;

    con->ssp.transcode_state = 0;
    con->ssp.ssp_container_filetype = 0;
    con->ssp.ssp_activated = FALSE;
    con->ssp.ssp_hash_authed = 0; /* reset for each request in keep-alive*/

    /*
     * afr related elements should be reset by the ssp_detect_media_attr() itself.
     * In attr_state 0, ssp_detect_media_attr() will reset all the value.
     * afr related memory should be free in every normal/abnormal exit codepath of
     * ssp_detect_media_attr(), and do not need to be handeld in attr_state 0;
     */
    con->ssp.attr_state = 0;

    return 0;
}
/*
 * End of ssp.c
 */
