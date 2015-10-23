#ifndef NKN_DISKMGR2_API_H
#define NKN_DISKMGR2_API_H
/*
 * (C) Copyright 2008-2010 Nokeena Networks, Inc
 *
 * This file contains symbols necessary to interface with the DM2 module.
 *
 * Author: Michael Nishimoto
 *
 * Non-obvious coding conventions:
 *	- Symbols begin with dm2_, dm2_, or nkn_
 *	- All functions should begin with dm2_ or dm2_
 *	- All functions have a name comment at the end of the function.
 */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#ifndef __USE_GNU
#define __USE_GNU
#endif
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/user.h>		// NBPG
#ifdef HAVE_FLOCK
#  include <sys/file.h>
#endif

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "nkn_sched_api.h"
#include "nkn_mediamgr_api.h"
#include "nkn_diskmgr_common_api.h"
#include "nkn_namespace_stats.h"

#define NKN_MAX_BLOCK_SZ	(2 * 1024 * 1024)	/* bytes */
#define NKN_MIN_BLOCK_SZ	(1 * 1024 * 1024)	/* bytes */
#define DM2_DISK_BLKSIZE_2M	(2 * 1024 * 1024)
#define DM2_DISK_BLKSIZE_256K	(256 * 1024)

#define NKN_MAX_PAGE_SZ		(64 * 1024)		/* bytes */
#define NKN_MIN_PAGE_SZ		(16 * 1024)		/* bytes */
#define NKN_SECTOR_TO_BLOCK(sector, blksz) ((sector) / ((blksz)/DEV_BSIZE))
#define NKN_Byte2S(byte)	((byte) >> 9)
/* Don't need NKN_S2Byte because compiler will convert "* 512" to shift */


#define DM2_BITMAP_FNAME	"freeblks"
#define DM2_BITMAP_MAGIC	0xFEEDCAFE
#define DM2_BITMAP_VERSION	2
#define DM2_BITMAP_HEADER_SZ	4096
#define DM2_DISK_PAGESIZE	(32 * 1024)	// initial value
#define DM2_DISK_PAGESIZE_32K	(32 * 1024)
#define DM2_DISK_PAGESIZE_4K	(4 * 1024)

#define DM2_INITCACHE_CMD	"/opt/nkn/bin/initcache_dm2_ext3.sh"
#define DM2_INITROOTCACHE_CMD	"/opt/nkn/bin/initrootcache_dm2_ext3.sh"
#define DM2_FINDSLOT_CMD	"/opt/nkn/generic_cli_bin/findslottodcno.sh"

/* Macros for group read control */
#define DM2_GROUP_READ_DISABLE  0x00
#define DM2_GROUP_READ_ENABLE	0x01

/*
 * First block (4096 bytes) of a Disk Manager bitmap file
 */
typedef union dm2_bitmap_header {
    struct bmh_fields {
	uint32_t bmh_magic;		// 0xFEEDCAFE
	uint32_t bmh_version;		// 1 is latest
	uint32_t bmh_disk_blocksize;	// Disk block size
	uint32_t bmh_disk_pagesize;	// Disk page size
	uint64_t bmh_num_disk_pages;	// Number of disk pages
    } u;
    char fullblock[DM2_BITMAP_HEADER_SZ];
} dm2_bitmap_header_t;

/* Also used for reading in /config/nkn/diskcache-startup.info */
#define DM2_MAX_SERIAL_NUM	100	// Not sure the correct value here
#define DM2_MAX_RAWDEV		40
#define DM2_MAX_FSDEV		40
#define DM2_MAX_DISKNAME	40
#define DM2_MAX_STATNAME	52
#define DM2_MAX_MANAGER		20
#define DM2_MAX_MOUNTPT		40
#define DM2_MAX_TIERNAME	20
#define DM2_MAX_MGMTNAME	20
#define DM2_MAX_OWNERNAME	8

