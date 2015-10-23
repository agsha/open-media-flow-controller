#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <fcntl.h>
#include <assert.h>
#include <pthread.h>
#include <errno.h>
#include <netinet/in.h>

#include "rtsp_func.h"
#include "nkn_vfs.h"
#include "rtsp_vpe_nkn_fmt.h"
#include "nkn_vpe_mp4_feature.h"
#include "nkn_vpe_types.h"
#include "nkn_vpe_rtp_packetizer.h"
#include "rtp_packetizer_common.h"
#include "iov_buf_state.h"

//static pthread_mutex_t psmss->cache_mutex = PTHREAD_MUTEX_INITIALIZER;

#if 0
#define FIXED_HEADER  8
#define SDP_SIZE_BYTES 4
#define SDP_SIZE_OFFSET FIXED_HEADER
#define SDP_DATA_OFFSET FIXED_HEADER+SDP_SIZE_BYTES
#define METADATA_SIZE_BYTES 4
#define META_DATA_FORMAT 13
#endif // 0
#define CHANNEL_ID_BYTES 1
#define RTP_PKT_SIZE_BYTES 4
#define SEEK_BOX_END_TIME_BYTES 4
#define SEEK_BOX_OFFSET_BYTES 4
#define FIXED_SEEK_TIME 10

#define RTP_PKT_TIMESTAMP_OFFSET 4

#pragma pack(push, 1)

#define ACF_MAGIC_DEAD 0xdeaddead
#define ACF_MAGIC_ALIVE 0x11111111

//static pthread_mutex_t parser_lock;
enum playState { 
	PLAY, /* NORMAL PLAYBACK */
	PAUSE, /* PAUSES DELIVERY, retains state information on resume */
	TRICKMODE, /* FF/RW */
	SEEK, /*SEEK TO */
	STOP /* PAUSES DELIVERY, resets state information on resume */
};

enum RTP_type {
	RTP_TCP,
	RTP_UDP
};
/*  Specific state */
typedef struct RTP_cache_t
{
	int32_t taskInterval;
	TaskToken ftaskToken;
	/* Ankeena format header */
	rvnf_header_t rvnfh;
	char * sdp_string;

	/* Seek Box */
	rvnf_seek_box_t cur_seek_box;

        //int32_t sdp_size;
        //int32_t meta_data_size;
        //int32_t meta_data_offset;
        //int32_t num_channels;
	int32_t channel_id;
        int32_t pkt_number;
        int32_t curr_pkt_offset;
        int32_t curr_rtp_pkt_size;
        uint8_t *curr_buf;
        uint32_t curr_pkt_rtp_time_stamp;
        int32_t next_pkt_offset;
        int32_t next_pkt_rtp_pkt_size;
        uint8_t *next_pkt_buf;
        int32_t next_pkt_rtp_time_stamp;
	int32_t last_seek_box_offset;
	int32_t last_seek_box_end_time;
	int32_t last_pkt_pos;
	int32_t next_bound_pos;
	int32_t last_pos;
	int32_t last_time_stamp;
	int32_t curr_time_stamp;
	int32_t look_ahead_time;
	int32_t seek_box_number ;
	int32_t last_bound_pos;
	int32_t clk_freq;
	int32_t last_pkt_rtp_time_stamp;
	pthread_mutex_t cache_mutex;
	int rtsp_fd;
        uint32_t magic;
	enum RTP_type rtp_type;
	float fDuration;
	//int ftcpSocketNum;
	FILE * p_file;
	char * fFileName;
   
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
	Sample_rtp_info_t *sample;
	Seek_params_t *seek;
	uint32_t rtpTime;
	uint32_t firstRTPTimestamp;
	uint32_t lastTimeStamp;
	uint32_t firstTimeStamp;
	int32_t numSamples;
	double nextDTS;

	/* payload info */
	void *p_video;
	void *p_audio;
	void *other;

	/* time sync information*/
	struct timeval currPktTime;
	struct timeval lastPktTime;
	struct timeval opStart;
	struct timeval opEnd;
	struct timeval ref_time;
	uint64_t timeElapsed;
	double lastDTS;
	float seek_time;

	/*scheduled callback function */
	int (*cbFn) (void *);
    
	/*Protocol info */
	float maxSubsessionDuration;
	float minSubsessionDuration;
	//char fTrackId[32];
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
	
	iov_buf_state_t* prev_iov_state;
        double play_end_time;
} RTP_cache;

#pragma pack(pop)
/************************PROTOTYPES********************/
extern "C" void * acf_deleteStream(void * p, unsigned clientSessionId, void* streamToken);
extern "C" int acf_setStreamScale(void * p, unsigned clientSessionId, void* streamToken, float scale);
extern "C" float acf_testScaleFactor(void * p, float scale);
extern "C" char const* acf_trackId(void * p);

//void read_rtp_cache_format_info(RTP_cache* psmss,size_t (*nkn_fread)( void *, size_t, size_t, FILE*), FILE *fid);
static int acf_sendNext_RTPCacheFormat(void *firstArg);
//send_RTPCache_pkt(psmss,channel_id,sample);
int32_t acf_send_RTPCache_pkt(RTP_cache_t* psmss,int32_t channel_id,Sample_rtp_info_t *rtp_sample);
int32_t acf_seek2time_cache(RTP_cache_t* psmss, uint32_t seek_time,int32_t channel_id);
uint32_t acf_getNTPTime(RTP_cache *st, int32_t timestamp);
static int32_t acf_isHintTrackPresent(RTP_cache *psmss);
int32_t acf_setupRTPTime(RTP_cache *st);
RTP_cache* acf_createSubsessionState(char *dataFileName, int32_t sockfd, uint32_t if_addr, char *);
int32_t acf_initSubsessionState(RTP_cache* psmss, int32_t taskInterval, int32_t trackType);

static inline void acf_buildRTPPacket(RTP_cache *st, RTP_pkt_t *pkt, uint8_t *hdr);
static inline int32_t acf_timeval_subtract (struct timeval *x, struct timeval *y, struct timeval *result);
static inline void acf_cleanupSessionState(RTP_cache *st);
static inline void acf_freeAndNULL(void *ptr);
static int acf_get_first_rtp_info(RTP_cache *psmss);

//void setRefClk(RTP_cache *psmss);
#define MP4_MAGIC_DEAD 0xdeaddead
/* constants */
//#define TASK_INTERVAL 30000
#define RTP_PACKET_SIZE 12

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

////////// RTP_cache //////////
//#define DM2_ENABLED

/**************************************************************************************************
 *                               UTILITY FUNCTIONS
 *************************************************************************************************/



/*convert to NTP timestamp
 */
uint32_t
acf_getNTPTime(RTP_cache *st, int32_t timestamp)
{
	//static struct timeval tv;
	//int64_t rtptime;
	//double dts;
    
	//tv = *st->rtpTime;
	//dts = (timestamp * 1000)/st->clkFreq;//microseconds
	//rtptime = ((tv.tv_sec) * st->clkFreq * 1000)%(INT_MAX); //ticks
	return ((st->rtpTime + st->firstRTPTimestamp + timestamp));
    
}


/* generate a sync time
 */
