#ifdef PROTO_HTTP_LIB
#include "proto_http_int.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <dlfcn.h>
#include <errno.h>
#include <netdb.h>
#include <time.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/timeb.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <pthread.h>

#ifdef PROTO_HTTP_LIB
#include "memalloc.h"
#include "http_header.h"
#include "parser_utils.h"
#include "nkn_tunnel_defs.h"
#include <alloca.h>
#else /* !PROTO_HTTP_LIB */
#include "nkn_cfg_params.h"
#include "nkn_debug.h"
#include "nkn_http.h"
#include "nkn_stat.h"
#include "parser_utils.h"
#include "pe_defs.h"
#include "nkn_tunnel_defs.h"

extern int nkn_strcmp_incase(const char * p1, const char * p2, int n);
#endif /* !PROTO_HTTP_LIB */

// List all counters
NKNCNT_DEF( cm_err_data_length, int, "", "Total Error in data length")
NKNCNT_DEF( tot_get_in_this_second, uint64_t, "", "Total HTTP transaction in this second")
NKNCNT_DEF( http_parse_err_nulls, uint64_t, "", "embedded null  errors")
NKNCNT_DEF( http_parse_err_req_rate, uint64_t, "", "request rate errors")
NKNCNT_DEF( http_parse_err_uri_tolong, uint64_t, "", "uri to long errors")
NKNCNT_DEF( http_parse_err_req_toshort, uint64_t, "", "short request errors")
NKNCNT_DEF( http_parse_err_normalize_uri, uint64_t, "", "normalize uri errors")
NKNCNT_DEF( http_09_request, uint64_t, "", "HTTP/0.9 requests")
NKNCNT_DEF( http_parse_err_badreq_1, uint64_t, "", "malformed case 1 errors")
NKNCNT_DEF( http_parse_err_badreq_2, uint64_t, "", "malformed case 2 errors")
NKNCNT_DEF( http_parse_err_badreq_3, uint64_t, "", "malformed case 3 errors")
NKNCNT_DEF( http_parse_err_badreq_4, uint64_t, "", "malformed case 4 errors")
NKNCNT_DEF( http_parse_err_badreq_5, uint64_t, "", "malformed case 5 errors")
NKNCNT_DEF( http_parse_err_longcookie, uint64_t, "", "long cookie errors")
NKNCNT_DEF( http_parse_err_longheader, uint64_t, "", "long header errors")
NKNCNT_DEF( tunnel_req_tunnel_only, uint64_t, "", "tunnel")
NKNCNT_DEF( tunnel_req_chunked_encode, uint64_t, "", "tunnel")
NKNCNT_DEF( tunnel_req_hdr_len, uint64_t, "", "tunnel")
NKNCNT_DEF( tunnel_req_uri_size, uint64_t, "", "tunnel")
NKNCNT_DEF( tunnel_req_authorization, uint64_t, "", "tunnel")
NKNCNT_DEF( tunnel_req_multi_bye_range, uint64_t, "", "tunnel")
NKNCNT_DEF( tunnel_req_not_get, uint64_t, "", "tunnel")
NKNCNT_DEF( tunnel_req_not_allow_content_type, uint64_t, "", "tunnel")
NKNCNT_DEF( tunnel_req_content_length, uint64_t, "", "tunnel")
NKNCNT_DEF( tunnel_req_cookie, uint64_t, "", "cookie")
NKNCNT_DEF( tunnel_req_cache_ctrl_no_cache, uint64_t, "", "cache_control")
NKNCNT_DEF( tunnel_req_max_age_0, uint64_t, "", "max_age=0")
NKNCNT_DEF( tunnel_req_pragma_no_cache, uint64_t, "", "pragma: no-cache")
NKNCNT_DEF( tunnel_req_query_str, uint64_t, "", "no SSP, query string, T-Proxy")
NKNCNT_DEF( tunnel_req_int_crawl_request, uint64_t, "", "Internal crawl request")
NKNCNT_DEF( accept_encoding, uint64_t, "", "no of accept-encoding request")

#ifdef PROTO_HTTP_LIB

#define MAX_HTTP_METHODS_ALLOWED 16
typedef struct http_method_types {
    int32_t  method_cnt;
    int32_t all_methods;
    struct {
	    int32_t allowed;	// This Method Type is allowed
	    int32_t method_len;		// Request Type Len
	    char	*name;	// Request Type string
    } method[MAX_HTTP_METHODS_ALLOWED];
	
} http_method_types_t;

http_method_types_t http_allowed_methods = {0, 1, { {0, 0, 0} }};
#define NKN_MUTEX_LOCK(_lck)
#define NKN_MUTEX_UNLOCK(_lck)

/*
 * Very fast case insensitive comparison for two string.
 * return:
 *    0: p1 and p2 are equal for n bytes.
 *    1: p1 and p2 are not equal.
 */
static
int32_t nkn_strcmp_incase(const char * p1, const char * p2, int n)
{
	// compare two string without case
	if(!p1 || !p2 || n==0) return 1;
	while(n) {
		if(*p1==0 || *p2==0) return 1;
		if((*p1|0x20) != (*p2|0x20)) return 1;
		p1++;
		p2++;
		n--;
	}
	return 0;
}

#else
http_method_types_t http_allowed_methods;
#endif

//http_allowed_methods.method_cnt = 0;
//http_allowed_methods.all_methods = 0;
////////////////////////////////////////////////////////////////////////////////////
// HTTP Request 
////////////////////////////////////////////////////////////////////////////////////

#define sig_accept		0x65636361
#define sig_accept_charset	sig_accept
#define sig_accept_encoding	sig_accept
#define sig_accept_language	sig_accept
#define sig_accept_ranges	sig_accept

#define sig_allow		0x6f6c6c61

#define sig_authentication_info 0x68747561
#define sig_authorization	sig_authentication_info

#define sig_cache_control	0x68636163

#define sig_connection		0x6e6e6f63 // "conn" Connection: Keep-Alive

#define sig_content_base	0x746e6f63
#define sig_content_disposition sig_content_base
#define sig_content_encoding	sig_content_base
#define sig_content_language	sig_content_base
#define sig_content_length	sig_content_base // "cont" Content-Length: 20000
#define sig_content_location	sig_content_base
#define sig_content_md5		sig_content_base
#define sig_content_range	sig_content_base
#define sig_content_type	sig_content_base

#define sig_cookie		0x6b6f6f63 // "cook" Cookie: cooke_name = cookie_value
#define sig_date		0x65746164
#define sig_etag		0x67617465
#define sig_expect		0x65707865
#define sig_expires		0x69707865
#define sig_from		0x6d6f7266
#define sig_get			0x20746567 // "get " GET /doc/location HTTP/1.0
#define sig_head		0x64616568 // "head" HEAD /doc/location HTTP/1.0
#define sig_post		0x74736f70 // "post" POST /doc/location HTTP/1.0
#define sig_connect		0x6e6e6f63 // "conn" CONNECT domain:port HTTP/1.0
#define sig_options		0x6974706f // "opti" OPTIONS /doc/location HTTP/1.0
#define sig_put			0x20747570 // "put " PUT /doc/location HTTP/1.0
#define sig_delete		0x656c6564 // "dele" DELETE /doc/location HTTP/1.0
#define sig_trace		0x63617274 // "trac" TRACE /doc/location HTTP/1.0
#define sig_host		0x74736f68
#define sig_http        	0x70747468 // "http" HTTP/1.0 200 OK

#define sig_if_match		0x6d2d6669
#define sig_if_modified_since	sig_if_match

#define sig_if_none_match	0x6e2d6669

#define sig_if_range		0x722d6669

#define sig_if_unmodfied_since	0x752d6669

#define sig_keep_alive		0x7065656b
#define sig_last_modified	0x7473616c
#define sig_link		0x6b6e696c
#define sig_location		0x61636f6c
#define sig_max_forwards	0x2d78616d
#define sig_mime_version	0x656d696d
#define sig_pragma		0x67617270

#define sig_proxy_authenticate	0x786f7270
#define sig_proxy_authentication_info sig_proxy_authenticate
#define sig_proxy_authorization	sig_proxy_authenticate
#define sig_proxy_connection	sig_proxy_authenticate

#define sig_public		0x6c627570
#define sig_range		0x676e6172 // "Rang" Range: bytes=21010-47021
#define sig_request_range	0x75716572
#define sig_referer		0x65666572
#define sig_retry_after		0x72746572
#define sig_server		0x76726573
#define sig_set_cookie		0x2d746573
#define sig_transfer_encoding	0x6e617274 // "Tran" Transfer-Encoding: chunked
#define sig_trailer		0x69617274
#define sig_upgrade		0x72677075
#define sig_user_agent		0x72657375
#define sig_vary		0x79726176
#define sig_warning		0x6e726177
#define sig_www_authenticate	0x2d777777
#define sig_internal     	0x6b6e2d78 // "X-NK" X-NKN-Internal: client=ftp
#define sig_client       	0x65696c63 // "clie" 'client=ftp'
#define sig_crawl		0x77617263 // "craw" 'crawl-req'

#if 0
#define sig_x_accel		0x63612d78 // "X-Accel"
#endif

// < 4 char variants
#define sig_age			0
#define sig_te			0
#define sig_via			0

#define gzip			0x70697a67
#define deflate			0x6c666564
#define compress		0x706d6f63
#define exi			0x20697865
#define identity		0x6e656469
#define pack200_gzip		0x6b61636b
#define sdch			0x73646368
#define bzip2			0x70697a62
#define peerdist		0x72656570


// Under known header condition,
// if single word in this header line, this line is ignored.
// if value does not exist in this header line, this line is ignored.
static inline int
add_if_match(mime_header_t * hdr, http_header_id id,
			const char * p, int * hit)
{
    const char * p_end;

    const char * tp;
    tp = nkn_find_hdrname_end(p);
    if ((tp-p+1) != http_known_headers[id].namelen) {
	return 0;
    }

    if (nkn_strcmp_incase(p,
			http_known_headers[id].name,
			http_known_headers[id].namelen)==0){
	    int check = 0;
	    p = nkn_skip_colon_check(p+http_known_headers[id].namelen, &check);
	    if(!check)
		return 0;
	    p_end = nkn_find_hdrvalue_end(p);
	    /* Removing Trailing spaces */
	    while (*p_end == ' ' || *p_end == '\t' || *p_end == '\r') p_end--;
            if (((*p_end == ':') || (p-1) <= p_end) && (*p != '\n')) {
               /*
                * Support only '100-continue' export header now.
                * For other expect tokens, return -2, which will be handled
                * in 'http_parse_header_bufptr'.
                */
                if((id == MIME_HDR_EXPECT) &&
                   (nkn_strcmp_incase(p, "100-continue", p_end-p+1))){
                   return -2 ;
                }

                if (id == MIME_HDR_X_INTERNAL) {
                   switch(*(unsigned int *)p | 0x20202020) {
                   case sig_client:  // matches 'clie' part of input header.
                         // Assuming only 3 values for internal client request now.
                         // Setting the internal - request flag in http->flags,
                         //    at a later point of time this could be enhanced to 
                         //    add different flags for each type of request, if need to.
                         if ((nkn_strcmp_incase(p, "client=ftp", 10) == 0)
                            || (nkn_strcmp_incase(p, "client=mfc-probe", 16) == 0)
                            || (nkn_strcmp_incase(p, "client=pre-fetch", 16) == 0)) {
                             return -3;
                         }
                         break;
                   case sig_crawl:  // matches 'crawl' part of input header.
                         if (nkn_strcmp_incase(p, "crawl-req", 9) == 0)
                             return -4;
                         break;
                   default:
                         break;
                   }
                }

                add_known_header(hdr, id, p, ((*p_end == ':') ? 0:
                                                             (p_end-p+1)));

		(*hit)++;
		return 1;
	    }
	    return 0;
    } else {
	    return 0;
    }
}

static inline int
add_kheader(mime_header_t * hdr, http_header_id id,
			const char * p, int * hit)
{
    const char * p_end;
    int check = 0;

    p = nkn_skip_colon_check(p+http_known_headers[id].namelen, &check);
    if(!check)
	return 0;

    p_end = nkn_find_hdrvalue_end(p);
    /* Removing Trailing spaces */
    while (*p_end == ' ' || *p_end == '\t' || *p_end == '\r') p_end--;
    if (((p-1) <= p_end) && (*p != '\n')) {
	add_known_header(hdr, id, p, p_end-p+1);
	(*hit)++;
	return 1;
    }
    return 0;
}

