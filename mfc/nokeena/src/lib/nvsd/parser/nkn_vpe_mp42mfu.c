#include <stdio.h>
#include <sys/stat.h>
#include "errno.h"
#include <sys/uio.h>
#include <sys/time.h>
#include <netinet/in.h>
#include "math.h"

#include "nkn_debug.h"
#include "nkn_assert.h"
#include "nkn_stat.h"
#include "nkn_vpe_mp42mfu_api.h"
#include "nkn_vpe_h264_parser.h"
#include "nkn_vpe_mfu_defs.h"

//#define PERF_TIMERS

#define AUDIO_SINGLE_MEMCPY
#define VIDEO_SINGLE_MEMCPY
//#define VIDEO_READ_WHOLESAMPLE
#define VIDEO_OPT_DURATION_CALC
#define AUDIO_OPT_DURATION_CALC
//#define AV_READ_STSC_WITHCTXT

#define NALU_DELIMIT 0x00000001
int32_t file_number = 0;

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

#if defined(VIDEO_READ_WHOLESAMPLE)

//define macro and data struct
#define MAX_NALU_IN_SAMPLE (10)
typedef struct NAL_index{
    uint32_t nal_size;
    uint32_t nal_type;
    uint32_t chunk_offset;
    uint32_t valid_entry;    
}NAL_index_t;

typedef struct part_vectors{
    uint32_t offset;
    uint32_t length;
}part_vectors_t;

#endif

static int32_t diffTimevalToMs(struct timeval const* from, 
	struct timeval const * val) {
    //if -ve, return 0
    double d_from = from->tv_sec + ((double)from->tv_usec)/1000000;
    double d_val = val->tv_sec + ((double)val->tv_usec)/1000000;
    double diff = d_from - d_val;
    if (diff < 0)
	return 0;
    return (int32_t)(diff * 1000);
}

#ifdef _BIG_ENDIAN_
/* data -> o/p pointer where data has to be written
 * pos -> i/p position of data where value has to e written
 * val -> i/p actual value to be written
 */
static int32_t  write32_byte_swap( uint8_t *data,uint32_t pos,uint32_t
	val)
{
    data[pos] = val>>24;
    data[pos+1] = (val>>16)&0xFF;
    data[pos+2] = (val>>8)&0xFF;
    data[pos+3] = val&0xFF;
    return 1;
}

#else
static int32_t  write32( uint8_t *data,uint32_t pos,uint32_t val)
{
    data[pos] = val&0xFF;
    data[pos+1] = (val>>8)&0xFF;
    data[pos+2] = (val>>16)&0xFF;
    data[pos+3] = (val>>24)&0xFF;
    return 1;
}
#endif


static uint32_t _read32(uint8_t *buf, int32_t pos)
{
    return
        (buf[pos+3]<<24)|(buf[pos+2]<<16)|(buf[pos+1]<<8)|(buf[pos]);
}
static uint32_t _read32_byte_swap(uint8_t *buf, int32_t pos)
{
    return
        (buf[pos]<<24)|(buf[pos+1]<<16)|(buf[pos+2]<<8)|(buf[pos+3]);
}

static uint32_t
uint32_byte_swap(uint32_t input){
    uint32_t ret=0;
    ret = (((input&0x000000FF)<<24) + ((input&0x0000FF00)<<8) +
           ((input&0x00FF0000)>>8) + ((input&0xFF000000)>>24));
    return ret;
}

/* This fucntion is used to allocate and intialize the mp4 to MFU
   builder structure.
   chunk_time-> i/p parameter, indicates the keyframe interval specified
   in PMF
   sl_pkg_filenames, streaming_type, num_pmf, n_traks, moov -- i/p param
   out -> o/p parameter mp42mfu builder which is updated
   iocb->i/p parameter */
int32_t
mp4_mfu_builder_init(int32_t chunk_time,
                     char **sl_pkg_filenames,
                     int32_t streaming_type,
                     int32_t num_pmf,
                     int32_t n_input_files,
                     mp4_parser_ctx_t **mp4_ctx,
                     mp4_mfu_builder_t **out)
{
    mp4_mfu_builder_t *bldr = NULL;
    char *tmp = NULL;
    int32_t i=0, j=0;
    int32_t err = 0, trak_type = 0;
    uint32_t n_vid_traks = 0, n_aud_traks = 0;
    trak_t *trak = NULL;
    *out = bldr = (mp4_mfu_builder_t*)
        nkn_calloc_type(1, sizeof(mp4_mfu_builder_t),
                        mod_mp4_mfu_builder_t);
    if (!bldr) {
        return -E_VPE_PARSER_NO_MEM;
    }
    bldr->num_pmf = num_pmf;
    bldr->chunk_time = chunk_time;
    bldr->streaming_type = streaming_type;
    bldr->n_profiles = n_input_files;
    bldr->sync_time = 0;
    bldr->profile_num = 0;
    //bldr->iocb = iocb;

    /*initializing the audio and video elements and other elements for
      each profile*/
    for(j=0; j<bldr->n_profiles; j++) {
	n_vid_traks = 0;
	n_aud_traks = 0;
	if (strstr(sl_pkg_filenames[j], "http://")) {
	  bldr->iocb[j] = &ioh_http;
	} else {
	  bldr->iocb[j] = &iohldr;
	}
	bldr->mp4_name[j] = strdup(sl_pkg_filenames[j]);
	bldr->mp4_file_path[j] = NULL;
	bldr->io_desc_mp4[j] = bldr->iocb[j]->ioh_open((char*)		\
					      bldr->mp4_name[j],
					      "rb", 0);
	if (!bldr->io_desc_mp4[j]) {
	    err = -E_VPE_PARSER_INVALID_FILE;
	    return err;
	}
	bldr->n_traks[j] = mp4_ctx[j]->n_traks;
	bldr->min_aud_trak_num[j] = 0;
	bldr->min_aud_bit_rate[j] = 0;

	for(i=0; i<mp4_ctx[j]->n_traks; i++) {
	    trak = mp4_ctx[j]->moov->trak[i];
	    trak_type = *((int32_t*)(&trak->mdia->hdlr->handler_type));
	    if(trak_type == VIDE_HEX){
		bldr->video[j][n_vid_traks].timescale = 0;
		bldr->video[j][n_vid_traks].track_id = 0;
		bldr->video[j][n_vid_traks].trak_num = 0;
		bldr->video[j][n_vid_traks].duration = 0;
		bldr->video[j][n_vid_traks].num_sync_points = 0;
		bldr->video[j][n_vid_traks].bit_rate = 0;
		bldr->video[j][n_vid_traks].default_sample_size = 0;
		bldr->video[j][n_vid_traks].default_sample_duration = 0;
		bldr->video[j][n_vid_traks].num_chunks_processed = 0;
		bldr->video[j][n_vid_traks].trak = trak;
		bldr->video[j][n_vid_traks].avc1 =
		    (stream_AVC_conf_Record_t*)nkn_calloc_type(1,
							       sizeof(stream_AVC_conf_Record_t),
							       mod_vpe_stream_AVC_conf_Record_t);
		if (!bldr->video[j][n_vid_traks].avc1) {
		    return -E_VPE_PARSER_NO_MEM;
		}
		bldr->video[j][n_vid_traks].sync_time_table = NULL;
		bldr->video[j][n_vid_traks].sync_sample_table = NULL;
		bldr->video[j][n_vid_traks].sync_sam_size_table =
		    NULL;
		bldr->video[j][n_vid_traks].sync_start_sam_table =
		    NULL;
		bldr->video[j][n_vid_traks].chunk_data = NULL;
		bldr->video[j][n_vid_traks].sample_time_table = NULL;

		n_vid_traks++;
	    }
	    if(trak_type == SOUN_HEX){
		bldr->audio[j][n_aud_traks].timescale = 0;
		bldr->audio[j][n_aud_traks].track_id = 0;
		bldr->audio[j][n_aud_traks].trak_num = 0;
		bldr->audio[j][n_aud_traks].duration = 0;
		bldr->audio[j][n_aud_traks].num_sync_points = 0;
		bldr->audio[j][n_aud_traks].bit_rate = 0;
		bldr->audio[j][n_aud_traks].default_sample_size = 0;
		bldr->audio[j][n_aud_traks].default_sample_duration = 0;
		bldr->audio[j][n_aud_traks].num_chunks_processed = 0;
		bldr->audio[j][n_aud_traks].trak = trak;
		bldr->audio[j][n_aud_traks].adts = (adts_t*) nkn_calloc_type(1, sizeof(adts_t),
									     mod_vpe_adts);
		if(!bldr->audio[j][n_aud_traks].adts) {
		    return -E_VPE_PARSER_NO_MEM;
		}
		bldr->audio[j][n_aud_traks].sync_time_table = NULL;
                bldr->audio[j][n_aud_traks].sync_sample_table = NULL;
                bldr->audio[j][n_aud_traks].sync_sam_size_table =
                    NULL;
                bldr->audio[j][n_aud_traks].sync_start_sam_table =
                    NULL;
                bldr->audio[j][n_aud_traks].chunk_data = NULL;
		bldr->audio[j][n_aud_traks].sample_time_table = NULL;
		n_aud_traks++;
	    }
	}
	bldr->n_aud_traks[j] = n_aud_traks;
	bldr->n_vid_traks[j] = n_vid_traks;
    }

    return err;
}


/* This function is used to update all the elemnts of the mp42mfu
   builder structure. It mainly updates the audio and video sample vector
   (sample_data_prop) and other elements whcih are used to for this
   vector updation. */ 

