/*
 *      COPYRIGHT: NOKEENA NETWORKS
 *
 * This file contains code which implements the Disk Manager
 *
 * Author: Vikram
 * Tester for direct unit testing of DM2.
 *
 */
#include <sys/types.h>
#include <sys/param.h>
#include <unistd.h>
#include <glib.h>
#include "nkn_am_api.h"
#include "nkn_diskmgr2_local.h"
#include "nkn_diskmgr_intf.h"
#include "nkn_sched_api.h"
#include "nkn_diskmgr2_disk_api.h"
#include "nkn_locmgr2_uri.h"
#include "nkn_locmgr2_extent.h"
#include "nkn_locmgr2_container.h"
#include "nkn_locmgr2_physid.h"
#include "nkn_locmgr2_attr.h"
#include "nkn_defs.h"
#include "nkn_util.h"
#include "nkn_mediamgr_api.h"

#define DM2_UNIT_TEST_URI_TBL_SIZE 1000000
#define DM2_TEST_BLK_LEN (2 * 1024 * 1024)

#define DM2_TEST_URI_SIZE 256
typedef struct dm2_test_file_s {
    char *uri;
    char *file;
    uint64_t offset;
    uint64_t len;
} dm2_test_file_t;

typedef struct dm2_put_cfg_s {
    dm2_test_file_t *file;
    nkn_provider_type_t src;
    nkn_provider_type_t dst;
    uint64_t tot_uri_len;
    int expiry;
    int count;
} dm2_put_cfg_t;

GThreadPool *dm2_test_get_thread_pool;
GThreadPool *dm2_test_put_thread_pool;
GThreadPool *dm2_test_delete_thread_pool;

char *dm2_test_get_hash_table[DM2_UNIT_TEST_URI_TBL_SIZE];
dm2_test_file_t *dm2_test_put_hash_table[DM2_UNIT_TEST_URI_TBL_SIZE];
dm2_test_file_t *dm2_test_partial_hash_table[DM2_UNIT_TEST_URI_TBL_SIZE];
uint64_t glob_dm2_test_buf_alloc_failed = 0;
uint64_t glob_dm2_test_partial_buf_alloc_failed = 0;
uint64_t glob_dm2_test_attr_alloc_failed = 0;
uint64_t glob_dm2_test_attr_alloc_success = 0;
uint64_t glob_dm2_test_gets_pushed = 0;
uint64_t glob_dm2_test_stats_pushed = 0;
uint64_t glob_dm2_test_puts_pushed = 0;
uint64_t glob_dm2_test_get_err = 0;
uint64_t glob_dm2_test_stat_err = 0;
uint64_t glob_dm2_test_stat_success = 0;
uint64_t glob_dm2_test_get_success = 0;
uint64_t glob_dm2_test_puts_full_failed = 0;
uint64_t glob_dm2_test_puts_full_success = 0;
uint64_t glob_dm2_test_tot_read_partial = 0;
uint64_t glob_dm2_test_thr_hdl = 0;
uint64_t glob_dm2_test_file_put = 0;
uint64_t glob_dm2_test_num_open_err = 0;
uint64_t glob_dm2_test_num_read_err = 0;
uint64_t glob_dm2_test_num_files_open = 0;
uint64_t glob_dm2_test_num_seek_err = 0;
uint64_t glob_dm2_test_num_files_closed = 0;
uint64_t glob_dm2_test_put_fail = 0;
uint64_t glob_dm2_test_put_err = 0;
uint64_t glob_dm2_test_put_success = 0;
uint64_t glob_dm2_test_part_put_file_err = 0;
uint64_t glob_dm2_test_full_put_file_err = 0;
uint64_t glob_dm2_test_delete_success = 0;
uint64_t glob_dm2_test_delete_fail = 0;
uint64_t glob_dm2_test_deletes_pushed = 0;

int dm2_test_cached_uri_index = 0;
int dm2_test_new_uri_index = 0;
int dm2_test_new_partial_uri_index = 0;
int dm2_test_expiry = 600;

int g_stop_test_thread;

static int
s_dm2_test_delete(dm2_put_cfg_t *cfg);

static void
s_unit_test_delete(dm2_test_file_t *filep,
		   nkn_provider_type_t dst);

static void
s_unit_test_full_put(dm2_test_file_t *filep,
		     nkn_provider_type_t src,
		     nkn_provider_type_t dst);
