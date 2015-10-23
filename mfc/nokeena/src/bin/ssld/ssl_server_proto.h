/*
 * ssl_server_proto.h -- Support for SPDY/HTTP2
 */
#ifndef SSL_SERVER_PROTO_H_
#define SSL_SERVER_PROTO_H_

#include "openssl/ssl.h"

#include "ssl_defs.h"
#include "nkn_debug.h"
#include "nkn_stat.h"
#include "nkn_assert.h"
#include "nkn_lockstat.h"
#include "ssl_server.h"

#include "server_common.h"
#include "server_spdy.h"
#include "server_http2.h"
#include "ssl_server_proto.h"

typedef struct token_proto_data {
    const unsigned char *data;
    unsigned int datalen;
} token_proto_data_t;

typedef struct proto_vers_def {
    unsigned char vers[1024]; // Wire format
    int vers_len;
    token_proto_data_t tk_proto[64];
    int tk_proto_entries;
} proto_vers_def_t;

extern server_procs_t server_spdy_procs;
extern server_procs_t server_http2_procs;
extern proto_vers_def_t ssl_next_proto_def;

int ssl_server_proto_init(void);

BIO *BIO_ssld_new_socket(int sock, int close_flag);

/* process_client_hello() return values */
#define PCH_RET_ERROR 		-1
#define PCH_RET_CONTINUE 	0
#define PCH_RET_NEED_READ	1
#define PCH_RET_NEED_WRITE	2
#define PCH_RET_CLOSE_CONN	3
#define PCH_RET_TCP_TUNNEL	4

int process_client_hello(ssl_con_t *ssl_con, int *errcode);

void add_ssl_next_proto_def(proto_vers_def_t *def, const unsigned char *vers);
void server_close_conn(ssl_con_t *ssl_con);
int setup_http2_con(ssl_con_t *ssl);
int re_register_socket(ssl_con_t *ssl_con, int ssl_fd);
int next_proto_cb(SSL *s, const unsigned char **data,
                  unsigned int *len, void *arg);
int tokenize_proto_data(const unsigned char *in, unsigned int inlen,
                        token_proto_data_t *out, int out_maxentries, 
			int *out_entries);
int alpn_select_proto_cb(SSL *s, const unsigned char **out,
                         unsigned char *outlen, const unsigned char *in,
                         unsigned int inlen, void *arg);
#endif /* SSL_SERVER_PROTO_H_ */

/*
 * End of ssl_server_proto.h
 */
