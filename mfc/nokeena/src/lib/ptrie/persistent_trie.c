/*
 * persistent_trie.c -- Persistent Trie interface built upon cprops trie.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <assert.h>
#include <poll.h>
#include <stdarg.h>
#include <pthread.h>

#include "ptrie/persistent_trie.h"

/*
 * Global data
 */
AO_t init_complete;
static int ptrie_def_log_level = PT_LOGL_MSG;
static int *ptrie_log_level;
static int (*ptrie_logfunc)(ptrie_log_level_t level, const char *fmt, ...)
		__attribute__ ((format (printf, 2, 3)));
static void *(*ptrie_malloc)(size_t size);
static void *(*ptrie_calloc)(size_t nmemb, size_t size);
static void *(*ptrie_realloc)(void *ptr, size_t size);
static void (*ptrie_free)(void *ptr);

/*
 *******************************************************************************
 * Macro definitions
 *******************************************************************************
 */
#define MALLOC(_size) (*ptrie_malloc)((_size))
#define CALLOC(_nemb, _size) (*ptrie_calloc)((_nemb), (_size))
#define REALLOC(_ptr, _size) (*ptrie_realloc)((_ptr), (_size))
#define FREE(_ptr) (*ptrie_free)((_ptr))

#define LOGMSG(fmt, ...) { \
    if (PT_LOGL_MSG <= *ptrie_log_level) { \
    	(*ptrie_logfunc)(PT_LOGL_MSG, \
		     	"[%s:%d] "fmt"\n", \
			__FUNCTION__, __LINE__, ##__VA_ARGS__); \
    } \
}

#define LOGSEV(fmt, ...) { \
    if (PT_LOGL_SEVERE <= *ptrie_log_level) { \
    	(*ptrie_logfunc)(PT_LOGL_SEVERE, \
		     	 "[%s:%d] "fmt"\n", \
			 __FUNCTION__, __LINE__, ##__VA_ARGS__); \
    } \
}

#define UNUSED_ARGUMENT(x) (void)x

/*
 *******************************************************************************
 * Internal cp_trie interfaces
 *******************************************************************************
 */
static void *
trie_copy_func(void *lf_node)
{
    node_data_t *nd = (node_data_t *) lf_node;
    ptrie_context_t *ctx = nd->ctx;
    node_data_t *new_nd;

    if (nd) {
    	new_nd = (node_data_t *)MALLOC(sizeof(node_data_t));
	if (ctx->copy_app_data) {
	    new_nd->dh = nd->dh;
	    (*ctx->copy_app_data)(&nd->ad, &new_nd->ad);
	    new_nd->ctx = ctx;
	} else {
	    *new_nd = *nd;
	}
    } else {
    	new_nd = 0;
    }
    return new_nd;
}

static void 
trie_destructor_func(void *lf_node)
{
    node_data_t *nd = (node_data_t *) lf_node;
    ptrie_context_t *ctx = nd->ctx;

    if (nd) {
    	if (ctx->destruct_app_data) {
	    (*ctx->destruct_app_data)(&nd->ad);
	}
    	FREE(nd);
    }
    return;
}

static cp_trie *
create_cp_trie(void)
{
    return cp_trie_create_trie((COLLECTION_MODE_NOSYNC|
			       	COLLECTION_MODE_COPY|COLLECTION_MODE_DEEP),
			       	trie_copy_func, trie_destructor_func);
}

static int 
reset_cp_trie(trie_data_t *td)
{
    cp_trie_destroy(td->trie);
    td->trie = create_cp_trie();
    if (!td->trie) {
    	LOGMSG("create_cp_trie() returns NULL");
    	return 1;
    }
    return 0;
}

/*
 *******************************************************************************
 * Internal log callback function 
 *******************************************************************************
 */
static const char *log_level_txt[PT_LOGL_MAX] = {
    "Alarm",
    "Severe",
    "Error",
    "Warning",
    "Message"
};

static int
internal_logfunc(ptrie_log_level_t level, const char *fmt, ...)
		__attribute__ ((format (printf, 2, 3)));

static int
internal_logfunc(ptrie_log_level_t level, const char *fmt, ...)
{
     va_list ap;

     va_start(ap, fmt);
     printf("[%s]:", log_level_txt[level]);
     return vprintf(fmt, ap);
}

/*
 *******************************************************************************
 * File node interfaces
 *******************************************************************************
 */
static int 
write_file_node_data(int fd, off_t loff, file_node_data_t *fnd)
{
    off_t off;
    off_t foff;
    size_t bytes;
    ssize_t bytes_written;

    foff = LOFF2FOFF(loff);
    off = lseek(fd, foff, SEEK_SET);
    if (off == -1) {
    	LOGMSG("lseek(%d, %ld) failed, errno=%d", fd, foff, errno);
    	return 1;
    }

    bytes = sizeof(*fnd);
    bytes_written = write(fd, (void *)fnd, bytes);
    if (bytes_written != (ssize_t)bytes) {
    	LOGMSG("write(%d) short xfer, bytes_written=%ld bytes=%ld",
	       fd, bytes_written, bytes);
    	return 2;
    }
    return 0;
}

static int 
read_node_data(int fd, off_t loff, node_data_t *nd)
{
    off_t off;
    off_t foff;
    ssize_t bytes_read;

    foff = LOFF2FOFF(loff);
    off = lseek(fd, foff, SEEK_SET);
    if (off == -1) {
    	LOGMSG("lseek(%d, %ld) failed, errno=%d", fd, foff, errno);
    	return 1;
    }

    bytes_read = read(fd, (void *)nd, sizeof(*nd));
    if (bytes_read != sizeof(*nd)) {
    	LOGMSG("read(%d) foff=%ld short xfer, bytes_read=%ld bytes=%ld",
	       fd, foff, bytes_read, sizeof(*nd));
    	return 2;
    }
    return 0;
}

static int 
write_node_data(int fd, off_t loff, const node_data_t *nd)
{
    off_t off;
    off_t foff;
    size_t bytes;
    ssize_t bytes_written;

    foff = LOFF2FOFF(loff);
    off = lseek(fd, foff, SEEK_SET);
    if (off == -1) {
    	LOGMSG("lseek(%d, %ld) failed, errno=%d", fd, foff, errno);
    	return 1;
    }

    bytes = sizeof(*nd);
    bytes_written = write(fd, (void *)nd, bytes);
    if (bytes_written != (ssize_t)bytes) {
    	LOGMSG("write(%d) short xfer, bytes_written=%ld bytes=%ld",
	       fd, bytes_written, bytes);
    	return 2;
    }
    return 0;
}

/*
 *******************************************************************************
 * File header interfaces
 *******************************************************************************
 */
static int 
write_file_header(int fd, int copy, file_header_t *fh)
{
    off_t foff;
    off_t off;
    size_t bytes;
    ssize_t bytes_written;

    foff = (copy == 1) ? FHDR_1_FOFF : FHDR_2_FOFF;
    off = lseek(fd, foff, SEEK_SET);
    if (off == -1) {
    	LOGMSG("lseek(%d, %ld) copy=%d failed, errno=%d", fd, foff, 
	       copy, errno);
    	return 1;
    }

    bytes = sizeof(*fh);
    bytes_written = write(fd, (void *)fh, bytes);
    if (bytes_written != (ssize_t)bytes) {
    	LOGMSG("write(%d) short xfer, bytes_written=%ld bytes=%ld",
	       fd, bytes_written, bytes);
    	return 2;
    }
    fsync(fd);
    return 0;
}

static int 
read_file_header(int fd, int copy, file_header_t *fh)
{
    off_t foff;
    off_t off;
    size_t bytes;
    ssize_t bytes_read;

    foff = (copy == 1) ? FHDR_1_FOFF : FHDR_2_FOFF;
    off = lseek(fd, foff, SEEK_SET);
    if (off == -1) {
    	LOGMSG("lseek(%d, %ld) copy=%d failed, errno=%d", fd, 
	       foff, copy, errno);
    	return 1;
    }

    bytes = sizeof(*fh);
    bytes_read = read(fd, (void *)fh, bytes);
    if (bytes_read != (ssize_t)bytes) {
    	LOGMSG("read(%d) short xfer,  bytes_read=%ld bytes=%ld",
	       fd, bytes_read, bytes);
    	return 2;
    }
    return 0;
}

/*
 *******************************************************************************
 * Checkpoint file recovery interfaces
 *******************************************************************************
 */
