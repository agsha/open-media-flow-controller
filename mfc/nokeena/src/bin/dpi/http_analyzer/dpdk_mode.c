/*
 * File: dpdk_mode.c
 *   
 * Author: Yuvaraja Mariappan & Ramanand Narayanan
 *
 *
 * (C) Copyright 2014 Juniper Networks, Inc.
 * All rights reserved.
 */

#ifdef _STUB_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <setjmp.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>

/**
 * Includes for DPDK.
 */
#include <sys/queue.h>
#include <rte_config.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_pci.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_timer.h>
#include <rte_hash_crc.h>
#include "rte_common.h"
#include "rte_memcpy.h"
#include "rte_tailq.h"
#include "rte_interrupts.h"

#include <linux/ip.h>

/* DPI related headers */
#include "thread.h"

#include "atomic_ops.h"
#include "nkn_stat.h"
#include "nkn_debug.h"

#define RTE_LOGTYPE_L2FWD RTE_LOGTYPE_USER1

#define MBUF_SIZE (2048 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
#define NB_MBUF   8192

/*
 * RX and TX Prefetch, Host, and Write-back threshold values should be
 * carefully set for optimal performance. Consult the network
 * controller's datasheet and supporting DPDK documentation for guidance
 * on how these parameters should be set.
 */
#define RX_PTHRESH 8 /**< Default values of RX prefetch threshold reg. */
#define RX_HTHRESH 8 /**< Default values of RX host threshold reg. */
#define RX_WTHRESH 4 /**< Default values of RX write-back threshold reg. */

/*
 * These default values are optimized for use with the Intel(R) 82599 10 GbE
 * Controller and the DPDK ixgbe PMD. Consider using other values for other
 * network controllers and/or network drivers.
 */
#define TX_PTHRESH 36 /**< Default values of TX prefetch threshold reg. */
#define TX_HTHRESH 0  /**< Default values of TX host threshold reg. */
#define TX_WTHRESH 0  /**< Default values of TX write-back threshold reg. */

#define MAX_PKT_BURST 32
#define BURST_TX_DRAIN_US 100 /* TX drain every ~100us */

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_TEST_RX_DESC_DEFAULT 512
#define RTE_TEST_TX_DESC_DEFAULT 128
static uint16_t nb_rxd = RTE_TEST_RX_DESC_DEFAULT;
static uint16_t nb_txd = RTE_TEST_TX_DESC_DEFAULT;

/* ethernet addresses of ports */
static struct ether_addr ports_eth_addr[RTE_MAX_ETHPORTS];

/* mask of enabled ports */
static uint32_t enabled_port_mask = 0;

static unsigned int rx_queue_per_lcore = 1;

#define MAX_RX_QUEUE_PER_LCORE 16
#define MAX_TX_QUEUE_PER_PORT 16
struct lcore_queue_conf {
	unsigned n_rx_port;
	unsigned rx_port_list[MAX_RX_QUEUE_PER_LCORE];
} __rte_cache_aligned;
struct lcore_queue_conf lcore_queue_conf[RTE_MAX_LCORE];

