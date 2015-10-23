#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <sys/queue.h>
#define USE_SIGNALS 1
#include <time.h>		// for "strftime()" and "gmtime()"
#include <fcntl.h>
#include <ctype.h>

#define USE_SPRINTF
#include "nkn_defs.h"
#include "rtsp_func.h"
#include "rtsp_server.h"
#include "rtsp_session.h"
#include "nkn_debug.h"
#include "nkn_util.h"
#include "nkn_stat.h"
#include "parser_utils.h"
#include "rtsp_header.h"
#include "rtsp_signature.h"
#include "strings.h"
#include "nkn_errno.h"

#include <linux/netfilter_ipv4.h>

#define RTPINFO_INCLUDE_RTPTIME 1

NKNCNT_DEF(rtsp_tot_req_parsed, uint64_t, "", "Total parsed RTSP requests");
NKNCNT_DEF(rtsp_tot_res_parsed, uint64_t, "", "Total parsed RTSP responses");

Boolean parseCSeqHeader(rtsp_cb_t * prtsp, char *p, int len)
{
	Boolean parseSucceeded = False;
	int j, n;

	// Look for "CSeq:", skip whitespace,
	// then read everything up to the next \r or \n as 'CSeq':
	parseSucceeded = False;
	for (j = 0; (int) j < (int) len; ++j) {
		while (j < len && (p[j] == ' ' || p[j] == '\t'))
			++j;
		for (n = 0; n < RTSP_PARAM_STRING_MAX - 1 && j < len; ++n, ++j) {
			char c = p[j];
			if (c == '\r' || c == '\n') {
				parseSucceeded = True;
				break;
			}
			prtsp->cseq[n] = c;
		}
		prtsp->cseq[n] = '\0';
		break;
	}
	if (!parseSucceeded)
		return False;

	return True;
}

Boolean parseRangeHeader(rtsp_cb_t * prtsp, char *p, int len)
{
	char *p1;
	int off;
	char nd[4] = "-\r\n";

	UNUSED_ARGUMENT(len);
	// First, find "Range:"

	// Then, run through each of the fields, looking for ones we handle:
	p1 = p;
	while (*p1 == ' ')
		++p1;
	//Locale("C", LC_NUMERIC); // MAXHE_TOBEDONE
	prtsp->rangeStart = -1;
	prtsp->rangeEnd = -1;
	if ( strncasecmp( p1, "npt=", 4 )== 0 ) {
		p1 += 4;
		
		if ( *p1 != '-' ) {
			if ( *p1 == 'n' && *(p1+1) == 'o' && *(p1+2) == 'w' )
				prtsp->rangeStart = -1;
			else
				prtsp->rangeStart = atof( p1 );
		}
		else {
			prtsp->rangeStart = -1;
		}
		off = strcspn( p1, nd );
		if ( *(p1+off) == '-' && *(p1+off+1) != '\r' )
			prtsp->rangeEnd = atof( p1+off+1 );
		else
			prtsp->rangeEnd = -1;
	}

	return True;
}

Boolean parseSessionHeader(rtsp_cb_t * prtsp, char *p, int len)
{
    UNUSED_ARGUMENT(len);
    if((unsigned int)len > sizeof(prtsp->session)) {
	DBG_LOG(MSG, MOD_RTSP, "parseSessionHeader: length of session (%d) too long > %ld", 
		len, sizeof(prtsp->session));
	return False;/* bz 4464, this return was commented for the above check */
    }

    memcpy(prtsp->session, p, len);
    prtsp->session[len]=0;

    if ( prtsp->fOurSessionId == (unsigned long long) atoll_len(p, len) )
	    return True;
    else
    	return False;
}

/* added for BZ 3242:
   parses the session header but doesnt validate the session id in the case of relay client.
   Additionally sets the fOurSessionId to the sesssion id gleaned from the SETUP response
*/
Boolean parseSessionHeaderWithoutValidation(rtsp_cb_t * prtsp, char *p, int len)
{
    UNUSED_ARGUMENT(len);
    if((unsigned int)len > sizeof(prtsp->session)) {
	DBG_LOG(MSG, MOD_RTSP, "parseSessionHeader: length of session (%d) too long > %ld", 
		len, sizeof(prtsp->session));
	return False;/* bz 4464, this return was commented for the above check */
    }

    memcpy(prtsp->session, p, len);
    prtsp->session[len]=0;

    //    prtsp->fOurSessionId = (unsigned long long) atoll_len(p, len); 
    return True;

}

Boolean parseScaleHeader(rtsp_cb_t * prtsp, char *p, int len)
{
    UNUSED_ARGUMENT(len);
    // First, find "Scale:"

    // Then, run through each of the fields, looking for ones we handle:
    char const *fields = p;
    while (*fields == ' ')
	++fields;
    float sc;
    if (sscanf(fields, "%f", &sc) == 1) {
	prtsp->scale = sc;
    } else {
	return False;		// The header is malformed
    }

    return True;
}

Boolean parseRTPInfoHeader(rtsp_cb_t * prtsp, char *p, int len)
{
	int trackID;

	UNUSED_ARGUMENT(len);
	// First, find "Transport:"

	// Then, run through each of the fields, looking for ones we handle:
	char const *fields = p;
	char *field = (char *) nkn_malloc_type(strlen(fields) + 1, mod_rtsp_parser_prih_malloc);

	trackID = 0;
	prtsp->rtp_info_seq[0] = ~0L;
	prtsp->rtp_info_seq[1] = ~0L;
	prtsp->rtp_time[0] = ~0L;
	prtsp->rtp_time[1] = ~0L;
	while (sscanf(fields, "%[^;,\r\n]", field) == 1) {
		if (strncmp(field, "url=", 4) == 0) {
			trackID ++;
		} else if (strncmp(field, "seq=", 4) == 0) {
			prtsp->rtp_info_seq[trackID-1] = atol(&field[4]);
		} else if (strncmp(field, "rtptime=", 8) == 0) {
			prtsp->rtp_time[trackID-1] = atol(&field[8]);
		}

		fields += strlen(field);
		while (*fields == ';' || *fields == ',' || *fields == ' ')
			++fields;		// skip over separating ';' chars
		if (*fields == '\0' || *fields == '\r' || *fields == '\n')
			break;
	}
	free(field);

	return True;
}

Boolean parseTransportHeader(rtsp_cb_t * prtsp, char *p, int len)
{
    uint16_t p1, p2;
    unsigned ttl, rtpCid, rtcpCid;

    UNUSED_ARGUMENT(len);
    // First, find "Transport:"

    // Then, run through each of the fields, looking for ones we handle:
    char const *fields = p;
    char *field = (char *) nkn_malloc_type(strlen(fields) + 1, mod_rtsp_parser_pth_malloc);

    prtsp->streamingMode = RAW_UDP;
    while (sscanf(fields, "%[^;\r\n]", field) == 1) {
	if (strcmp(field, "RTP/AVP/TCP") == 0) {
	    prtsp->streamingMode = RTP_TCP;
	} else if (strcmp(field, "RAW/RAW/UDP") == 0 ||
		   strcmp(field, "MP2T/H2221/UDP") == 0) {
	    prtsp->streamingMode = RAW_UDP;
	    strcpy(prtsp->streamingModeString, field);
	} else if (strcmp(field, "RTP/AVP") == 0 ||
		   strcmp(field, "RTP/AVP/UDP") == 0) {
	    prtsp->streamingMode = RTP_UDP;
	    strcpy(prtsp->streamingModeString, field);
	} else if (strcmp(field, "MP2T/H2221/TCP") == 0) {
	    prtsp->streamingMode = RAW_TCP;
	    strcpy(prtsp->streamingModeString, field);
	} else if (strncasecmp(field, "mode=", 5) == 0) {
		if(strlen(field+5) > 31)  /*mode field can be 32 bytes 32+NULL Termination */
			return False;
	    strcpy(prtsp->mode, field+5);
	} else if (strncasecmp(field, "destination=", 12) == 0) {
	    strcpy(prtsp->destinationAddressStr, field + 12);
	} else if (sscanf(field, "ttl%u", &ttl) == 1) {
	    prtsp->destinationTTL = (u_int8_t) ttl;
	} else if (sscanf(field, "port=%hu", &p1) == 1) {
	    prtsp->clientRTPPortNum = p1;
	} else if (sscanf(field, "client_port=%hu-%hu", &p1, &p2) == 2) {
	    prtsp->clientRTPPortNum = p1;
	    prtsp->clientRTCPPortNum = p2;
	} else if (sscanf(field, "client_port=%hu", &p1) == 1) {
	    prtsp->clientRTPPortNum = p1;
	    prtsp->clientRTCPPortNum =
		prtsp->streamingMode == RAW_UDP ? 0 : p1 + 1;
	} else if (sscanf(field, "server_port=%hu-%hu", &p1, &p2) == 2) {
	    prtsp->serverRTPPortNum = p1;
	    prtsp->serverRTCPPortNum = p2;
	} else if (strncasecmp(field, "ssrc=", 5) == 0) {
	    strcpy(prtsp->ssrc, field+5);
	} else if (sscanf(field, "interleaved=%u-%u", &rtpCid, &rtcpCid) ==
		   2) {
	    prtsp->rtpChannelId = (unsigned char) rtpCid;
	    prtsp->rtcpChannelId = (unsigned char) rtcpCid;
	}

	fields += strlen(field);
	while (*fields == ';')
	    ++fields;		// skip over separating ';' chars
	if (*fields == '\0' || *fields == '\r' || *fields == '\n')
	    break;
    }
    free(field);

    return True;
}

Boolean parsePlayNowHeader(rtsp_cb_t * prtsp, char *p, int len)
{
    // Find "x-playNow:" header, if present
    UNUSED_ARGUMENT(prtsp);
    UNUSED_ARGUMENT(p);
    UNUSED_ARGUMENT(len);

    return True;
}

Boolean parseAuthorizationHeader(rtsp_cb_t * prtsp, char *p, int len)
{
    int mylen;
    char *fields, *parameter, *value;

    UNUSED_ARGUMENT(len);
    // First, find "Authorization: Digest "

    // Then, run through each of the fields, looking for ones we handle:
    fields = p;
    while (*fields == ' ')
	++fields;

    /*bug 4292: strlen(fields) will only return length of the fields string;
     *this length returned is used to allocate memory for subsequent name:value
     *pairs. the worst case allocation required for name or value should be length  1
     *to accomodate for the null termination
     */
    mylen = strlen(fields) + 1;
    parameter = (char *) nkn_malloc_type(mylen, mod_rtsp_parser_pah_malloc);
    value = (char *) nkn_malloc_type(mylen, mod_rtsp_parser_pah_malloc);

    while (1) {
	value[0] = '\0';
	if (sscanf(fields, "%[^=]=\"%[^\"]\"", parameter, value) != 2 &&
	    sscanf(fields, "%[^=]=\"\"", parameter) != 1) {
	    break;
	}
	if (strcmp(parameter, "username") == 0) {
	    prtsp->username = nkn_strdup_type(value, mod_rtsp_parser_ah_strdup);
	} else if (strcmp(parameter, "realm") == 0) {
	    prtsp->realm = nkn_strdup_type(value, mod_rtsp_parser_ah_strdup);
	} else if (strcmp(parameter, "nonce") == 0) {
	    prtsp->nonce = nkn_strdup_type(value, mod_rtsp_parser_ah_strdup);
	} else if (strcmp(parameter, "uri") == 0) {
	    prtsp->uri = nkn_strdup_type(value, mod_rtsp_parser_ah_strdup);
	} else if (strcmp(parameter, "response") == 0) {
	    prtsp->response = nkn_strdup_type(value, mod_rtsp_parser_ah_strdup);
	}

	fields +=
	    strlen(parameter) + 2 /*="*/  + strlen(value) + 1 /*" */ ;
	while (*fields == ',' || *fields == ' ')
	    ++fields;
	// skip over any separating ',' and ' ' chars
	if (*fields == '\0' || *fields == '\r' || *fields == '\n')
	    break;
    }
    free(parameter);
    free(value);

    return True;
}

