#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>


#include "nkn_assert.h"
#include "nkn_vpe_h264_parser.h"
#include "mfp_video_aligner.h"
//#include "nkn_vpe_types.h"
#include "nkn_vpe_mfp_ts2mfu.h"
#include "latm_parser.h"
#include "nkn_vpe_mfu_defs.h"
#include "nkn_vpe_mfp_ts.h"
#include "nkn_vpe_mfu_parse.h"
#include "nkn_vpe_mfu_parser.h"
#include "nkn_memalloc.h"
#include "nkn_vpe_parser_err.h"
#include "nkn_vpe_utils.h"

uint32_t glob_enable_mfu_sanity_checker = 0;
uint32_t glob_print_aligner_output = 0;
uint32_t glob_print_avsync_log = 0;
uint32_t glob_anomaly_sample_duration_correction = 0;
uint32_t glob_latm_audio_encapsulation_enabled = 0;

//#define CHECK//file write for validating
//#define TS2MFU_TEST
#define AUDIO_LUT
#define _MFU_WRITE_

#if defined(CHECK)
#define A_(x)   x
#define B_(x)   x
#define C_(x)  	x
//#define DBG_MFPLOG(dId,dlevel,dmodule,fmt,...) //
#else
#include "nkn_debug.h"
#define A_(x)   //x
#define B_(x)   //x
#define C_(x)   //x
#endif

int32_t total_audio_bias = 0;


/* unit_count,timescale,default_unit_duration,default_unit size each of
   4 bytes */
#define VIDEO 0
#define AUDIO 1

#define MAX_SPS_PPS_SIZE 300

#define RAW_FG_FIXED_SIZE 16

#define MFU_FIXED_SIZE 16
#define FIXED_BOX_SIZE 8

//#define TRUE   (0)
//#define FALSE  (!TRUE)

//#define MAX_AAC_FRAME_LEN 	(2048)
#define ADTS_FRAME_SIZE 	(9)
uint32_t glob_max_adts_frame_size =2048;

uint64_t tot_vid =0, tot_aud = 0;


int32_t mfu_basic_check(
	mfu_data_t *mfu_data, 
	uint32_t* slve_pkts, 
	uint32_t slve_npkts,
	uint8_t *data);

#if !defined(INC_ALIGNER)
static uint32_t ts2mfu_parse_NALs(uint8_t* , uint32_t *,ts2mfu_desc_t*);
#endif

static int32_t ts2mfu_desc_update(accum_ts_t *accum,
	ts2mfu_desc_t* ts2mfu_desc, uint32_t* mstr_pkts, uint32_t mstr_npkts,
	uint32_t* slve_pkts, uint32_t slve_npkts,
	uint8_t *data, mfub_t *mfubox_header);

static int32_t ts2mfu_desc_init(ts2mfu_desc_t* ts2mfu_desc, uint32_t* mstr_pkts, uint32_t mstr_npkts,
	uint32_t* slve_pkts, uint32_t slve_npkts,
	uint8_t *data);

static int32_t mfu_converter_get_raw_video_box(accum_ts_t *accum, 
	uint8_t *data, uint32_t *mstr_pkts, uint32_t mstr_npkts,
	ts2mfu_desc_t* ts2mfu_desc, uint32_t data_type);

static int32_t mfu_converter_get_raw_audio_box(accum_ts_t *accum,
	uint8_t *data, uint32_t *slve_pkts, uint32_t slve_npkts,
	ts2mfu_desc_t* ts2mfu_desc, uint32_t data_type);

//static int32_t ts2mfu_desc_free(ts2mfu_desc_t *);

