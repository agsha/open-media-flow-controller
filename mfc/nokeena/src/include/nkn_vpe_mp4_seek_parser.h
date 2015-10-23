/*
 *
 * Filename:  nkn_vpe_mp4_seek_parser.h
 * Date:      2010/01/01
 * Module:    mp4 seek API implementation
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

#ifndef _MP4_SEEK_PARSING_
#define _MP4_SEEK_PARSING_

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "nkn_vpe_bitio.h"

#include "nkn_vpe_types.h"

#ifdef __cplusplus 
extern "C" {
#endif

#define MDIA_ID 0x6169646d
#define TRAK_ID 0x6b617274
#define MOOV_ID 0x766f6f6d
#define EDTS_ID 0x73746465
#define MINF_ID 0x666e696d
#define DINF_ID 0x666e6964
#define EDTS_ID 0x73746465
#define STBL_ID 0x6c627473
#define TKHD_ID 0x64686b74
#define MDHD_ID 0x6468646d
#define HDLR_ID 0x726c6468
#define STBL_ID 0x6c627473
#define STTS_ID 0x73747473
#define STCO_ID 0x6f637473
#define CO64_ID 0x34366F63
#define STSC_ID 0x63737473
#define STSS_ID 0x73737473
#define STSD_ID 0x64737473
#define STSZ_ID 0x7a737473
#define MVHD_ID 0x6468766d
#define META_ID 0x6174656d
#define ILST_ID 0x74736c69
#define CTTS_ID 0x73747463
#define UDTA_ID 0x61746475
#define FREE_ID 0x65657266
#define VIDE_HEX 0x65646976
#define SOUN_HEX 0x6e756f73
#define SDTP_ID 0x70746473
#define MFRO_ID 0x6f72666d
#define MFRA_ID 0x6172666d
#define TFRA_ID 0x61726674
#define SDTP_ID 0x70746473

/* F4V specific defines */
#define GSST_ID 0x74737367

