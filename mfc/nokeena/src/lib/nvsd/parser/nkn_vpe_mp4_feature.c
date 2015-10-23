
/*
 *
 * Filename:  mp4_feature.cpp
 * Date:      2009/03/23
 * Module:    Parser for hinted MP4 files
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

#include "nkn_vpe_mp4_parser.h"
#include "nkn_vpe_types.h"
#include "rtp_packetizer_common.h"
#include <string.h>
#include <sys/time.h>

#include "nkn_vpe_mp4_feature.h"
#include "string.h"

//#define _OLD_AUDIO_
//#define PERF_DEBUG1
#define VFS_READ_ERROR 122



extern size_t nkn_fread_opt(void *ptr, size_t size, size_t nmemb, FILE *stream,
				  void *temp_ptr, int *tofree);

int32_t timeval_subtract1 (struct timeval *x, struct timeval *y, struct timeval *result);
/* computes the difference between two timeval structures
 */
int32_t
timeval_subtract1(struct timeval *x, struct timeval *y, struct timeval *result)
{

    /* Perform the carry for the later subtraction by updating y. */
    if (x->tv_usec < y->tv_usec) {
	int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
	y->tv_usec -= 1000000 * nsec;
	y->tv_sec += nsec;
    }
    if (x->tv_usec - y->tv_usec > 1000000) {
	int nsec = (x->tv_usec - y->tv_usec) / 1000000;
	y->tv_usec += 1000000 * nsec;
	y->tv_sec -= nsec;
    }
     
    /* Compute the time remaining to wait.
       tv_usec is certainly positive. */
    result->tv_sec = x->tv_sec - y->tv_sec;
    result->tv_usec = x->tv_usec - y->tv_usec;
     
    /* Return 1 if result is negative. */
    return x->tv_sec < y->tv_sec;
}


#define DBG

/** inits the Seekinfo_t container
 */
int32_t init_seek_info(Seekinfo_t* seekinfo){
    seekinfo->Video.num_samples_sent=0;
    seekinfo->Audio.num_samples_sent=0;
    seekinfo->HintV.num_samples_sent=0;
    seekinfo->HintA.num_samples_sent=0;
    return 1;
}
#ifdef _SUMALATHA_OPT_MAIN_
/**************************************************************************
 *
 *                            API FUNCTIONS
 *
 **************************************************************************/
//#ifndef _USE_INTERFACE_
int32_t send_next_pkt(Seekinfo_t* seekinfo,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* fid,Media_stbl_t* media_stbl, send_ctx_t **ctx,Hint_sample_t * hint_video, Hint_sample_t * hint_audio, int32_t trak_type,int32_t mode, Seek_params_t* seek_params){

    //Note video,audio,hint, info etc.
    //Copy all the stsc,stsz, stco for all video and audio and hint
    int32_t first_instance = 1;
    uint8_t * data,*p_data;
    size_t size,bytes_read=0;
    box_t box;
    int32_t box_size;
    //Sampl_prop_t *vid_s, *aud_s,* hvid_s, *haud_s;
    send_ctx_t *st;
    int32_t ret, rv,offset;

    //variable initialization
    ret = 0;
    rv = 0;

    //init call, context SHOULD be NULL; this call builds the sample table for Audio,Video 
    //and their respective hint tracks
    if(!(*ctx)){
        //Collect the assets for audio video and hint traks.
        int64_t fpos;
        //Stbl_t *stbl;
        int32_t word=0;


        st = *ctx = (send_ctx_t*)malloc(sizeof(send_ctx_t));
        st->vid_s = (Sampl_prop_t*)calloc(1, sizeof(Sampl_prop_t));
        st->aud_s = (Sampl_prop_t*)calloc(1, sizeof(Sampl_prop_t));
        st->hvid_s = (Sampl_prop_t*)calloc(1, sizeof(Sampl_prop_t));
        st->haud_s = (Sampl_prop_t*)calloc(1, sizeof(Sampl_prop_t));

        st->vid_s->prev_base=0;
        st->aud_s->prev_base=0;
        st->hvid_s->prev_base=0;
        st->haud_s->prev_base=0;
        st->vid_s->size_offset=0;
        st->aud_s->size_offset=0;
        st->hvid_s->size_offset=0;
        st->haud_s->size_offset=0;
	st->haud_s->prev_sample_details.sample_num = 0;
        st->aud_s->prev_sample_details.sample_num = 0;
	st->haud_s->prev_sample_details.is_stateless = 0;
        st->aud_s->prev_sample_details.is_stateless = 0;
        st->hvid_s->prev_sample_details.sample_num = 0;
        st->vid_s->prev_sample_details.sample_num = 0;


        st->hvid_s->prev_sample_details.chunk_count = 0;
        st->vid_s->prev_sample_details.chunk_count = 0;
        st->haud_s->prev_sample_details.chunk_count = 0;
        st->aud_s->prev_sample_details.chunk_count = 0;
        st->hvid_s->prev_sample_details.prev_sample_total = 0;
        st->vid_s->prev_sample_details.prev_sample_total  = 0;
        st->haud_s->prev_sample_details.prev_sample_total = 0;
        st->aud_s->prev_sample_details.prev_sample_total  = 0;

        first_instance=0;
        box_size=0;
        data=(uint8_t*)malloc(8*sizeof(uint8_t));
        //stbl = (Stbl_t*)malloc(sizeof(Stbl_t));

	//search for the MOOV box and read it
        while(word!=MOOV_HEX_ID){
            nkn_vfs_fseek(fid,box_size,SEEK_SET);
            if((*nkn_fread)(data,8,1,fid)==0)
		return NOMORE_SAMPLES;
            box_size=_read32(data,0);
            word=_read32(data,4);
        }

        //in moov position
        fpos=nkn_vfs_ftell(fid);
        fpos-=8;
	offset = (int32_t)(fpos);
	init_stbls(&media_stbl->HintVidStbl,offset);
	init_stbls(&media_stbl->HintAudStbl,offset);
	init_stbls(&media_stbl->VideoStbl,offset);
	init_stbls(&media_stbl->AudioStbl,offset);

        nkn_vfs_fseek(fid,fpos,SEEK_SET);
	if(data != NULL)
	    free(data);
        data=(uint8_t*)malloc(box_size*sizeof(uint8_t));
	if((*nkn_fread)(data,box_size,1,fid)==0)
	    return NOMORE_SAMPLES;
        size=box_size;
        box_size = read_next_root_level_box(data, size, &box, &bytes_read);
        p_data=data;
	//build the Sample Tables;

	rv = get_Media_stbl_info(data,media_stbl,box_size);
	if(p_data != NULL)
	    free(p_data);
	if(media_stbl->HintVidStbl.stco_tab.size==-1&&media_stbl->VideoStbl.stco_tab.size>0)
            rv = MP4_PARSER_ERROR;
        if(media_stbl->HintAudStbl.stco_tab.size==-1&&media_stbl->AudioStbl.stco_tab.size>0)
            rv = MP4_PARSER_ERROR;
    } else {//parse and return offsets


	switch(trak_type){
	    case VIDEO_TRAK:
		if(seekinfo->HintV.num_samples_sent == 0 && media_stbl->HintVidStbl.stco == NULL) {
		    read_stbls_opt(&media_stbl->HintVidStbl,nkn_fread,fid);
		    read_stbls_opt(&media_stbl->VideoStbl,nkn_fread,fid);
		}
		break;
	    case AUDIO_TRAK:
		if(seekinfo->HintA.num_samples_sent == 0 && media_stbl->HintAudStbl.stco == NULL) {
		    read_stbls_opt(&media_stbl->HintAudStbl,nkn_fread,fid);
		    read_stbls_opt(&media_stbl->AudioStbl,nkn_fread,fid);
		}
		break;
	}
        if(mode == _NKN_SEEK_){
            st = *ctx;
	    switch(trak_type){
		case VIDEO_TRAK:
                    // Get closest sync sample number:
                    //Get alter the states accordingly:st->hvid_s,st->vid_s
		    st->hvid_s->prev_sample_details.chunk_count = 0;
		    st->vid_s->prev_sample_details.chunk_count = 0;
		    st->hvid_s->prev_sample_details.prev_sample_total = 0;
		    st->vid_s->prev_sample_details.prev_sample_total  = 0;
		    if(st->hvid_s->total_sample_size==0){
			st->hvid_s->total_sample_size = get_total_sample_size_opt(&media_stbl->HintVidStbl,nkn_fread,fid);
			st->vid_s->total_sample_size = get_total_sample_size_opt(&media_stbl->VideoStbl,nkn_fread,fid);
                    }
                    seekinfo->HintV.num_samples_sent=seek_state_change_opt(&media_stbl->HintVidStbl,seek_params->seek_time,media_stbl->HintVidStbl.timescale,st->hvid_s,nkn_fread,fid);
		    seekinfo->Video.num_samples_sent=seek_state_change_opt(&media_stbl->VideoStbl,seek_params->seek_time,media_stbl->VideoStbl.timescale,st->vid_s,nkn_fread,fid);

		    st->hvid_s->look_ahead_time = get_time_offset_opt(&media_stbl->HintVidStbl,seekinfo->HintV.num_samples_sent,nkn_fread,fid);
                    break;
		case AUDIO_TRAK:
		    st->haud_s->prev_sample_details.chunk_count = 0;
		    st->aud_s->prev_sample_details.chunk_count = 0;
		    st->haud_s->prev_sample_details.prev_sample_total = 0;
		    st->aud_s->prev_sample_details.prev_sample_total  = 0;
		    if(st->haud_s->total_sample_size==0){
                        st->haud_s->total_sample_size = get_total_sample_size_opt(&media_stbl->HintAudStbl,nkn_fread,fid);
			st->aud_s->total_sample_size = get_total_sample_size_opt(&media_stbl->AudioStbl,nkn_fread,fid);
                    }

                    seekinfo->HintA.num_samples_sent=seek_state_change_opt(&media_stbl->HintAudStbl,seek_params->seek_time,media_stbl->HintAudStbl.timescale,st->haud_s,nkn_fread,fid);

		    media_seek_state_change_opt(&media_stbl->AudioStbl,st->aud_s,seekinfo->HintA.num_samples_sent,nkn_fread,fid);

		    seekinfo->Audio.num_samples_sent = seekinfo->HintA.num_samples_sent;//-=1;
		    //		    seekinfo->Audio.num_samples_sent-=1;seekinfo->HintA.num_samples_sent-=1;

		    st->haud_s->look_ahead_time = get_time_offset_opt(&media_stbl->HintAudStbl,seekinfo->HintA.num_samples_sent,nkn_fread,fid);
                    break;
            }
        }
        else{

        int32_t i,j;
	uint8_t *dta;
        RTP_whole_pkt_t *rtp_header;
	int32_t sample_to_be_sent_aud, sample_to_be_sent_vid;
#ifdef PERF_DEBUG1
    struct timeval selfTimeStart, selfTimeEnd;
    int64_t skew;
#endif

	sample_to_be_sent_aud = sample_to_be_sent_vid = 0;
        st = *ctx;
#ifdef PERF_DEBUG1
    gettimeofday(&selfTimeStart, NULL); 
#endif

	switch(trak_type) {
	    case VIDEO_TRAK:
		if((seekinfo->HintV.num_samples_sent+SAMPLES_TO_BE_SENT<=st->hvid_s->total_sample_size)||(seekinfo->HintV.num_samples_sent==0)) {
		    ret=find_chunk_asset_opt(seekinfo->HintV.num_bytes,seekinfo->HintV.offset,SAMPLES_TO_BE_SENT,seekinfo->HintV.num_samples_sent,&media_stbl->HintVidStbl,st->hvid_s,nkn_fread,fid);
		    ret=find_chunk_asset_opt(seekinfo->Video.num_bytes,seekinfo->Video.offset,SAMPLES_TO_BE_SENT,seekinfo->Video.num_samples_sent,&media_stbl->VideoStbl,st->vid_s,nkn_fread,fid);

		    sample_to_be_sent_vid = SAMPLES_TO_BE_SENT;
		} 
		else {
		    if(st->hvid_s->total_sample_size - seekinfo->HintV.num_samples_sent<SAMPLES_TO_BE_SENT&&(st->hvid_s->total_sample_size - seekinfo->HintV.num_samples_sent >0)){
			sample_to_be_sent_vid = st->hvid_s->total_sample_size-seekinfo->HintV.num_samples_sent;
			ret=find_chunk_asset_opt(seekinfo->HintV.num_bytes,seekinfo->HintV.offset,sample_to_be_sent_vid,seekinfo->HintV.num_samples_sent,&media_stbl->HintVidStbl,st->hvid_s,nkn_fread,fid);
			ret=find_chunk_asset_opt(seekinfo->Video.num_bytes,seekinfo->Video.offset,sample_to_be_sent_vid,seekinfo->Video.num_samples_sent,&media_stbl->VideoStbl,st->vid_s,nkn_fread,fid);
		    } 
		    else if(st->hvid_s->total_sample_size == seekinfo->HintV.num_samples_sent) {
			rv = NOMORE_SAMPLES;
			goto cleanup;
		    }
		}
#ifdef PERF_DEBUG1
		{
		    struct timeval result1;
		    gettimeofday(&selfTimeEnd,NULL);
		    timeval_subtract1(&selfTimeEnd, &selfTimeStart, &result1);
		    //printf("TRAK TYPE %d, Sample Num %d, Time Elapsed %lu <---------> Expected Time %d ===== next DTS is  %d\n", psmss->trackType, psmss->sampleNumber, psmss->timeElapsed, psmss->lastDTS, psmss->nextDTS);
		    printf("TRAK TYPE %d, Self Time %ld\n", trak_type, (int64_t)((result1.tv_sec*1e6)+result1.tv_usec));
		}
#endif
		for(i=0;i<sample_to_be_sent_vid;i++) {
		    dta =(uint8_t*) malloc(seekinfo->HintV.num_bytes[i]);
		    nkn_vfs_fseek(fid,seekinfo->HintV.offset[i],SEEK_SET);
		    (*nkn_fread)(dta,seekinfo->HintV.num_bytes[i],1,fid);
		    read_rtp_sample(dta,&hint_video[i],seekinfo->HintV.offset[i], seekinfo->Video.offset[i]);
		    //          (hint_video+i)->rtp->header.relative_time+= get_time_offset(media_stbl->HintVidStbl.stts,seekinfo->HintV.num_samples_sent+i);
		    for(j=0;j<(hint_video+i)->num_rtp_pkts;j++) {
			rtp_header = (hint_video+i)->rtp;
			rtp_header+=j;
			rtp_header->header.relative_time+= get_time_offset_opt(&media_stbl->HintVidStbl,seekinfo->HintV.num_samples_sent+i,nkn_fread,fid) + rtp_header->time_stamp;
		    }
		    if(dta != NULL){
			free(dta);
			dta = NULL;
		    }
		}
		seekinfo->HintV.num_samples_sent+=sample_to_be_sent_vid;
		seekinfo->Video.num_samples_sent+=sample_to_be_sent_vid;
		
		break;
	    case AUDIO_TRAK:

		{
		    int uu = 0;
		    if(seekinfo->HintA.num_samples_sent == 4030) {
			uu = 1;
		    }
		}

		if((seekinfo->HintA.num_samples_sent+SAMPLES_TO_BE_SENT<=st->haud_s->total_sample_size)||(seekinfo->HintA.num_samples_sent==0)) {
		    ret=find_chunk_asset_opt(seekinfo->HintA.num_bytes,seekinfo->HintA.offset,SAMPLES_TO_BE_SENT,seekinfo->HintA.num_samples_sent,&media_stbl->HintAudStbl,st->haud_s,nkn_fread,fid);
                    ret=find_chunk_asset_opt(seekinfo->Audio.num_bytes,seekinfo->Audio.offset,SAMPLES_TO_BE_SENT,seekinfo->Audio.num_samples_sent,&media_stbl->AudioStbl,st->aud_s,nkn_fread,fid);

		    sample_to_be_sent_aud = SAMPLES_TO_BE_SENT;
		} else {
		    if(st->haud_s->total_sample_size - seekinfo->HintA.num_samples_sent<SAMPLES_TO_BE_SENT&&(st->haud_s->total_sample_size - seekinfo->HintA.num_samples_sent >0)){
			sample_to_be_sent_aud = st->haud_s->total_sample_size-seekinfo->HintA.num_samples_sent;
			ret=find_chunk_asset_opt(seekinfo->HintA.num_bytes,seekinfo->HintA.offset,sample_to_be_sent_aud,seekinfo->HintA.num_samples_sent,&media_stbl->HintAudStbl,st->haud_s,nkn_fread,fid);
                        ret=find_chunk_asset_opt(seekinfo->Audio.num_bytes,seekinfo->Audio.offset,sample_to_be_sent_aud,seekinfo->Audio.num_samples_sent,&media_stbl->AudioStbl,st->aud_s,nkn_fread,fid);

		    } else if(st->haud_s->total_sample_size == seekinfo->HintA.num_samples_sent) {
			rv =  NOMORE_SAMPLES;
			goto cleanup;
		    }
		}

#ifdef PERF_DEBUG1    
    {
	struct timeval result1;
	gettimeofday(&selfTimeEnd,NULL);
	timeval_subtract1(&selfTimeEnd, &selfTimeStart, &result1);
	//printf("TRAK TYPE %d, Sample Num %d, Time Elapsed %lu <---------> Expected Time %d ===== next DTS is  %d\n", psmss->trackType, psmss->sampleNumber, psmss->timeElapsed, psmss->lastDTS, psmss->nextDTS);
	printf("TRAK TYPE %d, Self Time %ld\n", trak_type, (int64_t)((result1.tv_sec*1e6)+result1.tv_usec));
    }
#endif
		for(i=0;i<sample_to_be_sent_aud;i++) {
		    dta =(uint8_t*) malloc(seekinfo->HintA.num_bytes[i]);
		    nkn_vfs_fseek(fid,seekinfo->HintA.offset[i],SEEK_SET);
		    (*nkn_fread)(dta,seekinfo->HintA.num_bytes[i],1,fid);
		    //		    read_rtp_sample_audio(dta,&hint_audio[i],seekinfo->HintA.offset[i], seekinfo->Audio.offset[i], &media_stbl->HintAudStbl, &media_stbl->AudioStbl);
		    read_rtp_sample_audio(dta,&hint_audio[i],seekinfo->HintA.offset[i], seekinfo->Audio.offset[i], &media_stbl->HintAudStbl, &media_stbl->AudioStbl,&st->haud_s->prev_sample_details,&st->aud_s->prev_sample_details,nkn_fread,fid);
		    for(j=0;j<(hint_audio+i)->num_rtp_pkts;j++) {
			//int32_t time_temp;
			rtp_header = (hint_audio+i)->rtp;
			rtp_header+=j;
			rtp_header->header.relative_time+= get_time_offset_opt(&media_stbl->HintAudStbl,seekinfo->HintA.num_samples_sent+i,nkn_fread,fid) + rtp_header->time_stamp;
		    }
		    if(dta != NULL){
			free(dta);
			dta = NULL;
		    }
		}

		seekinfo->HintA.num_samples_sent+=sample_to_be_sent_aud;
		seekinfo->Audio.num_samples_sent+=sample_to_be_sent_aud;
		break;
		/*CHANGES CHANGES HERE*/
	}

	
	if(trak_type == AUDIO_TRAK){
	    st->haud_s->look_ahead_time = get_time_offset_opt(&media_stbl->HintAudStbl,seekinfo->HintA.num_samples_sent+1,nkn_fread,fid);
	}
	else
	    st->hvid_s->look_ahead_time = get_time_offset_opt(&media_stbl->HintVidStbl,seekinfo->HintV.num_samples_sent+1,nkn_fread,fid);
	
	goto cleanup;
    }

	//printf("\n\t\tSAMple number =  %d\n",seekinfo->HintV.num_samples_sent);
    }
    printf("total = %d Samples number %d\n\n",st->hvid_s->total_sample_size,seekinfo->HintV.num_samples_sent);
 cleanup:
    return rv;
}

int32_t send_next_pkt_interface(Seekinfo_t* seekinfo,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* fid,Media_stbl_t* media_stbl, send_ctx_t *st,Media_sample_t * hint_video, Media_sample_t * hint_audio, int32_t trak_type){

    //Note video,audio,hint, info etc.
    //Copy all the stsc,stsz, stco for all video and audio and hint

    int32_t ret, rv;

    //variable initialization
    ret = 0;
    rv = 0;

	switch(trak_type){
	    case VIDEO_TRAK:
		if(seekinfo->HintV.num_samples_sent == 0 && media_stbl->HintVidStbl.stco == NULL) {
		    read_stbls_opt(&media_stbl->HintVidStbl,nkn_fread,fid);
		    read_stbls_opt(&media_stbl->VideoStbl,nkn_fread,fid);
		}
		break;
	    case AUDIO_TRAK:
		if(seekinfo->HintA.num_samples_sent == 0 && media_stbl->HintAudStbl.stco == NULL) {
		    read_stbls_opt(&media_stbl->HintAudStbl,nkn_fread,fid);
		    read_stbls_opt(&media_stbl->AudioStbl,nkn_fread,fid);
		}
		break;
	}

        int32_t i,j;
	uint8_t *dta;
        RTP_whole_pkt_t *rtp_header;
	int32_t sample_to_be_sent_aud, sample_to_be_sent_vid;
#if 0//def PERF_DEBUG1
    struct timeval selfTimeStart, selfTimeEnd;
    int64_t skew;
#endif

	sample_to_be_sent_aud = sample_to_be_sent_vid = 0;
        //st = *ctx;
#if 0//def PERF_DEBUG1
    gettimeofday(&selfTimeStart, NULL); 
#endif

	switch(trak_type) {
	    case VIDEO_TRAK:
		if((seekinfo->HintV.num_samples_sent+SAMPLES_TO_BE_SENT<=st->hvid_s->total_sample_size)||(seekinfo->HintV.num_samples_sent==0)) {
		    ret=find_chunk_asset_opt(seekinfo->HintV.num_bytes,seekinfo->HintV.offset,SAMPLES_TO_BE_SENT,seekinfo->HintV.num_samples_sent,&media_stbl->HintVidStbl,st->hvid_s,nkn_fread,fid);
		    ret=find_chunk_asset_opt(seekinfo->Video.num_bytes,seekinfo->Video.offset,SAMPLES_TO_BE_SENT,seekinfo->Video.num_samples_sent,&media_stbl->VideoStbl,st->vid_s,nkn_fread,fid);

		    sample_to_be_sent_vid = SAMPLES_TO_BE_SENT;
		} else {
		    if(st->hvid_s->total_sample_size - seekinfo->HintV.num_samples_sent<SAMPLES_TO_BE_SENT&&(st->hvid_s->total_sample_size - seekinfo->HintV.num_samples_sent >0)){
			sample_to_be_sent_vid = st->hvid_s->total_sample_size-seekinfo->HintV.num_samples_sent;
			ret=find_chunk_asset_opt(seekinfo->HintV.num_bytes,seekinfo->HintV.offset,sample_to_be_sent_vid,seekinfo->HintV.num_samples_sent,&media_stbl->HintVidStbl,st->hvid_s,nkn_fread,fid);
			ret=find_chunk_asset_opt(seekinfo->Video.num_bytes,seekinfo->Video.offset,sample_to_be_sent_vid,seekinfo->Video.num_samples_sent,&media_stbl->VideoStbl,st->vid_s,nkn_fread,fid);

		    } else if(st->hvid_s->total_sample_size == seekinfo->HintV.num_samples_sent) {
			rv = NOMORE_SAMPLES;
			goto cleanup;
		    }
		}
#ifdef PERF_DEBUG1
		{
		    struct timeval result1;
		    gettimeofday(&selfTimeEnd,NULL);
		    timeval_subtract1(&selfTimeEnd, &selfTimeStart, &result1);
		    //printf("TRAK TYPE %d, Sample Num %d, Time Elapsed %lu <---------> Expected Time %d ===== next DTS is  %d\n", psmss->trackType, psmss->sampleNumber, psmss->timeElapsed, psmss->lastDTS, psmss->nextDTS);
		    printf("TRAK TYPE %d, Self Time %ld\n", trak_type, (int64_t)((result1.tv_sec*1e6)+result1.tv_usec));
		}
#endif
		for(i=0;i<sample_to_be_sent_vid;i++) {
		    dta =(uint8_t*) malloc(seekinfo->HintV.num_bytes[i]);
		    nkn_vfs_fseek(fid,seekinfo->HintV.offset[i],SEEK_SET);
		    (*nkn_fread)(dta,seekinfo->HintV.num_bytes[i],1,fid);
		    //read_rtp_sample(dta,&hint_video[i],seekinfo->HintV.offset[i], seekinfo->Video.offset[i]);
		    read_rtp_sample_interface(dta,&hint_video[i],seekinfo->HintV.offset[i], seekinfo->Video.offset[i]);
		    //          (hint_video+i)->rtp->header.relative_time+= get_time_offset(media_stbl->HintVidStbl.stts,seekinfo->HintV.num_samples_sent+i);
		    for(j=0;j<(hint_video+i)->num_rtp_pkts;j++) {
			rtp_header = (hint_video+i)->rtp;
			rtp_header+=j;
			rtp_header->header.relative_time+= get_time_offset_opt(&media_stbl->HintVidStbl,seekinfo->HintV.num_samples_sent+i,nkn_fread,fid) + rtp_header->time_stamp;
		    }
		    if(dta != NULL){
			free(dta);
			dta = NULL;
		    }
		}
		seekinfo->HintV.num_samples_sent+=sample_to_be_sent_vid;
		seekinfo->Video.num_samples_sent+=sample_to_be_sent_vid;
		
		break;
	    case AUDIO_TRAK:

		{
		    int uu = 0;
		    if(seekinfo->HintA.num_samples_sent == 4030) {
			uu = 1;
		    }
		}

		if((seekinfo->HintA.num_samples_sent+SAMPLES_TO_BE_SENT<=st->haud_s->total_sample_size)||(seekinfo->HintA.num_samples_sent==0)) {

		    ret=find_chunk_asset_opt(seekinfo->HintA.num_bytes,seekinfo->HintA.offset,SAMPLES_TO_BE_SENT,seekinfo->HintA.num_samples_sent,&media_stbl->HintAudStbl,st->haud_s,nkn_fread,fid);
                    ret=find_chunk_asset_opt(seekinfo->Audio.num_bytes,seekinfo->Audio.offset,SAMPLES_TO_BE_SENT,seekinfo->Audio.num_samples_sent,&media_stbl->AudioStbl,st->aud_s,nkn_fread,fid);
		    sample_to_be_sent_aud = SAMPLES_TO_BE_SENT;
		} else {
		    if(st->haud_s->total_sample_size - seekinfo->HintA.num_samples_sent<SAMPLES_TO_BE_SENT&&(st->haud_s->total_sample_size - seekinfo->HintA.num_samples_sent >0)){
			sample_to_be_sent_aud = st->haud_s->total_sample_size-seekinfo->HintA.num_samples_sent;
			ret=find_chunk_asset_opt(seekinfo->HintA.num_bytes,seekinfo->HintA.offset,sample_to_be_sent_aud,seekinfo->HintA.num_samples_sent,&media_stbl->HintAudStbl,st->haud_s,nkn_fread,fid);
                        ret=find_chunk_asset_opt(seekinfo->Audio.num_bytes,seekinfo->Audio.offset,sample_to_be_sent_aud,seekinfo->Audio.num_samples_sent,&media_stbl->AudioStbl,st->aud_s,nkn_fread,fid);
		    } else if(st->haud_s->total_sample_size == seekinfo->HintA.num_samples_sent) {
			rv =  NOMORE_SAMPLES;
			goto cleanup;
		    }
		}

#ifdef PERF_DEBUG1    
    {
	struct timeval result1;
	gettimeofday(&selfTimeEnd,NULL);
	timeval_subtract1(&selfTimeEnd, &selfTimeStart, &result1);
	//printf("TRAK TYPE %d, Sample Num %d, Time Elapsed %lu <---------> Expected Time %d ===== next DTS is  %d\n", psmss->trackType, psmss->sampleNumber, psmss->timeElapsed, psmss->lastDTS, psmss->nextDTS);
	printf("TRAK TYPE %d, Self Time %ld\n", trak_type, (int64_t)((result1.tv_sec*1e6)+result1.tv_usec));
    }
#endif
		for(i=0;i<sample_to_be_sent_aud;i++) {
		    dta =(uint8_t*) malloc(seekinfo->HintA.num_bytes[i]);
		    nkn_vfs_fseek(fid,seekinfo->HintA.offset[i],SEEK_SET);
		    (*nkn_fread)(dta,seekinfo->HintA.num_bytes[i],1,fid);
		    //		    read_rtp_sample_audio(dta,&hint_audio[i],seekinfo->HintA.offset[i], seekinfo->Audio.offset[i], &media_stbl->HintAudStbl, &media_stbl->AudioStbl);
		    read_rtp_sample_audio_interface(dta,&hint_audio[i],seekinfo->HintA.offset[i], seekinfo->Audio.offset[i], &media_stbl->HintAudStbl, &media_stbl->AudioStbl,&st->haud_s->prev_sample_details,&st->aud_s->prev_sample_details,nkn_fread,fid);
		    for(j=0;j<(hint_audio+i)->num_rtp_pkts;j++) {
			//int32_t time_temp;
			rtp_header = (hint_audio+i)->rtp;
			rtp_header+=j;
			rtp_header->header.relative_time+= get_time_offset_opt(&media_stbl->HintAudStbl,seekinfo->HintA.num_samples_sent+i,nkn_fread,fid) + rtp_header->time_stamp;
		    }
		    if(dta != NULL){
			free(dta);
			dta = NULL;
		    }
		}

		seekinfo->HintA.num_samples_sent+=sample_to_be_sent_aud;
		seekinfo->Audio.num_samples_sent+=sample_to_be_sent_aud;
		break;
		/*CHANGES CHANGES HERE*/
	}

	
	if(trak_type == AUDIO_TRAK){
	    st->haud_s->look_ahead_time = get_time_offset_opt(&media_stbl->HintAudStbl,seekinfo->HintA.num_samples_sent+1,nkn_fread,fid);
	}
	else{
	    st->hvid_s->look_ahead_time = get_time_offset_opt(&media_stbl->HintVidStbl,seekinfo->HintV.num_samples_sent+1,nkn_fread,fid);
	}
	goto cleanup;

 cleanup:

    return rv;
}


int32_t  send_next_pkt_init(Seekinfo_t* seekinfo,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* fid,Media_stbl_t*  media_stbl, send_ctx_t *st){


	 //Note video,audio,hint, info etc.
	 //Copy all the stsc,stsz, stco for all video and audio and hint
	
	 uint8_t * data,*p_data;
	 size_t size,bytes_read=0;
	 box_t box;
	 int32_t box_size;
	 int32_t  rv,offset;

	 //variable initialization
	 rv = 0;

	 //init call, context SHOULD be NULL; this call builds the sample table for Audio,Video
	 //and their respective hint tracks
 
	     //Collect the assets for audio video and hint traks.
	     int64_t fpos;
	     //Stbl_t *stbl;
	     int32_t word=0;
	     st->vid_s->prev_base=0;
	     st->aud_s->prev_base=0;
	     st->hvid_s->prev_base=0;
	     st->haud_s->prev_base=0;
	     st->vid_s->size_offset=0;
	     st->aud_s->size_offset=0;
	     st->hvid_s->size_offset=0;
	     st->haud_s->size_offset=0;
	     st->haud_s->prev_sample_details.sample_num = 0;
	     st->aud_s->prev_sample_details.sample_num = 0;
	     st->hvid_s->prev_sample_details.sample_num = 0;
	     st->vid_s->prev_sample_details.sample_num = 0;
	     st->haud_s->prev_sample_details.is_stateless = 0;
	     st->aud_s->prev_sample_details.is_stateless = 0;
	     st->hvid_s->prev_sample_details.is_stateless = 0;
	     st->vid_s->prev_sample_details.is_stateless = 0;


	     st->hvid_s->prev_sample_details.chunk_count = 0;
	     st->vid_s->prev_sample_details.chunk_count = 0;
	     st->haud_s->prev_sample_details.chunk_count = 0;
	     st->aud_s->prev_sample_details.chunk_count = 0;
	     st->hvid_s->prev_sample_details.prev_sample_total = 0;
	     st->vid_s->prev_sample_details.prev_sample_total  = 0;
	     st->haud_s->prev_sample_details.prev_sample_total = 0;
	     st->aud_s->prev_sample_details.prev_sample_total  = 0;
	     //first_instance=0;
	     box_size=0;
	     data=(uint8_t*)malloc(8*sizeof(uint8_t));
	     //stbl = (Stbl_t*)malloc(sizeof(Stbl_t));

	     //search for the MOOV box and read it
	     while(word!=MOOV_HEX_ID){
		 nkn_vfs_fseek(fid,box_size,SEEK_SET);
		 (*nkn_fread)(data,8,1,fid);
		 box_size=_read32(data,0);
		 word=_read32(data,4);
	     }
	     //in moov position
	     fpos=nkn_vfs_ftell(fid);
	     fpos-=8;

	     offset = (int32_t)(fpos);
	     init_stbls(&media_stbl->HintVidStbl,offset);
	     init_stbls(&media_stbl->HintAudStbl,offset);
	     init_stbls(&media_stbl->VideoStbl,offset);
	     init_stbls(&media_stbl->AudioStbl,offset);

	     nkn_vfs_fseek(fid,fpos,SEEK_SET);
	     if(data != NULL)
		 free(data);
	     data=(uint8_t*)malloc(box_size*sizeof(uint8_t));
	     (*nkn_fread)(data,box_size,1,fid);
	     size=box_size;
	     box_size = read_next_root_level_box(data, size, &box, &bytes_read);
	     p_data=data;
	     //build the Sample Tables;
	     rv = get_Media_stbl_info(data,media_stbl,box_size);
	     if(p_data != NULL)
		 free(p_data);
	     if(media_stbl->HintVidStbl.stco_tab.size==-1&&media_stbl->VideoStbl.stco_tab.size>0)
		 rv = MP4_PARSER_ERROR;
	     if(media_stbl->HintAudStbl.stco_tab.size==-1&&media_stbl->AudioStbl.stco_tab.size>0)
		 rv = MP4_PARSER_ERROR;
	 
	 return rv;

 }

