/*
 * Nokeena Smoothflow Server Side Player
 *
 */

#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <openssl/md5.h>
#include <alloca.h>
#include <pthread.h>
#include <math.h>

#include "http_header.h"
#include "nkn_errno.h"
#include "nkn_cfg_params.h"
#include "nkn_debug.h"
#include "nkn_common_config.h"
#include "nkn_stat.h"

#include "ssp.h"
#include "nkn_ssp_players.h"
#include "ssp_authentication.h"
#include "ssp_queryparams.h"
#include "nkn_vpe_metadata.h"
#include "nkn_ssp_sessionbind.h"
#include "nkn_vpemgr_defs.h"

#define TSTR(_p, _l, _r)  char *(_r); ( 1 ? ( (_r) = alloca((_l)+1), memcpy((_r), (_p), (_l)), (_r)[(_l)] = '\0' ) : 0 )

#define DEFAULT_START_PROFILE 2
#define PROFILE_CHUNK_STR_LEN 21
#define INTERNAL_META_FILE_SUFF_LEN 4
#define PUB_META_FILE_SUFF_LEN 9
#define QUERY_PARAM_LEN strlen(ssp_cfg->sf_signals.state_uri_query)
#define URI_APPEND_LEN (existing_sobj->vidname_end)

#define INTERNAL_META_REQ_LEN URI_APPEND_LEN+QUERY_PARAM_LEN+INTERNAL_META_FILE_SUFF_LEN+4
#define PUB_META_REQ_LEN URI_APPEND_LEN+QUERY_PARAM_LEN+PUB_META_FILE_SUFF_LEN+1
#define CHUNK_REQ_LEN URI_APPEND_LEN+QUERY_PARAM_LEN+PROFILE_CHUNK_STR_LEN+4
#define CLIENT_META_REQ_LEN URI_APPEND_LEN+QUERY_PARAM_LEN+INTERNAL_META_FILE_SUFF_LEN+4

extern unsigned long long glob_ssp_hash_failures;

/* CORE SMOOTHFLOW FUNCTIONS - SMOOTH FLOW STATE HANDLERS*/
int smoothflowDisabled(con_t *con);
int smoothflowMediaController(con_t *con, const char *uriData, int uriLen, off_t contentLen, const char *dataBuf, int bufLen, char *sf_session_id, int remapLen, const char *hostData, int hostLen, const ssp_config_t *ssp_cfg);
int smoothflowMetaController(con_t *con, const char *uriData, int uriLen, char *sf_session_id, int remapLen);
int smoothflowSignalController(con_t *con, char *sf_session_id, const ssp_config_t *ssp_cfg);
int smoothflowIngestController(con_t *con, char *sf_session_id, const char *uriData, int uriLen, const char *hostData, int hostLen, const char *stateQuery, int stateQueryLen);
int smoothflowLocalHostFetch(con_t *con, char *sf_session_id, const char *uriData, int uriLen);
int smoothflowInterMFDRequest(con_t *con, const ssp_config_t *ssp_cfg);


/* UTILITY FUNCTIONS */
uint32_t getVidNameOffset(const char *uriData, int32_t remapLen);
int initSessionObj(nkn_ssp_session_t *, uint32_t startProfile, uint32_t startChunk, uint32_t con_max_bandwidth);
int getStartChunk(con_t *con, meta_data_reader *mdr, sf_meta_data *sf,const ssp_config_t *ssp_cfg);
static inline int getStartProfileFromURI(const char *uriData, int uriLen);
static inline int getStartProfile(con_t *con, const char *uriData, int uriLen, const ssp_config_t *ssp_cfg, sf_meta_data *sf);

