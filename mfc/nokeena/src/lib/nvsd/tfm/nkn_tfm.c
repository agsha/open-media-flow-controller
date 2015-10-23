/*
  COPYRIGHT: NOKEENA NETWORKS
  AUTHOR: Vikram Venkataraghavan
*/
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#ifdef HAVE_FLOCK
#  include <sys/file.h>
#endif
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <sys/uio.h>
#include <pthread.h>
#include <glib.h>
#include "nkn_tfm.h"
#include "nkn_tfm_api.h"
#include "nkn_mediamgr_api.h"
#include "nkn_diskmgr2_api.h"
#include "nkn_defs.h"
#include "nkn_trace.h"
#include "http_header.h"
#include "fqueue.h"
#include "nkn_debug.h"
#include "nkn_am_api.h"
#include "nkn_cfg_params.h"
#include "nkn_http.h"
#include "nkn_cod.h"
#include "nkn_assert.h"
#include <sys/vfs.h>
#include <sys/statvfs.h>

#define NKN_TFM_MAX_CACHES 1
#define NO_FILE_STR "no_file_name_assigned\0"
#define NKN_TFM_BLK_LEN (2 * 1024 *1024)
int glob_tfm_initialized = 0;
int tfm_partial_file_time_limit = 20;
int tfm_partial_file_thrd_time_limit = 5;

/* File list related defines */
/* File id list related defines. The list is used to maintain the free
 * file_id to be reused to avoid running out of memory because of the dentry
 * created by linux for unique file names
 */
typedef struct tfm_file_id_list {
    int file_id;
    STAILQ_ENTRY(tfm_file_id_list) entries;
}tfm_file_id_list;
static pthread_mutex_t nkn_tfm_file_id_mutex;
STAILQ_HEAD(nkn_tfm_file_id_list_head, tfm_file_id_list) nkn_tfm_file_id_list_q;
tfm_file_id_list *nkn_tfm_file_id_list_ptr;
/* Limit the number of files to 25K based on 250MB/10K size objects */
int tfm_max_num_files_limit = 25000;
uint64_t glob_tfm_max_num_parallel_files_exceeded = 0;

uint64_t glob_tfm_promote_thread_called = 0;
uint64_t glob_tfm_promote_thread_too_many_promotes = 0;
uint64_t glob_tfm_max_partial_files = 25;
uint64_t glob_tfm_max_complete_files = 25;
uint64_t glob_tfm_max_file_size = 1024 * 1024 * 10;	/* 10 MB */
uint64_t tfm_lo_thresh_partial_files = 100;
int64_t glob_tfm_promote_files_promote_start = 0;
uint64_t glob_tfm_promote_small_obj = 0;
uint64_t glob_tfm_promote_pending_err = 0;
uint64_t glob_tfm_promote_ptype_err = 0;
uint64_t glob_tfm_promote_promote_not_worthy_err = 0;
uint64_t glob_tfm_promote_alloc_err = 0;
AO_t glob_tfm_promote_files_promote_end = 0;
uint64_t glob_tfm_promote_files_promote_called = 0;
uint64_t glob_tfm_promote_obj_expired = 0;
uint64_t glob_tfm_promote_obj_err = 0;
uint64_t glob_tfm_promote_obj_attr_err = 0;
uint64_t glob_tfm_obj_expired = 0;
uint64_t glob_tfm_pht_obj_expired = 0;
uint64_t glob_tfm_promote_obj_not_found_after_complete = 0;
uint64_t glob_tfm_promote_complete_start = 0;
uint64_t glob_tfm_promote_complete_end = 0;
uint64_t glob_tfm_put_cod_invalid = 0;
uint64_t glob_tfm_get_cod_invalid = 0;
uint64_t glob_tfm_stat_cod_invalid = 0;
uint64_t glob_tfm_pht_delete_cod_invalid = 0;
uint64_t glob_tfm_promote_complete_cod_invalid = 0;
uint64_t glob_tfm_pop_new_cod_invalid = 0;
uint64_t glob_tfm_promote_cod_invalid = 0;
uint64_t glob_tfm_video_comp_cod_invalid = 0;
uint64_t glob_tfm_stat_no_cod = 0;
uint64_t glob_tfm_get_no_cod = 0;
uint64_t glob_tfm_cod_null_err = 0;
uint64_t glob_tfm_unlink_err = 0;
uint64_t glob_tfm_unlink_enoent_err = 0;

/* To control /nkn/tfm space */
int glob_tfm_spc_dir_use_percent = 80;
int glob_tfm_spc_default_mode = 0;
uint64_t glob_tfm_spc_dir_size = 0;
uint64_t glob_tfm_spc_dir_used = 0;
int glob_tfm_cli_promote_small_obj_enable = 1;
uint32_t glob_tfm_cli_promote_small_obj_size = CM_MEM_PAGE_SIZE;

/* Partial files */
int glob_tfm_pht_last_del_time = 0;
int64_t glob_tfm_spc_pht_bytes_used = 0;
int64_t glob_tfm_spc_pht_bytes_resv = 0;
int64_t glob_tfm_spc_pht_bytes_avail = 0;
uint64_t glob_tfm_spc_pht_use_percent = 50;
uint64_t glob_tfm_spc_pht_max_files_exceeded = 0;
uint64_t glob_tfm_spc_pht_max_bytes_exceeded = 0;
uint64_t glob_tfm_spc_pht_thresh_reached = 0;
int64_t glob_tfm_spc_pht_thresh = 0;
uint64_t glob_tfm_cleanup_num_files_deleted = 0;
uint64_t glob_tfm_partial_file_deletion_avoided = 0;
uint64_t glob_tfm_invalid_getm_obj = 0;
uint64_t glob_tfm_invalid_chunked_obj = 0;

/* SAC related variables */
uint64_t glob_tfm_put_sac_fail = 0;
int glob_tfm_pht_spc_sac_reject = 0;

/* Completed files */
int64_t glob_tfm_spc_cht_bytes_used = 0;
int64_t glob_tfm_spc_cht_bytes_avail = 0;
uint64_t glob_tfm_spc_cht_use_percent = 50;
uint64_t glob_tfm_spc_cht_max_files_exceeded = 0;
uint64_t glob_tfm_spc_cht_max_bytes_exceeded = 0;
int64_t glob_tfm_spc_cht_thresh = 0;
uint64_t glob_tfm_cht_buf_only_pend = 0;
uint64_t glob_tfm_cht_max_buf_only_pend = 40;

uint64_t glob_tfm_spc_use_percent = 20;

/* Promote Complete queue */
static pthread_cond_t tfm_promote_comp_thr_cond;
static pthread_mutex_t tfm_promote_comp_mutex;
STAILQ_HEAD(nkn_tfm_pmt_comp_q, TFM_promote_comp_s) nkn_tfm_promote_comp_q;

/* Promote queue */
unsigned int glob_tfm_outstanding_promote_thr = 40;
static pthread_cond_t tfm_promote_thr_cond;
static pthread_cond_t tfm_promote_thr_limit_cond;
static pthread_mutex_t tfm_promote_mutex;
static pthread_t tfm_promote_thread_obj;
uint64_t glob_tfm_promote_queue_num_obj = 0;
STAILQ_HEAD(nkn_tfm_pmt_q, TFM_promote_cfg_s) nkn_tfm_promote_q;

/* TFM Cleanup Queue */
TAILQ_HEAD(nkn_tfm_cln_q, TFM_obj_s) nkn_tfm_cleanup_q;

/* TFM Partial Promote Queue */
TAILQ_HEAD(nkn_tfm_par_prmt_q, TFM_obj_s) nkn_tfm_part_promote_q;

/* TFM URI hash table*/
static pthread_cond_t  cleanup_thr_cond;
static pthread_mutex_t nkn_tfm_cht_mutex;
static pthread_mutex_t nkn_tfm_pht_mutex;
GHashTable *nkn_tfm_completed_uri_hash_table = NULL;
GHashTable *nkn_tfm_partial_uri_hash_table = NULL;

/* TFM persistent promote hash table */
GHashTable *nkn_tfm_promote_hash_table = NULL;
GList *TFM_pending_promote_list = NULL;
GList *TFM_pending_delete_list = NULL;
GThread *tfm_async_thread;
GThread *tfm_delete_thread;
GThread *tfm_promote_comp_thread;
GThread *tfm_promote_pending_thread;
GAsyncQueue *tfm_async_q;

uint32_t glob_tfm_async_q_count;
uint64_t glob_tfm_cht_insert = 0;
uint64_t glob_tfm_pht_insert = 0;
uint64_t glob_tfm_cht_delete = 0;
uint64_t glob_tfm_promote_cht_delete = 0;
uint64_t glob_tfm_pht_delete = 0;
uint64_t glob_tfm_pht_mutex_lock_calls = 0;
uint64_t glob_tfm_cht_mutex_lock_calls = 0;
uint64_t glob_tfm_pht_mutex_cleanup_thread_lock_calls = 0;
uint64_t glob_tfm_pht_cht_mutex_lock_calls = 0;
uint64_t glob_tfm_uris_promoted = 0;
uint64_t glob_tfm_uris_promote_not_worthy = 0;
uint64_t glob_tfm_uris_promote_complete = 0;
uint64_t glob_tfm_uris_promote_complete_err = 0;
uint64_t glob_tfm_spc_cht_num_files = 0;
uint64_t glob_tfm_spc_cht_tot_num_files = 0;
uint64_t glob_tfm_num_promotes_in_progress = 0;
uint64_t glob_tfm_partial_file_limit_err = 0;
uint64_t glob_tfm_partial_file_delete = 0;
uint64_t glob_tfm_expired_file_delete = 0;
uint64_t glob_tfm_defer_delete = 0;
uint64_t glob_tfm_defered_file_delete = 0;
uint64_t glob_tfm_pobj_glib_mem = 0;
uint64_t glob_tfm_promote_pobj_glib_mem = 0;
int      glob_tfm_delete_thread_time = 0;
int glob_tfm_thread_num_files_deleted = 0;
int tfm_num_delete_chunk = 100;

uint64_t glob_tfm_getm_partial_cleanup_queue_added = 0;
uint64_t glob_tfm_getm_partial_cleanup_queue_removed = 0;
uint64_t glob_tfm_getm_partial_promote_pending_err = 0;
uint64_t glob_tfm_getm_partial_promote_get_next_ptype_err = 0;
uint64_t glob_tfm_getm_partial_promote_not_worthy_err = 0;
uint64_t glob_tfm_getm_partial_promote_uri_err = 0;
uint64_t glob_tfm_getm_partial_promote_uri_last_err = 0;
uint64_t glob_tfm_getm_promote_complete = 0;

static void s_tfm_fc_thread_hdl(void);
static void s_tfm_cleanup_thread_hdl(void);
#if 0
static void s_tfm_pending_promote_thread_hdl(void);
#endif
static void s_tfm_promote_complete_thread_hdl(void);
extern void nkn_locmgr_file_tbl_cleanup(void);
static void s_tfm_partial_promote_file(TFM_obj *pobj, int add_to_queue);

const char *tfm_queuefile = "/tmp/FMGR.queue";
int tfm_queue_fh;
int tfm_api_debug = 0;
int tfm_enqueue_retry_interval_secs = 1;
char s_tfm_promote_pending_file_name[] = "/nkn/tfm_promote_pending.txt";
char s_tfm_promote_complete_file_name[] = "/nkn/tfm_promote_complete.txt";

struct tfm_cache_info {
    char dirname[NKN_MAX_DIR_NAME_SZ];
    uint64_t cur_cache_bytes_filled; /* Needs to hold values of Gigabytes*/
    uint64_t max_cache_size;
    int disk_space_exceeded;
};

void s_print_br_list(TFM_obj *pobj);

static int s_input_cur_write_dir_idx = 0;
static struct tfm_cache_info g_cache_info[NKN_TFM_MAX_CACHES];
int glob_tfm_num_map_entries = 0;
int glob_tfm_num_locmgr_entries = 0;
int glob_tfm_num_caches = 0;
int glob_tfm_tot_map_read = 0;
int s_tfm_disk_space_exceeded = 0;
int glob_tfm_file_lseek_err = 0;
int glob_tfm_file_read_err = 0;

uint64_t glob_tfm_stat_err = 0;
uint64_t glob_tfm_stat_no_obj = 0;
uint64_t glob_tfm_stat_obj_incomplete = 0;
uint64_t glob_tfm_stat_obj_buf_only = 0;
uint64_t glob_tfm_stat_obj_expired = 0;
uint64_t glob_tfm_stat_obj_failed = 0;
uint64_t glob_tfm_stat_success_cnt = 0;
uint64_t glob_tfm_stat_not_found = 0;
uint64_t glob_tfm_get_cnt = 0;
uint64_t glob_tfm_get_err = 0;
uint64_t glob_tfm_get_too_much_data_err = 0;
uint64_t glob_tfm_get_attr_err = 0;
uint64_t glob_tfm_get_success = 0;
uint64_t glob_tfm_put_cnt = 0;
uint64_t glob_tfm_new_put_cnt = 0;
uint64_t glob_tfm_put_end = 0;
uint64_t glob_tfm_put_err = 0;
uint64_t glob_tfm_put_pending = 0;
uint64_t glob_tfm_put_complete = 0;
uint64_t glob_tfm_put_already_complete_err = 0;
uint64_t glob_tfm_put_reject = 0;
uint64_t glob_tfm_put_reject2 = 0;
uint64_t glob_tfm_put_seeks = 0;
uint64_t glob_tfm_put_opens = 0;
uint64_t glob_tfm_put_direct_opens = 0;
uint64_t glob_tfm_put_writes = 0;
uint64_t glob_tfm_put_seek_err = 0;
uint64_t glob_tfm_put_open_err = 0;
uint64_t glob_tfm_put_write_err = 0;
uint64_t glob_tfm_put_direct_write_err = 0;
uint64_t glob_tfm_put_disk_space_exceeded = 0;
uint64_t glob_tfm_put_pht_obj_not_found_after_write = 0;
uint64_t glob_tfm_put_video_not_complete = 0;
uint64_t glob_tfm_put_file_create_cnt = 0;
uint64_t glob_tfm_put_pht_buf_only = 0;
uint64_t glob_tfm_put_buf_only_reject = 0;
uint64_t glob_tot_read_from_tfm = 0;
uint64_t glob_tfm_spc_pht_num_files = 0;
uint64_t glob_tfm_spc_pht_tot_num_files = 0;
uint64_t glob_tfm_num_complete_files = 0;
uint64_t glob_tfm_obj_cleanup = 0;
uint64_t glob_tfm_num_files_open = 0;
uint64_t glob_tfm_num_files_closed = 0;
uint64_t glob_tfm_num_open_err = 0;
uint64_t glob_tfm_num_seek_err = 0;
uint64_t glob_tfm_num_read_err = 0;
uint64_t glob_tfm_num_move_files = 0;
uint64_t glob_tfm_num_other_files = 0;
uint64_t glob_tfm_file_err_delete_cnt = 0;
uint64_t glob_tfm_file_queue_cnt = 0;
uint64_t glob_tfm_init_pending_promote_started = 0;
uint64_t glob_tfm_partial_files_lo_thresh = 0;
uint64_t glob_tfm_completed_obj_already_promoted = 0;
uint64_t glob_tfm_completed_obj_not_present = 0;

static int s_merge_byte_ranges(TFM_obj *pobj,
                               GList **in_pprev, GList **mid, GList *pnext);
static int64_t s_get_content_len_from_attr(const nkn_attr_t *attr);

static time_t s_get_last_modified_from_attr(const nkn_attr_t *attr);

int s_rr_disk_caches(void);

void s_promote_hash_key_value_cleanup(gpointer data);

static int s_tfm_init_file_id_list(void);

