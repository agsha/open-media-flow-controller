/*
 * (C) Copyright 2008-9 Ankeena Networks, Inc
 *
 * This file contains code which implements the Disk Manager
 *
 * Author: Michael Nishimoto
 *
 * Non-obvious coding conventions:
 *	- All functions should begin with dm2_ or dm2_
 *	- All functions have a name comment at the end of the function.
 *	- All log messages use special logging macros
 *
 */
#ifndef NKN_DISKMGR2_LOCAL_H
#define NKN_DISKMGR2_LOCAL_H

#include <glib.h>
#include <pthread.h>
#include "zvm_api.h"

#include "nkn_hash.h"
#include "nkn_lockstat.h"
#include "nvsd_resource_mgr.h"
#include "nkn_namespace_stats.h"
#include "nkn_diskmgr2_disk_api.h"
#include "nkn_util.h"
#include "nkn_hash.h"
#include <atomic_ops.h>

#define DM2_MAX_CACHE_TYPE_LEN 20
#define DM2_MAX_CACHE_TYPE 20

/* The basename is 'cont_ptr/basename' and this is hashed to
 * store the uri_head in the ci_uri_hash_table
 */
#define DM2_URI_CONT_PTR_LEN	    16
#define DM2_MAX_BASENAME_ALLOC_LEN (DM2_URI_CONT_PTR_LEN + NAME_MAX)

/* Defines for Streaming Objects */
// Length is a power of 2 for calculation ease
#define DM2_STREAMING_OBJ_CONTENT_LEN (16*1024*1024)
#define DM2_CT_MAX_STREAMING_PUTS     100

/* Flags for dm2_do_stampout_attr() */
#define DM2_ATTR_IS_OK      0
#define DM2_ATTR_IS_CORRUPT 1

/* Flags for blocking on a lock */
#define DM2_NONBLOCKING 0
#define DM2_BLOCKING	1
/*
 * Leave 20MB worth of blocks free in ext3 f/s.  We could use the
 * block size in the statfs obj but then the compiler couldn't optimize
 * the expression as much.
 */
#define DM2_META_FS_THRESHOLD ((20*1024*1024)/(4*1024))

//Hotness val 0-128 will have individual evict buckets
//Hotness val 129 to 65535 will be grouped into power of two buckets

#define DM2_UNIQ_EVICT_BKTS   (128)
#define DM2_MAP_UNIQ_BKTS     (7) // 2^7 = 128
#define DM2_GROUP_EVICT_BKTS  (16 - DM2_MAP_UNIQ_BKTS)
#define DM2_MAX_EVICT_BUCKETS (DM2_GROUP_EVICT_BKTS + DM2_UNIQ_EVICT_BKTS + 1)

typedef struct dm2_evict_node {
    pthread_mutex_t evict_list_mutex;
    uint32_t	    num_entries;
    uint32_t	    num_blks;
    TAILQ_HEAD (dm2_evict_list, DM2_uri) dm2_evict_list_t;
} dm2_evict_node_t;

#define EVICT_WAIT	  1
#define EVICT_IN_PROGRESS 2
/* These defines might need to be revisted */
#define DM2_CI_DECAY_INTERVAL (2*60*60) // Diff between 2 decay events on disk

#define DM2_CACHE_PIN_INITIALIZER -1

