/*
 *	COPYRIGHT: Juniper Networks
 *
 * This file contains code which implements the Disk Manager
 *
 * Author: Srihari MS
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
#include <sys/prctl.h>
#include <time.h>

#include "nkn_locmgr2_uri.h"
#include "nkn_locmgr2_container.h"
#include "nkn_diskmgr2_local.h"
#include "nkn_diskmgr2_disk_api.h"
#include "nkn_diskmgr2_local.h"
#include "nkn_debug.h"
#include "nkn_assert.h"
#include "diskmgr2_util.h"
#include "diskmgr2_common.h"
#include "diskmgr2_locking.h"
#include "nkn_util.h"

#define DM2_DICT_EVICT_NUM_BLOCKS (10000)

/* DM2 Global evict thread */
pthread_t dm2_evict_thread;

int64_t  glob_dm2_prov_hot_threshold_1 = 3,
    glob_dm2_prov_hot_threshold_5 = 3,
    glob_dm2_prov_hot_threshold_6 = 3,
    glob_dm2_int_evict_cnt,
    glob_dm2_int_evict_enoent,
    glob_dm2_int_evict_dict_full,
    glob_dm2_int_evict_disk_full,
    glob_dm2_int_evict_time_cnt,
    glob_dm2_int_evict_tier_empty_err,
    glob_dm2_int_evict_skip_new_uri,
    glob_dm2_int_evict_skip_bad_uri,
    glob_dm2_int_evict_skip_preread,
    glob_dm2_int_evict_stop_less_blks,
    glob_dm2_log_decay;

int glob_dm2_last_dict_full_int_evict_ts,
    glob_dm2_last_disk_full_int_evict_ts,
    glob_dm2_dict_int_evict_percent;

extern int glob_dm2_dict_int_evict_hwm,
	   glob_dm2_dict_int_evict_lwm,
	   glob_dm2_dict_int_evict_in_progress;

extern int glob_cachemgr_init_done;

void dm2_evict_print_list(dm2_cache_info_t *ci, int bucket, FILE* fp);
void dm2_evict_dump_list(const char *disk_name, int bucket);
void dm2_set_num_evict_percent(const char* tier, int percent);


static inline void
dm2_evict_list_mutex_init(dm2_cache_info_t *ci,
			  int bucket)
{
    int ret;

    ret = pthread_mutex_init(&ci->evict_buckets[bucket].evict_list_mutex, NULL);
    assert(ret == 0);
}   /* dm2_evict_list_mutex_init */

static inline void
dm2_evict_list_mutex_lock(dm2_cache_info_t *ci,
			  int bucket)
{
    int ret;

    ret = pthread_mutex_lock(&ci->evict_buckets[bucket].evict_list_mutex);
    assert(ret == 0);
}   /* dm2_evict_list_mutex_lock */

static inline void
dm2_evict_list_mutex_unlock(dm2_cache_info_t *ci,
			    int bucket)
{
    int ret;

    ret = pthread_mutex_unlock(&ci->evict_buckets[bucket].evict_list_mutex);
    assert(ret == 0);
}   /* dm2_evict_list_mutex_unlock */

/* 'dm2_get_hotness_bucket' helps find a bucket for an object based
 * on its hotness.
 */
static int
dm2_get_hotness_bucket(int hotness)
{
    int bkt = 0;

    if (hotness < 0)
	assert(0);

     /* The following are the hotness to evict bucket mappings
     * 0-128 -> Individual Buckets
     * 129-255 -> 129
     * 256-511 -> 130 and so on.
     */

    if (hotness <= DM2_UNIQ_EVICT_BKTS)
	return hotness;

    while(hotness >>= 1) /* find MSB! */
        ++bkt;

    /* Map the hotness to the corresponding bucket */
    bkt = (bkt + DM2_UNIQ_EVICT_BKTS - DM2_MAP_UNIQ_BKTS + 1);

    return bkt;

}   /* dm2_get_hotness_bucket */

void
dm2_evict_print_list(dm2_cache_info_t *ci, int bucket, FILE* fp)
{
    DM2_uri_t *uh = NULL, *uh_tmp = NULL;
    char *ptime;
    int header = 0;
    int32_t update_time;

    dm2_evict_list_mutex_lock(ci, bucket);
    TAILQ_FOREACH_SAFE(uh, &ci->evict_buckets[bucket].dm2_evict_list_t,
		       evict_entries, uh_tmp) {
	if (!uh)
	    goto end;

	if (!header) {
	    fprintf(fp, "uri, hotness, orig_hotness, last_update_time\n");
	    header = 1;
	}

	update_time = am_decode_update_time(&uh->uri_hotval);
	ptime = dm2_get_time_single_token(update_time);
	fprintf(fp, "%s/%s, %d, %d, %s\n",
		uh->uri_container->c_uri_dir, dm2_uh_uri_basename(uh),
		am_decode_hotness(&uh->uri_hotval), uh->uri_orig_hotness,
		ptime);
    }
 end:
    dm2_evict_list_mutex_unlock(ci, bucket);
    return;
}   /* dm2_evict_print_list */

