/*
 * ssl_server_proto.c -- Support for SPDY/HTTP2
 */

#ifdef HTTP2_SUPPORT
/*
 *******************************************************************************
 * Experimental SPDY/HTTP2 implementation, not for general use.
 *******************************************************************************
 */

#include <sys/types.h>
#include <sys/param.h>
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

#include "ssl_server_proto.h"

extern network_mgr_t *gnm;
extern int enable_spdy;
extern int enable_http2;

proto_vers_def_t ssl_next_proto_def;

/* Configure supported versions */

proto_version_list_t s_http2_version_list[4] = {
    {(unsigned char*)HTTP2_VERSION_STR, HTTP2_VERSION_LEN, HTTP2_VER},
    {(unsigned char*)HTTP2_14_VERSION_STR, HTTP2_14_VERSION_LEN, HTTP2_VER_14},
    {(unsigned char*)HTTP2_16_VERSION_STR, HTTP2_16_VERSION_LEN, HTTP2_VER_16},
    {0, 0, -1} // Always last entry
};
const proto_version_list_t *http2_version_list = &s_http2_version_list[0];

proto_version_list_t s_spdy_version_list[2] = {
    {(unsigned char*)SPDY31_VERSION_STR, SPDY31_VERSION_LEN, SPDY_VER_31},
    {0, 0, -1} // Always last entry
};
const proto_version_list_t *spdy_version_list = &s_spdy_version_list[0];

server_procs_t server_spdy_procs;
server_procs_t server_http2_procs;

static int ssld_bio_write(BIO *, const char *, int);
static int ssld_bio_read(BIO *, char *, int);
static int ssld_bio_puts(BIO *, const char *);
static int ssld_bio_gets(BIO *, char *, int);
static long ssld_bio_ctrl(BIO *, int, long, void *);
static int ssld_bio_create(BIO *);
static int ssld_bio_destroy(BIO *);
static long ssld_bio_callback_ctrl(BIO *, int, bio_info_cb *);

static BIO_METHOD ssld_bio_methods = {
    BIO_TYPE_SOCKET,
    "ssld bio",
    ssld_bio_write,
    ssld_bio_read,
    ssld_bio_puts,
    NULL,
    ssld_bio_ctrl,
    ssld_bio_create,
    ssld_bio_destroy,
    NULL
};

static BIO_METHOD *bio_sock_methods;

int ssl_server_proto_init()
{
    bio_sock_methods = BIO_s_socket();
    return 0;
}

BIO *BIO_ssld_new_socket(int fd, int close_flag)
{
    BIO *bio;
    bio = BIO_new(&ssld_bio_methods);
    if (bio) {
        BIO_set_fd(bio, fd, close_flag);
        return bio;
    }
    return NULL;
}

static int ssld_bio_write(BIO *b, const char *out, int outl)
{
    return bio_sock_methods->bwrite(b, out, outl);
}

static int ssld_bio_read(BIO *b, char *in, int inl)
{
    ssl_con_t *ssl_con = (ssl_con_t *)gnm[b->num].private_data;
    ssl_hello_data_t *hd = &ssl_con->ctx.hd;
    int bytes;

    if (hd->consumed < hd->avail) {
    	bytes = MIN(inl, (hd->avail - hd->consumed));
    	memcpy(in, &hd->buf[hd->consumed], bytes);
	hd->consumed += bytes;
	return bytes;
    } else {
    	return bio_sock_methods->bread(b, in, inl);
    }
}

static int ssld_bio_puts(BIO *b, const char *in)
{
    return bio_sock_methods->bputs(b, in);
}

static int ssld_bio_gets(BIO *b, char *in, int inl)
{
    return bio_sock_methods->bgets(b, in, inl);
}

static long ssld_bio_ctrl(BIO *b, int cmd, long larg, void *ptr)
{
    return bio_sock_methods->ctrl(b, cmd, larg, ptr);
}

static int ssld_bio_create(BIO *b)
{
    return bio_sock_methods->create(b);
}

static int ssld_bio_destroy(BIO *b)
{
    return bio_sock_methods->destroy(b);
}

static long ssld_bio_callback_ctrl(BIO *b, int cmd, bio_info_cb *cb)
{
    return bio_sock_methods->callback_ctrl(b, cmd, cb);
}