static int
s_dm2_test_put(dm2_put_cfg_t *cfg,
	       char *file, char *uri, uint64_t in_offset,
	       uint64_t in_length, int expiry,
	       nkn_provider_type_t dst,
	       uint64_t tot_uri_len);
static void
s_run_sequential_partial_puts(nkn_provider_type_t src,
			      nkn_provider_type_t dst,
			      int num_files);

static void
s_unit_test_partial_put(dm2_test_file_t *filep,
			nkn_provider_type_t src,
			nkn_provider_type_t dst);

static int
s_read_file(const char     *in_filename,
	    const uint64_t in_offset,
	    struct iovec   *in_iov,
	    const int32_t  in_iovcnt,
	    uint64_t       *out_read_len);

static void
s_free_statr(MM_stat_resp_t *statr)
{
    if(!statr) {
	assert(0);
	return;
    }
#ifdef FINISH
    if(statr->in_uol.uri) {	// FINISH
	free(statr->in_uol.uri);	// FINISH
    }
#endif
    free(statr);
}

static MM_stat_resp_t *
s_dm2_stat(char *uri __attribute((unused)),
	   uint64_t offset,
	   uint64_t length)
{
    MM_stat_resp_t *statr;
    int ret = -1;
    nkn_provider_type_t ptype = 0;
    int i;

    statr = nkn_calloc_type(1, sizeof(MM_stat_resp_t), mod_dm2_MM_stat_resp_t);
    if (statr == NULL)
	return NULL;

#ifdef FINISH
    statr->in_uol.uri = nkn_strdup_type(uri, mod_dm2_strdup);//FINISH
#endif
    statr->in_uol.offset = offset;
    statr->in_uol.length = length;
    statr->sub_ptype = 0;

    for(i=SATADiskMgr_provider; i >= SolidStateMgr_provider; i--) {
	if((i == 4) || (i == 3) || (i == 2)) {
	    continue;
	}
	statr->ptype = i;
	ret = DM2_stat(statr->in_uol, statr);
	if((ret < 0) || (statr->mm_stat_ret < 0)) {
	    continue;
	}
	ptype = statr->ptype;
	break;
    }

    if(ptype == 0) {
	glob_dm2_test_stat_err ++;
    } else {
	glob_dm2_test_stat_success ++;
    }
    return statr;
}

static void
s_put_thread_hdl(gpointer data,
		 gpointer user_data  __attribute((unused)))
{
    dm2_put_cfg_t *cfg = (dm2_put_cfg_t *) data;

    if (g_stop_test_thread)
	return;

    glob_dm2_test_thr_hdl ++;
    s_dm2_test_put(cfg, cfg->file->file, cfg->file->uri, cfg->file->offset,
		   cfg->file->len, cfg->expiry,
		   cfg->dst,
		   cfg->tot_uri_len);
    free(cfg);
    return;
}

static void
s_delete_thread_hdl(gpointer data,
		    gpointer user_data  __attribute((unused)))
{
    dm2_put_cfg_t *cfg = (dm2_put_cfg_t *) data;

    if (g_stop_test_thread)
	return;

    glob_dm2_test_thr_hdl ++;
    s_dm2_test_delete(cfg);
    free(cfg);
    return;
}