typedef struct dm2_cache_type {
    pthread_t	     ct_evict_thread;
    pthread_mutex_t  ct_rwlock_mutex;
    pthread_cond_t   ct_rwlock_read_cond;
    pthread_cond_t   ct_rwlock_write_cond;
    uint16_t	     ct_rwlock_nreaders;
    uint16_t	     ct_rwlock_nwriters;
    uint16_t	     ct_rwlock_wait_nreaders;
    uint16_t	     ct_rwlock_wait_nwriters;
    uint64_t	     ct_rwlock_wowner;
    uint8_t	     ct_rwlock_lock_type;

    /* Lock stats which race.  Can collect on individual URI objects but
     * cannot display.  We can display on cache_type basis. */
    uint64_t	     ct_uri_wr_wait_times;// # times URI waits for WR lock
    uint64_t	     ct_uri_rd_wait_times;// # times URI waits for RD lock
    uint64_t	     ct_uri_wr_lock_times;// # times URI gets for WR lock
    uint64_t	     ct_uri_rd_lock_times;// # times URI gets for RD lock

    nkn_mutex_t	     ct_stat_mutex;

    char	     ct_name[DM2_MAX_CACHE_TYPE_LEN];
    int64_t	     ct_num;
    uint32_t	     ct_num_active_caches;
    AO_t	     ct_num_caches_preread_done;
    AO_t	     ct_num_caches_preread_success;
    pthread_rwlock_t ct_info_list_rwlock;
    GList	     *ct_info_list;
    GList	     *ct_del_info_list;
    GList	     *ct_del_cont_head_list;
    int32_t	     ct_weighting;
    uint8_t	     ct_internal_evict_working;		// 0 or 1
    uint8_t	     ct_internal_evict_percent;		// 0-100
    uint8_t	     ct_internal_high_water_mark;	// 0-100
    uint8_t	     ct_internal_low_water_mark;	// 0-100
    uint8_t	     ct_internal_time_low_water_mark;
    uint8_t	     ct_dm2_int_time_evict_in_progress; // flag - time eviction
    uint8_t	     ct_share_block_threshold;	//%occupancy to share the block
    nkn_hot_t	     ct_am_hot_evict_threshold;
    nkn_hot_t	     ct_am_hot_evict_candidate;
    nkn_provider_type_t ct_ptype;
    uint32_t	     ct_subptype;
    rp_type_en	     ct_rp_type;
    int32_t	     NOZCC_DECL(ct_tier_preread_done);	   // boolean
    int32_t	     NOZCC_DECL(ct_tier_preread_stat_opt); // boolean

    int64_t	     ct_dm2_num_put_threads;
    int64_t	     ct_dm2_num_get_threads;


    nkn_rwmutex_t    ct_dm2_uri_hash_table_rwlock;
    nkn_mutex_t	     ct_dm2_select_disk_mutex;

    GHashTable	     *ct_dm2_extent_hash_table;
    pthread_mutex_t  ct_dm2_extent_hash_table_mutex;

    GHashTable	     *ct_dm2_physid_hash_table;
    nkn_rwmutex_t    ct_dm2_physid_hash_table_rwlock;
    uint64_t	     ct_dm2_physid_hash_table_create_cnt;	// stat
    uint64_t	     ct_dm2_physid_hash_table_create_err;	// stat
    uint64_t	     ct_dm2_physid_hash_table_ext_remove_cnt;	// stat
    uint64_t	     ct_dm2_physid_hash_table_total_cnt;	// stat

    GHashTable	     *ct_dm2_conthead_hash_table;
    nkn_mutex_t      ct_dm2_conthead_hash_table_mutex;
    uint64_t	     ct_dm2_conthead_hash_table_create_cnt;	// stat
    uint64_t	     ct_dm2_conthead_hash_table_create_err;	// stat
    uint64_t	     ct_dm2_conthead_hash_table_total_cnt;	// stat
    /* time when a disk was enabled in this tier after preread */
    uint64_t	     ct_dm2_last_ci_enable_ts;

    /* Cache Tier Latency Stats */
    nkn_latency_t    ct_dm2_stat_latency;
    nkn_latency_t    ct_dm2_get_latency;
    nkn_latency_t    ct_dm2_put_latency;
    nkn_latency_t    ct_dm2_delete_latency;
    nkn_latency_t    ct_dm2_attrupdate_latency;

    uint64_t	     ct_dm2_last_decay_ts;
    uint64_t	     ct_dm2_decay_call_cnt;
    uint32_t	     ct_dm2_decay_interval;
    /* Cache Tier Statistics */
    int64_t	     ct_dm2_delete_not_ready;
    uint64_t	     ct_ext_evict_total_call_cnt;
    uint64_t	     ct_ext_evict_object_enoent_cnt;
    uint64_t	     ct_ext_evict_object_delete_cnt;
    uint64_t	     ct_ext_evict_object_delete_fail_cnt;

    uint64_t	     NOZCC_DECL(ct_dm2_put_cnt);
    uint64_t	     ct_dm2_put_err;
    uint64_t	     ct_dm2_put_not_ready;
    uint64_t	     NOZCC_DECL(ct_dm2_put_opt_not_found);

    uint64_t	     NOZCC_DECL(ct_dm2_stat_cnt);
    uint64_t	     ct_dm2_stat_err;
    uint64_t	     NOZCC_DECL(ct_dm2_stat_warn);
    uint64_t	     NOZCC_DECL(ct_dm2_stat_not_found);
    uint64_t	     NOZCC_DECL(ct_dm2_stat_not_ready);
    uint64_t	     NOZCC_DECL(ct_dm2_stat_opt_not_found);
    uint64_t	     ct_dm2_stat_dict_full;
    uint64_t	     ct_dm2_attr_update_opt_not_found;
    uint64_t	     ct_dm2_delete_opt_not_found;
    uint64_t	     ct_dm2_stat_uri_partial_cnt;

    uint64_t	     NOZCC_DECL(ct_dm2_get_cnt);
    uint64_t	     ct_dm2_get_physid_mismatch_cnt;
    uint64_t	     ct_dm2_get_err;

    uint64_t	     ct_dm2_attr_update_cnt;
    uint64_t	     ct_dm2_attr_update_write_cnt;

    AO_t	     ct_dm2_stat_curr_cnt;
    AO_t	     ct_dm2_pr_stat_curr_cnt;
    AO_t	     ct_dm2_get_curr_cnt;
    AO_t	     ct_dm2_delete_curr_cnt;
    AO_t	     ct_dm2_put_curr_cnt;
    AO_t	     ct_dm2_attr_update_curr_cnt;
    AO_t	     ct_dm2_prov_stat_curr_cnt;
    AO_t	     ct_dm2_streaming_curr_cnt;

    uint64_t	     NOZCC_DECL(ct_dm2_container_open_cnt);
    uint64_t	     ct_dm2_container_open_err;
    uint64_t	     NOZCC_DECL(ct_dm2_container_read_cnt);
    uint64_t	     ct_dm2_container_read_err;
    uint64_t	     ct_dm2_container_short_read_err;
    uint64_t	     ct_dm2_container_eof_read_err;
    uint64_t	     NOZCC_DECL(ct_dm2_container_read_bytes);
    uint64_t	     NOZCC_DECL(ct_dm2_container_close_cnt);
    uint64_t	     NOZCC_DECL(ct_dm2_container_write_cnt);
    uint64_t	     ct_dm2_container_write_err;
    uint64_t	     NOZCC_DECL(ct_dm2_container_write_bytes);

    uint64_t	     NOZCC_DECL(ct_dm2_attrfile_open_cnt);
    uint64_t	     ct_dm2_attrfile_open_err;
    uint64_t	     NOZCC_DECL(ct_dm2_attrfile_read_cnt);
    uint64_t	     ct_dm2_attrfile_read_err;
    uint64_t	     ct_dm2_attrfile_cod_null_err;
    uint64_t	     ct_dm2_attrfile_alloc_fail_err;
    uint64_t	     NOZCC_DECL(ct_dm2_attrfile_read_bytes);
    uint64_t	     NOZCC_DECL(ct_dm2_attrfile_write_cnt);
    uint64_t	     NOZCC_DECL(ct_dm2_attrfile_write_bytes);
    uint64_t	     ct_dm2_attrfile_read_cod_mismatch_warn;

    uint64_t	     NOZCC_DECL(ct_dm2_raw_read_cnt);
    uint64_t	     NOZCC_DECL(ct_dm2_raw_read_bytes);
    uint64_t	     NOZCC_DECL(ct_dm2_raw_write_cnt);
    uint64_t	     NOZCC_DECL(ct_dm2_raw_write_bytes);
    uint64_t	     ct_dm2_delete_bytes;
    uint64_t	     ct_dm2_end_put_cnt;

    uint64_t	     ct_dm2_ingest_bytes;
    uint64_t	     ct_dm2_evict_bytes;

    uint64_t	     ct_dm2_get_cod_mismatch_warn;
    uint64_t	     ct_dm2_get_cod_mismatch_err;
    uint64_t	     ct_dm2_stat_cod_mismatch_warn;
    uint64_t	     ct_dm2_put_cod_mismatch_warn;
    uint64_t	     ct_dm2_delete_cod_mismatch_warn;
    uint64_t	     ct_dm2_update_attr_cod_mismatch_warn;
    uint64_t	     ct_dm2_update_attr_version_mismatch_err;
    uint64_t	     ct_dm2_int_evict_cod_alloc_err;
    uint64_t	     ct_dm2_put_bad_uri_err;
    uint64_t	     ct_dm2_delete_bad_uri_err;
    uint64_t	     ct_dm2_cache_disabling;

    /* Internal eviction counters */
    uint64_t	     ct_dm2_int_evict_call_cnt;
    uint64_t	     ct_dm2_int_evict_del_cnt;
    uint64_t	     ct_dm2_int_evict_dict_full_cnt;
    uint64_t	     ct_dm2_int_evict_disk_full_cnt;
    uint64_t	     ct_dm2_int_evict_time_cnt;
    uint64_t	     ct_dm2_last_disk_full_int_evict_ts;
    uint64_t	     ct_dm2_last_dict_full_int_evict_ts;
    uint64_t         ct_dm2_last_time_int_evict_ts;

    uint64_t	     ct_dm2_preread_duplicate_skip_cnt;
    uint64_t	     ct_dm2_preread_hash_insert_cnt;
    uint64_t	     ct_dm2_preread_hash_free_cnt;
    uint64_t	     ct_dm2_preread_start_ts;
    NCHashTable	     *ct_dm2_preread_hash;
    nkn_mutex_t      ct_dm2_preread_mutex;

    /* Eviction Hand-off values */
    nkn_mutex_t	    ct_dm2_evict_cond_mutex;
    pthread_cond_t  ct_dm2_evict_cond;
    uint32_t	    ct_dm2_evict_blks;
    uint32_t	    ct_dm2_ci_evict_blks;
    uint32_t	    ct_dm2_ci_max_evict_bkt;
    int32_t	    ct_num_evict_disks;
    int32_t	    ct_num_evict_disk_prcnt;

    int8_t	    ct_dm2_evict_reason;
    int8_t	    ct_dm2_evict_more;
    int8_t	    ct_dm2_decay_objects;
    int8_t	    ct_evict_hour;
    int8_t	    ct_evict_min;
    int8_t	    ct_dm2_preread_opt;

    AO_t	    ct_dm2_ingest_objects;
    AO_t	    ct_dm2_delete_objects;

    AO_t	    ct_dm2_bkt_evict_blks[DM2_MAX_EVICT_BUCKETS];

    AO_t	    ct_dm2_init_thread_exit_cnt;
} dm2_cache_type_t;