int32_t send_next_pkt_seek2time(Seekinfo_t* seekinfo,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* fid,Media_stbl_t* media_stbl, send_ctx_t *st, int32_t trak_type, Seek_params_t* seek_params){

    //Note video,audio,hint, info etc.
    //Copy all the stsc,stsz, stco for all video and audio and hint
    //uint8_t * data,*p_data;
    //size_t size,bytes_read=0;
    //box_t box;
    //int32_t box_size;
    //Sampl_prop_t *vid_s, *aud_s,* hvid_s, *haud_s;
    //send_ctx_t *st;
    int32_t ret, rv;

    //variable initialization
    ret = 0;
    rv = 0;

    switch(trak_type){
	case VIDEO_HINT_TRAK:
	    if(seekinfo->HintV.num_samples_sent == 0 && media_stbl->HintVidStbl.stco == NULL) {
		read_stbls_opt(&media_stbl->HintVidStbl,nkn_fread,fid);
		read_stbls_opt(&media_stbl->VideoStbl,nkn_fread,fid);
	    }
	    break;
	case AUDIO_HINT_TRAK:
	    if(seekinfo->HintA.num_samples_sent == 0 && media_stbl->HintAudStbl.stco == NULL) {
		read_stbls_opt(&media_stbl->HintAudStbl,nkn_fread,fid);
		read_stbls_opt(&media_stbl->AudioStbl,nkn_fread,fid);
	    }
	    break;
	case VIDEO_TRAK:
	    if(seekinfo->Video.num_samples_sent == 0 && media_stbl->VideoStbl.stco == NULL) {
		read_stbls_opt(&media_stbl->VideoStbl,nkn_fread,fid);
	    }
	    break;
	case AUDIO_TRAK:
	    if(seekinfo->Audio.num_samples_sent == 0 && media_stbl->AudioStbl.stco == NULL) {
		read_stbls_opt(&media_stbl->AudioStbl,nkn_fread,fid);
	    }
	    break;

    }

    	switch(trak_type){
	    case VIDEO_HINT_TRAK:
		// Get closest sync sample number:
		//Get alter the states accordingly:st->hvid_s,st->vid_s

		st->hvid_s->prev_sample_details.chunk_count = 0;
		st->vid_s->prev_sample_details.chunk_count = 0;
		st->hvid_s->prev_sample_details.prev_sample_total = 0;
		st->vid_s->prev_sample_details.prev_sample_total  = 0;
		if(st->hvid_s->total_sample_size==0){
		    st->hvid_s->total_sample_size = get_total_sample_size_opt(&media_stbl->HintVidStbl,nkn_fread,fid);
		    st->vid_s->total_sample_size = get_total_sample_size_opt(&media_stbl->VideoStbl,nkn_fread,fid);
		}
		seekinfo->HintV.num_samples_sent=seek_state_change_opt(&media_stbl->HintVidStbl,seek_params->seek_time,media_stbl->HintVidStbl.timescale,st->hvid_s,nkn_fread,fid);
		seekinfo->Video.num_samples_sent=seek_state_change_opt(&media_stbl->VideoStbl,seek_params->seek_time,media_stbl->VideoStbl.timescale,st->vid_s,nkn_fread,fid);
		st->hvid_s->look_ahead_time = get_time_offset_opt(&media_stbl->HintVidStbl,seekinfo->HintV.num_samples_sent,nkn_fread,fid);
		break;
	    case AUDIO_HINT_TRAK:

		st->haud_s->prev_sample_details.chunk_count = 0;
		st->aud_s->prev_sample_details.chunk_count = 0;
		st->haud_s->prev_sample_details.prev_sample_total = 0;
		st->aud_s->prev_sample_details.prev_sample_total  = 0;

		if(st->haud_s->total_sample_size==0){
		    st->haud_s->total_sample_size = get_total_sample_size_opt(&media_stbl->HintAudStbl,nkn_fread,fid);
		    st->aud_s->total_sample_size = get_total_sample_size_opt(&media_stbl->AudioStbl,nkn_fread,fid);
		}

		seekinfo->HintA.num_samples_sent=seek_state_change_opt(&media_stbl->HintAudStbl,seek_params->seek_time,media_stbl->HintAudStbl.timescale,st->haud_s,nkn_fread,fid);
		media_seek_state_change_opt(&media_stbl->AudioStbl,st->aud_s,seekinfo->HintA.num_samples_sent,nkn_fread,fid);
		seekinfo->Audio.num_samples_sent = seekinfo->HintA.num_samples_sent;//-=1;
		//              seekinfo->Audio.num_samples_sent-=1;seekinfo->HintA.num_samples_sent-=1;

		st->haud_s->look_ahead_time = get_time_offset_opt(&media_stbl->HintAudStbl,seekinfo->HintA.num_samples_sent,nkn_fread,fid);
		break;
	    case VIDEO_TRAK:
		// Get closest sync sample number:
		//Get alter the states accordingly:st->hvid_s,st->vid_s

		st->vid_s->prev_sample_details.chunk_count = 0;
		st->vid_s->prev_sample_details.prev_sample_total  = 0;
#ifdef NK_CTTS_OPT
		st->vid_s->prev_ctts.prev_ctts_entry = 0;
		st->vid_s->prev_ctts.prev_cum_sample_cnt = 0;
#endif

		if(st->vid_s->total_sample_size==0){
		    st->vid_s->total_sample_size = get_total_sample_size_opt(&media_stbl->VideoStbl,nkn_fread,fid);
		}
		seekinfo->Video.num_samples_sent=seek_state_change_opt(&media_stbl->VideoStbl,seek_params->seek_time,media_stbl->VideoStbl.timescale,st->vid_s,nkn_fread,fid);
		st->vid_s->look_ahead_time = get_time_offset_opt(&media_stbl->VideoStbl,seekinfo->Video.num_samples_sent,nkn_fread,fid);
		break;
	    case AUDIO_TRAK:
		st->aud_s->prev_sample_details.chunk_count = 0;
		st->aud_s->prev_sample_details.prev_sample_total  = 0;
#ifdef NK_CTTS_OPT
		st->aud_s->prev_ctts.prev_ctts_entry = 0;
		st->aud_s->prev_ctts.prev_cum_sample_cnt = 0;
#endif

		if(st->aud_s->total_sample_size==0){
		    st->aud_s->total_sample_size = get_total_sample_size_opt(&media_stbl->AudioStbl,nkn_fread,fid);
		}
		seekinfo->Audio.num_samples_sent=seek_state_change_opt(&media_stbl->AudioStbl,seek_params->seek_time,media_stbl->AudioStbl.timescale,st->aud_s,nkn_fread,fid);
		st->aud_s->look_ahead_time = get_time_offset_opt(&media_stbl->AudioStbl,seekinfo->Audio.num_samples_sent,nkn_fread,fid);
		break;
	}

    return rv;

     }

int32_t send_next_pkt_seek2time_hinted(Seekinfo_t* seekinfo,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* fid,Media_stbl_t* media_stbl, send_ctx_t *st, int32_t trak_type, Seek_params_t* seek_params){

    //Note video,audio,hint, info etc.
    //Copy all the stsc,stsz, stco for all video and audio and hint
    //uint8_t * data,*p_data;
    //size_t size,bytes_read=0;
    //box_t box;
    //int32_t box_size;
    //Sampl_prop_t *vid_s, *aud_s,* hvid_s, *haud_s;
    //send_ctx_t *st;
    int32_t ret, rv;

    //variable initialization
    ret = 0;
    rv = 0;

    switch(trak_type){
	case VIDEO_TRAK:
	    if(seekinfo->HintV.num_samples_sent == 0 && media_stbl->HintVidStbl.stco == NULL) {
		read_stbls_opt(&media_stbl->HintVidStbl,nkn_fread,fid);
		read_stbls_opt(&media_stbl->VideoStbl,nkn_fread,fid);
	    }
	    break;
	case AUDIO_TRAK:
	    if(seekinfo->HintA.num_samples_sent == 0 && media_stbl->HintAudStbl.stco == NULL) {
		read_stbls_opt(&media_stbl->HintAudStbl,nkn_fread,fid);
		read_stbls_opt(&media_stbl->AudioStbl,nkn_fread,fid);
	    }
	    break;
    }

    	switch(trak_type){
	    case VIDEO_TRAK:
		// Get closest sync sample number:
		//Get alter the states accordingly:st->hvid_s,st->vid_s

		st->hvid_s->prev_sample_details.chunk_count = 0;
		st->vid_s->prev_sample_details.chunk_count = 0;
		st->hvid_s->prev_sample_details.prev_sample_total = 0;
		st->vid_s->prev_sample_details.prev_sample_total  = 0;

		if(st->hvid_s->total_sample_size==0){
		    st->hvid_s->total_sample_size = get_total_sample_size_opt(&media_stbl->HintVidStbl,nkn_fread,fid);
		    st->vid_s->total_sample_size = get_total_sample_size_opt(&media_stbl->VideoStbl,nkn_fread,fid);
		}
		seekinfo->HintV.num_samples_sent=seek_state_change_opt(&media_stbl->HintVidStbl,seek_params->seek_time,media_stbl->HintVidStbl.timescale,st->hvid_s,nkn_fread,fid);
		seekinfo->Video.num_samples_sent=seek_state_change_opt(&media_stbl->VideoStbl,seek_params->seek_time,media_stbl->VideoStbl.timescale,st->vid_s,nkn_fread,fid);
		st->hvid_s->look_ahead_time = get_time_offset_opt(&media_stbl->HintVidStbl,seekinfo->HintV.num_samples_sent,nkn_fread,fid);
		break;
	    case AUDIO_TRAK:

		st->haud_s->prev_sample_details.chunk_count = 0;
		st->aud_s->prev_sample_details.chunk_count = 0;
		st->haud_s->prev_sample_details.prev_sample_total = 0;
		st->aud_s->prev_sample_details.prev_sample_total  = 0;

		if(st->haud_s->total_sample_size==0){
		    st->haud_s->total_sample_size = get_total_sample_size_opt(&media_stbl->HintAudStbl,nkn_fread,fid);
		    st->aud_s->total_sample_size = get_total_sample_size_opt(&media_stbl->AudioStbl,nkn_fread,fid);
		}

		seekinfo->HintA.num_samples_sent=seek_state_change_opt(&media_stbl->HintAudStbl,seek_params->seek_time,media_stbl->HintAudStbl.timescale,st->haud_s,nkn_fread,fid);
		media_seek_state_change_opt(&media_stbl->AudioStbl,st->aud_s,seekinfo->HintA.num_samples_sent,nkn_fread,fid);
		seekinfo->Audio.num_samples_sent = seekinfo->HintA.num_samples_sent;//-=1;
		//              seekinfo->Audio.num_samples_sent-=1;seekinfo->HintA.num_samples_sent-=1;

		st->haud_s->look_ahead_time = get_time_offset_opt(&media_stbl->HintAudStbl,seekinfo->HintA.num_samples_sent,nkn_fread,fid);
		break;
	}

    return rv;

     }
int32_t send_next_pkt_get_sample(Seekinfo_t* seekinfo,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* fid,Media_stbl_t*  media_stbl, send_ctx_t *st,Media_sample_t * hint_video, Media_sample_t * hint_audio, Media_sample_t * video, Media_sample_t * audio, int32_t trak_type){

	 //Note video,audio,hint, info etc.
	 //Copy all the stsc,stsz, stco for all video and audio and hint

	 //uint8_t * data,*p_data;
	 //size_t size,bytes_read=0;
	 //box_t box;
	 //int32_t box_size;
	 //Sampl_prop_t *vid_s, *aud_s,* hvid_s, *haud_s;
	 //send_ctx_t *st;
	 int32_t ret, rv;

	 //variable initialization
	 ret = 0;
	 rv = 0;

	 //init call, context SHOULD be NULL; this call builds the sample table for Audio,Video
	 //and their respective hint tracks
	 switch(trak_type){
	     case VIDEO_HINT_TRAK:
		 if(seekinfo->HintV.num_samples_sent == 0 && media_stbl->HintVidStbl.stco == NULL) {
		     read_stbls_opt(&media_stbl->HintVidStbl,nkn_fread,fid);
		     read_stbls_opt(&media_stbl->VideoStbl,nkn_fread,fid);
		 }
		 break;
	     case AUDIO_HINT_TRAK:
		 if(seekinfo->HintA.num_samples_sent == 0 && media_stbl->HintAudStbl.stco == NULL) {
		     read_stbls_opt(&media_stbl->HintAudStbl,nkn_fread,fid);
		     read_stbls_opt(&media_stbl->AudioStbl,nkn_fread,fid);
		 }
		 break;
	     case VIDEO_TRAK:
		 if(seekinfo->Video.num_samples_sent == 0 && media_stbl->VideoStbl.stco == NULL) {
		     read_stbls_opt(&media_stbl->VideoStbl,nkn_fread,fid);
		 }
		 break;
	     case AUDIO_TRAK:
		 if(seekinfo->Audio.num_samples_sent == 0 && media_stbl->AudioStbl.stco == NULL) {
		     read_stbls_opt(&media_stbl->AudioStbl,nkn_fread,fid);
		 }
		 break;

	 }
	 int32_t i,j;//,offset;
	 uint8_t *dta;
	 RTP_whole_pkt_t *rtp_header;
	 int32_t sample_to_be_sent_aud, sample_to_be_sent_vid;
#ifdef PERF_DEBUG1
	 struct timeval selfTimeStart, selfTimeEnd;
	 int64_t skew;
#endif

	 sample_to_be_sent_aud = sample_to_be_sent_vid = 0;
	 //st = *ctx;// by suma
#ifdef PERF_DEBUG1
	 gettimeofday(&selfTimeStart, NULL);
#endif
	 switch(trak_type) {
	     case VIDEO_HINT_TRAK:
		 if((seekinfo->HintV.num_samples_sent+SAMPLES_TO_BE_SENT<=st->hvid_s->total_sample_size)||(seekinfo->HintV.num_samples_sent==0)) {
		     ret=find_chunk_asset_opt(seekinfo->HintV.num_bytes,seekinfo->HintV.offset,SAMPLES_TO_BE_SENT,seekinfo->HintV.num_samples_sent,&media_stbl->HintVidStbl,st-> hvid_s,nkn_fread,fid);
		     ret=find_chunk_asset_opt(seekinfo->Video.num_bytes,seekinfo->Video.offset,SAMPLES_TO_BE_SENT,seekinfo->Video.num_samples_sent,&media_stbl->VideoStbl,st->vid_s,nkn_fread,fid);
		     sample_to_be_sent_vid = SAMPLES_TO_BE_SENT;
		 } else {
		     if(st->hvid_s->total_sample_size - seekinfo->HintV.num_samples_sent<SAMPLES_TO_BE_SENT&&(st->hvid_s->total_sample_size - seekinfo->HintV.num_samples_sent > 0)){
			 sample_to_be_sent_vid = st->hvid_s->total_sample_size-seekinfo->HintV.num_samples_sent;
			 ret=find_chunk_asset_opt(seekinfo->HintV.num_bytes,seekinfo->HintV.offset,sample_to_be_sent_vid,seekinfo->HintV.num_samples_sent,&media_stbl->HintVidStbl,st->hvid_s,nkn_fread,fid);
			 ret=find_chunk_asset_opt(seekinfo->Video.num_bytes,seekinfo->Video.offset,sample_to_be_sent_vid,seekinfo->Video.num_samples_sent,&media_stbl->VideoStbl,st->vid_s,nkn_fread,fid);

		     } else if(st->hvid_s->total_sample_size == seekinfo->HintV.num_samples_sent) {
			 rv = NOMORE_SAMPLES;
			 goto cleanup;
		     }
		     printf("\n\nSamples Sent = %d\n ",seekinfo->HintV.num_samples_sent);
		 }
#ifdef PERF_DEBUG1
		 {
		     struct timeval result1;
		     gettimeofday(&selfTimeEnd,NULL);
		     timeval_subtract1(&selfTimeEnd, &selfTimeStart, &result1);
		     //printf("TRAK TYPE %d, Sample Num %d, Time Elapsed %lu <---------> Expected Time %d ===== next DTS is  %d\n", psmss->trackType, psmss->sampleNumber, psmss->timeElapsed, psmss->lastDTS, psmss->nextDTS);
		 printf("TRAK TYPE %d, Self Time %ld\n", trak_type, (int64_t)((result1.tv_sec*1e6)+result1.tv_usec));
	 }
#endif
	 for(i=0;i<sample_to_be_sent_vid;i++) {
	     dta =(uint8_t*) malloc(seekinfo->HintV.num_bytes[i]);
	     nkn_vfs_fseek(fid,seekinfo->HintV.offset[i],SEEK_SET);
	     (*nkn_fread)(dta,seekinfo->HintV.num_bytes[i],1,fid);

	     read_rtp_sample_interface(dta,&hint_video[i],seekinfo->HintV.offset[i], seekinfo->Video.offset[i]);
	     //          (hint_video+i)->rtp->header.relative_time+= get_time_offset(media_stbl->HintVidStbl.stts,seekinfo->HintV.num_samples_sent+i);
	     for(j=0;j<(hint_video+i)->num_rtp_pkts;j++) {
		 rtp_header = (hint_video+i)->rtp;
		 rtp_header+=j;
		 rtp_header->header.relative_time+= get_time_offset_opt(&media_stbl->HintVidStbl,seekinfo->HintV.num_samples_sent+i,nkn_fread,fid) + rtp_header->time_stamp;
	     }
	     if(dta != NULL){
		 free(dta);
		 dta = NULL;
	     }
	 }
	 seekinfo->HintV.num_samples_sent+=sample_to_be_sent_vid;
	 seekinfo->Video.num_samples_sent+=sample_to_be_sent_vid;

	 break;
	 case AUDIO_HINT_TRAK:

	     {
		 int uu = 0;
		 if(seekinfo->HintA.num_samples_sent == 4030) {
		     uu = 1;
		 }
	     }

	     if((seekinfo->HintA.num_samples_sent+SAMPLES_TO_BE_SENT<=st->haud_s->total_sample_size)||(seekinfo->HintA.num_samples_sent==0)) {
		 ret=find_chunk_asset_opt(seekinfo->HintA.num_bytes,seekinfo->HintA.offset,SAMPLES_TO_BE_SENT,seekinfo->HintA.num_samples_sent,&media_stbl->HintAudStbl,st->haud_s,nkn_fread,fid);
                 ret=find_chunk_asset_opt(seekinfo->Audio.num_bytes,seekinfo->Audio.offset,SAMPLES_TO_BE_SENT,seekinfo->Audio.num_samples_sent,&media_stbl->AudioStbl,st->aud_s,nkn_fread,fid);

		 sample_to_be_sent_aud = SAMPLES_TO_BE_SENT;
	     } else {
		 if(st->haud_s->total_sample_size - seekinfo->HintA.num_samples_sent<SAMPLES_TO_BE_SENT&&(st->haud_s->total_sample_size - seekinfo->HintA.num_samples_sent > 0)){
		     sample_to_be_sent_aud = st->haud_s->total_sample_size-seekinfo->HintA.num_samples_sent;
		     ret=find_chunk_asset_opt(seekinfo->HintA.num_bytes,seekinfo->HintA.offset,sample_to_be_sent_aud,seekinfo->HintA.num_samples_sent,&media_stbl->HintAudStbl,st->haud_s,nkn_fread,fid);
                     ret=find_chunk_asset_opt(seekinfo->Audio.num_bytes,seekinfo->Audio.offset,sample_to_be_sent_aud,seekinfo->Audio.num_samples_sent,&media_stbl->AudioStbl,st->aud_s,nkn_fread,fid);

		 } else if(st->haud_s->total_sample_size == seekinfo->HintA.num_samples_sent) {
		     rv =  NOMORE_SAMPLES;
		     goto cleanup;
		 }
	     }
#ifdef PERF_DEBUG1
	     {
		 struct timeval result1;
		 gettimeofday(&selfTimeEnd,NULL);
		 timeval_subtract1(&selfTimeEnd, &selfTimeStart, &result1);
		 //printf("TRAK TYPE %d, Sample Num %d, Time Elapsed %lu <---------> Expected Time %d ===== next DTS is  %d\n", psmss->trackType, psmss->sampleNumber, psmss->timeElapsed, psmss->lastDTS, psmss->nextDTS);
	     printf("TRAK TYPE %d, Self Time %ld\n", trak_type, (int64_t)((result1.tv_sec*1e6)+result1.tv_usec));
     }
#endif
     for(i=0;i<sample_to_be_sent_aud;i++) {
	 dta =(uint8_t*) malloc(seekinfo->HintA.num_bytes[i]);
	 nkn_vfs_fseek(fid,seekinfo->HintA.offset[i],SEEK_SET);
	 (*nkn_fread)(dta,seekinfo->HintA.num_bytes[i],1,fid);
	 //              read_rtp_sample_audio(dta,&hint_audio[i],seekinfo->HintA.offset[i], seekinfo->Audio.offset[i], &media_stbl->HintAudStbl, &media_stbl->AudioStbl);
     read_rtp_sample_audio_interface(dta,&hint_audio[i],seekinfo->HintA.offset[i], seekinfo->Audio.offset[i], &media_stbl->HintAudStbl, &media_stbl->AudioStbl,&st->haud_s->prev_sample_details,&st->aud_s->prev_sample_details,nkn_fread,fid);
     for(j=0;j<(hint_audio+i)->num_rtp_pkts;j++) {
	 //int32_t time_temp;
	 rtp_header = (hint_audio+i)->rtp;
	 rtp_header+=j;
	 rtp_header->header.relative_time+= get_time_offset_opt(&media_stbl->HintAudStbl,seekinfo->HintA.num_samples_sent+i,nkn_fread,fid) + rtp_header->time_stamp;
     }
     if(dta != NULL){
	 free(dta);
	 dta = NULL;
     }
 }
 seekinfo->HintA.num_samples_sent+=sample_to_be_sent_aud;
 seekinfo->Audio.num_samples_sent+=sample_to_be_sent_aud;
 break;
    case VIDEO_TRAK:
	if((seekinfo->Video.num_samples_sent+SAMPLES_TO_BE_SENT<=st->vid_s->total_sample_size)||(seekinfo->Video.num_samples_sent==0)) {
	    ret=find_chunk_asset_opt(seekinfo->Video.num_bytes,seekinfo->Video.offset,SAMPLES_TO_BE_SENT,seekinfo->Video.num_samples_sent,&media_stbl->VideoStbl,st->vid_s,nkn_fread,fid);
	    sample_to_be_sent_vid = SAMPLES_TO_BE_SENT;
	} else {
	    if(st->vid_s->total_sample_size - seekinfo->Video.num_samples_sent<SAMPLES_TO_BE_SENT&&(st->vid_s->total_sample_size - seekinfo->Video.num_samples_sent >0)){
		// sample_to_be_sent_vid = st->hvid_s->total_sample_size-seekinfo->HintV.num_samples_sent;//by suma
		ret=find_chunk_asset_opt(seekinfo->Video.num_bytes,seekinfo->Video.offset,sample_to_be_sent_vid,seekinfo->Video.num_samples_sent,&media_stbl->VideoStbl,st->vid_s,nkn_fread,fid);
	    } else if(st->vid_s->total_sample_size == seekinfo->Video.num_samples_sent) {
		rv = NOMORE_SAMPLES;
		goto cleanup;
	    }
	}
#ifdef PERF_DEBUG1
	{
	    struct timeval result1;
	    gettimeofday(&selfTimeEnd,NULL);
	    timeval_subtract1(&selfTimeEnd, &selfTimeStart, &result1);
	    //printf("TRAK TYPE %d, Sample Num %d, Time Elapsed %lu <---------> Expected Time %d ===== next DTS is  %d\n", psmss->trackType, psmss->sampleNumber, psmss->timeElapsed, psmss->lastDTS, psmss->nextDTS);
	printf("TRAK TYPE %d, Self Time %ld\n", trak_type, (int64_t)((result1.tv_sec*1e6)+result1.tv_usec));
}
#endif
 seekinfo->Video.num_samples_sent+=sample_to_be_sent_vid;

 break;
    case AUDIO_TRAK:

	{
	    int uu = 0;
	    if(seekinfo->HintA.num_samples_sent == 4030) {
		uu = 1;
	    }
	}

	if((seekinfo->Audio.num_samples_sent+SAMPLES_TO_BE_SENT<=st->aud_s->total_sample_size)||(seekinfo->Audio.num_samples_sent==0)) {
	    ret=find_chunk_asset_opt(seekinfo->Audio.num_bytes,seekinfo->Audio.offset,SAMPLES_TO_BE_SENT,seekinfo->Audio.num_samples_sent,&media_stbl->AudioStbl,st->aud_s,nkn_fread,fid);
	    sample_to_be_sent_aud = SAMPLES_TO_BE_SENT;
	} else {
	    if(st->aud_s->total_sample_size - seekinfo->Audio.num_samples_sent<SAMPLES_TO_BE_SENT&&(st->aud_s->total_sample_size - seekinfo->Audio.num_samples_sent >0)){
		sample_to_be_sent_aud = st->aud_s->total_sample_size-seekinfo->Audio.num_samples_sent;
		ret=find_chunk_asset_opt(seekinfo->Audio.num_bytes,seekinfo->Audio.offset,sample_to_be_sent_aud,seekinfo->Audio.num_samples_sent,&media_stbl->AudioStbl,st->aud_s,nkn_fread,fid);
	    } else if(st->aud_s->total_sample_size == seekinfo->Audio.num_samples_sent) {
		rv =  NOMORE_SAMPLES;
		goto cleanup;
	    }
	}
#ifdef PERF_DEBUG1
	{
	    struct timeval result1;
	    gettimeofday(&selfTimeEnd,NULL);
	    timeval_subtract1(&selfTimeEnd, &selfTimeStart, &result1);
	    //printf("TRAK TYPE %d, Sample Num %d, Time Elapsed %lu <---------> Expected Time %d ===== next DTS is  %d\n", psmss->trackType, psmss->sampleNumber, psmss->timeElapsed, psmss->lastDTS, psmss->nextDTS);
	printf("TRAK TYPE %d, Self Time %ld\n", trak_type, (int64_t)((result1.tv_sec*1e6)+result1.tv_usec));
}
#endif

seekinfo->Audio.num_samples_sent+=sample_to_be_sent_aud;
break;

/*CHANGES CHANGES HERE*/
}

switch(trak_type){
    case AUDIO_HINT_TRAK:
	st->haud_s->look_ahead_time = get_time_offset_opt(&media_stbl->HintAudStbl,seekinfo->HintA.num_samples_sent+1,nkn_fread,fid);
	break;
    case VIDEO_HINT_TRAK:

	st->hvid_s->look_ahead_time = get_time_offset_opt(&media_stbl->HintVidStbl,seekinfo->HintV.num_samples_sent+1,nkn_fread,fid);
	break;
    case AUDIO_TRAK:
	st->aud_s->look_ahead_time = get_time_offset_opt(&media_stbl->AudioStbl,seekinfo->Audio.num_samples_sent+1,nkn_fread,fid);
	if (media_stbl->AudioStbl.ctts_tab.size != -1)
            {
                st->aud_s->ctts_offset  = get_ctts_offset_opt(&media_stbl->AudioStbl,seekinfo->Audio.num_samples_sent,nkn_fread,fid);
            }
	break;
    case VIDEO_TRAK:
	st->vid_s->look_ahead_time = get_time_offset_opt(&media_stbl->VideoStbl,seekinfo->Video.num_samples_sent+1,nkn_fread,fid);
	if (media_stbl->VideoStbl.ctts_tab.size != -1){
                st->vid_s->ctts_offset  = get_ctts_offset_opt(&media_stbl->VideoStbl,seekinfo->Video.num_samples_sent,nkn_fread,fid);
	}
	break;
 }

goto cleanup;

cleanup:
return rv;
 }

 int32_t get_total_sample_size_opt(Stbl_t *stbl,size_t (*nkn_fread)( void *,size_t, size_t, FILE*),FILE *fid)
 {
     int32_t tsize;
     nkn_vfs_fseek(fid,(stbl->stsz_tab.offset)+16,SEEK_SET);
     (*nkn_fread)(&tsize,4,1,fid);
     tsize = uint32_byte_swap(tsize);
     return tsize;
 }


 int32_t find_nearest_sync_sample_opt(Stbl_t *stbl,int32_t sample_num,size_t (*nkn_fread)( void *,size_t, size_t, FILE*),FILE *fid)
{
    int32_t i,entry_count,smpl_no=1, prev_smpl_no=1,new_pos;
    // entry_count = _read32(data,data_pos);
    //data_pos+=4;
    nkn_vfs_fseek(fid,(stbl->stss_tab.offset)+12,SEEK_SET);
    (*nkn_fread)(&entry_count,4,1,fid);
    entry_count = uint32_byte_swap(entry_count);
    for(i=0;i<entry_count;i++){
	new_pos=read_stss_opt(i,stbl->stss_tab.offset+16);
	nkn_vfs_fseek(fid,new_pos,SEEK_SET);
	(*nkn_fread)(&smpl_no,4,1,fid);
	smpl_no = uint32_byte_swap(smpl_no);    
        if(sample_num<=smpl_no)
            break;
	prev_smpl_no=smpl_no;
    }
    return prev_smpl_no;
}

 int32_t find_chunk_offset_opt(Stbl_t *stbl,int32_t chunk_num,size_t (*nkn_fread)( void *,size_t, size_t, FILE*),FILE *fid){
     //data->stco. Chunk numbering starts from 1.
     int32_t offset,new_pos;
     new_pos=read_stco_opt((chunk_num -1),stbl->stco_tab.offset+16);
     nkn_vfs_fseek(fid,new_pos,SEEK_SET);
     (*nkn_fread)(&offset,4,1,fid);
     offset = uint32_byte_swap(offset);
     return offset;
 }

int32_t seek_state_change_opt(Stbl_t *stbl,int32_t seek_time,int32_t time_scale,Sampl_prop_t* dtls,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE *fid)
{
    int32_t sample_num,chunk_num,first_sample_of_chunk,offset,base_offset;
    //int32_t *zero_ptrs;
    //Get the sample number corresponding to the time stamp.
    sample_num = read_stts_opt_main(stbl,seek_time,time_scale,nkn_fread,fid);
    //Get the closest sync sample number
    //if(stbl->stss_tab.size!= -1)
    //sample_num = find_nearest_sync_sample_opt(stbl,sample_num,nkn_fread,fid);
    //Get the chunk number corresponding to this sample number

    chunk_num=get_chunk_offsets_opt(sample_num-1,stbl,dtls->total_chunks, &dtls->prev_sample_details.chunk_count, &dtls->prev_sample_details.prev_sample_total,nkn_fread,fid);

    //Get the sample number of the first sample in the chunk
    first_sample_of_chunk=find_first_sample_of_chunk_opt(stbl,chunk_num,nkn_fread,fid)+1;
    //Calculate the offset of the offset of this sample in file.
    base_offset = find_chunk_offset_opt(stbl,chunk_num,nkn_fread,fid);
    //Find cumulative offset in chunk to present sample in chunk
    // offset= base_offset + find_cumulative_sample_size(stbl->stsz,first_sample_of_chunk,sample_num);
    offset= base_offset + find_cumulative_sample_size_opt(stbl,first_sample_of_chunk,sample_num,nkn_fread,fid);
    dtls->prev_base = base_offset;
    dtls->size_offset = offset - base_offset;
    //NK:
    dtls->prev_sample_details.is_stateless = 1;
    return sample_num-1;
}

int32_t media_seek_state_change_opt(Stbl_t *stbl,Sampl_prop_t* dtls,int32_t sample_num,size_t (*nkn_fread)( void *,size_t, size_t, FILE*),FILE *fid)
 {

     int32_t chunk_num,first_sample_of_chunk,offset,base_offset;
     //Get the chunk number corresponding to this sample number

     chunk_num=get_chunk_offsets_opt(sample_num,stbl,dtls->total_chunks,&dtls->prev_sample_details.chunk_count,&dtls->prev_sample_details.prev_sample_total,nkn_fread,fid);

     //Get the sample number of the first sample in the chunk
     first_sample_of_chunk=find_first_sample_of_chunk_opt(stbl,chunk_num,nkn_fread,fid);
     //Calculate the offset of the offset of this sample in file.
     base_offset = find_chunk_offset_opt(stbl,chunk_num,nkn_fread,fid);
     //Find cumulative offset in chunk to present sample in chunk
     offset= base_offset + find_cumulative_sample_size_opt(stbl,first_sample_of_chunk,sample_num,nkn_fread,fid);
     dtls->prev_base = base_offset;
     dtls->size_offset = offset - base_offset;
     //NK:
     dtls->prev_sample_details.is_stateless = 1;

     return 1;
 }


int32_t find_first_sample_of_chunk_opt(Stbl_t *stbl,int32_t chunk_num,size_t (*nkn_fread)( void *,size_t, size_t, FILE*),FILE *fid)
 {
     //data->stsc
     int32_t sample_num=0,i,first_chunk,entry_count,samples_per_chunk,next_chnk_num,prev_samples_per_chunk,new_pos;
     // data_pos = 4+4+4;//account for exteneded box headers
     // entry_count = _read32(data,data_pos);
     // data_pos+=4;
     nkn_vfs_fseek(fid,(stbl->stsc_tab.offset)+12,SEEK_SET);
     (*nkn_fread)(&entry_count,4,1,fid);
     entry_count = uint32_byte_swap(entry_count);


     for(i=0;i<entry_count;i++){
	 //first_chunk=_read32(data,data_pos);
	 //data_pos+=4;
	 new_pos=read_stsc_opt(i,stbl->stsc_tab.offset+16);
         nkn_vfs_fseek(fid,new_pos,SEEK_SET);
         (*nkn_fread)(&first_chunk,4,1,fid);
         first_chunk = uint32_byte_swap(first_chunk);

	 // samples_per_chunk = _read32(data,data_pos);
	 nkn_vfs_fseek(fid,new_pos+4,SEEK_SET);
         (*nkn_fread)(&samples_per_chunk,4,1,fid);
         samples_per_chunk = uint32_byte_swap(samples_per_chunk);

	 if(i<entry_count-1)
	     // next_chnk_num = _read32(data,data_pos+8);
	     nkn_vfs_fseek(fid,new_pos+12,SEEK_SET);
	    (*nkn_fread)(&next_chnk_num,4,1,fid);
	     next_chnk_num = uint32_byte_swap(next_chnk_num);
	 if(chunk_num<=first_chunk){
	     /* changed from sample_num+=samples_per_chunk*(chunk_num-first_chunk); because if the run of chunks overruns
	      * the sample that we are searching for then we need to know the samples in the previous run of chunks; added
	      * a variable to this effect and fixed this bug
	      */
		if(chunk_num==1)
                    sample_num=1;
                else
	     sample_num+=prev_samples_per_chunk*(chunk_num-first_chunk);
	     break;
	 }
	 sample_num+=samples_per_chunk*(next_chnk_num-first_chunk);
	 prev_samples_per_chunk = samples_per_chunk;
	 // data_pos+=4+4;
     }
     return sample_num;
 }


int32_t find_cumulative_sample_size_opt(Stbl_t *stbl,int32_t first_sample,int32_t sample_num,size_t (*nkn_fread)( void *,size_t, size_t, FILE*),FILE *fid)
{
    int32_t total_size = 0,i,sample_size,new_pos,temp;

    nkn_vfs_fseek(fid,(stbl->stsz_tab.offset)+12,SEEK_SET);
    (*nkn_fread)(&sample_size,4,1,fid);
    sample_size = uint32_byte_swap(sample_size);


    // data_pos +=12;
    // sample_size = _read32(data,data_pos);
    // data_pos+=4;
    if(sample_size!=0)
        total_size= sample_size*(sample_num-first_sample);
    else{
	//	new_pos=read_stsz_opt((first_sample-1),stbl->stsz_tab.offset+20);
	//nkn_vfs_fseek(fid,new_pos,SEEK_SET);
	//can optimize further in for loop by suma
        for(i=first_sample;i<sample_num;i++){
	    new_pos=read_stsz_opt((i-1),stbl->stsz_tab.offset+20);
	    nkn_vfs_fseek(fid,new_pos,SEEK_SET);
	    (*nkn_fread)(&temp,4,1,fid);
	    temp = uint32_byte_swap(temp);
	    total_size+=temp;
        }
    }

    return total_size;
}

/* read the SDP information from the MP4 file
 *@param SDP [in] - will contain the SDP data as null terminated text
 *@param nkn_fread [in] - read function
 *@param fid [in] - file decriptor
 */
