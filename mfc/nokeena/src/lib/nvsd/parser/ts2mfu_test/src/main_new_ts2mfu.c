#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <getopt.h>

#include "nkn_vpe_types.h"
#include "nkn_vpe_mp4_parser.h"
#include "nkn_vpe_mfp_ts.h"
#include "nkn_mem_counter_def.h"
#include "nkn_vpe_mfp_ts2mfu.h"
#include "mfp_video_aligner.h"
#include "ts_parser.h"
extern uint32_t glob_latm_audio_encapsulation_enabled;

#define MAX_SPS_PPS_SIZE 300
//declaration

//static ts_desc_t* init(char *, mfub_t);
int32_t parse_mfu(unit_desc_t* , unit_desc_t* ,uint8_t *,uint32_t,
		  uint8_t *, uint32_t *,uint32_t *,uint32_t *,
		  mfub_t*);
int32_t free_vmd_amd(unit_desc_t* , unit_desc_t*);
//writting mfu
#if 1

extern uint32_t glob_enable_mfu_sanity_checker;
extern uint32_t glob_print_aligner_output;
extern uint32_t glob_latm_audio_encapsulation_enabled;

int32_t main(int argc, char *argv[]){

    FILE* audio_file_fp=NULL;
    FILE* video_file_fp = NULL;
    FILE *out_file_fp = NULL;
    uint32_t i, offset, alternate_flag, j, k , eof;
    char *audio_file_name,*video_file_name, *out_file_name;
    uint32_t aud_data_size=0,vid_data_size=0;
    uint8_t *data =NULL;
    uint32_t *mstr_pkts, *slve_pkts, mstr_npkts, slve_npkts;    
    mfu_data_t* mfu_data;
    mfub_t *mfubox_header;
    ts2mfu_cont_t ts2mfu_cont;
    accum_ts_t accum;
    spts_st_ctx_t spts;

    glob_print_aligner_output = 1;
    glob_latm_audio_encapsulation_enabled = 1;

    memset(&accum, 0, sizeof(accum_ts_t));
    memset(&spts, 0, sizeof(spts_st_ctx_t));
    memset(&ts2mfu_cont, 0, sizeof(ts2mfu_cont_t));
    glob_enable_mfu_sanity_checker = 1;
    glob_latm_audio_encapsulation_enabled = 1;

    if(argc == 1){
    	printf("./ts2mfu <audio_ts> <video_ts> <mfu_to_write>\n");
	return 1;
    }

    audio_file_name = argv[1];
    audio_file_fp = fopen(audio_file_name,"rb");
    if(audio_file_fp != NULL){
	fseek(audio_file_fp,0,SEEK_END);
	aud_data_size= ftell(audio_file_fp);
	fseek(audio_file_fp,0,SEEK_SET);
    }

    video_file_name = argv[2];
    video_file_fp = fopen(video_file_name,"rb");
    if(video_file_fp != NULL){
	fseek(video_file_fp,0,SEEK_END);
	vid_data_size= ftell(video_file_fp);
	fseek(video_file_fp,0,SEEK_SET);
    }

    out_file_name = argv[3];
    out_file_fp = fopen(out_file_name,"wb");
    if(out_file_fp == NULL){
	printf("failed to open out file\n");
	return -1;
    }
    data = (uint8_t*)malloc(sizeof(uint8_t) * (aud_data_size + vid_data_size));
    if(data == NULL){
    printf("failed to allocate mem for data %d\n", aud_data_size + vid_data_size);
    return -1;
    }

    accum.data = data;
    accum.spts = &spts;
    
#if defined(INC_ALIGNER)
	/*create aligner*/
	if(create_aligner_ctxt(&accum.aligner_ctxt) != aligner_success){
	printf("failed to create aligner ctxt\n");
	return -1;
	}
#endif

    mstr_npkts = vid_data_size / TS_PKT_SIZE;
    slve_npkts = aud_data_size / TS_PKT_SIZE;

    mstr_pkts = (uint32_t *) calloc(mstr_npkts, sizeof(uint32_t));
    if(mstr_pkts == NULL) {
    	printf("Error allocating memory for mstr_pkts\n");
	return -1;
    }
    
    slve_pkts = (uint32_t *) calloc(slve_npkts, sizeof(uint32_t));
    if(slve_pkts == NULL) {
    	printf("Error allocating memory for slve_pkts\n");
	return -1;
    }

    offset = 0; 
    alternate_flag = 1;
    eof = 0;
    j = 0;
    k = 0;
    
    for (i = 0; i <= (slve_npkts + mstr_npkts); i++) {
	if(!feof(audio_file_fp) && alternate_flag == 1 && j < slve_npkts){
		fread(data + offset,  TS_PKT_SIZE, 1, audio_file_fp);
		slve_pkts[j++] = offset;	
		offset += TS_PKT_SIZE;
		if(eof == 0) alternate_flag = 0;
		if(feof(audio_file_fp)) 
		{	
			eof = 1;
		}
	} else if(!feof(video_file_fp) && k < mstr_npkts){
		fread(data + offset,  TS_PKT_SIZE, 1, video_file_fp);
		mstr_pkts[k++] = offset;	
		offset += TS_PKT_SIZE;
		if(eof == 0)alternate_flag = 1;
		if(feof(video_file_fp)) 
		{
			eof = 1;
		}
	}
    }
    
    mfubox_header = &accum.mfu_hdr;
    mfubox_header->version = 0;
    mfubox_header->program_number = 0;
    mfubox_header->stream_id = 0;
    mfubox_header->flags = 4;
    mfubox_header->timescale = 90000;
    mfubox_header->video_rate = 500;
    mfubox_header->audio_rate = 500;

    uint8_t psi;
    uint8_t cc, adap;
    parse_TS_header(accum.data+mstr_pkts[0], 0, &psi, &accum.spts->mux.video_pid, &cc, &adap);
    parse_TS_header(accum.data+slve_pkts[0], 0, &psi, &accum.spts->mux.audio_pid1, &cc, &adap);

    //generate mfu box
    mfu_data = mfp_ts2mfu(mstr_pkts, mstr_npkts, slve_pkts, slve_npkts, &accum, &ts2mfu_cont);

    //write mfu_data to file
    if(mfu_data != NULL){
    fwrite(mfu_data->data, sizeof(uint8_t), mfu_data->data_size, out_file_fp);
    }



    if(audio_file_fp != NULL){
	fclose(audio_file_fp);
    }

    if(video_file_fp != NULL){
        fclose(video_file_fp);
    }

    if(out_file_fp != NULL){
	fclose(out_file_fp);
    }

    if(data != NULL){
       free(data);
       data = NULL;
    }    

    if(mstr_pkts != NULL){
       free(mstr_pkts);
       mstr_pkts = NULL;
    }    

    if(slve_pkts != NULL){
       free(slve_pkts);
       slve_pkts = NULL;
    }    


    if( mfu_data != NULL) {
	if(mfu_data->data != NULL)
	    free(mfu_data->data);
	free(mfu_data);
    }
    //fclose(audio_file_fp);

    return 0;
}
#endif


