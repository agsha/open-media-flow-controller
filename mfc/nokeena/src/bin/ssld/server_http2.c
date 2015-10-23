/*
 * server_http2.c -- HTTP2 specific interfaces
 */

#ifdef HTTP2_SUPPORT
/*
 *******************************************************************************
 * Experimental SPDY/HTTP2 implementation, not for general use.
 *******************************************************************************
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>
#include <time.h>
#include <limits.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <linux/netfilter_ipv4.h>
#include <sys/param.h>
#include <alloca.h>

#include "ssl_defs.h"
#include "nkn_debug.h"
#include "nkn_stat.h"
#include "nkn_assert.h"
#include "nkn_lockstat.h"
#include "ssl_server.h"
#include "ssl_interface.h"
#include "ssl_network.h"
#include "ssld_mgmt.h"
#include "openssl/ssl.h"
#include "nkn_ssl.h"

#include "proto_http/proto_http.h"
#include <nghttp2/nghttp2.h>
#include "server_http2.h"

static char str_trailers[8] = {'t', 'r', 'a', 'i', 'l', 'e', 'r', 's'};
static int str_trailers_len = sizeof(str_trailers);

extern network_mgr_t *gnm;

static ssize_t
http2_send_cb(nghttp2_session *session, const uint8_t *data, size_t length,
	      int flags, void *user_data);

static ssize_t
http2_recv_cb(nghttp2_session *session, uint8_t *buf, size_t length,
	      int flags, void *user_data);

static int
http2_on_frame_recv_cb(nghttp2_session *session, const nghttp2_frame *frame,
		       void *user_data);

static int
http2_on_invalid_frame_recv_cb(nghttp2_session *session, 
			       const nghttp2_frame *frame, uint32_t error_code,
    			       void *user_data);

static int
http2_on_data_chunk_recv_cb(nghttp2_session *session, uint8_t flags,
			    int32_t stream_id, const uint8_t *data,
			    size_t len, void *user_data);

static int
http2_before_frame_send_cb(nghttp2_session *session, const nghttp2_frame *frame,
			   void *user_data);

static int
http2_on_frame_send_cb(nghttp2_session *session, const nghttp2_frame *frame, 
		       void *user_data);

static int
http2_on_frame_not_send_cb(nghttp2_session *session, const nghttp2_frame *frame,
			   int lib_error_code, void *user_data);

static int
http2_on_stream_close_cb(nghttp2_session *session, int32_t stream_id,
			 uint32_t error_code, void *user_data);

static int
http2_on_begin_headers_cb(nghttp2_session *session, const nghttp2_frame *frame,
			  void *user_data);

static int
http2_on_header_cb(nghttp2_session *session, const nghttp2_frame *frame, const 
		   uint8_t *name, size_t namelen, const uint8_t *value, 
		   size_t valuelen, uint8_t flags, void *user_data);

static ssize_t
http2_select_padding_cb(nghttp2_session *session, const nghttp2_frame *frame,
			size_t max_payloadlen, void *user_data);

static ssize_t
http2_data_source_read_length_cb(nghttp2_session *session, uint8_t frame_type, 
				 int32_t stream_id, 
				 int32_t session_remote_window_size, 
				 int32_t stream_remote_window_size, 
				 uint32_t remote_max_frame_size, 
				 void *user_data);

static int
http2_on_begin_frame_cb(nghttp2_session *session, const nghttp2_frame_hd *hd, 
			void *user_data);

static int forward_response_to_http2_ssl(ssl_con_t *http_con);

static nghttp2_session_callbacks *http2_callbacks;
ssl_server_procs_t coreprocs;

/*
 *******************************************************************************
 * Internal functions
 *******************************************************************************
 */
static void
http2_reset_stream(ssl_con_t *con, nghttp2_error_code err)
{
    nghttp2_submit_rst_stream(con->ctx.u.http2.sess, NGHTTP2_NV_FLAG_NONE,
    			      con->ctx.u.http2.current_streamid, err);
}

static void
setup_stream_data(ssl_con_t *con, int32_t streamid)
{
    con->ctx.u.http2.current_streamid = streamid;
    con->ctx.u.http2.http_token_data = NewTokenData();
    if (!con->ctx.u.http2.http_token_data) {
	DBG_LOG(MSG, MOD_SSL,
		"NewTokenData() failed, reject stream id=%d fd=%d",
		streamid, con->fd);
	http2_reset_stream(con, NGHTTP2_ERR_NOMEM);
    }
}

