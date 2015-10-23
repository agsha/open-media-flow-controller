#include <stdio.h>
#include "nkn_vpe_mfu_writer.h"
#include "nkn_vpe_moof2mfu.h"
#include "nkn_vpe_mfu_defs.h"
#include "nkn_vpe_raw_h264.h"
#include "nkn_vpe_mfu_aac.h"
#include "nkn_memalloc.h"
//#define CHECK

/*
 * This function is used to convert moof data to corresponding mfu.
 * moov -> input (it has all the details of the moov( track, trun,
 * tfra...)
 * audio_moof_data-> input containg the audio moof data
 * video_moof_data -> input containg the video moof data
 * aud_trak_num -> input representing the audio track number in moov
 * vid_trak_num ->input representing the video track number in moov
 * parser_data -> input which maintains the state across the calls
 * mfu_data -> output which will have the entire mfu
 */

int32_t
ismv_moof2_mfu(moov_t *aud_moov, moov_t *vid_moov, uint8_t* audio_moof_data, uint8_t*
	       video_moof_data, uint32_t aud_trak_num, uint32_t
	       vid_trak_num, ismv_parser_t* parser_data, mfu_data_t* mfu_data)
{
    moof2mfu_desc_t * moof2mfu_desc = parser_data->moof2mfu_desc;
    uint32_t mfu_data_size = 0;
    uint32_t vdat_pos=0,adat_pos=0;
    uint32_t vdat_box_size = 0;
    uint8_t *mfubox_pos =NULL;
    uint8_t *rwfgbox_pos = NULL;
    uint8_t *vdatbox_pos = NULL;
    uint8_t *adatbox_pos = NULL;
    mfub_t *mfubox_header = NULL;
    int32_t ret = VPE_SUCCESS;
#ifdef CHECK
    FILE *f_audio = NULL;
    FILE *f_video= NULL;
    FILE *f_out = NULL;
    f_audio = fopen("audio.txt","wb");
    f_video = fopen("video.txt","wb");
    f_out = fopen("out.mfu","wb");
#endif
    moof2mfu_desc = (moof2mfu_desc_t*)
	nkn_malloc_type(sizeof(moof2mfu_desc_t),
			mod_vpe_moof2mfu_desc_t);
    if (!moof2mfu_desc) {
        return -E_VPE_PARSER_NO_MEM;
    }
    mfubox_header = (mfub_t*) nkn_malloc_type(sizeof(mfub_t),
					      mod_vpe_mfub_t);
    if (!mfubox_header) {
        return -E_VPE_PARSER_NO_MEM;
    }
    /* initializes the moof2mfu descriptor */   
    init_moof2mfu_desc(moof2mfu_desc);

    /* parse the audio moof and get the details reqired for
       mfu(audiofg deatils and adat)*/
    if (audio_moof_data != NULL) {
	moof2mfu_desc->is_audio = 1;
	ret = ismv_moof2_mfu_audio_process(aud_moov, audio_moof_data,
				     aud_trak_num,
				     moof2mfu_desc,
				     parser_data);
	if (ret <= 0) {
	    if(moof2mfu_desc != NULL)
		free_moof2mfu_desc(moof2mfu_desc);
	    if(mfubox_header != NULL)
		free(mfubox_header);
	    ismv_moof2_mfu_clean_audio_trak(parser_data,
					    parser_data->aud_trak_num);
	    return ret;
	}
	ismv_moof2_mfu_clean_audio_trak(parser_data,
			   parser_data->aud_trak_num);

	moof2mfu_desc->mfu_raw_box_size += 4 +
	    moof2mfu_desc->mfu_raw_box->audiofg_size;

    } 
    /* parse the video moof and get the details reqired for
       mfu(videofg details and vdat)*/
    if (video_moof_data != NULL) {
	moof2mfu_desc->is_video = 1;
	ret = ismv_moof2_mfu_video_process(vid_moov,
				     video_moof_data,
				     vid_trak_num, 
				     moof2mfu_desc,
				     parser_data);
	if (ret <= 0) {
	    if(moof2mfu_desc != NULL)
                free_moof2mfu_desc(moof2mfu_desc);
	    if(mfubox_header != NULL)
                free(mfubox_header);
	    ismv_moof2_mfu_clean_video_trak(parser_data,
					    parser_data->vid_trak_num);
	    return ret;
	}
	ismv_moof2_mfu_clean_video_trak(parser_data,
					parser_data->vid_trak_num);

	moof2mfu_desc->mfu_raw_box_size += 4 +
	    moof2mfu_desc->mfu_raw_box->videofg_size;

    }
    moof2mfu_desc->mfub_box_size = sizeof(mfub_t);

    /*calcualte the mfu size and allocate */
    mfu_data_size = MFU_FIXED_SIZE + moof2mfu_desc->mfub_box_size +
        moof2mfu_desc->mfu_raw_box_size;
    if (moof2mfu_desc->is_video) {
        mfu_data_size +=  moof2mfu_desc->vdat_size + FIXED_BOX_SIZE;
    }
    if (moof2mfu_desc->is_audio) {
        mfu_data_size +=  moof2mfu_desc->adat_size + FIXED_BOX_SIZE;
    }
    mfu_data->data = (uint8_t *)nkn_malloc_type(mfu_data_size,
						mod_vpe_mfu_data_buf);
    if (!mfu_data->data) {
	return -E_VPE_PARSER_NO_MEM;
    }


    /*Start the mfu file write from here*/
    /*write RAW Header box */
    rwfgbox_pos =mfu_data->data + moof2mfu_desc->mfub_box_size +
        (1*(BOX_NAME_SIZE + BOX_SIZE_SIZE));
    mfu_writer_rwfg_box(moof2mfu_desc->mfu_raw_box_size,
			moof2mfu_desc->mfu_raw_box, rwfgbox_pos,
			moof2mfu_desc);

    /*write VDAT box */
    if (moof2mfu_desc->is_video) {
        vdat_pos =  moof2mfu_desc->mfub_box_size +
            moof2mfu_desc->mfu_raw_box_size + (2 *(BOX_NAME_SIZE +
						   BOX_SIZE_SIZE));
        vdatbox_pos = mfu_data->data + vdat_pos;
        mfu_writer_vdat_box(moof2mfu_desc->vdat_size,
			    moof2mfu_desc->vdat,
			   vdatbox_pos);
        vdat_box_size = BOX_NAME_SIZE + BOX_SIZE_SIZE;
    }
    /*write ADAT box*/
    if (moof2mfu_desc->is_audio) {
        adat_pos = moof2mfu_desc->mfub_box_size +
            moof2mfu_desc->mfu_raw_box_size +
            moof2mfu_desc->vdat_size + vdat_box_size +
	    (2*(BOX_NAME_SIZE + BOX_SIZE_SIZE));

        adatbox_pos =  mfu_data->data + adat_pos;
        mfu_writer_adat_box(moof2mfu_desc->adat_size,
			    moof2mfu_desc->adat,
			    adatbox_pos);
    }
    /*write MFU Header box*/
    mfubox_pos = mfu_data->data;
    mfubox_header->program_number = parser_data->num_pmf;
    mfubox_header->stream_id = parser_data->profile_num;
    mfubox_header->sequence_num = parser_data->seq_num;
    mfubox_header->timescale = vid_moov->trak[0]->mdia->mdhd->timescale;
    //mfubox_header->duration = parser_data->chunk_time/1000;//in    seconds
    mfubox_header->duration =
        (parser_data->video[parser_data->vid_trak_num].total_sample_duration
	 / mfubox_header->timescale);
    if(mfubox_header->duration == 0) {
	mfubox_header->duration = 1;
    }

    mfubox_header->video_rate = parser_data->video_rate;
    mfubox_header->audio_rate = parser_data->audio_rate;
    mfubox_header->mfu_rate = 0;
    mfubox_header->offset_vdat = vdat_pos;
    mfubox_header->offset_adat = adat_pos;
    mfubox_header->mfu_size = mfu_data_size;
    mfu_writer_mfub_box(moof2mfu_desc->mfub_box_size, mfubox_header,
		       mfubox_pos);
    mfu_data->data_size = mfu_data_size;

#ifdef CHECK
    fwrite(moof2mfu_desc->vdat, moof2mfu_desc->vdat_size, 1, f_video);
    fwrite(moof2mfu_desc->adat, moof2mfu_desc->adat_size, 1, f_audio);
    fwrite( mfu_data->data, mfu_data_size, 1, f_out);
    fclose(f_video);
    fclose(f_audio);
    fclose(f_out);
#endif
    free_moof2mfu_desc(moof2mfu_desc);
    if (mfubox_header != NULL)
	free(mfubox_header);
    return VPE_SUCCESS;
}