static inline int
add_if_custom_name_match(mime_header_t *hdr, http_header_id id,
			const char *p, const char *hdr_name, int hdr_len, int *hit)
{
    const char * p_end;

    const char * tp;
    tp = nkn_find_hdrname_end(p);
    if ((tp-p+1) != hdr_len) {
	return 0;
    }

    if (nkn_strcmp_incase(p, hdr_name, hdr_len) == 0) {
	    int check = 0;
	    p = nkn_skip_colon_check(p+hdr_len, &check);
	    if(!check)
		return 0;
	    p_end = nkn_find_hdrvalue_end(p);
	    /* Removing Trailing spaces */
	    while (*p_end == ' ' || *p_end == '\t' || *p_end == '\r') p_end--;
	    if (((p-1) <= p_end) && (*p != '\n')) {
		add_known_header(hdr, id, p, p_end-p+1);
		(*hit)++;
		return 1;
	    }
	    return 0;
    } else {
	    return 0;
    }
}

static inline int
append_add_unknown_header(mime_header_t *hdr, const char *name,
		       int namelen, const char *value, int valuelen){

#define DEFAULT_OUTBUF_BUFSIZE (32*1024)

    char default_buf[DEFAULT_OUTBUF_BUFSIZE];
    char *outbuf = default_buf;
    char *tmp_outbuf;
    int outbufsize = DEFAULT_OUTBUF_BUFSIZE;
    int tmp_outbufsize;

#define OUTBUF(_data, _datalen, _bytesused) { \
    if (((_datalen)+(_bytesused)) >= outbufsize) { \
	tmp_outbuf = outbuf; \
    	tmp_outbufsize = outbufsize; \
	if (outbufsize == DEFAULT_OUTBUF_BUFSIZE) { \
	    outbuf = 0; \
	} \
	outbufsize = MAX(outbufsize * 2, (_datalen)+(_bytesused)+1); \
	outbuf = nkn_realloc_type(outbuf, outbufsize, mod_http_hdrbuf); \
	if (outbuf) { \
	    if (tmp_outbufsize == DEFAULT_OUTBUF_BUFSIZE) { \
	    	memcpy(outbuf, tmp_outbuf, (_bytesused)); \
	    } \
	    memcpy((void *)&outbuf[(_bytesused)], (_data), (_datalen)); \
	    (_bytesused) += (_datalen); \
	} else { \
	    outbuf = tmp_outbuf; \
	    outbufsize = tmp_outbufsize; \
	} \
    } else { \
       	memcpy((void *)&outbuf[(_bytesused)], (_data), (_datalen)); \
    	(_bytesused) += (_datalen); \
    } \
}

    int bytesused = 0;
    uint32_t attr;
    const char * data = NULL;
    int datalen = 0;
    int rv = 0;

    if(!name || !value || !hdr){
	return  (-1);
    }

    rv = get_unknown_header (hdr, name, namelen, &data, &datalen, &attr);

    if (rv == 0) {
	OUTBUF(data, datalen, bytesused);
	OUTBUF(",", 1, bytesused);
	OUTBUF(value, valuelen, bytesused);

	rv = add_unknown_header(hdr, name, namelen, outbuf, bytesused);

    } else {
	rv = add_unknown_header(hdr, name, namelen, value, valuelen);

    }
    return rv;
}

static int get_encoding_type(http_cb_t * phttp, int id, const char *p)
{
	int check = 0;
	int len = 0;
	int identity_type = 0;
	const char *ps, *p_end;

	ps = nkn_skip_colon_check(p+http_known_headers[id].namelen, &check);
	p_end = nkn_find_hdrvalue_end(ps);
	len = p_end - ps + 1;
	while (len > 0 && (*ps == ' ' || *ps == '\t')) ps++, len--;
	while (len > 0) {
		if (len && *ps == '*') {
			ps++, len--;
			if (len >= 4 && nkn_strcmp_incase(ps, ";q=0", 4) == 0) {
				phttp->http_encoding_type |= HRF_ENCODING_IDENTITY;
			} else {
				phttp->http_encoding_type |= HRF_ENCODING_ALL;
				break;
			}
		} else {
			switch (*(unsigned int *)ps | 0x20202020) {
			case gzip:
				phttp->http_encoding_type |= HRF_ENCODING_GZIP;
				ps += 4;
				len -= 4;
				break;
			case deflate:
				if (nkn_strcmp_incase(ps+4, "ate", 3) == 0) {
					phttp->http_encoding_type |= HRF_ENCODING_DEFLATE;
					ps += 7;
					len -= 7;
				}
				break;
			case compress:
				if (nkn_strcmp_incase(ps+4, "ress", 4) == 0) {
					phttp->http_encoding_type |= HRF_ENCODING_COMPRESS;
					ps += 8;
					len -= 8;
				}
				break;
			case exi:
				phttp->http_encoding_type |= HRF_ENCODING_EXI;
				ps += 3;
				len -= 3;
				break;
			case identity:
				if (nkn_strcmp_incase(ps+4, "tity", 4) == 0) {
					phttp->http_encoding_type |= HRF_ENCODING_IDENTITY;
					identity_type = 1;
					ps += 8;
					len -= 8;
				}
				break;
			case pack200_gzip:
				if (nkn_strcmp_incase(ps+4, "200_gzip", 8) == 0) {
					phttp->http_encoding_type |= HRF_ENCODING_PACK200_GZIP;
					ps += 12;
					len -= 12;
				}
				break;
			case sdch:
				phttp->http_encoding_type |= HRF_ENCODING_SDCH;
				ps += 4;
				len -= 4;
				break;
			case bzip2:
				phttp->http_encoding_type |= HRF_ENCODING_BZIP2;
				ps += 4;
                                len -= 4;
                                break;
			case peerdist:
				if (nkn_strcmp_incase(ps+4, "dist", 4) == 0) {
					phttp->http_encoding_type |= HRF_ENCODING_PEERDIST;
					ps += 8;
					len -= 8;
				}
				break;
			default:
				break;
			}
		}
		while (len > 0 && (*ps != ',')) ps++, len--;
		if (len > 0 ) ps++, len--;
		while (len > 0 && (*ps == ' ' || *ps == '\t')) ps++, len--;
	}

	if ((phttp->http_encoding_type == 0) && (identity_type == 0)) {
		phttp->http_encoding_type = HRF_ENCODING_UNKNOWN;
	}

	return 0;
}