int32_t
mp4_mfu_bldr_update(mp4_mfu_builder_t *bldr, mp4_parser_ctx_t **ctx, int32_t n_profiles)
{
    int32_t err = 0, trak_type = 0;
    trak_t  *trak;
    uint32_t box_hdr_size = 0;
    uint64_t size = 0;
    uint32_t num_sync_point = 0, total_samp_size = 0, n_vid_traks = 0,
	n_aud_traks = 0;
    uint64_t total_temp_size = 0;
    //uint16_t temp_bit_rate = 0;
    uint32_t j = 0, l=0;
    int32_t m = 0, k=0;
    int32_t  i = 0;
    stbl_t *stbl = NULL;
    uint32_t temp_bit_rate = 0;
    uint32_t num_sync_points = 0;
    uint8_t *stss_initial_data;
    for(m=0; m< n_profiles; m++) {
	n_vid_traks = 0;
	n_aud_traks = 0;
	/* update the stsz,num_sync_points,timescale,duration,track_id,
	   track_num, sync tables for the correspoding tracks*/
	for(i=0; i<bldr->n_traks[m]; i++) {
	    trak = ctx[m]->moov->trak[i];
	    trak_type = *((int32_t*)(&trak->mdia->hdlr->handler_type));
	    stbl = ctx[m]->moov->trak[i]->stbl;
	    if(trak_type == VIDE_HEX){
		video_trak_t *video =  &bldr->video[m][n_vid_traks];
		bldr->is_video = 1;
		video->num_samples = mp4_get_stsz_num_entries(trak);
		video->timescale =
		  trak->mdia->mdhd->timescale;
		video->duration =
		  trak->mdia->mdhd->duration;
		
		video->track_id = trak->tkhd->track_id;
		video->trak_num = i;
		total_samp_size =
		  mp4_find_cumulative_sample_size(trak,
						  0,
						  video->num_samples);
		float temp1 = (float)total_samp_size /
                  ((float)video->duration/video->timescale);
		float temp3 = (temp1*8)/1000;
		float temp4 = ceil(temp3);
		video->bit_rate = (int32_t)temp4;
		//video->bit_rate = ceil((temp_bit_rate*8) /1000);//converting to kbps
		video->stsz_data = stbl->stsz_data;
	    //find the syncpoint and allocate memory for tables
		
		stss_initial_data = stbl->stss_data;
		if(stbl->stss_data != NULL) {
		  stbl->stss_data +=12;//FULL_BOX
		  num_sync_points = _read32_byte_swap(stbl->stss_data, 0);
		  if(num_sync_points == 0) {
		    err = -E_VPE_MP4_NO_STSS_TAB;
		    return err;
		  }
		  stbl->stss_data +=4;
		} else {
		  err = -E_VPE_MP4_NO_STSS_TAB;
		  return err;
		}
		stbl->stss_data = stss_initial_data;
		video->sync_time_table = (uint64_t*)
		  nkn_calloc_type(num_sync_points, sizeof(uint64_t),
				  mod_vpe_vid_sync_table);
		if(!video->sync_time_table) {
		return -E_VPE_PARSER_NO_MEM;
		}
		video->sync_sample_table = (uint32_t*)
		  nkn_calloc_type(num_sync_points, sizeof(uint32_t),
				  mod_vpe_vid_sync_table);
		if(!video->sync_sample_table) {
		  return -E_VPE_PARSER_NO_MEM;
		}
		
		video->sync_sam_size_table = (uint64_t*)
		  nkn_calloc_type(num_sync_points, sizeof(uint64_t),
				  mod_vpe_vid_sync_table);
		if(!video->sync_sam_size_table) {
		  return -E_VPE_PARSER_NO_MEM;
		}
		
		video->sync_start_sam_table = (uint32_t*)
		  nkn_calloc_type(num_sync_points, sizeof(uint32_t),
				  mod_vpe_vid_sync_table);
		if(!video->sync_start_sam_table) {
		  return -E_VPE_PARSER_NO_MEM;
		}
		video->chunk_data = nkn_calloc_type(num_sync_points,
						    sizeof(uint8_t*),
						    mod_vpe_chunk_data);
		if(!video->chunk_data) {
		  return -E_VPE_PARSER_NO_MEM;
		}
		
		err = mp4_mfu_update_vid_sync_tables(stbl->stss_data,
						     video->sync_time_table,
						     video->sync_sample_table,
						     video->sync_sam_size_table,
						     video->sync_start_sam_table,
						     trak,
						     stbl->stsz_data,
						     &video->num_sync_points, 
						     video->timescale);
		
		if(err)
		  return err;
		
		//update the avc specific info 
		err = mp4_mfu_update_avc(stbl->stsd_data, video);
		if(err) {
		  //free the data allocated for avc info
		  return err;
		}
		memset(&trak->stbl->stts_ctxt, 0, sizeof(runt_ctxt_t));
		n_vid_traks++;
	    }
	    if(trak_type == SOUN_HEX){
	      audio_trak_t *audio =  &bldr->audio[m][n_aud_traks];
	      bldr->is_audio = 1;
	      audio->num_samples =
		mp4_get_stsz_num_entries(trak);
	      audio->timescale =
		trak->mdia->mdhd->timescale;
	      audio->duration =
		trak->mdia->mdhd->duration;
	      audio->track_id = trak->tkhd->track_id;
	      audio->trak_num = i;
	      total_samp_size =
		mp4_find_cumulative_sample_size(trak,
						0,
						audio->num_samples);
	      audio->bit_rate = total_samp_size /
		(audio->duration/audio->timescale);
	      temp_bit_rate = total_samp_size /
		(audio->duration/audio->timescale);
	      audio->bit_rate = temp_bit_rate / 1000; //converting to kbps
	      audio->stsz_data = stbl->stsz_data;
	      
	      //update the avc specific info
	      err = mp4_mfu_update_adts(stbl->stsd_data, audio);
	      if(err) {
                //free the data allocated for avc info
                return err;
            }
	      n_aud_traks++;
	    }
	}
	bldr->n_aud_traks[m] = n_aud_traks;
        bldr->n_vid_traks[m] = n_vid_traks;
    }
    //check for audio and video trak
    if(bldr->is_video == 0) {
      err= -E_VPE_AUD_ONLY_NOT_SUPPORTED;
      if(bldr->is_audio) {
	for(m=0; m< n_profiles; m++) {
	  for(k=0; k<bldr->n_aud_traks[m]; k++) {
	    audio_trak_t *audio =  &bldr->audio[m][k];
	    if(audio->header)
	      free(audio->header);
	    if(audio->adts)
	      free(audio->adts);
	  }
	}
      }
      return err;
    }    

    /*after the above checks it is assumed that all the video traks will have
      same sync points */
    bldr->num_sync_points = bldr->video[0][0].num_sync_points;
    bldr->sync_time = bldr->video[0]->sync_time_table[0] /90000;/*covert
								from
								millisec to sec*/

    for(m=0; m<n_profiles; m++) {
	/*update the sync tables for all audio trak with the help of video sync
	  duration caluclated.*/
	video_trak_t *video =  &bldr->video[m][0];
	for (k=0; k<bldr->n_aud_traks[m]; k++) {
	    audio_trak_t *audio =  &bldr->audio[m][k];
	    trak = ctx[m]->moov->trak[audio->trak_num];
            audio->sync_time_table = (uint64_t*)
                nkn_calloc_type(bldr->num_sync_points, sizeof(uint64_t),
                                mod_vpe_aud_sync_table);
            if(!audio->sync_time_table) {
                return -E_VPE_PARSER_NO_MEM;
            }
            audio->sync_sample_table = (uint32_t*)
                nkn_calloc_type(bldr->num_sync_points, sizeof(uint32_t),
                                mod_vpe_aud_sync_table);
            if(!audio->sync_sample_table) {
                return -E_VPE_PARSER_NO_MEM;
            }
	    audio->sync_sam_size_table = (uint64_t*)
                nkn_calloc_type(bldr->num_sync_points, sizeof(uint64_t),
                                mod_vpe_aud_sync_table);
            if(!audio->sync_sam_size_table) {
                return -E_VPE_PARSER_NO_MEM;
            }

            audio->sync_start_sam_table = (uint32_t*)
                nkn_calloc_type(bldr->num_sync_points, sizeof(uint32_t),
                                mod_vpe_aud_sync_table);
            if(!audio->sync_start_sam_table) {
                return -E_VPE_PARSER_NO_MEM;
            }
            audio->chunk_data = nkn_calloc_type(bldr->num_sync_points,
                                                sizeof(uint8_t*),
                                                mod_vpe_chunk_data);
            if(!audio->chunk_data) {
                return -E_VPE_PARSER_NO_MEM;
            }
	    err =
		mp4_mfu_update_aud_sync_tables(video->sync_time_table,
					       video->num_sync_points,
					       audio->sync_time_table,
					       audio->sync_sample_table,
					       audio->sync_sam_size_table,
					       audio->sync_start_sam_table,
					       trak, audio->stsz_data,
					       &audio->num_sync_points,
					       audio->timescale);
	    
	    memset(&trak->stbl->stts_ctxt, 0, sizeof(runt_ctxt_t));

	    if(err)
		return err;
	    
	    if(k == 0){
		temp_bit_rate = bldr->audio[m][k].bit_rate;
	    } else {
		if(temp_bit_rate > bldr->audio[m][k].bit_rate)
		    temp_bit_rate = bldr->audio[m][k].bit_rate;
	    }
	    
	}
	//update the min bitrate audio trak
	bldr->min_aud_bit_rate[m] = temp_bit_rate;
    }
    //check for constant no:of:sync points across the profiles
    num_sync_points = bldr->video[0][0].num_sync_points;
    for(m=0; m<n_profiles; m++) {
	for(k=0; k<bldr->n_vid_traks[m]; k++) {
	    if(bldr->video[m][k].num_sync_points != num_sync_points){
		err= -E_VPE_MP4_MISALIGMENT;
	    }
	}
    }
    //check for sync_tables consistency across profiles
    /*assigning the first video profile table as reference. It also
      checks the first profile table once again. need to optimize it */
    //sync_start_sam_table = bldr->video[0][0].sync_start_sam_table;
    for(m=0; m<n_profiles; m++) {
        for(k=0; k<bldr->n_vid_traks[m]; k++) {
	    for(l=0; l<num_sync_points; l++) {
		if(bldr->video[0][0].sync_start_sam_table[l] !=
		   bldr->video[m][k].sync_start_sam_table[l]) {
		    err = -E_VPE_MP4_MISALIGMENT;
		    return err;
		}
	    }
	}
    }
    //after above check all the profiles will have same sync_points 
    bldr->num_sync_points = bldr->video[0][0].num_sync_points;

    return err;
}