static void s_tfm_free_file_id(int id);

static int s_tfm_get_file_id (void);

static void
s_tfm_unlink(const char *filename)
{
    if (unlink(filename) < 0) {
	glob_tfm_unlink_err++;
	if (errno == ENOENT) {
	    glob_tfm_unlink_enoent_err++;
	    DBG_LOG(MSG, MOD_TFM, "Unlink failed: %s", filename);
	}
    }
}


static void
s_init_cache_helper(char     *dirname,
                    int      index,
                    uint64_t size)
{
    int i = index;

    snprintf(g_cache_info[i].dirname, NKN_MAX_DIR_NAME_SZ, "%s", dirname);
    g_cache_info[i].cur_cache_bytes_filled = 0;
    g_cache_info[i].max_cache_size = size;
    g_cache_info[i].disk_space_exceeded = 0;
}

static int
s_tmp_init_caches(char *dirname, int size)
{
    s_init_cache_helper(dirname, 0, size);
    glob_tfm_num_caches = 1;
    return 1;
}

#if 0
static FILE *
s_open_file_for_read(char *filename)
{
    FILE *file;
    file = fopen(filename, "r");
    if (file == NULL) {
        return NULL;
    }
    return file;
}
#endif

/* Bug: 3071
 * Created a Destroy function for 
 * cleanup up the value and the key allocated
 */
void
s_promote_hash_key_value_cleanup(gpointer data)
{
    if(data)
       free(data);
}

#if 0
static void
s_tfm_handle_partial_promote_files(void)
{
    FILE *file1 = NULL;
    FILE *file2 = NULL;
    char uri1[MAX_URI_SIZE];
    char uri2[MAX_URI_SIZE];
    char *dummy;
    char *uri = NULL, *uri_dup = NULL;
    int  promote_flag = 0;
    GError *err = NULL ;
    char message[64];

    /* Initialize the diff hash table */
    nkn_tfm_uri_tbl_init_full(&nkn_tfm_promote_hash_table, s_promote_hash_key_value_cleanup,
	    s_promote_hash_key_value_cleanup);
    assert(nkn_tfm_promote_hash_table);

    file2 = s_open_file_for_read(s_tfm_promote_complete_file_name);
    if(file2) {
	while(!feof(file2)) {
	    fscanf(file2, "%s", uri2);
	    uri = nkn_strdup_type(uri2, mod_tfm_strdup3);
	    uri_dup = nkn_strdup_type(uri2, mod_tfm_strdup4);
	    if(!uri) {
		continue;
	    }
            if(!uri_dup) {
                free(uri);
                continue;
            }
            glob_tfm_promote_pobj_glib_mem += 24;
	    nkn_tfm_uri_tbl_insert(nkn_tfm_promote_hash_table,
				   uri, uri_dup);
	}
    }

    file1 = s_open_file_for_read(s_tfm_promote_pending_file_name);
    if(!file1) {
        goto cleanup;
    }
    while(!feof(file1)) {
	fscanf(file1, "%s", uri1);
	dummy = nkn_tfm_uri_tbl_get(nkn_tfm_promote_hash_table,
				    uri1);
	if(!dummy) {
	    glob_tfm_init_pending_promote_started ++;
	    promote_flag = 1;
	    uri = nkn_strdup_type(uri1, mod_tfm_strdup5);
	    TFM_pending_promote_list =
		g_list_prepend(TFM_pending_promote_list, uri);
	}
    }

    if(promote_flag == 1) {
	tfm_promote_pending_thread =
	    g_thread_create_full(
				 (GThreadFunc)
				 s_tfm_pending_promote_thread_hdl,
				 (void *)message, (128*1024), TRUE,
				 FALSE, G_THREAD_PRIORITY_NORMAL,
				 &err);
	if(tfm_promote_pending_thread == NULL) {
	    g_error_free(err);
	    goto cleanup;
	}
    }

 cleanup:
    nkn_tfm_uri_tbl_cleanup(nkn_tfm_promote_hash_table);
    nkn_tfm_promote_hash_table = NULL;
}

static int
s_tfm_write_promote_pending_file(char *uri)
{
    int ret = 0;
    int fd = -1;

    fd = open(s_tfm_promote_pending_file_name, O_RDWR | O_CREAT | O_APPEND, 0644);
    if(fd < 0) {
	DBG_LOG(MSG, MOD_TFM, "fopen failed: errno = %d filename = %s", errno,
		s_tfm_promote_pending_file_name);
	return -1;
    }
    ret = write(fd, uri, strlen(uri));
    if(ret < 0) {
	DBG_LOG(MSG, MOD_TFM, "write failed: errno = %d, filename = %s", errno,
		s_tfm_promote_pending_file_name);
        close(fd);
	return -1;
    }
    ret = write(fd, "\n", 1);
    if(ret < 0) {
	DBG_LOG(MSG, MOD_TFM, "write failed: errno = %d, filename = %s", errno,
		s_tfm_promote_pending_file_name);
        close(fd);
	return -1;
    }
    close(fd);
    return 0;
}

static int
s_tfm_write_promote_complete_file(char *uri)
{
    int ret = 0;
    int fd = -1;

    fd = open(s_tfm_promote_complete_file_name, O_RDWR | O_CREAT | O_APPEND, 0644);
    if(fd < 0) {
        DBG_LOG(MSG, MOD_TFM, "fopen failed: errno = %d filename = %s", errno,
		s_tfm_promote_complete_file_name);
	return -1;
    }

    ret = write(fd, uri, strlen(uri));
    if(ret < 0) {
	DBG_LOG(MSG, MOD_TFM, "write failed: errno = %d, filename = %s", errno,
		s_tfm_promote_complete_file_name);
        close(fd);
	return -1;
    }
    ret = write(fd, "\n", 1);
    if(ret < 0) {
	DBG_LOG(MSG, MOD_TFM, "write failed: errno = %d, filename = %s", errno,
		s_tfm_promote_pending_file_name);
        close(fd);
	return -1;
    }
    close(fd);
    return 0;
}
#endif

static TFM_obj *
s_populate_new_uri(nkn_cod_t cod,
                   char      *in_filename,
                   uint64_t  in_total_len)
{
    TFM_obj *pobj = NULL;
    char    *uriname;
    int     ret_val;

    ret_val = tfm_cod_test_and_get_uri(cod, &uriname);
    if(ret_val < 0) {
	glob_tfm_pop_new_cod_invalid ++;
	return NULL;
    }

    pobj = (TFM_obj *) nkn_calloc_type (1, sizeof(TFM_obj),
                                        mod_tfm_obj);
    if(!pobj) {
        DBG_LOG_MEM_UNAVAILABLE(MOD_TFM);
        return NULL;
    }

    pobj->inUse = 1;
    pobj->tot_content_len = in_total_len;
    pobj->cod = nkn_cod_dup(cod, NKN_COD_TFM_MGR);
    if(pobj->cod == NKN_COD_NULL) {
	glob_tfm_cod_null_err ++;
        free(pobj);
        return NULL;
    }

    if(strlen(in_filename) < sizeof(pobj->filename)) {
	memcpy(pobj->filename, in_filename, strlen(in_filename));
    } else {
	return NULL;
    }

    pobj->time_modified = nkn_cur_ts;
    /* Set the uri offset */
    glob_tfm_pobj_glib_mem += 24;
    nkn_tfm_cod_tbl_insert(nkn_tfm_partial_uri_hash_table,
			   &pobj->cod, pobj);
    glob_tfm_pht_insert ++;
    return pobj;
}

void
s_print_br_list(TFM_obj *pobj)
{
    GList *tmp_head;
    int count = 0;
    br_obj_t *br;

    if(pobj == NULL)
        return;

    tmp_head = pobj->br_list;

    while(tmp_head) {
        br = (br_obj_t *) tmp_head->data;
        printf("\n count = %d, start offset: %ld, "
               "end_offset: %ld",
               count, br->start_offset, br->end_offset);
        tmp_head = tmp_head->next;
        count ++;
    }
}

static int
s_copy_tfm_obj(TFM_obj *src, TFM_obj *dst)
{
   /* Ref count the cod so that we can reuse in the
       completed hash table. */
    dst->cod = nkn_cod_dup(src->cod, NKN_COD_TFM_MGR);
    if(dst->cod == NKN_COD_NULL) {
	glob_tfm_cod_null_err ++;
        return -1;
    }

    memcpy(dst->filename, src->filename, TFM_MAX_FILENAME_LEN - 1);
    dst->filename[TFM_MAX_FILENAME_LEN - 1] = '\0';

    if(src->attr) {
	(void) nkn_attr_copy(src->attr, &dst->attr, mod_tfm_copy_nkn_attr_t);
    }

    dst->file_id = src->file_id;
    dst->tot_content_len = src->tot_content_len;
    dst->is_complete = src->is_complete;
    dst->is_promoted = src->is_promoted;
    dst->downloaded_len = src->downloaded_len;
    dst->inUse = src->inUse;
    dst->in_bytes = src->in_bytes;
    dst->time_modified = src->time_modified;
    dst->defer_delete = src->defer_delete;
    dst->is_buf_only = src->is_buf_only;
    return 0;
}

/* This function is called with the nkn_tfm_cht_mutex lock */
void
s_cleanup_obj(gpointer data)
{
    GList               *tmp_head;
    GList               *walk;
    br_obj_t            *br;
    TFM_obj             *pobj = (TFM_obj *) data;

    assert(pobj);

    if(pobj->attr) {
        free(pobj->attr);
        pobj->attr = NULL;
    }

    walk = tmp_head = pobj->br_list;
    while(walk) {
        walk = g_list_remove_link(walk, tmp_head);
        br = (br_obj_t *)tmp_head->data;
        if(br) {
            free(br);
            br = NULL;
        }
        g_list_free1(tmp_head);
        tmp_head = walk;
    }

    /* Trouble: This is where there could be a race condition. This
     function better be called with a lock. */
    nkn_cod_close(pobj->cod, NKN_COD_TFM_MGR);
    free (pobj);
    pobj = NULL;

    glob_tfm_obj_cleanup ++;
    glob_tfm_pobj_glib_mem -= 24;
}

/* This function returns success if the video has all its data (0 to content_lenght-1)
   and has the attributes.
*/
static int
s_is_video_complete(nkn_cod_t cod)
{
    TFM_obj          *pobj = NULL;
    GList            *tmp_head;
    uint64_t         prev_start = 0, prev_end = 0;
    br_obj_t         *br;
    int              count = 0;
    char             *uri;
    int              ret_val;

    ret_val = tfm_cod_test_and_get_uri(cod, &uri);
    if(ret_val < 0) {
	assert(0);
	glob_tfm_video_comp_cod_invalid ++;
	return -1;
    }

    pobj = (TFM_obj *) nkn_tfm_cod_tbl_get(nkn_tfm_partial_uri_hash_table,
					   cod);
    if(!pobj) {
        goto error;
    }

    tmp_head = pobj->br_list;
    if(!tmp_head) {
        goto error;
    }

    while(tmp_head) {
        br = (br_obj_t *)tmp_head->data;
        if(!br) {
            goto error;
        }

        if(count == 0) {
            if(br->start_offset != 0) {
                goto error;
            }
            if(br->end_offset >= (pobj->tot_content_len - 1)) {
                break;
            }
        }
        else {
            if(br->start_offset > prev_end + 1) {
                goto error;
            }

            if(br->end_offset >= (pobj->tot_content_len - 1)) {
                break;
            }
        }
        prev_start = br->start_offset;
        prev_end = br->end_offset;
        count ++;
        if(!tmp_head->next) {
            /* last element check */
	    if(br->end_offset != (pobj->tot_content_len - 1))
                goto error;
        }
        tmp_head = tmp_head->next;
    }

    if(pobj->attr != NULL) {
        if(pobj->attr->cache_expiry < nkn_cur_ts) {
	    pobj->is_complete = 0;
	    glob_tfm_obj_expired ++;
	    DBG_LOG(MSG, MOD_TFM, "Video is already expired. Don't ingest. uri = %s", uri);
            goto error;
	} else {
	    pobj->is_complete = 1;
	    DBG_LOG(MSG, MOD_TFM, "File complete: %s: INGEST....", uri);
	}
    } else {
        pobj->is_complete = 0;
        DBG_LOG(SEVERE, MOD_TFM, "File complete but attributes not "
                "available: %s", uri);
        DBG_ERR(SEVERE, "File complete but attributes not "
                "available: %s", uri);
        goto error;
    }
    return 0;

 error:
    return -1;
}

/*
 * This function is an optimization to merging contiguous byte ranges
 * together if there are no holes between the byte ranges.
 */
int
s_merge_byte_ranges(TFM_obj *pobj, GList **in_prev, GList **in_mid, GList *pnext)
{
    int ret_val =       0;
    br_obj_t    *prev_br;
    br_obj_t    *mid_br;
    br_obj_t    *next_br;
    GList *mid =        *in_mid;
    GList *pprev;

    assert(mid);
    mid_br = (br_obj_t *) mid->data;

    if((in_prev) && (*in_prev)) {
        pprev = *in_prev;
        prev_br = (br_obj_t *) pprev->data;
        if(mid_br->start_offset <= prev_br->end_offset + 1) {
            if(prev_br->start_offset > mid_br->start_offset) {
                prev_br->start_offset = mid_br->start_offset;
            }
            if(prev_br->end_offset <  mid_br->end_offset) {
                prev_br->end_offset = mid_br->end_offset;
            }
            if(mid->data) {
                free(mid->data);
            }
            g_list_free_1(mid);
            *in_mid = NULL;
            mid = pprev;
            mid_br = (br_obj_t *) mid->data;
        }
        else {
            mid->prev = pprev;
            mid->next = pprev->next;
            if(pprev->next)
                pprev->next->prev = mid;
            pprev->next = mid;
        }
    }
    else {
        pobj->br_list = mid;
    }

    if(pnext) {
        next_br = (br_obj_t *) pnext->data;
        if(next_br->start_offset <= mid_br->end_offset + 1) {
            if(next_br->start_offset > mid_br->start_offset) {
                next_br->start_offset = mid_br->start_offset;
            }
            if(next_br->end_offset < mid_br->end_offset) {
                next_br->end_offset = mid_br->end_offset;
            }
            pnext->prev = mid->prev;
            if(mid->prev) {
                mid->prev->next = pnext;
            }
            else {
                pobj->br_list = pnext;
            }

            if(*in_mid == mid) {
                *in_mid = NULL;
            }
            if(mid->data) {
                free(mid->data);
            }
            g_list_free_1(mid);
        }
        else {
            mid->next = pnext;
            pnext->prev = mid;
        }
    }
    return ret_val;
}

/* Checks the offset and returns the correct bit in the  bitmap
   NOTE: Needs to return 1: go ahead and injest
   or -1 if something wrong with video.
*/
static int
s_populate_byte_range(TFM_obj *pobj,
		      MM_put_data_t *map,
		      char *uri,
                      char *filename __attribute((unused)),
		      uint64_t start_range,
                      uint64_t end_range,
		      uint64_t tot_len __attribute((unused)))