int32_t
acf_setupRTPTime(RTP_cache *st) 
{
	struct timeval result;

	//srand(st->seed);
	st->seed++;
	st->rtpTime = st->lastTimeStamp;//st->seed;
	acf_timeval_subtract(&st->opEnd, &st->opStart, &result);
	st->firstRTPTimestamp = 0; //(uint32_t)random();
	//st->rtpTime = st->firstRTPTimestamp + (result.tv_sec * st->clkFreq);

	//st->rtpTime->tv_sec = rand();//st->rtpTime->tv_sec + 0x83AA7E80;
	//st->rtpTime->tv_usec = rand();

	//st->rtpTime = ( t.tv_sec * st->clkFreq ) + st->lastTimeStamp;
	return 0;
}

/*
 * Creates a new sub-session state
 */
RTP_cache *
acf_createSubsessionState(char *dataFileName, int32_t sockfd, uint32_t if_addr,
			  char *vfs_open_mode)
{
	RTP_cache *psmss;
	send_ctx_t *ctx;
	FILE *fd;
	timeval tv;
	int32_t ret_val;
	char filename[256];

	psmss = NULL;
	ctx = NULL;

	/*allocate memory for the RTP_cache object */
	psmss =(RTP_cache *)malloc(sizeof(RTP_cache));
	if(!psmss) {
		return NULL;
	}

	memset((char *)psmss, 0, sizeof(RTP_cache));
	psmss->fFileName = strdup(dataFileName);
	psmss->rtsp_fd = sockfd;
	psmss->magic = ACF_MAGIC_ALIVE;
	//init with PLAY state
	psmss->ps = PLAY;
    	psmss->trickMode = 0;

	pthread_mutex_init (&psmss->cache_mutex, NULL);//by suma
	/*init the parser*/
	snprintf(filename, sizeof(filename), "%s"ANKEENA_FMT_EXT, dataFileName);
	fd = nkn_vfs_fopen(filename, vfs_open_mode);
	if (!fd) return NULL;
	psmss->p_file = fd;    
       
	psmss->local_port = (random() & 0x0fffe);
	if (psmss->local_port <= 1024) {
		psmss->local_port += 1026;
	}
	psmss->local_ip = if_addr;
	psmss->remote_port = 0;
	psmss->remote_ip = 0;
	psmss->udpsocket = -1;
	psmss->ftaskToken = (TaskToken)-1;

	gettimeofday(&tv, NULL);
	psmss->seed = (uint32_t)tv.tv_usec;//needs to be passed by RTSP layer
	psmss->firstRTPTimestamp = 0; //psmss->seed;
	psmss->sequenceNumber = (psmss->seed & 0x0ffff);
	psmss->seek = (Seek_params_t*)calloc(1, sizeof(Seek_params_t));

	/*
	 * Read Ankeena Format header.
	 */
	nkn_vfs_fseek(psmss->p_file, 0, SEEK_SET);
	nkn_vpe_buffered_read((char *)&psmss->rvnfh, rvnf_header_s, 1, psmss->p_file);

	/*
	 * Sanity Check if the cached version is right.
	 */
	if (strncmp(psmss->rvnfh.magic, MAGIC_STRING, strlen(MAGIC_STRING)!=0) ||
	     (psmss->rvnfh.version!=RVNFH_VERSION) ) {
		nkn_vfs_fclose(fd);
		free(psmss);
		return NULL;
	}

	psmss->sdp_string = (char*)malloc(sizeof(char) * (psmss->rvnfh.sdp_size + 1));
	nkn_vpe_buffered_read(psmss->sdp_string, psmss->rvnfh.sdp_size, 1, psmss->p_file);
	psmss->sdp_string[psmss->rvnfh.sdp_size] = 0;

	psmss->fDuration = atof(psmss->rvnfh.sdp_info.npt_stop) - atof(psmss->rvnfh.sdp_info.npt_start);
	if (psmss->fDuration == 0) {
		long cur_pos;
		char *p;

		cur_pos = nkn_vfs_ftell(psmss->p_file);
		nkn_vfs_fseek(psmss->p_file, rvnf_seek_box_s, SEEK_END);
		nkn_vpe_buffered_read((char *)&psmss->cur_seek_box, rvnf_seek_box_s, 1, psmss->p_file);
		psmss->fDuration = psmss->cur_seek_box.end_time / 1000.0;
		nkn_vfs_fseek(psmss->p_file, cur_pos, SEEK_SET);

		/* Update sdp with range info
		 */
		snprintf(filename, 256,  "a=range:npt=0.000-%.3f", psmss->fDuration);
		p = strstr(psmss->sdp_string, "a=range:npt=now-");
		if (p) {
			char *psdp;
			int len, len1;

			len1 = strlen(filename);
			psdp = (char*)malloc(sizeof(char) * (psmss->rvnfh.sdp_size + 1 + len1));
			len = p - psmss->sdp_string;
			strncpy(psdp, psmss->sdp_string, len);
			strncpy(psdp + len, filename, len1);
			strncpy(psdp + len + len1, p + 16, psmss->rvnfh.sdp_size - len - 16);
			*(psdp + len + len1 + (psmss->rvnfh.sdp_size - len - 16)) = 0;
			free(psmss->sdp_string);
			psmss->sdp_string = psdp;
		}
	}

	psmss->cur_seek_box.end_time = 0;
	psmss->cur_seek_box.this_box_data_size = 0;

	psmss->prev_iov_state = createIovBufState();

	return psmss;
}


/*
 * initializes a new subsession state
 */
int32_t
acf_initSubsessionState(RTP_cache* psmss, int32_t taskInterval, int32_t trackType)
{
	acf_setupRTPTime(psmss);
	//psmss->lastTimeStamp = psmss->rtpTime;

	//seek2time(psmss->st_all_info, 0, trackType, nkn_vpe_buffered_read, psmss->p_file);

	psmss->taskInterval = 30000;//int32_t(psmss->numSamples / psmss->fDuration);
	psmss->nextDTS = 0;
	psmss->look_ahead_time = 0;
	psmss->last_time_stamp = 0;
	psmss->curr_time_stamp = 0;
	psmss->trackType = trackType;

    	psmss->play_end_time = psmss->fDuration;
	
	psmss->cbFn = acf_sendNext_RTPCacheFormat;//by suma

	//setRefClk(psmss);
	return 0;
}

static inline void acf_freeAndNULL(void *ptr)
{
	if(ptr) {
		free(ptr);
		ptr = NULL;
	}
}

void acf_cleanupSessionState(RTP_cache *st) 
{
	if((st->ftaskToken == (TaskToken)-1) &&
		(st->magic == ACF_MAGIC_ALIVE)) {
		st->magic = ACF_MAGIC_DEAD;
		return;
	}

	st->ftaskToken == (TaskToken)-1;
	
	//close the file handle
	if (st->p_file)
		nkn_vfs_fclose(st->p_file);
	st->p_file = NULL;

	//close the socket descriptors created in the subsession
	//namely - (a) RTP (b) RTCP
	//close(st->udpsocket_rtcp);
	if (st->udpsocket != -1) {
		close(st->udpsocket);
		st->udpsocket = -1;
	}

	//free the file name buf
	acf_freeAndNULL(st->fFileName);
	acf_freeAndNULL(st->sdp_string);
	acf_freeAndNULL(st->seek);

	/* cleanup any partially sent data and the resources required
	 * to store them
	 */
	deleteIovBufState(st->prev_iov_state);

	//finally free the state
	acf_freeAndNULL(st);
	
}

/* computes the difference between two timeval structures
 */
int32_t
acf_timeval_subtract (struct timeval *x, struct timeval *y, struct timeval *result)
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

