/*
 *	Author: Srihari MS (srihari@nokeena.com)
 *
 *	Copyright (c) Nokeena Networks Inc., 2008
 */

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <syslog.h>
#include <glib.h>
#include <ctype.h>
#include "nkn_diskmgr_api.h"
#include "nkn_diskmgr2_disk_api.h"
#include "nkn_util.h"
#include "nkn_defs.h"
#include "mime_header.h"
#include "nkn_mem_counter_def.h"

FILE* out_fp = NULL;

static int
da_check_attr_head(DM2_disk_attr_desc_t *dattrd,
                    const char           *attrpath,
                    const int            secnum,
                    const char           *pbuf,
                    const char           *rbuf)
{
    int         skip = 0;
    nkn_attr_t  *dattr = (nkn_attr_t *)dattrd;

    if (dattrd->dat_magic == DM2_ATTR_MAGIC_GONE) {
	printf( "attribute file (%s) name=%s offset=%ld secnum=%d"
	       " GONE magic\n", attrpath, dattrd->dat_basename, pbuf-rbuf, secnum);
        skip = 8;
    } else if (dattrd->dat_magic != DM2_ATTR_MAGIC) {
	printf("Illegal magic number in attribute file (%s) @"
	       " sec=%d: name=%16s expected=0x%x|0x%x read=0x%x offset=%ld\n",
	       attrpath, secnum, dattrd->dat_basename, DM2_ATTR_MAGIC,
	       DM2_ATTR_MAGIC_GONE, dattrd->dat_magic, pbuf-rbuf);
	skip = 1;
	if (dattr->na_magic == NKN_ATTR_MAGIC) {
	    printf("Expected disk descriptor in attr file (%s)"
		   "looks like disk attr at offset=%ld secnum=%d\n",
		   attrpath, pbuf-rbuf, secnum);
	}
    }
    if (skip == 0 && dattrd->dat_version != DM2_ATTR_VERSION) {
	printf("Illegal version in attribute file (%s) @ sec=%d: "
	       " name=%s expected=%d read=%d offset=%ld\n",
	       attrpath, secnum, dattrd->dat_basename, DM2_ATTR_VERSION,
	       dattrd->dat_version, pbuf-rbuf);
	skip = 1;
    }
    return skip;
}       /* da_check_attr_head */

static void 
print_attr(nkn_attr_t *attr, char* basename)
{
    mime_header_t mh;
    char outbuf[4096];
    int ret = 0;

    mime_hdr_init(&mh, MIME_PROT_HTTP, 0, 0);
    ret = nkn_attr2mime_header(attr, 0, &mh);
    assert(ret == 0);
    memset(outbuf, 0, 4096);
    dump_http_header(&mh, outbuf, 4096, 0);

    fprintf(out_fp, "Basename\t%s\n", basename);
    fprintf(out_fp, "Checksum\t0x%lx\n", attr->na_checksum);
    fprintf(out_fp, "Magic\t0x%x\n", attr->na_magic);
    fprintf(out_fp, "Version\t%d\n", attr->na_version);
    fprintf(out_fp, "Blob-Entries\t%d\n", attr->na_entries);

    fprintf(out_fp, "Content-Length\t%ld\n", attr->content_length);
    fprintf(out_fp, "Cache-Expiry\t%ld\n", attr->cache_expiry);
    fprintf(out_fp, "Cache-Time0\t%ld\n", attr->cache_time0);
    fprintf(out_fp, "Origin-Cache-Create\t%ld\n", attr->origin_cache_create);
    fprintf(out_fp, "Cache-Correctedage\t%ld\n", attr->cache_correctedage);
    fprintf(out_fp, "Cache-RevalTime\t%ld\n", attr->cache_reval_time);
    fprintf(out_fp, "Hotval\t0x%lx\n", attr->hotval);
    fprintf(out_fp, "Total-Size\t%d\n", attr->total_attrsize);
    fprintf(out_fp, "Blob-Size\t%d\n", attr->blob_attrsize);

    fprintf(out_fp, "Private-Type\t%d\n", attr->priv_type);
    fprintf(out_fp, "Video-Bitrate\t%d\n", attr->vpe_video_bitrate);
    fprintf(out_fp, "Audio-Bitrate\t%d\n", attr->vpe_audio_bitrate);
    fprintf(out_fp, "Duration\t%d\n", attr->vpe_duration);
    fprintf(out_fp, "Video-Codec-Type\t%d\n", attr->vpe_video_codec_type);
    fprintf(out_fp, "Audio-Codec-Type\t%d\n", attr->vpe_audio_codec_type);
    fprintf(out_fp, "Container-Type\t%d\n", attr->vpe_cont_type);
    
    fprintf(out_fp, "%s\n", outbuf);

    mime_hdr_shutdown(&mh);

    return;
}   /* print_attr */

