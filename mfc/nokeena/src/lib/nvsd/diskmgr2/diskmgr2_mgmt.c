/*
 * (C) Copyright 2008 Nokeena Networks, Inc
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

#include "nkn_diskmgr2_api.h"
#include "nkn_diskmgr2_disk_api.h"
#include "nkn_diskmgr2_local.h"
#include "nkn_debug.h"
#include "nkn_errno.h"
#include "nkn_locmgr2_uri.h"
#include "nkn_locmgr2_container.h"
#include "nkn_stat.h"
#include "nvsd_mgmt_api.h"
#include "nkn_mediamgr_api.h"
#include "nkn_util.h"
#include "nkn_assert.h"
#include "diskmgr2_locking.h"
#include "diskmgr2_common.h"
#include "diskmgr2_util.h"
#include "nkn_nknexecd.h"

const char *dm2_mgmt_state_strings[DM2_MGMT_STATE_MAX+1] =
    {
	"DM2_MGMT_STATE_NULL",
	"DM2_MGMT_STATE_UNKNOWN",
	"DM2_MGMT_STATE_NOT_PRESENT",
	"DM2_MGMT_STATE_CACHEABLE",
	"DM2_MGMT_STATE_INVAL_FORMAT_BEFORE_MOUNT",
	"DM2_MGMT_STATE_COMMENTED_OUT",
	"DM2_MGMT_STATE_DEACTIVATED",
	"DM2_MGMT_STATE_ACTIVATED",	// found disk but no MFD content
	"DM2_MGMT_STATE_IMPROPER_UNMOUNT",
	"DM2_MGMT_STATE_IMPROPER_MOUNT",
	"DM2_MGMT_STATE_CACHE_RUNNING",
	"DM2_MGMT_STATE_BITMAP_BAD",
	"DM2_MGMT_STATE_FORMAT_UNKNOWN_AFTER_MOUNT",
	"DM2_MGMT_STATE_BITMAP_NOT_FOUND",
	"DM2_MGMT_STATE_CONVERT_FAILURE",
	"DM2_MGMT_STATE_WRONG_VERSION",
	"DM2_MGMT_STATE_CACHE_DISABLING",
    };

extern int64_t glob_dm2_num_caches,
    glob_dm2_uri_remove_disk_cnt,
    glob_dm2_free_uri_head_uri_name,
    glob_dm2_free_uri_head,
    glob_dm2_uri_head_remove_from_cache_err;

const char *
dm2_mgmt_print_state(dm2_mgmt_disk_status_t *status)
{
    if (status == NULL || status->ms_state > DM2_MGMT_STATE_MAX)
	return "Illegal State Value";
    return dm2_mgmt_state_strings[status->ms_state];
}	/* dm2_mgmt_print_state */


static void
dm2_free_bitmap(dm2_bitmap_t *bm,
		const char *mgmt_name)
{
    int i, ret, macro_ret;
    char buf[128];

    PTHREAD_MUTEX_LOCK(&bm->bm_mutex);
    /* Free the 'free block list' */
    g_list_free(bm->bm_free_blk_list);
    bm->bm_free_blk_list = NULL;

    /* Must delete before freeing buffer: set in dm2_register_disk_counters */
    nkn_mon_delete(DM2_FREE_PAGES_MON, mgmt_name);
    nkn_mon_delete(DM2_TOTAL_PAGES_MON, mgmt_name);

    nkn_mon_delete(DM2_FREE_BLOCKS_MON, mgmt_name);
    nkn_mon_delete(DM2_TOTAL_BLOCKS_MON, mgmt_name);
    nkn_mon_delete(DM2_FREE_RESV_BLOCKS_MON, mgmt_name);

    nkn_mon_delete(DM2_RAW_READ_CNT_MON, mgmt_name);
    nkn_mon_delete(DM2_RAW_READ_BYTES_MON, mgmt_name);
    nkn_mon_delete(DM2_RAW_WRITE_CNT_MON, mgmt_name);
    nkn_mon_delete(DM2_RAW_WRITE_BYTES_MON, mgmt_name);

    nkn_mon_delete(DM2_STATE_MON, mgmt_name);
    nkn_mon_delete(DM2_PROVIDER_TYPE_MON, mgmt_name);

    nkn_mon_delete(DM2_PUT_CNT_MON, mgmt_name);
    nkn_mon_delete(DM2_URI_HASH_CNT_MON, mgmt_name);

    nkn_mon_delete(DM2_NO_META_MON, mgmt_name);
    nkn_mon_delete(DM2_NO_META_ERR_MON, mgmt_name);
    nkn_mon_delete(DM2_FREE_META_BLKS_MON, mgmt_name);
    nkn_mon_delete(DM2_RM_ATTRFILE_CNT_MON, mgmt_name);
    nkn_mon_delete(DM2_RM_CONTFILE_CNT_MON, mgmt_name);

    nkn_mon_delete(DM2_EXT_EVICT_DELS_MON, mgmt_name);
    nkn_mon_delete(DM2_EXT_EVICT_FAILS_MON, mgmt_name);

    /* Set in dm2_bitmap_process */
    nkn_mon_delete(DM2_PAGE_SIZE_MON, mgmt_name);
    nkn_mon_delete(DM2_BLOCK_SIZE_MON, mgmt_name);
    nkn_mon_delete(DM2_URI_LOCK_IN_USE_CNT_MON, mgmt_name);
    nkn_mon_delete("dm2_ext_append_cnt", mgmt_name);

    for (i=0; i< DM2_MAX_EVICT_BUCKETS; i++) {
	snprintf(buf, 128, "dm2_evict_bkt_%d_entries", i);
	nkn_mon_delete(buf, mgmt_name);
    }

    dm2_free(bm->bm_buf, bm->bm_bufsize, DM2_NO_ALIGN);
    bm->bm_buf = NULL;

    dm2_free(bm->bm_free_blk_map, bm->bm_blk_map_size, DM2_NO_ALIGN);
    bm->bm_free_blk_map = NULL;

    if ((ret = nkn_close_fd(bm->bm_fd, DM2_FD)) < 0) {
	DBG_DM2S("Failed to close bitmap for cache: %s", mgmt_name);
	glob_dm2_bitmap_close_cnt++;
    }
    bm->bm_fd = 0;
    PTHREAD_MUTEX_UNLOCK(&bm->bm_mutex);
    PTHREAD_MUTEX_DESTROY(&bm->bm_mutex);
    memset(&bm->bm_mutex, 0, sizeof(bm->bm_mutex));
}	/* dm2_free_bitmap */


