#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <fcntl.h>
#include <assert.h>
#include <pthread.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/uio.h>

#include "rtsp_func.h"
//#include "network.h"
#include "nkn_vfs.h"
#include "nkn_vpe_mp4_feature.h"
#include "nkn_vpe_types.h"
#include "nkn_vpe_rtp_packetizer.h"
#include "rtp_packetizer_common.h"
#include "nkn_vpe_mp4_unhinted.h"
#include "rtp_mpeg4_packetizer.h"
#include "nkn_mem_counter_def.h"
#include "nkn_defs.h"

#include "iov_buf_state.h"

//static pthread_mutex_t mp4_mutex = PTHREAD_MUTEX_INITIALIZER;
uint64_t n_calls_send = 0;

#define MP4_MAGIC_DEAD 0xdeaddead
#define MP4_MAGIC_ALIVE 0x11111111

#pragma pack(push, 1)
//static pthread_mutex_t parser_lock;
enum playState { PLAY, /* NORMAL PLAYBACK */
		 PAUSE, /* PAUSES DELIVERY, retains state information on resume */
		 TRICKMODE, /* FF/RW */
		 SEEK, /*SEEK TO */
		 STOP /* PAUSES DELIVERY, resets state information on resume */
};

/* MP4 Specific state */
typedef struct mp4RTP_t
{
    int32_t taskInterval;
    TaskToken ftaskToken;
    uint32_t magic;
    float fDuration;
    int ftcpSocketNum;
    FILE * fd;
    char * fFileName;

    pthread_mutex_t mp4_mutex;   
    /* Server Side Player related information */
    float scale;
    uint8_t trickMode; //moving to TRICKMODE 0 or 1
    uint8_t sdpCallCount;
    char trackID[2][32];
    int32_t sampleNumber;
    int32_t ts;
    enum playState ps;
    int32_t clkFreq;

    /* delivery related info */
    uint16_t sequenceNumber;
    uint32_t ssrc;
    uint8_t trackType;
    uint8_t channel_id;
#ifdef _USE_INTERFACE_
    all_info_t *st_all_info; 
    int32_t *look_ahead_time;
    output_details_t *output;
    Sample_rtp_info_t *sample;
#else//else of _USE_INTERFACE_
    Seekinfo_t *si;
    Media_stbl_t *stbl;
    send_ctx_t *sctx;
#endif//end of _USE_INTERFACE_
    Hint_sample_t *videoPkts;
    Hint_sample_t *audioPkts;
    Seek_params_t *seek;
    int32_t rtpTime;
    int32_t firstRTPTimestamp;
    int32_t lastTimeStamp;
    int32_t firstTimeStamp;
    int32_t numSamples;
    double nextDTS;
    parser_t *unhintedParser;

    /* payload info */
    void *p_video;
    void *p_audio;
    void *other;

    /* time sync information*/
    struct timeval currPktTime;
    struct timeval lastPktTime;
    struct timeval opStart;
    struct timeval opEnd;
    uint64_t timeElapsed;
    double lastDTS;

    /*scheduled callback function */
    void (*cbFn) (void *);
    
    /*Protocol info */
    float maxSubsessionDuration;
    float minSubsessionDuration;
    char fTrackId[32];
    uint32_t fServerAddressForSDP;
    uint32_t clientSessionId;
    int fIsStreamPlaying;
    uint32_t seed;
 
    int udpsocket;
    //int udpsocket_rtcp; // RTCP moved to common code path. -- Zubair/Max
    unsigned short local_port;
    unsigned int local_ip;
    unsigned short remote_port;
    unsigned int remote_ip;

    iov_buf_state_t* prev_iov_state; // used in RTP/TCP, to maintain half sent RTP pkt case
    int32_t temp_rtp_time;
    double play_end_time;

} mp4RTP;
#pragma pack(pop)

/************************PROTOTYPES********************/
extern "C" void * imp_deleteStream(void * p, unsigned clientSessionId, void* streamToken);
extern "C" int imp_setStreamScale(void * p, unsigned clientSessionId, void* streamToken, float scale);
extern "C" float imp_testScaleFactor(void * p, float scale);
extern "C" char const* imp_trackId(void * p);

static void sendNext_hinted(void* firstArg);
static void sendNext_unhinted(void* firstArg);

uint32_t getNTPTime(mp4RTP *st, int32_t timestamp);
static int32_t isHintTrackPresent(mp4RTP *psmss);
int32_t setupRTPTime(mp4RTP *st);
mp4RTP* createSubsessionState(char *dataFileName, int32_t sockfd,
			      uint32_t if_addr, char *vfs_open_mode, 
			      size_t *tot_file_size);
int32_t initSubsessionState(mp4RTP* psmss, int32_t taskInterval, int32_t trackType);
static inline void buildRTPPacket(mp4RTP *st, RTP_pkt_t *pkt, uint8_t *hdr);
static inline int32_t timeval_subtract (struct timeval *x, struct timeval *y, struct timeval *result);
static inline void cleanupSessionState(mp4RTP *st);
static inline void freeAndNULL(void *ptr);
void setRefClk(mp4RTP *psmss);

static inline void addRtpTcphdr(uint8_t* buf, uint8_t channelId, uint16_t len);

int32_t cleanup_mp4_hinted_parser(mp4RTP *);
int32_t cleanup_mp4_unhinted_parser(mp4RTP *);
int32_t cleanup_parser_state(mp4RTP *);

/* constants */
#define TASK_INTERVAL 30000
#define RTP_PACKET_SIZE 12
#define MP4_DURATION 180
#define N_SUPPORTED_TRACKS 2 
#define AUDIO_SCALE_FACTOR 2.34

/* DEBUG FLAGS */

/* PERF_DEBUG -  enables the self time calculation and other performance related information for debugging
 * NOTE: this enabling the flag will print a lot of information to stdout / syslog*/
//#define PERF_DEBUG

/* DUMP_PKTS - dumps all the RTP packets to the current directory in the file system, i.e. /opt/nkn/sbin
 * NOTE: you need to mount '/' with write permissions
 */
//#define DUMP_PKTS

/* prints some debug messages detaling the number of bytes sent, sample number, track id etc
 */
//#define PRINT_DBG

/* prints the status messages, and progression of connection states like imp_start, imp_getStartParams etc
 */
//#define PRINT_STATUS

////////// mp4RTP //////////
//#define DM2_ENABLED

/**************************************************************************************************
 *                               UTILITY FUNCTIONS
 *************************************************************************************************/

/* builds the RTP packet from the RTP pkt info structure in the MP4 file
 * this function is needed since the RTP packet headers are not implicitly present in the MP4 file.
 * we need to build them on the fly !
 *@param st[in] - session state
 *@param pkt[in] - RTP packet info gleaned from the HINT sample in the MP4 file
 *@param hrd[out] - 12 byte RTP header that needs to be sent on the wire
 *@return - returns nothing
 */
static inline void
buildRTPPacket(mp4RTP *st, RTP_pkt_t *pkt, uint8_t *hdr)
{
    uint8_t *p_buf;
    uint16_t tmp;
    uint32_t tmp1;

    p_buf = hdr;
    tmp = 0;
    tmp1 = 0;

    //first 2 bytes of the RTP header overlap with RTP flags    
    tmp = pkt->rtp_flags1;
    tmp |= 0x8000;
    tmp = htons(tmp);//convert to network compatible endianess
    memcpy(p_buf, &tmp, 2);
    p_buf+=2;

    //next 2 bytes are sequence number
    tmp = st->sequenceNumber;
    st->sequenceNumber++;
    tmp = htons(tmp);//convert to network compatible endianess
    memcpy(p_buf, &tmp, 2);
    p_buf+=2;

    //copy the time (inclusive of warping etc)
    //    tmp1 = getNTPTime(st, (pkt->relative_time - (st->seek->seek_time * st->clkFreq * 1000)));
    if(st->ps == SEEK || st->ps == PAUSE) { 
	st->firstTimeStamp = pkt->relative_time;
	st->ps = PLAY;
    }

    tmp1 = getNTPTime(st, (pkt->relative_time - (st->seek->seek_time * st->clkFreq)));
    //tmp1 = getNTPTime(st, pkt->relative_time - st->firstTimeStamp);
    //tmp1 = getNTPTime(st, pkt->relative_time);
    //#ifdef PRINT_STATUS
    //    printf("RTP Base Time Stamp %d, Packet Timestamp %d\n", st->rtpTime, tmp1);
    //#endif
    if(st->trackType == VIDEO_TRAK) {
	st->lastDTS = ((1e6*(float)(pkt->relative_time))/(float)(st->clkFreq));//microseconds
#ifdef _USE_INTERFACE_
	st->nextDTS = ((1e6*(float)(st->st_all_info->sample_prop_info->hvid_s->look_ahead_time))/(float)(st->clkFreq));
#else
	st->nextDTS = ((1e6*(float)(st->sctx->hvid_s->look_ahead_time))/(float)(st->clkFreq));
#endif
    } else {
	st->lastDTS = ((1e6*(double)(pkt->relative_time))/(double)(st->clkFreq));//microseconds
#ifdef _USE_INTERFACE_
	st->nextDTS = ((1e6*(double)(st->st_all_info->sample_prop_info->haud_s->look_ahead_time))/(double)(st->clkFreq));
#else
	st->nextDTS = ((1e6*(double)(st->sctx->haud_s->look_ahead_time))/(double)(st->clkFreq));
#endif
    }
#ifdef PRINT_DBG
    //    printf("TRACK Type %d, RTP timestamp %d\n", st->trackType, tmp1);
#endif
    st->lastTimeStamp = tmp1;    
    //    printf("Time = %d \n",tmp1);
    tmp1 = htonl(tmp1);//convert to network compatible endianess
    memcpy(p_buf, &tmp1, 4);
    p_buf+=4;

    //copy the ssrc
    tmp1 = st->ssrc;
    tmp1 = htonl(tmp1);//convert to network compatible endianess
    memcpy(p_buf, &tmp1, 4);
    
}


