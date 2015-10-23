/*
 * File.c
 *   
 * Author: Jeya ganesh babu
 * Description: PF_RING userspace library for reading packets and do url filter
 *
 * (C) Copyright 2014 Juniper Networks, Inc.
 * All rights reserved.
 */

#ifndef _STUB_
void nkn_pf_get_dst_mac(void)
{
}

int nkn_pfbridge_init()
{
    return 0;
}

#else /* _STUB_ */

#include <signal.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <atomic_ops.h>
#include <linux/ip.h>
#include <linux/tcp.h>

#include "nkn_defs.h"
#include "nkn_errno.h"
#include "nkn_assert.h"
#include "pfring.h"
#include "nkn_pfbridge.h"
#include "nkn_dpi.h"

#define MAX_PF_PKT_LEN 9000
#define QUICK_MODE 1

uint64_t glob_pf_num_pkts_cli_reset = 0;
uint64_t glob_pf_num_pkts_srv_reset = 0;
uint64_t glob_pf_num_pkts_fwded = 0;
uint64_t glob_pf_retry_count = 0;
uint64_t glob_pf_num_pkts_to_stack = 0;
uint64_t glob_pf_num_acpt_retry = 0;
uint64_t glob_pf_num_cli_retry = 0;
uint64_t glob_pf_num_srv_retry = 0;

/******************************************************* */
u_int32_t pf_dst_low = 0x00239CFB;
u_int16_t pf_dst_high = 0xA8C5;
int use_pf_flow = 0;

#define MAX_THREADS 32

pfring *stacka_ring[MAX_THREADS] = { NULL };
pfring *am_ring[MAX_THREADS] = { NULL };

u_int64_t bind_core = -1;
u_int8_t verbose = 0;
u_int32_t pf_local_address = 0;
int a_ifindex[MAX_THREADS], stacka_ifindex[MAX_THREADS];
int debug = 0;

/*
 * Find the destination mac for the default gateway
 * This will be used to be filled up with the return packet
 */
void nkn_pf_get_dst_mac(void)
{
    FILE *fp;
    char *bufp;
    char buf[256] = { '\0' };
    char cmd[256] = { '\0' };
    char mac_addr[256] = { '\0' };
    int i;
    unsigned long mac_address = 0;

    fp = popen("route -n | grep ^0.0.0.0 |awk '{ print $2}'", "r");
    fread(buf, 256, 1, fp);
    bufp = buf;
    while(*bufp) {
	if (*bufp == '\r' || *bufp == '\n' || *bufp == '\0') {
	    *bufp = '\0';
	    break;
	}
	bufp++;
    }
    pclose(fp);

    snprintf(cmd, 256, "arping -c 1 -I b0 %s | grep ^Unicast| awk '{ print $5}'", buf);
    buf[0] = '\0';
    fp = popen(cmd, "r");
    fread(buf, 256, 1, fp);
    pclose(fp);


    bufp = buf;
    i = 0;
    while(*bufp) {
	if (*bufp == '\r' || *bufp == '\n' || *bufp == '\0') {
	    *bufp = '\0';
	    mac_addr[i] = *bufp;
	    break;
	}
	if (*bufp >= '0' && *bufp <= '9') {
	    mac_addr[i] = *bufp;
	    i++;
	}
	if (*bufp >= 'A' && *bufp <= 'F') {
	    mac_addr[i] = *bufp;
	    i++;
	}
	if (*bufp >= 'a' && *bufp <= 'f') {
	    mac_addr[i] = *bufp;
	    i++;
	}
	bufp++;
    }

    sscanf(mac_addr, "%lx", &mac_address);
    pf_dst_low  = (mac_address >> 16) & 0xFFFFFFFF;
    pf_dst_high = mac_address & 0xFFFF;

}

/*
 * IP checksum for generated reset reject packets
 */
static
uint16_t ip_checksum(char *buff, int len)
{
	const uint16_t *buf = (const uint16_t *)buff;
	uint32_t sum;
	size_t length = len;

	sum = 0;

	while (len > 1) {
		sum += *buf++;
		if (sum & 0x80000000)
		sum = (sum & 0xFFFF) + (sum >> 16);
		len -= 2;
	}

	if (len & 1)
		sum += *((uint8_t *)buf);

	while (sum >> 16)
		sum = (sum & 0xFFFF) + (sum >> 16);

	return ((uint16_t)(~sum));
}


