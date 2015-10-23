/*
  COPYRIGHT: NOKEENA NETWORKS
  AUTHOR: VIKRAM VENKATARAGHAVAN
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
#include <sys/stat.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <glib.h>
#include <sys/prctl.h>
#include <pthread.h>
#include <sys/param.h>
#include "nkn_sched_api.h"
#include "nkn_mediamgr_api.h"
#include "nkn_mediamgr.h"
#include "nkn_diskmgr2_api.h"
#include "cache_mgr.h"
#include "nkn_defs.h"
#include "nkn_attr.h"
#include "nkn_stat.h"
#include "nkn_cfg_params.h"
#include "nkn_tfm_api.h"
#include "nkn_nfs_api.h"
#include "nkn_am_api.h"
#include "nkn_debug.h"
#include "nkn_errno.h"
#include "nkn_namespace.h"
#include "nkn_assert.h"
#include "nkn_cod.h"
#include "atomic_ops.h"
#include "nkn_pseudo_con.h"
#include "om_defs.h"
#include "nkn_mm_mem_pool.h"
#include "nkn_locmgr2_extent.h"
#include "nkn_cim.h"

char mm_cache_provider_names[21][64] = {
    {   "unknown" },
    {	"ssd"     },
    {	"flash"   },
    {	"buffer"  },
    {	"null"    },
    {	"sas"      },
    {	"sata"     },
    {	"null"     },
    {	"null"     },
    {	"null"     },
    {	"peer"     },
    {	"tfm"     },
    {	"null"     },
    {	"null"     },
    {	"null"     },
    {	"null"     },
    {	"null"     },
    {	"null"     },
    {	"null"     },
    {	"nfs"     },
    {	"origin"     },
    };

GThread *mm_alert_async_thread;
GAsyncQueue *mm_alert_async_q;
pthread_mutex_t mm_promote_jobs_mutex;
nkn_mm_malloc_t *nkn_mm_promote_mem_objp;
uint32_t nkn_mm_block_size[NKN_MM_MAX_CACHE_PROVIDERS];
uint32_t nkn_mm_small_write_size[NKN_MM_MAX_CACHE_PROVIDERS];
uint64_t mm_triggertime_hist[30];
// t0 to 240 seconds in 30 seconds interval
uint64_t mm_triggertime_histwide[8];
uint64_t mm_block_triggertime_hist[30];
// 60 to 360 seconds in 30 seconds interval
uint64_t mm_block_triggertime_histwide[12];

uint64_t glob_maxage_header2attr_failed;
uint64_t glob_mm_highest_block_size;

uint64_t glob_mm_validate_namespace_error = 0;
uint32_t glob_mm_max_local_providers;
uint32_t glob_mm_max_get_threads;
uint32_t nkn_mm_local_provider_get_start_tid[NKN_MM_MAX_CACHE_PROVIDERS];
uint32_t nkn_mm_local_provider_get_end_tid[NKN_MM_MAX_CACHE_PROVIDERS];
AO_t nkn_mm_local_provider_get_tid[NKN_MM_MAX_CACHE_PROVIDERS];
uint32_t glob_mm_max_put_threads;
uint32_t nkn_mm_local_provider_put_start_tid[NKN_MM_MAX_CACHE_PROVIDERS];
uint32_t nkn_mm_local_provider_put_end_tid[NKN_MM_MAX_CACHE_PROVIDERS];
AO_t nkn_mm_local_provider_put_tid[NKN_MM_MAX_CACHE_PROVIDERS];
uint32_t mm_alert_async_q_count;
uint32_t mm_promote_job_q_count;
uint64_t glob_mm_max_outstanding_get_err = 0;
uint64_t glob_mm_get_run_queue_stopped = 0;
uint64_t glob_mm_max_outstanding_put_err = 0;
uint64_t glob_mm_put_run_queue_stopped = 0;
uint64_t glob_mm_max_outstanding_validate_err = 0;
uint64_t glob_mm_promote_attr_alloc_err = 0;
uint64_t glob_mm_promote_attr_alloc_err1 = 0;
uint64_t glob_mm_promote_buf_alloc_err = 0;
uint64_t glob_mm_promote_success = 0;
uint64_t glob_mm_promote_buf_allocs = 0;
uint64_t glob_mm_promote_buf_holds = 0;
uint64_t glob_mm_promote_buf_releases = 0;
uint64_t glob_mm_promote_errors = 0;
uint64_t glob_mm_promote_put_errors = 0;
uint64_t glob_mm_promote_get_errors = 0;
uint64_t glob_mm_ingest_check_directory_depth_cnt = 0;
uint32_t glob_mm_largest_provider_attr_size = 0;
uint64_t glob_mm_promote_taskcreation_failure = 0;
uint64_t glob_mm_attr_buf_alloc_failed = 0;
uint64_t glob_mm_partial_overwrite_avoided = 0;
uint64_t glob_mm_put_max_interval;

extern uint64_t glob_am_ingest_check_directory_depth_cnt;
extern uint64_t glob_am_ingest_check_object_size_hit_cnt;
extern uint64_t glob_am_ingest_check_object_expiry_cnt;
extern uint64_t glob_am_avoided_large_attr_size;
extern uint64_t glob_am_origin_entry_ageout_cnt;
extern uint64_t glob_am_origin_entry_error_ageout_cnt;


AO_t glob_mm_promote_complete = 0;
AO_t glob_mm_promote_started = 0;

AO_t nkn_mm_tier_promote_started[NKN_MM_MAX_CACHE_PROVIDERS];
AO_t nkn_mm_tier_promote_complete[NKN_MM_MAX_CACHE_PROVIDERS];

AO_t glob_mm_ingest_wait = 0;
AO_t glob_mm_ingest_resume = 0;

AO_t glob_mm_hp_promote_started = 0;
AO_t glob_mm_hp_promote_complete = 0;
AO_t glob_mm_hp_max_promotes = 10;

uint64_t glob_mm_promote_too_many_promotes = 0;
uint64_t glob_mm_promote_space_unavailable = 0;
uint64_t glob_mm_promote_uri_err = 0;
uint64_t glob_mm_promote_unaligned_mem_cnt = 0;
uint64_t glob_mm_promote_align_complete= 0;
uint64_t glob_mm_promote_get_more_data_unaligned= 0;
uint64_t glob_mm_promote_get_more_data_aligned = 0;
uint64_t glob_mm_promote_err_delete_called = 0;
uint64_t glob_mm_promote_obj_nearly_expired_err = 0;
uint64_t glob_mm_promote_puts = 0;
uint64_t glob_mm_update_attr = 0;
uint64_t glob_mm_am_alerts = 0;
uint64_t glob_mm_stat_success = 0;
uint64_t glob_mm_stat_err = 0;
uint64_t glob_mm_stat_cod_err = 0;
uint64_t glob_mm_get_cnt = 0;
uint64_t glob_mm_get_success = 0;
uint64_t glob_mm_get_err = 0;
uint64_t glob_mm_get_retry_cnt = 0;
uint64_t glob_mm_get_cod_err = 0;
uint64_t glob_mm_get_no_calling_func_err = 0;
uint64_t glob_mm_put_cnt = 0;
uint64_t glob_mm_put_success = 0;
uint64_t glob_mm_put_err = 0;
uint64_t glob_mm_put_cod_err = 0;
uint64_t glob_mm_validate_cod_err = 0;
//static uint64_t s_mm_am_new_policy = 0;
uint64_t glob_mm_demotes = 0;
uint64_t glob_mm_alert_calls = 0;
uint64_t glob_mm_task_time_too_long = 0;
uint64_t glob_mm_get_task_time_too_long = 0;
uint64_t glob_mm_put_task_time_too_long = 0;
uint64_t glob_mm_val_task_time_too_long = 0;
uint64_t glob_mm_update_attr_called = 0;
uint64_t glob_mm_promote_get_num_iov_zero = 0;
uint64_t glob_mm_promote_read_cptr_err = 0;
uint64_t glob_mm_promote_pobj_err = 0;
uint64_t glob_mm_promote_pobj_cod_err = 0;
uint64_t glob_mm_promote_pobj_cod_err1 = 0;
uint64_t glob_mm_promote_pobj_write_err = 0;
uint64_t glob_mm_attr_alloc_err = 0;
AO_t glob_mm_promote_jobs_created = 0;
AO_t glob_mm_stat_queue_cnt = 0;
AO_t glob_mm_stat_cnt = 0;
uint64_t glob_cim_mm_cod_mismatch = 0;

int glob_mm_num_stat_threads = 100;
int mm_max_promote_jobs = 100;
uint64_t glob_mm_stats = 0;
uint64_t glob_mm_gets = 0;
uint64_t glob_mm_validates = 0;
uint64_t glob_mm_update_attrs = 0;
uint64_t glob_mm_deletes = 0;
uint64_t glob_mm_puts = 0;
uint64_t glob_mm_2mb_block_promotes = 0;
uint64_t glob_mm_promote_partial_file_write_called = 0;
uint64_t glob_mm_object_cache_worthy_failed = 0;
uint64_t glob_mm_partial_expired_delete_cnt = 0;
uint64_t glob_mm_cod_null_err = 0;
uint64_t glob_mm_server_busy_err;	//counter for error code NKN_SERVER_BUSY  10003
uint64_t glob_mm_def_stack_sz = NKN_MM_DEFAULT_STACK_SZ;
uint64_t glob_mm_num_eod_on_close = 0;
uint64_t glob_mm_client_driven_retry_cnt = 0;
uint64_t glob_mm_partial_obj_ingest_failed =0;
uint64_t glob_mm_cli_driv_to_aggr_streaming_cnt =0;
uint64_t glob_mm_cli_driv_to_aggr_hotness_thrsh_cnt =0;

uint32_t glob_mm_common_promotes = NKN_MAX_PROMOTES;
uint32_t nkn_mm_tier_promotes[NKN_MM_MAX_CACHE_PROVIDERS];
uint64_t glob_mm_ingest_ptype_changed = 0;
uint64_t glob_mm_invalid_delete_cnt = 0;
uint64_t glob_mm_new_object_put = 0;
uint64_t glob_mm_old_object_put_avoided = 0;

/* Tier specific SAC counters */
AO_t nkn_mm_tier_sac_reject_cnt[NKN_MM_MAX_CACHE_PROVIDERS];

/* Tier specific ingest counters */
AO_t nkn_mm_tier_ingest_promote_cnt[NKN_MM_MAX_CACHE_PROVIDERS];
AO_t nkn_mm_tier_put_cnt[NKN_MM_MAX_CACHE_PROVIDERS];
AO_t nkn_mm_tier_put_queue_cnt[NKN_MM_MAX_CACHE_PROVIDERS];
AO_t nkn_mm_tier_ingest_failed[NKN_MM_MAX_CACHE_PROVIDERS];
AO_t nkn_mm_tier_ingest_dm2_put_failed[NKN_MM_MAX_CACHE_PROVIDERS];
AO_t nkn_mm_tier_ingest_not_cache_worthy[NKN_MM_MAX_CACHE_PROVIDERS];
AO_t nkn_mm_tier_ingest_none_buf_not_avail[NKN_MM_MAX_CACHE_PROVIDERS];
AO_t nkn_mm_tier_ingest_none_clientbuf_not_avail[NKN_MM_MAX_CACHE_PROVIDERS];
AO_t nkn_mm_tier_ingest_partial_buf_not_avail[NKN_MM_MAX_CACHE_PROVIDERS];
AO_t nkn_mm_tier_ingest_partial_clientbuf_not_avail[NKN_MM_MAX_CACHE_PROVIDERS];
AO_t nkn_mm_tier_ingest_genbuf_not_avail[NKN_MM_MAX_CACHE_PROVIDERS];
AO_t nkn_mm_tier_ingest_ver_expired[NKN_MM_MAX_CACHE_PROVIDERS];
AO_t nkn_mm_tier_ingest_dm2_attr_large[NKN_MM_MAX_CACHE_PROVIDERS];
AO_t nkn_mm_pull_ingest_obj_with_hole[NKN_MM_MAX_CACHE_PROVIDERS];
AO_t nkn_mm_tier_ingest_unknown_error[NKN_MM_MAX_CACHE_PROVIDERS];
AO_t nkn_mm_tier_ingest_unknown_error_val[NKN_MM_MAX_CACHE_PROVIDERS];
AO_t nkn_mm_tier_ingest_nocache_expired[NKN_MM_MAX_CACHE_PROVIDERS];
AO_t nkn_mm_tier_ingest_server_fetch[NKN_MM_MAX_CACHE_PROVIDERS];
AO_t nkn_mm_tier_ingest_dm2_baduri[NKN_MM_MAX_CACHE_PROVIDERS];
AO_t nkn_mm_tier_ingest_dm2_preread_busy[NKN_MM_MAX_CACHE_PROVIDERS];

int small_obj_hit[NKN_MM_MAX_CACHE_PROVIDERS] = {
	0, /* Unknown_provider */
	6, /* SolidStateMgr_provider */
	0, /* FlashMgr_provider */
	0, /* BufferCache_provider */
	2, /* SASDiskMgr_provider */
	2, /* SAS2DiskMgr_provider */
	1, /* SATADiskMgr_provider */
	0, /* NKN_MM_max_pci_providers */
};

int tier_scale[NKN_MM_MAX_CACHE_PROVIDERS] = {
	0, /* Unknown_provider */
	2, /* SolidStateMgr_provider */
	0, /* FlashMgr_provider */
	0, /* BufferCache_provider */
	1, /* SASDiskMgr_provider */
	1, /* SAS2DiskMgr_provider */
	1, /* SATADiskMgr_provider */
	0, /* NKN_MM_max_pci_providers */
};
int tier_order[NKN_MM_MAX_CACHE_PROVIDERS + 1];
int num_tiers = 0;

void media_mgr_cleanup(nkn_task_id_t id);
void media_mgr_input(nkn_task_id_t id);
void media_mgr_output(nkn_task_id_t id);
static int s_mm_check_task_time(nkn_task_id_t id);
int nkn_am_ingest_small_obj_enable;
uint32_t nkn_am_ingest_small_obj_size;
static void
s_mm_promote_complete(nkn_cod_t cod, char *uri, nkn_provider_type_t src_ptype,
		      nkn_provider_type_t dst_ptype, int delete,
		      int partial_file_write, void *in_proto_data,
		      int streaming_put, size_t size, int err_code, int flags,
		      int64_t mmoffset, time_t obj_starttime,
		      time_t block_starttime, time_t clientupdatetime,
		      time_t expiry);

AO_t mm_glob_bytes_used_in_put[NKN_MM_MAX_CACHE_PROVIDERS];
int mm_glob_max_bytes_per_tier[NKN_MM_MAX_CACHE_PROVIDERS];

// Configurable: add to configuration
int             mm_prov_hi_threshold = 90;
int             mm_prov_lo_threshold = 85;
//int             mm_evict_thread_time_sec = 30;
int             mm_evict_thread_time_sec = 3;
GThread         *mm_evict_thread;
uint64_t        glob_mm_evict_timeout_cnt = 0;
GList           *mm_provider_list = NULL;

/* Push Ingest */

AO_t glob_mm_push_ingest_cnt;
uint64_t glob_mm_push_ingest_tmout_entry_info_removed;
uint64_t glob_mm_push_ingest_put_error;
uint64_t glob_mm_push_ingest_complete;
uint64_t glob_mm_push_ingest_partial_complete;
uint64_t glob_mm_push_ingest_cod_mismatch;
uint64_t glob_mm_push_ingest_ns_cache_worthy_failure;

/* This should be calculated based on the Buffer availability and min write size
 */
uint32_t glob_mm_push_ingest_max_cnt;
uint32_t glob_mm_push_ingest_dyn_cnt;
uint32_t glob_mm_push_ingest_limit;
int glob_mm_push_ingest_parallel_ingest_restrict = 1;

uint64_t glob_mm_push_ingest_max_bufs;


typedef struct mm_ext_info_st {
#define MM_PUSH_WRITE_READY        0x0001
#define MM_PUSH_WRITE_IN_PROG      0x0002
#define MM_PUSH_WRITE_COMP         0x0004
    int           flags;
    off_t         offset;
    uint32_t      size;
    int           write_num_bufs;
    int           write_ret_val;
    am_ext_info_t ext_info;
    time_t        last_update_time;
    nkn_uol_t     uol[64];
    nkn_iovec_t   content[64];
    TAILQ_ENTRY(mm_ext_info_st) entries;
}mm_ext_info_t;

typedef struct mm_push_ingest_st {
    int             src_provider;
    int             dst_provider;
#define MM_PUSH_AM_ADDED           0x0001
#define MM_PUSH_IN_QUEUE           0x0002
#define MM_PUSH_ENTRY_DELETE       0x0004
#define MM_PUSH_TMOUT_QUEUE_ADDED  0x0008
#define MM_PUSH_ENTRY_TO_DELETE    0x0010
#define MM_PUSH_INGEST_PUT_ERROR   0x0020
#define MM_PUSH_INGEST_TRIGGERED   0x0040
#define MM_PUSH_FIRST_INGEST       0x0080
#define MM_PUSH_PIN_OBJECT         0x0100
#define MM_PUSH_IGNORE_EXPIRY      0x0200
#define MM_PUSH_FIN_CHECK          0x0400
#define MM_PUSH_WRITE_DONE         0x0800
    int             flags;
    nkn_cod_t       cod;
    nkn_buffer_t    *attr_buf;
    nkn_attr_t      *attr;
    time_t          last_chk_time;
    size_t          in_disk_size;
    namespace_token_t ns_token;
    pthread_mutex_t mm_ext_entry_mutex;
    pthread_mutex_t mm_wrt_entry_mutex;
    pthread_mutex_t mm_entry_mutex;
    TAILQ_HEAD(mm_ext_info_queue_st, mm_ext_info_st)
		    mm_ext_info_q;
    TAILQ_HEAD(mm_wrt_info_queue_st, mm_ext_info_st)
		    mm_wrt_info_q;
    TAILQ_ENTRY(mm_push_ingest_st) entries;
    TAILQ_ENTRY(mm_push_ingest_st) tmout_entries;
}mm_push_ingest_t;

TAILQ_HEAD(nkn_mm_push_ingest_queue, mm_push_ingest_st)
    nkn_mm_push_ingest_q;
TAILQ_HEAD(nkn_mm_push_timeout_queue, mm_push_ingest_st)
    mm_push_ingest_tmout_q;
static pthread_cond_t  mm_push_ingest_q_cond;
static pthread_t       mm_push_ingest_thread;
static pthread_mutex_t mm_push_ingest_com_mutex;


typedef struct mm_move_mgr_struct {
    int                 src_provider;
    int                 dst_provider;
    nkn_uol_t           uol;
    nkn_task_id_t       taskid;
    uint64_t            tot_uri_len;
    uint64_t            rem_uri_len;
    op_t                move_op;
    nkn_task_id_t       get_task_id;
    cache_response_t    *cptr;
    uint64_t            buflen;
    uint8_t		*buf;
    uint32_t		bufcnt;
    uint8_t             partial_file_write;
    uint8_t             partial;
    uint64_t            partial_offset;
    nkn_attr_t          *attr;
    uint64_t            bytes_written;
    int                 retry_count;
    int                 delay;
    void		*in_proto_data;
    int			streaming;
    int			streaming_put;
    int                 flags;
    am_object_data_t    in_client_data;
    off_t		client_offset;
    time_t		block_trigger_time;
    time_t		obj_trigger_time;
    time_t		client_update_time;
    void		*abuf;
} mm_move_mgr_t;

typedef struct mm_provider {
    int (*put)(struct MM_put_data *, uint32_t);
    int (*stat)(nkn_uol_t, MM_stat_resp_t *);
    int (*get) (MM_get_resp_t *);
    int (*delete) (MM_delete_resp_t *);
    int (*shutdown) (void);
    int (*discover) (struct mm_provider_priv *);
    int (*evict) (void);
    int (*provider_stat) (MM_provider_stat_t *);
    int (*validate) (MM_validate_t *);
    int (*update_attr) (MM_update_attr_t *);
    void (*promote_complete)(MM_promote_data_t *);
    int num_get_threads;
    int num_put_threads;
    uint64_t iobw;
    nkn_provider_type_t ptype;
    GList *sub_prov_list;
    uint32_t get_io_q_depth;
    uint32_t put_io_q_depth;
    int flags;
    int	state;
    int get_run_queue;
    int put_run_queue;
    uint32_t num_disks;
} mm_provider_t;

static void s_mm_free_move_mgr_obj(mm_move_mgr_t *pobj);
struct mm_provider mm_provider_array[NKN_MM_MAX_CACHE_PROVIDERS] ;

/* Move manager thread handlers */
uint64_t               glob_mm_move_mgr_q_num_obj = 0;
static pthread_cond_t  mm_move_mgr_thr_cond;
static pthread_mutex_t mm_move_mgr_thr_mutex;
static pthread_t       mm_move_mgr_thread;

void *s_mm_move_mgr_thread_hdl(void *dummy);
void *s_mm_push_ingest_thread(void *dummy);
STAILQ_HEAD(mm_move_mgr_q_t, mm_move_mgr_thr_s) mm_move_mgr_q;

unsigned int nkn_sac_per_thread_limit[NKN_MM_MAX_CACHE_PROVIDERS] = {
						0,
						NKN_MM_SAC_SSD_DEF_THRESHOLD,
						0,
						0,
						NKN_MM_SAC_SAS_DEF_THRESHOLD,
						NKN_MM_SAC_SAS_DEF_THRESHOLD,
						NKN_MM_SAC_SATA_DEF_THRESHOLD,
						0,
						0 };

AO_t nkn_mm_get_queue_cnt[NKN_MM_MAX_CACHE_PROVIDERS];
AO_t nkn_mm_get_cnt[NKN_MM_MAX_CACHE_PROVIDERS];

typedef struct mm_move_mgr_thr_s {
    mm_move_mgr_t     *pobj;
    nkn_task_id_t     taskid;
    cache_response_t  *cptr;
    MM_put_data_t     *pput;
    STAILQ_ENTRY(mm_move_mgr_thr_s) entries;
} mm_move_mgr_thr_t;
static void s_mm_exec_move_mgr_output(mm_move_mgr_thr_t *mm);

static cache_response_t *cr_init(void) {
    cache_response_t *cr = nkn_calloc_type(1, sizeof(*cr), mod_mm_cache_response_t);
    return cr;
}

static void cr_free( cache_response_t *cr) {
    if (cr) {
        free(cr);
    }
}

static void
s_mm_evict_provider_cache(void)
{
    GList                       *plist = mm_provider_list;
    mm_provider_t               *mp;

    while(plist) {
        mp = (mm_provider_t *) plist->data;
        if(!mp) {
            continue;
        }
        if(mp->evict != NULL) {
            mp->evict();
        }
        plist = plist->next;
    }
}

static void
s_mm_evict_thread_hdl(void)
{

    prctl(PR_SET_NAME, "nvsd-mm-evict", 0, 0, 0);
    while(1) {
        sleep(mm_evict_thread_time_sec);
        glob_mm_evict_timeout_cnt ++;
        s_mm_evict_provider_cache();
    }
}

int
MM_init(void)
{
    nkn_provider_type_t         ptype;
    int                         ret = -1, i;
    GError                      *err1 = NULL ;
    char                        message[64];
    int                         ret_val;
    char			counter_str[512];
    pthread_condattr_t          a;

    for(ptype = 0; ptype < NKN_MM_MAX_CACHE_PROVIDERS; ptype++) {
        mm_provider_array[ptype].put = NULL;
        mm_provider_array[ptype].stat = NULL;
        mm_provider_array[ptype].get = NULL;
        mm_provider_array[ptype].delete = NULL;
        mm_provider_array[ptype].shutdown = NULL;
        mm_provider_array[ptype].discover = NULL;
        mm_provider_array[ptype].promote_complete = NULL;
        mm_provider_array[ptype].validate = NULL;
        mm_provider_array[ptype].update_attr = NULL;
        mm_provider_array[ptype].provider_stat = NULL;
        mm_provider_array[ptype].get_io_q_depth = 0;
        mm_provider_array[ptype].flags = 0;
	mm_provider_array[ptype].get_run_queue = 0;
	AO_store(&mm_glob_bytes_used_in_put[ptype], 0);
    }

    ret = NFS_init();
    if(ret < 0) {
        DBG_LOG(WARNING, MOD_MM, "\n NFS init FAILED");
    }

    mm_evict_thread = g_thread_create_full(
    			(GThreadFunc) s_mm_evict_thread_hdl,
			(void *)message, (128*1024),
			TRUE, FALSE, G_THREAD_PRIORITY_NORMAL, &err1);
    if(mm_evict_thread == NULL) {
        g_error_free ( err1 ) ;
    }

    /* Dont use this cond in a pthread_cond_timedwait
     * Initialize the cond variable with CLOCK_MONOTNOIC
     * before using it.
     */
    pthread_cond_init(&mm_move_mgr_thr_cond, NULL);

    ret_val = pthread_mutex_init(&mm_move_mgr_thr_mutex, NULL);
    if(ret_val < 0) {
        DBG_LOG(SEVERE, MOD_MM, "MM Promote Mutex not created. "
		"Severe MM failure");
        DBG_ERR(SEVERE, "MM Promote Mutex not created. "
		"Severe MM failure");
        return -1;
    }
    /* Allocate 20% more of the memory required for the promote 
     * just in case
     */
    nkn_mm_promote_mem_objp = nkn_mm_mem_pool_init(NKN_MAX_PROMOTES +
				    ((NKN_MAX_PROMOTES * 20)/100),
				    NKN_MAX_BLOCK_SZ,
				    mod_mm_posix_memalign);

    STAILQ_INIT(&mm_move_mgr_q);

    if ((ret = pthread_create(&mm_move_mgr_thread, NULL,
			      s_mm_move_mgr_thread_hdl, NULL))) {
        DBG_LOG(SEVERE, MOD_MM,"MM move mgr thread not created. "
		"Severe MM failure");
        DBG_ERR(SEVERE, "MM move mgr thread not created. "
		"Severe MM failure");
        return -1;
    }

    nkn_task_register_task_type(TASK_TYPE_PUSH_INGEST_MANAGER,
                                &mm_push_mgr_input,
                                &mm_push_mgr_output,
                                &mm_push_mgr_cleanup);

    pthread_condattr_init(&a);
    pthread_condattr_setclock(&a, CLOCK_MONOTONIC);

    pthread_cond_init(&mm_push_ingest_q_cond, &a);
    ret_val = pthread_mutex_init(&mm_push_ingest_com_mutex, NULL);
    if(ret_val < 0) {
        DBG_LOG(SEVERE, MOD_MM, "MM Push ingest Q Mutex not created. "
		"Severe MM failure");
        DBG_ERR(SEVERE, "MM Push ingest Q Mutex not created. "
		"Severe MM failure");
        return -1;
    }

    TAILQ_INIT(&nkn_mm_push_ingest_q);
    TAILQ_INIT(&mm_push_ingest_tmout_q);

    glob_mm_push_ingest_max_bufs = (((cm_max_cache_size *
					glob_mm_push_ingest_buffer_ratio)/100) /
					    CM_MEM_PAGE_SIZE);
#if 0
    /* Disable push ingest if buffer available is too low */
    if(glob_mm_push_ingest_max_bufs <
	    ((NKN_MAX_BLOCK_SZ/CM_MEM_PAGE_SIZE) * 10)) {
	glob_am_push_ingest_enabled = 0;
    }
#endif

    if(glob_mm_push_ingest_max_bufs <
	    (NKN_MAX_BLOCK_SZ/CM_MEM_PAGE_SIZE)) {
	glob_mm_push_ingest_max_bufs = (NKN_MAX_BLOCK_SZ/CM_MEM_PAGE_SIZE);
    }

    if(!glob_mm_highest_block_size)
	glob_mm_highest_block_size = NKN_MAX_BLOCK_SZ;

    if(!glob_mm_push_ingest_max_cnt)
	glob_mm_push_ingest_max_cnt     = (glob_mm_push_ingest_max_bufs *
						CM_MEM_PAGE_SIZE)/
						glob_mm_highest_block_size;
    /* To avoid divide by 0 errors */
    if(!glob_mm_push_ingest_max_cnt)
	glob_mm_push_ingest_max_cnt = 1;

    glob_mm_push_ingest_limit   = glob_mm_push_ingest_max_bufs;

    glob_mm_push_ingest_dyn_cnt = glob_mm_push_ingest_max_cnt;

    if ((ret = pthread_create(&mm_push_ingest_thread, NULL,
			      s_mm_push_ingest_thread, NULL))) {
        DBG_LOG(SEVERE, MOD_MM,"MM push ingest thread not created. "
		"Severe MM failure");
        DBG_ERR(SEVERE, "MM push ingest thread not created. "
		"Severe MM failure");
        return -1;
    }

    // counter alias
    snprintf(counter_str, 512, "ingest.fail.temp.pobj_cod_err_cnt");
    (void)nkn_mon_add(counter_str, NULL,
		      (void *)&glob_mm_promote_pobj_cod_err,
		      sizeof(glob_mm_promote_pobj_cod_err));

    snprintf(counter_str, 512, "ingest.fail.temp.pobj_cod_err1_cnt");
    (void)nkn_mon_add(counter_str, NULL,
		      (void *)&glob_mm_promote_pobj_cod_err,
		      sizeof(glob_mm_promote_pobj_cod_err));

    snprintf(counter_str, 512, "ingest.fail.temp.pobj_err_cnt");
    (void)nkn_mon_add(counter_str, NULL,
		      (void *)&glob_mm_promote_pobj_err,
		      sizeof(glob_mm_promote_pobj_err));

    snprintf(counter_str, 512, "ingest.fail.perm.cod_null_err_cnt");
    (void)nkn_mon_add(counter_str, NULL,
		      (void *)&glob_mm_cod_null_err,
		      sizeof(glob_mm_cod_null_err));

    snprintf(counter_str, 512, "ingest.fail.perm.cod_null_err_cnt");
    (void)nkn_mon_add(counter_str, NULL,
		      (void *)&glob_mm_cod_null_err,
		      sizeof(glob_mm_cod_null_err));

    snprintf(counter_str, 512, "ingest.fail.perm.object_min_size_hit_cnt");
    (void)nkn_mon_add(counter_str, NULL,
		      (void *)&glob_am_ingest_check_object_size_hit_cnt,
		      sizeof(glob_am_ingest_check_object_size_hit_cnt));

    snprintf(counter_str, 512, "ingest.fail.perm.directory_depth_cnt");
    (void)nkn_mon_add(counter_str, NULL,
		      (void *)&glob_am_ingest_check_directory_depth_cnt,
		      sizeof(glob_am_ingest_check_directory_depth_cnt));

    snprintf(counter_str, 512, "ingest.fail.perm.object_expiry_cnt");
    (void)nkn_mon_add(counter_str, NULL,
		      (void *)&glob_am_ingest_check_object_expiry_cnt,
		      sizeof(glob_am_ingest_check_object_expiry_cnt));

    snprintf(counter_str, 512, "ingest.fail.perm.large_attr_size_cnt");
    (void)nkn_mon_add(counter_str, NULL,
		      (void *)&glob_am_avoided_large_attr_size,
		      sizeof(glob_am_avoided_large_attr_size));

    snprintf(counter_str, 512, "ingest.fail.perm.origin_entry_ageout_cnt");
    (void)nkn_mon_add(counter_str, NULL,
		      (void *)&glob_am_origin_entry_ageout_cnt,
		      sizeof(glob_am_origin_entry_ageout_cnt));

    snprintf(counter_str, 512, "ingest.fail.temp.origin_entry_ageout_onerror_cnt");
    (void)nkn_mon_add(counter_str, NULL,
		      (void *)&glob_am_origin_entry_error_ageout_cnt,
		      sizeof(glob_am_origin_entry_error_ageout_cnt));

    snprintf(counter_str, 512, "ingest.fail.temp.too_many_promotes_cnt");
    (void)nkn_mon_add(counter_str, NULL,
		      (void *)&glob_mm_promote_too_many_promotes,
		      sizeof(glob_mm_promote_too_many_promotes));

    snprintf(counter_str, 512, "ingest.fail.temp.space_unavailable_cnt");
    (void)nkn_mon_add(counter_str, NULL,
		      (void *)&glob_mm_promote_space_unavailable,
		      sizeof(glob_mm_promote_space_unavailable));

    snprintf(counter_str, 512, "ingest.fail.temp.taskcreation_failure_cnt");
    (void)nkn_mon_add(counter_str, NULL,
		      (void *)&glob_mm_promote_taskcreation_failure,
		      sizeof(glob_mm_promote_taskcreation_failure));

    for ( i = 0; i < 30 ; ++i) {
	snprintf(counter_str, 128, "am_ingest_triggertime_%ds_to_%ds",
			      i * 2, (i * 2) +1);
	(void)nkn_mon_add(counter_str, NULL,
				(void *)&mm_triggertime_hist[i],
				sizeof(mm_triggertime_hist[i]));
    }
    snprintf(counter_str, 128, "am_ingest_triggertime_greathan_239s");
    (void)nkn_mon_add(counter_str, NULL,
			(void *)&mm_triggertime_histwide[0],
			sizeof(mm_triggertime_histwide[0]));

    for ( i = 2 ; i < 8;  ++i) {
	snprintf(counter_str, 128, "am_ingest_triggertime_%ds_to_%ds",
			      i * 30, (i + 1) * 30 - 1);
	(void)nkn_mon_add(counter_str, NULL,
				(void *)&mm_triggertime_histwide[i],
				sizeof(mm_triggertime_histwide[i]));
    }
    for ( i = 0; i < 30 ; ++i) {
	snprintf(counter_str, 128, "am_ingest_trigblocktime_%ds_to_%ds",
			      i * 2, (i * 2) +1);
	(void)nkn_mon_add(counter_str, NULL,
				(void *)&mm_block_triggertime_hist[i],
				sizeof(mm_block_triggertime_hist[i]));
    }
    snprintf(counter_str, 128, "am_ingest_trigblocktime_greathan_359s");
    (void)nkn_mon_add(counter_str, NULL,
			(void *)&mm_block_triggertime_histwide[0],
			sizeof(mm_block_triggertime_histwide[0]));

    for ( i = 2 ; i < 12;  ++i) {
	snprintf(counter_str, 128, "am_ingest_trigblocktime_%ds_to_%ds",
			      i * 30, (i + 1) * 30 - 1);
	(void)nkn_mon_add(counter_str, NULL,
				(void *)&mm_block_triggertime_histwide[i],
				sizeof(mm_block_triggertime_histwide[i]));
    }


    return ret;
}

