#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <dlfcn.h>
#include <errno.h>
#include <netdb.h>
#include <time.h>
#include <sys/types.h>
#include <sys/timeb.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/param.h>
#include <strings.h>

#include "nkn_http.h"
#include "nkn_stat.h"
#include "nkn_namespace.h"
#include "http_header.h"
#include "nkn_debug.h"
#include "pe_defs.h"

#define TSTR(_p, _l, _r)  char *(_r); ( 1 ? ( (_r) = alloca((_l)+1), \
        memcpy((_r), (_p), (_l)), (_r)[(_l)] = '\0' ) : 0 )


extern AO_t glob_tot_hdr_size_from_cache,
    glob_tot_hdr_size_from_sata_disk,
    glob_tot_hdr_size_from_sas_disk,
    glob_tot_hdr_size_from_ssd_disk,
    glob_tot_hdr_size_from_disk,
    glob_tot_hdr_size_from_nfs,
    glob_tot_hdr_size_from_origin,
    glob_tot_hdr_size_from_tfm;


extern char myhostname[1024];
extern int myhostname_len;
extern int sizeof_response_200_OK_body;

extern const char * get_http_content_type(const char * suffix, int len);

// List all counters
NKNCNT_DEF(http_err_parse, int, "", "HTTP parse error")
NKNCNT_DEF(http_tot_200_resp, uint64_t, "", "Total 200 response")
NKNCNT_DEF(http_tot_206_resp, uint64_t, "", "Total 206 response")
NKNCNT_DEF(http_tot_302_resp, uint64_t, "", "Total 302 response")
NKNCNT_DEF(http_tot_303_resp, uint64_t, "", "Total 303 response")
NKNCNT_DEF(http_tot_304_resp, uint64_t, "", "Total 304 response")
NKNCNT_DEF(http_tot_400_resp, uint64_t, "", "Total 400 response")
NKNCNT_DEF(http_tot_403_resp, uint64_t, "", "Total 403 response")
NKNCNT_DEF(http_tot_404_resp, uint64_t, "", "Total 404 response")
NKNCNT_DEF(http_tot_405_resp, uint64_t, "", "Total 405 response")
NKNCNT_DEF(http_tot_413_resp, uint64_t, "", "Total 413 response")
NKNCNT_DEF(http_tot_414_resp, uint64_t, "", "Total 414 response")
NKNCNT_DEF(http_tot_416_resp, uint64_t, "", "Total 416 response")
NKNCNT_DEF(http_tot_500_resp, uint64_t, "", "Total 500 response")
NKNCNT_DEF(http_tot_501_resp, uint64_t, "", "Total 501 response")
NKNCNT_DEF(http_tot_502_resp, uint64_t, "", "Total 502 response")
NKNCNT_DEF(http_tot_503_resp, uint64_t, "", "Total 503 response")
NKNCNT_DEF(http_tot_504_resp, uint64_t, "", "Total 504 response")
NKNCNT_DEF(http_tot_505_resp, uint64_t, "", "Total 505 response")
NKNCNT_DEF(http_tot_content_length, uint64_t, "", "Total Content Length Resp")
NKNCNT_DEF(http_tot_byte_range, uint64_t, "", "Total Byte Range Resp")

/*
 * HTTP Response
 */

static void http_buildup_resp_header(http_cb_t * phttp, mime_header_t * p_response_hdrs);

/*
 * HTTP Response Build up functions
 */

static inline void
http_add_date(mime_header_t * p_response_hdr)
{
	int datestr_len;
	char *datestr = nkn_get_datestr(&datestr_len);

	add_known_header(p_response_hdr, MIME_HDR_DATE, datestr, datestr_len);
}

static inline void
http_add_server(mime_header_t * p_response_hdr)
{
	add_known_header(p_response_hdr, MIME_HDR_SERVER, "juniper/1.0", 10);
}

static int
suppress_response_hdr(const namespace_config_t *nsc,
				 const char *name, int namelen)
{
	int n;
	header_descriptor_t *hd;

	if (!nsc) {
	    return 0; // Allow
	}

	for (n = 0; n < nsc->http_config->num_delete_response_headers; n++) {
	    hd = &nsc->http_config->delete_response_headers[n];
	    if (hd && hd->name && (hd->name_strlen == namelen) && 
	        strncasecmp(hd->name, name, namelen) == 0) {
		return 1; // Suppress
	    }
	}
	return 0; // Allow
}

static int get_nth_add_response_hdr(const namespace_config_t *nsc,
				    int nth, const char **name, int *namelen,
				    const char **val, int *vallen)
{	int n;
	int nth_valid = 0;
	header_descriptor_t *hd;

	if (!nsc) {
	    return 2; // None
	}

	for (n = 0; n < nsc->http_config->num_add_response_headers; n++) {
	    hd = &nsc->http_config->add_response_headers[n];
	    if (hd->name) {
	    	if (nth_valid == nth) {
		    *name = hd->name;
		    *namelen = hd->name_strlen;
		    if (hd->value) {
		    	*val = hd->value;
		    	*vallen = hd->value_strlen;
		    } else {
		    	*val = 0;
		    	*vallen = 0;
		    }
		    return 0;
		} else {
		    nth_valid++;
		}
	    }
	}

	return 1; // None
}