/*
 * IP checksum for generated reset reject packets
 */
static
uint16_t tcp_checksum(const void *buff, size_t len, unsigned int src_addr, unsigned int dest_addr)
{
	const uint16_t *buf = buff;
	uint16_t *ip_src = (void *)&src_addr, *ip_dst = (void *)&dest_addr;
	uint32_t sum;
	size_t length = len;

	sum = 0;
	while (len > 1) {
		sum += *buf++;
		if (sum & 0x80000000)
		sum = (sum & 0xFFFF) + (sum >> 16);
		len -= 2;
	}

	if (len & 1)
		sum += *((uint8_t *)buf);

	sum += *(ip_src++);
	sum += *ip_src;
	sum += *(ip_dst++);
	sum += *ip_dst;
	sum += htons(IPPROTO_TCP);
	sum += htons(length);

	while (sum >> 16)
		sum = (sum & 0xFFFF) + (sum >> 16);

	return ((uint16_t)(~sum));
}

#define IP_DF           0x4000

/*
 * Create reset packet to client
 */
static
int construct_client_reset(char *in, int in_len,  char *out, int out_len)
{
	struct iphdr *iph_in = (struct iphdr *)in;
	struct tcphdr *tcph_in = (struct tcphdr *) ((char *)(iph_in) +  iph_in->ihl * 4);
	struct iphdr *iph_out = (struct iphdr *)out;
	struct tcphdr *tcph_out;


	memset(iph_out, 0, sizeof(struct iphdr));

	iph_out->version = 4;
	iph_out->ihl       = sizeof(struct iphdr)/4;
        iph_out->tos       = 0;
	iph_out->tot_len   = htons(sizeof(struct iphdr) + sizeof(struct tcphdr));
        iph_out->id        = 0;
        iph_out->frag_off  = htons(IP_DF);
        iph_out->protocol  = IPPROTO_TCP;
        iph_out->saddr     = iph_in->daddr;
        iph_out->daddr     = iph_in->saddr;
	iph_out->ttl	   = 12;
        iph_out->check     = ip_checksum((char *)iph_out, sizeof(struct iphdr));

	tcph_out = (struct tcphdr *) ((char *)(iph_out) + iph_out->ihl * 4);

	memset(tcph_out, 0, sizeof(struct tcphdr));
        tcph_out->source    = tcph_in->dest;
        tcph_out->dest      = tcph_in->source;
        tcph_out->doff      = sizeof(struct tcphdr) / 4;

	if (tcph_in->ack) {
		tcph_out->seq = tcph_in->ack_seq;
	} else {
		tcph_out->ack_seq = htonl(ntohl(tcph_in->seq) + tcph_in->syn + tcph_in->fin +
					in_len - (iph_in->ihl * 4) - (tcph_in->doff << 2));
		tcph_out->ack = 1;
	}
	tcph_out->rst = 1;
	tcph_out->check = tcp_checksum((char *)tcph_out, sizeof(struct tcphdr), iph_out->saddr, iph_out->daddr);

	return sizeof(struct iphdr) + sizeof(struct tcphdr);;
}

/*
 * Create reset packet to server
 */
static
int construct_server_reset(char *in, int in_len,  char *out, int out_len)
{
	struct iphdr *iph_in = (struct iphdr *)in;
	struct tcphdr *tcph_in = (struct tcphdr *) ((char *)(iph_in) +  iph_in->ihl * 4);
	struct iphdr *iph_out = (struct iphdr *)out;
	struct tcphdr *tcph_out;


	memset(iph_out, 0, sizeof(struct iphdr));

	iph_out->version = 4;
	iph_out->ihl       = sizeof(struct iphdr)/4;
        iph_out->tos       = 0;
	iph_out->tot_len   = htons(sizeof(struct iphdr) + sizeof(struct tcphdr));
        iph_out->id        = 0;
        iph_out->frag_off  = htons(IP_DF);
        iph_out->protocol  = IPPROTO_TCP;
        iph_out->saddr     = iph_in->saddr;
        iph_out->daddr     = iph_in->daddr;
	iph_out->ttl	   = 12;
        iph_out->check     = ip_checksum((char *)iph_out, sizeof(struct iphdr));

	tcph_out = (struct tcphdr *) ((char *)(iph_out) + iph_out->ihl * 4);

	memset(tcph_out, 0, sizeof(struct tcphdr));
        tcph_out->source    = tcph_in->source;
        tcph_out->dest      = tcph_in->dest;
        tcph_out->doff      = sizeof(struct tcphdr) / 4;
	tcph_out->seq	    = tcph_in->seq;
	tcph_out->ack_seq   = tcph_in->ack_seq;
	tcph_out->ack	    = tcph_in->ack;
	tcph_out->rst = 1;
	tcph_out->check = tcp_checksum((char *)tcph_out, sizeof(struct tcphdr), iph_out->saddr, iph_out->daddr);

	return sizeof(struct iphdr) + sizeof(struct tcphdr);
}

