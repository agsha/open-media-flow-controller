/*
 * server_spdy.c -- SPDY specific interfaces
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
#include <spdylay/spdylay.h>
#include "server_spdy.h"

extern network_mgr_t *gnm;

static ssize_t 
spdy_send_cb(spdylay_session *session, const uint8_t *data, size_t length, 
	     int flags, void *user_data);

static ssize_t
spdy_recv_cb(spdylay_session *session, uint8_t *buf, size_t length, int flags, 
	     void *user_data);

static void
spdy_on_ctrl_recv_cb(spdylay_session *session, spdylay_frame_type type, 
		     spdylay_frame *frame, void *user_data);

static void
spdy_on_invalid_ctrl_recv_cb(spdylay_session *session, spdylay_frame_type type,
			     spdylay_frame *frame, uint32_t status_code, 
			     void *user_data); 

static void 
spdy_on_data_chunk_recv_cb(spdylay_session *session, uint8_t flags, 
			   int32_t stream_id, const uint8_t *data, 
			   size_t len, void *user_data); 

static void 
spdy_on_data_recv_cb(spdylay_session *session, uint8_t flags, 
		     int32_t stream_id, int32_t length, void *user_data);

static void
spdy_before_ctrl_send_cb(spdylay_session *session, spdylay_frame_type type,
			 spdylay_frame *frame, void *user_data);

static void
spdy_on_ctrl_send_cb(spdylay_session *session, spdylay_frame_type type, 
		     spdylay_frame *frame, void *user_data);

static void
spdy_on_ctrl_not_send_cb(spdylay_session *session, spdylay_frame_type type, 
			 spdylay_frame *frame, int error_code, void *user_data);

static void
spdy_on_data_send_cb(spdylay_session *session, uint8_t flags, 
		     int32_t stream_id, int32_t length, void *user_data);


static void
spdy_on_stream_close_cb(spdylay_session *session, int32_t stream_id, 
			spdylay_status_code status_code, void *user_data);

static void
spdy_on_request_recv_cb(spdylay_session *session, int32_t stream_id, 
			void *user_data);

static ssize_t 
spdy_get_credential_proof(spdylay_session *session, 
			  const spdylay_origin *origin,
			  uint8_t *proof, size_t prooflen, void *user_data);

static ssize_t 
spdy_get_credential_ncerts(spdylay_session *session, 
			   const spdylay_origin *origin, void *user_data);

static ssize_t
spdy_get_credential_cert(spdylay_session *session, 
			 const spdylay_origin *origin, size_t idx, 
			 uint8_t *cert, size_t certlen, void *user_data);

static void
spdy_on_ctrl_recv_parse_error_cb(spdylay_session *session, 
				 spdylay_frame_type type, const uint8_t *head, 
				 size_t headlen, const uint8_t *payload, 
				 size_t payloadlen, int error_code, 
				 void *user_data);

static void
spdy_on_unknown_ctrl_recv_cb(spdylay_session *session, const uint8_t *head, 
			     size_t headlen, const uint8_t *payload, 
			     size_t payloadlen, void *user_data);

static int forward_response_to_spdy_ssl(ssl_con_t *http_con);

static spdylay_session_callbacks s_spdy_callbacks;
static spdylay_session_callbacks *spdy_callbacks = &s_spdy_callbacks;
static ssl_server_procs_t coreprocs;


static void
spdy_reset_stream(ssl_con_t *con, spdylay_status_code st) 
{
    spdylay_submit_rst_stream(con->ctx.u.spdy.sess, 
    			      con->ctx.u.spdy.current_streamid, st);
}

static ssize_t 
spdy_send_cb(spdylay_session *session, const uint8_t *data, size_t length, 
	     int flags, void *user_data)
{
    ssize_t ret;
    ssl_con_t *con = (ssl_con_t *)user_data;

    DBG_LOG(MSG, MOD_SSL, 
	    "spdy_send_cb: fd=%d session=%p data=%p len=%ld flags=0x%x user=%p",
	    con->fd, session, data, length, flags, user_data);
    ret = SSL_write(con->ssl, data, length);
    if (ret == 0) {
    	ret = SPDYLAY_ERR_CALLBACK_FAILURE;
    } else if (ret < 0) {
	ret = SSL_get_error(con->ssl, ret);
	switch(ret) {
	case SSL_ERROR_WANT_WRITE:
	    con->ctx.flags |= FL_WANT_WRITE;
	    ret = SPDYLAY_ERR_WOULDBLOCK;
	    break;

	case SSL_ERROR_WANT_READ:
	    con->ctx.flags |= FL_WANT_READ;
	    ret = SPDYLAY_ERR_WOULDBLOCK;
	    break;

	default:
	    ret = SPDYLAY_ERR_CALLBACK_FAILURE;
	    break;
	}
    }
    return ret;
}

static ssize_t
spdy_recv_cb(spdylay_session *session, uint8_t *data, size_t length, int flags, 
	     void *user_data)
{
    ssize_t ret;
    ssl_con_t *con = (ssl_con_t *)user_data;

    DBG_LOG(MSG, MOD_SSL, 
	    "spdy_recv_cb: fd=%d session=%p buf=%p len=%ld flags=0x%x user=%p",
	    con->fd, session, data, length, flags, user_data);

    ret = SSL_read(con->ssl, data, length);
    if (ret == 0) { // EOF
    	ret = SPDYLAY_ERR_EOF;
    } else if (ret < 0) {
	ret = SSL_get_error(con->ssl, ret);
	switch(ret) {
	case SSL_ERROR_WANT_WRITE:
	    con->ctx.flags |= FL_WANT_WRITE;
	    ret = SPDYLAY_ERR_WOULDBLOCK;
	    break;

	case SSL_ERROR_WANT_READ:
	    con->ctx.flags |= FL_WANT_READ;
	    ret = SPDYLAY_ERR_WOULDBLOCK;
	    break;

	default:
	    ret = SPDYLAY_ERR_CALLBACK_FAILURE;
	    break;
	}
    }
    return ret;
}

static void add_headers(ssl_con_t *con, char **nv)
{
    char *name;
    int namelen;
    char *value;
    int valuelen;
    ProtoHTTPHeaderId id;
    uint64_t http_options = PH_ROPT_HTTP_11;

    for (int i = 0; nv[i]; i += 2) {
    	name = nv[i];
	namelen = strlen(nv[i]);
    	value = nv[i+1];
	valuelen = strlen(nv[i+1]);

	id = HTTPHdrStrToHdrToken(name, namelen);
	if ((int) id >= 0) {
	    HTTPAddKnownHeader(con->ctx.u.spdy.http_token_data, 0, id, 
	    		       value, valuelen);

	} else if (name[0] == ':') { // SPDY header
	    if (!strcasecmp(":method", name)) {
	    	if (!strcasecmp("GET", value)) {
		    http_options |= PH_ROPT_METHOD_GET;
		} else { // Only allow GET
		    spdy_reset_stream(con, SPDYLAY_INTERNAL_ERROR);
		    break;
		}
	    } else if (!strcasecmp(":scheme", name)) {
	    	if (!strcasecmp("https", value)) {
		    http_options |= PH_ROPT_SCHEME_HTTPS;
		}
	    } else if (!strcasecmp(":host", name)) {
	    	HTTPAddKnownHeader(con->ctx.u.spdy.http_token_data, 0, H_HOST,
				   value, valuelen);
	    } else if (!strcasecmp(":path", name)) {
	    	HTTPAddKnownHeader(con->ctx.u.spdy.http_token_data, 0, 
				   H_X_NKN_URI, value, valuelen);
	    } else if (!strcasecmp(":version", name)) {
	    	if (!strcasecmp("HTTP/1.1", value)) {
		    http_options |= PH_ROPT_HTTP_11;
		} else if (!strcasecmp("HTTP/1.0", value)) {
		    http_options |= PH_ROPT_HTTP_10;
		} else {
		    http_options |= PH_ROPT_HTTP_09;
		}
	    }

	} else { // unknown header
	    HTTPAddUnknownHeader(con->ctx.u.spdy.http_token_data, 0, name, 
	    			 namelen, value, valuelen);
	}
    }
    HTTPProtoSetReqOptions(con->ctx.u.spdy.http_token_data, http_options);
}

static void
spdy_on_ctrl_recv_cb(spdylay_session *session, spdylay_frame_type type, 
		     spdylay_frame *frame, void *user_data)
{
    ssl_con_t *con = (ssl_con_t *)user_data;
    int32_t stream_id;

    DBG_LOG(MSG, MOD_SSL,
	    "spdy_on_ctrl_recv_cb: session=%p type=%d frame=%p user=%p",
	    session, type, frame, user_data);

    switch (type) {
    case SPDYLAY_SYN_STREAM:
    	stream_id = frame->headers.stream_id;
	// Establish stream
	if (con->ctx.u.spdy.current_streamid < 0) {
	    con->ctx.u.spdy.current_streamid = stream_id;
	    con->ctx.u.spdy.http_token_data = NewTokenData();
	    if (con->ctx.u.spdy.http_token_data) {
	    	add_headers(con, frame->syn_stream.nv);
	    } else {
		DBG_LOG(MSG, MOD_SSL, 
			"NewTokenData() failed, reject stream id=%d fd=%d",
			stream_id, con->fd);
		spdy_reset_stream(con, SPDYLAY_ERR_NOMEM);
	    }
	} else {
	    DBG_LOG(MSG, MOD_SSL, "Reject concurrent stream, id=%d fd=%d",
		    stream_id, con->fd);
	    spdy_reset_stream(con, SPDYLAY_ERR_NOMEM);
	}
    	break;

    case SPDYLAY_HEADERS:
    	stream_id = frame->headers.stream_id;
	if (stream_id == con->ctx.u.spdy.current_streamid) {
	    add_headers(con, frame->headers.nv);
	} else {
	    DBG_LOG(MSG, MOD_SSL, "Received headers for unknown stream id=%d",
		    stream_id);
	}
    	break;

    default:
    	break;
    }
}

static void
spdy_on_invalid_ctrl_recv_cb(spdylay_session *session, spdylay_frame_type type,
			     spdylay_frame *frame, uint32_t status_code, 
			     void *user_data)
{
    ssl_con_t *con = (ssl_con_t *)user_data;
    UNUSED_ARGUMENT(con);

    DBG_LOG(MSG, MOD_SSL,
	    "spdy_on_invalid_ctrl_recv_cb: session=%p type=%d frame=%p "
	    "status=%d user=%p", session, type, frame, status_code, user_data);
}

static void 
spdy_on_data_chunk_recv_cb(spdylay_session *session, uint8_t flags, 
			   int32_t stream_id, const uint8_t *data, 
			   size_t len, void *user_data)
{
    ssl_con_t *con = (ssl_con_t *)user_data;
    UNUSED_ARGUMENT(con);

    DBG_LOG(MSG, MOD_SSL,
	    "spdy_on_data_chunk_recv_cb: session=%p flags=0x%hhx stream=%d "
	    "data=%p len=%ld user=%p", 
	    session, flags, stream_id, data, len, user_data);

}

static void 
spdy_on_data_recv_cb(spdylay_session *session, uint8_t flags, 
		     int32_t stream_id, int32_t length, void *user_data)
{
    ssl_con_t *con = (ssl_con_t *)user_data;
    UNUSED_ARGUMENT(con);

    DBG_LOG(MSG, MOD_SSL,
	    "spdy_on_data_recv_cb: session=%p flags=0x%hhx stream=%d "
	    "length=%d user=%p", session, flags, stream_id, length, user_data);
}

static void
spdy_before_ctrl_send_cb(spdylay_session *session, spdylay_frame_type type,
			 spdylay_frame *frame, void *user_data)
{
    ssl_con_t *con = (ssl_con_t *)user_data;
    UNUSED_ARGUMENT(con);

    DBG_LOG(MSG, MOD_SSL,
	    "spdy_before_ctrl_send_cb: session=%p type=%d frame=%p user=%p",
	    session, type, frame, user_data);

}

static void
spdy_on_ctrl_send_cb(spdylay_session *session, spdylay_frame_type type, 
		     spdylay_frame *frame, void *user_data)
{
    ssl_con_t *con = (ssl_con_t *)user_data;
    UNUSED_ARGUMENT(con);

    DBG_LOG(MSG, MOD_SSL,
	    "spdy_on_ctrl_send_cb: session=%p type=%d frame=%p user=%p",
	    session, type, frame, user_data);
}

static void
spdy_on_ctrl_not_send_cb(spdylay_session *session, spdylay_frame_type type, 
			 spdylay_frame *frame, int error_code, void *user_data)
{
    ssl_con_t *con = (ssl_con_t *)user_data;
    UNUSED_ARGUMENT(con);

    DBG_LOG(MSG, MOD_SSL,
	    "spdy_on_ctrl_not_send_cb: session=%p type=%d frame=%p "
	    "error=%d user=%p",
	    session, type, frame, error_code, user_data);
}

static void
spdy_on_data_send_cb(spdylay_session *session, uint8_t flags, 
		     int32_t stream_id, int32_t length, void *user_data)
{
    ssl_con_t *con = (ssl_con_t *)user_data;
    UNUSED_ARGUMENT(con);

    DBG_LOG(MSG, MOD_SSL,
	    "spdy_on_data_send_cb: session=%p flags=0x%hhx stream=%d "
	    "len=%d user=%p",
	    session, flags, stream_id, length, user_data);
}

static void
spdy_on_stream_close_cb(spdylay_session *session, int32_t stream_id, 
			spdylay_status_code status_code, void *user_data)
{
    ssl_con_t *con = (ssl_con_t *)user_data;
    UNUSED_ARGUMENT(con);

    DBG_LOG(MSG, MOD_SSL,
	    "spdy_on_stream_close_cb: session=%p strea=%d status=%d user=%p",
	    session, stream_id, status_code, user_data);

    if (stream_id == con->ctx.u.spdy.current_streamid) {
	con->ctx.u.spdy.current_streamid = -1;
	deleteHTTPtoken_data(con->ctx.u.spdy.http_token_data);
	con->ctx.u.spdy.http_token_data = 0;
	deleteHTTPtoken_data(con->ctx.u.spdy.http_resp_token_data);
	con->ctx.u.spdy.http_resp_token_data = 0;
	con->ctx.u.spdy.response_hdrs_sent = 0;
    }
}

static void
spdy_on_request_recv_cb(spdylay_session *session, int32_t stream_id, 
			void *user_data)
{
    ssl_con_t *con = (ssl_con_t *)user_data;
    int rv;
    const char *buf;
    int buflen;

    DBG_LOG(MSG, MOD_SSL,
	    "spdy_on_request_recv_cb: session=%p stream=%d user=%p",
	    session, stream_id, user_data);

    /* OTW HTTP request data */
    rv = HTTPRequest(con->ctx.u.spdy.http_token_data, &buf, &buflen);
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