/* This function orders the pointers in ascending order */
static int
s_compare_provider_bw(gconstpointer p1, gconstpointer p2)
{
    int bw1, bw2;

    struct mm_provider *mp1 = (struct  mm_provider *) p1;
    struct mm_provider *mp2 = (struct  mm_provider *) p2;

    /* Need a provider stat function from the providers
       mm_provider_array[ptype].pstat();
    */

    bw1 = mp1->ptype;
    bw2 = mp2->ptype;

    return (bw1 < bw2 ? +1 : bw1 == bw2 ? 0 : -1);
}

/* This function orders the pointers in ascending order */
static int
s_compare_sub_provider_weight(gconstpointer p1, gconstpointer p2)
{
    int bw1, bw2;

    MM_provider_stat_t *mp1 = (MM_provider_stat_t *) p1;
    MM_provider_stat_t *mp2 = (MM_provider_stat_t *) p2;

    /* Need a provider stat function from the providers
       mm_provider_array[ptype].pstat();
    */

    bw1 = mp1->weight;
    bw2 = mp2->weight;

    return (bw1 < bw2 ? +1 : bw1 == bw2 ? 0 : -1);
}

int
mm_update_write_size(nkn_provider_type_t ptype,
		     int size)
{
    nkn_mm_small_write_size[ptype] = size;

    if(size) {
	if(!glob_mm_push_ingest_max_bufs) {
	    glob_mm_push_ingest_max_bufs = (((cm_max_cache_size *
					glob_mm_push_ingest_buffer_ratio)/100) /
					    CM_MEM_PAGE_SIZE);
#if 0
	    /* Disable push ingest if buffer available is too low */
	    if(glob_mm_push_ingest_max_bufs <
		    ((NKN_MAX_BLOCK_SZ/CM_MEM_PAGE_SIZE) * 10)) {
		glob_am_push_ingest_enabled = 0;
	    }
#endif

	    if(glob_mm_push_ingest_max_bufs <
		    (NKN_MAX_BLOCK_SZ/CM_MEM_PAGE_SIZE)) {
		glob_mm_push_ingest_max_bufs = (NKN_MAX_BLOCK_SZ/CM_MEM_PAGE_SIZE);
	    }
	}

	glob_mm_push_ingest_max_cnt = (glob_mm_push_ingest_max_bufs *
					    CM_MEM_PAGE_SIZE)/size;

	/* To avoid divide by 0 errors */
	if(!glob_mm_push_ingest_max_cnt)
	    glob_mm_push_ingest_max_cnt = 1;
    }

    return 0;
}

int
MM_register_provider(nkn_provider_type_t ptype,
                     int sub_ptype,
                     int (*put)(struct MM_put_data *, uint32_t),
                     int (*mmstat)(nkn_uol_t, MM_stat_resp_t *),
                     int (*get) (MM_get_resp_t *),
                     int (*delete) (MM_delete_resp_t *),
                     int (*mmshutdown) (void),
                     int (*discover) (struct mm_provider_priv *),
                     int (*evict) (void),
                     int (*provider_stat) (MM_provider_stat_t *),
                     int (*validate) (MM_validate_t *),
                     int (*update_attr) (MM_update_attr_t *),
                     void (*promote_complete)(MM_promote_data_t *),
		     int num_put_threads,
		     int num_get_threads,
                     uint32_t get_io_q_depth,
		     uint32_t num_disks,
                     int flags)
{
    MM_provider_stat_t *pps;
    char counter_str[512], provider_str[16];
    uint32_t max_attr_size = 0;
    int tier = 0;
    uint32_t start_tid = 1, start_put_tid = 0;
    int num_threads_per_tier = 0;
    int tier_type, tmp_tier;

    if(num_put_threads < 1) {
	num_put_threads = 1;
    }

    if(num_get_threads < 4) {
	num_get_threads = 4;
    }
    if(ptype == NFSMgr_provider) {
        num_get_threads = 1;
        num_put_threads = 1;
    }

    assert((ptype > 0) && (ptype < NKN_MM_MAX_CACHE_PROVIDERS));

    mm_provider_array[ptype].put                = put;
    mm_provider_array[ptype].stat               = mmstat;
    mm_provider_array[ptype].get                = get;
    mm_provider_array[ptype].delete             = delete;
    mm_provider_array[ptype].shutdown           = mmshutdown;
    mm_provider_array[ptype].discover           = discover;
    mm_provider_array[ptype].evict              = evict;
    mm_provider_array[ptype].provider_stat      = provider_stat;
    mm_provider_array[ptype].ptype              = ptype;
    mm_provider_array[ptype].validate           = validate;
    mm_provider_array[ptype].update_attr        = update_attr;
    mm_provider_array[ptype].get_io_q_depth     = (get_io_q_depth * 8)/10;
    mm_provider_array[ptype].put_io_q_depth     = (get_io_q_depth * 2)/10;
    mm_provider_array[ptype].flags              = flags;
    mm_provider_array[ptype].get_run_queue      = 1;
    mm_provider_array[ptype].put_run_queue      = 1;
    mm_provider_array[ptype].promote_complete   = promote_complete;
    mm_provider_array[ptype].num_get_threads    = num_get_threads * tier_scale[ptype];
    mm_provider_array[ptype].num_put_threads    = num_put_threads * tier_scale[ptype];
    /* By default, mark that the provider is active. The provider shall
     * explicitly mark itself INACTIVE if needed */
    mm_provider_array[ptype].state		= MM_PROVIDER_STATE_ACTIVE;

    mm_glob_max_bytes_per_tier[ptype]           = 2 * NKN_MAX_BLOCK_SZ * num_put_threads;

    mm_provider_list = g_list_insert_sorted(mm_provider_list,
                                            &mm_provider_array[ptype],
                                            s_compare_provider_bw);

    if(glob_am_small_obj_hit < small_obj_hit[ptype])
	glob_am_small_obj_hit = small_obj_hit[ptype];

    /* All cache providers should have an entry here */
    if(ptype < NKN_MM_max_pci_providers) {
	glob_mm_max_get_threads += num_get_threads * tier_scale[ptype];
	glob_mm_max_put_threads += num_put_threads * tier_scale[ptype];
	glob_mm_max_local_providers++;
	tier_type = ptype;

	/* Order the tiers based on the number of disks in the tier */
	num_tiers ++;
	for(tier=0; tier<NKN_MM_max_pci_providers; tier++) {
	    if(!tier_order[tier]) {
		tier_order[tier] = tier_type;
		break;
	    } else {
		if(mm_provider_array[tier_type].num_get_threads <
		    mm_provider_array[tier_order[tier]].num_get_threads) {
		    tmp_tier = tier_type;
		    tier_type = tier_order[tier];
		    tier_order[tier] = tmp_tier;
		}
	    }
	}

	/* Recalculate the number of get threads per tier and the start and
	 * end
	 */
	for(tier=0; tier<NKN_MM_max_pci_providers; tier++) {
	    tier_type = tier_order[tier];
	    if(!tier_type)
		break;
	    if(!mm_provider_array[tier_type].get)
		continue;
	    num_threads_per_tier = (mm_provider_array[tier_type].num_get_threads *
				    g_nkn_num_get_threads) /
				    glob_mm_max_get_threads;
	    nkn_mm_local_provider_get_start_tid[tier_type] = start_tid;
	    AO_store(&nkn_mm_local_provider_get_tid[tier_type], start_tid);
	    if(!num_threads_per_tier) {
		num_threads_per_tier = 1;
	    }
	    start_tid += num_threads_per_tier;
	    if(start_tid > g_nkn_num_get_threads)
		start_tid = g_nkn_num_get_threads;
	    nkn_mm_local_provider_get_end_tid[tier_type] = start_tid - 1;

	    if(!mm_provider_array[tier_type].put)
		continue;
	    num_threads_per_tier = (mm_provider_array[tier_type].num_put_threads *
				    g_nkn_num_put_threads) /
				    glob_mm_max_put_threads;
	    nkn_mm_local_provider_put_start_tid[tier_type] = start_put_tid;
	    AO_store(&nkn_mm_local_provider_put_tid[tier_type], start_put_tid);
	    if(!num_threads_per_tier) {
		num_threads_per_tier = 1;
	    }
	    start_put_tid += num_threads_per_tier;
	    if(start_put_tid > g_nkn_num_put_threads)
		start_put_tid = g_nkn_num_put_threads;
	    nkn_mm_local_provider_put_end_tid[tier_type] = start_put_tid;
	}
    }

    switch(ptype) {
	case SATADiskMgr_provider:
	    nkn_mm_tier_promotes[ptype] = NKN_MAX_SATA_TIER_PROMOTES;
	    glob_mm_common_promotes -= NKN_MAX_SATA_TIER_PROMOTES;
	    strcpy(provider_str, "sata");
	    break;
	case SAS2DiskMgr_provider:
	    nkn_mm_tier_promotes[ptype] = NKN_MAX_SAS_TIER_PROMOTES;
	    glob_mm_common_promotes -= NKN_MAX_SAS_TIER_PROMOTES;
	    strcpy(provider_str, "sas");
	    break;
	case SolidStateMgr_provider:
	    nkn_mm_tier_promotes[ptype] = NKN_MAX_SSD_TIER_PROMOTES;
	    glob_mm_common_promotes -= NKN_MAX_SSD_TIER_PROMOTES;
	    strcpy(provider_str, "ssd");
	    break;
	default:
	    provider_str[0] = 0;
	    break;
    }

    if(provider_str[0]) {

	//alias ingestion failure counters
        snprintf(counter_str, 512, "ingest.fail.perm.tier.%s.dm2_large_attrib_cnt",
                        provider_str);
        (void)nkn_mon_add(counter_str, NULL,
                        (void *)&nkn_mm_tier_ingest_dm2_attr_large[ptype],
                        sizeof(nkn_mm_tier_ingest_dm2_attr_large[ptype]));

        snprintf(counter_str, 512, "ingest.fail.temp.tier.%s.push_ingest_object_with_hole",
                        provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_pull_ingest_obj_with_hole[ptype],
			sizeof(nkn_mm_pull_ingest_obj_with_hole[ptype]));

        snprintf(counter_str, 512, "ingest.fail.perm.tier.%s.dm2_put_cnt",
                        provider_str);
        (void)nkn_mon_add(counter_str, NULL,
                        (void *)&nkn_mm_tier_ingest_dm2_put_failed[ptype],
                        sizeof(nkn_mm_tier_ingest_dm2_put_failed[ptype]));

        snprintf(counter_str, 512, "ingest.fail.perm.tier.%s.not_cache_worthy_cnt",
                        provider_str);
        (void)nkn_mon_add(counter_str, NULL,
                        (void *)&nkn_mm_tier_ingest_not_cache_worthy[ptype],
                        sizeof(nkn_mm_tier_ingest_not_cache_worthy[ptype]));

	snprintf(counter_str, 512, "ingest.fail.perm.tier.%s.none_buf_not_avail_cnt",
			provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_tier_ingest_none_buf_not_avail[ptype],
			sizeof(nkn_mm_tier_ingest_none_buf_not_avail[ptype]));

	snprintf(counter_str, 512, "ingest.fail.temp.tier.%s.none_cli_nonreq_buf_not_avail_cnt",
			provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_tier_ingest_none_clientbuf_not_avail[ptype],
			sizeof(nkn_mm_tier_ingest_none_clientbuf_not_avail[ptype]));

	snprintf(counter_str, 512, "ingest.fail.perm.tier.%s.partial_buf_not_avail_cnt",
			provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_tier_ingest_partial_buf_not_avail[ptype],
			sizeof(nkn_mm_tier_ingest_partial_buf_not_avail[ptype]));

	snprintf(counter_str, 512, "ingest.fail.temp.tier.%s.partial_cli_nonreq_buf_not_avail_cnt",
			provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_tier_ingest_partial_clientbuf_not_avail[ptype],
			sizeof(nkn_mm_tier_ingest_partial_clientbuf_not_avail[ptype]));

	snprintf(counter_str, 512, "ingest.fail.perm.tier.%s.genbuf_not_avail_cnt",
			provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_tier_ingest_genbuf_not_avail[ptype],
			sizeof(nkn_mm_tier_ingest_genbuf_not_avail[ptype]));

	snprintf(counter_str, 512, "ingest.fail.perm.tier.%s.version_expired_cnt",
			provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_tier_ingest_ver_expired[ptype],
			sizeof(nkn_mm_tier_ingest_ver_expired[ptype]));

	snprintf(counter_str, 512, "ingest.fail.perm.tier.%s.unknown_error_cnt",
			provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_tier_ingest_unknown_error[ptype],
			sizeof(nkn_mm_tier_ingest_unknown_error[ptype]));

	snprintf(counter_str, 512, "ingest.fail.perm.tier.%s.unknown_error_value",
			provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_tier_ingest_unknown_error_val[ptype],
			sizeof(nkn_mm_tier_ingest_unknown_error_val[ptype]));

	snprintf(counter_str, 512, "ingest.fail.perm.tier.%s.nocache_expired",
			provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_tier_ingest_nocache_expired[ptype],
			sizeof(nkn_mm_tier_ingest_nocache_expired[ptype]));

	snprintf(counter_str, 512, "ingest.fail.perm.tier.%s.server_fetch",
			provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_tier_ingest_server_fetch[ptype],
			sizeof(nkn_mm_tier_ingest_server_fetch[ptype]));

	snprintf(counter_str, 512, "ingest.fail.perm.tier.%s.dm2_baduri",
			provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_tier_ingest_dm2_baduri[ptype],
			sizeof(nkn_mm_tier_ingest_dm2_baduri[ptype]));

	snprintf(counter_str, 512, "ingest.fail.perm.tier.%s.dm2_preread_busy",
			provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_tier_ingest_dm2_preread_busy[ptype],
			sizeof(nkn_mm_tier_ingest_dm2_preread_busy[ptype]));



	// non-aliased counters
	snprintf(counter_str, 512, "glob_mm_%s_get_cnt", provider_str);
	(void)nkn_mon_add(counter_str, NULL,
				(void *)&nkn_mm_get_cnt[ptype],
				sizeof(nkn_mm_get_cnt[ptype]));
	snprintf(counter_str, 512, "glob_mm_%s_get_queue_cnt", provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_get_queue_cnt[ptype],
			sizeof(nkn_mm_get_queue_cnt[ptype]));

	snprintf(counter_str, 512, "glob_mm_%s_sac_limit", provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_sac_per_thread_limit[ptype],
			sizeof(nkn_sac_per_thread_limit[ptype]));

	snprintf(counter_str, 512, "glob_mm_%s_sac_reject_cnt", provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_tier_sac_reject_cnt[ptype],
			sizeof(nkn_mm_tier_sac_reject_cnt[ptype]));

	snprintf(counter_str, 512, "glob_mm_%s_promotes", provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_tier_promotes[ptype],
			sizeof(nkn_mm_tier_promotes[ptype]));

	snprintf(counter_str, 512, "glob_mm_%s_promote_started", provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_tier_promote_started[ptype],
			sizeof(nkn_mm_tier_promote_started[ptype]));

	snprintf(counter_str, 512, "glob_mm_%s_promote_complete", provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_tier_promote_complete[ptype],
			sizeof(nkn_mm_tier_promote_complete[ptype]));

	snprintf(counter_str, 512, "glob_mm_%s_ingest_promote_cnt",
			provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_tier_ingest_promote_cnt[ptype],
			sizeof(nkn_mm_tier_ingest_promote_cnt[ptype]));

	snprintf(counter_str, 512, "glob_mm_%s_put_cnt", provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_tier_put_cnt[ptype],
			sizeof(nkn_mm_tier_put_cnt[ptype]));

	snprintf(counter_str, 512, "glob_mm_%s_put_queue_cnt", provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_tier_put_queue_cnt[ptype],
			sizeof(nkn_mm_tier_put_queue_cnt[ptype]));

	snprintf(counter_str, 512, "glob_mm_%s_ingest_failed", provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_tier_ingest_failed[ptype],
			sizeof(nkn_mm_tier_ingest_failed[ptype]));


	snprintf(counter_str, 512, "glob_mm_%s_ingest_dm2_large_attrib",
			provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_tier_ingest_dm2_attr_large[ptype],
			sizeof(nkn_mm_tier_ingest_dm2_attr_large[ptype]));

	snprintf(counter_str, 512, "glob_mm_%s_pull_ingest_obj_with_hole_avoided",
			provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_pull_ingest_obj_with_hole[ptype],
			sizeof(nkn_mm_pull_ingest_obj_with_hole[ptype]));

	snprintf(counter_str, 512, "glob_mm_%s_ingest_dm2_put_failed",
			provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_tier_ingest_dm2_put_failed[ptype],
			sizeof(nkn_mm_tier_ingest_dm2_put_failed[ptype]));

	snprintf(counter_str, 512, "glob_mm_%s_ingest_not_cache_worthy",
			provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_tier_ingest_not_cache_worthy[ptype],
			sizeof(nkn_mm_tier_ingest_not_cache_worthy[ptype]));

	snprintf(counter_str, 512, "glob_mm_%s_ingest_none_buf_not_avail",
			provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_tier_ingest_none_buf_not_avail[ptype],
			sizeof(nkn_mm_tier_ingest_none_buf_not_avail[ptype]));

	snprintf(counter_str, 512, "glob_mm_%s_ingest_none_client_nonreq_buf_not_avail",
			provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_tier_ingest_none_clientbuf_not_avail[ptype],
			sizeof(nkn_mm_tier_ingest_none_clientbuf_not_avail[ptype]));

	snprintf(counter_str, 512, "glob_mm_%s_ingest_partial_buf_not_avail",
			provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_tier_ingest_partial_buf_not_avail[ptype],
			sizeof(nkn_mm_tier_ingest_partial_buf_not_avail[ptype]));

	snprintf(counter_str, 512, "glob_mm_%s_ingest_partial_client_nonreq_buf_not_avail",
			provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_tier_ingest_partial_clientbuf_not_avail[ptype],
			sizeof(nkn_mm_tier_ingest_partial_clientbuf_not_avail[ptype]));

	snprintf(counter_str, 512, "glob_mm_%s_ingest_genbuf_not_avail",
			provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_tier_ingest_genbuf_not_avail[ptype],
			sizeof(nkn_mm_tier_ingest_genbuf_not_avail[ptype]));
	snprintf(counter_str, 512, "glob_mm_%s_ingest_ver_expired",
			provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_tier_ingest_ver_expired[ptype],
			sizeof(nkn_mm_tier_ingest_ver_expired[ptype]));

	snprintf(counter_str, 512, "glob_mm_%s_ingest_unknown_error",
			provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_tier_ingest_unknown_error[ptype],
			sizeof(nkn_mm_tier_ingest_unknown_error[ptype]));

	snprintf(counter_str, 512, "glob_mm_%s_ingest_unknown_error_val",
			provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_tier_ingest_unknown_error_val[ptype],
			sizeof(nkn_mm_tier_ingest_unknown_error_val[ptype]));

	snprintf(counter_str, 512, "glob_mm_%s_ingest_nocache_expired",
			provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_tier_ingest_nocache_expired[ptype],
			sizeof(nkn_mm_tier_ingest_nocache_expired[ptype]));

	snprintf(counter_str, 512, "glob_mm_%s_ingest_server_fetch",
			provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_tier_ingest_server_fetch[ptype],
			sizeof(nkn_mm_tier_ingest_server_fetch[ptype]));

	snprintf(counter_str, 512, "glob_mm_%s_ingest_dm2_baduri",
			provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_tier_ingest_dm2_baduri[ptype],
			sizeof(nkn_mm_tier_ingest_dm2_baduri[ptype]));

	snprintf(counter_str, 512, "glob_mm_%s_ingest_dm2_preread_busy",
			provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&nkn_mm_tier_ingest_dm2_preread_busy[ptype],
			sizeof(nkn_mm_tier_ingest_dm2_preread_busy[ptype]));

	snprintf(counter_str, 512, "glob_mm_%s_num_get_threads",
			provider_str);
	(void)nkn_mon_add(counter_str, NULL,
			(void *)&mm_provider_array[ptype].num_get_threads,
			sizeof(mm_provider_array[ptype].num_get_threads));

	nkn_am_counter_init(ptype, provider_str);
    }

    /* new sub provider */
    pps = (MM_provider_stat_t *) nkn_malloc_type (sizeof (MM_provider_stat_t),
			mod_mm_provider_stat_t);
    if(!pps) {
        return -ENOMEM;
    }

    memset(pps, 0, sizeof(MM_provider_stat_t));
    pps->ptype = ptype;
    pps->sub_ptype = sub_ptype;
    if(mm_provider_array[ptype].provider_stat != NULL) {
        mm_provider_array[ptype].provider_stat(pps);
	max_attr_size = pps->max_attr_size;
	if(max_attr_size > glob_mm_largest_provider_attr_size) {
	    glob_mm_largest_provider_attr_size = max_attr_size;
	}
    }

    if(pps->block_size)
	nkn_mm_block_size[ptype] = pps->block_size;
    else
	nkn_mm_block_size[ptype] = NKN_MAX_BLOCK_SZ;

    if(nkn_mm_block_size[ptype] > glob_mm_highest_block_size)
	glob_mm_highest_block_size = nkn_mm_block_size[ptype];

    mm_provider_array[ptype].sub_prov_list =
        g_list_insert_sorted(
                             mm_provider_array[ptype].sub_prov_list,
                             pps,
                             s_compare_sub_provider_weight
                             );

    /* Bug 5901: With multiple thread pools the disk performance is half
     * to that seen with a single thread pool. For 2.0.4 revert back to
     * single thread pool  */
#if 0
    if(!num_disks)
	num_disks = NKN_MM_DEF_NUM_DISK;
#else
    num_disks = NKN_MM_DEF_NUM_DISK;
#endif
    mm_provider_array[ptype].num_disks = num_disks;

    return 0;
}

int
MM_set_provider_state(nkn_provider_type_t ptype,
		      int sub_ptype  __attribute((unused)),
		      int state)
{
    if (ptype >= NKN_MM_MAX_CACHE_PROVIDERS) {
	NKN_ASSERT(0);
	return 0;
    }

    if ((state < MM_PROVIDER_STATE_ACTIVE) ||
	(state > MM_PROVIDER_STATE_INACTIVE))
	assert(0);

    mm_provider_array[ptype].state = state;

    return 0;
}

void MM_stop_provider_queue(nkn_provider_type_t ptype,
			    uint8_t sptype __attribute((unused)))
{
    assert((ptype > 0) && (ptype < NKN_MM_MAX_CACHE_PROVIDERS));
    mm_provider_array[ptype].get_run_queue = 0;
}

void MM_run_provider_queue(nkn_provider_type_t ptype,
			   uint8_t sptype __attribute((unused)))
{
    assert((ptype > 0) && (ptype < NKN_MM_MAX_CACHE_PROVIDERS));
    mm_provider_array[ptype].get_run_queue = 1;
}

static int
s_put_data_helper(nkn_cod_t cod, char *uri, uint64_t in_offset,
		  uint64_t in_length,
                  uint64_t tot_uri_len, int src_ptype,
		  int dst_ptype, cache_response_t *cptr,
		  nkn_task_id_t get_task_id, uint64_t rem_uri_len,
		  mm_move_mgr_t *in_pobj,
		  uint8_t partial_file_write,
		  void * in_proto_data, am_object_data_t *in_client_data)
{
    MM_put_data_t       *pput = NULL;
    mm_move_mgr_t       *pobj = NULL;
    int                 ret = 0;
    nkn_task_id_t       taskid = -1;
    int                 count = 5;
    int                 uri_len;
    struct mm_provider  *mp2 = NULL;

    assert(cptr);

    pput = (MM_put_data_t *) nkn_calloc_type (1, sizeof(MM_put_data_t), mod_mm_put_data_t);
    if(pput == NULL) {
        return -1;
    }

    assert(dst_ptype > 0);
    mp2 = &mm_provider_array[dst_ptype];
    assert(mp2);

    if(in_pobj && in_pobj->partial) {
	/* This should not happen 
	 * we just have to return error for now
	 */
	NKN_ASSERT(0);
	free(pput);
	return -1;
    }

    uri_len = strlen(uri);
    pput->uol.offset            = in_offset;
    pput->uol.length            = in_length;
    pput->uol.cod               = cod;
    pput->errcode               = 0;
    pput->iov_data_len          = cptr->num_nkn_iovecs;
    pput->domain                = 0;
    pput->iov_data              = cptr->content;
    if(src_ptype < NKN_MM_max_pci_providers)
	pput->flags |= MM_FLAG_DM2_PROMOTE;

    if(in_pobj && (in_pobj->flags & NKN_MM_IGNORE_EXPIRY))
	pput->flags |= MM_PUT_IGNORE_EXPIRY;

    if(((pput->uol.offset == 0) && in_pobj && in_pobj->attr)) {
	ret = nkn_mm_dm2_attr_copy(in_pobj->attr, &pput->attr,
				   mod_mm_promote_put_attr);
	if (ret != 0) {
	    free(pput);
	    return -1;
	}
    } else {
        pput->attr = NULL;
    }

    pput->total_length  = tot_uri_len;
    pput->ptype         = mp2->ptype;

    pobj = (mm_move_mgr_t *) nkn_calloc_type(1, sizeof(mm_move_mgr_t),
					     mod_mm_move_mgr_t);
    if(pobj == NULL) {
        free(pobj);
        return -1;
    }

    pobj->get_task_id   = get_task_id;
    assert(cptr);
    pobj->cptr              = cptr;
    pobj->uol.offset        = in_offset;
    pobj->uol.length        = in_length;
    pobj->uol.cod           = cod;
    pobj->src_provider      = src_ptype;
    pobj->dst_provider      = mp2->ptype;
    pobj->tot_uri_len       = tot_uri_len;
    pobj->move_op           = MM_OP_WRITE;
    pobj->rem_uri_len       = rem_uri_len;
    if(in_pobj) {
	pobj->buf               = in_pobj->buf;
	pobj->buflen            = in_pobj->buflen;
	pobj->partial           = in_pobj->partial;
	pobj->partial_offset    = in_pobj->partial_offset;
	pobj->bytes_written     = in_pobj->bytes_written;
	pobj->streaming		= in_pobj->streaming;
	pobj->streaming_put     = in_pobj->streaming_put;
	pobj->flags             = in_pobj->flags;
	pobj->client_offset     = in_pobj->client_offset;
	pobj->block_trigger_time = in_pobj->block_trigger_time;
	pobj->obj_trigger_time = in_pobj->obj_trigger_time;
	pobj->client_update_time =  in_pobj->client_update_time;
	if(in_pobj->attr) {
	    ret = nkn_mm_dm2_attr_copy(in_pobj->attr, &pobj->attr,
				       mod_mm_promote_put_attr);
	    if (ret != 0) {
		s_mm_free_move_mgr_obj(pobj);
		return -1;
	    }
	}
    }

    if(in_client_data) {
	memcpy(&(pobj->in_client_data.origin_ipv4v6), &(in_client_data->origin_ipv4v6),
			sizeof(ip_addr_t));
	pobj->in_client_data.origin_port   = in_client_data->origin_port;
	memcpy(&(pobj->in_client_data.client_ipv4v6), &(in_client_data->client_ipv4v6),
			sizeof(ip_addr_t));
	pobj->in_client_data.client_port   = in_client_data->client_port;
	pobj->in_client_data.client_ip_family   = in_client_data->client_ip_family;
	pobj->in_client_data.obj_hotness   = in_client_data->obj_hotness;
	if(in_client_data->proto_data)
	    AM_copy_client_data(src_ptype, in_client_data,
		    &pobj->in_client_data);
	pobj->in_client_data.flags         = in_client_data->flags;
	/* Pass on crawl ingest flag to disk manager */
	if (in_client_data->flags & AM_CIM_INGEST)
	    pput->flags |= MM_FLAG_CIM_INGEST;
    }

    /* Obtain a refcnt from this buffer */
    if(pobj->buf) {
	pobj->bufcnt++;
        glob_mm_promote_buf_holds ++;
    }
    pobj->partial_file_write= partial_file_write;
    pobj->in_proto_data     = in_proto_data;

    if(pobj->flags & NKN_MM_PIN_OBJECT)
	pput->cache_pin_enabled = 1;


    /* retry if scheduler cannot assign a task */
    while((taskid < 0) && (count > 0)) {
        taskid = nkn_task_create_new_task(
                                          TASK_TYPE_MEDIA_MANAGER,
                                          TASK_TYPE_MOVE_MANAGER,
                                          TASK_ACTION_INPUT,
                                          MM_OP_WRITE,
                                          pput,
                                          sizeof(MM_put_data_t),
                                          0 /* deadline */);
        count --;
    }
    if((count == 0) || (taskid < 0)) {
        free(pput);
        free(pobj);
        return -1;
    }

    AO_fetch_and_add(&mm_glob_bytes_used_in_put[pobj->dst_provider], in_length);
    pobj->taskid            = taskid;
    nkn_task_set_private(TASK_TYPE_MOVE_MANAGER, taskid,
                         (void *) pobj);
    nkn_task_set_state(taskid, TASK_STATE_RUNNABLE);

    return ret;
}

/* 
 * Checks the directory depth for the given uri string
 * returns 1 if the directory depth is > then max_depth
 * returns 0 if the directory depth is <= the max_depth
 * if max_depth is 0, return 0
 */
int
nkn_mm_validate_dir_depth(const char *uri, int max_depth)
{
    int num = 0;

    if((max_depth <= 0) || (max_depth > NKN_MM_MAX_DIR_DEPTH)) {
	NKN_ASSERT(0);
	max_depth = NKN_MM_MAX_DIR_DEPTH;
    }

    while (*uri) {
	if (*uri++ == '/')
	    num++;
    }

    /* ignore the / in prefix */
    if ((num - 1) > max_depth)
	return 1;
    else
	return 0;
}


