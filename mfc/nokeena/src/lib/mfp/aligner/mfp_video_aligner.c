#include "mfp_video_aligner.h"


//1 function declarations

static aligner_retcode_e create_init_frame_mem(
	video_frame_info_t **cur_frame);

static aligner_retcode_e create_frame_mem(
	video_frame_info_t *cur_frame);

static aligner_retcode_e create_init_pkt_mem(
	ts_pkt_info_t **cur_pkt);

static aligner_retcode_e create_pkt_mem(
	ts_pkt_info_t *cur_pkt);

static aligner_retcode_e delete_pkt_mem(
	ts_pkt_info_t *cur_pkt);


static aligner_retcode_e delete_frame_mem(
	aligner_ctxt_t *aligner_ctxt,
	video_frame_info_t *cur_frame);

static aligner_retcode_e delete_aligner_ctxt(
	aligner_ctxt_t *aligner_ctxt);

static aligner_retcode_e process_video_pkts(
	aligner_ctxt_t *aligner_ctxt,
	uint8_t *data,
	uint32_t *mstr_pkts,
	uint32_t n_mstr_pkts,
	spts_st_ctx_t *spts,
	video_frame_info_t **video_frame_tbl,
	uint32_t *num_frames
	);

static aligner_retcode_e create_mem(
	aligner_ctxt_t *aligner_ctxt,
	video_frame_info_t **video_frame_mem);


static aligner_retcode_e print_frame_info(
	aligner_ctxt_t *aligner_ctxt,
	video_frame_info_t *video_frame_tbl,
	FILE *fp,
	accum_ts_t *accum);

//1 function definitions

static aligner_retcode_e create_init_frame_mem(
	video_frame_info_t **cur_frame_ptr){

	video_frame_info_t *cur_frame;
	*cur_frame_ptr = NULL;
	cur_frame = nkn_calloc_type(sizeof(video_frame_info_t), 1, \
		mod_vpe_mfp_aligner_t);
	if(!cur_frame){
#if !defined(CHECK)
	   DBG_MFPLOG("Aligner", ERROR, MOD_MFPLIVE,\
	   	"Aligner failed to allocate memory for first frame");
#endif
	   return aligner_frame_memalloc_fail;	   
	}

	cur_frame->prev = cur_frame;
	cur_frame->next = NULL;
	*cur_frame_ptr = cur_frame;
   	return aligner_success;
}

static aligner_retcode_e create_frame_mem(
	video_frame_info_t *cur_frame){

	video_frame_info_t *next_frame;
	next_frame = nkn_calloc_type(sizeof(video_frame_info_t), 1, \
		mod_vpe_mfp_aligner_t);
	if(!next_frame){
#if !defined(CHECK)
	   DBG_MFPLOG("Aligner", ERROR, MOD_MFPLIVE,\
	   	"Aligner failed to allocate memory for next frame");
#endif
	   return aligner_frame_memalloc_fail;	   
	}

	next_frame->prev = cur_frame;
	next_frame->next = NULL;
	cur_frame->next = next_frame;
	
   	return aligner_success;
}

static aligner_retcode_e create_init_pkt_mem(
	ts_pkt_info_t **cur_pkt_ptr){

	ts_pkt_info_t *cur_pkt;
	cur_pkt = nkn_calloc_type(sizeof(ts_pkt_info_t), 1, \
		mod_vpe_mfp_aligner_t);
	if(!cur_pkt){
#if !defined(CHECK)
	   DBG_MFPLOG("Aligner", ERROR, MOD_MFPLIVE,\
	   	"Aligner failed to allocate memory for first pkt");
#endif
	   return aligner_pkt_memalloc_fail;	   
	}

	cur_pkt->prev = cur_pkt;
	cur_pkt->next = NULL;
	*cur_pkt_ptr = cur_pkt;
	return aligner_success;
}

