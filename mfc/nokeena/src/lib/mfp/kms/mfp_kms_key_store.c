#include "kms/mfp_kms_key_store.h"

#include "nkn_debug.h"
#include "nkn_assert.h"
#include "nkn_stat.h"

#include <stdlib.h>

static kms_key_resp_t* getKMS_Key(
		kms_client_key_store_t* kms_key_store, uint32_t key_seq_num);

static void delKMS_Key(kms_client_key_store_t *kms_key_store, 
		       uint32_t key_seq_num);

static void deleteKMS_KeyStore(kms_client_key_store_t* kms_key_store);

static gint u32SeqCompare(gconstpointer, gconstpointer, gpointer);

static void deleteKey(gpointer);

static void deleteKeyResponse(gpointer);

static int32_t initKMS_KeyStoreStats(kms_client_key_store_t *kms_key_store,
				     const char *sess_name,
				     int32_t stream_id);

kms_client_key_store_t* createKMS_ClientKeyStore(kms_client_t* kms_client, 
						 uint32_t key_rot_count, 
						 const char *sess_name, 
						 uint32_t stream_id) {
	if (kms_client == NULL)
		return NULL;

	kms_client_key_store_t* kms_key_store = kms_calloc(1,
			sizeof(kms_client_key_store_t));
	if (kms_key_store == NULL)
		return NULL;

	kms_key_store->key_store = g_tree_new_full(u32SeqCompare, NULL,
			deleteKey, deleteKeyResponse);

	kms_key_store->kms_client = kms_client;
	kms_key_store->key_rot_count = key_rot_count;
	kms_key_store->k_resp_rtz = NULL;

	initKMS_KeyStoreStats(kms_key_store, sess_name, stream_id);
	pthread_mutex_init(&kms_key_store->lock, NULL);

	kms_key_store->get_kms_key = getKMS_Key;
	kms_key_store->del_kms_key = delKMS_Key;
	kms_key_store->delete_ctx = deleteKMS_KeyStore;

	return kms_key_store;
}

static int32_t initKMS_KeyStoreStats(kms_client_key_store_t *kms_key_store,
				     const char *sess_name,
				     int32_t stream_id) {

       char *str = NULL;

       snprintf(kms_key_store->stats.instance_name,
		MAX_PUB_NAME_LEN, "%s", sess_name);

       /* setup counter for number of key fetches from kms
	* per stream per session
	*/       
       str = kms_key_store->stats.kms_num_keys_kms_name = 
	   kms_malloc(100);
       snprintf(str, 256, "mfp_live_strm_%d.num_keys_from_kms",
		stream_id % MAX_STREAM_PER_ENCAP);
       nkn_mon_add(str, kms_key_store->stats.instance_name,
		   &kms_key_store->stats.kms_num_keys_kms,
		   sizeof(kms_key_store->stats.kms_num_keys_kms));

       /* setup counter for number of key fetches from keystore
	* per stream per session
	*/       
       str = kms_key_store->stats.kms_num_keys_store_name = 
	   kms_malloc(100);
       snprintf(str, 256, "mfp_live_strm_%d.num_keys_from_store",
		stream_id % MAX_STREAM_PER_ENCAP);
       nkn_mon_add(str, kms_key_store->stats.instance_name,
		   &kms_key_store->stats.kms_num_keys_store,
		   sizeof(kms_key_store->stats.kms_num_keys_store));

       return 0;
}

static int32_t cleanKMS_KeyStoreStats(kms_client_key_store_t *kms_key_store) {

        kms_client_key_store_stats_t *st = NULL;

	NKN_ASSERT(kms_key_store);
	st = &kms_key_store->stats;    
	
	if (st->kms_num_keys_store_name) {
	    nkn_mon_delete(st->kms_num_keys_store_name,
			   st->instance_name);
	    free(st->kms_num_keys_store_name);
	}

	if (st->kms_num_keys_kms_name) {
	    nkn_mon_delete(st->kms_num_keys_kms_name,
			   st->instance_name);
	    free(st->kms_num_keys_kms_name);
	}
      
	return 0;
}