static ssize_t 
spdy_get_credential_proof(spdylay_session *session, 
			  const spdylay_origin *origin,
			  uint8_t *proof, size_t prooflen, void *user_data)
{
    ssl_con_t *con = (ssl_con_t *)user_data;
    UNUSED_ARGUMENT(con);

    DBG_LOG(MSG, MOD_SSL,
	    "spdy_get_credential_proof: session=%p origin=%p proof=%p "
	    "len=%ld user=%p",
	    session, origin, proof, prooflen, user_data);
    return 0;
}

static ssize_t 
spdy_get_credential_ncerts(spdylay_session *session, 
			   const spdylay_origin *origin, void *user_data)
{
    ssl_con_t *con = (ssl_con_t *)user_data;
    UNUSED_ARGUMENT(con);

    DBG_LOG(MSG, MOD_SSL,
	    "spdy_get_credential_ncerts: session=%p origin=%p user=%p",
	    session, origin, user_data);
    return 0;
}

static ssize_t
spdy_get_credential_cert(spdylay_session *session, 
			 const spdylay_origin *origin, size_t idx,
 			 uint8_t *cert, size_t certlen, void *user_data)
{
    ssl_con_t *con = (ssl_con_t *)user_data;
    UNUSED_ARGUMENT(con);

    DBG_LOG(MSG, MOD_SSL,
	    "spdy_get_credential_cert: session=%p origin=%p idx=%ld "
	    "cert=%p certlen=%ld user=%p",
	    session, origin, idx, cert, certlen, user_data);
    return 0;
}