static const char * http_parse_request_line(http_cb_t * phttp, const char * p, char ** pbak);
static const char * http_parse_request_line(http_cb_t * phttp, const char * p, char ** pbak)
{
        const char * ps, *pd;
	const char * p_query = 0;
	const char * p_end;
	const char *req_line = NULL;
	int len;
	int hit = 0;
	int url_decode_req, url_decode_non_space;
	mime_header_t *hdr = &phttp->hdr;

again:
	/* Remove leading space and tab character if there */
	p = nkn_skip_space(p);

	//
	// take the first four bytes as signature
	// It is case insensitive comparision
	//
	switch(*(unsigned int *)p | 0x20202020) {
		case sig_get:
			SET_HTTP_FLAG(phttp, HRF_METHOD_GET);
			add_known_header(hdr, MIME_HDR_X_NKN_METHOD, p, 3);
			req_line = p;
			p+=4;
			goto parse_uri;

		case sig_head:
			if(p[4] !=  ' ')
			    break;
			SET_HTTP_FLAG(phttp, HRF_METHOD_HEAD);
			add_known_header(hdr, MIME_HDR_X_NKN_METHOD, p, 4);
			req_line = p;
			p+=5;
			goto parse_uri;

		case sig_post:
			if(p[4] !=  ' ')
			    break;
			SET_HTTP_FLAG(phttp, HRF_METHOD_POST);
			add_known_header(hdr, MIME_HDR_X_NKN_METHOD, p, 4);
			req_line = p;
			p+=5;
			goto parse_uri;

		case sig_options:
			if(nkn_strcmp_incase(p, "OPTIONS ", sizeof("OPTIONS ")-1)!=0)
			    break;
			SET_HTTP_FLAG(phttp, HRF_METHOD_OPTIONS);
			if(p[7] !=  ' ')
			    break;
			add_known_header(hdr, MIME_HDR_X_NKN_METHOD, p, 7);
			req_line = p;
			p+=8;
			goto parse_uri;

		case sig_put:
			SET_HTTP_FLAG(phttp, HRF_METHOD_PUT);
			add_known_header(hdr, MIME_HDR_X_NKN_METHOD, p, 3);
			req_line = p;
			p+=4;
			goto parse_uri;

		case sig_delete:
			if(p[6] !=  ' ')
			    break;
			SET_HTTP_FLAG(phttp, HRF_METHOD_DELETE);
			add_known_header(hdr, MIME_HDR_X_NKN_METHOD, p, 6);
			req_line = p;
			p+=7;
			goto parse_uri;

		case sig_trace:
			if(p[5] !=  ' ')
			    break;
			SET_HTTP_FLAG(phttp, HRF_METHOD_TRACE);
			add_known_header(hdr, MIME_HDR_X_NKN_METHOD, p, 5);
			req_line = p;
			p+=6;
			goto parse_uri;

parse_uri:
			// GET /htdocs/location.html HTTP/1.0
			//printf("parse: p=%s\n", p);
			hit++;
			url_decode_req = 0;
			url_decode_non_space = 0;
			p=nkn_skip_space(p);
			ps=p;
			while(*p!=' ' && *p!='\n') { 
				if(*p==0) {
					glob_http_parse_err_uri_tolong++;
					return NULL; // URI is too long
				}
				if (*p == '?' && !p_query) p_query = p;
				if (*p == '%') { 
					url_decode_req = 1;
					/* Flag if the hex encoding has other characters
					 */
					/* BZ 5235: define a macro here so that we know which 
					 * hex encoded char is being tested for. 
					 * Also, check if url_decode_non_space is already set - 
					 * if so, then no need to test since we are already
					 * flagged to decode the url .. no point in brute-force
					 * testing of each character.
					 */
#define CHECK_HEX_ENC(p, a, b)		( ( *( ( (char *)(p) ) + 1 ) == (char) (a) ) && \
					  ( *( ( (char *)(p) ) + 2 ) == (char) (b) ) )
					if ( !url_decode_non_space && 
					    ! ( CHECK_HEX_ENC(p, '2', '0') ||	// space
						CHECK_HEX_ENC(p, '2', '1') ||	// !
						CHECK_HEX_ENC(p, '2', '3') ||	// #
						CHECK_HEX_ENC(p, '2', '4') ||	// $
						CHECK_HEX_ENC(p, '2', '5') ||	// %
						CHECK_HEX_ENC(p, '2', '6') ||	// &
						CHECK_HEX_ENC(p, '2', '8') ||	// (
						CHECK_HEX_ENC(p, '2', '9') ||	// )
						CHECK_HEX_ENC(p, '2', 'A') || CHECK_HEX_ENC(p, '2', 'a') || // *
						CHECK_HEX_ENC(p, '2', 'B') || CHECK_HEX_ENC(p, '2', 'B') || // +
						CHECK_HEX_ENC(p, '2', 'D') || CHECK_HEX_ENC(p, '2', 'd') || // -
						CHECK_HEX_ENC(p, '2', 'E') || CHECK_HEX_ENC(p, '2', 'e') || // .
						CHECK_HEX_ENC(p, '3', 'A') || CHECK_HEX_ENC(p, '3', 'a') || // :
						CHECK_HEX_ENC(p, '3', 'B') || CHECK_HEX_ENC(p, '3', 'b') || // ;
						CHECK_HEX_ENC(p, '3', 'C') || CHECK_HEX_ENC(p, '3', 'c') || // <
						CHECK_HEX_ENC(p, '3', 'D') || CHECK_HEX_ENC(p, '3', 'd') || // =
						CHECK_HEX_ENC(p, '3', 'E') || CHECK_HEX_ENC(p, '3', 'e') || // >
						CHECK_HEX_ENC(p, '3', 'F') || CHECK_HEX_ENC(p, '3', 'f') || // ?
						CHECK_HEX_ENC(p, '4', '0') || 	// @
						CHECK_HEX_ENC(p, '5', 'B') || CHECK_HEX_ENC(p, '5', 'b') || // [
						CHECK_HEX_ENC(p, '5', 'D') || CHECK_HEX_ENC(p, '5', 'd') || // ]
						CHECK_HEX_ENC(p, '5', 'E') || CHECK_HEX_ENC(p, '5', 'e') || // ^
						CHECK_HEX_ENC(p, '5', 'F') || CHECK_HEX_ENC(p, '5', 'f') || // _
						CHECK_HEX_ENC(p, '6', '0') || 	// `
						CHECK_HEX_ENC(p, '7', 'B') || CHECK_HEX_ENC(p, '7', 'b') || // {
						CHECK_HEX_ENC(p, '7', 'C') || CHECK_HEX_ENC(p, '7', 'c') || // |
						CHECK_HEX_ENC(p, '7', 'D') || CHECK_HEX_ENC(p, '7', 'd') || // }
						CHECK_HEX_ENC(p, '7', 'E') ||  CHECK_HEX_ENC(p, '7', 'e')) ) // ~
					{
						url_decode_non_space = 1;
					}

					//if (!((*(p+1) == '2' && *(p+2) =='0')||	// %20 Space
						//(*(p+1) == '2' && *(p+2) =='6')||	// %26 &
						//(*(p+1) == '3' && (*(p+2) == 'A' || 
						//*(p+2) == 'a')))) {			// %3A :
						//url_decode_non_space = 1;
					//}
#undef CHECK_HEX_ENC
				}
				p++; 
			}
			pd=p;
			len=p-ps;
			//printf("parse: len=%d\n", len);
			// We make a limitation here to avoid crash
                        if (len < 1) {
				glob_http_parse_err_req_toshort++;
				return NULL;
			}

			/*
			 * Case 1: GET /uri HTTP/1.0
			 * Case 2: GET http://www.cnn.com/uri HTTP/1.0
			 */
			if(nkn_strcmp_incase(ps, "http://", 7)==0) {
				const char * phost;
				// forward proxy case
				ps += 7; len -= 7;
				phost = ps;
				while(*ps!='/' && *ps!=' ' && *ps!=0 && ps < p) { ps++; len--; }
				SET_HTTP_FLAG(phttp, HRF_FORWARD_PROXY);
				add_known_header(hdr, MIME_HDR_X_NKN_ABS_URL_HOST, 
						 phost, ps-phost);
			}
			if(nkn_strcmp_incase(ps, "https://", 8) == 0) {
				const char *phost;
				ps +=8;
				phost = ps; len -=8; 
				while(*ps!='/' && *ps!=' ' && *ps!=0 && ps < p) { ps++; len--; }
				SET_HTTP_FLAG(phttp, HRF_FORWARD_PROXY);
				add_known_header(hdr, MIME_HDR_X_NKN_ABS_URL_HOST, 
						 phost, ps-phost);
				
			}
			if (ps && *ps && (len > 0) && 
				!((CHECK_HTTP_FLAG(phttp, HRF_METHOD_OPTIONS) ||
				 CHECK_HTTP_FLAG(phttp, HRF_METHOD_TRACE) )&& *ps == '*'))  {
				add_known_header(hdr, MIME_HDR_X_NKN_URI, ps, len);
			} else if ((ps && len == 0) || 
					(ps && *ps == '*' && len == 1)){
				add_known_header(hdr, MIME_HDR_X_NKN_URI, "/", sizeof("/") - 1);
			}
			if (p_query && (p-p_query)) {
			    SET_HTTP_FLAG(phttp, HRF_HAS_QUERY);
			    add_known_header(hdr, MIME_HDR_X_NKN_QUERY, p_query,
			    		p-p_query);
			}

			if (url_decode_req) {
			    char *tbuf;
			    int bytesused;
			    /* Set HRF_HAS_HEX_ENCODE flag only if the url
			     * contains hex encoding for char's other than space
			     */
			    if (url_decode_non_space)
			    	SET_HTTP_FLAG(phttp, HRF_HAS_HEX_ENCODE);
			    tbuf = (char *)alloca(len+1);
			    if (!canonicalize_url(ps, len, tbuf, len+1, 
			    			&bytesused)) {
			    	add_known_header(hdr, MIME_HDR_X_NKN_DECODED_URI,
						tbuf, bytesused-1);
			    }
			}

			if (len <= (MAX_URI_SIZE-MAX_URI_HDR_SIZE)) {
			    if (normalize_http_uri(hdr)) {
				glob_http_parse_err_normalize_uri++;
				return NULL;
			    }
			}

			// Get information of HTTP/1.0 or HTTP/1.1
			p=nkn_skip_space(p);
			if (*p == '\n') {
				// This could be a HTTP/0.9 request. Sample HTTP/0.9 request format is 
				// shown below,
				// Note: GET <document_path>
				if (CHECK_HTTP_FLAG(phttp, HRF_METHOD_GET)) {
					const char *uri_s;
					uri_s = ps;
					while(*uri_s!='\r' && *uri_s!='\n' && *uri_s!=' '
						&& *uri_s!=0 && uri_s < p) { 
						uri_s++; 
					}
					add_known_header(hdr, MIME_HDR_X_NKN_URI, ps, (uri_s-ps));
					add_known_header(hdr, MIME_HDR_X_NKN_REQ_LINE, req_line, (uri_s-req_line));	
					SET_HTTP_FLAG(phttp, HRF_HTTP_09);
					glob_http_09_request++;
					phttp->tunnel_reason_code = NKN_TR_REQ_HTTP_09_REQUEST;
				} else {
					glob_http_parse_err_badreq_1++;
				}
				return NULL;
			}

			p_end = nkn_find_hdrvalue_end(p);
			/* Removing Trailing spaces */
			while (*p_end == ' ') p_end--;
			p_end++;
			if ((8 == p_end-p) && nkn_strcmp_incase(p, "HTTP/1.0", 8)==0) {
				SET_HTTP_FLAG(phttp, HRF_HTTP_10);
			}
			else if ((8 == p_end-p) && nkn_strcmp_incase(p, "HTTP/1.1", 8)==0) {
				SET_HTTP_FLAG(phttp, HRF_HTTP_11);
			} else if ((5 <= p_end-p) && nkn_strcmp_incase(p, "HTTP/", 5)==0){
			    p +=5;
			    int major = 0;
			    int minor = 0;
			    /* Bug 3219
			     * Currently the code is to eliminate leading '0's
			     * later we can use atoi or some other means to find 
			     * versions other than 1.0 and 1.1
			     */
			    while (*p == '0') p++;
			    if (p[1] == '.') {
				major = *p - '0';
				p += 2;
				while (*p == '0') p++;
				if (p != p_end) {
				    minor = *p - '0';
				    p++;
				}
			    }
			    if (p == p_end) {
				/* Fully Parsed Version */
				if ((major == 1) && (minor == 0)) {
				    SET_HTTP_FLAG(phttp, HRF_HTTP_10);
				} else if ((major == 1) && (minor == 1)) {
				    SET_HTTP_FLAG(phttp, HRF_HTTP_11);
				}
			    }
			    /* MFD does not handle other versions 
			     * We may need to tunnel those requests*/
			}
			add_known_header(hdr, MIME_HDR_X_NKN_REQ_LINE, req_line, p_end-req_line);
                        //printf("http_parse_request: URI = %s, %d\n", phttp->uri, phttp->uri_len);

			break;

		case sig_connection:
			if(strncmp(p, "CONNECT ", 8)==0) {

				/*
				 * Hacking code:
				 * For SSL traffic, cb_buf[] is reused for tunnel code.
				 * So all data inside cb_buf will be overwritten,
				 * then add_known_header() function becomes invalid.
				 * fortunately add_known_header makes a copy if [p]
				 * is not inside cb_buf. so I take advantage of this feature here.
				 */
				if( p == phttp->cb_buf ) {
					*pbak = (char *)nkn_malloc_type(phttp->cb_totlen+1, mod_main);
					memcpy(*pbak, phttp->cb_buf, phttp->cb_totlen);
					phttp->cb_buf[phttp->cb_totlen]=0;
					p = *pbak;
					goto again;
				}

				SET_HTTP_FLAG(phttp, HRF_METHOD_CONNECT);
				SET_HTTP_FLAG(phttp, HRF_FORWARD_PROXY);
				req_line = p;
				add_known_header(hdr, MIME_HDR_X_NKN_METHOD, p, 7);
				p+=8;

				p = nkn_skip_space(p);
				/* Case 3: CONNECT www.cnn.com:443 HTTP/1.0 */
				ps = p;
				while(*ps != ' ') {
					ps++;
					if(*ps == 0) {
						glob_http_parse_err_badreq_2++;
						return NULL;
					}
				}
				len = ps - p;
				add_known_header(hdr, MIME_HDR_X_NKN_ABS_URL_HOST, p, len);
                                add_known_header(hdr, MIME_HDR_X_NKN_FP_SVRHOST, p, len);

				ps++;
				p = ps;
				p_end = nkn_find_hdrvalue_end(p);
				/* Removing Trailing spaces */
				while (*p_end == ' ') p_end--;
				p_end++;
				if ((8 == p_end-p) && nkn_strcmp_incase(p, "HTTP/1.0", 8)==0) {
					SET_HTTP_FLAG(phttp, HRF_HTTP_10);
				}
				else if ((8 == p_end-p) && nkn_strcmp_incase(p, "HTTP/1.1", 8)==0) {
					SET_HTTP_FLAG(phttp, HRF_HTTP_11);
				} else if ((5 <= p_end-p) && nkn_strcmp_incase(p, "HTTP/", 5)==0){
			   		p +=5;
			  		int major = 0;
			   	 	int minor = 0;
			    		/* Bug 3219
			     		* Currently the code is to eliminate leading '0's
			     		* later we can use atoi or some other means to find 
			     		* versions other than 1.0 and 1.1
			     		*/
			   		while (*p == '0') p++;
			  		if (p[1] == '.') {
						major = *p - '0';
						p += 2;
						while (*p == '0') p++;
						if (p != p_end) {
						    	minor = *p - '0';
				    			p++;
						}
					}
			    		if (p == p_end) {
						/* Fully Parsed Version */
						if ((major == 1) && (minor == 0)) {
						    SET_HTTP_FLAG(phttp, HRF_HTTP_10);
						} else if ((major == 1) && (minor == 1)) {
						    SET_HTTP_FLAG(phttp, HRF_HTTP_11);
						}
			    		}
			    		/* MFD does not handle other versions 
			     		* We may need to tunnel those requests*/
				}
				//if(*p=='0') SET_HTTP_FLAG(phttp, HRF_HTTP_10);
				//else SET_HTTP_FLAG(phttp, HRF_HTTP_11);

				// Fake one to make other code e.g. namespace lookup happy
				add_known_header(hdr, MIME_HDR_X_NKN_URI, "/", 1);
				add_known_header(hdr, MIME_HDR_X_NKN_REQ_LINE, req_line, p_end-req_line);
			}
			break;

                case sig_http:
                        // HTTP/1.0 200 OK

			hit++;
			if (strncasecmp(p+4 , "/1.1 ", 5) == 0) {
				SET_HTTP_FLAG(phttp, HRF_HTTP_11);
			} else if (strncasecmp(p+4 , "/1.0 ", 5) == 0) {
				SET_HTTP_FLAG(phttp, HRF_HTTP_10);
			}
                        p+=9;
			//phttp->respcode=atoi(p);
			//p+=4;
			//p_end = p;
			phttp->respcode = (int) strtol(p, (char **)&p_end, 10);
			if (p == p_end) {
				return NULL;
			}
			p = p_end;
			while (*p_end!='\r'&& *p_end!='\n' && *p_end!='\0') p_end++;
                       /*
                        * If we have reached end of cb_buf without matching '\r' or '\n',
                        * then we should not add the response string header, since
                        * a correct response string was not matched, possibly because
                        * there is more data to be read from origin.
                        */
			if (p_end == '\0') {
				return NULL;
			}
			add_known_header(hdr, MIME_HDR_X_NKN_RESPONSE_STR, p, p_end-p);
                        break;

		default:
			// Customerized HTTP header

			// This code is susceptible to context switch.
			// when config gets changed, http_allowed_method[]
			// may be modified by the config thread. 
			// We need to put a lock here to avoid changes to 
			// http_allowed_method[] when we are accessing it
			// in this thread.
			// Grab lock here
			NKN_MUTEX_LOCK(&http_allowed_methods.cfg_mutex);
			if (http_allowed_methods.all_methods) {
				int unk_req_len = 0;
				pd = p;
				while ( *pd !=' ' && *pd != '\n' && *pd!=0) {
					unk_req_len++;
					pd++;
				};
				if (unk_req_len > 0 ) {
					if (p[unk_req_len] != ' ') {
			 		        NKN_MUTEX_UNLOCK(&http_allowed_methods.cfg_mutex);
						// Release lock here
						break;
					}
					add_known_header(hdr, MIME_HDR_X_NKN_METHOD, 
							p, unk_req_len);
					req_line = p;
					p+=(unk_req_len+1);
			 		NKN_MUTEX_UNLOCK(&http_allowed_methods.cfg_mutex);
					// release lock
					goto parse_uri;
				}
			} else if (http_allowed_methods.method_cnt > 0){
				int i;
				for ( i = 0 ; i < MAX_HTTP_METHODS_ALLOWED; i++) {
					if (http_allowed_methods.method[i].allowed &&
						nkn_strcmp_incase(p, 
						  http_allowed_methods.method[i].name, 
						  http_allowed_methods.method[i].method_len) == 0) {

						if (p[http_allowed_methods.method[i].method_len] != ' ') {
							break;
						}
						add_known_header(hdr, 
							MIME_HDR_X_NKN_METHOD, 
							p,
							http_allowed_methods.method[i].method_len);
						req_line = p;
						p+=(http_allowed_methods.method[i].method_len + 1);
			 		        NKN_MUTEX_UNLOCK(&http_allowed_methods.cfg_mutex);
						// release lock
						goto parse_uri;
					}
				}
			}
		        NKN_MUTEX_UNLOCK(&http_allowed_methods.cfg_mutex);
			// release lock here
			phttp->respcode = 405;
			return NULL; // Not supported Method.
	}

	return nkn_skip_to_nextline(p);
}

