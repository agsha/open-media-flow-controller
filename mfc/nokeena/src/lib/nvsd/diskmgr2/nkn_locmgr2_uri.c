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

#include "nkn_locmgr2_uri.h"
#include "nkn_locmgr2_container.h"
#include "nkn_locmgr2_extent.h"
#include "nkn_locmgr2_physid.h"
#include "nkn_diskmgr2_local.h"
#include "nkn_diskmgr2_disk_api.h"
#include "nkn_debug.h"
#include "nkn_assert.h"
#include "diskmgr2_locking.h"
#include "diskmgr2_common.h"
#include "nkn_util.h"
#include "zvm_api.h"

FILE *nkn_locmgr2_uri_log_fd = NULL;
FILE *nkn_locmgr2_uri_dump_fd = NULL;

uint64_t glob_dm2_uri_hash_tbl_total_cnt,
    NOZCC_DECL(glob_dm2_uri_hash_tbl_create_cnt),
    glob_dm2_uri_lock_read_wait_cnt,
    glob_dm2_uri_lock_read_wait_timed_cnt,
    glob_dm2_uri_lock_read_wait_timedout_cnt,
    glob_dm2_uri_lock_read_wait_timed_success_cnt,
    glob_dm2_uri_lock_write_wait_cnt,
    glob_dm2_uri_hash_tbl_create_err,
    glob_dm2_uri_hash_tbl_delete_cnt,
    glob_dm2_uri_hash_tbl_steal,
    glob_dm2_stat_timeout_sec = 2;

void dm2_uri_lock_get(DM2_uri_t* uri_head);
void dm2_uri_lock_release(DM2_uri_t *uri_head);
void dm2_uri_dump_counters(void);
void nkn_locmgr2_uri_tbl_print(void);
static DM2_uri_t *dm2_uri_head_get_uri_in_tier(const char *uri_key,
					       dm2_container_head_t *ch,
					       int *ch_lock,
					       dm2_cache_type_t *ct);


void
dm2_uri_dump_counters(void)
{
    printf("glob_dm2_uri_hash_tbl_total_cnt = %ld\n",
	   glob_dm2_uri_hash_tbl_total_cnt);
    printf("glob_dm2_uri_hash_tbl_create_cnt = %ld\n",
	   glob_dm2_uri_hash_tbl_create_cnt);
    printf("glob_dm2_uri_hash_tbl_delete_cnt = %ld\n",
	   glob_dm2_uri_hash_tbl_delete_cnt);
}	/* dm2_uri_dump_counters */


static void
s_print_list(const gpointer data,
	     const gpointer user_data __attribute((unused)))
{
    DM2_extent_t *ext = (DM2_extent_t *)data;

    printf("off=%lu  len=%u  start=%lu\n",
	   ext->ext_offset, ext->ext_length, ext->ext_start_sector);
}	/* s_print_list */


static void
s_print_uri(const gpointer key,
	    const gpointer value,
	    const gpointer user_data)
{
    DM2_uri_t *uri_head = (DM2_uri_t *)value;
    char *p_basename;

    assert(key != NULL);
    p_basename = dm2_uh_uri_basename(uri_head);

    if (user_data == NULL)
	printf("uri=%s &uri=%p attr_off=%lu\n",
	       uri_head->uri_name, uri_head, uri_head->uri_at_off);
    else if (!strcmp((char *)user_data, p_basename)) {
	printf("uri: %s\n", uri_head->uri_name);
	g_list_foreach(uri_head->uri_ext_list, s_print_list, NULL);
    }
}	/* s_print_uri */


void
nkn_locmgr2_uri_tbl_print(void)
{
    unsigned i;
    dm2_cache_type_t *ct;
    GList *ci_obj;
    dm2_cache_info_t *ci;

    for (i = 0; i < glob_dm2_num_cache_types; i++) {
	ct = dm2_get_cache_type(i);
        dm2_ct_info_list_rwlock_rlock(ct);
	for (ci_obj = ct->ct_info_list; ci_obj; ci_obj = ci_obj->next) {
	    ci = (dm2_cache_info_t *)ci_obj->data;
	    printf("CACHE INFO: %s", ci->mgmt_name);
	    g_hash_table_foreach(ci->ci_uri_hash_table, s_print_uri, NULL);
	}
        dm2_ct_info_list_rwlock_runlock(ct);
    }
}	/* nkn_locmgr2_uri_tbl_print */


#if 0
void
nkn_locmgr2_uri_tbl_print_one(char *uri_base)
{
    g_hash_table_foreach(nkn_dm2_uri_hash_table, s_print_obj, uri_base);
}	/* nkn_locmgr2_uri_tbl_print */
#endif


#if 0
void
nkn_locmgr2_uri_tbl_cleanup(void)
{
    g_hash_table_destroy(nkn_dm2_uri_hash_table);
#if 0
    fclose(nkn_dm2_uri_dump_fd);
    //fclose(nkn_dm2_uri_log_fd);
#endif
}
#endif


#if 0
void
nkn_locmgr2_uri_tbl_init(void)
{
#if 0
    FILE *log_file;
    FILE *log_file1;
#endif

    nkn_locmgr2_uri_hash_table = g_hash_table_new(g_str_hash, g_str_equal);
    assert(nkn_locmgr2_uri_hash_table);

    assert(nkn_locmgr2_uri_mutex == NULL);
    PTHREAD_MUTEX_INIT(&nkn_locmgr2_uri_mutex);

#if 0
    log_file = fopen("./nkn_locmgr2_uri_log.txt" , "a+");
    assert(log_file);
    nkn_locmgr2_uri_log_fd = log_file;
    log_file1 = fopen("./nkn_locmgr2_uri_dump.txt" , "w");
    assert(log_file1);
    nkn_locmgr2_uri_dump_fd = log_file1;
#endif
}	/* nkn_locmgr2_uri_tbl_init */
#endif


int
dm2_uri_tbl_init(dm2_cache_info_t *ci)
{
    char mutex_name[40];
    int ret = 0, macro_ret;

    ci->ci_uri_hash_table = g_hash_table_new(g_str_hash, g_str_equal);
    if (ci->ci_uri_hash_table == NULL) {
	return -EINVAL;
    }
    snprintf(mutex_name, 40, "%s.dm2_ci_uri_hashtbl-mutex", ci->mgmt_name);;
    NKN_MUTEX_INITR(&ci->ci_uri_hash_table_mutex, NULL, mutex_name);
    if (ret != 0) {
        DBG_DM2S("Mutex Init: %d", ret);
        return -EINVAL;
    }

    return 0;
}	/* dm2_uri_tbl_init */


/*! Assumed that this function is called with ct mutex held */
static int
dm2_uri_tbl_insert(const char	    *key,
		   const void	    *value,
		   dm2_cache_type_t *ct __attribute((unused)))
{
    void *ret_val;
    DM2_uri_t *uri_head = (DM2_uri_t *)value;
    dm2_cache_info_t *ci = uri_head->uri_container->c_dev_ci;

    glob_dm2_uri_hash_tbl_create_cnt++;
    ci->ci_uri_hash_table_create_cnt++;

    ret_val = g_hash_table_lookup(ci->ci_uri_hash_table, (gpointer)key);
    if (ret_val) {
	glob_dm2_uri_hash_tbl_create_err++;
	ci->ci_uri_hash_table_create_err++;
	return -EINVAL;
    }

    g_hash_table_insert(ci->ci_uri_hash_table, (gpointer)key, (gpointer)value);

    ci->ci_uri_hash_table_total_cnt = g_hash_table_size(ci->ci_uri_hash_table);
    return 0;
}	/* dm2_uri_tbl_insert */


