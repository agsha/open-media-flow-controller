/*
 ******************************************
 * ssp_queryparams.c -- SSP Query Params
 ******************************************
 */

#include <string.h>
#include <stdlib.h>

#include "ssp_queryparams.h"
#include "ssp.h"
#include "http_header.h"
#include "nkn_errno.h"
#include "nkn_cfg_params.h"
#include "nkn_debug.h"
#include "ssp_authentication.h"
#include "nkn_vpe_mp4_parser.h"
#include "nkn_vpe_mp4_seek_api.h"
#include "nkn_vpe_mp4_seek_parser.h"
#include "nkn_vpe_bitio.h"
#include "nkn_vpe_flv_parser.h"
#include "nkn_vpe_parser_err.h"
#include "nkn_ssp_player_prop.h"
unsigned long long glob_ssp_seeks=0;
unsigned long long glob_flv_parse_data_span_err = 0;

#define TSTR(_p, _l, _r)  char *(_r); ( 1 ? ( (_r) = alloca((_l)+1), memcpy((_r), (_p), (_l)), (_r)[(_l)] = '\0' ) : 0 )
#define DAILYMOTION_SEEK_ENABLE

/*
 * query full download
 */
int
query_full_download(con_t *con, const char *queryName, const char *matchStr)
{
  const char *queryDataTmp=NULL;
  int queryLen=0;
  int queryNameLen=0;
  int rv;

  http_cb_t *phttp = &con->http;

  if (queryName) {
      queryNameLen = strlen(queryName);
      get_query_arg_by_name(&phttp->hdr, queryName, queryNameLen, &queryDataTmp, &queryLen);
  }

  TSTR(queryDataTmp, queryLen, queryData);
  if (queryData && strncmp(queryData, "", 1) != 0){
      if (matchStr && strcmp(queryData, matchStr) == 0) {
	  rv = 0;
      }
      else
	  rv = -NKN_SSP_DOWNLOAD_NOT_SUPP;
  }
  else
    rv = -NKN_SSP_DOWNLOAD_NOT_SUPP;

  return rv;
}

/*
 * header based full download
 */
int
header_full_download(con_t *con, const char *headerName, const char *matchStr)
{
  const char *headerDataTmp=NULL;
  int headerLen=0;
  int headerNameLen=0;
  uint32_t attributes;
  int rv;

  http_cb_t *phttp = &con->http;

  if (headerName) {
      headerNameLen = strlen(headerName);
      get_unknown_header(&phttp->hdr, headerName, headerNameLen, &headerDataTmp, &headerLen, &attributes);
  }
  TSTR(headerDataTmp, headerLen, headerData);
  if (headerData && strncmp(headerData, "", 1) != 0){
      if (matchStr && strcmp(headerData, matchStr) == 0) {
	  rv = 0;
      }
      else
	  rv = -NKN_SSP_DOWNLOAD_NOT_SUPP;
  }
  else
    rv = -NKN_SSP_DOWNLOAD_NOT_SUPP;

  return rv;
}


/*
 * Set download rate
 */
int
setDownloadRate(con_t *con, const ssp_config_t *ssp_cfg)
{
    uint64_t downloadRate=0;

    if (ssp_cfg->con_max_bandwidth > 0) { // If virtual player has a limit, use this
	downloadRate = MIN(ssp_cfg->con_max_bandwidth, con->max_allowed_bw);
	con_set_afr(con, downloadRate);
	DBG_LOG(MSG, MOD_SSP, "Download rate set to : %ld bytes/sec", downloadRate);
    }
    /* BZ 2302
    else if (sess_bandwidth_limit > 0) {// else check if the network has a limit
	downloadRate = MIN((unsigned int)sess_bandwidth_limit, con->max_allowed_bw);
	con_set_afr(con, downloadRate);
	DBG_LOG(MSG, MOD_SSP, "Download rate set to : %d bytes/sec", downloadRate);
	}*/
    else {          // else let network decide best means
	DBG_LOG(MSG, MOD_SSP, "Download rate set to best measure by network");
    }

    return 0;
}

#if 0
/*
 * query throttle to afr
 * Return value: AFR or error code 
 */
int
query_throttle_to_afr(con_t *con, const char *queryName)
{
  const char *queryDataTmp=NULL;
  int queryLen=0;
  int queryNameLen=0;
  int rv;

  http_cb_t *phttp = &con->http;

  queryNameLen = strlen(queryName);

  get_query_arg_by_name(&phttp->hdr, queryName, queryNameLen, &queryDataTmp, &queryLen);
  TSTR(queryDataTmp, queryLen, queryData);
  if ( strncmp(queryData, "", 1) == 0 ) {
      queryData = NULL;
  }

  if(queryData){
    if (atoi(queryData) <= 0) {
      DBG_LOG(WARNING, MOD_SSP, "Throttle AFR parameter has invalid value. %s=%d (Error: %d)", queryName, atoi(queryData), NKN_SSP_BAD_QUERY_OFFSET);
      rv = -NKN_SSP_BAD_QUERY_OFFSET;  //Close connection
      goto exit;
    }

    rv = atoi(queryData) * KBYTES; // in bytes/sec
    if (rv<0) {
	rv = 0; //Disable afr for large values or on rollover
    }
  }
  else
    rv = 0;

 exit:
  return rv;
}
#else
/*
 * query for rate control can work with MBR or AFR
 * return value is queryData or error code
 */
int
query_throttle_to_rc(con_t *con, const char *queryName)
{
    const char *queryDataTmp=NULL;
    int queryLen=0;
    int queryNameLen=0;
    int rv;

    http_cb_t *phttp = &con->http;

    if (queryName) {
	queryNameLen = strlen(queryName);
	get_query_arg_by_name(&phttp->hdr, queryName, queryNameLen, &queryDataTmp, &queryLen);
    }

    TSTR(queryDataTmp, queryLen, queryData);
    if ( strncmp(queryData, "", 1) == 0 ) {
	queryData = NULL;
    }

    if(queryData){
	if (atoi(queryData) <= 0) {
	    DBG_LOG(WARNING, MOD_SSP, "Throttle AFR parameter has invalid value. %s=%d (Error: %d)", queryName, atoi(queryData)\
		    , NKN_SSP_BAD_QUERY_OFFSET);
	    rv = -NKN_SSP_BAD_QUERY_OFFSET;  //Close connection
	    goto exit;
	}

	rv = atoi(queryData);
	if (rv < 0) {
	    rv = 0; //Disable afr for large values or on rollover
	}
    }
    else
	rv = 0;

 exit:
    return rv;
}

#endif


/*
 * query fast start size (if fast start query param is in units of kilobytes)
 * Return value: Fast start buffer size or error code 
 */
int
query_fast_start_size(con_t *con, const char *queryName)
{
  const char *queryDataTmp=NULL;
  int queryLen=0;
  int queryNameLen=0;
  int rv=0;

  http_cb_t *phttp = &con->http;

  if (queryName) {
      queryNameLen = strlen(queryName);
      get_query_arg_by_name(&phttp->hdr, queryName, queryNameLen, &queryDataTmp, &queryLen);
  }

  TSTR(queryDataTmp, queryLen, queryData);
  if ( strncmp(queryData, "", 1) == 0 ) {
      queryData = NULL;
  }

  if(queryData){
    if (atoi(queryData) < 0) {
      DBG_LOG(WARNING, MOD_SSP, "Fast start size parameter has invalid value. %s=%d (Error: %d)", queryName, atoi(queryData), NKN_SSP_BAD_QUERY_OFFSET);
      //rv stays at 0 rather than closing the connection
      //rv = -NKN_SSP_BAD_QUERY_OFFSET;  //Close connection
      goto exit;
    }

    rv = atoi(queryData) * KBYTES; // in bytes
    if (rv<0){
	rv = 0; // Disable fast start for very large values or on rollover
    }
  }
  else {
      rv = 0;
  }

 exit:
  return rv;
}



/*
 * query fast start time (if fast start query param is in units of seconds)
 * Return value: Fast start buffer size or error code 
 */
int
query_fast_start_time(con_t *con, const char *queryName, uint32_t afr)
{
  const char *queryDataTmp=NULL;
  int queryLen=0;
  int queryNameLen=0;
  int rv=0;

  http_cb_t *phttp = &con->http;

  if (queryName) {
      queryNameLen = strlen(queryName);
      get_query_arg_by_name(&phttp->hdr, queryName, queryNameLen, &queryDataTmp, &queryLen);
  }

  TSTR(queryDataTmp, queryLen, queryData);
  if ( strncmp(queryData, "", 1) == 0 ) {
      queryData = NULL;
  }

  if(queryData){
    if (atoi(queryData) < 0) {
      DBG_LOG(WARNING, MOD_SSP, "Fast start size parameter has invalid value. %s=%d (Error: %d)", queryName, atoi(queryData), NKN_SSP_BAD_QUERY_OFFSET);
      rv = -NKN_SSP_BAD_QUERY_OFFSET;  //Close connection
      goto exit;
    }

    rv = atoi(queryData) * afr; // in bytes
  }
  else {
    if (sess_afr_faststart > 0)
      rv = sess_afr_faststart * afr;
  }

 exit:
  return rv;
}



/*
 *
 * Query for FLV Seek/Scrub using Byte Offsets
 *
 */