static void
s_get_thread_hdl(gpointer data,
		 gpointer user_data  __attribute((unused)))
{
    MM_get_resp_t *getr = (MM_get_resp_t *)data;
    int ret = -1;
    unsigned int i;
    MM_stat_resp_t *statr;
    nkn_buffer_t *bufs[64];

    assert(getr);

    if (g_stop_test_thread)
	return;

#ifdef FINISH
    statr = s_dm2_stat(getr->in_uol.uri, getr->in_uol.offset,//FINISH
		       getr->in_uol.length);
    if(!statr) {
	glob_dm2_test_stat_err ++;
	return;
    }
#else
    statr = s_dm2_stat(0, getr->in_uol.offset,//FINISH
		       getr->in_uol.length);
#endif // Needs to be fixed

    getr->physid2 = statr->physid2;
    getr->in_ptype = statr->ptype;
    getr->in_num_content_objects = statr->media_blk_len / CM_MEM_PAGE_SIZE;

    if(statr->tot_content_len > statr->media_blk_len) {
	getr->in_uol.length = statr->media_blk_len;
    } else {
	getr->in_uol.length = statr->tot_content_len;
    }

    for(i = 0; i < getr->in_num_content_objects; i ++) {
	bufs[i] = nkn_buffer_alloc(NKN_BUFFER_DATA, 0, 0);
	if(bufs[i] == NULL) {
	    glob_dm2_test_buf_alloc_failed ++;
	    assert(0);
	    getr->in_num_content_objects = i;
	    goto cleanup;
	}
    }

    getr->in_content_array = (void **)&bufs;
    getr->in_attr =  nkn_buffer_alloc(NKN_BUFFER_ATTR, 0, 0);
    if(getr->in_attr == NULL) {
	glob_dm2_test_attr_alloc_failed ++;
	goto cleanup;
    }
    getr->in_ptype = statr->ptype;

    s_free_statr(statr);

    ret = DM2_get(getr);
    if(ret < 0) {
	glob_dm2_test_get_err ++;
	goto cleanup;
    }

    if(getr->err_code < 0) {
	glob_dm2_test_get_err ++;
	goto cleanup;
    }

#if 0
    if(getr->in_num_content_objects != getr->out_num_content_objects) {
	glob_dm2_test_get_err ++;
	goto cleanup;
    }
#endif

    glob_dm2_test_get_success ++;

 cleanup:
#ifdef FINISH
    free(getr->in_uol.uri);//FINISH
#endif
    for(i = 0; i < getr->in_num_content_objects; i ++) {
	nkn_buffer_release(bufs[i]);
    }
    nkn_buffer_release(getr->in_attr);
    free(getr);
    return;
}

static int
s_enque_delete_task(dm2_put_cfg_t *cfg)
{
    glob_dm2_test_deletes_pushed ++;
    g_thread_pool_push(dm2_test_delete_thread_pool,
		       cfg, NULL);

    return 0;
}

static int
s_enque_get_task(char *uri, uint64_t offset, uint64_t length)
{
    MM_get_resp_t *getr = NULL;

    if(!uri) {
	return -1;
    }
    getr = (MM_get_resp_t *) nkn_malloc_type (sizeof(MM_get_resp_t),
					mod_dm2_MM_get_resp_t);
    assert(getr);

#ifdef FINISH
    getr->in_uol.uri = nkn_strdup_type(uri, mod_dm2_strdup);//FINISH
#endif
    getr->in_uol.offset = offset;
    getr->in_uol.length = length;
    glob_dm2_test_gets_pushed ++;
    g_thread_pool_push(dm2_test_get_thread_pool,
		       getr, NULL);
    return 0;
}

static int
s_enque_put_task(dm2_put_cfg_t *cfg)
{
    glob_dm2_test_puts_pushed ++;
    g_thread_pool_push(dm2_test_put_thread_pool,
		       cfg, NULL);
    return 0;
}


static int
s_read_cached_uri_file(char *filename)
{
    FILE *fp = NULL;
    char *urip;
    int ret;

    fp = fopen(filename, "r");
    if(!fp) {
	DBG_DM2S("URI file does not exist for DM2 unit test: %s", filename);
	return -1;
    }

    while(!feof(fp)) {
	urip = (char *) nkn_malloc_type (DM2_TEST_URI_SIZE * sizeof(char),
					mod_dm2_uri_t);
	assert(urip);
	ret = fscanf(fp, "%s\n", urip);
	if (ret == EOF) {
	    free(urip);
	    break;
	}
	dm2_test_get_hash_table[dm2_test_cached_uri_index] = urip;
	dm2_test_cached_uri_index ++;
    }
    return dm2_test_cached_uri_index;
}

static int
s_read_new_uri_file(char *filename)
{
    FILE *fp = NULL;
    char *urip;
    char *filep;
    dm2_test_file_t *dfilep;
    int ret = -1;

    fp = fopen(filename, "r");
    if(!fp) {
	DBG_DM2S("URI file does not exist for DM2 unit test: %s", filename);
	return -1;
    }

    while(!feof(fp)) {
	urip = (char *) nkn_malloc_type (DM2_TEST_URI_SIZE * sizeof(char),
					mod_dm2_uri_t);
	assert(urip);
	filep = (char *) nkn_malloc_type (DM2_TEST_URI_SIZE * sizeof(char),
					mod_dm2_strbuf);
	assert(urip);
	ret = fscanf(fp, "%s %s\n", filep, urip);
	if (ret == EOF) {
	    free(urip);
	    free(filep);
	    break;
	}
	dfilep = (dm2_test_file_t *) nkn_malloc_type (sizeof(dm2_test_file_t),
					mod_dm2_strbuf);
	assert(dfilep);
	dfilep->file = filep;
	dfilep->uri = urip;
	dm2_test_put_hash_table[dm2_test_new_uri_index] = dfilep;
	dm2_test_new_uri_index ++;
    }
    return dm2_test_new_uri_index;
}