#ifndef PROTO_HTTP_LIB
/*  This is a small parser which do extra effort to fetch HOST and URI 
    information. It should be used when there is http request parse error
    or http parser can not get the HOST and URI fields successfully. */
int http_get_host_and_uri_from_bad_req(http_cb_t *phttp)
{
        const char *p = phttp->cb_buf;
	const char *purl_start = NULL;
	const char *purl_query = NULL;
	int length = 0;
	int match = 0;
	int len = phttp->cb_totlen;
	int url_decode_req = 0;
        mime_header_t *hdr = &phttp->hdr;

	while (len && (p[0] == ' ' || p[0] == '\t')) {
            p++; len--; // skipping leading spaces
        }
        if (len <= 0) {
            return FALSE;
        }

	while (len && (p[0] == '\r' || p[0] == '\n')) {
            p++; len--; // skipping leading empty lines
        }
        if (len <= 0) {
            return FALSE;
        }

        while (len && (p[0] == ' ' || p[0] == '\t')) {
            p++; len--; // skipping leading spaces
        }
        if (len <= 0) {
            return FALSE;
        }

	switch (*(unsigned int *)p | 0x20202020) {
	case sig_connect:
	    if (strncmp(p , "CONNECT ", 8) == 0) {

		SET_HTTP_FLAG(phttp, HRF_METHOD_CONNECT);
		add_known_header(hdr, MIME_HDR_X_NKN_METHOD, p, 7);

		p += 8;
		len -= 8;
		while (len && (p[0] == ' ' || p[0] == '\t')) {
                    p++; len--;
                }
                if (len <= 0) {
                    return FALSE;
                }
		purl_start = p;
		while (len && purl_start[0] != ' ') {
                    purl_start++;
                    len--;
                }
		length = purl_start - p;
		add_known_header(hdr, MIME_HDR_X_NKN_ABS_URL_HOST, 
					    purl_start, length);
		add_known_header(hdr, MIME_HDR_X_NKN_URI, "/", 1);
		goto end;
	    }
	    break;
	case sig_options:
	    SET_HTTP_FLAG(phttp, HRF_METHOD_OPTIONS);
	    add_known_header(hdr, MIME_HDR_X_NKN_METHOD, p, 7);
	    break;
	case sig_get:
	    SET_HTTP_FLAG(phttp, HRF_METHOD_GET);
	    add_known_header(hdr, MIME_HDR_X_NKN_METHOD, p, 3);
	    break;
	case sig_head:
	    SET_HTTP_FLAG(phttp, HRF_METHOD_HEAD);
	    add_known_header(hdr, MIME_HDR_X_NKN_METHOD, p, 4);
	    break;
	case sig_post:
	    SET_HTTP_FLAG(phttp, HRF_METHOD_POST);
	    add_known_header(hdr, MIME_HDR_X_NKN_METHOD, p, 4);
	    break;
	case sig_put:
	    SET_HTTP_FLAG(phttp, HRF_METHOD_PUT);
	    add_known_header(hdr, MIME_HDR_X_NKN_METHOD, p, 3);
	    break;
	case sig_delete:
	    SET_HTTP_FLAG(phttp, HRF_METHOD_DELETE);
	    add_known_header(hdr, MIME_HDR_X_NKN_METHOD, p, 6);
	    break;
	case sig_trace:
	    SET_HTTP_FLAG(phttp, HRF_METHOD_TRACE);
	    add_known_header(hdr, MIME_HDR_X_NKN_METHOD, p, 5);
	    break;
	default:
	    NKN_MUTEX_LOCK(&http_allowed_methods.cfg_mutex);
	    if (http_allowed_methods.all_methods) {
		match = 1;
	    } else if (http_allowed_methods.method_cnt > 0) {
		int i;
		for (i = 0; i < MAX_HTTP_METHODS_ALLOWED; i++) {
		    if (http_allowed_methods.method[i].allowed && 
			    nkn_strcmp_incase(p, 
			    http_allowed_methods.method[i].name,
			    http_allowed_methods.method[i].method_len) == 0) {
			match = 1;
			break;
		    }
		}
	    }
	    NKN_MUTEX_UNLOCK(&http_allowed_methods.cfg_mutex);
	    if (match == 0) return FALSE;
	    break;
	}

	while (len && p[0] != ' ') {
            p++; len--; // moving to url section
        }
        if (len <= 0) {
            return FALSE;
        }

        while (len && (p[0] == ' ' || p[0] == '\t')) {
            p++; len--; //skipping leading spaces
        }
        if (len <= 0) {
            return FALSE;
        }

	url_decode_req = 0;
	purl_start = p;

	while (len && p[0] != ' ' && p[0] != '\n') {
            if (p[0] == '?') purl_query = p;
            if (p[0] == '%') {
                url_decode_req = 1;
            }
            p++; len--;
        }
        if (len <= 0) {
            return FALSE;
        }

	length = p - purl_start;

	if (length < 1)  return FALSE;

	if (nkn_strcmp_incase(purl_start, "http://", 7) == 0) {
		const char *phost = NULL;

		purl_start += 7; length -= 7;
		phost = purl_start;
		while (length && purl_start[0] != '/' && purl_start[0] != ' ' &&
				    purl_start != 0 && purl_start < p) {
			purl_start++;
			length--;
		}
		add_known_header(hdr, MIME_HDR_X_NKN_ABS_URL_HOST, 
					    phost, purl_start - phost);
	}
	if (nkn_strcmp_incase(purl_start, "https://", 8) == 0) {
		const char *phost = NULL;

		purl_start += 8; length -= 8; 
		phost = purl_start;
		while (length && purl_start[0] != '/' && purl_start[0] != ' ' &&
				    purl_start != 0 && purl_start < p) {
			purl_start++;
			length--;
		}
		add_known_header(hdr, MIME_HDR_X_NKN_ABS_URL_HOST, 
					    phost, purl_start - phost);
	}

	if ((purl_start && length == 0) || 
		(purl_start && purl_start[0] == '*' && 
		    CHECK_HTTP_FLAG(phttp, HRF_METHOD_OPTIONS) && length == 1)) {
		add_known_header(hdr, MIME_HDR_X_NKN_URI, "/", sizeof("/") - 1);
	} else if (purl_start && length > 0) {
		add_known_header(hdr, MIME_HDR_X_NKN_URI, purl_start, length);
	}

	if (purl_query && (p - purl_query) > 0) {
		add_known_header(hdr, MIME_HDR_X_NKN_QUERY, 
				    purl_query, (p - purl_query));
	}

	if (url_decode_req) {
		char *tbuf = NULL;
		int bytesused;

		tbuf = (char *)alloca(length + 1);
		if (!canonicalize_url(purl_start, length, 
					tbuf, length + 1, &bytesused)) {
			add_known_header(hdr, 
			    MIME_HDR_X_NKN_DECODED_URI, tbuf, bytesused - 1);
			goto end;
		}
	}

	while (len && p[0] != '\n') {
            while (len && p[0] != '\n') {
                p++; len--;
            }
            if (len && (p[1] == ' ' || p[1] == '\t')) {
                p++; len--;
            }
        } // skipping empty lines
        if (len <= 0) {
            return FALSE;
        }

	while (len) {
		int hdr_len = 0;
		const char *pend = NULL;
		const char *pstart = NULL;

		while (len && (p[0] == ' ' || p[0] == '\t')) {
                    p++; len--; // skipping leading spaces
                }
                if (len <= 0) {
                    break;
                }

		if (p[0] == '\n') { p++; len--; continue; }
		if (p[0] == '\r' || p[1] == '\n') { p += 2; len -= 2; continue; }

		/* Sanity check to validate if p is within allocated memory bounds */
		if ((p - &phttp->cb_buf[0]) >= (phttp->cb_max_buf_size - 4)) {
			break;
		}

		switch(*(unsigned int *)p | 0x20202020) {
		case sig_host:
			hdr_len = http_known_headers[MIME_HDR_HOST].namelen;
			p += hdr_len;
			len -= hdr_len;
			while (len && (p[0] == ' ' || p[0] == '\t')) {
			    p++; len--;
			}
			if (len <= 0 || p[0] != ':') return FALSE;
			p++; len--;
			while (len && (p[0] == ' ' || p[0] == '\t')) {
			    p++; len--;
			}
			if (len <= 0) return FALSE;
			pstart = p;
			while (len && p[0] != '\n') {
			    while (len && p[0] != '\n') {
				p++;
				len--;
			    }
			    if (len && (p[1] == ' ' || p[1] == '\t')) {
				p++;
				len--;
			    }
			}
			pend = p;
			if (len > 0 && pend[0] == '\n') {
			    pend--;
			}
			while (pend[0] == ' ' || pend[0] == '\t' || pend[0] == '\r') {
			    pend--;
			}
			if (((pstart - 1) <= pend) && pstart[0] != '\n') {
			    add_known_header(hdr, MIME_HDR_HOST, pstart, pend - pstart + 1);
			    goto end;
			} else {
			    return FALSE;
			}
			break;
		default:
			while (len && p[0] != ' ' && 
				p[0] != '\t' && p[0] != '\r' && p[0] != '\n') {
			    p++, len--;
			}
			continue;  // Continue on while(len)
		}

		while (len && p[0] != '\n') {
                    while (len && p[0] != '\n') {
                        p++; len--;
                    }
                    if (len && (p[1] == ' ' || p[1] == '\t')) {
                        p++; len--;
                    }
                }
	}
	return FALSE;
end:
	phttp->cb_offsetlen = p - phttp->cb_buf;
	phttp->cb_bodyoffset = phttp->cb_offsetlen;

	return TRUE;
}
#endif /* !PROTO_HTTP_LIB */

//
// Parse the HTTP request and save all necessary information into http_cb_t structure
//
// This is a simplified HTTP parser. We may change it to be lex/yacc based in the future.
// We support the following HTTP request format
//
// GET .... HTTP/1.x\r\n
// ... : ....\r\n
// ... : ....\r\n
// \r\n
//
// In order to improve the performance, we check out the signature of each line first
// before we try to do any string comparision.
//

