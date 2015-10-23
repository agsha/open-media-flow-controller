
#include "mfp_video_aligner.h"
#include "nkn_vpe_pattern_search.h"


static uint32_t
uint32_byte_swap(uint32_t input){
    uint32_t ret=0;
    ret = (((input&0x000000FF)<<24) + ((input&0x0000FF00)<<8) +
           ((input&0x00FF0000)>>8) + ((input&0xFF000000)>>24));
    return ret;
}


int32_t anomaly_correction_with_average_ts(
					   uint32_t *sample_duration,
					   uint32_t num_frames,
					   uint32_t *tot_t){

    uint32_t i, index1;
    int32_t ret = 0;
    uint32_t *anomaly_sample;
    uint32_t anomaly_cnt = 0;
    uint32_t tot_samp_duration = 0;
    uint32_t num_samples = 0;
    uint32_t avg_samp_duration = 0;
    uint32_t avg_duration_decision_flag = 0;
    
	//2 constructors
	anomaly_sample	= (uint32_t *)nkn_calloc_type(num_frames, sizeof(uint32_t),
			    mod_vpe_mfp_ts2mfu_t);
	if(!anomaly_sample){
	    printf("failure to create mem\n");
	    ret = -1 ;
	    return (ret);	    
	}   
	uint32_t sample_threshold = (num_frames*3)/10; // thirty percent 
	if(!sample_threshold)
	    sample_threshold = 1;

	//2 find anomaly sample and calc total ts
	for(i = 0; i < num_frames - 1; i++){
	    if(sample_duration[i] == sample_duration[i+1]){
		    tot_samp_duration += uint32_byte_swap(sample_duration[i]);
		    num_samples++;
	    } else{
		    anomaly_sample[anomaly_cnt++] = i;
	    }
	}

	if(num_samples){
	     //2 calc average
	     avg_samp_duration = tot_samp_duration/num_samples;
	     if(num_samples > (num_frames - sample_threshold)){
		     avg_duration_decision_flag = 1;
	     }
	     //2 adjust the anomaly sample
	     if(avg_duration_decision_flag){
	     	for(i = 0; i < anomaly_cnt; i++){
		    index1 = anomaly_sample[i];
		    sample_duration[index1] = uint32_byte_swap(avg_samp_duration);
	     	}
		if(tot_t){
		    *tot_t = 0;
		    for(i = 0; i < num_frames; i++)
			*tot_t += uint32_byte_swap(sample_duration[i]);
		    //		    assert(0);
		}
	     }
	}

	//2 destructors
	if(anomaly_sample){
		free(anomaly_sample);
		anomaly_sample = NULL;
	}
	return (ret);

}



pattern_ctxt_t * create_pattern_ctxt(uint32_t num_AU){
	
	pattern_ctxt_t *pat_ctxt = (pattern_ctxt_t *)nkn_calloc_type(1,
		sizeof(pattern_ctxt_t), mod_vpe_mfp_ts2mfu_t);

	if(!pat_ctxt){
		printf("failure to create mem\n");
		return NULL;
	}

	pat_ctxt->ts_pattern = nkn_calloc_type(num_AU, 
	    	sizeof(uint32_t), mod_vpe_mfp_ts2mfu_t); 

	if(!(pat_ctxt->ts_pattern)){
		printf("failure to create mem\n");
		return NULL;
	}
	pat_ctxt->array_depth = 0;
	pat_ctxt->pattern_start = 0;
	pat_ctxt->pattern_end = 0;
	
	return (pat_ctxt);
}


int32_t delete_pattern_ctxt(pattern_ctxt_t *pat_ctxt){
	
	if(pat_ctxt->ts_pattern){
		free(pat_ctxt->ts_pattern);
		pat_ctxt->ts_pattern = NULL;
	}
	if(pat_ctxt){
		free(pat_ctxt);
		pat_ctxt = NULL;
	}
	return 0;
}

int32_t search_pattern_ctxt(pattern_ctxt_t *pat_ctxt,
	uint32_t sample_duration){

	uint32_t i;
	for(i = pat_ctxt->pattern_start; i < pat_ctxt->pattern_end; i++){
		if(pat_ctxt->ts_pattern[i] == sample_duration)
			return (int32_t)i;
	}
	return -1;
}

int32_t insert_pattern(pattern_ctxt_t *pat_ctxt,
	uint32_t sample_duration){
	
	pat_ctxt->ts_pattern[pat_ctxt->pattern_end] = sample_duration;
	pat_ctxt->pattern_end++;
	return(pat_ctxt->pattern_end - 1);
	
}


