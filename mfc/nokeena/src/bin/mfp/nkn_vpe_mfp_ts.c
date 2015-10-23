//#include "nkn_vpe_mfp_ts.h"
#include "nkn_vpe_bitio.h"
#include <stdlib.h>
#include <string.h>
#include <aio.h>

#include "nkn_vpe_mp4_parser.h"
#include "nkn_vpe_mp4_seek_api.h"
#include "nkn_vpe_utils.h"
#include "nkn_vpe_ts_video.h"
#include "nkn_memalloc.h"

#include "mfp_err.h"

#include "mfp_ref_count_mem.h"
#include "mfp_apple_io_hdlr.h"
#include "io_hdlr/mfp_io_hdlr.h"

#include "nkn_debug.h"

uint32_t glob_use_async_io_apple_fmtr = 1;
extern uint32_t glob_latm_audio_encapsulation_enabled;

io_handler_t* io_hdlr = NULL; 
io_complete_hdlr_fptr io_end_hdlr = NULL; 
//base timestamp used for output ts streams
uint64_t glob_output_ts_strms_base_time = 1;

io_handlers_t memio = {
    .ioh_open = nkn_vpe_memopen,
    .ioh_read = nkn_vpe_memread,
    .ioh_tell = nkn_vpe_memtell,
    .ioh_seek = nkn_vpe_memseek,
    .ioh_close = nkn_vpe_memclose,
    .ioh_write = nkn_vpe_memwrite
};

static io_handlers_t fileio = {
    .ioh_open = nkn_vpe_fopen,
    .ioh_read = nkn_vpe_fread,
    .ioh_tell = nkn_vpe_ftell,
    .ioh_seek = nkn_vpe_fseek,
    .ioh_close = nkn_vpe_fclose,
    .ioh_write = nkn_vpe_fwrite
};

static io_handlers_t pio = {
    .ioh_open = pio_open,
    .ioh_read = pio_read,
    .ioh_tell = pio_tell,
    .ioh_seek = pio_seek,
    .ioh_close = pio_close,
    .ioh_write = pio_write
};

static io_handlers_t sio = {
    .ioh_open = sio_fopen,
    .ioh_read = sio_fread,
    .ioh_tell = sio_ftell,
    .ioh_seek = sio_fseek,
    .ioh_close = sio_fclose,
    .ioh_write = sio_fwrite
};
    
//static function declarations
//#define PAT_PMT

static inline void mfu_form_ts_hdr(uint16_t, stream_t*);
static inline int32_t mfu_get_aac(unit_desc_t* , uint32_t,
	uint32_t,ts_pkt_list_t*,stream_t*);
static inline void mfu_form_aud_pes_hdr(pes_fields_t *, uint8_t*);
static inline uint32_t mfu_get_unit_num(unit_desc_t*,
	uint32_t,uint32_t);
static inline int32_t mfu_get_time_sync_info(uint32_t,uint32_t,
	uint32_t*, uint32_t *);
static inline uint32_t mfu_write_pat(uint8_t* ,pat_t*);
static inline int32_t mfu_form_pmt_pkt(pmt_t*,ts_desc_t*);
static inline int32_t mfu_form_pat_pkt(pat_t*);
static inline void mfu_write_ts_hdr(ts_pkt_t ,uint8_t*);