{
    GList       *newp;
    GList       *tmp_head;
    br_obj_t    *br, *tmp_br;
    int         ret_val = -1;
    GList       *last = NULL;

    DBG_LOG(MSG, MOD_TFM, "uri = %s, offset = %lld, len = %llu, total = %llu, clipfn = %s",
                           uri,  map->uol.offset,
			   map->uol.length, map->total_length,
			   map->clip_filename);
    DBG_LOG(MSG, MOD_TFM, "start_range = %llu, end_range = %llu", start_range, end_range);

    br = (br_obj_t *) nkn_calloc_type (1, sizeof(br_obj_t), mod_tfm_br);
    if(br == NULL) {
        return -1;
    }
    br->start_offset = start_range;
    br->end_offset = end_range;

    newp = g_list_alloc();
    if(newp == NULL) {
	free(br);
        return -1;
    }
    newp->data = br;
    newp->next = NULL;
    newp->prev = NULL;

    if(pobj == NULL) {
	/* Almost all the time we cannot be in this situation as the object has
	   already been added in TFM_put. */
	/* TODO: I hit this situation one time when the object got
	   deleted just when this function was getting called.
	   If you need to fix this condition, need to fix the
	   locks around s_populate_byte_ranges and TFM_put and
	   promote_complete. In other words, those functions
	   need to syncronized. For now, returning error, so that retry
	   can happen. */
        return -1;
    } else if(!pobj->br_list) {
	pobj->br_list = newp;
        if(map->attr != NULL) {
            /* Copy the attributes into the local database  */
            if(pobj->attr == NULL) {
		if (nkn_attr_copy(map->attr, &pobj->attr, mod_tfm_attr1) != 0) {
		    DBG_LOG(MSG, MOD_TFM, "Memory not obtained for attribs ");
                    return -1;
                }
            }
	    pobj->tot_content_len = s_get_content_len_from_attr(pobj->attr);
	    if ((int64_t)pobj->tot_content_len < 0) {
	    	pobj->tot_content_len = 0;
		return -1;
	    }
            pobj->total_blocks = (pobj->tot_content_len / NKN_MAX_BLOCK_SZ);
            if(pobj->tot_content_len & (NKN_MAX_BLOCK_SZ - 1))
                pobj->total_blocks++;
        }
    } else {
        if(map->attr != NULL) {
            /* Copy the attributes into the local database  */
            if(pobj->attr == NULL) {
		if (nkn_attr_copy(map->attr, &pobj->attr, mod_tfm_attr1) != 0) {
		    DBG_LOG(MSG, MOD_TFM, "Memory not obtained for attribs ");
                    return -1;
                }
            }
	    pobj->tot_content_len = s_get_content_len_from_attr(pobj->attr);
	    if ((int64_t)pobj->tot_content_len < 0) {
	    	pobj->tot_content_len = 0;
		return -1;
	    }
            pobj->total_blocks = (pobj->tot_content_len / NKN_MAX_BLOCK_SZ);
            if(pobj->tot_content_len & (NKN_MAX_BLOCK_SZ - 1))
                pobj->total_blocks++;
        }

        tmp_head = pobj->br_list;
        /* Traverse and compare the start ranges */
        while(tmp_head) {
            tmp_br = (br_obj_t *) tmp_head->data;

	    /* If the range recvd is already present, then decrement
	     * the reservations used in the TFM partition to avoid
	     * unwanted rejection of files.
	     */
	    if(start_range >= tmp_br->start_offset &&
		    end_range <= tmp_br->end_offset) {
		pobj->in_bytes -= map->uol.length;
		glob_tfm_spc_dir_size += map->uol.length;
		glob_tfm_spc_pht_bytes_used -= map->uol.length;
		glob_tfm_spc_pht_bytes_resv -= map->uol.length;
	    }

            if(start_range <= tmp_br->start_offset) {
                /* Warning: this function may delete the memory
                   pointed to by these pointers */
                ret_val = s_merge_byte_ranges(pobj, &tmp_head->prev,
                                              &newp, tmp_head);
                break;
            }
            if(!tmp_head->next)
                last = tmp_head;
            tmp_head = tmp_head->next;

        }
        if(last) {
            /* new element becomes the last element*/
            tmp_br = (br_obj_t *) last->data;
            ret_val = s_merge_byte_ranges(pobj, &last,
                                          &newp, NULL);
        }
    }
    return 0;
}

static void
s_tfm_partial_promote_file(TFM_obj *pobj,
			   int add_to_queue __attribute((unused)))
{
    GList       *tmp_head;
    br_obj_t    *tmp_br;
    int         ret_val = -1;
    AM_pk_t     am_pk;
    char        *uri = NULL;

    if(!pobj)
	return;

    ret_val = tfm_cod_test_and_get_uri(pobj->cod, &uri);
    if(ret_val < 0) {
	assert(0);
	glob_tfm_put_cod_invalid ++;
	return;
    }

    pobj->partial_file_write = 1;
    if(!pobj->partial_queue_added) {
	pobj->partial_promote_in_progress = 1;
	TAILQ_INSERT_TAIL(&nkn_tfm_part_promote_q, pobj, promote_entries);
	pobj->partial_queue_added = 1;
	glob_tfm_getm_partial_cleanup_queue_added++;
    }
#if 0
    if((glob_tfm_promote_files_promote_start -
	glob_tfm_promote_files_promote_end) >
	glob_tfm_outstanding_promote_thr) {
	glob_tfm_promote_thread_too_many_promotes ++;
	return;
    }
#endif
    am_pk.name = NULL;
    tmp_head = pobj->br_list;
    if(!tmp_head)
	return;
    tmp_br = (br_obj_t *) tmp_head->data;
    if(!tmp_br)
	return;
    if(pobj->partial_promote_start == 0) {
	/* Since its a get manager request, if we have a data,
	* we should be having it from the start
	*/
	am_pk.name = nkn_strdup_type(uri, mod_tfm_strdup1);
	am_pk.provider_id = TFMMgr_provider;
	am_pk.sub_provider_id = 0;
	am_pk.type = AM_OBJ_URI;

	/* For the first chunk promotion, get dst, call AM */

	if(AM_is_video_promote_pending(&am_pk) == 0) {
	    glob_tfm_promote_pending_err ++;
	    if(am_pk.name) {
		free(am_pk.name);
	    }
	    glob_tfm_getm_partial_promote_pending_err++;
	    return;
	}

        /* Change the provider in AM to TFM */
        am_pk.provider_id = OriginMgr_provider;
        am_pk.sub_provider_id = 0;
        am_pk.type = AM_OBJ_URI;
        AM_change_obj_provider(&am_pk, TFMMgr_provider,
            0);

	pobj->dst = MM_get_next_faster_ptype(TFMMgr_provider);
	if((pobj->dst <= 0) ||
	    (pobj->dst > (int)NKN_MM_MAX_CACHE_PROVIDERS)) {
	    glob_tfm_promote_ptype_err ++;
	    AM_set_promote_error(&am_pk);
	    if(am_pk.name) {
		free(am_pk.name);
	    }
	    glob_tfm_getm_partial_promote_get_next_ptype_err++;
	    return;
	}

	ret_val = AM_is_video_promote_worthy(&am_pk, pobj->dst);
	if(ret_val < 0) {
	    AM_set_promote_error(&am_pk);
	    glob_tfm_getm_partial_promote_not_worthy_err++;
	    if(am_pk.name) {
		free(am_pk.name);
	    }
	    return;
	}
    }

    if(pobj->partial_promote_start < (pobj->total_blocks - 1)) {
	if(pobj->partial_promote_start == 1 && pobj->partial_promote_complete != 1)
	    goto check_ret;
	if((pobj->partial_promote_start - pobj->partial_promote_complete) > MAX_PROMOTE_QUEUE_PER_FILE)
	    goto check_ret;
	if((pobj->partial_promote_offset + NKN_MAX_BLOCK_SZ) < tmp_br->end_offset) {
	    ret_val = MM_partial_promote_uri(uri, TFMMgr_provider,
			    pobj->dst, pobj->partial_promote_offset, NKN_MAX_BLOCK_SZ,
			    NKN_MAX_BLOCK_SZ, NKN_MAX_BLOCK_SZ);
	    if(ret_val == 0) {
		if(pobj->partial_promote_start == 0) {
		    am_pk.provider_id = TFMMgr_provider;
		    am_pk.sub_provider_id = 0;
		    am_pk.type = AM_OBJ_URI;
		    AM_set_video_promote_pending(&am_pk);
#if 0
		    s_tfm_write_promote_pending_file(uri);
#endif
		}
		pobj->partial_promote_start++;
		pobj->partial_promote_offset+=NKN_MAX_BLOCK_SZ;
		glob_tfm_promote_files_promote_start ++;
		DBG_LOG(MSG, MOD_TFM, "Promote started %d, offset %llu ", pobj->partial_promote_start,
			pobj->partial_promote_offset);
	    } else {
		glob_tfm_getm_partial_promote_uri_err++;
		glob_tfm_promote_thread_too_many_promotes ++;
		if(pobj->partial_promote_start == 0) {
		    am_pk.provider_id = TFMMgr_provider;
		    am_pk.sub_provider_id = 0;
		    am_pk.type = AM_OBJ_URI;
		    AM_set_promote_error(&am_pk);
		    if(am_pk.name) {
			free(am_pk.name);
		    }
		    return;
		}
	    }
	}
    }

    /* If this is the last chunk */
    if(pobj->partial_promote_start == (pobj->total_blocks - 1)){
	if((tmp_br->end_offset - tmp_br->start_offset) == (pobj->tot_content_len - 1)) {
	    ret_val = MM_partial_promote_uri(uri, TFMMgr_provider,
			    pobj->dst, pobj->partial_promote_offset,
			    pobj->tot_content_len - pobj->partial_promote_offset,
			    pobj->tot_content_len - pobj->partial_promote_offset,
			    pobj->tot_content_len - pobj->partial_promote_offset);
	    if(ret_val == 0) {
		pobj->partial_promote_start++;
		pobj->partial_promote_offset +=
		pobj->tot_content_len - pobj->partial_promote_offset;
		glob_tfm_promote_files_promote_start ++;
	    } else {
		glob_tfm_promote_thread_too_many_promotes ++;
		glob_tfm_getm_partial_promote_uri_last_err++;
	    }
	}
    }
check_ret:;
    if(pobj->partial_promote_start != (pobj->total_blocks)){
	pthread_cond_signal(&cleanup_thr_cond);
    }
    if(am_pk.name) {
	free(am_pk.name);
    }
    return;
}

/* This function stores the content in the next available location. Do not
call this function with the mutex held. */
static int
s_input_store_content_in_free_block(MM_put_data_t *map)
{
    int64_t             retf = 0, writelen=0, bytes_written=0;
    int                 open_fd, i;
    MM_put_data_t       *tmp = NULL;
    struct iovec        *myiov;
    AM_pk_t             am_pk;
    TFM_obj             *pobj = NULL, *pobj1 = NULL, *pobjptr = NULL;
    char                filename[TFM_MAX_FILENAME_LEN];
    int                 buf_only = 0, file_id = -1;
    int                 tfm_file_write = NO_TFM_WRITE;
    char                *uri;
    int                 ret_val;
    int                 ret_errno;

    ret_val = tfm_cod_test_and_get_uri(map->uol.cod, &uri);
    if(ret_val < 0) {
	glob_tfm_put_cod_invalid ++;
	return -1;
    }

    DBG_LOG(MSG, MOD_TFM, "uri = %s, offset = %lld, len = %llu, "
	    "total = %llu, clipfn = %s",
	    uri,  map->uol.offset,
	    map->uol.length, map->total_length,
	    map->clip_filename);

    if(map->src_manager == TASK_TYPE_GET_MANAGER_TFM) {
        tfm_file_write = NO_TFM_WRITE;
    }

    if (map->flags & MM_FLAG_PRIVATE_BUF_CACHE)
        tfm_file_write = NORMAL_WRITE;

    if(tfm_file_write != NO_TFM_WRITE) {
        if(tfm_file_write == O_DIRECT_WRITE) {
            open_fd = open(map->clip_filename, O_RDWR | O_CREAT | O_DIRECT , 0644);
    	    glob_tfm_put_direct_opens ++;
        } else {
            open_fd = open(map->clip_filename, O_RDWR | O_CREAT , 0644);
            glob_tfm_put_opens ++;
        }
        if (open_fd < 0) {
            DBG_LOG(WARNING, MOD_TFM, "\nOpen failed. Filename = %s errno = %d",
                    map->clip_filename, errno);
	    glob_tfm_put_open_err ++;
            return -1;
        }

        glob_tfm_put_seeks ++;
        retf = lseek(open_fd, map->uol.offset, SEEK_SET);
        if(retf != map->uol.offset) {
	    if(retf == -1)
		ret_errno = errno;
	    else
		ret_errno = -1;
            DBG_LOG(WARNING, MOD_TFM, "\nLseek failed. Filename = %s errno = %d",
                    map->clip_filename, ret_errno);
	    glob_tfm_put_seek_err ++;
	    /* Bug 3313: Issue caused by Network code to close a valid
	     * FD. Adding a NKN_ASSERT to capture any future issues 
	     */
	    NKN_ASSERT(ret_errno != EBADF);
            goto error;
        }
    }

    myiov = alloca(sizeof(struct iovec) * map->iov_data_len);
    DBG_LOG(MSG, MOD_TFM, "uri = %s, iov_data_len = %ld",
	    uri, map->iov_data_len);
    for (i=0; i<map->iov_data_len; i++) {
	myiov[i].iov_base = map->iov_data[i].iov_base;
	myiov[i].iov_len = map->iov_data[i].iov_len;
	writelen += map->iov_data[i].iov_len;
        DBG_LOG(MSG, MOD_TFM, "uri = %s, iov_base = %p, iov_len = %ld",
		uri, myiov[i].iov_base, myiov[i].iov_len);
    }

    if(tfm_file_write == NO_TFM_WRITE) {
        retf = writelen;
        buf_only = 1;
    }else {
        glob_tfm_put_writes ++;
        retf = writev(open_fd, myiov, map->iov_data_len);
        if((retf < 0) || (retf != writelen)) {
	    if(retf == -1)
		ret_errno = errno;
	    else
		ret_errno = -1;
	    glob_tfm_put_write_err ++;
            DBG_LOG(WARNING, MOD_TFM,"\nWrite failed. Filename = %s errno = %d",
		    map->clip_filename, ret_errno);
	    /* Bug 3313: Issue caused by Network code to close a valid
	     * FD. Adding a NKN_ASSERT to capture any future issues 
	     */
	    NKN_ASSERT(ret_errno != EBADF);
        }
    }

    bytes_written = retf;
    glob_tfm_put_end ++;

    if(map->src_manager != TASK_TYPE_GET_MANAGER_TFM) {
	glob_tfm_spc_dir_size -= bytes_written;
	glob_tfm_spc_pht_bytes_used += bytes_written;
	glob_tfm_spc_pht_bytes_resv += bytes_written;
    }

    DBG_LOG(MSG, MOD_TFM, "uri = %s, off=%ld retf=%ld tot=%ld\n", uri,
    	    map->uol.offset, retf, map->total_length);

    pthread_mutex_lock(&nkn_tfm_pht_mutex);
    glob_tfm_pht_mutex_lock_calls ++;

    pobj = (TFM_obj *) nkn_tfm_cod_tbl_get(nkn_tfm_partial_uri_hash_table,
					   map->uol.cod);
    if(!pobj) {
	pthread_mutex_unlock(&nkn_tfm_pht_mutex);
	glob_tfm_put_pht_obj_not_found_after_write ++;
	goto error;
    }

    if(buf_only) {
        glob_tfm_put_pht_buf_only++;
        pobj->is_buf_only = 1;
    }

    retf = s_populate_byte_range(pobj, map, uri,
				 map->clip_filename,
                                 map->uol.offset,
                                 map->uol.offset + retf - 1,
                                 map->total_length);

    pobj->in_bytes += bytes_written;
    if(map->src_manager == TASK_TYPE_GET_MANAGER_TFM) {
	s_tfm_partial_promote_file(pobj, 1) ;
        retf = NKN_TFM_OK;
	pthread_mutex_unlock(&nkn_tfm_pht_mutex);
        goto success;
    }
    retf = s_is_video_complete(map->uol.cod);
    if(retf < 0) {
        retf = NKN_TFM_OK;
	glob_tfm_put_video_not_complete ++;
	pthread_mutex_unlock(&nkn_tfm_pht_mutex);
        goto success;
    }

    pobjptr = (TFM_obj *) nkn_calloc_type (1, sizeof(TFM_obj), nkn_tfm_cht_obj);
    if(!pobjptr) {
	pthread_mutex_unlock(&nkn_tfm_pht_mutex);
	goto error;
    }
    /* If the object is in the completed hash table, we reject
       any puts for the same object. The object will eventually
       be deleted by promote or expiration and then new puts can
       come through.*/
    glob_tfm_pht_cht_mutex_lock_calls ++;
    pthread_mutex_lock(&nkn_tfm_cht_mutex);
    glob_tfm_cht_mutex_lock_calls ++;
    /* Clear the sac as we have a file to completion 
     */
    glob_tfm_pht_spc_sac_reject = 0;
    pobj1 = nkn_tfm_cod_tbl_get(nkn_tfm_completed_uri_hash_table,
				map->uol.cod);
    if(pobj1) {
	/* Object already complete. Reject this put*/
	DBG_LOG(MSG, MOD_TFM, "Object already complete: uri: %s",
		uri);
	pthread_mutex_unlock(&nkn_tfm_cht_mutex);
	glob_tfm_put_already_complete_err ++;
	memcpy(filename, pobj->filename, TFM_MAX_FILENAME_LEN);
	glob_tfm_spc_pht_bytes_used -= pobj->in_bytes;

	glob_tfm_spc_pht_bytes_resv -= pobj->in_bytes;
        glob_tfm_pht_delete ++;
        TAILQ_REMOVE(&nkn_tfm_cleanup_q, pobj, entries);
	file_id = pobj->file_id;
	nkn_tfm_cod_tbl_delete(nkn_tfm_partial_uri_hash_table,
			       pobj->cod);

        glob_tfm_spc_pht_num_files --;
	pthread_mutex_unlock(&nkn_tfm_pht_mutex);
        if(!pobjptr->is_buf_only)
	    s_tfm_unlink(filename);
	s_tfm_free_file_id(file_id);
        free(pobjptr);
	goto error;
    } else {
	pobj1 = pobjptr;
	ret_val = s_copy_tfm_obj(pobj, pobj1);
        pobj1->defer_delete = 0;
        pobj1->is_promoted = 0;
        pobj1->is_complete = 1;
	glob_tfm_spc_pht_bytes_used -= pobj->in_bytes;
        if(pobj1->is_buf_only) {
            glob_tfm_cht_buf_only_pend++;
        }
	/* Increment the resv portion by the total size of the object.
	   For some objects, we may not know this length apriori, in which
	   case, we will only approximate the resv portion. This may need
           to be fixed when mixed chunked encoded and normal objects are
	   being ingested.*/
	glob_tfm_spc_pht_bytes_resv -= pobj->in_bytes;
        TAILQ_REMOVE(&nkn_tfm_cleanup_q, pobj, entries);
	nkn_tfm_cod_tbl_delete(nkn_tfm_partial_uri_hash_table,
			       pobj->cod);
        glob_tfm_pht_delete ++;

	if(ret_val == 0) {
	    /* Insert into the cht only if the cod provided by
	       the pht entry is valid and was dup'd successfully*/
	    /* Need to pobj1, pobj is the partial object and freed 
	     * in the previous statement.
	     */
	    glob_tfm_spc_cht_bytes_used += pobj1->in_bytes;
	    glob_tfm_pobj_glib_mem += 24;
	    nkn_tfm_cod_tbl_insert(nkn_tfm_completed_uri_hash_table,
				   &pobj1->cod, pobj1);
	    glob_tfm_spc_cht_num_files ++;
	    glob_tfm_spc_cht_tot_num_files ++;
	    glob_tfm_cht_insert ++;
	}
    }
    pthread_mutex_unlock(&nkn_tfm_cht_mutex);

    glob_tfm_spc_pht_num_files --;
    pthread_mutex_unlock(&nkn_tfm_pht_mutex);

    glob_tfm_put_complete ++;
    /* Update the entry in AM as it now has TFM as the provider*/
    am_pk.name = nkn_strdup_type(uri, mod_tfm_strdup1);
    am_pk.provider_id = OriginMgr_provider;
    am_pk.sub_provider_id = 0;
    am_pk.type = AM_OBJ_URI;
    AM_change_obj_provider(&am_pk, TFMMgr_provider,
				 0);

    am_pk.provider_id = TFMMgr_provider;
    if(ret_val == 0)
	AM_set_attr_size(&am_pk, pobj1->attr->total_attrsize);

    /* This memory is freed in the other (listening) thread */
    tmp = (MM_put_data_t *) nkn_calloc_type (1, sizeof(MM_put_data_t),
                                             mod_tfm_mm_put_data);
    if(!tmp) {
	if(am_pk.name) {
	    free(am_pk.name);
	}
	goto error;
    }
    memcpy(tmp, map, sizeof(MM_put_data_t));
    tmp->uol.cod = map->uol.cod;

    glob_tfm_put_file_create_cnt ++;
    /* Queue this video to injest */
    g_async_queue_push(tfm_async_q, tmp);

    glob_tfm_async_q_count ++;
    retf = NKN_TFM_COMPLETE;
    if(am_pk.name) {
	free(am_pk.name);
    }
 success:
    if(tfm_file_write != NO_TFM_WRITE)
        close(open_fd);
    return retf;

 error:
    if(tfm_file_write != NO_TFM_WRITE)
        close(open_fd);
    return -1;
}

