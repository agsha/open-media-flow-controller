
#include "common.h"

#include "get_bits.h"

uint32_t loas_parse_v1(uint8_t *data, uint32_t pos, uint32_t length){
	static int32_t rem_length = 0;
	uint32_t init_pos = pos;
	uint8_t data1, data2, data3;

	if(!rem_length){
		data1 = *(data + pos);
		pos++;
		data2 = *(data + pos);
		pos++;
		data3 = *(data + pos);
		pos++;
		assert(data1 == 0x56 && ((data2 & 0xe0) == 0xe0));
		rem_length = (int32_t)((data2 & 0x1f) << 8) | ((int32_t)data3 & 0xff) ;
		assert(rem_length > 0);
		rem_length = rem_length - (length - 3);
	} else{
		rem_length = rem_length - (int32_t)length;
		assert (rem_length >= 0);
	}
	return (pos);
}

#define LOAS_HEADER (0x2b7)
#define LOAS_HDR_MASK (0xffe000)
#define PAYLOAD_LEN_MASK (0x1fff)

uint32_t loas_parse(
	uint8_t *data, 
	uint32_t pos, 
	uint32_t payload_len,
	uint32_t *rem_length){
	
	uint32_t init_pos = pos;
	//uint8_t data1, data2, data3;
	GetBitContext gb;
	uint32_t loas_header;
	
	init_get_bits(&gb, data+pos, payload_len);

	loas_header = get_bits(&gb, 11);
	
	assert(loas_header == LOAS_HEADER);
	*rem_length = get_bits(&gb, 13);
	assert(*rem_length > 0);
	pos += 3; //read 3 bytes
	
	//*rem_length = *rem_length - (length - 3);

	return (pos);
}

