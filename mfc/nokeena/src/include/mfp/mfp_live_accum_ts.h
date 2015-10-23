#ifndef MFP_LIVE_ACCUM_TS_H
#define MFP_LIVE_ACCUM_TS_H

#include <stdint.h>
#include <stdlib.h>

#include "mfp_accum_intf.h"
#include "mfp_live_global.h"
#include "nkn_vpe_mfu_defs.h"
#include "mfp_safe_io.h"
#include "mfp_sl_fmtr_conf.h"
#include "mfp_ref_count_mem.h"
//#include "nkn_vpe_ts_demux.h"

#if !defined(TS2MFU_TEST)
#include "thread_pool/mfp_thread_pool.h"
#endif

//#define BZ7998


/*Ma amount of data to be buffered*/
#define MFP_TS_MAX_ACC_TIME 60 
#define MFP_ACC_MAX_BUF_DEPTH 20
#define MFP_ACC_BUF_TOLERANCE 10
#define ACC_IN_SYNC     1
#define ACC_NOT_IN_SYNC 0
#define MFP_MAX_ST_BUF_DEPTH 100

#define MFP_MEM_OPT
#ifdef MFP_MEM_OPT
   #define MFP_MAX_AV_LAG 300
#else
#define MFP_TS_MAX_PKTS 30000
#define MFP_TS_MAX_ST_BUF_DEPTH 20 /*20*kfi seconds of data before Out of sync*/
#endif

#define ACCUM_MAX_RESYNC 4 /*Max resyncs for an accumulator*/


typedef struct mux_st{
    uint16_t pmt_pid;
    uint16_t video_pid;
    uint16_t audio_pid1;
    uint16_t audio_pid2;
    int8_t demux_flag; //Indicates if the streams have been demuxed sufficiently:1 if so; 0 if not.
    int8_t pmt_parsed; //0: PMT not parsed. Else 1.
    uint8_t video_in_sync;
    uint8_t audio1_in_sync;
    uint8_t audio2_in_sync;
}mux_st_elems_t;

typedef struct buffsdt{
#ifdef MFP_MEM_OPT
    uint32_t mfp_ts_max_pkts;
    uint32_t  *pkt_offsets;
#else
    uint32_t  pkt_offsets[MFP_TS_MAX_PKTS];
    //Buffer that will include the offsets of the data
    // from the input MUX buffer
#endif
    uint32_t num_of_pkts;       //number of packets actually stored.
    int8_t pending_read;        //-1: buffer invalid; 0: not pending read; 1: has data for MFP write
    uint32_t starting_time;     // in milliseconds
    uint32_t time_accumulated;  // in ms
    int8_t data_full;           //-1: buffer invalid; 0: valid buffer with data < chunk_time ;1: has chunk_time worth data.
    uint32_t audio_time_acced;  //The audio time accumulated in clock ticks.
    uint32_t video_time_acced;
}stream_buffer_data_t;

/*State of each of the buffers*/
typedef struct buffdt{
#ifdef MFP_MEM_OPT
    stream_buffer_data_t *stream;
    uint32_t mfp_max_buf_depth;
    uint32_t mfp_ts_max_pkts;
#else
    stream_buffer_data_t stream[MFP_MAX_ST_BUF_DEPTH];
#endif
    uint32_t write_pos;
    int8_t buff_full;
    uint32_t num_buffs_pending_read;
    uint32_t first_sync_id;
    int8_t just_in_sync;
    /*Variables used for Audio only for sync*/
    uint32_t global_time_accumulated;
    int32_t num_chunked;
    /*Variable for video bias*/
    uint32_t video_ts_bias;/*
                    Indicates the bias of the
                    video clock in 90KHz scale
                    Accounts  For non-integer frame rates
                    there is a bias associated
                    in ticks. For 29.97Hz frame
                    resolution is 3003 ticks
                    which will cause A/V sync
                    issues.*/
    uint8_t video_bias_chkd;
    /*Flag to check if bias has
      been checked*/
    int8_t ts_warped;
    uint32_t warp_start;
    uint32_t incoming_kfi;
#ifdef BZ7998
    uint32_t sync_time;
#endif
    int32_t residual_mfu_time; //difference between the acced time in acc and the mfu'ed duration
    int32_t prev_residue; 
}stream_buffer_state_t;


