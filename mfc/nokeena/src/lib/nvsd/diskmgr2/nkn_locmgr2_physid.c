/*
 *	COPYRIGHT: NOKEENA NETWORKS
 *
 * This file contains code which implements the Disk Manager
 *
 * Author: Michael Nishimoto
 *
 * Non-obvious coding conventions:
 *	- All functions should begin with dm2_
 *	- All functions have a name comment at the end of the function.
 *	- All log messages use special logging macros
 *
 */
#include <stdio.h>
#include <glib.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "nkn_locmgr2_physid.h"
#include "nkn_locmgr2_extent.h"
#include "nkn_diskmgr2_local.h"
#include "nkn_debug.h"
#include "nkn_assert.h"
#include "diskmgr2_locking.h"
#include "diskmgr2_common.h"
#include "zvm_api.h"

FILE *nkn_locmgr2_physid_log_fd = NULL;
FILE *nkn_locmgr2_physid_dump_fd = NULL;

int64_t NOZCC_DECL(glob_dm2_physid_hash_tbl_total_cnt),
    NOZCC_DECL(glob_dm2_physid_hash_tbl_create_cnt),
    glob_dm2_physid_hash_tbl_create_err,
    glob_dm2_physid_hash_tbl_delete_cnt,
    glob_dm2_physid_dup_entry,
    glob_dm2_physid_hash_tbl_ext_remove_cnt,
    NOZCC_DECL(glob_dm2_extent_total_cnt);

/* Don't want to include nkn_locmgr2_extent.h */
void dm2_print_mem_extent(void *value);
void dm2_physid_dump_counters(void);
void nkn_locmgr2_physid_tbl_print(void);


void
dm2_physid_dump_counters(void)
{
    printf("glob_dm2_physid_hash_tbl_total_cnt = %ld\n",
	   glob_dm2_physid_hash_tbl_total_cnt);
    printf("glob_dm2_physid_hash_tbl_create_cnt = %ld\n",
	   glob_dm2_physid_hash_tbl_create_cnt);
    printf("glob_dm2_physid_hash_tbl_delete_cnt = %ld\n",
	   glob_dm2_physid_hash_tbl_delete_cnt);
    printf("glob_dm2_physid_dup_entry = %ld\n",
	   glob_dm2_physid_dup_entry);
    printf("glob_dm2_extent_total_cnt = %ld\n",
	   glob_dm2_extent_total_cnt);
}


static void
s_print_list(gpointer data,
	     gpointer user_data __attribute__((unused)))
{
    dm2_print_mem_extent(data);
}


static void
s_print_obj(const gpointer key,
	    const gpointer value,
	    const gpointer user_data __attribute__((unused)))
{
    DM2_physid_head_t *ph;

    ph = (DM2_physid_head_t *) value;

    assert(key != NULL);
    g_list_foreach(ph->ph_ext_list, s_print_list, NULL);
}


void
nkn_locmgr2_physid_tbl_print(void)
{
    unsigned i;
    dm2_cache_type_t *ct;

    printf("URI off len physid start_sec start_attr cont_off hash\n");
    for (i = 0; i < glob_dm2_num_cache_types; i++) {
	ct = dm2_get_cache_type(i);
	g_hash_table_foreach(ct->ct_dm2_physid_hash_table, s_print_obj, NULL);
    }
}


#if 0
void
nkn_locmgr2_physid_tbl_cleanup(void)
{
    NKN_ASSERT(0);
#if 0
    fclose(nkn_locmgr2_physid_dump_fd);
    //fclose(nkn_locmgr2_physid_log_fd);
#endif
#if 0
    g_hash_table_foreach(nkn_locmgr2_physid_hash_table,
			 s_delete_extent_elements,
			 NULL);
#endif
    g_hash_table_destroy(nkn_locmgr2_physid_hash_table);
}
#endif

