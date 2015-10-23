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
#include <sys/types.h>
#include <sys/param.h>
#include <unistd.h>
#include <glib.h>

#include "nkn_am_api.h"
#include "nkn_diskmgr2_local.h"
#include "nkn_diskmgr_intf.h"
#include "nkn_sched_api.h"
#include "nkn_diskmgr2_disk_api.h"
#include "nkn_locmgr2_uri.h"
#include "nkn_locmgr2_extent.h"
#include "nkn_locmgr2_container.h"
#include "nkn_locmgr2_physid.h"
#include "nkn_locmgr2_attr.h"
#include "nkn_defs.h"
#include "nkn_util.h"
#include "nkn_assert.h"
#include "nkn_opts.h"		// likely/unlikely macros

#include "diskmgr2_locking.h"
#include "diskmgr2_common.h"

extern uint64_t glob_dm2_conthead_lock_read_wait_cnt,
    glob_dm2_conthead_lock_write_wait_cnt;
int64_t glob_dm2_conthead_rlock_busy_cnt;

uint64_t glob_dm2_ct_lock_read_wait_cnt,
    glob_dm2_ct_lock_write_wait_cnt;

int g_dm2_uri_locking = 0;

/* Because we have an old glib library */
void
dm2_g_queue_init(GQueue *q)
{
    q->head = q->tail = NULL;
    q->length = 0;
}

void
dm2_ct_stat_mutex_lock(dm2_cache_type_t *ct,
		       int *locked,
		       const char *str)
{
    int macro_ret;

    NKN_MUTEX_LOCKR(&ct->ct_stat_mutex);
    if (unlikely(dm2_show_locks))
	DBG_DM2S("[ct=%s][pth=%ld/%ld] CT STAT MUTEX LOCK stat: %s",
		 ct->ct_name, pthread_self(), gettid(), str);
    *locked = 1;
}	/* dm2_ct_stat_mutex_lock */

void
dm2_ct_stat_mutex_unlock(dm2_cache_type_t *ct,
			 int *locked,
			 char *str)
{
    int macro_ret;

    if (unlikely(dm2_show_locks))
	DBG_DM2S("[ct=%s][pth=%ld/%ld] CT STAT MUTEX UNLOCK stat: %s",
		 ct->ct_name, pthread_self(), gettid(), str);
    NKN_MUTEX_UNLOCKR(&ct->ct_stat_mutex);
    *locked = 0;
}	/* dm2_ct_stat_mutex_unlock */

void
dm2_ct_rwlock_init(dm2_cache_type_t *ct)
{
    int macro_ret;

    PTHREAD_MUTEX_INIT(&ct->ct_rwlock_mutex);
    PTHREAD_COND_INIT(&ct->ct_rwlock_read_cond, NULL);
    PTHREAD_COND_INIT(&ct->ct_rwlock_write_cond, NULL);
    ct->ct_rwlock_lock_type = 0;
}	/* dm2_ct_rwlock_init */


void
dm2_ct_rwlock_rlock(dm2_cache_type_t *ct)
{
    int macro_ret;

    PTHREAD_MUTEX_LOCK(&ct->ct_rwlock_mutex);
 loop:
    if (ct->ct_rwlock_nwriters > 0 || ct->ct_rwlock_wait_nwriters > 0 ||
	ct->ct_rwlock_wait_nreaders > 0) {

	ct->ct_rwlock_wait_nreaders++;
	glob_dm2_ct_lock_read_wait_cnt++;
	PTHREAD_COND_WAIT(&ct->ct_rwlock_read_cond, &ct->ct_rwlock_mutex);
	goto loop;
    } else {
	assert(ct->ct_rwlock_wait_nreaders == 0);
	ct->ct_rwlock_nreaders++;
	ct->ct_rwlock_lock_type = DM2_CT_RLOCKED;
    }
    if (unlikely(g_dm2_uri_locking) && !strcmp(ct->ct_name, "SATA"))
	DBG_DM2S("[ct=%s] # readers=%d wr=%d  # writers=%d  ww=%d",
		 ct->ct_name,
		 ct->ct_rwlock_nreaders,
		 ct->ct_rwlock_wait_nreaders,
		 ct->ct_rwlock_nwriters,
		 ct->ct_rwlock_wait_nwriters);
    PTHREAD_MUTEX_UNLOCK(&ct->ct_rwlock_mutex);

    if (unlikely(dm2_show_locks))
	DBG_DM2S("RW-RLOCK ct: %s", ct->ct_name);
}	/* dm2_ct_rwlock_rlock */


void
dm2_ct_rwlock_runlock(dm2_cache_type_t *ct)
{
    int macro_ret;

    if (unlikely(dm2_show_locks))
	DBG_DM2S("RW-RUNLOCK ct: %s", ct->ct_name);

    PTHREAD_MUTEX_LOCK(&ct->ct_rwlock_mutex);
    assert(ct->ct_rwlock_nreaders > 0);
    assert(ct->ct_rwlock_wowner == 0);
    assert(ct->ct_rwlock_nwriters == 0);
    assert(ct->ct_rwlock_lock_type == DM2_CT_RLOCKED);

    ct->ct_rwlock_nreaders--;
    if (ct->ct_rwlock_nreaders == 0) {
	ct->ct_rwlock_lock_type = 0;
	if (ct->ct_rwlock_wait_nwriters > 0) {
	    PTHREAD_COND_BROADCAST(&ct->ct_rwlock_write_cond);
	    ct->ct_rwlock_wait_nwriters = 0;
	}
    }
    if (unlikely(g_dm2_uri_locking) && !strcmp(ct->ct_name, "SATA"))
	DBG_DM2S("[ct=%s] # readers=%d wr=%d  # writers=%d  ww=%d",
		 ct->ct_name,
		 ct->ct_rwlock_nreaders,
		 ct->ct_rwlock_wait_nreaders,
		 ct->ct_rwlock_nwriters,
		 ct->ct_rwlock_wait_nwriters);
    PTHREAD_MUTEX_UNLOCK(&ct->ct_rwlock_mutex);
}	/* dm2_ct_rwlock_runlock */