static void http_add_via_and_date_header(http_cb_t * phttp, mime_header_t * p_response_hdr)
{
	const namespace_config_t *nsc = phttp->nsconf;
	int modify_date;
	int vallen;

	if (phttp->res_hdlen) {
		// already filled in
		return;
	}

	if (nsc == 0) {
		modify_date = 0; 
	} else {
		modify_date =  nsc->http_config->policies.modify_date_header;
	}
	if (!modify_date) {
		int ret;

		int    via_listsz = 0;
		int    via_bufsz = 0;
		char * via_list   = NULL;
                int    num_via_headers = 0;
                int    total_val_bytecount = 0;
                char   mfc_via[256] = {0,} ;
                int    mfc_via_hdr_len = 0 ;

               /* Before conctenating the via headers from origin, we have to findout
                * buffer space needed for mfc's via header value.
                */
                if (!(nsc && nkn_http_is_transparent_proxy(nsc))) {
                    mfc_via_hdr_len = snprintf(mfc_via, sizeof(mfc_via), 
                                      "%s %s:%hu",
                                      (CHECK_HTTP_RESPONSE_FLAG(phttp, HRF_HTTP_10)
                                                ? "1.0" : "1.1"),
                                      myhostname, phttp->remote_port);
                }

               /* Get the number of 'Via' headers in the list & and the total length of all header values */
                count_known_header_values(p_response_hdr, MIME_HDR_VIA, 
                                               &num_via_headers,
                                               &total_val_bytecount);

                via_listsz = total_val_bytecount       /* Space for N Via header values */
                             + num_via_headers         /* for commas between each value */
                             + 1 ;                     /* NULL terminator */
                via_bufsz  = via_listsz                /* Total length of all 'Via' header values */
                             + (mfc_via_hdr_len + 1) ; /* MFC's Via header & one comma*/
                via_list = alloca(via_bufsz) ; 

		if ((!concatenate_known_header_values(p_response_hdr, 
							MIME_HDR_VIA,
							via_list, via_listsz))
				|| (via_list[0] == '\0')) {
			via_list[0] = '\0' ; // Set list empty.
			vallen      = 0 ;
		} else {
			// There is some entries in Via header. So append ", " string to that.
			strcat(via_list, ",") ;
			vallen = strlen(via_list) ;
		}

		if (mfc_via_hdr_len) { /* A non-zero value indicate we have to append MFCs Via details */
			ret = snprintf(&via_list[vallen], via_bufsz - vallen - 1, "%s", mfc_via) ;
		      if (ret) {
				delete_known_header(p_response_hdr, MIME_HDR_VIA);
				add_known_header(p_response_hdr, MIME_HDR_VIA, via_list, ret+vallen);
			}
		}   
	}

	if (modify_date) {
        	http_add_date(p_response_hdr);
		if (CHECK_HTTP_FLAG2(phttp, HRF2_EXPIRED_OBJ_DEL)) {
			const cooked_data_types_t *ckdp;
			int ckd_len, rv;
			mime_hdr_datatype_t dtype;

			rv = get_cooked_known_header_data(p_response_hdr, MIME_HDR_CACHE_CONTROL,
					&ckdp, &ckd_len, &dtype);
			if (!rv && (dtype == DT_CACHECONTROL_ENUM) &&
			    (ckdp->u.dt_cachecontrol_enum.mask & CT_MASK_MAX_AGE)) {
				update_known_header(p_response_hdr, MIME_HDR_CACHE_CONTROL,
						    "max-age=0", 9, 0);
			} else {
				add_known_header(p_response_hdr, MIME_HDR_CACHE_CONTROL, "max-age=0", 9);
			}
			NS_INCR_STATS((phttp->nsconf), PROTO_HTTP, client, expired_objects);
			SET_HTTP_FLAG2(phttp, HRF2_EXPIRED_OBJ_ENT);
			CLEAR_HTTP_FLAG2(phttp, HRF2_EXPIRED_OBJ_DEL);
		}
	} else if (phttp->respcode == 200 || phttp->respcode == 206 || 
			phttp->respcode == 304) {
		// Add Age: header
		if (!CHECK_HTTP_FLAG2(phttp, HRF2_EXPIRED_OBJ_DEL)) {
			time_t current_age = 0;
			char current_age_str[64];
			int current_age_strlen;

			current_age = nkn_cur_ts - phttp->obj_create;
			if (current_age < 0 || current_age >= 0x7fffffff) {
				current_age = 0x80000000;
			}
			current_age = MAX(0, current_age);
			if (current_age > 0) {
				current_age_strlen = snprintf(current_age_str, 
						sizeof(current_age_str),
						"%ld", current_age);
				add_known_header(p_response_hdr, MIME_HDR_AGE, 
					current_age_str, current_age_strlen);
			}
		} else {
			const cooked_data_types_t *ckdp;
			int ckd_len, rv;
			mime_hdr_datatype_t dtype;

			rv = get_cooked_known_header_data(p_response_hdr, MIME_HDR_CACHE_CONTROL,
					&ckdp, &ckd_len, &dtype);
			if (!rv && (dtype == DT_CACHECONTROL_ENUM) &&
			    (ckdp->u.dt_cachecontrol_enum.mask & CT_MASK_MAX_AGE)) {
				update_known_header(p_response_hdr, MIME_HDR_CACHE_CONTROL,
						    "max-age=0", 9, 0);
			} else {
				add_known_header(p_response_hdr, MIME_HDR_CACHE_CONTROL, "max-age=0", 9);
			}
			NS_INCR_STATS((phttp->nsconf), PROTO_HTTP, client, expired_objects);
			SET_HTTP_FLAG2(phttp, HRF2_EXPIRED_OBJ_ENT);
			CLEAR_HTTP_FLAG2(phttp, HRF2_EXPIRED_OBJ_DEL);
		}
	}
	if(!is_known_header_present(p_response_hdr, MIME_HDR_DATE)) {
		http_add_date(p_response_hdr);
	}
}

static void http_buildup_resp_header(http_cb_t * phttp, mime_header_t * p_response_hdr)
{

#define OUTBUF(_data, _datalen, _bytesused) { \
    if (((_datalen)+(_bytesused)) >= phttp->resp_buflen) { \
    	tmp_resp_buf = phttp->resp_buf; \
    	tmp_resp_buflen = phttp->resp_buflen; \
	phttp->resp_buflen = \
		MAX(phttp->resp_buflen*2, (_datalen)+(_bytesused)+1); \
	phttp->resp_buf = \
		nkn_realloc_type(phttp->resp_buf, phttp->resp_buflen, \
				 mod_http_respbuf); \
	if (phttp->resp_buf) { \
	    memcpy((void *)&phttp->resp_buf[(_bytesused)], \
	    	   (_data), (_datalen)); \
	    (_bytesused) += (_datalen); \
	} else { \
	    phttp->resp_buf  = tmp_resp_buf; \
	    phttp->resp_buflen = tmp_resp_buflen; \
	} \
    } else { \
    	memcpy((void *)&phttp->resp_buf[(_bytesused)], (_data), (_datalen)); \
	(_bytesused) += (_datalen); \
    } \
}

