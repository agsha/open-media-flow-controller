/* File : nkn_dpi_netfilter.c
 * Copyright (C) 2014 Juniper Networks, Inc. 
 * All rights reserved.
 */

#ifdef _STUB_

/**
 * @file nkn_dpi_netfilter.c
 * @brief Utility functions for netfilter related functions
 * @author
 * @version 1.00
 * @date 2014-04-10
 */

#ifdef NFQUEUE_URI_FILTER
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <linux/types.h>
#include <linux/netfilter.h>	    /* for NF_ACCEPT */
#include <pthread.h>
#include <libnetfilter_queue/libnetfilter_queue.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <atomic_ops.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <errno.h>

#include "nkn_assert.h"
#include "nkn_dpi_netfilter.h"
#include "nkn_dpi.h"

struct ethhdr dummy_ethhdr;
TAILQ_HEAD(nkn_nf_dpi_xfer_list_t, nfqueue_dpi_xfer_st)
	    nkn_nf_dpi_xfer_list[MAX_NFQUEUE_THREADS];
nkn_mutex_t nkn_nf_dpi_xfer_lock[MAX_NFQUEUE_THREADS];
pthread_cond_t nkn_nf_dpi_xfer_cond[MAX_NFQUEUE_THREADS];
AO_t glob_dpi_filter_non_data_cnt;
extern uint64_t glob_dpi_filter_err_cnt[MAX_NFQUEUE_THREADS];

/*
 * Get the first entry from the nf queue to process
 * This is called from the NKN DPI module
 */
inline nfqueue_dpi_xfer_t *nkn_nf_dpi_get_entry(int64_t tid)
{
    nfqueue_dpi_xfer_t *xfer_data;
    NKN_MUTEX_LOCK(&nkn_nf_dpi_xfer_lock[tid]);
try_again:;
    xfer_data = TAILQ_FIRST(&nkn_nf_dpi_xfer_list[tid]);
    if(xfer_data)
	TAILQ_REMOVE(&nkn_nf_dpi_xfer_list[tid],
			xfer_data, xfer_entries);
    else {
	pthread_cond_wait(&nkn_nf_dpi_xfer_cond[tid],
			  &nkn_nf_dpi_xfer_lock[tid].lock);
	goto try_again;
    }
    NKN_MUTEX_UNLOCK(&nkn_nf_dpi_xfer_lock[tid]);
    return xfer_data;
}

/*
 * Queue an incoming packet to the NKN DPI module
 * Packets that have HTTP data are alone queueud
 */
static u_int32_t queue_pkt_for_url_filtering(struct nfq_q_handle *qh,
	                                     struct nfq_data *nfq_data,
					     int64_t tid)
{
    int id = 0;
    struct nfqnl_msg_packet_hdr *ph;
#if 0
    struct nfqnl_msg_packet_hw *hwph;
#endif
    int64_t ret;
    unsigned char *data;
    nfqueue_dpi_xfer_t *xfer_data;
    struct iphdr *iph;
    struct tcphdr *tcph;

    ph = nfq_get_msg_packet_hdr(nfq_data);
    if (ph)
	id = ntohl(ph->packet_id);

#if 0
    hwph = nfq_get_packet_hw(nfq_data);
    if (hwph) {
	int i, hlen = ntohs(hwph->hw_addrlen);

	printf("hw_src_addr=");
	for (i = 0; i < hlen-1; i++)
	printf("%02x:", hwph->hw_addr[i]);
	printf("%02x ", hwph->hw_addr[hlen-1]);
    }
#endif

    ret = nfq_get_payload(nfq_data, &data);

    iph = (struct iphdr *)data;
    tcph = (struct tcphdr *)(data + (iph->ihl * 4));

    if (ntohs(iph->tot_len) == ((iph->ihl * 4) + (tcph->doff * 4))) {
	nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
	AO_fetch_and_add1(&glob_dpi_filter_non_data_cnt);
	return 0;
#ifdef NFLOOP_NO_PROCESS
    } else {
	nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
	AO_fetch_and_add1(&glob_dpi_filter_non_data_cnt);
	return 0;
#endif
    }

    xfer_data = nkn_calloc_type(1, sizeof(nfqueue_dpi_xfer_t),
				mod_nf_dpi_xfer_t);
    if (!xfer_data) {
	NKN_ASSERT(0);
	nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
	glob_dpi_filter_err_cnt[tid];
	return -1;
    }

    xfer_data->buffer  = data;

    xfer_data->qh      = qh;
    xfer_data->pkt_id  = id;
    xfer_data->pkt_len = ret + sizeof(struct ethhdr);
    xfer_data->ip_src  = ntohl(iph->saddr);
    xfer_data->ip_dst  = ntohl(iph->daddr);

#ifdef PROCESS_IN_NF_THREAD
    nkn_dpi_process_packet(xfer_data, tid);
#else
    NKN_MUTEX_LOCK(&nkn_nf_dpi_xfer_lock[tid]);
    TAILQ_INSERT_TAIL(&nkn_nf_dpi_xfer_list[tid], xfer_data,
		      xfer_entries);
    pthread_cond_signal(&nkn_nf_dpi_xfer_cond[tid]);
    NKN_MUTEX_UNLOCK(&nkn_nf_dpi_xfer_lock[tid]);
#endif

    return 0;
}


