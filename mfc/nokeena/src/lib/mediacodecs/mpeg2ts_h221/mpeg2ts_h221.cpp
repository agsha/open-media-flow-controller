#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "rtsp_func.h"
#include "nkn_vfs.h"
#include "trickplay.h"
#include "nkn_vpe_bufio.h"


enum playState  { PLAY, /* NORMAL PLAYBACK */
		  PAUSE, /* PAUSES DELIVERY, retains state information on resume */
		  TRICKMODE, /* FF/RW */
		  STOP /* PAUSES DELIVERY, resets state information on resume */
                };

/* ZIllion specific state */
typedef struct Zillion_t
{
    float fDuration;
    int ftcpSocketNum;
    uint32_t if_addr;
    FILE * fd;
    char * fFileName;
   
    /* meta data information */
    char * metaData;
    int metaDataSize;
    int32_t avgBitrate;
    int64_t duration;

    /* Server Side Player related information */
    float scale;
    uint8_t trickMode;
    enum playState ps;

    /* delivery related info */
    int32_t lastPacketSent;
    int32_t nPacketsPerInterval;
    int32_t nTplayIDRSent;
    int32_t totPackets;

    /*Protocol info */
    TaskToken ftaskToken;
    float maxSubsessionDuration;
    float minSubsessionDuration;
    char fTrackId[10];
    uint32_t fServerAddressForSDP;
    uint32_t clientSessionId;
    int fIsStreamPlaying;
} Zillion;

/************************PROTOTYPES********************/
extern "C" void * imp_deleteStream(void * p, unsigned clientSessionId, void* streamToken);
//static int32_t sendIDRPacket(Zillion *st, int32_t pktNum, int32_t timeStamp);
static int32_t sendIDRPicture(Zillion *st, int32_t pktNum, int32_t nTSPackets);
static void sendTPlay1(void *firstArg);
static int32_t lookupTSPacketByNPT(Zillion *st, float seekTo);
static int32_t getNumMetaDataPackets(Zillion *st);
int64_t get_duration(uint8_t *p_idx_file_data, size_t size);
extern "C" int findRemapLen(char *fname, char remap_chr, int search_dirn);
static void sendNext(void* firstArg);
int change_PTS(uint8_t *pkt,int64_t PTS);
static size_t block_read(uint8_t *buf, size_t size, void *fd);
extern "C" float imp_testScaleFactor(void * p, float scale);

/* constants */
#define TASK_INTERVAL 2000
#define TS_PKT_SIZE 188
#define TS_PACKETS_PER_NW_PKT 7
#define H221_DELIMITER_SIZE 4
#define TS_CLK_FREQ 90 /*KHz*/

typedef struct tag_tplayReq{
    Zillion *st;
    float scale;
    int32_t IDRFrameSize;
    int32_t IDRInterval;
}tplayReq_t;

////////// Zillion //////////
#define DM2_ENABLED

/***************************************************************
 *                 UTILITY FUNCTIONS
 ***************************************************************/

/** returns the duration of the MPEG2 TS file, reading from the meta data
 *@param uint8_t *p_idx_file_data[in] - meta data
 *@param size[in] - size of meta data
 */
int64_t get_duration(uint8_t *p_idx_file_data, size_t size)
{
    return (*((int64_t*)(&p_idx_file_data[size-8])));
}

/** changes the PTS in a MPEG TS packet
 *@param uint8_t* pkt[in] - MPEG TS packet data
 *@param PTS [in] - timestamp
 */
