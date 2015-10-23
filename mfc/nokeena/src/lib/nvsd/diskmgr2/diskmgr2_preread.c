/*
 * (C) Copyright 2009 Ankeena Networks, Inc
 *
 *	Author: Michael Nishimoto
 */

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/mount.h>
#include <strings.h>
#include <dirent.h>
#include <pthread.h>
#include <ftw.h>
#include <time.h>
#include <sys/prctl.h>

#include "nkn_lockstat.h"
#include "nkn_cod.h"
#include "nkn_diskmgr2_api.h"
#include "nkn_diskmgr2_local.h"
#include "nkn_assert.h"
#include "diskmgr2_locking.h"
#include "diskmgr2_common.h"

/* Mount point + top level directory */
#define MAX_NS (MAX_URI_HDR_SIZE + DM2_MAX_MOUNTPT)

int glob_dm2_num_preread_disk_threads = 3;
extern int glob_cachemgr_init_done;

pthread_key_t g_dm2_preread_key;
static void
dm2_preread_key_destr(void *arg);

void
dm2_preread_q_lock_init(dm2_preread_q_t *pr_q)
{
    int ret;
    ret = pthread_mutex_init(&pr_q->qlock, NULL);
    assert(ret == 0);
}   /* dm2_preread_q_lock_init */


static void
dm2_preread_q_lock_destroy(dm2_preread_q_t *pr_q)
{
    int ret;
    ret = pthread_mutex_destroy(&pr_q->qlock);
    assert(ret == 0);
}   /* dm2_preread_q_lock_destroy */


static void
dm2_preread_q_lock(dm2_preread_q_t *pr_q)
{
    int ret;

    ret = pthread_mutex_lock(&pr_q->qlock);
    assert(ret == 0);
}   /* dm2_preread_q_lock */


static void
dm2_preread_q_unlock(dm2_preread_q_t *pr_q)
{
    int ret;

    ret = pthread_mutex_unlock(&pr_q->qlock);
    assert(ret == 0);
}   /* dm2_preread_q_unlock */


static void
dm2_preread_cont_push_tail(dm2_preread_q_t *pr_q,
			   void		   *cont)
{
    dm2_preread_q_lock(pr_q);
    g_queue_push_tail(&pr_q->q, cont);
    dm2_preread_q_unlock(pr_q);
}   /* dm2_preread_cont_push_tail */


static void*
dm2_preread_cont_pop_head(dm2_preread_q_t *pr_q)
{
    gpointer elm;

    dm2_preread_q_lock(pr_q);
    if (g_queue_get_length(&pr_q->q) > 0)
	elm = g_queue_pop_head(&pr_q->q);
    else
	elm = (gpointer)-1;
    dm2_preread_q_unlock(pr_q);

    return (void*)elm;
}   /* dm2_preread_cont_pop_head */

static void
dm2_preread_hash_mutex_init(dm2_cache_type_t *ct)
{
    int macro_ret;
    char mutex_name[40];

    snprintf(mutex_name, 40, "dm2_preread_mutex");
    NKN_MUTEX_INITR(&ct->ct_dm2_preread_mutex, NULL, mutex_name);
}   /* dm2_preread_hash_mutex_init */

static void
dm2_preread_hash_mutex_lock(dm2_cache_type_t *ct)
{
    int macro_ret;

    NKN_MUTEX_LOCKR(&ct->ct_dm2_preread_mutex);
}   /* dm2_prerepad_hash_mutex_lock */

static void
dm2_preread_hash_mutex_unlock(dm2_cache_type_t *ct)
{
    int macro_ret;

    NKN_MUTEX_UNLOCKR(&ct->ct_dm2_preread_mutex);
}   /* dm2_preread_hash_mutex_unlock */