#if 0
void
nkn_locmgr2_uri_tbl_delete(const char *key)
{
    glob_dm2_uri_hash_tbl_delete_cnt++;
    PTHREAD_MUTEX_LOCK(&nkn_locmgr2_uri_mutex);
    g_hash_table_remove(nkn_locmgr2_uri_hash_table, key);
    PTHREAD_MUTEX_UNLOCK(&nkn_locmgr2_uri_mutex);
}	/* nkn_locmgr2_uri_tbl_delete */
#endif


void
dm2_uri_head_free(DM2_uri_t *uri_head,
		  dm2_cache_type_t *ct,
		  int lock)
{

    if (lock)
	dm2_ct_uri_hash_rwlock_wlock(ct);

    dm2_free_uri_head(uri_head);

    if (lock)
	dm2_ct_uri_hash_rwlock_wunlock(ct);
}


void
dm2_uri_tbl_steal_and_wakeup(DM2_uri_t *uri_head,
			     dm2_cache_type_t *ct)
{
    dm2_cache_info_t *ci = uri_head->uri_container->c_dev_ci;
    dm2_uri_lock_t *uri_lock = NULL;
    int macro_ret;

    dm2_ct_uri_hash_rwlock_wlock(ct);
    NKN_MUTEX_LOCKR(&ci->ci_uri_hash_table_mutex);
    if (uri_head->uri_lock == NULL)	// This should not happen
	dm2_uri_lock_get(uri_head);

    uri_lock = uri_head->uri_lock;
    assert(uri_lock);

    /* If the lock was just allocated, some of these asserts will fire */
    assert(uri_lock->uri_nwriters > 0);
    assert(uri_lock->uri_wowner != 0);
    assert(uri_lock->uri_nreaders == 0);
    assert(uri_head->uri_lock_type == DM2_URI_WLOCKED);
    /* There can be uri_wait_nwriters because PUT tries to grab a write lock */

    glob_dm2_uri_hash_tbl_steal++;
    if (g_hash_table_steal(ci->ci_uri_hash_table, uri_head->uri_name) == 0)
	assert(0);

    dm2_uri_log_state(uri_head, 0, DM2_URI_NLOCK, 0, gettid(), nkn_cur_ts);
    dm2_uri_log_bump_idx(uri_head);

    /* Wake up waiting threads but the objects on which they were waiting are
     * now deleted */
    uri_head->uri_lock_type = 0;
    uri_lock->uri_nwriters--;
    if (uri_lock->uri_wait_nwriters > 0) {
	PTHREAD_COND_BROADCAST(uri_lock->uri_write_cond);
	PTHREAD_COND_DESTROY(uri_lock->uri_write_cond);
	memset(uri_lock->uri_write_cond, 1, sizeof(pthread_cond_t));
	dm2_free(uri_lock->uri_write_cond,
		 sizeof(pthread_cond_t), DM2_NO_ALIGN);
	uri_lock->uri_write_cond = NULL;
	uri_lock->uri_wait_nwriters = 0;
	/* reset the use count as all the others waiting need to re-looup */
	uri_lock->in_use = 1;
    }
    if (uri_lock->uri_wait_nreaders > 0) {
	PTHREAD_COND_BROADCAST(uri_lock->uri_read_cond);
	PTHREAD_COND_DESTROY(uri_lock->uri_read_cond);
	memset(uri_lock->uri_read_cond, 2, sizeof(pthread_cond_t));
	dm2_free(uri_lock->uri_read_cond,
		 sizeof(pthread_cond_t), DM2_NO_ALIGN);
	uri_lock->uri_read_cond = NULL;
	uri_lock->uri_wait_nreaders = 0;
	/* reset the use count as all the others waiting need to re-looup */
	uri_lock->in_use = 1;
    }

    dm2_uri_lock_release(uri_head);
    NKN_MUTEX_UNLOCKR(&ci->ci_uri_hash_table_mutex);
    dm2_ct_uri_hash_rwlock_wunlock(ct);

    ci->ci_uri_hash_table_total_cnt = g_hash_table_size(ci->ci_uri_hash_table);
}	/* dm2_uri_tbl_steal_and_wakeup */


#if 0
/*
 * Remove a uri_head from the uri_head hash table
 *
 * - Called from the process which is disabling a cache.
 * - Return zero on success.
 * - Return -ERROR on error.
 */
int
dm2_uri_head_shutdown_steal(DM2_uri_t	     *uh_key,
			    dm2_cache_info_t *ci __attribute((unused)),
			    dm2_cache_type_t *ct)
{
    DM2_uri_t *uh;
    int ret = 0, macro_ret;

    NKN_MUTEX_LOCKR(&ct->ct_dm2_uri_hash_table_mutex);
    uh = g_hash_table_lookup(ct->ct_dm2_uri_hash_table, uh_key->uri_name);
    if (uh == NULL) {
	NKN_ASSERT(0);
	ret = -ENOENT;
	goto error;
    }
    NKN_ASSERT(uh == uh_key);
    ret = g_hash_table_steal(ct->ct_dm2_uri_hash_table, uh_key->uri_name);
    if (ret == 0) {
	assert(0);	//* should never happen
	ret = -EINVAL;
	goto error;
    } else
	ret = 0;

 error:
    NKN_MUTEX_UNLOCKR(&ct->ct_dm2_uri_hash_table_mutex);
    ct->ct_dm2_uri_hash_table_total_cnt =
	g_hash_table_size(ct->ct_dm2_uri_hash_table);
    return ret;
}	/* dm2_uri_head_shutdown_steal */
#endif


/*
 * dm2_uri_head_insert_by_uri
 *
 * Insert a uri_head object into a hash table by uriname.  A list of
 * extents are hanging off the uri_head object.  The uri_head object
 * was used so the uri_name would not be duplicated in every extent
 * object.
 */
int
dm2_uri_head_insert_by_uri(const DM2_uri_t  *uri_head,
			   dm2_cache_type_t *ct)
{
    int	ret;

    dm2_ct_uri_hash_rwlock_wlock(ct);
    ret = dm2_uri_tbl_insert(uri_head->uri_name, uri_head, ct);
    if (ret < 0) {
	/*
	 * This isn't necessarily bad and can happen when we have two racing
	 * DM2_put calls.  The assumption here is that the contents of each
	 * uri_head must be identical.
	 */
	DBG_DM2E("Inserting duplicate uri in uri_head hash table: %s",
		 uri_head->uri_name);;
    }
    dm2_ct_uri_hash_rwlock_wunlock(ct);
    return ret;
}	/* dm2_uri_head_insert_by_uri */


int
dm2_uri_head_lookup_by_uri(const char	      *uri_key,
			   dm2_container_head_t *cont_head_in,
			   int		      *ch_lock_in,
			   dm2_cache_type_t   *ct,
			   int		      assert)
{
    dm2_container_head_t *cont_head = cont_head_in;
    DM2_uri_t		 *uri_head;
    char		 *uri_dir;
    int			 *ch_locked = ch_lock_in;
    int			 ret = 0, uh_lock = 0;

    if (uri_key == NULL) {
	DBG_DM2S("uri_key == NULL");
	NKN_ASSERT(uri_key);
	return -EINVAL;
    }
    uri_dir = alloca(strlen(uri_key)+1);
    strcpy(uri_dir, uri_key);
    dm2_uri_dirname(uri_dir, 0);

    if (cont_head == NULL) {
        cont_head = dm2_conthead_get_by_uri_dir_rlocked(uri_dir, ct, ch_locked,
							NULL, DM2_BLOCKING);
        if (!cont_head)
            return -ENOENT;
    }
    assert(*ch_locked != DM2_CONT_HEAD_UNLOCKED);

    dm2_ct_uri_hash_rwlock_rlock(ct, &uh_lock);

    uri_head = dm2_uri_head_get_uri_in_tier(uri_key, cont_head,
					    ch_locked, ct);
    if (uri_head) {
	if (uri_head->uri_container->c_dev_ci->ci_disabling)
	    ret = -EBUSY;
	else
	    ret = 0;
    } else
	ret = -ENOENT;
    dm2_ct_uri_hash_rwlock_runlock(ct, &uh_lock);
    if (assert)
	NKN_ASSERT(ret != 0);

    assert(!uh_lock);
    if (cont_head_in == NULL)
	dm2_conthead_rwlock_unlock(cont_head, ct, ch_locked);
    return ret;
}	/* dm2_uri_head_lookup_by_uri */