int query_flv_seek(con_t *con, const char *queryName, const char *querySeekLenName)
{
    const char *queryDataTmp=NULL;
    int queryLen=0;
    int queryNameLen=0;
    const char *queryData2Tmp=NULL;
    int queryLen2=0, querySeekLenSize=0;

    int64_t scrubOffset=0, scrubLen=0;

    int rv=0;

    http_cb_t *phttp = &con->http;

    if (queryName) {
	queryNameLen = strlen(queryName);
	get_query_arg_by_name(&phttp->hdr, queryName, queryNameLen, &queryDataTmp, &queryLen);
    }
    // Check for optional seek length paramter
    if (querySeekLenName) {
	querySeekLenSize = strlen(querySeekLenName);
	get_query_arg_by_name(&phttp->hdr, querySeekLenName, querySeekLenSize, &queryData2Tmp, &queryLen2);
    }
    TSTR(queryDataTmp,  queryLen,  queryData);
    TSTR(queryData2Tmp, queryLen2, queryData2);
    if ( strncmp(queryData, "", 1) == 0 ) {
	queryData = NULL;
    }
    if ( strncmp(queryData2, "", 1) == 0 ) {
        queryData2 = NULL;
    }

    if(queryData){

	queryData  != NULL ? (scrubOffset = atoi(queryData)) : (scrubOffset = 0);
	queryData2 != NULL ? (scrubLen =  atoi(queryData2) ) : (scrubLen = 0);
	if (scrubLen <= 0){
	    scrubLen = 0;
	} else {
	    scrubLen = scrubOffset + scrubLen - 1;
	}

	glob_ssp_seeks++;

	if(scrubOffset > 0){
	    // Set the seek_start and seek_stop parameters and enable byte range seek
	    DBG_LOG(WARNING, MOD_SSP, "Seek parameter offset = %ld, length = %ld", scrubOffset, scrubLen);
	    http_set_seek_request(phttp, scrubOffset, scrubLen);
	} else if (scrubOffset == 0) {
            DBG_LOG(WARNING, MOD_SSP, "Seek parameter offset = %ld", scrubOffset);
            goto exit;
        } else {
	    DBG_LOG(WARNING, MOD_SSP, "Seek parameter has negative offset. %s=%ld (Error: %d)", queryName, scrubOffset, NKN_SSP_BAD_QUERY_OFFSET);
	    rv = -NKN_SSP_BAD_QUERY_OFFSET;  //Close connection
	    goto exit;
	}

	// Re create the URI for SEEK so that OM can make a byte range request instead of a seek request
	// (URI w/o fs)
	generate_om_seek_uri(con, queryName);

    }//end if queryData

 exit:
    return rv;
}


/*
 * Enforce AFR with connection max bandwidth caps
 */
int
setAFRate(con_t *con, const ssp_config_t *ssp_cfg, int64_t steadyRate)
{
    int64_t afrlimit=0;
    int rv=0;

    // Obtain specified con max b/w value
    if (ssp_cfg->con_max_bandwidth > 0) // CMBvp > 0
	afrlimit = ssp_cfg->con_max_bandwidth;
    else if (sess_assured_flow_rate > 0)// if no limit is specified, then n/w is used or client_bw capped to n/w
	afrlimit = sess_assured_flow_rate;
    else
	afrlimit = con->max_client_bw; // client_bw is actually capped at MBRnw

    if (steadyRate <= 0 ){ // If query does not contain an AFR param
	if (ssp_cfg->con_max_bandwidth > 0) {
	    steadyRate = (int64_t)((float)ssp_cfg->con_max_bandwidth / 1.2); // set AFR to VP con max b/w value
	} else if (sess_assured_flow_rate > 0) {
	    steadyRate = (int64_t)((float)sess_assured_flow_rate / 1.2); // set AFR to NW cap
	}
	else
	    steadyRate = 0;
    }

    if (steadyRate > 0) { // If AFR is specified

	if ((int64_t)(1.2 * (float)steadyRate) <= (int64_t) MIN(afrlimit, (unsigned)con->pns->max_allowed_bw) ) {
	    con_set_afr(con, steadyRate);
	    DBG_LOG(MSG, MOD_SSP, "AFR Set: %ld Bytes/sec", con->min_afr);
	    if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
		DBG_TRACE("AFR Set to %ld Bytes/sec", con->min_afr);
	    }

	    rv = SSP_SEND_DATA_OTW;
	    goto exit;
	}
	else { 	    // Reject connection, since requested rate > available bandwidth
	    DBG_LOG(WARNING, MOD_SSP, "AFR enforcement failed: %ld Connection max bw at virtual player or total system capacity may be limiting AFR", steadyRate);
	    if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
		DBG_TRACE("AFR enforcement failed, Connection max bw at virtual player or total system capacity may be limiting AFR");
	    }

	    rv = -NKN_SSP_AFR_INSUFF;
	    goto exit;
	}
    }
    else if (steadyRate < 0) {
	if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
	    DBG_TRACE("AFR Failed, b/w unavailable");
	}

	rv = steadyRate; // error tag already set
	goto exit;
    }

 exit:
    return rv;
}


//////////////////////////////////////////////////////////////////////////////////
// Re create the URI for SEEK so that OM can make a regular request for full file
// (URI w/o seek request)
int generate_om_seek_uri(con_t *con, const char *queryName)
{
    int remapLen = 0;
    char *omUrl;
    const char *name, *val;
    int name_len=0, val_len=0, arg_num=0;
    int tot_length = 0;

    http_cb_t *phttp = &con->http;

    int uriCnt=0, uriLen=0;
    unsigned int uriAttrs=0;
    const char *uriData;

    //BZ: 8355
    if (get_known_header(&phttp->hdr, MIME_HDR_X_NKN_DECODED_URI, &uriData, &uriLen, &uriAttrs, &uriCnt)) {
	if (get_known_header(&phttp->hdr , MIME_HDR_X_NKN_URI, &uriData, &uriLen, &uriAttrs, &uriCnt)) {
	    return -NKN_SSP_BAD_URL;
	}
    }

    remapLen = findStr(uriData, "?", uriLen);


    tot_length += (remapLen + 1);  //1 = "?"
    do {
	name_len = 0;
	val_len = 0;
	name = NULL;
	val = NULL;
	get_query_arg(&phttp->hdr, arg_num++, &name, &name_len, &val, &val_len);
	if (name == NULL)
		break;
	if (queryName && strncmp(name, queryName, name_len) == 0)
		continue;
	if (arg_num > 1) {
		tot_length += 1;
	}
	tot_length += name_len + 1 + val_len; // 1 = "="
    }while (name != NULL);
    omUrl = (char *)alloca(tot_length + 1);
    memset(omUrl, 0, tot_length + 1);

    memcpy(omUrl, uriData, remapLen);
    strcat(omUrl,"?");
    arg_num = 0;
    do{
        name_len = 0;
        val_len = 0;
        name = NULL;
        val = NULL;
        get_query_arg(&phttp->hdr, arg_num++, &name, &name_len, &val, &val_len);
        if (name==NULL)
            break;
        // Create the Seek URI
        if (queryName && strncmp(name, queryName, name_len) == 0) //Omit the seek query param
            continue;
        if (arg_num > 1) {
            strcat(omUrl, "&");
	}
        strncat(omUrl, name, name_len);
        strcat(omUrl, "=");
        strncat(omUrl, val, val_len);
    }while (name != NULL);
    omUrl[tot_length] = '\0';
    add_known_header(&phttp->hdr, MIME_HDR_X_NKN_SEEK_URI, omUrl, strlen(omUrl));

    return 0;
}


/*
 *
 * Query for MP4 Seek/Scrub using Time Offsets
 * Support moov at any postion in file
 */
#define N_BOX_TO_FIND 3