static aligner_retcode_e create_pkt_mem(
	ts_pkt_info_t *cur_pkt){

	ts_pkt_info_t *next_pkt;
	next_pkt = nkn_calloc_type(sizeof(ts_pkt_info_t), 1, \
		mod_vpe_mfp_aligner_t);
	if(!next_pkt){
#if !defined(CHECK)
	   DBG_MFPLOG("Aligner", ERROR, MOD_MFPLIVE,\
	   	"Aligner failed to allocate memory for next pkt\n");
#endif
	   return aligner_pkt_memalloc_fail;	   
	}

	next_pkt->prev = cur_pkt;
	next_pkt->next = NULL;
	cur_pkt->next = next_pkt;
	
   	return aligner_success;
}

//1 delete memory for ts packet info table
//1 ts packet is doubly linked list
static aligner_retcode_e delete_pkt_mem(
	ts_pkt_info_t *cur_pkt){

delete_pkt_mem_start:
	if(!cur_pkt->next){
            //free the pkt mem that has no followup mem
            ts_pkt_info_t *prev_pkt;
	    prev_pkt = cur_pkt->prev;
	    if(!prev_pkt){
	    	printf("break in linked list of pkt mem\n");
		return aligner_pktmem_free_fail;
	    }	    
	    prev_pkt->next = NULL;
            free(cur_pkt);
	    return aligner_success;
	} else{
	   //recursively call the delete pkt mem
	   if(delete_pkt_mem(cur_pkt->next) == aligner_success){
	   	goto delete_pkt_mem_start;
	   } else{
		return aligner_pktmem_free_fail;
	   }
	}   		
}

//1 delete memory for video frame info table
//1 video frame is double linked list
static aligner_retcode_e delete_frame_mem(
	aligner_ctxt_t *aligner_ctxt,
	video_frame_info_t *cur_frame){

delete_frame_mem_start:
	if(!cur_frame->next){
            //free the pkt mem that has no followup mem
            video_frame_info_t *prev_frame;
	    prev_frame = cur_frame->prev;
	    if(!prev_frame){
	    	printf("break in linked list of frame mem\n");
		return aligner_framemem_free_fail;
	    }
	    //free the packets inside the frame
	    if(aligner_ctxt->delete_pkt(cur_frame->first_pkt) \
	    	== aligner_pktmem_free_fail){
	    	return aligner_pktmem_free_fail;
	    }
	    prev_frame->next = NULL;
            free(cur_frame);
	    return aligner_success;
	} else{
	   //recursively call the delete pkt mem
	   if(delete_frame_mem(aligner_ctxt, cur_frame->next) == aligner_success){
	   	goto delete_frame_mem_start;
	   } else{
		return aligner_framemem_free_fail;
	   }
	}   		
}

//1 aligner gets invoked here
aligner_retcode_e create_aligner_ctxt(
	aligner_ctxt_t **aligner_ctxt){

	aligner_retcode_e ret;
	aligner_ctxt_t *cur_alginer;

	ret = aligner_success;

	//alloc aligner context memory
	cur_alginer = (aligner_ctxt_t *)nkn_calloc_type(
		sizeof(aligner_ctxt_t), 1, mod_vpe_mfp_aligner_t);

	if(cur_alginer == NULL){
		ret = aligner_ctxt_memalloc_fail;
		goto create_aligner_ctxt_ret;
	}

	//assign function pointers
	cur_alginer->create_first_frame = &create_init_frame_mem;
	cur_alginer->create_frame = &create_frame_mem;
	cur_alginer->create_first_pkt = &create_init_pkt_mem;
	cur_alginer->create_pkt = &create_pkt_mem;

	cur_alginer->process_video_pkts = &process_video_pkts;
	cur_alginer->print_frameinfo = &print_frame_info;
	
	cur_alginer->delete_pkt = &delete_pkt_mem;
	cur_alginer->delete_frame = &delete_frame_mem;
	cur_alginer->del = &delete_aligner_ctxt;
	
	*aligner_ctxt = cur_alginer;
	
create_aligner_ctxt_ret:
	if(*aligner_ctxt && ret < aligner_success){
		free(*aligner_ctxt);
		*aligner_ctxt = NULL;
	}
	return(ret);
}