static int
dm2_preread_hash_insert(dm2_cache_type_t *ct, const char *contname)
{
    int ret = 0;
    int64_t  keyhash;
    dm2_preread_hash_entry_t *pr_hash = NULL;

    pr_hash = dm2_calloc(1, sizeof(dm2_preread_hash_entry_t),
			 mod_dm2_preread_hash);
    if (pr_hash == NULL)
	return -ENOMEM;

    memcpy(&pr_hash->cont, contname, MAX_URI_SIZE);
    keyhash = n_str_hash_wlen(&pr_hash->cont, 0);

    dm2_preread_hash_mutex_lock(ct);
    if (nchash_lookup(ct->ct_dm2_preread_hash, keyhash,
		      (void *)&pr_hash->cont)) {
	ret = 1;
	goto end;
    }

    nchash_insert(ct->ct_dm2_preread_hash, keyhash,
		 &pr_hash->cont, pr_hash);
    ct->ct_dm2_preread_hash_insert_cnt++;
 end:
    dm2_preread_hash_mutex_unlock(ct);
    if (ret)
	dm2_free(pr_hash, sizeof(dm2_preread_hash_entry_t), DM2_NO_ALIGN);
    return ret;

}   /* dm2_preread_hash_insert */

static void
dm2_free_preread_hash_entry(void *key,
			    void *value,
			    void *userdata)
{
    dm2_cache_type_t *ct = (dm2_cache_type_t *)userdata;
    int64_t  keyhash;
    int ret;

    keyhash = n_str_hash_wlen(key, 0);
    ret = nchash_remove(ct->ct_dm2_preread_hash, keyhash, key);
    if (!ret)
	assert(0);
    dm2_free(value, sizeof(dm2_preread_hash_entry_t), DM2_NO_ALIGN);
    ct->ct_dm2_preread_hash_free_cnt++;
    return;
}   /* dm2_free_preread_hash_entry */

void
dm2_cleanup_preread_hash(dm2_cache_type_t *ct)
{
    dm2_preread_hash_mutex_lock(ct);

    if (ct->ct_dm2_preread_hash == NULL)
	goto end;

    nchash_foreach(ct->ct_dm2_preread_hash,
		   dm2_free_preread_hash_entry, ct);

    nchash_destroy(ct->ct_dm2_preread_hash);
    ct->ct_dm2_preread_hash = NULL;
 end:
    dm2_preread_hash_mutex_unlock(ct);

    return;
}   /* dm2_cleanup_preread_hash */

static int
dm2_preread_container(const char *contname)
{
    nkn_uol_t uol;
    MM_stat_resp_t stat_reply;
    nkn_provider_type_t *ptypep;
    dm2_cache_type_t    *ct;
    int ret, tries = 0, idx;

    ptypep = (nkn_provider_type_t *)pthread_getspecific(g_dm2_preread_key);
    if (ptypep == NULL) {
	DBG_DM2S("pthread_getspecific failed");
	return -EINVAL;
    }

    idx = dm2_cache_array_map_ret_idx(*ptypep);
    if (idx < 0) {
        DBG_DM2S("Illegal ptype: %d", *ptypep);
        return -EINVAL;
    }

    ct = &g_cache2_types[idx];

    /* If we can optimize preread use a hash table to issue unique
     * STATs. This will make the threads work on multiple containers
     * at the same time without having to wait for locks on other threads
     */
    if (ct->ct_dm2_preread_opt) {
	if (dm2_preread_hash_insert(ct, contname)) {
	    ct->ct_dm2_preread_duplicate_skip_cnt++;
	    return 0;
	}
    }

    memset(&uol, 0, sizeof(uol));
    memset(&stat_reply, 0, sizeof(stat_reply));

    /* Allocate a COD for the dummy lookup. If COD is unavailable, try again
     * after a short sleep. If COD allocation fails repeatedly, we have no
     * choice other than to exit
     */
 try_again:
    uol.cod = nkn_cod_open(contname, strlen(contname), NKN_COD_DISK_MGR);
    if (uol.cod == NKN_COD_NULL) {
	DBG_DM2S("No more CODs during startup");
	DBG_DM2S("COD allocation failed for URI %s", contname);
	NKN_ASSERT(uol.cod != NKN_COD_NULL);
	tries++;
	if (tries > 10)
	    return -ENOMEM;
	else
	    sleep(1);
	goto try_again;
    }
    uol.length = 32*1024;

    stat_reply.ptype = *ptypep;
    stat_reply.in_flags |= MM_FLAG_PREREAD;
    glob_dm2_preread_stat_cnt++;
    ret = DM2_stat(uol, &stat_reply);
    /* Since we hand the STAT a URI which we don't expect to exist, it isn't
     * really an error if we can't find the URI */
    if (ret == -ENOENT)
	ret = 0;

    nkn_cod_close(uol.cod, NKN_COD_DISK_MGR);
    return ret;
}	/* dm2_preread_container */


