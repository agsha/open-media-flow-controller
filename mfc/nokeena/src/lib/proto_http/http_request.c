
#include <sys/types.h>
#include <sys/param.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "memalloc.h"
#include "proto_http_int.h"
#include "mime_header.h"

/*
 * Build the OTW form of the HTTP request represented by the http_cb_t.hdr
 */
#define INITIAL_MALLOC_SZ (8 * 1024)
#define MAX_MALLOC_SZ (64 * 1024)

#define OUTBUF(_data, _datalen) { \
    if (((_datalen) + cb->req_cb_totlen) >= cb->req_cb_max_buf_size) { \
        cb->req_cb_buf[cb->cb_max_buf_size-1] = '\0'; \
        return 1; \
    } \
    memcpy((void *)&cb->req_cb_buf[cb->req_cb_totlen], (_data), (_datalen)); \
    cb->req_cb_totlen += (_datalen); \
}

static int 
int_build_http_request(http_cb_t *cb)
{
    mime_header_t *hdr = &cb->hdr;
    int rv;
    const char *data;
    int datalen;
    const char *name;
    int namelen;
    u_int32_t attrs;
    int hdrcnt;
    int token;
    int nth;

    /* Method */
    if (CHECK_HTTP_FLAG(cb, HRF_METHOD_GET)) {
    	OUTBUF("GET", 3);
    } else if (CHECK_HTTP_FLAG(cb, HRF_METHOD_POST)) {
    	OUTBUF("POST", 4);
    } else if (CHECK_HTTP_FLAG(cb, HRF_METHOD_HEAD)) {
    	OUTBUF("HEAD", 4);
    } else if (CHECK_HTTP_FLAG(cb, HRF_METHOD_CONNECT)) {
    	OUTBUF("CONNECT", 7);
    } else if (CHECK_HTTP_FLAG(cb, HRF_METHOD_PUT)) {
    	OUTBUF("PUT", 3);
    } else if (CHECK_HTTP_FLAG(cb, HRF_METHOD_DELETE)) {
    	OUTBUF("DELETE", 6);
    } else if (CHECK_HTTP_FLAG(cb, HRF_METHOD_TRACE)) {
    	OUTBUF("TRACE", 5);
    }
    OUTBUF(" ", 1);

    /* Path */
    rv = get_known_header(hdr, MIME_HDR_X_NKN_URI, &data, &datalen, 
    			  &attrs, &hdrcnt);
    if (!rv) {
    	OUTBUF(data, datalen);
    }

    /* Version */
    if (CHECK_HTTP_FLAG(cb, HRF_HTTP_11)) {
    	OUTBUF(" HTTP/1.1", 9);
    } else if (CHECK_HTTP_FLAG(cb, HRF_HTTP_11)) {
    	OUTBUF(" HTTP/1.0", 9);
    }
    OUTBUF("\r\n", 2);

    /* Add known headers */
    for (token = 0; token < MIME_HDR_X_NKN_URI; token++) { // Skip all 
							   // MIME_HDR_X hdrs
	if (!http_end2end_header[token]) {
	    continue;
	}

	rv = get_known_header(hdr, token, &data, &datalen, &attrs, &hdrcnt);
	if (!rv) {
	    OUTBUF(http_known_headers[token].name, 
		   http_known_headers[token].namelen);
	    OUTBUF(": ", 2);
	    OUTBUF(data, datalen);
	} else {
	    continue;
	}

	for (nth = 1; nth < hdrcnt; nth++) { // Fold values
	    rv = get_nth_known_header(hdr, token, nth, &data, &datalen, &attrs);
	    if (!rv) {
	    	OUTBUF(",", 1);
		OUTBUF(data, datalen);
	    }
	}
	OUTBUF("\r\n", 2);
    }

    /* Add unknown headers */
    nth = 0;
    while ((rv = get_nth_unknown_header(hdr, nth, &name, &namelen, 
					&data, &datalen, &attrs)) == 0) {
	nth++;
	OUTBUF(name, namelen);
	OUTBUF(": ", 2);
	OUTBUF(data, datalen);
	OUTBUF("\r\n", 2);
    }

    OUTBUF("\r\n", 2);
    return 0;
}

int build_http_request(http_cb_t *cb)
{
    int rv;

    if (cb->req_cb_buf) {
    	free(cb->req_cb_buf);
    	cb->req_cb_buf = 0;
    	cb->req_cb_totlen = 0;
    }

    cb->req_cb_max_buf_size = INITIAL_MALLOC_SZ;

    while (1) {
    	cb->req_cb_buf = nkn_malloc_type(cb->req_cb_max_buf_size, 
					 mod_http_req_buf);
	if (!cb->req_cb_buf) {
	    rv = 2;
	    break;
	}
    	cb->req_cb_totlen = 0;

    	rv = int_build_http_request(cb);

	if (!rv) {
	    break;
	} else {
	    free(cb->req_cb_buf);
	    cb->req_cb_buf = 0;
	    cb->req_cb_max_buf_size *= 2;
	    if (cb->req_cb_max_buf_size > MAX_MALLOC_SZ) {
	    	rv = 3;
		break;
	    }
	}
    }
    return rv;
}

/*
 * End of http_request.c
 */