static void
spdy_on_ctrl_recv_parse_error_cb(spdylay_session *session, 
				 spdylay_frame_type type, const uint8_t *head, 
				 size_t headlen, const uint8_t *payload, 
				 size_t payloadlen, int error_code, 
				 void *user_data)
{
    ssl_con_t *con = (ssl_con_t *)user_data;
    UNUSED_ARGUMENT(con);

    DBG_LOG(MSG, MOD_SSL,
	    "spdy_on_ctrl_recv_parse_error_cb: session=%p type=%d head=%p "
	    "headlen=%ld payload=%p payloadlen=%ld error=%d user=%p",
	    session, type, head, headlen, payload, payloadlen, error_code,
	    user_data);
}

static void
spdy_on_unknown_ctrl_recv_cb(spdylay_session *session, const uint8_t *head, 
			     size_t headlen, const uint8_t *payload, 
			     size_t payloadlen, void *user_data)
{
    ssl_con_t *con = (ssl_con_t *)user_data;
    UNUSED_ARGUMENT(con);

    DBG_LOG(MSG, MOD_SSL,
	    "spdy_on_unknown_ctrl_recv_cb: session=%p head=%p headlen=%ld "
	    "payload=%p payloadlen=%ld user=%p",
	    session, head, headlen, payload, payloadlen, user_data);
}

