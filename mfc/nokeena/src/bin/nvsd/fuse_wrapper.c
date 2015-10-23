#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <stdint.h>
#include <dlfcn.h>
#include <time.h>
#include <stdint.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/prctl.h>

#include "nvsd_mgmt.h"
#include "nkn_debug.h"
#include "nkn_defs.h"
#include "nkn_cfg_params.h"
#include "nkn_sched_api.h"
#include "server.h"
#include "nkn_stat.h"
#include "nkn_trace.h"
#include "rtsp_session.h"
#include "vfs_defs.h"
#include "http_header.h"

#define USE_NKN_FUSE

#ifdef USE_NKN_FUSE

#define FUSE_USE_VERSION 26
#include "fuse.h"
#include "fuse_opt.h"
#include "fuse_lowlevel.h"
#include "fuse_common_compat.h"

//config declarations
const char *fuse_mnt_path = "/nkn/mnt/fuse"; //mount path
const char *bin_name = "/opt/nkn/sbin/nvsd"; //binary path and name


// container for the fuse context
typedef struct _tag_pnkn_fuse_context{
    unsigned int uid;
    unsigned int gid;
    struct fuse_chan *fc;
}nkn_fuse_context;

typedef struct {
	char *name;
	mode_t mode;
	size_t size;		// after last write
} nkn_fuse_entry_t;

static nkn_fuse_entry_t nkn_fuse_tmp;		// TBD - make hash table
static int nkn_fuse_debug = 0;
static int nkn_fuse_keepopen = 1;

AO_t glob_fuse_openfds;
unsigned long long glob_fuse_bytes_read = 0;

extern struct vfs_funcs_t vfs_funs;
static pthread_mutex_t read_lock;
//static pthread_mutex_t fuse_lock;
/*******************************************************************
/                          PROTOTYPES
*******************************************************************/
void* fuse_wrapper_initialize(void*);
int fuse_wrapper_unmount(void);
char* get_uri_prefix(char *);

struct fuse *fuse_new_common(struct fuse_chan *ch, struct fuse_args *args,
                             const struct fuse_operations *op,
                             size_t op_size, void *user_data, int compat);

int fuse_sync_compat_args(struct fuse_args *args);

struct fuse_chan *fuse_kern_chan_new(int fd);

struct fuse_session *fuse_lowlevel_new_common(struct fuse_args *args,
					      const struct fuse_lowlevel_ops *op,
					      size_t op_size, void *userdata);

void fuse_kern_unmount_compat22(const char *mountpoint);
void fuse_kern_unmount(const char *mountpoint, int fd);
int fuse_kern_mount(const char *mountpoint, struct fuse_args *args);
void fuse_teardown_common(struct fuse *fuse, char *mountpoint);


#ifdef VFS_AIO_UNITTEST
pthread_mutex_t aio_unit_test_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  aio_unit_test_cond;
int read_callback(void *aio_data);
int aio_read_call(char * p_data, int size, size_t nmemb, FILE *stream);
#endif
/********************************************************************
 *             UTILITY FUNCTIONS
 *
 ********************************************************************/

/** GLEAN URI PREFIX FROM FULL PHYSICAL PATH
 *  @param path - full path to the file
 *  @return returns null terminated string containing the URI prefix
 */
char *
get_uri_prefix(char *path)
{
	char *uri;
	int len;

	uri = NULL;
	len = 0;

	len = strlen(path);
	if(len == 0){
		return NULL;
    }

    return uri+len+1;

}

/******************************************************************************
/                          FUSE BACKEND FUNCTIONS
******************************************************************************/

/** Hook function to read content from NKN CACHE
 *@param path - char pointer to the physical path
 *@param p_data - data buffer - to be filled
 *@param size - size of data to be read into the buffer
 *@param offset - offset to the start of requested data
 *@param fi - private data structure of FUSE
*/
static int
fuse_wrapper_read(const char *path,
		  char *p_data,
		  size_t size,
		  off_t offset,
		  struct fuse_file_info *fi)
{

    FILE * fp;
    int ret;

   //    uri = get_uri_prefix((char*)path);
   // pthread_mutex_lock(&read_lock);
   if (nkn_fuse_keepopen)
	fp =  (FILE*)(fi->fh);
   else
	fp = vfs_funs.pnkn_fopen(path+1, "R");
    if(!fp) {
	errno = EBADF;
	fi->fh = 0;
//        pthread_mutex_unlock(&read_lock);
	return -1;
    }

