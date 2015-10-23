/*
 *	Author: Michael Nishimoto (miken@nokeena.com)
 *
 *	Copyright (c) Nokeena Networks Inc., 2008
 */

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <syslog.h>
#include <glib.h>
#include <ftw.h>
#include <ctype.h>
#include "nkn_diskmgr_api.h"
#include "nkn_diskmgr2_disk_api.h"
#include "nkn_defs.h"
#include "mime_header.h"
#include "nkn_mem_counter_def.h"

#define MAX_ATTR_CNT 512	// Set to MAX_IOV_CNT is correct
/* IOV count = (2 * attribute count), for hdr+attr */
#define MAX_IOV_CNT (MAX_ATTR_CNT * 2)	// Must be <= 1024 in Linux
#define RD_V1_ATTR_BUFSZ (MAX_ATTR_CNT * 1024)  // must be multiple of 1KiB

#define MAX_V2_ATTR_BUFSZ (DM2_MAX_ATTR_DISK_SIZE * MAX_ATTR_CNT)
#define MAX_V2_ATTR_HDR_BUFSZ (DEV_BSIZE * MAX_ATTR_CNT)

typedef struct {
    char *basename;
} attr_node_t;

FILE *debug_fp;
struct iovec giov[MAX_IOV_CNT];

GHashTable *attr_hash;

/* PROTOTYPES */

static void
cache_v1_to_v2_usage(const char *progname)
{
    fprintf(stderr, "Usage: %s [-f <filename>] [-R] <mount point> |"
		    " <attribute name>\n", progname);
    fprintf(stderr, "  f: output debug filename\n");
    fprintf(stderr, "  R: convert all V1 attributes to V2 under the specified"
		    " directory and its sub-directories\n");
    exit(1);
}	/* cache_v1_to_v2_usage */


static int
dm2_check_v1_attr(DM2_disk_attr_desc_v1_t *dattrd_v1,
		  const char		  *attrpath,
		  const char		  *pbuf,
		  const char		  *rbuf)
{
    int		  skip = 0;
    nkn_attr_v1_t *dattr_v1 = (nkn_attr_v1_t *)dattrd_v1;
    nkn_attr_v1_t *dattr_v1_next = ((nkn_attr_v1_t *)dattrd_v1)+1;

    if (dattrd_v1->dat_magic == DM2_ATTR_V1_MAGIC_GONE) {
	if (dattr_v1_next->magic == NKN_ATTR_V1_MAGIC ||
	    dattr_v1_next->magic == NKN_ATTR_V1_MAGIC_OLD) {
	    skip = 2;	// Valid 2 sectors
	} else
	    skip = 1;	// Following sector is bad
    } else if (dattrd_v1->dat_magic != DM2_ATTR_MAGIC) {
	syslog(LOG_NOTICE, "Illegal magic number in attribute file (%s): "
	       "name=%16s expected=0x%x|0x%x read=0x%x offset=%ld",
	       attrpath, dattrd_v1->dat_basename, DM2_ATTR_V1_MAGIC,
	       DM2_ATTR_V1_MAGIC_GONE, dattrd_v1->dat_magic, pbuf-rbuf);
	skip = 1;
	if (dattr_v1->magic == NKN_ATTR_V1_MAGIC ||
	    dattr_v1->magic == NKN_ATTR_V1_MAGIC_OLD) {

	    syslog(LOG_NOTICE, "Expected disk descriptor in attr file (%s) "
		   "looks like disk attr at offset=%ld", attrpath, pbuf-rbuf);
	}
    }
    if (skip == 0 && dattrd_v1->dat_version != DM2_ATTR_V1_VERSION) {
	syslog(LOG_NOTICE, "Illegal version in attribute file (%s): "
	       "name=%s expected=%d read=%d offset=%ld",
	       attrpath, dattrd_v1->dat_basename, DM2_ATTR_V1_VERSION,
	       dattrd_v1->dat_version, pbuf-rbuf);
	skip = 1;
    }
    return skip;
}	/* dm2_check_v1_attr */