int process_client_hello(ssl_con_t *ssl_con, int *errcode)
{
    ssl_hello_data_t *hd = &ssl_con->ctx.hd;
    int terminate = 0;
    ssize_t bytes;
    unsigned char *p;

    while (1) {

    switch (hd->state) {
    case HD_DONE:
    	return PCH_RET_CONTINUE;

    case HD_INIT:
    	hd->buflen = 16 * 1024;
    	hd->avail = 0;
    	hd->consumed = 0;
    	hd->buf = calloc(1, hd->buflen);
	if (!hd->buf) {
	    *errcode = 100;
	    terminate = 1;
	    break;
	}
	hd->state = HD_READ_HEADER;
    	break;

    case HD_READ_HEADER:
    	bytes = recv(ssl_con->fd, &hd->buf[hd->avail], 
		     hd->buflen - hd->avail, 0);
	if (bytes <= 0) {
	    if (!bytes || ((bytes < 0) && (errno == EAGAIN))) {
	    	return PCH_RET_NEED_READ;
	    }
	    *errcode = 2000 + errno;
	    terminate = 1;
	    break;
	}
	hd->avail += bytes;

	if (hd->avail < 5) {
	    return PCH_RET_NEED_READ;
	}

	if (hd->buf[0] == 0x16) {
	    /* SSL V3 hello message */
	    hd->hellosize = (hd->buf[3] << 8) + hd->buf[4];
	    hd->hellosize += 5;
	} else if ((hd->buf[0] & 0x80) && 
		   (hd->buf[2] == 0x1) && (hd->buf[3] == 0x3)) {
	    /* SSL V2 with V3 support */
	    hd->hellosize = hd->buf[1];
	    hd->hellosize += 5;
	} else {
	    /* No SNI or ALPN, proceed with standard flow */
	    hd->state = HD_DONE;
	    return PCH_RET_CONTINUE;
	}
	hd->state = HD_READ_HELLO;
    	break;

    case HD_READ_HELLO:
    	if (hd->hellosize > hd->buflen) {
	    p = (unsigned char *)realloc(hd->buf, hd->hellosize);
	    if (p) {
	    	hd->buf = p;
		hd->buflen = hd->hellosize;
	    } else {
	    	/* realloc() failed */
		*errcode = 300;
		terminate = 1;
		break;
	    }
	}

	if (hd->hellosize > hd->avail) {
	    bytes = recv(ssl_con->fd, &hd->buf[hd->avail], 
			 hd->buflen - hd->avail, 0);
	    if (bytes <= 0) {
	    	if (!bytes || ((bytes < 0) && (errno == EAGAIN))) {
		    return PCH_RET_NEED_READ;
	    	}
	    	*errcode = 4000 + errno;
	    	terminate = 1;
	    	break;
	    }
	    hd->avail += bytes;
	} else {
	    hd->state = HD_PROCESS_HELLO;
	}
    	break;

    case HD_PROCESS_HELLO:
    	/* Analyze the client hello and determine action */

	/* For now, do nothing and proceeed with SSL_accept() */
	hd->state = HD_DONE;
	return PCH_RET_CONTINUE;
    	break;

    default:
    	*errcode = 500;
    	terminate = 1;
	break;
    }

    if (terminate) {
    	break;
    }

    } // End of while

    return PCH_RET_ERROR;
}

void add_ssl_next_proto_def(proto_vers_def_t *def, const unsigned char *vers)
{
    int slen = strlen((char *)vers);

    if ((def->vers_len + slen + 1) <= (int)sizeof(def->vers)) {
	def->vers[def->vers_len] = slen;
	def->vers_len++;
	memcpy(&def->vers[def->vers_len], vers, slen);
	def->vers_len += slen;
    }
}

void server_close_conn(ssl_con_t *ssl_con)
{
    if (ssl_con->ctx.hd.buf) {
    	free(ssl_con->ctx.hd.buf);
	ssl_con->ctx.hd.buf = NULL;
    }
    if (CHECK_CON_FLAG(ssl_con, CONF_HTTP2)) {
	http2_close_conn(ssl_con);
    } else if (CHECK_CON_FLAG(ssl_con, CONF_SPDY3_1)) {
	spdy_close_conn(ssl_con);
    } 
}