    ret = vfs_funs.pnkn_fseek(fp, offset, SEEK_SET);
    if(ret  == -1) {
//        pthread_mutex_unlock(&read_lock);
	return -1;
    }


#ifdef VFS_AIO_UNITTEST
    pthread_cond_init(&aio_unit_test_cond, NULL);
    ret = aio_read_call(p_data, 1, size, fp);
#else
    ret = vfs_funs.pnkn_fread(p_data, 1, size, fp);
#endif
    if (ret > 0)
	glob_fuse_bytes_read += ret;
    // printf("offset %ld size %ld ret=%d\n", offset, size, ret);
    DBG_LOG(MSG, MOD_FUSE, "FUSE %s returned %d", path, ret);
   if (!nkn_fuse_keepopen)
	vfs_funs.pnkn_fclose(fp);
//    pthread_mutex_unlock(&read_lock);
    return ret;
}

/** Hooke function to return attributes for files and folders
 *@param path - char pointer to the physical path
 *@param stbuf - pointer to the linux stat structure that needs to be populated
 *@returns '0' for  no error
 */
static int
fuse_wrapper_getattr(const char *path, struct stat *stbuf)
{
    int rv;

    rv = 0;

    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
	stbuf->st_mode = S_IFDIR | 0755;
	stbuf->st_nlink = 2;
	return 0;
    }

    // Temporary hack for directories.  All basenames without a .suffix are
    // treated as a directory
    if (!strstr(path, ".")) {
	stbuf->st_mode = S_IFDIR | 0755;
	stbuf->st_nlink = 2;
	return 0;
    }

    //pthread_mutex_lock(&read_lock);
    rv = vfs_funs.pnkn_stat(path+1, stbuf);
    //pthread_mutex_unlock(&read_lock);

    if(rv == 0) {
       return 0;
    } else if (nkn_fuse_tmp.name && !strcmp(nkn_fuse_tmp.name, path)) {
	stbuf->st_mode = S_IFREG | nkn_fuse_tmp.mode;
	stbuf->st_nlink = 1;
    } else {
	return -ENOENT;
    }


    return 0;
}

static int fuse_wrapper_opendir(const char *path, struct fuse_file_info *fi)
{
	UNUSED_ARGUMENT(fi);
	UNUSED_ARGUMENT(path);
	return  -ENOENT;
}



static int fuse_wrapper_open(const char *path, struct fuse_file_info *fi)
{

    FILE *fp;

    UNUSED_ARGUMENT(fi);


    //pthread_mutex_lock(&read_lock);
    fp = NULL;

    if (fi->flags & O_SYNC)
	fp = vfs_funs.pnkn_fopen(path+1, "I");
    else
	fp = vfs_funs.pnkn_fopen(path+1, "K");
    fi->fh = (uint64_t)(fp);

    // printf("FILE handle %ld\n", fi->fh);
    if(!fp){
	DBG_LOG(MSG, MOD_FUSE, "Invalid Path %s", path);
	return -EACCES;
    }

   DBG_FUSE(" Opened uri : %s", path);

   if ((fi->flags & 3) != O_RDONLY) {
	 vfs_funs.pnkn_fclose(fp);
	 return -EACCES;
   }
   // pthread_mutex_unlock(&read_lock);
   if (!nkn_fuse_keepopen)
	vfs_funs.pnkn_fclose(fp);

   AO_fetch_and_add1(&glob_fuse_openfds);
   return 0;
}


static int fuse_wrapper_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi)
{

    (void) offset;
    (void) fi;


    if (strcmp(path, "/") != 0)
	return -ENOENT;

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    filler(buf, path + 1, NULL, 0);

    return 0;
}

static int
fuse_wrapper_write(const char *path,
		   const char *p_data,
		   size_t size,
		   off_t offset,
		   struct fuse_file_info *fi)
{
	if (nkn_fuse_debug)
		printf("FUSE write called %s %s %ld %ld %lx\n", path, p_data, size, offset,
		       (uint64_t)fi);
	if (!nkn_fuse_tmp.name || strcmp(nkn_fuse_tmp.name, path))
		return -ENOENT;
	if ((offset+size) > nkn_fuse_tmp.size)
		nkn_fuse_tmp.size = offset+size;
	return size;
}

