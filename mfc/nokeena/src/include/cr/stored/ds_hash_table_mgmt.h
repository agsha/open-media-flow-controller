#ifndef _DS_HASH_TABLE_MGMT_
#define _DS_HASH_TABLE_MGMT_

#include <stdio.h>
#include <inttypes.h>
#include <pthread.h>
#include <glib.h>
#include <cprops/trie.h>

#ifdef __cplusplus
extern "C" {
#endif

#if 0 //keeps emacs happy
}
#endif 

typedef struct ds_hash_table_mgmt_state{
    void * val_ptr;
    pthread_mutex_t lock;
}ds_hash_table_t;

typedef struct hash_table_props{
    GHashTable * htable;
    cp_trie *trie;
    pthread_rwlock_t lock;
}hash_tbl_props_t;

int32_t ds_hash_table_mgmt_init(hash_tbl_props_t* htp);
int32_t ds_hash_table_mgmt_deinit(hash_tbl_props_t *htp);
int32_t ds_hash_table_mgmt_add(void* val, char *key, 
			       hash_tbl_props_t* htp);
int32_t ds_hash_table_mgmt_alter(void* val, char *key, 
				 hash_tbl_props_t* htp);
ds_hash_table_t *ds_hash_table_mgmt_find(char *key,
					 hash_tbl_props_t* htp);
ds_hash_table_t *ds_hash_table_mgmt_acq(char *key,
					hash_tbl_props_t* htp);
int32_t ds_hash_table_mgmt_rel(char *key,
			       hash_tbl_props_t* htp);
int32_t ds_hash_table_mgmt_remove(char *key,
				  hash_tbl_props_t* htp);
void  ds_hash_table_mgmt_cleanup_state(ds_hash_table_t *p);

#define HOLD_TBL(_p) \
    pthread_mutex_lock(&_p->lock);\

#define REL_TBL(_p) \
    pthread_mutex_unlock(&_p->lock);\

#define HOLD_TBL_OBJ(_p) \
    pthread_mutex_lock(&_p->lock);\

#define REL_TBL_OBJ(_p) \
    pthread_mutex_unlock(&_p->lock);\
    
int32_t ds_trie_mgmt_init(hash_tbl_props_t* htp);

int32_t ds_trie_mgmt_add(void* val, char *key, 
			 hash_tbl_props_t* htp);

ds_hash_table_t *ds_trie_mgmt_find(char *key, 
		   hash_tbl_props_t* htp);

int32_t ds_trie_mgmt_remove(char *key, 
		    hash_tbl_props_t* htp);

int32_t ds_cont_write_lock(hash_tbl_props_t* ds_con);
int32_t ds_cont_write_unlock(hash_tbl_props_t* ds_con);
int32_t ds_cont_read_lock(hash_tbl_props_t* ds_con);
int32_t ds_cont_read_unlock(hash_tbl_props_t* ds_con);

#ifdef __cplusplus
}
#endif

#endif //_DS_HASH_TABLE_MGMT_
