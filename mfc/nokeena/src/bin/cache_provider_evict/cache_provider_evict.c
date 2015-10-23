/*
 *	Author: Michael Nishimoto (miken@nokeena.com)
 *
 *	Copyright (c) Nokeena Networks Inc., 2008
 */

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ftw.h>
#include <glib.h>
#include <assert.h>
#include <signal.h>
#include <sys/prctl.h>
#include <atomic_ops.h>
#include <malloc.h>

#include "nkn_debug.h"
#include "nkn_diskmgr_api.h"
#include "nkn_diskmgr2_disk_api.h"
#include "nkn_mem_counter_def.h"

#define CONF_FILE "/config/nkn/cache_provider_evict.conf"

/*
 * Formula to calculate eviction value is:
 *
 *  A(Hits) + B(Size) + C(time delay/sync_interval) +
 *	D(Hits)(Size) + E(Hits)(log(Size))
 *
 * The default formula is:
 *
 *	3*Hits -1*(time delay/sync_interval)
 */
typedef struct evict_uri_s {
    char	*e_uri;
    int32_t	e_decay_temp;
    uint64_t	e_orig_hot;
    int		e_num_pages;
    int		e_dup;
    uint64_t	e_size;
    uint64_t	e_contlen;
    GList	*e_physid_list;
} evict_uri_t;


int DEBUG = 0;

int g_provider = -1;
unsigned long g_hotness_seed = -1;
int g_sync_interval = -1;
int g_temperature_seed = -1;
int g_blocksize = -1;
int g_blocksize_shift;
int g_pagesize;
int g_pagesize_shift;
GQueue *g_evict_queue;
char *g_parent_dir;
char *g_rbuf;
long g_del_blocks;
long g_del_pages;

time_t g_evict_start_time;
/* Multipliers for eviction formula */
float g_A = 3.0;	// Multiplier for 'Hits'
float g_B;		// Multiplier for 'Size'
float g_C = -1.0;	// Multiplier for 'time delay'
float g_D;		// Multiplier for (Hits) x (Size)
float g_E;		// Multiplier for (Hits) x log(Size)
float g_F;		// Multiplier for (Hits) x log(content len)

struct timeval g_tv;

AO_t total_mem_alloc;
AO_t print_mem;
AO_t num_evict;
AO_t print_evict;

/* PROTOTYPES */
static void usage(const char *progname);
void my_AM_decay_hotness(nkn_hot_t *in_seed,
			 time_t cur_time,
			 int sync_interval,
			 uint64_t size,
			 float A, float B, float C, float D, float E);

#define RD_ATTR_BUFSZ	(100*4608)	// must be multiple of 1KiB

static void
print_mem_debug(void)
{
    AO_fetch_and_add(&total_mem_alloc, sizeof(evict_uri_t));
    if (DEBUG && total_mem_alloc > print_mem+10240) {
	fprintf(stderr, "mem: %ld\n", total_mem_alloc);
	AO_store(&print_mem, total_mem_alloc);
    }
    AO_fetch_and_add1(&num_evict);
    if (DEBUG && num_evict > print_evict+100) {
	fprintf(stderr, "evict: %ld\n", num_evict);
	AO_store(&print_evict, num_evict);
    }
}	/* print_mem_debug */


static void
normal_exit(void)
{
    DBG_LOG(MSG, MOD_DM2, "Finishing: cache_provider_evict ...");
    sleep (2);
    log_thread_end();
}

static void
usage(const char *progname)
{
    fprintf(stderr, "Usage: %s [-D][-d #][-p #][-s #] <mount pts>\n",
	    progname);
    fprintf(stderr, "  D: debug\n");
    fprintf(stderr, "  d: blocks to delete\n");
    fprintf(stderr, "  p: provider enum\n");
    fprintf(stderr, "  s: seed hotness\n");
    fprintf(stderr, "  t: sync interval in seconds\n");
    exit(1);
}	/* usage */