static void
fix_unprintables(char *basename)
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

/* Taken from old nkn_attr.c */
// Get the nth attribute value, where nth is 0 based.
// Returns 0 on success with the pointers set for id, value and length.
static int
nkn_attr_get_nth_v1(nkn_attr_v1_t     *ap_v1,
		    const int	      nth,
		    nkn_attr_id_t     *id,
		    void	      **data,
		    nkn_attr_len_v1_t *len)
{
    int i = 0, elen;
    nkn_attr_entry_v1_t *entry_v1;

    if (nth >= ap_v1->entries) {
	return 1; // Invalid nth value
    }
    entry_v1 = (nkn_attr_entry_v1_t *)ap_v1->blob;
    while (i < ap_v1->entries) {
	if (i == nth) {
	    *id = entry_v1->id;
	    *data = (char *)entry_v1 + sizeof(nkn_attr_entry_v1_t);
	    *len = entry_v1->len;
	    break;
	}
	elen = sizeof(nkn_attr_entry_v1_t) + entry_v1->len;
	i++;
	entry_v1 = (nkn_attr_entry_v1_t *)((char *)entry_v1 + elen);
	assert(((uint64_t)entry_v1 - (uint64_t)ap_v1) < NKN_DEFAULT_ATTR_SIZE);
    }
    return 0; // Success
}	/* nkn_attr_get_nth_v1 */


/* Taken verbatim from nkn_attr.c */
static int
s_nkn_attr_set(nkn_attr_t    *ap,
	       nkn_attr_id_t id,
	       uint32_t      len,
	       void	     *value)
{
    nkn_attr_entry_t *entry;
    int addlen;

    assert(sizeof(nkn_attr_entry_data_t) == 4);
    assert(sizeof(nkn_attr_entry_t) == 8);
    assert(NKN_ATTR_HEADER_LEN == 256);
    /* No need to check the validity of 'len'.  If it were too large, we
     * would overflow against total_attrsize */

    // Check for overflow
    addlen = sizeof(nkn_attr_entry_t) + len;
    if ((ap->blob_attrsize + addlen) > ap->total_attrsize)
	return NKN_ATTR_OVERFLOW;

    /* By adding these 2 values, we could have an odd address which is
     * supposed on x86 but not on other ARCHs, like MIPS */
    entry = (nkn_attr_entry_t *)((char *)ap + ap->blob_attrsize);
    entry->id = id;
    entry->attr_entry_len = len;
    memcpy((char *)entry + sizeof(nkn_attr_entry_t), value, len);
    ap->na_entries++;
    ap->blob_attrsize += addlen;
    return 0;
}


/* Taken almost verbatim from nkn_attr.c */
static void
s_nkn_attr_init(nkn_attr_t *ap)
{
    /* Assume that the caller has allocated a correct size buffer */
    memset(ap, 0, NKN_DEFAULT_ATTR_SIZE);
    ap->na_magic = NKN_ATTR_MAGIC;
    ap->na_version = NKN_ATTR_VERSION;
    ap->na_entries = 0;
    ap->blob_attrsize = NKN_ATTR_HEADER_LEN;
    ap->total_attrsize = DM2_MAX_ATTR_DISK_SIZE;	// Line changed
}


static unsigned long
v1_do_csum64_iterate_aligned(const unsigned char *buff,
			     int		 len,
			     unsigned long	 old_checksum)
{
    unsigned long result = old_checksum;

    assert(len % 8 == 0);
    assert(((uint64_t)buff & 0x7) == 0);

    for (; len > 0; len -= 8, buff += 8) {
	result ^= *(uint64_t *)buff;
    }
    return result;
}	/* v1_do_csum_iterate_aligned */


