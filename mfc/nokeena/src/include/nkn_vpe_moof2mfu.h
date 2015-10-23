#ifndef _NKN_VPE_MOOF2MFU__
#define _NKN_VPE_MOOF2MFU__
#include "stdio.h"
#include "nkn_vpe_types.h"
#include "nkn_vpe_mfu_defs.h"
#include "nkn_vpe_ismv_mfu.h"

int32_t
ismv_moof2_mfu(moov_t *, moov_t *, uint8_t* , uint8_t*, uint32_t , uint32_t,
	       ismv_parser_t*, mfu_data_t*);

int32_t
ismv_moof2_mfu_audio_process(moov_t*, uint8_t*, uint32_t,
			     moof2mfu_desc_t*, ismv_parser_t* );

int32_t
ismv_moof2_mfu_video_process(moov_t*, uint8_t*, uint32_t,
			     moof2mfu_desc_t*, ismv_parser_t*);

int32_t 
init_moof2mfu_desc(moof2mfu_desc_t*);
int32_t 
free_moof2mfu_desc(moof2mfu_desc_t*);
void
moof2mfu_clean_mfu_data(mfu_data_t *);

void
ismv_moof2_mfu_clean_audio_trak( ismv_parser_t *, int32_t);
void
ismv_moof2_mfu_clean_video_trak( ismv_parser_t *, int32_t);

#endif
