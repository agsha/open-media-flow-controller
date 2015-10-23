#ifndef _MFP_DATA_PAIR_H_
#define _MFP_DATA_PAIR_H_

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "mfp_accum_intf.h"



#ifdef __cplusplus
extern "C" {
#endif

typedef enum FMTR_LIST{
	FMTR_UNDEF = 0,
	APPLE_FMTR = 1,
	SILVERLIGHT_FMTR = 2,
}FMTR_LIST_E;

typedef struct data_pair_element{
	uint32_t mfu_seq_num;
	uint32_t mfu_duration;	
}data_pair_element_t;


typedef struct data_pair_root{
	data_pair_element_t *dpleaf;
	uint32_t wr_index;
	uint32_t max_len;
}data_pair_root_t;

void resetDataPair(data_pair_root_t *dproot);

data_pair_root_t *createDataPair(
	sess_stream_id_t* id,
	uint32_t key_frame_interval);



int32_t enterDataPair( sess_stream_id_t* id, 
			uint32_t seq_num, 
			uint32_t mfu_duration,
			FMTR_LIST_E fmtr);

int32_t
checkDataPair( sess_stream_id_t* id, 
			uint32_t seq_num, 
			uint32_t mfu_duration,
			FMTR_LIST_E fmtr);

uint32_t calc_video_duration(void *mfudata);


#ifdef __cplusplus
}
#endif

#endif
