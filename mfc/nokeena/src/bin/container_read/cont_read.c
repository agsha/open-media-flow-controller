/*
 *	Author: Michael Nishimoto (miken@nokeena.com)
 *
 *	Copyright (c) Nokeena Networks Inc., 2008
 */

#include <errno.h>
#include <stdio.h>
#include <ftw.h>
#include <unistd.h>
#include <syslog.h>
#include <glib.h>
#include <ctype.h>
#include <openssl/md5.h>
#include "nkn_diskmgr_api.h"
#include "nkn_diskmgr2_disk_api.h"
#include "nkn_http.h"

/* Macros */
#define	NAP_FREQUENCY	500		// Pause every 500 entries
#define	NAP_TIME	(250 * 1000)	// 250 millisec

typedef struct {
    char *basename;
    nkn_attr_t attr;
} attr_node_t;

typedef struct {
    char *uri;
    off_t offset;
    off_t length;
} uol_t;

typedef struct nkn_uol_wrapper {
    uol_t		uol;
    int			deleted;
} nkn_uol_wrap_t;

int extnumber = 1;
int checksum_output;
int print_attrs;
int print_attr_version;
int print_compact_output;
int print_content_len;
int print_dirname;
int print_for_cliweb;	// Used for CLI and Web listing of objects
int print_fullpath;
int print_header;
int print_one_entry_per;
int print_temp;
int recursive;
int skip_deleted;
int syslog_errors;
int ram_drive;	    // ram disk does not support O_DIRECT
int lowest_tier = 0;
int print_md5_chksum = 0; // md5 checksum flag
int md5_nbytes;     // calculate md5 hash for md5_nbytes value
int g_rawpart_fd = 0;       // disk fd for a given contname

GHashTable *attr_hash;

/* PROTOTYPES */
static void cont_usage(const char *progname);
static void default_output(int fd, char *contname, unsigned num_exts);
static void compact_output(int fd, char *contname, unsigned num_exts);
static void combine_uri_segments(int fd, char *contname, unsigned num_exts);
static void getrawpartfd(char *contname);
static void compact_output_print_temp(attr_node_t *an);
static void compact_output_print_content_length(attr_node_t *an);
int printmd5chksum(off_t offset, off_t length, off_t dext_start_sector);

static void
cont_usage(const char *progname)
{
    fprintf(stderr, "Usage: %s [-1cCdhL] [-5 <no_of_bytes>] [-R] <mount point> |"
		    " <container name>\n", progname);
    fprintf(stderr, "  1: Print one line per URI\n");
    fprintf(stderr, "  5 <no_of_bytes>: Print md5 checksum for <no_of_bytes>"
		    " in first extent.\n");
    fprintf(stderr, "  <no_of_bytes> should be between 0 and 2MB and"
		    " should be \n");
    fprintf(stderr, "  multiple of sector size.\n");
    fprintf(stderr, "  a: Print attributes (assumes compact output)\n");
    fprintf(stderr, "  c: Compact form output w/ extent numbers\n");
    fprintf(stderr, "  C: Compact form output w/o extent numbers\n");
    fprintf(stderr, "  d: Print directory names in compact form\n");
    fprintf(stderr, "  E: Print all errors to syslog\n");
    fprintf(stderr, "  h: Print header row in compact form\n");
    fprintf(stderr, "  L: Also print content length with extent info\n");
    fprintf(stderr, "  m: Print object list for mgmt interface (CLI/Web)\n");
    fprintf(stderr, "  M: The meta fs is a RAM disk\n");
    fprintf(stderr, "  p: Print full pathnames; negates 'd'\n");
    fprintf(stderr, "  R: Process container files under the specified directory"
		    " and its sub-directory\n");
    fprintf(stderr, "  s: Skip deleted objects\n");
    fprintf(stderr, "  t: Print last update time and temperature\n");
    fprintf(stderr, "  v: Print version with attributes\n");
    exit(1);
}	/* cont_usage */


static int
cr_check_attr_head(DM2_disk_attr_desc_t *dattrd,
		   const char		*attrpath,
		   const char		*pbuf,
		   const char		*rbuf)
{
    int		skip = 0;
    nkn_attr_t	*dattr = (nkn_attr_t *)dattrd;

    if (dattrd->dat_magic == DM2_ATTR_MAGIC_GONE) {
	/*
	 * Do nothing; Can skip this sector.
	 *
	 * It's possible to store these sector offsets in a list and
	 * reuse the sectors.  However, this is more work than I want
	 * to do at this point.
	 *
	 * Can no longer just increment to next sector when we see GONE magic
	 * because we could see GONE magic without having a valid attribute
	 * now that we can write empty attributes to reduce fragmentation.
	 */
	skip = 8;
    } else if (dattrd->dat_magic != DM2_ATTR_MAGIC) {
	fprintf(stderr, "Illegal magic number in attribute file (%s): "
		 "name=%s expected=0x%x|0x%x read=0x%x offset=%ld\n",
		 attrpath, dattrd->dat_basename, DM2_ATTR_MAGIC,
		 DM2_ATTR_MAGIC_GONE, dattrd->dat_magic, pbuf-rbuf);
	skip = 1;
	if (dattr->na_magic == NKN_ATTR_MAGIC) {
	    fprintf(stderr, "Expected disk descriptor in attr file (%s) looks "
		    "like disk attr at offset=%ld\n", attrpath, pbuf-rbuf);
	}
    }
    if (skip == 0 && dattrd->dat_version != DM2_ATTR_VERSION) {
	fprintf(stderr, "Illegal version in attribute file (%s): "
		 "name=%s expected=%d read=%d offset=%ld\n",
		 attrpath, dattrd->dat_basename, DM2_ATTR_VERSION,
		 dattrd->dat_version, pbuf-rbuf);
	skip = 1;
    }
    return skip;
}	/* cr_check_attr_head */


static unsigned long
cr_do_csum64_iterate_aligned(const unsigned char *buff,
			  int		      len,
			  unsigned long	      old_checksum)
{
    unsigned long result = old_checksum;

    assert(len % 8 == 0);
    assert(((uint64_t)buff & 0x7) == 0);

    for (; len > 0; len -= 8, buff += 8) {
	result ^= *(uint64_t *)buff;
    }
    return result;
}	/* cr_do_csum_iterate_aligned */