static int
s_read_new_partial_put_file(char *filename)
{
    FILE *fp = NULL;
    char *urip;
    int ret = -1;
    char *filep;
    dm2_test_file_t *dfilep;

    fp = fopen(filename, "r");
    if(!fp) {
	DBG_DM2S("URI file does not exist for DM2 unit test: %s", filename);
	return -1;
    }

    while(!feof(fp)) {
	urip = (char *) nkn_malloc_type (DM2_TEST_URI_SIZE * sizeof(char), 
						mod_dm2_uri_t);
	filep = (char *) nkn_malloc_type (DM2_TEST_URI_SIZE * sizeof(char),
						mod_dm2_strbuf);
	assert(urip);
	assert(filep);
	dfilep = (dm2_test_file_t *) nkn_malloc_type (sizeof(dm2_test_file_t),
						mod_dm2_strbuf);
	assert(dfilep);
	ret = fscanf(fp, "%s %s %ld %ld\n", filep, urip,
		     &dfilep->offset, &dfilep->len);
	dfilep->file = filep;
	dfilep->uri = urip;

	if (ret == EOF) {
	    free(urip);
	    free(filep);
	    free(dfilep);
	    break;
	}
	dm2_test_partial_hash_table[dm2_test_new_partial_uri_index]
	    = dfilep;
	dm2_test_new_partial_uri_index ++;
    }
    return dm2_test_new_partial_uri_index;
}


static void
s_run_sequential_gets(int num_files)
{
    int i;
    //    for(i = 0; i < DM2_UNIT_TEST_URI_TBL_SIZE; i++) {
    for(i = 0; i < num_files; i++) {
	s_enque_get_task(dm2_test_get_hash_table[i], 0, 0);
    }
    return;
}

static void
s_run_sequential_full_puts(nkn_provider_type_t src,
			   nkn_provider_type_t dst,
			   int num_files)
{
    int i;
    for(i = 0; i < num_files; i++) {
	s_unit_test_full_put(dm2_test_put_hash_table[i], src,
			     dst);
    }
}

static void
s_run_sequential_deletes(nkn_provider_type_t dst,
			 int num_files)
{
    int i;
    for(i = 0; i < num_files; i++) {
	s_unit_test_delete(dm2_test_put_hash_table[i], dst);
    }
}

static void
s_run_sequential_partial_puts(nkn_provider_type_t src,
			      nkn_provider_type_t dst,
			      int num_files)
{
    int i;
    for(i = 0; i < num_files; i++) {
	s_unit_test_partial_put(dm2_test_partial_hash_table[i], src,
				dst);
    }
}

static void
s_unit_test_init(int num_threads)
{
    char error_buffer[4096];
    GError  *err = (GError *)error_buffer;

    if(num_threads == 0) {
	num_threads = 10;
    }

    dm2_test_get_thread_pool = g_thread_pool_new(&s_get_thread_hdl,
						 NULL,
						 num_threads,
						 TRUE,
						 NULL);
    assert(dm2_test_get_thread_pool);
    g_thread_pool_set_max_threads(dm2_test_get_thread_pool,
                                  num_threads, &err);

    dm2_test_put_thread_pool = g_thread_pool_new(&s_put_thread_hdl,
						 NULL,
						 num_threads,
						 TRUE,
						 NULL);
    assert(dm2_test_put_thread_pool);

    g_thread_pool_set_max_threads(dm2_test_put_thread_pool,
                                  num_threads, &err);

    dm2_test_delete_thread_pool = g_thread_pool_new(&s_delete_thread_hdl,
						    NULL,
						    num_threads,
						    TRUE,
						    NULL);
    assert(dm2_test_delete_thread_pool);

    g_thread_pool_set_max_threads(dm2_test_delete_thread_pool,
                                  num_threads, &err);


    return;
}

