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

int net_epollin(struct socku *sku);
int net_epollout(struct socku *sku);
int net_epollerr(struct socku *sku);
int net_epollhup(struct socku *sku);
int add_net(struct network *net);
int remove_net(struct network *net);
uint32_t parse_ip(char *ip);
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

int32_t epfd = 0;


uint32_t parse_ip(char *ip)
{
    struct in_addr in;

    in.s_addr = inet_addr(ip);
    if (in.s_addr == (unsigned) (~0)) {
	return 0;
    } else {
	return ntohl(in.s_addr);
    }

}

int parse_args(int argc, char **argv)
{
    int n = 1; // how many args we parsed.
    int c;

    while((c = getopt(argc, argv, ":s:m:D:M:n:N:k:p:v")) != -1) {
	switch(c) {
	    case 's':
		s_sip = parse_ip(optarg);n++;
		break;
	    case 'D':
		s_dip = parse_ip(optarg);n++;
		break;
	    case 'n':
		port_per_client = atoi(optarg);n++;
		break;
	    case 'N':
		max_port_client = atoi(optarg);n++;
		break;
	    case 'k':
		ka = atoi(optarg);n++;
		break;
	    case 'p':
		port_base = atoi(optarg); n++;
		break;
	    case 'v':
		debug = 1;
		break;
	    case '?':
		fprintf(stderr, "Unrecognized option: -%c\n", optopt);
		break;
	}
	n++;
    }

    selfconf.max_fd = max_fd;

    return n;
}

int set_limits(void)
{
    struct rlimit rlim;


    rlim.rlim_cur = selfconf.max_fd; // Just set to 64K
    rlim.rlim_max = selfconf.max_fd; // Just set to 64K

    if (0 != setrlimit(RLIMIT_NOFILE, &rlim)) {
	perror("setrlimit");
	return -1;
    }


    return 0;
}


int setnonblock(int fd)
{
    int opt = 0;

    opt = fcntl(fd, F_GETFL);
    if (opt < 0) {
	return -errno;
    }

    opt = (opt | O_NONBLOCK);
    if (fcntl(fd, F_SETFL, opt) < 0) {
	return -errno;
    }

    return 0;
}

#endif
