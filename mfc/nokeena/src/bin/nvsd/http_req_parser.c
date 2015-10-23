#include <assert.h>
#include <strings.h>

#include "nkn_debug.h"
#include "nkn_stat.h"
#include "nkn_http.h"

#include "parser_utils.h"
#include "http_signature.h"
#include "http_micro_parser.c"




static inline http_parse_status_t
http_parse_url(http_cb_t * phttp, const char *url_buf, int buf_len,
	       const char **end)
{
    UNUSED_ARGUMENT(buf_len);
    UNUSED_ARGUMENT(end);
    // GET /htdocs/location.html HTTP/1.0
    //printf("parse: p=%s\n", p);
    int url_decode_req = 0;
    int len;
    const char *p, *ps;
    const char *p_query = NULL;

    p = url_buf;
    ps = p;
    while (*p != ' ' && *p != '\n') {
	if (*p == '?' && !p_query)
	    p_query = p;
	if (*p == '%')
	    url_decode_req = 1;
	if (*p == 0) {
	    glob_http_parse_err_uri_tolong++;
	    return HPS_ERROR;	// URI is too long
	}
	p++;
    }

    len = p - ps;
    //printf("parse: len=%d\n", len);
    // We make a limitation here to avoid crash
    if (len < 1) {
	glob_http_parse_err_req_toshort++;
	return HPS_ERROR;
    }

    /*
     * Case 1: GET /uri HTTP/1.0
     * Case 2: GET http://www.cnn.com/uri HTTP/1.0
     */
    if (nkn_strcmp_incase(ps, "http://", 7) == 0) {
	const char *phost;
	// forward proxy case
	ps += 7;
	len -= 7;
	phost = ps;
	while (*ps != '/' && *ps != ' ' && *ps != 0) {
	    ps++;
	    len--;
	}
	SET_HTTP_FLAG(phttp, HRF_FORWARD_PROXY);
	mime_hdr_add_known(&phttp->hdr, MIME_HDR_HOST, phost, ps - phost);
    }

    if (ps && *ps && (len > 0)) {
	mime_hdr_add_known(&phttp->hdr, MIME_HDR_X_NKN_URI, ps, len);
    }

    if (p_query && (p - p_query)) {
	SET_HTTP_FLAG(phttp, HRF_HAS_QUERY);
	mime_hdr_add_known(&phttp->hdr, MIME_HDR_X_NKN_QUERY, p_query, 
			   p - p_query);
    }

    if (url_decode_req) {
	char *tbuf;
	int bytesused;
	tbuf = (char *) alloca(len + 1);
	if (!canonicalize_url(ps, len, tbuf, len + 1, &bytesused)) {
	    mime_hdr_add_known(&phttp->hdr, MIME_HDR_X_NKN_DECODED_URI,
			       tbuf, bytesused - 1);
	}
    }

    if (normalize_http_uri(&phttp->hdr)) {
	glob_http_parse_err_normalize_uri++;
	return HPS_ERROR;
    }

    if(end){
	*end = p;
    }
    return HPS_OK;
}

static inline http_parse_status_t
http_parse_authority(http_cb_t * phttp, const char *url_buf, int buf_len,
		     const char **end)
{
    UNUSED_ARGUMENT(buf_len);
    UNUSED_ARGUMENT(end);
    /* TODO - Clean up Hacking code to be compliant */

    const char *p;
    const char *ps;
    int len;
    p = url_buf;
    /* Case 3: CONNECT www.cnn.com:443 HTTP/1.0 */
    ps = p;
    while (*ps != ':') {
	ps++;
	if (*ps == 0) {
	    glob_http_parse_err_badreq_2++;
	    return HPS_ERROR;
	}
    }
    len = ps - p;
    mime_hdr_add_known(&phttp->hdr, MIME_HDR_X_NKN_FP_SVRHOST, p, len);

    ps++;
    p = ps;
    while (*ps != ' ') {
	ps++;
	if (*ps == 0) {
	    glob_http_parse_err_badreq_3++;
	    return HPS_ERROR;
	}
    }
    len = ps - p;
    mime_hdr_add_known(&phttp->hdr, MIME_HDR_X_NKN_FP_SVRPORT, p, len);

    ps++;
    p = ps;
    // Fake one to make other code e.g. namespace lookup happy
    mime_hdr_add_known(&phttp->hdr, MIME_HDR_X_NKN_URI, "/", 1);
    if (end) {
	*end = p;
    }
    return (HPS_OK);
}


