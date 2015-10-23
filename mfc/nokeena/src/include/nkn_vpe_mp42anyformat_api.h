/*
 *
 * Filename:  nkn_vpe_mp42anyformat_api.h
 * Date:      2011/01/11
 * Module:    implements the mp42anyformat converter callbacks
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

#ifndef _MP42ANYFORMAT_CONV_
#define _MP42ANYFORMAT_CONV_

#ifdef __cplusplus
extern "C" {
#endif


#include <stdio.h>
#include <inttypes.h>

#include "nkn_vpe_types.h"
#include "nkn_vpe_mp4_parser.h"
#include "nkn_vpe_mp4_seek_api.h"
#include "nkn_vpe_mp4_seek_parser.h"
#include "nkn_vpe_mp4_ismv_access_api.h"
#include "nkn_vpe_mp42mfu_api.h"
#include "nkn_vpe_mfu_defs.h" 
#include "mfp_ref_count_mem.h"
#include "mfp_file_converter_intf.h"
#include "nkn_vpe_sync_http_read.h"
#include "mfu2iphone.h" 

#include "nkn_vpe_sl_fmtr.h"
#include "nkn_vpe_parser_err.h"

#define MAX_MFU 2000
    typedef struct tag_mp4src_mfu_moof_builder {
	
	uint32_t mfu2moof_conv_req_num_mfu;
	
    } mp4src_mfu_moof_builder_t;
    
    
    typedef struct tag_mp4src_mfu_ts_builder {
	
	uint32_t mfu2ts_conv_req_num_mfu;
	uint32_t mfu2ts_conv_processed[MAX_TRACKS];
        int32_t **num_active_mfu2ts_task;	
	int32_t n_profiles;
    } mp4src_mfu_ts_builder_t;
    
    
    typedef struct tag_mp4anyformat_converter_ctx {
	mp4_parser_ctx_t *mp4_ctx[MAX_PROFILES];/*specifies the context of the input mp4
				    file*/
	mp4_mfu_builder_t *bldr_mp4_mfu;/*structure used to maintain the
					  state for mp42mfu conversion*/
	mp4src_mfu_ts_builder_t *bldr_mfu_ts;/*structure used to maintain
					       thhe state for mfu2ts conversion*/
	sl_fmtr_t* slfmtr;/*structure used to
						   maintain the state for mfu2moof conversion*/
	mfu_data_t ***mfu_data;// row->num_moofs col ->num_vid_traksspecifies the maximum number of MFU's
	ref_count_mem_t ***ref_cont;/*specifies the ref count
							    container array each pointing
							    to the correspoding MFU. This
							    structure is used to avoid the
							    overlap of usage of memory*/
	uint32_t ms_parm_status;
	uint32_t ss_parm_status;
	uint32_t profile_bitrate;
	uint32_t max_vid_traks;
	uint32_t max_sync_points;
	int32_t  sess_id;

    } mp42anyformat_converter_ctx_t;
    
    
    void
    mfp_convert_mp42anyformat(void *arg);
    
    int32_t
    init_mp42anyformat_converter(file_conv_ip_params_t *,
				 void **);
    
    int32_t
    mp42anyformat_conversion(void *, void **);
    void
    mp42anyformat_cleanup_converter(void *);
    
    int32_t
    mp4_create_mp4_ctx_from_file(char *, mp4_parser_ctx_t **,
				 io_handlers_t *);
    
    
    int32_t
    init_mp4src_mfu2ts_builder(int32_t,
			       mp4src_mfu_ts_builder_t**, uint32_t, int32_t);    
    int32_t
    mfu2ts_conversion(mp4src_mfu_ts_builder_t*, ref_count_mem_t**,
		      uint32_t, uint32_t, int32_t, uint32_t, int32_t **);

    void
    cleanup_mp4_ctx(mp4_parser_ctx_t*);

    void
    cleanup_mp4src_mfu_ts_builder(mp4src_mfu_ts_builder_t*);

    void
    cleanup_mp4src_mfu_moof_builder(mp4src_mfu_moof_builder_t*);
    
#ifdef __cplusplus
}
#endif

#endif //_MP42ANYFORMAT_CONV_
