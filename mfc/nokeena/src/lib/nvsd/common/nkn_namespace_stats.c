/*
 * Filename:  nkn_namespace_stats.c
 *
 * (C) Copyright 2010 Juniper Networks, Inc.
 * All rights reserved.
 */

#include <atomic_ops.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>

#include "nkn_defs.h"
#include "nkn_debug.h"
#include "nkn_assert.h"
#include "nkn_hash.h"
#include "nkn_namespace.h"
#include "nkn_namespace_stats.h"
#include "nvsd_mgmt_namespace.h"
#include "nkn_diskmgr2_api.h"
#include "nkn_diskmgr_common_api.h"


/* Code in ns_stat_add_namespaces works when these 2 values are the same */
#define NS_NUM_STAT_ENTRIES (NKN_MAX_NAMESPACES)
#define NS_INIT_STAT_HASH_SIZE (NS_NUM_STAT_ENTRIES * 2)

static AO_t ns_stats[NS_NUM_STAT_ENTRIES][NS_STAT_MAX];
static ns_stat_entry_t *ns_slot_info[NS_NUM_STAT_ENTRIES];

static AO_t ns_token_gen[NS_NUM_STAT_ENTRIES];
static AO_t g_ns_index = 0;
static AO_t ns_num_namespaces = 0;

ns_stat_token_t NS_NULL_STAT_TOKEN = { .u.stat_token = 0 };

uint64_t glob_ns_stat_add_gen_mismatch_err,
    glob_ns_stat_add_type_einval_err,
    glob_ns_stat_add_too_many_err,
    glob_ns_stat_add_no_free_slot_err,
    glob_ns_stat_add_nomem_err,
    glob_ns_stat_set_gen_mismatch_err,
    glob_ns_stat_set_type_einval_err,
    glob_ns_stat_get_gen_mismatch_err,
    glob_ns_stat_get_type_einval_err,
    glob_ns_stat_get_lookup_err,
    glob_ns_stat_get_lookup_deleted_err,
    glob_ns_stat_free_gen_mismatch_err,
    glob_ns_stat_freedel_gen_mismatch_err,
    glob_ns_stat_already_deleted_err,
    glob_ns_stat_delete_not_found_err,
    glob_ns_stat_free_null_token_err;

uint64_t glob_ns_num_namespaces,
    glob_ns_stat_add_new_slot_cnt,
    glob_ns_stat_add_reuse_slot_cnt,
    glob_ns_stat_delete_cnt,
    glob_ns_stat_delete_on_free_cnt,
    glob_ns_stat_free_cnt,
    glob_ns_stat_freedel_cnt,
    glob_ns_stat_get_cnt,
    glob_ns_stat_to_be_deleted_cnt;

/*
 * Hash table for namespace stats
 *
 * Use the "namespace_uuid" string as a key to find a stat token
 */
static NHashTable *s_ns_stat_hash;
pthread_rwlock_t   s_ns_stat_hash_rwlock;
static AO_t	   s_ns_stat_refcnt[NS_NUM_STAT_ENTRIES];

#define NS_HASH_RWLOCK_INIT() { \
	int macro_ret; \
	macro_ret = pthread_rwlock_init(&s_ns_stat_hash_rwlock, 0); \
	assert(macro_ret == 0); \
    }
#define NS_HASH_WLOCK(lock_flag) { \
	int macro_ret; \
	if (lock_flag) { \
	    macro_ret = pthread_rwlock_wrlock(&s_ns_stat_hash_rwlock);	\
	    assert(macro_ret == 0);					\
	} \
    }
#define NS_HASH_RLOCK() { \
	int macro_ret; \
	macro_ret = pthread_rwlock_rdlock(&s_ns_stat_hash_rwlock); \
	assert(macro_ret == 0); \
    }
#define NS_HASH_WUNLOCK(lock_flag) { \
	int macro_ret; \
	if (lock_flag) { \
	    macro_ret = pthread_rwlock_unlock(&s_ns_stat_hash_rwlock);	\
	    assert(macro_ret == 0);					\
	} \
    }
#define NS_HASH_RUNLOCK() { \
	int macro_ret; \
	macro_ret = pthread_rwlock_unlock(&s_ns_stat_hash_rwlock); \
	assert(macro_ret == 0); \
    }