int change_PTS(uint8_t *pkt,int64_t PTS)
{
    uint8_t adaptation_field=0, adaptation_field_length=190,payload_start_byte=0,pes_pos=0,PTS_DTS_flags,word,*PES;
    adaptation_field=(pkt[3]>>4)&0x03;
    /*Adapatation Field Present:*/
    if(adaptation_field==2||adaptation_field==3)
	{
	    adaptation_field_length=pkt[4];
	    if(adaptation_field_length==0)
		adaptation_field_length=1; //accnt for 1 stuffing byte
	    payload_start_byte++;//1 byte for adaptation field length
	}
    else
	adaptation_field_length=0;
    
    payload_start_byte+=4+adaptation_field_length;
    PES=&pkt[payload_start_byte];
    pes_pos=6+1;
    PTS_DTS_flags=PES[pes_pos++]>>6;
    pes_pos++;
    if(PTS_DTS_flags==2||PTS_DTS_flags==3){
	word=PTS_DTS_flags<<4;
	word|=(uint8_t)((((PTS>>30)<<1)&0xE)|0x01);
	PES[pes_pos++]=word;
	word=(uint8_t)(PTS>>22);
	PES[pes_pos++]=word;
	word=(uint8_t)(((PTS>>15)&0x7f)<<1);
	word|=0x01;
	PES[pes_pos++]=word;
	word=(uint8_t)(PTS>>7);
	PES[pes_pos++]=word;
	word=(uint8_t)((PTS<<1)&0xFE);
	word|=1;
	PES[pes_pos]=word;
    }
    return 1;
}


/* reads data in 1 MB blocs, VFS supports only 1 MB reads
 *@param buf [in/out] - buffer to copy data into
 *@param size [in] - size of data to copy
 *@param fd [in/out] - file descriptor
 *@return returns the size of data copied in bytes
 */
static size_t block_read(uint8_t *buf, size_t size, void *fd)
{
    int64_t rem, blockSize, tot, ret;
    uint8_t *ptr;

    rem = size;
    ptr = buf;
    tot = 0;
    blockSize = (1024*1024)-1;

    do {
	ret = nkn_vfs_fread(ptr, 1, blockSize, (FILE*)fd);
	ptr += blockSize;
	rem -= ret;
	tot += ret;
    } while(rem);
    
    return tot;
	
}

/* find the total number of meta data packets */
static int32_t getNumMetaDataPackets(Zillion *st)
{
    return ((st->metaDataSize - sizeof(int64_t))/sizeof(tplay_index_record));
}

/** look up a TS packet based on the NPT
 * @param st - the state of the current session
 * @param seekTo - value to seek to in seconds (only seconds is allowed)
 * @return returns the packet number to seek to, if the seekTo value is <= 0 then it returns 0, if seekTo > duration of video
 * then it returns the last IDR frame.
 */
static int32_t lookupTSPacketByNPT(Zillion *st, float seekTo)
{
    int64_t seek;
    int32_t nMetaDataPackets;
    int32_t ll, ul, t,pkt;
    int64_t uPTS, lPTS, newPTS;
    tplay_index_record *tply;
    uint8_t found;

    nMetaDataPackets = 0;
    ll = 0;
    t = 0;
    newPTS = 0;
    tply = NULL;
    found = 0;
    pkt = 0;

    seek = (int64_t)floor(double(seekTo));

    seek = seek * 1000; //conver to milliseconds
    ul = nMetaDataPackets = getNumMetaDataPackets(st);
    lPTS = 0; uPTS = st->duration;
    tply = (tplay_index_record*)st->metaData;

    //basic sanity return the 0th packet for seek less than or equal to 0,
    //last IDR frame for values greater than the duration!
    if(seek <= 0) {
	return 0;
    }

    if(seek > st->duration) {
	return tply[nMetaDataPackets-1].prev_tplay_pkt;
    }
	
    //find the correct packet using a fast converging search based on the heuristic that the timescale and the 
    //packet number have a roughly one-to-one mapping (relative positions on the timescale and the packet scale)
    //will be similar. below is a brief explanation
    //  TIME SCALE      lNPT<--------------------------------------------------------------->uNPT where lNPT<-0 and uNPT<-duration
    //                                    ^(30%) 
    //                                    |--------
    //                                            |(30%)
    //  PACKET SCALE      ll<----------------------------------------------------------------------------------->ul where ll<-0 and ul<-nMetaDataPackets
    // we iteratively vary the lower bounds (lNPT,ll) or upper bounds (rNPT or ul) based on the guess!
    do {
	/* guess the value! */
	t = ll + int64_t((double(seek-lPTS)/(uPTS-lPTS)) * (ul-ll));

	/* find the PTS at the guessed packet number */
	newPTS = tply[tply[t].prev_tplay_pkt].PTS/TS_CLK_FREQ;//convert to milliseconds

	/*if the guessed PTS is less that search value, update the lower bounds else update the upper bounds */
	if(newPTS  < seek) {
	    lPTS = newPTS;
	    ll = tply[t].prev_tplay_pkt;
	    pkt = ll;
	} else {
	    uPTS = newPTS;
	    ul = tply[t].prev_tplay_pkt;
	    pkt = ul;
	}

	/*if the guessed search value lies within a GOP then we have found the correct seek point */
	if((tply[tply[t].prev_tplay_pkt].PTS/90) < seek && (tply[tply[t].next_tplay_pkt].PTS/90) > seek) {
	    found = 1;
	}

    }while(!found && t < nMetaDataPackets);

    if (pkt < 0)
	pkt = 0;

    return pkt;
}