static int
dm2_bitshift32(uint32_t size)
{
    int i;
    int bitcnt = 0;

    for (i=0; i<32; i++) {
	if ((size & 0x1) == 0) {
	    bitcnt++;
	    size >>= 1;
	} else {
	    assert((size >> 1) == 0);
	    break;
	}
    }
    return bitcnt;
}	/* dm2_bitshift32 */

static int
cmp_highest_temp_first(gconstpointer in_a,
		       gconstpointer in_b,
		       gpointer user_data __attribute((unused)))
{
    evict_uri_t *a = (evict_uri_t *)in_a;
    evict_uri_t *b = (evict_uri_t *)in_b;

    if (a->e_decay_temp < b->e_decay_temp)
	return 1;
    else
	return 0;
}	/* cmp_highest_temp_first */

static int
dm2_check_attr(DM2_disk_attr_desc_t *dattrd,
	       const char	    *attr_pathname,
	       const char	    *pbuf,
	       const char	    *rbuf)
{
    int skip = 0;

    if (dattrd->dat_magic == DM2_ATTR_MAGIC_GONE) {
	/*
	 * Do nothing; Can skip this sector.
	 * It's possible to store these sector offsets in a list and
	 * reuse the sectors.  However, this is more work than I want
	 * to do at this point.
	 */
	skip = 1;
    } else if (dattrd->dat_magic != DM2_ATTR_MAGIC) {
	skip = 1;
    }
    if (dattrd->dat_version != DM2_ATTR_VERSION) {
	skip = 1;
    }
    return skip;
}	/* dm2_check_attr */


static int
dm2_check_disk_ext(const DM2_disk_extent_t *dext,
		   const char		   *cont_pathname,
		   const char		   *pbuf,
		   const char		   *buf,
		   const int		   secnum)
{
    int skip = 0;

    if (dext->dext_header.dext_magic == DM2_DISK_EXT_DONE) {
	/*
	 * Do nothing; Can skip this sector.
	 * It's possible to store these sector offsets in a list and
	 * reuse the sectors.  However, this is more work than I want
	 * to do at this point.
	 */
	skip = 1;
    } else if (dext->dext_header.dext_magic != DM2_DISK_EXT_MAGIC) {
	DBG_LOG(ERROR, MOD_DM2,"ERROR: Illegal magic number in container (%s): "
		"name=%s expected=0x%x|0x%x read=0x%x offset=%ld/%d ",
		cont_pathname, dext->dext_basename, DM2_DISK_EXT_MAGIC,
		DM2_DISK_EXT_DONE, dext->dext_header.dext_magic, pbuf-buf, secnum);
	DBG_LOG(ERROR, MOD_DM2,
		"    physid=%lx uol_off=%zd uol_len=%ld ext_start=%zd",
		dext->dext_physid, dext->dext_offset,
		dext->dext_length, dext->dext_start_sector);
	skip = 1;
    }
    if (dext->dext_header.dext_version != DM2_DISK_EXT_VERSION) {
	DBG_LOG(ERROR, MOD_DM2, "ERROR: Illegal version in container (%s): "
		"name=%s expected=%d read=%d offset=%ld/%d ",
		cont_pathname, dext->dext_basename, DM2_DISK_EXT_VERSION,
		dext->dext_header.dext_version, pbuf-buf, secnum);
	DBG_LOG(ERROR, MOD_DM2,
		"     physid=%lx uol_off=%zd uol_len=%ld ext_start=%zd",
		dext->dext_physid, dext->dext_offset,
		dext->dext_length, dext->dext_start_sector);
	skip = 1;
    }
    return skip;
}	/* dm2_check_disk_ext */