/* enum for dm2_uri* functions to choose lock/unlock a ci atomically */

typedef enum {
    DM2_NOLOCK_CI,
    DM2_RLOCK_CI,
    DM2_RUNLOCK_CI
} dm2_ci_lock_t;

#define DM2_CONT_HEAD_UNLOCKED 3
#define DM2_CONT_HEAD_RLOCKED  4
#define DM2_CONT_HEAD_WLOCKED  5

//DM2 flag to delete expired uols
#define DM2_DELETE_EXPIRED_UOL          1
//DM2 flag to delete big attribute uols
#define DM2_DELETE_BIG_ATTR_UOL         2

typedef struct dm2_preread_queue_s {
    GQueue          q;
    pthread_mutex_t qlock;
    int		    q_init_done;
} dm2_preread_q_t;

/*
 * pr_q: argument passed to preread threads
 * ci_count:
 *	- Set at beginning of preread to # of running disks in tier
 *	- Incremented when 'cache enable' is done
 *	- Decremented when 'cache disable' is done
 */
typedef struct dm2_preread_arg_s {
    AO_t             pr_thread_done_cnt;
    pthread_key_t    pr_key;
    dm2_preread_q_t  pr_q;
} dm2_preread_arg_t;

typedef struct dm2_preread_hash_entry_s {
    NKN_CHAIN_ENTRY pr_entry;
    char cont[MAX_URI_SIZE];
} dm2_preread_hash_entry_t;

/*
 * Object which describes each cache device.
 * - set_cache_sec_cnt: Temporary field used during startup.  We could change
 *   this field to be in terms of pages by changing the init scripts.  It
 *   would be useful to leave this leave this in terms of sectors until we
 *   can convert to pages later on.
 *
 * Notes: XXXmiken
 * - Each object must be locked during shutdown.  Let current operations
 *   complete, but lock our further operations.
 * - The entire global structure must be locked before removing or adding
 *   new entries into the array
 */
