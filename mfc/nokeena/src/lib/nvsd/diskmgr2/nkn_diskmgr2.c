/*
 * (C) Copyright 2008 Nokeena Networks, Inc
 *
 * This file contains code which implements the Disk Manager
 *
 * Author: Michael Nishimoto
 *
 * Non-obvious coding conventions:
 *	- All functions should begin with dm2_
 *	- All functions have a name comment at the end of the function.
 *	- All log messages use special logging macros
 *
 */
#include <sys/types.h>
#include <sys/param.h>
#include <unistd.h>
#include <glib.h>

#include "zvm_api.h"

#include "nkn_defs.h"
#include "nkn_util.h"
#include "nkn_assert.h"
#include "nkn_am_api.h"
#include "nkn_sched_api.h"
#include "nkn_cod.h"

#include "nkn_diskmgr2_local.h"
#include "diskmgr2_locking.h"
#include "nkn_diskmgr_intf.h"
#include "nkn_diskmgr2_disk_api.h"
#include "nkn_locmgr2_uri.h"
#include "nkn_locmgr2_extent.h"
#include "nkn_locmgr2_container.h"
#include "nkn_locmgr2_physid.h"
#include "nkn_locmgr2_attr.h"
#include "nkn_hash.h"
#include "nkn_namespace_stats.h"
#include "diskmgr2_common.h"
#include "diskmgr2_util.h"

int64_t NOZCC_DECL(glob_dm2_ext_read_cnt),
    NOZCC_DECL(glob_dm2_ext_read_bytes),
    glob_dm2_int_evict_cnt,
    glob_dm2_int_evict_err,
    glob_dm2_delete_no_ext,
    glob_dm2_server_busy_err,
    glob_dm2_conthead_race_cnt,
    glob_dm2_conthead_race_get_contlist1_cnt,
    glob_dm2_conthead_race_get_contlist2_cnt,
    glob_dm2_cont_insert_err,
    glob_dm2_cont_exists_cnt,
    glob_dm2_empty_cont_race,
    glob_dm2_partial_extent_eexit_cnt,
    glob_dm2_put_no_attr;

extern int64_t NOZCC_DECL(glob_dm2_alloc_uri_head),
    NOZCC_DECL(glob_dm2_alloc_uri_head_uri_name),
    NOZCC_DECL(glob_dm2_alloc_uri_head_open_dblk),
    glob_dm2_put_bad_offset_err,
    glob_dm2_free_uri_head,
    glob_dm2_free_uri_head_uri_name,
    glob_dm2_put_mkdir_err,
    NOZCC_DECL(glob_dm2_quick_put_not_found),
    glob_dm2_put_no_meta_space_err;

extern uint64_t glob_malloc_mod_dm2_container_t,
    glob_malloc_mod_DM2_uri_t,
    glob_malloc_mod_dm2_uri_name_t,
    glob_malloc_mod_DM2_extent_t,
    glob_malloc_mod_dm2_preread_cont_name;

void dm2_print_mem_extent(void *value);
void dm2_dump_counters(void);

static int dm2_container_insert(const char *uri_dir, dm2_container_t *cont,
				dm2_cache_type_t *ct,
				dm2_container_head_t **cont_head,
				int *ch_locked);


void
dm2_dump_counters(void)
{
    printf("glob_dm2_ext_read_cnt: %ld\n", glob_dm2_ext_read_cnt);
    printf("glob_dm2_ext_read_bytes: %ld\n", glob_dm2_ext_read_bytes);
}


/*
 * Return a positive number if the first argument should be sorted after
 * the second argument.
 */
int
cmp_largest_freeext_first(gconstpointer a,
			  gconstpointer b)
{
    dm2_freelist_ext_t *x = (dm2_freelist_ext_t *)a;
    dm2_freelist_ext_t *y = (dm2_freelist_ext_t *)b;

    if (x->numpages < y->numpages)
	return 1;
    else
	return 0;
}	/* cmp_largest_freeext_first */

int
cmp_smallest_freeext_offset_first(gconstpointer a,
				  gconstpointer b)
{
    dm2_freelist_ext_t *x = (dm2_freelist_ext_t *)a;
    dm2_freelist_ext_t *y = (dm2_freelist_ext_t *)b;

    if (x->pagestart > y->pagestart)
	return 1;
    else
	return 0;
}	/* cmp_smallest_freeext_offset_first */


/*
 * Allocate a free list extent object and insert it to the list.
 */
static int
dm2_insert_free_ext(dm2_bitmap_t *bm,
		   const uint32_t max_blocks,
		   const off_t	 pagestart,
		   const off_t	 numpages)
{
    off_t	new_start, new_pages;
    uint64_t	block_num;
    int		blk_idx;
    unsigned char  *free_blk_map;

    /* Align the start of the extent to a block boundary */
    new_start = ROUNDUP_PWR2(pagestart, bm->bm_pages_per_block);
    new_pages = numpages - (new_start - pagestart);

    /* Cut-off pages at the end of the extent, if it is unaligned */
    if (new_pages % bm->bm_pages_per_block)
	new_pages -= new_pages % bm->bm_pages_per_block;

    /* Let's not add any extent which is too small */
    if (new_pages < bm->bm_pages_per_block) {
	DBG_DM2M3("FreeBitMap Discard (%s): pagestart=%lu numpages=%lu "
		  "total=%lu",
		  bm->bm_fname, pagestart, numpages, bm->bm_num_page_free);
	return 0;
    }

    while(new_pages) {
	/* Depending on what the user has set, shrink the number of useable
	 * pages. Normally, this functionality should never be used; however,
	 * it is useful for testing when trying to hit out-of-space conditions.
	 */
	block_num = new_start / bm->bm_pages_per_block;
	if (block_num >= max_blocks){
	    new_pages -= bm->bm_pages_per_block;
	    new_start +=  bm->bm_pages_per_block;
	    continue;
	}
	free_blk_map = (unsigned char *)&bm->bm_free_blk_map[block_num/8];
        blk_idx = block_num % 8;
	*free_blk_map |= (1 << blk_idx);
	/* add the block to the list and mark it as available */
	bm->bm_free_blk_list = g_list_prepend(bm->bm_free_blk_list,
					     (void *)block_num);
	bm->bm_num_blk_free++;
	/* Update no of free pages after adding free block to free list*/
	bm->bm_num_page_free += bm->bm_pages_per_block;
	new_pages -= bm->bm_pages_per_block;
	new_start +=  bm->bm_pages_per_block;
    }

    DBG_DM2M2("FreeBitMap (%s): pagestart=%lu numpages=%lu total=%lu",
		bm->bm_fname, pagestart, numpages, bm->bm_num_page_free);

    return 0;
}	/* dm2_insert_free_ext */


/*
 * Read in free space bitmap from disk
 *
 * Format of bitmap file:
 * - First 4KiB is header information
 * - Rest of file contains 1 bit per 1MB block
 *   - value = 0: block not used
 *   - value = 1: block is used
 */
int
dm2_bitmap_read_free_pages(dm2_cache_info_t *ci,
			   const int8_t cache_enabled)
{
    struct stat		sb;
    dm2_bitmap_header_t	*bmh;
    dm2_bitmap_t	*bm = &ci->bm;
    uint8_t		*pbits, bits;
    char		*in_mountpt = ci->ci_mountpt;
    off_t		pagestart, numpages, byteno;
    size_t		bmsize;
    uint32_t		sector_blksz, max_blocks;
    int			ret, nbytes, filling, b, bitsleft, macro_ret;

    NKN_ASSERT(bm != NULL);
    NKN_ASSERT(in_mountpt != NULL);

    memset(bm, 0, sizeof(*bm));
    ret = snprintf(bm->bm_fname, PATH_MAX-1, "%s/%s", in_mountpt,
		   DM2_BITMAP_FNAME);
    if (ret > PATH_MAX-1) {
	DBG_DM2S("ERROR: Pathname too long (dir=%s): %d", in_mountpt, ret);
	return -ENAMETOOLONG;
    }
    if (stat(bm->bm_fname, &sb) < 0) {
	DBG_DM2S("ERROR: Failed to open %s: %d", bm->bm_fname, errno);
	return -NKN_DM2_BITMAP_NOT_FOUND;
    }
    if (!cache_enabled) {
	/* Stop here due to admin action */
	return -NKN_DM2_DISK_ADMIN_ACTION;
    }
    bm->bm_bufsize = roundup(sb.st_size, NBPG);
    bm->bm_fsize = sb.st_size;
    PTHREAD_MUTEX_INIT(&bm->bm_mutex);

    ret = dm2_posix_memalign((void **)&bm->bm_buf, NBPG, bm->bm_bufsize,
			     mod_dm2_cache_buf_t);
    if (ret != 0) {
	DBG_DM2S("ERROR: memory allocation failed: %d", ret);
	return -ret;
    }
    memset(bm->bm_buf, 0, bm->bm_bufsize);

    /* Should be created by disk initialization script */
    glob_dm2_bitmap_open_cnt++;
    if ((bm->bm_fd = dm2_open(bm->bm_fname, O_RDWR, 0644)) == -1) {
	ret = -errno;
	NKN_ASSERT(ret != -EMFILE);
	DBG_DM2S("ERROR: Open %s failed: %d", bm->bm_fname, ret);
	glob_dm2_bitmap_open_err++;
	goto error;
    }
    nkn_mark_fd(bm->bm_fd, DM2_FD);
    PTHREAD_MUTEX_LOCK(&bm->bm_mutex);
    nbytes = read(bm->bm_fd, bm->bm_buf, bm->bm_bufsize);
    if (nbytes == -1) {
	ret = -errno;
	DBG_DM2S("IO ERROR: Failed to read full bytes fname(%s), fd(%d), "
		  "read from (0x%lx), expected(%zu), read(%d) errno(%d)",
		bm->bm_fname, bm->bm_fd, (uint64_t)bm->bm_buf,
		bm->bm_bufsize, nbytes, -ret);
	NKN_ASSERT(ret != -EBADF);
	ret = -EIO;
	glob_dm2_bitmap_read_err++;
	goto error;
    }
    if (nbytes != sb.st_size) {
	DBG_DM2S("IO ERROR: Short read for %s: expected %lu bytes,read %d"
		   " bytes errno(%d)",
		 bm->bm_fname, bm->bm_bufsize, nbytes, errno);
	ret = -EIO;
	glob_dm2_bitmap_read_err++;
	goto error;
    }

    bmh = (dm2_bitmap_header_t *)bm->bm_buf;
    if (bmh->u.bmh_magic != DM2_BITMAP_MAGIC) {
	DBG_DM2S("ERROR: [dev=%s] Wrong magic number: expected 0x%x, got 0x%x",
		 bm->bm_devname, DM2_BITMAP_MAGIC, bmh->u.bmh_magic);
	ret = -EIO;
	glob_dm2_bitmap_read_err++;
	goto error;
    }
    if (bmh->u.bmh_version != DM2_BITMAP_VERSION) {
	DBG_DM2S("ERROR: [dev=%s][mountpt=%s] Wrong version: expected %d, "
		 "got %d", bm->bm_devname, bm->bm_fname, DM2_BITMAP_VERSION,
		 bmh->u.bmh_version);
	ret = -NKN_DM2_WRONG_CACHE_VERSION;
	glob_dm2_bitmap_read_err++;
	goto error;
    }
    if (!(bmh->u.bmh_disk_pagesize == DM2_DISK_PAGESIZE_32K ||
	  bmh->u.bmh_disk_pagesize == DM2_DISK_PAGESIZE_4K)) {
	    DBG_DM2S("Warning: Unexpected disk page size: %d",
		    bmh->u.bmh_disk_pagesize);
    }
    /* This value needed for insertion routines below */
    bm->bm_pages_per_block =
	bmh->u.bmh_disk_blocksize / bmh->u.bmh_disk_pagesize;

    sector_blksz = NKN_Byte2S(bmh->u.bmh_disk_blocksize);  // was power of 2
    if ((ci->set_cache_sec_cnt & ~(sector_blksz-1)) != ci->set_cache_sec_cnt) {
        DBG_DM2M("INFO: Cache (%s) set sector count was not a "
                 "multiple of block size: sec_cnt=%ld sector_blksz=%d",
                 ci->ci_mountpt, ci->set_cache_sec_cnt, sector_blksz);
    }
    ci->set_cache_sec_cnt = ci->set_cache_sec_cnt & ~(sector_blksz-1);
    ci->set_cache_page_cnt =
        ci->set_cache_sec_cnt / (bmh->u.bmh_disk_pagesize / DEV_BSIZE);
    ci->set_cache_blk_cnt =
        ci->set_cache_sec_cnt / (bmh->u.bmh_disk_blocksize / DEV_BSIZE);

    max_blocks = ci->set_cache_page_cnt / bm->bm_pages_per_block;

    /* Each bit in the blk_map will represent a block. Add 1 to account
     * for the remainder blocks (if not exactly divisble by 8) */
    ci->bm.bm_blk_map_size = (ci->set_cache_blk_cnt/8) + 1;
    ci->bm.bm_free_blk_map =
                    dm2_calloc(ci->bm.bm_blk_map_size, 1, mod_dm2_free_blk_map);
    if (ci->bm.bm_free_blk_map == NULL) {
        DBG_DM2S("[%s] Unable to allocate free block map", ci->mgmt_name);
        return -ENOMEM;
    }

    /*
     * Convert the free space bitmap to a queue sorted by length of
     * consecutive free bits.  Since the basic unit of a file is a byte,
     * we can not have a partial byte filled.
     */
    pbits = (uint8_t *)(bmh+1);
    filling = pagestart = numpages = 0;
    bmsize = bm->bm_fsize - sizeof(dm2_bitmap_header_t);
    for (byteno = 0; byteno < (off_t)bmsize; byteno++, pbits++) {
	bits = *pbits;
#if 0
	if (byteno + 8 <= (off_t)bmsize)	// needed to handle last word
	    bitsleft = 8;
	else
	    bitsleft = bmsize - byteno;
#else
	bitsleft = 8;
#endif

	for (b = bitsleft; b > 0; b--) {
	    if (filling) {			// run of consecutive free blks
		if (bits == 0) {		// short circuit
		    numpages += b;
		    break;
		} else if ((bits & 0x1) == 0) {
		    numpages++;
		} else {			// bit is set
		    /* End of free block run */
		    assert((uint64_t)pbits-(uint64_t)(bmh+1) ==
			   (uint64_t)byteno);
		    ret = dm2_insert_free_ext(bm, max_blocks,
					      pagestart, numpages);
		    if (ret)
			goto error;
		    pagestart += numpages+1;
		    filling = numpages = 0;
		}
	    } else {
		if (bits == (0xff >> (bitsleft - b))) {  // short circuit
		    /* Don't care about optimizing last byte */
		    pagestart += b;
		    break;
		} else if ((bits & 0x1) == 0x1) {
		    pagestart++;
		} else {				// bit is not set
		    /* Start new free block run */
		    numpages++;
		    filling = 1;
		}
	    }
	    bits >>= 1;
	}
    }
    if (filling) {
	ret = dm2_insert_free_ext(bm, max_blocks, pagestart, numpages);
	if (ret)
	    goto error;
    }

    /* Since we prepend, revrese the list so that the we ingest in the
     * disk start */
    bm->bm_free_blk_list = g_list_reverse(bm->bm_free_blk_list);
    PTHREAD_MUTEX_UNLOCK(&bm->bm_mutex);
    return 0;

 error:
    if (bm->bm_buf) {
	dm2_free(bm->bm_buf, bm->bm_bufsize, NBPG);
	bm->bm_buf = NULL;
    }
    if (bm->bm_fd != 0 && bm->bm_fd != -1)
	nkn_close_fd(bm->bm_fd, DM2_FD);
    bm->bm_fd = -1;
    glob_dm2_bitmap_close_cnt++;
    PTHREAD_MUTEX_UNLOCK(&bm->bm_mutex);
    return ret;
}	/* dm2_bitmap_read_free_pages */


