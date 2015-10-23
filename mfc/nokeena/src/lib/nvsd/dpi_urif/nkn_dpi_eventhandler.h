/*
 * File: nkn_dpi_eventhandler.h
 *   
 * Author: Jeya ganesh babu
 *
 * (C) Copyright 2014 Juniper Networks, Inc.
 * All rights reserved.
 */
#ifndef NKN_DPI_EVENTHANDLER_H
#define NKN_DPI_EVENTHANDLER_H

#ifdef NFQUEUE_URI_FILTER
#include <linux/netfilter.h>
#include <libnetfilter_queue/libnetfilter_queue.h>
#endif
#include <dpi/modules/uevent.h>
#include <dpi/modules/uevent_hooks.h>
#include <dpi/modules/uapplication.h>
#include <dpi/protodef.h>

extern void event_handler_init(void);
extern void event_handler_cleanup(void);
int nkn_dpi_eventhandler_get_sess_status(void *ucnx,
					 uint32_t ip_src,
					 uint32_t ip_dst);
int nkn_dpi_eventhandler_link_pkt_to_session(void *ucnx,
					     struct nfq_q_handle *queue,
					     int pkt_id,
					     uint32_t ip_src,
					     uint32_t ip_dst,
					     uint64_t thread_id);

int nkn_dpi_eventhandler_link_pfpkt_to_session(void *ucnx,
					   void *ring,
					   int len,
					   unsigned char *buffer,
					   uint32_t ip_src,
					   uint32_t ip_dst,
					   uint64_t thread_id);
#endif /* NKN_DPI_EVENTHANDLER_H */
