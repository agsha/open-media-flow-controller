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

extern uint32_t ourIPAddress(void);

typedef struct Amino_t
{
        float fDuration;
        int ftcpSocketNum;
        FILE * fd;
        char * fFileName;
        TaskToken ftaskToken;

	float maxSubsessionDuration;
	float minSubsessionDuration;
        char fTrackId[10];

        uint32_t fServerAddressForSDP;
	int udpsocket;
	unsigned short local_port;
	unsigned int local_ip;
	unsigned short remote_port;
	unsigned int remote_ip;
} Amino;



////////// Amino //////////

extern "C" Boolean createNew_Amino (char * dataFileName, int sockfd, uint32_t if_addr, void ** pvideo, void ** paudio)
{
	Amino * p;
	*pvideo = NULL;
	*paudio = NULL;
	p =(Amino *)malloc(sizeof(Amino));
	if(!p) return False;
	memset((char *)p, 0, sizeof(Amino));
	p->fFileName = strdup(dataFileName);
	p->ftcpSocketNum = sockfd;
	p->local_port = 27389;
	p->local_ip = if_addr;
	p->remote_port = 0;
	p->remote_ip = 0;
	*pvideo = p;
	return True;
}


extern "C" char const* imp_trackId(void * p) {
	Amino * psmss = (Amino *)p;
	return psmss->fTrackId;
}

extern "C" char const* imp_sdpLines(void * p) 
{
	Amino * psmss = (Amino *)p;
        char buf[100];
        struct in_addr in;

        in.s_addr = psmss->local_ip;
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
        struct sockaddr_in si_me;
	Amino * psmss = (Amino *)p;

        if ((psmss->udpsocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1) {
                printf("rtsp_init_udp_socket: failed\n");
                exit(2);
        }

        memset((char *) &si_me, 0, sizeof(si_me));
again:
        si_me.sin_family = AF_INET;
        si_me.sin_port = htons(psmss->local_port);
        si_me.sin_addr.s_addr = psmss->local_ip;
        if (bind(psmss->udpsocket, (struct sockaddr *)&si_me, sizeof(si_me))==-1) {
                psmss->local_port ++;
                goto again;
        }
        printf("rtsp_init_udp_socket udp listen on port=%d\n", psmss->local_port);

        printf("rtsp_init_udp_socket clientRTPPort=%d, destinAddress=0x%x\n", 
				*clientRTPPort, *destinationAddress);

        psmss->remote_port = *clientRTPPort;
        *clientRTCPPort = 0;
        psmss->remote_ip = *destinationAddress;
        //*destinationAddress = 0;
        *destinationTTL = 0;
        *isMulticast = False;
        *serverRTPPort = htons(psmss->local_port);
        *serverRTCPPort = 0;
	*ssrc = 0;

        return 0; //psmss->streamToken;
}

static void sendNext(void* firstArg);
static void sendNext(void* firstArg) {

        Amino * p = (Amino *)firstArg;

        char buf[4096];
        int xlen = 7*188;
        int ret;
	struct sockaddr_in si_other;
	int slen = sizeof(struct sockaddr_in);

	//printf("Amino::sendNext\n");
        if(nkn_vfs_feof(p->fd)) {
                nkn_vfs_fclose(p->fd);
                p->fd = NULL;
                return;
        }

	ret = nkn_vfs_fread(buf, 1, xlen, p->fd);
	if(ret != xlen) {
		printf("nkn_vfs_fread: return len = %d, expect len = %d\n", ret, xlen);
                nkn_vfs_fclose(p->fd);
                p->fd = NULL;
                return;
	}

	memset((char *) &si_other, 0, sizeof(si_other));
	si_other.sin_family = AF_INET;
	si_other.sin_port = p->remote_port;
	si_other.sin_addr.s_addr = p->remote_ip;

	ret = sendto(p->udpsocket, buf, xlen, 0, (struct sockaddr *)&si_other, slen);
	updateRTSPCounters(ret);
	if(ret == -1) {
		printf("sendto returns %d, errno=%d\n", ret, errno);
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
	Amino * psmss = (Amino *)p;

        if(psmss->fd != NULL) return -1;

        int curFlags = fcntl(psmss->ftcpSocketNum, F_GETFL, 0);
        curFlags &= ~O_NONBLOCK;
        fcntl(psmss->ftcpSocketNum, F_SETFL, curFlags);

        psmss->fd = nkn_vfs_fopen(psmss->fFileName, "rb");
        if(!psmss->fd) return -1;

	printf("Amino::startStream\n");
        psmss->ftaskToken = scheduleDelayedTask(2000, (TaskFunc*)sendNext, p);
        return 0;
}

extern "C" int imp_pauseStream(void * p, unsigned clientSessionId,
					void* streamToken) {
	printf("Amino::pauseStream\n");
	return 0;
}
extern "C" int imp_seekStream(void * p, unsigned clientSessionId,
				       void* streamToken, double rangeStart, double rangeEnd) {
	printf("Amino::seekStream\n");
	return 0;
}
extern "C" int imp_setStreamScale(void * p, unsigned clientSessionId,
					   void* streamToken, float scale) {
	printf("Amino::setStreamScale\n");
	return 0;
}
extern "C" void * imp_deleteStream(void * p, unsigned clientSessionId,
					 void* streamToken) {
	Amino * psmss = (Amino *)p;
        printf("Amino::deleteStream\n");
        if(psmss->fd) {
                if(psmss->ftaskToken) {
			unscheduleDelayedTask(psmss->ftaskToken);
			psmss->ftaskToken = NULL;
		}
                nkn_vfs_fclose(psmss->fd);
                psmss->fd = NULL;
        }
	close(psmss->udpsocket);
        free(psmss->fFileName);
        free(psmss);
        return NULL;
}

extern "C" float imp_testScaleFactor(void * p, float scale) {
	return 1.0f;
}

extern "C" float imp_duration(void * p) 
{
	Amino * psmss = (Amino *)p;
        return psmss->fDuration;
}

extern "C" float imp_getPlayTime(void * p, unsigned clientSessionId,
					 void* streamToken) {
	Amino * psmss = (Amino *)p;
	//Todo extract and return current play time
        return -1;
}

extern "C" uint32_t imp_ServerAddressForSDP(void * p) {
        Amino * psmss = (Amino *)p;
        return psmss->fServerAddressForSDP;
}

extern "C" char const* imp_rangeSDPLine(void * p) {
	Amino * psmss = (Amino *)p;
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
	Amino * psmss = (Amino *)p;
	psmss->maxSubsessionDuration = maxSubsessionDuration;
	psmss->minSubsessionDuration = minSubsessionDuration;
	snprintf(psmss->fTrackId, sizeof(psmss->fTrackId), "track%d", TrackNumber);
	return 0;
}

extern "C" nkn_provider_type_t imp_getLastProviderType(void *p)
{

    Amino *psmss = (Amino*)p;

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
	createNew_Amino
};

extern void init_vfs_init(struct vfs_funcs_t * pfunc);
extern "C" struct rtsp_so_exp_funcs_t * nkn_vfs_init(struct vfs_funcs_t * func);
extern "C" struct rtsp_so_exp_funcs_t * nkn_vfs_init(struct vfs_funcs_t * func)
{
	init_vfs_init(func);

	return &soexpfuncs;
}