static int
cr_check_attr(nkn_attr_t	*dattr,
	      const int		secnum,
	      const char	*attrpath)
{
    uint64_t disk_checksum, calc_checksum;

    if (dattr->na_magic != NKN_ATTR_MAGIC) {
	fprintf(stderr, "Bad magic number (0x%x) at sector (%d) "
		 "(file=%s)\n", dattr->na_magic, secnum, attrpath);
	return 1;
    }
    disk_checksum = dattr->na_checksum;
    dattr->na_checksum = 0;
    calc_checksum = cr_do_csum64_iterate_aligned((uint8_t *)dattr,
						 dattr->total_attrsize, 0);
    if (disk_checksum != 0 && calc_checksum != disk_checksum) {
	fprintf(stderr, "Checksum mismatch: expected=0x%lx got=0x%lx at "
		 "sector=%d file=%s\n",
		disk_checksum, calc_checksum, secnum, attrpath);
	return 1;
    }
    return 0;
}	/* cr_check_attr */


static int32_t
cr_am_decode_hotness(nkn_hot_t *in_hotness)
{
    int hotness;

    /* Absolute value is the first 31 bits */
    hotness = (int)(*in_hotness & 0xffff);
    return (hotness);
}


static uint32_t
cr_am_decode_update_time(nkn_hot_t *in_hotness)
{
    return ((*in_hotness >> 32) & 0xffffffff);
}


static char *
get_time_single_token(time_t t)
{
    char *ptime, *space;

    ptime = ctime(&t);
    ptime[strlen(ptime)-1] = '\0';
    ptime += 4;	// skip day of week
    while ((space = strchr(ptime, ' ')) != NULL)
	*space = '_';

    return ptime;
}


static void
cr_fix_unprintables(char *basename)
{
    int len;

    len = strlen(basename);
    if (len > NAME_MAX)
	basename[NAME_MAX] = '\0';
    while (*basename != '\0') {
	if (!isprint(*basename))
	    *basename = 'X';
	basename++;
    }
}

static void
print_all_attrs(const int secnum,
		nkn_attr_t *dattr,
		char *attrpath,
		char *basename,
		int  deleted)
{
    char *uri, *slash, *ptime, *space;
    uint32_t update_time;
    int32_t temp;
    static int header_printed = 0, len;

    if (print_fullpath) {
	cr_fix_unprintables(basename);
	len = strlen(attrpath) + strlen(basename);
	uri = alloca(len);
	strcpy(uri, attrpath);
	slash = strrchr(uri, '/');
	*(slash+1) = '\0';
	strcat(uri, basename);
    }

    if (print_header && header_printed == 0) {
	printf("offset cont_len expiry time0 pin hot-time hot-temp "
	       "total_size num_entries attrsize flags PATH\n");
	header_printed = 1;
    }
    if (dattr->na_magic != NKN_ATTR_MAGIC) {
	if (syslog_errors)
	    syslog(LOG_NOTICE, "container_read: Internal error (code=1) "
		   "0x%x,%s", dattr->na_magic, print_fullpath ? uri : basename);
	else {
	    fprintf(stderr, "ERROR: Bad attr magic=0x%x URI=%s",
		    dattr->na_magic, print_fullpath ? uri : basename);
	    return;
	}
    }
    printf("%d %ld ", secnum, dattr->content_length);
    if (print_attr_version)
	printf("0x%lx/0x%lx ", dattr->obj_version.objv_l[0],
	       dattr->obj_version.objv_l[1]);

    if (dattr->cache_expiry) {
	ptime = get_time_single_token(dattr->cache_expiry);
	printf("%lu/%s ", dattr->cache_expiry, ptime);
    } else {
	printf("0 ");
    }
    if (dattr->cache_time0) {
	ptime = get_time_single_token(dattr->cache_time0);
	printf("%lu/%s ", dattr->cache_time0, ptime);
    } else {
	printf("0 ");
    }

    if ((dattr->na_flags & NKN_OBJ_CACHE_PIN) != 0) {
	printf("PIN ");
    } else {
	printf("0 ");
    }

    update_time = cr_am_decode_update_time(&dattr->hotval);
    if (update_time == 0)
	printf("0 ");
    else {
	ptime = get_time_single_token(update_time);
	printf("%d/%s ", update_time, ptime);
    }

    temp = cr_am_decode_hotness(&dattr->hotval);
    printf("%d ", temp);

    printf("%d ", dattr->total_attrsize);
    printf("%d ", dattr->na_entries);
    printf("%d ", dattr->blob_attrsize);
    printf("0x%x ", dattr->na_flags);
    printf("%s", print_fullpath ? uri : basename);
    if (deleted)
	printf(" DELETED");
    printf("\n");
}	/* print_all_attrs */


/*
 * This function will output in the following format to the CLI and Web
 *
 *  Loc        Size (KB)          Expiry                             URL
 *-----------------------------------------------------------------------------
 */