#if 0
static void
s_tfm_pending_promote_thread_hdl(void)
{
    GList *plist = TFM_pending_promote_list;
    GList *walk = TFM_pending_promote_list;
    char *uri;
    MM_delete_resp_t MM_delete_data;
    int uri_len = 0;
    nkn_cod_t cod;

    memset(&MM_delete_data, 0, sizeof(MM_delete_data));
    while(1) {
	if(nkn_system_inited == 1) {
	    break;
	}
        sleep(10);
    }

    /* Delete all pending promote files so that if the file
       is obtained again, we will not reject the TFM_put.*/
    while(plist) {
	uri = (char *) plist->data;
	uri_len = strlen(uri);
        cod = nkn_cod_open(uri, NKN_COD_TFM_MGR);
        if(cod == NKN_COD_NULL) {
	    glob_tfm_cod_null_err ++;
	    continue;
        }
        MM_delete_data.in_uol.cod = cod;
        /* Delete from all providers */
	MM_delete_data.in_ptype = Unknown_provider;
	MM_delete(&MM_delete_data);
	plist = plist->next;
        nkn_cod_close(cod, NKN_COD_TFM_MGR);
    }

    walk = plist = TFM_pending_promote_list;
    while(plist) {
        walk = g_list_remove_link(walk, plist);
	uri = (char *) plist->data;
	if(!uri) {
	    continue;
	}
	free(uri);
        g_list_free_1(plist);
	plist = walk;
    }
}
#endif

/* Delete function to take out files that have been partial
   for too long.
   This function needs to have a lock held while calling it.*/
static int
s_tfm_pht_delete_obj(TFM_obj *pobj)
{
    MM_delete_resp_t MM_delete_data;
    char             *uri = NULL;
    int              valid_uri = 0;
    int              ret_val;

    /* For large files, the write takes a long time, which will cause
     * pht to stay there for longer time. These should not be deleted
     * if the file is getting promoted without any error. The
     * time_last_promoted is set to nkn_cur_ts whenever a 2MB promote
     * completes successfully
     */
    if(pobj->partial_file_write && pobj->partial_promote_in_progress) {
	glob_tfm_partial_file_deletion_avoided++;
	goto returnfalse;
    }

    if(pobj->time_last_promoted && pobj->partial_file_write) {
	if((nkn_cur_ts - pobj->time_last_promoted)
	    < tfm_partial_file_time_limit) {
	    glob_tfm_partial_file_deletion_avoided++;
	    goto returnfalse;
	}
    }

    if((pobj->attr) && (pobj->attr->cache_expiry <= nkn_cur_ts)) {
	    pobj->is_complete = 0;
            glob_tfm_expired_file_delete ++;
            goto returntrue;
    }

    /* Find out the time when this object was created and delete
       if it has reached a certain limit */
    if((nkn_cur_ts - pobj->time_modified)
       > tfm_partial_file_time_limit) {
	glob_tfm_partial_file_delete ++;
        /* need to remove the pobj somehow */
        goto returntrue;
    }

    if(pobj->defer_delete == 1) {
        glob_tfm_defer_delete ++;
        goto returntrue;
    }

    if(pobj->is_complete == 1) {
	/* Timeout only partial files */
        goto returnfalse;
    }

returnfalse:
    return FALSE;

returntrue:
    glob_tfm_thread_num_files_deleted ++;
    glob_tfm_cleanup_num_files_deleted ++;
    if(pobj) {
	ret_val = tfm_cod_test_and_get_uri(pobj->cod, &uri);
	if(ret_val < 0) {
	    assert(0);
	    glob_tfm_pht_delete_cod_invalid ++;
	} else {
	    valid_uri = 1;
	}

        if(!pobj->is_buf_only)
	    s_tfm_unlink(pobj->filename);
	if(!pobj->partial_file_write) {
	    glob_tfm_spc_pht_bytes_used -= pobj->in_bytes;
	    glob_tfm_spc_pht_bytes_resv -= pobj->in_bytes;
	} else {
	    if(valid_uri) {
		memset(&MM_delete_data, 0, sizeof(MM_delete_data));
		MM_delete_data.in_uol.cod = pobj->cod;
		MM_delete_data.in_ptype = pobj->dst;
		MM_delete(&MM_delete_data);
	    }
	}
	if(pobj->partial_queue_added) {
	    TAILQ_REMOVE(&nkn_tfm_part_promote_q, pobj, promote_entries);
	    glob_tfm_getm_partial_cleanup_queue_removed++;
	}
        TAILQ_REMOVE(&nkn_tfm_cleanup_q, pobj, entries);
	s_tfm_free_file_id(pobj->file_id);
	nkn_tfm_cod_tbl_delete(nkn_tfm_partial_uri_hash_table,
			       pobj->cod);

        glob_tfm_spc_pht_num_files --;
        glob_tfm_pht_delete ++;
    }
    return TRUE;
}

/* Used for delete unused pht files as well as promotion of
 * get manager files to disk. Runs at an interval of 5 secs.
 * Delete is checked for tfm_partial_file_time_limit = 300secs.
 */
static void
s_tfm_cleanup_thread_hdl(void)
{
    TFM_obj *pobj, *pobj_temp;
    struct timeval delete_timer1;
    struct timeval delete_timer2;
    struct timespec abstime;

    prctl(PR_SET_NAME, "nvsd-tfm-cleanup", 0, 0, 0);
    while(1) {

        pthread_mutex_lock(&nkn_tfm_pht_mutex);
        glob_tfm_pht_mutex_lock_calls ++;
        glob_tfm_pht_mutex_cleanup_thread_lock_calls ++;

        TAILQ_FOREACH_SAFE(pobj, &nkn_tfm_part_promote_q, promote_entries, pobj_temp) {
	    assert(pobj->partial_file_write);
            if(pobj && !pobj->is_complete) {
		s_tfm_partial_promote_file(pobj, 0);
	    }
	}

        glob_tfm_thread_num_files_deleted = 0;
        gettimeofday(&delete_timer1, NULL);

        TAILQ_FOREACH_SAFE(pobj, &nkn_tfm_cleanup_q, entries, pobj_temp) {
            if(pobj) {
                if(glob_tfm_thread_num_files_deleted > tfm_num_delete_chunk)
                    break;
                if((nkn_cur_ts - pobj->time_modified) < tfm_partial_file_time_limit)
                    break;
                s_tfm_pht_delete_obj(pobj);
            }
        }
        gettimeofday(&delete_timer2, NULL);
        glob_tfm_delete_thread_time =
            delete_timer2.tv_sec-delete_timer1.tv_sec;

        clock_gettime(CLOCK_MONOTONIC, &abstime);
        abstime.tv_sec += tfm_partial_file_thrd_time_limit;
        abstime.tv_nsec = 0;
	pthread_cond_timedwait(&cleanup_thr_cond, &nkn_tfm_pht_mutex, &abstime);
	pthread_mutex_unlock(&nkn_tfm_pht_mutex);
    }
}

static int
s_tfm_promote_uri(nkn_cod_t cod)
{
    AM_pk_t             am_pk;
    nkn_provider_type_t dst_ptype;
    int                 ret = -1;
    TFM_promote_cfg_t   *tfm_cfg;
    char                *uri;

    ret = tfm_cod_test_and_get_uri(cod, &uri);
    if(ret < 0) {
	assert(0);
	glob_tfm_promote_cod_invalid ++;
	return -1;
    }

    /* Only keep a certain number of promotes in the system */
    if((glob_tfm_promote_files_promote_start -
	AO_load(&glob_tfm_promote_files_promote_end)) <
       10000) {
	am_pk.name = uri;
	am_pk.provider_id = TFMMgr_provider;
	am_pk.sub_provider_id = 0;
	am_pk.type = AM_OBJ_URI;

	if(AM_is_video_promote_pending(&am_pk) == 0) {
	    glob_tfm_promote_pending_err ++;
	    return -1;
	}

	dst_ptype = MM_get_next_faster_ptype(TFMMgr_provider);
	if((dst_ptype <= 0) ||
	   (dst_ptype > (int)NKN_MM_MAX_CACHE_PROVIDERS)) {
	    glob_tfm_promote_ptype_err ++;
	    AM_set_promote_error(&am_pk);
	    return -1;
	}

	ret = AM_is_video_promote_worthy(&am_pk, dst_ptype);
	if(ret < 0) {
	    AM_set_promote_error(&am_pk);
	    glob_tfm_promote_promote_not_worthy_err ++;
	    return -1;
	}

	tfm_cfg = (TFM_promote_cfg_t *) nkn_calloc_type (1,
					    sizeof(TFM_promote_cfg_t),
					    mod_tfm_promote_cfg);
	if(!tfm_cfg) {
	    AM_set_promote_error(&am_pk);
	    glob_tfm_promote_alloc_err ++;
	    return -1;
	}

        AM_set_video_promote_pending(&am_pk);
	tfm_cfg->cod = cod;
	tfm_cfg->src = TFMMgr_provider;
	tfm_cfg->dst = dst_ptype;

	am_pk.provider_id = dst_ptype;
	AM_video_promoted(&am_pk);
	pthread_mutex_lock(&tfm_promote_mutex);
	glob_tfm_promote_queue_num_obj ++;
	STAILQ_INSERT_TAIL(&nkn_tfm_promote_q, tfm_cfg, entries);
	pthread_cond_signal(&tfm_promote_thr_cond);
	pthread_mutex_unlock(&tfm_promote_mutex);
	return 0;
    }
    return -1;
}

