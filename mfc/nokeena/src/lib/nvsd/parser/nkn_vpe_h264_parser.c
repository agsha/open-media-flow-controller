#include "nkn_vpe_h264_parser.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "nkn_memalloc.h"


/*This function is used to indicatewhether nal delimiter is
  present in the data, it also returns the nal_type, slice_type
  @data->input pointing to the data tobe searched
  @input_size->size of the data tobe searched
  @h264_desc->o/p with updated nal_type,found nal delimiter
  return is the slice type */

uint32_t get_nal_delimiter(uint8_t* data, uint32_t *input_size,
			     h264_desc_t *h264_desc)
{
  uint8_t *temp_data = NULL;
  uint32_t /*initial_data_offset = 0,*/ act_data_size = 0;
  uint32_t data_size = 0, extra_size = 4, search_window = 0;
  uint32_t data_offset = 0, i = 0;
  uint8_t *initial_data = data, *nal_data = NULL;
  int sc_type = 0, nal_type = 0;
  uint32_t slice_type = 0,data_pos = 0;
  act_data_size = *input_size;
  data_size = act_data_size + extra_size;
  /* copying the last fout bytes into songle buffer */

  temp_data = (uint8_t *)nkn_calloc_type(data_size + extra_size, sizeof(uint8_t), mod_vpe_h264_parser_t);

  temp_data[0] = h264_desc->last_data[0];
  temp_data[1] = h264_desc->last_data[1];
  temp_data[2] = h264_desc->last_data[2];
  temp_data[3] = h264_desc->last_data[3];
  //pad the end with four 0xffs to avoid wrong NAL detection
  temp_data[data_size + 0] = 0xff;
  temp_data[data_size + 1] = 0xff;
  temp_data[data_size + 2] = 0xff;
  temp_data[data_size + 3] = 0xff;  
  memcpy(temp_data+extra_size, data, act_data_size);
  data = temp_data;

  search_window = act_data_size + extra_size;
  i = 0;
  do {
    sc_type = find_start_code(data+i);
  } while (sc_type == -1 && (++i) < search_window);
  if(sc_type != -1) {
    //h264_desc->delimiter_found = 1;
    h264_desc->sc_type = sc_type;
    data += (sc_type+i);
    data_pos = sc_type+i;
    data_offset = data_offset + sc_type+i;
    nal_data = data;
    if(data_offset < data_size) {
      nal_type = get_nal_type(nal_data, 0);
      h264_desc->delimiter_found = 1;
      h264_desc->nal_type = nal_type;
      data_pos++;
      switch(nal_type){
      case AVC_NALU_ACCESS_UNIT:
	slice_type =1;
	break;
      case AVC_NALU_SEI:
	slice_type = 1;
	break;
      case AVC_NALU_SEQ_PARAM:
	slice_type = 1;
	break;
      case AVC_NALU_PIC_PARAM:
	slice_type = 1;
	break;
      case AVC_NALU_IDR_SLICE:
	slice_type= 1;
	break;
      default:
	slice_type= 0;
	break;
      }
    }
  } else {
    h264_desc->delimiter_found = 0;
    h264_desc->sc_type = -1;
    data_pos = data_size;
    h264_desc->nal_type = -1;
  }
  if(h264_desc->delimiter_found == 0) {
    if(act_data_size < extra_size) {
      for(i = 0; i< act_data_size; i++) {
	h264_desc->last_data[0] = h264_desc->last_data[1];
	h264_desc->last_data[1] = h264_desc->last_data[2];
	h264_desc->last_data[2] = h264_desc->last_data[3];
	h264_desc->last_data[3] = initial_data[i];
	
      }
      //memcpy(h264_desc->last_data+(extra_size-act_data_size), initial_data, act_data_size);
    } else {
      h264_desc->last_data[0] = initial_data[act_data_size-extra_size+0];
      h264_desc->last_data[1] = initial_data[act_data_size-extra_size+1];
      h264_desc->last_data[2] = initial_data[act_data_size-extra_size+2];
      h264_desc->last_data[3] = initial_data[act_data_size-extra_size+3];
      //memcpy(h264_desc->last_data, initial_data+act_data_size-extra_size, extra_size);
    }
  } else {
    h264_desc->last_data[3] = 0xff;
    h264_desc->last_data[2] = 0xff;
    h264_desc->last_data[1] = 0xff;
    h264_desc->last_data[0] = 0xff;
  }
  h264_desc->data_offset = data_pos-sc_type;
  if(temp_data != NULL)
    free(temp_data);
  return slice_type;
}

