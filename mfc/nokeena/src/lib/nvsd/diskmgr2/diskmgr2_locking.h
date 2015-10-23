/*
 * (C) Copyright 2008 Nokeena Networks, Inc
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
#ifndef DISKMGR2_LOCKING_H
#define DISKMGR2_LOCKING_H
#include "nkn_diskmgr2_local.h"

#define PTHREAD_MUTEX_INIT(mutex_ptr) { \
	macro_ret = pthread_mutex_init(mutex_ptr, NULL); \
	assert(macro_ret == 0); \
    }
#define PTHREAD_MUTEX_LOCK(mutex_ptr) { \
	macro_ret = pthread_mutex_lock(mutex_ptr); \
	assert(macro_ret == 0); \
    }
#define PTHREAD_MUTEX_UNLOCK(mutex_ptr) { \
	macro_ret = pthread_mutex_unlock(mutex_ptr); \
	assert(macro_ret == 0); \
    }
#define PTHREAD_MUTEX_DESTROY(mutex_ptr) { \
	macro_ret = pthread_mutex_destroy(mutex_ptr); \
	assert(macro_ret == 0); \
}
#define PTHREAD_COND_INIT(cond, attr) { \
	macro_ret = pthread_cond_init(cond, attr); \
	assert(macro_ret == 0); \
    }
#define PTHREAD_COND_SIGNAL(cond) { \
	macro_ret = pthread_cond_signal(cond); \
	assert(macro_ret == 0); \
    }
#define PTHREAD_COND_BROADCAST(cond) { \
	macro_ret = pthread_cond_broadcast(cond); \
	assert(macro_ret == 0); \
    }
#define PTHREAD_COND_DESTROY(cond) { \
	macro_ret = pthread_cond_destroy(cond); \
	assert(macro_ret == 0); \
    }
#define PTHREAD_COND_WAIT(cond, mutex_ptr) { \
	macro_ret = pthread_cond_wait(cond, mutex_ptr); \
	assert(macro_ret == 0); \
    }

#define PTHREAD_COND_TIMED_WAIT(cond, mutex_ptr, ts) { \
	macro_ret = pthread_cond_timedwait(cond, mutex_ptr, ts); \
	if (macro_ret != ETIMEDOUT) \
	    assert(macro_ret == 0); \
    }

#define DM2_CT_RLOCKED 1
#define DM2_CT_WLOCKED 2

void dm2_g_queue_init(GQueue *q);

/* Locking */
#ifdef NKN_DM2_GLOBAL_LOCK_DEBUG
void dm2_ct_stat_mutex_lock(dm2_cache_type_t *ct, int *locked, const char *str);
void dm2_ct_stat_mutex_unlock(dm2_cache_type_t *ct, int *locked, char *str);
void dm2_ct_rwlock_init(dm2_cache_type_t *ct);
void dm2_ct_rwlock_rlock(dm2_cache_type_t *ct);
void dm2_ct_rwlock_runlock(dm2_cache_type_t *ct);
void dm2_ct_rwlock_wlock(dm2_cache_type_t *ct);
void dm2_ct_rwlock_wunlock(dm2_cache_type_t *ct);
void dm2_ct_uri_hash_rwlock_init(dm2_cache_type_t *ct);
void dm2_ct_uri_hash_rwlock_rlock(dm2_cache_type_t *ct, int *lock);
void dm2_ct_uri_hash_rwlock_runlock(dm2_cache_type_t *ct, int *lock);
void dm2_ct_uri_hash_rwlock_wlock(dm2_cache_type_t *ct);
void dm2_ct_uri_hash_rwlock_wunlock(dm2_cache_type_t *ct);
void dm2_ct_info_list_rwlock_init(dm2_cache_type_t *ct);
void dm2_ct_info_list_rwlock_rlock(dm2_cache_type_t *ct);
void dm2_ct_info_list_rwlock_runlock(dm2_cache_type_t *ct);
void dm2_ct_info_list_rwlock_wlock(dm2_cache_type_t *ct);
void dm2_ct_info_list_rwlock_wunlock(dm2_cache_type_t *ct);
void dm2_uri_lock_init(DM2_uri_t *uh);
void dm2_uri_lock_destroy(DM2_uri_t *uh);
void dm2_uri_rwlock_rlock(DM2_uri_t *uh);
int  dm2_uri_rwlock_try_rlock(DM2_uri_t *uh);
void dm2_uri_rwlock_runlock(DM2_uri_t *uh);
void dm2_uri_rwlock_wlock(DM2_uri_t *uh);
void dm2_uri_rwlock_wunlock(DM2_uri_t *uh);
void dm2_uri_rwlock_unlock(DM2_uri_t *uh);
void dm2_uri_ext_rwlock_rlock(DM2_uri_t *uh);
void dm2_uri_ext_rwlock_runlock(DM2_uri_t *uh);
void dm2_uri_ext_rwlock_wlock(DM2_uri_t *uh);
void dm2_uri_ext_rwlock_wunlock(DM2_uri_t *uh);
void dm2_conthead_rwlock_init(dm2_container_head_t *cont_head);
void dm2_conthead_rwlock_destroy(dm2_container_head_t *cont_head);
int dm2_conthead_rwlock_rlock_unsafe(dm2_container_head_t *cont_head,
				     dm2_cache_type_t *ct,
				     int *cont_head_locked,
				     int is_blocking);
