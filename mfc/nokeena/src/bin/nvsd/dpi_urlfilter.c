/*
 * dpi_urlfilter.c -- DPI interface into URL filter
 */
#include <string.h>

#include "nkn_defs.h"
#include "nkn_debug.h"
#include "mime_header.h"
#include "nkn_urlfilter.h"
#include "dpi_urlfilter.h"

NKNCNT_DEF(uf_reject_drop_pkts_80009, uint64_t, "", "URL filter - reject by dropping packets")

/*
 * DPI_urlfilter_lookup() -- DPI interface to URL Filter subsystem
 *   Assumptions:
 *    1) Host is assumed to be RFC compliant
 *    2) URI absolute path is RFC compliant
 */
namespace_uf_reject_t 
DPI_urlfilter_lookup(uint64_t http_method,
		     const char *host, int hostlen,
                     const char *uri_abs_path, int uri_abs_pathlen)
{
    UNUSED_ARGUMENT(http_method);

    int rv;
    namespace_uf_reject_t retval = NS_UF_REJECT_NOACTION;
    mime_header_t mh;
    int valid_mh = 0;
    const char *ext_buf_start;
    const char *ext_buf_end;
    char *tbuf;
    char *pbuf;
    int bytes_used;
    const namespace_config_t *nsconf;
    namespace_token_t ns_token = NULL_NS_TOKEN;

    ////////////////////////////////////////////////////////////////////////////
    while (1) {
    ////////////////////////////////////////////////////////////////////////////

    if (!host || !hostlen || !uri_abs_path || !uri_abs_pathlen) {
    	DBG_LOG(MSG, MOD_URL_FILTER, 
		"Null input paramter: "
		"host=%p hostlen=%d uri_abs_path=%p uri_abs_pathlen=%d",
		host, hostlen, uri_abs_path, uri_abs_pathlen);
    	break;
    }

    /* Avoid data copy, use external buffer reference */

    if (uri_abs_path <= host) {
    	ext_buf_start = uri_abs_path;
	ext_buf_end = host + hostlen;
    } else {
    	ext_buf_start = host;
	ext_buf_end = uri_abs_path + uri_abs_pathlen;
    }

    if ((ext_buf_end - ext_buf_start) <= INT_MAX) {
	rv = mime_hdr_init(&mh, MIME_PROT_HTTP, ext_buf_start, 
			   (ext_buf_end - ext_buf_start));
    } else {
	rv = mime_hdr_init(&mh, MIME_PROT_HTTP, 0, 0);
    }

    if (rv) {
	DBG_LOG(MSG, MOD_URL_FILTER,
		"mime_hdr_init() failed, bstart=%p bufend=%p rv=%d",
		ext_buf_start, ext_buf_end, rv);
	break;
    }
    valid_mh = 1;

    rv = mime_hdr_add_known(&mh, MIME_HDR_HOST, host, hostlen);
    if (rv) {
    	DBG_LOG(MSG, MOD_URL_FILTER, 
		"mime_hdr_add_known(MIME_HDR_HOST) failed, "
		"rv=%d host=%p hostlen=%d", rv, host, hostlen);
	break;
    }

    rv = mime_hdr_add_known(&mh, MIME_HDR_X_NKN_URI, 
			    uri_abs_path, uri_abs_pathlen);
    if (rv) {
    	DBG_LOG(MSG, MOD_URL_FILTER, 
		"mime_hdr_add_known(MIME_HDR_X_NKN_URI) failed, "
		"rv=%d uri_abs_path=%p uri_abs_pathlen=%d", 
		rv, uri_abs_path, uri_abs_pathlen);
	break;
    }

    if (memchr(uri_abs_path, '%', uri_abs_pathlen)) { // may contain %XX
	tbuf = alloca(uri_abs_pathlen + 1);

	pbuf = unescape_str(uri_abs_path, uri_abs_pathlen, 0, 
			    tbuf, uri_abs_pathlen + 1, &bytes_used);
	if (pbuf && (bytes_used < uri_abs_pathlen)) {
	    rv = mime_hdr_add_known(&mh, MIME_HDR_X_NKN_DECODED_URI, 
				    tbuf, bytes_used);
	    if (rv) {
		DBG_LOG(MSG, MOD_URL_FILTER, 
			"mime_hdr_add_known(MIME_HDR_X_NKN_DECODED_URI) "
			"failed, rv=%d uri_abs_path=%p uri_abs_pathlen=%d", 
			rv, tbuf, bytes_used);
		break;
	    }
	}
    }

    /* Map to namespace */

    ns_token = http_request_to_namespace(&mh, &nsconf);
    if (!VALID_NS_TOKEN(ns_token)) {
    	DBG_LOG(MSG, MOD_URL_FILTER, 
		"No namespace, http_request_to_namespace() failed");
	break;
    }

    /*  Apply URL filter if applicable */

    if (nsconf->uf_config->uf_trie) {
    	retval = url_filter_lookup(nsconf->uf_config, &mh);
    }

    break;

    ////////////////////////////////////////////////////////////////////////////
    } // End of while
    ////////////////////////////////////////////////////////////////////////////

    if (valid_mh) {
    	mime_hdr_shutdown(&mh);
    }

    if (VALID_NS_TOKEN(ns_token)) {
    	release_namespace_token_t(ns_token);
    }
    
    switch (retval) {
    case NS_UF_REJECT_DROP_PKTS:
    	AO_fetch_and_add1(&glob_uf_reject_drop_pkts_80009);
        break;
    default:
        break;
    }
    return retval;
}

/*
 * End of dpi_urlfilter.c
 */
