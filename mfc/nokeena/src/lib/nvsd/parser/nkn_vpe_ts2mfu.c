#include <stdio.h>
#include <sys/stat.h>
#include "nkn_vpe_ts2mfu_api.h"
#include "errno.h"
#include <sys/uio.h>
#include <netinet/in.h>
#include "math.h"
#include "mfp_video_header.h"

#ifdef MEMDEBUG
#define A_(x) x 
#else
#define A_(x) 
#endif

static inline void
ts_seg_setup_olt(bitstream_state_t*, size_t, int32_t, vpe_olt_t*);
static uint16_t parse_pat_for_pmt(uint8_t*);
static uint16_t get_pkt_pid(uint8_t*);
static uint16_t get_pmt_pid(uint8_t*,uint32_t);
static void get_av_pids_from_pmt(uint8_t *data, uint16_t* vpid,
                                 uint16_t *apid,  uint16_t* apid2);
static int8_t get_spts_pids(uint8_t*  data,  uint32_t len,
                            uint16_t* vpid,  uint16_t *apid,
                            uint16_t* apid2, uint16_t pmt_pid);
static uint8_t get_payload_unit_start_indicator(uint8_t* data);
static void prepare_mfu_hdr(mfub_t *mfu_hdr, uint32_t pgm_number, uint32_t
		stream_id, uint32_t aud_time, uint32_t vid_time,
		int32_t bit_rate);
static void get_pmt_offset(uint8_t* data, uint32_t data_len,
                    uint16_t pmt_pid, uint32_t* pmt_offset);

static void rebase_segment_data_offset(vpe_olt_t *ol,
                           uint32_t pmt_offset);
static int32_t get_I_frame_offset(uint8_t *ts_pkt, uint32_t data_len, 
 			   ts_segmenter_ctx_t *seg_ctx,  
      	                   uint32_t *first_I_frame_offset);

uint32_t glob_chunk_desc = 0;
uint32_t glob_sam_desc = 0;
uint32_t glob_free_chunk_desc = 0;
uint32_t glob_free_sam_desc = 0;
extern file_id_t* file_state_cont;

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


/* This fucntion is used to allocate and intialize the ts to MFU
   builder structure.
   chunk_time-> i/p parameter, indicates the keyframe interval specified
   in PMF
   sl_pkg_filenames, streaming_type, num_pmf, n_traks, moov -- i/p param
   out -> o/p parameter ts2mfu builder which is updated
   iocb->i/p parameter */
int32_t
ts_mfu_builder_init(int32_t chunk_time,
		    char **sl_pkg_filenames,
		    int32_t *profile_bitrate,
		    int32_t streaming_type,
		    int32_t num_pmf,
		    int32_t n_input_files,
		    int32_t sess_id,
		    ts_mfu_builder_t **out)
{
    ts_mfu_builder_t *bldr = NULL;
    uint8_t data[1];
    uint32_t data_len = 0;
    int32_t err = 0, size_read = 0;
    int32_t j = 0;
    uint8_t first_byte_data[1] = {0};
    *out = bldr = (ts_mfu_builder_t*)
        nkn_calloc_type(1, sizeof(ts_mfu_builder_t),
                        mod_ts_mfu_builder_t);
    if (!bldr) {
        return -E_VPE_PARSER_NO_MEM;
    }
    bldr->num_pmf = num_pmf;
    bldr->chunk_time = chunk_time;
    bldr->streaming_type = streaming_type;
    bldr->n_profiles = n_input_files;
    //bldr->iocb = iocb;
    bldr->is_data_complete = 1;
    A_(printf("ts_mfu_builder_init %lu\n", nkn_memstat_type(mod_vpe_charbuf)));

    /*initializing the profile level info for each profile*/
    for(j=0; j<bldr->n_profiles; j++) {
      //init_ts_seg_ctx(&bldr->profile_info[j].seg_ctx);
	bldr->profile_info[j].aud_pid1 = 0;
	bldr->profile_info[j].aud_pid2 = 0;
	bldr->profile_info[j].vid_pid = 0;
	bldr->profile_info[j].num_chunks = 0;
	bldr->profile_info[j].PMT_data_offset = 0;
	//bldr->profile_info[j].chunk_duration = 0;
	bldr->profile_info[j].curr_chunk_num = 0;
	bldr->profile_info[j].bitrate = profile_bitrate[j];
	//choosing the which handler to use further
	if (strstr(sl_pkg_filenames[j], "http://")) {
	  bldr->profile_info[j].iocb = &ioh_http;
	} else {
	  bldr->profile_info[j].iocb = &iohldr;
	}
	bldr->profile_info[j].ts_name = strdup(sl_pkg_filenames[j]);
	bldr->profile_info[j].io_desc_ts = 
	  bldr->profile_info[j].iocb->ioh_open((char*) bldr->profile_info[j].ts_name,
	  "rb", 0);
	A_(printf("after http open %lu\n", nkn_memstat_type(mod_vpe_charbuf)));
        if (!bldr->profile_info[j].io_desc_ts) {
            err = -E_VPE_PARSER_INVALID_FILE;
            return err;
        }
	bldr->profile_info[j].iocb->ioh_seek(bldr->profile_info[j].io_desc_ts, 0, SEEK_END);
	data_len = bldr->profile_info[j].iocb->ioh_tell(bldr->profile_info[j].io_desc_ts);
	bldr->profile_info[j].iocb->ioh_seek(bldr->profile_info[j].io_desc_ts, 0, SEEK_SET);

	size_read = bldr->profile_info[j].iocb->ioh_read(&data[0], 1, 1, bldr->profile_info[j].io_desc_ts);
	if(size_read != 1) {
	  err = -E_VPE_READ_INCOMPLETE;
	  return err;
	}
        if(data[0] != 0x47) {
          err = -E_VPE_INVALID_FILE_FROMAT;
          return err;
        }
	bldr->profile_info[j].tot_size = data_len;
	bldr->profile_info[j].bytes_remaining = data_len;
	//bldr->profile_info[j].data = data;
	bldr->profile_info[j].chunk_desc = NULL;
	bldr->profile_info[j].curr_chunk_num = 0;
	bldr->profile_info[j].pmf_chunk_time = chunk_time;
	bldr->profile_info[j].seek_offset = 0;
	bldr->profile_info[j].is_data_complete = 0;
	bldr->profile_info[j].block_num = 0;
	bldr->profile_info[j].is_span_data = 0;
	bldr->profile_info[j].tot_num_chunks = 0;
	bldr->profile_info[j].last_samp_len = 0;
	bldr->profile_info[j].act_bit_rate = 0;
	bldr->profile_info[j].num_pkts_per_second = 0;
	bldr->profile_info[j].to_read = 0;
	bldr->profile_info[j].prev_bytes = 0;
	memset(bldr->profile_info[j].aud_pkts, 0, MFP_TS_MAX_PKTS * sizeof(uint32_t));
	memset(bldr->profile_info[j].aud_pkts_time, 0, MFP_TS_MAX_PKTS * sizeof(uint32_t));
	memset(bldr->profile_info[j].vid_pkts, 0, MFP_TS_MAX_PKTS * sizeof(uint32_t));
	memset(bldr->profile_info[j].vid_pkts_time, 0, MFP_TS_MAX_PKTS * sizeof(uint32_t));

	sess_stream_id_t *id = (sess_stream_id_t*) nkn_calloc_type(1, sizeof(sess_stream_id_t), 
						 mod_vpe_sess_stream_id_t);
	if(!id) {
	    return -E_VPE_PARSER_NO_MEM;
	}
	id->sess_id = sess_id;
	id->stream_id = j;
	
	A_(printf("before accum %d %lu\n", j, nkn_memstat_type(mod_unknown)));
	if(!bldr->profile_info[j].accum){
		bldr->profile_info[j].accum = newAccumulatorTS(id, FILE_SRC);
		if (!bldr->profile_info[j].accum) {
		    if (id) free(id);
		    return -E_VPE_PARSER_NO_MEM;
		}
	}
	A_(printf("after accum %d %lu\n", j ,nkn_memstat_type(mod_unknown)));
	if (id) free(id);
	//iocb->ioh_close(bldr->profile_info[j].io_desc_ts);
    }

    return err;
}

