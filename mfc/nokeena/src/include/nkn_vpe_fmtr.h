#ifndef NKN_VPE_FMTR_H
#define NKN_VPE_FMTR_H

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

#include "mfp_limits.h"
//#include "mfp_publ_context.h"
//#include "nkn_vpe_mfu2moof.h"
#include "nkn_vpe_mfu_defs.h"
#include "nkn_vpe_mfp_ts.h"
#include "nkn_vpe_mfu_parse.h"
#include "nkn_vpe_h264_parser.h"
#include "mfp_ref_count_mem.h"
//#include "mfp_live_accum_ts.h"
#include "nkn_debug.h"
#include "nkn_vpe_utils.h"


#include "io_hdlr/mfp_io_hdlr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_TRACK_PER_CHANNEL		(5)

    typedef void (*register_formatter_err_t)(ref_count_mem_t *ref_cont);

    typedef enum zeri_stream_type_tt{
	    ZERI_VOD = 0,
	    ZERI_LIVE,
	    ZERI_LIVE_DVR 
    } zeri_stream_type_t;


    // utility function for retrieving adts data from AAC ADTS header
    typedef struct ADTS_data {

	uint8_t 	id;
	uint8_t 	mpeg_layer;
	uint8_t 	prot_ind;
	uint8_t 	profile_code;
	uint32_t 	sample_freq;
	uint8_t 	channel_count;
	uint16_t 	frame_length;
	uint8_t 	frame_count;

    } ADTS_data_t;

    //static ADTS_data_t* parse_adts(uint8_t const *stream);


    // **** PUBLISH state and handlers ****
    typedef struct mfu2seg_profile_state {

	uint8_t 	media_type[MAX_TRACK_PER_CHANNEL];	// 0 : Video, 1 : Audio
	uint32_t 	num_of_chunks[MAX_TRACK_PER_CHANNEL];
	uint32_t 	bit_rate;
	uint64_t 	total_duration;
	uint64_t 	current_base_ts[MAX_TRACK_PER_CHANNEL];
	uint32_t 	*exact_duration;

	//char* 		codec_data[MAX_TRACK_PER_CHANNEL];
	//uint32_t 	codec_data_len[MAX_TRACK_PER_CHANNEL];

	//zeri packager context - treated as void here
	void *		ctxt; //publ type specific ctx stored here
	uint32_t 	stream_id;
	//uint32_t 	seq_num[MAX_TRACK_PER_CHANNEL];

    } mfu2seg_profile_state_t;


    // converts mfu to segment and updates internal state on chunks, dur etc
    // internally uses util_mfu2seg for conversion

    struct fmtr_tt;

    typedef int32_t (*handle_mfu_fptr)(ref_count_mem_t *);

    typedef void (*delete_fptr)(struct fmtr_tt*);

    // DVR specific publish context
    typedef struct fmtr_dvr_ctx {

	int32_t chunk_wait_count;//used to skip initial 'x' publish requests
	// additional state var to be added based on need
    } fmtr_dvr_ctx_t;

    // LIVE specific publish context
    typedef struct fmtr_live_ctx {

	int32_t chunk_wait_count;//used to skip initial 'x' publish requests
	// additional state var to be added based on need
    } fmtr_live_ctx_t;


    // Possible Publish types
    typedef enum {
	FMT_DVR,
	FMT_LIVE
    } fmtr_publ_type_t;


    typedef struct fmtr_tt {

	uint32_t			num_profiles;
	char const* 			store_url;
	char const* 			delivery_url;
	fmtr_publ_type_t 		publ_type;
	
	uint32_t 			fragment_duration;
	uint32_t 			segment_duration;
	zeri_stream_type_t		stream_type;
	uint32_t			dvr_seconds;

	uint32_t 			encryption; //0 - off, 1 - on
	int8_t*  			common_key;
	int8_t*  			license_server_url;
	int8_t*  			license_server_cert;

	int8_t*  			transport_cert;
	int8_t*  			packager_cert;
	int8_t* 			credential_pwd;
	int8_t*  			policy_file;
	int8_t*  			content_id;

	uint32_t			orig_mod_req;

	// variable for performance measurement
	double 				fmtr_time_consumed;
	uint64_t 			fmtr_call_count;
	double 				fmtr_seg_write_time_consumed;
	uint64_t 			fmtr_seg_write_count;

	//function pointers
	handle_mfu_fptr 		handle_mfu;
	delete_fptr 			delete_ctx;
	register_formatter_err_t 	error_callback_fn;

	uint32_t			err_flag;
	uint32_t 			eos_flag;

	mfu2seg_profile_state_t*	profile[MAX_STREAM_PER_ENCAP];

    } fmtr_t;






#ifdef __cplusplus
}
#endif

#endif
