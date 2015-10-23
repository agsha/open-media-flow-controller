#ifndef _NKN_VPE_ISMV_MFU__
#define _NKN_VPE_ISMV_MFU__

#include "mfp_limits.h"
#include "nkn_vpe_mp4_parser.h"
#include "nkn_vpe_bitio.h"
#include <glib.h>
#include "nkn_vpe_mp4_seek_parser.h"
#include "nkn_vpe_types.h"
#include "nkn_vpe_mp4_ismv_access_api.h"
#include "nkn_vpe_mp4_seek_api.h"
#include "nkn_vpe_mfu_writer.h"

//#define MAX_VID_TRAKS 5
//#define MAX_AUD_TRAKS 5
//#define MAX_PROFILE 5
//#define MAX_MOOF_COUNT 5000
#define MAX_SPS_PPS_SIZE 300

/*Generic error codes*/
//#define VPE_SUCCESS 1
//#define VPE_ERROR 0

#define VPE_MFP_H264_CODEC 1
#define _BIG_ENDIAN_
typedef struct mdatdesc{
    uint32_t sample_count; //number of samples
    uint32_t total_sample_size;
    uint32_t default_sample_duration; // 0 if smpl durations are not the same 
    uint32_t default_sample_size;// 0 if sample sizes are not the same for all samples
    uint64_t timescale; 
    uint32_t* sample_duration; // NULL if all sampels of same durations
    uint32_t* composition_duration;
    uint32_t* sample_sizes; // NULL if all samples of same size.
    uint8_t* mdat_pos; //Pointer to all sample data
    uint8_t** dat_pos; //pointer to each sample.
}mdat_desc_t;

typedef struct adp{
    FILE * fout; //Output file for dump
    mdat_desc_t* out;  // Used for imfu
    uint8_t *moof_data;
    uint32_t moof_size;
    uint32_t moof_offset;
    uint32_t timescale;
    GList *list;
    /*Add ADTS headers*/
    uint32_t esds_flag; //Flag 1: if esds if already prepared.
    uint8_t* esds;
    void* adts; //adts strucutre as defined in the aac file.
    int32_t header_size;//0: if the header is NOT fixed. Else the size of the fixed header
    uint8_t* header; //Usually headers are computed once per
    // stream. This corresponds to fixed header.
    uint32_t track_num;
    uint32_t num_moof_stored;
    uint32_t num_moof_processed;
    uint32_t *moof_len;
    uint8_t **moofs;
    int32_t profile;
    uint64_t total_sample_duration;
    int32_t num_moofs;
    int32_t profile_seq;
}audio_pt;


typedef struct vdp{
    FILE * fout;  //Output file for dump
    mdat_desc_t* out;  // Used for imfu
    GList *list;
    uint8_t *moof_data;
    uint32_t moof_size;
    uint32_t moof_offset;
    uint32_t timescale;
    uint32_t sps_pps_size;
    uint8_t* sps_pps;
    /*Add NAL headers*/
    int32_t codec_type;
    int32_t trak_id;
    int32_t stbl_size;
    int32_t stbl_offset;
    int32_t stsd_size;
    int32_t stsd_offset;
    int32_t avc1_size;
    int32_t avc1_offset;
    int32_t avcC_size;
    int32_t avcC_offset;
    int32_t dump_pps_sps;
    /*AVC sspecific info */
    void* avc1;
    uint32_t track_num;
    uint32_t num_moof_stored;
    uint32_t num_moof_processed;
    uint32_t *moof_len;
    uint8_t **moofs;
    int32_t profile;
  //FILE *ismv_file;
    void *io_desc;
    uint64_t total_sample_duration;
    int32_t num_moofs;
    int32_t profile_seq;
}video_pt;