/*
 * Forward the packet to the server
 */
void nkn_pf_accept(unsigned char *buffer, int len, void *u)
{
    buffer = buffer;
    len = len;
    u = u;
#ifndef PF_MIRROR
    pfring *send_ring = (pfring *)u;
    int rc;

    /* Change the destination Mac */
    *(u_int32_t *)(buffer + ETH_ALEN) = *(u_int32_t *)buffer;
    *(u_int16_t *)(buffer + ETH_ALEN + sizeof(u_int32_t)) =
			    *(u_int16_t *)(buffer + sizeof(u_int32_t));
    *(u_int32_t *)(buffer) = ntohl(pf_dst_low);
    *(u_int16_t *)(buffer + sizeof(u_int32_t)) = ntohs(pf_dst_high);
send_retry:;
    rc = pfring_send(send_ring, (char *) buffer, len, 1);
    if(rc < 0) {
	glob_pf_num_acpt_retry++;
	goto send_retry;
    }
#endif
}

/*
 * Reject the request based on URL Filter output
 */
void nkn_pf_reject(unsigned char *buffer, int len, void *u)
{
    pfring *send_ring = (pfring *)u;
    char out_data[128];
    int rc;

    /* Change the destination Mac */
    *(u_int32_t *)(out_data + ETH_ALEN) = *(u_int32_t *)buffer;
    *(u_int16_t *)(out_data + ETH_ALEN + sizeof(u_int32_t)) =
			    *(u_int16_t *)(buffer + sizeof(u_int32_t));

    *(u_int32_t *)(out_data) = ntohl(pf_dst_low);
    *(u_int16_t *)(out_data + sizeof(u_int32_t)) = ntohs(pf_dst_high);

    *(u_int16_t *)(out_data + (2 * ETH_ALEN)) =
			     *(u_int16_t *)(buffer + (2 * ETH_ALEN));

send_client_retry:;
    rc = construct_client_reset((char *)(buffer + ETH_HLEN), len - ETH_HLEN,
			   (out_data + ETH_HLEN), 128 - ETH_HLEN);

    rc = pfring_send(send_ring, (char *) out_data, rc + ETH_HLEN, 1);
    if(rc < 0) {
	glob_pf_num_cli_retry++;
	goto send_client_retry;
    } else {
	glob_pf_num_pkts_cli_reset++;
    }

send_server_retry:;
    rc = construct_server_reset((char *)(buffer + ETH_HLEN), len - ETH_HLEN,
			   (out_data + ETH_HLEN), 128 - ETH_HLEN);

    rc = pfring_send(send_ring, (char *) out_data, rc + ETH_HLEN, 1);
    if(rc < 0) {
	glob_pf_num_srv_retry++;
	goto send_server_retry;
    } else {
	glob_pf_num_pkts_srv_reset++;
    }
}

/*
 * Main thread to read/apply filter
 */