extern "C" Boolean 
acf_createNew_RTP_cache (char * dataFileName, int sockfd, uint32_t if_addr, void ** pvideo, void ** paudio)
{
	RTP_cache * psmss;

	*pvideo = NULL;
	*paudio = NULL;

	/*
	 * Assume that we have at least one track.
	 */
	psmss = acf_createSubsessionState(dataFileName, sockfd, if_addr, (char *)"rb");
	if(!psmss) {
		return false;
	}
	acf_initSubsessionState(psmss, 5000, VIDEO_TRAK);
	if (psmss->rvnfh.sdp_info.track[0].sample_rate)
		psmss->clk_freq = psmss->rvnfh.sdp_info.track[0].sample_rate;
	else
		psmss->clk_freq = 90000;
	//psmss->firstRTPTimestamp = (uint32_t)random(); //psmss->rtpTime;
	acf_get_first_rtp_info(psmss);
	*pvideo = psmss;

	if (psmss->rvnfh.sdp_info.num_trackID > 1) {
                psmss = acf_createSubsessionState(dataFileName, sockfd, if_addr, (char *)"J");
		if(!psmss) {
			return false;
		}
		acf_initSubsessionState(psmss, 5000, AUDIO_TRAK);
		if (psmss->rvnfh.sdp_info.track[1].sample_rate)
			psmss->clk_freq = psmss->rvnfh.sdp_info.track[1].sample_rate;
		else
			psmss->clk_freq = 32000;
		acf_get_first_rtp_info(psmss);
		//psmss->firstRTPTimestamp = (uint32_t)random(); //psmss->rtpTime;
		*paudio = psmss;
	}

	return true;
}


extern "C" char const* acf_trackId(void * p) 
{
	RTP_cache * psmss = (RTP_cache *)p;
	return psmss->rvnfh.sdp_info.track[psmss->trackType].trackID;
}


//#define AUDIO_ONLY
/* builds the SDP information. At the moment strips out the Audio track
 */
extern "C" char const* acf_sdpLines(void * p) 
{
	RTP_cache * psmss = (RTP_cache *)p;
	return strdup(psmss->sdp_string);
#if 0
        char buf[100];
        struct in_addr in;

        sprintf(buf, 
		"m=%s\n"
		"c=IN IP4 0.0.0.0\n"
		"b=%s\r\n"
		"a=control:%s\n",
		psmss->rvnfh.sdp_info.track[psmss->trackType].m_string,
		psmss->rvnfh.sdp_info.track[psmss->trackType].b_string,
		psmss->rvnfh.sdp_info.track[psmss->trackType].trackID);
        return strdup(buf);
#endif // 0
}