/* 
 *******************************************************************************
 * External functions
 *******************************************************************************
 */
int server_spdy_init(ssl_server_procs_t *procs, server_procs_t *sprocs) 
{
    coreprocs = *procs;
    sprocs->forward_response_to_ssl = forward_response_to_spdy_ssl;

    /* SPDY callbacks */

    spdy_callbacks = &s_spdy_callbacks;
    memset(spdy_callbacks, sizeof(*spdy_callbacks), 0);

    spdy_callbacks->send_callback = spdy_send_cb;
    spdy_callbacks->recv_callback = spdy_recv_cb;
    spdy_callbacks->on_ctrl_recv_callback = spdy_on_ctrl_recv_cb;
    spdy_callbacks->on_invalid_ctrl_recv_callback = 
	spdy_on_invalid_ctrl_recv_cb;
    spdy_callbacks->on_data_chunk_recv_callback = 
	spdy_on_data_chunk_recv_cb;
    spdy_callbacks->on_data_recv_callback = spdy_on_data_recv_cb;
    spdy_callbacks->before_ctrl_send_callback = spdy_before_ctrl_send_cb;
    spdy_callbacks->on_ctrl_send_callback = spdy_on_ctrl_send_cb;
    spdy_callbacks->on_ctrl_not_send_callback = spdy_on_ctrl_not_send_cb;
    spdy_callbacks->on_data_send_callback = spdy_on_data_send_cb;
    spdy_callbacks->on_stream_close_callback = spdy_on_stream_close_cb;
    spdy_callbacks->on_request_recv_callback = spdy_on_request_recv_cb;
    spdy_callbacks->get_credential_proof = spdy_get_credential_proof;
    spdy_callbacks->get_credential_ncerts = spdy_get_credential_ncerts;
    spdy_callbacks->get_credential_cert = spdy_get_credential_cert;
    spdy_callbacks->on_ctrl_recv_parse_error_callback = 
	spdy_on_ctrl_recv_parse_error_cb;
    spdy_callbacks->on_unknown_ctrl_recv_callback = 
	spdy_on_unknown_ctrl_recv_cb;

    return 0; // Success
}