static int
dm2_preread_get_container(const char	    *fpath,
			  char		    *contname,
			  dm2_cache_info_t  **ret_ci,
			  int		    typeflag)
{
    char		*slash, *dc;
    dm2_cache_info_t	*ci = NULL;
    uint32_t		*ptype_ptr;
    char		mgmt_name[DM2_MAX_MGMTNAME];

    if ((typeflag & FTW_NS) == FTW_NS)  // Stat failed: shouldn't happen
	return(0);
    if ((typeflag & FTW_DNR) == FTW_DNR)// Can not read: shouldn't happen
	return(0);
    if ((typeflag & FTW_D) == FTW_D)    // Directory
	return 0;
    slash = strrchr(fpath, '/');
    if (slash == NULL)          // Can not find /: shouldn't happen
	assert(0);
    if (*(slash+1) == '\0')     // mount point
	return 0;

    if (strcmp(slash+1, NKN_CONTAINER_NAME) != 0)	// Skip non-containers
	return 0;

    dc = strstr(fpath, NKN_DM_DISK_CACHE_PREFIX);
    if (dc == NULL)
	return 0;
    /* disk name would be dc_xx, 4/5 characters */
    strncpy(mgmt_name, dc ,6);
    if (mgmt_name[4] == '/')
	mgmt_name[4] = '\0';
    else if (mgmt_name[5] == '/')
	mgmt_name[5] = '\0';

    ci = dm2_mgmt_find_disk_cache(mgmt_name, &ptype_ptr);
    if (ci == NULL) {
	DBG_DM2W("Unable to find ci for disk %s\n", mgmt_name);
	return -1;
    }

    if (dm2_stop_preread) {
	DBG_DM2W("Aborting pre-read on %s as per admin action", mgmt_name);
	return -1;
    }

    if (ci->state_overall != DM2_MGMT_STATE_CACHE_RUNNING) {
	DBG_DM2W("Aborting pre-read as Disk %s state changed", mgmt_name);
	return -1;
    }

    dc = strchr(dc, '/');	// Remove mount point
    if (dc == NULL)
	return 0;

    contname[0] = '\0';
    strncat(contname, dc, MAX_URI_SIZE); // OK to use if copying to &dest[0]

    *ret_ci = ci;
    //DBG_DM2M2("PRE-READ %s:%s", contname, fpath);
    return 0;
}   /* dm2_preread_get_container */


static int
dm2_preread_inode(const char        *fpath,
		  const struct stat *sb __attribute((unused)),
		  int               typeflag)
{
    dm2_cache_info_t *ci = NULL;
    dm2_preread_q_t  *pr_q = NULL;
    dm2_prq_entry_t  *pr_entry = NULL;
    int ret = 0;

    pr_entry = dm2_calloc(1, sizeof(dm2_prq_entry_t),
			  mod_dm2_preread_cont_name);
    if (pr_entry == NULL) {
	ret = -ENOMEM;
	goto error_exit;
    }

    ret = dm2_preread_get_container(fpath, pr_entry->cont, &ci, typeflag);
    if (ret || !ci)
	goto error_exit;

    if (ci->ci_preread_errno) {
	ret = ci->ci_preread_errno;
	goto error_exit;
    }

    pr_q = &ci->ci_preread_arg.pr_q;
    if (pr_q == NULL) {
	DBG_DM2S("[cache_type:%s] Unable to get preread queue",
		ci->ci_cttype->ct_name);
	ret = -EINVAL;
	goto error_exit;
    }

    pr_entry->ci = ci;
    dm2_preread_cont_push_tail(pr_q, pr_entry);
    AO_fetch_and_add1(&ci->ci_dm2_preread_q_depth);
    return 0;

error_exit:
    if (pr_entry)
	dm2_free(pr_entry, sizeof(dm2_prq_entry_t), DM2_NO_ALIGN);
    return ret;
}   /* dm2_preread_inode */