static void
delete_stream_data(ssl_con_t *con, int delete)
{
    con->ctx.u.http2.current_streamid = -1;

    if (delete) {
    	nghttp2_option_del(con->ctx.u.http2.options);
    	con->ctx.u.http2.options = 0;
    }

    if (delete) {
    	deleteHTTPtoken_data(con->ctx.u.http2.http_token_data);
    }
    con->ctx.u.http2.http_token_data = 0;
    con->ctx.u.http2.http_request_options = PH_ROPT_HTTP_11;

    if (delete) {
    	deleteHTTPtoken_data(con->ctx.u.http2.http_resp_token_data);
    }
    con->ctx.u.http2.http_resp_token_data = 0;

    con->ctx.u.http2.response_hdrs_sent = 0;
    con->ctx.u.http2.pseudo_header_map = 0;
    con->ctx.u.http2.last_header_is_pseudo = 0;
    con->ctx.u.http2.request_hdr_cnt = 0;
}

static void
destroy_stream_data(ssl_con_t *con)
{
    delete_stream_data(con, 1);
}

static void
init_stream_data(ssl_con_t *con)
{
    delete_stream_data(con, 0);
}

static ssize_t
http2_send_cb(nghttp2_session *session, const uint8_t *data, size_t length,
	      int flags, void *user_data)
{
    ssize_t ret;
    ssl_con_t *con = (ssl_con_t *)user_data;

    DBG_LOG(MSG, MOD_SSL, 
	    "http2_send_cb: fd=%d session=%p data=%p len=%ld "
	    "flags=0x%x user=%p",
	    con->fd, session, data, length, flags, user_data);
    ret = SSL_write(con->ssl, data, length);
    if (ret == 0) {
    	ret = NGHTTP2_ERR_CALLBACK_FAILURE;
    } else if (ret < 0) {
	ret = SSL_get_error(con->ssl, ret);
	switch(ret) {
	case SSL_ERROR_WANT_WRITE:
	    con->ctx.flags |= FL_WANT_WRITE;
	    ret = NGHTTP2_ERR_WOULDBLOCK;
	    break;

	case SSL_ERROR_WANT_READ:
	    con->ctx.flags |= FL_WANT_READ;
	    ret = NGHTTP2_ERR_WOULDBLOCK;
	    break;

	default:
	    ret = NGHTTP2_ERR_CALLBACK_FAILURE;
	    break;
	}
    }
    return ret;
}

static ssize_t
http2_recv_cb(nghttp2_session *session, uint8_t *buf, size_t length,
	      int flags, void *user_data)
{
    ssize_t ret;
    ssl_con_t *con = (ssl_con_t *)user_data;

    DBG_LOG(MSG, MOD_SSL, 
	    "http2_recv_cb: fd=%d session=%p buf=%p len=%ld flags=0x%x user=%p",
	    con->fd, session, buf, length, flags, user_data);

    ret = SSL_read(con->ssl, buf, length);
    if (ret == 0) { // EOF
    	ret = NGHTTP2_ERR_EOF;
    } else if (ret < 0) {
	ret = SSL_get_error(con->ssl, ret);
	switch(ret) {
	case SSL_ERROR_WANT_WRITE:
	    con->ctx.flags |= FL_WANT_WRITE;
	    ret = NGHTTP2_ERR_WOULDBLOCK;
	    break;

	case SSL_ERROR_WANT_READ:
	    con->ctx.flags |= FL_WANT_READ;
	    ret = NGHTTP2_ERR_WOULDBLOCK;
	    break;

	default:
	    ret = NGHTTP2_ERR_CALLBACK_FAILURE;
	    break;
	}
    } 
    return ret;
}

