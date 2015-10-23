#include <malloc.h>
#include <sys/statvfs.h>
#include <math.h>
#include <pthread.h>
#include <glib.h>

/* mfp defs */
#include "mfp_err.h"
#include "mfp_safe_io.h"

/* nkn defs */
#include "nkn_debug.h"
#include "nkn_defs.h"
#include "nkn_stat.h"
#include "nkn_memalloc.h"
#include "nkn_vpe_types.h"
#include "nkn_vpe_utils.h"

/* global vars */
static io_handlers_t fioh =  {
    .ioh_open = nkn_vpe_fopen,
    .ioh_read = nkn_vpe_fread,
    .ioh_tell = nkn_vpe_ftell,
    .ioh_seek = nkn_vpe_fseek,
    .ioh_close = nkn_vpe_fclose,
    .ioh_write = nkn_vpe_fwrite
};

GHashTable *mnt_point_list;
pthread_mutex_t mnt_point_list_lock;
typedef struct tag_mfp_mnt_point_stats {
    char *mnt_path;
    AO_t size_reserved;
    AO_t size_avl;
    pthread_mutex_t mutex;
}mfp_mnt_point_stats_t;

static int32_t mfp_safe_io_reserve_from_mntlist(const char *const\
						mount_path, 
						size_t size_reserved, 
						float use,
						mfp_safe_io_ctx_t *ret); 
static int32_t mfp_safe_io_set_local_reservn(size_t size_reserved, 
					     float use,
					     mfp_safe_io_ctx_t *ret);

/**********************************************************************
 * a. top level abstraction mfp_safe_io_ctx_t. you can attach this to
 * any descriptor context to indicate that the descriptor will use the
 * resources from the context pool. 
 * b. it is assumed that writes to the descriptor are inherently
 * serialized 
 * c. bounds check is done at the context level create time at a
 * context level to ensure that the context pool has enough
 * space. furthermore, it is done at the descriptor level during every
 * write (this is why we need an inherent serialization of the writes
 * on a per descriptor basis)
 *********************************************************************/
int32_t
mfp_safe_io_glob_init(void)
{
    int32_t macro_ret;

    PTHREAD_MUTEX_INIT(&mnt_point_list_lock);
    mnt_point_list = g_hash_table_new(g_str_hash, g_str_equal);
    if (!mnt_point_list) {
	return -E_MFP_NO_MEM;
    }

    return 0;
}

int32_t
mfp_safe_io_add_reservn_list(const char *mount_path, float use, 
			      size_t size_reserved)
{
    struct statvfs st_buf;
    size_t free_size;
    int32_t rv, macro_ret;
    mfp_mnt_point_stats_t *mnt_st;

    mnt_st = NULL;

    PTHREAD_MUTEX_LOCK(&mnt_point_list_lock);
    mnt_st = \
	(mfp_mnt_point_stats_t*)g_hash_table_lookup(mnt_point_list,
						    mount_path);
    if (mnt_st) {
	return -E_MFP_SAFE_IO_PATH_EXISTS;
    }
    
    /* stat the mount path */
    rv = statvfs(mount_path, &st_buf);
    if(rv == -1) {
	return -E_MFP_SAFE_IO_ESTAT;
    }

    if (st_buf.f_flag & ST_RDONLY) {
	return -E_MFP_SAFE_IO_ERONLY;
    }

    mnt_st = calloc(1, sizeof(mfp_mnt_point_stats_t));
    if (!mnt_st) {
	return -E_MFP_NO_MEM;
    }
    PTHREAD_MUTEX_INIT(&mnt_st->mutex);
    
    /* compute free size and tot reservation for this context */
    free_size = (st_buf.f_blocks * st_buf.f_frsize) - \
	(st_buf.f_bavail * st_buf.f_bsize);
    if (!size_reserved) {
	mnt_st->size_reserved = floor(use * free_size);
    } else {
	mnt_st->size_reserved = size_reserved;
    }
    
    mnt_st->size_avl = mnt_st->size_reserved;

    g_hash_table_insert(mnt_point_list, strdup(mount_path), mnt_st);
    PTHREAD_MUTEX_UNLOCK(&mnt_point_list_lock);

    return 0;

}

int32_t        
mfp_safe_io_init(const char *mount_path, float use, 
		 size_t size_reserved,
		 mfp_safe_io_ctx_t **ret) 
{
    int32_t err = 0;
    mfp_safe_io_ctx_t *ctx = NULL;
    int32_t macro_ret;

    *ret = ctx = (mfp_safe_io_ctx_t *)\
	calloc(1, sizeof(mfp_safe_io_ctx_t));
    if (!ctx) {
	return -E_MFP_NO_MEM;
    }
    PTHREAD_MUTEX_INIT(&ctx->mutex);

    if (mount_path) {
	err = mfp_safe_io_reserve_from_mntlist(mount_path,
					       size_reserved, use,
					       *ret);
    } else {
	err = mfp_safe_io_set_local_reservn(size_reserved, use, *ret);
    }

    return err;
    
}