static void
ns_stat_add_counters(const char *ns_uuid,
		     int	ns_index)
{
    char mon_diskname[60];
    char mon_ns_uuid[40];
    int  i;

    assert(39 > NKN_MAX_UID_LENGTH + 3 + 1);
    /*
     * For now, we just assume NKN_DM_MAX_CACHES but this could be tied to
     * disk operations.  Since these stats are not exported to users today,
     * it isn't a big problem to export too many disk stats.
     */
    for (i=0; i<NKN_DM_MAX_CACHES; i++) {
	snprintf(mon_ns_uuid, 40, "ns.%s", ns_uuid);
	snprintf(mon_diskname, 59, "cache.disk.%s%d.bytes_used",
		 NKN_DM_DISK_CACHE_PREFIX, i+1);
	nkn_mon_add(mon_diskname, mon_ns_uuid,
		    &ns_stats[ns_index][NS_USED_SPACE_D1+i],
		    sizeof(ns_stats[ns_index][NS_USED_SPACE_D1+i]));
	snprintf(mon_diskname, 59,
		 "cache.disk.%s%d.cache_pin.bytes_used",
		 NKN_DM_DISK_CACHE_PREFIX, i+1);
	nkn_mon_add(mon_diskname, mon_ns_uuid,
		    &ns_stats[ns_index][NS_CACHE_PIN_SPACE_D1+i],
		    sizeof(ns_stats[ns_index][NS_CACHE_PIN_SPACE_D1+i]));
	snprintf(mon_diskname, 59, "cache.disk.%s%d.total_objects",
		 NKN_DM_DISK_CACHE_PREFIX, i+1);
	nkn_mon_add(mon_diskname, mon_ns_uuid,
		    &ns_stats[ns_index][NS_TOTAL_OBJECTS_D1+i],
		    sizeof(ns_stats[ns_index][NS_TOTAL_OBJECTS_D1+i]));
    }

    /* SSD Ingest/Delete counts */
    strcpy(mon_diskname, "cache.tier.SSD.ingest_objects");
    nkn_mon_add(mon_diskname, mon_ns_uuid,
		&ns_stats[ns_index][NS_INGEST_OBJECTS_SSD],
		sizeof(ns_stats[ns_index][NS_INGEST_OBJECTS_SSD]));

    strcpy(mon_diskname, "cache.tier.SSD.delete_objects");
    nkn_mon_add(mon_diskname, mon_ns_uuid,
		&ns_stats[ns_index][NS_DELETE_OBJECTS_SSD],
		sizeof(ns_stats[ns_index][NS_DELETE_OBJECTS_SSD]));

    /* SAS Ingest/Delete counts */
    strcpy(mon_diskname, "cache.tier.SAS.ingest_objects");
    nkn_mon_add(mon_diskname, mon_ns_uuid,
		&ns_stats[ns_index][NS_INGEST_OBJECTS_SAS],
		sizeof(ns_stats[ns_index][NS_INGEST_OBJECTS_SAS]));

    strcpy(mon_diskname, "cache.tier.SAS.delete_objects");
    nkn_mon_add(mon_diskname, mon_ns_uuid,
		&ns_stats[ns_index][NS_DELETE_OBJECTS_SAS],
		sizeof(ns_stats[ns_index][NS_DELETE_OBJECTS_SAS]));

    /* SATA ns_uuid usage */
    strcpy(mon_diskname, "cache.tier.SATA.max_disk_usage");
    nkn_mon_add(mon_diskname, mon_ns_uuid,
		&ns_stats[ns_index][NS_SATA_MAX_USAGE],
		sizeof(ns_stats[ns_index][NS_SATA_MAX_USAGE]));

    strcpy(mon_diskname, "cache.tier.SATA.resv_disk_usage");
    nkn_mon_add(mon_diskname, mon_ns_uuid,
		&ns_stats[ns_index][NS_SATA_RESV_USAGE],
		sizeof(ns_stats[ns_index][NS_SATA_RESV_USAGE]));

    strcpy(mon_diskname, "cache.tier.SATA.actual_disk_usage");
    nkn_mon_add(mon_diskname, mon_ns_uuid,
		&ns_stats[ns_index][NS_SATA_ACT_USAGE],
		sizeof(ns_stats[ns_index][NS_SATA_ACT_USAGE]));

    /* SAS ns_uuid usage */
    strcpy(mon_diskname, "cache.tier.SAS.max_disk_usage");
    nkn_mon_add(mon_diskname, mon_ns_uuid,
		&ns_stats[ns_index][NS_SAS_MAX_USAGE],
		sizeof(ns_stats[ns_index][NS_SAS_MAX_USAGE]));

    strcpy(mon_diskname, "cache.tier.SAS.resv_disk_usage");
    nkn_mon_add(mon_diskname, mon_ns_uuid,
		&ns_stats[ns_index][NS_SAS_RESV_USAGE],
		sizeof(ns_stats[ns_index][NS_SAS_RESV_USAGE]));

    strcpy(mon_diskname, "cache.tier.SAS.actual_disk_usage");
    nkn_mon_add(mon_diskname, mon_ns_uuid,
		&ns_stats[ns_index][NS_SAS_ACT_USAGE],
		sizeof(ns_stats[ns_index][NS_SAS_ACT_USAGE]));

    /* SSD ns_uuid usage */
    strcpy(mon_diskname, "cache.tier.SSD.max_disk_usage");
    nkn_mon_add(mon_diskname, mon_ns_uuid,
		&ns_stats[ns_index][NS_SSD_MAX_USAGE],
		sizeof(ns_stats[ns_index][NS_SSD_MAX_USAGE]));

    strcpy(mon_diskname, "cache.tier.SSD.resv_disk_usage");
    nkn_mon_add(mon_diskname, mon_ns_uuid,
		&ns_stats[ns_index][NS_SSD_RESV_USAGE],
		sizeof(ns_stats[ns_index][NS_SSD_RESV_USAGE]));

    strcpy(mon_diskname, "cache.tier.SSD.act_disk_usage");
    nkn_mon_add(mon_diskname, mon_ns_uuid,
		&ns_stats[ns_index][NS_SSD_ACT_USAGE],
		sizeof(ns_stats[ns_index][NS_SSD_ACT_USAGE]));

    /* SATA Ingest/Delete counts */
    strcpy(mon_diskname, "cache.tier.SATA.ingest_objects");
    nkn_mon_add(mon_diskname, mon_ns_uuid,
		&ns_stats[ns_index][NS_INGEST_OBJECTS_SATA],
		sizeof(ns_stats[ns_index][NS_INGEST_OBJECTS_SATA]));

    strcpy(mon_diskname, "cache.tier.SATA.delete_objects");
    nkn_mon_add(mon_diskname, mon_ns_uuid,
		&ns_stats[ns_index][NS_DELETE_OBJECTS_SATA],
		sizeof(ns_stats[ns_index][NS_DELETE_OBJECTS_SATA]));

    /* SSD ns_uuid used bytes */
    strcpy(mon_diskname, "cache.tier.SSD.bytes_used");
    nkn_mon_add(mon_diskname, mon_ns_uuid,
		&ns_stats[ns_index][NS_USED_SPACE_SSD],
		sizeof(ns_stats[ns_index][NS_USED_SPACE_SSD]));

    /* SAS ns_uuid used bytes */
    strcpy(mon_diskname, "cache.tier.SAS.bytes_used");
    nkn_mon_add(mon_diskname, mon_ns_uuid,
		&ns_stats[ns_index][NS_USED_SPACE_SAS],
		sizeof(ns_stats[ns_index][NS_USED_SPACE_SAS]));

    /* SATA ns_uuid used bytes */
    strcpy(mon_diskname, "cache.tier.SATA.bytes_used");
    nkn_mon_add(mon_diskname, mon_ns_uuid,
		&ns_stats[ns_index][NS_USED_SPACE_SATA],
		sizeof(ns_stats[ns_index][NS_USED_SPACE_SATA]));

    /* Cache pinning ns_uuid resv bytes */
    strcpy(mon_diskname, "cache.tier.lowest.cache_pin.bytes_resv");
    nkn_mon_add(mon_diskname, mon_ns_uuid,
		&ns_stats[ns_index][NS_CACHE_PIN_SPACE_RESV_TOTAL],
		sizeof(ns_stats[ns_index][NS_CACHE_PIN_SPACE_RESV_TOTAL]));

    /* Cache pinning ns_uuid used bytes */
    strcpy(mon_diskname, "cache.tier.lowest.cache_pin.bytes_used");
    nkn_mon_add(mon_diskname, mon_ns_uuid,
		&ns_stats[ns_index][NS_CACHE_PIN_SPACE_USED_TOTAL],
		sizeof(ns_stats[ns_index][NS_CACHE_PIN_SPACE_USED_TOTAL]));

    /* Cache pinning ns_uuid used bytes */
    strcpy(mon_diskname, "cache.tier.lowest.cache_pin.obj_count");
    nkn_mon_add(mon_diskname, mon_ns_uuid,
		&ns_stats[ns_index][NS_CACHE_PIN_OBJECT_COUNT],
		sizeof(ns_stats[ns_index][NS_CACHE_PIN_OBJECT_COUNT]));


    /* Cache pinning ns_uuid enabled */
    strcpy(mon_diskname, "cache.tier.lowest.cache_pin.enabled");
    nkn_mon_add(mon_diskname, mon_ns_uuid,
		&ns_stats[ns_index][NS_CACHE_PIN_SPACE_ENABLED],
		sizeof(ns_stats[ns_index][NS_CACHE_PIN_SPACE_ENABLED]));

    /* Cache pinning ns_uuid object limit */
    strcpy(mon_diskname, "cache.tier.lowest.cache_pin.obj_limit");
    nkn_mon_add(mon_diskname, mon_ns_uuid,
		&ns_stats[ns_index][NS_CACHE_PIN_SPACE_OBJ_LIMIT],
		sizeof(ns_stats[ns_index][NS_CACHE_PIN_SPACE_OBJ_LIMIT]));
}	/* ns_stat_add_counters */