#define MAX_RESP_STR_SIZE 100
	char response_str[MAX_RESP_STR_SIZE];
        const char * codestr;
        char * p;
	int len;
 	const char *name;
	int namelen;
	const char *val;
	int vallen;
	const char *data;
	int datalen;
	int hdrcnt;
	u_int32_t attr;
	int n, nth;
	int rv;
	const namespace_config_t *nsc = phttp->nsconf;
	//int modify_date;

	int bytesused;
	char *tmp_resp_buf;
    	int tmp_resp_buflen;
	int conn_hdr_found;

        if( phttp->res_hdlen ) {
                // already filled in
                return;
        }

	// choice supported response code
        switch(phttp->respcode) {
		/* code 200, 206, 400: could be called from BM. */
		case 100: codestr="Continue"; break;
		case 200: codestr="OK"; 
			  if (!CHECK_HTTP_FLAG(phttp, HRF_MFC_PROBE_REQ)) {
			  	glob_http_tot_200_resp++; 
			  	NS_INCR_STATS(nsc, PROTO_HTTP, client, resp_2xx);
			  	NS_INCR_STATS(nsc, PROTO_HTTP, client, resp_200);
			  }	
			  break;
		case 206: codestr="Partial Content";
			  AO_fetch_and_add1(&glob_http_tot_206_resp); 
			  NS_INCR_STATS(nsc, PROTO_HTTP, client, resp_2xx);
			  NS_INCR_STATS(nsc, PROTO_HTTP, client, resp_206);
			  break;
		case 301: codestr="Moved Permanently"; break;
		case 302: codestr="Found";
			  glob_http_tot_302_resp++;
			  NS_INCR_STATS(nsc, PROTO_HTTP, client, resp_3xx);
			  NS_INCR_STATS(nsc, PROTO_HTTP, client, resp_302);
			  break;
		case 303: codestr="See Other";
			  glob_http_tot_303_resp++;
			  break;
		case 304: codestr="Not Modified";
			  glob_http_tot_304_resp++; 
			  NS_INCR_STATS(nsc, PROTO_HTTP, client, resp_3xx);
			  NS_INCR_STATS(nsc, PROTO_HTTP, client, resp_304);
			  break;
		case 400: codestr="Bad Request";
			  glob_http_tot_400_resp++; 
			  NS_INCR_STATS(nsc, PROTO_HTTP, client, resp_4xx);
			  break;
		case 403: codestr="Forbidden";
			  glob_http_tot_403_resp++; 
			  NS_INCR_STATS(nsc, PROTO_HTTP, client, resp_4xx);
			  break;
		case 404: codestr="Not Found";
			  glob_http_tot_404_resp++; 
			  NS_INCR_STATS(nsc, PROTO_HTTP, client, resp_4xx);
			  NS_INCR_STATS(nsc, PROTO_HTTP, client, resp_404);
			  break;
		case 405: codestr="Method Not Allowed";
			  glob_http_tot_405_resp++; 
			  NS_INCR_STATS(nsc, PROTO_HTTP, client, resp_4xx);
			  break;
		case 406: codestr="Not Acceptable"; break;
		case 408: codestr="Request Timeout"; break;
		case 410: codestr="Gone"; break;
		case 413: codestr="Request Entity Too Large";
			  glob_http_tot_413_resp++; 
			  NS_INCR_STATS(nsc, PROTO_HTTP, client, resp_4xx);
			  break;
		case 414: codestr="Request-URI Too Long";
			  glob_http_tot_414_resp++; 
			  NS_INCR_STATS(nsc, PROTO_HTTP, client, resp_4xx);
			  break;
		case 416: codestr="Requested Range Not Satisfiable";
			  glob_http_tot_416_resp++; 
			  NS_INCR_STATS(nsc, PROTO_HTTP, client, resp_4xx);
			  break;
		case 417: codestr="Expectation Failed";
			 /* TBD: Add this counter: 
                          * glob_http_tot_417_resp++;
                          */ 
			  NS_INCR_STATS(nsc, PROTO_HTTP, client, resp_4xx);
			  break;
		case 500: codestr="Internal Server Error";
			  glob_http_tot_500_resp++; 
			  NS_INCR_STATS(nsc, PROTO_HTTP, client, resp_5xx);
			  break;
		case 501: codestr="Not Implemented";
			  glob_http_tot_501_resp++; 
			  NS_INCR_STATS(nsc, PROTO_HTTP, client, resp_5xx);
			  break;
		case 502: codestr="Bad Gateway";
			  glob_http_tot_502_resp++; 
			  break;
		case 503: codestr="Service Unavailable";
			  glob_http_tot_503_resp++; 
			  NS_INCR_STATS(nsc, PROTO_HTTP, client, resp_5xx);
			  break;
		case 504: codestr="Gateway Timeout";
			  glob_http_tot_504_resp++; 
			  NS_INCR_STATS(nsc, PROTO_HTTP, client, resp_5xx);
			  break;
		case 505: codestr="HTTP Version Not Supported"; 
			  NS_INCR_STATS(nsc, PROTO_HTTP, client, resp_5xx);
			  break;

		/* default: most likely it is called from Tunnel code path */
                default: 
			if (get_known_header(p_response_hdr, MIME_HDR_X_NKN_RESPONSE_STR, 
				&val, &vallen, &attr, &hdrcnt)) {
				// Not found.
				return;
			}
			if (vallen > MAX_RESP_STR_SIZE - 1) {
				vallen = MAX_RESP_STR_SIZE - 1;
			}
			memcpy(&response_str[0], val, vallen);
			response_str[vallen] = 0;
			codestr = &response_str[0];
			break;
        }
        // Adding common HTTP response headers

#if 0
	nsc = phttp->nsconf;
	if(nsc == 0) {
		modify_date = 0; 
	} else {
		modify_date =  nsc->http_config->policies.modify_date_header;
	}
	if (!modify_date) {
		int ret;

                int    via_listsz = 4096;
                char * via_list   = alloca(via_listsz);

                if ((!concatenate_known_header_values(p_response_hdr, 
                                                     MIME_HDR_VIA,
                                                     via_list, via_listsz))
                    || (via_list[0] == '\0')) {
                    via_list[0] = '\0' ; // Set list empty.
                    vallen      = 0 ;
                } else {
                    // There is some entries in Via header. So append ", " string to that.
                    strcat(via_list, ",") ;
                    vallen = strlen(via_list) ;
                }

                
                if (!(nsc && nkn_namespace_use_client_ip(nsc))) {
                      ret = snprintf(&via_list[vallen], 4096 - vallen - 1, "%s %s:%hu", 
                                     (CHECK_HTTP_RESPONSE_FLAG(phttp, HRF_HTTP_10) 
                                                                ? "1.0" : "1.1"),
                                     myhostname, phttp->remote_port);
                      if (ret) {
                          delete_known_header(p_response_hdr, MIME_HDR_VIA);
                          add_known_header(p_response_hdr, MIME_HDR_VIA, via_list, ret+vallen);
                      }   
                }   
	}

	if (modify_date) {
        	http_add_date(p_response_hdr);
	} else if (phttp->respcode == 200 || phttp->respcode == 206 || 
			phttp->respcode == 304) {
		// Add Age: header
		time_t current_age = 0;
		char current_age_str[64];
		int current_age_strlen;

		current_age = nkn_cur_ts - phttp->obj_create;
		if (current_age < 0 || current_age >= 0x7fffffff)
			current_age = 0x80000000;
		current_age = MAX(0, current_age);
		if (current_age > 0) {
		current_age_strlen = 
			snprintf(current_age_str, sizeof(current_age_str),
				 "%ld", current_age);
		add_known_header(p_response_hdr, MIME_HDR_AGE, current_age_str, current_age_strlen);
		}
	}
	if(!is_known_header_present(p_response_hdr, MIME_HDR_DATE)) {
        	http_add_date(p_response_hdr);
	}