static
void *pf_forward_thread(void *arg)
{
    u_int64_t thread_id = (u_int64_t)arg;
    u_char buf[MAX_PF_PKT_LEN];
    unsigned char *buffer;
    struct pfring_pkthdr hdr;
    struct iphdr *iph;
    struct tcphdr *tcph;
    uint16_t eth_type;
    int rc;

#if 0
    if(bind_core >= 0)
	bind2core(bind_core + thread_id);
#endif
    printf("Thread id = %ld\n", thread_id);

    nkn_dpi_pf_set_cpu_id(thread_id);

    while(1) {

	buffer = &buf[0];
	if(pfring_recv(am_ring[thread_id], &buffer, MAX_PF_PKT_LEN, &hdr, 1) > 0) {
#if 0
	    eth_type = *(u_int16_t *)(buffer + (2 * ETH_ALEN));
#endif
	    iph = (struct iphdr *)(buffer + ETH_HLEN);
#if 0
	    if((ntohs(eth_type) == 0x800) &&
		(iph->daddr && ntohl(iph->daddr) != pf_local_address)) 
#endif
	    if((iph->daddr && ntohl(iph->daddr) != pf_local_address)) {
#ifndef QUICK_MODE
		tcph = (struct tcphdr *)(buffer + ETH_HLEN + (iph->ihl * 4));

		if (ntohs(iph->tot_len) == ((iph->ihl * 4) +
			    (tcph->doff * 4))) {
		    if (verbose)
			printf("NonStack Packet\n");
		    /* Change the destination Mac */
		    *(u_int32_t *)(buffer + ETH_ALEN) = *(u_int32_t *)buffer;
		    *(u_int16_t *)(buffer + ETH_ALEN + sizeof(u_int32_t)) =
				    *(u_int16_t *)(buffer + sizeof(u_int32_t));
		    *(u_int32_t *)(buffer) = ntohl(pf_dst_low);
		    *(u_int16_t *)(buffer + sizeof(u_int32_t)) =
				    ntohs(pf_dst_high);
retry:;
#ifndef PF_MIRROR
		    rc = pfring_send(am_ring[thread_id], (char *) buffer,
				    hdr.caplen, 1);
		    if(rc < 0) {
			glob_pf_retry_count++;
			goto retry;
		    } else {
			glob_pf_num_pkts_fwded++;
		    }
#endif
		} else {
#endif
#ifdef PF_LOOPBACK
		    nkn_pf_accept(buffer, hdr.caplen, (void *)am_ring[thread_id]);
#else
		    nkn_dpi_process_pf_packet(buffer,
					(void *)am_ring[thread_id],
					hdr.caplen,
					iph->saddr,
					iph->daddr,
					thread_id);
#endif
#ifndef QUICK_MODE
		}
#endif

	    } else {
#ifndef QUICK_MODE
#ifndef PF_MIRROR
		if (verbose)
		    printf("Stack Packet\n");
		rc = pfring_send(stacka_ring[thread_id], (char *) buffer,
				hdr.caplen, 1);
		if(rc >= 0)
		    glob_pf_num_pkts_to_stack++;
#endif
#endif
	    }
	}
    }
    return NULL;
}

char nkn_pf_interfaces[8][6] = { "eth10", "eth11", "eth12", "eth13", "eth20",
				 "eth21", "eth22", "eth23" };

/* 
 * PF Library Init
 */
