#ifndef __NKN_SSP_CB__H__
#define __NKN_SSP_CB__H__

#include "nkn_vpe_bitio.h"
#include "nkn_vpe_flv_parser.h"
#include "nkn_attr.h"
//#include "nkn_vpe_mp4_parser.h"
/*
 * Server Side Player Control Block
 */

#define CONTAINER_TYPE_FLV      0x00000001  //FLV container
#define CONTAINER_TYPE_MP4      0x00000002  //MP4 container
// MAX_SSP_BW_FILE_TYPES  2 is defined in nkn_namespace.h
#define CONTAINER_TYPE_WEBM     0x00000003  //WEBM container
#define CONTAINER_TYPE_ASF      0x00000004  //ASF container
#define CONTAINER_TYPE_3GPP     0x00000005  //3GPP
#define CONTAINER_TYPE_3GP2     0x00000006  //3GP2
#define MAX_NKN_SMOOTHFLOW_PROFILES     16      // Max number of profiles supported
#define MAX_NKN_SESSION_ID_SIZE         40

#define NKN_SSP_SMOOTHSTREAM_STREAMTYPE_MANIFEST (1)
#define NKN_SSP_SMOOTHSTREAM_STREAMTYPE_VIDEO	 (2) //video/mp4
#define NKN_SSP_SMOOTHSTREAM_STREAMTYPE_AUDIO	 (3) //audio/mp4
#define NKN_SSP_FLASHSTREAM_STREAMTYPE_FRAGMENT  (4) //video/f4f


typedef struct nkn_ssp_params {
    uint8_t       num_profiles;
    uint8_t	  attr_state;
    uint16_t      chunk_duration; // duration of chunks in seconds

    uint32_t      profile_rates[MAX_NKN_SMOOTHFLOW_PROFILES];     // profile-rate map in bytes/sec
    uint32_t      duration;       // total duration in seconds

    uint8_t       ssp_hash_authed;   // 1: Have finished the Hash Authentication, 0: not finished

    uint8_t       ssp_activated;  // set to true once metafile is read
    uint8_t       allow_download; // default 0(no); 1(yes)
    uint8_t       ssp_callback_status;
    uint8_t       ssp_scrub_req;

    uint16_t      ssp_client_id;  //sets the client id
    uint16_t      ssp_container_id;  // sets the container id

    uint16_t      ssp_bitrate;    //the bitrate for con's video file, used for AFR
    uint16_t      ssp_codec_id;

    uint32_t      ssp_attr_partial_content_len; // for the AFR detect
    uint8_t       *afr_header;     // store the header for AFR detect
    uint64_t      afr_header_size; // size of header for AFR detect
    bitstream_state_t *afr_buffered_data; // for AFR
    /*
     * The seek of webm video uses the byte range request of Http header.
     * But the AFR detect need several SEND_DATA_BACK, which will modi the byte range request
     * As a result, we will store the byte range request from client before the AFR detect,
     * and set the byte range request back after the AFR detect.
     */
    off_t         client_brstart;
    off_t         client_brstop; // for the AFR of webm

    uint8_t       rate_opt_state; // Youtube up/downgrade
    uint8_t       serve_fmt;      // Youtube up/downgrade

    uint32_t      ssp_container_filetype; // bool to show that the ssp has obtained the container format for this con

    uint32_t      ssp_video_br_flag; // This flag notice that the client video request is using byte range, such as youtube WebM

    uint8_t 	  ss_track_type; // Smoothstreaming states
    uint16_t      ss_track_id;
    uint32_t      ss_bit_rate;
    uint64_t      ss_seek_offset;

    int64_t 	  fs_seg_index; // Flashstreaming states
    int64_t 	  fs_frag_index;
    int16_t       fs_segurl_pos;

    uint64_t      ssp_content_length;
    uint32_t      ssp_partial_content_len;

    uint8_t 	  ssp_streamtype;
    uint8_t       seek_state; // To maintain state between the multiple internal loopbacks
    uint8_t       *mp4_header;
    uint8_t       *header;

    uint8_t       transcode_state; // To support BZ 8382; Trancoding feature

    uint64_t      mp4_header_size;
    uint64_t      header_size;
    uint64_t      mp4_mdat_offset;

    size_t 	  ssp_br_offset;
    size_t 	  ssp_br_length;

    void          *private_data;

    bitstream_state_t *buffered_data;
    flv_parser_t  *fp_state;

    void* mp4_fp_state; // mp4 seek store for moov detect
    void* mp4_afr_state; // mp4 bitrate detection store for moov detect

    nkn_attr_priv_t *vpe_attr;

} nkn_ssp_params_t;


#endif // __NKN_SSP_CB__H__