static int fuse_wrapper_release(const char *path,  struct fuse_file_info *fi)
{
	FILE * fp;

	if (nkn_fuse_debug)
		printf("FUSE release called %s %lx\n", path, (uint64_t)fi);
	fp = (FILE *)fi->fh;
	if (nkn_fuse_keepopen)
		vfs_funs.pnkn_fclose(fp);
	if (nkn_fuse_tmp.name && !strcmp(nkn_fuse_tmp.name, path)) {
		printf("File %s done, size = %ld\n", path, nkn_fuse_tmp.size);
		free(nkn_fuse_tmp.name);
		nkn_fuse_tmp.name = NULL;
	}
	AO_fetch_and_sub1(&glob_fuse_openfds);
	return 0;
}

static int fuse_wrapper_create(const char *path,  mode_t mode, struct fuse_file_info *fi)
{
	if (nkn_fuse_debug)
		printf("FUSE create called %s %d %lx\n", path, (int)mode, (uint64_t)fi);
	if (nkn_fuse_tmp.name != NULL)
		return -EBUSY;
	nkn_fuse_tmp.name = nkn_strdup_type(path, mod_fuse_strdup1);
	if (nkn_fuse_tmp.name == NULL)
		return -ENOMEM;
	nkn_fuse_tmp.mode = mode;
	nkn_fuse_tmp.size = 0;
	return 0;
}

static int fuse_wrapper_truncate(const char *path, off_t offset)
{
	if (nkn_fuse_debug)
		printf("FUSE truncate called %s %ld\n", path, offset);
	return 0;
}

static void *fuse_wrapper_init(struct fuse_conn_info *ci)
{
	ci->async_read = 0;
	// ci->max_readahead = 0;
	return NULL;
}

static struct fuse_operations fuse_oper = {
    .getattr        = fuse_wrapper_getattr,
    .readdir        = fuse_wrapper_readdir,
    .open           = fuse_wrapper_open,
    .read           = fuse_wrapper_read,
    .write	    = fuse_wrapper_write,
    .release	    = fuse_wrapper_release,
    .truncate	    = fuse_wrapper_truncate,
    .create	    = fuse_wrapper_create,
    .opendir	    = fuse_wrapper_opendir,
    .init	    = fuse_wrapper_init
};

#if 0
static struct fuse_chan *fuse_mount_common(const char *mountpoint,
                                           struct fuse_args *args)
{
    struct fuse_chan *ch;
    int fd;

    /*
     * Make sure file descriptors 0, 1 and 2 are open, otherwise chaos
     * would ensue.
     */
    do {
	fd = open("/dev/null", O_RDWR);
	if (fd > 2)
	    close(fd);
    } while (fd >= 0 && fd <= 2);

    fd = fuse_mount_compat25(mountpoint, args);
    if (fd == -1)
	return NULL;

    ch = fuse_chan_new(fd);
    if (!ch)
	fuse_unmount(mountpoint, fd);

    return ch;
}

#endif
static void
fuse_unmount_common(const char *mountpoint, struct fuse_chan *ch)
{
    //struct fuse_channel *fd = ch ? fuse_chan_fd(ch) : NULL;
    fuse_unmount(mountpoint, ch);
    fuse_chan_destroy(ch);
}


void
fuse_teardown_common(struct fuse *fuse, char *mountpoint)
{
    struct fuse_session *se = fuse_get_session(fuse);
    struct fuse_chan *ch = fuse_session_next_chan(se, NULL);

    //    pthread_mutex_lock(&fuse_lock);
    fuse_remove_signal_handlers(se);

    fuse_unmount_common(mountpoint, ch);

    fuse_exit(fuse);
    fuse_destroy(fuse);
    //pthread_mutex_unlock(&fuse_lock);

    free(mountpoint);
}



nkn_fuse_context *ctx;
struct fuse *fh;
#define MAX_RETRIES 3

/************************************************************************
/                    API TO INITIAZLIZE FUSE
***********************************************************************/

/** Iniitializes the FUSE system. Creates a FUSE context and runs the FUSE
 *  event loop. Needs to be called in a thread. THIS FUNCTION BLOCKS THE CALLER
 */
