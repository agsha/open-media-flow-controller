/**
  * @file   ds_hash_table_mgmt.c
  * @author Karthik/Jegadish
  * @date   2:33 PM 3/21/2012
  *
  * @brief  modules to manage the hashs table functions. Most of the code is
  re-used from the MFP sess-id management modules, but changed here to handle
  non-integer keys.
  
 */

#include <stdio.h>
#include <pthread.h>
#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include <cprops/trie.h>

#include "nkn_memalloc.h"
#include "ds_hash_table_mgmt.h"
#include "nkn_assert.h"            

//static GHashTable *ds_hash_table;
//static pthread_mutex_t hash_table_lock;
uint64_t glob_ds_hash_table_mgmt_entries;

int32_t ds_hash_table_mgmt_init(hash_tbl_props_t* htp){
    //    GHashTable *ds_hash_table;
    htp->htable = g_hash_table_new(g_str_hash, g_str_equal);
    pthread_rwlock_init(&htp->lock, NULL);
    return 0;
}

int32_t ds_hash_table_mgmt_deinit(hash_tbl_props_t *htp) {
    if (htp) {
	if (htp->htable) {
	    g_hash_table_destroy(htp->htable);
	}
    }
    return 0;
}

int32_t ds_hash_table_mgmt_add(void* val, char *key, hash_tbl_props_t* htp){
    int32_t rv;
    ds_hash_table_t *obj, *existing_obj;
    GHashTable *ds_hash_table = htp->htable;
    rv = 0;

    existing_obj = (ds_hash_table_t *)g_hash_table_lookup(ds_hash_table, key);
    if (existing_obj != NULL) {
	rv = 1;
    } else {
	obj = (ds_hash_table_t*)\
	    nkn_calloc_type(1, sizeof(ds_hash_table_t), mod_ds_hash_table_mgmt_add);
	obj->val_ptr = val;
	pthread_mutex_init(&obj->lock, NULL);
	g_hash_table_insert(ds_hash_table, strdup(key), obj);
	glob_ds_hash_table_mgmt_entries++;
    }
    return rv;
}

int32_t ds_hash_table_mgmt_alter(void* val, char *key, hash_tbl_props_t* htp){
    int32_t rv;
    ds_hash_table_t *obj;
    GHashTable *ds_hash_table = htp->htable;
    rv = ds_hash_table_mgmt_add(val, key, htp);
    if(rv){
	obj = (ds_hash_table_t *)g_hash_table_lookup(ds_hash_table, key);
        obj->val_ptr = val;
        g_hash_table_insert(ds_hash_table, strdup(key), obj);
    }
    return 0;
}



ds_hash_table_t *ds_hash_table_mgmt_find(char *key,  hash_tbl_props_t* htp)
{
    ds_hash_table_t *obj = NULL;
    GHashTable *ds_hash_table = htp->htable;
    obj = g_hash_table_lookup(ds_hash_table, key);

    return obj;
}

int32_t ds_hash_table_mgmt_remove(char *key,hash_tbl_props_t* htp){
    ds_hash_table_t *obj = NULL;
    GHashTable *ds_hash_table = htp->htable;
    obj = g_hash_table_lookup(ds_hash_table, key);
    if (!obj) {
	return 0;
    }
    g_hash_table_remove(ds_hash_table, key);
    free(key);
    glob_ds_hash_table_mgmt_entries--;
    return 0;
}


void  ds_hash_table_mgmt_cleanup_state(ds_hash_table_t *p)
{
    if (p) {
	free(p);
    }
}

static void *
trie_copy_func(void *nd)
{
    ds_hash_table_t *obj = NULL;

    obj = (ds_hash_table_t*)						\
	nkn_calloc_type(1, sizeof(ds_hash_table_t), 
			mod_ds_hash_table_mgmt_add);
    obj->val_ptr = nd;
    pthread_mutex_init(&obj->lock, NULL);

    return obj;
}

static void
trie_destruct_func(void *nd)
{
    ds_hash_table_t *obj = NULL;

    obj = (ds_hash_table_t *)nd;
    pthread_mutex_destroy(&obj->lock);
    free(obj);

    return;
}

int32_t ds_trie_mgmt_init(hash_tbl_props_t* htp){

    htp->trie = cp_trie_create_trie(
		(COLLECTION_MODE_NOSYNC|COLLECTION_MODE_COPY|
		 COLLECTION_MODE_DEEP),
		trie_copy_func, trie_destruct_func);
    pthread_rwlock_init(&htp->lock, NULL);
    return 0;
}

int32_t ds_trie_mgmt_add(void* val, char *key, 
			 hash_tbl_props_t* htp) {
    void *existing_obj = NULL;

    existing_obj = (void *)cp_trie_exact_match(htp->trie, key);
    if (existing_obj) {
	return -1;
    }
    cp_trie_add(htp->trie, strdup(key), val);
    return 0;
}

ds_hash_table_t *ds_trie_mgmt_find(char *key, 
			 hash_tbl_props_t* htp) {

    ds_hash_table_t *obj = NULL;
    cp_trie_prefix_match(htp->trie, key, (void **)&obj);
    if (!obj) {
	return NULL;
    }
    return obj;
}

int32_t ds_trie_mgmt_remove(char *key, hash_tbl_props_t* htp){

    ds_hash_table_t *obj = NULL;
    cp_trie_prefix_match(htp->trie, key, (void **) &obj);
    if (!obj) {
	return -1;
    }
    cp_trie_remove(htp->trie, key, (void **)&obj);
    glob_ds_hash_table_mgmt_entries--;
    return 0;
}


int32_t ds_cont_read_lock(hash_tbl_props_t* ds_cn) {

    return pthread_rwlock_rdlock(&ds_cn->lock);
}

int32_t ds_cont_read_unlock(hash_tbl_props_t* ds_cn) {

    return pthread_rwlock_unlock(&ds_cn->lock);
}

int32_t ds_cont_write_lock(hash_tbl_props_t* ds_cn) {

    return pthread_rwlock_wrlock(&ds_cn->lock);
}

int32_t ds_cont_write_unlock(hash_tbl_props_t* ds_cn) {

    return pthread_rwlock_unlock(&ds_cn->lock);
}