typedef struct ismvmp2{
    uint32_t dump_moof; // 1: if no headers need to be added. Else 0. 
    uint32_t file_dump; //1: for file dump; 0: for imfu dump
    uint32_t mpeg2ts_flag;
    audio_pt audio[MAX_TRACKS];
    video_pt video[MAX_TRACKS];
    int32_t n_traks; //Number of tracks: already populated. 
    int32_t n_aud_traks; //Number of audio traks
    int32_t n_vid_traks; // Number of video traks
    int32_t aud_trak_num; //Present audio track number
    int32_t vid_trak_num; // Present video track number
    uint32_t chunk_time;
    int32_t num_chunks;
    //uint32_t Is_first_audio;
    //uint32_t Is_audio;
    uint32_t Is_single_bit_rate;
    char *audio_file_name;
    FILE *fin;
    char*fname;
    uint32_t vid_cur_frag;
    uint32_t aud_cur_frag;
    uint32_t vid_pos;
    uint32_t aud_pos;
    uint32_t chunk_num;
    uint32_t profile_num;
    uint16_t video_rate;
    uint16_t audio_rate;
    uint32_t num_moof_count;
    moof2mfu_desc_t *moof2mfu_desc;
    int32_t num_pmf;
    uint32_t is_last_mfu;
    uint32_t num_moof_processed;
    uint32_t total_moof_count;
    uint32_t seq_num;
    uint8_t is_dummy_moofs_written;
    io_handlers_t *iocb;
    void *io_desc;
}ismv_parser_t;
typedef ismv_parser_t ismv2ts_builder_t;
typedef ismv_parser_t moof2mfu_builder_t;

typedef struct ismv2ts_profile_tt{
    uint32_t chunk_time;
    char *ismv_name;
    char *output_name;
    uint32_t ts;
    uint16_t video_rate;
    uint16_t audio_rate;
}ismv2ts_profile_t;

typedef struct ismv2ts_converter_tt{
    uint32_t n_profiles;
    uint32_t profile_num;
    char *m3u8_name;
    ismv2ts_profile_t profile_info[MAX_PROFILES];
    ismv_parser_t parser_data[MAX_PROFILES];
    ismv_parser_ctx_t *ctx[MAX_PROFILES];

}ismv2ts_converter_t;



//int32_t mfu_convert_ts_timescale(mdat_desc_t*, uint32_t*);
int32_t mfp_ismv_to_mfu(char *,char*,uint32_t,int32_t*,uint32_t *,char *);
//uint32_t init_ismv_mfu_parser(ismv_parser_t*);
//uint32_t ismv_dump_mfu(ismv_parser_ctx_t *, size_t *, size_t *,ismv_parser_t* );//ctx, &moof_offset, &moof_size,parser_data);
int32_t mfu_get_sample_from_moof(size_t* sample_offset, size_t* sample_len, int32_t sample_number);
//uint32_t mfu_handle_ismv_track(ismv_parser_ctx_t *, size_t *, size_t *,ismv_parser_t*, int32_t);
int32_t mfu_handle_video_moof(uint8_t*, video_pt*, moof2mfu_desc_t*);
int32_t mfu_handle_audio_moof(uint8_t * ,audio_pt*, moof2mfu_desc_t*);
//int32_t init_mfu_parser(ismv_parser_t* ,moov_t*);
int32_t mfu_capture_video_headers(stbl_t* ,ismv_parser_t* );
int32_t mfu_capture_audio_headers(stbl_t*,audio_pt*);
//int32_t mfu_get_mdat_desc(uint8_t *, mdat_desc_t*);
int32_t mfu_clean_video_headers(ismv_parser_t* );
int32_t mfu_clean_audio_headers(ismv_parser_t*);
//mdat_desc_t* mfu_init_mdat_desc();
//int32_t mfu_free_mdat_desc(mdat_desc_t *);
//void mfu_tdump_video_moofs(video_pt* );
//void mfu_tdump_audio_moofs(audio_pt*);
//int32_t mfu_convert2ts(ismv_parser_t*);
int32_t ismv2ts_convert(ismv2ts_converter_t*);




#endif
