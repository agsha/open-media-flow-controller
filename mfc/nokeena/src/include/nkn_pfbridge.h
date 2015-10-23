/* File : nkn_pfbridge.h
 * Copyright (C) 2014 Juniper Networks, Inc. 
 * All rights reserved.
 */

/**
 * @file nkn_pfbridge.h
 * @brief header for PF bridge related functions
 * @author
 * @version 1.00
 * @date 2014-04-10
 */
#ifndef _NKN_PFBRIDGE_H
#define _NKN_PFBRIDGE_H
#include "nkn_defs.h"
#include "nkn_debug.h"
#include "nkn_lockstat.h"
#include "nkn_namespace.h"

void nkn_pf_accept(unsigned char *buffer, int len, void *u);
void nkn_pf_reject(unsigned char *buffer, int len, void *u);
int nkn_pfbridge_init(void);
void nkn_pf_get_dst_mac(void);
#endif /* _NKN_PFBRIDGE_H */