static void
copy_common_attr_fields(nkn_attr_t *attr,
			nkn_attr_v1_t *dattr_v1)
{
    s_nkn_attr_init(attr);
    attr->na_entries = dattr_v1->entries;
    attr->na_flags = dattr_v1->flags;
    attr->content_length = dattr_v1->content_length;
    attr->cache_expiry = dattr_v1->cache_expiry;
    attr->cache_time0 = dattr_v1->cache_time0;
    attr->hotval = dattr_v1->hotval;
}	/* copy_common_attr_fields */


static void
copy_blob_attr_fields(nkn_attr_t *attr,
		      nkn_attr_v1_t *dattr_v1,
		      mime_header_t *mh,
		      const char *basename)
{
    nkn_attr_id_t	id;
    nkn_attr_len_v1_t	attr_len;
    void		*data;
    char		*serialbuf;
    int			serialbufsz, ret, i;

    for (i = 0; i < dattr_v1->entries; i++) {
	ret = nkn_attr_get_nth_v1(dattr_v1, i, &id, &data, &attr_len);
	if (ret != 0) {
	    syslog(LOG_NOTICE, "Found inconsistent attribute i=%d for "
		   "basename=%s, skipping conversion => object will get "
		   "deleted", i, basename);
	    if (debug_fp) {
		fprintf(debug_fp, "%s INCONSISTENT\n", basename);
		continue;
	    }
	}

	if ((id.u.attr.protocol_id == NKN_PROTO_HTTP) &&
	    (id.u.attr.id != NKN_ATTR_MIME_HDR_ID)) {
	    ret = mime_hdr_add_known(mh, id.u.attr.id, data,
				     (int32_t)attr_len);
	    if (ret) {
	    	syslog(LOG_NOTICE,
			"Problem adding attribute to mime_header_t "
			"attr.id=%d len=%d i=%d for basename=%s",
			(int32_t)id.u.attr.id, (int32_t)attr_len, i, basename);
	    	if (debug_fp) {
		    fprintf(debug_fp, "%s INCONSISTENT\n", basename);
		    continue;
		}
	    }
	} else {
	    ret = s_nkn_attr_set(attr, id, (uint32_t)attr_len, data);
	    if (ret != 0) {
	    	syslog(LOG_NOTICE,
		       "Problem setting attribute i=%d for basename=%s,"
		       " skipping conversion => object will get deleted",
		       i, basename);
	    	if (debug_fp) {
		    fprintf(debug_fp, "%s INCONSISTENT\n", basename);
		    continue;
	    	}
	    }
	}
    }

    if (mh->cnt_known_headers) {
    	serialbufsz = mime_hdr_serialize_datasize(mh, 0, 0, 0);
	if (serialbufsz) {
	    serialbuf = alloca(serialbufsz);
	    ret = mime_hdr_serialize(mh, serialbuf, serialbufsz);
	    if (!ret) {
	    	id.u.attr.protocol_id = NKN_PROTO_HTTP;
	    	id.u.attr.id = NKN_ATTR_MIME_HDR_ID;

	    	ret = s_nkn_attr_set(attr, id, serialbufsz, serialbuf);
	    	if (ret) {
		    syslog(LOG_NOTICE,
			   "Problem adding mime_header_t attribute "
			   "attr.id=%d len=%d for basename=%s",
			   (int32_t)id.u.attr.id, serialbufsz, basename);
		    if (debug_fp) {
		    	fprintf(debug_fp, "%s INCONSISTENT\n", basename);
		    }
	    	}
	    } else {
	    	syslog(LOG_NOTICE,
		       "Problem mime_hdr_serialize() ret=%d serialbufsz=%d "
		       "for basename=%s", ret, serialbufsz, basename);
	    	if (debug_fp)
		    fprintf(debug_fp, "%s INCONSISTENT\n", basename);
	    }
    	} else {
	    syslog(LOG_NOTICE, "mime_hdr_serialize_datasize() failed");
	    if (debug_fp) {
		fprintf(debug_fp, "%s INCONSISTENT\n", basename);
	    }
	}
    }
}	/* copy_blob_attr_fields */