/*
g_int64_hash() and g_int64_equal() are in glib 2.22.5 (CentOS 6.3)
but not in glib 2.12.3 (CentOS 5.7)
*/
#ifndef CENTOS_6
static inline guint
g_int64_hash(gconstpointer v)
{
    const guint32 w1 = ((*(const uint64_t *)v) & 0xffffffff);
    const guint32 w2 = ((*(const uint64_t *)v) & 0xffffffff00000000) >> 32;

    return (w1 ^ w2);
}

static inline gboolean
g_int64_equal(gconstpointer v1,
	      gconstpointer v2)
{
    return *((const uint64_t *)v1) == *((const uint64_t *)v2);
}
#endif /* not CENTOS_6 */

int
dm2_physid_tbl_init(dm2_cache_type_t *ct)
{
    char mutex_name[40];
    int ret = 0, macro_ret;

    snprintf(mutex_name, 40, "%s.dm2_physidtbl-rwlock", ct->ct_name);
    NKN_RWLOCK_INIT(&ct->ct_dm2_physid_hash_table_rwlock, NULL, mutex_name);
    if (ret != 0) {
       DBG_DM2S("[cache_type=%s] Mutex init: %d", ct->ct_name, ret);
       return -EINVAL;
    }

    ct->ct_dm2_physid_hash_table =
	g_hash_table_new(g_int64_hash, g_int64_equal);
    if (ct->ct_dm2_physid_hash_table == NULL) {
	return -EINVAL;
    }
    return 0;
}	/* dm2_physid_tbl_init */


/*
 * Return a positive number if the first argument should be sorted after
 * the second argument.
 */
static int
cmp_smallest_dm2_extent_start_sector_first(gconstpointer in_a,
					   gconstpointer in_b)
{
    DM2_extent_t *a = (DM2_extent_t *)in_a;
    DM2_extent_t *b = (DM2_extent_t *)in_b;

    if (a->ext_start_sector > b->ext_start_sector)
	return 1;
    else
	return 0;
}	/* cmp_smallest_dm2_extent_start_sector_first */


/*
 * Insert an object into the physid hash table
 * - the key could be made a (void *), but then we would lose type checking.
 * - we will be inserting multiple objects for the same key.  Hence, we
 *   need to keep these objects in a list.
 *
 * I'd like to avoid calling calloc() with the lock held, but I don't know
 * at this point if it would be much better to call unlock/relock around
 * the calloc.
 */
int
dm2_physid_tbl_ext_insert(const uint64_t   *key,
			  const void       *value,
			  dm2_cache_type_t *ct)
{
    DM2_physid_head_t *physid_head;
    int done = 0;

    NKN_RWLOCK_WLOCK(&ct->ct_dm2_physid_hash_table_rwlock);
    glob_dm2_physid_hash_tbl_create_cnt++;
    ct->ct_dm2_physid_hash_table_create_cnt++;

    physid_head = g_hash_table_lookup(ct->ct_dm2_physid_hash_table, key);
    if (physid_head == NULL) {
	physid_head = dm2_calloc(1, sizeof(DM2_physid_head_t),
				 mod_DM2_physid_head_t);
	if (physid_head == NULL) {
	    ct->ct_dm2_physid_hash_table_create_err++;
	    glob_dm2_physid_hash_tbl_create_err++;
	    NKN_RWLOCK_UNLOCK(&ct->ct_dm2_physid_hash_table_rwlock);
	    DBG_DM2S("dm2_calloc failed: %zd", sizeof(DM2_physid_head_t));
	    return -ENOMEM;
	}
	dm2_physid_rwlock_init(physid_head);
	memcpy(&physid_head->ph_physid, key, sizeof(physid_head->ph_physid));
	physid_head->ph_ext_list = g_list_append(NULL, (void *)value);
	g_hash_table_insert(ct->ct_dm2_physid_hash_table,
			    (char *)&physid_head->ph_physid,
			    (void *)physid_head);
	done++;
    }
    glob_dm2_extent_total_cnt++;
    glob_dm2_physid_hash_tbl_total_cnt =
	ct->ct_dm2_physid_hash_table_total_cnt =
	g_hash_table_size(ct->ct_dm2_physid_hash_table);

    NKN_ASSERT(physid_head->ph_physid == *key);
    NKN_RWLOCK_UNLOCK(&ct->ct_dm2_physid_hash_table_rwlock);
    /* This is safe to do as long as physid_head is never removed */
    if (physid_head && !done) {
	dm2_physid_rwlock_wlock(physid_head);
	physid_head->ph_ext_list =
	    g_list_insert_sorted(physid_head->ph_ext_list, (void *)value,
				 cmp_smallest_dm2_extent_start_sector_first);
	dm2_physid_rwlock_wunlock(physid_head);
    }

    return 0;
}	/* dm2_physid_tbl_ext_insert */