/*
 * This function is used to parse audio moof data and update the audio
 * related details required for mfu.
 * moov ->input (it has all the details of the moov( track, trun,
 * tfra...)
 * moof_data ->input(audio moof)
 * track_num ->input(audio track_num)
 * moof2mfu_desc -> output (mfu related details are updated)
 * parser_data -> input (maintains state across the call)
 */
int32_t
ismv_moof2_mfu_audio_process(moov_t* moov, uint8_t* moof_data,
			     uint32_t track_num, 
			     moof2mfu_desc_t* moof2mfu_desc,
			     ismv_parser_t* parser_data)
{
    int32_t ret = VPE_SUCCESS;
    moof2mfu_desc->mfu_raw_box->audiofg.timescale =
	moov->trak[track_num]->mdia->mdhd->timescale;
    /* extracts the information from stbl and stsd box of audio track
       and check for codec compatibility */

    parser_data->audio[parser_data->aud_trak_num].adts =
	(adts_t*)nkn_calloc_type(1, sizeof(adts_t),mod_vpe_adts);
    if (!parser_data->audio[parser_data->aud_trak_num].adts) {
	return -E_VPE_PARSER_NO_MEM;
    }
    ret = mfu_capture_audio_headers(moov->trak[track_num]->stbl,
				    &parser_data->audio[parser_data->aud_trak_num]);
    if(ret <= 0)
      return ret;
    /* updates moof2mfu_desc structure after parsing the moof data*/
    if (mfu_handle_audio_moof(moof_data,
			      &parser_data->audio[parser_data->aud_trak_num], moof2mfu_desc)
	== VPE_ERROR) {
	ret = VPE_ERROR;
	return ret;
    }
    
    return VPE_SUCCESS;
}

