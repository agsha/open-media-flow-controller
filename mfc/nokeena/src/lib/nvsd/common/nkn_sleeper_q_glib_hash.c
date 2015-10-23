#include <stdio.h>
#include <glib.h>
#include "nkn_hash_map.h"
#include "assert.h"

static GHashTable *nkn_sleeper_hash_table = NULL;
static GMutex *nkn_sleeper_q_mutex = NULL;

#define NKN_DEBUG_LVL_ON 0

#if (NKN_DEBUG_LVL_ON == 0)
#define __NKN_FUNC_ENTRY__ printf("\n%s(): ENTRY", __FUNCTION__); 
#define __NKN_FUNC_EXIT__ printf("\n%s(): ENTRY", __FUNCTION__); 
#else
#define __NKN_FUNC_ENTRY__  
#define __NKN_FUNC_EXIT__  
#endif

int glob_sleeper_q_inits = 0;
int glob_sleeper_q_inserts = 0;
int glob_sleeper_q_deletes = 0;

void sleeperq_dump_counters(void);

void
sleeperq_dump_counters(void)
{
	printf("\nglob_sleeper_q_inits = %d", glob_sleeper_q_inits);
	printf("\nglob_sleeper_q_inserts = %d", glob_sleeper_q_inserts);
	printf("\nglob_sleeper_q_deletes = %d", glob_sleeper_q_deletes);
}

void
c_sleeper_q_tbl_print()
{
	return;
}

#if 0
static void
s_print_obj(
	gpointer key, 
	gpointer value, 
	gpointer user_data)
{
	printf("\n%s(): KEY: %s, Value: %s", (char *)key, (char *)value);
}
#endif

void
c_sleeper_q_tbl_init()
{
	__NKN_FUNC_ENTRY__;

	glob_sleeper_q_inits ++;

	nkn_sleeper_hash_table = g_hash_table_new(g_str_hash, g_str_equal);
	assert(nkn_sleeper_hash_table);

	assert(nkn_sleeper_q_mutex == NULL);
	nkn_sleeper_q_mutex = g_mutex_new();
	
}

int
c_sleeper_q_tbl_insert(char *key, void *value)
{
	glob_sleeper_q_inserts ++;
	
	g_mutex_lock(nkn_sleeper_q_mutex);
	g_hash_table_insert(nkn_sleeper_hash_table, key, value);
	g_mutex_unlock(nkn_sleeper_q_mutex);
	
	return 0;
}

int
c_sleeper_q_tbl_delete(char *key) 
{
	int ret_val = -1;

	glob_sleeper_q_deletes ++;

	g_mutex_lock(nkn_sleeper_q_mutex);
	g_hash_table_remove(nkn_sleeper_hash_table, key);
	g_mutex_unlock(nkn_sleeper_q_mutex);

	return ret_val;
}

void *
c_sleeper_q_tbl_get(char *key) 
{
	//void *ret_val = NULL;
	gboolean ret_val = FALSE;
	gpointer orig_key = NULL;
	gpointer orig_value = NULL;

	g_mutex_lock(nkn_sleeper_q_mutex);
	//ret_val = g_hash_table_lookup(nkn_sleeper_hash_table, key);
	ret_val = g_hash_table_lookup_extended(nkn_sleeper_hash_table, key, &orig_key, &orig_value);
	g_mutex_unlock(nkn_sleeper_q_mutex);

	//return ret_val;
	if (ret_val == TRUE)
		return orig_value;
	else	
		return NULL;
}