static void
dm2_preread_get_top_dir(char *top_dir,
			const char *mountpt,
			const char *ent)
{
    int mnt_len, ent_len, cp_len;

    top_dir[0] = '\0';
    NKN_ASSERT(mountpt[0] == '/');
    strncat(top_dir, mountpt, MAX_NS);	// OK to use if copying to &dest[0]
    mnt_len = strlen(top_dir);
    if (top_dir[mnt_len-1] != '/') {
	top_dir[mnt_len] = '/';
	top_dir[mnt_len+1] = '\0';
	mnt_len++;
    }
    NKN_ASSERT(ent[0] != '/');
    ent_len = strlen(ent);
    cp_len = ent_len + mnt_len;
    if (cp_len > MAX_NS-1) {
	DBG_DM2S("[mountpt=%s] Directory length too large: %d",
		 mountpt, cp_len);
	cp_len = MAX_NS - 1 - mnt_len;
    } else
	cp_len = ent_len;
    strncat(top_dir, ent, cp_len);
}	/* dm2_preread_get_top_dir */


void
dm2_preread_ci(dm2_cache_info_t *ci)
{
    char		 top_dir[MAX_NS];
    DIR			 *dirp;
    struct dirent	 *ent;

    DBG_DM2W("Preread processing start: %s", ci->mgmt_name);

    /* There could be multiple directory entries which match
     * the namespace:uuid string.  Only read in active namespaces */
    if ((dirp = opendir(ci->ci_mountpt)) == NULL) {
	DBG_DM2S("[mgmt=%s] Opendir failed for %s: %d",
		 ci->mgmt_name, ci->ci_mountpt, errno);
	goto end_ci;
    }

    while ((ent = readdir(dirp)) != NULL) {

	if (dm2_stop_preread)
	    break;

	if (ci->ci_disabling ||
	    ci->state_overall != DM2_MGMT_STATE_CACHE_RUNNING) {
	    DBG_DM2W("Aborting pre-read on disk %s as its state "
		     "changed", ci->mgmt_name);
	    break;
	}

	if (ci->ci_preread_errno)
	    break;

	if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..") ||
	    !strcmp(ent->d_name, DM2_BITMAP_FNAME))
	    continue;

	/* . .. freeblks lost+found are all present */
	if (dm2_stop_preread)
	    break;

	if (ci->ci_disabling ||
	    ci->state_overall != DM2_MGMT_STATE_CACHE_RUNNING) {
	    DBG_DM2W("Aborting pre-read on disk %s as its state "
		     "changed", ci->mgmt_name);
	    break;
	}

	if (ci->ci_preread_errno)
	    break;
	DBG_DM2M("[mgmt=%s] Start preread namespace: %s",
		 ci->mgmt_name, ent->d_name);
	dm2_preread_get_top_dir(top_dir, ci->ci_mountpt, ent->d_name);
	ftw(top_dir, dm2_preread_inode, 10);
	DBG_DM2M("[mgmt=%s] End preread namespace: %s",
		 ci->mgmt_name, ent->d_name);
    }
    closedir(dirp);

 end_ci:
    DBG_DM2W("Preread processing end: %s", ci->mgmt_name);

}   /* dm2_preread_ci */

static void
dm2_preread_key_destr(void *arg)
{
    UNUSED_ARGUMENT(arg);
}