void
dm2_ct_rwlock_wlock(dm2_cache_type_t *ct)
{
    int macro_ret;

    PTHREAD_MUTEX_LOCK(&ct->ct_rwlock_mutex);
 loop:
    if (ct->ct_rwlock_nwriters > 0 || ct->ct_rwlock_nreaders > 0) {
	glob_dm2_ct_lock_write_wait_cnt++;
	ct->ct_rwlock_wait_nwriters++;
	PTHREAD_COND_WAIT(&ct->ct_rwlock_write_cond, &ct->ct_rwlock_mutex);
	goto loop;
    } else {
	ct->ct_rwlock_nwriters = 1;
	ct->ct_rwlock_lock_type = DM2_CT_WLOCKED;
	ct->ct_rwlock_wowner = gettid();
    }
    if (unlikely(g_dm2_uri_locking) && !strcmp(ct->ct_name, "SATA"))
	DBG_DM2S("[ct=%s] # readers=%d wr=%d  # writers=%d  ww=%d",
		 ct->ct_name,
		 ct->ct_rwlock_nreaders,
		 ct->ct_rwlock_wait_nreaders,
		 ct->ct_rwlock_nwriters,
		 ct->ct_rwlock_wait_nwriters);
    PTHREAD_MUTEX_UNLOCK(&ct->ct_rwlock_mutex);

    if (unlikely(dm2_show_locks))
	DBG_DM2S("RW-WLOCK ct: %s", ct->ct_name);
}	/* dm2_ct_rwlock_wlock */


void
dm2_ct_rwlock_wunlock(dm2_cache_type_t *ct)
{
    int macro_ret;

    if (unlikely(dm2_show_locks))
	DBG_DM2S("RW-WUNLOCK ct: %s", ct->ct_name);

    PTHREAD_MUTEX_LOCK(&ct->ct_rwlock_mutex);
    assert(ct->ct_rwlock_nwriters > 0);
    assert(ct->ct_rwlock_wowner != 0);
    assert(ct->ct_rwlock_nreaders == 0);
    assert(ct->ct_rwlock_lock_type == DM2_CT_WLOCKED);

    ct->ct_rwlock_lock_type = 0;
    ct->ct_rwlock_nwriters = 0;
    ct->ct_rwlock_wowner = 0;
    if (ct->ct_rwlock_wait_nwriters > 0) {
	PTHREAD_COND_BROADCAST(&ct->ct_rwlock_write_cond);
	ct->ct_rwlock_wait_nwriters = 0;
    } else if (ct->ct_rwlock_wait_nreaders > 0) {
	PTHREAD_COND_BROADCAST(&ct->ct_rwlock_read_cond);
	ct->ct_rwlock_wait_nreaders = 0;
    }
    if (unlikely(g_dm2_uri_locking) && !strcmp(ct->ct_name, "SATA"))
	DBG_DM2S("[ct=%s] # readers=%d wr=%d  # writers=%d  ww=%d",
		 ct->ct_name,
		 ct->ct_rwlock_nreaders,
		 ct->ct_rwlock_wait_nreaders,
		 ct->ct_rwlock_nwriters,
		 ct->ct_rwlock_wait_nwriters);
    PTHREAD_MUTEX_UNLOCK(&ct->ct_rwlock_mutex);
}	/* dm2_ct_rwlock_wunlock */


#ifdef NKN_DM2_GLOBAL_LOCK_DEBUG
void
dm2_ci_dev_rwlock_init(dm2_cache_info_t *ci)
{
    int ret;

    ret = pthread_rwlock_init(&ci->ci_dev_rwlock, NULL);
    assert(ret == 0);
}

void
dm2_ci_dev_rwlock_rlock(dm2_cache_info_t *ci)
{
    int ret;

    ret = pthread_rwlock_rdlock(&ci->ci_dev_rwlock);
    if (unlikely(dm2_show_locks))
	DBG_DM2S("RW-RLOCK ci: %s", ci->mgmt_name);
    assert(ret == 0);
}

void
dm2_ci_dev_rwlock_runlock(dm2_cache_info_t *ci)
{
    int ret;

    if (unlikely(dm2_show_locks))
	DBG_DM2S("RW-RLOCK ci: %s", ci->mgmt_name);
    assert(ci->ci_dev_rwlock.__data.__nr_readers);
    ret = pthread_rwlock_unlock(&ci->ci_dev_rwlock);
    assert(ret == 0);
}

void
dm2_ci_dev_rwlock_wlock(dm2_cache_info_t *ci)
{
    int ret;

    ret = pthread_rwlock_wrlock(&ci->ci_dev_rwlock);
    assert(ret == 0);
    if (unlikely(dm2_show_locks))
	DBG_DM2S("RW-WLOCK ci: %s", ci->mgmt_name);
}