int dm2_conthead_rwlock_rlock(dm2_container_head_t *cont_head,
			      dm2_cache_type_t *ct,
			      int *cont_head_locked, int is_blocking);
void dm2_conthead_rwlock_runlock(dm2_container_head_t *cont_head,
			         dm2_cache_type_t *ct,
			         int *cont_head_locked);
void dm2_conthead_rwlock_runlock_wlock(dm2_container_head_t *cont_head,
				       dm2_cache_type_t *ct,
				       int *cont_head_locked);
/* WARNING: no hash table mutex called
            but cond wait on hash table mutex called */
void dm2_conthead_rwlock_wlock_unsafe(dm2_container_head_t *cont_head,
				      dm2_cache_type_t *ct,
				      int *cont_head_locked);
void dm2_conthead_rwlock_wlock(dm2_container_head_t *cont_head,
			       dm2_cache_type_t *ct,
			       int *cont_head_locked);
void dm2_conthead_rwlock_wunlock(dm2_container_head_t *cont_head,
			         dm2_cache_type_t *ct,
				 int *cont_head_locked);
/* WARNING: no hash table mutex called */
void dm2_conthead_rwlock_wunlock_unsafe(dm2_container_head_t *cont_head,
				 	int *cont_head_locked);
void dm2_conthead_rwlock_unlock(dm2_container_head_t *cont_head,
			        dm2_cache_type_t *ct,
				int *cont_head_locked);
int dm2_conthead_free(dm2_container_head_t *ch,
		      dm2_cache_type_t     *ct,
		      int                  *cont_head_locked);

void dm2_cont_rwlock_init(dm2_container_t *cont);
void dm2_cont_rwlock_destroy(dm2_container_t *cont);
void dm2_cont_rwlock_rlock(dm2_container_t *cont);
void dm2_cont_rwlock_runlock(dm2_container_t *cont);
void dm2_cont_rwlock_wlock(dm2_container_t *cont, int *locked, const char *str);
void dm2_cont_rwlock_wunlock(dm2_container_t *cont, int *locked,
			     const char *str);
void dm2_cont_rwlock_if_wunlock(dm2_container_t *cont, const char *str);
struct DM2_physid_head;
void dm2_physid_rwlock_init(struct DM2_physid_head *ph);
void dm2_physid_rwlock_destroy(struct DM2_physid_head *ph);
void dm2_physid_rwlock_rlock(struct DM2_physid_head *ph);
void dm2_physid_rwlock_runlock(struct DM2_physid_head *ph);
void dm2_physid_rwlock_wlock(struct DM2_physid_head *ph);
void dm2_physid_rwlock_wunlock(struct DM2_physid_head *ph);
void dm2_attr_mutex_init(dm2_container_t *cont);
void dm2_attr_mutex_destroy(dm2_container_t *cont);
void dm2_attr_mutex_lock(dm2_container_t *cont);
void dm2_attr_mutex_unlock(dm2_container_t *cont);
void dm2_slot_mutex_init(dm2_container_t *cont);
void dm2_slot_mutex_destroy(dm2_container_t *cont);
void dm2_slot_mutex_lock(dm2_container_t *cont);
void dm2_slot_mutex_unlock(dm2_container_t *cont);
void dm2_ci_dev_rwlock_init(dm2_cache_info_t *ci);
void dm2_ci_dev_rwlock_rlock(dm2_cache_info_t *ci);
void dm2_ci_dev_rwlock_runlock(dm2_cache_info_t *ci);
void dm2_ci_dev_rwlock_wlock(dm2_cache_info_t *ci);
void dm2_ci_dev_rwlock_wunlock(dm2_cache_info_t *ci);