/* Used in mu_updated_fields to indicate which fields hold values */
#define DM2_MGMT_MOD_ACTIVATE_START	0x00000001
#define DM2_MGMT_MOD_ACTIVATED		0x00000001	// XXXmiken: temporary
#define DM2_MGMT_MOD_ACTIVATE_FINISH	0x00000002
#define DM2_MGMT_MOD_CACHE_ENABLED	0x00000004
#define DM2_MGMT_MOD_COMMENTED_OUT	0x00000008
#define DM2_MGMT_MOD_MISSING_AFTER_BOOT 0x00000010
#define DM2_MGMT_MOD_SET_SECTOR_CNT	0x00000020
#define DM2_MGMT_MOD_RAW_PARTITION	0x00000040
#define DM2_MGMT_MOD_FS_PARTITION	0x00000080
#define DM2_MGMT_MOD_AUTO_REFORMAT	0x00000100
#define DM2_MGMT_MOD_PRE_FORMAT         0X00000200
#define DM2_MGMT_MOD_POST_FORMAT	0X00000400

typedef struct dm2_mgmt_db_info {
    uint32_t	mi_updated_fields;
    char	mi_serial_num[DM2_MAX_SERIAL_NUM];
    char	mi_raw_partition[DM2_MAX_RAWDEV];
    char	mi_fs_partition[DM2_MAX_FSDEV];
    char	mi_diskname[DM2_MAX_DISKNAME];
    char	mi_statname[DM2_MAX_STATNAME];
    char	mi_provider[DM2_MAX_TIERNAME];
    char	mi_mount_point[DM2_MAX_MOUNTPT];
    char	mi_owner[DM2_MAX_OWNERNAME];
    size_t	mi_set_sector_cnt;	// 64bits
    uint32_t	mi_ioerror;		// zero indicates no error
    u_int8_t	mi_activated;		// boolean (0 or 1)
    u_int8_t	mi_cache_enabled;	// boolean (0 or 1)
    u_int8_t	mi_commented_out;	// boolean (0 or 1)
    u_int8_t	mi_missing_after_boot;	// boolean (0 or 1)
    u_int8_t	mi_auto_reformat;	// boolean (0 or 1)
    u_int8_t	mi_small_block_format;	// boolean (0 or 1)
} dm2_mgmt_db_info_t;

/*
 * Used for dm2_mgmt_disk_status->status
 *
 * Normal transition state:
 *	DEACTIVATED -> ACTIVATED -> CACHEABLE -> CACHE_RUNNING
 *
 * - DEACTIVATED: We don't know if the device exists
 *   - IMPROPER_UNMOUNT: [Error state] umount failed during deactivate
 * - ACTIVATED: We can stat the device
 *   - INVAL_FORMAT: [Error state] wrong format, not MFD format
 *   - IMPROPER_MOUNT: [Error state] mount failed while verifying format
 *   - FORMAT_UNKNOWN: [Error state] wrong format after mount, not MFD format
 * - CACHEABLE: We can see a 'freeblks' file in the root
 * - CACHE_RUNNING: We have successfully read 'freeblks'
 *
 * States which mean we don't attempt to look at the disk
 * - COMMENTED_OUT
 * - NOT_PRESENT
 *
 */
typedef enum dm2_mgmt_state {
    DM2_MGMT_STATE_NULL = 0,
    DM2_MGMT_STATE_UNKNOWN = 1,
    DM2_MGMT_STATE_NOT_PRESENT	= 2,
    DM2_MGMT_STATE_CACHEABLE = 3,
    DM2_MGMT_STATE_INVAL_FORMAT_BEFORE_MOUNT = 4,
    DM2_MGMT_STATE_COMMENTED_OUT = 5,
    DM2_MGMT_STATE_DEACTIVATED = 6,
    DM2_MGMT_STATE_ACTIVATED = 7,	/* found disk but no MFD content */
    DM2_MGMT_STATE_IMPROPER_UNMOUNT = 8,
    DM2_MGMT_STATE_IMPROPER_MOUNT = 9,
    DM2_MGMT_STATE_CACHE_RUNNING = 10,
    DM2_MGMT_STATE_BITMAP_BAD = 11,
    DM2_MGMT_STATE_FORMAT_UNKNOWN_AFTER_MOUNT = 12,
    DM2_MGMT_STATE_BITMAP_NOT_FOUND = 13,
    DM2_MGMT_STATE_CONVERT_FAILURE = 14,
    DM2_MGMT_STATE_WRONG_VERSION = 15,
    DM2_MGMT_STATE_CACHE_DISABLING = 16,
    DM2_MGMT_STATE_MAX = 16,
} dm2_mgmt_state_t;

extern const char *dm2_mgmt_state_strings[];