int32_t
mp4_mfu_update_adts(uint8_t *stsd_data, audio_trak_t *aud)
{
    int32_t err = 0;
    uint8_t *esds;
    /*Get the ESDS box*/
    if(mp4_get_esds_box(stsd_data,&esds) == VPE_ERROR)
        return -E_VPE_MP4_ESDS_ERR;

    /*Handle esds to get adts data*/
    if(mp4_handle_esds(esds, aud->adts) == VPE_ERROR)
        return -E_VPE_MP4_ESDS_ERR;
    mfu_form_aac_header((adts_t*)(aud->adts), &aud->header,
			&aud->header_size);


    return err;
}

int32_t
mp4_mfu_update_avc(uint8_t *stsd_data, video_trak_t *vid)
{
    box_t box;
    size_t bytes_read, total_bytes_read = 0;
    int32_t box_size,size = 10000, err = 0;//initial_data;
    char avc1_id[] = {'a', 'v', 'c', '1'};
    char avcC_id[] = {'a', 'v', 'c', 'C'};
    uint32_t stsd_size = 0;
    int32_t avcC_size = -1, avc1_size = 0;
    uint8_t* p_data = NULL, *p_avc1 = NULL;
    int32_t avcC_offset = -1, read_size = 0;
    uint8_t *avcC_data = NULL;
    uint8_t *initial_data = NULL;

    initial_data = stsd_data;

    stsd_size = nkn_vpe_swap_uint32(read_unsigned_dword(stsd_data));
    stsd_data+=16;//FULL BOX
    while(total_bytes_read < (size_t)stsd_size)
      {
	  box_size = read_next_root_level_box(stsd_data, size, &box,
					      &bytes_read);
	  total_bytes_read += box_size;
	  if(check_box_type(&box,avc1_id))
	    {
	      avc1_size = box_size;
	      stsd_data=stsd_data+bytes_read;
	      while(read_size < avc1_size){
		memcpy(box.type, stsd_data, 4);
#if 0
	      stsd_data+=86;
	      box_size = read_next_root_level_box(stsd_data, size,
						  &box,
						  &bytes_read);
#endif
	      if(check_box_type(&box,avcC_id))
		{
		  //-4 is for pointing just before the size of avcC box
		  avcC_offset = stsd_data - initial_data -4;
		  avcC_size = _read32_byte_swap(stsd_data-4, 0);
		  break;
		}
	      stsd_data += 1;
	      read_size++;
	      }	      
	    }
      }
    stsd_data = initial_data;
    if(avcC_offset != -1) {
      avcC_data = (uint8_t*) nkn_malloc_type(avcC_size,
					     mod_vpe_avcC_data);
      if (!avcC_data) {
	err = -E_VPE_PARSER_NO_MEM;
      }
      memcpy(avcC_data, stsd_data+avcC_offset, avcC_size);
      mp4_read_avc1(vid->avc1,avcC_data);
      if(avcC_data)
	free(avcC_data);
      //err = mfu_check_h264_profile_level(vid->avc1);
    } else {
      err = E_VPE_NO_AVCC_BOX;
    }
    return err;
}

/* This fucntion is used to get the sample vector for each sample */
int32_t 
mp4_get_sample_pos(uint32_t sample_num, sample_prop_t
		   *sample_data_prop, sample_prop_t *prev_sample,
		   trak_t *trak, uint32_t sample_size)
{
    int32_t err = 0;
    int32_t chunk_num = 0, first_sample_in_chunk = 0;
    int64_t chunk_offset = 0, sample_offset = 0;
    int32_t tot_samples = 0;
    uint8_t *stsc_data = trak->stbl->stsc_data;
    int32_t stsc_entry_count = 0;
    int32_t stsc_first_chunk = 0;
    int32_t stsc_num_sample_chnk = 0;
    stsc_data+=12;
    //reading the details of stsc box if the stsc entry is one
    stsc_entry_count = _read32_byte_swap(stsc_data, 0);
    if(stsc_entry_count == 1) {
      stsc_first_chunk = _read32_byte_swap(stsc_data, 4);
      stsc_num_sample_chnk = _read32_byte_swap(stsc_data, 8);
    }

    tot_samples = mp4_get_stsz_num_entries(trak);
    /*If stsc box has single entry, then the last chunk_num returned 
     * from mp4_sample_to_chunk_num is not correct, so handling it outside*/
    if(stsc_entry_count == 1) {
      if((tot_samples - sample_num) == (uint32_t)stsc_num_sample_chnk) {
	chunk_num = prev_sample->chunk_num+1;
	first_sample_in_chunk = tot_samples - stsc_num_sample_chnk;
	goto chunk_offset_calc;
      } else if((tot_samples - sample_num) < (uint32_t)stsc_num_sample_chnk) {
	chunk_num = prev_sample->chunk_num;
	first_sample_in_chunk = tot_samples - stsc_num_sample_chnk;
	goto chunk_offset_calc;
      }
    }


    chunk_num =
      mp4_sample_to_chunk_num(trak, sample_num, &first_sample_in_chunk,
			      NULL);
    //printf("trakpos %x sample_num %u chunk_num %d 1stsample %d\n", 
    //	(int32_t)trak->pos, sample_num, chunk_num, first_sample_in_chunk);

 chunk_offset_calc:
    if(trak->stbl->stco_data == NULL) {
      chunk_offset = mp4_chunk_num_to_chunk_offset_co64(trak,
							chunk_num-1);
    } else {
      chunk_offset = mp4_chunk_num_to_chunk_offset(trak,
						   chunk_num-1);
    }

    if(sample_data_prop->is_updated == 0) {
	sample_offset = chunk_offset +
	    mp4_find_cumulative_sample_size(trak,
					    first_sample_in_chunk,
					    sample_num);
	sample_data_prop->is_updated = 1;
    } else {
        if(chunk_num == prev_sample->chunk_num) {
            sample_offset = prev_sample->sample_offset +
		prev_sample->sample_size;
        }
        else{
            //Base has changed.
	    sample_offset = chunk_offset;
        }
    }
    //store this sample's number
    sample_data_prop->sample_num = sample_num;
    sample_data_prop->sample_size = sample_size;
    sample_data_prop->sample_offset = sample_offset;
    sample_data_prop->chunk_num = chunk_num;
    sample_data_prop->chunk_offset = chunk_offset;
    sample_data_prop->s_list_num_entries = 1;
    err = mp4_reader_alloc_sample_desc(sample_data_prop,
				       sample_data_prop->s_list_num_entries);
    if(err)
	return err;
    sample_data_prop->s_list->offset = sample_offset;
    sample_data_prop->s_list->length = sample_size;

    return err;
}

int32_t
mp4_reader_alloc_sample_desc(sample_prop_t *sample_data_prop, int32_t
			     num_entries)
{
    sample_data_prop->s_list = (sample_desc_t*)
        nkn_calloc_type(num_entries, sizeof(sample_desc_t),
                        mod_vpe_sample_desc_t);

    if (!sample_data_prop->s_list) {
        return -E_VPE_PARSER_NO_MEM;
    }

    return 0;
}

#if defined(VIDEO_SINGLE_MEMCPY)
static int32_t mfu_make_vdat_box(
    uint8_t 		*vdatbox_pos, 	/*output buffer*/
    uint32_t 		*vdat_size, 	/*input*/
    video_trak_t 	*vid, 		/*input, few mem will be freed*/
    io_handlers_t	*iocb, 		/*input*/ 
    void 		*io_desc_mp4, 	/*input*/
    mfu_rw_desc_t 	*videofg 	/*output*/){

    uint32_t pos = 0;
    uint32_t k;
    int32_t  j, err = 0;
    uint32_t temp_sam_size = 0;
    uint32_t tot_vdat_size = 0;
    
    /* writting box_name */
    vdatbox_pos[pos] = 'v';
    vdatbox_pos[pos+1] = 'd';
    vdatbox_pos[pos+2] = 'a';
    vdatbox_pos[pos+3] = 't';
    pos += BOX_NAME_SIZE;
    /* write box size */
#ifdef _BIG_ENDIAN_
    write32_byte_swap(vdatbox_pos, pos, *vdat_size);
#else
    write32(vdatbox_pos, pos, *vdat_size);
#endif
    pos += BOX_SIZE_SIZE;
    err = mp4_mfu_read_vid_chunk_data(vid->sync_start_sam_table[vid->num_chunks_processed]-1,
				      vid->sync_sample_table[vid->num_chunks_processed],
				      vdatbox_pos + pos,
				      vid->sample_data_prop,
				      io_desc_mp4, iocb);
    if(err < 0) {
      return err;
    }
    //sample size would have been altered by read_vid_chunk_data
    //becoz we are dumping access unit NALU in the payload

    //recalc each sample size and total video size
    for(k=0; k < vid->sync_sample_table[vid->num_chunks_processed]; k++) {
    	temp_sam_size = 0;
	for(j=0; j < vid->sample_data_prop[k]->s_list_num_entries; j++) {
	    temp_sam_size += vid->sample_data_prop[k]->s_list[j].length;
	}
#ifdef _BIG_ENDIAN_
	videofg->sample_sizes[k] = uint32_byte_swap(temp_sam_size);
#else
	videofg->sample_sizes[k] = temp_sam_size;
#endif
	tot_vdat_size += temp_sam_size;
    }

    //update the new total video size
    *vdat_size = tot_vdat_size;
    pos = BOX_NAME_SIZE;
#ifdef _BIG_ENDIAN_
    write32_byte_swap(vdatbox_pos, pos, *vdat_size);
#else
    write32(vdatbox_pos, pos, *vdat_size);
#endif

   return err; 
}

static int32_t update_sample_size(
    uint8_t 	    *rwfgbox_pos,   /*output buffer*/
    mfu_rw_desc_t    *videofg	    /*input*/){

    uint32_t pos = 0;
    pos += BOX_NAME_SIZE;
    pos += BOX_SIZE_SIZE;
    pos+=4; //videofg_size
    pos+=4; //videofg.unit_count
    pos+=4; //videofg.timescale
    pos+=4; //videofg.default_unit_duration
    pos+=4; //videofg.default_unit_size

    //videofg.sample_duration and videofg.compisition_duration
    pos += 2*4*videofg->unit_count;
    //update sample size 
    memcpy((rwfgbox_pos +pos),
	   videofg->sample_sizes,
	   4*videofg->unit_count);

    return 0;
}

