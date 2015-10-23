/*
 * zeroconf.c
 *
 * Copyright (c) 2006 Anand Kumria
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * See the file COPYING for the full details
 *
 */

#undef __STRICT_ANSI__
#define _ISOC99_SOURCE

#include <sys/types.h>
#include <sys/socket.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <arpa/inet.h>
#include <net/if_arp.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

#include "delay.h"

/* constants from RFC2937 */
#define PROBE_WAIT           1 /*second  (initial random delay)              */
#define PROBE_MIN            1 /*second  (minimum delay till repeated probe) */
#define PROBE_MAX            2 /*seconds (maximum delay till repeated probe) */
#define PROBE_NUM            3 /*         (number of probe packets)          */
#define ANNOUNCE_NUM         2 /*        (number of announcement packets)    */
#define ANNOUNCE_INTERVAL    2 /*seconds (time between announcement packets) */
#define ANNOUNCE_WAIT        2 /*seconds  (delay before announcing)          */
#define MAX_CONFLICTS       10 /*        (max conflicts before rate limiting)*/
#define RATE_LIMIT_INTERVAL 60 /*seconds (delay between successive attempts) */
#define DEFEND_INTERVAL     10 /*seconds (min. wait between defensive ARPs)  */

/* compilation constants */
#define NLBUF 512
#define ARP_IP_PROTO 2048
#define IPV4_LLBASE 0xa9fe0000 /* 169.254.0.0 */
#define FAILURE_INTERVAL 0  /* 0 -> no failure, otherwise max seconds */

/* some helpful macros */
#define FLAG_TEST_DUMP(x, y) if (x & y) fprintf(stderr, "%s ", #y);

/* our state machine */
enum {
  ADDR_INIT,
  ADDR_PROBE,
  ADDR_CLAIM,
  ADDR_TAKE,
  ADDR_DEFEND,
  ADDR_FINAL,
  ADDR_RELEASE
};

/* structures */
struct intf {
  char              name[IFNAMSIZ+1];
  int               index;
  unsigned int      flags;
  int               up;
  struct ether_addr mac;
  struct in_addr    ip;
};

struct arp_packet {
  struct arphdr       arp;
  struct ether_addr   sender_mac;
  struct in_addr      sender_ip;
  struct ether_addr   target_mac;
  struct in_addr      target_ip;
  unsigned char       reserved[18];
} __attribute__ ((packed));

/* forward declarations */
void check_args(int argc, char * const argv[], struct intf *intf, 
                char script[]);
void zeroconf_counter_reset(void);
int  netlink_open(int proto, __u32 groups);
int  netlink_close(int nl);
int  netlink_send(int nl, struct nlmsghdr *n);
void netlink_dump(struct nlmsghdr *n);
void netlink_dump_rta(struct rtattr *rta, int length, int family);
int  netlink_is_link_up(struct intf *intf, struct nlmsghdr *n);
int  netlink_is_from_us(struct nlmsghdr *n);
void netlink_is_our_iface(struct intf *intf, struct nlmsghdr *n);
int  netlink_qualify(struct nlmsghdr *n, size_t length);
void do_addr_add(const char *script, int nl, struct intf *intf);
void do_addr_del(const char *script, int nl, struct intf *intf);
void netlink_addr_add(int nl, struct intf *intf);
void netlink_addr_del(int nl, struct intf *intf);
int  check_ifname_exists(int nl, struct intf *intf);
int  check_ifname_type(struct intf *intf);
int  arp_open(struct intf *intf);
int  arp_conflict(struct intf *intf, struct arp_packet *pkt);
void arp_packet_dump(struct arp_packet *pkt);
void arp_packet_send(int as,
		     struct intf *intf,
		     short int arp_op,
		     int null_sender);
void arp_probe(int as, struct intf *intf);
void arp_claim(int as, struct intf *intf);
void arp_defend(int as, struct intf *intf);
void usage(void);
int  signal_install(int signo);
int addattr_l(struct nlmsghdr *n, unsigned int maxlen, int type, const void *data, int alen);
static void run_script(const char *script, const char *arg, char *intf, 
                       struct in_addr *ip);

/* globals */
int force = 0;
int verbose = 0;
int nofork = 0;
int quit = 0;
char script[PATH_MAX];

/* 
 * "7" means 7 / 24's : 169.254.1 / 24 -- 169.254.7 / 24 will not be
 * used by zeroconf, and are available for use in other addressing
 * schemes.  Note that the 169.254.0 / 24 and 169.254.255 / 24 ranges
 * are reserved by the protocol itself, and should never be used.
 *
 * Having this be "7" instead of "0" means there are about 3% fewer
 * addresses in the random pool.  This is not thought to be significant,
 * as there are still more than 60,000 addresses available.
 */
unsigned int addr_reserve_base_block = 7;

pid_t mypid = -1;

int probe_count = 0;
int conflict_count = 0;
int claim_count = 0;
int address_picked = 0;
int fail_by_time = 0;

int self_pipe[2] = { -1, -1 };

static void sig_handler(int signo)
{
    int i;
    do {
        i = write(self_pipe[1], &signo, sizeof(signo));
        if (i < 0 && errno == EINTR) {
            continue;
        }
        break;
    } while(1);
}