static void
write_attr(const int	 afd2,
	   char		 *wabuf,
	   char		 *whbuf,
	   nkn_attr_v1_t *dattr_v1,
	   const char	 *attrpath,
	   char		 *basename)
{
    mime_header_t mh;
    char *uri, *slash, *ptime;
    nkn_attr_t *attr = (nkn_attr_t *)wabuf;
    DM2_disk_attr_desc_t *head = (DM2_disk_attr_desc_t *)whbuf;
    struct iovec iov[2];
    int ret, len;

    if (debug_fp) {
	fix_unprintables(basename);
	len = strlen(attrpath) + strlen(basename);
	uri = alloca(len);
	strcpy(uri, attrpath);
	slash = strrchr(uri, '/');
	if (slash != NULL) {
	    *(slash+1) = '\0';
	} else {
	    uri[0] = '\0';
	}
	strcat(uri, basename);
	fprintf(debug_fp, "%s FOUND\n", uri);
    }

    copy_common_attr_fields(attr, dattr_v1);
    mime_hdr_init(&mh, MIME_PROT_HTTP, 0, 0);
    copy_blob_attr_fields(attr, dattr_v1, &mh, basename);

    memset(head, 0, DEV_BSIZE);
    head->dat_magic = DM2_ATTR_MAGIC;
    head->dat_version = DM2_ATTR_VERSION;
    head->dat_len = strlen(basename);
    strncpy(head->dat_basename, basename, NAME_MAX);

    attr->na_checksum =
	v1_do_csum64_iterate_aligned((uint8_t *)attr, attr->total_attrsize, 0);

    iov[0].iov_base = head;
    iov[0].iov_len = DEV_BSIZE;
    iov[1].iov_base = attr;
    iov[1].iov_len = DM2_MAX_ATTR_DISK_SIZE;
    ret = writev(afd2, iov, 2);
    if (ret == -1) {
	syslog(LOG_NOTICE, "Failed to writev attr %s to %s: %d (afd=%d) "
	       "(iov=%p) (head=%p) (attr=%p)",
	       basename, attrpath, errno, afd2, iov, head, attr);
	exit(1);
    }
    if (ret != DEV_BSIZE + DM2_MAX_ATTR_DISK_SIZE) {
	syslog(LOG_NOTICE, "Short writev %s to %s: %d", basename, attrpath,ret);
	exit(1);
    }

    mime_hdr_shutdown(&mh);
    return;
}	/* write_attr */

/* This function prepares the IOVs for writing the attributes to disk */
static void
prepare_attr_bulk(char		 *wabuf,
	   	  char		 *whbuf,
	   	  nkn_attr_v1_t *dattr_v1,
	   	  const char	 *attrpath,
	   	  char		 *basename,
		  int 		 index)
{
    mime_header_t mh;
    char *uri, *slash, *ptime;
    nkn_attr_t *attr = (nkn_attr_t *)wabuf;
    DM2_disk_attr_desc_t *head = (DM2_disk_attr_desc_t *)whbuf;
    int ret, len;

    if (debug_fp) {
	fix_unprintables(basename);
	len = strlen(attrpath) + strlen(basename);
	uri = alloca(len);
	strcpy(uri, attrpath);
	slash = strrchr(uri, '/');
	if (slash != NULL) {
	    *(slash+1) = '\0';
	} else {
	    uri[0] = '\0';
	}
	strcat(uri, basename);
	fprintf(debug_fp, "%s FOUND\n", uri);
    }

    copy_common_attr_fields(attr, dattr_v1);
    mime_hdr_init(&mh, MIME_PROT_HTTP, 0, 0);
    copy_blob_attr_fields(attr, dattr_v1, &mh, basename);

    memset(head, 0, DEV_BSIZE);
    head->dat_magic = DM2_ATTR_MAGIC;
    head->dat_version = DM2_ATTR_VERSION;
    head->dat_len = strlen(basename);
    strncpy(head->dat_basename, basename, NAME_MAX);

    attr->na_checksum =
	v1_do_csum64_iterate_aligned((uint8_t *)attr, attr->total_attrsize, 0);

    giov[index*2].iov_base = head;
    giov[index*2].iov_len = DEV_BSIZE;
    giov[index*2+1].iov_base = attr;
    giov[index*2+1].iov_len = DM2_MAX_ATTR_DISK_SIZE;

    mime_hdr_shutdown(&mh);
    return;
}	/* prepare_attr_bulk */