void
DM2_unit_test_start(int num_threads, char *cached_filenames,
		    char *new_full_put_filenames,
		    char *new_partial_put_filenames,
		    int pattern __attribute((unused)))
{
    int ret = -1;

    /* Init this only once */
    s_unit_test_init(num_threads);

    ret = s_read_cached_uri_file(cached_filenames);
    if(ret < 0) {
	return;
    }

    ret = s_read_new_uri_file(new_full_put_filenames);
    if(ret < 0) {
	return;
    }

    ret = s_read_new_partial_put_file(new_partial_put_filenames) ;
    if(ret < 0) {
	return;
    }

    s_run_sequential_partial_puts(SATADiskMgr_provider,
				  SATADiskMgr_provider, 0);
    s_run_sequential_full_puts(SATADiskMgr_provider,
			       SATADiskMgr_provider, 10000);
    s_run_sequential_gets(1000);
    s_run_sequential_deletes(SATADiskMgr_provider, 0);
}

#if 0
static void
s_unit_test_promote(char *urip, nkn_provider_type_t src,
		    nkn_provider_type_t dst)
{
    int ret = -1;
    ret = MM_promote_uri(urip, src, dst);

    if(ret < 0) {
	glob_dm2_test_puts_full_failed ++;
	return;
    }

    glob_dm2_test_puts_full_success ++;
}
#endif

static void
s_unit_test_delete(dm2_test_file_t *filep,
		   nkn_provider_type_t dst)
{
    dm2_put_cfg_t cfg;
    if(!filep) {
	return;
    }
    cfg.file = filep;
    cfg.dst = dst;
    s_enque_delete_task(&cfg);
}

static void
s_unit_test_full_put(dm2_test_file_t *filep,
		     nkn_provider_type_t src,
		     nkn_provider_type_t dst)
{
    struct stat sb;
    uint64_t    tot_uri_len;
    int         num_writes;
    int         i;
    uint64_t    rem_uri_len = 0, put_uri_len = 0;
    uint64_t    offset;
    dm2_put_cfg_t *cfg;

    if(!filep) {
	glob_dm2_test_full_put_file_err ++;
	return;
    }

    if (stat (filep->file, &sb) >= 0) {
	tot_uri_len = sb.st_size;
    } else {
	glob_dm2_test_full_put_file_err ++;
	return;
    }

    offset = 0;
    num_writes = tot_uri_len / DM2_TEST_BLK_LEN;
    if((tot_uri_len % DM2_TEST_BLK_LEN) != 0) {
	num_writes ++;
    }

    rem_uri_len = tot_uri_len;
    for(i = 0; i < num_writes; i++) {
	cfg = (dm2_put_cfg_t *) nkn_malloc_type (sizeof(dm2_put_cfg_t),
					mod_dm2_put_cfg_t);
	assert(cfg);
	cfg->file = filep;
	/* Randomly chose offset and length. Align them to respective
	   boundaries defined by DM. */
	cfg->file->offset = offset;

	if(rem_uri_len > DM2_TEST_BLK_LEN) {
	    put_uri_len = DM2_TEST_BLK_LEN;
	} else {
	    put_uri_len = rem_uri_len;
	}
	cfg->file->len = put_uri_len;
	cfg->expiry = dm2_test_expiry;
	cfg->src = src;
	cfg->dst = dst;
	cfg->tot_uri_len = tot_uri_len;
        cfg->count = i;
	s_enque_put_task(cfg);
	offset += DM2_TEST_BLK_LEN;
	rem_uri_len -= DM2_TEST_BLK_LEN;
    }
}

static void
s_unit_test_partial_put(dm2_test_file_t *filep,
			nkn_provider_type_t src,
			nkn_provider_type_t dst)
{
    struct stat sb;
    uint64_t    tot_uri_len, rem_uri_len, offset, diff;
    dm2_put_cfg_t *cfg;
    uint64_t length, put_uri_len;
    int i;
    int num_writes = 0;

    if(!filep) {
	glob_dm2_test_part_put_file_err ++;
	return;
    }
    if (stat (filep->file, &sb) >= 0) {
	tot_uri_len = sb.st_size;
    } else {
	glob_dm2_test_part_put_file_err ++;
	return;
    }

    offset =  1 + (int) (tot_uri_len * (rand() / (RAND_MAX + 1.0)));
    offset = offset & ~(CM_MEM_PAGE_SIZE - 1);
    diff = tot_uri_len - offset;

    length = 1 + (int) (diff * (rand() / (RAND_MAX + 1.0)));
    length = length & ~(512 - 1);

    num_writes = length / DM2_TEST_BLK_LEN;
    if((length % DM2_TEST_BLK_LEN) != 0) {
	num_writes ++;
    }

    rem_uri_len = length;
    for(i = 0; i < num_writes; i++) {
	cfg = (dm2_put_cfg_t *) nkn_malloc_type (sizeof(dm2_put_cfg_t),
					mod_dm2_put_cfg_t);
	assert(cfg);
	cfg->file = filep;
	/* Randomly chose offset and length. Align them to respective
	   boundaries defined by DM. */
	cfg->file->offset = offset;

	if(rem_uri_len > DM2_TEST_BLK_LEN) {
	    put_uri_len = DM2_TEST_BLK_LEN;
	} else {
	    put_uri_len = rem_uri_len;
	}
	cfg->file->len = put_uri_len;
	cfg->expiry = dm2_test_expiry;
	cfg->src = src;
	cfg->dst = dst;
	cfg->tot_uri_len = tot_uri_len;
        cfg->count = i;
	s_enque_put_task(cfg);
	offset += DM2_TEST_BLK_LEN;
	rem_uri_len -= DM2_TEST_BLK_LEN;
    }
}