void
dm2_evict_dump_list(const char *disk_name, int bucket)
{
    dm2_cache_info_t *ci;
    dm2_cache_type_t *ct;
    GList *ci_obj;
    FILE* fp = NULL;
    char fname[1024];
    int i, found = 0;

    for (i = 0; i < glob_dm2_num_cache_types; i++) {
	ct = dm2_get_cache_type(i);
	dm2_ct_info_list_rwlock_rlock(ct);
	for (ci_obj = ct->ct_info_list; ci_obj; ci_obj = ci_obj->next) {
	    ci = (dm2_cache_info_t *)ci_obj->data;
	    if (!strncmp(ci->mgmt_name, disk_name, DM2_MAX_MGMTNAME)) {
		found = 1;
		break;
	    }
	}
	dm2_ct_info_list_rwlock_runlock(ct);
	if (found)
	    break;
    }

    snprintf(fname, 512, "/nkn/tmp/evict_list_%s_%ld.csv",
	     ci->mgmt_name, nkn_cur_ts);
    fp = fopen(fname, "w+");
    if (!fp) {
	DBG_DM2S("Unable to open file %s", fname);
	return;
    }

    if (bucket == -1) {
	for (i = 0; i < DM2_MAX_EVICT_BUCKETS; i++) {
	    dm2_evict_print_list(ci, i, fp);
	}
    } else {
	dm2_evict_print_list(ci, bucket, fp);
    }
    fflush(fp);
    fclose(fp);
    return;
}   /* dm2_evict_dump_list */

/* 'dm2_evict_init_buckets' initializes the buckets for evict list. The evict
 * list is a TAILQ (doubly linked list) of the uri_head objects. This API
 * is invoked for every object during init
 */
int
dm2_evict_init_buckets(dm2_cache_info_t *ci)
{
    int i;
    dm2_evict_node_t *temp = NULL;

    temp = dm2_calloc(DM2_MAX_EVICT_BUCKETS, sizeof(dm2_evict_node_t),
		      mod_dm2_evict_node_t);
    if (temp == NULL)
	return -ENOMEM;

    ci->evict_buckets = temp;

    for (i=0; i<DM2_MAX_EVICT_BUCKETS; i++) {
	TAILQ_INIT(&ci->evict_buckets[i].dm2_evict_list_t);
	dm2_evict_list_mutex_init(ci, i);
	ci->evict_buckets[i].num_entries = 0;
    }

    return 0;
}   /* dm2_evict_init_buckets */

/* 'dm2_evict_add_uri' adds an uri to the evict list based on its hotness */
void
dm2_evict_add_uri(DM2_uri_t *uri_head,
		  int hotness)
{
    dm2_cache_info_t *ci;
    dm2_cache_type_t *ct;
    int bucket, blks_used, block_size;

    /* Pinned objects cannot be evicted and hence need not be added to
     * the evict list */
    if (uri_head->uri_cache_pinned)
	return;

    if (hotness < 0) {
	DBG_DM2S("Invalid hotness value (%d) for URI : %s", hotness,
		 uri_head->uri_name);
	assert(0);
	return;
    }
    ci = uri_head->uri_container->c_dev_ci;
    assert(ci);
    bucket = dm2_get_hotness_bucket(hotness);
    if (bucket < 0) {
	assert(0);
	return;
    }

    /* We should not be inserting duplicates to evict list */
    if (uri_head->uri_evict_list_add != 0) {
	DBG_DM2S("[Cache:%s] Cont:%s, URI:%s present in evict list",
		 ci->mgmt_name, uri_head->uri_container->c_uri_dir,
		 uri_head->uri_name);
        NKN_ASSERT(0);
	return;
    }

    ct = ci->ci_cttype;
    dm2_evict_list_mutex_lock(ci, bucket);
    TAILQ_INSERT_TAIL(&ci->evict_buckets[bucket].dm2_evict_list_t,
		      uri_head,
		      evict_entries);
    block_size = dm2_get_blocksize(ci);
    blks_used = (uri_head->uri_content_len / block_size);
    if (uri_head->uri_content_len % block_size)
	blks_used++;
    ci->evict_buckets[bucket].num_entries++;
    ci->evict_buckets[bucket].num_blks += blks_used;
    AO_fetch_and_add(&ct->ct_dm2_bkt_evict_blks[bucket], blks_used);

    uri_head->uri_evict_list_add = 1;
    dm2_evict_list_mutex_unlock(ci, bucket);

    return;
}   /* dm2_evict_add_uri */