HTTP_PARSE_STATUS http_parse_header(http_cb_t * phttp)
{
        const char * p, * ps;
	const char * p_st, * p_end;
	char * pbak = NULL;
	char * http_ver = 0;
	int len;
	int nl;
	mime_header_descriptor_t *kh = http_known_headers;
	int hit, ret;
	int parse_error = 0;
	int hdr_len = 0;
	mime_header_t *hdr = &phttp->hdr;

	if(phttp->cb_totlen < 4) {
		return HPS_NEED_MORE_DATA;
	}

	// Mark end of HTTP request
	phttp->cb_buf[phttp->cb_totlen]=0;

	//
	// search for the end of HTTP GET request
	// \r\n\r\n could be in the middle of HTTP GET request when content-length exists
	//
        len=phttp->cb_totlen;
        p=phttp->cb_buf;
	while(len>=2) {
		if (len>=4) {
			if( *p=='\r' && *(p+1)=='\n' && *(p+2)=='\r' && *(p+3)=='\n') {
				p+=4;
				goto complete;
			}
		}
		if (*p == '\n' && *(p+1) == '\n') { 
			p+=2;
			goto complete;
		}
		// Disallow request with embedded NULL(s)
		if (*p == 0) {
			// If we see \0 before \r\n\r\n, it is bad request.
			parse_error = 1;
		}
		len--;
		p++;
	}

        if (phttp->cb_totlen >= MAX_HTTP_HEADER_SIZE - 10) {
                phttp->http_hdr_len = phttp->cb_totlen;
		phttp->tunnel_reason_code = NKN_TR_REQ_MAX_SIZE_LIMIT;
                glob_http_parse_err_longheader++;
                return HPS_ERROR;
        }

	// Check for HTTP 0.9 GET request format and continue to complete.
	// HTTP 0.9 request can have a format ending with single \n as well as
	// \r\n.
	// Eg., 'GET <document_path>\n' and 'GET <document_path>\r\n'
	if (len == 1 && *p == '\n' && !CHECK_HTTP_FLAG(phttp, HRF_HTTP_RESPONSE)) {
		//Skip this case for normal HTTP/1.0 and HTTP/1.1 request cases.
		http_ver = strcasestr(phttp->cb_buf, " HTTP/1.");
		if (!http_ver) {
			p++;
			goto complete;
		}
	}

	return HPS_NEED_MORE_DATA;

complete: 
	phttp->http_hdr_len = p-phttp->cb_buf;

	if (parse_error) {
		glob_http_parse_err_nulls++;
		return HPS_ERROR;
	}

	//printf("<%s>\n", phttp->cb_buf);
	//
	// Prevent HTTP GET attack:
	// 	To avoid that client established lots of valid connections. 
	//	and these connections will send huge amount of GETs per second
	//	to overload this system CPU/Memory.
	//	We set up the max_get_rate_limit to prevent this kind of attack.
	//
	glob_tot_get_in_this_second++;
	if((int)glob_tot_get_in_this_second > max_get_rate_limit) {
		// Sorry, we have reached the limit of HTTP GET in this second.
		// We regard this GET as HTTP attack.
		// Then we will close the connection.
		DBG_LOG(MSG, MOD_HTTP, 
			"Reject GET request. It seems that we are under GET attack."
			" glob_tot_get_in_this_second=%d", glob_tot_get_in_this_second);
		glob_http_parse_err_req_rate++;
		return HPS_ERROR;
	}

	//
	// We had complete HTTP request header 
	//
        p=phttp->cb_buf;

	// Request/Response line is special.
	// It must be the first line.
	p=http_parse_request_line(phttp, p, &pbak);
	if((p==NULL) || (*p == '\0')) {
		glob_http_parse_err_badreq_5++;
		if (pbak) { free(pbak); }
        	return HPS_ERROR;
	}

	while(1) 
	{
		/* Remove leading space and tab character if there */
		p = nkn_skip_space(p);

		hit = 0;
		if(*p=='\n') { p++; break; }
		if(*p=='\r' || *(p+1)=='\n') { p+=2; break; }

		//
		// take the first four bytes as signature
		// It is case insensitive comparision
		//
		switch(*(unsigned int *)p | 0x20202020) {

		case sig_range:
			// "Rang" Range: Bytes 21010-47021
			if (nkn_strcmp_incase(p, 
					kh[MIME_HDR_RANGE].name, 
					nl=kh[MIME_HDR_RANGE].namelen)==0) {
				char * p2;
				int check = 0;
				int range_start_flag, range_stop_flag;
				const char *ptmp = NULL;
				range_start_flag = range_stop_flag = 0;
				ptmp = nkn_skip_colon_check(p+nl, &check);
				if (!check) 
				    break;

				p = ptmp;

				hit++;

				// Check for "Bytes="
				if (nkn_strcmp_incase(p, "Bytes=", 6) != 0) {
					return HPS_ERROR;
				}

				// skip "Bytes="
				p_st = p;
				p += 6;
				p = nkn_skip_space(p);
				if((*p!='-') && (*p<'0' || *p>'9')) {
					return HPS_ERROR; // MUST BE number
				}
				p2 = (char *) nkn_find_digit_end(p) + 1;
				if (*p2 == '-' || *p2 == ' ') {
					phttp->brstart = strtol(p, (char **)&ptmp, 10);
					if (phttp->brstart < 0 || p == ptmp) return HPS_ERROR;
					if (phttp->brstart == LONG_MIN || phttp->brstart == LONG_MAX)
						return HPS_ERROR;
					range_start_flag = 1;
				}
				else
					phttp->brstart = -1;
				p_end = p;
				p2 = strchr(p, '-');
				if (p2) {
					p_end = p2+1;
					p_end = nkn_skip_space(p_end);
					if((*p_end!='\r') && (*p_end!='\n') && (*p_end<'0' || *p_end>'9')) {
						return HPS_ERROR; // MUST BE number
					}
					p2 = (char *) nkn_find_digit_end(p_end) + 1;
					if (*p2 == '\r' || *p2 == ' ') {
						phttp->brstop = strtol(p_end, (char **)&ptmp, 10);
						if (phttp->brstop < 0 || p == ptmp) return HPS_ERROR;
						if (phttp->brstop == LONG_MIN || phttp->brstop == LONG_MAX)
							return HPS_ERROR;
						//Check to identify a valid stop range value in p_end variable
						if (*p_end != '\r' && *p_end != ' ')
							range_stop_flag = 1;
					}
					else if (*p2 == ',' ) {
						phttp->brstop = strtol(p_end, (char **)&ptmp, 10);
						if (phttp->brstop < 0 || ptmp == p_end) return HPS_ERROR;
						if (phttp->brstop == LONG_MIN || phttp->brstop == LONG_MAX)
							return HPS_ERROR;
						SET_HTTP_FLAG(phttp, HRF_MULTI_BYTE_RANGE);
					}
					else
						phttp->brstop = -1;
					/* three cases here: 
					 * Range: bytes=1-  ==> valid return 206 whole file - first byte
					 * Range: bytes=1-0 ==> invalid return 416
					 * Range: bytes=0-0 ==> valid return 206 with 1 byte.
					 */
					if ( (*p_end == '0') && 
					    (phttp->brstart > phttp->brstop) ) {
						phttp->brstop = -1;
						// hack set to -1. Then http_check_request() will fail.
					}
				} else {
					phttp->brstop = 0;
				}
				SET_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
				//printf("byte range: %d -- %d\n", phttp->brstart, phttp->brstop);
				if(CHECK_HTTP_FLAG(phttp, HRF_MULTI_BYTE_RANGE)) {
					p_end = p_st;
					while(*p_end!='\r' && *p_end!='\n' && *p_end!='\0') {
						p_end++;
					}
					p_end--; // because of add_known_header length.
				}
				else {
					p_end = nkn_find_digit_end(p_end);
				}
				if (range_start_flag && range_stop_flag)
					SET_HTTP_FLAG(phttp, HRF_BYTE_RANGE_START_STOP);
				add_known_header(hdr, MIME_HDR_RANGE, p_st,
						p_end-p_st+1);
			}
			break;

		//case sig_connect:
		case sig_connection:
			// Connection: Keep-Alive
			// Connection: Close
			if(nkn_strcmp_incase(p, 
			    	kh[MIME_HDR_CONNECTION].name,
			    	nl=kh[MIME_HDR_CONNECTION].namelen)==0) {
				int  check = 0;
				const char * ptmp = NULL;

				ptmp = nkn_skip_colon_check(p+nl, &check);
				if (!check) 
				    break;
				p = ptmp;
				hit++;

                                p_end = nkn_find_hdrvalue_end(p);
                              
                               /*     
                                * Case to handle mutliple values coming as part
                                * of 'Connection:' header value list.
                                * For eg: 'Connection: foo-bar-1,foo-bar-2, Keep-Alive'
                                * p     = Always point to each token in the value list.
                                * p_end = points to the connection value list end,
                                *         ie, to the char after 'e' in 'Keep-Alive'.
                                * ptmp  = Used to remove ' ', ',' or '\t' chars. 
                                */                     
                                while(p && ptmp && p <= p_end) {
                                    ptmp = nkn_comma_delim_start(p, p_end) ;
                                    if(nkn_strcmp_incase(p, "close", 5)==0) {
                                        CLEAR_HTTP_FLAG(phttp, HRF_CONNECTION_KEEP_ALIVE);
                                    }
                                    else if(nkn_strcmp_incase(p, "keep-alive", 10)==0) {
                                        SET_HTTP_FLAG(phttp, HRF_CONNECTION_KEEP_ALIVE);
                                    }
                                    if (ptmp) {
                                        add_known_header(hdr, MIME_HDR_CONNECTION,
                                                         p, ptmp - p);
                                        p = nkn_skip_comma_space(ptmp) ;
                                    }
                                }
			}
			break;

		case sig_content_length:
#ifdef COMMENT
		case sig_content_type:
		case sig_content_base:
		case sig_content_disposition:
		case sig_content_encoding:
		case sig_content_language:
		case sig_content_location:
		case sig_content_md5:
		case sig_content_range:
#endif
                        // "Cont" Content-Length: 47022
                        if(nkn_strcmp_incase(p, 
			    	kh[MIME_HDR_CONTENT_LENGTH].name,
			    	nl=kh[MIME_HDR_CONTENT_LENGTH].namelen)==0){
				int  check = 0;
				const char * ptmp = NULL;

				ptmp = nkn_skip_colon_check(p+nl, &check);
				if (!check) 
					break;
				p = ptmp;
				hit++;

				SET_HTTP_FLAG(phttp, HRF_CONTENT_LENGTH);
				phttp->content_length = strtol(p, (char **)&p_end, 10);
				if (phttp->content_length < 0 ||
				    p == p_end ||
				    phttp->content_length == LONG_MIN ||
				    phttp->content_length == LONG_MAX) {
					if (pbak) { free(pbak); }
					return HPS_ERROR;// Content-Length is negative

				}
				//printf("byte range: %d -- %d\n", con->req.brstart, con->req.brstop);
				p_end = nkn_find_digit_end(p);
				add_known_header(hdr, MIME_HDR_CONTENT_LENGTH, p,
							p_end-p+1);
                        } else if(add_if_match(hdr, MIME_HDR_CONTENT_TYPE, 
						p, &hit)) {
                        } else if(add_if_match(hdr, MIME_HDR_CONTENT_BASE,
						p, &hit)) {
			} else if(add_if_match(hdr, MIME_HDR_CONTENT_DISPOSITION,
						p, &hit)) {
			} else if(add_if_match(hdr, MIME_HDR_CONTENT_ENCODING,
						p, &hit)) {
				get_encoding_type(phttp, MIME_HDR_CONTENT_ENCODING, p);
			} else if(add_if_match(hdr, MIME_HDR_CONTENT_LANGUAGE,
						p, &hit)) {
			} else if(add_if_match(hdr, MIME_HDR_CONTENT_LOCATION,
						p, &hit)) {
			} else if(add_if_match(hdr, MIME_HDR_CONTENT_MD5,
						p, &hit)) {
			} else if(add_if_match(hdr, MIME_HDR_CONTENT_RANGE,
						p, &hit)) {
			}
                        break;

		case sig_transfer_encoding:
			// Tranfer-Encoding: chunked
                        if(nkn_strcmp_incase(p, 
				kh[MIME_HDR_TRANSFER_ENCODING].name,
				nl=kh[MIME_HDR_TRANSFER_ENCODING].namelen)==0) {
				int  check = 0;
				const char * ptmp = NULL;
				ptmp = nkn_skip_colon_check(p+nl, &check);
				if (!check)
				    break;

				p = ptmp;

				hit++;
				if(nkn_strcmp_incase(p, "chunked", 7)==0) {
					SET_HTTP_FLAG(phttp, HRF_TRANSCODE_CHUNKED);
				}
				p_end = nkn_find_hdrvalue_end(p);
				add_known_header(hdr, MIME_HDR_TRANSFER_ENCODING, p,
						p_end-p+1);
                        }
			break;

		case sig_cookie:
			// Cookie: cookie_name = cookie_value
			if(nkn_strcmp_incase(p, 
				kh[MIME_HDR_COOKIE2].name, 
				nl=kh[MIME_HDR_COOKIE2].namelen)==0) {
				int check = 0;
				const char * ptmp = NULL;

				ptmp = nkn_skip_colon_check(p+nl, &check);
				if (!check)
				    break;

				p = ptmp;
				hit++;
				ps=p;
				while(*p!='\r' && *p!='\n') {
					if(*p==0) {
						glob_http_parse_err_longcookie++;
						if (pbak) { free(pbak); }
						return HPS_ERROR;// COokie is too long
					}
					p++; 
				}
				//printf("cookie: len=%d\n", len);

				add_known_header(hdr, MIME_HDR_COOKIE2, ps, p-ps);
                        } else if(nkn_strcmp_incase(p, 
				kh[MIME_HDR_COOKIE].name, 
				nl=kh[MIME_HDR_COOKIE].namelen)==0) {
				int check = 0;
				const char * ptmp = NULL;

				ptmp = nkn_skip_colon_check(p+nl, &check);
				if (!check)
				    break;

				p = ptmp;
				hit++;
				ps=p;
				while(*p!='\r' && *p!='\n') {
					if(*p==0) {
						glob_http_parse_err_longcookie++;
						if (pbak) { free(pbak); }
						return HPS_ERROR;// COokie is too long
					}
					p++; 
				}
				//printf("cookie: len=%d\n", len);

				add_known_header(hdr, MIME_HDR_COOKIE, ps, p-ps);
                        }
			break;

		case sig_accept:
#ifdef COMMENT
		case sig_accept_charset:
		case sig_accept_encoding:
		case sig_accept_language:
		case sig_accept_ranges:
#endif
                        if(add_if_match(hdr, MIME_HDR_ACCEPT, 
						p, &hit)) {
			} else if(add_if_match(hdr, MIME_HDR_ACCEPT_CHARSET,
						p, &hit)) {
			} else if(add_if_match(hdr, MIME_HDR_ACCEPT_ENCODING,
						p, &hit)) {
				get_encoding_type(phttp, MIME_HDR_ACCEPT_ENCODING, p);
				glob_accept_encoding++;
			} else if(add_if_match(hdr, MIME_HDR_ACCEPT_LANGUAGE,
						p, &hit)) {
			} else if(add_if_match(hdr, MIME_HDR_ACCEPT_RANGES,
						p, &hit)) {
			}
			break;

		case sig_allow:
                        if(add_if_match(hdr, MIME_HDR_ALLOW, 
					p, &hit)) {
			}
			break;

		case sig_authentication_info:
#ifdef COMMENT
		case sig_authorization:
#endif
                        if(add_if_match(hdr, MIME_HDR_AUTHENTICATION_INFO,
					p, &hit)) {
			} else if(add_if_match(hdr, MIME_HDR_AUTHORIZATION,
					p, &hit)) {
			}
			break;

		case sig_cache_control:
                        if(add_if_match(hdr, MIME_HDR_CACHE_CONTROL,
					p, &hit)) {
			}
			break;

		case sig_date:
			add_kheader(hdr, MIME_HDR_DATE, p, &hit);
			break;

		case sig_etag:
			add_kheader(hdr, MIME_HDR_ETAG, p, &hit);
			break;

		case sig_expect:
                        if(add_if_match(hdr, MIME_HDR_EXPECT,
                                       p, &hit) == -2) {
                            phttp->respcode = 417 ;
                            if (pbak) { free(pbak); }
                            return HPS_ERROR;
			}
			break;

		case sig_expires:
                        if(add_if_match(hdr, MIME_HDR_EXPIRES,
					p, &hit)) {
			}
			break;

		case sig_from:
			add_kheader(hdr, MIME_HDR_FROM, p, &hit);
			break;

		case sig_host:
			hit++;
			add_kheader(hdr, MIME_HDR_HOST, p, &hit);
			break;

		case sig_if_match:
#ifdef COMMENT
		case sig_if_modified_since:
#endif
                        if(add_if_match(hdr, MIME_HDR_IF_MATCH,
					p, &hit)) {
			} else if(add_if_match(hdr, MIME_HDR_IF_MODIFIED_SINCE,
					p, &hit)) {
			}
			break;

		case sig_if_none_match:
                        if(add_if_match(hdr, MIME_HDR_IF_NONE_MATCH,
					p, &hit)) {
			}
			break;

		case sig_if_range:
                        if(add_if_match(hdr, MIME_HDR_IF_RANGE,
					p, &hit)) {
			}
			break;

		case sig_if_unmodfied_since:
                        if(add_if_match(hdr, MIME_HDR_IF_UNMODIFIED_SINCE,
					p, &hit)) {
			}
			break;

		case sig_keep_alive:
                        if(add_if_match(hdr, MIME_HDR_KEEP_ALIVE,
					p, &hit)) {
			}
			break;

		case sig_last_modified:
                        if(add_if_match(hdr, MIME_HDR_LAST_MODIFIED,
					p, &hit)) {
			}
			break;

		case sig_link:
			add_kheader(hdr, MIME_HDR_LINK, p, &hit);
			break;

		case sig_location:
                        if(add_if_match(hdr, MIME_HDR_LOCATION,
					p, &hit)) {
			}
			break;

		case sig_max_forwards:
                        if(add_if_match(hdr, MIME_HDR_MAX_FORWARDS,
					p, &hit)) {
			}
			break;

		case sig_mime_version:
                        if(add_if_match(hdr, MIME_HDR_MIME_VERSION,
					p, &hit)) {
			}
			break;

		case sig_pragma:
                        if(add_if_match(hdr, MIME_HDR_PRAGMA,
					p, &hit)) {
			}
			break;

		case sig_proxy_authenticate:
#ifdef COMMENT
		case sig_proxy_authentication_info:
		case sig_proxy_authorization:
		case sig_proxy_connection:
#endif
                        if(add_if_match(hdr, MIME_HDR_PROXY_AUTHENTICATE,
					p, &hit)) {
			} else if(add_if_match(hdr, 
					MIME_HDR_PROXY_AUTHENTICATION_INFO,
					p, &hit)) {
			} else if(add_if_match(hdr, MIME_HDR_PROXY_AUTHORIZATION,
					p, &hit)) {
			} else if(nkn_strcmp_incase(p,
                                kh[MIME_HDR_PROXY_CONNECTION].name,
                                nl=kh[MIME_HDR_PROXY_CONNECTION].namelen)==0) {
                                int  check = 0;
                                const char * ptmp = NULL;

                                ptmp = nkn_skip_colon_check(p+nl, &check);
                                if (!check)
                                    break;
                                p = ptmp;
                                hit++;

                                if(nkn_strcmp_incase(p, "close", 5)==0) {
                                        CLEAR_HTTP_FLAG(phttp, HRF_CONNECTION_KEEP_ALIVE);
                                }
                                else if(nkn_strcmp_incase(p, "keep-alive", 10)==0) {
                                        SET_HTTP_FLAG(phttp, HRF_CONNECTION_KEEP_ALIVE);
                                }
                                p_end = nkn_find_hdrvalue_end(p);
                                if (p_end) {
                                        add_known_header(hdr, MIME_HDR_PROXY_CONNECTION, p,
                                                         p_end-p+1);
                                }
                        }
                        break;
		case sig_public:
                        if(add_if_match(hdr, MIME_HDR_PUBLIC, 
					p, &hit)) {
			}
			break;

		case sig_request_range:
                        if(add_if_match(hdr, MIME_HDR_REQUEST_RANGE,
					p, &hit)) {
			}
			break;

		case sig_referer:
                        if(add_if_match(hdr, MIME_HDR_REFERER,
					p, &hit)) {
			}
			break;

		case sig_retry_after:
                        if(add_if_match(hdr, MIME_HDR_RETRY_AFTER,
					p, &hit)) {
			}
			break;

		case sig_server:
                        if(add_if_match(hdr, MIME_HDR_SERVER,
					p, &hit)) {
			}
			break;

		case sig_set_cookie:
                        if(add_if_match(hdr, MIME_HDR_SET_COOKIE,
					p, &hit)) {
			} else if(add_if_match(hdr, MIME_HDR_SET_COOKIE2,
					p, &hit)) {
			}
			break;

		case sig_trailer:
                        if(add_if_match(hdr, MIME_HDR_TRAILER,
					p, &hit)) {
			}
			break;

		case sig_upgrade:
                        if(add_if_match(hdr, MIME_HDR_UPGRADE,
					p, &hit)) {
			}
			break;

		case sig_user_agent:
                        if(add_if_match(hdr, MIME_HDR_USER_AGENT,
					p, &hit)) {
			}
			break;
	
		case sig_vary:
			add_kheader(hdr, MIME_HDR_VARY, p, &hit);
			break;

		case sig_warning:
                        if(add_if_match(hdr, MIME_HDR_WARNING,
					p, &hit)) {
			}
			break;

		case sig_www_authenticate:
                        if(add_if_match(hdr, MIME_HDR_WWW_AUTHENTICATE,
					p, &hit)) {
				SET_HTTP_FLAG(phttp, HRF_WWW_AUTHENTICATE);
			}
			break;
#if 0
		/*
		 * Right now mapping between X-Accel-Cache-Control and 
		 * MIME_HDR_X_ACCEL_CACHE_CONTROL is done statically. 
		 * Later on mapping will be done dynamically.
		 */
		case sig_x_accel:
			switch(*(unsigned int *)(p+8) | 0x20202020) {
			case sig_cache_control:
				if(add_if_match(hdr, MIME_HDR_X_ACCEL_CACHE_CONTROL,
							p, &hit)) {
				}
				break;
			}
			break;

#endif
		default:
			if (phttp->nsconf) {
				hdr_len = phttp->nsconf->http_config->policies.cache_redirect.hdr_len;
				if (hdr_len) {
					char *hdr_name = phttp->nsconf->
						http_config->policies.cache_redirect.hdr_name;
					add_if_custom_name_match(hdr, MIME_HDR_X_ACCEL_CACHE_CONTROL, p, 
									hdr_name, hdr_len, &hit);
				}
			}

                        switch(*(unsigned int *)p | 0x20200020) { // to match "X-NK" case insensitive.
                            case sig_internal:
                                  ret = add_if_match(hdr, MIME_HDR_X_INTERNAL, p, &hit);
                                  if ( ret == -3) { /* This is a prefetch request */
                                      SET_HTTP_FLAG(phttp, HRF_CLIENT_INTERNAL);
                                  } else if ( ret == -4) { /* This is a crawl request */
                                      SET_CRAWL_REQ(phttp);
                                  } else if (add_if_match(hdr, 
				  	MIME_HDR_X_NKN_INCLUDE_ORIG, p, &hit)) {
				  } else if (add_if_match(hdr, 
				  	MIME_HDR_X_NKN_CL7_PROXY, p, &hit)) {
                                  }  
                                  break; 

                            default:
                                  break;
                        }
			break;
		}

		if (!hit) {
			// Check for < 4 byte headers
			if(add_if_match(hdr, MIME_HDR_AGE,
					p, &hit)) {
			} else if(add_if_match(hdr, MIME_HDR_TE,
					p, &hit)) {
			} else if(add_if_match(hdr, MIME_HDR_VIA,
					p, &hit)) {
			} else {
				// Unknown header
				const char *pname, *pname_end;
				const char *pvalue, *pvalue_end;
				const char *ptmp;
				int check = 0;

				pname = nkn_skip_space(p);
				pname_end = nkn_find_hdrname_end(pname);
				ptmp = pname_end;
				if (*ptmp != '\n') ptmp++;

				pvalue = nkn_skip_colon_check(ptmp, &check);
				if(!check){
					glob_http_parse_err_badreq_4++;
					if (pbak) { free(pbak); }
        				return HPS_ERROR;
				}

				pvalue_end = nkn_find_hdrvalue_end(pvalue);

				if (*pvalue_end == '\n') {
					glob_http_parse_err_badreq_4++;
					if (pbak) { free(pbak); }
        				return HPS_ERROR;
				}

				if ( (pvalue_end==pvalue) && (*pvalue=='\n' || *pvalue=='\r') ) {
					/* Skip case: Cache-Control: \r\n" */
					append_add_unknown_header(hdr,
					    	pname, (pname_end-pname)+1,
						" ", 1);
				}
				else {
					append_add_unknown_header(hdr,
					    	pname, (pname_end-pname)+1,
						pvalue, (pvalue_end-pvalue)+1);
				}
			}
		}
		p=nkn_skip_to_nextline(p);
		if(p==NULL) {
			glob_http_parse_err_badreq_5++;
			if (pbak) { free(pbak); }
        		return HPS_ERROR;
		}
	}
        if (CHECK_HTTP_FLAG(phttp, HRF_METHOD_CONNECT)) {
                phttp->cb_offsetlen = p - pbak;
        } else {
                phttp->cb_offsetlen=p-phttp->cb_buf;
        }
	phttp->cb_bodyoffset=phttp->cb_offsetlen;
	SET_HTTP_FLAG(phttp, HRF_PARSE_DONE);

	if (pbak) { free(pbak); }

	if( CHECK_HTTP_FLAG(phttp, HRF_HTTP_10) && 
	   	CHECK_HTTP_FLAG(phttp, HRF_CONNECTION_KEEP_ALIVE)) {
		/*
		 * 1. HTTP 1.0 , Connection: Keep-ALive
		 */
		SET_HTTP_FLAG(phttp, HRF_CONNECTION_KEEP_ALIVE);
	}
	else if( CHECK_HTTP_FLAG(phttp, HRF_HTTP_11) && 
	   (CHECK_HTTP_FLAG(phttp, HRF_CONNECTION_KEEP_ALIVE) || 
	    ( !is_known_header_present(&phttp->hdr, MIME_HDR_CONNECTION) &&
	      !is_known_header_present(&phttp->hdr, MIME_HDR_PROXY_CONNECTION) )
	   )) {
		/*
		 * 1. HTTP 1.1 , Connection: Keep-Alive
		 * 2. HTTP 1.1, No Connection header
		 */
		SET_HTTP_FLAG(phttp, HRF_CONNECTION_KEEP_ALIVE);
	}
	else {
		/* 
		 * 1. HTTP 1.0 , No Connection header 
		 * 2. HTTP 1.1 , Connection: Close
		 * 3. HTTP 1.0 , Connection: Close
		 */
		CLEAR_HTTP_FLAG(phttp, HRF_CONNECTION_KEEP_ALIVE);
	}

        return HPS_OK;
}