static int 
restore_trie_from_ckpt(int fd, ptrie_context_t *ctx,
		       cp_trie *trie1, cp_trie *trie2,
		       ckpt_file_bmap_t *bmp1, ckpt_file_bmap_t *bmp2,
		       long *pmax_fnode_incarnation)
{
    struct stat sbuf;
    char *fmap = 0;
    file_node_data_t *fn;
    node_data_t nd;
    int ret;
    int rv = 0;
    long free_index;
    long max_fnode_incarnation = 0;

    LOGMSG("ctx=%p fd=%d trie1=%p trie2=%p bmap1=%p bmap2=%p", 
	   ctx, fd, trie1, trie2, bmp1, bmp2);

    if (bmp1) {
    	bmp1->entries = 0;
    	memset(bmp1->map, 0, bmp1->mapsize);
    }

    if (bmp2) {
    	bmp2->entries = 0;
    	memset(bmp2->map, 0, bmp2->mapsize);
    }

    ////////////////////////////////////////////////////////////////////////////
    while (1) { // Begin while
    ////////////////////////////////////////////////////////////////////////////

    if ((ret = fstat(fd, &sbuf)) != 0) {
    	LOGMSG("fstat(fd=%d) failed, ret=%d errno=%d", fd, ret, errno);
	rv = 1;
	break;
    }

    fmap = mmap((void *) 0, sbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (fmap == MAP_FAILED) {
    	LOGMSG("mmap(fd=%d) failed, errno=%d", fd, errno);
	rv = 2;
	break;
    }

    for (fn = (file_node_data_t *)(fmap + LOFF2FOFF(0)); 
    	 (char *)fn < &fmap[sbuf.st_size]; fn++) {
    	if (fn->nd.dh.magicno == DH_MAGICNO) {
	    if (fn->nd.dh.incarnation > max_fnode_incarnation) {
	    	max_fnode_incarnation = fn->nd.dh.incarnation;
	    }
	    if (trie1) {
	    	nd = fn->nd;
		nd.ctx = ctx;
	    	ret = cp_trie_add(trie1, fn->key, &nd);
		if (ret) {
		    LOGMSG("cp_trie_add(trie=%p) failed, ret=%d", trie1, ret);
		    rv = 3;
		    break;
		}
	    }
	    if (trie2) {
	    	nd = fn->nd;
		nd.ctx = ctx;
	    	ret = cp_trie_add(trie2, fn->key, &nd);
		if (ret) {
		    LOGMSG("cp_trie_add(trie=%p) failed, ret=%d", trie2, ret);
		    rv = 4;
		    break;
		}
	    }
	} else if (fn->nd.dh.magicno == DH_MAGICNO_FREE) { // Deleted 
	    free_index = (long)
			(((char *)fn - (fmap + LOFF2FOFF(0))) / sizeof(*fn));
	    if (free_index >= MAX_TRIE_ENTRIES) {
		LOGMSG("Out of range free_index=%ld %d", 
		       free_index, MAX_TRIE_ENTRIES);
		rv = 5;
		break;
	    }
	    if (bmp1) {
		setbit(bmp1->map, free_index);
		bmp1->entries++;
	    }

	    if (bmp2) {
		setbit(bmp2->map, free_index);
		bmp2->entries++;
	    }
	} else {
	    LOGMSG("Bad magicno=0x%lx fmap=%p fn=%p", 
		   fn->nd.dh.magicno, fmap, fn);
	    rv = 6;
	    break;
	}
    }
    break;

    ////////////////////////////////////////////////////////////////////////////
    } // End while
    ////////////////////////////////////////////////////////////////////////////

    if (fmap) {
    	munmap(fmap, sbuf.st_size);
    }
    if (pmax_fnode_incarnation) {
    	*pmax_fnode_incarnation = max_fnode_incarnation;
    }
    return rv;
}

static int 
init_trie_ckpt_file(int fd, file_header_t *fh1, file_header_t *fh2)
{
    int ret;

    ret = ftruncate(fd, 0);
    if (ret) {
	LOGMSG("ftruncate(fd=%d) failed, ret=%d errno=%d", fd, ret, errno);
	return 1;
    }

    memset((void *)fh1, 0, sizeof(*fh1));
    fh1->u.hdr.version = FH_VERSION;
    fh1->u.hdr.magicno = FH_MAGICNO;
    fh1->u.hdr.seqno = 1;

    ret = write_file_header(fd, 1, fh1);
    if (ret) {
    	LOGMSG("write_file_header(fd=%d) failed, ret=%d", fd, ret);
	return 2;
    }
    *fh2 = *fh1;
    ret = write_file_header(fd, 2, fh2);
    if (ret) {
    	LOGMSG("write_file_header(fd=%d) failed, ret=%d", fd, ret);
	return 3;
    }
    return 0;
}

static int 
open_trie_ckpt_file(const char *name, int cur_fd,
		    file_header_t *fh1, file_header_t *fh2,
		    int *pfd, off_t *pnxt_free_loff)
{
    int fd = -1;
    struct stat sbuf;
    int ret;
    int rv = 0;

    ////////////////////////////////////////////////////////////////////////////
    while (1) { // Begin while 
    ////////////////////////////////////////////////////////////////////////////

    if (name) {
    	fd = open(name, O_CREAT|O_RDWR, 0600);
    	if (fd < 0) {
	    LOGMSG("open(%s) failed", name);
	    rv = 1;
	    break;
    	}
	LOGMSG("fname=%s fd=%d", name, fd);
    } else {
    	fd = cur_fd;
	LOGMSG("fname=NULL fd=%d", fd);
    }

    if ((ret = fstat(fd, &sbuf)) != 0) {
	LOGMSG("fstat(%s) failed, errno=%d", name, errno);
    	close(fd);
    	rv = 2;
	break;
    }

    if (!sbuf.st_size) {
    	// Init case
	ret = init_trie_ckpt_file(fd, fh1, fh2);
    	if (ret) {
	    LOGMSG("init_trie_ckpt_file(name=%s fd=%d) failed, ret=%d", 
	    	   name, fd, ret);
	    rv = 3;
	    break;
	}
	*pnxt_free_loff = 0;
    } else {
    	// Headers exist
    	ret = read_file_header(fd, 1, fh1);
	if (ret) {
	    LOGMSG("read_file_header(name=%s fd=%d), ret=%d", name, fd, ret);
	    rv = 4;
	    break;
	}
    	ret = read_file_header(fd, 2, fh2);
	if (ret) {
	    LOGMSG("read_file_header(name=%s fd=%d), ret=%d", name, fd, ret);
	    rv = 5;
	    break;
	}
	if ((fh1->u.hdr.version != FH_VERSION) ||
	    (fh2->u.hdr.version != FH_VERSION) ||
	    (fh1->u.hdr.magicno != FH_MAGICNO) ||
	    (fh2->u.hdr.magicno != FH_MAGICNO) ||
	    (fh1->u.hdr.seqno != fh2->u.hdr.seqno) ||
	    (fh1->u.hdr.filesize != fh2->u.hdr.filesize) ||
	    (sbuf.st_size > (long)MAX_CKPT_FILESIZE) ||
	    ((sbuf.st_size != META_HDRSIZE) && 
	     (fh1->u.hdr.filesize != sbuf.st_size)) ||
	    (fh1->u.hdr.ts.tv_sec != fh2->u.hdr.ts.tv_sec) ||
	    (fh1->u.hdr.ts.tv_nsec != fh2->u.hdr.ts.tv_nsec)) {

	    LOGMSG("Inconsistent data, resetting checkpoint file [%s] fd=%d "
	    	   "version-1=0x%x version-2=0x%x "
	    	   "magic-1=%ld magic-2=%ld "
	    	   "seqno-1=%ld seqno-2=%ld "
		   "filesize-1=%ld filesize-2=%ld "
		   "filesize-1=%ld stat.st_size=%ld "
	    	   "tv_sec-1=%ld tv_sec-2=%ld "
	    	   "tv_nsec-1=%ld tv_nsec-2=%ld",
		   name, fd,
		   fh1->u.hdr.version, fh2->u.hdr.version,
		   fh1->u.hdr.magicno, fh2->u.hdr.magicno,
		   fh1->u.hdr.seqno, fh2->u.hdr.seqno,
		   fh1->u.hdr.filesize, fh2->u.hdr.filesize,
		   fh1->u.hdr.filesize, sbuf.st_size,
		   fh1->u.hdr.ts.tv_sec, fh2->u.hdr.ts.tv_sec,
		   fh1->u.hdr.ts.tv_nsec, fh2->u.hdr.ts.tv_nsec);

	    // Inconsistent data, reset checkpoint file
	    ret = init_trie_ckpt_file(fd, fh1, fh2);
	    if (ret) {
	    	LOGMSG("init_trie_ckpt_file(name=%s fd=%d) failed, ret=%d",
		       name, fd, ret);
	    	rv = 6;
		break;
	    }
	    *pnxt_free_loff = 0;
	} else {
	    *pnxt_free_loff = sbuf.st_size - META_HDRSIZE;
	}
    }
    break;

    ////////////////////////////////////////////////////////////////////////////
    } // End while 
    ////////////////////////////////////////////////////////////////////////////

    if (!rv) {
    	*pfd = fd;
    } else {
    	// Error exit
    	if (fd >= 0) {
	    close(fd);
	}
    }
    return rv;
}

static int 
close_trie_ckpt_file(ckpt_file_data_t *ckfd)
{
    if (ckfd->file_fd >= 0) {
    	close(ckfd->file_fd);
    	ckfd->file_fd = -1;
    }
    return 0;
}

static int 
copy_f1_to_f2(int from_fd, int to_fd)
{
    char buf[1024 * 1024];
    ssize_t bytes_read;
    ssize_t bytes_written;
    off_t off;
    int rv = 0;
    int ret;

    LOGMSG("from_fd=%d to_fd=%d", from_fd, to_fd);

    off = lseek(from_fd, 0, SEEK_SET);
    if (off == -1) {
    	LOGMSG("lseek(%d, 0) failed, errno=%d", from_fd, errno);
    	return 1;
    }

    ret = ftruncate(to_fd, 0);
    if (ret) {
	LOGMSG("ftruncate(fd=%d) failed, ret=%d errno=%d", to_fd, ret, errno);
	return 2;
    }

    off = lseek(to_fd, 0, SEEK_SET);
    if (off == -1) {
    	LOGMSG("lseek(%d, 0) failed, errno=%d", to_fd, errno);
    	return 3;
    }

    while (1) {
    	bytes_read = read(from_fd, (void *)buf, sizeof(buf));
    	if (bytes_read > 0) {
	    bytes_written = write(to_fd, (void *)buf, bytes_read);
	    if (bytes_written != bytes_read) {
    		LOGMSG("write(%d) short xfer, bytes_written=%ld bytes_read=%ld",
		       to_fd, bytes_written, bytes_read);
    		rv = 4;
		break;
	    }
	} else if (bytes_read == 0) { // EOF
	    fsync(to_fd);
	    break;
	} else {
	    LOGMSG("read(%d) error, errno=%d", from_fd, errno);
	    rv = 5;
	    break;
	}
    }
    return rv;
}

/*
 *******************************************************************************
 * Checkpoint file log interfaces
 *******************************************************************************
 */
#define ISSET_FNODE_FREEMAP(_cfd, _loff) \
	isset((_cfd)->freemap.map, ((_loff) / sizeof(file_node_data_t)))

#define ISCLR_FNODE_FREEMAP(_cfd, _loff) \
	isclr((_cfd)->freemap.map, ((_loff) / sizeof(file_node_data_t)))

#define SET_FNODE_FREEMAP(_cfd, _loff) { \
	(_cfd)->freemap.entries++; \
	setbit((_cfd)->freemap.map, ((_loff) / sizeof(file_node_data_t))); \
}