static int
dm2_remove_disk(dm2_cache_info_t *ci,
		dm2_cache_type_t *ct)
{
    int num_removed;

    /* This will remove all extents from physid_head hash table */
    num_removed = dm2_uri_remove_cache_info(ci, ct);
    DBG_DM2M("Number of URIs removed: %d", num_removed);
    glob_dm2_uri_remove_disk_cnt = num_removed;

    num_removed = dm2_conthead_remove_cache_info(ci, ct);
    DBG_DM2M("Number of containers removed: %d", num_removed);
    dm2_free_bitmap(&ci->bm, ci->mgmt_name);
    ci->cnt_registered = 0;
    /* Don't delete the uri hash table because we will re-use it */
    return 0;
}	/* dm2_remove_disk */


static int
dm2_mgmt_remove_disk(const char *mgmt_name)
{
    dm2_cache_type_t *ct;
    dm2_cache_info_t *ci = NULL;
    GList	     *ci_obj;
    int		     ct_index;
    int		     found = 0, macro_ret;

    for (ct_index = 0; ct_index < glob_dm2_num_cache_types; ct_index++) {
	ct = &g_cache2_types[ct_index];
	dm2_ct_info_list_rwlock_rlock(ct);	// noop
	for (ci_obj = ct->ct_info_list; ci_obj; ci_obj = ci_obj->next) {
	    ci = (dm2_cache_info_t *)ci_obj->data;
	    if (!strcasecmp(mgmt_name, ci->mgmt_name)) {
		found = 1;
		break;
	    }
	}
	dm2_ct_info_list_rwlock_runlock(ct);	// noop
	if (found)
	    break;
    }

    if (found) {
	/*
	 * Remove this disk from the list, so nothing else can access it while
	 * walking the list.  Another thread could already have a ptr to this
	 * object.
	 */
	NKN_ASSERT(ci);

	/* Mark the disk for disabling. Any ops that is using the disk would
	 * run to completion and the new ops would ignore this disk when they
	 * start. We would grab a write lock on the device and take it out
	 * of the active info list.
	 */
	ci->ci_disabling = 1;
	/* An intermediate state, will fall to 'CACHEABLE' on completion */
	ci->state_overall = DM2_MGMT_STATE_CACHE_DISABLING;

	/* Kill the eviction thread */
	PTHREAD_COND_BROADCAST(&ct->ct_dm2_evict_cond);

	dm2_ci_dev_rwlock_wlock(ci);
	dm2_ct_info_list_rwlock_wlock(ct);	// noop
	ct->ct_info_list = g_list_delete_link(ct->ct_info_list, ci_obj);
	ct->ct_del_info_list = g_list_prepend(ct->ct_del_info_list, ci);
	ct->ct_num_active_caches--;

	/* This disk is going away, reset preread releted flags */
	if (ci->ci_preread_done) {
	    AO_fetch_and_sub1(&ct->ct_num_caches_preread_done);
	    ci->ci_preread_done = 0;
	}

	if (ci->ci_preread_stat_opt) {
	    /* Subtract 'preread_success' count to match the
	     * num of active disk caches */
	    AO_fetch_and_sub1(&ct->ct_num_caches_preread_success);
	    ci->ci_preread_stat_opt = 0;
	}
	dm2_ct_info_list_rwlock_wunlock(ct);	// noop
	dm2_ci_dev_rwlock_wunlock(ci);

	dm2_remove_disk(ci, ct);
    }
    return 0;
}	/* dm2_mgmt_remove_disk */