int server_spdy_setup_con(ssl_con_t *ssl_con)
{
    int ret;
    const unsigned char *next_proto = 0;
    unsigned int next_proto_len;
    int res;
    spdylay_settings_entry entry;
    int spdylay_vers;

    SSL_get0_next_proto_negotiated(ssl_con->ssl, &next_proto, &next_proto_len);
#if OPENSSL_VERSION_NUMBER >= 0x10002000L
    if (!next_proto) {
    	SSL_get0_alpn_selected(ssl_con->ssl, &next_proto, &next_proto_len);
    }
#endif

    while (1) {

    spdylay_vers = spdylay_npn_get_version(next_proto, next_proto_len);
    if (spdylay_vers == SPDYLAY_PROTO_SPDY3_1) {
	res = spdylay_session_server_new(
			    &ssl_con->ctx.u.spdy.sess,
			    SPDYLAY_PROTO_SPDY3_1,
			    spdy_callbacks, ssl_con);
	if (res != 0) {
	    DBG_LOG(WARNING, MOD_SSL, "spdylay_session_server_new() failed");

	    ret = 1; // Error
	    break;
	}

	entry.settings_id = SPDYLAY_SETTINGS_MAX_CONCURRENT_STREAMS;
	entry.value = 1; // Disable concurrent streams
	entry.flags = SPDYLAY_ID_FLAG_SETTINGS_NONE;
	res = spdylay_submit_settings(ssl_con->ctx.u.spdy.sess, 
			      SPDYLAY_FLAG_SETTINGS_NONE, &entry, 1);
	if (res != 0) {
	    DBG_LOG(WARNING, MOD_SSL, "spdylay_submit_settings() failed");
	    spdylay_session_del(ssl_con->ctx.u.spdy.sess);
	    ssl_con->ctx.u.spdy.sess = 0;

	    ret = 2; // Error
	    break;
	}

	ssl_con->ctx.u.spdy.http_token_data = 0;
	ssl_con->ctx.u.spdy.http_resp_token_data = 0;
	ssl_con->ctx.u.spdy.current_streamid = -1;
	SET_CON_FLAG(ssl_con, CONF_SPDY3_1);

	ret = 0; // Success
    } else {
    	ret = -1; // Invalid version
    }
    break;

    } // End while

    return ret;
}