typedef struct dm2_cache_info {
    dm2_preread_arg_t	ci_preread_arg;
    pthread_t		ci_preread_thread;
    AO_t		ci_dm2_preread_q_depth;
    uint32_t		ci_preread_done;
    int32_t		ci_preread_errno;
    int32_t		ci_preread_stat_opt; // boolean
    int32_t		ci_preread_stat_cnt;
    int32_t		ci_preread_thread_active;

    uint8_t		ci_disabling;
    uint8_t		ci_updating;
    uint8_t		ci_unused1[6];
    AO_t		ci_update_cnt;
    AO_t		ci_select_cnt;

    pthread_t		ci_init_thread;
    pthread_t		ci_evict_thread;
    size_t		ci_evict_blks;
    int			ci_evict_thread_state;
    int			ci_stop_eviction;

    GHashTable		*ci_uri_hash_table;
    uint64_t		ci_uri_hash_table_create_cnt;
    uint64_t		ci_uri_hash_table_create_err;
    uint64_t		ci_uri_hash_table_total_cnt;
    pthread_rwlock_t	ci_dev_rwlock;

    nkn_mutex_t		ci_uri_lock_mutex;
    nkn_mutex_t		ci_uri_hash_table_mutex;
    nkn_mutex_t		ci_dm2_delete_mutex;
    STAILQ_HEAD(dm2_uri_lock_q_t, uri_lock) ci_uri_lock_q;

    dm2_evict_node_t	*evict_buckets;

    uint32_t	 ci_drive_size_gb;
    uint32_t	 ci_pe_cycles;

    char	 ci_mountpt[DM2_MAX_MOUNTPT];
    char	 raw_devname[DM2_MAX_RAWDEV];  // raw device (not f/s)
    char	 fs_devname[DM2_MAX_FSDEV];
    char	 disk_name[DM2_MAX_DISKNAME];
    char	 stat_name[DM2_MAX_STATNAME];
    char	 serial_num[DM2_MAX_SERIAL_NUM];
    char	 mgmt_name[DM2_MAX_MGMTNAME];
    dm2_cache_type_t *ci_cttype;	// Cache Type ptr
    int		 list_index;		// virtual disk index
    dm2_bitmap_t bm;
    int8_t	 valid_cache;		// boolean
    int8_t	 cnt_registered;	// boolean
    int8_t	 no_meta;		// boolean
    int8_t	 type_unknown;		// boolean
    int8_t	 pr_processed;		// taken up for pre-read processing
    int8_t	 ci_throttle;		//boolean - writes needs throttle
    int32_t	 weighting;
    dm2_mgmt_state_t state_overall;
    int8_t	 state_cache_enabled;
    int8_t	 state_activated;
    int8_t	 state_missing;
    int8_t	 state_commented_out;
    int8_t	 state_not_production;	// No real serial # for this disk found
    size_t	 set_cache_sec_cnt;	// set size of partition: # sectors
    size_t	 set_cache_page_cnt;	// set size of partition: # pages
    size_t	 set_cache_blk_cnt;	// set size of partition: # blocks

    uint64_t	 ci_dm2_last_decay_ts;
    /* Individual Disk Statictics */
    uint64_t	 ci_dm2_put_cnt;
    uint64_t	 ci_dm2_stat_cnt;
    uint64_t	 ci_dm2_get_cnt;

    uint64_t	 ci_dm2_raw_read_cnt;
    uint64_t	 ci_dm2_raw_read_bytes;
    uint64_t	 ci_dm2_raw_write_cnt;
    uint64_t	 ci_dm2_raw_write_bytes;
    uint64_t	 ci_dm2_bitmap_read_cnt;
    uint64_t	 ci_dm2_bitmap_read_bytes;
    uint64_t	 ci_dm2_bitmap_write_cnt;
    uint64_t	 ci_dm2_bitmap_write_bytes;
    uint64_t	 ci_dm2_cont_read_cnt;
    uint64_t	 ci_dm2_cont_read_bytes;
    uint64_t	 ci_dm2_cont_write_cnt;
    uint64_t	 ci_dm2_cont_write_bytes;
    uint64_t	 ci_dm2_attr_read_cnt;
    uint64_t	 ci_dm2_attr_read_bytes;
    uint64_t	 ci_dm2_attr_write_cnt;
    uint64_t	 ci_dm2_attr_write_bytes;
    uint64_t	 ci_dm2_put_statfs_chk_cnt;
    uint64_t	 ci_dm2_put_no_meta_space_err;
    uint64_t	 ci_dm2_free_meta_blks_cnt;
    uint64_t	 ci_dm2_remove_attrfile_cnt;
    uint64_t	 ci_dm2_remove_contfile_cnt;
    uint64_t	 ci_dm2_uri_lock_get_cnt;
    uint64_t	 ci_dm2_uri_lock_release_cnt;
    uint64_t	 ci_dm2_uri_lock_drop_cnt;
    uint64_t	 ci_dm2_uri_lock_in_use_cnt;

    uint64_t	 ci_dm2_ext_delete_short_read_err;
    uint64_t	 ci_dm2_ext_delete_bad_magic_err;
    uint64_t	 ci_dm2_ext_delete_basename_err;
    uint64_t	 ci_dm2_attr_delete_short_read_err;
    uint64_t	 ci_dm2_attr_delete_bad_magic_err;
    uint64_t	 ci_dm2_attr_delete_basename_err;

    uint64_t	ci_dm2_ext_evict_total_cnt;
    uint64_t	ci_dm2_ext_evict_fail_cnt;
    uint64_t	ci_dm2_ext_append_cnt;

    uint64_t	ci_soft_rejects;
    uint64_t	ci_hard_rejects;

} dm2_cache_info_t;

typedef struct dm2_prq_entry_s {
    dm2_cache_info_t *ci;
    char             cont[MAX_URI_SIZE];
} dm2_prq_entry_t;

typedef struct ns_physid_hash_entry_s {
    NKN_CHAIN_ENTRY ph_entry;
    uint64_t	    physid;
} ns_physid_hash_entry_t;

/* Strings used for monitoring disk caches */
#define DM2_PAGE_SIZE_MON	"dm2_page_size"
#define DM2_BLOCK_SIZE_MON	"dm2_block_size"
#define DM2_FREE_PAGES_MON	"dm2_free_pages"
#define DM2_TOTAL_PAGES_MON	"dm2_total_pages"
#define DM2_FREE_BLOCKS_MON	"dm2_free_blocks"
#define DM2_TOTAL_BLOCKS_MON	"dm2_total_blocks"
#define DM2_FREE_RESV_BLOCKS_MON "dm2_free_resv_blocks"