/* Public domain jenkins hash */
static uint32_t hash_one_at_a_time(const unsigned char *key, size_t len)
{
    uint32_t   hash = 0;
    size_t i = 0;

    for (i = 0; i < len; i++) {
        hash += key[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return (hash);
} 

int main(int argc, char **argv)
{

  int nl, as;
  int keep_running = 1;
  struct itimerval  delay;
  char pidloc[PATH_MAX];
  int prev_up = 1;
  uint32_t seed = 0;
  unsigned int base_addrs = 0;

  struct intf intf = {
    .index = -1,
    .flags = 0,
    .up = 0,
  };

  openlog("zeroconf", LOG_NDELAY | LOG_PID, LOG_USER);

  check_args(argc, argv, &intf, script);

  if ((nl = netlink_open(NETLINK_ROUTE, 0)) < 0) {
    log_it(LOG_ERR, "unable to connect to netlink\n");
    exit(1);
  }

  if (check_ifname_exists(nl, &intf) < 0) {
    log_it(LOG_ERR, "Interface %s does not exist\n", intf.name);
    exit(1);
  }

  if ((check_ifname_type(&intf) < 0) && (!force)) {
    log_it(LOG_ERR, "Interface %s (%d) not suitable for zeroconf\n",
	    intf.name, intf.index);
    exit(1);
  }

  if ((as = arp_open(&intf)) < 0) {
    log_it(LOG_ERR, "unable to obtain ARP socket: %s\n", strerror(errno));
    exit(1);
  }

  /* convert our generic netlink socket into one which only
   * reports link, ipv4 address and ipv4 route events
   */
  if (netlink_close(nl) < 0) {
    log_it(LOG_ERR, "netlink close error: %s\n", strerror(errno));
    exit(1);
  }

  if ((nl = netlink_open(NETLINK_ROUTE,
			 RTMGRP_LINK|RTMGRP_IPV4_IFADDR|RTMGRP_IPV4_ROUTE))< 0){
    log_it(LOG_ERR, "unable to connect to netlink groups\n");
    exit(1);
  }

  if (!nofork) {
      if (daemon(0, 0)) {
          log_it(LOG_ERR, "unable to daemonize: %s\n", strerror(errno));
          exit(1);
      }
  }

  if (!verbose) {
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
  }

  /* write /var/run/zeroconf.intf.pid */
  {
    FILE *pidfile;

    if (snprintf(pidloc, PATH_MAX, "/var/run/zeroconf-%s.pid", intf.name) < 0) {
      log_it(LOG_ERR, "unable to construct pid file location: %s\n", strerror(errno));
      exit(1);
    }

    pidfile = fopen(pidloc, "w+");

    if (pidfile != NULL) {

      mypid = getpid();

      fprintf(pidfile, "%d\n", mypid);

      fclose(pidfile);

    } else {

      log_it(LOG_ERR, "unable to create pid file (%s): %s\n", pidloc, strerror(errno));

    }

  }

  log_it(LOG_NOTICE, "zeroconf starting on interface %s", intf.name);

  /* Initialize the interface */
  if (script && *script) {
      run_script(script, "init", intf.name, NULL);
  }

  /* setup random number generator
   * we key this off the MAC-48 HW identifier
   * the first 3 octets are the manufacturer
   * the next 3 the serial number
   * we'll use the last four for the largest variety
   */
  seed = (intf.mac.ether_addr_octet[2] << 24) |
      (intf.mac.ether_addr_octet[3] << 16) |
      (intf.mac.ether_addr_octet[4] <<  8) |
      (intf.mac.ether_addr_octet[5] <<  0);

  /* Also, include the interface name */
  seed ^= hash_one_at_a_time((unsigned char *) intf.name, strlen(intf.name));

  srandom(seed);

  /* handle signals
   * SIGALARM: set delay_timeout = 1
   * SIGTERM : perform address release and exit
   * SIGINT  : perform address release and exit
   * SIGHUP  : perform address release and re-init
   */

  if (pipe(self_pipe) < 0) {
    log_it(LOG_ERR, "unable to create self-pipe fds: %s\n", strerror(errno));
    exit(1);
  }

  if (fcntl(self_pipe[0], F_SETFL, O_NONBLOCK) < 0) {
    log_it(LOG_ERR, "unable to set self-pipe fd non-blocking: %s\n", strerror(errno));
    exit(1);
  }

  if (fcntl(self_pipe[1], F_SETFL, O_NONBLOCK) < 0) {
    log_it(LOG_ERR, "unable to set self-pipe fd non-blocking: %s\n", strerror(errno));
    exit(1);
  }

  if (signal_install(SIGALRM) < 0) {
    log_it(LOG_ERR, "unable to install SIGALRM: %s\n", strerror(errno));
    exit(1);
  }

  if (signal_install(SIGTERM) < 0) {
    log_it(LOG_ERR, "unable to install SIGTERM: %s\n", strerror(errno));
    exit(1);
  }

  if (signal_install(SIGINT) < 0) {
    log_it(LOG_ERR, "unable to install SIGINT: %s\n", strerror(errno));
    exit(1);
  }

  if (signal_install(SIGHUP) < 0) {
    log_it(LOG_ERR, "unable to install SIGHUP: %s\n", strerror(errno));
    exit(1);
  }

  /* setup an immediate delay to kick things off */

  delay_setup_immed(&delay);

  delay_run(&delay);

  while (keep_running) {

    static int state = ADDR_INIT;

    struct pollfd     fds[3];
    struct arp_packet *pkt;
    struct nlmsghdr   *n;

    int           timeout;
    unsigned char replybuf[16384];

    timeout = 0;
    pkt = NULL;
    n = NULL;

    fds[0].fd = nl;
    fds[0].events = POLLIN|POLLERR;
    fds[1].fd = as;
    fds[1].events = POLLIN|POLLERR;
    fds[2].fd = self_pipe[0];
    fds[2].events = POLLIN|POLLERR;

    switch (poll(fds, 3, -1)) {
    case -1:
      /* signals are handled as file descriptors via the self-pipe */
      if (errno == EINTR)
	continue;

      log_it(LOG_ERR, "poll err: %s\n", strerror(errno));
      exit(1);
      break;
    case 0:
      log_it(LOG_ERR, "poll timed out during infinity\n");
      exit(1);
      break;
    default:
      {

	ssize_t ret;

	/* okay, one of our descriptors has been changed
	 * pull the data out, processing takes place later
	 */

	/* netlink */
	if (fds[0].revents) {

	  n = (struct nlmsghdr *)&replybuf;
	  if (verbose)
	    log_it(LOG_DEBUG, "netlink event\n");
	  ret = recv(fds[0].fd, replybuf, sizeof(replybuf), 0);
#if 0
	  for (int i = 0; i < ret; i++)
	    log_it(LOG_DEBUG, "!%02X", replybuf[i]);
	  log_it(LOG_DEBUG, "\n");
#endif
	  if (verbose)
	    netlink_dump(n);

	  if (netlink_qualify(n, ret) < 0)
	    n = NULL;

	}

	/* arp packet */
	if (fds[1].revents) {
	  if (verbose)
	    log_it(LOG_DEBUG, "arp event\n");
	  ret = recv(fds[1].fd, replybuf, sizeof(replybuf), 0);
	  pkt = (struct arp_packet *)&replybuf;
	  if (verbose)
	    arp_packet_dump(pkt);
	  intf.up = 1;
	}

	/* signal */
	if (fds[2].revents) {
	  int signo;
	  ret = read(fds[2].fd, &signo, sizeof(signo));
	  if (ret < 0)
	    log_it(LOG_ERR, "signal read failed: %s\n", strerror(errno));

	  if (verbose)
	    log_it(LOG_DEBUG, "signal %d\n", signo);

	  if (ret == sizeof(signo)) {
	    switch(signo) {
	    case SIGALRM:
	      delay_timeout = 1;
	      delay_is_running = 0;
	      break;
	    case SIGINT:
	    case SIGTERM:
	      state = ADDR_RELEASE;
	      keep_running = 0;
	      break;
	    case SIGHUP:
	      state = ADDR_RELEASE;
	      break;
	    default:
	      log_it(LOG_ERR, "unhandled signal %d\n", signo);
	      break;
	    }
	  }
	}

      }
      break;
    }

    /* ARP could possibly have various types of address formats.
     * Only Ethernet, IP protocol addresses and
     * ARP_REQUESTs or ARP_REPLYs are interesting
     */
    if ((pkt) &&
	(ntohs(pkt->arp.ar_hrd) != ARPHRD_ETHER) &&
	(ntohs(pkt->arp.ar_pro) != ARP_IP_PROTO) &&
	(
	 (ntohs(pkt->arp.ar_op) != ARPOP_REQUEST) ||
	 (ntohs(pkt->arp.ar_op) != ARPOP_REPLY)
	)
       ) {
      pkt = NULL;
    }

    /* This sets intf.up only if this is an RTM_NEWLINK message */
    intf.up = netlink_is_link_up(&intf, n);

    if ((n) && (!netlink_is_from_us(n))) {

      /* we have a message that is from elsewhere
       * the only ones we get are:
       * RTM_NEWLINK,  RTM_DELLINK
       * RTM_NEWADDR,  RTM_DELADDR
       * RTM_NEWROUTE, RTM_DELROUTE
       *
       * At this point a LINK message could happen. We deal with
       * some NEWLINKs in netlink_is_link_up(); but the interface
       * index might have changed. So we have to check here and
       * snarf it if the interface name matches our command line.
       *
       * Even though it is common to remove a wireless module
       * on suspend, there isn't anything special we can do.
       * Removing the module generates a DELLINK which we flag as
       * a termination event. Hopefully when the interface is
       * recreated zeroconf is re-invoked.
       *
       * DELADDR is possible from either the administrator or
       * another program (e.g. buggy DHCP program).
       * We could add the address back straight away but we
       * play safe by re-probing once again
       *
       * NEWADDR we should investigate to see if another IPv4
       * link-local is being assigned and then terminate if so.
       *
       * NEWROUTE / DELROUTE TODO
       *
       */

      switch (n->nlmsg_type) {
      case RTM_NEWLINK:
	netlink_is_our_iface(&intf, n);
	state = ADDR_PROBE;
	break;
      case RTM_DELLINK:
	{
	  struct ifinfomsg *i;

	  i = NLMSG_DATA(n);

	  if (intf.index == i->ifi_index) {
	    state = ADDR_RELEASE;
	    keep_running = 0;
	  }
	}
	break;
      case RTM_NEWADDR:
	{
	  struct ifaddrmsg *a;
	  int len;

	  a = NLMSG_DATA(n);
	  len = NLMSG_PAYLOAD(n, sizeof(struct ifaddrmsg));

	  if ((a->ifa_index == intf.index) &&
	      (a->ifa_family == AF_INET)) {
	    struct rtattr  *rta;
	    struct in_addr  ip;

	    rta = IFA_RTA(a);

	    while (RTA_OK(rta, len)) {
	      if (rta->rta_type == IFLA_ADDRESS) {

		memcpy(&ip, RTA_DATA(rta), rta->rta_len);

		if (verbose)
		  log_it(LOG_DEBUG,"saw %s\n",inet_ntoa(ip));

		if ((ip.s_addr & htonl(IPV4_LLBASE)) == htonl(IPV4_LLBASE)) {
		  /* perhaps a second instance is running?
		   * or the administrator has set an address
		   * manually. Either way, let's exit
		   */
		  state = ADDR_RELEASE;
		  keep_running = 0;
		}

	      }

	      rta = RTA_NEXT(rta, len);

	    }
	  }
	}
	break;
      case RTM_DELADDR:
	{

	  struct ifaddrmsg *a;
	  int len;

	  a = NLMSG_DATA(n);
	  len = NLMSG_PAYLOAD(n, sizeof(struct ifaddrmsg));

	  if ((a->ifa_index == intf.index) &&
	      (a->ifa_family == AF_INET)) {

	    struct rtattr  *rta;
	    struct in_addr  ip;

	    rta = IFA_RTA(a);

	    while (RTA_OK(rta, len)) {
	      if (rta->rta_type == IFLA_ADDRESS) {

		memcpy(&ip, RTA_DATA(rta), rta->rta_len);

		if (verbose)
		  log_it(LOG_DEBUG,"saw %s\n",inet_ntoa(ip));

		if ((ip.s_addr & htonl(IPV4_LLBASE)) == htonl(IPV4_LLBASE)) {

		  if (verbose)
		    log_it(LOG_DEBUG, "reasserting our address\n");

		  zeroconf_counter_reset();
		  state = ADDR_PROBE;
		}
	      }

	      rta = RTA_NEXT(rta, len);

	    }
	  }
	}
	break;
      case RTM_NEWROUTE:
	/* TODO */
	break;
      case RTM_DELROUTE:
	/* TODO */
	break;
      default:
	log_it(LOG_ERR, "unhandled case\n");
	break;
      }

    }

    if (intf.up && !prev_up) {
      log_it(LOG_INFO, "zeroconf has link up on interface %s", intf.name);
    }

    /* if we aren't up, continue waiting */
    if (!intf.up) {

      log_it(LOG_INFO, "zeroconf has no link on interface %s", intf.name);
      delay_cancel();

      zeroconf_counter_reset();
      state = ADDR_PROBE;

      prev_up = intf.up;
      continue;
    }

    prev_up = intf.up;

    switch (state) {
    case ADDR_INIT:
      {
	if (verbose)
	  log_it(LOG_DEBUG, "entering ADDR_INIT\n");

	delay_setup_random(&delay, 0, PROBE_WAIT);

	delay_run(&delay);

	zeroconf_counter_reset();
	address_picked = 0;

	state = ADDR_PROBE;

	if (verbose)
	  log_it(LOG_DEBUG, "leaving  ADDR_INIT\n");
      }
      break;
    case ADDR_PROBE:
      {
	if (verbose)
	  log_it(LOG_DEBUG, "entering ADDR_PROBE\n");

	delay_wait();

	if (arp_conflict(&intf, pkt)) {
	  conflict_count++;
	  address_picked = 0;
	  probe_count = 0;
	}

	if (!delay_is_waiting()) {

	  if (!address_picked) {

	    /*
	     * pick random IP address in IPv4 link-local range
	     * 169.254.0.0/16 is the allowed address range however 
	     * 169.254.0.0/24 and 169.254.255.0/24 must be excluded, 
	     * which removes 512 address from our 65535 candidates. 
	     * That leaves us with 65023 (0xfdff) to which we add 
	     * 256 (0x0100) to make sure it is within range.
             *
             * We also remove "addr_reserve_base_block" additional
             * /24's, from the bottom of the range (starting with
             * 169.254.1/24) .
	     */

            base_addrs = 0;
            if (addr_reserve_base_block != 0) {
              /* reserve less than 10% of the space */
              if (addr_reserve_base_block > 24) {
                  addr_reserve_base_block = 24;
              }
              base_addrs = 256 * addr_reserve_base_block;
            }

	    intf.ip.s_addr = htonl(IPV4_LLBASE |
                                   ((abs(random()) % (0xfdff - base_addrs)) + (0x0100 + base_addrs)));

	    if (verbose)
	      log_it(LOG_DEBUG, "picked address: %s\n", inet_ntoa(intf.ip));

	    address_picked = 1;

	  }

	  arp_probe(as, &intf);

	  probe_count++;

	}

	if (probe_count >= PROBE_NUM) {
	  state = ADDR_CLAIM;

	  delay_setup_fixed(&delay, ANNOUNCE_WAIT);

	  delay_run(&delay);

	  if (verbose) /* really only here to make things look tidy */
	    log_it(LOG_DEBUG,"leaving  ADDR_PROBE\n");
	  break;
	}

	if (conflict_count > MAX_CONFLICTS) {

	  delay_setup_fixed(&delay, RATE_LIMIT_INTERVAL);

	} else {

	  delay_setup_random(&delay, PROBE_MIN, PROBE_MAX);

	}

	delay_run(&delay);

	if (verbose)
	  log_it(LOG_DEBUG, "leaving  ADDR_PROBE\n");
      }
      break;
    case ADDR_CLAIM:
      {
	if (verbose)
	  log_it(LOG_DEBUG, "entering ADDR_CLAIM\n");

	if (arp_conflict(&intf, pkt)) {
	  state = ADDR_PROBE;

	  delay_setup_immed(&delay);

	  delay_run(&delay);

	  if (verbose)
	    log_it(LOG_DEBUG, "leaving  ADDR_CLAIM\n");
	  break;
	}

	delay_wait();

	arp_claim(as, &intf);

	claim_count++;

	if (claim_count >= ANNOUNCE_NUM) {
	  state = ADDR_TAKE;

	  delay_setup_immed(&delay);

	  delay_run(&delay);

	  if (verbose)
	    log_it(LOG_DEBUG, "leaving  ADDR_CLAIM\n");
	  break;
	}

	delay_setup_fixed(&delay, ANNOUNCE_INTERVAL);

	delay_run(&delay);

	if (verbose)
	  log_it(LOG_DEBUG, "leaving  ADDR_CLAIM\n");
      }
      break;
    case ADDR_TAKE:
      {
	if (verbose)
	  log_it(LOG_DEBUG, "entering ADDR_TAKE\n");

	delay_wait();

	do_addr_add(script, nl, &intf);

	state = ADDR_DEFEND;

	delay_setup_immed(&delay);

	delay_run(&delay);

	if (verbose)
	  log_it(LOG_DEBUG, "leaving  ADDR_TAKE\n");

        if (quit)
            keep_running = 0;

        fail_by_time = 0;
      }
      break;
    case ADDR_DEFEND:
      {

	if (verbose)
	  log_it(LOG_DEBUG, "entering ADDR_DEFEND\n");

	if (arp_conflict(&intf, pkt)) {

	  arp_defend(as, &intf);

	  delay_setup_immed(&delay);

	  delay_run(&delay);

	  state = ADDR_FINAL;

	  /* no need to break here, since we fall out anyway */
	}

	if (verbose)
	  log_it(LOG_DEBUG, "leaving  ADDR_DEFEND\n");
      }
      break;
    case ADDR_FINAL:
      {

	static int defence_done = 0;

	if (verbose)
	  log_it(LOG_DEBUG, "entering ADDR_FINAL\n");

	if (!defence_done) {

	  delay_setup_fixed(&delay, DEFEND_INTERVAL);

	  delay_run(&delay);

	  defence_done = 1;

	}

	if (arp_conflict(&intf, pkt)) {

	  delay_cancel();
	  
	  state = ADDR_RELEASE;

	  delay_setup_immed(&delay);

	  delay_run(&delay);

	  if (verbose)
	    log_it(LOG_DEBUG, "leaving  ADDR_FINAL\n");
	  break;
	}

	delay_wait();

	/* we get here only if the timeout has
	 * finished and there were no conflicts
	 */
	state = ADDR_DEFEND;

	defence_done = 0;

	if (verbose)
	  log_it(LOG_DEBUG, "leaving  ADDR_FINAL\n");
      }
      break;
    case ADDR_RELEASE:
      {

	if (verbose)
	  log_it(LOG_DEBUG, "entering ADDR_RELEASE\n");

	do_addr_del(script, nl, &intf);

	delay_setup_immed(&delay);

	delay_run(&delay);

	state = ADDR_INIT;

	if (verbose)
	  log_it(LOG_DEBUG, "leaving  ADDR_RELEASE\n");
      }
      break;
    default:
      log_it(LOG_ERR, "unhandled state\n");
      exit(1);
    }

    if (fail_by_time != 0 && fail_by_time < time(NULL)) {
        log_it(LOG_ERR, "zeroconf failed on interface %s\n", intf.name);
        exit(1);
    }
  }

  netlink_close(nl);

  unlink(pidloc);

  log_it(LOG_NOTICE, "zeroconf completed on interface %s", intf.name);

  return 0;
}

void check_args(int argc, char * const argv[], struct intf *intf, char *script)
{

  int ret;
  int optindex = 0;

  /* command line options */
  static struct option opts[] = {
    {"force", 0, 0, 'f'},
    {"interface", 1, 0, 'i'},
    {"no-fork", 0, 0, 'n'},
    {"ipaddr", 1, 0, 'p'},
    {"verbose", 0, 0, 'v'},
    {"quit", 0, 0, 'q'},
    {"script", 1, 0, 's'},
    {0, 0, 0, 0}
  };

  if (argc < 3) {
    usage();
  }

  memset(script, 0, PATH_MAX);

  memset(intf->name, 0, IFNAMSIZ+1);

  while (1) {
#ifdef __KLIBC__
#define getopt_long(a, b, c, d, e) getopt(a, b, c);
#endif
    ret = getopt_long(argc, argv, "fi:np:s:qv", opts, &optindex);

    if (ret == -1)
      break;

    switch (ret) {
    case 'f':
      force = 1;
      break;
    case 'i':
      strncpy(intf->name, optarg, IFNAMSIZ);
      break;
    case 'n':
      nofork = 1;
      break;
    case 'p':
      log_it(LOG_ERR, "XXX: not implemented\n"); /* TODO */
      break;
    case 's':
      strncpy(script, optarg, PATH_MAX);
      break;
    case 'q':
      quit = 1;
      if (FAILURE_INTERVAL != 0) {
          fail_by_time = time(0) + FAILURE_INTERVAL;
      }
      break;
    case 'v':
      verbose = 1;
      break;
    default:
      log_it(LOG_ERR, "unknown option '%c'\n", ret);
      usage();
      break;
    }

  }

  if (strlen(intf->name) == 0) {
    log_it(LOG_ERR, "no interface specified\n");
    exit(1);
  }

}

void zeroconf_counter_reset(void)
{
  probe_count = 0;
  conflict_count = 0;
  claim_count = 0;
  fail_by_time = 0;
  if (quit && FAILURE_INTERVAL != 0) {
      fail_by_time = time(0) + FAILURE_INTERVAL;
  }
}

int netlink_open(int proto, __u32 groups)
{

  int sock;
  struct sockaddr_nl addr;

  if ((sock = socket(PF_NETLINK, SOCK_RAW, proto)) < 0) {
    log_it(LOG_ERR, "socket(PF_NETLINK): %s\n", strerror(errno));
    return sock;
  }

  memset(&addr, 0, sizeof(addr));

  addr.nl_family = AF_NETLINK;
  addr.nl_pid = getpid();
  addr.nl_groups = groups;

  if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    log_it(LOG_ERR, "bind(): %s\n", strerror(errno));
    return sock;
  }

  return sock;
}

int netlink_close(int nl)
{
  return close(nl);
}

int netlink_send(int nl, struct nlmsghdr *n)
{

  if ((nl < 0) || (!n))
    return -1;

  return send(nl, n, n->nlmsg_len, 0);

}

void netlink_dump(struct nlmsghdr *n)
{
  struct ifinfomsg *i;
  struct ifaddrmsg *a;
  struct rtmsg     *r;
  int               l;

  log_it(LOG_DEBUG, "%u %u %u %u %u:",
	  n->nlmsg_len,
	  n->nlmsg_type,
	  n->nlmsg_flags,
	  n->nlmsg_seq,
	  n->nlmsg_pid);

  switch (n->nlmsg_type) {

  case NLMSG_ERROR:
    log_it(LOG_DEBUG, "netlink error\n");
    break;

  case RTM_NEWLINK:
    log_it(LOG_DEBUG, "RTM_NEWLINK\n");

    i = NLMSG_DATA(n);
    l = NLMSG_PAYLOAD(n, sizeof(struct ifinfomsg));

    log_it(LOG_DEBUG,"ifinfomsg: %u %u %d %u %u ",i->ifi_family, i->ifi_type, i->ifi_index, i->ifi_flags, i->ifi_change);

    FLAG_TEST_DUMP(i->ifi_flags,IFF_UP);
    FLAG_TEST_DUMP(i->ifi_flags,IFF_BROADCAST);
    FLAG_TEST_DUMP(i->ifi_flags,IFF_DEBUG);
    FLAG_TEST_DUMP(i->ifi_flags,IFF_LOOPBACK);
    FLAG_TEST_DUMP(i->ifi_flags,IFF_POINTOPOINT);
    FLAG_TEST_DUMP(i->ifi_flags,IFF_NOTRAILERS);
    FLAG_TEST_DUMP(i->ifi_flags,IFF_RUNNING);
    FLAG_TEST_DUMP(i->ifi_flags,IFF_NOARP);
    FLAG_TEST_DUMP(i->ifi_flags,IFF_PROMISC);
    FLAG_TEST_DUMP(i->ifi_flags,IFF_ALLMULTI);
    FLAG_TEST_DUMP(i->ifi_flags,IFF_MASTER);
    FLAG_TEST_DUMP(i->ifi_flags,IFF_SLAVE);
    FLAG_TEST_DUMP(i->ifi_flags,IFF_MULTICAST);
    FLAG_TEST_DUMP(i->ifi_flags,IFF_PORTSEL);
    FLAG_TEST_DUMP(i->ifi_flags,IFF_AUTOMEDIA);
    FLAG_TEST_DUMP(i->ifi_flags,IFF_DYNAMIC);

    netlink_dump_rta(IFLA_RTA(i), l, i->ifi_family);

    log_it(LOG_DEBUG,"\n");
    
    break;
  case RTM_DELLINK:
    log_it(LOG_DEBUG,"RTM_DELLINK\n");

    i = NLMSG_DATA(n);
    l = NLMSG_PAYLOAD(n, sizeof(struct ifinfomsg));

    log_it(LOG_DEBUG, "ifinfomsg: %u %u %d %u %u ", i->ifi_family, i->ifi_type, i->ifi_index, i->ifi_flags, i->ifi_change);

    FLAG_TEST_DUMP(i->ifi_flags, IFF_UP);
    FLAG_TEST_DUMP(i->ifi_flags, IFF_BROADCAST);
    FLAG_TEST_DUMP(i->ifi_flags, IFF_DEBUG);
    FLAG_TEST_DUMP(i->ifi_flags, IFF_LOOPBACK);
    FLAG_TEST_DUMP(i->ifi_flags, IFF_POINTOPOINT);
    FLAG_TEST_DUMP(i->ifi_flags, IFF_NOTRAILERS);
    FLAG_TEST_DUMP(i->ifi_flags, IFF_RUNNING);
    FLAG_TEST_DUMP(i->ifi_flags, IFF_NOARP);
    FLAG_TEST_DUMP(i->ifi_flags, IFF_PROMISC);
    FLAG_TEST_DUMP(i->ifi_flags, IFF_ALLMULTI);
    FLAG_TEST_DUMP(i->ifi_flags, IFF_MASTER);
    FLAG_TEST_DUMP(i->ifi_flags, IFF_SLAVE);
    FLAG_TEST_DUMP(i->ifi_flags, IFF_MULTICAST);
    FLAG_TEST_DUMP(i->ifi_flags, IFF_PORTSEL);
    FLAG_TEST_DUMP(i->ifi_flags, IFF_AUTOMEDIA);
    FLAG_TEST_DUMP(i->ifi_flags, IFF_DYNAMIC);

    netlink_dump_rta(IFLA_RTA(i), l, i->ifi_family);

    log_it(LOG_DEBUG, "\n");

    break;
  case RTM_NEWADDR:
    log_it(LOG_DEBUG, "RTM_NEWADDR\n");

    a = NLMSG_DATA(n);
    l = NLMSG_PAYLOAD(n, sizeof(struct ifaddrmsg));

    log_it(LOG_DEBUG, "ifaddrmsg: %u %u %u %u %d",
	    a->ifa_family, a->ifa_prefixlen,
	    a->ifa_flags, a->ifa_scope, a->ifa_index);

    switch (a->ifa_family){
    case AF_INET:
      log_it(LOG_DEBUG, " AF_INET ");
      break;
    case AF_INET6:
      log_it(LOG_DEBUG, " AF_INET6 ");
      break;
    default:
      log_it(LOG_DEBUG, " AF unknown ");
      break;
    }

    FLAG_TEST_DUMP(a->ifa_flags, IFA_F_SECONDARY);
    FLAG_TEST_DUMP(a->ifa_flags, IFA_F_DEPRECATED);
    FLAG_TEST_DUMP(a->ifa_flags, IFA_F_TENTATIVE);
    FLAG_TEST_DUMP(a->ifa_flags, IFA_F_PERMANENT);

    netlink_dump_rta(IFA_RTA(a), l, a->ifa_family);

    log_it(LOG_DEBUG, "\n");

    break;
  case RTM_DELADDR:
    log_it(LOG_DEBUG, "RTM_DELADDR\n");

    a = NLMSG_DATA(n);
    l = NLMSG_PAYLOAD(n, sizeof(struct ifaddrmsg));

    log_it(LOG_DEBUG, "ifaddrmsg: %u %u %u %u %d",
	    a->ifa_family, a->ifa_prefixlen,
	    a->ifa_flags, a->ifa_scope, a->ifa_index);

    switch (a->ifa_family){
    case AF_INET:
      log_it(LOG_DEBUG, " AF_INET ");
      break;
    case AF_INET6:
      log_it(LOG_DEBUG, " AF_INET6 ");
      break;
    default:
      log_it(LOG_DEBUG, " AF unknown ");
      break;
    }

    FLAG_TEST_DUMP(a->ifa_flags, IFA_F_SECONDARY);
    FLAG_TEST_DUMP(a->ifa_flags, IFA_F_DEPRECATED);
    FLAG_TEST_DUMP(a->ifa_flags, IFA_F_TENTATIVE);
    FLAG_TEST_DUMP(a->ifa_flags, IFA_F_PERMANENT);

    netlink_dump_rta(IFA_RTA(a), l, a->ifa_family);

    log_it(LOG_DEBUG, "\n");
    break;
  case RTM_NEWROUTE:
    log_it(LOG_DEBUG, "RTM_NEWROUTE\n");

    r = NLMSG_DATA(n);
    l = NLMSG_PAYLOAD(n, sizeof(struct rtmsg));

    log_it(LOG_DEBUG, "rtmsg: %u %u %u %u %u %u %u %u %u",
	    r->rtm_family, r->rtm_dst_len, r->rtm_src_len,
	    r->rtm_tos, r->rtm_table, r->rtm_protocol,
	    r->rtm_scope, r->rtm_type, r->rtm_flags);

    switch (r->rtm_family){
    case AF_INET:
      log_it(LOG_DEBUG, " AF_INET ");
      break;
    case AF_INET6:
      log_it(LOG_DEBUG, " AF_INET6 ");
      break;
    default:
      log_it(LOG_DEBUG, " AF unknown ");
      break;
    }

    FLAG_TEST_DUMP(r->rtm_flags, RTM_F_NOTIFY);
    FLAG_TEST_DUMP(r->rtm_flags, RTM_F_CLONED);
    FLAG_TEST_DUMP(r->rtm_flags, RTM_F_EQUALIZE);

    netlink_dump_rta(RTM_RTA(r), l, r->rtm_family);

    log_it(LOG_DEBUG, "\n");
    break;
  case RTM_DELROUTE:
    log_it(LOG_DEBUG, "RTM_DELROUTE\n");

    r = NLMSG_DATA(n);
    l = NLMSG_PAYLOAD(n, sizeof(struct rtmsg));

    log_it(LOG_DEBUG, "rtmsg: %u %u %u %u %u %u %u %u %u",
	    r->rtm_family, r->rtm_dst_len, r->rtm_src_len,
	    r->rtm_tos, r->rtm_table, r->rtm_protocol,
	    r->rtm_scope, r->rtm_type, r->rtm_flags);

    switch (r->rtm_family){
    case AF_INET:
      log_it(LOG_DEBUG, " AF_INET ");
      break;
    case AF_INET6:
      log_it(LOG_DEBUG, " AF_INET6 ");
      break;
    default:
      log_it(LOG_DEBUG, " AF unknown ");
      break;
    }

    FLAG_TEST_DUMP(r->rtm_flags, RTM_F_NOTIFY);
    FLAG_TEST_DUMP(r->rtm_flags, RTM_F_CLONED);
    FLAG_TEST_DUMP(r->rtm_flags, RTM_F_EQUALIZE);

    netlink_dump_rta(RTM_RTA(r), l, r->rtm_family);

    log_it(LOG_DEBUG, "\n");
    break;
  }

  log_it(LOG_DEBUG, "\n");
}


void netlink_dump_rta(struct rtattr *rta, int length, int family)
{

  struct rtnl_link_stats *stats;
  struct rtnl_link_ifmap *map;
  unsigned char          *addr;

  /* family is a kludge so we know what format to
   * print IFLA_ADDRESS and IFLA_BROADCAST in
   */

  while (RTA_OK(rta, length)) {

    log_it(LOG_DEBUG, ", %u %u:", rta->rta_len, rta->rta_type);

    switch(rta->rta_type) {
    case IFLA_UNSPEC:
      log_it(LOG_DEBUG, "IFLA_UNSPEC");
      break;
    case IFLA_ADDRESS:
      log_it(LOG_DEBUG, "IFLA_ADDRESS ");
      addr = (unsigned char *)RTA_DATA(rta);
      switch (family) {
      case AF_INET:
	log_it(LOG_DEBUG, "%u.%u.%u.%u", addr[0], addr[1], addr[2], addr[3]);
	break;
      default:
	/* assume only print 6 - it's wrong but I can't see a better way */
	log_it(LOG_DEBUG, "%02X:%02X:%02X:%02X:%02X:%02X",
		addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
	break;
      }
      break;
    case IFLA_BROADCAST:
      log_it(LOG_DEBUG, "IFLA_BROADCAST ");
      addr = (unsigned char *)RTA_DATA(rta);
      switch (family) {
      case AF_INET:
	log_it(LOG_DEBUG, "%u.%u.%u.%u", addr[0], addr[1], addr[2], addr[3]);
	break;
      default:
	/* assume only print 6 - it's wrong but I can't see a better way */
	log_it(LOG_DEBUG, "%02X:%02X:%02X:%02X:%02X:%02X",
		addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
	break;
      }
      break;
    case IFLA_IFNAME:
      log_it(LOG_DEBUG, "IFLA_IFNAME: %s", (char *)RTA_DATA(rta));
      break;
    case IFLA_MTU:
      log_it(LOG_DEBUG, "IFLA_MTU: %u", (unsigned long)RTA_DATA(rta));
      break;
    case IFLA_LINK:
      log_it(LOG_DEBUG, "IFLA_LINK: %d", (long)RTA_DATA(rta));
      break;
    case IFLA_QDISC:
      log_it(LOG_DEBUG, "IFLA_QDISC: %s", (char *)RTA_DATA(rta));
      break;
    case IFLA_STATS:
      log_it(LOG_DEBUG, "IFLA_STATS");
      stats = (struct rtnl_link_stats *)RTA_DATA(rta);
      break;
    case IFLA_COST:
      log_it(LOG_DEBUG, "IFLA_COST");
      break;
    case IFLA_PRIORITY:
      log_it(LOG_DEBUG, "IFLA_PRIORITY");
      break;
    case IFLA_MASTER:
      log_it(LOG_DEBUG, "IFLA_MASTER: %u", (unsigned long)RTA_DATA(rta));
      break;
    case IFLA_WIRELESS:
      log_it(LOG_DEBUG, "IFLA_WIRELESS");
      break;
#ifdef IFIFLA_PROTINFO
    case IFLA_PROTINFO:
      log_it(LOG_DEBUG, "IFLA_PROTINFO");
      break;
    case IFLA_TXQLEN:
      log_it(LOG_DEBUG, "IFLA_TXQLEN: %u", (unsigned int)RTA_DATA(rta));
      break;
    case IFLA_MAP:
      map = (struct rtnl_link_ifmap *)RTA_DATA(rta);
      log_it(LOG_DEBUG, "IFLA_MAP");
      break;
    case IFLA_WEIGHT:
      log_it(LOG_DEBUG, "IFLA_WEIGHT: %u", (unsigned int)RTA_DATA(rta));
      break;
#endif
    default:
      log_it(LOG_DEBUG, "unhandled rta type");
      break;
    }

    rta = RTA_NEXT(rta, length);
  }

}

int netlink_is_link_up(struct intf *intf, struct nlmsghdr *n)
{

  struct ifinfomsg *i;

  if (!n)
    return intf->up;

  if (n->nlmsg_type != RTM_NEWLINK)
    return intf->up;

  i = NLMSG_DATA(n);

  if (intf->index != i->ifi_index)
    return intf->up;

  return ((i->ifi_flags & IFF_RUNNING) == IFF_RUNNING);

}

int netlink_is_from_us(struct nlmsghdr *n)
{
  /* NOTE: does not handle case when n is NULL */

  /*
   * if the netlink message came from
   * either the kernel or our process id
   * return 1 otherwise return 0
   */

  if ((n->nlmsg_pid == 0 /* kernel */) ||
      (n->nlmsg_pid == mypid))
    return 1;

  return 0;

}

int netlink_qualify(struct nlmsghdr *n, size_t length)
{
  if (!NLMSG_OK(n, length) ||
      length < sizeof(struct nlmsghdr) ||
      length < n->nlmsg_len) {
    log_it(LOG_ERR, "netlink: packet too small or truncated. %u!=%u!=%u",
	    length, sizeof(struct nlmsghdr), n->nlmsg_len);
    return -1;
  }

  if (n->nlmsg_type == NLMSG_ERROR) {
    struct nlmsgerr *e = (struct nlmsgerr *) NLMSG_DATA(n);

    if (e->error) {
      log_it(LOG_ERR, "netlink error: %s\n", strerror(-e->error));
    }
    return -1;
  }

  return 0;
}

static
void netlink_addr_do(int nl, int op, struct intf *intf)
{

  struct in_addr brd;

  struct {
    struct nlmsghdr  n;
    struct ifaddrmsg a;
    char   opts[32];
  } req;

  if (inet_aton("169.254.255.255", &brd) == 0) {
    log_it(LOG_ERR, "couldn't generate broadcast address\n");
    exit(1);
  }

  memset(&req, 0, sizeof(req));

  req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
  req.n.nlmsg_type = op;
  req.n.nlmsg_flags = NLM_F_REQUEST;

  req.a.ifa_family = AF_INET;
  req.a.ifa_prefixlen = 16; /* AIPPA is 169.254.0.0/16 */
  req.a.ifa_flags = IFA_F_PERMANENT;
  req.a.ifa_scope = RT_SCOPE_LINK;
  req.a.ifa_index = intf->index;

  if (addattr_l(&req.n, sizeof(req),
		IFA_LOCAL, (void *)&intf->ip,
		sizeof(struct in_addr)) < 0) {
    log_it(LOG_ERR, "addattr_l: unable to add IFA_LOCAL attribute to netlink message\n");
  }

  if (addattr_l(&req.n, sizeof(req),
		IFA_BROADCAST, (void *)&brd,
		sizeof(struct in_addr)) < 0) {
    log_it(LOG_ERR, "addattr_l: unable to add IFA_BROADCAST attribute to netlink message\n");
  }

  if (netlink_send(nl, (struct nlmsghdr *)&req) < 0) {
    log_it(LOG_ERR, "netlink_send(): %s\n", strerror(errno));
  }
}

void do_addr_add(const char *script, int nl, struct intf *intf)
{
    log_it(LOG_NOTICE, "zeroconf on interface %s: adding address %s",
           intf->name, inet_ntoa(intf->ip));

    if (script && *script) {
        run_script(script, "config", intf->name, &(intf->ip));
    } else {
        netlink_addr_add(nl, intf);
    }
}

void do_addr_del(const char *script, int nl, struct intf *intf)
{
    log_it(LOG_NOTICE, "zeroconf on interface %s: deleting address %s",
           intf->name, inet_ntoa(intf->ip));
    if (script && *script) {
        run_script(script, "deconfig", intf->name, &(intf->ip));
    } else {
        netlink_addr_del(nl, intf);
    }
}

void netlink_addr_add(int nl, struct intf *intf)
{

  netlink_addr_do(nl, RTM_NEWADDR, intf);

}

void netlink_addr_del(int nl, struct intf *intf)
{

  netlink_addr_do(nl, RTM_DELADDR, intf);

}

int check_ifname_exists(int nl, struct intf *intf)
{

#if 0
  /* in theory, the below should work. in practise it doesn't
   * so we use the more complicated netlink method to get the index
   */
  struct ifreq ifr;

  memset( &ifr, 0, sizeof(struct ifreq));
  strncpy(ifr.ifr_name, int.name, sizeof(ifr.ifr_name));

  if (ioctl(nl, SIOCGIFINDEX, &ifr) < 0) {
    log_it(LOG_ERR, "ioctl SIOCGIFINDEX failed: %s\n", strerror(errno));
    return -1;
  }

  intf->index = ifr.ifr_ifindex;

  if (ioctl(nl, SIOCGIFFLAGS, &ifr) < 0) {
    log_it(LOG_ERR, "ioctl SIOCGIFINDEX failed: %s\n", strerror(errno));
    return -1;
  }

  intf->flags = ifr.ifr_flags;

  return 0;
#else

  if (!intf) {
      return -1;
  }

  struct pollfd fds[1];
  int done = 0;

  struct {
    struct nlmsghdr  n;
    struct rtgenmsg  i;
    char             ifnamebuf[NLBUF];
  } req;

  memset(&req, 0, sizeof(req));
  req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtgenmsg));
  req.n.nlmsg_flags = NLM_F_REQUEST|NLM_F_MATCH;
  req.n.nlmsg_type = RTM_GETLINK;
  req.i.rtgen_family = AF_UNSPEC;

  if (addattr_l(&req.n, sizeof(req), IFLA_IFNAME, intf->name, IFNAMSIZ+1) < 0) {
    log_it(LOG_ERR, "addattr_l: unable to add IFNAME attribute to netlink message\n");
    return -1;
  }


  if (netlink_send(nl, (struct nlmsghdr *)&req) < 0) {
    log_it(LOG_ERR, "netlink_send(): %s\n", strerror(errno));
    return -1;
  }

  /* So we can detect if we've seen our interface */
  intf->index = -1;

/* Loop so we see any multipart netlink responses */
while (!done) {
  /* wait up to 5 seconds for the kernel to decide
   * modprobe or udev might take that long
   */
  fds[0].fd = nl;
  fds[0].events = POLLIN|POLLERR;

  switch (poll(fds, 1, 5*100)) {
  case -1:
    log_it(LOG_ERR, "poll err: %s\n", strerror(errno));
    exit(1);
    break;
  case 0:
    if (intf && intf->index > 0) {
      return intf->index;
    }
    log_it(LOG_ERR, "couldn't find interface\n");
    return -1;
    break;
  case 1:
    {
      /* this is annoyingly complicated */
      if (verbose)
	log_it(LOG_ERR, "we got a netlink message in %s\n", __FUNCTION__);

      ssize_t  bytes;
      char replybuf[16384];
      struct nlmsghdr *n = (struct nlmsghdr *)&replybuf;

      if ((bytes = recv(nl, replybuf, sizeof(replybuf), 0)) < 0) {
	log_it(LOG_ERR, "recv(): %s\n", strerror(errno));
	return -1;
      }

      for (; bytes > 0; n = NLMSG_NEXT(n, bytes)) {

	if (netlink_qualify(n, (size_t)bytes)< 0)
	  return -1;

	if (n->nlmsg_type == NLMSG_DONE) {
          done = 1;
	  if (verbose)
	    log_it(LOG_ERR, "finished processing RTM_GETLINK in %s\n", __FUNCTION__);
	  return intf->index;
	}

	if (n->nlmsg_type != RTM_NEWLINK) {
	  log_it(LOG_ERR, "received unexpected %u response\n", n->nlmsg_type);
	  return -1;
	}

	netlink_is_our_iface(intf, n);

      }

    }
    break;
  default:
    log_it(LOG_ERR, "something abnormal happened in poll. aborting\n");
    exit(1);
    break;
  }

  /* If we've already found our interface, get out like we used to */
  if (intf && intf->index > 0) {
    done = 1;
  }

} /* while (!done) */

  if (done && intf) {
    return intf->index;
  }

  return -1;
#endif
}

int check_ifname_type(struct intf *intf)
{

  if ((intf->flags & IFF_NOARP) ||
      (intf->flags & IFF_LOOPBACK) ||
      (intf->flags & IFF_SLAVE) ||
      (intf->flags & IFF_POINTOPOINT))
    return -1;

  return 0;
}

int arp_open(struct intf *intf)
{
  int as;
  struct sockaddr_ll ll;

  if ((as = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_ARP))) < 0) {
    log_it(LOG_ERR, "arp socket failed: %s\n", strerror(errno));
    return -1;
  }

  memset(&ll, 0, sizeof(ll));
  ll.sll_family = AF_PACKET;
  ll.sll_ifindex = intf->index;

  if (bind(as, (struct sockaddr *)&ll, sizeof(ll)) < 0) {
    log_it(LOG_ERR, "arp bind failed: %s\n", strerror(errno));
    return -1;
  }

  return as;
}