int
dm2_physid_tbl_ext_remove(const uint64_t     *physid_key,
			  const DM2_extent_t *ext_data,
			  dm2_cache_type_t   *ct,
			  int		     *ph_empty)
{
    DM2_physid_head_t *ph;

    NKN_RWLOCK_WLOCK(&ct->ct_dm2_physid_hash_table_rwlock);
    glob_dm2_physid_hash_tbl_ext_remove_cnt++;
    ct->ct_dm2_physid_hash_table_ext_remove_cnt++;
    ph = g_hash_table_lookup(ct->ct_dm2_physid_hash_table, physid_key);
    if (ph) {
	glob_dm2_extent_total_cnt--;
	glob_dm2_physid_hash_tbl_total_cnt =
	    ct->ct_dm2_physid_hash_table_total_cnt =
	    g_hash_table_size(ct->ct_dm2_physid_hash_table);
    }
    NKN_RWLOCK_UNLOCK(&ct->ct_dm2_physid_hash_table_rwlock);
    if (ph) {
	dm2_physid_rwlock_wlock(ph);
	ph->ph_ext_list = g_list_remove(ph->ph_ext_list, ext_data);
	if (!ph->ph_ext_list && ph_empty)
	    *ph_empty = 1;
	/* XXXmiken: If we delete the physid_head when the list is empty,
	 * we would need to change how the rwlock is grabbed.  It would need
	 * to use a cond variable and look more like the URI lookup. */
	dm2_physid_rwlock_wunlock(ph);
    }

    return (ph == NULL ? -ENOENT : 0);
}	/* dm2_physid_tbl_uri_remove */


DM2_physid_head_t *
dm2_physid_tbl_get(const uint64_t   *physid_key,
		   dm2_cache_type_t *ct)
{
    void *ret_val = NULL;

    NKN_RWLOCK_RLOCK(&ct->ct_dm2_physid_hash_table_rwlock);
    ret_val = g_hash_table_lookup(ct->ct_dm2_physid_hash_table, physid_key);
    NKN_RWLOCK_UNLOCK(&ct->ct_dm2_physid_hash_table_rwlock);

    return ret_val;
}	/* dm2_physid_tbl_get */

/*
 * 'dm2_uri_physid_cnt' returns the number of unique physid (or unique blocks)
 * used by an URI.
 */
int
dm2_uri_physid_cnt(GList *uri_ext_list)
{
    GList *ext_obj;
    GHashTable *ph_hash = NULL;
    DM2_extent_t *ext;
    int count = 0;

    ph_hash = g_hash_table_new(g_int64_hash, g_int64_equal);
    if (ph_hash == NULL)
	return -1;

    for (ext_obj = uri_ext_list; ext_obj; ext_obj = ext_obj->next) {
	ext = (DM2_extent_t *)ext_obj->data;
	/* If physid already present in hash table, skip this extent */
	if (g_hash_table_lookup_extended(ph_hash,
					 (void *)&ext->ext_physid, NULL, NULL))
	    continue;
	g_hash_table_insert(ph_hash, (void *)&ext->ext_physid, NULL);
	count++;
    }

    g_hash_table_destroy(ph_hash);

    return count;
}	/* dm2_get_uri_physid_cnt */
