#ifndef NKN_DISKMGR2_DISK_API_H
#define NKN_DISKMGR2_DISK_API_H
/*
 * (C) Copyright 2008 Nokeena Networks, Inc
 */

#include <sys/param.h>		// roundup, NBBY
#include <glib.h>
#include <atomic_ops.h>
#include "nkn_debug.h"
#include "nkn_mediamgr_api.h"
#include "nkn_diskmgr2_api.h"
#include "nkn_locmgr2_extent.h"
#include "nkn_locmgr2_attr.h"

#define DM2_CONVERSION_SCRIPT "/opt/nkn/bin/cache_convert.sh"
#define DM2_CONT_BKP_SCRIPT "/opt/nkn/bin/container_bkp.py"

#define Byte2DP_pgsz(len, pgsz) (((len) + (pgsz) - 1) / (pgsz))
#define Byte2DP_cont(len, cont) \
    (Byte2DP_pgsz(len, cont->c_open_dblk.db_disk_page_sz))

/* Drive types used in initdisks.sh */
#define DM2_TYPE_SAS	 "SAS"
#define DM2_TYPE_SATA	 "SATA"
#define DM2_TYPE_SSD	 "SSD"
#define DM2_TYPE_UNKNOWN "TYPE_UNKNOWN"

/*
 * Disk manager free bitmap object
 */
typedef struct dm2_bitmap {
    pthread_mutex_t bm_mutex;
    char	bm_fname[PATH_MAX];	// filename of bitmap for this drive
    char	bm_devname[256];	// device name for this drive
    char	*bm_buf;		// aligned buffer of bitmap
    GList	*bm_free_page_list;	// sorted by longest first
    GList	*bm_partial_blk_list;	// list of partial blocks
    GList	*bm_free_blk_list;	// list of whole blocks
    char	*bm_free_blk_map;	// array of bitmaps for blocks
    uint64_t	bm_num_page_free;	// # of free pages
    uint64_t	bm_num_blk_free;	// # of free blocks
    AO_t	bm_num_resv_blk_free;	// # of reserved free blocks
    int		bm_fd;			// fd of bitmap; holding file open
    uint32_t	bm_fsize;		// current size of file
    uint32_t	bm_bufsize;		// current size of buffer
    uint32_t	bm_pages_per_block;	// helpful calculated value
    uint32_t	bm_blk_map_size;	// current size of blk_map
    int		bm_byte2block_bitshift;
} dm2_bitmap_t;

/*
 * One extent in the free bitmap list.  This describes (offset, length)
 * pairs of disk block pages.
 */
typedef struct dm2_freelist_ext {
    off_t  pagestart;
    size_t numpages;
} dm2_freelist_ext_t;


/*
 * The block range is described in sector terms.  Blocks are the unit which
 * the disk manager reads from disk.  A block is made up of multiple disk
 * pages which is the disk unit that maps to smaller objects.  DM will
 * manage its free space in terms of disk pages.  Buffer pages and disk
 * pages do not need to be the same size.  However, buffer pages can
 * never be larger than disk pages because single buffer pages can only
 * refer to a single object or part of a single object.
 */
#define DM2_DB_ACTIVE	0x1	// Is this disk block currently valid?
#define DM2_DB_DELETE	0x2	// Currently deleting URIs in block
#define DM2_DB_NOSHARE  0x4	// Don't share this block

typedef struct dm2_disk_block {
    pthread_mutex_t db_mutex;
    off_t	    db_start_page;
    uint32_t	    db_num_pages;
    uint32_t	    db_disk_page_sz;
    uint32_t	    db_num_free_disk_pages;
    uint32_t	    db_flags;
    uint8_t	    db_used[roundup(NKN_MAX_BLOCK_SZ, NKN_MIN_PAGE_SZ)/NKN_MIN_PAGE_SZ / NBBY];
} dm2_disk_block_t;

#define DM2_DISK_BLOCK_INITED(dbptr) ((dbptr)->db_disk_page_sz != 0)

#define NKN_CONTAINER_MAGIC		0x434F4E54	// "CONT"
#define NKN_CONTAINER_MAGIC_FREE	0x46524545	// "FREE"
#define NKN_CONTAINER_NAME		"container"
#define NKN_CONTAINER_HEADER_SZ		(2 * 4096)
#define NKN_CONTAINER_MAX_BLK_ALLOC	9		// Will be n+1
#define NKN_CONTAINER_ALLOC_CHUNK_SZ	(32*1024)