static int
dm2_fill_disk_container(const char* cont_path,
			dm2_disk_container_t *disk_cont)
{
    disk_cont->cd_checksum = 0;
    disk_cont->cd_magicnum = NKN_CONTAINER_MAGIC;
    disk_cont->cd_cont_sz = NKN_CONTAINER_HEADER_SZ;	// 2 f/s pages
    disk_cont->cd_num_exts = 0;
    disk_cont->cd_obj_sz = DEV_BSIZE;			// sector size=512 bytes

    strncpy(disk_cont->hc_pathname, cont_path, PATH_MAX);
    return 0;
}	/* dm2_fill_disk_container */

/*
 * For the common case where we write out just one extent, we can wait
 * to write out the header once we have figured out the first extent.
 */
static int
dm2_create_container(const char	      *container_pathname,
		     dm2_container_t  *cont,
		     dm2_cache_type_t *ct)
{
    int cfd, nbytes;
    int ret = 0, ret1;
    int disk_cont_sz = 0;
    char *alloc_buf = NULL;
    dm2_disk_container_t *disk_cont = NULL;
    dm2_cache_info_t *ci = cont->c_dev_ci;

    disk_cont_sz = roundup(sizeof(dm2_disk_container_t), DEV_BSIZE);
    assert(disk_cont_sz == 8*1024);

    /* Allocate memory in the stack for the disk_container.
     * This needs to be 512 bytes aligned */
    alloc_buf = alloca(disk_cont_sz + DEV_BSIZE);
    disk_cont = (dm2_disk_container_t *)roundup((off_t)alloc_buf, DEV_BSIZE);

    ret1 = dm2_check_meta_fs_space(ci);
    if (ret1 != 0) {
	DBG_DM2W("[cache_name=%s] Meta data partition reached its threshold",
		 ci->mgmt_name);
	return ret1;
    }

    cont->c_checksum = 0;
    cont->c_magicnum = NKN_CONTAINER_MAGIC;
    cont->c_cont_sz = NKN_CONTAINER_HEADER_SZ;	// 2 f/s pages
    cont->c_cont_mem_sz = sizeof(dm2_container_t);
    cont->c_num_exts = 0;
    cont->c_obj_sz = DEV_BSIZE;			// sector size=512 bytes
    cont->c_blk_allocs = NKN_CONTAINER_MAX_BLK_ALLOC;// not used anymore?
    cont->c_rflags |= DM2_CONT_RFLAG_EXTS_READ;

    memset(disk_cont, 0, disk_cont_sz);
    ret = dm2_fill_disk_container(container_pathname, disk_cont);
    if (ret != 0) {
	DBG_DM2S("[cache_name=%s] Failed to create disk container %s",
                 ci->mgmt_name, container_pathname);
	return ret;
    }
    /* get pathname to directory */
    cfd = dm2_open(container_pathname, O_RDWR | O_CREAT , 0644);
    if (cfd == -1) {
	ret = -errno;
	NKN_ASSERT(ret != -EMFILE);
	DBG_DM2S("Container open failed (%s): %d", container_pathname, ret);
	glob_dm2_container_open_err++;
	ct->ct_dm2_container_open_err++;
	goto error_unlock;
    }
    nkn_mark_fd(cfd, DM2_FD);
    glob_dm2_container_open_cnt++;
    ct->ct_dm2_container_open_cnt++;

    /* XXXmiken: Create checksum before writing container header */
    ct->ct_dm2_container_write_cnt++;
    ct->ct_dm2_container_write_bytes += NKN_CONTAINER_HEADER_SZ;
    nbytes = write(cfd, disk_cont, NKN_CONTAINER_HEADER_SZ);
    if (nbytes == -1) {
	ret = errno;
	DBG_DM2S("IO ERROR:[contname=%s] Failed to write out (%u) bytes "
		 "into fd(%d) from buf(0x%lx) errno=%d",
                 container_pathname, NKN_CONTAINER_HEADER_SZ, cfd,
                 disk_cont, ret);
	ct->ct_dm2_container_write_err++;
	NKN_ASSERT(ret != EBADF);
	ret = -EIO;
	goto error_unlock;
    }
    if (nbytes != NKN_CONTAINER_HEADER_SZ) {
	DBG_DM2S("IO ERROR[contname=%s] Failed to write out container "
		 "header: expected=%d got=%d errno=%d",
		 container_pathname, NKN_CONTAINER_HEADER_SZ, nbytes,
		 errno);
	ct->ct_dm2_container_write_err++;
	NKN_ASSERT(dm2_assert_for_disk_inconsistency);
	ret = -EIO;
	goto error_unlock;
    }
    ret = 0;

 error_unlock:
    if (cfd != -1) {
	glob_dm2_container_close_cnt++;
	ct->ct_dm2_container_close_cnt++;
	nkn_close_fd(cfd, DM2_FD);
    }
    return ret;
}	/* dm2_create_container */


static int
dm2_read_container(const char	    *cont_pathname,
		   dm2_container_t  *cont,
		   int		    *cont_is_valid,
		   dm2_cache_type_t *ct)
{
    dm2_disk_container_t *disk_cont = NULL;
    char	cmd[strlen(DM2_CONT_BKP_SCRIPT)+PATH_MAX];
    char	delete_dir[PATH_MAX], *slash, *buf = NULL;
    struct stat sb;
    size_t	ret2;
    off_t	offset;
    uint64_t	cont_len = 0;
    int		cfd, nbytes, ret, ret1;

    /* Allocate 1KB of memory in stack and align it to a 512 byte boundary.
     * Use this aligned memory to read the container header */
    buf = alloca(DEV_BSIZE*2);
    disk_cont = (dm2_disk_container_t *)roundup((off_t)buf, DEV_BSIZE);
    memset(disk_cont, 0, DEV_BSIZE);

    if ((cfd = dm2_open(cont_pathname, O_RDWR, 0)) == -1) {
	ret = -errno;
	NKN_ASSERT(ret != -EMFILE);
	DBG_DM2S("Failed to open container (%s): %d", cont_pathname, ret);
	glob_dm2_container_open_err++;
	ct->ct_dm2_container_open_err++;
	goto remove_container;
    }
    nkn_mark_fd(cfd, DM2_FD);
    glob_dm2_container_open_cnt++;
    ct->ct_dm2_container_open_cnt++;

    ret = stat(cont_pathname, &sb);
    assert(ret == 0);	/* this should never fail */
    cont_len = sb.st_size;
    if (cont_len < NKN_CONTAINER_HEADER_SZ) {
	/* The container has no extents and the header is corrupt, so
	 * copy the container to the diag dump dir and delete it.
	 * since there are no extents, a new PUT will create the
	 * container file again.
	 */
	/* Pretend that the container is not found */
	ret = -ENOENT;
	DBG_DM2S("Deleting corrupt container (%s): size %ld",
		 cont_pathname, cont_len);
	goto close_fd;
    } else if ((cont_len - NKN_CONTAINER_HEADER_SZ) % DEV_BSIZE) {
	offset = cont_len &~(DEV_BSIZE-1);
	/* the container is not aligned at extent boundary */
	DBG_DM2S("Corrupt container (%s): Size %ld is not a multiple"
		 " of %d. Truncating file to %ld bytes", cont_pathname,
		 cont_len, DEV_BSIZE, offset);

	/* truncate the file to an extent boundary */
	ret1 = ftruncate(cfd, offset);
	if (ret1 < 0) {
	    /* if not EIO, pretend that the container is not found */
	    if (errno != EIO)
		ret = -ENOENT;
	    else
		ret = -errno;
	    DBG_DM2S("ftruncate failed(%d): Deleting corrupt container (%s):"
		     "size %ld", -errno, cont_pathname, cont_len);
	    goto close_fd;
	}
	cont_len = offset;
    }

    glob_dm2_container_read_cnt++;
    ct->ct_dm2_container_read_cnt++;
    /* Only read 512 bytes from the container header which is persistent */
    nbytes = read(cfd, disk_cont, DEV_BSIZE);
    if (nbytes != DEV_BSIZE) {
	ret = -errno;
	DBG_DM2S("IO ERROR:Failed to read full bytes from container (%s) "
                 "fd(%d), read from(0x%lx), expected(%d) read(%d) "
                 "errno(%d)", cont_pathname, cfd, disk_cont,
		 DEV_BSIZE, nbytes, -ret);
	cont->c_rflags &= ~DM2_CONT_RFLAG_VALID;
	glob_dm2_container_read_err++;
	ct->ct_dm2_container_read_err++;
	ret = -EIO;
	NKN_ASSERT(nbytes != -1 || ret != -EBADF);
	goto close_fd;
    }

    if (disk_cont->cd_magicnum != NKN_CONTAINER_MAGIC) {
	DBG_DM2S("Deleting corrupt container (%s): Magic mismatch, expected:"
		 "0x%x, got:0x%x", cont_pathname, NKN_CONTAINER_MAGIC,
		 disk_cont->cd_magicnum);
	NKN_ASSERT(0);
	goto close_fd;
    }

    // XXXmiken: if checksum doesn't validate, then not valid
    cont->c_magicnum = NKN_CONTAINER_MAGIC;
    cont->c_rflags |= DM2_CONT_RFLAG_VALID;
    cont->c_cont_sz = cont_len;
    cont->c_obj_sz = DEV_BSIZE;			// sector size=512 bytes
    cont->c_num_exts = (cont_len - NKN_CONTAINER_HEADER_SZ) / DEV_BSIZE;
    cont->c_cont_mem_sz = sizeof(dm2_container_t);
    dm2_db_mutex_init(&cont->c_open_dblk);

    if (cfd != -1) {
	glob_dm2_container_close_cnt++;
	ct->ct_dm2_container_close_cnt++;
	nkn_close_fd(cfd, DM2_FD);
    }

    /* XXXmiken: verify checksum */
    *cont_is_valid = 1;
    return 0;

 close_fd:
    glob_dm2_container_close_cnt++;
    ct->ct_dm2_container_close_cnt++;
    nkn_close_fd(cfd, DM2_FD);

 remove_container:
    /*
     * XXXmiken: Don't think that this is the correct action.  Unless locking
     * is in place, there could be another thread racing to create the
     * container
     */

    delete_dir[0] = delete_dir[PATH_MAX-1] = '\0';
    strncat(delete_dir, cont_pathname, PATH_MAX-1);
    slash = strrchr(delete_dir, '/');
    *slash = '\0';

    /* copy the container for offline debug */
    ret2 = snprintf(cmd, strlen(DM2_CONT_BKP_SCRIPT)+PATH_MAX-1, "%s %s",
		    DM2_CONT_BKP_SCRIPT, delete_dir);
    if (ret2 > strlen(DM2_CONT_BKP_SCRIPT)+PATH_MAX) {
	NKN_ASSERT(0);
	goto end;
    }

    if (system(cmd) < 0)
	DBG_DM2S("Backup of container %s failed", delete_dir);

	/*
	 * I'm not sure what can cause this other than a bad drive, so I'm
	 * not sure what to do here.  If we can't delete the file, we most
	 * likely can't add to it which means that we won't be able to put
	 * this object.
	 *
	 * XXXmiken
	 */
 end:
    dm2_container_attrfile_remove(cont);
    *cont_is_valid = 0;
    return ret;
}	/* dm2_read_container */


#define DM2_MEM_ALARM_INTERVAL (15*60) //seconds
static AO_t last_alarm = 0;

int
dm2_dictionary_full(void)
{
    if (AO_load(&glob_dm2_alloc_total_bytes) + 1024*1024 >
					glob_dm2_mem_max_bytes) {
	DBG_DM2M("All dictionary memory used: %lu/%lu",
		 AO_load(&glob_dm2_alloc_total_bytes), glob_dm2_mem_max_bytes);
	if ((nkn_cur_ts - AO_load(&last_alarm)) > DM2_MEM_ALARM_INTERVAL) {
	    DBG_DM2A("ALARM: DM2 dictionary memory full; "\
		     "Used: %lu bytes, Max: %lu bytes",
		     AO_load(&glob_dm2_alloc_total_bytes),
		     glob_dm2_mem_max_bytes);
	    AO_store(&last_alarm, nkn_cur_ts);
	}
	return 1;
    }
    return 0;
}	/* dm2_dictionary_full */


/*
 * We only want DM2 to use a specific amount of memory.
 */
static int
dm2_mem_check(size_t sz)
{

    /*
     * To find the amount of memory here, look for nkn_max_dict_mem_MB
     * in mgmt/nvsd_mgmt_buffermgr.c
     *
     * glob_dm2_mem_max_bytes = nkn_max_dict_mem_MB * 1024 * 1024;
     */
    if ((AO_load(&glob_dm2_alloc_total_bytes) -
	glob_malloc_mod_dm2_preread_cont_name + sz ) >
	glob_dm2_mem_max_bytes) {
	glob_dm2_mem_overflow++;
	DBG_DM2M("All dictionary memory used: %lu",
		 AO_load(&glob_dm2_alloc_total_bytes) + sz);
	if ((nkn_cur_ts - AO_load(&last_alarm)) > DM2_MEM_ALARM_INTERVAL) {
	    DBG_DM2A("ALARM: DM2 dictionary memory full; "\
		     "Used: %lu bytes, Max: %lu bytes",
		     AO_load(&glob_dm2_alloc_total_bytes),
		     glob_dm2_mem_max_bytes);
	    AO_store(&last_alarm, nkn_cur_ts);
	}
	return -ENOCSI;
    }
    return 0;
}	/* dm2_mem_check */


/*
 * Must write lock returned container if we need to create the container.
 */