static void
dm2_mgmt_move_ci_del_list_to_active_list(dm2_cache_info_t *move_ci)
{
    dm2_cache_type_t *ct;
    dm2_cache_info_t *ci = NULL;
    GList	     *ci_obj;
    int		     ct_index;
    int		     found = 0;

    for (ct_index = 0; ct_index < glob_dm2_num_cache_types; ct_index++) {
	ct = &g_cache2_types[ct_index];
	dm2_ct_info_list_rwlock_rlock(ct);
	for (ci_obj = ct->ct_del_info_list; ci_obj; ci_obj = ci_obj->next) {
	    ci = (dm2_cache_info_t *)ci_obj->data;
	    if (move_ci == ci) {
		found = 1;
		break;
	    }
	}
	dm2_ct_info_list_rwlock_runlock(ct);
	if (found)
	    break;
    }
    if (!found)
	return;

    /*
     * Put this disk back on the active list, so new requests can place
     * data there.
     */
    NKN_ASSERT(ci);
    dm2_ct_info_list_rwlock_wlock(ct);
    ct->ct_del_info_list = g_list_delete_link(ct->ct_del_info_list, ci_obj);
    ct->ct_info_list = g_list_prepend(ct->ct_info_list, ci);
    ct->ct_num_active_caches++;
    ci->ci_disabling = 0;
    for (ci_obj = ct->ct_info_list; ci_obj; ci_obj = ci_obj->next) {
	ci = (dm2_cache_info_t *)ci_obj->data;
	/* Stop eviction in other disks of the tier as the new disk may
	 * have colder objects and/or increase the tier capacity */
	if (ci->ci_evict_thread_state == EVICT_IN_PROGRESS) {
	    ci->ci_stop_eviction = 1;
	    ci->ci_evict_blks = 0;
	}
    }
    dm2_ct_info_list_rwlock_wunlock(ct);
}	/* dm2_mgmt_move_ci_del_list_to_active_list */


/* A 'status active' was just issued on this disk */
int
dm2_mgmt_activate_start(dm2_cache_info_t *ci)
{
    struct stat sb;
    char	freeblk_name[DM2_MAX_MOUNTPT];
    int64_t	rawdev_totsize;
    int		ret, mounted = 0;

    switch (ci->state_overall) {
	case DM2_MGMT_STATE_UNKNOWN:
	case DM2_MGMT_STATE_NOT_PRESENT:
	case DM2_MGMT_STATE_CACHEABLE:
	case DM2_MGMT_STATE_COMMENTED_OUT:
	case DM2_MGMT_STATE_CACHE_RUNNING:
	case DM2_MGMT_STATE_CACHE_DISABLING:
	    DBG_DM2W("[Cache disable] wrong state: %s => %d",
		     dm2_mgmt_state_strings[ci->state_overall],
		     NKN_DM2_INVALID_MGMT_REQUEST);
	    return -NKN_DM2_INVALID_MGMT_REQUEST;

	case DM2_MGMT_STATE_BITMAP_BAD:
	case DM2_MGMT_STATE_BITMAP_NOT_FOUND:
	case DM2_MGMT_STATE_WRONG_VERSION:
	    DBG_DM2W("[Cache disable] wrong state: %s => %d",
		     dm2_mgmt_state_strings[ci->state_overall],
		     NKN_DM2_MUST_CLEAR_ERROR);
	    return -NKN_DM2_MUST_CLEAR_ERROR;

	case DM2_MGMT_STATE_ACTIVATED:
	    break;

	case DM2_MGMT_STATE_DEACTIVATED:
	    // Allow some operations to be retried
	case DM2_MGMT_STATE_IMPROPER_UNMOUNT:
	case DM2_MGMT_STATE_IMPROPER_MOUNT:
	case DM2_MGMT_STATE_INVAL_FORMAT_BEFORE_MOUNT:
	case DM2_MGMT_STATE_FORMAT_UNKNOWN_AFTER_MOUNT:
	case DM2_MGMT_STATE_CONVERT_FAILURE:
	    /* Does device entry exist still? */
	    if (stat(ci->disk_name, &sb) < 0) {
		if (errno != ENOENT)
		    DBG_DM2S("Invalid error from stat(%s): %d => %d",
			     ci->disk_name, errno,
			     NKN_DM2_DISK_DEVICE_NOT_FOUND);
		NKN_ASSERT(errno == ENOENT);
		return -NKN_DM2_DISK_DEVICE_NOT_FOUND;
	    }
	    ci->state_overall = DM2_MGMT_STATE_ACTIVATED;

	    if (stat(ci->raw_devname, &sb) < 0) {
		if (errno != ENOENT)
		    DBG_DM2S("Invalid error from stat(%s): %d",
			     ci->raw_devname, errno);
		NKN_ASSERT(errno == ENOENT);
		/* This is OK.  We have a new drive but it is not formatted */
		ci->state_overall = DM2_MGMT_STATE_INVAL_FORMAT_BEFORE_MOUNT;
		return 0;
	    }
	    rawdev_totsize = dm2_find_raw_dev_size(ci->stat_name);
	    if (rawdev_totsize == 0) {
		/* This happens occassionally and is probably a linux problem.
		 * Most likely a reboot will clear it up. */
		DBG_DM2S("Finding raw device (%s) size failed",
			 ci->stat_name);
		ci->state_overall = DM2_MGMT_STATE_INVAL_FORMAT_BEFORE_MOUNT;
		return -NKN_DM2_DISK_ACTIVATE_FAILED;
	    }
	    /* At this point, the system size is the set size */
	    ci->set_cache_sec_cnt = rawdev_totsize;

	    if (stat(ci->fs_devname, &sb) < 0) {
		if (errno != ENOENT)
		    DBG_DM2S("Invalid error from stat(%s): %d",
			     ci->fs_devname, errno);
		NKN_ASSERT(errno == ENOENT);
		/* This is OK.  We have a new drive but it is not formatted */
		return 0;
	    }
	    ret = dm2_clean_mount_points(ci->fs_devname, ci->ci_mountpt, 0,
					 &mounted);
	    if (ret < 0) {
		ci->state_overall = DM2_MGMT_STATE_INVAL_FORMAT_BEFORE_MOUNT;
		/* A umount has failed.  Most likely only a reboot will clean
		 * up this issue.  Otherwise, we would need to pick a different
		 * mount point. */
		return -NKN_DM2_DISK_ACTIVATE_FAILED;
	    }
	    if (!mounted) {
		if ((ret = nkn_create_directory(ci->ci_mountpt, 1)) < 0) {
		    DBG_DM2S("Failed to create mount point: %s",
			     ci->ci_mountpt);
		    ci->state_overall =	DM2_MGMT_STATE_IMPROPER_MOUNT;
		    return -NKN_DM2_DISK_ACTIVATE_FAILED;
		}
		if (dm2_mount_fs(ci->fs_devname, ci->ci_mountpt) < 0) {
		    ci->state_overall =	DM2_MGMT_STATE_IMPROPER_MOUNT;
		    /* Must return 0, so admin can reformat if necessary */
		    return 0;
		}
		if ((ret = dm2_convert_disk(ci->ci_mountpt)) < 0) {
		    ci->state_overall = DM2_MGMT_STATE_CONVERT_FAILURE;
		    return 0;
		    /* Must return 0, so admin can reformat if necessary */
		}
	    }
	    snprintf(freeblk_name, DM2_MAX_MOUNTPT, "%s/%s",
		     ci->ci_mountpt, DM2_BITMAP_FNAME);
	    freeblk_name[DM2_MAX_MOUNTPT-1] = '\0';
	    if (stat(freeblk_name, &sb) < 0) {
		ci->state_overall = DM2_MGMT_STATE_FORMAT_UNKNOWN_AFTER_MOUNT;
		/* Must return 0, so admin can reformat if necessary */
		return 0;
	    }
	    ci->state_overall = DM2_MGMT_STATE_CACHEABLE;
	    break;

	default:
	    DBG_DM2S("cache (%s) in invalid state: %d",
		     ci->mgmt_name, ci->state_overall);
	    return -NKN_DM2_INVALID_MGMT_REQUEST;
    }
    return 0;
}	/* dm2_mgmt_activate_start */