static kms_key_resp_t* getKMS_Key(kms_client_key_store_t* kms_key_store,
		uint32_t key_seq_num) {

	pthread_mutex_lock(&kms_key_store->lock);
	if (kms_key_store->key_rot_count == 0) {
		if (kms_key_store->k_resp_rtz == NULL) {
			kms_key_store->k_resp_rtz = kms_key_store->kms_client->
				get_next_key(kms_key_store->kms_client);
			kms_key_store->stats.kms_num_keys_kms++;
			kms_key_store->dbg_key_count++;
		} else
			kms_key_store->k_resp_rtz->is_new = 0;
		kms_key_resp_t* k_resp = kms_key_store->k_resp_rtz;
		if (k_resp == NULL) {
			/* error while retrieving key */
			DBG_MFPLOG ("0", ERROR, MOD_MFPLIVE, "Error fetching key."
					" err code: %d", kms_key_store->kms_client->errcode);

			pthread_mutex_unlock(&kms_key_store->lock);
			return NULL;
		}
		/*
		kms_key_resp_t* key_resp = createKmsKeyResponse(k_resp->key,
				k_resp->key_len, k_resp->key_uri, k_resp->is_new, 1);
				*/
		pthread_mutex_unlock(&kms_key_store->lock);
		return k_resp; 
	}

	/* Sequence number has to start from 1 (not 0) */
	uint32_t seq_num = (key_seq_num - 1)/kms_key_store->key_rot_count;
	kms_key_resp_t* k_resp = g_tree_lookup(kms_key_store->key_store,
			&key_seq_num);

	if (k_resp == NULL) {
		k_resp = kms_key_store->kms_client->get_next_key(
				kms_key_store->kms_client);
		kms_key_store->stats.kms_num_keys_kms++;
		if (k_resp != NULL) {
			uint32_t i;
			uint32_t key_idx = (seq_num *
       			         kms_key_store->key_rot_count) + 1;
			uint32_t* first_key = kms_malloc(sizeof(uint32_t));
			*first_key = key_idx;

			/* insert the first key and copy the same key
			 * 'key_rot_count times
			 */
			g_tree_insert(kms_key_store->key_store,
				      first_key, k_resp);
			/*printf("key store add - key num %d, key resp address %p\n",
					*first_key, k_resp);*/
			kms_key_store->dbg_key_count++;
			for (i = 1; i < kms_key_store->key_rot_count; i++) {
			    uint32_t* key = kms_malloc(sizeof(uint32_t));
			    *key = key_idx + i;
			    kms_key_resp_t* key_val = createKmsKeyResponse(k_resp->key,
					   k_resp->key_len, k_resp->key_uri, k_resp->is_new,
					   *key);  
			    /*printf("key store add - key num %d, key resp address %p\n",
						*key, key_val);*/
			    
			    g_tree_insert(kms_key_store->key_store,
					  key, key_val);
			    kms_key_store->dbg_key_count++;
			}
		} else {
			/* error retrieving key */
			DBG_MFPLOG ("0", ERROR, MOD_MFPLIVE, "Error fetching key."
					" err code: %d", kms_key_store->kms_client->errcode);
			pthread_mutex_unlock(&kms_key_store->lock);
			return NULL;
		}
	} else {
	    kms_key_store->stats.kms_num_keys_store++;
	}

	if ((key_seq_num - 1) % kms_key_store->key_rot_count != 0)
	    	k_resp->is_new = 0;
	else
		k_resp->is_new = 1;
	pthread_mutex_unlock(&kms_key_store->lock);
	return k_resp;
}


static void delKMS_Key(kms_client_key_store_t *kms_key_store, 
		       uint32_t key_seq_num)
{
    pthread_mutex_lock(&kms_key_store->lock);
	if (kms_key_store->key_rot_count != 0) {
		g_tree_remove(kms_key_store->key_store, &key_seq_num);
		kms_key_store->dbg_key_count--;
	}
	pthread_mutex_unlock(&kms_key_store->lock);
}

static void deleteKMS_KeyStore(kms_client_key_store_t* kms_key_store) {

	kms_key_store->kms_client->delete_client(kms_key_store->kms_client);
	g_tree_destroy(kms_key_store->key_store);
	if (kms_key_store->k_resp_rtz != NULL)
		deleteKmsKeyResponse(kms_key_store->k_resp_rtz);
	pthread_mutex_destroy(&kms_key_store->lock);
        cleanKMS_KeyStoreStats(kms_key_store);
	free(kms_key_store);
}


static gint u32SeqCompare(gconstpointer val1, gconstpointer val2,
		gpointer user_date) {

	uint32_t* val_1 = (uint32_t*)val1;
	uint32_t* val_2 = (uint32_t*)val2;

	if (*val_1 < *val_2)
		return -1;
	else if (*val_1 > *val_2)
		return 1;
	else
		return 0;
}


static void deleteKey(gpointer key) {

	uint32_t* val = (uint32_t*)key;
	free(val);
}


static void deleteKeyResponse(gpointer val) {

	kms_key_resp_t* key_resp = (kms_key_resp_t*)val;
	deleteKmsKeyResponse(key_resp);
}


#if 0
//if (((seq_num * kms_key_store->key_rot_count) + i) != 
//		key_seq_num) {
#endif