static void
ns_stat_delete_counters(const char *ns_uuid)
{
    char mon_diskname[60];
    char mon_ns_uuid[40];
    int  i;

    for (i=0; i<NKN_DM_MAX_CACHES; i++) {
	snprintf(mon_ns_uuid, 40, "ns.%s", ns_uuid);
	snprintf(mon_diskname, 59, "cache.disk.%s%d.bytes_used",
		 NKN_DM_DISK_CACHE_PREFIX, i+1);
	nkn_mon_delete(mon_diskname, mon_ns_uuid);
	snprintf(mon_diskname, 59,
		 "cache.disk.%s%d.cache_pin.bytes_used",
		 NKN_DM_DISK_CACHE_PREFIX, i+1);
	nkn_mon_delete(mon_diskname, mon_ns_uuid);
	snprintf(mon_diskname, 59, "cache.disk.%s%d.total_objects",
		 NKN_DM_DISK_CACHE_PREFIX, i+1);
	nkn_mon_delete(mon_diskname, mon_ns_uuid);
    }

    /* SSD Ingest/Delete counts */
    strcpy(mon_diskname, "cache.tier.SSD.ingest_objects");
    nkn_mon_delete(mon_diskname, mon_ns_uuid);

    strcpy(mon_diskname, "cache.tier.SSD.delete_objects");
    nkn_mon_delete(mon_diskname, mon_ns_uuid);

    /* SAS Ingest/Delete counts */
    strcpy(mon_diskname, "cache.tier.SAS.ingest_objects");
    nkn_mon_delete(mon_diskname, mon_ns_uuid);

    strcpy(mon_diskname, "cache.tier.SAS.delete_objects");
    nkn_mon_delete(mon_diskname, mon_ns_uuid);

    /* SATA ns_uuid usage */
    strcpy(mon_diskname, "cache.tier.SATA.max_disk_usage");
    nkn_mon_delete(mon_diskname, mon_ns_uuid);

    strcpy(mon_diskname, "cache.tier.SATA.resv_disk_usage");
    nkn_mon_delete(mon_diskname, mon_ns_uuid);

    strcpy(mon_diskname, "cache.tier.SATA.actual_disk_usage");
    nkn_mon_delete(mon_diskname, mon_ns_uuid);

    /* SAS ns_uuid usage */
    strcpy(mon_diskname, "cache.tier.SAS.max_disk_usage");
    nkn_mon_delete(mon_diskname, mon_ns_uuid);

    strcpy(mon_diskname, "cache.tier.SAS.resv_disk_usage");
    nkn_mon_delete(mon_diskname, mon_ns_uuid);

    strcpy(mon_diskname, "cache.tier.SAS.actual_disk_usage");
    nkn_mon_delete(mon_diskname, mon_ns_uuid);

    /* SSD ns_uuid usage */
    strcpy(mon_diskname, "cache.tier.SSD.max_disk_usage");
    nkn_mon_delete(mon_diskname, mon_ns_uuid);

    strcpy(mon_diskname, "cache.tier.SSD.resv_disk_usage");
    nkn_mon_delete(mon_diskname, mon_ns_uuid);

    strcpy(mon_diskname, "cache.tier.SSD.act_disk_usage");
    nkn_mon_delete(mon_diskname, mon_ns_uuid);

    /* SATA Ingest/Delete counts */
    strcpy(mon_diskname, "cache.tier.SATA.ingest_objects");
    nkn_mon_delete(mon_diskname, mon_ns_uuid);

    strcpy(mon_diskname, "cache.tier.SATA.delete_objects");
    nkn_mon_delete(mon_diskname, mon_ns_uuid);

    /* SSD ns_uuid used bytes */
    strcpy(mon_diskname, "cache.tier.SSD.bytes_used");
    nkn_mon_delete(mon_diskname, mon_ns_uuid);

    /* SAS ns_uuid used bytes */
    strcpy(mon_diskname, "cache.tier.SAS.bytes_used");
    nkn_mon_delete(mon_diskname, mon_ns_uuid);

    /* SATA ns_uuid used bytes */
    strcpy(mon_diskname, "cache.tier.SATA.bytes_used");
    nkn_mon_delete(mon_diskname, mon_ns_uuid);

    /* Cache pinning ns_uuid resv bytes */
    strcpy(mon_diskname, "cache.tier.lowest.cache_pin.bytes_resv");
    nkn_mon_delete(mon_diskname, mon_ns_uuid);

    /* Cache pinning ns_uuid used bytes */
    strcpy(mon_diskname, "cache.tier.lowest.cache_pin.bytes_used");
    nkn_mon_delete(mon_diskname, mon_ns_uuid);

    /* Cache pinning ns_uuid used bytes */
    strcpy(mon_diskname, "cache.tier.lowest.cache_pin.obj_count");
    nkn_mon_delete(mon_diskname, mon_ns_uuid);

    /* Cache pinning ns_uuid enabled */
    strcpy(mon_diskname, "cache.tier.lowest.cache_pin.enabled");
    nkn_mon_delete(mon_diskname, mon_ns_uuid);

    /* Cache pinning ns_uuid object limit */
    strcpy(mon_diskname, "cache.tier.lowest.cache_pin.obj_limit");
    nkn_mon_delete(mon_diskname, mon_ns_uuid);
}	/* ns_stat_delete_counters */


/*
 * The call of this routine may need to move earlier, outside of DM2
 * initialization.
 */
