#include <stdio.h>
#include <glib.h>
#include "nkn_hash_map.h"
#include "assert.h"
#include <malloc.h>

#include "nkn_defs.h"

static GHashTable *nkn_diskio_hash_table = NULL;
static GMutex *nkn_diskio_mutex = NULL;
FILE *nkn_diskio_dump_fd = NULL;

#define NKN_DEBUG_LVL_ON 0

#if (NKN_DEBUG_LVL_ON == 0)
#define __NKN_FUNC_ENTRY__ printf("\n%s(): ENTRY", __FUNCTION__); 
#define __NKN_FUNC_EXIT__ printf("\n%s(): ENTRY", __FUNCTION__); 
#else
#define __NKN_FUNC_ENTRY__  
#define __NKN_FUNC_EXIT__  
#endif

int glob_diskio_tbl_inserts = 0;
int glob_diskio_tbl_deletes = 0;
int glob_diskio_tbl_inits = 0;

void diskio_dump_counters(void);
void c_diskio_tbl_cleanup(void);

void
diskio_dump_counters(void)
{
	printf("\nglob_diskio_tbl_inserts = %d", glob_diskio_tbl_inserts);
	printf("\nglob_diskio_tbl_deletes = %d", glob_diskio_tbl_deletes);
	printf("\nglob_diskio_tbl_inits = %d", glob_diskio_tbl_inits);
}


static void
s_print_obj(
	gpointer key, 
	gpointer value, 
	gpointer user_data)
{
	UNUSED_ARGUMENT(user_data);

	fprintf(nkn_diskio_dump_fd, "MEMMGR: KEY: %s, Value: %p\n", (char *)key, value);
}

void
c_diskio_tbl_print()
{
        g_hash_table_foreach(nkn_diskio_hash_table, s_print_obj, NULL);

        return;
}

void
c_diskio_tbl_cleanup(void)
{
        fclose(nkn_diskio_dump_fd);
}

void
c_diskio_tbl_init()
{
	FILE *log_file1 = NULL;

	glob_diskio_tbl_inits ++;

	nkn_diskio_hash_table = g_hash_table_new_full(g_str_hash, g_str_equal, free, NULL);
	assert(nkn_diskio_hash_table);

	assert(nkn_diskio_mutex == NULL);
	nkn_diskio_mutex = g_mutex_new();

        log_file1 = fopen("./nkn_diskio_dump.txt" ,"a+");
        assert(log_file1);
        nkn_diskio_dump_fd = log_file1;	
}

int
c_diskio_tbl_insert(char *key, void *value)
{
	glob_diskio_tbl_inserts ++;
	g_mutex_lock(nkn_diskio_mutex);
        assert(NULL == g_hash_table_lookup(nkn_diskio_hash_table, key));
	g_hash_table_insert(nkn_diskio_hash_table, key, value);
        assert(value == g_hash_table_lookup(nkn_diskio_hash_table, key));
	g_mutex_unlock(nkn_diskio_mutex);
	
	return 0;
}

int
c_diskio_tbl_delete(char *key) 
{
	int ret_val = -1;

	glob_diskio_tbl_deletes ++;
	g_mutex_lock(nkn_diskio_mutex);
	ret_val = g_hash_table_remove(nkn_diskio_hash_table, key);
        assert (NULL == g_hash_table_lookup(nkn_diskio_hash_table,key));
	g_mutex_unlock(nkn_diskio_mutex);

	return ret_val;
}

void *
c_diskio_tbl_get(char *key) 
{
	void *ret_val = NULL;

	g_mutex_lock(nkn_diskio_mutex);
	ret_val = g_hash_table_lookup(nkn_diskio_hash_table, key);
	g_mutex_unlock(nkn_diskio_mutex);

	return ret_val;
}