/****************************************************************************************************
 *                               Media Codec CallBack Functions
 ***************************************************************************************************/

extern "C" Boolean createNew_Zillion (char * dataFileName, int sockfd, uint32_t if_addr, void ** pvideo, void ** paudio)
{
    Zillion * psmss;
    int32_t len, remapPos;
    char *indexFileName;
    FILE *fd;
    struct stat stbuf;

    /* initialization */
    len = 0;
    remapPos = 0;
    *pvideo = NULL;
    *paudio = NULL;
    indexFileName = NULL;
    fd = NULL;

    /*allocate memory for the Zillion object */
    psmss =(Zillion *)malloc(sizeof(Zillion));
    if(!psmss) return False;
    memset((char *)psmss, 0, sizeof(Zillion));
    psmss->fFileName = strdup(dataFileName);
    psmss->ftcpSocketNum = sockfd;
    psmss->if_addr = if_addr;

#ifdef DM2_ENABLED
    /* open the index file for the input video */
    len = strlen(dataFileName);
    remapPos = findRemapLen(dataFileName, '.' , -1);
    indexFileName = (char*)alloca((remapPos+1) + (/*'.dat+/0'*/5));
    memcpy(indexFileName, dataFileName, remapPos + 1);
    indexFileName[remapPos] = '\0';
    strcat(indexFileName, ".dat");
#else
    /* open the index file for the input video */
    {
	char *str = strdup(dataFileName);
	len = strlen(dataFileName);
	remapPos = findRemapLen(dataFileName, '.' , -1);
	indexFileName = (char*)alloca((remapPos+1) + 8 + (/*'.dat+/0'*/5));
	str[remapPos] = '\0';
	sprintf(indexFileName, "/lib/nkn/%s.dat", str);
    }
#endif


    /* read meta data */
    fd = nkn_vfs_fopen(indexFileName , "rb");
    nkn_vfs_stat(indexFileName, &stbuf);
    psmss->metaDataSize = stbuf.st_size;
    psmss->metaData = (char*)malloc(psmss->metaDataSize);
    //block_read((uint8_t*)psmss->metaData, psmss->metaDataSize, fd);
    nkn_vpe_buffered_read((uint8_t*)psmss->metaData, sizeof(uint8_t),psmss->metaDataSize, fd);
    nkn_vfs_fclose(fd); 

    /*read duration and calculate avg bitrate information from the meta data*/
    psmss->duration = get_duration((uint8_t*)psmss->metaData, (size_t)psmss->metaDataSize);
    psmss->duration = psmss->duration/90;//convert to milliseconds
    psmss->totPackets = (psmss->metaDataSize-sizeof(int64_t))/sizeof(tplay_index_record); 
    psmss->avgBitrate = (psmss->totPackets*TS_PKT_SIZE)/(psmss->duration/1000);//bytes per second
    psmss->nPacketsPerInterval =  (psmss->avgBitrate / TS_PKT_SIZE) + ((psmss->avgBitrate % TS_PKT_SIZE)!=0);
    psmss->avgBitrate = TS_PKT_SIZE * psmss->nPacketsPerInterval; //round it to nearest multiple of TS_PKT_SIZE

    *pvideo = psmss;
    return True;
}