static void
dm2_mark_removed_drives(GList *disk_slot_list)
{
    dm2_cache_type_t *ct;
    dm2_cache_info_t *ci;
    dm2_disk_slot_t  *ds;
    GList	     *ci_obj, *dsptr;
    int		     ct_index, found;

    for (ct_index=0; ct_index < glob_dm2_num_cache_types; ct_index++) {
	ct = &g_cache2_types[ct_index];
	dm2_ct_info_list_rwlock_rlock(ct);
	ci_obj = ct->ct_info_list;
	for (; ci_obj; ci_obj = ci_obj->next) {
	    ci = (dm2_cache_info_t *)ci_obj->data;
	    found = 0;
	    dsptr = g_list_first(disk_slot_list);
	    while (dsptr) {
		ds = (dm2_disk_slot_t *)dsptr->data;
		if (!strcasecmp(ci->serial_num, ds->ds_serialnum)) {
		    found = 1;
		    break;
		}
		dsptr = dsptr->next;
	    }
	    if (found)			// don't change state of found disks
		continue;
	    else {
		/* Not good.  Someone pulled drive w/o unmounting it cleanly */
		DBG_DM2S("Drive was pulled improperly: %s", ci->mgmt_name);
		DBG_DM2S("Proper instructions require a 'cache disable' "
			 "followed by 'status inactive' before removal");
		ci->state_overall = DM2_MGMT_STATE_NOT_PRESENT;
	    }
	}

	ci_obj = ct->ct_del_info_list;
	for (; ci_obj; ci_obj = ci_obj->next) {
	    ci = (dm2_cache_info_t *)ci_obj->data;
	    found = 0;
	    dsptr = g_list_first(disk_slot_list);
	    while (dsptr) {
		ds = (dm2_disk_slot_t *)dsptr->data;
		if (!strcasecmp(ci->serial_num, ds->ds_serialnum)) {
		    found = 1;
		    break;
		}
		dsptr = dsptr->next;
	    }
	    if (found)			// don't change state of found disks
		continue;
	    else {
		/* Normal case of drive removal */
		ci->state_overall = DM2_MGMT_STATE_NOT_PRESENT;
	    }
	}
	dm2_ct_info_list_rwlock_runlock(&g_cache2_types[ct_index]);
    }
}	/* dm2_mark_removed_drives */


/* Finish routine for 'mount new' */
int
dm2_mgmt_activate_finish(dm2_mgmt_db_info_t *disk_info)
{
    GList *disk_slot_list = NULL;
    int num_cache_types;
    int num_disks, slot, disk_found;

    DBG_DM2M("MOUNT NEW finish: [serial %s] [raw %s] [fs %s] [diskname %s] "
	     "[prov %s] [MP %s] [owner %s]", disk_info->mi_serial_num,
	     disk_info->mi_raw_partition, disk_info->mi_fs_partition,
	     disk_info->mi_diskname, disk_info->mi_provider,
	     disk_info->mi_mount_point, disk_info->mi_owner);

    num_cache_types = glob_dm2_num_cache_types;
    num_disks = glob_dm2_num_caches;
    slot = num_disks + 1;

    /* For now, we will only generate a disk_slot_list for VXA2100 systems */
    dm2_get_device_disk_slots(&disk_slot_list);
    dm2_find_a_disk_slot(disk_slot_list, disk_info->mi_serial_num, &slot,
			 &disk_found);
    if (disk_found == 0) {
	/* Should never happen */
	DBG_DM2S("Could not find disk slot: %s", disk_info->mi_serial_num);
	NKN_ASSERT(0);
	dm2_free_disk_slot_list(disk_slot_list);
	return -NKN_DM2_DISK_DEVICE_NOT_FOUND;
    }

    /* True for VXA2100 systems */
    if (disk_slot_list) {
	dm2_mark_removed_drives(disk_slot_list);
    }

    /*
     * For non-VXA2100 systems, slot will just increment.
     * For VXA2100 systems, slot will be derived from disk_slot_list
     */
    dm2_dyninit_one_cache(disk_info, slot, &num_cache_types);

    /* This can only happen for non-VXA2100 systems */
    if (disk_slot_list == NULL)
	glob_dm2_num_caches = slot;

    dm2_free_disk_slot_list(disk_slot_list);
    if (glob_dm2_num_cache_types != num_cache_types) {
	/* this disk is causing a new CT to appear, do the
	 * required registration for the upper layer to see this */
	glob_dm2_num_cache_types = num_cache_types;
	dm2_mgmt_add_new_ct(disk_info, num_cache_types);
    }
    return 0;
}	/* dm2_mgmt_activate_finish */


