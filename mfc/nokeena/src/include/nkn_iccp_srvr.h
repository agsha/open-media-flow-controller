/*
 * (C) Copyright 2010 Juniper Networks, Inc
 *
 * This file contains ICCP server related defines
 *
 * Author: Jeya ganesh babu
 *
 */
#ifndef _NKN_ICCP_SRVR_H
#define _NKN_ICCP_SRVR_H
#include "nkn_iccp.h"

/* Max Send retry */
#define MAX_ICCP_RETRIES 3

void * iccp_srvr_thrd(void *arg);
void iccp_srvr_init(void(*srvr_callback_func)(void *));
iccp_info_t *iccp_srvr_find_entry_cod(uint64_t cod);
iccp_info_t *iccp_srvr_find_all_entry_cod(uint64_t cod);
int iccp_srvr_find_dup_entry_cod(uint64_t cod,
					iccp_info_t *iccp_info);
void iccp_srvr_callback(void);
#endif /* _NKN_ICCP_SRVR_H */