#define DM2_PUT_CNT		"dm2_put_cnt"
#define DM2_PUT_ERR		"dm2_put_err"
#define DM2_RAW_READ_CNT_MON	"dm2_raw_read_cnt"
#define DM2_RAW_READ_BYTES_MON	"dm2_raw_read_bytes"
#define DM2_RAW_WRITE_CNT_MON	"dm2_raw_write_cnt"
#define DM2_RAW_WRITE_BYTES_MON "dm2_raw_write_bytes"

#define DM2_PROVIDER_TYPE_MON	"dm2_provider_type"
#define DM2_PUT_CNT_MON		"dm2_put_curr_cnt"
#define DM2_STATE_MON		"dm2_state"
#define DM2_URI_HASH_CNT_MON	"dm2_uri_hash_cnt"
#define DM2_NO_META_MON		"dm2_no_more_meta"
#define DM2_NO_META_ERR_MON	"dm2_no_more_meta_err"
#define DM2_FREE_META_BLKS_MON	"dm2_approx_free_meta_blks_cnt"
#define DM2_RM_ATTRFILE_CNT_MON "dm2_remove_attrfile_cnt"
#define DM2_RM_CONTFILE_CNT_MON "dm2_remove_contfile_cnt"
#define DM2_URI_LOCK_IN_USE_CNT_MON "dm2_uri_lock_in_use_cnt"


/* Strings used for monitoring cache tiers */
#define DM2_ATTRFILE_OPEN_CNT_MON "dm2_attrfile_open_cnt"
#define DM2_ATTRFILE_OPEN_ERR_MON "dm2_attrfile_open_err"
#define DM2_ATTRFILE_READ_CNT_MON "dm2_attrfile_read_cnt"
#define DM2_ATTRFILE_READ_ERR_MON "dm2_attrfile_read_err"
#define DM2_ATTRFILE_READ_BYTES_MON "dm2_attrfile_read_bytes"
#define DM2_ATTRFILE_WRITE_BYTES_MON "dm2_attrfile_write_bytes"
#define DM2_ATTRFILE_READ_COD_MISMATCH_WARN_MON "dm2_attrfile_read_cod_mismatch_warn"
#define DM2_ATTRFILE_WRITE_CNT_MON "dm2_attrfile_write_cnt"

#define DM2_CONTAINER_OPEN_CNT_MON "dm2_container_open_cnt"
#define DM2_CONTAINER_OPEN_ERR_MON "dm2_container_open_err"
#define DM2_CONTAINER_WRITE_BYTES_MON "dm2_container_write_bytes"
#define DM2_CONTAINER_WRITE_CNT_MON "dm2_container_write_cnt"
#define DM2_CONTAINER_READ_BYTES_MON "dm2_container_read_bytes"
#define DM2_CONTAINER_READ_CNT_MON "dm2_container_read_cnt"

#define DM2_DELETE_BYTES_MON	"dm2_delete_bytes"
#define DM2_PREREAD_DONE_MON	"dm2_preread_done"
#define DM2_PREREAD_STAT_OPT_CNT_MON "dm2_preread_stat_opt_cnt"
#define DM2_PREREAD_PUT_OPT_CNT_MON "dm2_preread_put_opt_cnt"
#define DM2_PREREAD_ATTR_UPDATE_OPT_CNT_MON "dm2_preread_attr_update_opt_cnt"
#define DM2_PREREAD_DELETE_OPT_CNT_MON "dm2_preread_delete_opt_cnt"
#define DM2_PREREAD_STAT_OPT_MON "dm2_preread_stat_opt"
#define DM2_PREREAD_STAT_CNT_MON "dm2_preread_stat_cnt"
#define DM2_PREREAD_Q_DEPTH_MON	 "dm2_preread_q_depth"
#define DM2_PUT_NOT_READY_MON	"dm2_put_not_ready_cnt"
#define DM2_STAT_NOT_READY_MON	"dm2_stat_not_ready_cnt"
#define DM2_STAT_DICT_FULL_MON  "dm2_stat_dict_full_cnt"

#define DM2_INTEVICT_HIGH_MARK	"dm2_int_evict_high_water_mark"
#define DM2_INTEVICT_LOW_MARK	"dm2_int_evict_low_water_mark"
#define DM2_INTEVICT_PERCENT	"dm2_int_evict_percent"

#define DM2_NUM_PUT_THRDS	"dm2_put_thr_cnt"
#define DM2_NUM_GET_THRDS	"dm2_get_thr_cnt"

#define DM2_EXT_EVICT_CALLS_MON	    "dm2_ext_evict_call_cnt"
#define DM2_EXT_EVICT_DELS_MON  "dm2_ext_evict_delete_cnt"
#define DM2_EXT_EVICT_FAILS_MON	    "dm2_ext_evict_fail_cnt"
#define DM2_EXT_EVICT_ENONENTS_MON  "dm2_ext_evict_enoent_cnt"

typedef struct dm2_cache_match {
    dm2_cache_info_t *cm_ci;
    dm2_cache_type_t *cm_ct;
    int		     cm_count;
} dm2_cache_match_t;


struct dm2_hashnode {
    void *h_key;
    void *h_value;
    struct dm2_hash *h_next;
    uint32_t h_hash_key;
} dm2_hashnode_t;

#ifndef OLD_GLIB_2_12
struct dm2_hashtable {
    int32_t sz;
    int32_t mod;
    int32_t mask;
    int32_t nnd;
    struct dm2_hashnode *nds;
    int32_t (*hfunc)(void *ptr);
    int32_t (*eqfunc)(void *ptr);
    int32_t refcnt;
    int32_t v;
} dm2_hashtable_t;
#else
struct dm2_hashtable {
    int32_t sz;
    int32_t nnd;
    struct dm2_hashnode **nds;
    int32_t (*hfunc)(void *ptr);
    int32_t (*eqfunc)(void *ptr);
    int32_t refcnt;
    int32_t v;
} dm2_hashtable_t;
#endif

