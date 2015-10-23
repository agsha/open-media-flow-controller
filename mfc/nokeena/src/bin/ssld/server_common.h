/*
 * server_common.h -- Common server SPDY/HTTP2 definitions
 */
#ifndef SERVER_COMMON_H
#define SERVER_COMMON_H
#include "ssl_defs.h"
#include "ssl_server.h"
#include "ssl_interface.h"
#include "ssl_network.h"

typedef struct ssl_server_procs {
    int (*ssl_epollin)(int fd, void *private_data);
    int (*ssl_epollout)(int fd, void *private_data);
    int (*ssl_epollerr)(int fd, void *private_data);
    int (*ssl_epollhup)(int fd, void *private_data);
    int (*ssl_timeout)(int fd, void *private_data, double timeout);
    void (*close_conn)(int fd);
    int (*forward_request_to_nvsd)(ssl_con_t *ssl_con);
} ssl_server_procs_t;

typedef struct server_procs {
    int (*forward_response_to_ssl)(ssl_con_t *http_con);
} server_procs_t;

typedef struct proto_version_list {
    const unsigned char *ver_str;
    unsigned int ver_strlen;
    int version;
} proto_version_list_t;

extern int 
streq(const unsigned char *str1, unsigned int str1_len,
      const unsigned char *str2, unsigned int str2_len);


#endif /* SERVER_COMMON_H */

/*
 * End of server_common.h
 */