#endif



#if defined(AUDIO_SINGLE_MEMCPY)
static int32_t mfu_make_adat_box(
    uint8_t 		*adatbox_pos, 	/*output buffer*/
    uint32_t 		adat_size, 	/*input*/
    audio_trak_t 	*aud, 		/*input, few mem will be freed*/
    io_handlers_t	*iocb, 		/*input*/ 
    void 		*io_desc_mp4){ /*input*/

    uint32_t pos = 0;
    int32_t err = 0;
    /* writting box_name */
    adatbox_pos[pos] = 'a';
    adatbox_pos[pos+1] = 'd';
    adatbox_pos[pos+2] = 'a';
    adatbox_pos[pos+3] = 't';
    pos += BOX_NAME_SIZE;
    /* write box size */
#ifdef _BIG_ENDIAN_
    write32_byte_swap(adatbox_pos, pos, adat_size);
#else
    write32(adatbox_pos, pos, adat_size);
#endif
    pos += BOX_SIZE_SIZE;

    err = mp4_mfu_read_aud_chunk_data(aud->sync_start_sam_table[aud->num_chunks_processed]-1,
				      aud->sync_sample_table[aud->num_chunks_processed],
				      adatbox_pos + pos,
				      aud->sample_data_prop,
				      io_desc_mp4, 
				      iocb, 
				      aud->header,
				      aud->header_size);
    if(err < 0) {
      return err;
    }

    return err;	
}
#endif

/* This function is used to accumulate the raw data in "chunk_data"
   buffer for "num_samples" samples starting from "start_sample_num"
   sample. the sample vectors in "sample_data_prop" is used to collect
   the raw data.*/
int32_t
mp4_mfu_read_aud_chunk_data(
	uint32_t 	start_sample_num, 	/*input*/
	uint32_t 	num_samples, 		/*input*/
	uint8_t  	*chunk_data,		/*output buffer*/
        sample_prop_t 	**sample_data_prop, 	/*input*/
        void 		*io_desc_mp4, 		/*input*/
        io_handlers_t 	*iocb, 			/*input*/
        uint8_t 	*aud_header, 		/*input*/
        int32_t 	hdr_len)		/*input*/
{
    int32_t err = 0, first_sample_in_chunk = 0;
    int32_t  num_samples_to_shift = 0, chunk_offset = 0, word;
    int32_t sample_offset = 0, sample_offset_temp = 0, sample_size =
	0;
    int32_t chunk_num = 0;
    uint32_t k = 0;
    int32_t  i = 0, flen = 0;
    uint8_t *buf = NULL, *hdr = NULL;
    int32_t pos = 0, bytes_read = 0;
    for(k=0; k<num_samples; k++) {
	hdr = aud_header;
	buf = hdr;
        for(i=0; i<sample_data_prop[k]->s_list_num_entries; i++) {
	    flen=sample_data_prop[k]->s_list[i].length+hdr_len;
	    pos=hdr_len-1-3;
	    buf[pos]&=0xFC;
	    buf[pos++]|=flen>>11;
	    buf[pos++]=(flen>>3)&0xFF;
	    buf[pos]&=0x1F;
	    buf[pos]|=(flen&0x7)<<5;

	    memcpy(chunk_data+sample_offset_temp, hdr, hdr_len);
	    sample_offset_temp += hdr_len;
            iocb->ioh_seek(io_desc_mp4,
			   sample_data_prop[k]->s_list[i].offset,
			   SEEK_SET);
            bytes_read = iocb->ioh_read(chunk_data+sample_offset_temp,
				 1,
				 sample_data_prop[k]->s_list[i].length,
				 io_desc_mp4);
	    if(bytes_read != (int32_t)sample_data_prop[k]->s_list[i].length) {
	      err = -E_VPE_READ_INCOMPLETE;
	      return err;
	    }
            sample_offset_temp+=sample_data_prop[k]->s_list[i].length;
        }
    }


    return err;


}

/* This function is used to accumulate the raw data in "chunk_data"
   buffer for "num_samples" samples starting from "start_sample_num"
   sample. the sample vectors in "sample_data_prop" is used to collect
   the raw data.*/ 
int32_t 
mp4_mfu_read_vid_chunk_data(
	uint32_t 	start_sample_num, 	/*input*/
	uint32_t 	num_samples, 		/*input*/
	uint8_t 	*chunk_data, 		/*output buffer*/
	sample_prop_t 	**sample_data_prop, 	/*input and output updates s_list[i].length*/
	void 		*io_desc_mp4,		/*input*/
	io_handlers_t 	*iocb)			/*input*/
{
    int32_t err = 0, first_sample_in_chunk = 0;
    int32_t  num_samples_to_shift = 0, chunk_offset = 0, word;
    int32_t sample_offset = 0, sample_offset_temp = 0, sample_size = 0;
    int32_t chunk_num = 0;
    uint32_t k = 0;
    int32_t  i = 0, bytes_read = 0;
    uint32_t NAL_size = 0, NAL_type;
    uint32_t total_sample_size = 0, actual_sam_size = 0;
    uint64_t NAL_offset = 0;
    uint32_t count = 0;
    word = NALU_DELIMIT;
    word = htonl(word);
#if defined(VIDEO_READ_WHOLESAMPLE)
    uint32_t nal_cnt;
    NAL_index_t nal_index[MAX_NALU_IN_SAMPLE];
    uint32_t *read32ptr_chunkdata;
    uint32_t AUD_present_flag;
#endif
    
    for(k=0; k<num_samples; k++) {
	for(i=0; i<sample_data_prop[k]->s_list_num_entries; i++) {

	    total_sample_size = sample_data_prop[k]->s_list[i].length;
	    actual_sam_size = sample_data_prop[k]->s_list[i].length;
	    NAL_offset =  sample_data_prop[k]->s_list[i].offset;

#if defined(VIDEO_READ_WHOLESAMPLE)
	    memset(nal_index, 0, MAX_NALU_IN_SAMPLE * sizeof(NAL_index_t));
	    nal_cnt = 0;
	    AUD_present_flag = 0;
	    iocb->ioh_seek(io_desc_mp4,	NAL_offset, SEEK_SET);
	    bytes_read = iocb->ioh_read(chunk_data + chunk_offset, 1, 
				 total_sample_size, io_desc_mp4);
	    if(bytes_read != total_sample_size) {
	      err = -E_VPE_READ_INCOMPLETE;
	      return err;
	    }

	    //replace NAL Size with annex B delimited 00 00 00 01
	    while(total_sample_size){
	    	read32ptr_chunkdata = (uint32_t *)(chunk_data + chunk_offset);
		NAL_size = *read32ptr_chunkdata++;
		NAL_size = uint32_byte_swap(NAL_size);
		nal_index[nal_cnt].nal_size = NAL_size + 4;
		nal_index[nal_cnt].nal_type = ((*read32ptr_chunkdata)&0x1f);
		if(nal_index[nal_cnt].nal_type == AVC_NALU_ACCESS_UNIT)
			AUD_present_flag++;
		nal_index[nal_cnt].chunk_offset = chunk_offset;
		nal_index[nal_cnt].valid_entry = 1;
		nal_cnt++;
		memcpy(chunk_data+chunk_offset, &word,
		       sizeof(int32_t));
		chunk_offset += (4 + NAL_size);		
		total_sample_size = total_sample_size-4-NAL_size;
	    }
	    
	    //remove AUD from the already copied video sample
	    if(AUD_present_flag){
	    	//if 1 AUD is present, then we can have max 2 video parts
	    	//if 2 AUD are present, then we can have max 3 video parts and so on..
	    	part_vectors_t *video_parts = nkn_calloc_type(
			AUD_present_flag+1, sizeof(part_vectors_t), 
			mod_vpe_moof2mfu_desc_t1);
		if(video_parts == NULL){
		    return -E_VPE_PARSER_NO_MEM;	
		}
		uint32_t part_cnt = 0;
		for(i = 0; (i < MAX_NALU_IN_SAMPLE) && (nal_index[i].valid_entry); i++){
		     if(nal_index[i].nal_type == AVC_NALU_ACCESS_UNIT){
 		        part_cnt++;
			actual_sam_size -= nal_index[i].nal_size;
			chunk_offset -= nal_index[i].nal_size;
		     	continue;
		     }
		     if(!video_parts[part_cnt].offset)
		     	video_parts[part_cnt].offset = nal_index[i].chunk_offset;
		     video_parts[part_cnt].length += nal_index[i].nal_size;		     
		}
		for(uint32_t cnt = 0; cnt < AUD_present_flag+1; cnt++){
			if(video_parts[cnt].offset == 0)
				continue;
			if(!video_parts[cnt].offset && cnt > 0){
				//memcpy the data after hole due to AUD purging
				memcpy(chunk_data + video_parts[cnt-1].offset+video_parts[cnt-1].length,
				       chunk_data + video_parts[cnt].offset,
				       video_parts[cnt].length);
			}
		}
		if(video_parts){
			free(video_parts);
		}
	    }
#else
	    while(total_sample_size) {
		count++;
		iocb->ioh_seek(io_desc_mp4,
			       NAL_offset,
			       SEEK_SET);
		bytes_read = iocb->ioh_read(&NAL_size, 1, 4, io_desc_mp4);
		if(bytes_read != 4) {
		  err = -E_VPE_READ_INCOMPLETE;
		  return err;
		}
		NAL_size = uint32_byte_swap(NAL_size);
		NAL_offset+=4;
		memcpy(chunk_data+chunk_offset, &word,
		       sizeof(int32_t));
		chunk_offset += sizeof(int32_t);;
		iocb->ioh_seek(io_desc_mp4,
                               NAL_offset,
                               SEEK_SET);
		bytes_read = iocb->ioh_read(chunk_data+chunk_offset, 1, NAL_size,
				     io_desc_mp4);
		if(bytes_read != (int32_t)NAL_size) {
		  err = -E_VPE_READ_INCOMPLETE;
		  return err;
		}

		chunk_offset += NAL_size;
		if(NAL_size >= 6) {
		  NAL_type = chunk_data[chunk_offset-2]&0x1F;
		  if(NAL_type == AVC_NALU_ACCESS_UNIT) {
		    chunk_offset -=  (sizeof(int32_t) +2);
		    actual_sam_size -= (sizeof(int32_t)+2);
		  }
		} 
		NAL_offset += NAL_size;
		total_sample_size = total_sample_size-4-NAL_size;
              }
#endif
	    sample_data_prop[k]->s_list[i].length = actual_sam_size;
	}
    }
    
    
    return err;
}


