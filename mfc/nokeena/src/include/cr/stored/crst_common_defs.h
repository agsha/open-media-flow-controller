/**
 * @file   crst_common_defs.h
 * @author Sunil Mukundan <smukundan@cmbu-build03>
 * @date   Tue Apr 10 17:42:29 2012
 * 
 * @brief  shared definitions across crst modules
 * 
 * 
 */
#ifndef _CRST_COMMON_DEFS_
#define _CRST_COMMON_DEFS_

#include <sys/queue.h>
#include "cr_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

#if 0 //keeps emacs happy
}
#endif

typedef enum tag_store_type {
    CRST_STORE_TYPE_CGRP,
    CRST_STORE_TYPE_CE,
    CRST_STORE_TYPE_DOM,
    CRST_STORE_TYPE_GEO,
    CRST_STORE_TYPE_MAX
} crst_store_type_t;

typedef struct tag_crst_cfg {
    char *geodb_file_name;
} crst_cfg_t;

typedef enum tag_cache_entity_addr_type {
    ce_addr_type_none = 0,
    ce_addr_type_ipv4,
    ce_addr_type_ipv6,
    ce_addr_type_cname,
    ce_addr_type_ns,
    ce_addr_type_max
} cache_entity_addr_type_t;

typedef enum tag_routing_type {
    ROUTING_TYPE_UNDEF,
    ROUTING_TYPE_STATIC,
    ROUTING_TYPE_RR,
    ROUTING_TYPE_GEOLOAD,
    ROUTING_TYPE_MAX
} routing_type_t;

typedef struct tag_ip_addr_range_list_elm {
    ip_addr_range_t addr_range;
    TAILQ_ENTRY(tag_ip_addr_range_list_elm) entries;
} ip_addr_range_list_elm_t;

TAILQ_HEAD(ip_addr_range_list_t, tag_ip_addr_range_list_elm);

typedef struct tag_static_routing {
    struct ip_addr_range_list_t addr_list;
} static_routing_t;

typedef struct tag_rr_routing {
} rr_routing_t;

typedef struct tag_geo_load_routing {
    double geo_weighting;
    double load_weighting;
} geo_load_routing_t;

typedef struct tag_routing_policy {
    routing_type_t type;
    union {
	static_routing_t sr;
	rr_routing_t rr;
	geo_load_routing_t glr;
    }p;
} routing_policy_t;

#ifdef __cplusplus
}
#endif 

#endif //_CRST_COMMON_DEFS_