///////////////////////////////////////////////////////////////////
// Nokeena Smoothflow Server Side Player (Smoothflow SSP Type 4) //
///////////////////////////////////////////////////////////////////
int
smoothflowSSP(con_t *con, const char *uriData, int uriLen, const char *hostData, int hostLen,  const ssp_config_t *ssp_cfg, off_t contentLen, const char *dataBuf, int bufLen )
{
    const char *queryData;
    int queryLen=0;
    int remapLen=0;
    int sf_state=0;
    char sf_session_id[MAX_NKN_SESSION_ID_SIZE];
    int rv=0;

    http_cb_t *phttp = &con->http;

    // Obtain the relative URL length stripped of all query params
    if ( (remapLen = findStr(uriData, "?", uriLen)) == -1 ) {
        rv = -NKN_SSP_INSUFF_QUERY_PARAM;
        DBG_LOG(WARNING, MOD_SSP, "URL does not contain any query params. Expects at least the session id and smoothflow state params (Error: %d)", -NKN_SSP_INSUFF_QUERY_PARAM );
        goto exit;
    }

    // Control Point, Session ID, State and Profile query params must be specified in CLI to proceed with SF
    if(ssp_cfg->sf_control == NULL){
	DBG_LOG(WARNING, MOD_SSP, "Required param, Control Signal unspecified in CLI, closing con");
        return -NKN_SSP_CLI_PARAM_UNSPECIFIED;
    }


    if ( strcasecmp(ssp_cfg->sf_control, "player") ){
	DBG_LOG(WARNING, MOD_SSP, "Currently Smoothflow is only supported with Control point as player");
        return -NKN_SSP_SF_INVALID_CONTROL_POINT;
    }

    if(ssp_cfg->sf_signals.session_id_uri_query == NULL){
        DBG_LOG(WARNING, MOD_SSP, "Required param, Session ID query unspecified in CLI, closing con");
        return -NKN_SSP_CLI_PARAM_UNSPECIFIED;
    }
    if(ssp_cfg->sf_signals.state_uri_query == NULL){
        DBG_LOG(WARNING, MOD_SSP, "Required param, SF State query unspecified in CLI, closing con");
        return -NKN_SSP_CLI_PARAM_UNSPECIFIED;
    }
    if(ssp_cfg->sf_signals.profile_uri_query == NULL){
        DBG_LOG(WARNING, MOD_SSP, "Required param, Profile query unspecified in CLI, closing con");
        return -NKN_SSP_CLI_PARAM_UNSPECIFIED;
    }

    //////////////////////////////  smooth flow Type 4 //////////////////////////////
    // Verify the hash for video URL. If successful then proceed, else reject connection
    if (ssp_cfg->hash_verify.enable){
        if (verifyMD5Hash(hostData, uriData, hostLen, &phttp->hdr, ssp_cfg->hash_verify) != 0) { // hashes dont match
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
                DBG_TRACE("Hash authentication successful for virtual player: %s" , ssp_cfg->player_name);
            }
	}
    }


    ///////////////////////////////////////////////////////////
    // Read the SmoothFlow Query parameter state
    queryData = NULL;
    queryLen=0;

    get_query_arg_by_name(&phttp->hdr, ssp_cfg->sf_signals.state_uri_query, strlen(ssp_cfg->sf_signals.state_uri_query), &queryData, &queryLen);
    if (queryData){
	sf_state = atoi(queryData);
    }
    else {
        rv = -NKN_SSP_SF_STATE_QUERY_MISSING;
        DBG_LOG(WARNING, MOD_SSP, "Required smoothflow state query param missing. (Error: %d)", rv);
        goto exit;
    }

    //////////////////////////////////////////////////////////
    // Read the Session ID
    queryData = NULL;
    queryLen=0;

    get_query_arg_by_name(&phttp->hdr, ssp_cfg->sf_signals.session_id_uri_query, strlen(ssp_cfg->sf_signals.session_id_uri_query), &queryData, &queryLen);
    if (queryData){
	memset( (char *)sf_session_id, 0 , MAX_NKN_SESSION_ID_SIZE);
	if (queryLen < MAX_NKN_SESSION_ID_SIZE){
 	    strncpy( sf_session_id, queryData, queryLen );
	}
	else{
	    strncpy( sf_session_id, queryData, MAX_NKN_SESSION_ID_SIZE-1 );
	    sf_session_id[MAX_NKN_SESSION_ID_SIZE-1] = '\0';
	}
    }
    else {
	if (sf_state == 1 || sf_state == 2 || sf_state == 3){
	    rv = -NKN_SSP_SF_SESSIONID_QUERY_MISSING;
	    DBG_LOG(WARNING, MOD_SSP, "Required smoothflow Session ID query param missing. (Error %d)", rv);
	    goto exit;
	}
    }

    if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
	DBG_TRACE("Logic for smoothflow state %d enabled" , sf_state);
    }

    ///////////////////////////////////////////////////////////////
    // Determine flow based on state of smoothflow query parameter
    switch (sf_state)
	{
	    case 0:
		// State 0: Disable Smoothflow, pass through as a regular request
                // http://www.abc.com/sf/foo_p01.flv?sf=0
		rv = smoothflowDisabled(con);
		break;

	    case 1:
		// State 1: Media Data Connection
		// http://www.abc.com/sf/foo_p01.flv?sid=01&sf=1

		rv = smoothflowMediaController(con, uriData, uriLen, contentLen, dataBuf, bufLen, sf_session_id, remapLen, hostData, hostLen, ssp_cfg);
		break;

	    case 2:
		// State 2: Meta Data Connection
		// http://www.abc.com/sf/foo.xml?sid=01&sf=2

		rv = smoothflowMetaController(con, uriData, uriLen, sf_session_id, remapLen);
		break;

	    case 3:
		// State 3: Control Signal Data Connection
		// http://www.abc.com/sf/foo?sid=01&sf=3&pf=2

		rv = smoothflowSignalController(con, sf_session_id, ssp_cfg);
		break;

	    case 4:
		// State 4: Trigger Signal to initiate smoothflow preprocessing
                // http://www.abc.com/sf/foo_meta.dat?sf=4

		rv = smoothflowIngestController(con, sf_session_id, uriData, uriLen, hostData, hostLen, \
						 ssp_cfg->sf_signals.state_uri_query, strlen(ssp_cfg->sf_signals.state_uri_query));
		break;

	    case 5:
		// State 5: Local Loop Fetch for Preprocessing
                // http://www.abc.com/sf/foo_meta.dat?sf=5

		rv = smoothflowLocalHostFetch(con, sf_session_id, uriData, uriLen);
		break;

	    case 6:
		//State 6 indicates request from another MFD

		rv = smoothflowInterMFDRequest(con, ssp_cfg);
		break;

	    default:
		rv = -NKN_SSP_SF_STATE_NOT_SUPPORTED;
		DBG_LOG(WARNING, MOD_SSP, "Unsupported state for smoothflow query param. (Error: %d)", rv);
		break;
	}

 exit:
    return rv;
}


/*
 * State 0: Disable Smoothflow, pass through as regular request
 * http://www.abc.com/sf/foo_p01.flv?sf=0
 *
 */
int
smoothflowDisabled(con_t *con)
{
    int rv=0;

    if ( sess_assured_flow_rate == 0) {
	rv = SSP_SEND_DATA_OTW;
    }
    else if ( sess_assured_flow_rate < MIN( (signed)con->max_client_bw, (signed)con->pns->max_allowed_bw) ) {
	con_set_afr(con, sess_assured_flow_rate);
	DBG_LOG(MSG, MOD_SSP, "AFR Set: %ld", sess_assured_flow_rate);
	rv = SSP_SEND_DATA_OTW;
    }
    else {
	// Reject connection, since requested rate > available bandwidth
	DBG_LOG(WARNING, MOD_SSP, "AFR Failed: %ld Unavailable BW", sess_assured_flow_rate);
	rv =  -NKN_SSP_AFR_INSUFF;
    }

    return rv;
}

/*
 * State 6: Disable Smoothflow, pass through as regular request to another MFD serving as Origin/Peer
 * http://www.abc.com/sf/foo_p01_c0000000009.flv?sf=6
 *
 */
int
smoothflowInterMFDRequest(con_t *con, const ssp_config_t *ssp_cfg)
{
    int rv=0;

    if (ssp_cfg->con_max_bandwidth > 0){

	if ( ssp_cfg->con_max_bandwidth <= (uint64_t) MIN(con->max_client_bw, (unsigned)con->pns->max_allowed_bw) ) {
	    con_set_afr(con, ssp_cfg->con_max_bandwidth);
	    DBG_LOG(MSG, MOD_SSP, "AFR set to con_max_bandwidth at  %ld Bytes/sec", ssp_cfg->con_max_bandwidth);
	}
	else { // AFR cannot be honoured
	    rv = -NKN_SSP_AFR_INSUFF;
	    DBG_LOG(WARNING, MOD_SSP, "Smoothflow AFR request failed: %ld. Please check if network connection max bw is limiting AFR", ssp_cfg->con_max_bandwidth);
	    return rv;
	}
    }
    else{
	// Do nothing, later we will auto-detect and use the bit-rate as AFR
	// Comment - since we do not know the bandwidth of the current profile (dat file unavailable to get the bitrate map
	// we cannot ascertain if there is enough bandwidth or not to reject the connection.
	DBG_LOG(MSG, MOD_SSP, "AFR will be set to best effort on system availability");
    }

    rv = SSP_SEND_DATA_OTW;
    return rv;
}