/* 
 * Sets up the following
 * a. RTP/RTCP server ports by binding to any two successive port pairs. 
 *    this information is returned via the DESCRIBE reply
 * b. Stores the RTCP and RTP Client ports for sending information to the client
 * c. creates UDP sockets for RTP and RTCP
*/
extern "C" void * acf_getStreamParameters(void * p,
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
	RTP_cache * psmss = (RTP_cache *)p;

	if(!psmss) {
		return NULL;
	}
	pthread_mutex_lock(&psmss->cache_mutex);

#ifdef PRINT_STATUS
	printf("cachertp: getStreamParameters\n");
#endif

	if ((psmss->udpsocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1) {
#ifdef PRINT_STATUS
		printf("rtsp_init_udp_socket: failed\n");
#endif
		return 0;
	}

	memset((char *) &si_me, 0, sizeof(si_me));

	psmss->remote_ip = *destinationAddress = clientAddress;
	psmss->remote_port = *clientRTPPort;
	*clientRTCPPort = htons(ntohs(*clientRTPPort) + 1);

again:
	si_me.sin_family = AF_INET;
	si_me.sin_port = htons(psmss->local_port);
	si_me.sin_addr.s_addr = psmss->local_ip;
	if (bind(psmss->udpsocket, (struct sockaddr *)&si_me, sizeof(si_me))==-1) {
		psmss->local_port=psmss->local_port + 2;
		goto again;
	}

#ifdef PRINT_STATUS
	printf("rtsp_init_udp_socket udp listen on port=%d\n", psmss->local_port);
	printf("rtsp_init_udp_socket clientRTPPort=%d, destinAddress=0x%x\n", ntohs(*clientRTPPort), *destinationAddress);
#endif

	/* 
	 * RTCP port is opened in common RTCP module 
	 * --Zubair/Max
	 */

	*destinationTTL = 0;
	*isMulticast    = False;
	*serverRTPPort  = htons(psmss->local_port);
	*serverRTCPPort = htons(psmss->local_port+1);

	if (tcpSocketNum != -1) {
		/* RTP/TCP */
		/* MUST: tcpSocketNum  == psmss->rtsp_fd */
		psmss->rtp_type = RTP_TCP;
	}
	else {
		psmss->rtp_type = RTP_UDP;
	}

	if (psmss->ssrc) {
		*ssrc = psmss->ssrc;
	}
	else {
		if (psmss->trackType == VIDEO_TRAK) {
			psmss->ssrc = *ssrc = 0x6952;
		} else {
			psmss->ssrc = *ssrc = 0x7013;
		}
	}

	pthread_mutex_unlock(&psmss->cache_mutex);
	return 0; //psmss->streamToken;
}


static int acf_sendNext_RTPCacheFormat(void *firstArg)
{
	RTP_cache *psmss = (RTP_cache *)firstArg;
	char c_buf[2048];
	struct sockaddr_in si_other;
	int slen, ret;
	int data_len, track;
	long cur_pos;
	struct timeval cur_time;
	uint32_t time_elapsed;
	int err = 0;
	uint32_t rtp_time;
	
	slen = sizeof(struct sockaddr_in);
	pthread_mutex_lock(&psmss->cache_mutex);
	/* 
	 * If file has already been closed 
	 */
	if (psmss->p_file == NULL) {
		pthread_mutex_unlock(&psmss->cache_mutex);
		return -1;
	}

	if(psmss->magic == ACF_MAGIC_DEAD) {
		pthread_mutex_t lock;
		//printf("task already scheduled; delayed free on callback MAGIC is DEAD (fd=%d)\n", psmss->udpsocket);
		//	psmss->ftaskToken = (TaskToken);
		lock = psmss->cache_mutex;
		acf_cleanupSessionState(psmss);
		pthread_mutex_unlock(&lock);
		return -1;
	}
	psmss->ftaskToken = (TaskToken)-1;
	//if(psmss->pkt_number == 0) {
	//    gettimeofday(&psmss->ref_time, NULL);
	//    psmss->curr_pkt_rtp_time_stamp = 0;
	//}
	gettimeofday(&cur_time, NULL);
	time_elapsed = (cur_time.tv_sec * 1000 + cur_time.tv_usec / 1000) - 
			(psmss->ref_time.tv_sec * 1000 + psmss->ref_time.tv_usec / 1000);

	while ((time_elapsed + 100) >= psmss->curr_pkt_rtp_time_stamp) {
		if (psmss->cur_seek_box.this_box_data_size == 0) {
			ret = nkn_vpe_buffered_read((char *)&psmss->cur_seek_box, rvnf_seek_box_s, 1, psmss->p_file);
			if (ret == 0) { err = 6; goto error; }
			if (psmss->cur_seek_box.this_box_data_size == 0) {
				psmss->ps = STOP;
				err = 7; 
				goto error;
			}
		}
		cur_pos = nkn_vfs_ftell(psmss->p_file);
		ret = nkn_vpe_buffered_read(c_buf, 8, 1, psmss->p_file);
		if (ret == 0) { err = 1; goto error; }
		psmss->cur_seek_box.this_box_data_size -= 8;
		if (*c_buf == '$') {
			psmss->channel_id = *(c_buf + 1);
			if (psmss->channel_id != 0 && psmss->channel_id != 1) { err = 2; goto error; }
			psmss->curr_rtp_pkt_size = *((uint16_t *)(c_buf + 2));
			if (psmss->curr_rtp_pkt_size > 1600) { err = 3; goto error; }
			psmss->curr_pkt_rtp_time_stamp = *((uint32_t *)(c_buf + 4));
			ret = nkn_vpe_buffered_read(c_buf + 8, psmss->curr_rtp_pkt_size, 1, psmss->p_file);
			if (ret == 0) { err = 4; goto error; }
			psmss->cur_seek_box.this_box_data_size -= psmss->curr_rtp_pkt_size;

#if 0
	printf("Send ok %d Time %d, Pos %ld Pkt Hdr %c %d %d %d SBox %d %d\r\n", 
		psmss->trackType, time_elapsed, cur_pos,
		*c_buf,
		psmss->channel_id, psmss->curr_rtp_pkt_size,
		psmss->curr_pkt_rtp_time_stamp,
		psmss->cur_seek_box.end_time,
		psmss->cur_seek_box.this_box_data_size );
#endif

			/* Only sent out when trackType are the same */
			if (psmss->channel_id == psmss->trackType) {
				/* Update sequence number and timestamp. Would need
				 * both if PAUSE and PLAY is given.
				 */
				//psmss->sequenceNumber = ntohs(*((uint16_t *)(c_buf + 8 + 2)));
				psmss->lastTimeStamp  = ntohl(*((uint32_t *)(c_buf + 8 + 4)));
				psmss->ssrc	      = ntohl(*((uint32_t *)(c_buf + 8 + 8)));
				psmss->rtpTime	      = psmss->lastTimeStamp;

				/* Check time, if less than current time, send the packet.
				 */
				gettimeofday(&cur_time, NULL);
				time_elapsed = (cur_time.tv_sec * 1000 + cur_time.tv_usec / 1000) - 
						(psmss->ref_time.tv_sec * 1000 + psmss->ref_time.tv_usec / 1000);
#if 0
				if ((psmss->pkt_number % 10) == 0)
					printf( "Pt=%d Pkt no=%d, time=%d pkt time=%d\r\n", 
						psmss->trackType, psmss->pkt_number,
						time_elapsed, psmss->curr_pkt_rtp_time_stamp );
#endif


				if((psmss->play_end_time*1e3) < psmss->curr_pkt_rtp_time_stamp){
				    //unscheduleDelayedTask(psmss->ftaskToken);
				    psmss->ftaskToken = (TaskToken)-1;
				    //psmss->magic == ACF_MAGIC_DEAD;
				    pthread_mutex_unlock(&psmss->cache_mutex);
				    return 0;
				}


				if ((time_elapsed + 100) < psmss->curr_pkt_rtp_time_stamp) {
					psmss->cur_seek_box.this_box_data_size += 8 + psmss->curr_rtp_pkt_size;
					nkn_vfs_fseek(psmss->p_file, cur_pos, SEEK_SET);
					break;
				}
				/* Rewrite sequence number and RTP time 
				 */
				*((uint16_t *)(c_buf + 8 + 2)) = htons(psmss->sequenceNumber++);
				rtp_time = ntohl(*((uint32_t *)(c_buf + 8 + 4)));
				rtp_time += psmss->firstRTPTimestamp;
				*((uint32_t *)(c_buf + 8 + 4)) = htonl(rtp_time);
				if (psmss->rtp_type == RTP_UDP) {
					memset((char *) &si_other, 0, sizeof(si_other));
					si_other.sin_family = AF_INET;
					si_other.sin_port = psmss->remote_port;
					si_other.sin_addr.s_addr = psmss->remote_ip;
					ret = sendto(psmss->udpsocket, 
						&c_buf[8], 
						psmss->curr_rtp_pkt_size, 
						MSG_DONTWAIT, 
						(struct sockaddr *)&si_other, 
						slen);
					if (ret == -1 && errno == EAGAIN)
						break;
					else
						updateRTSPCounters(ret);
				}
				else /* RTP_TCP */ {
					//c_buf[0] = '$';
					//c_buf[1] = 0;
					//snprintf(&c_buf[2], 2, "%2d", psmss->cur_seek_box.trackType); // channel id
					/* remove the 32 bit time, over write it
					 * with inline hdr.
					 */
					*(c_buf + 4) = *(c_buf + 0);
					*(c_buf + 5) = *(c_buf + 1) * 2;
					*((uint16_t *)(c_buf + 6)) = htons(*((uint16_t *)(c_buf + 2)));

					/* Rewrite sequence number and RTP time 
					 */
					*((uint16_t *)(c_buf + 8 + 2)) = htons(psmss->sequenceNumber++);
					rtp_time = ntohl(*((uint32_t *)(c_buf + 8 + 4)));
					rtp_time += psmss->firstRTPTimestamp;
					*((uint32_t *)(c_buf + 8 + 4)) = htonl(rtp_time);
					
					/* Intf. for keeping state of half sent pkts, uses sendmsg.
						So, packing the data accordingly */
					uint8_t* tcp_buf = (uint8_t*)malloc(2048);
					memcpy(tcp_buf, c_buf+4,psmss->curr_rtp_pkt_size + 4);
					struct iovec* iov = (struct iovec*) calloc(1, sizeof(struct iovec));
					struct msghdr mh;
            				mh.msg_control = NULL;
            				mh.msg_controllen = 0;
            				mh.msg_flags = 0;
            				mh.msg_iov = iov;
            				mh.msg_iovlen = 1;
                 			mh.msg_name = NULL;
                 			mh.msg_namelen = 0;
                 			iov->iov_base = tcp_buf;
                 			iov->iov_len = psmss->curr_rtp_pkt_size + 4;
		                        ret = tcpIovSend(psmss->prev_iov_state, psmss->rtsp_fd, 
							&mh, psmss->curr_rtp_pkt_size + 4, 0x01);
                		        if (ret < 0) {
                                		free(mh.msg_iov->iov_base);
                                		free(iov);
						break;
                        		} else
						updateRTSPCounters(ret);
					/*
					ret = send(psmss->rtsp_fd, c_buf + 4, psmss->curr_rtp_pkt_size + 4, 0);
					if (ret == -1 && errno == EAGAIN)
						break;
					*/
				}
				psmss->pkt_number++;
				psmss->last_pkt_rtp_time_stamp = psmss->curr_pkt_rtp_time_stamp;
			}
			else {
				continue; //break;
			}
			
		}
		else {
			err = 5;
			goto error;
		}
	}
	
	psmss->ftaskToken = scheduleDelayedTask(psmss->taskInterval, (TaskFunc*)psmss->cbFn, firstArg);
	pthread_mutex_unlock(&psmss->cache_mutex);
	return 0;
error:
	/* the task gets automatically deleted and could cause double free if deleted from withing
	 * the callback
	 */
	//unscheduleDelayedTask(psmss->ftaskToken);
	psmss->ftaskToken = (TaskToken)-1;
	//psmss->magic == ACF_MAGIC_DEAD;
	pthread_mutex_unlock(&psmss->cache_mutex);
	return -1;

#if 0
	RTP_cache *psmss = (RTP_cache *)firstArg;
	int32_t i, sret, ret, j, retrycnt, samp, currPktTime;
	uint8_t *p_buf /*static Buffer size since MTU is usually on 1420 bytes*/, *buf;
	struct sockaddr_in si_other;
	int32_t pos;
	int32_t rtp_pkt_size;
	//FILE *fid;
	struct timeval result;
	struct iovec *vec;
#ifdef PERF_DEBUG
	struct timeval selfTimeStart, selfTimeEnd;
	int64_t skew;
#endif
	int slen = sizeof(struct sockaddr_in);
	int seek_flag = 0;
	int32_t num_channels=0;


	//fid = psmss->p_file;
	pthread_mutex_lock(&psmss->cache_mutex);
	if(!psmss || !psmss->p_file) {
		unscheduleDelayedTask(psmss->ftaskToken);
		psmss->ftaskToken = (TaskToken)-1;
		pthread_mutex_unlock(&psmss->cache_mutex);
		return;
	}
	sret = ret = 0;

	if(psmss->pkt_number == 0 || psmss->ps == PAUSE) {
	    gettimeofday(&psmss->lastPktTime, NULL);
	    psmss->ps = PLAY;
	    seek_flag = 0;
	} else if (psmss->ps == SEEK) {
	    seek_flag = 1;
			gettimeofday(&psmss->lastPktTime, NULL);
			psmss->lastPktTime.tv_sec = psmss->lastPktTime.tv_sec + (time_t)(psmss->seek->seek_time)-1;
			psmss->timeElapsed = uint64_t((psmss->seek->seek_time) * 1e6);
			psmss->nextDTS = 0;
	    psmss->ps = PLAY;
	}
	gettimeofday(&psmss->currPktTime, NULL);
	psmss->currPktTime.tv_sec += psmss->seek->seek_time;
	timeval_subtract(&psmss->currPktTime, &psmss->lastPktTime, &result);
	memcpy(&psmss->lastPktTime, &psmss->currPktTime, sizeof(struct timeval));
	psmss->timeElapsed += uint64_t((result.tv_sec*1e6)+(result.tv_usec));
	
	if((psmss->timeElapsed*1000) >= floorl(psmss->nextDTS*1000)) {
	    psmss->sample->pkt_list = NULL;
	    //while(ret != RTP_PKT_READY) {
		send_RTPCache_pkt(psmss,psmss->channel_id,psmss->sample);
		//}

	    int32_t cnt =0;
	    psmss->sample->pkt_list= g_list_last(psmss->sample->pkt_list);

	    while(psmss->sample->pkt_list) {
		vec = (struct iovec*)psmss->sample->pkt_list->data;

		if(ret == NOMORE_SAMPLES) {
		    unscheduleDelayedTask(psmss->ftaskToken);
		    psmss->ftaskToken = (TaskToken)-1;
		    pthread_mutex_unlock(&psmss->cache_mutex);
		    return;
		}
		//push out the data on UDP sockets
		memset((char *) &si_other, 0, sizeof(si_other));
		si_other.sin_family = AF_INET;
		si_other.sin_port = psmss->remote_port;
		si_other.sin_addr.s_addr = psmss->remote_ip;//inet_addr("192.168.10.103");//psmss->remote_ip;
		
		retrycnt = 5;
		buf = (uint8_t*)vec->iov_base;
		sret = vec->iov_len;
		
		currPktTime = _read32(buf,4);
		
		psmss->nextDTS = ((1e6*(float)(psmss->look_ahead_time))/(float)(psmss->clkFreq));
		currPktTime = psmss->rtpTime + (currPktTime - psmss->seek->seek_time*psmss->sample->clock);
		psmss->lastDTS = ((1e6*(float)(currPktTime))/(float)(psmss->sample->clock));
		if((psmss->trackType == AUDIO_TRAK)&&seek_flag)
		    printf("Audio curr = %d prevtime = %d seek = %d\n",currPktTime,psmss->rtpTime,psmss->seek->seek_time);
		else if((psmss->trackType == VIDEO_TRAK)&&seek_flag)
		    printf("video curr = %d prevtime = %d seek = %d\n",currPktTime,psmss->rtpTime,psmss->seek->seek_time);
		buf[4] = currPktTime>>24;buf[5] = (currPktTime>>16)&0xff; buf[6] = (currPktTime>>8)&0xff;buf[7] = currPktTime&0xff;


		psmss->pkt_number++;
		psmss->lastTimeStamp = currPktTime;


		while(sret) {
		    ret = sendto(psmss->udpsocket, buf, sret, MSG_DONTWAIT, (struct sockaddr *)&si_other, slen);
		    if(ret<0) { //if any socket error occurs, drop current packet and go to the next packet
			psmss->ftaskToken = scheduleDelayedTask(psmss->taskInterval, (TaskFunc*)send_RTPCache_pkt, firstArg);
			pthread_mutex_unlock(&psmss->cache_mutex);
			return;
			
		    }
		    sret -= ret;
		    buf += ret;
		}
		
		SAFE_FREE(vec->iov_base);
		SAFE_FREE(vec);
		psmss->sample->pkt_list = g_list_previous(psmss->sample->pkt_list);
	    }
	    g_list_free1(psmss->sample->pkt_list);
	}
	
	
	//reschedule the callback function
	psmss->ftaskToken = scheduleDelayedTask(psmss->taskInterval, (TaskFunc*)psmss->cbFn, firstArg);
	//    pthread_mutex_unlock(&parser_lock);
	pthread_mutex_unlock(&psmss->cache_mutex);
	return;
#endif // 0

}

static int acf_get_first_rtp_info(RTP_cache *psmss)
{
	char c_buf[2048];
	int ret = 1;
	int data_len, track;
	long cur_pos;
	int this_box_data_size;
	int channel_id;
	int curr_rtp_pkt_size;
	rvnf_seek_box_t sb;
	
	pthread_mutex_lock(&psmss->cache_mutex);
	/* 
	 * If file has already been closed 
	 */
	if (psmss->p_file == NULL) {
		pthread_mutex_unlock(&psmss->cache_mutex);
		return 0;
	}

	cur_pos = nkn_vfs_ftell(psmss->p_file);
	this_box_data_size = psmss->cur_seek_box.this_box_data_size;
	while (1) {
		if (this_box_data_size == 0) {
			ret = nkn_vpe_buffered_read((char *)&sb, rvnf_seek_box_s, 1, psmss->p_file);
			if (ret == 0) break;
			this_box_data_size = sb.this_box_data_size;
		}
		ret = nkn_vpe_buffered_read(c_buf, 8, 1, psmss->p_file);
		if (ret == 0) break;
		psmss->curr_pkt_rtp_time_stamp = *((uint32_t *)(c_buf + 4));
		this_box_data_size -= 8;
		if (*c_buf == '$') {
			channel_id = *(c_buf + 1);
			curr_rtp_pkt_size = *((uint16_t *)(c_buf + 2));
			ret = nkn_vpe_buffered_read(c_buf + 8, curr_rtp_pkt_size, 1, psmss->p_file);
			if (ret == 0) break;
			this_box_data_size -= curr_rtp_pkt_size;

			/* Only sent out when trackType are the same */
			if (channel_id == psmss->trackType) {
				//psmss->sequenceNumber = ntohs(*((uint16_t *)(c_buf + 8 + 2)));
				psmss->lastTimeStamp  = ntohl(*((uint32_t *)(c_buf + 8 + 4)));
				psmss->ssrc	      = ntohl(*((uint32_t *)(c_buf + 8 + 8)));
				psmss->rtpTime	      = psmss->lastTimeStamp;
				break;
			}
		}
	}
	
	nkn_vfs_fseek(psmss->p_file, cur_pos, SEEK_SET);
	pthread_mutex_unlock(&psmss->cache_mutex);
	return ret;
}


extern "C" int acf_startStream(void * p,
				unsigned clientSessionId, 
				void* streamToken,
				TaskFunc* rtcpRRHandler,
				void* rtcpRRHandlerClientData,
				unsigned short * rtpSeqNum,
				unsigned * rtpTimestamp) 
{

	RTP_cache *psmss = (RTP_cache *)p;
	uint64_t elapsed_sec;
	uint64_t elapsed_usec;
	int ret;


#ifdef PRINT_STATUS
	printf("start stream\n");
#endif
	psmss = psmss;

	if(!psmss) {
		return -1;
	}
	pthread_mutex_lock(&psmss->cache_mutex);

	if(psmss->ps == TRICKMODE) {
		psmss->ps = PLAY;
		*rtpSeqNum = psmss->sequenceNumber;
		*rtpTimestamp = psmss->lastTimeStamp + psmss->firstRTPTimestamp;
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
		pthread_mutex_unlock(&psmss->cache_mutex);
		return 0;
	}

	if(psmss->ps == PAUSE) {//if the PAUSE request has been correctly recieved and responded to
		*rtpSeqNum = psmss->sequenceNumber;
		*rtpTimestamp = psmss->lastTimeStamp + psmss->firstRTPTimestamp;
		psmss->ftaskToken = scheduleDelayedTask(psmss->taskInterval, (TaskFunc*)psmss->cbFn, p);
		gettimeofday(&psmss->opEnd, NULL);
		//setupRTPTime(psmss);
		elapsed_sec = psmss->opEnd.tv_sec - psmss->opStart.tv_sec; 
		elapsed_usec = psmss->opEnd.tv_usec - psmss->opStart.tv_usec;
		psmss->ref_time.tv_sec += elapsed_sec;
		psmss->ref_time.tv_usec += elapsed_usec;
		if (psmss->ref_time.tv_usec < 0) {
			psmss->ref_time.tv_sec--;
			psmss->ref_time.tv_usec += 1000000;
		}
		else if (psmss->ref_time.tv_usec > 1000000){
			psmss->ref_time.tv_sec++;
			psmss->ref_time.tv_usec -= 1000000;
		}
		psmss->ps = PLAY;
		// we reset the clocks etc in the next subsequent call to sendNext. Also not the playstate is reset to PLAY in 
		// sendNext, which is why we have not handled it here
	
	} else { //seek or play - we need to init the state 

		psmss->trickMode = 1; // state is allowed to be set as tick mode
		if (psmss->trackType == VIDEO_TRAK) {
			acf_initSubsessionState(psmss, 5000, psmss->trackType);
		}
		else {
			acf_initSubsessionState(psmss, 5000, psmss->trackType);
		}
		//psmss->rtpTime = psmss->lastTimeStamp;
#if 0
		if (psmss->trackType == VIDEO_TRAK) {
			psmss->ssrc = 0x6952;
		} else {
			psmss->ssrc = 0x7013;
		}
#endif
		//call corresponding function by suma
		ret = acf_seek2time_cache(psmss, (uint32_t) (psmss->seek_time * 1000), psmss->trackType);
		if (ret == -1) {
			psmss->ps = STOP;
			psmss->ftaskToken = (TaskToken)-1;
			pthread_mutex_unlock(&psmss->cache_mutex);
			return -1;
		}
			
		*rtpSeqNum = psmss->sequenceNumber;
		*rtpTimestamp = acf_getNTPTime(psmss, 0);
		//printf( "Seq Num %d RTP Time %d ", *rtpSeqNum, *rtpTimestamp);

		//seek2time(psmss->st_all_info, psmss->seek->seek_time,psmss->trackType,nkn_vpe_buffered_read,psmss->p_file);
		psmss->ftaskToken = scheduleDelayedTask(psmss->taskInterval, (TaskFunc*)psmss->cbFn, p);
		gettimeofday(&psmss->ref_time, NULL);
		//printf( "Ref Time %ld.%ld ", psmss->ref_time.tv_sec, psmss->ref_time.tv_usec);
		elapsed_sec = (uint32_t) psmss->seek_time; 
		elapsed_usec = ((uint64_t) (psmss->seek_time * 1000000)) % 1000000;
		psmss->ref_time.tv_sec -= elapsed_sec;
		psmss->ref_time.tv_usec -= elapsed_usec;
		if (psmss->ref_time.tv_usec < 0) {
			psmss->ref_time.tv_sec--;
			psmss->ref_time.tv_usec += 1000000;
		}
		else if (psmss->ref_time.tv_usec > 1000000){
			psmss->ref_time.tv_sec++;
			psmss->ref_time.tv_usec -= 1000000;
		}
		psmss->ps = PLAY;
		//printf( "Seek Time %.3f Ref Time %ld.%ld\r\n", psmss->seek_time, psmss->ref_time.tv_sec, psmss->ref_time.tv_usec);
	} 

#ifdef PRINT_STATUS
	printf("RTP_cache::startStream - sendNext\n");
#endif

	pthread_mutex_unlock(&psmss->cache_mutex);
	return 0;
}

extern "C" int acf_pauseStream(void * p, 
				unsigned clientSessionId,
				void* streamToken) 
{
	RTP_cache *psmss = (RTP_cache *)p;

	if(!psmss) {
		return -1;
	}
	pthread_mutex_lock(&psmss->cache_mutex);

	gettimeofday(&psmss->opStart, NULL);
	psmss->ps = PAUSE;
	unscheduleDelayedTask(psmss->ftaskToken);
	psmss->ftaskToken = (TaskToken)-1;
	psmss->seek_time = psmss->curr_pkt_rtp_time_stamp/1000.0;

#ifdef PRINT_STATUS
	printf("RTP_cache::pauseStream\n");
#endif
	pthread_mutex_unlock(&psmss->cache_mutex);
	return 0;
}


extern "C" int acf_seekStream(void * p,
                               unsigned clientSessionId,
                               void* streamToken,
                               double rangeStart, double rangeEnd)
{
	RTP_cache *psmss = (RTP_cache *)p;

	if(!psmss) {
		return -1;
	}
	pthread_mutex_lock(&psmss->cache_mutex);
	psmss->play_end_time = rangeEnd;
	//if(ceil(rangeStart) != 0) {
		psmss->ps = SEEK;
		psmss->seek_time = rangeStart;
	//}

#ifdef PRINT_STATUS
	printf("cachertp::seekStream not supported !!!\n");
#endif
	pthread_mutex_unlock(&psmss->cache_mutex);

	return 0;
}




extern "C" int acf_setStreamScale(void * p, 
				   unsigned clientSessionId,
				   void* streamToken, 
				   float scale) 
{
	 RTP_cache *psmss = (RTP_cache *)p;
#ifdef PRINT_STATUS
	printf("mp4rtp: setStreamScale %f\n", scale);
#endif

	if(!psmss) {
		return -1;
	}

	pthread_mutex_lock(&psmss->cache_mutex);
	if ((psmss->trickMode) && (scale != 1)) {
	    if(psmss->ps == PAUSE) {
		/* BZ: 5388
		 * if already in PAUSE, then we need to 
		 * resume playback; setting the TRICKMODE
		 * playstate will not resume playback
		 */
		  psmss->trickMode = 2;
	    }
	    psmss->ps = TRICKMODE;
	}
	psmss->seek_time = psmss->curr_pkt_rtp_time_stamp/1000.0;
	pthread_mutex_unlock(&psmss->cache_mutex);

	return 0;
}

extern "C" void * acf_deleteStream(void * p, 
				   unsigned clientSessionId,
				   void* streamToken) 
{
	RTP_cache * psmss = (RTP_cache *)p;
	pthread_mutex_t lock;

	if(!psmss) {
		return NULL;
	}
	pthread_mutex_lock(&psmss->cache_mutex);
	if(psmss->p_file) {
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
		acf_cleanupSessionState(psmss);
		pthread_mutex_unlock(&psmss->cache_mutex);	
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
		lock = psmss->cache_mutex;
		acf_cleanupSessionState(psmss); 
		
		/* unlock using the temporary variable
		 */
		pthread_mutex_unlock(&lock);
	    } 
	} 

#ifdef PRINT_STATUS
	printf("RTP_cache::deleteStream\n");
#endif
	return NULL; 
}