void dm2_attr_slot_push_tail(dm2_container_t *cont, const off_t	loff);
off_t dm2_attr_slot_pop_head(dm2_container_t *cont);
void dm2_ext_slot_push_tail(dm2_container_t *cont, const off_t loff);
off_t dm2_ext_slot_pop_head(dm2_container_t *cont);

void dm2_db_mutex_init(dm2_disk_block_t *db);
void dm2_db_mutex_destroy(dm2_disk_block_t *db);
void dm2_db_mutex_lock(dm2_disk_block_t *db);
void dm2_db_mutex_unlock(dm2_disk_block_t *db);

#else	/* NKN_DM2_GLOBAL_LOCK_DEBUG */

static inline void
dm2_ct_rwlock_init(dm2_cache_type_t *ct)
{
    int ret;

    NKN_ASSERT(ct);
    ret = pthread_rwlock_init(&ct->ct_rwlock, NULL);
    assert(ret == 0);
}

static inline void
dm2_ct_rwlock_rlock(dm2_cache_type_t *ct)
{
    int ret;

    NKN_ASSERT(ct);
    if (dm2_show_locks)
	DBG_DM2S("RW-RLOCK ct: %s", ct->ct_name);
    ret = pthread_rwlock_rdlock(&ct->ct_rwlock);
    assert(ret == 0);
}

static inline void
dm2_ct_rwlock_runlock(dm2_cache_type_t *ct)
{
    int ret;

    NKN_ASSERT(ct);
    if (dm2_show_locks)
	DBG_DM2S("RW-RUNLOCK ct: %s", ct->ct_name);
    ret = pthread_rwlock_unlock(&ct->ct_rwlock);
    assert(ret == 0);
}

static inline void
dm2_ct_rwlock_wlock(dm2_cache_type_t *ct)
{
    int ret;

    NKN_ASSERT(ct);
    if (dm2_show_locks)
	DBG_DM2S("RW-WLOCK ct: %s", ct->ct_name);
    ret = pthread_rwlock_wrlock(&ct->ct_rwlock);
    assert(ret == 0);
}

static inline void
dm2_ct_rwlock_wunlock(dm2_cache_type_t *ct)
{
    int ret;

    NKN_ASSERT(ct);
    if (dm2_show_locks)
	DBG_DM2S("RW-WUNLOCK ct: %s", ct->ct_name);
    ret = pthread_rwlock_unlock(&ct->ct_rwlock);
    assert(ret == 0);
}

static inline void
dm2_ct_info_list_rwlock_init(dm2_cache_type_t *ct)
{
    int ret;

    NKN_ASSERT(ct);
    ret = pthread_rwlock_init(&ct->ct_info_list_rwlock, NULL);
    assert(ret == 0);
}

static inline void
dm2_ct_info_list_rwlock_rlock(dm2_cache_type_t *ct)
{
    int ret;

    NKN_ASSERT(ct);
    if (dm2_show_locks)
	DBG_DM2S("RW-RLOCK ct: %s", ct->ct_name);
    ret = pthread_rwlock_rdlock(&ct->ct_info_list_rwlock);
    assert(ret == 0);
}

static inline void
dm2_ct_info_list_rwlock_runlock(dm2_cache_type_t *ct)
{
    int ret;

    NKN_ASSERT(ct);
    if (dm2_show_locks)
	DBG_DM2S("RW-RUNLOCK ct: %s", ct->ct_name);
    ret = pthread_rwlock_unlock(&ct->ct_info_list_rwlock);
    assert(ret == 0);
}

static inline void
dm2_ct_info_list_rwlock_wlock(dm2_cache_type_t *ct)
{
    int ret;

    NKN_ASSERT(ct);
    if (dm2_show_locks)
	DBG_DM2S("RW-WLOCK ct: %s", ct->ct_name);
    ret = pthread_rwlock_wrlock(&ct->ct_info_list_rwlock);
    assert(ret == 0);
}

