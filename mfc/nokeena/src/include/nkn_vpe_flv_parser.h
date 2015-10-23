/*
 *
 * Filename:  nkn_vpe_flv_parser.h
 * Date:      30 APR 2009
 * Module:    iniline parser for video data
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

#ifndef _FLV_PARSER_
#define _FLV_PARSER_

#include <stdlib.h>
#include "nkn_vpe_flv.h"
#include "nkn_vpe_backend_io_plugin.h"
#include "nkn_vpe_bitio.h"
//#include "nkn_vpe_parser_api.h"

#ifdef __cplusplus
extern "C"{
#endif
#define E_VPE_FLV_DATA_INSUFF -1

// BZ 8382; Youtube Transcoding support
#define YT_ONMETADATA_SIZE  843  //datasize of youtube onmetadata tag
#define YAMDI_META_NEED_NUM 7
#define FLV_HEADER_SIZE (9+4)
#define FLV_TAG_HEADER_SIZE 11
#define FLV_PREVIOUS_TAG_SIZE 4
#define YT_ECMAAARRAYLEN 15

#define FLV_PARSER_VID_DESC_SPAN 1
#define FLV_PARSER_AUD_DESC_SPAN 2
#define FLV_PARSER_TAG_HDR_SPAN  4
#define FLV_PARSER_AVCC_SPAN 8
#define FLV_PARSER_AAC_SI_SPAN 16

    typedef union tag_meta_values{
	uint64_t i64;
	int32_t i32;
	double d;
    }mval;

    typedef struct tag_flv_meta_tag_modifiers {
	char *name;
	size_t pos;
	int32_t type;
	mval val;
    }flv_meta_tag_modifier_t;

    typedef struct tag_flv_seek_params {
	int32_t seek_to;
	size_t seek_pos;
	flv_meta_tag_modifier_t *mtm;
	int32_t n_items_to_modify;
	int32_t seek_vts;
    }flv_seek_params_t;
    
    typedef struct tag_flv_meta_tag {
	uint8_t *data;
	size_t size;
	nkn_vpe_flv_tag_t tag;
    }flv_meta_tag_t;

    /* BZ 8382 */
    typedef struct tag_flv_yamdi_onmetadata_tag {
        char   *metaDataCreator;
        uint8_t hasKeyFrames;
        uint8_t hasVideo;
        uint8_t hasAudio;
        uint8_t hasMetaData;
        uint8_t canSeekToEnd;
        double duration;
        double dataSize;
        double videoSize;
        double frameRate;
	double videoDataRate;
        double videoCodecId;
        double width;
        double height;
        double audioSize;
        double audioDataRate;
        double audioCodecId;
        double audioSampleRate;
        double audioSampleSize;
        uint8_t stereo;
        double fileSize;
        double lastTimestamp;
        double lastKeyFrameTimestamp;
        double lastKeyFrameLocation;
        double *filePositions;
        double *times;
    }flv_yamdi_onmetadata_tag_t;

    typedef struct tag_flv_yt_onmetadata_string {
	char* Data;
	uint32_t DataLen;
    }flv_yt_onmetadata_string;

    typedef struct tag_flv_yt_onmetadata_tag {
	double duration;// for yt seek
	double startTime;// for yt seek
	double totalDuration;
	double width;
	double height;
	double videoDataRate;
	double audioDataRate;
	double totalDataRate;
	double frameRate;
	double byteLength;// for yt seek
	double canSeekOnTime;

        flv_yt_onmetadata_string sourceData;
	flv_yt_onmetadata_string purl;
	flv_yt_onmetadata_string pmsg;
	flv_yt_onmetadata_string httpHostHeader;
    }flv_yt_onmetadata_tag_t;


    typedef struct tag_data_span {
	uint8_t data_span_flag;
	size_t bytes_in_curr_span;
	size_t bytes_in_next_span;
    }data_span_t;

    typedef struct tag_flv_parser {
	nkn_vpe_flv_header_t *fh;
	nkn_vpe_flv_tag_t *ft;
	uint8_t span_state;
	int32_t bytes_to_next_tag;
	size_t parsed_bytes_in_block;
	size_t parsed_bytes_in_file;
	void *iod;
	flv_seek_params_t *sp;
	flv_meta_tag_t *mt;
	flv_meta_tag_t *xmp_mt; /* some FLV file may have XMP metadata*/
	uint8_t *avcc;
	size_t avcc_size;
	uint8_t *aac_si;
	data_span_t *ds;
	size_t aac_si_size;
    }flv_parser_t;	
    