extern "C" char const* imp_trackId(void * p) {
    Zillion * psmss = (Zillion *)p;
    return psmss->fTrackId;
}

extern "C" char const* imp_sdpLines(void * p) 
{
    Zillion * psmss = (Zillion *)p;
    char buf[100];
    struct in_addr in;

    in.s_addr = psmss->if_addr;
    snprintf(buf, 100, "m=video 0 tcp 33\n"
	    "c=IN IP4 %s\n"
	    "a=control:track1\n",
	    inet_ntoa(in));
    return strdup(buf);
}

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
					  uint32_t * ssrc, // out
					  void* streamToken // out
					  ) 
{
    *clientRTPPort = 0;
    *clientRTCPPort = 0;
    *destinationAddress = 0;
    *destinationTTL = 0;
    *isMulticast = False;
    *serverRTPPort = 0;
    *serverRTCPPort = 0;
    *ssrc = 0;
    return 0; //psmss->streamToken;
}

/* Sends Trick Play packets in bursts
 */
static void sendTPlay1(void *firstArg)
{
    Zillion *psmss = (Zillion *)firstArg;
    tplay_index_record *tply;
    int64_t timeStamp, IDRInterval, timeElapsed, startTimeStamp;
    int64_t nIDRFrames;
    int32_t i,j;
    int16_t iScale;
    int32_t nTSPackets=0;

    //read state information from the input argument
    //tp = (tplayReq_t*)firstArg;

    //initialization
    i = 0;
    timeElapsed = 0;
    j = 0;
    tply = (tplay_index_record*)(psmss->metaData);
    nIDRFrames = psmss->duration/2; //Assumes there is an IDR frame every 2 seconds
    IDRInterval = (int64_t)((double(psmss->duration)/psmss->scale)/nIDRFrames * 90.0);//convert to MPEG TS units

    //if file handle is invalid
    if(psmss->fd!=NULL) {
	if(nkn_vfs_feof(psmss->fd)) {
	    nkn_vfs_fclose(psmss->fd);
	    psmss->fd = NULL;
	    return;
	}
    }

    do {
	i = psmss->lastPacketSent;
	startTimeStamp = timeStamp = tply[i].PTS;

	for(; i < psmss->totPackets; ) {
	    j = i;
	    nTSPackets=0;

	    //if IDR frame found then start sending all packets until the next non-IDR packet is reached
	    if(tply[i].type == TPLAY_RECORD_PIC_IFRAME) {

		do{
		    nTSPackets++;
		    j++;
		}while (tply[j].type != TPLAY_RECORD_PIC_NON_IFRAME);

                timeStamp += IDRInterval;
		sendIDRPicture(psmss, i, nTSPackets+1); 
		j=i;
	    }//end if all IDR packets sent 

	    // Accomodate Scale skips
	    // Scale factor terminology: 2x -> Every alternate I frame; 4x -> Every fourth I frame; Similarly in the -ve sense
	    iScale = (int16_t)psmss->scale;
	    if (iScale > 0) {
		while (iScale > 1) {
		    i = tply[j].next_tplay_pkt;
		    --iScale;
		    j=i;
		    if (!i || i >= psmss->totPackets || i == psmss->lastPacketSent){ // reached end of stream, start from beginning
			psmss->lastPacketSent = 0;
			// seek to  npt of 0 and resume playback
			if(psmss->fd) {
			    nkn_vfs_fseek(psmss->fd, psmss->lastPacketSent * TS_PKT_SIZE, SEEK_SET);
			}
                        goto done;
		    }
		    psmss->lastPacketSent = i;
		}
	    } else {
		while (iScale < -1) {
		    i = tply[j].prev_tplay_pkt;
		    ++iScale;
		    j=i;
		    if (!i || i == psmss->lastPacketSent){
			psmss->lastPacketSent=psmss->totPackets;
			// seek to npt of 0 and resume playback
			if(psmss->fd) {
                            nkn_vfs_fseek(psmss->fd, psmss->lastPacketSent * TS_PKT_SIZE, SEEK_SET);
                        }
			goto done;
		    }
		    psmss->lastPacketSent = i;
		}
		//		printf("In RW: Next Packet to Send: %d\n",psmss->lastPacketSent);
	    }
	    
	    timeElapsed = abs(timeStamp - startTimeStamp) / 90; //convert MPEG TS timestamp to millisecond
	    if(timeElapsed > (TASK_INTERVAL/1000)) {
	    	goto done;
	    }
	}
		
    }while(timeElapsed > (TASK_INTERVAL/1000));

 done:
	psmss->ftaskToken = scheduleDelayedTask(TASK_INTERVAL, (TaskFunc*)sendTPlay1, firstArg);
   
}

