/*
 *	Author: Michael Nishimoto (miken@nokeena.com)
 *
 *	Copyright (c) Nokeena Networks Inc., 2009
 */

#include <errno.h>
#include <stdio.h>
#include "nkn_diskmgr_api.h"
#include "nkn_diskmgr2_disk_api.h"
#if 0
#include "nkn_assert.h"
#else
/* TBD:NKN_ASSERT */
/* One of the NKN_ASSERTS is improper, so disable all of them until it is fixed. */
#define NKN_ASSERT(_e)
#include <assert.h>
#endif

/* PROTOTYPES */
static void usage(const char *progname) ;

int errorlog_socket;
int ram_drive = 0;

int64_t glob_dm2_bm_open_cnt,
    glob_dm2_bm_open_err,
    glob_dm2_bm_read_err,
    glob_dm2_bm_close_cnt;

int dm2_read_free_bitmap(const char	  *in_freeblk_name,
			 const int8_t cache_enabled __attribute((unused)),
			 dm2_bitmap_t *bm);


static void
usage(const char *progname)
{
    fprintf(stderr, "Usage: %s <freeblk directory name>\n", progname);
    exit(1);
}	/* usage */


static int
cmp_smallest_offset_first(gconstpointer in_a,
			  gconstpointer in_b)
{
    DM2_extent_t *a = (DM2_extent_t *)in_a;
    DM2_extent_t *b = (DM2_extent_t *)in_b;

    if (a->ext_offset > b->ext_offset)
	return 1;
    else
	return 0;
}	/* cmp_smallest_offset_first */