int32_t
mp4_mfu_update_vid_sync_tables(uint8_t* stss_data, 
			       uint64_t* sync_time_table,
                               uint32_t* sync_sample_table, 
			       uint64_t* sync_sam_size_table,
			       uint32_t *sync_start_sam_table,
                               trak_t *trak, uint8_t *stsz_data,
			       uint32_t *num_sync_points, uint32_t
			       timescale)
{
    int32_t err = 0;
    uint32_t i = 0, j = 0;
    uint64_t temp_sam_size = 0;
    uint32_t num_sync_point = 0;
    uint8_t *stss_initial_data = stss_data;
    uint32_t start_sample = 0, num_samples = 0, sample_count = 0;;
    uint64_t sync_time_temp = 0;
    //update sync_start_sam_table
    if(stss_data != NULL) {
	stss_data +=12;//FULL_BOX
	*num_sync_points = _read32_byte_swap(stss_data, 0);
	stss_data +=4;
	for(i=0; i<*num_sync_points; i++) {
	    sync_start_sam_table[i] = _read32_byte_swap(stss_data, i*4);
	}
    } else {
	err = E_VPE_MP4_NO_STSS_TAB;
	return err;
    }
    stss_data = stss_initial_data;
    //get the total num of samples
    stsz_data +=16;//FULL_BOX
    sample_count = _read32_byte_swap(stsz_data, 0);
    stsz_data +=4;
    uint64_t curr_syncsample_dts = 0, next_syncsample_dts;
    curr_syncsample_dts = mp4_sample_to_time(trak, sync_start_sam_table[0]);
    
    //update sync_sample_table, sync_time_table, sync_sam_size_table
    for(i=0; i<*num_sync_points; i++) {

	if(i == (*num_sync_points -1)) {
            sync_sample_table[i] = sample_count - (sync_start_sam_table[i]-1);
            sync_time_table[i] = sync_time_table[i-1];
        } else {
	    sync_sample_table[i] = sync_start_sam_table[i+1] -
		sync_start_sam_table[i];
	    next_syncsample_dts = 
	    	mp4_sample_to_time(trak, sync_start_sam_table[i+1]);
	    sync_time_temp =  next_syncsample_dts - curr_syncsample_dts;
	    sync_time_table[i] = (sync_time_temp * 90000) /timescale;
	    curr_syncsample_dts = next_syncsample_dts;
	}
	start_sample = sync_start_sam_table[i] - 1;
        num_samples = start_sample+sync_sample_table[i];
	
        sync_sam_size_table[i] = mp4_find_cumulative_sample_size(trak,
                                                                 start_sample,
                                                                 num_samples);
    }

    return err;
}

int32_t
mp4_mfu_update_aud_sync_tables(uint64_t* video_chunk_duration,
			       uint32_t vid_sync_pt,
			       uint64_t* sync_time_table,
			       uint32_t* sync_sample_table,
			       uint64_t*  sync_sam_size_table,
			       uint32_t* sync_start_sam_table,
			       trak_t *trak, uint8_t *stsz_data, uint32_t
			       *num_sync_points, uint32_t timescale)
{
    int32_t err = 0;
    uint32_t i = 0, j= 0;
    uint64_t samp_duration = 0,	prev_chunk_start_dts = 0;
    uint64_t audio_dts = 0;
    uint64_t temp_sam_size = 0;
    uint32_t num_samples = 0, prev_sample_num = 0;
    uint8_t* stsz_initial_data = stsz_data;
    uint32_t sample_count = 0;
    uint32_t start_sample = 0;
    uint32_t temp_i = 0, tot_temp_i = 0;
    uint64_t dts_video_syncsample = 0;

    stsz_data +=16;//FULL_BOX
    sample_count = _read32_byte_swap(stsz_data, 0);
    stsz_data +=4;
    //update sync_time_table, sync_start_sam_table
    /* Based on the keyframe interval of the video trak, corresponding
    number of audio samples for the correspoding video duration is
    collected and formed as array .This array will have no:of:aud
    samples that form each chunk(moof) */ 

    dts_video_syncsample += video_chunk_duration[j];
    
    for(i = 0; (i < sample_count) && (j < vid_sync_pt); i++) {
        samp_duration = mp4_sample_to_duration(trak, i);
	samp_duration = (samp_duration * 90000)/timescale;
	audio_dts += samp_duration;
	num_samples++;
	    
	if(audio_dts >= dts_video_syncsample) {
		if(audio_dts > dts_video_syncsample) {
		    i--;
		    sync_sample_table[j] = num_samples - 1;
		    audio_dts -= samp_duration;
		    sync_time_table[j] = audio_dts - prev_chunk_start_dts;
		    prev_chunk_start_dts = audio_dts;
		} else {
		    sync_sample_table[j] = num_samples;
		    sync_time_table[j] = audio_dts - prev_chunk_start_dts;
		    prev_chunk_start_dts = audio_dts;
		}
		j++;
		dts_video_syncsample += video_chunk_duration[j];
		num_samples = 0;
	}
    }
    if(num_samples){
       //put the remaining
       sync_time_table[j] = audio_dts - prev_chunk_start_dts;
       sync_sample_table[j] = num_samples;
       j++;
    }
    stsz_data = stsz_initial_data;
    
    *num_sync_points = j;
    //update sync_sample_table

    sync_start_sam_table[0] = 1;
    for(i=1; i<*num_sync_points; i++) {
        sync_start_sam_table[i] = sync_start_sam_table[i-1] +
	    sync_sample_table[i-1];
    }
    //update sync_sam_size_table
    for(i=0; i<*num_sync_points; i++) {
	start_sample = sync_start_sam_table[i] - 1;
        num_samples = start_sample+sync_sample_table[i];
        sync_sam_size_table[i] = mp4_find_cumulative_sample_size(trak,
								 start_sample, 
								 num_samples);


    }

    return err;

}

/* This function is used form MFU based on the details collected in
   the mp42mfu builder*/
