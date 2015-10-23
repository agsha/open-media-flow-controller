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
#include "nkn_locmgr2_container.h"
#include "nkn_locmgr_extent.h"
#include "nkn_locmgr2_physid.h"
#include "nkn_diskmgr2_local.h"
#include "nkn_diskmgr2_disk_api.h"
#include "nkn_assert.h"
#include "diskmgr2_locking.h"
#include "diskmgr2_common.h"
#include "zvm_api.h"

#ifdef FAKE_PATCHED_GLIB2
// Fake g_hash_table_foreach_remove_n()
guint g_hash_table_foreach_remove_n (GHashTable *hash_table, GHRFunc func, gpointer user_data, gint num_entries) { return (guint)0; }
#endif

FILE *nkn_locmgr2_container_log_fd = NULL;
FILE *nkn_locmgr2_container_dump_fd = NULL;

int64_t glob_dm2_conthead_hash_tbl_total_cnt,
    NOZCC_DECL(glob_dm2_conthead_hash_tbl_create_cnt),
    glob_dm2_conthead_hash_tbl_create_err,
    glob_dm2_conthead_hash_tbl_delete_cnt,
    glob_dm2_conthead_hash_tbl_delete_err,
    glob_dm2_conthead_dup_entry,
    glob_dm2_cache_info_delete_retry_cnt,
    glob_dm2_conthead_lock_read_wait_cnt,
    glob_dm2_conthead_lock_write_wait_cnt;

int glob_dm2_max_single_loop_del_entries = 1000;

void dm2_conthead_tbl_print(void);
static dm2_container_head_t *dm2_conthead_rawget_by_uri_dir(
					const char	*in_uri_dir,
					dm2_cache_type_t *ct);

static void
s_print_obj(const gpointer key,
	    const gpointer value,
	    const gpointer user_data __attribute((unused)))
{
    assert(key != NULL);
    //    char	   mem[2048];
    dm2_container_t *c = (dm2_container_t *) value;

    printf("%s \n", c->c_uri_dir);
    //    fwrite(&mem, 1, strlen(mem), nkn_locmgr2_container_dump_fd);
}	/* s_print_obj */


void
dm2_conthead_tbl_print(void)
{
    int i;
    dm2_cache_type_t *ct;

    for (i = 0; i < glob_dm2_num_cache_types; i++) {
	ct = dm2_get_cache_type(i);
	g_hash_table_foreach(ct->ct_dm2_conthead_hash_table,
			     s_print_obj, NULL);
    }
    return;
}	/* nkn_locmgr2_container_tbl_print */


#if 0
void
nkn_locmgr2_container_tbl_cleanup(void)
{
    g_hash_table_destroy(nkn_locmgr2_container_hash_table);
#if 0
    fclose(nkn_locmgr2_container_dump_fd);
    //fclose(nkn_locmgr2_container_log_fd);
#endif
}
#endif


int
dm2_conthead_tbl_init(dm2_cache_type_t *ct)
{
    char mutex_name[40];
    int ret = 0, macro_ret;

    snprintf(mutex_name, 40, "%s.dm2_ch_hashtbl-mutex", ct->ct_name);;
    NKN_MUTEX_INITR(&ct->ct_dm2_conthead_hash_table_mutex, NULL, mutex_name);
    if (ret != 0) {
	DBG_DM2S("[cache_type=%s] Mutex init: %d", ct->ct_name, ret);
	return -EINVAL;
    }
    ct->ct_dm2_conthead_hash_table =
	g_hash_table_new(g_str_hash, g_str_equal);
    if (ct->ct_dm2_conthead_hash_table == NULL) {
	return -EINVAL;
    }
    return 0;
}	/* dm2_conthead_tbl_init */