void
setRefClk(mp4RTP *psmss)
{
    int32_t trackType;
    
    trackType = psmss->trackType;
    
    if(isHintTrackPresent(psmss)) { 

#ifdef _USE_INTERFACE_
	if(trackType == VIDEO_TRAK) {
	    psmss->clkFreq = psmss->st_all_info->stbl_info->HintVidStbl.timescale; //psmss->st_all_info->stbl_info->HintVidStbl.timescale;//KHz
	    psmss->numSamples = psmss->st_all_info->sample_prop_info->hvid_s->total_sample_size;//;psmss->st_all_info->sample_prop_info->hvid_s->total_sample_size;
	} else if (trackType == AUDIO_TRAK) {
	    psmss->clkFreq = psmss->st_all_info->stbl_info->HintAudStbl.timescale; //44100;//psmss->st_all_info->stbl_info->HintAudStbl.timescale;//KHz
	    psmss->numSamples =psmss->st_all_info->sample_prop_info->haud_s->total_sample_size;// psmss->st_all_info->sample_prop_info->haud_s->total_sample_size;
	} else {
	    return; //unsupported track
	}
#else//else of _USE_INTERFACE_
	if(trackType == VIDEO_TRAK) {
	    psmss->clkFreq = psmss->stbl->HintVidStbl.timescale;//KHz
	    psmss->numSamples = psmss->sctx->hvid_s->total_sample_size;
	} else if (trackType == AUDIO_TRAK) {
	    psmss->clkFreq = psmss->stbl->HintAudStbl.timescale;//KHz
	    psmss->numSamples = psmss->sctx->haud_s->total_sample_size;
	} else {
	    return; //unsupported track
	}
#endif//end of _USE_INTERFACE_

    } else {

#ifdef _USE_INTERFACE_
	if(trackType == VIDEO_TRAK) {
	    psmss->clkFreq = psmss->st_all_info->stbl_info->VideoStbl.timescale; //psmss->st_all_info->stbl_info->HintVidStbl.timescale;//KHz
	    psmss->numSamples = psmss->st_all_info->sample_prop_info->vid_s->total_sample_size;//;psmss->st_all_info->sample_prop_info->hvid_s->total_sample_size;
	} else if (trackType == AUDIO_TRAK) {
	    psmss->clkFreq = psmss->st_all_info->stbl_info->AudioStbl.timescale; //44100;//psmss->st_all_info->stbl_info->HintAudStbl.timescale;//KHz
	    psmss->numSamples =psmss->st_all_info->sample_prop_info->aud_s->total_sample_size;// psmss->st_all_info->sample_prop_info->haud_s->total_sample_size;
	} else {
	    return; //unsupported track
	}
#else//else of _USE_INTERFACE_
	if(trackType == VIDEO_TRAK) {
	    psmss->clkFreq = psmss->stbl->HintVidStbl.timescale;//KHz
	    psmss->numSamples = psmss->sctx->hvid_s->total_sample_size;
	} else if (trackType == AUDIO_TRAK) {
	    psmss->clkFreq = psmss->stbl->HintAudStbl.timescale;//KHz
	    psmss->numSamples = psmss->sctx->haud_s->total_sample_size;
	} else {
	    return; //unsupported track
	}
#endif//end of _USE_INTERFACE_
    }
}

/* check if hint track is present
 */
static int32_t
isHintTrackPresent(mp4RTP *psmss)
{
    if((psmss->st_all_info->stbl_info->HintVidStbl.stsz_tab.size != -1) || (psmss->st_all_info->stbl_info->HintAudStbl.stsz_tab.size != -1)) {
	return 1;
    } else {
	return 0;
    }
}
/*convert to NTP timestamp
 */
uint32_t
getNTPTime(mp4RTP *st, int32_t timestamp)
{
    //static struct timeval tv;
    //int64_t rtptime;
    //double dts;
    
    //tv = *st->rtpTime;
    //dts = (timestamp * 1000)/st->clkFreq;//microseconds
    //rtptime = ((tv.tv_sec) * st->clkFreq * 1000)%(INT_MAX); //ticks
    return ((st->rtpTime + timestamp));
    
}

static inline double 
convertRefClockToNormalTime(uint64_t ticks, int32_t ticksPerSecond) 
{
    return (double)(ticks/ticksPerSecond);
}

/* generate a sync time
 */
int32_t
setupRTPTime(mp4RTP *st) 
{
    struct timeval result;

    //srand(st->seed);
    st->seed++;
#if 0
    st->rtpTime = st->lastTimeStamp+1;//st->seed;
#else
    if(st->temp_rtp_time == 0)
	st->rtpTime = st->seed;
    else
	st->rtpTime = st->lastTimeStamp+1;//st->seed;

#endif
    timeval_subtract(&st->opEnd, &st->opStart, &result);
    //st->rtpTime = st->firstRTPTimestamp + (result.tv_sec * st->clkFreq);

    //st->rtpTime->tv_sec = rand();//st->rtpTime->tv_sec + 0x83AA7E80;
    //st->rtpTime->tv_usec = rand();

    //st->rtpTime = ( t.tv_sec * st->clkFreq ) + st->lastTimeStamp;
    return 0;
 

}

/*Creates a new sub-session state
 */
mp4RTP *
createSubsessionState(char *dataFileName, int32_t sockfd, 
		      uint32_t if_addr, char *vfs_open_mode,
		      size_t *tot_file_size)
{
    mp4RTP *psmss;
    send_ctx_t *ctx;
    FILE *fd;
    timeval tv;
    int32_t ret_val;

    psmss = NULL;
    ctx = NULL;

    /*allocate memory for the mp4RTP object */
    psmss =(mp4RTP *)malloc(sizeof(mp4RTP));
    if(!psmss) 
	return NULL;

    memset((char *)psmss, 0, sizeof(mp4RTP));

    psmss->magic = MP4_MAGIC_ALIVE;
    psmss->fFileName = strdup(dataFileName);
    psmss->ftcpSocketNum = sockfd;
    psmss->udpsocket = -1;
    psmss->ftaskToken = (TaskToken)-1;

    psmss->temp_rtp_time = 0;


    //init with PLAY state
    psmss->ps = PLAY;
    psmss->trickMode = 0;
    /*init the parser*/
    fd = nkn_vfs_fopen(dataFileName, vfs_open_mode);
    if(!fd) {
	free(psmss);
	return NULL;
    }
    psmss->fd = fd;    

#if 0
    if(!(*tot_file_size)) {
	*tot_file_size = nkn_vfs_get_file_size(psmss->fd);
    } else { 
	nkn_vfs_set_file_size(psmss->fd, *tot_file_size);
    }
#endif
	
    psmss->local_port = 27390;
    psmss->local_ip = if_addr;
    psmss->remote_port = 0;
    psmss->remote_ip = 0;
    pthread_mutex_init(&psmss->mp4_mutex, NULL);

    gettimeofday(&tv, NULL);
    psmss->seed = (uint32_t)tv.tv_usec;//needs to be passed by RTSP layer
    psmss->firstRTPTimestamp = psmss->seed;
    memset(&psmss->opStart, 0, sizeof(struct timeval));
    memset(&psmss->opEnd, 0, sizeof(struct timeval));
    psmss->sequenceNumber = (psmss->seed % 65535);
    //srand(psmss->seed);
    //psmss->sequenceNumber = rand();
    psmss->seek = (Seek_params_t*)calloc(1, sizeof(Seek_params_t));
    psmss->videoPkts = (Hint_sample_t*)calloc(SAMPLES_TO_BE_SENT, sizeof(Hint_sample_t));
    psmss->audioPkts = (Hint_sample_t*)calloc(SAMPLES_TO_BE_SENT, sizeof(Hint_sample_t));
#ifdef _USE_INTERFACE_

    psmss->st_all_info = init_all_info();
    psmss->sample = init_rtp_sample_info();
    Init(psmss->st_all_info,nkn_vpe_buffered_read, psmss->fd);

#else//else of _USE_INTERFACE_
    ctx = NULL;
    /*allocate memory for the parser state objects */
    psmss->si = (Seekinfo_t*)calloc(1, sizeof(Seekinfo_t));
    psmss->stbl = (Media_stbl_t*)calloc(1, sizeof(Media_stbl_t));
    init_seek_info(psmss->si);
    
#ifdef _FEAT_SEEK_
    if(ret_val == MP4_PARSER_ERROR) {
	return NULL;
    }
    send_next_pkt(psmss->si, nkn_vpe_buffered_read, psmss->fd, psmss->stbl, &ctx, psmss->videoPkts, psmss->audioPkts, VIDEO_TRAK, _NKN_PLAY_, psmss->seek);
#else
    send_next_pkt(psmss->si, nkn_vpe_buffered_read, psmss->fd, psmss->stbl, &ctx, psmss->videoPkts, psmss->audioPkts, VIDEO_TRAK);
#endif
    psmss->sctx = ctx;
#endif//end of _USE_INTERFACE_

    psmss->prev_iov_state = createIovBufState();
    return psmss;
}

/*initializes a new subsession state
 */
int32_t
initSubsessionState(mp4RTP* psmss, int32_t taskInterval, int32_t trackType)
{
#ifdef _USE_INTERFACE_
    psmss->fDuration = psmss->st_all_info->stbl_info->duration;
#else
    psmss->fDuration = psmss->stbl->duration;
#endif

    psmss->play_end_time = psmss->fDuration;

    setupRTPTime(psmss);
    //psmss->lastTimeStamp = psmss->rtpTime;

    seek2time(psmss->st_all_info, 0, trackType, nkn_vpe_buffered_read, psmss->fd);

        psmss->taskInterval = 30000;//int32_t(psmss->numSamples / psmss->fDuration);
    psmss->nextDTS = 0;
    psmss->trackType = trackType;


    if(isHintTrackPresent(psmss)) {
	psmss->cbFn = sendNext_hinted;
    } else {
	int prev_seq_num;
	if(trackType == AUDIO_TRAK)
	    prev_seq_num = psmss->sample->pkt_builder->header.sequence_num;
	
	psmss->cbFn = sendNext_unhinted;
	psmss->unhintedParser = init_parser_state(psmss->st_all_info, psmss->fd, trackType);
	if(init_rtp_packet_builder(psmss->unhintedParser, psmss->sample) == -1)
	    return -1;

	if(trackType == AUDIO_TRAK)
	    psmss->sample->pkt_builder->header.sequence_num = prev_seq_num;
	
    }
    
    setRefClk(psmss);
    //psmss->taskInterval = 1000;//int32_t(psmss->numSamples / psmss->fDuration);
    //psmss->nextDTS = 0;
    //psmss->trackType = trackType;


    return 0;
}

