#ifndef _LOAS_PARSER_H_

#define _LOAS_PARSER_H_

uint32_t loas_parse_v1(uint8_t *data, uint32_t pos, uint32_t length);

uint32_t loas_parse(
	uint8_t *data, 
	uint32_t pos, 
	uint32_t payload_len,
	uint32_t *rem_length);


#endif

