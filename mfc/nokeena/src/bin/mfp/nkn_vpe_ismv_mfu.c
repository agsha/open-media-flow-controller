
#include <byteswap.h>
#include "nkn_vpe_mfu_aac.h"
#include "nkn_vpe_raw_h264.h"


//#include "nkn_vpe_ismv_mfu.h"


#define MOOV_SEARCH_SIZE (32*1024)


/***********************************************************
   Function: Wrapper function to convert an ismv file to mfu

************************************************************/

int32_t ismv_to_mfu(char *infile,char* outfile){
    FILE *fp,*fout;
    size_t mp4_size, mfra_off, moov_offset, moov_size, tmp_size,
	mfra_size, moof_size, moof_offset;
    uint8_t mfro[16], moov_search_data[MOOV_SEARCH_SIZE], *moov_data,
	*mfra_data;
    moov_t *moov;
    uint32_t  n_traks;
    ismv_parser_ctx_t *ctx;
    ismv_parser_t *parser_data;
    mp4_size = 0;
    fp = fout = NULL;
    fp = fopen(infile, "rb");
    fout = fopen(outfile,"wb");
    if(!fp||!fout) {
	printf("Error opening input or output file\n");
	return 0;
    }
    fseek(fp, 0, SEEK_END);
    mp4_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
	
    fseek(fp, 0, SEEK_SET);
    fread(moov_search_data, 1, MOOV_SEARCH_SIZE, fp);
	
    /* read information about the moov box */
    mp4_get_moov_info(moov_search_data, MOOV_SEARCH_SIZE,
		      &moov_offset, &moov_size);

    if(!moov_offset) {
	printf("moov box not at the top of the file, cannot proceed \n");
	return 0;
    }

    /* read the moov data into memory */
    fseek(fp,moov_offset, SEEK_SET);
    moov_data = (uint8_t*)calloc(1, moov_size);
    fread(moov_data, 1, moov_size, fp);

    /* initialize the moov box */
    moov = mp4_init_moov((uint8_t*)moov_data, moov_offset, moov_size);

    /* initialize the trak structure for the mp4 data */
    n_traks = mp4_populate_trak(moov);
    if(!n_traks) {
	printf("there shoud be atleast one trak; cannot proceed\n");
	return 0;
    }
    
    /* for fragmented MP4 files, the mfro box is usually the last box
     * in the file and is of fixed size. the mfro box gives the offset
     * in the mp4 file for the mfra box. the mfra box in turn has
     * information about the timestamp and offset of each moof
     * fragment
     */

    /* read the mfro box */
    fseek(fp, -16, SEEK_END);
    fread(mfro, 1, 16, fp);

    /* read the mfra offset from the mfro box */
    mfra_off = mp4_size - mp4_get_mfra_offset(mfro, 16);
    if(mfra_off == mp4_size) {
	printf("no mfro box; cannot proceed \n");
	return 0;
    }
    
    /* read the mfra box */
    tmp_size = mp4_size - mfra_off;
    mfra_data = (uint8_t*)malloc(tmp_size);
    fseek(fp, mfra_off, SEEK_SET);
    fread(mfra_data, 1, tmp_size, fp);


    /* get exact mfra box size */
    mfra_size = mp4_get_mfra_size(mfra_data, tmp_size);
    

    /* initialize the ismv context */
    ctx = mp4_init_ismv_parser_ctx(mfra_data, mfra_size, moov);

    /*Prepare the MFU parser data*/
    parser_data = (ismv_parser_t*)calloc(1,sizeof(ismv_parser_t));
    parser_data->n_traks = n_traks;
    parser_data->fin = fp;
    init_mfu_parser(parser_data,moov);


    if(ismv_dump_mfu(ctx, &moof_offset, &moof_size,parser_data) == VPE_ERROR)
	printf("error in dumping to MFU:Exitting .....\n");
    if(parser_data != NULL)
	free(parser_data);
    mp4_cleanup_ismv_ctx(ctx);
    mp4_moov_cleanup(moov, moov->n_tracks);
    free(moov_data);
    //    free(ism_data);
    free(mfra_data);

    fclose(fp);
    return 0;
}

