#ifndef NKN_MEDIAMGR_H
#define NKN_MEDIAMGR_H

#include "nkn_mediamgr_api.h" 
#include "nkn_defs.h"

//#define NKN_DM_MAX_CACHES	16
//#define NKN_DM_BLK_SIZE_KB	1000 /* In Kilobyes */
//#define NKN_DISK_PAGE_SZ	(200 * 1024)
//#define NKN_NUM_PAGE_PER_BLK	(NKN_DM_BLK_SIZE_KB / NKN_DISK_PAGE_SZ)

#define NKN_MM_DEFAULT_STACK_SZ (256 * 1024) /* Default Stack size in bytes */
#define NKN_MAX_CACHE_SIZE_KB	(250 * 1024 * 1024) /* In Kilobytes */

#define NKN_MAX_DISK_PER_TIER  256

#define NKN_MM_FULL_OBJ_IN_CACHE		0x0001
#define NKN_MM_TIER_SPECIFIC_QUEUE		0x0002
#define NKN_MM_INGEST_ABORT			0x0004
#define NKN_MM_0_OFFSET_CLI_0_OFFSET		0x0008
#define NKN_MM_0_OFFSET_CLI_NONZERO_OFFSET	0x0010
#define NKN_MM_MATCH_CLIENT_END			0x0020
#define NKN_MM_NONMATCH_CLIENT_END		0x0040
#define NKN_MM_UPDATE_CIM			0x0080
#define NKN_MM_PIN_OBJECT			0x0100
#define NKN_MM_CIM_UPDATE_EXPIRY		0x0200
#define NKN_MM_IGNORE_EXPIRY			0x0400
#define NKN_MM_HP_QUEUE				0x0800

/* Per disk Queue default values for other providers */
#define NKN_MM_DEF_NUM_DISK 1
#define NKN_MM_NO_DISK_ID 0

#define NKN_MOVEM_MAX_RETRY 2
#define NKN_MOVEM_RETRY_TIMEOUT 100

#define NKN_MAX_PROMOTES 100
#define NKN_MAX_SATA_TIER_PROMOTES 10
#define NKN_MAX_SAS_TIER_PROMOTES  10
#define NKN_MAX_SSD_TIER_PROMOTES  10
// For testing only, setting to 1MB
//#define NKN_MAX_CACHE_SIZE_KB (1024) /* In Kilobytes */

#define NKN_MAX_BLK_PER_CACHE	(NKN_MAX_CACHE_SIZE_KB / NKN_DM_BLK_SIZE_KB)

GThreadPool *mm_get_thread_pool[NKN_MM_MAX_CACHE_PROVIDERS][NKN_MAX_DISK_PER_TIER];
GThreadPool *mm_put_thread_pool[NKN_MM_MAX_CACHE_PROVIDERS];
GThreadPool *mm_stat_thread;
GThreadPool *mm_update_thread;
void test_mm_promote(void);

#endif /* NKN_MEDIAMGR_H */
