/*
 * (C) Copyright 2008-9 Ankeena Networks, Inc
 *
 * This file contains code which implements the Disk Manager
 *
 * Author: Michael Nishimoto / Srihari MS
 *
 * Non-obvious coding conventions:
 *	- All functions should begin with dm2_
 *	- All functions have a name comment at the end of the function.
 *	- All log messages use special logging macros
 *
 */
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <mntent.h>
#include <glib.h>

#include "nkn_diskmgr2_local.h"
#include "nkn_diskmgr2_disk_api.h"
#include "diskmgr2_common.h"
#include "diskmgr2_locking.h"
#include "diskmgr2_util.h"
#include "nkn_locmgr2_uri.h"
#include "smart_attr.h"
#include "nvsd_mgmt_api.h"
#include "nkn_cfg_params.h"
#include "nkn_assert.h"
#include "nkncnt_client.h"

#define PUT_THR_PER_DISK 1
#define GET_THR_PER_DISK 4

extern int64_t	glob_dm2_num_caches,
		glob_dm2_raw_open_cnt,
		glob_dm2_raw_open_err,
		glob_dm2_raw_close_cnt,
		dm2_ram_drive_only,
		glob_dm2_attr_update_log_hotness,
		g_dm2_unit_test,
		glob_dm2_sata_tier_avl,
		glob_dm2_sas_tier_avl,
		glob_dm2_ssd_tier_avl;

extern nkncnt_client_ctx_t dm2_smart_ctx;

typedef struct dm2_ptype_mapper_s {
    nkn_provider_type_t ptype;
    int                 array_idx;
} dm2_ptype_mapper_t;

GList *ptype_mapper_list = NULL;

int
dm2_cache_array_map_ret_idx(nkn_provider_type_t ptype)
{
    GList               *plist;
    dm2_ptype_mapper_t  *pmap;

    plist = ptype_mapper_list;
    while (plist) {
        pmap = (dm2_ptype_mapper_t *) plist->data;
        if (pmap == NULL)
            continue;
        if (pmap->ptype == ptype)
            return pmap->array_idx;
        plist = plist->next;
    }

    return -1;
}       /* dm2_cache_array_map_ret_idx */

static int
dm2_print_num_caches(const int num_cache_types)
{
    dm2_cache_type_t    *cache_type;
    int                 i;
    int                 num_caches = 0;

    g_static_mutex_lock(&dm2_cache2_types_mutex);
    for (i = 0; i < num_cache_types; i++) {
        cache_type = &g_cache2_types[i];
        DBG_DM2E("Number of %s caches found: %ld",
                 cache_type->ct_name, cache_type->ct_num);
        num_caches += cache_type->ct_num;
    }
    g_static_mutex_unlock(&dm2_cache2_types_mutex);
    return num_caches;
}       /* dm2_print_num_caches */

static void
dm2_bitmap_set_init_state(dm2_cache_info_t *ci,
                          int              *ret)
{
    if (*ret == -NKN_DM2_DISK_ADMIN_ACTION) {
        DBG_DM2S("Bitmap for cache (%s) was not read: Skipping cache due "
                 "to admin action.", ci->mgmt_name);
        ci->state_overall = DM2_MGMT_STATE_CACHEABLE;
        *ret = 0;
    } else if (*ret == -NKN_DM2_BITMAP_NOT_FOUND) {
        DBG_DM2S("Bitmap for cache (%s) was not found: Skipping cache due "
                 "to internal error", ci->mgmt_name);
        /* Do nothing, state should remain "ACTIVATED" */
    } else if (*ret == -NKN_DM2_WRONG_CACHE_VERSION) {
        DBG_DM2S("cache (%s) has wrong version: Skipping cache due "
                 "to internal error", ci->mgmt_name);
        ci->state_overall = DM2_MGMT_STATE_WRONG_VERSION;
    } else if (*ret == -EIO) {
        DBG_DM2S("Bitmap for cache (%s) could not be read: Skipping cache.",
                 ci->mgmt_name);
        ci->state_overall = DM2_MGMT_STATE_BITMAP_BAD;
    } else {
        DBG_DM2S("Bitmap for cache (%s) was not read: Skipping cache.",
                 ci->mgmt_name);
        ci->state_overall = DM2_MGMT_STATE_FORMAT_UNKNOWN_AFTER_MOUNT;
    }
}       /* dm2_bitmap_set_init_state */

static void
dm2_cache_array_map_insert_idx(nkn_provider_type_t ptype,
                               int                 in_idx)
{
    int                idx;
    dm2_ptype_mapper_t *pmap;

    idx = dm2_cache_array_map_ret_idx(ptype);
    if (idx >= 0) {
        /* Already inserted */
        return;
    }

    pmap = dm2_calloc(1, sizeof(dm2_ptype_mapper_t), mod_dm2_ptype_mapper_t);
    if (!pmap) {
        DBG_DM2S("Calloc failed: %ld", sizeof(dm2_ptype_mapper_t));
        return;
    }
    pmap->ptype = ptype;
    pmap->array_idx = in_idx;

    ptype_mapper_list = g_list_prepend(ptype_mapper_list, pmap);
    return;
}       /* dm2_cache_array_map_insert_idx */

static void
dm2_set_glob_tier_avl(nkn_provider_type_t ptype)
{
  switch (ptype) {
        case SAS2DiskMgr_provider:
            glob_dm2_sas_tier_avl = 1;
            break;
        case SATADiskMgr_provider:
            glob_dm2_sata_tier_avl = 1;
            break;
        case SolidStateMgr_provider:
            glob_dm2_ssd_tier_avl = 1;
            break;
        default:
        break;
    }
}   /* dm2_set_glob_tier_avl */