static inline http_parse_status_t http_parse_version(http_cb_t * phttp,
				       const char *ver_buf, int buf_len,
				       const char **end)
{
    UNUSED_ARGUMENT(buf_len);
    UNUSED_ARGUMENT(end);
    const char *p;
    // Get information of HTTP/1.0 or HTTP/1.1
    p = ver_buf;
    p = nkn_skip_space(p);
    if (*p == '\n') {
	glob_http_parse_err_badreq_1++;
	return HPS_ERROR;
    }
    if (nkn_strcmp_incase(p, "HTTP/1.0", 8) == 0) {
	SET_HTTP_FLAG(phttp, HRF_HTTP_10);
    } else if (nkn_strcmp_incase(p, "HTTP/1.1", 8) == 0) {
	SET_HTTP_FLAG(phttp, HRF_HTTP_11);
    }
    //printf("http_parse_request: URI = %s, %d\n", phttp->uri, phttp->uri_len);i
    return HPS_OK;
}

/**
 * @brief Match 'Header:' and give length
 *
 * @param token HTTP Header Token id
 * @param hdr_str Header String
 * @param hdr_len Header Length
 *
 * @return length to be skipped if match found
 *	   else 0 is returned
 */
static inline int
http_match_get_hdr_len(http_header_id_t token,
		       const char *hdr_str, int hdr_len)
{
    mime_header_descriptor_t *http_knownhdr = http_known_headers;
    const char *scan = hdr_str;
    assert(hdr_str);
    assert(token < MIME_HDR_MAX_DEFS);
    assert(http_knownhdr
	   && ((http_header_id_t) http_knownhdr[token].id == token));

    if (hdr_len < http_knownhdr[token].namelen)
	return 0;

    if (strncasecmp(scan, http_knownhdr[token].name,
		    http_knownhdr[token].namelen) != 0)
	return 0;
    scan += http_knownhdr[token].namelen;
    /* 
     * We Are including spaces between Header name and ':'
     * This deviation is for general compatibility.
     */
    scan = nkn_skip_space (scan);
    /* Check for Colon */
    if (*scan != ':') {
	return 0;
    }

    scan++;
    return ((int) (scan - hdr_str));
}

/**
 * @brief Local Macro that is used by http_add_header function
 *
 * @param http_ctxt - Http control block context ptr (http_cb_t)
 * @param token_in - Token to be compared against
 * @param hdr_str_in - Input header String (char pointer)
 * @param hdr_len_in - Length of the header string
 * @param token_out - Variable that holds the Match token
 * @param skip_len_out - Variable that holds the length to skip
 * @param end_ptr - const char ** type to indicate the end of the parsed string
 *
 * @return NONE
 */
#define MATCH_HDR(http_ctxt, token_in, hdr_str_in, hdr_len_in, token_out, skip_len_out, end_ptr ) \
{\
    /* \
     * Match Header name with token id\
     */\
    skip_len_out = http_match_get_hdr_len(token_in, hdr_str_in, hdr_len_in);\
    if (skip_len_out > 0){\
	/* \
	 * Call Micro parser function for that specific header\
	 */\
	if(MICRO_PARSER_0(token_in)){\
	    if(HPS_OK == (*MICRO_PARSER_0(token_in))(http_ctxt , token_in, \
						     &hdr_str_in[skip_len_out], \
						     (hdr_len_in-skip_len_out), end_ptr)){\
		token_out = token_in;\
		break;\
	    }\
	} else {\
		token_out = token_in;\
		break;\
	}\
    }\
}