/* This is the function that performs the disk write of the attributes */
static void
flush_attr(const int	 afd2,
	   int		 count,
	   const char	 *attrpath)
{
    int ret;

    ret = writev(afd2, giov, count);
    if (ret == -1) {
	syslog(LOG_NOTICE, "Failed to writev %d attributes to %s: %d "
	       "(afd=%d) (giov=%p) (count=%d)",
	       count/2, attrpath, errno, afd2, giov, count);
	exit(1);
    }
    /*
     * Each attribute is (512+4096) bytes long, so the expected return
     * value from writev is number of attribute times (512+4096). Number of
     * attributes is the half of iov count.
     */
    if (ret != (DEV_BSIZE + DM2_MAX_ATTR_DISK_SIZE)* count / 2) {
	syslog(LOG_NOTICE, "Short writev to %s: %d", attrpath, ret);
	exit(1);
    }
    return;
}	/* flush_attr */

static void
cleanup_attr_hash(const gpointer key,
		  const gpointer value,
		  const gpointer user_data)
{
    attr_node_t* an = (attr_node_t*)value;

    free(an->basename);
    free(an);
}	/* cleanup_attr_hash */

/*
 * This function attempts to convert 1024 attributes at a time and writes them 
 * one shot.
 */
static void
convert_attributes_bulk(const char *attrname,
		   	const char *attrname2)
{
    DM2_disk_attr_desc_v1_t *dattrd;
    attr_node_t *uri_attr;
    nkn_attr_v1_t *dattr;
    char *rbuf, *pbuf, *wabuf, *whbuf;
    int afd, afd2, secnum, rdsz, ret, skip, i;

    if ((afd = open(attrname, O_RDONLY | O_DIRECT)) == -1) {
	fprintf(stderr, "Can not open: %s\n", attrname);
	exit(1);
    }
    if ((afd2 = open(attrname2, O_WRONLY | O_CREAT | O_DIRECT, 0666)) == -1) {
	fprintf(stderr, "Can not open: %s\n", attrname2);
	exit(1);
    }

    if (posix_memalign((void *)&rbuf, DEV_BSIZE, RD_V1_ATTR_BUFSZ)) {
	fprintf(stderr, "posix_memalign failed: %d %d\n", 
		RD_V1_ATTR_BUFSZ, errno);
	exit(1);
    }

    if (posix_memalign((void *)&wabuf, DEV_BSIZE, MAX_V2_ATTR_BUFSZ)) {
	fprintf(stderr, "posix_memalign failed: %d %d\n",
		MAX_V2_ATTR_BUFSZ, errno);
	exit(1);
    }

    if (posix_memalign((void *)&whbuf, DEV_BSIZE, MAX_V2_ATTR_HDR_BUFSZ)) {
	fprintf(stderr, "posix_memalign failed: %d %d\n",
	        MAX_V2_ATTR_HDR_BUFSZ, errno);
	exit(1);
    }

    secnum = 0;
    while ((rdsz = pread(afd, rbuf, RD_V1_ATTR_BUFSZ, secnum*DEV_BSIZE)) != 0) {
	if (rdsz == -1) {
	    fprintf(stderr, "Read of attribute file (%s) failed: %d\n",
		    attrname, errno);
	    free(rbuf);
	    close(afd);
	    return;
	}
	if (rdsz < DEV_BSIZE) {
	    break;
	}
	if (rdsz % DEV_BSIZE != 0) {
	    syslog(LOG_NOTICE, "Read from attribute file (%s) is "
		   "not an integral sector amount: %d", attrname, rdsz);
	    /* Truncate remaining information */
	}

	i = 0;
	memset(giov, 0, sizeof(struct iovec) * MAX_IOV_CNT);
	for (pbuf = rbuf, dattrd = (DM2_disk_attr_desc_v1_t *)rbuf;
	     rdsz >= DEV_BSIZE;
	     rdsz -= DEV_BSIZE, pbuf += DEV_BSIZE, secnum++,
		 dattrd = (DM2_disk_attr_desc_v1_t *)pbuf) {
	    skip = dm2_check_v1_attr(dattrd, attrname, pbuf, rbuf);
	    if (skip == 1) {
		continue;
	    } else if (skip == 2) {	// GONE magic w/ good attr
		secnum++;
		pbuf += DEV_BSIZE;
		rdsz -= DEV_BSIZE;
		if (debug_fp) {
		    fprintf(debug_fp, "%s %s SKIPPED\n", attrname,
			    dattrd->dat_basename);
		}
		continue;
	    }
	    rdsz -= DEV_BSIZE;
	    pbuf += DEV_BSIZE;
	    dattr = (nkn_attr_v1_t *)pbuf;
	    if (rdsz < NKN_MAX_ATTR_V1_SIZE) {
		/* OK to do this because rdsz should always be a multiple
		 * of 2 sectors (descriptor + nkn_attr_v1_t) */
		syslog(LOG_NOTICE, "Lost attribute for %s (%s)",
		       dattrd->dat_basename, attrname);
		free(rbuf);
		close(afd);
		return;
	    }

	    if (dattr->magic != NKN_ATTR_V1_MAGIC &&
		dattr->magic != NKN_ATTR_V1_MAGIC_OLD) {
		syslog(LOG_NOTICE, "Bad ATTR magic number (0x%x) at sector "
		       "(%d) (file=%s)", dattr->magic, secnum, attrname);
		continue;
	    }

	    if (g_hash_table_lookup(attr_hash, dattrd->dat_basename)) {
		if (debug_fp)
		    fprintf(debug_fp, "%s DUPLICATE\n", uri_attr->basename);
		continue;
	    }

	    prepare_attr_bulk(wabuf+(i*DM2_MAX_ATTR_DISK_SIZE),
			      whbuf+(i*DEV_BSIZE), dattr, attrname,
			      dattrd->dat_basename, i);
	    uri_attr = calloc(1, sizeof(attr_node_t));
	    uri_attr->basename = strdup(dattrd->dat_basename);
	    g_hash_table_insert(attr_hash, uri_attr->basename, uri_attr);
	    secnum++;
	    i++;
	}
	flush_attr(afd2, i*2, attrname);
    }

    /* Free up the allocated memory */
    g_hash_table_foreach(attr_hash, cleanup_attr_hash, NULL);

    close(afd);
    close(afd2);
    free(rbuf);
    free(wabuf);
    free(whbuf);
    return;
}	/* convert_attributes_bulk */