int32_t
ts_mfu_bldr_get_chunk_desc(ts_mfu_profile_builder_t *curr_profile)
{
  int32_t i =0;
  uint32_t j = 0;
  int32_t err = 0;
  uint64_t num_ts_pkts = 0;

  /* get the PIDS*/
  if(curr_profile->block_num == 0) {
    err = get_pids(curr_profile->data, curr_profile->data_len,
		   &curr_profile->aud_pid1, &curr_profile->vid_pid,
		   &curr_profile->aud_pid2,
		   &curr_profile->pmt_pid);
    if(err<0)
      return err;
  
    /* Audio only not supported */
    if (!curr_profile->vid_pid) {
      err = -E_VPE_AUD_ONLY_NOT_SUPPORTED;
      return err;
    }
  }
  
  /* allocating the chunk_desc, assumed that num_chunks will not be
   * greater thatn number of ts pkts*/
  num_ts_pkts = curr_profile->data_len / 188;
  curr_profile->chunk_desc =
    nkn_calloc_type(num_ts_pkts,
		    sizeof(chunk_desc_t*),
		    mod_chunk_desc_t);
  if (!curr_profile->chunk_desc) {
    return -E_VPE_PARSER_NO_MEM;
  }

  if(curr_profile->block_num == 0) {
    get_pmt_offset(curr_profile->data, curr_profile->data_len,
		   curr_profile->pmt_pid, &curr_profile->pmt_offset);
  } else {
    curr_profile->pmt_offset = 0;
  }
  /* get number of segments or chunks and duration */
  err = ts_mfu_update_chunk_desc(curr_profile->data+curr_profile->pmt_offset,
				 &curr_profile->num_chunks,
				 curr_profile->data_len-curr_profile->pmt_offset,
				 curr_profile->chunk_duration, curr_profile->aud_pid1,
				 curr_profile->vid_pid, &curr_profile->first_I_offset,
				 curr_profile->chunk_desc, curr_profile->pmt_offset,
				 curr_profile->pmf_chunk_time);
  if(err)
    return err;
  return err;
  
}


/* This function is used to update all the elemnts of the ts2mfu
   builder structure. */

int32_t
ts_mfu_bldr_update(ts_mfu_builder_t *bldr, int32_t n_profiles)
{
    int32_t i =0;
    uint32_t j = 0;
    int32_t err = 0;
    ts_mfu_profile_builder_t *curr_profile = NULL;
    uint64_t num_ts_pkts = 0;
    for(i=0; i<bldr->n_profiles; i++) {
	curr_profile = &bldr->profile_info[i];
	/* get the PIDS*/
	err = get_pids(curr_profile->data, curr_profile->data_len,
		       &curr_profile->aud_pid1, &curr_profile->vid_pid,
		       &curr_profile->aud_pid2,
		       &curr_profile->pmt_pid);
	if(err<0)
	    return err;
	
	/* Audio only not supported */
	if (!curr_profile->vid_pid) {
	    err = -E_VPE_AUD_ONLY_NOT_SUPPORTED;
	    return err;
	}
	/* allocating the chunk_desc, assumed that num_chunks will not be 
	 * greater thatn number of ts pkts*/
	num_ts_pkts = curr_profile->data_len / 188;
        curr_profile->chunk_desc =
	  nkn_calloc_type(num_ts_pkts,
			  sizeof(chunk_desc_t*),
			  mod_chunk_desc_t);
        if (!curr_profile->chunk_desc) {
	  return -E_VPE_PARSER_NO_MEM;
        }
	
	get_pmt_offset(curr_profile->data, curr_profile->data_len,
		       curr_profile->pmt_pid, &curr_profile->pmt_offset);

	/* get number of segments or chunks and duration */
	err = ts_mfu_update_chunk_desc(curr_profile->data+curr_profile->pmt_offset,
				       &curr_profile->num_chunks,
				       curr_profile->data_len-curr_profile->pmt_offset,
				       curr_profile->chunk_duration, curr_profile->aud_pid1, 
				       curr_profile->vid_pid, &curr_profile->first_I_offset,
				       curr_profile->chunk_desc, curr_profile->pmt_offset,
				       curr_profile->pmf_chunk_time);
	if(err)
	    return err;
	/*get the offset of first I_frame */
#if !defined(INC_ALIGNER)
        get_I_frame_offset(curr_profile->data+curr_profile->pmt_offset,
                           curr_profile->data_len-curr_profile->pmt_offset,
                           curr_profile->seg_ctx,
                           &curr_profile->first_I_offset);

        curr_profile->seg_start_offset = curr_profile->pmt_offset +
	  curr_profile->first_I_offset;
#endif

    }//for loop
#if 0
    //check whether all the profiles num_chunks is same or not
    for(i=0; i<(bldr->n_profiles-1); i++) {
	if(bldr->profile_info[i].num_chunks !=
	   bldr->profile_info[i+1].num_chunks) {
	    err = -E_VPE_DIFF_TS_CHUNKS_ACR_PROFILE;
	}
    }

    bldr->num_chunks = bldr->profile_info[0].num_chunks;
    /*usually first chunk duration may be larger than the others so
    taking the second chunk duration as reference */
    bldr->chunk_time = bldr->profile_info[0].chunk_duration[1];
#endif
    return err;
}