void
dm2_ci_dev_rwlock_wunlock(dm2_cache_info_t *ci)
{
    int ret;

    if (unlikely(dm2_show_locks))
	DBG_DM2S("RW-WLOCK ci: %s", ci->mgmt_name);
    ret = pthread_rwlock_unlock(&ci->ci_dev_rwlock);
    assert(ret == 0);
}

void
dm2_ct_info_list_rwlock_init(dm2_cache_type_t *ct)
{
    int ret;

    ret = pthread_rwlock_init(&ct->ct_info_list_rwlock, NULL);
    assert(ret == 0);
}

void
dm2_ct_info_list_rwlock_rlock(dm2_cache_type_t *ct)
{
    int ret;

    ret = pthread_rwlock_rdlock(&ct->ct_info_list_rwlock);
    if (unlikely(dm2_show_locks))
	DBG_DM2S("RW-RLOCK ct info list: %s", ct->ct_name);
    assert(ret == 0);
}

void
dm2_ct_info_list_rwlock_runlock(dm2_cache_type_t *ct)
{
    int ret;

    if (unlikely(dm2_show_locks))
	DBG_DM2S("RW-RUNLOCK ct info list: %s", ct->ct_name);
    ret = pthread_rwlock_unlock(&ct->ct_info_list_rwlock);
    assert(ret == 0);
}

void
dm2_ct_info_list_rwlock_wlock(dm2_cache_type_t *ct)
{
    int ret;

    ret = pthread_rwlock_wrlock(&ct->ct_info_list_rwlock);
    assert(ret == 0);
    if (unlikely(dm2_show_locks))
	DBG_DM2S("RW-WLOCK ct info list: %s", ct->ct_name);
}

void
dm2_ct_info_list_rwlock_wunlock(dm2_cache_type_t *ct)
{
    int ret;

    if (unlikely(dm2_show_locks))
	DBG_DM2S("RW-WUNLOCK ct info list: %s", ct->ct_name);
    ret = pthread_rwlock_unlock(&ct->ct_info_list_rwlock);
    assert(ret == 0);
}

void
dm2_ct_uri_hash_rwlock_init(dm2_cache_type_t *ct)
{
    char mutex_name[40];
    int macro_ret;

    snprintf(mutex_name, 40, "%s.dm2_uri_hashtbl-mutex", ct->ct_name);;
    NKN_RWLOCK_INIT(&ct->ct_dm2_uri_hash_table_rwlock, NULL, mutex_name);
}

void
dm2_ct_uri_hash_rwlock_rlock(dm2_cache_type_t *ct, int *lock)
{
    NKN_RWLOCK_RLOCK(&ct->ct_dm2_uri_hash_table_rwlock);
    if (lock)
	*lock += 1;
    if (unlikely(dm2_show_locks))
	DBG_DM2S("RW-RLOCK ct info list: %s", ct->ct_name);
}

void
dm2_ct_uri_hash_rwlock_runlock(dm2_cache_type_t *ct, int *lock)
{

    if (lock)
	assert(*lock != 0);
    if (unlikely(dm2_show_locks))
	DBG_DM2S("RW-RUNLOCK ct info list: %s", ct->ct_name);
    NKN_RWLOCK_UNLOCK(&ct->ct_dm2_uri_hash_table_rwlock);
    if (lock)
	*lock -= 1;
}

void
dm2_ct_uri_hash_rwlock_wlock(dm2_cache_type_t *ct)
{
    NKN_RWLOCK_WLOCK(&ct->ct_dm2_uri_hash_table_rwlock);
    if (unlikely(dm2_show_locks))
	DBG_DM2S("RW-WLOCK ct info list: %s", ct->ct_name);
}

void
dm2_ct_uri_hash_rwlock_wunlock(dm2_cache_type_t *ct)
{
    if (unlikely(dm2_show_locks))
	DBG_DM2S("RW-WUNLOCK ct info list: %s", ct->ct_name);
    NKN_RWLOCK_UNLOCK(&ct->ct_dm2_uri_hash_table_rwlock);
}


#
#if 0
void
dm2_uri_lock_init(DM2_uri_t *uh)
{
    int ret;

    ret = pthread_mutex_init(&uh->uri_ext_mutex, NULL);
    assert(ret == 0);
}

void
dm2_uri_lock_destroy(DM2_uri_t *uh)
{
    int ret;

    ret = pthread_mutex_destroy(&uh->uri_ext_mutex);
    memset(&uh->uri_ext_mutex, 2, sizeof(uh->uri_ext_mutex));
    assert(ret == 0);
}

void
dm2_uri_ext_rwlock_rlock(DM2_uri_t *uh)
{
    int ret;

    /* We can't know the object lock state here because we don't change it
     * at the unlocked state */
    ret = pthread_mutex_lock(&uh->uri_ext_mutex);
    assert(ret == 0);
    if (unlikely(dm2_show_locks))
	DBG_DM2S("MUTEX-LOCK_EXT uri: %s", uh->uri_name);
}

void
dm2_uri_ext_rwlock_runlock(DM2_uri_t *uh)
{
    int ret;

    if (unlikely(dm2_show_locks))
	DBG_DM2S("MUTEX-UNLOCK_EXT uri: %s", uh->uri_name);
    ret = pthread_mutex_unlock(&uh->uri_ext_mutex);
    /* We can't know the object lock state here */
    assert(ret == 0);
}