int
dm2_conthead_tbl_insert(const char	 *key,
			const void	 *value,
			dm2_cache_type_t *ct)
{
    dm2_container_head_t *cont_head;
    int macro_ret;

    NKN_MUTEX_LOCKR(&ct->ct_dm2_conthead_hash_table_mutex);
    glob_dm2_conthead_hash_tbl_create_cnt++;
    ct->ct_dm2_conthead_hash_table_create_cnt++;

    cont_head = g_hash_table_lookup(ct->ct_dm2_conthead_hash_table, key);
    if (cont_head) {
	NKN_MUTEX_UNLOCKR(&ct->ct_dm2_conthead_hash_table_mutex);
	glob_dm2_conthead_hash_tbl_create_err++;
	ct->ct_dm2_conthead_hash_table_create_err++;
	return -EEXIST;
    }
    /* Replaces content if already present */
    g_hash_table_insert(ct->ct_dm2_conthead_hash_table,
			(char *)key, (void *)value);

    ct->ct_dm2_conthead_hash_table_total_cnt =
	g_hash_table_size(ct->ct_dm2_conthead_hash_table);
    NKN_MUTEX_UNLOCKR(&ct->ct_dm2_conthead_hash_table_mutex);
    return 0;
}	/* dm2_conthead_tbl_insert */


#if 0
void
nkn_locmgr2_container_tbl_delete(const char *key)
{
    glob_dm2_conthead_hash_tbl_delete_count++;

    PTHREAD_MUTEX_LOCK(&nkn_locmgr2_container_mutex);
    g_hash_table_remove(nkn_locmgr2_container_hash_table, key);
    PTHREAD_MUTEX_UNLOCK(&nkn_locmgr2_container_mutex);
}	/* nkn_locmgr2_container_tbl_delete */
#endif


#if 0
/* Unsafe */
static void *
dm2_conthead_tbl_get(const char *key,
		     dm2_cache_type_t *ct)
{
    void *ret_val = NULL;
    int macro_ret;

    NKN_MUTEX_LOCKR(&ct->ct_dm2_conthead_hash_table_mutex);
    ret_val = g_hash_table_lookup(ct->ct_dm2_conthead_hash_table, key);
    NKN_MUTEX_UNLOCKR(&ct->ct_dm2_conthead_hash_table_mutex);

    return ret_val;
}	/* dm2_conthead_tbl_get */
#endif

static dm2_container_head_t *
dm2_conthead_rawget_by_uri_dir(const char	*in_uri_dir,
			       dm2_cache_type_t *ct)
{
    dm2_container_head_t *ch;

    if (in_uri_dir == NULL) {
	DBG_DM2S("in_uri_dir == NULL");
	NKN_ASSERT(in_uri_dir);
	return NULL;
    }
    ch = (dm2_container_head_t *)g_hash_table_lookup(
					ct->ct_dm2_conthead_hash_table,
					in_uri_dir);
    return ch;
}	/* dm2_conthead_rawget_by_uri_dir */


#if 0
/* Unsafe */
dm2_container_head_t *
dm2_conthead_get_by_uri_dir(const char	     *in_uri_dir,
			    dm2_cache_type_t *ct)
{
    dm2_container_head_t *ch;

    if (in_uri_dir == NULL) {
	DBG_DM2S("in_uri_dir == NULL");
	NKN_ASSERT(in_uri_dir);
	return NULL;
    }
    ch = (dm2_container_head_t *)dm2_conthead_tbl_get(in_uri_dir, ct);
    return ch;
}	/* dm2_conthead_get_by_uri_dir */
#endif