extern "C" float acf_testScaleFactor(void * p, float scale) {
	RTP_cache * psmss = (RTP_cache *)p;

	if(!psmss) {
		return 1;
	}

	return scale;
}

extern "C" float acf_duration(void * p) {
	RTP_cache * psmss = (RTP_cache *)p;

	if(!psmss) {
		return 0;
	}

	pthread_mutex_lock(&psmss->cache_mutex);
	//set the duration
	//psmss->fDuration = atof(psmss->rvnfh.sdp_info.npt_stop) - atof(psmss->rvnfh.sdp_info.npt_start);
	pthread_mutex_unlock(&psmss->cache_mutex);
	return psmss->fDuration;
}

extern "C" float acf_getPlayTime(void * p, unsigned clientSessionId,
				 void* streamToken) 
{
	RTP_cache * psmss = (RTP_cache *)p;

	if(psmss->ps == SEEK)
		return (psmss->seek_time);
	else
		return (psmss->curr_pkt_rtp_time_stamp/1000.0); //convert from us to seconds
}


extern "C" uint32_t acf_ServerAddressForSDP(void * p) {
	RTP_cache * psmss = (RTP_cache *)p;

	return psmss->fServerAddressForSDP;
}

extern "C" char const* acf_rangeSDPLine(void * p) {
	RTP_cache *psmss = (RTP_cache *)p;
	float duration;

	if(!psmss) {
		return NULL;
	}

	pthread_mutex_lock(&psmss->cache_mutex);
	if (psmss->maxSubsessionDuration != psmss->minSubsessionDuration) {
		duration = -(psmss->maxSubsessionDuration); // because subsession durations differ
	} else {
		duration = psmss->maxSubsessionDuration; // all subsession durations are the same
	}

	// If all of our parent's subsessions have the same duration
	// (as indicated by "fParentSession->duration() >= 0"), there's no "a=range:" line:
	//if (duration >= 0.0) return strdup("");

	// Use our own duration for a "a=range:" line:
	pthread_mutex_unlock(&psmss->cache_mutex);
	float ourDuration = acf_duration(psmss);
	if (ourDuration == 0.0) {
		return strdup("a=range:npt=0-\r\n");
	} else {
		char buf[100];
		snprintf(buf, 100, "a=range:npt=0-%.3f\r\n", ourDuration);
		return strdup(buf);
	}
}
extern "C" int acf_setTrackNumer(void * p, float maxSubsessionDuration, 
				  float minSubsessionDuration, unsigned TrackNumber) 
{
#if 0
	RTP_cache *psmss = (RTP_cache *)p;
	pthread_mutex_lock(&psmss->cache_mutex);
	if(!psmss) {
		unscheduleDelayedTask(psmss->ftaskToken);
		psmss->ftaskToken = (TaskToken)-1;
		//      pthread_mutex_unlock(&parser_lock);
		pthread_mutex_unlock(&psmss->cache_mutex);
		return;
	}

	psmss->maxSubsessionDuration = maxSubsessionDuration;
	psmss->minSubsessionDuration = minSubsessionDuration;
	//if(TrackNumber == 1) {
	snprintf(psmss->fTrackId, strlen(psmss->trackID[0])+10, 
		"trackID=%s", psmss->trackID[0]/*TrackNumber*/);
	//    } else {
	//snprintf(psmss->fTrackId, (psmss->fTrackId), "trackID=%d", 4/*TrackNumber*/);
	//}
	pthread_mutex_unlock(&psmss->cache_mutex);
#endif // 0
	return 0;
}