#define NKN_CONTAINER_ATTR_NAME_V1	"attributes"
#define NKN_CONTAINER_ATTR_NAME		"attributes-2"

/*
 * dm2_container_t
 *
 * On-disk structure which represents an entire video.  For a Break style
 * video, this means there is one video per container.  For a Move style
 * video, there could be (# of clips per profile) x (# of profiles).  This
 * number could be as high as 28,000.
 *
 * Header is defined to be everything before the extents, so the header
 * is 8KiB.  header_pathname is not put in the first 4KiB just in case we
 * get a long path.  Disk space is not a critical resource.  This also
 * leaves room to put other objects before the pathname.  The checksum
 * in the container_disk_header_t is over the entire 8KiB header.
 *
 * After the header, we have one DM2_disk_extent_t for each extent.  This
 * structure should consume one sector.  One sector was chosen because it
 * will allow easy seek/update semantics.  Since we don't expect to be
 * writing to RAID, sector updates should not have a read-modify-write
 * penalty.
 */
typedef struct container_disk_header {
    uint64_t	    cdh_checksum;	// over header except checksum
    uint32_t	    cdh_magicnum;	// "CONT"
    uint32_t	    cdh_version;
    uint32_t	    cdh_cont_sz;	// sector multiples
    uint32_t	    cdh_num_exts;
    uint32_t	    cdh_obj_sz;		// Size of each extent
} container_disk_header_t;

#define DM2_CONT_RFLAG_VALID		0x00000001
#define DM2_CONT_RFLAG_EXTS_READ	0x00000002
#define DM2_CONT_RFLAG_ATTRS_READ	0x00000004
#define DM2_CONT_RWFLAG_WLOCK		0x00000008	// DEBUG flag

#define NKN_IS_CONT_VALID(c)	\
    (((c)->c_rflags & DM2_CONT_RFLAG_VALID) == DM2_CONT_RFLAG_VALID)
#define NKN_CONT_EXTS_READ(c)	\
    (((c)->c_rflags & DM2_CONT_RFLAG_EXTS_READ) == DM2_CONT_RFLAG_EXTS_READ)
#define NKN_CONT_ATTRS_READ(c)	\
    (((c)->c_rflags & DM2_CONT_RFLAG_ATTRS_READ) == DM2_CONT_RFLAG_ATTRS_READ)
/* Container is not initialized */
#define DM2_VIRGIN_CONT(c) \
    ((c)->c_open_dblk.db_disk_page_sz == 0)
#define DM2_CONT_OPEN_BLOCK_DELETED(c) \
    (((c)->c_open_dblk.db_flags & DM2_DB_DELETE) != 0)
#define DM2_CONT_OPEN_BLOCK_NOSHARE(c) \
    (((c)->c_open_dblk.db_flags & DM2_DB_NOSHARE) != 0)

#define DM2_CONT_NEW_SPACE_SAME_BLOCK 1
#define DM2_CONT_NEW_SPACE_SAME_DISK 2
#define DM2_CONT_NEW_SPACE_NEW_DISK 3

struct dm2_cache_info;
typedef struct container_runtime_header {
    pthread_rwlock_t	  crh_rwlock;	   // control access to container file
    void		  *crh_barrier;    // Don't move.
    pthread_mutex_t	  crh_amutex;	   // control access to attr file
    pthread_mutex_t	  crh_slot_mutex;  // control access to slot Q fields
    char		  *crh_uri_dir;	   // points to ch->ch_uri_dir
    struct dm2_cache_info *crh_ci;	   // point to specific disk
    GQueue		  crh_ext_slot_q;
    GQueue		  crh_attr_slot_q;
    time_t		  crh_last_update; // time for last update
    AO_t		  crh_good_ext_cnt;// number of good exts in container
    uint32_t		  crh_rflags;
    uint32_t		  crh_blksz;	   // Copy from bitmap header
    int32_t		  crh_blk_allocs;  // Init to value, then use up
    dm2_disk_block_t	  crh_open_dblk;   // present if DM_DB_ACTIVE flag set
    int16_t		  crh_cont_mem_sz; // sizeof(dm2_container_t)
    int8_t		  crh_created;
    int8_t		  crh_new_space;
    int8_t		  crh_run;	   // num of continuous blocks
} container_runtime_header_t;