uint32_t handle_mpeg4AVC(uint8_t* data, uint32_t *input_size){
    uint32_t slice_type,nal_size,inp_size;
    uint8_t *nal_data;
    int nal_type,sc_type;
    inp_size=*input_size;
    /**/
    do{
	sc_type = find_start_code(data);
	data+=sc_type;
	*input_size=(*input_size)+sc_type;
	nal_size = get_nal_mem(data, 188-(*input_size));
	nal_data = data;
	data+=nal_size;
	*input_size=(*input_size)+nal_size;
	nal_type = get_nal_type(nal_data, nal_size);
	
	switch(nal_type){
	    case AVC_NALU_IDR_SLICE:
		slice_type= 1;
		break;
	    case AVC_NALU_NON_IDR_SLICE:
		slice_type= 2;
		break;
	    case -1:
		slice_type = 5;
		break;
	    case AVC_NALU_ACCESS_UNIT:
		//		slice_type = (nal_data[1]>>5)&0x07;
		//		printf("Aud = %d",slice_type);
		//		if(slice_type == 0)
		//  slice_type = 1;
		//		else
		    slice_type = 0;
		break;

	    default:
		slice_type= 0;
		break;
	}
    }while(slice_type==0);
    return slice_type;
}

uint32_t handle_mpeg4AVC_bounded(uint8_t* data, uint32_t *input_size)
{
    uint32_t slice_type,nal_size,inp_size;
    uint8_t *nal_data;
    int nal_type,sc_type;
    inp_size=*input_size;
    /**/
    sc_type = find_start_code(data);
    data+=sc_type;
    *input_size=(*input_size)+sc_type;
    nal_size = get_nal_mem(data, 188-(*input_size));
    nal_data = data;
    data+=nal_size;
    *input_size=(*input_size)+nal_size;
	
    nal_type = get_nal_type(nal_data, nal_size);

    switch(nal_type){
	case AVC_NALU_IDR_SLICE:
	    slice_type= 1;
	    break;
	case AVC_NALU_NON_IDR_SLICE:
	    slice_type= 2;
	    break;
	default:
	    slice_type= 0;
	    break;
    }
    return slice_type;
}

int find_start_code(uint8_t *data)
{
    uint8_t b1, b2, b3, b4;

    b1=*data;
    b2=data[1];
	
    if(b1==0 && b2==0){
	b3=data[2];
	if(b3==1) return 3;
	b4=data[3];
	if(b3==0 && b4==1) return 4;
    }
    return -1;
}

int get_nal_type(uint8_t *nal_data, int nal_size)
{

    uint8_t *p_nal_data;
    int nal_type;

    nal_type = -1;
    p_nal_data = nal_data;
    nal_size = nal_size;
	
    nal_type = p_nal_data[0] & 0x1F;

    if(nal_type ==AVC_NALU_SVC_PACKET || nal_type == AVC_NALU_SVC_SLICE){
	return -1;
    }

    return nal_type;
}


int get_nal_mem(uint8_t *p_data, uint32_t input_size)
{

    uint8_t *buf;
    uint32_t start, end, bytes_processed;
    uint32_t packet_delimiter, curr_pos, data_boundary;
    uint8_t *p_buf, *p_end;
	
	
    start = 0;
    end = 0;
    bytes_processed = 0;
    data_boundary = 0;
    packet_delimiter = 0xffffffff;
    p_end = p_buf = buf = NULL;
    buf = p_data;
	
    curr_pos = start = 0;//iof->nkn_tell(in_desc);
	
	
    do{

	if(bytes_processed%NAL_BUFFER_SIZE == 0){
	    int bytes_to_read;
	    if((curr_pos+NAL_BUFFER_SIZE) < input_size){
		bytes_to_read = NAL_BUFFER_SIZE;
								
	    }else{
		bytes_to_read = input_size - curr_pos;
		data_boundary = input_size-curr_pos;
	    }
	    p_buf = buf;
	    buf+=bytes_to_read;
	}

	packet_delimiter = ((p_buf[0]) << 24) | ((p_buf[1]) << 16) |
	    ((p_buf[2]) << 8) | ((p_buf[3]));
	bytes_processed+=1;
	p_buf++;
	data_boundary--;
	if(packet_delimiter == 0x00000001){//0x00000001 end code
	    end = start+(bytes_processed+3)-4;
	}
	if( (packet_delimiter & 0xffffff00) == 0x100){//0x000001 end code
	    end = start+(bytes_processed+2)-3;
	}
    }while(!end && data_boundary);

    end = end + (end==0)*input_size;
    return end-start;
}
	

