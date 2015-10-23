#include "mfp_video_header.h"


//create_index_table
//data - input - incoming buffer address of ts pkts
//n_ts_pkts - input - number of consecutive ts packets
//spts - input - accum->spts structure 
//pkt_tbl - output - packet index table

int32_t create_index_table(
			   uint8_t * data,
			   uint32_t *pkt_offsets,
			   uint32_t n_ts_pkts,
			   uint32_t aud_pid1,
			   uint32_t vid_pid,
			   ts_pkt_infotable_t **pkt_tbl)
{
    uint32_t i;
    h264_desc_t *h264_desc, *orig_h264_desc;
    slice_type_e slice_type;
    ts_pkt_infotable_t *pkt_tbl_l, *prev_pkt_tbl;
    int32_t ret = 0;
    uint32_t pkt_num = 0;

    //alloc mem
    h264_desc = (h264_desc_t *)nkn_calloc_type(sizeof(h264_desc_t),1,
					       mod_vpe_h264_parser_t);
    if(!h264_desc){
	printf("unable to allocate mem for index tables");
	ret = -1;
	return (ret);
    }
    memset(h264_desc->last_data, 0xff, 4);

    orig_h264_desc = h264_desc;
    *pkt_tbl = (ts_pkt_infotable_t *)nkn_calloc_type(
						     sizeof(ts_pkt_infotable_t), n_ts_pkts, mod_vpe_h264_parser_t);
    if(*pkt_tbl == NULL){
	printf("unable to allocate mem for index tables");
	ret = -1;
	return (ret);
    }
    pkt_tbl_l = *pkt_tbl;

    for(i = 0; i < n_ts_pkts; i++){

	//parse TS for pid and payload start indicator flag
	//if PSI flag is on, get DTS and PTS
	int32_t pos = 0;
	pos = mfu_converter_get_timestamp(
					  (uint8_t *)(data+ pkt_offsets[i]), NULL, 
					  &pkt_tbl_l->pid, 
					  &pkt_tbl_l->pts, 
					  &pkt_tbl_l->dts, NULL);	
	
	if(pos == -E_VPE_NO_SYNC_BYTE){
	    //printf("no sync byte\n");
	    ret= -1;
	    goto create_index_table_ret;
	}

	if(aud_pid1) {
	    if(pkt_tbl_l->pid != vid_pid
	       && pkt_tbl_l->pid != aud_pid1){
		pkt_tbl_l++;
		continue;
	    }
	} else {
	    if(pkt_tbl_l->pid != vid_pid) {
		pkt_tbl_l++;
		continue;
	    }
	}

	pkt_tbl_l->adaptation_field_control = \
	    ((*(data+ pkt_offsets[i] + 3)&0x30)>>4);
	pkt_tbl_l->continuity_counter = (*(data+ pkt_offsets[i] + 3)&0xf);

	pkt_tbl_l->header_len = pos;

	//deduce whether PSI flag is on or off
	pkt_tbl_l->payload_start_indicator_flag = 0;
	if(pkt_tbl_l->pts != 0 || pkt_tbl_l->dts != 0)
	    pkt_tbl_l->payload_start_indicator_flag = 1;

	//parse slice type for video packets alone
	if(pkt_tbl_l->pid == vid_pid){
	    pkt_num++;
	    //init
	    pkt_tbl_l->frame_start = 0;
	    pkt_tbl_l->slice_start = pos;
	    pkt_tbl_l->slice_len = TS_PKT_SIZE - pos;

	    if(pkt_tbl_l->payload_start_indicator_flag == 1) {
		h264_desc->frame_start = 1;
	    }
	    //parse
	    slice_type = nal_header_parse_pkt(
					      (uint8_t *)(data + pkt_offsets[i]), 
					      TS_PKT_SIZE, h264_desc, pkt_tbl_l);
	    if((h264_desc->span == SEI_SPAN) && (i > 0)) {
	    	assert(prev_pkt_tbl->num_SEI > 0);
		prev_pkt_tbl->SEI_length[prev_pkt_tbl->num_SEI-1] -= h264_desc->mod_length;
	    }
	    pkt_tbl_l->frame_type = slice_type;
	    prev_pkt_tbl = pkt_tbl_l;
	    
	}
	pkt_tbl_l++;
    }

create_index_table_ret:
    if(h264_desc){
	if (orig_h264_desc != h264_desc)
	    assert(0);
	
	free(h264_desc);
	h264_desc = NULL;
    }
    return(ret);
	
}	


//nal_header_parse_pkt
//data - input - packet start address
//len - length of the ts packet
//h264_desc - local continuum data structure to be used in get_nal_mem_delimiter

