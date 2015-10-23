#include <stdio.h>
#include <glib.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include "nkn_am.h"

void
nkn_am_list_tbl_cleanup(GHashTable *hash_tbl)
{
	g_hash_table_destroy(hash_tbl);
}

void
nkn_am_list_tbl_init(GHashTable **hash_tbl, GMutex **mutex,
		     GDestroyNotify key_destroy_func,
		     GDestroyNotify value_destroy_func
		)
{
	/* Have a memory corruption if I add these functions to delete
	   stuff. I am going to null out now and try to handle the delets
	   for now.*/
	key_destroy_func = NULL;
	value_destroy_func = NULL;
	if(key_destroy_func && value_destroy_func) {
		*hash_tbl = g_hash_table_new_full(g_str_hash, g_str_equal,
						  key_destroy_func,
						  value_destroy_func);
	} else {
		*hash_tbl = g_hash_table_new(g_str_hash, g_str_equal);
	}
	assert(*mutex == NULL);
	*mutex = g_mutex_new();
}


int
nkn_am_list_tbl_insert(GHashTable *hash_tbl, GMutex *mutex,
		       char *key, void *value)
{
	g_mutex_lock(mutex);
	g_hash_table_insert(hash_tbl, key, value);
	g_mutex_unlock(mutex);
	return 0;
}

int
nkn_am_list_tbl_delete(GHashTable *hash_tbl, GMutex *mutex, char *key)
{
	int ret_val = -1;

	g_mutex_lock(mutex);
	g_hash_table_remove(hash_tbl, key);
	g_mutex_unlock(mutex);

	return ret_val;
}

void *
nkn_am_list_tbl_get(GHashTable *hash_tbl, GMutex *mutex, char *key)
{
	void *ret_val = NULL;

	g_mutex_lock(mutex);
	ret_val = g_hash_table_lookup(hash_tbl, key);
	g_mutex_unlock(mutex);

	return ret_val;
}

static void
s_print_list(
        gpointer in_data,
        gpointer user_data)
{
	FILE *fp = (FILE *) user_data;

        AM_obj_t *pobj = (AM_obj_t *) in_data;

	if(!pobj) {
	    return;
	}
	if((pobj->pk.provider_id <= 0) ||
	   (pobj->pk.provider_id >= NKN_MM_MAX_CACHE_PROVIDERS) ||
	   ((pobj->pk.type != AM_OBJ_URI) &&
	   (pobj->pk.type != AM_OBJ_DISK))
	   ) {
	    return;
	}
	fprintf(fp, " pobj: %p pk: name = %s, prov-id = %d, sub-provider-id = %d", pobj, pobj->pk.name, pobj->pk.provider_id, pobj->pk.sub_provider_id);
}

static void
s_print_list_local(
        gpointer in_data,
        gpointer user_data __attribute((unused)))
{

        AM_obj_t *pobj = (AM_obj_t *) in_data;
	int h1;

	if((pobj->pk.provider_id <= 0) ||
	   (pobj->pk.provider_id >= NKN_MM_MAX_CACHE_PROVIDERS) ||
	   ((pobj->pk.type != AM_OBJ_URI) &&
	   (pobj->pk.type != AM_OBJ_DISK))
	   ) {
	    assert(0);
	}

	h1 = am_decode_hotness(&pobj->hotness);
	printf(" pobj: %p pk: type = %d, name = %s, prov-id = %d, sub-provider-id = %d hotness = %d, num hits = %ld, last update time=%ld", pobj, pobj->pk.type, pobj->pk.name, pobj->pk.provider_id, pobj->pk.sub_provider_id, h1, pobj->stats.total_num_hits, pobj->stats.tm_last_update.tv_sec);
}