#if !defined(INC_ALIGNER)
int32_t 
ts_segment_data(bitstream_state_t *bs, vpe_olt_t *ol, 
		ts_segmenter_ctx_t *seg_ctx,
		uint32_t curr_chunk_num,
		int32_t *curr_profile_start_time)
{
    int32_t err = 0, PTS = 0;
    tplay_index_record tplay_record;
    //    ts_segmenter_ctx_t *seg_ctx = NULL;
    uint8_t *ts_pkt_data = NULL;
    size_t segment_start_offset = 0, ts_pkt_size = 0,
	segment_end_offset =0;
    uint32_t pkt_num = 0;
    uint32_t num_I_frame = 0;

    segment_end_offset = 0;
    segment_start_offset = 0;//bio_get_pos(bs);
    ts_pkt_size = TS_PKT_SIZE;
    
    /**
     * should always point to a I-Frame which needs to be skipped to
     * find the end of the current segment 
     */
    bio_aligned_seek_bitstream(bs, TS_PKT_SIZE,
			       SEEK_CUR);

    //init_ts_seg_ctx(&seg_ctx);

    while (bio_aligned_direct_read(bs, &ts_pkt_data, &ts_pkt_size)) {
	pkt_num++;
	if(pkt_num == 474) {
	    pkt_num = pkt_num;
	    pkt_num = 474;
	}
	handle_pkt_tplay(ts_pkt_data, seg_ctx->ts_pkt_props,
                         &tplay_record, seg_ctx->codec_span_data);
	if (seg_ctx->codec_span_data->frame_type_pending) {
            continue;
        }
	if (tplay_record.type == TPLAY_RECORD_PIC_IFRAME) {
	    num_I_frame++;
	    tplay_record.type = 0;
	    //seg_ctx->codec_span_data->frame_type_pending = 1;
	    if(curr_chunk_num == 0) {
                *curr_profile_start_time =        \
                    *curr_profile_start_time;
		/* + seg_ctx->curr_profile_start_time == 0) *	\
		   seg_ctx->codec_span_data->first_PTS;*/
            } else {
                *curr_profile_start_time =        \
                    *curr_profile_start_time +    \
                    (*curr_profile_start_time == 0) *   \
                    (tplay_record.PTS/TS_CLK_FREQ) ;
            }

            PTS = tplay_record.PTS/TS_CLK_FREQ -        \
                *curr_profile_start_time;

	    /* re-scan the current packet as a the first packet of
             * the next segment */
            if(0) {//curr_chunk_num != 0) {
                bio_aligned_seek_bitstream(bs, -TS_PKT_SIZE,
                                           SEEK_CUR);
            }
	    /* copy the offset length in the current buffer */
	    ts_seg_setup_olt(bs, segment_start_offset, PTS,
				   ol);
	    break;
	}
    }
    if(num_I_frame == 0) {
	/* copy the offset length in the current buffer */
	ts_seg_setup_olt(bs, segment_start_offset, PTS,
			 ol);
    }
    return err;
}
#else

int32_t
ts_segment_data(uint8_t *ts_pkt, uint32_t len, vpe_olt_t *ol,
                ts_segmenter_ctx_t *seg_ctx,
                uint32_t curr_chunk_num,
                int32_t *curr_profile_start_time, 
		uint32_t vid_pid, uint32_t aud_pid1)
{
  ts_pkt_infotable_t *pkt_tbl = NULL;
  uint32_t *pkt_offsets = NULL;//[MAX_NUM_INPT_PKTS];
  uint32_t cnt = 0, i =0, pid = 0;
  uint32_t n_ts_pkts = 0, frame_type_num = 0;
  int32_t err = 0;
  slice_type_e frame_type = slice_default_value;
  uint32_t first_I_frame = 0, num_I_frames = 0;
  uint32_t segment_start_offset = 0;//ts_pkt;

  n_ts_pkts = len / 188;
  pkt_offsets = (uint32_t*)nkn_calloc_type(n_ts_pkts, sizeof(uint32_t), mod_vpe_pkt_offsets_t);
  if (!pkt_offsets) {
    return -E_VPE_PARSER_NO_MEM;
  }
  for( cnt = 0; cnt < n_ts_pkts; cnt++){
    pkt_offsets[cnt] = cnt * TS_PKT_SIZE;
  }
  err = create_index_table(ts_pkt, pkt_offsets,
			   n_ts_pkts, aud_pid1,
			   vid_pid, &pkt_tbl);
  if(err < 0){
    return err;
  }
  while(i < n_ts_pkts) {
    pid = pkt_tbl[i].pid;
    if(pid!=vid_pid) {
      i++;
      continue;
    }
    frame_type = find_frame_type(n_ts_pkts, i, vid_pid, pkt_tbl, &frame_type_num);
    if(frame_type == slice_type_i && (pid == vid_pid)){
      if(first_I_frame != 0) {
	//ol->offset = segment_start_offset;
	//ol->length = i * 188;
	break;
      }
      first_I_frame++;
    }
    if(i == frame_type_num) {
      i++;
    } else {
      i = frame_type_num + 1;
    }

  }
  ol->offset = segment_start_offset;
  ol->length = i * 188;
  if(pkt_offsets != NULL)
    free(pkt_offsets);
  if(pkt_tbl != NULL)
    free(pkt_tbl);
  return err;

}
#endif

static inline void
ts_seg_setup_olt(bitstream_state_t *bs, size_t start_offset,
                       int32_t PTS, vpe_olt_t *ol)

{
    /* store the offset, length, time */
    ol->offset = start_offset;
    ol->length = bio_get_pos(bs) - TS_PKT_SIZE - start_offset;
    ol->time = PTS;
}

static void
rebase_segment_data_offset(vpe_olt_t *ol,
                           uint32_t pmt_offset)
{
    ol->offset = ol->offset + pmt_offset;
    ol->length = ol->length;


}

int32_t 
get_pids(uint8_t *data, uint32_t data_len, uint16_t *aud_pid1, uint16_t
	 *vid_pid, uint16_t *aud_pid2, uint16_t *pmt_pid)
{
    int32_t ret = 1;

    *pmt_pid =  get_pmt_pid((uint8_t*)data, data_len);
    if(*pmt_pid == 0)
	return -E_VPE_NO_PMT_FOUND;
    ret = get_spts_pids(data, data_len,
			vid_pid, aud_pid1,
			aud_pid2, *pmt_pid);
    if(ret == 0)
	return -E_VPE_ERR_PARSE_PIDS;
    return  ret;


}