static int
http2_on_frame_recv_cb(nghttp2_session *session, const nghttp2_frame *frame,
		       void *user_data)
{
    ssl_con_t *con = (ssl_con_t *)user_data;
    int32_t stream_id;
    int rv;
    const char *buf;
    int buflen;

    switch (frame->hd.type) {
    case NGHTTP2_DATA:
    	// TBD: Handle POST data

	if (con->ctx.u.http2.current_streamid < 0) {
	    return 0; // No stream
	}

	if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
	} else {
	}
    	break;

    case NGHTTP2_HEADERS:
    {
	if (con->ctx.u.http2.current_streamid < 0) {
	    return 0; // No stream
	}

	if (frame->headers.cat == NGHTTP2_HCAT_REQUEST) {
	}

	if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
	    rv = HTTPRequest(con->ctx.u.http2.http_token_data, &buf, &buflen);
	    if (!rv) {
		memcpy(con->cb_buf, buf, MIN(MAX_CB_BUF, buflen));
		con->cb_totlen = buflen;
		con->cb_offsetlen = 0;
		(*coreprocs.forward_request_to_nvsd)(con);
	    } else {
		DBG_LOG(MSG, MOD_SSL, "HTTPRequest() failed, rv=%d fd=%d",
			rv, con->fd);
	    }
	}
	break;
    }

    case NGHTTP2_SETTINGS:
    {
    	if (frame->hd.flags & NGHTTP2_FLAG_ACK) {
	}
    	break;
    }

    case NGHTTP2_PUSH_PROMISE:
    {
    	http2_reset_stream(con, NGHTTP2_REFUSED_STREAM);
	break;
    }

    default:
    {
    	break;
    }
    } // End switch

    return 0;
}

static int
http2_on_invalid_frame_recv_cb(nghttp2_session *session, 
			       const nghttp2_frame *frame, uint32_t error_code,
    			       void *user_data)
{
    return 0;
}

static int
http2_on_data_chunk_recv_cb(nghttp2_session *session, uint8_t flags,
			    int32_t stream_id, const uint8_t *data,
			    size_t len, void *user_data)
{
    return 0;
}

static int
http2_before_frame_send_cb(nghttp2_session *session, const nghttp2_frame *frame,
			   void *user_data)
{
    return 0;
}

static int
http2_on_frame_send_cb(nghttp2_session *session, const nghttp2_frame *frame, 
		       void *user_data)
{
    return 0;
}

static int
http2_on_frame_not_send_cb(nghttp2_session *session, const nghttp2_frame *frame,
			   int lib_error_code, void *user_data)
{
    return 0;
}

static int
http2_on_stream_close_cb(nghttp2_session *session, int32_t stream_id,
			 uint32_t error_code, void *user_data)
{
    ssl_con_t *con = (ssl_con_t *)user_data;

    // Close stream
	
    return 0;
}

static int
http2_on_begin_headers_cb(nghttp2_session *session, const nghttp2_frame *frame,
			  void *user_data)
{
    ssl_con_t *con = (ssl_con_t *)user_data;

    if ((frame->hd.type != NGHTTP2_HEADERS) || 
    	(frame->headers.cat != NGHTTP2_HCAT_REQUEST)) {
    	return 0;
    }

    // Establish stream

    if (con->ctx.u.http2.current_streamid < 0) {
    	setup_stream_data(con, frame->hd.stream_id);
	return 0;
    } else {
    	// Already exist, error return
	return NGHTTP2_ERR_CALLBACK_FAILURE;
    }
}

static pseudo_header_t 
lookup_pseudo_header(const uint8_t *name, size_t namelen) 
{
    switch (namelen) {
    case 5: // :path
    	if ((name[1] == 'p') && !strcmp((char *)&name[2], "ath")) {
	    return PHDR_PATH;
	}
	break;

    case 7: // :method
    	if ((name[1] == 'm') && !strcmp((char *)&name[2], "ethod")) {
	    return PHDR_METHOD;
	} else if (name[1] == 's') {
	    if (!strcmp((char *)&name[2], "cheme")) {
		return PHDR_SCHEME;
	    } else if (!strcmp((char *)&name[2], "tatus")) {
		return PHDR_STATUS;
	    }
	}
	break;

    case 10: // :authority
    	if ((name[1] == 'a') && !strcmp((char *)&name[2], "uthority")) {
	    return PHDR_AUTHORITY;
	}
	break;

    default:
	break;
    }
    return PHDR_UNKNOWN; // Not a pseudo header
}

static int 
valid_request_pseudo_header(ssl_con_t *con, pseudo_header_t phdr_type)
{
    switch (phdr_type) {
    case PHDR_AUTHORITY:
    case PHDR_METHOD:
    case PHDR_PATH:
    case PHDR_SCHEME:
    	if (con->ctx.u.http2.pseudo_header_map & (1 << phdr_type)) {
	    return 0; // Invalid, already set
	} else {
	    return 1; // Valid
	}
	break;

    default:
    	break;
    }
    return 0; // Invalid header type
}