Boolean ParseCmdName(rtsp_cb_t * prtsp, char *p, int len)
{
	if (!p || len + 1 >= RTSP_PARAM_STRING_MAX) {
		return (False);
	}
	strncpy(prtsp->cmdName, p, len);
	prtsp->cmdName[len] = '\0';
	return (True);
}

Boolean  ParseQTLateTolerance(rtsp_cb_t * prtsp, char *p, int len);
Boolean  ParseQTLateTolerance(rtsp_cb_t * prtsp, char *p, int len)
{
	if(!p || !len) {
		return False;
	}

	sscanf(p, "late-tolerance=%f\r\n", &prtsp->qt_late_tolerance);
	
	return True;
}

Boolean parseCacheControl(rtsp_cb_t * prtsp, char *p, int len);
Boolean parseCacheControl(rtsp_cb_t * prtsp, char *p, int len)
{
	if (!p || !len) {
		return False;
	}
	
	if (strncasecmp(p, "no-cache", len)) {
		prtsp->cacheControl = RTSP_CACHE_CONTROL_NO_CACHE;		
	}
	else if (strncasecmp(p, "must-revalidate", len)) {
		prtsp->cacheControl = RTSP_CACHE_CONTROL_MUST_REVALIDATE;
	}
	else if (strncasecmp(p, "no-transform", len)) {
		prtsp->cacheControl = RTSP_CACHE_CONTROL_NO_TRANSFORM;
	}
	else if (strncasecmp(p, "public", len)) {
		prtsp->cacheControl = RTSP_CACHE_CONTROL_PUBLIC;
	}
	else if (strncasecmp(p, "private", len)) {
		prtsp->cacheControl = RTSP_CACHE_CONTROL_PRIVATE;	
	}
	else if (strncasecmp(p, "max-age=", strlen("max-age="))) {
		prtsp->cacheControl = RTSP_CACHE_CONTROL_MAX_AGE;
		sscanf(p,"max-age=%u",&prtsp->cacheMaxAge);	
	}

	return True; 
}

Boolean parseDate(time_t *t, char *p, int len);
Boolean parseDate(time_t *t, char *p, int len)
{
	if (!p || !len) {
		return False;
	}
	
	*t = parse_date(p, p + (len - 1));	
	
	return True; 
}

#define RTPINFO_INCLUDE_RTPTIME 1

void rtsp_init_buf(rtsp_cb_t * prtsp)
{
	static char null_string[] = {'\0', '\0'};
	// Initialize the result parameters to default values:
	prtsp->flag = 0;
	prtsp->method = METHOD_UNKNOWN;
	prtsp->cmdName[0] = 0;
	prtsp->urlPreSuffix = null_string;
	prtsp->urlSuffix = prtsp->urlPreSuffix;
	prtsp->cseq[0] = 0;
	prtsp->session[0] = 0;
	//prtsp->rtsp_url[0] = 0;

	prtsp->rangeStart = -1.0;
	prtsp->rangeEnd = -1.0;
	prtsp->scale = 1.0;

	//prtsp->streamingMode = RTP_UDP;	// Set it only in parse transport
	prtsp->streamingModeString[0] = 0;
	prtsp->destinationAddressStr[0] = 0;
	prtsp->destinationTTL = 255;
	prtsp->clientRTPPortNum = 0;
	prtsp->clientRTCPPortNum = 1;
	prtsp->rtpChannelId = prtsp->rtcpChannelId = 0xFF;
	prtsp->cacheControl = RTSP_CACHE_CONTROL_NO_CACHE;
	prtsp->cacheMaxAge = 0;
	prtsp->expires = 0;
	prtsp->nkn_tsk_id = 0;

	prtsp->username = NULL;
	prtsp->realm = NULL;
	prtsp->nonce = NULL;
	prtsp->uri = NULL;
	prtsp->response = NULL;

	prtsp->rtp_info_seq[0] = ~0L;
	prtsp->rtp_info_seq[1] = ~0L;
	prtsp->rtp_time[0] = ~0L;
	prtsp->rtp_time[1] = ~0L;
}

/**
 * @brief Function to preserve backward compatibility
 *	As functionality is added with the mime parser
 *	We can remove this dependency.
 *
 * @param prtsp - rtsp control block context pointer
 *
 * @return - RPS_OK
 */
static rtsp_parse_status_t rtsp_parse_bkwd_compat(rtsp_cb_t * prtsp)
{
	unsigned int hdr_attr = 0;
	int hdr_cnt = 0;
	const char *hdr_str = NULL;
	int hdr_len = 0;

	if ((0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_X_METHOD,
			&hdr_str, &hdr_len, &hdr_attr, &hdr_cnt))) {
		if (False == ParseCmdName(prtsp, (char *) hdr_str, hdr_len)) {
			prtsp->method = METHOD_BAD;
			prtsp->status = 400;
			return RPS_ERROR;
		}
	}
	if ((RTSP_IS_FLAG_SET(prtsp->flag, RTSP_HDR_CSEQ))
			&& (0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_CSEQ,
			&hdr_str, &hdr_len, &hdr_attr, &hdr_cnt))) {
		parseCSeqHeader(prtsp, (char *) hdr_str, hdr_len);
	}
	else {
		RTSP_SET_ERROR(prtsp, RTSP_STATUS_400, NKN_RTSP_PVE_REQ_CSEQ);
		prtsp->status = 400;
		return (RPS_ERROR);
	}

	if ((RTSP_IS_FLAG_SET(prtsp->flag, RTSP_HDR_RANGE))
			&& (0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_RANGE,
			&hdr_str, &hdr_len, &hdr_attr, &hdr_cnt))) {
		parseRangeHeader(prtsp, (char *) hdr_str, hdr_len);
	}

	if ((RTSP_IS_FLAG_SET(prtsp->flag, RTSP_HDR_SCALE))
			&& (0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_SCALE,
			&hdr_str, &hdr_len, &hdr_attr, &hdr_cnt))) {
		parseScaleHeader(prtsp, (char *) hdr_str, hdr_len);
	}

	if ((RTSP_IS_FLAG_SET(prtsp->flag, RTSP_HDR_SESSION))
			&& (0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_SESSION,
			&hdr_str, &hdr_len, &hdr_attr, &hdr_cnt))) {
		if (parseSessionHeader(prtsp, (char *) hdr_str, hdr_len) == False) {
			RTSP_SET_ERROR(prtsp, RTSP_STATUS_454, NKN_RTSP_PVE_REQ_SESSION);
			prtsp->status = 454;
			return (RPS_ERROR);
		}
	}
	else if (prtsp->need_session_id) {
		RTSP_SET_ERROR(prtsp, RTSP_STATUS_400, NKN_RTSP_PVE_REQ_SESSION);
		prtsp->status = 400;
		return (RPS_ERROR);
	}

	if ((RTSP_IS_FLAG_SET(prtsp->flag, RTSP_HDR_RTP_INFO))
			&& (0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_RTP_INFO,
			&hdr_str, &hdr_len, &hdr_attr, &hdr_cnt))) {
		parseRTPInfoHeader(prtsp, (char *) hdr_str, hdr_len);
	}

	if ((RTSP_IS_FLAG_SET(prtsp->flag, RTSP_HDR_TRANSPORT))
			&& (0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_TRANSPORT,
			&hdr_str, &hdr_len, &hdr_attr, &hdr_cnt))) {
		if(parseTransportHeader(prtsp, (char *) hdr_str, hdr_len) == False) {
			prtsp->status = 400;
			return (RPS_ERROR);
		}
	}
	else if (prtsp->method == METHOD_SETUP) {
		RTSP_SET_ERROR(prtsp, RTSP_STATUS_400, NKN_RTSP_PVE_REQ_TRANSPORT);
		prtsp->status = 400;
		return (RPS_ERROR);
	}

	if ((RTSP_IS_FLAG_SET(prtsp->flag, RTSP_HDR_X_PLAY_NOW))
			&& (0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_X_PLAY_NOW,
			&hdr_str, &hdr_len, &hdr_attr, &hdr_cnt))) {
		parsePlayNowHeader(prtsp, (char *) hdr_str, hdr_len);
	}

	if ((RTSP_IS_FLAG_SET(prtsp->flag, RTSP_HDR_AUTHORIZATION))
			&& (0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_AUTHORIZATION,
			&hdr_str, &hdr_len, &hdr_attr, &hdr_cnt))) {
		/* bug 4292: even after fixing the incorrect malloc without acounting for
		 * NULL termination, seeing some more invalid writes in valgrind. Since there
		 * is no use for the data parsed in this function; commenting this out and opening
		 * a new defect for the same
		 */
		//parseAuthorizationHeader(prtsp, (char *) hdr_str, hdr_len);
	}

	if( (RTSP_IS_FLAG_SET(prtsp->flag, RTSP_HDR_X_QT_LATE_TOLERANCE)) &&
	    (0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_X_QT_LATE_TOLERANCE, 
				     &hdr_str, &hdr_len, &hdr_attr, &hdr_cnt)) ) {
		ParseQTLateTolerance(prtsp, (char*) hdr_str, hdr_len);
	}

	if( (RTSP_IS_FLAG_SET(prtsp->flag, RTSP_HDR_CACHE_CONTROL)) &&
	    (0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_CACHE_CONTROL, 
				     &hdr_str, &hdr_len, &hdr_attr, &hdr_cnt)) ) {
		parseCacheControl(prtsp, (char*) hdr_str, hdr_len);
	}

	if( (RTSP_IS_FLAG_SET(prtsp->flag, RTSP_HDR_EXPIRES)) &&
	    (0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_EXPIRES, 
				     &hdr_str, &hdr_len, &hdr_attr, &hdr_cnt)) ) {
		parseDate(&prtsp->expires, (char*) hdr_str, hdr_len);
	}

	if( (RTSP_IS_FLAG_SET(prtsp->flag, RTSP_HDR_DATE)) &&
	    (0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_DATE, 
				     &hdr_str, &hdr_len, &hdr_attr, &hdr_cnt)) ) {
		parseDate(&prtsp->curr_date, (char*) hdr_str, hdr_len);
	}

	/* This check needs to be last, do not add any code below this check.
	 */
	prtsp->cb_req_content_len = 0;
	if (0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_CONTENT_LENGTH,
			&hdr_str, &hdr_len, &hdr_attr, &hdr_cnt)) {
		prtsp->cb_req_content_len = atoi(hdr_str);
		if (prtsp->cb_req_content_len && 
				prtsp->cb_reqlen < prtsp->cb_req_content_len) {
			return RPS_NEED_CONTENT;
		}
	}
	return RPS_OK;
}

/**
 * @brief Function to preserve backward compatibility
 *	As functionality is added with the mime parser
 *	We can remove this dependency.
 *
 * @param prtsp - rtsp control block context pointer
 *
 * @return - RPS_OK
 */