int
ns_stat_get_initial_namespaces(void)
{
    namespace_node_data_t *tm_ns_info;
    ns_tier_entry_t	  ns_ssd, ns_sas, ns_sata;
    uint64_t		  cache_pin_max_obj_bytes;
    uint64_t		  cache_pin_resv_bytes;
    int			  nth, cache_pin_enabled, state, ret = 0;

    /* ns_token_gen will get initialized to zero */

    /* ns_token_gen will be zeroed because it is heap */

    NS_HASH_RWLOCK_INIT();
    s_ns_stat_hash = nhash_table_new(n_str_hash,		/* hashfunc */
				     n_str_equal,		/* keyequal */
				     NS_INIT_STAT_HASH_SIZE,	/* init_size */
				     mod_ns_stat_hash		/* type */);
    if (s_ns_stat_hash == NULL) {
	DBG_LOG(SEVERE, MOD_NAMESPACE,
		"Unable to create namespace stat hash table");
	return ENOMEM;
    }

    NS_HASH_WLOCK(1);
    nvsd_mgmt_namespace_lock();

    for (nth = 0; ; nth++) {
	tm_ns_info = nvsd_mgmt_get_nth_namespace(nth);
	if (!tm_ns_info)
	    break;

	NKN_ASSERT((tm_ns_info->active && tm_ns_info->deleted) == 0);
	if (tm_ns_info->active)
	    state = NS_STATE_ACTIVE;
	else if (tm_ns_info->deleted)
	    state = NS_STATE_DELETED;
	else
	    state = NS_STATE_INACTIVE;

	/* Get the cache pinning configs */
	nvsd_mgmt_get_cache_pin_config(tm_ns_info,
				       &cache_pin_max_obj_bytes,
				       &cache_pin_resv_bytes,
				       &cache_pin_enabled);

	/* Get the tier specific start values for the namespace */
	nvsd_mgmt_ns_ssd_config(tm_ns_info,
				&ns_ssd.read_size,
				&ns_ssd.block_free_threshold,
				&ns_ssd.group_read,
				&ns_ssd.max_disk_usage);

	nvsd_mgmt_ns_sas_config(tm_ns_info,
				&ns_sas.read_size,
				&ns_sas.block_free_threshold,
				&ns_sas.group_read,
				&ns_sas.max_disk_usage);

	nvsd_mgmt_ns_sata_config(tm_ns_info,
				&ns_sata.read_size,
				&ns_sata.block_free_threshold,
				&ns_sata.group_read,
				&ns_sata.max_disk_usage);

	/* validate the read_size to ensure that the input is right,
	 * for the others, since 0 is a valid input, can't do much validation */
	if (!ns_sata.read_size || !ns_sas.read_size || !ns_ssd.read_size) {
	    DBG_LOG(SEVERE, MOD_NAMESPACE,
		    "Unexpected values for Read Size, SSD:%d, SAS:%d, SATA:%d",
		    ns_ssd.read_size, ns_sas.read_size, ns_sata.read_size);
	    assert(0);
	}

	/*
	 * ns_uuid => -1: leave room for end-of-string; +1 to skip leading slash
	 */
	ret = ns_stat_add_namespace(nth, state, tm_ns_info->uid+1,
				    cache_pin_max_obj_bytes,
				    cache_pin_resv_bytes, cache_pin_enabled,
				    &ns_ssd, &ns_sas, &ns_sata,
				    0 /* lock_flag */);
	if (ret != 0)
	    goto unlock;
    }

 unlock:
    nvsd_mgmt_namespace_unlock();
    NS_HASH_WUNLOCK(1);
    return ret;
}	/* ns_stat_get_initial_namespaces */



int
ns_stat_add_namespace(const int nth,
		      const int state,
		      const char *ns_uuid,
		      const uint64_t cache_pin_max_obj_bytes,
		      const uint64_t cache_pin_resv_bytes,
		      const int cache_pin_enabled,
		      const ns_tier_entry_t *ssd,
		      const ns_tier_entry_t *sas,
		      const ns_tier_entry_t *sata,
		      const int lock_flag)
{
    ns_stat_entry_t	*ns;
    int64_t		old_index, orig_old_index = -1;
    uint32_t		keyhash;
    const AO_t		tmp_one = 1, tmp_zero = 0;
    int			not_found = 1;
    int			num_entries = 0;

    keyhash = n_str_hash(ns_uuid);

    NS_HASH_WLOCK(lock_flag);

    ns = nhash_lookup(s_ns_stat_hash, keyhash, (void *)ns_uuid /*key*/);
    if (ns) {
	/* This is the case where cache-inherit is done from multiple namespaces
	 * to a single deleted namespace or for an active namespace.
	 */
	ns->num_dup_ns++;
	NS_HASH_WUNLOCK(lock_flag);
	DBG_LOG(SEVERE, MOD_NAMESPACE, "Stat add called for existing"
		" Namespace (%s)", ns_uuid);
	return 0;
    }

    while (not_found) {
	/* By putting this first, we can atomically hand out indices */
	old_index = (int64_t)AO_fetch_and_add1(&g_ns_index);
	if (old_index == NS_NUM_STAT_ENTRIES) {	// size of array
	    AO_store(&g_ns_index, tmp_zero);
	    continue;
	}
	if (num_entries >= NKN_MAX_NAMESPACES) {// # of supported namespaces
	    NS_HASH_WUNLOCK(lock_flag);	// Ran out of valid namespace slots
	    glob_ns_stat_add_too_many_err++;
	    return ENOMEM;
	}
	if (orig_old_index == -1)
	    orig_old_index = old_index;
	else if (orig_old_index == old_index) {
	    /* No free slots */
	    NS_HASH_WUNLOCK(lock_flag);
	    NKN_ASSERT(0);	// should never happen
	    glob_ns_stat_add_no_free_slot_err++;
	    return ENOMEM;
	}
	if (ns_slot_info[old_index] == NULL) {
	    ns = nkn_calloc_type(1, sizeof(ns_stat_entry_t),
				 mod_ns_stat_entry_t);
	    if (ns == NULL) {
		NS_HASH_WUNLOCK(lock_flag);
		glob_ns_stat_add_nomem_err++;
		return ENOMEM;
	    }
	    ns_slot_info[old_index] = ns;
	    AO_fetch_and_add1(&ns_num_namespaces);
	    glob_ns_stat_add_new_slot_cnt++;
	    break;
	}
	if (ns_slot_info[old_index]->state == NS_STATE_DELETED) {
	    ns = ns_slot_info[old_index];
	    AO_fetch_and_add1(&ns_num_namespaces);
	    glob_ns_stat_add_reuse_slot_cnt++;
	    break;
	}
	num_entries++;
    }
    NS_HASH_WUNLOCK(lock_flag);

    DBG_LOG(MSG1, MOD_NAMESPACE, "Namespace (%s) stat added (index %lu) "
	    "(num_ns %lu) (state %d)", ns_uuid, old_index, ns_num_namespaces,
	    state);
    ns->index = old_index;
    ns->token_curr_gen = 1;
    ns->rp_index = nth;	// MGMT promises that value never changes

    ns->state = state;

    strncpy(ns->ns_key, ns_uuid, NKN_MAX_UID_LENGTH-1);
    ns->ns_key[NKN_MAX_UID_LENGTH] = '\0';

    /* If ns_key stored is truncated, it would cause
     * issue during stat_delete. Added assert at the source of the issue.
     */
    if (strlen(ns->ns_key) != strlen(ns_uuid)) {
	DBG_LOG(SEVERE, MOD_NAMESPACE,
		    "Namespace uid is longer than MAX_UID_LENGTH: %s",
		    ns_uuid);
	NKN_ASSERT(0);
    }

    ns->cache_pin_max_obj_bytes = cache_pin_max_obj_bytes;
    ns->cache_pin_resv_bytes = cache_pin_resv_bytes;
    ns->cache_pin_enabled = cache_pin_enabled;

    ns->ssd.read_size = ssd->read_size;
    ns->ssd.block_free_threshold = ssd->block_free_threshold;
    ns->ssd.group_read = ssd->group_read;
    ns->ssd.max_disk_usage = ssd->max_disk_usage * 1024 * 1024;

    ns->sas.read_size = sas->read_size;
    ns->sas.block_free_threshold = sas->block_free_threshold;
    ns->sas.group_read = sas->group_read;
    ns->sas.max_disk_usage = sas->max_disk_usage * 1024 *1024;

    ns->sata.read_size = sata->read_size;
    ns->sata.block_free_threshold = sata->block_free_threshold;
    ns->sata.group_read = sata->group_read;
    ns->sata.max_disk_usage = sata->max_disk_usage * 1024 *1024;

    /* insert into hash table with key "namespace:uuid" */
    NS_HASH_WLOCK(lock_flag);
    nhash_insert(s_ns_stat_hash, keyhash, ns->ns_key, ns /* value */);
    NS_HASH_WUNLOCK(lock_flag);

    AO_store(&ns_token_gen[ns->index], tmp_one);
    AO_store(&ns_stats[ns->index][NS_CACHE_PIN_SPACE_ENABLED],
	     ns->cache_pin_enabled);
    AO_store(&ns_stats[ns->index][NS_CACHE_PIN_SPACE_RESV_TOTAL],
	     ns->cache_pin_resv_bytes);
    AO_store(&ns_stats[ns->index][NS_CACHE_PIN_SPACE_OBJ_LIMIT],
	     ns->cache_pin_max_obj_bytes);

    AO_store(&ns_stats[ns->index][NS_SSD_MAX_USAGE],
	     ns->ssd.max_disk_usage);
    AO_store(&ns_stats[ns->index][NS_SAS_MAX_USAGE],
	     ns->sas.max_disk_usage);
    AO_store(&ns_stats[ns->index][NS_SATA_MAX_USAGE],
	     ns->sata.max_disk_usage);

    ns_stat_add_counters(ns->ns_key, ns->index);
    glob_ns_num_namespaces = AO_load(&ns_num_namespaces);
    return 0;
}	/* ns_stat_add_namespace */


