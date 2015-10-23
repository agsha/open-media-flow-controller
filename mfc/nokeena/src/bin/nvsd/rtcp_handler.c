#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include "network.h"
#include "nkn_defs.h"
#include "nkn_debug.h"
#include "nkn_stat.h"
#include "rtsp_server.h"

NKNCNT_DEF(tot_rtcp_sr, uint64_t, "", "Total SR RTCP packets")
NKNCNT_DEF(tot_rtcp_rr, uint64_t, "", "Total RR RTCP packets")
NKNCNT_DEF(tot_rtcp_bye, uint64_t, "", "Total BYE RTCP packets")
NKNCNT_DEF(tot_rtcp_unsupported_type, uint32_t, "", "Total unsupposed RTCP type packets")
NKNCNT_DEF(rtcp_tot_udp_packets_recv, uint64_t, "", "Total RTCP packets recieved")


int RTCP_createNew( rtsp_cb_t * prtsp,
		    int streamNum,
		    char * cname,
		    unsigned short serverRTPPort);
void free_RTCPInstance(struct streamState * pss);
static int rtcp_epollin(int sockfd, void *private_data);
static int rtcp_epollout(int sockfd, void * private_data);
static int rtcp_epollerr(int sockfd, void * private_data);
static int rtcp_epollhup(int sockfd, void * private_data);
static int setupDatagramSocket(uint32_t rtcpPortNum, struct in_addr * ifaddr);


static unsigned const IP_UDP_HDR_SIZE = 28;

typedef enum {
	PACKET_UNKNOWN_TYPE = 0,
	PACKET_RTCP_REPORT,
	PACKET_BYE
} packet_type_t;

// RTCP packet types:
#define RTCP_PT_SR 200
#define RTCP_PT_RR 201
#define RTCP_PT_SDES 202
#define RTCP_PT_BYE 203
#define RTCP_PT_APP 204

// SDES tags:
#define RTCP_SDES_END 0
#define RTCP_SDES_CNAME 1
#define RTCP_SDES_NAME 2
#define RTCP_SDES_EMAIL 3
#define RTCP_SDES_PHONE 4
#define RTCP_SDES_LOC 5
#define RTCP_SDES_TOOL 6
#define RTCP_SDES_NOTE 7
#define RTCP_SDES_PRIV 8

#define ADVANCE(n) pkt += (n); packetSize -= (n)


static int rtcp_epollin(int sockfd, void* private_data)
{
	char* pkt;
	struct sockaddr_in fromAddress;
	packet_type_t typeOfPacket;
	char buffer[2048];
	int bufferSize = sizeof(buffer);
	unsigned int addressSize;
	int packetSize;
	unsigned rtcpHdr;
	unsigned reportSenderSSRC;
	Boolean packetOK;
	unsigned rc;
	unsigned pt;
	int length;
	Boolean subPacketOK;
	unsigned NTPmsw;
	unsigned NTPlsw;
	unsigned rtpTimestamp;
	struct streamState * pss;

	pss = (struct streamState *)private_data;
	if (!pss || gnm[sockfd].fd == -1) {
		return TRUE;
	}

	if (pss->startRtcp == FALSE && pss->prtsp->isPlaying == TRUE) {
		pss->startRtcp = TRUE;
	}

	if (pss->startRtcp == FALSE) {
		return TRUE;
	}

	glob_rtcp_tot_udp_packets_recv ++;

	/*
	 * Read the RTCP packet from UDP socket.
	 */
	addressSize = sizeof(fromAddress);
	packetSize = recvfrom(sockfd, (char*)buffer, bufferSize, 0, /*MSG_DONTWAIT,*/
				(struct sockaddr*)&fromAddress, &addressSize);
	if (packetSize < 4) {
		// Read error, but don't close the UDP socket.
		return TRUE;
	}

	/* Reset Timeout flag, once we get some RTCP packet
	 */
	pss->isTimeout = FALSE;
	
	/*
	 * Analyze the RTCP/UDP packet.
	 */
	typeOfPacket = PACKET_UNKNOWN_TYPE;
	pkt = buffer;
	
	/*
	 * Check the RTCP packet for validity:
	 * It must at least contain a header (4 bytes), and this header
	 * must be version=2, with no padding bit, and a payload type of
	 * SR (200) or RR (201):
	 */
	rtcpHdr = ntohl(*(unsigned*)pkt);
	if ((rtcpHdr & 0xE0FE0000) != (0x80000000 | (RTCP_PT_SR<<16))) {
		return TRUE;
	}

	/*
	 * Process each of the individual RTCP 'subpackets' in (what may be)
	 * a compound RTCP packet.
	 */
	reportSenderSSRC = 0;
	packetOK = False;
	rc = (rtcpHdr>>24)&0x1F;
	pt = (rtcpHdr>>16)&0xFF;
	length = 4*(rtcpHdr&0xFFFF); // doesn't count hdr
	ADVANCE(4); // skip over the header
	if (length > packetSize) return TRUE;
		
	// Assume that each RTCP subpacket begins with a 4-byte SSRC:
	if (length < 4) return TRUE; length -= 4;
	reportSenderSSRC = ntohl(*(unsigned*)pkt); ADVANCE(4);

	/*
	 * Activate prtsp session again to avoid session timeout.
	 * pss->prtsp is available
	 */
	//rtsp_set_timeout(pss->prtsp);
		
	subPacketOK = False;
	switch (pt) {
	case RTCP_PT_SR:
	{
		glob_tot_rtcp_sr ++;
				
		if (length < 20) break; length -= 20;
				
		// Extract the NTP timestamp, and note this:
		NTPmsw = ntohl(*(unsigned*)pkt); ADVANCE(4);
		NTPlsw = ntohl(*(unsigned*)pkt); ADVANCE(4);
		rtpTimestamp = ntohl(*(unsigned*)pkt); ADVANCE(4);
		ADVANCE(8); // skip over packet count, octet count
				
		/*
		 * Call back to inform for Sender Report (SR)
		 */

		unsigned reportBlocksSize = rc*(6*4);
		if (length < (int)reportBlocksSize) break;
		length -= reportBlocksSize;

		ADVANCE(reportBlocksSize);
	}
	break;
		
	case RTCP_PT_RR: 
	{
		glob_tot_rtcp_rr ++;
		
		unsigned reportBlocksSize = rc*(6*4);
		if (length < (int)reportBlocksSize) break;
		length -= reportBlocksSize;

		ADVANCE(reportBlocksSize);

		subPacketOK = True;
		typeOfPacket = PACKET_RTCP_REPORT;
	}
	break;

	case RTCP_PT_BYE: 
	{
		glob_tot_rtcp_bye ++;

		subPacketOK = True;
		typeOfPacket = PACKET_BYE;
	}
	break;

	// Later handle SDES, APP, and compound RTCP packets #####
	default:
		glob_tot_rtcp_unsupported_type ++;
		subPacketOK = True;
	break;
	}
		
	return TRUE;
}

