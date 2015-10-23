/*
 *
 * Filename:  nkn_vpe_mfu2ts_api.h
 * Date:      2010/09/23
 * Module:    implements the mfu2ts file converter callbacks
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

#ifndef _MFU2MOOF_CONV_
#define _MFU2MOOF_CONV_

#include <stdio.h>
#include <inttypes.h>
#include "mfp_file_converter_intf.h"
#include "mfu2iphone.h"
#include "nkn_vpe_ismv_mfu.h"
#include "nkn_vpe_ismv_reader.h"
#include "nkn_vpe_mp4_ismv_access_api.h"
#include "nkn_vpe_mp4_seek_api.h"

#include "nkn_vpe_ismc_read_api.h"
#include "mfp_ref_count_mem.h"

#ifdef __cplusplus
extern "C" {
#endif
    typedef struct tag_mfu2ts_builder {
	gbl_fruit_ctx_t *fruit_ctx;
	fruit_stream_t *st;
	unit_desc_t *vmd;
	unit_desc_t *amd;
	uint8_t *sps_pps;
	uint32_t is_video;
	uint32_t is_audio;
	uint32_t sps_pps_size;
	mfub_t mfu_hdr;
	uint8_t *mfu_data_orig;
	uint32_t num_mfu_count;
	mfu_data_t *mfu_data;
	char *mfu_out_fruit_path;
	char *mfu_uri_fruit_prefix;
	uint32_t is_mfu2ts_conv_stop;
	uint32_t tot_mfu_count;
	uint32_t num_mfu_stored;
	uint32_t num_mfu_processed;
	int32_t num_pmf;
	uint32_t is_last_mfu;
	uint32_t seq_num;
	uint32_t streams_per_encap;
	int32_t **num_active_moof2ts_task;
      uint32_t n_traks;
    }mfu2ts_builder_t;

    /**
     * \struct container for all parser, media buffer handling
     * contexts for this converter
     */
    typedef struct tag_mfu2ts_converter_ctx {
        ismv_reader_t *reader;
        mfu2ts_builder_t *bldr;
    } mfu2ts_converter_ctx_t;

    /**
     * \struct context that contains all information for a write call
     */
    typedef struct tag_mfu2ts_write {
        mfu_data_t *out;
        int32_t profile;
        ismc_publish_stream_info_t *psi;
        int32_t curr_profile_idx;
    } mfu2ts_write_t;

    typedef struct tag_mfu2ts_write_array {
        mfu2ts_write_t writer[MAX_TRACKS];
        uint32_t num_mfu_towrite;
    }mfu2ts_write_array_t;


    int32_t init_mfu2ts_converter(\
                                     file_conv_ip_params_t
                                     *fconv_params,
                                     void **conv_ctx);


    int32_t mfu2ts_process_frags(void *private_data, void **out);
    /**
     * cleanup the converter context
     * @param private_data [in] - context to be cleaned up
     */
    void mfu2ts_cleanup_converter(void *private_data);
    int32_t mfu2ts_write_output(void *out);

#ifdef __cplusplus
}
#endif

#endif //_MOOF2MFU_CONV_
