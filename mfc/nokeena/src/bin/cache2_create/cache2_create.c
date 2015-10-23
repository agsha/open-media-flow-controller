/*
 *	Author: Michael Nishimoto (miken@nokeena.com)
 *
 *	Copyright (c) Nokeena Networks Inc., 2008
 */

#include <errno.h>
#include <stdio.h>
#include "nkn_diskmgr2_api.h"

static void
cache_usage(const char *progname)
{
    fprintf(stderr, "Usage: %s <mount point> <disk block size> "
	    "<disk page size> <number of pages> <is_ram_disk>\n", progname);
    exit(1);
}	/* usage */


/*
 * Program: cache_create
 *
 * Arguments:
 *	arg1: Block size
 *	arg2: Number of blocks
 */
int
main(int  argc,
     char *argv[])
{
    char		bitmap_fname[PATH_MAX];
    dm2_bitmap_header_t	*header;
    uint32_t		disk_pagesize, tdisk_pagesize;
    uint64_t		num_disk_pages, num_bit_maps;
    uint32_t		disk_blocksize, tdisk_blocksize;
    char		*dirname;
    int			fd, nbytes, i, cnt, flags = 0;
    int			is_ram_disk;
    uint8_t		*zeros;

    if (argc < 5)
	cache_usage(argv[0]);

    dirname = argv[1];
    disk_blocksize = atoi(argv[2]);
    disk_pagesize = atoi(argv[3]);
    num_disk_pages = atoll(argv[4]);
    if (argc > 5)
	is_ram_disk = atoi(argv[5]);
    else
	is_ram_disk = 0;

    cnt = 0;
    tdisk_pagesize = disk_pagesize;
    for (i = 0; i < 32; i++) {
	if ((tdisk_pagesize & 0x1) == 0x1)
	    cnt++;
	tdisk_pagesize >>= 1;
    }
    if (cnt != 1) {
	fprintf(stderr, "Disk pagesize should be power of 2\n");
	exit(1);
    }
    if (disk_pagesize < 4*1024 || disk_pagesize > 64*1024) {
	fprintf(stderr, "Disk pagesize should be 4KiB <= X <= 64KiB\n");
	exit(1);
    }

    cnt = 0;
    tdisk_blocksize = disk_blocksize;
    for (i = 0; i < 32; i++) {
	if ((tdisk_blocksize & 0x1) == 0x1)
	    cnt++;
	tdisk_blocksize >>= 1;
    }
    if (cnt != 1) {
	fprintf(stderr, "Disk blocksize should be power of 2\n");
	exit(1);
    }
    if (disk_blocksize < 256*1024 || disk_blocksize > 2*1024*1024) {
	fprintf(stderr, "Disk blocksize should be 256KiB or 2MiB\n");
	exit(1);
    }

    snprintf(bitmap_fname, sizeof(bitmap_fname), "%s/%s",
	     dirname, DM2_BITMAP_FNAME);
    /* RAM Disk does not support O_DIRECT */
    if (is_ram_disk)
	flags = (O_WRONLY | O_CREAT);
    else
	flags = (O_WRONLY | O_CREAT | O_DIRECT);
    fd = open(bitmap_fname, flags, 0644);
    if (fd < 0) {
	fprintf(stderr, "ERROR: Creating bitmap file %s: %d\n",
		bitmap_fname, errno);
	exit(1);
    }

    header = alloca(2*sizeof(dm2_bitmap_header_t));
    header = (dm2_bitmap_header_t *)roundup((uint64_t)header, NBPG);
    memset(header, 0, sizeof(*header));
    header->u.bmh_magic = DM2_BITMAP_MAGIC;
    header->u.bmh_version = DM2_BITMAP_VERSION;
    header->u.bmh_disk_pagesize = disk_pagesize;
    header->u.bmh_num_disk_pages = num_disk_pages;
    header->u.bmh_disk_blocksize = disk_blocksize;
    nbytes = write(fd, header, sizeof(*header));
    if (nbytes != sizeof(*header)) {
	fprintf(stderr, "ERROR: Header not written correctly.  Expected "
		"%d bytes, wrote %d bytes, errno:%d\n", 
		(int)sizeof(*header), nbytes, errno);
	close(fd);
	exit(1);
    }

    /* dm2_bitmap_read_free_pages()uses NBPG alignment for reading of the file. */
    zeros = alloca(2*NBPG);
    zeros = (uint8_t *)roundup((uint64_t)zeros, NBPG);
    memset(zeros, 0, NBPG);
    num_bit_maps = num_disk_pages / 8;
    cnt = num_bit_maps / NBPG;
    if (num_bit_maps % NBPG)
	cnt++;
    for (i=0; i<cnt; i++) {
	nbytes = write(fd, zeros, NBPG);
	if (nbytes != NBPG) {
	    fprintf(stderr, "ERROR: Free bit map (%lx-%lx) not written correctly.  Expected "
		"%ld bytes, wrote %d bytes, errno:%d\n", 
		(i*NBPG), ((i+1)*NBPG),
		NBPG, nbytes, errno);
	    close(fd);
	    exit(1);
	}
    }

    close(fd);
    printf("size: 0x%lx  nbytes: %d\n", num_disk_pages, nbytes);
    return 0;
}