static int32_t 
get_I_frame_offset(uint8_t *ts_pkt, uint32_t data_len, 
		   ts_segmenter_ctx_t *seg_ctx,
		   uint32_t *first_I_frame_offset)
{
    int32_t err = 0;
    tplay_index_record tplay_record;
    //ts_segmenter_ctx_t *seg_ctx;
    uint32_t pkt_num = 0, num_ts_pkt = 0;
    uint32_t i =0;
    uint32_t data_offset = 0;
    //    init_ts_seg_ctx(&seg_ctx);
    num_ts_pkt = data_len / TS_PKT_SIZE;
    for(i=0; i<num_ts_pkt; i++) {
	handle_pkt_tplay(ts_pkt+data_offset, seg_ctx->ts_pkt_props,
                         &tplay_record, seg_ctx->codec_span_data);
        data_offset += TS_PKT_SIZE;
        pkt_num++;
        if (seg_ctx->codec_span_data->frame_type_pending) {
            continue;
        }
        if (tplay_record.type == TPLAY_RECORD_PIC_IFRAME) {
	    tplay_record.type = 0;
            seg_ctx->codec_span_data->frame_type_pending = 1;
	    break;
	}
    }
    *first_I_frame_offset = (i)*TS_PKT_SIZE;

    //cleanup_ts_segmenter(seg_ctx);
    return err;
}


int32_t
ts_mfu_calculate_num_chunks(uint8_t *ts_pkt, uint32_t *num_chunks,
			    uint32_t data_len, uint32_t *duration, 
			    uint32_t aud_pid1, uint32_t vid_pid, 
			    uint32_t *first_I_offset)
{
#if !defined(INC_ALIGNER)
    int32_t err = 0;
    tplay_index_record tplay_record;
    ts_segmenter_ctx_t *seg_ctx;
    uint32_t num_I_frames = 0, data_offset = 0, num_ts_pkt = 0;
    uint32_t pkt_num = 0;
    uint32_t first_I_frame = 0;
    uint32_t prev_PTS = 0, present_PTS = 0;
    init_ts_seg_ctx(&seg_ctx);
    num_ts_pkt = data_len / TS_PKT_SIZE;
    data_offset = 0;

    while(num_ts_pkt > 0) {
	handle_pkt_tplay(ts_pkt+data_offset, seg_ctx->ts_pkt_props,
			 &tplay_record, seg_ctx->codec_span_data);
	data_offset += TS_PKT_SIZE;
        num_ts_pkt--;
        pkt_num++;
	if (seg_ctx->codec_span_data->frame_type_pending) {
            continue;
        }
	if (tplay_record.type == TPLAY_RECORD_PIC_IFRAME) {
	  present_PTS = tplay_record.PTS;///TS_CLK_FREQ;
	    if(first_I_frame == 0) {
		duration[num_I_frames] = present_PTS/TS_CLK_FREQ;
	    } else {
	      duration[num_I_frames] = (present_PTS - prev_PTS)/TS_CLK_FREQ;
	    }
	    num_I_frames += 1;
	    tplay_record.type = 0;
	    seg_ctx->codec_span_data->frame_type_pending = 1;
	    first_I_frame++;
	    prev_PTS = present_PTS;
	}
    }
    *num_chunks = num_I_frames;
    cleanup_ts_segmenter(seg_ctx);
    return err;
#else

    ts_pkt_infotable_t *pkt_tbl = NULL;    
    uint32_t *pkt_offsets = NULL;//[MAX_NUM_INPT_PKTS];
    uint32_t cnt = 0, i =0;
    uint32_t n_ts_pkts = 0;
    int32_t err = 0;
    slice_type_e frame_type = slice_default_value;
    uint32_t first_I_frame = 0, num_I_frames = 0;
    uint32_t prev_PTS = 0, present_PTS = 0, pid = 0;
    uint32_t pts = 0, dts = 0, prev_pkt_pts = 0;
    uint32_t frame_type_num = 0;
    n_ts_pkts = data_len / 188;
    pkt_offsets = (uint32_t*)nkn_calloc_type(n_ts_pkts, sizeof(uint32_t), mod_vpe_pkt_offsets_t);
    if (!pkt_offsets) {
      return -E_VPE_PARSER_NO_MEM;
    }
    for( cnt = 0; cnt < n_ts_pkts; cnt++){
      pkt_offsets[cnt] = cnt * TS_PKT_SIZE;
    }
    err = create_index_table(ts_pkt, pkt_offsets,
                             n_ts_pkts, aud_pid1,
                             vid_pid, &pkt_tbl);
    if(err < 0){
      return err;
    }
    //for (i = 0; i < n_ts_pkts; i++) {
    while(i < n_ts_pkts) {
      pid = pkt_tbl[i].pid;
      if(pid!=vid_pid) {
	i++;
	continue;
      }
      if(i == 471) {
	i = i;
	i = 471;
      }
      frame_type = find_frame_type(n_ts_pkts, i, vid_pid, pkt_tbl, &frame_type_num);
      dts = pkt_tbl[i].dts;
      pts = pkt_tbl[i].pts;
      if(frame_type == slice_type_i && (pid == vid_pid)){
	present_PTS = pts;
	if(first_I_frame == 0) {
	  *first_I_offset = i * TS_PKT_SIZE;
	  duration[num_I_frames] = present_PTS/TS_CLK_FREQ;
	} else {
	  duration[num_I_frames] = (present_PTS - prev_PTS)/TS_CLK_FREQ;
	}
	first_I_frame += 1;
	num_I_frames += 1;
	prev_PTS = present_PTS;
      }
      prev_pkt_pts = pts;
      if(i == frame_type_num) {
	i++;
      } else {
	i = frame_type_num + 1;
      }
    }
    *num_chunks = num_I_frames;
    if(pkt_offsets != NULL)
      free(pkt_offsets);
    if(pkt_tbl != NULL)
      free(pkt_tbl);
    return err;

#endif
}