/* A 'status inactive' was just issued on this disk */
int
dm2_mgmt_deactivate(dm2_cache_info_t *ci)
{
    int ret, mounted = 0;

    switch (ci->state_overall) {
	case DM2_MGMT_STATE_UNKNOWN:
	case DM2_MGMT_STATE_NOT_PRESENT:
	case DM2_MGMT_STATE_COMMENTED_OUT:
	case DM2_MGMT_STATE_CACHE_DISABLING:
	    DBG_DM2W("[Cache disable] wrong state: %s => %d",
		     dm2_mgmt_state_strings[ci->state_overall],
		     NKN_DM2_INVALID_MGMT_REQUEST);
	    return -NKN_DM2_INVALID_MGMT_REQUEST;

	case DM2_MGMT_STATE_CACHE_RUNNING:
	    /* User must issue 'cache disable' before 'status inactive' */
	    DBG_DM2W("[Cache disable] wrong state: %s => %d",
		     dm2_mgmt_state_strings[ci->state_overall],
		     NKN_DM2_MUST_CACHE_DISABLE);
	    return -NKN_DM2_MUST_CACHE_DISABLE;

	case DM2_MGMT_STATE_DEACTIVATED:
	    /* nothing to do.  We are here already */
	    return 0;

	case DM2_MGMT_STATE_CACHEABLE:
	case DM2_MGMT_STATE_INVAL_FORMAT_BEFORE_MOUNT:
	case DM2_MGMT_STATE_ACTIVATED:
	case DM2_MGMT_STATE_IMPROPER_UNMOUNT:
	case DM2_MGMT_STATE_IMPROPER_MOUNT:
	case DM2_MGMT_STATE_BITMAP_BAD:
	case DM2_MGMT_STATE_FORMAT_UNKNOWN_AFTER_MOUNT:
	case DM2_MGMT_STATE_BITMAP_NOT_FOUND:
	case DM2_MGMT_STATE_CONVERT_FAILURE:
	case DM2_MGMT_STATE_WRONG_VERSION:
	    ret = dm2_clean_mount_points(ci->fs_devname, ci->ci_mountpt, 1,
					 &mounted);
	    if (ret < 0) {
		ci->state_overall = DM2_MGMT_STATE_IMPROPER_UNMOUNT;
		return -NKN_DM2_DISK_DEACTIVATE_FAILED;
	    }
	    ci->state_overall = DM2_MGMT_STATE_DEACTIVATED;
	    break;

	default:
	    DBG_DM2S("cache (%s) in invalid state: %d",
		     ci->mgmt_name, ci->state_overall);
	    return -NKN_DM2_INVALID_MGMT_REQUEST;
    }
    return 0;
}	/* dm2_mgmt_deactivate */


#if 0
static int
dm2_not_empty(dm2_cache_info_t *ci)
{
    DIR *dirp;
    struct dirent *ent;
    int not_empty = 0;
    struct stat sb;

    /* Hack to prevent this check */
    if (stat("/config/nkn/allow_cache_enable_with_data", &sb) == 0)
	return 0;

    if ((dirp = opendir(ci->ci_mountpt)) == NULL) {
	DBG_DM2W("[mgmt=%s] Open failed for %s: %d",
		 ci->mgmt_name, ci->ci_mountpt, errno);
	return -errno;
    }
    while ((ent = readdir(dirp)) != NULL) {
	if (!strcmp(ent->d_name, "."))
	    continue;
	if (!strcmp(ent->d_name, ".."))
	    continue;
	if (!strcmp(ent->d_name, "freeblks"))
	    continue;
	if (!strcmp(ent->d_name, "lost+found"))
	    continue;
	not_empty = 1;
	break;
    }
    closedir(dirp);
    return not_empty;
}	/* dm2_not_empty */
#endif