static void
dm2_insert_cache_list(const char		*mountpt,
		      const size_t		rawdev_totsize,
		      dm2_mgmt_db_info_t	*disk_info,
		      const dm2_mgmt_state_t	dstate,
		      int			*num_cache_types)
{
    char	     mutex_name[40];
    dm2_cache_type_t *cache_type;
    dm2_cache_info_t *cache_info;
    char	     *drive_type = disk_info->mi_provider;
    char	     *rawdev = disk_info->mi_raw_partition;
    char	     *slash;
    int		     add_type = 0, unknown_type = 0;
    int		     len, ret, macro_ret;

    if (strlen(drive_type) > DM2_MAX_CACHE_TYPE_LEN-1) {
	DBG_DM2S("drive_type too long(%zd): %s",
		 strlen(drive_type), drive_type);
	DBG_DM2S("Cache device skipped: %s", rawdev);
	return;
    }

    if (!strcmp(drive_type, DM2_TYPE_UNKNOWN)) {
	drive_type = (char *)DM2_TYPE_SATA;
	unknown_type = 1;
    }

    g_static_mutex_lock(&dm2_cache2_types_mutex);
    cache_type = dm2_find_cache_list(drive_type, *num_cache_types);
    if (cache_type == NULL) {
	cache_type = &g_cache2_types[*num_cache_types];
	dm2_ct_rwlock_init(cache_type);
	dm2_ct_info_list_rwlock_init(cache_type);
	snprintf(mutex_name, 40, "%s.dm2_ct_stat_mutex", drive_type);;
	NKN_MUTEX_INITR(&cache_type->ct_stat_mutex, NULL, mutex_name);
	snprintf(mutex_name, 40, "%s.dm2_ct_disk_select_mutex", drive_type);;
	NKN_MUTEX_INITR(&cache_type->ct_dm2_select_disk_mutex, NULL,mutex_name);
	snprintf(mutex_name, 40, "%s.dm2_evict_cond_mutex", drive_type);;
	NKN_MUTEX_INITR(&cache_type->ct_dm2_evict_cond_mutex, NULL, mutex_name);
	PTHREAD_COND_INIT(&cache_type->ct_dm2_evict_cond, NULL);
	cache_type->ct_subptype = *num_cache_types;	// starts with 0
	strncpy(cache_type->ct_name, drive_type, DM2_MAX_CACHE_TYPE_LEN);
	cache_type->ct_name[DM2_MAX_CACHE_TYPE_LEN-1] = '\0';
	dm2_ct_uri_hash_rwlock_init(cache_type);
	cache_type->ct_tier_preread_stat_opt = 0;
	cache_type->ct_dm2_decay_interval = 3600*4;
	cache_type->ct_num_evict_disk_prcnt = 100;

	if (dm2_tbl_init_hash_tables(cache_type) < 0) {
	    DBG_DM2S("GLIB memory allocation failed: %s", rawdev);
	    return;
	}

	/* XXXmiken: HACK! */
	if (!strcmp(cache_type->ct_name, DM2_TYPE_SAS)) {
	    dm2_cache_array_map_insert_idx(SAS2DiskMgr_provider,
					   *num_cache_types);
	    cache_type->ct_weighting = 60;
	    cache_type->ct_ptype = SAS2DiskMgr_provider;
	    cache_type->ct_rp_type = RESOURCE_POOL_TYPE_UTIL_SAS;
	} else if (!strcmp(cache_type->ct_name, DM2_TYPE_SATA)) {
	    dm2_cache_array_map_insert_idx(SATADiskMgr_provider,
					   *num_cache_types);
	    cache_type->ct_weighting = 40;
	    cache_type->ct_ptype = SATADiskMgr_provider;
	    cache_type->ct_rp_type = RESOURCE_POOL_TYPE_UTIL_SATA;
	} else if (!strcmp(cache_type->ct_name, DM2_TYPE_SSD)) {
	    dm2_cache_array_map_insert_idx(SolidStateMgr_provider,
					   *num_cache_types);
	    cache_type->ct_weighting = 200;
	    cache_type->ct_ptype = SolidStateMgr_provider;
	    cache_type->ct_rp_type = RESOURCE_POOL_TYPE_UTIL_SSD;
	}
	dm2_set_glob_tier_avl(cache_type->ct_ptype);
	add_type++;
    }

    /* Setup cache info */
    cache_info = dm2_calloc(1, sizeof(dm2_cache_info_t), mod_dm2_cache_info_t);
    if (cache_info == NULL) {
	DBG_DM2S("Calloc failed: cache on %s not added", rawdev);
	goto unlock_type;
	return;
    }

    len = sizeof(cache_info->ci_mountpt);
    strncpy(cache_info->ci_mountpt, mountpt, len);
    cache_info->ci_mountpt[len-1] = '\0';
    len = sizeof(cache_info->raw_devname);
    strncpy(cache_info->raw_devname, rawdev, len);
    cache_info->raw_devname[len-1] = '\0';
    len = sizeof(cache_info->fs_devname);
    strncpy(cache_info->fs_devname, disk_info->mi_fs_partition, len);
    cache_info->fs_devname[len-1] = '\0';
    len = sizeof(cache_info->disk_name);
    strncpy(cache_info->disk_name, disk_info->mi_diskname, len);
    cache_info->disk_name[len-1] = '\0';
    len = sizeof(cache_info->stat_name);
    strncpy(cache_info->stat_name, disk_info->mi_statname, len);
    cache_info->stat_name[len-1] = '\0';
    len = sizeof(cache_info->serial_num);
    strncpy(cache_info->serial_num, disk_info->mi_serial_num, len);
    cache_info->serial_num[len-1] = '\0';
    len = strlen(cache_info->serial_num);
    if (cache_info->serial_num[len-1] == '@' &&
	cache_info->serial_num[len-2] == '*') {
	/* This indicates that the name is not a real serial number */
	cache_info->state_not_production = 1;
    }
    cache_info->set_cache_sec_cnt =
	(disk_info->mi_set_sector_cnt > rawdev_totsize) ?
	rawdev_totsize : disk_info->mi_set_sector_cnt;
    cache_info->valid_cache = 0;
    cache_info->list_index = cache_type->ct_num;
    cache_info->ci_cttype = cache_type;

    /* Grab management name */
    slash = strrchr(mountpt, '/');
    len = sizeof(cache_info->mgmt_name);
    strncpy(cache_info->mgmt_name, slash+1, len);
    cache_info->mgmt_name[len-1] = '\0';

    cache_info->state_cache_enabled = disk_info->mi_cache_enabled;
    cache_info->state_activated = disk_info->mi_activated;
    cache_info->state_missing = disk_info->mi_missing_after_boot;
    cache_info->state_commented_out = disk_info->mi_commented_out;

    if (dstate != DM2_MGMT_STATE_NULL)
	cache_info->state_overall = dstate;
    else
	cache_info->state_overall = DM2_MGMT_STATE_ACTIVATED;

    /* Force enable the ramdisk to the running state */
    if (strncmp(cache_info->serial_num, "RAMDISK", 7) == 0) {
	cache_info->state_overall = DM2_MGMT_STATE_CACHEABLE;
	cache_info->set_cache_sec_cnt  = cache_info->set_cache_sec_cnt * 8;
    }

    if (unknown_type)
	cache_info->type_unknown = 1;
    if ((ret = dm2_uri_lock_pool_init(cache_info, 256)) < 0) {
	DBG_DM2S("URI Lock pool init failed: %s %d",
		 cache_info->mgmt_name, ret);
	dm2_free(cache_info, sizeof(*cache_info), DM2_NO_ALIGN);
	return;
    }
    if ((ret = dm2_uri_tbl_init(cache_info)) < 0) {
	DBG_DM2S("GLIB memory allocation failed: %s %d",
		 cache_info->mgmt_name, ret);
	dm2_free(cache_info, sizeof(*cache_info), DM2_NO_ALIGN);
	return;
    }
    if ((ret = dm2_evict_init_buckets(cache_info)) < 0) {
	DBG_DM2S("Evict Buckets Memory allocation failed: %s %d",
		 cache_info->mgmt_name, ret);
	dm2_free(cache_info, sizeof(*cache_info), DM2_NO_ALIGN);
	return;
    }
    dm2_register_disk_counters(cache_info, &cache_type->ct_ptype);
    dm2_ci_dev_rwlock_init(cache_info);
    snprintf(mutex_name, 40, "%s.ci_dm2_delete_mutex", cache_info->mgmt_name);;
    NKN_MUTEX_INITR(&cache_info->ci_dm2_delete_mutex, NULL, mutex_name);
    cache_type->ct_num++;
    dm2_ct_info_list_rwlock_wlock(cache_type);
    cache_type->ct_del_info_list =
	g_list_append(cache_type->ct_del_info_list, cache_info);
    dm2_ct_info_list_rwlock_wunlock(cache_type);
    DBG_DM2S("DM2 mgmt_name=%s serial_num=%s drive_type=%s devname=%s "
	     "cache_mount_point=%s",
	     cache_info->mgmt_name, cache_info->serial_num, drive_type,
	     cache_info->raw_devname, cache_info->ci_mountpt);
    if (add_type)
	(*num_cache_types)++;
    g_static_mutex_unlock(&dm2_cache2_types_mutex);
    return;

 unlock_type:
    g_static_mutex_unlock(&dm2_cache2_types_mutex);
    return;
}	/* dm2_insert_cache_list */