//1 delete aligner context data structure
static aligner_retcode_e delete_aligner_ctxt(
	aligner_ctxt_t *aligner_ctxt){

	//video_frame_info_t *cur_frame;
	aligner_retcode_e ret = aligner_success;

	//free if frame mem exists
	if(aligner_ctxt->video_frame){
		//release memory in frame info structure
		ret = aligner_ctxt->delete_frame(
			aligner_ctxt, aligner_ctxt->video_frame);
		if(ret == aligner_framemem_free_fail){
#if !defined(CHECK)
			DBG_MFPLOG("Aligner", ERROR, MOD_MFPLIVE,\
				"Fail to free Aligner Frame Mem");
#endif
			return(ret);
		}else if(ret == aligner_pktmem_free_fail){
#if !defined(CHECK)
			DBG_MFPLOG("Aligner", ERROR, MOD_MFPLIVE,\
				"Fail to free Aligner Packet Mem");
#endif
			return(ret);
		}		
		aligner_ctxt->video_frame = NULL;
	}

	//free memory in backup ts packet store
	if(aligner_ctxt->pkt_store){
		free(aligner_ctxt->pkt_store);
		aligner_ctxt->pkt_store = NULL;
	}
	aligner_ctxt->tot_pkt_size = 0;
	aligner_ctxt->num_pkts_stored = 0;	

	free(aligner_ctxt);
	
	return (ret);
}

//1 create mem for first frame and first pkt
static aligner_retcode_e create_mem(
	aligner_ctxt_t *aligner_ctxt,
	video_frame_info_t **video_frame_mem){

	aligner_retcode_e ret = aligner_success;
	
	//2 allocate memory for first frame
	video_frame_info_t *video_frame;
	ret = aligner_ctxt->create_first_frame(&video_frame);
	if(ret != aligner_success){
#if !defined(CHECK)
		DBG_MFPLOG("Aligner", ERROR, MOD_MFPLIVE,\
			"Unable to alloc mem");
#endif
		return(ret);
	}
	//2 allocate memory for firs ts pkt in the first frame
	ret = aligner_ctxt->create_first_pkt(&video_frame->first_pkt);
	if(ret != aligner_success){
#if !defined(CHECK)
		DBG_MFPLOG("Aligner", ERROR, MOD_MFPLIVE,\
			"Unable to alloc mem");
#endif
		return(ret);
	}
	*video_frame_mem = video_frame;
	
	return (ret);
}

//1 main processing module that create video packet info table