void get_sdp_info(SDP_info_t* SDP,size_t (*nkn_fread)( void *, size_t, size_t, FILE*), FILE *fid){
    uint8_t* data,*p_data;
    box_t box;
    int64_t fpos;
    int32_t box_size,sub_moov_box_size,moov_not_parsed=1,word;
    size_t size,bytes_read=0;
    char moov_id[] = {'m', 'o', 'o', 'v'};
    char trak_id[] = {'t', 'r', 'a', 'k'};
    char udta_id[] = {'u', 'd', 't', 'a'};
    char hnti_id[] = {'h', 'n', 't', 'i'};
    char rtp_id[] = {'r','t','p',' '};
    char sdp_id[] = {'s','d','p',' '};
    SDP->num_bytes=0;
    SDP->sdp=NULL;
    
    box_size=0;
    data=(uint8_t*)malloc(8*sizeof(uint8_t));
    (*nkn_fread)(data,1,8,fid);
    word=_read32(data,4);
    while(word!=MOOV_HEX_ID){
	nkn_vfs_fseek(fid,box_size,SEEK_SET);
	(*nkn_fread)(data,8,1,fid);
	box_size=_read32(data,0);
	word=_read32(data,4);
    }
    //in moov position
    fpos=nkn_vfs_ftell(fid);
    fpos-=8;
    nkn_vfs_fseek(fid,fpos,SEEK_SET);
    if(data != NULL)
	free(data);
    data=(uint8_t*)malloc(box_size*sizeof(uint8_t));
    (*nkn_fread)(data,box_size,1,fid);
    size=box_size;
    p_data=data;

    while(moov_not_parsed){
	box_size = read_next_root_level_box(p_data, size, &box, &bytes_read);
	if(check_box_type(&box,moov_id)){
	    // uint8_t in_moov=1;
	    int32_t moov_box_size;
	    uint8_t* p_moov;
	    p_moov=p_data;
	     moov_box_size=box_size;
	    //Search for udta box here
	    p_data+=bytes_read;
	    while((p_data-p_moov)<moov_box_size){
		sub_moov_box_size = read_next_root_level_box(p_data, size, &box, &bytes_read);
		box_size=sub_moov_box_size;
		if(check_box_type(&box, trak_id)){
		    int32_t trak_size, bytes_left;
		    uint8_t* p_trak;
		    p_trak=p_data;
		    trak_size=box_size;
		    bytes_left=trak_size-8;
		    p_trak+=bytes_read;
		    while(bytes_left>0){
			box_size = read_next_root_level_box(p_trak, size, &box, &bytes_read);
			if(check_box_type(&box, udta_id)){
			    uint8_t* p_udta;
			    int32_t child_udta_size,udta_size;
			    udta_size=box_size;
			    p_udta=p_trak+bytes_read;
			    udta_size-=bytes_read;
			    while(udta_size>0){
				child_udta_size=read_next_root_level_box(p_udta, size, &box, &bytes_read);
				if(check_box_type(&box, hnti_id)){
				    int32_t non_first_hint_info=0,hnti_size;
				    if(SDP->num_bytes!=0){
					//Add the required bytes to the same. Copy the SDP buffer.
					uint8_t *temp_ptr;
                                        non_first_hint_info=1;
					hnti_size=child_udta_size-8-8;
					temp_ptr=(uint8_t*)malloc(SDP->num_bytes+1);
					memcpy(temp_ptr,SDP->sdp,SDP->num_bytes);
					if(SDP->sdp != NULL)
					    free(SDP->sdp);
					SDP->sdp=(uint8_t*)malloc(SDP->num_bytes+hnti_size+1);
					memcpy(SDP->sdp,temp_ptr,SDP->num_bytes);
					if(temp_ptr != NULL)
					    free(temp_ptr);
				    }


				   p_udta+=bytes_read;
				   child_udta_size=read_next_root_level_box(p_udta, size, &box, &bytes_read);
				   if(check_box_type(&box, sdp_id)){
				       if(non_first_hint_info){
					   uint8_t* temp_data;
					   temp_data=SDP->sdp+SDP->num_bytes;
					   SDP->num_bytes+=child_udta_size-(4+4);
                                           p_udta+=4+4;
                                           memcpy(temp_data,p_udta,hnti_size);
				       }
				       else{
					   SDP->num_bytes+=child_udta_size-(4+4);
					   SDP->sdp=(uint8_t*)malloc(SDP->num_bytes+1);
					   p_udta+=4+4;
					   memcpy(SDP->sdp,p_udta,SDP->num_bytes);
				       }
				       udta_size=0;
				       bytes_left=0;
				   }					
				}
				else{
				    udta_size-=child_udta_size;
				    p_udta+=child_udta_size;				    
				}
				    
			    }
			    
			}
			else{
			    bytes_left-=box_size;
			    p_trak+=box_size;
			}
			    
		    }//while(bytes_left>0)
		}

		if(check_box_type(&box, udta_id)){
		    uint8_t* p_udta;
		    int32_t child_udta_size,udta_size;
		    udta_size=box_size;
		    p_udta=p_data+bytes_read;
		    udta_size-=bytes_read;
		    while(udta_size>0){
			child_udta_size=read_next_root_level_box(p_udta, size, &box, &bytes_read);
			if(check_box_type(&box, hnti_id)){
			    p_udta+=bytes_read;
			    child_udta_size=read_next_root_level_box(p_udta, size, &box, &bytes_read);
			    if(check_box_type(&box, rtp_id)){
				int32_t rtp_hnti_size;
				uint8_t* sdp_ptr;
				rtp_hnti_size=child_udta_size-12;
				if(SDP->num_bytes!=0){
				    //Add the required bytes to the same. Copy the SDP buffer.
				    uint8_t *temp_ptr;
				    temp_ptr=(uint8_t*)malloc(SDP->num_bytes+1);
				    memcpy(temp_ptr,SDP->sdp,SDP->num_bytes);
				    if(SDP->sdp != NULL)
					free(SDP->sdp);				    
				    SDP->sdp=(uint8_t*)malloc(SDP->num_bytes+rtp_hnti_size+1);
				    memcpy(SDP->sdp,temp_ptr,SDP->num_bytes);
				    if(temp_ptr != NULL)
					free(temp_ptr);
				}
				sdp_ptr=SDP->sdp+SDP->num_bytes;
				p_udta+=12;
				memcpy(sdp_ptr,p_udta,rtp_hnti_size);
				SDP->num_bytes+=rtp_hnti_size;
				udta_size=0;
			    }
			}
			else{
			    udta_size-=child_udta_size;
			    p_udta+=child_udta_size;
			}

		    }

		}
		p_data+=sub_moov_box_size;
		if((p_data-p_moov)>=moov_box_size)
		    moov_not_parsed=0;
	    }// while(in_moov)
	}
    }
    if(data != NULL)
	free(data);
}

 int32_t read_stsz_opt(int32_t sample_num,int32_t pos)
 {
     pos+= 4*sample_num;
     return pos;

 }


int32_t read_stco_opt(int32_t chunk_num,int32_t pos)
 {
     pos+= 4*(chunk_num);
     return pos;

 }


int32_t read_stss_opt(int32_t entry_num,int32_t pos)
{
    pos+= 4*entry_num;
    return pos;

}


int32_t read_stts_opt(int32_t entry_num,int32_t pos)
 {
     pos+= 8*entry_num;
     return pos;

 }
int32_t read_stsc_opt(int32_t entry_num,int32_t pos)
 {
     pos+= 12*entry_num;
     return pos;
 }
int32_t read_ctts_opt(int32_t entry_num,int32_t pos)
 {
     pos+= 8*entry_num;
     return pos;

 }

int32_t uint32_byte_swap(int32_t input){
    return (((input&0x000000FF)<<24)+((input&0x0000FF00)<<8)+((input&0x00FF0000)>>8)+((input&0xFF000000)>>24));
}


/***********************************************************************
 *
 *                    UTILITY FUNCTIONS
 *
 ***********************************************************************/
/* returns the offset in the MP4 file for a given sample in a track
 *@param sample_size[in] - size of a sample
 *@param offsets[out] - offset in the file
 *@param num_samples_to_send[in] - number of samples to send
 *@param sample_number[in] - sample number whose offset is needed
 *@param stbl [in] - sample table
 *@param prop [in] - sample description
 */
int32_t find_chunk_asset_opt(int32_t* sample_size,int32_t* offsets,int32_t num_samples_to_send,int32_t sample_num,Stbl_t *stbl,Sampl_prop_t* prop,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE *fid){
     int32_t temp_sam_size,samp_cnt,i,new_pos;
     int32_t *chunk_num;
     //uint8_t* data;
     //sample_sizes=(int32_t*)malloc(num_samples_to_send*sizeof(int32_t));
     chunk_num=(int32_t*)malloc(num_samples_to_send*sizeof(int32_t));
     //GET SIZES OF SAMPLES:
     // int32_t new_pos;
     nkn_vfs_fseek(fid,(stbl->stsz_tab.offset)+12,SEEK_SET);
     (*nkn_fread)(&temp_sam_size,4,1,fid);
     temp_sam_size = uint32_byte_swap(temp_sam_size);

     nkn_vfs_fseek(fid,(stbl->stsz_tab.offset)+16,SEEK_SET);
     (*nkn_fread)(&samp_cnt,1,sizeof(int32_t),fid);
     samp_cnt = uint32_byte_swap(samp_cnt);
     prop->total_sample_size=samp_cnt;

     if(temp_sam_size==0)
         {
	     new_pos=read_stsz_opt(sample_num,stbl->stsz_tab.offset+20);
             for(i=0;i<num_samples_to_send;i++)
                 {
                     //new_pos=read_stsz_opt(sample_num,stbl->stsz_tab.offset+20);
                     nkn_vfs_fseek(fid,new_pos,SEEK_SET);
                     (*nkn_fread)(&sample_size[i],4,1,fid);
                     sample_size[i] = uint32_byte_swap(sample_size[i]);
		     new_pos+=4;
                 }
         }
     else
         {
             for(i=0;i<num_samples_to_send;i++)
                 {
                     sample_size[i] = temp_sam_size;
                 }
         }

     nkn_vfs_fseek(fid,(stbl->stco_tab.offset)+12,SEEK_SET);
     (*nkn_fread)(&prop->total_chunks,4,1,fid);
     prop->total_chunks = uint32_byte_swap(prop->total_chunks);

     prop->prev_sample_details.total_chunks= prop->total_chunks;

     //Get chunk in which sample is present.
     //data=stbl->stsc;
     for(i=0;i<num_samples_to_send;i++)

         chunk_num[i]=get_chunk_offsets_opt(sample_num+i,stbl,prop->total_chunks,&prop->prev_sample_details.chunk_count,&prop->prev_sample_details.prev_sample_total,nkn_fread,fid);

     //Get chunk offsets as file offsets:
     //data=stbl->stco;
     //data+=12;
     //for(i=0;i<num_samples_to_send;i++){
     // samp_cnt=chunk_num[i]*4;
     // offsets[i]=_read32(data,samp_cnt);
     //}


     for(i=0;i<num_samples_to_send;i++){
	 new_pos=read_stco_opt(chunk_num[i],stbl->stco_tab.offset+12);
	 nkn_vfs_fseek(fid,new_pos,SEEK_SET);
	 (*nkn_fread)(&offsets[i],4,1,fid);
	 offsets[i] = uint32_byte_swap(offsets[i]);
     }
     for(i=0;i<num_samples_to_send;i++){
         if((prop->prev_base==0) || (prop->prev_base==offsets[i])){
             offsets[i]+=prop->size_offset;
             prop->size_offset+=sample_size[i];
             if((prop->prev_base==0))
                 prop->prev_base=offsets[i];
         }
         else{
             prop->size_offset=sample_size[i];
             prop->prev_base=offsets[i];
         }

     }

     prop->prev_sample_details.sample_num = sample_num;
     if(chunk_num != NULL)
	 free(chunk_num);
     return 1;
 }

/* computes the DTS for a sample number
 */
int32_t get_ctts_offset_opt(Stbl_t *stbl, int32_t sample_num,size_t (*nkn_fread)( void *,size_t, size_t, FILE*),FILE *fid)
 {
     int32_t i,entry_count,new_pos,sample_offset,temp,prev_smp_cnt = 0,sample_cnt,offset;
     nkn_vfs_fseek(fid,(stbl->ctts_tab.offset)+12,SEEK_SET);
     (*nkn_fread)(&entry_count,4,1,fid);
     entry_count = uint32_byte_swap(entry_count);
     for(i=0;i<entry_count;i++){
	 new_pos=read_ctts_opt(i,stbl->ctts_tab.offset+16);
         nkn_vfs_fseek(fid,new_pos,SEEK_SET);
         (*nkn_fread)(&sample_cnt,4,1,fid);
         sample_cnt = uint32_byte_swap(sample_cnt);

         nkn_vfs_fseek(fid,new_pos+4,SEEK_SET);
         (*nkn_fread)(&sample_offset,4,1,fid);
         sample_offset = uint32_byte_swap(sample_offset);
	 temp = prev_smp_cnt +sample_cnt;
         if(sample_num<=temp){
             offset = sample_offset;
             break;
         }
	 prev_smp_cnt = temp;
     }
    return offset;
}



int32_t get_time_offset_opt(Stbl_t *stbl, int32_t sample_num,size_t (*nkn_fread)( void *,size_t, size_t, FILE*),FILE *fid){
     int32_t time_tt = 0,time_base=0,entry_count,i,delta, sample_cnt=0,prev_sample_num=0,new_pos;
     // data+=8+4;
     //entry_count = _read32(data,0);
     //data+=4;

     nkn_vfs_fseek(fid,(stbl->stts_tab.offset)+12,SEEK_SET);
     (*nkn_fread)(&entry_count,4,1,fid);
     entry_count = uint32_byte_swap(entry_count);


     for(i=0;i<entry_count;i++){
	 new_pos=read_stts_opt(i,stbl->stts_tab.offset+16);
	 nkn_vfs_fseek(fid,new_pos,SEEK_SET);
	 (*nkn_fread)(&sample_cnt,4,1,fid);
	 sample_cnt = uint32_byte_swap(sample_cnt);

	 nkn_vfs_fseek(fid,new_pos+4,SEEK_SET);
         (*nkn_fread)(&delta,4,1,fid);
         delta = uint32_byte_swap(delta);

	 if(sample_num<sample_cnt){
	     time_tt = time_base + delta*(sample_num-prev_sample_num);
	     break;
	 }

	 time_base+= delta*(sample_cnt-prev_sample_num);
	 prev_sample_num = sample_cnt;
	 // data+=8;
    }
    return time_tt;
}


/* translates the sample number to a chunk and chunk offset
 */



int32_t get_chunk_offsets_opt(int32_t sample_number, Stbl_t *stbl, int32_t last_chunk_num, int32_t* prev_sample_num, int32_t* prev_total,size_t (*nkn_fread)( void *,size_t, size_t, FILE*),FILE *fid)
 {
     int32_t entry_count,i,total_sample_num,starting_sample_num,next_chunk,num_chunks,num_sample_chnk,chunk_num,first_chunk,total_prev,new_pos;
     //data+=12;
     //entry_count=_read32(data,0);
     //data+=4;

     nkn_vfs_fseek(fid,(stbl->stsc_tab.offset)+12,SEEK_SET);
     (*nkn_fread)(&entry_count,4,1,fid);
     entry_count = uint32_byte_swap(entry_count);
     total_sample_num=*prev_total;
     chunk_num = 0;

     // data+=12*(*prev_sample_num);
     //new_pos=read_stsc_opt(*prev_sample_num,stbl->stsc_tab.offset+16);
     // nkn_vfs_fseek(fid,new_pos,SEEK_SET);
     for(i=*prev_sample_num;i<entry_count;i++){
         total_prev = total_sample_num;
         starting_sample_num=total_sample_num;
         // first_chunk=_read32(data,0);

         new_pos=read_stsc_opt(i,stbl->stsc_tab.offset+16);
         nkn_vfs_fseek(fid,new_pos,SEEK_SET);
         (*nkn_fread)(&first_chunk,4,1,fid);
         first_chunk = uint32_byte_swap(first_chunk);

         if(i!=entry_count-1){
             // next_chunk = _read32(data,12);
             nkn_vfs_fseek(fid,new_pos+12,SEEK_SET);
             (*nkn_fread)(&next_chunk,4,1,fid);
             next_chunk = uint32_byte_swap(next_chunk);
             num_chunks=next_chunk-first_chunk;
         }
	 else
             num_chunks=last_chunk_num-first_chunk+1;
         // num_sample_chnk = _read32(data,4);
         nkn_vfs_fseek(fid,new_pos+4,SEEK_SET);
         (*nkn_fread)(&num_sample_chnk,4,1,fid);
         num_sample_chnk = uint32_byte_swap(num_sample_chnk);

         total_sample_num+=num_chunks*num_sample_chnk;
         // data+=12;
         if(sample_number<total_sample_num){
             //      chunk_num =first_chunk+((total_sample_num-sample_number-1)/num_sample_chnk);
             //      chunk_num = first_chunk + (sample_number/num_sample_chnk);
             chunk_num = first_chunk + ((sample_number-total_prev)/num_sample_chnk);
             *prev_sample_num = i;
             *prev_total = total_sample_num-num_chunks*num_sample_chnk;
             i=entry_count;
         } else if(sample_number >= total_sample_num) {//should not exceed the total num of samples though, might want to add that check
             chunk_num = i;
         }
     }
     if(chunk_num<1)
	 chunk_num=1;

     return chunk_num;
 }

/* reads all teh track assets needed for parsing, namely sample tables and description boxes
 */
int32_t get_trak_asset(uint8_t* stbl_data, Stbl_t* stbl)
{
    /*This should return all 4 traks*/
    int32_t box_size,size,stbl_box_size;
    size_t bytes_read;
    box_t box;
    uint8_t* p_data;
    char stsc_id[] = {'s', 't', 's', 'c'};
    char stco_id[] = {'s', 't', 'c', 'o'};
    char stsz_id[] = {'s', 't', 's', 'z'};
    char stts_id[] = {'s', 't', 't', 's'};
    char ctts_id[] = {'c', 't', 't', 's'};
    char stsd_id[] = {'s', 't', 's', 'd'};
    char stss_id[] = {'s', 't', 's', 's'};

    size = 10000;
    stbl_box_size = read_next_root_level_box(stbl_data, size, &box, &bytes_read);
    p_data=stbl_data;
    stbl_data+=bytes_read;
    while(stbl_data-p_data<stbl_box_size){
	box_size = read_next_root_level_box(stbl_data, size, &box, &bytes_read);
	if (box_size < 0)
	    { 
		return NOMORE_SAMPLES;
	    }

	if(check_box_type(&box,stsc_id)){

	    stbl->stsc_tab.offset+= (int32_t)(stbl_data-p_data);
	    stbl->stsc_tab.size = box_size;
	    stbl_data+=box_size;
	}
	else if(check_box_type(&box,stsz_id)){
            stbl->stsz_tab.offset+= (int32_t)(stbl_data-p_data);
            stbl->stsz_tab.size = box_size;
            stbl_data+=box_size;
        }
	else if(check_box_type(&box,stco_id)){
            stbl->stco_tab.offset+= (int32_t)(stbl_data-p_data);
            stbl->stco_tab.size = box_size;
            stbl_data+=box_size;

        }
        else if(check_box_type(&box,stts_id)){
            stbl->stts_tab.offset+= (int32_t)(stbl_data-p_data);
            stbl->stts_tab.size = box_size;
            stbl_data+=box_size;
        }
	else if(check_box_type(&box,ctts_id)){
            stbl->ctts_tab.offset+= (int32_t)(stbl_data-p_data);
            stbl->ctts_tab.size = box_size;
            stbl_data+=box_size;
        }
	else if(check_box_type(&box,stsd_id)){
            stbl->stsd_tab.offset+= (int32_t)(stbl_data-p_data);
            stbl->stsd_tab.size = box_size;
            stbl_data+=box_size;
        }
        else if(check_box_type(&box,stss_id)){
            stbl->stss_tab.offset+= (int32_t)(stbl_data-p_data);
            stbl->stss_tab.size = box_size;
	    stbl_data+=box_size;
        }
	else
	    stbl_data+=box_size;
    }
    return 1;
}


int32_t  init_stbls(Stbl_t* stbl,int32_t pos)
{
    stbl->stsc_tab.size=-1;
    stbl->stsz_tab.size=-1;
    stbl->stco_tab.size=-1;
    stbl->stts_tab.size=-1;
    stbl->ctts_tab.size=-1;
    stbl->stsd_tab.size=-1;
    stbl->esds_tab.size=-1;
    stbl->codec_type = -1;

    stbl->stss_tab.size=-1;
    stbl->stss_tab.offset=pos;

    stbl->stsc_tab.offset=pos;
    stbl->stsz_tab.offset=pos;
    stbl->stco_tab.offset=pos;
    stbl->stts_tab.offset=pos;
    stbl->ctts_tab.offset=pos;
    stbl->stsd_tab.offset=pos;
    stbl->esds_tab.offset=pos;
    return 1;
}
#if 0
int32_t free_stbls(Stbl_t* stbl){


    if(stbl->stsc!=NULL){
        free(stbl->stsc);
    }
    if(stbl->stco!=NULL){
        free(stbl->stco);
    }
    if(stbl->stts!=NULL){
        free(stbl->stts);
    }
    if(stbl->ctts!=NULL){
        free(stbl->ctts);
    }
    if(stbl->stsz!=NULL){
        free(stbl->stsz);
    }
    if(stbl->stss!=NULL){
        free(stbl->stss);
    }
    return 1;
}
#endif
int32_t read_stbls_opt(Stbl_t* stbl,size_t (*nkn_fread)( void *, size_t, size_t, FILE*), FILE *fid)
 {
#if 0
     if(stbl->stsc_tab.size!=-1){
	 stbl->stsc = (uint8_t*)malloc(stbl->stsc_tab.size);
	 nkn_vfs_fseek(fid,stbl->stsc_tab.offset,SEEK_SET);
	 (*nkn_fread)( stbl->stsc,stbl->stsc_tab.size,1,fid);
     }
#endif
#if 0
     if(stbl->stco_tab.size!=-1){
	 stbl->stco = (uint8_t*)malloc(stbl->stco_tab.size);
	 nkn_vfs_fseek(fid,stbl->stco_tab.offset,SEEK_SET);
	 (*nkn_fread)( stbl->stco,stbl->stco_tab.size,1,fid);
     }
#endif
#if 0
     if(stbl->stsz_tab.size!=-1){
	 stbl->stsz = (uint8_t*)malloc(stbl->stsz_tab.size);
	 nkn_vfs_fseek(fid,stbl->stsz_tab.offset,SEEK_SET);
	 (*nkn_fread)( stbl->stsz,stbl->stsz_tab.size,1,fid);
     }
#endif
#if 0
     if(stbl->stts_tab.size!=-1){
	 stbl->stts = (uint8_t*)malloc(stbl->stts_tab.size);
	 nkn_vfs_fseek(fid,stbl->stts_tab.offset,SEEK_SET);
	 (*nkn_fread)(stbl->stts,stbl->stts_tab.size,1,fid);
     }
#endif
#if 0
     if(stbl->ctts_tab.size!=-1){
         stbl->ctts = (uint8_t*)malloc(stbl->ctts_tab.size);
         nkn_vfs_fseek(fid,stbl->ctts_tab.offset,SEEK_SET);
         (*nkn_fread)(stbl->ctts,stbl->ctts_tab.size,1,fid);
     }
#endif
#if 0
     if(stbl->stss_tab.size!=-1){
	 stbl->stss = (uint8_t*)malloc(stbl->stss_tab.size);
	 nkn_vfs_fseek(fid,stbl->stss_tab.offset,SEEK_SET);
	 (*nkn_fread)(stbl->stss,stbl->stss_tab.size,1,fid);
     }
#endif
     return 1;
 }

int32_t get_Media_stbl_info(uint8_t *data,Media_stbl_t* mstbl,int32_t moov_box_size)
{
    int32_t bytes_left,box_size,trak_size,video_track_id=-1,audio_track_id=-1,hint_ref_id=-1,size,timescale,ret;
    size_t bytes_read;
    uint8_t* p_data,*p_trak;
    box_t box;
    char trak_id[] = {'t', 'r', 'a', 'k'};
    char mdia_id[] = {'m', 'd', 'i', 'a'};
    char stbl_id[] = {'s', 't', 'b', 'l'};
    char tkhd_id[] = {'t', 'k', 'h', 'd'};
    char tref_id[] = {'t', 'r', 'e', 'f'};
    char hint_id[] = {'h', 'i', 'n', 't'};
    char hdlr_id[] = {'h', 'd', 'l', 'r'};
    char minf_id[] = {'m', 'i', 'n', 'f'};
    char mvhd_id[] = {'m', 'v', 'h', 'd'};
    char mdhd_id[] = {'m', 'd', 'h', 'd'};
    char mp4a_id[] = {'m', 'p', '4', 'a'};
    char avc1_id[] = {'a', 'v', 'c', '1'};
    char mp4v_id[] = {'m', 'p', '4', 'v'};

    p_data = data;
    bytes_left=moov_box_size;
    size=moov_box_size;
    p_data+=8;
    bytes_left-=8;

    while(bytes_left>0){
	box_size = read_next_root_level_box(p_data, size, &box, &bytes_read);
	if(check_box_type(&box,trak_id)){
	    int32_t track_id;
	    trak_size=box_size;
	    p_trak=p_data;
	    p_trak+=bytes_read;
	    while(p_trak-p_data < trak_size){
		int32_t trak_type;
		box_size = read_next_root_level_box(p_trak, size, &box, &bytes_read);
		if(check_box_type(&box,tkhd_id)){
		    track_id = return_track_id(p_trak+bytes_read);

		    p_trak+=box_size;
		}
		else if(check_box_type(&box,tref_id)){
		    uint8_t *tref_data;
		    int32_t subbox_size;
		    tref_data=p_trak+bytes_read;
		    subbox_size = read_next_root_level_box(tref_data, size, &box, &bytes_read);
		    if(check_box_type(&box,hint_id)){
			hint_ref_id = _read32(tref_data,8);
		    }		
                    p_trak+=box_size;
		}
		else if(check_box_type(&box,mdia_id)){
		    int32_t mdia_box_size;
		    uint8_t* p_mdia;
		    p_mdia=p_trak;
		    mdia_box_size=box_size;
		    p_mdia+=bytes_read;
		    while(p_mdia-p_trak<mdia_box_size){
			box_size = read_next_root_level_box(p_mdia, size, &box, &bytes_read);
			if(check_box_type(&box,hdlr_id)){
			    hdlr_t *hdlr;
			    hdlr = (hdlr_t*)hdlr_reader(p_mdia,0,box_size);
			    switch(hdlr->handler_type){
				case VIDE_HEX:
				    trak_type=VIDEO_TRAK;
				    video_track_id=track_id;
                                    mstbl->VideoStbl.trak_type=track_id;
                                    mstbl->VideoStbl.timescale = timescale;
				    break;
				case SOUN_HEX:
				    trak_type= AUDIO_TRAK;
				    audio_track_id=track_id;
                                    mstbl->AudioStbl.trak_type=track_id;
                                    mstbl->AudioStbl.timescale = timescale;
				    break;
				case HINT_HEX:
				    if(hint_ref_id==video_track_id){
					trak_type=VIDEO_HINT_TRAK;
                                        mstbl->HintVidStbl.trak_type=track_id;
                                        mstbl->HintVidStbl.timescale = timescale;
				    }
                                    if(hint_ref_id==audio_track_id){
                                        trak_type=AUDIO_HINT_TRAK;
                                        mstbl->HintAudStbl.trak_type=track_id;
                                        mstbl->HintAudStbl.timescale = timescale;
				    }
                                    break;
			    }
			    p_mdia+=box_size;
			    hdlr_cleanup(hdlr);
			}
			else if(check_box_type(&box,mdhd_id)){
                            uint8_t *p_mdhd;
                            p_mdhd = p_mdia;
                            p_mdhd+=8;
                            if(p_mdhd[0])
                                p_mdhd+=16+4;

                            else
                                p_mdhd+=8+4;

                            timescale = _read32(p_mdhd,0);
                            p_mdia+=box_size;
                        }
			else if(check_box_type(&box,minf_id)){
			    int32_t minf_size,temp_offset,box_size_pminf;
			    uint8_t* p_minf;
			    p_minf = p_mdia;
			    p_minf+=bytes_read;
			    minf_size=box_size;
			    while(p_minf-p_mdia<minf_size){
				box_size_pminf = read_next_root_level_box(p_minf, size, &box, &bytes_read);
				if(check_box_type(&box,stbl_id)){
				    uint8_t* stbl_data;
				    uint8_t* stsd_data;
				    uint8_t* mp4a_data;

				    Stbl_t* stb;
				    //int32_t stbl_size;
				    stbl_data=p_minf;

				    //stbl_data+=bytes_read;
				    //stbl_size = box_size;
				    stsd_data = stbl_data +8;
				    mp4a_data = stsd_data + 16;
				    box_size = read_next_root_level_box(mp4a_data, size, &box, &bytes_read);
				    switch(trak_type){
					case VIDEO_TRAK:
					    stb=&mstbl->VideoStbl;
					    if(check_box_type(&box,mp4v_id))
						{
						    mstbl->VideoStbl.codec_type = MPEG4_VIDEO;
						}
					    if(check_box_type(&box,avc1_id))
						{
						    mstbl->VideoStbl.codec_type = H264_VIDEO;
						}
					    if(check_box_type(&box,avc1_id)||check_box_type(&box,mp4v_id))
						{
					    temp_offset = (int32_t)(stbl_data-data);

					    stb->stco_tab.offset+=(int32_t)(stbl_data-data);
					    stb->stsz_tab.offset+=(int32_t)(stbl_data-data);
					    stb->stts_tab.offset+=(int32_t)(stbl_data-data);
					    stb->ctts_tab.offset+=(int32_t)(stbl_data-data);
					    stb->stsc_tab.offset+=temp_offset;
					    stb->stsd_tab.offset+=(int32_t)(stbl_data-data);
					    stb->esds_tab.offset+=(int32_t)(stbl_data-data);
					    stb->stss_tab.offset+=(int32_t)(stbl_data-data);

					    ret = get_trak_asset(stbl_data, stb);
					    if (ret == NOMORE_SAMPLES)
						{
						    return ret;
						}
						}
					    break;
					case AUDIO_TRAK:
                                            stb=&mstbl->AudioStbl;
					    if(check_box_type(&box,mp4a_id))
						{
						    mstbl->AudioStbl.codec_type = MPEG4_AUDIO;
						    temp_offset = (int32_t)(stbl_data-data);

						    stb->stco_tab.offset+=(int32_t)(stbl_data-data);
						    stb->stsz_tab.offset+=(int32_t)(stbl_data-data);
						    stb->stts_tab.offset+=(int32_t)(stbl_data-data);
						    stb->ctts_tab.offset+=(int32_t)(stbl_data-data);
						    stb->stsc_tab.offset+=temp_offset;
						    stb->stsd_tab.offset+=(int32_t)(stbl_data-data);
						    stb->esds_tab.offset+=(int32_t)(stbl_data-data);
						    stb->stss_tab.offset+=(int32_t)(stbl_data-data);

						    ret = get_trak_asset(stbl_data, stb);
						    if (ret == NOMORE_SAMPLES)
							{
							    return ret;
							}
                                                }
                                            break;
					case VIDEO_HINT_TRAK:
                                            stb=&mstbl->HintVidStbl;
					    if(check_box_type(&box,mp4v_id))
						{
						    mstbl->HintVidStbl.codec_type = MPEG4_VIDEO;
						}
					    if(check_box_type(&box,avc1_id))
						{
						    mstbl->HintVidStbl.codec_type = H264_VIDEO;
						}
					    if((mstbl->VideoStbl.codec_type == MPEG4_VIDEO)||(mstbl->VideoStbl.codec_type == H264_VIDEO))
                                                {
                                                    temp_offset = (int32_t)(stbl_data-data);

                                                    stb->stco_tab.offset+=(int32_t)(stbl_data-data);
                                                    stb->stsz_tab.offset+=(int32_t)(stbl_data-data);
                                                    stb->stts_tab.offset+=(int32_t)(stbl_data-data);
						    stb->ctts_tab.offset+=(int32_t)(stbl_data-data);
                                                    stb->stsc_tab.offset+=temp_offset;
                                                    stb->stsd_tab.offset+=(int32_t)(stbl_data-data);
                                                    stb->esds_tab.offset+=(int32_t)(stbl_data-data);
                                                    stb->stss_tab.offset+=(int32_t)(stbl_data-data);

						    ret = get_trak_asset(stbl_data, stb);
						    if (ret == NOMORE_SAMPLES)
							{
							    return ret;
							}
                                                }
                                            break;
					case AUDIO_HINT_TRAK:
                                            stb=&mstbl->HintAudStbl;
					    if(check_box_type(&box,mp4a_id))
						{
						    mstbl->HintAudStbl.codec_type = MPEG4_AUDIO;
						}
					    if(mstbl->AudioStbl.codec_type == MPEG4_AUDIO)
                                                {
                                                    temp_offset = (int32_t)(stbl_data-data);

                                                    stb->stco_tab.offset+=(int32_t)(stbl_data-data);
                                                    stb->stsz_tab.offset+=(int32_t)(stbl_data-data);
                                                    stb->stts_tab.offset+=(int32_t)(stbl_data-data);
						    stb->ctts_tab.offset+=(int32_t)(stbl_data-data);
                                                    stb->stsc_tab.offset+=temp_offset;
                                                    stb->stsd_tab.offset+=(int32_t)(stbl_data-data);
                                                    stb->esds_tab.offset+=(int32_t)(stbl_data-data);
                                                    stb->stss_tab.offset+=(int32_t)(stbl_data-data);

						    ret = get_trak_asset(stbl_data, stb);
						    if (ret == NOMORE_SAMPLES)
							{
							    return ret;
							}
                                                }
                                            break;
					default:
					    break;
				    }

                                    //p_minf+=box_size;
				    p_minf+=box_size_pminf;
				}
				else
				    //p_minf+=box_size;
				    p_minf+=box_size_pminf;
			    }
			    p_mdia+=minf_size;

			}
			else{
			    p_mdia+=box_size;
			}
		    }//while(p_mdia-p_data<mdia_box_size)
		    p_trak+=mdia_box_size;
		}//if(check_box_type(&box,mdia_id))
		else
		    p_trak+=box_size;
	    }// while(p_trak-p_data < trak_size)
	    p_data+=trak_size;
	    bytes_left-=trak_size;
	}// if(check_box_type(&box,trak_id))
	else if(check_box_type(&box,mvhd_id)){
            uint8_t *mvhd_data,version;
            //int32_t timescale;
            int64_t tempval;
            mvhd_data=p_data+8;
            version = mvhd_data[0];
            mvhd_data+=4;
            if(version){
                mvhd_data+=16;
                timescale = _read32(mvhd_data,0);
                mvhd_data+=4;
                tempval = (int64_t)(_read32(mvhd_data,0));//((_read32(p_data,0))<<32)|(_read32(p_data,4);
                tempval = (tempval<<32)|((int64_t)(_read32(mvhd_data,4)));
                mstbl->duration = (int32_t)(tempval/(int64_t)(timescale));
            }
            else{
                mvhd_data+=8;
                timescale = _read32(mvhd_data,0);
                mvhd_data+=4;
                mstbl->duration = _read32(mvhd_data,0);
                mstbl->duration/=timescale;
            }
            p_data+=box_size;
            bytes_left-=box_size;
        }
	else{
	    p_data+=box_size;
	    bytes_left-=box_size;
	}
    }
    return 0;
}



/*reads exactly one RTP sample
 */
 int32_t read_rtp_sample_interface(uint8_t* data, Media_sample_t* attr,int32_t base, int32_t base1)
 {
     int32_t extra_flag,data_pos,i,j;
     RTP_whole_pkt_t * start_rtp,*rtp_init;
     uint8_t *data_init;
     Payload_info_t* payload_init;
     int8_t data_ref_idx;//-1 indicates refer from same track, else from the media track corresponding to it

     data_init=data;
     attr->num_rtp_pkts = _read16(data,0);
     attr->rtp = (RTP_whole_pkt_t *)malloc(attr->num_rtp_pkts*sizeof(RTP_whole_pkt_t)); //where are we free'ing this
     data_pos=4;
     start_rtp = attr->rtp;
     for(i=0;i<attr->num_rtp_pkts;i++){
	 attr->rtp->header.relative_time = _read32(data,data_pos);
	 data_pos+=4;
	 attr->rtp->header.rtp_flags1 = _read16(data,data_pos);
	 data_pos+=2;
	 attr->rtp->header.RTPSequenceseed = _read16(data,data_pos);
	 data_pos+=2;
	 attr->rtp->header.rtp_flags2 = _read16(data,data_pos);
	 data_pos++;
	 extra_flag=(data[data_pos]>>2)&0x01;
	 data_pos++;
	 attr->rtp->header.entrycount = _read16(data,data_pos);
	 data_pos+=2;attr->rtp->time_stamp = 0;
	 if(extra_flag){
	     // data_pos+=4;
	     //data_pos+=_read32(data,data_pos);//+4
	     attr->rtp->time_stamp = _read32(data,data_pos+12);
	     data_pos+=_read32(data,data_pos);
	 }
	 attr->rtp->payload=(Payload_info_t*)malloc(sizeof(Payload_info_t)*attr->rtp->header.entrycount);
	 attr->rtp->total_payload_size=0;
	 attr->rtp->num_payloads=attr->rtp->header.entrycount;
	 for(j=0;j<attr->rtp->header.entrycount;j++){
	     attr->rtp->type=data[data_pos];
	     data_pos++;
	     switch(attr->rtp->type){
		 case 0:
		     //Just no-op data
		     attr->rtp->payload->payload_offset=data_pos+base;
		     attr->rtp->payload->payload_size=15;
		     attr->rtp->total_payload_size+=attr->rtp->payload->payload_size;
		     data_pos+=15;
		     break;
		 case 1:
		     attr->rtp->payload->payload_size=data[data_pos];
		     attr->rtp->payload->payload_offset=base+data_pos+1;/*data is directly referred here */ //data[data_pos+1]+base;
		     attr->rtp->total_payload_size+=attr->rtp->payload->payload_size;
		     data_pos+=15;
		     break;
		 case 2:
		     data_ref_idx = data[data_pos];
		     attr->rtp->payload->payload_size=_read16(data,data_pos+1);
		     attr->rtp->payload->sample_num=_read32(data,data_pos+3);
		     attr->rtp->payload->payload_offset = _read32(data,data_pos+7)+ ( (data_ref_idx==-1) * base) +( (data_ref_idx!=-1) *base1);
		     attr->rtp->total_payload_size+=attr->rtp->payload->payload_size;
		     data_pos+=15;
		     break;
		 case 3:
		     attr->rtp->payload->payload_size=0;
		     attr->rtp->total_payload_size+=attr->rtp->payload->payload_size;
		     data_pos+=15;
		     break;
	     }
	     if(j==0)
		 payload_init = attr->rtp->payload;
	     attr->rtp->payload++;
	 }
	 if(i==0)
	     rtp_init = attr->rtp;
	 attr->rtp->payload=payload_init;//payload pointer should be init'ed after every payload loop, it was done after all the packets were read.
	 //this will fail when there are more than one packets
	 attr->rtp++;

     }
     attr->rtp=rtp_init;

     return 1;
 }