void
dm2_dyninit_one_cache(dm2_mgmt_db_info_t *disk_info,
                      int                slot,
                      int                *num_cache_types)
{
    dm2_mgmt_state_t    dstate;
    char                *serialnum, *rawdev, *fsdev;
    char                mountpt[DM2_MAX_MOUNTPT];
    struct stat         sb;
    size_t              rawdev_totsize;
    int                 umount_all, ret, mounted;

    serialnum = disk_info->mi_serial_num;
    rawdev = disk_info->mi_raw_partition;
    fsdev = disk_info->mi_fs_partition;
    mounted = 0;
    dstate = DM2_MGMT_STATE_NULL;

    /*
     * For a typical mount, the state should remain STATE_NULL until
     * the f/s is mounted
     */
    if (disk_info->mi_commented_out) {
        DBG_DM2S("Skipping %s %s %s", serialnum, rawdev, fsdev);
        dstate = DM2_MGMT_STATE_COMMENTED_OUT;
    } else if (disk_info->mi_missing_after_boot) {
        DBG_DM2S("Missing %s %s %s", serialnum, rawdev, fsdev);
        dstate = DM2_MGMT_STATE_NOT_PRESENT;
    } else if (disk_info->mi_activated == 0) {
        /* If the disk is deactivated, then we don't even need to look for
         * devices yet */
        dstate = DM2_MGMT_STATE_DEACTIVATED;
    }

    /* Check out raw device */
    if ((dstate == DM2_MGMT_STATE_NULL) && stat(rawdev, &sb) < 0) {
        DBG_DM2S("Stat on rawdev (%s) failed: %d", rawdev, errno);
        dstate = DM2_MGMT_STATE_INVAL_FORMAT_BEFORE_MOUNT;
    }
    if ((dstate == DM2_MGMT_STATE_NULL) && !S_ISBLK(sb.st_mode)) {
        DBG_DM2S("Device name (%s) is not a block device: 0x%x",
                 rawdev, sb.st_mode);
        dstate = DM2_MGMT_STATE_INVAL_FORMAT_BEFORE_MOUNT;
    }
    if ((dstate == DM2_MGMT_STATE_NULL) &&
        (rawdev_totsize = dm2_find_raw_dev_size(disk_info->mi_statname)) == 0) {
        DBG_DM2S("Finding raw device (%s) size failed", disk_info->mi_statname);
        dstate = DM2_MGMT_STATE_INVAL_FORMAT_BEFORE_MOUNT;
    }

    /* Check out filesystem device */
    if ((dstate == DM2_MGMT_STATE_NULL) && stat(fsdev, &sb) < 0) {
        DBG_DM2S("Stat on fsdev (%s) failed: %d", fsdev, errno);
        dstate = DM2_MGMT_STATE_INVAL_FORMAT_BEFORE_MOUNT;
    }
    if ((dstate == DM2_MGMT_STATE_NULL) && !S_ISBLK(sb.st_mode)) {
        DBG_DM2S("Device name (%s) is not a block device: 0x%x",
                 fsdev, sb.st_mode);
        dstate = DM2_MGMT_STATE_INVAL_FORMAT_BEFORE_MOUNT;
    }

    /* Mount points are dynamically created */
    ret = snprintf(mountpt, DM2_MAX_MOUNTPT, "%s/%s%d", NKN_DM_MNTPT,
                   NKN_DM_DISK_CACHE_PREFIX, slot);
    if (ret > DM2_MAX_MOUNTPT-1) {
        DBG_DM2S("Device name (%s): mount directory too long: %d",
                 fsdev, ret);
        assert(0);      // should never happen
    }
    strcpy(disk_info->mi_mount_point, mountpt);

    if ((dstate == DM2_MGMT_STATE_NULL) && stat(mountpt, &sb) < 0) {
        if ((ret = nkn_create_directory(mountpt, 1)) < 0) {
            DBG_DM2E("Failed to create mount point: %s", mountpt);
            dstate = DM2_MGMT_STATE_IMPROPER_MOUNT;
        }
    }

    /* attempt to umount f/s and rawdev */
    umount_all = (dstate == DM2_MGMT_STATE_DEACTIVATED ||
                  dstate == DM2_MGMT_STATE_COMMENTED_OUT) ? 1 : 0;
    if ((dstate == DM2_MGMT_STATE_NULL || umount_all) &&
        (ret = dm2_clean_mount_points(fsdev, mountpt, umount_all,
                                      &mounted)) < 0) {
        dstate = DM2_MGMT_STATE_IMPROPER_MOUNT;
    }

    if (!mounted && (dstate == DM2_MGMT_STATE_NULL)) {
        if (dm2_mount_fs(fsdev, mountpt) < 0)
            dstate = DM2_MGMT_STATE_IMPROPER_MOUNT;
        else
            mounted = 1;
    }

    if (mounted && ((ret = dm2_convert_disk(mountpt)) < 0)) {
        dstate = DM2_MGMT_STATE_CONVERT_FAILURE;
    }
    dm2_insert_cache_list(mountpt, rawdev_totsize, disk_info, dstate,
                          num_cache_types);
}       /* dm2_dyninit_one_cache */


static int
dm2_dyninit_caches_db(GList *disk_slot_list)
{
    dm2_mgmt_db_info_t	*disk_info;
    GList		*disk_list, *disk_obj;
    int			slot = 1, num_cache_types = 0, disk_found;

    if ((disk_list = nvsd_mgmt_get_disklist_locked()) == NULL) {
	DBG_DM2S("No valid disk entries in DB");
	return 0;
    }
    /* When this call returns, the disk_list list is locked */

    /* Start with slot=1 and only VXA2100 systems will reset value */
    for (disk_obj = disk_list; disk_obj; disk_obj = disk_obj->next) {
	disk_info = (dm2_mgmt_db_info_t *)disk_obj->data;
	dm2_find_a_disk_slot(disk_slot_list, disk_info->mi_serial_num, &slot,
			     &disk_found);
	if (disk_found) {
	    /* For now, we don't insert drive if we can't find slot */
	    dm2_dyninit_one_cache(disk_info, slot, &num_cache_types);
	}
	if (disk_slot_list == NULL) {
	    /* We don't have a slot list, so we assign incrementing ids */
	    slot++;
	}
    }

    /* release the managment lock */
    nvsd_mgmt_diskcache_unlock();

    glob_dm2_num_caches = dm2_print_num_caches(num_cache_types);
    glob_dm2_num_cache_types = num_cache_types;
    DBG_DM2S("Number of caches found: %ld", glob_dm2_num_caches);

    return num_cache_types;
}	/* dm2_dyninit_caches_db */

/*
 * Read the /proc/mounts file.  Doing this would be safe
 * as long as initialization is single threaded.
 *
 * This code may be obsolete because ci->raw_devname should never be NULL?
 */
static int
dm2_get_raw_devname(struct dm2_cache_info *ci)
{
    FILE          *mnt;
    struct mntent *fs;
    int           found = 0;

    if ((mnt = setmntent("/proc/mounts", "r")) == 0) {
        DBG_DM2S("Failed to open mount file (%s): %d", _PATH_MOUNTED, errno);
        return 0;
    }
    while ((fs = getmntent(mnt)) != NULL) {
        if (!strcmp(fs->mnt_dir, ci->ci_mountpt)) {
            strcpy(ci->raw_devname, fs->mnt_fsname);
            strcpy(ci->bm.bm_devname, fs->mnt_fsname);
            ci->raw_devname[strlen(ci->raw_devname)-1] = '\0';
            strcat(ci->raw_devname, "1");
            DBG_DM2E("Found raw device for cache (%s): %s",
                     ci->ci_mountpt, ci->raw_devname);
            found = 1;
        }
    }
    endmntent(mnt);
    return found;
}       /* dm2_get_raw_devname */

static int
dm2_bitshift32(uint32_t size)
{
    int i;
    int bitcnt = 0;

    for (i=0; i<32; i++) {
        if ((size & 0x1) == 0) {
            bitcnt++;
            size >>= 1;
        } else {
            assert((size >> 1) == 0);
            break;
        }
    }
    return bitcnt;
}       /* dm2_bitshift32 */