void
dm2_uri_ext_rwlock_wlock(DM2_uri_t *uh)
{
    int ret;

    /* We can't know the object lock state here because we don't change it
     * at the unlocked state */
    ret = pthread_mutex_lock(&uh->uri_ext_mutex);
    assert(ret == 0);
    if (unlikely(dm2_show_locks))
	DBG_DM2S("MUTEX-LOCK_EXT uri: %s", uh->uri_name);
}

void
dm2_uri_ext_rwlock_wunlock(DM2_uri_t *uh)
{
    int ret;

    if (unlikely(dm2_show_locks))
	DBG_DM2S("MUTEX-UNLOCK_EXT uri: %s", uh->uri_name);
    ret = pthread_mutex_unlock(&uh->uri_ext_mutex);
    /* We can't know the object lock state here */
    assert(ret == 0);
}
#endif


/*
 * This routine is put here in this module because it is looking at the
 * internal nreaders counts.  Ideally, the container_head routines should be
 * doing a lookup and lock within the hash table lock, but that would require
 * more code churn, so it wasn't done.
 *
 * The current code works by having a delete defer to a new lock request.  This
 * works because we don't ever really need to delete a container_head object,
 * especially if a new request wants to re-add an object.  The only problem is
 * that the code isn't as clean and isn't as maintainable.
 */
int
dm2_conthead_free(dm2_container_head_t *ch,
		  dm2_cache_type_t     *ct,
		  int                  *ch_locked)
{
    int macro_ret;

    assert(ch->ch_rwlock_lock_type == DM2_CONT_HEAD_WLOCKED);

    /* Break out if someone else is trying to access the cont-head */
    if (ch->ch_rwlock_nwriters > 1 || ch->ch_rwlock_wait_nwriters > 0 ||
	    ch->ch_rwlock_wait_nreaders > 0)
	return -EBUSY;

    if (dm2_show_locks) {
	DBG_DM2S("FREE CONTHEAD: %s", ch->ch_uri_dir);
    }
    /* Remove the cont head from the hash table */
    NKN_MUTEX_LOCKR(&ct->ct_dm2_conthead_hash_table_mutex);

    /* Recheck the counts inside the hash table lock */
    if (ch->ch_rwlock_nwriters > 1 || ch->ch_rwlock_wait_nwriters > 0 ||
	    ch->ch_rwlock_wait_nreaders > 0) {
	NKN_MUTEX_UNLOCKR(&ct->ct_dm2_conthead_hash_table_mutex);
	return -EBUSY;
    }
    g_hash_table_remove(ct->ct_dm2_conthead_hash_table,	ch->ch_uri_dir);
    ct->ct_dm2_conthead_hash_table_total_cnt =
	g_hash_table_size(ct->ct_dm2_conthead_hash_table);

    NKN_MUTEX_UNLOCKR(&ct->ct_dm2_conthead_hash_table_mutex);

    /* Freeup the memory allocated */
    if (ch && ch->ch_uri_dir) {
	dm2_free(ch->ch_uri_dir, ch->ch_uri_dir_len, DM2_NO_ALIGN);
	dm2_conthead_rwlock_destroy(ch);
    }

    if (ch) {
        dm2_free(ch, sizeof(*ch), DM2_NO_ALIGN);
        *ch_locked = DM2_CONT_HEAD_UNLOCKED;
    }

    return 0;
}   /* dm2_conthead_free */


void
dm2_conthead_rwlock_init(dm2_container_head_t *ch)
{
    int macro_ret;

    PTHREAD_COND_INIT(&ch->ch_rwlock_read_cond, NULL);
    PTHREAD_COND_INIT(&ch->ch_rwlock_write_cond, NULL);
    ch->ch_rwlock_lock_type = 0;
}	/* dm2_conthead_rwlock_init */

void
dm2_conthead_rwlock_destroy(dm2_container_head_t *ch)
{
    int macro_ret;

    PTHREAD_COND_DESTROY(&ch->ch_rwlock_read_cond);
    PTHREAD_COND_DESTROY(&ch->ch_rwlock_write_cond);
}	/* dm2_conthead_rwlock_destroy */

int
dm2_conthead_rwlock_rlock_unsafe(dm2_container_head_t *ch,
				 dm2_cache_type_t     *ct,
				 int                  *cont_head_locked,
				 int		      is_blocking)
{
    int ret = 0, macro_ret = 0;

    assert(*cont_head_locked == DM2_CONT_HEAD_UNLOCKED);

    //    PTHREAD_MUTEX_LOCK(&ct->ct_ch_rwlock_mutex);
 loop:
    if (ch->ch_rwlock_nwriters > 0 || ch->ch_rwlock_wait_nwriters > 0 ||
		ch->ch_rwlock_wait_nreaders > 0) {
	if (is_blocking) {
	    ch->ch_rwlock_wait_nreaders++;
	    glob_dm2_conthead_lock_read_wait_cnt++;
	    PTHREAD_COND_WAIT(&ch->ch_rwlock_read_cond,
			      &ct->ct_dm2_conthead_hash_table_mutex.lock);
	    goto loop;
	} else {
	    ret = EBUSY;
	    glob_dm2_conthead_rlock_busy_cnt++;
	    goto end;
	}
    } else {
        assert(ch->ch_rwlock_wait_nreaders == 0);
        ch->ch_rwlock_nreaders++;
        ch->ch_rwlock_lock_type = DM2_CONT_HEAD_RLOCKED;
	*cont_head_locked = DM2_CONT_HEAD_RLOCKED;
    }
    //    PTHREAD_MUTEX_UNLOCK(&ct->ct_ch_rwlock_mutex);
 end:
    return -ret;
}	/* dm2_conthead_rwlock_rlock_unsafe */