int32_t read_rtp_sample(uint8_t* data, Hint_sample_t* attr,int32_t base, int32_t base1)
{
    int32_t extra_flag,data_pos,i,j;
    RTP_whole_pkt_t * start_rtp,*rtp_init;
    uint8_t *data_init;
    Payload_info_t* payload_init;
    int8_t data_ref_idx;//-1 indicates refer from same track, else from the media track corresponding to it

    data_init=data;
    attr->num_rtp_pkts = _read16(data,0);
    attr->rtp = (RTP_whole_pkt_t *)malloc(attr->num_rtp_pkts*sizeof(RTP_whole_pkt_t)); //where are we free'ing this
    data_pos=4;
    start_rtp = attr->rtp;
    for(i=0;i<attr->num_rtp_pkts;i++){
	attr->rtp->header.relative_time = _read32(data,data_pos);
	data_pos+=4;
	attr->rtp->header.rtp_flags1 = _read16(data,data_pos);
	data_pos+=2;
	attr->rtp->header.RTPSequenceseed = _read16(data,data_pos);
	data_pos+=2;
	attr->rtp->header.rtp_flags2 = _read16(data,data_pos);
	data_pos++;
	extra_flag=(data[data_pos]>>2)&0x01;
	data_pos++;
	attr->rtp->header.entrycount = _read16(data,data_pos);
	data_pos+=2;attr->rtp->time_stamp = 0;
	if(extra_flag){
	    // data_pos+=4;
	    //data_pos+=_read32(data,data_pos);//+4
            attr->rtp->time_stamp = _read32(data,data_pos+12);
            data_pos+=_read32(data,data_pos);
	}
	attr->rtp->payload=(Payload_info_t*)malloc(sizeof(Payload_info_t)*attr->rtp->header.entrycount);
	attr->rtp->total_payload_size=0;
	attr->rtp->num_payloads=attr->rtp->header.entrycount;
	for(j=0;j<attr->rtp->header.entrycount;j++){
	    attr->rtp->type=data[data_pos];
	    data_pos++;
	    switch(attr->rtp->type){
		case 0:
		    //Just no-op data
		    attr->rtp->payload->payload_offset=data_pos+base;
		    attr->rtp->payload->payload_size=15;
		    attr->rtp->total_payload_size+=attr->rtp->payload->payload_size;
		    data_pos+=15;
		    break;
		case 1: 
		    attr->rtp->payload->payload_size=data[data_pos];
		    attr->rtp->payload->payload_offset=base+data_pos+1;/*data is directly referred here */ //data[data_pos+1]+base;
                    attr->rtp->total_payload_size+=attr->rtp->payload->payload_size;
		    data_pos+=15;
		    break;
		case 2:
		    data_ref_idx = data[data_pos];
		    attr->rtp->payload->payload_size=_read16(data,data_pos+1);
		    attr->rtp->payload->sample_num=_read32(data,data_pos+3);
		    attr->rtp->payload->payload_offset = _read32(data,data_pos+7)+ ( (data_ref_idx==-1) * base) +( (data_ref_idx!=-1) *base1);
                    attr->rtp->total_payload_size+=attr->rtp->payload->payload_size;
		    data_pos+=15;
		    break;
		case 3:
		    attr->rtp->payload->payload_size=0;
                    attr->rtp->total_payload_size+=attr->rtp->payload->payload_size;
		    data_pos+=15;
		    break;
	    }
	    if(j==0)
		payload_init = attr->rtp->payload;
	    attr->rtp->payload++;
	}
	if(i==0)
	    rtp_init = attr->rtp;
	attr->rtp->payload=payload_init;//payload pointer should be init'ed after every payload loop, it was done after all the packets were read. 
	                                //this will fail when there are more than one packets
	attr->rtp++;

    }
    attr->rtp=rtp_init;
    
    return 1;
}




int32_t read_rtp_sample_audio(uint8_t* data, Hint_sample_t* attr,int32_t base, int32_t base1,Stbl_t* HintStbl,Stbl_t* MediaStbl,Sample_details_t *hnt_sample,Sample_details_t* mdia_sample,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE *fid)
{
    int32_t extra_flag,data_pos,i,j,base_b;
    RTP_whole_pkt_t * start_rtp,*rtp_init;
    uint8_t *data_init;
    Payload_info_t* payload_init;
    int8_t data_ref_idx;//-1 indicates refer from same track, else from the media track corresponding to it

    //FILE *fid;
    //fid = fopen("old_audio","ab");


    data_init=data;
    attr->num_rtp_pkts = _read16(data,0);
    attr->rtp = (RTP_whole_pkt_t *)malloc(attr->num_rtp_pkts*sizeof(RTP_whole_pkt_t)); //where are we free'ing this
    data_pos=4;
    start_rtp = attr->rtp;
    for(i=0;i<attr->num_rtp_pkts;i++){
        attr->rtp->header.relative_time = _read32(data,data_pos);
        data_pos+=4;
        attr->rtp->header.rtp_flags1 = _read16(data,data_pos);
        data_pos+=2;
        attr->rtp->header.RTPSequenceseed = _read16(data,data_pos);
        data_pos+=2;
        attr->rtp->header.rtp_flags2 = _read16(data,data_pos);
        data_pos++;
        extra_flag=(data[data_pos]>>2)&0x01;
        data_pos++;
        attr->rtp->header.entrycount = _read16(data,data_pos);
        data_pos+=2;attr->rtp->time_stamp = 0;
        if(extra_flag){
            // data_pos+=4;
            //data_pos+=_read32(data,data_pos);//+4
            attr->rtp->time_stamp = _read32(data,data_pos+12);
            data_pos+=_read32(data,data_pos);
        }

        attr->rtp->payload=(Payload_info_t*)malloc(sizeof(Payload_info_t)*attr->rtp->header.entrycount);
        attr->rtp->total_payload_size=0;
        attr->rtp->num_payloads=attr->rtp->header.entrycount;
        for(j=0;j<attr->rtp->header.entrycount;j++){
            attr->rtp->type=data[data_pos];
            data_pos++;
            switch(attr->rtp->type){
                case 0:
                    //Just no-op data
                    attr->rtp->payload->payload_offset=data_pos+base;
                    attr->rtp->payload->payload_size=15;
                    attr->rtp->total_payload_size+=attr->rtp->payload->payload_size;
                    data_pos+=15;
                    break;
                case 1:
                    attr->rtp->payload->payload_size=data[data_pos];
                    attr->rtp->payload->payload_offset=base+data_pos+1;/*data is directly referred here */ //data[data_pos+1]+base;
                    attr->rtp->total_payload_size+=attr->rtp->payload->payload_size;
                    data_pos+=15;
		    break;
                case 2:
                    data_ref_idx = data[data_pos];
                    attr->rtp->payload->payload_size=_read16(data,data_pos+1);
                    attr->rtp->payload->sample_num=_read32(data,data_pos+3);
		    if(data_ref_idx==-1)
		    base_b = get_smpl_pos_opt(attr->rtp->payload->sample_num,HintStbl,hnt_sample,nkn_fread,fid);
		    else

		    base_b = get_smpl_pos_opt(attr->rtp->payload->sample_num,MediaStbl,mdia_sample,nkn_fread,fid);
			//		fwrite(&base_b,sizeof(int32_t),1,fid);

                    attr->rtp->payload->payload_offset = _read32(data,data_pos+7)+ base_b;//( (data_ref_idx==-1) * base) +( (data_ref_idx!=-1) *base1);
                    attr->rtp->total_payload_size+=attr->rtp->payload->payload_size;
                    data_pos+=15;
                    break;
                case 3:
                    attr->rtp->payload->payload_size=0;
                    attr->rtp->total_payload_size+=attr->rtp->payload->payload_size;
                    data_pos+=15;
                    break;
            }
            if(j==0)
                payload_init = attr->rtp->payload;
            attr->rtp->payload++;
        }
        if(i==0)
            rtp_init = attr->rtp;
        attr->rtp->payload=payload_init;//payload pointer should be init'ed after every payload loop, it was done after all the packets were read.
                                        //this will fail when there are more than one packets
        attr->rtp++;

    }
    attr->rtp=rtp_init;


    //    fclose(fid);

    return 1;
}


int32_t read_rtp_sample_audio_interface(uint8_t* data, Media_sample_t* attr,int32_t base, int32_t base1,Stbl_t* HintStbl,Stbl_t* MediaStbl,Sample_details_t *hnt_sample,Sample_details_t* mdia_sample,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE *fid)
 {
     int32_t extra_flag,data_pos,i,j,base_b;
     RTP_whole_pkt_t * start_rtp,*rtp_init;
     uint8_t *data_init;
     Payload_info_t* payload_init;
     int8_t data_ref_idx;//-1 indicates refer from same track, else from the media track corresponding to it

     //FILE *fid;
     //fid = fopen("old_audio","ab");


     data_init=data;
     attr->num_rtp_pkts = _read16(data,0);
     attr->rtp = (RTP_whole_pkt_t *)malloc(attr->num_rtp_pkts*sizeof(RTP_whole_pkt_t)); //where are we free'ing this
     data_pos=4;
     start_rtp = attr->rtp;
     for(i=0;i<attr->num_rtp_pkts;i++){
	 attr->rtp->header.relative_time = _read32(data,data_pos);
	 data_pos+=4;
	 attr->rtp->header.rtp_flags1 = _read16(data,data_pos);
	 data_pos+=2;
	 attr->rtp->header.RTPSequenceseed = _read16(data,data_pos);
	 data_pos+=2;
	 attr->rtp->header.rtp_flags2 = _read16(data,data_pos);
	 data_pos++;
	 extra_flag=(data[data_pos]>>2)&0x01;
	 data_pos++;
	 attr->rtp->header.entrycount = _read16(data,data_pos);
	 data_pos+=2;attr->rtp->time_stamp = 0;
	 if(extra_flag){
	     // data_pos+=4;
	     //data_pos+=_read32(data,data_pos);//+4
	     attr->rtp->time_stamp = _read32(data,data_pos+12);
	     data_pos+=_read32(data,data_pos);
	 }
	 attr->rtp->payload=(Payload_info_t*)malloc(sizeof(Payload_info_t)*attr->rtp->header.entrycount);
	 attr->rtp->total_payload_size=0;
	 attr->rtp->num_payloads=attr->rtp->header.entrycount;
	 for(j=0;j<attr->rtp->header.entrycount;j++){
	     attr->rtp->type=data[data_pos];
	     data_pos++;
	     switch(attr->rtp->type){
		 case 0:
		     //Just no-op data
		     attr->rtp->payload->payload_offset=data_pos+base;
		     attr->rtp->payload->payload_size=15;
		     attr->rtp->total_payload_size+=attr->rtp->payload->payload_size;
		     data_pos+=15;
		     break;
		 case 1:
		     attr->rtp->payload->payload_size=data[data_pos];
		     attr->rtp->payload->payload_offset=base+data_pos+1;/*data is directly referred here */ //data[data_pos+1]+base;
		     attr->rtp->total_payload_size+=attr->rtp->payload->payload_size;
		     data_pos+=15;
		     break;
		 case 2:
		     data_ref_idx = data[data_pos];
		     attr->rtp->payload->payload_size=_read16(data,data_pos+1);
		     attr->rtp->payload->sample_num=_read32(data,data_pos+3);
		     if(data_ref_idx==-1)
		     base_b = get_smpl_pos_opt(attr->rtp->payload->sample_num,HintStbl,hnt_sample,nkn_fread,fid);
		     else
		     base_b = get_smpl_pos_opt(attr->rtp->payload->sample_num,MediaStbl,mdia_sample,nkn_fread,fid);
		     //              fwrite(&base_b,sizeof(int32_t),1,fid);

		     attr->rtp->payload->payload_offset = _read32(data,data_pos+7)+ base_b;//( (data_ref_idx==-1) * base) +( (data_ref_idx!=-1) *base1);
		     attr->rtp->total_payload_size+=attr->rtp->payload->payload_size;
		     data_pos+=15;
		     break;
		 case 3:
		     attr->rtp->payload->payload_size=0;
		     attr->rtp->total_payload_size+=attr->rtp->payload->payload_size;
		     data_pos+=15;
		     break;
	     }
	     if(j==0)
		 payload_init = attr->rtp->payload;
	     attr->rtp->payload++;
	 }
	 if(i==0)
	     rtp_init = attr->rtp;
	 attr->rtp->payload=payload_init;//payload pointer should be init'ed after every payload loop, it was done after all the packets were read.
	 //this will fail when there are more than one packets
	 attr->rtp++;

     }
     attr->rtp=rtp_init;


     //    fclose(fid);

     return 1;
 }


int32_t get_smpl_pos_opt(int32_t num_samples_to_send,Stbl_t *stbl,Sample_details_t* prev_sample,size_t (*nkn_fread)( void *, size_t, size_t, FILE*), FILE *fid)
{
    int32_t temp_sam_size,samp_cnt;
    int32_t chunk_num;
    uint8_t* data;
    int32_t sample_size,offsets;
    //Sampl_prop_t* prop;


    if(num_samples_to_send>0)
	num_samples_to_send--;
    else
	num_samples_to_send=0;

    //    if(num_samples_to_send!=prev_sample->sample_num+1)
    //      printf("\n\n\n\n\n\n\nSAMPLES DONT MATCH!!!!!!!!\n\n");

    //GET SIZES OF SAMPLES:
    int32_t new_pos;
    nkn_vfs_fseek(fid,(stbl->stsz_tab.offset)+12,SEEK_SET);
    (*nkn_fread)(&temp_sam_size,4,1,fid);
    temp_sam_size = uint32_byte_swap(temp_sam_size);


    nkn_vfs_fseek(fid,(stbl->stsz_tab.offset)+16,SEEK_SET);
    (*nkn_fread)(&samp_cnt,4,1,fid);
    samp_cnt = uint32_byte_swap(samp_cnt);
    // prop->total_sample_size=samp_cnt;

    if(temp_sam_size==0)
	{
	    new_pos=read_stsz_opt(num_samples_to_send,stbl->stsz_tab.offset+20);
	    nkn_vfs_fseek(fid,new_pos,SEEK_SET);
	    (*nkn_fread)(&sample_size,4,1,fid);
	    sample_size = uint32_byte_swap(sample_size);
        }
    else
	{
	    sample_size = temp_sam_size;
	}
    //Get chunk in which sample is present.
    data=stbl->stsc;
    //  for(i=0;i<num_samples_to_send;i++)
    //  chunk_num[i]=get_chunk_offsets(sample_num+i,stbl->stsc);

    chunk_num=get_chunk_offsets_opt(num_samples_to_send,stbl,prev_sample->total_chunks, &prev_sample->chunk_count,&prev_sample->prev_sample_total,nkn_fread,fid);

    //Get chunk offsets as file offsets:
    new_pos=read_stco_opt(chunk_num,stbl->stco_tab.offset+12);
    nkn_vfs_fseek(fid,new_pos,SEEK_SET);
    (*nkn_fread)(&offsets,4,1,fid);
    offsets = uint32_byte_swap(offsets);
    if(prev_sample->is_stateless){
	int32_t base_offset,first_sample_of_chunk,sample_num;
	sample_num= num_samples_to_send;
	//Find position given a chunk number and
	first_sample_of_chunk=find_first_sample_of_chunk_opt(stbl,chunk_num,nkn_fread,fid);
	//Calculate the offset of the offset of this sample in file.
	base_offset = find_chunk_offset_opt(stbl,chunk_num,nkn_fread,fid);
	//Find cumulative offset in chunk to present sample in chunk
	offsets = base_offset + find_cumulative_sample_size_opt(stbl,first_sample_of_chunk+1,sample_num+1,nkn_fread,fid);
	prev_sample->is_stateless = 0;
	prev_sample->chunk_base=base_offset;
    }
    else{
	if(offsets==prev_sample->chunk_base && prev_sample->sample_num > 0){
	    offsets= prev_sample->chunk_offset + prev_sample->sample_size;
	}
	else{
	    //Base has changed.
	    prev_sample->chunk_base=offsets;
	}
    }

    //store this sample's number
    prev_sample->sample_num = num_samples_to_send+1;
    prev_sample->sample_size = sample_size;
    prev_sample->chunk_offset = offsets;
    return offsets;

}


/** reads meta data from the  STTS box 
 *@param data[in] - input data buffers
 *@param ts[in] - time stamp
 *@param tscale[in] - time scaled
 */

 int32_t read_stts_opt_main(Stbl_t *stbl,int32_t ts,int32_t tscale,size_t (*nkn_fread)( void *,size_t, size_t, FILE*),FILE *fid)
 {
     int32_t entry_count,delta,i,new_pos;
     int32_t tim,maxtime=0,sample_num=0,sample_count;

     nkn_vfs_fseek(fid,(stbl->stts_tab.offset)+12,SEEK_SET);
     (*nkn_fread)(&entry_count,4,1,fid);
     entry_count = uint32_byte_swap(entry_count);

     tim = ts*tscale;
     for(i=0; i<entry_count; i++){
         new_pos=read_stts_opt(i,stbl->stts_tab.offset+16);
         nkn_vfs_fseek(fid,new_pos,SEEK_SET);
         (*nkn_fread)(&sample_count,4,1,fid);
         sample_count = uint32_byte_swap(sample_count);

         nkn_vfs_fseek(fid,new_pos+4,SEEK_SET);
         (*nkn_fread)(&delta,4,1,fid);
         delta = uint32_byte_swap(delta);

         maxtime+=sample_count*delta;
         if(tim<maxtime){
             //      sample_num=tim/delta;
             maxtime-=sample_count*delta;
             sample_num+=((tim-maxtime)/delta);
	     if(sample_num==0)
		 sample_num=1;

             break;
         }
         sample_num+=sample_count;
     }
     return sample_num;
 }


/** reads meta data from the tkhd box
 *@param data[in] - input data buffers
 *@param sk[in] - container for seek meta data
 */
int32_t read_tkhd(uint8_t* p_dat, Seek_t* sk)
{
    uint8_t version;
    uint32_t pos=0,track_id;
    version=p_dat[pos];
    pos+=4;
    if(version)
        pos+=16;
    else
	pos+=8;
    track_id=(p_dat[pos]<<24)|(p_dat[pos+1]<<16)|(p_dat[pos+2]<<8)|p_dat[pos+3];
    pos+=4;
    sk->seek_trak_info+=sk->num_traks;
    sk->seek_trak_info->trak_id=track_id;    
    //    sk->num_traks++;increment in hdlr
    return 1;
}

/** returns the track id if the data is pointed to the right box (USE CAREFULLY)
 *@param p_dat[in] - input data buffers
 *@returns the track id
 */
int32_t return_track_id(uint8_t *p_dat){
    uint8_t version;
    uint32_t pos=0,track_id;
    version=p_dat[pos];
    pos+=4;
    if(version)
        pos+=16;
    else
        pos+=8;
    track_id=(p_dat[pos]<<24)|(p_dat[pos+1]<<16)|(p_dat[pos+2]<<8)|p_dat[pos+3];
    return track_id;
}


void free_all(Hint_sample_t* hnt){
    free(hnt);


}
/*
inline uint32_t _read32(uint8_t *buf, int32_t pos)
{
    return (buf[pos]<<24)|(buf[pos+1]<<16)|(buf[pos+2]<<8)|(buf[pos+3]);
}

inline uint16_t _read16(uint8_t *buf, int32_t pos)
{
    return (buf[pos]<<8)|(buf[pos+1]);
}
*/

int32_t find_trak_type(Media_stbl_t* mstbl,int32_t pid)
{
    if(mstbl->HintAudStbl.trak_type == pid)
	return AUDIO_HINT_TRAK;
    if(mstbl->HintVidStbl.trak_type == pid)
	return VIDEO_HINT_TRAK;
    if(mstbl->VideoStbl.trak_type == pid)
	return VIDEO_TRAK;
    if(mstbl->AudioStbl.trak_type == pid)
        return AUDIO_TRAK;
    return -1;
}

void get_stream_info(Media_stbl_t* media_stbl, Stream_info_t *stream)
{
    stream->num_traks = 0;
    stream->track_type[0]=-1; stream->track_type[1]=-1;

    if((media_stbl->VideoStbl.stsc_tab.size != -1)&&(media_stbl->VideoStbl.stsz_tab.size !=-1)){
        stream->num_traks++;
        stream->track_type[0]=VIDEO_TRAK;
    }
    if((media_stbl->AudioStbl.stsc_tab.size != -1)&&(media_stbl->AudioStbl.stsz_tab.size !=-1)){
        stream->num_traks++;
        stream->track_type[1]=AUDIO_TRAK;
    }
}


void free_all_stbls(Media_stbl_t* media_stbl)
{
    /* Stbl_t VideoStbl;
    Stbl_t AudioStbl;
    Stbl_t HintVidStbl;
    Stbl_t HintAudStbl;
    */
#if 0//suma
    free_stbl(&media_stbl->VideoStbl);
    free_stbl(&media_stbl->AudioStbl);
    free_stbl(&media_stbl->HintVidStbl);
    free_stbl(&media_stbl->HintAudStbl);
#endif
}

void free_stbl(Stbl_t * stbl){
    if(stbl->stsz !=NULL)
        free(stbl->stsz);
    if(stbl->stco !=NULL)
        free(stbl->stco);
    if(stbl->stts !=NULL)
        free(stbl->stts);
    if(stbl->ctts !=NULL)
        free(stbl->ctts);
    if(stbl->stsc !=NULL)
        free(stbl->stsc);
    if(stbl->stsd !=NULL)
        free(stbl->stsd);
    if(stbl->stts !=NULL)
	free(stbl->stts);
}


void free_ctx(send_ctx_t *st){
    if(st->vid_s!=NULL)
	free(st->vid_s);
    if(st->aud_s!=NULL)
        free(st->aud_s);
    if(st->hvid_s!=NULL)
        free(st->hvid_s);
    if(st->haud_s!=NULL)
        free(st->haud_s);
}

/* depraceted */

all_info_t*
init_all_info()
    {
	all_info_t* inf = (all_info_t*)calloc(1, sizeof(all_info_t));

	Seekinfo_t* sinfo = inf->seek_state_info;
	Media_stbl_t *mstbl = inf->stbl_info;
	Media_sample_t* videoh = inf->videoh;
	Media_sample_t* audioh = inf->audioh;
	Media_sample_t* video = inf->video;
	Media_sample_t* audio = inf->audio;

	inf->sample_prop_info = (send_ctx_t*)malloc(sizeof(send_ctx_t));
	inf->sample_prop_info->vid_s = (Sampl_prop_t*)calloc(1, sizeof(Sampl_prop_t));
	inf->sample_prop_info->aud_s = (Sampl_prop_t*)calloc(1, sizeof(Sampl_prop_t));
	inf->sample_prop_info->hvid_s = (Sampl_prop_t*)calloc(1, sizeof(Sampl_prop_t));
	inf->sample_prop_info->haud_s = (Sampl_prop_t*)calloc(1, sizeof(Sampl_prop_t));

	inf->videoh = (Media_sample_t*)malloc(SAMPLES_TO_BE_SENT*sizeof(Media_sample_t));
	inf->audioh = (Media_sample_t*)malloc(SAMPLES_TO_BE_SENT*sizeof(Media_sample_t));
	inf->video = (Media_sample_t*)malloc(SAMPLES_TO_BE_SENT*sizeof(Media_sample_t));
	inf->audio = (Media_sample_t*)malloc(SAMPLES_TO_BE_SENT*sizeof(Media_sample_t));

	inf->videoh->rtp = (RTP_whole_pkt_t*)malloc(sizeof(RTP_whole_pkt_t));
	inf->audioh->rtp = (RTP_whole_pkt_t*)malloc(sizeof(RTP_whole_pkt_t));
	inf->video->rtp =(RTP_whole_pkt_t*)malloc(sizeof(RTP_whole_pkt_t));
	inf->audio->rtp = (RTP_whole_pkt_t*)malloc(sizeof(RTP_whole_pkt_t));

	inf->seek_state_info=(Seekinfo_t*)malloc(sizeof(Seekinfo_t));
	inf->stbl_info=(Media_stbl_t *)calloc(1,sizeof(Media_stbl_t));

	//    Stream_info_t *stream;
	//stream =  (Stream_info_t *)calloc(1,sizeof( Stream_info_t));

	return inf;
    }

 int32_t cleanup_output_details(output_details_t* output)
 {
     if (output->rtp->payload != NULL)
         {
             free(output->rtp->payload);
             output->rtp->payload = NULL;
         }
     if (output->rtp != NULL)
         {
             free(output->rtp);
             output->rtp = NULL;
         }
     if (output != NULL)
         {
             free(output);
             output = NULL;
         }
     return 1;
 }


 int32_t free_all_info(all_info_t* st_all_info)
 {
     if(st_all_info->seek_state_info!= NULL)
	 {
	     free(st_all_info->seek_state_info);
	 }
     if(st_all_info->stbl_info!= NULL)
         {
             free(st_all_info->stbl_info);
         }
     if(st_all_info->videoh->rtp->payload!= NULL)
	 {
	     free(st_all_info->videoh->rtp->payload);
	 }
     if(st_all_info->audioh->rtp->payload!= NULL)
         {
             free(st_all_info->audioh->rtp->payload);
         }
     if(st_all_info->videoh->rtp!= NULL)
         {
             free(st_all_info->videoh->rtp);
         }
     if(st_all_info->audioh->rtp!= NULL)
         {
             free(st_all_info->audioh->rtp);
         }
     if(st_all_info->video->rtp!= NULL)
         {
             free(st_all_info->video->rtp);
         }
     if(st_all_info->audio->rtp!= NULL)
         {
             free(st_all_info->audio->rtp);
         }
     if(st_all_info->videoh!= NULL)
         {
             free(st_all_info->videoh);
         }
     if(st_all_info->audioh!= NULL)
         {
             free(st_all_info->audioh);
         }
     if(st_all_info->video!= NULL)
         {
             free(st_all_info->video);
         }
     if(st_all_info->audio!= NULL)
         {
             free(st_all_info->audio);
         }
    if(st_all_info->sample_prop_info->haud_s!= NULL)
	{
	    free(st_all_info->sample_prop_info->haud_s);
	}
    if(st_all_info->sample_prop_info->hvid_s!= NULL)
        {
            free(st_all_info->sample_prop_info->hvid_s);
        }
    if(st_all_info->sample_prop_info->aud_s!= NULL)
        {
            free(st_all_info->sample_prop_info->aud_s);
        }
    if(st_all_info->sample_prop_info->vid_s!= NULL)
        {
            free(st_all_info->sample_prop_info->vid_s);
        }
    if(st_all_info->sample_prop_info!= NULL)
        {
            free(st_all_info->sample_prop_info);
        }

    if(st_all_info) {
	free(st_all_info);
	st_all_info = NULL;
    }
     return 1;
     }


 int32_t  getesds(all_info_t *st_all_info,int32_t trak_type,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* fid,int32_t *esds_offset)
 {
     box_t box;
     size_t bytes_read, total_bytes_read = 0;
     int32_t box_size,size = 10000;
     char esds_id[] = {'e', 's', 'd', 's'};
     int32_t esds_size;
     uint8_t *data, *ptr;

     data = NULL;
     ptr = 0;
     if (trak_type == AUDIO_TRAK)
	 {

	     ptr = data=(uint8_t*)malloc((st_all_info->stbl_info->AudioStbl.stsd_tab.size)*sizeof(uint8_t));
	     nkn_vfs_fseek(fid,(st_all_info->stbl_info->AudioStbl.stsd_tab.offset),SEEK_SET);
	     (*nkn_fread)(data,st_all_info->stbl_info->AudioStbl.stsd_tab.size,1,fid);
	     data+=52;
	     while(total_bytes_read < (size_t)st_all_info->stbl_info->AudioStbl.stsd_tab.size)
		 {
		     box_size = read_next_root_level_box(data, size, &box, &bytes_read);
		     total_bytes_read += box_size;
		     if(check_box_type(&box,esds_id))
			 {
			     *esds_offset = st_all_info->stbl_info->AudioStbl.stsd_tab.offset + 51;//box_size;
			     // stbl_data+=bytes_read;
			     esds_size = box_size;
			     data+=box_size;
			     goto cleanup;
			 }
		 }
	     *esds_offset = -1;
	     esds_size = -1;
	     
	 }

 cleanup:
     if(ptr) {
	 free(ptr);
	 ptr = NULL;
     }

     return esds_size;
 }
 int32_t  getavcc(all_info_t *st_all_info,int32_t trak_type,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* fid,int32_t *avcc_offset)
 {
     box_t box;
     size_t bytes_read, total_bytes_read = 0;
     int32_t box_size,size = 10000;//initial_data;
     char avc1_id[] = {'a', 'v', 'c', '1'};
     char avcC_id[] = {'a', 'v', 'c', 'C'};
     int32_t avc1_size,avcC_size;
     uint8_t *data,*initial_data;

     if (trak_type == VIDEO_TRAK)
	 {
	     data=(uint8_t*)malloc((st_all_info->stbl_info->VideoStbl.stsd_tab.size)*sizeof(uint8_t));
	     initial_data =data;
	     nkn_vfs_fseek(fid,(st_all_info->stbl_info->VideoStbl.stsd_tab.offset),SEEK_SET);
	     (*nkn_fread)(data,st_all_info->stbl_info->VideoStbl.stsd_tab.size,1,fid);
	     data+=16;
	     while(total_bytes_read < (size_t)st_all_info->stbl_info->VideoStbl.stsd_tab.size)
		 {
		     box_size = read_next_root_level_box(data, size, &box, &bytes_read);
		     total_bytes_read += box_size;
		     if(check_box_type(&box,avc1_id))
			 {
			     avc1_size = box_size;
			     data+=86;
			     box_size = read_next_root_level_box(data, size, &box, &bytes_read);
			     //total_bytes_read += box_size;
			     if(check_box_type(&box,avcC_id))
				 {
				     *avcc_offset = st_all_info->stbl_info->VideoStbl.stsd_tab.offset + (data - initial_data);
				     avcC_size = box_size;
				     data+=box_size;
				     if(data !=NULL)
					 free(data);
				     return avcC_size;
				 }

			 }
		 }
	     *avcc_offset = -1;
	     avcC_size = -1;

	 }
     if(data != NULL)
	 free(data);
     return avcC_size;
 }

int32_t Init(all_info_t* st_all_info, size_t (*nkn_fread)( void *, size_t, size_t, FILE*), FILE *fid){

    Seekinfo_t* sinfo = st_all_info->seek_state_info;
    Media_stbl_t *mstbl = st_all_info->stbl_info;
    send_ctx_t *st = st_all_info->sample_prop_info;
    int32_t ret;

    init_seek_info(sinfo);

    ret = send_next_pkt_init(sinfo,nkn_fread,fid,mstbl,st);

    if (st_all_info->stbl_info->HintVidStbl.stsz_tab.size != -1)
	{
	    st_all_info->timescale_video = st_all_info->stbl_info->HintVidStbl.timescale;
	}
    else 
	{
	    st_all_info->timescale_video = st_all_info->stbl_info->VideoStbl.timescale;
	}
    if (st_all_info->stbl_info->HintAudStbl.stsz_tab.size != -1)
	{
	    st_all_info->timescale_audio = st_all_info->stbl_info->HintAudStbl.timescale;
	}
    else
	{
	    st_all_info->timescale_audio = st_all_info->stbl_info->AudioStbl.timescale;
	}
    return ret;

}

void seek2time_hinted(all_info_t *st_all_info,int32_t seek_time,int32_t track_type,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* fid)
{
   
    Seek_params_t* seek_params1;
    Seekinfo_t* sinfo = st_all_info->seek_state_info;
    Media_stbl_t *mstbl = st_all_info->stbl_info;
    send_ctx_t *st = st_all_info->sample_prop_info;
    seek_params1 =  (Seek_params_t*)malloc(sizeof(Seek_params_t));
    seek_params1->seek_time = seek_time;
    //st=(send_ctx_t *)malloc(sizeof(send_ctx_t *));
    send_next_pkt_seek2time_hinted(sinfo,nkn_fread,fid,mstbl,st,track_type,seek_params1);
    return;
}


 void seek2time(all_info_t *st_all_info,int32_t seek_time,int32_t track_type,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* fid)
 {

     Seek_params_t* seek_params1;
     Seekinfo_t* sinfo = st_all_info->seek_state_info;
     Media_stbl_t *mstbl = st_all_info->stbl_info;
     send_ctx_t *st = st_all_info->sample_prop_info;
     seek_params1 =  (Seek_params_t*)malloc(sizeof(Seek_params_t));
     seek_params1->seek_time = seek_time;

     if(track_type == VIDEO_TRAK)
	 {
	     if(st_all_info->stbl_info->HintVidStbl.stsz_tab.size != -1)
		 {
		     send_next_pkt_seek2time_hinted(sinfo,nkn_fread,fid,mstbl,st,track_type,seek_params1);
		 }
	     else
		 {
                     send_next_pkt_seek2time(sinfo,nkn_fread,fid,mstbl,st,track_type,seek_params1);
                 }
	 }
     if(track_type == AUDIO_TRAK)
         {
             if(st_all_info->stbl_info->HintAudStbl.stsz_tab.size != -1)
                 {
                     send_next_pkt_seek2time_hinted(sinfo,nkn_fread,fid,mstbl,st,track_type,seek_params1);
                 }
             else
                 {
                     send_next_pkt_seek2time(sinfo,nkn_fread,fid,mstbl,st,track_type,seek_params1);
                 }
         }
     if(seek_params1 != NULL)
	 SAFE_FREE(seek_params1);

     return;
 }


 int32_t send_next_pkt_hinted(all_info_t *st_all_info,int32_t trak_type,size_t (*nkn_fread)( void *, size_t, size_t, FILE*, void **, int*),FILE* fid,Hint_sample_t *audiopkts, Hint_sample_t *videopkts )
{
    Seekinfo_t* sinfo = st_all_info->seek_state_info;
    Media_stbl_t *mstbl = st_all_info->stbl_info;
    //send_ctx_t **st = st_all_info->sample_prop_info;
    Media_sample_t* videoh = st_all_info->videoh;
    Media_sample_t* audioh = st_all_info->audioh;
    Media_sample_t* video = st_all_info->video;
    Media_sample_t* audio = st_all_info->audio;
    send_ctx_t *st = st_all_info->sample_prop_info;
    int32_t rv =0;

    rv = send_next_pkt_interface(sinfo,nkn_fread,fid,mstbl,st,videoh,audioh,trak_type);
    if (trak_type == VIDEO_TRAK)
	{
	    st_all_info->look_ahead_time = st_all_info->sample_prop_info->hvid_s->look_ahead_time ;
	}
    if (trak_type == AUDIO_TRAK)
        {
            st_all_info->look_ahead_time = st_all_info->sample_prop_info->haud_s->look_ahead_time ;
        }

    audiopkts = (Hint_sample_t *)st_all_info->audioh;
    videopkts = (Hint_sample_t *)st_all_info->videoh;
    return rv;

}
int32_t get_next_sample(all_info_t *st_all_info,int32_t trak_type,/*int32_t sample_num,*/size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* fid,int32_t *look_ahead_time , /*RTP_whole_pkt_t *rtp_pkt_data*/output_details_t *output )
{

    Seekinfo_t* sinfo = st_all_info->seek_state_info;
    Media_stbl_t *mstbl = st_all_info->stbl_info;
    //send_ctx_t **st = st_all_info->sample_prop_info;
    Media_sample_t* videoh = st_all_info->videoh;
    Media_sample_t* audioh = st_all_info->audioh;
    Media_sample_t* video = st_all_info->video;
    Media_sample_t* audio = st_all_info->audio;
    send_ctx_t *st = st_all_info->sample_prop_info;
    int32_t rv =0;

    rv = send_next_pkt_get_sample(sinfo,nkn_vpe_buffered_read,fid,mstbl,st,videoh,audioh,video,audio,trak_type);
    if (trak_type == AUDIO_HINT_TRAK)
	{

	    *output->rtp = *st_all_info->audioh->rtp;
	    output->num_bytes = st_all_info->seek_state_info->HintA.num_bytes[0];
	    output->offset = st_all_info->seek_state_info->HintA.offset[0];
	    output->num_samples_sent = st_all_info->seek_state_info->HintA.num_samples_sent;
	    *look_ahead_time = st_all_info->sample_prop_info->haud_s->look_ahead_time;

	}
    if (trak_type == VIDEO_HINT_TRAK)
	{
	    *output->rtp = *st_all_info->videoh->rtp;
	    output->num_bytes = st_all_info->seek_state_info->HintV.num_bytes[0];
	    output->offset = st_all_info->seek_state_info->HintV.offset[0];
	    output->num_samples_sent = st_all_info->seek_state_info->HintV.num_samples_sent;
	    *look_ahead_time = st_all_info->sample_prop_info->hvid_s->look_ahead_time;

	}
    if (trak_type == AUDIO_TRAK)
	{

	    output->rtp = NULL;
	    output->num_bytes = st_all_info->seek_state_info->Audio.num_bytes[0];
	    output->offset = st_all_info->seek_state_info->Audio.offset[0];
	    output->num_samples_sent = st_all_info->seek_state_info->Audio.num_samples_sent;
	    *look_ahead_time = st_all_info->sample_prop_info->aud_s->look_ahead_time;
	    st_all_info->look_ahead_time = st_all_info->sample_prop_info->aud_s->look_ahead_time ;
	}
    if (trak_type == VIDEO_TRAK)
	{

	    output->rtp = NULL;
	    output->num_bytes = st_all_info->seek_state_info->Video.num_bytes[0];
	    output->offset = st_all_info->seek_state_info->Video.offset[0];
	    output->num_samples_sent = st_all_info->seek_state_info->Video.num_samples_sent;
	    *look_ahead_time = st_all_info->sample_prop_info->vid_s->look_ahead_time;
	    st_all_info->look_ahead_time = st_all_info->sample_prop_info->vid_s->look_ahead_time ;
            output->ctts_offset =st_all_info->sample_prop_info->vid_s->ctts_offset ;

	}
    return rv;


}