int32_t
ts_mfu_update_chunk_desc(uint8_t *ts_pkt, uint32_t *num_chunks,
			 uint32_t data_len, uint32_t *duration,
			 uint32_t aud_pid1, uint32_t vid_pid,
			 uint32_t *first_I_offset, 
			 chunk_desc_t **chunk_desc, uint32_t pmt_offset, 
			 uint32_t pmf_chunk_time)
{
  ts_pkt_infotable_t *pkt_tbl = NULL;
  uint32_t *pkt_offsets = NULL;//[MAX_NUM_INPT_PKTS];
  uint32_t cnt = 0, i =0;
  uint32_t n_ts_pkts = 0;
  int32_t err = 0;
  slice_type_e frame_type = slice_default_value;
  uint32_t first_I_frame = 0, num_I_frames = 0;
  uint32_t prev_PTS = 0, present_PTS = 0, pid = 0;
  uint32_t pts = 0, dts = 0, prev_pkt_pts = 0;
  uint32_t frame_type_num = 0, segment_start_offset = 0, k=0;
  uint32_t curr_profile_start_time = 0, calc_PTS = 0, req_PTS = 0;
  n_ts_pkts = data_len / 188;
  pkt_offsets = (uint32_t*)nkn_calloc_type(n_ts_pkts, sizeof(uint32_t), mod_vpe_pkt_offsets_t);
  if (!pkt_offsets) {
    return -E_VPE_PARSER_NO_MEM;
  }
  for( cnt = 0; cnt < n_ts_pkts; cnt++){
    pkt_offsets[cnt] = cnt * TS_PKT_SIZE;
  }
  err = create_index_table(ts_pkt, pkt_offsets,
                           n_ts_pkts, aud_pid1,
                           vid_pid, &pkt_tbl);
  if(err < 0){
    return err;
  }
  while(i < n_ts_pkts) {
    pid = pkt_tbl[i].pid;
    if(pid!=vid_pid) {
      i++;
      continue;
    }
    frame_type = find_frame_type(n_ts_pkts, i, vid_pid, pkt_tbl, &frame_type_num);
    dts = pkt_tbl[i].dts;
    pts = pkt_tbl[i].pts;
    if(frame_type == slice_type_i && (pid == vid_pid)){
      for(k=i; k<=frame_type_num; k++) {
	if((pkt_tbl[k].pts != 0) &&(pkt_tbl[k].pid == vid_pid)) {
	  pts = pkt_tbl[k].pts;
	  break;
	}
      }

      if(first_I_frame != 0) {
	if(curr_profile_start_time == 0 ) {
	  curr_profile_start_time = curr_profile_start_time;
	} else {
	  curr_profile_start_time = curr_profile_start_time + 
	    (curr_profile_start_time == 0) * (pts/TS_CLK_FREQ);
	}
      } else {
	curr_profile_start_time = curr_profile_start_time +
	  (curr_profile_start_time == 0) * (pts/TS_CLK_FREQ);
      }
      if((pts > 0)) { 
	calc_PTS = pts/TS_CLK_FREQ - curr_profile_start_time;
      }
      /*if pmf_chunk_time is 0, then segmentation should occur 
       * at the actual key frame interval. So making the cal_PTS
       * and req_PTS as same to enter into if condition*/
      if(pmf_chunk_time == 0) {
	req_PTS = calc_PTS;
      } else {
	req_PTS = (num_I_frames +1) *pmf_chunk_time;
      }
      if(first_I_frame == 0) {
        segment_start_offset = i*188;
        prev_PTS = curr_profile_start_time * TS_CLK_FREQ;
      }
      if ((calc_PTS >= req_PTS) && (calc_PTS != 0 ) &&
	  (calc_PTS != ((prev_PTS/TS_CLK_FREQ) - curr_profile_start_time))){
	  present_PTS = pts;

	  /* allocating memeory for sam_desc */
	  chunk_desc[num_I_frames] =  NULL;
	  chunk_desc[num_I_frames] = (chunk_desc_t*)
	    nkn_calloc_type(1, sizeof(chunk_desc_t),
			    mod_vpe_chunk_desc_t);
	  if (!chunk_desc[num_I_frames]) {
	    return -E_VPE_PARSER_NO_MEM;
	  }
	  glob_chunk_desc++;
	  chunk_desc[num_I_frames]->sam_desc = (vpe_olt_t*)
	    nkn_calloc_type(1, sizeof(vpe_olt_t), mod_vpe_offsets_t);
	  if (!chunk_desc[num_I_frames]->sam_desc) {
	    return -E_VPE_PARSER_NO_MEM;
	  }
	  glob_sam_desc++;
	  //duration[num_I_frames] = (present_PTS - prev_PTS)/TS_CLK_FREQ;
	  chunk_desc[num_I_frames]->sam_desc->offset = segment_start_offset +pmt_offset;
	  chunk_desc[num_I_frames]->sam_desc->length = (i*188) - segment_start_offset;
	  num_I_frames += 1;
	  segment_start_offset = i * 188;

	prev_PTS = present_PTS;
      }
      first_I_frame += 1;
    }
    if(i == frame_type_num) {
      i++;
    } else {
      i = frame_type_num + 1;
    }
    
  }
  // last chunk
  chunk_desc[num_I_frames] =  NULL;
  chunk_desc[num_I_frames] = (chunk_desc_t*)
    nkn_calloc_type(1, sizeof(chunk_desc_t),
		    mod_vpe_chunk_desc_t);
  if (!chunk_desc[num_I_frames]) {
    return -E_VPE_PARSER_NO_MEM;
  }
  glob_chunk_desc++;
  chunk_desc[num_I_frames]->sam_desc = (vpe_olt_t*)
    nkn_calloc_type(1, sizeof(vpe_olt_t), mod_vpe_offsets_t);
  if (!chunk_desc[num_I_frames]->sam_desc) {
    return -E_VPE_PARSER_NO_MEM;
  }
  glob_sam_desc++;
  //duration[num_I_frames] = duration[num_I_frames-1];
  chunk_desc[num_I_frames]->sam_desc->offset = segment_start_offset +pmt_offset;
  chunk_desc[num_I_frames]->sam_desc->length = (i*188) - segment_start_offset;
  num_I_frames += 1;
  segment_start_offset = i * 188;
  *num_chunks = num_I_frames;
  if(pkt_offsets != NULL)
    free(pkt_offsets);
  if(pkt_tbl != NULL)
    free(pkt_tbl);
  return err;
}


static int32_t 
init_ts_seg_ctx(ts_segmenter_ctx_t **out)
{
    ts_segmenter_ctx_t *ctx;
    int32_t err;

    err = 0;
    ctx = NULL;
    *out = NULL;

    *out = ctx = (ts_segmenter_ctx_t*)\
        nkn_calloc_type(1, sizeof(ts_segmenter_ctx_t),
                        mod_vpe_ts_segmenter_ctx_t);
    if (!ctx) {
        return -E_VPE_PARSER_NO_MEM;
    }

    ctx->ts_pkt_props = (Ts_t*)\
        nkn_calloc_type(1, sizeof(Ts_t),
                        mod_vpe_ts_tplay_parser);
    if (!ctx->ts_pkt_props) {
        free(ctx);
        return -E_VPE_PARSER_NO_MEM;
    }
    /* initialize the TS packet props, wish there was a clean init
     * function to do this
     */
    Ts_alloc(ctx->ts_pkt_props);
    ctx->ts_pkt_props->vsinfo->num_pids=0;
    ctx->ts_pkt_props->asinfo->num_pids=0;
    ctx->ts_pkt_props->num_pids=0;
    ctx->ts_pkt_props->num_pkts=0;
    ctx->ts_pkt_props->pas->prev_cc=0x0F;

    ctx->codec_span_data = (Misc_data_t*)\
        nkn_calloc_type(1, sizeof(Misc_data_t),
                        mod_vpe_ts_tplay_parser);
    ctx->segment_duration = 0;//fconv_ip->key_frame_interval;
    ctx->curr_segment_num = 1;

    return err;
}