static const struct rte_eth_conf port_conf = {
	.rxmode = {
		.split_hdr_size = 0,
		.header_split   = 0, /**< Header Split disabled */
		.hw_ip_checksum = 0, /**< IP checksum offload disabled */
		.hw_vlan_filter = 0, /**< VLAN filtering disabled */
		.jumbo_frame    = 0, /**< Jumbo Frame Support disabled */
		.hw_strip_crc   = 0, /**< CRC stripped by hardware */
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
};

static const struct rte_eth_rxconf rx_conf = {
	.rx_thresh = {
		.pthresh = RX_PTHRESH,
		.hthresh = RX_HTHRESH,
		.wthresh = RX_WTHRESH,
	},
};

static const struct rte_eth_txconf tx_conf = {
	.tx_thresh = {
		.pthresh = TX_PTHRESH,
		.hthresh = TX_HTHRESH,
		.wthresh = TX_WTHRESH,
	},
	.tx_free_thresh = 0, /* Use PMD default values */
	.tx_rs_thresh = 0, /* Use PMD default values */
};

struct rte_mempool * pktmbuf_pool = NULL;

struct rte_timer dpdk_timer;
static uint64_t dpdk_tick = 0;

/* Per-port statistics struct */
struct port_statistics {
	uint64_t rx;
} __rte_cache_aligned;
struct port_statistics port_statistics[RTE_MAX_ETHPORTS];

/* A tsc-based timer responsible for triggering statistics printout */
#define TIMER_MILLISECOND 2000000ULL /* around 1ms at 2 Ghz */
#define MAX_TIMER_PERIOD 86400 /* 1 day max */
#define NB_TICK_PER_SECOND 256
#define MAIN_LOOP_TIMER 2000000ULL /* around 1000us at 2 Ghz */
static int64_t timer_period = 10 * TIMER_MILLISECOND * 1000; /* default period is 10 seconds */

/* Extern Variables */
extern unsigned long long packets_count;
extern unsigned long long packets_dropped;
extern unsigned long long packets_queued;
extern struct dpi_engine_data  deng_data;
extern volatile int application_not_stopped;
extern struct smp_thread smp_thread[NUM_THREADS];

/* Nkn Count variable */
NKNCNT_DEF(dpitproxy_total_packets_rx, AO_t, "", "total packets rx")
NKNCNT_DEF(dpitproxy_packets_count, AO_t, "", "packets count")
NKNCNT_DEF(dpitproxy_packets_dropped, AO_t, "", "packets dropped")
NKNCNT_DEF(dpitproxy_packets_queued, AO_t, "", "packets queued")
NKNCNT_DEF(dpitproxy_dpdk_tick, AO_t, "", "dpdk tick")
	




/* Extern Function Prototypes */
int process_dpdk_packet(char *data, unsigned int data_len);

/* Local Function Prototypes */
int dpdk_init(int argc, const char **argv);
int dpdk_main(void);


/* Local Functions Start Here */

/* Timer function for calculating the ticks */
static void
dpdk_timer_cb(struct rte_timer *timer, void *arg)
{
	(void) (timer);
	(void) (arg);
	dpdk_tick++;
	AO_store(&glob_dpitproxy_dpdk_tick, dpdk_tick);
}


/* Print out statistics on packets dropped */
static void
print_stats(void)
{
	uint64_t total_packets_dropped, total_packets_tx, total_packets_rx;
	unsigned portid;

	total_packets_dropped = 0;
	total_packets_tx = 0;
	total_packets_rx = 0;

	const char clr[] = { 27, '[', '2', 'J', '\0' };
	const char topLeft[] = { 27, '[', '1', ';', '1', 'H','\0' };

#if 0 // COMMENTED OUT - Ram
	/* Clear screen and move to top left */
	printf("%s%s", clr, topLeft);
#endif /* 0 COMMENTED BY Ram */

	DBG_LOG(MSG, MOD_DPI_TPROXY,"Port statistics ====================================");

	for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++) {
		/* skip disabled ports */
		if ((enabled_port_mask & (1 << portid)) == 0)
			continue;
		DBG_LOG(MSG, MOD_DPI_TPROXY,"Statistics for port %u ------------------------------"
			   "\nPackets received: %20"PRIu64,
			   portid,
			   port_statistics[portid].rx);

		total_packets_rx += port_statistics[portid].rx;
	}
	DBG_LOG(MSG, MOD_DPI_TPROXY,"Aggregate statistics ==============================="
		"\nTotal packets received: %14"PRIu64,
		total_packets_rx);
	DBG_LOG(MSG, MOD_DPI_TPROXY,"# of PKTS RECEIVED: %llu, DROPPED: %llu, QUEUED: %llu", 
		packets_count, packets_dropped, packets_queued);
	DBG_LOG(MSG, MOD_DPI_TPROXY,"dpdk_tick %llu\n", ((dpdk_tick & ~(NB_TICK_PER_SECOND - 1))));
	DBG_LOG(MSG, MOD_DPI_TPROXY,"====================================================");
	AO_store(&glob_dpitproxy_total_packets_rx, total_packets_rx);
}

