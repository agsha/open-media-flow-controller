#include <stdio.h>
#include "mfp_video_header.h"
#include "mfp_limits.h"
#include "nkn_vpe_mfp_ts2mfu.h"
#include "mfp_live_file_pump.h"

int usage(void);

static char * convert_slicetype_to_string(slice_type_e type){
	char *res;
	switch(type){
		case 1:
			res = (char*)"I Slice";
			break;
		case 2:
			res = (char*)"P Slice";
			break;
		case 3:
			res = (char*)"B Slice";
			break;
		case 0:
			res = (char*)"Not Assigned";
			break;
		case -1:
			res = (char*)"NeedMoreData";
			break;
	}
	return (res);
}

int main(int argc, char *argv[])
{
    FILE *fp;
	ts_pkt_infotable_t *pkt_tbl;
	
	if(argc < 3){
		usage();
		return (-1);
	}
	
	fp = fopen(argv[1], "rb");
	if(fp == NULL){
		printf("error opening %s\n", argv[1]);
		return (-1);
	}
	
	unsigned int apid = atoi(argv[2]);
	unsigned int vpid = atoi(argv[3]);
	
	int len;
	fseek(fp, 0, SEEK_END);
	len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	
	unsigned int num_ts_pkts = len /BYTES_IN_PACKET;

	unsigned char *data = (unsigned char *) malloc(len * sizeof(char));
	if(data == NULL){
		printf("error alloting memory to data\n");
		return (-1);	
	}
	
	fread(data, len, sizeof(char), fp);
	
	unsigned int *pkt_offsets;
	pkt_offsets = (unsigned int *) malloc(num_ts_pkts * sizeof(int));
	if(pkt_offsets == NULL){
		printf("error alloting memory to pkt_offsets\n");
		return (-1);	
	}
	
	unsigned int i;
	for(i = 0; i < num_ts_pkts; i++){
		pkt_offsets[i] = i * BYTES_IN_PACKET;
	}
	
	create_index_table(
				data, 
				pkt_offsets, 
				num_ts_pkts, 
				apid, 
				vpid, 
				&pkt_tbl);
	
	unsigned int cnti;
	
	for( cnti = 0; cnti < num_ts_pkts; cnti++){
		printf("pkt# %u  framestart %u Type %s pid#%u pts %u\n",
		cnti, pkt_tbl[cnti].frame_start,
		convert_slicetype_to_string(pkt_tbl[cnti].frame_type),
		pkt_tbl[cnti].pid, pkt_tbl[cnti].pts);
	}	
	
	slice_type_e ft;
	unsigned int frame_type_num;
	
	for( cnti = 0; cnti < num_ts_pkts; cnti++){
		if(pkt_tbl[cnti].frame_start){
			ft = find_frame_type(
					num_ts_pkts, 
					cnti,
					vpid, 
					pkt_tbl,
					&frame_type_num);
			printf("pktIndex %u --> %s\n", cnti, convert_slicetype_to_string(ft));
		}
	}
	
	
	
	
	if(pkt_tbl){
		free(pkt_tbl);
		pkt_tbl = NULL;
	}	
	if(pkt_offsets){
		free(pkt_offsets);
		pkt_offsets = NULL;
	}
	if(data){
		free(data);
		data = NULL;
	}	
	
	if(fp){
		fclose(fp);
		fp = NULL;
	}
	return 0;
}


int usage(void){
	printf("./vdtest <TS_PKTS_FILE_NAME> <AUDIO_PID> <VIDEO_PID> \n");
	return 0;
}


