/*
 * server_http2.h
 */
#ifndef SERVER_HTTP2_H
#define SERVER_HTTP2_H
#include "server_common.h"

#define HTTP2_VER 0
#define HTTP2_VERSION_STR "h2"
#define HTTP2_VERSION_LEN 2

#define HTTP2_VER_14 1
#define HTTP2_14_VERSION_STR "h2-14"
#define HTTP2_14_VERSION_LEN 5

#define HTTP2_VER_16 2
#define HTTP2_16_VERSION_STR "h2-16"
#define HTTP2_16_VERSION_LEN 5

extern int server_http2_init(ssl_server_procs_t *procs, server_procs_t *sprocs);

extern int server_http2_setup_con(ssl_con_t *ssl_con);

extern int ssl_http2_epollin(int fd, void *private_date);
extern int ssl_http2_epollout(int fd, void *private_data);
extern int ssl_http2_epollerr(int fd, void *private_data);
extern int ssl_http2_epollhup(int fd, void *private_data);
extern int ssl_http2_timeout(int fd, void * private_data, double timeout);

extern int http2_close_conn(ssl_con_t *ssl_con);

extern const proto_version_list_t *http2_version_list;

typedef enum {
    PHDR_UNKNOWN = 0,
    PHDR_AUTHORITY = 1,
    PHDR_METHOD,
    PHDR_PATH,
    PHDR_SCHEME,
    PHDR_STATUS,
    PHDR_DEFINED // Always last
} pseudo_header_t;

#endif /* SERVER_HTTP2_H */

/*
 * End of server_http2.h
 */