int
dm2_uri_head_raw_lookup_by_uri(const char	*uri_key,
			       dm2_cache_type_t *ct,
			       dm2_cache_info_t *ci,
			       int		assert)
{
    DM2_uri_t	*uri_head;
    int		ret, uh_lock = 0;

    if (uri_key == NULL) {
	DBG_DM2S("uri_key == NULL");
	NKN_ASSERT(uri_key);
	return 0;
    }

    dm2_ct_uri_hash_rwlock_rlock(ct, &uh_lock);

    uri_head = g_hash_table_lookup(ci->ci_uri_hash_table, uri_key);
    if (uri_head)
	ret = 1;
    else
	ret = 0;

    dm2_ct_uri_hash_rwlock_runlock(ct, &uh_lock);

    assert(!uh_lock);
    if (assert)
	NKN_ASSERT(ret != 0);
    return ret;
}	/* dm2_uri_head_raw_lookup_by_uri */


/*
 * Used only for debugging purposes, so no need to grab a lock
 */
int
dm2_uri_head_match_by_uri_head(const DM2_uri_t *uri_head_key,
			       dm2_cache_type_t *ct)
{
    DM2_uri_t *uri_head;
    dm2_cache_info_t *ci = uri_head_key->uri_container->c_dev_ci;
    int ret, uh_lock = 0;

    if (uri_head_key == NULL) {
	DBG_DM2S("uri_head_key == NULL");
	NKN_ASSERT(uri_head_key);
	return 0;
    }

    dm2_ct_uri_hash_rwlock_rlock(ct, &uh_lock);

    uri_head = g_hash_table_lookup(ci->ci_uri_hash_table,
				   uri_head_key->uri_name);
    if (uri_head == uri_head_key)
	ret = 1;
    else
	ret = 0;

    dm2_ct_uri_hash_rwlock_runlock(ct, &uh_lock);

    assert(!uh_lock);
    NKN_ASSERT(ret != 0);
    return ret;
}	/* dm2_uri_head_match_by_uri_head */


static DM2_uri_t *
dm2_uri_head_get_uri_in_tier(const char		  *uri_key,
			     dm2_container_head_t *cont_head,
			     int    *ch_locked __attribute((unused)),
			     dm2_cache_type_t	  *ct __attribute((unused)))
{
    dm2_cache_info_t	 *ci;
    dm2_container_t	 *cont;
    DM2_uri_t		 *uri_head = NULL;
    GList		 *cont_list, *cptr;
    char		 *uri_name, *p_basename;

    p_basename = dm2_uri_basename((char *)uri_key);
    uri_name = alloca(DM2_MAX_BASENAME_ALLOC_LEN);

    cont_list = cont_head->ch_cont_list;
    for (cptr = cont_list; cptr; cptr = cptr->next) {
	cont = (dm2_container_t *)cptr->data;
	ci = cont->c_dev_ci;
	if (ci->ci_disabling)
	    continue;

	snprintf(uri_name, DM2_MAX_BASENAME_ALLOC_LEN, "%lx/%s",
		 (long)cont, p_basename);
	uri_head = g_hash_table_lookup(ci->ci_uri_hash_table, uri_name);
	if (uri_head)
	    break;
    }

    return uri_head;
}	/* dm2_uri_head_get_uri_in_tier */

/*
 * Use this function when needing to get access to a read-only URI object.
 * This function should only be called within the STAT & GET paths.  If the
 * object is getting deleted, then we return that the object has already been
 * deleted after releasing the conditional variable.
 *
 * Note: A thread can be woken up spuriously.  When this happens, all the
 *  predicates must be rechecked.  In particular, if a waiting writer was
 *  just woken up in write_unlock but a waiting reader was spuriously woken
 *  up (bug 2810), the reader will go back to sleep because uri_wait_nreaders
 *  must be greater than zero.
 */
struct DM2_uri *
dm2_uri_head_get_by_uri_rlocked(const char	   *uri_key,
				dm2_container_head_t *cont_head_in,
				int		   *ch_lock_in,
				dm2_cache_type_t   *ct,
				int		   is_blocking,
				int		   lock_ci,
				int		   *err_ret)
{
    dm2_container_head_t *cont_head = cont_head_in;
    dm2_cache_info_t *ci = NULL;
    DM2_uri_t		 *uri_head;
    dm2_uri_lock_t	 *uri_lock = NULL;
    struct timespec	 ats;
    char		 *uri_dir;
    int			 *ch_locked = ch_lock_in, ret;
    int			 macro_ret, ct_uri_hash_lock = 0;

    if (uri_key == NULL) {
	DBG_DM2S("uri_key == NULL");
	NKN_ASSERT(uri_key);
	return NULL;
    }

    uri_dir = alloca(strlen(uri_key)+1);
    strcpy(uri_dir, uri_key);
    dm2_uri_dirname(uri_dir, 0);

    if (cont_head == NULL) {
	cont_head = dm2_conthead_get_by_uri_dir_rlocked(uri_dir, ct, ch_locked,
							err_ret, is_blocking);
	if (!cont_head)
	    return NULL;
    }
    assert(*ch_locked != DM2_CONT_HEAD_UNLOCKED);

    dm2_ct_uri_hash_rwlock_rlock(ct, &ct_uri_hash_lock);
    while ((uri_head = dm2_uri_head_get_uri_in_tier(uri_key, cont_head,
						    ch_locked ,ct))) {
	ci = uri_head->uri_container->c_dev_ci;
	/* Grab mutex before releasing tier lock to prevent delete */
	NKN_MUTEX_LOCKR(&ci->ci_uri_hash_table_mutex);
	dm2_ct_uri_hash_rwlock_runlock(ct, &ct_uri_hash_lock);
	if (!uri_lock) {
	    dm2_uri_lock_get(uri_head);
	    uri_lock = uri_head->uri_lock;
	}
	assert(uri_lock);
	if (uri_head->uri_container->c_dev_ci->ci_disabling) {
	    /* This URI exists on a disk which is getting disabled.  Hence,
	     * treat this object as not existing. */
	    dm2_uri_lock_release(uri_head);
	    uri_head = NULL;
	    NKN_MUTEX_UNLOCKR(&ci->ci_uri_hash_table_mutex);
	    break;
	} else if (uri_lock->uri_nwriters > 0 ||
		   uri_lock->uri_wait_nwriters > 0 ||
		   uri_lock->uri_wait_nreaders > 0) {
	    uri_lock->uri_wait_nreaders++;
	    ct->ct_uri_rd_wait_times++;			// Can race
	    if (uri_lock->uri_read_cond == NULL) {
		uri_lock->uri_read_cond = dm2_calloc(1, sizeof(pthread_cond_t),
						     mod_dm2_pthread_cond_t);
		assert(uri_lock->uri_read_cond != NULL);
		PTHREAD_COND_INIT(uri_lock->uri_read_cond, NULL);
	    }
	    /* If non-blocking operation is requested (specifically for STATs),
	     * bail out here as there are some writers waiting */
	    if (!is_blocking) {
		ats.tv_sec = nkn_cur_ts + glob_dm2_stat_timeout_sec;
		ats.tv_nsec = 0;
		glob_dm2_uri_lock_read_wait_timed_cnt++;
		PTHREAD_COND_TIMED_WAIT(uri_lock->uri_read_cond,
					&ci->ci_uri_hash_table_mutex.lock,
					&ats);
		ret = macro_ret;
		if (ret == ETIMEDOUT) {
		    glob_dm2_uri_lock_read_wait_timedout_cnt++;

		    /* ETIMEDOUT can be returned even if someone wakes up the
		     * condition. In this case, the uri_lock will be freed up
		     * and uri_head->uri_lock will be NULL. Need to release
		     * thre lock only if uri_head->uri_lock is the one that 
		     * we had before the calling the timed_wait
		     */
		    if (uri_head->uri_lock && uri_head->uri_lock == uri_lock)
			dm2_uri_lock_release(uri_head);

		    if (err_ret)
			*err_ret = ret;
		    uri_head = NULL;
		    NKN_MUTEX_UNLOCKR(&ci->ci_uri_hash_table_mutex);
		    break;
		}
		/* Variable to track wakeup without any timeout */
		glob_dm2_uri_lock_read_wait_timed_success_cnt++;
	    } else {
		/* If blocking, use 'pthread_cond_wait' as 'timed_wait'
		 * would SEGV with a NULL argument for timespec */
		glob_dm2_uri_lock_read_wait_cnt++;
		PTHREAD_COND_WAIT(uri_lock->uri_read_cond,
					&ci->ci_uri_hash_table_mutex.lock);
	    }

	    NKN_MUTEX_UNLOCKR(&ci->ci_uri_hash_table_mutex);
	    dm2_ct_uri_hash_rwlock_rlock(ct, &ct_uri_hash_lock);
	    uri_lock = NULL;
	} else {
	    assert(uri_lock->uri_wait_nreaders == 0);
	    uri_lock->uri_nreaders++;
	    ct->ct_uri_rd_lock_times++;			// Can race
	    dm2_uri_log_state(uri_head, 0, DM2_URI_RLOCK, 0, 0, nkn_cur_ts);
	    uri_head->uri_lock_type = DM2_URI_RLOCKED;
	    NKN_MUTEX_UNLOCKR(&ci->ci_uri_hash_table_mutex);
	    break;
	}
    }
    if (ct_uri_hash_lock)
	dm2_ct_uri_hash_rwlock_runlock(ct, &ct_uri_hash_lock);

    assert(!ct_uri_hash_lock);
    if (uri_head) {
	assert(uri_head->uri_container->c_dev_ci);
	if (lock_ci == DM2_RLOCK_CI)
	    dm2_ci_dev_rwlock_rlock(uri_head->uri_container->c_dev_ci);
    }

    if (cont_head_in == NULL)
	dm2_conthead_rwlock_unlock(cont_head, ct, ch_locked);
    return uri_head;
}	/* dm2_uri_head_get_by_uri_rlocked */

