#ifndef _NKN_COMPRESSM_API_H
#define _NKN_COMPRESSM_API_H


#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <assert.h>
#include <stdint.h>
#include <ctype.h>

#include "nkn_sched_api.h"
#include "atomic_ops.h"
#include "nkn_lockstat.h"
#include "nkn_stat.h"
#include "nkn_debug.h"
#include "nkn_memalloc.h"
#include "server.h"
#include <zlib.h>
#include <string.h>
#include <limits.h>
#include "nkn_defs.h" 

#define UNUSED_ARGUMENT(x)      (void)x
#define MAX_CM_THREADS 		8
#define MAX_CM_TASK_ARR 	64001
#define COMPRESS_MGR_MAGIC 		0xc0355555
#define COMPRESS_MGR_MAGIC_DEAD 	0xdeadc055


#define COMPRESS_MGR_INIT_START_WORKER_TH 0x00000001
typedef struct compress_msg_t
{
    int32_t magic;
    uint8_t comp_err;
    uint8_t errstr[1024]; //Error string returned by third party;
    int64_t task_id;    //For holding the task ID of the caller
    uint32_t comp_in_len;
    uint32_t comp_init;
    uint32_t comp_in_processed;
    char *comp_buf;
    uint32_t comp_offset;
    uint32_t comp_out;
    z_stream *strm;
    void * ocon_ptr;
    uint32_t comp_type;
    uint32_t tot_expected_in;
    uint32_t comp_end;
    nkn_iovec_t *in_content;
    uint32_t in_iovec_cnt;
    uint32_t out_iovec_cnt;
    nkn_buffer_t *out_bufs[MAX_CM_IOVECS];

    nkn_iovec_t out_content[MAX_CM_IOVECS];
    void *(* alloc_buf)(off_t * buf_size);
    char *(* buf2data)(void *token);
	

} compress_msg_t;
#if 0
typedef struct compress_msg_t
{
    int32_t magic;
    uint8_t comp_err;
    uint8_t errstr[1024]; //Error string returned by third party;
    int64_t task_id;    //For holding the task ID of the caller
    uint32_t comp_in_len;
    uint32_t comp_init;
    uint32_t comp_in_processed;
    uint32_t comp_max_out_len;
    uint32_t comp_out1;
    char *comp_buf1;
    uint32_t comp_out2;
    char *comp_buf2;
    char *comp_buf;
    uint32_t comp_out;
    z_stream strm;
    void * ocon_ptr;
    uint32_t comp_type;
    uint32_t tot_expected_in;
    uint32_t comp_end;
    nkn_iovec_t *content;
    uint32_t in_iovec_cnt;
    char*   comp_data;	//Based on COMPRESSTYPE, different structs could be deref
    void (* cp_connect_failed)(void * private_data, int called_from);
	

} compress_msg_t;
#endif
void compress_mgr_input(nkn_task_id_t id);		//puts into taskqueue
void compress_mgr_output(nkn_task_id_t id);		//dummy
void compress_mgr_cleanup(nkn_task_id_t id);	//dummy
void *compress_mgr_input_handler_thread(void *arg);	//picks tasks from queue
int compress_mgr_init(uint32_t init_flags);		//initializes the worker threads

#endif /* _NKN_COMPRESSM_API_H */