static void
s_tfm_fc_thread_hdl(void)
{
    TFM_obj *pobj = NULL;
    MM_put_data_t *map = NULL;
    int ret = -1, file_id = -1;
    char *filename = NULL;
    int  promote_err = 0;
    char *uri;
    int  ret_val;

    prctl(PR_SET_NAME, "nvsd-tfm-fc", 0, 0, 0);	

    while(1) {
        map = (MM_put_data_t *)g_async_queue_pop(tfm_async_q);

        if(map == NULL) {
            continue;
        }

	ret_val = tfm_cod_test_and_get_uri(map->uol.cod, &uri);
	if(ret_val < 0) {
	    assert(0);
	    glob_tfm_put_cod_invalid ++;
	    if(map) {
		free(map);
		map = NULL;
	    }
	    continue;
	}

        promote_err = 0;
        pthread_mutex_lock(&nkn_tfm_cht_mutex);
        glob_tfm_cht_mutex_lock_calls ++;
        glob_tfm_async_q_count --;
        pobj = (TFM_obj *)nkn_tfm_cod_tbl_get(nkn_tfm_completed_uri_hash_table,
					      map->uol.cod);
        if(pobj == NULL) {
            glob_tfm_completed_obj_not_present ++;
            pthread_mutex_unlock(&nkn_tfm_cht_mutex);
            promote_err = 1;
        } else if(pobj->is_promoted == 1) {
            glob_tfm_completed_obj_already_promoted ++;
	    pthread_mutex_unlock(&nkn_tfm_cht_mutex);
            promote_err = 1;
	}

        if(!promote_err) {
	    pthread_mutex_unlock(&nkn_tfm_cht_mutex);
	    /* Need to release the lock before calling MM_promote_uri because
	    it causes MM_promote_complete directly and that waits on the
	    same lock. */
	    ret = s_tfm_promote_uri(map->uol.cod);
	    if(ret < 0) {
                promote_err = 1;
            } else {

            }
        }

	if(!promote_err) {
            glob_tfm_num_promotes_in_progress ++;
#if 0
	    s_tfm_write_promote_pending_file(uri);
#endif
	} else {
	    pthread_mutex_lock(&nkn_tfm_cht_mutex);
            glob_tfm_cht_mutex_lock_calls ++;
	    file_id = -1;
	    /* Need to re-get the pobj because we can't trust that it
	       has not been deleted. */
	    pobj = (TFM_obj *)nkn_tfm_cod_tbl_get(nkn_tfm_completed_uri_hash_table,
						  map->uol.cod);
	    if(pobj) {
                pobj->defer_delete = 0;
                if(pobj->is_buf_only) {
                    glob_tfm_cht_buf_only_pend--;
		    filename = NULL;
                }else {
		    filename = nkn_strdup_type(pobj->filename, mod_tfm_strdup6);
		}
		glob_tfm_spc_cht_bytes_used -= pobj->in_bytes;
		file_id = pobj->file_id;
		nkn_tfm_cod_tbl_delete(nkn_tfm_completed_uri_hash_table,
				       pobj->cod);
		glob_tfm_spc_cht_num_files --;
                glob_tfm_cht_delete ++;
	    }
            glob_tfm_uris_promote_not_worthy ++;
	    pthread_mutex_unlock(&nkn_tfm_cht_mutex);
	    if(filename) {
		s_tfm_unlink(filename);
		free(filename);
	    }
	    if(file_id != -1)
		s_tfm_free_file_id(file_id);
	}

        if(map) {
            free(map);
            map = NULL;
        }
    }
    return;
}

int
s_rr_disk_caches(void)
{
    int i = 0, found = 0;

    if (glob_tfm_num_caches == 1) {
        if (g_cache_info[i].disk_space_exceeded != 1) {
            found = 1;
        }
    }
    else {
        for (i = 0; i < glob_tfm_num_caches; i++) {
            s_input_cur_write_dir_idx =
                (s_input_cur_write_dir_idx + 1) % glob_tfm_num_caches;

            if (g_cache_info[i].disk_space_exceeded != 1) {
                found = 1;
                break;
            }
        }
    }

    if (found != 1) {
        s_tfm_disk_space_exceeded = 1;
        return -1;
    }
    else {
        s_tfm_disk_space_exceeded = 0;
        return 0;
    }
}

static void s_remove_datafiles(void)
{
    const char rmcmd[] = "/bin/rm -rf /nkn/tfm1 &";
    const char mvcmd[] = "/bin/mv /nkn/tfm /nkn/tfm1 ";
    char file[] = "/tmp/brange_all.txt";
    char file1[] = "/tmp/brange.txt";

    s_tfm_unlink(file);
    s_tfm_unlink(file1);
    /* Move /nkn/tfm to another directory */
    system(mvcmd);
    system(rmcmd);
}

static void s_get_filesystem_size(void)
{
    struct statvfs st;
    int    ret = -1;

    ret = statvfs("/nkn", &st);
    if(ret < 0) {
        DBG_LOG(WARNING, MOD_TFM, "Cannot find size of TFM file system. "\
		"Switch to number of files mode", errno);
        glob_tfm_spc_default_mode = 1;
        return;
    }
    glob_tfm_spc_default_mode = 0;
    glob_tfm_spc_dir_size = (glob_tfm_spc_dir_use_percent *
			     st.f_bavail * st.f_bsize) / 100;

#if 0
    /* Allocate a percentage for partial files and one for complete files*/
    glob_tfm_spc_pht_bytes_avail = (glob_tfm_spc_pht_use_percent
	* glob_tfm_spc_dir_size) / 100;
    glob_tfm_spc_cht_bytes_avail = (glob_tfm_spc_cht_use_percent
	* glob_tfm_spc_dir_size) / 100;
#endif
    glob_tfm_spc_pht_bytes_avail = (glob_tfm_max_partial_files * glob_tfm_max_file_size);

    /* Allocate 20% more than needed */
    glob_tfm_spc_pht_bytes_avail += ((glob_tfm_spc_pht_bytes_avail *
				     glob_tfm_spc_use_percent)/100);

    glob_tfm_spc_cht_bytes_avail = (glob_tfm_max_complete_files * glob_tfm_max_file_size);
    glob_tfm_spc_cht_bytes_avail += ((glob_tfm_spc_cht_bytes_avail *
				     glob_tfm_spc_use_percent)/100);

    DBG_LOG(WARNING, MOD_TFM, "Size of TFM directory",
	    glob_tfm_spc_dir_size);
}

static int s_create_tfm_dir(char *container_path, int isdir)
{
    char *end_slash, *cptr;
    int ret;

    if (isdir) {
        end_slash = &container_path[strlen(container_path)];
    } else {
        end_slash = strrchr(container_path, '/');
    }
    cptr = container_path;
    while (cptr < end_slash) {
        cptr = strchr(cptr+1, '/');
        if (cptr) {
            *cptr = '\0';
        } else if (isdir) {
            cptr = end_slash;
        } else {
            break;
        }
        ret = mkdir(container_path, 0755);
        if (ret != 0 && errno != EEXIST) {
            /* This should never happen */
            DBG_LOG(MSG, MOD_TFM, "mkdir error (%s): %d\n", container_path, errno);
            return -errno;
        }
        if (!isdir || cptr != end_slash)
            *cptr = '/';
    }
    /* Restore slash, so the container name is complete again */
    if (!isdir)
        *end_slash = '/';
    return 0;
}

static void
dummy_func(gpointer data __attribute((unused)))
{
    return;
}

void *
TFM_promote_thread_hdl(void *dummy __attribute((unused)))
{
    TFM_promote_cfg_t *cfg = NULL;
    TFM_obj           *pobj = NULL;
    struct timespec   abstime;
    int               ret = -1;
    nkn_provider_type_t dst_ptype = 0;
    AM_pk_t           pk;
    nkn_hot_t         hotness;
    char              *filename;
    char              *uri;
    int               file_id = -1;

    glob_tfm_promote_thread_called = 1;
    abstime.tv_sec = 60;
    abstime.tv_nsec = 0;

    prctl(PR_SET_NAME, "nvsd-tfm-promote", 0, 0, 0);

    while(1) {
	pthread_mutex_lock(&tfm_promote_mutex);
#if 0
        if((glob_tfm_promote_files_promote_start -
            glob_tfm_promote_files_promote_end) >
            glob_tfm_outstanding_promote_thr) {
	    glob_tfm_promote_thread_too_many_promotes ++;
            goto too_many_promotes;
        }
#endif

	glob_tfm_promote_thread_called ++;
	cfg = NULL;
	cfg = STAILQ_FIRST(&nkn_tfm_promote_q);
	if(cfg) {
	    STAILQ_REMOVE_HEAD(&nkn_tfm_promote_q, entries);
	    glob_tfm_promote_queue_num_obj --;
	} else {
	    goto wait;
	}

	pthread_mutex_lock(&nkn_tfm_cht_mutex);
	ret = tfm_cod_test_and_get_uri(cfg->cod, &uri);
	if(ret < 0) {
	    assert(0);
	    glob_tfm_spc_cht_bytes_used -= pobj->in_bytes;
	    filename = nkn_strdup_type(pobj->filename, mod_tfm_strdup6);
	    file_id = pobj->file_id;
	    nkn_tfm_cod_tbl_delete(nkn_tfm_completed_uri_hash_table,
				   pobj->cod);
	    glob_tfm_promote_cht_delete ++;
	    glob_tfm_spc_cht_num_files --;
	    pthread_mutex_unlock(&nkn_tfm_cht_mutex);
	    s_tfm_unlink(filename);
	    free(filename);
	    s_tfm_free_file_id(file_id);
	    glob_tfm_promote_cod_invalid ++;
	    goto error;
	}

	glob_tfm_cht_mutex_lock_calls ++;
	pobj = (TFM_obj *) nkn_tfm_cod_tbl_get (
					   nkn_tfm_completed_uri_hash_table,
					   cfg->cod);
	if(!pobj) {
	    glob_tfm_promote_obj_err ++;
	    pthread_mutex_unlock(&nkn_tfm_cht_mutex);
	    goto error;
	}

	if(!pobj->attr) {
	    glob_tfm_promote_obj_attr_err ++;
	    pthread_mutex_unlock(&nkn_tfm_cht_mutex);
	    goto error;
	}

        if(pobj->is_buf_only)
            glob_tfm_cht_buf_only_pend--;

	if(pobj->attr->cache_expiry <= nkn_cur_ts) {
	    glob_tfm_promote_obj_expired ++;
	    glob_tfm_spc_cht_bytes_used -= pobj->in_bytes;
	    filename = NULL;
	    if(!pobj->is_buf_only)
		filename = nkn_strdup_type(pobj->filename, mod_tfm_strdup6);
	    file_id = pobj->file_id;
	    nkn_tfm_cod_tbl_delete(nkn_tfm_completed_uri_hash_table,
				   pobj->cod);
	    glob_tfm_promote_cht_delete ++;
	    glob_tfm_spc_cht_num_files --;
	    pthread_mutex_unlock(&nkn_tfm_cht_mutex);
	    if(filename) {
		s_tfm_unlink(filename);
		free(filename);
	    }
	    s_tfm_free_file_id(file_id);
	    goto error;
	}
	pthread_mutex_unlock(&nkn_tfm_cht_mutex);

	ret = 0;
	if((glob_tfm_cli_promote_small_obj_enable == 0) ||
	   (pobj->tot_content_len > glob_tfm_cli_promote_small_obj_size)) {
	    /* This object goes through normal promote process. Small objects
	       may get promoted directly to highest cache tier. */
	    ret = MM_promote_uri(uri, TFMMgr_provider,
				 cfg->dst, pobj->cod, NULL);
	} else {
	    dst_ptype = MM_get_fastest_put_ptype();
	    if((dst_ptype <= 0) ||
	       (dst_ptype > (int)NKN_MM_MAX_CACHE_PROVIDERS)) {
		ret = MM_promote_uri(uri, TFMMgr_provider,
				     cfg->dst, pobj->cod, NULL);
	    } else {
		/* Set the hotness-threshold to an artificially high value
		   so that
		   this object can sit in the highest cache provider. */
		pk.name = uri;
		pk.provider_id = TFMMgr_provider;
		pk.sub_provider_id = 0;
		pk.type = AM_OBJ_URI;
		/* Enough hotness to get into SSD. 18 : We already have a hotness
		 * of 3 because of the single hit 
		 */
                am_encode_hotness(am_cache_promotion_hotness_threshold * 5, &hotness);
		AM_inc_num_hits(&pk, 0, hotness, 1, NULL, NULL);
		/* Promote to the highest cache provider */
		glob_tfm_promote_small_obj ++;
		ret = MM_promote_uri(uri, TFMMgr_provider,
				     dst_ptype, pobj->cod, NULL);
	    }
	}

        if(ret < 0) {
            /* Retry this object at a later time. Don't increment
	       the promote counter. */
	    glob_tfm_promote_queue_num_obj ++;
	    if(pobj->is_buf_only)
		glob_tfm_cht_buf_only_pend++;
	    STAILQ_INSERT_TAIL(&nkn_tfm_promote_q, cfg, entries);
	    glob_tfm_promote_thread_too_many_promotes ++;
            goto too_many_promotes;
        }

	glob_tfm_promote_files_promote_start ++;
	glob_tfm_promote_files_promote_called ++;

	if(cfg) {
	    free(cfg);
	    cfg = NULL;
	}
	pthread_mutex_unlock(&tfm_promote_mutex);
	continue;

    too_many_promotes:
	pthread_cond_wait(&tfm_promote_thr_limit_cond,
			    &tfm_promote_mutex);
	pthread_mutex_unlock(&tfm_promote_mutex);
	continue;

    wait:
	if(cfg) {
	    free(cfg);
	    cfg = NULL;
	}

	pthread_cond_wait(&tfm_promote_thr_cond, &tfm_promote_mutex);
	pthread_mutex_unlock(&tfm_promote_mutex);
	continue;

    error:
	if(cfg) {
	    free(cfg);
	    cfg = NULL;
	}
	pthread_mutex_unlock(&tfm_promote_mutex);
    } /* while(1)*/
    return NULL;
}

