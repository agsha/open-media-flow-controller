#ifndef MFP_ACCUM_INTF_H
#define MFP_ACCUM_INTF_H

#include <sys/time.h>
//#include "nkn_vpe_mfp_ts2mfu.h"
/*
  This is an Accumulator Interface Specification file.
  There can be many types of accumulator.
  For example: Video and Audio streams can have different accumulators.
  Each of the defined accumulator MUST have the below interfaces. These 
  handlers are essential for other components to communicate with them
*/




#define MFP_ACC_BUF_PENDING_READ 1
#define MFP_ACC_BUF_WAIT 0
#define MFP_ACC_BUF_INVALID -1
#define MFP_ACC_BUF_FULL -2

#define MFP_ACC_SUCCESS 1234 
#define MFP_ACC_ERROR   -1234

#define AUDIO_BIAS


#ifdef AUDIO_BIAS
typedef struct audbias{
    //Biasing Parameters
    int32_t total_audio_bias;
    uint32_t num_chnks_beyond_bias;
}audio_bias_t;
#endif




typedef struct ts2mfu_cont_tt {
    //these variables are for maintaining continuity between fragments
    uint32_t is_audio;
    uint32_t num_tspkts_retx;
    uint32_t last_pkt_start_offset;
    uint32_t last_pkt_end_offset;
    uint32_t prev_chunk_frame_end;
    uint32_t curr_chunk_frame_start;
    uint32_t curr_chunk_frame_end;
    uint32_t first_instance;
    uint8_t  *prev_aud_chunk;
    uint32_t prev_aud_chunk_size;
    uint32_t prev_aud_chunk_cont[3];

    //AV sync variables
    uint32_t last_audio_index;
#ifdef AUDIO_BIAS
    //Biasing Parameters
    audio_bias_t bias;
#endif
    uint32_t video_duration;
    uint64_t video_tot_acc;
    uint64_t audio_tot_acc;
    uint32_t num_video_chunks;
    uint32_t num_audio_chunks;
    uint32_t avg_video_duration;
    uint32_t avg_audio_duration;
    int64_t tot_aud_vid_diff;
    int64_t aud_diff;
    uint32_t audio_sample_duration;
    void *ts2mfu_desc;
    int8_t store_ts2mfu_desc;
    uint32_t video_time_accumulated;
} ts2mfu_cont_t;



typedef struct sess_stream_id {
	uint32_t sess_id;
	uint32_t stream_id;
	ts2mfu_cont_t ts2mfu_cont;

	//timestamp of last received stream pkt
	struct timeval last_seen_at;
} sess_stream_id_t;

typedef void (*getDataReadPtr)(int8_t** data, uint32_t* len_avl,
			       sess_stream_id_t* id);
typedef int32_t (*dataInHandler)(int8_t* data, uint32_t len,
				 sess_stream_id_t* id);
typedef void (*deleteAccum)(void*);
typedef void (*handle_EOS)(void*, uint32_t);
typedef struct accum_interface {
	getDataReadPtr get_data_buf;
	dataInHandler data_in_handler;
	deleteAccum delete_accum;
        handle_EOS handle_EOS;
}accum_interface_t;

typedef struct buff_state{
    uint8_t* data;
    uint32_t data_size;         //Data filled in this buffer
    uint32_t buffer_len;        //total len of buffer - calculated form bitrate and duration
    int8_t pending_read;        //-1: buffer invalid; 0: not pending read; 1: has data for MFP write
    uint32_t starting_time;     // in milliseconds
    uint32_t time_accumulated;  // in ms
    int8_t data_full;           //-1: buffer invalid; 0: valid buffer with data < chunk_time ;1: has chunk_time worth data.
    uint32_t write_pos;         //The position where data is written to
                                //be written. Init value 0  
}buffer_state_t;


#endif