static aligner_retcode_e process_video_pkts(
	aligner_ctxt_t *aligner_ctxt,
	uint8_t *data,
	uint32_t *mstr_pkts,
	uint32_t n_mstr_pkts,
	spts_st_ctx_t *spts,
	video_frame_info_t **video_frame_tbl, /*output aligner table*/
	uint32_t *num_frames
	){

	int32_t ret1;
	ts_pkt_infotable_t *pkt_tbl;
	aligner_retcode_e ret = aligner_success;
	//uint32_t num_codec_info_pkt = 0;
	uint32_t k = 0;
	uint32_t temp_SEI_len = 0, temp_codec_len = 0;
	uint32_t temp_num = 0;
	//2 step 1: create ts packet info index table
	ret1 = create_index_table(data, mstr_pkts, n_mstr_pkts, 
				  spts->mux.audio_pid1, spts->mux.video_pid,  
				  &pkt_tbl);
	if(ret1 < 0){
#if !defined(CHECK)
		DBG_MFPLOG("Aligner", ERROR, MOD_MFPLIVE,\
			"failed to create index table for given ts pkts");
#endif
		ret = aligner_parsepkts_fail;
		return(ret);
	}


	//2 search for first frame start in the given chunk
	uint32_t first_pkt_cnt = 0;
	uint32_t pkt_cnt = 0;
	while(pkt_cnt < n_mstr_pkts){
		if(pkt_tbl[pkt_cnt].frame_start == 1){
			first_pkt_cnt = pkt_cnt;
			break;
		}
		pkt_cnt++;
	}

	//2 create mem for first video frame
	//2 video frame 1 is allocated outside the for loop
	video_frame_info_t *cur_frame;
	ts_pkt_info_t *cur_pkt;
	*num_frames = 0;
	
	ret = create_mem(aligner_ctxt, &cur_frame);
	if(ret != aligner_success){
#if !defined(CHECK)
		DBG_MFPLOG("Aligner", ERROR, MOD_MFPLIVE,\
			"unable to alloc mem");
#endif
		return (ret);
	}

	*video_frame_tbl = cur_frame;
	cur_pkt = cur_frame->first_pkt;		
	cur_frame->last_pkt = cur_pkt;
	cur_frame->dts = 0;
	cur_frame->pts = 0;
	cur_frame->frame_num = *num_frames;
	*num_frames= *num_frames + 1;
	
	//2 step 2: video frame table create
	uint32_t pkt;
	for(pkt = first_pkt_cnt; pkt < n_mstr_pkts; pkt++){
		
		if((pkt_tbl[pkt].frame_start == 1) && (pkt > first_pkt_cnt)){
			//2 allocate mem for next frame when frame_start is 1
			ret = aligner_ctxt->create_frame(cur_frame);
			if(ret != aligner_success){
#if !defined(CHECK)
				DBG_MFPLOG("Aligner", ERROR, MOD_MFPLIVE,\
					"unable to alloc mem");
#endif
				return(ret);
			}
			cur_frame = cur_frame->next;
			cur_frame->dts = 0;
			cur_frame->pts = 0;			
			cur_frame->frame_num = *num_frames;
			*num_frames= *num_frames + 1;
			//2 allocate memory for firs ts pkt in the given frame
			ret = aligner_ctxt->create_first_pkt(&cur_frame->first_pkt);
			if(ret != aligner_success){
#if !defined(CHECK)
				DBG_MFPLOG("Aligner", ERROR, MOD_MFPLIVE,\
					"Unable to alloc mem");
#endif
				return(ret);
			}	
			cur_pkt = cur_frame->first_pkt;	
			cur_frame->last_pkt = cur_pkt;
		} else if(pkt > first_pkt_cnt) {
			//2 at end of this pkt processing, assign mem for next pkt
			ret = aligner_ctxt->create_pkt(cur_pkt);
			if(ret != aligner_success){
#if !defined(CHECK)
				DBG_MFPLOG("Aligner", ERROR, MOD_MFPLIVE,\
					"unable to alloc mem");
#endif
				return(ret);
			}
			cur_pkt = cur_pkt->next;
			cur_frame->last_pkt = cur_pkt;
		}
		//2 Assign the cur packet residue to last frame
		// calculate the SEI length occuring before the SPS/PPS
		if(pkt_tbl[pkt].num_SEI != 0) {
                  for(k=0; k<pkt_tbl[pkt].num_SEI; k++) {
                    temp_SEI_len += pkt_tbl[pkt].SEI_length[k];
                  }
		}
		//calculate the codecinfo length
		temp_codec_len = pkt_tbl[pkt].codec_info_len;

		if(pkt_tbl[pkt].slice_start  
			> (pkt_tbl[pkt].header_len + DEF_ACCESS_UNIT_DELIMITER_SIZE 
			   + temp_SEI_len + temp_codec_len + pkt_tbl[pkt].dump_size)){
		    video_frame_info_t *prev_frame = cur_frame->prev;
		    
		    if(prev_frame &&  prev_frame != cur_frame){
			    ts_pkt_info_t *last_pkt = prev_frame->last_pkt;
			    ret = aligner_ctxt->create_pkt(last_pkt);
			    if(ret != aligner_success){
#if !defined(CHECK)
			    	DBG_MFPLOG("Aligner", ERROR, MOD_MFPLIVE,\
					"unable to alloc mem");
#endif
				return(ret);
			    }
			    last_pkt = last_pkt->next;
			    prev_frame->last_pkt = last_pkt;			
			    last_pkt->pkt_num = pkt;
			    last_pkt->data = data;
			    last_pkt->offset = mstr_pkts[pkt];
			    last_pkt->psi_flag = \
			    	pkt_tbl[pkt].payload_start_indicator_flag;
			    last_pkt->dts = pkt_tbl[pkt].dts;
			    last_pkt->pts = pkt_tbl[pkt].pts;
			    last_pkt->header_len = pkt_tbl[pkt].header_len;
			    last_pkt->slice_start = pkt_tbl[pkt].header_len;
			    last_pkt->slice_len = pkt_tbl[pkt].slice_start\
			    	- pkt_tbl[pkt].header_len\
			    	- DEF_ACCESS_UNIT_DELIMITER_SIZE;
			    last_pkt->continuity_counter = \
			    	pkt_tbl[pkt].continuity_counter;
			    last_pkt->vector.data = last_pkt->data;
			    last_pkt->vector.offset = last_pkt->offset \
			    	+ last_pkt->slice_start;
			    last_pkt->vector.length = last_pkt->slice_len;

			    prev_frame->frame_len += last_pkt->vector.length;
		    }
		}

		
		//2 assign pkt infotable to frame info

		if(pkt >= 1){
		    if(pkt_tbl[pkt].adaptation_field_control == 0x1 ||
				pkt_tbl[pkt].adaptation_field_control == 0x3){
			if (pkt_tbl[pkt].continuity_counter != \
				((pkt_tbl[pkt - 1].continuity_counter+ 1)& 0xf)){
				cur_frame->discontinuity_indicator = 1;
			}
		    }
		}
	
		
		if(((pkt_tbl[pkt].dts < cur_frame->dts) && pkt_tbl[pkt].dts)  \
			|| (!cur_frame->dts)){
			cur_frame->dts = pkt_tbl[pkt].dts;
		}
		if(((pkt_tbl[pkt].pts < cur_frame->pts) && pkt_tbl[pkt].pts) \
			|| (!cur_frame->pts)){
			cur_frame->pts = pkt_tbl[pkt].pts;
		}
		if(pkt_tbl[pkt].frame_type > slice_default_value){
			cur_frame->slice_type = pkt_tbl[pkt].frame_type;
		}
		if((int32_t)pkt_tbl[pkt].codec_info_len > 0){
		  cur_frame->codec_info_start_offset[cur_frame->num_codec_info_pkt] =			\
		    data + mstr_pkts[pkt] + pkt_tbl[pkt].codec_info_start_offset;
		  cur_frame->codec_info_len[cur_frame->num_codec_info_pkt] = pkt_tbl[pkt].codec_info_len;
		  cur_frame->num_codec_info_pkt++;
		}else{
		  if(cur_frame->num_codec_info_pkt > 0 && pkt_tbl[pkt].codec_info_start_offset){
		  cur_frame->codec_info_len[cur_frame->num_codec_info_pkt -1] += ((int32_t)pkt_tbl[pkt].codec_info_len - 1);
		  }
		}
		//assign SEI appearing before sps/pps as separate vector
		if(pkt_tbl[pkt].num_SEI != 0) {
		  for(k=0; k<pkt_tbl[pkt].num_SEI; k++) {
		    cur_pkt->SEI_offset[cur_pkt->num_SEI] = data + 
		      mstr_pkts[pkt]+ pkt_tbl[pkt].SEI_offset[k];
		    cur_pkt->SEI_length[cur_pkt->num_SEI] = pkt_tbl[pkt].SEI_length[k];
		    cur_pkt->num_SEI++;
		    cur_frame->frame_len += pkt_tbl[pkt].SEI_length[k];
		  }
		  temp_num++;
		}

		//2 assign pkt infotable data to pkt info struct
		cur_pkt->pkt_num = pkt;
		cur_pkt->data = data;
		cur_pkt->offset = mstr_pkts[pkt];
		cur_pkt->psi_flag = pkt_tbl[pkt].payload_start_indicator_flag;
		cur_pkt->dts = pkt_tbl[pkt].dts;
		cur_pkt->pts = pkt_tbl[pkt].pts;
		cur_pkt->header_len = pkt_tbl[pkt].header_len;
		cur_pkt->slice_start = pkt_tbl[pkt].slice_start;
		cur_pkt->is_slice_start = pkt_tbl[pkt].is_slice_start;
		cur_pkt->slice_len = pkt_tbl[pkt].slice_len;
		cur_pkt->continuity_counter = pkt_tbl[pkt].continuity_counter;

		cur_pkt->vector.data = cur_pkt->data;
		cur_pkt->vector.offset = cur_pkt->offset + cur_pkt->slice_start;
		cur_pkt->vector.length = cur_pkt->slice_len;
		if(cur_pkt->is_slice_start)
		  cur_frame->frame_len += cur_pkt->vector.length;
		
	}

	if(pkt_tbl){
		free(pkt_tbl);
		pkt_tbl = NULL;
	}
	
	return aligner_success;
}

