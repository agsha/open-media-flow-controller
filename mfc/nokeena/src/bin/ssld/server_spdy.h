/*
 * server_spdy.h
 */
#ifndef SERVER_SPDY_H
#define SERVER_SPDY_H
#include "server_common.h"

#define SPDY_VER_31 0
#define SPDY31_VERSION_STR "spdy/3.1"
#define SPDY31_VERSION_LEN 8

extern int server_spdy_init(ssl_server_procs_t *procs, server_procs_t *sprocs);

extern int server_spdy_setup_con(ssl_con_t *ssl_con);

extern int ssl_spdy_epollin(int fd, void *private_date);
extern int ssl_spdy_epollout(int fd, void *private_data);
extern int ssl_spdy_epollerr(int fd, void *private_data);
extern int ssl_spdy_epollhup(int fd, void *private_data);
extern int ssl_spdy_timeout(int fd, void * private_data, double timeout);

extern int spdy_close_conn(ssl_con_t *ssl_con);

extern const proto_version_list_t *spdy_version_list;

#endif /* SERVER_SPDY_H */

/*
 * End of server_spdy.h
 */
