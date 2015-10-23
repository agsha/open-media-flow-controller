#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>


#include "nkn_vpe_moof2mfu_api.h"

#include "mfp_file_converter_intf.h"
#include "nkn_vpe_parser_err.h"
#include "nkn_vpe_mp4_ismv_access_api.h"
#include "nkn_vpe_mp4_seek_api.h"
#include "nkn_vpe_ismv_reader.h"
#include "nkn_memalloc.h"
#include "nkn_vpe_mfu_aac.h"
#include "nkn_vpe_sync_http_read.h"
#include "nkn_vpe_utils.h"
#include "nkn_vpe_mfp_ts2mfu.h"

uint32_t moof2mfu_wirter_array_free=0;
uint32_t moof2mfu_wirter_array_alloc = 0;
int64_t glob_free_vid = 0;
int64_t glob_free_aud = 0;

#if 1
static io_handlers_t iohldr = {
    .ioh_open = nkn_vpe_fopen,
    .ioh_read = nkn_vpe_fread,
    .ioh_tell = nkn_vpe_ftell,
    .ioh_seek = nkn_vpe_fseek,
    .ioh_close = nkn_vpe_fclose,
    .ioh_write = nkn_vpe_fwrite
};

static io_handlers_t ioh_http = {
    .ioh_open = vpe_http_sync_open,
    .ioh_read = vpe_http_sync_read,
    .ioh_tell = vpe_http_sync_tell,
    .ioh_seek = vpe_http_sync_seek,
    .ioh_close = vpe_http_sync_close,
    .ioh_write = NULL
};
#endif
static int32_t
moof2mfu_write_dummy_mfu(int32_t n_aud_traks, int32_t n_vid_traks,
                         moof2mfu_write_array_t *writer_array, int32_t num_pmf);

static int32_t
init_moof2mfu_builder(int32_t, int32_t,
		      char **, int32_t *, int32_t *,
		      moov_t *,
		      moof2mfu_builder_t **, int32_t *, io_handlers_t *);
static int32_t
moof2mfu_fill_buffer(moof2mfu_converter_ctx_t *, void **);

static int32_t
ismv2moof_setup_writer(moof2mfu_write_t *,
                       moof2mfu_builder_t *,
                       mfu_data_t *);
static void
moof2mfu_cleanup_mfu_builder(moof2mfu_builder_t *);

static int32_t
moof2mfu_setup_writer(moof2mfu_write_t *,
		      moof2mfu_builder_t *,
		      mfu_data_t *);

