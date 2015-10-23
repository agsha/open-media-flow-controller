/*
 * (C) Copyright 2010 Juniper Networks, Inc
 *
 * This file contains Analytics Manager related defines
 *
 *
 */

#ifndef _NKN_AM_API_H
#define _NKN_AM_API_H

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/time.h>
#include <glib.h>
#include "nkn_defs.h"
#include "nkn_attr.h"
#include "queue.h"
#include "nkn_namespace.h"
#include "nkn_mm_mem_pool.h"
#include "nkn_sched_api.h"

#define NKN_AM_BIT_SET(value, flag) value |= flag;

#define NKN_AM_BIT_CLEAR(value, flag) value &= ~(flag)

#define NKN_AM_BIT_TEST(value, flag) (value & (flag) ? 1:0)

#define AM_MM_TIER_AO_COUNTER_INC(x, y, z) \
	switch(y) { \
	    case SATADiskMgr_provider: \
		AO_fetch_and_add1(&x##_##sata##_##z); \
		break; \
	    case SAS2DiskMgr_provider: \
		AO_fetch_and_add1(&x##_##sas##_##z); \
		break; \
	    case SolidStateMgr_provider: \
		AO_fetch_and_add1(&x##_##ssd##_##z); \
		break; \
	    default: \
		break; \
	}

#define AM_MM_TIER_AO_COUNTER_DEC(x, y, z) \
	switch(y) { \
	    case SATADiskMgr_provider: \
		AO_fetch_and_sub1(&x##_##sata##_##z); \
		break; \
	    case SAS2DiskMgr_provider: \
		AO_fetch_and_sub1(&x##_##sas##_##z); \
		break; \
	    case SolidStateMgr_provider: \
		AO_fetch_and_sub1(&x##_##ssd##_##z); \
		break; \
	    default: \
		break; \
	}

#define AM_MM_TIER_COUNTER_INC(x, y, z) \
	switch(y) { \
	    case SATADiskMgr_provider: \
		x##_##sata##_##z ++; \
		break; \
	    case SAS2DiskMgr_provider: \
		x##_##sas##_##z ++; \
		break; \
	    case SolidStateMgr_provider: \
		x##_##ssd##_##z ++; \
		break; \
	    default: \
		break; \
	}

#define AM_MM_TIER_COUNTER_DEC(x, y, z) \
	switch(y) { \
	    case SATADiskMgr_provider: \
		x##_##sata##_##z --; \
		break; \
	    case SAS2DiskMgr_provider: \
		x##_##sas##_##z --; \
		break; \
	    case SolidStateMgr_provider: \
		x##_##ssd##_##z --; \
		break; \
	    default: \
		break; \
	}

/* Global variables to find the hotness threshold for each provider*/
typedef uint64_t nkn_hot_t;
extern nkn_hot_t glob_am_prov_hot_threshold_5;
extern nkn_hot_t glob_am_prov_hot_threshold_6;
extern nkn_hot_t glob_am_prov_hot_threshold_1;
extern nkn_hot_t glob_am_prov_hot_threshold_20;
extern int glob_am_ingest_ignore_hotness_check;
extern time_t glob_am_hotness_interval_period;
extern nkn_mm_malloc_t *nkn_am_xfer_data_pool[NKN_SCHED_MAX_THREADS+1];
extern nkn_mm_malloc_t *nkn_am_xfer_ext_data_pool[NKN_SCHED_MAX_THREADS+1];

/* The primary key object is the identifier to the actual object stored
 * in the database. It contains a name id/string that identifies that
 * actual object and a provider-id that holds that object. */
#define NKN_AM_PK_LENGTH MAX_URI_SIZE+256
typedef struct AM_primary_key {
    char			*name;
    nkn_provider_type_t		provider_id;
    uint32_t			key_len;
} AM_pk_t;

typedef struct am_ext_info_t {
    int           num_bufs;
    nkn_buffer_t  *buffer[64];
}am_ext_info_t;


/* Flags for am_xfer_data and am_object_data */
/* Object is streaming */
#define AM_OBJ_TYPE_STREAMING       0x001
#define AM_NON_CHUNKED_STREAMING    0x002
/* Do not promote the object */
#define AM_INGEST_ONLY              0x004
/* Do a aggressive ingest */
#define AM_INGEST_AGGRESSIVE        0x008
/* Ignore hotness check */
#define AM_IGNORE_HOTNESS	    0x010
#define AM_INGEST_BYTE_RANGE	    0x020
#define AM_CIM_INGEST		    0x040
#define AM_INGEST_BYTE_SEEK	    0x080
#define AM_NEW_INGEST		    0x100
#define AM_INGEST_HP_QUEUE	    0x200
#define AM_COMP_OBJECT		    0x400
#define AM_PUSH_CONT_REQ	    0x800

typedef struct am_xfer_data_t {
    nkn_cod_t                   in_cod;
    namespace_token_t           ns_token;
    nkn_hot_t                   obj_hotness;
    void                        *attr_buf;
    void                        *host_hdr;
    void                        *proto_data;
    ip_addr_t                   client_ipv4v6;
    ip_addr_t                   origin_ipv4v6;
    int                         num_hits;
    uint16_t                    flags;
    uint16_t                    client_flags;
    uint16_t                    client_port;
    uint16_t                    origin_port;
    uint16_t                    encoding_type;
    uint16_t                    provider_id;
    uint8_t                     client_ip_family;
    am_ext_info_t               *ext_info;
    TAILQ_ENTRY(am_xfer_data_t) xfer_entries;
}am_xfer_data_t;

typedef struct am_object_data_t {
    namespace_token_t ns_token;
    uint32_t          attr_size:20;
    uint32_t          flags:12;
    uint16_t          encoding_type;
    ip_addr_t         client_ipv4v6;
    ip_addr_t         origin_ipv4v6;
    uint16_t          client_port;
    uint16_t          origin_port;
    uint8_t           client_ip_family;
    nkn_hot_t         obj_hotness;
    void              *host_hdr;
    void              *proto_data;
    nkn_cod_t         in_cod;
}am_object_data_t;

typedef struct AM_analytics_obj {
    AM_pk_t     pk;
    nkn_hot_t	hotness;
    time_t	interval_check_start;
    uint32_t	num_hits;
#define AM_POLICY_ONLY_LO_TIER	    0x00000001
#define AM_POLICY_ONLY_SATA	    0x00000002
#define AM_POLICY_ONLY_SAS	    0x00000004
#define AM_POLICY_ONLY_SSD	    0x00000008
#define AM_POLICY_ONLY_HI_TIER	    0x00000010
#define AM_POLICY_DONT_INGEST	    0x00000020
#define AM_POLICY_HP_QUEUE	    0x00000040
    uint16_t	policy;
#define AM_FLAG_PROVIDER_ADDED	    0x00000001
#define AM_FLAG_INGEST_CDT_ADDED    0x00000002
#define AM_FLAG_INGEST_ADDED	    0x00000004
#define AM_FLAG_INGEST_ERROR	    0x00000008
#define AM_FLAG_INGEST_PENDING	    0x00000010
#define AM_FLAG_INGEST_SKIP	    0x00000020
#define AM_FLAG_ADD_ENTRY	    0x00000040
#define AM_FLAG_DEL_ENTRY	    0x00000080
#define AM_FLAG_SET_INGEST_ERROR    0x00000100
#define AM_FLAG_SET_SMALL_OBJ_HOT   0x00000200
#define AM_FLAG_HP_ADDED            0x00000400
#define AM_FLAG_PUSH_INGEST         0x00000800
#define AM_FLAG_PUSH_INGEST_DEL     0x00001000
#define AM_FLAG_SET_INGEST_RETRY    0x00002000
#define AM_FLAG_SET_INGEST_COMPLETE 0x00004000
    uint16_t	flags;
    nkn_provider_type_t next_provider_id;
    am_object_data_t *in_object_data;
    int16_t	hp_id;
    int16_t	ingest_id;
    void        *push_ptr;
    TAILQ_ENTRY(AM_analytics_obj) ingest_cdt_entries;
    TAILQ_ENTRY(AM_analytics_obj) lru_entries;
    namespace_token_t ns_token;
} AM_obj_t;

#define AM_PROVIDER_DISABLED	0
#define AM_PROVIDER_ENABLED	1
typedef struct AM_provider_data_t {
    int		state;
    uint32_t	max_io_throughput;
    uint32_t	hotness_threshold;
}AM_provider_data_t;

extern int nkn_am_ingest_small_obj_enable;
extern int glob_am_small_obj_hit;
extern uint32_t nkn_am_ingest_small_obj_size;
extern uint32_t nkn_am_max_origin_entries;
extern uint32_t nkn_am_origin_tbl_timeout;
extern uint64_t g_am_promote_size_threshold;
extern AO_t glob_am_push_ingest_data_buffer_hold;
extern AO_t glob_am_push_ingest_attr_buffer_hold;
extern AO_t nkn_am_xfer_data_in_use[NKN_SCHED_MAX_THREADS+1];
extern AO_t nkn_am_ext_data_in_use[NKN_SCHED_MAX_THREADS+1];

/* This function is used to increment the number of hits on an object.
 * It triggers a bunch of calculations to eventually calculate the
 * 'hotness' factor of that object.
 */
nkn_hot_t AM_inc_num_hits(AM_pk_t *pk, uint32_t num_hits,
                          nkn_hot_t in_seed, void *buf,
			  am_object_data_t *in_client_data,
			  am_ext_info_t *am_ext_info);

void AM_init(void);
void AM_init_hotness(nkn_hot_t *out_hotness);
void AM_init_provider_thresholds(AM_pk_t *pk, uint32_t max_io_throughput,
				 int32_t hotness_threhold);
void AM_copy_client_data(nkn_provider_type_t ptype,
			    am_object_data_t *in_object_data,
			    am_object_data_t *object_data);
void AM_copy_client_data_to_xfer_data(nkn_provider_type_t ptype,
			 am_object_data_t *in_object_data,
			 am_xfer_data_t *am_xfer_data);
void AM_copy_client_data_from_xfer_data(nkn_provider_type_t ptype,
			 am_xfer_data_t *in_am_xfer_data,
			 am_object_data_t *out_object_data);
void AM_cleanup_client_data(nkn_provider_type_t ptype,
			    am_object_data_t *object_data);
void AM_cleanup_xfer_client_data(nkn_provider_type_t ptype,
			    am_xfer_data_t *am_xfer_data);
void AM_ingest_wakeup(void);
int  AM_delete_obj(nkn_cod_t in_cod, char *uri);
int  AM_delete_push_ingest(nkn_cod_t in_cod, void *attr_buf);
void AM_set_ingest_error(nkn_cod_t in_cod, int flags);
void AM_set_dont_ingest(nkn_cod_t in_cod);
void AM_change_obj_provider(nkn_cod_t in_cod, nkn_provider_type_t dst_ptype);
void AM_set_small_obj_hotness(nkn_cod_t in_cod, int hotness);
int  AM_get_next_evict_candidate_by_provider(nkn_provider_type_t ptype,
			    AM_pk_t *pk);
void AM_print_entries(FILE *fp);
void AM_decay_hotness(nkn_hot_t *in_seed, time_t cur_time);
void am_encode_hotness(nkn_hot_t *out_hotness, int32_t in_temp);
uint32_t am_decode_update_time(nkn_hot_t *in_hotness);
int32_t  am_decode_hotness(nkn_hot_t *in_hotness);
void am_encode_interval_check_start(nkn_hot_t *out_hotness, time_t object_ts);
uint32_t am_decode_interval_check_start(nkn_hot_t *in_hotness);
nkn_hot_t am_update_hotness(nkn_hot_t *in_hotness, int num_hits,
			    time_t object_ts, uint64_t hotness_inc, char *uri);
uint32_t am_get_hit_interval(void);
void nkn_am_counter_init(nkn_provider_type_t ptype, char *provider_str);

void nkn_am_log_error(const char *uriname, int errcode);

#define NKN_AM_PROVIDER_NONE (-1)
#define NKN_AM_SUB_PROVIDER_NONE (-1)

#endif /* _NKN_AM_API_H */
