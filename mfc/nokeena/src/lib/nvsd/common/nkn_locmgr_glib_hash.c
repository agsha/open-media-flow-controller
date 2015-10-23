#include <stdio.h>
#include <glib.h>
#include <stdlib.h>
#include "nkn_hash_map.h"
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include "nkn_defs.h"

static GHashTable *nkn_LOCmgr_hash_table = NULL;
static GMutex *nkn_LOCmgr_mutex = NULL;
FILE *nkn_locmgr_log_fd = NULL;
FILE *nkn_locmgr_dump_fd = NULL;

int glob_locmgr_hash_tbl_total_count = 0;
int glob_locmgr_hash_tbl_create_count = 0;
int glob_locmgr_hash_tbl_delete_count = 0;
int glob_locmgr_dup_entry = 0;

void locmgr_dump_counters(void);
void c_LOCmgr_tbl_cleanup(void);


void
locmgr_dump_counters(void) 
{
	printf("\nglob_locmgr_hash_tbl_total_count = %d", glob_locmgr_hash_tbl_total_count);
	printf("\nglob_locmgr_hash_tbl_create_count = %d", glob_locmgr_hash_tbl_create_count);
	printf("\nglob_locmgr_hash_tbl_delete_count = %d", glob_locmgr_hash_tbl_delete_count);
	printf("\nglob_locmgr_dup_entry = %d", glob_locmgr_dup_entry);

}

struct DM_offset_length {
        unsigned long long offset;
        unsigned long long length;
};

struct DM_clip_location{
        char *storage_file;
        struct DM_offset_length offlen;
        unsigned long long   *off_in_storage_file;
        int     segmentnum;
        char *uri_dir;
        char *uri_file;
} ;


static void
s_print_obj(
	gpointer key, 
	gpointer value, 
	gpointer user_data)
{
	char mem[2048];
	struct DM_clip_location *clip;
	int j = 0;
	int i = 1;

	UNUSED_ARGUMENT(key);
	UNUSED_ARGUMENT(user_data);

	clip = (struct DM_clip_location *) value;	

		for(j = 1; j <= 4; j++) {
#if 0
			sprintf(mem, "%ld %ld http://10.%d.%d.11:89%s\n", 
				clip->offlen.offset, clip->offlen.length, 
				i, j, clip->uri_file);
			fwrite(&mem, 1, strlen(mem), nkn_locmgr_log_fd);	
#endif
			snprintf(mem, 2048, "http://10.%d.%d.11:85%s\n", 
				i, j, clip->uri_file);
			fwrite(&mem, 1, strlen(mem), nkn_locmgr_log_fd);	

		}
		snprintf(mem, 2048, "%lld %lld %s %s\n", clip->offlen.offset, 
			clip->offlen.length, clip->storage_file, 
			clip->uri_file);
		fwrite(&mem, 1, strlen(mem), nkn_locmgr_dump_fd);	
}

void
c_LOCmgr_tbl_print()
{
	g_hash_table_foreach(nkn_LOCmgr_hash_table, s_print_obj, NULL);

	return;
}

void
c_LOCmgr_tbl_cleanup(void)
{
	fclose(nkn_locmgr_dump_fd);
	fclose(nkn_locmgr_log_fd);
}

void
c_LOCmgr_tbl_init()
{
	FILE *log_file;
	FILE *log_file1;

	nkn_LOCmgr_hash_table = g_hash_table_new(g_str_hash, g_str_equal);
	assert(nkn_LOCmgr_hash_table);

	assert(nkn_LOCmgr_mutex == NULL);
	nkn_LOCmgr_mutex = g_mutex_new();

	log_file = fopen("./nkn_locmgr_log.txt" ,"a+");	
	assert(log_file);
	nkn_locmgr_log_fd = log_file;

	log_file1 = fopen("./nkn_locmgr_dump.txt" ,"a+");	
	assert(log_file1);
	nkn_locmgr_dump_fd = log_file1;
}

int
c_LOCmgr_tbl_insert(char *key, void *value)
{

	glob_locmgr_hash_tbl_create_count ++;
	g_mutex_lock(nkn_LOCmgr_mutex);
	g_hash_table_insert(nkn_LOCmgr_hash_table, key, value);
	g_mutex_unlock(nkn_LOCmgr_mutex);

	glob_locmgr_hash_tbl_total_count = g_hash_table_size(nkn_LOCmgr_hash_table);
	return 0;
}

int
c_LOCmgr_tbl_delete(char *key) 
{
	int ret_val = -1;

	glob_locmgr_hash_tbl_delete_count ++;
	g_mutex_lock(nkn_LOCmgr_mutex);
	g_hash_table_remove(nkn_LOCmgr_hash_table, key);
	g_mutex_unlock(nkn_LOCmgr_mutex);

	return ret_val;
}

void *
c_LOCmgr_tbl_get(char *key) 
{
	void *ret_val = NULL;

	g_mutex_lock(nkn_LOCmgr_mutex);
	ret_val = g_hash_table_lookup(nkn_LOCmgr_hash_table, key);
	g_mutex_unlock(nkn_LOCmgr_mutex);

	return ret_val;
}