void 
freeAndNULL(void *ptr)
{
    if(ptr) {
	free(ptr);
	ptr = NULL;
    }
}
void
cleanupSessionState(mp4RTP *st) 
{
	if((st->ftaskToken == (TaskToken)-1) &&
			(st->magic == MP4_MAGIC_ALIVE)) {
	//	printf("task already scheduled; free on callback MAGIC is DEAD (fd=%d)\n", st->udpsocket);
		st->magic = MP4_MAGIC_DEAD;
		return;
	}

	//    printf("cleaning up (fd=%d), address %p\n", st->udpsocket, st);

	st->ftaskToken = (TaskToken) -1;
	//close the file handle
	//printf("closing vfs fd=%d\n", st->fd);

	if (st->fd)
		nkn_vfs_fclose(st->fd);
	st->fd = NULL;

	//close the socket descriptors created in the subsession
	//namely - (a) RTP (b) RTCP
	//close(st->udpsocket_rtcp);
	if(st->udpsocket != -1) {
		//printf("closing udp fd=%d\n", st->udpsocket);
		close(st->udpsocket);
	}

	/* free any partially sent data and the resources needed to
	 * store the partial data */
	deleteIovBufState(st->prev_iov_state);

	//free the file name buf
	freeAndNULL(st->fFileName);
#ifdef _USE_INTERFACE_
	//free video Pkts state
#if 0	
	freeAndNULL(st->st_all_info->videoh);
	freeAndNULL(st->st_all_info->video);
	//free audio Pkts state
	freeAndNULL(st->st_all_info->audioh);
	freeAndNULL(st->st_all_info->audio);
	//free seek info
	freeAndNULL(st->st_all_info->seek_state_info);
	free_all_stbls(st->st_all_info->stbl_info);
	free_ctx(st->st_all_info->sample_prop_info);
	freeAndNULL(st->st_all_info->stbl_info);
	freeAndNULL(st->st_all_info->sample_prop_info);
#else
	if(isHintTrackPresent(st)) {
	cleanup_mp4_hinted_parser(st);
	}
	else{
	cleanup_mp4_unhinted_parser(st);
	}
#endif
#else
   
	//free seek info
	freeAndNULL(st->si);
	free_all_stbls(st->stbl);
	free_ctx(st->sctx);
	freeAndNULL(st->stbl);
	freeAndNULL(st->sctx);
	//Karthik needs to provide functions for this
#endif

}

int32_t
cleanup_mp4_unhinted_parser(mp4RTP *st)
{
    cleanup_rtp_packet_builder(st->unhintedParser, st->sample);
    //cleanup_parser_state(st->unhintedParser);
    free_all_stbls(st->st_all_info->stbl_info);
    free_all_info_unhinted(st->st_all_info);
    cleanup_parser_state(st);
    cleanup_rtp_sample_info(st->sample);
    //free_all_stbls(st->st_all_info->stbl_info);
    //free_all_info(st->st_all_info);
    //cleanup_output_details(st->output);

    //SAFE_FREE(st->look_ahead_time);
    SAFE_FREE(st->seek);

    //free video Pkts state
    SAFE_FREE(st->videoPkts);

    //free audio Pkts state
    SAFE_FREE(st->audioPkts);

    SAFE_FREE(st);
    return 0;
}
int32_t cleanup_parser_state(mp4RTP *st){
    if(st->output != NULL)
        SAFE_FREE(st->output);
    //cleanup_output_details(parser->output);
    if(st->look_ahead_time != NULL)
        SAFE_FREE(st->look_ahead_time);
    if(st->unhintedParser->output != NULL)
        SAFE_FREE(st->unhintedParser->output);
    if(st->unhintedParser->look_ahead_time != NULL)
        SAFE_FREE(st->unhintedParser->look_ahead_time);
    if(st->unhintedParser != NULL)
        SAFE_FREE(st->unhintedParser);
    return 0;

}

int32_t
cleanup_mp4_hinted_parser(mp4RTP *st)
{
    cleanup_rtp_sample_info(st->sample);
    free_all_stbls(st->st_all_info->stbl_info);
    free_all_info(st->st_all_info);
    //cleanup_output_details(st->output);

    //SAFE_FREE(st->look_ahead_time);
    SAFE_FREE(st->seek);

    //free video Pkts state
    SAFE_FREE(st->videoPkts);

    //free audio Pkts state
    SAFE_FREE(st->audioPkts);

    SAFE_FREE(st);


    return 0;
}

/* computes the difference between two timeval structures
 */
int32_t
timeval_subtract (struct timeval *x, struct timeval *y, struct timeval *result)
{

    /* Perform the carry for the later subtraction by updating y. */
    if (x->tv_usec < y->tv_usec) {
	int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
	y->tv_usec -= 1000000 * nsec;
	y->tv_sec += nsec;
    }
    if (x->tv_usec - y->tv_usec > 1000000) {
	int nsec = (x->tv_usec - y->tv_usec) / 1000000;
	y->tv_usec += 1000000 * nsec;
	y->tv_sec -= nsec;
    }
     
    /* Compute the time remaining to wait.
       tv_usec is certainly positive. */
    result->tv_sec = x->tv_sec - y->tv_sec;
    result->tv_usec = x->tv_usec - y->tv_usec;
     
    /* Return 1 if result is negative. */
    return x->tv_sec < y->tv_sec;
}


/****************************************************************************************************
 *                               Media Codec CallBack Functions
 ***************************************************************************************************/
extern "C" Boolean createNew_mp4RTP (char * dataFileName, int sockfd, uint32_t if_addr, void ** pvideo, void ** paudio)
{
    mp4RTP * psmss;
    FILE *fd;
    int32_t i;
    send_ctx_t *ctx;
    Stream_info_t trackInfo;
    int32_t tracksToRead;
    int32_t temp,temp1,temp2,temp4;
    size_t tot_file_size;

    /* initialization */
    *pvideo = NULL;
    *paudio = NULL;
    fd = NULL;
#ifdef _USE_INTERFACE_
    i = 0;
#else
    i = 0;
#endif
    tracksToRead = 0;
    temp = 0;// by suma
    temp1 = 0;
    temp2 = 0;
    temp4 = 0;
    tot_file_size = 0;

    //    pthread_mutex_init(&parser_lock, NULL);
    //create atleast one subsession
    psmss = createSubsessionState(dataFileName, sockfd, if_addr, (char *)"rb",
				  &tot_file_size);
    if(!psmss) {
	return false;
    }

    //get the underlying MP4 track information; i.e. number of tracks and track types
#ifdef _USE_INTERFACE_
    get_stream_info(psmss->st_all_info->stbl_info, &trackInfo);
#else
    get_stream_info(psmss->stbl, &trackInfo);
#endif
    tracksToRead = trackInfo.num_traks;
    //number of tracks cant be less than 1
    if(tracksToRead < 1) { 
	return false;
    } else if(tracksToRead == 1) {
	if(trackInfo.track_type[0] != -1) {
	    //initialize a video subsession
	    if(initSubsessionState(psmss, 5000, trackInfo.track_type[0]) == -1)
		return false;
	    psmss->firstRTPTimestamp = psmss->rtpTime;
	    *pvideo = psmss;
	    psmss->other = NULL;
	}
	if(trackInfo.track_type[1] != -1) {
	    //initialize the audio subsession
	    if(initSubsessionState(psmss, 5000, trackInfo.track_type[1]) == -1)
		return false;
	    psmss->firstRTPTimestamp = psmss->rtpTime;
	    *paudio = psmss;
	    psmss->other = NULL;
	}
	return True;
    }

    //initializes the tracks
    while(tracksToRead) {

	switch(i) {
	    case VIDEO_TRAK:
		ctx = NULL;
		if(trackInfo.track_type[i] != -1) {
		    //initialize a video subsession
		    if(initSubsessionState(psmss, 5000, trackInfo.track_type[i]) == -1)
			return false;
		    psmss->firstRTPTimestamp = psmss->rtpTime;
		    *pvideo = psmss;
		    tracksToRead--;
		}
		i++;
		break;
	    case AUDIO_TRAK:
		if(trackInfo.track_type[i] != -1) {
		    //create a new audio subsession
		    psmss = createSubsessionState(dataFileName,
						  sockfd, if_addr,
						  (char *)"J", &tot_file_size);
		    if(!psmss) {
			return false;
		    }
		    //initialize the audio subsession
		    if(initSubsessionState(psmss, 5000, trackInfo.track_type[i]) == -1)
			return false;
		    psmss->firstRTPTimestamp = psmss->rtpTime;
		    *paudio = psmss;
		    tracksToRead--;
		}
		i++;
		break;
	}//end switch
    }//end while

    return True;
}


extern "C" char const* imp_trackId(void * p) 
{
    mp4RTP * psmss = (mp4RTP *)p;
    return psmss->fTrackId;
}


//#define AUDIO_ONLY
/* builds the SDP information. At the moment strips out the Audio track
 */