static int
http2_on_header_cb(nghttp2_session *session, const nghttp2_frame *frame, 
		   const uint8_t *name, size_t namelen, const uint8_t *value, 
		   size_t valuelen, uint8_t flags, void *user_data)
{
    ssl_con_t *con = (ssl_con_t *)user_data;
    pseudo_header_t phdr_type;
    uint64_t http_options;
    ProtoHTTPHeaderId id;

    if ((frame->hd.type != NGHTTP2_HEADERS) || 
    	(frame->headers.cat != NGHTTP2_HCAT_REQUEST)) {
    	return 0;
    }

    if (con->ctx.u.http2.current_streamid < 0) {
    	return 0; // No associated stream
    }

    if (!nghttp2_check_header_name(name, namelen)) {
    	return 0;
    }

    if (!nghttp2_check_header_value(value, valuelen)) {
    	return 0;
    }

    if (name[0] == ':') {
    	phdr_type = lookup_pseudo_header(name, namelen);
	if (phdr_type > 0) {
	    // All pseudo headers must come first as a sequential group
	    if ((con->ctx.u.http2.request_hdr_cnt &&
	    	 !con->ctx.u.http2.last_header_is_pseudo) ||
		!valid_request_pseudo_header(con, phdr_type)) {

	    	http2_reset_stream(con, NGHTTP2_PROTOCOL_ERROR);
		return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
	    }

	    http_options = 0;
	    con->ctx.u.http2.last_header_is_pseudo = 1;
	    con->ctx.u.http2.request_hdr_cnt++;
	    con->ctx.u.http2.pseudo_header_map |= (1 << phdr_type);

	    switch (phdr_type) {
	    case PHDR_AUTHORITY:
	    	HTTPAddKnownHeader(con->ctx.u.http2.http_token_data, 0, H_HOST,
				   (char *)value, (int)valuelen);
		break;

	    case PHDR_METHOD:
	    	if (!strcasecmp("GET", (char *)value)) {
		    http_options |= PH_ROPT_METHOD_GET;
		} else {
		    // Only allow "GET"
		    http2_reset_stream(con, NGHTTP2_PROTOCOL_ERROR);
		    return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
		}
		break;

	    case PHDR_PATH:
	    	HTTPAddKnownHeader(con->ctx.u.http2.http_token_data, 0, 
				   H_X_NKN_URI, (char *)value, (int)valuelen);
		break;

	    case PHDR_SCHEME:
	    	if (!strcasecmp("https", (char *)value)) {
		    http_options |= PH_ROPT_SCHEME_HTTPS;
		}
		break;

	    default:
		break;
	    }

	    if (http_options) {
	    	con->ctx.u.http2.http_request_options |= http_options;
	    	HTTPProtoSetReqOptions(con->ctx.u.http2.http_token_data, 
				       con->ctx.u.http2.http_request_options);
	    }
	    return 0;

	} else {
	    // Unknown pseudo header
	    http2_reset_stream(con, NGHTTP2_PROTOCOL_ERROR);
	    return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
	}
    }

    con->ctx.u.http2.last_header_is_pseudo = 0;
    con->ctx.u.http2.request_hdr_cnt++;

    id = HTTPHdrStrToHdrToken((char *)name, (int)namelen);
    if ((int) id >= 0) {
    	HTTPAddKnownHeader(con->ctx.u.http2.http_token_data, 0, id,
			   (char *)value, (int)valuelen);
    } else {
    	HTTPAddUnknownHeader(con->ctx.u.http2.http_token_data, 0, (char *) name,
			     (int) namelen, (char *)value, (int)valuelen);
    }
    return 0;
}

static ssize_t
http2_select_padding_cb(nghttp2_session *session, const nghttp2_frame *frame,
			size_t max_payloadlen, void *user_data)
{
    return frame->hd.length;
}

static ssize_t
http2_data_source_read_length_cb(nghttp2_session *session, uint8_t frame_type, 
				 int32_t stream_id, 
				 int32_t session_remote_window_size, 
				 int32_t stream_remote_window_size, 
				 uint32_t remote_max_frame_size, 
				 void *user_data)
{
    return 0;
}