static int
convert_attrfile(char *attrfile)
{
    char    attrname[PATH_MAX];
    char    attrname_tmp[PATH_MAX];
    char    attrname2[PATH_MAX];
    struct  stat sb;
    int	    ret = 0;
    char    *slash;

    attrname[0] = attrname_tmp[0] = attrname2[0] = '\0';

    strncat(attrname, attrfile, PATH_MAX);
    strncat(attrname_tmp, attrfile, PATH_MAX);
    strcat(attrname_tmp, ".tmp");
    remove(attrname_tmp);

    strncat(attrname2, attrfile, PATH_MAX);

    if ((slash = strrchr(attrname2, '/')) == NULL) {
	strcpy(attrname2, NKN_CONTAINER_ATTR_NAME);
    } else {
	*(slash+1) = '\0';
	strcat(attrname2, NKN_CONTAINER_ATTR_NAME);
    }

 //   printf("%s	%s\n",attrname_tmp, attrname2);
    if ((ret = stat(attrname2, &sb)) == 0) {
	/* This directory already converted */
	if (debug_fp)
	    fprintf(debug_fp, "%s Already Converted\n", attrname);

	goto end;
    }

    attr_hash = g_hash_table_new(g_str_hash, g_str_equal);
    convert_attributes_bulk(attrname, attrname_tmp);
    if ((ret = rename(attrname_tmp, attrname2)) != 0) {
	syslog(LOG_NOTICE, "Rename %s to %s failed: %d",
	       attrname_tmp, attrname2, errno);
	ret = 1;
    }
 end:
    if (attr_hash) {
	g_hash_table_destroy(attr_hash);
	attr_hash = NULL;
    }
    return ret;
}	/* convert_attrfile */