void arp_packet_dump(struct arp_packet *pkt)
{

  if (!pkt)
    return;

  log_it(LOG_ERR, "%d %d %d %d %d: ",
	  ntohs(pkt->arp.ar_hrd),
	  ntohs(pkt->arp.ar_pro),
	  pkt->arp.ar_hln,
	  pkt->arp.ar_pln,
	  ntohs(pkt->arp.ar_op));

  /* ARP could possibly have various types of address formats
   * only Ethernet, IP protocol addresses and
   * ARP_REQUESTs or ARP_REPLYs are interesting
   */
  if ((ntohs(pkt->arp.ar_hrd) != ARPHRD_ETHER) &&
      (ntohs(pkt->arp.ar_pro) != ARP_IP_PROTO) &&
      (
       (ntohs(pkt->arp.ar_op) != ARPOP_REQUEST) ||
       (ntohs(pkt->arp.ar_op) != ARPOP_REPLY)
      )
     ) {
    log_it(LOG_ERR, "unhandled kind of ARP packet\n");
    return;
  }

  log_it(LOG_ERR, "%02X:%02X:%02X:%02X:%02X:%02X",
	  pkt->sender_mac.ether_addr_octet[0],
	  pkt->sender_mac.ether_addr_octet[1],
	  pkt->sender_mac.ether_addr_octet[2],
	  pkt->sender_mac.ether_addr_octet[3],
	  pkt->sender_mac.ether_addr_octet[4],
	  pkt->sender_mac.ether_addr_octet[5]);

  log_it(LOG_ERR, " %s", inet_ntoa(pkt->sender_ip));

  log_it(LOG_ERR, " -> ");

  log_it(LOG_ERR, "%02X:%02X:%02X:%02X:%02X:%02X",
	  pkt->target_mac.ether_addr_octet[0],
	  pkt->target_mac.ether_addr_octet[1],
	  pkt->target_mac.ether_addr_octet[2],
	  pkt->target_mac.ether_addr_octet[3],
	  pkt->target_mac.ether_addr_octet[4],
	  pkt->target_mac.ether_addr_octet[5]);

  log_it(LOG_ERR, " %s", inet_ntoa(pkt->target_ip));

  log_it(LOG_ERR, "\n");
}