struct DM2_uri *
dm2_uri_head_get_by_ci_uri_wlocked(const char	    *uri_key,
				   dm2_container_head_t *cont_head_in,
				   int		    *ch_locked_in,
				   dm2_container_t  *cont,
				   dm2_cache_info_t *ci,
				   dm2_cache_type_t *ct,
				   int		    is_delete)
{
    dm2_container_head_t *cont_head = cont_head_in;
    DM2_uri_t            *uri_head = NULL;
    dm2_uri_lock_t	 *uri_lock = NULL;
    char                 *uri_dir, *p_basename, *uri_name;
    int                  *ch_locked = ch_locked_in;
    int			 macro_ret, ct_uri_hash_lock = 0;

    if (uri_key == NULL) {
	DBG_DM2S("uri_key == NULL");
	NKN_ASSERT(uri_key);
	return NULL;
    }

    if (ci->ci_disabling)
	return NULL;

    uri_dir = alloca(strlen(uri_key)+1);
    strcpy(uri_dir, uri_key);
    dm2_uri_dirname(uri_dir, 0);

    if (cont_head == NULL) {
        cont_head = dm2_conthead_get_by_uri_dir_rlocked(uri_dir, ct, ch_locked,
							NULL, DM2_BLOCKING);
        if (!cont_head)
	    return NULL;
    }
    assert(*ch_locked != DM2_CONT_HEAD_UNLOCKED);

    uri_name = alloca(DM2_MAX_BASENAME_ALLOC_LEN);
    p_basename = dm2_uri_basename((char *)uri_key);
    snprintf(uri_name, DM2_MAX_BASENAME_ALLOC_LEN, "%lx/%s",
	     (long)cont, p_basename);

    dm2_ct_uri_hash_rwlock_rlock(ct, &ct_uri_hash_lock);
    while ((uri_head = g_hash_table_lookup(ci->ci_uri_hash_table, uri_name))) {
	/* Grab mutex before releasing tier lock to prevent delete */
	NKN_MUTEX_LOCKR(&ci->ci_uri_hash_table_mutex);
	dm2_ct_uri_hash_rwlock_runlock(ct, &ct_uri_hash_lock);
	if (!uri_lock) {
	    dm2_uri_lock_get(uri_head);
	    uri_lock = uri_head->uri_lock;
	}
	assert(uri_lock);
	if (uri_lock->uri_nwriters > 0 || uri_lock->uri_wait_nwriters > 0 ||
	    uri_lock->uri_nreaders > 0) {

	    assert(!pthread_equal(uri_lock->uri_wowner, gettid()));
	    uri_lock->uri_wait_nwriters++;
	    glob_dm2_uri_lock_write_wait_cnt++;
	    if (uri_lock->uri_write_cond == NULL) {
		uri_lock->uri_write_cond = dm2_calloc(1, sizeof(pthread_cond_t),
						      mod_dm2_pthread_cond_t);
		assert(uri_lock->uri_write_cond != NULL);
		PTHREAD_COND_INIT(uri_lock->uri_write_cond, NULL);
	    }

	    PTHREAD_COND_WAIT(uri_lock->uri_write_cond,
			      &ci->ci_uri_hash_table_mutex.lock);
	    NKN_MUTEX_UNLOCKR(&ci->ci_uri_hash_table_mutex);
            dm2_ct_uri_hash_rwlock_rlock(ct, &ct_uri_hash_lock);
	    uri_lock = NULL;
	} else if (uri_head->uri_container->c_dev_ci->ci_disabling) {
	    /* This URI exists on a disk which is getting disabled.  Hence,
	     * treat this object as not existing. */
	    dm2_uri_lock_release(uri_head);
	    uri_head = NULL;
	    NKN_MUTEX_UNLOCKR(&ci->ci_uri_hash_table_mutex);
	    break;
	} else {
	    uri_lock->uri_nwriters = 1;
	    uri_head->uri_deleting = is_delete;
	    dm2_uri_log_state(uri_head, 0, DM2_URI_WLOCK, 0, 0, nkn_cur_ts);
	    uri_head->uri_lock_type = DM2_URI_WLOCKED;
	    uri_lock->uri_wowner = gettid();
	    NKN_MUTEX_UNLOCKR(&ci->ci_uri_hash_table_mutex);
	    break;
	}
    }
    if (ct_uri_hash_lock)
	dm2_ct_uri_hash_rwlock_runlock(ct, &ct_uri_hash_lock);

    assert(!ct_uri_hash_lock);
    if (cont_head_in == NULL)
	dm2_conthead_rwlock_unlock(cont_head, ct, ch_locked);
    if (uri_head)
	assert(uri_head->uri_container->c_dev_ci);

    return uri_head;
}	/* dm2_uri_head_get_by_ci_uri_wlocked */