int
TFM_init(void)
{
    int num_caches;
    int ret_val = -1;
    GError *err = NULL ;
    char message[64];
    char dirname[128] = "/nkn/tfm\0";
    int ret = -1;
    pthread_condattr_t a;

    glob_tfm_max_partial_files = glob_tfm_max_partial_files;
    glob_tfm_max_complete_files = glob_tfm_max_partial_files;

    nkn_tfm_cod_tbl_init_full(&nkn_tfm_completed_uri_hash_table,
			      dummy_func, s_cleanup_obj);
    assert(nkn_tfm_completed_uri_hash_table);

    nkn_tfm_cod_tbl_init_full(&nkn_tfm_partial_uri_hash_table,
			      dummy_func, s_cleanup_obj);
    assert(nkn_tfm_partial_uri_hash_table);

    s_remove_datafiles();

    ret_val = s_create_tfm_dir(dirname, 1);
    if(ret_val < 0) {
        DBG_LOG(SEVERE, MOD_TFM, "Cannot make TFM directory: %s", dirname);
        DBG_ERR(SEVERE, "Cannot make TFM directory: %s", dirname);
	glob_tfm_initialized = 0;
        return -1;
    }
    num_caches = s_tmp_init_caches(dirname, 73);

    if(s_tfm_init_file_id_list() < 0) {
        DBG_LOG(SEVERE, MOD_TFM, "File list Q init failed");
        DBG_ERR(SEVERE, "File list Q init failed");
	glob_tfm_initialized = 0;
        return -1;
    }

    s_get_filesystem_size();
    MM_register_provider(TFMMgr_provider, 0,
			 TFM_put, TFM_stat, TFM_get, TFM_delete, TFM_shutdown,
			 TFM_discover, TFM_evict, TFM_provider_stat,
			 NULL, NULL, TFM_promote_complete,
			 2, /* # of PUT threads */
			 8, /* # of GET threads */
			 10000, 0, MM_THREAD_ACTION_ASYNC);

    tfm_async_q = g_async_queue_new();
    if(tfm_async_q == NULL) {
        DBG_LOG(SEVERE, MOD_TFM, "\n%s(): Async Queue did not get inited",
                __FUNCTION__);
        DBG_ERR(SEVERE, "\n%s(): Async Queue did not get inited", __FUNCTION__);
    }
    glob_tfm_async_q_count = 0;

    ret_val = pthread_mutex_init(&nkn_tfm_cht_mutex, NULL);
    if(ret_val < 0) {
        DBG_LOG(SEVERE, MOD_TFM, "TFM Stat Mutex not created. "
		"Severe TFM failure");
        DBG_ERR(SEVERE, "TFM Stat Mutex not created. "
		"Severe TFM failure");
	glob_tfm_initialized = 0;
        return -1;
    }

    ret_val = pthread_mutex_init(&nkn_tfm_pht_mutex, NULL);
    if(ret_val < 0) {
        DBG_LOG(SEVERE, MOD_TFM, "TFM Stat Mutex not created. "
		"Severe TFM failure");
	DBG_ERR(SEVERE, "TFM Stat Mutex not created. "
                "Severe TFM failure");
	glob_tfm_initialized = 0;
        return -1;
    }

    /* Dont use this cond in a pthread_cond_timedwait
     * Initialize the cond variable with CLOCK_MONOTNOIC
     * before using it.
     */
    pthread_cond_init(&tfm_promote_comp_thr_cond, NULL);

    pthread_cond_init(&tfm_promote_thr_cond, NULL);

    pthread_cond_init(&tfm_promote_thr_limit_cond, NULL);

    STAILQ_INIT(&nkn_tfm_promote_comp_q);

    STAILQ_INIT(&nkn_tfm_promote_q);

    TAILQ_INIT(&nkn_tfm_cleanup_q);

    TAILQ_INIT(&nkn_tfm_part_promote_q);

    tfm_async_thread = g_thread_create_full((GThreadFunc) s_tfm_fc_thread_hdl,
					    (void *)message, (128*1024), TRUE,
					    FALSE, G_THREAD_PRIORITY_NORMAL, &err);
    if(tfm_async_thread == NULL) {
        DBG_LOG(SEVERE, MOD_TFM, "TFM async thread did not get initialized");
        DBG_ERR(SEVERE, "TFM async thread did not get initialized");
        g_error_free(err);
	glob_tfm_initialized = 0;
        return -1;
    }

    pthread_condattr_init(&a);
    pthread_condattr_setclock(&a, CLOCK_MONOTONIC);

    pthread_cond_init(&cleanup_thr_cond, &a);

    tfm_delete_thread = g_thread_create_full(
    					(GThreadFunc) s_tfm_cleanup_thread_hdl,
                                        (void *)message, (128*1024), TRUE,
					FALSE, G_THREAD_PRIORITY_NORMAL, &err);
    if(tfm_delete_thread == NULL) {
        DBG_LOG(SEVERE, MOD_TFM, "TFM delete thread did not get initialized.");
        DBG_ERR(SEVERE, "TFM delete thread did not get initialized.");
        g_error_free(err);
	glob_tfm_initialized = 0;
        return -1;
    }

    tfm_promote_comp_thread = g_thread_create_full(
    					(GThreadFunc) s_tfm_promote_complete_thread_hdl,
                                        (void *)message, (128*1024), TRUE,
					FALSE, G_THREAD_PRIORITY_NORMAL, &err);
    if(tfm_promote_comp_thread == NULL) {
        DBG_LOG(SEVERE, MOD_TFM, "TFM delete thread did not get initialized.");
        DBG_ERR(SEVERE, "TFM delete thread did not get initialized.");
        g_error_free(err);
	glob_tfm_initialized = 0;
        return -1;
    }


    /* Promote thread */
    if ((ret = pthread_create(&tfm_promote_thread_obj, NULL,
			      TFM_promote_thread_hdl, NULL))) {
	glob_tfm_initialized = 0;
        return -1;
    }

    ret_val = pthread_mutex_init(&tfm_promote_mutex, NULL);
    if(ret_val < 0) {
        DBG_LOG(SEVERE, MOD_TFM, "TFM Promote Mutex not created. "
		"Severe TFM failure");
	DBG_ERR(SEVERE, "TFM Promote Mutex not created. "
                "Severe TFM failure");
	glob_tfm_initialized = 0;
        return -1;
    }

    ret_val = pthread_mutex_init(&tfm_promote_comp_mutex, NULL);
    if(ret_val < 0) {
        DBG_LOG(SEVERE, MOD_TFM, "TFM Promote Complete Mutex not created. "
		"Severe TFM failure");
	DBG_ERR(SEVERE, "TFM Promote Complete Mutex not created. "
                "Severe TFM failure");
	glob_tfm_initialized = 0;
        return -1;
    }

#if 0
    s_tfm_handle_partial_promote_files();
    s_tfm_unlink(s_tfm_promote_complete_file_name);
    s_tfm_unlink(s_tfm_promote_pending_file_name);
#endif
    glob_tfm_initialized = 1;
    return 0;
}       /* TFM_init */

static int
s_tfm_init_file_id_list(void)
{
    int i;
    nkn_tfm_file_id_list_ptr = nkn_calloc_type(tfm_max_num_files_limit,
					    sizeof(tfm_file_id_list),
					    mod_tfm_file_id_list);
    if(nkn_tfm_file_id_list_ptr == NULL) {
        DBG_LOG(SEVERE, MOD_TFM, "TFM calloc failure. "
		"Severe TFM failure");
	DBG_ERR(SEVERE, "TFM calloc failure. "
                "Severe TFM failure");
	return -1;
    }

    if(pthread_mutex_init(&nkn_tfm_file_id_mutex, NULL) < 0) {
        DBG_LOG(SEVERE, MOD_TFM, "TFM File id Mutex not created. "
		"Severe TFM failure");
	DBG_ERR(SEVERE, "TFM File id Mutex not created. "
                "Severe TFM failure");
        return -1;
    }

    pthread_mutex_lock(&nkn_tfm_file_id_mutex);
    STAILQ_INIT(&nkn_tfm_file_id_list_q);
    for(i=0;i<tfm_max_num_files_limit;i++) {
	nkn_tfm_file_id_list_ptr[i].file_id = i;
	STAILQ_INSERT_TAIL(&nkn_tfm_file_id_list_q,
	       &nkn_tfm_file_id_list_ptr[i],
	       entries);
    }
    pthread_mutex_unlock(&nkn_tfm_file_id_mutex);
    return 0;
}

/* The following functions are used to maintain, get and free file_id 
 * which are used to create the filename used for the TFM files.
 * The file name is reused because linux uses the dentry for unique
 * filenames, which will cause the memory to be run out if the filenames 
 * are not reused.
 */
/* Initialize the file id list */
static int
s_tfm_get_file_id(void)
{
    tfm_file_id_list *tfm_file_id_list_ptr = NULL;
    pthread_mutex_lock(&nkn_tfm_file_id_mutex);
    tfm_file_id_list_ptr = STAILQ_FIRST(&nkn_tfm_file_id_list_q);
    if(tfm_file_id_list_ptr) {
	STAILQ_REMOVE_HEAD(&nkn_tfm_file_id_list_q, entries);
	pthread_mutex_unlock(&nkn_tfm_file_id_mutex);
	return tfm_file_id_list_ptr->file_id;
    }
    pthread_mutex_unlock(&nkn_tfm_file_id_mutex);
    return -1;
}

/* Function to get a free file id */
static void
s_tfm_free_file_id(int id)
{
    if(id < 0 || id >= tfm_max_num_files_limit)
       return;
    pthread_mutex_lock(&nkn_tfm_file_id_mutex);
    STAILQ_INSERT_HEAD(&nkn_tfm_file_id_list_q, &nkn_tfm_file_id_list_ptr[id],
	       entries);
    pthread_mutex_unlock(&nkn_tfm_file_id_mutex);
}

/* Function to release a used file id after ingestion */
static int
s_construct_data_map(MM_put_data_t *map)
{
    int id = s_tfm_get_file_id ();
    if(id != -1) {
	snprintf(map->clip_filename, sizeof(map->clip_filename)-1, "%s/%d",
		g_cache_info[s_input_cur_write_dir_idx].dirname,
		id);
	map->clip_filename[sizeof(map->clip_filename)-1] = '\0';
    }
    return id;
}

/* Needs to be called with nkn_tfm_pht_mutex held*/
static int
s_check_cache_allow_ingest(uint64_t file_len)
{
    /* This mode executes when we could not find the size of the /nkn
       filesystem*/
    if(glob_tfm_spc_default_mode == 1) {
	if((glob_tfm_spc_pht_num_files > glob_tfm_max_partial_files) ||
	   (glob_tfm_spc_cht_num_files > glob_tfm_max_complete_files)) {
	    if(glob_tfm_spc_cht_num_files > glob_tfm_max_complete_files) {
		glob_tfm_spc_cht_max_files_exceeded ++;
	    } else {
		glob_tfm_spc_pht_max_files_exceeded ++;
	    }
	    return -1;
	}
	return 0;
    }

    glob_tfm_spc_pht_thresh = (glob_tfm_spc_pht_bytes_avail);
    glob_tfm_spc_cht_thresh = (glob_tfm_spc_cht_bytes_avail);
    file_len = 0;

    /* If there are no files, no need to check for reservations
     * If partial files are present, check if there is space for one
     * more file of size == average size of the partial files
     * present in the TFM directory
     */
    if(glob_tfm_spc_pht_num_files && glob_tfm_spc_pht_bytes_resv) {
	if((glob_tfm_spc_pht_bytes_resv) >=
		(int64_t)(glob_tfm_spc_pht_bytes_avail -
		(glob_tfm_spc_pht_bytes_resv/glob_tfm_spc_pht_num_files))) {
	    glob_tfm_spc_pht_max_bytes_exceeded ++;
	    return -1;
	}
    }

    /* Only fill completed files to 95% of total amount
       allocated because we may still have partial files
       still needing to be completed.*/
    if((glob_tfm_spc_cht_bytes_used + (int64_t)file_len) >=
       glob_tfm_spc_cht_thresh) {
	glob_tfm_spc_cht_max_bytes_exceeded ++;
	return -2;
    }

    /* Start taking out partial files earlier than when
       the filesystem gets full. We will always leave some percent of
       of the pht amount available for partial files to complete.*/
    if(glob_tfm_spc_pht_bytes_resv >=
       glob_tfm_spc_pht_thresh) {
	glob_tfm_spc_pht_thresh_reached ++;
	glob_tfm_spc_pht_bytes_resv += file_len;
	return -3;
    }

    glob_tfm_spc_pht_bytes_resv += file_len;
    return 0;
}


int
tfm_cod_test_and_get_uri(nkn_cod_t cod, char **uri)
{
    int len;
    int ret = -1;

    ret = nkn_cod_get_cn(cod, uri, &len);
    if(ret == NKN_COD_INVALID_COD) {
	uri = NULL;
	return -1;
    }
    return 0;
}


