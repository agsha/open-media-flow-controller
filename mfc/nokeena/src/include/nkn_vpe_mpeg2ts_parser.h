//#define MS_DEV


//#ifndef MPEG2_TS
//#define MPEG2_TS

#ifdef MS_DEV
#include "inttypeswin.h"
#include "main.h"
#else
#include "inttypes.h"
#endif

#include "nkn_vpe_metadata.h"

#define TPLY_HDR_SIZE 0
#define TPLY_EXTRA_INFO 2*sizeof(int64_t)

/*Preprocessor Directives*/
#define VPE_OK 1
#define VPE_ERR 0
#define MPEG2_TS_PKT_SIZE 188

#define IDR_SLICE          1
#define NON_IDR_SLICE      2 
#define SLICE_TYPE_UNKNOWN 0

#define MPEG2TS_TRICK_PLAY 0
#define MAX_PIDS_VIDEO 16
#define MAX_PIDS_AUDIO 64
#define MAX_PIDS 8192
#define MAX_PKTS_BW_IDR 50000

/*Error Defines*/
#define VPE_SYNC_BYTE_ERR -1

#define VPE_PES_ERR -10
/*PIDs*/
#define UNKWN_PID 0
#define PMT_PID   1
#define VIDEO_PID 2
#define AUDIO_PID 3
#define PVT_PID   4//Also includes Ntk and CAS

/*Video Codecs*/
#define MPEG2_PES    0
#define MPEG4_VISUAL 1
#define MPEG4_AVC    2


/*Trickplay Defines*/

#define TPLAY_RECORD_JUNK  5
#define TPLAY_RECORD_RANDOM_ACCESS  6

/*Data structures*/
typedef struct VStream{
    int32_t pid;
    int32_t codecId;
    uint8_t  *data;
}VStream_t;

typedef struct AStream{
    int32_t pid;
    int32_t codecId;
    uint8_t *data;
}AStream_t;

/*Structure for VideoStream Info */
typedef struct VsInfo{
    uint16_t num_pids;
    VStream_t **vstream;
}VStrmInfo_t;

/*Structure for Audio Info */
typedef struct AsInfo{
    uint16_t num_pids;
    AStream_t **astream;
}AStrmInfo_t;


typedef struct PgmAssSec{
    uint8_t version_num;
    uint8_t current_next_indicator;
    uint8_t section_number;
    uint8_t last_section_number;
    uint8_t prev_cc;//continuity counter

}PAS_t;

typedef struct PMT{
    uint32_t pid;
    int32_t stream_type; //0:PAT,1:CA,2:PMT,3:Video,4:Audio 
}PMT_t;

typedef struct Misc{
    int64_t first_PTS;//First video pkt with Timestamp
    int64_t last_PTS;//With Time stamp
	uint8_t frame_type_pending;
	uint8_t present_video_stream_num;
}Misc_data_t;

/*Structure for Present TS*/
typedef struct TS{
    int TsId; /*Id obtained from Pgm association Table*/
    PMT_t *pmtinfo; //PID,stream_type	
    uint32_t num_pids;
    uint32_t num_pkts;
    uint32_t start_of_pid;
    PAS_t *pas;
    VStrmInfo_t *vsinfo;
    AStrmInfo_t *asinfo;
    //PStrmInfo_t *psinfo;
}Ts_t;


typedef struct trickplay_str{
    tplay_index_record *prev_index;
    tplay_index_record *curr_index;
}Tplay_index_t;

/*Function Declarations*/
int32_t gen_trickplay_hdr(char *,char *);
int32_t handle_pkt_tplay(uint8_t*,Ts_t*,tplay_index_record*,Misc_data_t*);
int32_t handle_pid(Ts_t*,uint32_t);
int32_t update_pid(Ts_t*,uint16_t,uint8_t);
int32_t Tplay_malloc(Tplay_index_t* );
int Ts_alloc(Ts_t* ts1);
void Ts_reset(Ts_t *ts);
void Ts_free(Ts_t *ts);
int32_t check_mpeg2ts_index_file(FILE * fid);
//#endif