/*************************************************
  Find number of audio and video tracks in file
**************************************************/
int32_t init_mfu_parser(ismv_parser_t* parser_data,moov_t* moov){
    int32_t i,trak_type;
    trak_t  *trak;
    parser_data->vid_trak_num =parser_data->aud_trak_num = 0;
    for(i=0;i<parser_data->n_traks;i++){
        trak = moov->trak[i];
        trak_type = *((int32_t*)(&trak->mdia->hdlr->handler_type));
	if(trak_type == VIDE_HEX){
	    parser_data->n_vid_traks++;
	}
	if(trak_type == SOUN_HEX){
            parser_data->n_aud_traks++;
	    parser_data->audio[i].esds_flag = 0;
	    parser_data->audio[i].adts = (adts_t*)calloc(sizeof(adts_t),1);
	}
    }
    return VPE_SUCCESS;
}



/**************************************************
  Description:
  Inputs:
  Output:
**************************************************/
uint32_t ismv_dump_mfu(ismv_parser_ctx_t *ctx, size_t *moof_offset, size_t *moof_size,ismv_parser_t* parser_data){

    /*Declarations*/
    int32_t i,trak_type,n_traks;
    moov_t *moov;
    trak_t  *trak;
    size_t moof_time;

    moov = ctx->moov;
    n_traks = parser_data->n_traks;
    /*Initialize the ISMV parser data*/
    if(init_ismv_mfu_parser(parser_data) == VPE_ERROR)
	return VPE_ERROR;

    /*Start dumping the individual files*/
    for(i=0;i<n_traks;i++){
	trak = moov->trak[i];
	trak_type = *((int32_t*)(&trak->mdia->hdlr->handler_type));

	/*Seek to the 1st sample - this is time = 0*/
	/* NOTE:change this logic later based on moofs. We need to sek to the first moof.Might fail if the first time stamp is not 0!*/
	mp4_frag_seek(ctx, moov->trak[i]->tkhd->track_id, 0, moov->trak[i]->mdia->mdhd->timescale,  moof_offset, moof_size,&moof_time);

	/*Handle the video and audio headr codes*/	

	switch(trak_type){
	    case VIDE_HEX:
		if(capture_video_headers(moov->trak[i]->stbl,parser_data) == VPE_ERROR)
		    return VPE_ERROR;
		break;
	    case SOUN_HEX:
		if(capture_audio_headers(moov->trak[i]->stbl,&parser_data->audio[parser_data->aud_trak_num]) == VPE_ERROR)
                    return VPE_ERROR;
		break;
	    default:
		break;
	}



	/*Handle the moof of each track separately*/
	if(handle_ismv_track(ctx,moof_offset, moof_size,parser_data,trak_type) == VPE_ERROR)
	    return VPE_ERROR;

	/*clean the video and audio headr codes*/

        switch(trak_type){
            case VIDE_HEX:
                if(clean_video_headers(parser_data) == VPE_ERROR)
                    return VPE_ERROR;
		parser_data->vid_trak_num++;
                break;

            case SOUN_HEX:
                if(clean_audio_headers(parser_data) == VPE_ERROR)
                    return VPE_ERROR;
		parser_data->aud_trak_num++;
                break;
            default:
                break;
        }



    }
    return VPE_SUCCESS;
}


int32_t capture_video_headers(stbl_t* stbl,ismv_parser_t* parser_data){
    int32_t present_video_trak;
    uint8_t *stbl_data;
    present_video_trak = parser_data->vid_trak_num;
    stbl_data = stbl->data;
    parser_data->video[present_video_trak].stbl_size = stbl->size;
    parser_data->video[present_video_trak].stbl_offset = stbl->pos +24;

    //allocating memory for video stream avc1
    init_ismv_ind_video_stream(&parser_data->video[present_video_trak]);
    //opening the .h264 file
    allocate_video_streams(&parser_data->video[present_video_trak],present_video_trak);
    //getting the stsd box offset,size and position
    get_stsd_info(stbl_data, &parser_data->video[present_video_trak]);
    //getting the avcC box offset,size and position
    get_avcC_info(parser_data->fin,fread,&parser_data->video[present_video_trak]);
    if(parser_data->video[present_video_trak].codec_type != 1/*H264_VIDEO*/)
        {
            printf("This video is not H264 supported \n");
            return 0;
        }
    //get the pps and sps data for video stream and update in the avc1 structure
    get_pps_sps(parser_data->fin,fread,&parser_data->video[present_video_trak]);

    return VPE_SUCCESS;

}