struct DM2_uri *
dm2_uri_head_get_by_ci_uri_rlocked(const char	    *uri_key,
				   dm2_container_head_t *cont_head_in,
				   int		    *ch_locked_in,
				   dm2_container_t  *cont,
				   dm2_cache_info_t *ci,
				   dm2_cache_type_t *ct)
{
    dm2_container_head_t *cont_head = cont_head_in;
    DM2_uri_t            *uri_head = NULL;
    dm2_uri_lock_t	 *uri_lock = NULL;
    char                 *uri_dir, *p_basename, *uri_name;
    int                  *ch_locked = ch_locked_in;
    int			 macro_ret, ct_uri_hash_lock = 0;

    if (uri_key == NULL) {
	DBG_DM2S("uri_key == NULL");
	NKN_ASSERT(uri_key);
	return NULL;
    }

    if (ci->ci_disabling)
	return NULL;

    uri_dir = alloca(strlen(uri_key)+1);
    strcpy(uri_dir, uri_key);
    dm2_uri_dirname(uri_dir, 0);

    if (cont_head == NULL) {
        cont_head = dm2_conthead_get_by_uri_dir_rlocked(uri_dir, ct, ch_locked,
							NULL, DM2_BLOCKING);
        if (!cont_head)
	    return NULL;
    }
    assert(*ch_locked != DM2_CONT_HEAD_UNLOCKED);

    uri_name = alloca(DM2_MAX_BASENAME_ALLOC_LEN);
    p_basename = dm2_uri_basename((char *)uri_key);
    snprintf(uri_name, DM2_MAX_BASENAME_ALLOC_LEN, "%lx/%s",
	     (long)cont, p_basename);

    dm2_ct_uri_hash_rwlock_rlock(ct, &ct_uri_hash_lock);
    while ((uri_head = g_hash_table_lookup(ci->ci_uri_hash_table, uri_name))) {
	/* Grab mutex before releasing tier lock to prevent delete */
	NKN_MUTEX_LOCKR(&ci->ci_uri_hash_table_mutex);
	dm2_ct_uri_hash_rwlock_runlock(ct, &ct_uri_hash_lock);
	if (!uri_lock) {
	    dm2_uri_lock_get(uri_head);
	    uri_lock = uri_head->uri_lock;
	}
	assert(uri_lock);
	if (uri_lock->uri_nwriters > 0 || uri_lock->uri_wait_nwriters > 0 ||
	    uri_lock->uri_wait_nreaders > 0) {

	    uri_lock->uri_wait_nreaders++;
	    glob_dm2_uri_lock_read_wait_cnt++;
	    ct->ct_uri_rd_wait_times++;			// Can race
	    if (uri_lock->uri_read_cond == NULL) {
		uri_lock->uri_read_cond = dm2_calloc(1, sizeof(pthread_cond_t),
						     mod_dm2_pthread_cond_t);
		assert(uri_lock->uri_read_cond != NULL);
		PTHREAD_COND_INIT(uri_lock->uri_read_cond, NULL);
	    }
            PTHREAD_COND_WAIT(uri_lock->uri_read_cond,
                              &ci->ci_uri_hash_table_mutex.lock);
	    NKN_MUTEX_UNLOCKR(&ci->ci_uri_hash_table_mutex);
            dm2_ct_uri_hash_rwlock_rlock(ct, &ct_uri_hash_lock);
	    uri_lock = NULL;
	} else if (uri_head->uri_container->c_dev_ci->ci_disabling) {
	    /* This URI exists on a disk which is getting disabled.  Hence,
	     * treat this object as not existing. */
	    dm2_uri_lock_release(uri_head);
	    NKN_MUTEX_UNLOCKR(&ci->ci_uri_hash_table_mutex);
	    uri_head = NULL;
	    break;
	} else {
	    assert(uri_lock->uri_wait_nreaders == 0);
	    uri_lock->uri_nreaders++;
	    ct->ct_uri_rd_lock_times++;			// Can race
	    dm2_uri_log_state(uri_head, 0, DM2_URI_RLOCK, 0, 0, nkn_cur_ts);
	    uri_head->uri_lock_type = DM2_URI_RLOCKED;
	    NKN_MUTEX_UNLOCKR(&ci->ci_uri_hash_table_mutex);
	    break;
	}
    }
    if (ct_uri_hash_lock)
	dm2_ct_uri_hash_rwlock_runlock(ct, &ct_uri_hash_lock);
    assert(!ct_uri_hash_lock);

    if (cont_head_in == NULL)
	dm2_conthead_rwlock_unlock(cont_head, ct, ch_locked);
    if (uri_head)
	assert(uri_head->uri_container->c_dev_ci);

    return uri_head;
}	/* dm2_uri_head_get_by_ci_uri_rlocked */


static void
dm2_uri_head_rwlock_do_runlock(DM2_uri_t	*uri_head,
			       dm2_cache_type_t *ct __attribute((unused)),
			       int		flag)
{
    dm2_uri_lock_t *uri_lock = NULL;
    dm2_cache_info_t *ci = uri_head->uri_container->c_dev_ci;
    int macro_ret;

    NKN_MUTEX_LOCKR(&ci->ci_uri_hash_table_mutex);
    uri_lock = uri_head->uri_lock;
    assert(uri_lock);

    assert(ci);
    assert(uri_lock->uri_nreaders > 0);
    assert(uri_lock->uri_wowner == 0);
    assert(uri_lock->uri_nwriters == 0);
    assert(uri_head->uri_lock_type == DM2_URI_RLOCKED);

    uri_lock->uri_nreaders--;
    if (uri_lock->uri_nreaders == 0) {
	dm2_uri_log_state(uri_head, 0, DM2_URI_NLOCK, 0, 0, nkn_cur_ts);
	dm2_uri_log_bump_idx(uri_head);
	uri_head->uri_lock_type = 0;
	if (uri_lock->uri_wait_nwriters > 0) {
	    PTHREAD_COND_BROADCAST(uri_lock->uri_write_cond);
	    PTHREAD_COND_DESTROY(uri_lock->uri_write_cond);
	    memset(uri_lock->uri_write_cond, 0x74, sizeof(pthread_cond_t));
	    dm2_free(uri_lock->uri_write_cond,
		     sizeof(pthread_cond_t), DM2_NO_ALIGN);
	    uri_lock->uri_write_cond = NULL;
	    uri_lock->uri_wait_nwriters = 0;
	    /* reset the use count as all the others waiting need to re-looup */
	    uri_lock->in_use = 1;
	}
    }

    dm2_uri_lock_release(uri_head);
    NKN_MUTEX_UNLOCKR(&ci->ci_uri_hash_table_mutex);

    if (flag == DM2_RUNLOCK_CI)
	dm2_ci_dev_rwlock_runlock(ci);
}	/* dm2_uri_head_rwlock_do_runlock */


void
dm2_uri_head_rwlock_runlock(DM2_uri_t	*uri_head,
			    dm2_cache_type_t *ct,
			    int flag)
{

    dm2_ct_uri_hash_rwlock_rlock(ct, NULL);
    dm2_uri_head_rwlock_do_runlock(uri_head, ct, flag);
    dm2_ct_uri_hash_rwlock_runlock(ct, NULL);
}	/* dm2_uri_head_rwlock_runlock */


