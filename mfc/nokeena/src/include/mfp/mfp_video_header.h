#ifndef _VIDEO_HEADER_H_
#define _VIDEO_HEADER_H_


#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
//#include "mfp_live_accum_ts.h"
#include "nkn_vpe_parser_err.h"
#include "nkn_vpe_h264_parser.h"
#include "nkn_memalloc.h"
#include "nkn_vpe_mfp_ts2mfu.h"

#define NAL_DELIMITER_SIZE 4

typedef enum slice_type_ee {
	slice_need_more_data = -1,
	slice_default_value = 0,
	slice_type_i = 1,
	slice_type_p = 2,
	slice_type_b = 3
}slice_type_e;

typedef struct ts_pkt_infotable_tt{

	uint16_t pid;
	uint32_t payload_start_indicator_flag;
	uint32_t adaptation_field_control;
	uint32_t continuity_counter;
	uint32_t dts;
	uint32_t pts;
	uint32_t frame_start;
	slice_type_e frame_type;
	uint32_t header_len;/*length of TS and PES headers*/
	uint32_t slice_start;/*start offset of given sub frame in current packet */
	uint32_t slice_len;  /*length of sub frame in current TS packet*/
	uint32_t codec_info_start_offset;
	uint32_t codec_info_len;
        uint32_t num_SEI;// number of SEI within one pkt
        uint32_t SEI_offset[MAX_SEI];/* SEI offset's within one pkt */
        uint32_t SEI_length[MAX_SEI];/* SEI length */
  uint32_t is_slice_start;
  uint32_t i_frame_present;
  uint32_t dump_size;
}ts_pkt_infotable_t;


int32_t create_index_table(
	uint8_t * data,
	uint32_t *pkt_offsets,
	uint32_t n_ts_pkts,
	uint32_t audio_pid1,
	uint32_t video_pid,
	ts_pkt_infotable_t **pkt_tbl);

slice_type_e nal_header_parse_pkt(
	uint8_t *data,
	uint32_t len,
	h264_desc_t *h264_desc,
	ts_pkt_infotable_t *pkt_tbl);

slice_type_e find_frame_type(
	uint32_t n_ts_pkts,
	uint32_t i,
	uint16_t vpid,
	ts_pkt_infotable_t *pkt_tbl,
	uint32_t *frame_pkt_num);


#ifdef __cplusplus
}
#endif


#endif