static int32_t sendIDRPicture(Zillion *st, int32_t pktNum, int32_t nTSPackets)
{
    char *buf;
    int32_t ret=0, sret, i;
    char *p5;

    buf = (char*)malloc(TS_PKT_SIZE + H221_DELIMITER_SIZE);

    buf[0]='$';
    buf[1]=0x00;
    buf[2]=0x00;
    buf[3]=0xBC;


    nkn_vfs_fseek(st->fd, pktNum * TS_PKT_SIZE, SEEK_SET);

    for (i=0; i< nTSPackets; i++){
	p5 = buf;

	ret = nkn_vfs_fread(&buf[4], 1, TS_PKT_SIZE, st->fd);
	sret = TS_PKT_SIZE + H221_DELIMITER_SIZE;

	while(sret) {
	    ret = send(st->ftcpSocketNum, p5, sret, 0);
	    updateRTSPCounters(ret);
	    //	printf("sent %d bytes\n with packet number %d", ret, pktNum);

	    if(ret<0) {
		printf("socket send error =%d\n", errno);
		return errno;
	    }
	    sret -= ret;
	    p5 += ret;
	}
    }
 
    st->lastPacketSent = pktNum + nTSPackets;
    free(buf);	

    return 0;
}

static void sendNext(void* firstArg) {

    Zillion * psmss = (Zillion *)firstArg;

    char *buf;
    int bytes_to_read;    
    int xlen = 0x524;
    int ret, sret;
    char * p5;

    bytes_to_read = 0;

    //bytes_to_read = psmss->avgBitrate /*bytespersec*/ * TASK_INTERVAL;
    //buf = (char*)malloc(bytes_to_read + H221_DELIMITER_SIZE);
    //xlen = bytes_to_read;
    buf = (char*)malloc(4096);
  
    if(psmss->fd == NULL) {
	return;
    }

    if(nkn_vfs_feof(psmss->fd)) {
	nkn_vfs_fclose(psmss->fd);
	psmss->fd = NULL;
	return;
    }
    
    //    if (!psmss->fIsStreamPlaying)
    //return;

    buf[0]='$';
    buf[1]=0x00;
    buf[2]=0x05;
    buf[3]=0x24;

    ret = nkn_vfs_fread(&buf[H221_DELIMITER_SIZE], 1, xlen, psmss->fd);
    //    psmss->lastPacketSent += psmss->nPacketsPerInterval;
    psmss->lastPacketSent += xlen/TS_PKT_SIZE;
    printf("sendNext::last packet send %d\n", psmss->lastPacketSent);

    sret = ret+H221_DELIMITER_SIZE;
    p5 = buf;
    while(sret) {
	ret = send(psmss->ftcpSocketNum, p5, sret, 0);
	updateRTSPCounters(ret);
	if(ret<0) {
	    printf("socket send error =%d\n", errno);
	    return;
	}
	sret -= ret;
	p5 += ret;
    }

    free(buf);

    psmss->ftaskToken = scheduleDelayedTask(TASK_INTERVAL, (TaskFunc*)sendNext, firstArg);

    //   printf ("sendNext: last Packet Sent : %d\n", psmss->lastPacketSent);
    return;
}

