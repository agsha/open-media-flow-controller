/* File : nkn_dpi_netfilter.h
 * Copyright (C) 2014 Juniper Networks, Inc. 
 * All rights reserved.
 */

/**
 * @file nkn_dpi_netfilter.h
 * @brief header for netfilter related functions
 * @author
 * @version 1.00
 * @date 2014-04-10
 */
#ifndef _NKN_DPI_NETFILTER_H
#define _NKN_DPI_NETFILTER_H

#include "nkn_defs.h"
#include "nkn_debug.h"
#include "nkn_lockstat.h"
#include "queue.h"

#define MAX_PKT_SIZE 9000

#define PROCESS_IN_NF_THREAD 1
#define MAX_NFQUEUE_THREADS 32

int nkn_nfqueue_init(void);

typedef struct nfqueue_dpi_xfer_st {
    struct nfq_q_handle *qh;
    int pkt_id;
    int pkt_len;
    uint32_t ip_src;
    uint32_t ip_dst;
    unsigned char *buffer;
    TAILQ_ENTRY(nfqueue_dpi_xfer_st) xfer_entries;
}nfqueue_dpi_xfer_t;

nfqueue_dpi_xfer_t *nkn_nf_dpi_get_entry(int64_t tid);

#endif /* _NKN_DPI_NETFILTER_H */