static GHashTable *
read_container(const char *attrpath)
{
#define RD_EXT_BUFSZ    (256*1024)
    struct stat sb;
    char *contpath, *slash, *buf, *pbuf;
    DM2_disk_extent_t *dext;
    GHashTable *uri_hash;
    GList *uri_list;
    evict_uri_t *uri_node;
    uint64_t *physid;
    int cfd, secnum, nexts, rdsz;

    contpath = alloca(strlen(attrpath) + 5);
    strcpy(contpath, attrpath);
    slash = strrchr(contpath, '/');
    *(slash+1) = '\0';
    strcat(contpath, NKN_CONTAINER_NAME);

    uri_hash = g_hash_table_new(g_str_hash, g_str_equal);

    if (posix_memalign((void *)&buf, DEV_BSIZE, RD_EXT_BUFSZ)) {
	DBG_LOG(ERROR, MOD_DM2, "ERROR: posix_memalign failed: %d %d",
		RD_EXT_BUFSZ, errno);
	return 0;
    }
    AO_fetch_and_add(&total_mem_alloc, RD_EXT_BUFSZ);
    if ((cfd = open(contpath, O_RDONLY | O_DIRECT)) == -1) {
	DBG_LOG(ERROR, MOD_DM2, "ERROR: Failed to open container (%s): %d",
		contpath, errno);
	return 0;
    }

    stat(contpath, &sb);
    nexts = (sb.st_size - NKN_CONTAINER_HEADER_SZ)/ DEV_BSIZE;
    /* Set secnum, so we skip the container header */
    secnum = NKN_CONTAINER_HEADER_SZ / DEV_BSIZE;
    while (nexts > 0) {
	rdsz = pread(cfd, buf, RD_EXT_BUFSZ, secnum*DEV_BSIZE);
	if (rdsz < 0) {
	    DBG_LOG(ERROR, MOD_DM2, "ERROR: Read from container (%s)"
		    " failed:%d", contpath, rdsz);
	    break;
	}
	if (rdsz == 0) {
	    if (nexts > 0) {
		DBG_LOG(ERROR, MOD_DM2, "ERROR: Read '0' from container (%s)"
			"when more data is expected (%d)", contpath, nexts);
	    }
	    break;
	}
	if (rdsz % DEV_BSIZE != 0) {
	    DBG_LOG(ERROR, MOD_DM2, "ERROR: Read from container (%s) is not an "
		    "integral sector amount: %d", contpath, rdsz);
	}
	/*
	 * - If there is more data than what is referenced in 'nexts', we
	 *   skip that data.
	 * - If there is sector unaligned data at the end, we skip that data
	 *   and print a warning.
	 */
	for (pbuf = buf, dext = (DM2_disk_extent_t *)buf;
	     nexts > 0 && rdsz >= DEV_BSIZE;
	     rdsz -= DEV_BSIZE, nexts--, pbuf += DEV_BSIZE, secnum++,
		 dext = (DM2_disk_extent_t *)pbuf) {

	    /* verify checksum */
	    if (dm2_check_disk_ext(dext, contpath, pbuf, buf, secnum))
		continue;

	    physid = calloc(1, sizeof(uint64_t));
	    AO_fetch_and_add(&total_mem_alloc, sizeof(uint64_t));
	    *(uint64_t *)physid = dext->dext_physid;
	    uri_node = g_hash_table_lookup(uri_hash, dext->dext_basename);
	    if (uri_node) {
		uri_node->e_physid_list =
		    g_list_prepend(uri_node->e_physid_list, physid);
	    } else {
		uri_node = calloc(1, sizeof(evict_uri_t));
		print_mem_debug();
		uri_node->e_uri = strdup(dext->dext_basename);
		AO_fetch_and_add(&total_mem_alloc,
			       strlen(dext->dext_basename)+sizeof(evict_uri_t));
		uri_node->e_physid_list =
		    g_list_prepend(uri_node->e_physid_list, physid);
		g_hash_table_insert(uri_hash, uri_node->e_uri, uri_node);
	    }
	    uri_node->e_size += dext->dext_length;
	}
    }
    close(cfd);
    free(buf);
    AO_fetch_and_add(&total_mem_alloc, -RD_EXT_BUFSZ);
    return uri_hash;
}	/* read_container */


static void
free_physid_node(gpointer data,
		 gpointer user_data __attribute((unused)))
{
    free(data);
    AO_fetch_and_add(&total_mem_alloc, -sizeof(uint64_t));
}	/* free_physid_node */


