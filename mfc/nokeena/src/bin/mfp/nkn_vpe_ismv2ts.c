#include <byteswap.h>
#include "nkn_vpe_mfu_aac.h"
#include "nkn_vpe_raw_h264.h"
#include <string.h>
#include "nkn_vpe_mfp_ts.h"

#include "nkn_vpe_ismv_mfu.h"
#include "nkn_vpe_moof2mfu.h"
#include "mfu2iphone.h"
#include "nkn_memalloc.h"
#include "mfp_ref_count_mem.h"

#define MOOV_SEARCH_SIZE (32*1024)
#define MAX_NAL 500
#define MAX_NAL_SIZE 5000
#define MAX_BYTES_MOOV 1000

//static function declarations 
static inline int32_t 
init_mfu_parser(ismv_parser_t* ,moov_t*);
static inline mdat_desc_t* 
mfu_init_mdat_desc(void);
static inline int32_t 
mfu_free_mdat_desc(mdat_desc_t *);
#if 0
static inline int32_t 
mfu_clean_video_headers(ismv_parser_t* );
static inline int32_t 
mfu_clean_audio_headers(ismv_parser_t*);
#endif
static inline uint32_t 
init_ismv_mfu_parser(ismv_parser_t*);
static inline int32_t 
mfu_get_mdat_desc(uint8_t *, mdat_desc_t*);
static inline int32_t
ismv_get_moof(ismv_parser_ctx_t *, ismv_parser_t* ,
              uint32_t );
static inline int32_t 
ismv2ts_converter_desc_init(ismv2ts_converter_t*);
static inline int32_t
ismv2ts_converter_desc_free_ctx(ismv_parser_ctx_t *);
static inline int32_t
ismv2ts_converter_desc_free_parser_data( ismv_parser_t *,
                                         moov_t *);


static uint32_t 
uint32_byte_swap(uint32_t input){
    uint32_t ret=0;
    ret = (((input&0x000000FF)<<24) + ((input&0x0000FF00)<<8) +
           ((input&0x00FF0000)>>8) + ((input&0xFF000000)>>24));
    return ret;
}