/*
 * State 1: Media Data Connection
 * http://www.abc.com/sf/foo_p01.flv?sid=01&sf=1
 *
 */
int
smoothflowMediaController(con_t 	*con,
			  const char 	*uriData,
			  int 		uriLen,
			  off_t 	contentLen,
			  const char 	*dataBuf,
			  int 		bufLen,
			  char 		*sf_session_id,
			  int 		remapLen,
			  const char 	*hostData,
			  int 		hostLen,  
			  const ssp_config_t *ssp_cfg)
{
    UNUSED_ARGUMENT(uriLen);

    nkn_ssp_session_t *session_obj = NULL;
    nkn_ssp_session_t *existing_sobj = NULL;
    sf_meta_data *sf = NULL;
    meta_data_reader *mdr = NULL;
    char *remapBuf;
    http_cb_t *phttp = &con->http;
    uint32_t sf_afr=0;
    int rv=0;
    int32_t startChunk = 1;
    uint32_t startProfile = 2;
    const char *queryData;
    int32_t queryLen = 0;
    //    float seekOffset;
    //nkn_ssp_params_t *ssp = NULL;
    //avcc_config *avcc;
    //char *p_avcc = NULL;

    // Assign Session ID (Check for already existing id)
    // Session ID is only assigned for State 1 requests

    if ( !atol(con->session_id) ) {
	// Check if Session ID is already in use before assigning to this connection
	existing_sobj = nkn_ssp_sess_get(sf_session_id);                // returns with sobj locked

	if(existing_sobj) {
            pthread_mutex_unlock(&existing_sobj->lock);
            DBG_LOG(WARNING, MOD_SSP, "[SesID: %s] already assigned and in use (collision). Cannot accept a new ID for existing connection", sf_session_id);
            rv = -NKN_SSP_SF_SESSION_ID_EXISTS;
	    goto exit;
        }
        else { // !existing_sobj
	    strncpy( con->session_id, sf_session_id, strlen(sf_session_id) );
	    session_obj = sess_obj_alloc();
	    strncpy ( session_obj->session_id, con->session_id, strlen(con->session_id) );
	    pthread_mutex_init(&session_obj->lock, NULL);
	}
	existing_sobj = NULL;
    }
    else if ( strncmp (con->session_id, sf_session_id, strlen(sf_session_id)) != 0 ){
	DBG_LOG(WARNING, MOD_SSP, "[SesID: %s] exists and already assigned to this connection. Cannot accept a new ID for existing connection", sf_session_id);
	rv = -NKN_SSP_SF_SESSION_ID_EXISTS;
	goto exit;
    }

    // First entry from client request
    if (contentLen == 0 && dataBuf == NULL) { // Internal call to fetch meta file

	// Remap URI to fetch meta data file
	existing_sobj = session_obj;
	session_obj->vidname_end = getVidNameOffset((const char*)(uriData), remapLen);

	if (session_obj->vidname_end == 0){
	    DBG_LOG(WARNING, MOD_SSP, "[SesID: %s] Requested file %s for smoothflow does not follow specified naming convention -> name_p##.ext \n Use sf=0 for such non smoothflow files. Closing connection ", sf_session_id, uriData);
	    rv = -NKN_SSP_SF_INCORRECT_NAMING_CONVENTION;
	    goto UnlockObject;
	}

	remapBuf = (char *)alloca(INTERNAL_META_REQ_LEN);
	memset(remapBuf, 0, INTERNAL_META_REQ_LEN);
	strncpy(remapBuf, uriData, session_obj->vidname_end);
	snprintf(remapBuf + session_obj->vidname_end, INTERNAL_META_REQ_LEN - session_obj->vidname_end, ".dat?%s=6", ssp_cfg->sf_signals.state_uri_query);

	nkn_ssp_sess_add(session_obj);

	add_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI, remapBuf, strlen(remapBuf)); // Meta data uri

	// Overwriting the SEEK_URI so that we can go to http origin. Make sure to clear it later
	add_known_header(&phttp->hdr, MIME_HDR_X_NKN_SEEK_URI, remapBuf, strlen(remapBuf));

	DBG_LOG(MSG, MOD_SSP, "[SesID: %s] Request to fetch internal meta file %s issued", con->session_id, remapBuf);
	if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
	    DBG_TRACE("[SesID: %s] Request to fetch internal meta file %s issued", con->session_id, remapBuf);
	}

	rv = SSP_SEND_DATA_BACK;
	goto exit;
    }
    else if ( contentLen == OM_STAT_SIG_TOT_CONTENT_LEN ) { // Indicates meta file was not found

	delete_known_header(&phttp->hdr, MIME_HDR_X_NKN_SEEK_URI); // Clear the origin SEEK URI overload

	existing_sobj = nkn_ssp_sess_get(sf_session_id);                // returns with sobj locked

	if(existing_sobj) {
	    // Meta data file is not available, revert to non smoothflow option
	    existing_sobj->sflow_flag = FALSE;

	    // Remap to original URI and serve regular non-smoothflow file
	    add_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI, uriData, remapLen);

	    // retrieves the correct start profile, the query paramater precedes the video name
	    startProfile = getStartProfile(con, uriData, uriLen, ssp_cfg, sf);

	    //////////////////////////////////////////
	    // INITIATE CACHE MISS TRIGGER INGEST
	    // Add trigger to VPE Queue destination

	    // Modify meta file name to default publisher naming convention
	    existing_sobj->vidname_end = getVidNameOffset((const char*)(uriData), remapLen);
            remapBuf = (char *)alloca(PUB_META_REQ_LEN);
            memset(remapBuf, 0, PUB_META_REQ_LEN);
            strncpy(remapBuf, uriData, existing_sobj->vidname_end);
	    strcat(remapBuf, "_meta.dat");
	    smoothflowIngestController(con, sf_session_id, remapBuf, strlen(remapBuf), hostData, hostLen, ssp_cfg->sf_signals.state_uri_query, strlen(ssp_cfg->sf_signals.state_uri_query));
	    /////////////////////////////////////////

	    // Auto detection of AFR is enhancement issue in 1.0.2
	    // Till then it will be best-effort and we will serve at con_max_bandwidth rate if specified
	    if (ssp_cfg->con_max_bandwidth > 0){
		
		if ( ssp_cfg->con_max_bandwidth <= (uint64_t) MIN(con->max_client_bw, (unsigned)con->pns->max_allowed_bw) ) {
                    con_set_afr(con, ssp_cfg->con_max_bandwidth);
                    DBG_LOG(MSG, MOD_SSP, "[SesID: %s] AFR set to con_max_bandwidth for profile %d at %ld Bytes/sec", con->session_id, startProfile, ssp_cfg->con_max_bandwidth);
                }
                else { // AFR cannot be honoured
		    rv = -NKN_SSP_AFR_INSUFF;
		    DBG_LOG(WARNING, MOD_SSP, "[SesID: %s] Smoothflow AFR request failed: %ld. Please check if network connection max bw is limiting AFR", con->session_id, ssp_cfg->con_max_bandwidth);
		    goto UnlockObject;//fixed bug 2040 was jumping to 'exit' label
                }
	    }
	    else{
		// Do nothing, later we will auto-detect and use the bit-rate as AFR
		// Comment - since we do not know the bandwidth of the current profile (dat file unavailable to get the bitrate map
		// we cannot ascertain if there is enough bandwidth or not to reject the connection.
		DBG_LOG(MSG, MOD_SSP, "[SesID: %s] AFR will be set to best effort on system availability", con->session_id);
	    }

	    pthread_mutex_unlock(&existing_sobj->lock);

	    TSTR(uriData, remapLen, tmp);
	    DBG_LOG(WARNING, MOD_SSP, "[SesID: %s] Smoothflow assets/metadata missing. Try to deliver reqular file %s", con->session_id, tmp);
	    if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
		DBG_TRACE("[SesID: %s] Smoothflow assets/metadata missing. Try to deliver reqular file %s", con->session_id, tmp);
	    }

	    rv = SSP_SEND_DATA_OTW;
	}
	else { // !existing_sobj
	    DBG_LOG(WARNING, MOD_SSP, "[SesID: %s] not found. (State 1 Media Data Connection closed)", sf_session_id);
	    rv = -NKN_SSP_SF_SESSION_ID_NOT_FOUND;
	}

	goto exit;
    }
    else if ( contentLen > 0 && contentLen == bufLen ) { // Indicates meta file was found

	delete_known_header(&phttp->hdr, MIME_HDR_X_NKN_SEEK_URI); // Clear the origin SEEK URI overload

	existing_sobj = nkn_ssp_sess_get(sf_session_id);                // returns with sobj locked

	if(existing_sobj) { // Meta data file is present
	    mdr = init_meta_reader((void*)dataBuf); 	    // Parse meta data and populate session object
	    if(!mdr){
		rv = -NKN_SSP_SF_METADATA_PARSING_ERR;
		DBG_LOG(WARNING, MOD_SSP, "[SesID: %s] Smoothflow meta data parsing failed (Error: %d)", con->session_id, rv);
		goto UnlockObject;//fixed bug 2040 type issues where the code was jumping to 'exit' label instead of UnlockObject label
	    }
	    sf = (sf_meta_data*)get_feature(mdr, FEATURE_SMOOTHFLOW);
	    if(!sf){
		rv = -NKN_SSP_SF_METADATA_PARSING_ERR;
		DBG_LOG(WARNING, MOD_SSP, "[SesID: %s] Smoothflow meta data parsing failed (Error: %d)", con->session_id, rv);
		goto UnlockObject;
	    }


	    //set the start profile if there is a specifc request from the client
	    queryData = NULL;
	    queryLen=0;
	    get_query_arg_by_name(&phttp->hdr, ssp_cfg->sf_signals.profile_uri_query, strlen(ssp_cfg->sf_signals.profile_uri_query), &queryData, &queryLen);
	    if (queryData){
		startProfile = atoi(queryData);
		if(startProfile == 0 || startProfile > sf->n_profiles){
		    startProfile = DEFAULT_START_PROFILE;
		}
	    }

	    existing_sobj->sflow_data = sf;

	    startChunk = getStartChunk(con, mdr, sf, ssp_cfg);
	    startProfile = getStartProfile(con, uriData, uriLen, ssp_cfg, sf);

	    if(startChunk < 0) {
		rv = startChunk;
		goto UnlockObject;
	    }

	    if(startProfile == 0 || startProfile > sf->n_profiles){
		startProfile = DEFAULT_START_PROFILE;
	    }

#if 0
	    ssp = &con->ssp;
	    //copy relevant meta data into ssp params for other downstream components
	    if(sf->codec_id & VCODEC_ID_AVC){

		//Read SPS/PPS information from the feature table
		avcc = (avcc_config*)malloc(sizeof(avcc_config));
		p_avcc = (char*)get_feature(mdr, FEATURE_AVCC_PACKET);
		avcc->avcc_data_size = *((uint32_t*)(p_avcc));
		p_avcc+=sizeof(avcc->avcc_data_size);
		avcc->p_avcc_data = p_avcc;

		ssp->private_data = NULL;
		ssp->private_data = (void*)(avcc);
		ssp->ssp_codec_id = sf->codec_id;
		ssp->ssp_container_id = sf->container_id;
	    } else { //all other codecs have the same logic for seek.

                ssp->private_data = NULL;
                ssp->ssp_codec_id = sf->codec_id;
                ssp->ssp_codec_id = sf->container_id;
	    }


            //check for scrub request, set the start chunk number appropriately. Add the FLV headers during updateSSP. Use the
            //ssp_scrub_req variable in the con->ssp structure to indicate this in updateSSp
            queryData = NULL;
            queryLen=0;
	    if (ssp_cfg->seek.enable && ssp_cfg->seek.uri_query) {
		get_query_arg_by_name(&phttp->hdr, ssp_cfg->seek.uri_query, strlen(ssp_cfg->seek.uri_query), &queryData, &queryLen);
		if (queryData){
		    seekOffset = atof(queryData);
		    startChunk = floor(seekOffset/(sf->chunk_duration/1000));
		    if(startChunk > 0 && startChunk <= (int32_t)(sf->num_chunks)){
			ssp->ssp_scrub_req = 1;
		    } else if(startChunk > (int32_t)(sf->num_chunks)) {
			rv = -NKN_SSP_BAD_QUERY_OFFSET;
			DBG_LOG(WARNING, MOD_SSP, "SesID: %s Seek Out of Bounds (Error %d)", con->session_id, rv);
			goto UnlockObject;
		    } else if(startChunk == 0){
			startChunk = 1;
		    } else if(startChunk < 0) {
			rv = -NKN_SSP_BAD_QUERY_OFFSET;
			DBG_LOG(WARNING, MOD_SSP, "SesID: %s Negative Seek Value (Error %d)", con->session_id, rv);
			goto UnlockObject;
		    }
		}
	    }
#endif
	    destory_meta_data_reader(mdr);
	    initSessionObj(existing_sobj, startProfile, (uint32_t)(startChunk), ssp_cfg->con_max_bandwidth);

	    // Activate the multipart response request
	    http_set_multipart_response(phttp, existing_sobj->sflow_data->pseudo_content_length);

	    // Remap to the new chunked URI and auto loop backs will happen due to the multipart flag being set
	    existing_sobj->vidname_end = getVidNameOffset((const char*)(uriData), remapLen);
	    remapBuf = (char *)alloca(CHUNK_REQ_LEN);
	    memset(remapBuf, 0, CHUNK_REQ_LEN);
	    strncpy(remapBuf, uriData, existing_sobj->vidname_end);
	    snprintf(remapBuf + existing_sobj->vidname_end, CHUNK_REQ_LEN - existing_sobj->vidname_end - 1, "_p%02d_c%010d.flv?%s=6", existing_sobj->curr_profile, existing_sobj->curr_chunk,  ssp_cfg->sf_signals.state_uri_query);

	    // Check for AFR and enforce it
	    sf_afr = existing_sobj->sflow_data->afr_tbl[existing_sobj->curr_profile-1];

	    if (sf_afr < (int64_t) MIN(con->max_client_bw, (unsigned)con->pns->max_allowed_bw) ) {
		con_set_afr(con, sf_afr);
		DBG_LOG(MSG, MOD_SSP, "[SesID: %s] Smoothflow AFR Set: %ld Bytes/sec", con->session_id, con->min_afr);
		if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
		    DBG_TRACE("[SesID: %s] Smoothflow AFR Set: %ld Bytes/sec", con->session_id, con->min_afr);
		}
	    }
	    else {
		// Reject connection, since requested rate > available bandwidth
		DBG_LOG(WARNING, MOD_SSP, "[SesID: %s] Smoothflow AFR request failed: %d. Please check if network connection max bw is limiting AFR", con->session_id, sf_afr);
		if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
                    DBG_TRACE("[SesID: %s] Smoothflow AFR request failed: %d. Please check if network connection max bw is limiting AFR", con->session_id, sf_afr);
                }
		rv = -NKN_SSP_AFR_INSUFF;
		goto UnlockObject;
	    }

	    add_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI, remapBuf, strlen(remapBuf));
	    add_known_header(&phttp->hdr, MIME_HDR_X_NKN_SEEK_URI, remapBuf, strlen(remapBuf)); //Origin overload

	    rv = SSP_SEND_DATA_OTW;
	    goto UnlockObject;
	}
	else { // !existing_sobj
	    DBG_LOG(WARNING, MOD_SSP, "[SesID: %s] not found. (State 1 Media Data Connection closed)", sf_session_id);
	    rv = -NKN_SSP_SF_SESSION_ID_NOT_FOUND;
	    goto exit;
	}

    UnlockObject:
	pthread_mutex_unlock(&existing_sobj->lock);
	goto exit;
    }
    else if ( contentLen > 0 && bufLen < contentLen ) { // Indicates chunked response. Not supported
	DBG_LOG(WARNING, MOD_SSP, "Chunked meta file read not supported (Since meta files are assumed to be small in size). Error: %d", NKN_SSP_CHUNK_METAFILE_NOT_SUPP);
	rv = -NKN_SSP_CHUNK_METAFILE_NOT_SUPP;
	goto exit;
    }

 exit:
    return rv;
}