/*======================================================================================
Input: 
The stream types are to be filled before hand;
Audio: 0x0F for ISO/IEC 13818-7 Audio with ADTS transport syntax
Video: 0x1B for AVC video stream as defined in ITU-T Rec. H.264 | ISO/IEC 14496-10 Video
The following ts components are to be filled on entry:
uint8_t num_streams; // number of streams
uint8_t num_aud_streams;
uint8_t num_vid_streams;
stream_t stream_aud[16]; // Each stream's info
stream_t stream_vid[16]; // Each stream's info
uint8_t  stream_type;// 8 uimsbf
uint8_t cc;
stream_t* st;
uint32_t chunk_num;
Output: 
Does all init stream allocation for the ts stream. Forms PAT, PMT and required headers.
=======================================================================================*/
int32_t mfu2ts_init(ts_desc_t* ts){
    int32_t i = 0;
    stream_t* stt = NULL;
    ts->chunk_num = 1;
    ts->pat_data = (uint8_t*)
	nkn_malloc_type(sizeof(uint8_t)*TS_PKT_SIZE,
		mod_vpe_pat_data_buf);
    ts->pmt_data = (uint8_t*)
	nkn_malloc_type(sizeof(uint8_t)*TS_PKT_SIZE,
		mod_vpe_pmt_data_buf);
    memset(ts->pat_data,0xFF,TS_PKT_SIZE);
    memset(ts->pmt_data,0xFF,TS_PKT_SIZE);
    ts->pat = (pat_t*) nkn_calloc_type(1, sizeof(pat_t),
	    mod_vpe_pat_t);
    ts->pmt = (pmt_t*) nkn_calloc_type(1, sizeof(pmt_t),
	    mod_vpe_pmt_t);
    /* form PAT header and writting it */
    mfu_form_pat_pkt(ts->pat);
    mfu_write_pat(ts->pat_data,ts->pat);

    ts->pcr_pid = VPE_VID_PID;
    ts->num_streams = ts->num_vid_streams+ts->num_aud_streams;
    ts->st = (stream_t*)
	nkn_malloc_type(sizeof(stream_t)*ts->num_streams,
		mod_vpe_stream_t);
    stt = ts->st;
    for(i=0;i<ts->num_aud_streams;i++){
	if(ts->stream_aud[i].stream_type == TS_AAC_ADTS_ST){
	    ts->stream_aud[i].elementary_PID = VPE_AUD_PID;
	    /*
	     * Table 2-18: MPEG2 TS:ISO/IEC 13818-3 or ISO/IEC
	     * 11172-3 or ISO/IEC 13818-7 or//ISO/IEC 14496-3 audio
	     * stream number x xxxx 
	     */
	    ts->stream_aud[i].stream_id = 0xC0;
	    ts->stream_aud[i].stream_id|=i; 
	    ts->stream_aud[i].frame_rate = 0;
	    ts->stream_aud[i].frame_time = 1;
	    ts->stream_aud[i].aud_temp_size=0;
	    if(i>0x1)
		return VPE_ERROR;
	}
	ts->stream_aud[i].ES_info_length = 0;
	ts->stream_aud[i].base_time = glob_output_ts_strms_base_time;
	memcpy(stt++,&ts->stream_aud[i],sizeof(stream_t));
	mfu_form_ts_hdr(ts->stream_aud[i].elementary_PID,&ts->stream_aud[i]);
    }
    for(i=0;i<ts->num_vid_streams;i++){
	if(ts->stream_vid[i].stream_type == TS_H264_ST){
	    ts->stream_vid[i].elementary_PID = VPE_VID_PID;
	    /*	    
	     * Table 2-18: ITU-T Rec. H.262 | ISO/IEC 13818-2, ISO/IEC
	     * 11172-2, ISO/IEC 14496-2 or ITU-T Rec. H.264 | ISO/IEC
	     * 14496-10 video stream   number xxxx
	     */
	    ts->stream_vid[i].stream_id = 0xE0;
	    ts->stream_vid[i].stream_id|=i;
	    ts->stream_vid[i].vid_temp_size=0;
	    if(i>0x1)
		return VPE_ERROR;
	    ts->base_vid_time = glob_output_ts_strms_base_time;//0xFFFFFFFF;
	}
	ts->stream_vid[i].ES_info_length = 0;
	memcpy(stt++,&ts->stream_vid[i],sizeof(stream_t));
	mfu_form_ts_hdr(ts->stream_vid[i].elementary_PID,&ts->stream_vid[i]);
    }
    /* form PMT header */
    mfu_form_pmt_pkt(ts->pmt,ts);

    ts->frame_rate = 0;
    ts->frame_time = 0;
    ts->time_of_day = 0;
    ts->avg_bitrate = 0;
    ts->max_bitrate = 0;
    ts->standard_deviation = 0;
    ts->sum_of_deviation_square = 0;
    ts->num_samples = 0;
    return VPE_SUCCESS;
}


    static inline void 
mfu_form_ts_hdr(uint16_t pid, stream_t *st)
{
    ts_pkt_t *pkt = NULL;
    pkt = (ts_pkt_t*) nkn_malloc_type(sizeof(ts_pkt_t),
	    mod_vpe_ts_pkt_t);
    pkt->sync_byte = 0x47;
    pkt->transport_error_indicator = 0;
    pkt->transport_priority = 0;
    pkt->PID = pid;
    pkt->transport_scrambling_control = 0;
    pkt->continuity_counter = 0;
    pkt->adaptation_field_control = 1; 
    mfu_write_ts_hdr(*pkt,st->header_no_af);
    pkt->adaptation_field_control = 3;
    mfu_write_ts_hdr(*pkt,st->header_af);
    free(pkt);
}

int32_t 
mfu_clean_ts(ts_desc_t* ts){
    if(ts->pat_data != NULL)
	free(ts->pat_data);
    if(ts->pmt_data != NULL)
	free(ts->pmt_data);
    if(ts->pat != NULL)
	free(ts->pat);
    if(ts->pmt != NULL)
	free(ts->pmt);
    if(ts->st != NULL)
	    free(ts->st);
    
    return VPE_SUCCESS;
}