/* while the RFC (and my design) notes that there are three
 * ways to detect a conflict, those three collapse readily
 * into just these two required tests
 */
int  arp_conflict(struct intf *intf, struct arp_packet *pkt)
{
  if (!pkt)
    return 0;

  /* handles 2.2.1: ARP packet (request or reply)
   * handles 2.5  : ARP packet (request of reply)
   * sender IP is our probe address
   */
  if (memcmp(&intf->ip, &pkt->sender_ip, sizeof(struct in_addr)) == 0) {
      log_it(LOG_NOTICE, "CONFLICT detected: sender: "
             "%02X:%02X:%02X:%02X:%02X:%02X has our IP: %s\n",
             pkt->sender_mac.ether_addr_octet[0],
             pkt->sender_mac.ether_addr_octet[1],
             pkt->sender_mac.ether_addr_octet[2],
             pkt->sender_mac.ether_addr_octet[3],
             pkt->sender_mac.ether_addr_octet[4],
             pkt->sender_mac.ether_addr_octet[5],
             inet_ntoa(pkt->sender_ip));
      return 1;
  }

  /* handles 2.2.1: ARP packet (ARP PROBE)
   * sender HW address is not our MAC address
   * target IP address is our probe address
   */
  if ((ntohs(pkt->arp.ar_op) == ARPOP_REQUEST) &&
      (memcmp(&intf->mac, &pkt->sender_mac, sizeof(struct ether_addr)) != 0) &&
      (memcmp(&intf->ip, &pkt->target_ip, sizeof(struct in_addr)) == 0)) {
      log_it(LOG_NOTICE, "CONFLICT detected: request from sender: "
             "%02X:%02X:%02X:%02X:%02X:%02X for our IP: %s\n",
             pkt->sender_mac.ether_addr_octet[0],
             pkt->sender_mac.ether_addr_octet[1],
             pkt->sender_mac.ether_addr_octet[2],
             pkt->sender_mac.ether_addr_octet[3],
             pkt->sender_mac.ether_addr_octet[4],
             pkt->sender_mac.ether_addr_octet[5],
             inet_ntoa(pkt->target_ip));
      return 1;
  }

  return 0;

}

