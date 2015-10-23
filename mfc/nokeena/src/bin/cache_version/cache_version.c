/*
 *	Author: Michael Nishimoto (miken@ankeena.com)
 *
 *	Copyright (c) Nokeena Networks Inc., 2009
 */

#include <errno.h>
#include <stdio.h>
#include "nkn_diskmgr2_api.h"

static void
cache_vers_usage(const char *progname)
{
    fprintf(stderr, "Usage: %s [-w <version>] <mount point>\n", progname);
    exit(1);
}	/* cache_vers_usage */


static int
read_version(char *argv[])
{
    char		bitmap_fname[PATH_MAX];
    dm2_bitmap_header_t	*header;
    char		*dirname;
    int			fd, nbytes;

    dirname = argv[optind];

    header = alloca(2*sizeof(dm2_bitmap_header_t));
    header = (dm2_bitmap_header_t *)roundup((uint64_t)header, DEV_BSIZE);
    snprintf(bitmap_fname, sizeof(bitmap_fname), "%s/%s",
	     dirname, DM2_BITMAP_FNAME);
    if ((fd = open(bitmap_fname, O_RDONLY | O_DIRECT)) < 0) {
	fprintf(stderr, "ERROR: Opening bitmap file %s: %d\n",
		bitmap_fname, errno);
	return 1;
    }

    nbytes = read(fd, header, sizeof(*header));
    if (nbytes == -1) {
	fprintf(stderr, "ERROR: Header not read correctly: %d\n", errno);
	return 1;
    }
    if (nbytes != sizeof(*header)) {
	fprintf(stderr, "ERROR: Header not read correctly.  Expected "
		"%d bytes, read %d bytes\n", (int)sizeof(*header), nbytes);
	return 1;
    }
    if (header->u.bmh_magic != DM2_BITMAP_MAGIC) {
	fprintf(stderr, "ERROR: Bitmap header not correct.  Expected MAGIC "
		"0x%x, got MAGIC=0x%x\n",
		DM2_BITMAP_MAGIC, header->u.bmh_magic);
	return 1;
    }
    printf("%d\n", header->u.bmh_version);

    close(fd);
    return 0;
}	/* read_version */


static int
write_version(char *argv[],
	      int new_version)
{
    char		bitmap_fname[PATH_MAX];
    dm2_bitmap_header_t	*header;
    char		*dirname;
    int			fd, nbytes;

    dirname = argv[optind];

    header = alloca(2*sizeof(dm2_bitmap_header_t));
    header = (dm2_bitmap_header_t *)roundup((uint64_t)header, DEV_BSIZE);
    snprintf(bitmap_fname, sizeof(bitmap_fname), "%s/%s",
	     dirname, DM2_BITMAP_FNAME);

    /* Read in bitmap header */
    if ((fd = open(bitmap_fname, O_RDWR | O_DIRECT)) < 0) {
	fprintf(stderr, "ERROR: Opening bitmap file %s: %d\n",
		bitmap_fname, errno);
	return 1;
    }

    nbytes = read(fd, header, sizeof(*header));
    if (nbytes == -1) {
	fprintf(stderr, "ERROR: Header not read correctly: %d\n", errno);
	return 1;
    }
    if (nbytes != sizeof(*header)) {
	fprintf(stderr, "ERROR: Header not read correctly.  Expected "
		"%d bytes, read %d bytes\n", (int)sizeof(*header), nbytes);
	return 1;
    }
    if (header->u.bmh_magic != DM2_BITMAP_MAGIC) {
	fprintf(stderr, "ERROR: Bitmap header not correct.  Expected MAGIC "
		"0x%x, got MAGIC=0x%x\n",
		DM2_BITMAP_MAGIC, header->u.bmh_magic);
	return 1;
    }

    /* Assign new version */
    header->u.bmh_version = new_version;

    nbytes = lseek(fd, SEEK_SET, 0);
    if (nbytes != 0) {
	fprintf(stderr, "ERROR: lseek failed: %d\n", errno);
	return 1;
    }

    /* Write new version with old header */
    nbytes = write(fd, header, sizeof(*header));
    if (nbytes == -1) {
	fprintf(stderr, "ERROR: Header not written correctly: %d\n", errno);
	return 1;
    }
    if (nbytes != sizeof(*header)) {
	fprintf(stderr, "ERROR: Header not written correctly.  Expected "
		"%d bytes, wrote %d bytes\n", (int)sizeof(*header), nbytes);
	return 1;
    }
    close(fd);
    return 0;
}	/* write_version */


/*
 * Program: cache_version
 *
 * Arguments:
 *	arg1: mount point
 */
int
main(int  argc,
     char *argv[])
{
    char *progname = argv[0];
    int new_version, ret, ch;
    int write_vers = 0;

    if (argc < 2)
	cache_vers_usage(progname);

    while ((ch = getopt(argc, argv, "w:")) != -1) {
	switch (ch) {
	    case 'w':
		new_version = atoi(optarg);
		if (new_version < 2)
		    cache_vers_usage(progname);
		write_vers++;
		break;
	    default:
		cache_vers_usage(progname);
		break;
	}
    }

    if (write_vers) {
	ret = write_version(argv, new_version);
    } else {
	ret = read_version(argv);
    }
    return ret;
}