#define TS_HEADER_OVERHEAD 25600 /*bytes @ 200kkbps*/
//#define CHECK
int32_t 
mfu2_ts(ts_desc_t *ts,
	unit_desc_t* vid, 
	unit_desc_t* aud, 
	uint8_t	*sps,
	uint32_t sps_size,
	char* chunk_fname,
	size_t mfu_size,
	uint32_t encr_flag,
	const mfp_safe_io_ctx_t *sioc,
	bitstream_state_t **out)
{
    uint32_t naud = 0, nvid = 0,vid_unit_num=0,aud_unit_num = 0;//len = 0;
    int32_t na_done = 0,nv_done = 0,rem_aud = 0,rem_vid = 0,i,err = 0;
    uint32_t t=0;
    ts_pkt_list_t *alist = NULL, *vlist = NULL;
    uint32_t num_aud_units = 0,num_vid_units = 0;
    stream_t *aud_st = NULL,*vid_st = NULL;
    //    char fname[100] = {'\0'};
    void* fid = NULL;;
    mfu_videots_desc_t video_desc;
    uint8_t cc = 0;
    io_handlers_t *l_ioh = NULL;
    ref_count_mem_t* ref_cont = NULL;
	file_handle_t* fh = NULL;
	uint32_t file_offset = 0;
    size_t est_size = 0;
    char *f1 = NULL;

#ifdef TS_FILE
    FILE* fout;
    fout = fopen("ts_file.ts","wb");
    static int32_t vid_pkts =0, aud_pkts=0;
#endif

    video_desc.sps_data = sps;
    video_desc.sps_size = sps_size;
    video_desc.Is_sps_dumped = 0;
    video_desc.num_TS_pkts = 0;
    video_desc.base_time = ts->base_vid_time;
    video_desc.time_of_day = ts->time_of_day;
    video_desc.frame_time = ts->frame_time;
    video_desc.continuity_counter = ts->continuity_counter;

    /*Mallocs*/
    alist = (ts_pkt_list_t*) nkn_calloc_type(1,
					     sizeof(ts_pkt_list_t),
					     mod_vpe_aud_ts_pkt_list_t);
    vlist = (ts_pkt_list_t*) nkn_calloc_type(1,
					     sizeof(ts_pkt_list_t),
					     mod_vpe_vid_ts_pkt_list_t);
    if (encr_flag)
	*out = NULL;

    /*Set Time*/
    t = ts->chunk_time*1000; // t 'ms' chunks
    /*With this info get the number of video/audio samples to send in
      1 burst*/    
    if (aud !=NULL && vid !=NULL) {
	mfu_get_time_sync_info(vid->sample_count, aud->sample_count,
			       &naud, &nvid);
    } else {
	naud = 1;
	nvid = 1;
    }
    if (aud) {
	aud_st = &ts->stream_aud[0];
	na_done = aud->sample_count;
    }
    if (vid) {
	vid_st = &ts->stream_vid[0];
	nv_done = vid->sample_count;
    }
    /*Get number of units := 1 time slot*/
    if (na_done) {
	num_aud_units = na_done;
	rem_aud= num_aud_units;
	aud_st->frame_rate = (double)(t/1000)/num_aud_units;
    }
    if (nv_done) {
	num_vid_units = nv_done;
	rem_vid = num_vid_units;
	video_desc.frame_rate = (double)(t/1000)/num_vid_units;
    }

    /**
     * the following decision point is to decide whether to use a
     * memory stream or write to a file stream. we require a memory
     * stream if we need to encrypt the data.
     */
    if (encr_flag) {
	l_ioh = &memio;
	/* calculate estimated size as follows a sum of the mfu size
	 * and 200kbps overhead for transport headers. the underlying
	 * memio routines are capable of resizing (which is expensive)
	 * hence we are trying to make the best possible guess
	 */
	est_size = mfu_size + (TS_HEADER_OVERHEAD * ts->chunk_time);
	f1 = (char *)nkn_malloc_type(est_size, mod_vpe_vdat_buf);
    } else {
	if (sioc) {
	    est_size = mfu_size + (TS_HEADER_OVERHEAD * ts->chunk_time);
	    l_ioh = &sio;
	}
	f1 = chunk_fname;
    }

    if (l_ioh != NULL) {
	fid = l_ioh->ioh_open(f1, "wb", est_size);
	if (!fid) {
	    err = -E_MFP_NO_SPACE;
	    goto error;
	}
    } else {
	if (!io_hdlr) {
	    if(glob_use_async_io_apple_fmtr){
		io_hdlr = createAIO_Hdlr();
		io_end_hdlr = AppleAIO_CompleteHdlr; 
	    }else{
		io_hdlr = createIO_Hdlr();
		io_end_hdlr = AppleIO_CompleteHdlr; 
	    }	
	}	
	io_open_ctx_t open_ctx;
	open_ctx.path = (uint8_t*)f1;
	open_ctx.flags = O_CREAT | O_WRONLY | O_TRUNC | O_ASYNC;
	open_ctx.mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
	fh = io_hdlr->io_open(&open_ctx);
	if (fh == NULL) {
	    err = -E_MFP_NO_SPACE;
	    goto error;
	}
	if(glob_use_async_io_apple_fmtr){
	ref_cont = createRefCountedContainer(fh, deleteAIO_RefCtx);
	}else{
	ref_cont = createRefCountedContainer(fh, deleteIO_RefCtx);
	}
	if (ref_cont == NULL) {
	    err = -E_MFP_NO_SPACE;
	    goto error;
	}
	ref_cont->hold_ref_cont(ref_cont);
    }

    if (sioc) {
	mfp_safe_io_attach((mfp_safe_io_desc_t *)fid,
			   (mfp_safe_io_ctx_t *)sioc);
	err = mfp_safe_io_reserve_size((mfp_safe_io_desc_t *)fid,
				       est_size);
	if (err) {
	    goto error;
	}
    }

    /*Write PAT and PMT*/
    if (l_ioh != NULL) {
	l_ioh->ioh_write(ts->pat_data, TS_PKT_SIZE, 1, fid);
	l_ioh->ioh_write(ts->pmt_data, TS_PKT_SIZE, 1, fid);
    } else {
	char* pat_data = nkn_malloc_type(TS_PKT_SIZE, mod_mfp_apple_io);
	memcpy(pat_data, ts->pat_data, TS_PKT_SIZE);
	char* pmt_data = nkn_malloc_type(TS_PKT_SIZE, mod_mfp_apple_io);
	memcpy(pmt_data, ts->pmt_data, TS_PKT_SIZE);
	
	Apple_WriteNewFile(io_hdlr, io_end_hdlr, fh, pat_data, TS_PKT_SIZE,
			file_offset, ref_cont);
	file_offset += TS_PKT_SIZE;
	Apple_WriteNewFile(io_hdlr, io_end_hdlr, fh, pmt_data, TS_PKT_SIZE,
			file_offset, ref_cont);
	file_offset += TS_PKT_SIZE;
    }

#ifdef TS_FILE
    fwrite(ts->pat_data, TS_PKT_SIZE, 1, fout);
    fwrite(ts->pmt_data, TS_PKT_SIZE, 1, fout);
#endif
    /*Change PAT pmt cc counter for next iteration*/
    cc = ts->pat_data[3]&0xf;
    cc = (cc+1)&0x0F;
    ts->pat_data[3] = (ts->pat_data[3]&0xf0)|cc;
    cc = ts->pmt_data[3]&0xf;
    cc = (cc+1)&0x0F;
    ts->pmt_data[3] = (ts->pmt_data[3]&0xf0)|cc;
    /*Mux them in the order of naud:nvid*/
    while(rem_aud>0 || rem_vid>0){
	/*writting audio pkts */
	if (na_done>0) {
	    alist->entry_num_pkts = (uint32_t*)
		nkn_malloc_type(4*naud,
			mod_vpe_aud_ts_pkt_buf);
	    if (na_done>=(int32_t)naud) {
		for(i=0;i<(int32_t)naud;i++){
		    mfu_get_aac(aud, aud_unit_num+i, 1, alist,
			    aud_st);
			if (l_ioh != NULL)
		    	l_ioh->ioh_write(alist->data,
						alist->num_entries*TS_PKT_SIZE, 1, fid);
			else {
				Apple_WriteNewFile(io_hdlr, io_end_hdlr, fh, (char*)alist->data,
						alist->num_entries * TS_PKT_SIZE, file_offset,
						ref_cont);
				file_offset += alist->num_entries * TS_PKT_SIZE;
			}
#ifdef TS_FILE
		    fwrite(alist->data,
			   alist->num_entries*TS_PKT_SIZE, 1, fout);
		    aud_pkts += alist->num_entries;
#endif
			if (l_ioh != NULL)
				free(alist->data);
		 }
		 na_done-=naud;
		 rem_aud-=naud;
		 aud_unit_num+=naud;
	     } else {
		 for(i=0;i<na_done;i++){
		     mfu_get_aac(aud, aud_unit_num+i, 1, alist,
				 aud_st);
			 if (l_ioh != NULL)
			     l_ioh->ioh_write(alist->data,
						 alist->num_entries*TS_PKT_SIZE, 1, fid);
			 else {
				Apple_WriteNewFile(io_hdlr, io_end_hdlr, fh, (char*)alist->data,
						alist->num_entries * TS_PKT_SIZE, file_offset,
						ref_cont);
				file_offset += alist->num_entries * TS_PKT_SIZE;
			 }
#ifdef TS_FILE
		    fwrite(alist->data,
			    alist->num_entries*TS_PKT_SIZE, 1, fout);
		    aud_pkts += alist->num_entries;
#endif
			if (l_ioh != NULL)
				free(alist->data);
		 }
		 na_done=0;
		 rem_aud=0;
		 aud_unit_num+=na_done;
	     }

	     free(alist->entry_num_pkts);
	 }
	 /* writting video pkts */
	 if (nv_done) {
	     if (nv_done>=(int32_t)nvid) {
		 for (i=0;i<(int32_t)nvid;i++) {
		     mfu_get_ts_h264_pkts((mdat_desc_t*)vid, vlist,
					  &video_desc, 1, vid_unit_num+1+i);
		     ts->base_vid_time = video_desc.base_time;
		    /*Write PAT and PMT*/
#ifdef PAT_PMT
		    l_ioh->ioh_write(ts->pat_data, TS_PKT_SIZE, 1, fid);
		    l_ioh->ioh_write(ts->pmt_data, TS_PKT_SIZE, 1, fid);
#ifdef TS_FILE
		    l_ioh->ioh_write(ts->pat_data, TS_PKT_SIZE, 1, fout);
		    l_ioh->ioh_write(ts->pmt_data, TS_PKT_SIZE, 1, fout);
#endif
		    /*Change PAT pmt cc counter for next iteration*/
		    cc = ts->pat_data[3]&0xf;
		    cc = (cc+1)&0x0F;
		    ts->pat_data[3] = (ts->pat_data[3]&0xf0)|cc;
		    cc = ts->pmt_data[3]&0xf;
		    cc = (cc+1)&0x0F;
		    ts->pmt_data[3] = (ts->pmt_data[3]&0xf0)|cc;
#endif
			if (l_ioh != NULL)
			    l_ioh->ioh_write(vlist->data,
						vlist->num_entries*TS_PKT_SIZE, 1, fid);
			else {
				Apple_WriteNewFile(io_hdlr, io_end_hdlr, fh, (char*)vlist->data,
						vlist->num_entries * TS_PKT_SIZE, file_offset,
						ref_cont);
				file_offset += vlist->num_entries * TS_PKT_SIZE;
			}
#ifdef TS_FILE
		    fwrite(vlist->data,
			    vlist->num_entries*TS_PKT_SIZE, 1, fout);
		    vid_pkts += vlist->num_entries;
#endif
			if (l_ioh != NULL)
				free(vlist->data);
		 }
		 nv_done -= nvid;
		 rem_vid -= nvid;
		 vid_unit_num += nvid;
	     } else {
		 for (i=0;i<nv_done;i++) {
		     mfu_get_ts_h264_pkts((mdat_desc_t*)vid, vlist,
					  &video_desc, 1, vid_unit_num+1+i);
		     ts->base_vid_time = video_desc.base_time;
	     
			 if (l_ioh != NULL)
				 l_ioh->ioh_write(vlist->data,
						 vlist->num_entries*TS_PKT_SIZE, 1, fid);
			 else {
				Apple_WriteNewFile(io_hdlr, io_end_hdlr, fh, (char*)vlist->data,
						vlist->num_entries * TS_PKT_SIZE, file_offset,
						ref_cont);
				file_offset += vlist->num_entries * TS_PKT_SIZE;
			 }

#ifdef TS_FILE
		    fwrite(vlist->data,
			    vlist->num_entries*TS_PKT_SIZE, 1, fout);
		    vid_pkts += vlist->num_entries;
#endif
			if (l_ioh != NULL)
				free(vlist->data);
		 }
		 nv_done = 0;
		 rem_vid = 0;
		 vid_unit_num += nv_done;
	     }
	 }
    }
	if (l_ioh != NULL)
		ts->chunk_size = l_ioh->ioh_tell(fid);
	else
		ts->chunk_size = file_offset;

    /* dont close the memory stream here; the memory stream will be
     * written only after encryption
     */
    if (encr_flag) {
	*out = (bitstream_state_t *)fid;
    } else {
		if (l_ioh != NULL)
			l_ioh->ioh_close(fid);
		else
			ref_cont->release_ref_cont(ref_cont);
    }

#ifdef TS_FILE
    fclose(fout);
#endif
    ts->chunk_num++;
    if(alist != NULL)
	free(alist);
    if(vlist != NULL)
	free(vlist);
    ts->time_of_day = video_desc.time_of_day;
    ts->frame_time = video_desc.frame_time;
    ts->continuity_counter = video_desc.continuity_counter;
    return VPE_SUCCESS;

 error:
    if (alist) free(alist);
    if (vlist) free(vlist);
    if (fid && l_ioh) l_ioh->ioh_close(fid);

    return err;

}

