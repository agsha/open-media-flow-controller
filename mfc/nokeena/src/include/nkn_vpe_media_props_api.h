/*
 *
 * Filename:  nkn_vpe_media_props_api.h
 * Date:      2010/01/01
 * Module:    mp4 ismv random access api
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */
#ifndef _VPE_MEDIA_PROPS_API_
#define _VPE_MEDIA_PROPS_API_

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "nkn_vpe_types.h"
#include "nkn_vpe_mp4_parser.h"
#include "nkn_vpe_mp4_seek_api.h"
#include "nkn_vpe_flv_parser.h"

#ifdef __cplusplus 
extern "C" {
#endif

#define HDR_DATA_SIZE 32768
    /* Error Codes */
    /** \def invalid container format */
#define E_MPROPS_CONTAINER_FOUND 201
    /** \def insufficient data to parse */
#define E_MPROPS_INSUFF_DATA     202
    /** \def container parsing error */
#define E_MPROPS_CONTAINER_PARSE_ERR 203	

    /**
     * container type to fill the video attributes for DM2
     */
    typedef struct tag_vpe_media_props {
	uint8_t vtype:5;                 /*!< version type 0x1 - VPE */
	uint8_t v_subtype:3;             /*!< Sub-type 0x1 */
	uint8_t v_unused[1];             /*!< unused */
	uint16_t v_video_bitrate;        /*!< kbps;max 65Mbps */
	uint16_t v_audio_bitrate:13;     /*!< kbps; max 8Mbps */
	uint16_t v_reserved:3;           /*!< reserved */
	uint16_t v_video_codec_type:5;   /*!< max 32 video codec types 
					   0 - reserved, 
					   1 - H.264/AVC
					   2 - VP6, etc.
					   3 - MPEG4 - part2
					   4 - H.263 Sorenson
					 */
	uint16_t v_audio_codec_type:5;   /*!<  max 32 audio codec types
					   0 - reseverd
					   1 - AAC
					   2 - MP3, etc
					 */
	uint16_t v_cont_type:6;          /*!< max 64 container types 
					   0 - reserved, 
					   1 - MP4,
					   2 - FLV, etc.
					 */
	uint8_t v_padding[6];            /*!< padding to DWORD
					   boundary */
    } vpe_media_props_t;

    /**
     * the media detector API's context
     */
    typedef struct tag_media_detector_ctx {
	vpe_media_props_t *mp; /*!< media properties data type */ 
	uint8_t           *data; /*!< pointer to data that is
				   currently being parsed */ 
	size_t            size; /*!< size of data being parsed
				  currently */ 
	size_t            tot_size; /*!< total size of the object */
	int32_t           n_traks; /*!< number of traks, MP4 files
				     only */
	int32_t           err; /*!< error code during parse operation
				*/ 
	void              *parser_ctx; /*!< container specific parser
					 context */ 
    } vpe_media_detector_ctx_t;

    /**
     * @brief initializes the media detector context
     * @param data [in] - media data from the top of the video file. for correct
     * parsing requires the first 32KB of data
     * @param data_size [in]  - size of data, typically 32KB
     * @param tot_size [in] - size of the video data
     * @return - returns NULL, if unable to create context. read the
     * err code in the context to ensure there is no error (only when
     * we are able to create a valid context
     */
    vpe_media_detector_ctx_t * vpe_mprops_create_media_detector(uint8_t *data, 
								size_t data_size,
								size_t tot_size,
								uint16_t cont_type);

    /**
     * @brief Compute the offset to the meta data and the size of the meta
     * data that needs to be passed to glean the media properties
     * @param md [in] - instance of the media detector
     * @param ol [out] - data type that can contain an [offset, length]
     * pair
     * @return - returns '0' on success and negative value
     * corresponding to a valid E_MPROPS_XXXX error code
     */    
    int32_t
    vpe_mprops_get_metadata_size(vpe_media_detector_ctx_t *md, 
				 vpe_ol_t *ol);

    /**
     * @brief Initialize the meta data buffers into the media detector context
     * @param md [in] - instance of the media detector context
     * @param metadata [in] - buffer containing the metadata
     * @param metadata_size [in] - size of the metadata buffer
     * @return - returns '0' on success and a negative number on error
     */
    int32_t 
    vpe_mprops_set_meta_data(vpe_media_detector_ctx_t *md, 
			     uint8_t *metadata, 
			     size_t metadata_size);

    /**
     * @brief populates the media attributes from the meta data
     * @param md [in] - instance of the media detector context
     * @return returns '0' on success; negative number on error. for
     * more error information; read 'err' field in the media detector
     * context 
     */
    int32_t
    vpe_mprops_populate_media_attr(vpe_media_detector_ctx_t *md);

    /**
     * @brief retrieves the fully populated  media properties data type from
     * the media detector context
     * @param md [in] - media detector context
     * @return returns the media properties data type
     */
    vpe_media_props_t*
    vpe_mprops_get_media_attr(vpe_media_detector_ctx_t *md); 

    /**
     * @brief cleans up the resources used by the media detector context 
     * @param md [in] - media detector context
     */
    void
    vpe_mprops_cleanup_media_detector(vpe_media_detector_ctx_t *md);
	
#ifdef __cplusplus
}
#endif

#endif