int
dm2_bitmap_process(dm2_cache_info_t *ci,
		   int		    cache_enabled)
{
    dm2_bitmap_header_t *bmh;
    int			ret, raw_fd;

    switch (ci->state_overall) {
	case DM2_MGMT_STATE_UNKNOWN:
	case DM2_MGMT_STATE_NOT_PRESENT:
	case DM2_MGMT_STATE_INVAL_FORMAT_BEFORE_MOUNT:
	case DM2_MGMT_STATE_COMMENTED_OUT:
	case DM2_MGMT_STATE_DEACTIVATED:
	case DM2_MGMT_STATE_IMPROPER_UNMOUNT:
	case DM2_MGMT_STATE_IMPROPER_MOUNT:
	case DM2_MGMT_STATE_FORMAT_UNKNOWN_AFTER_MOUNT:
	case DM2_MGMT_STATE_BITMAP_BAD:
	case DM2_MGMT_STATE_BITMAP_NOT_FOUND:
	case DM2_MGMT_STATE_CONVERT_FAILURE:
	case DM2_MGMT_STATE_WRONG_VERSION:
	case DM2_MGMT_STATE_CACHE_DISABLING:
	    /* Do nothing */
	    return 0;

	case DM2_MGMT_STATE_ACTIVATED:
	    /* We need to mount the filesystem */
	    ret = -NKN_DM2_INVALID_MGMT_REQUEST;
	    break;

	case DM2_MGMT_STATE_CACHEABLE:
	    /* We'll try to read the bitmap from this state */
	    break;

	case DM2_MGMT_STATE_NULL:
	case DM2_MGMT_STATE_CACHE_RUNNING:
	    /* We should never be in these states */
	    NKN_ASSERT(0);
	    break;
    }
    ret = dm2_bitmap_read_free_pages(ci, cache_enabled);
    if (ret != 0) {
	/* Change state_overall & possibly 'ret' */
	dm2_bitmap_set_init_state(ci, &ret);
	ci->valid_cache = 0;
	return ret;
    }

    /* New buffer is allocted in dm2_bitmap_read_free_pages */
    bmh = (dm2_bitmap_header_t *)ci->bm.bm_buf;
    nkn_mon_add(DM2_PAGE_SIZE_MON, ci->mgmt_name, &bmh->u.bmh_disk_pagesize,
		sizeof(bmh->u.bmh_disk_pagesize));
    nkn_mon_add(DM2_BLOCK_SIZE_MON, ci->mgmt_name, &bmh->u.bmh_disk_blocksize,
		sizeof(bmh->u.bmh_disk_blocksize));

    ci->valid_cache = 1;
    if (ci->raw_devname[0] == '\0' && dm2_get_raw_devname(ci) == 0) {
	DBG_DM2S("Failed to find raw devname for cache (%s)",
		 ci->ci_mountpt);
	ci->valid_cache = 0;
    } else {
	glob_dm2_raw_open_cnt++;
	if ((raw_fd = dm2_open(ci->raw_devname, O_RDWR, 0)) == -1) {
	    ret = -errno;
	    NKN_ASSERT(ret != -EMFILE);
	    DBG_DM2S("Failed to open raw devname for cache (%s): %d",
		     ci->raw_devname, ret);
	    ci->valid_cache = 0;
	    glob_dm2_raw_open_err++;
	} else {
	    nkn_mark_fd(raw_fd, DM2_FD);
	    nkn_close_fd(raw_fd, DM2_FD);
	    glob_dm2_raw_close_cnt++;
	}
    }

   ci->bm.bm_byte2block_bitshift = dm2_bitshift32(bmh->u.bmh_disk_blocksize);

   /* Shrinking has already been done */
   AO_store(&ci->bm.bm_num_resv_blk_free, ci->bm.bm_num_blk_free);

   DBG_DM2M("[%s] FreeBitMap: free_pages=%ld  free_blocks=%ld",
	     ci->mgmt_name, ci->bm.bm_num_page_free, ci->bm.bm_num_blk_free);
    return 0;
}	/* dm2_bitmap_process */

static void
dm2_setup_global_counter_aliases(void)
{
    nkn_mon_add("memory.maximum_bytes", "dictionary",
		&glob_dm2_mem_max_bytes, sizeof(glob_dm2_mem_max_bytes));
    nkn_mon_add("memory.used_bytes", "dictionary",
		&glob_dm2_alloc_total_bytes,
		sizeof(glob_dm2_alloc_total_bytes));
    nkn_mon_add("memory.overflow_cnt", "dictionary",
		&glob_dm2_mem_overflow, sizeof(glob_dm2_mem_overflow));
    //   nkn_mon_add("entries.cnt", "dictionary");
}	/* dm2_setup_global_counter_aliases */