static int
http2_on_begin_frame_cb(nghttp2_session *session, const nghttp2_frame_hd *hd, 
			void *user_data)
{
    return 0;
}

/*
 *******************************************************************************
 * External functions
 *******************************************************************************
 */
int server_http2_init(ssl_server_procs_t *procs, server_procs_t *sprocs)
{
    coreprocs = *procs;
    sprocs->forward_response_to_ssl = forward_response_to_http2_ssl;

    /* HTTP2 callbacks */

    nghttp2_session_callbacks_new(&http2_callbacks);

    nghttp2_session_callbacks_set_send_callback(http2_callbacks, 
	http2_send_cb);
    nghttp2_session_callbacks_set_recv_callback(http2_callbacks,
	http2_recv_cb);
    nghttp2_session_callbacks_set_on_frame_recv_callback(http2_callbacks,
	http2_on_frame_recv_cb);
    nghttp2_session_callbacks_set_on_invalid_frame_recv_callback(
	http2_callbacks, http2_on_invalid_frame_recv_cb);
    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(
	http2_callbacks, http2_on_data_chunk_recv_cb);
    nghttp2_session_callbacks_set_before_frame_send_callback(
	http2_callbacks, http2_before_frame_send_cb);
    nghttp2_session_callbacks_set_on_frame_send_callback(
	http2_callbacks, http2_on_frame_send_cb);
    nghttp2_session_callbacks_set_on_frame_not_send_callback(
	http2_callbacks, http2_on_frame_not_send_cb);
    nghttp2_session_callbacks_set_on_stream_close_callback(
	http2_callbacks, http2_on_stream_close_cb);
    nghttp2_session_callbacks_set_on_begin_headers_callback(
	http2_callbacks, http2_on_begin_headers_cb);
    nghttp2_session_callbacks_set_on_header_callback(
	http2_callbacks, http2_on_header_cb);
    nghttp2_session_callbacks_set_select_padding_callback(
	http2_callbacks, http2_select_padding_cb);
#if 0
    nghttp2_session_callbacks_set_data_source_read_length_callback(
	http2_callbacks, http2_data_source_read_length_cb);
#endif
    nghttp2_session_callbacks_set_on_begin_frame_callback(
	http2_callbacks, http2_on_begin_frame_cb);

    return 0; // Success
}

static int 
get_http2_version(const unsigned char *next_proto,  
		  unsigned int next_proto_len)
{
    const proto_version_list_t *pv;

    for (pv = http2_version_list; pv->ver_str; pv++) {
	if (streq(pv->ver_str, pv->ver_strlen, next_proto, next_proto_len)) {
	    return pv->version;
	}
    }
    return -1; // No match
}

int server_http2_setup_con(ssl_con_t *ssl_con)
{
    int ret = 0;
    const unsigned char *next_proto = 0;
    unsigned int next_proto_len;
    int res;
    int http2_vers;
    int rv;
    nghttp2_settings_entry entry[4];
    size_t niv;

    SSL_get0_next_proto_negotiated(ssl_con->ssl, &next_proto, &next_proto_len);
#if OPENSSL_VERSION_NUMBER >= 0x10002000L
    if (!next_proto) {
    	SSL_get0_alpn_selected(ssl_con->ssl, &next_proto, &next_proto_len);
    }
#endif

    while (1) {

    http2_vers = get_http2_version(next_proto, next_proto_len);
    switch (http2_vers) {
    case HTTP2_VER:
    case HTTP2_VER_14:
    case HTTP2_VER_16:
    	rv = nghttp2_option_new(&ssl_con->ctx.u.http2.options);
	if (rv) {
	    DBG_LOG(WARNING, MOD_SSL, "nghttp2_option_new() failed, rv=%d", rv);
	    ret = 1;
	    break;
	}
	// Set concurrent streams
	nghttp2_option_set_peer_max_concurrent_streams(
		ssl_con->ctx.u.http2.options, 1);

	// Check client connection preface
	nghttp2_option_set_recv_client_preface(ssl_con->ctx.u.http2.options, 1);

    	rv = nghttp2_session_server_new2(&ssl_con->ctx.u.http2.sess,
					 http2_callbacks, ssl_con,
					 ssl_con->ctx.u.http2.options);
	if (rv) {
	    DBG_LOG(WARNING, MOD_SSL, 
		    "nghttp2_session_server_new2() failed, rv=%d", rv);
	    ret = 2;
	}

	entry[0].settings_id = NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS;
	entry[0].value = 1;
	entry[1].settings_id = NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE;
	entry[1].value = NGHTTP2_INITIAL_WINDOW_SIZE;
	niv = 2;
	rv = nghttp2_submit_settings(ssl_con->ctx.u.http2.sess, 
				     NGHTTP2_FLAG_NONE, entry, niv);
	if (rv) {
	    DBG_LOG(WARNING, MOD_SSL, 
		    "nghttp2_submit_settings() failed, rv=%d", rv);
	    ret = 3;
	}
	break;

    default:
    	ret = -1; // Invalid version
	break;
    }

    if (!ret) {
    	init_stream_data(ssl_con);
	SET_CON_FLAG(ssl_con, CONF_HTTP2);
    }

    break;

    } // End while

    return ret;
}