extern "C" int imp_setStreamScale(void * p, unsigned clientSessionId, void* streamToken, float scale);
extern "C" int imp_startStream(void * p,
				unsigned clientSessionId, 
				void* streamToken,
				TaskFunc* rtcpRRHandler,
				void* rtcpRRHandlerClientData,
				unsigned short * rtpSeqNum,
				unsigned * rtpTimestamp) 
{
    Zillion *psmss = (Zillion *)p;

    //the first time this function is called, scale factor will not be available. this data 
    //will be available after the streamSetScale function is called and then we set the correct
    //callback function

    if(psmss->fd != NULL) {
	if(psmss->scale != 1.0) {
	    
	    /* if the current state of the player is in PLAY then unschedule the task token before switching to TRICKMODE 
	     * typically this happens when the player tries to go to a trick mode playback from a  normal playback
	     */
#if 0
	    if(psmss->ps == PLAY) { 
		if(psmss->ftaskToken) { 
		    unscheduleDelayedTask(psmss->ftaskToken);
		    psmss->ftaskToken = NULL;
		}
	    }
#endif
	    psmss->ftaskToken = scheduleDelayedTask(TASK_INTERVAL, (TaskFunc*)sendTPlay1, p);
	    printf("Zillion::startStream - sendTPlay1\n");
	}
	else {

	    /* if the current state of the player is in TRICKMODE then unschedule the task token before switching to normal PLAY 
	     * typically this happens when the player tries to recover from a trick mode playback to normal playback
	     */
#if 0
	    if(psmss->ps == TRICKMODE) { 
		if(psmss->ftaskToken) { 
		    unscheduleDelayedTask(psmss->ftaskToken);
		    psmss->ftaskToken = NULL;
		}
	    }
#endif
	    psmss->ftaskToken = scheduleDelayedTask(400000, (TaskFunc*)sendNext, p);
	    printf("Zillion::startStream - sendNext\n");
	}
	return 0;
    }

    int curFlags = fcntl(psmss->ftcpSocketNum, F_GETFL, 0);
    curFlags &= ~O_NONBLOCK;
    fcntl(psmss->ftcpSocketNum, F_SETFL, curFlags);

    psmss->fd = nkn_vfs_fopen(psmss->fFileName, "rb");
    if(!psmss->fd) return -1;

    //psmss->fIsStreamPlaying = 1;
    //psmss->ftaskToken = scheduleDelayedTask(400000, (TaskFunc*)sendNext, p);
    printf("Zillion::startStream - sendNext\n");
    return 0;
}

extern "C" int imp_pauseStream(void * p, 
				unsigned clientSessionId,
				void* streamToken) 
{
    //    Zillion *psmss = (Zillion *)p;

    //psmss->fIsStreamPlaying = !psmss->fIsStreamPlaying;
    printf("Zillion::pauseStream\n");
    return 0;
}

extern "C" int imp_seekStream(void * p, 
			       unsigned clientSessionId,
			       void* streamToken, 
			       double rangeStart, double rangeEnd) 
{
    Zillion *psmss = (Zillion *)p;

    psmss->lastPacketSent = lookupTSPacketByNPT(psmss, rangeStart);

    if(psmss->fd) {
	nkn_vfs_fseek(psmss->fd, psmss->lastPacketSent * TS_PKT_SIZE, SEEK_SET);
    }

    printf("Zillion::seekStream\t npt = %f\t lastPacket = %d\n", rangeStart, psmss->lastPacketSent);
    return 0;
}

extern "C" int imp_setStreamScale(void * p, 
				   unsigned clientSessionId,
				   void* streamToken, 
				   float scale) 
{
    Zillion *psmss = (Zillion *)p;

    if(psmss->ftaskToken) {
	unscheduleDelayedTask(psmss->ftaskToken);
	psmss->ftaskToken = NULL;
    }

    psmss->scale = imp_testScaleFactor(p, scale);
    if(psmss->scale != 1) {
	psmss->trickMode = 1;
	psmss->ps = TRICKMODE;
    }
    else {
	psmss->trickMode = 0;
	psmss->ps = PLAY;
    }

    printf("Zillion::setStreamScale %d \n", (int)scale);
    return 0;
}