int
dm2_conthead_rwlock_rlock(dm2_container_head_t *ch,
			  dm2_cache_type_t     *ct,
			  int                  *cont_head_locked,
			  int		       is_blocking)
{
    int ret = 0, macro_ret = 0;

    assert(*cont_head_locked == DM2_CONT_HEAD_UNLOCKED);

    NKN_MUTEX_LOCKR(&ct->ct_dm2_conthead_hash_table_mutex);
    dm2_conthead_rwlock_rlock_unsafe(ch, ct, cont_head_locked, is_blocking);
    NKN_MUTEX_UNLOCKR(&ct->ct_dm2_conthead_hash_table_mutex);

    if (unlikely(dm2_show_locks))
	DBG_DM2S("[pth=%ld/%ld] RW-RLOCK cont_head: %s",
		 pthread_self(), gettid(), ch->ch_uri_dir);
    return -ret;
}	/* dm2_conthead_rwlock_rlock */


void
dm2_conthead_rwlock_runlock(dm2_container_head_t *ch,
			    dm2_cache_type_t	 *ct,
			    int                  *cont_head_locked)
{
    int macro_ret;

    assert(*cont_head_locked == DM2_CONT_HEAD_RLOCKED);

    NKN_MUTEX_LOCKR(&ct->ct_dm2_conthead_hash_table_mutex);
    assert(ch->ch_rwlock_nreaders > 0);
    assert(ch->ch_rwlock_wowner == 0);
    assert(ch->ch_rwlock_nwriters == 0);
    assert(ch->ch_rwlock_lock_type == DM2_CONT_HEAD_RLOCKED);

    ch->ch_rwlock_nreaders--;
    if (ch->ch_rwlock_nreaders == 0) {
        ch->ch_rwlock_lock_type = 0;
        if (ch->ch_rwlock_wait_nwriters > 0) {
            PTHREAD_COND_BROADCAST(&ch->ch_rwlock_write_cond);
            ch->ch_rwlock_wait_nwriters = 0;
        }
    }

    *cont_head_locked = DM2_CONT_HEAD_UNLOCKED;
    NKN_MUTEX_UNLOCKR(&ct->ct_dm2_conthead_hash_table_mutex);

    if (unlikely(dm2_show_locks))
	DBG_DM2S("[pth=%ld/%ld] RW-RUNLOCK cont_head: %s",
		 pthread_self(), gettid(), ch->ch_uri_dir);
}	/* dm2_conthead_rwlock_runlock */


void
dm2_conthead_rwlock_wlock_unsafe(dm2_container_head_t *ch,
				 dm2_cache_type_t     *ct,
				 int                  *cont_head_locked)
{
    int macro_ret;

    assert(*cont_head_locked == DM2_CONT_HEAD_UNLOCKED);

    //NKN_MUTEX_LOCKR(&ct->ct_dm2_conthead_hash_table_mutex);
 loop:
    if (ch->ch_rwlock_nwriters > 0 || ch->ch_rwlock_nreaders > 0) {
        glob_dm2_conthead_lock_write_wait_cnt++;
        ch->ch_rwlock_wait_nwriters++;
        PTHREAD_COND_WAIT(&ch->ch_rwlock_write_cond,
			  &ct->ct_dm2_conthead_hash_table_mutex.lock);
        goto loop;
    } else {
        ch->ch_rwlock_nwriters = 1;
        ch->ch_rwlock_lock_type = DM2_CONT_HEAD_WLOCKED;
        ch->ch_rwlock_wowner = gettid();
    }

    *cont_head_locked = DM2_CONT_HEAD_WLOCKED;
    //NKN_MUTEX_UNLOCKR(&ct->ct_dm2_conthead_hash_table_mutex);

    if (unlikely(dm2_show_locks))
	DBG_DM2S("[pth=%ld/%ld] RW-WLOCK cont_head: %s",
		 pthread_self(), gettid(), ch->ch_uri_dir);
}	/* dm2_conthead_rwlock_wlock_unsafe */


void
dm2_conthead_rwlock_wlock(dm2_container_head_t *ch,
			  dm2_cache_type_t     *ct,
			  int                  *cont_head_locked)
{
    int macro_ret;

    assert(*cont_head_locked == DM2_CONT_HEAD_UNLOCKED);

    NKN_MUTEX_LOCKR(&ct->ct_dm2_conthead_hash_table_mutex);
    dm2_conthead_rwlock_wlock_unsafe(ch, ct, cont_head_locked);
    NKN_MUTEX_UNLOCKR(&ct->ct_dm2_conthead_hash_table_mutex);
    if (unlikely(dm2_show_locks))
	DBG_DM2S("[pth=%d/%d] RW-WLOCK cont_head: %s",
		 pthread_self(), gettid(), ch->ch_uri_dir);
}	/* dm2_conthead_rwlock_wlock */