static void
print_obj_info_to_cliweb(const int  secnum,
			 nkn_attr_t *dattr,
			 char       *attrpath,
			 char       *basename)
{
#define MAX_SHORT_DN 10
    char *uri, *slash, *ptime, *space;
    uint32_t update_time;
    int32_t temp;
    static int len;
    char sz_expiry_time[26];
    char *cp_temp, sz_diskname[MAX_SHORT_DN+1];
    char *cp_uri, *cp_temp1;
    char *uri_spl;

    /* Get full path of URL */
    cr_fix_unprintables(basename);
    len = strlen(attrpath) + strlen(basename);
    uri = alloca(len);
    uri_spl = alloca(len);
    strcpy(uri, attrpath);
    slash = strrchr(uri, '/');
    *(slash+1) = '\0';
    strcat(uri, basename);

    /* Get the diskname */
    cp_temp = strstr(uri, NKN_DM_DISK_CACHE_PREFIX);
    if (cp_temp) {
	cp_temp1 = strchr(cp_temp, '/'); /* Find / in dc_??/<namespace> */
	if (cp_temp1) {
	    len = cp_temp1 - cp_temp;	// Gives the length of disk name
	    if (len > MAX_SHORT_DN)
		len = MAX_SHORT_DN; // since sz_diskname is of size 11
	    strncpy(sz_diskname, cp_temp, len);
	    sz_diskname[len] = '\0';
	} else {
	    strcpy(sz_diskname, "DISK");
	}
    } else
	strcpy(sz_diskname, "DISK");

    /* Now get the domain/uri part alone */
    cp_temp = strchr(uri, ':');
    if (cp_temp) {
	cp_temp = strchr(cp_temp, '_') ;
	if (cp_temp) cp_temp++; // skip the _
    }
    if (cp_temp) {
	 unsigned int i=0, j=0;

	 for (i=0; i<strlen(cp_temp); i++) {
		 /*
		  * BUG 5705
		  * Check for the escape seq in an object, if present escape
		  * it again so that the printf command doesnt apply the
		  * escape sequence rule on the escape seq.
		  * This part of code is mainly for objects like tmp\file.
		  */
		// if((cp_temp[i] == '\\')) {
		//	 uri_spl[j] = cp_temp[i];
		//	 j++;
		//	 uri_spl[j] = cp_temp[i];
		//	 j++;
		// }
		 uri_spl[j] = cp_temp[i];
		 j++;

	 }
	uri_spl[j] = '\0';
	cp_uri = uri_spl;
    } else
        cp_uri = uri;	/* Should never happen */

    if (dattr->na_magic != NKN_ATTR_MAGIC) {
	if (syslog_errors)
	    syslog(LOG_NOTICE, "container_read: Internal error (code=1) "
		   "0x%x,%s", dattr->na_magic, print_fullpath ? uri : basename);
	else {
	    fprintf(stderr, "ERROR: Bad attr magic=0x%x URI=%s",
		    dattr->na_magic, print_fullpath ? uri : basename);
	    return;
	}
    }
//    printf(" %5s %4s    %10ld", sz_diskname, (!dattr->na_flags) ? "Y":"N", (dattr->content_length / 1024));
    /*Commenting out the cache pinning changes are it is not part of R2.1*/
//    printf(" %5s  %10ld", sz_diskname, (dattr->content_length / 1024));
    /*Enabling cache pinning as part of bug 8344*/
    if (lowest_tier) {
	printf(" %5s %4s    %10ld", sz_diskname,
	       (dattr->na_flags & NKN_OBJ_CACHE_PIN) ? "Y":"N",
	       (dattr->content_length / 1024));
    } else {
	printf(" %5s %4s    %10ld", sz_diskname,
	       "N", (dattr->content_length / 1024));
    }



    /* Print Compressed status */
    if ((dattr->na_flags & NKN_OBJ_COMPRESSED) && 
		(dattr->encoding_type == HRF_ENCODING_GZIP))
       printf("    %4c  ", 'G');
    else if ((dattr->na_flags & NKN_OBJ_COMPRESSED) &&
		(dattr->encoding_type == HRF_ENCODING_DEFLATE))
       printf("    %4c  ", 'D');
    else
	printf("    %4c  ", '-');

    /* Get the Expiry time */
    strncpy(sz_expiry_time, ctime(&dattr->cache_expiry), 24);
    sz_expiry_time [24] = '\0';
    printf("  %24s", sz_expiry_time);


    printf("  %s", cp_uri);

    printf("\n");
}	/* print_obj_info_to_cliweb */


static int
read_attributes(const char *contname)
{
#define RD_ATTR_BUFSZ	(1024*1024)	// must be multiple of 1KiB
    DM2_disk_attr_desc_t *dattrd;
    attr_node_t *uri_attr;
    nkn_attr_t *dattr;
    char *attrpath, *slash, *rbuf, *pbuf;
    int afd, rdsz, ret, skip, flags = 0;
    off_t secnum;

    attrpath = alloca(strlen(contname)+strlen(NKN_CONTAINER_ATTR_NAME));
    strcpy(attrpath, contname);
    slash = strrchr(attrpath, '/');
    *(slash+1) = '\0';
    strcat(attrpath, NKN_CONTAINER_ATTR_NAME);

    if (ram_drive)
	flags = O_RDONLY;
    else
	flags =  O_RDONLY | O_DIRECT;

    if ((afd = open(attrpath, flags)) == -1) {
	/*
	 * Commenting the log to stderr as this error message is meaningless
	 * for the user.
	 * Failing here does not mean that there is any functional issue, it is
	 * normal to have a directory without attributes-2 file. Done for
	 * bug 10536
	 */
	// fprintf(stderr, "Can not open: %s\n", attrpath);
	/*
	 * We could be failing because the file doesn't exist due to a
	 * ENOSPC condition.
	 */
	return 1;
    }

    if (posix_memalign((void *)&rbuf, DEV_BSIZE, RD_ATTR_BUFSZ)) {
	fprintf(stderr, "posix_memalign failed: %d %d\n", RD_ATTR_BUFSZ,errno);
	exit(1);
    }

    secnum = 0;
    while ((rdsz = pread(afd, rbuf, RD_ATTR_BUFSZ, secnum*DEV_BSIZE)) != 0) {
	if (rdsz == -1) {
	    fprintf(stderr, "Read of attribute file (%s) failed: %d\n",
		    attrpath, errno);
	    free(rbuf);
	    close(afd);
	    return 1;
	}
	if (rdsz % DEV_BSIZE != 0) {
	    fprintf(stderr, "Read from attribute file (%s) is "
		    "not an integral sector amount: %d\n", attrpath, rdsz);
	    free(rbuf);
	    close(afd);
	    return 1;
	}
	if (rdsz < DM2_ATTR_TOT_DISK_SIZE) {
	    fprintf(stderr, "Short read from attribute file (%s): %d",
		    attrpath, rdsz);
	    /* We are finished reading this file */
	    goto free_buf;
	}
	/*
	 * If we don't have enough data for an entire attr, we don't go back
	 * and perform another read w/o incrementing any counters
	 */
	for (pbuf = rbuf, dattrd = (DM2_disk_attr_desc_t *)rbuf;
	     rdsz >= DM2_ATTR_TOT_DISK_SIZE;
	     dattrd = (DM2_disk_attr_desc_t *)pbuf) {
	    skip = cr_check_attr_head(dattrd, attrpath, pbuf, rbuf);
	    if (skip == 1) {		// Unknown data
		goto next_attr;
	    } else if (skip == 8) {	// GONE magic
		goto next_attr;
	    }
	    dattr = (nkn_attr_t *)(pbuf + DEV_BSIZE);
	    if (cr_check_attr(dattr, secnum, attrpath)) {
		/* Problem with nkn_attr_t object */
		goto next_attr;
	    }
	    if (print_for_cliweb) {
	        /* Only if print_attrs is set deleted are printed.
		 * Hence, no check is done here for deleted
		 * This will need to change if that logic changes */
	        print_obj_info_to_cliweb(secnum, dattr,
					attrpath, dattrd->dat_basename);
	    } else if (print_attrs) {
		print_all_attrs(secnum, dattr, attrpath, dattrd->dat_basename,
				dattrd->dat_magic == DM2_ATTR_MAGIC_GONE);
	    } else {
		uri_attr = calloc(1, sizeof(attr_node_t));
		uri_attr->basename = strdup(dattrd->dat_basename);
		memcpy(&uri_attr->attr, dattr, sizeof(nkn_attr_t));
		g_hash_table_insert(attr_hash, uri_attr->basename, uri_attr);
	    }
	next_attr:
	    secnum += DM2_ATTR_TOT_DISK_SECS;
	    pbuf += DM2_ATTR_TOT_DISK_SIZE;
	    rdsz -= DM2_ATTR_TOT_DISK_SIZE;
	}
    }
 free_buf:
    close(afd);
    free(rbuf);
    if (print_attrs || print_for_cliweb)
	return 1;
    else
	return 0;
}	/* read_attributes */