/*
 * This function is used to parse video moof data and update the video
 * related details required for mfu.
 * moov ->input (it has all the details of the moov( track, trun,
 * tfra...)
 * moof_data ->input(video moof)
 * track_num ->input(video track_num)
 * moof2mfu_desc -> output (mfu related details are updated)
 * parser_data -> input (maintains state across the call)
 */
int32_t
ismv_moof2_mfu_video_process(moov_t* moov, uint8_t* moof_data,
			     uint32_t track_num, moof2mfu_desc_t*
			     moof2mfu_desc, ismv_parser_t*
			     parser_data)
{
    int32_t ret = VPE_SUCCESS;
    moof2mfu_desc->mfu_raw_box->videofg.timescale =
	moov->trak[track_num]->mdia->mdhd->timescale;
    /* extracts the information from stsd ann avc1 box */
    ret = mfu_capture_video_headers(moov->trak[track_num]->stbl,
				    parser_data);
    if (ret <= 0)
	return ret;
    /*Write Sps/PPs details into moof2mfu desc*/
    if (parser_data->video[parser_data->vid_trak_num].dump_pps_sps) {
	mfu_write_sps_pps(parser_data->video[parser_data->vid_trak_num].avc1,
			  moof2mfu_desc);
	parser_data->video[parser_data->vid_trak_num].dump_pps_sps =
	    0;
    }
    /* update the moof2mfu_desc with mfu details like
       sample_sizes,mdat,duration */
    if (mfu_handle_video_moof ( moof_data,
			       &parser_data->video[parser_data->vid_trak_num], 
				moof2mfu_desc ) == VPE_ERROR) {
	ret = VPE_ERROR;
	return ret;
    }

    return VPE_SUCCESS;
}

/*
  This function is uded to free all the elements of moof2mfu descriptor */
