/*
 *	COPYRIGHT: NOKEENA NETWORKS
 */
#ifndef NKN_LOCMGR2_EXTENT_H
#define NKN_LOCMGR2_EXTENT_H

#include <glib.h>
#include "nkn_defs.h"

/*
 * A physid must be unique across an entire system.  A block number/drive id
 * pair is unique.  We don't need the full 64 bits for the block number, so
 * we can squeeze the device id and provider id there.  It's important that
 * the device ids and provider ids be assigned at runtime, so it can be 
 * unique regardless of devices and providers present.  The device ids
 * and provider ids do not need to be persistent.
 *
 *   --------------------------------------------------------------------
 *   |			64 bits						|
 *   --------------------------------------------------------------------
 *   | 4 bits=providers | 8 bits=devices |    50 bits=sector number     |
 *   --------------------------------------------------------------------
 */
// XXXmiken: sectors are really blocks
#define NKN_PHYSID_SECTOR_BITS	50	// lots of sectors
#define NKN_PHYSID_DEV_BITS	8	// 256 devices
#define NKN_PHYSID_PROV_BITS	4	// 16 providers
#define NKN_PHYSID_SECTOR_MASK	(((1ULL) << NKN_PHYSID_SECTOR_BITS) - 1)
#define NKN_PHYSID_DEV_MASK	(((1ULL) << NKN_PHYSID_DEV_BITS) - 1)
#define NKN_PHYSID_PROV_MASK	(((1ULL) << NKN_PHYSID_PROV_BITS) - 1)
#define nkn_physid_to_sectornum(physid) ((physid) & NKN_PHYSID_SECTOR_MASK)
#define nkn_physid_to_device(physid)					\
    (((physid) >> NKN_PHYSID_SECTOR_BITS) & NKN_PHYSID_DEV_MASK)
#define nkn_physid_to_provider(physid)				    \
    (((physid) >> (NKN_PHYSID_SECTOR_BITS + NKN_PHYSID_DEV_BITS)) & \
     NKN_PHYSID_PROV_MASK)
#define nkn_physid_to_byteoff(physid, blksz)	\
    (((physid) & NKN_PHYSID_SECTOR_MASK) * (blksz))
#define nkn_physid_create(prov, dev_id, blkno)			    \
    ( (((prov) & NKN_PHYSID_PROV_MASK) <<			    \
       (NKN_PHYSID_SECTOR_BITS + NKN_PHYSID_DEV_BITS)) |	     \
      (((dev_id) & NKN_PHYSID_DEV_MASK) << NKN_PHYSID_SECTOR_BITS) | \
      ((blkno) & NKN_PHYSID_SECTOR_MASK))


/*
 * Flags for DM2_extent_t objects
 */
#define DM2_EXT_FLAG_INUSE	0x1
#define DM2_EXT_FLAG_CHKSUM	0x2

#define DM2_EXT_RDSZ_MULTIPLIER 1024
#define DM2_EXT_INIT_CONT_OFF	(uint32_t)(-1)
/*
 * Location manager extent
 *
 * This is used for in memory representation of location manager extents.
 * The uri_cont field is only filled when there is an open, unsynced block.
 *
 * ext_start_sector: How to find the real data by using the 512-byte sector
 *	offset into the raw partition.
 * ext_start_attr: How to find the real attribute data by using the 512-byte
 *	sector offset into the attribute file.
 * ext_cont_off: How to find this disk extent by using the 512-byte sector
 *	offset into the container file.
 */
struct DM2_uri;
typedef union dm2_ext_chksum {
    uint64_t ext_checksum;
    uint32_t ext_v2_checksum[8];                //
} dm2_ext_checksum_t;