static void
dm2_register_cache_type_counters(int num_cache_types)
{
    char		tstr[120];
    int			hotval, i, ptype_idx;
    dm2_cache_type_t	*ct;
    register char	*tier_name;

    for (ptype_idx = 0; ptype_idx < num_cache_types; ptype_idx++) {
	ct = &g_cache2_types[ptype_idx];
	tier_name = ct->ct_name;

	nkn_mon_add("dm2_active_caches_cnt", tier_name,
		    &ct->ct_num_active_caches,
		    sizeof(ct->ct_num_active_caches));
	nkn_mon_add("dm2_stat_cnt", tier_name,
		    &ct->ct_dm2_stat_cnt, sizeof(ct->ct_dm2_stat_cnt));
	nkn_mon_add("dm2_get_cnt", tier_name,
		    &ct->ct_dm2_get_cnt, sizeof(ct->ct_dm2_get_cnt));
	nkn_mon_add(DM2_PUT_CNT, tier_name,
		    &ct->ct_dm2_put_cnt, sizeof(ct->ct_dm2_put_cnt));
	nkn_mon_add("dm2_attr_update_cnt", tier_name,
		    &ct->ct_dm2_attr_update_cnt,
		    sizeof(ct->ct_dm2_attr_update_cnt));
	nkn_mon_add("dm2_attr_update_write_cnt", tier_name,
		    &ct->ct_dm2_attr_update_write_cnt,
		    sizeof(ct->ct_dm2_attr_update_write_cnt));

	nkn_mon_add(DM2_PUT_ERR, tier_name,
		    &ct->ct_dm2_put_err, sizeof(ct->ct_dm2_put_err));

	nkn_mon_add(DM2_RAW_WRITE_BYTES_MON, tier_name,
		    &ct->ct_dm2_raw_write_bytes,
		    sizeof(ct->ct_dm2_raw_write_bytes));
	nkn_mon_add(DM2_RAW_READ_BYTES_MON, tier_name,
		    &ct->ct_dm2_raw_read_bytes,
		    sizeof(ct->ct_dm2_raw_read_bytes));
	nkn_mon_add(DM2_DELETE_BYTES_MON, tier_name,
		    &ct->ct_dm2_delete_bytes, sizeof(ct->ct_dm2_delete_bytes));

	snprintf(tstr, sizeof(tstr), "tier.%s.ingest_bytes", tier_name);
	nkn_mon_add(tstr, "dm2", &ct->ct_dm2_ingest_bytes,
		    sizeof(ct->ct_dm2_ingest_bytes));
	snprintf(tstr, sizeof(tstr), "tier.%s.evict_bytes", tier_name);
	nkn_mon_add(tstr, "dm2", &ct->ct_dm2_evict_bytes,
		    sizeof(ct->ct_dm2_evict_bytes));

	nkn_mon_add(DM2_ATTRFILE_OPEN_CNT_MON, tier_name,
		    &ct->ct_dm2_attrfile_open_cnt,
		    sizeof(ct->ct_dm2_attrfile_open_cnt));
	nkn_mon_add(DM2_ATTRFILE_OPEN_ERR_MON, tier_name,
		    &ct->ct_dm2_attrfile_open_err,
		    sizeof(ct->ct_dm2_attrfile_open_err));
	nkn_mon_add(DM2_ATTRFILE_READ_CNT_MON, tier_name,
		    &ct->ct_dm2_attrfile_read_cnt,
		    sizeof(ct->ct_dm2_attrfile_read_cnt));
	nkn_mon_add(DM2_ATTRFILE_READ_ERR_MON, tier_name,
		    &ct->ct_dm2_attrfile_read_err,
		    sizeof(ct->ct_dm2_attrfile_read_err));
	nkn_mon_add(DM2_ATTRFILE_READ_BYTES_MON, tier_name,
		    &ct->ct_dm2_attrfile_read_bytes,
		    sizeof(ct->ct_dm2_attrfile_read_bytes));
	nkn_mon_add(DM2_ATTRFILE_READ_COD_MISMATCH_WARN_MON, tier_name,
		    &ct->ct_dm2_attrfile_read_cod_mismatch_warn,
		    sizeof(ct->ct_dm2_attrfile_read_cod_mismatch_warn));
	nkn_mon_add(DM2_ATTRFILE_WRITE_BYTES_MON, tier_name,
		    &ct->ct_dm2_attrfile_write_bytes,
		    sizeof(ct->ct_dm2_attrfile_write_bytes));
	nkn_mon_add(DM2_ATTRFILE_WRITE_CNT_MON, tier_name,
		    &ct->ct_dm2_attrfile_write_cnt,
		    sizeof(ct->ct_dm2_attrfile_write_cnt));

	nkn_mon_add(DM2_CONTAINER_OPEN_CNT_MON, tier_name,
		    &ct->ct_dm2_container_open_cnt,
		    sizeof(ct->ct_dm2_container_open_cnt));
	nkn_mon_add(DM2_CONTAINER_OPEN_ERR_MON, tier_name,
		    &ct->ct_dm2_container_open_err,
		    sizeof(ct->ct_dm2_container_open_err));
	nkn_mon_add(DM2_CONTAINER_WRITE_BYTES_MON, tier_name,
		    &ct->ct_dm2_container_write_bytes,
		    sizeof(ct->ct_dm2_container_write_bytes));
	nkn_mon_add(DM2_CONTAINER_WRITE_CNT_MON, tier_name,
		    &ct->ct_dm2_container_write_cnt,
		    sizeof(ct->ct_dm2_container_write_cnt));
	nkn_mon_add(DM2_CONTAINER_READ_BYTES_MON, tier_name,
		    &ct->ct_dm2_container_read_bytes,
		    sizeof(ct->ct_dm2_container_read_bytes));
	nkn_mon_add(DM2_CONTAINER_READ_CNT_MON, tier_name,
		    &ct->ct_dm2_container_read_cnt,
		    sizeof(ct->ct_dm2_container_read_cnt));

	nkn_mon_add(DM2_STAT_NOT_READY_MON, tier_name,
		    &ct->ct_dm2_stat_not_ready,
		    sizeof(ct->ct_dm2_stat_not_ready));
	nkn_mon_add(DM2_PUT_NOT_READY_MON, tier_name,
		    &ct->ct_dm2_put_not_ready,
		    sizeof(ct->ct_dm2_put_not_ready));
	// Reject further container reads in stat if dictionary is full
	nkn_mon_add(DM2_STAT_DICT_FULL_MON, tier_name,
		    &ct->ct_dm2_stat_dict_full,
		    sizeof(ct->ct_dm2_stat_dict_full));

	nkn_mon_add("dm2_preread_opt", tier_name,
		    &ct->ct_dm2_preread_opt,
		    sizeof(ct->ct_dm2_preread_opt));
	nkn_mon_add("dm2_preread_hash_insert_cnt", tier_name,
		    &ct->ct_dm2_preread_hash_insert_cnt,
		    sizeof(ct->ct_dm2_preread_hash_insert_cnt));
	nkn_mon_add("dm2_preread_hash_free_cnt", tier_name,
		    &ct->ct_dm2_preread_hash_free_cnt,
		    sizeof(ct->ct_dm2_preread_hash_free_cnt));
	nkn_mon_add("dm2_preread_duplicate_skip_cnt", tier_name,
		    &ct->ct_dm2_preread_duplicate_skip_cnt,
		    sizeof(ct->ct_dm2_preread_duplicate_skip_cnt));
	nkn_mon_add(DM2_PREREAD_STAT_OPT_CNT_MON, tier_name,
		    &ct->ct_dm2_stat_opt_not_found,
		    sizeof(ct->ct_dm2_stat_opt_not_found));
	nkn_mon_add(DM2_PREREAD_PUT_OPT_CNT_MON, tier_name,
		    &ct->ct_dm2_put_opt_not_found,
		    sizeof(ct->ct_dm2_put_opt_not_found));
	nkn_mon_add(DM2_PREREAD_ATTR_UPDATE_OPT_CNT_MON, tier_name,
		    &ct->ct_dm2_attr_update_opt_not_found,
		    sizeof(ct->ct_dm2_attr_update_opt_not_found));
	nkn_mon_add(DM2_PREREAD_DELETE_OPT_CNT_MON, tier_name,
		    &ct->ct_dm2_delete_opt_not_found,
		    sizeof(ct->ct_dm2_delete_opt_not_found));
	// dictionary pre-read done
	nkn_mon_add("dm2_preread_success_cnt", tier_name,
		    &ct->ct_num_caches_preread_success,
		    sizeof(ct->ct_num_caches_preread_success));
	nkn_mon_add(DM2_PREREAD_DONE_MON, tier_name,
		    &ct->ct_tier_preread_done,
		    sizeof(ct->ct_tier_preread_done));
	snprintf(tstr, sizeof(tstr), "tier.%s.preread.done", tier_name);
	nkn_mon_add(tstr, "dictionary", &ct->ct_tier_preread_done,
		    sizeof(ct->ct_tier_preread_done));
	// dictionary pre-read optimized stat calls
	nkn_mon_add(DM2_PREREAD_STAT_OPT_MON, tier_name,
		    &ct->ct_tier_preread_stat_opt,
		    sizeof(ct->ct_tier_preread_stat_opt));
	snprintf(tstr, sizeof(tstr), "tier.%s.preread.stat_optimized",
		 tier_name);
	nkn_mon_add(tstr, "dictionary", &ct->ct_tier_preread_stat_opt,
		    sizeof(ct->ct_tier_preread_stat_opt));

	nkn_mon_add(DM2_INTEVICT_HIGH_MARK, tier_name,
		    &ct->ct_internal_high_water_mark,
		    sizeof(ct->ct_internal_high_water_mark));
	nkn_mon_add(DM2_INTEVICT_LOW_MARK, tier_name,
		    &ct->ct_internal_low_water_mark,
		    sizeof(ct->ct_internal_low_water_mark));
	nkn_mon_add("dm2_int_evict_time_low_water_mark", tier_name,
		    &ct->ct_internal_time_low_water_mark,
		    sizeof(ct->ct_internal_time_low_water_mark));
	nkn_mon_add(DM2_INTEVICT_PERCENT, tier_name,
		    &ct->ct_internal_evict_percent,
		    sizeof(ct->ct_internal_evict_percent));

	nkn_mon_add(DM2_NUM_PUT_THRDS, tier_name,
		    &ct->ct_dm2_num_put_threads,
		    sizeof(ct->ct_dm2_num_put_threads));
	nkn_mon_add(DM2_NUM_GET_THRDS, tier_name,
		    &ct->ct_dm2_num_get_threads,
		    sizeof(ct->ct_dm2_num_get_threads));

	nkn_mon_add(DM2_EXT_EVICT_CALLS_MON, tier_name,
		    &ct->ct_ext_evict_total_call_cnt,
		    sizeof(ct->ct_ext_evict_total_call_cnt));
	nkn_mon_add(DM2_EXT_EVICT_DELS_MON, tier_name,
		    &ct->ct_ext_evict_object_delete_cnt,
		    sizeof(ct->ct_ext_evict_object_delete_cnt));
	nkn_mon_add(DM2_EXT_EVICT_FAILS_MON, tier_name,
		    &ct->ct_ext_evict_object_delete_fail_cnt,
		    sizeof(ct->ct_ext_evict_object_delete_fail_cnt));
	nkn_mon_add(DM2_EXT_EVICT_ENONENTS_MON, tier_name,
		    &ct->ct_ext_evict_object_enoent_cnt,
		    sizeof(ct->ct_ext_evict_object_enoent_cnt));

	snprintf(tstr, sizeof(tstr),"tier.%s.ingest_objects", tier_name);
	nkn_mon_add(tstr, "dm2", &ct->ct_dm2_ingest_objects,
		    sizeof(ct->ct_dm2_ingest_objects));

	snprintf(tstr, sizeof(tstr),"tier.%s.delete_objects", tier_name);
	nkn_mon_add(tstr, "dm2", &ct->ct_dm2_delete_objects,
		    sizeof(ct->ct_dm2_delete_objects));

	nkn_mon_add("dm2_evict_threshold", tier_name,
		    &ct->ct_am_hot_evict_threshold,
		    sizeof(ct->ct_am_hot_evict_threshold));
	nkn_mon_add("dm2_physid_hash_cnt", tier_name,
		    &ct->ct_dm2_physid_hash_table_total_cnt,
		    sizeof(ct->ct_dm2_physid_hash_table_total_cnt));
	nkn_mon_add("dm2_container_hash_cnt", tier_name,
		    &ct->ct_dm2_conthead_hash_table_total_cnt,
		    sizeof(ct->ct_dm2_conthead_hash_table_total_cnt));
	nkn_mon_add("dm2_stat_curr_cnt", tier_name,
		    &ct->ct_dm2_stat_curr_cnt,
		    sizeof(ct->ct_dm2_stat_curr_cnt));
	nkn_mon_add("dm2_preread_stat_curr_cnt", tier_name,
		    &ct->ct_dm2_pr_stat_curr_cnt,
		    sizeof(ct->ct_dm2_pr_stat_curr_cnt));
	nkn_mon_add("dm2_stat_uri_partial_cnt", tier_name,
		    &ct->ct_dm2_stat_uri_partial_cnt,
		    sizeof(ct->ct_dm2_stat_uri_partial_cnt));
	nkn_mon_add("dm2_get_curr_cnt", tier_name,
		    &ct->ct_dm2_get_curr_cnt,
		    sizeof(ct->ct_dm2_get_curr_cnt));
	nkn_mon_add("dm2_delete_curr_cnt", tier_name,
		    &ct->ct_dm2_delete_curr_cnt,
		    sizeof(ct->ct_dm2_delete_curr_cnt));
	nkn_mon_add("dm2_put_curr_cnt", tier_name,
		    &ct->ct_dm2_put_curr_cnt,
		    sizeof(ct->ct_dm2_put_curr_cnt));
	nkn_mon_add("dm2_attr_update_curr_cnt", tier_name,
		    &ct->ct_dm2_attr_update_curr_cnt,
		    sizeof(ct->ct_dm2_attr_update_curr_cnt));
	nkn_mon_add("dm2_prov_stat_curr_cnt", tier_name,
		    &ct->ct_dm2_prov_stat_curr_cnt,
		    sizeof(ct->ct_dm2_prov_stat_curr_cnt));
	nkn_mon_add("dm2_raw_read_cnt", tier_name,
		    &ct->ct_dm2_raw_read_cnt,
		    sizeof(ct->ct_dm2_raw_read_cnt));
	nkn_mon_add("dm2_raw_write_cnt", tier_name,
		    &ct->ct_dm2_raw_write_cnt,
		    sizeof(ct->ct_dm2_raw_write_cnt));

	nkn_mon_add("dm2_uri.rwLOCK.wr.wait", tier_name,
		    &ct->ct_uri_wr_wait_times,
		    sizeof(ct->ct_uri_wr_wait_times));
	nkn_mon_add("dm2_uri.rwLOCK.wr.count", tier_name,
		    &ct->ct_uri_wr_lock_times,
		    sizeof(ct->ct_uri_wr_lock_times));
	nkn_mon_add("dm2_uri.rwLOCK.rd.wait", tier_name,
		    &ct->ct_uri_rd_wait_times,
		    sizeof(ct->ct_uri_rd_wait_times));
	nkn_mon_add("dm2_uri.rwLOCK.rd.count", tier_name,
		    &ct->ct_uri_rd_lock_times,
		    sizeof(ct->ct_uri_rd_lock_times));

	nkn_mon_add("dm2_get_cod_mismatch_warn", tier_name,
		    &ct->ct_dm2_get_cod_mismatch_warn,
		    sizeof(ct->ct_dm2_get_cod_mismatch_warn));
	nkn_mon_add("dm2_get_cod_mismatch_err", tier_name,
		    &ct->ct_dm2_get_cod_mismatch_err,
		    sizeof(ct->ct_dm2_get_cod_mismatch_err));
	nkn_mon_add("dm2_stat_cod_mismatch_warn", tier_name,
		    &ct->ct_dm2_stat_cod_mismatch_warn,
		    sizeof(ct->ct_dm2_stat_cod_mismatch_warn));
	nkn_mon_add("dm2_put_cod_mismatch_warn", tier_name,
		    &ct->ct_dm2_put_cod_mismatch_warn,
		    sizeof(ct->ct_dm2_put_cod_mismatch_warn));
	nkn_mon_add("dm2_delete_cod_mismatch_warn", tier_name,
		    &ct->ct_dm2_delete_cod_mismatch_warn,
		    sizeof(ct->ct_dm2_delete_cod_mismatch_warn));
	nkn_mon_add("dm2_update_attr_cod_mismatch_warn", tier_name,
		    &ct->ct_dm2_update_attr_cod_mismatch_warn,
		    sizeof(ct->ct_dm2_update_attr_cod_mismatch_warn));
	nkn_mon_add("dm2_update_attr_version_mismatch_warn", tier_name,
		    &ct->ct_dm2_update_attr_version_mismatch_err,
		    sizeof(ct->ct_dm2_update_attr_version_mismatch_err));
	nkn_mon_add("dm2_int_evict_cod_alloc_err", tier_name,
		    &ct->ct_dm2_int_evict_cod_alloc_err,
		    sizeof(ct->ct_dm2_int_evict_cod_alloc_err));

	nkn_mon_add("dm2_put_bad_uri_err", tier_name,
		    &ct->ct_dm2_put_bad_uri_err,
		    sizeof(ct->ct_dm2_put_bad_uri_err));

	nkn_mon_add("dm2_cache_disabling", tier_name,
		    &ct->ct_dm2_cache_disabling,
		    sizeof(ct->ct_dm2_cache_disabling));

	/* Latencies */
	snprintf(tstr, sizeof(tstr), "%s.dm2_stat", tier_name);
	LATENCY_INIT(ct->ct_dm2_stat_latency, tstr);
	snprintf(tstr, sizeof(tstr), "%s.dm2_get", tier_name);
	LATENCY_INIT(ct->ct_dm2_get_latency, tstr);
	snprintf(tstr, sizeof(tstr), "%s.dm2_put", tier_name);
	LATENCY_INIT(ct->ct_dm2_put_latency, tstr);
	snprintf(tstr, sizeof(tstr), "%s.dm2_delete", tier_name);
	LATENCY_INIT(ct->ct_dm2_delete_latency, tstr);
	snprintf(tstr, sizeof(tstr), "%s.dm2_attr_update", tier_name);
	LATENCY_INIT(ct->ct_dm2_attrupdate_latency, tstr);

	/* Internal eviction counters */

	for (i=0; i<DM2_MAX_EVICT_BUCKETS; i++) {
	    if (i <= DM2_UNIQ_EVICT_BKTS) {
		snprintf(tstr, sizeof(tstr), "dm2_evict_bkt_%d_blks", i);
	    } else {
		hotval = 1 << (i - DM2_UNIQ_EVICT_BKTS + DM2_MAP_UNIQ_BKTS);
		snprintf(tstr, sizeof(tstr), "dm2_evict_bkt_%d_blks", hotval-1);
	    }
	    nkn_mon_add(tstr, tier_name, &ct->ct_dm2_bkt_evict_blks[i],
			sizeof(ct->ct_dm2_bkt_evict_blks[i]));
	}
	nkn_mon_add("int_evict_num_disk_prcnt", tier_name,
                    &ct->ct_num_evict_disk_prcnt,
                    sizeof(ct->ct_num_evict_disk_prcnt));
	nkn_mon_add("int_evict_call_cnt", tier_name,
		    &ct->ct_dm2_int_evict_call_cnt,
		    sizeof(ct->ct_dm2_int_evict_call_cnt));
	nkn_mon_add("int_evict_del_cnt", tier_name,
		    &ct->ct_dm2_int_evict_del_cnt,
		    sizeof(ct->ct_dm2_int_evict_del_cnt));
	nkn_mon_add("int_evict_disk_full_cnt", tier_name,
		    &ct->ct_dm2_int_evict_disk_full_cnt,
		    sizeof(ct->ct_dm2_int_evict_disk_full_cnt));
	nkn_mon_add("int_evict_dict_full_cnt", tier_name,
		    &ct->ct_dm2_int_evict_dict_full_cnt,
		    sizeof(ct->ct_dm2_int_evict_dict_full_cnt));
	nkn_mon_add("int_evict_last_disk_full_ts", tier_name,
		    &ct->ct_dm2_last_disk_full_int_evict_ts,
		    sizeof(ct->ct_dm2_last_disk_full_int_evict_ts));
	nkn_mon_add("int_evict_last_dict_full_ts", tier_name,
		    &ct->ct_dm2_last_dict_full_int_evict_ts,
		    sizeof(ct->ct_dm2_last_dict_full_int_evict_ts));
	nkn_mon_add("int_evict_time_cnt", tier_name,
		    &ct->ct_dm2_int_evict_time_cnt,
		    sizeof(ct->ct_dm2_int_evict_time_cnt));
	nkn_mon_add("int_evict_last_time", tier_name,
		    &ct->ct_dm2_last_time_int_evict_ts,
		    sizeof(ct->ct_dm2_last_time_int_evict_ts));
	nkn_mon_add("dm2_int_evict_time_evict_state", tier_name,
		    &ct->ct_dm2_int_time_evict_in_progress,
		    sizeof(ct->ct_dm2_int_time_evict_in_progress));
    }
}	/* dm2_register_cache_type_counters */