struct dm2_iter {
    struct dm2_hashtable *hashtable;
    struct dm2_hashnode *prev_nd;
    struct dm2_hashnode *nd;
    int32_t pos;
    int32_t noop;
    int32_t v;
};

typedef struct dm2_disk_slot {
    char ds_serialnum[DM2_MAX_SERIAL_NUM];
    char ds_devname[DM2_MAX_DISKNAME];
    int  ds_slot;
} dm2_disk_slot_t;

/*
 * Can not make this an inline because I'm calling alloca()
 *
 * "/nkn/mnt/dc_xx" - 14
 * strlen(uri_dir) = directory len
 * 1 = slash character
 * strlen(NKN_CONTAINER_ATTR_NAME) = len of basename
 * Use the biggest length here ('container' vs 'attributes-2') as
 * 'DM2_FORM_CONT_DIR' is commonly used to create both attribute &
 * container pathnames.
 * 1 = end of string
 */
#define DM2_FORM_CONT_DIR(cont, p) \
{\
    p = alloca(14 + strlen((cont)->c_uri_dir) \
		+ 1 \
		+ strlen(NKN_CONTAINER_ATTR_NAME) \
		+ 1); \
    strcpy(p, NKN_DM_MNTPT); \
    strcat(p, "/"); \
    strcat(p, cont->c_dev_ci->mgmt_name); \
    strcat(p, cont->c_uri_dir); \
    strcat(p, "/"); \
}

#define DM2_GET_CONTPATH(cont, cp) \
{\
    DM2_FORM_CONT_DIR(cont, cp) \
    strcat(cp, NKN_CONTAINER_NAME);\
}

#define DM2_GET_ATTRPATH(cont, cp) \
{\
    DM2_FORM_CONT_DIR(cont, cp) \
    strcat(cp, NKN_CONTAINER_ATTR_NAME);\
}

/* Global variables */
extern AO_t *dm2_signal_disk_change_to_mgmt;
extern int dm2_show_locks;
extern dm2_cache_type_t g_cache2_types[DM2_MAX_CACHE_TYPE];
extern GStaticMutex dm2_cache2_types_mutex;
extern AO_t glob_dm2_alloc_total_bytes;
extern int64_t glob_dm2_num_cache_types,
    NOZCC_DECL(glob_dm2_container_open_cnt),
    glob_dm2_container_open_err,
    NOZCC_DECL(glob_dm2_container_close_cnt),
    glob_dm2_container_close_err,
    NOZCC_DECL(glob_dm2_container_read_cnt),
    glob_dm2_container_read_err,
    glob_dm2_container_read_bytes,
    glob_dm2_bitmap_open_cnt,
    glob_dm2_bitmap_open_err,
    glob_dm2_bitmap_close_cnt,
    glob_dm2_bitmap_close_err,
    glob_dm2_bitmap_read_cnt,
    glob_dm2_bitmap_read_err,
    glob_dm2_bitmap_read_bytes,
    NOZCC_DECL(glob_dm2_attr_open_cnt),
    glob_dm2_attr_open_err,
    NOZCC_DECL(glob_dm2_attr_close_cnt),
    glob_dm2_attr_close_err,
    NOZCC_DECL(glob_dm2_attr_read_cnt),
    glob_dm2_attr_read_err,
    glob_dm2_attr_read_cod_null_err,
    glob_dm2_attr_read_alloc_failed_err,
    NOZCC_DECL(glob_dm2_attr_read_bytes),
    glob_dm2_attr_read_setid_cnt,
    glob_dm2_uri_create_race,

    glob_dm2_attr_delete_err,
    glob_dm2_attr_delete_read_err,
    glob_dm2_attr_delete_short_read_err,
    glob_dm2_attr_delete_repeated_delete_cnt,
    glob_dm2_attr_delete_bad_magic_err,
    glob_dm2_attr_delete_bad_version_err,
    glob_dm2_attr_delete_basename_mismatch_err,
    glob_dm2_attr_delete_write_err,
    glob_dm2_attr_delete_short_write_err,
    glob_dm2_attr_delete_no_attr_err,
    glob_dm2_attr_delete_open_err,
    glob_dm2_attr_delete_eio_override,

    glob_dm2_ext_delete_err,
    glob_dm2_ext_delete_read_err,
    glob_dm2_ext_delete_short_read_err,
    glob_dm2_ext_delete_repeated_delete_cnt,
    glob_dm2_ext_delete_bad_magic_err,
    glob_dm2_ext_delete_bad_version_err,
    glob_dm2_ext_delete_basename_mismatch_err,
    glob_dm2_ext_delete_write_err,
    glob_dm2_ext_delete_short_write_err,
    glob_dm2_ext_delete_eio_override,

    glob_dm2_put_expiry_cnt,
    glob_dm2_put_delete_expired_uol_err,

    NOZCC_DECL(glob_dm2_preread_stat_cnt),
    glob_dm2_preread_disabled,

    glob_dm2_cod_null_cnt,
    glob_dm2_mem_overflow;

extern uint64_t glob_dm2_mem_max_bytes;
extern uint64_t glob_dm2_init_done;

extern int dm2_perform_get_chksum;
extern int dm2_perform_put_chksum;
extern int dm2_print_mem;
extern int dm2_assert_for_disk_inconsistency;
extern int dm2_stop_preread;
extern nkn_provider_type_t glob_dm2_lowest_tier_ptype;