int32_t
init_moof2mfu_converter(file_conv_ip_params_t *fconv_params,
                       void **conv_ctx)
{
    moov_t *moov = NULL;
    uint32_t n_traks = 0, m=0;
    ismv_parser_ctx_t *ismv_ctx = NULL;
    moof2mfu_builder_t *bldr = NULL;
    moof2mfu_converter_ctx_t *moof2mfu_ctx = NULL;
    ismv_reader_t *reader = NULL;
    ismc_publish_stream_info_t *psi = NULL;
    int32_t err = 0;
    FILE *fp = NULL, *fout =NULL;
    uint16_t audio_rate =0, video_rate = 0;
    uint32_t i=0;
    ism_bitrate_map_t *ism_map = NULL;
    int32_t trak_type_idx = 0, curr_profile = 0, curr_trak_id[MAX_TRACKS],
        temp = 0, j = 0, num_moofs[MAX_TRACKS];
    uint32_t k = 0;
    int32_t trak_type[MAX_TRACKS], trak_num[MAX_TRACKS];
    FILE *ismv_file[MAX_TRACKS];
    char *ismv_name_arr[MAX_TRACKS];
    char *tmp = NULL, ism_file_path[500] = {'\0'};
    char ismc_name[256];
    uint32_t string_len = 0;
    io_handlers_t *iocb = NULL;
    ismc_cust_attr_t *ca;
    ismc_cust_attr_t *ca_ismc;
    uint32_t ca_ismc_attr_count = 0, l =0;

    err = 0;
    *conv_ctx = NULL;
    reader = NULL;
    *conv_ctx = moof2mfu_ctx = (moof2mfu_converter_ctx_t*)\
        nkn_calloc_type(1, sizeof(moof2mfu_converter_ctx_t),\
                        mod_vpe_moof2mfu_converter_ctx_t);
    if (!moof2mfu_ctx) {
        err = -E_VPE_PARSER_NO_MEM;
        return err;
    }

    if (strstr(fconv_params->input_path[1], "http://")) {
        iocb = &ioh_http;
    } else {
        iocb = &iohldr;
    }
    err = ism_read_map_from_file(fconv_params->input_path[1],
                                 &ism_map, iocb);
    if (err) {
        goto error;
    }
    /*extracting the path of .ism file to use it for .ismc file and
      .ismv file*/
    tmp = strrchr(fconv_params->input_path[1], '/')+1;
    string_len = tmp - fconv_params->input_path[1] ;
    strncpy(ism_file_path, fconv_params->input_path[1], string_len);
    ism_file_path[string_len] = '\0';
    //forming ismc file name along with the path
    snprintf(ismc_name, 256, "%s/%s", ism_file_path,
             ism_map->ismc_name);

    err = ismc_read_profile_map_from_file(ismc_name,
                                          &psi, iocb);
    if (err) {
        goto error;
    }
    if(psi->attr[0]->n_profiles > MAX_TRACKS) {
	err = -E_VPE_MAX_PROFILE_ERR;
	goto error;
    }

    /* create a valid ismv context from the ISMV file */
    /* create individual ismv contxt for individual track */
    for (i=0;i< MAX_TRAK_TYPES;i++) {
	temp = psi->attr[i]->n_profiles;
	if(temp !=0) {
	    for (j=0;j<temp;j++) {
		char ismv_name[256];
		const char *src_video_name;
		char *ca_ismc_name[MAX_ATTR]={NULL};
                char *ca_ismc_val[MAX_ATTR]={NULL};
                ca_ismc_attr_count = 0;
		trak_type_idx = i;
		trak_type[k] = i;
		curr_profile =
		    psi->attr[i]->bitrates[j];
		curr_trak_id[k] =
		    ism_get_track_id(ism_map,
				     curr_profile,
				     trak_type_idx);

		TAILQ_FOREACH(ca_ismc,
			      &psi->attr[i]->cust_attr_head[j],
			      entries) {
		    //printf("ca_ismc->name: %s\n", ca_ismc->name);
		    ca_ismc_name[ca_ismc_attr_count] = strdup(ca_ismc->name);
		    ca_ismc_val[ca_ismc_attr_count] = strdup(ca_ismc->val);
		    ca_ismc_attr_count++;
		}

		src_video_name = ism_get_video_name(ism_map,
						    i,
						    curr_trak_id[k],
						    curr_profile,
						    &ca_ismc_name[0],
						    &ca_ismc_val[0], ca_ismc_attr_count);
		if(src_video_name == NULL) {
                    err = -E_VPE_MISMATCH_CUST_ATTR;
                    goto error;
                }

		snprintf(ismv_name, 256, "%s/%s", ism_file_path,
			 src_video_name);
		ismv_name_arr[k] = strdup(ismv_name);
		for(l=0; l<ca_ismc_attr_count; l++) {
                    if(ca_ismc_name[l])
                        free(ca_ismc_name[l]);
                }
		err = mp4_create_ismv_ctx_from_file(
						    ismv_name,
						    &ismv_ctx, iocb);
		moof2mfu_ctx->ismv_ctx[k] = ismv_ctx;
		mp4_get_trak_num(ismv_ctx, &trak_num[k],
                             curr_trak_id[k]);
		num_moofs[k] = mp4_get_frag_count(ismv_ctx, trak_num[k]);
		mp4_set_reader(ismv_ctx, iocb);
		k++;
	    }//for loop for j
	}//if(temp !=0)
    }//for loop for i
    n_traks = k;

    //tobe assigned later 
    audio_rate = 0;
    video_rate = 0;

    err = init_moof2mfu_builder(fconv_params->key_frame_interval,
				n_traks, ismv_name_arr, trak_type,
				num_moofs,
				moof2mfu_ctx->ismv_ctx[0]->moov,
				&bldr, trak_num, iocb);
    if (err) {
        goto error;
    }

    moof2mfu_ctx->bldr = bldr;
    if (ism_map != NULL)
        ism_cleanup_map(ism_map);
    if (psi != NULL)
        ismc_cleanup_profile_map(psi);    
    for (m=0; m<n_traks; m++) {
	if (ismv_name_arr[m])
	    free(ismv_name_arr[m]);
    }

    return err;
 error:
    if (ism_map != NULL)
	ism_cleanup_map(ism_map);
    if (psi != NULL)
	ismc_cleanup_profile_map(psi);
    if (ismv_ctx) mp4_cleanup_ismv_ctx(ismv_ctx);
    if (moov) mp4_moov_cleanup(moov, moov->n_tracks);
    if (fp) fclose(fp);
    for (m=0; m<n_traks; m++) {
        if (ismv_name_arr[m])
            free(ismv_name_arr[m]);
    }
    return err;
}