static http_parse_status_t
http_add_header(http_cb_t * phttp, const char *hdr_buf,
		int hdr_len, const char **end)
{
    UNUSED_ARGUMENT(end);
    assert(phttp);

    const char *scan = hdr_buf;
    const char *scan_end = scan + hdr_len;
    /* Micro Parser End */
    const char *mp_end = NULL;

    int skip_len = 0;
    http_header_id_t hdr_token = MIME_HDR_MAX_DEFS;
    const char *unknown_header = NULL;
    http_parse_status_t parse_status = HPS_ERROR;

    switch (*(unsigned int *) scan | 0x20202020) {
    case HTTP_SIG_HDR_acce:
	MATCH_HDR(phttp, MIME_HDR_ACCEPT, scan, hdr_len, hdr_token, skip_len,
		  &mp_end);
	MATCH_HDR(phttp, MIME_HDR_ACCEPT_CHARSET, scan, hdr_len, hdr_token,
		  skip_len, &mp_end);
	MATCH_HDR(phttp, MIME_HDR_ACCEPT_ENCODING, scan, hdr_len, hdr_token,
		  skip_len, &mp_end);
	MATCH_HDR(phttp, MIME_HDR_ACCEPT_LANGUAGE, scan, hdr_len, hdr_token,
		  skip_len, &mp_end);
	MATCH_HDR(phttp, MIME_HDR_ACCEPT_RANGES, scan, hdr_len, hdr_token,
		  skip_len, &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_age_:
	MATCH_HDR(phttp, MIME_HDR_AGE, scan, hdr_len, hdr_token, skip_len,
		  &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_allo:
	MATCH_HDR(phttp, MIME_HDR_ALLOW, scan, hdr_len, hdr_token, skip_len,
		  &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_auth:
	MATCH_HDR(phttp, MIME_HDR_AUTHORIZATION, scan, hdr_len, hdr_token,
		  skip_len, &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_cach:
	MATCH_HDR(phttp, MIME_HDR_CACHE_CONTROL, scan, hdr_len, hdr_token,
		  skip_len, &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_conn:
	MATCH_HDR(phttp, MIME_HDR_CONNECTION, scan, hdr_len, hdr_token,
		  skip_len, &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_cont:
	MATCH_HDR(phttp, MIME_HDR_CONTENT_BASE, scan, hdr_len, hdr_token,
		  skip_len, &mp_end);
	MATCH_HDR(phttp, MIME_HDR_CONTENT_DISPOSITION, scan, hdr_len,
		  hdr_token, skip_len, &mp_end);
	MATCH_HDR(phttp, MIME_HDR_CONTENT_ENCODING, scan, hdr_len, hdr_token,
		  skip_len, &mp_end);
	MATCH_HDR(phttp, MIME_HDR_CONTENT_LANGUAGE, scan, hdr_len, hdr_token,
		  skip_len, &mp_end);
	MATCH_HDR(phttp, MIME_HDR_CONTENT_LENGTH, scan, hdr_len, hdr_token,
		  skip_len, &mp_end);
	MATCH_HDR(phttp, MIME_HDR_CONTENT_LOCATION, scan, hdr_len, hdr_token,
		  skip_len, &mp_end);
	MATCH_HDR(phttp, MIME_HDR_CONTENT_MD5, scan, hdr_len, hdr_token,
		  skip_len, &mp_end);
	MATCH_HDR(phttp, MIME_HDR_CONTENT_RANGE, scan, hdr_len, hdr_token,
		  skip_len, &mp_end);
	MATCH_HDR(phttp, MIME_HDR_CONTENT_TYPE, scan, hdr_len, hdr_token,
		  skip_len, &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_cook:
	MATCH_HDR(phttp, MIME_HDR_COOKIE, scan, hdr_len, hdr_token, skip_len,
		  &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_date:
	MATCH_HDR(phttp, MIME_HDR_DATE, scan, hdr_len, hdr_token, skip_len,
		  &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_etag:
	MATCH_HDR(phttp, MIME_HDR_ETAG, scan, hdr_len, hdr_token, skip_len,
		  &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_expe:
	MATCH_HDR(phttp, MIME_HDR_EXPIRES, scan, hdr_len, hdr_token,
		  skip_len, &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_expi:
	MATCH_HDR(phttp, MIME_HDR_EXPIRES, scan, hdr_len, hdr_token,
		  skip_len, &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_from:
	MATCH_HDR(phttp, MIME_HDR_FROM, scan, hdr_len, hdr_token, skip_len,
		  &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_host:
	MATCH_HDR(phttp, MIME_HDR_HOST, scan, hdr_len, hdr_token, skip_len,
		  &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_if_M:
	MATCH_HDR(phttp, MIME_HDR_IF_MATCH, scan, hdr_len, hdr_token,
		  skip_len, &mp_end);
	MATCH_HDR(phttp, MIME_HDR_IF_MODIFIED_SINCE, scan, hdr_len,
		  hdr_token, skip_len, &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_if_N:
	MATCH_HDR(phttp, MIME_HDR_IF_NONE_MATCH, scan, hdr_len, hdr_token,
		  skip_len, &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_if_R:
	MATCH_HDR(phttp, MIME_HDR_IF_RANGE, scan, hdr_len, hdr_token,
		  skip_len, &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_if_U:
	MATCH_HDR(phttp, MIME_HDR_IF_UNMODIFIED_SINCE, scan, hdr_len,
		  hdr_token, skip_len, &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_keep:
	MATCH_HDR(phttp, MIME_HDR_KEEP_ALIVE, scan, hdr_len, hdr_token,
		  skip_len, &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_last:
	MATCH_HDR(phttp, MIME_HDR_LAST_MODIFIED, scan, hdr_len, hdr_token,
		  skip_len, &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_loca:
	MATCH_HDR(phttp, MIME_HDR_LOCATION, scan, hdr_len, hdr_token,
		  skip_len, &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_max_:
	MATCH_HDR(phttp, MIME_HDR_MAX_FORWARDS, scan, hdr_len, hdr_token,
		  skip_len, &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_mime:
	MATCH_HDR(phttp, MIME_HDR_MIME_VERSION, scan, hdr_len, hdr_token,
		  skip_len, &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_prag:
	MATCH_HDR(phttp, MIME_HDR_PRAGMA, scan, hdr_len, hdr_token, skip_len,
		  &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_prox:
	MATCH_HDR(phttp, MIME_HDR_PROXY_AUTHENTICATE, scan, hdr_len,
		  hdr_token, skip_len, &mp_end);
	MATCH_HDR(phttp, MIME_HDR_PROXY_AUTHENTICATION_INFO, scan, hdr_len,
		  hdr_token, skip_len, &mp_end);
	MATCH_HDR(phttp, MIME_HDR_PROXY_AUTHORIZATION, scan, hdr_len,
		  hdr_token, skip_len, &mp_end);
	MATCH_HDR(phttp, MIME_HDR_PROXY_CONNECTION, scan, hdr_len, hdr_token,
		  skip_len, &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_publ:
	MATCH_HDR(phttp, MIME_HDR_PUBLIC, scan, hdr_len, hdr_token, skip_len,
		  &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_rang:
	MATCH_HDR(phttp, MIME_HDR_RANGE, scan, hdr_len, hdr_token, skip_len,
		  &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_refe:
	MATCH_HDR(phttp, MIME_HDR_REFERER, scan, hdr_len, hdr_token,
		  skip_len, &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_requ:
	MATCH_HDR(phttp, MIME_HDR_REQUEST_RANGE, scan, hdr_len, hdr_token,
		  skip_len, &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_retr:
	MATCH_HDR(phttp, MIME_HDR_RETRY_AFTER, scan, hdr_len, hdr_token,
		  skip_len, &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_serv:
	MATCH_HDR(phttp, MIME_HDR_SERVER, scan, hdr_len, hdr_token, skip_len,
		  &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_set_:
	MATCH_HDR(phttp, MIME_HDR_SET_COOKIE, scan, hdr_len, hdr_token,
		  skip_len, &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_trai:
	MATCH_HDR(phttp, MIME_HDR_TRAILER, scan, hdr_len, hdr_token,
		  skip_len, &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_tran:
	MATCH_HDR(phttp, MIME_HDR_TRANSFER_ENCODING, scan, hdr_len,
		  hdr_token, skip_len, &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_upgr:
	MATCH_HDR(phttp, MIME_HDR_UPGRADE, scan, hdr_len, hdr_token,
		  skip_len, &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_user:
	    MATCH_HDR(phttp, MIME_HDR_USER_AGENT, scan, hdr_len, hdr_token,
		      skip_len, &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_vary:
	    MATCH_HDR(phttp, MIME_HDR_VARY, scan, hdr_len, hdr_token,
		      skip_len, &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_via_:
	    MATCH_HDR(phttp, MIME_HDR_VIA, scan, hdr_len, hdr_token,
		      skip_len, &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_warn:
	    MATCH_HDR(phttp, MIME_HDR_WARNING, scan, hdr_len, hdr_token,
		      skip_len, &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_www_:
	    MATCH_HDR(phttp, HTTP_SIG_HDR_www_, scan, hdr_len,
		      hdr_token, skip_len, &mp_end);
	goto extension_header;
    case HTTP_SIG_HDR_te_:
	MATCH_HDR(phttp, HTTP_SIG_HDR_te_, scan, hdr_len, hdr_token,
		  skip_len, &mp_end);
	goto extension_header;
    default:
	/* Add headers that are smaller
	 * TE */

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
	break;
    }

    if (skip_len <= 0) {
	/* No valid headers */
	parse_status = HPS_ERROR;
	goto exit;
    }

    scan += skip_len;

    /* Skip Spaces */
    scan = nkn_skip_space(scan);

    if (mp_end) {
	/* Previous parser does this
	 * Only the valid string parsed by the microparser is 
	 * added as value 
	 * FIXME - It would be ideal if we dont do this. 
	 */
	scan_end = mp_end;
    }
    /* TODO - 
     * Skip spaces at end of the header value and add that
     */

    /* Add Headers */
    if (MIME_HDR_MAX_DEFS == hdr_token) {
	/* Unknown Header - excluding colon for unknown header */
	mime_hdr_add_unknown(&phttp->hdr, unknown_header, skip_len - 1,
			     scan, (scan_end - scan));
    } else {
	/* Known Header */
	mime_hdr_add_known(&phttp->hdr, hdr_token,
			   scan, (scan_end - scan));
    }
    parse_status = HPS_OK;
  exit:
    return parse_status;
}

static http_parse_status_t
http_parse_request_line(http_cb_t * phttp, const char *req_buf,
			int req_len, const char **end)
{

    http_parse_status_t parse_status = HPS_OK;
    assert(phttp && req_buf);

    if (req_len < 4) {
	return (HPS_NEED_MORE_DATA);
    }

    const char *scan = req_buf;
    const char *scan_end = scan + req_len;
    const char *temp = NULL;

    switch (*(unsigned int *) scan) {
    case HTTP_SIG_MTHD_GET:
	if (strncmp(scan, "GET ", ST_STRLEN("GET ")) != 0)
	    goto extension_method;
	scan += ST_STRLEN("GET");
	SET_HTTP_FLAG(phttp, HRF_METHOD_GET);
	break;
    case HTTP_SIG_MTHD_HEAD:
	if (strncmp(scan, "HEAD ", ST_STRLEN("HEAD ")) != 0)
	    goto extension_method;
	scan += ST_STRLEN("HEAD");
	SET_HTTP_FLAG(phttp, HRF_METHOD_HEAD);
	break;
    case HTTP_SIG_MTHD_POST:
	if (strncmp(scan, "POST ", ST_STRLEN("POST "))
	    != 0)
	    goto extension_method;
	scan += ST_STRLEN("POST");
	SET_HTTP_FLAG(phttp, HRF_METHOD_POST);
	break;
    case HTTP_SIG_MTHD_PUT:
	if (strncmp(scan, "PUT ", ST_STRLEN("PUT ")) != 0)
	    goto extension_method;
	scan += ST_STRLEN("PUT");
	break;
    case HTTP_SIG_MTHD_DELETE:
	if (strncmp(scan, "DELETE ", ST_STRLEN("DELETE ")) != 0)
	    goto extension_method;
	scan += ST_STRLEN("DELETE");
	break;
    case HTTP_SIG_MTHD_TRACE:
	if (strncmp(scan, "TRACE ", ST_STRLEN("TRACE ")) != 0)
	    goto extension_method;
	scan += ST_STRLEN("TRACE");
	break;
    case HTTP_SIG_MTHD_CONNECT:
	if (strncmp(scan, "CONNECT ", ST_STRLEN("CONNECT ")) != 0)
	    goto extension_method;
	scan += ST_STRLEN("CONNECT");
	SET_HTTP_FLAG(phttp, HRF_METHOD_CONNECT);
	SET_HTTP_FLAG(phttp, HRF_FORWARD_PROXY);
	break;
    case HTTP_SIG_MTHD_OPTIONS:
	if (strncmp(scan, "OPTIONS ", ST_STRLEN("OPTIONS ")) != 0)
	    goto extension_method;
	scan += ST_STRLEN("OPTIONS");
	SET_HTTP_FLAG(phttp, HRF_METHOD_OPTIONS);
	break;
    default:
      extension_method:
	temp = nkn_skip_token(scan, ' ');
	if (temp == NULL) {
	    break;
	}
	scan = temp;
	break;
    }

    /* Invalid format of request line hence returning failure */
    if (*scan != ' ' && scan == req_buf) {
	return HPS_ERROR;
    }

    /*
     * Add method to header
     */
    mime_hdr_add_known(&phttp->hdr, MIME_HDR_X_NKN_METHOD, req_buf,
		       scan - req_buf);

    /* Skip SP */
    scan = nkn_skip_space(scan);

    if (scan > scan_end) {
	return (HPS_NEED_MORE_DATA);
    }

    /* Parse HTTP URL 
     * URLs will be parsed and added to mime HDRs*/
    if (CHECK_HTTP_FLAG(phttp, HRF_METHOD_CONNECT)) {
	parse_status =
	    http_parse_authority(phttp, scan, scan_end - scan,
				 (const char **) &scan);
    } else {
	parse_status =
	    http_parse_url(phttp, scan, scan_end - scan,
			   (const char **) &scan);
    }

    if (HPS_OK != parse_status) {
	return parse_status;
    }

    /* Skip SP */
    scan = nkn_skip_space(scan);

    if (scan > scan_end) {
	return (HPS_NEED_MORE_DATA);
    }

    /* Parse Version string */
    parse_status = http_parse_version(phttp, scan, scan_end - scan, &scan);

    if (HPS_OK != parse_status) {
	return (parse_status);
    }

    if (end) {
	*end = scan;
    }
    return HPS_OK;
}

/**
 * @brief Parses HTTP Request
 *	Request Parsed into Known Headers and Unknown Headers
 *
 * @param phttp - Http control block context
 * @param req_buf - Request Message Buffer
 * @param req_len - Request Message Length
 * @param end - Return pointer till which the message is parsed
 *
 * @return - Parse Status
 */
inline http_parse_status_t
http_parse_request(http_cb_t * phttp, const char *req_buf,
		   int req_len, const char **end)
{
    http_parse_status_t parse_status = HPS_ERROR;

    const char *scan = req_buf;
    const char *hdr_end = NULL;
    int scan_len = 0;
    const char *scan_next;
    int http_hdr_len;
    /* Character used to swap and 
     * terminate string with Null */
    char swap_ch = 0;


    /*
     * Mark end of HTTP request
     */
    //Should be done before calling this function
    //req_buf[req_len] = '\0';

    /*
     * search for the end of HTTP request
     * \r\n\r\n could be in the middle of HTTP 
     * request when content-length exists
     */

    scan = req_buf;
    scan_len = req_len;

    if (scan_len < 4) {
	while (*scan != '\r' && *scan != '\n') {
	    scan++;
	}
	if (*scan == '\r' || *scan == '\n') {
	    return HPS_ERROR;
	}
	return HPS_NEED_MORE_DATA;
    }

    DBG_LOG(MSG, MOD_HTTP, "recieved request: <%s>", req_buf);

    while (scan_len >= 4) {
	if (*scan == '\r' && *(scan + 1) == '\n' && *(scan + 2) == '\r'
	    && *(scan + 3) == '\n') {
	    goto complete;
	}
	// Disallow request with embedded NULL(s)
	if (*scan == 0) {
	    // If we see \0 before \r\n\r\n, it is bad request.
	    glob_http_parse_err_nulls++;
	    return HPS_ERROR;
	}
	scan_len--;
	scan++;
    }
    return HPS_NEED_MORE_DATA;

  complete:
    http_hdr_len = scan + 4 - &req_buf[0];
    hdr_end = scan + http_hdr_len;

    // Prevent HTTP GET attack:
    //      To avoid that client established lots of valid connections. 
    //      and these connections will send huge amount of GETs per second
    //      to overload this system CPU/Memory.
    //      We set up the max_get_rate_limit to prevent this kind of attack.
    //
    glob_tot_get_in_this_second++;
    if ((int) glob_tot_get_in_this_second > max_get_rate_limit) {
	// Sorry, we have reached the limit of HTTP GET in this second.
	// We regard this GET as HTTP attack.
	// Then we will close the connection.
	DBG_LOG(MSG, MOD_HTTP,
		"Reject GET request. It seems that we are under GET attack."
		" glob_tot_get_in_this_second=%d",
		glob_tot_get_in_this_second);
	glob_http_parse_err_req_rate++;
	return HPS_ERROR;
    }
    scan = req_buf;

    /* Method, Uri, HTTP Version validated here */
    parse_status =
	http_parse_request_line(phttp, scan, http_hdr_len, &scan);

    while (parse_status == HPS_OK) {

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

	http_hdr_len = 0;
	scan_next = scan;

	while (*scan_next != '\r' && *scan_next != '\n') {
	    http_hdr_len++;
	    scan_next++;
	}

	swap_ch = *scan_next;
	*(char *) scan_next = 0;
	parse_status = http_add_header(phttp, scan, http_hdr_len, &scan);
	*(char *) scan_next = swap_ch;
    }

    if (HPS_NEED_MORE_DATA == parse_status) {
	parse_status = HPS_ERROR;
    }

    if(end){
	*end = scan;
    }
    return parse_status;
}
