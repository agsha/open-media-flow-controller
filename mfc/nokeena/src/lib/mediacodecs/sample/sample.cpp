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

typedef struct Zillion_t
{
        float fDuration;
        int ftcpSocketNum;
        uint32_t if_addr;
        FILE * fd;
        char * fFileName;
        TaskToken ftaskToken;

	float maxSubsessionDuration;
	float minSubsessionDuration;
        char fTrackId[10];

        uint32_t fServerAddressForSDP;
} Zillion;



////////// Zillion //////////

extern "C" Boolean createNew_Zillion (char * dataFileName, int sockfd, uint32_t if_addr, void ** pvideo, void ** paudio)
{
	Zillion * p;
	*pvideo = NULL;
	*paudio = NULL;
	p =(Zillion *)malloc(sizeof(Zillion));
	if(!p) return False;
	memset((char *)p, 0, sizeof(Zillion));
	p->fFileName = strdup(dataFileName);
	p->ftcpSocketNum = sockfd;
	p->if_addr = if_addr;
	*pvideo = p;
	return True;
}


extern "C" char const* imp_trackId(void * p) {
	Zillion * psmss = (Zillion *)p;
	return psmss->fTrackId;
}

extern "C" char const* imp_sdpLines(void * p) 
{
	Zillion * psmss = (Zillion *)p;
        char buf[1024];
        struct in_addr in;

        in.s_addr = psmss->if_addr;
        snprintf(buf,
		1024,
		"m=video 0 tcp 33\n"
		"v=0\r\n"
		"o=- 201002262049000250 201002262049000250 IN IP4 127.0.0.1\r\n"
		"s=Juniper Format\r\n"
		"t=0 0\r\n"
		"a=control:*\r\n"
		"m=video 0 tcp 33\r\n"
		"c=IN IP4 %s\r\n"
		"a=control:track1\r\n",
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

static void sendNext(void* firstArg);
static void sendNext(void* firstArg) {

        Zillion * p = (Zillion *)firstArg;

        char buf[4096];
        int xlen = 0x800;
        int ret, sret, totret;
        char * p5;

        if(nkn_vfs_feof(p->fd)) {
                nkn_vfs_fclose(p->fd);
                p->fd = NULL;
                return;
        }

        buf[0]='$';
        buf[1]=0x00;
        buf[2]=0x08;
        buf[3]=0x00;

        totret = 0;

        ret = nkn_vfs_fread(&buf[4], 1, xlen, p->fd);
        totret += ret;
        //printf("read: totret=%d\n", totret);

        sret = ret+4;
        p5 = buf;
        while(sret) {
                ret = send(p->ftcpSocketNum, p5, sret, 0);
                if(ret<0) {
                        printf("socket send error =%d\n", errno);
                        return;
                }
                sret -= ret;
                p5 += ret;
        }

        p->ftaskToken = scheduleDelayedTask(2000, (TaskFunc*)sendNext, firstArg);
}

extern "C" int imp_startStream(void * p,
                        unsigned clientSessionId, void* streamToken,
                           TaskFunc* rtcpRRHandler,
                           void* rtcpRRHandlerClientData,
                           unsigned short * rtpSeqNum,
                           unsigned * rtpTimestamp) 
{
	Zillion * psmss = (Zillion *)p;

        if(psmss->fd != NULL) return -1;

        int curFlags = fcntl(psmss->ftcpSocketNum, F_GETFL, 0);
        curFlags &= ~O_NONBLOCK;
        fcntl(psmss->ftcpSocketNum, F_SETFL, curFlags);

        psmss->fd = nkn_vfs_fopen(psmss->fFileName, "rb");
        if(!psmss->fd) return -1;

        psmss->ftaskToken = scheduleDelayedTask(2000, (TaskFunc*)sendNext, p);
        return 0;
}

extern "C" int imp_pauseStream(void * p, unsigned clientSessionId,
					void* streamToken) {
	printf("Zillion::pauseStream\n");
	return 0;
}
extern "C" int imp_seekStream(void * p, unsigned clientSessionId,
				       void* streamToken, double rangeStart, double rangeEnd) {
	printf("Zillion::seekStream\n");
	return 0;
}
extern "C" int imp_setStreamScale(void * p, unsigned clientSessionId,
					   void* streamToken, float scale) {
	printf("Zillion::setStreamScale\n");
	return 0;
}
extern "C" void * imp_deleteStream(void * p, unsigned clientSessionId,
					 void* streamToken) {
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
	return 1.0f;
}

extern "C" float imp_duration(void * p) {
	Zillion * psmss = (Zillion *)p;
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
	Zillion * psmss = (Zillion *)p;
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
	Zillion * psmss = (Zillion *)p;
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