static int32_t
init_moof2mfu_builder(int32_t chunk_time,
		      int32_t n_traks,
		      char **ismv_name_arr,
		      int32_t *trak_type,
		      int32_t *num_moofs,
		      moov_t *moov,
		      moof2mfu_builder_t **out, 
		      int32_t *trak_num, io_handlers_t *iocb)
{
    moof2mfu_builder_t *bldr;
    int32_t err, i, trak_type_idx;
    FILE *fout;
    trak_t  *trak;
    FILE *fp;
    void *io_desc = NULL;

    *out = bldr = NULL;
    err = 0;

    *out = bldr = (moof2mfu_builder_t *)nkn_calloc_type\
	(1, sizeof(moof2mfu_builder_t),	mod_vpe_moof2mfu_builder_t);
    if (!bldr) {
        err = -E_VPE_PARSER_NO_MEM;
        return err;
    }
    bldr->iocb = iocb;
    bldr->chunk_time = chunk_time;
    bldr->audio_rate = 0;
    bldr->video_rate = 0;
    bldr->n_traks = n_traks;
    //bldr->fin = NULL;
    bldr->io_desc = NULL;

    bldr->vid_trak_num = bldr->aud_trak_num = 0;
    bldr->n_vid_traks = bldr->n_aud_traks = 0;
    bldr->file_dump=0;
    bldr->vid_cur_frag=0;
    bldr->aud_cur_frag = 0;
    bldr->chunk_num = 0;
    bldr->aud_pos = 0;
    bldr->vid_pos = 0;
    bldr->moof2mfu_desc = NULL;
    bldr->num_moof_processed = 0;
    bldr->seq_num = 0;
    bldr->is_dummy_moofs_written = 0;
    for(i=0;i<bldr->n_traks;i++){
        trak_type_idx = trak_type[i];
        if(trak_type_idx == 0){
	    if(bldr->n_vid_traks >= MAX_TRACKS) {
		err = -E_VPE_MAX_PROFILE_ERR;
		return err;
	    }
            video_pt *video = &bldr->video[bldr->n_vid_traks];
	    video->out = NULL;
	    video->sps_pps = NULL;
	    video->avc1 = NULL;
            video->list = NULL;
            video->moof_data = NULL;
	    //fp = fopen(ismv_name_arr[i], "rb");
	    io_desc = bldr->iocb->ioh_open((char*)\
						 ismv_name_arr[i],
						 "rb", 0);
	    if (!io_desc) {
		err = -E_VPE_PARSER_INVALID_FILE;
		printf("invalid input path (errno=%d)\n",
		       err);
		return err;
	    }
	    bldr->video[bldr->n_vid_traks].profile_seq = i;
	    //bldr->video[bldr->n_vid_traks].ismv_file = fp;
	    bldr->video[bldr->n_vid_traks].io_desc = io_desc;
	    bldr->video[bldr->n_vid_traks].track_num = trak_num[i] -1;
	    bldr->video[bldr->n_vid_traks].num_moof_stored = 0;
	    bldr->video[bldr->n_vid_traks].num_moof_processed = 0;
	    bldr->video[bldr->n_vid_traks].num_moofs = num_moofs[i];
	    bldr->video[bldr->n_vid_traks].moof_len = (uint32_t*)
		nkn_malloc_type((num_moofs[i]*sizeof(uint32_t)),
				mod_vpe_moof_temp_buf);
	    if (!bldr->video[bldr->n_vid_traks].moof_len) {
		err = -E_VPE_PARSER_NO_MEM;
		return err;
	    }
	    bldr->video[bldr->n_vid_traks].moofs = 
		nkn_calloc_type(num_moofs[i], sizeof(uint32_t*),
				mod_vpe_moof_temp_buf);
	    if (!bldr->video[bldr->n_vid_traks].moofs) {
                err = -E_VPE_PARSER_NO_MEM;
                return err;
            }

            bldr->n_vid_traks++;
        }
        if(trak_type_idx == 1/*SOUN_HEX*/){
	    if(bldr->n_aud_traks >= MAX_TRACKS) {
                err = -E_VPE_MAX_PROFILE_ERR;
                return err;
            }
            audio_pt *audio =
		&bldr->audio[bldr->n_aud_traks];
	    audio->out = NULL;
	    audio->esds = NULL;
            audio->esds_flag = 0;
            audio->adts = NULL;
            audio->list = NULL;
            audio->moof_data = NULL;
	    bldr->audio[bldr->n_aud_traks].profile_seq = i;
	    bldr->audio[bldr->n_aud_traks].track_num = trak_num[i]-1;
	    bldr->audio[bldr->n_aud_traks].num_moof_stored = 0;
            bldr->audio[bldr->n_aud_traks].num_moof_processed = 0;
	    bldr->audio[bldr->n_aud_traks].num_moofs = num_moofs[i];
	    bldr->audio[bldr->n_aud_traks].moof_len = (uint32_t*)
                nkn_malloc_type((num_moofs[i]*sizeof(uint32_t)),
                                mod_vpe_moof_temp_buf);
	    if (!bldr->audio[bldr->n_aud_traks].moof_len) {
                err = -E_VPE_PARSER_NO_MEM;
                return err;
            }
            bldr->audio[bldr->n_aud_traks].moofs =
                nkn_calloc_type(num_moofs[i], sizeof(uint32_t*),
                                mod_vpe_moof_temp_buf);
	    if (!bldr->audio[bldr->n_aud_traks].moofs) {
                err = -E_VPE_PARSER_NO_MEM;
                return err;
            }
            bldr->n_aud_traks++;
        }
    }
    // check whether the moofs across all the profiles is same
    for(i=0; i< (bldr->n_vid_traks-1); i++) {
        if(bldr->video[i].num_moofs != bldr->video[i+1].num_moofs) {
            err = -E_VPE_DIFF_MOOFS_ACROSS_PROFILE;
            return err;
        }
    }

    return err;
}