/*
 * These are all on-disk structures, so we can not afford to have them become
 * unaligned.
 */
static void
dm2_verify_structure_sizes(void)
{
    assert(sizeof(dm2_disk_container_t) == 2*4096);
    assert(sizeof(dm2_container_t) < 512);
    assert(sizeof(container_head_block_t) == 4096);
    assert(sizeof(container_disk_header_t) <= 2*1024);
    assert(NKN_CONTAINER_HEADER_SZ ==
	   roundup(sizeof(dm2_disk_container_t), DEV_BSIZE));

    /* This is the old attribute object */
    assert(sizeof(nkn_attr_v1_t) == 512);

    /* Disk extents should be one sector */
    assert(sizeof(DM2_disk_extent_t) <= 512);

    /* Mem extent should fit in 64b buffer (assume 8b allocator overhead) */
    assert(sizeof(DM2_extent_t) <= 128);

    /* A uri_head should fit in 128b buffer (assume 8b allocator overhead) */
    assert(sizeof(DM2_uri_t) <= 120);

    /*
     * We only want 256 bytes to be allocated to the C portion of the new
     * nkn_attr_t
     */
    assert(NKN_ATTR_HEADER_LEN == 256);

    /*
     * For now this field should only consume 8 bytes, but it can grow.  A
     * more likely scenario to blow this assert would be someone accidentally
     * defining a new object within the union which doesn't align properly.
     */
    assert(sizeof(nkn_attr_priv_t) == 16);

    /*
     * If this hits, then a new DM2_MGMT_STATE value was added but we failed
     * to add the state to dm2_mgmt_state_strings.
     */
    assert(dm2_mgmt_state_strings[DM2_MGMT_STATE_MAX] != NULL);
}	/* dm2_verify_structure_sizes */