static inline void
dm2_ct_info_list_rwlock_wunlock(dm2_cache_type_t *ct)
{
    int ret;

    NKN_ASSERT(ct);
    if (dm2_show_locks)
	DBG_DM2S("RW-WUNLOCK ct: %s", ct->ct_name);
    ret = pthread_rwlock_unlock(&ct->ct_info_list_rwlock);
    assert(ret == 0);
}

static inline void
dm2_uri_lock_init(DM2_uri_t *uh)
{
    int ret;

    NKN_ASSERT(uh);
    ret = pthread_mutex_init(&uh->uri_ext_mutex, NULL);
    assert(ret == 0);
}

static inline void
dm2_uri_lock_destroy(DM2_uri_t *uh)
{
    int ret;

    NKN_ASSERT(uh);
    ret = pthread_mutex_destroy(&uh->uri_ext_mutex);
    assert(ret == 0);
    memset(&uh->uri_ext_mutex, 2, sizeof(uh->uri_ext_mutex));
}


static inline void
dm2_uri_ext_rwlock_rlock(DM2_uri_t *uh)
{
    int ret;

    NKN_ASSERT(uh);
    /* We can't know the object lock state here because we don't change it
     * at the unlocked state */
    if (dm2_show_locks)
	DBG_DM2S("MUTEX-LOCK_EXT uri: %s", uh->uri_name);
    ret = pthread_mutex_lock(&uh->uri_ext_mutex);
    assert(ret == 0);
}

static inline void
dm2_uri_ext_rwlock_runlock(DM2_uri_t *uh)
{
    int ret;

    NKN_ASSERT(uh);
    if (dm2_show_locks)
	DBG_DM2S("MUTEX-UNLOCK_EXT uri: %s", uh->uri_name);
    ret = pthread_mutex_unlock(&uh->uri_ext_mutex);
    /* We can't know the object lock state here */
    assert(ret == 0);
}

static inline void
dm2_uri_ext_rwlock_wlock(DM2_uri_t *uh)
{
    int ret;

    NKN_ASSERT(uh);
    /* We can't know the object lock state here because we don't change it
     * at the unlocked state */
    if (dm2_show_locks)
	DBG_DM2S("MUTEX-LOCK_EXT uri: %s", uh->uri_name);
    ret = pthread_mutex_lock(&uh->uri_ext_mutex);
    assert(ret == 0);
}

static inline void
dm2_uri_ext_rwlock_wunlock(DM2_uri_t *uh)
{
    int ret;

    NKN_ASSERT(uh);
    if (dm2_show_locks)
	DBG_DM2S("MUTEX-UNLOCK_EXT uri: %s", uh->uri_name);
    ret = pthread_mutex_unlock(&uh->uri_ext_mutex);
    /* We can't know the object lock state here */
    assert(ret == 0);
}

static inline void
dm2_conthead_rwlock_init(dm2_container_head_t *cont_head)
{
    int macro_ret;

    PTHREAD_COND_INIT(&conthead->ch_rwlock_read_cond, NULL);
    PTHREAD_COND_INIT(&conthead->ch_rwlock_write_cond, NULL);
    conthead->ch_rwlock_lock_type = 0;
}

static inline void
dm2_conthead_rwlock_destroy(dm2_container_head_t *cont_head)
{
    int macro_ret;

    PTHREAD_COND_DESTROY(&conthead->ch_rwlock_read_cond);
    PTHREAD_COND_DESTROY(&conthead->ch_rwlock_write_cond);
}

static inline void
dm2_conthead_rwlock_rlock(dm2_container_head_t *cont_head,
			  int                  *cont_head_locked,
			  int		       is_blocking)
{
    int ret = 0;

    assert(*cont_head_locked == DM2_CONT_HEAD_UNLOCKED);
    if (is_blocking)
	ret = pthread_rwlock_rdlock(&cont_head->ch_rwlock);
    else {
	ret = pthread_rwlock_tryrdlock(&cont_head->ch_rwlock);
        /* If unable to acquire the lock, bail out as we do not want to block*/
        if (ret == EBUSY) {
            glob_dm2_conthead_busy_cnt++;
            goto end;
        }
    }

    assert(ret == 0);
    *cont_head_locked = DM2_CONT_HEAD_RLOCKED;
 end:
    if (unlikely(dm2_show_locks))
        DBG_DM2S("[pth=%d/%d] RW-RLOCK cont_head: %s",
                 pthread_self(), gettid(), cont_head->ch_uri_dir);

    return ret;
}

