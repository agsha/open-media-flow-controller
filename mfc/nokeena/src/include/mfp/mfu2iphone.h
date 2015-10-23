#ifndef _MFU2IPHONE_
#define _MFU2IPHONE_

#include <pthread.h>

#include "mfp_limits.h"
#include "nkn_vpe_mfp_ts2mfu.h"
#include "nkn_vpe_mfp_ts.h"
#include "mfp_ref_count_mem.h"
#include "mfp_safe_io.h"
#include "nkn_sched_api.h"
#include "nkn_authmgr.h"
#include "nkn_encmgr.h"
#include "kms/mfp_kms_key_store.h"

/*
   Headers for maintianig playlists
   for "fruit" devices 
 */
#define MAX_FRUIT_CHNLS  150

#define MAX_FRUIT_STREAMS MAX_STREAM_PER_ENCAP

typedef enum ms_fmtr_encap_status_tt{
    FRUIT_ENCAP_INACTIVE =	0,
    FRUIT_ENCAP_PART_ACTIVE = 1,
    FRUIT_ENCAP_TEMP_MASTER_PL = 2,
    FRUIT_ENCAP_ACTIVE = 3
}ms_fmtr_encap_status_t;

typedef enum ms_fmtr_strm_status_tt{
    FRUIT_STREAM_INACTIVE =    0,
    FRUIT_STREAM_PART_ACTIVE = 1,
    FRUIT_STREAM_ACTIVE =      2,
    FRUIT_STREAM_ERROR =       4,
    FRUIT_STREAM_RESYNC =      8,
    FRUIT_STREAM_ENDLIST_PUB = 9,
}ms_fmtr_strm_status_t;

#define FRUIT_FMT_INPUT_SET_STATE(x,y) (x)->state = (y);

/*FRUIT formatter states */
typedef enum MS_FMTR_STATE_TT{
    FRUIT_FMT_ENCRYPT = 1,
    FRUIT_FMT_CONVERT = 2,
    FRUIT_FMT_PL_UPDATE = 4,
    FRUIT_FMT_AUTH = 8
}ms_fmtr_state_t;


typedef struct tag_fruit_fmtr_input {
    ms_fmtr_state_t state;
    auth_msg_t am;
    bitstream_state_t *ts_stream;
    char chunk_name[256];
    uint32_t mfu_seq_num;
    const mfp_safe_io_ctx_t *sioc;
    enc_msg_t em;
    kms_key_resp_t key_resp;
    uint32_t streams_per_encap;
    int32_t *active_task_cntr;
    int8_t fmt_refresh;
}fruit_fmt_input_t;

typedef struct fruit_fmt{
    uint32_t segment_start_idx;
    uint8_t min_segment_child_plist;
    uint8_t segment_idx_precision;
    uint32_t segment_rollover_interval;
    uint8_t dvr;
    char mfu_out_fruit_path[500];
    char mfu_uri_fruit_prefix[500];
    uint32_t key_frame_interval;
    kms_client_key_store_t** kms_key_stores;//array of kms key store(1 / stream)
}fruit_fmt_t;

typedef struct fruitst{
    ms_fmtr_strm_status_t status; /*0: unregistered;1:registered*/
    uint16_t stream_id;
    uint32_t chunk_num; /*Wrapping counter*/
    char m3u8_file_name[700];
    ts_desc_t* ts; /*Transprt stream descriptor for this encap/stream*/
    uint32_t total_bit_rate;//Audio+video bitrate bps
    uint32_t seq_num;
    uint32_t total_num_chunks;
    uint32_t is_end_list;
    pthread_mutex_t lock;
    uint32_t num_resched;
    uint32_t max_duration;
}fruit_stream_t;

typedef struct update_playlist_queue_tt{
    uint32_t program_num;
    uint32_t stream_id;
    uint32_t duration;
    int8_t discontinuity_flag;
    kms_key_resp_t *key_resp;
    uint32_t slot_filled;
}update_playlist_queue_t;

typedef struct fruiten{
    uint16_t num_streams_active; //Number of streams active.
    fruit_stream_t stream[MAX_FRUIT_STREAMS];
    char m3u8_file_name[500]; //Master m3u8 file name
    ms_fmtr_encap_status_t status;
    fruit_fmt_t fruit_formatter;
    uint32_t is_end_list;
    uint32_t mfu_seq_num;
    uint32_t num_streams_pub_msn; //number of streams that completed minimum mfu seq number
    update_playlist_queue_t updplqueue[MAX_FRUIT_STREAMS];
    uint32_t task_build_warn_limit;
    uint32_t task_build_high_limit;

	uint32_t kms_ctx_active_count;
	ref_count_mem_t* kms_ctx_del_track;
	pthread_mutex_t lock;
    register_formatter_error_t error_callback_fn;
    pthread_mutex_t error_callback_lock;
	char name[100];
}fruit_encap_t;

typedef struct fruitctx{
    uint16_t num_encaps_active; //Number of streams active. Set as 0 initially
    fruit_encap_t encaps[MAX_MFP_SESSIONS_SUPPORTED];
}gbl_fruit_ctx_t;

/*Function Decalarations*/
uint32_t get_sessid_from_mfu(mfu_data_t *mfu_data);
void apple_fmtr_hold_encap_ctxt(uint32_t sess_id);
void apple_fmtr_release_encap_ctxt(uint32_t sess_id);

int32_t mfubox_ts(ref_count_mem_t *ref_cont);
int32_t activate_fruit_encap(
	fruit_fmt_t fmt, 
	int32_t encap_num,
	uint32_t num_streams_per_encaps);

int32_t update_master_end_list(fruit_encap_t* encap);
int32_t update_child_end_list(char *child_name);
void destroyMfuData(void *mfu_data); 
void mfp_live_input(nkn_task_id_t tid);
void register_stream_failure(
	int32_t sess_id,
	int32_t stream_id);

void register_stream_dead(
	int32_t sess_id,
	int32_t stream_id);
void register_stream_resync(
	int32_t sess_id,
	int32_t stream_id,
	uint32_t accum_mfu_seq_num,
	uint32_t refreshed);
void mfp_live_output(nkn_task_id_t tid);
void mfp_live_cleanup(nkn_task_id_t tid);


#define FMTR_ERRORCALLBACK(x){\
	pthread_mutex_lock(&encaps->error_callback_lock);\
	if(encaps->error_callback_fn){\
	mfu_data_t *mfu_data_l = (mfu_data_t*)ref_cont->mem;\
	mfu_data_l->fmtr_type = FMTR_APPLE;\
	if(x == 2)\
	mfu_data_l->fmtr_err = ERROR_INPUT_DATA;\
	else\
	mfu_data_l->fmtr_err = ERROR_OUTPUT_IO;\
	encaps->error_callback_fn(ref_cont);\
	}\
	pthread_mutex_unlock(&encaps->error_callback_lock);\
}


#endif