static void
default_output(int fd,
	       char *contname,
	       unsigned num_exts)
{
    char		*cont_head = NULL, *sector = NULL;
    dm2_container_t	*c;
    DM2_disk_extent_t	*dext;
    DM2_disk_extent_v2_t *dext_v2 = NULL;
    int			nbytes;
    unsigned		i;

    if (posix_memalign((void *)&cont_head, DEV_BSIZE,
		       NKN_CONTAINER_HEADER_SZ)) {
	printf("posix_memalign failed: %d %d\n", DEV_BSIZE,
	       NKN_CONTAINER_HEADER_SZ);
	exit(1);
    }
    c = (dm2_container_t *)cont_head;
    if (posix_memalign((void *)&sector, DEV_BSIZE, DEV_BSIZE)) {
	printf("posix_memalign failed: %d %d\n", DEV_BSIZE, DEV_BSIZE);
	exit(1);
    }
    dext = (DM2_disk_extent_t *)sector;
    dext_v2 = (DM2_disk_extent_v2_t *)sector;
    nbytes = read(fd, cont_head, NKN_CONTAINER_HEADER_SZ);
    if (nbytes != NKN_CONTAINER_HEADER_SZ) {
	printf("ERROR: read header problem: %d %d\n",
	       nbytes, nbytes == -1 ? errno : 0);
	goto free_mem;
    }
    printf("Container name: %s\n", contname);
    printf("  checksum: 0x%lx\n", c->c_checksum);
    printf("  magicnum: 0x%x\n", c->c_magicnum);
    printf("  version: %d\n", c->c_version);
    printf("  size: %d\n", c->c_cont_sz);
    printf("  num_exts: %d\n", num_exts);
    printf("  obj_sz: %d\n", c->c_obj_sz);

    printf("\n");
    for (i = 0; i < num_exts; i++) {
	/* Pause to give room for other IO activities */
	if (0 == (i % NAP_FREQUENCY))
		usleep(NAP_TIME);

	nbytes = read(fd, sector, 512);
	if (nbytes != 512) {
	    printf("ERROR: read extent problem: %d %d %d\n", i, nbytes,
		   nbytes == -1 ? errno : 0);
	    goto free_mem;
	}

	cr_fix_unprintables(dext->dext_basename);
	if (dext->dext_header.dext_magic != DM2_DISK_EXT_DONE &&
	    dext->dext_header.dext_magic != DM2_DISK_EXT_MAGIC) {
	    printf("Bad MAGIC: URI=%s offset=%ld length=%ld "
		   "(expected=0x%x|0x%x/got=0x%x)\n", dext->dext_basename,
		   dext->dext_offset, dext->dext_length,
		   DM2_DISK_EXT_MAGIC, DM2_DISK_EXT_DONE,
		   dext->dext_header.dext_magic);
	    continue;
	}
	if (dext->dext_header.dext_version != DM2_DISK_EXT_VERSION &&
	    dext->dext_header.dext_version != DM2_DISK_EXT_VERSION_V2) {
	    printf("Bad Version: URI=%s offset=%ld length=%ld "
		   "(expected=%d,%d/got=%d)\n", dext->dext_basename,
		   dext->dext_offset, dext->dext_length,
		   DM2_DISK_EXT_VERSION, DM2_DISK_EXT_VERSION_V2,
		   dext->dext_header.dext_version);
	    continue;
	}

	printf("Extent: %d\n", i+1);
	if (dext->dext_header.dext_version == DM2_DISK_EXT_VERSION) {
	    printf("  my checksum: 0x%lx\n",dext->dext_header.dext_my_checksum);
	    printf("  ext checksum: 0x%lx\n", dext->dext_ext_checksum);
	    printf("  magic: 0x%x\n", dext->dext_header.dext_magic);
	    printf("  version: %d\n", dext->dext_header.dext_version);
	    printf("  physid: 0x%lx\n", dext->dext_physid);
	    printf("  uol.len: %lu\n", dext->dext_length);
	    printf("  uol.off: %lu\n", dext->dext_offset);
	    printf("  start sec: %zd 0x%zx\n",
		   dext->dext_start_sector, dext->dext_start_sector);
	    printf("  start attr: %zx\n", dext->dext_start_attr);
	    printf("  basename: %s\n", dext->dext_basename);
	} else {
	    printf("  my checksum: 0x%lx\n",
		   dext_v2->dext_header.dext_my_checksum);
//	    printf("  ext checksum: 0x%lx\n", dext_v2->dext_ext_checksum);
	    printf("  magic: 0x%x\n", dext_v2->dext_header.dext_magic);
	    printf("  version: %d\n", dext_v2->dext_header.dext_version);
	    printf("  physid: 0x%lx\n", dext_v2->dext_physid);
	    printf("  uol.len: %lu\n", dext_v2->dext_length);
	    printf("  uol.off: %lu\n", dext_v2->dext_offset);
	    printf("  start sec: %zd 0x%zx\n",
		   dext_v2->dext_start_sector, dext_v2->dext_start_sector);
	    printf("  start attr: %zx\n", dext_v2->dext_start_attr);
	    printf("  basename: %s\n", dext_v2->dext_basename);

	}


    }

 free_mem:
    if (cont_head)
	free(cont_head);
    if (sector)
	free(sector);
}	/* default_output */