/**
   This function takes a data pointer and information about a file as
   input and populates persistent storage (disk) with that information.
   Also, it will update a map file that stores meta-data about that
   content.
*/
int
TFM_put(struct MM_put_data *map,
	uint32_t dummy __attribute((unused)))
{
    TFM_obj      *pobj = NULL;
    int          ret_val = -1;
    int          len;
    int          ret = -1, file_id;
    time_t       mm_last_modified = 0, pobj_last_modified = 0;
    char         *uri = NULL;

    ret_val = tfm_cod_test_and_get_uri(map->uol.cod, &uri);
    if(ret_val < 0) {
	glob_tfm_put_cod_invalid ++;
	return -1;
    }

    DBG_LOG(MSG, MOD_TFM, "uri = %s, offset = %lld, len = %llu, total = %llu, clipfn = %s",
                           uri,  map->uol.offset,
			   map->uol.length, map->total_length,
			   map->clip_filename);

    /* Though TFM will not be called for objects other than 
     * chunked encoded, reject all the other objects anway
     */
    if (!(map->flags & MM_FLAG_PRIVATE_BUF_CACHE))
	return 0;

    pthread_mutex_lock(&nkn_tfm_pht_mutex);
    glob_tfm_pht_mutex_lock_calls ++;
#if 0
    s_tfm_dbg_write_brange_all(map, uri, map->uol.offset,
			   map->uol.length, map->total_length,
			   map->clip_filename);
#endif

    glob_tfm_put_cnt++;
    pobj = nkn_tfm_cod_tbl_get(nkn_tfm_partial_uri_hash_table,
			       map->uol.cod);
    if(pobj && pobj->attr) {
        pobj_last_modified = s_get_last_modified_from_attr(pobj->attr);
        if(map->attr)
           mm_last_modified = s_get_last_modified_from_attr(map->attr);
	DBG_LOG(MSG, MOD_TFM, "mm_lm = %llu, pobj_lm = %llu, expiry = %llu",
                mm_last_modified, pobj_last_modified, pobj->attr->cache_expiry);
        if((mm_last_modified && (mm_last_modified != pobj_last_modified))
            || (!mm_last_modified && (pobj->attr->cache_expiry < nkn_cur_ts ))) {
	    pobj->defer_delete = 0;
	    pobj->is_complete = 0;
	    glob_tfm_pht_obj_expired ++;
	    if(!pobj->partial_file_write) {
		glob_tfm_spc_pht_bytes_used -= pobj->in_bytes;
		glob_tfm_spc_pht_bytes_resv -= pobj->in_bytes;
	    }
            TAILQ_REMOVE(&nkn_tfm_cleanup_q, pobj, entries);
	    if(pobj->partial_queue_added) {
		TAILQ_REMOVE(&nkn_tfm_part_promote_q, pobj, promote_entries);
		glob_tfm_getm_partial_cleanup_queue_removed++;
	    }
	    if(!pobj->is_buf_only)
		s_tfm_unlink(pobj->filename);
	    s_tfm_free_file_id(pobj->file_id);
	    nkn_tfm_cod_tbl_delete(nkn_tfm_partial_uri_hash_table,
				   pobj->cod);
            glob_tfm_pht_delete ++;
	    glob_tfm_spc_pht_num_files--;
            pobj = NULL;
        }
    }
    if(pobj) {
	if(pobj->in_bytes > (glob_tfm_spc_pht_bytes_avail/glob_tfm_spc_pht_num_files)) {
	    DBG_LOG(WARNING, MOD_TFM, "File %u is not ingested because of tfm disk"
		    "space contraint", uri);
	    pobj->defer_delete = 0;
	    pobj->is_complete = 0;
	    if(!pobj->partial_file_write) {
		glob_tfm_spc_pht_bytes_used -= pobj->in_bytes;
		glob_tfm_spc_pht_bytes_resv -= pobj->in_bytes;
	    }
	    TAILQ_REMOVE(&nkn_tfm_cleanup_q, pobj, entries);
	    if(pobj->partial_queue_added) {
		TAILQ_REMOVE(&nkn_tfm_part_promote_q, pobj, promote_entries);
		glob_tfm_getm_partial_cleanup_queue_removed++;
	    }
	    if(!pobj->is_buf_only)
		s_tfm_unlink(pobj->filename);
	    s_tfm_free_file_id(pobj->file_id);
	    nkn_tfm_cod_tbl_delete(nkn_tfm_partial_uri_hash_table,
				    pobj->cod);
	    glob_tfm_pht_delete ++;
	    glob_tfm_spc_pht_num_files--;
	    /* We have deleted a file because of space unavailability 
	     * Mark for new file rejection, until atleast one file gets 
	     * completed
	     */
	    glob_tfm_pht_spc_sac_reject = 1;
	    pthread_mutex_unlock(&nkn_tfm_pht_mutex);
	    return 0;
	}
    }
    if (pobj == NULL) {
	/* Dont try to ingest object with offset != 0. We will never get the object
	 */
	if(map->uol.offset != 0) {
	    pthread_mutex_unlock(&nkn_tfm_pht_mutex);
	    return -1;
	}
#if 0
        if(map->src_manager != TASK_TYPE_GET_MANAGER_TFM) {
            if(glob_tfm_cht_buf_only_pend >= glob_tfm_cht_max_buf_only_pend) {
                glob_tfm_put_buf_only_reject++;
                pthread_mutex_unlock(&nkn_tfm_pht_mutex);
                return -1;
            }
        }
#endif
	if(map->src_manager == TASK_TYPE_GET_MANAGER_TFM) {
	    if((glob_tfm_spc_pht_num_files > glob_tfm_max_partial_files)) {
		glob_tfm_pht_last_del_time = nkn_cur_ts;
		glob_tfm_put_reject ++;
		tfm_num_delete_chunk = 100;
		pthread_cond_signal(&cleanup_thr_cond);
		pthread_mutex_unlock(&nkn_tfm_pht_mutex);
		return -1;
	    }
	} else {
	    /* Check for SAC whether tfm can accept new 
	     * files. If there are no files in the pht,
	     * clear the SAC_reject, reset the reservations
	     */
	    if(glob_tfm_pht_spc_sac_reject &&
		    glob_tfm_spc_pht_num_files) {
		glob_tfm_put_sac_fail ++;
		pthread_mutex_unlock(&nkn_tfm_pht_mutex);
		return -1;
	    } else if (!glob_tfm_spc_pht_num_files) {
		glob_tfm_spc_pht_bytes_used = 0;
		glob_tfm_spc_pht_bytes_resv = 0;
		glob_tfm_pht_spc_sac_reject = 0;
	    }
	    ret = s_check_cache_allow_ingest(map->total_length);
	    if(ret < 0) {
		/* If we have exceeded the 95% thresold, call delete every 10s */
		if((glob_tfm_spc_pht_bytes_resv >= glob_tfm_spc_pht_thresh) &&
		((nkn_cur_ts - glob_tfm_pht_last_del_time > 10))) {
		    /* Call delete every N seconds (after the first delete round)
		    so that we don't flood the deletes*/
		    glob_tfm_pht_last_del_time = nkn_cur_ts;
		    glob_tfm_put_reject ++;
		    tfm_num_delete_chunk = 100;
		    pthread_cond_signal(&cleanup_thr_cond);
		    pthread_mutex_unlock(&nkn_tfm_pht_mutex);
		    return -1;
		} else if ((ret == -1) || (ret == -2)){
		    /* Reject the new put if the partial or the completed table is full*/
		    glob_tfm_put_reject2 ++;
		    pthread_mutex_unlock(&nkn_tfm_pht_mutex);
		    return -1;
		}
		/* if the threshold has been reached, we allow the file. If we have exceeded
		the total reserved space, we reject*/
	    }
	}

	/* If a new file is successfully added and we go past the threshold,
	   the next time, we will delete immediately. */
	glob_tfm_pht_last_del_time = 0;
        file_id = s_construct_data_map(map);
        if(file_id == -1 ) {
	    glob_tfm_max_num_parallel_files_exceeded ++;
	    pthread_mutex_unlock(&nkn_tfm_pht_mutex);
	    return -1;
	}
	glob_tfm_new_put_cnt++;
        pobj = s_populate_new_uri(map->uol.cod,
				  map->clip_filename,
				  map->total_length);
        if(!pobj) {
            DBG_LOG(MSG, MOD_TFM, "Object not created: uri: %s",
                    uri);
	    goto put_reject;
	}
	pobj->file_id = file_id;

	/* Addon to bug 2906: Set partial file_write for GetM objects.
	 * We have 2 put threads, the next put thread
	 * can use this object immediately before the flag
	 * is set in the s_tfm_partial_promote_file()
	 * which will cause the below set of fix for bug 2906
	 * to reject the puts
	 */
	if(map->src_manager == TASK_TYPE_GET_MANAGER_TFM) {
	    pobj->partial_file_write = 1;
	}

	TAILQ_INSERT_TAIL(&nkn_tfm_cleanup_q, pobj, entries);
	/* Space is reserved in TFM (pht table) for this file */
	pobj->pht_spc_resv = 1;
	glob_tfm_spc_pht_num_files++;
	glob_tfm_spc_pht_tot_num_files++;
    } else {
	if(pobj->is_complete == 1) {
	    /* Object is already in the tmp directory */
            DBG_LOG(MSG, MOD_TFM, "Object already "\
		    "complete: uri: %s",
                    uri);
	    goto error;
	}
	/* Resolved: bug 2906
	 * For the Chunked encoded objects, Apache is
	 * returning both chunked response as well as the
	 * normal response with the content length
	 * At some point, tfm has an object that created with
	 * Chunked response, and then the same object is being
	 * ingested with a Getm response.
	 * Trash the unwanted objects as anyway we are writing
	 * to cache.
	 */
	if((map->src_manager == TASK_TYPE_GET_MANAGER_TFM)
		&& !pobj->partial_file_write) {
	    glob_tfm_invalid_getm_obj++;
	    goto put_reject;
	}
	if ((map->flags & MM_FLAG_PRIVATE_BUF_CACHE) && pobj->partial_file_write) {
	    glob_tfm_invalid_chunked_obj++;
	    goto put_reject;
	}
	pobj->time_modified = nkn_cur_ts;
        len = sizeof(pobj->filename);
        memcpy(map->clip_filename, pobj->filename, len-1);
        map->clip_filename[len-1] = '\0';
        TAILQ_REMOVE(&nkn_tfm_cleanup_q, pobj, entries);
	TAILQ_INSERT_TAIL(&nkn_tfm_cleanup_q, pobj, entries);
    }

    pthread_mutex_unlock(&nkn_tfm_pht_mutex);
    ret_val = s_input_store_content_in_free_block(map);
    return ret_val;

 put_reject:
    glob_tfm_put_reject ++;
    pthread_mutex_unlock(&nkn_tfm_pht_mutex);
    return -1;

 error:
    glob_tfm_put_err++;
    pthread_mutex_unlock(&nkn_tfm_pht_mutex);
    return -1;
}


int
TFM_stat(nkn_uol_t      uol,
         MM_stat_resp_t *in_ptr_resp)
{
    TFM_obj     *pobj = NULL;
    struct      stat sb;
    int         blk_id;

    pthread_mutex_lock(&nkn_tfm_cht_mutex);
    glob_tfm_cht_mutex_lock_calls ++;
    pobj = (TFM_obj *) nkn_tfm_cod_tbl_get (nkn_tfm_completed_uri_hash_table,
					    uol.cod);
    if(pobj == NULL) {
        /* Handle stat error */
        glob_tfm_stat_no_obj ++;
        pthread_mutex_unlock(&nkn_tfm_cht_mutex);
        goto tfm_stat_not_found;
    }

    if(pobj->cod != uol.cod) {
        glob_tfm_stat_no_cod ++;
	pobj->defer_delete = 1;
        pthread_mutex_unlock(&nkn_tfm_cht_mutex);
        goto tfm_stat_not_found;
    }

    if(pobj->is_buf_only == 1) {
        glob_tfm_stat_obj_buf_only++;
        pthread_mutex_unlock(&nkn_tfm_cht_mutex);
        goto tfm_stat_not_found;
    }

    if(pobj->is_complete != 1) {
        glob_tfm_stat_obj_incomplete ++;
        pthread_mutex_unlock(&nkn_tfm_cht_mutex);
        goto tfm_stat_not_found;
    }

    if(pobj->attr->cache_expiry < nkn_cur_ts) {
        glob_tfm_stat_obj_expired ++;
        pthread_mutex_unlock(&nkn_tfm_cht_mutex);
        goto tfm_stat_not_found;
    }

    if (stat(pobj->filename, &sb) < 0) {
        glob_tfm_stat_obj_failed ++;
        DBG_LOG(MSG, MOD_TFM, "\n Could not stat filename: %s",
                pobj->filename);
        pthread_mutex_unlock(&nkn_tfm_cht_mutex);
        goto tfm_stat_not_found;
    }

    in_ptr_resp->media_blk_len = NKN_TFM_BLK_LEN;
    blk_id = in_ptr_resp->in_uol.offset / NKN_TFM_BLK_LEN;
    snprintf(in_ptr_resp->physid, sizeof(in_ptr_resp->physid)-1,
	     "%s?%d", pobj->filename, blk_id);
    in_ptr_resp->physid[sizeof(in_ptr_resp->physid)-1] = '\0';

    pthread_mutex_unlock(&nkn_tfm_cht_mutex);

    in_ptr_resp->ptype              = TFMMgr_provider;
    in_ptr_resp->tot_content_len    = sb.st_size;

    if(uol.offset > sb.st_size) {
        in_ptr_resp->mm_stat_ret = -1;
        return -1;
    }
    glob_tfm_stat_success_cnt ++;
    return 0;

 tfm_stat_not_found:
    glob_tfm_stat_not_found++;
    in_ptr_resp->tot_content_len = 0;
    return -1;
}

static int
s_get_content(const char     *in_filename,
              const uint64_t in_offset,
              struct iovec   *in_iov,
              const int32_t  in_iovcnt,
              uint64_t       *out_read_len)
{
    int fd;
    int out_errno = 0;
    off_t long_retf;

    //fd = open(in_filename, O_RDONLY | O_DIRECT);
    fd = open(in_filename, O_RDONLY);
    if (fd < 0) {
        out_errno = errno;
        glob_tfm_num_open_err ++;
        DBG_LOG(SEVERE, MOD_TFM, "\n%s(): FILESYSTEM OPEN "
                "FAILED %s \n", __FUNCTION__, in_filename);
	DBG_ERR(SEVERE, "\n%s(): FILESYSTEM OPEN "
                "FAILED %s \n", __FUNCTION__, in_filename);
        return out_errno;
    }
    glob_tfm_num_files_open ++;

    long_retf = lseek(fd, in_offset, SEEK_SET);
    if (long_retf >= 0) {
        long_retf = readv(fd, in_iov, in_iovcnt);
        if (long_retf <= 0) {
	    if(long_retf == -1)
		out_errno = errno;
	    else
		out_errno = -1;
            glob_tfm_num_read_err ++;
            DBG_LOG(SEVERE, MOD_TFM, "\n%s(): FILESYSTEM "
                    "READ ERROR: %s %ld",
                    __FUNCTION__, in_filename, long_retf);
	    DBG_ERR(SEVERE, "\n%s(): FILESYSTEM "
                    "READ ERROR: %s %ld",
                    __FUNCTION__, in_filename, long_retf);

	    /* Bug 3313: Issue caused by Network code to close a valid
	     * FD. Adding a NKN_ASSERT to capture any future issues 
	     */
	    NKN_ASSERT(out_errno != EBADF);
        }
        *out_read_len = long_retf;
    }
    else {
	if(long_retf == -1)
	    out_errno = errno;
	else
	    out_errno = -1;
        glob_tfm_num_seek_err ++;
        DBG_LOG(SEVERE, MOD_TFM, "\n%s(): FILESYSTEM "
                "SEEK ERROR: %ld", __FUNCTION__, long_retf);
	DBG_ERR(SEVERE, "\n%s(): FILESYSTEM "
                "SEEK ERROR: %ld", __FUNCTION__, long_retf);
	/* Bug 3313: Issue caused by Network code to close a valid
	 * FD. Adding a NKN_ASSERT to capture any future issues 
	 */
	NKN_ASSERT(out_errno != EBADF);
    }

    close(fd);
    glob_tfm_num_files_closed ++;
    return out_errno;
}

static int
s_tfm_get_name_from_physid(char *physid,
			   char *tfmfile,
			   int maxlen,
			   int *dummy __attribute((unused)))
{
    char* dummy_str = NULL;
    char  store_str[maxlen];
    char  ret_str[maxlen];
    char  *tmpp = &ret_str[0];

    if((int)strlen(physid) > maxlen) {
	return -1;
    }

    memcpy(store_str, physid, strlen(physid));
    store_str[strlen(physid)] = '\0';
    tmpp = strtok_r(store_str, "?", &dummy_str);
    snprintf(tfmfile, maxlen, "%s", tmpp);
    return 0;
}