static int
process_attrfile(const char        *fpath,
             const struct stat *sb,
             int               typeflag)
{

    char *slash;
    char attrname[PATH_MAX];

    if ((typeflag & FTW_NS) == FTW_NS)  // Stat failed: shouldn't happen
	return 0;
    if ((typeflag & FTW_DNR) == FTW_DNR)// Can not read: shouldn't happen
	return 0;
    if ((typeflag & FTW_D) == FTW_D)    // Directory
	return 0;
    slash = strrchr(fpath, '/');
    if (slash == NULL)          // Can not find /: shouldn't happen
	return 0;
    if (*(slash+1) == '\0')     // mount point
	return 0;

    if (strcmp(slash+1, NKN_CONTAINER_ATTR_NAME_V1))
	return 0;

    attrname[0] = '\0';
    strncat(attrname, fpath, PATH_MAX); 

    return convert_attrfile(attrname);
}	/* process_attrfile */

/*
 * Program: cont_read
 *
 */
int
main(int  argc,
     char *argv[])
{
    char *progname, *mount_pt = NULL;
    char *debug_fname = NULL;
    int	 ch, recursive = 0;
    struct  stat sb;
    int     ret = 0;

    progname = argv[0];
    if (argc < 2)
	cache_v1_to_v2_usage(progname);

    while ((ch = getopt(argc, argv, "f:R")) != -1) {
	switch (ch) {
	    case 'f':
		debug_fname = optarg;
		break;
	    case 'R':
		recursive++;
		break;
	    default:
		cache_v1_to_v2_usage(progname);
		break;
	}
    }

    if (optind >= argc)
	cache_v1_to_v2_usage(progname);

    if (debug_fname) {
	debug_fp = fopen(debug_fname, "a");
	if (debug_fp == NULL) {
	    fprintf(stderr, "Debug file error: %d\n", errno);
	    exit(1);
	}
    }

    if ((ret = stat(argv[optind], &sb)) != 0) {
	fprintf(stderr, "File not found: %s\n", argv[optind]);
	exit(1);
    }

    if (!recursive) {
	if (!S_ISREG(sb.st_mode)) {
	    fprintf(stderr,"%s is not a regular file\n", argv[optind]);
	    cache_v1_to_v2_usage(progname);
	}
	convert_attrfile(argv[optind]);
    } else {
	if (!S_ISDIR(sb.st_mode)) {
	    fprintf(stderr,"%s is not a directory\n", argv[optind]);
	    cache_v1_to_v2_usage(progname);
	}
	ftw(argv[optind], process_attrfile, 10);
    }

    return 0;
}	/* main */

extern int nkn_mon_add(const char *name_tmp, char *instance, void *obj,
		       uint32_t size);
int nkn_mon_add(const char *name_tmp __attribute((unused)),
		char *instance __attribute((unused)),
		void *obj __attribute((unused)),
		uint32_t size __attribute((unused)))
{
	return 0;
}