static rtsp_parse_status_t rtsp_parse_origin_bkwd_compat(rtsp_cb_t * prtsp)
{
	unsigned int hdr_attr = 0;
	int hdr_cnt = 0;
	const char *hdr_str = NULL;
	int hdr_len = 0;

	if ((0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_X_METHOD,
			&hdr_str, &hdr_len, &hdr_attr, &hdr_cnt))) {
		if (False == ParseCmdName(prtsp, (char *) hdr_str, hdr_len)) {
			prtsp->method = METHOD_BAD;
			return RPS_ERROR;
		}
	}
	if ((RTSP_IS_FLAG_SET(prtsp->flag, RTSP_HDR_CSEQ))
			&& (0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_CSEQ,
			&hdr_str, &hdr_len, &hdr_attr, &hdr_cnt))) {
		parseCSeqHeader(prtsp, (char *) hdr_str, hdr_len);
	}
	else {
		RTSP_SET_ERROR(prtsp, RTSP_STATUS_400, NKN_RTSP_PVE_REQ_CSEQ);
		return (RPS_ERROR);
	}

	if ((RTSP_IS_FLAG_SET(prtsp->flag, RTSP_HDR_SESSION))
			&& (0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_SESSION,
			&hdr_str, &hdr_len, &hdr_attr, &hdr_cnt))) {
		if (parseSessionHeader(prtsp, (char *) hdr_str, hdr_len) == False) {
			RTSP_SET_ERROR(prtsp, RTSP_STATUS_454, NKN_RTSP_PVE_REQ_SESSION);
			return (RPS_ERROR);
		}
	}
	else if (prtsp->need_session_id) {
		RTSP_SET_ERROR(prtsp, RTSP_STATUS_400, NKN_RTSP_PVE_REQ_SESSION);
		return (RPS_ERROR);
	}
#if 0
	if ((RTSP_IS_FLAG_SET(prtsp->flag, RTSP_HDR_RTP_INFO))
			&& (0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_RTP_INFO,
			&hdr_str, &hdr_len, &hdr_attr, &hdr_cnt))) {
		parseRTPInfoHeader(prtsp, (char *) hdr_str, hdr_len);
	}

	if ((RTSP_IS_FLAG_SET(prtsp->flag, RTSP_HDR_TRANSPORT))
			&& (0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_TRANSPORT,
			&hdr_str, &hdr_len, &hdr_attr, &hdr_cnt))) {
		if(parseTransportHeader(prtsp, (char *) hdr_str, hdr_len) == False) {
			return (RPS_ERROR);
		}
	}
	else if (prtsp->method == METHOD_SETUP) {
		RTSP_SET_ERROR(prtsp, RTSP_STATUS_400, NKN_RTSP_PVE_REQ_TRANSPORT);
		return (RPS_ERROR);
	}

	if ((RTSP_IS_FLAG_SET(prtsp->flag, RTSP_HDR_X_PLAY_NOW))
			&& (0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_X_PLAY_NOW,
			&hdr_str, &hdr_len, &hdr_attr, &hdr_cnt))) {
		parsePlayNowHeader(prtsp, (char *) hdr_str, hdr_len);
	}

	if ((RTSP_IS_FLAG_SET(prtsp->flag, RTSP_HDR_AUTHORIZATION))
			&& (0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_AUTHORIZATION,
			&hdr_str, &hdr_len, &hdr_attr, &hdr_cnt))) {
		/* bug 4292: even after fixing the incorrect malloc without acounting for
		 * NULL termination, seeing some more invalid writes in valgrind. Since there
		 * is no use for the data parsed in this function; commenting this out and opening
		 * a new defect for the same
		 */
		//parseAuthorizationHeader(prtsp, (char *) hdr_str, hdr_len);
	}

	if( (RTSP_IS_FLAG_SET(prtsp->flag, RTSP_HDR_CACHE_CONTROL)) &&
	    (0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_CACHE_CONTROL, 
				     &hdr_str, &hdr_len, &hdr_attr, &hdr_cnt)) ) {
		parseCacheControl(prtsp, (char*) hdr_str, hdr_len);
	}

	if( (RTSP_IS_FLAG_SET(prtsp->flag, RTSP_HDR_EXPIRES)) &&
	    (0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_EXPIRES, 
				     &hdr_str, &hdr_len, &hdr_attr, &hdr_cnt)) ) {
		parseDate(&prtsp->expires, (char*) hdr_str, hdr_len);
	}

	if( (RTSP_IS_FLAG_SET(prtsp->flag, RTSP_HDR_X_QT_LATE_TOLERANCE)) &&
	    (0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_X_QT_LATE_TOLERANCE, 
				     &hdr_str, &hdr_len, &hdr_attr, &hdr_cnt)) ) {
		ParseQTLateTolerance(prtsp, (char*) hdr_str, hdr_len);
	}
#endif
	if( (RTSP_IS_FLAG_SET(prtsp->flag, RTSP_HDR_DATE)) &&
	    (0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_DATE, 
				     &hdr_str, &hdr_len, &hdr_attr, &hdr_cnt)) ) {
		parseDate(&prtsp->curr_date, (char*) hdr_str, hdr_len);
	}

	/* This check needs to be last, do not add any code below this check.
	 */
	prtsp->cb_resp_content_len = 0;
	if (0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_CONTENT_LENGTH,
			&hdr_str, &hdr_len, &hdr_attr, &hdr_cnt)) {
		prtsp->cb_resp_content_len = atoi(hdr_str);
		if (prtsp->cb_resp_content_len && 
				prtsp->cb_resplen < prtsp->cb_resp_content_len) {
			return RPS_NEED_CONTENT;
		}
	}
	return RPS_OK;
}

/**
 * @brief Function to preserve backward compatibility for responses
 *	As functionality is added with the mime parser
 *	We can remove this dependency.
 *
 * @param prtsp - rtsp control block context pointer
 *
 * @return - RPS_OK
 */
static rtsp_parse_status_t rtsp_parse_bkwd_compat_response(rtsp_cb_t * prtsp)
{
	unsigned int hdr_attr = 0;
	int hdr_cnt = 0;
	const char *hdr_str = NULL;
	int hdr_len = 0;

	if ((0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_X_METHOD,
			&hdr_str, &hdr_len, &hdr_attr, &hdr_cnt))) {
		if (False == ParseCmdName(prtsp, (char *) hdr_str, hdr_len)) {
			prtsp->method = METHOD_BAD;
			return RPS_ERROR;
		}
	}
	if ((RTSP_IS_FLAG_SET(prtsp->flag, RTSP_HDR_CSEQ))
			&& (0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_CSEQ,
			&hdr_str, &hdr_len, &hdr_attr, &hdr_cnt))) {
		parseCSeqHeader(prtsp, (char *) hdr_str, hdr_len);
	}
	else {
		RTSP_SET_ERROR(prtsp, RTSP_STATUS_400, NKN_RTSP_PVE_REQ_CSEQ);
		return (RPS_ERROR);
	}

	if ((RTSP_IS_FLAG_SET(prtsp->flag, RTSP_HDR_RANGE))
			&& (0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_RANGE,
			&hdr_str, &hdr_len, &hdr_attr, &hdr_cnt))) {
		parseRangeHeader(prtsp, (char *) hdr_str, hdr_len);
	}

	if ((RTSP_IS_FLAG_SET(prtsp->flag, RTSP_HDR_SCALE))
			&& (0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_SCALE,
			&hdr_str, &hdr_len, &hdr_attr, &hdr_cnt))) {
		parseScaleHeader(prtsp, (char *) hdr_str, hdr_len);
	}

	/*
	  BZ 3242:
	  when we are a relay client, we will not have generated the session id ourselves and
	  will have to glean the same from the SETUP response given by the Publishing Server. 
	  We will not have a valid session id at this point and the subsequent validation in the 
	  parseSessionHeader is destined to fail _always_. Hence the parseSessionHeader when parsing 
	  the session id from the response should set the prtsp->fOurSessionId to the value gleaned 
	  from the response in the case of parsing a response.
	*/
	if ((RTSP_IS_FLAG_SET(prtsp->flag, RTSP_HDR_SESSION))
			&& (0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_SESSION,
			&hdr_str, &hdr_len, &hdr_attr, &hdr_cnt))) {
		if(prtsp->fOurSessionId) {
			/* if we are a relay client then the control flow will reach here */
			if (parseSessionHeader(prtsp, (char *) hdr_str, hdr_len) == False) {
				RTSP_SET_ERROR(prtsp, RTSP_STATUS_454, NKN_RTSP_PVE_REQ_SESSION);
				return (RPS_ERROR);
			}
		} else {
			/* shoud never come here for a relay client, which is the only 
			 * case when this function will be called */
			if (parseSessionHeaderWithoutValidation(prtsp, (char *) hdr_str, hdr_len) == False) {
				RTSP_SET_ERROR(prtsp, RTSP_STATUS_454, NKN_RTSP_PVE_REQ_SESSION);
				return (RPS_ERROR);
			}
		}
		
	}

	else if (prtsp->need_session_id) {
		RTSP_SET_ERROR(prtsp, RTSP_STATUS_400, NKN_RTSP_PVE_REQ_SESSION);
		return (RPS_ERROR);
	}

	if ((RTSP_IS_FLAG_SET(prtsp->flag, RTSP_HDR_RTP_INFO))
			&& (0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_RTP_INFO,
			&hdr_str, &hdr_len, &hdr_attr, &hdr_cnt))) {
		parseRTPInfoHeader(prtsp, (char *) hdr_str, hdr_len);
	}

	if ((RTSP_IS_FLAG_SET(prtsp->flag, RTSP_HDR_TRANSPORT))
			&& (0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_TRANSPORT,
			&hdr_str, &hdr_len, &hdr_attr, &hdr_cnt))) {
		if(parseTransportHeader(prtsp, (char *) hdr_str, hdr_len) == False) {
			return (RPS_ERROR);
		}

	}
	else if (prtsp->method == METHOD_SETUP) {
		RTSP_SET_ERROR(prtsp, RTSP_STATUS_400, NKN_RTSP_PVE_REQ_TRANSPORT);
		return (RPS_ERROR);
	}

	if ((RTSP_IS_FLAG_SET(prtsp->flag, RTSP_HDR_X_PLAY_NOW))
			&& (0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_X_PLAY_NOW,
			&hdr_str, &hdr_len, &hdr_attr, &hdr_cnt))) {
		parsePlayNowHeader(prtsp, (char *) hdr_str, hdr_len);
	}

	if ((RTSP_IS_FLAG_SET(prtsp->flag, RTSP_HDR_AUTHORIZATION))
			&& (0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_AUTHORIZATION,
			&hdr_str, &hdr_len, &hdr_attr, &hdr_cnt))) {
		/* bug 4292: even after fixing the incorrect malloc without acounting for
		 * NULL termination, seeing some more invalid writes in valgrind. Since there
		 * is no use for the data parsed in this function; commenting this out and opening
		 * a new defect for the same
		 */
		//parseAuthorizationHeader(prtsp, (char *) hdr_str, hdr_len);
	}

	if( (RTSP_IS_FLAG_SET(prtsp->flag, RTSP_HDR_X_QT_LATE_TOLERANCE)) &&
			(0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_X_QT_LATE_TOLERANCE, 
			&hdr_str, &hdr_len, &hdr_attr, &hdr_cnt)) ) {
		ParseQTLateTolerance(prtsp, (char*) hdr_str, hdr_len);
	}

	/* This check needs to be last, do not add any code below this check.
	 */
	prtsp->cb_resp_content_len = 0;
	if (0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_CONTENT_LENGTH,
			&hdr_str, &hdr_len, &hdr_attr, &hdr_cnt)) {
		prtsp->cb_resp_content_len = atoi(hdr_str);
		if (prtsp->cb_resp_content_len && 
				prtsp->cb_resplen < prtsp->cb_resp_content_len) {
			return RPS_NEED_CONTENT;
		}
	}
	return RPS_OK;
}



