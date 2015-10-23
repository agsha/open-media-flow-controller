#include "rtcp_util.h"

#define RTCP_SR_PTYPE 200
#define RTCP_RR_PTYPE 201
#define RTCP_SDES_PTYPE 202
#define RTCP_BYE_PTYPE 203

#define GET_RTCP_RC(x) (*(x) & 0x1F) // x is a ptr to the start of RTCP pkt

////// Functions used in dissecting the RTCP packet {

static uint32_t get16BitNetLen(uint8_t const* p);

void init_rtcp_pkt_dissect(rtcp_pkt_dissect_t* rtcp_dissect) {

	rtcp_dissect->sr = NULL;
	rtcp_dissect->rr_count = 0;
	rtcp_dissect->rr = NULL;
	rtcp_dissect->sdes = NULL;
	rtcp_dissect->bye = NULL;
}


int32_t rtcp_dissector(uint8_t* p, int32_t total, int32_t offset, 
			rtcp_pkt_dissect_t* rtcp_dissect) {
	
	assert(total - offset >= 0);
	if (total - offset == 0) {
		if (rtcp_dissect->sr || rtcp_dissect->rr)
			return 0;
		return -1;
	}
	if (total - offset < 4)
		goto rtcp_error;
	uint8_t x = *(p+offset+1);
	if (x == RTCP_SR_PTYPE) {
		uint32_t len = get16BitNetLen(p+offset+2)*4 + 4;
		rtcp_dissect->rr_count = GET_RTCP_RC(p+offset);
		int32_t tmp_offset = offset;
		if (total - offset < RTCP_SR_ALL_SIZE)
			goto rtcp_error;
		rtcp_dissect->sr = p + offset;
		offset += RTCP_SR_ALL_SIZE;
		// Skipping other possible profile specific extns
		offset += (len - (offset - tmp_offset));
		
		int32_t to_read = 0;
		if (rtcp_dissect->rr_count > 0)
			to_read = RTCP_RECEIVER_INFO_SIZE * rtcp_dissect->rr_count;
		if (total - offset < to_read) 
			goto rtcp_error;
		if (to_read)
			rtcp_dissect->rr = p + offset;
		offset += to_read;
		if (rtcp_dissector(p, total, offset, rtcp_dissect) < 0)
			goto rtcp_error;
	} else if (x == RTCP_RR_PTYPE) {
		uint32_t len = get16BitNetLen(p+offset+2)*4 + 4;
		int32_t tmp_offset = offset;
		if (total - offset < RTCP_RR_ALL_SIZE) 
			goto rtcp_error;
		rtcp_dissect->rr_count = GET_RTCP_RC(p+offset);
		int32_t to_read = 4;
		if (rtcp_dissect->rr_count > 0)
			to_read = 
				RTCP_RR_ALL_SIZE + RTCP_RECEIVER_INFO_SIZE * (rtcp_dissect->rr_count - 1);
		if (total - offset < to_read) 
			goto rtcp_error;
		rtcp_dissect->rr = p + offset;
		offset += to_read;
		// Skipping other possible profile specific extns
		offset += (len - (offset - tmp_offset));
		if (rtcp_dissector(p, total, offset, rtcp_dissect) < 0)
			goto rtcp_error;
	} else if (x == RTCP_BYE_PTYPE) {
		uint32_t len = get16BitNetLen(p+offset+2)*4 + 4;
		if (total - offset < (int32_t)len) 
			goto rtcp_error;
		rtcp_dissect->bye = p + offset;
		offset += len;
		if (rtcp_dissector(p, total, offset, rtcp_dissect) < 0)
			goto rtcp_error;
	} else if (x == RTCP_SDES_PTYPE) {
		uint32_t len = get16BitNetLen(p+offset+2)*4 + 4;
		if (total - offset < (int32_t)len) 
			goto rtcp_error;
		rtcp_dissect->sdes = p + offset;
		offset += len;
		if (rtcp_dissector(p, total, offset, rtcp_dissect) < 0)
			goto rtcp_error;
	} else {
		// Not Handling any APP specific events
		uint32_t len = get16BitNetLen(p+offset+2)*4 + 4;
		if (total - offset < (int32_t)len)
			goto rtcp_error;
		offset += len;
		if (rtcp_dissector(p, total, offset, rtcp_dissect) < 0)
			goto rtcp_error;
	}
	return 0;
rtcp_error:
	init_rtcp_pkt_dissect(rtcp_dissect);
	return -1;
}



static uint32_t get16BitNetLen(uint8_t const* p) {

	uint16_t rc;
	memcpy(&rc, p, 2);
	return ntohs(rc);
}

///////////////// RTCP dissection functions }