/* 'dm2_evict_delete_uri' removes an uri from the evict list */
void
dm2_evict_delete_uri(DM2_uri_t *uri_head,
		     int hotness)
{
    dm2_cache_info_t *ci;
    dm2_cache_type_t *ct;
    int bucket, rem_blks, blks_used, block_size;

    if (hotness < 0) {
	DBG_DM2S("Invalid hotness value (%d) for URI : %s", hotness,
		 uri_head->uri_name);
	NKN_ASSERT(0);
	return;
    }

    if (uri_head->uri_evict_list_add == 0) {
	DBG_DM2E("Attempting to remove URI %s (hotness:%d) that was not added"
		 "to evict list", uri_head->uri_name, hotness);
	return;
    }

    /* If the hotness got changed bcas of a race, assert */
    if (hotness != am_decode_hotness(&uri_head->uri_hotval))
	assert(0);

    ci = uri_head->uri_container->c_dev_ci;
    assert(ci);
    bucket = dm2_get_hotness_bucket(hotness);
    if (bucket < 0) {
	NKN_ASSERT(0);
	return;
    }

    ct = ci->ci_cttype;
    dm2_evict_list_mutex_lock(ci, bucket);
    TAILQ_REMOVE(&ci->evict_buckets[bucket].dm2_evict_list_t,
		 uri_head,
		 evict_entries);
    block_size = dm2_get_blocksize(ci);
    blks_used = (uri_head->uri_content_len / block_size);
    if (uri_head->uri_content_len % block_size)
	blks_used++;
    ci->evict_buckets[bucket].num_entries--;
    /* If an URI used more blocks than initially that was accounted for,
     * the accounting will go negative. Ensure that we do the subtraction
     * such that we dont make the values negative. This is a hack and the real
     * change would need to track block allocation and account it against
     * the evict buckets */
    if (ci->evict_buckets[bucket].num_blks >= (uint32_t)blks_used) {
	ci->evict_buckets[bucket].num_blks -= blks_used;
	AO_fetch_and_add(&ct->ct_dm2_bkt_evict_blks[bucket], -blks_used);
    } else {
	rem_blks = ci->evict_buckets[bucket].num_blks;
	ci->evict_buckets[bucket].num_blks -= rem_blks;
	AO_fetch_and_add(&ct->ct_dm2_bkt_evict_blks[bucket], -rem_blks);
    }
    uri_head->uri_evict_list_add = 0;
    dm2_evict_list_mutex_unlock(ci, bucket);

    return;
}   /* dm2_evict_delete_uri */

/* 'dm2_evict_promote_uri' removes the uri from its existing list and
 * adds it to the evict list of the new hotness bucket */
void
dm2_evict_promote_uri(DM2_uri_t *uri_head,
		      int old_hotness,
		      int new_hotness)
{

    int new_bkt, old_bkt;

    new_bkt = dm2_get_hotness_bucket(new_hotness);
    old_bkt = dm2_get_hotness_bucket(old_hotness);

    /* '0' is a valid bucket as we allow hotness values of '0' */
    if (new_bkt < 0 || old_bkt < 0) {
	NKN_ASSERT(0);
	return;
    }

    if (new_bkt != old_bkt) {
	/* Remove the uri_head from the current evict_list. We will have
	 * the uri_head wlocked and grab a mutex on the evict list inside
	 * the delete_uri function */
        dm2_evict_delete_uri(uri_head, old_hotness);

	/* The 'uri_evict_list_add ' should be rest if the delete
	 * succeeded. It is really bad to have this set in here */
	if (uri_head->uri_evict_list_add != 0)
	    assert(0);

	/* Add the uri_head to the new evict list */
	dm2_evict_add_uri(uri_head, new_hotness);
    }

    return;
}   /* dm2_evict_promote_uri */

/* 'dm2_evict_get_next_uri' returns the first object (from the head) in the
 * evict list for the requested bucket. The uri object should be wlocked
 * before hadning off as it is going to be DELETE'd.
 */
static DM2_uri_t*
dm2_evict_get_next_uri(dm2_cache_info_t *ci,
		       int bucket,
		       int is_delete)
{
    DM2_uri_t *uri_head = NULL;

    dm2_evict_list_mutex_lock(ci, bucket);

    if (ci->evict_buckets[bucket].num_entries == 0) {
	dm2_evict_list_mutex_unlock(ci, bucket);
	return NULL;
    }

    /* Get the first entry from the list */
    uri_head = TAILQ_FIRST(&ci->evict_buckets[bucket].dm2_evict_list_t);

    /* Bug 7988 - Hack!! We should not have a condition where the uri_head
     * is NULL and the num_entries is non-zero. For now, handle this by
     * returning NULL, until the root cause is known */
    if ((ci->evict_buckets[bucket].num_entries) && (uri_head == NULL))
    {
	ci->evict_buckets[bucket].num_entries = 0;
	dm2_evict_list_mutex_unlock(ci, bucket);
	NKN_ASSERT(0);
	return NULL;
    }

    dm2_evict_list_mutex_unlock(ci, bucket);

    /* Write lock the URI before handing off */
    dm2_uri_head_rwlock_wlock(uri_head, ci->ci_cttype, is_delete);
    dm2_uri_log_state(uri_head, DM2_URI_OP_EVICT, 0, 0, gettid(), nkn_cur_ts);

    return uri_head;
}   /* dm2_evict_get_next_uri */

static void
dm2_set_prov_hot_threshold(int ptype,
                           int hotval)
{
    switch (ptype) {
        case SAS2DiskMgr_provider:
            glob_dm2_prov_hot_threshold_5 = hotval;
            break;
        case SATADiskMgr_provider:
            glob_dm2_prov_hot_threshold_6 = hotval;
            break;
        case SolidStateMgr_provider:
            glob_dm2_prov_hot_threshold_1 = hotval;
            break;
    }
    return;
}       /* dm2_set_prov_hot_threshold */