static int32_t 
mfu_get_audio_sample_duration(uint32_t *sample_duration, uint32_t *PTS,
	uint32_t num_AU, ts2mfu_desc_t *ts2mfu_desc);

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
static int32_t write16_byte_swap( uint8_t *data,uint32_t pos,uint16_t
	val)
{
    data[pos] = val>>8;
    data[pos+1] = val&0xFF;
    return 1;
}
static int32_t write64_byte_swap( uint8_t *data,uint32_t pos,uint64_t
	val)
{
    data[pos] = val>>56;
    data[pos+1] = (val>>48)&0xFF;
    data[pos+2] = (val>>40)&0xFF;
    data[pos+3] = (val>>32)&0xFF;
    data[pos+4] = (val>>24)&0xFF;
    data[pos+5] = (val>>16)&0xFF;
    data[pos+6] = (val>>8)&0xFF;
    data[pos+7] = (val&0xFF);
    return 1;
}
static uint32_t uint32_byte_swap(uint32_t input){
    uint32_t ret=0;
    ret = (((input&0x000000FF)<<24) + ((input&0x0000FF00)<<8) +
	    ((input&0x00FF0000)>>8) + ((input&0xFF000000)>>24));
    return ret;
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

static void dump_input_pkts(
	uint8_t * data,
	uint32_t *slve_pkts,
	uint32_t slve_npkts,
	uint32_t is_Audio){
	
	FILE *fid;
	char fname[50]={'\0'};
	uint8_t * s_ts_data;
	uint32_t ssize;
	static uint32_t frag = 0;
	uint32_t len = 0;
	uint32_t i;

	s_ts_data = ( uint8_t*)nkn_malloc_type(slve_npkts * TS_PKT_SIZE,
		mod_vpe_mfp_ts2mfu_t);
	if(s_ts_data != NULL){
		for(i=0;i<slve_npkts;i++){
		    memcpy(s_ts_data +	(i*TS_PKT_SIZE),
			    data + (slve_pkts[i]),
			    TS_PKT_SIZE);
		}
		ssize = slve_npkts*TS_PKT_SIZE;
		if(is_Audio){
			len =	snprintf(NULL,0,"/var/log/nkn/A_%d.ts",frag);
			snprintf(fname,len+1,"/var/log/nkn/A_%d.ts",frag);
		}else{
			len =	snprintf(NULL,0,"/var/log/nkn/V_%d.ts",frag);
			snprintf(fname,len+1,"/var/log/nkn/V_%d.ts",frag);
		}
		fid = fopen(fname,"wb");
		if(fid != NULL){
			fwrite(s_ts_data,ssize,1,fid);
			fclose(fid);
		}		
		free(s_ts_data);
		frag++;
	}	
}
	

/*
 *    mfu_sanity_check
 *    parse the mfu data and check for few key params sanity
 *
 */
	//strip down version of mfu_sanity_check
int32_t mfu_basic_check(
	mfu_data_t *mfu_data, 
	uint32_t* slve_pkts, 
	uint32_t slve_npkts,
	uint8_t *data){

	mfu_contnr_t *mfuc;
	int32_t ret = 0;
	
	mfuc = parseMFUCont(mfu_data->data);

	if (mfuc == NULL) {
		ret = -1;
		goto mfu_basic_check_return;
	}
	
	uint32_t i;
	int32_t audio_index = -1;
	int32_t video_index = -1;
	uint8_t *buf;

	for (i = 0; i < mfuc->rawbox->desc_count; i++){
		if(!strncmp(mfuc->datbox[i]->cmnbox->name, "vdat", 4))
			video_index = i;
		if(!strncmp(mfuc->datbox[i]->cmnbox->name, "adat", 4))
			audio_index = i;				
	}
	
	assert(video_index != -1);
	assert(audio_index != -1);
	assert(video_index != audio_index);
	
	buf = (uint8_t *)mfuc->datbox[audio_index]->dat;
	for(i = 0; i < mfuc->rawbox->descs[audio_index]->unit_count; i++){
		//check whether adts header exist
		//TODO exit gracefully if buf[0] or buf[1] cross their buffer limit ie no invalid read
		if((buf[0] != 0xff) || ((buf[1]&0xf6) != 0xf0)){
			ret = -1;
			goto mfu_basic_check_return;
		}
		buf += mfuc->rawbox->descs[audio_index]->sample_sizes[i];
	}


mfu_basic_check_return:
	if(ret == -1){
		dump_input_pkts(data, slve_pkts, slve_npkts, 1);
	}
	
	if(mfuc)
	mfuc->del(mfuc);
	return(ret);
}

static void mfu_sanity_check(mfu_data_t *mfu_data, uint32_t* slve_pkts, uint32_t slve_npkts,
	uint8_t *data, ts2mfu_cont_t *ts2mfu_cont)
{
    uint8_t *mfu_data_orig;
    uint8_t *sps_pps = NULL;
    uint32_t sps_pps_size;
    unit_desc_t *vmd=NULL,*amd = NULL;
    uint32_t is_video;
    uint32_t is_audio;
    mfub_t mfu_hdr;
    int32_t ret;
    uint32_t i;
    uint8_t *data_start;
    uint8_t byte1, byte2;
    uint32_t data_pos;
    uint32_t dump_data = 0;
    int32_t corrupt_length;

    mfu_data_orig = mfu_data->data;

    vmd = (unit_desc_t*)nkn_malloc_type(sizeof(unit_desc_t),
	    mod_vpe_mfp_ts2mfu_t);
    amd = (unit_desc_t*)nkn_malloc_type(sizeof(unit_desc_t),
	    mod_vpe_mfp_ts2mfu_t);
    sps_pps = (uint8_t*)nkn_malloc_type(MAX_SPS_PPS_SIZE,
	    mod_vpe_mfp_ts2mfu_t);

    //parse the MFU file thus created    
    ret = mfu_parse (vmd, amd, mfu_data_orig, mfu_data->data_size,
	    sps_pps, &sps_pps_size, &is_audio, &is_video,
	    &mfu_hdr);
    data_start = amd->mdat_pos;
    data_pos = mfu_hdr.offset_adat + 8;

    for(i = 0; i < amd->sample_count; i++) {

	//check whether all sample size are valid and non-zero
	if( amd->sample_sizes[i] <= 0){
	    (printf("Err: sample %u: sample size %u\n", i, amd->sample_sizes[i]));
	}	

	//check whether all sample duration are valid and non-zero
	if( amd->sample_duration[i] == 0) {
	    (printf("Err: sample %u: sample_duration %u\n", i, amd->sample_duration[i]));
	}

	//check whether all audio frame starts with adts header
	byte1 = *data_start;
	byte2 = *(data_start + 1);
	if(byte1 != 0xff || (byte2 & 0xf6) != 0xf0) {
	    (printf("Err: sample %u ADTS header not found in frame start, %x %x\n", i, byte1, byte2));
	    dump_data = 1;
	}

	data_start = data_start + amd->sample_sizes[i];
	data_pos = data_pos + amd->sample_sizes[i];
	if(data_pos > mfu_data->data_size) {
	    (printf("Err: sending partial frame, last_frame_end %u mfusize %u \n", data_pos, mfu_data->data_size));
	}

    }

    corrupt_length = ts2mfu_cont->last_pkt_end_offset - ts2mfu_cont->last_pkt_start_offset; 
    if(corrupt_length < 0 || corrupt_length > TS_PKT_SIZE -4){
	//	dump_data = 1;
	//	(printf("Err: emulation prevention logic may fail\n"));
    }

    if(ts2mfu_cont->curr_chunk_frame_start < ts2mfu_cont->prev_chunk_frame_end){
	ts2mfu_cont->curr_chunk_frame_start += 16;
    }

    if(ts2mfu_cont->first_instance != 0){
	//continuity in ts packets received
	if(ts2mfu_cont->curr_chunk_frame_start - ts2mfu_cont->prev_chunk_frame_end != 1){
	    (printf("Err: discontinuity, tx extra/less audio frames\n"));
	    (printf("last chunk end %u, this chunk started at %u\n", ts2mfu_cont->prev_chunk_frame_end, ts2mfu_cont->curr_chunk_frame_start));
	    (printf("prev: pkts_retx %u \tstart %u \tend %u\n",ts2mfu_cont->prev_aud_chunk_cont[0],ts2mfu_cont->prev_aud_chunk_cont[1],ts2mfu_cont->prev_aud_chunk_cont[2]));
	    dump_data = 1;
	}
    }

    if(ts2mfu_cont->first_instance == 0){
	//make first instance as non-zero for subsequent check
	ts2mfu_cont->first_instance = 1;
    }

    if(dump_data == 1) {
	FILE *fid;
	char fname[50]={'\0'};
	mfu_data_t* mfu;
	uint32_t len;
	mfu = mfu_data;
	static int32_t frag_no1;
	uint8_t * s_ts_data;
	uint32_t ssize;

	s_ts_data = ( uint8_t*)nkn_malloc_type(slve_npkts * TS_PKT_SIZE,
		mod_vpe_mfp_ts2mfu_t);
	for(i=0;i<slve_npkts;i++){
	    memcpy(s_ts_data +	(i*TS_PKT_SIZE),
		    data + (slve_pkts[i]),
		    TS_PKT_SIZE);
	}
	ssize = slve_npkts*TS_PKT_SIZE;

	len =	snprintf(NULL,0,"A_%d.ts",frag_no1);
	snprintf(fname,len+1,"A_%d.ts",frag_no1);
	fid = fopen(fname,"wb");
	fwrite(s_ts_data,ssize,1,fid);
	fclose(fid);
	free(s_ts_data);

	if(ts2mfu_cont->prev_aud_chunk != NULL){
	    len =	snprintf(NULL,0,"pre_A_%d.ts",frag_no1);
	    snprintf(fname,len+1,"pre_A_%d.ts",frag_no1);
	    fid = fopen(fname,"wb");
	    fwrite(ts2mfu_cont->prev_aud_chunk, ts2mfu_cont->prev_aud_chunk_size, 1, fid);
	    fclose(fid);
	}

	len =	snprintf(NULL,0,"MFU_%d.ts",frag_no1);
	snprintf(fname,len+1,"MFU_%d.ts",frag_no1);
	fid = fopen(fname, "wb");	    
	fwrite(mfu->data,mfu->data_size,1,fid);
	fclose(fid);
	frag_no1++;
	//	assert(dump_data != 1);
    }

    {
	//copy the previous frame
	if(ts2mfu_cont->prev_aud_chunk != NULL){
	    free(ts2mfu_cont->prev_aud_chunk);
	    ts2mfu_cont->prev_aud_chunk = NULL;
	}
	if(ts2mfu_cont->prev_aud_chunk == NULL){
	    ts2mfu_cont->prev_aud_chunk_size = slve_npkts * TS_PKT_SIZE;
	    ts2mfu_cont->prev_aud_chunk = (uint8_t *)nkn_malloc_type(ts2mfu_cont->prev_aud_chunk_size,
		    mod_vpe_mfp_ts2mfu_t);
	    if(ts2mfu_cont->prev_aud_chunk != NULL){
		for(i=0;i<slve_npkts;i++){
		    memcpy(ts2mfu_cont->prev_aud_chunk + (i*TS_PKT_SIZE),
			    data + (slve_pkts[i]),
			    TS_PKT_SIZE);
		}
	    }
	    else{
#if !defined(CHECK)
		DBG_MFPLOG("TS2MFU", ERROR, MOD_MFPLIVE, "no memory for storing previous audio chunk");
#endif
	    }
	}
	ts2mfu_cont->prev_aud_chunk_cont[0] = ts2mfu_cont->num_tspkts_retx;
	ts2mfu_cont->prev_aud_chunk_cont[1] = ts2mfu_cont->last_pkt_start_offset;
	ts2mfu_cont->prev_aud_chunk_cont[2] = ts2mfu_cont->last_pkt_end_offset;
    }

    mfu2ts_clean_vmd_amd((unit_desc_t*)vmd, (unit_desc_t*)amd, (uint8_t*)sps_pps);
    return;
}

/* mfu_data_t* -> o/p mfu structure
 * vid_data -> i/p video data
 * vid_data_size -> i/p video data size
 * aud_data -> i/p audio data
 * aud_data_size -> i/p audio data size 
 * mfubox_header -> i/p mfu box header info
 */
mfu_data_t* 
mfp_ts2mfu(uint32_t* mstr_pkts, uint32_t mstr_npkts,
	   uint32_t* slve_pkts, uint32_t slve_npkts,
	   accum_ts_t *accum,
	   ts2mfu_cont_t *ts2mfu_cont )
{
    ts2mfu_desc_t* ts2mfu_desc;
    mfu_data_t* mfu_data;
    uint32_t mfu_data_size;//mfu_data_pos=0;
    //uint32_t temp_size;
    uint32_t vdat_pos=0,adat_pos=0;
    uint32_t vdat_box_size = 0;
    uint8_t *mfubox_pos =NULL;
    uint8_t *rwfgbox_pos = NULL;
    uint8_t *vdatbox_pos = NULL;
    uint8_t *adatbox_pos = NULL;
    int32_t ret = 1;

    uint8_t *data;
    mfub_t *mfubox_header;

    data = accum->data;
    mfubox_header = &accum->mfu_hdr;

    mfu_data = (mfu_data_t*) nkn_calloc_type(1, sizeof(mfu_data_t),
	    mod_vpe_mfp_ts2mfu_t);
    if(mfu_data == NULL){
#if !defined(CHECK)    	
	DBG_MFPLOG("TS2MFU", ERROR, MOD_MFPLIVE, "Couldn't allocate memory for mfu_data struct");
#endif
	return VPE_ERROR;
    }

    /*allocate, initailize and update the ts2mfu descriptor structure */
    ts2mfu_desc = (ts2mfu_desc_t*) nkn_calloc_type(1, sizeof(ts2mfu_desc_t),
						   mod_vpe_mfp_ts2mfu_t);
    if(ts2mfu_desc == NULL){
#if !defined(CHECK)    	
      DBG_MFPLOG("TS2MFU", ERROR, MOD_MFPLIVE, "Couldn't allocate memory for ts2mfu_desc struct");
#endif

      return VPE_ERROR;
    }
    //    printf("ts2mfu_desc allocated %p\n", ts2mfu_desc);

    ret = ts2mfu_desc_init(ts2mfu_desc, mstr_pkts, mstr_npkts, slve_pkts, slve_npkts, data);
    if (ret == VPE_ERROR) {
	ts2mfu_desc_free(ts2mfu_desc);
	free(mfu_data);
#if !defined(CHECK)    	
	DBG_MFPLOG("TS2MFU", ERROR, MOD_MFPLIVE, "Err in ts2mfu_desc_init");
#endif
	return VPE_ERROR;
    }
    ret = ts2mfu_desc_update(accum, ts2mfu_desc, mstr_pkts, mstr_npkts,
			     slve_pkts, slve_npkts, data, 
			     mfubox_header);
    if (ret == VPE_ERROR) {
	ts2mfu_desc_free(ts2mfu_desc);
	free(mfu_data);
#if !defined(CHECK)    	
	DBG_MFPLOG("TS2MFU", ERROR, MOD_MFPLIVE, "Err in ts2mfu_desc_update");
#endif
	return VPE_ERROR;
    }

    /*allocate for actual mfu */
    mfu_data_size = MFU_FIXED_SIZE + ts2mfu_desc->mfub_box_size +
	ts2mfu_desc->mfu_raw_box_size;
    if (ts2mfu_desc->is_video) {
	mfu_data_size +=  ts2mfu_desc->vdat_size + FIXED_BOX_SIZE; 
	ts2mfu_desc->video_duration = ts2mfu_cont->video_duration;
	ts2mfu_desc->video_time_accumulated = ts2mfu_cont->video_time_accumulated;
    }
    if (ts2mfu_desc->is_audio) {
	if(*(ts2mfu_desc->adat_vector[0].data + ts2mfu_desc->adat_vector[0].offset) != (uint8_t)0xff 
		&& !glob_latm_audio_encapsulation_enabled){
	    ts2mfu_desc->adat_size++;//case where extra 0xff will be inserted- this happens only for first frame
	}
	mfu_data_size +=  ts2mfu_desc->adat_size + FIXED_BOX_SIZE;
	ts2mfu_desc->audio_duration = mfubox_header->audio_duration;

    }
    mfu_data->data = (uint8_t *)nkn_malloc_type(mfu_data_size,
						mod_vpe_mfp_ts2mfu_t);
    if(mfu_data->data == NULL){
#if !defined(CHECK)
	DBG_MFPLOG("TS2MFU", ERROR, MOD_MFPLIVE, "Couldn't allocate memory for mfu_data->data struct");
#endif

      return VPE_ERROR;
    }

    /*Start the mfu file write from here*/
    /*write RAW Header box */
    rwfgbox_pos =mfu_data->data + ts2mfu_desc->mfub_box_size +
	(1*(BOX_NAME_SIZE + BOX_SIZE_SIZE));
#ifdef AUDIO_BIAS
    memcpy(&ts2mfu_desc->bias,&ts2mfu_cont->bias, sizeof(audio_bias_t));
#endif
    mfu_write_rwfg_box(ts2mfu_desc->mfu_raw_box_size,
		       ts2mfu_desc->mfu_raw_box, 
		       rwfgbox_pos, 
		       ts2mfu_desc);

    //    if(//glob_print_avsync_log)
    {
	uint32_t tot_t = 0;
	uint32_t i;
	
	for(i = 0 ; i < ts2mfu_desc->mfu_raw_box->videofg.unit_count; i++){
	    tot_t+= uint32_byte_swap(ts2mfu_desc->mfu_raw_box->videofg.sample_duration[i]);
	    if(!ts2mfu_cont->audio_sample_duration)
		ts2mfu_cont->audio_sample_duration=uint32_byte_swap(ts2mfu_desc->mfu_raw_box->videofg.sample_duration[0]);
	}
	ts2mfu_cont->video_tot_acc += tot_t;
	ts2mfu_cont->num_video_chunks++;
	ts2mfu_cont->avg_video_duration = \
		(uint32_t)(ts2mfu_cont->video_tot_acc/ts2mfu_cont->num_video_chunks);
	uint32_t tmp;
	tmp = (ts2mfu_cont->avg_video_duration >> 1);
	if((ts2mfu_desc->video_duration > (ts2mfu_cont->avg_video_duration + tmp)) ||
	   (ts2mfu_desc->video_duration < (ts2mfu_cont->avg_video_duration - tmp))){
	    #if !defined(CHECK)
	DBG_MFPLOG("AVSyncLog", MSG, MOD_MFPLIVE, "Video time exceeds average duration");
	#endif  
	    
	}
        #if !defined(CHECK)
	DBG_MFPLOG("AVSyncLog", MSG, MOD_MFPLIVE, "vt %lu\t vd %u\t", ts2mfu_cont->video_tot_acc,tot_t);
	#endif
	tot_t = 0;
	for(i = 0 ; i < ts2mfu_desc->mfu_raw_box->audiofg.unit_count; i++){
	    tot_t+= uint32_byte_swap(ts2mfu_desc->mfu_raw_box->audiofg.sample_duration[i]);
	}

	ts2mfu_cont->audio_tot_acc += tot_t;
	ts2mfu_cont->num_audio_chunks++;
	ts2mfu_cont->avg_audio_duration = \
		(uint32_t)(ts2mfu_cont->audio_tot_acc/ts2mfu_cont->num_audio_chunks);	
	tmp = (ts2mfu_cont->avg_audio_duration >> 1);
	if((ts2mfu_desc->audio_duration > (ts2mfu_cont->avg_audio_duration + tmp)) ||
		(ts2mfu_desc->audio_duration < (ts2mfu_cont->avg_audio_duration - tmp))){
		#if !defined(CHECK)
		DBG_MFPLOG("AVSyncLog", MSG, MOD_MFPLIVE, "Audio time exceeds average duration");
		#endif
	}
	#if !defined(CHECK)
	DBG_MFPLOG("AVSyncLog", MSG, MOD_MFPLIVE, "A-aaced %ld\t at %lu\t ad %u\t df %ld\t bias %d",
		   (int64_t)ts2mfu_desc->audio_duration-(int64_t)tot_t,
		   ts2mfu_cont->audio_tot_acc, tot_t,
		   ((int64_t)ts2mfu_cont->video_tot_acc - (int64_t)ts2mfu_cont->audio_tot_acc),
		   ts2mfu_desc->bias.total_audio_bias);
	#endif
	ts2mfu_cont->tot_aud_vid_diff =  (int64_t)ts2mfu_cont->audio_tot_acc-(int64_t)ts2mfu_cont->video_tot_acc;
	ts2mfu_cont->aud_diff = (int64_t)tot_t - (int64_t)ts2mfu_desc->audio_duration;
    }


#ifdef AUDIO_BIAS
    uint32_t tot_t = 0, i;
    for(i = 0 ; i < ts2mfu_desc->mfu_raw_box->audiofg.unit_count; i++)
	tot_t+= uint32_byte_swap(ts2mfu_desc->mfu_raw_box->audiofg.sample_duration[i]);
    
    /* not necessary for file publishing */
    if (accum->spts->audio1) {
	accum->spts->audio1->residual_mfu_time =
	    (int32_t)tot_t-(int32_t)ts2mfu_desc->audio_duration;
	#if !defined(CHECK)
	DBG_MFPLOG("AVSyncLog", MSG, MOD_MFPLIVE,"Accum residual time"
		   " = %d",accum->spts->audio1->residual_mfu_time);    
	#endif
    }
    memcpy(&ts2mfu_cont->bias,&ts2mfu_desc->bias, sizeof(audio_bias_t));
#endif
    //NK'
#ifdef AUDIO_LUT
    //    ts2mfu_desc->audio_duration = mfubox_header->audio_duration;
    ts2mfu_desc->last_audio_index = ts2mfu_cont->last_audio_index;
#endif

    /*write VDAT box */
    if (ts2mfu_desc->is_video) {
	vdat_pos =  ts2mfu_desc->mfub_box_size +
	    ts2mfu_desc->mfu_raw_box_size + (2 *(BOX_NAME_SIZE + BOX_SIZE_SIZE));
	vdatbox_pos = mfu_data->data + vdat_pos;
	mfu_write_vdat_box(accum, ts2mfu_desc->vdat_size, vdatbox_pos, ts2mfu_desc);
	vdat_box_size = BOX_NAME_SIZE + BOX_SIZE_SIZE;
    }
    /*write ADAT box*/
    if (ts2mfu_desc->is_audio) {
	adat_pos = ts2mfu_desc->mfub_box_size +
	    ts2mfu_desc->mfu_raw_box_size +
	    ts2mfu_desc->vdat_size + vdat_box_size +(2*( BOX_NAME_SIZE
			+ BOX_SIZE_SIZE));

	adatbox_pos =  mfu_data->data + adat_pos;
	mfu_write_adat_box(ts2mfu_desc->adat_size, adatbox_pos, ts2mfu_desc);
	ts2mfu_cont->is_audio = ts2mfu_desc->is_audio;
	ts2mfu_cont->num_tspkts_retx = ts2mfu_desc->num_tspkts_retx;
	ts2mfu_cont->last_pkt_start_offset = ts2mfu_desc->last_pkt_start_offset;
	ts2mfu_cont->last_pkt_end_offset = ts2mfu_desc->last_pkt_end_offset;	
	ts2mfu_cont->curr_chunk_frame_start = ts2mfu_desc->curr_chunk_frame_start;
	ts2mfu_cont->curr_chunk_frame_end = ts2mfu_desc->curr_chunk_frame_end;
    }


    /*write MFU Header box*/
    mfubox_pos = mfu_data->data;
    mfubox_header->offset_vdat = vdat_pos;
    mfubox_header->offset_adat = adat_pos;
    mfubox_header->mfu_size = mfu_data_size;
    mfu_write_mfub_box(ts2mfu_desc->mfub_box_size, mfubox_header,
	    mfubox_pos);

    mfu_data->data_size = mfu_data_size;
#undef _MFU_WRITE_
#ifdef _MFU_WRITE_
    FILE *fp;
    fp = fopen("mfu_write.txt", "wb");
    fwrite(mfu_data->data, mfu_data->data_size, 1, fp);
    fflush(fp);
    fclose(fp);

#endif
	if(!glob_latm_audio_encapsulation_enabled){
		//do sanity and basic check only for ADTS encaps
	    if(glob_enable_mfu_sanity_checker && slve_npkts) {
			mfu_sanity_check(mfu_data, slve_pkts, slve_npkts, data, ts2mfu_cont);
	    }
	    if(slve_pkts != NULL && slve_npkts) {
	      if(mfu_basic_check(mfu_data, slve_pkts, slve_npkts, data) == -1){
#if !defined(CHECK)
			DBG_MFPLOG("TS2MFU", MSG, MOD_MFPLIVE, "got error in mfu basic checks");
#endif    	
	    	return VPE_ERROR;
	      }
	    }
	} else {
		//glob_latm_audio_encapsulation_enabled
		free_latmctx_mem(ts2mfu_desc->latmctx, ts2mfu_desc->num_latmmux_elements);
		uint64_t res;
		res = nkn_memstat_type(mod_vpe_mfp_latm_parser);
		if(res != 0){
			NKN_ASSERT(0);
		}
	}

    ts2mfu_cont->prev_chunk_frame_end = ts2mfu_cont->curr_chunk_frame_end;

#if defined(INC_ALIGNER)
    if(accum->aligner_ctxt->delete_frame(accum->aligner_ctxt,\
    	accum->video_frame_tbl) != aligner_success){
#if !defined(CHECK)
        DBG_MFPLOG("TS2MFU", MSG, MOD_MFPLIVE, "Failed to delete frame memory");
#endif    	
    	return VPE_ERROR;
    }
#endif



    if(!ts2mfu_cont->store_ts2mfu_desc)
	ts2mfu_desc_free(ts2mfu_desc);
    else
	ts2mfu_cont->ts2mfu_desc = (void*)(ts2mfu_desc);
    return mfu_data;

}

/*
 * mfub_box_size -> i/p size of that box
 * mfubox_header ->i/p pointer having the actual data to be written into the box
 * mfubox_pos -> o/p indicates the pos where this box has tobe written 
 */
    int32_t 
mfu_write_mfub_box(uint32_t mfub_box_size, mfub_t* mfubox_header,
	uint8_t* mfubox_pos)
{
    uint32_t pos =0;
    /* writting box_name */
    mfubox_pos[pos] = 'm';
    mfubox_pos[pos+1] = 'f';
    mfubox_pos[pos+2] = 'u';
    mfubox_pos[pos+3] = 'b';
    pos += BOX_NAME_SIZE;
#ifdef _BIG_ENDIAN_
    /* write box size */
    write32_byte_swap(mfubox_pos, pos, mfub_box_size);
    pos += BOX_SIZE_SIZE;

    mfub_t mfub_hn;
    memset(&mfub_hn, 0,sizeof(mfub_t));
    /* writting actual mfu data */
    write16_byte_swap((uint8_t*)&(mfub_hn.version),0,mfubox_header->version);
    write16_byte_swap((uint8_t*)&(mfub_hn.program_number), 0, 
	    mfubox_header->program_number);
    write16_byte_swap((uint8_t*)&(mfub_hn.stream_id), 0,
	    mfubox_header->stream_id);
    write16_byte_swap((uint8_t*)&(mfub_hn.flags), 0,mfubox_header->flags);
    write32_byte_swap((uint8_t*)&(mfub_hn.sequence_num), 0, 
	    mfubox_header->sequence_num);
    write32_byte_swap((uint8_t*)&(mfub_hn.timescale), 0,
	    mfubox_header->timescale);
    write32_byte_swap((uint8_t*)&(mfub_hn.duration), 0,
	    mfubox_header->duration);
    write32_byte_swap((uint8_t*)&(mfub_hn.audio_duration), 0,
	    mfubox_header->audio_duration);
    write16_byte_swap((uint8_t*)&(mfub_hn.video_rate), 0,
	    mfubox_header->video_rate);
    write16_byte_swap((uint8_t*)&(mfub_hn.audio_rate), 0,
	    mfubox_header->audio_rate);
    write16_byte_swap((uint8_t*)&(mfub_hn.mfu_rate), 0,
	    mfubox_header->mfu_rate);
    mfub_hn.video_codec_type = mfubox_header->video_codec_type;
    mfub_hn.audio_codec_type = mfubox_header->audio_codec_type;
    write32_byte_swap((uint8_t*)&(mfub_hn.compliancy_indicator_mask),
	    0,mfubox_header->compliancy_indicator_mask);
    write64_byte_swap((uint8_t*)&(mfub_hn.offset_vdat), 0 ,
	    mfubox_header->offset_vdat);
    write64_byte_swap((uint8_t*)&(mfub_hn.offset_adat), 0, 
	    mfubox_header->offset_adat);
    write32_byte_swap((uint8_t*)&(mfub_hn.mfu_size), 0, 
	    mfubox_header->mfu_size);
    memcpy(mfubox_pos + pos, &(mfub_hn), sizeof(mfub_t));
#else//_BIG_ENDIAN_
    /* write box size */
    write32(mfubox_pos, pos, mfub_box_size);
    pos += BOX_SIZE_SIZE;
    /* writting actual mfu data */
    memcpy((mfubox_pos + pos), mfubox_header, mfub_box_size);
#endif//_BIG_ENDIAN_
    return VPE_SUCCESS;
}

/*
 * vdat_size -> i/p size of the box
 * vdat -> i/p pointer having the actual data tobe written
 * vdatbox_pos -> o/p indicates the pos where this box has tobe written 
 */
int32_t 
mfu_write_vdat_box(accum_ts_t *accum, uint32_t vdat_size, uint8_t*
	vdatbox_pos, ts2mfu_desc_t *ts2mfu_desc)
{
    uint32_t pos = 0;
    /* writting box_name */
    vdatbox_pos[pos] = 'v';
    vdatbox_pos[pos+1] = 'd';
    vdatbox_pos[pos+2] = 'a';
    vdatbox_pos[pos+3] = 't';
    pos += BOX_NAME_SIZE;
 
    /* write box size */
#ifdef _BIG_ENDIAN_
    write32_byte_swap(vdatbox_pos, pos, vdat_size);
#else
    write32(vdatbox_pos, pos, vdat_size);
#endif
    pos += BOX_SIZE_SIZE;


#if defined(INC_ALIGNER)
    //create vdat-new
    video_frame_info_t *cur_frame;
    ts_pkt_info_t *cur_pkt;    
    uint32_t i =0;
    uint32_t k= 0;
    cur_frame = accum->video_frame_tbl;
    //copy SEI occuring before SPS/PPS first followed by video data
#if 0
    for(i=0; i<cur_frame->num_SEI; i++) {
      memcpy(vdatbox_pos + pos, cur_frame->SEI_offset[i], cur_frame->SEI_length[i]);
      pos += cur_frame->SEI_length[i];
      vdat_size -= cur_frame->SEI_length[i];
    }
#endif
    //pos = 0;
    while(cur_frame){
    	cur_pkt = cur_frame->first_pkt;
	while(cur_pkt){
	  for(i=0; i< cur_pkt->num_SEI; i++) {
	    memcpy(vdatbox_pos + pos, cur_pkt->SEI_offset[i], cur_pkt->SEI_length[i]);
	    pos += cur_pkt->SEI_length[i];
	    vdat_size -= cur_pkt->SEI_length[i];
	  }
	  k++;
	  if(cur_pkt->is_slice_start != 0) {
		memcpy(vdatbox_pos + pos, 
			cur_pkt->vector.data + cur_pkt->vector.offset,
			cur_pkt->vector.length);
		pos += cur_pkt->vector.length;
		vdat_size -= cur_pkt->vector.length;
	  }
	  cur_pkt = cur_pkt->next;
	}
	cur_frame = cur_frame->next;
    }
    assert(vdat_size == 0);
    B_(printf("total vdat mempcy size %d\n", pos));    
    
#else
    uint32_t i;
    /* writting actual vdat data */
    //memcpy((vdatbox_pos + pos), vdat, vdat_size);
    for( i = 0; i < ts2mfu_desc->num_vdat_vector_entries; i++) {
	memcpy(vdatbox_pos + pos, 
		ts2mfu_desc->vdat_vector[i].data + ts2mfu_desc->vdat_vector[i].offset, 
		ts2mfu_desc->vdat_vector[i].length);
	pos += ts2mfu_desc->vdat_vector[i].length;
	B_(printf("i = %d, ts2mfu_desc->vdat_vector[i].length %d\n", i, ts2mfu_desc->vdat_vector[i].length));
    }
    B_(printf("total vdat mempcy size %d\n", pos));    
#endif
   
    return VPE_SUCCESS;
}

/*
 * adat_size -> i/p size of the box
 * adat -> i/p pointer having the actual data tobe written
 * adatbox_pos -> o/p indicates the pos where this box has tobe written 
 */
int32_t 
mfu_write_adat_box(uint32_t adat_size, 
		   uint8_t* adatbox_pos, 
		   ts2mfu_desc_t *ts2mfu_desc)
{
    uint32_t pos = 0;
    uint32_t i;
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
	if(!glob_latm_audio_encapsulation_enabled){
	    if(*(ts2mfu_desc->adat_vector[0].data + ts2mfu_desc->adat_vector[0].offset) != (uint8_t)0xff){
		*(adatbox_pos + pos) = 0xff;
		pos++;
	    }
	}
    for(i = 0; i < ts2mfu_desc->num_adat_vector_entries; i++) {
    	if(ts2mfu_desc->adat_vector[i].asclength){
		memcpy(adatbox_pos + pos, 
			ts2mfu_desc->adat_vector[i].ascdata, 
			ts2mfu_desc->adat_vector[i].asclength);
		pos += ts2mfu_desc->adat_vector[i].asclength;    
    	}
	memcpy(adatbox_pos + pos, 
		ts2mfu_desc->adat_vector[i].data + ts2mfu_desc->adat_vector[i].offset, 
		ts2mfu_desc->adat_vector[i].length);
	pos += ts2mfu_desc->adat_vector[i].length;
	B_(printf("i = %d, ts2mfu_desc->adat_vector[i].length %d\n", i, ts2mfu_desc->adat_vector[i].length));
    }
    B_(printf("total adat mempcy size %d\n", pos));
    return VPE_SUCCESS;
}

/*
 * mfu_raw_box_size -> i/p size of the box
 * mfu_raw_box -> i/p pointer having the actual data tobe written
 * rwfgbox_pos -> o/p indicates the pos where this box has tobe written 
 * ts2mfu_desc -> i/p ts2mfu desriptor structure 
 */
int32_t mfu_write_rwfg_box(uint32_t mfu_raw_box_size,
			   mfu_rwfg_t *mfu_raw_box, 
			   uint8_t* rwfgbox_pos,
			   ts2mfu_desc_t* ts2mfu_desc)

{
    uint32_t pos = 0;
#ifndef _BIG_ENDIAN_
    uint32_t temp_size = 0;
#endif
    /* writting box_name */
    rwfgbox_pos[pos] = 'r';
    rwfgbox_pos[pos+1] = 'w';
    rwfgbox_pos[pos+2] = 'f';
    rwfgbox_pos[pos+3] = 'g';
    pos += BOX_NAME_SIZE;
    /* write box size */
#ifdef _BIG_ENDIAN_
    write32_byte_swap(rwfgbox_pos, pos, mfu_raw_box_size);
#else
    write32(rwfgbox_pos, pos, mfu_raw_box_size);
#endif
    pos += BOX_SIZE_SIZE;
    /* write actual data into box */
    if (ts2mfu_desc->is_video != 0) {
#ifdef _BIG_ENDIAN_
	write32_byte_swap(rwfgbox_pos, pos,
		ts2mfu_desc->mfu_raw_box->videofg_size);
	pos+=4;
	/* write uint_count,timescale,default unit duration,unit size*/
	write32_byte_swap(rwfgbox_pos, pos, mfu_raw_box->videofg.unit_count);
	pos+=4;
	write32_byte_swap(rwfgbox_pos, pos, mfu_raw_box->videofg.timescale);
	pos+=4;
	write32_byte_swap(rwfgbox_pos, pos, mfu_raw_box->videofg.default_unit_duration);
	pos+=4;
	write32_byte_swap(rwfgbox_pos, pos, mfu_raw_box->videofg.default_unit_size);
	pos+=4;
#else
	write32(rwfgbox_pos, pos,
		ts2mfu_desc->mfu_raw_box->videofg_size);
	pos+=4;
	temp_size = RAW_FG_FIXED_SIZE;
	memcpy((rwfgbox_pos + pos), &mfu_raw_box->videofg, temp_size);
	pos += temp_size;
#endif
#if 0
	/* 
	 * write PTS,DTS and sample_sizes
	 * PTS,DTS and sample sizes are in contiguous memory location,
	 * so writting as bulk. PTS, DTS and sample_sizes were already
	 * converted based on the flag
	 */
	memcpy((rwfgbox_pos + pos),mfu_raw_box->videofg.PTS,
		3*4*ts2mfu_desc->num_video_AU);
	pos += 3*4*ts2mfu_desc->num_video_AU;
#endif
	if(ts2mfu_desc->ignore_PTS_DTS != 2){
	    /* get sample_duration from DTS value */
	    mfu_get_sample_duration(mfu_raw_box->videofg.sample_duration,
				    ts2mfu_desc->vid_DTS,
				    ts2mfu_desc->num_video_AU,
				    ts2mfu_desc->video_duration,
				    ts2mfu_desc->video_time_accumulated);
	    
	    /*Get composition time duration from PTS/DTS*/
	    mfu_get_composition_duration(mfu_raw_box->videofg.composition_duration,
					 ts2mfu_desc->vid_PTS,
					 ts2mfu_desc->vid_DTS,
					 ts2mfu_desc->num_video_AU);
	}

	/* writting sample_duration and composition_duration,
	   sample_sizes*/
	memcpy((rwfgbox_pos +pos),
		mfu_raw_box->videofg.sample_duration,
		4*ts2mfu_desc->num_video_AU);
	pos += 4*ts2mfu_desc->num_video_AU;

	memcpy((rwfgbox_pos +pos),
		mfu_raw_box->videofg.composition_duration,
		4*ts2mfu_desc->num_video_AU);
	pos += 4*ts2mfu_desc->num_video_AU;


	memcpy((rwfgbox_pos +pos),
		mfu_raw_box->videofg.sample_sizes,
		4*ts2mfu_desc->num_video_AU);
	pos += 4*ts2mfu_desc->num_video_AU;

	/* writting codec_info_size */

#ifdef _BIG_ENDIAN_
	write32_byte_swap(rwfgbox_pos, pos,
		mfu_raw_box->videofg.codec_info_size);
	pos += 4;
#else
	write32(rwfgbox_pos, pos,
		mfu_raw_box->videofg.codec_info_size);
	pos += 4;
#endif
	/* writting sps and pps info */
	memcpy((rwfgbox_pos + pos),mfu_raw_box->videofg.codec_specific_data,
		mfu_raw_box->videofg.codec_info_size);
	pos += mfu_raw_box->videofg.codec_info_size;
    }
    if (ts2mfu_desc->is_audio != 0) {
#ifdef _BIG_ENDIAN_
	write32_byte_swap(rwfgbox_pos, pos,
		ts2mfu_desc->mfu_raw_box->audiofg_size);
	pos+=4;
	/* write uint_count,timescale,defaultunit duration,unit size*/
	write32_byte_swap(rwfgbox_pos, pos, mfu_raw_box->audiofg.unit_count);
	pos+=4;
	write32_byte_swap(rwfgbox_pos, pos, mfu_raw_box->audiofg.timescale);
	pos+=4;
	write32_byte_swap(rwfgbox_pos, pos, mfu_raw_box->audiofg.default_unit_duration);
	pos+=4;
	write32_byte_swap(rwfgbox_pos, pos, mfu_raw_box->audiofg.default_unit_size);
	pos+=4;
#else
	write32(rwfgbox_pos, pos,
		ts2mfu_desc->mfu_raw_box->audiofg_size);
	pos+=4;
	temp_size = RAW_FG_FIXED_SIZE;
	memcpy((rwfgbox_pos + pos), &mfu_raw_box->audiofg, temp_size);
	pos += temp_size;
#endif
#if 0
	/*
	 * write PTS,DTS and sample_sizes
	 * PTS,DTS and sample sizes are in contiguous memory location,
	 * so writting as bulk
	 */
	memcpy((rwfgbox_pos + pos),mfu_raw_box->audiofg.PTS,
		3*4*ts2mfu_desc->num_audio_AU);
	pos += 3*4*ts2mfu_desc->num_audio_AU;
#endif

        if(ts2mfu_desc->ignore_PTS_DTS != 2){
	/* get sample_duration from PTS value */
	    mfu_get_audio_sample_duration(mfu_raw_box->audiofg.sample_duration,
					  ts2mfu_desc->aud_PTS,
					  ts2mfu_desc->num_audio_AU,
					  ts2mfu_desc);
	}

	/* writting sample_duration */
	memcpy((rwfgbox_pos + pos),mfu_raw_box->audiofg.sample_duration,
		4*ts2mfu_desc->num_audio_AU);
	pos += 4*ts2mfu_desc->num_audio_AU;
	/* writting  composition_duration*/
	memcpy((rwfgbox_pos + pos),mfu_raw_box->audiofg.composition_duration,
		4*ts2mfu_desc->num_audio_AU);
	pos += 4*ts2mfu_desc->num_audio_AU;
	/* writting sample_sizes*/
	memcpy((rwfgbox_pos + pos),mfu_raw_box->audiofg.sample_sizes,
		4*ts2mfu_desc->num_audio_AU);
	pos += 4*ts2mfu_desc->num_audio_AU;	
#if 0	
	{
	    int32_t i;
	    for( i = 0; i <= ts2mfu_desc->num_audio_AU; i++) {
		printf("MFU sample num %d, PTS %8u, size %8u\n", i, mfu_raw_box->audiofg.sample_duration[i], uint32_byte_swap(mfu_raw_box->audiofg.sample_sizes[i]));
	    }
	}
#endif 
	/* writting codec_info_size */
#ifdef _BIG_ENDIAN_
	write32_byte_swap(rwfgbox_pos, pos,
		mfu_raw_box->audiofg.codec_info_size);
	pos += 4;
#else
	write32(rwfgbox_pos, pos,
		mfu_raw_box->audiofg.codec_info_size);
	pos += 4;
#endif
	/* writting sps and pps info */
	memcpy((rwfgbox_pos + pos), mfu_raw_box->audiofg.codec_specific_data,
		mfu_raw_box->audiofg.codec_info_size);
	pos += mfu_raw_box->audiofg.codec_info_size;
    }
    return VPE_SUCCESS;
}


int32_t mfu_get_composition_duration(uint32_t *sample_duration,
	uint32_t *PTS,
	uint32_t *DTS,
	uint32_t num_AU)
{
    uint32_t i=0;
    C_(printf("new - mfu_get_composition_duration; num_AU %u\n", num_AU));
#ifdef _BIG_ENDIAN_
    for(i=0;i<num_AU;i++){
	sample_duration[i] = uint32_byte_swap(PTS[i] -DTS[i]);
	C_(printf("new - %u: PTS  %u, DTS  %u, Sample Duration %u\n", i, PTS[i], DTS[i], sample_duration[i]));
    }
    //anomaly_correction_with_average_ts(sample_duration, num_AU, NULL);    
#else
    for(i=0;i<num_AU;i++){
	sample_duration[i] = PTS[i]-DTS[i];
	C_(printf("new - %u: PTS  %u, DTS  %u, Sample Duration %u\n", i, PTS[i], DTS[i], sample_duration[i]));
    }
#endif
    return VPE_SUCCESS;
}









int32_t mfu_get_sample_duration(uint32_t *sample_duration,
	uint32_t *PTS,
	uint32_t num_AU,
	uint32_t vid_duration,
	uint32_t video_time_accumulated)
{
    uint32_t i=0, tot_t=0, sd;
    C_(printf("new - mfu_get_sample_duration; num_AU %u\n", num_AU));
#ifdef _BIG_ENDIAN_
    for(i=0;i<(num_AU-1);i++){
	if(((int64_t)PTS[i+1]-(int64_t)PTS[i]) < 0){
	    uint32_t d = 0;
	    d = PTS[i+1]+(0xFFFFFFFF-PTS[i]);
	    sample_duration[i] = uint32_byte_swap(d);
	}
	else
	    sample_duration[i] = uint32_byte_swap(PTS[i+1] -PTS[i]);
	tot_t+= uint32_byte_swap(sample_duration[i]);
	C_(printf("new - %u: PTS next %u, PTS ct %u, Sample Duration %u\n", i, PTS[i+1], PTS[i], sample_duration[i]));
    }
    //last sample_duration
    //sample_duration[num_AU-1] = uint32_byte_swap(video_time_accumulated - PTS[num_AU-1]);
    sample_duration[num_AU-1] = uint32_byte_swap(vid_duration - tot_t);
    tot_t+= uint32_byte_swap(sample_duration[num_AU-1]);
#if defined(INC_ALIGNER)	
    if(glob_anomaly_sample_duration_correction){
	anomaly_correction_with_average_ts(sample_duration, num_AU, &tot_t);
    }
#endif
    sd = uint32_byte_swap(sample_duration[0]);
    
#else
    for(i=0;i<(num_AU-1);i++){
	if(((int64_t)PTS[i+1]-(int64_t)PTS[i]) < 0){
            uint32_t d = 0;
            d = PTS[i+1]+(0xFFFFFFFF-PTS[i]);
            sample_duration[i] = d;
        }
	else
	    sample_duration[i] = PTS[i+1] -PTS[i];
	C_(printf("%u: PTS next %u, PTS ct %u, Sample Duration %u\n",i, PTS[i+1], PTS[i], sample_duration[i]));
	tot_t+= sample_duration[i];
    }
    //last sample_duration
    sample_duration[num_AU-1] = sample_duration[num_AU-2];
    tot_t+= sample_duration[num_AU-1];
    sd = sample_duration[0];
#endif
    B_(printf("cc: num Video Units: %u Video Duration %u (%u)\n", num_AU, vid_duration/90, tot_t/90));
    if(((int32_t)(vid_duration)-(int32_t)tot_t) < (int32_t)sd)
	return VPE_SUCCESS;

    if((vid_duration-tot_t) >= sd){
#ifdef _BIG_ENDIAN_
	sample_duration[0]+= uint32_byte_swap(vid_duration-tot_t);
#else
	sample_duration[0]+= vid_duration-tot_t;
#endif

#if !defined(CHECK)
	DBG_MFPLOG("TS2MFU", MSG, MOD_MFPLIVE, "Video being biased by %u",vid_duration-tot_t);
#endif
    	printf("Video being biased by %u\n",vid_duration-tot_t);
	//assert(0);
    }


    return VPE_SUCCESS;
}

//#define PRINT

static int32_t mfu_get_audio_sample_duration(uint32_t *sample_duration, 
					     uint32_t *PTS,
					     uint32_t num_AU, 
					     ts2mfu_desc_t *ts2mfu_desc)
{

    uint32_t i=0;
    uint32_t sample_duration_calc = 0;
    //frame duration in 90000 clock ticks for various sample frequency
    //96k,	  88k,	 64k,	    48k,
    //44.1k, 32k,	24k,	   22.05k,
    //16k,	  12k,	 11.025k, 8k, 
    //undef, undef, undef, undef
    uint32_t std_sample_duration[16] = {960, 1045, 1440, 1920, 
	2090, 2880, 3840, 4180, 
	5760, 7680, 8360, 11520, 
	0, 0, 0, 0};
    uint32_t frame_duration_table_11_025[49] = {
	8359,   8359,   8360,   8359,   8359,   8359,   8359,   8359,   8360,   8359,   8359,   8359,
	8359,   8360,   8359,   8359,   8359,   8359,   8359,   8360,   8359,   8359,   8359,   8359,   8360,   8359,   8359,   8359,   8359,
	8360,   8359,   8359,   8359,   8359,   8359,   8360,   8359,   8359,   8359,   8359,   8360,   8359,   8359,   8359,   8359,   8359,
	8360,   8359,   8359};
    uint32_t frame_duration_table_22_050[49] = {
	4180,   4179,   4180,   4179,   4180,   4179,   4180,   4179,   4180,   4179,   4180,   4180,
	4179,   4180,   4179,   4180,   4180,   4179,   4180,   4179,   4180,   4180,   4179,   4180,   4179,   4180,   4179,   4180,   4180,
	4179,   4180,   4179,   4180,   4180,   4179,   4180,   4179,   4180,   4180,   4179,   4180,   4179,   4180,   4180,   4179,   4180,
	4179,   4180,   4180};
    uint32_t frame_duration_table_44_100[49] = {
	2090,    2089,   2090,   2090,   2089,   2090,   2090,   2090,   2090,   2089,   2090,   2090,
	2090,   2090,   2089,   2090,   2090,   2090,   2090,   2089,   2090,   2090,   2090,   2090,   2089,   2090,   2090,   2090,   2090,
	2089,   2090,   2090,   2090,   2090,   2089,   2090,   2090,   2090,   2090,   2089,   2090,   2090,   2090,   2090,   2089,   2090,
	2090,   2090,   2090};
    uint32_t frame_duration_table_88_200[49] = {
	1045,    1045,   1045,   1045,   1044,   1045,   1045,   1045,   1045,   1045,   1045,   1045,
	1045,   1045,   1044,   1045,   1045,   1045,   1045,   1045,   1045,   1045,   1045,   1045,   1044,   1045,   1045,   1045,   1045,
	1045,   1045,   1045,   1045,   1044,   1045,   1045,   1045,   1045,   1045,   1045,   1045,   1045,   1045,   1045,   1044,   1045,
	1045,   1045,   1045};
    uint32_t *frame_duration_table;
    switch(ts2mfu_desc->sample_freq_index){
	case 1:
	    frame_duration_table = frame_duration_table_88_200;
	    break;
	case 4:
	    frame_duration_table = frame_duration_table_44_100;
	    break;
	case 7:
	    frame_duration_table = frame_duration_table_22_050;
	    break;
	case 10:
	    frame_duration_table = frame_duration_table_11_025;
	    break;
	default:
	    frame_duration_table = NULL;
	    break;
    }

	uint32_t biased_flag = 0;

#ifdef AUDIO_BIAS
    {
	memset(sample_duration,0,num_AU);
	int32_t tot_t=0;
	if(ts2mfu_desc->bias.total_audio_bias > AUDIO_BIAS_TH){
	    //printf("Greater than threshold\n");
	    ts2mfu_desc->bias.num_chnks_beyond_bias++;
	    tot_t = ts2mfu_desc->bias.total_audio_bias;
	    if(ts2mfu_desc->bias.num_chnks_beyond_bias > AUDIO_BIAS_MAX_OVER_TH){
		/*Bias the First Sample by an offset*/
#ifdef PRINT
		printf("We are biasing now\n");
#endif
		if(!frame_duration_table){
		    sample_duration_calc = std_sample_duration[ts2mfu_desc->sample_freq_index];
		    B_(printf("sample_duration_calc %u", sample_duration_calc));
		    for(i = 0 ; i < num_AU; i++){
			sample_duration[i] = uint32_byte_swap(sample_duration_calc);
			if((tot_t-=sample_duration[i])<AUDIO_BIAS_MAX_OVER_TH)
			    break;
		    }
		}
		else{
		    uint32_t j;
		    j =  ts2mfu_desc->last_audio_index;
		    for(i = 0 ; i < num_AU; i++){
			sample_duration[i] = uint32_byte_swap(frame_duration_table[j%49]);
			j++;
			if((tot_t-=sample_duration[i])<=0)
			    break;
		    }
		    ts2mfu_desc->last_audio_index = j%49;
		}
		ts2mfu_desc->bias.num_chnks_beyond_bias = 0;
		biased_flag = 1;
	    }
	}
	else
	    ts2mfu_desc->bias.num_chnks_beyond_bias = 0;
    }
    if(!frame_duration_table){
	sample_duration_calc = std_sample_duration[ts2mfu_desc->sample_freq_index];
	B_(printf("Audio sample_duration_calc %u", sample_duration_calc));
	for(i = 0 ; i < num_AU; i++){
	    sample_duration[i] += uint32_byte_swap(sample_duration_calc);
	}
    }
    else{
	uint32_t j;
	j =  ts2mfu_desc->last_audio_index;
	for(i = 0 ; i < num_AU; i++){
	    sample_duration[i] += uint32_byte_swap(frame_duration_table[j%49]);
	    j++;
	}
	ts2mfu_desc->last_audio_index = j%49;
    }

    {
	uint32_t tot_t = 0;
	for(i = 0 ; i < num_AU; i++){
	    tot_t+= uint32_byte_swap(sample_duration[i]);
	}
	if((int32_t)(ts2mfu_desc->audio_duration - tot_t) > 0 || biased_flag){
		ts2mfu_desc->bias.total_audio_bias+=(int32_t)(ts2mfu_desc->audio_duration-tot_t);
#ifdef PRINT
		printf("Sample level Bias = %d, Total audio bias = %d\n",(int32_t)(ts2mfu_desc->audio_duration-tot_t), ts2mfu_desc->bias.total_audio_bias);
#endif
	}
    }


#else

    if(!frame_duration_table){
	sample_duration_calc = std_sample_duration[ts2mfu_desc->sample_freq_index];
	B_(printf("sample_duration_calc %u", sample_duration_calc));
	for(i = 0 ; i < num_AU; i++){
	    sample_duration[i] = uint32_byte_swap(sample_duration_calc);
	}
    }
    else{
        uint32_t j;
        j =  ts2mfu_desc->last_audio_index;
        for(i = 0 ; i < num_AU; i++){
            sample_duration[i] = uint32_byte_swap(frame_duration_table[j%49]);
            j++;
        }
	//printf("Previous indx = %u Next indx = %u\n",ts2mfu_desc->last_audio_index,j%49);
        ts2mfu_desc->last_audio_index = j%49;
    }
#define PRINT7
#ifdef PRINT7
    {
	uint32_t tot_t = 0;
	for(i = 0 ; i < num_AU; i++){
	    tot_t+= uint32_byte_swap(sample_duration[i]);
	}
	tot_aud+=tot_t;
	total_audio_bias+=(int32_t)(ts2mfu_desc->audio_duration-tot_t);
	printf("Sample level Bias = %d, Total audio bias = %d\n",(int32_t)(ts2mfu_desc->audio_duration-tot_t), total_audio_bias);

    }
#endif //PRINT7

#endif //Audiobias


    return VPE_SUCCESS;
}

/* 
 * This function returns no:of:AU's presnt in the given ts file
 * pointed by data 
 * data -> i/p pointer pointing to ts file
 * data_size-> i/p size of the data
 * ret value -> o/p gives no:of:AU's
 */

    uint32_t 
mfu_conveter_get_AU_num(uint32_t* mstr_pkts, uint32_t mstr_npkts, uint8_t *data)
{
    uint32_t num_AU=0;
    uint32_t i=0,pos =0;
    uint8_t payload_unit_start_indicator,sync_byte;


    for (i = 0 ; i < mstr_npkts; i++) {

	pos = mstr_pkts[i];
	sync_byte = *(data + pos);

	if (sync_byte != 0x47) {
#if !defined(CHECK)		
	    DBG_MFPLOG("TS2MFU", ERROR, MOD_MFPLIVE, "sync byte is not found, got %x %x %x mstr_npkts %u loopcnt %u", *(data+pos),*(data+pos+1),*(data+pos+2),mstr_npkts, i);
#endif
	    return VPE_ERROR;
	}

	pos++;//skip sync byte
	payload_unit_start_indicator = ((*(data + pos))&0x40)>>6;
	pos+=2;//skip pid

	if (payload_unit_start_indicator == 1) {
	    num_AU++;
	}//if condition

    }

    return num_AU;
}

/* This function is used to update elements of ts2mfu_desc structure.
 * ts2mfu_desc -> i/p and o/p  pointer pointing to ts2mfu desc
 * vid_data -> i/p pointer pointing to video data
 * vid_data_size -> i/p video data size
 * aud_data -> i/p pointer pointing to audio
 * aud_data_size -> i/p audio data size
 * mfubox_header -> i/p mfu box header info
 */
static int32_t 
ts2mfu_desc_update(accum_ts_t *accum, 
		   ts2mfu_desc_t* ts2mfu_desc, 
		   uint32_t* mstr_pkts, uint32_t mstr_npkts,
		   uint32_t* slve_pkts, uint32_t slve_npkts,
		   uint8_t *data, mfub_t *mfubox_header)
{
    uint16_t temp = 0;
    int32_t ret = 1;
    //check for big endian flag
    temp = mfubox_header->flags;
    //ts2mfu_desc->is_big_endian = (temp &&0x4);

    if (mstr_npkts != 0) {
	ts2mfu_desc->mfu_raw_box->videofg.unit_count =
	    ts2mfu_desc->num_video_AU;
	ts2mfu_desc->mfu_raw_box->videofg.timescale =
	    mfubox_header->timescale;//90000;

	/* to update PTS,sample_sizes, codec info */
	ret = mfu_converter_get_raw_video_box(accum, data, mstr_pkts, mstr_npkts,
		ts2mfu_desc, VIDEO);
	if (ret == VPE_ERROR)
	    return VPE_ERROR;
	ts2mfu_desc->mfu_raw_box->videofg_size = RAW_FG_FIXED_SIZE + 3
	    *(4*ts2mfu_desc->num_video_AU) + CODEC_INFO_SIZE_SIZE +
	    ts2mfu_desc->mfu_raw_box->videofg.codec_info_size;

	ts2mfu_desc->mfu_raw_box_size +=
	    ts2mfu_desc->mfu_raw_box->videofg_size + 4; 

    }
    if (slve_npkts != 0) {

	ts2mfu_desc->mfu_raw_box->audiofg.timescale =
	    mfubox_header->timescale;//90000;

	/* to update DTS,sample_sizes,codecinfo */
	ret = mfu_converter_get_raw_audio_box(accum, data, slve_pkts, slve_npkts,
		ts2mfu_desc, AUDIO);
	if (ret == VPE_ERROR)
	    return VPE_ERROR;

	ts2mfu_desc->mfu_raw_box->audiofg.unit_count =
	    ts2mfu_desc->num_audio_AU;

	ts2mfu_desc->mfu_raw_box->audiofg_size = RAW_FG_FIXED_SIZE + 3
	    *(4*ts2mfu_desc->num_audio_AU) + CODEC_INFO_SIZE_SIZE +
	    ts2mfu_desc->mfu_raw_box->audiofg.codec_info_size;

	ts2mfu_desc->mfu_raw_box_size +=
	    ts2mfu_desc->mfu_raw_box->audiofg_size + 4;
    }
    ts2mfu_desc->mfub_box_size = sizeof(mfub_t);

    return VPE_SUCCESS;
}

/* This function is used to initialize elements of ts2mfu_desc structure.
 * ts2mfu_desc -> i/p and o/p  pointer pointing to ts2mfu desc
 * vid_data -> i/p pointer pointing to video data
 * vid_data_size -> i/p video data size
 * aud_data -> i/p pointer pointing to audio
 * aud_data_size -> i/p audio data size
 */
    static int32_t 
ts2mfu_desc_init(ts2mfu_desc_t* ts2mfu_desc, uint32_t* mstr_pkts, uint32_t mstr_npkts,
	uint32_t* slve_pkts, uint32_t slve_npkts,
	uint8_t *data)
{
    uint32_t *temp = NULL, *temp_1 = NULL;
    int32_t ret = VPE_SUCCESS;
    uint32_t num_ts_packets = 0;//, temp_num_video_frames=0;

    ts2mfu_desc->mfub_box = (mfub_t*) nkn_calloc_type(1, sizeof(mfub_t),
	    mod_vpe_mfp_ts2mfu_t);
    if(ts2mfu_desc->mfub_box == NULL){
      #if !defined(CHECK)
	DBG_MFPLOG("TS2MFU", ERROR, MOD_MFPLIVE, "Couldn't allocate memory for ts2mfu_desc->mfub_box struct");
      #endif
      return VPE_ERROR;
    }
    ts2mfu_desc->mfu_raw_box = (mfu_rwfg_t*)nkn_calloc_type(1, sizeof(mfu_rwfg_t),
	    mod_vpe_mfp_ts2mfu_t);
    if(ts2mfu_desc->mfu_raw_box == NULL){
      #if !defined(CHECK)
	DBG_MFPLOG("TS2MFU", ERROR, MOD_MFPLIVE, "Couldn't allocate memory for ts2mfu_desc->mfu_raw_box struct");
      #endif
      return VPE_ERROR;
    }
    ts2mfu_desc->is_audio = 0;
    ts2mfu_desc->is_video = 0;
    ts2mfu_desc->mfu_raw_box_size = 0;
    ts2mfu_desc->sps_size = 0;
    ts2mfu_desc->pps_size = 0;
    if (mstr_npkts != 0) {
	/* find num of AU's */
	ret = mfu_conveter_get_AU_num(mstr_pkts, mstr_npkts, data);
	if (ret == VPE_ERROR) {
	    return VPE_ERROR;
	}
	//	temp_num_video_frames = ret;
	ts2mfu_desc->num_video_AU =  ret*10;//Max of 10 samples within n ts pkt
	ts2mfu_desc->mfu_raw_box->videofg.default_unit_duration = 0;
	ts2mfu_desc->mfu_raw_box->videofg.default_unit_size = 0;

	/* allocating for  sample_duration, composition_duration,
	   sample_sizes by single nkn_malloc_type */

	temp = (uint32_t*)nkn_malloc_type((4*ts2mfu_desc->num_video_AU)*3,
		mod_vpe_mfp_ts2mfu_t);
	if(temp == NULL){
	  #if !defined(CHECK)
	DBG_MFPLOG("TS2MFU", ERROR, MOD_MFPLIVE, "Couldn't allocate memory for vid sam_duration struct");
	#endif
	return VPE_ERROR;
	}
	ts2mfu_desc->mfu_raw_box->videofg.sample_duration = temp;
	ts2mfu_desc->mfu_raw_box->videofg.composition_duration = temp +
	    ts2mfu_desc->num_video_AU;
	ts2mfu_desc->mfu_raw_box->videofg.sample_sizes = temp +
	    (2*ts2mfu_desc->num_video_AU);	

	ts2mfu_desc->mfu_raw_box->videofg.codec_info_size = 0;
	ts2mfu_desc->mfu_raw_box->videofg.codec_specific_data = NULL;
	ts2mfu_desc->vid_PTS = (uint32_t*) nkn_malloc_type(4*ts2mfu_desc->num_video_AU,
		mod_vpe_mfp_ts2mfu_t);
	if(ts2mfu_desc->vid_PTS == NULL){
          #if !defined(CHECK)
	DBG_MFPLOG("TS2MFU", ERROR, MOD_MFPLIVE, "Couldn't allocate memory for vid PTS struct");
        #endif
	return VPE_ERROR;
        }
	ts2mfu_desc->vid_DTS = (uint32_t*) nkn_malloc_type(4*ts2mfu_desc->num_video_AU,
		mod_vpe_mfp_ts2mfu_t);
	if(ts2mfu_desc->vid_DTS == NULL){
          #if !defined(CHECK)
	DBG_MFPLOG("TS2MFU", ERROR, MOD_MFPLIVE, "Couldn't allocate memory for vid DTS struct");
        #endif
	return VPE_ERROR;
        }
	num_ts_packets = mstr_npkts;
	num_ts_packets = num_ts_packets + TOT_MAX_SEI;
#if !defined(INC_ALIGNER)
	ts2mfu_desc->vdat_vector = NULL;
	ts2mfu_desc->vdat_vector = (nkn_vpe_vector_t*)nkn_calloc_type(num_ts_packets * sizeof(nkn_vpe_vector_t), 1,
		mod_vpe_mfp_ts2mfu_t);
	if (ts2mfu_desc->vdat_vector == NULL) {
	  #if !defined(CHECK)
	DBG_MFPLOG("TS2MFU", ERROR, MOD_MFPLIVE, "Couldn't allocate memory for vid vdat struct");
	#endif
	return VPE_ERROR;
	}
	ts2mfu_desc->max_vdat_vector_entries = num_ts_packets;
	ts2mfu_desc->num_vdat_vector_entries = 0;
	ts2mfu_desc->num_video_AU =  ret;
#endif
	ts2mfu_desc->is_video = 1;
	ts2mfu_desc->vdat_size = 0;

    }

    if (slve_npkts != 0) {

	/* find num of AU's */
	ret =  slve_npkts;
	ret = 10*ret ; //assuming max n+10 frames in n ts packets
	B_(printf("space alloted for storing %d audio units\n",ret));

	ts2mfu_desc->num_audio_AU =  ret;
	ts2mfu_desc->mfu_raw_box->audiofg.default_unit_duration = 0;
	ts2mfu_desc->mfu_raw_box->audiofg.default_unit_size = 0;

	/* allocating for  sample_duration, composition_duration,
	   sample_sizes by single nkn_malloc_type */
	temp_1 = (uint32_t*)nkn_calloc_type((4*ts2mfu_desc->num_audio_AU)*3, 1,
		mod_vpe_mfp_ts2mfu_t);
	if (temp_1 == NULL) {
          #if !defined(CHECK)
	DBG_MFPLOG("TS2MFU", ERROR, MOD_MFPLIVE, "Couldn't allocate memory for aud sam_dur struct");
	#endif
	return VPE_ERROR;
        }
	ts2mfu_desc->mfu_raw_box->audiofg.sample_duration = temp_1;
	ts2mfu_desc->mfu_raw_box->audiofg.composition_duration = temp_1 +
	    ts2mfu_desc->num_audio_AU;
	ts2mfu_desc->mfu_raw_box->audiofg.sample_sizes = temp_1 +
	    (2*ts2mfu_desc->num_audio_AU);

	ts2mfu_desc->mfu_raw_box->audiofg.codec_info_size = 0;
	ts2mfu_desc->mfu_raw_box->audiofg.codec_specific_data = NULL;
	ts2mfu_desc->aud_PTS = (uint32_t*) nkn_calloc_type(4*ts2mfu_desc->num_audio_AU, 1,
		mod_vpe_mfp_ts2mfu_t);
	if (ts2mfu_desc->aud_PTS == NULL) {
          #if !defined(CHECK)
	DBG_MFPLOG("TS2MFU", ERROR, MOD_MFPLIVE, "Couldn't allocate memory for aud PTS struct");
        #endif
	return VPE_ERROR;
        }

	num_ts_packets = slve_npkts;
	//printf("num_ts_packets %d\n", num_ts_packets);
	ts2mfu_desc->adat_vector = NULL;
	ts2mfu_desc->adat_vector = (nkn_vpe_vector_t*)nkn_calloc_type(
		ts2mfu_desc->num_audio_AU * sizeof(nkn_vpe_vector_t), 1,
		mod_vpe_mfp_ts2mfu_t);
	if (ts2mfu_desc->adat_vector == NULL) {
	  #if !defined(CHECK)
	DBG_MFPLOG("TS2MFU", ERROR, MOD_MFPLIVE, "Couldn't allocate memory for aud adat struct");
	#endif
	return VPE_ERROR;
	}
	ts2mfu_desc->max_adat_vector_entries = num_ts_packets;
	ts2mfu_desc->num_adat_vector_entries = 0;
	ts2mfu_desc->is_audio = 1;
	ts2mfu_desc->adat_size = 0;
	ts2mfu_desc->latmctx = (struct LATMContext **)nkn_calloc_type(
				ts2mfu_desc->num_audio_AU,sizeof(struct LATMContext *),
				mod_vpe_mfp_ts2mfu_t);
	if (ts2mfu_desc->latmctx == NULL) {
        #if !defined(CHECK)
			DBG_MFPLOG("TS2MFU", ERROR, MOD_MFPLIVE, "Couldn't allocate memory for latmctx");
        #endif
			return VPE_ERROR;
    }

    }
    ts2mfu_desc->Is_sps_pps = 0;    
    ts2mfu_desc->is_SEI_extract = 0;
    ts2mfu_desc->dump_size = 0;
    ts2mfu_desc->num_tspkts_retx = 0;
    ts2mfu_desc->last_pkt_start_offset = 0;
    ts2mfu_desc->last_pkt_end_offset = 0;

    return VPE_SUCCESS;
}

/* 
   this function is used to free the elements of ts2mfu desc
 * ts2mfu_desc -> pointer pointing to ts2mfu structure
 */
int32_t ts2mfu_desc_free( ts2mfu_desc_t* ts2mfu_desc)
{
    if (ts2mfu_desc != NULL) {
	if (ts2mfu_desc->mfub_box != NULL) {
	    free(ts2mfu_desc->mfub_box);
	}
	if (ts2mfu_desc->mfu_raw_box != NULL) {
	    if (ts2mfu_desc->is_video != 0) {
		if (ts2mfu_desc->mfu_raw_box->videofg.sample_duration != NULL) {
		    free(ts2mfu_desc->mfu_raw_box->videofg.sample_duration);
		    ts2mfu_desc->mfu_raw_box->videofg.sample_duration = NULL;
		}
		/*
		 * since sample_duration, composition_duration and
		 * sample_sizes are of contiguous memory, once sample_duration is
		 * freed, the remaing will alos be freed
		 */
#if 0
		if (ts2mfu_desc->mfu_raw_box->videofg.composition_duration != NULL)
		    free(ts2mfu_desc->mfu_raw_box->videofg.composition_duration);
		if (ts2mfu_desc->mfu_raw_box->videofg.sample_sizes != NULL)
		    free(ts2mfu_desc->mfu_raw_box->videofg.sample_sizes);
#endif
		if (ts2mfu_desc->mfu_raw_box->videofg.codec_specific_data
			!= NULL)
		    free(ts2mfu_desc->mfu_raw_box->videofg.codec_specific_data);
	    }
	    if (ts2mfu_desc->is_audio != 0) {
		if (ts2mfu_desc->mfu_raw_box->audiofg.sample_duration != NULL) {
		    free(ts2mfu_desc->mfu_raw_box->audiofg.sample_duration);
		    ts2mfu_desc->mfu_raw_box->audiofg.sample_duration = NULL;
		}
		/*
		 * since sample_duration, composition_duration and
		 * sample_sizes are of contiguous memory, once
		 * sample_duration is
		 * freed, the remaing will alos be freed
		 */
#if 0
		if (ts2mfu_desc->mfu_raw_box->audiofg.composition_duration != NULL)
		    free(ts2mfu_desc->mfu_raw_box->audiofg.composition_duration);
		if (ts2mfu_desc->mfu_raw_box->audiofg.sample_sizes !=
			NULL)
		    free(ts2mfu_desc->mfu_raw_box->audiofg.sample_sizes);
#endif
		if (ts2mfu_desc->mfu_raw_box->audiofg.codec_specific_data != NULL)
		    free(ts2mfu_desc->mfu_raw_box->audiofg.codec_specific_data);
	    }
	    free(ts2mfu_desc->mfu_raw_box);
	}
	if (ts2mfu_desc->is_video != 0) {
#if !defined(INC_ALIGNER)
	    if (ts2mfu_desc->vdat_vector != NULL) {
		free(ts2mfu_desc->vdat_vector);
	    }
#endif
	    if (ts2mfu_desc->vid_PTS != NULL)
		free(ts2mfu_desc->vid_PTS);
	    if (ts2mfu_desc->vid_DTS != NULL)
		free(ts2mfu_desc->vid_DTS);

	}
	if (ts2mfu_desc->is_audio != 0) {
	    if (ts2mfu_desc->adat_vector != NULL) {
		free(ts2mfu_desc->adat_vector);
	    }
	    if (ts2mfu_desc->aud_PTS != NULL)
		free(ts2mfu_desc->aud_PTS);
	}
	if(ts2mfu_desc->latmctx){
		free(ts2mfu_desc->latmctx);
	}
	free(ts2mfu_desc);
    }

    return VPE_SUCCESS;
}

/* 
 * This function updates the frame size in MFU struct
 * temp_AU_size -> i/p frame size
 * present_AU_num ->  i/p frame number
 * ts2mfu_desc -> o/p frame size is updated in this MFU data struct
 */

static void update_frame_size(uint32_t temp_AU_size, uint32_t present_AU_num,
	uint32_t data_type, ts2mfu_desc_t* ts2mfu_desc) {

    uint32_t temp_sample_sizes =0;
    A_(printf("type %d frame_num %d, frame size %d\n",data_type, present_AU_num,temp_AU_size));
    temp_sample_sizes = uint32_byte_swap(temp_AU_size);
    if (data_type == VIDEO)
	ts2mfu_desc->mfu_raw_box->videofg.sample_sizes[present_AU_num] = temp_sample_sizes;
    else
	ts2mfu_desc->mfu_raw_box->audiofg.sample_sizes[present_AU_num] = temp_sample_sizes;
    return ;	
}

/* 
 * This function parses the PES header for PTS value
 * data -> i/p pointer input data buffer
 * pos ->  i/p buffer start offset
 * PTS_value -> o/p pointer containing PTS value
 */

static uint32_t parse_PES_header(uint8_t *data, 
	uint32_t pos, 
	uint64_t *PTS_value,  
	uint64_t *DTS_value,  		
	uint32_t *payload_pos) 
{

    uint32_t temp_pos = 0;
    uint64_t PTS_temp1, PTS_temp2, PTS_temp3, PTS_temp4, PTS_temp5;
    uint64_t DTS_temp1, DTS_temp2, DTS_temp3, DTS_temp4, DTS_temp5;
    uint8_t  PTS_DTS_flags, PES_header_data_length;


    if (!(*(data+pos) == 0 && *(data+pos+1) == 0 && *(data+pos+2) == 1)) {
	/* not PES */
	*PTS_value =  0;
	pos = 0xffffffff;
	return (pos);
    }

    /* pointing to PES header */
    pos += 6;//skip  packet_start_code_prefix; stream_id; PES_packet_length;
    pos += 1; //skip PES_scrambling_control;PES_priority;data_alignment_indicator;copyright;original_or_copy;
    PTS_DTS_flags = (*(data+pos) &0xc0)>>6;
    pos+=1;//skip flag byte
    PES_header_data_length = *(data+pos);
    *payload_pos = PES_header_data_length + pos + 1;   
    pos += 1;//skip to PTS value
    temp_pos = pos;
    temp_pos+=PES_header_data_length;

    if ((PTS_DTS_flags == 0x2)) {
	PTS_temp1 = (((*(data + pos) & 0x0e) >> 1) << 30);
	pos++;
	PTS_temp2 = (((*(data + pos)) & 0xff) << 22);
	pos++;
	PTS_temp3 = ((*(data + pos) & 0xfe) >> 1) << 15;
	pos++;
	PTS_temp4 = (((*(data + pos)) & 0xff) << 7);
	pos++;
	PTS_temp5 = ((*(data+pos) & 0xfe) >> 1);
	pos++;
	*PTS_value = (PTS_temp1 | PTS_temp2 | PTS_temp3 |
		PTS_temp4 | PTS_temp5);
    }

    if (PTS_DTS_flags == 0x3) {
	PTS_temp1 = (((*(data + pos) & 0x0e) >> 1) << 30);
	pos++;
	PTS_temp2 = (((*(data + pos)) & 0xff) << 22);
	pos++;
	PTS_temp3 = ((*(data + pos) & 0xfe) >> 1) << 15;
	pos++;
	PTS_temp4 = (((*(data + pos)) & 0xff) << 7);
	pos++;
	PTS_temp5 = ((*(data + pos) & 0xfe) >> 1);
	pos++;
	*PTS_value = (PTS_temp1 | PTS_temp2 | PTS_temp3 |
		PTS_temp4 | PTS_temp5);
	//	pos += 5;//accounting for DTS. if DTS is required, extract similar to PTS

	DTS_temp1 = (((*(data + pos) & 0x0e) >> 1) << 30);
	pos++;
	DTS_temp2 = (((*(data + pos)) & 0xff) << 22);
	pos++;
	DTS_temp3 = ((*(data + pos) & 0xfe) >> 1) << 15;
	pos++;
	DTS_temp4 = (((*(data + pos)) & 0xff) << 7);
	pos++;
	DTS_temp5 = ((*(data + pos) & 0xfe) >> 1);
	pos++;
	*DTS_value = (DTS_temp1 | DTS_temp2 | DTS_temp3 |
		DTS_temp4 | DTS_temp5);
    }

    pos = temp_pos;
    return(pos);
}

/* 
 * This function parses the ts header
 * data -> i/p input data buffer
 * pos -> i/p buffer offset
 * payload_unit_start_indicator -> o/p pointer returning payload_unit_start_indicator
 */

static uint32_t parse_TS_header(uint8_t *data, uint32_t pos, 
	uint8_t *payload_unit_start_indicator,
	uint16_t* rpid, 
	uint8_t	*continuity_counter,
	uint8_t *adaptation_field_control) {

    uint8_t  sync_byte;
    uint32_t adaptation_field_len = 0;
    uint16_t pid;

    sync_byte = *(data+pos);
    if (sync_byte != 0x47) {
#if !defined(CHECK)    	
	DBG_MFPLOG("TS2MFU", ERROR, MOD_MFPLIVE, "sync byte - not found, got %x",sync_byte);
#endif
	return VPE_ERROR;
    }

    pos++;//skip sync byte
    *payload_unit_start_indicator = ((*(data+pos))&0x40)>>6;
    pid = data[pos]&0x1F;
    pid = (pid<<8)|data[pos+1];
    *rpid = pid;    
    pos += 2;//skip transport_priority,pid
    *adaptation_field_control = ((*(data+pos))&0x30)>>4;
    *continuity_counter = (*(data+pos)) & 0x0f;
    pos++;

    if ((*adaptation_field_control == 0x3) ||
	    (*adaptation_field_control == 0x2)) { 
	adaptation_field_len = *(data+pos);
	pos++;
	pos += adaptation_field_len;
    }
    return(pos);
}


static int32_t parse_ADTS_header(uint8_t* data, uint32_t *pos, int32_t *last_frame_leftover, 
				 uint32_t start_offset, int32_t *FSI_flag, uint32_t carryfwd_index, 
				 uint8_t  *carryfwd_data, ts2mfu_desc_t *ts2mfu_desc, int32_t first_frame_flag, uint32_t pkt_index)
{
    uint32_t orig_pos=0;
    uint8_t *orig_data=NULL;
    uint32_t header;
    uint8_t  old_byte, new_byte;
    uint32_t valid_header_found =FALSE;
    uint32_t frame_length = 0, temp;
    uint32_t j;
    uint8_t  restore_bytes[4];
    uint32_t restore_offset = 0;
    uint8_t  data3, data4, byte1 = 0;
    uint32_t carryfwd_index_five_flag = 0;

    C_(printf("start position %u\n", *pos));
    orig_pos = *pos + *last_frame_leftover;
    orig_data = data;
    ts2mfu_desc->dump_size = 0;

    if(*FSI_flag == TRUE){
	//carryfwd data will be utmost 5 bytes
	//max guaranteed bytes available for overloading is 4 bytes
	//hence the 1st byte in carryfwd data should not be overloaded onto tspacket
	if(carryfwd_index == 5){
	    byte1 = *carryfwd_data++;
	    carryfwd_index--;
	    orig_pos++;
	    carryfwd_index_five_flag = 1;
	}
	//*FSI_flag = FALSE;
	restore_offset = orig_pos;
	for(j = 0; j < carryfwd_index; j++) {
	    C_(printf("orig data %x\n",*(orig_data + orig_pos + j)));
	    restore_bytes[j] = *(orig_data + orig_pos + j);
	    *(orig_data + orig_pos + j) = *(carryfwd_data + j);
	    B_(printf("overwrite copy %x, orig_pos %d\n", *(carryfwd_data + j),orig_pos+j));
	}
    }

    //initial safety check for lastframeleftover
    if(orig_pos >= (start_offset + TS_PKT_SIZE)) { 
	return 0; 
    }

    //read first byte from buffer or byte1
    if(byte1 == 0){
	old_byte = *(orig_data + orig_pos);
	orig_pos++;
	ts2mfu_desc->dump_size++;
	if(orig_pos >= (start_offset + TS_PKT_SIZE)) { 
	    ts2mfu_desc->dump_size--;
	    return 0; 
	}
    } else {
	old_byte = byte1; 
	if(first_frame_flag == TRUE){
	    //these calculation changes are only for first frame in a chunk
	    *last_frame_leftover = *last_frame_leftover + 1;
	}
	ts2mfu_desc->dump_size++;
    }

    /**/
    while(valid_header_found == FALSE && (orig_pos < (start_offset + TS_PKT_SIZE))){
	//read second byte
	new_byte = *(orig_data + orig_pos);
	orig_pos++;
	if(orig_pos >= (start_offset + TS_PKT_SIZE)) { 
	    ts2mfu_desc->dump_size--;
	    break; 
	}

	ts2mfu_desc->dump_size++;

	header = ((old_byte & 0xff) << 8) | ((new_byte & 0xff)) ;
	C_(printf("header %x\n", header));

	if((header & 0xfff6) == 0xfff0){
	    //max aac frame length detection
	    //first byte after 0xfff denotes mpeg2 if 1, mpeg4 if 0
	    //mpeg2 aac frame sizes are fixed to 1024
	    //mpeg4 aac frame sizes can be 1024 or 2048
	    uint32_t tmp = 0;
	    tmp = (header & 0x0008);
	    if(tmp > 0){
	    	glob_max_adts_frame_size = 1024;
	    }else{
	        glob_max_adts_frame_size = 2048;
	    }
	    

	    //valid adts header found
	    ts2mfu_desc->last_pkt_start_offset = *pos - start_offset;
	    ts2mfu_desc->last_pkt_end_offset = orig_pos - 2 - start_offset;
	    ts2mfu_desc->num_tspkts_retx = pkt_index;
	    if(*FSI_flag == TRUE){
		ts2mfu_desc->num_tspkts_retx --;
		ts2mfu_desc->last_pkt_end_offset = TS_PKT_SIZE 
			- carryfwd_index -carryfwd_index_five_flag;
	    }

	    valid_header_found = TRUE;
	    data3 = *(orig_data + orig_pos); orig_pos++;
	    if(orig_pos >= (start_offset + TS_PKT_SIZE)) { 
		ts2mfu_desc->dump_size -= 2;
		break; 
	    }
	    ts2mfu_desc->sample_freq_index = (data3 & 0x3c) >> 2;

	    data4 = *(orig_data + orig_pos); orig_pos++;
	    temp = data4;
	    if(orig_pos >= (start_offset + TS_PKT_SIZE)) { 
		ts2mfu_desc->dump_size -= 2;
		break; 
	    }
	    C_(printf("byte1: temp %x\n", temp));

	    ts2mfu_desc->num_channels = (((data3 & 0x01) << 8 | (data4 & 0xc0)) & 0x01c0) >> 6;
	    if(ts2mfu_desc->num_channels == 0) {
		ts2mfu_desc->num_channels = 1;
	    }
	    frame_length  = (temp & 0x0003) << 11; //take lsb 2 bits, skip other

	    temp = *(orig_data + orig_pos); orig_pos++;
	    if(orig_pos >= (start_offset + TS_PKT_SIZE)) { 
		ts2mfu_desc->dump_size -= 2;
		break; 
	    }
	    C_(printf("byte2: temp %x\n", temp));
	    frame_length |= (temp & 0x00ff) << 3;  //take entire byte
	    temp = *(orig_data + orig_pos); 
	    C_(printf("byte3: temp %x\n", temp));

	    frame_length |= (temp & 0x00e0) >> 5; //take msb 3 bits
	    C_(printf("frame_length %d\n", frame_length));

#if 0
	    if(frame_length > glob_max_adts_frame_size/*MAX_AAC_FRAME_LEN*/  || frame_length <= ADTS_FRAME_SIZE){
#if !defined(CHECK)	    	
		DBG_MFPLOG("TS2MFU", MSG, MOD_MFPLIVE, "preventing adts frame header emulation");
#endif
		orig_pos = orig_pos - 3;
		valid_header_found = FALSE;
		frame_length = 0;
	    } else {
		ts2mfu_desc->dump_size = ts2mfu_desc->dump_size - 2;
	    }
#else
	    ts2mfu_desc->dump_size = ts2mfu_desc->dump_size - 2;
#endif

	} else {
	    //no adts header found, continue
	    old_byte = new_byte;

	}
    }

    if(*FSI_flag == TRUE){
	*FSI_flag = FALSE;
	//recover the bits for all frames except the first frame in the chunk
	if(first_frame_flag == FALSE){
	    for(j = 0; j < carryfwd_index; j++) {
		*(orig_data + restore_offset + j) = restore_bytes[j];
	    }
	}
    }

    return (frame_length);

}

/* 
 * This function update the frame size in output data structures and maintains the state variable
 * ts2mfu_desc -> i/p  state structure
 * data             -> i/p  input buffer pointer address
 * pos              -> i/p  input buffer position
 * i                  -> i/p  ts packet index
 * last_frame_flag -> o/p pointer flag variable
 * data_pos     -> o/p pointer to output buffer increment counter
 */

static uint32_t update_raw_data(ts2mfu_desc_t* ts2mfu_desc, uint8_t *data, 
	uint32_t pos, int32_t *last_frame_flag, uint32_t start_offset)
{
    //error check to see whether enough mem available for storing all vectors
    if(ts2mfu_desc->num_adat_vector_entries >= ts2mfu_desc->max_adat_vector_entries) {
#if !defined(CHECK)    	
	DBG_MFPLOG("TS2MFU", ERROR, MOD_MFPLIVE,  "adat_vector mem NA- alloted(%d), written(%d)",
		ts2mfu_desc->max_adat_vector_entries, ts2mfu_desc->num_adat_vector_entries);
#endif
	return VPE_ERROR;
    }

    //update the adat vector
    ts2mfu_desc->adat_vector[ts2mfu_desc->num_adat_vector_entries].data = data;
    ts2mfu_desc->adat_vector[ts2mfu_desc->num_adat_vector_entries].offset = pos;
    ts2mfu_desc->adat_vector[ts2mfu_desc->num_adat_vector_entries].length = (TS_PKT_SIZE - (pos - start_offset));

    //drop the new frame start in last ts packet of the given fragment
    if(*last_frame_flag == TRUE &&
    	ts2mfu_desc->dump_size <= (TS_PKT_SIZE - (pos - start_offset))) {
	A_(printf("last packet in fragment, memcpy %d\n", ts2mfu_desc->dump_size));	
	ts2mfu_desc->adat_vector[ts2mfu_desc->num_adat_vector_entries].length 
		= ts2mfu_desc->dump_size;
	//*last_frame_flag = FALSE;
	ts2mfu_desc->adat_size += ts2mfu_desc->dump_size;
    } else {
	B_(printf("audio_dat: memcpy %d\n", TS_PKT_SIZE - (pos - start_offset)));
	ts2mfu_desc->adat_size += TS_PKT_SIZE - (pos - start_offset);	    
    }  
    ts2mfu_desc->num_adat_vector_entries++;
    return(VPE_SUCCESS);
}

/* 
 * This function finds whether first 6 bytes  of adts header is split across ts packets
 * data -> i/p pointer input buffer pointer
 * available_payload -> i/p payload size in the ts packet
 * last_frame_leftover -> i/p leftover bytes of the previous frame
 * carryfwd_index -> o/p pointer that will return the number of adts frame header split
 * carryfwd_data -> o/p pointer to array containing the first 6 bytes of adts header split
 */

static int32_t find_adts_header_split(uint8_t *data, int32_t  available_payload,
	uint32_t *carryfwd_index, uint8_t  *carryfwd_data ) {

    uint32_t i;
    int32_t  frame_split_index = 0;
    int32_t FSI_flag = FALSE;
    *carryfwd_index = 0;

    frame_split_index = available_payload;
    B_(printf("available_payload %d, frame_split_index %d\n", available_payload, frame_split_index));
    if(frame_split_index > 5 || frame_split_index < 0) {
	B_(printf("frame split index falling beyond its limit\n"));    	
	return(FSI_flag);
    }
    if(frame_split_index > 0) {
	FSI_flag = TRUE;
	*carryfwd_index = frame_split_index;
	for(i = 0; i < *carryfwd_index; i++) {
	    *(carryfwd_data + i) = *(data + i);
	}
    }
    return(FSI_flag);
}

/* 
 * This function parses the ts file and updates PTS,DTS and
 * sample_sizes in ts2mfu desc - video data only
 * data -> i/p pointer ponintg to ts file
 * data_size -> i/p size of the data
 * ts2mfu_desc -> o/p pointer pointing to ts2mfu desc
 * data_type -> i/p either video or audio data type
 */

static int32_t mfu_converter_get_raw_video_box(accum_ts_t *accum,
	uint8_t *data, uint32_t *mstr_pkts, uint32_t mstr_npkts,
	ts2mfu_desc_t* ts2mfu_desc, uint32_t data_type)
{
    uint32_t present_AU_num = 0;
#if defined(INC_ALIGNER)
    uint32_t num_frames;
    uint32_t k = 0;
    //2 New Video Aligner Code
    if(accum->aligner_ctxt->process_video_pkts(accum->aligner_ctxt, data, \
    	mstr_pkts, mstr_npkts, accum->spts, &accum->video_frame_tbl, &num_frames) 
    	!= aligner_success){
    	printf("Failed to create alginer video table\n");
	return VPE_ERROR;
    }

    //update num frames
    if(ts2mfu_desc->num_video_AU != num_frames){
    	ts2mfu_desc->mfu_raw_box->videofg.unit_count = num_frames;
	ts2mfu_desc->num_video_AU = num_frames;
    }
    
    if(glob_print_aligner_output){
	FILE *fp;
	char fname[50]={'\0'};
	uint32_t len;

	int32_t rc = mkdir("debuglog/", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
	if ((rc < 0) && (errno != EEXIST)) {
		printf("mkdir failed 1: \n");
	}
	
	len = 	snprintf(NULL,0,"debuglog/VFlog_st%d_%d.txt",accum->mfu_hdr.stream_id,
		accum->mfu_seq_num);
	snprintf(fname,len+1,"debuglog/VFlog_st%d_%d.txt",accum->mfu_hdr.stream_id,
		accum->mfu_seq_num);	
	fp = fopen(fname, "wb");
	if(fp != NULL){
	    fprintf(fp,"Total Num Frames %u\n", num_frames);
	    if(accum->aligner_ctxt->print_frameinfo(
		    accum->aligner_ctxt, accum->video_frame_tbl,
		    fp, accum) != aligner_success){
		    printf("Unable to print video frame log file\n");
	    }
	    fclose(fp);
	}
    }


    video_frame_info_t *cur_frame;

    cur_frame = accum->video_frame_tbl;
    while(cur_frame){
    	
    	//2 calc overall video frames size
    	ts2mfu_desc->vdat_size += cur_frame->frame_len;

	//2 update PTS, DTS
	ts2mfu_desc->vid_PTS[present_AU_num] = cur_frame->pts;
	ts2mfu_desc->vid_DTS[present_AU_num] = cur_frame->dts;
	if(!cur_frame->dts)
	    ts2mfu_desc->vid_DTS[present_AU_num] = cur_frame->pts;

	//2 update individual frame size
	update_frame_size(
		cur_frame->frame_len, present_AU_num,
		data_type, ts2mfu_desc);

	if(ts2mfu_desc->Is_sps_pps == 0 && cur_frame->num_codec_info_pkt){
	  uint32_t temp_size = 0;
	  for(k=0; k<cur_frame->num_codec_info_pkt; k++) {
	    temp_size +=cur_frame->codec_info_len[k];
	  }
	  ts2mfu_desc->mfu_raw_box->videofg.codec_info_size = temp_size;
	  ts2mfu_desc->mfu_raw_box->videofg.codec_specific_data =
	    (uint8_t*)nkn_calloc_type(1,
				      ts2mfu_desc->mfu_raw_box->videofg.codec_info_size,
				      mod_vpe_mfp_ts2mfu_t);
	  if (ts2mfu_desc->mfu_raw_box->videofg.codec_specific_data == NULL) {
	    #if !defined(CHECK)
	DBG_MFPLOG("TS2MFU", ERROR, MOD_MFPLIVE, "Couldn't allocate memory for vid codec struct");
	#endif
	return VPE_ERROR;
	  }
	  uint32_t temp_offset = 0;
	  for(k=0; k<cur_frame->num_codec_info_pkt; k++) {
	    memcpy((ts2mfu_desc->mfu_raw_box->videofg.codec_specific_data+temp_offset),
		   cur_frame->codec_info_start_offset[k], cur_frame->codec_info_len[k]);
	    temp_offset = cur_frame->codec_info_len[k];
	  }
	  ts2mfu_desc->Is_sps_pps =1;
	}
	if(cur_frame->discontinuity_indicator == 1){
#if !defined(CHECK)		    
	    DBG_MFPLOG("TS2MFU", ERROR, MOD_MFPLIVE, "Vidio - missing continuity counter");
#endif
	    return VPE_ERROR;			
	}
	
	present_AU_num += 1;
	cur_frame = cur_frame->next;    	
    }
    assert(ts2mfu_desc->vdat_size > 0);

    //TODO  handle continuity counter failure check

    if (present_AU_num == 0) {
	ts2mfu_desc->vdat_size = 0;
#if !defined(CHECK)		    
        DBG_MFPLOG("TS2MFU", ERROR, MOD_MFPLIVE, "No Video Data in the chunk");
	NKN_ASSERT(0);
#endif
	return VPE_ERROR;
    }
    



#else
    //Old - video frame identifier design
    uint32_t i=0,pos = 0, payload_pos = 0, k=0;
    uint32_t temp_AU_size = 0;
    uint8_t payload_unit_start_indicator = 0;
    uint64_t PTS_value,DTS_value;
    uint16_t  rpid;
    uint8_t  continuity_counter, prev_continuity_counter, adaptation_field_control;
    int32_t  first_frame_flag = TRUE;
    int32_t ret = VPE_SUCCESS;

    B_(printf("---------------------------------------------------\n"));
    B_(printf("num_ts_pkts %d, data_type %d\n", mstr_npkts, data_type));

    for (i = 0; i < mstr_npkts ; i++) {
	pos = mstr_pkts[i];
	B_(printf("----------------\n"));
	B_(printf("parse ts_header, pkt %d\n", i));

	pos = parse_TS_header(data, pos,
		&payload_unit_start_indicator, &rpid,
		&continuity_counter, &adaptation_field_control);  
	if (pos == 0) {
#if !defined(CHECK)		
	    DBG_MFPLOG("TS2MFU", ERROR, MOD_MFPLIVE, "sync byte - not found");
#endif
	    return VPE_ERROR;
	}

	if(first_frame_flag == FALSE) {
	    //check on continuity counter
	    if((adaptation_field_control == 0x01 ||
			adaptation_field_control == 0x03)) {
		if((prev_continuity_counter + 1) % 16 != continuity_counter){
		    int32_t diff = continuity_counter - prev_continuity_counter;
		    if(diff < 0) {
			diff = 15 + diff;
		    }
		    accum->stats.tot_video_pkt_loss += diff;
#if !defined(CHECK)		    
		    DBG_MFPLOG("TS2MFU", ERROR, MOD_MFPLIVE, "Vidio - missing continuity counter -expected %u, got %u",(prev_continuity_counter + 1) % 16,continuity_counter );
#endif
		    return VPE_ERROR;		
		}

	    }		
	}
	prev_continuity_counter = continuity_counter;

	if(first_frame_flag == TRUE){
	    first_frame_flag = FALSE;
	}

	if (payload_unit_start_indicator == 1) {

	    B_(printf("payload start indicator = 1\n"));
	    //parser PES header and find out PTS value
	    PTS_value = DTS_value = 0;
	    pos = parse_PES_header(data, pos, &PTS_value, &DTS_value,&payload_pos);

	    //parse frame start header for few required info
	    //parse NAL header to find frame start excluding sps and pps
	    ret = ts2mfu_parse_NALs(data+pos,&pos,ts2mfu_desc);
	    if(ret == VPE_ERROR)
	      return ret;
	    pos = pos + ts2mfu_desc->dump_size;
	    temp_AU_size = 0;
	    //first vector is SEI vector and remaing is the IDR frame
	    //copy data to MFU vdat struct
	    if(ts2mfu_desc->is_SEI_extract ==0) {
	      for(k=0; k<ts2mfu_desc->num_SEI; k++) {
		ts2mfu_desc->vdat_vector[ts2mfu_desc->num_vdat_vector_entries].data = data;
		ts2mfu_desc->vdat_vector[ts2mfu_desc->num_vdat_vector_entries].offset = ts2mfu_desc->SEI_pos[k];
		ts2mfu_desc->vdat_vector[ts2mfu_desc->num_vdat_vector_entries].length = ts2mfu_desc->SEI_size[k];
		ts2mfu_desc->num_vdat_vector_entries++;
		ts2mfu_desc->vdat_size += ts2mfu_desc->SEI_size[k];
		temp_AU_size +=ts2mfu_desc->SEI_size[k];
	      }
	      //ts2mfu_desc->num_SEI = 0;
	      //ts2mfu_desc->is_SEI_extract = 1;
	    }
	    B_(printf("incr the frame counter\n"));
	    present_AU_num++;	    
	}


	//if no PES header, add the available payload to frame size
	temp_AU_size += (TS_PKT_SIZE - (pos - mstr_pkts[i]));
	C_(printf("video data, update PTS, calc frame size\n"));

	if (present_AU_num > 0) {

	    //copy PTS/DTS value to mfu struct
	    //We shoudl be adding a if (payload_unit_start_indicator == 1) here:
	    ts2mfu_desc->vid_PTS[present_AU_num-1] = PTS_value;
	    ts2mfu_desc->vid_DTS[present_AU_num-1] = DTS_value;
	    if(!DTS_value)	    /*DTS = PTS if DTS = 0*/
		ts2mfu_desc->vid_DTS[present_AU_num-1] = PTS_value;

	    //update frame size in mfu struct
	    C_(printf("update frame size\n"));
	    update_frame_size(temp_AU_size, present_AU_num-1, data_type, ts2mfu_desc);
	}
	/* update the raw data */
	B_(printf("video - update raw data\n"));

	//copy data to MFU vdat struct
	ts2mfu_desc->vdat_vector[ts2mfu_desc->num_vdat_vector_entries].data = data;
	ts2mfu_desc->vdat_vector[ts2mfu_desc->num_vdat_vector_entries].offset = pos;
	ts2mfu_desc->vdat_vector[ts2mfu_desc->num_vdat_vector_entries].length = (TS_PKT_SIZE - (pos - mstr_pkts[i]));
	ts2mfu_desc->num_vdat_vector_entries++;	
	ts2mfu_desc->vdat_size += (TS_PKT_SIZE - (pos - mstr_pkts[i]));

    }

    if (present_AU_num == 0) {
#if !defined(CHECK)    	
	DBG_MFPLOG("TS2MFU", ERROR, MOD_MFPLIVE,  "Video - There is no payload start_indicator");
#endif
	ts2mfu_desc->vdat_size = 0;
	return VPE_ERROR;
    }
#endif

    return VPE_SUCCESS;
}

static int32_t translate_audio_vector(
	ts2mfu_desc_t* ts2mfu_desc){

	uint32_t prog, lay;
	uint32_t strmCnt;
	uint32_t chunkCnt;
	//uint32_t i;
	//uint8_t *base_addr;
	//uint32_t payload_len;
	uint32_t mux_cnt = 0;
	struct LATMContext *latmctx;
	uint32_t subFrameIdx;

	for(mux_cnt = 0; mux_cnt < ts2mfu_desc->num_latmmux_elements; mux_cnt++){
		latmctx = ts2mfu_desc->latmctx[mux_cnt];
		
		for(subFrameIdx = 0; subFrameIdx < latmctx->numSubFrames + 1; subFrameIdx++){
			if(latmctx->allStreamSameTimeFraming){
				  for(prog = 0; prog <= latmctx->numPrograms; prog++){
					  for(lay = 0; lay <= latmctx->numLayer[prog]; lay++){
						strmCnt = latmctx->streamID[prog][lay];
						if(0){//latmctx->m4a_dec_si[0]){
							//if audio specific config present
							ts2mfu_desc->adat_vector[ts2mfu_desc->num_adat_vector_entries].ascdata =
								latmctx->m4a_dec_si[0]->ascData;
							ts2mfu_desc->adat_vector[ts2mfu_desc->num_adat_vector_entries].asclength = 
								latmctx->m4a_dec_si[0]->ascLen;
							ts2mfu_desc->mfu_raw_box->audiofg.sample_sizes[ts2mfu_desc->num_adat_vector_entries] =
								latmctx->m4a_dec_si[0]->ascLen;
							ts2mfu_desc->adat_size += latmctx->m4a_dec_si[0]->ascLen;
						}
						ts2mfu_desc->adat_vector[ts2mfu_desc->num_adat_vector_entries].data =
							latmctx->audio_vector[subFrameIdx][strmCnt].data;
						ts2mfu_desc->adat_vector[ts2mfu_desc->num_adat_vector_entries].offset =
							latmctx->audio_vector[subFrameIdx][strmCnt].offset;
						ts2mfu_desc->adat_vector[ts2mfu_desc->num_adat_vector_entries].length=
							latmctx->audio_vector[subFrameIdx][strmCnt].length;
						ts2mfu_desc->mfu_raw_box->audiofg.sample_sizes[ts2mfu_desc->num_adat_vector_entries]
							= uint32_byte_swap(ts2mfu_desc->mfu_raw_box->audiofg.sample_sizes[ts2mfu_desc->num_adat_vector_entries]+ 
							latmctx->audio_vector[subFrameIdx][strmCnt].length);
						ts2mfu_desc->adat_size += latmctx->audio_vector[subFrameIdx][strmCnt].length;
						ts2mfu_desc->num_adat_vector_entries++;
					  }
				  }
			}else{
				for(chunkCnt = 0; chunkCnt <= latmctx->numChunks; chunkCnt++){
					prog = latmctx->progCIndx[chunkCnt];
					lay = latmctx->layCIndx[chunkCnt];
					strmCnt = latmctx->streamID[prog][lay];
					if(latmctx->m4a_dec_si[0]){
						//if audio specific config present
						ts2mfu_desc->adat_vector[ts2mfu_desc->num_adat_vector_entries].ascdata =
							latmctx->m4a_dec_si[0]->ascData;
						ts2mfu_desc->adat_vector[ts2mfu_desc->num_adat_vector_entries].asclength = 
							latmctx->m4a_dec_si[0]->ascLen;
						ts2mfu_desc->mfu_raw_box->audiofg.sample_sizes[ts2mfu_desc->num_adat_vector_entries] =
							latmctx->m4a_dec_si[0]->ascLen;
						ts2mfu_desc->adat_size += latmctx->m4a_dec_si[0]->ascLen;
					}
					ts2mfu_desc->adat_vector[ts2mfu_desc->num_adat_vector_entries].data = 
						latmctx->audio_vector[subFrameIdx][strmCnt].data;
					ts2mfu_desc->adat_vector[ts2mfu_desc->num_adat_vector_entries].offset =
						latmctx->audio_vector[subFrameIdx][strmCnt].offset;
					ts2mfu_desc->adat_vector[ts2mfu_desc->num_adat_vector_entries].length =
						latmctx->audio_vector[subFrameIdx][strmCnt].length;
					ts2mfu_desc->mfu_raw_box->audiofg.sample_sizes[ts2mfu_desc->num_adat_vector_entries]
						= uint32_byte_swap(ts2mfu_desc->mfu_raw_box->audiofg.sample_sizes[ts2mfu_desc->num_adat_vector_entries] + 
						latmctx->audio_vector[subFrameIdx][strmCnt].length);
					ts2mfu_desc->adat_size += latmctx->audio_vector[subFrameIdx][strmCnt].length;
					ts2mfu_desc->num_adat_vector_entries++;
				}
			}
		}
	}
	return VPE_SUCCESS;
}

static int32_t parse_latm_encapsulated_audio(
	uint8_t *data, 
	uint32_t *slve_pkts, 
	uint32_t slve_npkts,
	ts2mfu_desc_t* ts2mfu_desc){
	
	int32_t ret = VPE_SUCCESS;
	uint32_t present_AU_num = 0;
	uint32_t mux_cnt = 0;
	//uint32_t subfr_cnt = 0;
	//uint32_t strmCnt = 0;
	
	ret = parse_loas_latm(data, slve_pkts, slve_npkts, 
		ts2mfu_desc->latmctx, &ts2mfu_desc->num_latmmux_elements);

	if(ret < 0){
		return VPE_ERROR;
	}
	
	ts2mfu_desc->num_tspkts_retx = ret;

	for(mux_cnt = 0; mux_cnt < ts2mfu_desc->num_latmmux_elements; mux_cnt++){
		present_AU_num += (ts2mfu_desc->latmctx[mux_cnt])->mux_element_rpt->num_valid_strms;		
	}
	translate_audio_vector(ts2mfu_desc);

	if(!present_AU_num){
		//unable to find atleast one audio frame
		ret = VPE_ERROR;
		return(ret);
	}
	
	ts2mfu_desc->num_audio_AU = present_AU_num;
	ts2mfu_desc->sample_freq_index = (ts2mfu_desc->latmctx[0])->m4a_dec_si[0]->sampling_freq_idx;
	return VPE_SUCCESS;
}


static int32_t parse_adts_encapsulated_audio(
	accum_ts_t *accum,
	uint8_t *data, 
	uint32_t *slve_pkts, 
	uint32_t slve_npkts,
	ts2mfu_desc_t* ts2mfu_desc, 
	uint32_t data_type)
{
	int32_t ret = VPE_SUCCESS;
	uint32_t i=0,pos = 0,payload_pos = 0;
    uint32_t present_AU_num = 0,temp_AU_size = 0;
    uint8_t payload_unit_start_indicator = 0;
    uint64_t PTS_value = 0, DTS_value =0;
    int32_t  first_frame_flag = TRUE;
    int32_t  last_frame_flag = FALSE;
    int32_t  FSI_flag = FALSE; //adts frame split indicator flag - 6 bytes of adts frame header is split across ts packets
    int32_t  available_payload = 0;
    int32_t  last_frame_leftover = 0;
    uint8_t  carryfwd_data[5] = { 0 };
    uint32_t carryfwd_index = 0;
    int32_t new_audio_frame_length = 1;
    uint16_t  rpid;
    uint8_t continuity_counter, prev_continuity_counter, adaptation_field_control;

    A_(printf("---------------------------------------------------\n"));
    A_(printf("num_ts_pkts %d, AUDIO chunk\n", slve_npkts));

    for (i = 0; i < slve_npkts ; i++) {
	A_(printf("----------------\n"));
	B_(printf("parse ts_header, pkt %d\n", i));

	//get the starting position of the ts packet
	pos = slve_pkts[i];

	//2 parse ts header
	pos = parse_TS_header(data, pos,
		&payload_unit_start_indicator, &rpid,
		&continuity_counter,
		&adaptation_field_control);

	if (pos == 0) {
#if !defined(CHECK)		
	    DBG_MFPLOG("TS2MFU", ERROR, MOD_MFPLIVE, "sync byte - not found");
#endif
	    return VPE_ERROR;
	}

	//2 check continuity counter 
	if(first_frame_flag == FALSE){
	    //check on continuity counter
	    if((adaptation_field_control == 0x01 ||
			adaptation_field_control == 0x03)) {
		if((prev_continuity_counter + 1) % 16 != continuity_counter){
		    int32_t diff = continuity_counter - prev_continuity_counter;
		    if(diff < 0) {
			diff = 15 + diff;
		    }
		    accum->stats.tot_audio_pkt_loss += diff;			
#if !defined(CHECK)
		    DBG_MFPLOG("TS2MFU", ERROR, MOD_MFPLIVE, "Audio - missing continuity counter -expected %u, got %u",
			    (prev_continuity_counter + 1) % 16,continuity_counter );
#endif
		    return VPE_ERROR;		
		}
	    }
	}
	prev_continuity_counter = continuity_counter;

	//variable to check continuity counter across 2 fragments in sanity_check()
	ts2mfu_desc->curr_chunk_frame_end = (uint32_t) continuity_counter;


	//2 parser PES header 
	if (payload_unit_start_indicator == 1) {
	    B_(printf("payload start indicator = 1\n"));
	    pos = parse_PES_header(data, pos, &PTS_value, &DTS_value, 
		    &payload_pos);
	    B_(printf("PTS value %u\n", (uint32_t)PTS_value));
	}

	//2 parse adts frame start
	new_audio_frame_length = parse_ADTS_header(data,&pos,
		&last_frame_leftover, slve_pkts[i], &FSI_flag,  carryfwd_index,
		carryfwd_data, ts2mfu_desc, first_frame_flag, i);
	B_(printf("adts_fs: dump_size %d, last frame leftover %d, new_audio_frame_length %d\n",
		    ts2mfu_desc->dump_size, last_frame_leftover, new_audio_frame_length));


	//2 first frame in chunk.
	//for other frames in a chunk, dump_size is don't care
	if(first_frame_flag == TRUE) {
	    B_(printf("first frame flag\n"));
	    pos = pos + ts2mfu_desc->dump_size + last_frame_leftover;
	    last_frame_leftover = 0;
	    if(i == 0){
		//want the first ts packet continuity counter only
		ts2mfu_desc->curr_chunk_frame_start = (uint32_t)continuity_counter;
	    }
	}

	//2 calc available payload 
	available_payload = (TS_PKT_SIZE - (pos - slve_pkts[i]));

	//2 last frame in a chunk
	if(i == (slve_npkts -1)) {
	    B_(printf("last frame flag\n"));
	    last_frame_flag = TRUE;
	    if(last_frame_leftover < 0) {
		last_frame_leftover = 0;
	    }
	    ts2mfu_desc->dump_size += last_frame_leftover; 
	}

	//2 find adts header that is split across ts packets
	if(new_audio_frame_length == 0) {
	    //new frame len will be zero if there is no adts header or header is split across ts packets
	    FSI_flag = find_adts_header_split((data+pos)+ last_frame_leftover, 
		    available_payload - last_frame_leftover, 
		    &carryfwd_index, carryfwd_data);
	    if(FSI_flag == TRUE) {
		B_(printf("find_adts_header_split,FSI_flag TRUE,carryfwd_index %d\n",carryfwd_index));
	    }
	} 

	//2 frame size calc
	temp_AU_size += new_audio_frame_length;



	//2 calc last frame leftover
	{
	    last_frame_leftover = new_audio_frame_length - available_payload + 
		last_frame_leftover;
	    if((data[pos] != 0xff) && (first_frame_flag == TRUE) && (new_audio_frame_length > 0)){
		//case where carrfwd data is snipped to 4 bytes instead of 5 bytes
		last_frame_leftover--;
	    }
	    B_(printf("last_frame_leftover %d\n",last_frame_leftover));
	    if(last_frame_flag == TRUE){
		if(last_frame_leftover > 0){
		    //incomplete frame - check whether frame header split has happened
		    if(new_audio_frame_length == 0) {
			present_AU_num--;
		    }
		}else if(last_frame_leftover == 0){
		    //complete frame, no need for any re-transmission
		    ts2mfu_desc->num_tspkts_retx = i + 1;
		}else if(last_frame_leftover < 0){
		    //previous frame is complete, part of next frame exist
		    if(ts2mfu_desc->num_tspkts_retx != i)
			ts2mfu_desc->num_tspkts_retx += 1;
		    ts2mfu_desc->last_pkt_end_offset = 
		    	TS_PKT_SIZE + last_frame_leftover -carryfwd_index - new_audio_frame_length;
		}
	    }
	}

	//2 update output data
	if((last_frame_flag == FALSE && new_audio_frame_length > 0) || 
		(last_frame_flag == TRUE && last_frame_leftover <= 0 
		 && new_audio_frame_length > 0)) {

	    if(last_frame_flag == TRUE && last_frame_leftover <= 0\
			&& new_audio_frame_length > 0){
	    B_(printf("audio frame num %d updating PTS value\n",
					       present_AU_num));
	    ts2mfu_desc->dump_size += new_audio_frame_length;
	    //ts2mfu_desc->last_pkt_start_offset = ts2mfu_desc->last_pkt_end_offset;
	    ts2mfu_desc->last_pkt_end_offset += new_audio_frame_length;
	    }

	    ts2mfu_desc->aud_PTS[present_AU_num]= PTS_value;
	    //C_(printf("update frame size\n"));
	    update_frame_size(temp_AU_size, present_AU_num, data_type, 
		    ts2mfu_desc);
	    temp_AU_size = 0;
	    //B_(printf("incr the frame counter\n"));
	    present_AU_num++;	    
	} 

	//3 Spl case: No adts header in first ts packet
	if(first_frame_flag == TRUE){
	    //first tspacket needn't contain an adts frame start;
	    //treat all tspacket till we met with adts frame as first frame only
	    if(new_audio_frame_length != 0) {
		first_frame_flag = FALSE;
	    }		
	}

	//2 vector copy raw data
	if(first_frame_flag == FALSE) {
	    if(update_raw_data(ts2mfu_desc, data, pos, &last_frame_flag, 
			slve_pkts[i]) != VPE_SUCCESS) {
		return VPE_ERROR;
	    }
	    B_(printf("ts2mfu_desc->adat_vector[%d].length %d\n", 
			ts2mfu_desc->num_adat_vector_entries-1, 
			ts2mfu_desc->adat_vector[ts2mfu_desc->num_adat_vector_entries-1].length));
	}

	//2 get next packet
	pos = slve_pkts[i] + TS_PKT_SIZE;

handle_more_frms_ts_pkt:
	//2 Handle 2 adts frames in a ts packet
	if(last_frame_leftover < 0 && FSI_flag == FALSE && last_frame_flag == FALSE) {
	    //no issues if last frame leftover >=0
	    //if last frame_leftover < 0, make it 0, new frame is starting here
	    // whole/part of frame start 6 bytes present in this header
	    //update the frame size in mfu struct
	    B_(printf("handling 2frames in a tspacket\n"));
	    available_payload = -last_frame_leftover;

	    //parse ts packet to find the new adts frame start
	    new_audio_frame_length = parse_ADTS_header(data,&pos,
						       &last_frame_leftover, slve_pkts[i],
						       &FSI_flag,  
						       carryfwd_index,
						       carryfwd_data,
						       ts2mfu_desc, first_frame_flag, i);
	    B_(printf("2fints: dumpsize %d, last_frame_leftover %d, pos %d, new_audio_frame_length %d\n",
			ts2mfu_desc->dump_size,last_frame_leftover, pos, new_audio_frame_length));

	    if(i == (slve_npkts -1)) {
		last_frame_flag = TRUE;
	    }

	    last_frame_leftover = -last_frame_leftover;

	    if(new_audio_frame_length == 0) {
		//new frame len will be zero if there is no adts header or header is split
		//across ts packets
		FSI_flag = find_adts_header_split((data+pos)- last_frame_leftover, available_payload, 
			&carryfwd_index, carryfwd_data);
		if(FSI_flag == TRUE) {
		    B_(printf("find_adts_header_split,FSI_flag TRUE,carryfwd_index %d\n",carryfwd_index));
		    last_frame_leftover = -carryfwd_index;
		}
	    } 


	    //update frame size in mfu struct
	    if(last_frame_flag == FALSE && new_audio_frame_length > 0) {
		B_(printf("audio frame num %d updating PTS value\n",present_AU_num));
		ts2mfu_desc->aud_PTS[present_AU_num]= PTS_value;
		C_(printf("update frame size\n"));
		update_frame_size(new_audio_frame_length, present_AU_num, data_type, ts2mfu_desc);
		temp_AU_size = 0;
		B_(printf("incr the frame counter\n"));
		present_AU_num++;	
	    } 
	    if(new_audio_frame_length != 0) {
		//calc last frame leftover
		last_frame_leftover = new_audio_frame_length - available_payload ;
	    if(last_frame_leftover < 0){
	    	    B_(printf("third frame in same packet\n"));
		    goto handle_more_frms_ts_pkt;
	    }
	    }	    
	}	


	A_(printf("num_tspkts_retx %u and start_off %u : end_off %u\n",
		ts2mfu_desc->num_tspkts_retx, ts2mfu_desc->last_pkt_start_offset, 
		ts2mfu_desc->last_pkt_end_offset));

    }

    if (present_AU_num == 0) {
#if !defined(CHECK)    	
	DBG_MFPLOG("TS2MFU", ERROR, MOD_MFPLIVE,  "No Audio Data in the chunk");
#endif
		ts2mfu_desc->adat_size = 0;
#if !defined(CHECK)    	
		NKN_ASSERT(0);
#endif
		return VPE_ERROR;
    }

    if(present_AU_num < 4){
#if !defined(CHECK)
		DBG_MFPLOG("TS2MFU", ERROR, MOD_MFPLIVE,  "Audio - There should be atleast 4 audio samples\n");
		NKN_ASSERT(0);
#endif
		return VPE_ERROR;
    }

    /* update the  vdat_size/adat_size */
    if (present_AU_num > 0) {
		//calculate the num of packets that are necessary to be re-transmitted
		//subtract the total audio packets (input) - valid audio packets copied to output
		//before this line of code, ts2mfu_desc->num_tspkts_retx holds the packet number
		//upto which the our code copied the entire content
		ts2mfu_desc->num_tspkts_retx = slve_npkts - ts2mfu_desc->num_tspkts_retx;
		ts2mfu_desc->num_audio_AU = present_AU_num;

		if(ts2mfu_desc->curr_chunk_frame_end < ts2mfu_desc->num_tspkts_retx){
		    ts2mfu_desc->curr_chunk_frame_end = ts2mfu_desc->curr_chunk_frame_end 
			+ 16 - ts2mfu_desc->num_tspkts_retx;	
		}else{
		    ts2mfu_desc->curr_chunk_frame_end -= ts2mfu_desc->num_tspkts_retx;	
		}
		B_(printf("calc ts2mfu_desc->adat_size %d\n",ts2mfu_desc->adat_size));
		A_(printf("audio_sample_cnt %u/ num_tspkts_retx %u/ start_off %u/ end_off %u\n", 
			    ts2mfu_desc->num_audio_AU, ts2mfu_desc->num_tspkts_retx, 
			    ts2mfu_desc->last_pkt_start_offset, ts2mfu_desc->last_pkt_end_offset));
    }

	return(ret);
}	

/* 
 * This function parses the ts file and updates PTS,DTS and
 * sample_sizes in ts2mfu desc - audio data only
 * data -> i/p pointer ponintg to ts file
 * data_size -> i/p size of the data
 * ts2mfu_desc -> o/p pointer pointing to ts2mfu desc
 * data_type -> i/p either video or audio data type
 */

static int32_t mfu_converter_get_raw_audio_box(
					       accum_ts_t *accum,
					       uint8_t *data, uint32_t *slve_pkts, uint32_t slve_npkts,
					       ts2mfu_desc_t* ts2mfu_desc, uint32_t data_type)
{
	int32_t ret = VPE_SUCCESS;

	if(glob_latm_audio_encapsulation_enabled){
		ret = parse_latm_encapsulated_audio(data, slve_pkts, slve_npkts, ts2mfu_desc);
	}else{
		ret = parse_adts_encapsulated_audio(accum, data, slve_pkts, slve_npkts,
				ts2mfu_desc, data_type);
	}
    return (ret);
}


/*
   This function is used to get the DTS from the ts pkt
 * pkt -> i/p pointer pointing to ts pkt
 * rpid ->o/p updates the pid of the ts pkt
 * frame_type-> o/p updates the frame_type of the ts pkt
 */
int32_t mfu_converter_get_timestamp(
	uint8_t *pkt, 
	int32_t *frame_type, 
	uint16_t* rpid, 
	uint32_t *pts, 
	uint32_t *dts, 
	uint32_t (*handle)(uint8_t *, uint32_t *))
{
    uint8_t payload_unit_start_indicator;//,adaptation_field_control,PTS_DTS_flags,sync_byte;
    //uint64_t PTS_temp1,PTS_temp2,PTS_temp3,PTS_temp4,PTS_temp5,
    uint64_t PTS_value=0,DTS_value=0;
    uint32_t pos =0;//,adaptation_field_len;
    uint8_t *data;
    uint32_t payload_pos;
    uint8_t continuity_counter, adaptation_field_control;
    int32_t i_pos = 0;

    data= pkt;
    if(pts)
	*pts = 0;
    if(dts)
	*dts = 0;

    pos = parse_TS_header(data, pos, &payload_unit_start_indicator,
	    rpid, &continuity_counter, &adaptation_field_control);
    if (pos == 0) {
#if !defined(CHECK)    	
	DBG_MFPLOG("TS2MFU", ERROR, MOD_MFPLIVE, "sync byte - not found");
#endif
	//assert(0);
	return -E_VPE_NO_SYNC_BYTE;
    }

    if (payload_unit_start_indicator == 1) {
	pos = parse_PES_header(data, pos, &PTS_value, &DTS_value, &payload_pos);
	if(pos == 0xffffffff) {
	    return -1;
	}

	if(frame_type){
	    *frame_type = 0;
	    if(handle( data+payload_pos,&payload_pos) == 1)
		*frame_type =1;
	}
	/*When DTS not present or DTS=0 use PTS*/
	if(pts)
	    *pts = PTS_value;
	if(dts)
	    *dts = DTS_value;
	if(*dts==0)
	    *dts = PTS_value;
    }
    i_pos = (int32_t)pos;
    return (i_pos);//VPE_SUCCESS;
}

#if !defined(INC_ALIGNER)
/* 
 * This function is used to parse the ts pkt and updates the sps, pps
 * info
 * data -> i/p pointer pointing to ts pkt
 * pos -> i/p indicates the pos in the ts pkt
 * ts2mfu_desc -> o/p updates the sps, pps info in ts2mfu desc 
 */
static uint32_t ts2mfu_parse_NALs(uint8_t* data, uint32_t *pos,ts2mfu_desc_t *ts2mfu_desc){
  uint32_t ret = 0,nal_size =0;
  uint8_t *nal_data=NULL;
  int nal_type=0,sc_type=0;
  uint32_t sps_pos=0,pps_pos=0;
  uint32_t orig_pos=0;
  uint8_t *orig_data=NULL;
  int32_t err = VPE_SUCCESS;
  
  //if(*pos > TS_PKT_SIZE) {
  //return 0xffffffff;
  //}
  orig_pos=*pos;
  orig_data = data;
  ts2mfu_desc->dump_size = 0;
  ts2mfu_desc->num_SEI = 0;
  /**/
  do{
    sc_type = find_start_code(data);
    //printf("sc_type %d\n", sc_type);
    data+=sc_type;
    *pos=(*pos)+sc_type;
    nal_size = get_nal_mem(data, 188-(*pos%188));
    nal_data = data;
    data+=nal_size;
    *pos=(*pos)+nal_size;
    nal_type = get_nal_type(nal_data, nal_size);

    switch(nal_type){
    case AVC_NALU_ACCESS_UNIT:
      ts2mfu_desc->dump_size += sc_type + nal_size;
      ret =1;
      break;
    case AVC_NALU_SEI:
      if(ts2mfu_desc->is_SEI_extract ==0) {
	ts2mfu_desc->SEI_size[ts2mfu_desc->num_SEI] = nal_size+sc_type;
	ts2mfu_desc->SEI_pos[ts2mfu_desc->num_SEI] = *pos - ts2mfu_desc->SEI_size[ts2mfu_desc->num_SEI];
	ts2mfu_desc->dump_size += sc_type + nal_size;
	ts2mfu_desc->num_SEI++;
	ts2mfu_desc->tot_num_SEI++;
	if(ts2mfu_desc->tot_num_SEI >= TOT_MAX_SEI) {
	  err = VPE_ERROR;
	  ret = 0;
	  break;
	}

      }
      ret = 1;
      break;
    case AVC_NALU_SEQ_PARAM:
      ts2mfu_desc->sps_size = nal_size+sc_type;
      //sps_pos = orig_pos + ts2mfu_desc->dump_size;
      sps_pos = *pos -orig_pos - ts2mfu_desc->sps_size;
      
      ts2mfu_desc->dump_size += sc_type + nal_size;
      ret = 1;
      break;
    case AVC_NALU_PIC_PARAM:
      ts2mfu_desc->pps_size = nal_size+sc_type;
      //pps_pos = orig_pos + ts2mfu_desc->dump_size;
      pps_pos = *pos - orig_pos - ts2mfu_desc->pps_size;
      ts2mfu_desc->dump_size += sc_type + nal_size;
      ret = 1;
      break;
      
    default:
      ret= 0;
      break;
    }
  }while(ret==1);
  data = orig_data;
    /* update the sps and pps value only for first time */
  if(ts2mfu_desc->Is_sps_pps ==0){
    ts2mfu_desc->mfu_raw_box->videofg.codec_info_size =
      ts2mfu_desc->sps_size +
      ts2mfu_desc->pps_size;
    ts2mfu_desc->mfu_raw_box->videofg.codec_specific_data =
	    (uint8_t*)nkn_malloc_type(sizeof(uint8_t)*
				      ts2mfu_desc->mfu_raw_box->videofg.codec_info_size,
				      mod_vpe_mfp_ts2mfu_t);
    if(ts2mfu_desc->mfu_raw_box->videofg.codec_specific_data == NULL){
      #if !defined(CHECK)
	DBG_MFPLOG("TS2MFU", ERROR, MOD_MFPLIVE, "Couldn't allocate memory for vid codec struct");
      #endif
      return VPE_ERROR;
    }
    memcpy((ts2mfu_desc->mfu_raw_box->videofg.codec_specific_data),
	   (data+sps_pos), ts2mfu_desc->sps_size);
    memcpy((ts2mfu_desc->mfu_raw_box->videofg.codec_specific_data +
	    ts2mfu_desc->sps_size),
	   (data+pps_pos), ts2mfu_desc->pps_size);
    ts2mfu_desc->Is_sps_pps =1;
  }
  *pos = orig_pos;
  return err;
}
#endif