static int rtcp_epollout(int sockfd, void* private_data)
{
	UNUSED_ARGUMENT(sockfd);
	UNUSED_ARGUMENT(private_data);
	return TRUE;
}

static int rtcp_epollerr(int sockfd, void* private_data)
{
	UNUSED_ARGUMENT(sockfd);
	UNUSED_ARGUMENT(private_data);
	return TRUE;
}

static int rtcp_epollhup(int sockfd, void* private_data)
{
	UNUSED_ARGUMENT(sockfd);
	UNUSED_ARGUMENT(private_data);
	return TRUE;
}

/* port network order */
static int setupDatagramSocket(uint32_t rtcpPortNum, struct in_addr * ifaddr)
{
	int fd;
	int reuse_port;
	uint32_t addr;
        int opts;

	UNUSED_ARGUMENT(rtcpPortNum);
	UNUSED_ARGUMENT(ifaddr);

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		return -1;
	}
	nkn_mark_fd(fd, RTSP_UDP_FD);

	reuse_port = 0;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
			(const char*)&reuse_port, 
			sizeof(reuse_port)) < 0) {
		nkn_close_fd(fd, RTSP_UDP_FD);
		return -1;
	}

	addr = ( ifaddr != NULL ) ? ifaddr->s_addr : INADDR_ANY;
	if (rtcpPortNum != 0) {
		MAKE_SOCKADDR_IN(name, addr, rtcpPortNum);
		if (bind(fd, (struct sockaddr*)&name, sizeof(name)) != 0) {
			nkn_close_fd(fd, RTSP_UDP_FD);
			return -1;
		}
	}

	opts = O_RDWR; //fcntl(fd, F_GETFL);
	opts = (opts | O_NONBLOCK);
	if(fcntl(fd, F_SETFL, opts) < 0)
	{
		return -1;
	}

	return fd;
}

int RTCP_createNew( rtsp_cb_t * prtsp,
		    int streamNum,
		    char * cname,
		    unsigned short serverRTPPort) 
{
	struct streamState * pss;
	struct in_addr ifAddr;
	uint16_t rtcp_udp_port;

	UNUSED_ARGUMENT(cname);

	pss = &prtsp->fStreamStates[streamNum];

	rtcp_udp_port = ntohs(serverRTPPort) + 1;
	ifAddr.s_addr = prtsp->pns->if_addrv4;
	while (1) {
		pss->rtcp_udp_fd = setupDatagramSocket(htons(rtcp_udp_port), &ifAddr);
		if (pss->rtcp_udp_fd != -1) break;
		rtcp_udp_port += 2;
		if (rtcp_udp_port < 1024)
			rtcp_udp_port = 1027;
	}
	pss->rtcpPortNum = htons(rtcp_udp_port);
	pss->startRtcp = FALSE;

	pthread_mutex_lock(&gnm[pss->rtcp_udp_fd].mutex);
	register_NM_socket(pss->rtcp_udp_fd,
			(void *)pss,
			rtcp_epollin,
			rtcp_epollout,
			rtcp_epollerr,
			rtcp_epollhup,
			NULL,
			NULL,
			60,
			USE_LICENSE_FALSE,
			TRUE);
	NM_add_event_epollin(pss->rtcp_udp_fd);
	pthread_mutex_unlock(&gnm[pss->rtcp_udp_fd].mutex);

	return 1;
}


void free_RTCPInstance(struct streamState * pss)
{	
	if (pss->rtcp_udp_fd != -1) {
		// NOTE: unregister_NM_socket(fd, TRUE) deletes the fd from epoll loop.
		pthread_mutex_lock(&gnm[pss->rtcp_udp_fd].mutex);
		unregister_NM_socket(pss->rtcp_udp_fd, TRUE);
		nkn_close_fd(pss->rtcp_udp_fd, RTSP_UDP_FD);
		pthread_mutex_unlock(&gnm[pss->rtcp_udp_fd].mutex);
		pss->rtcp_udp_fd = -1;
	}
}