#define CLR_FNODE_FREEMAP(_cfd, _loff) { \
	(_cfd)->freemap.entries--; \
	clrbit((_cfd)->freemap.map, ((_loff) / sizeof(file_node_data_t))); \
}

static int 
find_setbit(char *list, int size_in_bytes)
{
    int num_longs;
    int num_chars;
    int nl;
    long *pl;
    int nbit;
    int nc;
    char *pc;

    num_longs = size_in_bytes / sizeof(long);
    num_chars = size_in_bytes % sizeof(long);

    for (nl = 0, pl = (long *)list; nl < num_longs; nl++) {
	if (pl[nl]) {
	    nbit = ffsl(pl[nl]) - 1;
	    return ((nl * sizeof(long)) * NBBY) + nbit;
	}
    }

    for (nc = 0, pc = (char *)&pl[nl]; nc < num_chars; nc++) {
    	if (pc[nc]) {
	    for (nbit = 0; nbit < NBBY; nbit++) {
	    	if (isset(&pc[nc], nbit)) {
		    return ((nl * sizeof(long)) * NBBY) + (nc * NBBY) + nbit;
		}
	    }
	}
    }
    return -1;
}

static off_t 
alloc_ckpt_file_data_t(ckpt_file_data_t *cfd)
{
    off_t loff;

    if (cfd->freemap.entries) {
    	loff = find_setbit(cfd->freemap.map, cfd->freemap.mapsize);
	if (loff >= 0) {
	    loff = loff * sizeof(file_node_data_t);
	    CLR_FNODE_FREEMAP(cfd, loff);
	    return loff;
	} else {
	    LOGMSG("No bits found, freemap.entries=%d", cfd->freemap.entries);
	    return -1;
	}
    } else if (cfd->next_entry_loff < (off_t)(MAX_CKPT_FILESIZE-META_HDRSIZE)) {
    	loff = cfd->next_entry_loff;
	cfd->next_entry_loff += sizeof(file_node_data_t);
	return loff;
    } else {
    	LOGMSG("Out of space, fd=%d", cfd->file_fd);
    	return -1;
    }
}

static int 
init_fnode(ckpt_file_data_t *cfd, file_node_data_t *fnd, const char *key,
	   app_data_t *ad, ptrie_context_t *ctx)
{
    int keysize;

    keysize = strlen(key);
    strncpy(fnd->key, key, sizeof(fnd->key));
    fnd->key_null = '\0';

    fnd->nd.dh.magicno = DH_MAGICNO;
    fnd->nd.dh.loff = alloc_ckpt_file_data_t(cfd);
    if (fnd->nd.dh.loff == -1) {
    	LOGMSG("alloc_ckpt_file_data_t() failed, loff=-1");
    	return 1;
    }
    fnd->nd.dh.key_strlen = keysize;
    fnd->nd.dh.pad = 0;
    fnd->nd.dh.incarnation = ctx->fnode_incarnation++;
    fnd->nd.dh.flags = 0;
    memset(&fnd->nd.ad, 0, sizeof(fnd->nd.ad));
    if (ctx->copy_app_data) {
    	(*ctx->copy_app_data)(ad, &fnd->nd.ad);
    } else {
    	fnd->nd.ad = *ad;
    }
    fnd->nd.ctx = ctx;

    return 0;
}

static int 
write_new_fnode(ckpt_file_data_t *cfd, file_node_data_t *fnd)
{
    int ret;
    node_data_t tmp_nd;

    // Adjust allocation data
    if (fnd->nd.dh.loff < cfd->next_entry_loff) {
    	// Reclaim entry
	if (cfd->freemap.entries) {
	    if (ISCLR_FNODE_FREEMAP(cfd, fnd->nd.dh.loff)) {
	    	// Bitmap inconsistency
		LOGMSG("write_new_fnode(), bitmap wrong loff=0x%lx",
		       fnd->nd.dh.loff);
		return 1;
	    }
	    CLR_FNODE_FREEMAP(cfd, fnd->nd.dh.loff);
	} else {
	    LOGMSG("free bitmap inconsistency cfd=%p fnd=%p "
	    	   "loff=0x%lx next_loff=0x%lx", cfd, fnd, 
		   fnd->nd.dh.loff,  cfd->next_entry_loff);
	    return 2;
	}

	// Verify existing entry is free
	ret = read_node_data(cfd->file_fd, fnd->nd.dh.loff, &tmp_nd);
	if (!ret) {
	    if (tmp_nd.dh.magicno != DH_MAGICNO_FREE) {
	    	LOGMSG("dh.magicno != DH_MAGICNO_FREE, fd=%d loff=0x%lx "
		       "magicno=0x%lx", cfd->file_fd, fnd->nd.dh.loff, 
		       tmp_nd.dh.magicno);
		return 3;
	    }
	} else {
	    LOGMSG("read_node_data(fd=%d, loff=0x%lx) failed, ret=%d",
	    	   cfd->file_fd, fnd->nd.dh.loff, ret);
	    return 4;
	}

    } else {
    	cfd->next_entry_loff += sizeof(file_node_data_t);
    }

    ret = write_file_node_data(cfd->file_fd, fnd->nd.dh.loff, fnd);
    if (ret) {
	LOGMSG("write_file_node_data(fd=%d) loff=%ld failed, ret=%d",
	       cfd->file_fd, fnd->nd.dh.loff, ret);
	return 5;
    }
    return 0;
}

static int 
delete_fnode(ckpt_file_data_t *cfd, file_node_data_t *fnd)
{
    off_t loff;
    int ret;

    fnd->nd.dh.magicno = DH_MAGICNO_FREE;
    loff = fnd->nd.dh.loff;

    ret = write_node_data(cfd->file_fd, loff, &fnd->nd);
    if (ret) {
    	LOGMSG("write_node_data(fd=%d) loff=%ld failed, ret=%d",
	       cfd->file_fd, loff, ret);
	return 1;
    }

    if (ISSET_FNODE_FREEMAP(cfd, loff)) {
    	LOGMSG("free bitmap inconsistency cfd=%p fnd=%p loff=0x%lx",
	       cfd, fnd, loff);
	return 2;
    }
    SET_FNODE_FREEMAP(cfd, loff);

    return 0;
}


static int 
copy_ckpt_file_alloc_data(ptrie_context_t *ctx)
{
    ctx->bi_next_entry_loff = ctx->shdw_ckpt_file->next_entry_loff;
    ctx->bi_ckpt_file_bmap.entries = ctx->shdw_ckpt_file->freemap.entries;
    memcpy(ctx->bi_ckpt_file_bmap.map, ctx->shdw_ckpt_file->freemap.map,
    	   ctx->shdw_ckpt_file->freemap.mapsize);
    return 0;
}

static int 
restore_ckpt_file_alloc_data(ptrie_context_t *ctx)
{
    char *pmap;

    ctx->shdw_ckpt_file->next_entry_loff = ctx->bi_next_entry_loff;
    ctx->shdw_ckpt_file->freemap.entries = ctx->bi_ckpt_file_bmap.entries;

    pmap = ctx->shdw_ckpt_file->freemap.map;
    ctx->shdw_ckpt_file->freemap.map = ctx->bi_ckpt_file_bmap.map;
    ctx->bi_ckpt_file_bmap.map = pmap;

    return 0;
}

static int
reset_ckpt_file_alloc_data(ckpt_file_data_t *ckpd)
{
    ckpd->next_entry_loff = 0;
    ckpd->freemap.entries = 0;
    memset(ckpd->freemap.map, 0, ckpd->freemap.mapsize);
    return 0;
}

/*
 *******************************************************************************
 * Log apply interfaces
 *******************************************************************************
 */
