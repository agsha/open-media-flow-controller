#include <stdio.h>
#include <glib.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include "nkn_tfm.h"
#include "nkn_tfm_api.h"
#include "nkn_cod.h"
#include "nkn_tfm.h"

int glob_tfm_cod_hash_tbl_total_count = 0;
int glob_tfm_cod_hash_tbl_create_count = 0;
int glob_tfm_cod_hash_tbl_delete_count = 0;
int glob_tfm_cod_dup_entry = 0;

static void
s_print_list(gpointer data,
	     gpointer user_data __attribute((unused)))
{
    br_obj_t *br = (br_obj_t *) data;
    if(!br)
        return;
    printf("br: start_offset: %ld, end_offset: %ld",
            br->start_offset, br->end_offset);
}	/* s_print_list */

static void
s_print_obj(gpointer key,
	    gpointer value,
	    gpointer user_data __attribute((unused)))
{
    TFM_obj *objp = NULL;
    int     ret;
    char    *uri;

    objp = (TFM_obj *) value;
    if(!objp)
        return;

    ret = tfm_cod_test_and_get_uri(objp->cod, &uri);
    if(ret < 0) {
	return;
    }

    assert(key != NULL);
    ret = *(int *)key;
    printf("\nkey = %d, cod: %lld uri = %s, filename: %s len: %ld attr: %p complete: %d ===", ret, 
	   (unsigned long long)objp->cod, uri, objp->filename,
	   objp->tot_content_len, objp->attr, objp->is_complete);
    g_list_foreach(objp->br_list, s_print_list, NULL);
}	/* s_print_obj */

void
tfm_cod_db_print(GHashTable *tfm_db)
{
    g_hash_table_foreach(tfm_db, s_print_obj, NULL);
    return;
}	/* tfm_cod_tbl_print */

static void
s_delete_mem(gpointer key __attribute((unused)),
	     gpointer value,
	     gpointer user_data __attribute((unused)))
{
    char *value1 = (char *) value;

    if(!value1) {
	return;
    }
    free(value1);
}	/* s_print_obj */

void
nkn_tfm_cod_tbl_cleanup(GHashTable *tfm_db)
{
    g_hash_table_foreach(tfm_db, s_delete_mem, NULL);
    g_hash_table_destroy(tfm_db);
}

void
nkn_tfm_cod_tbl_init_full(GHashTable **tfm_db,
                          GDestroyNotify delete_key,
                          GDestroyNotify delete_value)
{
    *tfm_db = g_hash_table_new_full(g_int_hash, g_int_equal,
				    delete_key, delete_value);
    assert(*tfm_db);
}       /* nkn_tfm_cod_tbl_init_full */

void
nkn_tfm_cod_tbl_init(GHashTable **tfm_db)
{
    *tfm_db = g_hash_table_new(g_int_hash, g_int_equal);
    assert(*tfm_db);
}	/* nkn_tfm_cod_tbl_init */

int
nkn_tfm_cod_tbl_insert(GHashTable *tfm_db, void *key,
		       void *value)
{
    g_hash_table_insert(tfm_db, key, value);
    return 0;
}	/* nkn_tfm_cod_tbl_insert */

void
nkn_tfm_cod_tbl_delete(GHashTable *tfm_db, nkn_cod_t key)
{
    g_hash_table_remove(tfm_db, (void *)&key);
}	/* nkn_tfm_cod_tbl_delete */

void *
nkn_tfm_cod_tbl_get(GHashTable *tfm_db, nkn_cod_t key)
{
    void *ret_val = NULL;

    ret_val = g_hash_table_lookup(tfm_db, (void *)&key);

    return ret_val;
}	/* nkn_tfm_cod_tbl_get */

