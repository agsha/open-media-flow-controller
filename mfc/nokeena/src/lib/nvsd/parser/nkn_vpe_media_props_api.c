#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>

#include "nkn_vpe_media_props_api.h"
#include "nkn_vpe_mp4_parser.h"
#include "nkn_vpe_mp4_seek_parser.h"
#include "nkn_memalloc.h"
#include "nkn_ssp_cb.h"
#include "nkn_vpe_webm_parser.h"
#include "nkn_vpe_asf_parser.h"

/* static functions */
static inline double vpe_mprops_compute_bitrate(size_t tot_size,
						double duration); 
static inline void vpe_mprops_init_media_attr(vpe_media_props_t *mp); 

/******************************************************************
 *               MEDIA ATTRIBUTE DETECTION API's
 * 1. vpe_mprops_create_media_detector - creates a media detector
 *    context.
 * 2. vpe_mprops_get_metadata_size - find the metadata offset and
 *    size. this is used to find the media attributes
 * 3. vpe_mprops_set_metadata - set the metadata buffers
 * 4. vpe_mprops_populate_media_attr - parses the metadata buffers and
 *    fills the media attribute structure
 * 5. vpe_mprops_get_media_attr - retrive a completely populated media
 *    attribute structure
 * 6. vpe_mprops_cleanup_media_detector - free all resources used by
 *    the media detector context
 *****************************************************************/

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
vpe_media_detector_ctx_t *
vpe_mprops_create_media_detector(uint8_t *data, size_t data_size,
				 size_t tot_size, uint16_t cont_type)
{

    vpe_media_detector_ctx_t *md;
    int32_t rv;

    rv = 0;

    /* allocate for the media detector context */
    md = (vpe_media_detector_ctx_t*)
	    nkn_calloc_type (1, sizeof(vpe_media_detector_ctx_t),
			     mod_vpe_media_detector_ctx_t);

    /* check for error */
    if( !md ) {
	return NULL;
    }

    /* allocate for the media properties data type */
    md->mp = (vpe_media_props_t *)nkn_calloc_type(1,
						  sizeof(vpe_media_props_t),
						  mod_vpe_media_props_t);

    if( !md->mp ) {
	free(md);
	return NULL;
    }

#if 0 /* I do not think this is useful */
    /* sanity - size of data should be a mimimum of 32KB */
    if( data_size < HDR_DATA_SIZE ) {
	free(md->mp);
	free(md);
        //md->err = -E_MPROPS_INSUFF_DATA;
	return NULL;
    }
#endif

    /* initialize the context */
    md->data = data;
    md->size = data_size;
    md->tot_size = tot_size;

    /* initialize media props container */
    vpe_mprops_init_media_attr(md->mp);

    /* container type */
    md->mp->v_cont_type = cont_type;


    return md;
}

/**
 * @brief Compute the offset to the meta data and the size of the meta
 * data that needs to be passed to glean the media properties
 * @param md [in] - instance of the media detector
 * @param ol [out] - data type that can contain an [offset, length]
 * pair
 * @return - returns '0' on success and other value for failure
 *
 */