static inline void
dm2_conthead_rwlock_runlock(dm2_container_head_t *cont_head,
                            int                  *cont_head_locked)
{
    int ret;

    assert(*cont_head_locked == DM2_CONT_HEAD_RLOCKED);
    if (unlikely(dm2_show_locks))
        DBG_DM2S("[pth=%d/%d] RW-RUNLOCK cont_head: %s",
                 pthread_self(), gettid(), cont_head->ch_uri_dir);
    assert(cont_head->ch_rwlock.__data.__nr_readers);
    ret = pthread_rwlock_unlock(&cont_head->ch_rwlock);
    assert(ret == 0);
    *cont_head_locked = DM2_CONT_HEAD_UNLOCKED;
}

static inline void
dm2_conthead_rwlock_wlock(dm2_container_head_t *cont_head,
                          int                  *cont_head_locked)
{
    int ret;

    assert(*cont_head_locked == DM2_CONT_HEAD_UNLOCKED);
    ret = pthread_rwlock_wrlock(&cont_head->ch_rwlock);
    assert(ret == 0);
    *cont_head_locked = DM2_CONT_HEAD_WLOCKED;
    if (unlikely(dm2_show_locks))
        DBG_DM2S("[pth=%d/%d] RW-WLOCK cont_head: %s",
                 pthread_self(), gettid(), cont_head->ch_uri_dir);
}

static inline void
dm2_conthead_rwlock_wunlock(dm2_container_head_t *cont_head,
                            int                  *cont_head_locked)
{
    int ret;

    assert(*cont_head_locked == DM2_CONT_HEAD_WLOCKED);
    if (unlikely(dm2_show_locks))
        DBG_DM2S("[pth=%d/%d] RW-WUNLOCK cont_head: %s",
                 pthread_self(), gettid(),cont_head->ch_uri_dir);
    ret = pthread_rwlock_unlock(&cont_head->ch_rwlock);
    assert(ret == 0);
    *cont_head_locked = DM2_CONT_HEAD_UNLOCKED;
}

static inline void
dm2_conthead_rwlock_unlock(dm2_container_head_t *cont_head,
                           int                  *cont_head_locked)
{
    int ret;

    assert(*cont_head_locked != DM2_CONT_HEAD_UNLOCKED);
    if (unlikely(dm2_show_locks)) {
        if (*cont_head_locked == DM2_CONT_HEAD_WLOCKED) {
            DBG_DM2S("[pth=%d/%d] RW-WUNLOCK cont_head: %s",
                     pthread_self(), gettid(), cont_head->ch_uri_dir);
        } else if (*cont_head_locked == DM2_CONT_HEAD_RLOCKED) {
            DBG_DM2S("[pth=%d/%d] RW-RUNLOCK cont_head: %s",
                     pthread_self(), gettid(), cont_head->ch_uri_dir);
        } else
            assert(0);  // corruption or logic error
    }
    ret = pthread_rwlock_unlock(&cont_head->ch_rwlock);
    assert(ret == 0);
    *cont_head_locked = DM2_CONT_HEAD_UNLOCKED;
}

static inline void
dm2_cont_rwlock_init(dm2_container_t *cont)
{
    int ret;

    assert(cont);
    ret = pthread_rwlock_init(&cont->ch_rwlock, NULL);
    assert(ret == 0);
}

static inline void
dm2_cont_rwlock_destroy(dm2_container_t *cont)
{
    int ret;

    assert(cont);
    assert(cont->c_magicnum != NKN_CONTAINER_MAGIC_FREE);
    ret = pthread_rwlock_destroy(&cont->ch_rwlock);
    assert(ret == 0);
    memset(&cont->ch_rwlock, 3, sizeof(cont->ch_rwlock));
}

static inline void
dm2_cont_rwlock_rlock(dm2_container_t *cont)
{
    int ret;

    assert(cont);
    if (dm2_show_locks)
	DBG_DM2S("RW-RLOCK cont: %s", cont->ch_uri_dir);
    ret = pthread_rwlock_rdlock(&cont->ch_rwlock);
    assert(ret == 0);
}