static MM_put_data_t *
s_allocate_put_data(char *uri, uint64_t in_offset, uint64_t in_length,
		    nkn_provider_type_t dst,
		    nkn_iovec_t *iov, int num_iovecs, nkn_attr_t *attr,
		    uint64_t tot_uri_len, int expiry_time)
{
    MM_put_data_t *pput = NULL;
    int uri_len;

    pput = (MM_put_data_t *) nkn_calloc_type (1, sizeof(MM_put_data_t),
					mod_dm2_MM_put_data_t);
    if(pput == NULL) {
        return NULL;
    }

    uri_len = strlen(uri);
#if 0
    pput->uol.uri = nkn_malloc_type(uri_len+1, mod_dm2_uri_t);
    if(pput->uol.uri == NULL) {
        free(pput);
        return NULL;
    }

    memcpy(pput->uol.uri, uri, uri_len);
#endif
#ifdef FINISH
    pput->uol.uri[uri_len] = '\0';//FINISH
#endif
    pput->uol.offset            = in_offset;
    pput->uol.length            = in_length;
    pput->errcode               = 0;
    pput->iov_data_len          = num_iovecs;
    pput->domain                = 0;
    pput->iov_data              = iov;

    if(attr) {
        pput->attr                 = attr;
	pput->attr->content_length = tot_uri_len;
	pput->attr->cache_expiry   = expiry_time;
    } else {
        pput->attr              = NULL;
    }
    pput->total_length  = tot_uri_len;
    pput->ptype         = dst;

    return pput;
}

static int
s_read_file(const char     *in_filename,
	    const uint64_t in_offset,
	    struct iovec   *in_iov,
	    const int32_t  in_iovcnt  __attribute((unused)),
	    uint64_t       *out_read_len)
{
    int fd;
    int out_errno = 0;
    off_t long_retf, tot_read_len = 0;
    int   i;

    //fd = open(in_filename, O_RDONLY | O_DIRECT);
    fd = open(in_filename, O_RDONLY);
    if (fd < 0) {
        out_errno = errno;
        glob_dm2_test_num_open_err ++;
        return out_errno;
    }
    glob_dm2_test_num_files_open ++;

    long_retf = lseek(fd, in_offset, SEEK_SET);
    if (long_retf >= 0) {
        //long_retf = readv(fd, in_iov, in_iovcnt);
#if 1
	for(i = 0; i < in_iovcnt; i ++) {
	    long_retf = read(fd, (void *)in_iov[i].iov_base, 32768);
	    tot_read_len += long_retf;
	}
#endif
        if (tot_read_len <= 0) {
            out_errno = errno;
            glob_dm2_test_num_read_err ++;
        }
        *out_read_len = tot_read_len;
    }
    else {
        out_errno = errno;
        glob_dm2_test_num_seek_err ++;
    }

    close(fd);
    glob_dm2_test_num_files_closed ++;
    return out_errno;
}

static int
s_dm2_test_delete(dm2_put_cfg_t *cfg)
{
    MM_delete_resp_t dp;
    int              ret = -1;

#ifdef FINISH
    dp.in_uol.uri = cfg->file->uri;//FINISH
#endif

    dp.in_uol.offset = 0;
    dp.in_uol.length = 0;
    dp.in_ptype = cfg->dst;
    dp.in_sub_ptype = 0;

    ret = DM2_delete(&dp);
    if(ret < 0) {
	glob_dm2_test_delete_fail ++;
	return -1;
    }
    glob_dm2_test_delete_success ++;
    return 0;
}

