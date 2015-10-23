/*
 * (C) Copyright 2010 Juniper Networks, Inc
 *
 * This file contains code which implements the Analytics Manager
 *  object tables
 *
 * Author: Jeya ganesh babu
 *
 */

#include "nkn_am_api.h"
#include "nkn_am.h"
#include <stdint.h>
#include <stdio.h>
#include <glib.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include "nkn_mediamgr_api.h"
#include "nkn_debug.h"
#include "nkn_lockstat.h"

static GHashTable *nkn_am_hash_table = NULL;

uint32_t nkn_am_max_prov_entries   = 200000;
uint32_t nkn_am_max_origin_entries = 200000;
uint32_t nkn_am_memory_limit	   = 0;
uint32_t nkn_am_origin_tbl_timeout;

uint64_t glob_am_hash_tbl_create_cnt;
uint64_t glob_am_hash_tbl_delete_cnt;
uint64_t glob_am_tbl_total_cnt;
uint64_t glob_am_origin_entry_total_cnt;
uint64_t glob_am_hash_tbl_delete_not_found;

uint64_t glob_am_ssd_entry_create_cnt;
uint64_t glob_am_sas_entry_create_cnt;
uint64_t glob_am_sata_entry_create_cnt;

uint64_t glob_am_ssd_entry_delete_cnt;
uint64_t glob_am_sas_entry_delete_cnt;
uint64_t glob_am_sata_entry_delete_cnt;


static char *
s_create_key(AM_pk_t *pk, AM_pk_t *new_pk)
{
    char *key;
    int len = 0;
    int ret;

    if(pk->key_len)
	len = pk->key_len + 1;
    else
	len = strlen(pk->name) + 1;

    key = (char *) nkn_malloc_type (len, mod_am_tbl_malloc_key);
    if(!key) {
	DBG_LOG_MEM_UNAVAILABLE(MOD_AM);
	return NULL;
    }

    ret = snprintf(key, len, "%s", pk->name);
    if(ret < 0) {
	return NULL;
    }

    new_pk->name = key;
    new_pk->provider_id = pk->provider_id;
    new_pk->key_len = len;
    return key;
}

static AM_obj_t *
s_create_am_obj(void)
{
    AM_obj_t     *obj;

    obj = (AM_obj_t *) nkn_calloc_type (1, sizeof(AM_obj_t),
                                        mod_am_tbl_create_am_obj);
    if(!obj) {
        return NULL;
    }

    return obj;
}

static inline void
nkn_am_tbl_destroy_key(gpointer data)
{
    char *key = (char *) data;
    if(key) {
        free(key);
        key = NULL;
    }
}

static inline void
nkn_am_tbl_destroy_obj(AM_obj_t *objp)
{
    if(!objp)
        return;

    /* Remove Client related info */
    if(objp->in_object_data) {
	AM_cleanup_client_data(objp->pk.provider_id, objp->in_object_data);
	free(objp->in_object_data);
	objp->in_object_data = NULL;
    }

    free(objp);
    objp = NULL;
}

AM_obj_t *
nkn_am_tbl_create_entry(AM_pk_t *pk)
{
    char *key = NULL;
    AM_obj_t *objp = NULL;

    objp = s_create_am_obj();
    if(objp == NULL) {
        return NULL;
    }

    key = s_create_key(pk, &objp->pk);
    if(key == NULL) {
	nkn_am_tbl_destroy_obj(objp);
        return NULL;
    }

    return objp;
}

void
nkn_am_tbl_add_entry(AM_obj_t *objp)
{

    if(objp->pk.provider_id > NKN_MM_max_pci_providers)
	glob_am_origin_entry_total_cnt ++;

    g_hash_table_insert(nkn_am_hash_table, objp->pk.name, objp);
    glob_am_hash_tbl_create_cnt ++;
    glob_am_tbl_total_cnt ++;
    AM_MM_TIER_COUNTER_INC(glob_am, objp->pk.provider_id, entry_create_cnt);

}

AM_obj_t *
nkn_am_tbl_get(AM_pk_t *pk)
{
    AM_obj_t            *objp = NULL;
    objp = g_hash_table_lookup(nkn_am_hash_table, pk->name);
    return objp;
}

int
nkn_am_tbl_delete_entry(AM_pk_t *pk)
{
    AM_obj_t    *objp = NULL;

    objp = nkn_am_tbl_get(pk);
    if(objp == NULL) {
	DBG_LOG(WARNING, MOD_AM, "AM delete cannot find the specified obj: "
		"ptype = %d", pk->provider_id);
	glob_am_hash_tbl_delete_not_found ++;
        return NKN_AM_OBJ_NOT_FOUND;
    }

    if(!(objp->flags & AM_FLAG_INGEST_PENDING) && !(objp->push_ptr))
	nkn_am_tbl_delete_object(objp);
    return 0;
}

int
nkn_am_tbl_delete_object(AM_obj_t *objp)
{

    nkn_am_list_delete(objp);

    g_hash_table_remove(nkn_am_hash_table, objp->pk.name);

    /* Remove the entry from the list */
    glob_am_hash_tbl_delete_cnt ++;
    glob_am_tbl_total_cnt --;
    AM_MM_TIER_COUNTER_INC(glob_am, objp->pk.provider_id, entry_delete_cnt);

    if(objp->pk.provider_id > NKN_MM_max_pci_providers)
	glob_am_origin_entry_total_cnt --;


    /* Remove the object and key */
    nkn_am_tbl_destroy_key(objp->pk.name);
    nkn_am_tbl_destroy_obj(objp);

    return 0;
}

void
nkn_am_tbl_init()
{

    nkn_am_hash_table = g_hash_table_new(g_str_hash, g_str_equal);
    return;
}