int http2_close_conn(ssl_con_t *ssl_con)
{
    if (ssl_con->ctx.u.http2.sess) {
    	nghttp2_session_del(ssl_con->ctx.u.http2.sess);
	ssl_con->ctx.u.http2.sess = 0;
	destroy_stream_data(ssl_con);
    }
    return 0;
}

static int http2_sess_done(nghttp2_session *sess)
{
    return !nghttp2_session_want_read(sess) &&
	   !nghttp2_session_want_write(sess);
}

static void http2_setup_epoll(ssl_con_t *con)
{
    if (nghttp2_session_want_read(con->ctx.u.http2.sess) || 
    	con->ctx.flags & FL_WANT_READ) {
	if (!NM_add_event_epollin(con->fd)) {
	    (*coreprocs.close_conn)(con->fd);
	}
    } else if (nghttp2_session_want_write(con->ctx.u.http2.sess) ||
    	       con->ctx.flags & FL_WANT_WRITE) {
	if (!NM_add_event_epollout(con->fd)) {
	    (*coreprocs.close_conn)(con->fd);
	}
    }
}

static int http2_poll_handler(ssl_con_t *con, const char *caller_str) 
{
    int r;

    con->ctx.flags &= ~(FL_WANT_READ | FL_WANT_WRITE);

    r = nghttp2_session_recv(con->ctx.u.http2.sess);
    if (r == 0) {
	r = nghttp2_session_send(con->ctx.u.http2.sess);
	if (r == 0) {
	    if (http2_sess_done(con->ctx.u.http2.sess)) { // done
	    	(*coreprocs.close_conn)(con->fd);
	    } else { // still active
	    	http2_setup_epoll(con);
	    }
	} else { // send error
	    DBG_LOG(MSG, MOD_SSL, 
		    "%s:http2_session_send() failed, rv=%d, fd=%d",
		    caller_str, r, con->fd);
	    (*coreprocs.close_conn)(con->fd);
	}
    } else { // recv error
	DBG_LOG(MSG, MOD_SSL, 
		"%s:http2_session_recv() failed, rv=%d, fd=%d",
		caller_str, r, con->fd);
	(*coreprocs.close_conn)(con->fd);
    }
    return TRUE;
}

int ssl_http2_epollin(int sockfd, void * private_data)
{
    ssl_con_t *con = (ssl_con_t *)private_data;

    DBG_LOG(MSG, MOD_SSL, "fd=%d called", sockfd);
    return http2_poll_handler(con, "IN");
}

int ssl_http2_epollout(int sockfd, void * private_data)
{
    ssl_con_t *con = (ssl_con_t *)private_data;

    DBG_LOG(MSG, MOD_SSL, "fd=%d called", sockfd);
    return http2_poll_handler(con, "OUT");
}

int ssl_http2_epollerr(int sockfd, void * private_data)
{
    DBG_LOG(MSG, MOD_SSL, "fd=%d called", sockfd);
    return (*coreprocs.ssl_epollerr)(sockfd, private_data);
}

int ssl_http2_epollhup(int sockfd, void * private_data)
{
    DBG_LOG(MSG, MOD_SSL, "fd=%d called", sockfd);
    return (*coreprocs.ssl_epollhup)(sockfd, private_data);
}

int ssl_http2_timeout(int sockfd, void * private_data, double timeout)
{
    DBG_LOG(MSG, MOD_SSL, "fd=%d called", sockfd);
    return (*coreprocs.ssl_timeout)(sockfd, private_data, timeout);
}