int spdy_close_conn(ssl_con_t *ssl_con)
{
    if (ssl_con->ctx.u.spdy.sess) {
	spdylay_session_del(ssl_con->ctx.u.spdy.sess);
	ssl_con->ctx.u.spdy.sess = 0;
	deleteHTTPtoken_data(ssl_con->ctx.u.spdy.http_token_data);
	ssl_con->ctx.u.spdy.http_token_data = 0;
	deleteHTTPtoken_data(
		ssl_con->ctx.u.spdy.http_resp_token_data);
	ssl_con->ctx.u.spdy.http_resp_token_data = 0;
    }
    return 0;
}

static int spdy_sess_done(spdylay_session *sess)
{
    return !spdylay_session_want_read(sess) &&
	   !spdylay_session_want_write(sess);
}

static void spdy_setup_epoll(ssl_con_t *con)
{
    if (spdylay_session_want_read(con->ctx.u.spdy.sess) || 
    	con->ctx.flags & FL_WANT_READ) {
	if (!NM_add_event_epollin(con->fd)) {
	    (*coreprocs.close_conn)(con->fd);
	}
    } else if (spdylay_session_want_write(con->ctx.u.spdy.sess) ||
    	       con->ctx.flags & FL_WANT_WRITE) {
	if (!NM_add_event_epollout(con->fd)) {
	    (*coreprocs.close_conn)(con->fd);
	}
    }
}

static int spdy_poll_handler(ssl_con_t *con, const char *caller_str) 
{
    int r;

    con->ctx.flags &= ~(FL_WANT_READ | FL_WANT_WRITE);

    r = spdylay_session_recv(con->ctx.u.spdy.sess);
    if (r == 0) {
	r = spdylay_session_send(con->ctx.u.spdy.sess);
	if (r == 0) {
	    if (spdy_sess_done(con->ctx.u.spdy.sess)) { // done
	    	(*coreprocs.close_conn)(con->fd);
	    } else { // still active
	    	spdy_setup_epoll(con);
	    }
	} else { // send error
	    DBG_LOG(MSG, MOD_SSL, 
		    "%s:spdylay_session_send() failed, rv=%d, fd=%d",
		    caller_str, r, con->fd);
	    (*coreprocs.close_conn)(con->fd);
	}
    } else { // recv error
	DBG_LOG(MSG, MOD_SSL, 
		"%s:spdylay_session_recv() failed, rv=%d, fd=%d",
		caller_str, r, con->fd);
	(*coreprocs.close_conn)(con->fd);
    }
    return TRUE;
}

int ssl_spdy_epollin(int sockfd, void * private_data)
{
    ssl_con_t *con = (ssl_con_t *)private_data;
    int r;

    DBG_LOG(MSG, MOD_SSL, "fd=%d called", sockfd);
    return spdy_poll_handler(con, "IN");
}

int ssl_spdy_epollout(int sockfd, void * private_data)
{
    ssl_con_t *con = (ssl_con_t *)private_data;

    DBG_LOG(MSG, MOD_SSL, "fd=%d called", sockfd);
    return spdy_poll_handler(con, "OUT");
}

int ssl_spdy_epollerr(int sockfd, void * private_data)
{
    DBG_LOG(MSG, MOD_SSL, "fd=%d called", sockfd);
    return (*coreprocs.ssl_epollerr)(sockfd, private_data);
}