void *
dm2_preread_worker_thread(void *arg)
{
    dm2_cache_info_t *ci = (dm2_cache_info_t *)arg;
    dm2_cache_type_t *ct = ci->ci_cttype;
    dm2_preread_q_t  *pr_q = &ci->ci_preread_arg.pr_q;
    dm2_prq_entry_t  *pr_entry = NULL;
    AO_t	     *pr_thrd_done;
    char	     thread_name[64];
    struct timespec  ts;
    int		     ret;

    /* Detach the thread so that its resources get reclaimed on exit */
    ret = pthread_detach(pthread_self());
    assert(ret == 0);

    snprintf(thread_name, 40, "nvsd-prwrk-%s", ci->mgmt_name);
    prctl(PR_SET_NAME, thread_name, 0, 0, 0);


    pr_thrd_done = &ci->ci_preread_arg.pr_thread_done_cnt;

    ret = pthread_setspecific(g_dm2_preread_key, &ct->ct_ptype);
    if (ret) {
	DBG_DM2S("[cache_type=%s] pthread_setspecific failed: %d",
		 ct->ct_name, ret);
	goto thread_exit;
    }

    ts.tv_sec = 0;
    ts.tv_nsec = 100000;

    DBG_DM2W("[cache_types:%s] Preread thread starting", ct->ct_name);

    /* Get the containers to be read from the queue. If an error is
     * seen on this ct, stop reading any further */
    while (1) {
	ret = 0;
	if (dm2_stop_preread)
	    break;
	if (ci->ci_preread_errno)
	    break;
	pr_entry = (dm2_prq_entry_t *)dm2_preread_cont_pop_head(pr_q);
	if (pr_entry == (dm2_prq_entry_t *)-1) {
	    /* Break if all the disks have been read in */
	    if (ci->pr_processed)
		break;
	    nanosleep(&ts, NULL);
	} else {
	    /* Exit processing, if the disk is getting disabled */
	    if (ci->ci_disabling ||
		ci->state_overall != DM2_MGMT_STATE_CACHE_RUNNING) {
		    dm2_free(pr_entry, sizeof(dm2_prq_entry_t), DM2_NO_ALIGN);
		    AO_fetch_and_sub1(&ci->ci_dm2_preread_q_depth);
		    break;
	    }
	    dm2_ci_dev_rwlock_rlock(ci);
	    ci->ci_preread_stat_cnt++;
	    ret = dm2_preread_container(pr_entry->cont);
	    dm2_ci_dev_rwlock_runlock(ci);

	    dm2_free(pr_entry, sizeof(dm2_prq_entry_t), DM2_NO_ALIGN);
	    AO_fetch_and_sub1(&ci->ci_dm2_preread_q_depth);
	    pr_entry = NULL;
	    if (ret && ci->ci_preread_errno == 0)
		ci->ci_preread_errno = ret;
	    if (ci->ci_preread_errno)
		break;
	}
    }

 thread_exit:
    AO_fetch_and_add1(pr_thrd_done);
    pthread_setspecific(g_dm2_preread_key, NULL);
    DBG_DM2W("[cache_types:%s] Preread worker thread exiting", ct->ct_name);
    return NULL;
}   /* dm2_preread_worker_thread */