void arp_packet_send(int as,
		     struct intf *intf,
		     short int arp_op,
		     int null_sender)
{

  struct arp_packet  ap;
  struct sockaddr_ll ll;
  ssize_t ret;

  memset(&ap, 0, sizeof(struct arp_packet));

  ap.arp.ar_hrd = htons(ARPHRD_ETHER);
  ap.arp.ar_pro = htons(ARP_IP_PROTO);
  ap.arp.ar_hln = ETH_ALEN;
  ap.arp.ar_pln = 4; /* octets in IPv4 address */
  ap.arp.ar_op = htons(arp_op);

  /* filling with 0xff sets the destination to
   * the broadcast link-layer address for free
   */
  memset(&ll, 0xff, sizeof(struct sockaddr_ll));
  ll.sll_family = AF_PACKET;
  ll.sll_protocol = htons(ETH_P_ARP);
  ll.sll_ifindex = intf->index;
  ll.sll_halen = ETH_ALEN;

  memcpy(&ap.sender_mac, &intf->mac, sizeof(struct ether_addr));
  memcpy(&ap.target_ip, &intf->ip, sizeof(struct in_addr));

  if (!null_sender)
    memcpy(&ap.sender_ip, &intf->ip, sizeof(struct in_addr));

  ret = sendto(as, (void *)&ap,
	       sizeof(struct arp_packet), 0,
	       (struct sockaddr *)&ll, sizeof(struct sockaddr_ll));

  if (ret < 0) {
     log_it(LOG_ERR,"failed to send ARP packet: %s\n", strerror(errno));
    if (errno == ENETDOWN) {
      intf->up = 0;
    }
  }

}