static void
compact_output_print_temp(attr_node_t *an)
{
    time_t	update_time;
    char	*ptime;
    uint32_t	temp;

    if (print_temp) {
	if (an) {
	    update_time = cr_am_decode_update_time(&an->attr.hotval);
	    temp = cr_am_decode_hotness(&an->attr.hotval);
	    ptime = get_time_single_token(update_time);
	    printf("%s %d ", ptime, temp);
	} else {
	    printf("X X ");
	}
    }
}   /* compact_output_print_temp */


static void
compact_output_print_content_length(attr_node_t *an)
{
    if (print_content_len) {
	if (an) {
	    printf("%ld ", an->attr.content_length);
	} else
	    printf("X ");
    }
}   /* compact_output_print_content_length */


/*
 * Method that calculates md5 hash value and prints it.
 */
int
printmd5chksum(off_t dext_offset,
	       off_t dext_length,
	       off_t dext_start_sector)
{
    char	    *rbuf;
    int		    rdsz;
    unsigned char   *md5_hash;
    int		    loop_i;
    unsigned	    md5_len;

    if (dext_offset != 0 || g_rawpart_fd < 0) {
	 printf("N/A ");
    } else {
	if (posix_memalign((void *)&rbuf, DEV_BSIZE, RD_ATTR_BUFSZ)) {
	    printf("posix_memalign failed: %d %d\n", RD_ATTR_BUFSZ, errno);
	    return (-1);
	}
	rdsz = pread(g_rawpart_fd, rbuf, md5_nbytes, dext_start_sector*512);
	if (rdsz == -1) {
	    printf("Read of disk failed: %d\n", errno);
	    free(rbuf);
	    return (-1);
	}
	md5_len = (dext_length < md5_nbytes) ?  dext_length : md5_nbytes;
	md5_hash = MD5((const unsigned char*)rbuf, md5_len, NULL);
	free(rbuf);
	printf("0x");
	for (loop_i = 0; loop_i <MD5_DIGEST_LENGTH; loop_i++) {
	    printf("%02x", md5_hash[loop_i]);
	}
	printf(" ");
    }
    return (0);
}   /* printmd5chksum */