static int
s_get_data_helper(nkn_cod_t cod, char *uri, off_t in_offset,
		  uint64_t in_length,
		  uint64_t rem_uri_len, uint64_t in_total_length,
		  int src_ptype, int dst_ptype, mm_move_mgr_t *in_pobj,
		  uint8_t partial_file_write,
		  void *in_proto_data, am_object_data_t *in_client_data,
		  int flags, time_t update_time)
{
    cache_response_t    *cr = NULL;
    nkn_task_id_t       taskid = -1;
    mm_move_mgr_t       *pobj = NULL;
    int                 count = 5;
    struct mm_provider  *mp1;
    struct mm_provider  *mp2;
    int                 ret = -1;
    const namespace_config_t *cfg = NULL;
    int			force_client_driven = 0;
    off_t		client_offset = 0;
    uint64_t		max_bytes = 0;
    int                 pin_object = 0;
    time_t		min_age = 0;

    if((dst_ptype < 0) || (src_ptype < 0)) {
        return -1;
    }
    mp2 = &mm_provider_array[dst_ptype];
    assert(mp2);

    mp1 = &mm_provider_array[src_ptype];
    assert(mp1);

    cr = cr_init();
    if(cr == NULL) {
        return -1;
    }

    cr->in_proto_data = NULL;
    cr->magic = CACHE_RESP_REQ_MAGIC;
    cr->uol.offset = in_offset;
    cr->uol.length = in_length;
    cr->uol.cod = cod;
    cr->ns_token = uol_to_nstoken(&cr->uol);
    if(!VALID_NS_TOKEN(cr->ns_token)) {
        DBG_LOG(WARNING, MOD_MM_PROMOTE, "Token is 0. Caused by a malformed "
		"offset = %ld, length = %ld",
		cr->uol.offset, cr->uol.length);
        cr_free(cr);
	return -1;
    }

    cfg = namespace_to_config(cr->ns_token);
    if(!cfg) {
        cr_free(cr);
	return -1;
    }

    if(nkn_mm_validate_dir_depth(uri,
			    cfg->http_config->policies.uri_depth_threshold)) {
	DBG_LOG(MSG, MOD_MM_PROMOTE, "[URI=%s] Too many directory levels", uri);
	glob_mm_ingest_check_directory_depth_cnt ++;
	release_namespace_token_t(cr->ns_token);
	cr_free(cr);
	return -1;
    }

    pin_object = cfg->cache_pin_config->cache_auto_pin;
    min_age = cfg->http_config->policies.store_cache_min_age;

    client_offset = nkn_cod_get_client_offset(cod);
    max_bytes = ((((client_offset +
		nkn_mm_block_size[dst_ptype]))/nkn_mm_block_size[dst_ptype]) *
		nkn_mm_block_size[dst_ptype]);

    if(in_client_data && !(in_client_data->flags & AM_CIM_INGEST))
	if(in_pobj && ((uint64_t)in_offset >= max_bytes))
	    force_client_driven = 1;

    if(cfg->http_config->policies.cache_fill == INGEST_CLIENT_DRIVEN) {
	if((!in_length) || (in_length > max_bytes)) {
	    if(max_bytes > nkn_mm_block_size[dst_ptype])
		in_length = nkn_mm_block_size[dst_ptype];
	    else
		in_length = max_bytes;
	    cr->uol.length = in_length;
	}
    } else {
	in_length = (in_pobj && in_pobj->rem_uri_len) ?
			(in_pobj->rem_uri_len -
			    ((in_pobj->buf) ? in_pobj->buflen: 0)) : 0;
	cr->uol.length = in_length;
    }

    /* based on config, do a bm only request */
    if(cfg->http_config->policies.cache_fill == INGEST_CLIENT_DRIVEN) {
	if(in_client_data && !(in_client_data->flags &
		AM_OBJ_TYPE_STREAMING)) {
	    if(in_client_data && (!(in_client_data->flags &
		AM_INGEST_AGGRESSIVE) ||
		((in_client_data->flags & AM_INGEST_AGGRESSIVE) &&
		 force_client_driven)))
	    cr->in_rflags |= CR_RFLAG_BM_ONLY;
	}
    }

    if((cfg->http_config->policies.cache_fill == INGEST_CLIENT_DRIVEN) &&
	(!(in_client_data && (in_client_data->flags & AM_OBJ_TYPE_STREAMING))) &&
	((in_client_data && (in_client_data->flags & AM_INGEST_AGGRESSIVE))))
	in_client_data->flags |= AM_INGEST_BYTE_RANGE;

    if(cfg->http_config->policies.cache_fill == INGEST_CLIENT_DRIVEN) {
	if(in_client_data && (in_client_data->flags & AM_OBJ_TYPE_STREAMING))
	    glob_mm_cli_driv_to_aggr_streaming_cnt ++;
	if(in_client_data && (in_client_data->flags & AM_INGEST_AGGRESSIVE))
	    glob_mm_cli_driv_to_aggr_hotness_thrsh_cnt ++;
    }

    cr->in_rflags |= CR_RFLAG_INTERNAL;		// do not count as hit

    /* If age-threshold is 0, enable revalidation to validate the object
     * before doing a PUT
     */
    if(min_age)
	cr->in_rflags |= CR_RFLAG_NO_REVAL;		// do not revalidate

    if(in_client_data && (in_client_data->flags & AM_CIM_INGEST)) {
	cr->in_rflags &= ~CR_RFLAG_NO_REVAL;
	in_client_data->flags &= ~AM_INGEST_BYTE_RANGE;
	cr->in_client_data.flags |= NKN_CLIENT_CRAWL_REQ;
	if(in_offset)
	    in_client_data->flags |= AM_INGEST_BYTE_SEEK;
    }

    /*! NO_REVAL flag needs to be cleared for chunked offline
        request and reval all is set */
    if ((cfg->http_config->policies.reval_type == 1) &&
	(cfg->http_config->policies.reval_barrier !=
			NS_REVAL_ALWAYS_TIME_BARRIER) &&
	(in_client_data && in_client_data->flags & AM_OBJ_TYPE_STREAMING))
	cr->in_rflags &= ~CR_RFLAG_NO_REVAL;

    if(!in_pobj || in_offset == 0) {
	cr->in_rflags |= CR_RFLAG_GETATTR;
	cr->in_rflags |= CR_RFLAG_FIRST_REQUEST;
	if(in_client_data && (in_client_data->flags & AM_CIM_INGEST)) {
	    cr->req_max_age = 0;
	    cr->in_rflags |= CR_RFLAG_REVAL;
	}
    } else if(in_pobj && in_pobj->streaming && in_pobj->attr) {
	cr->in_rflags |= CR_RFLAG_GETATTR;
    } else {
        cr->in_rflags &= ~(CR_RFLAG_GETATTR);
    }

    if(cfg->http_config->policies.eod_on_origin_close)
	cr->in_rflags |= CR_RFLAG_GETATTR;

    if(in_pobj && in_pobj->attr && !in_pobj->streaming
	    && !cfg->http_config->policies.eod_on_origin_close)
	cr->in_rflags &= ~CR_RFLAG_GETATTR;

    release_namespace_token_t(cr->ns_token);

    /* In case object is streaming, force a separate request to origin */
    if((in_client_data && (in_client_data->flags & AM_OBJ_TYPE_STREAMING))
	    || (in_pobj && in_pobj->streaming))
       cr->in_rflags |= CR_RFLAG_SEP_REQUEST;

    if(in_client_data && (src_ptype > NKN_MM_max_pci_providers))
	nkn_populate_proto_data(&in_proto_data, &cr->in_client_data,
				in_client_data, uri, cr->ns_token, src_ptype);

    cr->in_proto_data = in_proto_data;
    if (in_pobj && in_pobj->attr && in_proto_data) {
	    nkn_populate_attr_data(in_pobj->attr, in_proto_data, src_ptype);
    }

    /* retry if scheduler cannot assign a task */
    while((taskid < 0) && (count > 0)) {
        taskid = nkn_task_create_new_task(
                                          TASK_TYPE_CHUNK_MANAGER,
                                          TASK_TYPE_MOVE_MANAGER,
                                          TASK_ACTION_INPUT,
                                          0,
                                          cr,
                                          sizeof(cache_response_t),
                                          0 /* deadline */);
        count --;
    }

    if((count == 0) || (taskid < 0)) {
        cr_free(cr);
        DBG_LOG(WARNING, MOD_MM_PROMOTE, "Uri: %s moving URI"
                " with MM_put failed. Task not created.",
		uri);
	glob_mm_promote_taskcreation_failure++;
        return -1;
    }

    pobj = (mm_move_mgr_t *) nkn_calloc_type (1, sizeof(mm_move_mgr_t),
					      mod_mm_move_mgr_t);
    if(pobj == NULL) {
        cr_free(cr);
        nkn_task_set_action_and_state(taskid, TASK_ACTION_CLEANUP,
					TASK_STATE_RUNNABLE);
        return -1;
    }
    pobj->uol.offset    = in_offset;
    pobj->uol.length    = in_length;
    pobj->src_provider  = mp1->ptype;
    pobj->dst_provider  = mp2->ptype;
    pobj->taskid        = taskid;
    pobj->tot_uri_len   = in_total_length;
    pobj->rem_uri_len   = rem_uri_len;
    pobj->flags         = flags;
    pobj->move_op       = MM_OP_READ;
    if(in_pobj) {
	pobj->buf           = in_pobj->buf;
	pobj->bytes_written = in_pobj->bytes_written;
	pobj->buflen        = in_pobj->buflen;
	pobj->partial       = in_pobj->partial;
	pobj->partial_offset= in_pobj->partial_offset;
	pobj->streaming     = in_pobj->streaming;
	pobj->streaming_put = in_pobj->streaming_put;
	pobj->retry_count   = in_pobj->retry_count;
	pobj->client_offset = in_pobj->client_offset;
	pobj->block_trigger_time = in_pobj->block_trigger_time;
	pobj->obj_trigger_time = in_pobj->obj_trigger_time;
	pobj->client_update_time =  in_pobj->client_update_time;
	if(in_pobj->attr) {
	    ret = nkn_mm_dm2_attr_copy(in_pobj->attr, &pobj->attr,
				       mod_mm_promote_get_attr);
	    if (ret != 0) {
		s_mm_free_move_mgr_obj(pobj);
		return -1;
	    }
	}
    } else {
	// ret success uri triggered success ret
	pobj->block_trigger_time = nkn_cur_ts;
	pobj->obj_trigger_time = nkn_cur_ts;
	pobj->client_update_time = update_time;
	int timediff =  pobj->obj_trigger_time - update_time;
	if (timediff >= 0) {
	    switch(timediff) {
		case 0 ... 59:
			mm_triggertime_hist[timediff/2]++;
			break;
		case 60 ... 239:
			mm_triggertime_histwide[timediff/30]++;
			break;
		default:
			mm_triggertime_histwide[0]++;
	    }
	}

    }

    /* If age-threshold is 0, then set IGNORE_EXPIRY,
     * to enable dm2 to write the expired content to
     * disk
     */
    if(!min_age)
	pobj->flags    |= NKN_MM_IGNORE_EXPIRY;

    pobj->uol.cod       = cod;
    pobj->in_proto_data = in_proto_data;
    if(in_client_data) {
	memcpy(&(pobj->in_client_data.origin_ipv4v6), &(in_client_data->origin_ipv4v6),
		sizeof(ip_addr_t));
	pobj->in_client_data.origin_port   = in_client_data->origin_port;
	memcpy(&(pobj->in_client_data.client_ipv4v6), &(in_client_data->client_ipv4v6),
		sizeof(ip_addr_t));
	pobj->in_client_data.client_port   = in_client_data->client_port;
	pobj->in_client_data.client_ip_family   = in_client_data->client_ip_family;
	pobj->in_client_data.obj_hotness   = in_client_data->obj_hotness;
	if(in_client_data->proto_data)
	    AM_copy_client_data(src_ptype, in_client_data,
		    &pobj->in_client_data);
	pobj->in_client_data.flags         = in_client_data->flags;

	if(pobj->in_client_data.flags & AM_CIM_INGEST)
	    pobj->flags |= (NKN_MM_UPDATE_CIM | NKN_MM_PIN_OBJECT);

	if(pin_object)
	    pobj->flags |= NKN_MM_PIN_OBJECT;
    }
    /* Obtain a ref cnt for this buffer
     because this task can finish indepently*/
    if(pobj->buf) {
	pobj->bufcnt++;
        glob_mm_promote_buf_holds ++;
    }
    pobj->partial_file_write = partial_file_write;


    assert(cr);
    nkn_task_set_private(TASK_TYPE_MOVE_MANAGER, taskid,
                         (void *) pobj);
    nkn_task_set_state(taskid, TASK_STATE_RUNNABLE);

    return 0;
}

nkn_provider_type_t
MM_get_next_faster_ptype(nkn_provider_type_t pt)
{
    nkn_provider_type_t     hi = 0;
    GList                   *tmp_head;
    GList                   *prev;
    GList                   *next = NULL;
    struct mm_provider      *mp1;
    struct mm_provider      *mp2;
    struct mm_provider      *mp3;
    int                     next_hit = 0;
    MM_provider_stat_t      pstat;
    int                     ret = -1;

    tmp_head = mm_provider_list;

    if(pt >= NKN_MM_max_pci_providers) {
        pt = NKN_MM_max_pci_providers - 1;
        /* The provider that was passed in was not a pci provider.
           Hence, the next provider should be a pci provider that has
           equal or better IOBW than SATA */
        next_hit = 1;
    }

    while(tmp_head) {
        mp1 = (struct mm_provider *) tmp_head->data;
        if(mp1->ptype <= pt) {
            if(next_hit == 0) {
                next = tmp_head->next;
                if(!next) {
                    break;
                }
            } else {
                next = tmp_head;
            }
            mp3 = (struct mm_provider *) next->data;
            if(mm_provider_array[mp3->ptype].put == NULL) {
                /* Only return if the put function is valid */
		/* Search the next provider */
                pt = mp3->ptype;
		next_hit = 0;
                continue;
            } else if(mm_provider_array[mp3->ptype].provider_stat) {
		/* In this case, the put routine exists for this provider
		   but we need to check whether the cache is enabled or
		   not. */
		pstat.sub_ptype = 0;
		pstat.ptype = mp3->ptype;
		ret = mm_provider_array[mp3->ptype].provider_stat(&pstat);
		if((ret < 0) || (pstat.caches_enabled == 0)) {
		    /* Search the next provider */
		    pt = mp3->ptype;
		    next_hit = 0;
		    continue;
		}
	    } else {
		/* Search the next provider */
		pt = mp3->ptype;
		next_hit = 0;
		continue;
	    }
            break;
        }
        prev = tmp_head;
        tmp_head = tmp_head->next;
    }

    if(next) {
        mp2 = (struct mm_provider *) next->data;
        hi = mp2->ptype;
    }
    else {
        DBG_LOG(WARNING, MOD_MM_PROMOTE, "\nReached max number of caches."
                "Cannot promote. This is not an error.");
        return 0;
    }
    return hi;
}

/* This api is used to find the fastest provider available for some specialized
   processing. For example, for very small files, we should use this API to get the
   fastest provider so that PUTs go very fast.*/
nkn_provider_type_t
MM_get_fastest_put_ptype(void)
{
    MM_provider_stat_t      pstat;
    int                     ret = -1;
    int                     i;

    for(i = Unknown_provider; i < NKN_MM_max_pci_providers; i++) {
        if(mm_provider_array[i].put == NULL) {
            continue;
        }

        if(mm_provider_array[i].provider_stat) {
            pstat.sub_ptype = 0;
            pstat.ptype = i;
            ret = mm_provider_array[i].provider_stat(&pstat);
            if((ret < 0) || (pstat.caches_enabled == 0)) {
                continue;
            }
            return i;
        }
    }
    return 0;
}

static int
nkn_mm_getstartend_clientoffset(uint64_t cod, int64_t *start, int64_t *end)
{
    nkn_buffer_t *objp = nkn_cod_attr_lookup(cod);
    if (objp) {
	nkn_attr_t *attrcount = nkn_buffer_getcontent(objp);
	*start = attrcount->start_offset;
	*end = attrcount->end_offset;
	return 0;
    } else {
	*start = *end = -1;
	return 1;
    }
}

static void
update_mm_ingeststat_flags(mm_move_mgr_t *pobj, int64_t offset, int64_t start, int64_t end)
{
    if (offset == 0) {
	if (start == 0) {
	    // evictted since start_offset = 0
	    pobj->flags |= NKN_MM_0_OFFSET_CLI_0_OFFSET;
	} else {
	    // byterange request from client
	     pobj->flags |= NKN_MM_0_OFFSET_CLI_NONZERO_OFFSET;
	}
    } else {
	if (offset < end) {
	    // evicted case
	    pobj->flags |= NKN_MM_MATCH_CLIENT_END;
	} else {
	    pobj->flags |= NKN_MM_NONMATCH_CLIENT_END;
	}
    }
}

int
MM_partial_promote_uri(char *uri, nkn_provider_type_t src,
		       nkn_provider_type_t dst,
		       uint64_t in_offset, uint64_t in_length,
		       uint64_t rem_length, uint64_t in_total_length,
		       time_t update_time)
{
    AM_pk_t                 am_pk;
    int                     ret = -1;
    int                     space_available = 0;
    nkn_cod_t               cod;

    am_pk.name = uri;
    am_pk.provider_id = src;

    if(src >= OriginMgr_provider) {
	return 0;
    }

    space_available = mm_glob_max_bytes_per_tier[dst] -  AO_load(&mm_glob_bytes_used_in_put[dst]);
    if(space_available <= 0) {
	glob_mm_promote_space_unavailable ++;
	return -1;
    }

    if( (AO_load(&glob_mm_promote_started) - AO_load(&glob_mm_promote_complete))
	    > NKN_MAX_PROMOTES) {
	glob_mm_promote_too_many_promotes ++;
	return -1;
    }

    AO_fetch_and_add1(&glob_mm_promote_jobs_created);
    AO_fetch_and_add1(&glob_mm_promote_started);


    cod = nkn_cod_open(uri, strlen(uri),NKN_COD_MEDIA_MGR);
    if(cod == NKN_COD_NULL) {
	glob_mm_cod_null_err ++;
	goto error;
    }
    ret = s_get_data_helper(cod, uri, in_offset, in_length, rem_length,
			    in_total_length, src, dst, NULL, 1, NULL, NULL, 0,
			    update_time);
    if(ret < 0) {
	goto error;
    }
    return 0;

error:
    glob_mm_promote_uri_err ++;
    AO_fetch_and_add1(&glob_mm_promote_jobs_created);
    s_mm_promote_complete(cod, uri, src, dst, 0, 1, 0, 0,
			  0, NKN_MM_PROMOTE_GEN_ERR, 0, in_offset,
			  -1, -1, update_time, 0);
    return -1;
}

void
nkn_mm_cim_ingest_failed(char *uri, int err_code)
{
    char      ns_uuid[NKN_MAX_UID_LENGTH];
    nkn_cod_t cod;

    cod = nkn_cod_open(uri, strlen(uri),NKN_COD_MEDIA_MGR);
    if(cod != NKN_COD_NULL) {
	ns_uuid[0]='\0';
	mm_uridir_to_nsuuid(uri, ns_uuid);
	cim_task_complete(cod, ns_uuid, err_code, ICCP_ACTION_ADD, 0, 0);
	nkn_cod_close(cod, NKN_COD_MEDIA_MGR);
    }
}

/*
 * End put function
 */
static void
s_mm_end_put(nkn_cod_t cod, nkn_provider_type_t dst)
{
    MM_put_data_t mm_put_data;

    memset(&mm_put_data, 0, sizeof(mm_put_data));
    mm_put_data.uol.cod         = cod;
    mm_put_data.uol.offset      = 0;
    mm_put_data.uol.length      = 0;
    mm_put_data.errcode         = 0;
    mm_put_data.iov_data_len    = 0;
    mm_put_data.iov_data        = NULL;
    mm_put_data.domain	        = NULL;
    mm_put_data.total_length	= 0;
    mm_put_data.ptype           = dst;
    mm_put_data.flags           = MM_FLAG_DM2_END_PUT;
    if(mm_provider_array[dst].put)
	mm_provider_array[dst].put(&mm_put_data, NKN_VT_MOVE);
}

/*
 * Free up push data structure
 * Note: This will not free up the extents. Call this only during the final 
 *	 cleanup
 */
static void
s_mm_free_mm_push_ptr(mm_push_ingest_t *mm_push_ptr)
{
    if(!mm_push_ptr)
	return;

    if(mm_push_ptr->cod != NKN_COD_NULL)
	nkn_cod_close(mm_push_ptr->cod, NKN_COD_MEDIA_MGR);

    release_namespace_token_t(mm_push_ptr->ns_token);

    if(mm_push_ptr->attr_buf) {
	nkn_buffer_release(mm_push_ptr->attr_buf);
	AO_fetch_and_sub1(&glob_am_push_ingest_attr_buffer_hold);
    }

    if(mm_push_ptr->attr)
	free((mm_push_ptr->attr));

    free(mm_push_ptr);
}

/*
 * Push ingest function - called from AM thread
 * Creates push ptr if not available. In case of continued
 * request re-uses pointer from AM entry
 */
int
MM_push_ingest(AM_obj_t *am_objp, am_xfer_data_t *am_xfer_data,
	       nkn_provider_type_t src, nkn_provider_type_t dst)
{
    const namespace_config_t *ns_cfg = NULL;
    mm_push_ingest_t *mm_push_ptr = NULL;
    mm_ext_info_t    *mm_ext_info = NULL;
    nkn_attr_t       *ap = NULL, *tobe_freed = NULL;
    int              alloc_new = 0;
    int              ret_val, ref_cnt = 0;
    int              pin_object = 0;
    time_t           min_age = 0;
    nkn_uol_t        uol;

    if(!am_objp->push_ptr) {

	if(glob_mm_push_ingest_cnt >= glob_mm_push_ingest_dyn_cnt)
	    return -1;

	if((am_xfer_data->client_flags & AM_PUSH_CONT_REQ) &&
		    !am_xfer_data->ext_info)
	    return -1;

	if(am_xfer_data->flags & AM_FLAG_PUSH_INGEST_DEL) {
	    return -1;
	}

	ns_cfg = namespace_to_config(am_objp->ns_token);
	if(!ns_cfg) {
	    return -1;
	}

	if(!dst || (dst >= NKN_MM_max_pci_providers))
	    return -1;

	pin_object = ns_cfg->cache_pin_config->cache_auto_pin;
	min_age = ns_cfg->http_config->policies.store_cache_min_age;


	mm_push_ptr = nkn_calloc_type(1, sizeof(mm_push_ingest_t),
				      mod_mm_push_t);
	if(!mm_push_ptr)
	    return -1;

	alloc_new = 1;

	mm_push_ptr->ns_token     = am_objp->ns_token;
	mm_push_ptr->src_provider = src;
	mm_push_ptr->dst_provider = dst;

	ret_val = pthread_mutex_init(&mm_push_ptr->mm_ext_entry_mutex, NULL);
	if(ret_val < 0) {
	    release_namespace_token_t(am_objp->ns_token);
	    free(mm_push_ptr);
	    return -1;
	}

	ret_val = pthread_mutex_init(&mm_push_ptr->mm_entry_mutex, NULL);
	if(ret_val < 0) {
	    release_namespace_token_t(am_objp->ns_token);
	    free(mm_push_ptr);
	    return -1;
	}

	ret_val = pthread_mutex_init(&mm_push_ptr->mm_wrt_entry_mutex, NULL);
	if(ret_val < 0) {
	    release_namespace_token_t(am_objp->ns_token);
	    free(mm_push_ptr);
	    return -1;
	}

	TAILQ_INIT(&mm_push_ptr->mm_ext_info_q);
	TAILQ_INIT(&mm_push_ptr->mm_wrt_info_q);

	/* This has to be assigned or removed only in AM thread */
	am_objp->push_ptr    = mm_push_ptr;
	mm_push_ptr->flags  |= MM_PUSH_AM_ADDED;

	if(pin_object)
	    mm_push_ptr->flags |= MM_PUSH_PIN_OBJECT;
	 if(!min_age)
	    mm_push_ptr->flags |= MM_PUSH_IGNORE_EXPIRY;

	if(glob_mm_push_ingest_parallel_ingest_restrict)
	    mm_push_ptr->flags |= MM_PUSH_FIRST_INGEST;

    } else {
	mm_push_ptr = am_objp->push_ptr;
	if(mm_push_ptr->cod != am_xfer_data->in_cod) {
	    glob_mm_push_ingest_cod_mismatch++;
	    return -1;
	}
    }

    if(am_xfer_data->flags & AM_FLAG_PUSH_INGEST_DEL) {
	goto skip_ext_update;
    }

    pthread_mutex_lock(&mm_push_ptr->mm_entry_mutex);
    if((!(mm_push_ptr->flags & MM_PUSH_INGEST_TRIGGERED)) &&
	    VALID_NS_TOKEN(am_xfer_data->ns_token) &&
	    (NS_TOKEN_VAL(mm_push_ptr->ns_token) !=
		NS_TOKEN_VAL(am_xfer_data->ns_token))) {
	ns_cfg = namespace_to_config(am_xfer_data->ns_token);
	if(ns_cfg) {
	    release_namespace_token_t(mm_push_ptr->ns_token);
	    mm_push_ptr->ns_token     = am_xfer_data->ns_token;

	    pin_object = ns_cfg->cache_pin_config->cache_auto_pin;
	    min_age = ns_cfg->http_config->policies.store_cache_min_age;

	    if(pin_object)
		mm_push_ptr->flags |= MM_PUSH_PIN_OBJECT;
	    if(!min_age)
		mm_push_ptr->flags |= MM_PUSH_IGNORE_EXPIRY;

	    if(am_xfer_data->attr_buf) {
		if(mm_push_ptr->attr_buf) {
		    nkn_buffer_release(mm_push_ptr->attr_buf);
		    AO_fetch_and_sub1(&glob_am_push_ingest_attr_buffer_hold);
		    if(mm_push_ptr->attr)
			tobe_freed = mm_push_ptr->attr;
		    mm_push_ptr->attr = NULL;
		}

		mm_push_ptr->attr_buf = am_xfer_data->attr_buf;
		nkn_buffer_hold(mm_push_ptr->attr_buf);
		AO_fetch_and_add1(&glob_am_push_ingest_attr_buffer_hold);
		ap = nkn_buffer_getcontent(mm_push_ptr->attr_buf);
		if(pin_object)
		    nkn_attr_set_cache_pin(ap);
		ret_val = nkn_mm_dm2_attr_copy(ap, &mm_push_ptr->attr,
					   mod_mm_push_t);
		if(ret_val) {
		    /* Not able to allocate attribute,
		     * for now use the old attribute as the existing ext_info
		     * structures need it. 
		     * Don't accept new ext infos. The old entries
		     * should get timedout eventually and push_ptr will be
		     * freed up.
		     */
		    mm_push_ptr->attr = tobe_freed;
		    pthread_mutex_unlock(&mm_push_ptr->mm_entry_mutex);
		    return -1;
		}
		free(tobe_freed);
	    }
	}
    }
    pthread_mutex_unlock(&mm_push_ptr->mm_entry_mutex);

    mm_ext_info = nkn_calloc_type(1, sizeof(mm_ext_info_t), mod_mm_push_t);

    if(!mm_ext_info) {
	if(alloc_new) {
	    am_objp->push_ptr = NULL;
	    release_namespace_token_t(am_xfer_data->ns_token);
	    free(mm_push_ptr);
	}
	return -1;
    }

    if(alloc_new) {
	mm_push_ptr->cod = nkn_cod_dup(am_xfer_data->in_cod,
						NKN_COD_MEDIA_MGR);
	if(am_xfer_data->attr_buf) {
	    mm_push_ptr->attr_buf = am_xfer_data->attr_buf;
	} else {
	    uol.cod = mm_push_ptr->cod;
	    uol.offset = 0;
	    uol.length = 0;
	    mm_push_ptr->attr_buf = nkn_buffer_get(&uol, NKN_BUFFER_ATTR);
	    ref_cnt = 1;
	}

	ret_val = 0;
	if(mm_push_ptr->attr_buf) {
	    if(!ref_cnt)
		nkn_buffer_hold(mm_push_ptr->attr_buf);
	    AO_fetch_and_add1(&glob_am_push_ingest_attr_buffer_hold);
	    ap = nkn_buffer_getcontent(mm_push_ptr->attr_buf);
	    if(pin_object)
		nkn_attr_set_cache_pin(ap);
	    ret_val = nkn_mm_dm2_attr_copy(ap, &mm_push_ptr->attr,
					   mod_mm_push_t);
	}

	if(ret_val || mm_push_ptr->cod == NKN_COD_NULL ||
		!mm_push_ptr->attr_buf ||
		!mm_push_ptr->attr) {
	    am_objp->push_ptr = NULL;
	    s_mm_free_mm_push_ptr(mm_push_ptr);
	    if(mm_ext_info)
		free(mm_ext_info);
	    return -1;
	}

	AO_fetch_and_add1(&glob_mm_push_ingest_cnt);
	DBG_LOG(MSG, MOD_MM, "Push ingest triggered for uri %s dst %d",
		am_objp->pk.name, dst);

    }

    if(am_xfer_data->ext_info)
	memcpy(&mm_ext_info->ext_info, am_xfer_data->ext_info,
	    sizeof(am_ext_info_t));

    pthread_mutex_lock(&mm_push_ptr->mm_ext_entry_mutex);
    TAILQ_INSERT_TAIL(&mm_push_ptr->mm_ext_info_q, mm_ext_info, entries);
    pthread_mutex_unlock(&mm_push_ptr->mm_ext_entry_mutex);

skip_ext_update:;
    pthread_mutex_lock(&mm_push_ingest_com_mutex);

    if(am_xfer_data->flags & AM_FLAG_PUSH_INGEST_DEL) {
	/* Not marked for delete or something in queue */
	if((!(mm_push_ptr->flags & MM_PUSH_ENTRY_TO_DELETE)) ||
		(mm_push_ptr->flags & MM_PUSH_IN_QUEUE)) {
	    mm_push_ptr->flags &= ~MM_PUSH_ENTRY_TO_DELETE;
	    pthread_mutex_unlock(&mm_push_ingest_com_mutex);
	    return -1;
	} else {

	    if(mm_push_ptr->flags & MM_PUSH_TMOUT_QUEUE_ADDED) {
		TAILQ_REMOVE(&mm_push_ingest_tmout_q, mm_push_ptr,
				    tmout_entries);
		mm_push_ptr->flags &= ~MM_PUSH_TMOUT_QUEUE_ADDED;
	    }

	    mm_push_ptr->flags |= MM_PUSH_ENTRY_DELETE;
	    am_objp->push_ptr   = NULL;
	    mm_push_ptr->flags &= ~MM_PUSH_AM_ADDED;
	    if(mm_push_ptr->in_disk_size != mm_push_ptr->attr->content_length) {
		if(mm_push_ptr->flags & MM_PUSH_INGEST_PUT_ERROR) {
		    AM_set_ingest_error(mm_push_ptr->cod, 0);
		    glob_mm_push_ingest_put_error++;
		    if(am_objp->pk.name) {
			DBG_LOG(MSG3, MOD_MM,"push ingest error for URI %s",
				am_objp->pk.name);
		    }
		} else {
		    /* Retry ingest */
		    if(am_objp->pk.name) {
			DBG_LOG(MSG3, MOD_MM,"push ingest complete for URI %s, "
					"file_size = %ld", am_objp->pk.name,
					mm_push_ptr->in_disk_size);
		    }
		    AM_set_ingest_error(mm_push_ptr->cod, 1);
		    glob_mm_push_ingest_partial_complete++;
		}
	    }
	    s_mm_free_mm_push_ptr(mm_push_ptr);
	    AO_fetch_and_sub1(&glob_mm_push_ingest_cnt);
	    pthread_mutex_unlock(&mm_push_ingest_com_mutex);
	    return 0;
	}
    } else {
	if(mm_push_ptr->flags & MM_PUSH_ENTRY_TO_DELETE)
	    mm_push_ptr->flags &= ~MM_PUSH_ENTRY_TO_DELETE;
	mm_push_ptr->flags &= ~MM_PUSH_FIN_CHECK;
    }

    if(!(mm_push_ptr->flags & MM_PUSH_IN_QUEUE)) {
	TAILQ_INSERT_TAIL(&nkn_mm_push_ingest_q, mm_push_ptr, entries);
	mm_push_ptr->flags |= MM_PUSH_IN_QUEUE;
    }

    pthread_cond_signal(&mm_push_ingest_q_cond);
    pthread_mutex_unlock(&mm_push_ingest_com_mutex);

    return 0;
}