void *
mfp_safe_io_open(const char *fname, const char *mode)
{
    mfp_safe_io_desc_t *ctx;

    /* create the descriptor */
    ctx = (mfp_safe_io_desc_t*)calloc(1, sizeof(mfp_safe_io_desc_t));
    if (!ctx) {
	return NULL;
    }

    /* allow user to create a dummy descriptor */
    if (!fname) {
	return (void *)(ctx);
    }

    /* open file */
    ctx->iod = fioh.ioh_open((char*)fname, mode, 0);
    if (!ctx->iod) {
	return NULL;
    }

    return (void *)(ctx);
}

int32_t
mfp_safe_io_attach(mfp_safe_io_desc_t *siod, mfp_safe_io_ctx_t *sioc)
{
    if ( !sioc || !siod) {
	return E_MFP_SAFE_IO_ATTACH_FAIL;
    }

    siod->sioc = sioc;
    return 0;
}

int32_t
mfp_safe_io_reserve_size(mfp_safe_io_desc_t *siod, size_t size)
{
    int32_t rv, macro_ret;
    size_t avl_size;
    mfp_safe_io_ctx_t *sioc;

    rv = 0;

    sioc = siod->sioc;

    /* acquire context level lock; update stats and release lock
     * since the is a decision loop we need a lock and cannot be done
     * purely using Atomic Ops
     */
    PTHREAD_MUTEX_LOCK(&sioc->mutex);
    avl_size = AO_load(&sioc->size_reserved) -
	AO_load(&sioc->size_written);
    if (avl_size >= size) {
	AO_fetch_and_add(&sioc->size_written, size);
	DBG_MFPLOG("SAFEIO", MSG, MOD_MFPLIVE,
		   "CTXT:thread id %lx, tot size used %ld, tot size"
		   " reserved %ld\n", pthread_self(),
		   AO_load(&sioc->size_written),
		   AO_load(&sioc->size_reserved));
    } else {
	DBG_MFPLOG("SAFEIO", MSG, MOD_MFPLIVE,
		   "CTXT: thread id %lx, no more space\n", pthread_self());
	rv = -E_MFP_SAFE_IO_NOSPACE;
    }
    siod->size_reserved = size;
    PTHREAD_MUTEX_UNLOCK(&sioc->mutex);

    return rv;    
}

int32_t
mfp_safe_io_return_size(mfp_safe_io_desc_t *siod, size_t size)
{
    mfp_safe_io_ctx_t *sioc;
    mfp_mnt_point_stats_t *mnt_st;
    int32_t macro_ret;

    sioc = siod->sioc;
    PTHREAD_MUTEX_LOCK(&sioc->mutex);
    if (size > sioc->size_reserved) {
	assert(0);
    }
    
    AO_fetch_and_add(&sioc->size_written, -size);
    if (sioc->path[0] != '\0') {
	PTHREAD_MUTEX_LOCK(&mnt_point_list_lock);
	mnt_st =\
	    (mfp_mnt_point_stats_t*)g_hash_table_lookup(mnt_point_list,
							sioc->path);
	AO_fetch_and_add(&mnt_st->size_avl, size);
    }
    /* if we are returning the entire reserved size back, then we need
     * to stop partial returns when closing the descriptor
     */
    if (size == siod->size_reserved) {
	siod->size_written = siod->size_reserved;
    }
    if (sioc->path[0] != '\0') PTHREAD_MUTEX_UNLOCK(&mnt_point_list_lock);
    PTHREAD_MUTEX_UNLOCK(&sioc->mutex);
    
    return 0;
}

size_t
mfp_safe_io_get_free_space(mfp_safe_io_ctx_t *sioc)
{
    return (size_t) (AO_load(&sioc->size_reserved) - \
		     AO_load(&sioc->size_written));
}

size_t 
mfp_safe_io_write(void *buf, size_t size, size_t nelem, void *desc)
{
    mfp_safe_io_desc_t *siod;
    size_t rv;
    AO_t avl_size;

    avl_size = 0;
    siod = (mfp_safe_io_desc_t*)desc;
    
    /* compute available size for this descriptor and do bounds check
     */
    AO_fetch_and_add(&avl_size, siod->size_reserved -	\
		     siod->size_written);
    if (AO_load(&avl_size) < (size * nelem)) {
	//printf("DESC: thread id %lx, no more space, avail space %ld, write" 
	//     " size %ld\n", pthread_self(), AO_load(&avl_size), 
	//     size * nelem);
	return -E_MFP_SAFE_IO_NOSPACE;
    }

    /* write data and update stats */    
    rv = fioh.ioh_write(buf, size, nelem, siod->iod);
    AO_fetch_and_add(&siod->size_written, rv * size);

    //printf("DESC: thread id %lx, tot size used %ld, tot size reserved"
    ///   " %ld\n", pthread_self(), 
    //   AO_load(&siod->size_written),
    //   AO_load(&siod->size_reserved));

    return rv;
}