void *
dm2_preread_main_thread(void *arg)
{
    dm2_cache_info_t	 *ci = (dm2_cache_info_t *)arg;
    dm2_cache_type_t	 *ct = ci->ci_cttype;
    dm2_preread_q_t	 *pr_q = &ci->ci_preread_arg.pr_q;
#if 0
    ns_active_uid_list_t *ns_list = NULL;
#endif
    pthread_t		 *pr_threads = NULL;
    AO_t		 *pr_thrd_done;
    dm2_prq_entry_t	 *prq_entry;
    char		 thread_name[64];
    pthread_attr_t	 attr;
    int			 i, num_threads = 0;
    int			 ret = 0, stacksize = 4*1024*1024;

    /* Make a note that preread threads are active. We should
     * not be starting another set of preread threads */
    ci->ci_preread_thread_active = 1;

    /* Detach the thread so that its resources get reclaimed on exit */
    ret = pthread_detach(pthread_self());
    assert(ret == 0);

    snprintf(thread_name, 40, "nvsd-prmain-%s", ci->mgmt_name);
    prctl(PR_SET_NAME, thread_name, 0, 0, 0);

    while (!glob_cachemgr_init_done) {
	sleep(1);
    }

    /* create a queue for the worker threads to pick up the
     * container file to be read */
    if (!pr_q->q_init_done) {
	AO_store(&ci->ci_preread_arg.pr_thread_done_cnt, 0);
	dm2_g_queue_init(&pr_q->q);
	dm2_preread_q_lock_init(pr_q);
	pr_q->q_init_done = 1;
    }

    pr_thrd_done = &ci->ci_preread_arg.pr_thread_done_cnt;

#if 0
    ns_list = get_active_namespace_uids();
    if (ns_list == NULL || ns_list->num_entries == 0) {
	DBG_DM2S("[cache_type=%s] No active namespaces", ct->ct_name);
	goto thread_end;
    }
#endif

    DBG_DM2W("[cache_type=%s] Spawning worker threads",
	     ct->ct_name);

    pr_threads = alloca(glob_dm2_num_preread_disk_threads * sizeof(pthread_t));

    ret = pthread_attr_init(&attr);
    if (ret) {
	DBG_DM2S("pthread_attr_init() failed, ret=%d", ret);
	goto thread_end;
    }

    ret = pthread_attr_setstacksize(&attr, stacksize);
    if (ret) {
	DBG_DM2S("pthread_attr_setstacksize() stacksize=%d failed ret=%d",
		 stacksize, ret);
	goto thread_end;
    }

    /* start the worker threads that pick up the 'containers' to be read from
     * a queue. This main thread will loop over the ci list and populate the
     * queue. If an error occurs during a preread STAT, all the worker threads
     * will exit and the main thread will cleanup the queue. */
    for (i = 0; i < glob_dm2_num_preread_disk_threads ; i++) {
	pthread_create(&pr_threads[i], &attr, dm2_preread_worker_thread, arg);
	num_threads++;
    }

    if (dm2_stop_preread)
	goto thread_end;

    if (ci->ci_disabling ||
	ci->state_overall != DM2_MGMT_STATE_CACHE_RUNNING)
	goto thread_end;

    /* We already fully walked through the disk */
    if (ci->pr_processed)
	goto thread_end;

    /* Stop filling the Queue, if an error is detected */
    if (ci->ci_preread_errno)
	goto thread_end;

    /* process the ci and fill the pre-read queue */
    dm2_preread_ci(ci);
    ci->pr_processed = 1;

 thread_end:
    /* wait until all the worker threads exit */
    while (1) {
	if (num_threads == (int)AO_load(pr_thrd_done))
	    break;
	sleep(1);
    }

#if 0
    if (ns_list)
	free_active_uid_list(ns_list);
#endif
    if (!ci->ci_disabling) {
	ci->ci_preread_done = 1;
	AO_fetch_and_add1(&ct->ct_num_caches_preread_done);
    }
    if (dm2_stop_preread) {
	DBG_DM2S("[cache_type=%s] Aborting pre-read on disk %s as per "
		 "admin action", ct->ct_name, ci->mgmt_name);
    } else if (ci->ci_disabling) {
	DBG_DM2S("[cache_type=%s] Aborting pre-read as disk %s is disabled",
		 ct->ct_name, ci->mgmt_name);
    } else if (ci->ci_preread_errno == 0) {
	/* We have completed reading this disk w/o errors, so we can perform
	 * this stat optimization */
	ci->ci_preread_stat_opt = 1;
	AO_fetch_and_add1(&ct->ct_num_caches_preread_success);
    } else if (ci->ci_preread_errno == -ENOCSI) {
	DBG_DM2S("[cache_type=%s] Pre-read aborted early from dictionary "
		 "memory exhaustion: %ld", ct->ct_name, glob_dm2_mem_max_bytes);
    } else {
	DBG_DM2S("[cache_type=%s] Pre-read aborted early on disk %s : %d",
		 ct->ct_name, ci->mgmt_name, ci->ci_preread_errno);
    }

    DBG_DM2S("[cache_type=%s] Preread Complete on disk %s",
	     ct->ct_name, ci->mgmt_name);
    dm2_update_tier_preread_state(ct);

    /* Make sure that the queue is emptied before the main thread exits.
     * This might be required if the worker threads had to exit prematurely
     * because of some error */
    while (1) {
	prq_entry = (dm2_prq_entry_t *)dm2_preread_cont_pop_head(pr_q);
	if (prq_entry == (dm2_prq_entry_t*)-1) {
	    break;
	} else {
	    dm2_free(prq_entry, sizeof(dm2_prq_entry_t), DM2_NO_ALIGN);
	    AO_fetch_and_sub1(&ci->ci_dm2_preread_q_depth);
	}
    }

    /* Disk resources are not in RP yet. This call is printing un-needed
     * messahes during init. So comment for now */
    //dm2_ns_set_namespace_res_pools(ct->ct_ptype);

    dm2_preread_q_lock_destroy(pr_q);
    pr_q->q_init_done = 0;

    if (dm2_stop_preread)
	dm2_reset_preread_stop_state();

    /* Preread threads are not alive anymore */
    ci->ci_preread_thread_active = 0;

    DBG_DM2W("[cache_type=%s] Preread main thread (disk:%s) exiting",
	      ct->ct_name, ci->mgmt_name);
    return NULL;
}   /* dm2_preread_main_thread */


