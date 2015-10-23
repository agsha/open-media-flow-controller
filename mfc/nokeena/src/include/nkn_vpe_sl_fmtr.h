#ifndef NKN_VPE_SL_FMTR_H
#define NKN_VPE_SL_FMTR_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>

#include <libxml/xmlwriter.h>

#include "nkn_vpe_mfu2moof.h"
#include "nkn_vpe_mfu_defs.h"
#include "nkn_vpe_h264_parser.h"
#include "mfp_ref_count_mem.h"
#include "mfp_live_accum_ts.h"
#include "nkn_debug.h"
#include "nkn_vpe_utils.h"
#include "nkn_vpe_mfu_parse.h"
#include "nkn_vpe_ha_state.h"

#include "io_hdlr/mfp_io_hdlr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MFU2MOOF_MAX_CHANNELS (20)

	struct adts_data;

	typedef void (*SL_TaskHdlr_fptr)(void*);

	typedef void (*sched_timed_event_fptr)(struct timeval const* s_val,
			SL_TaskHdlr_fptr hdlr, void* ctx);

	// **** PUBLISH state and handlers ****
	typedef struct mfu2moof_track_state {

		uint8_t media_type;// 0 : Video, 1 : Audio
		char mdat_type[5]; // null terminated 4 letter string
		uint32_t bit_rate;

		uint32_t mnf_intv_chunk_count;
		uint32_t num_of_chunks;
		uint32_t tot_num_of_chunks;
		uint64_t *base_ts;
		uint32_t *exact_duration;

		char* codec_data;
		uint32_t codec_data_len;

		nkn_vpe_sps_t* sps;//Not null only if mdat_type is "H264"

		struct adts_data* adts;// audio specific
		stream_state_e strm_state;

	} mfu2moof_track_state_t;


	struct sl_fmtr;

	typedef struct mfu2moof_task_data {
		struct sl_fmtr* fmtr_state;
		char *mfu;
	} mfu2moof_task_data_t;

	// converts mfu to moof and updates internal state on chunks, dur etc
	// internally uses util_mfu2moof for conversion
	typedef int16_t (*handleMfu_fptr)(ref_count_mem_t *ref_cont);

	typedef mfu2moof_track_state_t const* (*get_track_state_sl_fptr)
		(struct sl_fmtr const *fmtr_state,
		 uint32_t channel_id, uint32_t track_idx);

	typedef void (*delete_sl_fptr)(struct sl_fmtr*);

	typedef int32_t (*lock_if_sl_fmtr)(struct sl_fmtr*);

	typedef uint32_t (*get_sess_chunk_ctr_fptr)(struct sl_fmtr const*);

	typedef void (*setErrorState_fptr)(struct sl_fmtr*, uint32_t group_id);


	// Possible Publish types
	typedef enum {

		SL_FILE,
		SL_LIVE,
		SL_DVR
	} sl_fmtr_publ_type_t;


	typedef struct sl_fmtr {
		mfu2moof_track_state_t* tracks[MFU2MOOF_MAX_CHANNELS];
		uint32_t track_count[MFU2MOOF_MAX_CHANNELS];
		uint32_t seq_num[MFU2MOOF_MAX_CHANNELS];
		char* sess_name;
		char* store_url;
		uint32_t chunk_duration_approx;
	    uint32_t abitrate_computed_flag;
	    uint32_t abitrate;
		sl_fmtr_publ_type_t publ_type;
		uint32_t chunk_wait_count;
		uint32_t dvr_chunk_count;
		int8_t eos_flag;
		int8_t err_flag;
		uint32_t sess_chunk_ctr;
		uint32_t reset_ctr;
		pthread_mutex_t lock;

		io_handler_t* io_cb;
		io_complete_hdlr_fptr io_end_hdlr;

		// variables for performance measurement
		double fmtr_time_consumed;
		uint64_t fmtr_call_count;
		double fmtr_min_time;
		double fmtr_max_time;

		double fmtr_moof_write_time_consumed;
		uint64_t fmtr_moof_write_count;
		double fmtr_min_moof_write_time;
		double fmtr_max_moof_write_time;

		// function ptrs
		handleMfu_fptr handle_mfu;
		get_sess_chunk_ctr_fptr get_sess_chunk_ctr;
		lock_if_sl_fmtr acquire_fmtr;
		lock_if_sl_fmtr release_fmtr;
		get_track_state_sl_fptr get_track_state;
		delete_sl_fptr reset_state;
		delete_sl_fptr delete_ctx;
		setErrorState_fptr set_error_state;

		sched_timed_event_fptr sched_timed_event;

		//error callback function pointers
		register_formatter_error_t error_callback_fn;
	} sl_fmtr_t;


	sl_fmtr_t* createSLFormatter(char const* sess_name,
		 char const* store_url, sl_fmtr_publ_type_t publ_type,
		 uint32_t chunk_duration_approx, uint32_t dvr_chunk_count,
		 sched_timed_event_fptr sched_timed_event,
		 register_formatter_error_t error_callback_fn);

	void slTaskInterface(void* task);
	// ************************


	// utility function for retrieving adts data from AAC ADTS header
	typedef struct adts_data {

		uint8_t id;
		uint8_t mpeg_layer;
		uint8_t prot_ind;
		uint8_t profile_code;
		uint32_t sample_freq;
		uint8_t channel_count;
		uint16_t frame_length;
		uint8_t frame_count;
	} adts_data_t;

	adts_data_t* parseADTS(uint8_t const *stream);
	// ************************


#ifdef __cplusplus
}
#endif

#endif

