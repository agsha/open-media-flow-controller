/*
 *
 * Filename:  nkn_vpe_mfu_defs.h
 * Date:      2010/05/26
 * Module:    MFU definitions (see specification for more details)    
 *
 * (C) Copyright 2020 Juniper Networks, Inc.
 * All rights reserved.
 *
 *
 */

#ifndef _MFU_DEFS_
#define _MFU_DEFS_

#ifdef __cplusplus
extern "C" {
#endif

  /**
     * \struct Media Fundamental unit box
     */
    typedef struct tag_mfub
    {
	uint16_t version; /**< version of the MFU box; MajorVersion(8 bytes):MinorVersion(8 bytes)*/
	uint16_t program_number;/*our encaps number*/
	uint16_t stream_id;/*unique id to identify each streaam*/

	uint16_t flags; /*
			  Bit 2: 1: big endian is present;
			  Bit 1: 1: Present encap  is new;0: default
			  Bit 0: 1: Present stream is new;0: default
			*/



	uint32_t sequence_num;  /** <sequence number for a MFU box; we
				 *  can roll over*/
	uint32_t timescale; /**< timescale sampling frequency for the
			     * timestamps */
	uint32_t duration;  /**< duration of the MFU */
	
#ifdef VERSION1
	uint32_t video_duration;/* Indicative of video duration*/
#endif
	
	uint32_t audio_duration; /*Duration of audio in the native \
				   timescale*/
	uint16_t video_rate; /**< video bit rate of the MFU in kbps */
	uint16_t audio_rate; /**< audio bitrate of the MFU in kbps  */
	uint16_t mfu_rate; /**< consolidated mfu rate for interleaved
			    * content */
	
	uint8_t video_codec_type:4;  /**< video codec type 
				      * 0:reserved;
				      * 1:AVC;
				      * 2:VC-1; 
				      * 3: */
	uint8_t audio_codec_type:4;  /**< audio codec type 
				      * 0:reserved; 
				      * 1:AAC-LC; 
				      * 2:AAC-HE; 
				      * 3: MP3;
				      * 4: */ 

	uint32_t compliancy_indicator_mask; /** <compliancy indicator is
					     * a 32-bit field mask to
					     * indicate  compliancy
					     * (for  iphone, ipad, sl,
					     * zeri, etc) */
	
	uint64_t offset_vdat; /**< offset to the first video
			       * elementary stream unit  */
	uint64_t offset_adat;/**< offset to the first audio elementary
			      * stream unit */
	uint32_t mfu_size;/* size of the entire mfu */
    } mfub_t;

  typedef struct mfu_discont_ind {

    uint8_t discontinuity_flag;
    uint64_t timestamp;
  }  mfu_discont_ind_t;

typedef enum formatter_types{
    FMTR_NONE = 0,
    FMTR_APPLE = 1,
    FMTR_SILVERLIGHT = 2,
} formatter_types_e;

typedef enum formatter_err_types{
    ERR_NONE = 0,
    ERROR_OUTPUT_IO = 1,
    ERROR_INPUT_DATA = 2
}formatter_err_types_e;



typedef struct mfu_data_st{
    uint8_t *data;//pointer to MFP
    uint32_t data_size;//sixe of mfu
    uint32_t data_id;
    void *sl_fmtr_ctx;
    void *fruit_fmtr_ctx;
    void *zeri_fmtr_ctx;
    mfu_discont_ind_t **mfu_discont;
    formatter_types_e fmtr_type;
    formatter_err_types_e fmtr_err;
}mfu_data_t;

#define UNIT_COUNT_SIZE 4
#define TIMESCALE_SIZE 4
#define DEFAULT_UNIT_DURATION_SIZE 4
#define DEFAULT_UNIT_SIZE_SIZE 4
#define CODEC_INFO_SIZE_SIZE 4
#define BOX_NAME_SIZE 4
#define BOX_SIZE_SIZE 4

    /**
     * /struct MFU raw descriptor unit
     */
    typedef struct tag_mfu_raw_desc{
	uint32_t unit_count; /**< number of fundamental codec units:  
			      * samples/AUs/slices etc */
	uint32_t timescale; /**< Timescale in which timestamps are
			       present */
	uint32_t default_unit_duration; /**< 0 if unit durations are
					 * not the same */
	uint32_t default_unit_size; /**< 0 if unit sizes are not the
				     * same for all samples */
#ifdef VERSION1
	uint32_t timestamp_bias;/*
				  Indicative of the bias in the first
				  tiemstap of the first sample of the 
				  MFU. For instance if this is 100,
				  the timestamp for the first sample 
				  would have to be incremented by this
				  value.
				 */
#endif
	uint32_t *sample_duration;//This is sample_durationof each sample
	uint32_t *composition_duration;//Let this be NULL for now
	uint32_t *sample_sizes;/*NULL if all samples of same size
				 This would be size of an AU with NAL byte separator*/
	uint32_t codec_info_size; /**< Size Codec specific information
				   * to be added: 0 if nothing for H264 sps_pps_size*/
	uint8_t *codec_specific_data;//Has sps followed by pps
    } mfu_rw_desc_t;

    /** 
     * /struct MFU raw fragment tag
     */
    typedef struct tag_raw_fg{
	int32_t videofg_size; /**< Size of video codec descriptor */
	mfu_rw_desc_t videofg;
	int32_t audiofg_size; /**< size of audio codec descriptor */
	mfu_rw_desc_t audiofg;
    }mfu_rwfg_t;


#ifdef __cplusplus
}
#endif

#endif