static void
s_print_prov_list(
        gpointer key,
        gpointer value,
        gpointer user_data)
{
	char *key1 = (char *) key;
	GList *tmp;
	FILE *fp = (FILE *) user_data;

	fprintf(fp, "\n KEY: %s\n", key1);
        nkn_am_prov_list_t *clip = (nkn_am_prov_list_t *) value;
        assert(key != NULL);
        //g_list_foreach(clip->am_prov_list_head, s_print_list, NULL);
	tmp = clip->am_prov_list_head;
	while(tmp) {
	    fprintf(fp, "\nplist: %p", tmp);
	    s_print_list((gpointer)tmp->data, fp);
	    tmp = tmp->next;
	}
	printf("\nEND LIST ---------------------\n");
}

static void
s_print_prov_list_local(
        gpointer key,
        gpointer value,
        gpointer user_data)
{
    char *key1 = (char *) key;
    GList *tmp;
    int* ptype = (int *) user_data;
    AM_obj_t *pobj;
    int count = 0;

    if((*ptype) == 0) {
    /* Print all */
	printf("\n KEY: %s\n", key1);
        nkn_am_prov_list_t *clip = (nkn_am_prov_list_t *) value;
        assert(key != NULL);
	tmp = clip->am_prov_list_head;
	while(tmp) {
	    printf("\n%d: plist: %p", count, tmp);
	    s_print_list_local((gpointer)tmp->data, NULL);
	    tmp = tmp->next;
	    count ++;
	}
    } else {
        printf("\n KEY: %s\n", key1);
        nkn_am_prov_list_t *clip = (nkn_am_prov_list_t *) value;
        assert(key != NULL);
        tmp = clip->am_prov_list_head;
        if(!tmp)
            return;
        pobj = (AM_obj_t *)tmp->data;
        if(pobj == NULL)
            return;

        if((int)pobj->pk.provider_id != (*ptype)) {
            return;
        }

        while(tmp) {
            printf("\nplist: %p", tmp);
            s_print_list_local((gpointer)tmp->data, NULL);
            tmp = tmp->next;
            count ++;
        }
    }
    printf("\nTotal number of items in this list: %d\n", count);
}


void
nkn_am_hotness_list_print(GHashTable *hash_tbl, FILE *fp)
{
        g_hash_table_foreach(hash_tbl, s_print_prov_list, fp);
        return;
}

void
nkn_am_hotness_list_print_local(GHashTable *hash_tbl, int ptype)
{
        g_hash_table_foreach(hash_tbl, s_print_prov_list_local, 
			     (gpointer *)&ptype);
        return;
}

static void
s_print_prov_lru_list_local(
        gpointer key,
        gpointer value,
        gpointer user_data)
{
    char *key1 = (char *) key;
    GList *tmp;
    int* ptype = (int *) user_data;
    AM_obj_t *pobj;
    int count = 0;

    if((*ptype) == 0) {
	/* Print all */
	printf("\n KEY: %s\n", key1);
        nkn_am_prov_list_t *clip = (nkn_am_prov_list_t *) value;
        assert(key != NULL);
	tmp = clip->am_prov_list_head;
	while(tmp) {
	    printf("\n%d: plist: %p", count, tmp);
	    s_print_list_local((gpointer)tmp->data, NULL);
	    tmp = tmp->next;
	    count ++;
	}
    } else {
        printf("\n KEY: %s\n", key1);
        nkn_am_prov_list_t *clip = (nkn_am_prov_list_t *) value;
        assert(key != NULL);
        tmp = clip->am_prov_list_head;
        if(!tmp)
            return;
        pobj = (AM_obj_t *)tmp->data;
        if(pobj == NULL)
            return;

        if((int)pobj->pk.provider_id != (*ptype)) {
            return;
        }

        while(tmp) {
            printf("\nplist: %p", tmp);
            s_print_list_local((gpointer)tmp->data, NULL);
            tmp = tmp->next;
            count ++;
        }
    }
    printf("\nTotal number of items in this list: %d\n", count);
}

void
nkn_am_hotness_lru_list_print_local(GHashTable *hash_tbl, int ptype)
{
        g_hash_table_foreach(hash_tbl, s_print_prov_lru_list_local, 
			     (gpointer *)&ptype);
        return;
}