struct DM2_uri *
dm2_uri_head_get_by_uri_wlocked(const char	   *uri_key,
				dm2_container_head_t *cont_head_in,
				int		   *ch_lock_in,
				dm2_cache_type_t   *ct,
				int		   is_delete,
				int		   lock_ci)
{
    dm2_container_head_t *cont_head = cont_head_in;
    dm2_cache_info_t	 *ci = NULL;
    DM2_uri_t		 *uri_head;
    dm2_uri_lock_t	 *uri_lock = NULL;
    char		 *uri_dir;
    int			 *ch_locked = ch_lock_in;
    int			 macro_ret, ct_uri_hash_lock = 0;

    if (uri_key == NULL) {
	DBG_DM2S("uri_key == NULL");
	NKN_ASSERT(uri_key);
	return NULL;
    }

    uri_dir = alloca(strlen(uri_key)+1);
    strcpy(uri_dir, uri_key);
    dm2_uri_dirname(uri_dir, 0);

    if (cont_head == NULL) {
	cont_head = dm2_conthead_get_by_uri_dir_rlocked(uri_dir, ct, ch_locked,
							NULL, DM2_BLOCKING);
	if (!cont_head)
	    return NULL;
    }
    assert(*ch_locked != DM2_CONT_HEAD_UNLOCKED);

    dm2_ct_uri_hash_rwlock_rlock(ct, &ct_uri_hash_lock);
    while ((uri_head = dm2_uri_head_get_uri_in_tier(uri_key, cont_head,
						    ch_locked ,ct))) {

	ci = uri_head->uri_container->c_dev_ci;
	NKN_MUTEX_LOCKR(&ci->ci_uri_hash_table_mutex);
	dm2_ct_uri_hash_rwlock_runlock(ct, &ct_uri_hash_lock);
	if (!uri_lock) {
	    dm2_uri_lock_get(uri_head);
	    uri_lock = uri_head->uri_lock;
	}
	assert(uri_lock);
	if (ci->ci_disabling) {
	    /* This URI exists on a disk which is getting disabled.  Hence,
	     * treat this object as not existing. */
	    dm2_uri_lock_release(uri_head);
	    uri_head = NULL;
	    NKN_MUTEX_UNLOCKR(&ci->ci_uri_hash_table_mutex);
	    break;
	} else if (uri_lock->uri_nwriters > 0 ||
		   uri_lock->uri_wait_nwriters > 0 ||
		   uri_lock->uri_nreaders > 0) {

	    assert(!pthread_equal(uri_lock->uri_wowner, gettid()));
	    glob_dm2_uri_lock_write_wait_cnt++;
	    uri_lock->uri_wait_nwriters++;
	    ct->ct_uri_wr_wait_times++;			// Can race
	    if (uri_lock->uri_write_cond == NULL) {
		uri_lock->uri_write_cond = dm2_calloc(1, sizeof(pthread_cond_t),
						      mod_dm2_pthread_cond_t);
		assert(uri_lock->uri_write_cond != NULL);
		PTHREAD_COND_INIT(uri_lock->uri_write_cond, NULL);
	    }
	    PTHREAD_COND_WAIT(uri_lock->uri_write_cond,
			      &ci->ci_uri_hash_table_mutex.lock);
	    NKN_MUTEX_UNLOCKR(&ci->ci_uri_hash_table_mutex);
            dm2_ct_uri_hash_rwlock_rlock(ct, &ct_uri_hash_lock);
	    uri_lock = NULL;
	} else {
	    uri_lock->uri_nwriters = 1;
	    ct->ct_uri_wr_lock_times++;			// Can race
	    dm2_uri_log_state(uri_head, 0, DM2_URI_WLOCK, 0, gettid(),
			      nkn_cur_ts);
	    uri_head->uri_lock_type = DM2_URI_WLOCKED;
	    uri_lock->uri_wowner = gettid();
	    uri_head->uri_deleting = is_delete;
	    NKN_MUTEX_UNLOCKR(&ci->ci_uri_hash_table_mutex);
	    break;
	}
    }
    if (ct_uri_hash_lock)
	dm2_ct_uri_hash_rwlock_runlock(ct, &ct_uri_hash_lock);

    assert(!ct_uri_hash_lock);
    if (uri_head) {
	assert(uri_head->uri_container->c_dev_ci);
	if (lock_ci == DM2_RLOCK_CI)
	    dm2_ci_dev_rwlock_rlock(uri_head->uri_container->c_dev_ci);
    }

    if (cont_head_in == NULL)
	dm2_conthead_rwlock_unlock(cont_head, ct, ch_locked);

    return uri_head;
}	/* dm2_uri_head_get_by_uri_wlocked */


void
dm2_uri_head_rwlock_wlock(DM2_uri_t	    *uri_head,
			  dm2_cache_type_t  *ct,
			  int		    is_delete)
{
    int macro_ret;
    int ct_uri_hash_locked = 0;

    dm2_uri_lock_t *uri_lock = NULL;
    dm2_cache_info_t *ci = uri_head->uri_container->c_dev_ci;

    dm2_ct_uri_hash_rwlock_rlock(ct, &ct_uri_hash_locked);

    while (1) {
	/* Grab mutex before releasing tier lock to prevent delete */
	NKN_MUTEX_LOCKR(&ci->ci_uri_hash_table_mutex);
	dm2_ct_uri_hash_rwlock_runlock(ct, &ct_uri_hash_locked);
	if (!uri_lock) {
	    dm2_uri_lock_get(uri_head);
	    uri_lock = uri_head->uri_lock;
	}
	assert(uri_lock);
	if (uri_lock->uri_nwriters > 0 || uri_lock->uri_wait_nwriters > 0 ||
	    uri_lock->uri_nreaders > 0) {

	    assert(!pthread_equal(uri_lock->uri_wowner, gettid()));
	    glob_dm2_uri_lock_write_wait_cnt++;
	    uri_lock->uri_wait_nwriters++;
	    ct->ct_uri_wr_wait_times++;			// Can race
	    if (uri_lock->uri_write_cond == NULL) {
		uri_lock->uri_write_cond = dm2_calloc(1, sizeof(pthread_cond_t),
						      mod_dm2_pthread_cond_t);
		assert(uri_lock->uri_write_cond != NULL);
		PTHREAD_COND_INIT(uri_lock->uri_write_cond, NULL);
	    }
	    PTHREAD_COND_WAIT(uri_lock->uri_write_cond,
                              &ci->ci_uri_hash_table_mutex.lock);
	    NKN_MUTEX_UNLOCKR(&ci->ci_uri_hash_table_mutex);
	    dm2_ct_uri_hash_rwlock_rlock(ct, &ct_uri_hash_locked);
	    uri_lock = NULL;
	} else {
	    uri_lock->uri_nwriters = 1;
	    ct->ct_uri_wr_lock_times++;			// Can race
	    dm2_uri_log_state(uri_head, 0, DM2_URI_WLOCK, 0,
			      gettid(), nkn_cur_ts);
	    uri_head->uri_lock_type = DM2_URI_WLOCKED;
	    uri_lock->uri_wowner = gettid();
	    uri_head->uri_deleting = is_delete;
	    NKN_MUTEX_UNLOCKR(&ci->ci_uri_hash_table_mutex);
	    break;
	}
    }
    assert(!ct_uri_hash_locked);
    assert(uri_head->uri_lock);
    return;
}	/* dm2_uri_head_get_by_uri_wlocked */


static void
dm2_uri_head_rwlock_do_wunlock(DM2_uri_t	*uri_head,
			       dm2_cache_type_t *ct __attribute((unused)),
			       int		flag)
{
    int macro_ret;
    dm2_uri_lock_t *uri_lock = NULL;
    dm2_cache_info_t *ci = uri_head->uri_container->c_dev_ci;

    NKN_MUTEX_LOCKR(&ci->ci_uri_hash_table_mutex);
    uri_lock = uri_head->uri_lock;
    assert(uri_lock);

    assert(uri_lock->uri_nwriters == 1);
    assert(uri_lock->uri_wowner != 0);
    assert(uri_lock->uri_nreaders == 0);
    assert(uri_head->uri_lock_type == DM2_URI_WLOCKED);

    assert(pthread_equal(uri_lock->uri_wowner, gettid()));

    dm2_uri_log_state(uri_head, 0, DM2_URI_NLOCK, 0, gettid(), nkn_cur_ts);
    dm2_uri_log_bump_idx(uri_head);
    uri_head->uri_lock_type = 0;
    uri_lock->uri_nwriters = 0;
    uri_lock->uri_wowner = 0;
    if (uri_lock->uri_wait_nwriters > 0) {
	PTHREAD_COND_BROADCAST(uri_lock->uri_write_cond);
	PTHREAD_COND_DESTROY(uri_lock->uri_write_cond);
	memset(uri_lock->uri_write_cond, 0x42, sizeof(pthread_cond_t));
	dm2_free(uri_lock->uri_write_cond,
		 sizeof(pthread_cond_t), DM2_NO_ALIGN);
	uri_lock->uri_write_cond = NULL;
	uri_lock->uri_wait_nwriters = 0;
	/* reset the use count as all the others waiting need to re-looup */
	uri_lock->in_use = 1;
    } else if (uri_lock->uri_wait_nreaders > 0) {
	PTHREAD_COND_BROADCAST(uri_lock->uri_read_cond);
	PTHREAD_COND_DESTROY(uri_lock->uri_read_cond);
	memset(uri_lock->uri_read_cond, 0x51, sizeof(pthread_cond_t));
	dm2_free(uri_lock->uri_read_cond,
		 sizeof(pthread_cond_t), DM2_NO_ALIGN);
	uri_lock->uri_read_cond = NULL;
	uri_lock->uri_wait_nreaders = 0;
	/* reset the use count as all the others waiting need to re-looup */
	uri_lock->in_use = 1;
    }

    dm2_uri_lock_release(uri_head);
    NKN_MUTEX_UNLOCKR(&ci->ci_uri_hash_table_mutex);

    if (flag == DM2_RUNLOCK_CI)
        dm2_ci_dev_rwlock_runlock(ci);
}	/* dm2_uri_head_rwlock_do_wunlock */