/***********************************************************
   Function: Wrapper function to convert an ismv file to mfu

************************************************************/
int32_t 
ismv2ts_convert(ismv2ts_converter_t* ismv2ts_converter_desc)
{
    uint32_t i=0;
    int32_t j=0, k=0, l=0;
    uint32_t moof_count = 0,len = 0;
    //char fname_temp[250];
    uint32_t actual_moof_count = 0;
    ismv_parser_t* parser_data = NULL;
    mfu_data_t* mfu_data = NULL;
    int32_t ret = VPE_SUCCESS;
    /* initialize ismv2ts_converter_desc->ctx and parserdata for all the profiles */
    for (i=0;i<ismv2ts_converter_desc->n_profiles;i++) {
	ismv2ts_converter_desc->profile_num = i;
	ismv2ts_converter_desc_init(ismv2ts_converter_desc);
    }
    /* assumed that for all the profiles num of moof_count is same */
    actual_moof_count =
	ismv2ts_converter_desc->ctx[0]->mfra->tfra[0]->n_entry;
    if (actual_moof_count == 0 ){
	printf( "There should be atleast one moof to process \n");
	goto free_code;
    }
#define _CHECK_FILE_
#ifdef _CHECK_FILE_
    FILE* fout_single_file_profile1;
    FILE* f_ind_file;
    char filename[350];
    char actual_name[350] = "/home/sumalatha/sumalatha/iphone/Channel_0_2";
    uint32_t file_size;
    uint8_t *file_buffer = NULL;
    fout_single_file_profile1 = fopen("ts_file_profile2.ts","wb");
#endif
    do {
	/* obtain each moof for all the profiles and process them */
	for (i=0;i<ismv2ts_converter_desc->n_profiles;i++) {
	    ismv2ts_converter_desc->profile_num = i;
#if 0
	    len =snprintf(NULL, 0, "%s_p%02d",
			  ismv2ts_converter_desc->m3u8_name, i);
	    snprintf(fname_temp, len+1, "%s_p%02d",
		     ismv2ts_converter_desc->m3u8_name, i);
	    ismv2ts_converter_desc->profile_info[i].output_name =
		fname_temp;
	    ismv2ts_converter_desc->parser_data[i].fname =
		fname_temp;
#endif
	    ismv2ts_converter_desc->parser_data[i].chunk_num =
		moof_count;
	    parser_data = &ismv2ts_converter_desc->parser_data[i];
	    /* process for each track */
	    for(j=0; j<parser_data->n_traks; j++) {
		/* get each moof, parser_data->audio.moof_data or
		parser_data->video.moof_data is updated */
		ret = ismv_get_moof(ismv2ts_converter_desc->ctx[i],
			      parser_data, j);
		if (ret == VPE_ERROR)
		   goto free_code;
	    }//for n_traks
	    for	(k=0; k<parser_data->n_aud_traks; k++) {
		for (l=0; l<parser_data->n_vid_traks; l++) {
		    mfu_data = (mfu_data_t*)
			nkn_malloc_type(sizeof(mfu_data_t),
					mod_vpe_mfu_data_t);

		    /* convert moof to mfu (one audiomoof + one
		       video_moof)*/
		    ismv_moof2_mfu (
				    ismv2ts_converter_desc->ctx[i]->moov,
				    ismv2ts_converter_desc->ctx[i]->moov,
				    parser_data->audio[k].moof_data,
				    parser_data->video[l].moof_data,
				    parser_data->audio[k].track_num,
				    parser_data->video[l].track_num,
				    parser_data, mfu_data);
		    if (parser_data->audio[k].moof_data != NULL)
			free(parser_data->audio[k].moof_data);
		    if (parser_data->video[l].moof_data != NULL)
			free(parser_data->video[l].moof_data);
		    /*convert mfu to ts */
		    ref_count_mem_t *ref_cont = createRefCountedContainer(
						  mfu_data, destroyMfuData);
		    ref_cont->hold_ref_cont(ref_cont);
		    apple_fmtr_hold_encap_ctxt(get_sessid_from_mfu(mfu_data));
		    mfubox_ts(ref_cont);
		/*
		    if (mfu_data != NULL) {
			free (mfu_data);
		    }
			*/

		}//for n_vid_traks
	    }//for n_aud_traks

	}//for n_profiles
	moof_count++;
    } while (moof_count != (actual_moof_count -1));
#ifdef _CHECK_FILE_
    for (i=0;i <actual_moof_count-1 ;i++) {
	len =snprintf(NULL, 0, "%s_%06d.ts",actual_name,
		      i);
	snprintf(filename, len+1, "%s_%06d.ts", actual_name, i+1);
	f_ind_file = fopen(filename,"rb");
	fseek (f_ind_file , 0 , SEEK_END);
	file_size = ftell (f_ind_file);
	rewind (f_ind_file);
	file_buffer = (uint8_t*) malloc (sizeof(uint8_t)*file_size);
	fread (file_buffer,1,file_size, f_ind_file);

	fwrite(file_buffer,file_size, 1,fout_single_file_profile1);
	fclose(f_ind_file);

    }
    fclose(fout_single_file_profile1);
#endif
 free_code:
    /* free the ismv2ts_converter_desc ctx and parser_data */
    for (i=0;i<ismv2ts_converter_desc->n_profiles;i++) {
        ismv2ts_converter_desc->profile_num = i;
	ismv2ts_converter_desc_free_parser_data(&ismv2ts_converter_desc->parser_data[i],
						ismv2ts_converter_desc->ctx[i]->moov);
	mp4_moov_cleanup(ismv2ts_converter_desc->ctx[i]->moov,
			 ismv2ts_converter_desc->ctx[i]->moov->n_tracks);

	mp4_cleanup_ismv_ctx(ismv2ts_converter_desc->ctx[i]);
	//ismv2ts_converter_desc_free_ctx(ismv2ts_converter_desc->ctx[i]);
    }
    return VPE_SUCCESS;
}

/* This function is used to initialise the ismv2ts_converter_desc->ctx
   and ismv2ts_converter_desc->parserdata for all the profiles */
