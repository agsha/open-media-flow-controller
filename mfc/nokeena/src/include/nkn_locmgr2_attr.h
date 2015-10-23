#ifndef NKN_LOCMGR2_ATTR_H
#define NKN_LOCMGR2_ATTR_H
/*
 * (C) Copyright 2008 Nokeena Networks, Inc
 */

#define DM2_ATTR_MAGIC		0x41545452	// 'ATTR'
#define DM2_ATTR_V1_MAGIC	0x41545452	// 'ATTR'
#define DM2_ATTR_MAGIC_GONE	0x474F4E45	// 'GONE'  0x474f4e45
#define DM2_ATTR_V1_MAGIC_GONE	0x474F4E45	// 'GONE'  0x474f4e45
#define DM2_ATTR_VERSION	2		// Current version
#define DM2_ATTR_V1_VERSION	1		// Current version
#define DM2_ATTR_INIT_OFF	((off_t)(-1))
#define DM2_ATTR_INIT_LEN	((uint16_t)(-1))
/* Max size of nkn_attr_t on disk; BM can allocate larger objects */
#define DM2_MAX_ATTR_DISK_SIZE	(4*1024)
#define DM2_MAX_ATTR_DISK_SECS	8
/* Total disk size of each attribute, including the header */
#define DM2_ATTR_TOT_DISK_SIZE	(DM2_MAX_ATTR_DISK_SIZE + DEV_BSIZE)
#define DM2_ATTR_TOT_DISK_SECS	9
#define DM2_ATTR_MAX_CHUNKS	16

typedef struct DM2_disk_attr_desc_v1 {
    uint32_t	dat_magic;			// 'ATTR'
    uint8_t	dat_version;
    uint8_t	dat_unused1[3];
    uint32_t	dat_count;
    char	dat_basename[NAME_MAX+1];
} DM2_disk_attr_desc_v1_t;

/* Should be less than 1 sector in length */
typedef struct DM2_disk_attr_desc {
    uint32_t	dat_magic;			// 'ATTR'
    uint8_t	dat_version;			// Current version
    uint8_t	dat_unused1;			// alignment
    uint16_t	dat_len;			// Length of basename
    uint64_t	dat_unused2[2];			// just in case
    char	dat_basename[NAME_MAX+1];
} DM2_disk_attr_desc_t;

#endif /* NKN_LOCMGR2_ATTR_H */