static int 
apply_log2ckpt_file(ckpt_file_data_t *ckpd, trie_xaction_log_t *log)
{
    trie_xaction_entry_t *te;
    int n;
    int ret;
    int rv = 0;
    node_data_t tmp_nd;

    for (n = 0; n < log->entries; n++) {
    	te = &log->en[n];

	switch(te->type) {
	case PT_XC_ADD:
	    ret = write_new_fnode(ckpd, &te->fn);
	    if (ret) {
	    	LOGMSG("write_new_fnode() failed, type=%d ret=%d", 
		       te->type, ret);
		rv = 1;
	    }
	    break;

	case PT_XC_UPDATE:
	    ret = read_node_data(ckpd->file_fd, te->fn.nd.dh.loff, &tmp_nd);
	    if (!ret) {
	    	if ((tmp_nd.dh.magicno != DH_MAGICNO) ||
		    (tmp_nd.dh.loff != te->fn.nd.dh.loff) ||
		    (tmp_nd.dh.key_strlen != te->fn.nd.dh.key_strlen) ||
		    (tmp_nd.dh.incarnation != te->fn.nd.dh.incarnation)) {
		    LOGMSG("Data inconsistent, fd=%d loff=0x%ld "
		    	   "dh.magicno (0x%lx 0x%lx) "
		    	   "dh.loff (0x%lx 0x%lx) "
		    	   "dh.key_strlen (%d %d) "
		    	   "dh.incarnation (0x%lx 0x%lx)",
			   ckpd->file_fd, te->fn.nd.dh.loff,
			   tmp_nd.dh.magicno, DH_MAGICNO,
			   tmp_nd.dh.loff, te->fn.nd.dh.loff,
			   tmp_nd.dh.key_strlen, te->fn.nd.dh.key_strlen,
			   tmp_nd.dh.incarnation, te->fn.nd.dh.incarnation);
		    rv = 20;
		    break;
		}
	    } else {
		LOGMSG("read_node_data() failed, fd=%d loff=0x%lx ret=%d",
		       ckpd->file_fd, te->fn.nd.dh.loff, ret);
		rv = 21;
		break;
	    }
	    ret = write_node_data(ckpd->file_fd, te->fn.nd.dh.loff, &te->fn.nd);
	    if (ret) {
	    	LOGMSG("write_node_data() failed, type=%d ret=%d", 
		       te->type, ret);
		rv = 2;
	    }
	    break;

	case PT_XC_DELETE:
	    ret = read_node_data(ckpd->file_fd, te->fn.nd.dh.loff, &tmp_nd);
	    if (!ret) {
	    	if ((tmp_nd.dh.magicno != DH_MAGICNO) ||
		    (tmp_nd.dh.loff != te->fn.nd.dh.loff) ||
		    (tmp_nd.dh.key_strlen != te->fn.nd.dh.key_strlen) ||
		    (tmp_nd.dh.incarnation != te->fn.nd.dh.incarnation)) {
		    LOGMSG("Data inconsistent, fd=%d loff=0x%ld "
		    	   "dh.magicno (0x%lx 0x%lx) "
		    	   "dh.loff (0x%lx 0x%lx) "
		    	   "dh.key_strlen (%d %d) "
		    	   "dh.incarnation (0x%lx 0x%lx)",
			   ckpd->file_fd, te->fn.nd.dh.loff,
			   tmp_nd.dh.magicno, DH_MAGICNO,
			   tmp_nd.dh.loff, te->fn.nd.dh.loff,
			   tmp_nd.dh.key_strlen, te->fn.nd.dh.key_strlen,
			   tmp_nd.dh.incarnation, te->fn.nd.dh.incarnation);
		    rv = 30;
		    break;
		}
	    } else {
		LOGMSG("read_node_data() failed, fd=%d loff=0x%lx ret=%d",
		       ckpd->file_fd, te->fn.nd.dh.loff, ret);
		rv = 31;
		break;
	    }
	    ret = delete_fnode(ckpd, &te->fn);
	    if (ret) {
	    	LOGMSG("delete_fnode() failed, type=%d ret=%d", te->type, ret);
		rv = 3;
	    }
	    break;

	case PT_XC_RESET:
	    ret = init_trie_ckpt_file(ckpd->file_fd, &ckpd->fhd1, &ckpd->fhd2);
	    if (ret) {
	    	LOGMSG("init_trie_ckpt_file() failed, fd=%d ret=%d", 
		       ckpd->file_fd, ret);
		rv = 4;
		break;
	    }
	    ret = reset_ckpt_file_alloc_data(ckpd);
	    if (ret) {
	    	LOGMSG("reset_ckpt_file_alloc_data() failed, fd=%d ret=%d",
		       ckpd->file_fd, ret);
		rv = 5;
		break;
	    }
	    break;

	default:
	    LOGMSG("Unknown type=%d", te->type);
	    rv = 6;
	    break;
	}
	if (rv) {
	    break;
	}
    }
    return rv;
}

static int 
apply_log2trie(ptrie_context_t *ctx, trie_data_t *td, trie_xaction_log_t *log)
{
    trie_xaction_entry_t *te;
    node_data_t *nd;
    void *leaf;
    int n;
    int ret;
    int rv = 0;

    for (n = 0; n < log->entries; n++) {
    	te = &log->en[n];

	switch(te->type) {
	case PT_XC_ADD:
	    ret = cp_trie_add(td->trie, te->fn.key, &te->fn.nd);
	    if (ret) {
	    	LOGMSG("cp_trie_add() failed, trie=%p key=%s type=%d ret=%d",
		       td->trie, te->fn.key, te->type, ret);
		rv = 1;
	    }
	    break;

	case PT_XC_UPDATE:
	    nd = (node_data_t *)cp_trie_exact_match(td->trie, te->fn.key); 
	    if (nd) {
	    	if (ctx->copy_app_data) {
		    (*ctx->copy_app_data)(&te->fn.nd.ad, &nd->ad);
		} else {
		    nd->ad = te->fn.nd.ad;
		}
	    } else {
	    	LOGMSG("cp_trie_exact_match() returned NULL, trie=%p key=%s",
		       td->trie, te->fn.key);
	    	rv = 2;
	    }
	    break;

	case PT_XC_DELETE:
	    ret = cp_trie_remove(td->trie, te->fn.key, &leaf);
#ifdef CHECK_CP_TRIE_REMOVE_RET
	    if (ret) {
	    	LOGMSG("cp_trie_remove() failed, trie=%p key=%s type=%d ret=%d",
		       td->trie, te->fn.key, te->type, ret);
	    	rv = 4;
	    }
#endif
	    break;

	case PT_XC_RESET:
	    // Delete all trie entries
	    ret = reset_cp_trie(td);
	    if (ret) {
		LOGMSG("reset_cp_trie() failed, ctx=%p ret=%d", ctx, ret);
    		rv = 5;
	    }
	    break;

	default:
	    LOGMSG("Unknown type=%d", te->type);
	    rv = 6;
	    break;
	}
	if (rv) {
	    break;
	}
    }
    return rv;
}

/*
 *******************************************************************************
 * Recover from interfaces
 *******************************************************************************
 */
static int 
recover_trie_from_ckpt_file(ptrie_context_t *ctx, trie_data_t *td, 
			    ckpt_file_data_t *ckptd)
{
    int ret;

    LOGMSG("ctx=%p td=%p fd=%d", ctx, td, ckptd->file_fd);

    ret = reset_cp_trie(td);
    if (ret) {
    	LOGMSG("reset_cp_trie() failed, ret=%d", ret);
    	return 1;
    }

    ret = restore_trie_from_ckpt(ckptd->file_fd, ctx, td->trie, 0,
				 &ckptd->freemap, 0, 0);
    if (ret) {
	LOGMSG("restore_trie_from_ckpt() failed, fd=%d ret=%d", 
	       ckptd->file_fd, ret);
	return 2;
    }
    return 0;
}

static int 
recover_ckpt_file_from(ckpt_file_data_t *backup, ckpt_file_data_t *target)
{
    int ret;
    int fd;

    LOGMSG("from_fd=%d target_fd=%d", backup->file_fd, target->file_fd);

    ret = copy_f1_to_f2(backup->file_fd, target->file_fd);
    if (ret) {
    	LOGMSG("copy_f1_to_f2() failed, src_fd=%d dest_fd=%d ret=%d",
	       backup->file_fd, target->file_fd, ret);
	return 1;
    }

    ret = open_trie_ckpt_file(0, target->file_fd, 
			      &target->fhd1, &target->fhd2, 
			      &fd, &target->next_entry_loff);
    if (ret) {
    	LOGMSG("open_trie_ckpt_file() failed, ret=%d", ret);
	return 2;
    }
    return 0;

}

/*
 * update_ckpt_file() - Commit log to target checkpoint file
 *
 * Return:
 *  ==0 Success
 *  > 0 Error
 *  < 0 Fatal error, reinitialize system
 */
static int 
update_ckpt_file(ckpt_file_data_t *target, ckpt_file_data_t *backup,
		 trie_xaction_log_t *log, long seqno, struct timespec *ts, 
		 fh_user_data_t *fhd)
{
    int ret;
    int rv = 0;
    struct stat sbuf;

    // Update header seqno and time
    target->fhd1.u.hdr.seqno = seqno;
    target->fhd1.u.hdr.ts = *ts;

    ////////////////////////////////////////////////////////////////////////////
    while (1) { // Begin while
    ////////////////////////////////////////////////////////////////////////////

    // Write fh1 to checkpoint file
    ret = write_file_header(target->file_fd, 1, &target->fhd1);
    if (ret) {
	LOGMSG("write_file_header() failed, ret=%d", ret);
	ret = recover_ckpt_file_from(backup, target);
	if (ret) {
	    LOGSEV("FATAL recover_ckpt_file_from() error, ret=%d", ret);
	    rv = -1;
	} else {
	    rv = 1;
	}
	break;
    }

    // Note: Failures starting here will roll back to the last 
    //       committed transaction.

    // Apply log to checkpoint file
    ret = apply_log2ckpt_file(target, log);
    if (ret) {
	LOGMSG("apply_log2ckpt_file() failed, ckpd=%p ret=%d", target, ret);
	ret = recover_ckpt_file_from(backup, target);
	if (ret) {
	    LOGSEV("FATAL recover_ckpt_file_from() error, ret=%d", ret);
	    rv = -2;
	} else {
	    rv = 2;
	}
	break;
    }

    // Update filesize header data
    if ((ret = fstat(target->file_fd, &sbuf)) != 0) {
    	LOGMSG("fstat(fd=%d) failed, ret=%d errno=%d", 
	       target->file_fd, ret, errno);
	ret = recover_ckpt_file_from(backup, target);
	if (ret) {
	    LOGSEV("FATAL recover_ckpt_file_from() error, ret=%d", ret);
	    rv = -3;
	} else {
	    rv = 3;
	}
	break;
    }
    target->fhd1.u.hdr.filesize = sbuf.st_size;

    // Note: Reset the following, since PT_XC_RESET will zero these
    target->fhd1.u.hdr.seqno = seqno;
    target->fhd1.u.hdr.ts = *ts;

    // Update user file header data
    target->fhd1.u.hdr.ud = *fhd;

    // Write fh{1,2} to checkpoint file
    ret = write_file_header(target->file_fd, 1, &target->fhd1);
    if (ret) {
	LOGMSG("write_file_header() failed, ret=%d", ret);
	ret = recover_ckpt_file_from(backup, target);
	if (ret) {
	    LOGSEV("FATAL recover_ckpt_file_from() error, ret=%d", ret);
	    rv = -4;
	} else {
	    rv = 4;
	}
	break;
    }

    ret = write_file_header(target->file_fd, 2, &target->fhd1);
    if (ret) {
	LOGMSG("write_file_header() failed, ret=%d", ret);
	ret = recover_ckpt_file_from(backup, target);
	if (ret) {
	    LOGSEV("FATAL recover_ckpt_file_from() error, ret=%d", ret);
	    rv = -5;
	} else {
	    rv = 5;
	}
	break;
    }

    // Note: Current transaction committed, any failure from here on will 
    //       preserve the transaction.

    break;

    ////////////////////////////////////////////////////////////////////////////
    } // End while
    ////////////////////////////////////////////////////////////////////////////

    return rv;
}