int32_t
vpe_mprops_get_metadata_size(vpe_media_detector_ctx_t *md,
			     vpe_ol_t *ol)
{

    int32_t video_codec, audio_codec;
    int rv = 0;
    int fc_rv;
    video_codec = audio_codec = 0;
    ol->offset = 0;
    ol->length = 0;
    flv_parser_t *fp;

    switch(md->mp->v_cont_type) {
	case CONTAINER_TYPE_MP4: // MP4
	case CONTAINER_TYPE_3GPP:
	case CONTAINER_TYPE_3GP2:
	    /* find the moov offset, length */
	    mp4_get_moov_info(md->data, md->size, &ol->offset,
			      &ol->length);
	    break;
	case CONTAINER_TYPE_WEBM:
	    /* find the traks offset, length*/
	    rv = nkn_vpe_webm_segment_pos(md->data, md->size, ol);
	    break;
	case CONTAINER_TYPE_ASF:
	    /* find the traks offset, length*/
	    rv = nkn_vpe_asf_header_obj_pos(md->data, md->size, ol);
	    break;
	case CONTAINER_TYPE_FLV:
	    fp = flv_init_parser();
	    md->parser_ctx = (void*)fp;
	    if( !fp ) {
		md->err = -E_MPROPS_CONTAINER_PARSE_ERR;
		return -E_MPROPS_CONTAINER_PARSE_ERR;
	    }

	    /* disable seek parsing in flv parser */
	    fc_rv = flv_set_seekto(fp, 0, NULL, 0);
	    if (fc_rv < 0) {
		flv_parser_cleanup(fp);
		return -E_MPROPS_CONTAINER_PARSE_ERR;
	    }
	    /* read FLV header */
	    if (flv_parse_header(fp, md->data, md->size) == E_VPE_FLV_DATA_INSUFF) {
		flv_parser_cleanup(fp);
		return -E_MPROPS_CONTAINER_PARSE_ERR;
	    }

	    /*
	     * find the meta tag offset, length
	     *
	     * we will not use the return value of this function
	     * return value of this function can be 0 or E_VPE_FLV_DATA_INSUFF
	     * if it returns 0, it means that it fetchs all of ol, video_codec, audio_codec
	     * successfully.
	     * if it returns E_VPE_FLV_DATA_INSUFF, it means some of ol, video_codec, audio_codec
	     * cannot be fetched.
	     * As we have a higher level of error handle of ol, the return value of this function
	     * is not important. we will use the error handle of ol to handle failure fetch of ol
	     * The video_codec and audio_codec are optional information.
	     */
	    flv_get_meta_tag_info(fp, md->data, md->size, &ol->offset,
				  &ol->length, &video_codec,
				  &audio_codec);

	    md->mp->v_video_codec_type = video_codec;
	    md->mp->v_audio_codec_type = audio_codec;
	    break;
    }

    return rv;
}

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
			 size_t metadata_size)
{
    md->data = metadata;
    md->size = metadata_size;
    return 0;
}

/**
 * @brief populates the media attributes from the meta data
 * @param md [in] - instance of the media detector context
 * @return returns '0' on success; negative number on error. for
 * more error information; read 'err' field in the media detector
 * context
 */
