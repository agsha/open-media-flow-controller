
#ifndef RTP_FORMAT
#define RTP_FORMAT


//#include "rtp_packetizer_common.h"
//#include "nkn_vpe_bufio.h"
#include "nkn_vpe_bitio.h"
#define VPE_SUCCESS 1
#define VPE_ERROR 0
#define MAX_SINGLE_NALU 23
#define MIN_SINGLE_NALU 1
#define STAPA 24
#define FUA 28
#define NALU_DELIMIT 0x00000001
#define MAX_VIDEO_STREAMS 5
#define MAX_AUDIO_STREAMS 10
#define VID_H264 1
#define AUD_AAC  1
#define VID_UNKNOWN 0   


#define AUDIO_TRAK 2
#define VIDEO_TRAK 1
#define CACHE_FIXED_HDR_SIZE 8
#define CACHE_SDP_SIZE 4
#define CACHE_META_SIZE 4
#define CACHE_MDATA_RSVD 4

//#define _UNIT_TEST

#ifdef _UNIT_TEST
#define VPE_OUTPUT_PATH "./"
#define VPE_SCRATCH_PATH "./"
#else
#define VPE_OUTPUT_PATH "/nkn/vpe/"
#define VPE_SCRATCH_PATH "/vpe/scratch/"
#endif


typedef struct pktformat{
    uint8_t *data;
    uint32_t size;
    FILE *fout;
    uint16_t last_seq_num;
    uint8_t cid; //channel id
    uint32_t prev_time;
    uint32_t frame_rate;
    uint32_t num_frames;
}pkt_format_data_t;

typedef struct index_info{
    uint32_t sizelen;
    uint32_t indexlen;
    uint32_t deltalen;
}indinfo_t;

typedef struct adtsaac{
    uint8_t is_mpeg2;
    uint8_t object_type;
    uint8_t sr_index;
    uint16_t sample_len;
    uint8_t num_channels;

}adts_t;

typedef struct streamstate{
    int32_t codec_type;
    FILE * fout;
    char * fname;
    uint16_t seq_num;
    uint16_t prev_seq_num;
    uint8_t cid; //channel id
    pkt_format_data_t pkt; 
    uint32_t timescale;
    char* sdp;
    adts_t adts;
    indinfo_t ind;
    int32_t trak_id;
}stream_state_t;


typedef struct rtp_formatizer_tt{
    stream_state_t audio[MAX_AUDIO_STREAMS];
    stream_state_t video[MAX_VIDEO_STREAMS];
    int32_t num_audio_streams;
    int32_t num_video_streams;
    int32_t rtp_data_size;
    char afname[MAX_AUDIO_STREAMS][20];
    char vfname[MAX_VIDEO_STREAMS][20];
    int fin;
    float duration;
}rtp_formatizer_t;



int32_t h264_rtp_dump(pkt_format_data_t*);
int32_t rtp_format_convert(rtp_formatizer_t*);
int32_t allocate_rtp_streams(rtp_formatizer_t *);
int32_t aac_rtp_dump(stream_state_t*);
int32_t parse_sdp_box(char*, int32_t,rtp_formatizer_t *);
int32_t write_h264_ps(char*,FILE*);
int32_t aac_add_adts(stream_state_t*);
uint8_t get_aac_sr(int32_t);


int32_t get_sdp_size(uint8_t*);
int32_t map_track_channel_ids(uint8_t* ,int32_t,rtp_formatizer_t *);
int32_t read_cache_translate(char*,char*);
#endif
