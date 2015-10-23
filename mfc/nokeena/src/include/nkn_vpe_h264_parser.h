#ifndef NKN_VPE_H264_PARSER_H
#define NKN_VPE_H264_PARSER_H


#ifdef MS_DEV
#include "inttypeswin.h"
#include "main.h"
#else
#include "inttypes.h"
#endif


#include <stdio.h>
#include <stdint.h>
//SPS parser related includes
#include <math.h>
#include "nkn_vpe_bitio.h"

//#define MAX_SEI 5
#define NAL_BUFFER_SIZE 4096
#define SEI_SPAN 5

#define NAL_SIZE 4

#define AVC_NALU_NON_IDR_SLICE 0x1
#define AVC_NALU_DP_A_SLICE 0x2
#define AVC_NALU_DP_B_SLICE 0x3
#define AVC_NALU_DP_C_SLICE 0x4
#define AVC_NALU_IDR_SLICE 0x5
#define AVC_NALU_SEI 0x6
#define AVC_NALU_SEQ_PARAM 0x7
#define AVC_NALU_PIC_PARAM 0x8
#define AVC_NALU_ACCESS_UNIT 0x9
#define AVC_NALU_END_OF_SEQ 0xa
#define AVC_NALU_END_OF_STREAM 0xb
#define AVC_NALU_FILLER_DATA 0xc
#define AVC_NALU_SVC_PACKET 0xe
#define AVC_NALU_SVC_SLICE 0x14

typedef struct h264_desc_tt{
  uint32_t data_offset;
  uint32_t delimiter_found;
  int32_t sc_type;
  int32_t nal_type;
  uint8_t last_data[4];
  uint32_t frame_start;
  uint32_t codec_info_start_flag;
  uint32_t SEI_incomplete; /*whether SEI in the current pkt is completed */
  uint32_t is_slice_start;
  uint32_t codec_info_incomplete;
  uint32_t slice_start;
  uint32_t is_SEI_separate_vector;
  uint32_t is_slice_start_assigned;
  uint32_t dump_size;
  uint32_t span;
  uint32_t mod_length;
  uint32_t codec_info_updated;
}h264_desc_t;


/*Function declaration*/
uint32_t handle_mpeg4AVC(uint8_t* data, uint32_t *input_size);
uint32_t get_nal_delimiter(uint8_t* data, uint32_t *input_size, h264_desc_t *h264_desc);
uint32_t handle_mpeg4AVC_bounded(uint8_t* data, uint32_t *input_size);
int get_nal_type(uint8_t *, int );
int get_nal_mem(uint8_t *, uint32_t);
int find_start_code(uint8_t *);


// SPS Parser 

struct nkn_vpe_sps;

typedef uint32_t (*getPictureWidth_fptr)(struct nkn_vpe_sps const*);

typedef uint32_t (*getPictureHeight_fptr)(struct nkn_vpe_sps const*);

typedef void (*deleteSPS_Parser_Ptr)(struct nkn_vpe_sps*);

typedef struct nkn_vpe_sps {

	uint8_t profile_idc;
	uint8_t constraint_flags;
	uint8_t level_idc;
	uint32_t seq_param_set_id;

	uint32_t chroma_format_idc;
	uint8_t residual_colour_transform_flag;
	uint32_t bit_depth_luma_minus8;
	uint32_t bit_depth_chroma_minus8;
	uint8_t qpprime_y_zero_transform_bypass_flag;
	uint8_t seq_scaling_matrix_present_flag;
	uint8_t seq_scaling_list_present_flag[8];
	
	uint32_t max_frame_num;
	uint32_t pic_order_cnt_type;
	uint32_t max_pic_order_cnt;
	uint8_t delta_pic_order_always_zero_flag;

	// ref picture related details
	int32_t offset_for_non_ref_pic;
	int32_t offset_for_top_to_bottom_field;
	uint32_t num_ref_frames_in_pic_order_cnt_cycle;
	int32_t* offset_for_ref_frame;
	uint32_t num_ref_frames;
	uint8_t gaps_in_frame_num_value_allowed_flag;

	// heigth and width details
	uint32_t pic_width_in_mbs;
	uint32_t pic_height_in_map_units;


	uint8_t frame_specific_flags;
	/*
	msb 0 frame_mbs_only_flag;
	msb 1 mb_adaptive_frame_field_flag;
	msb 2 direct_8x8_inference_flag;
	msb 3 frame_cropping_flag;
	msb 4 -7 : unused
	*/


	uint32_t frame_crop_left_offset;
	uint32_t frame_crop_right_offset;
	uint32_t frame_crop_top_offset;
	uint32_t frame_crop_bottom_offset;

	uint8_t vui_parameters_present_flag;

	getPictureWidth_fptr get_picture_width;
	getPictureHeight_fptr get_picture_height;

	//function ptr for delete
	deleteSPS_Parser_Ptr delete_sps_parser;
} nkn_vpe_sps_t;


nkn_vpe_sps_t* parseSPS(char * sps_ptr, uint32_t sps_len);


// End SPS Parser


#endif
