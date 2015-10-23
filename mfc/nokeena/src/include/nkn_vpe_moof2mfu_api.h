/*
 *
 * Filename:  nkn_vpe_moof2mfu_api.h
 * Date:      2010/08/02
 * Module:    implements the moof2mfu file converter callbacks
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

#ifndef _MOOF2MFU_CONV_
#define _MOOF2MFU_CONV_

#include <stdio.h>
#include <inttypes.h>
#include "nkn_vpe_mp4_ismv_access_api.h"
#include "mfp_file_converter_intf.h"
#include "nkn_vpe_ismv_reader.h"
#include "nkn_vpe_ismc_read_api.h"
#include "nkn_vpe_ismv_mfu.h"
#include "nkn_vpe_moof2mfu.h"

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * \struct container for all parser, media buffer handling
     * contexts for this converter
     */
    typedef struct tag_moof2mfu_converter_ctx {
        ismv_parser_ctx_t *ismv_ctx[MAX_TRACKS];
        ismv_reader_t *reader[MAX_TRACKS];
        moof2mfu_builder_t *bldr;
    } moof2mfu_converter_ctx_t;

    /**
     * \struct context that contains all information for a write call
     */
    typedef struct tag_moof2mfu_write {
        mfu_data_t *out;
        int32_t profile;
        ismc_publish_stream_info_t *psi;
        int32_t curr_profile_idx;
    } moof2mfu_write_t;

    typedef struct tag_moof2mfu_write_array {
        moof2mfu_write_t writer[MAX_TRACKS];
        uint32_t num_mfu_towrite;
	uint32_t is_moof2mfu_conv_stop;
	uint32_t is_last_moof;
	int32_t num_pmf;
	uint32_t seq_num;
	uint32_t streams_per_encap;
        uint32_t n_traks;
    }moof2mfu_write_array_t;


    int32_t init_moof2mfu_converter(file_conv_ip_params_t
				     *fconv_params,
				     void **conv_ctx);


    int32_t moof2mfu_process_frags(void *private_data, void **out);
    /**
     * cleanup the converter context
     * @param private_data [in] - context to be cleaned up
     */
    void moof2mfu_cleanup_converter(void *private_data);
    int32_t moof2mfu_write_output(void *out);
    void
    moof2mfu_cleanup_writer_array(moof2mfu_write_array_t *);

#ifdef __cplusplus
}
#endif

#endif //_MOOF2MFU_CONV_