#endif

	// Malloc response buffer
	phttp->resp_buf = (char *)nkn_malloc_type(phttp->cb_max_buf_size, 
						mod_http_respbuf);
	if(!phttp->resp_buf) {
		return;
	}
	phttp->resp_buflen = phttp->cb_max_buf_size;

	//
        // Build up the HTTP response
	//
	phttp->resp_buf[phttp->resp_buflen-1] = '\0';
	p=phttp->resp_buf;
        phttp->res_hdlen = 0;

	// Adding the HTTP code line
        len = snprintf(p, phttp->cb_max_buf_size-phttp->res_hdlen, 
		       "HTTP/1.1 %d %s", phttp->respcode, codestr);
        p += len; phttp->res_hdlen += len;

	if(phttp->subcode) {
        	len = snprintf(p, phttp->cb_max_buf_size-phttp->res_hdlen, 
			       " %d", phttp->subcode);
        	p += len; phttp->res_hdlen += len;
	}

	strcat(p, "\r\n");
	p += 2; phttp->res_hdlen += 2;


	bytesused = p - phttp->resp_buf;

	// Add known headers
	for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
		if (!p_response_hdr->known_header_map[n]) {
			continue;
		}
		name = http_known_headers[n].name;
		namelen = http_known_headers[n].namelen;
		switch(n) {
		case MIME_HDR_CONNECTION:
		case MIME_HDR_TRANSFER_ENCODING:
			break;
		case MIME_HDR_X_ACCEL_CACHE_CONTROL:
			name = phttp->nsconf->http_config->
				    policies.cache_redirect.hdr_name;
			namelen = phttp->nsconf->http_config->
				    policies.cache_redirect.hdr_len;
			if (namelen == 0) {
				continue;
			}
			break;
		case MIME_HDR_TRAILER:
			/* For Chunked encoding, pass the Trailer header to client
			 */
			if (CHECK_HTTP_FLAG(phttp, HRF_TRANSCODE_CHUNKED)) {
				break;
			}
			else {
				continue;
			}
		default:
			if (http_end2end_header[n] == 0) {
				// continue of for loop, not switch case.
				continue;
			}
			break;
		}
		if (suppress_response_hdr(nsc, name, namelen)) {
			continue;
		}
		rv = get_known_header(p_response_hdr, n, &data, &datalen, &attr, &hdrcnt);
		if (!rv) {
			if(hdrcnt == 1) {
				OUTBUF(name, namelen, bytesused);
				phttp->res_hdlen += namelen;
				OUTBUF(": ", 2, bytesused);
				phttp->res_hdlen += 2;

				OUTBUF(data, datalen, bytesused);
				phttp->res_hdlen += datalen;
				OUTBUF("\r\n", 2, bytesused);
				phttp->res_hdlen += 2;
				continue;
			}

			// Fold multi value headers
			for (nth = 0; nth < hdrcnt; nth++) {
				rv = get_nth_known_header(p_response_hdr, n, nth, &data,
                                                &datalen, &attr);
				if (!rv) {
					OUTBUF(name, namelen, bytesused);
					phttp->res_hdlen += namelen;
					OUTBUF(": ", 2, bytesused);
					phttp->res_hdlen += 2;

					OUTBUF(data, datalen, bytesused);
					phttp->res_hdlen += datalen;
					OUTBUF("\r\n", 2, bytesused);
					phttp->res_hdlen += 2;
				}
			}
		}
	}

	// Add unknown headers
	nth = 0;
	while ((rv = get_nth_unknown_header(p_response_hdr, nth, &name,
                                        &namelen, &data, &datalen,
                                        &attr)) == 0) {
		nth++;
		if (suppress_response_hdr(nsc, name, namelen)) {
			continue;
		}
		OUTBUF(name, namelen, bytesused);
		phttp->res_hdlen += namelen;
		OUTBUF(": ", 2, bytesused);
		phttp->res_hdlen += 2;

		OUTBUF(data, datalen, bytesused);
		phttp->res_hdlen += datalen;
		OUTBUF("\r\n", 2, bytesused);
		phttp->res_hdlen += 2;
	}

	// Add configured response headers
	n = 0;
	conn_hdr_found = 0;
	while ((rv = get_nth_add_response_hdr(nsc, n, &name, &namelen,
					      &val, &vallen)) == 0) {
		/*
		 * bug 5226 - if "connection" header is configured to add
		 * with the value "close", then HRF_CONNECTION_KEEP_ALIVE 
		 * flag should be cleared.
		 */
		if (conn_hdr_found == 0 && strcasecmp(name, "connection") == 0) {
			conn_hdr_found = 1;
			if (strcasecmp(val, "close") == 0) {
				CLEAR_HTTP_FLAG(phttp, HRF_CONNECTION_KEEP_ALIVE);
			}
		} 
		OUTBUF(name, namelen, bytesused);
		phttp->res_hdlen += namelen;
		OUTBUF(": ", 2, bytesused);
		phttp->res_hdlen += 2;

		if (val && vallen) {
			OUTBUF(val, vallen, bytesused);
		}
		phttp->res_hdlen += vallen;
		OUTBUF("\r\n", 2, bytesused);
		phttp->res_hdlen += 2;
		n++;
	}

	OUTBUF("\r\n", 2, bytesused);
	phttp->res_hdlen += 2;
	OUTBUF("\0", 1, bytesused);

	if (CHECK_HTTP_FLAG(phttp, HRF_TRACE_REQUEST)) {
		const char *uri = NULL;
		int urilen = 0;
		u_int32_t attrs;
		get_known_header(&phttp->hdr, MIME_HDR_X_NKN_URI, &uri, &urilen, 
				 &attrs, &hdrcnt);
		TSTR(uri, urilen, my_uri);
		DBG_TRACE("HTTP Response object %s\n%s", 
			  my_uri, phttp->resp_buf);
	}

	return;

#undef OUTBUF
#undef MAX_RESP_STR_SIZE
}

static
int get_uri(http_cb_t * phttp, const char **uri, int *urilen)
{
	u_int32_t attrs;
	int hdrcnt;
	
	if (get_known_header(&phttp->hdr, MIME_HDR_X_NKN_REMAPPED_URI,
                       uri, urilen, &attrs, &hdrcnt)) {
		if (get_known_header(&phttp->hdr, MIME_HDR_X_NKN_DECODED_URI,
                               uri, urilen, &attrs, &hdrcnt)) {
               		if (get_known_header(&phttp->hdr, MIME_HDR_X_NKN_URI,
                               	uri, urilen, &attrs, &hdrcnt)) {
				*uri = 0;
				*urilen = 0;
				return 1; // Should never happen
			}
               }
       }
       return 0;
}