int32_t 
ts_mfu_conversion(ts_mfu_profile_builder_t *bldr, mfu_data_t**
		  mfu_data, uint32_t pgm_number, uint32_t stream_id)
{
    int32_t err = 0;
    //ts2mfu_cont_t *ts2mfu_cont = NULL;
    //accum_ts_t *accum = NULL;
    sess_stream_id_t *id = NULL;
    uint32_t audio_time = 0, video_time = 0;
    uint32_t i =0;
    int32_t bit_rate = 0;
    uint8_t *saved_accum_data_ptr = NULL;
    mfp_publ_t *pub_ctx = NULL;
    pub_ctx = file_state_cont->get_ctx(file_state_cont, pgm_number);

#if !defined(INC_ALIGNER)
    bldr->chunk_desc[bldr->curr_chunk_num] =  NULL;
    bldr->chunk_desc[bldr->curr_chunk_num] = (chunk_desc_t*)
	nkn_calloc_type(1, sizeof(chunk_desc_t),
			mod_vpe_chunk_desc_t);
    if (!bldr->chunk_desc[bldr->curr_chunk_num]) {
	return -E_VPE_PARSER_NO_MEM;
    }
    bldr->chunk_desc[bldr->curr_chunk_num]->sam_desc = (vpe_olt_t*)
	nkn_calloc_type(1, sizeof(vpe_olt_t), mod_vpe_offsets_t);
    if (!bldr->chunk_desc[bldr->curr_chunk_num]->sam_desc) {
	return -E_VPE_PARSER_NO_MEM;
    }
    bldr->bs =
	bio_init_safe_bitstream(bldr->data+bldr->seg_start_offset,
				bldr->data_len-bldr->seg_start_offset);
    err = ts_segment_data(bldr->bs,
			  bldr->chunk_desc[bldr->curr_chunk_num]->sam_desc,
			  bldr->seg_ctx,
			  bldr->curr_chunk_num,
			  &bldr->curr_profile_start_time);
    if(err)
	return err;
    if(bldr->bs !=NULL) {
	bio_close_bitstream(bldr->bs);
    }
    rebase_segment_data_offset(bldr->chunk_desc[bldr->curr_chunk_num]->sam_desc, bldr->seg_start_offset);
#endif
    update_aud_vid_offset(bldr->data,
			  bldr->chunk_desc[bldr->curr_chunk_num]->sam_desc,
			  bldr->aud_pid1, bldr->aud_pkts,
			  &bldr->aud_npkts, bldr->aud_pkts_time,
			  bldr->vid_pid, bldr->vid_pkts,
			  &bldr->vid_npkts, bldr->vid_pkts_time,
			  &bldr->num_vid_payload_start_pkts,
			  &bldr->num_aud_payload_start_pkts);

    /**
     * handle the case where the last segment has only one AU 
     */
    if ((bldr->num_vid_payload_start_pkts == 1)){// || (bldr->num_aud_payload_start_pkts < 4)) {
	err = E_VPE_FILE_CONVERT_STOP;
      	DBG_MFPLOG (pub_ctx->name, ERROR, MOD_MFPFILE,
		"sess %u stream %u: chunking error num_vidpkts %u num_audpkts %u",
		pgm_number, stream_id, bldr->num_vid_payload_start_pkts, 
		bldr->num_aud_payload_start_pkts);	
	//err = -E_VPE_PARSER_PAYLOAD_CHUNKING_ERR;
	goto done;
    }

    /*
     * if the media has a video pid then every segment should have
     * atleast 1 AU. otherwise something is fatally wrong, need to
     * assert here
     */
    if (bldr->vid_pid && !bldr->num_vid_payload_start_pkts) {
	assert(0);
    }

    /*calculate the vid duration */
    if(bldr->vid_pid && !video_time) {
	for(i=0;i<(bldr->num_vid_payload_start_pkts-1);i++){
	    if(((int64_t)bldr->vid_pkts_time[i+1]-(int64_t)bldr->vid_pkts_time[i]) < 0){
		uint32_t d = 0;
		video_time += bldr->vid_pkts_time[i+1]+(0xFFFFFFFF-bldr->vid_pkts_time[i]);
	    }
	    else
		video_time += bldr->vid_pkts_time[i+1] -bldr->vid_pkts_time[i];
	}
	//last sample_duration
	video_time += bldr->vid_pkts_time[bldr->num_vid_payload_start_pkts-1] -
	    bldr->vid_pkts_time[bldr->num_vid_payload_start_pkts-2];
    }

    /*calculate the aud duration */
    if(bldr->aud_pid1 && !audio_time) {
	if (!bldr->num_aud_payload_start_pkts) {
	    audio_time = 0;
	    goto skip_audio;
	}
	for(i=0;i<(bldr->num_aud_payload_start_pkts-1);i++){
	    if(((int64_t)bldr->aud_pkts_time[i+1]-(int64_t)bldr->aud_pkts_time[i]) < 0){
		uint32_t d = 0;
		audio_time +=
		    bldr->aud_pkts_time[i+1]+(0xFFFFFFFF-bldr->aud_pkts_time[i]);
	    } else {
		audio_time += bldr->aud_pkts_time[i+1]
		    -bldr->aud_pkts_time[i];
	    }
	}
	//last sample_duration
	if(bldr->num_aud_payload_start_pkts >= 2) {
	  audio_time +=
	    bldr->aud_pkts_time[bldr->num_aud_payload_start_pkts-1] -
	    bldr->aud_pkts_time[bldr->num_aud_payload_start_pkts-2];
	} else {
	  audio_time += bldr->aud_pkts_time[0];
	}
    }

skip_audio:


#if !defined(INC_ALIGNER)
    bldr->seg_start_offset =
	bldr->chunk_desc[bldr->curr_chunk_num]->sam_desc->offset +
	bldr->chunk_desc[bldr->curr_chunk_num]->sam_desc->length;
#endif
    id = (sess_stream_id_t*) nkn_calloc_type(1, sizeof(sess_stream_id_t), 
					     mod_vpe_sess_stream_id_t);
    if(!id) {
	return -E_VPE_PARSER_NO_MEM;
    }
    id->sess_id = pgm_number;
    id->stream_id = stream_id;
    saved_accum_data_ptr = bldr->accum->data;
    bldr->accum->data = bldr->data;
    bit_rate = bldr->act_bit_rate;
    prepare_mfu_hdr(&bldr->accum->mfu_hdr, pgm_number, stream_id, audio_time,
		    video_time, bit_rate);
    id->ts2mfu_cont.video_duration = video_time;
    bldr->accum->spts->mux.video_pid = bldr->vid_pid;
    bldr->accum->spts->mux.audio_pid1 =bldr->aud_pid1;
    bldr->accum->spts->mux.audio_pid2 = 0;
    bldr->accum->spts->mux.pmt_pid = bldr->pmt_pid;
    *mfu_data = mfp_ts2mfu(bldr->vid_pkts, bldr->vid_npkts,
			   bldr->aud_pkts, bldr->aud_npkts,
			   bldr->accum, &id->ts2mfu_cont);
    bldr->accum->data = saved_accum_data_ptr;
    if(*mfu_data == VPE_ERROR){
	*mfu_data = NULL;
	return - E_VPE_MFU_CREATION_FAILED;
    }

    /*freeing accum_t and id */
done:
    cleanup_chunk_desc_contents(bldr, bldr->curr_chunk_num);

    if(id)
	free(id);
    return err;
}