/*
 * Dummy function
 */
void
mm_push_mgr_cleanup(nkn_task_id_t taskid __attribute((unused)))
{
    return;
}

/*
 * Dummy function
 */
void
mm_push_mgr_input(nkn_task_id_t id __attribute((unused)))
{
    return;
}

/*
 * Push mgr output function
 * Will be called in scheduler context after ingestion is complete
 */
void
mm_push_mgr_output(nkn_task_id_t taskid)
{
    MM_put_data_t     *pput = NULL;
    mm_push_ingest_t  *push_ptr;
    mm_ext_info_t     *write_ext;
    nkn_uol_t         uol;

    nkn_task_set_state(taskid, TASK_STATE_EVENT_WAIT);
    write_ext = (mm_ext_info_t *)nkn_task_get_private
				    (TASK_TYPE_PUSH_INGEST_MANAGER, taskid);
    pput = (MM_put_data_t *)nkn_task_get_data(taskid);
    if(!pput)
	assert(0);

    push_ptr = pput->push_ptr;
    if(!push_ptr)
	assert(0);

    pthread_mutex_lock(&mm_push_ingest_com_mutex);
    pthread_mutex_lock(&push_ptr->mm_wrt_entry_mutex);
    write_ext->flags |= MM_PUSH_WRITE_COMP;
    pthread_mutex_unlock(&push_ptr->mm_wrt_entry_mutex);

    if(pput->uol.cod != push_ptr->cod)
	NKN_ASSERT(0);

    if(pput->errcode) {
	write_ext->write_ret_val = pput->errcode;
	push_ptr->flags |= MM_PUSH_INGEST_PUT_ERROR;
    } else {
	uol.cod = push_ptr->cod;
	uol.offset = 0;
	uol.length = 0;
	nkn_uol_setattribute_provider(&uol, push_ptr->dst_provider);
    }

    DBG_LOG(MSG3, MOD_MM,"Put completed cod = %ld, offset = %ld, length = %ld",
	    pput->uol.cod, pput->uol.offset, pput->uol.length);

    push_ptr->flags |= MM_PUSH_WRITE_DONE;
    if(push_ptr->flags & MM_PUSH_INGEST_TRIGGERED) {
	push_ptr->flags &= ~(MM_PUSH_INGEST_TRIGGERED);
    }
    if(!(push_ptr->flags & MM_PUSH_IN_QUEUE)) {
	TAILQ_INSERT_TAIL(&nkn_mm_push_ingest_q, push_ptr, entries);
	push_ptr->flags |= MM_PUSH_IN_QUEUE;
    }
    pthread_cond_signal(&mm_push_ingest_q_cond);
    pthread_mutex_unlock(&mm_push_ingest_com_mutex);

    nkn_task_set_action_and_state(taskid, TASK_ACTION_CLEANUP,
				  TASK_STATE_RUNNABLE);
    free(pput);
}

/*
 * Get size of ingested object from disk and update to
 * push_ptr. This is done before completion to identify whether
 * the object is completely ingested to disk or not.
 */
static void
s_mm_update_ingested_object_size(mm_push_ingest_t *mm_push_ptr)
{
    nkn_uol_t      check_uol;
    MM_stat_resp_t in_ptr_resp;
    int            ret;

    check_uol.cod    = mm_push_ptr->cod;
    check_uol.offset = 0;
    check_uol.length = 1;

    memset((char *)&in_ptr_resp, 0, sizeof(MM_stat_resp_t));
    in_ptr_resp.ptype       = mm_push_ptr->dst_provider;
    in_ptr_resp.sub_ptype   = 0;
    in_ptr_resp.mm_stat_ret = 0;
    in_ptr_resp.in_flags   |= MM_FLAG_IGNORE_EXPIRY;

    /* Do a stat to check if the file is already present in the tier.
     * In case of partial file, resume from the partial offset.
     * If the file is modified, DM2_stat will be returned with error.
     */
    if(mm_provider_array[mm_push_ptr->dst_provider].stat)
	ret = mm_provider_array[mm_push_ptr->dst_provider].stat(check_uol,
								&in_ptr_resp);
    if(!ret && !in_ptr_resp.mm_stat_ret) {
	mm_push_ptr->in_disk_size = in_ptr_resp.tot_content_len;
    }

}

/*
 * Check for ingestible content and Ingest
 * Namespace check done here
 * In case of failure, return non-zero
 * Call with mm_push_ingest_com_mutex locked
 */
static int
s_mm_push_ingest_check_and_ingest(mm_push_ingest_t *push_ptr,
				  mm_ext_info_t *write_ext, uint32_t write_size)
{
    MM_put_data_t *pput = NULL;
    nkn_task_id_t taskid = -1;
    nkn_attr_t    *ap;
    int           count = 5;
    int           put_length = 0;
    int           size = 0, i;
    int           num_write_bufs = 0;
    int           ret;

    ap = nkn_buffer_getcontent(push_ptr->attr_buf);


    if(((uint64_t)write_ext->offset + write_ext->size) == ap->content_length)
	put_length = write_ext->size;
    else {
	if(write_ext->size < write_size)
	    return 0;
	put_length = ((write_ext->size / write_size) * write_size);
    }

    /* Permanent failure, return error */
    ret = nkn_is_object_cache_worthy(push_ptr->attr, push_ptr->ns_token, 0);
    if(ret) {
	glob_mm_push_ingest_ns_cache_worthy_failure++;
	return ret;
    }

    for(i=0;i<write_ext->ext_info.num_bufs;i++) {
	/* Permanent failure, return error */
	if(push_ptr->cod != write_ext->uol[i].cod) {
	    glob_mm_push_ingest_cod_mismatch++;
	    return -1;
	}
	write_ext->content[i].iov_base =
		    nkn_buffer_getcontent(write_ext->ext_info.buffer[i]);
	write_ext->content[i].iov_len  = write_ext->uol[i].length;
	size += write_ext->uol[i].length;
	if(size == put_length)
	    break;
    }

    num_write_bufs = i + 1;

    if(num_write_bufs > write_ext->ext_info.num_bufs)
	assert(0);

    write_ext->write_num_bufs = num_write_bufs;

    pput = (MM_put_data_t *)nkn_calloc_type(1, sizeof(MM_put_data_t),
					    mod_mm_put_data_t);
    if(pput == NULL) {
        return 0;
    }

    pput->uol.offset            = write_ext->offset;
    pput->uol.length            = put_length;
    pput->uol.cod               = push_ptr->cod;
    pput->errcode               = 0;
    pput->iov_data_len          = num_write_bufs;
    pput->domain                = 0;
    pput->iov_data              = write_ext->content;
    pput->flags                |= MM_PUT_PUSH_INGEST;


    pput->total_length          = put_length;
    pput->ptype                 = push_ptr->dst_provider;
    pput->start_time            = nkn_cur_ts;

    if(push_ptr->flags & MM_PUSH_IGNORE_EXPIRY)
	pput->flags |= MM_PUT_IGNORE_EXPIRY;

    if(push_ptr->flags & MM_PUSH_PIN_OBJECT)
	pput->cache_pin_enabled = 1;

    /* retry if scheduler cannot assign a task */
    while((taskid < 0) && (count > 0)) {
        taskid = nkn_task_create_new_task(
                                          TASK_TYPE_MEDIA_MANAGER,
                                          TASK_TYPE_PUSH_INGEST_MANAGER,
                                          TASK_ACTION_INPUT,
                                          MM_OP_WRITE,
                                          pput,
                                          sizeof(MM_put_data_t),
                                          0 /* deadline */);
        count --;
    }

    if((count == 0) || (taskid < 0)) {
        free(pput);
        return 0;
    }

    DBG_LOG(MSG3, MOD_MM,"Put started cod = %ld, offset = %ld, length = %ld",
	    pput->uol.cod, pput->uol.offset, pput->uol.length);

    pthread_mutex_lock(&push_ptr->mm_entry_mutex);

    pput->push_ptr              = push_ptr;
    pput->attr                  = push_ptr->attr; /* one time allocated, dont
						  * free
						  */
    write_ext->flags   |= MM_PUSH_WRITE_IN_PROG;
    if(push_ptr->flags & MM_PUSH_FIRST_INGEST)
	push_ptr->flags |= MM_PUSH_INGEST_TRIGGERED;

    pthread_mutex_unlock(&push_ptr->mm_entry_mutex);

    nkn_task_set_private(TASK_TYPE_PUSH_INGEST_MANAGER, taskid,
			(void *) write_ext);
    nkn_task_set_state(taskid, TASK_STATE_RUNNABLE);

    return 0;
}

/*
 * Cleanup the entry
 * Dont check for any flag
 */
static void
s_mm_push_ingest_cleanup(mm_push_ingest_t *mm_push_ptr,
			 mm_ext_info_t *exist_ext)
{
    am_ext_info_t *exist_ext_info;
    int           exist_num_bufs;
    int           i;

    exist_ext_info = &exist_ext->ext_info;
    exist_num_bufs = exist_ext_info->num_bufs;

    for(i=0;i<exist_num_bufs;i++) {
	nkn_buffer_release(exist_ext_info->buffer[i]);
	AO_fetch_and_sub1(&glob_am_push_ingest_data_buffer_hold);
	exist_ext->size -= exist_ext->uol[i].length;
	exist_ext_info->buffer[i] = 0;
    }
    exist_ext->write_num_bufs = 0;
    exist_ext_info->num_bufs -= i;
    exist_ext->offset = exist_ext->uol[0].offset;

    if(!exist_ext->ext_info.num_bufs) {
	TAILQ_REMOVE(&mm_push_ptr->mm_wrt_info_q, exist_ext, entries);
	free(exist_ext);
    }

}

/*
 * Cleanup entry after a PUT
 * Remove ingested content alone
 */
static void
s_mm_push_ingest_put_cleanup(mm_ext_info_t *exist_ext)
{
    am_ext_info_t *exist_ext_info;
    int           exist_num_bufs;
    int           i,j;

    exist_ext_info = &exist_ext->ext_info;
    exist_num_bufs = exist_ext_info->num_bufs;

    for(i=0;i<exist_ext->write_num_bufs;i++) {
	nkn_buffer_release(exist_ext_info->buffer[i]);
	AO_fetch_and_sub1(&glob_am_push_ingest_data_buffer_hold);
	exist_ext->size -= exist_ext->uol[i].length;
	exist_ext_info->buffer[i] = 0;
    }
    exist_ext->write_num_bufs = 0;
    exist_ext_info->num_bufs -= i;
    for(j=0;i<exist_num_bufs;i++,j++) {
	exist_ext_info->buffer[j] = exist_ext_info->buffer[i];
	exist_ext->uol[j] = exist_ext->uol[i];
    }

    exist_ext->offset = exist_ext->uol[0].offset;
    exist_ext->flags &= ~(MM_PUSH_WRITE_IN_PROG|MM_PUSH_WRITE_COMP);

}

/* Check extent entry and split into two for alignment
 */
static mm_ext_info_t *
s_mm_push_ingest_check_and_split(mm_ext_info_t *exist_ext, uint32_t write_size)
{
    mm_ext_info_t *new_ext = NULL;
    am_ext_info_t *exist_ext_info;
    off_t         exist_offset;
    int           exist_num_bufs, i, j;
    int           created_new_entry = 0;

    if(exist_ext->flags & MM_PUSH_WRITE_IN_PROG)
	return new_ext;

    exist_offset = exist_ext->offset;
    exist_ext_info = &exist_ext->ext_info;
    exist_num_bufs = exist_ext_info->num_bufs;

    if((exist_offset % write_size)) {
	for(i=0; i<exist_num_bufs; i++) {
	    if(!created_new_entry) {
		if(exist_ext->uol[i].offset % write_size) {
		    continue;
		} else {
		    new_ext = nkn_calloc_type(1, sizeof(mm_ext_info_t),
					      mod_mm_push_t);
		    j=0;
		    created_new_entry = 1;
		    new_ext->ext_info.buffer[j] = exist_ext_info->buffer[i];
		    new_ext->ext_info.num_bufs++;
		    exist_ext_info->num_bufs--;
		    exist_ext->size -= exist_ext->uol[i].length;
		    new_ext->size += exist_ext->uol[i].length;
		    exist_ext_info->buffer[i] = 0;
		    new_ext->uol[j] = exist_ext->uol[i];
		    new_ext->offset = new_ext->uol[0].offset;
		    new_ext->last_update_time = nkn_cur_ts;
		    exist_ext->last_update_time = nkn_cur_ts;
		    j++;
		}
	    } else {
		new_ext->ext_info.buffer[j] = exist_ext_info->buffer[i];
		new_ext->ext_info.num_bufs++;
		exist_ext_info->num_bufs--;
		exist_ext->size -= exist_ext->uol[i].length;
		new_ext->size += exist_ext->uol[i].length;
		exist_ext_info->buffer[i] = 0;
		new_ext->uol[j] = exist_ext->uol[i];
		j++;
	    }
	}
    }
    return new_ext;
}

/* 
 * Check and merge contents of 2 extents
 */
static int
s_mm_push_ingest_check_and_merge(mm_ext_info_t *new_ext,
				 mm_ext_info_t *exist_ext,
				 uint32_t write_size)
{
    off_t new_offset, exist_offset;
    off_t new_size, exist_size;
    am_ext_info_t *exist_ext_info, *new_ext_info;
    int exist_num_bufs, new_num_bufs;
    int i,j,k;
    int free_bufs, total_bufs;
    int ret = 0;
    nkn_uol_t tmp_uol;
    void *tmp_buf;

    exist_offset = exist_ext->offset;
    exist_size   = exist_ext->size;
    new_offset = new_ext->offset;
    new_size   = new_ext->size;
    exist_ext_info = &exist_ext->ext_info;
    new_ext_info = &new_ext->ext_info;
    exist_num_bufs = exist_ext_info->num_bufs;
    new_num_bufs = new_ext_info->num_bufs;

    assert((exist_num_bufs >= 0) && (exist_num_bufs <= 64));
    assert((new_num_bufs >= 0) && (new_num_bufs <= 64));

    /* Incase of write in progress for the extent return.
     */
    if(new_ext->flags & MM_PUSH_WRITE_IN_PROG)
	return ret;

    if(exist_ext->flags & MM_PUSH_WRITE_IN_PROG)
	return ret;

    /*  case 1:	exist   |------------|
		new     |-------|
		new     |------------|
		new                  |----------|
		new       |-----|
		new       |----------|
		new       |--------------|
		new     |----------------|
	case 2: exist   |------------|
		new  |--|
		new  |--------|
		new  |---------------|
	case 3: exist   |------------|
		new  |------------------|
     */
    DBG_LOG(MSG3, MOD_MM,"new offset = %ld, new_size = %ld, exist_offset = %ld,"
	    " exist_size = %ld", new_offset, new_size, exist_offset, exist_size);
    if((new_offset >= exist_offset) &&
	(new_offset <= (exist_offset + exist_size))) {

	if((new_offset+new_size) <= (exist_offset + exist_size)) {
	    for(i=0;i<new_ext_info->num_bufs;i++) {
		nkn_buffer_release(new_ext_info->buffer[i]);
		AO_fetch_and_sub1(&glob_am_push_ingest_data_buffer_hold);
		new_ext_info->buffer[i] = 0;
	    }
	    new_ext_info->num_bufs = 0;
	} else if(new_offset == (exist_offset + exist_size)) {
	    /* Incase of new offset is aligned, do not modify
	     */

	    if(!(new_offset % write_size))
		return ret;

	    if(exist_num_bufs < 64) {
		total_bufs = exist_num_bufs + new_num_bufs;
		free_bufs = 64 - exist_num_bufs;
		if(free_bufs >= (new_num_bufs))
		    free_bufs = (new_num_bufs);
		for(i=0,j=exist_num_bufs;i<free_bufs;i++,j++) {
		    exist_ext_info->buffer[j] = new_ext_info->buffer[i];
		    exist_ext->uol[j] = new_ext->uol[i];
		    exist_ext->size += new_ext->uol[i].length;
		    new_ext->size -= new_ext->uol[i].length;
		    exist_ext_info->num_bufs++;
		    new_ext_info->buffer[i] = 0;
		    ret = 0;
		}
		if(i) {
		    exist_ext->last_update_time = nkn_cur_ts;
		    new_ext->last_update_time = nkn_cur_ts;
		}
		new_ext_info->num_bufs -= i;
		for(j=0;i<new_num_bufs;i++,j++) {
		    new_ext_info->buffer[j] = new_ext_info->buffer[i];
		    new_ext->uol[j] = new_ext->uol[i];
		}
		new_ext->offset -= new_ext->uol[0].offset;
	    }
	} else {
	    for(i=0;i<new_num_bufs;i++) {
		DBG_LOG(MSG3, MOD_MM,"new offset = %ld, extOffset+size = %ld",
			new_ext->uol[i].offset, exist_offset+exist_size);
		if(new_ext->uol[i].offset ==
					exist_ext->uol[exist_num_bufs-1].offset) {
		    DBG_LOG(MSG3, MOD_MM,"new_i length = %ld, extlength = %ld",
			    new_ext->uol[i].length, exist_ext->uol[exist_num_bufs-1].length);
		    if(new_ext->uol[i].length >
					exist_ext->uol[exist_num_bufs-1].length) {
			tmp_buf = new_ext_info->buffer[i];
			new_ext_info->buffer[i] =
					exist_ext_info->buffer[exist_num_bufs-1];
			exist_ext_info->buffer[exist_num_bufs-1] = tmp_buf;
			exist_ext->size -= exist_ext->uol[exist_num_bufs-1].length;
			new_ext->size -= new_ext->uol[i].length;
			tmp_uol = exist_ext->uol[exist_num_bufs-1];
			exist_ext->uol[exist_num_bufs-1] = new_ext->uol[i];
			new_ext->uol[i] = tmp_uol;
			new_ext->size += new_ext->uol[i].length;
			exist_ext->size += exist_ext->uol[exist_num_bufs-1].length;
			new_size = new_ext->size;
			exist_size = exist_ext->size;
			new_offset = new_ext->uol[i].offset;
		    }
		}
		if(new_ext->uol[i].offset < (exist_offset + exist_size)) {
		    nkn_buffer_release(new_ext_info->buffer[i]);
		    AO_fetch_and_sub1(&glob_am_push_ingest_data_buffer_hold);
		    new_ext->size -= new_ext->uol[i].length;
		    new_ext_info->buffer[i] = 0;
		} else
		    break;
	    }
	    if(i) {
		new_ext->last_update_time = nkn_cur_ts;
	    }
	    if(exist_num_bufs < 64) {
		total_bufs = exist_num_bufs + new_num_bufs - i;
		free_bufs = 64 - exist_num_bufs;
		if(free_bufs >= (new_num_bufs - i))
		    free_bufs = (new_num_bufs - i);
		for(j=exist_num_bufs,k=0;k<free_bufs;k++,i++,j++) {
		    exist_ext_info->buffer[j] = new_ext_info->buffer[i];
		    exist_ext->uol[j] = new_ext->uol[i];
		    exist_ext->size += new_ext->uol[i].length;
		    new_ext->size -= new_ext->uol[i].length;
		    exist_ext_info->num_bufs++;
		    ret = 0;
		}
	    }
	    if(k) {
		exist_ext->last_update_time = nkn_cur_ts;
		new_ext->last_update_time = nkn_cur_ts;
	    }
	    new_ext_info->num_bufs -= i;
	    for(j=0;i<new_num_bufs;i++,j++) {
		new_ext_info->buffer[j] = new_ext_info->buffer[i];
		new_ext->uol[j] = new_ext->uol[i];
	    }
	    new_ext->offset -= new_ext->uol[0].offset;
	}
    } else if(((new_offset + new_size) >= exist_offset) &&
	((new_offset + new_size) <= (exist_offset + exist_size))) {

	/* Incase of existing offset is aligned, do not modify
	 */

	if(!(exist_offset % write_size))
	    return ret;

	if((new_offset + new_size) ==  (exist_offset)) {
	    if(new_num_bufs < 64) {
		total_bufs = exist_num_bufs + new_num_bufs;
		free_bufs = 64 - new_num_bufs;
		if(free_bufs >= (exist_num_bufs))
		    free_bufs = (exist_num_bufs);
		for(i=0,j=new_num_bufs;i<free_bufs;i++,j++) {
		    new_ext_info->buffer[j] = exist_ext_info->buffer[i];
		    new_ext->uol[j] = exist_ext->uol[i];
		    new_ext->size += exist_ext->uol[i].length;
		    exist_ext->size -= exist_ext->uol[i].length;
		    new_ext_info->num_bufs++;
		    exist_ext_info->buffer[i] = 0;
		    ret = 0;
		}
		exist_ext_info->num_bufs -= i;
		if(i) {
		    exist_ext->last_update_time = nkn_cur_ts;
		    new_ext->last_update_time = nkn_cur_ts;
		}
		for(j=0;i<exist_num_bufs;i++,j++) {
		    exist_ext_info->buffer[j] = exist_ext_info->buffer[i];
		    exist_ext->uol[j] = exist_ext->uol[i];
		}
		exist_ext->offset = exist_ext->uol[0].offset;
	    }
	} else {
	    for(i=0;i<exist_num_bufs;i++) {
		DBG_LOG(MSG3, MOD_MM,"new offset = %ld, extOffset+size = %ld",
			exist_ext->uol[i].offset, new_offset+new_size);
		if(new_ext->uol[new_num_bufs-1].offset ==
					exist_ext->uol[i].offset) {
		    DBG_LOG(MSG3, MOD_MM,"new_i length = %ld, extlength = %ld",
			    new_ext->uol[new_num_bufs-1].length, exist_ext->uol[i].length);
		    if(exist_ext->uol[i].length >
					new_ext->uol[new_num_bufs-1].length) {
			tmp_buf = new_ext_info->buffer[new_num_bufs-1];
			new_ext_info->buffer[new_num_bufs-1] =
					exist_ext_info->buffer[i];
			    exist_ext_info->buffer[i] = tmp_buf;
			new_ext->size -= new_ext->uol[new_num_bufs-1].length;
			exist_ext->size -= exist_ext->uol[i].length;
			tmp_uol = new_ext->uol[new_num_bufs-1];
			new_ext->uol[new_num_bufs-1] = exist_ext->uol[i];
			exist_ext->uol[i] = tmp_uol;
			new_ext->size += new_ext->uol[new_num_bufs-1].length;
			exist_ext->size += exist_ext->uol[i].length;
			new_size = new_ext->size;
			exist_size = exist_ext->size;
			exist_offset = exist_ext->uol[i].offset;
		    }
		}
		if(exist_ext->uol[i].offset < (new_offset + new_size)) {
		    nkn_buffer_release(exist_ext_info->buffer[i]);
		    AO_fetch_and_sub1(&glob_am_push_ingest_data_buffer_hold);
		    exist_ext->size -= exist_ext->uol[i].length;
		    exist_ext_info->buffer[i] = 0;
		} else
		    break;
	    }
	    if(i) {
		exist_ext->last_update_time = nkn_cur_ts;
	    }
	    if(new_num_bufs < 64) {
		total_bufs = exist_num_bufs + new_num_bufs - i;
		free_bufs = 64 - new_num_bufs;
		if(free_bufs >= (exist_num_bufs - i))
		    free_bufs = (exist_num_bufs - i);
		for(j=new_num_bufs,k=0;k<free_bufs;k++,i++,j++) {
		    new_ext_info->buffer[j] = exist_ext_info->buffer[i];
		    new_ext->uol[j] = exist_ext->uol[i];
		    new_ext->size += exist_ext->uol[i].length;
		    exist_ext->size -= exist_ext->uol[i].length;
		    new_ext_info->num_bufs++;
		    ret = 0;
		}
	    }
	    if(k) {
		exist_ext->last_update_time = nkn_cur_ts;
		new_ext->last_update_time = nkn_cur_ts;
	    }
	    exist_ext_info->num_bufs -= i;
	    for(j=0;i<exist_num_bufs;i++,j++) {
		exist_ext_info->buffer[j] = exist_ext_info->buffer[i];
		exist_ext->uol[j] = exist_ext->uol[i];
	    }
	    exist_ext->offset = exist_ext->uol[0].offset;
	}
    } else if((new_offset < exist_offset) &&
	(new_offset + new_size) > (exist_offset + exist_size)) {

	if(!(exist_offset % write_size))
	    return ret;

	for(i=0;i<exist_num_bufs;i++) {
	    nkn_buffer_release(exist_ext_info->buffer[i]);
	    AO_fetch_and_sub1(&glob_am_push_ingest_data_buffer_hold);
	    exist_ext_info->buffer[i] = 0;
	    ret = 0;
	}
	exist_ext_info->num_bufs = 0;
    }
    return ret;
}

/*
 * Update offset and size of the extent
 */
static inline void
s_mm_update_offset_and_size(mm_push_ingest_t *push_ptr,
			    mm_ext_info_t *mm_ext_info)
{
    am_ext_info_t *ext_info;
    off_t         new_offset, new_size;
    int           i;
    off_t         file_size = push_ptr->attr->content_length;

    ext_info = &mm_ext_info->ext_info;
    new_size = 0;
    for(i=0;i<ext_info->num_bufs;i++) {
	nkn_buffer_getid(ext_info->buffer[i],
			    &mm_ext_info->uol[i]);
	if(i==0)
	    new_offset = mm_ext_info->uol[i].offset;
	if((mm_ext_info->uol[i].offset + mm_ext_info->uol[i].length) >
		file_size) {
	    if(file_size > mm_ext_info->uol[i].offset)
		mm_ext_info->uol[i].length = file_size - mm_ext_info->uol[i].offset;
	    else
		mm_ext_info->uol[i].length = 0;
	}
	new_size += mm_ext_info->uol[i].length;
    }
    mm_ext_info->offset = new_offset;
    mm_ext_info->size   = new_size;
}

/* Check if buffer is in cache otherwise release it */
static inline void
s_mm_check_buffer(mm_push_ingest_t *push_ptr,
		  mm_ext_info_t *mm_ext_info)
{
    am_ext_info_t *ext_info;
    int           i=0,j;

    if(mm_ext_info->flags & MM_PUSH_WRITE_IN_PROG)
	return;

    ext_info = &mm_ext_info->ext_info;
    while(i<ext_info->num_bufs) {
	if(!nkn_buffer_incache(ext_info->buffer[i])) {
	    nkn_buffer_release(ext_info->buffer[i]);
	    AO_fetch_and_sub1(&glob_am_push_ingest_data_buffer_hold);
	    ext_info->num_bufs--;
	    for(j=i;j<ext_info->num_bufs;j++) {
		ext_info->buffer[j] = ext_info->buffer[j+1];
	    }
	} else
	    i++;
    }
    s_mm_update_offset_and_size(push_ptr, mm_ext_info);

}

int glob_mm_push_entry_check_tmout=2;
int glob_mm_push_entry_info_check_tmout=28800;
int glob_mm_push_ingest_buffer_ratio=10;

/*
 * Check and trigger delete of Push entry
 * This should be called within mm_push_ingest_com_mutex
 */
static void
s_mm_push_ingest_check_and_trig_remove(mm_push_ingest_t *mm_push_ptr)
{
    const char *uri = NULL;

    if(!TAILQ_FIRST(&mm_push_ptr->mm_wrt_info_q) &&
	    !TAILQ_FIRST(&mm_push_ptr->mm_ext_info_q)) {
	if(mm_push_ptr->flags & MM_PUSH_WRITE_DONE) {
	    s_mm_update_ingested_object_size(mm_push_ptr);
	    mm_push_ptr->flags &= ~MM_PUSH_WRITE_DONE;
	    if(mm_push_ptr->in_disk_size == mm_push_ptr->attr->content_length) {
		uri = nkn_cod_get_cnp(mm_push_ptr->cod);
		AM_change_obj_provider(mm_push_ptr->cod,
				       mm_push_ptr->dst_provider);
		if(uri) {
		    DBG_LOG(MSG3, MOD_MM,"push ingest complete for URI %s, "
					"file_size = %ld", uri,
					mm_push_ptr->in_disk_size);
		}
		glob_mm_push_ingest_complete++;
	    }
	}
	if(!(nkn_cod_push_ingest_get_clients(mm_push_ptr->cod))) {
	    if(!(mm_push_ptr->flags & MM_PUSH_ENTRY_TO_DELETE)) {
		nkn_cod_clear_push_in_progress(mm_push_ptr->cod);
		if(mm_push_ptr->in_disk_size <
			mm_push_ptr->attr->content_length)
		    s_mm_end_put(mm_push_ptr->cod, mm_push_ptr->dst_provider);
		AM_delete_push_ingest(mm_push_ptr->cod,
					mm_push_ptr->attr_buf);
		mm_push_ptr->flags |= MM_PUSH_ENTRY_TO_DELETE;
	    }
	}
    }
}

/*
 * Set the parallel ingest limits based on available buffers
 */
static void
s_mm_push_ingest_limit_calc(void)
{
    static int inc_update_time = 0;
    static int dec_update_time = 0;
    int used_buffer_ratio = (glob_am_push_ingest_data_buffer_hold * 100)/
				    glob_mm_push_ingest_max_bufs;
    int used_ingest_cnt_ratio = (glob_mm_push_ingest_cnt * 100)/
				    glob_mm_push_ingest_max_cnt;

    /*
     * Decrement faster than incrementing to avoid
     * buffer full condition
     * condition - 
     * buffer usage > 95 - reduce by 30%
     *		    > 75 - reduce by 20%
     *		    > 60 - reduce by 10%
     *		    < 40 - increa by 10%
     *		    < 25 - increa by 20%
     *		    < 10 - increa by 30%
     */
    if(!dec_update_time)
	dec_update_time = nkn_cur_ts;

    if(dec_update_time <= (nkn_cur_ts - 4)) {
	dec_update_time = nkn_cur_ts;
	if(used_buffer_ratio > 95) {
	    glob_mm_push_ingest_dyn_cnt -= (glob_mm_push_ingest_dyn_cnt * 30)/100;
	} else if(used_buffer_ratio > 75) {
	    glob_mm_push_ingest_dyn_cnt -= ((glob_mm_push_ingest_dyn_cnt * 20)/100);
	} else if(used_buffer_ratio > 60) {
	    glob_mm_push_ingest_dyn_cnt -= ((glob_mm_push_ingest_dyn_cnt * 10)/100);
	}
    }

    if(!inc_update_time)
	inc_update_time = nkn_cur_ts;

    if(inc_update_time <= (nkn_cur_ts - 11)) {
	inc_update_time = nkn_cur_ts;
	if(used_buffer_ratio < 10) {
	    if(used_ingest_cnt_ratio > 95) {
		glob_mm_push_ingest_dyn_cnt += ((glob_mm_push_ingest_dyn_cnt * 30)/100);
	    }
	} else if(used_buffer_ratio < 25) {
	    if(used_ingest_cnt_ratio > 95) {
		glob_mm_push_ingest_dyn_cnt += ((glob_mm_push_ingest_dyn_cnt * 20)/100);
	    }
	} else if(used_buffer_ratio < 40) {
	    if(used_ingest_cnt_ratio > 95) {
		glob_mm_push_ingest_dyn_cnt += ((glob_mm_push_ingest_dyn_cnt * 10)/100);
	    }
	}
    }

    if(glob_mm_push_ingest_dyn_cnt < glob_mm_push_ingest_max_cnt)
	glob_mm_push_ingest_dyn_cnt = glob_mm_push_ingest_max_cnt;
    if(glob_mm_push_ingest_dyn_cnt > glob_mm_push_ingest_limit)
	glob_mm_push_ingest_dyn_cnt = glob_mm_push_ingest_limit;
}