static rtsp_parse_status_t
rtsp_parse_version(rtsp_cb_t * prtsp, const char *ver_string,
		   int ver_str_len, const char **end)
{
    assert(prtsp && ver_string);

    const char *scan = ver_string;
    const char *scan_end = ver_string + ver_str_len;
    const char *ver_num = NULL;
    int decimal_done = 0;

    if (ver_str_len < (int) RTSP_V_STR_LEN) {
	RTSP_SET_ERROR(prtsp, RTSP_STATUS_400, NKN_RTSP_PE_VER_LT1);
	return (RPS_NEED_MORE_DATA);
    }

    if (0 == strncasecmp(ver_string, RTSP_V_STR, RTSP_V_STR_LEN)) {

	scan += RTSP_V_STR_LEN;
	ver_num = scan;
	while (scan <= scan_end) {
	    if (!isdigit(*scan)) {
		if (decimal_done) {
		    break;
		} else if (*scan == '.') {
		    decimal_done++;
		    if (!isdigit(*(scan + 1))) {
			RTSP_SET_ERROR(prtsp, RTSP_STATUS_400, NKN_RTSP_PE_VER_1);
			return (RPS_ERROR);
		    }
		} else {
		    break;
		}
	    }
	    scan++;
	}

	if (!decimal_done){
	    RTSP_SET_ERROR(prtsp, RTSP_STATUS_400, NKN_RTSP_PE_VER_2);
	    return (RPS_ERROR);
	}

	if (scan > scan_end) {
	    RTSP_SET_ERROR(prtsp, RTSP_STATUS_400, NKN_RTSP_PE_VER_LT2);
	    return (RPS_NEED_MORE_DATA);
	}

	if (strncmp(ver_num, "1.0", 3) != 0) {
	    RTSP_SET_ERROR(prtsp, RTSP_STATUS_400, NKN_RTSP_PE_VER_2);
	    return (RPS_ERROR);
	}
	/* Add Known Header */
	mime_hdr_add_known(&prtsp->hdr, RTSP_HDR_X_VERSION, ver_string,
			   scan - ver_string);
	mime_hdr_add_known(&prtsp->hdr, RTSP_HDR_X_VERSION_NUM, ver_num,
			   scan - ver_num);
	if (end) {
	    *end = scan;
	}
	return (RPS_OK);
    } else {
	RTSP_SET_ERROR(prtsp, RTSP_STATUS_400, NKN_RTSP_PE_VER_3);
	return (RPS_ERROR);
    }
}


/**
 * @brief  Extract URL information
 *
 * @param prtsp - RTSP control block context
 * @param str - URL String
 * @param string_len - Length of the url string
 * @param next_ptr - Pointer upto which parsing is done
 *
 * The following known headers are added in prtsp->hdr data struct.
 *	RTSP_HDR_X_PROT
 *	RTSP_HDR_X_HOST
 *	RTSP_HDR_X_URL
 *	RTSP_HDR_X_ABS_PATH
 *
 * TODO  - Add sub error Codes
 *
 * @return parse status
 *
 */
static rtsp_parse_status_t rtsp_parse_url(rtsp_cb_t *prtsp,
					  const char *str,
					  int32_t string_len,
					  const char **next_ptr)
{

	const char *scan = NULL, *end = NULL;
	const char *host = NULL, *abs_path = NULL, *port = NULL, *query = NULL;
	int32_t host_len = 0, abs_path_len = 0, port_len = 0, query_len = 0;
	int32_t sym_state = 0x00;
	uint32_t port_num = 0;

	assert(prtsp && str);
	if (string_len <= 0)
		return RPS_ERROR;

	/** 
	 * rtsp_URL  =   ( "rtsp:" | "rtspu:" ) "//" host [":" port][abs_path]
	 */

	/* Minimum length the URL can have is
	 * sizeof("rtsp://a") 
	 * since host can have a minimum len of 1 
	 * rtsp://a is a valid URL
	 */

	if (string_len <= (int32_t) ST_STRLEN("rtsp://a")) {
		RTSP_SET_ERROR(prtsp, RTSP_STATUS_400, NKN_RTSP_PE_URL_LT1);
		return (RPS_NEED_MORE_DATA);
	}

	scan = str;
	end = str + string_len;
	if (('u' == tolower(scan[ST_STRLEN("rtspu") - 1]))
			&& (0 == strncasecmp(str, "rtspu://", ST_STRLEN("rtspu://")))) {
		/* URL Type is UDP */
		mime_hdr_add_known(&prtsp->hdr, RTSP_HDR_X_PROTOCOL, scan, ST_STRLEN("rtspu://"));
		scan += ST_STRLEN("rtspu://");
	} else if (0 == strncasecmp(str, "rtsp://", ST_STRLEN("rtsp://"))) {
		/* URL Type is TCP */
		mime_hdr_add_known(&prtsp->hdr, RTSP_HDR_X_PROTOCOL, scan, ST_STRLEN("rtsp://"));
		scan += ST_STRLEN("rtsp://");
	} else if ((*str == '*') && (prtsp->method == METHOD_OPTIONS)) {
		++scan;
		if (next_ptr)
			*next_ptr = scan;
		return (RPS_OK);
	}
	else {
		RTSP_SET_ERROR(prtsp, RTSP_STATUS_400, NKN_RTSP_PE_URL_1_PROTOCOL);
		return (RPS_ERROR);
	}

	/* host */
	host = scan;
	if (*scan == '-' || !isalnum(*scan)){
		RTSP_SET_ERROR(prtsp, RTSP_STATUS_400, NKN_RTSP_PE_URL_2_HOST);
		return (RPS_ERROR);
	}

	/* TODO  need to validate with proper spec
	 * Numbers allowed in start of name
	 * hname :: <name>*["." name]
	 * name :: <let or digit>[*[<let or digit or hyphen]<let or digit>]
	 * */
#define STATE_DOT 0x01
#define STATE_DASH 0x02

	while ((scan < end) && (*scan)) {
		if (isalnum(*scan)) {
			sym_state = 0x00;
		} 
		else if (*scan == '-') {
			if (sym_state & STATE_DOT) {
				break;
			}
			sym_state = STATE_DASH;
		} else if (*scan == '.') {
			if (sym_state & (STATE_DOT | STATE_DASH)) {
				break;
			}
			sym_state = STATE_DOT;
		} else {
			/* Any other char End mark */
			break;
		}
		scan++;
	}

	if (sym_state) {
		/* Host cannot be in this states below
		 * a .-
		 * b -.
		 * c ..
		 * */
		RTSP_SET_ERROR(prtsp, RTSP_STATUS_400, NKN_RTSP_PE_URL_3_HOST);
		return (RPS_ERROR);
	}
	host_len = scan - host;

	/* [":" port] */
	if ((scan < end) && *scan == ':') {
		scan++;
		port = scan;
		while ((scan < end) && *scan && isdigit(*scan)) {
			port_num = port_num * 10 + (unsigned int) ((*scan) - '0');
			scan++;
		}
		if ((scan < end) && (port_num == 0)) {
			RTSP_SET_ERROR(prtsp, RTSP_STATUS_400, NKN_RTSP_PE_URL_4_PORT);
			return (RPS_ERROR);
		}
		port_len = scan - port;
	}

	/* Check is / is present after host name. For options request real player
	 * is just giving server name/IP only. Allow this also for only options.
	 * BZ 4353.
	 */
	if (*scan != '/' && !(*scan == ' ' && prtsp->method == METHOD_OPTIONS)) {
		RTSP_SET_ERROR(prtsp, RTSP_STATUS_400, NKN_RTSP_PE_URL_3_HOST);
		return (RPS_ERROR);
	}

	/* abs_path 
	 * TODO - validation according to grammar */
	if (*scan == '/') {
		while((scan < end ) && (*scan == '/')) scan++;
		scan--;
	}

	abs_path = scan;
	while ((scan < end) && (*scan) && (*scan != '\r') && (*scan != '\n')
		&& (*scan != ' ')) {
		scan++;
	}

	abs_path_len = scan - abs_path;
	while (abs_path_len > 1 && abs_path[abs_path_len-1] == '/'){
		abs_path_len--;
	}

	if (scan > end) {
		RTSP_SET_ERROR(prtsp, RTSP_STATUS_400, NKN_RTSP_PE_URL_LT2);
		return (RPS_NEED_MORE_DATA);
	}

	query = memchr(str, '?', (scan - str));
	if (query) {
		query_len = (scan - str) - (str - query);
	}

	/* Add to mime headers */
	mime_hdr_add_known(&prtsp->hdr, RTSP_HDR_X_HOST, host, host_len);
	mime_hdr_add_known(&prtsp->hdr, RTSP_HDR_X_PORT, port, port_len);
	mime_hdr_add_known(&prtsp->hdr, RTSP_HDR_X_ABS_PATH, abs_path, abs_path_len);
	mime_hdr_add_known(&prtsp->hdr, RTSP_HDR_X_URL, str, (scan - str));
	if (query) {
		mime_hdr_add_known(&prtsp->hdr, RTSP_HDR_X_QUERY, query, query_len);
	}

	if (next_ptr)
		*next_ptr = scan;
	return (RPS_OK);
}

/**
 * @brief Parses request line
 *	    METHOD SP Request-URI SP RTSP-Version CRLF
 *
 * @param prtsp rtsp control block context
 * @param req_str request string pointer (null terminated)
 * @param req_len Length of the request string
 * @param end Return the End of the request string after parsing
 *
 * @return rtsp_parse_status
 */