void
dm2_conthead_rwlock_wunlock(dm2_container_head_t *ch,
			    dm2_cache_type_t	 *ct,
			    int                  *cont_head_locked)
{

    int macro_ret;

    assert(*cont_head_locked == DM2_CONT_HEAD_WLOCKED);
    if (unlikely(dm2_show_locks))
	DBG_DM2S("[pth=%ld/%ld] RW-WUNLOCK cont_head: %s",
		 pthread_self(), gettid(), ch->ch_uri_dir);

    NKN_MUTEX_LOCKR(&ct->ct_dm2_conthead_hash_table_mutex);
    dm2_conthead_rwlock_wunlock_unsafe(ch, cont_head_locked);
    NKN_MUTEX_UNLOCKR(&ct->ct_dm2_conthead_hash_table_mutex);

}	/* dm2_conthead_rwlock_wunlock */



void
dm2_conthead_rwlock_wunlock_unsafe(dm2_container_head_t *ch,
				   int                  *cont_head_locked)
{

    int macro_ret;

    assert(*cont_head_locked == DM2_CONT_HEAD_WLOCKED);
    if (unlikely(dm2_show_locks))
	DBG_DM2S("[pth=%ld/%ld] RW-WUNLOCK cont_head: %s",
		 pthread_self(), gettid(), ch->ch_uri_dir);

    assert(ch->ch_rwlock_nwriters > 0);
    assert(ch->ch_rwlock_wowner != 0);
    assert(ch->ch_rwlock_nreaders == 0);
    assert(ch->ch_rwlock_lock_type == DM2_CONT_HEAD_WLOCKED);

    ch->ch_rwlock_lock_type = 0;
    ch->ch_rwlock_nwriters = 0;
    ch->ch_rwlock_wowner = 0;
    if (ch->ch_rwlock_wait_nwriters > 0) {
        PTHREAD_COND_BROADCAST(&ch->ch_rwlock_write_cond);
        ch->ch_rwlock_wait_nwriters = 0;
    } else if (ch->ch_rwlock_wait_nreaders > 0) {
        PTHREAD_COND_BROADCAST(&ch->ch_rwlock_read_cond);
        ch->ch_rwlock_wait_nreaders = 0;
    }

    *cont_head_locked = DM2_CONT_HEAD_UNLOCKED;
}	/* dm2_conthead_rwlock_wunlock unsafe */


void
dm2_conthead_rwlock_unlock(dm2_container_head_t *ch,
			   dm2_cache_type_t	*ct,
			   int			*cont_head_locked)
{
    assert(*cont_head_locked != DM2_CONT_HEAD_UNLOCKED);
    if (unlikely(dm2_show_locks)) {
	if (*cont_head_locked == DM2_CONT_HEAD_WLOCKED) {
	    DBG_DM2S("[pth=%ld/%ld] RW-WUNLOCK cont_head: %s",
		     pthread_self(), gettid(), ch->ch_uri_dir);
	} else if (*cont_head_locked == DM2_CONT_HEAD_RLOCKED) {
	    DBG_DM2S("[pth=%ld/%ld] RW-RUNLOCK cont_head: %s",
		     pthread_self(), gettid(), ch->ch_uri_dir);
	} else
	    assert(0);	// corruption or logic error
    }
    if (*cont_head_locked == DM2_CONT_HEAD_WLOCKED) {
	dm2_conthead_rwlock_wunlock(ch, ct, cont_head_locked);
    } else if (*cont_head_locked == DM2_CONT_HEAD_RLOCKED) {
	dm2_conthead_rwlock_runlock(ch, ct, cont_head_locked);
    } else
	assert(0);	// corruption or logic error
}	/* dm2_conthead_rwlock_unlock */


void
dm2_conthead_rwlock_runlock_wlock(dm2_container_head_t *ch,
				  dm2_cache_type_t     *ct,
				  int		       *ch_locked)
{
    int macro_ret;

    assert(*ch_locked == DM2_CONT_HEAD_RLOCKED);
    if (unlikely(dm2_show_locks)) {
	DBG_DM2S("[pth=%ld/%ld] RW-RUNLOCK WLOCK cont_head: %s",
		 pthread_self(), gettid(), ch->ch_uri_dir);
    }
    NKN_MUTEX_LOCKR(&ct->ct_dm2_conthead_hash_table_mutex);

    /* Read unlock code */
    assert(ch->ch_rwlock_nreaders > 0);
    assert(ch->ch_rwlock_wowner == 0);
    assert(ch->ch_rwlock_nwriters == 0);
    assert(ch->ch_rwlock_lock_type == DM2_CONT_HEAD_RLOCKED);

    ch->ch_rwlock_nreaders--;
    if (ch->ch_rwlock_nreaders == 0) {
        ch->ch_rwlock_lock_type = 0;
        if (ch->ch_rwlock_wait_nwriters > 0) {
            PTHREAD_COND_BROADCAST(&ch->ch_rwlock_write_cond);
            ch->ch_rwlock_wait_nwriters = 0;
        }
    }

    *ch_locked = DM2_CONT_HEAD_UNLOCKED;

    /* Write lock code */
 loop:
    if (ch->ch_rwlock_nwriters > 0 || ch->ch_rwlock_nreaders > 0) {
        glob_dm2_conthead_lock_write_wait_cnt++;
        ch->ch_rwlock_wait_nwriters++;
        PTHREAD_COND_WAIT(&ch->ch_rwlock_write_cond,
			  &ct->ct_dm2_conthead_hash_table_mutex.lock);
        goto loop;
    } else {
        ch->ch_rwlock_nwriters = 1;
        ch->ch_rwlock_lock_type = DM2_CONT_HEAD_WLOCKED;
        ch->ch_rwlock_wowner = gettid();
    }

    *ch_locked = DM2_CONT_HEAD_WLOCKED;

    NKN_MUTEX_UNLOCKR(&ct->ct_dm2_conthead_hash_table_mutex);
}	/* dm2_conthead_rwlock_runlock_wlock */


