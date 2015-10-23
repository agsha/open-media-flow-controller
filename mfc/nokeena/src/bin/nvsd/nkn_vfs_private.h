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
#include <stdint.h>
#include <dlfcn.h>
#include <time.h>
#include <stdint.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <math.h>

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
#include "nkn_cod.h"
#include "nkn_pseudo_con.h"
#include "mime_header.h"



NKNCNT_DEF(vfs_tot_task, int32_t, "", "Total RTSP scheduler task")
NKNCNT_DEF(vfs_open_cache, uint32_t, "", "open cache")
NKNCNT_DEF(vfs_read_cache, uint32_t, "", "read cache")
NKNCNT_DEF(vfs_close_cache, uint32_t, "", "close cache")
NKNCNT_DEF(vfs_err_nomem, uint32_t, "", "No memory")
NKNCNT_DEF(vfs_out_bytes, uint64_t, "bytes", "Total RTSP deliver bytes")
NKNCNT_DEF(vfs_open_task, uint32_t, "", "open rtsp task")
NKNCNT_DEF(vfs_cod_open, uint32_t, "", "open cod")
NKNCNT_DEF(vfs_cod_open_fail, uint32_t, "", "open cod")
NKNCNT_DEF(vfs_cod_close, uint32_t, "", "close cod")

#  define swap_uint64(x) ((uint64_t)((((x) & 0x00000000000000FFU) << 56)  | \
                                     (((x) & 0x000000000000FF00U) << 40)  | \
                                     (((x) & 0x0000000000FF0000U) >> 24)  | \
                                     (((x) & 0x00000000FF000000U) << 8)   | \
                                     (((x) & 0x000000FF00000000U) >> 8)   | \
                                     (((x) & 0x0000FF0000000000U) >> 24)  | \
                                     (((x) & 0x00FF000000000000U) >> 40)  | \
                                     (((x) & 0xFF00000000000000U) >> 56)))

#  define swap_uint32(x) ((uint32_t)((((x) & 0x000000FFU) << 24) | \
                                   (((x) & 0x0000FF00U) << 8)  | \
                                   (((x) & 0x00FF0000U) >> 8)  | \
                                   (((x) & 0xFF000000U) >> 24)))

#  define swap_uint16(x) ((uint16_t)((((x) & 0x00FFU) << 8) | \
                                   (((x) & 0xFF00U) >> 8)))




#define MAX_FS_FILESIZE         (2*GBYTES)
#define ONE_RTSP_CHUNK		(2 * MiBYTES)
#define MAX_TASKID (16)
#define BM_IOV_MAX (64)   // same as BM
#define MOOV_ID 0x766f6f6d
#define MDAT_ID 0x7461646d
#define BM_PAGE_SIZE 32768
#define BM_PAGE_SHIFT 15
#define MAX_TEMP_TASK 1024

#define RTSP_SET_FLAG(x, f)       (x)->flag |= (f);
#define RTSP_UNSET_FLAG(x, f)     (x)->flag &= ~(f);
#define RTSP_CHECK_FLAG(x, f)     ((x)->flag & (f))

#define nkn_vfs_cache_s sizeof(struct nkn_vfs_cache)
// cache flags
#define RTSP_FLAG_CACHE		1
#define RTSP_FLAG_DIRECT	2	// Do not cache reads
#define RTSP_FLAG_META_DATA	4	// flag indicates data or meta
					// data
// task flags
#define RTSP_FLAG_TASK_DONE 1
#define RTSP_INTERNAL_TASK  2
#define RTSP_FLAG_NOAMHITS  4       // do not read attr
#define RTMP_FUSE_TASK      8

typedef struct nkn_vfs_mdt_obj {
	char			**data_pages;
	off_t                   content_pos;
	off_t                   content_size;
	char			*filename;
	uint32_t		ref_cnt;
} nkn_vfs_mdt_obj_t;

typedef struct nkn_vfs_con {
	int			flag;
	pthread_mutex_t		mutex;
	pthread_cond_t		cond;
	cache_response_t	*cptr;
	int			taskindex;
	nkn_task_id_t		*nkn_taskid;
	con_t			*con;
	char			*uri;
	nkn_cod_t		cod;
	namespace_token_t	ns_token;
	int			error;
} nkn_vfs_con_t;