static void
dump_attr_usage(char* progname)
{
    fprintf(stderr, "Usage: %s <attribute-file> <output-file>\n", progname);
    exit(1);
}   /* dump_attr_usage */

static int
da_check_attr(nkn_attr_t       *dattr,
               const size_t     secnum,
	       const char*	attrpath)
{
    uint64_t disk_checksum, calc_checksum;

    if (dattr->na_magic != NKN_ATTR_MAGIC) {
	printf("Bad magic number (0x%x) at sector (%ld) "
	       "(file=%s)\n", dattr->na_magic, secnum, attrpath);
	return 1;
    }

    disk_checksum = dattr->na_checksum;
    dattr->na_checksum = 0;
    calc_checksum = do_csum64_iterate_aligned((uint8_t *)dattr,
					       dattr->total_attrsize, 0);
    if (disk_checksum != 0 && calc_checksum != disk_checksum) {
	printf("Checksum mismatch: expected=0x%lx got=0x%lx at "
	       "sector=%ld file=%s\n", disk_checksum, calc_checksum,
	       secnum, attrpath);
	return 1;
    }

    return 0;
}       /* da_check_attr */

/*
 * Program: attr_dump
 *
 */
int
main(int  argc,
     char *argv[])
{
    char *attrpath = NULL, *out_fname = NULL, *progname = NULL;
    struct stat sb;
    nkn_attr_t *attr;
    DM2_disk_attr_desc_t *dattrd;
    char *rbuf, *pbuf;
    int ch, afd, rdsz, ret = 0, skip, i;
    off_t secnum;
#define RD_ATTR_BUFSZ (4096+512)

    progname = argv[0];
    if (argc < 3)
	dump_attr_usage(progname);

    attrpath = argv[1];
    out_fname = argv[2];

    out_fp = fopen(out_fname, "w+");
    if (out_fp == NULL) {
	fprintf(stderr, "Debug file error: %d\n", errno);
	exit(1);
    }

    afd = open(attrpath, O_RDONLY | O_DIRECT);
    if (afd < 0) {
	printf("open error: %d\n", errno);
	exit(1);
    }

    if (posix_memalign((void *)&rbuf, DEV_BSIZE, RD_ATTR_BUFSZ)) {
	fprintf(stderr, "posix_memalign failed: %d %d\n", RD_ATTR_BUFSZ, errno);
	exit(1);
    }

    secnum = 0;
    while ((rdsz = pread(afd, rbuf, RD_ATTR_BUFSZ, secnum*DEV_BSIZE)) != 0) {
	if (rdsz == -1) {
	    fprintf(stderr, "Read of attribute file (%s) failed: %d\n",
		    attrpath, errno);
	    free(rbuf);
	    close(afd);
	    return rdsz;
	}

	if (rdsz % DEV_BSIZE != 0) {
	    printf( "Read from attribute file (%s) is "
		   "not an integral sector amount: %d\n", attrpath, rdsz);
	}

	for (pbuf = rbuf, dattrd = (DM2_disk_attr_desc_t *)rbuf;
	    rdsz >= DM2_MAX_ATTR_DISK_SIZE;
	    dattrd = (DM2_disk_attr_desc_t *)pbuf) {
	    skip = da_check_attr_head(dattrd, attrpath, secnum, pbuf, rbuf);
	    if (skip)
		goto next_attr;

	    attr = (nkn_attr_t*)(pbuf + DEV_BSIZE);
	    da_check_attr(attr, secnum, attrpath);
	    print_attr(attr, dattrd->dat_basename);

	next_attr:
	    secnum += DM2_ATTR_TOT_DISK_SECS;
	    rdsz -= DM2_ATTR_TOT_DISK_SECS * DEV_BSIZE;
	    pbuf += DM2_ATTR_TOT_DISK_SECS * DEV_BSIZE;
	}
    }

    close(afd);
    return ret;
}	/* main */

/* Function is called in libnkn_memalloc, but we don't care about this. */
extern int nkn_mon_add(const char *name_tmp, char *instance, void *obj,
		       uint32_t size);
int nkn_mon_add(const char *name_tmp __attribute((unused)),
		char *instance __attribute((unused)),
		void *obj __attribute((unused)),
		uint32_t size __attribute((unused)))
{
	return 0;
}