static int setup_http_build_100(http_cb_t * phttp, mime_header_t * presponse_hdr);
static int setup_http_build_100(http_cb_t * phttp, mime_header_t * presponse_hdr)
{
	UNUSED_ARGUMENT(phttp);

	delete_known_header(presponse_hdr, MIME_HDR_CONNECTION) ;
	add_known_header(presponse_hdr, MIME_HDR_CONNECTION, "Keep-Alive", 10);
	add_known_header(presponse_hdr, MIME_HDR_CONTENT_LENGTH, "0", 1);

	// Malloc response buffer
	phttp->resp_buf = (char *)nkn_malloc_type(phttp->cb_max_buf_size,
						mod_http_respbuf);
	phttp->resp_buflen = phttp->cb_max_buf_size;
	phttp->res_hdlen = snprintf(phttp->resp_buf, phttp->cb_max_buf_size,
						"HTTP/1.1 100 Continue\r\n\r\n");
	/* There will not be any body part for 100-continue response,
	 * so mark the end timestamp here.
  	 */
	gettimeofday(&(phttp->end_ts), NULL); 

	return 1;
}

static int setup_http_build_200(http_cb_t * phttp, mime_header_t * presponse_hdr);
static int setup_http_build_200(http_cb_t * phttp, mime_header_t * presponse_hdr)
{
	char buf[128];
	const char *data;
	const char * type;
        const cooked_data_types_t *pckd;
        int dlen;
	mime_hdr_datatype_t dtype;
	int len;

	// Setup content length
	if (!get_cooked_known_header_data(presponse_hdr,
                                        MIME_HDR_CONTENT_LENGTH, &pckd, &dlen,
                                        &dtype)) {
		if (dtype == DT_SIZE) {
			phttp->content_length = pckd->u.dt_size.ll;
		}
		if( !CHECK_HTTP_FLAG(phttp, HRF_TRANSCODE_CHUNKED) ) {
			len = snprintf(buf, sizeof(buf), "%ld", 
				CHECK_HTTP_FLAG(phttp, HRF_MULTIPART_RESPONSE) ?
					phttp->pseudo_content_length : 
					phttp->content_length);
			add_known_header(presponse_hdr, MIME_HDR_CONTENT_LENGTH, buf, len);
		}
	}


	glob_http_tot_content_length ++;

	if (!is_known_header_present(presponse_hdr, MIME_HDR_CONTENT_TYPE)) {
		const namespace_config_t *nsc;
		/* Only fill up content_type when it is NFS OM */
		nsc = phttp->nsconf;
		if(nsc && nsc->http_config) {
		    if((nsc->http_config->origin.select_method == OSS_NFS_CONFIG) ||
		       (nsc->http_config->origin.select_method == OSS_NFS_SERVER_MAP)) {
			get_uri(phttp, &data, &len);
			type = get_http_content_type(data, len);
			add_known_header(presponse_hdr, MIME_HDR_CONTENT_TYPE, type, strlen(type));
		    }
		}
	}
        delete_known_header(presponse_hdr, MIME_HDR_CONNECTION) ;
	if( CHECK_HTTP_FLAG(phttp, HRF_CONNECTION_KEEP_ALIVE) ) {
		add_known_header(presponse_hdr, MIME_HDR_CONNECTION, "Keep-Alive", 10);
		SET_HTTP_FLAG(phttp, HRF_CONNECTION_KEEP_ALIVE);
	}
	else {
		add_known_header(presponse_hdr, MIME_HDR_CONNECTION, "Close", 5);
		CLEAR_HTTP_FLAG(phttp, HRF_CONNECTION_KEEP_ALIVE);
	}

#if 0
	add_known_header(presponse_hdr, MIME_HDR_CACHE_CONTROL, "no-cache", 8);
#endif

        return 1;
}

static int setup_http_build_206(http_cb_t * phttp, mime_header_t * presponse_hdr);
static int setup_http_build_206(http_cb_t * phttp, mime_header_t * presponse_hdr)
{
	char buf[128];
	const char *data;
	const char * type;
	const cooked_data_types_t *pckd;
	int dlen;
	mime_hdr_datatype_t dtype;
	int len;
	off_t start, stop;

	// Setup content length
	if (!get_cooked_known_header_data(presponse_hdr,
                                        MIME_HDR_CONTENT_LENGTH, &pckd, &dlen,
                                        &dtype)) {
		if (dtype == DT_SIZE) {
			phttp->content_length = pckd->u.dt_size.ll;
		}
	}

	glob_http_tot_byte_range ++;

        phttp->respcode = CHECK_HTTP_FLAG(phttp, HRF_BYTE_SEEK) ? 200 : 206;
        phttp->subcode = 0;

	if (!is_known_header_present(presponse_hdr, MIME_HDR_CONTENT_TYPE)) {
		const namespace_config_t *nsc;
		/* Only fill up content_type when it is NFS OM */
		nsc = phttp->nsconf;
		if(nsc && nsc->http_config) {
		    if((nsc->http_config->origin.select_method == OSS_NFS_CONFIG) ||
		       (nsc->http_config->origin.select_method == OSS_NFS_SERVER_MAP)) {
			get_uri(phttp, &data, &len);
			type = get_http_content_type(data, len);
			add_known_header(presponse_hdr, MIME_HDR_CONTENT_TYPE, type, strlen(type));
		    }
		}
	}
        delete_known_header(presponse_hdr, MIME_HDR_CONNECTION) ;
	if( CHECK_HTTP_FLAG(phttp, HRF_CONNECTION_KEEP_ALIVE) ) {
		add_known_header(presponse_hdr, MIME_HDR_CONNECTION, "Keep-Alive", 10);
		SET_HTTP_FLAG(phttp, HRF_CONNECTION_KEEP_ALIVE);
	}
	else {
		add_known_header(presponse_hdr, MIME_HDR_CONNECTION, "Close", 5);
		CLEAR_HTTP_FLAG(phttp, HRF_CONNECTION_KEEP_ALIVE);
	}

	if (CHECK_HTTP_FLAG(phttp, HRF_MULTI_BYTE_RANGE)) {
		/* 
		 * bug 5301 - MFD remembers only the first byte range in the multi 
		 * byte range request. So we should not modify the content_length
		 * and add content_range based on brstart and brstop values. If MFD
		 * remembers all brstart adn brstop, we can think of doing.
		 */
		return 1;
	} else if (CHECK_HTTP_FLAG(phttp, HRF_BYTE_RANGE)) {
		start = phttp->brstart;
		stop = phttp->brstop;
	} else if (CHECK_HTTP_FLAG(phttp, HRF_BYTE_SEEK)) {
		start = phttp->seekstart;
		stop = phttp->seekstop;
	} else {
		// Should never happen
		start = 0;
		stop = 0;
	}

	if (!is_known_header_present(presponse_hdr, MIME_HDR_CONTENT_RANGE)) {
           if (!CHECK_HTTP_FLAG(phttp, HRF_BYTE_SEEK)) {
               len = snprintf(buf, sizeof(buf), "bytes %ld-%ld/%ld", start,
			stop, phttp->content_length);
		add_known_header(presponse_hdr, MIME_HDR_CONTENT_RANGE, buf, len);
           }
	}

	len = snprintf(buf, sizeof(buf), "%ld", 
		stop - start + 1 + 
		(CHECK_HTTP_FLAG(phttp, HRF_BYTE_SEEK) ? 
			phttp->prepend_content_size +
			phttp->append_content_size  : 0));
	add_known_header(presponse_hdr, MIME_HDR_CONTENT_LENGTH, buf, len);

#if 0
	add_known_header(presponse_hdr, MIME_HDR_CACHE_CONTROL, "no-cache", 8);
#endif

        return 1;
}

