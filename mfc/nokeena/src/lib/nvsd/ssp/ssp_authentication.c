/*
 *******************************************************************************
 * ssp_authetication.c -- SSP Authentication Mechanism Definitions
 *******************************************************************************
 */

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <openssl/md5.h>
#include "ssp_authentication.h"
#include "http_header.h"
#include "nkn_errno.h"
#include "nkn_cfg_params.h"
#include "nkn_debug.h"
#include "nkn_common_config.h"
#include "ssp_python_plugin.h"

#define TSTR(_p, _l, _r)  char *(_r); ( 1 ? ( (_r) = alloca((_l)+1), memcpy((_r), (_p), (_l)), (_r)[(_l)] = '\0' ) : 0 )

/*
 * verifyMD5Hash
 * Return values
 * 0: Successful hash match found
 * non zero: Error, hash does not match
 */
int
verifyMD5Hash(const char *hostData, const char *uriData, int hostLen, mime_header_t *hd, const hash_verify_t cfg_hash)
{
    unsigned char calHash[MD5_DIGEST_LENGTH]; // MD5_DIGEST_LENGTH is 16 bytes
    char calHashHex[MD5_DIGEST_LENGTH*2+1];

    const char *queryHashTmp=NULL;
    const char *query_expiryTmp=NULL;
    char *matchStr;
    char *tmpStr, *p_uribase;
    int matchStrlen=0;
    int i, qlen=0, len=0;
    int rv;
    int hashLen;
    int remapLen=0, offset=0;
    time_t timeutc_current=0, timeutc_query=0;

    // Perform MD5 Hash Verification
    MD5_CTX ctx;

    hashLen = MD5_DIGEST_LENGTH*2;

    // Get query hash
    if (cfg_hash.uri_query == NULL) {
	DBG_LOG(ERROR, MOD_SSP,
		"CLI Seek Config for hash query-string-parm is not set (Error: %d)",
		NKN_SSP_MISSING_QUERY_PARAM);
	return -NKN_SSP_HASHCHECK_FAIL;
    }
    get_query_arg_by_name(hd, cfg_hash.uri_query, strlen(cfg_hash.uri_query), &queryHashTmp, &qlen);

    TSTR(queryHashTmp, qlen, queryHash);
    if ( strncmp(queryHash, "", 1) == 0 ) {
	queryHash = NULL;
    }

    if (!queryHash) {
        DBG_LOG(WARNING, MOD_SSP,
		"Query Hash param not provided for verification in URL (Error: %d)",
		NKN_SSP_MISSING_QUERY_PARAM);
        return -NKN_SSP_HASHCHECK_FAIL;
    }

    MD5_Init(&ctx);     // Initiate the md5 context

    // Perform update piecewise
    if (cfg_hash.secret_position && strcmp(cfg_hash.secret_position, "prefix") == 0) {

	if (cfg_hash.secret_key) {
	    MD5_Update(&ctx, cfg_hash.secret_key, strlen(cfg_hash.secret_key));  // md5 secret key
	}
	if(cfg_hash.url_type && strcmp(cfg_hash.url_type, "absolute-url") == 0) {
	    MD5_Update(&ctx, "http://", 7);                     // md5 http prefix
	    MD5_Update(&ctx, hostData, hostLen);                // md5 host header
	}

	matchStrlen = strlen(cfg_hash.uri_query)+4;

	matchStr = (char *)alloca(matchStrlen);
	memset(matchStr, 0, matchStrlen);
	strcat(matchStr, "&");
	strcat(matchStr, cfg_hash.uri_query);
	strcat(matchStr, "=");
	strcat(matchStr, "\0");

	len = findStr(uriData, matchStr, strlen(uriData));   // uriData before &query=
	if ( len == -1 ){ // no match, try ?query= instead
	    memset(matchStr, 0, matchStrlen);
	    strcat(matchStr, "?");
	    strcat(matchStr, cfg_hash.uri_query);
	    strcat(matchStr, "=");
	    strcat(matchStr, "\0");
	    len = findStr(uriData, matchStr, strlen(uriData));   // uriData before ?query=
	    if (len == -1){
		DBG_LOG(WARNING, MOD_SSP,
			"Authentication query could not be found in the URI. Close connection: %s",
			matchStr);
		return -NKN_SSP_HASHCHECK_FAIL;
	    }
	}

	if(cfg_hash.url_type &&
	   (strcmp(cfg_hash.url_type, "absolute-url") == 0 ||
	    strcmp(cfg_hash.url_type, "relative-uri") == 0)) {
            MD5_Update(&ctx, uriData, len);     // md5 uri
        } else if (cfg_hash.url_type && strcmp(cfg_hash.url_type, "object-name") == 0 ) {

            if ( (remapLen = findStr(uriData, "?", strlen(uriData))) == -1 ) {
                remapLen = strlen(uriData);
            }
            tmpStr = (char *)alloca(remapLen+1);
            memset(tmpStr, 0, remapLen+1);
	    tmpStr[0] = '\0';
            strncat(tmpStr, uriData, remapLen);
            p_uribase = strrchr(tmpStr, '/');
            if (p_uribase == NULL) {
                DBG_LOG(WARNING, MOD_SSP,
			"Missing / between domain and object-name. \
			Should not happen. Failing hash verify");
                return -NKN_SSP_HASHCHECK_FAIL;
            }
            offset = p_uribase - tmpStr + 1;
            if ( (len-offset) < 0){
                DBG_LOG(WARNING, MOD_SSP, "Hash uri length -ve. Should not happen. Failing hash verify");
                return -NKN_SSP_HASHCHECK_FAIL;
            }
            MD5_Update(&ctx, uriData + offset, len - offset);     // md5 uri
        }
	MD5_Final(calHash, &ctx);           // create Hash
    }
    else if (cfg_hash.secret_position && strcmp(cfg_hash.secret_position, "append") == 0) {

	if (cfg_hash.url_type && strcmp(cfg_hash.url_type, "absolute-url") == 0) {
	    MD5_Update(&ctx, "http://", 7);                     // md5 http prefix
	    MD5_Update(&ctx, hostData, hostLen);                        // md5 host header
	}

        matchStrlen = strlen(cfg_hash.uri_query)+4;

        matchStr = (char *)alloca(matchStrlen);
        memset(matchStr, 0, matchStrlen);
        strcat(matchStr, "&");
        strcat(matchStr, cfg_hash.uri_query);
        strcat(matchStr, "=");
        strcat(matchStr, "\0");

        len = findStr(uriData, matchStr, strlen(uriData));   // uriData before &query=
        if ( len == -1 ){ // no match, try ?query= instead
            memset(matchStr, 0, matchStrlen);
            strcat(matchStr, "?");
            strcat(matchStr, cfg_hash.uri_query);
            strcat(matchStr, "=");
            strcat(matchStr, "\0");
            len = findStr(uriData, matchStr, strlen(uriData));   // uriData before ?query=
            if (len == -1){
                DBG_LOG(WARNING, MOD_SSP,
			"Authentication query could not be found in the URI. Close connection: %s",
			matchStr);
                return -NKN_SSP_HASHCHECK_FAIL;
            }
        }

        if (cfg_hash.url_type &&
	    (strcmp(cfg_hash.url_type, "absolute-url") == 0 ||
	     strcmp(cfg_hash.url_type, "relative-uri") == 0)) {
            MD5_Update(&ctx, uriData, len);     // md5 uri
        } else if (cfg_hash.url_type &&
		   strcmp(cfg_hash.url_type, "object-name") == 0) {
            if ( (remapLen = findStr(uriData, "?", strlen(uriData))) == -1 ) {
                remapLen = strlen(uriData);
            }
            tmpStr = (char *)alloca(remapLen+1);
            memset(tmpStr, 0, remapLen+1);
	    tmpStr[0] = '\0';
            strncat(tmpStr, uriData, remapLen);
            p_uribase = strrchr(tmpStr, '/');
            if (p_uribase == NULL) {
                DBG_LOG(WARNING, MOD_SSP,
			"Missing / between domain and object-name. \
			Should not happen. Failing hash verify");
                return -NKN_SSP_HASHCHECK_FAIL;
            }
            offset = p_uribase - tmpStr + 1;
            if ( (len-offset) <0){
                DBG_LOG(WARNING, MOD_SSP, "Hash uri length -ve. Should not happen. Failing hash verify");
                return -NKN_SSP_HASHCHECK_FAIL;
            }
            MD5_Update(&ctx, uriData + offset, len - offset);     // md5 uri
        }
	if (cfg_hash.secret_key) {
	    MD5_Update(&ctx, cfg_hash.secret_key, strlen(cfg_hash.secret_key));   // md5 secret key
	}
        MD5_Final(calHash, &ctx);           // create Hash
    }

    // Convert to hex to enable matching with query hash
    for(i = 0; i < MD5_DIGEST_LENGTH; i++) {
	snprintf(&calHashHex[i*2], 3, "%02x", calHash[i]);
    }

    //Perform mem comparison of the two hashes
    rv = memcmp(calHashHex, queryHash, hashLen);

    DBG_LOG(MSG, MOD_SSP, "Query    Hash: %s", queryHash);
    DBG_LOG(MSG, MOD_SSP, "Computed Hash: %s", calHashHex);

    if (rv != 0) {
	rv = -NKN_SSP_HASHCHECK_FAIL;
        goto exit;
    }

    // Hash check passed, now validate hash expiry time, if enabled
    if (cfg_hash.expirytime_query != NULL) { // Query CLI enabled, implies expiry validation mandatory
        // Check for presence of expiry query in CLI
        get_query_arg_by_name(hd, cfg_hash.expirytime_query,
			      strlen(cfg_hash.expirytime_query), &query_expiryTmp, &qlen);
	TSTR(query_expiryTmp, qlen, query_expiry);
	if ( strncmp(query_expiry, "", 1) == 0 ) {
            query_expiry = NULL;
        }

        if (!query_expiry) {
            DBG_LOG(WARNING, MOD_SSP,
		    "Query Expiry param missing in URL (Error: %d)", NKN_SSP_MISSING_QUERY_PARAM);
            return -NKN_SSP_HASHCHECK_FAIL;
        }

        timeutc_current = time(NULL); // Current timestamp
        timeutc_query = atoi(query_expiry);

        if (timeutc_query != 0 && timeutc_query < timeutc_current) {
            DBG_LOG(WARNING, MOD_SSP, "Hash expired, rejecting request [Qtime=%ld/Ctime=%ld]",
                    timeutc_query, timeutc_current);
            return -NKN_SSP_HASHEXPIRY_FAILED;
        }
    }

 exit:
    return rv;

}