int query_mp4_seek_for_any_moov(con_t *con, const char *queryName, off_t contentLen, const char *dataBuf, int bufLen, int timeUnits)
{
    const char *queryDataTmp=NULL;
    bitstream_state_t *bs;
    int queryLen=0;
    int queryNameLen=0;

    int64_t timeOffset=0, bytesWritten = 0;
    uint64_t moov_offset=0, moov_size=0, mdat_offset=0, udta_size=0;
    uint8_t *p_udta;
    moov_t *moov = NULL, *moov_out=NULL;
    int rv=0, traks = 0, seek_ret = 0;
    http_cb_t *phttp = &con->http;
    int32_t mdat_id;
    struct mdat_size_s {
	uint32_t short_size;
	uint64_t long_size;
    } mdat_size;
    uint32_t mdat_hd_size;
    MP4_SEEK_PROVIDER_TYPE seek_type = SEEK_PTYPE_UNKNOWN;

    nkn_vpe_mp4_find_box_parser_t* fp;
    const char* find_box_name_list[N_BOX_TO_FIND] = {"moov", "mdat", "ftyp"};
    int n_box_to_find = N_BOX_TO_FIND;
    vpe_ol_t ol[N_BOX_TO_FIND]; /* ol of box in find_box_name_list */
    int32_t fc_rv; /* return value of calling function */
    uint8_t *tmp_p;
    off_t brstart, brstop;
    brstart = brstop = -1;
    tmp_p = NULL;
    fp = NULL;
    bs = NULL;

    if (queryName) {
	queryNameLen = strlen(queryName);
	get_query_arg_by_name(&phttp->hdr, queryName, queryNameLen, &queryDataTmp, &queryLen);
    }

    TSTR(queryDataTmp, queryLen, queryData);
    if ( strncmp(queryData, "", 1) == 0 ) {
        queryData = NULL;
    }

    if (queryData){ //Seek query was found

        queryData != NULL ? (timeOffset = atoi(queryData)/timeUnits) : (timeOffset = 0);
	// timeUnits is expressed as # of clicks in a second
        DBG_LOG(MSG, MOD_SSP,
		"Entered mp4 seek time = %ld mp4-hdr-size %ld",
		timeOffset, con->ssp.mp4_header_size);

        if (timeOffset <= 0) {
	    generate_om_seek_uri(con, queryName); // Create the SEEK URI for OM
            rv = SSP_SEND_DATA_OTW;
            goto exit;
        }

	if (!con->ssp.seek_state ){//&& contentLen == 0 ) {
	    // First entry, to initiate internal loop-back for MP4 header fetch
            DBG_LOG(MSG, MOD_SSP, "MP4Seek: First entry to start internal fetch");

            con->ssp.seek_state = 5; // Persistent state variable
            generate_om_seek_uri(con, queryName); // Create the SEEK URI for OM

            if (CHECK_HTTP_FLAG(phttp, HRF_BYTE_RANGE)) {
		// Clear any byte range requests sent by flash player
                CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
                phttp->brstart = 0;
                phttp->brstop = 0;
            }
	    //SET_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
	    CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
            rv = SSP_SEND_DATA_BACK;
            goto exit;
	    /*
	     * Move the check of fetch data availability below the state 0
	     * As the data availability check is only for current fucntion
	     */
	} else if (contentLen == OM_STAT_SIG_TOT_CONTENT_LEN &&
		   !CHECK_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH)) { // file was not found
	    DBG_LOG(WARNING, MOD_SSP,
		    "MP4Seek: Requested file for seek not found, "
		    "re-try, forward to client. Disable seek. seek_state: %d",
		    con->ssp.seek_state);
	    rv = -NKN_SSP_FORCE_TUNNEL_PATH;
	    goto exit;
	} else if (contentLen == OM_STAT_SIG_TOT_CONTENT_LEN &&
		   CHECK_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH)) {
	    DBG_LOG(WARNING, MOD_SSP, "MP4Seek: File not in cache, "
		    "re-try and forward to client w/o seeking. seek_state: %d",
		    con->ssp.seek_state);
	    rv = -NKN_SSP_FORCE_TUNNEL_PATH;
	    goto exit;
        } else if (con->ssp.seek_state == 5 ||
		    con->ssp.seek_state == 10 || con->ssp.seek_state == 20) {
	    // Second entry, to determine how much we need to get entire MP4 header
	    //	    DBG_LOG(MSG, MOD_SSP,
	    //	    "MP4Seek: Next entry to fetch subsequent chunks. seek_state: %d",
	    //	    con->ssp.seek_state);
	    //	    if (is_known_header_present(&phttp->hdr, MIME_HDR_X_NKN_SEEK_URI)) {
	    //		delete_known_header(&phttp->hdr, MIME_HDR_X_NKN_SEEK_URI);
	    //	    }
	    switch (con->ssp.seek_state) {
		case 5:
		    /*
		     * State 5: parse the entire MP4 file to obtain the pos and size
		     *          of moov and mdat box
		     */
		    if (bufLen > 0 && con->ssp.ssp_partial_content_len < contentLen) {
			/*
			 * Normally, for video streaming, moov box should
			 * be placed at the first 32KB of MP4 files
			 * But this is not mandatory.
			 * Some MP4 files put the moov box at the end of
			 * file or not at the first 32KB
			 *
			 * Add a new function to detect the pos and size of
			 * moov and mdat box before the processing of moov box
			 */
			if (con->ssp.ssp_client_id == 5 && con->ssp.ssp_attr_partial_content_len == 0){
			    /*
			     * Youtube SSP use the itag query for file type
			     * add a file type detection to check whether it is the same as itag.
			     */
			    uint8_t* p_file_buf;
			    p_file_buf = (uint8_t*)dataBuf;
			    if (bufLen > 16) {
				if (vpe_detect_media_type(p_file_buf) != con->ssp.ssp_container_id) {
				    DBG_LOG(WARNING, MOD_SSP,
					    "MP4: Youtube file type detect is different from itag. Have to tunnel. "
					    "seek_state: %d",
					    con->ssp.seek_state);
				    rv = -NKN_SSP_FORCE_TUNNEL_PATH;
				    goto exit;
				}
			    }
			}

			/* only come to the init for the first time*/
			if (con->ssp.mp4_fp_state == NULL) {
			    fp = mp4_init_find_box_parser();
			    if (!fp) {
				DBG_LOG(MSG, MOD_SSP,
					"MP4Seek: Unable to init MP4 parser "
					"have to tunnel this seek request. seek_state: %d",
					con->ssp.seek_state);
				rv = -NKN_SSP_FORCE_TUNNEL_PATH;
				goto exit;
			    }
			    con->ssp.mp4_fp_state = (void*)fp; /* store for future loop*/

			    /* set which boxes need to find */
			    fc_rv = mp4_set_find_box_name(fp, find_box_name_list,
							  n_box_to_find);
			    if (fc_rv < 0) {
				DBG_LOG(MSG, MOD_SSP,
					"MP4Seek: Fail to Set find box name "
					"have to tunnel this seek request. seek_state: %d",
					con->ssp.seek_state);
				rv = -NKN_SSP_FORCE_TUNNEL_PATH;
				goto exit;
			    }

			    /* parser open the databuf*/
			    fc_rv = mp4_parse_init_databuf(fp, (uint8_t*)dataBuf, bufLen);
			    if (fc_rv < 0) {
				DBG_LOG(MSG, MOD_SSP,
					"MP4Seek: Fail to find ftyp box, not a Valid MP4 file "
					"have to tunnel this seek request. seek_state: %d",
					con->ssp.seek_state);
				rv = -NKN_SSP_FORCE_TUNNEL_PATH;
				goto exit;
			    }
			} /* if (con->ssp.mp4_fp_state == NULL) */

			fp = (nkn_vpe_mp4_find_box_parser_t*)con->ssp.mp4_fp_state;

			/* Loop to parse all the root box of MP4 file */
			fc_rv = mp4_parse_find_box_info(fp, (uint8_t*)dataBuf, bufLen,
							&brstart, &brstop);
			if (fc_rv == n_box_to_find) {
			    /* found all the boxes in find_box_name_list */

			    // I do not think we need this
			    // con->ssp.mp4_fp_state = NULL;
			    // shall we need to reset other values?
			    con->ssp.ssp_partial_content_len = 0; /* reset value */

			    /* check whether box size and offset are valid */
			    for (int i = 0; i < n_box_to_find - 1/*do not check ftyp box*/; i++) {
				if (fp->pbox_list->box_info[i].offset == 0 ||
				    fp->pbox_list->box_info[i].size == 0) {
				    DBG_LOG(WARNING, MOD_SSP,
					    "MP4Seek: Corrupted moov/mdat offset or size "
					    "have to tunnel this seek request. seek_state: %d",
					    con->ssp.seek_state);
				    rv = -NKN_SSP_FORCE_TUNNEL_PATH;
				    goto exit;
				}
			    }

			    for (int i =0; i < n_box_to_find; i++) {
				DBG_LOG(MSG, MOD_SSP,
                                        "MP4Seek: box name = %s, offset = %ld, size = %ld. ",
					fp->pbox_list->box_info[i].name,
					fp->pbox_list->box_info[i].offset,
					fp->pbox_list->box_info[i].size);
			    }

			    /* must reset this */
			    phttp->brstart = 0;
			    phttp->brstop  = 0;
			    rv = SSP_SEND_DATA_BACK;
			    /*
			     * fp will be cleaned at the end of MP4 seek
			     * go to state 10 */
			    con->ssp.seek_state = 10;
			    goto exit;
			}

			if (E_VPE_MP4_DATA_INVALID == rv || brstart < 0) {
			    // Tunnel the seek request in this scenario
			    DBG_LOG(MSG, MOD_SSP,
				    "MP4Seek: Invalid file"
				    "have to tunnel this seek request. seek_state: %d",
				    con->ssp.seek_state);
			    rv = -NKN_SSP_FORCE_TUNNEL_PATH;
			    goto exit;
			}
			/* add the parsed bufLen into ssp_partial_content_len */
			con->ssp.ssp_partial_content_len = brstart;
			if (con->ssp.ssp_partial_content_len < contentLen) {
			    //printf("ssp_partial_content_len = %d\n", con->ssp.ssp_partial_content_len);
                            phttp->brstart = brstart;
                            phttp->brstop  = contentLen - 1;
			    rv = SSP_SEND_DATA_BACK; // continue the parse loop
			} else {
			    // Tunnel the seek request in this scenario
			    DBG_LOG(MSG, MOD_SSP,
                                "MP4Seek: Reach the end of file, but Fail to find all boxes "
				    "have to tunnel this seek request. seek_state: %d",
				    con->ssp.seek_state);
			    rv = -NKN_SSP_FORCE_TUNNEL_PATH;
			}
			goto exit;
		    } else {
			// Tunnel the seek request in this scenario
			DBG_LOG(MSG, MOD_SSP,
				"MP4Seek: Fail to find all boxes "
				"have to tunnel this seek request. seek_state: %d",
				con->ssp.seek_state);
			rv = -NKN_SSP_FORCE_TUNNEL_PATH;
			goto exit;
		    }
		    break;
		case 10:
		    /*
		     * State 10: alloc buffer for whole moov box
		     *           Read the whole moov box into buffer
		     *           ftype box + moov box will be read into buffer
		     */
		    fp = (nkn_vpe_mp4_find_box_parser_t*)con->ssp.mp4_fp_state;

		    if (bufLen > 0 && con->ssp.ssp_partial_content_len < contentLen) {
			/* alloc the buffer */
			if (!con->ssp.buffered_data) {
			    /* header_size = moov box size + ftype box size*/
			    con->ssp.mp4_header_size = fp->pbox_list->box_info[0].size +
				fp->pbox_list->box_info[2].size  + 16 /* add 16 to prevent valgrind issue*/ ;
			    con->ssp.mp4_header = (uint8_t *) \
				nkn_calloc_type(con->ssp.mp4_header_size,
						sizeof(char),
						mod_ssp_mp4_rebased_seekheader_t);
			    if (con->ssp.mp4_header == NULL) {
				DBG_LOG(MSG, MOD_SSP,
					"MP4Seek: Failed to allocate space for MP4 header. "
					"seek_state: %d", con->ssp.seek_state);
				rv = -NKN_SSP_FORCE_TUNNEL_PATH;
				goto exit;
			    }
			    /* init bitstream */
			    con->ssp.buffered_data = bio_init_bitstream(con->ssp.mp4_header,
									con->ssp.mp4_header_size);
			    if (con->ssp.buffered_data == NULL) {
				DBG_LOG(MSG, MOD_SSP,
					"MP4Seek: Failed to init the data buffer for MP4 header. "
					"seek_state: %d", con->ssp.seek_state);
				rv = -NKN_SSP_FORCE_TUNNEL_PATH;
				goto exit;
			    }
			} // if (!con->ssp.buffered_data)

			/* 1: read the ftyp box into buffer */
			if (bio_get_pos(con->ssp.buffered_data) == 0) {
			    int bytes_to_read = fp->pbox_list->box_info[2].size; /* ftyp box size */
			    if (fp->pbox_list->box_info[2].offset != 0) {
                                DBG_LOG(MSG, MOD_SSP,
                                        "MP4Seek: Invalid MP4 file. "
                                        "seek_state: %d", con->ssp.seek_state);
                                rv = -NKN_SSP_FORCE_TUNNEL_PATH;
                                goto exit;
			    }
			    bytesWritten = bio_aligned_write_block(con->ssp.buffered_data,
                                                                   (uint8_t*)dataBuf,
                                                                   bytes_to_read);
                            con->ssp.ssp_partial_content_len += fp->pbox_list->box_info[0].offset;
			    phttp->brstart = fp->pbox_list->box_info[0].offset; /* moov box pos*/
			    phttp->brstop = contentLen - 1;
			    rv = SSP_SEND_DATA_BACK;
			    goto exit;

			}
			/* 2: loop to read the whole moov box into buffer */
			if (bio_get_pos(con->ssp.buffered_data) \
			    < con->ssp.mp4_header_size) {
			    int bytes_left = con->ssp.mp4_header_size - 16 -
					     bio_get_pos(con->ssp.buffered_data);
			    int bytes_to_read = (bufLen < bytes_left ? bufLen : bytes_left);
			    bytesWritten = bio_aligned_write_block(con->ssp.buffered_data,
								   (uint8_t*)dataBuf,
								   bytes_to_read);
			    if (bytesWritten < 0) {
                                DBG_LOG(MSG, MOD_SSP,
                                        "MP4Seek: Failed to Read enough data into MP4 header. "
                                        "seek_state: %d", con->ssp.seek_state);
                                rv = -NKN_SSP_FORCE_TUNNEL_PATH;
                                goto exit;
			    }
			    con->ssp.ssp_partial_content_len += bytes_to_read;

			    if ((bio_get_pos(con->ssp.buffered_data) + 16)  == con->ssp.mp4_header_size) {
				DBG_LOG(MSG, MOD_SSP,
					"MP4Seek: Read ftyp and moov box into buffer. "
                                        "go to state 20. seek_state: %d",
                                        con->ssp.seek_state);
				/* only this condition will go to state 20 */
				con->ssp.seek_state = 20;
			    } else if ((con->ssp.ssp_partial_content_len < contentLen) &&
				       ((bio_get_pos(con->ssp.buffered_data) + 16) < con->ssp.mp4_header_size)) {
				/* read more data*/
		                //printf("ssp_partial_content_len = %d\n", con->ssp.ssp_partial_content_len);
                                rv = SSP_WAIT_FOR_DATA;
                                goto exit;
			    } else {
				/* tunnel this request */
				DBG_LOG(MSG, MOD_SSP,
	                        "MP4Seek: Reach the end of file, but fail to read entire moov box into buffer "
					"have to tunnel this seek request. seek_state: %d",
					con->ssp.seek_state);
				rv = -NKN_SSP_FORCE_TUNNEL_PATH;
				goto exit;
			    }
			} else {
			    /* tunnel this request */
			    DBG_LOG(MSG, MOD_SSP,
                                "MP4Seek: This buffered_data condition should not happen. "
				    "have to tunnel this seek request. seek_state: %d",
				    con->ssp.seek_state);
			    rv = -NKN_SSP_FORCE_TUNNEL_PATH;
			    goto exit;
			}

			/*
			 * now, we should have the entire moov box in buffer
			 * Will not break, go to state 20 directly
			 */

		    } else {
			/* tunnel this request */
			DBG_LOG(MSG, MOD_SSP,
                                "MP4Seek: Fail to read entire moov box into buffer "
                                "have to tunnel this seek request. seek_state: %d",
				con->ssp.seek_state);
			rv = -NKN_SSP_FORCE_TUNNEL_PATH;
			goto exit;
		    }
		    /* no break here */
		case 20:
		    /*
		     * State 20: process the moov box for seek request
		     *
		     */
		    fp =(nkn_vpe_mp4_find_box_parser_t*)con->ssp.mp4_fp_state;

		    bs = (bitstream_state_t*)con->ssp.buffered_data;
		    moov_size   = fp->pbox_list->box_info[0].size;
		    /*
		     * moov_offset here is the offset in the con->ssp.mp4_header
		     * not the one in the original MP4 file
		     * moov_offset is set to size of ftype box
		     */
		    moov_offset = fp->pbox_list->box_info[2].size;
		    moov = mp4_init_moov((uint8_t *)bs->data + moov_offset, moov_offset, moov_size);
		    if (moov == NULL) {
			DBG_LOG(WARNING, MOD_SSP,
				"MP4Seek: Fail to init_moov. "
				"have to tunnel this seek request. seek_state: %d",
				con->ssp.seek_state);
			rv = -NKN_SSP_FORCE_TUNNEL_PATH;
			goto exit;
		    }
		    traks = mp4_populate_trak(moov);
		    if (traks <= 0) {
			DBG_LOG(ERROR, MOD_SSP,
				"MP4Seek: (Tracks in file = %d). "
				"have to tunnel this seek request. seek_state: %d",
				traks, con->ssp.seek_state);
                        rv = -NKN_SSP_FORCE_TUNNEL_PATH;
			goto exit;
		    }

		    for (int i = 0; i < n_box_to_find; i++) {
			ol[i].offset = fp->pbox_list->box_info[i].offset;
			ol[i].length = fp->pbox_list->box_info[i].size;
		    }

		    mdat_offset = fp->pbox_list->box_info[1].offset /* mdat pos in old file */
			+ (seek_ret = mp4_seek_trak(moov, traks, timeOffset, &moov_out, ol, n_box_to_find));
		    if(seek_ret < 0) {
			DBG_LOG(WARNING, MOD_SSP,
				"MP4Seek: Invalid request for seek, "
				"maybe outside the timeline (seektime = %ld). seek_state: %d",
				timeOffset, con->ssp.seek_state);
                        rv = -NKN_SSP_FORCE_TUNNEL_PATH;
			goto exit;
		    }
#ifdef DAILYMOTION_SEEK_ENABLE
		    /* if the virtual player type is 0; test
		       if the seek provider is dailymotion  */
		    if (con->ssp.ssp_client_id == 0) { /* generic virtual player */
			uint8_t ptype = 0;
			int32_t err = 0;
			dm_seek_input_t dsi;
			err = mp4_test_known_seek_provider(moov,
							   SEEK_PTYPE_DAILYMOTION,
							   &ptype);
			if (ptype) {
			    dsi.tot_file_size = phttp->content_length;
			    /* Dailymotion MP4 has moov box in first 32KB */
			    dsi.seek_mdat_size = phttp->content_length - mdat_offset;
			    dsi.duration = mp4_get_vid_duration(moov);
			    dsi.moov_offset = moov_offset;
			    fc_rv = mp4_dm_modify_udta(moov_out, &dsi);
			    if (fc_rv < 0) {
				DBG_LOG(MSG, MOD_SSP,
					"MP4Seek: Failed to do udta modify for dailymotion. seek_state: %d",
					con->ssp.seek_state);
				rv = -NKN_SSP_FORCE_TUNNEL_PATH;
				goto exit;
			    }
			    seek_type = SEEK_PTYPE_DAILYMOTION;
			    //goto copy_udta;
			}
		    }
#endif
		    /* For youtube seek we need to call the following API's
		     * (a) create a new udta tag with the seek position stored in it
		     * (b) make the existing udta box as free box;
		     * (c) modify the moov size and stco box to reflect the new udta tag
		     * new MP4 file will look thus
		     * moov
		     *    |--> free (old udta)
		     *    |--> udta (new)
		     * mdat
		     */
		    //Is this correct? need to check again
		    mdat_size.long_size = fp->pbox_list->box_info[1].size -
					(mdat_offset - fp->pbox_list->box_info[1].offset);

		    if (mdat_size.long_size > 0xFFFFFFFF /*uint32.maxvalue*/) {
			mdat_hd_size = 16;
			mdat_size.short_size = 1;
		    } else {
			mdat_hd_size = 8;
			mdat_size.short_size = (uint32_t)mdat_size.long_size;
		    }

		    if (con->ssp.ssp_client_id == 5 || // If YouTube player is enabled
			seek_type == SEEK_PTYPE_DAILYMOTION){ // If Dailymotion
			if (con->ssp.ssp_client_id == 5) { // for YouTube player
			    fc_rv = mp4_yt_modify_udta(moov_out);
			    if (fc_rv < 0) {
				DBG_LOG(MSG, MOD_SSP,
					"MP4Seek: Failed to do udta modify. seek_state: %d",
					con->ssp.seek_state);
				rv = -NKN_SSP_FORCE_TUNNEL_PATH;
				goto exit;
			    }
			}
			//copy_udta:
			mp4_get_modified_udta(moov_out, &p_udta, &udta_size);
			mp4_modify_moov(moov_out, udta_size);

			tmp_p = (uint8_t *)nkn_realloc_type(con->ssp.mp4_header,
							    (moov_offset + moov_out->size +
							     udta_size + mdat_hd_size),
							    mod_ssp_mp4_rebased_seekheader_t);
			if (tmp_p == NULL) {
			    DBG_LOG(MSG, MOD_SSP,
				    "MP4Seek: Failed to re-allocate seek headers. seek_state: %d",
				    con->ssp.seek_state);
			    rv = -NKN_SSP_FORCE_TUNNEL_PATH;
			    goto exit;
			}
			con->ssp.mp4_header = tmp_p;
			bio_reinit_bitstream(con->ssp.buffered_data,
					     con->ssp.mp4_header,
					     moov_offset + moov_out->size + udta_size + mdat_hd_size);

			// Overwrite the new moof box
			memcpy(con->ssp.mp4_header + moov_offset, moov_out->data,
			       moov_out->size);
			// Append the udta box
			memcpy(con->ssp.mp4_header + moov_offset + moov_out->size,
			       p_udta, udta_size);

			mdat_size.short_size = nkn_vpe_swap_uint32(mdat_size.short_size);
			memcpy(con->ssp.mp4_header + moov_offset + moov_out->size + udta_size,
			       &mdat_size, 4);

			mdat_id = 0x7461646d;
			memcpy(con->ssp.mp4_header + moov_offset + moov_out->size + udta_size + 4,
			       &mdat_id, 4);
			if (mdat_hd_size == 16) {
			    mdat_size.long_size = nkn_vpe_swap_uint64(mdat_size.long_size);
			    memcpy(con->ssp.mp4_header + moov_offset + moov_out->size + udta_size + 8,
				   &mdat_size.long_size, 8);
			}

			con->ssp.mp4_header_size = moov_offset + moov_out->size + udta_size + mdat_hd_size;

			if (moov_out->data) {
			    free(moov_out->data);
			    moov_out->data = NULL;
			}
			if (moov_out->udta_mod) {
			    free(moov_out->udta_mod);
			    moov_out->udta_mod = NULL;
			}
			if (moov->udta_mod) {
			    free(moov->udta_mod);
			    moov->udta_mod = NULL;
			}

			/*cleanup the moov box */
			mp4_moov_cleanup(moov, traks);
			moov = NULL;
			/* cleanup the output moov box */
			mp4_moov_out_cleanup(moov_out, traks);
			moov_out = NULL;
		    } else { // for all other players, e.g., Break, etc
			tmp_p = (uint8_t*)nkn_realloc_type(con->ssp.mp4_header,
							   (moov_offset + moov_out->size + mdat_hd_size),
							   mod_ssp_mp4_rebased_seekheader_t);
			if (tmp_p == NULL) {
                            DBG_LOG(MSG, MOD_SSP,
                                    "MP4Seek: Failed to re-allocate seek headers. seek_state: %d",
				    con->ssp.seek_state);
                            rv = -NKN_SSP_FORCE_TUNNEL_PATH;
			    goto exit;
			}
			con->ssp.mp4_header = tmp_p;
			// Overwrite the new moof box
			memcpy(con->ssp.mp4_header + moov_offset, moov_out->data,
			       moov_out->size);
			mdat_size.short_size = nkn_vpe_swap_uint32(mdat_size.short_size);
			memcpy(con->ssp.mp4_header + moov_offset + moov_out->size,
			       &mdat_size.short_size, 4);
			mdat_id = 0x7461646d;
			memcpy(con->ssp.mp4_header + moov_offset + moov_out->size + 4,
			       &mdat_id, 4);
			if (mdat_hd_size == 16) {
			    mdat_size.long_size = nkn_vpe_swap_uint64(mdat_size.long_size);
			    memcpy(con->ssp.mp4_header + moov_offset + moov_out->size + udta_size + 8,
				   &mdat_size.long_size, 8);
			}

			con->ssp.mp4_header_size = moov_offset + moov_out->size + mdat_hd_size;

			if (moov_out->data) {
			    free(moov_out->data);
			    moov_out->data = NULL;
			}
			/*cleanup the moov box */
			mp4_moov_cleanup(moov, traks);
			moov = NULL;
			/* cleanup the output moov box */
			mp4_moov_out_cleanup(moov_out, traks);
			moov_out = NULL;
		    }
		    // All data required for re-basing has been obtained
		    // Now set the correct SEEK_RANGE_REQUEST, generate the pre-pend header and SEND_OTW
		    if (CHECK_HTTP_FLAG(phttp, HRF_BYTE_RANGE)) {
			CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
			phttp->brstart = 0;
			phttp->brstop = 0;
		    }
		    // This is now a HRF_SEEK_REQUEST to generate valid 206 response
		    http_set_seek_request(phttp, mdat_offset + 8,/* start pos */
					  fp->pbox_list->box_info[1].offset + fp->pbox_list->box_info[1].size - 1
					  /* end pos */);

		    //SET_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
		    CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
		    con->ssp.seek_state = 30;

		    if (fp) {
			mp4_find_box_parser_cleanup(fp);
			con->ssp.mp4_fp_state = NULL;
		    }
		    if (bs) {
			bio_close_bitstream(bs);
			con->ssp.buffered_data = NULL;
		    }
		    //CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
		    rv = SSP_SEND_DATA_BACK;
		    break;
	    }
	} else if (con->ssp.seek_state == 30) {

	    CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
	    con->ssp.ssp_activated = TRUE;
	    rv = SSP_SEND_DATA_OTW;
	    goto exit;
	} /* if ( con->ssp.seek_state == 1 || */
    } /* if(queryData) */

 exit:
    if (rv == -NKN_SSP_FORCE_TUNNEL_PATH) {
	/*
         * for tunnel mode, reset all the value state
         * clean all the allocated buffer
         */

        if (moov && moov_out) {
            if (moov_out->data) {
                free(moov_out->data);
                moov_out->data = NULL;
            }

            mp4_moov_out_cleanup(moov_out, moov->n_tracks/* need modify this */);
            moov_out = NULL;
        }

        if (moov) {
            mp4_moov_cleanup(moov, moov->n_tracks);
            moov = NULL;
        }

	if (is_known_header_present(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI)) {
            delete_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI);
	}
        if (is_known_header_present(&phttp->hdr, MIME_HDR_X_NKN_SEEK_URI)) {
            delete_known_header(&phttp->hdr, MIME_HDR_X_NKN_SEEK_URI);
	}

        if (CHECK_HTTP_FLAG(phttp, HRF_BYTE_RANGE)) {
            CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
            phttp->brstart = 0;
            phttp->brstop = 0;
	}

	SET_HTTP_FLAG(phttp, HRF_SSP_SEEK_TUNNEL);

    }
    return rv;
}


