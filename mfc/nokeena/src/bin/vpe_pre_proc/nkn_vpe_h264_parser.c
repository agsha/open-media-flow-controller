#include "nkn_vpe_h264_parser.h"
#include <stdlib.h>

uint32_t handle_mpeg4AVC(uint8_t* data, uint32_t *input_size){
    uint32_t slice_type,nal_size,inp_size;
    uint8_t *nal_data;
    int nal_type,sc_type;
    inp_size=*input_size;
    /**/
    sc_type = find_start_code(data);
    data+=sc_type;
    *input_size=(*input_size)+sc_type;
    nal_size = get_nal_mem(data, 188-(*input_size));
    nal_data = data;
    data+=nal_size;
    *input_size=(*input_size)+nal_size;
	
    nal_type = get_nal_type(nal_data, nal_size);

    switch(nal_type){
	case AVC_NALU_IDR_SLICE:
	    slice_type= 1;
	    break;
	case AVC_NALU_NON_IDR_SLICE:
	    slice_type= 2;
	    break;
	default:
	    slice_type= 0;
	    break;
    }

    return slice_type;
}

int find_start_code(uint8_t *data)
{
    uint8_t b1, b2, b3, b4;

    b1=*data;
    b2=data[1];
	
    if(b1==0 && b2==0){
	b3=data[2];
	if(b3==1) return 3;
	b4=data[3];
	if(b4==1) return 4;
    }
    return -1;
}

int get_nal_type(uint8_t *nal_data, int nal_size)
{

    uint8_t *p_nal_data;
    int nal_type;

    nal_type = -1;
    p_nal_data = nal_data;
    nal_size = nal_size;
	
    nal_type = p_nal_data[0] & 0x1F;

    if(nal_type ==AVC_NALU_SVC_PACKET || nal_type == AVC_NALU_SVC_SLICE){
	return -1;
    }

    return nal_type;
}


int get_nal_mem(uint8_t *p_data, uint32_t input_size)
{

    uint8_t *buf;
    uint32_t start, end, bytes_processed;
    uint32_t packet_delimiter, curr_pos, data_boundary;
    uint8_t *p_buf, *p_end;
	
	
    start = 0;
    end = 0;
    bytes_processed = 0;
    data_boundary = 0;
    packet_delimiter = 0xffffffff;
    p_end = p_buf = buf = NULL;
    buf = p_data;
	
    curr_pos = start = 0;//iof->nkn_tell(in_desc);
	
	
    do{

	if(bytes_processed%NAL_BUFFER_SIZE == 0){
	    int bytes_to_read;
	    if((curr_pos+NAL_BUFFER_SIZE) < input_size){
		bytes_to_read = NAL_BUFFER_SIZE;
								
	    }else{
		bytes_to_read = input_size - curr_pos;
		data_boundary = input_size-curr_pos;
	    }
	    p_buf = buf;
	    buf+=bytes_to_read;
	}

	packet_delimiter = ((p_buf[0]) << 24) | ((p_buf[1]) << 16) |
	    ((p_buf[2]) << 8) | ((p_buf[3]));
	bytes_processed+=1;
	p_buf++;
	data_boundary--;
	if(packet_delimiter == 0x00000001){//0x00000001 end code
	    end = start+(bytes_processed+3)-4;
	}
	if( (packet_delimiter & 0xffffff00) == 0x00000001){//0x000001 end code
	    end = start+(bytes_processed+2)-3;
	}
    }while(!end && data_boundary);

    end = end + (end==0)*input_size;
    return end-start;
}
	