int32_t
vpe_mprops_populate_media_attr(vpe_media_detector_ctx_t *md)
{
    moov_t *moov;
    int32_t traks, i, codec_desc;
    double duration;
    size_t pos;
    const char *search_tag = "duration";
    mval m;
    uint64_t *ptr;
    int rv;

    nkn_vpe_webm_segment_t* p_webm_segment;
    uint64_t ebml_head_size;
    uint64_t elem_ID_val;
    uint64_t elem_datasize_val;

    nkn_vpe_asf_header_obj_t* p_asf_header_obj;
    codec_desc = 0;

    switch(md->mp->v_cont_type) {
	case CONTAINER_TYPE_MP4: //MP4
	case CONTAINER_TYPE_3GPP:
	case CONTAINER_TYPE_3GP2:
	    /* initialize moov context */
	    moov = mp4_init_moov(md->data, 0, md->size);

	    /* store the moov context into the media detector context */
	    md->parser_ctx = (void *)moov;
	    if( !moov ){
		md->err = -E_MPROPS_CONTAINER_PARSE_ERR;
		return md->err;
	    }

	    /* populate the traks in the moov context */
	    traks = mp4_populate_trak(moov);
	    md->n_traks = traks;
	    if( traks < 0 ) {
		md->err = -E_MPROPS_CONTAINER_PARSE_ERR;
		return md->err;
	    }

	    /* get the duration from mvhd box */
	    duration = (double)moov->mvhd->duration /
		moov->mvhd->timescale;

	    /* compute bitrate */
	    md->mp->v_video_bitrate =
		vpe_mprops_compute_bitrate(md->tot_size, duration);

	    for( i = 0; i < traks; i++ ) {
		codec_desc = mp4_trak_get_codec(moov->trak[i]);
		if ( codec_desc & 0x00020000 ) {
		    /* audio codec */
		    md->mp->v_audio_codec_type =
			codec_desc & 0x0000ffff; /* mask lower 2 bytes
						  */
		} else if ( codec_desc & 0x00010000 ) {
		    md->mp->v_video_codec_type =
			codec_desc & 0x0000ffff;
		}
	    }
	    break;

	case CONTAINER_TYPE_WEBM:

	    /* parse SEGMENT ebml header*/
	    rv = nkn_vpe_ebml_parser(md->data, &elem_ID_val, &elem_datasize_val,
				&ebml_head_size);
	    if (rv != 0) {
		md->err = -E_MPROPS_CONTAINER_PARSE_ERR;
		return md->err;
	    }
	    if (elem_ID_val == WEBM_SEGMENT) {
		/* Initial webm segment parser*/
		p_webm_segment = nkn_vpe_webm_init_segment(md->data, 0,
							   ebml_head_size,
							   md->size - ebml_head_size);
	    } else {
		md->err = -E_MPROPS_CONTAINER_PARSE_ERR;
                return md->err;
	    }

	    md->parser_ctx = (void *)p_webm_segment;
	    if (p_webm_segment == NULL) {
		md->err = -E_MPROPS_CONTAINER_PARSE_ERR;
                return md->err;
	    }
	    /* Parse Segment */
	    rv = nkn_vpe_webm_segment_parser(p_webm_segment);
	    if (rv != 0) {
		md->err = -E_MPROPS_CONTAINER_PARSE_ERR;
                return md->err;
	    }

	    /* get duration */

	    /*
	     * some webm file doesnot have the timescale, not sure whether
	     * this is valid, but MKV time scale default value is 1,000,000 nanosecs
	     */
	    if (p_webm_segment->p_webm_segment_info->webm_timescale == 0 ) {
		p_webm_segment->p_webm_segment_info->webm_timescale = 1000000;
	    }

	    duration = p_webm_segment->p_webm_segment_info->webm_duration *
		((double)p_webm_segment->p_webm_segment_info->webm_timescale / 1000000000);

	    /* Compute Bitrate */
	    md->mp->v_video_bitrate =
		vpe_mprops_compute_bitrate(md->tot_size, duration);
	    /* Codec Information */
	    for (i = 0; i < p_webm_segment->p_webm_traks->webm_num_traks; i++) {
		if (p_webm_segment->p_webm_traks->p_webm_trak_entry[i]->webm_trak_type \
		    == WEBM_VIDEO_TRAK) {
		    if (memcmp(p_webm_segment->p_webm_traks->p_webm_trak_entry[i]->p_webm_codec_ID,
				"V_VP8", 5) == 0) {
			    md->mp->v_video_codec_type = VPE_MPROPS_VCODEC_VP8;
		    }
		}
		if (p_webm_segment->p_webm_traks->p_webm_trak_entry[i]->webm_trak_type \
		    == WEBM_AUDIO_TRAK) {
                    if (memcmp(p_webm_segment->p_webm_traks->p_webm_trak_entry[i]->p_webm_codec_ID,
                                "A_VORBIS", 8) == 0) {
                            md->mp->v_audio_codec_type = VPE_MPROPS_ACODEC_VORBIS;
		    }
		}
	    }
	    break;
	case CONTAINER_TYPE_ASF:
	    /*initial asf Header Object parser*/
	    p_asf_header_obj = nkn_vpe_asf_initial_header_obj(md->data, 0, md->size);

	    md->parser_ctx = (void *)p_asf_header_obj;
	    if (p_asf_header_obj == NULL) {
                md->err = -E_MPROPS_CONTAINER_PARSE_ERR;
                return md->err;
	    }

	    /* parse the asf Header Object */
	    rv = nkn_vpe_asf_header_obj_parser(p_asf_header_obj);
            if (rv != 0) {
                md->err = -E_MPROPS_CONTAINER_PARSE_ERR;
                return md->err;
            }

	    /* get duration */
	    duration = (p_asf_header_obj->p_asf_file_prop_obj->play_duration /
			(10000000 / 1000) -
			p_asf_header_obj->p_asf_file_prop_obj->preroll) / 1000;
	    if (!duration) {
		/*
		 * According to Asf sepc, if the broadcasting flag on
		 * it is possible that duration be zero
		 */
		md->err = -E_MPROPS_CONTAINER_PARSE_ERR;
                return md->err;
	    }
	    /* Compute Bitrate */
	    {
		uint16_t max_bitrate, avg_bitrate; // Kbps
	        max_bitrate = (uint16_t)(p_asf_header_obj->p_asf_file_prop_obj->max_bitrate / 1000.0);
		avg_bitrate = vpe_mprops_compute_bitrate(md->tot_size, duration);
		md->mp->v_video_bitrate = max_bitrate > avg_bitrate ? max_bitrate : avg_bitrate;
	    }

	    /*
	     * detect of video and audio codec type is not implemented yet
	     * It looks like it is not useful now
	     */
	    break;
	case CONTAINER_TYPE_FLV: //FLV

	    /* find the "duration" meta tag */
	    pos = flv_find_meta_tag(md->data +
				    sizeof(nkn_vpe_flv_tag_t),
				    md->size,
				    (uint8_t*)search_tag);
	    if( !pos ) {
		md->err = -E_MPROPS_CONTAINER_PARSE_ERR;
		return md->err;
	    }

	    /* read the value in the "duration" meta tag */
	    m.i64 = *( ptr = (uint64_t*)( flv_read_meta_value(md->data +
							sizeof(nkn_vpe_flv_tag_t)
							+ pos,
							8, 0) ) );

	    /* compute total bitrate */
	    md->mp->v_video_bitrate =
		vpe_mprops_compute_bitrate(md->tot_size, m.d);

	    free(ptr);
	    break;
    }

    return 0;
}