static rtsp_parse_status_t
rtsp_parse_request_line(rtsp_cb_t * prtsp, const char *req_str,
			int req_len, const char **end)
{
	rtsp_parse_status_t parse_status = RPS_OK;
	assert(prtsp && req_str);

	if (req_len < 4){
		RTSP_SET_ERROR(prtsp, RTSP_STATUS_400, NKN_RTSP_PE_REQL_LT1);
		return (RPS_NEED_MORE_DATA);
	}

	const char *scan = req_str;
	const char *scan_end = scan + req_len;
	const char *temp = NULL;

	prtsp->method = METHOD_UNKNOWN;
	switch (*(unsigned int *) req_str) {
	case RTSP_SIG_MTHD_DESCRIBE:
		if (strncmp(req_str, "DESCRIBE ", ST_STRLEN("DESCRIBE ")) != 0)
			goto extension_method;
		prtsp->method = METHOD_DESCRIBE;
		scan += ST_STRLEN("DESCRIBE");
		break;
	case RTSP_SIG_MTHD_ANNOUNCE:
		if (strncmp(req_str, "ANNOUNCE ", ST_STRLEN("ANNOUNCE ")) != 0)
			goto extension_method;
		prtsp->method = METHOD_ANNOUNCE;
		scan += ST_STRLEN("ANNOUNCE");
		break;
	case RTSP_SIG_MTHD_GET_PARAM:
		if (strncmp(req_str, "GET_PARAMETER ", ST_STRLEN("GET_PARAMETER ")) != 0)
			goto extension_method;
		prtsp->method = METHOD_GET_PARAMETER;
		scan += ST_STRLEN("GET_PARAMETER");
		break;
	case RTSP_SIG_MTHD_OPTIONS:
		if (strncmp(req_str, "OPTIONS ", ST_STRLEN("OPTIONS ")) != 0)
			goto extension_method;
		prtsp->method = METHOD_OPTIONS;
		scan += ST_STRLEN("OPTIONS");
		break;
	case RTSP_SIG_MTHD_PAUSE:
		if (strncmp(req_str, "PAUSE ", ST_STRLEN("PAUSE ")) != 0)
			goto extension_method;
		prtsp->method = METHOD_PAUSE;
		scan += ST_STRLEN("PAUSE");
		break;
	case RTSP_SIG_MTHD_PLAY:
		if (strncmp(req_str, "PLAY ", ST_STRLEN("PLAY ")) != 0)
			goto extension_method;
		prtsp->method = METHOD_PLAY;
		scan += ST_STRLEN("PLAY");
		break;
	case RTSP_SIG_MTHD_RECORD:
		if (strncmp(req_str, "RECORD ", ST_STRLEN("RECORD ")) != 0)
			goto extension_method;
		prtsp->method = METHOD_RECORD;
		scan += ST_STRLEN("RECORD");
		break;
	case RTSP_SIG_MTHD_REDIRECT:
		if (strncmp(req_str, "REDIRECT ", ST_STRLEN("REDIRECT ")) != 0)
			goto extension_method;
		prtsp->method = METHOD_REDIRECT;
		scan += ST_STRLEN("REDIRECT");
		break;
	case RTSP_SIG_MTHD_SETUP:
		if (strncmp(req_str, "SETUP ", ST_STRLEN("SETUP ")) != 0)
			goto extension_method;
		prtsp->method = METHOD_SETUP;
		scan += ST_STRLEN("SETUP");
		break;
	case RTSP_SIG_MTHD_SET_PARAM:
		if (strncmp(req_str, "SET_PARAMETER ", ST_STRLEN("SET_PARAMETER ")) != 0)
			goto extension_method;
		prtsp->method = METHOD_SET_PARAM;
		scan += ST_STRLEN("SET_PARAMETER");
		break;
	case RTSP_SIG_MTHD_TEARDOWN:
		if (strncmp(req_str, "TEARDOWN ", ST_STRLEN("TEARDOWN ")) != 0)
			goto extension_method;
		prtsp->method = METHOD_TEARDOWN;
		scan += ST_STRLEN("TEARDOWN");
		break;
	case RTSP_SIG_MTHD_GET:
		if(strncmp(req_str, "GET ", ST_STRLEN("GET ")) != 0)
			goto extension_method;
		prtsp->method = METHOD_BAD;//unsupported for the moment
		scan += ST_STRLEN("GET ");
		break;
	default:
	extension_method:
		temp = nkn_skip_token(scan, ' ');
		if (temp == NULL) {
			prtsp->method = METHOD_BAD;
			break;
		}
		prtsp->method = METHOD_UNKNOWN;
		scan = temp;
		break;
	}

	/* Invalid format of request line hence returning failure */
	if (*scan != ' ' || prtsp->method == METHOD_BAD) {
		prtsp->method = METHOD_BAD;
		RTSP_SET_ERROR(prtsp, RTSP_STATUS_400, NKN_RTSP_PE_REQL_1_METHOD);
		return RPS_ERROR;
	}

	/*
	 * Add method to header
	 */
	mime_hdr_add_known(&prtsp->hdr, RTSP_HDR_X_METHOD, req_str, scan - req_str);

	/* Skip SP */
	scan = nkn_skip_SP(scan);

	if (scan > scan_end){
		RTSP_SET_ERROR(prtsp, RTSP_STATUS_400, NKN_RTSP_PE_REQL_LT2);
		return (RPS_NEED_MORE_DATA);
	}

	/* Parse RTSP URL 
	 * URLs will be parsed and added to mime HDRs*/
	parse_status = rtsp_parse_url(prtsp, scan, scan_end - scan, (const char **) &scan);
	if (RPS_OK != parse_status) {
		return parse_status;
	}

	/* Skip SP */
	scan = nkn_skip_SP(scan);

	if (scan > scan_end){
		RTSP_SET_ERROR(prtsp, RTSP_STATUS_400, NKN_RTSP_PE_REQL_LT3);
		return (RPS_NEED_MORE_DATA);
	}

	/* Parse Version string */
	parse_status =
	rtsp_parse_version(prtsp, scan, scan_end - scan, &scan);

	if (RPS_OK != parse_status) {
		return (parse_status);
	}

	if (end) {
		*end = scan;
	}
	return RPS_OK;
}

#define NUM(num_char) ((int)(num_char - '0'))
#define RTSP_STATUS_CODE(x) ((isdigit(x[0]) && isdigit(x[1]) && isdigit(x[2]) ) ? \
			     ((NUM(x[0])*100) + (NUM(x[1])*10) + (NUM(x[2]))) : -1)

static rtsp_parse_status_t
rtsp_parse_status_line(rtsp_cb_t * prtsp, const char *resp_str,
		       int resp_len, const char **end)
{
    rtsp_parse_status_t parse_status = RPS_OK;
    assert(prtsp && resp_str);

    if (resp_len < (int) RTSP_V_STR_LEN)
	return (RPS_NEED_MORE_DATA);

    const char *scan = resp_str;
    const char *scan_end = scan + resp_len;

    parse_status = rtsp_parse_version(prtsp, scan, resp_len, &scan);

    if (RPS_OK != parse_status) {
	return (parse_status);
    }

    scan = nkn_skip_SP(scan);

    if (scan > scan_end)
	return (RPS_NEED_MORE_DATA);

    /* Status-Code */
    if ((scan + RTSP_STATUS_CODE_LEN) <= scan_end) {
	prtsp->status = (int) RTSP_STATUS_CODE(scan);
	if (prtsp->status <= 0)
	    return (RPS_ERROR);
    } else {
	return (RPS_NEED_MORE_DATA);
    }

    /* Add Status Code */
    mime_hdr_add_known(&prtsp->hdr, RTSP_HDR_X_STATUS_CODE,
		       scan, RTSP_STATUS_CODE_LEN);
    scan += RTSP_STATUS_CODE_LEN;

    scan = nkn_skip_SP(scan);
    if (scan > scan_end)
	return (RPS_NEED_MORE_DATA);

    /* Find End of Reason Phrase (not validating) */
    scan_end = scan;
    while(is_text(*scan_end)) {
	scan_end++;
    }

    /* Add Reason Phrase */
    mime_hdr_add_known(&prtsp->hdr, RTSP_HDR_X_REASON, scan,
		       (scan_end - scan));

    if (end) {
	*end = scan_end;
    }
    return (RPS_OK);
}

/**
 * @brief Match 'Header:' and give length
 *
 * @param token RTSP Header Token id
 * @param hdr_str Header String
 * @param hdr_len Header Length
 *
 * @return length to be skipped if match found
 *	   else 0 is returned
 */
static inline int
rtsp_match_get_hdr_len(rtsp_header_id_t token,
		       const char *hdr_str, int hdr_len)
{
	mime_header_descriptor_t *rtsp_knownhdr = rtsp_known_headers;
	const char *scan = hdr_str;
	assert(hdr_str);
	assert(token < RTSP_HDR_MAX_DEFS);
	assert(rtsp_knownhdr && ((rtsp_header_id_t) rtsp_knownhdr[token].id == token));

	if (hdr_len < rtsp_knownhdr[token].namelen)
		return 0;

	if (strncasecmp(scan, rtsp_knownhdr[token].name, rtsp_knownhdr[token].namelen) != 0)
		return 0;
	scan += rtsp_knownhdr[token].namelen;

	/* Check for Colon */
	if (*scan != ':') {
		return 0;
	}

	scan++;
	return ((int) (scan - hdr_str));
}

/** 
 * @brief Local Macro that is used by rtsp_add_header function
 * 
 * @param token_in - Token to be compared against
 * @param hdr_str_in - Input header String (char pointer)
 * @param hdr_len_in - Length of the header string
 * @param token_out - Variable that holds the Match token
 * @param skip_len_out - Variable that holds the length to skip
 * 
 * @return NONE
 */
#define MATCH_HDR(token_in, hdr_str_in, hdr_len_in, token_out, skip_len_out ) \
{\
    skip_len_out = rtsp_match_get_hdr_len(token_in, hdr_str_in, hdr_len_in);\
    if (skip_len_out > 0){\
	token_out = token_in;\
	RTSP_FLAG_SET_HDR(prtsp->flag, token_in);\
	break;\
    }\
}

/**
 * @brief Adds a header line to the mime header heap
 * Prerequisite
 *	hdr_str should be only one header string
 *	hdr_len should not include \r\n and the allocated mem 
 *	should be hdr_len+1
 *
 * @param prtsp
 * @param hdr_str
 * @param hdr_len
 * @param end
 *
 * @return
 */
