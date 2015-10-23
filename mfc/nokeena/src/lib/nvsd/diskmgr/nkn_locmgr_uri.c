#include <stdio.h> 
#include <glib.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include "nkn_locmgr_uri.h"
#include "nkn_locmgr_extent.h"
#include "nkn_locmgr_physid.h"

static GHashTable *nkn_locmgr_uri_hash_table = NULL;
static GMutex *nkn_locmgr_uri_mutex = NULL;
#if 0
FILE *nkn_locmgr_uri_log_fd = NULL;
FILE *nkn_locmgr_uri_dump_fd = NULL;
#endif

int glob_locmgr_uri_hash_tbl_total_count = 0;
int glob_locmgr_uri_hash_tbl_create_count = 0;
int glob_locmgr_uri_hash_tbl_delete_count = 0;
int glob_locmgr_uri_dup_entry = 0;
int tmp_lm_cnt_already_there = 0;

void locmgr_uri_dump_counters(void);

void
locmgr_uri_dump_counters(void)
{
    printf("\nglob_locmgr_uri_hash_tbl_total_count = %d",
	   glob_locmgr_uri_hash_tbl_total_count);
    printf("\nglob_locmgr_uri_hash_tbl_create_count = %d",
	   glob_locmgr_uri_hash_tbl_create_count);
    printf("\nglob_locmgr_uri_hash_tbl_delete_count = %d",
	   glob_locmgr_uri_hash_tbl_delete_count);
    printf("\nglob_locmgr_uri_dup_entry = %d", glob_locmgr_uri_dup_entry);
}	/* locmgr_uri_dump_counters */


static void
s_print_list(gpointer data,
	     gpointer user_data)
{
    char	     mem[2048];
    struct LM_extent *clip = (struct LM_extent *) data;

#if 0
    sprintf(mem, "%ld %s %d %d %ld, %s, %s\n", clip->extent_id,
	    clip->uol.uri, clip->uol.offset, clip->uol.length,
	    clip->tot_content_len,
	    clip->physid, clip->filename);
    sprintf(mem, "%s %lu %lu %s, %s %ld %ld\n",
	    clip->uol.uri, clip->uol.offset, clip->uol.length,
	    clip->physid, clip->filename, clip->file_offset,
	    clip->tot_content_len);
    fwrite(&mem, 1, strlen(mem), nkn_locmgr_uri_dump_fd);
    printf("%s %d %d %s, %s\n",
	   clip->uol.uri, clip->uol.offset, clip->uol.length,
	   clip->physid, clip->filename);
#endif
}	/* s_print_list */


static void
s_print_obj(gpointer key,
	    gpointer value,
	    gpointer user_data)
{
#if 1
    GList *clip;

    clip = (GList *) value;

    assert(key != NULL);
    g_list_foreach(clip, s_print_list, NULL);
#endif
}	/* s_print_obj */


void
nkn_locmgr_uri_tbl_print(void)
{
#if 1
    g_hash_table_foreach(nkn_locmgr_uri_hash_table, s_print_obj, NULL);
#endif
    return;
}	/* nkn_locmgr_uri_tbl_print */


void
nkn_locmgr_uri_tbl_cleanup(void)
{
    g_hash_table_destroy(nkn_locmgr_uri_hash_table);
#if 0
    fclose(nkn_locmgr_uri_dump_fd);
    //fclose(nkn_locmgr_uri_log_fd);
#endif
}


void
nkn_locmgr_uri_tbl_init(void)
{
#if 0
    FILE *log_file;
    FILE *log_file1;
#endif

    nkn_locmgr_uri_hash_table = g_hash_table_new(g_str_hash, g_str_equal);
    assert(nkn_locmgr_uri_hash_table);

    assert(nkn_locmgr_uri_mutex == NULL);
    nkn_locmgr_uri_mutex = g_mutex_new();

#if 0
    log_file = fopen("/nkn/nkn_locmgr_uri_log.txt" , "a+");
    assert(log_file);
    nkn_locmgr_uri_log_fd = log_file;
    log_file1 = fopen("/nkn/nkn_locmgr_uri_dump.txt" , "w");
    assert(log_file1);
    nkn_locmgr_uri_dump_fd = log_file1;
#endif
}	/* nkn_locmgr_uri_tbl_init */


int
nkn_locmgr_uri_tbl_insert(char *key,
			  void *value)
{
    glob_locmgr_uri_hash_tbl_create_count++;
    g_mutex_lock(nkn_locmgr_uri_mutex);
    g_hash_table_insert(nkn_locmgr_uri_hash_table, key, value);
    g_mutex_unlock(nkn_locmgr_uri_mutex);

    glob_locmgr_uri_hash_tbl_total_count =
	g_hash_table_size(nkn_locmgr_uri_hash_table);
    return 0;
}	/* nkn_locmgr_uri_tbl_insert */


void
nkn_locmgr_uri_tbl_delete(char *key)
{
    glob_locmgr_uri_hash_tbl_delete_count++;
    g_mutex_lock(nkn_locmgr_uri_mutex);
    g_hash_table_remove(nkn_locmgr_uri_hash_table, key);
    g_mutex_unlock(nkn_locmgr_uri_mutex);
}	/* nkn_locmgr_uri_tbl_delete */


void *
nkn_locmgr_uri_tbl_get(char *key)
{
    void *ret_val = NULL;

    g_mutex_lock(nkn_locmgr_uri_mutex);
    ret_val = g_hash_table_lookup(nkn_locmgr_uri_hash_table, key);
    g_mutex_unlock(nkn_locmgr_uri_mutex);

    return ret_val;
}	/* nkn_locmgr_uri_tbl_get */