extern "C" void * imp_deleteStream(void * p, 
				   unsigned clientSessionId,
				   void* streamToken) 
{
    Zillion * psmss = (Zillion *)p;
    printf("Zillion::deleteStream\n");
    if(psmss->fd) {
	if(psmss->ftaskToken) {
		unscheduleDelayedTask(psmss->ftaskToken);
		psmss->ftaskToken = NULL;
	}
	nkn_vfs_fclose(psmss->fd);
	psmss->fd = NULL;
    }
    free(psmss->fFileName);
    free(psmss);
    return NULL;
}

extern "C" float imp_testScaleFactor(void * p, float scale) {
    //    Zillion * psmss = (Zillion *)p;

    // Only integral non zero scales are supported
    int iScale = scale < 0.0 ? (int)(scale - 0.5f) : (int)(scale + 0.5f); 
    if (iScale == 0) {
	iScale = 1;
    } 

    scale = (float)iScale;
    return scale;
}

extern "C" float imp_duration(void * p) {
    Zillion * psmss = (Zillion *)p;

    //set the duration
    psmss->fDuration = psmss->duration;
    return psmss->fDuration;
}

extern "C" float imp_getPlayTime(void * p, unsigned clientSessionId,
					 void* streamToken) 
{
	Zillion * psmss = (Zillion *)p;
	//Todo extract and return current play time
	return -1;
}

extern "C" uint32_t imp_ServerAddressForSDP(void * p) {
    Zillion * psmss = (Zillion *)p;

    return psmss->fServerAddressForSDP;
}

extern "C" char const* imp_rangeSDPLine(void * p) {
    Zillion *psmss = (Zillion *)p;
    float duration;

    if (psmss->maxSubsessionDuration != psmss->minSubsessionDuration) {
	duration = -(psmss->maxSubsessionDuration); // because subsession durations differ
    } else {
	duration = psmss->maxSubsessionDuration; // all subsession durations are the same
    }

    // If all of our parent's subsessions have the same duration
    // (as indicated by "fParentSession->duration() >= 0"), there's no "a=range:" line:
    if (duration >= 0.0) return strdup("");

    // Use our own duration for a "a=range:" line:
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
    Zillion *psmss = (Zillion *)p;
    psmss->maxSubsessionDuration = maxSubsessionDuration;
    psmss->minSubsessionDuration = minSubsessionDuration;
    snprintf(psmss->fTrackId, sizeof(psmss->fTrackId), "track%d", TrackNumber);
    return 0;
}

extern "C" nkn_provider_type_t imp_getLastProviderType(void *p)
{

    Zillion *psmss = (Zillion*)p;

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

    createNew_Zillion
};

extern void init_vfs_init(struct vfs_funcs_t * pfunc);
extern "C" struct rtsp_so_exp_funcs_t * nkn_vfs_init(struct vfs_funcs_t * func);
extern "C" struct rtsp_so_exp_funcs_t * nkn_vfs_init(struct vfs_funcs_t * func)
{
    init_vfs_init(func);

    return &soexpfuncs;
}

extern "C" int findRemapLen(char *fname, char remap_chr, int search_dirn)
{
    char *pos;

    pos = NULL;

    if(search_dirn < 0) {
	pos = strrchr(fname, remap_chr);
    } else {
	pos = strchr(fname, remap_chr);
    }
    
    return pos - fname;
}



// Deprecated code