/*
 * State 2: Meta Data Connection
 * http://www.abc.com/sf/foo.xml?sid=01&sf=2
 *
 */
int
smoothflowMetaController(con_t *con, const char *uriData, int uriLen, char *sf_session_id, int remapLen)
{
    UNUSED_ARGUMENT(uriLen);
    nkn_ssp_session_t *existing_sobj;
    http_cb_t *phttp = &con->http;
    char *remapBuf;
    int rv=0;
    const namespace_config_t *cfg;
    const ssp_config_t *ssp_cfg;

    cfg = namespace_to_config(phttp->ns_token);
    if(cfg == NULL){
	rv = 10;
	DBG_LOG(MSG, MOD_SSP, "namespace_to_config() ret=NULL; rv=%d", rv);
	return rv;
    }
    ssp_cfg = cfg->ssp_config;


    // Query the hash table to find a matching entry
    existing_sobj = nkn_ssp_sess_get(sf_session_id);		// returns with sobj locked

    if(existing_sobj) {

	if (existing_sobj->sflow_flag) { // Smoothflow is ON

	    remapBuf = (char *)alloca(CLIENT_META_REQ_LEN);
            memset(remapBuf, 0, CLIENT_META_REQ_LEN);
            strncpy(remapBuf, uriData, remapLen);
	    snprintf(remapBuf + existing_sobj->vidname_end, INTERNAL_META_REQ_LEN - existing_sobj->vidname_end, ".xml?%s=6", ssp_cfg->sf_signals.state_uri_query);

	    //Remap the URI to serve client xml meta data file
	    add_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI, uriData, remapLen);
	    add_known_header(&phttp->hdr, MIME_HDR_X_NKN_SEEK_URI, remapBuf, strlen(remapBuf)); // To go to HTTP origin

	    DBG_LOG(WARNING, MOD_SSP, "[SesID: %s] Requesting client meta data file", sf_session_id);
	    release_namespace_token_t(phttp->ns_token);
	    rv = SSP_SEND_DATA_OTW;
	}
	else { // Smoothflow is OFF
	    DBG_LOG(WARNING, MOD_SSP, "[SesID: %s] Smoothflow is not supported for this asset currently. In State 2 and client meta data file cannot be delivered", con->session_id);
	    release_namespace_token_t(phttp->ns_token);
	    rv = -NKN_SSP_SF_SMOOTHFLOW_NOT_SUPP;
	}

	pthread_mutex_unlock(&existing_sobj->lock);
    }
    else { // !existing_sobj
	DBG_LOG(WARNING, MOD_SSP, "[SesID: %s] not found. (State 2 Meta Data Connection closed)", sf_session_id);
	release_namespace_token_t(phttp->ns_token);
	rv = -NKN_SSP_SF_SESSION_ID_NOT_FOUND;
    }

    return rv;
}