/* 
 * Push ingest thread
 * - Time out checks for unused buffers
 *   Maintain new ingest, merge new extents with existing extents
 *   Triggers ingest when ingest conditions are met.
 */
void *
s_mm_push_ingest_thread(void *dummy_var __attribute((unused)))
{
    struct timespec  abstime;
    mm_push_ingest_t *mm_push_ptr;
    mm_ext_info_t    *mm_ext_info, *mm_wrt_info, *mm_wrt_tmp_info,
		     *mm_new_ext_info, *mm_wrt_info_next, *mm_wrt_info_ent;
    uint32_t         write_size;
    int              ret;
    const char       *uri;

    prctl(PR_SET_NAME, "nvsd-mm-push-ingest", 0, 0, 0);

    while(1) {
	/* Update the limits */
	s_mm_push_ingest_limit_calc();

	/* Timeout functionality */
	pthread_mutex_lock(&mm_push_ingest_com_mutex);
	TAILQ_FOREACH(mm_push_ptr, &mm_push_ingest_tmout_q, tmout_entries) {

	    if(mm_push_ptr->flags & MM_PUSH_ENTRY_TO_DELETE)
		continue;

	    if(!mm_push_ptr->last_chk_time)
		mm_push_ptr->last_chk_time = nkn_cur_ts;

	    if(mm_push_ptr->last_chk_time >=
		    (nkn_cur_ts - glob_mm_push_entry_check_tmout)) {
		continue;
	    }

	    if(!(nkn_cod_push_ingest_get_clients(mm_push_ptr->cod))) {
		if(!(mm_push_ptr->flags & MM_PUSH_FIN_CHECK)) {
		    mm_push_ptr->flags |= MM_PUSH_FIN_CHECK;
		    pthread_mutex_unlock(&mm_push_ingest_com_mutex);
		    goto next_ext_info_entry;
		}
	    } else
		mm_push_ptr->flags &= ~MM_PUSH_FIN_CHECK;

	    pthread_mutex_lock(&mm_push_ptr->mm_wrt_entry_mutex);
	    mm_push_ptr->last_chk_time = nkn_cur_ts;
	    TAILQ_FOREACH_SAFE(mm_wrt_info, &mm_push_ptr->mm_wrt_info_q,
			       entries, mm_wrt_tmp_info) {
		if(!mm_wrt_info->last_update_time)
		    mm_wrt_info->last_update_time = nkn_cur_ts;
		if(mm_wrt_info->last_update_time <
			(nkn_cur_ts - glob_mm_push_entry_info_check_tmout)) {
		    if(!(mm_wrt_info->flags & MM_PUSH_WRITE_IN_PROG)) {
			glob_mm_push_ingest_tmout_entry_info_removed++;
			s_mm_push_ingest_cleanup(mm_push_ptr, mm_wrt_info);
		    }
		}
	    }
	    pthread_mutex_unlock(&mm_push_ptr->mm_wrt_entry_mutex);
	    s_mm_push_ingest_check_and_trig_remove(mm_push_ptr);
	}

	/* Get new updates from AM */
	mm_push_ptr = TAILQ_FIRST(&nkn_mm_push_ingest_q);
	if(mm_push_ptr) {
	    TAILQ_REMOVE(&nkn_mm_push_ingest_q, mm_push_ptr, entries);
	    mm_push_ptr->flags &= ~MM_PUSH_IN_QUEUE;

	    if(!(mm_push_ptr->flags & MM_PUSH_TMOUT_QUEUE_ADDED)) {
		TAILQ_INSERT_TAIL(&mm_push_ingest_tmout_q, mm_push_ptr,
				    tmout_entries);
		mm_push_ptr->flags |= MM_PUSH_TMOUT_QUEUE_ADDED;
	    }
	}

	pthread_mutex_unlock(&mm_push_ingest_com_mutex);

	if(!mm_push_ptr)
	    goto ingest_wait;


next_ext_info_entry:;
	/* Update the write size */
	if(nkn_mm_small_write_size[mm_push_ptr->dst_provider])
	    write_size = nkn_mm_small_write_size[mm_push_ptr->dst_provider];
	else
	    write_size = nkn_mm_block_size[mm_push_ptr->dst_provider];

	if(!write_size)
	    write_size = NKN_MAX_BLOCK_SZ;


	uri = nkn_cod_get_cnp(mm_push_ptr->cod);
	/* Get the new ext info and merge with existing entry */
	pthread_mutex_lock(&mm_push_ptr->mm_ext_entry_mutex);
	mm_ext_info = TAILQ_FIRST(&mm_push_ptr->mm_ext_info_q);
	if(mm_ext_info) {
	    TAILQ_REMOVE(&mm_push_ptr->mm_ext_info_q, mm_ext_info, entries);
	    pthread_mutex_unlock(&mm_push_ptr->mm_ext_entry_mutex);
	    mm_ext_info->last_update_time = nkn_cur_ts;
	    s_mm_update_offset_and_size(mm_push_ptr, mm_ext_info);
	    s_mm_check_buffer(mm_push_ptr, mm_ext_info);
	    DBG_LOG(MSG3, MOD_MM,"push update for URI %s offset = %ld,"
		    " lenght = %d", uri, mm_ext_info->offset,
		    mm_ext_info->size);
	    pthread_mutex_lock(&mm_push_ptr->mm_wrt_entry_mutex);
	    TAILQ_FOREACH_SAFE(mm_wrt_info, &mm_push_ptr->mm_wrt_info_q,
			       entries, mm_wrt_tmp_info) {
		s_mm_check_buffer(mm_push_ptr, mm_wrt_info);
		if(!mm_wrt_info->ext_info.num_bufs) {
		    TAILQ_REMOVE(&mm_push_ptr->mm_wrt_info_q, mm_wrt_info,
				entries);
		    free(mm_wrt_info);
		    continue;
		}
		ret = s_mm_push_ingest_check_and_merge(mm_ext_info,
						       mm_wrt_info, write_size);
		if(!mm_wrt_info->ext_info.num_bufs) {
		    TAILQ_REMOVE(&mm_push_ptr->mm_wrt_info_q, mm_wrt_info,
				 entries);
		    free(mm_wrt_info);
		}
		if(!mm_ext_info->ext_info.num_bufs)
		    break;
	    }

	    /* Check for alignment issues here */
	    if(mm_ext_info->ext_info.num_bufs) {
		if(mm_ext_info->offset % write_size) {
		    mm_new_ext_info =
			s_mm_push_ingest_check_and_split(mm_ext_info,
							 write_size);
		    if(mm_new_ext_info)
			TAILQ_INSERT_TAIL(&mm_push_ptr->mm_wrt_info_q,
					  mm_new_ext_info, entries);

		}
		TAILQ_INSERT_TAIL(&mm_push_ptr->mm_wrt_info_q, mm_ext_info,
				  entries);
	    } else {
		free(mm_ext_info);
	    }
#if 0
	    TAILQ_FOREACH_SAFE(mm_wrt_info, &mm_push_ptr->mm_wrt_info_q,
			       entries, mm_wrt_tmp_info) {
		DBG_LOG(MSG3, MOD_MM,"Entries - URI %s offset = %ld,"
		    " lenght = %ld", uri, mm_wrt_info->offset,
		    mm_wrt_info->size);
	    }
#endif
	    pthread_mutex_unlock(&mm_push_ptr->mm_wrt_entry_mutex);
	    goto next_ext_info_entry;
	} else {
	    pthread_mutex_unlock(&mm_push_ptr->mm_ext_entry_mutex);
	}

	/* Cleanup entries after a put, do a split if data is unaligned */
	pthread_mutex_lock(&mm_push_ptr->mm_wrt_entry_mutex);
	TAILQ_FOREACH_SAFE(mm_wrt_info, &mm_push_ptr->mm_wrt_info_q,
			   entries, mm_wrt_tmp_info) {
	    if(mm_wrt_info->flags & MM_PUSH_WRITE_COMP) {
		s_mm_push_ingest_put_cleanup(mm_wrt_info);
		if(!mm_wrt_info->ext_info.num_bufs) {
		    TAILQ_REMOVE(&mm_push_ptr->mm_wrt_info_q, mm_wrt_info,
				 entries);
		    free(mm_wrt_info);
		    continue;
		}
	    }

	    if(mm_wrt_info->offset % write_size) {
		mm_new_ext_info =
		    s_mm_push_ingest_check_and_split(mm_wrt_info,
						    write_size);
		if(mm_new_ext_info)
		    TAILQ_INSERT_TAIL(&mm_push_ptr->mm_wrt_info_q,
				    mm_new_ext_info, entries);
	    }
	    s_mm_update_offset_and_size(mm_push_ptr, mm_wrt_info);
	}

#if 0
	TAILQ_FOREACH_SAFE(mm_wrt_info, &mm_push_ptr->mm_wrt_info_q,
			       entries, mm_wrt_tmp_info) {
	    DBG_LOG(MSG3, MOD_MM,"Entries - URI %s offset = %ld,"
		    " lenght = %ld", uri, mm_wrt_info->offset,
		    mm_wrt_info->size);
	}
#endif

	/* Merge Older entries */
	mm_wrt_info_ent = TAILQ_FIRST(&mm_push_ptr->mm_wrt_info_q);
	while(mm_wrt_info_ent) {
	    mm_wrt_info = TAILQ_NEXT(mm_wrt_info_ent, entries);
	    while(mm_wrt_info) {
		mm_wrt_info_next = TAILQ_NEXT(mm_wrt_info, entries);
		ret = s_mm_push_ingest_check_and_merge(mm_wrt_info_ent,
						       mm_wrt_info, write_size);
		if(!mm_wrt_info->ext_info.num_bufs) {
		    TAILQ_REMOVE(&mm_push_ptr->mm_wrt_info_q, mm_wrt_info,
				entries);
		    free(mm_wrt_info);
		}
		mm_wrt_info = mm_wrt_info_next;
		if(!mm_wrt_info_ent->ext_info.num_bufs)
		    break;
	    }
	    mm_wrt_tmp_info = TAILQ_NEXT(mm_wrt_info_ent, entries);
	    if(!mm_wrt_info_ent->ext_info.num_bufs) {
		TAILQ_REMOVE(&mm_push_ptr->mm_wrt_info_q, mm_wrt_info_ent,
				entries);
		free(mm_wrt_info_ent);
	    }
	    mm_wrt_info_ent = mm_wrt_tmp_info;
	}

#if 0
	TAILQ_FOREACH_SAFE(mm_wrt_info, &mm_push_ptr->mm_wrt_info_q,
			       entries, mm_wrt_tmp_info) {
	    DBG_LOG(MSG3, MOD_MM,"Entries - URI %s offset = %ld,"
		    " lenght = %ld", uri, mm_wrt_info->offset,
		    mm_wrt_info->size);
	}
#endif
	pthread_mutex_unlock(&mm_push_ptr->mm_wrt_entry_mutex);

	pthread_mutex_lock(&mm_push_ingest_com_mutex);
	if(mm_push_ptr->flags & MM_PUSH_INGEST_TRIGGERED) {
	    pthread_mutex_unlock(&mm_push_ingest_com_mutex);
	    continue;
	}

	pthread_mutex_lock(&mm_push_ptr->mm_wrt_entry_mutex);
	/* Check for writable entry and trigger ingest */
	TAILQ_FOREACH_SAFE(mm_wrt_info, &mm_push_ptr->mm_wrt_info_q,
			  entries, mm_wrt_tmp_info) {


	    if(!(mm_wrt_info->size) ||
		    (mm_wrt_info->offset % write_size) ||
		    (mm_wrt_info->flags & MM_PUSH_WRITE_IN_PROG) ||
		    (mm_push_ptr->flags & MM_PUSH_INGEST_TRIGGERED))
		continue;

	    ret = s_mm_push_ingest_check_and_ingest(mm_push_ptr, mm_wrt_info,
					      write_size);
	    if(ret)
		s_mm_push_ingest_cleanup(mm_push_ptr, mm_wrt_info);
	}
	pthread_mutex_unlock(&mm_push_ptr->mm_wrt_entry_mutex);

	/* Cleanup entry if nothing has to be done */
	s_mm_push_ingest_check_and_trig_remove(mm_push_ptr);
	pthread_mutex_unlock(&mm_push_ingest_com_mutex);

	continue;

ingest_wait:;
        clock_gettime(CLOCK_MONOTONIC, &abstime);
        abstime.tv_sec += 1;
        abstime.tv_nsec = 0;
	pthread_mutex_lock(&mm_push_ingest_com_mutex);
	pthread_cond_timedwait(&mm_push_ingest_q_cond, &mm_push_ingest_com_mutex,
			       &abstime);
	pthread_mutex_unlock(&mm_push_ingest_com_mutex);
    }
}

int
MM_promote_uri(char *uri, nkn_provider_type_t src, nkn_provider_type_t dst,
		nkn_cod_t in_cod, am_object_data_t *in_client_data,
		time_t update_time)
{
    AM_pk_t		am_pk;
    int			ret = -1;
    int			space_available = 0;
    nkn_cod_t		cod;
    void		*in_proto_data = NULL;
    int			flag = 0;
    int			err_val = NKN_MM_PROMOTE_GEN_ERR;
    int			cim_cleanup = 0;
    off_t		offset = 0, req_len = 0, rem_len = 0, tot_len = 0,
			write_offset;
    nkn_uol_t           check_uol;
    MM_stat_resp_t	in_ptr_resp;


    am_pk.name = uri;
    am_pk.provider_id = src;

    if(src > OriginMgr_provider) {
	return 0;
    }

    if(in_client_data && (in_client_data->flags & AM_INGEST_HP_QUEUE)) {
	if((glob_mm_hp_promote_started -
		    glob_mm_hp_promote_complete)
		< glob_mm_hp_max_promotes) {
	    AO_fetch_and_add1(&glob_mm_hp_promote_started);
	    flag |= NKN_MM_HP_QUEUE;
	    goto skip_queue_checks;
	}
    }

    space_available = mm_glob_max_bytes_per_tier[dst] -
				AO_load(&mm_glob_bytes_used_in_put[dst]);
    if(space_available <= 0) {
	glob_mm_promote_space_unavailable ++;
	return -1;
    }

    if(nkn_mm_tier_promotes[dst]) {
	if((nkn_mm_tier_promote_started[dst] -
		    nkn_mm_tier_promote_complete[dst])
		< nkn_mm_tier_promotes[dst]) {
	    AO_fetch_and_add1(&nkn_mm_tier_promote_started[dst]);
	    flag |= NKN_MM_TIER_SPECIFIC_QUEUE;
	}
    }

    if(!flag && ((AO_load(&glob_mm_promote_started) -
		    AO_load(&glob_mm_promote_complete))
	    > glob_mm_common_promotes)) {
	glob_mm_promote_too_many_promotes ++;
	return -1;
    }

    if(!flag)
	AO_fetch_and_add1(&glob_mm_promote_started);

skip_queue_checks:;
    if(in_client_data && (in_client_data->flags & AM_CIM_INGEST))
	    flag |= NKN_MM_UPDATE_CIM;

    AO_fetch_and_add1(&glob_mm_promote_jobs_created);
    AO_fetch_and_add1(&nkn_mm_tier_ingest_promote_cnt[dst]);

    /* If we already have the cod, use it otherwise use the uri to get the cod
     */
    if(in_cod) {
	cod = nkn_cod_dup(in_cod, NKN_COD_MEDIA_MGR);
	if(cod == NKN_COD_NULL) {
	    glob_mm_cod_null_err ++;
	    goto error;
	}
    } else {
	cod = nkn_cod_open(uri, strlen(uri), NKN_COD_MEDIA_MGR);
	if(cod == NKN_COD_NULL) {
	    glob_mm_cod_null_err ++;
	    goto error;
	}
	if((flag & NKN_MM_UPDATE_CIM) && (cod != in_client_data->in_cod)) {
	    glob_cim_mm_cod_mismatch ++;
	    err_val = NKN_MM_CIM_RETRY;
	    cim_cleanup = 1;
	    goto error;
	}
    }

    if(nkn_cod_is_ingest_in_progress(cod)) {
	DBG_LOG(ERROR, MOD_MM, "Ingest already in progress for cod %ld", cod);
	goto error;
    } else {
	nkn_cod_set_ingest_in_progress(cod);
    }

    if(in_client_data && ((in_client_data->flags & AM_NEW_INGEST) ||
			  (in_client_data->flags & AM_OBJ_TYPE_STREAMING) ||
			  (in_client_data->flags & AM_CIM_INGEST)))
	goto skip_stat_check;

    check_uol.cod = cod;
    check_uol.offset = 0;
    check_uol.length = 1;
    memset((char *)&in_ptr_resp, 0, sizeof(MM_stat_resp_t));
    in_ptr_resp.ptype = dst;
    in_ptr_resp.sub_ptype = 0;
    in_ptr_resp.mm_stat_ret = 0;
    in_ptr_resp.in_flags |= (MM_FLAG_IGNORE_EXPIRY|MM_FLAG_MM_ING_REQ);

    /* Do a stat to check if the file is already present in the tier.
     * In case of partial file, resume from the partial offset.
     * If the file is modified, DM2_stat will be returned with error.
     */
    if(mm_provider_array[dst].stat)
	ret = mm_provider_array[dst].stat(check_uol, &in_ptr_resp);
    write_offset = nkn_mm_small_write_size[dst];
    if(!write_offset)
	write_offset = nkn_mm_block_size[dst];
    if(in_ptr_resp.out_flags & MM_OFLAG_OBJ_WITH_HOLE) {
	err_val = NKN_MM_PULL_INGST_OBJ_WITH_HOLE;
	goto error;
    }
    if(!ret && !in_ptr_resp.mm_stat_ret) {
	if(in_ptr_resp.tot_content_len &&
		(in_ptr_resp.tot_content_len != in_ptr_resp.content_len)) {
	    if(!(in_ptr_resp.unavail_offset % write_offset)) {
		offset = in_ptr_resp.unavail_offset;
		req_len = in_ptr_resp.content_len - in_ptr_resp.tot_content_len;
		rem_len = in_ptr_resp.content_len - in_ptr_resp.tot_content_len;
		tot_len = in_ptr_resp.content_len;
	    }
	}
    }

skip_stat_check:;

    if(offset != 0) {
	/* possibly a partial file with invalid offset.
	 * It should be fine to delete it
	 */
	if(offset % nkn_mm_block_size[dst]) {
	    err_val = NKN_DM2_PARTIAL_EXTENT_EEXIST;
	    goto error;
	}

	glob_mm_partial_overwrite_avoided ++;
    }

    ret = s_get_data_helper(cod, uri, offset, req_len, rem_len, tot_len,
			    src, dst, NULL, 0, in_proto_data, in_client_data, flag,
			    update_time);
    if(ret < 0) {
	goto error;
    }
    return 0;

error:
    glob_mm_promote_uri_err ++;
    s_mm_promote_complete(cod, uri, src, dst, 0, 0, in_proto_data, 0,
			    0, err_val, flag, 0, -1, -1,
			    update_time, 0);
    if(cim_cleanup)
	cim_task_complete(in_client_data->in_cod, NULL, NKN_MM_CIM_RETRY,
			    ICCP_ACTION_ADD, 0, 0);
    return -1;
}

int
MM_stat(nkn_uol_t uol,
        MM_stat_resp_t *in_ptr_resp)
{
    int                      i;
    GList                    *plist;
    int                      ret_val = -1;
    MM_provider_stat_t       *pstat;
    provider_cookie_t        prov_id;
    nkn_provider_type_t      orig_ptype;
    const namespace_config_t *cfg = NULL;
    int                      found = 0;
    char                     *uri;
    int                      len;

    if((!in_ptr_resp) || (uol.offset < 0) || (uol.length < 0)) {
        in_ptr_resp->mm_stat_ret = NKN_MM_STAT_URI_ERR;
        goto finish;
    }

    ret_val = nkn_cod_get_cn(uol.cod, &uri, &len);
    if(ret_val == NKN_COD_INVALID_COD) {
        in_ptr_resp->mm_stat_ret = NKN_MM_STAT_COD_ERR;
	glob_mm_stat_cod_err ++;
        goto finish;
    }

    /* Find out which origin provider from the namespace config */
    cfg = namespace_to_config(in_ptr_resp->ns_token);
    if(!cfg) {
	in_ptr_resp->mm_stat_ret = NKN_MM_STAT_URI_ERR;
	goto finish;
    }

    switch(cfg->http_config->origin.select_method) {
    case OSS_HTTP_CONFIG:
    case OSS_HTTP_PROXY_CONFIG:
    case OSS_HTTP_ABS_URL:
    case OSS_HTTP_FOLLOW_HEADER:
    case OSS_HTTP_DEST_IP:
    case OSS_HTTP_SERVER_MAP:
    case OSS_HTTP_NODE_MAP:
	orig_ptype = OriginMgr_provider;
	break;
    case OSS_NFS_CONFIG:
    case OSS_NFS_SERVER_MAP:
	orig_ptype = NFSMgr_provider;
	break;
    default:
	orig_ptype = Unknown_provider;
    }
    release_namespace_token_t(in_ptr_resp->ns_token);

    if((in_ptr_resp->in_flags & MM_FLAG_NO_CACHE) == 0) {
	for(i = 0; i < NKN_MM_origin_providers; i ++) {
	    if (mm_provider_array[i].state != MM_PROVIDER_STATE_ACTIVE)
		continue;
	    if((in_ptr_resp->in_flags & MM_FLAG_CACHE_ONLY) &&
		    (i >= NKN_MM_max_pci_providers))
		break;
	    plist = mm_provider_array[i].sub_prov_list;
            if((i < NKN_MM_max_pci_providers) && nkn_sac_per_thread_limit[i] &&
                (AO_load(&nkn_mm_get_queue_cnt[i]) >=
                (nkn_sac_per_thread_limit[i] *
		    mm_provider_array[i].num_get_threads))){
		AO_fetch_and_add1(&nkn_mm_tier_sac_reject_cnt[i]);
                continue;
            }

	    while (plist) {
		if(mm_provider_array[i].stat != NULL) {
		    pstat = (MM_provider_stat_t *) plist->data;
		    if(!pstat) {
			break;
		    }
		    in_ptr_resp->ptype = pstat->ptype;
		    in_ptr_resp->sub_ptype = pstat->sub_ptype;

		    if(mm_provider_array[i].stat != NULL) {
			in_ptr_resp->mm_stat_ret = 0;
			in_ptr_resp->debug = NKN_MM_DEBUG_CALLED;
			ret_val = mm_provider_array[i].stat(
							    uol, in_ptr_resp);
		    }

		    if(ret_val == 0) {
			prov_id = AM_encode_provider_cookie(pstat->ptype,
							    pstat->sub_ptype, 0);
			in_ptr_resp->ptype = prov_id;
			i = NKN_MM_origin_providers;
			found = 1;
			goto finish;
		    } else if (ret_val == -NKN_FOUND_BUT_NOT_VALID_YET) {
			/* Object exists but not ready to be served */
			goto finish;
		    }
		}
		plist = plist->next;
	    }
	}

	if(found == 0) {
	    /* Dont go to the origin provider in case CACHE_ONLY
	     * flag is set
	     */
	    if(in_ptr_resp && in_ptr_resp->in_flags & MM_FLAG_CACHE_ONLY) {
		in_ptr_resp->mm_stat_ret = NKN_MM_OBJ_NOT_FOUND_IN_CACHE;
		ret_val = NKN_MM_OBJ_NOT_FOUND_IN_CACHE;
		goto finish;
	    }
	    /* Go through origin providers because we didnt find it
	     in the cache providers */
	    if(orig_ptype == Unknown_provider) {
		if(in_ptr_resp) {
		    in_ptr_resp->mm_stat_ret = NKN_MM_STAT_URI_ERR;
		    ret_val = NKN_MM_STAT_URI_ERR;
		    goto finish;
		}
	    }

	    if(mm_provider_array[orig_ptype].stat != NULL) {
		in_ptr_resp->ptype = orig_ptype;
		in_ptr_resp->sub_ptype = 0;
		in_ptr_resp->mm_stat_ret = 0;
		in_ptr_resp->debug = NKN_MM_DEBUG_CALLED;
		ret_val = mm_provider_array[orig_ptype].stat(uol, in_ptr_resp);
	    }
	    if(ret_val == 0) {
		prov_id = AM_encode_provider_cookie(orig_ptype,
						    0, 0);
		in_ptr_resp->ptype = prov_id;
		ret_val = 0;
	    } else if(!in_ptr_resp->mm_stat_ret) {
		in_ptr_resp->mm_stat_ret = NKN_MM_STAT_URI_ERR;
	    }
	}
    } else {
	/* No-cache option is set. Miss the provider caches and go
	 only to the origin providers */
	if(orig_ptype == Unknown_provider) {
	    if(in_ptr_resp) {
		in_ptr_resp->mm_stat_ret = NKN_MM_STAT_URI_ERR;
		ret_val = NKN_MM_STAT_URI_ERR;
		goto finish;
	    }
	}

	if(mm_provider_array[orig_ptype].stat != NULL) {
	    in_ptr_resp->ptype = orig_ptype;
	    in_ptr_resp->sub_ptype = 0;
	    in_ptr_resp->mm_stat_ret = 0;
	    in_ptr_resp->debug = NKN_MM_DEBUG_CALLED;
	    ret_val = mm_provider_array[orig_ptype].stat(
							 uol, in_ptr_resp);
	    if(ret_val == 0) {
		prov_id = AM_encode_provider_cookie(orig_ptype,
						    0, 0);
		in_ptr_resp->ptype = prov_id;
		ret_val = 0;
	    } else if(!in_ptr_resp->mm_stat_ret) {
		in_ptr_resp->mm_stat_ret = NKN_MM_STAT_URI_ERR;
	    }
	} else {
	    in_ptr_resp->mm_stat_ret = NKN_MM_STAT_URI_ERR;
	    ret_val = NKN_MM_STAT_URI_ERR;
	}
    }

 finish:
    if(ret_val == 0) {
        glob_mm_stat_success ++;
    } else {
	if (ret_val == -NKN_FOUND_BUT_NOT_VALID_YET)
	    in_ptr_resp->mm_stat_ret = NKN_FOUND_BUT_NOT_VALID_YET;
	if(!in_ptr_resp->mm_stat_ret)
	    in_ptr_resp->mm_stat_ret = NKN_MM_STAT_URI_ERR;
        glob_mm_stat_err ++;
    }
    return ret_val;
}

int
MM_put(struct MM_put_data *in_data)
{
    int                         ret_val = -1;
    nkn_provider_type_t         ptype;
    nkn_provider_type_t         tmp = in_data->ptype;
    GList                       *plist = NULL;
    MM_provider_stat_t          *pstat;
    int				done = 0;
    char                        *uri;
    int                         len;
    uint64_t			put_start_time, put_end_time, put_time_taken;

    glob_mm_puts ++;
    glob_mm_put_cnt ++;

    ret_val = nkn_cod_get_cn(in_data->uol.cod, &uri, &len);
    if(ret_val == NKN_COD_INVALID_COD) {
	in_data->errcode = -EINVAL;
        glob_mm_put_cod_err ++;
        return -1;
    }

    if(tmp == Unknown_provider) {
        tmp = MM_get_next_faster_ptype(NKN_MM_max_pci_providers);
        assert(tmp < NKN_MM_max_pci_providers);
    }

    for(ptype = tmp; ptype > Unknown_provider;
	ptype = MM_get_next_faster_ptype(ptype)) {
	if(ptype >= NKN_MM_MAX_CACHE_PROVIDERS) {
	    in_data->errcode = -EINVAL;
	    return -1;
	}
	/* Check all the sub-providers for the threshold.
	   Cache providers will return error if the PUT fails. */
	plist = mm_provider_array[ptype].sub_prov_list;
	for (; plist; plist = plist->next) {
	    pstat = (MM_provider_stat_t *)plist->data;
	    ret_val = mm_provider_array[ptype].provider_stat(pstat);
	    if(ret_val < 0) {
		continue;
	    }
	    if(mm_provider_array[ptype].put) {
		in_data->sub_ptype = pstat->sub_ptype;
		in_data->ptype = ptype;
		in_data->debug = NKN_MM_DEBUG_CALLED;
		put_start_time = nkn_cur_ts;
		ret_val = mm_provider_array[ptype].put(in_data, NKN_VT_MOVE);
		put_end_time = nkn_cur_ts;
		put_time_taken = put_end_time - put_start_time;
		if(put_time_taken > glob_mm_put_max_interval) {
		    glob_mm_put_max_interval = put_time_taken;
		}
		if(ret_val >= 0) {
		    ret_val = 0;
		    done = 1;
		    break;
#if 0
                /* Bug 2908: For some reason, we were trying other providers
                   if the PUT to one particular provider failded. Now, that is
                   not a requirement and it was causing 2908. Removing it.*/
		} else if((ret_val != -EIO) && (ret_val != -ENOSPC) &&
                         (ret_val != -NKN_DM2_EMPTY_CACHE_TIER)) {
                    /* Retry with other providers if the ret_val was
                        EIO and ENOSPC*/
#endif
		} else { 
                    /* If there is a failure with the target cache tier, just fail the PUT*/
                    ret_val = -1;
                    done = 1;
                    break;
                }
	    }
	    else {
		return -1;
	    }
	}
	if(done)
	    break;
    }
    if(ret_val < 0)
        glob_mm_put_err ++;
    else
        glob_mm_put_success ++;

    return ret_val;
}

int
MM_validate(MM_validate_t *pvld)
{
    nkn_provider_type_t ptype;
    char                *uri;
    int                 len, ret_val;

    glob_mm_validates ++;

    if(!pvld) {
	return -1;
    }

    ret_val = nkn_cod_get_cn(pvld->in_uol.cod, &uri, &len);
    if(ret_val == NKN_COD_INVALID_COD) {
	pvld->ret_code = MM_VALIDATE_ERROR;
        glob_mm_validate_cod_err ++;
        return -1;
    }

    ptype = pvld->ptype;
    if(mm_provider_array[ptype].validate != NULL) {
	pvld->debug = NKN_MM_DEBUG_CALLED;
	mm_provider_array[ptype].validate(pvld);
    } else {
        pvld->ret_code = MM_VALIDATE_ERROR;
	return -1;
    }
    return 0;
}

int
MM_update_attr(MM_update_attr_t *pup)
{
    nkn_provider_type_t ptype = pup->in_ptype;

    if (mm_provider_array[ptype].state != MM_PROVIDER_STATE_ACTIVE)
	return 0;

    glob_mm_update_attrs ++;

    glob_mm_update_attr_called ++;
    if(mm_provider_array[ptype].update_attr != NULL) {
	pup->debug = NKN_MM_DEBUG_CALLED;
	mm_provider_array[ptype].update_attr(pup);
    } else {
	return -1;
    }
    return 0;
}

int
MM_provider_stat(MM_provider_stat_t *pstat)
{
    int ret = -1;

    if((pstat->ptype <= 0) || (pstat->ptype >= NKN_MM_MAX_CACHE_PROVIDERS)) {
        return -1;
    }

    if(pstat->sub_ptype != 0) {
        pstat->sub_ptype = 0;
    }

    if (mm_provider_array[pstat->ptype].state != MM_PROVIDER_STATE_ACTIVE)
	return -1;

    if(mm_provider_array[pstat->ptype].provider_stat) {
        ret = mm_provider_array[pstat->ptype].provider_stat(pstat);
    } else {
        ret = -1;
    }
    return ret;
}

int MM_delete(MM_delete_resp_t *pdel)
{
    int ret = -1;
    nkn_provider_type_t ptype;

    if(!pdel) {
	return -1;
    }

    glob_mm_deletes ++;

    ptype = pdel->in_ptype;
    if(ptype != Unknown_provider) {
	if(mm_provider_array[ptype].delete != NULL) {
	    pdel->in_sub_ptype = 0;
	    ret = mm_provider_array[ptype].delete(pdel);
	}
    } else {
	/* Remove video from any provider that has it. Don't delete
	   from the origin providers (obviously).*/
	for(ptype = 0; ptype < NKN_MM_origin_providers; ptype++) {
	    if(mm_provider_array[ptype].delete != NULL) {
		pdel->in_ptype = ptype;
		pdel->in_sub_ptype = 0;
		ret = mm_provider_array[ptype].delete(pdel);
	    }
	}
    }
    /* Clean up the video from the bufmgr */
    nkn_buffer_purge(&pdel->in_uol);
    return 0;
}