#else//_SUMALATHA_OPT_MAIN_



/**************************************************************************
 *
 *                            API FUNCTIONS
 *
 **************************************************************************/
//#ifndef _USE_INTERFACE_
int32_t send_next_pkt(Seekinfo_t* seekinfo,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* fid,Media_stbl_t* media_stbl, send_ctx_t **ctx,Hint_sample_t * hint_video, Hint_sample_t * hint_audio, int32_t trak_type,int32_t mode, Seek_params_t* seek_params){
    //Note video,audio,hint, info etc.
    //Copy all the stsc,stsz, stco for all video and audio and hint
    int32_t first_instance = 1;
    uint8_t * data,*p_data;
    size_t size,bytes_read=0;
    box_t box;
    int32_t box_size;
    //Sampl_prop_t *vid_s, *aud_s,* hvid_s, *haud_s;
    send_ctx_t *st;
    int32_t ret, rv,offset;

    //variable initialization
    ret = 0;
    rv = 0;

    //init call, context SHOULD be NULL; this call builds the sample table for Audio,Video 
    //and their respective hint tracks
    if(!(*ctx)){
        //Collect the assets for audio video and hint traks.
        int64_t fpos;
        //Stbl_t *stbl;
        int32_t word=0;


        st = *ctx = (send_ctx_t*)malloc(sizeof(send_ctx_t));
        st->vid_s = (Sampl_prop_t*)calloc(1, sizeof(Sampl_prop_t));
        st->aud_s = (Sampl_prop_t*)calloc(1, sizeof(Sampl_prop_t));
        st->hvid_s = (Sampl_prop_t*)calloc(1, sizeof(Sampl_prop_t));
        st->haud_s = (Sampl_prop_t*)calloc(1, sizeof(Sampl_prop_t));

        st->vid_s->prev_base=0;
        st->aud_s->prev_base=0;
        st->hvid_s->prev_base=0;
        st->haud_s->prev_base=0;
        st->vid_s->size_offset=0;
        st->aud_s->size_offset=0;
        st->hvid_s->size_offset=0;
        st->haud_s->size_offset=0;
	st->haud_s->prev_sample_details.sample_num = 0;
        st->aud_s->prev_sample_details.sample_num = 0;
	st->haud_s->prev_sample_details.is_stateless = 0;
        st->aud_s->prev_sample_details.is_stateless = 0;
        st->hvid_s->prev_sample_details.sample_num = 0;
        st->vid_s->prev_sample_details.sample_num = 0;


        st->hvid_s->prev_sample_details.chunk_count = 0;
        st->vid_s->prev_sample_details.chunk_count = 0;
        st->haud_s->prev_sample_details.chunk_count = 0;
        st->aud_s->prev_sample_details.chunk_count = 0;
        st->hvid_s->prev_sample_details.prev_sample_total = 0;
        st->vid_s->prev_sample_details.prev_sample_total  = 0;
        st->haud_s->prev_sample_details.prev_sample_total = 0;
        st->aud_s->prev_sample_details.prev_sample_total  = 0;

        first_instance=0;
        box_size=0;
        data=(uint8_t*)malloc(8*sizeof(uint8_t));
        //stbl = (Stbl_t*)malloc(sizeof(Stbl_t));

	//search for the MOOV box and read it
        while(word!=MOOV_HEX_ID){
            nkn_vfs_fseek(fid,box_size,SEEK_SET);
            if((*nkn_fread)(data,8,1,fid) == 0)
		return NOMORE_SAMPLES;
            box_size=_read32(data,0);
            word=_read32(data,4);
        }

        //in moov position
        fpos=nkn_vfs_ftell(fid);
        fpos-=8;

	offset = (int32_t)(fpos);
	init_stbls(&media_stbl->HintVidStbl,offset);
	init_stbls(&media_stbl->HintAudStbl,offset);
	init_stbls(&media_stbl->VideoStbl,offset);
	init_stbls(&media_stbl->AudioStbl,offset);

        nkn_vfs_fseek(fid,fpos,SEEK_SET);
	if(data != NULL)
	    free(data);
        data=(uint8_t*)malloc(box_size*sizeof(uint8_t));
        if((*nkn_fread)(data,box_size,1,fid) == 0)
	    return NOMORE_SAMPLES;
        size=box_size;
        box_size = read_next_root_level_box(data, size, &box, &bytes_read);
        p_data=data;
	//build the Sample Tables;

	rv = get_Media_stbl_info(data,media_stbl,box_size);
	if(p_data != NULL)
	    free(p_data);
	if(media_stbl->HintVidStbl.stco_tab.size==-1&&media_stbl->VideoStbl.stco_tab.size>0)
            rv = MP4_PARSER_ERROR;
        if(media_stbl->HintAudStbl.stco_tab.size==-1&&media_stbl->AudioStbl.stco_tab.size>0)
            rv = MP4_PARSER_ERROR;
    } else {//parse and return offsets
	switch(trak_type){
	    case VIDEO_TRAK:
		if(seekinfo->HintV.num_samples_sent == 0 && media_stbl->HintVidStbl.stco == NULL) {
		    if(read_stbls(&media_stbl->HintVidStbl,nkn_fread,fid) == VFS_READ_ERROR)
			return NOMORE_SAMPLES;
		    if(read_stbls(&media_stbl->VideoStbl,nkn_fread,fid) == VFS_READ_ERROR)
			return NOMORE_SAMPLES;;
		}
		break;
	    case AUDIO_TRAK:
		if(seekinfo->HintA.num_samples_sent == 0 && media_stbl->HintAudStbl.stco == NULL) {
		    if(read_stbls(&media_stbl->HintAudStbl,nkn_fread,fid) == VFS_READ_ERROR)
			return NOMORE_SAMPLES;
		    if(read_stbls(&media_stbl->AudioStbl,nkn_fread,fid) == VFS_READ_ERROR)
			return NOMORE_SAMPLES;
		}
		break;
	}
        if(mode == _NKN_SEEK_){
            st = *ctx;
	    switch(trak_type){
		case VIDEO_TRAK:
                    // Get closest sync sample number:
                    //Get alter the states accordingly:st->hvid_s,st->vid_s
		    st->hvid_s->prev_sample_details.chunk_count = 0;
		    st->vid_s->prev_sample_details.chunk_count = 0;
		    st->hvid_s->prev_sample_details.prev_sample_total = 0;
		    st->vid_s->prev_sample_details.prev_sample_total  = 0;
		    if(st->hvid_s->total_sample_size==0){
                        st->hvid_s->total_sample_size = get_total_sample_size(media_stbl->HintVidStbl.stsz);
                        st->vid_s->total_sample_size = get_total_sample_size(media_stbl->VideoStbl.stsz);
                    }
                    seekinfo->HintV.num_samples_sent=seek_state_change(&media_stbl->HintVidStbl,seek_params->seek_time,media_stbl->HintVidStbl.timescale,st->hvid_s);
                    seekinfo->Video.num_samples_sent=seek_state_change(&media_stbl->VideoStbl,seek_params->seek_time,media_stbl->VideoStbl.timescale,st->vid_s);
		    st->hvid_s->look_ahead_time = get_time_offset(media_stbl->HintVidStbl.stts,seekinfo->HintV.num_samples_sent);
                    break;
		case AUDIO_TRAK:
		    st->haud_s->prev_sample_details.chunk_count = 0;
		    st->aud_s->prev_sample_details.chunk_count = 0;
		    st->haud_s->prev_sample_details.prev_sample_total = 0;
		    st->aud_s->prev_sample_details.prev_sample_total  = 0;
		    if(st->haud_s->total_sample_size==0){
                        st->haud_s->total_sample_size = get_total_sample_size(media_stbl->HintAudStbl.stsz);
                        st->aud_s->total_sample_size = get_total_sample_size(media_stbl->AudioStbl.stsz);
                    }

                    seekinfo->HintA.num_samples_sent=seek_state_change(&media_stbl->HintAudStbl,seek_params->seek_time,media_stbl->HintAudStbl.timescale,st->haud_s);

                    media_seek_state_change(&media_stbl->AudioStbl,st->aud_s,seekinfo->HintA.num_samples_sent);
		    seekinfo->Audio.num_samples_sent = seekinfo->HintA.num_samples_sent;//-=1;
		    //		    seekinfo->Audio.num_samples_sent-=1;seekinfo->HintA.num_samples_sent-=1;

		    st->haud_s->look_ahead_time = get_time_offset(media_stbl->HintAudStbl.stts,seekinfo->HintA.num_samples_sent);
                    break;
            }
        }
        else{
        int32_t i,j;
	uint8_t *dta;
        RTP_whole_pkt_t *rtp_header;
	int32_t sample_to_be_sent_aud, sample_to_be_sent_vid;
#ifdef PERF_DEBUG1
    struct timeval selfTimeStart, selfTimeEnd;
    int64_t skew;
#endif

	sample_to_be_sent_aud = sample_to_be_sent_vid = 0;
        st = *ctx;
#ifdef PERF_DEBUG1
    gettimeofday(&selfTimeStart, NULL); 
#endif

	switch(trak_type) {
	    case VIDEO_TRAK:
		if((seekinfo->HintV.num_samples_sent+SAMPLES_TO_BE_SENT<=st->hvid_s->total_sample_size)||(seekinfo->HintV.num_samples_sent==0)) {
		    ret=find_chunk_asset(seekinfo->HintV.num_bytes,seekinfo->HintV.offset,SAMPLES_TO_BE_SENT,seekinfo->HintV.num_samples_sent,&media_stbl->HintVidStbl,st->hvid_s);
		    ret=find_chunk_asset(seekinfo->Video.num_bytes,seekinfo->Video.offset,SAMPLES_TO_BE_SENT,seekinfo->Video.num_samples_sent,&media_stbl->VideoStbl,st->vid_s);
		    sample_to_be_sent_vid = SAMPLES_TO_BE_SENT;
		} 
		else {
		    if(st->hvid_s->total_sample_size - seekinfo->HintV.num_samples_sent<SAMPLES_TO_BE_SENT&&(st->hvid_s->total_sample_size - seekinfo->HintV.num_samples_sent >0)){
			sample_to_be_sent_vid = st->hvid_s->total_sample_size-seekinfo->HintV.num_samples_sent;
			ret=find_chunk_asset(seekinfo->HintV.num_bytes,seekinfo->HintV.offset,sample_to_be_sent_vid,seekinfo->HintV.num_samples_sent,&media_stbl->HintVidStbl,st->hvid_s);
			ret=find_chunk_asset(seekinfo->Video.num_bytes,seekinfo->Video.offset,sample_to_be_sent_vid,seekinfo->Video.num_samples_sent,&media_stbl->VideoStbl,st->vid_s);
		    } 
		    else if(st->hvid_s->total_sample_size == seekinfo->HintV.num_samples_sent) {
			rv = NOMORE_SAMPLES;
			goto cleanup;
		    }
		}
#ifdef PERF_DEBUG1
		{
		    struct timeval result1;
		    gettimeofday(&selfTimeEnd,NULL);
		    timeval_subtract1(&selfTimeEnd, &selfTimeStart, &result1);
		    //printf("TRAK TYPE %d, Sample Num %d, Time Elapsed %lu <---------> Expected Time %d ===== next DTS is  %d\n", psmss->trackType, psmss->sampleNumber, psmss->timeElapsed, psmss->lastDTS, psmss->nextDTS);
		    printf("TRAK TYPE %d, Self Time %ld\n", trak_type, (int64_t)((result1.tv_sec*1e6)+result1.tv_usec));
		}
#endif
		for(i=0;i<sample_to_be_sent_vid;i++) {
		    dta =(uint8_t*) malloc(seekinfo->HintV.num_bytes[i]);
		    nkn_vfs_fseek(fid,seekinfo->HintV.offset[i],SEEK_SET);
		    if((*nkn_fread)(dta,seekinfo->HintV.num_bytes[i],1,fid) == 0)
			return NOMORE_SAMPLES;
		    read_rtp_sample(dta,&hint_video[i],seekinfo->HintV.offset[i], seekinfo->Video.offset[i]);
		    //          (hint_video+i)->rtp->header.relative_time+= get_time_offset(media_stbl->HintVidStbl.stts,seekinfo->HintV.num_samples_sent+i);
		    for(j=0;j<(hint_video+i)->num_rtp_pkts;j++) {
			rtp_header = (hint_video+i)->rtp;
			rtp_header+=j;
			rtp_header->header.relative_time+= get_time_offset(media_stbl->HintVidStbl.stts,seekinfo->HintV.num_samples_sent+i) + rtp_header->time_stamp;
		    }
		    if(dta != NULL){
			free(dta);
			dta = NULL;
		    }
		}
		seekinfo->HintV.num_samples_sent+=sample_to_be_sent_vid;
		seekinfo->Video.num_samples_sent+=sample_to_be_sent_vid;
		
		break;
	    case AUDIO_TRAK:

		{
		    int uu = 0;
		    if(seekinfo->HintA.num_samples_sent == 4030) {
			uu = 1;
		    }
		}

		if((seekinfo->HintA.num_samples_sent+SAMPLES_TO_BE_SENT<=st->haud_s->total_sample_size)||(seekinfo->HintA.num_samples_sent==0)) {

		    ret=find_chunk_asset(seekinfo->HintA.num_bytes,seekinfo->HintA.offset,SAMPLES_TO_BE_SENT,seekinfo->HintA.num_samples_sent,&media_stbl->HintAudStbl,st->haud_s);
		    ret=find_chunk_asset(seekinfo->Audio.num_bytes,seekinfo->Audio.offset,SAMPLES_TO_BE_SENT,seekinfo->Audio.num_samples_sent,&media_stbl->AudioStbl,st->aud_s);
		    sample_to_be_sent_aud = SAMPLES_TO_BE_SENT;
		} else {
		    if(st->haud_s->total_sample_size - seekinfo->HintA.num_samples_sent<SAMPLES_TO_BE_SENT&&(st->haud_s->total_sample_size - seekinfo->HintA.num_samples_sent >0)){
			sample_to_be_sent_aud = st->haud_s->total_sample_size-seekinfo->HintA.num_samples_sent;
			ret=find_chunk_asset(seekinfo->HintA.num_bytes,seekinfo->HintA.offset,sample_to_be_sent_aud,seekinfo->HintA.num_samples_sent,&media_stbl->HintAudStbl,st->haud_s);
			ret=find_chunk_asset(seekinfo->Audio.num_bytes,seekinfo->Audio.offset,sample_to_be_sent_aud,seekinfo->Audio.num_samples_sent,&media_stbl->AudioStbl,st->aud_s);
		    } else if(st->haud_s->total_sample_size == seekinfo->HintA.num_samples_sent) {
			rv =  NOMORE_SAMPLES;
			goto cleanup;
		    }
		}

#ifdef PERF_DEBUG1    
    {
	struct timeval result1;
	gettimeofday(&selfTimeEnd,NULL);
	timeval_subtract1(&selfTimeEnd, &selfTimeStart, &result1);
	//printf("TRAK TYPE %d, Sample Num %d, Time Elapsed %lu <---------> Expected Time %d ===== next DTS is  %d\n", psmss->trackType, psmss->sampleNumber, psmss->timeElapsed, psmss->lastDTS, psmss->nextDTS);
	printf("TRAK TYPE %d, Self Time %ld\n", trak_type, (int64_t)((result1.tv_sec*1e6)+result1.tv_usec));
    }
#endif
		for(i=0;i<sample_to_be_sent_aud;i++) {
		    dta =(uint8_t*) malloc(seekinfo->HintA.num_bytes[i]);
		    nkn_vfs_fseek(fid,seekinfo->HintA.offset[i],SEEK_SET);
		    if((*nkn_fread)(dta,seekinfo->HintA.num_bytes[i],1,fid) == 0)
			return NOMORE_SAMPLES; 
		    //		    read_rtp_sample_audio(dta,&hint_audio[i],seekinfo->HintA.offset[i], seekinfo->Audio.offset[i], &media_stbl->HintAudStbl, &media_stbl->AudioStbl);
		    read_rtp_sample_audio(dta,&hint_audio[i],seekinfo->HintA.offset[i], seekinfo->Audio.offset[i], &media_stbl->HintAudStbl, &media_stbl->AudioStbl,&st->haud_s->prev_sample_details,&st->aud_s->prev_sample_details);

		    for(j=0;j<(hint_audio+i)->num_rtp_pkts;j++) {
			//int32_t time_temp;
			rtp_header = (hint_audio+i)->rtp;
			rtp_header+=j;
			rtp_header->header.relative_time+= get_time_offset(media_stbl->HintAudStbl.stts,seekinfo->HintA.num_samples_sent+i) + rtp_header->time_stamp;
		    }
		    if(dta != NULL){
			free(dta);
			dta = NULL;
		    }
		}

		seekinfo->HintA.num_samples_sent+=sample_to_be_sent_aud;
		seekinfo->Audio.num_samples_sent+=sample_to_be_sent_aud;
		break;
		/*CHANGES CHANGES HERE*/
	}

	
	if(trak_type == AUDIO_TRAK){
	    st->haud_s->look_ahead_time = get_time_offset(media_stbl->HintAudStbl.stts,seekinfo->HintA.num_samples_sent+1);
	}
	else
	    st->hvid_s->look_ahead_time = get_time_offset(media_stbl->HintVidStbl.stts,seekinfo->HintV.num_samples_sent+1);
	goto cleanup;
    }

	//printf("\n\t\tSAMple number =  %d\n",seekinfo->HintV.num_samples_sent);
    }
    printf("total = %d Samples number %d\n\n",st->hvid_s->total_sample_size,seekinfo->HintV.num_samples_sent);
 cleanup:
    return rv;
}

int32_t send_next_pkt_interface(Seekinfo_t* seekinfo,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),size_t (*nkn_fread1)( void *, size_t, size_t, FILE*, void **, int *), FILE* fid,Media_stbl_t* media_stbl, send_ctx_t *st,Media_sample_t * hint_video, Media_sample_t * hint_audio, int32_t trak_type){

    //Note video,audio,hint, info etc.
    //Copy all the stsc,stsz, stco for all video and audio and hint

    int32_t ret, rv;

    //variable initialization
    ret = 0;
    rv = 0;
#if 0
	switch(trak_type){
	    case VIDEO_TRAK:
		if(seekinfo->HintV.num_samples_sent == 0 && media_stbl->HintVidStbl.stco == NULL) {
		    if(read_stbls(&media_stbl->HintVidStbl,nkn_fread,fid) == VFS_READ_ERROR)
			return NOMORE_SAMPLES;
		    if(read_stbls(&media_stbl->VideoStbl,nkn_fread,fid) == VFS_READ_ERROR)
			return NOMORE_SAMPLES;
		}
		break;
	    case AUDIO_TRAK:
		if(seekinfo->HintA.num_samples_sent == 0 && media_stbl->HintAudStbl.stco == NULL) {
		    if(read_stbls(&media_stbl->HintAudStbl,nkn_fread,fid) == VFS_READ_ERROR)
			return NOMORE_SAMPLES;
		    if(read_stbls(&media_stbl->AudioStbl,nkn_fread,fid) == VFS_READ_ERROR)
			return NOMORE_SAMPLES;
		}
		break;
	}
#endif
        int32_t i,j;
	uint8_t *dta;
        RTP_whole_pkt_t *rtp_header;
	int32_t sample_to_be_sent_aud, sample_to_be_sent_vid;
#if 0//def PERF_DEBUG1
    struct timeval selfTimeStart, selfTimeEnd;
    int64_t skew;
#endif

	sample_to_be_sent_aud = sample_to_be_sent_vid = 0;
        //st = *ctx;
#if 0//def PERF_DEBUG1
    gettimeofday(&selfTimeStart, NULL); 
#endif

	switch(trak_type) {
	    case VIDEO_TRAK:
		if((seekinfo->HintV.num_samples_sent+SAMPLES_TO_BE_SENT<=st->hvid_s->total_sample_size)||(seekinfo->HintV.num_samples_sent==0)) {
		    ret=find_chunk_asset(seekinfo->HintV.num_bytes,seekinfo->HintV.offset,SAMPLES_TO_BE_SENT,seekinfo->HintV.num_samples_sent,&media_stbl->HintVidStbl,st->hvid_s);
		    ret=find_chunk_asset(seekinfo->Video.num_bytes,seekinfo->Video.offset,SAMPLES_TO_BE_SENT,seekinfo->Video.num_samples_sent,&media_stbl->VideoStbl,st->vid_s);
		    sample_to_be_sent_vid = SAMPLES_TO_BE_SENT;
		} else {
		    if(st->hvid_s->total_sample_size - seekinfo->HintV.num_samples_sent<SAMPLES_TO_BE_SENT&&(st->hvid_s->total_sample_size - seekinfo->HintV.num_samples_sent >0)){
			sample_to_be_sent_vid = st->hvid_s->total_sample_size-seekinfo->HintV.num_samples_sent;

			ret=find_chunk_asset(seekinfo->HintV.num_bytes,seekinfo->HintV.offset,sample_to_be_sent_vid,seekinfo->HintV.num_samples_sent,&media_stbl->HintVidStbl,st->hvid_s);
			ret=find_chunk_asset(seekinfo->Video.num_bytes,seekinfo->Video.offset,sample_to_be_sent_vid,seekinfo->Video.num_samples_sent,&media_stbl->VideoStbl,st->vid_s);

		    } else if(st->hvid_s->total_sample_size == seekinfo->HintV.num_samples_sent) {
			rv = NOMORE_SAMPLES;
			goto cleanup;
		    }
		}
#ifdef PERF_DEBUG1
		{
		    struct timeval result1;
		    gettimeofday(&selfTimeEnd,NULL);
		    timeval_subtract1(&selfTimeEnd, &selfTimeStart, &result1);
		    //printf("TRAK TYPE %d, Sample Num %d, Time Elapsed %lu <---------> Expected Time %d ===== next DTS is  %d\n", psmss->trackType, psmss->sampleNumber, psmss->timeElapsed, psmss->lastDTS, psmss->nextDTS);
		    printf("TRAK TYPE %d, Self Time %ld\n", trak_type, (int64_t)((result1.tv_sec*1e6)+result1.tv_usec));
		}
#endif
		uint8_t *temp;
		int free_buf;
		for(i=0;i<sample_to_be_sent_vid;i++) {
		    dta =(uint8_t*) malloc(seekinfo->HintV.num_bytes[i]);
		    nkn_vfs_fseek(fid,seekinfo->HintV.offset[i],SEEK_SET);
		    if((*nkn_fread1)(dta,seekinfo->HintV.num_bytes[i],1,fid,(void **) &temp, &free_buf) == 0){
                        rv = NOMORE_SAMPLES;
                        goto cleanup;
		    }
		    if (!free_buf) {
			free(dta);
			dta = temp;
		    }
#ifdef _MP4Box_
		    read_rtp_sample_interface(dta,&hint_video[i],seekinfo->HintV.offset[i],seekinfo->Video.offset[i],&media_stbl->HintVidStbl,&media_stbl->VideoStbl,seekinfo->\
					      HintV.num_samples_sent);
#else
		    read_rtp_sample_interface(dta,&hint_video[i],seekinfo->HintV.offset[i], seekinfo->Video.offset[i]);
#endif


		    //		    read_rtp_sample_interface(dta,&hint_video[i],seekinfo->HintV.offset[i], seekinfo->Video.offset[i]);



		    for(j=0;j<(hint_video+i)->num_rtp_pkts;j++) {
			rtp_header = (hint_video+i)->rtp;
			rtp_header+=j;
			rtp_header->header.relative_time+= get_time_offset(media_stbl->HintVidStbl.stts,seekinfo->HintV.num_samples_sent+i) + rtp_header->time_stamp;
		    }
		    if (free_buf)
		    	free(dta);
		    dta = NULL;
		}
		seekinfo->HintV.num_samples_sent+=sample_to_be_sent_vid;
		seekinfo->Video.num_samples_sent+=sample_to_be_sent_vid;
		
		break;
	    case AUDIO_TRAK:
		if((seekinfo->HintA.num_samples_sent+SAMPLES_TO_BE_SENT<=st->haud_s->total_sample_size)||(seekinfo->HintA.num_samples_sent==0)) {
		    ret=find_chunk_asset(seekinfo->HintA.num_bytes,seekinfo->HintA.offset,SAMPLES_TO_BE_SENT,seekinfo->HintA.num_samples_sent,&media_stbl->HintAudStbl,st->haud_s);
		    ret=find_chunk_asset(seekinfo->Audio.num_bytes,seekinfo->Audio.offset,SAMPLES_TO_BE_SENT,seekinfo->Audio.num_samples_sent,&media_stbl->AudioStbl,st->aud_s);
		    sample_to_be_sent_aud = SAMPLES_TO_BE_SENT;
		} else {
		    if(st->haud_s->total_sample_size - seekinfo->HintA.num_samples_sent<SAMPLES_TO_BE_SENT&&(st->haud_s->total_sample_size - seekinfo->HintA.num_samples_sent >0)){
			sample_to_be_sent_aud = st->haud_s->total_sample_size-seekinfo->HintA.num_samples_sent;
			ret=find_chunk_asset(seekinfo->HintA.num_bytes,seekinfo->HintA.offset,sample_to_be_sent_aud,seekinfo->HintA.num_samples_sent,&media_stbl->HintAudStbl,st->haud_s);
			ret=find_chunk_asset(seekinfo->Audio.num_bytes,seekinfo->Audio.offset,sample_to_be_sent_aud,seekinfo->Audio.num_samples_sent,&media_stbl->AudioStbl,st->aud_s);
		    } else if(st->haud_s->total_sample_size == seekinfo->HintA.num_samples_sent) {
			rv =  NOMORE_SAMPLES;
			goto cleanup;
		    }
		}

#ifdef PERF_DEBUG1    
    {
	struct timeval result1;
	gettimeofday(&selfTimeEnd,NULL);
	timeval_subtract1(&selfTimeEnd, &selfTimeStart, &result1);
	//printf("TRAK TYPE %d, Sample Num %d, Time Elapsed %lu <---------> Expected Time %d ===== next DTS is  %d\n", psmss->trackType, psmss->sampleNumber, psmss->timeElapsed, psmss->lastDTS, psmss->nextDTS);
	printf("TRAK TYPE %d, Self Time %ld\n", trak_type, (int64_t)((result1.tv_sec*1e6)+result1.tv_usec));
    }
#endif
		for(i=0;i<sample_to_be_sent_aud;i++) {
		    dta =(uint8_t*) malloc(seekinfo->HintA.num_bytes[i]);
		    nkn_vfs_fseek(fid,seekinfo->HintA.offset[i],SEEK_SET);
		    if((*nkn_fread1)(dta,seekinfo->HintA.num_bytes[i],1,fid, (void **)&temp, &free_buf) == 0){
			rv = NOMORE_SAMPLES;
                        goto cleanup;
		    }
		    if (!free_buf) {
			free(dta);
			dta = temp;
		    }
		    read_rtp_sample_audio_interface(dta,&hint_audio[i],seekinfo->HintA.offset[i], seekinfo->Audio.offset[i], &media_stbl->HintAudStbl, &media_stbl->AudioStbl,&st->haud_s->prev_sample_details,&st->aud_s->prev_sample_details);
		    for(j=0;j<(hint_audio+i)->num_rtp_pkts;j++) {
			rtp_header = (hint_audio+i)->rtp;
			rtp_header+=j;
			rtp_header->header.relative_time+= get_time_offset(media_stbl->HintAudStbl.stts,seekinfo->HintA.num_samples_sent+i) + rtp_header->time_stamp;
		    }
		    if (free_buf) 
			free(dta);
		    dta = NULL;
		}

		seekinfo->HintA.num_samples_sent+=sample_to_be_sent_aud;
		seekinfo->Audio.num_samples_sent+=sample_to_be_sent_aud;
		break;
		/*CHANGES CHANGES HERE*/
	}

	
	if(trak_type == AUDIO_TRAK){
	    st->haud_s->look_ahead_time = get_time_offset(media_stbl->HintAudStbl.stts,seekinfo->HintA.num_samples_sent+1);
	}
	else{
	    st->hvid_s->look_ahead_time = get_time_offset(media_stbl->HintVidStbl.stts,seekinfo->HintV.num_samples_sent+1);
	}
	goto cleanup;

 cleanup:
    return rv;
}


int32_t  send_next_pkt_init(Seekinfo_t* seekinfo,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* fid,Media_stbl_t*  media_stbl, send_ctx_t *st){


	 //Note video,audio,hint, info etc.
	 //Copy all the stsc,stsz, stco for all video and audio and hint
	
	 uint8_t * data,*p_data;
	 size_t size,bytes_read=0;
	 box_t box;
	 int32_t box_size;
	 int32_t  rv,offset;
	 uint32_t temp_box_size;
	 //variable initialization
	 rv = 0;

	 //init call, context SHOULD be NULL; this call builds the sample table for Audio,Video
	 //and their respective hint tracks
 
	     //Collect the assets for audio video and hint traks.
	     int64_t fpos;
	     //Stbl_t *stbl;
	     int32_t word=0;
	     st->vid_s->prev_base=0;
	     st->aud_s->prev_base=0;
	     st->hvid_s->prev_base=0;
	     st->haud_s->prev_base=0;
	     st->vid_s->size_offset=0;
	     st->aud_s->size_offset=0;
	     st->hvid_s->size_offset=0;
	     st->haud_s->size_offset=0;
	     st->haud_s->prev_sample_details.sample_num = 0;
	     st->aud_s->prev_sample_details.sample_num = 0;
	     st->hvid_s->prev_sample_details.sample_num = 0;
	     st->vid_s->prev_sample_details.sample_num = 0;
	     st->haud_s->prev_sample_details.is_stateless = 0;
	     st->aud_s->prev_sample_details.is_stateless = 0;
	     st->hvid_s->prev_sample_details.is_stateless = 0;
	     st->vid_s->prev_sample_details.is_stateless = 0;


	     st->hvid_s->prev_sample_details.chunk_count = 0;
	     st->vid_s->prev_sample_details.chunk_count = 0;
	     st->haud_s->prev_sample_details.chunk_count = 0;
	     st->aud_s->prev_sample_details.chunk_count = 0;
	     st->hvid_s->prev_sample_details.prev_sample_total = 0;
	     st->vid_s->prev_sample_details.prev_sample_total  = 0;
	     st->haud_s->prev_sample_details.prev_sample_total = 0;
	     st->aud_s->prev_sample_details.prev_sample_total  = 0;

	     //first_instance=0;
	     box_size=0;
	     data=(uint8_t*)malloc(8*sizeof(uint8_t));
	     //stbl = (Stbl_t*)malloc(sizeof(Stbl_t));

	     //search for the MOOV box and read it
	     while(word!=MOOV_HEX_ID){
		 nkn_vfs_fseek(fid,box_size,SEEK_SET);
		 if((*nkn_fread)(data,8,1,fid) == 0)
		     return MP4_PARSER_ERROR;
		 temp_box_size = _read32(data,0);
		 box_size+=temp_box_size;
		 word=_read32(data,4);
	     }
	     //in moov position
	     fpos=nkn_vfs_ftell(fid);
	     fpos-=8;
	     box_size = temp_box_size;
	     offset = (int32_t)(fpos);
	     init_stbls(&media_stbl->HintVidStbl,offset);
	     init_stbls(&media_stbl->HintAudStbl,offset);
	     init_stbls(&media_stbl->VideoStbl,offset);
	     init_stbls(&media_stbl->AudioStbl,offset);
	     nkn_vfs_fseek(fid,fpos,SEEK_SET);
	     if(data !=NULL)
		 free(data);
	     data=(uint8_t*)malloc(box_size*sizeof(uint8_t));
	     if((*nkn_fread)(data,box_size,1,fid) == 0)
		 return MP4_PARSER_ERROR;
	     size=box_size;
	     box_size = read_next_root_level_box(data, size, &box, &bytes_read);
	     p_data=data;
	     //build the Sample Tables;
	     rv = get_Media_stbl_info(data,media_stbl,box_size);
	     if(p_data != NULL)
		 free(p_data);
	     if(media_stbl->HintVidStbl.stco_tab.size==-1&&media_stbl->VideoStbl.stco_tab.size>0)
		 rv = MP4_PARSER_ERROR;
	     if(media_stbl->HintAudStbl.stco_tab.size==-1&&media_stbl->AudioStbl.stco_tab.size>0)
		 rv = MP4_PARSER_ERROR;
	  return rv;
 }