static inline void
dm2_cont_rwlock_runlock(dm2_container_t *cont)
{
    int ret;

    assert(cont);
    if (dm2_show_locks)
	DBG_DM2S("RW-RUNLOCK cont: %s", cont->ch_uri_dir);
    ret = pthread_rwlock_unlock(&cont->ch_rwlock);
    assert(ret == 0);
}

static inline void
dm2_cont_rwlock_wlock(dm2_container_t *cont)
{
    int ret;

    assert(cont);
    if (dm2_show_locks)
	DBG_DM2S("RW-WLOCK cont: %s", cont->ch_uri_dir);
    ret = pthread_rwlock_wrlock(&cont->ch_rwlock);
    assert(ret == 0);
    cont->c_rflags |= DM2_CONT_RWFLAG_WLOCK;
}

static inline void
dm2_cont_rwlock_wunlock(dm2_container_t *cont)
{
    int ret;

    assert(cont);
    if (dm2_show_locks)
	DBG_DM2S("RW-WUNLOCK cont: %s", cont->ch_uri_dir);
    ret = pthread_rwlock_unlock(&cont->ch_rwlock);
    assert(ret == 0);
    cont->c_rflags &= ~DM2_CONT_RWFLAG_WLOCK;
}

static inline void
dm2_cont_rwlock_if_wunlock(dm2_container_t *cont)
{
    int ret;

    assert(cont);
    if (dm2_show_locks)
	DBG_DM2S("RW-IF-WUNLOCK cont: %s", cont->ch_uri_dir);
    if ((cont->c_rflags & DM2_CONT_RWFLAG_WLOCK) == DM2_CONT_RWFLAG_WLOCK) {
	ret = pthread_rwlock_unlock(&cont->ch_rwlock);
	assert(ret == 0);
	cont->c_rflags &= ~DM2_CONT_RWFLAG_WLOCK;
    }
}

static inline void
dm2_physid_rwlock_init(DM2_physid_head_t *ph)
{
    int ret;

    NKN_ASSERT(ph);
    ret = pthread_rwlock_init(&ph->ph_rwlock, NULL);
    assert(ret == 0);
}

static inline void
dm2_physid_rwlock_destroy(DM2_physid_head_t *ph)
{
    int ret;

    NKN_ASSERT(ph);
    ret = pthread_rwlock_destroy(&ph->ph_rwlock);
    assert(ret == 0);
    memset(&ph->ph_rwlock, 0, sizeof(ph->ph_rwlock));
}

static inline void
dm2_physid_rwlock_rlock(DM2_physid_head_t *ph)
{
    int ret;

    NKN_ASSERT(ph);
    if (dm2_show_locks)
	DBG_DM2S("RW-RLOCK physid_head: 0x%lx", ph->ph_physid);
    ret = pthread_rwlock_rdlock(&ph->ph_rwlock);
    assert(ret == 0);
}

static inline void
dm2_physid_rwlock_runlock(DM2_physid_head_t *ph)
{
    int ret;

    NKN_ASSERT(ph);
    if (dm2_show_locks)
	DBG_DM2S("RW-RUNLOCK physid_head: 0x%lx", ph->ph_physid);
    ret = pthread_rwlock_unlock(&ph->ph_rwlock);
    assert(ret == 0);
}

static inline void
dm2_physid_rwlock_wlock(DM2_physid_head_t *ph)
{
    int ret;

    NKN_ASSERT(ph);
    if (dm2_show_locks)
	DBG_DM2S("RW-WLOCK physid_head: 0x%lx", ph->ph_physid);
    ret = pthread_rwlock_wrlock(&ph->ph_rwlock);
    assert(ret == 0);
}

static inline void
dm2_physid_rwlock_wunlock(DM2_physid_head_t *ph)
{
    int ret;

    NKN_ASSERT(ph);
    if (dm2_show_locks)
	DBG_DM2S("RW-WUNLOCK physid_head: 0x%lx", ph->ph_physid);
    ret = pthread_rwlock_unlock(&ph->ph_rwlock);
    assert(ret == 0);
}

static inline void
dm2_attr_mutex_init(dm2_container_t *cont)
{
    int ret;

    NKN_ASSERT(cont);
    ret = pthread_mutex_init(&cont->c_amutex, NULL);
    assert(ret == 0);
}