void
dm2_uri_head_rwlock_wunlock(DM2_uri_t *uri_head,
			    dm2_cache_type_t *ct,
			    int	flag)
{
    dm2_ct_uri_hash_rwlock_rlock(ct, NULL);
    dm2_uri_head_rwlock_do_wunlock(uri_head, ct, flag);
    dm2_ct_uri_hash_rwlock_runlock(ct, NULL);
}	/* dm2_uri_head_rwlock_wunlock */


void
dm2_uri_head_rwlock_unlock(DM2_uri_t	    *uri_head,
			   dm2_cache_type_t *ct,
			   int		    flag)
{
    dm2_ct_uri_hash_rwlock_rlock(ct, NULL);
    if (uri_head->uri_lock_type == DM2_URI_RLOCKED)
	dm2_uri_head_rwlock_do_runlock(uri_head, ct, flag);
    else if (uri_head->uri_lock_type == DM2_URI_WLOCKED)
	dm2_uri_head_rwlock_do_wunlock(uri_head, ct, flag);
    else {
	DBG_DM2S("[cache_type=%s] Trying to unlock URI (%s) with lock type %d",
		 ct->ct_name, uri_head->uri_name, uri_head->uri_lock_type);
	assert(0);
    }
    dm2_ct_uri_hash_rwlock_runlock(ct, NULL);
}	/* dm2_uri_head_rwlock_unlock */

DM2_uri_t *
dm2_uri_head_get_ci_by_uri_wlocked(const char	 *uri_key,
				dm2_cache_type_t *ct,
				dm2_container_t	 *cont,
				dm2_cache_info_t *ci,
				int		 is_delete)
{
    DM2_uri_t	*uri_head;
    char	*uri_name, *p_basename;
    dm2_uri_lock_t  *uri_lock = NULL;;
    int		macro_ret, ct_uri_hash_lock = 0;

    if (uri_key == NULL) {
	DBG_DM2S("uri_key == NULL");
	NKN_ASSERT(uri_key);
	return NULL;
    }

    if (ci->ci_disabling)
	return NULL;

    uri_name = alloca(DM2_MAX_BASENAME_ALLOC_LEN);
    p_basename = dm2_uri_basename((char *)uri_key);

    snprintf(uri_name, DM2_MAX_BASENAME_ALLOC_LEN, "%lx/%s",
	     (long)cont, p_basename);

    dm2_ct_uri_hash_rwlock_rlock(ct, &ct_uri_hash_lock);
    while ((uri_head = g_hash_table_lookup(ci->ci_uri_hash_table, uri_name))) {
	/* Grab mutex before releasing tier lock to prevent delete */
	NKN_MUTEX_LOCKR(&ci->ci_uri_hash_table_mutex);
	dm2_ct_uri_hash_rwlock_runlock(ct, &ct_uri_hash_lock);;
	if (!uri_lock) {
	    dm2_uri_lock_get(uri_head);
	    uri_lock = uri_head->uri_lock;
	}
	assert(uri_lock);
	if (uri_lock->uri_nwriters > 0 || uri_lock->uri_wait_nwriters > 0 ||
	    uri_lock->uri_nreaders > 0) {

	    assert(!pthread_equal(uri_lock->uri_wowner, gettid()));
	    glob_dm2_uri_lock_write_wait_cnt++;
	    uri_lock->uri_wait_nwriters++;
	    ct->ct_uri_wr_wait_times++;			// Can race
	    if (uri_lock->uri_write_cond == NULL) {
		uri_lock->uri_write_cond = dm2_calloc(1, sizeof(pthread_cond_t),
						      mod_dm2_pthread_cond_t);
		assert(uri_lock->uri_write_cond != NULL);
		PTHREAD_COND_INIT(uri_lock->uri_write_cond, NULL);
	    }
	    PTHREAD_COND_WAIT(uri_lock->uri_write_cond,
                              &ci->ci_uri_hash_table_mutex.lock);
	    NKN_MUTEX_UNLOCKR(&ci->ci_uri_hash_table_mutex);
            dm2_ct_uri_hash_rwlock_rlock(ct, &ct_uri_hash_lock);
	    uri_lock = NULL;
	} else if (uri_head->uri_container->c_dev_ci->ci_disabling) {
	    /* This URI exists on a disk which is getting disabled.  Hence,
	     * treat this object as not existing. */
	    dm2_uri_lock_release(uri_head);
	    uri_head = NULL;
	    NKN_MUTEX_UNLOCKR(&ci->ci_uri_hash_table_mutex);
	    break;
	} else {
	    uri_lock->uri_nwriters = 1;
	    ct->ct_uri_wr_lock_times++;			// Can race
	    dm2_uri_log_state(uri_head, 0, DM2_URI_WLOCK, 0,
			      gettid(), nkn_cur_ts);
	    uri_head->uri_lock_type = DM2_URI_WLOCKED;
	    uri_lock->uri_wowner = gettid();
	    uri_head->uri_deleting = is_delete;
	    NKN_MUTEX_UNLOCKR(&ci->ci_uri_hash_table_mutex);
	    break;
	}
    }
    if (ct_uri_hash_lock)
	dm2_ct_uri_hash_rwlock_runlock(ct, &ct_uri_hash_lock);

    assert(!ct_uri_hash_lock);
    if (uri_head)
	assert(uri_head->uri_container->c_dev_ci);
    return uri_head;
}	/* dm2_uri_head_get_ci_by_uri_wlocked */



#if 0
static void
cmp_match_cache_info_find_uri(gpointer key __attribute((unused)),
			      gpointer value,
			      gpointer user_data)
{
    DM2_uri_t		*uh = (DM2_uri_t *)value;
    dm2_cache_match_t	*cm = (dm2_cache_match_t *)user_data;
    dm2_cache_info_t	*find_ci = cm->cm_ci;
    dm2_cache_type_t	*find_ct = cm->cm_ct;

    if (uh->uri_container->c_dev_ci == find_ci) {
	DBG_DM2S("[cache_type=%s] Found URI after remove_disk: (%s)/%p",
		 find_ct->ct_name, uh->uri_name, uh);
	cm->cm_count++;
    }
}	/* cmp_match_cache_info_find_uri */


int
dm2_uri_head_verify_empty(dm2_cache_info_t *ci,
			  dm2_cache_type_t *ct)
{
    int macro_ret;
    dm2_cache_match_t cm;

    cm.cm_ci = ci;
    cm.cm_ct = ct;

    NKN_MUTEX_LOCKR(&ct->ct_dm2_uri_hash_table_mutex);
    g_hash_table_foreach(ct->ct_dm2_uri_hash_table,
			 cmp_match_cache_info_find_uri, (void *)&cm);
    NKN_MUTEX_UNLOCKR(&ct->ct_dm2_uri_hash_table_mutex);
    return cm.cm_count;
}	/* dm2_uri_head_verify_empty */
#endif