int32_t send_next_pkt_seek2time(Seekinfo_t* seekinfo,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* fid,Media_stbl_t* media_stbl, send_ctx_t *st, int32_t trak_type, Seek_params_t* seek_params){

    //Note video,audio,hint, info etc.
    //Copy all the stsc,stsz, stco for all video and audio and hint
    //uint8_t * data,*p_data;
    //size_t size,bytes_read=0;
    //box_t box;
    //int32_t box_size;
    //Sampl_prop_t *vid_s, *aud_s,* hvid_s, *haud_s;
    //send_ctx_t *st;
    int32_t ret, rv;

    //variable initialization
    ret = 0;
    rv = 0;

    switch(trak_type){
	case VIDEO_HINT_TRAK:
	    if(seekinfo->HintV.num_samples_sent == 0 && media_stbl->HintVidStbl.stco == NULL) {
		read_stbls(&media_stbl->HintVidStbl,nkn_fread,fid);
		read_stbls(&media_stbl->VideoStbl,nkn_fread,fid);
	    }
	    break;
	case AUDIO_HINT_TRAK:
	    if(seekinfo->HintA.num_samples_sent == 0 && media_stbl->HintAudStbl.stco == NULL) {
		read_stbls(&media_stbl->HintAudStbl,nkn_fread,fid);
		read_stbls(&media_stbl->AudioStbl,nkn_fread,fid);
	    }
	    break;
	case VIDEO_TRAK:
	    if(seekinfo->Video.num_samples_sent == 0 && media_stbl->VideoStbl.stco == NULL) {
		read_stbls(&media_stbl->VideoStbl,nkn_fread,fid);
	    }
	    break;
	case AUDIO_TRAK:
	    if(seekinfo->Audio.num_samples_sent == 0 && media_stbl->AudioStbl.stco == NULL) {
		read_stbls(&media_stbl->AudioStbl,nkn_fread,fid);
	    }
	    break;

    }

    	switch(trak_type){
	    case VIDEO_HINT_TRAK:
		// Get closest sync sample number:
		//Get alter the states accordingly:st->hvid_s,st->vid_s
		st->hvid_s->prev_sample_details.chunk_count = 0;
		st->vid_s->prev_sample_details.chunk_count = 0;
		st->hvid_s->prev_sample_details.prev_sample_total = 0;
		st->vid_s->prev_sample_details.prev_sample_total  = 0;
		if(st->hvid_s->total_sample_size==0){
		    st->hvid_s->total_sample_size = get_total_sample_size(media_stbl->HintVidStbl.stsz);
		    st->vid_s->total_sample_size = get_total_sample_size(media_stbl->VideoStbl.stsz);
		}
		seekinfo->HintV.num_samples_sent=seek_state_change(&media_stbl->HintVidStbl,seek_params->seek_time,media_stbl->HintVidStbl.timescale,st->hvid_s);
		seekinfo->Video.num_samples_sent=seek_state_change(&media_stbl->VideoStbl,seek_params->seek_time,media_stbl->VideoStbl.timescale,st->vid_s);
		st->hvid_s->look_ahead_time = get_time_offset(media_stbl->HintVidStbl.stts,seekinfo->HintV.num_samples_sent);
		break;
	    case AUDIO_HINT_TRAK:
		st->haud_s->prev_sample_details.chunk_count = 0;
		st->aud_s->prev_sample_details.chunk_count = 0;
		st->haud_s->prev_sample_details.prev_sample_total = 0;
		st->aud_s->prev_sample_details.prev_sample_total  = 0;
		if(st->haud_s->total_sample_size==0){
		    st->haud_s->total_sample_size = get_total_sample_size(media_stbl->HintAudStbl.stsz);
		    st->aud_s->total_sample_size = get_total_sample_size(media_stbl->AudioStbl.stsz);
		}
		seekinfo->HintA.num_samples_sent=seek_state_change(&media_stbl->HintAudStbl,seek_params->seek_time,media_stbl->HintAudStbl.timescale,st->haud_s);
		media_seek_state_change(&media_stbl->AudioStbl,st->aud_s,seekinfo->HintA.num_samples_sent);
		seekinfo->Audio.num_samples_sent = seekinfo->HintA.num_samples_sent;//-=1;
		//              seekinfo->Audio.num_samples_sent-=1;seekinfo->HintA.num_samples_sent-=1;

		st->haud_s->look_ahead_time = get_time_offset(media_stbl->HintAudStbl.stts,seekinfo->HintA.num_samples_sent);
		break;
	    case VIDEO_TRAK:
		// Get closest sync sample number:
		//Get alter the states accordingly:st->hvid_s,st->vid_s
		st->vid_s->prev_sample_details.chunk_count = 0;
		st->vid_s->prev_sample_details.prev_sample_total  = 0;

#ifdef NK_CTTS_OPT
                st->vid_s->prev_ctts.prev_ctts_entry = 0;
                st->vid_s->prev_ctts.prev_cum_sample_cnt = 0;
#endif

		if(st->vid_s->total_sample_size==0){
		    st->vid_s->total_sample_size = get_total_sample_size(media_stbl->VideoStbl.stsz);
		}
		seekinfo->Video.num_samples_sent=seek_state_change(&media_stbl->VideoStbl,seek_params->seek_time,media_stbl->VideoStbl.timescale,st->vid_s);
		st->vid_s->look_ahead_time = get_time_offset(media_stbl->VideoStbl.stts,seekinfo->Video.num_samples_sent);
		break;
	    case AUDIO_TRAK:
		st->aud_s->prev_sample_details.chunk_count = 0;
		st->aud_s->prev_sample_details.prev_sample_total  = 0;

#ifdef NK_CTTS_OPT
                st->aud_s->prev_ctts.prev_ctts_entry = 0;
                st->aud_s->prev_ctts.prev_cum_sample_cnt = 0;
#endif

		if(st->aud_s->total_sample_size==0){
		    st->aud_s->total_sample_size = get_total_sample_size(media_stbl->AudioStbl.stsz);
		}
		seekinfo->Audio.num_samples_sent=seek_state_change(&media_stbl->AudioStbl,seek_params->seek_time,media_stbl->AudioStbl.timescale,st->aud_s);
		//                    media_seek_state_change(&media_stbl->AudioStbl,st->aud_s,seekinfo->HintA.num_samples_sent);
		//seekinfo->Audio.num_samples_sent = seekinfo->HintA.num_samples_sent;//-=1;//by suma
		//              seekinfo->Audio.num_samples_sent-=1;seekinfo->HintA.num_samples_sent-=1;
		st->aud_s->look_ahead_time = get_time_offset(media_stbl->AudioStbl.stts,seekinfo->Audio.num_samples_sent);
		break;
	}

    return rv;

     }

int32_t send_next_pkt_seek2time_hinted(Seekinfo_t* seekinfo,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* fid,Media_stbl_t* media_stbl, send_ctx_t *st, int32_t trak_type, Seek_params_t* seek_params){

    //Note video,audio,hint, info etc.
    //Copy all the stsc,stsz, stco for all video and audio and hint
    //uint8_t * data,*p_data;
    //size_t size,bytes_read=0;
    //box_t box;
    //int32_t box_size;
    //Sampl_prop_t *vid_s, *aud_s,* hvid_s, *haud_s;
    //send_ctx_t *st;
    int32_t ret, rv;

    //variable initialization
    ret = 0;
    rv = 0;

    switch(trak_type){
	case VIDEO_TRAK:
	    if(seekinfo->HintV.num_samples_sent == 0 && media_stbl->HintVidStbl.stco == NULL) {
		read_stbls(&media_stbl->HintVidStbl,nkn_fread,fid);
		read_stbls(&media_stbl->VideoStbl,nkn_fread,fid);
	    }
	    break;
	case AUDIO_TRAK:
	    if(seekinfo->HintA.num_samples_sent == 0 && media_stbl->HintAudStbl.stco == NULL) {
		read_stbls(&media_stbl->HintAudStbl,nkn_fread,fid);
		read_stbls(&media_stbl->AudioStbl,nkn_fread,fid);
	    }
	    break;
    }

    	switch(trak_type){
	    case VIDEO_TRAK:
		// Get closest sync sample number:
		//Get alter the states accordingly:st->hvid_s,st->vid_s
		st->hvid_s->prev_sample_details.chunk_count = 0;
		st->vid_s->prev_sample_details.chunk_count = 0;
		st->hvid_s->prev_sample_details.prev_sample_total = 0;
		st->vid_s->prev_sample_details.prev_sample_total  = 0;
		if(st->hvid_s->total_sample_size==0){
		    st->hvid_s->total_sample_size = get_total_sample_size(media_stbl->HintVidStbl.stsz);
		    st->vid_s->total_sample_size = get_total_sample_size(media_stbl->VideoStbl.stsz);
		}
		seekinfo->HintV.num_samples_sent=seek_state_change(&media_stbl->HintVidStbl,seek_params->seek_time,media_stbl->HintVidStbl.timescale,st->hvid_s);
		seekinfo->Video.num_samples_sent=seek_state_change(&media_stbl->VideoStbl,seek_params->seek_time,media_stbl->VideoStbl.timescale,st->vid_s);
		st->hvid_s->look_ahead_time = get_time_offset(media_stbl->HintVidStbl.stts,seekinfo->HintV.num_samples_sent);
		break;
	    case AUDIO_TRAK:
		st->haud_s->prev_sample_details.chunk_count = 0;
		st->aud_s->prev_sample_details.chunk_count = 0;
		st->haud_s->prev_sample_details.prev_sample_total = 0;
		st->aud_s->prev_sample_details.prev_sample_total  = 0;
		if(st->haud_s->total_sample_size==0){
		    st->haud_s->total_sample_size = get_total_sample_size(media_stbl->HintAudStbl.stsz);
		    st->aud_s->total_sample_size = get_total_sample_size(media_stbl->AudioStbl.stsz);
		}

		seekinfo->HintA.num_samples_sent=seek_state_change(&media_stbl->HintAudStbl,seek_params->seek_time,media_stbl->HintAudStbl.timescale,st->haud_s);
		media_seek_state_change(&media_stbl->AudioStbl,st->aud_s,seekinfo->HintA.num_samples_sent);

		seekinfo->Audio.num_samples_sent = seekinfo->HintA.num_samples_sent;//-=1;
		//              seekinfo->Audio.num_samples_sent-=1;seekinfo->HintA.num_samples_sent-=1;

		st->haud_s->look_ahead_time = get_time_offset(media_stbl->HintAudStbl.stts,seekinfo->HintA.num_samples_sent);
		break;
	}

    return rv;

     }


int32_t send_next_pkt_get_sample(Seekinfo_t* seekinfo,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* fid,Media_stbl_t*  media_stbl, send_ctx_t *st,Media_sample_t * hint_video, Media_sample_t * hint_audio, Media_sample_t * video, Media_sample_t * audio, int32_t trak_type){

	 //Note video,audio,hint, info etc.
	 //Copy all the stsc,stsz, stco for all video and audio and hint

	 //uint8_t * data,*p_data;
	 //size_t size,bytes_read=0;
	 //box_t box;
	 //int32_t box_size;
	 //Sampl_prop_t *vid_s, *aud_s,* hvid_s, *haud_s;
	 //send_ctx_t *st;
	 int32_t ret, rv;

	 //variable initialization
	 ret = 0;
	 rv = 0;

	 //init call, context SHOULD be NULL; this call builds the sample table for Audio,Video
	 //and their respective hint tracks
	 switch(trak_type){
	     case VIDEO_HINT_TRAK:
		 if(seekinfo->HintV.num_samples_sent == 0 && media_stbl->HintVidStbl.stco == NULL) {
		     if(read_stbls(&media_stbl->HintVidStbl,nkn_fread,fid) == VFS_READ_ERROR){
			 rv =  NOMORE_SAMPLES;
			 goto cleanup;
		     }
		     if(read_stbls(&media_stbl->VideoStbl,nkn_fread,fid) == VFS_READ_ERROR){
                         rv =  NOMORE_SAMPLES;
                         goto cleanup;
		     }
		 }
		 break;
	     case AUDIO_HINT_TRAK:
		 if(seekinfo->HintA.num_samples_sent == 0 && media_stbl->HintAudStbl.stco == NULL) {
		     if(read_stbls(&media_stbl->HintAudStbl,nkn_fread,fid) == VFS_READ_ERROR){
                         rv =  NOMORE_SAMPLES;
                         goto cleanup;
                     }
		     if(read_stbls(&media_stbl->AudioStbl,nkn_fread,fid)== VFS_READ_ERROR){
                         rv =  NOMORE_SAMPLES;
                         goto cleanup;
                     }
		 }
		 break;
	     case VIDEO_TRAK:
		 if(seekinfo->Video.num_samples_sent == 0 && media_stbl->VideoStbl.stco == NULL) {
		     if(read_stbls(&media_stbl->VideoStbl,nkn_fread,fid)== VFS_READ_ERROR){
                         rv =  NOMORE_SAMPLES;
                         goto cleanup;
                     }

		 }
		 break;
	     case AUDIO_TRAK:
		 if(seekinfo->Audio.num_samples_sent == 0 && media_stbl->AudioStbl.stco == NULL) {
		     if(read_stbls(&media_stbl->AudioStbl,nkn_fread,fid) == VFS_READ_ERROR){
                         rv =  NOMORE_SAMPLES;
                         goto cleanup;
                     }
		 }
		 break;
	 }

	 int32_t i,j;//,offset;
	 uint8_t *dta;
	 RTP_whole_pkt_t *rtp_header;
	 int32_t sample_to_be_sent_aud, sample_to_be_sent_vid;
#ifdef PERF_DEBUG1
	 struct timeval selfTimeStart, selfTimeEnd;
	 int64_t skew;
#endif

	 sample_to_be_sent_aud = sample_to_be_sent_vid = 0;
	 //st = *ctx;// by suma
#ifdef PERF_DEBUG1
	 gettimeofday(&selfTimeStart, NULL);
#endif
	 switch(trak_type) {
	     case VIDEO_HINT_TRAK:
		 if((seekinfo->HintV.num_samples_sent+SAMPLES_TO_BE_SENT<=st->hvid_s->total_sample_size)||(seekinfo->HintV.num_samples_sent==0)) {

		     ret=find_chunk_asset(seekinfo->HintV.num_bytes,seekinfo->HintV.offset,SAMPLES_TO_BE_SENT,seekinfo->HintV.num_samples_sent,&media_stbl->HintVidStbl,st->hvid_s);
		     ret=find_chunk_asset(seekinfo->Video.num_bytes,seekinfo->Video.offset,SAMPLES_TO_BE_SENT,seekinfo->Video.num_samples_sent,&media_stbl->VideoStbl,st->vid_s);
		     sample_to_be_sent_vid = SAMPLES_TO_BE_SENT;
		 } else {
		     if(st->hvid_s->total_sample_size - seekinfo->HintV.num_samples_sent<SAMPLES_TO_BE_SENT&&(st->hvid_s->total_sample_size - seekinfo->HintV.num_samples_sent > 0)){
			 sample_to_be_sent_vid = st->hvid_s->total_sample_size-seekinfo->HintV.num_samples_sent;

			 ret=find_chunk_asset(seekinfo->HintV.num_bytes,seekinfo->HintV.offset,sample_to_be_sent_vid,seekinfo->HintV.num_samples_sent,&media_stbl->HintVidStbl,st->hvid_s);
			 ret=find_chunk_asset(seekinfo->Video.num_bytes,seekinfo->Video.offset,sample_to_be_sent_vid,seekinfo->Video.num_samples_sent,&media_stbl->VideoStbl,st->vid_s);
		     } else if(st->hvid_s->total_sample_size == seekinfo->HintV.num_samples_sent) {
			 rv = NOMORE_SAMPLES;
			 goto cleanup;
		     }
		     printf("\n\nSamples Sent = %d\n ",seekinfo->HintV.num_samples_sent);
		 }
#ifdef PERF_DEBUG1
		 {
		     struct timeval result1;
		     gettimeofday(&selfTimeEnd,NULL);
		     timeval_subtract1(&selfTimeEnd, &selfTimeStart, &result1);
		     //printf("TRAK TYPE %d, Sample Num %d, Time Elapsed %lu <---------> Expected Time %d ===== next DTS is  %d\n", psmss->trackType, psmss->sampleNumber, psmss->timeElapsed, psmss->lastDTS, psmss->nextDTS);
		 printf("TRAK TYPE %d, Self Time %ld\n", trak_type, (int64_t)((result1.tv_sec*1e6)+result1.tv_usec));
	 }
#endif
	 for(i=0;i<sample_to_be_sent_vid;i++) {
	     dta =(uint8_t*) malloc(seekinfo->HintV.num_bytes[i]);
	     nkn_vfs_fseek(fid,seekinfo->HintV.offset[i],SEEK_SET);
	     if((*nkn_fread)(dta,seekinfo->HintV.num_bytes[i],1,fid)== 0){
		 rv =  NOMORE_SAMPLES;
		 goto cleanup;
	     }

#ifdef _MP4Box_
             read_rtp_sample_interface(dta,&hint_video[i],seekinfo->HintV.offset[i],seekinfo->Video.offset[i],&media_stbl->HintVidStbl,&media_stbl->VideoStbl,seekinfo->\
                                       HintV.num_samples_sent);
#else
             read_rtp_sample_interface(dta,&hint_video[i],seekinfo->HintV.offset[i], seekinfo->Video.offset[i]);
#endif

	     //          (hint_video+i)->rtp->header.relative_time+= get_time_offset(media_stbl->HintVidStbl.stts,seekinfo->HintV.num_samples_sent+i);
	     for(j=0;j<(hint_video+i)->num_rtp_pkts;j++) {
		 rtp_header = (hint_video+i)->rtp;
		 rtp_header+=j;
		 rtp_header->header.relative_time+= get_time_offset(media_stbl->HintVidStbl.stts,seekinfo->HintV.num_samples_sent+i) + rtp_header->time_stamp;
	     }
	     if(dta != NULL){
		 free(dta);
		 dta = NULL;
	     }
	 }
	 seekinfo->HintV.num_samples_sent+=sample_to_be_sent_vid;
	 seekinfo->Video.num_samples_sent+=sample_to_be_sent_vid;
	 break;
	 case AUDIO_HINT_TRAK:
	     if((seekinfo->HintA.num_samples_sent+SAMPLES_TO_BE_SENT<=st->haud_s->total_sample_size)||(seekinfo->HintA.num_samples_sent==0)) {
		 ret=find_chunk_asset(seekinfo->HintA.num_bytes,seekinfo->HintA.offset,SAMPLES_TO_BE_SENT,seekinfo->HintA.num_samples_sent,&media_stbl->HintAudStbl,st->haud_s);
		 ret=find_chunk_asset(seekinfo->Audio.num_bytes,seekinfo->Audio.offset,SAMPLES_TO_BE_SENT,seekinfo->Audio.num_samples_sent,&media_stbl->AudioStbl,st->aud_s);
		 sample_to_be_sent_aud = SAMPLES_TO_BE_SENT;
	     } else {
		 if(st->haud_s->total_sample_size - seekinfo->HintA.num_samples_sent<SAMPLES_TO_BE_SENT&&(st->haud_s->total_sample_size - seekinfo->HintA.num_samples_sent > 0)){
		     sample_to_be_sent_aud = st->haud_s->total_sample_size-seekinfo->HintA.num_samples_sent;
		     ret=find_chunk_asset(seekinfo->HintA.num_bytes,seekinfo->HintA.offset,sample_to_be_sent_aud,seekinfo->HintA.num_samples_sent,&media_stbl->HintAudStbl,st->haud_s);
		     ret=find_chunk_asset(seekinfo->Audio.num_bytes,seekinfo->Audio.offset,sample_to_be_sent_aud,seekinfo->Audio.num_samples_sent,&media_stbl->AudioStbl,st->aud_s);
		 } else if(st->haud_s->total_sample_size == seekinfo->HintA.num_samples_sent) {
		     rv =  NOMORE_SAMPLES;
		     goto cleanup;
		 }
	     }
#ifdef PERF_DEBUG1
	     {
		 struct timeval result1;
		 gettimeofday(&selfTimeEnd,NULL);
		 timeval_subtract1(&selfTimeEnd, &selfTimeStart, &result1);
		 printf("TRAK TYPE %d, Self Time %ld\n", trak_type, (int64_t)((result1.tv_sec*1e6)+result1.tv_usec));
	     }
#endif
	     for(i=0;i<sample_to_be_sent_aud;i++) {
		 dta =(uint8_t*) malloc(seekinfo->HintA.num_bytes[i]);
		 nkn_vfs_fseek(fid,seekinfo->HintA.offset[i],SEEK_SET);
		 if((*nkn_fread)(dta,seekinfo->HintA.num_bytes[i],1,fid) == 0){
		     rv = NOMORE_SAMPLES;
		     goto cleanup;
		 }
		 
		 read_rtp_sample_audio_interface(dta,&hint_audio[i],seekinfo->HintA.offset[i], seekinfo->Audio.offset[i], &media_stbl->HintAudStbl, &media_stbl->AudioStbl,&st->haud_s->prev_sample_details,&st->aud_s->prev_sample_details);
		 for(j=0;j<(hint_audio+i)->num_rtp_pkts;j++) {
		     rtp_header = (hint_audio+i)->rtp;
		     rtp_header+=j;
		     rtp_header->header.relative_time+= get_time_offset(media_stbl->HintAudStbl.stts,seekinfo->HintA.num_samples_sent+i) + rtp_header->time_stamp;
		 }
		 if(dta != NULL){
		     free(dta);
		     dta = NULL;
		 }
	     }
	     seekinfo->HintA.num_samples_sent+=sample_to_be_sent_aud;
	     seekinfo->Audio.num_samples_sent+=sample_to_be_sent_aud;
	     break;
    case VIDEO_TRAK:
	if((seekinfo->Video.num_samples_sent+SAMPLES_TO_BE_SENT<=st->vid_s->total_sample_size)||(seekinfo->Video.num_samples_sent==0)) {
	    ret=find_chunk_asset(seekinfo->Video.num_bytes,seekinfo->Video.offset,SAMPLES_TO_BE_SENT,seekinfo->Video.num_samples_sent,&media_stbl->VideoStbl,st->vid_s);
	    sample_to_be_sent_vid = SAMPLES_TO_BE_SENT;
	} else {
	    if(st->vid_s->total_sample_size - seekinfo->Video.num_samples_sent<SAMPLES_TO_BE_SENT&&(st->vid_s->total_sample_size - seekinfo->Video.num_samples_sent >0)){
		ret=find_chunk_asset(seekinfo->Video.num_bytes,seekinfo->Video.offset,sample_to_be_sent_vid,seekinfo->Video.num_samples_sent,&media_stbl->VideoStbl,st->vid_s);
	    } else if(st->vid_s->total_sample_size == seekinfo->Video.num_samples_sent) {
		rv = NOMORE_SAMPLES;
		goto cleanup;
	    }
	}
#ifdef PERF_DEBUG1
	{
	    struct timeval result1;
	    gettimeofday(&selfTimeEnd,NULL);
	    timeval_subtract1(&selfTimeEnd, &selfTimeStart, &result1);
	    //printf("TRAK TYPE %d, Sample Num %d, Time Elapsed %lu <---------> Expected Time %d ===== next DTS is  %d\n", psmss->trackType, psmss->sampleNumber, psmss->timeElapsed, psmss->lastDTS, psmss->nextDTS);
	printf("TRAK TYPE %d, Self Time %ld\n", trak_type, (int64_t)((result1.tv_sec*1e6)+result1.tv_usec));
}
#endif
	seekinfo->Video.num_samples_sent+=sample_to_be_sent_vid;	
	break;
    case AUDIO_TRAK:
	if((seekinfo->Audio.num_samples_sent+SAMPLES_TO_BE_SENT<=st->aud_s->total_sample_size)||(seekinfo->Audio.num_samples_sent==0)) {
	    ret=find_chunk_asset(seekinfo->Audio.num_bytes,seekinfo->Audio.offset,SAMPLES_TO_BE_SENT,seekinfo->Audio.num_samples_sent,&media_stbl->AudioStbl,st->aud_s);
	    sample_to_be_sent_aud = SAMPLES_TO_BE_SENT;
	} else {
	    if(st->aud_s->total_sample_size - seekinfo->Audio.num_samples_sent<SAMPLES_TO_BE_SENT&&(st->aud_s->total_sample_size - seekinfo->Audio.num_samples_sent >0)){
		sample_to_be_sent_aud = st->aud_s->total_sample_size-seekinfo->Audio.num_samples_sent;

		ret=find_chunk_asset(seekinfo->Audio.num_bytes,seekinfo->Audio.offset,sample_to_be_sent_aud,seekinfo->Audio.num_samples_sent,&media_stbl->AudioStbl,st->aud_s);
	    } else if(st->aud_s->total_sample_size == seekinfo->Audio.num_samples_sent) {
		rv =  NOMORE_SAMPLES;
		goto cleanup;
	    }
	}
#ifdef PERF_DEBUG1
	{
	    struct timeval result1;
	    gettimeofday(&selfTimeEnd,NULL);
	    timeval_subtract1(&selfTimeEnd, &selfTimeStart, &result1);
	    //printf("TRAK TYPE %d, Sample Num %d, Time Elapsed %lu <---------> Expected Time %d ===== next DTS is  %d\n", psmss->trackType, psmss->sampleNumber, psmss->timeElapsed, psmss->lastDTS, psmss->nextDTS);
	printf("TRAK TYPE %d, Self Time %ld\n", trak_type, (int64_t)((result1.tv_sec*1e6)+result1.tv_usec));
}
#endif

	seekinfo->Audio.num_samples_sent+=sample_to_be_sent_aud;
	break;

/*CHANGES CHANGES HERE*/
	 }

	 switch(trak_type){
	     case AUDIO_HINT_TRAK:
		 st->haud_s->look_ahead_time = get_time_offset(media_stbl->HintAudStbl.stts,seekinfo->HintA.num_samples_sent+1);
		 break;
	     case VIDEO_HINT_TRAK:
		 st->hvid_s->look_ahead_time = get_time_offset(media_stbl->HintVidStbl.stts,seekinfo->HintV.num_samples_sent+1);
		 break;
#ifdef NK_CTTS_OPT
	     case AUDIO_TRAK:
		 st->aud_s->look_ahead_time = get_time_offset(media_stbl->AudioStbl.stts,seekinfo->Audio.num_samples_sent+1);
		 if (media_stbl->AudioStbl.ctts_tab.size != -1)
		     st->aud_s->ctts_offset  = get_ctts_offset(media_stbl->AudioStbl.ctts,seekinfo->Audio.num_samples_sent,&st->aud_s->prev_ctts);
		 break;
	     case VIDEO_TRAK:
		 st->vid_s->look_ahead_time = get_time_offset(media_stbl->VideoStbl.stts,seekinfo->Video.num_samples_sent+1);
		 if (media_stbl->VideoStbl.ctts_tab.size != -1)
		     st->vid_s->ctts_offset  = get_ctts_offset(media_stbl->VideoStbl.ctts,seekinfo->Video.num_samples_sent,&st->vid_s->prev_ctts);
		 break;
#else
             case AUDIO_TRAK:
                 st->aud_s->look_ahead_time = get_time_offset(media_stbl->AudioStbl.stts,seekinfo->Audio.num_samples_sent+1);
                 if (media_stbl->AudioStbl.ctts_tab.size != -1)
                     st->aud_s->ctts_offset  = get_ctts_offset(media_stbl->AudioStbl.ctts,seekinfo->Audio.num_samples_sent);
                 break;
             case VIDEO_TRAK:
                 st->vid_s->look_ahead_time = get_time_offset(media_stbl->VideoStbl.stts,seekinfo->Video.num_samples_sent+1);
                 if (media_stbl->VideoStbl.ctts_tab.size != -1)
                     st->vid_s->ctts_offset  = get_ctts_offset(media_stbl->VideoStbl.ctts,seekinfo->Video.num_samples_sent);
                 break;
#endif
	 }

	 goto cleanup;	 
cleanup:
	 return rv;
 }

 int32_t get_total_sample_size(uint8_t* data)
 {
     int32_t tsize;
     data+=16;
     tsize=_read32(data,0);
     return tsize;
 }



 int32_t find_nearest_sync_sample(uint8_t *data,int32_t sample_num)
 {
     int32_t i,entry_count,smpl_no=1, data_pos=12,prev_smpl_no=1;
     entry_count = _read32(data,data_pos);
     data_pos+=4;
     for(i=0;i<entry_count;i++){
	 smpl_no=_read32(data,data_pos);
	 if(sample_num<=smpl_no)
	     break;
	 data_pos+=4;
	 prev_smpl_no=smpl_no;
     }
     return prev_smpl_no;
 }


int32_t find_chunk_offset(uint8_t* data,int32_t chunk_num){
    //data->stco. Chunk numbering starts from 1.
    int32_t data_pos=12,offset;
    data_pos+=4;
    data_pos+= (chunk_num-1)*4;
    offset = _read32(data,data_pos);
    return offset;
}

 
int32_t seek_state_change(Stbl_t *stbl,int32_t seek_time,int32_t time_scale,Sampl_prop_t* dtls)
{
    int32_t sample_num,chunk_num,first_sample_of_chunk,offset,base_offset;
    //int32_t *zero_ptrs;
    //Get the sample number corresponding to the time stamp.
    sample_num = read_stts(stbl->stts,seek_time,time_scale);
    //Get the closest sync sample number
    //if(stbl->stss!=NULL)
    //sample_num = find_nearest_sync_sample(stbl->stss,sample_num);
    //Get the chunk number corresponding to this sample number


    chunk_num=get_chunk_offsets(sample_num-1,stbl->stsc,dtls->total_chunks, &dtls->prev_sample_details.chunk_count, &dtls->prev_sample_details.prev_sample_total);
    //Get the sample number of the first sample in the chunk
    //    first_sample_of_chunk=find_first_sample_of_chunk(stbl->stsc,chunk_num)+1;
  

    if((first_sample_of_chunk=find_first_sample_of_chunk(stbl->stsc,chunk_num)) == -1)
        first_sample_of_chunk = sample_num;
    else
        first_sample_of_chunk+=1;


    //Calculate the offset of the offset of this sample in file.
    base_offset = find_chunk_offset(stbl->stco,chunk_num);
    //Find cumulative offset in chunk to present sample in chunk
    offset= base_offset + find_cumulative_sample_size(stbl->stsz,first_sample_of_chunk,sample_num);
    dtls->prev_base = base_offset;
    dtls->size_offset = offset - base_offset;
    //NK:
    dtls->prev_sample_details.is_stateless = 1;
    return sample_num-1;
}




int32_t media_seek_state_change(Stbl_t *stbl,Sampl_prop_t* dtls,int32_t sample_num)
{

     int32_t chunk_num,first_sample_of_chunk,offset,base_offset;
     //Get the chunk number corresponding to this sample number
     chunk_num=get_chunk_offsets(sample_num,stbl->stsc,dtls->total_chunks,&dtls->prev_sample_details.chunk_count,&dtls->prev_sample_details.prev_sample_total);

     //Get the sample number of the first sample in the chunk
     first_sample_of_chunk=find_first_sample_of_chunk(stbl->stsc,chunk_num)+1;
     //Calculate the offset of the offset of this sample in file.
     base_offset = find_chunk_offset(stbl->stco,chunk_num);
     //Find cumulative offset in chunk to present sample in chunk
     offset= base_offset + find_cumulative_sample_size(stbl->stsz,first_sample_of_chunk,sample_num);
     dtls->prev_base = base_offset;
     dtls->size_offset = offset - base_offset;
     //NK:
     dtls->prev_sample_details.is_stateless = 1;

     return 1;
}

 int32_t find_first_sample_of_chunk(uint8_t *data,int32_t chunk_num)
 {
     //data->stsc
     int32_t sample_num=0,i,data_pos=0,first_chunk,entry_count,samples_per_chunk,next_chnk_num,prev_samples_per_chunk;
     data_pos = 4+4+4;//account for exteneded box headers
     entry_count = _read32(data,data_pos);
     if(entry_count == 1)
	 return -1;
     data_pos+=4;
     for(i=0;i<entry_count;i++){
	 first_chunk=_read32(data,data_pos);
	 data_pos+=4;
	 samples_per_chunk = _read32(data,data_pos);
	 if(i<entry_count-1)
	     next_chnk_num = _read32(data,data_pos+8);
	 if(chunk_num<=first_chunk){
	     /* changed from sample_num+=samples_per_chunk*(chunk_num-first_chunk); because if the run of chunks overruns
	      * the sample that we are searching for then we need to know the samples in the previous run of chunks; added
	      * a variable to this effect and fixed this bug
	      */
	     if(chunk_num==1)
		 sample_num=1;
	     else
		 sample_num+=prev_samples_per_chunk*(chunk_num-first_chunk);
	     break;
	 }
	 sample_num+=samples_per_chunk*(next_chnk_num-first_chunk);
	 prev_samples_per_chunk = samples_per_chunk;
	 data_pos+=4+4;
     }
     return sample_num;
 }


 int32_t find_cumulative_sample_size(uint8_t* data,int32_t first_sample,int32_t sample_num)
 {
     int32_t total_size = 0,i,data_pos=0,sample_size;
     data_pos +=12;
     sample_size = _read32(data,data_pos);
     data_pos+=4;
     if(sample_size!=0) { 
	 if(sample_num == 1) {
	     total_size= sample_size*(sample_num-(first_sample-1));
	 } else {
	     total_size= sample_size*(sample_num-(first_sample));
	 }
     } else {
	 data_pos+=4;//account for sample_count
	 data_pos+=(first_sample-1)*4;
	 for(i=first_sample;i<sample_num;i++){
	     total_size+=_read32(data,data_pos);
	     data_pos+=4;
	 }
     }

     return total_size;
 }


/* read the SDP information from the MP4 file
 *@param SDP [in] - will contain the SDP data as null terminated text
 *@param nkn_fread [in] - read function
 *@param fid [in] - file decriptor
 */
void get_sdp_info(SDP_info_t* SDP,size_t (*nkn_fread)( void *, size_t, size_t, FILE*), FILE *fid){
    uint8_t* data,*p_data;
    box_t box;
    int64_t fpos;
    int32_t box_size,sub_moov_box_size,moov_not_parsed=1,word;
    size_t size,bytes_read=0;
    char moov_id[] = {'m', 'o', 'o', 'v'};
    char trak_id[] = {'t', 'r', 'a', 'k'};
    char udta_id[] = {'u', 'd', 't', 'a'};
    char hnti_id[] = {'h', 'n', 't', 'i'};
    char rtp_id[] = {'r','t','p',' '};
    char sdp_id[] = {'s','d','p',' '};
    SDP->num_bytes=0;
    SDP->sdp=NULL;
    
    box_size=0;
    data=(uint8_t*)malloc(8*sizeof(uint8_t));
    (*nkn_fread)(data,1,8,fid);
    word=_read32(data,4);
    while(word!=MOOV_HEX_ID){
	nkn_vfs_fseek(fid,box_size,SEEK_SET);
	(*nkn_fread)(data,8,1,fid);
	box_size=_read32(data,0);
	word=_read32(data,4);
    }
    //in moov position
    fpos=nkn_vfs_ftell(fid);
    fpos-=8;
    nkn_vfs_fseek(fid,fpos,SEEK_SET);
    if(data != NULL)
	free(data);
    data=(uint8_t*)malloc(box_size*sizeof(uint8_t));
    (*nkn_fread)(data,box_size,1,fid);
    size=box_size;
    p_data=data;

    while(moov_not_parsed){
	box_size = read_next_root_level_box(p_data, size, &box, &bytes_read);
	if(check_box_type(&box,moov_id)){
	    // uint8_t in_moov=1;
	    int32_t moov_box_size;
	    uint8_t* p_moov;
	    p_moov=p_data;
	     moov_box_size=box_size;
	    //Search for udta box here
	    p_data+=bytes_read;
	    while((p_data-p_moov)<moov_box_size){
		sub_moov_box_size = read_next_root_level_box(p_data, size, &box, &bytes_read);
		box_size=sub_moov_box_size;
		if(check_box_type(&box, trak_id)){
		    int32_t trak_size, bytes_left;
		    uint8_t* p_trak;
		    p_trak=p_data;
		    trak_size=box_size;
		    bytes_left=trak_size-8;
		    p_trak+=bytes_read;
		    while(bytes_left>0){
			box_size = read_next_root_level_box(p_trak, size, &box, &bytes_read);
			if(check_box_type(&box, udta_id)){
			    uint8_t* p_udta;
			    int32_t child_udta_size,udta_size;
			    udta_size=box_size;
			    p_udta=p_trak+bytes_read;
			    udta_size-=bytes_read;
			    while(udta_size>0){
				child_udta_size=read_next_root_level_box(p_udta, size, &box, &bytes_read);
				if(check_box_type(&box, hnti_id)){
				    int32_t non_first_hint_info=0,hnti_size;
				    if(SDP->num_bytes!=0){
					//Add the required bytes to the same. Copy the SDP buffer.
					uint8_t *temp_ptr;
                                        non_first_hint_info=1;
					hnti_size=child_udta_size-8-8;
					temp_ptr=(uint8_t*)malloc(SDP->num_bytes+1);
					memcpy(temp_ptr,SDP->sdp,SDP->num_bytes);
					if(SDP->sdp != NULL)
					    free(SDP->sdp);
					SDP->sdp=(uint8_t*)malloc(SDP->num_bytes+hnti_size+1);
					memcpy(SDP->sdp,temp_ptr,SDP->num_bytes);
					if(temp_ptr != NULL)
					    free(temp_ptr);
				    }


				   p_udta+=bytes_read;
				   child_udta_size=read_next_root_level_box(p_udta, size, &box, &bytes_read);
				   if(check_box_type(&box, sdp_id)){
				       if(non_first_hint_info){
					   uint8_t* temp_data;
					   temp_data=SDP->sdp+SDP->num_bytes;
					   SDP->num_bytes+=child_udta_size-(4+4);
                                           p_udta+=4+4;
                                           memcpy(temp_data,p_udta,hnti_size);
				       }
				       else{
					   SDP->num_bytes+=child_udta_size-(4+4);
					   SDP->sdp=(uint8_t*)malloc(SDP->num_bytes+1);
					   p_udta+=4+4;
					   memcpy(SDP->sdp,p_udta,SDP->num_bytes);
				       }
				       udta_size=0;
				       bytes_left=0;
				   }					
				}
				else{
				    udta_size-=child_udta_size;
				    p_udta+=child_udta_size;				    
				}
				    
			    }
			    
			}
			else{
			    bytes_left-=box_size;
			    p_trak+=box_size;
			}
			    
		    }//while(bytes_left>0)
		}

		if(check_box_type(&box, udta_id)){
		    uint8_t* p_udta;
		    int32_t child_udta_size,udta_size;
		    udta_size=box_size;
		    p_udta=p_data+bytes_read;
		    udta_size-=bytes_read;
		    while(udta_size>0){
			child_udta_size=read_next_root_level_box(p_udta, size, &box, &bytes_read);
			if(check_box_type(&box, hnti_id)){
			    p_udta+=bytes_read;
			    child_udta_size=read_next_root_level_box(p_udta, size, &box, &bytes_read);
			    if(check_box_type(&box, rtp_id)){
				int32_t rtp_hnti_size;
				uint8_t* sdp_ptr;
				rtp_hnti_size=child_udta_size-12;
				if(SDP->num_bytes!=0){
				    //Add the required bytes to the same. Copy the SDP buffer.
				    uint8_t *temp_ptr;
				    temp_ptr=(uint8_t*)malloc(SDP->num_bytes+1);
				    memcpy(temp_ptr,SDP->sdp,SDP->num_bytes);
				    if(SDP->sdp != NULL)
					free(SDP->sdp);				    
				    SDP->sdp=(uint8_t*)malloc(SDP->num_bytes+rtp_hnti_size+1);
				    memcpy(SDP->sdp,temp_ptr,SDP->num_bytes);
				    if(temp_ptr != NULL)
					free(temp_ptr);
				}
				sdp_ptr=SDP->sdp+SDP->num_bytes;
				p_udta+=12;
				memcpy(sdp_ptr,p_udta,rtp_hnti_size);
				SDP->num_bytes+=rtp_hnti_size;
				udta_size=0;
			    }
			}
			else{
			    udta_size-=child_udta_size;
			    p_udta+=child_udta_size;
			}

		    }

		}
		p_data+=sub_moov_box_size;
		if((p_data-p_moov)>=moov_box_size)
		    moov_not_parsed=0;
	    }// while(in_moov)
	}
    }
    if(data != NULL)
	free(data);

    SDP->sdp[SDP->num_bytes]='\0';
}