/*
 *
 * Query for MP4 Seek/Scrub using Time Offsets
 * We will keep this function used for youtube MP4 seek
 */
int query_mp4_seek(con_t *con, const char *queryName, off_t contentLen, const char *dataBuf, int bufLen, int timeUnits)
{
    const char *queryDataTmp=NULL;
    bitstream_state_t *bs;
    int queryLen=0;
    int queryNameLen=0;

    int64_t timeOffset=0, bytesWritten = 0;
    uint64_t moov_offset=0, moov_size=0, mdat_offset=0, udta_size=0;
    uint8_t *p_udta;
    moov_t *moov = NULL, *moov_out=NULL;
    int rv=0, traks = 0, seek_ret = 0;
    http_cb_t *phttp = &con->http;
    int32_t mdat_id;
    uint64_t mdat_size=0;
    MP4_SEEK_PROVIDER_TYPE seek_type = SEEK_PTYPE_UNKNOWN;
    uint8_t *tmp_p;
    vpe_ol_t ol[N_BOX_TO_FIND]; /* ol of box in find_box_name_list */
    int fc_rv;
    int n_box_to_find = N_BOX_TO_FIND;

    tmp_p = NULL;
    bs = NULL;

    if (queryName) {
	queryNameLen = strlen(queryName);
	get_query_arg_by_name(&phttp->hdr, queryName, queryNameLen, &queryDataTmp, &queryLen);
    }

    TSTR(queryDataTmp, queryLen, queryData);
    if ( strncmp(queryData, "", 1) == 0 ) {
        queryData = NULL;
    }

    if(queryData){ //Seek query was found
        queryData != NULL ? (timeOffset = atoi(queryData)/timeUnits) : (timeOffset = 0); // timeUnits is expressed as # of clicks in a second
        DBG_LOG(MSG, MOD_SSP, "Entered mp4 seek time = %ld mp4-hdr-size %ld", timeOffset, con->ssp.mp4_header_size);

        if (timeOffset <= 0) {
	    generate_om_seek_uri(con, queryName); // Create the SEEK URI for OM
            rv = SSP_SEND_DATA_OTW;
            goto exit;
        }

        if ( contentLen == OM_STAT_SIG_TOT_CONTENT_LEN && !CHECK_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH) ) { // file was not found
            DBG_LOG(WARNING, MOD_SSP, "MP4Seek: Requested file for seek not found, re-try, forward to client. Disable seek");
	    rv = -NKN_SSP_FORCE_TUNNEL_PATH;
	    goto exit;
        } else if ( !con->ssp.seek_state ){//&& contentLen == 0 ) {
	    // First entry, to initiate internal loop-back for MP4 header fetch
            DBG_LOG(MSG, MOD_SSP, "MP4Seek: First entry to start internal fetch");

            con->ssp.seek_state = 10; // Persistent state variable
            generate_om_seek_uri(con, queryName); // Create the SEEK URI for OM

            if (CHECK_HTTP_FLAG(phttp, HRF_BYTE_RANGE)) { // Clear any byte range requests sent by flash player
                CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
                phttp->brstart = 0;
                phttp->brstop = 0;
            }
	    CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
	    //SET_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
            rv = SSP_SEND_DATA_BACK;
            goto exit;
        } else if ( con->ssp.seek_state == 10 || con->ssp.seek_state == 20) {
	    // Second entry, to determine how much we need to get entire MP4 header
	    DBG_LOG(MSG, MOD_SSP, "MP4Seek: Next entry to fetch subsequent chunks, seek_state: %d", con->ssp.seek_state);
	    //	    delete_known_header(&phttp->hdr, MIME_HDR_X_NKN_SEEK_URI);
	    DBG_LOG(MSG, MOD_SSP, "MP4Seek: bufLen: %d, contentLen: %ld, seek_state: %d",
		    bufLen, contentLen, con->ssp.seek_state);
            if ( bufLen > 0 && con->ssp.ssp_partial_content_len < contentLen ) {

                // Do the buffered read
                if(!con->ssp.buffered_data) {
                    con->ssp.mp4_header = (uint8_t *)nkn_calloc_type(MP4_INIT_BYTE_RANGE_LEN, sizeof(char),  mod_ssp_mp4_rebased_seekheader_t);
                    if (con->ssp.mp4_header == NULL) {
                        DBG_LOG(MSG, MOD_SSP, "MP4Seek: Failed to allocate space for MP4 header");
			rv = -NKN_SSP_FORCE_TUNNEL_PATH;
                        goto exit;
                    }
                    con->ssp.buffered_data = bio_init_bitstream(con->ssp.mp4_header, MP4_INIT_BYTE_RANGE_LEN);
		    if (con->ssp.buffered_data == NULL) {
			DBG_LOG(MSG, MOD_SSP, "MP4Seek: Failed to init the data buffer");
			rv = -NKN_SSP_FORCE_TUNNEL_PATH;
			goto exit;
		    }
                    con->ssp.mp4_header_size = MP4_INIT_BYTE_RANGE_LEN;
                }
                bytesWritten = bio_aligned_write_block(con->ssp.buffered_data, (uint8_t*)dataBuf, bufLen);

                if( (bio_get_pos(con->ssp.buffered_data) > con->ssp.mp4_header_size) ) {
                    switch(con->ssp.seek_state) {
                        case 10:
                            bs = (bitstream_state_t*)con->ssp.buffered_data;
			    if (con->ssp.ssp_client_id == 5){
				/*
				 * Youtube SSP use the itag query for file type
				 * add a file type detection to check whether it is the same as itag.
				 */
				uint8_t* p_file_buf;
				p_file_buf = (uint8_t*)bs->data;
				if (bs->size > 16) {
				    if (vpe_detect_media_type(p_file_buf) != con->ssp.ssp_container_id) {
					DBG_LOG(WARNING, MOD_SSP,
						"MP4: Youtube file type detect is different from itag. Have to tunnel. "
						"seek_state: %d",
						con->ssp.seek_state);
					rv = -NKN_SSP_FORCE_TUNNEL_PATH;
					goto exit;
				    }
				}
			    }
                            mp4_get_moov_info((uint8_t*)bs->data, MP4_INIT_BYTE_RANGE_LEN, &moov_offset, &moov_size);
                            if(moov_offset == 0 || moov_size == 0) {
                                DBG_LOG(WARNING, MOD_SSP, "MP4Seek: Corrupted moov offset or size. Will revert to serving full file instead");
				rv = -NKN_SSP_FORCE_TUNNEL_PATH;
                                goto exit;
                            }
                            con->ssp.mp4_header_size = moov_offset + moov_size + 8;

                            if ( bytesWritten < 0 ) {
                                tmp_p = (uint8_t *)nkn_realloc_type(con->ssp.mp4_header, bufLen + bs->size, mod_ssp_mp4_rebased_seekheader_t);
                                if (tmp_p == NULL) {
                                    DBG_LOG(MSG, MOD_SSP, "MP4Seek: Failed to allocate seek headers");
				    rv = -NKN_SSP_FORCE_TUNNEL_PATH;
                                    goto exit;
                                }
				con->ssp.mp4_header =tmp_p;
                                bio_reinit_bitstream(bs, con->ssp.mp4_header, bufLen + bs->size);
                                bio_aligned_seek_bitstream(bs, con->ssp.ssp_partial_content_len, SEEK_SET);
                                bytesWritten = bio_aligned_write_block(con->ssp.buffered_data, (uint8_t*)dataBuf, bufLen);
                            }

                            con->ssp.seek_state = 20;
                            break;
                        case 20:
                            bs = (bitstream_state_t*)con->ssp.buffered_data;
                            mp4_get_moov_info((uint8_t*)bs->data, MP4_INIT_BYTE_RANGE_LEN, &moov_offset, &moov_size);

                            moov = mp4_init_moov((uint8_t *)bs->data+moov_offset, moov_offset, moov_size);
			    if (moov == NULL) {
				DBG_LOG(WARNING, MOD_SSP, "MP4Seek: Fail to init_moov");
				rv = -NKN_SSP_FORCE_TUNNEL_PATH;
				goto exit;
			    }
                            traks = mp4_populate_trak(moov);

                            if(traks <= 0) {
                                DBG_LOG(ERROR, MOD_SSP, "MP4Seek: (Tracks in file = %d). Will revert to serving full file instead", traks);
				rv = -NKN_SSP_FORCE_TUNNEL_PATH;
                                goto exit;
                            }

			    /* ol[0] moov, ol[1] mdat, ol [2], ftyp */
			    ol[0].offset = moov_offset; // moov
			    ol[0].length = moov_size;
			    ol[1].offset = moov_offset + moov_size; //mdat
			    ol[1].length = contentLen - moov_offset - moov_size;
			    ol[2].offset = 0; //ftyp
			    ol[2].length = moov_offset;

                            mdat_offset = moov_offset + moov_size +
				(seek_ret = mp4_seek_trak(moov, traks, timeOffset, &moov_out, ol, n_box_to_find));

                            if(seek_ret < 0) {
                                DBG_LOG(WARNING, MOD_SSP, "MP4Seek: Invalid request for seek, maybe outside the timeline (seektime = %ld)", timeOffset);
				rv = -NKN_SSP_FORCE_TUNNEL_PATH;
                                goto exit;
                            }
#ifdef DAILYMOTION_SEEK_ENABLE
			    /* if the virtual player type is 0; test
			       if the seek provider is dailymotion  */
			    if (con->ssp.ssp_client_id == 0) { /* generic virtual player */
				uint8_t ptype = 0;
				int32_t err = 0;
				dm_seek_input_t dsi;
				err = mp4_test_known_seek_provider(moov,
								   SEEK_PTYPE_DAILYMOTION,
								   &ptype);
				if (ptype) {
				    dsi.tot_file_size = phttp->content_length;
				    dsi.seek_mdat_size = phttp->content_length - mdat_offset;
				    dsi.duration = mp4_get_vid_duration(moov);
				    dsi.moov_offset = moov_offset;
				    fc_rv = mp4_dm_modify_udta(moov_out, &dsi);
				    if (fc_rv < 0) {
					DBG_LOG(MSG, MOD_SSP,
						"MP4Seek: Failed to do udta modify for dailymotion. seek_state: %d",
						con->ssp.seek_state);
					rv = -NKN_SSP_FORCE_TUNNEL_PATH;
					goto exit;
				    }
				    seek_type =
					SEEK_PTYPE_DAILYMOTION;
				    goto copy_udta;
				}
			    }
#endif
			    /* For youtube seek we need to call the following API's
			       * (a) create a new udta tag with the seek position stored in it
			       * (b) make the existing udta box as free box;
			       * (c) modify the moov size and stco box to reflect the new udta tag
			       * new MP4 file will look thus
			       * moov
			       *    |--> free (old udta)
			       *    |--> udta (new)
			       * mdat
			       */
			    if (con->ssp.ssp_client_id == 5 ||
				seek_type == SEEK_PTYPE_DAILYMOTION){ // If YouTube player is enabled
				fc_rv = mp4_yt_modify_udta(moov_out);
				if (fc_rv < 0) {
				    DBG_LOG(MSG, MOD_SSP,
					    "MP4Seek: Failed to do udta modify.");
				    rv = -NKN_SSP_FORCE_TUNNEL_PATH;
				    goto exit;
				}
			    copy_udta:
				mp4_get_modified_udta(moov_out, &p_udta, &udta_size);
				mp4_modify_moov(moov_out, udta_size);

				tmp_p = (uint8_t *)nkn_realloc_type(con->ssp.mp4_header, moov_offset + moov_out->size + udta_size + 8, mod_ssp_mp4_rebased_seekheader_t);
				if (tmp_p == NULL) {
				    DBG_LOG(MSG, MOD_SSP, "MP4Seek: Failed to allocate seek headers");
				    rv = -NKN_SSP_FORCE_TUNNEL_PATH;
				    goto exit;
				}
				con->ssp.mp4_header = tmp_p;
				bio_reinit_bitstream(con->ssp.buffered_data, con->ssp.mp4_header, moov_offset + moov_out->size + udta_size + 8);

				// Overwrite the new moof box
				memcpy(con->ssp.mp4_header + moov_offset, moov_out->data, moov_out->size);
				// Append the udta box
				if (udta_size) {
					memcpy(con->ssp.mp4_header + moov_offset + moov_out->size, p_udta, udta_size);
				}

				mdat_size = nkn_vpe_swap_uint32( phttp->content_length - mdat_offset );
				memcpy(con->ssp.mp4_header + moov_offset + moov_out->size + udta_size, &mdat_size, 4);

				mdat_id = 0x7461646d;
				memcpy(con->ssp.mp4_header + moov_offset + moov_out->size + udta_size + 4, &mdat_id, 4);

				con->ssp.mp4_header_size = moov_offset + moov_out->size + udta_size + 8;

				if (moov_out->data) {
				    free(moov_out->data);
				    moov_out->data = NULL;
				}
				if (moov_out->udta_mod) {
				    free(moov_out->udta_mod);
				    moov_out->udta_mod = NULL;
				}
				if (moov->udta_mod) {
				    free(moov->udta_mod);
				    moov->udta_mod = NULL;
				}
				/*cleanup the moov box */
				mp4_moov_cleanup(moov, traks);
				moov = NULL;
				/* cleanup the output moov box */
				mp4_moov_out_cleanup(moov_out, traks);
				moov_out = NULL;
			    } else { // for all other players, e.g., Break, etc
				tmp_p = (uint8_t *)nkn_realloc_type(con->ssp.mp4_header, moov_offset + moov_out->size + 8, mod_ssp_mp4_rebased_seekheader_t);
				if (tmp_p == NULL) {
				    DBG_LOG(MSG, MOD_SSP, "MP4Seek: Failed to allocate seek headers");
				    rv = -NKN_SSP_FORCE_TUNNEL_PATH;
				    goto exit;
				}
				con->ssp.mp4_header = tmp_p;
				// Overwrite the new moof box
                                memcpy(con->ssp.mp4_header + moov_offset, moov_out->data, moov_out->size);

     				mdat_size = nkn_vpe_swap_uint32( phttp->content_length - mdat_offset );
				memcpy(con->ssp.mp4_header + moov_offset + moov_out->size, &mdat_size, 4);
				mdat_id = 0x7461646d;
				memcpy(con->ssp.mp4_header + moov_offset + moov_out->size + 4, &mdat_id, 4);
				con->ssp.mp4_header_size = moov_offset + moov_out->size + 8;

				free(moov_out->data);

				/*cleanup the moov box */
                                mp4_moov_cleanup(moov, traks);
                                /* cleanup the output moov box */
                                mp4_moov_out_cleanup(moov_out, traks);
			    }

                            goto escapemp4;
                            break;
                    }
                } else { // looped buffered read
		    bs = (bitstream_state_t*)con->ssp.buffered_data;
                    if ( bytesWritten < 0 ) {
                        tmp_p = (uint8_t *)nkn_realloc_type(con->ssp.mp4_header, bufLen + bs->size, mod_ssp_mp4_rebased_seekheader_t);
                        if (tmp_p == NULL) {
                            DBG_LOG(MSG, MOD_SSP, "MP4Seek: Failed to allocate seek headers");
			    rv = -NKN_SSP_FORCE_TUNNEL_PATH;
                            goto exit;
                        }
			con->ssp.mp4_header = tmp_p;
                        bio_reinit_bitstream(con->ssp.buffered_data, con->ssp.mp4_header, bufLen + bs->size);
                        bio_aligned_seek_bitstream(con->ssp.buffered_data, con->ssp.ssp_partial_content_len, SEEK_SET);
                        bytesWritten = bio_aligned_write_block(con->ssp.buffered_data, (uint8_t*)dataBuf, bufLen);
                    }
                }

                con->ssp.ssp_partial_content_len += bufLen;
                rv = SSP_WAIT_FOR_DATA;
                goto exit;
            } else {
		// Tunnel the seek request in this scenario
		DBG_LOG(MSG, MOD_SSP, "MP4Seek: tunnel the request, seek_state: %d", con->ssp.seek_state);
		rv = -NKN_SSP_FORCE_TUNNEL_PATH;
		goto exit;
	    }//if ( bufLen > 0 &&

        escapemp4: // All data required for re-basing has been obtained
            // Now set the correct SEEK_RANGE_REQUEST, generate the pre-pend header and SEND_OTW
            if (CHECK_HTTP_FLAG(phttp, HRF_BYTE_RANGE)) {
                CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
                phttp->brstart = 0;
                phttp->brstop = 0;
            }
            // This is now a HRF_SEEK_REQUEST to generate valid 206 response
            http_set_seek_request(phttp, mdat_offset+8, contentLen - 1); //+8

            //con->ssp.ssp_activated = TRUE;
	    //            rv = SSP_SEND_DATA_OTW;

	    //	    SET_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
	    /* A test for this */
	    CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
	    con->ssp.seek_state = 30;
	    bio_close_bitstream(bs);
	    bs = NULL;
	    con->ssp.buffered_data = NULL;
	    rv = SSP_SEND_DATA_BACK;
            goto exit;

        } else if ( con->ssp.seek_state == 30) {

	    if ( contentLen == OM_STAT_SIG_TOT_CONTENT_LEN ) { // Tunnel the seek request in this scenario
		DBG_LOG(MSG, MOD_SSP, "MP4Seek: file not in cache, seek is tunneled");
		rv = -NKN_SSP_FORCE_TUNNEL_PATH;
		goto exit;

	    } else { // We can serve this seek request locally from cache
		CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
		con->ssp.ssp_activated = TRUE;

		rv = SSP_SEND_DATA_OTW;
		goto exit;
	    }

	} // if ( con->ssp.seek_state == 1 ||
    }//end if queryData

 exit:
    if (rv == -NKN_SSP_FORCE_TUNNEL_PATH) {
        /*
         * for tunnel mode, reset all the value state
         * clean all the allocated buffer
         */
	if (moov_out && moov) {
	    if (moov_out->data) {
		free(moov_out->data);
		moov_out->data = NULL;
	    }

	    mp4_moov_out_cleanup(moov_out, moov->n_tracks);
	    moov_out = NULL;
	}

	if (moov) {
	    mp4_moov_cleanup(moov, moov->n_tracks);
	    moov = NULL;
	}

        if (is_known_header_present(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI)) {
            delete_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI);
        }
        if (is_known_header_present(&phttp->hdr, MIME_HDR_X_NKN_SEEK_URI)) {
            delete_known_header(&phttp->hdr, MIME_HDR_X_NKN_SEEK_URI);
        }

        if (CHECK_HTTP_FLAG(phttp, HRF_BYTE_RANGE)) {
            CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
            phttp->brstart = 0;
            phttp->brstop = 0;
        }

	/* cleanSSP_force_tunnel will clean this flag */
	//CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);

	SET_HTTP_FLAG(phttp, HRF_SSP_SEEK_TUNNEL);
    }
    return rv;
}