static int setup_http_build_304(http_cb_t * phttp, mime_header_t * presponse_hdr);
static int setup_http_build_304(http_cb_t * phttp, mime_header_t * presponse_hdr)
{
        delete_known_header(presponse_hdr, MIME_HDR_CONNECTION) ;
	if(CHECK_HTTP_FLAG(phttp, HRF_CONNECTION_KEEP_ALIVE)) {
		add_known_header(presponse_hdr, MIME_HDR_CONNECTION, "Keep-Alive", 10);
	}
	else {
		add_known_header(presponse_hdr, MIME_HDR_CONNECTION, "Close", 5);
	}

	add_known_header(presponse_hdr, MIME_HDR_CONTENT_LENGTH, "0", 1);

	//add_known_header(presponse_hdr, MIME_HDR_CONTENT_TYPE, "text/plain", 10);

	return 1;
}

static int setup_http_build_others(http_cb_t * phttp, mime_header_t * presponse_hdr);
static int setup_http_build_others(http_cb_t * phttp, mime_header_t * presponse_hdr)
{
        delete_known_header(presponse_hdr, MIME_HDR_CONNECTION) ;
	if(CHECK_HTTP_FLAG(phttp, HRF_CONNECTION_KEEP_ALIVE)) {
		add_known_header(presponse_hdr, MIME_HDR_CONNECTION, "Keep-Alive", 10);
	}
	else {
		add_known_header(presponse_hdr, MIME_HDR_CONNECTION, "Close", 5);
	}

	/*
	 * As per RFC 2616, "All 1xx (informational), 204 (no content),
	 *                   and 304 (not modified) responses MUST NOT include a
	 *                   message-body. All other responses do include a
	 *                   message-body, although it MAY be of zero length."
	 */
	switch(phttp->respcode) {
		case  100:  /* Continue */
		case  101:  /* Switching Protocols */
		case  204:  /* No Content */
		case  304:  /* Not Modified */
			add_known_header(presponse_hdr, MIME_HDR_CONTENT_LENGTH, "0", 1);
			break;
		case  302:
			/* Bug 9829, add content length if not present.
			 */
			if (!is_known_header_present(presponse_hdr, MIME_HDR_CONTENT_LENGTH)) { 
				add_known_header(presponse_hdr, MIME_HDR_CONTENT_LENGTH, "0", 1);
			}
			break;
		default:
			break;
	}
	//add_known_header(presponse_hdr, MIME_HDR_CONTENT_TYPE, "text/plain", 10);

	return 1;
}

