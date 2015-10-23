/*
 *
 * Filename:  nkn_vpe_metadata.h
 * Date:      2009/02/25
 * Module:    Nokeena's Video Meta Data Container Format
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.  
 * All rights reserved.
 *
 *
 */

#ifndef _NKN_VPE_METADATA_
#define _NKN_VPE_METADATA_

#include <stdio.h>	
#include <stdio.h>
#include <pthread.h>

#ifndef MPEG_TS_TPLAY_PARSER
#include "server.h"
#endif 

#ifdef __cplusplus
extern "C"{
#endif

#define MAX_NKN_SMOOTHFLOW_PROFILES     16      // Max number of profiles supported
#define MAX_NKN_SESSION_ID_SIZE         40

#define NKN_META_DATA_SIGNATURE 0x4e6f4a6974746572
#define FEATURE_SMOOTHFLOW 0x00000001
#define FEATURE_TRICKPLAY 0x00000002
#define FEATURE_AVCC_PACKET 0x00000003

// CODEC TYPES
#define VCODEC_ID_VP6     0x00000001
#define VCODEC_ID_AVC     0x00000002
#define ACODEC_ID_MP3     0x00010000
#define ACODEC_ID_AAC     0x00020000

// CONTAINER TYPES
#define CONTAINER_ID_FLV  0x00000001
#define CONTAINER_ID_MP4  0x00000002
#define CONTAINER_ID_TS   0x00000003

// TRICK PLAY RECORD TYPE CONSTANTS
#define TPLAY_RECORD_UNPARSED  0
#define TPLAY_RECORD_VSH  1
#define TPLAY_RECORD_GOP  2
#define TPLAY_RECORD_PIC_NON_IFRAME 3
#define TPLAY_RECORD_PIC_IFRAME 4
#define TPLAY_RECORD_JUNK 5
#define TPLAY_RECORD_VIDEO_UNDEF 6
#define TPLAY_RECORD_REQD_INFO 7

#define FEATURE_TAG_SIZE sizeof(nkn_feature_tag)
#define FEATURE_TABLE_SIZE sizeof(nkn_feature_table)
#define META_HDR_SIZE sizeof(nkn_meta_hdr)

#pragma pack(push, 1)

    //struct nkn_feature_table;//fwd declaration

    typedef struct _tag_nkn_meta_hdr{
	uint64_t meta_signature;
	uint32_t n_features;
    }nkn_meta_hdr;

    typedef struct _tag_nkn_feature_table{
	uint32_t feature_id;
	uint64_t feature_tag_offset;
    }nkn_feature_table;

    typedef struct  _tag_nkn_feature_tag{
	uint32_t feature_version;
	uint32_t feature_id;
	uint32_t tag_body_size;
    }nkn_feature_tag;

    typedef struct _tag_sf_meta_data{
	uint32_t version;
	uint32_t n_profiles;
	uint32_t profile_rate[MAX_NKN_SMOOTHFLOW_PROFILES];
	uint32_t afr_tbl[MAX_NKN_SMOOTHFLOW_PROFILES];
	uint32_t keyframe_period; 
	uint32_t sequence_duration;
	uint32_t chunk_duration;
	uint32_t num_chunks;
	uint64_t pseudo_content_length;
	uint32_t codec_id;
	uint32_t container_id;
    }sf_meta_data;

    typedef struct _tag_tplay_meta_data{
	uint32_t keyframe_period;
	uint32_t n_iframes;
    }tplay_meta_data;

    typedef struct _avcc_config{
	  uint32_t avcc_data_size;
	char  *p_avcc_data;
    }avcc_config;

    /** Trick Play Feature Table
     */


    typedef struct _tag_trickplay {
	/** size of the trick play header
	 */
	uint32_t size;

	/*the container type and codec type (audio and video) present in the stream
	  1 - FLV/VP6
	  2 - FLV/H.264
	  3 - MP4/H.264
	  4 - MPEG-2 TS/MPEG-2
	  5 - MPEG-2 TS/H/.264
	*/
	uint8_t codec_sub_type;
    }tplay_index;

    /** trick play record 
     */
    typedef struct _tag_trickplay_index_record {
	/** Record Type 8 - buit mask
	 * TPLAY_RECORD_UNPARSED = 0
	 * TPLAY_RECORD_VSH = 1
	 * TPLAY_RECORD_GOP = 2
	 * TPLAY_RECORD_PIC_NON_IFRAME = 3
	 * TPLAY_RECORD_PIC_IFRAME = 4
	 * TPLAY_RECORD_JUNK = 5
	 */
	uint8_t type;

	/**
	 * start position/ offset in bytes within the 188 byte TS packet cannot be greater than 188 bytes
	 */
	uint8_t start_offset;

	/**
	 * Size in bytes within the 188 byte TS packet
	 * Note start_offset+size < 188 bytes
	 */
	uint8_t size;

	/**
	 * Stores the Program Time Stamp (PTS) as a 3 byte integer and 1 byte fraction
	 */
	int64_t PTS;

	/**
	 * Transport Stream Packet Number in little ending format
	 */
	uint32_t pkt_num;

	/**
	 * Next Random Access Pkt Number
	 */
	uint32_t next_tplay_pkt;

	/**
	 * Previous Random Access Pkt Number
	 */
	uint32_t prev_tplay_pkt;
    }tplay_index_record;
	
    //META FILE API
    typedef struct _tag_meta_data_reader{
        uint8_t *p_data;
        uint64_t pos;
        uint8_t * p_start;

        nkn_meta_hdr *hdr;
        nkn_feature_table *feature_tbl;

    }meta_data_reader;

    typedef struct nkn_ssp_session_params {
	sf_meta_data        *sflow_data;

	char 		    session_id[MAX_NKN_SESSION_ID_SIZE];
	uint8_t             sflow_flag;
	// Current State
	uint32_t            curr_profile;
	uint32_t            curr_chunk;
	// Requested State
	uint32_t            next_profile;
	uint32_t            next_chunk;

	pthread_mutex_t     lock;

	//URI handling params
	uint32_t            vidname_end;

    } nkn_ssp_session_t;

#pragma pack(pop)
    //META FILE API
    /** Initializes the Meta Data Read Handle
     *@param void *meta_data - buffer containing the meta data
     *@return meta_data_reader* - handle to the meta data reader, should be passed in subsequent calls
     */
    meta_data_reader* init_meta_reader(void *meta_data);

    /** Gets a feature using the feature tabel
     *@param meta_data_reader *mdr - handle to the meta data reader
     *@param uint32 feature_id - feature id to fetch
     *@return returns the feature specific meta data (typecast it into respective objects). Returns NULL on error
     */
    void* get_feature(meta_data_reader* mdr, uint32_t feature_id);

    /** Destroy the Meta Data Reader Handle
     *@param meta_data_reader *mdr - handle to the meta data reader
     *@return returns '0' on no error and a negative number on error
     */
    int destory_meta_data_reader(meta_data_reader *mdr);

#ifdef __cplusplus
}
#endif

#endif //_NKN_VPE_METADATA_
 