static int
free_uri_node(gpointer key __attribute((unused)),
	      gpointer value,
	      gpointer user_data __attribute((unused)))
{
    evict_uri_t *node = (evict_uri_t *)value;

    if (!node)
	return 1;

    if (node->e_uri) {
	AO_fetch_and_add(&total_mem_alloc, -strlen(node->e_uri));
	free(node->e_uri);
    }

    g_list_foreach(node->e_physid_list, free_physid_node, NULL);
    g_list_free(node->e_physid_list);
    node->e_uri = NULL;
    AO_fetch_and_add(&total_mem_alloc, -sizeof(*node));
    free(node);
    AO_fetch_and_sub1(&num_evict);
    return 1;
}	/* free_uri_node */


static int
process_inode(const char	*fpath,
	      const struct stat *sb,
	      int		typeflag)
{
    DM2_disk_attr_desc_t *dattrd;
    char		 *slash, *uri, *pbuf;
    int			 del_ok, parent_len, rdsz, fd, urilen, secnum, ret = 0;
    nkn_attr_t		 *dattr, dattr_cp;
    nkn_buffer_t	 *abuf = NULL;
    evict_uri_t		 *evict_obj, *head_obj;
    GHashTable		 *uri_hash;

    /* Since the container's and attribute-2's files are removed on
     * deleting all the extents, we could get a STAT failure */
    if ((typeflag & FTW_NS) == FTW_NS)	// Stat failed
	return 0;
    if ((typeflag & FTW_DNR) == FTW_DNR)// Can not read: shouldn't happen
	assert(0);
    if ((typeflag & FTW_D) == FTW_D)	// Directory
	return 0;
    slash = strrchr(fpath, '/');
    if (slash == NULL)		// Can not find /: shouldn't happen
	assert(0);
    if (*(slash+1) == '\0')	// mount point
	return 0;

    if (strcmp(slash+1, NKN_CONTAINER_ATTR_NAME))// Only look at attribute files
	return 0;

    parent_len = strlen(g_parent_dir);
    //    rel_path = &fpath[parent_len];

    uri_hash = read_container(fpath);
    if (uri_hash == 0) {
	/* If attribute file exists but container file doesn't */
	return 0;
    }

    if ((fd = open(fpath, O_RDONLY | O_DIRECT)) == -1) {
	DBG_LOG(ERROR, MOD_DM2, "ERROR: opening %s: %d", fpath, errno);
	exit(1);
    }

    secnum = 0;
    while ((rdsz = read(fd, g_rbuf, RD_ATTR_BUFSZ)) != 0) {
	if (rdsz == -1) {
	    DBG_LOG(ERROR, MOD_DM2,
		    "ERROR: Read of attribute file (%s) failed: %d",
		    fpath, errno);
	    ret = -errno;
	    goto free_buf;
	}
	if (rdsz % DEV_BSIZE != 0) {
	    DBG_LOG(ERROR, MOD_DM2,
		    "ERROR: [path=%s] Read from attrfile is not an "
		    "integral sector amount: %d", fpath, rdsz);
	    DBG_LOG(ERROR, MOD_DM2,
		    "ERROR: Will attempt to cache as much as possible");
	}
	for (pbuf = g_rbuf, dattrd = (DM2_disk_attr_desc_t *)g_rbuf;
	     rdsz > DEV_BSIZE;
	     rdsz -= DEV_BSIZE, pbuf += DEV_BSIZE, secnum++,
		 dattrd = (DM2_disk_attr_desc_t *)pbuf) {
	    if (dm2_check_attr(dattrd, fpath, pbuf, g_rbuf))
		continue;

	    evict_obj = g_hash_table_lookup(uri_hash, dattrd->dat_basename);
	    if (evict_obj == NULL) {
		DBG_LOG(WARNING, MOD_DM2,
			"WARNING: [path=%s] Can not find base_uri=%s "
			"=> Skipping", fpath, dattrd->dat_basename);
		continue;
	    }
	    del_ok = g_hash_table_steal(uri_hash, dattrd->dat_basename);
	    assert(del_ok);
	    /* free the basename URI and replace it with full path URI */
	    AO_fetch_and_add(&total_mem_alloc, -strlen(evict_obj->e_uri));
	    free(evict_obj->e_uri);
	    evict_obj->e_uri = NULL;

	    urilen = strlen(&fpath[parent_len]) + strlen(dattrd->dat_basename)
		+ 2;
	    uri = malloc(urilen);
	    AO_fetch_and_add(&total_mem_alloc, urilen);
	    strcpy(uri, &fpath[parent_len]);
	    slash = strrchr(uri, '/');
	    *(slash+1) = '\0';
	    strcat(uri, dattrd->dat_basename);
	    rdsz -= DEV_BSIZE;
	    pbuf += DEV_BSIZE;

	    dattr = (nkn_attr_t *)pbuf;
	    /* XXXmiken: verify checksum/magic number */
	    if (dattr->na_magic != NKN_ATTR_MAGIC) {
		DBG_LOG(WARNING, MOD_DM2, "WARNING: [path=%s] Bad magic "
			"number (0x%x) at sector (%d) => Skipping",
			fpath, dattr->na_magic, secnum);
		free(uri);
		AO_fetch_and_add(&total_mem_alloc, -urilen);
		continue;
	    }

	    /* If the object is pinned, we cannot evict */
	    if (dattr->na_flags & NKN_OBJ_CACHE_PIN) {
		DBG_LOG(WARNING, MOD_DM2, "WARNING: [path=%s]"
			"URI [%s] is pinned => Skipping", fpath, uri);
		free(uri);
		AO_fetch_and_add(&total_mem_alloc, -urilen);
		continue;
	    }
	    evict_obj->e_uri = uri;
	    /*
	     * Make a copy because we need to pass back the original value
	     * from disk since that value is what BM has stored and is what
	     * we need to compare within nvsd.
	     */
	    memcpy(&dattr_cp, dattr, sizeof(dattr_cp));
	    my_AM_decay_hotness(&dattr_cp.hotval, g_evict_start_time,
			     g_sync_interval, evict_obj->e_size,
			     g_A, g_B, g_C, g_D, g_E);
	    evict_obj->e_decay_temp = am_decode_hotness(&dattr_cp.hotval);
	    evict_obj->e_orig_hot = dattr->hotval;
	    evict_obj->e_contlen = dattr->content_length;
	    if (dattr->content_length >= (unsigned)g_blocksize) {
		evict_obj->e_num_pages =
		    (roundup(dattr->content_length, g_blocksize) >>
		     g_pagesize_shift);
	    } else {
		evict_obj->e_num_pages =
		    (roundup(dattr->content_length, g_pagesize) >>
		     g_pagesize_shift);
	    }
	    /*
	     * This comparison isn't really meaningful because e_decay_temp
	     * is based on a decay which is only used in this eviction code
	     * and g_temperature_seed comes from within nvsd.  We should be
	     * able to drop the if-check altogether.
	     */
	    if ((signed short)evict_obj->e_decay_temp <= g_temperature_seed) {
		if (g_del_pages > 0) {
		    g_queue_insert_sorted(g_evict_queue, evict_obj,
					  cmp_highest_temp_first, NULL);
		    g_del_pages -= evict_obj->e_num_pages;
		} else {
		    head_obj = (evict_uri_t *)g_queue_peek_head(g_evict_queue);
		    if (evict_obj->e_decay_temp < head_obj->e_decay_temp) {
			g_queue_insert_sorted(g_evict_queue, evict_obj,
					      cmp_highest_temp_first, NULL);
			g_del_pages -= evict_obj->e_num_pages;
			while (g_del_pages < 0) {
			    head_obj = g_queue_pop_head(g_evict_queue);
			    g_del_pages += head_obj->e_num_pages;
			    free_uri_node(NULL, head_obj, NULL);
			}
		    } else {
			free_uri_node(NULL, evict_obj, NULL);
		    }
		}
	    } else {
		free_uri_node(NULL, evict_obj, NULL);
	    }

	    if (rdsz < DM2_MAX_ATTR_DISK_SIZE) {
		/* OK to do this because rdsz should always be a multiple
		 * of 2 sectors (descriptor + nkn_attr_t)
		 */
		DBG_LOG(WARNING, MOD_DM2,
			"WARNING: [path=%s] Lost attribute for %s", fpath, uri);
		ret = -EINVAL;
		goto free_buf;
	    }
	    secnum++;
	    /* XXXmiken: Verify checksum stuff in magic number */
	    dattr->na_magic = NKN_ATTR_MAGIC;
	}
    }
    close(fd);
    g_hash_table_foreach_remove(uri_hash, free_uri_node, NULL);
    g_hash_table_destroy(uri_hash);
    return 0;

 free_buf:
    close(fd);
    g_hash_table_foreach_remove(uri_hash, free_uri_node, NULL);
    g_hash_table_destroy(uri_hash);
    return ret;
}	/* process_inode */


