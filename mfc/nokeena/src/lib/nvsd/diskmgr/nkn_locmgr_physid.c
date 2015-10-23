#include <stdio.h>
#include <glib.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include "nkn_locmgr_physid.h"
#include "nkn_locmgr_extent.h"

static GHashTable *nkn_locmgr_physid_hash_table = NULL;
static GMutex *nkn_locmgr_physid_mutex = NULL;
#if 0
FILE *nkn_locmgr_physid_log_fd = NULL;
FILE *nkn_locmgr_physid_dump_fd = NULL;
#endif

int glob_locmgr_physid_hash_tbl_total_count = 0;
int glob_locmgr_physid_hash_tbl_create_count = 0;
int glob_locmgr_physid_hash_tbl_delete_count = 0;
int glob_locmgr_physid_dup_entry = 0;

void locmgr_physid_dump_counters(void);

void
locmgr_physid_dump_counters(void)
{
	printf("\nglob_locmgr_physid_hash_tbl_total_count = %d", glob_locmgr_physid_hash_tbl_total_count);
	printf("\nglob_locmgr_physid_hash_tbl_create_count = %d", glob_locmgr_physid_hash_tbl_create_count);
	printf("\nglob_locmgr_physid_hash_tbl_delete_count = %d", glob_locmgr_physid_hash_tbl_delete_count);
	printf("\nglob_locmgr_physid_dup_entry = %d", glob_locmgr_physid_dup_entry);

}

static void
s_print_list(
	gpointer data,
	gpointer user_data)
{
	char mem[2048];
	struct LM_extent *clip = (struct LM_extent *) data;

#if 0
	sprintf(mem, "%ld %s %d %d %ld, %s, %s\n", clip->extent_id,
		clip->uol.physid, clip->uol.offset, clip->uol.length,
		clip->tot_content_len,
		clip->physid, clip->filename);
	sprintf(mem, "%s %ld %ld %s, %s\n",
		clip->uol.uri, clip->uol.offset, clip->uol.length,
		clip->physid, clip->filename);
	fwrite(&mem, 1, strlen(mem), nkn_locmgr_physid_dump_fd);
#endif
}

static void
s_print_obj(
	gpointer key,
	gpointer value,
	gpointer user_data)
{
#if 1
	GList *clip;

	clip = (GList *) value;	

        assert(key != NULL);
        g_list_foreach(clip, s_print_list, NULL);
#endif
}


void
nkn_locmgr_physid_tbl_print()
{
#if 1
	g_hash_table_foreach(nkn_locmgr_physid_hash_table, s_print_obj, NULL);
#endif
	return;
}

static void
s_delete_LM_extent(
	gpointer key, 
	gpointer value, 
	gpointer user_data)
{
	struct LM_extent *lm_extent;
	lm_extent = (struct LM_extent *) value;	
        if (lm_extent) {
                if (lm_extent->uol.uri) {
                        free(lm_extent->uol.uri);
                }
                free(lm_extent);
        }
}

static void
s_delete_extent_elements(
        gpointer key,
	gpointer data,
	gpointer user_data)
{
        if (data) {
	        GList *list = (GList *) data;	
                g_list_foreach(list, (GFunc)s_delete_LM_extent, NULL);
        }
}

void
nkn_locmgr_physid_tbl_cleanup()
{
#if 0
	fclose(nkn_locmgr_physid_dump_fd);
	//fclose(nkn_locmgr_physid_log_fd);
#endif
	g_hash_table_foreach(nkn_locmgr_physid_hash_table, 
                             s_delete_extent_elements,
                             NULL);
	g_hash_table_destroy(nkn_locmgr_physid_hash_table);
}

void
nkn_locmgr_physid_tbl_init()
{
#if 0
	FILE *log_file;
	FILE *log_file1;
#endif

	nkn_locmgr_physid_hash_table = g_hash_table_new(g_str_hash, g_str_equal);
	assert(nkn_locmgr_physid_hash_table);

	assert(nkn_locmgr_physid_mutex == NULL);
	nkn_locmgr_physid_mutex = g_mutex_new();

#if 0
	log_file = fopen("/nkn/nkn_locmgr_physid_log.txt" ,"a+");
	assert(log_file);
	nkn_locmgr_physid_log_fd = log_file;
	log_file1 = fopen("/nkn/nkn_locmgr_physid_dump.txt" ,"w");
	assert(log_file1);
	nkn_locmgr_physid_dump_fd = log_file1;
#endif
}

int
nkn_locmgr_physid_tbl_insert(char *key, void *value)
{
	glob_locmgr_physid_hash_tbl_create_count ++;
	g_mutex_lock(nkn_locmgr_physid_mutex);
	g_hash_table_insert(nkn_locmgr_physid_hash_table, key, value);
	g_mutex_unlock(nkn_locmgr_physid_mutex);

	glob_locmgr_physid_hash_tbl_total_count = g_hash_table_size(nkn_locmgr_physid_hash_table);
	return 0;
}

void
nkn_locmgr_physid_tbl_delete(char *key) 
{
	glob_locmgr_physid_hash_tbl_delete_count ++;
	g_mutex_lock(nkn_locmgr_physid_mutex);
	g_hash_table_remove(nkn_locmgr_physid_hash_table, key);
	g_mutex_unlock(nkn_locmgr_physid_mutex);
}

void *
nkn_locmgr_physid_tbl_get(char *key) 
{
	void *ret_val = NULL;

	g_mutex_lock(nkn_locmgr_physid_mutex);
	ret_val = g_hash_table_lookup(nkn_locmgr_physid_hash_table, key);
	g_mutex_unlock(nkn_locmgr_physid_mutex);

	return ret_val;
}

