#ifndef _LATM_PARSER_H_

#define _LATM_PARSER_H_

#ifdef __cplusplus
extern "C" {
#endif


#define MAX_NUM_PROGRAMS (16)
#define MAX_NUM_LAYERS (8)
#define MAX_NUM_CHUNKS	(16)
#define MAX_NUM_SUBFRAMES (64)
#define MAX_NUM_STREAMS   (MAX_NUM_PROGRAMS * MAX_NUM_LAYERS)

#define MAX_TIMESTAMPS_IN_LATMCHUNK (50)

typedef struct GetBitContext_TT {
    const uint8_t *buffer, *buffer_end;
    int index;
    int size_in_bits;
    int size_in_bits_plus8;
} GetBitContext;



#include "rtp_mpeg4_es.h"

typedef struct latm_payload{
	uint8_t *payload;
	uint32_t payload_filllen;
}latm_payload_t;

typedef struct dbg_res{
	uint32_t	    num_valid_strms;
	uint32_t		num_payloadrd_pass;
	uint32_t	    num_payloadrd_fail;
	uint32_t		num_payload_size0;
}dbg_res_t;

typedef struct out_vector_tt{
    uint8_t *data;
    uint32_t offset;
    uint32_t length;
}out_vector_t;

typedef struct LATMContext {
    //AACContext      aac_ctx;             ///< containing AACContext
    m4a_decoder_specific_info_t *m4a_dec_si[MAX_NUM_LAYERS];
    int             initialized;         ///< initilized after a valid extradata was seen

    // parser data
    uint32_t 		streamCnt;
    int             audio_mux_version_A; ///< LATM syntax version
    uint32_t        frame_length_type[MAX_NUM_STREAMS];   ///< 0/1 variable/fixed frame length
    uint32_t        frame_length[MAX_NUM_STREAMS];        ///< frame length for fixed frame length
    uint32_t        numPrograms;
    uint32_t 	    numLayer[MAX_NUM_PROGRAMS];
    uint32_t 	    progSIndx[MAX_NUM_STREAMS];
    uint32_t 	    laySIndx[MAX_NUM_STREAMS];
    uint32_t 	    streamID[MAX_NUM_PROGRAMS][MAX_NUM_LAYERS];
    uint32_t 	    allStreamSameTimeFraming;
    uint32_t 	    numSubFrames;
    uint32_t  	    latmBufferFullness[MAX_NUM_STREAMS];
    uint32_t 	   	CELPframeLengthTableIndex[MAX_NUM_STREAMS];
    uint32_t	   	HVXCframeLengthTableIndex[MAX_NUM_STREAMS];
    uint32_t 	   	coreFrameOffset;
    uint32_t 	   	MuxSlotLengthBits[MAX_NUM_SUBFRAMES][MAX_NUM_STREAMS];
    uint32_t 	   	MuxSlotLengthCoded[MAX_NUM_SUBFRAMES][MAX_NUM_STREAMS];
    uint32_t	   	AuEndFlag[MAX_NUM_STREAMS];
    uint32_t 	   	numChunks;    
    uint32_t	   	progCIndx[MAX_NUM_CHUNKS];
    uint32_t	   	layCIndx[MAX_NUM_CHUNKS];
	latm_payload_t	latm_payload[MAX_NUM_SUBFRAMES][MAX_NUM_STREAMS];
	out_vector_t	audio_vector[MAX_NUM_SUBFRAMES][MAX_NUM_STREAMS];

	uint32_t 		otherDataLengthBits;
	uint32_t		PTS_Value[MAX_TIMESTAMPS_IN_LATMCHUNK];
	uint32_t		DTS_Value[MAX_TIMESTAMPS_IN_LATMCHUNK];
	uint32_t		sampleDuration[MAX_TIMESTAMPS_IN_LATMCHUNK];
	uint32_t		compositeDuration[MAX_TIMESTAMPS_IN_LATMCHUNK];
	uint32_t		TotTimestampCnt;

   	uint8_t 		*mux_element;
	uint32_t 		mux_element_wr_pos;
	uint32_t 		mux_element_length;	
    dbg_res_t 		*mux_element_rpt;
}LATMContext_t;

/**
 * Required number of additionally allocated bytes at the end of the input bitstream for decoding.
 * This is mainly needed because some optimized bitstream readers read
 * 32 or 64 bit at once and could read over the end.<br>
 * Note: If the first 23 bits of the additional bytes are not 0, then damaged
 * MPEG bitstreams could cause overread and segfault.
 */
#define FF_INPUT_BUFFER_PADDING_SIZE 16


void print_hex_buffer(uint8_t *data, uint32_t count, uint8_t *name);

int32_t free_latmctx_mem(
	struct LATMContext **latmctx,
	uint32_t num_latmmux_elements);


void init_get_bits(GetBitContext *s, const uint8_t *buffer,
                                 int bit_size);

int parse_loas_latm(
	uint8_t *data, 
	uint32_t *slve_pkts, 
	uint32_t slve_npkts,
	struct LATMContext **latmctx,
	uint32_t *num_latmmux_elements);

#ifdef __cplusplus
}
#endif
#endif