/*
 * State 3: Control Signal Data Connection
 * http://www.abc.com/sf/foo?sid=01&sf=3&pf=2
 *
 */
int
smoothflowSignalController(con_t *con, char *sf_session_id, const ssp_config_t *ssp_cfg)
{
    nkn_ssp_session_t *existing_sobj;
    http_cb_t *phttp = &con->http;
    const char *queryData;
    int queryLen=0;
    int rv=0;
    int sf_prof_req=0;

    // Query the hash table to find a matching entry
    existing_sobj = nkn_ssp_sess_get(sf_session_id);                // returns with sobj locked

    if(existing_sobj) {

	if (existing_sobj->sflow_flag) { // Smoothflow is ON
	    // Query for the adaptation request and update the session object
	    queryData = NULL;
	    get_query_arg_by_name(&phttp->hdr, ssp_cfg->sf_signals.profile_uri_query, strlen( ssp_cfg->sf_signals.profile_uri_query), &queryData, &queryLen);
	    if (queryData){
		sf_prof_req = atoi(queryData);

		if (sf_prof_req > 0 && sf_prof_req <= (signed)existing_sobj->sflow_data->n_profiles) {
		    existing_sobj->next_profile = sf_prof_req;
		    DBG_LOG(MSG, MOD_SSP, "[SesID: %s] Request to switch to profile %02d acknowledged", sf_session_id, sf_prof_req);
		    phttp->obj_create = nkn_cur_ts;
		    phttp->attr = NULL;
		    SET_HTTP_FLAG(phttp, HRF_SSP_SF_RESPONSE);
		    rv = SSP_200_OK; //return 200 OK and keep alive conn if possible
		}
		else {
		    DBG_LOG(WARNING, MOD_SSP, "[SesID: %s] State 3, Control signal profile change request to %02d out of bounds. Disregard request and continue normal operation. Max profile supported is %02d", sf_session_id, sf_prof_req, (signed)existing_sobj->sflow_data->n_profiles);
		    rv = -NKN_SSP_SF_PROFILE_CHANGE_OUT_OF_BOUND;
		}
	    }
	    else {
		DBG_LOG(WARNING, MOD_SSP, "[SesID: %s] State 3, Control signal is missing adaptation query parameter. Disregard request and continue normal operation", sf_session_id);
		rv = -NKN_SSP_SF_ADAPTATION_QUERY_MISSING;
	    }
	}
	else { // Smoothflow is OFF
	    DBG_LOG(WARNING, MOD_SSP, "[SesID: %s] Smoothflow is not supported for this asset currently. Rejecting control request in State 3", sf_session_id);
            rv = -NKN_SSP_SF_SMOOTHFLOW_NOT_SUPP;
	}

	pthread_mutex_unlock(&existing_sobj->lock);
    }
    else { // !existing_sobj
	DBG_LOG(WARNING, MOD_SSP, "[SesID: %s] not found. (State 3 Control Signal Data Connection closed)", sf_session_id);
        rv = -NKN_SSP_SF_SESSION_ID_NOT_FOUND;
    }

    return rv;
}