#define DM2_MAX_SZ_CONT_DISK_HEAD_T	(2*1024)

union container_head_block {
    struct {
	union {
	    container_disk_header_t dh;
	    char disk_header_space[2*1024];
	} save_header;
    } d;
    char fullblock_padding[4096];
};
typedef union container_head_block container_head_block_t;


/*
 *
 */
struct DM2_disk_extent;
typedef struct dm2_disk_container {
    container_head_block_t hb;
    char hc_pathname[PATH_MAX];		// pathname to container PATH_MAX=4096
    /*
     * Runtime fields:
     * - pointer to current partial block
     */
} dm2_disk_container_t;

typedef struct dm2_container {
    container_disk_header_t dh;
    container_runtime_header_t rh;
} dm2_container_t;

typedef struct dm2_container_head {
    GList		*ch_cont_list;
    char		*ch_uri_dir;	   // hash key
    uint64_t		ch_last_cont_read_ts; // time a container was last read
					   // for this cont_list
    int			ch_uri_dir_len;

    pthread_cond_t	ch_rwlock_read_cond;  // allocated when needed 48B
    pthread_cond_t	ch_rwlock_write_cond; // allocated when needed 48B
    uint32_t		ch_rwlock_wowner;     // thread id owning write lock
    uint16_t		ch_rwlock_nreaders;   // # thr with read lock
    uint16_t		ch_rwlock_nwriters;   // # thr with write lock
    uint16_t		ch_rwlock_wait_nreaders; // # thr waiting for read lock
    uint16_t		ch_rwlock_wait_nwriters;
    uint8_t		ch_rwlock_lock_type;
} dm2_container_head_t;

#define cd_checksum	hb.d.save_header.dh.cdh_checksum
#define cd_magicnum	hb.d.save_header.dh.cdh_magicnum
#define cd_version	hb.d.save_header.dh.cdh_version
#define cd_cont_sz	hb.d.save_header.dh.cdh_cont_sz
#define cd_num_exts	hb.d.save_header.dh.cdh_num_exts
#define cd_obj_sz	hb.d.save_header.dh.cdh_obj_sz

#define c_checksum	dh.cdh_checksum
#define c_magicnum	dh.cdh_magicnum
#define c_version	dh.cdh_version
#define c_cont_sz	dh.cdh_cont_sz
#define c_num_exts	dh.cdh_num_exts
#define c_obj_sz	dh.cdh_obj_sz

#define c_rwlock	rh.crh_rwlock
#define c_barrier	rh.crh_barrier
#define c_amutex	rh.crh_amutex
#define c_slot_mutex	rh.crh_slot_mutex
#define c_uri_dir	rh.crh_uri_dir
#define c_dev_ci	rh.crh_ci
#define c_ext_slot_q	rh.crh_ext_slot_q
#define c_attr_slot_q	rh.crh_attr_slot_q
#define c_last_update	rh.crh_last_update
#define c_good_ext_cnt	rh.crh_good_ext_cnt
#define c_rflags	rh.crh_rflags
#define c_blksz		rh.crh_blksz
#define c_blk_allocs	rh.crh_blk_allocs
#define c_open_dblk	rh.crh_open_dblk
#define c_cont_mem_sz   rh.crh_cont_mem_sz
#define c_created	rh.crh_created
#define c_new_space	rh.crh_new_space
#define c_run		rh.crh_run

#define DM2_URI_NOT_LOCKED 0
#define DM2_URI_RLOCKED 1
#define DM2_URI_WLOCKED 2

#define DM2_URI_LOG_OP_MAX 8

#define DM2_URI_OP_STAT 0x01
#define DM2_URI_OP_PUT  0x02
#define DM2_URI_OP_GET  0x04
#define DM2_URI_OP_DEL  0x08
#define DM2_URI_OP_ATTR 0x10
#define DM2_URI_OP_PRE  0x20
#define DM2_URI_OP_CRR  0x40
#define DM2_URI_OP_EVICT 0x80

#define DM2_URI_NLOCK  1
#define DM2_URI_RLOCK  2
#define DM2_URI_WLOCK  3

typedef struct uri_dbg {
    uint16_t uri_log_ts;
    uint16_t uri_wowner;
    uint8_t uri_lastop;
    uint8_t uri_flags;
    uint8_t uri_errno;
} uri_dbg_t;