int32_t
moof2mfu_process_frags(void *private_data, void **out)
{
    moof2mfu_converter_ctx_t *conv_ctx;
    int32_t err;

    err = 0;
    conv_ctx = (moof2mfu_converter_ctx_t*)(private_data);
    err = moof2mfu_fill_buffer(conv_ctx, out);
    return err;
}

static int32_t
moof2mfu_fill_buffer(moof2mfu_converter_ctx_t *conv_ctx, void **out)
{
    ismv_parser_ctx_t *ctx = NULL;
    moof2mfu_write_array_t *writer_array = NULL;
    moof2mfu_builder_t *bldr = NULL;
    ismv_reader_t *reader = NULL;
    int32_t k = 0,l = 0;
    uint32_t i = 0;
    mfu_data_t* mfu_data = NULL;
    uint32_t aud_num_moof_processed=0;
    uint32_t vid_num_moof_processed=0;
    uint32_t temp = 0;
    int32_t temp_vid_trak_num=0;
    int32_t temp_num=0;
    int32_t ret = 0;
    moov_t *aud_moov, *vid_moov;
    writer_array = (moof2mfu_write_array_t *) 
	nkn_calloc_type (1, sizeof(moof2mfu_write_array_t),
			 mod_vpe_moof2mfu_write_array_t);
    if (!writer_array) {
	return -E_VPE_PARSER_NO_MEM;
    }

    writer_array->num_mfu_towrite = 0;
    writer_array->is_moof2mfu_conv_stop = 0;
    writer_array->is_last_moof = 0;
    bldr = conv_ctx->bldr;
    writer_array->num_pmf = bldr->num_pmf;
    //writer_array->n_traks = bldr->n_traks;
    writer_array->n_traks = bldr->n_vid_traks;
    //writer_array->num_moofs = bldr->num_moofs;
    if (bldr->n_vid_traks ==0)
	return -E_VPE_AUD_ONLY_NOT_SUPPORTED;
    if (bldr->n_aud_traks == 0)
	return -E_VPE_VID_ONLY_NOT_SUPPORTED;


    for (k=0; k<bldr->n_aud_traks; k++) {
	for (l=0; l<bldr->n_vid_traks; l++) {
	    if((bldr->audio[k].num_moof_processed <
		bldr->audio[k].num_moof_stored) &&
	       (bldr->video[l].num_moof_processed <
		bldr->video[l].num_moof_stored )) {
		mfu_data = (mfu_data_t*)nkn_calloc_type(1,
							sizeof(mfu_data_t),
							mod_vpe_mfu_data_t1);
		if (!mfu_data) {
		    return -E_VPE_PARSER_NO_MEM;
		}
		//ctx = conv_ctx->ismv_ctx[l];
		aud_moov =
		    conv_ctx->ismv_ctx[bldr->audio[k].profile_seq]->moov;
		vid_moov =
		    conv_ctx->ismv_ctx[bldr->video[l].profile_seq]->moov;

#if 0
		/* the temp_num indicates where the video track starts
		   in the given "n" number of tracks*/
		temp_vid_trak_num = ctx->moov->n_tracks_to_process -1;
		if (temp_vid_trak_num == bldr->n_vid_traks) 
		    temp_num = l + 1;//MBR
		else
		    temp_num = 1 ;//SBR
#endif
		bldr->profile_num = l;//stream_id
		bldr->io_desc = bldr->video[l].io_desc;
		bldr->audio_rate = bldr->audio[k].profile/1024;//to kbps
		bldr->video_rate = bldr->video[l].profile/1024;//to
		/* assuming that audio is the first trak and video
		   comes next */
		//bldr->video[l].track_num =temp_num;
		//bldr->audio[k].track_num = 0;
		aud_num_moof_processed =
		    bldr->audio[k].num_moof_processed;
		vid_num_moof_processed =
		    bldr->video[l].num_moof_processed;

		/* convert moof to mfu (one audiomoof + one
		   video_moof)*/
		if ((bldr->audio[k].moofs[aud_num_moof_processed] !=
		     NULL) &&
		    (bldr->video[l].moofs[vid_num_moof_processed] !=
		     NULL)) {
		    ret = ismv_moof2_mfu (
				    aud_moov,
				    vid_moov,
				    bldr->audio[k].moofs[aud_num_moof_processed],
				    bldr->video[l].moofs[vid_num_moof_processed],
				    bldr->audio[k].track_num,
				    bldr->video[l].track_num,
				    bldr, mfu_data);
		    if (ret <= 0) {
			int32_t m=0, n=0;
			uint32_t p=0;
			//free the already allocated vid and aud moofs
			for (m=0; m<bldr->n_aud_traks; m++) {
			    uint32_t temp_aud_moof_stored =
				bldr->audio[m].num_moof_stored;
			    uint32_t temp_aud_moof_tobe_freed =
				temp_aud_moof_stored - aud_num_moof_processed;
			    for (n=0; n<bldr->n_vid_traks; n++) {
				uint32_t temp_vid_moof_stored =
				    bldr->video[n].num_moof_stored;
				uint32_t temp_vid_moof_tobe_freed =
				    temp_vid_moof_stored -
				    vid_num_moof_processed;
				for(p=0; p<temp_vid_moof_tobe_freed;
				    p++) {
				    if(bldr->video[n].moofs[vid_num_moof_processed+p] != NULL) {
					free(bldr->video[n].moofs[vid_num_moof_processed+p]);
					bldr->video[n].moofs[vid_num_moof_processed+p] = NULL;
				    }
				}
			    }
			    for(p=0; p<temp_aud_moof_tobe_freed; p++){
				if(bldr->audio[m].moofs[aud_num_moof_processed+p] != NULL) {
				    free(bldr->audio[m].moofs[aud_num_moof_processed+p]);
				    bldr->audio[m].moofs[aud_num_moof_processed+p] = NULL;
				}
			    }
			}
			if(mfu_data != NULL) {
			    free(mfu_data);
			}
			if(writer_array != NULL) {
			    free(writer_array);
			}
			return ret;
#if 0
			if(bldr->is_dummy_moofs_written == 0) {
			  ret = moof2mfu_write_dummy_mfu(bldr->n_aud_traks, bldr->n_vid_traks,
							 writer_array, bldr->num_pmf);
			  if(ret < 0) {
			    return ret;
			  }
			  bldr->num_moof_processed++;
			  bldr->seq_num++;
			  writer_array->seq_num = bldr->seq_num;
			  *out = writer_array;
			  bldr->is_dummy_moofs_written = 1;
			  return E_VPE_FILE_CONVERT_CONTINUE;
			} 
#endif
		    }
		}
		if (!mfu_data) {
		    return E_VPE_FILE_CONVERT_CONTINUE;
		}
	    } else {
		if(bldr->is_dummy_moofs_written == 0) {
		  ret = moof2mfu_write_dummy_mfu(bldr->n_aud_traks, bldr->n_vid_traks,
						 writer_array, bldr->num_pmf);
		  if(ret < 0) {
		    return ret;
		  }
		    bldr->num_moof_processed++;
		    bldr->seq_num++;
		    writer_array->seq_num = bldr->seq_num;
		    *out = writer_array;
		    bldr->is_dummy_moofs_written = 1;
		    return E_VPE_FILE_CONVERT_CONTINUE;
		} else {
		writer_array->is_moof2mfu_conv_stop = 1;
		*out = writer_array;
		int32_t m=0, n=0;
		uint32_t p=0;
		//free the already allocated vid and aud moofs
		aud_num_moof_processed =
		    bldr->audio[k].num_moof_processed;
                vid_num_moof_processed =
                    bldr->video[l].num_moof_processed;
		for (m=0; m<bldr->n_aud_traks; m++) {
		    uint32_t temp_aud_moof_stored =
			bldr->audio[m].num_moof_stored;
		    uint32_t temp_aud_moof_tobe_freed =
			temp_aud_moof_stored -
			bldr->audio[m].num_moof_processed;;
		    for (n=0; n<bldr->n_vid_traks; n++) {
			uint32_t temp_vid_moof_stored =
			    bldr->video[n].num_moof_stored;
			uint32_t temp_vid_moof_tobe_freed =
			    temp_vid_moof_stored -
			    bldr->video[n].num_moof_processed;
			for(p=0; p<temp_vid_moof_tobe_freed;
			    p++) {
			    if(bldr->video[n].moofs[vid_num_moof_processed+p] != NULL) {
				free(bldr->video[n].moofs[vid_num_moof_processed+p]);
				bldr->video[n].moofs[vid_num_moof_processed+p] = NULL;
				glob_free_vid++;
			    }
			}
		    }
		    for(p=0; p<temp_aud_moof_tobe_freed; p++){
			if(bldr->audio[m].moofs[aud_num_moof_processed+p] != NULL) {
			    free(bldr->audio[m].moofs[aud_num_moof_processed+p]);
			    bldr->audio[m].moofs[aud_num_moof_processed+p] = NULL;
			    glob_free_aud++;
			}
		    }
		}
		}
		//return E_VPE_FILE_CONVERT_STOP;
		return E_VPE_FILE_CONVERT_CONTINUE;
		
	    }
	    //free the allocated video moof data which was processed
	    if (bldr->video[l].moofs[vid_num_moof_processed] != NULL) {
		free(bldr->video[l].moofs[vid_num_moof_processed]);
		bldr->video[l].moofs[vid_num_moof_processed] = NULL;
		glob_free_vid++;
	    }
	    bldr->video[l].num_moof_processed++;
	    writer_array->num_mfu_towrite++;
	    moof2mfu_setup_writer(&writer_array->writer[k+l], bldr,
				  mfu_data);
	    writer_array->streams_per_encap++;
	}//for n_vid_traks
	// free the allocated audio moof data which was processed
	if(bldr->audio[k].moofs[aud_num_moof_processed] != NULL) {
	    free(bldr->audio[k].moofs[aud_num_moof_processed]);
	    bldr->audio[k].moofs[aud_num_moof_processed] = NULL;
	    glob_free_aud++;
	}
	bldr->audio[k].num_moof_processed++;
    }//for n_aud_traks
    bldr->num_moof_processed++;
    bldr->seq_num++;
    if (bldr->total_moof_count ==  bldr->num_moof_processed)
	writer_array->is_last_moof = 1;

    writer_array->seq_num = bldr->seq_num;
    *out = writer_array;

    return E_VPE_FILE_CONVERT_CONTINUE;
}
static int32_t 
moof2mfu_write_dummy_mfu(int32_t n_aud_traks, int32_t n_vid_traks, 
			 moof2mfu_write_array_t *writer_array, int32_t num_pmf) 
{
  int32_t k = 0, l=0;
  int32_t err = 0;
  mfu_data_t* mfu_data = NULL;
  for (k=0; k<n_aud_traks; k++) {
    for (l=0; l<n_vid_traks; l++) {
      //creating dummy mfu's
      mfu_data = (mfu_data_t*)nkn_calloc_type(1,
					      sizeof(mfu_data_t),
					      mod_vpe_mfu_data_t2);
      if (mfu_data == NULL)
	return -E_VPE_PARSER_NO_MEM;
      mfub_t* mfu_hdr = nkn_calloc_type(1, sizeof(mfub_t),
					mod_vpe_mfu_data_t3);
      if (mfu_hdr == NULL) {
	free(mfu_data);
	return -E_VPE_PARSER_NO_MEM;
      }
      /* allocate for the mfu */
      mfu_data->data_size = MFU_FIXED_SIZE + sizeof(mfub_t);
      mfu_data->data = (uint8_t*) nkn_malloc_type(mfu_data->data_size,
						  mod_vpe_mfp_ts2mfu_t);
      if (!mfu_data->data) {
	return -E_VPE_PARSER_NO_MEM;
      }
      mfu_hdr->offset_vdat = 0xffffffffffffffff;
      mfu_hdr->offset_adat = 0xffffffffffffffff;
      mfu_hdr->stream_id = k+l;
      mfu_hdr->mfu_size =  mfu_data->data_size;
      mfu_hdr->program_number = num_pmf;

      /* write the mfu header */
      mfu_write_mfub_box(sizeof(mfub_t),
			 mfu_hdr,
			 mfu_data->data);
      writer_array->num_mfu_towrite++;
      writer_array->writer[k+l].out = mfu_data;
      //moof2mfu_setup_writer(&writer_array->writer[k+l], bldr,
      //		    mfu_data);
      if(mfu_hdr)
	free(mfu_hdr);
    }
  }

  return err;
}

