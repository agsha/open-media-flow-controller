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

static GHashTable *AM_origin_hash_table = NULL;
nkn_mutex_t AM_origin_hash_table_mutex;
TAILQ_HEAD(nkn_origin_tbl, AM_analytics_obj) nkn_am_origin_tbl_q;

uint64_t glob_am_origin_hash_tbl_create_count = 0;
uint64_t glob_am_origin_hash_tbl_delete_count = 0;
uint64_t glob_am_origin_hash_tbl_total_count = 0;
uint64_t glob_am_origin_hash_tbl_update_count = 0;

uint64_t glob_am_origin_tbl_age_out_cnt = 0;
uint64_t glob_am_origin_tbl_age_out_err = 0;
uint64_t glob_am_origin_tbl_age_out_failed = 0;
uint64_t glob_am_origin_tbl_age_out_failed1 = 0;
uint64_t glob_am_origin_tbl_age_out_failed2 = 0;
uint64_t glob_am_origin_tbl_mem = 0;
extern uint64_t glob_am_clr_not_ingested_cnt;

int am_origin_tbl_print_count = 0;
uint32_t am_max_origin_entries=200000;
uint32_t am_origin_tbl_timeout=0;

void am_origin_tbl_cleanup(void);
/* Builds a key into the hash table. The length of the string 'key' should be 'len'.
   NOTE: Memory is allocated by the caller. */
static int
s_origin_build_key(AM_pk_t *pk, char *key, int len)
{
    if(!key) {
	return -1;
    }
    assert(pk->name != NULL);
    if((pk->provider_id > 0) && (pk->sub_provider_id >= 0)) {
        snprintf(key, len-1, "%s_%d_%d", pk->name, pk->provider_id,
                 pk->sub_provider_id);
        key[len-1] = '\0';
    } else if(pk->provider_id > 0) {
        snprintf(key, len-1, "%s_%d", pk->name, pk->sub_provider_id);
        key[len-1] = '\0';
    } else {
        snprintf(key, len-1, "%s", pk->name);
        key[len-1] = '\0';
    }
    return 0;
}

static char *
s_create_key(AM_pk_t *pk)
{
    char *key;
    int len = AM_PROVIDER_KEY_LEN;
    int ret;

    if(pk->key_len)
	len += pk->key_len;
    else
	len += MAX_URI_SIZE;

    key = (char *) nkn_malloc_type (len, mod_am_tbl_malloc_key);
    if(!key) {
	DBG_LOG_MEM_UNAVAILABLE(MOD_AM);
	return NULL;
    }

    ret = s_origin_build_key(pk, key, len);
    if(ret < 0) {
	return NULL;
    }
    return key;
}

static AM_obj_t *
s_create_am_origin_obj(AM_obj_t *val, AM_pk_t *pk __attribute__((unused)))
{
    AM_obj_t     *obj;

    obj = (AM_obj_t *) nkn_calloc_type (1, sizeof(AM_obj_t),
                                        mod_am_origin_tbl_create_am_obj);
    if(!obj) {
        return NULL;
    }

    AM_copy_am_obj(val, obj);
    obj->hotness_list_ptr = NULL;

    return obj;
}


void
AM_origin_tbl_destroy_key(gpointer data)
{
    char *key = (char *) data;
    if(key) {
        free(key);
        key = NULL;
    }
}

void
AM_origin_tbl_destroy_obj(AM_obj_t *amp)
{
    if(!amp)
        return;

    if(amp->pk.name) {
        free(amp->pk.name);
        amp->pk.name = NULL;
    }
    free(amp);
    amp = NULL;
}

void
AM_origin_tbl_init()
{
    int ret = -1;
    TAILQ_INIT(&nkn_am_origin_tbl_q);

    AM_origin_hash_table = g_hash_table_new_full(g_str_hash, g_str_equal,
                                          (GDestroyNotify)AM_origin_tbl_destroy_key,
                                          (GDestroyNotify)AM_origin_tbl_destroy_obj);
    assert(AM_origin_hash_table);

    ret = NKN_MUTEX_INIT(&AM_origin_hash_table_mutex, NULL,
			    "am-origin-hash-tbl-mutex");
    if(ret < 0) {
	//debug
	printf("\n AM hash table not inited.");
    }
    return;
}