typedef struct nkn_vfs_cache {
	uint16_t		flag;
	uint16_t		eof;
	uint32_t 		content_size;
	char			**data_pages;
	off_t			content_pos;
	off_t			cur_pos;
	off_t			tot_file_size;
	char			*filename;
	pthread_mutex_t		mutex;
	nkn_vfs_con_t		*con_info;
	uint64_t		provider;
	struct nkn_vfs_cache	*meta_data;
} nkn_vfs_cache_t;


// ghash for storing meta data
static GHashTable *md_table;
static pthread_mutex_t mdt_lock;

// meta table init
static void vfs_md_init(void);
// create task data
static int vfs_create_task_data(nkn_vfs_con_t *con_info);
// create http con data
static int vfs_create_http_con(nkn_vfs_con_t *con_info,
			       char *domain,
			       char *filename, int proto_sw);
// read fs cache
static off_t vfs_read_fs_cache(nkn_vfs_cache_t * pfs,
			       off_t pos, off_t reqlen,
			       off_t (*copy_function)(nkn_vfs_cache_t *pfs,
			       cache_response_t *cptr));
// copy data pointer from BM dont release
static off_t copy_to_mem(nkn_vfs_cache_t *pfs, cache_response_t *cptr);
// post a task
static void vfs_post_sched_a_task(nkn_vfs_con_t * con_info,
				  off_t tot_file_size,
				  off_t offset_requested,
				  off_t length_requested);
// free task buffers
static void free_buffer_pointers(nkn_task_id_t *nkn_taskid, int taskindex);
// clean up a task
static void vfs_cleanup_a_task(nkn_task_id_t task_id);
// clean up vfs task
static void vfs_mgr_cleanup(nkn_task_id_t id);
// vfs input call
static void vfs_mgr_input(nkn_task_id_t id);
// vfs output call
static void vfs_mgr_output(nkn_task_id_t id);
// copy from cache to client
static void copy_iovec_to_user(nkn_vfs_cache_t *pfs,
			       char **data_ptr, off_t csize);
// mp4 meta data read
static int nkn_read_mp4_metadata(nkn_vfs_cache_t *pfs);
// get4Val
int get4Val(nkn_vfs_cache_t *pfs, off_t diff, off_t readsize,
	    char **page, off_t *j, uint64_t *u64bit);
// get8val
int get8Val(nkn_vfs_cache_t *pfs, off_t diff, off_t readsize,
	    char **page, off_t *j, uint64_t *u64bit);
// metadata read
static off_t nkn_metadata_read(nkn_vfs_cache_t *pfs, off_t size,
			    off_t rmemb, char **data_ptr,
			    off_t *pos);
// metadata ext read
static int nkn_metadata_read_ext(nkn_vfs_cache_t *pfs, char *ext);

/* *****************************************************
 * RTSP virtual file system
 * virtual file system APIs
 ******************************************************* */

// direct/indirect file open
static nkn_provider_type_t vfs_get_last_provider(FILE *stream);
static FILE *nkn_fopen(const char *path, const char *mode);
static void nkn_clearerr(FILE *stream);
static int nkn_feof(FILE *stream);
static int nkn_ferror(FILE *stream);
static int nkn_fileno(FILE *stream);
static int nkn_fseek(FILE *stream, long offset, int whence);
static long nkn_ftell(FILE *stream);
static void nkn_rewind(FILE *stream);
static int nkn_fgetpos(FILE *stream, fpos_t *pos);
static int nkn_fsetpos(FILE *stream, fpos_t *pos);
static size_t nkn_fread(void *ptr,
			size_t size, size_t nmemb, FILE *stream);
static size_t nkn_fwrite(const void *ptr,
			 size_t size, size_t nmemb, FILE *stream);
static off_t nkn_ftello(FILE *stream);
static int nkn_fseeko(FILE *stream, off_t offset, int whence);
static int nkn_fputc(int c, FILE *stream);
static int nkn_fputs(const char *s, FILE *stream);
static int nkn_fgetc(FILE *stream);
static char *nkn_fgets(char *s, int size, FILE *stream);
static int nkn_fclose(FILE *fp);
static int nkn_fflush(FILE *stream);

extern void rtspsvr_init(void);

void (* pvfs_new_socket)(int fd) = NULL;