static rtsp_parse_status_t
rtsp_add_header(rtsp_cb_t * prtsp, const char *hdr_str, int hdr_len)
{
	assert(prtsp);

	const char *scan = hdr_str;
	const char *scan_end = scan + hdr_len;

	int skip_len = 0;
	rtsp_header_id_t hdr_token = RTSP_HDR_MAX_DEFS;
	const char *unknown_header = NULL;
	rtsp_parse_status_t parse_status = RPS_ERROR;

	switch (*(unsigned int *) hdr_str | 0x20202020) {
		case RTSP_SIG_HDR_cseq:
			MATCH_HDR(RTSP_HDR_CSEQ, hdr_str, hdr_len, hdr_token, skip_len);
			goto extension_header;
		case RTSP_SIG_HDR_acce:
			MATCH_HDR(RTSP_HDR_ACCEPT, hdr_str, hdr_len, hdr_token, skip_len);
			MATCH_HDR(RTSP_HDR_ACCEPT_ENCODING, hdr_str, hdr_len, hdr_token, skip_len);
			MATCH_HDR(RTSP_HDR_ACCEPT_LANGUAGE, hdr_str, hdr_len, hdr_token, skip_len);
			goto extension_header;
		case RTSP_SIG_HDR_allo:
			MATCH_HDR(RTSP_HDR_ALLOW, hdr_str, hdr_len, hdr_token, skip_len);
			goto extension_header;
		case RTSP_SIG_HDR_auth:
			MATCH_HDR(RTSP_HDR_AUTHORIZATION, hdr_str, hdr_len, hdr_token, skip_len);
			goto extension_header;
		case RTSP_SIG_HDR_band:
			MATCH_HDR(RTSP_HDR_BANDWIDTH, hdr_str, hdr_len, hdr_token, skip_len);
			goto extension_header;
		case RTSP_SIG_HDR_bloc:
			MATCH_HDR(RTSP_HDR_BLOCKSIZE, hdr_str, hdr_len, hdr_token, skip_len);
			goto extension_header;
		case RTSP_SIG_HDR_cach:
			MATCH_HDR(RTSP_HDR_CACHE_CONTROL, hdr_str, hdr_len, hdr_token, skip_len);
			goto extension_header;
		case RTSP_SIG_HDR_conf:
			MATCH_HDR(RTSP_HDR_CONFERENCE, hdr_str, hdr_len, hdr_token, skip_len);
			goto extension_header;
		case RTSP_SIG_HDR_conn:
			MATCH_HDR(RTSP_HDR_CONNECTION, hdr_str, hdr_len, hdr_token, skip_len);
			goto extension_header;
		case RTSP_SIG_HDR_cont:
			MATCH_HDR(RTSP_HDR_CONTENT_BASE, hdr_str, hdr_len, hdr_token, skip_len);
			MATCH_HDR(RTSP_HDR_CONTENT_ENCODING, hdr_str, hdr_len, hdr_token, skip_len);
			MATCH_HDR(RTSP_HDR_CONTENT_LANGUAGE, hdr_str, hdr_len, hdr_token, skip_len);
			MATCH_HDR(RTSP_HDR_CONTENT_LENGTH, hdr_str, hdr_len, hdr_token, skip_len);
			MATCH_HDR(RTSP_HDR_CONTENT_LOCATION, hdr_str, hdr_len, hdr_token, skip_len);
			MATCH_HDR(RTSP_HDR_CONTENT_TYPE, hdr_str, hdr_len, hdr_token, skip_len);
			goto extension_header;
		case RTSP_SIG_HDR_date:
			MATCH_HDR(RTSP_HDR_DATE, hdr_str, hdr_len, hdr_token, skip_len);
			goto extension_header;
		case RTSP_SIG_HDR_expi:
			MATCH_HDR(RTSP_HDR_EXPIRES, hdr_str, hdr_len, hdr_token, skip_len);
			goto extension_header;
		case RTSP_SIG_HDR_from:
			MATCH_HDR(RTSP_HDR_FROM, hdr_str, hdr_len, hdr_token, skip_len);
			goto extension_header;
		case RTSP_SIG_HDR_if_m:
			MATCH_HDR(RTSP_HDR_IF_MODIFIED_SINCE, hdr_str, hdr_len, hdr_token, skip_len);
			goto extension_header;
		case RTSP_SIG_HDR_last:
			MATCH_HDR(RTSP_HDR_LAST_MODIFIED, hdr_str, hdr_len, hdr_token, skip_len);
			goto extension_header;
		case RTSP_SIG_HDR_prox:
			MATCH_HDR(RTSP_HDR_PROXY_AUTHENTICATE, hdr_str, hdr_len, hdr_token, skip_len);
			MATCH_HDR(RTSP_HDR_PROXY_REQUIRE, hdr_str, hdr_len, hdr_token, skip_len);
			goto extension_header;
		case RTSP_SIG_HDR_publ:
			MATCH_HDR(RTSP_HDR_PUBLIC, hdr_str, hdr_len, hdr_token, skip_len);
			goto extension_header;
		case RTSP_SIG_HDR_rang:
			MATCH_HDR(RTSP_HDR_RANGE, hdr_str, hdr_len, hdr_token, skip_len);
			goto extension_header;
		case RTSP_SIG_HDR_refe:
			MATCH_HDR(RTSP_HDR_REFERER, hdr_str, hdr_len, hdr_token, skip_len);
			goto extension_header;
		case RTSP_SIG_HDR_requ:
			MATCH_HDR(RTSP_HDR_REQUIRE, hdr_str, hdr_len, hdr_token, skip_len);
			goto extension_header;
		case RTSP_SIG_HDR_retr:
			MATCH_HDR(RTSP_HDR_RETRY_AFTER, hdr_str, hdr_len, hdr_token, skip_len);
			goto extension_header;
		case RTSP_SIG_HDR_rtp_:
			MATCH_HDR(RTSP_HDR_RTP_INFO, hdr_str, hdr_len, hdr_token, skip_len);
			goto extension_header;
		case RTSP_SIG_HDR_scal:
			MATCH_HDR(RTSP_HDR_SCALE, hdr_str, hdr_len, hdr_token, skip_len);
			goto extension_header;
		case RTSP_SIG_HDR_sess:
			MATCH_HDR(RTSP_HDR_SESSION, hdr_str, hdr_len, hdr_token, skip_len);
			goto extension_header;
		case RTSP_SIG_HDR_serv:
			MATCH_HDR(RTSP_HDR_SERVER, hdr_str, hdr_len, hdr_token, skip_len);
			goto extension_header;
		case RTSP_SIG_HDR_spee:
			MATCH_HDR(RTSP_HDR_SPEED, hdr_str, hdr_len, hdr_token, skip_len);
			goto extension_header;
		case RTSP_SIG_HDR_tran:
			MATCH_HDR(RTSP_HDR_TRANSPORT, hdr_str, hdr_len, hdr_token, skip_len);
			goto extension_header;
		case RTSP_SIG_HDR_unsu:
			MATCH_HDR(RTSP_HDR_UNSUPPORTED, hdr_str, hdr_len, hdr_token, skip_len);
			goto extension_header;
		case RTSP_SIG_HDR_user:
			MATCH_HDR(RTSP_HDR_USER_AGENT, hdr_str, hdr_len, hdr_token, skip_len);
			goto extension_header;
		case RTSP_SIG_HDR_via_:
			MATCH_HDR(RTSP_HDR_VIA, hdr_str, hdr_len, hdr_token, skip_len);
			goto extension_header;
		case RTSP_SIG_HDR_www_:
			MATCH_HDR(RTSP_HDR_WWW_AUTHENTICATE, hdr_str, hdr_len, hdr_token, skip_len);
			goto extension_header;
		case RTSP_SIG_HDR_x_pl:
			MATCH_HDR(RTSP_HDR_X_PLAY_NOW, hdr_str, hdr_len, hdr_token, skip_len);
			goto extension_header;
		case RTSP_SIG_HDR_late_tolerance:
			MATCH_HDR(RTSP_HDR_X_QT_LATE_TOLERANCE, hdr_str, hdr_len, hdr_token, skip_len);
			goto extension_header;
		default:
			/* Add as unknown header and return
			 * RFC 2326 R[8.1]
			 * extension-header = message-header
			 * message-header     =      field-name ":" [ field-value ] CRLF
			 * field-name         =      token
			 * field-value        =      *( field-content | LWS )
			 */
extension_header:
			{
				unknown_header = scan;
				scan = nkn_skip_token(scan, ':');
				if (scan == NULL) {
					/* Invalid token */
					/* Restore Scan */
					scan = unknown_header;
					break;
				}
				scan++;
				skip_len = scan - unknown_header;
				/* Restore Scan
				* Incremented later in general logic */
				scan = unknown_header;
			}
	}

	if (skip_len <= 0) {
		/* No valid headers */
		RTSP_SET_ERROR(prtsp, RTSP_STATUS_400, NKN_RTSP_PE_HDR_1);
		parse_status = RPS_ERROR;
		goto exit;
	}

	scan += skip_len;

	/* Skip Spaces */
	scan = nkn_skip_space(scan);

	/* Add Headers */
	if (RTSP_HDR_MAX_DEFS == hdr_token) {
		/* Unknown Header - excluding colon for unknown header */
		mime_hdr_add_unknown(&prtsp->hdr, unknown_header, skip_len - 1,
		scan, (scan_end - scan));
	} 
	else {
		/* Known Header */
		mime_hdr_add_known(&prtsp->hdr, hdr_token,
		scan, (scan_end - scan));
	}
	parse_status = RPS_OK;
exit:
	return parse_status;
}

/**
 * @brief Parses RTSP Request
 *
 * @param prtsp - Control block context
 *
 * @return Parse Status
 */
rtsp_parse_status_t rtsp_parse_request(rtsp_cb_t * prtsp)
{

	rtsp_parse_status_t parse_status = RPS_ERROR;
	const char *scan = prtsp->cb_reqbuf;
	const char *hdr_end = NULL;
	int scan_len = 0;
	const char *scan_next;
	int rtsp_hdr_len;
	/* Character used to swap and 
	* terminate string with Null */
	char swap_ch = 0;
	char req_dest_ip_str[64];
	struct sockaddr_in dip;
	socklen_t dlen = sizeof(dip);
	int ret;
	uint16_t pkt_len;

	/*
	 * Mark end of HTTP request
	 */
	prtsp->cb_reqbuf[prtsp->cb_reqoffsetlen + prtsp->cb_reqlen] = 0;

	/*
	 * search for the end of RTSP request
	 * \r\n\r\n could be in the middle of RTSP
	 * request when content-length exists
	 */

	scan = prtsp->cb_reqbuf + prtsp->cb_reqoffsetlen;
	scan_len = prtsp->cb_reqlen;

	/* Check is TCP inlined data. If so, ignore the data for now.
	 * If enough data is not avialable return RPS_NEED_MORE_DATA.
	 */
	while (scan_len && *scan == '$') {
		if (scan_len < 4)
			return RPS_NEED_MORE_DATA;
		pkt_len = ntohs(*((uint16_t *)(scan + 2)));
		if (prtsp->cb_reqlen >= (pkt_len + 4)) {
			prtsp->cb_reqoffsetlen += (pkt_len + 4);
			prtsp->cb_reqlen -= (pkt_len + 4);
			scan = prtsp->cb_reqbuf + prtsp->cb_reqoffsetlen;
			scan_len = prtsp->cb_reqlen;
			if (scan_len == 0) {
				prtsp->cb_reqoffsetlen = 0;
				return RPS_NEED_MORE_DATA;
			}
		}
		else {
			return RPS_NEED_MORE_DATA;
		}
	}
	
	if (scan_len < 4) {
		while (*scan && *scan != '\r' && *scan != '\n'){
			scan++;
		}
		if (*scan == '\r' || *scan == '\n'){
			RTSP_SET_ERROR(prtsp, RTSP_STATUS_400, NKN_RTSP_PE_REQ_1);
			prtsp->status = 400;
			return RPS_ERROR;
		}
		return RPS_NEED_MORE_DATA;
	}

	DBG_LOG(MSG, MOD_RTSP, "Recieved request (fd=%d, len=%d, off=%d):\n<%s>", 
			prtsp->fd, 
			prtsp->cb_reqlen,
			prtsp->cb_reqoffsetlen,
			prtsp->cb_reqbuf + prtsp->cb_reqoffsetlen);

	while (scan_len >= 4) {
		if (*scan == '\r' && *(scan+1) == '\n' && *(scan+2) == '\r' && *(scan+3) == '\n') {
			goto complete;
		}
		scan_len--;
		scan++;
	}
	return RPS_NEED_MORE_DATA;

complete:
	glob_rtsp_tot_req_parsed++;

	rtsp_hdr_len = scan + 4 - (prtsp->cb_reqbuf + prtsp->cb_reqoffsetlen);
	hdr_end = scan + rtsp_hdr_len;
	prtsp->rtsp_hdr_len = rtsp_hdr_len;

	scan = prtsp->cb_reqbuf + prtsp->cb_reqoffsetlen;

	/* Initializing context Data
	 * TODO move init out of this code
	 */
	rtsp_init_buf(prtsp);
	mime_hdr_init(&prtsp->hdr, MIME_PROT_RTSP, prtsp->cb_reqbuf + prtsp->cb_reqoffsetlen, prtsp->cb_reqlen);

	/* Method, Uri, RTSP Version validated here */
	parse_status = rtsp_parse_request_line(prtsp, scan, rtsp_hdr_len, &scan);

	/* check for pipelined requests */
	if (prtsp->cb_reqlen > rtsp_hdr_len) {
		prtsp->cb_reqoffsetlen += rtsp_hdr_len;
		prtsp->cb_reqlen -= rtsp_hdr_len;
	}
	else {
		prtsp->cb_reqoffsetlen = 0;
		prtsp->cb_reqlen = 0;
	}

	/* Grab the destination IP here - we dont know yet 
	 * what the origin server will turn out to be. 
	 */
	memset(&dip, 0, dlen);
	if (getsockname(prtsp->fd, &dip, &dlen) == -1) {
	    /* Error!! Dont do anything for now. This will 
	     * fail when we try to lookup the origin
	     * from the namespace
	     */
	}

	/* If destination IP is our interface IP itself */
	if (dip.sin_addr.s_addr == prtsp->pns->if_addrv4) {
		/* Log an error ?? */
	}
	ret = snprintf(req_dest_ip_str, sizeof(req_dest_ip_str), "%s",
			inet_ntoa(dip.sin_addr));
	mime_hdr_add_known(&prtsp->hdr, RTSP_HDR_X_NKN_REQ_REAL_DEST_IP,
			req_dest_ip_str, ret);


	while (parse_status == RPS_OK) {
		scan = nkn_skip_to_nextline(scan);
		if (scan == NULL)
			break;

		if (*scan == '\n') {
			scan++;
			break;
		}

		if (*scan == '\r' || *(scan + 1) == '\n') {
			scan += 2;
			break;
		}

		rtsp_hdr_len = 0;
		scan_next = scan;

		while (*scan_next != '\r' && *scan_next != '\n') {
			rtsp_hdr_len++;
			scan_next++;
		}

		swap_ch = *scan_next;
		*(char *) scan_next = 0;
		parse_status = rtsp_add_header(prtsp, scan, rtsp_hdr_len);
		*(char *) scan_next = swap_ch;
	}

	/* To provide backward compatibility for old code */
	if (RPS_OK == parse_status)
		parse_status = rtsp_parse_bkwd_compat(prtsp);

	return parse_status;
}