/* Prototypes */
uint64_t dm2_physid_hash(const void *v, size_t len);
int dm2_physid_equal(const void *v1, const void *v2);
void dm2_spawn_dm2_evict_thread(void);
void* dm2_evict_ci_fn(void *arg);
void dm2_reset_preread_stop_state(void);
void dm2_update_tier_preread_state(dm2_cache_type_t *ct);
void dm2_set_glob_preread_state(nkn_provider_type_t ptype, int state);
void dm2_cleanup_preread_hash(dm2_cache_type_t *ct);
void* dm2_preread_main_thread(void *arg);
void* dm2_preread_worker_thread(void *arg);
void dm2_preread_ci(dm2_cache_info_t *ci);
void dm2_preread_q_lock_init(dm2_preread_q_t *pr_q);
int dm2_check_meta_fs_space(dm2_cache_info_t  *ci);
void dm2_container_attrfile_remove(dm2_container_t *free_cont);
int DM2_type_delete(MM_delete_resp_t *delete, dm2_cache_type_t *ct,
		    dm2_cache_info_t **ci);
int dm2_cache_array_map_ret_idx(nkn_provider_type_t ptype);
void dm2_ct_update_thresholds(dm2_cache_type_t *ct);
int dm2_get_am_thresholds(int      ptype,
			  int      *cache_use_percent,
			  uint32_t *max_io_throughput,
			  int      *hotness,
			  char     *ctype);

void dm2_setup_am_thresholds(uint32_t max_io_throughput,
			     int32_t hotness_threshold,
			     nkn_provider_type_t ptype,
			     int8_t sub_ptype __attribute((unused)));
dm2_cache_type_t *dm2_find_cache_list(const char *type,
				      const int  num_cache_types);
void dm2_register_ptype_mm_am(dm2_cache_type_t *ct);
int dm2_bitmap_read_free_pages(dm2_cache_info_t *ci,
			       const int8_t cache_enabled);
int dm2_bitmap_process(dm2_cache_info_t *ci, int cache_enabled);
int dm2_cache_container_stat(const char *uri, dm2_cache_info_t *ci);
#if 1
int dm2_mgmt_is_disk_mounted(const char *devname, const char *mountpt,
			     int *mounted);
#endif
int dm2_clean_mount_points(const char *devname, const char *mountpt,
			   int umount_all, int *mounted);
int dm2_common_delete_uol(dm2_cache_type_t *ct, nkn_cod_t cod, int ptype,
			  int sub_ptype, char *uri,
			  dm2_cache_info_t **bad_ci_ret,
			  int delete_type);
void dm2_container_destroy(dm2_container_t  *free_cont, dm2_cache_type_t *ct);
int dm2_create_uri_head_wlocked(const char *uri, dm2_container_t *cont,
				dm2_cache_type_t *ct, int dblk, DM2_uri_t **uh);
void dm2_delete_container(dm2_container_t *cont, dm2_cache_type_t *ct,
			  ns_stat_token_t ns_token);

int dm2_delete_physid_extents(DM2_uri_t *uri_head, dm2_cache_type_t *ct,
			      ns_stat_token_t ns_stoken);
void dm2_delete_uri(DM2_uri_t *uh, dm2_cache_type_t *ct);
int dm2_dictionary_full(void);
int dm2_do_stampout_attr(const int afd, DM2_disk_attr_desc_t *dattr,
			 const dm2_container_t *cont, const off_t loff,
			 const char *basename, const int is_bad,
			 const int is_corrupt);
void dm2_dyninit_one_cache(dm2_mgmt_db_info_t *disk_info, int slot,
			   int *num_cache_types);
void dm2_spawn_tier_evict_thread(void);
void dm2_find_a_disk_slot(GList	*disk_slot_list, const char *serial_num,
			  int	*slot, int *disk_found);
int dm2_find_obj_in_cache(MM_put_data_t *putreq, char *put_uri,
			  dm2_cache_type_t *ct, DM2_uri_t **uri_ret,
			  dm2_cache_info_t **bad_ci_ret, int *found);
uint64_t dm2_find_raw_dev_size(const char *rawdev);
void dm2_free_disk_slot_list(GList *disk_slot_list);
void dm2_free_ext_list(GList *ext_list);
void dm2_free_uri_head(DM2_uri_t *uri_head);
uint32_t dm2_get_blocksize(const dm2_cache_info_t *ci);
uint32_t dm2_get_pagesize(const dm2_cache_info_t *ci);
dm2_cache_type_t *dm2_get_cache_type(int i);
void dm2_get_cache_type_resv_free_percent(dm2_cache_type_t *ct,
				     size_t *tot_free_ret,
				     size_t *tot_avail_ret);
int dm2_get_contlist(const char *uri, const int create_container,
		     dm2_cache_info_t *create_dev_ci, dm2_cache_type_t *ct,
		     int *valid_cont_found_ret,
		     dm2_container_head_t **conthead_ret,
		     dm2_container_t **cont_ret, int *cont_wlocked,
		     int *cont_head_locked);
void dm2_get_device_disk_slots(GList **disk_slot_list_ret);
void dm2_get_uri_del_list(dm2_cache_type_t *ct, DM2_uri_t *uri_head_locked,
			  int *occupancy, GList **uri_list_ret, int del_pin,
			  int *obj_pin);
void dm2_make_container_name(const char *mountpt, const char *uri_dir,
			     char *container_pathname);
int dm2_matching_physid(DM2_uri_t *uri_head);
int dm2_mgmt_activate_finish(dm2_mgmt_db_info_t *disk_info);
int dm2_mgmt_activate_start(dm2_cache_info_t *ci);
int dm2_mgmt_deactivate(dm2_cache_info_t *ci);
dm2_cache_info_t *dm2_mgmt_find_disk_cache(const char *mgmt_name,
					   uint32_t **ptype_ptr);
int dm2_mgmt_find_new_disks(void);
int dm2_mgmt_pre_format_diskcache(dm2_cache_info_t *ci, uint32_t *ptype_ptr,
				  int small_block, char *cmd);