int
dm2_mgmt_cache_enable(dm2_cache_info_t	*ci,
		      uint32_t		*ptype_ptr)
{
    dm2_cache_type_t *ct = NULL;
    int		     block_size, ret = 0;

    switch (ci->state_overall) {
	case DM2_MGMT_STATE_UNKNOWN:
	case DM2_MGMT_STATE_NOT_PRESENT:
	case DM2_MGMT_STATE_INVAL_FORMAT_BEFORE_MOUNT:
	case DM2_MGMT_STATE_COMMENTED_OUT:
	case DM2_MGMT_STATE_CACHE_DISABLING:
	    DBG_DM2W("[Cache enable] wrong state: %s => %d",
		     dm2_mgmt_state_strings[ci->state_overall],
		     NKN_DM2_INVALID_MGMT_REQUEST);
	    return -NKN_DM2_INVALID_MGMT_REQUEST;

	case DM2_MGMT_STATE_IMPROPER_UNMOUNT:
	case DM2_MGMT_STATE_IMPROPER_MOUNT:
	case DM2_MGMT_STATE_BITMAP_BAD:
	case DM2_MGMT_STATE_FORMAT_UNKNOWN_AFTER_MOUNT:
	case DM2_MGMT_STATE_BITMAP_NOT_FOUND:
	case DM2_MGMT_STATE_CONVERT_FAILURE:
	case DM2_MGMT_STATE_WRONG_VERSION:
	    DBG_DM2W("[Cache enable] wrong state: %s => %d",
		     dm2_mgmt_state_strings[ci->state_overall],
		     NKN_DM2_INVALID_MGMT_REQUEST);
	    return -NKN_DM2_MUST_CLEAR_ERROR;

	case DM2_MGMT_STATE_DEACTIVATED:
	    /* User must issue 'status active' before 'cache enable' */
	    DBG_DM2W("[Cache enable] wrong state: %s => %d",
		     dm2_mgmt_state_strings[ci->state_overall],
		     NKN_DM2_MUST_STATUS_ACTIVE);
	    return -NKN_DM2_MUST_STATUS_ACTIVE;

	case DM2_MGMT_STATE_CACHE_RUNNING:
	    /* Do nothing */
	    break;

	case DM2_MGMT_STATE_CACHEABLE:
	case DM2_MGMT_STATE_ACTIVATED:
#if 0
	    if (dm2_not_empty(ci))
		return -NKN_DM2_MUST_FORMAT;
#endif
	    DBG_DM2A("Cache enable: %s start", ci->mgmt_name);
	    ret = dm2_bitmap_process(ci, 1);
	    if (ret == 0) {
		ci->state_overall = DM2_MGMT_STATE_CACHE_RUNNING;
		ct = ci->ci_cttype;
		dm2_ct_rwlock_wlock(ct);
		dm2_mgmt_move_ci_del_list_to_active_list(ci);
		if (g_list_length(ct->ct_info_list) == 1)
		    dm2_update_lowest_tier();
		dm2_register_disk_counters(ci, ptype_ptr);

		/* We can no longer make this stat optimization */
		ct->ct_tier_preread_stat_opt = 0;
		ci->ci_preread_stat_opt = 0;
		ci->ci_preread_errno = 0;
		ci->ci_preread_stat_cnt = 0;

		/* Reset 'prered_done' for this tier */
		ct->ct_tier_preread_done = 0;
		/* Reset 'prere_opt' and force preread to issue STATs for all
		 * the containers in a disk without optimizing for duplicate
		 * stats */
		ct->ct_dm2_preread_opt = 0;
		dm2_set_glob_preread_state(ct->ct_ptype, 0);

		ct->ct_dm2_last_ci_enable_ts = nkn_cur_ts;
		ci->ci_dm2_last_decay_ts = 0;
		if (g_list_length(ct->ct_info_list) == 1)
		    MM_set_provider_state(ct->ct_ptype, 0,
					  MM_PROVIDER_STATE_ACTIVE);
		dm2_ct_rwlock_wunlock(ct);

		/* If the disk is formatted using a 256K/4K layout,
		 * disable block sharing */
		block_size = dm2_get_blocksize(ci);
		if (block_size == DM2_DISK_BLKSIZE_256K) {
		   dm2_ct_reset_block_share_threshold(ct->ct_ptype);
		   ct->ct_share_block_threshold = 0;
		}

		/* Kick off preread for the new incoming disk */
		pthread_create(&ci->ci_preread_thread, NULL,
			       dm2_preread_main_thread, (void *)ci);

		DBG_DM2S("[cache_type=%s] Starting evict thread for disk %s",
			 ct->ct_name, ci->mgmt_name);
		/* Start the disk eviction thread */
		pthread_create(&ci->ci_evict_thread, NULL,
			    dm2_evict_ci_fn, ci);
		DBG_DM2A("Cache enable: %s complete", ci->mgmt_name);
	    } else {
		DBG_DM2A("Cache enable: %s failed (err:%d)",ci->mgmt_name, ret);
	    }
	    break;

	default:
	    DBG_DM2S("cache (%s) in invalid state: %d",
		     ci->mgmt_name, ci->state_overall);
	    return -NKN_DM2_INVALID_MGMT_REQUEST;
    }
    return ret;
}	/* dm2_mgmt_cache_enable */

/* Delete this code when real function is written */

extern void nvsd_disk_mgmt_offline(char *serial_num);
void nvsd_disk_mgmt_offline(char *serial_num)
{
    DBG_DM2S("%s", serial_num);
    return;
}

