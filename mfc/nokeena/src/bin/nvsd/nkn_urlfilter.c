/*
 * nkn_urlfilter.c -- URL Filter interface
 */
#include <ctype.h>
#include <string.h>

#include "nkn_defs.h"
#include "nkn_debug.h"
#include "uf_utils.h"
#include "nkn_urlfilter.h"

namespace_uf_reject_t url_filter_lookup(const url_filter_config_t *cf,
					const mime_header_t *hdr)
{
    namespace_uf_reject_t ret;
    const char *host = 0;
    int hostlen;
    const char *uri = 0;
    int urilen;
    u_int32_t attrs;
    int hdrcnt;
    const char *key;
    char *pkey;
    int n;
    nkn_trie_node_t pnd;
    const char *p;
    int keylen;
    int removed_slashes;

    if (get_known_header(hdr, MIME_HDR_X_NKN_DECODED_URI,
                         &uri, &urilen, &attrs, &hdrcnt)) {
	if (get_known_header(hdr, MIME_HDR_X_NKN_URI,
                       	     &uri, &urilen, &attrs, &hdrcnt)) {
	    return NS_UF_REJECT_NOACTION; // no URI
        }
    }

    if (!cf->uf_trie) {
	/* Set default to accept */
	ret = NS_UF_REJECT_NOACTION;
	goto uri_size_check;
    }

    if (get_known_header(hdr, MIME_HDR_HOST,
                         &host, &hostlen, &attrs, &hdrcnt)) {
	return NS_UF_REJECT_NOACTION; // no host header
    }

    // strip :80 from host header
    p = memchr(host, ':', hostlen);
    if (p && ((&host[hostlen] - p) == 3) && (p[1] == '8') && (p[2] == '0')) {
    	hostlen -= 3;
    }

    // trie key: <HOSTNAME>/<URI absolute path> 
    //   where HOSTNAME is upper case and URI contains no %XX encoding

    key = alloca(hostlen + urilen + 1);
    pkey = (char *)key;

    for (n = 0; n < hostlen; n++) {
	*(pkey++) = toupper(host[n]);
    }
    memcpy(pkey, uri, urilen);
    ((char *)key)[hostlen + urilen] = '\0';

    keylen = hostlen + urilen;
    removed_slashes = CompressURLSlashes((char *)key, &keylen);
   
    n = nkn_trie_prefix_match(cf->uf_trie->trie, (char *)key, &pnd);
    if (n) {
 	if (cf->uf_is_black_list) {
	    ret = cf->uf_reject_action;
	    DBG_LOG(MSG, MOD_URL_FILTER,
		    "Black list: REJECT act=%d n=%d val=%ld key: \"%s\"", 
		    ret, n, (long)pnd, key);
	} else {
	    ret = NS_UF_REJECT_NOACTION;
	    DBG_LOG(MSG, MOD_URL_FILTER,
		    "White list: ALLOW act=%d n=%d val=%ld key: \"%s\"",
		    ret, n, (long)pnd, key);
	}
    } else {
 	if (cf->uf_is_black_list) {
	    ret = NS_UF_REJECT_NOACTION;
	    DBG_LOG(MSG, MOD_URL_FILTER,
		    "Black list: ALLOW act=%d n=%d key: \"%s\"", ret, n, key);
	} else {
	    ret = cf->uf_reject_action;
	    DBG_LOG(MSG, MOD_URL_FILTER,
		    "White list: REJECT act=%d n=%d key: \"%s\"", ret, n, key);
	}
    }

uri_size_check:
    /* 
     * Check if uri length is greater than the configured allowable length,
     * if so, reject the request
     * If filter map is not configured, the reject action will not be set,
     * use REJECT_RESET as default.
     * if uri filter is not present, the ret is set to NOACTION, to 
     * accept the packets. So either ret will have the result of the uri filter
     * or it will have NOACTION.
     */
    if (cf->uf_max_uri_size && (urilen > (int)cf->uf_max_uri_size)) {
	ret = cf->uf_uri_size_reject_action;
	if (ret == NS_UF_REJECT_UNDEF || 
		ret == NS_UF_REJECT_NOACTION) {
	    ret = NS_UF_REJECT_RESET;
	}
	DBG_LOG(MSG, MOD_URL_FILTER,
		"Uri size filter: REJECT conf size=%d act size =%d ",
		cf->uf_max_uri_size, urilen);
    }

    return ret;
}

/*
 * End of nkn_urlfilter.c
 */