void arp_probe(int as, struct intf *intf)
{

  arp_packet_send(as, intf, ARPOP_REQUEST, 1);

}

void arp_claim(int as, struct intf *intf)
{

  arp_packet_send(as, intf, ARPOP_REQUEST, 0);

}

void arp_defend(int as, struct intf *intf)
{

  arp_packet_send(as, intf, ARPOP_REPLY, 0);

}

void usage(void)
{
  log_it(LOG_ERR, "usage error\n");
  log_it(LOG_ERR, "zeroconf [-f|--force] [-v|--verbose] [-n|--no-fork] [-i|--interface <interface>] [-q|--quit] [-s|--script <scriptname>]\n");
  log_it(LOG_ERR, "where:\n");
  log_it(LOG_ERR, "\t-f\tforce zeroconf to run on the specified interface\n");
  log_it(LOG_ERR, "\t-v\treport verbose information\n");
  log_it(LOG_ERR, "\t-n\tdo not fork into the background\n");
  log_it(LOG_ERR, "\t-i\twhich interface to run on [required]\n");
  log_it(LOG_ERR, "\t-q\tquit after obtaining address\n");
  log_it(LOG_ERR, "\t-s\tscript to run instead of changing interface\n");
  exit(1);
}


int signal_install(int signo)
{

  sigset_t         signals;
  struct sigaction sighdlr = {
    .sa_handler = sig_handler,
    .sa_flags = SA_RESTART,
  };

  if (sigemptyset(&signals) < 0) {
    log_it(LOG_ERR, "unable to clear signal set: %s\n", strerror(errno));
    return -1;
  }

  if (sigaddset(&signals, signo) < 0) {
    log_it(LOG_ERR, "unable to add signal to set: %s\n", strerror(errno));
    return -1;
  }

  if (sigaction(signo, &sighdlr, NULL ) < 0) {
    log_it(LOG_ERR, "unable to set signal handler: %s\n", strerror(errno));
    return -1;
  }

  if (sigprocmask(SIG_UNBLOCK, &signals, NULL) < 0) {
    log_it(LOG_ERR, "unable to unblock %d: %s\n", signo, strerror(errno));
    return -1;
  }

  return 0;
}