/* main processing loop */
static void
main_loop(void)
{
	char *data;
	int data_len;
	char buf[2048];
	unsigned lcore_id;
	struct rte_mbuf *m;
	unsigned i, j, portid, nb_rx;
	struct lcore_queue_conf *qconf;
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	uint64_t prev_tick_tsc, prev_tsc, diff_tsc, cur_tsc, timer_tsc;
	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S * BURST_TX_DRAIN_US;


	/* Initialize */
	prev_tsc = 0;
	prev_tick_tsc = 0;
	timer_tsc = 0;

	lcore_id = rte_lcore_id();
	qconf = &lcore_queue_conf[lcore_id];

	if (qconf->n_rx_port == 0) {
		RTE_LOG(INFO, L2FWD, "lcore %u has nothing to do\n", lcore_id);
		return;
	}

	RTE_LOG(INFO, L2FWD, "entering main loop on lcore %u\n", lcore_id);

	for (i = 0; i < qconf->n_rx_port; i++) {

		portid = qconf->rx_port_list[i];
		RTE_LOG(INFO, L2FWD, " -- lcoreid=%u portid=%u\n", lcore_id,
			portid);
	}

	/* Loop till we get a ctrl-c */
	while (application_not_stopped) {
		cur_tsc = rte_rdtsc();

		/* Now check for the tick timer */
		if(cur_tsc - prev_tick_tsc > MAIN_LOOP_TIMER) {
			rte_timer_manage();
			prev_tick_tsc = cur_tsc;
		}

		diff_tsc = cur_tsc - prev_tsc;
		if (unlikely(diff_tsc > drain_tsc)) {
			/* if timer is enabled */
			if (timer_period > 0) {

				/* advance the timer */
				timer_tsc += diff_tsc;
				/* if timer has reached its timeout */
				if (unlikely(timer_tsc >= (uint64_t) timer_period)) {
					/* do this only on master core */
					if (lcore_id == rte_get_master_lcore()) {
						print_stats();
						/* reset the timer */
						timer_tsc = 0;
					}
				}
			}
			prev_tsc = cur_tsc;
		}

		/*
		 * Read packet from RX queues
		 */
		for (i = 0; i < qconf->n_rx_port; i++) {
			portid = qconf->rx_port_list[i];
			nb_rx = rte_eth_rx_burst((uint8_t) portid, 0,
						 pkts_burst, MAX_PKT_BURST);
			port_statistics[portid].rx += nb_rx;
			for (j = 0; j < nb_rx; j++) {
				m = pkts_burst[j];
				rte_prefetch0(rte_pktmbuf_mtod(m, void *));
				if (m->type == RTE_MBUF_PKT) {

					data = (char *)m->pkt.data;
					data_len = m->pkt.data_len;

					/* Commenting out rte_memcpy () *
					 * for process_dpdk_packet ()
					rte_memcpy(pkt_buffer, data, data_len);
					 * */

					/* Call the dpi logic to process pkt */
					process_dpdk_packet (data, data_len);

					/* Free the mbuf */
					rte_pktmbuf_free(m);

				} else if (m->type == RTE_MBUF_CTRL) {
					data = (char *)m->ctrl.data;
					data_len = m->ctrl.data_len;
					rte_memcpy(buf, data, data_len);
					rte_ctrlmbuf_free(m);
				}
			} /* end of for (j = ...) */
		} /* end of for (i = ...) */
	} /* end of while (1) */

} /* end of main_loop () */


static int
launch_one_lcore(__attribute__((unused)) void *dummy)
{
	main_loop();
	return 0;
}


/* display usage */
static void usage(const char *prgname)
{
	printf("%s [EAL options] -- -p PORTMASK [-q NQ]\n"
	       "  -p PORTMASK: hexadecimal bitmask of ports to configure\n"
	       "  -q NQ: number of queue (=ports) per lcore (default is 1)\n"
		   "  -T PERIOD: statistics will be refreshed each PERIOD seconds (0 to disable, 10 default, 86400 maximum)\n",
	       prgname);
}