static int 
add_log_entry(ptrie_context_t *ctx, ptrie_xaction_type_t type, 
	      const char *key, const data_header_t *dh, 
	      const app_data_t *ad)
{
    int entry_ix;

    if (ctx->log->entries >= MAX_XACTION_ENTRIES) {
    	LOGMSG("Log full, ctx=%p", ctx);
    	return -1; // Log full
    }
    entry_ix = ctx->log->entries++;
    ctx->log->en[entry_ix].type = type;
    ctx->log->en[entry_ix].fn.nd.dh = *dh;
    if (ctx->copy_app_data) {
    	memset(&ctx->log->en[entry_ix].fn.nd.ad, 0, 
	       sizeof(ctx->log->en[entry_ix].fn.nd.ad));
    	(*ctx->copy_app_data)(ad, &ctx->log->en[entry_ix].fn.nd.ad);
    } else {
    	ctx->log->en[entry_ix].fn.nd.ad = *ad;
    }
    ctx->log->en[entry_ix].fn.nd.ctx = ctx;

    strncpy(ctx->log->en[entry_ix].fn.key, key, 
	    sizeof(ctx->log->en[entry_ix].fn.key));
    ctx->log->en[entry_ix].fn.key_null = '\0';

    return 0;
}

/*
 *******************************************************************************
 * External Interface Functions
 *******************************************************************************
 */

/*
 * ptrie_init() - Subsystem initialization
 *
 * NULL proc arg will use the system default.
 *
 * Return:
 *  ==0, Success
 *  !=0, Error
 */
int 
ptrie_init(const ptrie_config_t *cfg)
{
    assert(sizeof(file_header_t) == DEV_BSIZE);

    if (!cfg) {
    	return 1;
    }

    if (cfg->interface_version != PTRIE_INTF_VERSION) {
    	return 2;
    }

    if (cfg->log_level) {
    	ptrie_log_level = cfg->log_level;
    } else {
    	ptrie_log_level = &ptrie_def_log_level;
    }

    if (cfg->proc_logfunc) {
    	ptrie_logfunc = cfg->proc_logfunc;
    } else {
    	ptrie_logfunc = internal_logfunc;
    }

    if (cfg->proc_malloc) {
    	ptrie_malloc = cfg->proc_malloc;
    } else {
    	ptrie_malloc = malloc;
    }

    if (cfg->proc_calloc) {
    	ptrie_calloc = cfg->proc_calloc;
    } else {
    	ptrie_calloc = calloc;
    }

    if (cfg->proc_realloc) {
    	ptrie_realloc = cfg->proc_realloc;
    } else {
    	ptrie_realloc = realloc;
    }

    if (cfg->proc_free) {
    	ptrie_free = cfg->proc_free;
    } else {
    	ptrie_free = free;
    }

    AO_store(&init_complete, 1);

    return 0;
}

/*
 * new_ptrie_context() - Create Persistent Trie (ptrie) context
 *
 * NULL proc arg implies no action required.
 *
 * Return:
 *  !=0, Success, pointer ptrie_context_t
 *  == 0, Error
 */
ptrie_context_t *
new_ptrie_context(void (*copy_app_data)(const app_data_t *src,
				    	app_data_t *dest),
		  void (*destruct_app_data)(app_data_t *d))
{
    ptrie_context_t *ctx = 0;

    if (!AO_load(&init_complete)) {
	LOGSEV("FATAL Subsystem not initialized, ptrie_init() not called");
	return 0;
    }

    ////////////////////////////////////////////////////////////////////////////
    while (1) { // Begin while
    ////////////////////////////////////////////////////////////////////////////

    ctx = (ptrie_context_t *)CALLOC(1, sizeof(ptrie_context_t));
    if (!ctx) {
	LOGMSG("new_ptrie_context(), calloc failed");
	break;
    }
    ctx->f[0].file_fd = -1;
    ctx->f[0].freemap.mapsize = MAX_TRIE_ENTRIES / NBBY;
    ctx->f[0].freemap.map = CALLOC(1, ctx->f[0].freemap.mapsize);
    if (!ctx->f[0].freemap.map) {
	LOGMSG("f[0].freemap.map, calloc failed");
	break;
    }

    ctx->f[1].file_fd = -1;
    ctx->f[1].freemap.mapsize = MAX_TRIE_ENTRIES / NBBY;
    ctx->f[1].freemap.map = CALLOC(1, ctx->f[1].freemap.mapsize);
    if (!ctx->f[1].freemap.map) {
	LOGMSG("f[1].freemap.map, calloc failed");
	break;
    }

    ctx->bi_ckpt_file_bmap.mapsize =  MAX_TRIE_ENTRIES / NBBY;
    ctx->bi_ckpt_file_bmap.map = CALLOC(1, ctx->bi_ckpt_file_bmap.mapsize);
    if (!ctx->bi_ckpt_file_bmap.map) {
	LOGMSG("bi_ckpt_file_bmap.map, calloc failed");
	break;
    }

    ctx->log = CALLOC(1, sizeof(trie_xaction_log_t));
    if (!ctx->log) {
	LOGMSG("log, calloc failed");
	break;
    }

    ctx->copy_app_data = copy_app_data;
    ctx->destruct_app_data = destruct_app_data;

    return ctx;

    ////////////////////////////////////////////////////////////////////////////
    } // End while
    ////////////////////////////////////////////////////////////////////////////

    if (ctx) {
	if (ctx->f[0].freemap.map) {
	    FREE(ctx->f[0].freemap.map);
	    ctx->f[0].freemap.map = 0;
	}

	if (ctx->f[1].freemap.map) {
	    FREE(ctx->f[1].freemap.map);
	    ctx->f[1].freemap.map = 0;
	}

	if (ctx->bi_ckpt_file_bmap.map) {
	    FREE(ctx->bi_ckpt_file_bmap.map);
	    ctx->bi_ckpt_file_bmap.map = 0;
	}

	if (ctx->log) {
	    FREE(ctx->log);
	    ctx->log = 0;
	}

	if (ctx) {
	    FREE(ctx);
	    ctx = 0;
	}
    }
    return 0;
}

/*
 * delete_ptrie_context() - Delete Persistent Trie (ptrie) context
 */
void 
delete_ptrie_context(ptrie_context_t *ctx)
{
    if (ctx) {
    	if (ctx->f[0].file_fd >= 0) {
	    close(ctx->f[0].file_fd);
	    ctx->f[0].file_fd = -1;
	}

    	if (ctx->f[0].freemap.map) {
	    FREE(ctx->f[0].freemap.map);
	    ctx->f[0].freemap.map = 0;
    	}

    	if (ctx->f[1].file_fd >= 0) {
	    close(ctx->f[1].file_fd);
	    ctx->f[1].file_fd = -1;
	}

    	if (ctx->f[1].freemap.map) {
	    FREE(ctx->f[1].freemap.map);
	    ctx->f[1].freemap.map = 0;
    	}

    	if (ctx->bi_ckpt_file_bmap.map) {
	    FREE(ctx->bi_ckpt_file_bmap.map);
	    ctx->bi_ckpt_file_bmap.map = 0;
    	}

	if (ctx->td[0].trie) {
	    cp_trie_destroy(ctx->td[0].trie);
	    ctx->td[0].trie = 0;
	}

	if (ctx->td[1].trie) {
	    cp_trie_destroy(ctx->td[1].trie);
	    ctx->td[1].trie = 0;
	}

    	if (ctx->log) {
	    FREE(ctx->log);
	    ctx->log = 0;
	}

    	FREE(ctx);
    }
}

/*
 * ptrie_recover_from_ckpt() - Recover Trie from checkpoint file
 *
 * Return:
 *  ==0, Success
 *  !=0, Error
 */