static int
get_geometry(void)
{
    char freeblk_path[80];
    int nbytes;
    char *buf;
    dm2_bitmap_header_t	*bmh;
    int fd;

    snprintf(freeblk_path, 80, "%s/%s", g_parent_dir, DM2_BITMAP_FNAME);
    if ((fd = open(freeblk_path, O_DIRECT | O_RDWR, 0644)) < 0) {
	DBG_LOG(ERROR, MOD_DM2,
		"ERROR: Open %s failed: %d", freeblk_path, errno);
	return -errno;
    }
    buf = alloca(2* 4096);
    buf = (char *)roundup(((uint64_t)buf), 4096);
    if ((nbytes = read(fd, buf, 4096)) == -1) {
	DBG_LOG(ERROR, MOD_DM2, "ERROR: read error (%s)[%d,0x%lx]: %d",
		freeblk_path, fd, (uint64_t)buf, errno);
	close(fd);
	return -errno;
    }
    if (nbytes != 4096) {
	DBG_LOG(ERROR, MOD_DM2,
		"ERROR: Short read for %s: expected 4096 bytes, read %d bytes",
		freeblk_path, nbytes);
	close(fd);
	return -errno;
    }
    bmh = (dm2_bitmap_header_t *)buf;
    g_blocksize = bmh->u.bmh_disk_blocksize;
    g_pagesize = bmh->u.bmh_disk_pagesize;
    close(fd);
    return 0;
}	/* get_geometry */


