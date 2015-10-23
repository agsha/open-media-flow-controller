#ifndef RTCP_UTIL_HH
#define RTCP_UTIL_HH

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <netinet/in.h>
#include <string.h>

#define RTCP_SENDER_INFO_SIZE 20
#define RTCP_RECEIVER_INFO_SIZE 28
#define RTCP_SENDER_SSRC_SIZE 4
#define RTCP_INITIAL_HDR_SIZE 4

#define RTCP_SR_ALL_SIZE ((RTCP_SENDER_INFO_SIZE) +  (RTCP_SENDER_SSRC_SIZE) + (RTCP_INITIAL_HDR_SIZE))
#define RTCP_RR_ALL_SIZE ((RTCP_RECEIVER_INFO_SIZE) + (RTCP_SENDER_SSRC_SIZE) + (RTCP_INITIAL_HDR_SIZE))

//////// Functions used in rtcp dissections {

typedef struct rtcp_pkt_dissect {
	uint8_t* sr;//Fixed length : 28 bytes (RTCP_SENDER_INFO + RTCP_SENDER_SSRC + RTCP_INITIAL_HDR)
	uint8_t rr_count;//Number of RR reports in the received RTCP pkt
	
		/* char* rr - It can point to more than one RR. If sr = NULL, 
		first rr will be of length 32 (bytes) and the subsequent 
		ones will be of length 28 bytes.
		(minus RTCP_INITIAL_HDR, RTCP_SENDER_SSRC
		Else All rr will be of length 28 bytes. 
		This region can be read with the help of rr_count*/
	uint8_t* rr;
	uint8_t* sdes;
	uint8_t* bye;
} rtcp_pkt_dissect_t;

void init_rtcp_pkt_dissect(rtcp_pkt_dissect_t* rtcp_dissect);

/* Given a RTCP pkt to the dissector, 
it parses and fills the struct rtcp_pkt_dissect as required by the standard*/
int32_t rtcp_dissector(uint8_t* p, int32_t total_bytes, int32_t offset, rtcp_pkt_dissect_t* rtcp_dissect);

//////// Functions used in rtcp dissections }

#endif