int dm2_mgmt_post_format_diskcache(dm2_cache_info_t *ci);
int dm2_mgmt_cache_disable(dm2_cache_info_t *ci);
int dm2_mgmt_cache_enable(dm2_cache_info_t *ci, uint32_t *ptype_ptr);
void dm2_ct_reset_block_share_threshold(int ptype);
const char *dm2_mgmt_print_state(dm2_mgmt_disk_status_t *status);
int dm2_open_attrpath(char *attrpath);
int dm2_quick_uri_cont_stat(char *uri, dm2_cache_type_t *ct, int is_blocking);
int dm2_read_container_attrs(dm2_container_head_t *cont_head, int *ch_lock,
			     uint32_t preread, dm2_cache_type_t *ct);
int dm2_read_contlist(dm2_container_head_t *cont_head,
		      uint32_t preread, dm2_cache_type_t *ct);
void dm2_register_disk_counters(dm2_cache_info_t *ci, uint32_t *ptype);
int dm2_return_unused_pages(dm2_container_t *cont,
			    DM2_uri_t *uri_head);
void dm2_start_reading_caches(int num_cache_types);
void dm2_unlock_uri_del_list(dm2_cache_type_t *ct,
			     GList		 *uri_del_list);
void dm2_uri_head_queue_init(dm2_cache_info_t *ci);
DM2_uri_t *dm2_uri_head_pop_queue(dm2_cache_info_t *ci);
int dm2_tbl_init_hash_tables(dm2_cache_type_t *ct);
void dm2_mgmt_add_new_ct(dm2_mgmt_db_info_t *disk_info, int num_cache_types);
void dm2_mgmt_update_threads(dm2_mgmt_db_info_t *disk_info);
void DM2_unit_test_start(int num_threads, char *cached_filenames,
			 char *new_full_put_filenames,
			 char *new_partial_put_filenames,
			 int pattern __attribute((unused)));
void dm2_uri_log_state(DM2_uri_t *uri_head, int uri_op, int uri_flags,
		       int uri_errno, int tid, uint32_t uri_log_ts);

void dm2_uri_log_bump_idx(DM2_uri_t *uri_head);
void dm2_uri_log_dump(DM2_uri_t *uri_head);

int dm2_evict_init_buckets(dm2_cache_info_t *ci);
int dm2_uri_lock_pool_init(dm2_cache_info_t *ci, int);
void dm2_evict_add_uri(DM2_uri_t *uri_head, int hotness);
void dm2_evict_delete_uri(DM2_uri_t *uri_head, int hotness);
void dm2_evict_promote_uri(DM2_uri_t *uri_head, int old_val,int new_val);
int dm2_do_delete(MM_delete_resp_t *delete,
		  dm2_cache_type_t *ct,
                  DM2_uri_t        *uri_head,
		  dm2_cache_info_t **bad_ci_ret);

/* From diskmgr2_namespace.c */
int dm2_ns_resv_disk_usage(ns_stat_token_t     ns_token,
			   nkn_provider_type_t ptype,
			   int                 blocks);
int dm2_ns_calc_disk_used(ns_stat_token_t     ns_token,
			  nkn_provider_type_t ptype,
			  NCHashTable          *ns_physid_hash,
			  uint64_t            physid);
int dm2_ns_add_disk_usage(ns_stat_token_t     ns_token,
			  nkn_provider_type_t ptype,
			  int                 blocks);
int dm2_ns_sub_disk_usage(ns_stat_token_t     ns_token,
			  nkn_provider_type_t ptype,
			  int                 blocks,
			  int		      resv_only);
int dm2_ns_calc_cache_pin_usage(const ns_stat_token_t ns_token,
				const ns_stat_category_t cp_type,
				const nkn_provider_type_t ptype,
				const nkn_attr_t *attr);
int dm2_ns_calc_space_usage(const ns_stat_token_t  ns_token,
			    const ns_stat_category_t ns_stype,
			    const ns_stat_category_t ns_tier_stype,
			    const nkn_provider_type_t ptype,
			    size_t raw_length);
int dm2_ns_delete_calc_namespace_usage(const char *uri_dir,
				       const char *mgmt_name,
				       ns_stat_token_t	ns_token,
				       nkn_provider_type_t ptype,
				       int num_freed_disk_pgs,
				       int uri_pinned);
int dm2_ns_put_calc_space_resv(MM_put_data_t *put, char *put_uri,
			       nkn_provider_type_t ptype);
int dm2_ns_put_calc_space_actual_usage(MM_put_data_t *put,
				       char *put_uri,
				       DM2_uri_t *uri_head,
				       nkn_provider_type_t ptype);
int dm2_ns_update_total_objects(const ns_stat_token_t ns_token,
				const char *mgmt_name,
				dm2_cache_type_t *ct,
				int add_object_flag);
void dm2_ns_set_namespace_res_pools(const nkn_provider_type_t ptype);
void dm2_ns_set_tier_size_totals(dm2_cache_type_t *ct);
ns_stat_token_t dm2_ns_make_stat_token_from_uri_dir(const char *uri_dir);
int dm2_ns_rpindex_from_uri_dir(const char *uri_dir, int *rp_index);
ns_stat_token_t dm2_ns_get_stat_token_from_uri_dir(const char *uri_dir);
void dm2_inc_container_write_bytes(dm2_cache_type_t *ct,
				  dm2_cache_info_t *ci,
				  size_t	   nbytes);
void dm2_inc_attribute_write_bytes(dm2_cache_type_t *ct,
				  dm2_cache_info_t *ci,
				  size_t	   nbytes);
void dm2_inc_container_read_bytes(dm2_cache_type_t *ct,
				  dm2_cache_info_t *ci,
				  size_t	   nbytes);
void dm2_inc_attribute_read_bytes(dm2_cache_type_t *ct,
				  dm2_cache_info_t *ci,
				  size_t	   nbytes);
#endif /* NKN_DISKMGR2_LOCAL_H */