int32_t comp_duration_validator(
	uint32_t *sample_duration,
	uint32_t *PTS,
	uint32_t *DTS,
	uint32_t num_AU){

    uint32_t i;
    //int32_t ret = 0;
    

    //ret = avg_ts_anomaly_correction(sample_duration, num_AU);
    //if(ret != 0){
    	//return (ret);
    //}
    

    //find pattern
    //uint32_t ts_pattern_decision_flag = 0;
    pattern_ctxt_t *pat_ctxt = create_pattern_ctxt(num_AU);
    if(!pat_ctxt){
	printf("failure to create mem\n");
	return -1 ; 	
    }
    int32_t pat_cnt = 0;
    
    for(i = 0; i < num_AU; i++){
    	pat_cnt = search_pattern_ctxt(pat_ctxt, sample_duration[i]);
	if(pat_cnt < 0)
		pat_cnt = insert_pattern(pat_ctxt, sample_duration[i]);   	
	
    }


    delete_pattern_ctxt(pat_ctxt);

    return 1;
}


#if 0
uint32_t validate_ptsentry_fw(
	video_frame_info_t *cur_frame,
	uint32_t avg_samp_duration){
	
	video_frame_info_t *next_frame = cur_frame->next;
	if(!next_frame)
		return 0;
	if(cur_frame->pts + avg_samp_duration ==
		next_frame->pts){
		return 1;
	}
	return 0;
}

uint32_t validate_ptsentry_bw(
	video_frame_info_t *cur_frame,
	uint32_t avg_samp_duration){
	
	video_frame_info_t *prev_frame = cur_frame->prev;
	if(!prev_frame)
		return 0;
	if(cur_frame->pts - avg_samp_duration ==
		prev_frame->pts){
		return 1;
	}
	return 0;
}

uint32_t validate_dtsentry_fw(
	video_frame_info_t *cur_frame,
	uint32_t avg_samp_duration){
	
	video_frame_info_t *next_frame = cur_frame->next;
	if(!next_frame)
		return 0;
	if(cur_frame->dts + avg_samp_duration ==
		next_frame->dts){
		return 1;
	}
	return 0;
}

uint32_t validate_dtsentry_bw(
	video_frame_info_t *cur_frame,
	uint32_t avg_samp_duration){
	
	video_frame_info_t *prev_frame = cur_frame->prev;
	if(!prev_frame)
		return 0;
	if(cur_frame->dts - avg_samp_duration ==
		prev_frame->dts){
		return 1;
	}
	return 0;
}



/*
* Important Note:
* pts_anomaly_correction and dts_anomaly_correction are same functions
* operating on different data sets. Any fix in one function should be ported to
* other function
*/


int32_t pts_anomaly_correction(
	video_frame_info_t *first_frame,
	uint32_t num_frames){

    uint32_t i;
    int32_t ret = 0;

    uint32_t num_samples = 0;
    uint32_t tot_samp_duration = 0;
    uint32_t avg_samp_duration = 0;
    uint32_t avg_duration_decision_flag = 0;

    anomaly_sample_t *anomaly_sample, *cur_anomaly_sample;
    uint32_t anomaly_cnt = 0;
    uint32_t frame_num = 0;
    video_frame_info_t *cur_frame, *prev_frame;
    uint32_t candidate1, candidate2;

    //2 constructors
    anomaly_sample  = (uint32_t *)nkn_calloc_type(num_frames, 
    			sizeof(anomaly_sample_t), mod_vpe_mfp_ts2mfu_t);
    if(!anomaly_sample){
		printf("failure to create mem\n");
		ret = -1 ;
		return (ret);    	
    }	
    uint32_t sample_threshold = (num_frames*3)/10; // thirty percent 
    if(!sample_threshold)
    	sample_threshold = 1;

    cur_anomaly_sample = anomaly_sample;

    cur_frame = first_frame->next;
    prev_frame = cur_frame->prev;
    if(prev_frame->pts == 0){
    	cur_anomaly_sample->frame_num = 0;
	cur_anomaly_sample->prediction_direction = backward;
	cur_anomaly_sample->valid_entry = 1;
	cur_anomaly_sample->try_again = 1;
	cur_anomaly_sample++;
	anomaly_cnt++;
    }
    
    //2 find anomaly sample and calc total ts
    for(i = 1; i < num_frames; i++){
    	if(cur_frame->pts != 0 && prev_frame->pts != 0){
		tot_samp_duration += (cur_frame->pts -prev_frame->pts);
		num_samples++;
    	} else if(cur_frame->pts == 0){
		cur_anomaly_sample->frame_num = 0;
		cur_anomaly_sample->prediction_direction = forward;
		cur_anomaly_sample->valid_entry = 1;
		cur_anomaly_sample->try_again = 1;
		cur_anomaly_sample++;
		anomaly_cnt++;
    	}
	cur_frame = cur_frame->next;
	prev_frame = cur_frame->prev;
    }
    
    if(num_samples){
	//2 calc average
    	avg_samp_duration = tot_samp_duration/num_samples;
	if(num_samples > (num_frames - sample_threshold)){
		avg_duration_decision_flag = 1;
	}
	//2 adjust the anomaly sample
	if(avg_duration_decision_flag){
		cur_anomaly_sample = anomaly_sample;
		while(anomaly_cnt){
			//for every count, start search from begining
			cur_frame = first_frame->next;
			prev_frame = cur_frame->prev;
			while(cur_frame){
				if(cur_frame->frame_num == cur_anomaly_sample->frame_num){
					if(cur_anomaly_sample->prediction_direction == backward){
						//check whether  base entry is valid
						if(validate_ptsentry_fw(cur_frame->next)){
							cur_frame->pts = cur_frame->next->pts -
								avg_samp_duration;
							cur_anomaly_count
	  						anomaly_cnt--;
						}						
					}
					if(cur_anomaly_sample->prediction_direction == forward){
						//check whether  base entry is valid
						if(validate_ptsentry_bw(cur_frame->prev)){
							cur_frame->pts = prev_frame->pts +
								avg_samp_duration;
	  						anomaly_cnt--;
						}						
					}
					
				}
				cur_frame = cur_frame->next;
				prev_frame = cur_frame->prev;
				
			}			
		}		
		ret = 1;		
	}
    }
    //2 destructors
    if(anomaly_sample){
	    free(anomaly_sample);
	    anomaly_sample = NULL;
    }
    return (ret);
    
}