int
ns_stat_del_namespace(const char *ns_uuid)
{
    ns_stat_entry_t *ns;
    int64_t	    keyhash;
    int		    err = 0;

    /* We might get a delete call before the hash table is created and filled */
    if (!s_ns_stat_hash)
	return err;

    keyhash = n_str_hash(ns_uuid);

    NS_HASH_WLOCK(1);
    ns = nhash_lookup(s_ns_stat_hash, keyhash, (void *)ns_uuid /*key*/);
    if (ns == NULL || IS_NS_STATE_DELETING(ns)) {
	if (ns == NULL) {
	    DBG_LOG(SEVERE, MOD_NAMESPACE,
		    "Namespace not found: %s", ns_uuid);
	    NKN_ASSERT(0);
	    glob_ns_stat_delete_not_found_err++;
	} else {
	    /* Duplicate delete request */
	    DBG_LOG(SEVERE, MOD_NAMESPACE,
		    "Namespace already deleted: %s", ns_uuid);
	    NKN_ASSERT(0);
	    glob_ns_stat_already_deleted_err++;
	}
	err = ENOENT;
	goto end;
    }
    AO_fetch_and_add1(&ns_token_gen[ns->index]);	// bump generation #

    if (ns->num_dup_ns != 0) {
	/* Duplicate Namespace referring to the stat structure present.
	 * Dont delete the stat structure or mark for deletion.
	 */
	/* Ref down the count which was incremented during the
	 * add_namespace call */
	ns->num_dup_ns--;
	err = 0;
	goto end;
    }

    /* If the refcnt is 0, which means that the stat is no longer used for
     * duplicate inherited namespaces, we can remove it from the hash table
     */

    if (AO_load(&s_ns_stat_refcnt[ns->index]) == 0) {
	if (nhash_remove(s_ns_stat_hash, keyhash, (void *)ns_uuid) == 0) {
	    /* Object not found */
	    DBG_LOG(SEVERE, MOD_NAMESPACE,
		    "Unable to remove namespace stat hash table object: %s",
		    ns_uuid);
	    assert(0);
	}
	ns_stat_delete_counters(ns->ns_key);
	ns->state = NS_STATE_DELETED;
	glob_ns_stat_delete_cnt++;
    } else {
	/* Delete on last free */
	ns->state = NS_STATE_TO_BE_DELETED;
	glob_ns_stat_to_be_deleted_cnt++;
    }
    AO_fetch_and_sub1(&ns_num_namespaces);
 end:
    NS_HASH_WUNLOCK(1);
    glob_ns_num_namespaces = AO_load(&ns_num_namespaces);
    return err;
}	/* ns_stat_del_namespace */


int
ns_stat_get_tier_group_read_state(const char		*ns_uuid,
				  nkn_provider_type_t	ptype,
				  int			*enabled)
{
    ns_stat_entry_t *ns;
    int64_t keyhash;
    int err = 0;

    keyhash = n_str_hash(ns_uuid);

    NS_HASH_RLOCK();
    ns = nhash_lookup(s_ns_stat_hash, keyhash, (void *)ns_uuid /*key*/);
    if (ns == NULL || ns->state == NS_STATE_DELETED) {
	err = ENOENT;
	goto end;
    }
    switch (ptype) {
	case SATADiskMgr_provider:
	    *enabled = ns->sata.group_read;
	    break;
	case SAS2DiskMgr_provider:
	    *enabled = ns->sas.group_read;
	    break;
	case SolidStateMgr_provider:
	    *enabled = ns->ssd.group_read;
	    break;
	default:
	    err = EINVAL;
    }
 end:
    NS_HASH_RUNLOCK();
    return err;
}	/* ns_stat_get_tier_group_read_state */


int
ns_stat_set_tier_group_read_state(const char		*ns_uuid,
                                  nkn_provider_type_t	ptype,
                                  int			enabled)
{
    ns_stat_entry_t *ns;
    int64_t keyhash;
    int err = 0;

    keyhash = n_str_hash(ns_uuid);

    NS_HASH_RLOCK();
    ns = nhash_lookup(s_ns_stat_hash, keyhash, (void *)ns_uuid /*key*/);
    if (ns == NULL || ns->state == NS_STATE_DELETED) {
	err = ENOENT;
	goto end;
    }
    switch (ptype) {
        case SATADiskMgr_provider:
            ns->sata.group_read = enabled;
            break;
        case SAS2DiskMgr_provider:
            ns->sas.group_read = enabled;
            break;
        case SolidStateMgr_provider:
            ns->ssd.group_read = enabled;
            break;
	default:
	    err = EINVAL;
    }
 end:
    NS_HASH_RUNLOCK();
    return err;
}	/* ns_stat_set_tier_group_read_state */


