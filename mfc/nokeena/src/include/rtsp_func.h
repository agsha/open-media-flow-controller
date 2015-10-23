#ifndef RTSP_DEFS__H
#define RTSP_DEFS__H

#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __cplusplus 
extern "C" {
#endif

#include "rtsp_session.h"
#include "vfs_defs.h"

/*
 * These functions are exported from rtsp shared bibrary to nvsd 
 */
struct rtsp_so_exp_funcs_t {
	char const * (* p_subs_trackId)(void * psubs);
	char const* (* p_subs_sdpLines)(void * psubs);
	int (*p_subs_setTrackNumer)(void * psubs, 
				float maxSubsessionDuration, 
				float minSubsessionDuration, 
				unsigned TrackNumber);

	// return StreamToken
	void * (*p_subs_getStreamParameters)(void * psubs,
                                unsigned clientSessionId, // in
                                uint32_t clientAddress, // in
                                uint16_t * clientRTPPort, // in network order
                                uint16_t * clientRTCPPort, // in network order
                                int tcpSocketNum, // in (-1 means use UDP, not TCP)
                                unsigned char rtpChannelId, // in (used if TCP)
                                unsigned char rtcpChannelId, // in (used if TCP)
                                uint32_t * destinationAddress, // in out
                                uint8_t * destinationTTL, // in out
                                Boolean * isMulticast, // out
                                uint16_t * serverRTPPort, // out network order
                                uint16_t * serverRTCPPort, // out network order, depreciated
				uint32_t * ssrc, // out
                                void * streamToken // out
                                );
	int (*p_subs_startStream)(void * psubs,
				unsigned clientSessionId, void* streamToken,
				TaskFunc* rtcpRRHandler,
				void* rtcpRRHandlerClientData,
				unsigned short * rtpSeqNum,	// out
				unsigned* rtpTimestamp);	// out
	int (*p_subs_pauseStream)(void * psubs,unsigned clientSessionId, void* streamToken);
	int (*p_subs_seekStream)(void * psubs,unsigned clientSessionId, void* streamToken, double rangeStart, double rangeEnd);
	int (*p_subs_setStreamScale)(void * psubs,unsigned clientSessionId, void* streamToken, float scale);
	void * (*p_subs_deleteStream)(void * psubs,unsigned clientSessionId, void* streamToken);
	float (*p_subs_testScaleFactor)(void * psubs,float scale);
	float (*p_subs_duration)(void * psubs) ;
	float (*p_subs_getPlayTime)(void * psubs, unsigned clientSessionId, void* streamToken);
        nkn_provider_type_t (*p_subs_getLastProviderType)(void *psubs);
    
	/* Create sessions */
	Boolean (*p_new_subsession)(char * fileName, 
				int client_sockfd,
				uint32_t if_addr, // network order
				void ** pvideo, 
				void ** paudio);
};

#ifdef __cplusplus
} //cpluplus externs
#endif

#endif // RTSP_DEFS__H
