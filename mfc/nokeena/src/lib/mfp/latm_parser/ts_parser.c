#include "common.h"

#if defined(CHECK)
#define VPE_ERROR (-1)
#endif

uint32_t parse_TS_header(uint8_t *data, uint32_t pos, 
	uint8_t *payload_unit_start_indicator,
	uint16_t* rpid, 
	uint8_t	*continuity_counter,
	uint8_t *adaptation_field_control) {

    uint8_t  sync_byte;
    uint32_t adaptation_field_len = 0;
    uint16_t pid;

    sync_byte = *(data+pos);
    if (sync_byte != 0x47) {
	printf("sync byte - not found, got %x",sync_byte);
	return 0xffffffff;
    }

    pos++;//skip sync byte
    *payload_unit_start_indicator = ((*(data+pos))&0x40)>>6;
    pid = data[pos]&0x1F;
    pid = (pid<<8)|data[pos+1];
    *rpid = pid;    
    pos += 2;//skip transport_priority,pid
    *adaptation_field_control = ((*(data+pos))&0x30)>>4;
    *continuity_counter = (*(data+pos)) & 0x0f;
    pos++;

    if ((*adaptation_field_control == 0x3) ||
	    (*adaptation_field_control == 0x2)) { 
	adaptation_field_len = *(data+pos);
	pos++;
	pos += adaptation_field_len;
    }
    return(pos);
}


/* 
 * This function parses the PES header for PTS value
 * data -> i/p pointer input data buffer
 * pos ->  i/p buffer start offset
 * PTS_value -> o/p pointer containing PTS value
 */

uint32_t parse_PES_header(uint8_t *data, 
	uint32_t pos, 
	uint64_t *PTS_value,  
	uint64_t *DTS_value,  		
	uint32_t *payload_pos) 
{

    uint32_t temp_pos = 0;
    uint64_t PTS_temp1, PTS_temp2, PTS_temp3, PTS_temp4, PTS_temp5;
    uint64_t DTS_temp1, DTS_temp2, DTS_temp3, DTS_temp4, DTS_temp5;
    uint8_t  PTS_DTS_flags, PES_header_data_length;


    if (!(*(data+pos) == 0 && *(data+pos+1) == 0 && *(data+pos+2) == 1)) {
	/* not PES */
	*PTS_value =  0;
	pos = 0xffffffff;
	return (pos);
    }

    /* pointing to PES header */
    pos += 6;//skip  packet_start_code_prefix; stream_id; PES_packet_length;
    pos += 1; //skip PES_scrambling_control;PES_priority;data_alignment_indicator;copyright;original_or_copy;
    PTS_DTS_flags = (*(data+pos) &0xc0)>>6;
    pos+=1;//skip flag byte
    PES_header_data_length = *(data+pos);
    *payload_pos = PES_header_data_length + pos + 1;   
    pos += 1;//skip to PTS value
    temp_pos = pos;
    temp_pos+=PES_header_data_length;

    if ((PTS_DTS_flags == 0x2)) {
	PTS_temp1 = (((*(data + pos) & 0x0e) >> 1) << 30);
	pos++;
	PTS_temp2 = (((*(data + pos)) & 0xff) << 22);
	pos++;
	PTS_temp3 = ((*(data + pos) & 0xfe) >> 1) << 15;
	pos++;
	PTS_temp4 = (((*(data + pos)) & 0xff) << 7);
	pos++;
	PTS_temp5 = ((*(data+pos) & 0xfe) >> 1);
	pos++;
	*PTS_value = (PTS_temp1 | PTS_temp2 | PTS_temp3 |
		PTS_temp4 | PTS_temp5);
    }

    if (PTS_DTS_flags == 0x3) {
	PTS_temp1 = (((*(data + pos) & 0x0e) >> 1) << 30);
	pos++;
	PTS_temp2 = (((*(data + pos)) & 0xff) << 22);
	pos++;
	PTS_temp3 = ((*(data + pos) & 0xfe) >> 1) << 15;
	pos++;
	PTS_temp4 = (((*(data + pos)) & 0xff) << 7);
	pos++;
	PTS_temp5 = ((*(data + pos) & 0xfe) >> 1);
	pos++;
	*PTS_value = (PTS_temp1 | PTS_temp2 | PTS_temp3 |
		PTS_temp4 | PTS_temp5);
	//	pos += 5;//accounting for DTS. if DTS is required, extract similar to PTS

	DTS_temp1 = (((*(data + pos) & 0x0e) >> 1) << 30);
	pos++;
	DTS_temp2 = (((*(data + pos)) & 0xff) << 22);
	pos++;
	DTS_temp3 = ((*(data + pos) & 0xfe) >> 1) << 15;
	pos++;
	DTS_temp4 = (((*(data + pos)) & 0xff) << 7);
	pos++;
	DTS_temp5 = ((*(data + pos) & 0xfe) >> 1);
	pos++;
	*DTS_value = (DTS_temp1 | DTS_temp2 | DTS_temp3 |
		DTS_temp4 | DTS_temp5);
    }

    pos = temp_pos;
    return(pos);
}