static aligner_retcode_e print_frame_info(
	aligner_ctxt_t *aligner_ctxt,
	video_frame_info_t *video_frame_tbl,
	FILE *fp,
	accum_ts_t *accum){

	video_frame_info_t *cur_frame;
	ts_pkt_info_t *cur_pkt;
	uint32_t i = 0;
	cur_frame = video_frame_tbl;
	uint32_t acc_slice_len = 0;
	uint32_t acc_vector_len = 0;
	uint32_t acc_sei_len = 0;
	uint32_t acc_spspps_len = 0;
	
	while(cur_frame != NULL){
		fprintf(fp, "Frame %u\n", cur_frame->frame_num);
		fprintf(fp, "frame length %u\n", cur_frame->frame_len);
		fprintf(fp, "slice type ");
		switch(cur_frame->slice_type){
			case slice_type_i:
				fprintf(fp, "I Frame\n");
				break;
			case slice_type_p:
				fprintf(fp, "P Frame\n");
				break;
			case slice_type_b:
				fprintf(fp, "B Frame\n");
				break;
			case slice_default_value:
				fprintf(fp, " Default Value -Err\n");
				break;				
			case slice_need_more_data:
				fprintf(fp, " Incomplete Frame -Err\n");
				break;				
		}
		fprintf(fp, "DTS %u\n", cur_frame->dts);
		fprintf(fp, "PTS %u\n", cur_frame->pts);
		fprintf(fp, "Discontinuity Indicator %u\n", cur_frame->discontinuity_indicator);
		acc_spspps_len = 0;
		for(i=0; i<cur_frame->num_codec_info_pkt; i++) {
		  fprintf(fp, "codec info len %u\n", cur_frame->codec_info_len[i]);
		  fprintf(fp, "sps pps start %lx\n", (uint64_t)cur_frame->codec_info_start_offset[i]);
		  //acc_spspps_len += cur_frame->codec_info_len[i];
		}
		cur_pkt = cur_frame->first_pkt;
		acc_slice_len = 0;
		acc_vector_len = 0;
		acc_sei_len = 0;
		
		while(cur_pkt != NULL){
			fprintf(fp, "Packet %u", cur_pkt->pkt_num);
			//fprintf(fp, " Addr   %lx", (uint64_t)(cur_pkt->vector.data));
			fprintf(fp, " Offset %u", cur_pkt->vector.offset);	
			fprintf(fp, " Length %u", cur_pkt->vector.length);				
			fprintf(fp, " DTS    %u", cur_pkt->dts);	
			fprintf(fp, " PTS    %u", cur_pkt->pts);	
			fprintf(fp, " PSFlag %u", cur_pkt->psi_flag);	
			fprintf(fp, " hdrlen %u", cur_pkt->header_len);	
			fprintf(fp, " SlcSrt %u", cur_pkt->slice_start);	
			fprintf(fp, " SlcLen %u", cur_pkt->slice_len);
			fprintf(fp, " continuityCnt %u num_SEI %u\n", cur_pkt->continuity_counter, cur_pkt->num_SEI);
			if(1){//cur_pkt->is_slice_start){
				acc_slice_len += cur_pkt->slice_len;
				acc_vector_len += cur_pkt->vector.length;
			}
			for(i = 0; i < cur_pkt->num_SEI; i++)
				acc_sei_len += cur_pkt->SEI_length[i];
			
			cur_pkt = cur_pkt->next;
		}		
		fprintf(fp, "----------------------------------\n");
		//assert((acc_slice_len + acc_sei_len + acc_spspps_len) == cur_frame->frame_len);
		//assert((acc_vector_len + acc_sei_len + acc_spspps_len) == cur_frame->frame_len);
		cur_frame = cur_frame->next;		
	}

	return aligner_success;
}
	