dm2_container_head_t *
dm2_conthead_get_by_uri_dir_rlocked(const char	     *in_uri_dir,
				    dm2_cache_type_t *ct,
				    int		     *ch_locked,
				    int		     *err_ret,
				    int		     is_blocking)
{
    dm2_container_head_t *ch;
    int ret = 0;
    int macro_ret;

    if (in_uri_dir == NULL) {
	DBG_DM2S("in_uri_dir == NULL");
	NKN_ASSERT(in_uri_dir);
	if (err_ret)
	    *err_ret = EINVAL;
	return NULL;
    }
    NKN_MUTEX_LOCKR(&ct->ct_dm2_conthead_hash_table_mutex);
    while ((ch = dm2_conthead_rawget_by_uri_dir(in_uri_dir, ct)) != 0) {
	if (ch->ch_rwlock_nwriters > 0 || ch->ch_rwlock_wait_nwriters > 0 ||
		ch->ch_rwlock_wait_nreaders > 0) {
	    /* Cannot get lock.  Normally go to sleep */
	    if (is_blocking) {
		ch->ch_rwlock_wait_nreaders++;
		glob_dm2_conthead_lock_read_wait_cnt++;
		PTHREAD_COND_WAIT(&ch->ch_rwlock_read_cond,
				  &ct->ct_dm2_conthead_hash_table_mutex.lock);
		continue;
	    } else {
		if (err_ret) {
		    *err_ret = EBUSY;
		    ch = NULL;
		}
		break;
	    }
	}
	assert(ch->ch_rwlock_wait_nreaders == 0);
	ch->ch_rwlock_nreaders++;
	ch->ch_rwlock_lock_type = DM2_CONT_HEAD_RLOCKED;
	*ch_locked = DM2_CONT_HEAD_RLOCKED;
	if (unlikely(dm2_show_locks))
	    DBG_DM2S("[ct=%s][pth=%ld/%ld] conthead get by uri_dir RLOCK: %s",
		     ct->ct_name, pthread_self(), gettid(), in_uri_dir);
	break;
    }
 end:
    NKN_MUTEX_UNLOCKR(&ct->ct_dm2_conthead_hash_table_mutex);
    return ch;
}	/* dm2_conthead_get_by_uri_dir_rlocked */


dm2_container_head_t *
dm2_conthead_get_by_uri_dir_wlocked(const char		*in_uri_dir,
				    dm2_cache_type_t	*ct,
				    int			*ch_locked)
{
    dm2_container_head_t *ch;
    int macro_ret;

    if (in_uri_dir == NULL) {
	DBG_DM2S("in_uri_dir == NULL");
	NKN_ASSERT(in_uri_dir);
	return NULL;
    }
    NKN_MUTEX_LOCKR(&ct->ct_dm2_conthead_hash_table_mutex);
    while ((ch = dm2_conthead_rawget_by_uri_dir(in_uri_dir, ct)) != 0) {
	if (ch->ch_rwlock_nwriters > 0 || ch->ch_rwlock_nreaders > 0) {
	    glob_dm2_conthead_lock_write_wait_cnt++;
	    ch->ch_rwlock_wait_nwriters++;
	    PTHREAD_COND_WAIT(&ch->ch_rwlock_write_cond,
			      &ct->ct_dm2_conthead_hash_table_mutex.lock);
	    continue;
	}
	ch->ch_rwlock_nwriters = 1;
	ch->ch_rwlock_lock_type = DM2_CONT_HEAD_WLOCKED;
	ch->ch_rwlock_wowner = gettid();
	*ch_locked = DM2_CONT_HEAD_WLOCKED;
	//NKN_MUTEX_UNLOCKR(&ct->ct_dm2_conthead_hash_table_mutex);

	if (unlikely(dm2_show_locks))
	    DBG_DM2S("[ct=%s][pth=%ld/%ld] conthead get by uri_dir WLOCK: %s",
		     ct->ct_name, pthread_self(), gettid(), in_uri_dir);
	break;
    }
    NKN_MUTEX_UNLOCKR(&ct->ct_dm2_conthead_hash_table_mutex);
    return ch;
}	/* dm2_conthead_get_by_uri_dir_wlocked */