static int
dm2_internal_evict_object(dm2_cache_info_t *evict_ci,
			  size_t	   blks_to_evict,
			  int		   max_evict_bkt,
                          dm2_cache_info_t **bad_ci)
{
    DM2_uri_t        *uri_head;
    dm2_cache_type_t *ct = evict_ci->ci_cttype;
    int32_t          uri_hotval;
    int              ret, bkt = 0, evict_done = 0;
    int              block_size = 0, macro_ret = 0;
    size_t	     del_blks = 0;


    block_size = dm2_get_blocksize(evict_ci);

    dm2_ci_dev_rwlock_rlock(evict_ci);

    for (bkt = 0; bkt <= max_evict_bkt; bkt++) {
	while(1) {
	    ret = 0;
	    if ((evict_ci == NULL) || (evict_ci->ci_disabling))
		break;

	    /* Eviction could be stopped when a new disk comes in while
	     * we are already evicting */
	    if (evict_ci->ci_stop_eviction) {
		evict_done = 1;
		DBG_DM2S("[cache_type=%s] Evicted %ld blocks from disk %s",
			ct->ct_name, del_blks, evict_ci->mgmt_name);
		DBG_DM2S("[cache_type=%s] Eviction on disk %s stopped",
			ct->ct_name, evict_ci->mgmt_name);
		break;
	    }

	    NKN_MUTEX_LOCKR(&evict_ci->ci_dm2_delete_mutex);

	    uri_head = dm2_evict_get_next_uri(evict_ci, bkt, 1);
	    /* Even if we did not delete all the objects in the bucket, we could
	     * have exhausted the list as the DELETE could actually delete
	     * multiple files per call.
	     */
	    if (uri_head == NULL) {
		NKN_MUTEX_UNLOCKR(&evict_ci->ci_dm2_delete_mutex);
		break;
	    }

	    if (uri_head->uri_bad_delete) {
		DBG_DM2S("[cache_type=%s] Evict rejected for bad URI: %s",
			  ct->ct_name, uri_head->uri_name);
		glob_dm2_int_evict_skip_bad_uri++;
		/* Remove this object from the evict list */
		dm2_evict_delete_uri(uri_head,
				     am_decode_hotness(&uri_head->uri_hotval));
		dm2_uri_head_rwlock_wunlock(uri_head, ct, DM2_NOLOCK_CI);
		NKN_MUTEX_UNLOCKR(&evict_ci->ci_dm2_delete_mutex);
		continue;
	    }

	    uri_hotval = am_decode_hotness(&uri_head->uri_hotval);
	    ct->ct_am_hot_evict_threshold = uri_hotval;
	    dm2_set_prov_hot_threshold(ct->ct_ptype, uri_hotval);

	    /* Calculate the blocks that will be free'd up, don't worry about
	     * block sharing here */
	    del_blks += (uri_head->uri_content_len / block_size);
	    if (uri_head->uri_content_len % block_size)
		del_blks += 1;

	    /* the uri_head that we get is wlocked. 'dm2_do_delete' will try
	     * to DELETE the URI at any cause and if an URI is not deleted,
	     * 'dm2_do_delete' will unlock it for us */
	    ret = dm2_do_delete(NULL, ct, uri_head, bad_ci);
	    if (ret < 0) {		// we cant get an ENOENT
		DBG_DM2E("[cache_type=%s] Eviction failed on disk %s(%d)",
			 ct->ct_name, evict_ci->mgmt_name, ret);
		/* Should I break here or continue on ?? */
		if (ret == -EIO) {
		    /* possibly a bad disk, exit the loop */
		    NKN_MUTEX_UNLOCKR(&evict_ci->ci_dm2_delete_mutex);
		    break;
		}
	    }

	    if (!ret) {
	       ct->ct_dm2_int_evict_del_cnt++;
	       glob_dm2_int_evict_cnt++;
	    }

	    /* Approximate calculation of the number of blocks to delete
	     * to reach the low water mark. Since the calculation is liberal
	     * we will not over evict and if we really evicted less, the next
	     * iteration will evict more to reach the lower water mark level */
	    if (del_blks >= blks_to_evict) {
		evict_done = 1;
		DBG_DM2S("[cache_type=%s] Evicted %ld blocks from disk %s",
			ct->ct_name, del_blks, evict_ci->mgmt_name);
		NKN_MUTEX_UNLOCKR(&evict_ci->ci_dm2_delete_mutex);
		break;
	    }

	    NKN_MUTEX_UNLOCKR(&evict_ci->ci_dm2_delete_mutex);
	}

	if (evict_done)
	    break;
    }
    if (evict_ci->ci_stop_eviction)
	evict_ci->ci_stop_eviction = 0;
    dm2_ci_dev_rwlock_runlock(evict_ci);

    return ret;
}   /* dm2_internal_evict_object */

