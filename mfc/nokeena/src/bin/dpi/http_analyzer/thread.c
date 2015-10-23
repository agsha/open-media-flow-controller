/*
 * File: thread.c
 *   
 * Author: Sivakumar Gurumurthy  & Ramanand Narayanan
 *
 *
 * (C) Copyright 2014 Juniper Networks, Inc.
 * All rights reserved.
 */

#ifdef _STUB_

/* Include Files */
#include <stdio.h>
#include "thread.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sched.h>

/* Include DPI/DPDK headers */
#include <rte_config.h>
#include <rte_ring.h>

#include "atomic_ops.h"
#include "nkn_stat.h"
#include "nkn_debug.h"

/* Local Marcros */
#define	LOGFILE_PREFIX	"/var/log/dpilog"
#define MAX_LOGFILE_SIZE 104857600 //1024 * 1024 * 100 = 100MB

/* Local Prototypes */
void rotate_log(int thread_number, thread_info *tinfo);


/**
 * List node initialization
 */
void init_list_head(struct list_entry *list) {
        list->next = list;
        list->prev = list;
}

/**
 * Test if list is empty
 */
int list_empty(struct list_entry *list) {
        return (list->next == list);
}

/**
 * Add a node at list tail
 */
void list_add_tail(struct list_entry *new,
                                 struct list_entry *head) {

        new->next = head;
        head->prev->next = new;
        new->prev = head->prev;
        head->prev = new;
}

/**
 * Remove a node from list
 */
void list_del(struct list_entry *entry) {
        entry->next->prev = entry->prev;
        entry->prev->next = entry->next;
        entry->next = NULL;
        entry->prev = NULL;
}

/**
 * Given the packet data and len calculate the hashkey for the packet based on
 * the source and destination ip
 **/
unsigned int
get_hashkey(unsigned char *data, unsigned int p_len)
{
    if (p_len >= sizeof(ueth_hdr_t) + sizeof(uip_hdr_t)) {
        ueth_hdr_t *eth;
        uip_hdr_t *ip;

        eth = (ueth_hdr_t *)data;
        if (ntohs(eth->h_proto) == 0x0800)
	{
            ip = (uip_hdr_t *)&eth[1];
            if (ip->protocol == 0x11) { // UDP
                unsigned int len = (ip->ihl << 2) + 8 ; // UDP size ...
                if (len >= len + sizeof (ueth_hdr_t) + 244) { // Whole DHCP Packet: 240 = minimal DHCP size. There is at least one option
                    // Message Type (3 octets) et une option END (1), ce qui fait 244 octets minimum
                    // Message Type (3 bytes) and an END option (1), that makes 244 bytes minimum
                    const unsigned char *dhcp = (const unsigned char *)ip + len;
                    if (dhcp[1] == 1 // type = ethernet
                            && dhcp[2] == 6  // Length 6
                            && dhcp[236] == 0x63 && dhcp[237] == 0x82 && dhcp[238] == 0x53 && dhcp[239] == 0x63) { // dhcp magic cookie)
                                return ntohl(*(const int*)&dhcp[30]);
                            }
                }
            }
            return ntohl(ip->saddr) ^ ntohl(ip->daddr);
        }
    }

    return 0;

} /* end of get_hashkey() */


/*
 * Worker thread processing function
 */
static void
write_dpilog_header(FILE *fp_log)
{
	/* Write the standard W3C headers */
	fprintf(fp_log, "#Version: 1.0\n");
	fprintf(fp_log, "#Date: \n");
	fprintf(fp_log, "#Fields: ");
	fprintf(fp_log, " %s %s %s %s %s \"%s\" %s %s \'%s\' %s %s %s \"%s\" %s %s %s %s \"%s\" \"%s\" %s %s\n",
			"cs(Host)", "cs-request", "sc(Status)",
			"sc-bytes-content",
			"sc(Age)", "cs(Range)", "cs(Pragma)", "cs(Cache-Control)",
			"sc(Etag)", "cs(Referer)", "s-ip",
			"cs(Authorization)", "cs(Cookie)", "cs(Content-Length)",
			"cs(Update)", "cs(Transfer-Encoding)", 
			"sc(Transfer-Encoding)", "sc(Date)",
			"sc(Expires)", "sc(Pragma)", "sc(Cache-Control)");
} /* end of write_dpilog_header() */


/**
 * Given the thread info structure, rotate the log file 
 **/
void
rotate_log(int thread_number, thread_info *tinfo)
{
	int i; 
	char sz_oldname [64];
	char sz_newname [64];
	struct stat stat_buf;


	/* First close the current log */
	fclose (tinfo->fp);

	/* Run through the archived files to rotate */
	for (i = 10; i > 0; i--)
	{
		/* First create the old file name */
		sprintf (sz_oldname, "%s_%d.%d", LOGFILE_PREFIX, thread_number, i);

		/* Check if the file exists */
		if (stat(sz_oldname, &stat_buf))
			 /* File does not exist, nothing to do */
			 continue;

		/* File exists, hence rename */
		if (10 == i) /* 10 is a special case */
		{
			/* since we have it, first remove the file */
			if (-1 == unlink(sz_oldname))
				DBG_LOG(ERROR, MOD_DPI_TPROXY,"error: failed to remove %s", sz_oldname);
		}
		else /* Now it is a question of renaming */
		{
			/* First create the new file name which is + 1 */
			sprintf (sz_newname, "%s_%d.%d", LOGFILE_PREFIX, thread_number, i+1);
			/* Now rename */
			if (-1 == rename (sz_oldname, sz_newname))
				DBG_LOG(ERROR, MOD_DPI_TPROXY,"error: failed to rename %s to %s", sz_oldname, sz_newname);
		}

	} /* end of for () */

	/* Now that rotation of the numbered files are over, rename main file */
	sprintf (sz_oldname, "%s_%d", LOGFILE_PREFIX, thread_number);
	sprintf (sz_newname, "%s_%d.1", LOGFILE_PREFIX, thread_number);
	if (-1 == rename (sz_oldname, sz_newname))
		DBG_LOG(ERROR, MOD_DPI_TPROXY,"error: failed to rename %s to %s", sz_oldname, sz_newname);

	/* Open the new file for logging and reset size */
	tinfo->fp = fopen(sz_oldname, "w");
	if (tinfo->fp == NULL) {
		DBG_LOG(ERROR, MOD_DPI_TPROXY,"error: failed to open file %s", sz_oldname);
		exit(1);
	}
	tinfo->bytes_written = 0;

	/* Write the standard W3C headers */
	write_dpilog_header(tinfo->fp);

	/* Done ... */
	return;
	
} /* end of rotate_log () */