/**********************************************************************************
  Function:
            Extracts the necessary elements for each audio track from the stbl:
            1. Check for codec compatibility.
            2. Extract the information from stsd
            3. Update th information in the parser_data->audio[present_aud_trak_no]
  Inputs:
            Stbl (which contains stsd)
            Parser data(contains present parser structure and all trak specific info)
  Outputs:
            Written into Parser data structure

**********************************************************************************/
int32_t capture_audio_headers(stbl_t* stbl,audio_pt* audio){//ismv_parser_t* parser_data){

    if(get_aac_headers(stbl,(adts_t*)(audio->adts)) == VPE_ERROR)
	return VPE_ERROR;
    form_aac_header((adts_t*)(audio->adts),&audio->header,&audio->header_size);

    return VPE_SUCCESS;
}



uint32_t handle_ismv_track(ismv_parser_ctx_t *ctx, size_t *moof_offset, size_t *moof_size,ismv_parser_t* parser_data, int32_t trak_type){
    uint8_t *moof_data=NULL;
    uint32_t ret = VPE_SUCCESS;
    size_t moof_time;
    switch(trak_type){
	case VIDE_HEX:
	    do{
		/*moof includes moof and mdat-read the same*/
		fseek(parser_data->fin,*moof_offset,SEEK_SET); 
		moof_data = (uint8_t*)malloc(sizeof(uint8_t)**moof_size);
		fread(moof_data,*moof_size,sizeof(uint8_t),parser_data->fin);
		if(handle_video_moof(ctx->moov,moof_data,parser_data) == VPE_ERROR){
		    ret = VPE_ERROR;
                    free(moof_data);
                    goto returncode;
                }
                free(moof_data);
	    }while(mp4_get_next_frag(ctx, moof_offset, moof_size, &moof_time) != E_ISMV_NO_MORE_FRAG);
            //parser_data->vid_trak_num++;
	    break;
	case SOUN_HEX:
	    parser_data->audio[parser_data->aud_trak_num].fout = fopen("test.aac","wb");
	    do{
		/*moof includes moof and mdat-read the same*/
		fseek(parser_data->fin,*moof_offset,SEEK_SET);
		moof_data = (uint8_t*)malloc(sizeof(uint8_t)**moof_size);
		fread(moof_data,*moof_size,sizeof(uint8_t),parser_data->fin);
		if(handle_audio_moof(moof_data,&parser_data->audio[parser_data->aud_trak_num]) == VPE_ERROR){
		    ret = VPE_ERROR;
		    free(moof_data);
		    goto returncode;
		}
		if(moof_data!=NULL){
		    free(moof_data);
		    moof_data = NULL;
		}
	    }while(mp4_get_next_frag(ctx, moof_offset, moof_size, &moof_time) != E_ISMV_NO_MORE_FRAG);
	    fclose(parser_data->audio[parser_data->aud_trak_num].fout);
            //parser_data->aud_trak_num++;
	    break;
	default:
	    break;
        }
 returncode:
    return ret;
}



int32_t handle_audio_moof(uint8_t * moof,audio_pt* audio){//ismv_parser_t* parser_data){
    /*
      1. Obtain each audio sample
      2. for each sample of moof append 7 byte adts header.
      3. Dump the sample with headers. 
    */

    int32_t i;
    mdat_desc_t* md;
    uint8_t* mdat;
    /*Obtain mdat descriptor*/
    md=( mdat_desc_t*)malloc(sizeof( mdat_desc_t));
    get_mdat_desc(moof, md);   

    /*Dump Each sample*/

    mdat = md->mdat_pos+8;
    if(md->default_sample_size){
	for(i=0; i<md->sample_count;i++){
            dump_aac_unit(mdat,md->default_sample_size,audio->header,audio->header_size,audio->fout);
            mdat+=md->default_sample_size;
	}
    }
    else{
        for(i=0; i<md->sample_count;i++){
	    dump_aac_unit(mdat,md->sample_sizes[i],audio->header,audio->header_size,audio->fout);
	    mdat+=md->sample_sizes[i];
        }
    }
    return VPE_SUCCESS;
}


int32_t handle_video_moof(moov_t* moov, uint8_t * moof,ismv_parser_t* parser_data){
    mdat_desc_t* mdat_desc;
    int32_t present_video_trak = parser_data->vid_trak_num;
    stream_AVC_conf_Record_t * avc1 = parser_data->video[present_video_trak].avc1;
    FILE *fout = parser_data->video[present_video_trak].fout;
    //write pps and sps first time for each track
    if(parser_data->video[present_video_trak].dump_pps_sps ==1){
            write_sps_pps(avc1,fout);
            parser_data->video[present_video_trak].dump_pps_sps = 0;
    }
    //initialise mdat descriptor
    mdat_desc = init_mdat_desc();
    get_mdat_desc(moof, mdat_desc);
    //write h264 samples
    write_h264_samples(mdat_desc,moof,fout);
    free_mdat_desc(mdat_desc);
    return VPE_SUCCESS;
}

