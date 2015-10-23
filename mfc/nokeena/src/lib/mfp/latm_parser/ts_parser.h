#ifndef _TS_PARSER_H_
#define _TS_PARSER_H_

uint32_t parse_TS_header(uint8_t *data, uint32_t pos, 
	uint8_t *payload_unit_start_indicator,
	uint16_t* rpid, 
	uint8_t	*continuity_counter,
	uint8_t *adaptation_field_control) ;


uint32_t parse_PES_header(uint8_t *data, 
	uint32_t pos, 
	uint64_t *PTS_value,  
	uint64_t *DTS_value,  		
	uint32_t *payload_pos);


#endif 