static void*
dm2_ci_init_thread_fn(void *arg)
{
    dm2_cache_info_t	*ci = (dm2_cache_info_t *)arg;
    dm2_cache_type_t	*ct = ci->ci_cttype;
    int ret;

    ret = pthread_detach(pthread_self());
    assert(ret == 0);

    DBG_DM2S("Process disk %s", ci->mgmt_name);
    ret = dm2_bitmap_process(ci, ci->state_cache_enabled);
    if (ret != 0) {
	goto error;
    }
    if (ci->valid_cache == 1) {
	/* Start the evict thread for this disk */
	pthread_create(&ci->ci_evict_thread, NULL,
		       dm2_evict_ci_fn, (void *)ci);
    }
    /* Initialize the meta partition availability */
    dm2_check_meta_fs_space(ci);

 error:
    if (ret) {
	DBG_DM2S("Processing of bitmap failed for disk %s:%d",
		 ci->mgmt_name, ret);
    }
    AO_fetch_and_add1(&ct->ct_dm2_init_thread_exit_cnt);
    return NULL;
}   /* dm2_init_thread_fn */

/*
 * Only register when that cache provider type has been detected or configured
 */
void
dm2_register_ptype_mm_am(dm2_cache_type_t *ct)
{
    int ptype_idx, num_put_thrds, num_get_thrds, num_disks;
    int cache_use_percent, hotness;
    uint32_t max_io_throughput;
    nkn_provider_type_t ptype;
    char ctype[DM2_MAX_TIERNAME];

    ptype = ct->ct_ptype;

    if ((ptype_idx = dm2_cache_array_map_ret_idx(ptype)) < 0) {
	DBG_DM2S("Invalid provider type %d\n", ptype);
	return;
    }

    num_disks = g_cache2_types[ptype_idx].ct_num;
    if (num_disks <= 0) {
	DBG_DM2S("Invalid disk count for provider %d", ptype);
	return;
    }

    if (dm2_get_am_thresholds(ptype, &cache_use_percent, &max_io_throughput,
			      &hotness, ctype) < 0)
	return;

    num_put_thrds = PUT_THR_PER_DISK * num_disks;
    num_get_thrds = GET_THR_PER_DISK * num_disks;

    g_cache2_types[ptype_idx].ct_dm2_num_put_threads = num_put_thrds;
    g_cache2_types[ptype_idx].ct_dm2_num_get_threads = num_get_thrds;

    MM_register_provider(ptype, 0,
			 DM2_put, DM2_stat, DM2_get, DM2_delete,
			 DM2_shutdown, DM2_discover, NULL,
			 DM2_provider_stat, NULL, DM2_attribute_update,
			 DM2_promote_complete, num_put_thrds,
			 num_get_thrds, 12000, num_disks,
			 MM_THREAD_ACTION_ASYNC);

    /* Set up an AM policy for disk */
    dm2_setup_am_thresholds(max_io_throughput, hotness, ptype, 0);

    /* If there are no disks that are usable (not in CACHE_RUNNING) state
     * tell MM that this provider is not active */

    if (ct->ct_info_list == NULL)
	MM_set_provider_state(ptype, 0, MM_PROVIDER_STATE_INACTIVE);

}	/* dm2_register_ptype_mm_am */

static int
dm2_attach_to_nsmartd(void)
{
    char  producer[64] = "/opt/nkn/sbin/nsmartd";
    int   shmid;
    int   sh_size, ret, max_cnt_space;
    key_t shmkey;
    char  *shm;

    shmkey = ftok(producer, NKN_SHMKEY);
    if (shmkey < 0) {
        DBG_DM2S("ftok failed (%s): %d", producer, errno);
        return -1;
    }

    max_cnt_space = MAX_CNTS_OTHERS;

    sh_size = (sizeof(nkn_shmdef_t)+max_cnt_space*(sizeof(glob_item_t)+30));

    shmid = shmget(shmkey, sh_size, 0666);
    if (shmid < 0) {
        DBG_DM2S("shmget failed: %d", errno);
        return -1;
    }

    shm = shmat(shmid, NULL, 0);
    if (shm == (char *) -1) {
        DBG_DM2S("shmat failed: %d", errno);
        return -1;
    }

    ret = nkncnt_client_init(shm, MAX_CNTS_OTHERS, &dm2_smart_ctx);
    if (ret < 0) {
        DBG_DM2S("nkncnt client context create failed");
        return -1;
    }

    return 0;
}   /* dm2_attach_to_nsmartd */