void* fuse_wrapper_initialize(void *pd)
{

    int err_no, retry_cnt, ret;
    struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
    struct stat stbuf;

    UNUSED_ARGUMENT(pd);
    UNUSED_ARGUMENT(stbuf);
    err_no = 0;
    retry_cnt = 0;
    fh = NULL;

    prctl(PR_SET_NAME, "nvsd-fuse", 0,0, 0);
    pthread_mutex_init(&read_lock, NULL);

    AO_store(&glob_fuse_openfds, 0);
    ret = fuse_opt_add_arg(&args, bin_name);
    ret |= fuse_opt_add_arg(&args, "-o");
    //    ret |= fuse_opt_add_arg(&args, "nonempty,ro,blksize=131072,max_read=131072,fsname=fuseblk");
    ret |= fuse_opt_add_arg(&args, "nonempty,direct_io,ro,fsname=fuseblk");

    if (ret == -1)
	goto cleanup;

    ctx = (nkn_fuse_context*)nkn_calloc_type(1, sizeof(nkn_fuse_context), mod_fuse_context_t);

    *ctx = (nkn_fuse_context){
	.uid = getuid(),
	.gid = getgid(),
    };

    if(lstat(fuse_mnt_path, &stbuf) == -1) { //bz 2592. fix the transport endpoint not connected error message
	umount2(fuse_mnt_path, 2);
    }

 retry:
    ctx->fc = fuse_mount(fuse_mnt_path, &args);
    if (!ctx->fc){
	if(retry_cnt < MAX_RETRIES){
	    //do a lazy unmount and try again
	    umount2(fuse_mnt_path, 0);
	    retry_cnt++;
	    goto retry;
	}else{
	    goto cleanup;
	}
    }

    fh = fuse_new(ctx->fc, &args,  &fuse_oper, sizeof(fuse_oper), NULL);
    if(!fh){
	goto unmount;
    }

    if (fuse_set_signal_handlers(fuse_get_session(fh))){
	goto destroy;
    }

    fuse_loop_mt(fh);
    fuse_teardown_common(fh, (char*)fuse_mnt_path);
    pthread_exit(NULL);

 destroy:
    fuse_destroy(fh);
    fh = NULL;
 unmount:
    fuse_unmount_common(fuse_mnt_path, ctx->fc);
 cleanup:
    fuse_opt_free_args(&args);
    pthread_exit(NULL);
    free(ctx); //bz 2594
    return NULL;

}

#endif // USE_NKN_FUSE

int nkn_fuse_exit(void);
int nkn_fuse_exit(void)
{
    if(fuse_enable == 0) return 0;


#ifdef USE_NKN_FUSE
    fuse_teardown_common(fh, (char*)fuse_mnt_path);
    free(ctx); //bz 2594
    return 0;

    fuse_exit(fh);
    fuse_destroy(fh);
    fuse_unmount(fuse_mnt_path, ctx->fc);
#endif // USE_NKN_FUSE

    return 0;
}

void nkn_fuse_init(void);

void nkn_fuse_init(void)
{
	if(fuse_enable == 0) return;

#ifdef USE_NKN_FUSE
	pthread_t th_fuse;
	long th_fuse_id;

	pthread_create(&th_fuse, NULL, fuse_wrapper_initialize, &th_fuse_id);// Initialize fuse - mount fuse path
#endif // USE_NKN_FUSE

}


#ifdef VFS_AIO_UNITTEST
int glob_test_wait = 0;
int glob_data_read;
int read_callback(void *aio_data)
{
    DBG_LOG(MSG, MOD_FUSE, "readcallback");
    nkn_vfs_aio_t *t = (nkn_vfs_aio_t *)aio_data;
    if (!aio_data) {
        DBG_LOG(MSG, MOD_FUSE, "aio_data is null");
        glob_test_wait = 0;
        return 0;
    }
    DBG_LOG(MSG, MOD_FUSE, "aio test : %d %d %d %d",t->size,t->nmemb,t->retval,errno);
    glob_data_read = t->retval;
    free(t);
    pthread_mutex_lock(&aio_unit_test_mutex);
    glob_test_wait = 0;
    pthread_cond_signal(&aio_unit_test_cond);
    pthread_mutex_unlock(&aio_unit_test_mutex);
    return 0;
}

int aio_read_call(char * p_data, int size, size_t nmemb, FILE *stream)
{
    int ret =0;
    nkn_vfs_aio_t *t = (nkn_vfs_aio_t *)nkn_calloc_type(1, sizeof(nkn_vfs_aio_t), mod_fuse_context_t);
    t->size = size;
    t->nmemb = nmemb;
    t->data_ptr = p_data;
    t->aio_callback = read_callback;
    glob_test_wait = 1;
    ret = vfs_funs.pnkn_fread_aio(stream, t);
    if (!ret) {
	pthread_mutex_lock(&aio_unit_test_mutex);
	if (glob_test_wait) {
		pthread_cond_wait(&aio_unit_test_cond, &aio_unit_test_mutex);
	}
	pthread_mutex_unlock(&aio_unit_test_mutex);
	ret = glob_data_read;
    }
    return ret;
}
#endif
