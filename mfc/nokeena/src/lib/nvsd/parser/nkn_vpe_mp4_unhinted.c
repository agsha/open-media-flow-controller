/*
 *
 * Filename:  mp4_unhinted.c
 * Date:      2009/03/23
 * Module:    Parser for hinted MP4 files
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

#include <string.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <glib.h>

#include "nkn_vpe_types.h"
#include "nkn_vpe_mp4_unhinted.h"
#include "nkn_vpe_mp4_feature.h"
#include "nkn_vpe_bufio.h"
#include "nkn_vpe_rtp_packetizer.h"
#include "rtp_packetizer_common.h"
#include "rtp_mpeg4_packetizer.h"
#include "rtp_avc_packetizer.h"
#include "rtp_avc_read.h"

/*
Inputs: 2 elements: 1 to feed input to the optimized mp4 file parser. Another to feed inputs to the rtp packetizer.
*/
int32_t 
send_next_pkt_unhinted(parser_t* parser, Sample_rtp_info_t *rtp_sample)
{
    int32_t ret;

    ret = -1;
    switch(parser->trak_type){
	case VIDEO_TRAK:
	    {
		
		if(parser->st_all_info->stbl_info->VideoStbl.codec_type == H264_VIDEO){
		//if(1){
		    uint8_t *sample,*avcc_data;
		    int32_t avc1_size,*avc1_offset;
		    float time_scale;


		    ret = get_next_sample(parser->st_all_info, parser->trak_type,nkn_vpe_buffered_read, parser->fid, parser->look_ahead_time, parser->output);
		    if(parser->output->num_samples_sent == 13753){
                        int uijk = 1;
			uijk+=1;
                    }

		    if(ret==NOMORE_SAMPLES)
			return NOMORE_SAMPLES;
		    sample = (uint8_t*)malloc((parser->output->num_bytes));
		    nkn_vfs_fseek(parser->fid,parser->output->offset,SEEK_SET);
#ifdef VPE_VFS_SECURE		    
		    if((int)nkn_vpe_buffered_read(sample,1,parser->output->num_bytes,parser->fid) != (int)parser->output->num_bytes){
			if(sample!=NULL){
			    free(sample);
			}
			return  NOMORE_SAMPLES;
		    }
#else
		    nkn_vpe_buffered_read(sample,1,parser->output->num_bytes,parser->fid);
#endif
		    rtp_sample->sample_data = sample;
		    rtp_sample->sample_size = parser->output->num_bytes;
		    //		    printf("Sample Number = %d\n",parser->output->num_samples_sent);		    
		    if(parser->output->num_samples_sent == 1){
			rtp_sample->sample_time = 0;
			parser->prev_fid = parser->fid;
			//			rtp_sample->sample_time+=parser->output->ctts_offset;
		    }
			rtp_sample->ft= 90000.0/(float)(parser->st_all_info->stbl_info->VideoStbl.timescale);
		    rtp_sample->sample_time+=parser->output->ctts_offset;
		    if(rtp_sample->pkt_builder == NULL){
			rtp_sample->pkt_builder = (rtp_packet_builder_t*)calloc(1,sizeof(rtp_packet_builder_t));
		    }
		    if(rtp_sample->pkt_builder->avc1 == NULL){
			char* sdp;
			int32_t *sdp_size;
			avc1_offset = (int32_t*)malloc(sizeof(int32_t));
			avc1_size =  getavcc(parser->st_all_info,VIDEO_TRAK,nkn_vpe_buffered_read,parser->fid,avc1_offset);
			avcc_data = (uint8_t*)malloc(sizeof(uint8_t)*avc1_size);
			nkn_vfs_fseek(parser->fid,*avc1_offset,SEEK_SET);
			nkn_vpe_buffered_read(avcc_data,avc1_size,sizeof(uint8_t),parser->fid);
			rtp_sample->pkt_builder->avc1 = (AVC_conf_Record_t*)malloc(sizeof(AVC_conf_Record_t));
			read_avc1_box(rtp_sample->pkt_builder->avc1,avcc_data, 100);
			sdp_size = (int32_t*)malloc(sizeof(int32_t));
			//build_sdp_avc_info(rtp_sample->pkt_builder, &sdp, sdp_size);
			build_sdp_avc_info(rtp_sample->pkt_builder, &sdp, sdp_size,parser->st_all_info->stbl_info->VideoStbl.height,parser->st_all_info->stbl_info->VideoStbl.width);
		    }
		    rtp_packetize_avc(rtp_sample);
		    rtp_sample->sample_time = *parser->look_ahead_time;
		    ret = RTP_PKT_READY;
		    append_list_deterministic(rtp_sample);
		    if(rtp_sample->pkt_builder->rtp_pkt_size != NULL)
			free(rtp_sample->pkt_builder->rtp_pkt_size);
		    if(sample!=NULL)
			free(sample);
		}
	    }
	    break;
	case AUDIO_TRAK:
	    {
		rtp_packet_builder_t *bldr;
		FILE *fid;
		uint8_t *nalu;
		int32_t nal_size, sample_available;
		GList *list;
		
		//initialization			
		fid = parser->fid;
		nalu = NULL;
		nal_size = 0;
		sample_available = 0;
		bldr = rtp_sample->pkt_builder;
		list = NULL;

		if(!bldr->es_desc) { //initialize the packet builder
		    init_mpeg4_packet_builder(parser, bldr, 96, (fnp_packet_ready_handler)packet_ready_handler, rtp_sample);
		}
		
		do {
		    sample_available = get_next_sample(parser->st_all_info, parser->trak_type, nkn_vpe_buffered_read, parser->fid, parser->look_ahead_time, parser->output);
		    if(sample_available == NOMORE_SAMPLES) {
			//force flush
			if(!bldr->new_pkt) {
			    build_mpeg4_rtp_packet(bldr, 0, 0, 0, 1, 1);
			}

			ret = sample_available;
			goto cleanup;
		    }
		    
		    //read nal from parser output
		    nkn_vfs_fseek(fid, parser->output->offset, SEEK_SET);
		    nal_size = parser->output->num_bytes;
		    nalu = (uint8_t*)malloc(parser->output->num_bytes);
		    nkn_vpe_buffered_read(nalu, 1, nal_size, fid);
		    
		    ret = build_mpeg4_rtp_packet(bldr, 0,  nal_size, nalu, 1, 1);
		    bldr->nal->num_bytes_written = 0;
		    bldr->nal->DTS = *parser->look_ahead_time;
		    if(nalu != NULL)
			SAFE_FREE(nalu);

		}while(ret != RTP_PKT_READY);
	    }
	    break;
	default:
	    break;
    }
 cleanup:
    return ret;
}