int http_build_res_common(http_cb_t * phttp, int status_code, int errcode, mime_header_t * presponse_hdr);
int http_build_res_common(http_cb_t * phttp, int status_code, int errcode, mime_header_t * presponse_hdr)
{
	int rv;
	int presponse_hdr_from;
	const namespace_config_t *nsconf = phttp->nsconf;
	const char *cookie_str;
	int cookie_len, rv1;
	u_int32_t attrs;
	int hdrcnt, nth;

	/*
	 * response header has already been built.
	 */
	if(phttp->res_hdlen || CHECK_HTTP_FLAG(phttp, HRF_RES_HEADER_SENT))
		return 1;

	// For HTTP 0.9 requests do not generate response headers.
	if (CHECK_HTTP_FLAG(phttp, HRF_HTTP_09)) return 1;

	/* 
	 * case 1: presponse_hdr presents. e.g. tunnel code path calls for build_up_response.
	 * case 2: phttp->attr presents. e.g. cache hit code path calls for build_up_response.
	 * case 3: Both are NULL or !errcode. e.g. HTTP request parser error.
	 */
	if(presponse_hdr && 
	   (!errcode || ((NKN_UF_MIN <= errcode) && (errcode <= NKN_UF_MAX)))) {
		// case 1
		presponse_hdr_from = 1;
	}
	else if(phttp->attr && !errcode) {
		// case 2
		presponse_hdr_from = 2;
		if (phttp->p_resp_hdr) {
			shutdown_http_header(phttp->p_resp_hdr);
			free(phttp->p_resp_hdr);
		}
		phttp->p_resp_hdr = (mime_header_t *)nkn_malloc_type(sizeof(mime_header_t),
							    mod_http_mime_header_t);
		if (phttp->p_resp_hdr == NULL) return 1;
		presponse_hdr = phttp->p_resp_hdr;
		init_http_header(presponse_hdr, 0, 0);
	        rv = nkn_attr2http_header(phttp->attr, 1, presponse_hdr);
		if (rv) {
			DBG_LOG(MSG, MOD_HTTP, "nkn_attr2http_header() failed,  rv=%d", rv);
			return 1;
		}

		if(CHECK_HTTP_FLAG(phttp, HRF_SSP_CONFIGURED)) {
		    if ( phttp->p_ssp_cb->ssp_client_id == 6 ) {
			switch (phttp->p_ssp_cb->ssp_streamtype) {
			    case NKN_SSP_SMOOTHSTREAM_STREAMTYPE_MANIFEST:
				add_known_header(presponse_hdr, MIME_HDR_CONTENT_TYPE, "text/xml", 8);
				break;
			    case NKN_SSP_SMOOTHSTREAM_STREAMTYPE_VIDEO:
				add_known_header(presponse_hdr, MIME_HDR_CONTENT_TYPE, "video/mp4", 9);
                                break;
			    case NKN_SSP_SMOOTHSTREAM_STREAMTYPE_AUDIO:
				add_known_header(presponse_hdr, MIME_HDR_CONTENT_TYPE, "audio/mp4", 9);
                                break;
			}
		    } else if ( phttp->p_ssp_cb->ssp_client_id == 7 ) {
			switch (phttp->p_ssp_cb->ssp_streamtype) {
			    case NKN_SSP_FLASHSTREAM_STREAMTYPE_FRAGMENT:
				add_known_header(presponse_hdr, MIME_HDR_CONTENT_TYPE, "video/f4f", 9);
                                break;
			}
		    }
		}

		/* Special case for cookie handling. If set cookie cacheable, then we've to append
		 * the respective cookie headers back to the client response. This is done here 
		 * because under such scenarios we would've stripped the cookie header from the 
		 * origin reponse before caching the object.
		 */
		if (CHECK_HTTP_FLAG(phttp, HRF_CACHE_COOKIE)) {
			if (is_known_header_present(&phttp->hdr, MIME_HDR_SET_COOKIE)) {
				mime_hdr_get_known(&phttp->hdr, MIME_HDR_SET_COOKIE, 
						&cookie_str, &cookie_len, &attrs, &hdrcnt);
				for(nth = 0; nth < hdrcnt; nth++) {
					rv1 = get_nth_known_header(&phttp->hdr, MIME_HDR_SET_COOKIE, 
								nth, &cookie_str, &cookie_len, &attrs);
					if (!rv1) {
						add_known_header(presponse_hdr, MIME_HDR_SET_COOKIE, 
								cookie_str, cookie_len);
					} else {
						DBG_LOG(MSG, MOD_OM,
							"add_known_header() failed rv=%d nth=%d, MIME_HDR_SET_COOKIE ",
							rv1, nth);
					}
				}
			} 

			if (is_known_header_present(&phttp->hdr, MIME_HDR_SET_COOKIE2)) {
				mime_hdr_get_known(&phttp->hdr, MIME_HDR_SET_COOKIE2, 
						&cookie_str, &cookie_len, &attrs, &hdrcnt);
				for(nth = 0; nth < hdrcnt; nth++) {
					rv1 = get_nth_known_header(&phttp->hdr, MIME_HDR_SET_COOKIE2, 
								nth, &cookie_str, &cookie_len, &attrs);
					if (!rv1) {
						add_known_header(presponse_hdr, MIME_HDR_SET_COOKIE2, 
								cookie_str, cookie_len);
					} else {
						DBG_LOG(MSG, MOD_OM,
							"add_known_header() failed rv=%d nth=%d, MIME_HDR_SET_COOKIE2 ",
							rv1, nth);
					}
				}
			}
		}
	}
	else {
		// case 3
		presponse_hdr_from = 3;
		if (phttp->p_resp_hdr) {
			shutdown_http_header(phttp->p_resp_hdr);
			free(phttp->p_resp_hdr);
		}
		phttp->p_resp_hdr = (mime_header_t *)nkn_malloc_type(sizeof(mime_header_t),
							    mod_http_mime_header_t);
		if (phttp->p_resp_hdr == NULL) return 1;
		presponse_hdr = phttp->p_resp_hdr;
		init_http_header(presponse_hdr, 0, 0);
		if(CHECK_HTTP_FLAG(phttp, HRF_SSP_SF_RESPONSE)) {
			char buf[100];
			snprintf(buf, 100, "%d", sizeof_response_200_OK_body);
			add_known_header(presponse_hdr, MIME_HDR_CONTENT_LENGTH, buf, strlen(buf));
		}
	}

	/*
	 * build up the response header, output is saved in
	 * phttp->resp_buf, length is phttp->res_hdlen;
	 */
	phttp->respcode = status_code;
	if (errcode) {
		phttp->subcode = errcode;
	}

	/*
	 * Counter update.
	 */
	switch(status_code) {
		case 100: 
			setup_http_build_100(phttp, presponse_hdr);
			break;
		case 200: 
			setup_http_build_200(phttp, presponse_hdr);
			break;
		case 206: 
			setup_http_build_206(phttp, presponse_hdr);
			break;
		case 304: 
			setup_http_build_304(phttp, presponse_hdr);
			break;
		case 301: 
		case 302: 
		case 400:
		case 403:
		case 404:
		case 405:
		case 413:
		case 414:
		case 416:
		case 417:
			setup_http_build_others(phttp, presponse_hdr);
			break;
		case 500:
		case 501:
		case 503:

			/* Requirement 2.1 - 34. 
			 * If retry-after is enabled, set the header in the
			 * response message.
			 */
			if ((phttp->nsconf != NULL) && 
				(errcode == NKN_HTTP_NS_MAX_CONNS) &&
				(phttp->nsconf->http_config->retry_after_time > 0)) {
				char buf[100];
				snprintf(buf, 100, "%d", phttp->nsconf->http_config->retry_after_time);
				add_known_header(presponse_hdr, MIME_HDR_RETRY_AFTER, buf, strlen(buf));
			}

		case 502:
		case 504:
		default:
			CLEAR_HTTP_FLAG(phttp, HRF_CONNECTION_KEEP_ALIVE);
			setup_http_build_others(phttp, presponse_hdr);
			break;
	}

	/* Add Via header and date headers before PE is called
	 */
	http_add_via_and_date_header(phttp, presponse_hdr);

	/* Apply Policy for http send response here
	 */
	if (nsconf && nsconf->http_config && nsconf->http_config->policy_engine_config.policy_file) {
		pe_ilist_t * p_list = NULL;
		pe_rules_t * p_perule;

		p_perule = pe_create_rule(&nsconf->http_config->policy_engine_config, &p_list);
		if (p_perule) {
			uint64_t action;
			action = pe_eval_http_send_response(p_perule, phttp, presponse_hdr);
			pe_free_rule(p_perule, p_list);
		}
	}

	http_buildup_resp_header(phttp, presponse_hdr);

	/*   
	 * accesslog format could configure to record response header.   
	 * Because sizeof(mime_header_t) is too big,   
	 * we better not add response header into phttp structure which will waste too much memory.   
	 * It is not needed for R-Proxy case. 
	 */
	if (phttp->nsconf && phttp->nsconf->acclog_config->al_resp_header_configured) {
		if (presponse_hdr_from == 1) {
			if (phttp->p_resp_hdr) {
				shutdown_http_header(phttp->p_resp_hdr);
				free(phttp->p_resp_hdr);
			}
			phttp->p_resp_hdr = (mime_header_t *)nkn_malloc_type(sizeof(mime_header_t),
							    mod_http_mime_header_t);
			if (phttp->p_resp_hdr) {
				init_http_header(phttp->p_resp_hdr, 0, 0);
				copy_http_headers(phttp->p_resp_hdr, presponse_hdr);   
			}
		}
	} else {
		if (presponse_hdr_from != 1) {
			shutdown_http_header(presponse_hdr);
			free(phttp->p_resp_hdr);
			phttp->p_resp_hdr = NULL;
		}
	}

	/*
	 * Update counters with header size.
	 */
	if ( (status_code != 304) && 
	     !CHECK_HTTP_FLAG(phttp, HRF_METHOD_HEAD) &&
	     !CHECK_HTTP_FLAG(phttp, HRF_MFC_PROBE_REQ) ) {
	switch(phttp->provider){
	case BufferCache_provider:
		AO_fetch_and_add(&glob_tot_hdr_size_from_cache, phttp->res_hdlen);
		break;

	case SASDiskMgr_provider:
	case SAS2DiskMgr_provider:
		AO_fetch_and_add(&glob_tot_hdr_size_from_sas_disk, phttp->res_hdlen);
		AO_fetch_and_add(&glob_tot_hdr_size_from_disk, phttp->res_hdlen);
		break;
	case SATADiskMgr_provider:
		AO_fetch_and_add(&glob_tot_hdr_size_from_sata_disk, phttp->res_hdlen);
		AO_fetch_and_add(&glob_tot_hdr_size_from_disk, phttp->res_hdlen);
		break;
	case SolidStateMgr_provider:
		AO_fetch_and_add(&glob_tot_hdr_size_from_ssd_disk, phttp->res_hdlen);
		AO_fetch_and_add(&glob_tot_hdr_size_from_disk, phttp->res_hdlen);
		break;
	case NFSMgr_provider:
		AO_fetch_and_add(&glob_tot_hdr_size_from_nfs, phttp->res_hdlen);
		break;
	case TFMMgr_provider:
		AO_fetch_and_add(&glob_tot_hdr_size_from_tfm, phttp->res_hdlen);
		break;
	case OriginMgr_provider:
		AO_fetch_and_add(&glob_tot_hdr_size_from_origin, phttp->res_hdlen);
		break;
	case Tunnel_provider:
		// Will be added in the fp_forward_server_to_client().
	default:
		break;
	}

	}

    if (304 == status_code)
    {
	switch(phttp->provider){
	case BufferCache_provider:
		AO_fetch_and_add(&glob_tot_hdr_size_from_cache, -(phttp->tot_reslen));
		break;

	case SASDiskMgr_provider:
	case SAS2DiskMgr_provider:
		AO_fetch_and_add(&glob_tot_hdr_size_from_sas_disk, -(phttp->tot_reslen));
		AO_fetch_and_add(&glob_tot_hdr_size_from_disk, -(phttp->tot_reslen));
		break;
	case SATADiskMgr_provider:
		AO_fetch_and_add(&glob_tot_hdr_size_from_sata_disk, -(phttp->tot_reslen));
		AO_fetch_and_add(&glob_tot_hdr_size_from_disk, -(phttp->tot_reslen));
		break;
	case SolidStateMgr_provider:
		AO_fetch_and_add(&glob_tot_hdr_size_from_ssd_disk, -(phttp->tot_reslen));
		AO_fetch_and_add(&glob_tot_hdr_size_from_disk, -(phttp->tot_reslen));
		break;
	case NFSMgr_provider:
		AO_fetch_and_add(&glob_tot_hdr_size_from_nfs, -(phttp->tot_reslen));
		break;
	case TFMMgr_provider:
		AO_fetch_and_add(&glob_tot_hdr_size_from_tfm, -(phttp->tot_reslen));
		break;
	case OriginMgr_provider:
		AO_fetch_and_add(&glob_tot_hdr_size_from_origin, -(phttp->tot_reslen));
		break;
	case Tunnel_provider:
		// Will be added in the fp_forward_server_to_client().
	default:
		break;
	}

	}

	return 1;
}