static int
dm2_insert_free_ext(dm2_bitmap_t *bm,
		   const off_t	 pagestart,
		   const off_t	 numpages)
{
    dm2_freelist_ext_t *fl_ext;

    fl_ext = (dm2_freelist_ext_t *)calloc(1, sizeof(dm2_freelist_ext_t));
    if (fl_ext == NULL) {
	printf("calloc failed: %lu", sizeof(dm2_freelist_ext_t));
	NKN_ASSERT(fl_ext != NULL);
	return -ENOMEM;
    }

    fl_ext->pagestart = pagestart;
    fl_ext->numpages = numpages;;
    bm->bm_free_page_list = g_list_insert_sorted(bm->bm_free_page_list, fl_ext,
						 cmp_smallest_offset_first);
    bm->bm_num_page_free += numpages;
#if 0
    if (numpages > 10) {
	printf("FreeBitMap (%s/%s): pagestart=%lu numpages=%lu total=%lu\n",
		 bm->bm_fname, bm->bm_devname, pagestart, numpages,
		 bm->bm_num_free);
    } else {
	printf("FreeBitMap (%s/%s): pagestart=%lu numpages=%lu total=%lu\n",
		  bm->bm_fname, bm->bm_devname, pagestart, numpages,
		  bm->bm_num_free);
    }
#endif

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
dm2_read_free_bitmap(const char	  *in_freeblk_name,
		     const int8_t cache_enabled __attribute((unused)),
		     dm2_bitmap_t *bm)
{
    struct stat		sb;
    dm2_bitmap_header_t	*bmh;
    uint8_t		*pbits, bits;
    off_t		pagestart, numpages, byteno;
    size_t		bmsize;
    int			ret, nbytes, filling, b, bitsleft;
    int			flags = 0;

    NKN_ASSERT(bm != NULL);
    NKN_ASSERT(in_dirname != NULL);

    strcpy(bm->bm_fname, in_freeblk_name);
    if (stat(bm->bm_fname, &sb) < 0) {
	printf("ERROR: Failed to open %s: %d\n", bm->bm_fname, errno);
	return -errno;
    }
    bm->bm_bufsize = roundup(sb.st_size, NBPG);
    bm->bm_fsize = sb.st_size;
#if 0
    bm->bm_mutex = g_mutex_new();
#endif

    if (posix_memalign((void **)&bm->bm_buf, NBPG, bm->bm_bufsize)) {
	printf("ERROR: memory allocation failed: %d\n", errno);
	return -errno;
    }

    /* Should be created by disk initialization script */
    glob_dm2_bm_open_cnt++;
    if (ram_drive)
	flags = O_RDWR;
    else
	flags = O_DIRECT | O_RDWR;
    if ((bm->bm_fd = open(bm->bm_fname, flags, 0644)) < 0) {
	printf("ERROR: Open %s failed: %d\n", bm->bm_fname, errno);
	glob_dm2_bm_open_err++;
	return -errno;
    }
#if 0
    g_mutex_lock(bm->bm_mutex);
#endif
    if ((nbytes = read(bm->bm_fd, bm->bm_buf, bm->bm_bufsize)) == -1) {
	printf("ERROR: read error (%s)[%d,0x%lx,%zu]: %d\n",
		 bm->bm_fname, bm->bm_fd, (uint64_t)bm->bm_buf, sb.st_size,
		 errno);
	ret = -errno;
	glob_dm2_bm_read_err++;
	goto error;
    }
    if (nbytes != sb.st_size) {
	printf("ERROR: Short read for %s: expected %lu bytes,read %d bytes\n",
		 bm->bm_fname, sb.st_size, nbytes);
	ret = -EIO;
	glob_dm2_bm_read_err++;
	goto error;
    }

    bmh = (dm2_bitmap_header_t *)bm->bm_buf;
    if (bmh->u.bmh_magic != DM2_BITMAP_MAGIC) {
	printf("ERROR: Wrong magic number: expected 0x%x, got 0x%x\n",
		 DM2_BITMAP_MAGIC, bmh->u.bmh_magic);
	ret = -EINVAL;
	glob_dm2_bm_read_err++;
	goto error;
    }
    if (bmh->u.bmh_version != DM2_BITMAP_VERSION) {
	printf("ERROR: Wrong version: expected %d, got %d\n",
		 DM2_BITMAP_VERSION, bmh->u.bmh_version);
	ret = -EINVAL;
	glob_dm2_bm_read_err++;
	goto error;
    }
    if (!(bmh->u.bmh_disk_pagesize == DM2_DISK_PAGESIZE_32K || 
	bmh->u.bmh_disk_pagesize == DM2_DISK_PAGESIZE_4K)) {
	printf("Warning: Unexpected disk page size: %d\n",
		 bmh->u.bmh_disk_pagesize);
    }
    bm->bm_free_page_list = NULL;

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
	bitsleft = 8;

	for (b = bitsleft; b > 0; b--) {
	    if (filling) {			// run of consecutive free blks
		if (bits == 0) {		// short circuit
		    numpages += b;
		    break;
		} else if ((bits & 0x1) == 0) {
		    numpages++;
		} else {			// bit is set
		    /* End of free block run */
		    ret = dm2_insert_free_ext(bm, pagestart, numpages);
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
	ret = dm2_insert_free_ext(bm, pagestart, numpages);
	if (ret)
	    goto error;
    }

#if 0
    g_mutex_unlock(bm->bm_mutex);
#endif
    return 0;

 error:
    // XXXmiken: need to free entire bitmap
    NKN_ASSERT(0);
    close(bm->bm_fd);
    glob_dm2_bm_close_cnt++;
    bm->bm_fd = -1;
#if 0
    g_mutex_unlock(bm->bm_mutex);
#endif
    return ret;
}	/* dm2_read_free_bitmap */


/*
 * Program: cache2_freeblk
 *
 */
int
main(int  argc,
     char *argv[])
{
    char freeblk_name[PATH_MAX];
    dm2_bitmap_t bm;
    dm2_bitmap_header_t	*bmh;
    char *progname;
    int	 ret;
    GList *ext_obj;
    dm2_freelist_ext_t *fl;

    progname = argv[0];
    if (argc < 2)
	usage(progname);

    if (argc > 2)
	ram_drive++;

#if 0
    while ((ch = getopt(argc, argv, "dhcC")) != -1) {
	switch (ch) {
	    case 'd':
		printdirname++;
		break;
	    case 'h':
		printheader++;
		break;
	    case 'c':
		compactoutput++;
		break;
	    case 'C':
		compactoutput++;
		extnumber = 0;
		break;
	    default:
		usage(progname);
		break;
	}
    }
#endif

    if (optind >= argc)
	usage(progname);

    strcpy(freeblk_name, argv[optind]);
    memset(&bm, 0, sizeof(bm));

    if ((ret = dm2_read_free_bitmap(freeblk_name, 0, &bm)) < 0) {
	fprintf(stderr, "Can not read bitmap: %d\n", ret);
	exit(1);
    }

    bmh = (dm2_bitmap_header_t *)bm.bm_buf;

    printf("\nFree Space Bitmap: %s\n", freeblk_name);
    printf("Block Size: %d\n", bmh->u.bmh_disk_blocksize);
    printf("Page Size: %d\n\n", bmh->u.bmh_disk_pagesize);
    printf("PageStart	num_pages\n");
    for (ext_obj = bm.bm_free_page_list; ext_obj; ext_obj = ext_obj->next) {
	fl = (dm2_freelist_ext_t *)ext_obj->data;

	printf("%ld		%ld\n", fl->pagestart, fl->numpages);
    }
    return 0;
}	/* main */