int32_t
mp4_mfu_conversion(audio_trak_t *aud, video_trak_t *vid, mfu_data_t
		   *mfu_data, mp4_mfu_builder_t *bldr, void *io_desc,
		   io_handlers_t *iocb)
{
    int32_t err = 0;
    mfub_t *mfubox_header;
    moof2mfu_desc_t * moof2mfu_desc = NULL;
    uint32_t mfu_data_size;
    uint32_t vdat_pos=0,adat_pos=0;
    uint32_t vdat_box_size = 0;
    uint8_t *mfubox_pos =NULL;
    uint8_t *rwfgbox_pos = NULL;
    uint8_t *vdatbox_pos = NULL;
    uint8_t *adatbox_pos = NULL;
    double temp = 0;
    struct timeval now1, now2;
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
#if defined(PERF_TIMERS)       
    gettimeofday(&now1, NULL);
#endif

    if(bldr->is_audio) {
      /* This if condition is to check whether there are any audio 
       * samples constituing the mfu. This condition is hit when there 
       * are audio samples in the mp4 , but absent in specific chunk*/
      if(aud->sync_sample_table[aud->num_chunks_processed] > 0) {
	aud->is_aud_present_chunk = 1;
	err = mp4_mfu_audio_process(moof2mfu_desc, aud, iocb,
				    io_desc);
	
	if(err) {
	  //free(moof2mfu_desc->mfu_raw_box);
	  goto error;
	  //return err;
	}
	moof2mfu_desc->is_audio = 1;
	moof2mfu_desc->mfu_raw_box_size += 4 +
	  moof2mfu_desc->mfu_raw_box->audiofg_size;
      } else {
	aud->is_aud_present_chunk = 0;
      }
    }
#if defined(PERF_TIMERS)       
     gettimeofday(&now2, NULL);
     printf("audproc %d\n",diffTimevalToMs(&now2, &now1));
     

    /* parse the video moof and get the details reqired for
       mfu(videofg details and vdat)*/
    gettimeofday(&now1, NULL);
#endif    
    if(bldr->is_video) {
      if(vid->sync_sample_table[vid->num_chunks_processed] > 0) {
      vid->is_vid_present_chunk = 1;
      err = mp4_mfu_video_process(moof2mfu_desc, vid, iocb,
				  io_desc);
      if(err) {
	//free(moof2mfu_desc->mfu_raw_box);
	//free(moof2mfu_desc);
	//return err;
	goto error;
      }
      moof2mfu_desc->is_video = 1;     
      moof2mfu_desc->mfu_raw_box_size += 4 +
	moof2mfu_desc->mfu_raw_box->videofg_size;
      }else {
        vid->is_vid_present_chunk = 0;
      }
    }
    moof2mfu_desc->mfub_box_size = sizeof(mfub_t);
#if defined(PERF_TIMERS)       
    gettimeofday(&now2, NULL);
    printf("vidproc %d\n",diffTimevalToMs(&now2, &now1));
#endif
    


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
    mfu_data->mfu_discont = nkn_calloc_type(3, sizeof(mfu_discont_ind_t*), 
					    mod_vpe_discont_t);
    if(!mfu_data->mfu_discont) {
      return -E_VPE_PARSER_NO_MEM;
    }

    /*Start the mfu file write from here*/
    /*write RAW Header box */
    rwfgbox_pos =mfu_data->data + moof2mfu_desc->mfub_box_size +
        (1*(BOX_NAME_SIZE + BOX_SIZE_SIZE));
    mfu_writer_rwfg_box(moof2mfu_desc->mfu_raw_box_size,
                        moof2mfu_desc->mfu_raw_box, rwfgbox_pos,
                        moof2mfu_desc);
#if defined(PERF_TIMERS)       
    gettimeofday(&now1, NULL);
#endif
    /*write VDAT box */
    if (moof2mfu_desc->is_video) {
        vdat_pos =  moof2mfu_desc->mfub_box_size +
            moof2mfu_desc->mfu_raw_box_size + (2 *(BOX_NAME_SIZE +
                                                   BOX_SIZE_SIZE));
        vdatbox_pos = mfu_data->data + vdat_pos;
#if defined(VIDEO_SINGLE_MEMCPY)
	err = mfu_make_vdat_box(vdatbox_pos,
				&moof2mfu_desc->vdat_size,
				vid,
				iocb,
				io_desc,
				&moof2mfu_desc->mfu_raw_box->videofg);
	if(err < 0 ){
	  goto error;
	  //return err;
	}

	update_sample_size(rwfgbox_pos,
		&moof2mfu_desc->mfu_raw_box->videofg);
#else
        mfu_writer_vdat_box(moof2mfu_desc->vdat_size,
                            moof2mfu_desc->vdat,
			    vdatbox_pos);
#endif
        vdat_box_size = BOX_NAME_SIZE + BOX_SIZE_SIZE;
	mfu_data->mfu_discont[0] = nkn_calloc_type(1, sizeof(mfu_discont_ind_t),
						   mod_vpe_discont_t);
	if(!mfu_data->mfu_discont[0]) {
	  return -E_VPE_PARSER_NO_MEM;
	}
	uint32_t vid_sam_num = vid->sync_start_sam_table[vid->num_chunks_processed] - 1;
	mfu_data->mfu_discont[0]->timestamp = (mp4_sample_to_time(vid->trak, vid_sam_num)*90000)/vid->timescale;
	if((vid->is_vid_prev_chunk == 0)&&(vid->num_chunks_processed > 0)) {
	  mfu_data->mfu_discont[0]->discontinuity_flag = 1;
	}
	if(mfu_data->mfu_discont[0]->discontinuity_flag == 1) {
	  mfu_data->mfu_discont[0]->discontinuity_flag = mfu_data->mfu_discont[0]->discontinuity_flag;
	  mfu_data->mfu_discont[0]->discontinuity_flag = 1;
	}
    }
#if defined(PERF_TIMERS)       
    gettimeofday(&now2, NULL);
    printf("mfuvdat %d\t",diffTimevalToMs(&now2, &now1));
    gettimeofday(&now1, NULL);
#endif
    /*write ADAT box*/
    if (moof2mfu_desc->is_audio) {
        adat_pos = moof2mfu_desc->mfub_box_size +
            moof2mfu_desc->mfu_raw_box_size +
            moof2mfu_desc->vdat_size + vdat_box_size +
            (2*(BOX_NAME_SIZE + BOX_SIZE_SIZE));

        adatbox_pos =  mfu_data->data + adat_pos;
#if defined(AUDIO_SINGLE_MEMCPY)	
	err = mfu_make_adat_box(adatbox_pos,
				moof2mfu_desc->adat_size,
				aud,
				iocb,
				io_desc);
	if(err < 0) {
	  goto error;
	  //return err;
	}
#else
        mfu_writer_adat_box(moof2mfu_desc->adat_size,
                            moof2mfu_desc->adat,
                            adatbox_pos);	
#endif
	mfu_data->mfu_discont[1] = nkn_calloc_type(1, sizeof(mfu_discont_ind_t),
                                                   mod_vpe_discont_t);
        if(!mfu_data->mfu_discont[1]) {
          return -E_VPE_PARSER_NO_MEM;
        }
	uint32_t aud_sam_num = aud->sync_start_sam_table[aud->num_chunks_processed] - 1;
        mfu_data->mfu_discont[1]->timestamp = (mp4_sample_to_time(aud->trak, aud_sam_num)*90000)/aud->timescale;
        if((aud->is_aud_prev_chunk == 0)&&(aud->num_chunks_processed > 0)) {
          mfu_data->mfu_discont[1]->discontinuity_flag = 1;
        }
	if(mfu_data->mfu_discont[1]->discontinuity_flag == 1) {
          mfu_data->mfu_discont[1]->discontinuity_flag = mfu_data->mfu_discont[1]->discontinuity_flag;
          mfu_data->mfu_discont[1]->discontinuity_flag = 1;
        }
    }
#if defined(PERF_TIMERS)       
    gettimeofday(&now2, NULL);
    printf("mfuadat %d\n",diffTimevalToMs(&now2, &now1));    
#endif    
    /*write MFU Header box*/
    mfubox_pos = mfu_data->data;
    mfubox_header->version = 0;
    mfubox_header->program_number = bldr->num_pmf;
    mfubox_header->stream_id = bldr->profile_num;
    mfubox_header->flags = 0;
    mfubox_header->sequence_num = 0;
    mfubox_header->timescale = vid->timescale;
    //mfubox_header->timescale = bldr->timescale;
    temp = (double)vid->sync_time_table[vid->num_chunks_processed]
	/ 90000;
    if(round(temp))
      mfubox_header->duration =round(temp);//convert to seconds
    else
      mfubox_header->duration = 1;
    mfubox_header->audio_duration = 0;

    mfubox_header->video_rate = vid->bit_rate;
    mfubox_header->audio_rate = aud->bit_rate;
    mfubox_header->mfu_rate = 0;
    mfubox_header->offset_vdat = vdat_pos;
    mfubox_header->offset_adat = adat_pos;
    mfubox_header->mfu_size = mfu_data_size;
    mfu_writer_mfub_box(moof2mfu_desc->mfub_box_size,
			mfubox_header,
			mfubox_pos);
    mfu_data->data_size = mfu_data_size;

    aud->is_aud_prev_chunk = aud->is_aud_present_chunk;
    vid->is_vid_prev_chunk = vid->is_vid_present_chunk;
//#define _CHECK_MFU_ 1
#ifdef _CHECK_MFU_

    char file_name[500] = {'\0'};;
    FILE *fout_mfu_check = NULL;
    int32_t retrun_val = 0, len = 0;
    len = snprintf(NULL,0,"/var/home/root/mfu_data_%d.txt", file_number);
    snprintf(file_name, len+1, "/var/home/root/mfu_data_%d.txt", file_number);
    fout_mfu_check = fopen("file_name","wb");
    retrun_val =
	fwrite(mfu_data->data,1,mfu_data_size,fout_mfu_check);
    perror("fwrite error:  ");
    fflush(fout_mfu_check);
    fclose(fout_mfu_check);
    file_number++;
#endif
 error:
    if(moof2mfu_desc->is_video) {
      mp4_mfu_clean_sample_data_prop(vid->sample_data_prop,
				     vid->sync_sample_table[vid->num_chunks_processed]);
    }
    if(moof2mfu_desc->is_audio) {
      mp4_mfu_clean_sample_data_prop(aud->sample_data_prop,
				     aud->sync_sample_table[aud->num_chunks_processed]);
    }
    free_moof2mfu_desc(moof2mfu_desc);
    if (mfubox_header != NULL)
	free(mfubox_header);
    return err;
    


}
int32_t
mp4_mfu_video_process(moof2mfu_desc_t *moof2mfu_desc,
		      video_trak_t *vid, io_handlers_t* iocb, void
                      *io_desc_mp4)
{
    mfu_rw_desc_t *videofg = &moof2mfu_desc->mfu_raw_box->videofg;
    uint32_t *temp = NULL;
    uint32_t temp1 = 0;
    int32_t err = 0, j=0;
    uint32_t i = 0, k=0;
    bitstream_state_t *bs = NULL;
    int32_t size = 0;
    uint8_t *data = NULL;
    uint32_t temp_sam_size = 0, sample_size = 0, temp_sam_duration = 0;
    uint32_t sample_number = 0, temp_sample_duration = 0;
    uint32_t temp_delta = 0, start_sample = 0;
    uint32_t tot_vdat_size = 0;
    struct timeval now1, now2;

    videofg->unit_count =
        vid->sync_sample_table[vid->num_chunks_processed];
    videofg->timescale = vid->timescale;
    videofg->default_unit_duration = vid->default_sample_duration;
    videofg->default_unit_size = vid->default_sample_size;
    temp = (uint32_t*)
        nkn_calloc_type(1, videofg->unit_count*4*3,
                        mod_vpe_vid_sample_dur_buf);
    if(!temp) {
        err = -E_VPE_PARSER_NO_MEM;
        return err;
    }
    videofg->sample_duration = temp;
    videofg->composition_duration = temp + videofg->unit_count;
    videofg->sample_sizes = temp + videofg->unit_count*2;
    
#if defined(PERF_TIMERS)       
	gettimeofday(&now1, NULL);
#endif
#if !defined(VIDEO_OPT_DURATION_CALC)

    vid->sample_time_table = (uint32_t*)
        nkn_calloc_type(videofg->unit_count, sizeof(uint32_t),
                        mod_vpe_vid_sync_table);
    if (!vid->sample_time_table) {
        return -E_VPE_PARSER_NO_MEM;
    }

    mp4_get_sample_duration(vid->sync_start_sam_table[vid->num_chunks_processed]-1,
			    vid->sync_sample_table[vid->num_chunks_processed],
			    vid->sample_time_table, vid->trak);

    vid->total_sample_duration = 0;
    if(vid->sync_sample_table[vid->num_chunks_processed] !=
       videofg->unit_count) {
	videofg->unit_count = videofg->unit_count;
	vid->sync_sample_table[vid->num_chunks_processed] =
	    vid->sync_sample_table[vid->num_chunks_processed];
    }
    start_sample = vid->sync_start_sam_table[vid->num_chunks_processed];

#ifdef _BIG_ENDIAN_
    for(i=0; i<videofg->unit_count; i++) {
	if(i==0) {
	  if(videofg->unit_count != 1) {
            temp_sample_duration = vid->sample_time_table[i+1]-
	      vid->sample_time_table[i];
	  } else {
	    uint32_t  temp_sam_num = vid->sync_start_sam_table[vid->num_chunks_processed];
	    uint32_t current_sample_time = mp4_sample_to_time(vid->trak, temp_sam_num);
	    uint32_t next_sample_time = mp4_sample_to_time(vid->trak, temp_sam_num+1);
	    temp_sample_duration = next_sample_time - current_sample_time;
	  }
	} else {
	    temp_sample_duration = vid->sample_time_table[i] -
		vid->sample_time_table[i-1];
	}
        videofg->sample_duration[i] = uint32_byte_swap(temp_sample_duration);
	vid->total_sample_duration += vid->sample_time_table[i];
	if(vid->trak->stbl->ctts_data != NULL)
	  temp_delta = mp4_get_ctts_offset(vid->trak, start_sample+i);
	else
	  temp_delta = 0;
	videofg->composition_duration[i] = uint32_byte_swap(temp_delta);

    }
#else
    for(i=0; i<videofg->unit_count; i++) {
	if(i==0) {
	  if(videofg->unit_count != 1) {
            videofg->sample_duration[i] = vid->sample_time_table[i+1]-
	      vid->sample_time_table[i];
	  } else {
	    uint32_t  temp_sam_num = vid->sync_start_sam_table[vid->num_chunks_processed];
	    uint32_t current_sample_time = mp4_sample_to_time(vid->trak, temp_sam_num);
	    uint32_t next_sample_time = mp4_sample_to_time(vid->trak, temp_sam_num+1);
	    videofg->sample_duration[i] = next_sample_time - current_sample_time;
	  }
	} else {
	  videofg->sample_duration[i] = vid->sample_time_table[i]-
	    vid->sample_time_table[i-1];
	}
	vid->total_sample_duration += vid->sample_time_table[i];
	if(vid->trak->stbl->ctts_data != NULL)
	  temp_delta = mp4_get_ctts_offset(vid->trak, start_sample+i);
	else
	  temp_delta = 0;
        videofg->composition_duration[i] = (temp_delta);
    }
#endif
    if(vid->sample_time_table) {
        free(vid->sample_time_table);
        vid->sample_time_table = NULL;
    }
#else
    start_sample = vid->sync_start_sam_table[vid->num_chunks_processed];
#ifdef _BIG_ENDIAN_
    for(i=0; i<videofg->unit_count; i++) {
    	videofg->sample_duration[i] = 
		uint32_byte_swap(mp4_sample_to_duration(vid->trak, 
				start_sample+i));
	if(vid->trak->stbl->ctts_data != NULL)
	  temp_delta = mp4_get_ctts_offset(vid->trak, start_sample+i);
	else
	  temp_delta = 0;
	videofg->composition_duration[i] = uint32_byte_swap(temp_delta);
    }
#else
    for(i=0; i<videofg->unit_count; i++) {
	    videofg->sample_duration[i] = 
		    mp4_sample_to_duration(vid->trak, 
		    start_sample+i);
	    if(vid->trak->stbl->ctts_data != NULL)
	      temp_delta = mp4_get_ctts_offset(vid->trak, start_sample+i);
	    else
	      temp_delta = 0;
	    videofg->composition_duration[i] = (temp_delta);
    }
#endif
#endif
#if defined(PERF_TIMERS)   
     gettimeofday(&now2, NULL);
     printf("vid-durn %d\t",diffTimevalToMs(&now2, &now1));    
    gettimeofday(&now1, NULL);
#endif
    mfu_write_sps_pps(vid->avc1, moof2mfu_desc);

    moof2mfu_desc->mfu_raw_box->videofg_size = RAW_FG_FIXED_SIZE +
        3 *(4*videofg->unit_count) + CODEC_INFO_SIZE_SIZE +
        moof2mfu_desc->mfu_raw_box->videofg.codec_info_size;
    vid->sample_data_prop =  nkn_calloc_type(videofg->unit_count, sizeof(sample_prop_t*),
					     mod_vpe_vid_sample_prop_table);
    if (!vid->sample_data_prop) {
        return -E_VPE_PARSER_NO_MEM;
    }
    //get the video chunk_data sample postion

    err = mp4_get_segment(vid->sync_start_sam_table[vid->num_chunks_processed]-1,
			  vid->sync_sample_table[vid->num_chunks_processed],
			  vid->sample_data_prop, vid->trak, vid->stsz_data);
    if(err)
	return err;

#if !defined(VIDEO_SINGLE_MEMCPY)
    //get the video chunk_data
    vid->chunk_data[vid->num_chunks_processed] = (uint8_t*)
        nkn_malloc_type(vid->sync_sam_size_table[vid->num_chunks_processed],
			mod_vpe_vid_chunk_data);
    if (!vid->chunk_data[vid->num_chunks_processed]) {
        return -E_VPE_PARSER_NO_MEM;
    }

    err = mp4_mfu_read_vid_chunk_data(vid->sync_start_sam_table[vid->num_chunks_processed]-1,
				      vid->sync_sample_table[vid->num_chunks_processed],
				      vid->chunk_data[vid->num_chunks_processed],
				      vid->sample_data_prop,
				      io_desc_mp4, iocb);

    moof2mfu_desc->vdat = vid->chunk_data[vid->num_chunks_processed];
    if(err < 0) {
      return err;
    }
#endif    
    for(k=0; k<vid->sync_sample_table[vid->num_chunks_processed]; k++) {
      temp_sam_size = 0;
      for(j=0; j<vid->sample_data_prop[k]->s_list_num_entries; j++) {
	temp_sam_size += vid->sample_data_prop[k]->s_list[j].length;
      }
#ifdef _BIG_ENDIAN_
      videofg->sample_sizes[k] = uint32_byte_swap(temp_sam_size);
#else
      videofg->sample_sizes[k] = temp_sam_size;
#endif
      tot_vdat_size += temp_sam_size;
    }
#if defined(PERF_TIMERS)       
     gettimeofday(&now2, NULL);
     printf("vid-sampprop %d\t",diffTimevalToMs(&now2, &now1));    
#endif

    moof2mfu_desc->vdat_size = tot_vdat_size;
    return err;
}

