#ifndef __COMMON_C_
#define __COMMON_C_

#define __USE_MISC  1
#include <sys/param.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define IP_TRANSPARENT 19


/*
 * Args
 *  -s	<source IP base>
 *  -m	<source mask>
 *  -D  <dest IP base>
 *  -M	<dest mask>
 *  -n  <ports per client>
 *  -N  <Max ports per client>
 *  -v.  <debug>
 *  -k  <Keep-Alive time>
 */
#if 0
int net_epollin(struct socku *sku);
int net_epollout(struct socku *sku);
int net_epollerr(struct socku *sku);
int net_epollhup(struct socku *sku);
//int add_net(struct network *net);
//int remove_net(struct network *net);
int parse_args(int argc, char **argv);
int set_limits(void);
int setnonblock(int fd);

static uint32_t	max_fd = 4096;

static uint32_t	s_sip = 0;
//static uint32_t n_sources = 0;
//static uint32_t n_dests = 0;
//static uint32_t s_mask = 0;
//static uint32_t d_mask = 0;
static uint32_t s_dip = 0;
static uint16_t dport = 80;
static uint32_t	ka = 0;
static uint32_t max_port_client = 4096;
static uint32_t port_per_client = 2048;
static uint16_t port_base = 30000;
static uint32_t debug = 0;
#endif

#endif