static int
s_dm2_test_put(dm2_put_cfg_t *cfg, char *file, char *uri, uint64_t in_offset,
	       uint64_t in_length, int in_expiry,
	       nkn_provider_type_t dst,
	       uint64_t tot_uri_len)
{
#define DM2_TEST_STACK_ALLOC (2*1024*1024/CM_MEM_PAGE_SIZE)
    int             i, j;
    nkn_iovec_t     iovp_mem[DM2_TEST_STACK_ALLOC];
    nkn_iovec_t     *iov = iovp_mem;
    void            *content_ptr = NULL, *attr_buf = NULL;
    nkn_attr_t      *attr_ptr = NULL;
    void            *ptr_vobj;
    nkn_uol_t       uol;
    uint64_t        length_read = 0;
    int             cont_idx = 0;
    int             num_pages_needed = 0;
    int             partial_pages = 0;
    uint64_t        read_length = 0;
    uint64_t        base_offset = 0, read_offset = 0;
    int             ret_val = -1;
    int             num_allocd_pages = 0;
    nkn_buffer_t    *bufs[64];
    MM_put_data_t   *pput;

    glob_dm2_test_file_put ++;

    num_pages_needed = in_length / CM_MEM_PAGE_SIZE;
    partial_pages = in_length % CM_MEM_PAGE_SIZE;
    if(partial_pages > 0)
        num_pages_needed ++;

    /* Set the iovec to contain the buf pointers from bufmgr*/
    for(i = 0; i < num_pages_needed; i++) {
	if ((bufs[i] = nkn_buffer_alloc(NKN_BUFFER_DATA, 0, 0)) == NULL) {
	    glob_dm2_test_partial_buf_alloc_failed ++;
	    goto cleanup;
	}
	num_allocd_pages ++;
        content_ptr = nkn_buffer_getcontent(bufs[i]);
        assert(content_ptr);
        iov[i].iov_base = content_ptr;
    }

    base_offset = read_offset = in_offset & ~(CM_MEM_PAGE_SIZE - 1);
    ret_val = s_read_file(file, read_offset,
			  (struct iovec *)iov,
			  num_pages_needed, &length_read);

    if(ret_val != 0) {
	goto cleanup;
    }

#ifdef FINISH
    uol.uri = uri;//FINISH
#endif
    if(cfg->count == 0) {
	if ((attr_buf = nkn_buffer_alloc(NKN_BUFFER_ATTR, 0, 0)) == NULL) {
	    glob_dm2_test_attr_alloc_failed ++;
	    goto cleanup;
	}
	glob_dm2_test_attr_alloc_success ++;
	attr_ptr = (nkn_attr_t *) nkn_buffer_getcontent(attr_buf);
	/* Attributes are already provided. Just copy it.*/
	//s_dm2_test_set_attr(attr_ptr);
	nkn_buffer_setid(attr_buf, &uol,
			 SATADiskMgr_provider, 0);
    } else {
	attr_ptr = NULL;
    }

    read_length = length_read;
    glob_dm2_test_tot_read_partial += read_length;
    /* Set the IDs of each buffer for bufmgr to identify  */
    for (j = 0; j < num_pages_needed; j++) {
        ptr_vobj = bufs[j];
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
        }

        iov[j].iov_len = uol.length;
        nkn_buffer_setid(ptr_vobj, &uol, SATADiskMgr_provider, 0);
        cont_idx ++;
    }

    pput = s_allocate_put_data(uri, base_offset, in_length,
			       dst, iov,
			       num_allocd_pages, attr_ptr,
			       tot_uri_len, in_expiry + nkn_cur_ts);

    ret_val = DM2_put(pput, 0);
    if(ret_val < 0) {
	glob_dm2_test_put_fail ++;
	goto cleanup;
    } else {
	glob_dm2_test_put_success ++;
    }

    for(i = 0; i < num_allocd_pages; i ++) {
        nkn_buffer_release(bufs[i]);
    }
    nkn_buffer_release(attr_buf);

    return ret_val;

 cleanup:
    glob_dm2_test_put_err ++;
    for(i = 0; i < num_allocd_pages; i ++) {
	nkn_buffer_release(bufs[i]);
    }
    nkn_buffer_release(attr_buf);
    return ret_val;
}
