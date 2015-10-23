/*
 *
 * Filename:  nkn_vpe_ismv2moof_api.h
 * Date:      2010/08/02
 * Module:    implements the ismv2moof file converter callbacks
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

#ifndef _ISMV2MOOF_CONV_
#define _ISMV2MOOF_CONV_

#include <stdio.h>
#include <inttypes.h>

#include "nkn_vpe_mp4_ismv_access_api.h"
#include "mfp_file_converter_intf.h"
#include "nkn_vpe_ismv_reader.h"
#include "nkn_vpe_ismc_read_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ISMV_PKG_TYPE_MBR 0
#define ISMV_PKG_TYPE_SBR 1

    /**
     * \struct moof builder, maintains state for the moof
     *  write/processing
     */
    typedef struct tag_ismv_moof_builder {
	FILE *ismv_file;
	char *ismv_name[MAX_TRACKS];
	char *ismv_file_path;
	int32_t chunk_time;
	int32_t n_traks;
	char *output_path;
	char *output_video_name;
	uint8_t *ismc_data;
	size_t ismc_size;
	ismc_publish_stream_info_t *psi;
	ism_bitrate_map_t *ism_map;
	int32_t trak_type_idx[MAX_TRACKS];
	int32_t trak_num[MAX_TRACKS];
	int32_t profiles_to_process;
	int32_t curr_profile_idx[MAX_TRACKS];
	int32_t curr_profile[MAX_TRACKS];
	int32_t curr_trak_id[MAX_TRACKS];
	uint8_t ss_package_type;
	uint8_t is_processed[MAX_TRACKS];
	int32_t trak_type_name[MAX_TRACKS];
	int32_t streaming_type;
	int32_t num_pmf;
	uint32_t total_moof_count;
	uint32_t num_moof_processed;
	io_handlers_t *iocb;
	void *io_desc;
	void *io_desc_ismc;
	void *io_desc_ism;
	void *io_desc_output;
    } ismv_moof_builder_t;

    /**
     * \struct container for all parser, media buffer handling
     * contexts for this converter
     */
    typedef struct tag_ismv2moof_converter_ctx {
	ismv_parser_ctx_t *ismv_ctx[MAX_TRACKS];
	ismv_reader_t *reader[MAX_TRACKS];
	ismv_moof_builder_t *bldr;
    } ismv2moof_converter_ctx_t;

    /** 
     * \struct context that contains all information for a write call
     */
    typedef struct tag_ismv2moof_write {
	ismv_moof_proc_buf_t *out;
	char *video_name;
	char *output_dir;
	int32_t profile;
	int32_t trak_type;
	ismc_publish_stream_info_t *psi;
	int32_t curr_profile_idx;
	int32_t trak_type_name;
	uint32_t n_cust_attrs;
	ism_bitrate_map_t *ism_map;
    } ismv2moof_write_t;
    
    typedef struct tag_ismv2moof_write_array {
	ismv2moof_write_t writer[MAX_TRACKS];
	uint32_t num_tracks_towrite;
	int32_t num_pmf;
	uint32_t total_moof_count;
	io_handlers_t *iocb;
	void *io_desc_output;
    }ismv2moof_write_array_t;
    /** 
     * interface handler for initializing the converter context for
     * ISMV to MOOF file conversions
     * @param fconv_params - the converter input parmars, subset of
     * the PMF publisher context
     * @param conv_ctx [out] - a fully populated converter context
     * that will be passed to subsequent converter interface handlers
     * @return - returns 0 on success and a non-zero negative integer
     * on error
     */
    int32_t init_ismv2moof_converter(\
			    file_conv_ip_params_t *fconv_params,  
			    void **conv_ctx);

    /**
     * interface implementation for processing data; this function
     * partitions a chunk of memory from an ISMV file into MOOF's
     * @param private data[in] - the converter context created with a
     * init call
     * @param out [out] - a unit of data that can be written
     * asynchronously
     * @return - returns 0 on success and a negative integer on error
     */
    int32_t ismv2moof_process_frags(void *private_data, void **out);

    /** 
     * interface implmentation to write a block of memory (this is a
     * blocking call and waits for I/O to complete
     * @param out [in] - writer state that contains information with
     * respect to the write location, file name, write buffer etc
     * @return - returns 0 on success and a negative integer on error
     */
    int32_t ismv2moof_write_output(void *out);

    /**
     * cleanup the converter context
     * @param private_data [in] - context to be cleaned up
     */
    void ismv2moof_cleanup_converter(void *private_data);

    void
    ismv2moof_cleanup_writer_array(ismv2moof_write_array_t *writer_array);

#ifdef __cplusplus
}
#endif

#endif //_ISMV2MOOF_CONV_