static inline int32_t 
mfu_get_aac(unit_desc_t* aud, uint32_t st_unum, uint32_t num_units,
	ts_pkt_list_t* pktlist, stream_t*st_aud)
{
    int32_t temp =0, no_skip = 0, pkt_data_left =0, adaf_flag = 0;
    int32_t *residual_size = NULL;
    uint32_t total_pld_size= 0, *num_pkts ,tsize = 0;
    uint32_t total_num_pkts=0, j =0, rem_pes_data = 0, rem_adaf_bytes=0,i;
    uint8_t *data = NULL, adaptation_flen = 0, *pes_data = NULL, stuff_split[10];
    pes_fields_t *pes = NULL;


    if(((aud->mdat_pos[0] !=0xFF) ||((aud->mdat_pos[1]&0xF0) !=0xF0)))
	assert(0);

    //uint8_t *temp_aud = aud->mdat_pos;
    residual_size = (int32_t*) nkn_malloc_type(4*num_units,
	    mod_vpe_residual_size_aud);
    pktlist->num_entries = num_units;
    num_pkts = pktlist->entry_num_pkts;

    /*Calculate number of packets required*/  
    for(i=st_unum;i<st_unum+num_units;i++){
	stuff_split[temp]=0;
	//Payload + PES + adaptation field
	tsize = aud->sample_sizes[i]+AAC_PES_SIZE+AAC_ADAF; 
	//ceil(tsize/178)
	num_pkts[temp] = (tsize/(TS_PKT_SIZE-4))+1;
	//tsize%(TS_PKT_SIZE-4);
	residual_size[temp] = (num_pkts[temp]*TS_PKT_SIZE) -
	    (tsize+4*num_pkts[temp]);

	if (num_pkts[temp]>1) {
	    if (residual_size[temp] > ADAF_STUFF_MAX+AAC_ADAF)
		residual_size[temp] -= AAC_ADAF;
	    else if ((residual_size[temp] > ADAF_STUFF_MAX) &&
		    (residual_size[temp] <= ADAF_STUFF_MAX+AAC_ADAF) )
		stuff_split[temp] = 1;
	}
	total_num_pkts += num_pkts[temp];
	temp++;
	total_pld_size += total_num_pkts*TS_PKT_SIZE;
    }
    pktlist->data = (uint8_t*) nkn_malloc_type(total_pld_size,
	    mod_vpe_pktlist_data_buf);
    pktlist->num_entries = total_num_pkts;
    data = pktlist->data;

    /*Form Standard PES structure*/
    pes = (pes_fields_t*) nkn_calloc_type(1, sizeof(pes_fields_t),
	    mod_vpe_pes_fields_t);
    pes->PTS_DTS_flags = 2;
    pes->data_alignment_indicator = 1;
    pes->packet_stat_code_prefix = 0x000001;
    pes->stream_id = st_aud->stream_id;
    pes->PES_header_data_length = PES_HR_LEN;
    temp = 0;
    for (i=st_unum;i<st_unum+num_units;i++) {
	j=0;
	pkt_data_left = TS_PKT_SIZE;
	/*Handle the first packet of every AU separately*/
	/*Form a TS header*/
	memcpy(data, st_aud->header_af, 4);
	data[1] |= 0x40;
	data[3] = (data[3]&0xf0)|st_aud->cc;
	st_aud->cc = (st_aud->cc+1)&0x0F;
	data += 4;
	pkt_data_left -= 4;

	/*Determine AF size*/ 
	if(residual_size[temp]>=0){
	    /*For aac audio we will use adaptation fields ONLY for
	      stuffing*/
	    adaptation_flen = residual_size[temp];
	    if ((adaptation_flen<=ADAF_STUFF_MAX) ||
		    (num_pkts[temp]==1) || (stuff_split[temp]==1)) 
		adaf_flag = 0;
	    else {
		/*Dump 150 stuffing  bytes- dump the rest in the last
		  packet*/
		rem_adaf_bytes = adaptation_flen-ADAF_STUFF_MAX;
		adaptation_flen = ADAF_STUFF_MAX;
		adaf_flag = 1;
	    }
	    *data++=adaptation_flen+1;
	    *data++ = 0;   
	    memset(data,0xFF,adaptation_flen);
	    data+=adaptation_flen;
	    pkt_data_left-=adaptation_flen+AAC_ADAF;
	}
	/*Write PES fields*/ 
	//Fields following pes_packet_length in PES_pkt
	pes->PES_packet_length = aud->sample_sizes[i] +
	    AAC_PES_SIZE-6;
#if 0
	pes->PTS = (uint64_t)
	    (((double)(st_aud->base_time)/aud->timescale) * 90000);
#else
	pes->PTS = (uint64_t)
            (((double)(st_aud->base_time*90000)/aud->timescale));
#endif
	//	pes->PTS %= POW2_32;
	pes->PTS &= 0xFFFFFFFF;
	//	printf("Audio PTS calculated in Formatter = %lu\n", pes->PTS);
	st_aud->base_time += aud->sample_duration[i];
	/*Write the 1st packet*/
	if (pkt_data_left < AAC_PES_SIZE) {
	    /*PES header is to be split and written*/
	    /* This will not be hit if adaf_flag = 1*/
	    pes_data = (uint8_t*) nkn_malloc_type(AAC_PES_SIZE,
		    mod_vpe_pes_data);
	    mfu_form_aud_pes_hdr(pes, pes_data);
	    rem_pes_data = AAC_PES_SIZE-pkt_data_left;
	    memcpy(data, pes_data, pkt_data_left);
	    pes_data += pkt_data_left;
	    data += pkt_data_left;
	    /*Dump remaining PES header into next packet*/ 
	    pkt_data_left = TS_PKT_SIZE;
	    no_skip = 0;
	    /*dump header of TS*/
	    memcpy(data, st_aud->header_no_af, 4);
	    data[1] &= 0xBF;
	    data[3] = (data[3]&0xf0)|st_aud->cc;
	    data += 4;
	    pkt_data_left -= 4;
	    st_aud->cc = (st_aud->cc+1)&0x0F;
	    /*Copy remaining PES header data*/
	    memcpy(data, pes_data, rem_pes_data);
	    free(pes_data+rem_pes_data-AAC_PES_SIZE);
	    pes_data = NULL;
	    data += rem_pes_data;
	    pkt_data_left -= rem_pes_data;
	} else {
	    pes_data = data;
	    mfu_form_aud_pes_hdr(pes, pes_data);
	    data += AAC_PES_SIZE;
	    no_skip = 1;
	    pkt_data_left -= AAC_PES_SIZE;
	    if (pkt_data_left>0) {
		memcpy(data, aud->mdat_pos, pkt_data_left);
		aud->mdat_pos += pkt_data_left;
		data += pkt_data_left;
#ifdef APPLE
		audio_data_written += pkt_data_left;
#endif
	    }
	    pkt_data_left = TS_PKT_SIZE;
	}
	/*Handle the remaining packets*/
	for (j=1;j<num_pkts[temp];j++) {
	    if (no_skip) {
		if (adaf_flag) {
		    adaf_flag = 0;
		    memcpy(data, st_aud->header_af, 4);
		    data[1] &= 0xBF;
		    data[3] = (data[3]&0xf0)|st_aud->cc;
		    data += 4;
		    pkt_data_left -= 4;
		    adaptation_flen = rem_adaf_bytes;
		    *data++=adaptation_flen+1;
		    *data++ = 0;
		    memset(data, 0xFF, adaptation_flen);
		    data += adaptation_flen;
		    pkt_data_left -= adaptation_flen+AAC_ADAF;
		}
		else{
		    memcpy(data, st_aud->header_no_af, 4);
		    data[1] &= 0xBF;
		    data[3] = (data[3]&0xf0)|st_aud->cc;
		    data += 4;
		    pkt_data_left -= 4;
		}
		st_aud->cc = (st_aud->cc+1)&0x0F;
	    }
	    else
		no_skip = 1;
	    memcpy(data, aud->mdat_pos, pkt_data_left);
	    aud->mdat_pos += pkt_data_left;
	    data += pkt_data_left;
	    pkt_data_left = TS_PKT_SIZE;
	}
	temp++;
    }
    //    aud->mdat_pos = temp_aud;
    free(residual_size);
    free(pes);
    return VPE_SUCCESS;
}


    static inline void 