int32_t 
mp4_mfu_audio_process(moof2mfu_desc_t *moof2mfu_desc,
		      audio_trak_t *aud, io_handlers_t* iocb, void
		      *io_desc_mp4 )
{
    mfu_rw_desc_t *audiofg = &moof2mfu_desc->mfu_raw_box->audiofg;
    uint32_t *temp = NULL;
    uint32_t temp1 = 0;
    int32_t  err = 0;
    uint32_t i = 0;
    bitstream_state_t *bs = NULL;
    int32_t size = 0;
    uint8_t *data = NULL;
    uint32_t temp_sam_size = 0, sample_size = 0, temp_sam_duration = 0;;
    uint32_t sample_number = 0, temp_sample_duration = 0;
    uint32_t tot_mem_req = 0;
    struct timeval now1, now2;
    
    audiofg->unit_count =
	aud->sync_sample_table[aud->num_chunks_processed];
    audiofg->timescale = aud->timescale;
    audiofg->default_unit_duration = aud->default_sample_size;
    audiofg->default_unit_size = aud->default_sample_duration;
    temp = (uint32_t*)
	nkn_calloc_type(1, audiofg->unit_count*4*3,
			mod_vpe_aud_sample_dur_buf);
    if(!temp) {
        err = -E_VPE_PARSER_NO_MEM;
        return err;
    }
    audiofg->sample_duration = temp;
    audiofg->composition_duration = temp + audiofg->unit_count;
    audiofg->sample_sizes = temp + audiofg->unit_count*2;

    for(i=0; i<audiofg->unit_count; i++) {
	data = aud->stsz_data;
        data+=12;
        temp_sam_size=_read32_byte_swap(data,0);
        data+=8;
        if(temp_sam_size==0) {
            //sample_size=_read32_byte_swap(data,i*4);
	    sample_number = (aud->sync_start_sam_table[aud->num_chunks_processed]-1);
	    sample_size = _read32_byte_swap(data, (sample_number+i)*4);

        } else {
            sample_size = temp_sam_size;
        }
#ifdef _BIG_ENDIAN_
        audiofg->sample_sizes[i] = uint32_byte_swap(sample_size
				+ FR_LEN_NO_PROTECTION);
#else
	audiofg->sample_sizes[i] = sample_size + FR_LEN_NO_PROTECTION;
#endif
    }
#if defined(PERF_TIMERS)       
	gettimeofday(&now1, NULL); 
#endif
#if !defined(AUDIO_OPT_DURATION_CALC)   

    aud->sample_time_table = (uint32_t*)
	nkn_calloc_type(audiofg->unit_count, sizeof(uint32_t),
			mod_vpe_aud_sync_table);
    if (!aud->sample_time_table) {
        return -E_VPE_PARSER_NO_MEM;
    }
 
    mp4_get_sample_duration(aud->sync_start_sam_table[aud->num_chunks_processed]-1,
                            aud->sync_sample_table[aud->num_chunks_processed],
                            aud->sample_time_table, aud->trak);

    aud->total_sample_duration = 0;

    if(aud->sync_sample_table[aud->num_chunks_processed] !=
       audiofg->unit_count) {
        audiofg->unit_count = audiofg->unit_count;
        aud->sync_sample_table[aud->num_chunks_processed] =
            aud->sync_sample_table[aud->num_chunks_processed];
    }

#ifdef _BIG_ENDIAN_
    for(i=0; i<audiofg->unit_count; i++) {
      if(i==0) {
	if(audiofg->unit_count != 1) {
	    temp_sample_duration = aud->sample_time_table[i+1]-
		aud->sample_time_table[i];
	} else {
	  uint32_t  temp_sam_num = aud->sync_start_sam_table[aud->num_chunks_processed];
	  uint32_t current_sample_time = mp4_sample_to_time(aud->trak, temp_sam_num);
	  uint32_t next_sample_time = mp4_sample_to_time(aud->trak, temp_sam_num+1);
	  temp_sample_duration = next_sample_time - current_sample_time;
	}
      } else {
	    temp_sample_duration = aud->sample_time_table[i] -
		aud->sample_time_table[i-1];
	}
	audiofg->sample_duration[i] = uint32_byte_swap(temp_sample_duration);
	aud->total_sample_duration += aud->sample_time_table[i];

    }
#else
    for(i=0; i<audiofg->unit_count; i++) {
      if(i==0) {
	if(audiofg->unit_count != 1) {
	  audiofg->sample_duration[i] = aud->sample_time_table[i+1]-
	      aud->sample_time_table[i];
        } else {
          uint32_t  temp_sam_num = aud->sync_start_sam_table[aud->num_chunks_processed];
          uint32_t current_sample_time = mp4_sample_to_time(aud->trak, temp_sam_num);
          uint32_t next_sample_time = mp4_sample_to_time(aud->trak, temp_sam_num+1);
          audiofg->sample_duration[i] = next_sample_time - current_sample_time;
        }
      } else {
	audiofg->sample_duration[i] = aud->sample_time_table[i] -
	  aud->sample_time_table[i-1];
	}
      aud->total_sample_duration += aud->sample_time_table[i];
    }
#endif
    if(aud->sample_time_table) {
      free(aud->sample_time_table);
      aud->sample_time_table = NULL;
    }
#else
    uint32_t start_sample;
    start_sample = aud->sync_start_sam_table[aud->num_chunks_processed];
#ifdef _BIG_ENDIAN_
    for(i=0; i<audiofg->unit_count; i++) {
    	audiofg->sample_duration[i] = 
		uint32_byte_swap(mp4_sample_to_duration(aud->trak, 
				start_sample+i));
    }
#else
    for(i=0; i<audiofg->unit_count; i++) {
	    audiofg->sample_duration[i] = 
		    mp4_sample_to_duration(aud->trak, 
		    start_sample+i);
    }
#endif

#endif
#if defined(PERF_TIMERS)   
     gettimeofday(&now2, NULL);
     printf("aud-durn %d\t",diffTimevalToMs(&now2, &now1));    
    gettimeofday(&now1, NULL);
#endif
    aud->sample_data_prop =  nkn_calloc_type(audiofg->unit_count,
					     sizeof(sample_prop_t*),
                                             mod_vpe_aud_sample_prop_table);
    if (!aud->sample_data_prop) {
        return -E_VPE_PARSER_NO_MEM;
    }
    audiofg->codec_info_size = 0;
    audiofg->codec_specific_data = NULL;

    moof2mfu_desc->mfu_raw_box->audiofg_size = RAW_FG_FIXED_SIZE +
	3 *(4*audiofg->unit_count) + CODEC_INFO_SIZE_SIZE +
	moof2mfu_desc->mfu_raw_box->audiofg.codec_info_size;
    //get the audio chunk_data sample postion

    err = mp4_get_segment(aud->sync_start_sam_table[aud->num_chunks_processed]-1,
			  aud->sync_sample_table[aud->num_chunks_processed],
			  aud->sample_data_prop, aud->trak, aud->stsz_data);
    if(err)
        return err;

    //get the audio chunk_data
    tot_mem_req = aud->sync_sam_size_table[aud->num_chunks_processed]
	+ (aud->sync_sample_table[aud->num_chunks_processed] *
	   FR_LEN_NO_PROTECTION);//FR_LEN_NO_PROTECTION for adts header
#if defined(PERF_TIMERS)   	   
    gettimeofday(&now2, NULL);
    printf("aud-sampprop %d\t",diffTimevalToMs(&now2, &now1));
#endif
#if !defined(AUDIO_SINGLE_MEMCPY)
    aud->chunk_data[aud->num_chunks_processed] = (uint8_t*)
        nkn_malloc_type(tot_mem_req, mod_vpe_aud_chunk_data);
    if (!aud->chunk_data[aud->num_chunks_processed]) {
        return -E_VPE_PARSER_NO_MEM;
    }

    err = mp4_mfu_read_aud_chunk_data(aud->sync_start_sam_table[aud->num_chunks_processed]-1,
				      aud->sync_sample_table[aud->num_chunks_processed],
				      aud->chunk_data[aud->num_chunks_processed],
				      aud->sample_data_prop,
				      io_desc_mp4, iocb, aud->header,
				      aud->header_size);

    if(err < 0) {
      return err;
    }
    moof2mfu_desc->adat = aud->chunk_data[aud->num_chunks_processed];
#endif    
    moof2mfu_desc->adat_size = tot_mem_req;
    return err;


}
int32_t 
mp4_get_sample_duration(uint32_t start_sample, uint32_t num_sample,
			uint32_t *sample_time_table, trak_t *trak)
{
    int32_t err = 0;
    uint32_t i = 0;
    for(i=0; i<num_sample; i++) {
	//sample_time_table[start_sample+i] = mp4_sample_to_time(trak,
	//				       start_sample+i+1);
	sample_time_table[i] = mp4_sample_to_time(trak,
					  start_sample+i+1);

    }


    return err;
}
void
mp4_mfu_clean_sample_data_prop(sample_prop_t **sample_data_prop,
			       uint32_t num_sample)
{
    int32_t i = 0;
    if(sample_data_prop != NULL) {
      for(i=0; i< (int32_t)num_sample; i++) {
	if(sample_data_prop[i]) {
	  if(sample_data_prop[i]->s_list)
	    free(sample_data_prop[i]->s_list);
	  free(sample_data_prop[i]);
	}
      }
      free(sample_data_prop);
      sample_data_prop = NULL;
    }
    

}