int addattr_l(struct nlmsghdr *n,
	      unsigned int maxlen,
	      int type,
	      const void *data,
	      int alen)
{
	int len = RTA_LENGTH(alen);
	struct rtattr *rta;

	if (NLMSG_ALIGN(n->nlmsg_len) + len > maxlen) {
		log_it(LOG_ERR, "addattr_l ERROR: message exceeded bound of %d\n", maxlen);
		return -1;
	}
	rta = (struct rtattr*)(((char*)n) + NLMSG_ALIGN(n->nlmsg_len));
	rta->rta_type = type;
	rta->rta_len = len;
	memcpy(RTA_DATA(rta), data, alen);
	n->nlmsg_len = NLMSG_ALIGN(n->nlmsg_len) + len;
	return 0;
}


void netlink_is_our_iface(struct intf *intf, struct nlmsghdr *n)
{

  struct ifinfomsg *i   = NULL;
  struct rtattr    *rta = NULL;
  int len;

  i   = NLMSG_DATA(n);
  len = NLMSG_PAYLOAD(n, sizeof(struct ifinfomsg));
  rta = IFLA_RTA(i);

  while (RTA_OK(rta, len)) {

    if (rta->rta_type == IFLA_IFNAME) {
      if (verbose)
	log_it(LOG_ERR, "got iface %d:%s\n", i->ifi_index, 
		                             (char *)RTA_DATA(rta));

      /* 
       * This strlen() call here is because this used to be a strncmp(),
       * for reasons which are not clear to us.  Likely it could just be
       * a !strcmp(), but we're attempting to honor whatever intent the
       * original author may have had.
       */
      if ((strlen((char *) RTA_DATA(rta)) < sizeof(intf->name)) &&
          !strcmp(intf->name, RTA_DATA(rta))) {
	intf->index = i->ifi_index;
	intf->flags = i->ifi_flags;
	intf->up = ((i->ifi_flags & IFF_RUNNING) == IFF_RUNNING);
      } else {
	break;
      }
    }

    if (rta->rta_type == IFLA_ADDRESS) {
      unsigned char *addr;

      addr = (unsigned char *)RTA_DATA(rta);

      intf->mac.ether_addr_octet[0] = addr[0];
      intf->mac.ether_addr_octet[1] = addr[1];
      intf->mac.ether_addr_octet[2] = addr[2];
      intf->mac.ether_addr_octet[3] = addr[3];
      intf->mac.ether_addr_octet[4] = addr[4];
      intf->mac.ether_addr_octet[5] = addr[5];
    }

    rta = RTA_NEXT(rta, len);
  }
}

/**
 * Run a script.
 */
static void
run_script(const char *script, const char *arg, char *intf, struct in_addr *ip)
{
    int pid, status;

    if (script != NULL) {
	if (verbose) {
	    log_it(LOG_ERR, "%s: run %s %s\n", intf, script, arg);
	}
	pid = fork();
	if (pid < 0) {
	    perror("fork failed");
	    exit(1);
	}
	if (pid == 0) {
	    // child process
	    setenv("interface", intf, 1);
	    if (ip != NULL) {
		setenv("ip", inet_ntoa(*ip), 1);
	    }

	    execl(script, script, arg, intf, NULL);
	    perror("execl failed");
	    exit(1);
	}
	if (waitpid(pid, &status, 0) <= 0) {
	    perror("waitpid failed");
	    exit(1);
	}

	if (WEXITSTATUS(status) != 0) {
	    log_it(LOG_WARNING, "Zeroconf script failed (exit code %d)",
                   script, WEXITSTATUS(status));
	    exit(1);
	}
    }
}