int 
ptrie_recover_from_ckpt(ptrie_context_t *ctx, 
			const char *ckp_file1, const char *ckp_file2)
{
    int ret1;
    int ret2;
    int ret;
    int rv = 0;
    int sync_file;
    file_header_t *pfh1;
    file_header_t *pfh2;

    struct stat st1;
    struct stat st2;
    long max_fnode_incarnation;

    ctx->f[0].file_fd = -1;
    ctx->f[1].file_fd = -1;
    ctx->td[0].trie = 0;
    ctx->td[1].trie = 0;

    LOGMSG("ckpt_f1=%s ckpt_f2=%s", ckp_file1 ? ckp_file1 : "", 
	   ckp_file2 ? ckp_file2 : "");

    ////////////////////////////////////////////////////////////////////////////
    while (1) { // Begin while
    ////////////////////////////////////////////////////////////////////////////

    ret1 = open_trie_ckpt_file(ckp_file1, -1,
			       &ctx->f[0].fhd1, &ctx->f[0].fhd2,
			       &ctx->f[0].file_fd, &ctx->f[0].next_entry_loff);
    if (ret1) {
    	// Should never happen
	LOGMSG("open_trie_ckpt_file(%s) failed, ret=%d", ckp_file1, ret1);
	rv = 1;
	break;
    }
    ret2 = open_trie_ckpt_file(ckp_file2, -1,
    			       &ctx->f[1].fhd1, &ctx->f[1].fhd2,
			       &ctx->f[1].file_fd, &ctx->f[1].next_entry_loff);
    if (ret2) {
    	// Should never happen
	LOGMSG("open_trie_ckpt_file(%s) failed, ret=%d", ckp_file2, ret2);
	rv = 2;
	break;
    }

    // Determine the most recent checkpoint file to use

    sync_file = 0;
    pfh1 = &ctx->f[0].fhd1;
    pfh2 = &ctx->f[1].fhd1;

    if (pfh1->u.hdr.seqno == pfh2->u.hdr.seqno) {
    	// In sync, verify additional fields
	LOGMSG("seqno (%ld == %ld) case",
	       pfh1->u.hdr.seqno, pfh2->u.hdr.seqno);

	memset(&st1, 0, sizeof(st1));
	ret = fstat(ctx->f[0].file_fd, &st1);
	memset(&st2, 0, sizeof(st2));
	ret = fstat(ctx->f[1].file_fd, &st2);

	if (st1.st_size != st2.st_size) {
	    LOGMSG("seqno (%ld) equal case, file size mismatch (%ld %ld)",
	    	   pfh1->u.hdr.seqno, st1.st_size, st2.st_size);
	    sync_file++;
	}

    	if (memcmp(&pfh1->u.hdr.ts, &pfh2->u.hdr.ts, 
		   sizeof(pfh1->u.hdr.ts))) {
	    LOGMSG("seqno (%ld) equal case, ts mismatch force copy, "
	    	   "tv_sec=%ld %ld tv_nsec=%ld %ld",
		   pfh1->u.hdr.seqno,
		   pfh1->u.hdr.ts.tv_sec, pfh2->u.hdr.ts.tv_sec,
		   pfh1->u.hdr.ts.tv_nsec, pfh2->u.hdr.ts.tv_nsec);
	    sync_file++;
	}

    	if (memcmp(&pfh1->u.hdr.ud, &pfh2->u.hdr.ud, sizeof(fh_user_data_t))) {
	    LOGMSG("seqno (%ld) equal case, fh_user_data_t mismatch "
	    	   "force copy",
	    	   pfh1->u.hdr.seqno);
	    sync_file++;
	}

	if (sync_file) {
	    ret = copy_f1_to_f2(ctx->f[0].file_fd, ctx->f[1].file_fd);
	    if (!ret) {
	    	// Restart file recovery to be sure we are consistent
	    	close_trie_ckpt_file(&ctx->f[0]);
	    	close_trie_ckpt_file(&ctx->f[1]);
		continue;
	    } else {
		LOGMSG("copy_f1_to_f2(fd1=%d fd2=%d) failed, ret=%d",
		       ctx->f[0].file_fd, ctx->f[1].file_fd, ret);
		rv = 3;
		break;
	    }
	}
    } else if (pfh1->u.hdr.seqno > pfh2->u.hdr.seqno) {
	LOGMSG("seqno (%ld > %ld) case, force copy",
	       pfh1->u.hdr.seqno, pfh2->u.hdr.seqno);

	ret = copy_f1_to_f2(ctx->f[0].file_fd, ctx->f[1].file_fd);
	if (!ret) {
	    // Restart file recovery to be sure we are consistent
	    close_trie_ckpt_file(&ctx->f[0]);
	    close_trie_ckpt_file(&ctx->f[1]);
	    continue;
	} else {
	    LOGMSG("copy_f1_to_f2(fd1=%d fd2=%d) failed, ret=%d",
		   ctx->f[0].file_fd, ctx->f[1].file_fd, ret);
	    rv = 4;
	    break;
	}
    } else {
	LOGMSG("seqno (%ld < %ld) case, force copy",
	       pfh1->u.hdr.seqno, pfh2->u.hdr.seqno);

	ret = copy_f1_to_f2(ctx->f[1].file_fd, ctx->f[0].file_fd);
	if (!ret) {
	    // Restart file recovery to be sure we are consistent
	    close_trie_ckpt_file(&ctx->f[0]);
	    close_trie_ckpt_file(&ctx->f[1]);
	    continue;
	} else {
	    LOGMSG("copy_f1_to_f2(fd1=%d fd2=%d) failed, ret=%d",
		   ctx->f[1].file_fd, ctx->f[0].file_fd, ret);
	    rv = 5;
	    break;
	}
    }

    ctx->td[0].trie = create_cp_trie();
    if (!ctx->td[0].trie) {
	LOGMSG("create_cp_trie() failed");
    	rv = 6;
	break;
    }
    ctx->td[1].trie = create_cp_trie(); 
    if (!ctx->td[1].trie) {
	LOGMSG("create_cp_trie() failed");
    	rv = 7;
	break;
    }

    ret = restore_trie_from_ckpt(ctx->f[0].file_fd, ctx,
    				 ctx->td[0].trie, ctx->td[1].trie,
				 &ctx->f[0].freemap, &ctx->f[1].freemap,
				 &max_fnode_incarnation);
    if (ret) {
	LOGMSG("restore_trie_from_ckpt() failed, ret=%d, "
	       "reinitializing checkpoint files", ret);

    	ret = ftruncate(ctx->f[0].file_fd, 0);
    	if (ret) {
	    LOGMSG("ftruncate(fd=%d) failed, ret=%d errno=%d", 
		   ctx->f[0].file_fd, ret, errno);
	}

    	ret = ftruncate(ctx->f[1].file_fd, 0);
    	if (ret) {
	    LOGMSG("ftruncate(fd=%d) failed, ret=%d errno=%d", 
		   ctx->f[1].file_fd, ret, errno);
	}

	// Restart file recovery
	close_trie_ckpt_file(&ctx->f[0]);
	close_trie_ckpt_file(&ctx->f[1]);
	continue;
    }
    ctx->cur_ckpt_file = &ctx->f[0];
    ctx->shdw_ckpt_file = &ctx->f[1];
    AO_store(&ctx->active_td, (uint64_t)&ctx->td[0]);
    ctx->cur_td = &ctx->td[0];
    ctx->shdw_td = &ctx->td[1];
    ctx->fnode_incarnation = max_fnode_incarnation + 1;

    break;

    ////////////////////////////////////////////////////////////////////////////
    } // End while
    ////////////////////////////////////////////////////////////////////////////

    if (rv) {
	close_trie_ckpt_file(&ctx->f[0]);
	close_trie_ckpt_file(&ctx->f[1]);

	if (ctx->td[0].trie) {
	    cp_trie_destroy(ctx->td[0].trie);
	    ctx->td[0].trie = 0;
	}
	if (ctx->td[1].trie) {
	    cp_trie_destroy(ctx->td[1].trie);
	    ctx->td[1].trie = 0;
	}
    }
    return rv;
}

#define GET_TRIE_DATA(_ctx, _td) { \
    uint64_t _val; \
    _val = AO_load(&(_ctx)->active_td); \
    (_td) = (trie_data_t *)_val; \
    if ((_td)) { \
    	AO_fetch_and_add1(&(_td)->refcnt); \
    } \
}

#define SET_TRIE_DATA(_ctx, _td) { \
    AO_store(&(_ctx)->active_td, (_td)); \
}

#define RELEASE_TRIE_DATA(_td) { \
    AO_fetch_and_sub1(&(_td)->refcnt); \
}

/*
 * ptrie_prefix_match() - Reader side prefix match, return app_data_t
 *
 * Assumptions:
 *  1) Reader side interface.
 *  2) Concurrent callers supported.
 *  3) Return values as defined by cp_trie_prefix_match()
 */
int 
ptrie_prefix_match(ptrie_context_t *ctx, const char *key, app_data_t *ad)
{
    trie_data_t *td;
    node_data_t *nd;
    int rv = 0;

    GET_TRIE_DATA(ctx, td);
    if (!td) {
	LOGMSG("td == 0, ctx=%p", ctx);
    	return 0; // Should never happen
    }

    rv = cp_trie_prefix_match(td->trie, (char *)key, (void **)&nd);
    if (rv > 0) {
    	if (ctx->copy_app_data) {
	    (*ctx->copy_app_data)(&nd->ad, ad);
	} else {
	    *ad = nd->ad;
	}
    } 

    RELEASE_TRIE_DATA(td);
    return rv;
}

/* 
 * ptrie_exact_match() -  Reader side exact match, return app_data_t
 *
 * Assumptions:
 *  1) Reader side interface.
 *  2) Concurrent callers supported.
 *
 * Return:
 *  ==0, Success
 *  !=0, Error
 */
int 
ptrie_exact_match(ptrie_context_t *ctx, const char *key, app_data_t *ad)
{
    trie_data_t *td;
    node_data_t *nd;
    int rv = 0;

    GET_TRIE_DATA(ctx, td);
    if (!td) {
	LOGMSG("td == 0, ctx=%p", ctx);
    	return 1; // Should never happen
    }

    nd = (node_data_t *)cp_trie_exact_match(td->trie, (char *)key); 
    if (nd) {
    	if (ctx->copy_app_data) {
	    (*ctx->copy_app_data)(&nd->ad, ad);
	} else {
	    *ad = nd->ad;
	}
    } else {
    	rv = 2;
    }

    RELEASE_TRIE_DATA(td);
    return rv;
}

/* 
 * ptrie_lock() - Reader side ptrie lock primitive
 *
 * Assumptions:
 *  1) Reader side interface.
 *  2) Concurrent callers supported.
 *
 * Return:
 *  ==0, Success
 *  !=0, Error
 */
int
ptrie_lock(ptrie_context_t *ctx)
{
    trie_data_t *td;

    GET_TRIE_DATA(ctx, td);
    if (!td) {
	LOGMSG("td == 0, ctx=%p", ctx);
    	return 1; // Should never happen
    }
    return 0;
}

/* 
 * ptrie_unlock() - Reader side ptrie unlock primitive
 *
 * Assumptions:
 *  1) Reader side interface.
 *  2) Concurrent callers supported.
 *
 * Return:
 *  ==0, Success
 *  !=0, Error
 */
int
ptrie_unlock(ptrie_context_t *ctx)
{
    trie_data_t *td;

    GET_TRIE_DATA(ctx, td);
    if (!td) {
	LOGMSG("td == 0, ctx=%p", ctx);
    	return 1; // Should never happen
    }
    RELEASE_TRIE_DATA(td);
    RELEASE_TRIE_DATA(td);
    return 0;
}