static gboolean
cmp_match_cache_info_delete_cont_from_list(gpointer key __attribute((unused)),
					   gpointer value,
					   gpointer user_data)
{
    dm2_container_head_t *cont_head = (dm2_container_head_t *)value;
    dm2_cache_match_t	 *cm = (dm2_cache_match_t *)user_data;
    dm2_cache_info_t	 *find_ci = cm->cm_ci;
    dm2_cache_type_t	 *find_ct = cm->cm_ct;
    GList		 *cont_obj = NULL;
    dm2_container_t	 *cont;
    ns_stat_token_t	 ns_stoken;
    int			 ch_locked = DM2_CONT_HEAD_UNLOCKED;

    /*
     * The conthead lock may not be necessary because this function is only
     * called after the entire cache_info object is removed from the
     * cache_info list which means it cannot be accessed by other threads
     */
    dm2_conthead_rwlock_wlock_unsafe(cont_head, find_ct, &ch_locked);
    cont_obj = cont_head->ch_cont_list;
    for (; cont_obj; cont_obj = cont_obj->next) {
	cont = (dm2_container_t *)cont_obj->data;
	if (cont && (cont->c_dev_ci == find_ci)) {
	    DBG_DM2M3("Deleting: %s", cont->c_uri_dir);
	    ns_stoken = dm2_ns_get_stat_token_from_uri_dir(cont->c_uri_dir);
	    dm2_delete_container(cont, find_ct, ns_stoken);
	    ns_stat_free_stat_token(ns_stoken);
	    cont_head->ch_cont_list =
		g_list_delete_link(cont_head->ch_cont_list, cont_obj);

	    if (g_list_length(cont_head->ch_cont_list) == 0) {
		/* Build up a list of empty container heads */
		find_ct->ct_del_cont_head_list =
		    g_list_prepend(find_ct->ct_del_cont_head_list, cont_head);
		dm2_conthead_rwlock_wunlock_unsafe(cont_head, &ch_locked);
		return TRUE;
	    }
	    dm2_conthead_rwlock_wunlock_unsafe(cont_head, &ch_locked);
	    return FALSE;
	}
    }
    dm2_conthead_rwlock_wunlock_unsafe(cont_head, &ch_locked);
    return FALSE;
}	/* cmp_match_cache_info_delete_cont_from_list */


int
dm2_conthead_remove_cache_info(dm2_cache_info_t *ci,
			       dm2_cache_type_t *ct)
{
    dm2_cache_match_t	 cm;
    GList		 *list;
    dm2_container_head_t *cont_head;
    int			 num_removed, total_num_removed = 0, macro_ret;

    cm.cm_ci = ci;
    cm.cm_ct = ct;

    NKN_ASSERT(ct->ct_del_cont_head_list == NULL);

retry:
    NKN_MUTEX_LOCKR(&ct->ct_dm2_conthead_hash_table_mutex);
    num_removed =
	g_hash_table_foreach_remove_n(ct->ct_dm2_conthead_hash_table,
		cmp_match_cache_info_delete_cont_from_list, (void *)&cm,
		glob_dm2_max_single_loop_del_entries);
    total_num_removed += num_removed;
    if (num_removed >= glob_dm2_max_single_loop_del_entries) {
	NKN_MUTEX_UNLOCKR(&ct->ct_dm2_conthead_hash_table_mutex);
	glob_dm2_cache_info_delete_retry_cnt++;
	/* Need to sleep here to give the stat threads some priority
	 * as they will be waiting for a read lock which has lower priority
	 * over the write lock that this thread will hold
	 */
	usleep(10000);
	goto retry;
    }

    for (list = ct->ct_del_cont_head_list; list; list = list->next) {
	cont_head = (dm2_container_head_t *)list->data;
	dm2_free(cont_head->ch_uri_dir,
		 cont_head->ch_uri_dir_len, DM2_NO_ALIGN);
	cont_head->ch_uri_dir = NULL;
	dm2_conthead_rwlock_destroy(cont_head);
	cont_head->ch_uri_dir_len = -1;
	dm2_free(cont_head, sizeof(dm2_container_head_t), DM2_NO_ALIGN);
    }
    g_list_free(ct->ct_del_cont_head_list);
    ct->ct_del_cont_head_list = NULL;
    NKN_MUTEX_UNLOCKR(&ct->ct_dm2_conthead_hash_table_mutex);
    return total_num_removed;
}	/* dm2_conthead_remove_cache_info */