static int
dm2_get_container(const char		*uri_dir,
		  const int		create_container,
		  dm2_cache_info_t	*create_dev_ci,
		  dm2_cache_type_t	*ct,
		  int			*cont_is_valid,
		  int			*cont_wlocked,
		  dm2_container_t	**cont_ret,
		  dm2_container_head_t	**cont_head_ret,
		  int			*cont_head_lock_ret)
{
    dm2_container_head_t *cont_head = *cont_head_ret;
    char		 container_pathname[PATH_MAX];
    dm2_container_t	 *cont = NULL;
    size_t		 cont_sz;
    int			 ret = 0;
    int			 cont_head_locked = *cont_head_lock_ret;

    dm2_make_container_name(create_dev_ci->ci_mountpt, uri_dir,
			    container_pathname);

    cont_sz = sizeof(dm2_container_t);
    /* Do we have memory to fit container into cont_head list? */
    if ((ret = dm2_mem_check(cont_sz + strlen(uri_dir))) < 0)
	goto mem_error;

    cont = dm2_calloc(1, cont_sz, mod_dm2_container_t);
    if (cont == NULL) {
	DBG_DM2S("dm2_calloc failed: %lu", sizeof(dm2_container_t));
	ret = -ENOMEM;
	goto mem_error;
    }
    cont->c_cont_mem_sz = sizeof(dm2_container_t);

    /* Don't initialize anything here except c_rwlock because
     * dm2_read_container will clear it */
    dm2_cont_rwlock_init(cont);
    if (create_container)
	dm2_cont_rwlock_wlock(cont, cont_wlocked, create_dev_ci->mgmt_name);

    /* We need the ci to check if a container alreasy exists */
    cont->c_dev_ci = create_dev_ci;

    /* Possibly create cont_head.  EEXIST if collision. */
    ret = dm2_container_insert(uri_dir, cont, ct,
			       &cont_head, &cont_head_locked);
    if (ret < 0)
	goto free_cont;

    /* We don't allocate memory for c_uri_dir, but just point to the uri_dir
     * that was already allocated for the cont_head (in dm2_container_insert)*/
    cont->c_uri_dir = cont_head->ch_uri_dir;

    if (create_container) {
	/*
	 * We are creating the container because we know that it doesn't
	 * exist.
	 */
	ret = nkn_create_directory(container_pathname, 0); // mods argument
	if (ret < 0) {
	    glob_dm2_put_mkdir_err++;
	    goto unlink_cont;
	}

	/* XXXmiken: don't need mount point in name */
	ret = dm2_create_container(container_pathname, cont, ct);
	if (ret < 0)
	    goto unlink_cont;
	/*
	 * XXXmiken: lock the file here.  If there is a race to create
	 * this file, then locking the file will determine the winner.
	 * After getting the lock, check to see if the container is in
	 * the container hash again before proceeding.
	 *
	 * Dueling container creates is harder than this.  We can't have
	 * one of the reads remove the container if it sees an invalid
	 * container.
	 */
	*cont_is_valid = 1;
	cont->c_created = 1;
    } else {
	ret = dm2_read_container(container_pathname, cont, cont_is_valid, ct);
	if (ret < 0)
	    goto unlink_cont;

	// We've deleted the bogus container
	if (*cont_is_valid == 0)
	    goto unlink_cont;
    }
    cont->c_blksz = dm2_get_blocksize(create_dev_ci);	// runtime
    cont->c_dev_ci = create_dev_ci;			// runtime
    dm2_attr_mutex_init(cont);
    dm2_slot_mutex_init(cont);
    dm2_g_queue_init(&cont->c_ext_slot_q);
    dm2_g_queue_init(&cont->c_attr_slot_q);

    *cont_ret = cont;
    *cont_head_ret = cont_head;
    *cont_head_lock_ret = cont_head_locked;
    return 0;

 unlink_cont:
    cont_head->ch_cont_list = g_list_remove(cont_head->ch_cont_list, cont);
    glob_dm2_cont_insert_err++;

 free_cont:
    if (cont_head_locked != DM2_CONT_HEAD_UNLOCKED)
	dm2_conthead_rwlock_wunlock(cont_head, ct, &cont_head_locked);
    *cont_head_lock_ret = cont_head_locked;
    if (*cont_wlocked)
	dm2_cont_rwlock_wunlock(cont, cont_wlocked, create_dev_ci->mgmt_name);
    dm2_container_destroy(cont, ct);

 mem_error:
    return ret;
}	/* dm2_get_container */


int
dm2_get_contlist(const char		*uri,
		 const int		create_container,
		 dm2_cache_info_t	*create_dev_ci,
		 dm2_cache_type_t	*ct,
		 int			*valid_cont_found_ret,
		 dm2_container_head_t	**conthead_ret,
		 dm2_container_t	**cont_ret,
		 int			*cont_wlocked,
		 int			*cont_head_locked)
{
    dm2_container_head_t *cont_head = NULL;
    char		 uri_dir[PATH_MAX];
    GList		 *cont_list, *cptr, *ci_ptr;
    dm2_container_t	 *cont;
    dm2_cache_info_t	 *ci;
    char		 *fake;
    int			 ret, valid_cont_found, cont_valid;
    int			 ct_rlocked, ci_exists, err_ret = 0;

    ct_rlocked = valid_cont_found = 0;
    cont_valid = ci_exists = 0;

    strcpy(uri_dir, uri);
    fake = dm2_uri_dirname(uri_dir, 0);
    /* uri_dir might be an empty string.  glib works for this case */

 collision:
    ret = 0;
    if (*conthead_ret != NULL) {
	/* If we got here on a cont_exists race, we should not be holding
	 * any lock on the cont_head. Now that the cont_head is created
	 * by someone else we should acquire a rlock on it before
	 * proceeding further.
	 */
	cont_head = *conthead_ret;
	assert(*cont_head_locked != DM2_CONT_HEAD_WLOCKED);
#if 0
	/* We should not just grab locks on conthead by accessing a ptr */
	if (*cont_head_locked != DM2_CONT_HEAD_RLOCKED)
	     dm2_conthead_rwlock_rlock(cont_head, ct, cont_head_locked,
				       DM2_BLOCKING);
#else
	if (*cont_head_locked != DM2_CONT_HEAD_RLOCKED) {
	    cont_head = dm2_conthead_get_by_uri_dir_rlocked(uri_dir, ct,
							    cont_head_locked,
							    &err_ret,
							    DM2_BLOCKING);
	    /* Since this is called during pre-read, we should not get EBUSY or
	     * EINVAL */
	    assert(err_ret == 0);
	}
#endif
    } else {
	cont_head = dm2_conthead_get_by_uri_dir_rlocked(uri_dir, ct,
							cont_head_locked, NULL,
							DM2_BLOCKING);
    }
    if (cont_head) {
	cont_list = cont_head->ch_cont_list;
	if (create_container == 0) {
	    dm2_ct_info_list_rwlock_rlock(ct);
	    ct_rlocked = 1;

	    /* Let us walk through the ct_info_list to read in the
	     * containers from the disks that have just been enabled */
	    for (ci_ptr = ct->ct_info_list; ci_ptr; ci_ptr = ci_ptr->next) {
		ci = (dm2_cache_info_t *)ci_ptr->data;
		ret = dm2_cache_container_stat(uri, ci);
		if (ret == 0) {
		    ci_exists = 0;
		    /* Check if this disk (ci) is already present in the
		     * container head list. If present skip, else add */
		    for (cptr = cont_list; cptr; cptr = cptr->next) {
		        cont = (dm2_container_t *)cptr->data;
			if (cont->c_dev_ci == ci) {
			    ci_exists = 1;
			    break;
			}
		    }

		    if (ci_exists)
			continue;

		    NKN_ASSERT(*cont_wlocked == 0);
		    if (*cont_head_locked == DM2_CONT_HEAD_RLOCKED) {
			dm2_conthead_rwlock_runlock_wlock(cont_head, ct,
							  cont_head_locked);
		    }
		    ret = dm2_get_container(uri_dir, 0, ci, ct, &cont_valid,
					    cont_wlocked, &cont, &cont_head,
					    cont_head_locked);
		    if (ret < 0) {
			/* Possible errors are ENOMEM or disk failure/EIO */
			dm2_ct_info_list_rwlock_runlock(ct);
			ct_rlocked = 0;
			if (ret == -EIO)
			    ci->valid_cache = 0;
			if (ret == -EEXIST)
			    goto collision;
			goto error_unlock;
		    }

		    if (cont_valid) {
			/* This container is not used by the callee func */
			valid_cont_found = 1;
			*cont_ret = cont;
		    }
		    /* Breaking out here does not handle multiple
		     * disk enables at a time */
		    break;
		}
	    }

	    if (ct_rlocked)
		dm2_ct_info_list_rwlock_runlock(ct);

	    /* STAT request or existence check in PUT */
	    *conthead_ret = cont_head;
	    *valid_cont_found_ret = 1;
	    /* Valid container found, but URI not necessarily found */
	    return 0;
	}

	/*
	 * Must be PUT and willing to create container if we reached here.
	 *
	 * We may not have a locked container yet if the container is not
	 * created yet.  The only thing chosen would be a create_dev_ci.
	 * To find a match in the following code, a container would need to
	 * be created, meaning that it should be write locked (done at end
	 * of dm2_get_new_freespace_dev_cont_wlocked_ci_rlocked).
	 */
	for (cptr = cont_list; cptr; cptr = cptr->next) {
	    cont = (dm2_container_t *)cptr->data;
	    if (cont->c_dev_ci == create_dev_ci) {
		/* What we need is already there */
		if (*cont_wlocked == 0) {
		    dm2_cont_rwlock_wlock(cont, cont_wlocked,
					  create_dev_ci->mgmt_name);
		    glob_dm2_empty_cont_race++;
		}
		*conthead_ret = cont_head;
		*cont_ret = cont;
		*valid_cont_found_ret = 1;
	//	*cont_head_locked = DM2_CONT_HEAD_RLOCKED;
		NKN_ASSERT(cont->c_rwlock.__data.__writer == gettid());
		NKN_ASSERT(*cont_wlocked);
		return 0;
	    }
	}
	/* PUT request: Create container if it doesn't exist */
	NKN_ASSERT(*cont_wlocked == 0);
	dm2_conthead_rwlock_runlock_wlock(cont_head, ct, cont_head_locked);
	ret = dm2_get_container(uri_dir, 1, create_dev_ci, ct,
				&cont_valid, cont_wlocked, &cont,
				&cont_head, cont_head_locked);
	if (ret < 0) {
	    /*
	     * Possible errors are ENOMEM, disk or filesystem failure (EIO),
	     * mkdir failure (EMLINK), EEXIST (race with insert)
	     */
	    if (ret == -EEXIST) {
		NKN_ASSERT(*cont_wlocked == 0);
		glob_dm2_conthead_race_get_contlist1_cnt++;
		goto collision;
	    }
	    goto error_unlock;
	}
	NKN_ASSERT(*cont_wlocked);
	NKN_ASSERT(cont->c_rwlock.__data.__writer == gettid());
	valid_cont_found = cont_valid;
	*cont_ret = cont;
	NKN_ASSERT(cont_head);
    } else {
	assert(!cont_head);
	dm2_ct_info_list_rwlock_rlock(ct);
	ct_rlocked = 1;
	for (ci_ptr = ct->ct_info_list; ci_ptr; ci_ptr = ci_ptr->next) {
	    ci = (dm2_cache_info_t *)ci_ptr->data;
	    ret = dm2_cache_container_stat(uri, ci);
	    /* create_dev_ci != NULL only for PUT */
	    if (ret == 0 || create_dev_ci == ci) {
		NKN_ASSERT(*cont_wlocked == 0);
		ret = dm2_get_container(uri_dir,
			(create_dev_ci == ci) ? create_container : 0, ci, ct,
			&cont_valid, cont_wlocked, &cont, &cont_head,
			cont_head_locked);
		if (ret < 0) {
		    /* Possible errors are ENOMEM or disk failure/EIO */
		    dm2_ct_info_list_rwlock_runlock(ct);
		    ct_rlocked = 0;
		    if (ret == -EIO)
			ci->valid_cache = 0;
		    if (ret == -EEXIST) {
			glob_dm2_conthead_race_get_contlist2_cnt++;
			NKN_ASSERT(*cont_wlocked == 0);
			goto collision;
		    }
		    /* If we looped and passed the cont_head to
		     * 'dm2_get_container', we will be already holding
		     * a wlock on the cont_head and 'cont_head_locked'
		     * should reflect the same.
		     * Addendum: There are cases where the locking model has
		     * not been followed strictly. For some error cases the ch
		     * is unlocked and some unlocked in the called functions.
		     * There is no functional issue anywhere (and the assert
		     * below is just an ALARM) but the locking model needs to
		     * is poor and needs to be fixed while fixing bug 5953.
		     */
		    //if (cont_head &&
		    //	*cont_head_locked == DM2_CONT_HEAD_UNLOCKED)
		    //	assert(0);
		    goto error_unlock;
		}
		if (cont_valid)
		    valid_cont_found = 1;
		if (create_dev_ci == ci)
		    *cont_ret = cont;
	    }
	}
	/*
	 * If we go through all disks and don't find a container, we may not
	 * have a valid cont_head
	 */
	if (cont_head)
	    *cont_head_locked = DM2_CONT_HEAD_WLOCKED;
	dm2_ct_info_list_rwlock_runlock(ct);
    }
    *conthead_ret = cont_head;
    *valid_cont_found_ret = valid_cont_found;
    return 0;

 error_unlock:
    if (*cont_wlocked)
	dm2_cont_rwlock_wunlock(cont, cont_wlocked, uri);
    if (cont_head && *cont_head_locked != DM2_CONT_HEAD_UNLOCKED)
	dm2_conthead_rwlock_unlock(cont_head, ct, cont_head_locked);
    if (ct_rlocked)
	dm2_ct_info_list_rwlock_runlock(ct);
    return ret;
}	/* dm2_get_contlist */


