#include <stdio.h>
#include <glib.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include "nkn_tfm.h"
#include "nkn_tfm_api.h"

int glob_tfm_uri_hash_tbl_total_count = 0;
int glob_tfm_uri_hash_tbl_create_count = 0;
int glob_tfm_uri_hash_tbl_delete_count = 0;
int glob_tfm_uri_dup_entry = 0;
int tmp_tfm_cnt_already_there = 0;

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
	    gpointer value __attribute((unused)),
	    gpointer user_data __attribute((unused)))
{
    TFM_obj *objp = NULL;

    objp = (TFM_obj *) value;
    if(!objp)
        return;

    assert(key != NULL);
    printf("\nfilename: %s len: %ld attr: %p complete: %d ===",
	   objp->filename,
	   objp->tot_content_len, objp->attr, objp->is_complete);
    g_list_foreach(objp->br_list, s_print_list, NULL);
}	/* s_print_obj */

void
tfm_db_print(GHashTable *tfm_db)
{
#if 1
    g_hash_table_foreach(tfm_db, s_print_obj, NULL);
#endif
    return;
}	/* nkn_tfm_uri_tbl_print */

static void
s_pdb_print_obj(gpointer key,
		gpointer value __attribute((unused)),
		gpointer user_data __attribute((unused)))
{
    char *objp = NULL;

    objp = (char *) key;
    if(!objp)
        return;

    assert(key != NULL);
    printf("\nURI = %s", objp);
}	/* s_print_obj */

void
tfm_promote_db_print(GHashTable *tfm_db)
{
    return;
#if 1
    g_hash_table_foreach(tfm_db, s_pdb_print_obj, NULL);
#endif
    return;
}	/* nkn_tfm_uri_tbl_print */

void
nkn_tfm_uri_tbl_cleanup(GHashTable *tfm_db)
{
    /* Bug: 3071
     * Need to call just the hash_table_destroy
     * which will call internal free functions
     */
    g_hash_table_destroy(tfm_db);
}

void
nkn_tfm_uri_tbl_init_full(GHashTable **tfm_db,  
                          GDestroyNotify delete_key, 
                          GDestroyNotify delete_value)
{
    *tfm_db = g_hash_table_new_full(g_str_hash, g_str_equal, delete_key, delete_value);
    assert(*tfm_db);
}       /* nkn_tfm_uri_tbl_init */

void
nkn_tfm_uri_tbl_init(GHashTable **tfm_db)
{
    *tfm_db = g_hash_table_new(g_str_hash, g_str_equal);
    assert(*tfm_db);
}	/* nkn_tfm_uri_tbl_init */

int
nkn_tfm_uri_tbl_insert(GHashTable *tfm_db, char *key,
		       void *value)
{
    g_hash_table_insert(tfm_db, key, value);
    return 0;
}	/* nkn_tfm_uri_tbl_insert */

void
nkn_tfm_uri_tbl_delete(GHashTable *tfm_db, char *key)
{
    g_hash_table_remove(tfm_db, key);

}	/* nkn_tfm_uri_tbl_delete */

void *
nkn_tfm_uri_tbl_get(GHashTable *tfm_db, char *key)
{
    void *ret_val = NULL;

    if(!key) {
        return NULL;
    }
    ret_val = g_hash_table_lookup(tfm_db, key);

    return ret_val;
}	/* nkn_tfm_uri_tbl_get */