mfu_form_aud_pes_hdr(pes_fields_t *pes, uint8_t* data)
{
    bitstream_state_t *bs;
    data[0] = data[1] = 0;
    data[2] = 1;
    data[3] = pes->stream_id;
    data[4] = pes->PES_packet_length>>8;
    data[5] = pes->PES_packet_length&0xFF;
    bs = bio_init_bitstream(&data[6], AAC_PES_SIZE-6);
    bio_write_int(bs, 2, 2);
    bio_write_int(bs, pes->PES_scrambling_control, 2);
    bio_write_int(bs, pes->PES_priority, 1);
    bio_write_int(bs, pes->data_alignment_indicator, 1);
    bio_write_int(bs, pes->copyright, 1);
    bio_write_int(bs, pes->original_or_copy, 1);
    bio_write_int(bs, pes->PTS_DTS_flags, 2);
    bio_write_int(bs, pes->ESCR_flag, 1);
    bio_write_int(bs, pes->ES_rate_flag, 1);
    bio_write_int(bs, pes->DSM_trick_mode_flag, 1);
    bio_write_int(bs, pes->additional_copy_info_flag, 1);
    bio_write_int(bs, pes->PES_CRC_flag, 1);
    bio_write_int(bs, pes->PES_extension_flag, 1);
    bio_write_int(bs, pes->PES_header_data_length, 8);
    if (pes->PTS_DTS_flags == 2) {
	bio_write_int(bs, 2, 4);
	bio_write_int(bs, pes->PTS>>30, 3);
	bio_write_int(bs, 1, 1);
	bio_write_int(bs, (pes->PTS>>15)&0x7FFF, 15);
	bio_write_int(bs, 1, 1);
	bio_write_int(bs, pes->PTS&0x7FFF, 15);
	bio_write_int(bs, 1, 1);
    }
    bio_close_bitstream(bs);
}


    static inline uint32_t 