// SPS Parser


static void deleteSPS_Parser(nkn_vpe_sps_t *);

static uint32_t ue_golomb(bitstream_state_t* bit_buf);


static int32_t se_golomb(bitstream_state_t* bit_buf);

static uint32_t getPictureWidth(nkn_vpe_sps_t const* sps);

static uint32_t getPictureHeight(nkn_vpe_sps_t const* sps);

nkn_vpe_sps_t* parseSPS(char* sps_ptr, uint32_t sps_len) {


	nkn_vpe_sps_t* sps_param = nkn_calloc_type(1, sizeof(nkn_vpe_sps_t), mod_vpe_h264_parser_t);
	if (sps_param == NULL)
		return NULL;
	sps_param->offset_for_ref_frame = NULL;

	sps_param->profile_idc  = *(unsigned char*)(sps_ptr + 0); 
	sps_param->constraint_flags  = *(unsigned char*)(sps_ptr + 1); 
	sps_param->level_idc  = *(unsigned char*)(sps_ptr + 2);
	bitstream_state_t* bit_buf = bio_init_bitstream((uint8_t*)(sps_ptr + 3),
			(size_t)sps_len);
	if (bit_buf == NULL) {
		free(sps_param);
		return NULL;
	}

	sps_param->seq_param_set_id = ue_golomb(bit_buf);
	if(sps_param->profile_idc == 100 || sps_param->profile_idc == 110 || 
		sps_param->profile_idc == 122 || sps_param->profile_idc == 144){
		sps_param->chroma_format_idc = ue_golomb(bit_buf);
		if(sps_param->chroma_format_idc == 3)
			sps_param->residual_colour_transform_flag = 
					bio_read_bit(bit_buf);
		sps_param->bit_depth_luma_minus8 = ue_golomb(bit_buf);
		sps_param->bit_depth_chroma_minus8 = ue_golomb(bit_buf);
		sps_param->qpprime_y_zero_transform_bypass_flag = 
				bio_read_bit(bit_buf);
		sps_param->seq_scaling_matrix_present_flag = 
				bio_read_bit(bit_buf);
		if(sps_param->seq_scaling_matrix_present_flag){
			uint32_t i;
			for(i = 0; i < 8; i++){
				sps_param->seq_scaling_list_present_flag[i] =
				bio_read_bit(bit_buf);	
			}
		}		
	}
	sps_param->max_frame_num = ue_golomb(bit_buf);
	sps_param->max_frame_num = pow(2, sps_param->max_frame_num + 4);
	sps_param->pic_order_cnt_type = ue_golomb(bit_buf);
	if (sps_param->pic_order_cnt_type == 0) {
		sps_param->max_pic_order_cnt = ue_golomb(bit_buf);
		sps_param->max_pic_order_cnt =
			pow(2, sps_param->max_pic_order_cnt + 4); 
	} else if (sps_param->pic_order_cnt_type == 1) {

		sps_param->delta_pic_order_always_zero_flag =
			bio_read_bit(bit_buf);
		sps_param->offset_for_non_ref_pic = se_golomb(bit_buf);
		sps_param->offset_for_top_to_bottom_field = se_golomb(bit_buf); 
		sps_param->num_ref_frames_in_pic_order_cnt_cycle =
			ue_golomb(bit_buf);

		sps_param->offset_for_ref_frame = nkn_calloc_type(
			sps_param->num_ref_frames_in_pic_order_cnt_cycle,
			sizeof(int32_t), mod_vpe_h264_parser_t);
		if (sps_param->offset_for_ref_frame == NULL) {
			free(bit_buf);
			free(sps_param);
			return NULL;
		}
		uint32_t i = 0;
		for (; i < sps_param->num_ref_frames_in_pic_order_cnt_cycle;i++)
			sps_param->offset_for_ref_frame[i] = se_golomb(bit_buf);
	}
	sps_param->num_ref_frames = ue_golomb(bit_buf);
	sps_param->gaps_in_frame_num_value_allowed_flag = bio_read_bit(bit_buf);
	sps_param->pic_width_in_mbs = ue_golomb(bit_buf) + 1;
	sps_param->pic_height_in_map_units = ue_golomb(bit_buf) + 1;

	sps_param->frame_specific_flags = bio_read_bit(bit_buf);
	if (!sps_param->frame_specific_flags)
		sps_param->frame_specific_flags |= (bio_read_bit(bit_buf) << 1);

	sps_param->frame_specific_flags |= (bio_read_bit(bit_buf) << 2);
	sps_param->frame_specific_flags |= (bio_read_bit(bit_buf) << 3);
 
	if (sps_param->frame_specific_flags & 0x08) {

		sps_param->frame_crop_left_offset = ue_golomb(bit_buf);
		sps_param->frame_crop_right_offset = ue_golomb(bit_buf);
		sps_param->frame_crop_top_offset = ue_golomb(bit_buf);
		sps_param->frame_crop_bottom_offset = ue_golomb(bit_buf);
	}
	sps_param->vui_parameters_present_flag = bio_read_bit(bit_buf);

	sps_param->delete_sps_parser = deleteSPS_Parser;
	sps_param->get_picture_width = getPictureWidth;
	sps_param->get_picture_height = getPictureHeight;
	free(bit_buf);
	return sps_param;
}