/*
 * Callback function for the NFQUEUE
 * Inovked when a packet is recvd, from within nfq_handle_packet
 */
static int nkn_nfqueue_callback(struct nfq_q_handle *qh,
				struct nfgenmsg *nf_msg,
				struct nfq_data *nf_data,
				void *data)
{
    UNUSED_ARGUMENT(nf_msg);
    UNUSED_ARGUMENT(data);
    int64_t tid = (int64_t) data;

    queue_pkt_for_url_filtering(qh, nf_data, tid);
    return -1;

}

/*
 * NFQUEUE main loop
 * Reads packet and calls callback
 */
static void *nkn_nfqueue_thread(void *arg)
{
    struct nfq_handle *nfq_h;
    struct nfq_q_handle *nfq_q_h;
    int64_t thread_id = (int64_t) arg;
    char buf[65536] __attribute__ ((aligned));
    int fd, rv, opt, ret;
    int queue_created;
    char t_name[50];
#if 0
    char mem_pool_str[64];
#endif

    snprintf(t_name, 50, "nfqueue-%ld", thread_id);
    prctl(PR_SET_NAME, t_name, 0, 0, 0);

#if 0
    snprintf(mem_pool_str, sizeof(mem_pool_str), "nfqueuemempool-%ld",
	     thread_id);
    nkn_mem_create_thread_pool(mem_pool_str);
#endif

#ifdef PROCESS_IN_NF_THREAD
    nkn_dpi_event_handle_init(thread_id);
#endif

    UNUSED_ARGUMENT(arg);
    while (1) {
	queue_created = 0;
	nfq_h = nfq_open();
	if (!nfq_h) {
	    DBG_LOG(ERROR, MOD_DPI_URIF, "error during nfq_open()\n");
	    goto end;
	}

	if (nfq_unbind_pf(nfq_h, AF_INET) < 0) {
	    DBG_LOG(ERROR, MOD_DPI_URIF, "error during nfq_unbind_pf()\n");
	    goto end;
	}

	if (nfq_bind_pf(nfq_h, AF_INET) < 0) {
	    if (errno != EEXIST) {
		DBG_LOG(ERROR, MOD_DPI_URIF, "error during nfq_bind_pf()\n");
		goto end;
	    }
	}

	nfq_q_h = nfq_create_queue(nfq_h,  thread_id,
				   &nkn_nfqueue_callback, arg);
	if (!nfq_q_h) {
	    DBG_LOG(ERROR, MOD_DPI_URIF, "error during nfq_create_queue()\n");
	    goto end;
	}

	queue_created = 1;
	if (nfq_set_mode(nfq_q_h, NFQNL_COPY_PACKET, 0xffff) < 0) {
	    DBG_LOG(ERROR, MOD_DPI_URIF, "can't set packet_copy mode\n");
	    goto end;
	}

	fd = nfq_fd(nfq_h);

	opt = 0;
	ret = setsockopt(fd, SOL_NETLINK, NETLINK_BROADCAST_SEND_ERROR,
			 &opt, sizeof(int));
	if (ret < 0) {
	    DBG_LOG(ERROR, MOD_DPI_URIF,
		    "Failed to un set broadcast send error\n");
	}

	opt = 1;
	ret = setsockopt(fd, SOL_NETLINK, NETLINK_NO_ENOBUFS, &opt,
			 sizeof(int));
	if (ret < 0) {
	    DBG_LOG(ERROR, MOD_DPI_URIF, "Failed to set no enobug\n");
	}


	opt = 1024 * 1024 * 256;
	ret = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &opt, sizeof(int));
	if (ret < 0) {
	    DBG_LOG(ERROR, MOD_DPI_URIF, "Failed to set recv buf size\n");
	}

	ret = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &opt, sizeof(int));
	if (ret < 0) {
	    DBG_LOG(ERROR, MOD_DPI_URIF, "Failed to set send buf size\n");
	}

	while ((rv = recv(fd, buf, sizeof(buf), 0)) && rv >= 0) {
	    nfq_handle_packet(nfq_h, buf, rv);
	}