/*
 * verifyYahooHash
 * Return values
 * 0: Successful hash match found
 * non zero: Error, hash does not match
 */
int
verifyYahooHash(const char *hostData, const char *uriData, int hostLen, mime_header_t *hd, const req_auth_t req_auth)
{
    UNUSED_ARGUMENT(hostData);
    UNUSED_ARGUMENT(uriData);
    UNUSED_ARGUMENT(hostLen);

    //const char secret_key[] = "this_is_A_secret_for_now";
    //const char stream_id_uri_query[] = "streamid";
    //const char auth_id_uri_query[] = "authid";
    //const char ticket_uri_query[] = "ticket";
    const int num_interval_offsets = 3;

    unsigned char calHash[MD5_DIGEST_LENGTH]; // MD5_DIGEST_LENGTH is 16 bytes
    char calHashHex[MD5_DIGEST_LENGTH*2 + 1];
    char buf[11]; // Fixed size of 10 bytes documented in spec

    const char *queryHash=NULL;
    const char *queryData=NULL;
    int queryLen=0;
    int i,j, qlen=0;
    int rv;
    int hashLen=0;
    time_t utc_time=0, current=0;
    int interval;
    int interval_offset[3] = {0, -15, +15};

    // Perform MD5 Hash Verification
    MD5_CTX ctx;

    hashLen = MD5_DIGEST_LENGTH*2;

    // Stream id, shared secret, time interval & match string must be specified to proceed with Authentication
    if (req_auth.stream_id_uri_query == NULL){
	DBG_LOG(WARNING, MOD_SSP, "Required Stream ID query for Authentication unspecified in CLI");
	return -NKN_SSP_CLI_PARAM_UNSPECIFIED;
    }

    if (req_auth.shared_secret == NULL){
        DBG_LOG(WARNING, MOD_SSP, "Required Secret Key for Authentication unspecified in CLI");
        return -NKN_SSP_CLI_PARAM_UNSPECIFIED;
    }

    if (req_auth.match_uri_query == NULL){
	DBG_LOG(WARNING, MOD_SSP, "Required matching URI query for Authentication unspecified in CLI");
	return -NKN_SSP_CLI_PARAM_UNSPECIFIED;
    }

    if (req_auth.time_interval <=0){
	DBG_LOG(WARNING, MOD_SSP, "Time interval value for authentication must be specified and > 0 in CLI");
        return -NKN_SSP_CLI_PARAM_UNSPECIFIED;
    }

    // Get the ticket value provided in the URI for authentication
    if (req_auth.match_uri_query){
	get_query_arg_by_name(hd, req_auth.match_uri_query, strlen(req_auth.match_uri_query), &queryHash, &qlen);
	TSTR(queryHash, qlen, queryHashTmp);
	if ( strncmp(queryHashTmp, "", 1) == 0 ) {
            queryHashTmp = NULL;
        }

	if (!queryHashTmp) {
	    DBG_LOG(WARNING, MOD_SSP, "Hash param to match with not provided for verification in URL. Nothing to compare with. Closing connection");
	    return -NKN_SSP_MISSING_QUERY_PARAM;
	}
    }

    // Field 1: Determine the UTC time in seconds using time interval
    interval = req_auth.time_interval;
    current = time(NULL);
    utc_time = (int)(current/interval) * interval; // rounds downward to nearest expiration interval

    for (j = 0; j < num_interval_offsets; j++) {

	memset(buf, 0, 11);
	snprintf(buf, 11, "%010ld", utc_time + interval_offset[j]);
	DBG_LOG(MSG, MOD_SSP, "UTC is %s", buf);

	// Initiate the md5 context
	MD5_Init(&ctx);
	MD5_Update(&ctx, buf, strlen(buf));

	// Field 2: Secret Key
	MD5_Update(&ctx, req_auth.shared_secret, strlen(req_auth.shared_secret));
	DBG_LOG(MSG, MOD_SSP, "Key is %s", req_auth.shared_secret);

	// Field 3: Get Stream ID value from the URI
	memset(buf, 0, 11);
	get_query_arg_by_name(hd, req_auth.stream_id_uri_query, strlen(req_auth.stream_id_uri_query), &queryData, &queryLen);

	TSTR(queryData, queryLen, queryDataTmp);
	if ( strncmp(queryDataTmp, "", 1) == 0 ) {
            queryDataTmp = NULL;
        }

	if (queryDataTmp) {
	    snprintf(buf, 11, "%010d", atoi(queryDataTmp));
	    DBG_LOG(MSG, MOD_SSP, "Stream ID is %s", buf);
	    MD5_Update(&ctx, buf, strlen(buf));
	}
	else {
	    DBG_LOG(WARNING, MOD_SSP, "Stream ID missing in URI to calculate MD5 ticket. Closing connection");
	    return -NKN_SSP_MISSING_QUERY_PARAM;
	}

	// Field 4: Get Auth ID value from URL (If present, optional)
	queryData = NULL;
	queryLen = 0;
	get_query_arg_by_name(hd, req_auth.auth_id_uri_query, strlen(req_auth.auth_id_uri_query), &queryData, &queryLen);

	TSTR(queryData, queryLen, queryData2Tmp);
	if ( strncmp(queryData2Tmp, "", 1) == 0 ) {
            queryData2Tmp = NULL;
        }

	if (queryData2Tmp) {
	    MD5_Update(&ctx, queryData2Tmp, strlen(queryData2Tmp));
	    DBG_LOG(MSG, MOD_SSP, "Auth id %s", queryData2Tmp);
	}
	else {
	    DBG_LOG(WARNING, MOD_SSP, "Auth ID missing to calculate MD5 Ticket");
	}

	MD5_Final(calHash, &ctx);		// create Hash

	// Convert to hex to enable matching with query hash
	for(i = 0; i < MD5_DIGEST_LENGTH; i++) {
	    snprintf(&calHashHex[i*2], 3,"%02x", calHash[i]);
	}

	//Perform mem comparison of the two hashes
	TSTR(queryHash, qlen, queryTmp);
	if ( strncmp(queryTmp, "", 1) == 0 ) {
            queryTmp = NULL;
        }

	rv = memcmp(calHashHex, queryTmp, hashLen);

	DBG_LOG(MSG, MOD_SSP, "Provided Ticket: %s", queryTmp);
	DBG_LOG(MSG, MOD_SSP, "Computed Ticket: %s", calHashHex);

	if (!rv){
	    goto exit;
	}

    } //for

 exit:
    return rv;
}