static inline void
dm2_attr_mutex_destroy(dm2_container_t *cont)
{
    int ret;

    NKN_ASSERT(cont);
    ret = pthread_mutex_destroy(&cont->c_amutex);
    assert(ret == 0);
    memset(&cont->c_amutex, 0x14, sizeof(cont->c_amutex));
}

static inline void
dm2_attr_mutex_lock(dm2_container_t *cont)
{
    int ret;

    NKN_ASSERT(cont);
    if (dm2_show_locks)
	DBG_DM2S("ATTR MUTEX LOCK attr: %s", cont->c_uri_dir);
    ret = pthread_mutex_lock(&cont->c_amutex);
    assert(ret == 0);
}

static inline void
dm2_attr_mutex_unlock(dm2_container_t *cont)
{
    int ret;

    NKN_ASSERT(cont);
    if (dm2_show_locks)
	DBG_DM2S("ATTR MUTEX UNLOCK attr: %s", cont->c_uri_dir);
    ret = pthread_mutex_unlock(&cont->c_amutex);
    assert(ret == 0);
}

static inline void
dm2_slot_mutex_init(dm2_container_t *cont)
{
    int ret;

    NKN_ASSERT(cont);
    ret = pthread_mutex_init(&cont->c_slot_mutex, NULL);
    assert(ret == 0);
}

static inline void
dm2_slot_mutex_destroy(dm2_container_t *cont)
{
    int ret;

    NKN_ASSERT(cont);
    ret = pthread_mutex_destroy(&cont->c_slot_mutex);
    assert(ret == 0);
    memset(&cont->c_slot_mutex, 6, sizeof(cont->c_amutex));
}

static inline void
dm2_slot_mutex_lock(dm2_container_t *cont)
{
    int ret;

    NKN_ASSERT(cont);
    if (dm2_show_locks)
	DBG_DM2S("SLOT MUTEX LOCK attr: %s", cont->c_uri_dir);
    ret = pthread_mutex_lock(&cont->c_slot_mutex);
    assert(ret == 0);
}

static inline void
dm2_slot_mutex_unlock(dm2_container_t *cont)
{
    int ret;

    NKN_ASSERT(cont);
    if (dm2_show_locks)
	DBG_DM2S("SLOT MUTEX UNLOCK attr: %s", cont->c_uri_dir);
    ret = pthread_mutex_unlock(&cont->c_slot_mutex);
    assert(ret == 0);
}

static inline void
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

static inline off_t
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

static inline void
dm2_ext_slot_push_tail(dm2_container_t  *cont,
		       const off_t	loff)
{
    assert(sizeof(loff) == sizeof(gpointer));
    assert(loff % 512 == 0);

    dm2_slot_mutex_lock(cont);
    g_queue_push_tail(&cont->c_ext_slot_q, (gpointer)loff);
    dm2_slot_mutex_unlock(cont);
}	/* dm2_ext_slot_push_tail */

static inline off_t
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

static inline void
dm2_db_mutex_init(dm2_disk_block_t *db)
{
    int ret;

    ret = pthread_mutex_init(&db->db_mutex, NULL);
    assert(ret == 0);
}	/* dm2_db_mutex_init */

static inline void
dm2_db_mutex_destroy(dm2_disk_block_t *db)
{
    int ret;

    ret = pthread_mutex_destroy(&db->db_mutex);
    assert(ret == 0);
    memset(&db->db_mutex, 7, sizeof(db->db_mutex));
}	/* dm2_db_mutex_destroy */

static inline void
dm2_db_mutex_lock(dm2_disk_block_t *db)
{
    int ret;

    ret = pthread_mutex_lock(&db->db_mutex);
    assert(ret == 0);
    if (unlikely(dm2_show_locks))
	DBG_DM2S("DISK DB MUTEX LOCK: %ld", db->db_start_page);
}	/* dm2_db_mutex_lock */

static inline void
dm2_db_mutex_unlock(dm2_disk_block_t *db)
{
    int ret;

    if (unlikely(dm2_show_locks))
	DBG_DM2S("DISK DB MUTEX UNLOCK: %ld", db->db_start_page);
    ret = pthread_mutex_unlock(&db->db_mutex);
    assert(ret == 0);
}	/* dm2_db_mutex_unlock */
#endif   // NKN_DM2_GLOBAL_LOCK_DEBUG

#endif /* DISKMGR2_LOCKING_H */