void
writeMDAT_Header(char *p_src, uint64_t mdat_size)
{

    int32_t mdat_id = 0x7461646d; //0x6d646174;

    memcpy(p_src, &mdat_size, 4);
    memcpy(p_src + 4, &mdat_id, 4);

}

/*
 *
 * Query Routine for FLV Seek/Scrub using Time Offsets
 *
 */

const char *modifier_list[] = {"duration",
                               "starttime",
                               "bytelength"};
const int32_t modifier_list_count = 3;
const char flv_header[13] = {0x46, 0x4C, 0x56, 0x01,
                             0x05, 0x00, 0x00, 0x00,
                             0x09, 0x00, 0x00, 0x00, 0x00};

int query_flv_timeseek(con_t *con, const char *queryName, off_t contentLen, const char *dataBuf, int bufLen, int timeUnits)
{
    const char *queryDataTmp=NULL;
    int queryLen=0;
    int queryNameLen=0;
    int64_t timeOffset=0;
    int64_t seek_pos=0;
    int rv=0;
    int fc_rv;
    http_cb_t *phttp = &con->http;
    flv_parser_t *fp;
    uint64_t write_offset;

    int seek_found = 0; // False

    if (queryName) {
	queryNameLen = strlen(queryName);
	get_query_arg_by_name(&phttp->hdr, queryName, queryNameLen, &queryDataTmp, &queryLen);
    }

    TSTR(queryDataTmp, queryLen, queryData);
    if ( strncmp(queryData, "", 1) == 0 ) {
        queryData = NULL;
    }

    if(queryData){ //Seek query was found
        queryData != NULL ? (timeOffset = atoi(queryData)/timeUnits) : (timeOffset = 0); // timeUnits is expressed as # of clicks in a second
        DBG_LOG(MSG, MOD_SSP, "Entering FLV timeseek to = %ld secs", timeOffset);

        if (timeOffset <= 0) { // graceful return, plays from the beginning
	    generate_om_seek_uri(con, queryName); // Create the SEEK URI for OM
            rv = SSP_SEND_DATA_OTW;
            goto exit;
        }

	if (!con->ssp.seek_state) {//&& contentLen==0 && dataBuf==NULL ) {
            // First entry, to initiate internal loop-back for file fetch
            con->ssp.seek_state = 5; // Persistent state variable
            generate_om_seek_uri(con, queryName); // Create the SEEK URI for OM

            if (CHECK_HTTP_FLAG(phttp, HRF_BYTE_RANGE)) {
                // Clear any byte range requests sent by flash player
                CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
                phttp->brstart = 0;
                phttp->brstop = 0;
            }
	    SET_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
            DBG_LOG(MSG, MOD_SSP, "FLVSeek: (1) Request to fetch file internally to parse to seek offset");
            rv = SSP_SEND_DATA_BACK;
            goto exit;
	    /*
             * Move the check of fetch data availability below the state 0
             * As the data availability check is only for current fucntion
             */
        } else if (contentLen == OM_STAT_SIG_TOT_CONTENT_LEN &&
		   !CHECK_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH)) { // File not found
	    DBG_LOG(WARNING, MOD_SSP,
		    "FLVSeek: (2) File not found, re-try and forward to client w/o seeking. "
		    "seek_state: %d", con->ssp.seek_state);
	    rv = -NKN_SSP_FORCE_TUNNEL_PATH;
	    goto exit;
	} else if (contentLen == OM_STAT_SIG_TOT_CONTENT_LEN &&
		   CHECK_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH)) {
	    DBG_LOG(WARNING, MOD_SSP,
		    "FLVSeek: (2) File not in cache, re-try and forward to client w/o seeking. "
		    "seek_state: %d", con->ssp.seek_state);
	    rv = -NKN_SSP_FORCE_TUNNEL_PATH;
	    goto exit;
	} else if (con->ssp.seek_state == 5){

	    if (con->ssp.ssp_client_id == 5){
		/*
		 * Youtube SSP use the itag query for file type
		 * add a file type detection to check whether it is the same as itag.
		 */
		uint8_t* p_file_buf;
		p_file_buf = (uint8_t*)dataBuf;
		if (bufLen > 16) {
		    if (vpe_detect_media_type(p_file_buf) != con->ssp.ssp_container_id) {
			DBG_LOG(WARNING, MOD_SSP,
				"FLVSeek: Youtube file type detect is different from itag. Have to tunnel. "
				"seek_state: %d",
				con->ssp.seek_state);
			rv = -NKN_SSP_FORCE_TUNNEL_PATH;
			goto exit;
		    }
		}
	    }

	    // Now check if entire file is present in local cache, else tunnel
	    if (is_known_header_present(&phttp->hdr, MIME_HDR_X_NKN_SEEK_URI)) {
		delete_known_header(&phttp->hdr, MIME_HDR_X_NKN_SEEK_URI);
	    }
	    SET_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
	    phttp->brstart = contentLen-128;
	    phttp->brstop = contentLen - 1;
	    SET_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);

	    con->ssp.seek_state = 6;
	    rv = SSP_SEND_DATA_BACK;
	    goto exit;

	} else if (con->ssp.seek_state == 6) {
	    if ( contentLen == OM_STAT_SIG_TOT_CONTENT_LEN ) { // Tunnel the seek request in this scenario
		rv = -NKN_SSP_FORCE_TUNNEL_PATH;
                goto exit;
            } else { // We can serve this seek request locally from cache
                CLEAR_HTTP_FLAG(phttp, HRF_SSP_CACHE_ONLY_FETCH);
		if (CHECK_HTTP_FLAG(phttp, HRF_BYTE_RANGE)) {
                    CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
                    phttp->brstart = 0;
                    phttp->brstop = 0;
                }

		con->ssp.seek_state = 10;
                rv = SSP_SEND_DATA_BACK;
                goto exit;
	    }
	} else if ( con->ssp.seek_state==10 ||  con->ssp.seek_state==20 ) {
            // Second entry, to start fetching/parsing chunks of media data

            if ( con->ssp.ssp_partial_content_len < contentLen && bufLen > 0 ) {
                switch(con->ssp.seek_state) {
                    case 10:
                        fp = flv_init_parser();
                        if (!fp) {
                            DBG_LOG(MSG, MOD_SSP, "FLVSeek: Unable it init flv parser");
			    rv = -NKN_SSP_FORCE_TUNNEL_PATH;
			    goto exit;
                        }
                        con->ssp.fp_state = fp; // Store state for future re-entry

                        if ( flv_parse_header(fp, (uint8_t*)dataBuf, bufLen) == E_VPE_FLV_DATA_INSUFF ){
                            DBG_LOG(MSG, MOD_SSP,
				    "FLVSeek: not enough data for flv parser header");
                            rv = -NKN_SSP_FORCE_TUNNEL_PATH;
                            goto exit;
                        }
			DBG_LOG(MSG, MOD_SSP,
				"FLVSeek: (3) FLV Header parsed successfully, moving on to find timestamp. "
				"seek_state: %d", con->ssp.seek_state);
                        con->ssp.seek_state = 20;
                        //Set the seek params - seek time, meta data modifier list etc
			if (con->ssp.ssp_client_id == 5){ // If YouTube player is enabled
			    fc_rv = flv_set_seekto(fp, timeOffset, modifier_list, modifier_list_count);
			} else {
			    fc_rv = flv_set_seekto(fp, timeOffset, NULL, 0);
			}

			if (fc_rv < 0) {
                            DBG_LOG(MSG, MOD_SSP,
                                    "FLVSeek: fail to set flv seekto");
                            rv = -NKN_SSP_FORCE_TUNNEL_PATH;
                            goto exit;
			}
                        // No break, just fall through to case 2...

                    case 20:
                        // Parse a block of FLV data
                       if((seek_pos = flv_parse_data(con->ssp.fp_state, (uint8_t*)dataBuf, bufLen)) > 0) {
                            DBG_LOG(MSG, MOD_SSP, "FLVSeek: Time to Byte translated to %ld", seek_pos);
			    seek_found = 1;
                            break;
		       }
		       /**
			* currently FLV parser is incapble of handling
			* cases where the audio/video codec headers
			* (SPS, PPS, AAC_SI) span buffer pages. we
			* tunnel such requests
			*/

		       if (seek_pos <= -E_VPE_FLV_PARSER_SPAN_UNHANDLED) {
			   /* parser error un-handled data spanning */
			   rv = -NKN_SSP_FORCE_TUNNEL_PATH;
			   DBG_LOG(SEVERE, MOD_SSP, "FLVSeek:"
				   " Unhandled data span in"
				   " Audio/Video codec headers,"
				   " errcode %d",
				   E_VPE_FLV_PARSER_SPAN_UNHANDLED);
			   glob_flv_parse_data_span_err++;
			   goto exit;
		       }
		       con->ssp.ssp_partial_content_len += bufLen;
		       rv = SSP_WAIT_FOR_DATA;
		       goto exit;
		       break;
                } //switch
            } //con->ssp.ssp_partial_content_len ...

	    if (seek_found) {
		// All data required for re-basing has been obtained
		// Now set the correct SEEK_RANGE_REQUEST, generate the pre-pend header and SEND_OTW
		// printf("FLVSeek: Time to Byte translated to %ld", seek_pos);
		if (CHECK_HTTP_FLAG(phttp, HRF_BYTE_RANGE)) {
		    CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
		    phttp->brstart = 0;
		    phttp->brstop = 0;
		}
		// This is now a HRF_SEEK_REQUEST to generate valid 206 response
		http_set_seek_request(phttp, seek_pos, 0);

		// Create the new FLV header based on seek offset request

		/* pre - pend data; organized as follows
		 * ---------------------------------------------------------------------------------------
		 * | FLV   (13)| FLV META (11) | META DATA | SPS+PPS (Y) | AAC DSI (Z) | SEEK DATA
		 * | HEADER    | TAG HDR       | (X)       | (AVC only)  | (AAC only)  |
		 * ---------------------------------------------------------------------------------------
		 * AAC DSI - AAC decoder specific information
		 *  ----------------------------------------------------------------------
		 * | element         |   pre-pend data pointers | pre-pend data size       |
		 *  ----------------------------------------------------------------------
		 * | FLV META TAG HDR|   fp->mt->tag            | 11 bytes                 |
		 * | FLV META DATA   |   fp->mt->data           | fp->mt->size             |
		 * | SPS+PPS         |   fp->avcc               | fp->avcc_size            |
		 * | AAC DSI         |   fp->aac_si             | fp->aac_si_size          |
		 *  -----------------------------------------------------------------------
		 * Total prepend data size = 13 + 11 + META DATA SIZE + SPS/PPS size + AAC DSI size
		 *                         = 13 + 11 + fp->mt->size + fp->avcc_size + fp->aac_si_size
		 */
		if (con->ssp.fp_state == NULL) {
		    /* con->ssp.fp_state should not be NULL */
		    const char *tmpData;
		    int uriLen = 0, uriCnt = 0;
		    unsigned int uriAttrs = 0;

		    DBG_LOG(ERROR, MOD_SSP,
			    "FLVSeek: seek_found == 1, and fp_state is NULL. Tunnel request. "
			    "seek_state: %d", con->ssp.seek_state);

		    if (!get_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI,
					  &tmpData, &uriLen, &uriAttrs,&uriCnt)) {
			DBG_LOG(ERROR, MOD_SSP,
				"FLVSeek: REMAP URI: %s", tmpData);
		    }
		    rv = -NKN_SSP_FORCE_TUNNEL_PATH;
		    goto exit;
		}

		con->ssp.header_size = 13 + 11 +
					con->ssp.fp_state->mt->size +
					con->ssp.fp_state->avcc_size +
					con->ssp.fp_state->aac_si_size +
					((con->ssp.fp_state->xmp_mt && (con->ssp.fp_state->xmp_mt->size > 0)) ? \
					 11 + con->ssp.fp_state->xmp_mt->size : 0);

		con->ssp.header = (uint8_t *)nkn_calloc_type(con->ssp.header_size,
							     sizeof(char),
							     mod_ssp_mp4_rebased_seekheader_t);
		if (con->ssp.header == NULL) {
		    DBG_LOG(MSG, MOD_SSP, "FLVSeek: Failed to allocate seek header");
		    rv = -NKN_SSP_FORCE_TUNNEL_PATH;
		    goto exit;
		}

		write_offset = 0;
		memcpy(con->ssp.header, 	               flv_header,
		       13);                                 // FLV Header
		write_offset += 13;                         //FLV Header offset
		memcpy(con->ssp.header + write_offset,     &con->ssp.fp_state->mt->tag,
		       sizeof(nkn_vpe_flv_tag_t));          // Meta Data TAG Hdr
		write_offset += sizeof(nkn_vpe_flv_tag_t);  //Meta Data Tag Hdr size
		memcpy(con->ssp.header + write_offset,     con->ssp.fp_state->mt->data,
		       con->ssp.fp_state->mt->size);        // Meta Data
		write_offset += con->ssp.fp_state->mt->size;// Meta Data size
		if (con->ssp.fp_state->xmp_mt && (con->ssp.fp_state->xmp_mt->size > 0)) {
		    memcpy(con->ssp.header + write_offset, &con->ssp.fp_state->xmp_mt->tag,
			   sizeof(nkn_vpe_flv_tag_t));      // XMP Meta Data TAG Hdr
		    write_offset += sizeof(nkn_vpe_flv_tag_t);
		    memcpy(con->ssp.header + write_offset, con->ssp.fp_state->xmp_mt->data,
			   con->ssp.fp_state->xmp_mt->size); // XMP Meta Data
		    write_offset += con->ssp.fp_state->xmp_mt->size;
		}
		if (con->ssp.fp_state->avcc) {   // SPS/PPS for AVC
		    memcpy(con->ssp.header + write_offset, con->ssp.fp_state->avcc,
			   con->ssp.fp_state->avcc_size);
		    write_offset += con->ssp.fp_state->avcc_size;
		}
		if (con->ssp.fp_state->aac_si) { // AAC DSI for AAC
		    memcpy(con->ssp.header + write_offset, con->ssp.fp_state->aac_si,
			   con->ssp.fp_state->aac_si_size);
		}

		con->ssp.ssp_activated = TRUE;
		rv = SSP_SEND_DATA_OTW;
		goto exit;
	    } else {
		/* if seek_found is FALSE, we will tunnel this request*/
		DBG_LOG(MSG, MOD_SSP,
			"FLVSeek: Failed to find the seek bytes in FLV. Tunnel request");
		if (con->ssp.fp_state == NULL) {
                    DBG_LOG(ERROR, MOD_SSP,
                            "FLVSeek: seek_found == 0, and fp_state is NULL. Tunnel request. "
		            "seek_state: %d", con->ssp.seek_state);
		}
		rv = -NKN_SSP_FORCE_TUNNEL_PATH;
		goto exit;
	    } //if (seek_found)
	} //
    } //if queryData

 exit:
    if (rv == -NKN_SSP_FORCE_TUNNEL_PATH) {
	/*
	 * for tunnel mode, reset all the value state
	 * clean all the allocated buffer
	 */
	if (is_known_header_present(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI)) {
	    delete_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI);
	}
	if (is_known_header_present(&phttp->hdr, MIME_HDR_X_NKN_SEEK_URI)) {
	    delete_known_header(&phttp->hdr, MIME_HDR_X_NKN_SEEK_URI);
	}

	if (CHECK_HTTP_FLAG(phttp, HRF_BYTE_RANGE)) {
	    CLEAR_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
	    phttp->brstart = 0;
	    phttp->brstop = 0;
	}

	SET_HTTP_FLAG(phttp, HRF_SSP_SEEK_TUNNEL);
    }
    return rv;
}
