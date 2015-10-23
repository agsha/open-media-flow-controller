/*
 * Yahoo Server Side Player
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

extern unsigned long long glob_ssp_hash_failures;
extern unsigned long long glob_ssp_container_flv, glob_ssp_container_mp4;


/////////////////////////////////////////////////
// Yahoo Server Side Player (Yahoo SSP Type 3) //
/////////////////////////////////////////////////
int
yahooSSP(con_t *con, const char *uriData, int uriLen, const char *hostData, int hostLen,  const ssp_config_t *ssp_cfg)
{
    const char *queryData;
    int remapLen=0;
    int rv, queryrv;
    int queryLen=0;
    //    int64_t steadyRate=0;
    //    int autoAFR=0;

    http_cb_t *phttp = &con->http;
    queryData = NULL;

    // Obtain the relative URL length stripped of all query params
    if ( (remapLen = findStr(uriData, "?", uriLen)) == -1 ) {
	remapLen = uriLen;
        DBG_LOG(WARNING, MOD_SSP, "URL does not contain any query params" );
	//rv = -NKN_SSP_INSUFF_QUERY_PARAM;
        //goto exit;
    }

    // Verify the hash for yahoo video URL. If successful then proceed, else reject connection
    if (ssp_cfg->req_auth.enable){
	if (verifyYahooHash(hostData, uriData, hostLen, &phttp->hdr, ssp_cfg->req_auth) != 0) { // hashes dont match
	    glob_ssp_hash_failures++;
	    if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
                DBG_TRACE("Hash authentication failed for virtual player: %s" , ssp_cfg->player_name);
            }
	    DBG_LOG(WARNING, MOD_SSP, "Hash Verification Failed. Closing connection (Error: %d)", NKN_SSP_HASHCHECK_FAIL );
	    rv = -NKN_SSP_HASHCHECK_FAIL;  //Close/Refuse connection
	    goto exit;
	}
	else {
	    if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
                DBG_TRACE("Hash authentication sucsessful for virtual player: %s" , ssp_cfg->player_name);
            }
	}
    }

    // Check for presence of Health Probe in request
    queryData = NULL;
    if (ssp_cfg->health_probe.enable){
	if (ssp_cfg->health_probe.uri_query && ssp_cfg->health_probe.matchstr){
	    get_query_arg_by_name(&phttp->hdr, ssp_cfg->health_probe.uri_query, strlen(ssp_cfg->health_probe.uri_query), &queryData, &queryLen);
	    if (queryData){
		if ( !strcmp(queryData, ssp_cfg->health_probe.matchstr) ) {
		    http_set_no_cache_request(phttp); // Set the No Cache Flag
		    if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
			DBG_TRACE("Health probe request enabled and no cache flag is set");
		    }

		    rv = SSP_SEND_DATA_OTW;
		    goto exit;
		}//strcmp
	    }//if (queryData
	}
	else{
	    DBG_LOG(WARNING, MOD_SSP, "Health Probe URI Query Param or Match string are unspecified in CLI");
	    return -NKN_SSP_CLI_PARAM_UNSPECIFIED;
	}
    }

    //Check for seek request
    if (ssp_cfg->seek.enable && ssp_cfg->seek.uri_query) {
        queryrv = query_flv_seek(con, ssp_cfg->seek.uri_query, ssp_cfg->seek.query_length);
        if ( queryrv < 0 ){
            rv = queryrv;
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

    //get the container format
    if( (memcmp(uriData + remapLen - 4, ".flv", 4) == 0) || (memcmp(uriData + remapLen - 4, ".FLV", 4) == 0) ){
        con->ssp.ssp_container_id = CONTAINER_TYPE_FLV;
        glob_ssp_container_flv++;
    }
    else if ( (memcmp(uriData + remapLen - 4, ".mp4", 4) == 0) || (memcmp(uriData + remapLen - 4, ".MP4", 4) == 0) ){
        con->ssp.ssp_container_id = CONTAINER_TYPE_MP4;
        glob_ssp_container_mp4++;
    }
    else {
        DBG_LOG(MSG, MOD_SSP, "Requested container format not supported. Passing the file through anyways: %d", NKN_SSP_CONTAINER_FRMT_NOT_SUPP);
    }

    con->ssp.ssp_activated = TRUE;

    //Remap to URI to serve video
    add_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI, uriData, remapLen);

    rv = SSP_SEND_DATA_OTW;

 exit:
    return rv;

}