int32_t uint32_byte_swap(int32_t input){
    return (((input&0x000000FF)<<24)+((input&0x0000FF00)<<8)+((input&0x00FF0000)>>8)+((input&0xFF000000)>>24));
}

/***********************************************************************
 *
 *                    UTILITY FUNCTIONS
 *
 ***********************************************************************/
/* returns the offset in the MP4 file for a given sample in a track
 *@param sample_size[in] - size of a sample
 *@param offsets[out] - offset in the file
 *@param num_samples_to_send[in] - number of samples to send
 *@param sample_number[in] - sample number whose offset is needed
 *@param stbl [in] - sample table
 *@param prop [in] - sample description
 */

int32_t find_chunk_asset(int32_t* sample_size,int32_t* offsets,int32_t num_samples_to_send,int32_t sample_num,Stbl_t *stbl,Sampl_prop_t* prop){
    int32_t temp_sam_size,samp_cnt,i;
    int32_t *chunk_num;
    uint8_t* data;

    int32_t tt_nk = 0;

    //sample_sizes=(int32_t*)malloc(num_samples_to_send*sizeof(int32_t));
    chunk_num=(int32_t*)malloc(num_samples_to_send*sizeof(int32_t));
    //GET SIZES OF SAMPLES:
    data=stbl->stsz;
    data+=12;
    temp_sam_size=_read32(data,0);
    data+=4;
    samp_cnt=_read32(data,0);
    prop->total_sample_size=samp_cnt;
    data+=4;
    if(temp_sam_size==0){
	data+=4*sample_num;
	for(i=0;i<num_samples_to_send;i++){
	    sample_size[i]=_read32(data,0);
	    data+=4;
	}
    } else {
	for(i=0;i<num_samples_to_send;i++) {
	    sample_size[i]=temp_sam_size;
	}
    }

    data=stbl->stco;
    data+=12;
    prop->total_chunks = _read32(data,0);
    prop->prev_sample_details.total_chunks= prop->total_chunks;

    //Get chunk in which sample is present.
    data=stbl->stsc;
    for(i=0;i<num_samples_to_send;i++)
        chunk_num[i]=get_chunk_offsets(sample_num+i,stbl->stsc,prop->total_chunks,&prop->prev_sample_details.chunk_count,&prop->prev_sample_details.prev_sample_total);
    
    //Get chunk offsets as file offsets:
    data=stbl->stco;
    data+=12;
    //#define TEMMMP
#ifdef TEMMMP
    if((sample_num == 1580) && tt_nk){
	FILE * fid;
	fid = fopen("/var/home/root/pkts/stco_gen","wb");
	fwrite(data,4*prop->total_chunks,1,fid);
	fclose(fid);
    }

#endif

    for(i=0;i<num_samples_to_send;i++){
	samp_cnt=chunk_num[i]*4;
	offsets[i]=_read32(data,samp_cnt);
    }
    for(i=0;i<num_samples_to_send;i++){
	if((prop->prev_base==0) || (prop->prev_base==offsets[i])){
	    offsets[i]+=prop->size_offset;
	    prop->size_offset+=sample_size[i];
	    if((prop->prev_base==0))
		prop->prev_base=offsets[i];
	}
	else{
	    prop->size_offset=sample_size[i];
	    prop->prev_base=offsets[i];
	}
	 
    }
    prop->prev_sample_details.sample_num = sample_num;
    if(chunk_num != NULL)
	free(chunk_num);
    return 1;
}


/* computes the DTS for a sample number
 */

 int32_t get_time_offset(uint8_t* data, int32_t sample_num){
     int32_t time_tt = 0,time_base=0,entry_count,i,delta, sample_cnt=0,prev_sample_num=0;
     data+=8+4;
     entry_count = _read32(data,0);
     data+=4;
     for(i=0;i<entry_count;i++){
	 sample_cnt+=_read32(data,0);
	 delta = _read32(data,4);
	 if(sample_num<sample_cnt){
	     time_tt = time_base + delta*(sample_num-prev_sample_num);
	     break;
	 }

	 time_base+= delta*(sample_cnt-prev_sample_num);
	 prev_sample_num = sample_cnt;
	 data+=8;
     }
     return time_tt;
 }


#ifndef NK_CTTS_OPT 
int32_t get_ctts_offset(uint8_t* data, int32_t sample_num){
     int32_t time_tt = 0,time_base=0,entry_count,i,delta, sample_cnt=0,prev_sample_num=0;
     data+=8+4;
     entry_count = _read32(data,0);
     data+=4;
     for(i=0;i<entry_count;i++){
         sample_cnt+=_read32(data,0);
         delta = _read32(data,4);
         if(sample_num<=sample_cnt)
            break;
        
         data+=8;
     }
     return delta;
}

#else
/*
 This holds the optimzed version of the ctts lookup. Maintains state and averts for loops.
 Was found to be a blocking item.
*/
int32_t get_ctts_offset(uint8_t* data, int32_t sample_num, ctts_state_t* prev){
    int32_t sample_base,entry_count,i,delta, sample_cnt=0,prev_sample_num=0;
    data+=8+4;
    if(prev->entry_count == 0)
	prev->entry_count = _read32(data,0);
    data+=4;
    data+=8*prev->prev_ctts_entry;
    sample_cnt = prev->prev_cum_sample_cnt;
    for(i=prev->prev_ctts_entry;i<prev->entry_count;i++){
	sample_base = _read32(data,0);
	sample_cnt+=sample_base;
	delta = _read32(data,4);
	if(sample_num<=sample_cnt){
	    prev->prev_ctts_entry = i;
	    prev->prev_cum_sample_cnt = sample_cnt -sample_base;
	    break;
	}
	data+=8;
    }
    return delta;
}

#endif


/* translates the sample number to a chunk and chunk offset
 */


 int32_t get_chunk_offsets(int32_t sample_number, uint8_t* data, int32_t last_chunk_num, int32_t* prev_sample_num, int32_t* prev_total)
{
    int32_t entry_count,i,total_sample_num,starting_sample_num,next_chunk,num_chunks,num_sample_chnk,chunk_num,first_chunk,total_prev;
    data+=12;
    entry_count=_read32(data,0);
    data+=4;
    total_sample_num=*prev_total;
    chunk_num = 0;
    data+=12*(*prev_sample_num);

    for(i=*prev_sample_num;i<entry_count;i++){
	total_prev = total_sample_num;
        starting_sample_num=total_sample_num;
        first_chunk=_read32(data,0);
        if(i!=entry_count-1){
            next_chunk = _read32(data,12);
            num_chunks=next_chunk-first_chunk;
        }
        else
	    num_chunks=last_chunk_num-first_chunk+1;

        num_sample_chnk = _read32(data,4);
        total_sample_num+=num_chunks*num_sample_chnk;
        data+=12;
        if(sample_number<total_sample_num){
	    //	    chunk_num =first_chunk+((total_sample_num-sample_number-1)/num_sample_chnk);
	    //	    chunk_num = first_chunk + (sample_number/num_sample_chnk);
	    chunk_num = first_chunk + ((sample_number-total_prev)/num_sample_chnk);
	    *prev_sample_num = i;
	    *prev_total = total_sample_num-num_chunks*num_sample_chnk;
	    i=entry_count;
        } else if(sample_number >= total_sample_num) {//should not exceed the total num of samples though, might want to add that check
	    chunk_num = i;
	}
    }
    if(chunk_num<1)
	chunk_num=1;

    return chunk_num;
}

/* reads all teh track assets needed for parsing, namely sample tables and description boxes
 */
int32_t get_trak_asset(uint8_t* stbl_data, Stbl_t* stbl)
{
    /*This should return all 4 traks*/
    int32_t box_size,size,stbl_box_size;
    size_t bytes_read;
    box_t box;
    uint8_t* p_data;
    char stsc_id[] = {'s', 't', 's', 'c'};
    char stco_id[] = {'s', 't', 'c', 'o'};
    char stsz_id[] = {'s', 't', 's', 'z'};
    char stts_id[] = {'s', 't', 't', 's'};
    char ctts_id[] = {'c', 't', 't', 's'};
    char stsd_id[] = {'s', 't', 's', 'd'};
    char stss_id[] = {'s', 't', 's', 's'};

    size = 10000;
    stbl_box_size = read_next_root_level_box(stbl_data, size, &box, &bytes_read);
    p_data=stbl_data;
    stbl_data+=bytes_read;
    while(stbl_data-p_data<stbl_box_size){
	box_size = read_next_root_level_box(stbl_data, size, &box, &bytes_read);
	if (box_size < 0)
	    { 
		return NOMORE_SAMPLES;
	    }

	if(check_box_type(&box,stsc_id)){

	    stbl->stsc_tab.offset+= (int32_t)(stbl_data-p_data);
	    stbl->stsc_tab.size = box_size;
	    stbl_data+=box_size;
	}
	else if(check_box_type(&box,stsz_id)){
            stbl->stsz_tab.offset+= (int32_t)(stbl_data-p_data);
            stbl->stsz_tab.size = box_size;
            stbl_data+=box_size;
        }
	else if(check_box_type(&box,stco_id)){
            stbl->stco_tab.offset+= (int32_t)(stbl_data-p_data);
            stbl->stco_tab.size = box_size;
            stbl_data+=box_size;

        }
        else if(check_box_type(&box,stts_id)){
            stbl->stts_tab.offset+= (int32_t)(stbl_data-p_data);
            stbl->stts_tab.size = box_size;
            stbl_data+=box_size;
        }
	else if(check_box_type(&box,ctts_id)){
            stbl->ctts_tab.offset+= (int32_t)(stbl_data-p_data);
            stbl->ctts_tab.size = box_size;
            stbl_data+=box_size;
        }
	else if(check_box_type(&box,stsd_id)){
            stbl->stsd_tab.offset+= (int32_t)(stbl_data-p_data);
            stbl->stsd_tab.size = box_size;
            stbl_data+=box_size;
        }
        else if(check_box_type(&box,stss_id)){
            stbl->stss_tab.offset+= (int32_t)(stbl_data-p_data);
            stbl->stss_tab.size = box_size;
	    stbl_data+=box_size;
        }
	else
	    stbl_data+=box_size;
    }
    return 1;
}


int32_t  init_stbls(Stbl_t* stbl,int32_t pos)
{
    stbl->stsc_tab.size=-1;
    stbl->stsz_tab.size=-1;
    stbl->stco_tab.size=-1;
    stbl->stts_tab.size=-1;
    stbl->ctts_tab.size=-1;
    stbl->stsd_tab.size=-1;
    stbl->esds_tab.size=-1;
    stbl->codec_type = -1;

    stbl->stss_tab.size=-1;
    stbl->stss_tab.offset=pos;
    stbl->stsc_tab.offset=pos;
    stbl->stsz_tab.offset=pos;
    stbl->stco_tab.offset=pos;
    stbl->stts_tab.offset=pos;
    stbl->ctts_tab.offset=pos;
    stbl->stsd_tab.offset=pos;
    stbl->esds_tab.offset=pos;
    return 1;
}
#if 0
int32_t free_stbls(Stbl_t* stbl){


    if(stbl->stsc!=NULL){
        free(stbl->stsc);
    }
    if(stbl->stco!=NULL){
        free(stbl->stco);
    }
    if(stbl->stts!=NULL){
        free(stbl->stts);
    }
    if(stbl->ctts!=NULL){
        free(stbl->ctts);
    }
    if(stbl->stsz!=NULL){
        free(stbl->stsz);
    }
    if(stbl->stss!=NULL){
        free(stbl->stss);
    }
    return 1;
}
#endif
int32_t read_stbls(Stbl_t* stbl,size_t (*nkn_fread)( void *, size_t, size_t, FILE*), FILE *fid)
{
  
    if(stbl->stsc_tab.size!=-1){
	stbl->stsc = (uint8_t*)malloc(stbl->stsc_tab.size);
	nkn_vfs_fseek(fid,stbl->stsc_tab.offset,SEEK_SET);
	if((*nkn_fread)( stbl->stsc,stbl->stsc_tab.size,1,fid) == 0)
	    return VFS_READ_ERROR;
    }
    if(stbl->stco_tab.size!=-1){
        stbl->stco = (uint8_t*)malloc(stbl->stco_tab.size);
        nkn_vfs_fseek(fid,stbl->stco_tab.offset,SEEK_SET);
        if((*nkn_fread)( stbl->stco,stbl->stco_tab.size,1,fid) == 0)
	    return VFS_READ_ERROR;
    }
    if(stbl->stsz_tab.size!=-1){
        stbl->stsz = (uint8_t*)malloc(stbl->stsz_tab.size);
        nkn_vfs_fseek(fid,stbl->stsz_tab.offset,SEEK_SET);
        if((*nkn_fread)( stbl->stsz,stbl->stsz_tab.size,1,fid) == 0)
	    return VFS_READ_ERROR;
    }
    if(stbl->stts_tab.size!=-1){
        stbl->stts = (uint8_t*)malloc(stbl->stts_tab.size);
        nkn_vfs_fseek(fid,stbl->stts_tab.offset,SEEK_SET);
        if((*nkn_fread)(stbl->stts,stbl->stts_tab.size,1,fid) ==0)
	    return VFS_READ_ERROR;
    }
    if(stbl->ctts_tab.size!=-1){
        stbl->ctts = (uint8_t*)malloc(stbl->ctts_tab.size);
        nkn_vfs_fseek(fid,stbl->ctts_tab.offset,SEEK_SET);
        if((*nkn_fread)(stbl->ctts,stbl->ctts_tab.size,1,fid) == 0)
	    return VFS_READ_ERROR;
    }
    if(stbl->stsd_tab.size!=-1){
        stbl->stsd = (uint8_t*)malloc(stbl->stsd_tab.size);
        nkn_vfs_fseek(fid,stbl->stsd_tab.offset,SEEK_SET);
        if((*nkn_fread)(stbl->stsd,stbl->stsd_tab.size,1,fid) == 0)
	    return VFS_READ_ERROR;
    }
    if(stbl->esds_tab.size!=-1){
        stbl->esds = (uint8_t*)malloc(stbl->esds_tab.size);
        nkn_vfs_fseek(fid,stbl->esds_tab.offset,SEEK_SET);
        if((*nkn_fread)(stbl->esds,stbl->esds_tab.size,1,fid) == 0)
	    return VFS_READ_ERROR;
    }
    if(stbl->stss_tab.size!=-1){
        stbl->stss = (uint8_t*)malloc(stbl->stss_tab.size);
        nkn_vfs_fseek(fid,stbl->stss_tab.offset,SEEK_SET);
        if((*nkn_fread)(stbl->stss,stbl->stss_tab.size,1,fid) == 0)
	    return VFS_READ_ERROR;
    }
    return 1;
}

int32_t get_Media_stbl_info(uint8_t *data,Media_stbl_t* mstbl,int32_t moov_box_size)
{
    int32_t bytes_left,box_size,trak_size,video_track_id=-1,audio_track_id=-1,hint_ref_id=-1,size,timescale,ret;
    size_t bytes_read;
    uint8_t* p_data,*p_trak;
    int32_t width=0,height=0;
    box_t box;
    char trak_id[] = {'t', 'r', 'a', 'k'};
    char mdia_id[] = {'m', 'd', 'i', 'a'};
    char stbl_id[] = {'s', 't', 'b', 'l'};
    char tkhd_id[] = {'t', 'k', 'h', 'd'};
    char tref_id[] = {'t', 'r', 'e', 'f'};
    char hint_id[] = {'h', 'i', 'n', 't'};
    char hdlr_id[] = {'h', 'd', 'l', 'r'};
    char minf_id[] = {'m', 'i', 'n', 'f'};
    char mvhd_id[] = {'m', 'v', 'h', 'd'};
    char mdhd_id[] = {'m', 'd', 'h', 'd'};
    char mp4a_id[] = {'m', 'p', '4', 'a'};
    char avc1_id[] = {'a', 'v', 'c', '1'};
    char mp4v_id[] = {'m', 'p', '4', 'v'};

    p_data = data;
    bytes_left=moov_box_size;
    size=moov_box_size;
    p_data+=8;
    bytes_left-=8;

    while(bytes_left>0){
	box_size = read_next_root_level_box(p_data, size, &box, &bytes_read);
	if(check_box_type(&box,trak_id)){
	    int32_t track_id;
	    trak_size=box_size;
	    p_trak=p_data;
	    p_trak+=bytes_read;
	    while(p_trak-p_data < trak_size){
		int32_t trak_type,temp=0;
		uint8_t *temp_data;
		box_size = read_next_root_level_box(p_trak, size, &box, &bytes_read);
		if(check_box_type(&box,tkhd_id)){
		    track_id = return_track_id(p_trak+bytes_read);
		    //height=*(p_trak+box_size-8);
		    //width=*(p_trak+box_size-4);
		    temp_data = p_trak +box_size -8;
		    temp = _read32(temp_data,0);
		    width= ((temp&0x0000ffff)<<16) | ((temp & 0xffff0000)>>16);

		    temp = _read32(temp_data,4);
		    height= ((temp&0x0000ffff)<<16) | ((temp & 0xffff0000)>>16);

		    p_trak+=box_size;
		}
		else if(check_box_type(&box,tref_id)){
		    uint8_t *tref_data;
		    int32_t subbox_size;
		    tref_data=p_trak+bytes_read;
		    subbox_size = read_next_root_level_box(tref_data, size, &box, &bytes_read);
		    if(check_box_type(&box,hint_id)){
			hint_ref_id = _read32(tref_data,8);
		    }		
                    p_trak+=box_size;
		}
		else if(check_box_type(&box,mdia_id)){
		    int32_t mdia_box_size;
		    uint8_t* p_mdia;
		    p_mdia=p_trak;
		    mdia_box_size=box_size;
		    p_mdia+=bytes_read;
		    while(p_mdia-p_trak<mdia_box_size){
			box_size = read_next_root_level_box(p_mdia, size, &box, &bytes_read);
			if(check_box_type(&box,hdlr_id)){
			    hdlr_t *hdlr;
			    hdlr = (hdlr_t*)hdlr_reader(p_mdia,0,box_size);
			    switch(hdlr->handler_type){
				case VIDE_HEX:
				    trak_type=VIDEO_TRAK;
				    video_track_id=track_id;
                                    mstbl->VideoStbl.trak_type=track_id;
                                    mstbl->VideoStbl.timescale = timescale;
				    mstbl->VideoStbl.height = height;
				    mstbl->VideoStbl.width = width;
				    break;
				case SOUN_HEX:
				    trak_type= AUDIO_TRAK;
				    audio_track_id=track_id;
                                    mstbl->AudioStbl.trak_type=track_id;
                                    mstbl->AudioStbl.timescale = timescale;
				    break;
				case HINT_HEX:
				    if(hint_ref_id==video_track_id){
					trak_type=VIDEO_HINT_TRAK;
                                        mstbl->HintVidStbl.trak_type=track_id;
                                        mstbl->HintVidStbl.timescale = timescale;
					mstbl->VideoStbl.height = height;
					mstbl->VideoStbl.width = width;
				    }
                                    if(hint_ref_id==audio_track_id){
                                        trak_type=AUDIO_HINT_TRAK;
                                        mstbl->HintAudStbl.trak_type=track_id;
                                        mstbl->HintAudStbl.timescale = timescale;
				    }
                                    break;
			    }
			    p_mdia+=box_size;
			    hdlr_cleanup(hdlr);
			}
			else if(check_box_type(&box,mdhd_id)){
                            uint8_t *p_mdhd;
                            p_mdhd = p_mdia;
                            p_mdhd+=8;
                            if(p_mdhd[0])
                                p_mdhd+=16+4;

                            else
                                p_mdhd+=8+4;

                            timescale = _read32(p_mdhd,0);
                            p_mdia+=box_size;
                        }
			else if(check_box_type(&box,minf_id)){
			    int32_t minf_size,temp_offset,box_size_pminf;
			    uint8_t* p_minf;
			    p_minf = p_mdia;
			    p_minf+=bytes_read;
			    minf_size=box_size;
			    while(p_minf-p_mdia<minf_size){
				box_size_pminf = read_next_root_level_box(p_minf, size, &box, &bytes_read);
				if(check_box_type(&box,stbl_id)){
				    uint8_t* stbl_data;
				    uint8_t* stsd_data;
				    uint8_t* mp4a_data;

				    Stbl_t* stb;
				    //int32_t stbl_size;
				    stbl_data=p_minf;
				    stsd_data = stbl_data +8;
				    mp4a_data = stsd_data + 16;
				    box_size = read_next_root_level_box(mp4a_data, size, &box, &bytes_read);
				    switch(trak_type){
					case VIDEO_TRAK:
					    stb=&mstbl->VideoStbl;
					    if(check_box_type(&box,mp4v_id))
						{
						    mstbl->VideoStbl.codec_type = MPEG4_VIDEO;
						}
					    if(check_box_type(&box,avc1_id))
						{
						    mstbl->VideoStbl.codec_type = H264_VIDEO;
						}
					    if(check_box_type(&box,avc1_id)||check_box_type(&box,mp4v_id))
						{
					    temp_offset = (int32_t)(stbl_data-data);

					    stb->stco_tab.offset+=(int32_t)(stbl_data-data);
					    stb->stsz_tab.offset+=(int32_t)(stbl_data-data);
					    stb->stts_tab.offset+=(int32_t)(stbl_data-data);
					    stb->ctts_tab.offset+=(int32_t)(stbl_data-data);
					    stb->stsc_tab.offset+=temp_offset;
					    stb->stsd_tab.offset+=(int32_t)(stbl_data-data);
					    stb->esds_tab.offset+=(int32_t)(stbl_data-data);
					    stb->stss_tab.offset+=(int32_t)(stbl_data-data);

					    ret = get_trak_asset(stbl_data, stb);
					    if (ret == NOMORE_SAMPLES)
						{
						    return ret;
						}
						}
					    break;
					case AUDIO_TRAK:
                                            stb=&mstbl->AudioStbl;
					    if(check_box_type(&box,mp4a_id))
						{
						    mstbl->AudioStbl.codec_type = MPEG4_AUDIO;
						    temp_offset = (int32_t)(stbl_data-data);

						    stb->stco_tab.offset+=(int32_t)(stbl_data-data);
						    stb->stsz_tab.offset+=(int32_t)(stbl_data-data);
						    stb->stts_tab.offset+=(int32_t)(stbl_data-data);
						    stb->ctts_tab.offset+=(int32_t)(stbl_data-data);
						    stb->stsc_tab.offset+=temp_offset;
						    stb->stsd_tab.offset+=(int32_t)(stbl_data-data);
						    stb->esds_tab.offset+=(int32_t)(stbl_data-data);
						    stb->stss_tab.offset+=(int32_t)(stbl_data-data);
						    ret = get_trak_asset(stbl_data, stb);
						    if (ret == NOMORE_SAMPLES)
							{
							    return ret;
							}
                                                }
                                            break;
					case VIDEO_HINT_TRAK:
                                            stb=&mstbl->HintVidStbl;
					    if(check_box_type(&box,mp4v_id))
						{
						    mstbl->HintVidStbl.codec_type = MPEG4_VIDEO;
						}
					    if(check_box_type(&box,avc1_id))
						{
						    mstbl->HintVidStbl.codec_type = H264_VIDEO;
						}
					    if((mstbl->VideoStbl.codec_type == MPEG4_VIDEO)||(mstbl->VideoStbl.codec_type == H264_VIDEO))
                                                {
                                                    temp_offset = (int32_t)(stbl_data-data);

                                                    stb->stco_tab.offset+=(int32_t)(stbl_data-data);
                                                    stb->stsz_tab.offset+=(int32_t)(stbl_data-data);
                                                    stb->stts_tab.offset+=(int32_t)(stbl_data-data);
						    stb->ctts_tab.offset+=(int32_t)(stbl_data-data);
                                                    stb->stsc_tab.offset+=temp_offset;
                                                    stb->stsd_tab.offset+=(int32_t)(stbl_data-data);
                                                    stb->esds_tab.offset+=(int32_t)(stbl_data-data);
                                                    stb->stss_tab.offset+=(int32_t)(stbl_data-data);

						    ret = get_trak_asset(stbl_data, stb);
						    if (ret == NOMORE_SAMPLES)
							{
							    return ret;
							}
                                                }
                                            break;
					case AUDIO_HINT_TRAK:
                                            stb=&mstbl->HintAudStbl;
					    if(check_box_type(&box,mp4a_id))
						{
						    mstbl->HintAudStbl.codec_type = MPEG4_AUDIO;
						}
					    if(mstbl->AudioStbl.codec_type == MPEG4_AUDIO)
                                                {
                                                    temp_offset = (int32_t)(stbl_data-data);

                                                    stb->stco_tab.offset+=(int32_t)(stbl_data-data);
                                                    stb->stsz_tab.offset+=(int32_t)(stbl_data-data);
                                                    stb->stts_tab.offset+=(int32_t)(stbl_data-data);
						    stb->ctts_tab.offset+=(int32_t)(stbl_data-data);
                                                    stb->stsc_tab.offset+=temp_offset;
                                                    stb->stsd_tab.offset+=(int32_t)(stbl_data-data);
                                                    stb->esds_tab.offset+=(int32_t)(stbl_data-data);
                                                    stb->stss_tab.offset+=(int32_t)(stbl_data-data);

						    ret = get_trak_asset(stbl_data, stb);
						    if (ret == NOMORE_SAMPLES)
							{
							    return ret;
							}
                                                }
                                            break;
					default:
					    break;
				    }

                                    //p_minf+=box_size;
				    p_minf+=box_size_pminf;
				}
				else
				    //p_minf+=box_size;
				    p_minf+=box_size_pminf;
			    }
			    p_mdia+=minf_size;

			}
			else{
			    p_mdia+=box_size;
			}
		    }//while(p_mdia-p_data<mdia_box_size)
		    p_trak+=mdia_box_size;
		}//if(check_box_type(&box,mdia_id))
		else
		    p_trak+=box_size;
	    }// while(p_trak-p_data < trak_size)
	    p_data+=trak_size;
	    bytes_left-=trak_size;
	}// if(check_box_type(&box,trak_id))
	else if(check_box_type(&box,mvhd_id)){
            uint8_t *mvhd_data,version;
            //int32_t timescale;
            int64_t tempval;
            mvhd_data=p_data+8;
            version = mvhd_data[0];
            mvhd_data+=4;
            if(version){
                mvhd_data+=16;
                timescale = _read32(mvhd_data,0);
                mvhd_data+=4;
                tempval = (int64_t)(_read32(mvhd_data,0));//((_read32(p_data,0))<<32)|(_read32(p_data,4);
                tempval = (tempval<<32)|((int64_t)(_read32(mvhd_data,4)));
                mstbl->duration = (int32_t)(tempval/(int64_t)(timescale));
            }
            else{
                mvhd_data+=8;
                timescale = _read32(mvhd_data,0);
                mvhd_data+=4;
                mstbl->duration = _read32(mvhd_data,0);
                mstbl->duration/=timescale;
            }
            p_data+=box_size;
            bytes_left-=box_size;
        }
	else{
	    p_data+=box_size;
	    bytes_left-=box_size;
	}
    }
    return 0;
}



/*reads exactly one RTP sample
 */