int
ns_stat_get_tier_block_free_threshold(const char	  *ns_uuid,
				      nkn_provider_type_t ptype,
				      uint32_t		  *prcnt)
{
    ns_stat_entry_t *ns;
    int64_t keyhash;
    int err = 0;

    keyhash = n_str_hash(ns_uuid);

    NS_HASH_RLOCK();
    ns = nhash_lookup(s_ns_stat_hash, keyhash, (void *)ns_uuid /*key*/);
    if (ns == NULL || ns->state == NS_STATE_DELETED) {
	err = ENOENT;
	goto end;
    }
    switch (ptype) {
	case SATADiskMgr_provider:
	    *prcnt = ns->sata.block_free_threshold;
	    break;
	case SAS2DiskMgr_provider:
	    *prcnt = ns->sas.block_free_threshold;
	    break;
	case SolidStateMgr_provider:
	    *prcnt = ns->ssd.block_free_threshold;
	    break;
	default:
	    err = EINVAL;
    }
 end:
    NS_HASH_RUNLOCK();
    return err;
}	/* ns_stat_get_tier_block_free_threshold */


int
ns_stat_set_tier_block_free_threshold(const char	  *ns_uuid,
				      nkn_provider_type_t ptype,
				      uint32_t		  prcnt)
{
    ns_stat_entry_t *ns;
    int64_t keyhash;
    int err = 0;

    keyhash = n_str_hash(ns_uuid);

    NS_HASH_RLOCK();
    ns = nhash_lookup(s_ns_stat_hash, keyhash, (void *)ns_uuid /*key*/);
    if (ns == NULL || ns->state == NS_STATE_DELETED) {
	err = ENOENT;
	goto end;
    }
    switch (ptype) {
	case SATADiskMgr_provider:
	    ns->sata.block_free_threshold = prcnt;
	    break;
	case SAS2DiskMgr_provider:
	    ns->sas.block_free_threshold = prcnt;
	    break;
	case SolidStateMgr_provider:
	    ns->ssd.block_free_threshold = prcnt;
	    break;
	default:
	    err = EINVAL;
    }
 end:
    NS_HASH_RUNLOCK();
    return err;
}	/* ns_stat_set_tier_block_free_threshold */


int
ns_stat_get_tier_read_size(const char		*ns_uuid,
			   nkn_provider_type_t	ptype,
			   uint32_t		*read_size)
{
    ns_stat_entry_t *ns;
    int64_t keyhash;
    int err = 0;

    keyhash = n_str_hash(ns_uuid);

    NS_HASH_RLOCK();
    ns = nhash_lookup(s_ns_stat_hash, keyhash, (void *)ns_uuid /*key*/);
    if (ns == NULL || ns->state == NS_STATE_DELETED) {
	err = ENOENT;
	goto end;
    }
    switch (ptype) {
	case SATADiskMgr_provider:
	    *read_size = ns->sata.read_size;
	    break;
	case SAS2DiskMgr_provider:
	    *read_size = ns->sas.read_size;
	    break;
	case SolidStateMgr_provider:
	    *read_size = ns->ssd.read_size;
	    break;
	default:
	    err = EINVAL;
    }
 end:
    NS_HASH_RUNLOCK();
    return err;
}	/* ns_stat_get_tier_read_size */


int
ns_stat_set_tier_read_size(const char		*ns_uuid,
			   nkn_provider_type_t	ptype,
			   uint32_t		read_size)
{
    ns_stat_entry_t *ns;
    int64_t keyhash;
    int err = 0;

    keyhash = n_str_hash(ns_uuid);

    NS_HASH_RLOCK();
    ns = nhash_lookup(s_ns_stat_hash, keyhash, (void *)ns_uuid /*key*/);
    if (ns == NULL || ns->state == NS_STATE_DELETED) {
	err = ENOENT;
	goto end;
    }
    switch (ptype) {
	case SATADiskMgr_provider:
	    ns->sata.read_size = read_size;
	    break;
	case SAS2DiskMgr_provider:
	    ns->sas.read_size = read_size;
	    break;
	case SolidStateMgr_provider:
	    ns->ssd.read_size = read_size;
	    break;
	default:
	    err = EINVAL;
    }
 end:
    NS_HASH_RUNLOCK();
    return err;
}	/* ns_stat_set_tier_read_size */


int
ns_stat_get_tier_max_disk_usage(const char		*ns_uuid,
				nkn_provider_type_t	ptype,
				size_t			*max_size)
{
    ns_stat_entry_t *ns;
    int64_t keyhash;
    int err = 0;

    keyhash = n_str_hash(ns_uuid);

    NS_HASH_RLOCK();
    ns = nhash_lookup(s_ns_stat_hash, keyhash, (void *)ns_uuid /*key*/);
    if (ns == NULL || ns->state == NS_STATE_DELETED) {
	err = ENOENT;
	goto end;
    }
    switch (ptype) {
	case SATADiskMgr_provider:
	    *max_size = ns->sata.max_disk_usage;
	    break;
	case SAS2DiskMgr_provider:
	    *max_size = ns->sas.max_disk_usage;
	    break;
	case SolidStateMgr_provider:
	    *max_size = ns->ssd.max_disk_usage;
	    break;
	default:
	    err = EINVAL;
    }
 end:
    NS_HASH_RUNLOCK();
    return err;
}	/* ns_stat_get_tier_max_disk_usage */


int
ns_stat_set_tier_max_disk_usage(const char	    *ns_uuid,
				nkn_provider_type_t ptype,
				size_t		    max_size)
{
    ns_stat_entry_t *ns;
    int64_t keyhash;
    int err = 0;

    keyhash = n_str_hash(ns_uuid);

    NS_HASH_RLOCK();
    ns = nhash_lookup(s_ns_stat_hash, keyhash, (void *)ns_uuid /*key*/);
    if (ns == NULL || ns->state == NS_STATE_DELETED) {
	err = ENOENT;
	goto end;
    }
    switch (ptype) {
	case SATADiskMgr_provider:
	    ns->sata.max_disk_usage = max_size * 1024 *1024;
	    AO_store(&ns_stats[ns->index][NS_SATA_MAX_USAGE],
		     ns->sata.max_disk_usage);
	    break;
	case SAS2DiskMgr_provider:
	    ns->sas.max_disk_usage = max_size * 1024 *1024;
	    AO_store(&ns_stats[ns->index][NS_SAS_MAX_USAGE],
		     ns->sas.max_disk_usage);
	    break;
	case SolidStateMgr_provider:
	    ns->ssd.max_disk_usage = max_size * 1024 *1024;
	    AO_store(&ns_stats[ns->index][NS_SSD_MAX_USAGE],
		     ns->ssd.max_disk_usage);
	    break;
	default:
	    err = EINVAL;
    }
 end:
    NS_HASH_RUNLOCK();
    return err;
}	/* ns_stat_set_tier_max_disk_usage */