mfu_get_unit_num(unit_desc_t* unit, uint32_t stnum, uint32_t ts)
{
    uint32_t i = 0;
    uint32_t time_temp = 0,j=0;
#ifdef TIME_SCALE
    ts/=90000;
#else
    ts*=unit->timescale/1000;
#endif
    for(i=stnum;i<unit->sample_count;i++){
	time_temp+=unit->sample_duration[i];
	if(time_temp>=ts)
	    return j+1;
	j++;
    }
    return unit->sample_count;
}


    static inline int32_t 
mfu_get_time_sync_info(uint32_t vsamp, uint32_t asamp, uint32_t*na,
	uint32_t *nv)
{
    if (asamp>vsamp) {
	*nv = 1;
	*na = asamp/vsamp;
    } else {
	*nv = vsamp/asamp;
	*na =1;
    }
    return VPE_SUCCESS;
}


    static inline int32_t 
mfu_form_pat_pkt(pat_t *pat)
{
    /*Write the TS header*/
    pat->pat_hr.sync_byte = 0x47;
    pat->pat_hr.transport_error_indicator = 0;
    pat->pat_hr.payload_unit_start_indicator = 1;
    pat->pat_hr.transport_priority = 0;
    pat->pat_hr.PID = VPE_PAT_PID;
    pat->pat_hr.transport_scrambling_control = 0;
    pat->pat_hr.adaptation_field_control = 1;
    pat->pat_hr.continuity_counter = 0;
    pat->pat_hr.af = NULL;

    /*Write the Program Association Section*/
    pat->pas.table_id = 0;
    pat->pas.section_syntax_indicator =1;
    //Can be defined as anything by the user.
    pat->pas.transport_stream_id = 1; 
    pat->pas.version_number = pat->pas.section_number =
	pat->pas.last_section_number = 0;
    pat->pas.current_next_indicator = 1;
    pat->program_map_PID = VPE_PMT_PID;
    pat->pas.section_length = 13; //needs to be calculated.

    return VPE_SUCCESS;
}



    static inline uint32_t 
