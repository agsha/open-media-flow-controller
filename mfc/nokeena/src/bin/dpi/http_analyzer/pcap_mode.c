/*
 * File: pcap_mode.c
 *   
 * Author: Sivakumar Gurumurthy 
 *
 *
 * (C) Copyright 2014 Juniper Networks, Inc.
 * All rights reserved.
 */

#ifdef _STUB_

/* Include Files */
#include <stdio.h>
#include <string.h>
#include <signal.h>

#include "event_handler.h"
#include "thread.h"

#include "atomic_ops.h"
#include "nkn_stat.h"
#include "nkn_debug.h"

/* Local Variables */


/* Function Prototypes */
static int dpi_performance_tuning(void);
static int dpi_init(void);
int process_pcap_packet(const struct pcap_pkthdr *pkthdr, unsigned char *data);
void run_threads(void);
int pcap_main (int argc, const char **argv);


/* Extern Variables */
extern unsigned long long packets_count;
extern struct dpi_engine_data  deng_data;
extern volatile int application_not_stopped;
extern struct smp_thread smp_thread[NUM_THREADS];


/********************************************************************************
 * function: pcap_usage()
 * 
 ********************************************************************************/
static void pcap_usage(const char *prg_name) {
        DBG_LOG(MSG, MOD_DPI_TPROXY,"%s [--interface <name> | <pcap_file_name>]", prg_name);
} /* end of pcap_usage() */


/********************************************************************************
 * function: process_pcap_packet () (used for pcap mode)
 *
 * purpose: allocate memory, copy the packet data and add it to the list 
 ********************************************************************************/
int
process_pcap_packet(const struct pcap_pkthdr *pkthdr, unsigned char *data)
{
	struct smp_pkt *smp_pkt;
	struct smp_thread *th;
	packets_count++;

	//Take a copy of every packet and push into the list of the destination thread
	if ((smp_pkt = calloc(1, sizeof(struct smp_pkt) + pkthdr->caplen)) == NULL) {
		DBG_LOG(ERROR, MOD_DPI_TPROXY,"Can't calloc new packet");
		return -1;
	}
	smp_pkt->pkt.deng_data = &deng_data;
	smp_pkt->pkt.caplen = pkthdr->caplen;
	smp_pkt->pkt.ts = pkthdr->ts;
	smp_pkt->pkt.data = (unsigned char *)&smp_pkt[1];
	memcpy(smp_pkt->pkt.data, data, pkthdr->caplen);
	
	/* compute hashkey on this packet for thread dispatching */
	unsigned int hashkey = get_hashkey(data, pkthdr->caplen);

	/* get thread destination structure */
	th = &smp_thread[hashkey & (NUM_THREADS-1)];

	pthread_spin_lock(&th->lock);
	list_add_tail((struct list_entry *)smp_pkt,(struct list_entry *)&th->pkt_head);
	pthread_spin_unlock(&th->lock);

	return 0;

} /* end of process_pcap_packet ) */


/********************************************************************************
 * 	function: pcap_main ()
 *
 ********************************************************************************/
int
pcap_main (int argc, const char **argv)
{
        pcap_t *pcap;
        unsigned char *data;
        struct pcap_pkthdr pkthdr;
	struct smp_pkt *smp_pkt;
	int i;
        char errbuf[1024];
	int live_packet_capture = 0;
	int total_transactions = 0;
        
	if (argc < 2) {
                pcap_usage(argv[0]);
                return -1;
        }
	//Process live traffic from the given interface
	if (!strncmp(argv[1], "--interface",11) || !strcmp(argv[1], "-i")) {
		pcap =  pcap_open_live(argv[2], BUFSIZ,1 , -1, errbuf);
		if (!pcap)
		{
			DBG_LOG(ERROR, MOD_DPI_TPROXY,"Couldn't open interface %s: %s\n", argv[2], errbuf);
			exit(-1);
		}
		//Filter only port 80 traffic
		struct bpf_program fp;		/* The compiled filter expression */
		char filter_exp[] = "port 80";	/* The filter expression */
		if (pcap_compile(pcap, &fp, filter_exp, 0, PCAP_NETMASK_UNKNOWN) == -1) {
			DBG_LOG(ERROR, MOD_DPI_TPROXY,"Couldn't parse filter %s: %s", filter_exp, pcap_geterr(pcap));
			exit(-1);
		}
		if (pcap_setfilter(pcap, &fp) == -1) {
		 DBG_LOG(ERROR, MOD_DPI_TPROXY,"Couldn't install filter %s: %s", filter_exp, pcap_geterr(pcap));
		 exit(-1);
		}
		//Free the compiled filter expession
		pcap_freecode(&fp);
		live_packet_capture = 1;
	}
	//Read the given pcap file 
	else {
        	pcap = pcap_open_offline(argv[1], errbuf); // open offline trace
		if (!pcap) { /* pcap error ? */
			DBG_LOG(ERROR, MOD_DPI_TPROXY,"pcap_open: %s", errbuf);
			return -1;
		}
	}

	//For reading from a pcap file you don't need to be in the loop. Loop just till the end of pcap file
	if (!live_packet_capture) {
		while ((data = (unsigned char*)pcap_next(pcap, &pkthdr)))
			process_pcap_packet(&pkthdr, data);
	}
	//LIVE PACKET CAPTURE. Loop till the user gives CTRL-C
	else {
		while (application_not_stopped) {
			if ((data = (unsigned char *)pcap_next(pcap, &pkthdr)))
				process_pcap_packet(&pkthdr, data);
		}
	}

        /* Close pcap file*/
        pcap_close(pcap);

        return 0;

} /* end of pcap_main () */

#endif /* _STUB_ */


/* End of pcap_mode.c */