extern "C" nkn_provider_type_t acf_getLastProviderType(void *p)
{

    RTP_cache *psmss = (RTP_cache*)p;

    return nkn_vfs_get_last_provider(psmss->p_file);
}

/* *****************************************************
 * Initialization Functions
 ******************************************************* */
/* Create sessions */
static struct rtsp_so_exp_funcs_t soexpfuncs = {
	acf_trackId,
	acf_sdpLines,
	acf_setTrackNumer,

	acf_getStreamParameters,
	acf_startStream,
	acf_pauseStream,
	acf_seekStream,
	acf_setStreamScale,
	acf_deleteStream,
	acf_testScaleFactor,
	acf_duration,
	acf_getPlayTime,
	acf_getLastProviderType, 
	acf_createNew_RTP_cache
};

extern void init_vfs_init(struct vfs_funcs_t * pfunc);
extern "C" struct rtsp_so_exp_funcs_t * nkn_vfs_init(struct vfs_funcs_t * func);
extern "C" struct rtsp_so_exp_funcs_t * nkn_vfs_init(struct vfs_funcs_t * func)
{
	init_vfs_init(func);

	return &soexpfuncs;
}

int32_t acf_send_RTPCache_pkt(RTP_cache_t* psmss,int32_t channel_id,Sample_rtp_info_t *rtp_sample)
{
	int32_t pos=0,temp_curr_time_stamp =0;
	uint8_t *c_buf;
	FILE *fid = psmss->p_file;
	struct iovec *vec;
	int32_t last_seek_box_end_time,last_seek_box_offset,c_channel_id;
	int32_t c_rtp_pkt_size,c_time_stamp,temp_time_stamp;

	last_seek_box_end_time=0;
	last_seek_box_offset = 0;
	c_channel_id =0;
	c_rtp_pkt_size =0;
	c_time_stamp = 0;
	temp_time_stamp=0;

	while(temp_time_stamp == psmss->look_ahead_time)
	{
	    if(psmss->last_pkt_pos <  psmss->next_bound_pos)
		{
		    pos = psmss->last_pkt_pos;
		}
	    else
		{
			psmss->seek_box_number++;
			nkn_vpe_buffered_read(&last_seek_box_end_time,4,psmss->last_pkt_pos,fid);
			pos = psmss->last_pkt_pos + SEEK_BOX_END_TIME_BYTES;
			psmss->last_seek_box_end_time = uint32_byte_swap(last_seek_box_end_time);
			nkn_vpe_buffered_read(&last_seek_box_offset,4,pos,fid);
			psmss->last_seek_box_offset = uint32_byte_swap(last_seek_box_offset);
			psmss->last_bound_pos = psmss->next_bound_pos;
			psmss->last_pkt_pos = pos + SEEK_BOX_OFFSET_BYTES;
			psmss->next_bound_pos = pos + SEEK_BOX_OFFSET_BYTES + psmss->last_seek_box_offset;
			pos = psmss->last_pkt_pos;
		}
		nkn_vpe_buffered_read(&c_channel_id,1,pos,fid);//channel id
		pos+=CHANNEL_ID_BYTES;
		nkn_vpe_buffered_read(&c_rtp_pkt_size,4,pos,fid);
		c_rtp_pkt_size = uint32_byte_swap(c_rtp_pkt_size);
		pos+=RTP_PKT_SIZE_BYTES;

		nkn_vpe_buffered_read((char *)&psmss->cur_seek_box, rvnf_seek_box_s, 1, psmss->p_file);

		c_buf=(uint8_t*)malloc(c_rtp_pkt_size*sizeof(uint8_t));
		nkn_vpe_buffered_read(c_buf, psmss->cur_seek_box.this_box_data_size, 1, psmss->p_file);

		pos += c_rtp_pkt_size;
		c_time_stamp = _read32(c_buf,4);
		if(c_channel_id == channel_id)
		{
			psmss->curr_buf = c_buf;
			psmss->curr_rtp_pkt_size = c_rtp_pkt_size;
			//psmss->curr_time_stamp = c_time_stamp;
			if(c_time_stamp == psmss->look_ahead_time)
			{
				//add function for putting the packet in list by suma
				vec = (struct iovec*)calloc(1, sizeof(struct iovec));
				vec->iov_base = psmss->curr_buf;
				vec->iov_len = (size_t) psmss->curr_rtp_pkt_size;
				rtp_sample->pkt_list = g_list_prepend(rtp_sample->pkt_list, vec);
				temp_time_stamp = c_time_stamp;
			}
		}
		psmss->last_pkt_pos = pos;
	}//while loop
	//psmss->nextDTS = psmss->curr_time_stamp;
	psmss->look_ahead_time =  temp_curr_time_stamp;
	psmss->last_pkt_pos = pos - (CHANNEL_ID_BYTES + RTP_PKT_SIZE_BYTES +psmss->curr_rtp_pkt_size);
	return 0;
}