static void
dm2_get_drive_throttle_list(void)
{
    GList *ci_obj;
    dm2_cache_info_t *ci;
    char cntr[128], *disk;
    glob_item_t   *val;
    int ct_index;

    for (ct_index=0; ct_index < glob_dm2_num_cache_types; ct_index++) {
        dm2_ct_info_list_rwlock_rlock(&g_cache2_types[ct_index]);

        ci_obj = g_cache2_types[ct_index].ct_info_list;
        for (; ci_obj; ci_obj = ci_obj->next) {
            ci = (dm2_cache_info_t *)ci_obj->data;
            disk = strrchr(ci->disk_name, '/'); disk++;

            snprintf(cntr, 128,"%s.%s", disk, NEED_THROTTLE);
            nkncnt_client_get_exact_match(&dm2_smart_ctx, cntr, &val);
            if (val && val->value) {
                ci->ci_throttle = 1;
                snprintf(cntr, 128,"%s.%s", disk, DRIVE_SIZE_IN_GB);
                val = NULL;
                nkncnt_client_get_exact_match(&dm2_smart_ctx, cntr, &val);
                if (val)
                    ci->ci_drive_size_gb = val->value;

                val = NULL;
                snprintf(cntr, 128,"%s.%s", disk, PE_CYCLES);
                nkncnt_client_get_exact_match(&dm2_smart_ctx, cntr, &val);
                if (val)
                    ci->ci_pe_cycles = val->value;
            }
        }

        ci_obj = g_cache2_types[ct_index].ct_del_info_list;
        for (; ci_obj; ci_obj = ci_obj->next) {
            ci = (dm2_cache_info_t *)ci_obj->data;
            disk = strrchr(ci->disk_name, '/'); disk++;

            snprintf(cntr, 128,"%s.%s", disk, NEED_THROTTLE);
            nkncnt_client_get_exact_match(&dm2_smart_ctx, cntr, &val);
            if (val && val->value) {
                ci->ci_throttle = 1;
                snprintf(cntr, 128,"%s.%s", disk, DRIVE_SIZE_IN_GB);
                val = NULL;
                nkncnt_client_get_exact_match(&dm2_smart_ctx, cntr, &val);
                if (val)
                    ci->ci_drive_size_gb = val->value;

                val = NULL;
                snprintf(cntr, 128,"%s.%s", disk, PE_CYCLES);
                nkncnt_client_get_exact_match(&dm2_smart_ctx, cntr, &val);
                if (val)
                    ci->ci_pe_cycles = val->value;
            }
        }

        dm2_ct_info_list_rwlock_runlock(&g_cache2_types[ct_index]);
    }
}   /* dm2_get_drive_throttle_list */

/*
 *
 */
int
DM2_init(void)
{
    dm2_cache_info_t	*ci;
    dm2_cache_type_t	*ct;
    struct stat		sb;
    GList		*ci_obj, *tmp_del_info_list, *disk_slot_list = NULL;
    uint32_t		caches;
    int			ret, block_size;
    int			ptype_idx, valid_caches, num_cache_types;
    char                test_file[] = "/nkn/dm2_uri_file.txt";
    char                test_full_file[] = "/nkn/dm2_full_uri_file.txt";
    char                test_part_file[] = "/nkn/dm2_part_uri_file.txt";

    DBG_DM2M("Initializing DM2");

    dm2_verify_structure_sizes();

    if ((ret = ns_stat_get_initial_namespaces()) != 0) {
	DBG_DM2S("Unable to get initial namespaces");
	return ret;
    }

    if (stat("/config/nkn/ram_disk_only", &sb) == 0) {
        DBG_DM2S("DM2 configured to work on a RAM drive. \
		  Please make sure all the other drives are removed from \
		  the TM config database");
        dm2_ram_drive_only = 1;
    }

    dm2_setup_global_counter_aliases();

    dm2_get_device_disk_slots(&disk_slot_list);

    /* Read in configuration information */
    num_cache_types = dm2_dyninit_caches_db(disk_slot_list);
    dm2_free_disk_slot_list(disk_slot_list);

    valid_caches = 0;
    for (ptype_idx = 0; ptype_idx < num_cache_types; ptype_idx++) {
	ct = &g_cache2_types[ptype_idx];
	dm2_ct_info_list_rwlock_rlock(&g_cache2_types[ptype_idx]);
	tmp_del_info_list = NULL;
	/* everything starts off on the del_info_list */
	caches = 0;
	for (ci_obj = ct->ct_del_info_list; ci_obj; ci_obj = ci_obj->next) {
	    ci = (dm2_cache_info_t *)ci_obj->data;
	    /* Spawn a thread to read in the bitmap file */
	    pthread_create(&ci->ci_init_thread, NULL,
			   dm2_ci_init_thread_fn, (void *)ci);
	    tmp_del_info_list = g_list_append(tmp_del_info_list, ci);
	    caches++;
	}

	while (1) {
	    /* Wait till all the init threads exit */
	    sleep (1);
	    if (caches == AO_load(&ct->ct_dm2_init_thread_exit_cnt))
		break;
	}

	for (ci_obj = tmp_del_info_list; ci_obj; ci_obj = ci_obj->next) {
	    ci = (dm2_cache_info_t *)ci_obj->data;
	    if (ci->valid_cache) {
		ci->state_overall = DM2_MGMT_STATE_CACHE_RUNNING;
		ct->ct_info_list = g_list_append(ct->ct_info_list, ci);
		valid_caches++;
		ct->ct_num_active_caches++;
		/* If the disk is formatted using a 256K/4K layout,
                 * disable block sharing */
		block_size = dm2_get_blocksize(ci);
                if (block_size == DM2_DISK_BLKSIZE_256K) {
                    dm2_ct_reset_block_share_threshold(ct->ct_ptype);
                    ct->ct_share_block_threshold = 0;
                }
		dm2_set_lowest_tier(ct->ct_ptype);
		/* Remove this disk from del_info_list. If a disk cache is
		 * not valid, it will stay in the del_info_list */
		ct->ct_del_info_list = g_list_remove(ct->ct_del_info_list, ci);
	    }
	}
	g_list_free(tmp_del_info_list);

	/* Fill the water mark and free block thresholds */
	dm2_ct_update_thresholds(ct);

	dm2_ct_info_list_rwlock_runlock(&g_cache2_types[ptype_idx]);
	dm2_register_ptype_mm_am(ct);	// Register with MM & AM
	dm2_ns_set_tier_size_totals(ct);
    }
    if (valid_caches == 0) {
	DBG_DM2A("ALARM: No valid disk caches present");
    }
    dm2_register_cache_type_counters(num_cache_types);

    if (stat("/config/nkn/attribute_update_hotness", &sb) == 0) {
	DBG_DM2W("Attribute hotness update will be logged in cache.log");
	glob_dm2_attr_update_log_hotness = 1;
    }

    /* Use -T option to run DM2 unit test */
    if (g_dm2_unit_test)
	DM2_unit_test_start(10, test_file, test_full_file, test_part_file, 0);

    glob_dm2_mem_max_bytes = nkn_max_dict_mem_MB * 1024 * 1024;
    glob_dm2_init_done = 1;

    if (glob_dm2_throttle_writes) {
	ret = dm2_attach_to_nsmartd();
	if (ret < 0) {
	    DBG_DM2S("Unable to attach to nsmartd counters");
	} else {
	    /* Get the list of drives to be throttled */
	    dm2_get_drive_throttle_list();
	}
    }

    dm2_start_reading_caches(num_cache_types);
    dm2_spawn_dm2_evict_thread();
    return 0;
}	/* DM2_init */