int
MM_get(MM_get_resp_t *in_ptr_resp)
{
    int                         ret_val = -1;
    nkn_provider_type_t         ptype;
    int32_t                     sub_ptype;
    char                        *uri;
    int                         len;

    glob_mm_gets ++;

    ret_val = nkn_cod_get_cn(in_ptr_resp->in_uol.cod, &uri, &len);
    if(ret_val == NKN_COD_INVALID_COD) {
        in_ptr_resp->err_code = NKN_MM_GET_COD_ERR;
        glob_mm_get_cod_err ++;
        return NKN_MM_GET_COD_ERR;
    }

    if(!in_ptr_resp) {
        glob_mm_get_err ++;
        return NKN_MM_GET_URI_ERR;
    }

    ptype = in_ptr_resp->in_ptype;
    sub_ptype = in_ptr_resp->in_sub_ptype;

#if 0
    ret_val = s_create_am_promote_policy(&in_ptr_resp->in_uol, uri, ptype,
                                         sub_ptype,
					 am_cache_promotion_hotness_threshold,
					 0,
					 am_cache_promotion_hit_threshold, 0);
#endif

    assert((ptype > 0) && (ptype <= NKN_MM_MAX_CACHE_PROVIDERS));

    glob_mm_get_cnt ++;

    if(mm_provider_array[ptype].get != NULL) {
	in_ptr_resp->debug = NKN_MM_DEBUG_CALLED;
        ret_val = mm_provider_array[ptype].get(in_ptr_resp);
    } else {
	glob_mm_get_no_calling_func_err ++;
        return NKN_MM_GET_URI_ERR;
    }

    if(ret_val != 0) {
	/* 
	 * Dont increment the get_err for RETRY_REQUEST from
	 * OM. BM will retry this get again.
	 */
	if(ret_val != NKN_MM_CONF_RETRY_REQUEST &&
		ret_val != NKN_MM_UNCOND_RETRY_REQUEST)
	    glob_mm_get_err ++;
	else
	    glob_mm_get_retry_cnt ++;
    } else {
        glob_mm_get_success ++;
    }

    return 0;
}

#if 0
static uint32_t
s_mm_get_disk_id( nkn_provider_type_t ptype, MM_get_resp_t *get_ptr)
{
    uint32_t disk_id;
    if(ptype < NKN_MM_max_pci_providers) {
	disk_id = nkn_physid_to_device(get_ptr->physid2);
    } else {
	disk_id = NKN_MM_NO_DISK_ID;
    }
    if(disk_id >= mm_provider_array[ptype].num_disks) {
	DBG_LOG(WARNING, MOD_MM, "Invalid disk_id %d for physid %d, "
		"max disks in tier is %d", disk_id, get_ptr->physid2,
		mm_provider_array[ptype].num_disks);
	disk_id = NKN_MM_NO_DISK_ID;
    }
    return disk_id;
}
#endif

void
mm_async_thread_hdl(gpointer data,
                    gpointer user_data __attribute((unused)))
{
    struct nkn_task     *ntask = (struct nkn_task *) data;
    MM_get_resp_t       *in_ptr_resp = NULL;
    MM_put_data_t       *in_mm_data_ptr = NULL;
    nkn_task_id_t       task_id;
    int                 op;
    int                 i, ret;
    nkn_provider_type_t ptype = 0;
    int                 do_not_cleanup = 0;
    MM_validate_t       *pvld;
    int                 mm_op = 0;
    MM_update_attr_t    *pup = NULL;
    MM_stat_resp_t      *resp;
    MM_delete_resp_t    *pdel = NULL;
    int                 trace = 0;
    nkn_cod_t		in_cod = 0;

    assert(ntask);
    op = ntask->dst_module_op;
    if((ntask->task_action == TASK_ACTION_IDLE) ||
       (ntask->task_state == TASK_STATE_NONE)) {
	assert(0);
	return;
    }

    task_id = nkn_task_get_id(ntask);

    if(op == MM_OP_READ) {
	mm_op = MM_OP_READ;
        in_ptr_resp = (MM_get_resp_t *) nkn_task_get_data (task_id);
        assert(in_ptr_resp);

        ptype = in_ptr_resp->in_ptype;
	trace = (in_ptr_resp->in_flags & MM_FLAG_TRACE_REQUEST);
	in_cod = in_ptr_resp->in_uol.cod;

	if (trace)
	    DBG_TRACELOG("MM_GET cod %ld begin - taskid: %ld",
		in_cod, task_id);

	((MM_get_resp_t *)in_ptr_resp)->debug = NKN_MM_DEBUG_INVOK;

        if((ptype < NKN_MM_max_pci_providers) &&
                (mm_provider_array[ptype].flags != MM_THREAD_ACTION_IMMEDIATE)) {
	    AO_fetch_and_add1(&nkn_mm_get_cnt[ptype]);
	}
        if(MM_get((MM_get_resp_t *) in_ptr_resp) == 0) {
	    if(mm_provider_array[ptype].flags == MM_THREAD_ACTION_IMMEDIATE) {
		/* Task context is invalid since it could be freed */
		/* This flag means that the callee will take care of the task
		in its own time. */
		do_not_cleanup = 1;
	    } else {
		if(s_mm_check_task_time(task_id) < 0) {
		    glob_mm_get_task_time_too_long ++;
		}
	    }
	}
        if((ptype < NKN_MM_max_pci_providers) &&
                (mm_provider_array[ptype].flags != MM_THREAD_ACTION_IMMEDIATE)) {
            AO_fetch_and_sub1(&nkn_mm_get_queue_cnt[ptype]);
            AO_fetch_and_sub1(&nkn_mm_get_cnt[ptype]);
        }

	if (trace)
	    DBG_TRACELOG("MM_GET cod %ld end - taskid: %ld",
		in_cod, task_id);

    } else if(op == MM_OP_WRITE) {
	mm_op = MM_OP_WRITE;
        in_mm_data_ptr = (MM_put_data_t *) nkn_task_get_data (task_id);
        assert(in_mm_data_ptr);
        ptype = in_mm_data_ptr->ptype;

	AO_fetch_and_add1(&nkn_mm_tier_put_cnt[ptype]);

	((MM_put_data_t *)in_mm_data_ptr)->debug = NKN_MM_DEBUG_INVOK;
        MM_put((MM_put_data_t *) in_mm_data_ptr);
	if(s_mm_check_task_time(task_id) < 0) {
	    glob_mm_put_task_time_too_long ++;
	}

	AO_fetch_and_sub1(&nkn_mm_tier_put_cnt[ptype]);

    } else if(op == MM_OP_VALIDATE) {
	mm_op = MM_OP_VALIDATE;
        pvld = (MM_validate_t *) nkn_task_get_data(ntask->task_id);
        ptype = pvld->ptype;
	pvld->debug = NKN_MM_DEBUG_INVOK;
        if(MM_validate(pvld) == 0) {
	    if(mm_provider_array[ptype].flags == MM_THREAD_ACTION_IMMEDIATE) {
		/* Task context is invalid since it could be freed */
		/* This flag means that the callee will take care of the task
		in its own time. */
		do_not_cleanup = 1;
	    } else {
		if(s_mm_check_task_time(task_id) < 0) {
		   glob_mm_val_task_time_too_long ++;
		}
	    }
	}
    } else if(op == MM_OP_UPDATE_ATTR) {
        pup = (MM_update_attr_t *) nkn_task_get_data(ntask->task_id);
        if(pup) {
	    pup->debug = NKN_MM_DEBUG_INVOK;
            for(i = NKN_MM_max_pci_providers; i > Unknown_provider; i--) {
                pup->in_ptype = i;
                if(MM_update_attr(pup) >= 0) {
		    glob_mm_update_attr ++;
		}
            }
        }
    } else if(op == MM_OP_STAT) {
        resp = (MM_stat_resp_t *) nkn_task_get_data (ntask->task_id);
        assert(resp);
	if (resp->in_flags & MM_FLAG_TRACE_REQUEST)
	    DBG_TRACELOG("MM_STAT cod %ld  begin - taskid: %ld",
		resp->in_uol.cod, ntask->task_id);
        resp->mm_stat_ret = 0;
	resp->debug = NKN_MM_DEBUG_INVOK;
        ret = MM_stat(resp->in_uol, resp);
	if(s_mm_check_task_time(ntask->task_id) < 0) {
	    glob_mm_get_task_time_too_long ++;
	}
	if (resp->in_flags & MM_FLAG_TRACE_REQUEST)
	    DBG_TRACELOG("MM_STAT cod %ld end - taskid: %ld",
		resp->in_uol.cod,ntask->task_id);
	AO_fetch_and_sub1(&glob_mm_stat_queue_cnt);
    } else if(op == MM_OP_DELETE) {
        pdel = (MM_delete_resp_t *) nkn_task_get_data(ntask->task_id);
	MM_delete(pdel);
    } else {
        assert(0);
    }
     
    if(do_not_cleanup == 0) {
        nkn_task_set_action_and_state(task_id, TASK_ACTION_OUTPUT,
					TASK_STATE_RUNNABLE);
    }
    return;
}



void media_mgr_cleanup(nkn_task_id_t id __attribute((unused)))
{
    return;
}

void
media_mgr_thread_input(nkn_task_id_t id);
/* This function is the entry point for all tasks to the media managers
   and cache providers. It will figure out which provider is the destination
   for this command and will push a io task into the appropriate queue.
   The io task will be serviced by one of a pool of threads. For example,
   the disk manager will have a pool of threads to handle disk io.
*/

void
media_mgr_input(nkn_task_id_t id)
{
	nkn_task_set_state(id, TASK_STATE_EVENT_WAIT);
	media_mgr_thread_input(id);
}


void
media_mgr_thread_input(nkn_task_id_t id)
{
    struct nkn_task     *ntask = nkn_task_get_task(id);
    nkn_provider_type_t  ptype;
    uint8_t              sptype;
    uint16_t             ssptype;
    MM_put_data_t        *put_ptr = NULL;
    MM_get_resp_t        *get_ptr = NULL;
    MM_validate_t        *pvld = NULL;
    MM_stat_resp_t	*pstat= NULL;
    provider_cookie_t    cookie;
    const namespace_config_t *cfg = NULL;
    MM_update_attr_t    *pup;
    int                 disk_id;
    int			limit_read = 1;
    uint32_t            tid = 0, max_tid;

    assert(ntask);
    if(ntask->task_action == TASK_ACTION_IDLE) {
	assert(0);
	return;
    }

    switch (ntask->dst_module_op) {
        case MM_OP_READ:
            get_ptr = (MM_get_resp_t *) nkn_task_get_data(id);

            cookie = get_ptr->in_ptype;
            AM_decode_provider_cookie(cookie, &ptype, &sptype,
                                      &ssptype);

            get_ptr->in_ptype = ptype;
            get_ptr->in_sub_ptype = sptype;
	    get_ptr->get_task_id = id;

	    disk_id = 0;
	    //disk_id = s_mm_get_disk_id(ptype, get_ptr);

           if (get_ptr->in_flags & MM_FLAG_TRACE_REQUEST)
		DBG_TRACELOG("MM_GET bf_queue cod %ld end - taskid: %ld",
			get_ptr->in_uol.cod, id);

           if((ptype < NKN_MM_max_pci_providers) &&
                (mm_provider_array[ptype].flags != MM_THREAD_ACTION_IMMEDIATE)) {
                AO_fetch_and_add1(&nkn_mm_get_queue_cnt[ptype]);
           }
	    if(mm_provider_array[ptype].get_run_queue == 1) {
		get_ptr->debug = NKN_MM_DEBUG_QUEUED;
		if(ptype > NKN_MM_max_pci_providers) {
		    nkn_sched_mm_queue(ntask, limit_read);
		} else {
		    max_tid = nkn_mm_local_provider_get_end_tid[ptype] -
				    nkn_mm_local_provider_get_start_tid[ptype];
		    tid = AO_fetch_and_add1(
					&nkn_mm_local_provider_get_tid[ptype]);
		    if(max_tid == 0) {
			NKN_ASSERT(0);
			tid = 0;
		    } else {
			tid = tid % max_tid;
			tid += nkn_mm_local_provider_get_start_tid[ptype];
		    }
		    nkn_sched_mm_queue(ntask, tid);
		}
	    } else{
		glob_mm_get_run_queue_stopped ++;
		nkn_task_set_action_and_state(id, TASK_ACTION_OUTPUT,
						TASK_STATE_RUNNABLE);
	    }
            break;

        case MM_OP_WRITE:
            put_ptr = (MM_put_data_t *) nkn_task_get_data (id);
            assert(put_ptr);
            if(put_ptr->ptype == 0) {
                /* Provider is not specified for for PUT */
                /* This function is always returning a dummy provider
                   for now. We need to figure out the next pci provider
                   and use it for the put. */
                ptype = MM_get_next_faster_ptype(NKN_MM_max_pci_providers);
                assert(ptype < NKN_MM_max_pci_providers);
                put_ptr->ptype = ptype;
            } else {
                ptype = put_ptr->ptype;
            }

	    if(mm_provider_array[ptype].put_run_queue == 1) {
		put_ptr->debug = NKN_MM_DEBUG_QUEUED;
		max_tid = nkn_mm_local_provider_put_end_tid[ptype] -
				nkn_mm_local_provider_put_start_tid[ptype];
		tid = AO_fetch_and_add1(&nkn_mm_local_provider_put_tid[ptype]);
		if(max_tid == 0) {
		    NKN_ASSERT(0);
		    tid = 0;
		} else {
		    tid = tid % max_tid;
		    tid += nkn_mm_local_provider_put_start_tid[ptype];
		}
		nkn_sched_mm_queue(ntask, tid);
		AO_fetch_and_add1(
			&nkn_mm_tier_put_queue_cnt[ptype]);
	    } else{
		glob_mm_put_run_queue_stopped ++;
		nkn_task_set_action_and_state(id, TASK_ACTION_OUTPUT,
						TASK_STATE_RUNNABLE);
	    }
            break;

        case MM_OP_STAT:
            /* Stat is an async call but all stat calls are
               handled by same thread pool.*/
            ntask->dbg_start_time = nkn_cur_ts;
	    pstat = (MM_stat_resp_t *)nkn_task_get_data(id);
            if (pstat && pstat->in_flags & MM_FLAG_TRACE_REQUEST) {
		DBG_TRACELOG("MM_STAT bf_queue cod %ld end - taskid: %ld",
			pstat->in_uol.cod, id);
	    }
	    pstat->debug = NKN_MM_DEBUG_QUEUED;
	    nkn_sched_mm_queue(ntask, 0);
	    AO_fetch_and_add1(&glob_mm_stat_queue_cnt);
	    AO_fetch_and_add1(&glob_mm_stat_cnt);
            break;

        case MM_OP_VALIDATE:
            pvld = (MM_validate_t *) nkn_task_get_data(id);

            /* Get the provider type from the namespace token */
            cfg = namespace_to_config(pvld->ns_token);
	    if(!cfg) {
		glob_mm_validate_namespace_error++;
		pvld->ret_code = -1;
                nkn_task_set_action_and_state(id, TASK_ACTION_OUTPUT,
						TASK_STATE_RUNNABLE);
		break;
	    }
	    switch(cfg->http_config->origin.select_method) {
	    case OSS_HTTP_CONFIG:
	    case OSS_HTTP_PROXY_CONFIG:
	    case OSS_HTTP_ABS_URL:
	    case OSS_HTTP_FOLLOW_HEADER:
	    case OSS_HTTP_DEST_IP:
	    case OSS_HTTP_SERVER_MAP:
	    case OSS_HTTP_NODE_MAP:
		ptype = OriginMgr_provider;
		break;
	    case OSS_NFS_CONFIG:
	    case OSS_NFS_SERVER_MAP:
		ptype = NFSMgr_provider;
		break;
	    default:
		ptype = Unknown_provider;
	    }
            release_namespace_token_t(pvld->ns_token);

            if(ptype == Unknown_provider) {
                if(pvld) {
                    pvld->ret_code = -1;
                }
                nkn_task_set_action_and_state(id, TASK_ACTION_OUTPUT,
						TASK_STATE_RUNNABLE);
                break;
            }

	    pvld->ptype = ptype;
	    pvld->get_task_id = id;
	    if(mm_provider_array[ptype].get_run_queue == 1) {
		pvld->debug = NKN_MM_DEBUG_QUEUED;
		if(ptype > NKN_MM_max_pci_providers)
		    nkn_sched_mm_queue(ntask, limit_read);
		else
		    nkn_sched_mm_queue(ntask, 0);
	    } else{
		glob_mm_get_run_queue_stopped ++;
		nkn_task_set_action_and_state(id, TASK_ACTION_OUTPUT,
						TASK_STATE_RUNNABLE);
	    }
            break;

        case MM_OP_UPDATE_ATTR:
	    pup = (MM_update_attr_t *) nkn_task_get_data(id);
	    pup->debug = NKN_MM_DEBUG_QUEUED;
	    if(pup->in_utype == MM_UPDATE_TYPE_HOTNESS) {
		nkn_sched_mm_queue(ntask, limit_read);
	    } else {
		nkn_sched_mm_queue(ntask, 0);
	    }
            break;
        case MM_OP_DELETE:
	    nkn_sched_mm_queue(ntask, 0);
            break;

        default:
            assert(0);
    }
}

static int
s_mm_check_task_time(nkn_task_id_t id)
{
    struct nkn_task     *ntask = nkn_task_get_task(id);
    int time_l;
    assert(ntask);

    ntask->dbg_end_time = nkn_cur_ts;

    if((time_l = ntask->dbg_end_time - ntask->dbg_start_time) > 1) {
        DBG_LOG(WARNING, MOD_MM, "Task took more than 1s. "
		"Task id = %d time = %d", (int)id, time_l);
#if 0
	if((ntask->dbg_end_time - ntask->dbg_start_time) > 10) {
	    NKN_ASSERT(0);
	}
#endif
        glob_mm_task_time_too_long ++;
	return -1;
    }
    return 0;
}

void
media_mgr_output(nkn_task_id_t id __attribute((unused)))
{
    return;
}

void
move_mgr_input(nkn_task_id_t id __attribute((unused)))
{
    return;
}

static int64_t
s_get_content_len_from_attr(const nkn_attr_t *attr)
{
    if(!attr) {
        DBG_LOG(WARNING, MOD_MM, "\nAttributes not set "
                "correctly");
        return -1;
    }
    return attr->content_length;
}

static
void s_mm_free_move_mgr_obj(mm_move_mgr_t *pobj)
{
    if(!pobj)
	return;

    if(pobj->abuf)
	nkn_buffer_release(pobj->abuf);

    if(pobj->buf) {
	if (--pobj->bufcnt == 0) {
	    nkn_mm_free_memory(nkn_mm_promote_mem_objp, pobj->buf);
	    pobj->buf = NULL;
	    glob_mm_promote_buf_releases ++;
	}
    }
    if(pobj->in_client_data.proto_data)
	AM_cleanup_client_data(pobj->src_provider,
		&pobj->in_client_data);

    if(pobj->attr) {
	free(pobj->attr);
        pobj->attr = NULL;
    }
    free(pobj);
}

int
mm_uridir_to_nsuuid(const char *uri_dir,
		     char *ns_uuid)
{
    const char *ns, *uid;
    int ret, ns_len, uid_len;

    ret = decompose_ns_uri_dir(uri_dir, &ns, &ns_len,
			       &uid, &uid_len,
			       NULL /* host */, NULL /* host_len */,
			       NULL /* port */, NULL /* port_len */);
    if (ret != 0) {
	DBG_DM2S("[URI_DIR=%s] Decompose of cache name directory failed: %d",
		 uri_dir, ret);
	return -EINVAL;
    }
    if (ns_len + 1 + uid_len + 1 > (int)NKN_MAX_UID_LENGTH) {
	assert(0);
	return -EINVAL;
    }

    strncpy(ns_uuid, ns, ns_len+1+uid_len);
    ns_uuid[ns_len+1+uid_len] = '\0';
    return 0;
}	/* mm_uridir_to_nsuuid */

/* This function calls the respective promote complete function
   of the src provider. NOTE: Remember to release the COD before
   returning.*/
static void
s_mm_promote_complete(nkn_cod_t cod, char *uri,
		      nkn_provider_type_t src_ptype,
		      nkn_provider_type_t dst_ptype,
		      int cleanup, int partial_file_write,
		      void *in_proto_data,
		      int streaming_put,
		      size_t file_size __attribute((unused)),
                      int err_code, int flags,
		      int64_t mmoffset, time_t obj_starttime,
		      time_t block_starttime,
		      time_t clientupdatetime,
		      time_t in_expiry)
{
    MM_promote_data_t       pr_data;
    MM_put_data_t	    mm_put_data;
    MM_delete_resp_t        MM_delete_data;
    char                    ns_uuid[NKN_MAX_UID_LENGTH];
    time_t		    expiry = 0;
    int			    cache_pin = 0;
    int			    ret;
    int			    cim_flags = 0;

    memset(&pr_data, 0, sizeof(pr_data));
    pr_data.in_uol.offset = 0;
    pr_data.in_uol.length = 0;
    pr_data.in_uol.cod    = cod;
    pr_data.in_ptype      = src_ptype;
    pr_data.in_sub_ptype  = 0;
    pr_data.out_ret_code  = err_code;
    pr_data.partial_file_write = partial_file_write;

    AO_fetch_and_sub1(&nkn_mm_tier_ingest_promote_cnt[dst_ptype]);

    if(((cleanup == 1) && (dst_ptype != src_ptype)) ||
	    (streaming_put && !err_code)) {
	glob_mm_promote_partial_file_write_called ++;
	memset(&mm_put_data, 0, sizeof(mm_put_data));
	mm_put_data.uol.cod	    = cod;
	mm_put_data.uol.offset	    = 0;
	mm_put_data.uol.length	    = 0;
	mm_put_data.errcode	    = 0;
	mm_put_data.iov_data_len    = 0;
	mm_put_data.iov_data	    = NULL;
	mm_put_data.domain	    = NULL;
	mm_put_data.total_length    = 0;
	mm_put_data.ptype	    = dst_ptype;
	mm_put_data.flags	    = MM_FLAG_DM2_END_PUT;
	if(mm_provider_array[dst_ptype].put)
	    mm_provider_array[dst_ptype].put(&mm_put_data, NKN_VT_MOVE);
    }
    if((streaming_put && err_code && cleanup) ||
	(err_code == NKN_OM_INV_PARTIAL_OBJ)  ||
	(err_code == NKN_DM2_PARTIAL_EXTENT_EEXIST)) {
	    if(err_code == NKN_DM2_PARTIAL_EXTENT_EEXIST)
		MM_delete_data.dm2_del_reason = NKN_DEL_REASON_OBJECT_PARTIAL;
	    else
		MM_delete_data.dm2_del_reason = NKN_DEL_REASON_STRM_OBJECT_PARTIAL;
	glob_mm_promote_err_delete_called ++;
	MM_delete_data.in_uol.cod       = cod;
	MM_delete_data.in_ptype         = dst_ptype;
	MM_delete_data.evict_flag       = 0;
	MM_delete_data.evict_trans_flag = 0;
	MM_delete_data.evict_hotness    = 0;
	MM_delete_data.out_ret_code     = 0;
	MM_delete(&MM_delete_data);
    }

    if(err_code) {
	int64_t clientstart, clientend;

	nkn_mm_getstartend_clientoffset(cod, &clientstart, &clientend);
	if(nkn_cod_get_status(cod) == NKN_COD_STATUS_STREAMING) {
	    /* If the full object is in ram cache, do not expire the COD
		*/
	    if(!(flags & NKN_MM_FULL_OBJ_IN_CACHE))
		nkn_cod_set_expired(cod);
	    else
		nkn_cod_set_complete(cod);
	}
	AM_set_ingest_error(cod, 0);

	/* Generic Tier specific failure counter */
	AO_fetch_and_add1(&nkn_mm_tier_ingest_failed[dst_ptype]);

	if (glob_log_cache_ingest_failure) {
	    char tempvalue[5][64];
	    if (clientstart == -1) {
		tempvalue[0][0] = '-'; tempvalue[0][1] = '\0';
	    } else {
		snprintf(tempvalue[0], 64, "%ld", clientstart);
	    }
	    if (clientend == -1) {
		tempvalue[1][0] = '-'; tempvalue[1][1] = '\0';
	    } else {
		snprintf(tempvalue[1], 64, "%ld", clientend);
	    }
	    if (clientupdatetime == -1) {
		tempvalue[2][0] = '-'; tempvalue[2][1] = '\0';
	    } else {
		snprintf(tempvalue[2], 64, "%ld", clientupdatetime);
	    }
	    if (obj_starttime == -1) {
		tempvalue[3][0] = '-'; tempvalue[3][1] = '\0';
	    } else {
		snprintf(tempvalue[3], 64, "%ld", obj_starttime);
	    }
	    if (block_starttime == -1) {
		tempvalue[4][0] = '-'; tempvalue[4][1] = '\0';
	    } else {
		snprintf(tempvalue[4], 64, "%ld", block_starttime);
	    }
	    if (err_code != NKN_BUF_NOT_AVAILABLE) {
		DBG_CACHE(" INGESTFAIL \"%s\" %d %s %s %s %s %s %ld %ld %x noageout",
		    uri, err_code, tempvalue[0], tempvalue[1], tempvalue[2],
		    tempvalue[3], tempvalue[4], nkn_cur_ts, mmoffset, flags);
	    } else {
		if (flags & NKN_MM_0_OFFSET_CLI_0_OFFSET ||
		    flags & NKN_MM_MATCH_CLIENT_END) {
		    DBG_CACHE(" INGESTFAIL \"%s\" %d %s %s %s %s %s %ld %ld %x noageout",
			uri, err_code, tempvalue[0], tempvalue[1], tempvalue[2],
			tempvalue[3], tempvalue[4], nkn_cur_ts, mmoffset, flags);
		}
	    }
	}
	/* Tier specific error counter because of DM2 errors */
	if(err_code < ERANGE) {
	    AO_fetch_and_add1(&nkn_mm_tier_ingest_dm2_put_failed[dst_ptype]);
	}

	/* If the error is not recoverable, set that the object cannot
	 * be ingested. TODO - Add cases as applicable */
	switch(err_code) {
	    case NKN_MM_PULL_INGST_OBJ_WITH_HOLE:
		AO_fetch_and_add1(
		    &nkn_mm_tier_ingest_not_cache_worthy[dst_ptype]);
		break;
	    case NKN_DM2_ATTR_TOO_LARGE:
		AM_set_dont_ingest(cod);
		AO_fetch_and_add1(
		    &nkn_mm_tier_ingest_dm2_attr_large[dst_ptype]);
		break;
	    /* Tier specific error counter because of BM errors */
	    case NKN_MM_OBJ_NOT_CACHE_WORTHY:
		AO_fetch_and_add1(
		    &nkn_mm_tier_ingest_not_cache_worthy[dst_ptype]);
		break;
	    case NKN_BUF_NOT_AVAILABLE:
		if (flags & NKN_MM_0_OFFSET_CLI_0_OFFSET) {
		    AO_fetch_and_add1(
			&nkn_mm_tier_ingest_none_buf_not_avail[dst_ptype]);
		} else if (flags & NKN_MM_0_OFFSET_CLI_NONZERO_OFFSET) {
		    AO_fetch_and_add1(
			&nkn_mm_tier_ingest_none_clientbuf_not_avail[dst_ptype]);
		} else if (flags & NKN_MM_MATCH_CLIENT_END) {
		    AO_fetch_and_add1(
			&nkn_mm_tier_ingest_partial_buf_not_avail[dst_ptype]);
		} else if (flags & NKN_MM_NONMATCH_CLIENT_END) {
		    AO_fetch_and_add1(
			&nkn_mm_tier_ingest_partial_clientbuf_not_avail[dst_ptype]);
		} else {
		    AO_fetch_and_add1(
			&nkn_mm_tier_ingest_genbuf_not_avail[dst_ptype]);
		}
		break;
	    case NKN_BUF_VERSION_EXPIRED:
		AO_fetch_and_add1(&nkn_mm_tier_ingest_ver_expired[dst_ptype]);
		break;
	    case NKN_OM_NON_CACHEABLE:
		AO_fetch_and_add1(&nkn_mm_tier_ingest_nocache_expired[dst_ptype]);
		break;
	    case NKN_OM_SERVER_FETCH_ERROR:
		AO_fetch_and_add1(&nkn_mm_tier_ingest_server_fetch[dst_ptype]);
		break;
	    case NKN_DM2_BAD_URI:
		AO_fetch_and_add1(&nkn_mm_tier_ingest_dm2_baduri[dst_ptype]);
		break;
	    case NKN_SERVER_BUSY:
		AO_fetch_and_add1(&nkn_mm_tier_ingest_dm2_preread_busy[dst_ptype]);
		break;
	    default:
		AO_fetch_and_add1(
			&nkn_mm_tier_ingest_unknown_error[dst_ptype]);
		AO_store(&nkn_mm_tier_ingest_unknown_error_val[dst_ptype],
			err_code);
		break;
	}

    }else {
	nkn_cod_set_complete(cod);
	AM_change_obj_provider(cod, dst_ptype);
    }

    ns_uuid[0]='\0';
    mm_uridir_to_nsuuid(uri, ns_uuid);

    ret = cim_get_params(cod, &expiry, &cache_pin,
					&cim_flags);
    if(!ret) {
	if(flags & NKN_MM_UPDATE_CIM)
	    cim_task_complete(cod, ns_uuid,
		    err_code, ICCP_ACTION_ADD, file_size, in_expiry);
	else
	    cim_task_complete(cod, ns_uuid,
		    NKN_MM_CIM_RETRY, ICCP_ACTION_ADD, file_size, in_expiry);
    }


    /* 
     * Wake up AM for trying next ingest/promote for both error 
     * and success case
     */
    AM_ingest_wakeup();

#if 0
    /* Wake up TFM as well, chunked objects ingestion might as well
     * be waiting on a mutex
     */
    TFM_promote_wakeup();
#endif

    if(mm_provider_array[src_ptype].promote_complete
       != NULL) {
	mm_provider_array[src_ptype].promote_complete(&pr_data);
    }

    AO_fetch_and_sub1(&glob_mm_promote_jobs_created);

    if(flags & NKN_MM_HP_QUEUE) {
	AO_fetch_and_add1(&glob_mm_hp_promote_complete);
    } else if(flags & NKN_MM_TIER_SPECIFIC_QUEUE) {
	AO_fetch_and_add1(&nkn_mm_tier_promote_complete[dst_ptype]);
    } else
	AO_fetch_and_add1(&glob_mm_promote_complete);

    if(in_proto_data) {
	nkn_free_proto_data(in_proto_data, src_ptype);
    }
    nkn_cod_clear_ingest_in_progress(cod);
    nkn_cod_close(cod, NKN_COD_MEDIA_MGR);
    return;
}

void
mm_send_promote_debug(char *promote_uri, nkn_provider_type_t src,
		      nkn_provider_type_t dst,
                      int flag)
{
    if(flag == 1) {
        DBG_LOG(MSG, MOD_MM_PROMOTE, "Promote Success: Uri: %s: "
		"from cache %s to cache %s",
		promote_uri, mm_cache_provider_names[src],
		mm_cache_provider_names[dst]);
    }
}

const char*
MM_get_provider_name(nkn_provider_type_t provider)
{
    return (const char *)mm_cache_provider_names[provider];
}

static void
s_mm_copy_move_mgr_struct(mm_move_mgr_thr_t *mm,
			  mm_move_mgr_t     *pobj,
			  nkn_task_id_t     taskid,
			  cache_response_t  *cptr,
			  MM_put_data_t     *pput)
{
    mm->pobj = pobj;
    mm->taskid = taskid;
    mm->cptr = cptr;
    mm->pput = pput;
}