end:;
	if (queue_created)
	    nfq_destroy_queue(nfq_q_h);

	nfq_close(nfq_h);
	/*
	 * Wait for 2 secs before retrying
	 */
	sleep(2);
    }

    return NULL;
}

/*
 * NFQUEUE module init
 * MAX_NFQUEUE_THREADS should be set to the number of nf queues used in iptable
 */
int nkn_nfqueue_init()
{
    uint64_t thread;
    pthread_attr_t attr;
    pthread_t nfqueue_pt[MAX_NFQUEUE_THREADS];
    char xfer_name[255];
    int ret;
#ifdef PROCESS_IN_NF_THREAD
    cpu_set_t cpu_set;
    uint64_t cpu_id;
#endif

    setpriority(PRIO_PROCESS, 0, -1);

    dummy_ethhdr.h_dest[0] = 0x00;
    dummy_ethhdr.h_dest[1] = 0x02;
    dummy_ethhdr.h_dest[2] = 0x03;
    dummy_ethhdr.h_dest[3] = 0x04;
    dummy_ethhdr.h_dest[4] = 0x05;
    dummy_ethhdr.h_dest[5] = 0x00;
    dummy_ethhdr.h_source[0] = 0x00;
    dummy_ethhdr.h_source[1] = 0x03;
    dummy_ethhdr.h_source[2] = 0x04;
    dummy_ethhdr.h_source[3] = 0x05;
    dummy_ethhdr.h_source[4] = 0x06;
    dummy_ethhdr.h_source[5] = 0x00;
    dummy_ethhdr.h_proto = ntohs(ETH_P_IP);

    for (thread = 0; thread < MAX_NFQUEUE_THREADS; thread++) {
	snprintf(xfer_name, 255, "nf_dpi-xfer-mutex-%ld", thread);
	ret = NKN_MUTEX_INIT(&nkn_nf_dpi_xfer_lock[thread], NULL, xfer_name);
	if(ret < 0) {
	    assert(0);
	    return -1;
	}
	pthread_cond_init(&nkn_nf_dpi_xfer_cond[thread], NULL);
	TAILQ_INIT(&nkn_nf_dpi_xfer_list[thread]);
	ret = pthread_attr_init(&attr);
	if (ret < 0) {
	    DBG_LOG(ERROR, MOD_DPI_URIF, "Failed to init pthread attribute\n");
	    return -1;
	}

	pthread_create(&nfqueue_pt[thread], &attr, nkn_nfqueue_thread,
		       (void *)thread);

#ifdef PROCESS_IN_NF_THREAD
	if (thread >= 30)
	    cpu_id = thread - 30;
	else
	    cpu_id = thread;
	/* Set the CPU affinity */
	CPU_ZERO(&cpu_set);
	CPU_SET(cpu_id + 2, &cpu_set);
	pthread_setaffinity_np(nfqueue_pt[thread], sizeof(cpu_set_t), &cpu_set);
#endif
    }
    return 0;
}

#endif

#endif /* _STUB_ */
