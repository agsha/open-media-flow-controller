/*
 *
 * Filename:  nkn_vpe_mp42mfu_api.h
 * Date:      2010/08/02
 * Module:    implements the mp42mfu file converter callbacks
 *
 * (C) Copyright 2008 Nokeena Networks, Inc.
 * All rights reserved.
 *
 *
 */

#ifndef _MP42MFU_CONV_
#define _MP42MFU_CONV_

#include <stdio.h>
#include <inttypes.h>
#include "nkn_vpe_types.h"
#include "nkn_vpe_mfu_writer.h"
#include "nkn_vpe_ismv_mfu.h"
#include "nkn_vpe_mp4_parser.h"
#include "nkn_memalloc.h"
#include "nkn_vpe_raw_h264.h"
#include "nkn_vpe_moof2mfu.h"
#include "nkn_vpe_mfu_aac.h" 
#include "nkn_vpe_sync_http_read.h"
#include "nkn_vpe_utils.h"
//#include "nkn_vpe_mp42anyformat_api.h"
#ifdef __cplusplus
extern "C" {
#endif


    typedef struct tag_mp4_parser_ctx {
        moov_t *moov;
        int32_t n_traks;
        io_handlers_t *iocb;
    }mp4_parser_ctx_t;

    struct sample_prop;

    typedef struct sample_desc {
	size_t offset;
        size_t length;
    }sample_desc_t;

    //struct sample_prop;

    typedef struct sample_prop{
	sample_desc_t *s_list;
	int32_t s_list_num_entries;
	uint32_t is_updated;
        uint32_t sample_num;
        uint64_t sample_offset;
        int32_t chunk_num;
        uint64_t chunk_offset;
        uint32_t sample_size;
	struct sample_prop *prev_sample;
    }sample_prop_t;

    typedef struct aud{
	uint32_t timescale;//timescale of this track
	uint32_t track_id;//track id of this track
	uint32_t trak_num;//track number of this track
	uint64_t duration;//duration of trak
	uint32_t num_sync_points;// number of sync points
	uint32_t num_samples;//specifies the total number of samples
	uint16_t bit_rate;//audio bit rate
	uint64_t total_sample_duration;
	sample_prop_t **sample_data_prop;/*set of
							    sample vectors*/
	uint64_t *sync_time_table;/*this table has the
	                                      time duration(in millisec) of all the
					      sync points in each
					      track */
	uint32_t *sync_sample_table;/*this table has the
                                              number of samples that
					      forms one moof data */
	uint64_t *sync_sam_size_table;/* this table has
						     the total sample
						     size that forms
						     one moof data*/
	uint32_t *sync_start_sam_table; /* this table has
						       the sample
						       number from
						       which each moof
						       starts(ie)similar to stss*/
	uint8_t **chunk_data;/* this has the audio chunk data
				       that can form one MFU */
	uint8_t *stsz_data;
	uint32_t *sample_time_table;
	uint32_t default_sample_size;
	uint32_t default_sample_duration;
	uint32_t num_chunks_processed;
	trak_t *trak;
	uint32_t sample_time_factor;
	/*Add ADTS headers*/
	uint32_t esds_flag; //Flag 1: if esds if already prepared.
	uint8_t* esds;
	void* adts; //adts strucutre as defined in the aac file.
	int32_t header_size;//0: if the header is NOT fixed. Else the
	//	size of the fixed header
	uint8_t* header; //Usually headers are computed once per
	// stream. This corresponds to fixed header.
      uint8_t is_aud_present_chunk;
      uint8_t is_aud_prev_chunk;
    }audio_trak_t;

    typedef struct vid{
	uint32_t timescale;//timescale of this track
        uint32_t track_id;//track id of this track
	uint32_t trak_num;
	uint64_t duration;//duration of trak
        uint32_t num_sync_points;// number of sync points
	uint32_t num_samples;//specifies the total number of samples
	uint16_t bit_rate;//video bit rate
	uint64_t total_sample_duration;
	sample_prop_t **sample_data_prop;/*set of
                                                            sample vectors*/
        uint64_t *sync_time_table;/*this table has the
                                              time duration(in milli sec) of all the
                                              sync points in each
                                              track */
        uint32_t *sync_sample_table;/*this table has the
                                              number of samples that
                                              forms one moof data */
        uint64_t *sync_sam_size_table;/* this table has
                                                     the total sample
                                                     size that forms
                                                     one moof data*/
        uint32_t *sync_start_sam_table; /* this table has
                                                       the sample
                                                       number from
                                                       which each moof
						       starts (ie)
						       similar to stss*/
        uint8_t **chunk_data;/* this has the video chunk data
                                       that can form one MFU */
	uint8_t *stsz_data;
	uint32_t *sample_time_table;
	uint32_t default_sample_size;
        uint32_t default_sample_duration;
	uint32_t num_chunks_processed;
	trak_t *trak;
	uint32_t sample_time_factor;
	//AVC specific info
	void* avc1;
      uint8_t is_vid_present_chunk;
      uint8_t is_vid_prev_chunk;
    }video_trak_t;
    /**
     * \struct mfu builder, maintains state for the mfu
     *  write/processing
     */
    typedef struct tag_mp4_mfu_builder {

	void *io_desc_mp4[MAX_PROFILES];// MP4 file descriptor
        char *mp4_name[MAX_PROFILES];//MP4 file name
        char *mp4_file_path[MAX_PROFILES];// MP4 file path
	audio_trak_t audio[MAX_PROFILES][MAX_TRACKS];
	video_trak_t video[MAX_PROFILES][MAX_TRACKS];
        int32_t sync_time;//specifies the sync time
	uint32_t num_sync_points;
        int32_t n_profiles; //Number of profiles
	int32_t n_aud_traks[MAX_PROFILES]; //Number of audio traks
	int32_t n_vid_traks[MAX_PROFILES]; // Number of video traks
        int32_t streaming_type;/*specifies the streaming type either
				 mobile or smooth streaming */
        int32_t num_pmf;
        int32_t chunk_time;
	int32_t n_traks[MAX_PROFILES];
	//uint32_t num_profiles; /*specifies the number of mp4 profies
	//		 which is equal to number of video
	//		 traks */
	uint32_t profile_num;
        io_handlers_t *iocb[MAX_PROFILES];// io call back handler
	uint32_t min_aud_trak_num[MAX_PROFILES];/*min bitrate audio trak's number
				    among all the audio traks in the MP4
				    file*/
	uint16_t min_aud_bit_rate[MAX_PROFILES];// min audio bitrate
	uint32_t timescale; //LCM of both aud and vid
        uint32_t is_audio;
        uint32_t is_video;
    } mp4_mfu_builder_t;


    int32_t
    mp4_mfu_builder_init(int32_t, char**, int32_t, int32_t,
			 int32_t, mp4_parser_ctx_t**,
			 mp4_mfu_builder_t**);
    
    int32_t
    mp4_mfu_bldr_update(mp4_mfu_builder_t*, mp4_parser_ctx_t**, int32_t);
    int32_t
    mp4_get_segment(uint32_t, uint32_t, sample_prop_t**, trak_t*,
		    uint8_t *);
    
    int32_t
    mp4_get_sample_pos(uint32_t, sample_prop_t*, sample_prop_t*, trak_t*, uint32_t);
    
    int32_t
    mp4_mfu_update_avc(uint8_t*, video_trak_t*);
    int32_t
    mp4_mfu_update_all_chunk_data(mp4_mfu_builder_t*, moov_t*);
    
    int32_t
    mp4_mfu_update_all_sample_data(mp4_mfu_builder_t*, moov_t*);
    
    int32_t
    mp4_mfu_read_vid_chunk_data(uint32_t, uint32_t, uint8_t*, sample_prop_t**,
			    void*, io_handlers_t*);
    
    int32_t
    mp4_mfu_read_aud_chunk_data(uint32_t, uint32_t, uint8_t*, sample_prop_t**,
				void*, io_handlers_t*, uint8_t *, int32_t);
    int32_t
    mp4_mfu_update_vid_sync_tables(uint8_t*, uint64_t*, uint32_t*,
				   uint64_t*, uint32_t*,
				   trak_t*,uint8_t *,
				   uint32_t*, uint32_t);
    
    
    
    int32_t
    mp4_mfu_update_aud_sync_tables(uint64_t*, uint32_t, uint64_t*, uint32_t*,
				   uint64_t*, uint32_t*, trak_t*,
				   uint8_t*, uint32_t*, uint32_t);
    
    int32_t
    mp4_reader_alloc_sample_desc(sample_prop_t*, int32_t);
    
    int32_t
    mp4_mfu_conversion(audio_trak_t*, video_trak_t*,
		       mfu_data_t*, mp4_mfu_builder_t*, void *, io_handlers_t *);
    
    int32_t
    mp4_mfu_video_process(moof2mfu_desc_t*, video_trak_t*, io_handlers_t*,
			  void*);
    
    int32_t
    mp4_mfu_audio_process(moof2mfu_desc_t*, audio_trak_t*, io_handlers_t*,
			  void*);
    int32_t
    mp4_get_sample_duration(uint32_t, uint32_t, uint32_t*, trak_t*);    
    int32_t
    mp4_mfu_update_adts(uint8_t*, audio_trak_t*);
    
    void
    cleanup_mp4_mfu_builder(mp4_mfu_builder_t*);
    int32_t
    mp4_get_moov_offset(void*, size_t, size_t*, size_t*, io_handlers_t*);

    void
    mp4_mfu_clean_sample_data_prop(sample_prop_t**, uint32_t);
#ifdef __cplusplus
}
#endif

#endif //_MP42MFU_CONV_