static void
dm2_evict_decay_ci(dm2_cache_type_t *ct, dm2_cache_info_t *decay_ci)
{
    DM2_uri_t	     *uri_head;
    nkn_hot_t	     old_hotness, new_hotness;
    int		     bkt, count, decay_num = 0, macro_ret;
    uint32_t	     time_since_last_update = 0, system_hit_interval;

    /* Decay the objects from lowest to highest bucket only once
     * If we did the reverse, we would decay the same object at
     * every bucket causing multiple decays */
    for (bkt=0; bkt < DM2_MAX_EVICT_BUCKETS-1; bkt++) {
	count = decay_ci->evict_buckets[bkt].num_entries;
	while (count > 0) {
	    /* We need to hold the delete lock while we get an URI
	     * from evict list. This is because while we are waiting on
	     * on the uri wlock, someone could delete the object
	     * leaving a stale pointer with us */
	    NKN_MUTEX_LOCKR(&decay_ci->ci_dm2_delete_mutex);

	    /* Get an URI from the list to decay it */
	    uri_head = dm2_evict_get_next_uri(decay_ci, bkt, 0);

	    /* What could cause this? */
	    if (uri_head == NULL) {
		NKN_MUTEX_UNLOCKR(&decay_ci->ci_dm2_delete_mutex);
		break;
	    }

	    /* If the uri is bad, remove it from the list */
	    if (uri_head->uri_bad_delete) {
		DBG_DM2S("[cache_type=%s] Evict rejected for bad URI: %s",
			  ct->ct_name, uri_head->uri_name);
		glob_dm2_int_evict_skip_bad_uri++;
		dm2_evict_delete_uri(uri_head,
				     am_decode_hotness(&uri_head->uri_hotval));
		dm2_uri_head_rwlock_wunlock(uri_head, ct, DM2_NOLOCK_CI);
		NKN_MUTEX_UNLOCKR(&decay_ci->ci_dm2_delete_mutex);
		count--;
		continue;
	    }

	    old_hotness = uri_head->uri_hotval;

	    time_since_last_update =
		(nkn_cur_ts - am_decode_update_time(&old_hotness));
	    /* Call the AM api to get the decayed hotness */
	    system_hit_interval = am_get_hit_interval();
	    if (time_since_last_update > system_hit_interval) {
		new_hotness = am_update_hotness(&old_hotness, 0, 0, 0,
						uri_head->uri_name);
		decay_num++;
	    } else {
		new_hotness = old_hotness;
	    }

	    /* Remove this URI from the list and add to the new hotness bucket
	     * Even if the bucket remains the same after decay, this object
	     * needs to be removed to get to the next object in the list */
	    dm2_evict_delete_uri(uri_head, am_decode_hotness(&old_hotness));

	    dm2_evict_add_uri(uri_head, am_decode_hotness(&new_hotness));

	    /* Store the decayed hotness */
	    uri_head->uri_hotval = new_hotness;

	    if (glob_dm2_log_decay)
		dm2_cache_log_decay(uri_head, ct, old_hotness, new_hotness);

	    /* Unlock the uri_head as we are done */
	    dm2_uri_head_rwlock_wunlock(uri_head, ct, DM2_NOLOCK_CI);
	    NKN_MUTEX_UNLOCKR(&decay_ci->ci_dm2_delete_mutex);
	    count--;
	}
    }

    decay_ci->ci_dm2_last_decay_ts = nkn_cur_ts;

    DBG_DM2S("[cache_type=%s] Disk %s, decayed %d objects",
	     ct->ct_name, decay_ci->mgmt_name, decay_num);

}   /* dm2_evict_decay_ci */

static int
dm2_evict_is_evict_time(dm2_cache_type_t *ct)
{
    struct tm lt;
    localtime_r(&nkn_cur_ts, &lt);

    /* The value of time that is stored is actually the duration of time
     * that should have elapsed from midnight before eviction can kick in.
     * In other words it is the localtime of the system. Hence, we should be
     * validating the stored time against the localtime
     */
    if (ct->ct_evict_hour == -1 || ct->ct_evict_min == -1 ||
	!ct->ct_internal_time_low_water_mark)
	return 0;

    if (ct->ct_internal_evict_percent < ct->ct_internal_time_low_water_mark)
	return 0;

    if (lt.tm_hour == ct->ct_evict_hour && lt.tm_min == ct->ct_evict_min)
	return 1;
    else
	return 0;
}   /* dm2_evict_is_evict_time */

static int
dm2_evict_check_dict_full(void)
{

    glob_dm2_dict_int_evict_percent =
	((AO_load(&glob_dm2_alloc_total_bytes) * 100) / glob_dm2_mem_max_bytes);

    if (glob_dm2_dict_int_evict_in_progress) {
	/* If we breached HWM already, check that if we reduced enough memory
	 * to reach the low water mark level */
	if (AO_load(&glob_dm2_alloc_total_bytes) >=
	    ((glob_dm2_mem_max_bytes * glob_dm2_dict_int_evict_lwm) / 100))
	    return 1;
	else {
	    glob_dm2_dict_int_evict_in_progress = 0;
	    return 0;
	}
    }

    /* Check if we breached HWM on the dictionary */
    if (AO_load(&glob_dm2_alloc_total_bytes) >=
	((glob_dm2_mem_max_bytes * glob_dm2_dict_int_evict_hwm) / 100))
	return 1;

    return 0;
}   /* dm2_evict_check_dict_full */