/**
 * @brief retrieves the fully populated  media properties data type from
 * the media detector context
 * @param md [in] - media detector context
 * @return returns the media properties data type
 */
vpe_media_props_t*
vpe_mprops_get_media_attr(vpe_media_detector_ctx_t *md)
{
    if( md && md->mp ) {
	return md->mp;
    }
    return NULL;
}

/**
 * @brief cleans up the resources used by the media detector context
 * @param md [in] - media detector context
 */
void
vpe_mprops_cleanup_media_detector(vpe_media_detector_ctx_t *md)
{

    if( md && md->mp ) {
	switch(md->mp->v_cont_type) {
	    case CONTAINER_TYPE_MP4: //MP4
	    case CONTAINER_TYPE_3GPP:
	    case CONTAINER_TYPE_3GP2:
		//mp4_yt_cleanup_udta_copy((moov_t*)md->parser_ctx);
		mp4_moov_cleanup((moov_t*)md->parser_ctx,
				 ((moov_t*)md->parser_ctx)->n_tracks);
		break;
	    case CONTAINER_TYPE_FLV: //FLV
		flv_parser_cleanup((flv_parser_t *)md->parser_ctx);
		break;
	    case CONTAINER_TYPE_WEBM:
		nkn_vpe_webm_clean_segment((nkn_vpe_webm_segment_t*)md->parser_ctx);
		break;
	    case CONTAINER_TYPE_ASF:
		nkn_vpe_asf_clean_header_obj((nkn_vpe_asf_header_obj_t*)md->parser_ctx);
		break;
	}
	free(md->mp);
	free(md);
    }

}

/***************************************************************
 *                      HELPER FUNCTIONS
 **************************************************************/
static inline void
vpe_mprops_init_media_attr(vpe_media_props_t *mp)
{
    mp->vtype = 0x01;
    mp->v_subtype = 0x01;
}

static inline double
vpe_mprops_compute_bitrate(size_t tot_size, double duration)
{

    return ( ( ( tot_size/duration ) * 8 ) / 1000 ); //kbps
}