static void
compact_output(int fd,
	       char *contname,
	       unsigned num_exts)
{
    char		*cont_head = NULL, *sector = NULL;
    char		*slash, *space;
    dm2_container_t	*c;
    DM2_disk_extent_t	*dext;
    DM2_disk_extent_v2_t *dext_v2 = NULL;
    attr_node_t		*an = NULL;
    int			nbytes;
    unsigned		i;
    int			retval;

    if (posix_memalign((void *)&cont_head, DEV_BSIZE,
		       NKN_CONTAINER_HEADER_SZ)) {
	printf("posix_memalign failed: %d %d\n",
	       DEV_BSIZE, NKN_CONTAINER_HEADER_SZ);
	exit(1);
    }
    if (posix_memalign((void *)&sector, DEV_BSIZE, DEV_BSIZE)) {
	printf("posix_memalign failed: %d %d\n", DEV_BSIZE, DEV_BSIZE);
	exit(1);
    }
    c = (dm2_container_t *)cont_head;
    dext = (DM2_disk_extent_t *)sector;
    dext_v2 = (DM2_disk_extent_v2_t *)sector;

    nbytes = read(fd, cont_head, NKN_CONTAINER_HEADER_SZ);
    if (nbytes != NKN_CONTAINER_HEADER_SZ) {
	printf("ERROR: read header problem: %d %d\n",
	       nbytes, nbytes == -1 ? errno : 0);
	goto free_mem;
    }
    slash = strrchr(contname, '/');
    if (print_dirname) {
	*slash = '\0';
	printf("%s\n", contname);
    } else {
	*(slash+1) = '\0';
    }
    if (print_header) {
	if (extnumber && !checksum_output)
	    printf("ext# ");
	printf("off len ");
	if (print_content_len)
	    printf("cont_len ");
	printf("physid ");
	if (print_temp)
	    printf("temp-time temp-hot ");
	printf("sec-off sec-off ");
	if (checksum_output) {
	    if (dext->dext_header.dext_version == DM2_DISK_EXT_VERSION) {
		printf("chksum ");
	    } else {
		for (i = 0; i < 8; i++)
		    printf("chksum_%d ", i+1);
	    }
	}
	if (print_md5_chksum)
	    printf("md5_chksum ");
	printf("name\n");
    }

    for (i = 0; i < num_exts; i++) {
	/* Pause to give room for other IO activities */
	if (0 == (i % NAP_FREQUENCY))
		usleep(NAP_TIME);

	nbytes = read(fd, sector, 512);
	if (nbytes != 512) {
	    printf("ERROR: read extent problem: %d %d %d\n", i, nbytes,
		   nbytes == -1 ? errno : 0);
	    goto free_mem;
	}

	cr_fix_unprintables(dext->dext_basename);
	if (dext->dext_header.dext_version != DM2_DISK_EXT_VERSION &&
	    dext->dext_header.dext_version != DM2_DISK_EXT_VERSION_V2) {
	    printf("Bad Version: URI=%s offset=%ld length=%ld "
		   "(expected=%d,%d/got=%d)\n", dext->dext_basename,
		   dext->dext_offset, dext->dext_length,
		   DM2_DISK_EXT_VERSION, DM2_DISK_EXT_VERSION_V2,
		   dext->dext_header.dext_version);
	    continue;
	}

	if (print_temp || print_content_len) {
	    an = g_hash_table_lookup(attr_hash, dext->dext_basename);
	    if (an == NULL &&
		dext->dext_header.dext_magic != DM2_DISK_EXT_DONE) {
		printf("Attribute not found for %s (%s)\n",
			dext->dext_basename, contname);
	    }
	}

	if (skip_deleted &&
		    (dext->dext_header.dext_magic == DM2_DISK_EXT_DONE))
	    continue;

	if (extnumber && !checksum_output)
	    printf("%d ", i+1);

	if (dext->dext_header.dext_version == DM2_DISK_EXT_VERSION) {
	    printf("%lu ", dext->dext_offset);
	    printf("%7lu ", dext->dext_length);
	    compact_output_print_content_length(an);
	    printf("0x%lx ", dext->dext_physid);
	    compact_output_print_temp(an);
	    printf("%zd 0x%zx ",
		   dext->dext_start_sector, dext->dext_start_sector);
	    if (checksum_output)
		printf("0x%lx ", dext->dext_ext_checksum);
	    if (print_md5_chksum) {
		retval = printmd5chksum(dext->dext_offset, dext->dext_length,
					dext->dext_start_sector);
		if (retval != 0)
		    goto free_mem;
	    }
	    if (print_fullpath) {
		strcat(contname, dext->dext_basename);
		printf("%s", contname);
	    } else
		printf("%s", dext->dext_basename);
	    if (dext->dext_header.dext_magic == DM2_DISK_EXT_DONE)
		printf(" DELETED");
	    else if (dext->dext_header.dext_magic != DM2_DISK_EXT_MAGIC)
		printf(" BAD_MAGIC: 0x%x", dext->dext_header.dext_magic);
	} else {
	    printf("%lu ", dext_v2->dext_offset);
	    printf("%7lu ", dext_v2->dext_length);
	    compact_output_print_content_length(an);
	    printf("0x%lx ", dext_v2->dext_physid);
	    compact_output_print_temp(an);
	    printf("%zd 0x%zx ",
		   dext_v2->dext_start_sector, dext_v2->dext_start_sector);
	    if (checksum_output) {
		for (i = 0; i < 8; i++)
		    printf("0x%x ", dext_v2->dext_ext_checksum[i]);
	    }
	    if (print_md5_chksum) {
		retval = printmd5chksum(dext_v2->dext_offset,
					dext_v2->dext_length,
					dext_v2->dext_start_sector);
		if (retval != 0)
		    goto free_mem;
	    }
	    if (print_fullpath) {
		strcat(contname, dext_v2->dext_basename);
		printf("%s", contname);
	    } else
		printf("%s", dext_v2->dext_basename);
	    if (dext_v2->dext_header.dext_magic == DM2_DISK_EXT_DONE)
		printf(" DELETED");
	    else if (dext_v2->dext_header.dext_magic != DM2_DISK_EXT_MAGIC)
		printf(" BAD_MAGIC: 0x%x", dext_v2->dext_header.dext_magic);

	}
	printf("\n");
	*(slash+1) = '\0';
    }
 free_mem:
    if (sector)
	free(sector);
    if (cont_head)
	free(cont_head);
}	/* compact_output */


static int
cmp_smallest_offset_first(gconstpointer in_a,
			  gconstpointer in_b)
{
    uol_t *a = (uol_t *)in_a;
    uol_t *b = (uol_t *)in_b;

    if (a->offset > b->offset)
	return 1;
    else
	return 0;
}	/* cmp_smallest_offset_first */

static void
uol_print(const gpointer key,
	  const gpointer value,
	  const gpointer user_data)
{
    GList*uol_list = (GList *)value;
    GList *uol_obj;
    nkn_uol_wrap_t *uolw;
    char *contname = (char *)user_data;
    char *slash;
    attr_node_t	*an = NULL;
    off_t len;

    uol_list = g_list_sort(uol_list, cmp_smallest_offset_first);
    slash = &contname[strlen(contname)];
    for (uol_obj = uol_list; uol_obj; ) {
	uolw = (nkn_uol_wrap_t *)uol_obj->data;
	//	printf("%s	%12ld", (char *)key, uol->offset);
	printf("%12ld", uolw->uol.offset);
	len = uolw->uol.offset;
	for (; uol_obj; uol_obj = uol_obj->next) {
	    uolw = (nkn_uol_wrap_t *)uol_obj->data;
	    if (len == uolw->uol.offset) {
		len = uolw->uol.offset + uolw->uol.length;
	    } else {
		break;
	    }
	}
	/* Always print length found */
	printf("%12ld", len);

	if (print_content_len) {
	    an = g_hash_table_lookup(attr_hash, key);
	    if (an == NULL)
		fprintf(stderr, "Attribute not found for %s (%s)\n",
			(char *)key, contname);
	    else {
		printf("%12ld", an->attr.content_length);
	    }
	}

	if (print_fullpath) {
	    strcat(contname, key);
	    printf(" %s", contname);
	} else
	    printf(" %s", (char *)key);

	if (uolw->deleted)
	    printf(" DELETED");

	printf("\n");
	*slash = '\0';
    }

    /* Free all the memory and cleanup the list */
    for (uol_obj = uol_list; uol_obj; uol_obj = uol_list) {
	uolw = (nkn_uol_wrap_t *)uol_obj->data;
	free(uolw->uol.uri);
	free(uolw);
	uol_list = g_list_delete_link(uol_list, uol_obj);
    }
}