void
dm2_cont_rwlock_init(dm2_container_t *cont)
{
    int ret;

    ret = pthread_rwlock_init(&cont->c_rwlock, NULL);
    assert(ret == 0);
}

void
dm2_cont_rwlock_destroy(dm2_container_t *cont)
{
    int ret;

    ret = pthread_rwlock_destroy(&cont->c_rwlock);
    assert(ret == 0);
    memset(&cont->c_rwlock, 3, sizeof(cont->c_rwlock));
}

void
dm2_cont_rwlock_rlock(dm2_container_t *cont)
{
    int ret;

    ret = pthread_rwlock_rdlock(&cont->c_rwlock);
    assert(ret == 0);
    if (unlikely(dm2_show_locks))
	DBG_DM2S("[pth=%ld/%ld] RW-RLOCK cont: %s",
		 pthread_self(), gettid(),cont->c_uri_dir);
}

void
dm2_cont_rwlock_runlock(dm2_container_t *cont)
{
    int ret;

    if (unlikely(dm2_show_locks))
	DBG_DM2S("[pth=%ld/%ld] RW-RUNLOCK cont: %s",
		 pthread_self(), gettid(), cont->c_uri_dir);
    assert(((signed int)cont->c_rwlock.__data.__nr_readers) > 0);
    ret = pthread_rwlock_unlock(&cont->c_rwlock);
    assert(ret == 0);
}

void
dm2_cont_rwlock_wlock(dm2_container_t *cont,
		      int *locked,
		      const char *str)
{
    int ret;

    ret = pthread_rwlock_wrlock(&cont->c_rwlock);
    assert(ret == 0);
    *locked = 1;
    cont->c_rflags |= DM2_CONT_RWFLAG_WLOCK;	// Debug flag
    if (unlikely(dm2_show_locks))
	DBG_DM2S("[pth=%ld/%ld] RW-WLOCK got cont: %s [%s][%p]",
		 pthread_self(), gettid(), cont->c_uri_dir, str, cont);
}

void
dm2_cont_rwlock_wunlock(dm2_container_t *cont,
			int *locked,
			const char *str)
{
    int ret;

    if (unlikely(dm2_show_locks))
	DBG_DM2S("[pth=%ld/%ld] RW-WUNLOCK cont: %s [%s][%p]",
		 pthread_self(), gettid(), cont->c_uri_dir, str, cont);
    cont->c_rflags &= ~DM2_CONT_RWFLAG_WLOCK;	// Debug flag
    assert(cont->c_rwlock.__data.__writer != 0);
    ret = pthread_rwlock_unlock(&cont->c_rwlock);
    *locked = 0;
    assert(ret == 0);
}


void
dm2_physid_rwlock_init(DM2_physid_head_t *ph)
{
    int ret;

    ret = pthread_rwlock_init(&ph->ph_rwlock, NULL);
    assert(ret == 0);
}

void
dm2_physid_rwlock_destroy(DM2_physid_head_t *ph)
{
    int ret;

    ret = pthread_rwlock_destroy(&ph->ph_rwlock);
    assert(ret == 0);
    memset(&ph->ph_rwlock, 4, sizeof(ph->ph_rwlock));
}

void
dm2_physid_rwlock_rlock(DM2_physid_head_t *ph)
{
    int ret;

    ret = pthread_rwlock_rdlock(&ph->ph_rwlock);
    assert(ret == 0);
    if (unlikely(dm2_show_locks))
	DBG_DM2S("RW-RLOCK physid_head: 0x%lx", ph->ph_physid);
}

void
dm2_physid_rwlock_runlock(DM2_physid_head_t *ph)
{
    int ret;

    if (unlikely(dm2_show_locks))
	DBG_DM2S("RW-RUNLOCK physid_head: 0x%lx", ph->ph_physid);
    ret = pthread_rwlock_unlock(&ph->ph_rwlock);
    assert(ret == 0);
}

void
dm2_physid_rwlock_wlock(DM2_physid_head_t *ph)
{
    int ret;

    ret = pthread_rwlock_wrlock(&ph->ph_rwlock);
    assert(ret == 0);
    if (unlikely(dm2_show_locks))
	DBG_DM2S("RW-WLOCK physid_head: 0x%lx", ph->ph_physid);
}

void
dm2_physid_rwlock_wunlock(DM2_physid_head_t *ph)
{
    int ret;

    if (unlikely(dm2_show_locks))
	DBG_DM2S("RW-WUNLOCK physid_head: 0x%lx", ph->ph_physid);
    ret = pthread_rwlock_unlock(&ph->ph_rwlock);
    assert(ret == 0);
}

void
dm2_attr_mutex_init(dm2_container_t *cont)
{
    int ret;

    ret = pthread_mutex_init(&cont->c_amutex, NULL);
    assert(ret == 0);
}

void
dm2_attr_mutex_destroy(dm2_container_t *cont)
{
    int ret;

    ret = pthread_mutex_destroy(&cont->c_amutex);
    assert(ret == 0);
    memset(&cont->c_amutex, 5, sizeof(cont->c_amutex));
}

