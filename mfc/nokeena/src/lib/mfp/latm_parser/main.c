
#include "common.h"

static void print_usage(void)
{
    printf("------------------------------\n");
    printf("latm parser: VERSION %d.%d\n", MAJOR_VERSION, MINOR_VERSION);
    printf("Usage: \n");
    printf("latm_parse <INPUT_TS> <APID> <OUTPUT_LATM_FILE>\n");
    printf("------------------------------\n");
    return;
}




int main(int argc, char* argv[]){

	char *ts_file_name;
	FILE *fp;
	FILE *outfp;
	
	int32_t length;
	uint16_t reqd_pid;
	
   //check for sanity of arguments to the tool     
    if(argc < 4) {
	print_usage();
	return(VPE_ERROR);	
    }

    uint8_t *buffer = NULL;;
	
    ts_file_name = argv[1];
    
    fp = fopen(ts_file_name, "rb");
    if (fp == NULL) {
	printf("Error opening file %s\n", ts_file_name);
	return VPE_ERROR;
    }
	
    outfp = fopen(argv[3], "wb");
    if (outfp == NULL) {
	printf("Error opening file %s\n", argv[3]);
	return VPE_ERROR;
    }	
	
    fseek(fp, 0, SEEK_END);
    length = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    printf("length of file %u\n", length);

	buffer = (uint8_t *)malloc(length * sizeof(uint8_t));
	if(buffer == NULL){
	printf("unable to allocate mem for buffer\n");
	return VPE_ERROR;
	}

	reqd_pid = atoi(argv[2]);
	fread(buffer, length, sizeof(uint8_t), fp);
	
	uint32_t num_ts_pkts = length / (TS_PKT_SIZE); 
	uint32_t pos;
	uint8_t payload_unit_start_indicator;
	uint16_t rpid;
	uint8_t	continuity_counter;
	uint8_t adaptation_field_control;	
    uint32_t i;
	uint32_t slve_npkts = 0;
	uint8_t *audio_data;

	//find number of audio pkts
	for(i = 0; i < num_ts_pkts; i++){
		pos = i * TS_PKT_SIZE;
		pos = parse_TS_header(buffer, pos, 
			&payload_unit_start_indicator, 
			&rpid, 
			&continuity_counter, &adaptation_field_control);
		if(rpid == reqd_pid){
			slve_npkts++;
		}
	}
	
	//allocate mem for slve_npkts
    audio_data = (uint8_t*)malloc(slve_npkts * TS_PKT_SIZE* sizeof(uint8_t));
	if(audio_data == NULL){
		printf("unable to allocate mem for audio data\n");
		return VPE_ERROR;
	}
	
	//slve_npkts reset
	slve_npkts = 0;

	//copy audio payload to another buffer	
	for(i = 0; i < num_ts_pkts; i++){
		pos = i * TS_PKT_SIZE;
		pos = parse_TS_header(buffer, pos, 
			&payload_unit_start_indicator, 
			&rpid, 
			&continuity_counter, &adaptation_field_control);
		if(rpid == reqd_pid){
			memcpy(audio_data + slve_npkts * TS_PKT_SIZE, 
				buffer + i * TS_PKT_SIZE,
				TS_PKT_SIZE);
			slve_npkts++;
		}
	}

	//calc audio pkts offset
	uint32_t *slve_pkts = (uint32_t *)calloc(slve_npkts,sizeof(uint32_t));
    for(i = 0; i < slve_npkts; i++)
		slve_pkts[i] = i * TS_PKT_SIZE;

	uint32_t num_latmmux_elements = 0;
	struct LATMContext *latmctx[MAX_LATMCHUNKS];	

	//main processing API
	parse_loas_latm(audio_data, slve_pkts, slve_npkts, latmctx, &num_latmmux_elements);

	uint32_t latm_chunk_cnt;
	for(latm_chunk_cnt = 0; latm_chunk_cnt < num_latmmux_elements; latm_chunk_cnt++ ){
		fwrite(	latmctx[latm_chunk_cnt]->mux_element, 
				latmctx[latm_chunk_cnt]->mux_element_wr_pos, 
				sizeof(uint8_t), outfp);		
	}

	free_latmctx_mem(latmctx, num_latmmux_elements);

	if(audio_data){
		free(audio_data);
		audio_data = NULL;
	}	
	if(buffer){
		free(buffer);
		buffer = NULL;
	}
	fclose(outfp);
    fclose(fp);
	return 0;
}