static int
dm2_internal_evict(void)
{
    dm2_cache_info_t    *ci = NULL;
    GList		*ci_obj;
    dm2_cache_type_t	*ct = NULL;
    size_t              tot_avail, tot_free;
    int			blks_to_evict = 0, ct_bkt_blks, bkt_blks;
    int			bkt_blks_ratio, num_evict_disks;
    uint32_t            percent, lwm;
    int                 i, j, dict_full, evict_progress = 0;
    int			macro_ret, max_evict_bkt, its_evict_time = 0;

    for (i = 0; i < glob_dm2_num_cache_types; i++) {
	ct = &g_cache2_types[i];
	/* Evict only if pre-read is complete */
	if (!ct->ct_tier_preread_done) {
	    glob_dm2_int_evict_skip_preread++;
	    continue;
	}

	dm2_get_cache_type_resv_free_percent(ct, &tot_free, &tot_avail);
	if (tot_avail > 0)
	    percent = ((tot_avail - tot_free) * 100) / tot_avail;
	else
	    percent = 0;
	ct->ct_internal_evict_percent = percent;
	dict_full = dm2_evict_check_dict_full();

	its_evict_time = dm2_evict_is_evict_time(ct);
	/* If its timed_evict, take the lwm appropriately */
	if (its_evict_time || ct->ct_dm2_int_time_evict_in_progress)
	    lwm = ct->ct_internal_time_low_water_mark;
	else
	    lwm = ct->ct_internal_low_water_mark;

	if (!ct->ct_internal_evict_working) {
	    if (ct->ct_num_active_caches != 0) {
		ct->ct_num_evict_disks =
		    (ct->ct_num_active_caches*ct->ct_num_evict_disk_prcnt)/100;
		if (!ct->ct_num_evict_disks)
		    ct->ct_num_evict_disks++;
	    }

	    /* We should evict if the dictionary is full or the water mark
	     * levels have been reached */
	    if (dict_full || (percent >= ct->ct_internal_high_water_mark ||
			   its_evict_time || ct->ct_dm2_evict_more )) {
		if (dict_full) {
		    DBG_DM2S("[cache_type=%s] Dictionary Full: "
			      "Triggering Eviction", ct->ct_name);
		    glob_dm2_int_evict_dict_full++;
		    glob_dm2_dict_int_evict_in_progress = 1;
		    ct->ct_dm2_int_evict_dict_full_cnt++;
		    ct->ct_dm2_last_dict_full_int_evict_ts = nkn_cur_ts;
		    ct->ct_dm2_evict_reason =
				NKN_DEL_REASON_MEM_FULL_INT_EVICTION;
		} else if (its_evict_time) {
		    DBG_DM2S("[cache_type=%s] Timed Evict: (%d/%d) "
			      "Triggering Eviction", ct->ct_name, percent,
			      ct->ct_internal_time_low_water_mark);
		    glob_dm2_int_evict_time_cnt++;
		    ct->ct_dm2_int_evict_time_cnt++;
		    ct->ct_dm2_last_time_int_evict_ts = nkn_cur_ts;
		    ct->ct_dm2_int_time_evict_in_progress = 1;
		    ct->ct_dm2_evict_reason =
				NKN_DEL_REASON_TIMED_INT_EVICTION;
		} else {
		    DBG_DM2S("[cache_type=%s] Disk Full: (%d/%d/%d) "
			      "Triggering Eviction", ct->ct_name, percent,
			      ct->ct_internal_low_water_mark,
			      ct->ct_internal_high_water_mark);
		    glob_dm2_int_evict_disk_full++;
		    ct->ct_dm2_int_evict_disk_full_cnt++;
		    ct->ct_dm2_last_disk_full_int_evict_ts = nkn_cur_ts;
		    ct->ct_dm2_evict_reason =
				NKN_DEL_REASON_DISK_FULL_INT_EVICTION;
		}

		/* Decay only if we are above HWM or if evicting based on time.
		 * We might also be here since
		 * the previous iteration did not take us to the LWM
		 * (evict_more = 1), in which case do not decay again */
		if ((percent >= ct->ct_internal_high_water_mark &&
				    !ct->ct_dm2_evict_more) || its_evict_time)
		    ct->ct_dm2_decay_objects = 1;

		/* Find out the number of blocks to be evicted to reach LWM */
		if (dict_full) {
		    blks_to_evict = DM2_DICT_EVICT_NUM_BLOCKS;
		} else {
		    blks_to_evict = (percent - lwm)*tot_avail/100.0;
		}
		ct->ct_dm2_evict_blks = blks_to_evict;

		/* Decay the objects in the selected disk, this is usually quick
		 * and should be OK to hold the lock on the info list */
		ct->ct_dm2_decay_call_cnt++;
		dm2_ct_info_list_rwlock_rlock(ct);
		for (ci_obj = ct->ct_info_list; ci_obj; ci_obj = ci_obj->next) {
		    ci = (dm2_cache_info_t *)ci_obj->data;
		    if (ci->ci_disabling)
			continue;
		    ci->ci_evict_blks = 0;
		    if (ct->ct_dm2_decay_objects ||
			(nkn_cur_ts - ci->ci_dm2_last_decay_ts) >= (4*3600)){
			dm2_evict_decay_ci(ct, ci);
		    }
		}
		dm2_ct_info_list_rwlock_runlock(ct);

		/* Set up the evict inputs for the disk evict threads.
		 * #1: Hot objects should not be evicted when colder ones exist
		 * #2: Ensure that objects are evicted in all the disks
		 * #3: Do not over evict in any single disk
		 */
		num_evict_disks = 0;
		for (j=0; j<DM2_MAX_EVICT_BUCKETS; j++) {
		    ct_bkt_blks = AO_load(&ct->ct_dm2_bkt_evict_blks[j]);
		    if (!ct_bkt_blks)
			continue;
		    if (ct_bkt_blks >= blks_to_evict) {
			/* If the current bkt has more than reqd number of
			 * blocks split the eviction across all the disks
			 */
			dm2_ct_info_list_rwlock_rlock(ct);
			for (ci_obj = ct->ct_info_list; ci_obj;
					    ci_obj = ci_obj->next) {
			    ci = (dm2_cache_info_t *)ci_obj->data;
			    if (ci->ci_disabling)
				continue;
			    bkt_blks = ci->evict_buckets[j].num_blks;
			    bkt_blks_ratio =
				(bkt_blks/(1.0*ct_bkt_blks)) * blks_to_evict;
			    ci->ci_evict_blks += bkt_blks_ratio;
			    DBG_DM2S("Disk %s: Selected %d blocks from %d bkt",
				     ci->mgmt_name, bkt_blks_ratio, j);
			    if (bkt_blks)
				num_evict_disks++;
			    /* If we selected required number of disks, break
			     * out here only if part of the disks need to be
			     * evicted. We will evict one bucket at a time if
			     * a sunset of the disks are selected */
			    if (ct->ct_num_evict_disk_prcnt != 100 &&
				num_evict_disks == ct->ct_num_evict_disks)
				break;
			}
			dm2_ct_info_list_rwlock_runlock(ct);
			max_evict_bkt = j;
			/* We should have accounted for all the blocks to be
			 * evicted, so reset blks_to_evict here */
			blks_to_evict = 0;
			break;
		    } else {
			/* This bkt has lesser blks than reqd, wipe it out */
			dm2_ct_info_list_rwlock_rlock(ct);
			for (ci_obj = ct->ct_info_list; ci_obj;
					    ci_obj = ci_obj->next) {
			    ci = (dm2_cache_info_t *)ci_obj->data;
			    if (ci->ci_disabling)
				continue;
			    bkt_blks = ci->evict_buckets[j].num_blks;
			    ci->ci_evict_blks += bkt_blks;
			    blks_to_evict -= bkt_blks;
			    DBG_DM2S("Disk %s: Selected %d blocks from %d bkt",
				     ci->mgmt_name, bkt_blks, j);
			    if (bkt_blks)
				num_evict_disks++;
			    /* If we selected required number of disks, break
			     * out here only if part of the disks need to be
			     * evicted */
			    if (ct->ct_num_evict_disk_prcnt != 100 &&
				num_evict_disks == ct->ct_num_evict_disks) {
				blks_to_evict = 0;
				break;
			    }
			}
			dm2_ct_info_list_rwlock_runlock(ct);
			max_evict_bkt = j;
		    }

		    if (blks_to_evict <= 0) {
			max_evict_bkt = j;
			break;
		    }
		}

		/* If we are already evicting and do not have sufficient blocks
		 * to reach LWM, bail out or else we would be evicting newer
		 * objects before we hit the HWM again
		 */
		if ((blks_to_evict > 0) && ct->ct_dm2_evict_more) {
		    ct->ct_internal_evict_working = 0;
		    ct->ct_dm2_evict_more = 0;
		    if (ct->ct_dm2_int_time_evict_in_progress)
			ct->ct_dm2_int_time_evict_in_progress = 0;
		    glob_dm2_int_evict_stop_less_blks++;
		    continue;
		}
		/* Send signal to start eviction on all the disks */
		ct->ct_internal_evict_working = 1;
		ct->ct_dm2_ci_max_evict_bkt = max_evict_bkt;
		PTHREAD_COND_BROADCAST(&ct->ct_dm2_evict_cond);
	    }
	} else {
            DBG_DM2M3("[cache_type=%s] Percent: %d, Dictionary Full: %d",
                      ct->ct_name, percent, dict_full);

	    if (!dict_full && (percent <= lwm)) {
		ct->ct_internal_evict_working = 0;
		/* If we reached the time_lwm, reset the in progress flag */
		if (ct->ct_dm2_int_time_evict_in_progress)
		    ct->ct_dm2_int_time_evict_in_progress = 0;
		ct->ct_dm2_evict_more = 0;
	    } else {
		/* Ensure that the eviction is running */
		evict_progress = 0;
		dm2_ct_info_list_rwlock_rlock(ct);
		for (ci_obj = ct->ct_info_list; ci_obj; ci_obj = ci_obj->next) {
		    ci = (dm2_cache_info_t *)ci_obj->data;
		    if (ci->ci_disabling)
			continue;
		    /* If atleast one of the threads is not sleeping, some
		     * eviction is going on */
		    if (ci->ci_evict_thread_state == EVICT_IN_PROGRESS) {
			evict_progress = 1;
			break;
		    }
		}
		dm2_ct_info_list_rwlock_runlock(ct);
		if (!evict_progress) {
		    /* The disk evict threads are not working, start over */
		    ct->ct_internal_evict_working = 0;
		    ct->ct_dm2_decay_objects = 0;
		    ct->ct_dm2_evict_more = 1;
		}
	    }
	}
    }
    return 0;
}       /* dm2_internal_evict */