extern "C" char const* imp_sdpLines(void * p) 
{
    char buf[1024];
    SDP_info_t sdpInfo;

    mp4RTP * psmss = (mp4RTP *)p;
    memset(buf, 0, 1024);

	if(!psmss) {
		return NULL;
	}
    pthread_mutex_lock(&psmss->mp4_mutex);

    // SDP information is present at both the movie and the track level.
    // The text in the movie level SDP, if present should be placed before any media-specific lines.
    // Movie level SDP, should appear before the first 'm=' line in the SDP file

    // Call to get movie level SDP information

    if (psmss->trackType == VIDEO_TRAK) {

	if (psmss->sdpCallCount == 0 || psmss->sdpCallCount == 1) {
	    if((psmss->st_all_info->stbl_info->HintVidStbl.stsz_tab.size != -1)) {
		get_sdp_info( &sdpInfo, nkn_vpe_buffered_read, psmss->fd);
	    } else {
		get_hinted_sdp_info(psmss->unhintedParser, psmss->sample, &sdpInfo);
	    }
	    psmss->sdpCallCount++;
	           
	    char *pos, *pos_aud, *pos_vid;
	    const char *audioTag = "m=audio";
	    const char *videoTag = "m=video";
	    const char *trackIDSearch = "a=control:trackID=";
	    
	    pos_aud = strstr((char*)sdpInfo.sdp, audioTag);
	    pos_vid = strstr((char*)sdpInfo.sdp, videoTag);

	    if((uint8_t*)pos_aud == sdpInfo.sdp) {
		pos = pos_vid;
		memcpy(buf, &sdpInfo.sdp[((uint8_t*)pos)-sdpInfo.sdp], sdpInfo.num_bytes - (((uint8_t*)pos)-sdpInfo.sdp));
		//buf[sdpInfo.num_bytes - (((uint8_t*)pos)-sdpInfo.sdp)] =  '\n';
		buf[sdpInfo.num_bytes - (((uint8_t*)pos)-sdpInfo.sdp)+0] = '\0';
	    } else {
		pos = pos_aud;
		if(pos) {
		    sdpInfo.sdp[((uint8_t*)pos)-sdpInfo.sdp] = '\0';
		}
		snprintf(buf, sdpInfo.num_bytes+1, "%s", sdpInfo.sdp);
	    }

	    //psmss->fDuration = MP4_DURATION;

	    pos = strstr(buf, trackIDSearch);
	    sscanf(pos, "a=control:trackID=%s\n", psmss->trackID[0]);
	    snprintf(psmss->fTrackId, strlen(psmss->trackID[0])+10, "trackID=%s", psmss->trackID[0]/*TrackNumber*/);
	    free(sdpInfo.sdp);
	    pthread_mutex_unlock(&psmss->mp4_mutex);
	    return strdup(buf);
	} else {
	    pthread_mutex_unlock(&psmss->mp4_mutex);
	    return strdup(buf);
	} 
	
    }else { //AUDIO TRAK
	if (psmss->sdpCallCount == 0 || psmss->sdpCallCount == 1) {
	    if((psmss->st_all_info->stbl_info->HintAudStbl.stsz_tab.size != -1)) {
		get_sdp_info( &sdpInfo, nkn_vpe_buffered_read, psmss->fd);
	    }else {
		get_hinted_sdp_info(psmss->unhintedParser, psmss->sample, &sdpInfo);
	    }
	    psmss->sdpCallCount++;
	        
	    char *pos, *pos_aud, *pos_vid;
	    const char *audioTag = "m=audio";
	    const char *videoTag = "m=video";
	    const char *trackIDSearch = "a=control:trackID=";

	    pos_aud = strstr((char*)sdpInfo.sdp, audioTag);
	    pos_vid = strstr((char*)sdpInfo.sdp, videoTag);

	    if((uint8_t*)pos_vid == sdpInfo.sdp) {
		pos = pos_aud;
		memcpy(buf, &sdpInfo.sdp[((uint8_t*)pos)-sdpInfo.sdp], sdpInfo.num_bytes - (((uint8_t*)pos)-sdpInfo.sdp));
		//buf[sdpInfo.num_bytes - (((uint8_t*)pos)-sdpInfo.sdp)] =  '\n';
		buf[sdpInfo.num_bytes - (((uint8_t*)pos)-sdpInfo.sdp)+0] = '\0';

	    } else {
		pos = pos_vid;
		if(pos) {
		    sdpInfo.sdp[((uint8_t*)pos)-sdpInfo.sdp] = '\0';
		}
		snprintf(buf, sdpInfo.num_bytes+1, "%s", sdpInfo.sdp);
	    }

	    //sdpInfo.sdp[((uint8_t*)pos)-sdpInfo.sdp] = '\0';
	    
	    //psmss->fDuration = MP4_DURATION;
	    pos = strstr(buf, trackIDSearch);
	    sscanf(pos, "a=control:trackID=%s\n", psmss->trackID[0]);
	    snprintf(psmss->fTrackId, strlen(psmss->trackID[0])+10, "trackID=%s", psmss->trackID[0]/*TrackNumber*/);
	    free(sdpInfo.sdp);
	    pthread_mutex_unlock(&psmss->mp4_mutex);
	    return strdup(buf);
	        
	} else {
	    pthread_mutex_unlock(&psmss->mp4_mutex);
	    return strdup(buf);
	}
    }
    psmss->sdpCallCount++;
    return NULL;

}