#if 0
static inline guint
g_int64_hash(gconstpointer v)
{
    const guint32 w1 = ((*(const uint64_t *)v) & 0xffffffff);
    const guint32 w2 = ((*(const uint64_t *)v) & 0xffffffff00000000) >> 32;

    return (w1 ^ w2);
}


static inline gboolean
g_int64_equal(gconstpointer v1,
	      gconstpointer v2)
{
    return *((const uint64_t *)v1) == *((const uint64_t *)v2);
}
#endif


static void
free_hash_node(gpointer key __attribute((unused)),
	       gpointer value,
	       gpointer user_data __attribute((unused)))
{
    free(value);
    AO_fetch_and_add(&total_mem_alloc, -sizeof(uint64_t));
}	/* free_hash_node */


static void
read_config_file(void)
{
    struct stat sb;
    FILE *fp;
    int num;
    float l_A, l_B, l_C, l_D, l_E;

    if (stat(CONF_FILE, &sb)) {
	DBG_LOG(SEVERE, MOD_DM2, "Cannot find %s", CONF_FILE);
	return;
    }
    if ((fp = fopen(CONF_FILE, "r")) == NULL) {
	DBG_LOG(SEVERE, MOD_DM2, "Cannot open %s", CONF_FILE);
	return;
    }
    num = fscanf(fp, "%f %f %f %f %f", &l_A, &l_B, &l_C, &l_D, &l_E);
    if (num != 5) {
	DBG_LOG(SEVERE, MOD_DM2, "Config file %s bad format: arguments found= %d",
		CONF_FILE, num);
	return;
    }
    DBG_LOG(SEVERE, MOD_DM2, "Args: A=%f, B=%f, C=%f, D=%f, E=%f",
	    l_A, l_B, l_C, l_D, l_E);
    g_A = l_A;
    g_B = l_B;
    g_C = l_C;
    g_D = l_D;
    g_E = l_E;

    fclose(fp);
    return;
}	/* read_config_file */