static void
prepare_mfu_hdr(mfub_t *mfu, uint32_t pgm_number, uint32_t stream_id,
		uint32_t aud_time, uint32_t vid_time, int32_t bit_rate)
{
    double temp_duration = 0;
    mfu->version = 0x10;
    mfu->program_number = pgm_number;
    //mfu->program_number = glob_mfp_live_tot_pmfs-1;
    mfu->stream_id = stream_id;
    mfu->timescale = 90000;
    mfu->audio_duration = aud_time;
    mfu->video_rate = bit_rate;
#if 0
    if(st->med_type == VIDEO|| st->med_type == MUX)
        mfu->video_rate = st->bit_rate;
    if(st->med_type == AUDIO)
        mfu->audio_rate = st->bit_rate;
#endif
    temp_duration = (double)((double)vid_time/90000);
    mfu->duration = round(temp_duration);
    mfu->offset_vdat = 0;
    mfu->offset_adat = 0;


}

static
void get_pmt_offset(uint8_t* data, uint32_t data_len,
		    uint16_t pmt_pid, uint32_t* pmt_offset)
{
    uint32_t num_pkts = 0;
    uint32_t i =0;
    uint16_t pid =0;
    uint32_t pkt_num = 0;
    num_pkts = data_len/TS_PKT_SIZE;
    for(i=0; i< num_pkts; i++) {

        pid = get_pkt_pid(data+(i*TS_PKT_SIZE));
	if(pid == pmt_pid) {
	    *pmt_offset = i*TS_PKT_SIZE;
	    break;
	}
	pkt_num++;
    }

}

int32_t 
update_aud_vid_offset(uint8_t *data, vpe_olt_t *sam_desc, uint16_t
		      aud_pid1, uint32_t *aud_pkts, uint32_t
		      *aud_npkts, uint32_t *aud_pkts_time, uint16_t
		      vid_pid, uint32_t *vid_pkts, uint32_t
		      *vid_npkts, uint32_t *vid_pkts_time, uint32_t
		      *num_vid_payload_start_pkts, uint32_t
		      *num_aud_payload_start_pkts)
{
    int32_t err = 0;
    int32_t start_offset = sam_desc->offset;
    int32_t len = sam_desc->length;
    uint16_t pid = 0;
    uint8_t *initial_data = data + sam_desc->offset;
    uint32_t num_pkts = 0, aud_num_pkts = 0, vid_num_pkts = 0, k = 0,
	k1 = 0;
    uint32_t i = 0;
    uint8_t payload_unit_start_indicator = 0;
    uint32_t pkt_time = 0, temp_pkt_time = 0;
    uint32_t pts, dts;
    num_pkts = len/TS_PKT_SIZE;
    for(i=0; i< num_pkts; i++) {
	pid = get_pkt_pid(initial_data+(i*TS_PKT_SIZE));
	mfu_converter_get_timestamp(initial_data+(i * 188),NULL,&pid, &pts, &dts, NULL);
	
	pkt_time = dts;
	if(!dts)
	  pkt_time = pts;

	payload_unit_start_indicator =
	    get_payload_unit_start_indicator(initial_data+(i*TS_PKT_SIZE));
	if(1) {//payload_unit_start_indicator == 1) {
	  if(aud_pid1) {
	    if(pid == aud_pid1) {
	      if(payload_unit_start_indicator) {
		    temp_pkt_time = pkt_time;
		    aud_pkts_time[k1] = pkt_time;
		    k1++;
		}
		aud_pkts[aud_num_pkts] = start_offset
		    +(i*TS_PKT_SIZE);
		aud_num_pkts++;
	    }
	  }
	  if(vid_pid) {
	    if(pid == vid_pid) {
		if(payload_unit_start_indicator) {
                    temp_pkt_time = pkt_time;
		    vid_pkts_time[k] = pkt_time;
		    k++;
                }
		vid_pkts[vid_num_pkts] = start_offset
		    +(i*TS_PKT_SIZE);
		//vid_pkts_time[vid_num_pkts] = temp_pkt_time;
		vid_num_pkts++;
	    }
	  }
	}
    }
    *aud_npkts = aud_num_pkts;
    *vid_npkts = vid_num_pkts;
    *num_vid_payload_start_pkts = k;
    *num_aud_payload_start_pkts = k1;
    return err;
}

static uint8_t get_payload_unit_start_indicator(uint8_t* data)
{
    uint8_t payload_unit_start_indicator = 0;
    payload_unit_start_indicator=(data[1]>>6)&0x01; 
    return payload_unit_start_indicator;

}
static uint16_t get_pmt_pid(uint8_t* data,uint32_t len){

    uint32_t i, n_ts_pkts=0;
    uint16_t pid, pmt_pid = 0;
    n_ts_pkts = len/188;
    for(i=0;i<n_ts_pkts;i++){
        pid = get_pkt_pid(data + i*188);
        if(!pid){
            pmt_pid = parse_pat_for_pmt(data + i*188);
            break;
        }
    }

    return pmt_pid;
}

static uint16_t get_pkt_pid(uint8_t* data){
    uint16_t pid = 0;
    pid = data[1]&0x1F;
    pid<<=8;
    pid|=data[2];
    return pid;
}