int
dm2_mgmt_cache_disable(dm2_cache_info_t	*ci)
{
    int	ret = 0;
    dm2_cache_type_t *ct;

    DBG_DM2S("Cache disable: %s", ci->mgmt_name);
    switch (ci->state_overall) {
	case DM2_MGMT_STATE_UNKNOWN:
	case DM2_MGMT_STATE_NOT_PRESENT:
	case DM2_MGMT_STATE_INVAL_FORMAT_BEFORE_MOUNT:
	case DM2_MGMT_STATE_COMMENTED_OUT:
	case DM2_MGMT_STATE_DEACTIVATED:
	case DM2_MGMT_STATE_IMPROPER_UNMOUNT:
	case DM2_MGMT_STATE_IMPROPER_MOUNT:
	case DM2_MGMT_STATE_BITMAP_BAD:
	case DM2_MGMT_STATE_FORMAT_UNKNOWN_AFTER_MOUNT:
	case DM2_MGMT_STATE_BITMAP_NOT_FOUND:
	case DM2_MGMT_STATE_CONVERT_FAILURE:
	case DM2_MGMT_STATE_WRONG_VERSION:
	case DM2_MGMT_STATE_CACHE_DISABLING:
	    DBG_DM2W("[Cache disable] wrong state: %s => %d",
		     dm2_mgmt_state_strings[ci->state_overall],
		     NKN_DM2_INVALID_MGMT_REQUEST);
	    return -NKN_DM2_INVALID_MGMT_REQUEST;

	case DM2_MGMT_STATE_ACTIVATED:
	case DM2_MGMT_STATE_CACHEABLE:
	    /* Do nothing. I'm already in this state */
	    break;

	case DM2_MGMT_STATE_CACHE_RUNNING:
	    /* Does device entry exist still? */
	    ct = ci->ci_cttype;
	    DBG_DM2A("Cache disable: %s start", ci->mgmt_name);
	    ct->ct_dm2_cache_disabling = 1;
	    dm2_ct_rwlock_wlock(ct);
	    ret = dm2_mgmt_remove_disk(ci->mgmt_name);
	    ci->state_overall = DM2_MGMT_STATE_CACHEABLE;
	    /* Next time the disk is enabled, it needs to be read again */
	    ci->pr_processed = 0;
	    ct->ct_dm2_cache_disabling = 0;
	    dm2_ct_rwlock_wunlock(ct);

	    /* Wait till the preread threads exit before marking the drive
	     * cacheable. If a cache enable comes in before the preread
	     * thread exits, we will have multiple preread threads running */
	    while (ci->ci_preread_thread_active == 1)
		sleep(1);

	    DBG_DM2A("Cache disable: %s complete", ci->mgmt_name);
	    nvsd_disk_mgmt_offline(ci->serial_num);
	    /* When this returns this cache is no longer on the list */
	    if (ct->ct_info_list == NULL) {
		MM_set_provider_state(ct->ct_ptype, 0,
				      MM_PROVIDER_STATE_INACTIVE);
		dm2_update_lowest_tier();
	    }
	    break;

	default:
	    DBG_DM2S("cache (%s) in invalid state: %d",
		     ci->mgmt_name, ci->state_overall);
	    return -NKN_DM2_INVALID_MGMT_REQUEST;
    }
    return ret;
}	/* dm2_mgmt_cache_disable */


int
dm2_mgmt_find_new_disks(void)
{
    char		cmd[NKNEXECD_MAXCMD_LEN];
    char		basename_buf[NKNEXECD_MAXBASENAME_LEN];
    nknexecd_retval_t	retval;

    snprintf(cmd, NKNEXECD_MAXCMD_LEN, "/opt/nkn/bin/write_diskinfo_to_db.sh"
	     " newdisks no-save-config");
    memset(&retval, 0, sizeof(retval));
    strcpy(basename_buf, "write_diskinfo_to_db");
    nknexecd_run(cmd, basename_buf, &retval);
    if (retval.retval_reply_code != 0) {
	DBG_DM2S("ERROR: Finding new disks failed");
	/* No of lines to move from stderr to syslog file */
        nkn_print_file_to_syslog(retval.retval_stderrfile, 10);
        return retval.retval_reply_code;
    }
    return 0;
}	/* dm2_mgmt_find_new_disks */