/**
 * @brief Parses RTSP Request from server
 *
 * @param prtsp - Control block context
 *
 * @return Parse Status
 */
rtsp_parse_status_t rtsp_parse_origin_request(rtsp_cb_t * prtsp)
{

	rtsp_parse_status_t parse_status = RPS_ERROR;
	const char *scan = NULL;
	const char *hdr_end = NULL;
	int scan_len = 0;
	const char *scan_next;
	int rtsp_hdr_len;
	/* Character used to swap and 
	* terminate string with Null */
	char swap_ch = 0;

	/*
	 * Mark end of HTTP request
	 */
	prtsp->cb_respbuf[prtsp->cb_respoffsetlen + prtsp->cb_resplen] = 0;

	/*
	 * search for the end of RTSP request
	 * \r\n\r\n could be in the middle of RTSP
	 * request when content-length exists
	 */

	scan = prtsp->cb_respbuf + prtsp->cb_respoffsetlen;
	scan_len = prtsp->cb_resplen;

	if (scan_len < 4) {
		while (*scan && *scan != '\r' && *scan != '\n'){
			scan++;
		}
		if (*scan == '\r' || *scan == '\n'){
			RTSP_SET_ERROR(prtsp, RTSP_STATUS_400, NKN_RTSP_PE_REQ_1);
			return RPS_ERROR;
		}
		return RPS_NEED_MORE_DATA;
	}

	DBG_LOG(MSG, MOD_RTSP, "Recieved Server request (fd=%d, len=%d, off=%d):\n<%s>", 
			prtsp->fd, 
			prtsp->cb_resplen,
			prtsp->cb_respoffsetlen,
			prtsp->cb_respbuf + prtsp->cb_respoffsetlen);

	while (scan_len >= 4) {
		if (*scan == '\r' && *(scan+1) == '\n' && *(scan+2) == '\r' && *(scan+3) == '\n') {
			goto complete;
		}
		scan_len--;
		scan++;
	}
	return RPS_NEED_MORE_DATA;

complete:
	glob_rtsp_tot_req_parsed++;

	rtsp_hdr_len = scan + 4 - (prtsp->cb_respbuf + prtsp->cb_respoffsetlen);
	hdr_end = scan + rtsp_hdr_len;
	prtsp->rtsp_hdr_len = rtsp_hdr_len;

	scan = prtsp->cb_respbuf + prtsp->cb_respoffsetlen;

	/* Initializing context Data
	 * TODO move init out of this code
	 */
	rtsp_init_buf(prtsp);
	mime_hdr_init(&prtsp->hdr, MIME_PROT_RTSP, prtsp->cb_respbuf + prtsp->cb_respoffsetlen, prtsp->cb_resplen);

	/* Method, Uri, RTSP Version validated here */
	parse_status = rtsp_parse_request_line(prtsp, scan, rtsp_hdr_len, &scan);

	/* check for pipelined requests */
	if (prtsp->cb_resplen > rtsp_hdr_len) {
		prtsp->cb_respoffsetlen += rtsp_hdr_len;
		prtsp->cb_resplen -= rtsp_hdr_len;
	}
	else {
		prtsp->cb_respoffsetlen = 0;
		prtsp->cb_resplen = 0;
	}

	while (parse_status == RPS_OK) {
		scan = nkn_skip_to_nextline(scan);
		if (scan == NULL)
			break;

		if (*scan == '\n') {
			scan++;
			break;
		}

		if (*scan == '\r' || *(scan + 1) == '\n') {
			scan += 2;
			break;
		}

		rtsp_hdr_len = 0;
		scan_next = scan;

		while (*scan_next != '\r' && *scan_next != '\n') {
			rtsp_hdr_len++;
			scan_next++;
		}

		swap_ch = *scan_next;
		*(char *) scan_next = 0;
		parse_status = rtsp_add_header(prtsp, scan, rtsp_hdr_len);
		*(char *) scan_next = swap_ch;
	}

	/* To provide backward compatibility for old code */
	if (RPS_OK == parse_status) {
		parse_status = rtsp_parse_origin_bkwd_compat(prtsp);
	}

	return parse_status;
}


/**
 * @brief Parses RTSP Response
 *
 * @param prtsp - Control block context
 *
 * @return Parse Status
 */
rtsp_parse_status_t rtsp_parse_response(rtsp_cb_t * prtsp)
{
	rtsp_parse_status_t parse_status = RPS_ERROR;

	const char *scan = prtsp->cb_respbuf;
	const char *hdr_end = NULL;
	int scan_len = 0;
	const char *scan_next;
	int rtsp_hdr_len;
	/* Character used to swap and 
	* terminate string with Null */
	char swap_ch = 0;
	int method;

	if (prtsp->cb_resplen < 4) {
		return RPS_NEED_MORE_DATA;
	}

	/*
	** Mark end of HTTP request
	*/
	prtsp->cb_respbuf[prtsp->cb_respoffsetlen + prtsp->cb_resplen] = 0;

	/* There could be a case where we are using TCP for 
	** transmitting RTP/RTCP data . Need to check for that case
	** over here 
	*/

	if (prtsp->cb_respbuf[prtsp->cb_respoffsetlen] == RTSP_RTP_DELIMITER) {
		//int result = 0;
		//result = rtsp_parse_rtp_data(prtsp);
		return RPS_INLINE_DATA;
	}

	DBG_LOG(MSG, MOD_RTSP, "Recieved response (fd=%d, len=%d, off=%d):\n<%s>", 
			prtsp->fd,
			prtsp->cb_resplen,
			prtsp->cb_respoffsetlen,
			prtsp->cb_respbuf + prtsp->cb_respoffsetlen);

	/*
	 * search for the start and end of RTSP response
	 * \r\n\r\n could be in the middle of RTSP
	 * response buffer in case of RTP/TCP.
	 */

	scan = prtsp->cb_respbuf + prtsp->cb_respoffsetlen;
	scan_len = prtsp->cb_resplen;

	while (scan_len >= 4) {
		if (*scan == 'R' && *(scan + 1) == 'T' && 
			*(scan + 2) == 'S' && *(scan + 3) == 'P') {
			goto scan_end;
		}
		scan_len--;
		scan++;
	}
	return RPS_NEED_MORE_DATA;

	prtsp->cb_respoffsetlen += prtsp->cb_resplen - scan_len;
	prtsp->cb_resplen = scan_len;

  scan_end:
	while (scan_len >= 4) {
		if (*scan == '\r' && *(scan + 1) == '\n' && 
			*(scan + 2) == '\r' && *(scan + 3) == '\n') {
			goto complete;
		}
		scan_len--;
		scan++;
	}
	return RPS_NEED_MORE_DATA;

  complete:
	glob_rtsp_tot_res_parsed++;

	rtsp_hdr_len = scan + 4 - &prtsp->cb_respbuf[prtsp->cb_respoffsetlen];
	hdr_end = scan + rtsp_hdr_len;

	scan = prtsp->cb_respbuf + prtsp->cb_respoffsetlen;

	/* Initializing context Data
	* TODO move init out of this code
	*/
	method = prtsp->method;
	rtsp_init_buf(prtsp);
	prtsp->method = method;
	mime_hdr_init(&prtsp->hdr, MIME_PROT_RTSP,
		  prtsp->cb_respbuf + prtsp->cb_respoffsetlen, prtsp->cb_resplen);

	/* RTSP-Version, Status-Code Reason Phrase extrated here */
	parse_status = rtsp_parse_status_line(prtsp, scan, rtsp_hdr_len, &scan);

	while (parse_status == RPS_OK) {
		scan = nkn_skip_to_nextline(scan);
		if (scan == NULL)
			break;
		if (*scan == '\n') {
			scan++;
			break;
		}
		if (*scan == '\r' || *(scan + 1) == '\n') {
			scan += 2;
			break;
		}
		rtsp_hdr_len = 0;
		scan_next = scan;

		while (*scan_next != '\r' && *scan_next != '\n') {
			rtsp_hdr_len++;
			scan_next++;
		}

		swap_ch = *scan_next;
		*(char *) scan_next = 0;
		parse_status = rtsp_add_header(prtsp, scan, rtsp_hdr_len);
		*(char *) scan_next = swap_ch;
	}

	scan_len = scan - &prtsp->cb_respbuf[prtsp->cb_respoffsetlen];
	prtsp->cb_respoffsetlen += scan_len;
	prtsp->cb_resplen -= scan_len;

	/* To provide backward compatibility for old code */
	if (RPS_OK == parse_status)
		parse_status = rtsp_parse_bkwd_compat_response(prtsp);

	if (parse_status != RPS_NEED_CONTENT && prtsp->cb_resplen == 0) {
		prtsp->cb_respoffsetlen = 0;
	}

	return parse_status;
}

#if 1
rtsp_parse_status_t rtsp_parse_rtp_data(rtsp_cb_t *prtsp)
{
	char *p;
	int rtp_data_len;

	p = prtsp->cb_respbuf + prtsp->cb_respoffsetlen;

	/* Check if some data is pending for last packet
	 */
	if (prtsp->rtp_data.required_len) {
		/* Do we have enough data
		 */
		if (prtsp->rtp_data.required_len > prtsp->cb_resplen) {
			/* Available data less than required
			 */
			prtsp->partialRTPDataRcvd = TRUE;
			prtsp->rtp_data.required_len -= prtsp->cb_resplen;

			/* Append partial data to data buffer
			 */
			memcpy(prtsp->rtp_data.data_buf + prtsp->rtp_data.data_len,
				p, 
				prtsp->cb_resplen);
			prtsp->rtp_data.data_len += prtsp->cb_resplen;
#if 0
			printf( "RTP/TCP data1: %c %2d %4d %4d %4d\n", 
				prtsp->rtp_data.data_buf[0], prtsp->rtp_data.channel_id, 
				prtsp->cb_resplen, prtsp->cb_respoffsetlen, 
				prtsp->rtp_data.data_len);
#endif
			prtsp->cb_resplen = 0;
			prtsp->cb_respoffsetlen = 0;
			prtsp->rtp_data.rtp_buf_len = 0;
			return RPS_NEED_MORE_DATA;
		}
		else {
			memcpy(prtsp->rtp_data.data_buf + prtsp->rtp_data.data_len,
				p, 
				prtsp->rtp_data.required_len);
			prtsp->rtp_data.data_len += prtsp->rtp_data.required_len;
#if 0
			printf( "RTP/TCP data2: %c %2d %4d %4d %4d\n", 
				prtsp->rtp_data.data_buf[0], prtsp->rtp_data.channel_id, 
				prtsp->cb_resplen, prtsp->cb_respoffsetlen, 
				prtsp->rtp_data.data_len);
#endif
			prtsp->cb_resplen           -= prtsp->rtp_data.required_len;
			prtsp->cb_respoffsetlen     += prtsp->rtp_data.required_len;
			prtsp->partialRTPDataRcvd    = FALSE;
			prtsp->rtp_data.rtp_buf      = prtsp->rtp_data.data_buf;
			prtsp->rtp_data.rtp_buf_len  = prtsp->rtp_data.data_len;
			prtsp->rtp_data.required_len = 0;

			return RPS_NEED_MORE_DATA;
		}
	}

	/* Parse one packet
	 */
	if (prtsp->cb_resplen > 4) {
		/* Check if RTSP Response
		 */
		p = prtsp->cb_respbuf + prtsp->cb_respoffsetlen;
		if (*p == 'R' && *(p + 1) == 'T' && *(p + 2) == 'S' && *(p + 3) == 'P') {
			return RPS_RTSP_RESP_HEADER;
		} else if (*p != '$') {
			return RPS_ERROR;
		}
		prtsp->rtp_data.channel_id = p[1];
		rtp_data_len = htons(*(uint16_t *)(p + 2));
		//if (rtp_data_len > 2048) {
		//	printf( "RT/TCP data: %c %d %d\n", *p, prtsp->rtp_data.channel_id, rtp_data_len );
		//}

		/* Check if enough data is available
		 */
		if (rtp_data_len > (prtsp->cb_resplen - 4)) {
			prtsp->partialRTPDataRcvd = TRUE;
			prtsp->rtp_data.required_len = rtp_data_len - (prtsp->cb_resplen - 4); 
			prtsp->rtp_data.data_len = prtsp->cb_resplen;

			memcpy(prtsp->rtp_data.data_buf,
				p, 
				prtsp->cb_resplen);
#if 0
			printf( "RTP/TCP data3: %c %2d %4d %4d %4d %4d %4d\n", 
				*p, prtsp->rtp_data.channel_id, 
				prtsp->cb_resplen, prtsp->cb_respoffsetlen, 
				prtsp->rtp_data.data_len, rtp_data_len, 
				prtsp->rtp_data.required_len );
#endif
			prtsp->cb_resplen = 0;
			prtsp->cb_respoffsetlen = 0;
			prtsp->rtp_data.rtp_buf_len = 0;
			return RPS_NEED_MORE_DATA;
		}
		else {
			prtsp->rtp_data.rtp_buf_len = rtp_data_len + 4;
			prtsp->rtp_data.rtp_buf = p;
			prtsp->partialRTPDataRcvd = FALSE;
			prtsp->rtp_data.data_len = 0;
			prtsp->rtp_data.required_len = 0;
#if 0
			printf( "RTP/TCP data4: %c %2d %4d %4d %4d %4d\n", 
				*p, prtsp->rtp_data.channel_id,
				prtsp->cb_resplen, prtsp->cb_respoffsetlen,
				prtsp->rtp_data.rtp_buf_len, rtp_data_len );
#endif
			prtsp->cb_respoffsetlen += rtp_data_len + 4;
			prtsp->cb_resplen -= rtp_data_len + 4;
		}
	}

	if (prtsp->cb_resplen == 0) {
		prtsp->cb_respoffsetlen = 0;
	}
	return RPS_NEED_MORE_DATA;
}
#endif