static DM2_uri_t *
dm2_create_uri_head(const char *uri,
		    dm2_container_t *cont,
		    dm2_cache_type_t *ct,
		    int disk_block)
{
    DM2_uri_t *uri_head = NULL;
    char      *p_basename = NULL;

    p_basename = dm2_uri_basename((char*)uri);
    if (p_basename == NULL)
	goto error;

    /* Allocate uri head */
    if ((uri_head = dm2_calloc(1, sizeof(DM2_uri_t), mod_DM2_uri_t)) == NULL) {
	DBG_DM2S("[cache_type=%s] dm2_calloc failed: %lu",
		 ct->ct_name,  sizeof(DM2_uri_t));
	NKN_ASSERT(uri_head);
	goto error;
    }
    glob_dm2_alloc_uri_head++;

    /* copy uri to use as hash key */
    uri_head->uri_name_len = DM2_URI_CONT_PTR_LEN+strlen(p_basename)+1;
    uri_head->uri_name = dm2_calloc(1, uri_head->uri_name_len,
				    mod_dm2_uri_name_t);
    if (uri_head->uri_name == NULL) {
	DBG_DM2S("[cache_type=%s] dm2_calloc failed: %d",
		 ct->ct_name, uri_head->uri_name_len);
	NKN_ASSERT(uri_head->uri_name);
	goto error;
    }
    glob_dm2_alloc_uri_head_uri_name++;
    snprintf(uri_head->uri_name, uri_head->uri_name_len,
	     "%lx/%s", (long)cont, p_basename);

    uri_head->uri_open_dblk = NULL;
    if (disk_block) {
	uri_head->uri_open_dblk = dm2_calloc(1, sizeof(dm2_disk_block_t),
					     mod_dm2_disk_block_t);
	if (uri_head->uri_open_dblk == NULL) {
	    DBG_DM2S("[cache_type=%s] dm2_calloc failed: %ld",
		     ct->ct_name, sizeof(dm2_disk_block_t));
	    NKN_ASSERT(uri_head->uri_open_dblk);
	    goto error;
	}
	glob_dm2_alloc_uri_head_open_dblk++;
    }

    uri_head->uri_ext_list = NULL;
    uri_head->uri_at_off = DM2_ATTR_INIT_OFF;
    uri_head->uri_at_len = DM2_ATTR_INIT_LEN;
    uri_head->uri_container = cont;

    return uri_head;

 error:
    if (uri_head && uri_head->uri_open_dblk) {
	dm2_free(uri_head->uri_open_dblk, sizeof(dm2_disk_block_t),
		 DM2_NO_ALIGN);
    }
    if (uri_head && uri_head->uri_name) {
	dm2_free(uri_head->uri_name, uri_head->uri_name_len, DM2_NO_ALIGN);
	glob_dm2_free_uri_head_uri_name++;
    }
    if (uri_head) {
	dm2_free(uri_head, sizeof(DM2_uri_t), DM2_NO_ALIGN);
	glob_dm2_free_uri_head++;
    }
    return NULL;
}	/* dm2_create_uri_head */


void
dm2_free_uri_head(DM2_uri_t *uri_head)
{
    /* only need to check uh->uri_prev when calling on a uri_head which was
     * never added and has NULL ptrs */
    assert(uri_head->uri_deleting == 0 || uri_head->uri_deleting == 1);

    /* Remove this URI from the evict list, if it was inserted
     * We would not have inserted to the evict list only if the PUT had failed,
     * the attribute for this object was not read in or the object is pinned */
    if (uri_head->uri_evict_list_add)
        dm2_evict_delete_uri(uri_head,am_decode_hotness(&uri_head->uri_hotval));

    dm2_free(uri_head->uri_name, uri_head->uri_name_len, DM2_NO_ALIGN);
    glob_dm2_free_uri_head_uri_name++;
    dm2_free(uri_head->uri_open_dblk, sizeof(dm2_disk_block_t), DM2_NO_ALIGN);
    dm2_free_ext_list(uri_head->uri_ext_list);
    /*
     * After destroying the uri_head rwlock, anyone waiting on that
     * lock will be woken up.  Lookups will return ENOENT from the
     * lookup
     */
    uri_head->uri_deleting = 0x2;
    dm2_free(uri_head, sizeof(DM2_uri_t), DM2_NO_ALIGN);
    glob_dm2_free_uri_head++;
}	/* dm2_free_uri_head */


int
dm2_create_uri_head_wlocked(const char       *uri,
			    dm2_container_t  *cont,
			    dm2_cache_type_t *ct,
			    int		     disk_block,
			    DM2_uri_t	     **uri_head_ret)
{
    DM2_uri_t *uh;
    dm2_cache_info_t *ci;
    int ret = 0;

    uh = dm2_create_uri_head(uri, cont, ct, disk_block);
    if (uh) {
	dm2_uri_head_rwlock_wlock(uh, ct, 0);
	uh->uri_initing = 1;		// debug
	ci = uh->uri_container->c_dev_ci;
	if ((ret = dm2_uri_head_insert_by_uri(uh, ct)) != 0) {
	    /* Collision, return EINVAL.  Converted to EAGAIN above */
	    dm2_free_uri_head(uh);
	    glob_dm2_uri_create_race++;
	    return ret;
	}
    } else
	ret = -ENOMEM;
    *uri_head_ret = uh;
    return ret;
}	/* dm2_create_uri_head_wlocked */


/*
 * Return a positive number if the first argument should be sorted after
 * the second argument.
 */
int
cmp_smallest_dm2_extent_offset_first(gconstpointer in_a,
				     gconstpointer in_b)
{
    DM2_extent_t *a = (DM2_extent_t *)in_a;
    DM2_extent_t *b = (DM2_extent_t *)in_b;

    if (a->ext_offset > b->ext_offset)
	return 1;
    else
	return 0;
}	/* cmp_smallest_dm2_extent_offset_first */


/*
 * XXXmiken: Rather than just skipping specific extents, we should
 * delete the entire container.
 */
static int
dm2_check_disk_ext(const char		   *cont_pathname,
		   dm2_container_t	   *cont,
		   const int		   secnum,
		   const char		   *pbuf,
		   const char		   *buf,
		   int			   *ext_ver)
{
    int skip = 0;

    DM2_disk_extent_header_t *dhdr = (DM2_disk_extent_header_t *)pbuf;
    DM2_disk_extent_t *dext = (DM2_disk_extent_t *)pbuf;

    if (dhdr->dext_magic == DM2_DISK_EXT_DONE) {
	/*
	 * Do nothing; Can skip this sector.
	 * It's possible to store these sector offsets in a list and
	 * reuse the sectors.  However, this is more work than I want
	 * to do at this point.
	 */
	DBG_DM2M2("container (%s) name=%s file offset=%ld DONE magic",
		  cont_pathname, dext->dext_basename, pbuf-buf);
	DBG_DM2M3("physid=%lx uol_off=%zd uol_len=%ld ext_start=%zd",
		  dext->dext_physid, dext->dext_offset,
		  dext->dext_length, dext->dext_start_sector);
	skip = 1;
	dm2_ext_slot_push_tail(cont, ((uint64_t)secnum)*DEV_BSIZE);
    } else if (dhdr->dext_magic != DM2_DISK_EXT_MAGIC) {
	DBG_DM2S("Illegal magic number in container (%s): "
		 "name=%s expected=0x%x|0x%x read=0x%x offset=%ld",
		 cont_pathname, dext->dext_basename, DM2_DISK_EXT_MAGIC,
		 DM2_DISK_EXT_DONE, dhdr->dext_magic, pbuf-buf);
	DBG_DM2M3("physid=%lx uol_off=%zd uol_len=%ld ext_start=%zd",
		  dext->dext_physid, dext->dext_offset,
		  dext->dext_length, dext->dext_start_sector);
	skip = 1;
    }
    if ((dhdr->dext_version != DM2_DISK_EXT_VERSION) &&
        (dhdr->dext_version != DM2_DISK_EXT_VERSION_V2)) {
	DBG_DM2S("Illegal version in container (%s): "
		 "name=%s expected=%d/%d read=%d offset=%ld",
		 cont_pathname, dext->dext_basename, DM2_DISK_EXT_VERSION,
		 DM2_DISK_EXT_VERSION_V2,
		 dhdr->dext_version, pbuf-buf);
	DBG_DM2M3("physid=%lx uol_off=%zd uol_len=%ld ext_start=%zd",
		  dext->dext_physid, dext->dext_offset,
		  dext->dext_length, dext->dext_start_sector);
	skip = 1;
    }
    *ext_ver = dhdr->dext_version;
    return skip;
}	/* dm2_check_disk_ext */

uint64_t
dm2_physid_hash(const void *v, size_t len)
{
    (void)len;
    const uint64_t w1 = (((const uint64_t)v) & 0xffffffff);
    const uint64_t w2 = (((const uint64_t)v) & 0xffffffff00000000) >> 32;

    return (w1 ^ w2);
}   /* dm2_physid_hash */


int
dm2_physid_equal(const void *v1,
		 const void *v2)
{
    return ((const uint64_t)v1) == ((const uint64_t)v2);
}   /* dm2_physid_equal */

static void
dm2_free_physid_hash_entry(void *key,
                           void *value,
                           void *userdata)
{
    NCHashTable *ns_physid_hash = (NCHashTable *)userdata;
    int64_t  keyhash;
    int ret;

    keyhash = dm2_physid_hash((void *)key, 0);
    ret = nchash_remove(ns_physid_hash, keyhash, key);
    if (!ret)
        assert(0);
    dm2_free(value, sizeof(ns_physid_hash_entry_t), DM2_NO_ALIGN);
    return;
}   /* dm2_free_physid_hash_entry */


/*
 * Assumes that the container file is locked
 * dm2_read_cont_extents_all_uris2() is invoked with its 'ci' device rlocked.
 * So we need not lock the ci while the uri_head's are being created
 */