/* initialise the mdat descriptor*/
mdat_desc_t* init_mdat_desc(){
    mdat_desc_t* mdat_desc;
    mdat_desc = (mdat_desc_t*)calloc(1,sizeof(mdat_desc_t));
    return mdat_desc;
}

/* free the mdat descriptor */

int32_t free_mdat_desc(mdat_desc_t *mdat_desc){
    if (mdat_desc->sample_sizes != NULL)
        free(mdat_desc->sample_sizes);
    if(mdat_desc->sample_duration != NULL)
        free(mdat_desc->sample_duration);
    if(mdat_desc != NULL)
        free(mdat_desc);
    return VPE_SUCCESS;
}


int32_t clean_video_headers(ismv_parser_t* parser_data){
    int32_t present_video_trak;
    present_video_trak = parser_data->vid_trak_num;
    free_ismv_ind_video_stream(&parser_data->video[present_video_trak]);


    return VPE_SUCCESS;
}


int32_t clean_audio_headers(ismv_parser_t* parser_data){




    return VPE_SUCCESS;
}



uint32_t init_ismv_mfu_parser(ismv_parser_t* parser_data){





    return VPE_SUCCESS;
}



/*
  Function: Provides an mdat description. Provides the Sample sizes, time stamps for each sample & mdat position.
  Inputs: moof, moof size.
  Outputs: Mdat description strucutre:
          - Sample sizes
	  - Sample times
	  - All samples alike flag
	  - Number of samples. 
	  The outputs can ideally go into MFU as well.

*/
int32_t get_mdat_desc(uint8_t *moof, mdat_desc_t* mdat_desc){

    box_t box;
    uint8_t *data,no_moov=1,*traf_data,not_mdat = 1;
    size_t box_size,traf_size,i;
    const char traf_id[] = {'t','r','a','f'};
    const char tfhd_id[] = {'t','f','h','d'};
    const char trun_id[] = {'t','r','u','n'};
    const char mdat_id[] = {'m','d','a','t'};

    tfhd_t *tfhd;
    trun_t *trun;
    /*Parse within moof*/
    data = moof+8;
    do{
	read_next_root_level_box(data, 8, &box, &box_size);
	box_size= bswap_32(*((int32_t *)(data)));
	if(check_box_type(&box, traf_id)){
	    no_moov = 0;
	    traf_data = data+8;
	    traf_size = box_size-8;
	    while(traf_size){
		read_next_root_level_box(traf_data, 8, &box, &box_size);
		box_size =  bswap_32(*((int32_t *)(traf_data)));
		traf_size-=box_size;
		if(check_box_type(&box, tfhd_id)){
		    tfhd = tfhd_reader(traf_data, 0,0);
		    mdat_desc->default_sample_size=tfhd->def_sample_size ;
		    mdat_desc->default_sample_duration = tfhd->def_sample_duration;
		    free(tfhd);
		    tfhd = NULL;
		}
                if(check_box_type(&box, trun_id)){
		    trun = trun_reader(traf_data, 0,0);
		    mdat_desc->sample_count = trun->sample_count;
		    if(trun->flags & 0x00000100){
			mdat_desc->sample_duration = (uint32_t*)malloc(sizeof(uint32_t)*mdat_desc->sample_count);
			for(i =0;i < trun->sample_count;i++)
			    mdat_desc->sample_duration[i] =  trun->tr_stbl[i].sample_duration;			
		    }
		    if(trun->flags & 0x00000200){
                        mdat_desc->sample_sizes = (uint32_t*)malloc(sizeof(uint32_t)*mdat_desc->sample_count);
                        for(i =0;i < trun->sample_count;i++)
                            mdat_desc->sample_sizes[i] =  trun->tr_stbl[i].sample_size;
		    }
		    if(trun->tr_stbl!=NULL){
			free(trun->tr_stbl);
			trun->tr_stbl = NULL;
		    }
		    free(trun);
		    trun = NULL;		    
		}
		traf_data+=box_size;
	    }
	}
	else
	    data+=box_size;
    }while(no_moov);

    /*get the mdat position*/
    data = moof;
    do{
        box_size = read_next_root_level_box(data, 8, &box, &box_size);
	if(check_box_type(&box, mdat_id)){
	    mdat_desc->mdat_pos = data;
	    not_mdat = 0;
	}
	else
	    data+=box_size;
    }while(not_mdat);
    return VPE_SUCCESS;
}