static void
combine_uri_segments(int fd,
		     char *contname,
		     unsigned num_exts)
{
    GHashTable		*uol_hash;
    char		*cont_head = NULL, *sector = NULL;
    char		*slash;
    dm2_container_t	*c;
    DM2_disk_extent_t	*dext;
    DM2_disk_extent_v2_t	*dext_v2;
    GList		*uol_list;
    nkn_uol_wrap_t	*uolw;
    int			nbytes;
    unsigned		i, deleted = 0;

    if (print_content_len) {
	attr_hash = g_hash_table_new(g_str_hash, g_str_equal);
	if (read_attributes(contname))
	    goto free_mem;
    }

    uol_hash = g_hash_table_new(g_str_hash, g_str_equal);

    if (posix_memalign((void *)&cont_head, DEV_BSIZE,
		       NKN_CONTAINER_HEADER_SZ)) {
	printf("posix_memalign failed: %d %d\n", DEV_BSIZE,
	       NKN_CONTAINER_HEADER_SZ);
	exit(1);
    }
    if (posix_memalign((void *)&sector, DEV_BSIZE, DEV_BSIZE)) {
	printf("posix_memalign failed: %d %d\n", DEV_BSIZE, DEV_BSIZE);
	exit(1);
    }
    c = (dm2_container_t *)cont_head;
    dext = (DM2_disk_extent_t *)sector;
    dext_v2 = (DM2_disk_extent_v2_t *)sector;

    nbytes = read(fd, cont_head, NKN_CONTAINER_HEADER_SZ);
    if (nbytes != NKN_CONTAINER_HEADER_SZ) {
	printf("ERROR: read header problem: %d %d\n",
	       nbytes, nbytes == -1 ? errno : 0);
	goto free_mem;
    }
    slash = strrchr(contname, '/');
    if (print_dirname) {
	*slash = '\0';
	printf("%s\n", contname);
    } else
	*(slash+1) = '\0';
    if (print_header) {
	if (print_content_len)
	    printf("%12s%12s%12s %s\n", "offset", "length", "content_len",
		   "URI");
	else
	    printf("%12s%12s %s\n", "offset", "length", "URI");
    }

    for (i = 0; i < num_exts; i++) {
	/* Take a nap to give room for other IO activities */
	if (0 == (i % NAP_FREQUENCY))
		usleep(NAP_TIME);

	nbytes = read(fd, sector, DEV_BSIZE);
	if (nbytes != DEV_BSIZE) {
	    printf("ERROR: read extent problem: %d %d %d\n", i, nbytes,
		   nbytes == -1 ? errno : 0);
	    goto free_mem;
	}
	if (dext->dext_header.dext_magic != DM2_DISK_EXT_DONE &&
	    dext->dext_header.dext_magic != DM2_DISK_EXT_MAGIC) {
	    printf("Bad MAGIC: URI=%s offset=%ld length=%ld "
		   "(expected=0x%x|0x%x/got=0x%x)\n", dext->dext_basename,
		   dext->dext_offset, dext->dext_length,
		   DM2_DISK_EXT_MAGIC, DM2_DISK_EXT_DONE,
		   dext->dext_header.dext_magic);
	    continue;
	}
	if (dext->dext_header.dext_version != DM2_DISK_EXT_VERSION &&
	    dext->dext_header.dext_version != DM2_DISK_EXT_VERSION_V2) {
	    printf("Bad Version: URI=%s offset=%ld length=%ld "
		   "(expected=%d,%d/got=%d)\n", dext->dext_basename,
		   dext->dext_offset, dext->dext_length,
		   DM2_DISK_EXT_VERSION, DM2_DISK_EXT_VERSION_V2,
		   dext->dext_header.dext_version);
	    continue;
	}
	deleted = 0;

	/* Skip deleted entries */
	if (skip_deleted && dext->dext_header.dext_magic == DM2_DISK_EXT_DONE)
	    continue;
	if (dext->dext_header.dext_magic == DM2_DISK_EXT_DONE)
	    deleted = 1;

	uolw = calloc(1, sizeof(nkn_uol_wrap_t));
	if (dext->dext_header.dext_version == DM2_DISK_EXT_VERSION) {
	    uolw->uol.offset = dext->dext_offset;
	    uolw->uol.length = dext->dext_length;
	    uolw->uol.uri = strdup(dext->dext_basename);
	} else {
	    uolw->uol.offset = dext_v2->dext_offset;
	    uolw->uol.length = dext_v2->dext_length;
	    uolw->uol.uri = strdup(dext->dext_basename);
	}
	if (deleted)
	    uolw->deleted = 1;

	uol_list = g_hash_table_lookup(uol_hash, dext->dext_basename);
	uol_list = g_list_prepend(uol_list, uolw);
	g_hash_table_insert(uol_hash, uolw->uol.uri, uol_list);
    }
    g_hash_table_foreach(uol_hash, uol_print, contname);
    g_hash_table_destroy(uol_hash);

 free_mem:
    if (cont_head)
	free(cont_head);
    if (sector)
	free(sector);
}	/* combine_uri_segments */


static void
free_an(const gpointer key,
        const gpointer value,
	const gpointer user_data)
{
    attr_node_t* an = (attr_node_t*)value;

    free(an->basename);
    free(an);
}   /* free_an */


/*
 * For a given container, find the disk from which it is mounted.
 *
 * Ex: A typical contname looks like this:
 * /nkn/mnt/dc_X/<namespace>:<uuid>_<origin|domain>:<port>/a/b/c/d.flv
 * ex: /nkn/mnt/dc_1/verification:2cb04f89_10.1.1.101:80/single/30/file.2
 * We need to extract /nkn/mnt/dc_X from contname, grep it in
 * /proc/mounts to find the mount point, and use that mount point
 * to find the disk from the config(/config/nkn/diskcache-startup.info)
 * file. Once we obtain the disk, open the disk in Direct I/O
 * mode to read data.
 * Notes: The caller of this function needs to close the fd returned.
 * We assign g_rawpart_fd to -1 when we couldn't open the disk.
 */
