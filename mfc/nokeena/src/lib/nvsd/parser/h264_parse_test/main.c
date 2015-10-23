#include <stdio.h>
#include "string.h"
#include <stdint.h> 
#include "nkn_vpe_h264_parser.h"

static uint32_t parse_TS_header(uint8_t *data, uint32_t pos,
				uint8_t *payload_unit_start_indicator,
				uint16_t* rpid,
				uint8_t *continuity_counter,
				uint8_t *adaptation_field_control); 

static uint32_t parse_PES_header(uint8_t *data,
				 uint32_t pos,
				 uint64_t *PTS_value,
				 uint64_t *DTS_value,
				 uint32_t *payload_pos);

#define VID_PID 256

int32_t main(int argc, char* argv[])
{
  FILE *ts_fin = NULL;
  uint32_t file_size = 0, num_pkts = 0, i = 0;
  uint8_t *data = NULL;
  uint32_t pos = 0, payload_pos = 0, data_pos = 0;
  uint8_t continuity_counter = 0, adaptation_field_control = 0;
  uint8_t payload_unit_start_indicator = 0;
  uint16_t rpid = 0;
  h264_desc_t *h264_desc = NULL;
  uint64_t PTS_value, DTS_value;
  uint32_t temp_pos = 0, size = 0;

  ts_fin = fopen(argv[1], "rb");
  if(ts_fin == NULL) {
    printf("The input ts file not found \n");
  }
  fseek(ts_fin, 0, SEEK_END);
  file_size = ftell(ts_fin);
  fseek(ts_fin, 0, SEEK_SET);
  data = (uint8_t *)malloc(file_size);
  fread(data, file_size, 1, ts_fin);
  num_pkts = file_size / 188;

  h264_desc = (h264_desc_t*)calloc(1, sizeof(h264_desc_t));
  h264_desc->last_data[0] = 0xff;
  h264_desc->last_data[1] = 0xff;
  h264_desc->last_data[2] = 0xff;
  h264_desc->last_data[3] = 0xff;
  for(i=0; i<num_pkts; i++) {
    pos = parse_TS_header(data, pos, &payload_unit_start_indicator,
			  &rpid, &continuity_counter, &adaptation_field_control);
    if (pos == 0) {
      printf("sync byte not found \n");
    }
    if(rpid == VID_PID) {

      if (payload_unit_start_indicator == 1) {
	pos = parse_PES_header(data, pos, &PTS_value, &DTS_value, &payload_pos);
      }
	temp_pos = pos - data_pos;//this number indicates payload offset within the pkt
	size = ((i+1)*188) - pos;
	do{
	  h264_desc->delimiter_found = 0;
	  handle_mpeg4AVC_new(data+pos, &size, h264_desc);
	  pos+=h264_desc->data_offset;//+h264_desc->sc_type;
	  size = ((i+1)*188) - pos;
	}while(h264_desc->delimiter_found == 1);
    }
      pos = data_pos +188;
      data_pos+=188;

  }

  return 0;
}


static uint32_t parse_TS_header(uint8_t *data, uint32_t pos,
				uint8_t *payload_unit_start_indicator,
				uint16_t* rpid,
				uint8_t *continuity_counter,
				uint8_t *adaptation_field_control) {

  uint8_t  sync_byte;
  uint32_t adaptation_field_len = 0;
  uint16_t pid;

  sync_byte = *(data+pos);
  if (sync_byte != 0x47) {
    return -1; 
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


static uint32_t parse_PES_header(uint8_t *data,
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
    return (pos);
  }

  /* pointing to PES header */
  pos += 6;//skip  packet_start_code_prefix; stream_id; PES_packet_length;
  pos += 1; //skip PES_scrambling_control;PES_priority;data_alignment_indicator;copyright;original_or_copy;
  PTS_DTS_flags = (*(data+pos) &0xc0)>>6;
  pos+=1;//skip flag byte
  PES_header_data_length = *(data+pos);
  *payload_pos = PES_header_data_length + pos + 2;// Position[PES_header_data_length]+PES_header_data_length  
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