int ssl_spdy_epollhup(int sockfd, void * private_data)
{
    DBG_LOG(MSG, MOD_SSL, "fd=%d called", sockfd);
    return (*coreprocs.ssl_epollhup)(sockfd, private_data);
}

int ssl_spdy_timeout(int sockfd, void * private_data, double timeout)
{
    DBG_LOG(MSG, MOD_SSL, "fd=%d called", sockfd);
    return (*coreprocs.ssl_timeout)(sockfd, private_data, timeout);
}

static ssize_t 
http_source_read_cb(spdylay_session *session, int32_t stream_id,  
		    uint8_t *buf, size_t length, int *eof,
		    spdylay_data_source *source, void *user_data)
{
    ssl_con_t *ssl_con = (ssl_con_t *) user_data;
    ssl_con_t *http_con = (ssl_con_t *) source->ptr;

    if (stream_id != ssl_con->ctx.u.spdy.current_streamid) {
    	return SPDYLAY_ERR_CALLBACK_FAILURE;
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
			ssl_con->ctx.u.spdy.http_resp_content_len)) {
		*eof = 1;
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
	    return SPDYLAY_ERR_DEFERRED;
	} else {
	    *eof = 1;
	}
    }
    return bytes_to_xfer;
}

static void push_headers_data(ssl_con_t *ssl_con, const char **nv)
{
    ssl_con_t *http_con = (ssl_con_t *)gnm[ssl_con->peer_fd].private_data;
    int ret;
    spdylay_data_provider data_prd;


    /* Start transfer of headers and data to ssl SPDY client */
    data_prd.source.ptr = http_con;
    data_prd.read_callback = http_source_read_cb;

    ret = spdylay_submit_response(ssl_con->ctx.u.spdy.sess, 
    				  ssl_con->ctx.u.spdy.current_streamid, 
				  nv, &data_prd);
}

static int forward_response_to_spdy_ssl(ssl_con_t *http_con)
{
    ssl_con_t *ssl_con = gnm[http_con->peer_fd].private_data;
    proto_data_t pd;
    HTTPProtoStatus status;
    int header_bytes;
    int bytes_avail;
    int http_respcode;

    int rv;
    char *nv;
    int nv_len;
    int nv_count;
    int eof;

    const char *val;
    int vallen;
    int hdrcnt;

    if (!ssl_con->ctx.u.spdy.response_hdrs_sent) {
    	memset(&pd, 0, sizeof(proto_data_t));
	pd.u.HTTP.data = &http_con->cb_buf[http_con->cb_offsetlen];
	pd.u.HTTP.datalen = http_con->cb_totlen - http_con->cb_offsetlen;

    	ssl_con->ctx.u.spdy.http_resp_token_data = 
	    HTTPProtoRespData2TokenData(0, &pd, &status, &http_respcode);
	if (status == HP_SUCCESS) {

	    rv = HTTPGetKnownHeader(ssl_con->ctx.u.spdy.http_resp_token_data, 
				    0, H_CONTENT_LENGTH, &val, 
				    &vallen, &hdrcnt);
	    if (!rv) {
	    	char valstr[128];
		memcpy(valstr, val, MIN(vallen, 127));
		valstr[127] = '\0';
	    	ssl_con->ctx.u.spdy.http_resp_content_len = 
			strtoll(valstr, 0, 10);
	    } else {
	    	ssl_con->ctx.u.spdy.http_resp_content_len = ULLONG_MAX;
	    }

	    header_bytes = HTTPHeaderBytes(
	    	ssl_con->ctx.u.spdy.http_resp_token_data);
	    http_con->cb_offsetlen += header_bytes;

	    /* Convert headers to SPDY format and push to client */
	    rv = HTTPHdr2NV(ssl_con->ctx.u.spdy.http_resp_token_data, 0, 
			    FL_2NV_SPDY_PROTO,
			    &nv, &nv_len, &nv_count);
	    if (!rv) {
	    	push_headers_data(ssl_con, (const char **)nv);
	    	ssl_con->ctx.u.spdy.response_hdrs_sent = 1;
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
	    spdylay_session_resume_data(ssl_con->ctx.u.spdy.sess, 
	    				ssl_con->ctx.u.spdy.current_streamid);
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
 * End of server_spdy.c
 */