mfu_write_pat(uint8_t* data, pat_t* pat)
{
    int32_t pos = 0;
    bitstream_state_t *bs;

    /*Write TS header*/
    mfu_write_ts_hdr(pat->pat_hr, data);
    data += 4;
    /*Write pointer access*/
    data[pos++] = 0x00;
    /*Write PAT header*/
    bs = bio_init_bitstream(&data[pos], 16);
    bio_write_int(bs,pat->pas.table_id, 8);
    bio_write_int(bs, pat->pas.section_syntax_indicator, 1);
    bio_write_int(bs, 0, 1);
    bio_write_int(bs, 3, 2);
    bio_write_int(bs, pat->pas.section_length, 12);
    bio_write_int(bs, pat->pas.transport_stream_id, 16);
    bio_write_int(bs, 3, 2);
    bio_write_int(bs, pat->pas.version_number, 5);
    bio_write_int(bs, pat->pas.current_next_indicator, 1); 
    bio_write_int(bs, pat->pas.section_number, 8);
    bio_write_int(bs, pat->pas.last_section_number, 8);

    pos+=8;
    /*Should be in a for loop NOT FOR iPhone*/
    bio_write_int(bs,1,16);    //program_number always  = 1
    bio_write_int(bs,7,3); //rsvd
    bio_write_int(bs,pat->program_map_PID,13);
    bio_write_int(bs,0xe8f95e7d,32);
    bio_close_bitstream(bs);
    pos+=8;

    return VPE_ERROR; 


}


    static inline void 