int32_t acf_seek2time_cache(RTP_cache_t* psmss, uint32_t seek_time,int32_t channel_id)
{
	uint8_t c_buf[2048];
	rvnf_seek_box_t sb;
	long cur_pos;
	int ret;
	int err = 0;

	/*
	 * Read Ankeena Format header.
	 */
	ret = nkn_vfs_fseek(psmss->p_file, rvnf_header_s + psmss->rvnfh.sdp_size, SEEK_SET);
	if (ret < 0) { err = 1; goto error; }

	if (seek_time == 0) {
		ret = nkn_vpe_buffered_read((char *)&psmss->cur_seek_box, rvnf_seek_box_s, 1, psmss->p_file);
		if (ret == 0) { err = 2; goto error; }
		pthread_mutex_unlock(&psmss->cache_mutex);
		ret = acf_get_first_rtp_info(psmss);
		pthread_mutex_lock(&psmss->cache_mutex);
		if (ret == 0) { err = 3; goto error; }
		return 0;
	}

	sb.end_time = 0;
	sb.this_box_data_size = 0;
	while (seek_time > sb.end_time) {
		ret = nkn_vfs_fseek(psmss->p_file, sb.this_box_data_size, SEEK_CUR);
		if (ret < 0) { err = 4; goto error; }
		cur_pos = nkn_vfs_ftell(psmss->p_file);
		ret = nkn_vpe_buffered_read((char *)&sb, rvnf_seek_box_s, 1, psmss->p_file);
		if (ret == 0) { err = 5; goto error; }
	}

	nkn_vpe_buffered_read(c_buf, 8, 1, psmss->p_file);
	if (*c_buf == '$') {
		psmss->channel_id = *(c_buf + 1);
		psmss->curr_rtp_pkt_size = *((uint16_t *)(c_buf + 2));
		psmss->curr_pkt_rtp_time_stamp = *((uint32_t *)(c_buf + 4));
	}
	else {
		err = 6; 
		goto error;
	}
	psmss->cur_seek_box.end_time = 0;
	psmss->cur_seek_box.this_box_data_size = 0;
	
	ret = nkn_vfs_fseek(psmss->p_file, cur_pos, SEEK_SET);
	if (ret < 0) { err = 7; goto error; }
	while (seek_time > psmss->curr_pkt_rtp_time_stamp) {
		if (psmss->cur_seek_box.this_box_data_size == 0) {
			ret = nkn_vpe_buffered_read((char *)&psmss->cur_seek_box, rvnf_seek_box_s, 1, psmss->p_file);
			if (ret == 0) { err = 8; goto error; }
		}
		cur_pos = nkn_vfs_ftell(psmss->p_file);
		ret = nkn_vpe_buffered_read(c_buf, 8, 1, psmss->p_file);
		if (ret == 0) { err = 9; goto error; }
		psmss->cur_seek_box.this_box_data_size -= 8;
		if (*c_buf == '$') {
			psmss->channel_id = *(c_buf + 1);
			if (psmss->channel_id != 0 && psmss->channel_id != 1) goto error;
			psmss->curr_rtp_pkt_size = *((uint16_t *)(c_buf + 2));
			if (psmss->curr_rtp_pkt_size > 1600) goto error;
			psmss->curr_pkt_rtp_time_stamp = *((uint32_t *)(c_buf + 4));
			ret = nkn_vpe_buffered_read(c_buf + 8, psmss->curr_rtp_pkt_size, 1, psmss->p_file);
			if (ret == 0) { err = 10; goto error; }
			psmss->cur_seek_box.this_box_data_size -= psmss->curr_rtp_pkt_size;

			/* Only sent out when trackType are the same */
			if (psmss->channel_id == channel_id) {
				/* Update sequence number and timestamp. Would need
				 * both if PAUSE and PLAY is given.
				 */
				//psmss->sequenceNumber = ntohs(*((uint16_t *)(c_buf + 8 + 2)));
				psmss->lastTimeStamp  = ntohl(*((uint32_t *)(c_buf + 8 + 4)));
				psmss->ssrc	      = ntohl(*((uint32_t *)(c_buf + 8 + 8)));
				psmss->rtpTime	      = psmss->lastTimeStamp;

				/* Check time, if less than current time, send the packet.
				 */
				if (seek_time < psmss->curr_pkt_rtp_time_stamp) {
					psmss->cur_seek_box.this_box_data_size += 8 + psmss->curr_rtp_pkt_size;
					ret = nkn_vfs_fseek(psmss->p_file, cur_pos, SEEK_SET);
					break;
				}
				psmss->last_pkt_rtp_time_stamp = psmss->curr_pkt_rtp_time_stamp;
			}
			else {
				continue; //break;
			}
			
		}
		else {
			err = 11; 
			goto error;
		}
	}
#if 0
	printf("Seek ok Time %d, Pos %ld Pkt Hdr %c %d %d %d SBox %d %d\r\n", 
		seek_time, cur_pos,
		*c_buf, psmss->channel_id, psmss->curr_rtp_pkt_size,
		psmss->curr_pkt_rtp_time_stamp,
		psmss->cur_seek_box.end_time,
		psmss->cur_seek_box.this_box_data_size );
#endif
	return 0;

error:
#if 0
	printf("Seek error Time %d, Pos %ld Pkt Hdr %c %d %d %d SBox %d %d\r\n", 
		seek_time, cur_pos,
		*c_buf, psmss->channel_id, psmss->curr_rtp_pkt_size,
		psmss->curr_pkt_rtp_time_stamp,
		sb.end_time,
		sb.this_box_data_size );
#endif
	return -1;
}

