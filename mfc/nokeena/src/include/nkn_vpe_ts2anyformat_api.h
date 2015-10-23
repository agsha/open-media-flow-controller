/*
 *
 * Filename:  nkn_vpe_ts2anyformat_api.h
 * Date:      2011/01/11
 * Module:    implements the ts2anyformat converter callbacks
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

#ifndef _TS2ANYFORMAT_CONV_
#define _TS2ANYFORMAT_CONV_

#ifdef __cplusplus
extern "C" {
#endif


#include <stdio.h>
#include <inttypes.h>

#include "nkn_vpe_types.h"
#include "nkn_vpe_mp4_parser.h"
#include "nkn_vpe_ts2mfu_api.h"
#include "nkn_vpe_mfu_defs.h"
#include "mfp_ref_count_mem.h"
#include "mfp_file_converter_intf.h"
#include "nkn_vpe_sync_http_read.h"
#include "mfu2iphone.h"


    typedef struct tag_ts_src_mfu_ts_builder {
	
	uint32_t mfu2ts_conv_req_num_mfu;
	uint32_t mfu2ts_conv_processed[MAX_TRACKS];
      int32_t **num_active_mfu2ts_task;	
    } ts_src_mfu_ts_builder_t;

  typedef struct pid_map_tt{
    uint32_t pid;
    uint32_t min_pkt_time;
    uint32_t max_pkt_time;
    uint32_t max_duration;
    uint32_t entry_valid;
  }pid_map_t;
    
    typedef struct tag_ts2anyformat_converter_ctx {
	ts_mfu_builder_t *bldr_ts_mfu;/*structure used to maintain the
					state for ts2mfu conversion*/
	ts_src_mfu_ts_builder_t *bldr_mfu_ts;/*structure used to maintain
					       the state for mfu2ts conversion*/
	sl_fmtr_t* slfmtr;/*structure used to
			    maintain the state for mfu2moof conversion*/
	mfu_data_t ***mfu_data;/*specifies the
				 maximum number of
				 MFU's [MAX_TRACKS][MAX_MFU]*/
	
	ref_count_mem_t ***ref_cont;/*specifies the ref count
				      container array each pointing
				      to the correspoding MFU. This
				      structure is used to avoid the
				      overlap of usage of memory*/
	uint32_t ms_parm_status;
	uint32_t ss_parm_status;
	uint32_t profile_bitrate;
	uint32_t max_profile;
	uint32_t sess_id;
    } ts2anyformat_converter_ctx_t;
    

void
mfp_convert_ts2anyformat(void *arg);
int32_t
init_ts2anyformat_converter(file_conv_ip_params_t*, void**);
int32_t
ts2anyformat_conversion(void*, void**);
int32_t
ts_mfu2ts_conversion(ts_src_mfu_ts_builder_t*, ref_count_mem_t**,
		     uint32_t, uint32_t, uint32_t, uint32_t, int32_t **, uint32_t);
void
ts2anyformat_cleanup_converter(void*);
int32_t
init_ts_src_mfu2ts_builder(int32_t, ts_src_mfu_ts_builder_t**,
			   uint32_t, int32_t);

void
cleanup_ts_src_mfu_ts_builder(ts_src_mfu_ts_builder_t *);





    
#ifdef __cplusplus
}
#endif

#endif //_TS2ANYFORMAT_CONV_