/*
 * Worker thread processing function
 */
void*
smp_thread_routine(void *arg)
{
	int quit =0;
	struct smp_pkt *pkt_head = NULL;
	char fname[128];
	thread_info *tinfo = NULL; 
	struct smp_thread *th = NULL;
	struct stat stat_buf;

	//Set the thread id to be used by the ixe Engine
	th = (struct smp_thread *)arg;
	afc_set_cpuid(th->thread_number);

	DBG_LOG(MSG, MOD_DPI_TPROXY,"===> THREAD #%d IS STARTING", th->thread_number);

	/* Create the dpilog file name */
	snprintf(fname, sizeof(fname), "%s_%d", LOGFILE_PREFIX, th->thread_number); 
	tinfo = &thread_details[th->thread_number];

	/* Open the dpilog file */
	tinfo->fp = fopen(fname, "a+");
	if (tinfo->fp == NULL) {
		DBG_LOG(ERROR, MOD_DPI_TPROXY,"error: failed to open file %s", fname);
		exit(1);
	}

	/* Get the current size of the file */
	if (!fstat(fileno(tinfo->fp), &stat_buf))
	{
		tinfo->bytes_written = stat_buf.st_size;

		/* If file is empty meaning new file */
		if (0 == stat_buf.st_size)
			/* Write the file header */
			write_dpilog_header (tinfo->fp);
	}
	else
	{
		// some issue with the file pointer, should never happen
		tinfo->bytes_written = 0;
	}

	//Loop till there are no more packets to be processed by this thread
	while (!quit)
	{
#if 0 // COMMENTING OUT THIS LOGIC TO REPLACE WITH RING BUFFER LOGIC 
		pthread_spin_lock(&th->lock);
		if (!list_empty((struct list_entry *)&th->pkt_head)) {
			pkt_head = (struct smp_pkt *)th->pkt_head.entry.next;
                        list_del((struct list_entry *)pkt_head);
			pthread_spin_unlock(&th->lock);
			tinfo->nb_thread_pkts++;

			/* is it a dummy packet ? => means thread must exit */
                        quit = (pkt_head->pkt.data == NULL);
			if (!quit) {
		    		dpi_data_main(pkt_head->pkt.data, pkt_head->pkt.caplen, &pkt_head->pkt.ts, 0, pkt_head->pkt.deng_data, NULL);
			}
			free(pkt_head);
		}
		else {
			pthread_spin_unlock(&th->lock);
		}
#endif // 0 

		/* Dequeue the next packet if any */
		if (rte_ring_sc_dequeue(th->queue, (void **) &pkt_head) < 0) {
			/* Nothing to process hence continue */
			continue;
		}

		/* Send the packet to DPI engine */
		quit = (pkt_head->pkt.data == NULL);
		if (!quit) {
			dpi_data_main(pkt_head->pkt.data, pkt_head->pkt.caplen,
					&pkt_head->pkt.ts, 0, pkt_head->pkt.deng_data, NULL);
		}

		/* We are done, free the packet */
		free(pkt_head);

		/* Check the amount of log written */
		if (MAX_LOGFILE_SIZE <= tinfo->bytes_written)
			rotate_log (th->thread_number, tinfo);
	} /* while () */

	/* Prints the stats */
	DBG_LOG(MSG, MOD_DPI_TPROXY,"[%d]NUMBER OF PACKETS RECEIVED  %llu", th->thread_number,
							tinfo->nb_thread_pkts);
	DBG_LOG(MSG, MOD_DPI_TPROXY,"[%d]NUMBER OF HTTP REQ PROCESSED %llu", th->thread_number,
							tinfo->num_req);
	DBG_LOG(MSG, MOD_DPI_TPROXY,"[%d]NUMBER OF HTTP RESP PROCESSED %llu", th->thread_number,
							tinfo->num_resp);
	DBG_LOG(MSG, MOD_DPI_TPROXY,"[%d]NUMBER OF REQ PACKET END RECEIVED %llu", th->thread_number,
							tinfo->num_req_end);
	DBG_LOG(MSG, MOD_DPI_TPROXY,"[%d]NUMBER OF TRANSACTIONS PROCESSED %llu", th->thread_number,
							tinfo->counter);
	DBG_LOG(MSG, MOD_DPI_TPROXY,"[%d]NUMBER OF REQ PACKETS DROPPED %llu", th->thread_number,
							tinfo->no_peer_counter);

	DBG_LOG(MSG, MOD_DPI_TPROXY,"THREAD %d STOPPED", th->thread_number);

	/* Close the file */
	fclose(tinfo->fp);

	return NULL;

} /* end of void *smp_thread_routine() */

#endif /* _STUB_ */

/* End of thread.c */