static gboolean
cmp_match_cache_info_delete_uri(gpointer key __attribute((unused)),
                                gpointer value,
                                gpointer user_data)
{
    DM2_uri_t           *uh = (DM2_uri_t *)value;
    dm2_cache_match_t   *cm = (dm2_cache_match_t *)user_data;
    dm2_cache_info_t    *find_ci = cm->cm_ci;
    dm2_cache_type_t    *find_ct = cm->cm_ct;

    /* Every URI should match */
    if (uh->uri_container->c_dev_ci == find_ci) {
        DBG_DM2M3("Deleting: %s", uh->uri_name);
        dm2_delete_uri(uh, find_ct);
        return TRUE;
    } else {
	DBG_DM2S("Corrupt hash table: %s %p %p",
		 uh->uri_name, uh->uri_container->c_dev_ci, find_ci);
	assert(0);
    }
    return FALSE;
}       /* cmp_match_cache_info_delete_uri */


int
dm2_uri_remove_cache_info(dm2_cache_info_t *ci,
                          dm2_cache_type_t *ct)
{
    dm2_cache_match_t cm;
    int num_removed;

    cm.cm_ci = ci;
    cm.cm_ct = ct;

    /*
     * This function does not need to hold any global lock while the uris
     * in the cache_info hash table are being deleted as..
     *
     * - The 'ci' object passed to this function has already been moved out
     *   of the active cache info list to the delete cache info list.
     *   At this point no other data path should be able to access this 'ci'
     *   (or the disk) as it is not part of the active list.
     *
     * - The 'cmp_match_cache_info_delete_uri' function that gets called for
     *   every URI in the ci hash table will ultimately hold the
     *   'ct->ct_dm2_uri_hash_table_mutex' which is the top level hash table
     *   lock for all the URI hash tables covering all the ci's (or disks).
     */

    num_removed = g_hash_table_foreach_remove(ci->ci_uri_hash_table,
					      cmp_match_cache_info_delete_uri,
					      (void *)&cm);
    ci->ci_uri_hash_table_total_cnt = g_hash_table_size(ci->ci_uri_hash_table);

    return num_removed;
}       /* dm2_uri_remove_cache_info */

/*
 * Initialize the URI lock pool. The individual locks are linked together
 * using STAILQ. This init function allocates the necessay locks and forms the
 * linked list. This function gets invoked from DM2_init()
 */
int
dm2_uri_lock_pool_init(dm2_cache_info_t *ci,
		       int num)
{
    dm2_uri_lock_t *ulock = NULL;
    char mutex_name[40];
    int ret = 0, macro_ret;

    snprintf(mutex_name, 40, "%s.dm2_ci_uri_lock-mutex", ci->mgmt_name);
    NKN_MUTEX_INITR(&ci->ci_uri_lock_mutex, NULL, mutex_name);
    if (ret != 0) {
	DBG_DM2S("Mutex Init: %d", ret);
	return -EINVAL;
    }

    STAILQ_INIT(&ci->ci_uri_lock_q);

    while (num > 0) {
	ulock = dm2_calloc(1, sizeof(dm2_uri_lock_t), mod_dm2_uri_lock_t);
	if (ulock == NULL)
	    return -ENOMEM;
	STAILQ_INSERT_TAIL(&ci->ci_uri_lock_q, ulock, entries);
	num--;
    }

    return 0;
}   /* dm2_uri_lock_init */

/* 'dm2_uri_lock_get' associates a uri_lock to a uri_name. If a URI already
 * has a lock associated to it, the same lock is returned else a new lock is
 * got from the uri_lock lnked list and associated to the uri_name. By design
 * we should not be running out of locks unless there is a bug.
 */
void
dm2_uri_lock_get(DM2_uri_t *uri_head)
{
    int macro_ret;
    dm2_uri_lock_t   *ulock = NULL;
    dm2_cache_info_t *ci = uri_head->uri_container->c_dev_ci;;

    NKN_MUTEX_LOCKR(&ci->ci_uri_lock_mutex);
    if (uri_head->uri_lock) {
	uri_head->uri_lock->in_use++;
	NKN_MUTEX_UNLOCKR(&ci->ci_uri_lock_mutex);
	return;
    }
    ulock = STAILQ_FIRST(&ci->ci_uri_lock_q);
    if (ulock) {
	STAILQ_REMOVE_HEAD(&ci->ci_uri_lock_q, entries);
	memset(ulock, 0, sizeof(dm2_uri_lock_t));
	ulock->in_use++;
	ci->ci_dm2_uri_lock_in_use_cnt++;
    } else {
	assert(0);
    }
    ci->ci_dm2_uri_lock_get_cnt++;
    uri_head->uri_lock = ulock;
    NKN_MUTEX_UNLOCKR(&ci->ci_uri_lock_mutex);
}   /* dm2_uri_lock_get */

/* 'dm2_uri_lock_release' disassociates a uri_lock from the URI. If
 * the lock is in use by other threads, this function does nothing.
 * If the lock is not shared with other threads, the lock is put
 * back to the pool.
 */
void
dm2_uri_lock_release(DM2_uri_t *uri_head)
{
    dm2_uri_lock_t *uri_lock = NULL;
    dm2_cache_info_t *ci = uri_head->uri_container->c_dev_ci;;
    int macro_ret;

    uri_lock = uri_head->uri_lock;
    assert(uri_lock);

    NKN_MUTEX_LOCKR(&ci->ci_uri_lock_mutex);
    uri_lock->in_use--;
    if (uri_lock->uri_nwriters > 0 ||
	uri_lock->uri_wait_nwriters > 0 ||
	uri_lock->uri_nreaders > 0 ||
	uri_lock->uri_wait_nreaders > 0 ||
	uri_lock->in_use > 0) {

	if (uri_lock->in_use && !uri_lock->uri_wait_nreaders &&
	    !uri_lock->uri_nreaders && !uri_lock->uri_wait_nwriters &&
	    !uri_lock->uri_nwriters)
	    assert(0);

	NKN_MUTEX_UNLOCKR(&ci->ci_uri_lock_mutex);
	return;
    }

    STAILQ_INSERT_TAIL(&ci->ci_uri_lock_q, uri_lock, entries);
    ci->ci_dm2_uri_lock_release_cnt++;
    ci->ci_dm2_uri_lock_in_use_cnt--;
    uri_head->uri_lock = NULL;
    NKN_MUTEX_UNLOCKR(&ci->ci_uri_lock_mutex);
}   /* dm2_uri_lock_release */

/* 'dm2_uri_lock_drop' unconditionally drops the lock for the URI. This
 * function is useful in places like DELETE, where the uri_head needs to
 * be destroyed and sharing of this URI (by other threads) needs to be
 * avoided.
 */
void
dm2_uri_lock_drop(DM2_uri_t *uri_head)
{
    dm2_uri_lock_t *uri_lock = NULL;
    dm2_cache_info_t *ci = uri_head->uri_container->c_dev_ci;;
    int macro_ret;

    uri_lock = uri_head->uri_lock;
    assert(uri_lock);

    NKN_MUTEX_LOCKR(&ci->ci_uri_lock_mutex);
    uri_lock->in_use--;
    STAILQ_INSERT_TAIL(&ci->ci_uri_lock_q, uri_lock, entries);
    ci->ci_dm2_uri_lock_in_use_cnt--;
    ci->ci_dm2_uri_lock_drop_cnt++;
    uri_head->uri_lock = NULL;
    NKN_MUTEX_UNLOCKR(&ci->ci_uri_lock_mutex);
}   /* dm2_uri_lock_drop */