int
dm2_mgmt_pre_format_diskcache(dm2_cache_info_t	*ci,
			      uint32_t		*ptype_ptr,
			      int		small_block,
			      char		*cmd)
{
    int		len, ret, mounted = 0;
    int		block_size = (DM2_DISK_BLKSIZE_2M / 1024);
    int		page_size = (DM2_DISK_PAGESIZE_32K / 1024);

    if (cmd == NULL) {
	DBG_DM2S("NULL cmd buffer");
	return -EINVAL;
    }

    switch (ci->state_overall) {
	case DM2_MGMT_STATE_UNKNOWN:
	case DM2_MGMT_STATE_NOT_PRESENT:
	case DM2_MGMT_STATE_COMMENTED_OUT:
	case DM2_MGMT_STATE_DEACTIVATED:
	case DM2_MGMT_STATE_IMPROPER_UNMOUNT:
	case DM2_MGMT_STATE_CACHE_DISABLING:
	    DBG_DM2W("[Pre-format] wrong state: %s => %d",
		     dm2_mgmt_state_strings[ci->state_overall],
		     NKN_DM2_INVALID_MGMT_REQUEST);
	    return -NKN_DM2_INVALID_MGMT_REQUEST;

	    /* nothing to do.  We are here already */
	    return 0;

	case DM2_MGMT_STATE_CACHE_RUNNING:
	    /* User must issue 'cache disable' before reformatting */
	    DBG_DM2W("[Pre-format] wrong state: %s => %d",
		     dm2_mgmt_state_strings[ci->state_overall],
		     NKN_DM2_MUST_CACHE_DISABLE);
	    return -NKN_DM2_MUST_CACHE_DISABLE;

	case DM2_MGMT_STATE_CACHEABLE:
	case DM2_MGMT_STATE_INVAL_FORMAT_BEFORE_MOUNT:
	case DM2_MGMT_STATE_ACTIVATED:
	case DM2_MGMT_STATE_IMPROPER_MOUNT:
	case DM2_MGMT_STATE_BITMAP_BAD:
	case DM2_MGMT_STATE_FORMAT_UNKNOWN_AFTER_MOUNT:
	case DM2_MGMT_STATE_BITMAP_NOT_FOUND:
	case DM2_MGMT_STATE_CONVERT_FAILURE:
	case DM2_MGMT_STATE_WRONG_VERSION:
	    ret = dm2_clean_mount_points(ci->fs_devname, ci->ci_mountpt, 1,
					 &mounted);
	    if (ret < 0) {
		ci->state_overall = DM2_MGMT_STATE_IMPROPER_UNMOUNT;
		return -NKN_DM2_FORMAT_FAILED;
	    }
	    ci->state_overall = DM2_MGMT_STATE_ACTIVATED;

	    /* SAS/SATA drive can use smaller blocks for T-proxy sites */
	    if (*ptype_ptr == SolidStateMgr_provider ||	small_block != 0) {
		block_size = DM2_DISK_BLKSIZE_256K / 1024;
		page_size = DM2_DISK_PAGESIZE_4K / 1024;
	    } else {
		block_size = DM2_DISK_BLKSIZE_2M / 1024;
		page_size = DM2_DISK_PAGESIZE_32K / 1024;
	    }

	    len = strlen(ci->fs_devname);
	    if (ci->fs_devname[len-1] == '2' &&	islower(ci->fs_devname[len-2])) {
		/* Non-root disk cache uses name like "/dev/sdb2" */
		ret = snprintf(cmd, MAX_FORMAT_CMD_LEN/2, "%s %s %d %d 0",
			       DM2_INITCACHE_CMD, ci->disk_name,
			       block_size, page_size);
	    } else {
		/* root disk cache uses name like "/dev/sda12" */
		ret = snprintf(cmd, MAX_FORMAT_CMD_LEN/2, "%s %s %s %d %d 0",
			       DM2_INITROOTCACHE_CMD, ci->raw_devname,
			       ci->fs_devname, block_size, page_size);
	    }

	    if (ret < 0 || ret >= MAX_FORMAT_CMD_LEN/2) {
		NKN_ASSERT(0);
		return -NKN_DM2_FORMAT_FAILED;
	    }
	    break;
	default:
	    DBG_DM2S("cache (%s) in invalid state: %d",
		     ci->mgmt_name, ci->state_overall);
	    return -NKN_DM2_INVALID_MGMT_REQUEST;
	}
    return 0;
}	/* dm2_mgmt_pre_format_diskcache */


int
dm2_mgmt_post_format_diskcache(dm2_cache_info_t *ci)
{
    char	freeblk_name[DM2_MAX_MOUNTPT];
    struct stat sb;
    int		ret, mounted = 0;

    switch (ci->state_overall) {
	case DM2_MGMT_STATE_UNKNOWN:
	case DM2_MGMT_STATE_NOT_PRESENT:
	case DM2_MGMT_STATE_COMMENTED_OUT:
	case DM2_MGMT_STATE_DEACTIVATED:
	case DM2_MGMT_STATE_IMPROPER_UNMOUNT:
	case DM2_MGMT_STATE_CACHE_RUNNING:
	case DM2_MGMT_STATE_CACHE_DISABLING:
	    DBG_DM2W("[Post-format] wrong state: %s => %d",
		     dm2_mgmt_state_strings[ci->state_overall],
		     NKN_DM2_INVALID_MGMT_REQUEST);
	    return -NKN_DM2_INVALID_MGMT_REQUEST;

	    /* nothing to do.  We are here already */
	    return 0;

	case DM2_MGMT_STATE_ACTIVATED:
	    ret = dm2_mgmt_is_disk_mounted(ci->fs_devname, ci->ci_mountpt,
					   &mounted);
            if (ret < 0) {
                ci->state_overall = DM2_MGMT_STATE_IMPROPER_UNMOUNT;
                return -NKN_DM2_FORMAT_FAILED;
            }

	    if (!mounted) {
		if (dm2_mount_fs(ci->fs_devname, ci->ci_mountpt) < 0) {
		    ci->state_overall =
			DM2_MGMT_STATE_FORMAT_UNKNOWN_AFTER_MOUNT;
		    return -NKN_DM2_FORMAT_FAILED;
		}
	    }
	    snprintf(freeblk_name, DM2_MAX_MOUNTPT, "%s/%s",
		     ci->ci_mountpt, DM2_BITMAP_FNAME);
	    freeblk_name[DM2_MAX_MOUNTPT-1] = '\0';
	    if (stat(freeblk_name, &sb) < 0) {
		ci->state_overall = DM2_MGMT_STATE_FORMAT_UNKNOWN_AFTER_MOUNT;
		return -NKN_DM2_FORMAT_FAILED;
	    }
	    /*
  	     * The cache counter must be updated with the raw disk size after
  	     * every format 
  	     */
	    ci->set_cache_sec_cnt = dm2_find_raw_dev_size(ci->stat_name);
	    ci->state_overall = DM2_MGMT_STATE_CACHEABLE;
	    dm2_cache_log_disk_format(ci->mgmt_name, ci->ci_cttype);
	    DBG_DM2A("ALARM: Format complete, disk cacheable: %s",
		     ci->mgmt_name);
	    break;

	default:
	    DBG_DM2S("cache (%s) in invalid state: %d",
		     ci->mgmt_name, ci->state_overall);
	    return -NKN_DM2_INVALID_MGMT_REQUEST;
    }
    return 0;
}	/* dm2_mgmt_post_format_diskcache */