/*
 * Program: cache_provider_evict
 *
 */
int
main(int  argc,
     char *argv[])
{
    char *progname;
    GList *e_list, *physid_list;
    evict_uri_t *e_uri;
    GHashTable *physid_hash;
    uint64_t physid;
    int ch, i, ret, duplicate, del_blocks, cnt;
    int print_once, more_physids = 1;

    /* Kill child when the parent is dead */
    prctl(PR_SET_PDEATHSIG, SIGKILL);

    read_config_file();

    /* Grab current time once */
    if((ret = time(&g_evict_start_time)) == -1) {
	DBG_LOG(MSG, MOD_DM2, "Error getting time");
	exit(1);
    }

    atexit(normal_exit);
    log_thread_start();
    sleep (1); // Need to pause as thread/socket has to get created 
    DBG_LOG(MSG, MOD_DM2, "Starting : %s ...", argv[0]);
    progname = argv[0];
    if (argc < 2)
	usage(progname);

    while ((ch = getopt(argc, argv, "Dd:p:s:t:")) != -1) {
	switch (ch) {
	    case 'D':
		DEBUG = 1;
		break;
	    case 'd':
		g_del_blocks = atol(optarg);
		break;
	    case 'p':
		g_provider = atoi(optarg);
		break;
	    case 's':
		g_hotness_seed = atol(optarg);
		g_temperature_seed = am_decode_hotness(&g_hotness_seed);
		break;
	    case 't':
		/* sync_interval is in seconds */
		g_sync_interval = atol(optarg);
		break;
	    default:
		usage(progname);
		break;
	}
    }

    if (optind >= argc)
	usage(progname);

    if (g_provider == -1 || g_hotness_seed == (unsigned)-1 ||
	g_del_blocks == -1 || g_sync_interval == -1 || g_del_blocks == 0) {
	usage(progname);
    }


    g_evict_queue = g_queue_new();
    if (posix_memalign((void *)&g_rbuf, DEV_BSIZE, RD_ATTR_BUFSZ)) {
	DBG_LOG(ERROR, MOD_DM2, "ERROR: posix_memalign failed: %d %d",
		RD_ATTR_BUFSZ, errno);
	return -ENOMEM;
    }
    AO_fetch_and_add(&total_mem_alloc, RD_EXT_BUFSZ);

    /*
     * Loop thru top level cache mount points and call tree walking routine.
     * Each inode processed should only add to a global list in terms of
     * memory usage.  The hash tables created at each inode should be
     * completely freed.
     */
    for (i = optind; i < argc; i++) {
	g_parent_dir = argv[i];
	if ((ret = get_geometry()) == 0) {
	    g_blocksize_shift = dm2_bitshift32(g_blocksize);
	    g_pagesize_shift = dm2_bitshift32(g_pagesize);
	    g_del_pages = g_del_blocks * (g_blocksize / g_pagesize);
	    ftw(g_parent_dir, process_inode, 10);
	}
    }
    /*
     * Given a candidate list of URIs to evict, mark the URIs whose physids
     * are completely overlapping with previous list entry URIs.  This may
     * not be optimal, but the check is fast.
     *
     * The memory isn't freed for found duplicates because it is already
     * allocated and we are allocating little additional memory at this point.
     */
#if 0
    physid_hash = g_hash_table_new(g_int64_hash, g_int64_equal);
    e_list = g_evict_queue->tail;
    for (cnt = g_evict_queue->length; cnt > 0; cnt--, e_list = e_list->prev) {
	e_uri = (evict_uri_t *)e_list->data;
	duplicate = 1;
	physid_list = e_uri->e_physid_list;
	for (; physid_list; physid_list = physid_list->next) {
	    physid = *(uint64_t *)physid_list->data;
	    if (g_hash_table_lookup(physid_hash, &physid) == 0) {
		uint64_t *physid_copy = calloc(1, sizeof(uint64_t));
		AO_fetch_and_add(&total_mem_alloc, sizeof(uint64_t));
		duplicate = 0;
		*physid_copy = physid;
		g_hash_table_insert(physid_hash, physid_copy, physid_copy);
	    }
	}
	if (duplicate)
	    e_uri->e_dup = 1;
    }
#endif

    /*
     * Loop through list and print URIs with non-duplicate physids.
     * Even though we have a list with at most one URI per physid,
     * the original accounting was done over pages.  Since we can only
     * evict blocks, we know that each one of these URIs will produce a
     * free block.  Hence, we only evict the first 'del_blocks' entries
     * from the list.
     *
     * This code makes no attempt to print only one URI per block because
     * nvsd supports eviction thresholds where we may delete only a single
     * object.
     */
    del_blocks = g_del_blocks;
    e_list = g_evict_queue->tail;
    physid_hash = g_hash_table_new(g_int64_hash, g_int64_equal);
    for (cnt = g_evict_queue->length; cnt > 0; cnt--, e_list = e_list->prev) {
	e_uri = (evict_uri_t *)e_list->data;

	physid_list = e_uri->e_physid_list;
	print_once = 1;
	for (; physid_list; physid_list = physid_list->next) {
	    physid = *(uint64_t *)physid_list->data;
	    if (g_hash_table_lookup(physid_hash, &physid) == 0) {
		if (more_physids) {
		    uint64_t *physid_copy = calloc(1, sizeof(uint64_t));
		    AO_fetch_and_add(&total_mem_alloc, sizeof(uint64_t));
		    *physid_copy = physid;
		    g_hash_table_insert(physid_hash, physid_copy, physid_copy);
		    del_blocks--;
		    if (print_once) {
			/* Print only one line per large URI */
			printf("%s\t%ld\n", e_uri->e_uri, e_uri->e_orig_hot);
			print_once = 0;
		    }
		}
	    } else if (print_once) {
		/* We could have found another URI in the same block which
		 * we are targetting for eviction. */
		printf("%s\t%ld\n", e_uri->e_uri, e_uri->e_orig_hot);
		print_once = 0;
	    }
	}
	/* Once we go negative, we don't want to evict any more blocks */
	if (del_blocks <= 0)
	    more_physids = 0;
    }
 done:

    /* free up all memory */
    while (!g_queue_is_empty(g_evict_queue)) {
	e_uri = g_queue_pop_head(g_evict_queue);
	free_uri_node(NULL, e_uri, NULL);
    }
    g_queue_free(g_evict_queue);
    g_hash_table_foreach(physid_hash, free_hash_node, NULL);
    g_hash_table_destroy(physid_hash);

    if (DEBUG) {
	fprintf(stderr, "final mem: %ld\n", total_mem_alloc);
	fprintf(stderr, "final evict: %ld\n", num_evict);
    }

    return 0;
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

/*
 *
 *
 * The following code is needed, so we don't need to link with libnkn_memalloc
 *
 *
 */
void *nkn_malloc_type(size_t size,
		      nkn_obj_type_t type __attribute((unused)))
{
    return malloc(size);
}

void *nkn_realloc_type(void *ptr,
		       size_t size,
		       nkn_obj_type_t type __attribute((unused)))
{
    return realloc(ptr, size);
}

void *nkn_calloc_type(size_t n,
		      size_t size,
		      nkn_obj_type_t type __attribute((unused)))
{
    return calloc(n, size);
}

void *nkn_memalign_type(size_t align,
			size_t s,
			nkn_obj_type_t type __attribute((unused)))
{
    return memalign(align, s);
}

void *nkn_valloc_type(size_t size,
		      nkn_obj_type_t type __attribute((unused)))
{
    return valloc(size);
}

void *nkn_pvalloc_type(size_t size,
		       nkn_obj_type_t type __attribute((unused)))
{
    return pvalloc(size);
}

int nkn_posix_memalign_type(void **r,
			    size_t align,
			    size_t size,
                            nkn_obj_type_t type __attribute((unused)))
{
    return posix_memalign(r, align, size);
}

char *nkn_strdup_type(const char *s,
		      nkn_obj_type_t type __attribute((unused)))
{
    return strdup(s);
}
