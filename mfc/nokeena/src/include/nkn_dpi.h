/* File : nkn_dpi.h
 * Copyright (C) 2014 Juniper Networks, Inc. 
 * All rights reserved.
 */

/**
 * @file nkn_dpi.h
 * @brief header for dpi related functions
 * @author
 * @version 1.00
 * @date 2014-04-10
 */
#ifndef _NKN_DPI_H
#define _NKN_DPI_H

#include "nkn_defs.h"
#include "nkn_debug.h"
#include "nkn_lockstat.h"
#include "queue.h"
#include "nkn_dpi_netfilter.h"
#include "nkn_memalloc.h"

void nkn_dpi_process_pf_packet(unsigned char *buffer, void *user_data, int len,
			       uint32_t ip_src, uint32_t ip_dst, uint64_t thread_id);

void nkn_dpi_process_packet(nfqueue_dpi_xfer_t *xfer_data, uint64_t thread_id);

void nkn_dpi_event_handle_init(uint64_t thread_id);

void nkn_dpi_pf_set_cpu_id(uint64_t thread_id);

int nkn_dpi_init(void);

extern int nkn_cfg_dpi_uri_mode;

extern uint32_t glob_dpi_uri_filter_size;

#endif /* _NKN_DPI_H */