static int parse_portmask(const char *portmask)
{
	char *end = NULL;
	unsigned long pm;

	/* parse hexadecimal string */
	pm = strtoul(portmask, &end, 16);
	if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;

	if (pm == 0)
		return -1;

	return pm;
}

static unsigned int parse_nqueue(const char *q_arg)
{
	char *end = NULL;
	unsigned long n;

	/* parse hexadecimal string */
	n = strtoul(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		return 0;
	if (n == 0)
		return 0;
	if (n >= MAX_RX_QUEUE_PER_LCORE)
		return 0;

	return n;
}

static int parse_timer_period(const char *q_arg)
{
	char *end = NULL;
	int n;

	/* parse number string */
	n = strtol(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;
	if (n >= MAX_TIMER_PERIOD)
		return -1;

	return n;
}

/* Parse the argument given in the command line of the application */
static int parse_args(int argc, const char **argv)
{
	int opt, ret;
	char **argvopt;
	int option_index;
	char *prgname = argv[0];
	static struct option lgopts[] = {
		{NULL, 0, 0, 0}
	};

	argvopt = argv;

	while ((opt = getopt_long(argc, argvopt, "p:q:T:",
				  lgopts, &option_index)) != EOF) {

		switch (opt) {
		/* portmask */
		case 'p':
			enabled_port_mask = parse_portmask(optarg);
			if (enabled_port_mask == 0) {
				printf("invalid portmask\n");
				usage(prgname);
				return -1;
			}
			break;

		/* nqueue */
		case 'q':
			rx_queue_per_lcore = parse_nqueue(optarg);
			if (rx_queue_per_lcore == 0) {
				printf("invalid queue number\n");
				usage(prgname);
				return -1;
			}
			break;

		/* timer period */
		case 'T':
			timer_period = parse_timer_period(optarg) * 1000 * TIMER_MILLISECOND;
			if (timer_period < 0) {
				printf("invalid timer period\n");
				usage(prgname);
				return -1;
			}
			break;

		/* long options */
		case 0:
			usage(prgname);
			return -1;

		default:
			usage(prgname);
			return -1;
		}
	}

	if (optind >= 0)
		argv[optind-1] = prgname;

	ret = optind-1;
	optind = 0; /* reset getopt lib */
	return ret;
}

/* Check the link status of all ports in up to 9s, and print them finally */
static void check_all_ports_link_status(uint8_t port_num, uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	uint8_t portid, count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;

	printf("\nChecking link status");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		all_ports_up = 1;
		for (portid = 0; portid < port_num; portid++) {
			if ((port_mask & (1 << portid)) == 0)
				continue;
			memset(&link, 0, sizeof(link));
			rte_eth_link_get_nowait(portid, &link);
			/* print link status if flag set */
			if (print_flag == 1) {
				if (link.link_status)
					printf("Port %d Link Up - speed %u "
						"Mbps - %s\n", (uint8_t)portid,
						(unsigned)link.link_speed,
				(link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
					("full-duplex") : ("half-duplex\n"));
				else
					printf("Port %d Link Down\n",
						(uint8_t)portid);
				continue;
			}
			/* clear all_ports_up flag if any link down */
			if (link.link_status == 0) {
				all_ports_up = 0;
				break;
			}
		}
		/* after finally printing all link status, get out */
		if (print_flag == 1)
			break;

		if (all_ports_up == 0) {
			printf(".");
			fflush(stdout);
			rte_delay_ms(CHECK_INTERVAL);
		}

		/* set the print_flag if all ports up or timeout */
		if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
			print_flag = 1;
			printf("done\n");
		}
	}
}

/**
 * function: process_dpdk_packet ()
 *
 * purpose: allocate memory, copy the packet data and add it to the list 
 * */
int
process_dpdk_packet (char *data, unsigned int data_len)
{
	struct smp_pkt *smp_pkt;
	struct smp_thread *th;
	static int thread_id = 0;
	struct iphdr *ip;
	ctb_uint32    crc = 0;


	/* Increment the packet count */
	packets_count++;
	AO_fetch_and_add1(&glob_dpitproxy_packets_count);

	/* Take a copy of every packet and push into the list of the 
	 * destination thread
	 */
	if ((smp_pkt = calloc(1, sizeof(struct smp_pkt) + data_len)) == NULL) {
		DBG_LOG(ERROR, MOD_DPI_TPROXY,"error: memory allocation failed for new packet");
		return -1;
	}

	smp_pkt->pkt.deng_data = &deng_data;
	smp_pkt->pkt.caplen = data_len;

	/* Get tick for filling ts */
	smp_pkt->pkt.ts.tv_sec = (dpdk_tick & ~(NB_TICK_PER_SECOND - 1));
	smp_pkt->pkt.ts.tv_usec = 0;

	if (data) {
		smp_pkt->pkt.data = (unsigned char *)&smp_pkt[1];
		memcpy(smp_pkt->pkt.data, data, data_len);
	}
	
	/* compute hashkey on this packet for thread dispatching */
	ip = (struct iphdr *) (data + 14);
	crc = rte_hash_crc_4byte(ip->saddr, 0x04C11DB7) ^ rte_hash_crc_4byte(ip->daddr, 0x04C11DB7);
	thread_id = crc % NUM_THREADS;

#if 0 /* Trying out doing the hash in the worker thread */
	unsigned int hashkey = get_hashkey((unsigned char*)data, data_len);

	/* get thread destination structure */
	thread_id = hashkey & (NUM_THREADS - 1);
#endif /* 0 - COMMENTED by Ram */

	th = &smp_thread[thread_id];

	/* Enqueue the packet info in the ring buffer */
	if(rte_ring_sp_enqueue(th->queue, smp_pkt) < 0) {
		/* Failed to add hence dropping */
		packets_dropped++;
		AO_fetch_and_add1(&glob_dpitproxy_packets_dropped);
		free (smp_pkt);
	}
	else {
		packets_queued++;
		AO_fetch_and_add1(&glob_dpitproxy_packets_queued);
	}
	
#if 0 // COMMENTED OUT TO TRY THE RING BUFFER LOGIC
	pthread_spin_lock(&th->lock);
	list_add_tail((struct list_entry *)smp_pkt,(struct list_entry *)&th->pkt_head);
	pthread_spin_unlock(&th->lock);
#endif // 0

	return 0;

} /* end process_dpdk_packet () */



/********************************************************************************
 * function: dpdk_init()
 *
 ********************************************************************************/
int
dpdk_init(int argc, const char **argv)
{
	int ret;
	uint64_t hz;
	uint8_t portid;
	uint8_t nb_ports;
	uint8_t nb_ports_available;
	unsigned lcore_id, rx_lcore_id;
	struct lcore_queue_conf *qconf;


	/* init EAL */
	ret = rte_eal_init(argc, (char*)argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
	argc -= ret;
	argv += ret;

	/* parse application arguments (after the EAL ones) */
	ret = parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid L2FWD arguments\n");

	/* create the mbuf pool */
	pktmbuf_pool =
		rte_mempool_create("mbuf_pool", NB_MBUF,
				   MBUF_SIZE, 32,
				   sizeof(struct rte_pktmbuf_pool_private),
				   rte_pktmbuf_pool_init, NULL,
				   rte_pktmbuf_init, NULL,
				   rte_socket_id(), 0);
	if (pktmbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");



	/* init driver(s) */
	if (rte_ixgbe_pmd_init() < 0) {
		rte_exit(EXIT_FAILURE, "Cannot init ixgbe pmd\n");
	}

	if (rte_eal_pci_probe() < 0)
		rte_exit(EXIT_FAILURE, "Cannot probe PCI\n");

	nb_ports = rte_eth_dev_count();
	if (nb_ports == 0)
		rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");

	if (nb_ports > RTE_MAX_ETHPORTS)
		nb_ports = RTE_MAX_ETHPORTS;

	rx_lcore_id = 0;
	qconf = NULL;

	/* Initialize the port/queue configuration of each logical core */
	for (portid = 0; portid < nb_ports; portid++) {
		/* skip ports that are not enabled */
		if ((enabled_port_mask & (1 << portid)) == 0)
			continue;

		/* get the lcore_id for this port */
		while (rte_lcore_is_enabled(rx_lcore_id) == 0 ||
		       lcore_queue_conf[rx_lcore_id].n_rx_port ==
		       rx_queue_per_lcore) {
			rx_lcore_id++;
			if (rx_lcore_id >= RTE_MAX_LCORE)
				rte_exit(EXIT_FAILURE, "Not enough cores\n");
		}

		if (qconf != &lcore_queue_conf[rx_lcore_id])
			/* Assigned a new logical core in the loop above. */
			qconf = &lcore_queue_conf[rx_lcore_id];

		qconf->rx_port_list[qconf->n_rx_port] = portid;
		qconf->n_rx_port++;
		printf("Lcore %u: RX port %u\n", rx_lcore_id, (unsigned) portid);
	}

	nb_ports_available = nb_ports;

	/* Initialise each port */
	for (portid = 0; portid < nb_ports; portid++) {
		/* skip ports that are not enabled */
		if ((enabled_port_mask & (1 << portid)) == 0) {
			printf("Skipping disabled port %u\n", (unsigned) portid);
			nb_ports_available--;
			continue;
		}
		/* init port */
		printf("Initializing port %u... ", (unsigned) portid);
		fflush(stdout);
		ret = rte_eth_dev_configure(portid, 1, 1, &port_conf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n",
				  ret, (unsigned) portid);

		rte_eth_macaddr_get(portid,&ports_eth_addr[portid]);

		/* init one RX queue */
		fflush(stdout);
		ret = rte_eth_rx_queue_setup(portid, 0, nb_rxd,
					     rte_eth_dev_socket_id(portid), &rx_conf,
					     pktmbuf_pool);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup:err=%d, port=%u\n",
				  ret, (unsigned) portid);

		/* init one TX queue on each port */
		fflush(stdout);
		ret = rte_eth_tx_queue_setup(portid, 0, nb_txd,
				rte_eth_dev_socket_id(portid), &tx_conf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup:err=%d, port=%u\n",
				ret, (unsigned) portid);

		/* Start device */
		ret = rte_eth_dev_start(portid);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err=%d, port=%u\n",
				  ret, (unsigned) portid);

		printf("done: \n");

		rte_eth_promiscuous_enable(portid);

		printf("Port %u, MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n\n",
				(unsigned) portid,
				ports_eth_addr[portid].addr_bytes[0],
				ports_eth_addr[portid].addr_bytes[1],
				ports_eth_addr[portid].addr_bytes[2],
				ports_eth_addr[portid].addr_bytes[3],
				ports_eth_addr[portid].addr_bytes[4],
				ports_eth_addr[portid].addr_bytes[5]);

		/* initialize port stats */
		memset(&port_statistics, 0, sizeof(port_statistics));
	}

	if (!nb_ports_available) {
		rte_exit(EXIT_FAILURE,
			"All available ports are disabled. Please set portmask.\n");
	}

	check_all_ports_link_status(nb_ports, enabled_port_mask);

	/* Setup timer function for calculating ticks */
	hz = rte_get_tsc_hz();
	lcore_id = rte_lcore_id();
	rte_timer_init(&dpdk_timer);
	rte_timer_reset(&dpdk_timer, hz / NB_TICK_PER_SECOND, PERIODICAL, lcore_id, dpdk_timer_cb, NULL);

	return 0;

} /* end of dpdk_init () */


/********************************************************************************
 * function: dpdk_main()
 *
 ********************************************************************************/
int
dpdk_main()
{
	unsigned lcore_id;

	/* launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(launch_one_lcore, NULL, CALL_MASTER);
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0)
			return -1;
	}

	return 0;

} /* end of dpdk_main () */

#endif /* _STUB_ */

/* End of dpdk_mode.c */