/* 
 * ptrie_list_keys() - Reader side, list keys associated with reader ptrie
 *		       checkpoint file.
 *
 * For each key, invoke int (*proc)(key, arg). non zero proc return 
 * indicates abort further action and return.
 *
 * Assumptions:
 *  1) Reader side interface.
 *  2) Concurrent callers supported.
 *
 * Return:
 *  ==0, Success
 *  !=0, Error
 */
int 
ptrie_list_keys(ptrie_context_t *ctx, void *proc_arg,
		int (*proc)(const char *key, void *proc_arg))
{
    int rv = 0;
    int rv2;
    int ret;
    int fd;
    struct stat sbuf;
    char *fmap = 0;
    file_node_data_t *fn;
    file_header_t *filehdr_1;
    file_header_t *filehdr_2;

    rv = ptrie_lock(ctx);
    if (rv) {
    	LOGMSG("ptrie_lock() failed, rv=%d ctx=%p", rv, ctx);
	return 1;
    }

    ////////////////////////////////////////////////////////////////////////////
    while (1) { // Begin while
    ////////////////////////////////////////////////////////////////////////////

    fd = ctx->cur_ckpt_file->file_fd;
    ret = fstat(fd, &sbuf);
    if (ret) {
    	LOGMSG("fstat() failed, errno=%d fd=%d", errno, fd);
    	rv = 2;
	break;
    }

    fmap = mmap((void *) 0, sbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (fmap == MAP_FAILED) {
    	LOGMSG("mmap() failed, errno=%d fd=%d", errno, fd);
	rv = 3;
	break;
    }

    filehdr_1 = (file_header_t *)fmap;
    filehdr_2 = (file_header_t *)(fmap + FHDR_2_FOFF);

    if (bcmp(filehdr_1, filehdr_2, sizeof(file_header_t))) {
    	LOGMSG("current ckpt file headers not equal, ctx=%p fd=%d", ctx, fd);
    	rv = 4;
	break;
    }

    for (fn = (file_node_data_t *)(fmap + LOFF2FOFF(0)); 
    	 (char *)fn < &fmap[sbuf.st_size]; fn++) {
    	if (fn->nd.dh.magicno == DH_MAGICNO) {
	    ret = (*proc)(fn->key, proc_arg);
	    if (ret) {
    		LOGMSG("list operation aborted, callback ret=%d", ret);
		rv = 5;
		break;
	    }
	} 
    }
    break;

    ////////////////////////////////////////////////////////////////////////////
    } // End while
    ////////////////////////////////////////////////////////////////////////////

    rv2 = ptrie_unlock(ctx);
    if (rv2) {
    	LOGMSG("ptrie_unlock() failed, rv=%d ctx=%p", rv2, ctx);
	rv += 1000;
    }

    if (fmap && (fmap != MAP_FAILED)) {
    	munmap(fmap, sbuf.st_size);
    }

    return rv;
}

/*
 * ptrie_tr_prefix_match() - Update side prefix match, return (app_data_t *)
 *
 * Assumptions:
 *  1) Update side interface.
 *  2) Must be executed under ptrie_{begin,end}_xaction()
 *  3) Concurrent callers not supported, user enforces this condition.
 *  4) Return values as defined by cp_trie_prefix_match()
 */
int 
ptrie_tr_prefix_match(ptrie_context_t *ctx, const char *key, app_data_t **ad)
{
    node_data_t *nd;
    int rv = 0;

    if (!AO_load(&ctx->in_xaction)) {
    	LOGMSG("ptrie_begin_xaction() not called, ctx=%p", ctx);
    	return 0; // ptrie_begin_xaction() not called
    }

    rv = cp_trie_prefix_match(ctx->shdw_td->trie, (char *)key, (void **)&nd);
    if (rv > 0) {
    	*ad = &nd->ad;
    } else {
    	*ad = 0;
    }

    return rv;
}

/* 
 * ptrie_tr_exact_match() -  Update side exact match, return (app_data_t *)
 *
 * Assumptions:
 *  1) Update side interface.
 *  2) Must be executed under ptrie_{begin,end}_xaction()
 *  3) Concurrent callers not supported, user enforces this condition.
 *
 * Return:
 *  !=0, Success, pointer app_data_t
 *  ==0, Error
 */
app_data_t *
ptrie_tr_exact_match(ptrie_context_t *ctx, const char *key)
{
    node_data_t *nd;

    if (!AO_load(&ctx->in_xaction)) {
    	LOGMSG("ptrie_begin_xaction() not called, ctx=%p", ctx);
    	return 0; // ptrie_begin_xaction() not called
    }

    nd = (node_data_t *)cp_trie_exact_match(ctx->shdw_td->trie, (char *)key); 
    if (nd) {
    	return &nd->ad;
    } else {
    	return 0;
    }
}

/*
 * ptrie_update_appdata() - Update side, update trie app_data_t
 *
 * Assumptions:
 *  1) Update side interface.
 *  2) Must be executed under ptrie_{begin,end}_xaction()
 *  3) Concurrent callers not supported, user enforces this condition.
 *  4) orig_ad must be obtained via ptrie_tr_prefix_match() or
 *     ptrie_tr_exact_match().
 *  5) orig_ad is only valid within the current ptrie_begin_xaction()
 *     context.   When ptrie_end_xaction() is issued, all orig_ad pointers
 *     are invalid.
 *
 * Return:
 *    ==0, success
 *    > 0, error
 *    < 0, transaction log full, commit current transaction and retry
 */
int 
ptrie_update_appdata(ptrie_context_t *ctx, const char *key,
		     app_data_t *orig_ad, const app_data_t *new_ad)
{
#define APPDATA2NODEDATA(_ad) \
	(node_data_t *)((char *)(_ad) - (long)(&(((node_data_t *)0)->ad)))

    node_data_t *nd;
    int ret;
    int rv = 0;

    if (!AO_load(&ctx->in_xaction)) {
    	LOGMSG("ptrie_begin_xaction() not called, ctx=%p", ctx);
    	return 1; // ptrie_begin_xaction() not called
    }

    nd = APPDATA2NODEDATA(orig_ad);
    if (nd->dh.magicno != DH_MAGICNO) {
    	return 2; // Invalid orig_ad ptr
    }
    ret = add_log_entry(ctx, PT_XC_UPDATE, key, &nd->dh, new_ad);
    if (!ret) {
    	// Update data
	if (ctx->copy_app_data) {
	    (*ctx->copy_app_data)(new_ad, orig_ad);
	} else {
	    *orig_ad = *new_ad;
	}
    } else {
    	LOGMSG("add_log_entry() failed, ctx=%p ret=%d", ctx, ret);
	if (ret > 0) {
	    rv = 100 + ret;
	} else {
	    rv = -1;
	}
    }
    return rv;
}

/*
 * ptrie_add() - Update side, add or update trie app_data_t 
 *		 associated with given key
 *
 * Assumptions:
 *  1) Update side interface.
 *  2) Must be executed under ptrie_{begin,end}_xaction()
 *  3) Concurrent callers not supported, user enforces this condition.
 *
 * Return:
 *    ==0, success
 *    > 0, error
 *    < 0, transaction log full, commit current transaction and retry
 */
int 
ptrie_add(ptrie_context_t *ctx, const char *key, app_data_t *ad)
{
    node_data_t *nd;
    file_node_data_t tmp_fnd;
    int key_strlen;
    int ret;
    int rv = 0;

    if (!AO_load(&ctx->in_xaction)) {
    	LOGMSG("ptrie_begin_xaction() not called, ctx=%p", ctx);
    	return 1; // ptrie_begin_xaction() not called
    }

    key_strlen = strlen(key);
    nd = (node_data_t *)cp_trie_exact_match(ctx->shdw_td->trie, (char *)key); 

    ////////////////////////////////////////////////////////////////////////////
    while (1) { // Begin while
    ////////////////////////////////////////////////////////////////////////////

    if (nd) { // Update
	ret = ptrie_update_appdata(ctx, key, &nd->ad, ad);
	if (ret) {
	    LOGMSG("ptrie_update_appdata() failed, ctx=%p ret=%d", 
	    	   ctx, ret);
	    if (ret > 0) {
	    	rv = 100 + ret;
	    } else {
	    	rv = -1;
	    }
	    break;
	}
    } else { // Add
    	ret = init_fnode(ctx->shdw_ckpt_file, &tmp_fnd, key, ad, ctx);
	if (ret) {
	    LOGMSG("init_fnode() failed, ctx=%p ret=%d", ctx, ret);
	    rv = 2; // Init fnode error
	    break;
	}

    	ret = add_log_entry(ctx, PT_XC_ADD, key, &tmp_fnd.nd.dh, ad);
	if (ret) {
	    LOGMSG("add_log_entry() failed, ctx=%p ret=%d", ctx, ret);
	    if (ret > 0) {
	    	rv = 200 + ret;
	    } else {
		rv = -1;
	    }
	    break;
	}

	ret = cp_trie_add(ctx->shdw_td->trie, (char *)key, &tmp_fnd);
	if (ret) {
	    LOGMSG("cp_trie_add() failed, ret=%d", ret);
	    rv = 3;
	    break;
	}
    }
    break;

    ////////////////////////////////////////////////////////////////////////////
    } // End while
    ////////////////////////////////////////////////////////////////////////////

    return rv;
}

/*
 * ptrie_remove - Update side, remove trie app_data_t associated 
 *		  with the given key
 *
 * Assumptions:
 *  1) Update side interface.
 *  2) Must be executed under ptrie_{begin,end}_xaction()
 *  3) Concurrent callers not supported, user enforces this condition.
 *  
 * Return:
 *    ==0, success
 *    > 0, error
 *    < 0, transaction log full, commit current transaction and retry
 */
int 
ptrie_remove(ptrie_context_t *ctx, const char *key)
{
    node_data_t *nd;
    int ret;
    int rv = 0;
    void *ptr;
    off_t loff;

    if (!AO_load(&ctx->in_xaction)) {
    	LOGMSG("ptrie_begin_xaction() not called, ctx=%p", ctx);
    	return 1; // ptrie_begin_action() not called
    }

    nd = (node_data_t *)cp_trie_exact_match(ctx->shdw_td->trie, (char *)key); 

    ////////////////////////////////////////////////////////////////////////////
    while (1) {
    ////////////////////////////////////////////////////////////////////////////

    if (nd) {
    	ret = add_log_entry(ctx, PT_XC_DELETE, key, &nd->dh, &nd->ad);
	if (ret) {
	    LOGMSG("add_log_entry() failed, ctx=%p ret=%d", ctx, ret);
	    if (ret > 0) {
	    	rv = 100 + ret;
	    } else {
	    	rv = -1;
	    }
	    break;
	}
	loff = nd->dh.loff; // nd deleted by cp_trie_remove()

	ret = cp_trie_remove(ctx->shdw_td->trie, (char *)key, &ptr);
#ifdef CHECK_CP_TRIE_REMOVE_RET
	if (ret) {
	    LOGMSG("cp_trie_remove() failed, ctx=%p ret=%d", ctx, ret);
	    rv = 2;
	    break;
	}
#endif
    	if (ISSET_FNODE_FREEMAP(ctx->shdw_ckpt_file, loff)) {
	    LOGMSG("free bitmap inconsistency cfd=%p key=%s loff=0x%lx",
		   ctx->shdw_ckpt_file, key, loff);
	    rv= 3;
	    break;
    	}
    	SET_FNODE_FREEMAP(ctx->shdw_ckpt_file, loff);
    }
    break;

    ////////////////////////////////////////////////////////////////////////////
    }
    ////////////////////////////////////////////////////////////////////////////
    return rv;
}

/*
 * ptrie_reset - Reset trie and checkpoint file state to null.
 *
 * Assumptions:
 *  1) Update side interface.
 *  2) Must be executed under ptrie_{begin,end}_xaction()
 *  3) Concurrent callers not supported, user enforces this condition.
 *  
 * Return:
 *    ==0, success
 *    > 0, error
 *    < 0, transaction log full, commit current transaction and retry
 */
int 
ptrie_reset(ptrie_context_t *ctx)
{
    int ret;
    int rv = 0;
    const char *key;
    data_header_t dh;
    app_data_t ad;
    
    ////////////////////////////////////////////////////////////////////////////
    while (1) {
    ////////////////////////////////////////////////////////////////////////////

    key = "";
    memset(&dh, 0, sizeof(dh));
    memset(&ad, 0, sizeof(ad));

    ret = add_log_entry(ctx, PT_XC_RESET, key, &dh, &ad);
    if (ret) {
	LOGMSG("add_log_entry() failed, ctx=%p ret=%d", ctx, ret);
	if (ret > 0) {
	    rv = 100 + ret;
	} else {
	    rv = -1;
	}
	break;
    }

    // Delete all trie entries
    ret = reset_cp_trie(ctx->shdw_td);
    if (ret) {
	LOGMSG("reset_cp_trie() failed, ctx=%p ret=%d", ctx, ret);
    	rv = 2;
	break;
    }

    // Reset file_node_data_t freemap
    ret = reset_ckpt_file_alloc_data(ctx->shdw_ckpt_file);
    if (ret) {
	LOGMSG("reset_ckpt_file_alloc_data() failed, ckpd=%p ret=%d", 
	       ctx->shdw_ckpt_file, ret);
    	rv = 3;
	break;
    }
    break;

    ////////////////////////////////////////////////////////////////////////////
    }
    ////////////////////////////////////////////////////////////////////////////
    return rv;
}

/*
 * ptrie_get_fh_data() - Update side, get fh_user_data_t persistent data 
 *			     associated with last xaction commit.
 * Assumptions:
 *  1) Update side interface.
 *
 * Return:
 *  !=0, Success, pointer fh_user_data_t
 *  ==0, Error
 */
const fh_user_data_t *
ptrie_get_fh_data(ptrie_context_t *ctx)
{
    if (ctx->cur_ckpt_file) {
    	return &ctx->cur_ckpt_file->fhd1.u.hdr.ud;
    } else {
    	return 0;
    }
}

/*
 * ptrie_begin_xaction() - Update side, begin transaction
 *
 * Assumptions:
 *  1) Update side interface.
 *  2) Concurrent callers not supported, user enforces this condition.
 *
 * Return:
 *  ==0, Success
 *  !=0, Error
 */
int 
ptrie_begin_xaction(ptrie_context_t *ctx)
{
    int rv;

    LOGMSG("ctx=%p", ctx);

    if (AO_fetch_and_add1(&ctx->in_xaction)) {
    	AO_fetch_and_sub1(&ctx->in_xaction);
	LOGMSG("Already invoked for ctx=%p", ctx);
    	return 1; // ptrie_begin_action() already called
    }
    ctx->log->entries = 0;

    rv = copy_ckpt_file_alloc_data(ctx);
    if (rv) {
	LOGMSG("copy_ckpt_file_alloc_data(ctx=%p) failed, rv=%d", ctx, rv);
    }
    return rv;
}

/*
 * ptrie_end_xaction() - Update side, commit or abort transaction
 *
 * Assumptions:
 *  1) Update side interface.
 *  2) Concurrent callers not supported, user enforces this condition.
 *
 * Return:
 *  ==0, Success
 *  >0, Error, commit aborted
 *  <0, Unrecoverable error, reinitialize system
 *
 */
int 
ptrie_end_xaction(ptrie_context_t *ctx, int commit, fh_user_data_t *fhd)
{
    int rv = 0;
    int ret;
    int ret2;
    long seqno;
    struct timespec ts;
    trie_data_t *tmp_td;
    struct pollfd pfd;
    long refcnt;

    LOGMSG("ctx=%p commit=%d fhd=%p", ctx, commit, fhd);

    if (!AO_load(&ctx->in_xaction)) {
	return 1; // ptrie_begin_action() not called
    }

    ret = restore_ckpt_file_alloc_data(ctx);
    if (ret) {
    	LOGMSG("restore_ckpt_file_alloc_data(ctx=%p) failed, ret=%d", ctx, ret);
	return 2;
    }

    ////////////////////////////////////////////////////////////////////////////
    while (1) { // Begin while
    ////////////////////////////////////////////////////////////////////////////

    if (!ctx->log->entries) {
    	ctx->log->entries = 0;
    	break; // Nothing do do
    }

    if (!commit) {
	ret = recover_trie_from_ckpt_file(ctx, ctx->shdw_td, 
					  ctx->cur_ckpt_file);
	if (ret) {
	    LOGSEV("FATAL recover_trie_from_ckpt_file() failed, ret=%d", ret);
	    rv = -10; // fatal error
	}
	break;
    }

    // Update shadow ckeckpoint file with log
    seqno = ctx->shdw_ckpt_file->fhd1.u.hdr.seqno + 1;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    ret = update_ckpt_file(ctx->shdw_ckpt_file, ctx->cur_ckpt_file,
    			   ctx->log, seqno, &ts, fhd);
    if (ret) {
	LOGMSG("update_ckpt_file() failed, ret=%d", ret);
	if (ret > 0) {
	    ret2 = recover_trie_from_ckpt_file(ctx, ctx->shdw_td, 
					       ctx->cur_ckpt_file);
	    if (!ret2) {
	    	rv = 100 + ret; // abort commit
	    } else {
	    	LOGSEV("FATAL recover_trie_from_ckpt_file() failed, ret2=%d", 
		       ret2);
		rv = -50; // fatal error
	    }
	} else {
	    LOGSEV("FATAL update_ckpt_file() failed, ret=%d", ret);
	    rv = -100 + ret; // fatal error
	}
	break;
    }

    // Update current ckeckpoint file with log
    ret = update_ckpt_file(ctx->cur_ckpt_file, ctx->shdw_ckpt_file,
    			   ctx->log, seqno, &ts, fhd);
    if (ret) {
	if (ret > 0) {
	    LOGMSG("update_ckpt_file() failed, ret=%d", ret);
	    rv = 200 + ret; // abort commit
	} else {
	    LOGSEV("update_ckpt_file() failed, ret=%d", ret);
	    rv = -200 + ret; // fatal error
	}
	break;
    }

    // Switch readers to updated trie 
    SET_TRIE_DATA(ctx, (uint64_t)ctx->shdw_td);
    tmp_td = ctx->cur_td;
    ctx->cur_td = ctx->shdw_td;
    ctx->shdw_td = tmp_td;

    // Wait until no user references exist prior to updating the stale trie 
    refcnt = 0;
    while (1) {
        // Always delay at least one interval
    	if (refcnt) {
	    LOGMSG("Waiting for (refcnt(%ld) == 0), ctx=%p trie=%p", 
		   refcnt, ctx, ctx->shdw_td->trie);
	}
    	poll(&pfd, 0, 100); // 100 msec sleep
	refcnt = AO_load(&ctx->shdw_td->refcnt);
	if (!refcnt) {
	    break;
	}
    }

    // Apply log to stale trie
    ret = apply_log2trie(ctx, ctx->shdw_td, ctx->log);
    if (ret) {
	LOGMSG("apply_log2trie() failed, trie=%p ret=%d", 
	       ctx->shdw_td->trie, ret);
        ret = recover_trie_from_ckpt_file(ctx, ctx->shdw_td, 
					  ctx->cur_ckpt_file);
	if (ret) {
	    LOGSEV("recover_trie_from_ckpt_file() failed, "
	    	   "trie=%p ckpt=%p ret=%d", 
		   ctx->shdw_td->trie, ctx->cur_ckpt_file, ret);
	    rv = -300; // fatal error
	}
    }

    break;

    ////////////////////////////////////////////////////////////////////////////
    } // End while
    ////////////////////////////////////////////////////////////////////////////

    AO_fetch_and_sub1(&ctx->in_xaction);
    ctx->log->entries = 0;
    return rv;
}

/*
 * End of persistent_trie.c
 */
