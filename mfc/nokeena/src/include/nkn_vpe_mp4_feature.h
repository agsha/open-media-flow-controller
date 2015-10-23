#ifndef _MP4_FEATURE_
#define _MP4_FEATURE_

//#include <inttypes.h>

#include <stdlib.h>
#include <inttypes.h>
#include "nkn_vpe_mp4_parser.h"
#include "glib.h"
#include "nkn_vpe_bufio.h"

#pragma pack(push, 1)
#ifdef __cplusplus
extern "C" {
#endif

#define _USE_INTERFACE_
#define VPE_VFS_SECURE
    //#define _SUMALATHA_OPT_MAIN_



#define NKN_FSEEK 0
#define NKN_TPLY  1
#define MAX_TRAKS 10


#define MPEG4_AUDIO 0
#define MPEG4_VIDEO 1
#define H264_VIDEO 2



#define VIDEO_TRAK 0
#define AUDIO_TRAK 1
#define VIDEO_HINT_TRAK 2
#define AUDIO_HINT_TRAK 3

#define SAMPLES_TO_BE_SENT 1

#define MP4_PARSER_ERROR -10

#define NOMORE_SAMPLES -1


#define _NKN_PLAY_  0
#define _NKN_SEEK_  1
#define _NKN_TPLAY_ 2
#define NK_CTTS_OPT

#define _MP4Box_




//#define _USE_NKN_VFS_
/*
#ifndef _USE_NKN_VFS_
#define nkn_vfs_fseek(fid,box_size,SEEK_SET) fseek(fid,box_size,SEEK_SET)
#define nkn_vfs_ftell(fid) ftell(fid)
#else
#include "nkn_vfs.h"
#endif


inline uint32_t _read32(uint8_t *buf,int32_t pos); //(buf[pos]<<24)|(buf[pos+1]<<16)|(buf[pos+2]<<8)|(buf[pos+3])
inline uint16_t _read16(uint8_t *buf,int32_t pos); //(buf[pos]<<8)|(buf[pos+1])
*/


#define MOOV_HEX_ID 0x6d6f6f76
#define VIDE_HEX 0x65646976//0x76696465
#define SOUN_HEX 0x6e756f73//0x736f756e
#define HINT_HEX 0x746e6968//0x68696e74
/*Structures Declared*/

typedef struct prev_sampl_info{
    int32_t sample_num;
    int32_t sample_size;
    int32_t chunk_base;
    int32_t chunk_offset;
    int32_t total_chunks;
    int32_t is_stateless;
    int32_t chunk_count;
    int32_t prev_sample_total;
}Sample_details_t;

#ifdef NK_CTTS_OPT
typedef struct cttsst{
    int32_t prev_ctts_entry;
    int32_t entry_count;
    int32_t prev_cum_sample_cnt;
}ctts_state_t;
#endif


typedef struct props{
    int32_t size_offset;
    int32_t prev_base;
    int32_t total_sample_size;
    int32_t time_data_sent;
    int32_t look_ahead_time;
    Sample_details_t prev_sample_details;
    int32_t total_chunks;
    int32_t ctts_offset;
#ifdef NK_CTTS_OPT
    ctts_state_t prev_ctts;
    char dummy[24];
#else
    char dummy[36];
#endif
}Sampl_prop_t;


typedef struct tbl_prop{
    int32_t offset;
    int32_t size;
}Table_prop_t;

typedef struct stbll{
    uint8_t *stsc;
    uint8_t *stco;
    uint8_t *stsz;
    uint8_t *stts;
    uint8_t *ctts;
    uint8_t *stsd;
    uint8_t *esds;
    uint8_t *stss;
    int32_t trak_type;
    int32_t codec_type;
    int32_t timescale;
    int32_t width;
    int32_t height;
    Table_prop_t stsc_tab;
    Table_prop_t stco_tab;
    Table_prop_t stsz_tab;
    Table_prop_t stts_tab;
    Table_prop_t ctts_tab;
    Table_prop_t stsd_tab;
    Table_prop_t esds_tab;
    Table_prop_t stss_tab;
    char dummy[116];
}Stbl_t;

typedef struct file_stbl{
    Stbl_t VideoStbl;
    Stbl_t AudioStbl;
    Stbl_t HintVidStbl;
    Stbl_t HintAudStbl;
    int32_t duration;
}Media_stbl_t;

typedef struct ttype{
    uint32_t trak_id;
    uint32_t type;
    uint32_t num_ref_traks;
    uint32_t* ref_trak_id; 
}Trak_info_t;

typedef struct seekR{
    uint8_t num_traks;
    Trak_info_t* seek_trak_info;
}Seek_t;


#define MAX_NUM_SAMPLES 15

typedef struct seek_state{
    int32_t num_bytes[MAX_NUM_SAMPLES];
    int32_t offset[MAX_NUM_SAMPLES];
    int32_t num_samples_sent;
    int32_t num_samples_to_send;
}Seek_state_t;

typedef struct seekinfo_t{
    Seek_state_t Video;
    Seek_state_t Audio;
    Seek_state_t HintV;
    Seek_state_t HintA;
}Seekinfo_t;


typedef struct SDP_info{
    int32_t num_bytes;
    uint8_t* sdp;    
}SDP_info_t;

typedef struct payload{
    int32_t sample_num;
    int32_t payload_offset;
    int32_t payload_size;
}Payload_info_t;


typedef struct rtp_payload_info{
    uint16_t type;
    int32_t time_stamp;
    RTP_pkt_t header;
    int16_t num_payloads;
    int32_t total_payload_size;
    Payload_info_t* payload;
}RTP_whole_pkt_t;


typedef struct rtp_sample{
    int32_t stream_type;
    int32_t num_rtp_pkts;
    RTP_whole_pkt_t *rtp;
}Hint_sample_t;

typedef struct tag_send_ctx {
    Sampl_prop_t *vid_s, *aud_s,* hvid_s, *haud_s;
}send_ctx_t;


typedef struct rtp_sample_media{
    int32_t stream_type;
    int32_t num_rtp_pkts;
    RTP_whole_pkt_t *rtp;
}Media_sample_t;



typedef struct all_info{
    Seekinfo_t *seek_state_info;
    Media_stbl_t *stbl_info;
    send_ctx_t *sample_prop_info;
    Media_sample_t *videoh;
    Media_sample_t *audioh;
    Media_sample_t *video;
    Media_sample_t *audio;
    int32_t look_ahead_time;
    int32_t timescale_audio;
    //int32_t total_sample_size_audio;
    int32_t timescale_video;
    //int32_t total_sample_size_video;
}all_info_t;


typedef struct output_details{
    int32_t num_bytes;
    int32_t offset;
    int32_t num_samples_sent;
    RTP_whole_pkt_t *rtp;
    int32_t ctts_offset;
}output_details_t;


typedef struct tream_info{
    int32_t num_traks;
    int32_t track_type[2];

}Stream_info_t;

typedef struct seek_params{
    int32_t seek_time;
    int32_t time_scale;
    int32_t warped_time;
}Seek_params_t;

#ifdef _SUMALATHA_OPT_MAIN_

/*API Defs */
void get_sdp_info(SDP_info_t*,size_t (*nkn_fread)( void *, size_t, size_t, FILE*), FILE*);
int32_t send_next_pkt(Seekinfo_t*,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* ,Media_stbl_t* , send_ctx_t **,Hint_sample_t * , Hint_sample_t * , int32_t ,int32_t , Seek_params_t*);
void seek2time_hinted(all_info_t *st_all_info,int32_t seek_time,int32_t track_type,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* fid);
int32_t find_trak_type(Media_stbl_t* mstbl,int32_t pid);


/*Function declarations*/

int32_t read_tkhd(uint8_t* , Seek_t *);
int32_t get_trak_asset(uint8_t*,Stbl_t*);
int32_t get_Media_stbl_info(uint8_t*,Media_stbl_t*,int32_t);
int32_t return_track_id(uint8_t *);

int32_t init_seek_info(Seekinfo_t*);

int32_t read_rtp_sample(uint8_t*, Hint_sample_t*,int32_t, int32_t);

void get_stream_info(Media_stbl_t* , Stream_info_t *);
int32_t read_rtp_sample_audio(uint8_t*, Hint_sample_t*,int32_t , int32_t,Stbl_t* ,Stbl_t*,Sample_details_t *,Sample_details_t*,size_t (*nkn_fread)( void *, size_t, size_t, FILE*), FILE *);


#if 1
void free_all_stbls(Media_stbl_t*);
void free_stbl(Stbl_t *);
void free_ctx(send_ctx_t *);
void free_all(Hint_sample_t* hnt);
#endif
//int32_t read_stbls(Stbl_t*, size_t (*nkn_fread)( void *, size_t, size_t, FILE*), FILE *);
int32_t init_stbls(Stbl_t*,int32_t);
int32_t free_stbls(Stbl_t*);

int32_t read_stsz_opt(int32_t ,int32_t);
int32_t read_stss_opt(int32_t ,int32_t);
int32_t read_stts_opt(int32_t ,int32_t);
int32_t read_stco_opt(int32_t ,int32_t);
int32_t read_ctts_opt(int32_t entry_num,int32_t pos);
int32_t find_chunk_asset_opt(int32_t*,int32_t*,int32_t, int32_t,Stbl_t *,Sampl_prop_t*,size_t (*nkn_fread)( void *, size_t, size_t, FILE*), FILE *);
int32_t get_smpl_pos_opt(int32_t ,Stbl_t *stbl,Sample_details_t*,size_t (*nkn_fread)( void *, size_t, size_t, FILE*), FILE *);
int32_t get_time_offset_opt(Stbl_t *, int32_t,size_t (*nkn_fread)( void *, size_t, size_t, FILE*), FILE *);
int32_t get_ctts_offset_opt(Stbl_t *, int32_t,size_t (*nkn_fread)( void *, size_t, size_t, FILE*), FILE *);
int32_t get_total_sample_size_opt(Stbl_t *,size_t (*nkn_fread)( void *, size_t, size_t, FILE*), FILE *);
int32_t find_chunk_offset_opt(Stbl_t *,int32_t ,size_t (*nkn_fread)( void *,size_t, size_t, FILE*),FILE *);
int32_t seek_state_change_opt(Stbl_t *,int32_t ,int32_t ,Sampl_prop_t* ,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE *);
int32_t media_seek_state_change_opt(Stbl_t *,Sampl_prop_t*,int32_t,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE *);
int32_t find_cumulative_sample_size_opt(Stbl_t *,int32_t ,int32_t ,size_t (*nkn_fread)( void *,size_t, size_t, FILE*),FILE *);
int32_t find_nearest_sync_sample_opt(Stbl_t *,int32_t ,size_t (*nkn_fread)( void *,size_t, size_t, FILE*),FILE *);
int32_t read_stbls_opt(Stbl_t* ,size_t (*nkn_fread)( void *, size_t, size_t, FILE*), FILE *);
int32_t read_stts_opt_main(Stbl_t *,int32_t ,int32_t ,size_t (*nkn_fread)( void *,size_t, size_t, FILE*),FILE *);
int32_t uint32_byte_swap(int32_t );
int32_t get_chunk_offsets_opt(int32_t , Stbl_t *, int32_t , int32_t* , int32_t* ,size_t (*nkn_fread)( void *,size_t, size_t, FILE*),FILE *);
int32_t find_first_sample_of_chunk_opt(Stbl_t *,int32_t ,size_t (*nkn_fread)( void *,size_t, size_t, FILE*),FILE *);
int32_t read_stsc_opt(int32_t ,int32_t );


//int32_t get_total_sample_size(uint8_t* );
#if 0
    int32_t seek_state_change(Stbl_t *, int32_t,int32_t ,Sampl_prop_t*);
    int32_t media_seek_state_change(Stbl_t *,Sampl_prop_t*,int32_t);
    int32_t find_first_sample_of_chunk(uint8_t *,int32_t);
    int32_t find_cumulative_sample_size(uint8_t*,int32_t ,int32_t );
#endif

int32_t read_rtp_sample_interface(uint8_t*, Media_sample_t*,int32_t, int32_t);

int32_t read_rtp_sample_audio_interface(uint8_t* , Media_sample_t* ,int32_t , int32_t ,Stbl_t* ,Stbl_t* ,Sample_details_t *,Sample_details_t* ,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE *);

int32_t  send_next_pkt_init(Seekinfo_t* ,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* ,Media_stbl_t*  , send_ctx_t *);
int32_t send_next_pkt_seek2time(Seekinfo_t* ,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* ,Media_stbl_t* , send_ctx_t *, int32_t ,Seek_params_t* );
int32_t send_next_pkt_seek2time_hinted(Seekinfo_t* ,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* ,Media_stbl_t* , send_ctx_t *, int32_t ,Seek_params_t* );

int32_t send_next_pkt_get_sample(Seekinfo_t* ,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* ,Media_stbl_t*  , send_ctx_t *,Media_sample_t *, Media_sample_t * , Media_sample_t * , Media_sample_t * , int32_t );
int32_t send_next_pkt_interface(Seekinfo_t* ,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* ,Media_stbl_t*  , send_ctx_t *,Media_sample_t *, Media_sample_t * , int32_t );

int32_t Init(all_info_t* , size_t (*nkn_fread)( void *, size_t, size_t, FILE*), FILE *);
all_info_t* init_all_info(void);
int32_t free_all_info(all_info_t* );
int32_t cleanup_output_details(output_details_t* );
void seek2time(all_info_t *,int32_t ,int32_t ,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* );
int32_t get_next_sample(all_info_t *,int32_t ,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* ,int32_t *,output_details_t * );
int32_t   send_next_pkt_hinted(all_info_t *st_all_info,int32_t trak_type,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* fid,Hint_sample_t *audiopkts, Hint_sample_t *videopkts);

int32_t  getesds(all_info_t *,int32_t ,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* ,int32_t *);
int32_t  getavcc(all_info_t *,int32_t ,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* ,int32_t *);

#else//_SUMALATHA_OPT_MAIN_

int32_t find_first_sample_of_chunk(uint8_t *,int32_t);
int32_t find_nearest_sync_sample(uint8_t *,int32_t);
//int32_t find_chunk_asset_seek(Seek_state_t*,Stbl_t *, int32_t ,int32_t);
//int32_t find_sample_size(uint8_t*,int32_t);
int32_t find_cumulative_sample_size(uint8_t*,int32_t ,int32_t );
int32_t find_chunk_offset(uint8_t*,int32_t);
int32_t seek_state_change(Stbl_t *, int32_t,int32_t ,Sampl_prop_t*);




/*API Defs */
void get_sdp_info(SDP_info_t*,size_t (*nkn_fread)( void *, size_t, size_t, FILE*), FILE*);
int32_t send_next_pkt(Seekinfo_t*,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* ,Media_stbl_t* , send_ctx_t **,Hint_sample_t * , Hint_sample_t * , int32_t ,int32_t , Seek_params_t*);
int32_t media_seek_state_change(Stbl_t *,Sampl_prop_t*,int32_t);
void seek2time_hinted(all_info_t *st_all_info,int32_t seek_time,int32_t track_type,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* fid);
int32_t find_trak_type(Media_stbl_t* mstbl,int32_t pid);
int32_t uint32_byte_swap(int32_t input);


/*Function declarations*/
//int32_t parse_mp4_fseek(uint8_t*,size_t,uint32_t);
int32_t read_tkhd(uint8_t* , Seek_t *);
int32_t read_stts(uint8_t*,int32_t,int32_t);
int32_t get_trak_asset(uint8_t*,Stbl_t*);
int32_t get_Media_stbl_info(uint8_t*,Media_stbl_t*,int32_t);
int32_t return_track_id(uint8_t *);
int32_t find_chunk_asset(int32_t*,int32_t*,int32_t, int32_t,Stbl_t *,Sampl_prop_t*);


int32_t get_chunk_offsets(int32_t, uint8_t*, int32_t,int32_t*,int32_t*);

int32_t init_seek_info(Seekinfo_t*);

int32_t read_rtp_sample(uint8_t*, Hint_sample_t*,int32_t, int32_t);

int32_t read_rtp_sample_audio(uint8_t*, Hint_sample_t*,int32_t , int32_t,Stbl_t* ,Stbl_t*,Sample_details_t *,Sample_details_t*);

int32_t get_time_offset(uint8_t*, int32_t);


#ifdef NK_CTTS_OPT
    int32_t get_ctts_offset(uint8_t*, int32_t, ctts_state_t*);    
#else
    int32_t get_ctts_offset(uint8_t*, int32_t);    
#endif

void get_stream_info(Media_stbl_t* , Stream_info_t *);

int32_t get_smpl_pos(int32_t num_samples_to_send,Stbl_t *stbl,Sample_details_t*);


#if 1
void free_all_stbls(Media_stbl_t*);
void free_stbl(Stbl_t *);
void free_ctx(send_ctx_t *);
void free_all(Hint_sample_t* hnt);
#endif
int32_t read_stbls(Stbl_t*, size_t (*nkn_fread)( void *, size_t, size_t, FILE*), FILE *);
int32_t init_stbls(Stbl_t*,int32_t);
int32_t free_stbls(Stbl_t*);

int32_t get_total_sample_size(uint8_t* );



#ifdef _MP4Box_
    int32_t read_rtp_sample_interface(uint8_t*, Media_sample_t*,int32_t , int32_t,Stbl_t*,Stbl_t* , int32_t );
    int32_t get_offset_in_file(int32_t,Stbl_t*);
#else
    int32_t read_rtp_sample_interface(uint8_t*, Media_sample_t*,int32_t, int32_t);
#endif

int32_t read_rtp_sample_audio_interface(uint8_t* , Media_sample_t* ,int32_t , int32_t ,Stbl_t* ,Stbl_t* ,Sample_details_t *,Sample_details_t* );

int32_t  send_next_pkt_init(Seekinfo_t* ,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* ,Media_stbl_t*  , send_ctx_t *);

int32_t send_next_pkt_seek2time(Seekinfo_t* ,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* ,Media_stbl_t* , send_ctx_t *, int32_t ,Seek_params_t* );
int32_t send_next_pkt_seek2time_hinted(Seekinfo_t* ,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* ,Media_stbl_t* , send_ctx_t *, int32_t ,Seek_params_t* );

int32_t send_next_pkt_get_sample(Seekinfo_t* ,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* ,Media_stbl_t*  , send_ctx_t *,Media_sample_t *, Media_sample_t * , Media_sample_t * , Media_sample_t * , int32_t );
int32_t send_next_pkt_interface(Seekinfo_t* ,size_t (*nkn_fread)( void *, size_t, size_t, FILE*), size_t (*nkn_fread1)( void *, size_t, size_t, FILE*, void **, int *len), FILE* ,Media_stbl_t*  , send_ctx_t *,Media_sample_t *, Media_sample_t * , int32_t );

int32_t Init(all_info_t* , size_t (*nkn_fread)( void *, size_t, size_t, FILE*), FILE *);
all_info_t* init_all_info(void);
int32_t free_all_info_unhinted(all_info_t* );

int32_t free_all_info(all_info_t* );
int32_t cleanup_output_details(output_details_t* );
void seek2time(all_info_t *,int32_t ,int32_t ,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* );
int32_t get_next_sample(all_info_t *,int32_t ,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* ,int32_t *,output_details_t * );
int32_t   send_next_pkt_hinted(all_info_t *st_all_info,int32_t trak_type,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),size_t (*nkn_fread1)( void *, size_t, size_t, FILE*,void **, int *), FILE* fid,Hint_sample_t *audiopkts, Hint_sample_t *videopkts);

int32_t  getesds(all_info_t *,int32_t ,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* ,int32_t *);
int32_t  getavcc(all_info_t *,int32_t ,size_t (*nkn_fread)( void *, size_t, size_t, FILE*),FILE* ,int32_t *);

#endif//_SUMALATHA_OPT_MAIN_

#ifdef __cplusplus
} //extern cplusplus
#endif
#pragma pack(pop)


#endif//_MP4_FEATURE_