static int
dm2_read_cont_extents_all_uris2(dm2_container_head_t	*cont_head,
				uint32_t		preread,
				dm2_cache_info_t	*dev_ci,
				dm2_container_t		*cont,
				dm2_cache_type_t	*ct)
{
#define RD_EXT_BUFSZ    (2*1024*1024)
    uint64_t		physid;
    char		*buf = NULL;
    char		dext_uri[PATH_MAX];
    char		*alloc_buf = NULL;
    off_t		tot_len, dext_off, dext_len;
    char		*cont_pathname, *pbuf, *slash;
    GList		*ext_list;
    DM2_disk_extent_t	*dext;
    DM2_disk_extent_v2_t *dext_v2;
    DM2_extent_t	*ext = NULL, *find_ext;
    DM2_uri_t		*uh = NULL;		// temporary uri head ptr
    NCHashTable		*namespace_physid_hash = NULL;
    ns_stat_token_t	ns_stoken;
    ns_stat_category_t	ns_cat, cp_cat, to_cat, ns_tier_cat;
    uint32_t		blksz, disk_block_size;
    size_t		raw_length;
    int			rdsz, ret, secnum, raw_secnum, cfd, dext_version = 0;
    int			i, uri_head_wlocked = 0, nexts = cont->c_num_exts;
    int			good_stoken = 0;

    if (NKN_CONT_EXTS_READ(cont))
	return 0;

    /* Do we have memory to take this container in the dictionary?
     * Assume that we will use 256 bytes of dictionary per extent.
     * uri_head is 128 bytes, extent is 96 bytes & uri name rest */

    if ((ret = dm2_mem_check(nexts*256)) < 0)
	return ret;
    /*
     * A bunch of variables to deal with namespace and cache pinning space
     */
    namespace_physid_hash = nchash_table_new(dm2_physid_hash, dm2_physid_equal,
					     1000, 0, mod_ns_physid_hash);
    if (namespace_physid_hash == NULL) {
	DBG_DM2S("nhash_table_new failed for namespace physid_hash");
	return -ENOMEM;
    }

    ns_stoken = dm2_ns_get_stat_token_from_uri_dir(cont->c_uri_dir);
    if (ns_is_stat_token_null(ns_stoken)) {
	DBG_DM2W("Stats not collected for namespace - can not get token: %s",
		 cont->c_uri_dir);
    } else {
	good_stoken = 1;
    }
    ret = dm2_stat_get_diskstat_cats(dev_ci->mgmt_name, &cp_cat, &ns_cat,
				     &to_cat);
    if (ret != 0)
	goto free_stoken;

    ns_tier_cat = dm2_ptype_to_nscat(ct->ct_ptype);
    if (ns_tier_cat == 0) {	// Illegal value
	DBG_DM2S("[URI_DIR=%s] Failed to find rpindex: %d",
		 cont->c_uri_dir, ret);
	ret = -ret;
	goto free_stoken;
    }

    DM2_GET_CONTPATH(cont, cont_pathname);
    strcpy(dext_uri, cont->c_uri_dir);
    strcat(dext_uri, "/");
    slash = &dext_uri[strlen(dext_uri)-1];
    disk_block_size = dm2_get_blocksize(dev_ci);

    if (preread) {
	/* preread stacks are 2MB */
	alloc_buf = alloca(RD_EXT_BUFSZ+DEV_BSIZE);
	buf = (char *)roundup((off_t)alloc_buf, DEV_BSIZE);
    } else {
	if (dm2_posix_memalign((void *)&buf, DEV_BSIZE, RD_EXT_BUFSZ,
			       mod_dm2_ext_read_buf_t)) {
	    DBG_DM2S("posix_memalign failed: %d %d", RD_EXT_BUFSZ, errno);
	    ret = -ENOMEM;
	    goto free_stoken;
	}
    }
    cfd = dm2_open(cont_pathname, O_RDWR | O_CREAT, 0644);
    if (cfd == -1) {
	ret = -errno;
	NKN_ASSERT(ret != -EMFILE);
	DBG_DM2S("Failed to open container (%s): %d", cont_pathname, ret);
	glob_dm2_container_open_err++;
	ct->ct_dm2_container_open_err++;
	goto free_buf;
    }
    nkn_mark_fd(cfd, DM2_FD);
    glob_dm2_container_open_cnt++;
    ct->ct_dm2_container_open_cnt++;

    //    ext_list = uri_head->uri_ext_list;	/* works even if NULL */

    /* Set secnum, so we skip the container header */
    secnum = NKN_CONTAINER_HEADER_SZ / DEV_BSIZE;
    while (nexts > 0) {
	if (dev_ci->ci_disabling)
            break;
	glob_dm2_ext_read_cnt++;
	ct->ct_dm2_container_read_cnt++;
	rdsz = pread(cfd, buf, RD_EXT_BUFSZ, secnum*DEV_BSIZE);
	if (rdsz == -1) {
	    ret = -errno;
	    DBG_DM2S("IO ERROR:Read from container (%s) failed "
		"fd(%d) read from(0x%x) expected(%zu) with errno: %d",
		cont_pathname, cfd, secnum*DEV_BSIZE, RD_EXT_BUFSZ, -ret);
	    ct->ct_dm2_container_read_err++;
	    NKN_ASSERT(ret != -EBADF);
	    goto free_buf;
	}
	if (rdsz == 0) {
	    /* Reached EOF, just break out of the while loop */
	    DBG_DM2S("IO ERROR:Reached EOF on container (%s) "
		     "pending nexts: %d fd(%d) read from(0x%x) "
                     "expected(%zu) with errno: %d", cont_pathname, nexts,
                     cfd, secnum*DEV_BSIZE, RD_EXT_BUFSZ, -ret);
	    ct->ct_dm2_container_eof_read_err++;
	    break;
	}
	if (rdsz % DEV_BSIZE != 0) {
	    DBG_DM2S("IO ERROR:Read from container (%s) is not an "
                     "integral sector amount: %d fd(%d) read from(0x%x) "
                     "expected(%zu) with errno: %d", cont_pathname, rdsz,
                     cfd, secnum*DEV_BSIZE, RD_EXT_BUFSZ, -ret);
	    NKN_ASSERT(rdsz % DEV_BSIZE == 0);
	    ct->ct_dm2_container_short_read_err++;
	}

	dm2_inc_container_read_bytes(ct, dev_ci, rdsz);
	glob_dm2_ext_read_bytes += rdsz;
	/*
	 * - If there is more data than what is referenced in 'nexts', we
	 *   skip that data.
	 * - If there is sector unaligned data at the end, we skip that data
	 *   and print a warning.
	 */
	for (pbuf = buf; nexts > 0 && rdsz >= DEV_BSIZE;
	     rdsz -= DEV_BSIZE, nexts--, pbuf += DEV_BSIZE, secnum++) {

	    if (dev_ci->ci_disabling)
		break;

	    dext = (DM2_disk_extent_t *)pbuf;
	    /* verify checksum */
	    if (dm2_check_disk_ext(cont_pathname, cont,secnum, pbuf, buf,
				   &dext_version))
		continue;

	    if (dext_version == DM2_DISK_EXT_VERSION) {
		dext = (DM2_disk_extent_t *)pbuf;
		strcat(dext_uri, dext->dext_basename);
		dext_off = dext->dext_offset;
		dext_len = dext->dext_length;
		raw_secnum = dext->dext_start_sector;
	    } else if (dext_version == DM2_DISK_EXT_VERSION_V2) {
		dext_v2 = (DM2_disk_extent_v2_t *)pbuf;
		strcat(dext_uri, dext_v2->dext_basename);
		dext_off = dext_v2->dext_offset;
		dext_len = dext_v2->dext_length;
		raw_secnum = dext_v2->dext_start_sector;
	    }

	    /* We grab a write lock, so readers can't start reading until
	     * we create a complete object */
	    uh = dm2_uri_head_get_ci_by_uri_wlocked(dext_uri, ct, cont,
						    dev_ci, 0);
	    if (uh == NULL) {
		ret = dm2_create_uri_head_wlocked(dext_uri, cont, ct, 0, &uh);
		if (ret != 0) {
		    NKN_ASSERT(uh == NULL && ret == 0);
		    continue;
		}
	    }
	    assert(uh->uri_container->c_dev_ci);
	    uri_head_wlocked = 1;

	    blksz = cont->c_blksz;
	    physid = nkn_physid_create(ct->ct_ptype, dev_ci->list_index,
				       NKN_SECTOR_TO_BLOCK(raw_secnum, blksz));
	    find_ext = dm2_find_extent_by_offset(uh->uri_ext_list, dext_off,
						 dext_len, &tot_len, NULL, NULL);
	    if (find_ext) {
		if (find_ext->ext_offset == dext_off &&
		    find_ext->ext_length == dext_len) {
		    DBG_DM2S("Overlapping, duplicate extent found: "
			     "old_disk=%s old_off=%zu old_len=%d "
			     "new_disk=%s new_off=%zu new_len=%zu [URI=%s]",
			     find_ext->ext_uri_head->uri_container->c_dev_ci->mgmt_name,
			     find_ext->ext_offset, find_ext->ext_length,
			     dev_ci->mgmt_name, dext_off, dext_len,
			     find_ext->ext_uri_head->uri_name);
		    NKN_ASSERT(0);
		} else {
		    DBG_DM2S("Overlapping, non-duplicate extent found: "
			     "old_disk=%s old_off=%zu old_len=%d "
			     "new_disk=%s new_off=%zu new_len=%zu [URI=%s]",
			     find_ext->ext_uri_head->uri_container->c_dev_ci->mgmt_name,
			     find_ext->ext_offset,
			     find_ext->ext_length, dev_ci->mgmt_name,
			     dext_off, dext_len,
			     find_ext->ext_uri_head->uri_name);
		    NKN_ASSERT(0);
		}
		*(slash+1) = '\0';
		/* XXXmiken: this must change.  Need to return URI list */
		dm2_uri_head_rwlock_wunlock(uh, ct, DM2_NOLOCK_CI);
		uri_head_wlocked = 0;
		/* Account the space used in this disk based on page size */
		raw_length = ROUNDUP_PWR2(find_ext->ext_length,
				      dm2_tier_pagesize(ct->ct_ptype));
		if (good_stoken)
		    ret = dm2_ns_calc_space_usage(ns_stoken, ns_cat,
						  ns_tier_cat, ct->ct_ptype,
						  raw_length);
		continue;
	    }
	    if (dev_ci != uh->uri_container->c_dev_ci) {
		/* This is a different disk, so we have found a duplicate
		 * entry on a different disk */
		DBG_DM2S("Extent found on different disk: old_disk=%s "
			 "new_disk=%s new_off=%zu new_len=%zu",
			 uh->uri_container->c_dev_ci->mgmt_name,
			 dev_ci->mgmt_name, dext_off, dext_len);
		*(slash+1) = '\0';
		/* XXXmiken: this must change.  Need to return URI list */
		dm2_uri_head_rwlock_wunlock(uh, ct, DM2_NOLOCK_CI);
		uri_head_wlocked = 0;
		continue;
	    }

	    /* All checks done.  Let's process the sector */
	    ext = dm2_calloc(1, sizeof(DM2_extent_t), mod_DM2_extent_t);
	    if (ext == NULL) {
		DBG_DM2S("dm2_calloc failed: %ld", sizeof(DM2_extent_t));
		NKN_ASSERT(ext);
		ret = -ENOMEM;
		goto free_extents;
	    }
	    ext->ext_offset = dext_off;
	    ext->ext_length = dext_len;
	    ext->ext_version = dext_version;

	    if (dext_version == DM2_DISK_EXT_VERSION) {

		ext->ext_physid = physid;
		NKN_ASSERT(NKN_SECTOR_TO_BLOCK(raw_secnum, blksz) ==
			   nkn_physid_to_sectornum(dext->dext_physid));

		ext->ext_csum.ext_checksum = dext->dext_ext_checksum;
		ext->ext_start_sector = dext->dext_start_sector;
		ext->ext_cont_off = secnum;
		ext->ext_flags |= DM2_EXT_FLAG_INUSE;
		ext->ext_flags |= (dext->dext_header.dext_flags &
				   DM2_DISK_EXT_FLAG_CHKSUM) ? DM2_EXT_FLAG_CHKSUM : 0;
		ext->ext_read_size = disk_block_size / DM2_EXT_RDSZ_MULTIPLIER;
	    } else if (dext_version == DM2_DISK_EXT_VERSION_V2) {
		raw_secnum = dext_v2->dext_start_sector;
#if 1
		ext->ext_physid = physid;
		NKN_ASSERT(NKN_SECTOR_TO_BLOCK(raw_secnum, blksz) ==
			   nkn_physid_to_sectornum(dext_v2->dext_physid));
#else
		ext->ext_physid =
		    nkn_physid_create(ct->ct_ptype, dev_id, raw_secnum);
		NKN_ASSERT(raw_secnum == nkn_physid_to_sectornum(dext_v2->dext_physid));
#endif
		for (i = 0; i < 8; i++)
		    ext->ext_csum.ext_v2_checksum[i] = dext_v2->dext_ext_checksum[i];
		ext->ext_start_sector = dext_v2->dext_start_sector;
		ext->ext_cont_off = secnum;
		ext->ext_flags |= DM2_EXT_FLAG_INUSE;
		ext->ext_flags |= (dext_v2->dext_header.dext_flags & DM2_DISK_EXT_FLAG_CHKSUM) ?
		    DM2_EXT_FLAG_CHKSUM : 0;
		ext->ext_read_size = disk_block_size / (8 * DM2_EXT_RDSZ_MULTIPLIER);
	    }
	    /*
	     * Account the amount of data that is already in the disk
	     * Later we will be doing a reservation for the pending data
	     * when a new PUT comes (if the URI is partial)
	     */
	    uh->uri_resv_len -= ext->ext_length;

	    //dm2_print_mem_extent(ext);
	    ext_list =
		g_list_insert_sorted(uh->uri_ext_list, ext,
				     cmp_smallest_dm2_extent_offset_first);
	    uh->uri_ext_list = ext_list;
	    ext->ext_uri_head = uh;
	    ret = dm2_extent_insert_by_physid(&ext->ext_physid, ext, ct);
	    if (ret < 0) {
		assert(0);	/* ENOSPC is possible now */
		/* XXXmiken: will need to back out everything */
	    }
	    *(slash+1) = '\0';
	    /* XXXmiken: this must change to return URI list */
	    dm2_uri_head_rwlock_wunlock(uh, ct, DM2_NOLOCK_CI);
	    assert(uh->uri_container->c_dev_ci);
	    uri_head_wlocked = 0;
	    AO_fetch_and_add1(&uh->uri_container->c_good_ext_cnt);
	    /* Account the space used in this disk based on page size */
	    raw_length = ROUNDUP_PWR2(ext->ext_length,
				      dm2_tier_pagesize(ct->ct_ptype));
	    if (good_stoken) {
		ret = dm2_ns_calc_space_usage(ns_stoken, ns_cat, ns_tier_cat,
					      ct->ct_ptype, raw_length);
		dm2_ns_calc_disk_used(ns_stoken, ct->ct_ptype,
				      namespace_physid_hash, physid);
	    }
	    DBG_DM2M2("ReadDir UriNAME: %s %s",
		      uh->uri_name, uh->uri_container->c_uri_dir);
	}
    }

    NKN_ASSERT(uri_head_wlocked == 0);
    if (cfd != -1) {
	glob_dm2_container_close_cnt++;
	nkn_close_fd(cfd, DM2_FD);
    }
    *(slash+1) = '/';
    if (!preread && buf) {
	/* free in function so don't add to global */
	dm2_free(buf, RD_EXT_BUFSZ, DEV_BSIZE);
    }
    cont->c_rflags |= DM2_CONT_RFLAG_EXTS_READ;
    cont_head->ch_last_cont_read_ts = nkn_cur_ts;
    if (namespace_physid_hash) {
	nchash_foreach(namespace_physid_hash,
		       dm2_free_physid_hash_entry,
		       namespace_physid_hash);
	nchash_destroy(namespace_physid_hash);
    }
    if (good_stoken)
	ns_stat_free_stat_token(ns_stoken);
    return 0;

 free_extents:
 free_buf:
    if (uh && uri_head_wlocked)
	dm2_uri_head_rwlock_wunlock(uh, ct, DM2_NOLOCK_CI);
    if (cfd != -1) {
	glob_dm2_container_close_cnt++;
	ct->ct_dm2_container_close_cnt++;
	nkn_close_fd(cfd, DM2_FD);
    }
    *(slash+1) = '/';
    if (!preread && buf)
	dm2_free(buf, RD_EXT_BUFSZ, DEV_BSIZE);
    /* These 2 hash tables only store physids rather than ptrs, so no private
     * objects to free */

    if (namespace_physid_hash) {
	nchash_foreach(namespace_physid_hash,
		       dm2_free_physid_hash_entry,
		       namespace_physid_hash);
	nchash_destroy(namespace_physid_hash);
    }
 free_stoken:
    if (good_stoken)
	ns_stat_free_stat_token(ns_stoken);
    return ret;
}	/* dm2_read_cont_extents_all_uris2 */


/*
 * Read in all containers across all disk caches with the same uri directory
 *
 * The only errors which can be returned from this routine are ENOMEM and
 * disk failure.  We should never run out of memory, but disks can go bad.
 * Let's free up the memory for the drive when the drive is shutdown.
 *
 * cont_head is already locked.
 */
int
dm2_read_contlist(dm2_container_head_t *cont_head,
		  uint32_t preread,
		  dm2_cache_type_t *ct)
{
    dm2_container_t *cont;
    GList	    *cptr;
    int		    ret;

    cptr = cont_head->ch_cont_list;
    while (cptr) {
	cont = (dm2_container_t *)cptr->data;
	if (cont->c_dev_ci->ci_disabling)
	    goto next_cont;
	dm2_ci_dev_rwlock_rlock(cont->c_dev_ci);
	ret = dm2_read_cont_extents_all_uris2(cont_head, preread,
					      cont->c_dev_ci,
					      cont, ct);
	if (ret < 0) {
	    /* Out of memory or disk failure.
	     * ENOCSI - We used up all the memory that was given to us.
	     * If we were pre-reading we would have just read in only some of
	     * the objects in the disk and pre-read would abort early.
	     */
	    if (ret == -ENOMEM || ret == -ENOCSI) {/* Cannot do anything else */
		dm2_ci_dev_rwlock_runlock(cont->c_dev_ci);
		DBG_DM2S("Ran out of memory: [cache:%s] %s",
			 cont->c_dev_ci->mgmt_name, cont->c_uri_dir);
		return ret;
	    }
	    NKN_ASSERT(0);
	    /* XXXmiken: Let's mark this drive bad */
	}
	dm2_ci_dev_rwlock_runlock(cont->c_dev_ci);
 next_cont:
	cptr = cptr->next;
    }
    return 0;
}	/* dm2_read_contlist */


static int
dm2_delete_expired_uol(dm2_cache_type_t *ct,
		       MM_put_data_t	*put,
		       char		*put_uri,
		       DM2_uri_t	**uri_head_ret,
		       dm2_cache_info_t **bad_ci_ret)
{
    DM2_uri_t	*uri_head = *uri_head_ret;
    int		ret;

    DBG_DM2M2("PUT: URI Expired=%s current=%ld read=%d",
	      uri_head->uri_name, nkn_cur_ts, uri_head->uri_expiry);
    dm2_uri_head_rwlock_wunlock(uri_head, ct, DM2_RUNLOCK_CI);
    ret = dm2_common_delete_uol(ct, put->uol.cod, put->ptype,
				 put->sub_ptype, put_uri,
				 bad_ci_ret, DM2_DELETE_EXPIRED_UOL);
    if (ret < 0)
	return ret;

    if (put->attr == NULL) {
	/*
	 * If the delete request which causes the object to be deleted
	 * is not the first object, then this pointer will be NULL and
	 * we will continue to reject any further PUTs for this long
	 * object.
	 */
	DBG_DM2W("PUT request missing attribute object: URI=%s off=%ld len=%ld",
		 put_uri, put->uol.offset, put->uol.length);
	return -EINVAL;
    }
    *uri_head_ret = NULL;
    return 0;
}	/* dm2_delete_expired_uol */