int
ns_stat_get_cache_pin_enabled_state(const char	*ns_uuid,
				    int		*enabled)
{
    ns_stat_entry_t *ns;
    int64_t keyhash;

    keyhash = n_str_hash(ns_uuid);

    NS_HASH_RLOCK();
    ns = nhash_lookup(s_ns_stat_hash, keyhash, (void *)ns_uuid /*key*/);
    if (ns == NULL || ns->state == NS_STATE_DELETED) {
	NS_HASH_RUNLOCK();
	return ENOENT;
    }
    *enabled = ns->cache_pin_enabled;
    NS_HASH_RUNLOCK();
    return 0;
}	/* ns_stat_get_cache_pin_enabled_state */


int
ns_stat_set_cache_pin_enabled_state(const char	*ns_uuid,
				    int		enabled)
{
    ns_stat_entry_t *ns;
    int64_t keyhash;

    keyhash = n_str_hash(ns_uuid);

    NS_HASH_RLOCK();
    ns = nhash_lookup(s_ns_stat_hash, keyhash, (void *)ns_uuid/*key*/);
    if (ns == NULL || ns->state == NS_STATE_DELETED) {
	NS_HASH_RUNLOCK();
	return ENOENT;
    }
    ns->cache_pin_enabled = enabled;
    AO_store(&ns_stats[ns->index][NS_CACHE_PIN_SPACE_ENABLED], enabled);
    NS_HASH_RUNLOCK();
    return 0;
}	/* ns_stat_set_cache_pin_enabled_state */


int
ns_stat_set_cache_pin_max_obj_size(const char	*ns_uuid,
				   uint64_t	max_bytes)
{
    ns_stat_entry_t *ns;
    int64_t keyhash;

    keyhash = n_str_hash(ns_uuid);

    NS_HASH_RLOCK();
    ns = nhash_lookup(s_ns_stat_hash, keyhash, (void *)ns_uuid /*key*/);
    if (ns == NULL || ns->state == NS_STATE_DELETED) {
	NS_HASH_RUNLOCK();
	return ENOENT;
    }
    ns->cache_pin_max_obj_bytes = max_bytes;
    AO_store(&ns_stats[ns->index][NS_CACHE_PIN_SPACE_OBJ_LIMIT], max_bytes);
    NS_HASH_RUNLOCK();
    return 0;
}	/* ns_stat_set_cache_pin_max_obj_size */


int
ns_stat_get_cache_pin_max_obj_size(const char	*ns_uuid,
				   uint64_t	*max_bytes)
{
    ns_stat_entry_t *ns;
    int64_t keyhash;

    keyhash = n_str_hash(ns_uuid);

    NS_HASH_RLOCK();
    ns = nhash_lookup(s_ns_stat_hash, keyhash, (void *)ns_uuid /*key*/);
    if (ns == NULL || ns->state == NS_STATE_DELETED) {
	NS_HASH_RUNLOCK();
	return ENOENT;
    }
    *max_bytes = ns->cache_pin_max_obj_bytes;
    NS_HASH_RUNLOCK();
    return 0;
}	/* ns_stat_get_cache_pin_enabled_state */


int
ns_stat_set_cache_pin_diskspace(const char *ns_uuid,
				uint64_t   max_resv_bytes)
{
    ns_stat_entry_t *ns;
    int64_t keyhash;

    keyhash = n_str_hash(ns_uuid);

    /* Perform operation w/ the token under lock, so refcnt is not needed */
    NS_HASH_RLOCK();
    ns = nhash_lookup(s_ns_stat_hash, keyhash, (void *)ns_uuid /*key*/);
    if (ns == NULL || IS_NS_STATE_DELETING(ns)) {
	NS_HASH_RUNLOCK();
	return ENOENT;
    }
    ns->cache_pin_resv_bytes = max_resv_bytes;
    AO_store(&ns_stats[ns->index][NS_CACHE_PIN_SPACE_RESV_TOTAL],
	     max_resv_bytes);

    NS_HASH_RUNLOCK();
    return 0;
}	/* ns_stat_set_cache_pin_diskspace */


/*
 * Return a refcnt'd stat token
 */
ns_stat_token_t
ns_stat_get_stat_token(const char *ns_uuid)
{
    ns_stat_entry_t *ns;
    int64_t keyhash, old_val;
    ns_stat_token_t stoken;
    uint32_t num_dup_ns;

    keyhash = n_str_hash(ns_uuid);

    NS_HASH_RLOCK();
    ns = nhash_lookup(s_ns_stat_hash, keyhash, (void *)ns_uuid /*key*/);
    if (ns == NULL || IS_NS_STATE_DELETING(ns)) {
	NS_HASH_RUNLOCK();
	if (ns == NULL)
	    glob_ns_stat_get_lookup_err++;
	else if (IS_NS_STATE_DELETING(ns))
	    glob_ns_stat_get_lookup_deleted_err++;
	return NS_NULL_STAT_TOKEN;
    }
    NKN_ASSERT(ns->index < NS_NUM_STAT_ENTRIES);
    old_val = AO_fetch_and_add1(&s_ns_stat_refcnt[ns->index]);
    NKN_ASSERT(old_val >= 0);
    num_dup_ns = ns->num_dup_ns;
    NS_HASH_RUNLOCK();
    glob_ns_stat_get_cnt++;

    if (num_dup_ns) {
	DBG_LOG(SEVERE, MOD_NAMESPACE,
		"Stat update request for namespace (%s) having multiple"
		" references", ns_uuid);
    }

    stoken.u.stat_token_data.val = ns->index;
    stoken.u.stat_token_data.gen = ns->token_curr_gen;
    return stoken;
}	/* ns_stat_get_stat_token */