/*
 * This function creates memory for the 'key' and 'value' pair
 * to be stored in the AM hash table.
 * NOTE: All memory that the AM hash table needs is allocated
 * here. The caller function does not need to allocate memory.
 * The values passed in are copied.
 * If the trigger policy is not being used, the change bitmap
 * field should be set to 0 so that no changes are made to the
 * trigger policy.
 * NOTE: This new entry is NOT inserted into the HOTNESS list here.
 * The update_analytics function is used for that purpose.
 * NOTE: The CALLER must hold the mutex.(am_origin_hash_tbl_mutex)
 */
static AM_obj_t *
AM_origin_tbl_insert_new(AM_obj_type_t type __attribute__((unused)),
			 AM_pk_t *pk, AM_obj_t *objp)
{
    char     *key = NULL;
    AM_obj_t *stored_val = NULL;

    assert(objp);

    key = s_create_key(pk);
    if(key == NULL) {
        return NULL;
    }

    objp->pk.name = pk->name;
    objp->pk.type = pk->type;
    objp->pk.provider_id = pk->provider_id;
    objp->pk.sub_provider_id = pk->sub_provider_id;
    objp->promote_errors = 0;

    stored_val = s_create_am_origin_obj(objp, pk);
    if(stored_val == NULL) {
        return NULL;
    }

    NKN_MUTEX_LOCK(&AM_origin_hash_table_mutex);
    TAILQ_INSERT_TAIL(&nkn_am_origin_tbl_q, stored_val, entries);
    NKN_MUTEX_UNLOCK(&AM_origin_hash_table_mutex);

    /* Age out an entry first before inserting a new one
     because this entry may be deleted even before we use it.*/
    if(glob_am_origin_hash_tbl_total_count > am_max_origin_entries) {
	AM_origin_tbl_age_out(pk->provider_id, pk->sub_provider_id);
    }

    NKN_MUTEX_LOCK(&AM_origin_hash_table_mutex);
    glob_am_origin_tbl_mem += 24;
    g_hash_table_insert(AM_origin_hash_table, key, stored_val);
    glob_am_origin_hash_tbl_create_count ++;
    glob_am_origin_hash_tbl_total_count ++;
    NKN_MUTEX_UNLOCK(&AM_origin_hash_table_mutex);

    return stored_val;
}

/* This function updates an entry in the table. It also
 * inserts an entry into the 'hotness' list based on the
 * 'hotness' value of an object.
 * NOTE: This function needs to be called while holding the mutex.
 * NOTE: The objp arg better be a valid piece of memory.
 * */
AM_obj_t *
AM_origin_tbl_update_hits(AM_obj_type_t type, AM_pk_t *pk, AM_obj_t *objp, int new)
{
    assert(objp);

    if(new) {
	objp = AM_origin_tbl_insert_new(type, pk, objp);
	glob_am_origin_hash_tbl_update_count ++;
    }
    return objp;
}

int
AM_origin_tbl_video_promoted(AM_pk_t *pk)
{
    int ret = 0;
    AM_obj_t *objp = NULL;

    if(!pk)
	return -1;

    NKN_MUTEX_LOCK(&AM_origin_hash_table_mutex);
    objp = AM_origin_tbl_get(pk->type, pk);
    if(!objp) {
	NKN_MUTEX_UNLOCK(&AM_origin_hash_table_mutex);
	return -1;
    }
    //CHECK: optimize the promoted field to one flag.
    objp->promoted[pk->provider_id] = 1;
    NKN_MUTEX_UNLOCK(&AM_origin_hash_table_mutex);
    return ret;
}


int
AM_origin_tbl_set_promote_error(AM_pk_t *pk)
{
    int ret = 0;
    AM_obj_t *objp = NULL;

    if(!pk)
	return -1;

    NKN_MUTEX_LOCK(&AM_origin_hash_table_mutex);
    objp = AM_origin_tbl_get(pk->type, pk);
    if(!objp) {
	NKN_MUTEX_UNLOCK(&AM_origin_hash_table_mutex);
	return -1;
    }

    objp->promoted[pk->provider_id] = 0;
    objp->promote_pending = 0;
    objp->ingest_promote_error = 1;
    objp->promote_errors ++;
    /* Remove Client related info */
    AM_cleanup_client_data(pk->provider_id, objp->in_client_data.proto_data);
    objp->in_client_data.proto_data = NULL;

    NKN_MUTEX_UNLOCK(&AM_origin_hash_table_mutex);
    return ret;
}