void
move_mgr_output(nkn_task_id_t taskid)
{
    mm_move_mgr_thr_t   *mm   = NULL;
    cache_response_t    *cptr = NULL;
    mm_move_mgr_t       *pobj = NULL;
    MM_put_data_t       *pput = NULL;
    MM_delete_resp_t    *pdelete = NULL;
    char                *uri;
    int                 len, ret_val, cleanup;
    char                ns_uuid[NKN_MAX_UID_LENGTH];
    time_t		expiry = 0;

    nkn_task_set_state(taskid, TASK_STATE_EVENT_WAIT);

    pobj = (mm_move_mgr_t *) nkn_task_get_private
        (TASK_TYPE_MOVE_MANAGER, taskid);
    if(!pobj) {
	glob_mm_promote_pobj_err ++;
	NKN_ASSERT(0);
	nkn_task_set_action_and_state(taskid, TASK_ACTION_CLEANUP,
					TASK_STATE_RUNNABLE);
	return;
    }


    ret_val = nkn_cod_get_cn(pobj->uol.cod, &uri, &len);
    if(ret_val == NKN_COD_INVALID_COD) {
	glob_mm_promote_pobj_cod_err ++;
	nkn_task_set_action_and_state(taskid, TASK_ACTION_CLEANUP,
					TASK_STATE_RUNNABLE);
	return;
    }


    if(pobj->move_op == MM_OP_READ) {
        cptr = (cache_response_t *) nkn_task_get_data(taskid);
	if(!cptr || (pobj->taskid != taskid)) {
	    glob_mm_promote_read_cptr_err ++;
	    NKN_ASSERT(0);
	    cleanup = 0;
	    s_mm_promote_complete(pobj->uol.cod, uri,
				  pobj->src_provider,
				  pobj->dst_provider, cleanup,
				  pobj->partial_file_write,
				  pobj->in_proto_data,
				  pobj->streaming_put,
				  pobj->tot_uri_len,
				  NKN_MM_PROMOTE_GET_ERR,
				  pobj->flags, pobj->uol.offset,
				  pobj->obj_trigger_time,
				  pobj->block_trigger_time,
				  pobj->client_update_time,
				  expiry);
	    s_mm_free_move_mgr_obj(pobj);
            nkn_task_set_action_and_state(taskid, TASK_ACTION_CLEANUP,
					    TASK_STATE_RUNNABLE);
	    return;
	}
    } else if (pobj->move_op == MM_OP_WRITE) {
        pput = nkn_task_get_data(taskid);
	if(!pput) {
	    cleanup = 0;
	    s_mm_promote_complete(pobj->uol.cod, uri,
				  pobj->src_provider,
				  pobj->dst_provider, cleanup,
				  pobj->partial_file_write,
				  pobj->in_proto_data,
				  pobj->streaming_put,
				  pobj->tot_uri_len,
				  NKN_MM_PROMOTE_PUT_ERR,
				  pobj->flags, pobj->uol.offset,
				  pobj->obj_trigger_time,
				  pobj->block_trigger_time,
				  pobj->client_update_time,
				  expiry);
	    s_mm_free_move_mgr_obj(pobj);
	    glob_mm_promote_pobj_write_err ++;
	    NKN_ASSERT(0);
	    nkn_task_set_action_and_state(taskid, TASK_ACTION_CLEANUP,
					    TASK_STATE_RUNNABLE);
	    return;
	}
    } else if (pobj->move_op == MM_OP_DELETE) {
	pdelete = nkn_task_get_data(taskid);
	if(pdelete) {
	    ns_uuid[0]='\0';
	    mm_uridir_to_nsuuid(uri, ns_uuid);
	    if(pobj->flags & NKN_MM_UPDATE_CIM)
		cim_task_complete(pdelete->in_uol.cod, ns_uuid,
				    pdelete->out_ret_code,
				    ICCP_ACTION_DELETE, 0, expiry);
	    s_mm_free_move_mgr_obj(pobj);
	    free(pdelete);
	    nkn_task_set_action_and_state(taskid, TASK_ACTION_CLEANUP,
					    TASK_STATE_RUNNABLE);
	    return;
	}
	s_mm_free_move_mgr_obj(pobj);
	glob_mm_invalid_delete_cnt ++;
	return;
    }

    /* Bug: 3144
     * memory allocation is moved before the memcpy. No need for allocation
     * before the above statements and freeing in case of error
     */
    mm = (mm_move_mgr_thr_t *) nkn_calloc_type (1, sizeof(mm_move_mgr_thr_t),
	    mod_mm_move_mgr_thrd_t);
    if(!mm) {
	glob_mm_promote_pobj_err ++;
	NKN_ASSERT(0);
	nkn_task_set_action_and_state(taskid, TASK_ACTION_CLEANUP,
					TASK_STATE_RUNNABLE);
	return;
    }

    s_mm_copy_move_mgr_struct(mm, pobj, taskid, cptr, pput);

    pthread_mutex_lock(&mm_move_mgr_thr_mutex);
    STAILQ_INSERT_TAIL(&mm_move_mgr_q, mm, entries);
    glob_mm_move_mgr_q_num_obj ++;
    pthread_cond_signal(&mm_move_mgr_thr_cond);
    pthread_mutex_unlock(&mm_move_mgr_thr_mutex);
    return;
}


void *
s_mm_move_mgr_thread_hdl(void *dummy_var __attribute((unused)))
{
    mm_move_mgr_thr_t   *mm   = NULL;
    prctl(PR_SET_NAME, "nvsd-mm-movemgr", 0, 0, 0);
    while(1) {
	pthread_mutex_lock(&mm_move_mgr_thr_mutex);
	mm = STAILQ_FIRST(&mm_move_mgr_q);
	if(mm) {
	    STAILQ_REMOVE_HEAD(&mm_move_mgr_q, entries);
	    glob_mm_move_mgr_q_num_obj --;
	    pthread_mutex_unlock(&mm_move_mgr_thr_mutex);
	    /* Can call this after releasing lock because task is still not freed.
	       This function will free the task appropriately */
	    s_mm_exec_move_mgr_output(mm);
	} else {
	    goto wait;
	}
        continue;
    wait:
	pthread_cond_wait(&mm_move_mgr_thr_cond, &mm_move_mgr_thr_mutex);
	pthread_mutex_unlock(&mm_move_mgr_thr_mutex);
    }
}

static inline
int nkn_mm_ingest_attach(mm_move_mgr_t *pobj)
{
    int    ret;
    char   *uriname;
    int    urilen;
    time_t expiry = 0;
    int    cache_pin = 0;
    int    cim_flags = 0;
    off_t  client_offset = 0;

    /*
     * Donot attach if it is a crawler requests. (PR 772867)
     */
    ret = cim_get_params(pobj->uol.cod, &expiry, &cache_pin, &cim_flags);
    if(!ret) {
	return -1;
    } else {
	ret = nkn_cod_ingest_attach(pobj->uol.cod, pobj, pobj->uol.offset);
	if(!ret) {
	    /* 
	    * revert the ingest counters to allow ingestion for
	    * other objects
	    */
	    if(pobj->flags & NKN_MM_HP_QUEUE) {
		AO_fetch_and_sub1(&glob_mm_hp_promote_started);
	    } else if(pobj->flags & NKN_MM_TIER_SPECIFIC_QUEUE) {
		AO_fetch_and_sub1(&nkn_mm_tier_promote_started[pobj->dst_provider]);
	    } else
		AO_fetch_and_sub1(&glob_mm_promote_started);
	    AO_fetch_and_add1(&glob_mm_ingest_wait);
	} else {
	    ret = nkn_cod_get_cn(pobj->uol.cod, &uriname, &urilen);
	    if(!ret) {
		if(pobj->flags & NKN_MM_HP_QUEUE) {
		    AO_fetch_and_sub1(&glob_mm_hp_promote_started);
		} else if(pobj->flags & NKN_MM_TIER_SPECIFIC_QUEUE) {
			AO_fetch_and_sub1(&nkn_mm_tier_promote_started[pobj->dst_provider]);
		} else
			AO_fetch_and_sub1(&glob_mm_promote_started);
		client_offset = nkn_cod_get_client_offset(pobj->uol.cod);

		/* Increment ingest_wait to match the resume counter */
		AO_fetch_and_add1(&glob_mm_ingest_wait);

		nkn_mm_resume_ingest(pobj, uriname, client_offset,
				     INGEST_CLEANUP_RETRY);

	    }
	}
    }
    return ret;
}

static int
s_mm_check_for_object_in_dm(cache_response_t *cptr, mm_move_mgr_t *pobj,
			    char *uri, int cleanup, nkn_task_id_t taskid)
{
    nkn_uol_t           check_uol;
    MM_stat_resp_t	in_ptr_resp;
    int			ret, i;
    MM_update_attr_t    pup;
    time_t		expiry = 0;
    int			partial = 0;

    expiry = cptr->attr->cache_expiry;

    if(cptr->attr->content_length &&
	cptr->attr->content_length != OM_STAT_SIG_TOT_CONTENT_LEN) {
	check_uol.cod = pobj->uol.cod;
	check_uol.offset = 0;
	check_uol.length = 1;
	memset((char *)&in_ptr_resp, 0, sizeof(MM_stat_resp_t));
	in_ptr_resp.ptype = pobj->dst_provider;
	in_ptr_resp.sub_ptype = 0;
	in_ptr_resp.mm_stat_ret = 0;
	in_ptr_resp.in_flags |= MM_FLAG_IGNORE_EXPIRY;
	ret = mm_provider_array[pobj->dst_provider].stat(check_uol,
							&in_ptr_resp);
	if(!ret && !in_ptr_resp.mm_stat_ret) {
	    partial = 1;
	}
	check_uol.offset = cptr->attr->content_length - 1;
	in_ptr_resp.ptype = pobj->dst_provider;
	in_ptr_resp.sub_ptype = 0;
	in_ptr_resp.mm_stat_ret = 0;
	in_ptr_resp.in_flags |= MM_FLAG_IGNORE_EXPIRY;
	ret = mm_provider_array[pobj->dst_provider].stat(check_uol,
							&in_ptr_resp);
	/* Object is already there in disk, No need to do another put.
	 * (or)
	 * The object is partial, need to update the attribute for 
	 * expiry change.
	 */
	if((!ret && !in_ptr_resp.mm_stat_ret) || (partial == 1)){
	    if(pobj->flags & NKN_MM_CIM_UPDATE_EXPIRY) {
		pup.in_attr = cptr->attr;
		pup.uol = pobj->uol;
		pup.in_utype = MM_UPDATE_TYPE_EXPIRY;
		for(i = NKN_MM_max_pci_providers; i > Unknown_provider; i--) {
		    pup.in_ptype = i;
		    if(MM_update_attr(&pup) >= 0) {
			glob_mm_update_attr ++;
		    }
		}
	    }
	}

	if(!ret && !in_ptr_resp.mm_stat_ret) {
	    glob_mm_old_object_put_avoided++;
	    s_mm_promote_complete(pobj->uol.cod, uri,
				    pobj->src_provider,
				    pobj->dst_provider, cleanup,
				    pobj->partial_file_write,
				    pobj->in_proto_data,
				    pobj->streaming_put,
				    pobj->tot_uri_len,
				    0,
				    pobj->flags, pobj->uol.offset,
				    pobj->obj_trigger_time,
				    pobj->block_trigger_time,
				    pobj->client_update_time,
				    expiry);
	    cr_free(cptr);
	    nkn_task_set_action_and_state(taskid, TASK_ACTION_CLEANUP,
					    TASK_STATE_RUNNABLE);
	    s_mm_free_move_mgr_obj(pobj);
	    return 0;
	} else {
	    glob_mm_new_object_put++;
	}
    }
    return -1;
}

/*
 * Set cache control in the attribute with mime header
 */
static nkn_buffer_t *
nkn_mm_set_attr_cache_control(cache_response_t *cptr,
				uint64_t max_age,
				nkn_provider_type_t provider)
{
    char		ascii_str[64];
    int			ret;
    mime_header_t	mh;
    nkn_attr_t		*new_attr = NULL;
    nkn_buffer_t	*nbuf = NULL;

    nbuf = nkn_buffer_alloc(NKN_BUFFER_ATTR, cptr->attr->total_attrsize, 0);
    if(nbuf == NULL) {
	DBG_LOG(ERROR, MOD_MM, "Attribute buffer allocation failed ");
	glob_mm_attr_buf_alloc_failed++;
	goto end;
    }

    new_attr = nkn_buffer_getcontent(nbuf);

    nkn_attr_docopy(cptr->attr, new_attr, cptr->attr->total_attrsize);

    mime_hdr_init(&mh, MIME_PROT_HTTP, 0, 0);

    snprintf(ascii_str, sizeof(ascii_str)-1, "max-age=%ld", max_age);
    ascii_str[sizeof(ascii_str)-1] = '\0';

    ret = nkn_attr2http_header(new_attr, 0, &mh);
    if (!ret) {
	if (!mime_hdr_is_known_present(&mh, MIME_HDR_CACHE_CONTROL)) {
	    mime_hdr_add_known(&mh, MIME_HDR_CACHE_CONTROL, ascii_str,
				strlen(ascii_str));
	} else {
	    mime_hdr_update_known(&mh, MIME_HDR_CACHE_CONTROL, ascii_str,
				    strlen(ascii_str), 1);
	}
    }

    http_header2nkn_attr(&mh, 0, new_attr);

    mime_hdr_shutdown(&mh);


    cptr->attr = new_attr;

    nkn_buffer_setid(nbuf, &cptr->uol, provider, NKN_BUFFER_UPDATE);

end:;
    return nbuf;
}

/* This function is called when the PROMOTE manager GET or PUT task
   task is done. */