typedef struct  spts{
    mux_st_elems_t mux;      //Contains properties of stream if MUX'ed. Else NULL.
    /*Individual audio and video buffers.*/
    stream_buffer_state_t *video;
    stream_buffer_state_t *audio1;
    stream_buffer_state_t *audio2;
    uint32_t spts_data_size; //This refers to the MAX size of each buffer
    uint32_t sync_time; // sync time for this SPTS
}spts_st_ctx_t;

typedef struct tag_accum_stats {
    /* instance name and stream id are required to remove the
     * counters; the deleteAccumulator interface does not pass the
     * stream-id 
     */
    char *instance;

    char *resync_count_name;
    uint32_t resync_count;

    char *tot_pkts_rx_name;
    uint32_t tot_pkts_rx;

    char *tot_video_duration_name;
    uint32_t tot_video_duration;

    char *tot_audio_duration_strm1_name;
    uint32_t tot_audio_duration_strm1;

    char *tot_audio_duration_strm2_name;
    uint32_t tot_audio_duration_strm2;

    char *tot_proc_ms_chunks_name;
    uint32_t tot_proc_ms_chunks;

    char *tot_proc_ss_chunks_name;
    uint32_t tot_proc_ss_chunks;

    char *tot_video_pkt_loss_name;
    uint32_t tot_video_pkt_loss;

    char *tot_audio_pkt_loss_name;
    uint32_t tot_audio_pkt_loss;

    char *tot_kfnf_err_name;
    uint32_t tot_kfnf_err;  //key frame not found 

    char *avg_audio_bitrate_name;
    uint32_t avg_audio_bitrate;

    char *avg_video_bitrate_name;
    uint32_t avg_video_bitrate;

    char *apple_fmtr_num_active_tasks_name;
    int32_t *apple_fmtr_num_active_tasks;

    uint64_t acc_video_bytes;
    uint64_t acc_audio_1_bytes;
    uint64_t acc_audio_2_bytes;
      
} accum_stats_t;

typedef enum accum_state_tt{
    ACCUM_INIT = 0,
    ACCUM_SYNC= 1,
    ACCUM_VIDEO_INSYNC = 2,
    ACCUM_AUDIO1_INSYNC = 4,
    ACCUM_AUDIO2_INSYNC = 8,
    ACCUM_RUNNING = 16,
    ACCUM_ERROR = -1
}accum_state_t;

typedef void (*register_formatter_error_t)(ref_count_mem_t *ref_cont);

typedef struct accum_ts {
    uint32_t mfu_seq_num;
    int32_t present_read_index;  // Present buffer index that is being read for mfu write
    accum_interface_t accum_intf;
    //int8_t sync_state;         //Indicates whether acc is in a SYNC state or not. 
    mfub_t mfu_hdr;
    int32_t num_resyncs; //Indicates number of consecutive resyncs
    int8_t refreshed;
    /*New ones*/
    spts_st_ctx_t *spts;   
    int8_t start_accum;   //1: Start accum. Else dont.
    uint8_t *data;       // Buffer carrying the input data.
    uint32_t data_size;   //Max Size of this buffer.
    uint32_t data_pos;   // Present position of the data buffer.
    uint32_t last_read_index;
    /*remove later*/
    uint32_t key_frame_interval;
    mfp_safe_io_ctx_t *sioc;
    accum_stats_t stats;
    //int8_t running;
    accum_state_t accum_state;
    register_formatter_error_t error_callback_fn;
#if defined(INC_ALIGNER)
    //aligner
    struct aligner_ctxt *aligner_ctxt;
    struct video_frame_info *video_frame_tbl;
#endif
    int8_t refresh_ms_fmtr;
} accum_ts_t;


//accum_ts_t* newAccumulatorTS(sess_stream_id_t*);

typedef void (*mfpTaskFunc)(void* clientData);

uint64_t mfpRtscheduleDelayedTask(int64_t microseconds,
					   mfpTaskFunc proc,
					   void* clientData) ;
void getBufferArea(int8_t** data, uint32_t* len_avl,
		   sess_stream_id_t* id);
int32_t processData(int8_t* data, uint32_t len,
		    sess_stream_id_t* id);
void handleTsEOS(void *ctx, uint32_t stream_id);
void register_formatter_error(ref_count_mem_t *ref_cont);

#endif