int
TFM_get(MM_get_resp_t *in_ptr_resp)
{
#define TFM_STACK_ALLOC (1024*1024/CM_MEM_PAGE_SIZE)
    int                     i, j;
    struct iovec    iovp_mem[TFM_STACK_ALLOC];
    struct iovec    *iov = iovp_mem;
    void            *content_ptr = NULL;
    nkn_attr_t      *attr_ptr = NULL;
    void            *ptr_vobj;
    int             in_num_obj = in_ptr_resp->in_num_content_objects;
    nkn_uol_t       uol;
    uint64_t        length_read = 0;
    int             cont_idx = 0;
    int             num_pages = 0;
    int             partial_pages = 0;
    uint64_t        read_length = 0;
    uint64_t        read_offset = 0;
    int             iov_alloced = 0;
    int             ret_val = -1;
    uint64_t        tot_content_len = 0;
    TFM_obj         *pobj = NULL;
    char            tfmfile[PATH_MAX + MAX_URI_SIZE];
    int             dummy;
    int             ret = -1;
    char            *uri;

    glob_tfm_get_cnt ++;
    pthread_mutex_lock(&nkn_tfm_cht_mutex);
    ret_val = tfm_cod_test_and_get_uri(in_ptr_resp->in_uol.cod, &uri);
    if(ret_val < 0) {
	glob_tfm_get_cod_invalid ++;
	goto tfm_get_error;
    }

    DBG_LOG(MSG, MOD_TFM, "1.TFM_get: uri: %s off: %ld, len: %ld",
	   uri,
	   in_ptr_resp->in_uol.offset, in_ptr_resp->in_uol.length);

    glob_tfm_cht_mutex_lock_calls ++;
    pobj = (TFM_obj *) nkn_tfm_cod_tbl_get (nkn_tfm_completed_uri_hash_table,
					    in_ptr_resp->in_uol.cod);
    if(pobj == NULL) {
        in_ptr_resp->err_code = -NKN_TFM_GET_OBJ_ERR;
        goto tfm_get_error;
    }

    if(pobj->cod != in_ptr_resp->in_uol.cod) {
        glob_tfm_get_no_cod ++;
	pobj->defer_delete = 1;
        goto tfm_get_error;
    }

    if(pobj->is_complete != 1) {
        in_ptr_resp->err_code = -NKN_TFM_GET_INCOMPLETE_ERR;
        goto tfm_get_error;
    }

    tot_content_len = pobj->tot_content_len;

    if(in_num_obj > TFM_STACK_ALLOC) {
        iov = nkn_calloc_type(sizeof(struct iovec), in_num_obj,
                                     mod_tfm_iov);
        if(iov == NULL) {
            in_ptr_resp->err_code = -NKN_TFM_GET_MEM_ERR;
            goto tfm_get_error;
        }
        iov_alloced = 1;
    }
    uol.cod = in_ptr_resp->in_uol.cod;
    uol.offset = in_ptr_resp->in_uol.offset;
    uol.length = in_ptr_resp->in_uol.length;
    /* Align the read offset to the current page size */
    read_offset = uol.offset & ~(NKN_TFM_BLK_LEN - 1);

    ret = s_tfm_get_name_from_physid(in_ptr_resp->physid,
				     tfmfile, sizeof(tfmfile)-1, &dummy);
    if(ret < 0) {
	in_ptr_resp->err_code = -NKN_TFM_GET_NAME_ERR;
	goto tfm_get_error;
    }

    if((in_ptr_resp->in_attr != NULL) && (pobj->attr != NULL)) {
	attr_ptr = (nkn_attr_t *) nkn_buffer_getcontent(in_ptr_resp->in_attr);
        /* Attributes are already provided. Just copy it. We could copy just
	 * blob_attrsize bytes, but we want to hide that name. */
        memcpy(attr_ptr, pobj->attr, attr_ptr->total_attrsize);
	nkn_buffer_setid(in_ptr_resp->in_attr, &uol, TFMMgr_provider, 0);
    } else if(in_ptr_resp->in_attr != NULL) {
        in_ptr_resp->err_code = -NKN_TFM_GET_ATTR_ERR;
        glob_tfm_get_attr_err ++;
        goto tfm_get_error;
    }
    pthread_mutex_unlock(&nkn_tfm_cht_mutex);

    /* Set the iovec to contain the buf pointers from bufmgr*/
    for(i = 0; i < in_num_obj; i++) {
        ptr_vobj = in_ptr_resp->in_content_array[i];
        assert(ptr_vobj);

        content_ptr = nkn_buffer_getcontent(ptr_vobj);
        assert(content_ptr);

        iov[i].iov_base = content_ptr;
        iov[i].iov_len = CM_MEM_PAGE_SIZE;
    }

    DBG_LOG(MSG, MOD_TFM, "2.TFM_get: uri: %s uol.offset = %ld, "
	    "uol.length = %ld read_offset:%ld length_read = %ld",
	    uri, uol.offset, uol.length, read_offset,
	    length_read);

    ret_val = s_get_content(tfmfile, read_offset,
                            iov, in_num_obj, &length_read);

    if(ret_val != 0) {
            in_ptr_resp->err_code = -NKN_TFM_GET_READ_ERR;
            goto tfm_get_error;
    }

    /* Need to set the correct ids on the buffers of bufmgr*/
    if(length_read > tot_content_len) {
        in_ptr_resp->err_code = -NKN_TFM_GET_LEN_ERR;
        goto tfm_get_error_cleanup_obj;
    }
    if(length_read >= tot_content_len) {
	length_read = tot_content_len;
    }

    num_pages = length_read / CM_MEM_PAGE_SIZE;
    partial_pages = length_read % CM_MEM_PAGE_SIZE;
    if(partial_pages > 0)
        num_pages ++;

    read_length = length_read;
    glob_tot_read_from_tfm += read_length;

    /* Set the IDs of each buffer for bufmgr to identify  */
    for (j = 1; j <= num_pages; j++) {
        if(in_ptr_resp->out_num_content_objects ==
           in_ptr_resp->in_num_content_objects) {
            break;
        }
        ptr_vobj = in_ptr_resp->in_content_array[cont_idx];
        assert(ptr_vobj);

        uol.offset = read_offset;
        if(read_length >= CM_MEM_PAGE_SIZE) {
            uol.length = CM_MEM_PAGE_SIZE;
            read_offset += CM_MEM_PAGE_SIZE;
            read_length -= CM_MEM_PAGE_SIZE;
        }
        else {
            uol.length = read_length;
            read_offset += read_length;
            read_length = 0;
        }

        nkn_buffer_setid(ptr_vobj, &uol, TFMMgr_provider, 0);
        in_ptr_resp->out_num_content_objects++;
        cont_idx ++;

        DBG_LOG(MSG, MOD_TFM,
		"3.TFM_get: iov_cnt: %d, iov buf:%p len:%ld uol.offset=%ld, "
                "uol.length=%ld", cont_idx, iov[j-1].iov_base, iov[j-1].iov_len,
                uol.offset, uol.length);
    }

    if(iov_alloced)
        free(iov);
    glob_tfm_get_success ++;

    return ret_val;

 tfm_get_error:
    glob_tfm_get_err ++;
    pthread_mutex_unlock(&nkn_tfm_cht_mutex);
    if(iov_alloced)
        free(iov);
    return -1;

 tfm_get_error_cleanup_obj:
    pthread_mutex_lock(&nkn_tfm_cht_mutex);
    glob_tfm_cht_mutex_lock_calls ++;
    pobj = (TFM_obj *)nkn_tfm_cod_tbl_get(nkn_tfm_completed_uri_hash_table,
					  uol.cod);
    if(pobj) {
	pobj->defer_delete = 0;
	pobj->is_complete = 0;
	if(!pobj->is_buf_only)
	    s_tfm_unlink(pobj->filename);
	glob_tfm_spc_cht_bytes_used -= pobj->in_bytes;
	s_tfm_free_file_id(pobj->file_id);
	nkn_tfm_cod_tbl_delete(nkn_tfm_completed_uri_hash_table,
			       pobj->cod);
	glob_tfm_spc_cht_num_files --;
        glob_tfm_cht_delete ++;
    }
    glob_tfm_get_too_much_data_err ++;
    pthread_mutex_unlock(&nkn_tfm_cht_mutex);
    if(iov_alloced)
        free(iov);
    return -1;
}

void
TFM_cleanup(void)
{
    nkn_tfm_cod_tbl_cleanup(nkn_tfm_completed_uri_hash_table);
    nkn_tfm_cod_tbl_cleanup(nkn_tfm_partial_uri_hash_table);
    if(nkn_tfm_promote_hash_table)
	nkn_tfm_uri_tbl_cleanup(nkn_tfm_promote_hash_table);
    tfm_async_q = NULL;
}

int
TFM_delete(MM_delete_resp_t *in_ptr_resp __attribute((unused)))
{
    return 0;
}

int
TFM_shutdown(void)
{
    return 0;
}

int
TFM_discover(struct mm_provider_priv *p __attribute((unused)))
{
    return 0;
}

int
TFM_evict(void)
{
    return 0;
}

int
TFM_provider_stat (MM_provider_stat_t *tmp)
{
    tmp->weight = 0;
    tmp->percent_full = 0;
    return 0;
}

/* This function is an api to indicate to the TFM that a uri has been
 promoted. */
void
TFM_promote_complete(MM_promote_data_t *tfm_data)
{
    TFM_promote_comp_t *tfm_pr_comp_data;

    tfm_pr_comp_data = (TFM_promote_comp_t *)
	nkn_calloc_type (1, sizeof(TFM_promote_comp_t),
			 mod_tfm_obj);
    if(!tfm_pr_comp_data) {
	return;
    }
    tfm_pr_comp_data->cod = nkn_cod_dup(tfm_data->in_uol.cod, NKN_COD_TFM_MGR);
    if(tfm_pr_comp_data->cod == NKN_COD_NULL) {
	glob_tfm_cod_null_err ++;
        free(tfm_pr_comp_data);
        return;
    }
    tfm_pr_comp_data->out_ret_code = tfm_data->out_ret_code;
    tfm_pr_comp_data->partial_file_write = tfm_data->partial_file_write;
    pthread_mutex_lock(&tfm_promote_comp_mutex);
    glob_tfm_promote_complete_start++;
    STAILQ_INSERT_TAIL(&nkn_tfm_promote_comp_q, tfm_pr_comp_data, entries);
	pthread_cond_signal(&tfm_promote_comp_thr_cond);
    pthread_mutex_unlock(&tfm_promote_comp_mutex);
}

void TFM_promote_wakeup()
{
    pthread_mutex_lock(&tfm_promote_mutex);
    pthread_cond_signal(&tfm_promote_thr_limit_cond);
    pthread_mutex_unlock(&tfm_promote_mutex);
}

void s_tfm_promote_complete_thread_hdl()
{
    TFM_promote_comp_t *tfm_pr_comp_data;
    TFM_obj *pobj;
    AM_pk_t  pk;
    char    *filename;
    char    *uri;
    int     ret_val, file_id;

    prctl(PR_SET_NAME, "nvsd-tfm-promcomp", 0, 0, 0);

    while(1) {
	pthread_mutex_lock(&tfm_promote_comp_mutex);
	tfm_pr_comp_data = NULL;
	tfm_pr_comp_data = STAILQ_FIRST(&nkn_tfm_promote_comp_q);
	if(tfm_pr_comp_data) {
	    STAILQ_REMOVE_HEAD(&nkn_tfm_promote_comp_q, entries);
            glob_tfm_promote_complete_end++;
            pthread_mutex_unlock(&tfm_promote_comp_mutex);
            pthread_mutex_lock(&tfm_promote_mutex);
            pthread_cond_signal(&tfm_promote_thr_limit_cond);
            pthread_mutex_unlock(&tfm_promote_mutex);

            glob_tfm_cht_mutex_lock_calls ++;
	    AO_fetch_and_add1(&glob_tfm_promote_files_promote_end);	

            if(tfm_pr_comp_data->partial_file_write) {
		pthread_mutex_lock(&nkn_tfm_pht_mutex);
                glob_tfm_pht_mutex_lock_calls ++;
                pobj = (TFM_obj *)nkn_tfm_cod_tbl_get(nkn_tfm_partial_uri_hash_table,
					          tfm_pr_comp_data->cod);
                if(pobj == NULL) {
		    pthread_mutex_lock(&nkn_tfm_cht_mutex);
                    glob_tfm_num_promotes_in_progress --;
	            /* This case should never be hit */
	            glob_tfm_promote_obj_not_found_after_complete ++;
                    pthread_mutex_unlock(&nkn_tfm_cht_mutex);
		    pthread_mutex_unlock(&nkn_tfm_pht_mutex);
                    nkn_cod_close(tfm_pr_comp_data->cod, NKN_COD_TFM_MGR);
                    free(tfm_pr_comp_data);
	            continue;
                }

		ret_val = tfm_cod_test_and_get_uri(pobj->cod, &uri);
		if(ret_val < 0) {
		    assert(0);
		    glob_tfm_promote_complete_cod_invalid ++;
		    pthread_mutex_lock(&nkn_tfm_cht_mutex);
                    glob_tfm_num_promotes_in_progress --;
	            /* This case should never be hit */
	            glob_tfm_promote_obj_not_found_after_complete ++;
                    pthread_mutex_unlock(&nkn_tfm_cht_mutex);
		    pthread_mutex_unlock(&nkn_tfm_pht_mutex);
                    nkn_cod_close(tfm_pr_comp_data->cod, NKN_COD_TFM_MGR);
                    free(tfm_pr_comp_data);
	            continue;
		}

                if(tfm_pr_comp_data->out_ret_code == 0) {
		    pobj->partial_promote_complete++;
		    pobj->time_last_promoted = nkn_cur_ts;
		}else{
		    pobj->partial_promote_err_complete++;
		    pobj->partial_promote_in_progress = 0;
		}
                if(pobj->partial_promote_complete == pobj->total_blocks) {
		    glob_tfm_getm_promote_complete++;
		    pobj->is_complete = 1;

                    /* Set this video promoted so that duplicate promotes of the same
                       object does not happen. */
                    pk.name = nkn_strdup_type(uri, mod_tfm_strdup1);
		    pk.provider_id = TFMMgr_provider;
		    pk.sub_provider_id = 0;
		    pk.type = AM_OBJ_URI;
		    AM_video_promoted(&pk);

		    pk.provider_id = TFMMgr_provider;
		    pk.sub_provider_id = 0;
		    pk.type = AM_OBJ_URI;
		    AM_change_obj_provider(&pk, pobj->dst,
						0);
#if 0
		    s_tfm_write_promote_complete_file(uri);
#endif
		    if(pk.name)
			free(pk.name);
		    if(pobj->partial_queue_added) {
			TAILQ_REMOVE(&nkn_tfm_part_promote_q, pobj, promote_entries);
			glob_tfm_getm_partial_cleanup_queue_removed++;
		    }
		    if(!pobj->is_buf_only)
			s_tfm_unlink(pobj->filename);
		    TAILQ_REMOVE(&nkn_tfm_cleanup_q, pobj, entries);
		    s_tfm_free_file_id(pobj->file_id);
		    nkn_tfm_cod_tbl_delete(nkn_tfm_partial_uri_hash_table,
				   pobj->cod);
                    glob_tfm_pht_delete ++;
		    glob_tfm_spc_pht_num_files--;
                } else {
		    pthread_cond_signal(&cleanup_thr_cond);
		}
                pthread_mutex_unlock(&nkn_tfm_pht_mutex);
            } else {
		pthread_mutex_lock(&nkn_tfm_cht_mutex);
                pobj = (TFM_obj *)nkn_tfm_cod_tbl_get(nkn_tfm_completed_uri_hash_table,
					          tfm_pr_comp_data->cod);
                if(pobj == NULL) {
                    glob_tfm_num_promotes_in_progress --;
	            /* This case should never be hit */
	            glob_tfm_promote_obj_not_found_after_complete ++;
                    pthread_mutex_unlock(&nkn_tfm_cht_mutex);
                    nkn_cod_close(tfm_pr_comp_data->cod, NKN_COD_TFM_MGR);
                    free(tfm_pr_comp_data);
	            continue;
                }
		ret_val = tfm_cod_test_and_get_uri(pobj->cod, &uri);
		if(ret_val < 0) {
		    assert(0);
		    glob_tfm_promote_complete_cod_invalid ++;
                    glob_tfm_num_promotes_in_progress --;
	            /* This case should never be hit */
	            glob_tfm_promote_obj_not_found_after_complete ++;
                    pthread_mutex_unlock(&nkn_tfm_cht_mutex);
                    nkn_cod_close(tfm_pr_comp_data->cod, NKN_COD_TFM_MGR);
                    free(tfm_pr_comp_data);
	            continue;
		}
                pobj->is_promoted = 1;
#if 0
                s_tfm_write_promote_complete_file(uri);
#endif
                pobj->defer_delete = 0;
		filename = NULL;
		if(!pobj->is_buf_only)
		    filename = nkn_strdup_type(pobj->filename, mod_tfm_strdup8);
                glob_tfm_spc_cht_bytes_used -= pobj->in_bytes;
		file_id = pobj->file_id;
		nkn_tfm_cod_tbl_delete(nkn_tfm_completed_uri_hash_table,
				       pobj->cod);
                glob_tfm_spc_cht_num_files --;
                glob_tfm_cht_delete ++;
                glob_tfm_num_promotes_in_progress --;
                pthread_mutex_unlock(&nkn_tfm_cht_mutex);

                if(tfm_pr_comp_data->out_ret_code == 0) {
		    glob_tfm_uris_promote_complete ++;
                } else {
		    glob_tfm_uris_promote_complete_err ++;
                }
                if(filename) {
		    s_tfm_unlink(filename);
		    free(filename);
                }
		s_tfm_free_file_id(file_id);
            }
            nkn_cod_close(tfm_pr_comp_data->cod, NKN_COD_TFM_MGR);
            free(tfm_pr_comp_data);
        }else{
	    pthread_cond_wait(&tfm_promote_comp_thr_cond, &tfm_promote_comp_mutex);
	    pthread_mutex_unlock(&tfm_promote_comp_mutex);
        }
    }
}

void tfm_completed_print(void);
void tfm_completed_print(void)
{
    tfm_cod_db_print(nkn_tfm_completed_uri_hash_table);
}

void tfm_partial_print(void);
void tfm_partial_print(void)
{
    tfm_cod_db_print(nkn_tfm_partial_uri_hash_table);
}

static int64_t
s_get_content_len_from_attr(const nkn_attr_t *attr)
{
    if(!attr) {
        DBG_LOG(WARNING, MOD_TFM, "\nAttributes not set "
                "correctly");
        return -1;
    }
    return attr->content_length;
}

static time_t
s_get_last_modified_from_attr(const nkn_attr_t *attr)
{
    int                         ret = -1;
    time_t                      out_attr_lm;
    mime_header_t		hdr;
    const char 			*data;
    int				datalen;
    uint32_t			hattr;
    int				hdrcnt;

    if(!attr) {
        DBG_LOG(WARNING, MOD_TFM, "\nAttributes not set "
                "correctly");
        return -1;
    }

    ret = nkn_attr2http_header(attr, 0, &hdr);
    if (!ret) {
    	init_http_header(&hdr, 0, 0);
    	ret = get_known_header(&hdr, MIME_HDR_LAST_MODIFIED, &data, &datalen, 
			       &hattr, &hdrcnt);
	if (!ret) {
            out_attr_lm = parse_date((char *)data,
				     (char *)data + (datalen - 1));
	    shutdown_http_header(&hdr);
            return out_attr_lm;
	    
	}
	shutdown_http_header(&hdr);
    }
    return 0;
}