static int
dm2_delete_replace_uri(MM_put_data_t	*put,
		       char		*put_uri,
		       dm2_cache_type_t	*ct,
		       DM2_uri_t	**uri_head_ret,
		       dm2_cache_info_t **bad_ci_ret)
{
    DM2_uri_t *uri_head = *uri_head_ret;
    int ret = 0;

    /* check for version mismatch */
    ret = nkn_cod_test_and_set_version(put->uol.cod, uri_head->uri_version, 0);
    NKN_ASSERT(ret != NKN_COD_INVALID_COD);
    if (ret != 0)
	ct->ct_dm2_put_cod_mismatch_warn++;

    /*
     * Delete the object, if its either bad or expired or a new version
     * of the object is available even if not expired.
     */
    if ((uri_head->uri_at_off == DM2_ATTR_INIT_OFF) ||
	uri_head->uri_chksum_err || uri_head->uri_invalid ||
	((!(put->flags & MM_PUT_IGNORE_EXPIRY)) &&
	 ((int)uri_head->uri_expiry) != NKN_EXPIRY_FOREVER &&
	 uri_head->uri_expiry < nkn_cur_ts) || (ret != 0)) {
	glob_dm2_put_expiry_cnt++;

	if (uri_head->uri_expiry == 0)
	    DBG_DM2W("[cache_type=%s] Empty expiry for URI=%s",
		     ct->ct_name, put_uri);

	/*
	 * While deleting an expired URI, its possible that this delete
	 * shall delete the other URIs sharing the same block.
	 * In this situation one of the following could happen
	 *
	 * 1 - If an object gets expired, this could cause another PUT. However,
	 * before the PUT, it needs to be write locked. If the neighboring
	 * DELETE arrives first, it grabs write locks for the entire block
	 * which will lock out the PUT write lock.  Once the DELETE finishes,
	 * the PUT write lock will return a NULL as the object doesn't exist.
	 * This should allow the PUT to proceed.
	 *
	 * 2 - If the PUT arrives first, it grabs the write lock for that URI.
	 * That will lock out the neighboring DELETE.  As the PUT will also
	 * attempt to delete the object first, it must release the write lock
	 * before it calls DELETE. There are now two racing deletes on the same
	 * block. The PUT delete will most likely lose because of the queuing
	 * model, so it gets an ENOENT returned from the delete request.
	 */
	ret = dm2_delete_expired_uol(ct, put, put_uri, uri_head_ret,
				     bad_ci_ret);
	if (ret < 0 && ret != -ENOENT) {
	    glob_dm2_put_delete_expired_uol_err++;
	    return ret;
	}
	/*
	 * Dualing duplicate PUTs may cause the same object to be placed
	 * twice in the same cache tier if we continue from here.  Instead,
	 * go to the top now that the object has been deleted
	 */
	return -EAGAIN;
    }

    /* No replacement */
    return 0;
}	/* dm2_delete_replace_uri */


/*
 * Besides having a need to find out whether this object is already in the
 * cache, we can find out if there is an existing container and if there is
 * an existing disk block which is not filled completely.
 */
int
dm2_find_obj_in_cache(MM_put_data_t	*put,
		      char		*put_uri,
		      dm2_cache_type_t	*ct,
		      DM2_uri_t		**uri_head_ret,
		      dm2_cache_info_t	**bad_ci_ret,
		      int		*found)
{
    dm2_container_head_t *cont_head = NULL;
    DM2_uri_t		 *uri_head;
    DM2_extent_t	 *ext;
    dm2_container_t	 *cont;			// Not used
    off_t		 tot_len = 0;
    int			 ret = 0;
    int			 valid_cont_found = 0;
    int			 uri_head_wlocked = 0;
    int			 stat_mutex_locked = 0;
    int			 cont_wlocked = 0;
    int			 cont_head_locked = DM2_CONT_HEAD_UNLOCKED;
    size_t		 uri_size;

    *found = 0;

    /* Make sure we have enough memory to add this URI/extent to the
     * dictionary. If 'put->attr' is valid, assume that the URI is not
     * there and account for the uri_head also.
     */
    if (put->attr)
	uri_size = sizeof(DM2_uri_t) + sizeof(DM2_extent_t);
    else
	uri_size = sizeof(DM2_extent_t);
    if ((ret = dm2_mem_check(uri_size)) < 0) {
	ret = -ENOMEM;
	goto no_locks;
    }

    uri_head = dm2_uri_head_get_by_uri_wlocked(put_uri, cont_head,
					       &cont_head_locked,
					       ct, 0, DM2_RLOCK_CI);
    if (uri_head == NULL) {
	if (dm2_quick_uri_cont_stat(put_uri, ct, DM2_BLOCKING) == -ENOENT) {
	    glob_dm2_quick_put_not_found++;
	    goto unlock_success;
	}

	if (ct->ct_tier_preread_stat_opt) {
	    /* As long as ct_tier_preread_stat_opt is set and preread is done,
	     * we know that there are no directories which are not found */
	    ct->ct_dm2_put_opt_not_found++;
	    goto unlock_success;
	}

	/* This is really a lock to do a disk read of containers */
	dm2_ct_stat_mutex_lock(ct, &stat_mutex_locked, put_uri);
	ret = dm2_get_contlist(put_uri, 0, NULL, ct, &valid_cont_found,
			       &cont_head, &cont, &cont_wlocked,
			       &cont_head_locked);
	if (ret < 0)
	    goto unlock_stat_mutex;
	NKN_ASSERT(cont_wlocked == 0);	/* create can not happen here */

	/*
	 * If no cache hits now, don't bother creating a uri_head or
	 * container just yet.  Leave that for later.
	 */
	if (valid_cont_found == 0)
	    goto unlock_success;

	if ((ret = dm2_read_contlist(cont_head, 0, ct)) < 0)
	    goto unlock_with_error;

	if ((ret = dm2_read_container_attrs(cont_head, &cont_head_locked, 0, ct)) < 0)
	    goto unlock_with_error;

	dm2_ct_stat_mutex_unlock(ct, &stat_mutex_locked, put_uri);
	uri_head = dm2_uri_head_get_by_uri_wlocked(put_uri, cont_head,
						   &cont_head_locked,
						   ct, 0, DM2_RLOCK_CI);
	if (uri_head == NULL) {
	    if (put->attr && nkn_attr_is_streaming(put->attr)) {
		/* Check if we have reached the threshold of max simultaeous
		 * streaming objects, if so return now */
		if (AO_load(&ct->ct_dm2_streaming_curr_cnt) >=
			    DM2_CT_MAX_STREAMING_PUTS) {
		    DBG_DM2S("[cache_type=%s] Too many streaming objects "
			     "rejecting PUT for URI=%s", ct->ct_name, put_uri);
		    ret = -NKN_DM2_TOO_MANY_STREAMING_PUTS;
		    goto unlock_with_error;
		}
	    }
	    goto unlock_success;
	}
	dm2_conthead_rwlock_unlock(cont_head, ct, &cont_head_locked);
    }
    uri_head_wlocked = 1;
    if (uri_head->uri_bad_delete) {
	/* If the object is bad, then we won't allow the object to be replaced
	 * or PUT to.  This means we just reject new PUT operations for obj */
	DBG_DM2M("[cache_type=%s] PUT rejected for bad URI: %s",
		 ct->ct_name, uri_head->uri_name);
	ct->ct_dm2_put_bad_uri_err++;
	ret = -EPERM;
	goto unlock_with_error;
    }
    /*
     * If the uri_head already exists and the attribute is still not
     * read in, attempt to re-ingest the object by deleting the object
     */
    if (put->attr && uri_head->uri_at_off == DM2_ATTR_INIT_OFF) {
	DBG_DM2W("Attribute Uninitialized. Probably URI not fully constructed");
	DBG_DM2W("URI %s wil be ingested fresh", uri_head->uri_name);
	glob_dm2_put_no_attr++;
    }

    if ((uri_head->uri_obj_type & NKN_OBJ_STREAMING) &&
	(uri_head->uri_content_len != (uint64_t)put->uol.offset)) {
	    DBG_DM2S("[cache_type=%s] Unexpected offset %ld for URI (%s) "
		     "new ingest", ct->ct_name, put->uol.offset, put_uri);
	    glob_dm2_put_bad_offset_err++;
	    ret = -EINVAL;
	    goto unlock_with_error;
    }

    /* Do we need to replace this URI? */
    ret = dm2_delete_replace_uri(put, put_uri, ct, &uri_head, bad_ci_ret);
    if (ret != 0) {
	/* No longer write locked if we have attempted a delete.
	 * If deleted, the return value would be -EAGAIN */
	goto unlock_stat_mutex;
    }

    /* On any further errors, uri_head_ret should be reset before returning */
    *uri_head_ret = uri_head;

    if (uri_head->uri_ext_list == NULL) {
	/* No extents present yet: short circuit */
	goto unlock_success;
    }

    NKN_ASSERT(uri_head->uri_ext_list);
    NKN_ASSERT(uri_head->uri_name);

    if (put->attr && uri_head->uri_at_len != 0) {
	/* Attribute already written, if this is a PUSH ingest
	 * reset put->attr */
	if (put->flags & MM_PUT_PUSH_INGEST)
	    put->attr = NULL;
    }


    ext = dm2_find_extent_by_offset(uri_head->uri_ext_list, put->uol.offset,
				    put->uol.length, &tot_len, NULL, NULL);
    if (ext != NULL)
	*found = 1;

    if (*found && (tot_len < (put->uol.offset + put->uol.length))) {

	*found = NKN_DM2_PARTIAL_EXTENT_EEXIST;
	//ret = -NKN_DM2_PARTIAL_EXTENT_EEXIST;
	//goto unlock_with_error;
	glob_dm2_partial_extent_eexit_cnt++;
    }

    if (*found == 0 && put->attr && uri_head->uri_at_len != 0) {
	/*
	 * Only one DM2_put() call per URI can have an attribute associated
	 * with it.
	 */
	DBG_DM2S("URI (%s) already has attributes at off=%ld",
		 uri_head->uri_name, uri_head->uri_at_off);
	DBG_DM2M("Only one PUT call per URI can have an attribute "
		 "ptr associated with it.");
	/* Since we unlock the uri_head on error, reset uri_head_ret */
	*uri_head_ret = NULL;
	ret = -EINVAL;
	goto unlock_with_error;
    }

 unlock_success:

    /*
     * This is the 1st PUT call for a non-existing URI.  Hence, the
     * PUT attribute field must be non-NULL because providers need
     * to know the content-length.
     */
    if (uri_head == NULL && put->attr == NULL) {
	DBG_DM2S("[cache_type=%s] PUT req missing attr object: URI=%s "
		 "off=%ld, len=%ld", ct->ct_name, put_uri,
		 put->uol.offset, put->uol.length);
	ret = -EINVAL;
	goto unlock_with_error;
    }
    if (stat_mutex_locked)
	dm2_ct_stat_mutex_unlock(ct, &stat_mutex_locked, put_uri);
    if (cont_head_locked != DM2_CONT_HEAD_UNLOCKED)
	dm2_conthead_rwlock_unlock(cont_head, ct, &cont_head_locked);
    /* return with uri head write locked */
    return 0;

 unlock_with_error:
    if (cont_head_locked != DM2_CONT_HEAD_UNLOCKED)
	dm2_conthead_rwlock_unlock(cont_head, ct, &cont_head_locked);
    if (uri_head_wlocked)
	dm2_uri_head_rwlock_wunlock(uri_head, ct, DM2_RUNLOCK_CI);
 unlock_stat_mutex:
    if (stat_mutex_locked)
	dm2_ct_stat_mutex_unlock(ct, &stat_mutex_locked, put_uri);
    if (cont_wlocked)
	dm2_cont_rwlock_wunlock(cont, &cont_wlocked, put_uri);
 no_locks:
    return ret;
}	/* dm2_find_obj_in_cache */


int
dm2_open_attrpath(char *attrpath)
{
    int afd, ret;

    if ((afd = dm2_open(attrpath, O_RDWR | O_CREAT, 0644)) == -1) {
	ret = -errno;
	NKN_ASSERT(ret != -EMFILE);
	glob_dm2_attr_open_err++;
	return ret;
    }
    nkn_mark_fd(afd, DM2_FD);
    glob_dm2_attr_open_cnt++;
    NKN_ASSERT(afd != 0);
    return afd;
}	/* dm2_open_attrpath */


/*
 * This routine is not just called from dm2_stampout_attr, which is only called
 * through the DELETE path.
 */
