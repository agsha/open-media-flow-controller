/*
 *
 * Filename:  rtp_packetizer_common.c
 * Date:      2009/03/28
 * Module:    implementation of commonly used modules
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

#ifndef _RTP_COMMON_
#define _RTP_COMMON_

#include <inttypes.h>
#include <stdio.h>

#define SAFE_FREE(PTR) free(PTR);\
                       PTR = NULL;
#ifdef __cplusplus
extern "C"{
#endif

    void bin2base64(uint8_t*,uint8_t,char *);

#ifdef __cplusplus
}
#endif

#endif