void
mfp_safe_io_clean_ctx(mfp_safe_io_ctx_t *sioc)
{
    int32_t macro_ret;
    
    if (sioc) {
	PTHREAD_MUTEX_DESTROY(&sioc->mutex);
	free(sioc);
    }

}

void
mfp_safe_io_clean_desc(mfp_safe_io_desc_t *siod)
{

    if (siod) {
	if (siod->size_written < siod->size_reserved) {
	    mfp_safe_io_return_size(siod,
				    siod->size_reserved - 
				    siod->size_written);
	}	
	if (siod->iod) {
	    fclose(siod->iod);
	}
	siod->sioc = NULL;
	free(siod);
    }
	
}

static int32_t
mfp_safe_io_reserve_from_mntlist(const char *const mount_path, 
				 size_t size_reserved,
				 float use,
				 mfp_safe_io_ctx_t *ret)
{
    mfp_safe_io_ctx_t *ctx;
    mfp_mnt_point_stats_t *mnt_st;
    size_t size_requested;
    int32_t rv, macro_ret;

    size_requested = 0;
    ctx = ret;

    PTHREAD_MUTEX_LOCK(&mnt_point_list_lock);
    mnt_st = \
	(mfp_mnt_point_stats_t*)g_hash_table_lookup(mnt_point_list,
						    mount_path);
    PTHREAD_MUTEX_UNLOCK(&mnt_point_list_lock);
    if (!mnt_st) {
	return -E_MFP_SAFE_IO_EMOUNT_POINT;
    }
    
    /* grab some space from the mount point's resource pool. all
     * operations under a global lock
     */
    PTHREAD_MUTEX_LOCK(&mnt_st->mutex);    
    snprintf(ctx->path, 256, "%s", mount_path);
    if (!size_reserved) {
	size_requested = floor(use * mnt_st->size_reserved);
    } else {
	size_requested = size_reserved;
    }

    if (size_requested >= mnt_st->size_avl) {
	AO_fetch_and_add(&mnt_st->size_avl, -size_requested);
	ctx->size_reserved = size_requested;
    } else {
	rv = -E_MFP_SAFE_IO_NOSPACE;
	PTHREAD_MUTEX_UNLOCK(&mnt_st->mutex);
	goto err;
    }
    PTHREAD_MUTEX_UNLOCK(&mnt_st->mutex);

    return 0;

 err:
    if (ctx) free(ctx);
    return rv;
}

static int32_t
mfp_safe_io_set_local_reservn(size_t size_reserved, float use,
			      mfp_safe_io_ctx_t *ret)
{
    mfp_safe_io_ctx_t *ctx = ret;
    int32_t macro_ret, err = 0;
    
    PTHREAD_MUTEX_LOCK(&ctx->mutex);
    if (!size_reserved) {
	ctx->size_reserved = floor(use * size_reserved);
    } else {
	ctx->size_reserved = size_reserved;
    }
    PTHREAD_MUTEX_UNLOCK(&ctx->mutex);

    return err;
}

void *
sio_fopen(char *p_data, const char *mode, size_t size)
{
    size = size;
    return (void *)mfp_safe_io_open(p_data, mode);
}

size_t
sio_fseek(void *desc, size_t seek, size_t whence)
{
    mfp_safe_io_desc_t *siod = (mfp_safe_io_desc_t *)desc;
    FILE *f = (FILE*)siod->iod;

    return fseek(f, seek, whence);
}

size_t
sio_ftell(void *desc)
{
    mfp_safe_io_desc_t *siod = (mfp_safe_io_desc_t *)desc;                                                                                                                                                       FILE *f = (FILE*)siod->iod; 

    return ftell(f);
}

void
sio_fclose(void *desc)
{
    mfp_safe_io_desc_t *siod = (mfp_safe_io_desc_t *)desc;
    FILE *f = (FILE*)siod->iod; 

    mfp_safe_io_clean_desc(siod);
 
    return;
}

size_t
sio_fwrite(void *buf, size_t n, size_t size, void *desc)
{
    mfp_safe_io_desc_t *siod = (mfp_safe_io_desc_t *)desc;
    FILE *f = (FILE*)siod->iod;

    return mfp_safe_io_write(buf, n, size, siod);
}

size_t
sio_fread(void *buf, size_t n, size_t size, void *desc)
{
    FILE *f = (FILE*)desc;

    return fread(buf, n, size, f);
}