// BZ 8382: For the udta box injection
// the little to big endian convert is already included
#define YT_UDTA_INJECT_SIZE_WO_SWAP                 756
#define YT_META_INJECT_SIZE_WO_SWAP                 748
#define YT_ILST_INJECT_SIZE_WO_SWAP                 703
#define YT_GSTD_INJECT_SIZE_WO_SWAP                  30
#define YT_UDTA_INJECT_SIZE	nkn_vpe_swap_uint32(756 )
#define YT_META_INJECT_SIZE	nkn_vpe_swap_uint32(748 )
#define YT_HDLR_INJECT_SIZE	nkn_vpe_swap_uint32(33  )
#define YT_ILST_INJECT_SIZE	nkn_vpe_swap_uint32(703 )
#define YT_GSST_INJECT_SIZE	nkn_vpe_swap_uint32(25  )
#define YT_GSTD_INJECT_SIZE	nkn_vpe_swap_uint32(30  )
#define YT_GSSD_INJECT_SIZE	nkn_vpe_swap_uint32(56  )
#define YT_GSPU_INJECT_SIZE	nkn_vpe_swap_uint32(152 )
#define YT_GSPM_INJECT_SIZE	nkn_vpe_swap_uint32(152 )
#define YT_GSHH_INJECT_SIZE	nkn_vpe_swap_uint32(280 )
#define GSTD_ID 0x64747367
#define GSSD_ID 0x64737367
#define GSPU_ID 0x75707367
#define GSPM_ID 0x6d707367
#define GSHH_ID 0x68687367
#define DATA_ID 0x61746164

    typedef struct tag_stts_shift_params {
	int32_t entry_no;
	int32_t new_delta;
	int32_t new_num_samples;
    }stts_shift_params_t;

    typedef struct tag_stss_shift_params {
	int32_t entry_no;
    }stss_shift_params_t;

    typedef struct tag_stsc_shift_params {
	int32_t entry_no;
	int32_t new_samples_per_chunk;
	int32_t new_first_sample_in_chunk;
    }stsc_shift_params_t;

    typedef struct tag_ctts_shift_params {
	int32_t entry_no;
	int32_t new_delta;
	int32_t new_num_samples;
    }ctts_shift_params_t;

    typedef struct tag_table_shift_params {
	stts_shift_params_t stts_sp;
	ctts_shift_params_t ctts_sp;
	stsc_shift_params_t stsc_sp;
	stss_shift_params_t stss_sp;
    }table_shift_params_t;

    /****************************************CORE MP4 parsing *************************************
     *
     **********************************************************************************************/
    int32_t mp4_find_child_box(bitstream_state_t *bs, 
			       char *child_box_id, size_t *pos, 
			       size_t *size);
    int32_t mp4_read_next_box(bitstream_state_t *bs, 
			      size_t *child_box_pos, box_t *box);
    void *mp4_init_parent(uint8_t* p_data, size_t pos, size_t size,
			  int32_t box_id);
    mdia_t* mp4_init_mdia(uint8_t *p_data, size_t pos, size_t size);
    mfra_t* mp4_init_mfra(uint8_t *p_data, size_t off, size_t size);
    trak_t * mp4_init_trak(uint8_t *p_data, size_t pos, size_t size);
    stbl_t * mp4_init_stbl(uint8_t *p_data, size_t off, size_t size);
    udta_t* mp4_init_udta(uint8_t *p_data, size_t off, size_t size);
    trak_t* mp4_read_trak(uint8_t *p_data, size_t pos, size_t size,
			  bitstream_state_t *desc);
    int32_t mp4_timestamp_to_sample(trak_t *trak, int32_t ts,
				    stts_shift_params_t *sp);
    int32_t mp4_find_nearest_sync_sample(trak_t *trak, 
					 int32_t sample_num,
					 stss_shift_params_t *sp);    
    int32_t mp4_get_stsc_details(bitstream_state_t *bs, 
				 int32_t sample_num, 
				 int32_t *chunk_num, 
				 int32_t *samples_per_chunk,
				 int32_t *sample_description);
    int32_t mp4_sample_to_chunk_num(trak_t *trak, 
				    int32_t sample_number, 
				    int32_t *first_sample_in_chunk,
				    stsc_shift_params_t *sp);
    size_t mp4_chunk_num_to_chunk_offset(trak_t *trak, int32_t chunk_num);
    uint64_t mp4_chunk_num_to_chunk_offset_co64(trak_t *trak, int32_t chunk_num);
    int32_t mp4_read_stsz_entry(bitstream_state_t *bs, 
				int32_t	entry_no, int32_t *size);
    size_t mp4_find_cumulative_sample_size(trak_t *trak, 
					   int32_t first_sample,
					   int32_t sample_num);
    int32_t mp4_get_stsz_num_entries(trak_t *trak);
    size_t mp4_get_first_sample_offset(trak_t *trak);
    size_t mp4_stts_compute_trak_length(trak_t* trak);
    uint64_t mp4_sample_to_duration(trak_t *trak, int32_t sample);
    uint64_t mp4_sample_to_time(trak_t *trak, int32_t sample);
    uint32_t mp4_get_stco_entry_count(trak_t *trak);
    uint32_t mp4_get_co64_entry_count(trak_t *trak);
    int32_t mp4_yt_adjust_ilst_props(moov_t *moov, double seek_time, void *iod);
    int32_t mp4_rebase_stco(trak_t *trak, int64_t bytes_to_skip);
    stsd_t * mp4_read_stsd_box(uint8_t* data, size_t size);
    int32_t mp4_trak_get_codec(trak_t *trak);
    mp4_audio_sample_entry_t * 
                  mp4_read_audio_sample_entry(uint8_t *data, 
					      size_t  size);

    void mp4_cleanup_stsd(stsd_t *stsd);
    void \
    mp4_cleanup_audio_sample_entry(mp4_audio_sample_entry_t *ase);
    mp4_visual_sample_entry_t*\
    mp4_read_visual_sample_entry(uint8_t* data, size_t size);
    void mp4_cleanup_visual_sample_entry(mp4_visual_sample_entry_t *vse);
    int32_t mp4_dm_adjust_ilst_props(moov_t *moov, double seek_time,
				     void *seek_provider_specific_data, 
				     void *iod);
    double mp4_get_vid_duration(const moov_t *const moov);
    int32_t mp4_write_ilst_tag(bitstream_state_t *bs, uint32_t tag_name,
			       uint32_t type, uint8_t *data,
			       uint32_t data_size);
	
    /********************************** FUNCTIONS TO SHIFT THE STBL's ***************************************************
     *
     *******************************************************************************************************************/
    size_t mp4_adjust_sample_tables_inplace(moov_t *moov, int32_t n_tracks_to_adjust, int32_t *start_sample_num, 
					    int32_t *end_sample_num, int32_t *start_chunk_num, int32_t *end_chunk_num,
					    size_t *mdat_offset, table_shift_params_t *tsp);
    size_t mp4_adjust_trak(moov_t *moov, uint32_t trak_no, int32_t *start_sample_num, 
			   int32_t *end_sample_num, int32_t *start_chunk_num, int32_t *end_chunk_num,
			   size_t * mdat_offset, table_shift_params_t *tsp, size_t bytes_to_skip, 
			   moov_t *moov_out, bitstream_state_t *out,
			   size_t *time, int32_t *errcode);
    size_t mp4_adjust_sample_tables_copy(moov_t *moov, int32_t n_tracks_to_adjust, int32_t *start_sample_num,
					 int32_t *end_sample_num, int32_t *start_chunk_num, int32_t *end_chunk_num,
					 size_t *mdat_offset, table_shift_params_t *tsp, moov_t **modified_moov, vpe_ol_t* ol, int n_box_to_find);
    int32_t mp4_get_stts_shift_offset(trak_t *trak, int32_t sample, stts_shift_params_t *sp);
    int32_t mp4_get_ctts_shift_offset(trak_t *trak, int32_t sample, ctts_shift_params_t *sp);
    int32_t mp4_set_stsc_details(bitstream_state_t *bs, int32_t sample_num, int32_t first_chunk, int32_t samples_per_chunk, int32_t sample_description);
    int32_t mp4_adjust_ctts(trak_t *trak, int32_t start_sample, int32_t end_sample, ctts_shift_params_t *sp, uint8_t *out);
    int32_t mp4_adjust_stts(trak_t *trak, int32_t start_sample, int32_t end_sample, stts_shift_params_t *sp, uint8_t *out);
    int32_t mp4_adjust_stsc(trak_t *trak, int32_t start_chunk, int32_t end_chunk, int32_t start_sample, stsc_shift_params_t *sp, uint8_t *out);
    int32_t mp4_adjust_stco(trak_t *trak, int32_t start_chunk, size_t mdat_offset, size_t bytes_to_skip, uint8_t *out);
    int32_t mp4_adjust_stss(trak_t *trak, int32_t start_sample, int32_t end_sample, stss_shift_params_t *sp, uint8_t *out);
    int32_t mp4_adjust_stsz(trak_t *trak, int32_t sample_num, int32_t end_sample_num, uint8_t *out);
    int32_t mp4_write_full_box_header(uint8_t *p_buf, uint32_t type, uint32_t box_size, uint32_t flags);
    int32_t mp4_write_box_header(uint8_t *p_buf, uint32_t type, uint32_t box_size);
    size_t mp4_copy_box(bitstream_state_t *bs, bitstream_state_t *bs_out, box_t *box);
    size_t mp4_compute_parent_size(uint8_t *p_data);
    int32_t mp4_validate_trak(trak_t *trak);
    uint32_t  mp4_get_ctts_offset(trak_t *trak, int32_t sample_num);
    int32_t mp4_search_box_list(uint8_t *data, size_t data_size,
				const uint32_t *box_list, uint32_t num_boxes,
				uint32_t *num_found);
    int32_t mp4_search_ilst_tags(const bitstream_state_t *const bs1,
				 size_t data_size,
				 const uint32_t *tag_list,
				 uint32_t n_tags,
				 uint32_t *num_found);
	

	
#ifdef __cplusplus
}
#endif

#endif