/*
 * verifyPythonHash
 * Return values
 * 0: Successful hash match found
 * non zero: Error, hash does not match
 */
int
verifyPythonHash(const char *hostData, const char *uriData, int hostLen, mime_header_t *hd)
{
    const char *queryHashTmp=NULL;
    int qlen;
    int rv;

    // Get query hash
    get_query_arg_by_name(hd, "h", 1, &queryHashTmp, &qlen);

    TSTR(queryHashTmp, qlen, queryHash);
    if ( strncmp(queryHash, "", 1) == 0 ) {
	queryHash = NULL;
    }

    if (!queryHash) {
        DBG_LOG(WARNING, MOD_SSP, "Query Hash param not provided for verification in URL (Error: %d)", NKN_SSP_MISSING_QUERY_PARAM);
        return -NKN_SSP_MISSING_QUERY_PARAM;
    }

    rv = py_checkMD5Hash(hostData, hostLen, uriData, queryHash);

    return rv;
}

/*
 * findStr
 * Search for a substring within a string
 * Return values:
 * pos >= 0 : Match; -1: No match
 */
int
findStr(const char *pcMainStr, const char *pcSubStr, int length)
{
    int t;
    const char *p, *p2;

    for(t=0; t<length; t++) {
        p = &pcMainStr[t];
        p2 = pcSubStr;

        while(*p2 && *p2==*p) {
            p++;
            p2++;
        }
        if(!*p2) return t; // return pos of match
    }
    return -1; // no match found
}



/*
 * End of ssp_authentication.c
 */