int32_t append_list_deterministic(Sample_rtp_info_t *rtp_info){

    struct iovec *vec;
    rtp_packet_builder_t *bldr;
    int32_t i;
    uint8_t *data;

    bldr = rtp_info->pkt_builder;
    data =  bldr->rtp_pkts;

    for(i=0;i<bldr->num_rtp_pkts;i++){
	vec = (struct iovec*)calloc(1, sizeof(struct iovec));
	vec->iov_base = data;
	vec->iov_len = (size_t)(*(bldr->rtp_pkt_size+i));
	rtp_info->pkt_list = g_list_prepend(rtp_info->pkt_list, vec);
	data+=vec->iov_len;
	//	SAFE_FREE(vec->iov_base);
	//SAFE_FREE(vec);
    }
    return 0;
}

int32_t 
packet_ready_handler(rtp_packet_builder_t *bldr, void *cb)
{
    struct iovec *vec;
    Sample_rtp_info_t *rtp_info;
    
    vec = (struct iovec*)calloc(1, sizeof(struct iovec));
    rtp_info = (Sample_rtp_info_t*)(cb);

    vec->iov_base = bldr->rtp_pkts;
    vec->iov_len = (size_t)bldr->payload_size;
    rtp_info->pkt_list = g_list_prepend(rtp_info->pkt_list, vec);

    return 0;
}

parser_t *
init_parser_state(all_info_t *inf, FILE *fin, int32_t track_type) 
{
    parser_t *st;

    st = (parser_t*)calloc(1, sizeof(parser_t));

    //init the unhinted parser state
    st->st_all_info = inf;
    st->trak_type = track_type;
    st->fid = fin;
    st->look_ahead_time = (int32_t*)calloc(1, sizeof(int32_t));
    st->output = (output_details_t*)calloc(1, sizeof(output_details_t));

    return st;
}
#if 0
int32_t cleanup_parser_state(parser_t *parser){
    free_all_stbls(parser->st_all_info->stbl_info);
    free_all_info(parser->st_all_info);
    if(parser->output != NULL)
	SAFE_FREE(parser->output);
    //cleanup_output_details(parser->output);
    if(parser->look_ahead_time != NULL)
	SAFE_FREE(parser->look_ahead_time);
    if(parser != NULL)
	SAFE_FREE(parser);
    return 0;
}
#endif