/*
 * Attempt to prime the disk cache structures.
 * - ct_tier_preread_done is set to 1 after pre-reading is done.  Up until that
 *   point, STAT will return success if the object is found or return
 *   NKN_SERVER_BUSY if not found.  PUT will return NKN_SERVER_BUSY until
 *   pre-reading is done.
 *
 * - Start one thread per tier.  Pass the ptype enum value in a pthread key.
 *
 * - No data malloced.  Instead, use heap data which is sized to the number
 *   of possible tiers.
 *
 * - Only read data from disks in the 'cache running' state.
 *
 * - Only read data from active namespaces.
 */
void
dm2_start_reading_caches(int num_cache_types)
{
    dm2_cache_type_t	*ct;
    dm2_cache_info_t	*ci;
    GList		*ci_obj;
    struct stat		sb;
    pthread_key_t	pkey;
    int			ptype_idx, ret, ci_count;

    /* Disable mechanism */
    if (stat("/config/nkn/cache.no_preread", &sb) == 0) {
	for (ptype_idx = 0; ptype_idx < num_cache_types; ptype_idx++) {
	    ct = &g_cache2_types[ptype_idx];
	    for (ci_obj = ct->ct_info_list; ci_obj; ci_obj = ci_obj->next) {
		ci = (dm2_cache_info_t *)ci_obj->data;
		if (ci->state_overall != DM2_MGMT_STATE_CACHE_RUNNING)
		    continue;
		ci->ci_preread_done = 1;
		AO_fetch_and_add1(&ct->ct_num_caches_preread_done);
	    }
	    dm2_update_tier_preread_state(ct);
	}
	glob_dm2_preread_disabled = 1;
	return;
    }

    if ((ret = pthread_key_create(&pkey, dm2_preread_key_destr)) != 0) {
	DBG_DM2S("PRE-READING caches failed: pthread_key_create failed: %d",
		 ret);
	return;
    }
    g_dm2_preread_key = pkey;

    for (ptype_idx = 0; ptype_idx < num_cache_types; ptype_idx++) {
	ct = &g_cache2_types[ptype_idx];
	ct->ct_dm2_preread_hash =  nchash_table_new(n_str_hash_wlen,
						    n_str_equal,
						    1000, 0,
						    mod_dm2_preread_hash);
	assert(ct->ct_dm2_preread_hash != NULL);
	dm2_preread_hash_mutex_init(ct);

	DBG_DM2S("[cache_type=%s] Preread Start", ct->ct_name);
	ci_count = 0;
	ct->ct_dm2_preread_start_ts = nkn_cur_ts;
	/* Issue unique STAT requests only. If two disks have the same
	 * containers, a single STAT will read both of them. If a disk was
	 * enabled after preread began, we can not do this optimization */
	ct->ct_dm2_preread_opt = 1;
	for (ci_obj = ct->ct_info_list; ci_obj; ci_obj = ci_obj->next) {
	    ci = (dm2_cache_info_t *)ci_obj->data;
	    if (ci->state_overall != DM2_MGMT_STATE_CACHE_RUNNING)
		continue;

	    ci_count++;
	    ci->ci_preread_arg.pr_q.q_init_done = 0;
	    AO_store(&ci->ci_preread_arg.pr_thread_done_cnt, 0);
	    pthread_create(&ci->ci_preread_thread, NULL,
			   dm2_preread_main_thread, (void *)ci);
	}

	if (ci_count == 0)	// No thread started for this tier
	    dm2_update_tier_preread_state(ct);
    }
}	/* dm2_start_reading_caches */