/*
 * The URI locks would be a separate pool and on start of an operation each URI
 * needs to be associated to a lock. The URI Lock Hash table will help in
 * picking up the lock for an URI when the association exists. When the
 * operation completes, the assocaiation is broken and the lock returned to the
 * free lock pool.
 */
typedef struct uri_lock {
    pthread_cond_t	*uri_read_cond;		// allocated when needed 48B
    pthread_cond_t	*uri_write_cond;	// allocated when needed 48B
    uint32_t		uri_wowner;		// thread id owning write lock
    uint16_t		uri_nreaders;		// # thr with read lock
    uint16_t		uri_nwriters;		// # thr with write lock
    uint16_t		uri_wait_nreaders;	// # thr waiting for read lock
    uint16_t		uri_wait_nwriters;	// # thr waiting for write lock
    uint8_t		in_use;
    uint8_t		uri_log_idx;
    STAILQ_ENTRY(uri_lock) entries;
    uri_dbg_t		uri_log[DM2_URI_LOG_OP_MAX];
} dm2_uri_lock_t;


#define DM2_URI_NOT_LOCKED 0
#define DM2_URI_RLOCKED 1
#define DM2_URI_WLOCKED 2

typedef struct DM2_uri {
    nkn_objv_t		uri_version;		// opaque object version
    char		*uri_name;
    GList		*uri_ext_list;		// existing extents
    dm2_container_t	*uri_container;		// pointer to container
    dm2_uri_lock_t	*uri_lock;
    dm2_disk_block_t	*uri_open_dblk;		// open disk block for this URI
    nkn_hot_t		uri_hotval;
    off_t		uri_at_off;		// attribute offset
    uint16_t		uri_at_len;
    uint16_t		uri_blob_at_len;
    /* expiry fields were time_t, but they only need to be 32 bits.
     * changed the next 3 fields to 32 bits to save 12 bytes, but they will
     * run out on year 2038 */
    uint32_t		uri_expiry;		// when object expires
    uint32_t		uri_time0;		// when object 1st becomes valid
    uint32_t		uri_cache_create;

    /* these two fields together occupy 8 bytes */
    uint64_t		uri_content_len:40;	// length of object
    uint64_t		uri_name_len:16;	// malloc len for uri_name

    /* rest of the fields occupy 8 bytes */
    uint64_t		uri_resv_len:40;
    uint64_t		uri_obj_type:2;		// flag to indicate object type
						// 0x00 - obj with content len
						// 0x01 -obj without content len
    uint64_t		uri_lock_type:2;	// need 2 bits
    uint64_t		uri_deleting:2;		// debug
    uint64_t		uri_bad_delete:1;	// need 1 bit
    uint64_t		uri_chksum_err:1;	// need 1 bit
    uint64_t		uri_resvd:1;		// need 1 bit
    uint64_t		uri_evict_list_add:1;	// added to evict list - 1 bit
    uint64_t		uri_cache_pinned:1;	// need 1 bit
    /* Debug fields */
    uint64_t		uri_put_err:1;		// error during PUT
    uint64_t		uri_initing:1;		// uri inited
    uint64_t		uri_invalid:1;		// uri inserted

    /* Hotness of the object without any decay */
    uint64_t		uri_orig_hotness:8;
    /* TAILQ for the evict list */
    TAILQ_ENTRY(DM2_uri) evict_entries;
} DM2_uri_t;

int nkn_debug_group;
int nkn_debug_level;

void dm2_cache_invalidate(int devid);
int dm2_create_directory(char *container_path, int isdir);
char *dm2_get_cache_dir(int find_drive);
int dm2_read_single_attr(MM_get_resp_t		    *get,
			 const nkn_provider_type_t  ptype,
			 DM2_uri_t                  *uri_head,
			 GList                      *ext_list_head,
			 char			    *ext_uri,
			 int			    read_single);
int dm2_set_read_contlist(GList *contlist);
int dm2_stampout_attr(DM2_uri_t *uri_head);
int dm2_stampout_extents(DM2_uri_t *uri_head);

DM2_uri_t *dm2_extent_get_by_uri(const char *in_uri);
int cmp_smallest_dm2_extent_offset_first(gconstpointer in_a,
					 gconstpointer in_b);
int cmp_smallest_freeext_offset_first(gconstpointer a, gconstpointer b);
int cmp_largest_freeext_first(gconstpointer a, gconstpointer b);

#endif /* NKN_DISKMGR2_DISK_API_H */