Sample_rtp_info_t *
init_rtp_sample_info()
{
    Sample_rtp_info_t *sample;
    sample = (Sample_rtp_info_t *)malloc(sizeof(Sample_rtp_info_t));
    //printf("the allocated address for sample is %p \n",sample);
    sample->sample_data = NULL;
    sample->pkt_builder = (rtp_packet_builder_t*)calloc(1, sizeof(rtp_packet_builder_t));
    
    return sample;
}

int32_t
cleanup_rtp_sample_info(Sample_rtp_info_t *sample)
{
    //cleanup_mpeg4_packet_builder(sample->pkt_builder);
    if(sample != NULL)
	{
	    if(sample->pkt_builder != NULL){
		//SAFE_FREE(sample->pkt_builder);
		free(sample->pkt_builder);
	    }
	    //SAFE_FREE(sample);
	    //printf("the free'd address for sample is %p \n",sample);
	    free(sample);
	}

    return 0;
}


int32_t
get_hinted_sdp_info(parser_t *parser, Sample_rtp_info_t *sample, SDP_info_t *sdp_info)
{
    int32_t (*fnp_sdp_builder)(rtp_packet_builder_t *, char **, int32_t *,int32_t,int32_t);

    switch(parser->trak_type) {
	case VIDEO_TRAK:
	    fnp_sdp_builder = build_sdp_avc_info;//(parser->pkt_builder, &sdp_info->sdp, &sdp_info->num_bytes);
	    break;
	case AUDIO_TRAK:
	    fnp_sdp_builder = build_mpeg4_sdp_lines;
	    break;
    }
    fnp_sdp_builder(sample->pkt_builder, (char **)(&sdp_info->sdp), &sdp_info->num_bytes,parser->st_all_info->stbl_info->VideoStbl.height,parser->st_all_info->stbl_info->VideoStbl.width);
    return 0;
	    
}

int32_t
init_rtp_packet_builder(parser_t *parser, Sample_rtp_info_t *sample)
{
    int32_t ret = 0;
      
    switch(parser->trak_type) {
	case VIDEO_TRAK:
	    if(parser->st_all_info->stbl_info->VideoStbl.codec_type == H264_VIDEO)
		init_avc_pkt_builder(parser,sample);
	    else
		ret = -1;
	    break;
	case AUDIO_TRAK:
	    init_mpeg4_packet_builder(parser, sample->pkt_builder, 96, (fnp_packet_ready_handler)packet_ready_handler, sample);
	    sample->clock = parser->st_all_info->stbl_info->AudioStbl.timescale;
	    break;
    }

    return ret;
}

void
cleanup_rtp_packet_builder(parser_t *parser, Sample_rtp_info_t *sample)
{
    switch(parser->trak_type) {
        case VIDEO_TRAK:
	    cleanup_avc_packet_builder(sample->pkt_builder);
	    break;
        case AUDIO_TRAK:
            cleanup_mpeg4_packet_builder(sample->pkt_builder);
            break;
    }

}



void init_avc_pkt_builder(parser_t* parser,Sample_rtp_info_t *rtp_sample){
    uint8_t *avcc_data,*initial_avcc_data;
    int32_t avc1_size,*avc1_offset;
    avc1_offset = (int32_t*)calloc(1,sizeof(int32_t));
    rtp_sample->clock = 90000;
    rtp_sample->sample_time = 0;
    rtp_sample->ft= ( rtp_sample->clock/parser->st_all_info->stbl_info->VideoStbl.timescale);
    if(rtp_sample->pkt_builder == NULL){
	rtp_sample->pkt_builder = (rtp_packet_builder_t*)calloc(1,sizeof(rtp_packet_builder_t));
    }
    if(rtp_sample->pkt_builder->avc1 == NULL){
	avc1_size =  getavcc(parser->st_all_info,VIDEO_TRAK,nkn_vpe_buffered_read,parser->fid,avc1_offset);
	avcc_data = (uint8_t*)malloc(sizeof(uint8_t)*avc1_size);
	initial_avcc_data = avcc_data;
	nkn_vfs_fseek(parser->fid,*avc1_offset,SEEK_SET);
	nkn_vpe_buffered_read(avcc_data,avc1_size,sizeof(uint8_t),parser->fid);
	rtp_sample->pkt_builder->avc1 = (AVC_conf_Record_t*)malloc(sizeof(AVC_conf_Record_t));
	read_avc1_box(rtp_sample->pkt_builder->avc1,avcc_data, 100);
	nkn_vfs_fseek(parser->fid,0,SEEK_SET);
	if(initial_avcc_data != NULL)
	    SAFE_FREE(initial_avcc_data);
    }
    if(avc1_offset != NULL)
	SAFE_FREE(avc1_offset);

}