static void deleteSPS_Parser(nkn_vpe_sps_t* sps) {

	if (sps->offset_for_ref_frame != NULL)
		free(sps->offset_for_ref_frame);
	free(sps);
}


static uint32_t ue_golomb(bitstream_state_t* bit_buf) {

	uint32_t leading_zero_count = 0;
	while (bio_read_bit(bit_buf) == 0)
		leading_zero_count++;
	uint32_t val = 0, i = 0;
	uint32_t tmp = 0;
	for (; i < leading_zero_count; i++) {

		tmp = bio_read_bit(bit_buf);
		val |= (tmp << (leading_zero_count - (i + 1)));
	}
	return (pow(2, leading_zero_count) - 1 + val);
}


static int32_t se_golomb(bitstream_state_t* bit_buf) {

	// our test cases, did not call this functions: UNTESTED
	uint32_t code_num = ue_golomb(bit_buf);
	return pow(-1, (code_num + 1)) * ceil(code_num / 2); 
}


static uint32_t getPictureWidth(nkn_vpe_sps_t const* sps) {

	return (sps->pic_width_in_mbs + sps->frame_crop_left_offset +
			sps->frame_crop_right_offset) * 16;
}


static uint32_t getPictureHeight(nkn_vpe_sps_t const* sps) {

	return (sps->pic_height_in_map_units + sps->frame_crop_top_offset +
			sps->frame_crop_bottom_offset) * 16;
}


#ifdef UNIT_TEST_SPS_PARSE

int main() {

	char test_sps[] = { 0x42, 0xC0, 0x15, 0x96, 0x54, 0x04, 0x81,
		0x4B, 0x2E, 0x80 };
	/*
	char test_sps[] = {0x42, 0x00, 0x1E, 0x96, 0x56, 0x05, 0x01, 0xE8, 0x80,
		0x01, 0x00, 0x04 };
		*/
	nkn_vpe_sps_t* sps_param = parseSPS(&(test_sps[0]), 12);
	printf("seq_parameter_set_id: %u\n", sps_param->seq_param_set_id);
	printf("max_frame_num: %u\n", sps_param->max_frame_num);
	printf("pic_order_cnt_type: %u\n", sps_param->pic_order_cnt_type);
	printf("max_pic_order_cnt: %u\n", sps_param->max_pic_order_cnt);
	printf("num_ref_frames: %u\n", sps_param->num_ref_frames);
	printf("gaps_in_frame_num_value_allowed_flag: %u\n",
			sps_param->gaps_in_frame_num_value_allowed_flag);
	printf("SPS profile width: %u\n", sps_param->pic_width_in_mbs);
	printf("SPS profile heigth: %u\n", sps_param->pic_height_in_map_units);
	printf("frame specific flags: %u\n", sps_param->frame_specific_flags);
	printf("frame_crop_left_offset: %u\n",
			sps_param->frame_crop_left_offset);
	printf("frame_crop_right_offset: %u\n",
			sps_param->frame_crop_right_offset);
	printf("frame_crop_top_offset: %u\n",
			sps_param->frame_crop_top_offset);
	printf("frame_crop_bottom_offset: %u\n",
			sps_param->frame_crop_bottom_offset);
	printf("vui_parameters_present_flag: %u\n",
			sps_param->vui_parameters_present_flag);

	sps_param->delete_sps_parser(sps_param);
	return 0;
}

#endif
// End SPS Parser