void*
dm2_evict_ci_fn(void *arg)
{
    char thread_name[64];
    dm2_cache_info_t *ci = (dm2_cache_info_t *)arg, *bad_ci;
    dm2_cache_type_t *ct = ci->ci_cttype;
    int		     macro_ret, ret;
    int blks_to_evict, max_bkts_to_evict;

    /* Detach the thread so that its resources get reclaimed on exit */
    ret = pthread_detach(pthread_self());
    assert(ret == 0);

    snprintf(thread_name, 40, "nvsd-dm2-%s-evict", ci->mgmt_name);
    prctl(PR_SET_NAME, thread_name, 0, 0, 0);

    while(1) {
	/* Ensure the 'stop' is reset once the thread goes to sleep
	 * 'stop_eviction' can be set only when the evict thread is active
	 */
	ci->ci_stop_eviction = 0;
	/* Wait until we are woken up */
	ci->ci_evict_thread_state = EVICT_WAIT;
	NKN_MUTEX_LOCKR(&ct->ct_dm2_evict_cond_mutex);
	PTHREAD_COND_WAIT(&ct->ct_dm2_evict_cond,
			  &ct->ct_dm2_evict_cond_mutex.lock);
	NKN_MUTEX_UNLOCKR(&ct->ct_dm2_evict_cond_mutex);

	/* Exit if the disk was disabled */
	if (ci->ci_disabling) {
	    DBG_DM2S("Disk %s is disabled. Evict thread exiting",
		     ci->mgmt_name);
	    break;
	}

	/* Could be a spurious event */
	if (!ct->ct_internal_evict_working)
	    continue;

	ci->ci_evict_thread_state = EVICT_IN_PROGRESS;

	blks_to_evict = ci->ci_evict_blks;
	max_bkts_to_evict = ct->ct_dm2_ci_max_evict_bkt;

	/* Nothing to evict for this disk */
	if (!blks_to_evict)
	    continue;

	DBG_DM2S("[cache_type=%s] Starting eviction on disk %s, blocks to "
		 "evict %d, max_evict_bkt %d", ct->ct_name, ci->mgmt_name,
		  blks_to_evict, max_bkts_to_evict);
	ret = dm2_internal_evict_object(ci, blks_to_evict,
					max_bkts_to_evict, &bad_ci);
	if (ret < 0) {
	    DBG_DM2E("[cache_type=%s] Internal Evict failed:%d",
		     ct->ct_name, ret);
	    if (bad_ci && ret == -EIO) {
		DBG_DM2S("Automatic 'Cache Disable' due to apparent drive "
			 "failure: name=%s serial_num=%s type=%s",
			 bad_ci->mgmt_name, bad_ci->serial_num,
			 bad_ci->ci_cttype->ct_name);
		dm2_mgmt_cache_disable(bad_ci);
	    }
	    break;
	}
    }
    return NULL;
}       /* dm2_evict_ci_fn */

static void*
dm2_evict_thread_fn (void* arg)
{
    struct stat sb;
    char thread_name[64];
    int dm2_evict_thread_time_sec = 10;

    (void)arg;
    /* Set the thread name */
    snprintf(thread_name, 40, "nvsd-dm2-glob-evict");
    prctl(PR_SET_NAME, thread_name, 0, 0, 0);

    while (!glob_cachemgr_init_done) {
	sleep(1);
    }
    /* Run the eviction loop */
    while(1) {
        sleep(dm2_evict_thread_time_sec);

	glob_dm2_log_decay = 0;
	if (stat("/config/nkn/log_decay", &sb) == 0) {
	    DBG_DM2W("Decay will be logged in cache.log");
	    glob_dm2_log_decay = 1;
	}

	/* Kick off eviction */
	dm2_internal_evict();
    }
    return (void *)0;
}       /* dm2_evict_thread_fn */

void
dm2_spawn_dm2_evict_thread(void)
{
    DBG_DM2W("Starting Global Evict thread");
    pthread_create(&dm2_evict_thread, NULL, dm2_evict_thread_fn, NULL);
}       /* dm2_spawn_tier_evict_thread */
