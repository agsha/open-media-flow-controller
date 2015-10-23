#include "nkn_vpe_mpeg2ps_parser.h"

/*
  next_start_code()
  sequence_header()
  if ( nextbits() == extension_start_code ) {
  sequence_extension()
  do {
  extension_and_user_data( 0 )
  do {
  if (nextbits() == group_start_code) {
  group_of_pictures_header()
  extension_and_user_data( 1 )
  }
  picture_header()
  picture_coding_extension()
  extensions_and_user_data( 2 )
  picture_data()
  } while ( (nextbits() == picture_start_code) ||
  (nextbits() == group_start_code) )
  if ( nextbits() != sequence_end_code ) {
  sequence_header()
  sequence_extension()
  }
  } while ( nextbits() != sequence_end_code )
*/

#define _read_32bits(data) (data<<24)|(data[1]<<16)|(data[2]<<8)|(data[3]) 
#define _read_24bits(data) (data[0]<<16)|(data[1]<<8)|(data[2]) 


uint32_t handle_mpeg2(uint8_t* data, uint32_t *input_size){
    uint32_t data_left=MPEG2_TS_PKT_SIZE-(*input_size),pic_type=0;
    uint8_t no_start_code_flag=0,picture_coding_type=250;
    /*Parse out packet such to and make I/Non-I/Unknown decision*/

    /*Pointer and return logic here*/

    /*Video Parsing Logic*/
    while(data_left>0){
	while( (_read_24bits(data)) != 0x000001){
	    data++;	
	    data_left-=1;
	    if(data_left==2){
		no_start_code_flag=1;
		break;
	    }
	}
	if(no_start_code_flag)
	    return 0; //Say unknown frame type. 
	data+=3; 
	data_left-=3;
	if(data_left<2)
	    return 0;
	switch(*data){
	    //case SEQUENCE_HEADER_CODE:
	    //	sequence_header(data,&data_left);
	    //case GROUP_START_CODE:
	    case PICTURE_START_CODE:
		data++; //PICTURE_START_CODE
		data+=1;//offset
		picture_coding_type=(data[0]>>3)&0x07;
		break;
	    default:
		break;
	}
	if(picture_coding_type<7)
	    break;
    }
    switch(picture_coding_type){
	case 1:
	    pic_type=1;
	    break;
	case 2:
	case 3:
	    pic_type=2;
	    break;
	default:
	    pic_type=0;
	    break;
    }
    return pic_type;
}