pthread_t pd_thread[MAX_THREADS];
int nkn_pfbridge_init()
{
    pfring *a_ring;
#ifndef QUICK_MODE
    pfring *lstacka_ring;
#endif
    char *a_dev = NULL;
    u_int16_t watermark = 1;
    u_int16_t poll_duration = 0;
    int fd;
    struct ifreq ifr;
    char stackifname[512];
    char pfifname[512];
    char counter_str[512];
    int num_channels, num_channel_per_thread, start_channel, end_channel;
    int channel_id = -1;
    uint32_t threads_per_interface = 4;
    u_int64_t ifloopvar, thrloopvar, tid;
    uint32_t num_interfaces = 8;
    uint32_t total_threads = num_interfaces * threads_per_interface;
    ip_addr stack_addr;

    for (ifloopvar=0; ifloopvar<num_interfaces; ifloopvar++) {
	a_dev = nkn_pf_interfaces[ifloopvar];

	fd = socket(AF_INET, SOCK_DGRAM, 0);

	/* I want to get an IPv4 IP address */
	ifr.ifr_addr.sa_family = AF_INET;

	/* I want IP address attached to a_dev */
	strncpy(ifr.ifr_name, "b0", IFNAMSIZ-1);

	ioctl(fd, SIOCGIFADDR, &ifr);

	close(fd);
	
	if (!pf_local_address) {
	    /* display result */
	    printf("%s\n",
		    inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
	    pf_local_address = ntohl((
		    ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr));
	} else {
	    printf("%x\n", pf_local_address);
	}

	stack_addr.v4 = ntohl(pf_local_address);

	snprintf(stackifname, 512, "stack:%s", a_dev);

#ifndef QUICK_MODE
#ifdef OLD
	if((lstacka_ring = pfring_open(stackifname, MAX_PF_PKT_LEN,
				       PF_RING_LONG_HEADER)) == NULL) {
	    printf("pfring_open error for %s [%s]\n", stackifname,
		    strerror(errno));
	    return -1;
	}
#else
	if((lstacka_ring = pfring_open(stackifname, MAX_PF_PKT_LEN,
				       0)) == NULL) {
	    printf("pfring_open error for %s [%s]\n", stackifname,
		    strerror(errno));
	    return -1;
	}
#endif
	printf("ring open for %s\n", stackifname);

	pfring_set_application_name(lstacka_ring, (char *)"pfbridge-stacka");
	pfring_set_socket_mode(lstacka_ring, send_only_mode);

	/* Enable Sockets */

	if (pfring_enable_ring(lstacka_ring)) {
	    printf("Unable enabling ring 'a' :-(\n");
	    return -1;
	}
#endif

#ifdef OLD
	if((a_ring = pfring_open(a_dev, MAX_PF_PKT_LEN,  PF_RING_LONG_HEADER))
		== NULL) {
	    printf("pfring_open error for %s [%s]\n", a_dev, strerror(errno));
	    return(-1);
	}
#else
	if((a_ring = pfring_open(a_dev, MAX_PF_PKT_LEN,  0))
		== NULL) {
	    printf("pfring_open error for %s [%s]\n", a_dev, strerror(errno));
	    return(-1);
	}
#endif
	num_channels = pfring_get_num_rx_channels(a_ring);
	pfring_close(a_ring);

	printf("num_channels = %d\n", num_channels);

	num_channel_per_thread = num_channels / threads_per_interface;
	start_channel = 0;
	end_channel = num_channel_per_thread - 1;

	for (thrloopvar=0;thrloopvar<threads_per_interface;thrloopvar++) {
	    tid = (ifloopvar * threads_per_interface) + thrloopvar;
#ifndef QUICK_MODE
	    stacka_ring[tid] = lstacka_ring;
	    pfring_get_bound_device_ifindex(lstacka_ring, &stacka_ifindex[tid]);
#endif
	    snprintf(pfifname, 512, 
		     "%s@%d-%d", a_dev, start_channel, end_channel);
	    printf("ring open for %s\n", pfifname);
#ifdef OLD
	    if((am_ring[tid] = pfring_open(pfifname, MAX_PF_PKT_LEN,
					 PF_RING_LONG_HEADER))
		    == NULL) {
		printf("pfring_open error for %s [%s]\n",
			a_dev, strerror(errno));
		return -1;
	    }
#else
	    if((am_ring[tid] = pfring_open(pfifname, MAX_PF_PKT_LEN,
					 0))
		    == NULL) {
		printf("pfring_open error for %s [%s]\n",
			a_dev, strerror(errno));
		return -1;
	    }
#endif

	    pfring_set_application_name(am_ring[tid], (char *)"pfbridge-a");

	    /* Set the stack ip, so that the packets directed to us are sent
	     * directly to stack and not through pf ring
	     */
	    pfring_set_stack_ip(am_ring[tid], &stack_addr);
	    pfring_set_direction(am_ring[tid], rx_and_tx_direction);
	    pfring_set_socket_mode(am_ring[tid], send_and_recv_mode);
	    //pfring_enable_rss_rehash(am_ring[tid]);
	    pfring_set_poll_watermark(am_ring[tid], watermark);
	    if (use_pf_flow) {
		pfring_set_cluster(am_ring[tid], ifloopvar + 1, cluster_per_flow_2_tuple);
	    }
	    if (pfring_enable_ring(am_ring[tid]) != 0) {
		printf("Unable enabling ring 'a' :-(\n");
		return -1;
	    }
	    pfring_get_bound_device_ifindex(am_ring[tid], &a_ifindex[tid]);
	    pthread_create(&pd_thread[tid], NULL, pf_forward_thread,
			   (void*)tid);
	    start_channel = end_channel + 1;
	    end_channel += num_channel_per_thread;
	    if (thrloopvar == (threads_per_interface - 2))
		end_channel = num_channels;
	}
    }

#if 0
    for (thrloopvar=0;thrloopvar<(num_interfaces * threads_per_interface);
		thrloopvar++) {
	pthread_join(pd_thread[thrloopvar], NULL);
	if (am_ring[thrloopvar])
	    pfring_close(am_ring[thrloopvar]);
	if (stacka_ring[thrloopvar])
	    pfring_close(stacka_ring[thrloopvar]);
    }
#endif

    return(0);
}

#endif /* _STUB_ */