static void
s_mm_exec_move_mgr_output(mm_move_mgr_thr_t *mm)
{
    cache_response_t    *cptr = NULL;
    mm_move_mgr_t       *pobj = NULL;
    uint64_t            tot_ret_len = 0, tot_uri_len;
    uint64_t            req_len = 0;
    uint64_t            len = 0, cp_len = 0;
    MM_put_data_t       *pput = NULL;
    int                 write_to_disk = 0;
    int                 i;
    int                 ret = -1;
    int                 get_task_id;
    uint8_t             get_more_data = 0;
    uint8_t             *content_ptr = NULL;
    uint64_t            attr_tot_len = 0;
    nkn_task_id_t       taskid;
    uint8_t             partial_file_write = 0;
    uint8_t		not_aligned = 0;
    char                *uri;
    int                 dummy_len, cleanup = 0;
    nkn_cod_t           cod;
    void		*in_proto_data = NULL;
    namespace_token_t	ns_token;
    int                 streaming_put = 0;
    int			flag = 0;
    MM_update_attr_t    pup;
    int                 dst_ptype;
    uint64_t            cache_ingest_size_threshold;
    int                 cache_ingest_hotness_threshold;
    time_t              expiry = 0;
    int                 cache_pin = 0;
    size_t              file_size = 0;
    int			cim_flags = 0;
    int			worthy_flag = 0;
    uint64_t            max_age;
    off_t               client_offset = 0;
    nkn_provider_type_t provider;
    int			bytes_to_be_written = 0;

    if(!mm) {
	glob_mm_promote_pobj_err ++;
	NKN_ASSERT(0);
	return;
    }

    taskid = mm->taskid;
    cptr = mm->cptr;
    pobj = mm->pobj;
    pput = mm->pput;

    free(mm);

    pobj = (mm_move_mgr_t *) nkn_task_get_private
        (TASK_TYPE_MOVE_MANAGER, taskid);
    if(!pobj) {
	glob_mm_promote_pobj_err ++;
	NKN_ASSERT(0);
	nkn_task_set_action_and_state(taskid, TASK_ACTION_CLEANUP,
					TASK_STATE_RUNNABLE);
	return;
    }

    if (pobj->attr) {
	expiry = pobj->attr->cache_expiry;
    }

    ret = nkn_cod_get_cn(pobj->uol.cod, &uri, &dummy_len);
    if(ret == NKN_COD_INVALID_COD) {
	glob_mm_promote_pobj_cod_err1 ++;
	nkn_task_set_action_and_state(taskid, TASK_ACTION_CLEANUP,
					TASK_STATE_RUNNABLE);
	return;
    }

    DBG_LOG(MSG, MOD_MM_PROMOTE, "Uri: %s: enter output fn",
            uri);

    /* Mark the object partial in cache, only if it has been 
     * ingested before */
    if (pobj->uol.offset)
	cleanup = 1;
    else
	cleanup = 0;

    if(pobj->move_op == MM_OP_READ) {
        cptr = (cache_response_t *) nkn_task_get_data(taskid);
	if(!cptr || (pobj->taskid != taskid)) {
	    glob_mm_promote_read_cptr_err ++;
	    NKN_ASSERT(0);
	    s_mm_promote_complete(pobj->uol.cod, uri,
				  pobj->src_provider,
				  pobj->dst_provider, cleanup,
				  pobj->partial_file_write,
				  pobj->in_proto_data,
				  pobj->streaming_put,
				  pobj->tot_uri_len,
				  NKN_MM_PROMOTE_GET_ERR,
				  pobj->flags, pobj->uol.offset,
				  pobj->obj_trigger_time,
				  pobj->block_trigger_time,
				  pobj->client_update_time,
				  expiry);
	    s_mm_free_move_mgr_obj(pobj);
            nkn_task_set_action_and_state(taskid, TASK_ACTION_CLEANUP,
					    TASK_STATE_RUNNABLE);
	    return;
	}

	if(cptr->attr) {

	    ns_token = uol_to_nstoken(&pobj->uol);
	    cache_ingest_size_threshold =
			nkn_get_namespace_cache_ingest_size_threshold(ns_token);

	    if((cache_ingest_size_threshold) &&
		(cptr->attr->content_length &&
		 cptr->attr->content_length != OM_STAT_SIG_TOT_CONTENT_LEN &&
		 cptr->attr->content_length <= cache_ingest_size_threshold)) {

		/* This object goes through normal promote process.
		 * Small objects may get promoted directly to highest
		 * cache tier.
		 */

		dst_ptype = MM_get_fastest_put_ptype();
		if((dst_ptype > 0) &&
		    (dst_ptype < (int)NKN_MM_max_pci_providers)) {
		    if(dst_ptype != pobj->dst_provider&&
			    !pobj->bytes_written) {
			if(pobj->flags & NKN_MM_TIER_SPECIFIC_QUEUE) {
			    AO_fetch_and_sub1(
				&nkn_mm_tier_promote_started[pobj->dst_provider]);
			    AO_fetch_and_add1(
				&nkn_mm_tier_promote_started[dst_ptype]);
			}
			AO_fetch_and_sub1(
				&nkn_mm_tier_ingest_promote_cnt[pobj->dst_provider]);
			AO_fetch_and_add1(
				&nkn_mm_tier_ingest_promote_cnt[dst_ptype]);
			pobj->dst_provider = dst_ptype;

			glob_mm_ingest_ptype_changed ++;
		    }
		}
	    }

	    if((pobj->flags & NKN_MM_UPDATE_CIM)) {
		ret = cim_get_params(pobj->uol.cod, &expiry, &cache_pin,
					&cim_flags);
		if(!ret) {
		    provider = nkn_uol_getattribute_provider(&pobj->uol);
		    if(expiry != 0 &&
			((cptr->attr->cache_expiry != expiry) ||
			    (cim_flags & VERSION_EXPIRED) ||
			    (provider >= NKN_MM_max_pci_providers))) {
			pobj->flags |= NKN_MM_CIM_UPDATE_EXPIRY;
			cptr->attr->cache_expiry = expiry;
		    }
		    if(expiry == 0) {
			if (cptr->attr)
			    expiry = cptr->attr->cache_expiry;
		    }

		    if(expiry) {
			max_age = expiry - (cptr->attr->origin_cache_create
					 + cptr->attr->cache_correctedage);

			/* Some browsers may not support max-age greater than
			 * signed integer. So limit the value to the maximum
			 * that a signed int can hold
			 */
			if(max_age > 0x7FFFFFFF)
			    max_age = 0x7FFFFFFF;

			if(pobj->abuf)
			    nkn_buffer_release(pobj->abuf);

			/* Set the cache-control attribute in the mime header. */
			pobj->abuf = nkn_mm_set_attr_cache_control(cptr, max_age,
							    provider);
		    } else {
			DBG_LOG(ERROR, MOD_CIM,
				"Expiry is 0, max-age header not inserted.");
		    }
		}

		if((pobj->flags & NKN_MM_PIN_OBJECT)
			    || cache_pin) {
		    nkn_attr_set_cache_pin(cptr->attr);
		}

		ret = s_mm_check_for_object_in_dm(cptr, pobj, uri, cleanup,
						    taskid);
		if(!ret) {
		    return;
		}
	    }
	    if((pobj->flags & NKN_MM_PIN_OBJECT)
		    || cache_pin) {
		nkn_attr_set_cache_pin(cptr->attr);
	    }

	}


	if(cptr->attr && pobj->attr &&
		pobj->attr->content_length &&
		cptr->attr->content_length &&
		pobj->attr->content_length != cptr->attr->content_length) {
	    pobj->tot_uri_len = cptr->attr->content_length;
	    pobj->rem_uri_len = pobj->tot_uri_len - pobj->bytes_written;
	    free(pobj->attr);
	    pobj->attr = NULL;
	    nkn_mm_dm2_attr_copy(cptr->attr, &pobj->attr,
				 mod_mm_promote_get_attr);
	    pobj->streaming_put = 1;
	    glob_mm_num_eod_on_close++;
	}

	if(cptr->attr) {
	    MM_provider_stat_t  pstat;
	    uint32_t max_attr_size;

	    pstat.ptype = pobj->dst_provider;
	    pstat.sub_ptype = 0;

	    /* Find out the cache threshold for this level*/
	    ret = MM_provider_stat(&pstat);
	    if(!ret) {
		max_attr_size = pstat.max_attr_size;
		if(cptr->attr->total_attrsize > max_attr_size) {
	            s_mm_promote_complete(pobj->uol.cod, uri,
					  pobj->src_provider,
					  pobj->dst_provider, cleanup,
					  pobj->partial_file_write,
					  pobj->in_proto_data,
					  pobj->streaming_put,
					  pobj->tot_uri_len,
					  NKN_DM2_ATTR_TOO_LARGE,
					  pobj->flags, pobj->uol.offset,
					  pobj->obj_trigger_time,
					  pobj->block_trigger_time,
					  pobj->client_update_time,
					  expiry);
		    /* In case of put task creation failure
		     * cleanup the get task
		     */
		    cr_free(cptr);
		    nkn_task_set_action_and_state(taskid, TASK_ACTION_CLEANUP,
						    TASK_STATE_RUNNABLE);
		    /* Action READ is done for this task. Free. */
		    s_mm_free_move_mgr_obj(pobj);
		    return;
		}
	    }
	}

	/* If error code is NKN_BUF_NOT_AVAILABLE, then it might be a
	 * race between the client request and movem request. We shall
	 * retry after 100msec for 10 times before we give up
	 */
        if(cptr->errcode) {
            glob_mm_promote_get_errors ++;
            DBG_LOG(WARNING, MOD_MM_PROMOTE,
		    "Uri: %s err from GET call: %d,"
		    "num errors: %ld", uri,
		    cptr->errcode, glob_mm_promote_get_errors);
	    if(pobj->partial && !pobj->partial_offset)
		cleanup = 0;

	    /* If there is some buffer left, write it to disk
	     */

	    if(pobj->buf && pobj->buflen &&
		     (pobj->tot_uri_len > nkn_mm_block_size[pobj->dst_provider]) &&
		    ((pobj->buflen == nkn_mm_block_size[pobj->dst_provider]) ||
		     (nkn_mm_small_write_size[pobj->dst_provider] &&
		      (pobj->buflen >=
		       nkn_mm_small_write_size[pobj->dst_provider])))) {
		if(pobj->buflen != nkn_mm_block_size[pobj->dst_provider]) {
		    bytes_to_be_written = (pobj->buflen/nkn_mm_small_write_size[pobj->dst_provider]) *
					    nkn_mm_small_write_size[pobj->dst_provider];
		    pobj->buflen = bytes_to_be_written;
		}

		pobj->uol.offset = pobj->partial_offset;
		pobj->partial_offset = 0;
                /* Prepare to send the data to the cache provider */
		cptr->content[0].iov_base = (char *)pobj->buf;
                cptr->content[0].iov_len = pobj->buflen;
                tot_ret_len = cptr->content[0].iov_len;
		pobj->partial = 0;
		pobj->buflen = 0;
		/* Indicate to the providers how many iovecs are
		    valid in this PUT request. */
		cptr->num_nkn_iovecs = 1;
		glob_mm_promote_align_complete ++;
		write_to_disk = 1;
		pobj->bytes_written += tot_ret_len;
		if(!pobj->streaming)
		    pobj->rem_uri_len = pobj->tot_uri_len - pobj->bytes_written;

		ret = s_put_data_helper(pobj->uol.cod, uri,
					pobj->uol.offset,
					tot_ret_len, pobj->tot_uri_len,
					pobj->src_provider,
					pobj->dst_provider,
					cptr, taskid, pobj->rem_uri_len,
					pobj, pobj->partial_file_write,
					pobj->in_proto_data,
					&pobj->in_client_data);
                if(ret < 0) {
	            s_mm_promote_complete(pobj->uol.cod, uri,
					  pobj->src_provider,
					  pobj->dst_provider, cleanup,
					  pobj->partial_file_write,
					  pobj->in_proto_data,
					  pobj->streaming_put,
					  pobj->tot_uri_len,
					  NKN_MM_PROMOTE_BUFLEFT_PUTERR,
					  pobj->flags, pobj->uol.offset,
					  pobj->obj_trigger_time,
					  pobj->block_trigger_time,
					  pobj->client_update_time,
					  expiry);
		    /* In case of put task creation failure
		     * cleanup the get task
		     */
		    cr_free(cptr);
		    nkn_task_set_action_and_state(taskid, TASK_ACTION_CLEANUP,
						    TASK_STATE_RUNNABLE);
                } else {
		    /* Need to increment this only if task creation succeeds */
		    pobj->bufcnt++;
		}
		/* Action READ is done for this task. Free. */
		s_mm_free_move_mgr_obj(pobj);
		/* Task cleanup is done when PUT is finished */
	    } else {
		if(cptr->errcode == NKN_BUF_NOT_AVAILABLE) {
		    if (!(pobj->flags & NKN_MM_INGEST_ABORT)) {
			if(pobj->buf && pobj->buflen) {
			    pobj->uol.offset = pobj->partial_offset;
			    pobj->partial_offset = 0;
			    pobj->buflen = 0;
			    nkn_mm_free_memory(nkn_mm_promote_mem_objp, pobj->buf);
			    pobj->buf = NULL;
			    pobj->bufcnt = 0;
			    pobj->partial = 0;
			    glob_mm_promote_buf_releases ++;
			}
			if(!nkn_mm_ingest_attach(pobj)) {
			    cr_free(cptr);
			    nkn_task_set_action_and_state(taskid,
							TASK_ACTION_CLEANUP,
							TASK_STATE_RUNNABLE);
			    return;
			}
		    }
		    int64_t startoffset, endoffset;
		    if (!nkn_mm_getstartend_clientoffset(pobj->uol.cod,
			&startoffset, &endoffset))
			update_mm_ingeststat_flags(pobj, cptr->uol.offset, startoffset, endoffset);
		}

		if(pobj->buf && pobj->buflen)
		    pobj->uol.offset = pobj->partial_offset;
		if(pobj->uol.offset != 0)
		    nkn_uol_setattribute_provider(&pobj->uol, pobj->dst_provider);
		glob_mm_partial_obj_ingest_failed ++;

		s_mm_promote_complete(pobj->uol.cod, uri,
				  pobj->src_provider,
				  pobj->dst_provider, cleanup,
				  pobj->partial_file_write,
				  pobj->in_proto_data,
				  pobj->streaming_put,
				  pobj->tot_uri_len,
				  cptr->errcode,
				  pobj->flags,
				  cptr->uol.offset,
				  pobj->obj_trigger_time,
				  pobj->block_trigger_time,
				  pobj->client_update_time,
				  expiry);
		s_mm_free_move_mgr_obj(pobj);
		cr_free(cptr);
		nkn_task_set_action_and_state(taskid, TASK_ACTION_CLEANUP,
						TASK_STATE_RUNNABLE);
	    }
            return;
	} else {
	    pobj->retry_count = 0;
	    client_offset = nkn_cod_get_client_offset(pobj->uol.cod);
	    if(pobj->client_offset != client_offset)
		pobj->flags &= ~NKN_MM_INGEST_ABORT;
	}


	/* Store the attribute pointer to send to the providers
	   whenever we have aligned data to send and offset = 0 */
	if(cptr->attr && !pobj->tot_uri_len) {
	    if(pobj->attr) {
		free(pobj->attr);
		pobj->attr = NULL;
	    }

	    /* If the attribute hotness is less than the hotness 
	     * value given by AM. Use the AM hotness value
	     * The hotness is a 64 bit value, which has the last
	     * updated time in the upper 32bits and hotness in the
	     * lower 32bits. A direct comparision will check for 
	     * both the recent update time as well the highest hotness.
	     */
	    if(pobj->in_client_data.obj_hotness > cptr->attr->hotval)
		    cptr->attr->hotval = pobj->in_client_data.obj_hotness;

	    ret = nkn_mm_dm2_attr_copy(cptr->attr, &pobj->attr,
				       mod_mm_promote_get_attr);

	    if ((ret != 0)) {
		glob_mm_promote_attr_alloc_err ++;
		s_mm_promote_complete(pobj->uol.cod, uri,
				      pobj->src_provider,
				      pobj->dst_provider, cleanup,
				      pobj->partial_file_write,
				      pobj->in_proto_data,
				      pobj->streaming_put,
				      pobj->tot_uri_len,
				      NKN_MM_PROMOTE_DM2_ATTRCOPY_ERR,
				      pobj->flags, pobj->uol.offset,
				      pobj->obj_trigger_time,
				      pobj->block_trigger_time,
				      pobj->client_update_time,
				      expiry);
		s_mm_free_move_mgr_obj(pobj);
		cr_free(cptr);
		nkn_task_set_action_and_state(taskid, TASK_ACTION_CLEANUP,
						TASK_STATE_RUNNABLE);
		return;
	    }

	    if(cptr->num_nkn_iovecs == 0)
		get_more_data = 1;

            /* First time encountering this URI. Read the attributes for
               the content length */
	    if(cptr->attr->cache_expiry <= (nkn_cur_ts + 60) ) {
		/* Object will be expired in the next 60 seconds,
		   don't promote it.*/
		glob_mm_promote_obj_nearly_expired_err ++;
	    }

            attr_tot_len = s_get_content_len_from_attr(cptr->attr);
            if((attr_tot_len <= 0) &&
		    !nkn_attr_is_streaming(cptr->attr)) {
                DBG_LOG(WARNING, MOD_MM_PROMOTE, "Uri: %s Error "
			"Attribute: len: %ld",
			uri, attr_tot_len);

		s_mm_promote_complete(pobj->uol.cod, uri,
				      pobj->src_provider,
				      pobj->dst_provider, cleanup,
				      pobj->partial_file_write,
				      pobj->in_proto_data,
				      pobj->streaming_put,
				      pobj->tot_uri_len,
				      NKN_MM_PROMOTE_NEGCONTLEN_ERR,
				      pobj->flags, pobj->uol.offset,
				      pobj->obj_trigger_time,
				      pobj->block_trigger_time,
				      pobj->client_update_time,
				      expiry);
		s_mm_free_move_mgr_obj(pobj);
                cr_free(cptr);
                nkn_task_set_action_and_state(taskid,
                                    TASK_ACTION_CLEANUP,
				    TASK_STATE_RUNNABLE);
                return;
            }

	    if(!pobj->streaming) {
		DBG_LOG(MSG, MOD_MM_PROMOTE, "First access to URI: %s",
			uri);
		pobj->buf = NULL;
		pobj->buflen = 0;
		pobj->partial = 0;
	    }
	    /* We got the content length now and we can use it
	     */
	    if(pobj->streaming && !nkn_attr_is_streaming(cptr->attr)) {
		pobj->streaming = 0;
		pobj->tot_uri_len = attr_tot_len;
		NKN_ASSERT(pobj->bytes_written <= attr_tot_len);
		if(pobj->bytes_written <= attr_tot_len)
		    pobj->rem_uri_len = attr_tot_len - pobj->bytes_written;
	    } else {
		if(nkn_attr_is_streaming(cptr->attr)) {
		    pobj->tot_uri_len = 0;
		    pobj->rem_uri_len = 0;
		    pobj->streaming = 1;
		    pobj->streaming_put = 1;
		} else {
		    pobj->tot_uri_len = attr_tot_len;
		    pobj->rem_uri_len = attr_tot_len;
		}
	    }

	}

	/* Do the attr expiry check for every put. This is to
	 * avoid getting error in DM2_put because of object
	 * expiry
	 */
	if(pobj->attr) {
	    ns_token = uol_to_nstoken(&pobj->uol);
	    if(!VALID_NS_TOKEN(ns_token)) {
		DBG_LOG(WARNING, MOD_MM_PROMOTE, "Token is 0."
			"Caused by a malformed "
			"offset = %ld, length = %ld",
			pobj->uol.offset, pobj->uol.length);
		s_mm_promote_complete(pobj->uol.cod, uri,
				      pobj->src_provider,
				      pobj->dst_provider, cleanup,
				      pobj->partial_file_write,
				      pobj->in_proto_data,
				      pobj->streaming_put,
				      pobj->tot_uri_len,
				      NKN_MM_TOKEN_ERR,
				      pobj->flags, pobj->uol.offset,
				      pobj->obj_trigger_time,
				      pobj->block_trigger_time,
				      pobj->client_update_time,
				      expiry);
		s_mm_free_move_mgr_obj(pobj);
                cr_free(cptr);
                nkn_task_set_action_and_state(taskid, TASK_ACTION_CLEANUP,
						TASK_STATE_RUNNABLE);
                return;
	    }
	    if (cptr->provider != RTSPMgr_provider) {
		    if(pobj->flags & NKN_MM_UPDATE_CIM)
			worthy_flag = NAMESPACE_WORTHY_CHECK_INGNORE_SIZE;
		    else
			worthy_flag = 0;
		    ret = nkn_is_object_cache_worthy(pobj->attr,
			    ns_token, worthy_flag);
	            if(ret) {
			glob_mm_object_cache_worthy_failed ++;
			if(pobj->uol.offset) {
			    glob_mm_partial_expired_delete_cnt ++;
			}
			/* Check if the full object is in RAM
		 	*/
			if(pobj->tot_uri_len &&
				(pobj->bytes_written + cptr->length_response ==
					pobj->tot_uri_len))
			    flag = NKN_MM_FULL_OBJ_IN_CACHE;
			s_mm_promote_complete(pobj->uol.cod, uri,
					      pobj->src_provider,
					      pobj->dst_provider, cleanup,
					      pobj->partial_file_write,
					      pobj->in_proto_data,
					      pobj->streaming_put,
				              pobj->tot_uri_len,
					      ret, flag | pobj->flags, 
					      pobj->uol.offset,
					      pobj->obj_trigger_time,
					      pobj->block_trigger_time,
					      pobj->client_update_time,
					      expiry);
			s_mm_free_move_mgr_obj(pobj);
	                cr_free(cptr);
	                nkn_task_set_action_and_state(taskid, TASK_ACTION_CLEANUP,
							TASK_STATE_RUNNABLE);
	                return;
	            }
	    }
	}

	/* Check for alignment issues */
        for(i = 0; i < cptr->num_nkn_iovecs; i++) {
	    if(((uint64_t)cptr->content[i].iov_base & (4096-1)) != 0) {
		not_aligned = 1;
		break;
	    }
	}

	if(!pobj->partial && (cptr->length_response >
			(int32_t)nkn_mm_block_size[pobj->dst_provider])) {
	    cptr->length_response = (int32_t)nkn_mm_block_size[pobj->dst_provider];
	}

        for(i = 0; i < cptr->num_nkn_iovecs; i++) {
            len = cptr->content[i].iov_len;
	    assert(len <= CM_MEM_PAGE_SIZE);
            assert(len != 0);
	    //assert(pobj->tot_uri_len != 0);
	    /* If the iovec's are aligned and we have enough
	     * data, write to disk
	     */
	    if( (not_aligned == 0) &&
		(((pobj->tot_uri_len <=
			nkn_mm_block_size[pobj->dst_provider]) &&
		((int)pobj->tot_uri_len == cptr->length_response)) ||
		(pobj->rem_uri_len &&
		(int64_t)pobj->rem_uri_len == cptr->length_response) ||
		(cptr->length_response ==
			(int32_t)nkn_mm_block_size[pobj->dst_provider]))) {
		if(!pobj->partial) {
		    tot_ret_len = cptr->length_response;
		    write_to_disk = 1;
		    break;
		}
	    }

	    /* This is the case either the iovec's are not aligned
	     * or we dont have all the 2MB. So we copy it to the
	     * temp buffer and request for more, untill we have
	     * all 2MB
	     */
            DBG_LOG(MSG, MOD_MM_PROMOTE, "Uri: %s partial file entry:"
                    "offset: %ld, len: %ld, partial:%d, "
                    "iov_base:%p, len:%ld, "
                    "rem_uri_len:%ld ",
		    uri, pobj->uol.offset,
                    pobj->uol.length, pobj->partial,
                    cptr->content[i].iov_base, len, pobj->rem_uri_len);
	    if(!pobj->buf) {
		/* This is a temp buffer that will will last until the
		 * len is aligned to 2MB or data is finished*/
		/* 
		 * Try to use memory from our memory pool, instead of 
		 * using system memory to avoid holes
		 */
		pobj->buf = nkn_mm_get_memory(nkn_mm_promote_mem_objp);
		if (pobj->buf == NULL) {
		    glob_mm_promote_buf_alloc_err ++;
		    if(pobj->flags & NKN_MM_UPDATE_CIM) {
			s_mm_promote_complete(pobj->uol.cod, uri,
					    pobj->src_provider,
					    pobj->dst_provider, cleanup,
					    pobj->partial_file_write,
					    pobj->in_proto_data,
					    pobj->streaming_put,
				            pobj->tot_uri_len,
					    NKN_MM_CIM_RETRY,
					    pobj->flags,
					    pobj->uol.offset,
					    pobj->obj_trigger_time,
					    pobj->block_trigger_time,
					    pobj->client_update_time,
					    expiry);
			s_mm_free_move_mgr_obj(pobj);
		    } else {
			if(nkn_mm_ingest_attach(pobj)) {
			    s_mm_promote_complete(pobj->uol.cod, uri,
					    pobj->src_provider,
					    pobj->dst_provider, cleanup,
					    pobj->partial_file_write,
					    pobj->in_proto_data,
					    pobj->streaming_put,
				            pobj->tot_uri_len,
					    NKN_MM_PROMOTE_NOBUF_ATTACH_ERR,
					    pobj->flags,
					    pobj->uol.offset,
					    pobj->obj_trigger_time,
					    pobj->block_trigger_time,
					    pobj->client_update_time,
					    expiry);
			    s_mm_free_move_mgr_obj(pobj);
			}
		    }
		    cr_free(cptr);
		    nkn_task_set_action_and_state(taskid, TASK_ACTION_CLEANUP,
						    TASK_STATE_RUNNABLE);
		    return;
		}
		glob_mm_promote_buf_allocs ++;
		pobj->buflen = 0;
		pobj->partial_offset = 0;
		pobj->bufcnt = 1;
		/* How bufcnt should work:
		 * The previous code used to utilize BM buffers which
		 * would provide an external refcnt mechanism.  The current
		 * code attempts to duplicate that functionality.  All the
		 * helper functions make copies of the buffer, so they
		 * also make copies of the refcnt field (called bufcnt).
		 * To ensure that the posix_memalign buffer gets freed
		 * properly, we increment bufcnt after calling any of the
		 * helper functions (copy 1) and set bufcnt to 1 in the
		 * helper function (copy 2).
		 */
	    }
	    /* Keep track of the offset before the unaligned access
		request came so that when we get everything aligned,
		we can set the correct offset to send to DM2. */
	    if(pobj->partial == 0) {
		pobj->partial_offset = pobj->uol.offset;
		assert( (pobj->partial_offset % 32768) == 0);
	    }
	    glob_mm_promote_unaligned_mem_cnt ++;

	    if(pobj->buflen > nkn_mm_block_size[pobj->dst_provider])
		pobj->buflen = nkn_mm_block_size[pobj->dst_provider];

	    cp_len = nkn_mm_block_size[pobj->dst_provider] - pobj->buflen;

	    if(cptr->content[i].iov_len < (int)cp_len) {
		cp_len = cptr->content[i].iov_len;
	    }
            /* We use the aligned buffers from bufmgr */
	    content_ptr = pobj->buf;
	    assert(content_ptr);
	    memcpy((content_ptr + pobj->buflen),
		    cptr->content[i].iov_base, cp_len);
	    pobj->partial = 1;
            /* Need to keep track of where we left off in the scratch buf*/
	    pobj->buflen += cp_len;
	    /* Change the UOL offset as we might request more partial data
	     */
	    pobj->uol.offset += cp_len;
	    assert(pobj->buflen <= nkn_mm_block_size[pobj->dst_provider]);
            /* We have received enough data to complete the uri */
	    if((pobj->buflen == nkn_mm_block_size[pobj->dst_provider]) ||
		(!pobj->streaming && (pobj->rem_uri_len - cp_len) == 0) ||
		(!pobj->streaming && (pobj->rem_uri_len - pobj->buflen) == 0)) {
		/* When we are sending data that has been collected in
		    the tmp buffer, we need to adjust the offset
		    to include the whole buffer. The offset was changed
		    earlier to enable the get of more data for alignement.
		*/
		pobj->uol.offset = pobj->partial_offset;
		pobj->partial_offset = 0;
                /* Prepare to send the data to the cache provider */
		cptr->content[0].iov_base = (char *)content_ptr;
                cptr->content[0].iov_len = pobj->buflen;
                tot_ret_len = cptr->content[0].iov_len;
		pobj->partial = 0;
		pobj->buflen = 0;
		/* Indicate to the providers how many iovecs are
		    valid in this PUT request. */
		cptr->num_nkn_iovecs = 1;
		glob_mm_promote_align_complete ++;
		write_to_disk = 1;
		break;
	    }
	}

	/* If the buffer is incomplete, set get_more_data
	 */
	if(!write_to_disk) {
	    get_more_data = 1;
	}

        DBG_LOG(MSG, MOD_MM_PROMOTE, "Calculate remainder: "
		"pobj->rem_uri_len=%ld "
		"tot_ret_len = %ld", pobj->rem_uri_len, tot_ret_len);
	if(!pobj->streaming) {
	    assert(tot_ret_len <= pobj->tot_uri_len);
	    if(pobj->rem_uri_len == 0) {
		pobj->rem_uri_len = pobj->tot_uri_len - tot_ret_len;
	    } else {
		if(tot_ret_len > pobj->rem_uri_len) {
		    assert(0); 
		} else {
		    pobj->rem_uri_len = pobj->rem_uri_len - tot_ret_len;
		}
	    }
	}

	/* In case of aligned data, fill in the req_len based on 
	 * remaining length if less than 2MB, or the max of 2MB
	 */
	if(pobj->streaming ||
		(pobj->rem_uri_len > nkn_mm_block_size[pobj->dst_provider]))
	    req_len = nkn_mm_block_size[pobj->dst_provider] - pobj->buflen;
	else
	    req_len = pobj->rem_uri_len - pobj->buflen;

	if(get_more_data == 0) {
	    /* Send out any aligned data buffers. Keep track of it
	     using the iov len that is sent to the DM2_put routine.*/
            glob_mm_promote_puts ++;
	    if(cptr->num_nkn_iovecs == 0) {
		glob_mm_promote_get_num_iov_zero ++;
	    } else {
		DBG_LOG(MSG, MOD_MM_PROMOTE, "Uri: %s calling PUT "
			"offset = %ld, total_ret_len = %ld, "
			"tot_uri_len = %ld, pobj->buf_len = %ld "
			"partial = %d, req_len = %ld",
			uri, pobj->uol.offset, tot_ret_len,
			pobj->tot_uri_len, pobj->buflen, pobj->partial, req_len);

		pobj->bytes_written += tot_ret_len;
		ret = s_put_data_helper(pobj->uol.cod, uri,
					pobj->uol.offset,
					tot_ret_len, pobj->tot_uri_len,
					pobj->src_provider,
					pobj->dst_provider,
					cptr, taskid, pobj->rem_uri_len,
					pobj, pobj->partial_file_write,
					pobj->in_proto_data,
					&pobj->in_client_data);
                if(ret < 0) {
	            s_mm_promote_complete(pobj->uol.cod, uri,
					  pobj->src_provider,
					  pobj->dst_provider, cleanup,
					  pobj->partial_file_write,
					  pobj->in_proto_data,
					  pobj->streaming_put,
				          pobj->tot_uri_len,
					  NKN_MM_PROMOTE_MOREPUT_ERR,
					  pobj->flags,
					  pobj->uol.offset,
					  pobj->obj_trigger_time,
					  pobj->block_trigger_time,
					  pobj->client_update_time,
					  expiry);
		    /* In case of put task creation failure
		     * cleanup the get task
		     */
		    cr_free(cptr);
		    nkn_task_set_action_and_state(taskid, TASK_ACTION_CLEANUP,
						    TASK_STATE_RUNNABLE);
                } else {
		    /* Need to increment this only if task creation succeeds */
		    pobj->bufcnt++;
		}
	    }
	} else {
	    /* Get more data to fill up the buffer */
            DBG_LOG(MSG, MOD_MM_PROMOTE, "Calling Get helper: Uri: %s "
                    "get unaligned data: pobj->uol.offset = %ld,"
		    " rem_uri_len = %ld, tot_uri_len = %ld, "
		    "pobj->buf_len = %ld partial = %d req_len = %ld", uri,
		    pobj->uol.offset, pobj->rem_uri_len,
		    pobj->tot_uri_len, pobj->buflen, pobj->partial, req_len);

	    glob_mm_promote_get_more_data_unaligned ++;
	    /* NOTE:
	       1. nkn_attr_t: Copy the attribute pointer
	       because on prior PUTs may fail with EEXIST
	       and if we are filling up the holes in the data
	       we need to have the attributes to send to the
	       cache provider.
	    */
	    ret = s_get_data_helper(pobj->uol.cod, uri,
			      pobj->uol.offset,
			      req_len,
			      pobj->rem_uri_len,
			      pobj->tot_uri_len,
			      pobj->src_provider,
			      pobj->dst_provider,
			      pobj, pobj->partial_file_write,
			      pobj->in_proto_data,
			      &pobj->in_client_data, 
			      pobj->flags, 0);
            if(ret < 0) {
	        s_mm_promote_complete(pobj->uol.cod, uri,
				      pobj->src_provider,
				      pobj->dst_provider, cleanup,
				      pobj->partial_file_write,
				      pobj->in_proto_data,
				      pobj->streaming_put,
				      pobj->tot_uri_len,
				      NKN_MM_PROMOTE_GET_ERR1,
				      pobj->flags, pobj->uol.offset,
				      pobj->obj_trigger_time,
				      pobj->block_trigger_time,
				      pobj->client_update_time,
				      expiry);
            } else {
		/* Need to increment this only if task creation succeeds */
		pobj->bufcnt++;
	    }
            cr_free(cptr);
            nkn_task_set_action_and_state(taskid, TASK_ACTION_CLEANUP,
					    TASK_STATE_RUNNABLE);
	}

	/* Action READ is done for this task. Free. */
	s_mm_free_move_mgr_obj(pobj);
        /* Task cleanup is done when PUT is finished */
        return;
    }
    else if (pobj->move_op == MM_OP_WRITE) {
        nkn_provider_type_t     src_provider;
        nkn_provider_type_t     dst_provider;
        char                    *promote_uri;
        int                     end = 0;
	int                     errcode;

        pput = nkn_task_get_data(taskid);
        assert(pput);
	AO_fetch_and_add(&mm_glob_bytes_used_in_put[pobj->dst_provider],
			-pobj->uol.length);

        DBG_LOG(MSG, MOD_MM_PROMOTE, "Uri: %s PUT task output",
		uri);

	AO_fetch_and_sub1(
		    &nkn_mm_tier_put_queue_cnt[pobj->dst_provider]);

        if(((pput->errcode == 0) || (pput->errcode == EEXIST)) &&
	    !(pobj->cptr->errcode)) {

            /* Only do the next get when the last put succeeds */
            if(pobj->streaming || pobj->rem_uri_len > 0) {
		if(pobj->streaming) {
		    pobj->uol.offset += pobj->uol.length;
		    req_len = nkn_mm_block_size[pobj->dst_provider];
		} else {
		    pobj->uol.offset = pobj->tot_uri_len - pobj->rem_uri_len;
		    if(pobj->rem_uri_len > nkn_mm_block_size[pobj->dst_provider])
			req_len = nkn_mm_block_size[pobj->dst_provider];
		    else
			req_len = pobj->rem_uri_len;
		}
                /* Initiate another GET task for the rest of the data */
                DBG_LOG(MSG, MOD_MM_PROMOTE, "Uri: %saligned get more data:"
			"offset = %ld, rem_uri_len = %ld, "
			"tot_uri_len = %ld, pobj->buf_len = %ld partial = %d",
			uri, pobj->uol.offset, pobj->rem_uri_len,
			pobj->tot_uri_len, pobj->buflen, pobj->partial);

	        glob_mm_promote_get_more_data_aligned ++;
		int timediff =  nkn_cur_ts - pobj->block_trigger_time;
		if (timediff >= 0) {
		    switch(timediff) {
			case 0 ... 59:
				mm_block_triggertime_hist[timediff/2]++;
				break;
			case 60 ... 359:
				mm_block_triggertime_histwide[timediff/30]++;
				break;
			default:
				mm_block_triggertime_histwide[0]++;
		    }
		}
		pobj->block_trigger_time = nkn_cur_ts;
                ret = s_get_data_helper(pobj->uol.cod, uri,
                                  pobj->uol.offset,
				  req_len,
                                  pobj->rem_uri_len,
                                  pobj->tot_uri_len,
                                  pobj->src_provider,
                                  pobj->dst_provider,
				  pobj, pobj->partial_file_write,
				  pobj->in_proto_data,
				  &pobj->in_client_data,
				  pobj->flags, 0);
                if(ret < 0) {
	            s_mm_promote_complete(pobj->uol.cod, uri,
					  pobj->src_provider,
					  pobj->dst_provider, cleanup,
					  pobj->partial_file_write,
					  pobj->in_proto_data,
					  pobj->streaming_put,
				          pobj->tot_uri_len,
					  NKN_MM_PROMOTE_GET_ERR2,
					  pobj->flags, pobj->uol.offset,
					  pobj->obj_trigger_time,
					  pobj->block_trigger_time,
					  pobj->client_update_time,
					  expiry);
                } else {
		    /* Need to increment this only if task creation succeeds */
		    pobj->bufcnt++;
		}
            } else {
                /* All the data has been moved. Change the
                   provider in bufmgr because this is how
                   bufmgr keeps track of where the data resides
                */
                end = 1;
                DBG_LOG(MSG, MOD_MM_PROMOTE, "Uri: %s change uol provider for "
                                     "dst_provider: %d",
                                     uri, pobj->dst_provider);
                nkn_uol_setattribute_provider(&pobj->uol, pobj->dst_provider);
            }
        } else {
	    if(pput->errcode) {
		glob_mm_promote_put_errors ++;
		errcode = pput->errcode;
	    } else {
		cleanup = 1;
	    }
	    if(pobj->cptr->errcode) {
		errcode = pobj->cptr->errcode;
		if((pput->errcode == 0) || (pput->errcode == EEXIST))
			nkn_uol_setattribute_provider(&pobj->uol, pobj->dst_provider);
	    }
	    if((pobj->cptr->errcode == NKN_BUF_NOT_AVAILABLE) && (!pput->errcode)){
		if (!(pobj->flags & NKN_MM_INGEST_ABORT)) {

		    /* Update the offset and rem_uri_len */
		    pobj->uol.offset += pobj->uol.length;

		    if(pobj->buf) {
			nkn_mm_free_memory(nkn_mm_promote_mem_objp, pobj->buf);
			pobj->buf = NULL;
			pobj->bufcnt = 0;
			glob_mm_promote_buf_releases ++;
		    }

		    /* 
		     * revert the ingest counters to allow ingestion for
		     * other objects
		     */
		    if(!nkn_mm_ingest_attach(pobj)) {
			cr_free(pobj->cptr);
			nkn_task_set_action_and_state(pobj->get_task_id,
						    TASK_ACTION_CLEANUP,
						    TASK_STATE_RUNNABLE);
			goto put_cleanup;
		    }
		}
		int64_t startoffset, endoffset;
		if (!nkn_mm_getstartend_clientoffset(pobj->uol.cod,
		    &startoffset, &endoffset))
		    update_mm_ingeststat_flags(pobj, pobj->cptr->uol.offset,
					       startoffset, endoffset);
	    }

            DBG_LOG(WARNING, MOD_MM_PROMOTE, "Uri: %s Error from "
                                 "PUT call: %d,"
                                 "num errors: %ld", uri,
                                 errcode, glob_mm_promote_put_errors);
	    /* Check if the full object is in RAM
		*/
	    if(pobj->bytes_written &&
		    pobj->bytes_written == pobj->tot_uri_len)
		flag = NKN_MM_FULL_OBJ_IN_CACHE;

	    s_mm_promote_complete(pobj->uol.cod, uri,
				  pobj->src_provider,
				  pobj->dst_provider, cleanup,
				  pobj->partial_file_write,
				  pobj->in_proto_data,
				  pobj->streaming_put,
				  pobj->tot_uri_len,
				  errcode, flag | pobj->flags,
				  pobj->cptr->errcode? pobj->cptr->uol.offset: pobj->uol.offset,
				  pobj->obj_trigger_time,
				  pobj->block_trigger_time,
				  pobj->client_update_time,
				  expiry);
        }

	cod = pobj->uol.cod;
	flag = pobj->flags;
	in_proto_data = pobj->in_proto_data;

	ns_token = uol_to_nstoken(&pobj->uol);
	cache_ingest_size_threshold =
			nkn_get_namespace_cache_ingest_size_threshold(ns_token);
	cache_ingest_hotness_threshold =
			nkn_get_namespace_cache_ingest_hotness_threshold(
								    ns_token);

        /* Action WRITE is done for this task. Free. */
        assert(pobj->get_task_id >= 0);

        /* Now free the get task related to this PUT task*/
        get_task_id = pobj->get_task_id;
        cptr = pobj->cptr;
	file_size = pobj->tot_uri_len;
        assert(cptr);

        /* Change the providers in the AM.*/
        dst_provider = pobj->dst_provider;
        src_provider = pobj->src_provider;
	partial_file_write = pobj->partial_file_write;
	tot_uri_len = pobj->tot_uri_len;

	/* Do an attribute update for content-length Sync
	 */
	if(end && pobj->streaming_put) { 
	    pup.in_attr = pobj->attr;
	    pup.uol = pobj->uol;
	    if(pobj->flags & NKN_MM_UPDATE_CIM)
		pup.in_utype = MM_UPDATE_TYPE_EXPIRY;
	    else
		pup.in_utype = MM_UPDATE_TYPE_SYNC;
            for(i = NKN_MM_max_pci_providers; i > Unknown_provider; i--) {
                pup.in_ptype = i;
                if(MM_update_attr(&pup) >= 0) {
		    glob_mm_update_attr ++;
		}
            }
	    streaming_put = 1;
	}

        /* Done with the state pointers here. The promote may
           still be going on. */
        cr_free(cptr);
	s_mm_free_move_mgr_obj(pobj);
        nkn_task_set_action_and_state(get_task_id, TASK_ACTION_CLEANUP,
					TASK_STATE_RUNNABLE);

put_cleanup:
        /* Cleanup the PUT task */
        promote_uri = nkn_strdup_type(uri, mod_mm_strdup4);
        if(pput->attr) {
            free(pput->attr);
        }
        free(pput);
        nkn_task_set_action_and_state(taskid, TASK_ACTION_CLEANUP,
					TASK_STATE_RUNNABLE);

        if(end == 1) {
            /* The complete URI has been PUT into the provider cache */
            DBG_LOG(MSG, MOD_MM_PROMOTE, "Uri: %s: PUT is done",
                                 promote_uri);
	    if(!partial_file_write) {
		mm_send_promote_debug(promote_uri, src_provider,
				    dst_provider, 1);
		/* Change the provider for this uri in the AM*/
		if((cache_ingest_size_threshold) &&
		    (tot_uri_len &&
		    tot_uri_len <= cache_ingest_size_threshold)) {
		    if(src_provider > NKN_MM_max_pci_providers) {
			AM_set_small_obj_hotness(cod,
					cache_ingest_hotness_threshold);
		    }
		}
	    }

	    /* Signal to the source provider that the promote
	       is done.*/
            glob_mm_promote_success ++;
	    s_mm_promote_complete(cod, promote_uri, src_provider,
				  dst_provider, 0,
				  partial_file_write, in_proto_data,
				  streaming_put,
				  file_size,
				  0, flag, 0, -1, -1, -1,
				  expiry);
            free(promote_uri);
        } else {
	    if(promote_uri) {
		free(promote_uri);
	    }
	}
        return;
    }
    else {
        assert(0);
    }
    return;
}

void
move_mgr_cleanup(nkn_task_id_t taskid __attribute((unused)))
{
    return;
}

void
nkn_mm_resume_ingest(void *mm_obj, char *uri, off_t client_offset, int cod_close)
{
    int		    ret;
    uint64_t	    req_len = 0;
    int		    cleanup = 1;
    time_t	    expiry  = 0;
    mm_move_mgr_t   *pobj = (mm_move_mgr_t *)mm_obj;

    AO_fetch_and_add1(&glob_mm_ingest_resume);

    if(cod_close == INGEST_NO_CLEANUP || cod_close == INGEST_CLEANUP_RETRY) {
	DBG_LOG(MSG, MOD_MM_PROMOTE, "Uri: %s: enter output fn",
            uri);

	pobj->client_offset = client_offset;

	/* In case of aligned data, fill in the req_len based on 
	* remaining length if less than 2MB, or the max of 2MB
	*/
	if(pobj->streaming ||
		(pobj->rem_uri_len > nkn_mm_block_size[pobj->dst_provider]))
	    req_len = nkn_mm_block_size[pobj->dst_provider] - pobj->buflen;
	else
	    req_len = pobj->rem_uri_len - pobj->buflen;

	/* Do the cleanup after a single retry
	 */
	if(cod_close == INGEST_CLEANUP_RETRY)
	    pobj->flags |= NKN_MM_INGEST_ABORT;

	/* Restore the ingest counters */
	if(pobj->flags & NKN_MM_HP_QUEUE) {
	    AO_fetch_and_add1(&glob_mm_hp_promote_started);
	} else if(pobj->flags & NKN_MM_TIER_SPECIFIC_QUEUE) {
	    AO_fetch_and_add1(&nkn_mm_tier_promote_started[pobj->dst_provider]);
	} else
	    AO_fetch_and_add1(&glob_mm_promote_started);

	ret = s_get_data_helper(pobj->uol.cod, uri,
				pobj->uol.offset,
				req_len,
				pobj->rem_uri_len,
				pobj->tot_uri_len,
				pobj->src_provider,
				pobj->dst_provider,
				pobj, pobj->partial_file_write,
				pobj->in_proto_data,
				&pobj->in_client_data,
				pobj->flags, 0);
	if(ret < 0) {
	    s_mm_promote_complete(pobj->uol.cod, uri,
				pobj->src_provider,
				pobj->dst_provider, cleanup,
				pobj->partial_file_write,
				pobj->in_proto_data,
				pobj->streaming_put,
				pobj->tot_uri_len,
				NKN_MM_PROMOTE_RESUME_GETERR,
				pobj->flags, pobj->uol.offset,
				pobj->obj_trigger_time,
				pobj->block_trigger_time,
				pobj->client_update_time,
				expiry);
	} else {
	    /* Need to increment this only if task creation succeeds */
	    pobj->bufcnt++;
	}
    } else {
	s_mm_promote_complete(pobj->uol.cod, uri,
			    pobj->src_provider,
			    pobj->dst_provider, cleanup,
			    pobj->partial_file_write,
			    pobj->in_proto_data,
			    pobj->streaming_put,
			    pobj->tot_uri_len,
			    NKN_MM_PROMOTE_INVCODCLOSE_ERR,
			    pobj->flags, pobj->uol.offset,
			    pobj->obj_trigger_time,
			    pobj->block_trigger_time,
			    pobj->client_update_time,
			    expiry);
    }

    /* Action READ is done for this task. Free. */
    s_mm_free_move_mgr_obj(pobj);
    /* Task cleanup is done when PUT is finished */
    return;

}

/*
  This function encodes the provider and subprovider ids into
  one integer.
*/
provider_cookie_t
AM_encode_provider_cookie(nkn_provider_type_t ptype, uint8_t sptype,
                          uint16_t ssptype)
{
    provider_cookie_t id;
    id = ((ptype & 0xff) << 24) | ((sptype & 0xff) << 16) |
        (ssptype & 0xffff);
    return id;
}

void
AM_decode_provider_cookie(provider_cookie_t id, nkn_provider_type_t *ptype,
                          uint8_t *sptype, uint16_t *ssptype)
{
    *ptype = (id >> 24) & 0xff;
    *sptype = (id >> 16)  & 0xff;
    *ssptype = id  & 0xffff;
}


int
nkn_mm_dm2_attr_copy(nkn_attr_t *ap_src,
	      nkn_attr_t **ap_dst,
	      nkn_obj_type_t objtype)
{
    int ret;

    if(ap_src->total_attrsize > glob_mm_largest_provider_attr_size) {
	return -1;
    }

    ret = nkn_posix_memalign_type((void *)ap_dst, DEV_BSIZE,
				  glob_mm_largest_provider_attr_size, objtype);
    if (ret != 0) {
	glob_mm_attr_alloc_err++;
	return ret;
    }

    nkn_attr_docopy(ap_src, *ap_dst, ap_src->total_attrsize);
    return 0;
}

int
nkn_mm_create_delete_task(nkn_cod_t cod)
{
    int            ret = 0;
    MM_delete_resp_t *pdelete = NULL;
    mm_move_mgr_t  *pobj = NULL;
    nkn_task_id_t  taskid = -1;
    int            count = 5;


    pdelete = (MM_delete_resp_t *) nkn_calloc_type (1, sizeof(MM_delete_resp_t),
						mod_mm_delete_t);
    if(pdelete == NULL) {
        return -1;
    }

    pdelete->in_uol.cod    = cod;

    pobj = (mm_move_mgr_t *) nkn_calloc_type(1, sizeof(mm_move_mgr_t),
					     mod_mm_move_mgr_t);
    if(pobj == NULL) {
        free(pobj);
        return -1;
    }

    pobj->uol.cod       = cod;
    pobj->move_op       = MM_OP_DELETE;

    /* retry if scheduler cannot assign a task */
    while((taskid < 0) && (count > 0)) {
        taskid = nkn_task_create_new_task(
                                          TASK_TYPE_MEDIA_MANAGER,
                                          TASK_TYPE_MOVE_MANAGER,
                                          TASK_ACTION_INPUT,
                                          MM_OP_DELETE,
                                          pdelete,
                                          sizeof(MM_delete_t),
                                          0 /* deadline */);
        count --;
    }
    if((count == 0) || (taskid < 0)) {
        free(pdelete);
        free(pobj);
        return -1;
    }

    pobj->taskid       = taskid;
    pobj->flags        = NKN_MM_UPDATE_CIM;
    nkn_task_set_private(TASK_TYPE_MOVE_MANAGER, taskid,
                         (void *) pobj);
    nkn_task_set_state(taskid, TASK_STATE_RUNNABLE);

    return ret;
}