typedef struct dm2_mgmt_disk_status {
    char	ms_name[DM2_MAX_MGMTNAME];
    char	ms_tier[DM2_MAX_TIERNAME];
    uint32_t	ms_state;
    int8_t	ms_not_production;	// Set if no serial # found
} dm2_mgmt_disk_status_t;

/* We can have unit tests which call the functions directly */
int  DM2_attribute_update(MM_update_attr_t *update_attr);
void DM2_cleanup(void);
int  DM2_delete(MM_delete_resp_t *delete);
int  DM2_discover(struct mm_provider_priv *p);
void DM2_promote_complete(MM_promote_data_t *dm2_data);
int  DM2_get(MM_get_resp_t *get);
int  DM2_provider_stat (MM_provider_stat_t *pstat);
int  DM2_put(struct MM_put_data *a_info, video_types_t video_type);
int  DM2_stat(nkn_uol_t uol, MM_stat_resp_t *in_ptr_resp);
int  DM2_shutdown(void);

int DM2_find_cache_running_state(int num, ns_stat_category_t category);
void DM2_ns_set_rp_size_for_namespace(ns_stat_category_t cat,
				      uint32_t rp_index,
				      uint64_t total);
int DM2_mgmt_ns_add_namespace(namespace_node_data_t *tm_ns_info,
			      int rp_index);
int DM2_mgmt_ns_modify_namespace(const char *ns_uuid,
				 int new_rp_index);
int DM2_mgmt_ns_delete_namespace(const char *ns_uuid);
int DM2_mgmt_ns_set_cache_pin_max_obj_size(const char *ns_uuid,
					   int64_t max_bytes);
int DM2_mgmt_ns_set_cache_pin_diskspace(const char *ns_uuid,
					int64_t resv_bytes);
int DM2_mgmt_ns_set_cache_pin_enabled(const char *ns_uuid, int enabled);

int DM2_mgmt_ns_set_tier_max_disk_usage(const char *ns_uuid,
					const char *tier_name,
					size_t	    size_in_mb);
int DM2_mgmt_ns_set_tier_group_read(const char *ns_uuid,
				    const char *tier_name,
				    int        enabled);
int DM2_mgmt_ns_set_tier_read_size(const char *ns_uuid,
				   const char *tier_name,
				   uint32_t    read_size);
int DM2_mgmt_ns_set_tier_block_free_threshold(const char *ns_uuid,
					      const char *tier_name,
					      uint32_t   prcnt);
int DM2_mgmt_remove_disk(const char *mgmt_name);
int DM2_mgmt_TM_DB_update_msg(const char *msmt_name __attribute((unused)),
			      const uint32_t update_field __attribute((unused)),
			      const void *update_value __attribute((unused)),
			      char  *out_buffer);
int DM2_mgmt_get_diskcache_info(const dm2_mgmt_db_info_t *obj,
				dm2_mgmt_disk_status_t   *status);
int DM2_mgmt_get_internal_water_mark(const char *tier_name,
				     int32_t *high_water_mark,
				     int32_t *low_water_mark);
int DM2_mgmt_set_internal_water_mark(const char *tier_name,
				     const int32_t high_water_mark,
				     const int32_t low_water_mark);
int DM2_mgmt_set_time_evict_config(const char *tier_name,
				   const int32_t time_low_water_mark,
				   const int32_t hour,
                                   const int32_t min);
int DM2_mgmt_set_internal_percentage(const char *tier_name,
				     const int32_t prcnt);
int DM2_mgmt_set_free_block_threshold(const char *tier_name,
				      const int32_t prcnt);
int DM2_mgmt_get_free_block_threshold(const char *tier_name,
				      int32_t *prcnt);
int DM2_mgmt_set_block_share_threshold(const char *tier_name,
				       const int32_t prcnt);
int DM2_mgmt_get_block_share_threshold(const char *tier_name,
				      int32_t *prcnt);
int DM2_mgmt_config_group_read(const char *tier_name, int mode);
void DM2_mgmt_stop_preread(void);
void DM2_mgmt_update_am_thresholds(void);
int DM2_mgmt_set_dict_evict_water_mark(int hwm, int lwm);
int DM2_mgmt_set_write_size_config(int size);
#endif	/* NKN_DISKMGR2_API_H */