int32_t ismv2ts_converter_desc_init(ismv2ts_converter_t* ismv2ts_converter_desc)
{
    FILE *fp,*fout;
    size_t mp4_size, mfra_off,moov_offset, moov_size, tmp_size,
	mfra_size;
    uint8_t mfro[16], moov_search_data[MOOV_SEARCH_SIZE], *moov_data,
	*mfra_data;
    moov_t *moov;
    uint32_t  n_traks, ret = 0;
    ismv_parser_ctx_t *ctx;
    uint32_t profile_num = ismv2ts_converter_desc->profile_num;

    mp4_size = 0;
    fp = fout = NULL;
    fp = fopen(ismv2ts_converter_desc->profile_info[profile_num].ismv_name,
	      "rb");
    if(fp == NULL) {
	printf("Error opening input ismv file\n");
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
	return VPE_ERROR;
    }

    /* read the moov data into memory */
    fseek(fp,moov_offset, SEEK_SET);

    moov_data = (uint8_t*)nkn_calloc_type(1, moov_size,
					  mod_vpe_moov_data_buf);

    fread(moov_data, 1, moov_size, fp);

    /* initialize the moov box */
    moov = mp4_init_moov((uint8_t*)moov_data, moov_offset, moov_size);

    /* initialize the trak structure for the mp4 data */
    n_traks = mp4_populate_trak(moov);
    if(!n_traks) {
	printf("there shoud be atleast one trak; cannot proceed\n");
	return VPE_ERROR;
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
    mfra_data = (uint8_t*)nkn_malloc_type(tmp_size,
					  mod_vpe_mfra_data_buf);

    fseek(fp, mfra_off, SEEK_SET);
    fread(mfra_data, 1, tmp_size, fp);


    /* get exact mfra box size */
    mfra_size = mp4_get_mfra_size(mfra_data, tmp_size);
    

    /* initialize the ismv context */
    ctx = mp4_init_ismv_parser_ctx(mfra_data, mfra_size, moov);
    ismv2ts_converter_desc->ctx[profile_num] = ctx;

    /*Prepare the MFU parser data structure*/
    ismv2ts_converter_desc->parser_data[profile_num].chunk_time =
	ismv2ts_converter_desc->profile_info->ts;
    ismv2ts_converter_desc->parser_data[profile_num].n_traks =
	n_traks;

    ismv2ts_converter_desc->parser_data[profile_num].fin = fp;
#if 0
    ismv2ts_converter_desc->parser_data[profile_num].fname =
	ismv2ts_converter_desc->profile_info->output_name;
#endif
    ismv2ts_converter_desc->parser_data[profile_num].profile_num =
	profile_num;
    ismv2ts_converter_desc->parser_data[profile_num].audio_rate =
	ismv2ts_converter_desc->profile_info[profile_num].audio_rate;
    ismv2ts_converter_desc->parser_data[profile_num].video_rate =
	ismv2ts_converter_desc->profile_info[profile_num].video_rate;
    init_mfu_parser(&ismv2ts_converter_desc->parser_data[profile_num],
		    moov);

    return VPE_SUCCESS;
}

int32_t
ismv2ts_converter_desc_free_parser_data( ismv_parser_t *parser_data,
					 moov_t *moov)
{
    int32_t i=0;
    trak_t  *trak;
    int32_t trak_type=0;
    if(parser_data->moof2mfu_desc != NULL)
	free(parser_data->moof2mfu_desc);
    for(i=0;i<parser_data->n_traks;i++){
	trak = moov->trak[i];
	trak_type = *((int32_t*)(&trak->mdia->hdlr->handler_type));
	/*clean the video and audio headr codes*/
	switch(trak_type){
	    case VIDE_HEX:
		if(mfu_clean_video_headers(parser_data) == VPE_ERROR)
		    return VPE_ERROR;
		parser_data->vid_trak_num++;
		break;
	    case SOUN_HEX:
		if(mfu_clean_audio_headers(parser_data) == VPE_ERROR)
		    return VPE_ERROR;
		parser_data->aud_trak_num++;
                break;
	    default:
		break;
	}
    }
    
    return VPE_SUCCESS;
}

int32_t
ismv2ts_converter_desc_free_ctx(ismv_parser_ctx_t *ctx)
{
    if (ctx != NULL) {
	free(ctx);
    }
    return VPE_SUCCESS;
}

/*************************************************
  Initialize the parser_data structure
**************************************************/
static inline int32_t init_mfu_parser(ismv_parser_t*
  parser_data,moov_t* moov)
{
    int32_t i,trak_type;
    trak_t  *trak;
    parser_data->vid_trak_num =parser_data->aud_trak_num = 0;
    parser_data->n_vid_traks = parser_data->n_aud_traks = 0;
    parser_data->file_dump=0;
    parser_data->vid_cur_frag=0;
    parser_data->aud_cur_frag = 0;
    parser_data->chunk_num = 0;
    parser_data->aud_pos = 0;
    parser_data->vid_pos = 0;
    parser_data->moof2mfu_desc =
	(moof2mfu_desc_t*) nkn_malloc_type(sizeof(moof2mfu_desc_t),
					   mod_vpe_moof2mfu_desc_t);
    for(i=0;i<parser_data->n_traks;i++){
        trak = moov->trak[i];
        trak_type = *((int32_t*)(&trak->mdia->hdlr->handler_type));
	if(trak_type == VIDE_HEX){
	    video_pt *video = &parser_data->video[parser_data->n_vid_traks];
	    video->list = NULL;
	    video->moof_data = NULL;
	    parser_data->n_vid_traks++;
	}
	if(trak_type == SOUN_HEX){
	    audio_pt *audio = &parser_data->audio[parser_data->n_aud_traks];
	    audio->esds_flag = 0;
	    audio->adts = (adts_t*) nkn_calloc_type(1, sizeof(adts_t),
						    mod_vpe_adts);
	    audio->list = NULL;
	    audio->moof_data = NULL;
            parser_data->n_aud_traks++;
	}
    }
    return VPE_SUCCESS;
}
/* This function is used to get audio_moof data and video moof_data
 * from the given ctx->moov and parser_data structure 
 * ctx-> input, it has the details abt the moov
 * parser_data ->input/output, used to update
 * parser_data->audio.moof_data and parser_data->video.moof_data
 * this structure is also used to maintain the state
 * track_num -> input, indicates the either audio or video
 * track 
 */
int32_t
ismv_get_moof(ismv_parser_ctx_t *ctx, ismv_parser_t* parser_data,
	      uint32_t track_num)
{
    /*Declarations*/
    int32_t trak_type;
    moov_t *moov;
    trak_t  *trak;
    size_t moof_time,moof_offset,moof_size;
    moov = ctx->moov;
    /*find the trak_type*/
    trak = moov->trak[track_num];
    ctx->curr_trak = track_num;
    trak_type = *((int32_t*)(&trak->mdia->hdlr->handler_type));
    
    switch(trak_type){
	case VIDE_HEX:
	    /* find the moof_offset */
	    ctx->bs->pos = parser_data->vid_pos;
	    ctx->curr_frag = parser_data->vid_cur_frag;
	    if (parser_data->vid_cur_frag == 0) {
		//call this only first time
		mp4_frag_seek(ctx, moov->trak[track_num]->tkhd->track_id, 0,
			      moov->trak[track_num]->mdia->mdhd->timescale,
			      &moof_offset,
			      &moof_size, &moof_time);
		if (mp4_get_next_frag(ctx, &moof_offset, &moof_size,
                                      &moof_time) ==
                    E_ISMV_NO_MORE_FRAG)
                    break;
	    } else {
		if (mp4_get_next_frag(ctx, &moof_offset, &moof_size,
				      &moof_time) ==
		    E_ISMV_NO_MORE_FRAG)
		    break;
	    }
	    if (moof_size <= 0) {
		printf(" video moof_size should be non-zero \n");
		return VPE_ERROR;
	    }
	    parser_data->video[parser_data->vid_trak_num].track_num =
		track_num;
	    parser_data->video[parser_data->vid_trak_num].moof_offset
		= moof_offset;
	    parser_data->video[parser_data->vid_trak_num].moof_size =
		moof_size;
	    /* read the moof from the moof_offset obtained */
	    fseek(parser_data->fin,moof_offset,SEEK_SET);
	    parser_data->video[parser_data->vid_trak_num].moof_data =
		(uint8_t*) nkn_malloc_type(sizeof(uint8_t)*moof_size,
					   mod_vpe_moof_data_vid_buf);
	    fread(parser_data->video[parser_data->vid_trak_num].moof_data, 
		  moof_size, sizeof(uint8_t), parser_data->fin);
	    parser_data->vid_cur_frag++;
	    parser_data->vid_pos = ctx->bs->pos;

	    break;
	case SOUN_HEX:
	    /* find the moof_offset */
	    ctx->bs->pos = parser_data->aud_pos;
	    ctx->curr_frag = parser_data->aud_cur_frag;
	    if (parser_data->aud_cur_frag == 0) {
		// call this only first time
		mp4_frag_seek(ctx, moov->trak[track_num]->tkhd->track_id,
			      0, moov->trak[track_num]->mdia->mdhd->timescale,
			      &moof_offset, &moof_size,
			      &moof_time);
		if(mp4_get_next_frag(ctx, &moof_offset, &moof_size,
                                     &moof_time) ==
                   E_ISMV_NO_MORE_FRAG)
                    break;
		
	    } else {
		if(mp4_get_next_frag(ctx, &moof_offset, &moof_size,
				     &moof_time) ==
		   E_ISMV_NO_MORE_FRAG)
		    break;
	    }
	    if (moof_size <= 0) {
                printf(" audio moof_size should be non-zero \n");
                return VPE_ERROR;
            }
	    parser_data->audio[parser_data->aud_trak_num].track_num =
		track_num;
	    parser_data->audio[parser_data->aud_trak_num].moof_offset
		= moof_offset;
	    parser_data->audio[parser_data->aud_trak_num].moof_size =
		moof_size;

	    /* read the moof from the moof_offset obtained */
	    fseek(parser_data->fin,moof_offset,SEEK_SET);
	    parser_data->audio[parser_data->aud_trak_num].moof_data =
		(uint8_t*) nkn_malloc_type(sizeof(uint8_t)*moof_size,
					   mod_vpe_moof_data_aud_buf);

	    fread(parser_data->audio[parser_data->aud_trak_num].moof_data, 
		  moof_size, sizeof(uint8_t), parser_data->fin);
	    parser_data->aud_cur_frag++;
	    parser_data->aud_pos = ctx->bs->pos;
	    break;
	default:
	    break;
    }
    return VPE_SUCCESS;
}

/*
 * This function is used to get the video related details like avcC
 */
int32_t 
mfu_capture_video_headers(stbl_t* stbl,ismv_parser_t*
			  parser_data)
{
    int32_t present_video_trak, ret = VPE_SUCCESS;
    uint8_t *stbl_data;
    uint8_t *data,*initial_data;
    box_t box;
    size_t bytes_read, total_bytes_read = 0;
    int32_t box_size,size = 10000, size_read = 0;
    char moov_id[] = {'m', 'o', 'o', 'v'};
    uint32_t moov_offset = 0, moov_search_win = MOOV_SEARCH_SIZE;
    present_video_trak = parser_data->vid_trak_num;
    stbl_data = stbl->data;
    parser_data->video[present_video_trak].stbl_size = stbl->size;
    ////
    data = (uint8_t*) nkn_malloc_type(moov_search_win,
				      mod_vpe_stsd_buf);
    
    initial_data =data;
    parser_data->iocb->ioh_seek(parser_data->io_desc, 0, SEEK_SET);
    size_read = parser_data->iocb->ioh_read(data, 1, moov_search_win, parser_data->io_desc);
    if(size_read != (int32_t)moov_search_win) {
      ret = -E_VPE_READ_INCOMPLETE;
      goto error;
    }
    //fseek(parser_data->fin, 0, SEEK_SET);
    //fread(data, moov_search_win, 1, parser_data->fin);
    while(total_bytes_read < moov_search_win) {
	box_size = read_next_root_level_box(data, size, &box,
					    &bytes_read);
	total_bytes_read += box_size;
	if(check_box_type(&box,moov_id)) {
	    moov_offset = data - initial_data;
	    break;
	} else {
	    data+=box_size;
	    total_bytes_read+=box_size;
	}
    }
    
    if (moov_offset == 0) {
	ret = -E_VPE_MP4_MOOV_ERR;
	goto error;
    }

    ///
    parser_data->video[present_video_trak].stbl_offset = stbl->pos
	+moov_offset;
    //allocating memory for video stream avc1
    mfu_init_ind_video_stream(&parser_data->video[present_video_trak]);

    //getting the stsd box offset,size and position
    mp4_get_stsd_info(stbl_data, &parser_data->video[present_video_trak]);

    //getting the avcC box offset,size and position
    ret = mp4_get_avcC_info(parser_data->io_desc, parser_data->iocb,&parser_data->video[present_video_trak]);
    if(ret < 0) {
      goto error;
    }
    if(parser_data->video[present_video_trak].codec_type != VPE_MFP_H264_CODEC/*H264_VIDEO*/){
            printf("This video is not H264 supported \n");
            return -E_VPE_VIDEO_CODEC_ERR;
    }
    /*get the pps and sps data for video stream and update in the avc1
      structure*/
    ret = mfu_get_pps_sps(parser_data->io_desc, parser_data->iocb,
			  &parser_data->video[present_video_trak]);
    if(ret < 0) {
      goto error;
    }

 error:
    if (initial_data)
	free(initial_data);    
    return ret;
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
int32_t mfu_capture_audio_headers(stbl_t* stbl,audio_pt* audio){//ismv_parser_t* parser_data){
  int32_t ret = VPE_SUCCESS;
  ret = mfu_get_aac_headers(stbl,(adts_t*)(audio->adts));
  if(ret <= 0)
    return ret;
  mfu_form_aac_header((adts_t*)(audio->adts),&audio->header,&audio->header_size);
  
  return VPE_SUCCESS;
}


int32_t mfu_handle_audio_moof(uint8_t * moof, audio_pt*
    audio, moof2mfu_desc_t* moof2mfu_desc)
{//ismv_parser_t* parser_data){
    /*
      1. Obtain each audio sample
      2. for each sample of moof append 7 byte adts header.
      3. Dump the sample with headers. 
    */
    uint32_t *temp = NULL;
    uint32_t *temp1 = NULL;
    uint32_t i;
    mdat_desc_t* md;
    uint8_t* mdat,*org_pos,*org_md_mdat_pos;
    uint32_t total_mem_reqd = 0;
    mfu_rw_desc_t *audiofg = &moof2mfu_desc->mfu_raw_box->audiofg;
    /*Obtain mdat descriptor*/
    md = (mdat_desc_t*) nkn_calloc_type(1, sizeof(mdat_desc_t),
					mod_vpe_mdat_desc_t);
    org_md_mdat_pos = md->mdat_pos;
    mfu_get_mdat_desc(moof, md);   
    md->timescale = audio->timescale;
    audio->total_sample_duration = 0;
    /*Dump Each sample*/
    /*Calculate the total size of data to be dumped.*/

     audiofg->unit_count = md->sample_count;
     audiofg->default_unit_duration
	 = md->default_sample_duration;
     audiofg->default_unit_size =
	 md->default_sample_size;
     temp = (uint32_t*) nkn_calloc_type(1, md->sample_count*3*4,
					mod_vpe_aud_sample_dur_buf);

     //temp1 = (uint32_t *)malloc(md->sample_count *sizeof(uint32_t));
     audiofg->sample_duration = temp;
     audiofg->composition_duration = temp + md->sample_count;
     audiofg->sample_sizes = temp + md->sample_count*2;
     moof2mfu_desc->mfu_raw_box->audiofg_size = RAW_FG_FIXED_SIZE +
	 3 *(4*audiofg->unit_count) + CODEC_INFO_SIZE_SIZE +
	 moof2mfu_desc->mfu_raw_box->audiofg.codec_info_size;

    for(i=0; i<md->sample_count;i++){
	if(md->default_sample_size) {
#ifdef _BIG_ENDIAN_
            audiofg->sample_sizes[i] =
                uint32_byte_swap(md->default_sample_size+audio->header_size);
	    total_mem_reqd+=md->default_sample_size+audio->header_size;
#else
	    audiofg->sample_sizes[i] =
		md->default_sample_size+audio->header_size;
	    total_mem_reqd+=md->default_sample_size+audio->header_size;
#endif
	} else {
#ifdef _BIG_ENDIAN_
            audiofg->sample_sizes[i] =
                uint32_byte_swap(md->sample_sizes[i]+audio->header_size);
            total_mem_reqd+=md->sample_sizes[i]+audio->header_size;
#else
            audiofg->sample_sizes[i] =
		md->sample_sizes[i]+audio->header_size;
            total_mem_reqd+=md->sample_sizes[i]+audio->header_size;
#endif
	}
	if(md->default_sample_duration) {
#ifdef _BIG_ENDIAN_
            audiofg->sample_duration[i] =
                uint32_byte_swap(md->default_sample_duration);
            audio->total_sample_duration+=md->default_sample_duration;
#else
            audiofg->sample_duration[i] = md->default_sample_duration;
            audio->total_sample_duration+=md->default_sample_duration;
#endif
	    
	} else {
#ifdef _BIG_ENDIAN_
            audiofg->sample_duration[i] =
                uint32_byte_swap(md->sample_duration[i]);
            audio->total_sample_duration+=md->sample_duration[i];
#else
            audiofg->sample_duration[i] = md->sample_duration[i];
            audio->total_sample_duration+=md->sample_duration[i];
#endif
	}
    }//
    moof2mfu_desc->adat =
	(uint8_t*) nkn_malloc_type(sizeof(uint8_t)*total_mem_reqd,
				   mod_vpe_adat_buf);
    moof2mfu_desc->adat_size = total_mem_reqd;
    org_pos =  moof2mfu_desc->adat;
    mdat = md->mdat_pos+8;
    if(md->default_sample_size){
	for(i=0; i<md->sample_count;i++){
            mfu_dump_aac_unit(mdat, md->default_sample_size, audio, moof2mfu_desc);
            mdat+=md->default_sample_size;
	}
    }
    else{
        for(i=0; i<md->sample_count;i++){
	    mfu_dump_aac_unit(mdat, md->sample_sizes[i], audio, moof2mfu_desc);
	    mdat+=md->sample_sizes[i];
        }
    }

    moof2mfu_desc->adat = org_pos;
    //md->mdat_pos = org_md_mdat_pos;
    //mfu_free_mdat_desc(md);
    if(md->sample_sizes != NULL)
	free(md->sample_sizes);
    if(md->sample_duration != NULL)
	free(md->sample_duration);
    if(md != NULL)
	free(md);
    return VPE_SUCCESS;
}


int32_t mfu_handle_video_moof(uint8_t * moof, video_pt* video, 
    moof2mfu_desc_t* moof2mfu_desc)
{
    mdat_desc_t* mdat_desc;
    uint32_t *temp,i;
    mfu_rw_desc_t *videofg = &moof2mfu_desc->mfu_raw_box->videofg;
   //initialise mdat descriptor
    mdat_desc = mfu_init_mdat_desc();
    mfu_get_mdat_desc(moof, mdat_desc);
    video->total_sample_duration = 0;

    /*updating the videofg elements */
    videofg->unit_count =
	mdat_desc->sample_count;
    videofg->default_unit_duration
	= mdat_desc->default_sample_duration;
    videofg->default_unit_size =
	mdat_desc->default_sample_size;
    /* allocate for sample_duration, composition_duration and
       sample_sizes */
    temp = (uint32_t*) nkn_calloc_type(1, mdat_desc->sample_count*3*4,
				       mod_vpe_vid_sample_dur_buf);
    videofg->sample_duration = temp;
    videofg->composition_duration =  temp + (mdat_desc->sample_count*1);
    
#ifdef _BIG_ENDIAN_

    for (i=0;i<mdat_desc->sample_count;i++) {
	if(mdat_desc->default_sample_duration) {
	    videofg->sample_duration[i] =
		uint32_byte_swap(mdat_desc->default_sample_duration);
	    video->total_sample_duration+=mdat_desc->default_sample_duration;
	} else {
	    videofg->sample_duration[i] =
		uint32_byte_swap(mdat_desc->sample_duration[i]);
	    video->total_sample_duration+=mdat_desc->sample_duration[i];
	}
	if(mdat_desc->composition_duration)
	videofg->composition_duration[i] = 
		uint32_byte_swap(mdat_desc->composition_duration[i]);
    }
#else
    for (i=0;i<mdat_desc->sample_count;i++) {
	if(mdat_desc->default_sample_duration) {
	    videofg->sample_duration[i] =
		mdat_desc->default_sample_duration;
	    video->total_sample_duration+=mdat_desc->default_sample_duration;
	} else {
	    videofg->sample_duration[i] =
		mdat_desc->sample_duration[i];
	    video->total_sample_duration+=videofg->sample_duration[i];
	}
	if(mdat_desc->composition_duration)
	videofg->composition_duration[i] = 
		(mdat_desc->composition_duration[i]);	
    }
    //    memcpy(videofg->sample_duration,
    //   mdat_desc->sample_duration, mdat_desc->sample_count*4);
#endif
 
    videofg->sample_sizes = temp +
        (mdat_desc->sample_count*2);
    moof2mfu_desc->vdat = (uint8_t *)
	nkn_malloc_type(mdat_desc->sample_count *
        MAX_NAL*MAX_NAL_SIZE, mod_vpe_vdat_buf);
    /*update the sample_sizes and vdat for videofg structure */
    mfu_get_h264_samples(mdat_desc, moof2mfu_desc);
    moof2mfu_desc->mfu_raw_box->videofg_size =  RAW_FG_FIXED_SIZE + 3
	*(4*videofg->unit_count) + CODEC_INFO_SIZE_SIZE +
	moof2mfu_desc->mfu_raw_box->videofg.codec_info_size;


    if(mdat_desc->sample_sizes != NULL)
        free(mdat_desc->sample_sizes);
    if(mdat_desc->sample_duration != NULL)
        free(mdat_desc->sample_duration);
   if(mdat_desc->composition_duration!= NULL)
        free(mdat_desc->composition_duration);     
    if(mdat_desc !=NULL)
	free(mdat_desc);
    return VPE_SUCCESS;
}

/* initialise the mdat descriptor*/
static inline mdat_desc_t* mfu_init_mdat_desc(){
    mdat_desc_t* mdat_desc;
    mdat_desc = (mdat_desc_t*) nkn_calloc_type(1, sizeof(mdat_desc_t),
					       mod_vpe_mdat_desc_t);
    return mdat_desc;
}

/* free the mdat descriptor */

static inline int32_t mfu_free_mdat_desc(mdat_desc_t *mdat_desc){
    if (mdat_desc->sample_sizes != NULL)
        free(mdat_desc->sample_sizes);
    if(mdat_desc->sample_duration != NULL)
        free(mdat_desc->sample_duration);
    if(mdat_desc->mdat_pos != NULL)
	free(mdat_desc->mdat_pos);
    if(mdat_desc != NULL)
        free(mdat_desc);
    return VPE_SUCCESS;
}


int32_t mfu_clean_video_headers(ismv_parser_t* parser_data){
    int32_t present_video_trak;
    present_video_trak = parser_data->vid_trak_num;
    mfu_free_ind_video_stream(&parser_data->video[present_video_trak]);

    return VPE_SUCCESS;
}


int32_t mfu_clean_audio_headers(ismv_parser_t* parser_data){

   if( parser_data->audio[parser_data->aud_trak_num].adts != NULL)
	free( parser_data->audio[parser_data->aud_trak_num].adts);
#if 0
   if(parser_data->audio[parser_data->aud_trak_num].header != NULL)
       free(parser_data->audio[parser_data->aud_trak_num].header);
#endif
    return VPE_SUCCESS;
}



static inline uint32_t init_ismv_mfu_parser(ismv_parser_t* parser_data){





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
static inline int32_t mfu_get_mdat_desc(uint8_t *moof, mdat_desc_t* mdat_desc){

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
		    tfhd = tfhd_reader(traf_data, 0, 0);
		    mdat_desc->default_sample_size = tfhd->def_sample_size ;
		    mdat_desc->default_sample_duration = tfhd->def_sample_duration;
		    free(tfhd);
		    tfhd = NULL;
		}
                if(check_box_type(&box, trun_id)){
		    trun = trun_reader(traf_data, 0, 0);
		    mdat_desc->sample_count = trun->sample_count;
		    if(trun->flags & 0x00000100){
			mdat_desc->sample_duration = (uint32_t*)
			    nkn_malloc_type(sizeof(uint32_t)*mdat_desc->sample_count, mod_vpe_sam_dur);
			for(i =0;i < trun->sample_count;i++)
			    mdat_desc->sample_duration[i] =  trun->tr_stbl[i].sample_duration;			
		    }
		    if(trun->flags & 0x00000200){
                        mdat_desc->sample_sizes = (uint32_t*)
			    nkn_malloc_type(4*mdat_desc->sample_count,
					    mod_vpe_sam_size);
                        for(i =0;i < trun->sample_count;i++)
                            mdat_desc->sample_sizes[i] =  trun->tr_stbl[i].sample_size;
		    }
		    if(trun->flags & 0x00000800){
                        mdat_desc->composition_duration = (uint32_t*)
			    nkn_malloc_type(4*mdat_desc->sample_count,
					    mod_vpe_sam_size);
                        for(i =0;i < trun->sample_count;i++)
                            mdat_desc->composition_duration[i] =  trun->tr_stbl[i].scto;
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


/*DEPRECATED*/

#ifdef LIST_CHK
	if(trak_type == SOUN_HEX){
		  GList *list;
		  int32_t num = 0;
		  list = parser_data->audio[0].list;
		  list = g_list_first(list);
		  for(;;){
		      list =  g_list_next(list);
		      if(num<=116){
			  printf("=%d %p %p %p\n",num,list->data,list->prev,list->next);
		      }
		      num++;
		      if(list == NULL)
			  break;
		  }
	      }
#endif