int setup_http2_con(ssl_con_t *ssl) 
{
    int ret;

    if (enable_http2 || enable_spdy) {
	if (enable_http2) {
	    ret = server_http2_setup_con(ssl);
	    if (!ret || (ret > 0)) {
		// Success or supported version setup error
		return ret;
	    }
	}

	if (enable_spdy) {
	    ret = server_spdy_setup_con(ssl);
	    if (!ret || (ret > 0)) {
		// Success or supported version setup error
		return ret;
	    }
	}

	return -1; // Invalid HTTP2 version
    } else {
	return 0; // HTTP2 not enabled
    }
}

int re_register_socket(ssl_con_t *ssl_con, int ssl_fd)
{
    int ret;

    if (CHECK_CON_FLAG(ssl_con, CONF_HTTP2)) {
	unregister_NM_socket(ssl_fd);
	ret = register_NM_socket(ssl_fd,
				    (void *)ssl_con,
				    ssl_http2_epollin,
				    ssl_http2_epollout,
				    ssl_http2_epollerr,
				    ssl_http2_epollhup,
				    ssl_http2_timeout,
				    ssl_idle_timeout,
				    -1);
    } else if (CHECK_CON_FLAG(ssl_con, CONF_SPDY3_1)) {
	unregister_NM_socket(ssl_fd);
	ret = register_NM_socket(ssl_fd,
				    (void *)ssl_con,
				    ssl_spdy_epollin,
				    ssl_spdy_epollout,
				    ssl_spdy_epollerr,
				    ssl_spdy_epollhup,
				    ssl_spdy_timeout,
				    ssl_idle_timeout,
				    -1);
    }
    return !ret;
}

int next_proto_cb(SSL *s, const unsigned char **data, 
		  unsigned int *len, void *arg)
{
    proto_vers_def_t *d = (proto_vers_def_t *) arg;

    *data = d->vers;
    *len = d->vers_len;
    return SSL_TLSEXT_ERR_OK;
}

int tokenize_proto_data(const unsigned char *in, unsigned int inlen,
			token_proto_data_t *out, int out_maxentries,
			int *out_entries)
{
    int ix = 0;
    int slen;

    *out_entries = 0;
    if (!inlen) {
    	return 0; // Done
    }
    slen = in[ix];

    while ((ix + 1 + slen) <= (int)inlen) {
    	if (*out_entries < out_maxentries) {
	    out[*out_entries].data = &in[ix+1];
	    out[*out_entries].datalen = slen;
	    (*out_entries)++;
	    ix += (1 + slen);

	    if (ix < (int)inlen) {
	    	slen = in[ix];
	    } else {
	    	return 0; // Done
	    }
	} else {
	    return 1; // Short output buffer
	}
    }
    return 0;
}


int alpn_select_proto_cb(SSL *s, const unsigned char **out,
                         unsigned char *outlen, const unsigned char *in,
                         unsigned int inlen, void *arg)
{
    proto_vers_def_t *d = (proto_vers_def_t *) arg;

    token_proto_data_t tk_in[64];
    int tk_in_entries;
    token_proto_data_t *tk_option;
    int tk_option_entries;
    int n_key;
    int n_option;
    int rv;

    rv = tokenize_proto_data(in, inlen, tk_in, 
			     (int)(sizeof(tk_in)/sizeof(token_proto_data_t)),
			     &tk_in_entries);
    if (rv) {
    	return SSL_TLSEXT_ERR_NOACK;
    }

    tk_option = d->tk_proto;
    tk_option_entries = d->tk_proto_entries;

    for (n_key = 0; n_key < tk_in_entries; n_key++) {
    	for (n_option = 0; n_option < tk_option_entries; n_option++) {
	    if (tk_in[n_key].datalen == tk_option[n_option].datalen) {
	    	if (!memcmp(tk_in[n_key].data, tk_option[n_option].data, 
			    tk_option[n_option].datalen)) {
		    *out = tk_option[n_option].data;
		    *outlen = tk_option[n_option].datalen;
		    return SSL_TLSEXT_ERR_OK;
		}
	    }
	}
    }
    return SSL_TLSEXT_ERR_NOACK;
}

#endif /* HTTP2_SUPPORT */

/*
 * End of ssl_server_proto.c
 */