#define nkn_vpe_flv_video_codec_id(tag)     (((tag) & 0x0F) >> 0)
#define nkn_vpe_flv_video_frame_type(tag)   (((tag) & 0xF0) >> 4)
#define nkn_vpe_flv_audio_codec_id(tag)   (((tag) & 0xF0) >> 4)

//    inline int32_t nkn_vpe_flv_get_timestamp(nkn_vpe_flv_tag_t tag);
//    inline void flv_tag_set_timestamp(nkn_vpe_flv_tag_t * tag, uint32_t timestamp);
    flv_parser_t* flv_init_parser(void);
    int32_t flv_parse_header(flv_parser_t *fp, uint8_t *data, size_t size);
    int32_t flv_parse_data(flv_parser_t *fp, uint8_t *data, size_t size);
    int32_t flv_set_seekto(flv_parser_t *fp, double seek_to, const
			   char *meta_tag_modifier_list[], int32_t n_items_to_modify);
    int32_t flv_get_meta_tag_info(flv_parser_t *fp, uint8_t *data,
				  size_t size, 
				  size_t *meta_tag_offset, 
				  size_t *meta_tag_length,
				  int32_t *video_codec,
				  int32_t *audio_codec);
    void flv_parser_cleanup(flv_parser_t *fp);

    int32_t flv_find_meta_tag(uint8_t *mt_start, int32_t mt_size, uint8_t* search_str);
    void* flv_read_meta_value(uint8_t *data, size_t size, uint8_t type);

/*
 * BZ: 8382
 * yamdi onmetadata to youtube onmetadata
 *
 */

    int32_t yt_flv_onmetadata_convert(uint8_t *mt_ori_start, uint32_t mt_ori_size,
				      uint8_t *mt_modi_start, uint32_t mt_modi_size);

    int32_t yt_flv_yamdi_onmetadata_parse(uint8_t *mt_ori_start, uint32_t mt_ori_size,
			      	          flv_yamdi_onmetadata_tag_t *yamdi_onmetadata);

    int32_t yt_flv_yamdi_find_useful_meta(bitstream_state_t *bs_in,
					  char *field, uint32_t len,
					  flv_yamdi_onmetadata_tag_t *yamdi_onmetadata,
					  int32_t *skip_bytes);

    int32_t yt_flv_yamdi2yt_onmetadata(flv_yamdi_onmetadata_tag_t *yamdi_onmetadata,
				       uint32_t mt_ori_size,
                                       flv_yt_onmetadata_tag_t *yt_onmetadata,
				       uint32_t mt_modi_size);

    int32_t yt_flv_yt_onmetadata_create(uint8_t *mt_modi_start, uint32_t mt_modi_size,
			    		flv_yt_onmetadata_tag_t *yt_onmetadata);

    int32_t yt_flv_write_buf_dataval_bool(bitstream_state_t *bs_out,
					  uint8_t boolValue);

    int32_t yt_flv_write_buf_dataval_num(bitstream_state_t *bs_out,
					 double number);

    int32_t yt_flv_write_buf_dataval_str(bitstream_state_t *bs_out,
					 const char* strData,uint16_t strSize);

    int32_t yt_flv_write_buf_data_str(bitstream_state_t *bs_out,
				      const char* strData, uint16_t strSize);

    int32_t yt_flv_write_buf_objprop_bool(bitstream_state_t *bs_out,
					  const char* propName, uint8_t boolValue);

    int32_t yt_flv_write_buf_objprop_num(bitstream_state_t *bs_out,
					 const char* propName, double number);

    int32_t yt_flv_write_objprop_str(bitstream_state_t *bs_out,
				     const char* propName,char* propData,
				     uint16_t propDataSize);

    int32_t yt_flv_write_buffer_scriptdata_ecmaarry(bitstream_state_t *bs_out,
					            uint32_t ECMAArrayLength,
             			                    flv_yt_onmetadata_tag_t *yt_onmetadata);

#ifdef __cplusplus
}
#endif

#endif //_FLV_PARSER_