int
dm2_do_stampout_attr(const int		   afd,
		     DM2_disk_attr_desc_t  *dattrd,
		     const dm2_container_t *cont,
		     const off_t	   loff,
		     const char		   *p_basename,
		     const int		   is_bad,
		     const int		   is_corrupt)
{
    off_t nbytes;
    dm2_cache_info_t *ci = cont->c_dev_ci;
    dm2_cache_type_t *ct = ci->ci_cttype;
    char *dc_name = cont->c_dev_ci->mgmt_name;
    int ret = 0;

    nbytes = pread(afd, dattrd, DEV_BSIZE, loff);
    if (nbytes == -1) {
	ret = -errno;
	DBG_DM2S("IO ERROR:[mgmt=%s] fd(%d) read (%s) expected(%u) read "
                 "from(0x%x) errno(%d)", dc_name, afd, cont->c_uri_dir,
                 DEV_BSIZE, loff, -ret);
	NKN_ASSERT(ret != -EBADF);
	if (ret != -EIO) {
	    ret = -EINVAL; //ret = -EIO;
	    glob_dm2_attr_delete_eio_override++;
	}
	glob_dm2_attr_delete_read_err++;
	NKN_ASSERT(dm2_assert_for_disk_inconsistency);
	goto error;
    }
    if (nbytes != DEV_BSIZE) {
	DBG_DM2S("IO ERROR:[mgmt=%s] read (%s) short read at %lu: "
		 "expected=%u got=%lu errno=%d",dc_name, cont->c_uri_dir,
		   loff, DEV_BSIZE, nbytes, errno);
	glob_dm2_attr_delete_short_read_err++;
	ci->ci_dm2_attr_delete_short_read_err++;
	NKN_ASSERT(dm2_assert_for_disk_inconsistency);
	if (nbytes == 0) {
	    /* XXXmiken: this should never happen */
	    DBG_DM2S("Attribute not found: %s", p_basename);
	    ret = -EINVAL;
	} else {
	    ret = -EINVAL; //ret = -EIO;
	    glob_dm2_attr_delete_eio_override++;
	}
	goto error;
    }
    dm2_inc_attribute_read_bytes(ct, ci, nbytes);
    if (dattrd->dat_magic == DM2_ATTR_MAGIC_GONE && is_bad) {
	/* May have already deleted the attribute */
	glob_dm2_attr_delete_repeated_delete_cnt++;
	return -NKN_DM2_ATTR_DELETED;
    }
    if (!is_corrupt && dattrd->dat_magic != DM2_ATTR_MAGIC) {
	DBG_DM2S("[mgmt=%s] Illegal magic number for %s at %lu: expected=0x%x "
		 "got=0x%x", dc_name,
		 cont->c_uri_dir, loff, DM2_ATTR_MAGIC, dattrd->dat_magic);
	glob_dm2_attr_delete_bad_magic_err++;
	ci->ci_dm2_attr_delete_bad_magic_err++;
	NKN_ASSERT(dm2_assert_for_disk_inconsistency);
	if (dattrd->dat_magic == DM2_ATTR_MAGIC_GONE) {
	    /* XXXmiken: this should never happen */
	    DBG_DM2S("Attribute already deleted (possibly never written): %s",
		     p_basename);
	    ret = -EINVAL;
	} else {
	    ret = -EINVAL; //ret = -EIO;
	    glob_dm2_attr_delete_eio_override++;
	}
	goto error;
    }
    if (!is_corrupt && dattrd->dat_version != DM2_ATTR_VERSION) {
	DBG_DM2S("[mgmt=%s] Wrong version number for %s at %lu: expected=%d "
		 "got=%d", dc_name, cont->c_uri_dir, loff, DM2_ATTR_VERSION,
		 dattrd->dat_version);
	ret = -EINVAL; //ret = -EIO;
	glob_dm2_attr_delete_eio_override++;
	glob_dm2_attr_delete_bad_version_err++;
	NKN_ASSERT(dm2_assert_for_disk_inconsistency);
	goto error;
    }
    if (!is_corrupt && strcmp(p_basename, dattrd->dat_basename) != 0) {
	DBG_DM2S("[mgmt=%s] Wrong basename for %s at %lu: expected=%s got=%s",
		 dc_name, cont->c_uri_dir, loff, p_basename,
		 dattrd->dat_basename);
	glob_dm2_attr_delete_basename_mismatch_err++;
	ci->ci_dm2_attr_delete_basename_err++;
	NKN_ASSERT(dm2_assert_for_disk_inconsistency);
	/* XXXmiken: this should never happen */
	ret = -EINVAL;
	goto error;
    }

    /* Do the write */
    dattrd->dat_magic = DM2_ATTR_MAGIC_GONE;
    if (is_corrupt)	// Mark this slot as corrupted
	dattrd->dat_version = 0x99;
    nbytes = pwrite(afd, dattrd, DEV_BSIZE, loff);
    if (nbytes == -1) {
	ret = errno;
	DBG_DM2S("IO ERROR:[mgmt=%s] Failed write (%s) (%u) bytes"
		" at %lu in fd(%d) errno(%d)", dc_name, cont->c_uri_dir,
		DEV_BSIZE, loff, afd, ret);
	NKN_ASSERT(ret != EBADF);
	if (ret != EIO) {
	    glob_dm2_attr_delete_eio_override++;
	    ret = -EINVAL;
	}
	glob_dm2_attr_delete_write_err++;
	NKN_ASSERT(dm2_assert_for_disk_inconsistency);
	goto error;
    }
    if (nbytes != DEV_BSIZE) {
	DBG_DM2S("IO ERROR:[mgmt=%s] write (%s) short write at %lu: "
		 "expected=%u got=%lu errno=%d", dc_name,
		 cont->c_uri_dir, loff, DEV_BSIZE, nbytes, errno);
	glob_dm2_attr_delete_eio_override++;
	ret = -EINVAL; //ret = -EIO;
	glob_dm2_attr_delete_short_write_err++;
	NKN_ASSERT(dm2_assert_for_disk_inconsistency);
	goto error;
    }
    dm2_inc_attribute_write_bytes(ct, ci, nbytes);

 error:
    return ret;
}	/* dm2_do_stampout_attr */


/*
 * Delete path, so performance not critical.  Perform read/modify write to
 * get higher reliability.
 *
 * If any of the checks in this routine fail, we need to delete the attribute
 * and associated container file and free up all the space. XXXmiken
 *
 * Should every error in here be EIO? XXXmiken
 */
int
dm2_stampout_attr(DM2_uri_t *uri_head)
{
    off_t		 loff;
    struct stat		 sb;
    dm2_container_t	 *cont = uri_head->uri_container;
    DM2_disk_attr_desc_t *dattr;
    char		 *buf, *abuf, *p_basename, *attrpath;
    int			 afd = -1, ret = 0;

    /* Get an aligned buffer */
    buf = alloca(DEV_BSIZE*2);
    abuf = (char *)roundup((off_t)buf, DEV_BSIZE);
    dattr = (DM2_disk_attr_desc_t *)abuf;
    p_basename = dm2_uh_uri_basename(uri_head);

    DM2_GET_ATTRPATH(cont, attrpath);
    /* Do the read */
    if (uri_head->uri_at_off == DM2_ATTR_INIT_OFF) {
	DBG_DM2S("URI=%s has no attributes to delete", uri_head->uri_name);
	glob_dm2_attr_delete_no_attr_err++;
	return 0;
    }
    loff = uri_head->uri_at_off * DEV_BSIZE;
    if ((ret = dm2_open_attrpath(attrpath)) < 0) {
	DBG_DM2S("[name=%s] Failed to open attribute file (%s): %d",
		 cont->c_dev_ci->mgmt_name, attrpath, ret);
	glob_dm2_attr_delete_open_err++;
	goto error;
    }
    afd = ret;
    ret = dm2_do_stampout_attr(afd, dattr, cont, loff, p_basename,
			       uri_head->uri_bad_delete, DM2_ATTR_IS_OK);
    if (ret != 0 && ret != -NKN_DM2_ATTR_DELETED) {
	glob_dm2_attr_delete_err++;
	if (fstat(afd, &sb) == 0) {
	    DBG_DM2S("[name=%s] fstat size on %s: %ld",
		     cont->c_dev_ci->mgmt_name, p_basename, sb.st_size);
	} else {
	    DBG_DM2S("[name=%s] fstat failed on %s: %d",
		     cont->c_dev_ci->mgmt_name, p_basename, ret);
	}
    } else if (ret == 0) {
#ifdef SLOT_DEBUG
	DBG_DM2S("Adding attr slot %ld to [name=%s] (%s) (%s)",
		 loff, cont->c_dev_ci->mgmt_name, cont->c_uri_dir,
		 uri_head->uri_name);
#endif
	dm2_attr_slot_push_tail(cont, loff);
    }

    if (ret == -NKN_DM2_ATTR_DELETED)
	ret = 0;
 error:
    if (afd != -1) {
	glob_dm2_attr_close_cnt++;
	nkn_close_fd(afd, DM2_FD);
    }
    return ret;
}	/* dm2_stampout_attr */


static int
dm2_do_stampout_extents(int cfd,
			DM2_uri_t *uri_head)
{
    off_t		loff, nbytes;
    dm2_container_t	*cont = uri_head->uri_container;
    dm2_cache_info_t	*ci = cont->c_dev_ci;
    dm2_cache_type_t	*ct = ci->ci_cttype;
    DM2_disk_extent_t	*dext;
    DM2_disk_extent_header_t	*dhdr;
    DM2_extent_t	*ext;
    GList		*obj;
    char		*buf, *abuf, *p_basename, *uri;
    int			ret = 0;

    /* Get an aligned buffer */
    buf = alloca(DEV_BSIZE*2);
    abuf = (char *)roundup((off_t)buf, DEV_BSIZE);
    dext = (DM2_disk_extent_t *)abuf;
    dhdr = (DM2_disk_extent_header_t *)abuf;
    uri = uri_head->uri_name;

    /* Let's skip locking the uri_ext_list because the uri_head is write
     * locked and no one can update the uri_ext_list right now except us */
    for (obj = uri_head->uri_ext_list; obj; obj = obj->next) {
	ext = (DM2_extent_t *)obj->data;

	/* Do the read */
	if (ext->ext_cont_off == DM2_EXT_INIT_CONT_OFF) {
	    glob_dm2_delete_no_ext++;
	    continue;
	}
	loff = ext->ext_cont_off * DEV_BSIZE;
	nbytes = pread(cfd, abuf, cont->c_obj_sz, loff);
	if (nbytes == -1) {
	    ret = -errno;
	    DBG_DM2S("IO ERROR:[URI=%s] read (%s) fd(%d) expected(%u) "
            "read from (0x%lx) errno(%d)", uri, cont->c_uri_dir, cfd,
            cont->c_obj_sz, loff, -ret);
	    NKN_ASSERT(ret != -EBADF);
	    if (ret != -EIO) {
		ret = -EINVAL; //ret = -EIO;
		glob_dm2_ext_delete_eio_override++;
	    }
	    glob_dm2_ext_delete_read_err++;
	    NKN_ASSERT(dm2_assert_for_disk_inconsistency);
	    goto error_unlock;
	}
	if (nbytes != cont->c_obj_sz) {
	    DBG_DM2S("[URI=%s] read (%s) short read: expected=%u got=%lu",
		     uri, cont->c_uri_dir, cont->c_obj_sz, nbytes);
	    glob_dm2_ext_delete_short_read_err++;
	    ci->ci_dm2_ext_delete_short_read_err++;
	    NKN_ASSERT(dm2_assert_for_disk_inconsistency);
	    if (nbytes == 0) {
		/* XXXmiken: this should never happen! */
		DBG_DM2S("[disk=%s] extent at off=%ld not found",
			 ci->mgmt_name, loff);
		ret = -EINVAL;
		continue;
	    } else {
		ret = -EINVAL; //ret = -EIO;
		glob_dm2_ext_delete_eio_override++;
	    }
	    goto error_unlock;
	}
	dm2_inc_container_read_bytes(ct, ci, nbytes);
	if (dhdr->dext_magic == DM2_DISK_EXT_DONE && uri_head->uri_bad_delete) {
	    DBG_DM2W("[URI=%s] Already deleted extent in %s at %lu",
		     uri, cont->c_uri_dir, loff);
	    glob_dm2_ext_delete_repeated_delete_cnt++;
	    /* XXXmiken: Should we loop to delete other extents? */
	    return 0;
	}
	if (dhdr->dext_magic != DM2_DISK_EXT_MAGIC) {
	    DBG_DM2S("[URI=%s] Illegal magic number for %s at %lu: "
		     "expected=0x%x got=0x%x", uri, cont->c_uri_dir, loff,
		     DM2_DISK_EXT_MAGIC, dhdr->dext_magic);
	    glob_dm2_ext_delete_bad_magic_err++;
	    ci->ci_dm2_ext_delete_bad_magic_err++;
	    NKN_ASSERT(dm2_assert_for_disk_inconsistency);
	    ret = -EINVAL;
	    if (dhdr->dext_magic == DM2_DISK_EXT_DONE) {
		/* XXXmiken: this should never happen! */
		DBG_DM2S("[mgmt=%s] Extent already deleted", ci->mgmt_name);
		ret = -EINVAL;
		continue;
	    } else {
		ret = -EINVAL; //ret = -EIO;
		glob_dm2_ext_delete_eio_override++;
	    }
	    goto error_unlock;
	}
	if (dhdr->dext_version != DM2_DISK_EXT_VERSION &&
	    dhdr->dext_version != DM2_DISK_EXT_VERSION_V2) {
	    DBG_DM2S("[URI=%s] Wrong version number for %s at %lu:"
		     " expected=%d/%d got=%d", uri, cont->c_uri_dir, loff,
		    DM2_DISK_EXT_VERSION, DM2_DISK_EXT_VERSION_V2,
                    dhdr->dext_version);
	    ret = -EINVAL; //ret = -EIO;
	    glob_dm2_ext_delete_eio_override++;
	    glob_dm2_ext_delete_bad_version_err++;
	    NKN_ASSERT(dm2_assert_for_disk_inconsistency);
	    goto error_unlock;
	}
	p_basename = dm2_uh_uri_basename(uri_head);
	if (strcmp(p_basename, dext->dext_basename) != 0) {	// mismatch
	    DBG_DM2S("Wrong basename for %s at %lu: expected=%s got=%s",
		     cont->c_uri_dir, loff, p_basename,	dext->dext_basename);
	    glob_dm2_ext_delete_basename_mismatch_err++;
	    ci->ci_dm2_ext_delete_basename_err++;
	    NKN_ASSERT(dm2_assert_for_disk_inconsistency);
	    /* XXXmiken: this should never happen */
	    ret = -EINVAL;
	    continue;
	    /*goto error_unlock;*/
	}

	/* Do the write */
	dhdr->dext_magic = DM2_DISK_EXT_DONE;
	nbytes = pwrite(cfd, abuf, cont->c_obj_sz, loff);
	if (nbytes == -1) {
	    ret = errno;
	    DBG_DM2S("IO ERROR:[URI=%s] container (%s) magic overwrite "
                     "failed @ off=%ld (%u) bytes in fd(%d) errno(%d)",
                     uri, cont->c_uri_dir, loff, cont->c_obj_sz, cfd, ret);
	    NKN_ASSERT(ret != EBADF);
	    if (ret != EIO) {
		ret = -EINVAL;
		glob_dm2_ext_delete_eio_override++;
	    }
	    glob_dm2_ext_delete_write_err++;
	    NKN_ASSERT(dm2_assert_for_disk_inconsistency);
	    /*
	     * XXXmiken: The above cases which should never happen are
	     * happening.  If two writes to the same offset are missed,
	     * we could end up here.
	     */
	    goto error_unlock;
	}
	if (nbytes != cont->c_obj_sz) {
	    DBG_DM2S("[URI=%s] container (%s) magic overwrite is short @ "
		     "off=%ld: expected=%u got=%lu",
		     uri, cont->c_uri_dir, loff, cont->c_obj_sz, nbytes);
	    ret = -EINVAL; //ret = -EIO;
	    glob_dm2_ext_delete_eio_override++;
	    glob_dm2_ext_delete_short_write_err++;
	    NKN_ASSERT(dm2_assert_for_disk_inconsistency);
	    goto error_unlock;
	}
	dm2_inc_container_write_bytes(ct, ci, nbytes);
	dm2_ext_slot_push_tail(cont, loff);
#ifdef SLOT_DEBUG
	DBG_DM2S("Adding ext slot %ld to (%s) (%s)",
		 loff, cont->c_uri_dir, uri);
#endif
    }

 error_unlock:
    return ret;

}	/* dm2_do_stampout_extents */


int
dm2_stampout_extents(DM2_uri_t *uri_head)
{
    dm2_container_t *cont = uri_head->uri_container;
    char *cont_pathname;
    int  cfd, ret = 0;

    DM2_GET_CONTPATH(cont, cont_pathname);

    cfd = dm2_open(cont_pathname, O_RDWR | O_CREAT, 0644);
    if (cfd == -1) {
	ret = -errno;
	NKN_ASSERT(ret != -EMFILE);
	DBG_DM2S("Container open failed (%s): %d", cont_pathname, ret);
	glob_dm2_container_open_err++;
	goto error;
    }
    nkn_mark_fd(cfd, DM2_FD);
    glob_dm2_container_open_cnt++;
    ret = dm2_do_stampout_extents(cfd, uri_head);
    if (ret != 0)
	glob_dm2_ext_delete_err++;

 error:
    if (cfd != -1) {
	glob_dm2_container_close_cnt++;
	nkn_close_fd(cfd, DM2_FD);
    }
    return ret;
}	/* dm2_stampout_extents */