#ifndef PROTO_HTTP_LIB
//
// This function is called when this HTTP module is used as server
// such as lighttpd
//
int http_check_request(http_cb_t * phttp)
{
	int rv;
	u_int32_t attrs;
	int hdrcnt;
	const char *data;
	int datalen;

	// Validate "Host:" header format
	rv = get_known_header(&phttp->hdr, MIME_HDR_HOST, &data, &datalen, 
			      &attrs, &hdrcnt);
	if (!rv) {
		if (!valid_FQDN(data, datalen)) {
			//http_build_res_400(phttp, NKN_HTTP_BAD_HOST_HEADER);
			phttp->respcode = 400;
			phttp->subcode = NKN_HTTP_BAD_HOST_HEADER;
			phttp->tunnel_reason_code = NKN_TR_REQ_HTTP_BAD_HOST_HEADER;
			DBG_LOG(MSG, MOD_HTTP, 
				"http request error - subcode %d", 
					NKN_HTTP_BAD_HOST_HEADER);
			return FALSE;
		}
	}

	// Validate absolute URL host
	rv = get_known_header(&phttp->hdr, MIME_HDR_X_NKN_ABS_URL_HOST, &data, 
			      &datalen, &attrs, &hdrcnt);
	if (!rv) {
		if (!valid_FQDN(data, datalen)) {
			//http_build_res_400(phttp, NKN_HTTP_BAD_URL_HOST_HEADER);
			phttp->respcode = 400;
			phttp->subcode = NKN_HTTP_BAD_URL_HOST_HEADER;
			phttp->tunnel_reason_code = NKN_TR_REQ_HTTP_BAD_URL_HOST_HEADER;
			DBG_LOG(MSG, MOD_HTTP, 
				"http reqeust error - subcode %d", 
					NKN_HTTP_BAD_URL_HOST_HEADER);
			return FALSE;
		}
	}

	// Get the response body total length
	if (!is_known_header_present(&phttp->hdr, MIME_HDR_X_NKN_URI)) {
		//http_build_res_400(phttp, NKN_HTTP_URI);
		phttp->respcode = 400;
		phttp->subcode = NKN_HTTP_URI;
		phttp->tunnel_reason_code = NKN_TR_REQ_HTTP_URI;
		DBG_LOG(MSG, MOD_HTTP, 
			"http request error - subcode (%d)", NKN_HTTP_URI);
		return FALSE;
	}

	// Case Range: bytes=101-
	if( CHECK_HTTP_FLAG(phttp, HRF_BYTE_RANGE) ) {
		// Some Sanity check
		if(phttp->brstart<0 || phttp->brstop<0) {
			//http_build_res_416(phttp, NKN_HTTP_REQ_RANGE_1);
			phttp->respcode = 416;
			phttp->subcode = NKN_HTTP_REQ_RANGE_1;
			phttp->tunnel_reason_code = NKN_TR_REQ_HTTP_REQ_RANGE1;
			DBG_LOG(MSG, MOD_HTTP, 
				"http request error - subcode (%d)", NKN_HTTP_REQ_RANGE_1);
			return FALSE;
		}

		if(phttp->brstop && (phttp->brstart > phttp->brstop)) {
			//http_build_res_416(phttp, NKN_HTTP_REQ_RANGE_2);
			phttp->respcode  = 416;
			phttp->subcode = NKN_HTTP_REQ_RANGE_2;
			phttp->tunnel_reason_code = NKN_TR_REQ_HTTP_REQ_RANGE2;
			DBG_LOG(MSG, MOD_HTTP, 
				"http request error - subcode (%d)", NKN_HTTP_REQ_RANGE_2);
			return FALSE;
		}
	}

	// Check out version
	if( !CHECK_HTTP_FLAG(phttp, HRF_HTTP_10) &&
	    !CHECK_HTTP_FLAG(phttp, HRF_HTTP_11) ) {
		//http_build_res_505(phttp, NKN_HTTP_UNSUPPORT_VERSION);
		phttp->respcode = 505;
		phttp->subcode = NKN_HTTP_UNSUPPORT_VERSION;
		phttp->tunnel_reason_code = NKN_TR_REQ_HTTP_UNSUPP_VER;
		DBG_LOG(MSG, MOD_HTTP, 
			"http request error - subcode (%d)", NKN_HTTP_UNSUPPORT_VERSION);
		return FALSE;
	}

	// RFC 2616: Section 14.23
	if( CHECK_HTTP_FLAG(phttp, HRF_HTTP_11) && 
	   !is_known_header_present(&phttp->hdr, MIME_HDR_HOST)) {
		//http_build_res_400(phttp, NKN_HTTP_NO_HOST_HTTP11);
		phttp->respcode = 400;
		phttp->subcode = NKN_HTTP_NO_HOST_HTTP11;
		phttp->tunnel_reason_code = NKN_TR_REQ_HTTP_NO_HOST_HTTP11;
		DBG_LOG(MSG, MOD_HTTP, 
			"http request error - subcode (%d)", NKN_HTTP_NO_HOST_HTTP11);
		return FALSE;
	}

	// Check out Connection: keep-alive
	if( CHECK_HTTP_FLAG(phttp, HRF_HTTP_10) && 
	   	CHECK_HTTP_FLAG(phttp, HRF_CONNECTION_KEEP_ALIVE)) {
		/*
		 * 1. HTTP 1.0 , Connection: Keep-ALive
		 */
		SET_HTTP_FLAG(phttp, HRF_CONNECTION_KEEP_ALIVE);
	}
	else if( CHECK_HTTP_FLAG(phttp, HRF_HTTP_11) && 
	   (CHECK_HTTP_FLAG(phttp, HRF_CONNECTION_KEEP_ALIVE) || 
	    ( !is_known_header_present(&phttp->hdr, MIME_HDR_CONNECTION) &&
	      !is_known_header_present(&phttp->hdr, MIME_HDR_PROXY_CONNECTION) )
	   )) {
		/*
		 * 1. HTTP 1.1 , Connection: Keep-Alive
		 * 2. HTTP 1.1, No Connection header
		 */
		SET_HTTP_FLAG(phttp, HRF_CONNECTION_KEEP_ALIVE);
	}
	else {
		/* 
		 * 1. HTTP 1.0 , No Connection header 
		 * 2. HTTP 1.1 , Connection: Close
		 * 3. HTTP 1.0 , Connection: Close
		 */
		CLEAR_HTTP_FLAG(phttp, HRF_CONNECTION_KEEP_ALIVE);
	}
#if 0
	/* Commenting out this, as this is already handled in 
	 * http_check_mfd_limitation, a common place to check
	 * for all origin providers
	 */
	//Chuncked Encoding
	if( !CHECK_HTTP_FLAG(phttp, HRF_METHOD_POST) && 
			CHECK_HTTP_FLAG(phttp, HRF_TRANSCODE_CHUNKED)){
		//http_build_res_400(phttp, NKN_HTTP_ERR_CHUNKED_REQ);
		phttp->respcode = 400;
		phttp->subcode = NKN_HTTP_ERR_CHUNKED_REQ;
		phttp->tunnel_reason_code = NKN_TR_REQ_HTTP_ERR_CHUNKED_REQ;
		DBG_LOG(MSG, MOD_HTTP, 
			"http request error - subcode (%d)", NKN_HTTP_ERR_CHUNKED_REQ);
                return FALSE;
	}

       /* Just needed for debugging. Hence commenting out this block now. */
        {
            int    via_listsz = 4096;
            char * via_list   = alloca(via_listsz);

            via_list[0] = '\0';
            concatenate_known_header_values(&phttp->hdr,
                                             MIME_HDR_CONNECTION,
                                             via_list, via_listsz) ;
            DBG_LOG(MSG, MOD_HTTP,
                    "HTTP-request: Connection header values:"
                    " '%s'\n", via_list);
        }
#endif
       /*
        * Remove the unknown headers which are from the connection-token
        * list. For eg:- if there are 2 headers like,
        *                "Connection: foo-bar\r\n" \
        *                "foo-bar: value-1\r\n"
        * Then MFD should not fwd header "foo-bar: value-1" back to client.
        * BUG: 3428.
        */
        http_hdr_block_connection_token(&phttp->hdr) ;

	// Success
	return TRUE;
}