int32_t dts_anomaly_correction(
	video_frame_info_t *first_frame,
	uint32_t num_frames){

    uint32_t i;
    int32_t ret = 0;

    uint32_t num_samples = 0;
    uint32_t tot_samp_duration = 0;
    uint32_t avg_samp_duration = 0;
    uint32_t avg_duration_decision_flag = 0;

    uint32_t *anomaly_sample;
    uint32_t anomaly_cnt = 0;
    uint32_t frame_num = 0;
    video_frame_info_t *cur_frame, *prev_frame;

    //2 constructors
    anomaly_sample  = (uint32_t *)nkn_calloc_type(num_frames, sizeof(uint32_t),
    			mod_vpe_mfp_ts2mfu_t);
    if(!anomaly_sample){
		printf("failure to create mem\n");
		ret = -1 ;
		return (ret);    	
    }	
    uint32_t sample_threshold = (num_frames*3)/10; // thirty percent 
    if(!sample_threshold)
    	sample_threshold = 1;



    cur_frame = first_frame->next;
    prev_frame = cur_frame->prev;
    
    //2 find anomaly sample and calc total ts
    for(i = 1; i < num_frames; i++){
    	if(cur_frame->dts != 0){
		tot_samp_duration += (cur_frame->dts -prev_frame->dts);
		num_samples++;
    	} else{
		anomaly_sample[anomaly_cnt++] = i;
    	}
	cur_frame = cur_frame->next;
	prev_frame = cur_frame->prev;
    }
    
    if(num_samples){
	//2 calc average
    	avg_samp_duration = tot_samp_duration/num_samples;
	if(num_samples > (num_frames - sample_threshold)){
		avg_duration_decision_flag = 1;
	}
	//2 adjust the anomaly sample
	if(avg_duration_decision_flag){
		cur_frame = first_frame->next;
		prev_frame = cur_frame->prev;
		for(i = 0; i < anomaly_cnt; i++){
			frame_num = anomaly_sample[i];
			if(frame_num == 0) 
				continue;
			while(cur_frame){
				if(cur_frame->frame_num == frame_num){
					cur_frame->dts = prev_frame->dts + \
						avg_samp_duration;
					break;
				}
				cur_frame = cur_frame->next;
				prev_frame = cur_frame->prev;
			}
		}

		//2 if first frame has an anomaly, correct it based
		//2 on second sample
		if(anomaly_sample[i] == 0){
			cur_frame = first_frame->next;
			prev_frame = cur_frame->prev;
			prev_frame->dts = cur_frame->dts - avg_samp_duration;
		}
		
		ret = 1;		
	}
    }
    //2 destructors
    if(anomaly_sample){
	    free(anomaly_sample);
	    anomaly_sample = NULL;
    }
    return (ret);
    
}
pts_anomaly_correction(accum->video_frame_tbl, num_frames);
dts_anomaly_correction(accum->video_frame_tbl, num_frames);

#endif