int32_t
mp4_get_segment(uint32_t start_sample, uint32_t num_sample,
		sample_prop_t **sample_data_prop, trak_t *trak, uint8_t*
		stsz_data)
{
    int32_t err = 0;
    int32_t i = 0;
    uint8_t *data;
    uint32_t temp_sam_size = 0;
    uint32_t samp_cnt = 0, sample_size = 0;
    for(i=0; i< (int32_t)num_sample; i++) {
	sample_data_prop[i] = (sample_prop_t*) nkn_calloc_type(1,
							       sizeof(sample_prop_t),
							       mod_vpe_sample_prop_t);
	//if(!sample_data_prop[i])
	//  return -E_VPE_PARSER_NO_MEM;
	if(i==0)
	    sample_data_prop[i]->prev_sample = NULL;
	else
	    sample_data_prop[i]->prev_sample = sample_data_prop[i-1];


	data = stsz_data;
	data+=12;
	temp_sam_size=_read32_byte_swap(data,0);
	data+=4;
	samp_cnt=_read32_byte_swap(data,0);
	data+=4;
	if(temp_sam_size==0) {
	    sample_size=_read32_byte_swap(data,(start_sample+i)*4);
	} else {
	    sample_size = temp_sam_size;
	}

	/* get sample vector for each sample*/
	err = mp4_get_sample_pos (start_sample+i, sample_data_prop[i],
				  sample_data_prop[i]->prev_sample,
				  trak,
				  sample_size);

	if(err)
	    return err;


    }

    return err;
}

void
cleanup_mp4_mfu_builder(mp4_mfu_builder_t* bldr)
{
    int32_t i = 0, j = 0;
    if(bldr != NULL) {
	for(j=0; j<bldr->n_profiles; j++) {
	    if(bldr->io_desc_mp4[j])
		bldr->iocb[j]->ioh_close(bldr->io_desc_mp4[j]);
	    if(bldr->mp4_name[j])
		free(bldr->mp4_name[j]);
	    if(bldr->mp4_file_path[j])
		free(bldr->mp4_file_path[j]);
	    
	    for(i=0; i<bldr->n_vid_traks[j]; i++) {
		video_trak_t *present_stream = &bldr->video[j][i];
		if((stream_AVC_conf_Record_t*) present_stream->avc1 != NULL) {
		  if((stream_param_sets_t*) ((stream_AVC_conf_Record_t*)
					     (present_stream->avc1))->pps != NULL) {
		    if(((stream_param_sets_t*)
			((stream_AVC_conf_Record_t*)
			 (present_stream->avc1))->pps)->param_set != NULL) {
		      free(((stream_param_sets_t*)
			    ((stream_AVC_conf_Record_t*)
			     (present_stream->avc1))->pps)->param_set);
		    }
		    free((stream_param_sets_t*)
			 ((stream_AVC_conf_Record_t*)
			  (present_stream->avc1))->pps);
		  }
		  if((stream_param_sets_t*) ((stream_AVC_conf_Record_t*)
                                             (present_stream->avc1))->sps != NULL) {
                    if(((stream_param_sets_t*)
                        ((stream_AVC_conf_Record_t*)
                         (present_stream->avc1))->sps)->param_set != NULL) {
                      free(((stream_param_sets_t*)
                            ((stream_AVC_conf_Record_t*)
                             (present_stream->avc1))->sps)->param_set);
                    }
                    free((stream_param_sets_t*)
                         ((stream_AVC_conf_Record_t*)
                          (present_stream->avc1))->sps);
                  }
		  free(present_stream->avc1 );
		}
		if(bldr->video[j][i].sync_time_table)
		    free(bldr->video[j][i].sync_time_table);
		if(bldr->video[j][i].sync_sample_table)
		    free(bldr->video[j][i].sync_sample_table);
		if(bldr->video[j][i].sync_sam_size_table)
		    free(bldr->video[j][i].sync_sam_size_table);
		if(bldr->video[j][i].sync_start_sam_table)
		    free(bldr->video[j][i].sync_start_sam_table);
		if(bldr->video[j][i].chunk_data)
		    free(bldr->video[j][i].chunk_data);
	    }
	    for(i=0; i<bldr->n_aud_traks[j]; i++) {
		if(bldr->audio[j][i].adts)
                    free(bldr->audio[j][i].adts);
		if(bldr->audio[j][i].header)
		    free(bldr->audio[j][i].header);
		if(bldr->audio[j][i].sync_time_table)
		    free(bldr->audio[j][i].sync_time_table);
		if(bldr->audio[j][i].sync_sample_table)
		    free(bldr->audio[j][i].sync_sample_table);
		if(bldr->audio[j][i].sync_sam_size_table)
		    free(bldr->audio[j][i].sync_sam_size_table);
		if(bldr->audio[j][i].sync_start_sam_table)
		    free(bldr->audio[j][i].sync_start_sam_table);
		if(bldr->audio[j][i].chunk_data)
		    free(bldr->audio[j][i].chunk_data);
	    }
	}
	free(bldr);
    }
}