static void
getrawpartfd(char *contname)
{
#define	DISK_LENGTH	    24
    char    rawpart_id[DISK_LENGTH];
    FILE    *pf;
    char    cmd[512];
    char    disk[DISK_LENGTH];
    char    *back;
    int	    cntslash = 4;
    int	    tmplen = 0;

    while (cntslash > 0 && contname[tmplen] != '\0') {
	if (contname[tmplen] == '/')
	    cntslash--;
	disk[tmplen] = contname[tmplen];
	tmplen++;
    }
    if (cntslash > 1) {
	printf("unable to get disk info for container %s\n", contname);
	g_rawpart_fd = -1;
	return;
    } else {
	if (cntslash == 0)
	    disk[tmplen-1] = '\0';
    }
    sprintf(cmd, "/opt/nkn/generic_cli_bin/getdisk_for_container.sh %s", disk);
    pf = popen(cmd, "r");
    if (!pf) {
	printf("popen error, cmd is %s \n", cmd);
	g_rawpart_fd = -1;
	return;
    }
    if (fgets(rawpart_id, DISK_LENGTH, pf) == NULL) {
	g_rawpart_fd = -1;
	if (!pf)
	    pclose(pf);
	return;
    }
    pclose(pf);
    /*
     * Trim off any new line chars at the end and just get disk info
     */
    back = rawpart_id + strlen(rawpart_id);
    while (isspace(*--back));
    *(back+1) = '\0';
    g_rawpart_fd = open(rawpart_id, O_RDONLY | O_DIRECT);
    if (g_rawpart_fd < 0) {
	printf("disk open error: %d for file %s\n", errno, contname);
	g_rawpart_fd = -1;
	return;
    }
    return;
}   /* getrawpartfd */


static int
read_container(char* contname)
{
    struct stat sb;
    int	 fd, num_exts, ret = 0, flags = 0;

    if (ram_drive)
	flags = O_RDONLY;
    else
	flags =  O_RDONLY | O_DIRECT;

    fd = open(contname, flags);
    if (fd < 0) {
	printf("open error: %d\n", errno);
	exit(1);
    }

    if (print_temp || print_content_len || print_for_cliweb) {
	attr_hash = g_hash_table_new(g_str_hash, g_str_equal);
	if (read_attributes(contname)) {
	    ret = 0;
	    goto exit;
	}
    }

    if (stat(contname, &sb) < 0) {
	fprintf(stderr, "stat error: %d\n", errno);
	exit(1);
    }

    /* If the container file is empty, return now */
    if (!sb.st_size)
	return ret;

    num_exts = (sb.st_size - NKN_CONTAINER_HEADER_SZ) / DEV_BSIZE;
    if (print_one_entry_per) {
	combine_uri_segments(fd, contname, num_exts);
    } else if (print_compact_output == 0)
	default_output(fd, contname, num_exts);
    else
	compact_output(fd, contname, num_exts);

 exit:
    if (attr_hash) {
	g_hash_table_foreach(attr_hash, free_an, NULL);
	g_hash_table_destroy(attr_hash);
	attr_hash = NULL;
    }
    close(fd);
    return ret;
}	/* read_container */


static int
process_cont(const char        *fpath,
	     const struct stat *sb,
	     int               typeflag)
{
    char contname[PATH_MAX];
    char *slash;

    if ((typeflag & FTW_NS) == FTW_NS)  // Stat failed: shouldn't happen
	return(0);
    if ((typeflag & FTW_DNR) == FTW_DNR)// Can not read: shouldn't happen
	return(0);
    if ((typeflag & FTW_D) == FTW_D)    // Directory
	return 0;
    slash = strrchr(fpath, '/');
    if (slash == NULL)          // Can not find /: shouldn't happen
	assert(0);
    if (*(slash+1) == '\0')     // mount point
	return 0;

    if (strcmp(slash+1, NKN_CONTAINER_NAME))
	return 0;

    contname[0] = '\0';
    strncat(contname, fpath, PATH_MAX);
    return read_container(contname);
}


/*
 * Program: cont_read
 *
 */
int
main(int  argc,
     char *argv[])
{
    char *progname;
    int	 ch, ret = 0;
    struct  stat sb;

    progname = argv[0];
    if (argc < 2)
	cont_usage(progname);

    while ((ch = getopt(argc, argv, "1acCdEhlLmMpstvR5:")) != -1) {
	switch (ch) {
	    case '1':
		print_one_entry_per++;
		break;
	    case '5':
		print_md5_chksum++;
		md5_nbytes = atoi(optarg);
		if (md5_nbytes >= 0 && md5_nbytes <= 1024*1024*2) {
		    if ( (md5_nbytes % 512) != 0) {
			printf("Number of bytes for md5 hash has to be multiple"
				" of 512 and less than  or equal to 2MB.\n");
		    cont_usage(progname);
		    }
		}
		break;
	    case 'a':
		print_attrs++;
		print_temp++;
		break;
	    case 'd':
		print_dirname++;
		break;
	    case 'c':
		print_compact_output++;
		break;
	    case 'C':
		checksum_output++;
		break;
	    case 'E':
		syslog_errors++;
		break;
	    case 'h':
		print_header++;
		break;
	    case 'l':
		lowest_tier++;
		break;
	    case 'L':
		print_content_len++;
		break;
	    case 'm':
		print_for_cliweb++;
		break;
	    case 'M':
		ram_drive++;
		break;
	    case 'p':
		print_fullpath++;
		print_dirname = 0;
		break;
	    case 's':
		skip_deleted++;
		break;
	    case 't':
		print_temp++;
		break;
	    case 'v':
		print_attr_version++;
		break;
	    case 'R':
		recursive++;
		break;
	    default:
		cont_usage(progname);
		break;
	}
    }

    if (optind >= argc)
	cont_usage(progname);

    while (optind < argc) {

	if ((ret = stat(argv[optind], &sb)) != 0) {
	    fprintf(stderr, "Unable to find: %s\n", argv[optind]);
	    exit(1);
	}

	if (print_md5_chksum)
	    getrawpartfd(argv[optind]);
	if (recursive) {
	    if (!S_ISDIR(sb.st_mode)) {
		fprintf(stderr,"%s is not a directory\n", argv[optind]);
		cont_usage(progname);
	    }
	    ftw(argv[optind], process_cont, 10);
	} else {
	    if (!S_ISREG(sb.st_mode)) {
		fprintf(stderr,"%s is not a regular file\n", argv[optind]);
		cont_usage(progname);
	    }
	    read_container(argv[optind]);
	}
	optind++;
	if (print_md5_chksum)
	    if (g_rawpart_fd > 0)
		close(g_rawpart_fd);
    }


    return 0;
}	/* main */