static ssize_t 
http_source_read_cb(nghttp2_session *session, int32_t stream_id,  
		    uint8_t *buf, size_t length, uint32_t *data_flags,
		    nghttp2_data_source *source, void *user_data)
{
    ssl_con_t *ssl_con = (ssl_con_t *) user_data;
    ssl_con_t *http_con = (ssl_con_t *) source->ptr;

    if (stream_id != ssl_con->ctx.u.http2.current_streamid) {
    	return NGHTTP2_ERR_CALLBACK_FAILURE;
    }

    size_t read_avail = http_con->cb_totlen - http_con->cb_offsetlen;
    size_t bytes_to_xfer = MIN(read_avail, length);

    if (bytes_to_xfer) {
    	memcpy(buf, &http_con->cb_buf[http_con->cb_offsetlen], bytes_to_xfer);
	http_con->sent_len += bytes_to_xfer;

	if (read_avail == bytes_to_xfer) {
	    /* All available data consumed */
	    http_con->cb_offsetlen = 0;
	    http_con->cb_totlen = 0;

	    /* Detect EOF */
	    if (CHECK_CON_FLAG(http_con, CONF_HTTP_CLOSE) ||
	    	(http_con->sent_len >= 
			ssl_con->ctx.u.http2.http_resp_content_len)) {
		*data_flags = NGHTTP2_DATA_FLAG_EOF;
	    } else {
	    	NM_add_event_epollin(http_con->fd); // Await more data
	    }
	} else {
	    /* Partial data consumed */
	    http_con->cb_offsetlen += bytes_to_xfer;
	}

    } else {
	if (!CHECK_CON_FLAG(http_con, CONF_HTTP_CLOSE)) {
	    ssl_con->ctx.flags |= FL_HTTPIN_SUSPENDED;
	    NM_add_event_epollin(http_con->fd); // Await more data
	    return NGHTTP2_ERR_DEFERRED;
	} else {
	    *data_flags = NGHTTP2_DATA_FLAG_EOF;
	}
    }
    return bytes_to_xfer;
}

static void NVdesc2nghttp2NV(const NV_desc_t *nvd, nghttp2_nv *nvng, 
			     int nv_count)
{
    int n;
    for (n = 0; n < nv_count; n++) {
    	nvng[n].name = (uint8_t *)nvd[n].name;
    	nvng[n].value = (uint8_t *)nvd[n].val;
    	nvng[n].namelen = nvd[n].namelen;
    	nvng[n].valuelen = nvd[n].vallen;
    	nvng[n].flags = NGHTTP2_NV_FLAG_NONE;
    }
}

static void push_headers_data(ssl_con_t *ssl_con, const NV_desc_t *nv,
			      int nv_count)
{
    ssl_con_t *http_con = (ssl_con_t *)gnm[ssl_con->peer_fd].private_data;
    int ret;
    nghttp2_data_provider data_prd;
    nghttp2_nv *nvng;

    nvng = (nghttp2_nv *)alloca(nv_count * sizeof(nghttp2_nv));
    NVdesc2nghttp2NV(nv, nvng, nv_count);

    /* Start transfer of headers and data to ssl HTTP2 client */
    data_prd.source.ptr = http_con;
    data_prd.read_callback = http_source_read_cb;

    ret = nghttp2_submit_response(ssl_con->ctx.u.http2.sess, 
    				  ssl_con->ctx.u.http2.current_streamid, 
				  nvng, nv_count, &data_prd);
}

