/*
 *
 * Filename:  nkn_vpe_ism_read_api.h
 * Date:      2010/05/13
 * Module:    ismc xml read API implementation
 *
 * (C) Copyright 2020 Juniper Networks, Inc.
 * All rights reserved.
 *
 *
 */

#ifndef _ISMC_READ_API_
#define _ISMC_READ_API_

#include <stdio.h>
#include <inttypes.h>
#include <sys/queue.h>

#include "nkn_vpe_mp4_parser.h"
#include "nkn_vpe_ism_read_api.h"

#ifdef __cplusplus 
extern "C" {
#endif

    typedef struct tag_ismc_cust_attr {
	char *name;
	char *val;
	TAILQ_ENTRY(tag_ismc_cust_attr) entries;
    }ismc_cust_attr_t;

    typedef struct tag_ismc_av_attr {
	uint32_t n_profiles;
	uint32_t n_cust_attrs[MAX_TRACKS];
	uint32_t bitrates[MAX_TRACKS];
	TAILQ_HEAD(, tag_ismc_cust_attr) cust_attr_head[MAX_TRACKS];
    }ismc_av_attr_t;
  
    typedef struct tag_ismc_publish_stream_info {
	ismc_av_attr_t *attr[MAX_TRACKS];
    }ismc_publish_stream_info_t;

    int32_t ismc_read_profile_map_from_file(char *ismc_name,
					    ismc_publish_stream_info_t
					    **out, io_handlers_t *iocb); 
    ismc_publish_stream_info_t* ismc_read_profile_map(xml_read_ctx_t *ctx);
    void ismc_cleanup_profile_map(ismc_publish_stream_info_t *psi);

#ifdef __cplusplus
}
#endif

#endif
