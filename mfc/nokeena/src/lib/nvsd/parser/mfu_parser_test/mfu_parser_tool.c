#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <getopt.h>

#include "nkn_vpe_types.h"
//#include "nkn_vpe_mfp_ts2mfu.h"
//#include "nkn_vpe_mp4_parser.h"
#include "nkn_vpe_mfp_ts.h"
#include "nkn_vpe_mfu_defs.h"
//#include "mfu2iphone.h"
#include "nkn_vpe_mfu_parser.h"


#define MAX_SPS_PPS_SIZE 300
#define MAJOR_VERSION (0)
#define MINOR_VERSION (1)

void print_usage()
{
    printf("------------------------------\n");
    printf("mfu parser tool: VERSION %d.%d\n", MAJOR_VERSION, MINOR_VERSION);
    printf("Usage: \n");
    printf("mfu_parser -i <INPUT MFU FILE NAME>");
    printf("------------------------------\n");
    return;
}


int main(int argc, const char *argv[]) {

    unit_desc_t *vmd=NULL,*amd = NULL;
    uint8_t *sps_pps = NULL;
    uint32_t sps_pps_size;
    uint8_t *mfu_data_orig;
    uint32_t is_video;
    uint32_t is_audio;
    mfub_t mfu_hdr;
    mfu_data_t *mfu_data;
    uint32_t length, ret;
    const char *audio_file_name;
    FILE *fp;
    int32_t i;
    uint8_t *data_start;
    uint8_t byte1, byte2;

    //check for sanity of arguments to the tool     
    if(argc < 3) {
	print_usage();
	return(VPE_ERROR);	
    }

    audio_file_name = argv[2];
    //read the input file and copy the mfu data to a data struct
    fp = fopen(audio_file_name, "rb");
    if (fp == NULL) {
	printf("Error opening file %s\n", audio_file_name);
	return VPE_ERROR;
    }
    fseek(fp, 0, SEEK_END);
    length = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    printf("length of file %u\n", length);

    mfu_data = (mfu_data_t *) malloc(sizeof(mfu_data_t));

    mfu_data->data_size = length;
    mfu_data->data = (uint8_t *) malloc(length);
    if(mfu_data->data == NULL) {
	printf("Error allocating memory for data\n");
	return VPE_ERROR;
    }

    ret = fread(mfu_data->data, 1, length, fp);
    if(ret != length){
	printf("Error reading data from file; required %u, got %u\n", length, ret);
	return VPE_ERROR;
    }


    //allocate the required memory
    mfu_data_orig = mfu_data->data;
    vmd = (unit_desc_t*)malloc(sizeof(unit_desc_t));
    amd = (unit_desc_t*)malloc(sizeof(unit_desc_t));
    sps_pps = (uint8_t *)malloc(MAX_SPS_PPS_SIZE);


    ret = mfu_parse(vmd, amd, mfu_data_orig, mfu_data->data_size, sps_pps,
	    &sps_pps_size, &is_audio, &is_video, &mfu_hdr);

    if (ret == VPE_ERROR) {
	free(vmd);
	free (amd);
	free(sps_pps);
	return VPE_ERROR;
    }
    if (is_video == 0) {
	free(vmd);
	vmd = NULL;
    }
    if (is_audio == 0) {
	free(amd);
	amd = NULL;
    }

    if(is_audio != 0) {
	data_start = amd->mdat_pos;

	for(i = 0; i < amd->sample_count; i++) {

	    //check whether all sample size are valid and non-zero
	    if( amd->sample_sizes[i] <= 0){
		printf("Err: sample %u: sample size %u\n", i, amd->sample_sizes[i]);
	    }   

	    //check whether all sample duration are valid and non-zero
	    if( amd->sample_duration[i] == 0) {
		//printf("Err: sample %u: sample_duration %u\n", i, amd->sample_duration[i]);
	    }

	    //check whether all audio frame starts with adts header
	    byte1 = *data_start;
	    byte2 = *(data_start + 1);

	    printf("audio sample %d, frame start bytes %x %x\n", i, byte1, byte2);

	    if(byte1 != 0xff || (byte2 & 0xf6) != 0xf0) {
		printf("Err: ADTS header not found in frame start, %x %x\n", byte1, byte2);	       
	    }
	    data_start = data_start + amd->sample_sizes[i];

	}
    }


    printf("Parsing Complete - return SUCCESS\n");
    free(vmd);
    free(amd);
    free(sps_pps);
    return VPE_SUCCESS;

}