int
ns_stat_free_stat_token(ns_stat_token_t ns_stoken)
{
    ns_stat_entry_t *ns;
    char     *key;
    int64_t  old_refcnt, keyhash;
    uint32_t ns_index = NS_STAT_TOKEN_IDX(ns_stoken);
    uint32_t gen = NS_STAT_TOKEN_GEN(ns_stoken);
    uint32_t curr_gen;
    int      free_ns = 0;

    if (ns_is_stat_token_null(ns_stoken)) {
	glob_ns_stat_free_null_token_err++;
	return 0;
    }

    key = ns_slot_info[ns_index]->ns_key;
    keyhash = n_str_hash(key);

    NS_HASH_RLOCK();
    ns = nhash_lookup(s_ns_stat_hash, keyhash, key);
    if (ns == NULL || ns->state == NS_STATE_DELETED) {
	/* We should always find the namespace stat object */
	NKN_ASSERT(0);
	NS_HASH_RUNLOCK();
	return ENOENT;
    }
    curr_gen = (uint32_t) AO_load(&ns_token_gen[ns_index]);
    old_refcnt = AO_fetch_and_sub1(&s_ns_stat_refcnt[ns_index]);
    assert(old_refcnt >= 1);
    if (ns->state == NS_STATE_TO_BE_DELETED) {
	/* Delete on last free */
	NKN_ASSERT(curr_gen == gen+1);
	if (curr_gen != gen+1)
	    glob_ns_stat_freedel_gen_mismatch_err++;
	if (old_refcnt == 1)
	    free_ns = 1;
	assert(old_refcnt != 0);	// Extra free called
	glob_ns_stat_freedel_cnt++;
    } else {
	NKN_ASSERT(curr_gen == gen);
	if (curr_gen != gen)
	    glob_ns_stat_free_gen_mismatch_err++;
	glob_ns_stat_free_cnt++;
    }
    NS_HASH_RUNLOCK();

    if (free_ns) {
	NS_HASH_WLOCK(1);
	if (nhash_remove(s_ns_stat_hash, keyhash, key) == 0) {
	    /* Object not found */
	    DBG_LOG(SEVERE, MOD_NAMESPACE,
		    "Unable to remove namespace stat hash table object: %s",
		    key);
	    assert(0);
	}
	ns_stat_delete_counters(ns->ns_key);
	ns->state = NS_STATE_DELETED;
	glob_ns_stat_delete_on_free_cnt++;
	NS_HASH_WUNLOCK(1);
    }
    return 0;
}	/* ns_stat_free_stat_token */


int
ns_stat_get_rpindex_from_nsuuid(const char *ns_uuid,
				int *rp_index)
{
    ns_stat_entry_t *ns_entry;
    int64_t	    keyhash;

    keyhash = n_str_hash(ns_uuid);

    NS_HASH_RLOCK();
    ns_entry = nhash_lookup(s_ns_stat_hash, keyhash, (void *)ns_uuid /*key*/);
    if (ns_entry == NULL) {
	NS_HASH_RUNLOCK();
	return EINVAL;
    }
    *rp_index = ns_entry->rp_index;
    NS_HASH_RUNLOCK();

    return 0;
}	/* ns_stat_get_rpindex_cache_name_dir */


int
ns_stat_add(ns_stat_token_t	stoken,
	    ns_stat_category_t	stat_type,
	    int64_t		add_val,
	    int64_t		*old_val)
{
    int64_t  tmp_val;
    uint32_t ns_index = stoken.u.stat_token_data.val;
    uint32_t gen = stoken.u.stat_token_data.gen;
    uint32_t curr_gen;

    curr_gen = (uint32_t) AO_load(&ns_token_gen[ns_index]);
    if (curr_gen != gen) {
	glob_ns_stat_add_gen_mismatch_err++;
	return NKN_STAT_GEN_MISMATCH;
    }

    if (stat_type >= NS_STAT_MAX) {
	glob_ns_stat_add_type_einval_err++;
	return NKN_STAT_TYPE_EINVAL;
    }

    tmp_val = AO_fetch_and_add(&ns_stats[ns_index][stat_type], add_val);

    if (old_val)
	*old_val = tmp_val;
    return 0;
}	/* ns_stat_add */


int
ns_stat_set(ns_stat_token_t	stoken,
	    ns_stat_category_t	stat_type,
	    int64_t		val)
{
    uint32_t ns_index = stoken.u.stat_token_data.val;
    uint32_t gen = stoken.u.stat_token_data.gen;
    uint32_t curr_gen;

    curr_gen = (uint32_t) AO_load(&ns_token_gen[ns_index]);
    if (curr_gen != gen) {
	glob_ns_stat_set_gen_mismatch_err++;
	return NKN_STAT_GEN_MISMATCH;
    }

    if (stat_type >= NS_STAT_MAX) {
	glob_ns_stat_set_type_einval_err++;
	return NKN_STAT_TYPE_EINVAL;
    }

    AO_store(&ns_stats[ns_index][stat_type], val);
    return 0;
}	/* ns_stat_add */


int
ns_stat_get(ns_stat_token_t	stoken,
	    ns_stat_category_t	stat_type,
	    int64_t		*curr_val)
{
    uint32_t ns_index = stoken.u.stat_token_data.val;
    uint32_t gen = stoken.u.stat_token_data.gen;
    uint32_t curr_gen;

    curr_gen = (uint32_t) AO_load(&ns_token_gen[ns_index]);
    if (curr_gen != gen) {
	glob_ns_stat_get_gen_mismatch_err++;
	return NKN_STAT_GEN_MISMATCH;
    }

    if (stat_type >= NS_STAT_MAX) {
	glob_ns_stat_get_type_einval_err++;
	return NKN_STAT_TYPE_EINVAL;
    }

    *curr_val = AO_load(&ns_stats[ns_index][stat_type]);
    return 0;
}	/* ns_stat_add */


struct user_data {
    ns_stat_category_t cat;
    uint64_t total;
    int (*disk_state_func)(int, ns_stat_category_t);
    void (*disk_set_rp_func)(ns_stat_category_t, uint32_t, uint64_t);
};

static void
ns_sum_column(void *key __attribute((unused)),
	      void *value,
	      void *userdata)
{
    struct user_data *ud = (struct user_data *)userdata;
    ns_stat_entry_t *ns = (ns_stat_entry_t *)value;
    ns_stat_category_t cat = ud->cat;
    uint64_t total = 0;
    int i;

    if (cat >= NS_USED_SPACE_SSD && cat <= NS_USED_SPACE_SATA) {
	for (i=NS_USED_SPACE_D1; i<NS_USED_SPACE_MAX; i++) {
	    if (ud->disk_state_func(i-NS_USED_SPACE_D1+1, cat)) {
		total += AO_load(&ns_stats[ns->index][i]);
		//		printf("cat=%d  total=%ld\n", cat, total);
	    }
	}
	AO_store(&ns_stats[ns->index][cat], total);
	if (ud->disk_set_rp_func)
	    ud->disk_set_rp_func(cat, ns->rp_index, total);
    }
    ud->total += total;
}	/* ns_sum_column */


int
ns_stat_total_subcategory(ns_stat_category_t cat,
			  int64_t *total)
{
    struct user_data ud;
    void *user_data = (void *)&ud;

    if (cat >= NS_USED_SPACE_D1 && cat < NS_USED_SPACE_MAX) {
	return EINVAL;
    }
    if (cat >= NS_USED_SPACE_SSD && cat <= NS_USED_SPACE_SATA) {
	memset(&ud, 0, sizeof(ud));
	user_data = &ud;
	ud.cat = cat;
	ud.disk_state_func = DM2_find_cache_running_state;
	ud.disk_set_rp_func = DM2_ns_set_rp_size_for_namespace;
    }
    NS_HASH_RLOCK();
    nhash_foreach(s_ns_stat_hash, ns_sum_column, user_data);
    NS_HASH_RUNLOCK();

    *total = ud.total;
    return 0;
}	/* ns_stat_total_subcategory */