static int forward_response_to_http2_ssl(ssl_con_t *http_con)
{
    ssl_con_t *ssl_con = gnm[http_con->peer_fd].private_data;
    proto_data_t pd;
    HTTPProtoStatus status;
    int header_bytes;
    int bytes_avail;
    int http_respcode;

    int rv;
    NV_desc_t *nv;
    int nv_len;
    int nv_count;
    int eof;

    const char *val;
    int vallen;
    int hdrcnt;

    if (!ssl_con->ctx.u.http2.response_hdrs_sent) {
    	memset(&pd, 0, sizeof(proto_data_t));
	pd.u.HTTP.data = &http_con->cb_buf[http_con->cb_offsetlen];
	pd.u.HTTP.datalen = http_con->cb_totlen - http_con->cb_offsetlen;

    	ssl_con->ctx.u.http2.http_resp_token_data = 
	    HTTPProtoRespData2TokenData(0, &pd, &status, &http_respcode);
	if (status == HP_SUCCESS) {

	    rv = HTTPGetKnownHeader(ssl_con->ctx.u.http2.http_resp_token_data, 
				    0, H_CONTENT_LENGTH, &val, 
				    &vallen, &hdrcnt);
	    if (!rv) {
	    	char valstr[128];
		memcpy(valstr, val, MIN(vallen, 127));
		valstr[127] = '\0';
	    	ssl_con->ctx.u.http2.http_resp_content_len = 
			strtoll(valstr, 0, 10);
	    } else {
	    	ssl_con->ctx.u.http2.http_resp_content_len = ULLONG_MAX;
	    }

	    /*
	     * Delete the following headers as they are not allowed in HTTP2
	     */
	    HTTPDeleteKnownHeader(ssl_con->ctx.u.http2.http_resp_token_data,
	    			  0, H_CONNECTION);
	    HTTPDeleteKnownHeader(ssl_con->ctx.u.http2.http_resp_token_data,
	    			  0, H_KEEP_ALIVE);
	    HTTPDeleteKnownHeader(ssl_con->ctx.u.http2.http_resp_token_data,
	    			  0, H_PROXY_CONNECTION);
	    HTTPDeleteKnownHeader(ssl_con->ctx.u.http2.http_resp_token_data,
	    			  0, H_TRANSFER_ENCODING);
	    rv = HTTPGetKnownHeader(ssl_con->ctx.u.http2.http_resp_token_data, 
				    0, H_TE, &val, &vallen, &hdrcnt);
	    if (!rv && 
	    	((vallen == str_trailers_len) && 
		 !strncasecmp(str_trailers, val, str_trailers_len))) {
	    	HTTPDeleteKnownHeader(ssl_con->ctx.u.http2.http_resp_token_data,
				      0, H_TE);
	    }

	    header_bytes = HTTPHeaderBytes(
	    	ssl_con->ctx.u.http2.http_resp_token_data);
	    http_con->cb_offsetlen += header_bytes;

	    /* Convert headers to HTTP2 format and push to client */
	    rv = HTTPHdr2NVdesc(ssl_con->ctx.u.http2.http_resp_token_data, 0, 
	    			FL_2NV_HTTP2_PROTO,
			    	&nv, &nv_len, &nv_count);
	    if (!rv) {
	    	push_headers_data(ssl_con, nv, nv_count);
	    	ssl_con->ctx.u.http2.response_hdrs_sent = 1;
		free(nv);

		if (NM_del_event_epoll(http_con->fd) == FALSE) {
		    (*coreprocs.close_conn)(http_con->fd);
		    return FALSE;
		}
		if (NM_add_event_epollout(ssl_con->fd) == FALSE) {
		    (*coreprocs.close_conn)(ssl_con->fd);
		    return FALSE;
		}
	    } else {
	    	(*coreprocs.close_conn)(http_con->fd);
	    	return FALSE;
	    }
		
	} else if (status == HP_MORE_DATA) {
	    if (NM_add_event_epollin(http_con->fd) == FALSE) {
                (*coreprocs.close_conn)(http_con->fd);
	    	return FALSE;
	    }
	} else {
	    (*coreprocs.close_conn)(http_con->fd);
	    return FALSE;
	}
    } else {
    	eof = CHECK_CON_FLAG(http_con, CONF_HTTP_CLOSE);
    	bytes_avail = http_con->cb_totlen - http_con->cb_offsetlen;

	if ((eof || bytes_avail) && 
	    (ssl_con->ctx.flags & FL_HTTPIN_SUSPENDED)) {
	    nghttp2_session_resume_data(ssl_con->ctx.u.http2.sess, 
	    				ssl_con->ctx.u.http2.current_streamid);
	    ssl_con->ctx.flags &= ~FL_HTTPIN_SUSPENDED;
	}
	if (NM_del_event_epoll(http_con->fd) == FALSE) {
	    (*coreprocs.close_conn)(http_con->fd);
	    return FALSE;
	}
	if (NM_add_event_epollout(ssl_con->fd) == FALSE) {
	    (*coreprocs.close_conn)(ssl_con->fd);
	    return FALSE;
	}
    }
    return TRUE;
}

#endif /* HTTP2_SUPPORT */

/*
 * End of server_http2.c
 */