void
dm2_attr_mutex_lock(dm2_container_t *cont)
{
    int ret;

    ret = pthread_mutex_lock(&cont->c_amutex);
    assert(ret == 0);
    if (unlikely(dm2_show_locks))
	DBG_DM2S("ATTR MUTEX LOCK attr: %s", cont->c_uri_dir);
}

void
dm2_attr_mutex_unlock(dm2_container_t *cont)
{
    int ret;

    if (unlikely(dm2_show_locks))
	DBG_DM2S("ATTR MUTEX UNLOCK attr: %s", cont->c_uri_dir);
    ret = pthread_mutex_unlock(&cont->c_amutex);
    assert(ret == 0);
}

void
dm2_slot_mutex_init(dm2_container_t *cont)
{
    int ret;

    ret = pthread_mutex_init(&cont->c_slot_mutex, NULL);
    assert(ret == 0);
}	/* dm2_slot_mutex_init */

void
dm2_slot_mutex_destroy(dm2_container_t *cont)
{
    int ret;

    ret = pthread_mutex_destroy(&cont->c_slot_mutex);
    assert(ret == 0);
    memset(&cont->c_slot_mutex, 6, sizeof(cont->c_amutex));
}

void
dm2_slot_mutex_lock(dm2_container_t *cont)
{
    int ret;

    ret = pthread_mutex_lock(&cont->c_slot_mutex);
    assert(ret == 0);
    if (unlikely(dm2_show_locks))
	DBG_DM2S("SLOT MUTEX LOCK attr: %s", cont->c_uri_dir);
}

void
dm2_slot_mutex_unlock(dm2_container_t *cont)
{
    int ret;

    if (unlikely(dm2_show_locks))
	DBG_DM2S("SLOT MUTEX UNLOCK attr: %s", cont->c_uri_dir);
    ret = pthread_mutex_unlock(&cont->c_slot_mutex);
    assert(ret == 0);
}

void
dm2_attr_slot_push_tail(dm2_container_t *cont,
			const off_t	loff)
{
    assert(sizeof(loff) == sizeof(gpointer));
    assert(loff % 512 == 0);
    NKN_ASSERT(loff % DM2_ATTR_TOT_DISK_SIZE == 0);

    dm2_slot_mutex_lock(cont);
    g_queue_push_tail(&cont->c_attr_slot_q, (gpointer)loff);
    dm2_slot_mutex_unlock(cont);
}	/* dm2_attr_slot_push_tail */

off_t
dm2_attr_slot_pop_head(dm2_container_t  *cont)
{
    gpointer loff;

    assert(sizeof(loff) == sizeof(gpointer));
    dm2_slot_mutex_lock(cont);
    if (g_queue_get_length(&cont->c_attr_slot_q) > 0)
	loff = g_queue_pop_head(&cont->c_attr_slot_q);
    else
	loff = (gpointer)-1;
    dm2_slot_mutex_unlock(cont);
    return (off_t)loff;
}	/* dm2_attr_slot_pop_head */

void
dm2_ext_slot_push_tail(dm2_container_t  *cont,
		       const off_t	loff)
{
    assert(sizeof(loff) == sizeof(gpointer));
    assert(loff % 512 == 0);

    dm2_slot_mutex_lock(cont);
    g_queue_push_tail(&cont->c_ext_slot_q, (gpointer)loff);
    dm2_slot_mutex_unlock(cont);
}	/* dm2_ext_slot_push_tail */

off_t
dm2_ext_slot_pop_head(dm2_container_t *cont)
{
    gpointer loff;

    assert(sizeof(loff) == sizeof(gpointer));
    dm2_slot_mutex_lock(cont);
    if (g_queue_get_length(&cont->c_ext_slot_q) > 0)
	loff = g_queue_pop_head(&cont->c_ext_slot_q);
    else
	loff = (gpointer)-1;
    dm2_slot_mutex_unlock(cont);
    return (off_t)loff;
}	/* dm2_ext_slot_pop_head */

void
dm2_db_mutex_init(dm2_disk_block_t *db)
{
    int ret;

    ret = pthread_mutex_init(&db->db_mutex, NULL);
    assert(ret == 0);
}	/* dm2_db_mutex_init */

void
dm2_db_mutex_destroy(dm2_disk_block_t *db)
{
    int ret;

    ret = pthread_mutex_destroy(&db->db_mutex);
    assert(ret == 0);
    memset(&db->db_mutex, 7, sizeof(db->db_mutex));
}	/* dm2_db_mutex_destroy */

void
dm2_db_mutex_lock(dm2_disk_block_t *db)
{
    int ret;

    ret = pthread_mutex_lock(&db->db_mutex);
    assert(ret == 0);
    if (unlikely(dm2_show_locks))
	DBG_DM2S("DISK DB MUTEX LOCK: %ld", db->db_start_page);
}	/* dm2_db_mutex_lock */

void
dm2_db_mutex_unlock(dm2_disk_block_t *db)
{
    int ret;

    if (unlikely(dm2_show_locks))
	DBG_DM2S("DISK DB MUTEX UNLOCK: %ld", db->db_start_page);
    ret = pthread_mutex_unlock(&db->db_mutex);
    assert(ret == 0);
}	/* dm2_db_mutex_unlock */

#endif	// NKN_DM2_GLOBAL_LOCK_DEBUG