int
dm2_delete_physid_extents(DM2_uri_t *uri_head,
			  dm2_cache_type_t *ct,
			  ns_stat_token_t ns_stoken)
{
    GList	 *elist;	// Extent list
    DM2_extent_t *ext;
    char *uri_dir = NULL;
    dm2_cache_info_t *ci = uri_head->uri_container->c_dev_ci;
    int		 ret, list_ret = 0;
    int		 num_free_disk_pages = 0;
    int		 ph_empty;

    uri_dir = uri_head->uri_container->c_uri_dir;

    /* Let's skip locking the uri_ext_list because the uri_head is write
     * locked and no one can update the uri_ext_list right now except us */
    for (elist = uri_head->uri_ext_list; elist; elist = elist->next) {
	ph_empty = 0;
	ext = (DM2_extent_t *)elist->data;
	num_free_disk_pages +=
	    Byte2DP_pgsz(ext->ext_length, dm2_get_pagesize(ci));
	ret = dm2_physid_tbl_ext_remove(&ext->ext_physid, ext, ct, &ph_empty);
	if (ret < 0) {
	    DBG_DM2S("Physid head not found for %s,0x%lx",
		     uri_head->uri_name, ext->ext_physid);
	    //NKN_ASSERT(0);
	    list_ret = ret;
	}
	if (!ret && ph_empty) {
	    /*
	     * If the physid ext list is empty, it means that it is free now
	     * and we can account it as a block available.  The ns_stoken is
	     * NULL when we are doing a cache disable on a disk.  In that
	     * case, we don't really care about stats.
	     */
	    if (!ns_is_stat_token_null(ns_stoken))
		dm2_ns_sub_disk_usage(ns_stoken, ct->ct_ptype, 1, 0);
	}
    }
    if (!ns_is_stat_token_null(ns_stoken)) {

    dm2_ns_delete_calc_namespace_usage(uri_head->uri_container->c_uri_dir,
                                       ci->mgmt_name, ns_stoken, ct->ct_ptype,
                                       num_free_disk_pages,
				       uri_head->uri_cache_pinned);
    }
    return list_ret;
}	/* dm2_delete_physid_extents */


void
dm2_get_cache_type_resv_free_percent(dm2_cache_type_t *ct,
				     size_t *tot_free_ret,
				     size_t *tot_avail_ret)
{
    dm2_cache_info_t *ci;
    GList *ci_obj;
    size_t tot_free, tot_avail;

    dm2_ct_info_list_rwlock_rlock(ct);
    tot_free = tot_avail = 0;
    for (ci_obj = ct->ct_info_list; ci_obj; ci_obj = ci_obj->next) {
	ci = (dm2_cache_info_t *)ci_obj->data;
	tot_avail += ci->set_cache_blk_cnt;
	tot_free += AO_load(&ci->bm.bm_num_resv_blk_free);
    }
    *tot_avail_ret = tot_avail;
    *tot_free_ret = tot_free;
    dm2_ct_info_list_rwlock_runlock(ct);
}	/* dm2_get_cache_type_resv_free_percent */

/*
 * Upon entering this function, the uri_head is write locked and is removed
 * from the uri hash table.  We need to find out what other URIs share the
 * same disk block.
 *
 * At this time, URIs consuming more than a disk block get their own blocks,
 * so the operation of finding other URIs which share the disk block should
 * not cause a delete of another disk block.  Other URIs must fit within this
 * disk block.
 */
void
dm2_get_uri_del_list(dm2_cache_type_t	*ct,
		     DM2_uri_t		*uri_head_wlocked,
		     int		*occupancy,
		     GList		**uri_list_ret,
		     int		delete_pinned,
		     int		*obj_pin)
{
    GList	 *uri_list = NULL;
    GList	 *ext_list = uri_head_wlocked->uri_ext_list;
    GList	 *ph_obj_ptr, *ph_list = NULL;
    DM2_uri_t	 *uh;
    DM2_extent_t *ext;
    DM2_physid_head_t *ph;
    dm2_cache_info_t *ci = uri_head_wlocked->uri_container->c_dev_ci;
    int		 ret, pages_used = 0, num_pages = 0, pin_count = 0;
    uint32_t	 disk_page_size = dm2_get_pagesize(ci);;

    /* Let's skip locking the uri_ext_list because the uri_head is write
     * locked and no one can update the uri_ext_list right now except us */
    for (; ext_list; ext_list = ext_list->next) {
	ext = (DM2_extent_t *)ext_list->data;

	/* Simplify code by storing value as ptr to object */
	if (g_list_find(ph_list, (void *)ext->ext_physid) != NULL)
	    continue;

	ph = dm2_extent_get_by_physid_rlocked(&ext->ext_physid, ct);
	if (ph == NULL) {
	    /* When the extent can not be placed into the physid hash table
	     * due to an error on PUT, we will not find a physid here */
	    goto del_list_exit;
	    assert(0);
	}

	ph_list = g_list_prepend(ph_list, (void *)ext->ext_physid);
	ph_obj_ptr = ph->ph_ext_list;
	pages_used = 0;
	for (; ph_obj_ptr; ph_obj_ptr = ph_obj_ptr->next) {
	    ext = (DM2_extent_t *)ph_obj_ptr->data;
	    uh = ext->ext_uri_head;
	    if (g_list_find(uri_list, uh))
		continue;

	    if (uh == uri_head_wlocked) {
		/* already write locked */
		ret = dm2_uri_head_raw_lookup_by_uri(uh->uri_name, ct, ci, 1);
		NKN_ASSERT(ret != 0);
		ret = dm2_uri_head_match_by_uri_head(uri_head_wlocked, ct);
		NKN_ASSERT(ret != 0);
	    } else {
		/* write lock this URI, the delete_mutex is already held by
		 * the URI that was requested to be deleted */
		dm2_uri_head_rwlock_wlock(uh, ct, 1);
		dm2_uri_log_state(uh, DM2_URI_OP_DEL, 0, 0,
				  gettid(), nkn_cur_ts);
		num_pages = ext->ext_length / disk_page_size;
		if ((ext->ext_length % disk_page_size) > 0)
		    num_pages++;

		pages_used += num_pages;
	    }

	    if (uh->uri_cache_pinned) {
		if (delete_pinned) {
		    /* Delete is from CLI, delete the pinned object*/
		    uri_list = g_list_prepend(uri_list, uh);
		} else {
		    pin_count++;
		    dm2_uri_head_rwlock_unlock(uh, ct, DM2_NOLOCK_CI);
		    continue;
		}
	    } else {
		/* This is a normal object */
		uri_list = g_list_prepend(uri_list, uh);
	    }
	}
    dm2_physid_rwlock_runlock(ph);
    }
    if (occupancy)
	*occupancy = pages_used * disk_page_size;
 del_list_exit:
    *obj_pin = pin_count;
    g_list_free(ph_list);
    *uri_list_ret = uri_list;
}	/* dm2_get_uri_del_list */


void
dm2_unlock_uri_del_list(dm2_cache_type_t *ct,
			GList		 *uri_unlock_list)
{
    DM2_uri_t	*uh;
    GList	*uri_obj;

    for (uri_obj = uri_unlock_list; uri_obj; uri_obj = uri_obj->next) {
	uh = (DM2_uri_t *)uri_obj->data;
	uh->uri_deleting = 0;
	dm2_uri_head_rwlock_wunlock(uh, ct, DM2_NOLOCK_CI);
    }
}	/* dm2_unlock_uri_del_list */


/*
 * Called only when disabling an entire disk cache
 *
 * It is OK to have waiting readers for this object because one can come in
 * immediately after we grab the write lock for this URI.  The GET op doesn't
 * grab a ci_dev read lock.
 */
void
dm2_delete_uri(DM2_uri_t	*uh,
	       dm2_cache_type_t *ct)
{
    dm2_uri_lock_t *ulock = NULL;
    int ret;
    ns_stat_token_t ns_stoken = NS_NULL_STAT_TOKEN;
    char *uri_dir, *mgmt_name;

    dm2_uri_head_rwlock_wlock(uh, ct, 1);
    ulock = uh->uri_lock;
    assert(ulock->uri_write_cond == NULL);
    assert(ulock->uri_nreaders == 0);
    NKN_ASSERT(ulock->uri_wait_nwriters == 0);
    assert(uh->uri_deleting == 0 || uh->uri_deleting == 1);

    /* It would be best to pass a real ns_stoken but that is not much use here
     * when we are tearing down a disk cache */
    if ((ret = dm2_delete_physid_extents(uh, ct, ns_stoken)) < 0) {
	/*
	 * If this operation fails, there could be an internal inconsistency.
	 * Only bugs should cause a failure.
	 *
	 * Addendum: It looks like a failure to write a disk extent during
	 *  PUT will cause this assert to be hit because we don't delete the
	 *  object correctly after getting the EIO during PUT.
	 */
	DBG_DM2S("Failure while deleting mem extents for %s: %d",
		 uh->uri_name, -ret);
	//assert(0);
    }

    /* Remove this URI from the evict list, if it was inserted
     * We would not have inserted to the evict list only if the PUT had failed
     * or the attribute for this object was not read in */
    if (uh->uri_evict_list_add)
        dm2_evict_delete_uri(uh, am_decode_hotness(&uh->uri_hotval));

    /* Update the namespace total objects counts */
    uri_dir = alloca(MAX_URI_SIZE);
    dm2_uh_uri_dirname(uh, uri_dir, 1);
    ns_stoken = dm2_ns_get_stat_token_from_uri_dir(uri_dir);
    if (ns_is_stat_token_null(ns_stoken)) {
	/* Namespace may have been deleted asynchronously */
	DBG_DM2S("Can not get Namespace token: %s", uri_dir);
    } else {
	mgmt_name = uh->uri_container->c_dev_ci->mgmt_name;
	dm2_ns_update_total_objects(ns_stoken, mgmt_name, ct, 0);
	ns_stat_free_stat_token(ns_stoken);
    }

    /*
     * We are just deleting the uri_head entries and not deleting the actual
     * data; hence, we do not need to modify the disk blocks or the free
     * list.
     */
    dm2_free_ext_list(uh->uri_ext_list);
    glob_dm2_free_uri_head_uri_name++;
    //dm2_uri_lock_destroy(uh);
    uh->uri_deleting = 0x3;
    dm2_uri_lock_drop(uh);
    /* Free here so write-unlock can print name */
    dm2_free(uh->uri_name, uh->uri_name_len, DM2_NO_ALIGN);
    uh->uri_name = NULL;
    dm2_free(uh, sizeof(DM2_uri_t), DM2_NO_ALIGN);
    glob_dm2_free_uri_head++;

    return;
}	/* dm2_delete_uri */


void
dm2_delete_container(dm2_container_t *cont,
		     dm2_cache_type_t *ct,
		     ns_stat_token_t ns_stoken __attribute((unused)))
{
    dm2_return_unused_pages(cont, NULL);
    DBG_DM2M("[cache_type:%s] Delete container %s", ct->ct_name,
	     cont->c_uri_dir);
    cont->c_dev_ci = NULL;
    cont->c_uri_dir = NULL;

    dm2_container_destroy(cont, ct);
}	/* dm2_delete_container */


/*
 * When this funciton returns, the cont_head ptr should point to a cont_head
 * which is write locked.
 */
static int
dm2_container_insert(const char		  *uri_dir,
		     dm2_container_t	  *cont,
		     dm2_cache_type_t	  *ct,
		     dm2_container_head_t **cont_head_ret,
		     int		  *ch_locked)
{
    dm2_container_head_t *ch = *cont_head_ret;
    dm2_container_t *list_cont;
    GList           *cptr;
    int             ret;

    if (ch == NULL) {
	/* Insert new container head */
	ch = dm2_calloc(1, sizeof(dm2_container_head_t),
			mod_dm2_container_head_t);
	if (ch == NULL) {
	    DBG_DM2S("dm2_calloc failed: %ld", sizeof(dm2_container_head_t));
	    NKN_ASSERT(ch);
	    ret = -ENOMEM;
	    goto free_destroy_conthead;
	}
	ch->ch_uri_dir_len = strlen(uri_dir)+1;
	ch->ch_uri_dir = dm2_calloc(1, ch->ch_uri_dir_len, mod_dm2_uri_dir_t);
	if (ch->ch_uri_dir == NULL) {
	    DBG_DM2S("dm2_calloc failed: %d", ch->ch_uri_dir_len);
	    NKN_ASSERT(ch->ch_uri_dir);
	    ret = -ENOMEM;
	    goto free_destroy_conthead;
	}
	strcpy(ch->ch_uri_dir, uri_dir);
	dm2_conthead_rwlock_init(ch);
	/* Nothing can race with this write lock call */
	dm2_conthead_rwlock_wlock(ch, ct, ch_locked);

	/* Insert container head in container hash */
	if ((ret = dm2_conthead_tbl_insert(ch->ch_uri_dir, ch, ct)) < 0) {
	    if (ret == -EEXIST) {
		/* there should be only one entry in list */
		glob_dm2_conthead_race_cnt++;
	    } else {
		/* This could only happen with an unchecked race */
		DBG_DM2S("Failed to insert container head: %s", ch->ch_uri_dir);
	    }
	    goto free_destroy_conthead;
	}
	/* Insert in container list */
	ch->ch_cont_list = g_list_prepend(ch->ch_cont_list, cont);
	*cont_head_ret = ch;
    } else {
	/* cont_head must be write locked by me */
	//assert(ch->ch_rwlock.__data.__writer == gettid());

	cptr = ch->ch_cont_list;
	while (cptr) {
	    list_cont = (dm2_container_t *)cptr->data;
	    /* Check ifa container for this disk already exists */
	    if (list_cont->c_dev_ci == cont->c_dev_ci) {
		ret = -EEXIST;
		glob_dm2_cont_exists_cnt++;
		goto cont_exists;
	    }
	    cptr = cptr->next;
	}
	/* Insert in container list */
	ch->ch_cont_list = g_list_prepend(ch->ch_cont_list, cont);
    }
    return 0;

free_destroy_conthead:
    /*
     * undo everything which could have been done in this routine if we
     * encounter an error
     */
    if (ch && ch->ch_uri_dir) {
	dm2_free(ch->ch_uri_dir, ch->ch_uri_dir_len, DM2_NO_ALIGN);
	dm2_conthead_rwlock_destroy(ch);
    }
    if (ch) {
	dm2_free(ch, sizeof(*ch), DM2_NO_ALIGN);
	*ch_locked = DM2_CONT_HEAD_UNLOCKED;
    }
cont_exists:
    return ret;
}	/* dm2_container_insert */