static uint16_t parse_pat_for_pmt(uint8_t *data){
    uint16_t pid = 0, pos = 0,program_number,program_map_PID=0;;
    uint8_t adaf_field;
    /*Get adaptation field*/

    adaf_field = (data[3]>>4)&0x3;
    pos+= 4;
    if(adaf_field == 0x02 || adaf_field == 0x03)
        pos+=1+data[pos]; //skip adaptation filed.

    pos+=1; //pointer field
    /*Table 2-25 -- Program association section*/
    /*skip 8 bytes till last_section_number (included)*/
    pos += 8;
    for(;;){
        program_number =0;
        program_number = data[pos];
        program_number<<=8;
        program_number|=data[pos+1];
        pos+=2;
	if(program_number == 0) {
	    pos+=2;
	} else {
            program_map_PID = data[pos]&0x1F;
            program_map_PID<<=8;
            program_map_PID|=data[pos+1];
            pos+=2;
            break;
        }
    }
#ifdef PRINT_MSG
    printf("PAT found\n");
#endif
    return program_map_PID;
}

static int8_t get_spts_pids(uint8_t*  data,  uint32_t len,
			    uint16_t* vpid,  uint16_t *apid,
			    uint16_t* apid2, uint16_t pmt_pid){

    int8_t ret = 0;
    uint32_t i, n_ts_pkts = 0;
    uint16_t pid;
    n_ts_pkts = len/188;
    for(i=0;i<n_ts_pkts;i++){
        pid = get_pkt_pid(data + i*188);
        if(pid == pmt_pid){
            get_av_pids_from_pmt(data+i*188,vpid,apid,apid2);
            ret = 1;
        }
    }

    return ret;

}

static void get_av_pids_from_pmt(uint8_t *data, uint16_t* vpid,
				 uint16_t *apid,  uint16_t* apid2){

    uint16_t pos = 0,program_info_length=0,pid, elementary_PID=0,
	ES_info_length=0;
    uint8_t adaf_field, num_aud = 0, stream_type;

    *vpid = *apid =0;
    /*Get adaptation field*/
    adaf_field = (data[3]>>4)&0x3;
    pos+= 4;
    if(adaf_field == 0x02 || adaf_field == 0x03)
        pos+=1+data[pos]; //skip adaptation filed.

    pos+=1; //pointer field
    /*Table 2-28 -- Transport Stream program map section*/
    /*skip 10 bytes till PCR_PID (included)*/
    pos += 10;
    /*skip the data descriptors*/
    program_info_length = data[pos]&0x0F;
    program_info_length<<=8;
    program_info_length|=data[pos+1];
    pos+=program_info_length;
    pos+=2;

    /*parse for PIDs*/
    for(;;){
        stream_type = data[pos++];
        elementary_PID = ES_info_length = 0;
        elementary_PID = data[pos]&0x1F;
        elementary_PID<<=8;
        elementary_PID |= data[pos+1];
        pos+=2;
        ES_info_length =  data[pos]&0xF;
        ES_info_length<<=8;
        ES_info_length|= data[pos+1];
        pos+=2;
        pos+= ES_info_length;
        switch(stream_type){
            case 0x0F:
            case 0x11:
                /*Audio: 0x0F for ISO/IEC 13818-7*/
                if(*apid == 0)
                    *apid = elementary_PID;
                else
                    *apid2 = elementary_PID;
                break;
            case 0x1B:
                /*Video: 0x1B for H.264*/
                *vpid = elementary_PID;
                break;
            default:
                break;
        }
        if(pos>=180|| (*vpid&&*apid))
            break;
    }
}

void cleanup_chunk_desc_contents(
    ts_mfu_profile_builder_t *bldr_ts_mfu,
    uint32_t chunk_num){
    
    uint32_t k = chunk_num;
    if(bldr_ts_mfu->chunk_desc[k] != NULL) {
	if(bldr_ts_mfu->chunk_desc[k]->sam_desc != NULL){
	    free(bldr_ts_mfu->chunk_desc[k]->sam_desc);
	    glob_free_chunk_desc++;
	    bldr_ts_mfu->chunk_desc[k]->sam_desc = NULL;
        }
        free(bldr_ts_mfu->chunk_desc[k]);
        bldr_ts_mfu->chunk_desc[k] = NULL;
        glob_free_sam_desc++;
    }	
}

void cleanup_chunk_desc(
	ts_mfu_builder_t *bldr,
	int32_t strm_id){
	
    uint32_t k = 0;
    if(bldr->profile_info[strm_id].chunk_desc != NULL){
        ts_mfu_profile_builder_t *bldr_ts_mfu = &bldr->profile_info[strm_id];
	for(k = 0; k < bldr->profile_info[strm_id].num_chunks; k++) {
	    cleanup_chunk_desc_contents(bldr_ts_mfu, k);	    
	}
	//printf("stream %d cleanup chunk desc\n", strm_id);
	free(bldr->profile_info[strm_id].chunk_desc);	
	bldr->profile_info[strm_id].chunk_desc = NULL;
    }
    return;
}

void 
cleanup_ts_mfu_builder(ts_mfu_builder_t *bldr)
{
    int32_t i = 0;
    uint32_t j = 0;
    uint32_t k = 0;
    A_(printf("cleanup-start %lu\n", nkn_memstat_type(mod_unknown)));
	
    if(bldr) {
	for(i=0; i<bldr->n_profiles; i++) {
#if 0
	    if(bldr->profile_info[i].data != NULL)
		free(bldr->profile_info[i].data);
#endif
	    if(bldr->profile_info[i].ts_name != NULL)
		free(bldr->profile_info[i].ts_name);
	    if(bldr->profile_info[i].io_desc_ts != NULL)
	      bldr->profile_info[i].iocb->ioh_close(bldr->profile_info[i].io_desc_ts);
	    if(bldr->profile_info[i].accum) {
		void *ptr = NULL;
		/* need to free the active tasks spearately but only after
		 * nkn_mon_delete is called for the same variable
		 */
		if (bldr->profile_info[i].accum->stats.apple_fmtr_num_active_tasks)
		    ptr = bldr->profile_info[i].accum->stats.apple_fmtr_num_active_tasks;
		deleteAccumulator(bldr->profile_info[i].accum);
		free(ptr);
		A_(printf("cleanup-accum %d %lu\n", i, nkn_memstat_type(mod_unknown)));
	    }

	    cleanup_chunk_desc(bldr, i);
	    
	    if (bldr->profile_info[i].seg_ctx) {
		cleanup_ts_segmenter(bldr->profile_info[i].seg_ctx);
	    }
	}
	free(bldr);
	A_(printf("cleanup end %lu\n", nkn_memstat_type(mod_unknown)));
    }

}

