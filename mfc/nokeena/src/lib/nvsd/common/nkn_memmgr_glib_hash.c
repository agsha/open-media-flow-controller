#include <stdio.h>
#include <glib.h>
#include "nkn_hash_map.h"
#include "assert.h"
#include "nkn_defs.h"

static GHashTable *nkn_memmgr_hash_table = NULL;
static GMutex *nkn_memmgr_mutex = NULL;
FILE *nkn_memmgr_dump_fd = NULL;

#define NKN_DEBUG_LVL_ON 0

#if (NKN_DEBUG_LVL_ON == 0)
#define __NKN_FUNC_ENTRY__ printf("\n%s(): ENTRY", __FUNCTION__); 
#define __NKN_FUNC_EXIT__ printf("\n%s(): ENTRY", __FUNCTION__); 
#else
#define __NKN_FUNC_ENTRY__  
#define __NKN_FUNC_EXIT__  
#endif

int glob_memmgr_tbl_inserts = 0;
int glob_memmgr_tbl_deletes = 0;
int glob_memmgr_tbl_inits = 0;

void memmgr_dump_counters(void);
void c_memmgr_tbl_cleanup(void);

void
memmgr_dump_counters(void)
{
	printf("\nglob_memmgr_tbl_inserts = %d", glob_memmgr_tbl_inserts);
	printf("\nglob_memmgr_tbl_deletes = %d", glob_memmgr_tbl_deletes);
	printf("\nglob_memmgr_tbl_inits = %d", glob_memmgr_tbl_inits);
}


static void
s_print_obj(
	gpointer key, 
	gpointer value, 
	gpointer user_data)
{
	UNUSED_ARGUMENT(user_data);

	fprintf(nkn_memmgr_dump_fd, "MEMMGR: KEY: %s, Value: %p\n", (char *)key, value);
}

void
c_memmgr_tbl_print()
{
        g_hash_table_foreach(nkn_memmgr_hash_table, s_print_obj, NULL);

        return;
}

void
c_memmgr_tbl_cleanup(void)
{
        fclose(nkn_memmgr_dump_fd);

	return;
}

void
c_memmgr_tbl_init()
{
	FILE *log_file1 = NULL;

	glob_memmgr_tbl_inits ++;

	nkn_memmgr_hash_table = g_hash_table_new(g_str_hash, g_str_equal);
	assert(nkn_memmgr_hash_table);

	assert(nkn_memmgr_mutex == NULL);
	nkn_memmgr_mutex = g_mutex_new();

        log_file1 = fopen("./nkn_memmgr_dump.txt" ,"a+");
        assert(log_file1);
        nkn_memmgr_dump_fd = log_file1;	
}

int
c_memmgr_tbl_insert(char *key, void *value)
{
	glob_memmgr_tbl_inserts ++;
	g_mutex_lock(nkn_memmgr_mutex);
	g_hash_table_insert(nkn_memmgr_hash_table, key, value);
	g_mutex_unlock(nkn_memmgr_mutex);
	
	return 0;
}

int
c_memmgr_tbl_delete(char *key) 
{
	int ret_val = -1;

	glob_memmgr_tbl_deletes ++;
	g_mutex_lock(nkn_memmgr_mutex);
	g_hash_table_remove(nkn_memmgr_hash_table, key);
	g_mutex_unlock(nkn_memmgr_mutex);

	return ret_val;
}

void *
c_memmgr_tbl_get(char *key) 
{
	void *ret_val = NULL;

	g_mutex_lock(nkn_memmgr_mutex);
	ret_val = g_hash_table_lookup(nkn_memmgr_hash_table, key);
	g_mutex_unlock(nkn_memmgr_mutex);

	return ret_val;
}