/*
 * State 4: Trigger Signal to initiate smoothflow preprocessing
 * http://www.abc.com/sf/foo_meta.dat?sf=4
 *
 */
int smoothflowIngestController(con_t *con, char *sf_session_id, const char *uriData, int uriLen, const char *hostData, int hostLen, const char *stateQuery, int stateQueryLen)
{
    UNUSED_ARGUMENT(con);
    UNUSED_ARGUMENT(sf_session_id);

    int rv;
    int remapLen=0;

    // Obtain the relative URL length stripped of all query params
    if ( (remapLen = findStr(uriData, "?", uriLen)) == -1 ) {
        remapLen = uriLen;
    }

    //////////////////////////////////////////
    // INITIATE TRIGGER BASED INGEST
    // Add trigger to VPE Queue destination
    rv = submit_vpemgr_sf_request(hostData, hostLen, uriData, remapLen, stateQuery, stateQueryLen);

    if (rv){
	rv = -NKN_SSP_SF_PUBLISHING_REQ_FAILED;
	DBG_LOG(WARNING, MOD_SSP, "Request to trigger smoothflow processing failed. rv = %d", rv);
    }
    else {
	rv = 3; // 3 is for 200 OK response to client
    }

    return rv;
}

/*
 * State 5: Local Loop Fetch for Preprocessing
 * http://www.abc.com/sf/foo_meta.dat?sf=5
 *
 */
int smoothflowLocalHostFetch(con_t *con, char *sf_session_id, const char *uriData, int uriLen)
{
    UNUSED_ARGUMENT(sf_session_id);

    int rv=0;
    int remapLen=0;
    http_cb_t *phttp = &con->http;

    delete_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI);
    delete_known_header(&phttp->hdr, MIME_HDR_X_NKN_SEEK_URI);
    //delete_known_header(&phttp->hdr, MIME_HDR_X_NKN_URI);

    // Obtain the relative URL length stripped of all query params
    if ( (remapLen = findStr(uriData, "?", uriLen)) == -1 ) {
        remapLen = uriLen;
    }

    //Remap to URI to serve video
    //add_known_header(&phttp->hdr, MIME_HDR_X_NKN_URI, uriData, remapLen);
    add_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI, uriData, remapLen);

    // Overwriting the SEEK_URI so that we can go to http origin. Make sure to clear it later
    add_known_header(&phttp->hdr, MIME_HDR_X_NKN_SEEK_URI, uriData, remapLen);


    return rv;
}



/*
 * smoothflowLoopbackRequest
 * Return values:
 * > 0 - Process another request
 * <= 0 - Close connection
 */
