#include "rtp_avc_read.h"
#include "nkn_vpe_bufio.h"

int32_t
get_nal_size( uint8_t *data,uint8_t NALlen)
{
    uint32_t nal_uint;
    uint16_t nal_short;
    uint8_t nal_char;
    int32_t nal_size;
    if(NALlen == 4){
	nal_uint = _read32(data,0);
	nal_size = (int32_t)(nal_uint);
    }
    if(NALlen == 2){
	nal_short = _read16(data,0);
	nal_size = (int32_t)(nal_short);
    }
    if(NALlen == 1){
	nal_char = *data;
	nal_size = (int32_t)(nal_char);
    }
    return nal_size;
}


int32_t 
get_num_NALs_AU(uint8_t* data,uint8_t NALlen,int32_t data_size)
{
    int32_t data_left = data_size,nal_size,num_NALS = 0;
    while(data_left>0){
	if(NALlen ==4){
	    nal_size = _read32(data,0);
	    num_NALS++;
	    data_left-=nal_size+4;
	    if(data_left<0)
		break;
	    data+=nal_size+4;
	}
	else if(NALlen == 2){
            nal_size = _read16(data,0);
            num_NALS++;
            data_left-=nal_size+2;

            if(data_left<0)
                break;

            data+=nal_size+2;
	}
	else{ 
            nal_size = *data;
            num_NALS++;
            data_left-=nal_size+1;

            if(data_left<0)
                break;

            data+=nal_size+1;
	}
    }

    return num_NALS;
}