typedef struct DM2_extent {
    struct DM2_uri *ext_uri_head;               // Back pointer
    uint64_t       ext_physid;                  // drive id/12b + block #/52b
    dm2_ext_checksum_t ext_csum;
    off_t          ext_start_sector;            // 64b sector offset into raw
    off_t          ext_offset;
    uint32_t       ext_length;
    uint32_t       ext_start_attr;              // attr sector offset in ext
    uint32_t       ext_cont_off;                // sector off in container
    uint16_t	   ext_read_size;		// read size in KB
    uint8_t        ext_flags;                   // inuse;
    uint8_t        ext_version;                 // version;
} DM2_extent_t;


#define DM2_DISK_EXT_MAGIC	0x44455854	// DEXT
#define DM2_DISK_EXT_DONE	0x444F4E45	// DONE  0x444f4e45
#define DM2_DISK_EXT_VERSION	1		// old version
#define DM2_DISK_EXT_VERSION_V2	2		// version 2
#define DM2_DISK_EXT_SIZE	DEV_BSIZE	// 512 bytes

/*
 * The version 3 change is done to take care of checksum performance issue.
 */
#define DM2_DISK_EXT_VERSION_V3 3               // current version

/*
 * Disk extents are present in a container and describe a single piece
 * of data which fits within a block (2MiB).
 *
 * After the container header, there will be a series of disk extents.
 * If an attribute is present, it will be placed immediately after the
 * disk extent.
 *
 * Note: If an extent on disk must be invalidated, I write DONE to the
 * sector in the magic number field.
 *
 */
typedef struct DM2_disk_extent_header {
    uint64_t	dext_my_checksum;		// this objects's checksum
    uint32_t	dext_magic;			// 0x44455854 "DEXT"
    uint8_t	dext_version;			// 1
    uint8_t	dext_attr_len;			// # of sectors in attr
    uint8_t	dext_flags;			// Flags
    uint8_t	dext_unused;
} DM2_disk_extent_header_t;

typedef struct DM2_disk_extent {
    DM2_disk_extent_header_t dext_header;
    uint64_t	dext_ext_checksum;		// this extent's checksum
    /* XXXmiken: Do we need physid on disk when we can calculate this? */
    uint64_t	dext_physid;			// dev id/12b + block #/52b
    uint64_t	dext_unused1;			// used to be mem ptr
    off_t	dext_offset;			// byte offset of disk ext
    off_t	dext_length;			// byte length of disk ext
    off_t	dext_start_sector;		// 64b sector offset into raw
    off_t	dext_start_attr;		// attr offset in extent
    uint8_t	dext_unused[32];		// 4 64-bit fields
    char	dext_basename[NAME_MAX+1];	// name_max = 255
} DM2_disk_extent_t;

/* New disk extent structure that stores 4 byte checksum's for 1/8th block size
 * of data. This is named as V2 */
typedef struct DM2_disk_extent_v2 {
    DM2_disk_extent_header_t dext_header;
    uint32_t    dext_ext_checksum[8];              // this extent's checksum
    /* XXXmiken: Do we need physid on disk when we can calculate this? */
    uint64_t    dext_physid;                    // dev id/12b + block #/52b
    uint64_t    dext_read_size;                   // minimum read size
    off_t       dext_offset;                    // byte offset of disk ext
    off_t       dext_length;                    // byte length of disk ext
    off_t       dext_start_sector;              // 64b sector offset into raw
    off_t       dext_start_attr;                // attr offset in extent
    uint8_t     dext_unused[8];			// 1 64-bit field
    char        dext_basename[NAME_MAX+1];      // name_max = 255
} DM2_disk_extent_v2_t;

/* Disk extent flags */
#define DM2_DISK_EXT_FLAG_CHKSUM	0x1

DM2_extent_t *dm2_find_extent_by_offset(GList *in_ext_list,
		const off_t uri_offset, const off_t len, off_t *tot_len,
		off_t *unavail_offset, int *object_with_hole);
DM2_extent_t *dm2_find_extent_by_offset2(GList *in_ext_list,
					 const off_t uri_offset);
#endif /* NKN_LOCMGR2_EXTENT_H */