#ifdef _MP4Box_
int32_t read_rtp_sample_interface(uint8_t* data, Media_sample_t* attr,int32_t base, int32_t base1,Stbl_t* hint_stbl,Stbl_t* media_stbl, int32_t input_sample_num){
#else
    int32_t read_rtp_sample_interface(uint8_t* data, Media_sample_t* attr,int32_t base, int32_t base1){
#endif
     int32_t extra_flag,data_pos,i,j;
     RTP_whole_pkt_t * start_rtp,*rtp_init;
     uint8_t *data_init;
     Payload_info_t* payload_init;
     int8_t data_ref_idx;//-1 indicates refer from same track, else from the media track corresponding to it

     data_init=data;
     attr->num_rtp_pkts = _read16(data,0);
     attr->rtp = (RTP_whole_pkt_t *)malloc(attr->num_rtp_pkts*sizeof(RTP_whole_pkt_t)); //where are we free'ing this
     data_pos=4;
     start_rtp = attr->rtp;
     for(i=0;i<attr->num_rtp_pkts;i++){
	 attr->rtp->header.relative_time = _read32(data,data_pos);
	 data_pos+=4;
	 attr->rtp->header.rtp_flags1 = _read16(data,data_pos);
	 data_pos+=2;
	 attr->rtp->header.RTPSequenceseed = _read16(data,data_pos);
	 data_pos+=2;
	 attr->rtp->header.rtp_flags2 = _read16(data,data_pos);
	 data_pos++;
	 extra_flag=(data[data_pos]>>2)&0x01;
	 data_pos++;
	 attr->rtp->header.entrycount = _read16(data,data_pos);
	 data_pos+=2;attr->rtp->time_stamp = 0;
	 if(extra_flag){
	     // data_pos+=4;
	     //data_pos+=_read32(data,data_pos);//+4
	     attr->rtp->time_stamp = _read32(data,data_pos+12);
	     data_pos+=_read32(data,data_pos);
	 }
	 attr->rtp->payload=(Payload_info_t*)malloc(sizeof(Payload_info_t)*attr->rtp->header.entrycount);
	 attr->rtp->total_payload_size=0;
	 attr->rtp->num_payloads=attr->rtp->header.entrycount;
	 for(j=0;j<attr->rtp->header.entrycount;j++){
	     attr->rtp->type=data[data_pos];
	     data_pos++;
	     switch(attr->rtp->type){
		 case 0:
		     //Just no-op data
		     attr->rtp->payload->payload_offset=data_pos+base;
		     attr->rtp->payload->payload_size=15;
		     attr->rtp->total_payload_size+=attr->rtp->payload->payload_size;
		     data_pos+=15;
		     break;
		 case 1:
		     attr->rtp->payload->payload_size=data[data_pos];
		     attr->rtp->payload->payload_offset=base+data_pos+1;/*data is directly referred here */ //data[data_pos+1]+base;
		     attr->rtp->total_payload_size+=attr->rtp->payload->payload_size;
		     data_pos+=15;
		     break;
		 case 2:
		     data_ref_idx = data[data_pos];
		     attr->rtp->payload->payload_size=_read16(data,data_pos+1);
		     attr->rtp->payload->sample_num=_read32(data,data_pos+3);
#ifdef _MP4Box_
                     if( attr->rtp->payload->sample_num != input_sample_num+1){
                         int32_t bbase;
                         if(data_ref_idx!=-1)
                             bbase = get_offset_in_file(attr->rtp->payload->sample_num,media_stbl);
                         else
                             bbase = get_offset_in_file(attr->rtp->payload->sample_num,hint_stbl);
                         attr->rtp->payload->payload_offset = _read32(data,data_pos+7)+bbase;
                     }
                     else
#endif
			 attr->rtp->payload->payload_offset = _read32(data,data_pos+7)+ ( (data_ref_idx==-1) * base) +( (data_ref_idx!=-1) *base1);
		     attr->rtp->total_payload_size+=attr->rtp->payload->payload_size;
		     data_pos+=15;
		     break;
		 case 3:
		     attr->rtp->payload->payload_size=0;
		     attr->rtp->total_payload_size+=attr->rtp->payload->payload_size;
		     data_pos+=15;
		     break;
	     }
	     if(j==0)
		 payload_init = attr->rtp->payload;
	     attr->rtp->payload++;
	 }
	 if(i==0)
	     rtp_init = attr->rtp;
	 attr->rtp->payload=payload_init;//payload pointer should be init'ed after every payload loop, it was done after all the packets were read.
	 //this will fail when there are more than one packets
	 attr->rtp++;

     }
     attr->rtp=rtp_init;

     return 1;
 }


#ifdef _MP4Box_
    int32_t get_offset_in_file(int32_t sample_num,Stbl_t *stbl){
	//Cleaner version of find chunk asset:

	int32_t chunk_pos;
	Sample_details_t dummy_prev_sample;
	dummy_prev_sample.is_stateless = 1;
	dummy_prev_sample.total_chunks = dummy_prev_sample.chunk_count = dummy_prev_sample.prev_sample_total = 0;
	chunk_pos = get_smpl_pos(sample_num, stbl,&dummy_prev_sample);
	return chunk_pos;
    }
#endif






 int32_t read_rtp_sample(uint8_t* data, Hint_sample_t* attr,int32_t base, int32_t base1)
{
    int32_t extra_flag,data_pos,i,j;
    RTP_whole_pkt_t * start_rtp,*rtp_init;
    uint8_t *data_init;
    Payload_info_t* payload_init;
    int8_t data_ref_idx;//-1 indicates refer from same track, else from the media track corresponding to it

    data_init=data;
    attr->num_rtp_pkts = _read16(data,0);
    attr->rtp = (RTP_whole_pkt_t *)malloc(attr->num_rtp_pkts*sizeof(RTP_whole_pkt_t)); //where are we free'ing this
    data_pos=4;
    start_rtp = attr->rtp;
    for(i=0;i<attr->num_rtp_pkts;i++){
	attr->rtp->header.relative_time = _read32(data,data_pos);
	data_pos+=4;
	attr->rtp->header.rtp_flags1 = _read16(data,data_pos);
	data_pos+=2;
	attr->rtp->header.RTPSequenceseed = _read16(data,data_pos);
	data_pos+=2;
	attr->rtp->header.rtp_flags2 = _read16(data,data_pos);
	data_pos++;
	extra_flag=(data[data_pos]>>2)&0x01;
	data_pos++;
	attr->rtp->header.entrycount = _read16(data,data_pos);
	data_pos+=2;attr->rtp->time_stamp = 0;
	if(extra_flag){
	    // data_pos+=4;
	    //data_pos+=_read32(data,data_pos);//+4
            attr->rtp->time_stamp = _read32(data,data_pos+12);
            data_pos+=_read32(data,data_pos);
	}
	attr->rtp->payload=(Payload_info_t*)malloc(sizeof(Payload_info_t)*attr->rtp->header.entrycount);
	attr->rtp->total_payload_size=0;
	attr->rtp->num_payloads=attr->rtp->header.entrycount;
	for(j=0;j<attr->rtp->header.entrycount;j++){
	    attr->rtp->type=data[data_pos];
	    data_pos++;
	    switch(attr->rtp->type){
		case 0:
		    //Just no-op data
		    attr->rtp->payload->payload_offset=data_pos+base;
		    attr->rtp->payload->payload_size=15;
		    attr->rtp->total_payload_size+=attr->rtp->payload->payload_size;
		    data_pos+=15;
		    break;
		case 1: 
		    attr->rtp->payload->payload_size=data[data_pos];
		    attr->rtp->payload->payload_offset=base+data_pos+1;/*data is directly referred here */ //data[data_pos+1]+base;
                    attr->rtp->total_payload_size+=attr->rtp->payload->payload_size;
		    data_pos+=15;
		    break;
		case 2:
		    data_ref_idx = data[data_pos];
		    attr->rtp->payload->payload_size=_read16(data,data_pos+1);
		    attr->rtp->payload->sample_num=_read32(data,data_pos+3);
		    attr->rtp->payload->payload_offset = _read32(data,data_pos+7)+ ( (data_ref_idx==-1) * base) +( (data_ref_idx!=-1) *base1);
                    attr->rtp->total_payload_size+=attr->rtp->payload->payload_size;
		    data_pos+=15;
		    break;
		case 3:
		    attr->rtp->payload->payload_size=0;
                    attr->rtp->total_payload_size+=attr->rtp->payload->payload_size;
		    data_pos+=15;
		    break;
	    }
	    if(j==0)
		payload_init = attr->rtp->payload;
	    attr->rtp->payload++;
	}
	if(i==0)
	    rtp_init = attr->rtp;
	attr->rtp->payload=payload_init;//payload pointer should be init'ed after every payload loop, it was done after all the packets were read. 
	                                //this will fail when there are more than one packets
	attr->rtp++;

    }
    attr->rtp=rtp_init;
    
    return 1;
}



int32_t read_rtp_sample_audio(uint8_t* data, Hint_sample_t* attr,int32_t base, int32_t base1,Stbl_t* HintStbl,Stbl_t* MediaStbl,Sample_details_t *hnt_sample,Sample_details_t* mdia_sample)
{
    int32_t extra_flag,data_pos,i,j,base_b;
    RTP_whole_pkt_t * start_rtp,*rtp_init;
    uint8_t *data_init;
    Payload_info_t* payload_init;
    int8_t data_ref_idx;//-1 indicates refer from same track, else from the media track corresponding to it

    //FILE *fid;
    //fid = fopen("old_audio","ab");


    data_init=data;
    attr->num_rtp_pkts = _read16(data,0);
    attr->rtp = (RTP_whole_pkt_t *)malloc(attr->num_rtp_pkts*sizeof(RTP_whole_pkt_t)); //where are we free'ing this
    data_pos=4;
    start_rtp = attr->rtp;
    for(i=0;i<attr->num_rtp_pkts;i++){
        attr->rtp->header.relative_time = _read32(data,data_pos);
        data_pos+=4;
        attr->rtp->header.rtp_flags1 = _read16(data,data_pos);
        data_pos+=2;
        attr->rtp->header.RTPSequenceseed = _read16(data,data_pos);
        data_pos+=2;
        attr->rtp->header.rtp_flags2 = _read16(data,data_pos);
        data_pos++;
        extra_flag=(data[data_pos]>>2)&0x01;
        data_pos++;
        attr->rtp->header.entrycount = _read16(data,data_pos);
        data_pos+=2;attr->rtp->time_stamp = 0;
        if(extra_flag){
            // data_pos+=4;
            //data_pos+=_read32(data,data_pos);//+4
            attr->rtp->time_stamp = _read32(data,data_pos+12);
            data_pos+=_read32(data,data_pos);
        }

        attr->rtp->payload=(Payload_info_t*)malloc(sizeof(Payload_info_t)*attr->rtp->header.entrycount);
        attr->rtp->total_payload_size=0;
        attr->rtp->num_payloads=attr->rtp->header.entrycount;
        for(j=0;j<attr->rtp->header.entrycount;j++){
            attr->rtp->type=data[data_pos];
            data_pos++;
            switch(attr->rtp->type){
                case 0:
                    //Just no-op data
                    attr->rtp->payload->payload_offset=data_pos+base;
                    attr->rtp->payload->payload_size=15;
                    attr->rtp->total_payload_size+=attr->rtp->payload->payload_size;
                    data_pos+=15;
                    break;
                case 1:
                    attr->rtp->payload->payload_size=data[data_pos];
                    attr->rtp->payload->payload_offset=base+data_pos+1;/*data is directly referred here */ //data[data_pos+1]+base;
                    attr->rtp->total_payload_size+=attr->rtp->payload->payload_size;
                    data_pos+=15;
		    break;
                case 2:
                    data_ref_idx = data[data_pos];
                    attr->rtp->payload->payload_size=_read16(data,data_pos+1);
                    attr->rtp->payload->sample_num=_read32(data,data_pos+3);
		    if(data_ref_idx==-1)
			base_b = get_smpl_pos(attr->rtp->payload->sample_num,HintStbl,hnt_sample);
		    else
			base_b = get_smpl_pos(attr->rtp->payload->sample_num,MediaStbl,mdia_sample);
			//		fwrite(&base_b,sizeof(int32_t),1,fid);

                    attr->rtp->payload->payload_offset = _read32(data,data_pos+7)+ base_b;//( (data_ref_idx==-1) * base) +( (data_ref_idx!=-1) *base1);
                    attr->rtp->total_payload_size+=attr->rtp->payload->payload_size;
                    data_pos+=15;
                    break;
                case 3:
                    attr->rtp->payload->payload_size=0;
                    attr->rtp->total_payload_size+=attr->rtp->payload->payload_size;
                    data_pos+=15;
                    break;
            }
            if(j==0)
                payload_init = attr->rtp->payload;
            attr->rtp->payload++;
        }
        if(i==0)
            rtp_init = attr->rtp;
        attr->rtp->payload=payload_init;//payload pointer should be init'ed after every payload loop, it was done after all the packets were read.
                                        //this will fail when there are more than one packets
        attr->rtp++;

    }
    attr->rtp=rtp_init;


    //    fclose(fid);

    return 1;
}

 

int32_t read_rtp_sample_audio_interface(uint8_t* data, Media_sample_t* attr,int32_t base, int32_t base1,Stbl_t* HintStbl,Stbl_t* MediaStbl,Sample_details_t *hnt_sample,Sample_details_t* mdia_sample)
 {
     int32_t extra_flag,data_pos,i,j,base_b;
     RTP_whole_pkt_t * start_rtp,*rtp_init;
     uint8_t *data_init;
     Payload_info_t* payload_init;
     int8_t data_ref_idx;//-1 indicates refer from same track, else from the media track corresponding to it

     //FILE *fid;
     //fid = fopen("old_audio","ab");


     data_init=data;
     attr->num_rtp_pkts = _read16(data,0);
     attr->rtp = (RTP_whole_pkt_t *)malloc(attr->num_rtp_pkts*sizeof(RTP_whole_pkt_t)); //where are we free'ing this
     data_pos=4;
     start_rtp = attr->rtp;
     for(i=0;i<attr->num_rtp_pkts;i++){
	 attr->rtp->header.relative_time = _read32(data,data_pos);
	 data_pos+=4;
	 attr->rtp->header.rtp_flags1 = _read16(data,data_pos);
	 data_pos+=2;
	 attr->rtp->header.RTPSequenceseed = _read16(data,data_pos);
	 data_pos+=2;
	 attr->rtp->header.rtp_flags2 = _read16(data,data_pos);
	 data_pos++;
	 extra_flag=(data[data_pos]>>2)&0x01;
	 data_pos++;
	 attr->rtp->header.entrycount = _read16(data,data_pos);
	 data_pos+=2;attr->rtp->time_stamp = 0;
	 if(extra_flag){
	     // data_pos+=4;
	     //data_pos+=_read32(data,data_pos);//+4
	     attr->rtp->time_stamp = _read32(data,data_pos+12);
	     data_pos+=_read32(data,data_pos);
	 }
	 attr->rtp->payload=(Payload_info_t*)malloc(sizeof(Payload_info_t)*attr->rtp->header.entrycount);
	 attr->rtp->total_payload_size=0;
	 attr->rtp->num_payloads=attr->rtp->header.entrycount;
	 for(j=0;j<attr->rtp->header.entrycount;j++){
	     attr->rtp->type=data[data_pos];
	     data_pos++;
	     switch(attr->rtp->type){
		 case 0:
		     //Just no-op data
		     attr->rtp->payload->payload_offset=data_pos+base;
		     attr->rtp->payload->payload_size=15;
		     attr->rtp->total_payload_size+=attr->rtp->payload->payload_size;
		     data_pos+=15;
		     break;
		 case 1:
		     attr->rtp->payload->payload_size=data[data_pos];
		     attr->rtp->payload->payload_offset=base+data_pos+1;/*data is directly referred here */ //data[data_pos+1]+base;
		     attr->rtp->total_payload_size+=attr->rtp->payload->payload_size;
		     data_pos+=15;
		     break;
		 case 2:
		     data_ref_idx = data[data_pos];
		     attr->rtp->payload->payload_size=_read16(data,data_pos+1);
		     attr->rtp->payload->sample_num=_read32(data,data_pos+3);
		     if(data_ref_idx==-1)
		     base_b = get_smpl_pos(attr->rtp->payload->sample_num,HintStbl,hnt_sample);
		     else
		     base_b = get_smpl_pos(attr->rtp->payload->sample_num,MediaStbl,mdia_sample);
		     //              fwrite(&base_b,sizeof(int32_t),1,fid);

		     attr->rtp->payload->payload_offset = _read32(data,data_pos+7)+ base_b;//( (data_ref_idx==-1) * base) +( (data_ref_idx!=-1) *base1);
		     attr->rtp->total_payload_size+=attr->rtp->payload->payload_size;
		     data_pos+=15;
		     break;
		 case 3:
		     attr->rtp->payload->payload_size=0;
		     attr->rtp->total_payload_size+=attr->rtp->payload->payload_size;
		     data_pos+=15;
		     break;
	     }
	     if(j==0)
		 payload_init = attr->rtp->payload;
	     attr->rtp->payload++;
	 }
	 if(i==0)
	     rtp_init = attr->rtp;
	 attr->rtp->payload=payload_init;//payload pointer should be init'ed after every payload loop, it was done after all the packets were read.
	 //this will fail when there are more than one packets
	 attr->rtp++;

     }
     attr->rtp=rtp_init;


     //    fclose(fid);

     return 1;
 }


int32_t get_smpl_pos(int32_t num_samples_to_send,Stbl_t *stbl,Sample_details_t* prev_sample)
{
    int32_t temp_sam_size,samp_cnt;
    int32_t chunk_num;
    uint8_t* data;
    int32_t sample_size,offsets;
    //Sampl_prop_t* prop;

    if(num_samples_to_send>0)
	num_samples_to_send--;
    else
	num_samples_to_send=0;

    //    if(num_samples_to_send!=prev_sample->sample_num+1)
	//	printf("\n\n\n\n\n\n\nSAMPLES DONT MATCH!!!!!!!!\n\n");

    //GET SIZES OF SAMPLES:
    data=stbl->stsz;
    data+=12;
    temp_sam_size=_read32(data,0);
    data+=4;
    samp_cnt=_read32(data,0);
    //prop->total_sample_size=samp_cnt;
    data+=4;
    if(temp_sam_size==0){
	//data+=4*sample_num;
	//	for(i=0;i<num_samples_to_send;i++){
	//    sample_size[i]=_read32(data,0);
	//    data+=4;
	//	}
	sample_size=_read32(data,num_samples_to_send*4);
    }
    else{
        //for(i=0;i<num_samples_to_send;i++)
	//    sample_size[i]=temp_sam_size;
	sample_size = temp_sam_size;

    }
    //Get chunk in which sample is present.
    data=stbl->stsc;
    //  for(i=0;i<num_samples_to_send;i++)
    //	chunk_num[i]=get_chunk_offsets(sample_num+i,stbl->stsc);

    chunk_num=get_chunk_offsets(num_samples_to_send,stbl->stsc,prev_sample->total_chunks, &prev_sample->chunk_count,&prev_sample->prev_sample_total);
    
    //Get chunk offsets as file offsets:
    data=stbl->stco;
    data+=12;
    //   for(i=0;i<num_samples_to_send;i++){
    //	samp_cnt=chunk_num[i]*4;
    //	offsets[i]=_read32(data,samp_cnt);
    //    }
    samp_cnt=chunk_num*4;
    offsets =_read32(data,samp_cnt); // This is only a base address of the chunk
    //NK:
    if(prev_sample->is_stateless){
        int32_t base_offset,first_sample_of_chunk,sample_num;
        sample_num= num_samples_to_send+0;
        //Find position given a chunk number and
        first_sample_of_chunk=find_first_sample_of_chunk(stbl->stsc,chunk_num);
        //Calculate the offset of the offset of this sample in file.
        base_offset = find_chunk_offset(stbl->stco,chunk_num);
        //Find cumulative offset in chunk to present sample in chunk
        offsets = base_offset + find_cumulative_sample_size(stbl->stsz,first_sample_of_chunk+1,sample_num+1);
        prev_sample->is_stateless = 0;
        prev_sample->chunk_base=base_offset;
    }
    else{
        if(offsets==prev_sample->chunk_base && prev_sample->sample_num > 0){
            offsets= prev_sample->chunk_offset + prev_sample->sample_size;
        }
        else{
            //Base has changed.
            prev_sample->chunk_base=offsets;
        }
    }
    //store this sample's number
    prev_sample->sample_num = num_samples_to_send+1;
    prev_sample->sample_size = sample_size;
    prev_sample->chunk_offset = offsets;
    return offsets;

}

/** reads meta data from the  STTS box 
 *@param data[in] - input data buffers
 *@param ts[in] - time stamp
 *@param tscale[in] - time scaled
 */

int32_t read_stts(uint8_t* data,int32_t ts,int32_t tscale)
{
    uint32_t stts_pos=12;
    int32_t entry_count,delta,i;
    int32_t tim,maxtime=0,sample_num=0,sample_count;


    entry_count = _read32(data,stts_pos);
    stts_pos+=4;
    tim = ts*tscale;
    for(i=0; i<entry_count; i++){
	sample_count = _read32(data,stts_pos);
	stts_pos+=4;
	delta=_read32(data,stts_pos);
	stts_pos+=4;
	maxtime+=sample_count*delta;
	if(tim<maxtime){
	    //      sample_num=tim/delta;
	    maxtime-=sample_count*delta;
	    sample_num+=((tim-maxtime)/delta);
	    if(sample_num==0)
		sample_num=1;
	    break;
	}
	sample_num+=sample_count;
    }
    return sample_num;
}

/** reads meta data from the tkhd box
 *@param data[in] - input data buffers
 *@param sk[in] - container for seek meta data
 */
int32_t read_tkhd(uint8_t* p_dat, Seek_t* sk)
{
    uint8_t version;
    uint32_t pos=0,track_id;
    version=p_dat[pos];
    pos+=4;
    if(version)
        pos+=16;
    else
	pos+=8;
    track_id=(p_dat[pos]<<24)|(p_dat[pos+1]<<16)|(p_dat[pos+2]<<8)|p_dat[pos+3];
    pos+=4;
    sk->seek_trak_info+=sk->num_traks;
    sk->seek_trak_info->trak_id=track_id;    
    //    sk->num_traks++;increment in hdlr
    return 1;
}

/** returns the track id if the data is pointed to the right box (USE CAREFULLY)
 *@param p_dat[in] - input data buffers
 *@returns the track id
 */
int32_t return_track_id(uint8_t *p_dat){
    uint8_t version;
    uint32_t pos=0,track_id;
    version=p_dat[pos];
    pos+=4;
    if(version)
        pos+=16;
    else
        pos+=8;
    track_id=(p_dat[pos]<<24)|(p_dat[pos+1]<<16)|(p_dat[pos+2]<<8)|p_dat[pos+3];
    return track_id;
}


void free_all(Hint_sample_t* hnt){
    free(hnt);


}
/*
inline uint32_t _read32(uint8_t *buf, int32_t pos)
{
    return (buf[pos]<<24)|(buf[pos+1]<<16)|(buf[pos+2]<<8)|(buf[pos+3]);
}

inline uint16_t _read16(uint8_t *buf, int32_t pos)
{
    return (buf[pos]<<8)|(buf[pos+1]);
}
*/

int32_t find_trak_type(Media_stbl_t* mstbl,int32_t pid)
{
    if(mstbl->HintAudStbl.trak_type == pid)
	return AUDIO_HINT_TRAK;
    if(mstbl->HintVidStbl.trak_type == pid)
	return VIDEO_HINT_TRAK;
    if(mstbl->VideoStbl.trak_type == pid)
	return VIDEO_TRAK;
    if(mstbl->AudioStbl.trak_type == pid)
        return AUDIO_TRAK;
    return -1;
}

void get_stream_info(Media_stbl_t* media_stbl, Stream_info_t *stream)
{
    stream->num_traks = 0;
    stream->track_type[0]=-1; stream->track_type[1]=-1;

    if((media_stbl->VideoStbl.stsc_tab.size != -1)&&(media_stbl->VideoStbl.stsz_tab.size !=-1)){
        stream->num_traks++;
        stream->track_type[0]=VIDEO_TRAK;
    }
    if((media_stbl->AudioStbl.stsc_tab.size != -1)&&(media_stbl->AudioStbl.stsz_tab.size !=-1)){
        stream->num_traks++;
        stream->track_type[1]=AUDIO_TRAK;
    }
}


void free_all_stbls(Media_stbl_t* media_stbl)
{
    /* Stbl_t VideoStbl;
    Stbl_t AudioStbl;
    Stbl_t HintVidStbl;
    Stbl_t HintAudStbl;
    */
    free_stbl(&media_stbl->VideoStbl);
    free_stbl(&media_stbl->AudioStbl);
    free_stbl(&media_stbl->HintVidStbl);
    free_stbl(&media_stbl->HintAudStbl);
}

void free_stbl(Stbl_t * stbl){
    if(stbl->stsz !=NULL)
        free(stbl->stsz);
    if(stbl->stco !=NULL)
        free(stbl->stco);
    if(stbl->stts !=NULL)
        free(stbl->stts);
    if(stbl->ctts !=NULL)
        free(stbl->ctts);
    if(stbl->stsc !=NULL)
        free(stbl->stsc);
    if(stbl->stsd !=NULL)
        free(stbl->stsd);
    if(stbl->stss !=NULL)
	free(stbl->stss);
    if(stbl->esds !=NULL)
        free(stbl->esds);
    
}


void free_ctx(send_ctx_t *st){
    if(st->vid_s!=NULL)
	free(st->vid_s);
    if(st->aud_s!=NULL)
        free(st->aud_s);
    if(st->hvid_s!=NULL)
        free(st->hvid_s);
    if(st->haud_s!=NULL)
        free(st->haud_s);
}

/* depraceted */
all_info_t*
init_all_info()
    {
	all_info_t* inf = (all_info_t*)calloc(1, sizeof(all_info_t));

	Seekinfo_t* sinfo = inf->seek_state_info;
	Media_stbl_t *mstbl = inf->stbl_info;
	Media_sample_t* videoh = inf->videoh;
	Media_sample_t* audioh = inf->audioh;
	Media_sample_t* video = inf->video;
	Media_sample_t* audio = inf->audio;

	inf->sample_prop_info = (send_ctx_t*)malloc(sizeof(send_ctx_t));
	inf->sample_prop_info->vid_s = (Sampl_prop_t*)calloc(1, sizeof(Sampl_prop_t));
	inf->sample_prop_info->aud_s = (Sampl_prop_t*)calloc(1, sizeof(Sampl_prop_t));
	inf->sample_prop_info->hvid_s = (Sampl_prop_t*)calloc(1, sizeof(Sampl_prop_t));
	inf->sample_prop_info->haud_s = (Sampl_prop_t*)calloc(1, sizeof(Sampl_prop_t));

	inf->videoh = (Media_sample_t*)malloc(SAMPLES_TO_BE_SENT*sizeof(Media_sample_t));
	inf->audioh = (Media_sample_t*)malloc(SAMPLES_TO_BE_SENT*sizeof(Media_sample_t));
	inf->video = (Media_sample_t*)malloc(SAMPLES_TO_BE_SENT*sizeof(Media_sample_t));
	inf->audio = (Media_sample_t*)malloc(SAMPLES_TO_BE_SENT*sizeof(Media_sample_t));

	//inf->videoh->rtp = (RTP_whole_pkt_t*)malloc(sizeof(RTP_whole_pkt_t));
	//inf->audioh->rtp = (RTP_whole_pkt_t*)malloc(sizeof(RTP_whole_pkt_t));
	//inf->video->rtp =(RTP_whole_pkt_t*)malloc(sizeof(RTP_whole_pkt_t));
	//inf->audio->rtp = (RTP_whole_pkt_t*)malloc(sizeof(RTP_whole_pkt_t));

	inf->seek_state_info=(Seekinfo_t*)malloc(sizeof(Seekinfo_t));
	inf->stbl_info=(Media_stbl_t *)calloc(1,sizeof(Media_stbl_t));

	//    Stream_info_t *stream;
	//stream =  (Stream_info_t *)calloc(1,sizeof( Stream_info_t));

	return inf;
    }

 int32_t cleanup_output_details(output_details_t* output)
 {
     if (output->rtp->payload != NULL)
         {
             free(output->rtp->payload);
             output->rtp->payload = NULL;
         }
     if (output->rtp != NULL)
         {
             free(output->rtp);
             output->rtp = NULL;
         }
     if (output != NULL)
         {
             free(output);
             output = NULL;
         }
     return 1;
 }


int32_t free_all_info_unhinted(all_info_t* st_all_info)
{
    if(st_all_info->seek_state_info!= NULL)
	{
	    free(st_all_info->seek_state_info);
	}
    if(st_all_info->stbl_info!= NULL)
	{
	    free(st_all_info->stbl_info);
	}
#if 0
    if(st_all_info->videoh->rtp!= NULL)
      {
          free(st_all_info->videoh->rtp);
      }
    if(st_all_info->audioh->rtp!= NULL)
      {
            free(st_all_info->audioh->rtp);
        }
    if(st_all_info->video->rtp!= NULL)
      {
          free(st_all_info->video->rtp);
      }
    if(st_all_info->audio->rtp!= NULL)
      {
          free(st_all_info->audio->rtp);
      }
#endif
    if(st_all_info->videoh!= NULL)
	{
	    free(st_all_info->videoh);
	}
    if(st_all_info->audioh!= NULL)
	{
	    free(st_all_info->audioh);
	}
    if(st_all_info->video!= NULL)
	{
	    free(st_all_info->video);
	}
    if(st_all_info->audio!= NULL)
	{
	    free(st_all_info->audio);
	}
    if(st_all_info->sample_prop_info->haud_s!= NULL)
        {
            free(st_all_info->sample_prop_info->haud_s);
        }
    if(st_all_info->sample_prop_info->hvid_s!= NULL)
        {
            free(st_all_info->sample_prop_info->hvid_s);
        }
    if(st_all_info->sample_prop_info->aud_s!= NULL)
        {
            free(st_all_info->sample_prop_info->aud_s);
        }
    if(st_all_info->sample_prop_info->vid_s!= NULL)
        {
            free(st_all_info->sample_prop_info->vid_s);
        }
    if(st_all_info->sample_prop_info!= NULL)
        {
            free(st_all_info->sample_prop_info);
        }

    if(st_all_info) {
        free(st_all_info);
        st_all_info = NULL;
    }
    return 1;
}


 int32_t free_all_info(all_info_t* st_all_info)
 {
     if(st_all_info->seek_state_info!= NULL)
	 {
	     free(st_all_info->seek_state_info);
	 }
     if(st_all_info->stbl_info!= NULL)
         {
             free(st_all_info->stbl_info);
         }

     //if(st_all_info->videoh->rtp->payload!= NULL)
     // {
     //    free(st_all_info->videoh->rtp->payload);
     //	}
     //if(st_all_info->audioh->rtp->payload!= NULL)
     // {
     //	     free(st_all_info->audioh->rtp->payload);
     //	 }

     // if(st_all_info->videoh->rtp!= NULL)
     //  {
     //      free(st_all_info->videoh->rtp);
     //  }
     //if(st_all_info->audioh->rtp!= NULL)
     ///  {
     ///        free(st_all_info->audioh->rtp);
     //    }
     //if(st_all_info->video->rtp!= NULL)
     //  {
     //      free(st_all_info->video->rtp);
     //  }
     //if(st_all_info->audio->rtp!= NULL)
     //  {
     //      free(st_all_info->audio->rtp);
     //  }
     if(st_all_info->videoh!= NULL)
         {
             free(st_all_info->videoh);
         }
     if(st_all_info->audioh!= NULL)
         {
             free(st_all_info->audioh);
         }
     if(st_all_info->video!= NULL)
         {
             free(st_all_info->video);
         }
     if(st_all_info->audio!= NULL)
         {
             free(st_all_info->audio);
         }
    if(st_all_info->sample_prop_info->haud_s!= NULL)
	{
	    free(st_all_info->sample_prop_info->haud_s);
	}
    if(st_all_info->sample_prop_info->hvid_s!= NULL)
        {
            free(st_all_info->sample_prop_info->hvid_s);
        }
    if(st_all_info->sample_prop_info->aud_s!= NULL)
        {
            free(st_all_info->sample_prop_info->aud_s);
        }
    if(st_all_info->sample_prop_info->vid_s!= NULL)
        {
            free(st_all_info->sample_prop_info->vid_s);
        }
    if(st_all_info->sample_prop_info!= NULL)
        {
            free(st_all_info->sample_prop_info);
        }

    if(st_all_info) {
	free(st_all_info);
	st_all_info = NULL;
    }
     return 1;
     }


 int32_t  getesds(all_info_t *st_all_info,int32_t trak_type,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* fid,int32_t *esds_offset)
 {
     box_t box;
     size_t bytes_read, total_bytes_read = 0;
     int32_t box_size,size = 10000;
     char esds_id[] = {'e', 's', 'd', 's'};
     int32_t esds_size;
     uint8_t *data, *ptr;

     data = NULL;
     ptr = 0;
     if (trak_type == AUDIO_TRAK)
	 {

	     ptr = data=(uint8_t*)malloc((st_all_info->stbl_info->AudioStbl.stsd_tab.size)*sizeof(uint8_t));
	     nkn_vfs_fseek(fid,(st_all_info->stbl_info->AudioStbl.stsd_tab.offset),SEEK_SET);
	     (*nkn_fread)(data,st_all_info->stbl_info->AudioStbl.stsd_tab.size,1,fid);
	     data+=52;
	     while(total_bytes_read < (size_t)st_all_info->stbl_info->AudioStbl.stsd_tab.size)
		 {
		     box_size = read_next_root_level_box(data, size, &box, &bytes_read);
		     total_bytes_read += box_size;
		     if(check_box_type(&box,esds_id))
			 {
			     *esds_offset = st_all_info->stbl_info->AudioStbl.stsd_tab.offset + 51;//box_size;
			     // stbl_data+=bytes_read;
			     esds_size = box_size;
			     data+=box_size;
			     goto cleanup;
			 }
		 }
	     *esds_offset = -1;
	     esds_size = -1;
	     
	 }

 cleanup:
     if(ptr) {
	 free(ptr);
	 ptr = NULL;
     }

     return esds_size;
 }
 int32_t  getavcc(all_info_t *st_all_info,int32_t trak_type,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* fid,int32_t *avcc_offset)
 {
     box_t box;
     size_t bytes_read, total_bytes_read = 0;
     int32_t box_size,size = 10000;//initial_data;
     char avc1_id[] = {'a', 'v', 'c', '1'};
     char avcC_id[] = {'a', 'v', 'c', 'C'};
     int32_t avc1_size,avcC_size;
     uint8_t *data,*initial_data;

     if (trak_type == VIDEO_TRAK)
	 {
	     data=(uint8_t*)malloc((st_all_info->stbl_info->VideoStbl.stsd_tab.size)*sizeof(uint8_t));
	     initial_data =data;
	     nkn_vfs_fseek(fid,(st_all_info->stbl_info->VideoStbl.stsd_tab.offset),SEEK_SET);
	     (*nkn_fread)(data,st_all_info->stbl_info->VideoStbl.stsd_tab.size,1,fid);
	     data+=16;
	     while(total_bytes_read < (size_t)st_all_info->stbl_info->VideoStbl.stsd_tab.size)
		 {
		     box_size = read_next_root_level_box(data, size, &box, &bytes_read);
		     total_bytes_read += box_size;
		     if(check_box_type(&box,avc1_id))
			 {
			     avc1_size = box_size;
			     data+=86;
			     box_size = read_next_root_level_box(data, size, &box, &bytes_read);
			     //total_bytes_read += box_size;
			     if(check_box_type(&box,avcC_id))
				 {
				     *avcc_offset = st_all_info->stbl_info->VideoStbl.stsd_tab.offset + (data - initial_data);
				     avcC_size = box_size;
				     data+=box_size;
				     if(initial_data != NULL)
					 free(initial_data);
				     return avcC_size;
				 }

			 }
		 }
	     *avcc_offset = -1;
	     avcC_size = -1;
	     
	 }
     if(initial_data != NULL)
	 free(initial_data);
     return avcC_size;
 }

int32_t Init(all_info_t* st_all_info, size_t (*nkn_fread)( void *, size_t, size_t, FILE*), FILE *fid){

    Seekinfo_t* sinfo = st_all_info->seek_state_info;
    Media_stbl_t *mstbl = st_all_info->stbl_info;
    send_ctx_t *st = st_all_info->sample_prop_info;
    int32_t ret;

    init_seek_info(sinfo);

    ret = send_next_pkt_init(sinfo,nkn_fread,fid,mstbl,st);

    if (st_all_info->stbl_info->HintVidStbl.stsz_tab.size != -1)
	{
	    st_all_info->timescale_video = st_all_info->stbl_info->HintVidStbl.timescale;
	}
    else 
	{
	    st_all_info->timescale_video = st_all_info->stbl_info->VideoStbl.timescale;
	}
    if (st_all_info->stbl_info->HintAudStbl.stsz_tab.size != -1)
	{
	    st_all_info->timescale_audio = st_all_info->stbl_info->HintAudStbl.timescale;
	}
    else
	{
	    st_all_info->timescale_audio = st_all_info->stbl_info->AudioStbl.timescale;
	}
    return ret;

}

void seek2time_hinted(all_info_t *st_all_info,int32_t seek_time,int32_t track_type,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* fid)
{
   
    Seek_params_t* seek_params1;
    Seekinfo_t* sinfo = st_all_info->seek_state_info;
    Media_stbl_t *mstbl = st_all_info->stbl_info;
    send_ctx_t *st = st_all_info->sample_prop_info;
    seek_params1 =  (Seek_params_t*)malloc(sizeof(Seek_params_t));
    seek_params1->seek_time = seek_time;
    //st=(send_ctx_t *)malloc(sizeof(send_ctx_t *));
    send_next_pkt_seek2time_hinted(sinfo,nkn_fread,fid,mstbl,st,track_type,seek_params1);
    return;
}


 void seek2time(all_info_t *st_all_info,int32_t seek_time,int32_t track_type,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* fid)
 {

     Seek_params_t* seek_params1;
     Seekinfo_t* sinfo = st_all_info->seek_state_info;
     Media_stbl_t *mstbl = st_all_info->stbl_info;
     send_ctx_t *st = st_all_info->sample_prop_info;
     seek_params1 =  (Seek_params_t*)malloc(sizeof(Seek_params_t));
     seek_params1->seek_time = seek_time;
     if(track_type == VIDEO_TRAK)
	 {
	     if(st_all_info->stbl_info->HintVidStbl.stsz_tab.size != -1)
		 {
		     send_next_pkt_seek2time_hinted(sinfo,nkn_fread,fid,mstbl,st,track_type,seek_params1);
		 }
	     else
		 {
                     send_next_pkt_seek2time(sinfo,nkn_fread,fid,mstbl,st,track_type,seek_params1);
                 }
	 }
     if(track_type == AUDIO_TRAK)
         {
             if(st_all_info->stbl_info->HintAudStbl.stsz_tab.size != -1)
                 {
                     send_next_pkt_seek2time_hinted(sinfo,nkn_fread,fid,mstbl,st,track_type,seek_params1);
                 }
             else
                 {
                     send_next_pkt_seek2time(sinfo,nkn_fread,fid,mstbl,st,track_type,seek_params1);
                 }
         }
     if(seek_params1 != NULL)
	 SAFE_FREE(seek_params1);

     return;
 }


 int32_t send_next_pkt_hinted(all_info_t *st_all_info,int32_t trak_type,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),size_t (*nkn_fread1)( void *, size_t, size_t, FILE*, void **, int *), FILE* fid,Hint_sample_t *audiopkts, Hint_sample_t *videopkts )
{
    Seekinfo_t* sinfo = st_all_info->seek_state_info;
    Media_stbl_t *mstbl = st_all_info->stbl_info;
    //send_ctx_t **st = st_all_info->sample_prop_info;
    Media_sample_t* videoh = st_all_info->videoh;
    Media_sample_t* audioh = st_all_info->audioh;
    Media_sample_t* video = st_all_info->video;
    Media_sample_t* audio = st_all_info->audio;
    send_ctx_t *st = st_all_info->sample_prop_info;
    int32_t rv =0;

    rv = send_next_pkt_interface(sinfo,nkn_fread, nkn_fread1, fid,mstbl,st,videoh,audioh,trak_type);
    if (trak_type == VIDEO_TRAK)
	{
	    st_all_info->look_ahead_time = st_all_info->sample_prop_info->hvid_s->look_ahead_time ;
	}
    if (trak_type == AUDIO_TRAK)
        {
            st_all_info->look_ahead_time = st_all_info->sample_prop_info->haud_s->look_ahead_time ;
        }

    audiopkts = (Hint_sample_t *)st_all_info->audioh;
    videopkts = (Hint_sample_t *)st_all_info->videoh;
    return rv;

}
int32_t get_next_sample(all_info_t *st_all_info,int32_t trak_type,/*int32_t sample_num,*/size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* fid,int32_t *look_ahead_time , /*RTP_whole_pkt_t *rtp_pkt_data*/output_details_t *output )
{

    Seekinfo_t* sinfo = st_all_info->seek_state_info;
    Media_stbl_t *mstbl = st_all_info->stbl_info;
    //send_ctx_t **st = st_all_info->sample_prop_info;
    Media_sample_t* videoh = st_all_info->videoh;
    Media_sample_t* audioh = st_all_info->audioh;
    Media_sample_t* video = st_all_info->video;
    Media_sample_t* audio = st_all_info->audio;
    send_ctx_t *st = st_all_info->sample_prop_info;
    int32_t rv =0;

    rv = send_next_pkt_get_sample(sinfo,nkn_vpe_buffered_read,fid,mstbl,st,videoh,audioh,video,audio,trak_type);
    if (trak_type == AUDIO_HINT_TRAK)
	{

	    *output->rtp = *st_all_info->audioh->rtp;
	    output->num_bytes = st_all_info->seek_state_info->HintA.num_bytes[0];
	    output->offset = st_all_info->seek_state_info->HintA.offset[0];
	    output->num_samples_sent = st_all_info->seek_state_info->HintA.num_samples_sent;
	    *look_ahead_time = st_all_info->sample_prop_info->haud_s->look_ahead_time;

	}
    if (trak_type == VIDEO_HINT_TRAK)
	{
	    *output->rtp = *st_all_info->videoh->rtp;
	    output->num_bytes = st_all_info->seek_state_info->HintV.num_bytes[0];
	    output->offset = st_all_info->seek_state_info->HintV.offset[0];
	    output->num_samples_sent = st_all_info->seek_state_info->HintV.num_samples_sent;
	    *look_ahead_time = st_all_info->sample_prop_info->hvid_s->look_ahead_time;

	}
    if (trak_type == AUDIO_TRAK)
	{

	    output->rtp = NULL;
	    output->num_bytes = st_all_info->seek_state_info->Audio.num_bytes[0];
	    output->offset = st_all_info->seek_state_info->Audio.offset[0];
	    output->num_samples_sent = st_all_info->seek_state_info->Audio.num_samples_sent;
	    *look_ahead_time = st_all_info->sample_prop_info->aud_s->look_ahead_time;
	    st_all_info->look_ahead_time = st_all_info->sample_prop_info->aud_s->look_ahead_time ;
	}
    if (trak_type == VIDEO_TRAK)
	{

	    output->rtp = NULL;
	    output->num_bytes = st_all_info->seek_state_info->Video.num_bytes[0];
	    output->offset = st_all_info->seek_state_info->Video.offset[0];
	    output->num_samples_sent = st_all_info->seek_state_info->Video.num_samples_sent;
	    *look_ahead_time = st_all_info->sample_prop_info->vid_s->look_ahead_time;
	    st_all_info->look_ahead_time = st_all_info->sample_prop_info->vid_s->look_ahead_time ;
            output->ctts_offset =st_all_info->sample_prop_info->vid_s->ctts_offset ;

	}
    return rv;


}
#endif//_SUMALATHA_OPT_MAIN_