mfu_write_ts_hdr(ts_pkt_t hr ,uint8_t*data)
{
    bitstream_state_t *bs;
    bs = bio_init_bitstream(data,4);
    bio_write_int(bs,hr.sync_byte,8);
    bio_write_int(bs,hr.transport_error_indicator,1);
    bio_write_int(bs,hr.payload_unit_start_indicator,1);
    bio_write_int(bs,hr.transport_priority,1);
    bio_write_int(bs,hr.PID,13);
    bio_write_int(bs,hr.transport_scrambling_control,2);
    bio_write_int(bs,hr.adaptation_field_control,2);
    bio_write_int(bs,hr.continuity_counter,4);
    bio_close_bitstream(bs);
}


    static inline int32_t 
mfu_form_pmt_pkt(pmt_t *pmt, ts_desc_t* ts)
{
    bitstream_state_t *bs;
    stream_t *st;
    int32_t i=0;
    pmt = (pmt_t*) nkn_calloc_type(1, sizeof(pmt_t), mod_vpe_pmt_t);
    /*Write the TS header*/
    pmt->hr.sync_byte = 0x47;
    pmt->hr.transport_error_indicator =0;
    pmt->hr.payload_unit_start_indicator = 1;
    pmt->hr.transport_priority = 0;
    pmt->hr.PID = VPE_PMT_PID;
    pmt->hr.transport_scrambling_control = 0;
    pmt->hr.adaptation_field_control = 1;
    pmt->hr.continuity_counter = 0;
    pmt->hr.af = NULL;

    /*Write the Program Association Section*/
    pmt->pas.table_id = 0x02;
    pmt->pas.section_syntax_indicator =1;
    pmt->pas.transport_stream_id = 1; //Can be defined as anything by the user.
    pmt->pas.version_number = pmt->pas.section_number= pmt->pas.last_section_number = 0;
    pmt->pas.current_next_indicator = 1;
    pmt->pmt.PCR_PID = ts->pcr_pid;
    pmt->pmt.program_info_length = 0;
    pmt->pmt.program_number = 1;
    pmt->pmt.num_streams = ts->num_streams;
    pmt->pmt.streams = ts->st;
    /*Calculate section_length*/
    pmt->pas.section_length = 9+4+(ts->num_streams*5);//bytes following section_length+CRC+5*num_streams


    /*Write the PMT data in the ts structure*/
    /*Calculate the size in bytes. 
Assume: program_info_length = ES_info_length = 0
Length:
Ts header -                                     4 bytes.
pointer_field -                                 1
PMT header table_id ..... program_info_length   12
Stream data                                      5 bytes per stream
CRC                                              4    
     */
    ts->pmt_size = 0;
    ts->pmt_size+=4+1;
    ts->pmt_size+=12; 
    ts->pmt_size+=ts->num_streams*5;
    ts->pmt_size+=4; 

    /*Write the PMT data*/

    bs = bio_init_bitstream(ts->pmt_data,(size_t)ts->pmt_size);
    /*TS header*/
    bio_write_int(bs,pmt->hr.sync_byte,8);
    bio_write_int(bs,pmt->hr.transport_error_indicator,1);
    bio_write_int(bs,pmt->hr.payload_unit_start_indicator,1);
    bio_write_int(bs,pmt->hr.transport_priority,1);
    bio_write_int(bs,pmt->hr.PID,13);
    bio_write_int(bs,pmt->hr.transport_scrambling_control,2);
    bio_write_int(bs,pmt->hr.adaptation_field_control,2);
    bio_write_int(bs,pmt->hr.continuity_counter,4);
    /*pointer Field*/
    bio_write_int(bs,0,8);
    /*PMT header*/
    bio_write_int(bs,pmt->pas.table_id,8);
    bio_write_int(bs,pmt->pas.section_syntax_indicator,1);
    bio_write_int(bs,0,1);
    bio_write_int(bs,0x03,2); //rsvd
    bio_write_int(bs,pmt->pas.section_length,12); 
    bio_write_int(bs,pmt->pmt.program_number,16);
    bio_write_int(bs,0x03,2); //rsvd
    bio_write_int(bs,pmt->pas.version_number,5);
    bio_write_int(bs,pmt->pas.current_next_indicator,1);
    bio_write_int(bs,pmt->pas.section_number,8);
    bio_write_int(bs,pmt->pas.last_section_number,8);
    bio_write_int(bs,0x7,3); //rsvd
    bio_write_int(bs, pmt->pmt.PCR_PID,13);
    bio_write_int(bs, 0xF,4);//rsvd
    bio_write_int(bs, pmt->pmt.program_info_length,12);
    for(i=pmt->pmt.num_streams-1;i>=0;i--){
	st = &pmt->pmt.streams[i];
	bio_write_int(bs, st->stream_type,8);
	bio_write_int(bs,0x7,3);
	bio_write_int(bs, st->elementary_PID,13);
	bio_write_int(bs,0xF,4);
	bio_write_int(bs,st->ES_info_length,12);
    }

    bio_write_int(bs,0x9e28c6dd,32); //CRC = 0  for now
    bio_close_bitstream(bs);
    free(pmt);
    return VPE_SUCCESS;
}