int
smoothflowLoopbackRequest(con_t *con)
{
    const char *uriData;
    int uriLen=0, uriCnt=0;
    unsigned int uriAttrs=0;
    int rv = 0;
    char *remapBuf;
    int remapLen=0;
    uint32_t sf_afr=0, sf_afr_curr=0;
    http_cb_t *phttp = &con->http;
    nkn_ssp_session_t *existing_sobj;
    const namespace_config_t *cfg;
    const ssp_config_t *ssp_cfg;
    int32_t dbg_var;

    cfg = namespace_to_config(phttp->ns_token);
    if(cfg == NULL){
        rv = 10;
        DBG_LOG(MSG, MOD_SSP, "namespace_to_config() ret=NULL; rv=%d", rv);
        return rv;
    }

    ssp_cfg = cfg->ssp_config;

    // Get URI
    if (get_known_header(&phttp->hdr, MIME_HDR_X_NKN_DECODED_URI, &uriData, &uriLen, &uriAttrs, &uriCnt)) {
	if (get_known_header(&phttp->hdr , MIME_HDR_X_NKN_URI, &uriData, &uriLen, &uriAttrs, &uriCnt)) {
	    return -NKN_SSP_BAD_URL;
	}
    }

    // Obtain the relative URL length stripped of all query params
    if ( (remapLen = findStr(uriData, "?", uriLen)) == -1 ) {
        rv = -NKN_SSP_INSUFF_QUERY_PARAM;
        DBG_LOG(WARNING, MOD_SSP, "URL does not contain any query params. Expects at least the session id and smoothflow state params (Error: %d)", rv );
        return rv;
    }

    // Fetch session object
    existing_sobj = nkn_ssp_sess_get(con->session_id);                // returns with sobj locked

    if(existing_sobj) {
	delete_known_header(&phttp->hdr, MIME_HDR_X_NKN_SEEK_URI); // Clear the origin SEEK URI overload

	if (existing_sobj->sflow_flag) { // Smoothflow is ON

	    //No more data to serve, inform HTTP to close connection
	    if(existing_sobj->curr_chunk >= existing_sobj->sflow_data->num_chunks){
		rv = SSP_SF_MULTIPART_CLOSE_CONN;
		DBG_LOG(MSG, MOD_SSP, "[SesID: %s] Done sending all data. Calling close connection", con->session_id);
		goto UnlockObject;
	    }

	    ++existing_sobj->curr_chunk;

	    // Check if the requested profile can be delivered dependent on availability and network AFR
	    // No profile change (No requirement to check for availability of interface b/w. Just remap URI and serve)

	    // Upgrade request
	    if (existing_sobj->next_profile > existing_sobj->curr_profile) {
		// Check if MFD can support this upgrade request and set higher AFR only if possible
		sf_afr = existing_sobj->sflow_data->afr_tbl[existing_sobj->next_profile-1];
		sf_afr_curr = existing_sobj->sflow_data->afr_tbl[existing_sobj->curr_profile-1];

		// Better to check for delta increase
		if ( (sf_afr-sf_afr_curr) < (int64_t) MIN(con->max_client_bw, (unsigned)con->pns->max_allowed_bw) ) {
		    con_set_afr(con, sf_afr);
		    existing_sobj->curr_profile = existing_sobj->next_profile;
		    DBG_LOG(MSG, MOD_SSP, "[SesID: %s] Smoothflow profile upgraded. AFR Set: %ld Bytes/sec", con->session_id, con->min_afr);
		}
		else { // Upgrade AFR cannot be served. Continue with curr profile
		    existing_sobj->next_profile = existing_sobj->curr_profile;
		    DBG_LOG(WARNING, MOD_SSP, "[SesID: %s] Smoothflow profile upgrade request denied ***", con->session_id);
		}
	    }
	    else if (existing_sobj->next_profile < existing_sobj->curr_profile) { // Downgrade request
		// MFD should be able to honour the downgrade request. Just remap URI and reduce AFR
		sf_afr = existing_sobj->sflow_data->afr_tbl[existing_sobj->next_profile-1];
		con_set_afr(con, sf_afr);
		existing_sobj->curr_profile = existing_sobj->next_profile;
		DBG_LOG(MSG, MOD_SSP, "[SesID: %s] Smoothflow profile downgraded. AFR reduced to %ld Bytes/sec", con->session_id, con->min_afr);
	    }

	    remapBuf = (char *)alloca(CHUNK_REQ_LEN);
	    memset(remapBuf, 0, CHUNK_REQ_LEN);
	    strncpy(remapBuf, uriData, existing_sobj->vidname_end);
	    dbg_var = existing_sobj->vidname_end;
	    //	    printf("dbg: end of vid str: %d, CHUNK_REQ_LEN: %ld", existing_sobj->vidname_end, CHUNK_REQ_LEN);
            snprintf(remapBuf + existing_sobj->vidname_end, CHUNK_REQ_LEN - existing_sobj->vidname_end - 1, "_p%02d_c%010d.flv?%s=6", existing_sobj->curr_profile, existing_sobj->curr_chunk,  ssp_cfg->sf_signals.state_uri_query);

	    delete_known_header(&phttp->hdr, MIME_HDR_X_NKN_SEEK_URI);
	    delete_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI);

	    add_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI, remapBuf, strlen(remapBuf));
	    add_known_header(&phttp->hdr, MIME_HDR_X_NKN_SEEK_URI, remapBuf, strlen(remapBuf)); // Overload to HTTP origin
	    CLEAR_HTTP_FLAG(&con->http, HRF_BYTE_RANGE_HM);

	    DBG_LOG(MSG, MOD_SSP, "[SesID: %s] Switching to chunk %s", con->session_id, remapBuf);
	    rv = SSP_SF_MULTIPART_LOOPBACK;
	}
	else { // SF OFF, should never happen
	    DBG_LOG(WARNING, MOD_SSP, "[SesID: %s] Smoothflow is not supported for this asset currently. In Loopback State", con->session_id);
            rv = -NKN_SSP_SF_SMOOTHFLOW_NOT_SUPP;
	}

    UnlockObject:
	pthread_mutex_unlock(&existing_sobj->lock);
	goto exit;

    }// !existing_sobj
    else {
	DBG_LOG(WARNING, MOD_SSP, "[SesID: %s] not found. (Session ID lost in loop back state. Connection closed)", con->session_id);
        rv = -NKN_SSP_SF_SESSION_ID_NOT_FOUND;
    }

 exit:
    release_namespace_token_t(phttp->ns_token);
    return rv;
}