int http_check_mfd_limitation(http_cb_t * phttp, uint64_t pe_action)
{
	const char *data;
	int datalen;
	u_int32_t attr;
	int header_cnt;
	int ret;
	const namespace_config_t *nsconf;
	origin_server_select_method_t oss_method = OSS_UNDEFINED;
	int cl7_proxy = 0;

	nsconf = phttp->nsconf;
	if (nsconf != NULL) {
		oss_method = nsconf->http_config->origin.select_method;
		cl7_proxy = (oss_method == OSS_HTTP_PROXY_CONFIG);
	}

	if((!cl7_proxy && (tunnel_only_enable == 1)) ||
	    ((phttp->nsconf != NULL) &&
	     (phttp->nsconf->http_config->request_policies.tunnel_all
	      == NKN_TRUE) && !(pe_action & PE_ACTION_REQ_CACHE_OBJECT))) {
		glob_tunnel_req_tunnel_only++;
		phttp->tunnel_reason_code = NKN_TR_REQ_TUNNEL_ONLY;
		goto limit_fail;
	}

	// Check if its an internal crawl request
	if (CHECK_CRAWL_REQ(phttp)) {
		glob_tunnel_req_int_crawl_request++;
		phttp->tunnel_reason_code = NKN_TR_REQ_INT_CRAWL_REQ;
		goto limit_fail;
	}

	// Only support GET and HEAD for PROXY_REVERSE and PROXY_VIRTUAL mode
	//
	// BZ 2328: remove check for HRF_METHOD_HEAD - HEAD requests 
	// are also tunnelled
	//
	// BZ 5245 : HEAD requests are not longer tunneled. 
	if( !CHECK_HTTP_FLAG(phttp, HRF_METHOD_GET) &&
			!CHECK_HTTP_FLAG(phttp, HRF_METHOD_HEAD)) {
		glob_tunnel_req_not_get++;
		phttp->tunnel_reason_code = NKN_TR_REQ_NOT_GET;
		goto limit_fail;
	}


	// Check if requested content-type is allowed
	get_known_header(&phttp->hdr, MIME_HDR_X_NKN_URI, &data, &datalen,
			&attr, &header_cnt);
	if (!allow_content_type(data, datalen) && 
		!(pe_action & PE_ACTION_REQ_CACHE_OBJECT)) {
		glob_tunnel_req_not_allow_content_type++;
		phttp->tunnel_reason_code = NKN_TR_REQ_UNSUPP_CONT_TYPE;
		goto limit_fail;
	}

	// MUST: uri size < Max URI Size (256 for now)
	if(datalen >= (MAX_URI_SIZE - MAX_URI_HDR_SIZE)) {
		glob_tunnel_req_uri_size++;
		phttp->tunnel_reason_code = NKN_TR_REQ_BIG_URI;
		goto limit_fail;
	}

	// Check out Content-Length is zero if present.
	if (phttp->content_length) {
		glob_tunnel_req_content_length++;
		phttp->tunnel_reason_code = NKN_TR_REQ_CONT_LEN;
		goto limit_fail;
	}

	// Check if chunked encoding.
	if (CHECK_HTTP_FLAG(phttp, HRF_TRANSCODE_CHUNKED)) {
		glob_tunnel_req_chunked_encode++;
		phttp->tunnel_reason_code = NKN_TR_REQ_CHUNK_ENCODED;
		goto limit_fail;
	}

	// Check if additional data is present after header
	if (phttp->http_hdr_len != phttp->cb_totlen) {
		if (CHECK_HTTP_FLAG(phttp, HRF_CONTENT_LENGTH)) {
		    // If Content-Length Header present, it is the same
		    // request and we dont handle it
		    glob_tunnel_req_hdr_len++;
		    phttp->tunnel_reason_code = NKN_TR_REQ_HDR_LEN;
		    goto limit_fail;
		}
	}

	// Check if Multi byte range request.
	if (CHECK_HTTP_FLAG(phttp, HRF_MULTI_BYTE_RANGE)) {
		glob_tunnel_req_multi_bye_range++;
		phttp->tunnel_reason_code = NKN_TR_REQ_MULT_BYTE_RANGE;
		goto limit_fail;
	}

	/* If the URI had hex encoding, process the request through MFD,
	 * but tunnel the response, so that the request headers could be modified
	 * by MFD. The check below is moved to receive side.
	 * BZ 3731
	 */
#if 0	 
	// Check if URI has hex encoded characters.
	if (CHECK_HTTP_FLAG(phttp, HRF_HAS_HEX_ENCODE)) {
		goto limit_fail;
	}
#endif
	// Check if Authorization header is present
	if(is_known_header_present(&phttp->hdr, MIME_HDR_AUTHORIZATION)) {
		glob_tunnel_req_authorization++;
		if (!CHECK_TR_REQ_OVERRIDE(phttp->nsconf->tunnel_req_override, NKN_TR_REQ_AUTH_HDR)) {
			phttp->tunnel_reason_code = NKN_TR_REQ_AUTH_HDR;
			goto limit_fail;
		}
	}

	if (nsconf != NULL) {
		if (nsconf->http_config->request_policies.tunnel_req_with_cookie &&
		    (is_known_header_present(&phttp->hdr, MIME_HDR_COOKIE) ||
		     is_known_header_present(&phttp->hdr, MIME_HDR_COOKIE2)) &&
		     !(pe_action & PE_ACTION_REQ_CACHE_OBJECT)) {
			glob_tunnel_req_cookie++;
			if (!CHECK_TR_REQ_OVERRIDE(phttp->nsconf->tunnel_req_override, NKN_TR_REQ_COOKIE)) {
				phttp->tunnel_reason_code = NKN_TR_REQ_COOKIE;
				goto limit_fail;
			}
		}

		/* BUG 5054
		 * If ssp config is not present in T-Proxy namespace
		 * 1) Query parameter exists
		 * 2) No virtual player configured
		 * 3) T-Proxy Namespace only
		 * 4) cache query parameter request is disabled
		 */
		if (CHECK_HTTP_FLAG(phttp, HRF_HAS_QUERY) &&
		    ((nsconf->ssp_config == 0) ||
		     (nsconf->ssp_config->player_type == -1)) &&
		    (nsconf->http_config &&
		     (nsconf->http_config->policies.disable_cache_on_query == NKN_TRUE)) &&
		    !(pe_action & PE_ACTION_REQ_CACHE_OBJECT)
		   ) {
			glob_tunnel_req_query_str++;
			if (!CHECK_TR_REQ_OVERRIDE(phttp->nsconf->tunnel_req_override, NKN_TR_REQ_QUERY_STR)) {
				phttp->tunnel_reason_code = NKN_TR_REQ_QUERY_STR;
				goto limit_fail;
			}
		}
	}

	// Client Cache-Control: no-cache request
	if ((!cl7_proxy || CHECK_HTTP_FLAG(phttp, HRF_METHOD_HEAD)) &&
	    is_known_header_present(&phttp->hdr, MIME_HDR_CACHE_CONTROL)) {
		const cooked_data_types_t *ckdp;
		int ckd_len;
		mime_hdr_datatype_t dtype;

		ret = get_cooked_known_header_data(&phttp->hdr,
					MIME_HDR_CACHE_CONTROL, &ckdp, &ckd_len,
					&dtype);
		if (!ret && (dtype == DT_CACHECONTROL_ENUM) ) {
			if (((ckdp->u.dt_cachecontrol_enum.mask & CT_MASK_NO_CACHE) || 
			    (ckdp->u.dt_cachecontrol_enum.mask & CT_MASK_NO_STORE)) &&
			    !(pe_action & PE_ACTION_REQ_CACHE_OBJECT)) {
				if ((oss_method == OSS_HTTP_NODE_MAP) &&
				    (!CHECK_HTTP_FLAG(phttp, HRF_METHOD_HEAD))) {
					http_set_no_cache_request(phttp);
				} else {
					glob_tunnel_req_cache_ctrl_no_cache++;
					if (!CHECK_TR_REQ_OVERRIDE(phttp->nsconf->tunnel_req_override, NKN_TR_REQ_CC_NO_CACHE)) {
						phttp->tunnel_reason_code = NKN_TR_REQ_CC_NO_CACHE;
						goto limit_fail;
					}
				}
			}
		}
	}

	// BUG 3555: Client request "Pragma: no-cache" is present get object from origin
	if ((!cl7_proxy || CHECK_HTTP_FLAG(phttp, HRF_METHOD_HEAD)) &&
	    is_known_header_present(&phttp->hdr, MIME_HDR_PRAGMA)) {
		ret = get_known_header(&phttp->hdr, MIME_HDR_PRAGMA,
				&data, &datalen,
				&attr, &header_cnt);
		if ((ret == 0) && (header_cnt == 1) &&
		    (strncasecmp(data, "no-cache", datalen) == 0) &&
		    !(pe_action & PE_ACTION_REQ_CACHE_OBJECT)) {
			if ((oss_method == OSS_HTTP_NODE_MAP) &&
			    (!CHECK_HTTP_FLAG(phttp, HRF_METHOD_HEAD))) {
				http_set_no_cache_request(phttp);
			} else {
				glob_tunnel_req_pragma_no_cache++;
				if (!CHECK_TR_REQ_OVERRIDE(phttp->nsconf->tunnel_req_override, NKN_TR_REQ_PRAGMA_NO_CACHE)) {
					phttp->tunnel_reason_code = NKN_TR_REQ_PRAGMA_NO_CACHE;
					goto limit_fail;
				}
			}
		}
	}

	if (phttp->http_encoding_type == HRF_ENCODING_UNKNOWN) {
		    phttp->tunnel_reason_code = NKN_TR_REQ_NOT_SUPPORTED_ENCODING;
		    goto limit_fail;
	}

	if(CHECK_HTTP_FLAG(phttp, HRF_METHOD_HEAD)) {
		if((oss_method == OSS_NFS_CONFIG) ||
			(oss_method == OSS_NFS_SERVER_MAP))
			phttp->nfs_tunnel = TRUE;
	}
	// Success
	return TRUE;

limit_fail:
	if((oss_method == OSS_NFS_CONFIG) ||
		(oss_method == OSS_NFS_SERVER_MAP)) {
		/* Support NFS Tunnel for the following cases
		 * Request with Query string and no-cache is set for query
		 * Request with Cookie and tunnel override is not set
		 * Request with Authorization and tunnel override is not set
		 * Request with TE chunked
		 * Request with URI size > 512
		 * Request with No cache
		 * Request with Multi-byte range header
		 * Request with content length
		 */
		switch(phttp->tunnel_reason_code) {
			case NKN_TR_REQ_QUERY_STR:
			case NKN_TR_REQ_COOKIE:
			case NKN_TR_REQ_AUTH_HDR:
			case NKN_TR_REQ_CHUNK_ENCODED:
			case NKN_TR_REQ_BIG_URI:
			case NKN_TR_REQ_PRAGMA_NO_CACHE:
			case NKN_TR_REQ_CC_NO_CACHE:
			case NKN_TR_REQ_MULT_BYTE_RANGE:
			case NKN_TR_REQ_HDR_LEN:
				phttp->nfs_tunnel = TRUE;
				break;
			default:
				break;
		}
	}
	if(phttp->nfs_tunnel)
		return TRUE;
	else
		return FALSE;
}