slice_type_e nal_header_parse_pkt(
				  uint8_t *data,
				  uint32_t len,
				  h264_desc_t *h264_desc,
				  ts_pkt_infotable_t *pkt_tbl){

    slice_type_e slice_type = slice_default_value;
    uint32_t ret; 
    uint32_t offset = pkt_tbl->header_len;
    //uint32_t update_slice_start = 0;
    uint32_t nal_flag = 0;
    uint32_t write_dump_size = 1;
    //uint32_t temp_dump_size = 0;
    uint32_t sc_type = 0;
       
    h264_desc->data_offset = 0;
    h264_desc->dump_size = 0;
    h264_desc->span = 0;
    int32_t upd_len = (int32_t)len - offset;
    //pkt_tbl->codec_info_start_flag = 0;
    //pkt_tbl->num_SEI = 0;
    //pkt_tbl->codec_info_len = 0;
    h264_desc->slice_start = pkt_tbl->slice_start;
    while(upd_len > 0){

	ret = get_nal_delimiter(
				data + offset, 
				(uint32_t *)&upd_len, 
				h264_desc);

	if(h264_desc->nal_type != -1)
          sc_type = h264_desc->sc_type;

	/*if the NALe delimiter spans across the pkts, then 
	  decrement the size of SEI or codec-info in the previous pkt */
	if(h264_desc->data_offset <= 4) {
	    if(h264_desc->nal_type != -1) {
		if(h264_desc->SEI_incomplete) {
		    h264_desc->span = SEI_SPAN;
		    h264_desc->SEI_incomplete = 0;
		}
		h264_desc->mod_length = sc_type - h264_desc->data_offset +1;
	    }
	    goto nal_type;
	}

	// for SEI
	if(h264_desc->SEI_incomplete) {
	    if(h264_desc->nal_type != -1) {
		if(h264_desc->nal_type == AVC_NALU_SEI) {
		    if(nal_flag == 0) {
			pkt_tbl->SEI_length[pkt_tbl->num_SEI] =
			    (h264_desc->data_offset - sc_type - 1);
		    } else {
			pkt_tbl->SEI_length[pkt_tbl->num_SEI] = h264_desc->data_offset -1;//+ NAL_DELIMITER_SIZE;
		    }
		} else {
		    if(nal_flag == 0) {
			pkt_tbl->SEI_offset[pkt_tbl->num_SEI] = offset;
			pkt_tbl->SEI_length[pkt_tbl->num_SEI] = h264_desc->data_offset - sc_type - 1;
		    } else {
			//pkt_tbl->SEI_offset[pkt_tbl->num_SEI] = offset;
			pkt_tbl->SEI_length[pkt_tbl->num_SEI] = h264_desc->data_offset -1;
		    }
		}
		pkt_tbl->num_SEI++;
		h264_desc->SEI_incomplete = 0;
	    } else {
		if(nal_flag == 0) {
		    pkt_tbl->SEI_offset[pkt_tbl->num_SEI] = offset;
		    pkt_tbl->SEI_length[pkt_tbl->num_SEI] =
			(h264_desc->data_offset- NAL_DELIMITER_SIZE -1);
		} else {
		    pkt_tbl->SEI_length[pkt_tbl->num_SEI] =
			(h264_desc->data_offset - 1);
		}
		pkt_tbl->num_SEI++;
	    }
	}

	nal_type:
	// for codec-info
	if(h264_desc->frame_start) {
	    if(h264_desc->codec_info_incomplete){
		if(h264_desc->nal_type == -1) {
		    if(nal_flag == 0) {
			pkt_tbl->codec_info_start_offset = offset;
			pkt_tbl->codec_info_len  += (h264_desc->data_offset);
		    } else {
		      pkt_tbl->codec_info_len  += (h264_desc->data_offset -1);// + NAL_DELIMITER_SIZE);
		    }
		} else {
		    if (h264_desc->nal_type != AVC_NALU_PIC_PARAM) {
			if(pkt_tbl->codec_info_len){
				//pps completion in same ts packet
				pkt_tbl->codec_info_len  += (h264_desc->data_offset- 1);
			}else{
				//pps is split across 2 packets
				pkt_tbl->codec_info_start_offset = offset;
				pkt_tbl->codec_info_len  += (h264_desc->data_offset- 1 - h264_desc->sc_type);
			}
			h264_desc->codec_info_incomplete = 0;
			h264_desc->codec_info_updated = 1;
			h264_desc->is_SEI_separate_vector = 1;
		    } else {
			//pkt_tbl->codec_info_len  += (h264_desc->data_offset - NAL_DELIMITER_SIZE);
		    }
		}
	    }
	} 

	h264_desc->data_offset -= 1;
	if(pkt_tbl->frame_start == 0) {
	    if(h264_desc->nal_type != AVC_NALU_SEI) {
		h264_desc->dump_size += h264_desc->data_offset;
	    }
	}

	switch(h264_desc->nal_type){

	    case -1:
		slice_type = slice_need_more_data;
		break;
			
	    case AVC_NALU_IDR_SLICE:
		if(h264_desc->frame_start) {
		    h264_desc->is_slice_start = 1;
		    h264_desc->is_SEI_separate_vector = 0;
		    slice_type = slice_type_i;
		    nal_flag = 1;
		} else {
		    write_dump_size = 1;
		}
		h264_desc->codec_info_updated = 0;
		break;
			
	    case AVC_NALU_NON_IDR_SLICE:
		if(h264_desc->frame_start) {
		    h264_desc->is_slice_start = 1;
		    h264_desc->is_SEI_separate_vector = 0;
		    slice_type = slice_type_p;
		    nal_flag = 1;
		} else {
		    write_dump_size = 1;
		}
		h264_desc->codec_info_updated = 0;
		break;
			
	    case AVC_NALU_SEI:
		slice_type = 0;
		/* fill all SEI before SPS has separate vector, SEI after sps is part of data */
		if((h264_desc->is_SEI_separate_vector == 0)) {
		    pkt_tbl->SEI_offset[pkt_tbl->num_SEI] = offset + 
			h264_desc->data_offset - sc_type;
		    h264_desc->SEI_incomplete = 1;
		    write_dump_size = 0;
		} else {
		    h264_desc->is_slice_start = 1;
		    write_dump_size = 0;
		}
		nal_flag = 1;
		break;
	    case AVC_NALU_ACCESS_UNIT:
		h264_desc->is_slice_start = 0;
		h264_desc->is_slice_start_assigned = 0;
		h264_desc->frame_start = 1;
		pkt_tbl->frame_start = 1;
		//h264_desc->codec_info_updated = 0;
		h264_desc->codec_info_start_flag = 0;
		slice_type = 0;
		nal_flag = 1;
		write_dump_size = 0;
		sc_type = h264_desc->sc_type;
		break;
	    case AVC_NALU_SEQ_PARAM:
	      if((h264_desc->frame_start) && (h264_desc->codec_info_updated == 0)){
		    pkt_tbl->codec_info_start_offset = offset	\
			+ h264_desc->data_offset - sc_type;
		    h264_desc->codec_info_start_flag = 1;
		    h264_desc->codec_info_incomplete = 1;
		    slice_type = 0;
		    nal_flag = 1;
		} else {
		    write_dump_size = 1;
		}
		break;
	    case AVC_NALU_PIC_PARAM:
	      if((h264_desc->frame_start) && (h264_desc->codec_info_updated == 0)){
		    if(h264_desc->codec_info_start_flag) {
			if(nal_flag == 0) {
			    pkt_tbl->codec_info_start_offset = offset;
			    pkt_tbl->codec_info_len  += (h264_desc->data_offset - sc_type);
			    h264_desc->codec_info_incomplete = 1;
			}else {
			    pkt_tbl->codec_info_len  += (h264_desc->data_offset);// - h264_desc->sc_type);
			    h264_desc->codec_info_incomplete = 1;
			}
		    } else {
		      pkt_tbl->codec_info_start_offset = offset	
			+ h264_desc->data_offset - sc_type;
		      h264_desc->codec_info_start_flag = 1;
		      h264_desc->codec_info_incomplete = 1;
		      slice_type = 0;
		      nal_flag = 1;
		    }
		    slice_type = 0;
		    nal_flag = 1;
		} else {
		    write_dump_size = 1;
		}
		break;
	    default:
		//search through
		slice_type = 0;
		break;
	}

#if 1
	if(h264_desc->is_slice_start) {
	    if(h264_desc->is_slice_start_assigned == 0) {
		if(nal_flag != 0) {
		    h264_desc->slice_start = offset + h264_desc->data_offset -4;
		    h264_desc->is_slice_start_assigned = 1;
		}
	    }
	}
#endif
	offset = offset + h264_desc->data_offset;
	upd_len = upd_len - h264_desc->data_offset;

	if( slice_type != 0 )
	    break;
    }
    pkt_tbl->slice_len = 188 - h264_desc->slice_start;
    pkt_tbl->is_slice_start = h264_desc->is_slice_start;
    pkt_tbl->slice_start = h264_desc->slice_start;
    pkt_tbl->dump_size = h264_desc->dump_size - sc_type;
    return (slice_type); 
}


slice_type_e find_frame_type(
			     uint32_t n_ts_pkts,
			     uint32_t i,
			     uint16_t vpid,
			     ts_pkt_infotable_t *pkt_tbl,
			     uint32_t *frame_type_num){
	
    slice_type_e frame_type;
    uint32_t j;
	
    frame_type = pkt_tbl[i].frame_type;
    if(frame_type ==  slice_need_more_data || frame_type == slice_default_value){
	//check whether it is frame start
	if(pkt_tbl[i].frame_start == 1){
	    //search till n_ts_pkts for correct frame type
	    for(j = i + 1; j < n_ts_pkts; j++){
		if(pkt_tbl[j].pid == vpid){
		    frame_type = pkt_tbl[j].frame_type;
		}
		if(frame_type > slice_default_value) {
		    *frame_type_num = j;
		    break;
		}
	    }
	} else {
	    *frame_type_num = i;
	}
    } else {
	*frame_type_num = i;
    }
    return(frame_type);
}