uint32_t
getVidNameOffset(const char *uriData, int32_t remapLen)
{
    int rv=0;
    int i;
    char searchStr[]={'_','p'};

    i = remapLen;

    if(!remapLen || !uriData){
	return rv;
    }

    // TBD: Add a reg exp check for _pNN
    while((memcmp(&uriData[--i], searchStr, 2)) && i !=0){
    }

    i=(i!=0) * (i);
    return i;
}

int
initSessionObj(nkn_ssp_session_t *existing_sobj, uint32_t startProfile, uint32_t startChunk, uint32_t con_max_bandwidth)
{
    int rv=0, i;

    // Enforcing con_max_bandwidth to restrict higher smoothflow profiles as part of CLI config
    if (con_max_bandwidth > 0){

	for (i = existing_sobj->sflow_data->n_profiles-1; i >= 0; i--){

	    if (existing_sobj->sflow_data->profile_rate[i] > con_max_bandwidth)
		continue;
	    else {
		existing_sobj->sflow_data->n_profiles = i+1;
		existing_sobj->sflow_data->afr_tbl[i] = con_max_bandwidth;

		if ((signed)startProfile > i)
		    startProfile = i;

		break;
	    }

	} //for (i =

    } //if (con_max


    existing_sobj->curr_profile = startProfile; // Change later to use value from URI TBD:d
    existing_sobj->curr_chunk = startChunk;

    existing_sobj->next_profile = startProfile; // Change later to use value from URI TBD:d
    existing_sobj->next_chunk = startChunk+1;

    existing_sobj->sflow_flag = TRUE;

    return rv;
}

int
getStartChunk(con_t *con, meta_data_reader *mdr, sf_meta_data *sf,const ssp_config_t *ssp_cfg)
{

    int32_t startChunk = 1;
    const char *queryData;
    int32_t queryLen = 0;
    float seekOffset;
    nkn_ssp_params_t *ssp = NULL;
    avcc_config *avcc;
    char *p_avcc = NULL;
    http_cb_t *phttp = &con->http;

    ssp = &con->ssp;

    //copy relevant meta data into ssp params for other downstream components
    if(sf->codec_id & VCODEC_ID_AVC){

	//Read SPS/PPS information from the feature table
	avcc = (avcc_config*)nkn_malloc_type(sizeof(avcc_config), mod_ssp_sf_avcc_cfg);
	p_avcc = (char*)get_feature(mdr, FEATURE_AVCC_PACKET);
	avcc->avcc_data_size = *((uint32_t*)(p_avcc));
	p_avcc+=sizeof(avcc->avcc_data_size);
	avcc->p_avcc_data = p_avcc;

	ssp->private_data = NULL;
	ssp->private_data = (void*)(avcc);
	ssp->ssp_codec_id = sf->codec_id;
	ssp->ssp_container_id = sf->container_id;
    } else { //all other codecs have the same logic for seek.

	ssp->private_data = NULL;
	ssp->ssp_codec_id = sf->codec_id;
	ssp->ssp_codec_id = sf->container_id;
    }

    //check for scrub request, set the start chunk number appropriately. Add the FLV headers during updateSSP. Use the
    //ssp_scrub_req variable in the con->ssp structure to indicate this in updateSSp
    queryData = NULL;
    queryLen=0;
    if (ssp_cfg->seek.enable && ssp_cfg->seek.uri_query) {
	get_query_arg_by_name(&phttp->hdr, ssp_cfg->seek.uri_query, strlen(ssp_cfg->seek.uri_query), &queryData, &queryLen);
	if (queryData){
	    seekOffset = atof(queryData);
	    startChunk = floor((float)(seekOffset)/((float)(sf->keyframe_period)/1000));
	    if(startChunk > 0 && startChunk <= (int32_t)(sf->num_chunks)){
		ssp->ssp_scrub_req = 1;
	    } else if(startChunk > (int32_t)(sf->num_chunks)) {
		startChunk = -NKN_SSP_BAD_QUERY_OFFSET;
		DBG_LOG(WARNING, MOD_SSP, "SesID: %s Seek Out of Bounds (Error %d)", con->session_id, startChunk);
		goto UnlockObject;
	    } else if(startChunk == 0){
		startChunk = 1;
	    } else if(startChunk < 0) {
		startChunk = -NKN_SSP_BAD_QUERY_OFFSET;
		DBG_LOG(WARNING, MOD_SSP, "SesID: %s Negative Seek Value (Error %d)", con->session_id, startChunk);
		goto UnlockObject;
	    }
	} else {
	    startChunk = 1;
	}
    }

 UnlockObject:
    return startChunk;

}

static inline int
getStartProfile(con_t *con, const char *uriData, int uriLen, const ssp_config_t *ssp_cfg,  sf_meta_data *sf)
{

    const char *queryData;
    int32_t queryLen = 0;
    http_cb_t *phttp = &con->http;
    uint32_t startProfile;

    // if sf_meta_data is not available glean the pf from the URI name
    // This fixes bug 2073 where the start profile which is gleaned from the Query Parameters will
    // checked for bounds. The variable that has the bounds will be uninitialized when this function
    // is called for the first time with an empty cache devoid of the internal meta data file
    if(!sf) {
	startProfile = getStartProfileFromURI(uriData, uriLen);
	return startProfile;
    }

    //set the start profile if there is a specifc request from the client
    queryData = NULL;
    queryLen=0;
    get_query_arg_by_name(&phttp->hdr, ssp_cfg->sf_signals.profile_uri_query, strlen(ssp_cfg->sf_signals.profile_uri_query), &queryData, &queryLen);
    if (queryData){
	startProfile = atoi(queryData);
	if(startProfile == 0 || startProfile > sf->n_profiles){
	    startProfile = DEFAULT_START_PROFILE;
	}
    } else {
	startProfile = getStartProfileFromURI(uriData, uriLen);
    }

    return (int32_t)startProfile;
}

static inline int
getStartProfileFromURI(const char *uriData, int uriLen)
{
    int32_t off;
    char profile_id[3];

    off = getVidNameOffset(uriData, uriLen);
    memcpy(&profile_id, &uriData[off+2], 2);
    profile_id[2] = '\0';

    return atoi(profile_id);

}