int
AM_origin_tbl_set_dont_ingest(AM_pk_t *pk)
{
    int ret = 0;
    AM_obj_t *objp = NULL;

    if(!pk)
	return -1;

    NKN_MUTEX_LOCK(&AM_origin_hash_table_mutex);
    objp = AM_origin_tbl_get(pk->type, pk);
    if(!objp) {
	NKN_MUTEX_UNLOCK(&AM_origin_hash_table_mutex);
	return -1;
    }
    objp->dont_ingest = 1;
    NKN_MUTEX_UNLOCK(&AM_origin_hash_table_mutex);
    return ret;
}

int
AM_origin_tbl_clr_ingest_error(AM_pk_t *pk)
{
    int ret = 0;
    AM_obj_t *objp = NULL;

    if(!pk)
	return -1;

    NKN_MUTEX_LOCK(&AM_origin_hash_table_mutex);
    objp = AM_origin_tbl_get(pk->type, pk);
    if(!objp) {
	NKN_MUTEX_UNLOCK(&AM_origin_hash_table_mutex);
	return -1;
    }
    if(objp->ingest_promote_error) {
	objp->ingest_promote_error = 0;
	glob_am_clr_not_ingested_cnt ++;
    }
    NKN_MUTEX_UNLOCK(&AM_origin_hash_table_mutex);
    return ret;
}

int
AM_origin_tbl_is_video_promote_pending(AM_pk_t *pk)
{
    AM_obj_t objp;
    int ret = -1;

    objp.pk.name = NULL;
    ret = AM_origin_tbl_get_copy(pk->type, pk, &objp);
    if(ret < 0) {
	return -1;
    }
    if(objp.pk.name) {
	free(objp.pk.name);
    }
    if(objp.promote_pending == 1) {
	return 0;
    }
    return -1;
}

int
AM_origin_tbl_set_video_promote_pending(AM_pk_t *pk)
{
    int ret = 0;
    AM_obj_t *objp = NULL;

    if(!pk)
	return -1;

    NKN_MUTEX_LOCK(&AM_origin_hash_table_mutex);
    objp = AM_origin_tbl_get(pk->type, pk);
    if(!objp) {
	NKN_MUTEX_UNLOCK(&AM_origin_hash_table_mutex);
	/* NOTE: we need to release the lock here and re-aquire.
	   It is inefficient but ok. */
	AM_inc_num_hits(pk, 0, 0, 0, NULL, NULL);
	NKN_MUTEX_LOCK(&AM_origin_hash_table_mutex);
	objp = AM_origin_tbl_get(pk->type, pk);
	if(objp) {
	    objp->promote_pending = 1;
	}
	NKN_MUTEX_UNLOCK(&AM_origin_hash_table_mutex);
	return 0;
    }
    //CHECK: optimize the promoted field to one flag.
    objp->promote_pending = 1;
    NKN_MUTEX_UNLOCK(&AM_origin_hash_table_mutex);
    return ret;
}

/*
 * This function removes a key-value pair from the hash table.
 * NOTE: This function deletes the memory allocated for the key
 * and value.
 * NOTE: NOTE: the mutex (AM_origin_hash_table_mutex) MUST be held by the
 * caller of this function.
 */
int
AM_origin_tbl_delete(AM_obj_type_t type, AM_pk_t *pk)
{
    int                 ret_val = 0;
    AM_obj_t            *objp = NULL;
    int                 len = MAX_URI_SIZE + 128;
    char                keystr[len];
    char                *key = &keystr[0];

    NKN_MUTEX_LOCK(&AM_origin_hash_table_mutex);
    objp = AM_origin_tbl_get(type, pk);
    if(objp == NULL) {
	DBG_LOG(WARNING, MOD_AM, "AM delete cannot find the specified obj: "
		"type = %d, ptype = %d, sub_ptype = %d",
		type, pk->provider_id, pk->sub_provider_id);
	NKN_MUTEX_UNLOCK(&AM_origin_hash_table_mutex);
        return -1;
    }
    NKN_MUTEX_UNLOCK(&AM_origin_hash_table_mutex);

    ret_val = s_origin_build_key(pk, key, len);
    if(ret_val < 0) {
	DBG_LOG(SEVERE, MOD_AM, "AM delete cannot allocate the key: "
		"type = %d, ptype = %d, sub_ptype = %d",
		type, pk->provider_id, pk->sub_provider_id);
        return -1;
    }

    NKN_MUTEX_LOCK(&AM_origin_hash_table_mutex);

    /* Remove Client related info */
    AM_cleanup_client_data(pk->provider_id, objp->in_client_data.proto_data);
    objp->in_client_data.proto_data = NULL;

    AM_hotness_list_delete(type, objp);

    /* Remove the entry from the list */
    glob_am_origin_hash_tbl_delete_count ++;
    glob_am_origin_hash_tbl_total_count --;

    glob_am_origin_tbl_mem -= 24;
    TAILQ_REMOVE(&nkn_am_origin_tbl_q, objp, entries);
    g_hash_table_remove(AM_origin_hash_table, key);
    NKN_MUTEX_UNLOCK(&AM_origin_hash_table_mutex);
    return ret_val;
}