static int32_t
moof2mfu_setup_writer(moof2mfu_write_t *writer,
                       moof2mfu_builder_t *bldr,
                       mfu_data_t *mfu_data)
{
    writer->out = mfu_data;
    //writer->profile = bldr->curr_profile;
    //writer->psi = bldr->psi;
    //writer->curr_profile_idx = bldr->curr_profile_idx - 1;

    return 0;
}

void
moof2mfu_cleanup_converter(void *private_data)
{
    moof2mfu_converter_ctx_t *conv;
    moof2mfu_builder_t *bldr;
    int32_t i=0;

    conv = (moof2mfu_converter_ctx_t*)private_data;
    bldr  = conv->bldr;
    if (conv) {
	for (i=0; i<bldr->n_traks; i++) {
            free(conv->ismv_ctx[i]->moov->data);
            mp4_cleanup_ismv_ctx_attach_buf(conv->ismv_ctx[i]);
            mp4_moov_cleanup(conv->ismv_ctx[i]->moov,
                             conv->ismv_ctx[i]->moov->n_tracks);
            mp4_cleanup_ismv_ctx(conv->ismv_ctx[i]);
        }
        moof2mfu_cleanup_mfu_builder(conv->bldr);
        free(conv);
    }
}

static void
moof2mfu_cleanup_mfu_builder(moof2mfu_builder_t *bldr)
{
    int32_t i=0;
    uint32_t j=0;
    if (bldr) {
	for(i=0; i<bldr->n_vid_traks; i++) {
	  if(bldr->video[i].io_desc)
	  bldr->iocb->ioh_close(bldr->video[i].io_desc);
	    uint32_t vid_num_moof_processed =
		bldr->video[i].num_moof_processed;
	    uint32_t temp_vid_moof_stored =
		bldr->video[i].num_moof_stored;
	    uint32_t temp_vid_moof_tobe_freed =
		temp_vid_moof_stored -
		bldr->video[i].num_moof_processed;
	    for(j=0; j<temp_vid_moof_tobe_freed; j++) {
		if(bldr->video[i].moofs[vid_num_moof_processed+j] != NULL) {
		    free(bldr->video[i].moofs[vid_num_moof_processed+j]);
		    bldr->video[i].moofs[vid_num_moof_processed+j] = NULL;
		}
	    }
	    if(bldr->video[i].moof_len != NULL)
		free(bldr->video[i].moof_len);
	    if(bldr->video[i].moofs != NULL)
		free(bldr->video[i].moofs);
	}
	for(i=0; i<bldr->n_aud_traks; i++) {
	    uint32_t aud_num_moof_processed =
		bldr->audio[i].num_moof_processed;
	    uint32_t temp_aud_moof_stored =
		bldr->audio[i].num_moof_stored;
            uint32_t temp_aud_moof_tobe_freed =
                temp_aud_moof_stored -
                bldr->audio[i].num_moof_processed;
            for(j=0; j<temp_aud_moof_tobe_freed; j++) {
                if(bldr->audio[i].moofs[aud_num_moof_processed+j] !=
		   NULL) {
                    free(bldr->audio[i].moofs[aud_num_moof_processed+j]);
                    bldr->audio[i].moofs[aud_num_moof_processed+j] =
		    NULL;
                }
            }
            if(bldr->audio[i].moof_len != NULL)
                free(bldr->audio[i].moof_len);
            if(bldr->audio[i].moofs != NULL)
                free(bldr->audio[i].moofs);
        }
	//this bldr->io_desc is same as bldr->video[]->io_desc, it is already closed 
	//if(bldr->io_desc != NULL)
	//bldr->iocb->ioh_close(bldr->io_desc);
	free(bldr);
    }
}

void
moof2mfu_cleanup_writer_array(moof2mfu_write_array_t *writer_array)
{
    uint32_t i=0;
    moof2mfu_write_t *writer;
    if (writer_array != NULL) {
	for(i=0;i<writer_array->num_mfu_towrite;i++) {
	    writer = &writer_array->writer[i];
            if (writer->out != NULL){
                moof2mfu_clean_mfu_data(writer->out);
            }
	}
	free(writer_array);
	writer_array = NULL;

        moof2mfu_wirter_array_free++;
    }
}



int32_t
moof2mfu_write_output(void *out)
{
    moof2mfu_write_array_t *writer_array;
    writer_array = (moof2mfu_write_array_t*)out;
    moof2mfu_cleanup_writer_array(writer_array);
    return 0;
}
