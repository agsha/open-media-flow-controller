/*
 *
 * Filename:  nkn_vpe_mfu_parser.h
 * Date:      2010/10/12
 * Module:    MFU definitions (see specification for more details)    
 *
 * (C) Copyright 2020 Juniper Networks, Inc.
 * All rights reserved.
 *
 *
 */

#ifndef _NKN_VPE_MFU_PARSER_H_
#define _NKN_VPE_MFU_PARSER_H_

#ifdef __cplusplus
extern "C" {
#endif

	//Proposed MFU parser utility declaration and MFU container definition

	typedef struct mfu_cont {

		mfub_t *mfub;
		uint8_t desc_count;
		mfu_rw_desc_t **desc;
		uint32_t *dat_len;
		uint8_t **dat_ptr;
	} mfu_cont_t;

	mfu_cont_t* parseMFU(uint8_t *mfu_data);

	// 

    int32_t mfu_parse(unit_desc_t* vmd, unit_desc_t* amd,
	    uint8_t *mfu_data_orig,uint32_t mfu_data_size, 
	    uint8_t *sps_pps, uint32_t  *sps_pps_size, 
	    uint32_t *is_audio,uint32_t *is_video, 
	    mfub_t *mfu_header);

    uint32_t
	mfu_get_adat_box(uint8_t *mfu_data_orig, uint32_t adat_pos, unit_desc_t
		*amd);


    uint32_t
	mfu_get_vdat_box(uint8_t *mfu_data_orig, uint32_t vdat_pos, unit_desc_t
		*vmd);



    uint32_t 
	mfu_get_rwfg_box(uint8_t *mfu_data_orig, uint32_t rwfg_pos, uint32_t
		*is_audio,uint32_t *is_video, unit_desc_t*
		vmd, unit_desc_t* amd, uint8_t *sps_pps, uint32_t
		*sps_pps_size);


    int32_t 
	mfu_get_video_rw_desc(uint8_t *videofg, unit_desc_t* vmd, uint8_t
		*sps_pps, uint32_t *sps_pps_size);


    int32_t
	mfu_get_audio_rw_desc(uint8_t *audiofg, unit_desc_t* amd);


    uint32_t
	mfu_get_mfub_box(uint8_t* mfu_data_orig, uint32_t mfub_pos, mfub_t
		*mfu_header);


    int32_t mfu2ts_clean_vmd_amd(unit_desc_t* vmd, unit_desc_t* amd,
	    uint8_t * sps_pps);



#ifdef __cplusplus
}
#endif


#endif