int32_t free_moof2mfu_desc(moof2mfu_desc_t* moof2mfu_desc)
{
    if (moof2mfu_desc != NULL) {
	if (moof2mfu_desc->mfu_raw_box != NULL) {
	    if (moof2mfu_desc->is_video) {
		if (moof2mfu_desc->mfu_raw_box->videofg.sample_duration
		    != NULL)
		    free(moof2mfu_desc->mfu_raw_box->videofg.sample_duration);
		if (moof2mfu_desc->mfu_raw_box->videofg.codec_specific_data
		    != NULL)
		    free(moof2mfu_desc->mfu_raw_box->videofg.codec_specific_data);
		if (moof2mfu_desc->vdat != NULL)
		    free(moof2mfu_desc->vdat);
	    }
	    if (moof2mfu_desc->is_audio) {
		if (moof2mfu_desc->mfu_raw_box->audiofg.sample_duration
		    != NULL)
		    free(moof2mfu_desc->mfu_raw_box->audiofg.sample_duration);
		if
		    (moof2mfu_desc->mfu_raw_box->audiofg.codec_specific_data
		     != NULL)
		    free(moof2mfu_desc->mfu_raw_box->audiofg.codec_specific_data);
		if (moof2mfu_desc->adat != NULL)
		    free(moof2mfu_desc->adat);
	    }
	}
	free(moof2mfu_desc->mfu_raw_box);
    }
    free(moof2mfu_desc);
    return VPE_SUCCESS;
}

/* Initializes all the elements of moof2mfu descriptor */
int32_t init_moof2mfu_desc( moof2mfu_desc_t* moof2mfu_desc)
{

    moof2mfu_desc->vdat_size = 0;
    moof2mfu_desc->adat_size = 0;
    moof2mfu_desc->sps_size = 0;
    moof2mfu_desc->pps_size = 0;
    moof2mfu_desc->mfub_box_size = 0;
    moof2mfu_desc->mfu_raw_box_size = 0;
    moof2mfu_desc->is_audio = 0;
    moof2mfu_desc->is_video = 0;
    moof2mfu_desc->mfu_raw_box = (mfu_rwfg_t*)
	nkn_malloc_type(sizeof(mfu_rwfg_t), mod_vpe_mfu_rwfg_t);
    moof2mfu_desc->mfu_raw_box->videofg.sample_duration = NULL;
    moof2mfu_desc->mfu_raw_box->videofg.composition_duration = NULL;
    moof2mfu_desc->mfu_raw_box->videofg.sample_sizes = NULL;
    moof2mfu_desc->mfu_raw_box->videofg.codec_info_size = 0;
    moof2mfu_desc->mfu_raw_box->videofg.codec_specific_data = NULL;
    moof2mfu_desc->vdat = NULL;
    moof2mfu_desc->mfu_raw_box->videofg_size = 0;

    moof2mfu_desc->mfu_raw_box->audiofg.sample_duration = NULL;
    moof2mfu_desc->mfu_raw_box->audiofg.composition_duration = NULL;
    moof2mfu_desc->mfu_raw_box->audiofg.sample_sizes = NULL;
    moof2mfu_desc->mfu_raw_box->audiofg.codec_info_size = 0;
    moof2mfu_desc->mfu_raw_box->audiofg.codec_specific_data = NULL;
    moof2mfu_desc->adat = NULL;
    moof2mfu_desc->mfu_raw_box->audiofg_size = 0;

    return VPE_SUCCESS;
} 

void
moof2mfu_clean_mfu_data(mfu_data_t *mfu_data)
{
    if(mfu_data != NULL) {
	if(mfu_data->data != NULL)
	    free(mfu_data->data);
	free(mfu_data);
    }

}


void
ismv_moof2_mfu_clean_audio_trak( ismv_parser_t *parser_data, int32_t
				 aud_trak_num)
{
    audio_pt *audio =  &parser_data->audio[aud_trak_num];
    if(audio != NULL) {
	if(audio->moof_data !=  NULL)
	    free(audio->moof_data);
	if(audio->esds != NULL)
	    free(audio->esds);
	if(audio->header != NULL)
	    free(audio->header);
	if(audio->adts != NULL)
	    free(audio->adts);
	//need to free adts and moofs
    }
}

void
ismv_moof2_mfu_clean_video_trak( ismv_parser_t *parser_data, int32_t
			    vid_trak_num)
{
    video_pt *video =  &parser_data->video[vid_trak_num];
    if(video->out != NULL)
	//mfu_free_mdat_desc(video->out);
    if(video->moof_data !=  NULL)
	free(video->moof_data);
    if(video->sps_pps !=  NULL)
        free(video->sps_pps);
    //free avc1
    mfu_free_ind_video_stream(video);
}