/* This function takes out the last object from the AM table.
 NOTE: the mutex (AM_origin_hash_table_mutex) MUST be held by the
 CALLER of this function. */
void
AM_origin_tbl_age_out(nkn_provider_type_t ptype __attribute__((unused)),
		      uint8_t sptype __attribute__((unused)))
{
    AM_obj_t *objp;
    AM_obj_t *found_objp;
    int      i = 0;
    int      ret = -1;

    /* Look through the first 100 entries in the origin q and see
       if you can find anything to evict. I don't want to go through
       each of the possible 100,000 entries. Note that I am deleting
       these entries in the q if they are not valid. They may not be
       valid if the objects have been deleted in the database.*/
    for (i = 0; i < 100; i++) {
	objp = NULL;
	NKN_MUTEX_LOCK(&AM_origin_hash_table_mutex);
	objp = TAILQ_FIRST(&nkn_am_origin_tbl_q);
	if (!objp) {
	    glob_am_origin_tbl_age_out_failed ++;
	    NKN_MUTEX_UNLOCK(&AM_origin_hash_table_mutex);
	    return;
	}

	if (!objp->pk.name ||
	    (objp->pk.provider_id >= NKN_MM_MAX_CACHE_PROVIDERS) ||
	    (objp->pk.type != AM_OBJ_URI)
	    ) {
	    glob_am_origin_tbl_age_out_failed1 ++;
	    TAILQ_REMOVE(&nkn_am_origin_tbl_q, objp, entries);
	    NKN_MUTEX_UNLOCK(&AM_origin_hash_table_mutex);
	    return;
	}

	found_objp = AM_origin_tbl_get(objp->pk.type, &objp->pk);
	if (!found_objp) {
	    glob_am_origin_tbl_age_out_failed2 ++;
	    TAILQ_REMOVE(&nkn_am_origin_tbl_q, objp, entries);
	    NKN_MUTEX_UNLOCK(&AM_origin_hash_table_mutex);
	    return;
	}
	NKN_MUTEX_UNLOCK(&AM_origin_hash_table_mutex);
	/* If you have found a valid obj break out */
	break;
    }

    DBG_LOG(ERROR, MOD_AM, "AM origin table queue full"
			    "evicting uri %s",objp->pk.name); 
    ret = AM_origin_tbl_delete(AM_OBJ_URI, &objp->pk);
    if (ret < 0) {
	glob_am_origin_tbl_age_out_err ++;
    } else {
	glob_am_origin_tbl_age_out_cnt ++;
    }
    return;
}

/* Returns the object that is stored in the tree. This function
 must be used with the lock held. Only for AM tbl internal functions.
 NOTE: the mutex (AM_origin_hash_table_mutex) MUST be held by the
 CALLER of this function. */
AM_obj_t *
AM_origin_tbl_get(AM_obj_type_t type __attribute__((unused)), AM_pk_t *pk)
{
    void  *objp = NULL;
    int   ret;
    int   len = MAX_URI_SIZE + 128;
    char  keystr[len];
    char  *key = &keystr[0];

    ret = s_origin_build_key(pk, key, len);
    if(ret < 0)
        return NULL;

    objp = g_hash_table_lookup(AM_origin_hash_table, key);
    return objp;
}

int
AM_origin_tbl_get_copy(AM_obj_type_t type __attribute__((unused)), 
		       AM_pk_t *pk, AM_obj_t *outp)
{
    void  *objp = NULL;
    int   ret;
    int   len = MAX_URI_SIZE + 128;
    char  keystr[len];
    char  *key = &keystr[0];

    ret = s_origin_build_key(pk, key, len);
    if(ret < 0)
        return -1;

    NKN_MUTEX_LOCK(&AM_origin_hash_table_mutex);

    objp = g_hash_table_lookup(AM_origin_hash_table, key);
    if(objp) {
       AM_copy_am_obj(objp, outp);
    } else {
	NKN_MUTEX_UNLOCK(&AM_origin_hash_table_mutex);
	return -1;
    }
    NKN_MUTEX_UNLOCK(&AM_origin_hash_table_mutex);
    return 0;
}