int http_parse_uri(http_cb_t * phttp, const char * p)
{
        const char * ps;
	const char * p_query = 0;
	int len;
	int url_decode_req, url_decode_non_space;
	mime_header_t *hdr = &phttp->hdr;
	
	// GET /htdocs/location.html HTTP/1.0
	//printf("parse: p=%s\n", p);
	url_decode_req = 0;
	url_decode_non_space = 0;
	p = nkn_skip_space(p);
	ps = p;
	while(*p && *p!=' ') { 
		if (*p == '?' && !p_query) p_query = p;
		if (*p == '%') { 
			url_decode_req = 1;
			/* Flag if the hex encoding has other characters
			 */
			/* BZ 5235: define a macro here so that we know which 
			 * hex encoded char is being tested for. 
			 * Also, check if url_decode_non_space is already set - 
			 * if so, then no need to test since we are already
			 * flagged to decode the url .. no point in brute-force
			 * testing of each character.
			 */
#define CHECK_HEX_ENC(p, a, b)	( ( *(((char *)(p))+1) == (char)(a) ) && \
				  ( *(((char *)(p))+2) == (char)(b) ) )
			if ( !url_decode_non_space && 
			    ! ( CHECK_HEX_ENC(p, '2', '0') ||	// space
				CHECK_HEX_ENC(p, '2', '1') ||	// !
				CHECK_HEX_ENC(p, '2', '3') ||	// #
				CHECK_HEX_ENC(p, '2', '4') ||	// $
				CHECK_HEX_ENC(p, '2', '5') ||	// %
				CHECK_HEX_ENC(p, '2', '6') ||	// &
				CHECK_HEX_ENC(p, '2', '8') ||	// (
				CHECK_HEX_ENC(p, '2', '9') ||	// )
				CHECK_HEX_ENC(p, '2', 'A') || CHECK_HEX_ENC(p, '2', 'a') || // *
				CHECK_HEX_ENC(p, '2', 'B') || CHECK_HEX_ENC(p, '2', 'b') || // +
				CHECK_HEX_ENC(p, '2', 'D') || CHECK_HEX_ENC(p, '2', 'd') || // -
				CHECK_HEX_ENC(p, '2', 'E') || CHECK_HEX_ENC(p, '2', 'e') || // .
				CHECK_HEX_ENC(p, '3', 'A') || CHECK_HEX_ENC(p, '3', 'a') || // :
				CHECK_HEX_ENC(p, '3', 'B') || CHECK_HEX_ENC(p, '3', 'b') || // ;
				CHECK_HEX_ENC(p, '3', 'C') || CHECK_HEX_ENC(p, '3', 'c') || // <
				CHECK_HEX_ENC(p, '3', 'D') || CHECK_HEX_ENC(p, '3', 'd') || // =
				CHECK_HEX_ENC(p, '3', 'E') || CHECK_HEX_ENC(p, '3', 'e') || // >
				CHECK_HEX_ENC(p, '3', 'F') || CHECK_HEX_ENC(p, '3', 'f') || // ?
				CHECK_HEX_ENC(p, '4', '0') || 	// @
				CHECK_HEX_ENC(p, '5', 'B') || CHECK_HEX_ENC(p, '5', 'b') || // [
				CHECK_HEX_ENC(p, '5', 'D') || CHECK_HEX_ENC(p, '5', 'd') || // ]
				CHECK_HEX_ENC(p, '5', 'E') || CHECK_HEX_ENC(p, '5', 'e') || // ^
				CHECK_HEX_ENC(p, '5', 'F') || CHECK_HEX_ENC(p, '5', 'f') || // _
				CHECK_HEX_ENC(p, '6', '0') || 	// `
				CHECK_HEX_ENC(p, '7', 'B') || CHECK_HEX_ENC(p, '7', 'b') || // {
				CHECK_HEX_ENC(p, '7', 'C') || CHECK_HEX_ENC(p, '7', 'c') || // |
				CHECK_HEX_ENC(p, '7', 'D') || CHECK_HEX_ENC(p, '7', 'd') || // }
				CHECK_HEX_ENC(p, '7', 'E') || CHECK_HEX_ENC(p, '7', 'e')) ) // ~
			{
				url_decode_non_space = 1;
			}
#undef CHECK_HEX_ENC
		}
		p++; 
	}

	len = p-ps;
	//printf("parse: len=%d\n", len);
	// We make a limitation here to avoid crash
        if (len < 1) {
		glob_http_parse_err_req_toshort++;
		return 1;
	}

	/*
	 * Case 1: GET /uri HTTP/1.0
	 */
	if (ps && *ps && (len > 0) && 
		!((CHECK_HTTP_FLAG(phttp, HRF_METHOD_OPTIONS) ||
		 CHECK_HTTP_FLAG(phttp, HRF_METHOD_TRACE) )&& *ps == '*'))  {
		add_known_header(hdr, MIME_HDR_X_NKN_URI, ps, len);
	} else if ((ps && len == 0) || 
			(ps && *ps == '*' && len == 1)){
		add_known_header(hdr, MIME_HDR_X_NKN_URI, "/", sizeof("/") - 1);
	}
	if (p_query && (p-p_query)) {
	    SET_HTTP_FLAG(phttp, HRF_HAS_QUERY);
	    add_known_header(hdr, MIME_HDR_X_NKN_QUERY, p_query, p-p_query);
	}

	if (url_decode_req) {
	    char *tbuf;
	    int bytesused;
	    /* Set HRF_HAS_HEX_ENCODE flag only if the url
	     * contains hex encoding for char's other than space
	     */
	    if (url_decode_non_space)
	    	SET_HTTP_FLAG(phttp, HRF_HAS_HEX_ENCODE);
	    tbuf = (char *)alloca(len+1);
	    if (!canonicalize_url(ps, len, tbuf, len+1, &bytesused)) {
	    	add_known_header(hdr, MIME_HDR_X_NKN_DECODED_URI, tbuf, bytesused-1);
	    }
	}

	if (len <= (MAX_URI_SIZE-MAX_URI_HDR_SIZE)) {
	    if (normalize_http_uri(hdr)) {
		glob_http_parse_err_normalize_uri++;
		return 1;
	    }
	}
	return 0;
}

#endif /* !PROTO_HTTP_LIB */
