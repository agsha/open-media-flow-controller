/*
 * (C) Copyright 2010 Juniper Networks, Inc
 *
 * This file contains Analytics manager private declarations
 *
 * Author: Jeya ganesh babu
 *
 */

#ifndef _NKN_AM_H
#define _NKN_AM_H

#include <glib.h>
#include "nkn_am_api.h"
#include "nkn_cfg_params.h"
#include "nkn_debug.h"
#include <pthread.h>
#include "nkn_lockstat.h"

#define AM_DEF_HOT_THRESHOLD 3
#define AM_MAX_SMALL_OBJ_HIT 6

#define AM_INGEST_POLICY_AUTO	    1
#define AM_INGEST_POLICY_NAMESPACE  2
#define AM_INGEST_POLICY_COMBO	    3
#define AM_MAX_SHORT_VAL    0xFFFF

/* 500MB memory Limit */
#define AM_DEFAULT_MEMORY_LIMIT (500 * 1024 * 1024)
#define MAX_AM_INGEST_QUEUE_ENTRIES 1000
extern uint32_t am_max_entries;

extern uint64_t glob_am_tbl_total_cnt;
extern uint64_t glob_am_origin_entry_total_cnt;
extern uint64_t glob_malloc_mod_am_tbl_malloc_key;
extern uint64_t glob_malloc_mod_am_object_data;
extern uint64_t glob_malloc_mod_am_tbl_create_am_obj;
extern uint64_t glob_malloc_mod_normalize_pcon_pcon_t;
extern uint64_t glob_malloc_mod_pseudo_con_host_hdr;
extern uint64_t glob_malloc_mod_http_heap_ext;
extern uint32_t nkn_am_max_prov_entries;

extern AM_provider_data_t nkn_am_provider_data[NKN_MM_MAX_CACHE_PROVIDERS];
extern int hotness_scale[NKN_MM_MAX_CACHE_PROVIDERS];

void nkn_am_ingest_init(void);
void am_encode_update_time(nkn_hot_t *out_hotness, time_t object_ts);

AM_obj_t * nkn_am_list_get_hottest(AM_pk_t *pk);
void nkn_am_update_ingest_list(AM_obj_t *objp);
void nkn_am_list_remove_ingest_entry(AM_obj_t *pobj);
void nkn_am_list_add_ingest_cdt_entry(AM_obj_t *pobj);
void nkn_am_list_remove_ingest_cdt_entry(AM_obj_t *pobj);
void nkn_am_list_add_provider_entry(AM_obj_t *pobj);
void nkn_am_list_remove_provider_entry(AM_obj_t *pobj);
void nkn_am_list_update_provider_entry(AM_obj_t *objp);
void nkn_am_list_delete(AM_obj_t *pobj);
void nkn_am_list_init(void);
void nkn_am_list_remove_hp_ingest_entry(AM_obj_t *objp);
void nkn_am_ingest_list_delete(AM_obj_t *objp);

AM_obj_t * nkn_am_tbl_create_entry(AM_pk_t *pk);
AM_obj_t * nkn_am_tbl_get(AM_pk_t *pk);
int nkn_am_tbl_delete_entry(AM_pk_t *pk);
int nkn_am_tbl_delete_object(AM_obj_t *objp);
void nkn_am_tbl_init(void);
void nkn_am_tbl_age_out_entry(nkn_provider_type_t ptype);
void nkn_am_tbl_add_entry(AM_obj_t *objp);
AM_obj_t * nkn_am_list_get_hp_ingest_entry(AM_pk_t *pk);

#endif /* _NKN_AM_H */
