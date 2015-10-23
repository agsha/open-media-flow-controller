#include <stdio.h>
#include <glib.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include "nkn_locmgr_uri.h"

static GHashTable *nkn_locmgr_file_hash_table = NULL;
static GMutex *nkn_locmgr_file_mutex = NULL;
#if 0
FILE *nkn_locmgr_file_log_fd = NULL;
FILE *nkn_locmgr_file_dump_fd = NULL;
#endif

int glob_locmgr_file_hash_tbl_total_count = 0;
int glob_locmgr_file_hash_tbl_create_count = 0;
int glob_locmgr_file_hash_tbl_delete_count = 0;
int glob_locmgr_file_dup_entry = 0;

void locmgr_file_dump_counters(void);
void nkn_locmgr_file_tbl_cleanup(void);

void
locmgr_file_dump_counters(void)
{
	printf("\nglob_locmgr_file_hash_tbl_total_count = %d", glob_locmgr_file_hash_tbl_total_count);
	printf("\nglob_locmgr_file_hash_tbl_create_count = %d", glob_locmgr_file_hash_tbl_create_count);
	printf("\nglob_locmgr_file_hash_tbl_delete_count = %d", glob_locmgr_file_hash_tbl_delete_count);
	printf("\nglob_locmgr_file_dup_entry = %d", glob_locmgr_file_dup_entry);

}

#if 0
static void
s_print_obj(
	gpointer key, 
	gpointer value, 
	gpointer user_data)
{
#if 0
	char mem[2048];
	struct DM_clip_location *clip;
	int j = 0;
	int i = 1;

	clip = (struct DM_clip_location *) value;	

		for(j = 1; j <= 4; j++) {
#if 0
			sprintf(mem, "%ld %ld http://10.%d.%d.11:89%s\n", 
				clip->offlen.offset, clip->offlen.length, 
				i, j, clip->file_file);
			fwrite(&mem, 1, strlen(mem), nkn_locmgr_file_log_fd);	
#endif
			sprintf(mem, "http://10.%d.%d.11:85%s\n", 
				i, j, clip->file_file);
			fwrite(&mem, 1, strlen(mem), nkn_locmgr_file_log_fd);	

		}
		sprintf(mem, "%lld %lld %s %s\n", clip->offlen.offset, 
			clip->offlen.length, clip->storage_file, 
			clip->file_file);
		fwrite(&mem, 1, strlen(mem), nkn_locmgr_file_dump_fd);	
#endif
}
#endif


void
nkn_locmgr_file_tbl_print()
{
#if 0
	g_hash_table_foreach(nkn_locmgr_file_hash_table, s_print_obj, NULL);
#endif
	return;
}
static void
s_delete_LM_file(
	gpointer key, 
	gpointer value, 
	gpointer user_data)
{
	struct LM_file *lm_file;
	lm_file = (struct LM_file *) value;	
        if (lm_file) {
                free(lm_file);
        }
}

void
nkn_locmgr_file_tbl_cleanup(void)
{
        g_hash_table_foreach(nkn_locmgr_file_hash_table, s_delete_LM_file,
                             NULL);
	g_hash_table_destroy(nkn_locmgr_file_hash_table);
#if 0
	fclose(nkn_locmgr_file_dump_fd);
	fclose(nkn_locmgr_file_log_fd);
#endif
}

void
nkn_locmgr_file_tbl_init()
{
#if 0
	FILE *log_file;
	FILE *log_file1;
#endif

	nkn_locmgr_file_hash_table = g_hash_table_new(g_str_hash, g_str_equal);
	assert(nkn_locmgr_file_hash_table);

	assert(nkn_locmgr_file_mutex == NULL);
	nkn_locmgr_file_mutex = g_mutex_new();

#if 0
	log_file = fopen("/nkn/nkn_locmgr_file_log.txt" ,"a+");
	assert(log_file);
	nkn_locmgr_file_log_fd = log_file;

	log_file1 = fopen("/nkn/nkn_locmgr_file_dump.txt" ,"a+");
	assert(log_file1);
	nkn_locmgr_file_dump_fd = log_file1;
#endif
}


int
nkn_locmgr_file_tbl_insert(char *key, void *value)
{
	glob_locmgr_file_hash_tbl_create_count ++;
	g_mutex_lock(nkn_locmgr_file_mutex);
	g_hash_table_insert(nkn_locmgr_file_hash_table, key, value);
	g_mutex_unlock(nkn_locmgr_file_mutex);

	glob_locmgr_file_hash_tbl_total_count = g_hash_table_size(nkn_locmgr_file_hash_table);
	return 0;
}

int
nkn_locmgr_file_tbl_delete(char *key) 
{
	int ret_val = -1;

	glob_locmgr_file_hash_tbl_delete_count ++;
	g_mutex_lock(nkn_locmgr_file_mutex);
	g_hash_table_remove(nkn_locmgr_file_hash_table, key);
	g_mutex_unlock(nkn_locmgr_file_mutex);

	return ret_val;
}

void *
nkn_locmgr_file_tbl_get(char *key) 
{
	void *ret_val = NULL;

	g_mutex_lock(nkn_locmgr_file_mutex);
	ret_val = g_hash_table_lookup(nkn_locmgr_file_hash_table, key);
	g_mutex_unlock(nkn_locmgr_file_mutex);

	return ret_val;
}