#if 0
static void sendTplay(void *firstArg) 
{

    pkt_list_t *plist;
    //tplayReq_t *tp;
    Zillion *st;
    char *buf, *p_buf;
    int ret, sret, i, search_st;
    int32_t  j;
    char *p5;

    //tp = (tplayReq_t*)firstArg;
    st = (Zillion*)firstArg;
    plist = NULL;
    ret = sret = i = j = 0;
    p_buf = buf = NULL;

    if(st->fd!=NULL) {
	if(nkn_vfs_feof(st->fd)) {
	    nkn_vfs_fclose(st->fd);
	    st->fd = NULL;
	    return;
	}
    }

    do {

	search_st = st->lastPacketSent;
	plist = NULL;
	do {
	    plist = nkn_vpe_get_first_idr_pkts((uint8_t*)st->metaData, st->metaDataSize, search_st+1, int16_t(st->scale), 1);
	    search_st++;
	} while(plist->num_pkts == 0 && plist->num_pkts != -1 );

	if(plist->num_pkts == -1 ) {
	    //nkn_vfs_fclose(st->fd);
	    //st->fd = NULL;
	    //imp_deleteStream(st, st->clientSessionId, NULL);

	    return;
	}

	if((int32_t)plist->pkt_nums[0] <= st->lastPacketSent) {
	    return;
	}


	tp->IDRFrameSize = plist->num_pkts * TS_PKT_SIZE;
	tp->IDRInterval = int32_t((st->duration/tp->scale)/(st->duration/2));

	if(st->nTplayIDRSent != 0) {
	    p_buf = buf = (char*)malloc((plist->num_pkts * TS_PKT_SIZE) + H221_DELIMITER_SIZE);
	} else {
	    //for the first IDR frame copy all the packets at the begining which will contain PAT,PMT and other MPEG2-TS info.
	    p_buf = buf = (char*)malloc(((plist->num_pkts + (plist->pkt_nums[0]-1)) * TS_PKT_SIZE) + H221_DELIMITER_SIZE);	    
	}
	    
	//read 1 IDR frame
	p_buf += H221_DELIMITER_SIZE;

	if(st->nTplayIDRSent == 0) {
	    for(j=0;j<(int32_t)(plist->pkt_nums[0]-1);j++) {
		nkn_vfs_fseek(st->fd, j * TS_PKT_SIZE, SEEK_SET);
		ret += nkn_vfs_fread(p_buf, 1, TS_PKT_SIZE, st->fd);
		st->lastPacketSent = j;
		p_buf += TS_PKT_SIZE;
	    }
	}

	for(j = 0;j < plist->num_pkts;j++) {
	    nkn_vfs_fseek(st->fd, plist->pkt_nums[j] * TS_PKT_SIZE, SEEK_SET);
	    ret += nkn_vfs_fread(p_buf, 1, TS_PKT_SIZE, st->fd);
	    st->lastPacketSent = plist->pkt_nums[j];
	    p_buf += TS_PKT_SIZE;
	}
	i += tp->IDRInterval;
	st->nTplayIDRSent++;

	p5 = buf;
	buf[0]='$';
	buf[1]=0x00;
	buf[2]=0x08;
	buf[3]=0x00;

	//send 1 IDR frame
	sret = ret+H221_DELIMITER_SIZE;
	p5 = buf;
	while(sret) {
	    ret = send(st->ftcpSocketNum, p5, sret, 0);
	    printf("sent %d bytes\n", ret);

	    if(ret<0) {
		printf("socket send error =%d\n", errno);
		return;
	    }
	    sret -= ret;
	    p5 += ret;
	}

	//reset send state
	ret = 0;
	free(buf);

    }while(i < TASK_INTERVAL);

    //send first set of data
    st->ftaskToken = scheduleDelayedTask(TASK_INTERVAL, (TaskFunc*)sendTplay, firstArg);

}


static int32_t sendIDRPacket(Zillion *st, int32_t pktNum, int32_t timeStamp)
{
    int8_t buf[TS_PKT_SIZE + H221_DELIMITER_SIZE];
    int32_t ret, sret;
    int8_t *p5;


    nkn_vfs_fseek(st->fd, pktNum * TS_PKT_SIZE, SEEK_SET);
    ret += nkn_vfs_fread(&buf[4], 1, TS_PKT_SIZE, st->fd);
    if(timeStamp >= 0){ 
	change_PTS((uint8_t*)(&buf[4]), timeStamp);
    }

    p5 = buf;
    buf[0]='$';
    buf[1]=0x00;
    buf[2]=0x00;
    buf[3]=0xBC;

    //send 1 IDR frame
    sret = TS_PKT_SIZE+H221_DELIMITER_SIZE;
    p5 = buf;
    while(sret) {
	ret = send(st->ftcpSocketNum, p5, sret, 0);
	//	printf("sent %d bytes\n with packet number %d", ret, pktNum);

	if(ret<0) {
	    printf("socket send error =%d\n", errno);
	    return errno;
	}
	sret -= ret;
	p5 += ret;
    }

    st->lastPacketSent = pktNum;
		
    return 0;
}


#endif