/* Sets up the following
 * a. RTP/RTCP server ports by binding to any two successive port pairs. this information is returned via the 
 DESCRIBE reply
 b. Stores the RTCP and RTP Client ports for sending information to the client
 c. creates UDP sockets for RTP and RTCP
*/
extern "C" void * imp_getStreamParameters(void * p,
					  unsigned clientSessionId, // in
					  uint32_t clientAddress, // in
					  uint16_t * clientRTPPort, // in network order
					  uint16_t * clientRTCPPort, // in network order
					  int tcpSocketNum, // in (-1 means use UDP, not TCP)
					  unsigned char rtpChannelId, // in (used if TCP)
					  unsigned char rtcpChannelId, // in (used if TCP)
					  uint32_t* destinationAddress, // in out
					  u_int8_t* destinationTTL, // in out
					  Boolean* isMulticast, // out
					  uint16_t * serverRTPPort, // out network order
					  uint16_t * serverRTCPPort, // out network order
					  uint32_t *ssrc, // out
					  void* streamToken // out
					  ) 
{
	struct sockaddr_in si_me;
	mp4RTP * psmss = (mp4RTP *)p;

	if(!psmss) {
		return NULL;
	}

    pthread_mutex_lock(&psmss->mp4_mutex);
#ifdef PRINT_STATUS
    printf("mp4rtp: getStreamParameters\n");
#endif

    if (tcpSocketNum == -1) {

    	if ((psmss->udpsocket = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1) {
#ifdef PRINT_STATUS
		printf("rtsp_init_udp_socket: failed\n");
#endif
		perror("socket() failed: ");
		exit(2);
    	}

         memset((char *) &si_me, 0, sizeof(si_me));
 again:
        si_me.sin_family = AF_INET;
        si_me.sin_port = htons(psmss->local_port);
        si_me.sin_addr.s_addr = psmss->local_ip;
        if (bind(psmss->udpsocket, (struct sockaddr *)&si_me, sizeof(si_me))==-1) {
		psmss->local_port=psmss->local_port + 2;
		goto again;
        }
    }
    
    psmss->remote_ip = *destinationAddress = clientAddress;
    psmss->remote_port = *clientRTPPort;
    *clientRTCPPort = htons(ntohs(*clientRTPPort) + 1);

#ifdef PRINT_STATUS
    printf("rtsp_init_udp_socket udp listen on port=%d\n", psmss->local_port);
    printf("rtsp_init_udp_socket clientRTPPort=%d, destinAddress=0x%x\n", ntohs(*clientRTPPort), *destinationAddress);
#endif

    /* 
     * RTCP port is opened in common RTCP module 
     * --Zubair/Max
     */
#if 0
    memset((char *) &si_me, 0, sizeof(si_me));
 again_rtcp:
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(psmss->local_port+1);
    si_me.sin_addr.s_addr = psmss->local_ip;
    if (bind(psmss->udpsocket_rtcp, (struct sockaddr *)&si_me, sizeof(si_me))==-1) {
	psmss->local_port = psmss->local_port+2;
	goto again_rtcp;
    }

#ifdef PRINT_STATUS
    printf("rtsp_init_udp_socket udp listen on port=%d\n", psmss->local_port+1);
    printf("rtsp_init_udp_socket clientRTCPPort=%d, destinAddress=0x%x\n", ntohs(*clientRTCPPort), *destinationAddress);
#endif
#endif // 0

    *destinationTTL = 0;
    *isMulticast = False;
    *serverRTPPort = htons(psmss->local_port);
    *serverRTCPPort =  htons(psmss->local_port+1);

    if (psmss->trackType == VIDEO_TRAK) {
	psmss->ssrc = *ssrc = 0x6952;
    } else {
	psmss->ssrc = *ssrc = 0x7013;
    }

    psmss->channel_id = rtpChannelId;

    pthread_mutex_unlock(&psmss->mp4_mutex);
    return 0; //psmss->streamToken;
}

/* Sends next access unit sample. we are now grouping 3 samples and sending them out. This needs to be modified to send
   out samples based on the AFR/VBR  of the video
*/


static void sendNext_hinted(void *firstArg)
{
    mp4RTP *psmss = (mp4RTP *)firstArg;
    struct sockaddr_in si_other;

    Seekinfo_t *si;
    Media_stbl_t *stbl;
    send_ctx_t *ctx;
    int32_t i, sret, ret, j, retrycnt, samp;
    uint8_t *p_buf = NULL /*static Buffer size since MTU is usually on 1420 bytes*/, *buf;
    RTP_whole_pkt_t *rtpPkt;
    size_t pos;
#ifdef _USE_INTERFACE_
    Media_sample_t *hs;
#else
    Hint_sample_t *hs;
#endif
    struct timeval result;
    pthread_mutex_t lock;

    struct timespec selfTimeStart, selfTimeEnd;
#ifdef PERF_DEBUG
    int64_t skew;
#endif

    int slen = sizeof(struct sockaddr_in);

    pthread_mutex_lock(&psmss->mp4_mutex);
    if(psmss->magic == MP4_MAGIC_DEAD) {
	//	printf("task already scheduled; delayed free on callback MAGIC is DEAD (fd=%d)\n", psmss->udpsocket);
	lock = psmss->mp4_mutex;
	cleanupSessionState(psmss);
	pthread_mutex_unlock(&lock);
	return;
    }
    psmss->ftaskToken = (TaskToken)-1;
    if(!psmss || !psmss->fd) {
	pthread_mutex_unlock(&psmss->mp4_mutex);
	return;
    }

#ifdef _USE_INTERFACE_
    si = psmss->st_all_info->seek_state_info;
    stbl = psmss->st_all_info->stbl_info;
#else
    si = psmss->si;
    stbl = psmss->stbl;
#endif

    sret = ret = 0;
    i = 0;
    j = 0;
    samp = 0;
    buf = p_buf;
    ctx = NULL;


    if(psmss->sampleNumber == 0) { //|| psmss->ps == PAUSE) {
	gettimeofday(&psmss->lastPktTime, NULL);
	psmss->ps = PLAY;
    } else if (psmss->ps == SEEK || psmss->ps == PAUSE) {
	gettimeofday(&psmss->lastPktTime, NULL);
#ifdef _USE_INTERFACE_
	if(psmss->trackType==VIDEO_TRAK){
	    psmss->seek->seek_time =int32_t((((float)(psmss->st_all_info->sample_prop_info->hvid_s->look_ahead_time))/(float)(psmss->clkFreq)));
	}
	else
	    psmss->seek->seek_time =int32_t((((float)(psmss->st_all_info->sample_prop_info->haud_s->look_ahead_time))/(float)(psmss->clkFreq)));
#else
	if(psmss->trackType==VIDEO_TRAK)
	    psmss->seek->seek_time =int32_t((((float)(psmss->sctx->hvid_s->look_ahead_time))/(float)(psmss->clkFreq)));
	else
	    psmss->seek->seek_time =int32_t((((float)(psmss->sctx->haud_s->look_ahead_time))/(float)(psmss->clkFreq)));
#endif
	psmss->lastPktTime.tv_sec = psmss->lastPktTime.tv_sec + (time_t)(psmss->seek->seek_time);
	//NK:
	psmss->timeElapsed = uint64_t((psmss->seek->seek_time) * 1e6);
	psmss->nextDTS = 0;
    }
    gettimeofday(&psmss->currPktTime, NULL);
    psmss->currPktTime.tv_sec += psmss->seek->seek_time;
    timeval_subtract(&psmss->currPktTime, &psmss->lastPktTime, &result);
    memcpy(&psmss->lastPktTime, &psmss->currPktTime, sizeof(struct timeval));
    psmss->timeElapsed += uint64_t((result.tv_sec*1e6)+(result.tv_usec));
#ifdef PERF_DEBUG
    //gettimeofday(&selfTimeStart, NULL); 
#endif
    if((psmss->play_end_time*1e6) < psmss->nextDTS){
        ret == NOMORE_SAMPLES;
        pthread_mutex_unlock(&psmss->mp4_mutex);
        return;
    }

    if((psmss->timeElapsed*1000) >= floorl(psmss->nextDTS*1000)) {  
#ifdef _USE_INTERFACE_
	ret = send_next_pkt_hinted(psmss->st_all_info,psmss->trackType,nkn_vpe_buffered_read, nkn_vpe_buffered_read_opt,psmss->fd,psmss->videoPkts,psmss->audioPkts);
#else//else of _USE_INTERFACE_
	ctx = psmss->sctx;
#ifndef _FEAT_SEEK_
	ret = send_next_pkt(si, nkn_vpe_buffered_read, psmss->fd, stbl, &ctx, psmss->videoPkts, psmss->audioPkts, psmss->trackType);
#else
	ret = send_next_pkt(si, nkn_vpe_buffered_read, psmss->fd, stbl, &ctx, psmss->videoPkts, psmss->audioPkts, psmss->trackType, _NKN_PLAY_, psmss->seek);
#endif
#endif//end of _USE_INTERFACE_

#ifdef PERF_DEBUG    
	{
	    struct timeval result1;
	    gettimeofday(&selfTimeEnd,NULL);
	    timeval_subtract(&selfTimeEnd, &selfTimeStart, &result1);
	    //printf("TRAK TYPE %d, Sample Num %d, Time Elapsed %lu <---------> Expected Time %d ===== next DTS is  %d\n", psmss->trackType, psmss->sampleNumber, psmss->timeElapsed, psmss->lastDTS, psmss->nextDTS);
	    //printf("TRAK TYPE %d, Sample Num %d, Self Time %ld\n", psmss->trackType, psmss->sampleNumber, (int64_t)((result1.tv_sec*1e6)+result1.tv_usec));
	}
#endif
	if(ret == NOMORE_SAMPLES) {
	    //unscheduleDelayedTask(psmss->ftaskToken);
	    // psmss->ftaskToken = (TaskToken)-1;
	    //    pthread_mutex_unlock(&parser_lock);
	    pthread_mutex_unlock(&psmss->mp4_mutex);
	    return;
	}

#ifdef _USE_INTERFACE_
	if(psmss->trackType == VIDEO_TRAK) {
	    psmss->numSamples = psmss->st_all_info->sample_prop_info->hvid_s->total_sample_size;
	    //psmss->taskInterval = int32_t(psmss->fDuration / psmss->numSamples *1e6)-5000;
	    hs =  psmss->st_all_info->videoh;//[samp].num_rtp_pkts
	    //psmss->audioPkts = (Hint_sample_t *)audiopkts;
	} else {
	    psmss->numSamples = psmss->st_all_info->sample_prop_info->haud_s->total_sample_size;
	    //psmss->taskInterval = int32_t(psmss->fDuration / psmss->numSamples * 1e6)-5000;
	    hs = psmss->st_all_info->audioh;
	    //psmss->videoPkts = (Hint_sample_t *)videopkts;
	}
#else    
	if(psmss->trackType == VIDEO_TRAK) {
	    psmss->numSamples = psmss->sctx->hvid_s->total_sample_size;
	    //psmss->taskInterval = int32_t(psmss->fDuration / psmss->numSamples *1e6)-5000;
	    hs =  psmss->videoPkts;//[samp].num_rtp_pkts
	} else {
	    psmss->numSamples = psmss->sctx->haud_s->total_sample_size;
	    //psmss->taskInterval = int32_t(psmss->fDuration / psmss->numSamples * 1e6)-5000;
	    hs = psmss->audioPkts;
	}
#endif

#ifdef PRINT_DBG
	printf("Track Type %d, time elapsed %lu, next DTS %lu\n", psmss->trackType, (psmss->timeElapsed*1000), (uint64_t)(floorl(psmss->nextDTS*1000)));
	if(psmss->nextDTS == 0)
	    int ijk = 0;
#endif
	//store the file pos for future parsing
	pos = nkn_vfs_ftell(psmss->fd);

	/*RTP hit sample structure 
******************************************************************************************
* HINT SAMPLE ORGANIZATION
* Hint_sample_t (container for exactly 1 hint sample, can be audio or video)
*             |
*             ------->RTP_whole_pkt_t(1)
*             |                      |
*             |                       ----->payload_info_t(entry_counts)
*             ------->RTP_whole_pkt_t(2)
*             |                      |
*             |                       ----->payload_info_t(entry_counts)
*             |
*             |
*             |
*             ------->RTP_whole_pkt_t(n)
********************************************************************************************/
	//start sending packets
	//Each RTP hint sample can have multiple RTP packets (num_rtp_pkts) in number
	struct iovec *iov;
	int counter;
	struct msghdr mh;
	for(samp = 0; samp < SAMPLES_TO_BE_SENT; samp++) {

	    psmss->sampleNumber++;
	    //psmss->ts = int32_t(232171483)+(psmss->sampleNumber * 9009);

	    for(i = 0;i < hs[samp].num_rtp_pkts; i++) {

		buf = p_buf;

		//get the RTP packet definition
		rtpPkt = &(hs[samp].rtp[i]);
		
		//allocate the buffer for total payload size
		sret = rtpPkt->total_payload_size + RTP_PACKET_SIZE + 4; // additional 4 used, if RTP/TCP
		p_buf = buf = (uint8_t*)malloc(sret);
		
	 	iov = (struct iovec *) calloc(2 + rtpPkt->num_payloads, sizeof(struct iovec))	;
		counter = 0;
		iov[counter].iov_base = buf;
		iov[counter++].iov_len = 4; 
		iov[counter].iov_base = buf +4;
		iov[counter++].iov_len = RTP_PACKET_SIZE;
		//build RTP packet headers (MPEG4 hint structures dont have the RTP time offsets, SSRC as a part of the header -
		//we need to build this manually)
		buildRTPPacket(psmss, &rtpPkt->header, buf+4);
		psmss->ts = rtpPkt->header.relative_time;
		buf += (RTP_PACKET_SIZE + 4);


		//	printf("RTP time = %d\n",psmss->ts);
		//each RTP packet can have num_payloads data offsets
		char *temp_buf;
		int free_buf;
		for(j = 0; j < rtpPkt->num_payloads; j++) { 

		    //read the payload from offsets in the MP4 file for the packet 
		    //printf("write offset %u read offset %d bytes read %d\n", buf, rtpPkt->payload[j].payload_offset, rtpPkt->payload[j].payload_size);
		    nkn_vfs_fseek(psmss->fd, rtpPkt->payload[j].payload_offset, SEEK_SET);

		    nkn_vpe_buffered_read_opt(buf, 1, rtpPkt->payload[j].payload_size, psmss->fd, (void **)&temp_buf,&free_buf);
		    if (!free_buf) {
			iov[counter].iov_base = temp_buf;
		    } else {
			iov[counter].iov_base = buf;
		    }	 
		    iov[counter++].iov_len = rtpPkt->payload[j].payload_size; 
		    //update the data pointers
		    buf += rtpPkt->payload[j].payload_size;
		        
		}

		retrycnt = 5;
		buf = p_buf;

		if (psmss->udpsocket != -1) {
			buf += 4;
			sret -= 4;
			memset((char *) &si_other, 0, sizeof(si_other));
			si_other.sin_family = AF_INET;
			si_other.sin_port = psmss->remote_port;
			si_other.sin_addr.s_addr = psmss->remote_ip;//inet_addr("192.168.10.103");//psmss->remote_ip;
		} else 
			addRtpTcphdr(buf, psmss->channel_id, sret-4);

		if (sret) {
		    //printf("sending %d bytes from adress %u\n", sret, buf);
#ifdef DUMP_PKTS //requires write permission on '/'
		    {
			char fn[256];
			FILE *f;
			int32_t pp;
			uint16_t *dbg_buf;
			dbg_buf = (uint16_t*)buf;
			sprintf(fn, "out%d", psmss->sequenceNumber);
			f = fopen(fn, "wb");
			for(pp = 0;pp < (sret/2);pp++) {
			    //dbg_tmp = swap_uint16(dbg_buf[pp]);
<			    fwrite(&dbg_buf[pp], 1, 2, f);
			}
			fclose(f);
		    }
#endif
		    mh.msg_control = NULL;
		    mh.msg_controllen = 0;
		    mh.msg_flags = 0;
		    if (psmss->udpsocket != -1) { // RTP/UDP
			mh.msg_name = &si_other;
			mh.msg_namelen = sizeof(si_other);
			mh.msg_iov = &iov[1];
			mh.msg_iovlen = counter - 1;
			ret = sendmsg(psmss->udpsocket, &mh, MSG_DONTWAIT);
			if (ret >= 0) { // curr pkt sent. Half sent is
			                // not an issue with UDP
				updateRTSPCounters(ret);
				free(p_buf);
				free(iov);
			}
		    }
		    else { // RTP/TCP
			mh.msg_name = NULL;
			mh.msg_namelen = 0;
			mh.msg_iov = iov;
			mh.msg_iovlen = counter;
			ret = tcpIovSend(psmss->prev_iov_state, psmss->ftcpSocketNum, &mh, sret, 0x02);
			if (ret > 0)
				updateRTSPCounters(ret);
		    }	
		    if(ret<0) { //if any socket error occurs, drop current packet and go to the next packet
			perror("socket send error hinted: " );
			free(rtpPkt->payload);
			free(p_buf);
			free(iov);
			free(&hs[samp].rtp[0]);
			psmss->ftaskToken = scheduleDelayedTask(psmss->taskInterval, (TaskFunc*)sendNext_hinted, firstArg);
			pthread_mutex_unlock(&psmss->mp4_mutex);
			return;
		    }
#ifdef PRINT_DBG
		    //printf("sequence Number %d, sample Number %d, timestamp %ud, bytes sent %d of %d\n", psmss->sequenceNumber, psmss->sampleNumber, (psmss->ts), ret, rtpPkt->total_payload_size + RTP_PACKET_SIZE);
#endif
		}
		free(rtpPkt->payload);
	    }
	    free(&hs[samp].rtp[0]);
	}
	nkn_vfs_fseek(psmss->fd, pos, SEEK_SET);

#ifdef PRINT_DBG
	//printf("TRAK TYPE %d, Sample Num %d, Time Elapsed %lu <---------> Expected Time %u %d next DTS \n", psmss->trackType, psmss->sampleNumber, psmss->timeElapsed, psmss->lastDTS, psmss->nextDTS);
#endif
    }
    
    //reschedule the callback function
    psmss->ftaskToken = scheduleDelayedTask(psmss->taskInterval, (TaskFunc*)sendNext_hinted, firstArg);
    pthread_mutex_unlock(&psmss->mp4_mutex);
    return;

}

static void sendNext_unhinted(void *firstArg)
{
    Seekinfo_t *si;
    Media_stbl_t *stbl;
    send_ctx_t *ctx;
    mp4RTP *psmss = (mp4RTP *)firstArg;
    int32_t i, sret, ret, j, retrycnt, samp, currPktTime;
    uint8_t *p_buf=NULL /*static Buffer size since MTU is usually on 1420 bytes*/, *buf;
    struct sockaddr_in si_other;
    size_t pos = 0;
    struct iovec *vec;
    RTP_pkt_t pkt_hdr;
    uint8_t *videoRTPStart = NULL;
    uint32_t firstPacket;
    GList *lastElement;    
#ifdef _USE_INTERFACE_
    Media_sample_t *hs;
#else
    Hint_sample_t *hs;
#endif
    struct timeval result;

#ifdef PERF_DEBUG
    struct timeval selfTimeStart, selfTimeEnd;
    int64_t skew;
#endif

    int slen = sizeof(struct sockaddr_in);
    int seek_flag = 0;
    uint16_t final_seq_num;
    pthread_mutex_t lock;
    pthread_mutex_lock(&psmss->mp4_mutex);
    if(psmss->magic == MP4_MAGIC_DEAD) {
	//	printf("task already scheduled; delayed free on callback MAGIC is DEAD (fd=%d)\n", psmss->udpsocket);
	lock = psmss->mp4_mutex;
	cleanupSessionState(psmss);
	pthread_mutex_unlock(&lock);
	return;
    }
    psmss->ftaskToken = (TaskToken)-1;
    if(!psmss || !psmss->fd) {
	// unscheduleDelayedTask(psmss->ftaskToken);
	// psmss->ftaskToken = (TaskToken)-1;
	//      pthread_mutex_unlock(&parser_lock);
	pthread_mutex_unlock(&psmss->mp4_mutex);
	return;
    }
#ifdef _USE_INTERFACE_
    si = psmss->st_all_info->seek_state_info;
    stbl = psmss->st_all_info->stbl_info;
#else
    si = psmss->si;
    stbl = psmss->stbl;
#endif
    sret = ret = 0;
    i = 0;
    j = 0;
    samp = 0;
    buf = p_buf;
    ctx = NULL;
    lastElement = NULL;

    //    pthread_mutex_lock(&parser_lock);
    if(psmss->sampleNumber == 0){// || psmss->ps == PAUSE) {
	gettimeofday(&psmss->lastPktTime, NULL);
	psmss->ps = PLAY;
	seek_flag = 0;//		psmss->sample->pkt_builder->nal->DTS = psmss->seek->seek_time*psmss->sample->clock;
    } else if (psmss->ps == SEEK|| psmss->ps == PAUSE) {
	seek_flag = 1;//		psmss->sample->pkt_builder->nal->DTS = psmss->seek->seek_time*psmss->sample->clock;
	gettimeofday(&psmss->lastPktTime, NULL);
	psmss->lastPktTime.tv_sec = psmss->lastPktTime.tv_sec + (time_t)(psmss->seek->seek_time)-1;
	//NK:
	psmss->timeElapsed = uint64_t((psmss->seek->seek_time) * 1e6);
	psmss->nextDTS = 0;
	psmss->ps = PLAY;
	    
    }

    gettimeofday(&psmss->currPktTime, NULL);
    psmss->currPktTime.tv_sec += psmss->seek->seek_time;
    timeval_subtract(&psmss->currPktTime, &psmss->lastPktTime, &result);
    //	if(result.tv_sec>=0)
    memcpy(&psmss->lastPktTime, &psmss->currPktTime, sizeof(struct timeval));
    psmss->timeElapsed += uint64_t((result.tv_sec*1e6)+(result.tv_usec));

#ifdef PERF_DEBUG
    gettimeofday(&selfTimeStart, NULL); 
#endif

    if((psmss->play_end_time*1e6) < psmss->nextDTS){
	ret == NOMORE_SAMPLES;
	pthread_mutex_unlock(&psmss->mp4_mutex);
	return;
    }
    if((psmss->timeElapsed*1000) >= floorl(psmss->nextDTS*1000)) {
	psmss->sample->pkt_list = NULL;
	while(ret != RTP_PKT_READY && ret!=NOMORE_SAMPLES) {
	    ret = send_next_pkt_unhinted(psmss->unhintedParser, psmss->sample);
	}
	
	if(ret == NOMORE_SAMPLES) {
	    // unscheduleDelayedTask(psmss->ftaskToken);
	    // psmss->ftaskToken = (TaskToken)-1;
	    //    pthread_mutex_unlock(&parser_lock);
	    pthread_mutex_unlock(&psmss->mp4_mutex);
	    return;
	}

	int32_t cnt =0;
	lastElement = g_list_first(psmss->sample->pkt_list);
	psmss->sample->pkt_list= g_list_last(psmss->sample->pkt_list);
	
	firstPacket = 1;
	while(psmss->sample->pkt_list) {
	    vec = (struct iovec*)psmss->sample->pkt_list->data;
				
#ifdef PERF_DEBUG    
	    {
		struct timeval result1;
		gettimeofday(&selfTimeEnd,NULL);
		timeval_subtract(&selfTimeEnd, &selfTimeStart, &result1);
		//printf("TRAK TYPE %d, Sample Num %d, Time Elapsed %lu <---------> Expected Time %d ===== next DTS is  %d\n", psmss->trackType, psmss->sampleNumber, psmss->timeElapsed, psmss->lastDTS, psmss->nextDTS);
		//printf("TRAK TYPE %d, Sample Num %d, Self Time %ld\n", psmss->trackType, psmss->sampleNumber, (int64_t)((result1.tv_sec*1e6)+result1.tv_usec));
	    }
#endif
		    
#ifdef PRINT_DBG
	    //		printf("Pkt num = %d size = %d\n",cnt++,(int32_t)vec->iov_len);
	    //		printf("Track Type %d, time elapsed %lu, next DTS %lu\n", psmss->trackType, (psmss->timeElapsed*1000), (uint64_t)(floorl(psmss->nextDTS*1000)));
#endif
	    retrycnt = 5;
	    buf = (uint8_t*)vec->iov_base;
	    sret = vec->iov_len;

	    currPktTime = _read32(buf,4);//*((int32_t*)(&buf[4]));
	    //    currPktTime += (psmss->seek->seek_time * psmss->clkFreq);
	    //		psmss->lastDTS = ((1e6*(float)(currPktTime))/(float)(psmss->sample->clock)) + psmss->seek->seek_time;
	    //		psmss->nextDTS = ((1e6*(float)(*psmss->unhintedParser->look_ahead_time))/(float)(psmss->clkFreq)) + (1e6 *psmss->seek->seek_time);


	    psmss->nextDTS = ((1e6*(float)(*psmss->unhintedParser->look_ahead_time))/(float)(psmss->clkFreq));// -  (1e6 *psmss->seek->seek_time);	
	    psmss->lastDTS = ((1e6*(float)(currPktTime))/(float)(psmss->sample->clock));// -  (1e6*psmss->seek->seek_time); 

	    currPktTime = psmss->rtpTime + (currPktTime - psmss->seek->seek_time*psmss->sample->clock);
				
	    buf[4] = currPktTime>>24;buf[5] = (currPktTime>>16)&0xff; buf[6] = (currPktTime>>8)&0xff;buf[7] = currPktTime&0xff;
	    //				final_seq_num = _read16(buf,2);
	    //final_seq_num+= psmss->sequenceNumber;
	    final_seq_num = psmss->sequenceNumber++;
	    buf[2] = final_seq_num>>8;
	    buf[3] = final_seq_num&0xff;
	    psmss->sampleNumber++;
	    psmss->lastTimeStamp = currPktTime;

	    uint8_t* buf_tmp = NULL;
	    struct iovec* iov = (struct iovec*) calloc(1, sizeof(struct iovec));
	    struct msghdr mh;
	    mh.msg_control = NULL;
	    mh.msg_controllen = 0;
	    mh.msg_flags = 0;
	    mh.msg_iov = iov;
	    mh.msg_iovlen = 1;

    	    if (psmss->udpsocket != -1) {
	    	 memset((char *) &si_other, 0, sizeof(si_other));
	    	 si_other.sin_family = AF_INET;
	    	 si_other.sin_port = psmss->remote_port;
	    	 si_other.sin_addr.s_addr = psmss->remote_ip;//inet_addr("192.168.10.103");//psmss->remote_ip;
		 mh.msg_name = &si_other;
		 mh.msg_namelen = sizeof(si_other);
		 iov->iov_base = buf;
		 iov->iov_len = sret;
	    } else {
	    	 buf_tmp = (uint8_t*)malloc(sret+4);
       	  	 if (buf_tmp == NULL)
			exit(2);
	    	 addRtpTcphdr(buf_tmp, psmss->channel_id, sret);
	    	 memcpy(buf_tmp+4, buf, sret);		
	 	 sret += 4;
		 mh.msg_name = NULL;
		 mh.msg_namelen = 0;
		 iov->iov_base = buf_tmp;
		 iov->iov_len = sret;
	    }
	    if (sret) {
#ifdef DUMP_PKTS //requires write permission on '/'
		{
		    char fn[256];
		    FILE *f;
		    int32_t pp;
		    uint16_t *dbg_buf;
		    dbg_buf = (uint16_t*)buf;
		    sprintf(fn, "out%d", psmss->sequenceNumber);
		    f = fopen(fn, "wb");
		    for(pp = 0;pp < (sret/2);pp++) {
			//dbg_tmp = swap_uint16(dbg_buf[pp]);
			fwrite(&dbg_buf[pp], 1, 2, f);
		    }
		    fclose(f);
		}
#endif
    	 	if (psmss->udpsocket != -1) {
		    	//ret = sendto(psmss->udpsocket, buf, sret, MSG_DONTWAIT, (struct sockaddr *)&si_other, slen);
			ret = sendmsg(psmss->udpsocket, &mh,
				      MSG_DONTWAIT);
			updateRTSPCounters(ret);
			
			free(iov);
	        } else {
			ret = tcpIovSend(psmss->prev_iov_state, psmss->ftcpSocketNum, &mh, sret, 0x02);
			if (ret < 0) {
				free(mh.msg_iov->iov_base);
				free(iov);
			} else
				updateRTSPCounters(ret);
	        }
		
		if(ret<0) { //if any socket error occurs, drop current packet and go to the next packet
		    perror("unhinted socket send error :");
		    psmss->ftaskToken = scheduleDelayedTask(psmss->taskInterval, (TaskFunc*)sendNext_unhinted, firstArg);
		    if(psmss->trackType == AUDIO_TRAK) {
			SAFE_FREE(vec->iov_base);
			SAFE_FREE(vec);
		    } else {
			//SAFE_FREE(vec);
			if(videoRTPStart) {
			    SAFE_FREE(videoRTPStart);
			} else {
			    SAFE_FREE(vec->iov_base);
			}
			SAFE_FREE(vec);
		    }
		    g_list_free(lastElement);
		    pthread_mutex_unlock(&psmss->mp4_mutex);
		    return;
		}
#ifdef PRINT_DBG
		//		    printf("sequence Number %d, sample Number %d, timestamp %ud, bytes sent %d of %d\n", psmss->sequenceNumber, psmss->sampleNumber, (psmss->ts), ret, rtpPkt->total_payload_size + RTP_PACKET_SIZE);
#endif
	    }

	    if(psmss->trackType == AUDIO_TRAK) {
		SAFE_FREE(vec->iov_base);
		SAFE_FREE(vec);
	    } else {
		if(firstPacket) {
		    videoRTPStart = (uint8_t*)(vec->iov_base);
		    firstPacket = 0;
		}

		SAFE_FREE(vec);
	    }
	    psmss->sample->pkt_list = g_list_previous(psmss->sample->pkt_list);
	}
	
	GList *tmp, *next;
	tmp = lastElement;
	    for(; tmp; ) {
	    next = tmp->next;
	    lastElement = g_list_remove_link(lastElement, tmp);
	    g_list_free(tmp);
	    tmp = next;
	}

	if(psmss->trackType != AUDIO_TRAK && videoRTPStart) {
	    SAFE_FREE(videoRTPStart);
	}
	n_calls_send++;
	//	printf("mem lost so far = %ld\n", n_calls_send);		
    }

    nkn_vfs_fseek(psmss->fd, pos, SEEK_SET);

#ifdef PRINT_DBG
    //printf("TRAK TYPE %d, Sample Num %d, Time Elapsed %lu <---------> Expected Time %u %d next DTS \n", psmss->trackType, psmss->sampleNumber, psmss->timeElapsed, psmss->lastDTS, psmss->nextDTS);
#endif
    //reschedule the callback function
    psmss->ftaskToken = scheduleDelayedTask(psmss->taskInterval, (TaskFunc*)psmss->cbFn, firstArg);
    //    pthread_mutex_unlock(&parser_lock);
    pthread_mutex_unlock(&psmss->mp4_mutex);
    return;

}

extern "C" int imp_startStream(void * p,
				unsigned clientSessionId, 
				void* streamToken,
				TaskFunc* rtcpRRHandler,
				void* rtcpRRHandlerClientData,
				unsigned short * rtpSeqNum,
				unsigned * rtpTimestamp) 
{

	mp4RTP *psmss = (mp4RTP *)p;


#ifdef PRINT_STATUS
	printf("start stream\n");
#endif
	psmss = psmss;

	if(!psmss) {
		return -1;
	}
	pthread_mutex_lock(&psmss->mp4_mutex);

	if(psmss->ps == TRICKMODE) {
	        /* If trick mode is set we continue to play at 1x speed
		 * it should be noted that we __should__ not schedule a
		 * new task at this point since we will have a scenario 
		 * where there will be more than 1 pending tasks per context
		 * with the current design we __cannot__ handle that
		 */
		*rtpSeqNum = psmss->sequenceNumber;
		*rtpTimestamp = psmss->lastTimeStamp;
		/* reset the play state to PLAY if the trickMode field in the
		 * context is set
		 */
		switch(psmss->trickMode) {
		    case 1:
			psmss->ps = PLAY;
			break;
		    case 2:
			psmss->ps = PAUSE;
			break;
		}
		pthread_mutex_unlock(&psmss->mp4_mutex);
		return 0;
	}

	if(psmss->ps == PAUSE) {//if the PAUSE request has been correctly recieved and responded to

		*rtpSeqNum = psmss->sequenceNumber;
		*rtpTimestamp = psmss->lastTimeStamp;
		psmss->ftaskToken = scheduleDelayedTask(psmss->taskInterval, (TaskFunc*)psmss->cbFn, p);
		gettimeofday(&psmss->opEnd, NULL);
		if(isHintTrackPresent(psmss))
		    setupRTPTime(psmss);
		else
		    setupRTPTime(psmss);//psmss->seed++;
		// we reset the clocks etc in the next subsequent call to sendNext. Also not the playstate is reset to PLAY in 
		// sendNext, which is why we have not handled it here
	
	} else { //seek or play - we need to init the state 
		//if(!isHintTrackPresent(psmss)) {
			//cleanup_rtp_packet_builder(psmss->unhintedParser, psmss->sample);
	    //}

		psmss->trickMode = 1; // state is allowed to be set as tick mode
		if(isHintTrackPresent(psmss)) {
		    //cleanup_mp4_hinted_parser(st);
		}
		else{
		    if (psmss->trackType == AUDIO_TRAK){
#if 0
		    if(psmss->sample->pkt_builder->es_desc->dc->specific_info != NULL)
                        SAFE_FREE(psmss->sample->pkt_builder->es_desc->dc->specific_info);
		    if(psmss->sample->pkt_builder->es_desc->dc != NULL)
                        SAFE_FREE(psmss->sample->pkt_builder->es_desc->dc);
		    if(psmss->sample->pkt_builder->es_desc != NULL)
			SAFE_FREE(psmss->sample->pkt_builder->es_desc);
		    if(psmss->sample->pkt_builder->nal != NULL)
			SAFE_FREE(psmss->sample->pkt_builder->nal);
#else

		    cleanup_mpeg4_packet_builder(psmss->sample->pkt_builder);
#endif
		    }
		    cleanup_parser_state(psmss);
		    //cleanup_mp4_unhinted_parser(st);
		}

		initSubsessionState(psmss, 5000, psmss->trackType);
		psmss->temp_rtp_time = 1;
		//psmss->rtpTime = psmss->lastTimeStamp;
		*rtpSeqNum = psmss->sequenceNumber;
		*rtpTimestamp = getNTPTime(psmss, 0);
		if (psmss->trackType == VIDEO_TRAK) {
			psmss->ssrc = 0x6952;
		} else {
			psmss->ssrc = 0x7013;
		}

#ifdef _USE_INTERFACE_
		seek2time(psmss->st_all_info, psmss->seek->seek_time,psmss->trackType,nkn_vpe_buffered_read,psmss->fd);
#else//else of _USE_INTERFACE_
		send_next_pkt(psmss->si, nkn_vpe_buffered_read, psmss->fd, psmss->stbl, &psmss->sctx, psmss->videoPkts, psmss->audioPkts, psmss->trackType, _NKN_SEEK_, psmss->seek);
#endif//end of _USE_INTERFACE_
		if(!isHintTrackPresent(psmss)){
			if(psmss->trackType == VIDEO_TRAK){
				psmss->sample->sample_time = psmss->seek->seek_time*psmss->clkFreq;//psmss->st_all_info->sample_prop_info->vid_s->look_ahead_time;
				//				printf("Seek time = %f\n Sample Time = %d\n",psmss->sample->sample_time/2400.0,psmss->seek->seek_time);
			}
			else{
				if(psmss->sample->pkt_builder->nal != NULL)
					psmss->sample->pkt_builder->nal->DTS = psmss->seek->seek_time*psmss->clkFreq+1;//psmss->sample->clock;// psmss->st_all_info->sample_prop_info->aud_s->look_ahead_time;
			}
		}
		psmss->ftaskToken = scheduleDelayedTask(psmss->taskInterval, (TaskFunc*)psmss->cbFn, p);
	} 

#ifdef PRINT_STATUS
	printf("mp4RTP::startStream - sendNext\n");
#endif

	pthread_mutex_unlock(&psmss->mp4_mutex);
	return 0;
}

extern "C" int imp_pauseStream(void * p, 
				unsigned clientSessionId,
				void* streamToken) 
{
	mp4RTP *psmss = (mp4RTP *)p;

	if(!psmss) {
		return -1;
	}

	pthread_mutex_lock(&psmss->mp4_mutex);
	gettimeofday(&psmss->opStart, NULL);
	psmss->ps = PAUSE;
	unscheduleDelayedTask(psmss->ftaskToken);
	psmss->ftaskToken = (TaskToken)-1;
	psmss->seek->seek_time = int32_t(psmss->lastDTS/1e6);

#ifdef PRINT_STATUS
	printf("mp4RTP::pauseStream\n");
#endif
	pthread_mutex_unlock(&psmss->mp4_mutex);
	return 0;
}



#if 0
extern "C" void imp_seekStream(void * p, 
			       unsigned clientSessionId,
			       void* streamToken, 
			       double seekNPT) 
{
	mp4RTP *psmss = (mp4RTP *)p;
	pthread_mutex_lock(&psmss->mp4_mutex);

	if(!psmss) {
		unscheduleDelayedTask(psmss->ftaskToken);
		psmss->ftaskToken = (TaskToken)-1;
		//      pthread_mutex_unlock(&parser_lock);
		pthread_mutex_unlock(&psmss->mp4_mutex);
		return;
	}

	if(ceil(seekNPT) != 0) { 
		psmss->ps = SEEK;
		psmss->seek->seek_time = (int32_t)(seekNPT);
		//psmss->minSubsessionDuration = seekNPT;
	}

#ifdef PRINT_STATUS
	printf("mp4rtp::seekStream not supported !!!\n");
#endif
	pthread_mutex_unlock(&psmss->mp4_mutex);

	return;
}


#else
extern "C" int imp_seekStream(void * p,
                               unsigned clientSessionId,
                               void* streamToken,
                               double rangeStart, double rangeEnd)
{
	mp4RTP *psmss = (mp4RTP *)p;

	if(!psmss) {
		return -1;
	}
	
	pthread_mutex_lock(&psmss->mp4_mutex);

	psmss->play_end_time = rangeEnd;

	if(ceil(rangeStart) != 0 || (rangeStart == 0 && psmss->lastDTS!=0)) {
		psmss->ps = SEEK;
		psmss->seek->seek_time = (int32_t)(rangeStart);
		//psmss->minSubsessionDuration = seekNPT;
	}

#ifdef PRINT_STATUS
	printf("mp4rtp::seekStream not supported !!!\n");
#endif
	pthread_mutex_unlock(&psmss->mp4_mutex);

	return 0;
}


#endif



extern "C" int imp_setStreamScale(void * p, 
				   unsigned clientSessionId,
				   void* streamToken, 
				   float scale) 
{
	 mp4RTP *psmss = (mp4RTP *)p;
#ifdef PRINT_STATUS
	printf("mp4rtp: setStreamScale %f\n", scale);
#endif

	if(!psmss) {
		return -1;
	}

	pthread_mutex_lock(&psmss->mp4_mutex);
	if ((psmss->trickMode) && (scale != 1)) {
	#if 1
	      if(psmss->ps == PAUSE) {
		/* BZ: 5388
		 * if already in PAUSE, then we need to 
		 * resume playback; setting the TRICKMODE
		 * playstate will not resume playback
		 */
		  psmss->trickMode = 2;
	    }
	#endif
	    psmss->ps = TRICKMODE;
	}
	//	psmss->seek->seek_time = (int32_t) (psmss->lastDTS/1e6); //convert from us to seconds
	pthread_mutex_unlock(&psmss->mp4_mutex);

	return 0;
}

extern "C" void * imp_deleteStream(void * p, 
				   unsigned clientSessionId,
				   void* streamToken) 
{
	mp4RTP * psmss = (mp4RTP *)p;
	pthread_mutex_t lock;

	if(!psmss) {
	    printf("should have never come here\n");
		return NULL;
	}


#ifdef PRINT_STATUS
	printf("mp4RTP::deleteStream\n");
#endif

	pthread_mutex_lock(&psmss->mp4_mutex);
	if(psmss->fd) {
	    if(psmss->ftaskToken != (TaskToken) -1) {
		/* this means that there is a task pending and we __should__ not clear the 
		 * context here; instruct the cleanup function to mark the context as DEAD
		 * and ensure cleanup is done in the pending task
		 */
		
		/* mark the Task Token as -1 to instruct the cleanup function to just mark 
		 * the context as DEAD and not free any resource; the actual free will be done
		 * in the pending call
		 */
		psmss->ftaskToken = (TaskToken) -1;
		cleanupSessionState(psmss);
		pthread_mutex_unlock(&psmss->mp4_mutex);	
	    } else {
		/* No task pending can proceed with normal cleanup without risk of reusing the 
		 * context
		 */
		/* set the task token to '0' for normal cleanup */
		psmss->ftaskToken = (TaskToken) 0;
		
		/* store the lock in a temporary variable __only__
		 * after acquiring the lock; else the temp variable will 
		 *be storing only the previous state
		 */
		lock = psmss->mp4_mutex;
		cleanupSessionState(psmss); 
		
		/* unlock using the temporary variable
		 */
		pthread_mutex_unlock(&lock);
	    } 
	} 
	return NULL; 
}

extern "C" float imp_testScaleFactor(void * p, float scale) {
	mp4RTP * psmss = (mp4RTP *)p;

	if(!psmss) {
		return 1;
	}


	pthread_mutex_lock(&psmss->mp4_mutex);
	//	scale = (float)1; // Only scale of 1 is supported for now
	pthread_mutex_unlock(&psmss->mp4_mutex);
	return scale;
}

extern "C" float imp_duration(void * p) {
	mp4RTP * psmss = (mp4RTP *)p;

	if(!psmss) {
		return 0;
	}

	pthread_mutex_lock(&psmss->mp4_mutex);
	//set the duration
#ifdef _USE_INTERFACE_
	psmss->fDuration = psmss->st_all_info->stbl_info->duration;
#else
	psmss->fDuration = psmss->stbl->duration;
#endif
	pthread_mutex_unlock(&psmss->mp4_mutex);
	return psmss->fDuration;
}

extern "C" float imp_getPlayTime(void * p, unsigned clientSessionId,
				 void* streamToken) 
{
	mp4RTP * psmss = (mp4RTP *)p;

	if(psmss->ps == SEEK)
		return (psmss->seek->seek_time);
	else
		return (psmss->lastDTS/1e6); //convert from us to seconds
}


extern "C" uint32_t imp_ServerAddressForSDP(void * p) {
	mp4RTP * psmss = (mp4RTP *)p;

	return psmss->fServerAddressForSDP;
}

extern "C" char const* imp_rangeSDPLine(void * p) {
	mp4RTP *psmss = (mp4RTP *)p;
	float duration;

	if(!psmss) {
		return NULL;
	}

	pthread_mutex_lock(&psmss->mp4_mutex);
	if (psmss->maxSubsessionDuration != psmss->minSubsessionDuration) {
		duration = -(psmss->maxSubsessionDuration); // because subsession durations differ
	} else {
		duration = psmss->maxSubsessionDuration; // all subsession durations are the same
	}

	// If all of our parent's subsessions have the same duration
	// (as indicated by "fParentSession->duration() >= 0"), there's no "a=range:" line:
	//if (duration >= 0.0) return strdup("");

	// Use our own duration for a "a=range:" line:
	pthread_mutex_unlock(&psmss->mp4_mutex);
	float ourDuration = imp_duration(psmss);
	if (ourDuration == 0.0) {
		return strdup("a=range:npt=0-\r\n");
	} else {
		char buf[100];
		snprintf(buf, 100, "a=range:npt=0-%.3f\r\n", ourDuration);
		return strdup(buf);
	}
}
extern "C" int imp_setTrackNumer(void * p, float maxSubsessionDuration, 
				  float minSubsessionDuration, unsigned TrackNumber) 
{
	mp4RTP *psmss = (mp4RTP *)p;
	if(!psmss) {
		return -1;
	}

	pthread_mutex_lock(&psmss->mp4_mutex);
	psmss->maxSubsessionDuration = maxSubsessionDuration;
	psmss->minSubsessionDuration = minSubsessionDuration;
	//if(TrackNumber == 1) {
	snprintf(psmss->fTrackId, strlen(psmss->trackID[0])+10, "trackID=%s", psmss->trackID[0]/*TrackNumber*/);
	//    } else {
	//snprintf(psmss->fTrackId, (psmss->fTrackId), "trackID=%d", 4/*TrackNumber*/);
	//}
	pthread_mutex_unlock(&psmss->mp4_mutex);
	return 0;
}

extern "C" nkn_provider_type_t imp_getLastProviderType(void *p)
{

    mp4RTP *psmss = (mp4RTP*)p;

    return nkn_vfs_get_last_provider(psmss->fd);
}
/* *****************************************************
 * Initialization Functions
 ******************************************************* */
/* Create sessions */
static struct rtsp_so_exp_funcs_t soexpfuncs = {
	imp_trackId,
	imp_sdpLines,
	imp_setTrackNumer,

	imp_getStreamParameters,
	imp_startStream,
	imp_pauseStream,
	imp_seekStream,
	imp_setStreamScale,
	imp_deleteStream,
	imp_testScaleFactor,
	imp_duration,
	imp_getPlayTime,
	imp_getLastProviderType,
	createNew_mp4RTP
};

extern void init_vfs_init(struct vfs_funcs_t * pfunc);
extern "C" struct rtsp_so_exp_funcs_t * nkn_vfs_init(struct vfs_funcs_t * func);
extern "C" struct rtsp_so_exp_funcs_t * nkn_vfs_init(struct vfs_funcs_t * func)
{
	init_vfs_init(func);

	return &soexpfuncs;
}


static inline void addRtpTcphdr(uint8_t* buf, uint8_t channelId, uint16_t len) {
	// RTP/TCP related headers
	buf[0] = '$';
	buf[1] = channelId; 
	uint16_t tmpLen = htons(len);
	memcpy(buf+2, &tmpLen, 2);
}