#if 0
rtsp_parse_status_t rtsp_parse_rtp_data(rtsp_cb_t *prtsp)
{
	char *p;
	int rtp_data_len;

	p = prtsp->cb_respbuf + prtsp->cb_respoffsetlen;

	/* Check if some data is pending for last packet
	 */
	if (prtsp->rtp_data.required_len) {
		/* Do we have enough data
		 */
		if (prtsp->rtp_data.required_len > prtsp->cb_resplen) {
			prtsp->partialRTPDataRcvd = TRUE;
			prtsp->rtp_data.required_len -= prtsp->cb_resplen;
			prtsp->rtp_data.rtp_buf_len = prtsp->cb_resplen;
			prtsp->rtp_data.rtp_buf = p;
			printf( "RTP/TCP data1:   %2d %4d %4d %4d\n", 
				prtsp->rtp_data.channel_id, 
				prtsp->cb_resplen, prtsp->cb_respoffsetlen, 
				prtsp->rtp_data.rtp_buf_len);
			prtsp->cb_resplen = 0;
			prtsp->cb_respoffsetlen = 0;
			return RPS_NEED_MORE_DATA;
		}
		else {
			prtsp->rtp_data.rtp_buf_len = prtsp->rtp_data.required_len;
			prtsp->rtp_data.rtp_buf = p;

			printf( "RTP/TCP data2:   %2d %4d %4d %4d\n", 
				prtsp->rtp_data.channel_id, 
				prtsp->cb_resplen, prtsp->cb_respoffsetlen, 
				prtsp->rtp_data.rtp_buf_len);

			prtsp->cb_resplen -= prtsp->rtp_data.required_len;
			prtsp->cb_respoffsetlen += prtsp->rtp_data.required_len;
			prtsp->rtp_data.required_len = 0;
			prtsp->partialRTPDataRcvd = FALSE;
			if (prtsp->cb_resplen == 0) {
				prtsp->cb_respoffsetlen = 0;
			}
			return RPS_NEED_MORE_DATA;
		}
	}
	else {
		prtsp->rtp_data.rtp_buf_len = 0;
		prtsp->rtp_data.required_len = 0;
		prtsp->partialRTPDataRcvd = FALSE;
	}

	/* Repeat while we have data
	 */
	if (prtsp->cb_resplen > 4) {
		/* Check if RTSP Response
		 */
		p = prtsp->cb_respbuf + prtsp->cb_respoffsetlen;
		if (*p == 'R' && *(p + 1) == 'T' && *(p + 2) == 'S' && *(p + 3) == 'P') {
			return RPS_RTSP_HEADER;
		} else if (*p != '$') {
			return RPS_ERROR;
		}
		prtsp->rtp_data.channel_id = p[1];
		rtp_data_len = htons(*(uint16_t *)(p + 2));
		//if (rtp_data_len > 2048) {
		//	printf( "RT/TCP data: %c %d %d\n", *p, prtsp->rtp_data.channel_id, rtp_data_len );
		//}

		/* Check if enough data is available
		 */
		if (rtp_data_len > (prtsp->cb_resplen - 4)) {
			prtsp->partialRTPDataRcvd = TRUE;
			prtsp->rtp_data.required_len = rtp_data_len - (prtsp->cb_resplen - 4); 
			prtsp->rtp_data.rtp_buf_len = prtsp->cb_resplen;
			prtsp->rtp_data.rtp_buf = p;

			printf( "RTP/TCP data3: %c %2d %4d %4d %4d %4d %4d\n", 
				*p, prtsp->rtp_data.channel_id, 
				prtsp->cb_resplen, prtsp->cb_respoffsetlen, 
				prtsp->rtp_data.rtp_buf_len, rtp_data_len, 
				prtsp->rtp_data.required_len );
			prtsp->cb_resplen = 0;
			prtsp->cb_respoffsetlen = 0;
			return RPS_NEED_MORE_DATA;
		}
		else {
			prtsp->rtp_data.rtp_buf_len = rtp_data_len + 4;
			prtsp->rtp_data.rtp_buf = p;
			printf( "RTP/TCP data4: %c %2d %4d %4d %4d %4d\n", 
				*p, prtsp->rtp_data.channel_id,
				prtsp->cb_resplen, prtsp->cb_respoffsetlen,
				prtsp->rtp_data.rtp_buf_len, rtp_data_len );
			prtsp->partialRTPDataRcvd = FALSE;
			prtsp->cb_respoffsetlen += rtp_data_len + 4;
			prtsp->cb_resplen -= rtp_data_len + 4;
		}
	}

	if (prtsp->cb_resplen == 0) {
		prtsp->cb_respoffsetlen = 0;
	}
	return RPS_NEED_MORE_DATA;
}
#endif

#if 0
rtsp_parse_status_t rtsp_parse_rtp_data(rtsp_cb_t *prtsp)
{
	char *p;
	int rtp_data_len;

	prtsp->rtp_data.rtp_buf = prtsp->cb_respbuf + prtsp->cb_respoffsetlen;

	/* Check if some data is pending for last packet
	 */
	if (prtsp->rtp_data.required_len) {
		/* Do we have enough data
		 */
		if (prtsp->rtp_data.required_len > prtsp->cb_resplen) {
			prtsp->partialRTPDataRcvd = TRUE;
			prtsp->rtp_data.required_len -= prtsp->cb_resplen;
			prtsp->rtp_data.rtp_buf_len = prtsp->cb_resplen;
			printf( "RTP/TCP data1:   %2d %4d %4d %4d\n", 
				prtsp->rtp_data.channel_id, 
				prtsp->cb_resplen, prtsp->cb_respoffsetlen, 
				prtsp->rtp_data.rtp_buf_len);
			prtsp->cb_resplen = 0;
			prtsp->cb_respoffsetlen = 0;
			return RPS_NEED_MORE_DATA;
		}
		else {
			prtsp->rtp_data.rtp_buf_len = prtsp->rtp_data.required_len;
			printf( "RTP/TCP data2:   %2d %4d %4d %4d\n", 
				prtsp->rtp_data.channel_id, 
				prtsp->cb_resplen, prtsp->cb_respoffsetlen, 
				prtsp->rtp_data.rtp_buf_len);

			prtsp->cb_resplen -= prtsp->rtp_data.required_len;
			prtsp->cb_respoffsetlen += prtsp->rtp_data.required_len;
			prtsp->rtp_data.required_len = 0;
			prtsp->partialRTPDataRcvd = FALSE;
		}
	}
	else {
		prtsp->rtp_data.rtp_buf_len = 0;
		prtsp->rtp_data.required_len = 0;
		prtsp->partialRTPDataRcvd = FALSE;
	}

	/* Repeat while we have data
	 */
	while (prtsp->cb_resplen > 4) {
		/* Check if RTSP Response
		 */
		p = prtsp->cb_respbuf + prtsp->cb_respoffsetlen;
		if (*p == 'R' && *(p + 1) == 'T' && *(p + 2) == 'S' && *(p + 3) == 'P') {
			return RPS_RTSP_HEADER;
		} else if (*p != '$') {
			return RPS_ERROR;
		}
		prtsp->rtp_data.channel_id = p[1];
		rtp_data_len = htons(*(uint16_t *)(p + 2));
		//if (rtp_data_len > 2048) {
		//	printf( "RT/TCP data: %c %d %d\n", *p, prtsp->rtp_data.channel_id, rtp_data_len );
		//}

		/* Check if enough data is available
		 */
		if (rtp_data_len > (prtsp->cb_resplen - 4)) {
			prtsp->partialRTPDataRcvd = TRUE;
			prtsp->rtp_data.required_len = rtp_data_len - (prtsp->cb_resplen - 4); 
			prtsp->rtp_data.rtp_buf_len += prtsp->cb_resplen;

			printf( "RTP/TCP data3: %c %2d %4d %4d %4d %4d %4d\n", 
				*p, prtsp->rtp_data.channel_id, 
				prtsp->cb_resplen, prtsp->cb_respoffsetlen, 
				prtsp->rtp_data.rtp_buf_len, rtp_data_len, 
				prtsp->rtp_data.required_len );
			prtsp->cb_resplen = 0;
			prtsp->cb_respoffsetlen = 0;
			return RPS_NEED_MORE_DATA;
		}
		else {
			prtsp->rtp_data.rtp_buf_len += rtp_data_len + 4;
			printf( "RTP/TCP data4: %c %2d %4d %4d %4d %4d\n", 
				*p, prtsp->rtp_data.channel_id,
				prtsp->cb_resplen, prtsp->cb_respoffsetlen,
				prtsp->rtp_data.rtp_buf_len, rtp_data_len );
			prtsp->cb_respoffsetlen += rtp_data_len + 4;
			prtsp->cb_resplen -= rtp_data_len + 4;
		}
	}

#define UINT unsigned int
	if (prtsp->cb_resplen) {
		p = prtsp->cb_respbuf + prtsp->cb_respoffsetlen;
		printf( "RTP/TCP data5: %c    %4d %4d [%02x %02x %02x %02x]\n", 
			*p,
			prtsp->cb_resplen, prtsp->cb_respoffsetlen,
			(UINT) *(p), (UINT) *(p+1), (UINT) *(p+2), (UINT) *(p+3));
	}

	if (prtsp->cb_resplen == 0) {
		prtsp->cb_respoffsetlen = 0;
	}
	return RPS_NEED_MORE_DATA;
}
#endif