int http_build_res_200(http_cb_t * phttp) 
{
	return http_build_res_common(phttp, 200, 0, NULL);
}

int http_build_res_200_ext(http_cb_t * phttp, mime_header_t * presponse_hdr) 
{
	return http_build_res_common(phttp, 200, 0, presponse_hdr);
}

int http_build_res_206(http_cb_t * phttp) 
{
	return http_build_res_common(phttp, 206, 0, NULL);
}

int http_build_res_301(http_cb_t * phttp, int errcode, mime_header_t * presponse_hdr) 
{
	return http_build_res_common(phttp, 301, errcode, presponse_hdr);
}

int http_build_res_302(http_cb_t * phttp, int errcode, mime_header_t * presponse_hdr) 
{
	return http_build_res_common(phttp, 302, errcode, presponse_hdr);
}

int http_build_res_304(http_cb_t * phttp, int errcode, mime_header_t * presponse_hdr) 
{
	return http_build_res_common(phttp, 304, errcode, presponse_hdr);
}

int http_build_res_400(http_cb_t * phttp, int errcode)
{
	return http_build_res_common(phttp, 400, errcode, NULL);
}


int http_build_res_403(http_cb_t * phttp, int errcode)
{
	return http_build_res_common(phttp, 403, errcode, NULL);
}

int http_build_res_404(http_cb_t * phttp, int errcode)
{
	return http_build_res_common(phttp, 404, errcode, NULL);
}

int http_build_res_405(http_cb_t * phttp, int errcode)
{
	return http_build_res_common(phttp, 405, errcode, NULL);
}

int http_build_res_413(http_cb_t * phttp, int errcode)
{
	return http_build_res_common(phttp, 413, errcode, NULL);
}

int http_build_res_414(http_cb_t * phttp, int errcode)
{
	return http_build_res_common(phttp, 414, errcode, NULL);
}

int http_build_res_416(http_cb_t * phttp, int errcode)
{
	return http_build_res_common(phttp, 416, errcode, NULL);
}

int http_build_res_417(http_cb_t * phttp, int errcode)
{
       return http_build_res_common(phttp, 417, errcode, NULL);
}

int http_build_res_500(http_cb_t * phttp, int errcode)
{
	return http_build_res_common(phttp, 500, errcode, NULL);
}

int http_build_res_501(http_cb_t * phttp, int errcode)
{
	return http_build_res_common(phttp, 501, errcode, NULL);
}

int http_build_res_502(http_cb_t * phttp, int errcode)
{
	return http_build_res_common(phttp, 502, errcode, NULL);
}

int http_build_res_503(http_cb_t * phttp, int errcode)
{
	return http_build_res_common(phttp, 503, errcode, NULL);
}

int http_build_res_504(http_cb_t *phttp, int errcode) {
	return http_build_res_common(phttp, 504, errcode, NULL);
}

int http_build_res_505(http_cb_t * phttp, int errcode)
{
	return http_build_res_common(phttp, 505, errcode, NULL);
}

int http_build_res_unsuccessful(http_cb_t * phttp, int response) 
{
	return http_build_res_common(phttp, response, 0, NULL);
}
