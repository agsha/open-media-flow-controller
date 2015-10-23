/* File : rtsp_table.c Author : Patrick Mishel R
 * Copyright (C) 2008 - 2009 Nokeena Networks, Inc. 
 * All rights reserved.
 */

/** 
 * @file rtsp_table.c
 * @brief Lookup Tables for Parsing and generation of messages
 * @author Patrick Mishel R
 * @version 1.00
 * @date 2009-07-15
 */

#include <rtsp.h>
#include <rtsp_table.h>
#include <rtsp_error.h>

#include <stdio.h>
#include <strings.h>
#include <string.h>

#define RTSP_SET_MTHD_MAP(method, name, int_char_map) \
	{ 	\
		.rtsp_method = method, \
		.method_name = name,\
		.method_name_len = sizeof(name) - 1,\
		{\
			.char_map_8 = int_char_map,\
		}\
	}

#define RTSP_MTHD_INDEX_START ((uint32_t)(RTSP_MTHD_DESCRIBE-1))
#define RTSP_MTHD_INDEX_END ((uint32_t)(RTSP_MTHD_TEARDOWN-1))
#define RTSP_MTHD_INDEX(method) ((uint32_t)(method-1))

/** 
 * @brief Method Map Table 
 * Case-sensitive and hence the map (KEY)
 */
static rtsp_method_map_t method_map_table[] = {
	RTSP_SET_MTHD_MAP(RTSP_MTHD_DESCRIBE, "DESCRIBE","DESCRIBE"),
	RTSP_SET_MTHD_MAP(RTSP_MTHD_ANNOUNCE, "ANNOUNCE","ANNOUNCE"),
	RTSP_SET_MTHD_MAP(RTSP_MTHD_GET_PARAMETER, "GET_PARAMETER","GET_PARA"),
	RTSP_SET_MTHD_MAP(RTSP_MTHD_OPTIONS, "OPTIONS","OPTIONS"),
	RTSP_SET_MTHD_MAP(RTSP_MTHD_PAUSE, "PAUSE", "PAUSE"),
	RTSP_SET_MTHD_MAP(RTSP_MTHD_PLAY, "PLAY", "PLAY"),
	RTSP_SET_MTHD_MAP(RTSP_MTHD_RECORD, "RECORD", "RECORD"),
	RTSP_SET_MTHD_MAP(RTSP_MTHD_REDIRECT, "REDIRECT", "REDIRECT"),
	RTSP_SET_MTHD_MAP(RTSP_MTHD_SETUP, "SETUP", "SETUP"),
	RTSP_SET_MTHD_MAP(RTSP_MTHD_SET_PARAMETER, "SET_PARAMETER", "SET_PARA"),
	RTSP_SET_MTHD_MAP(RTSP_MTHD_TEARDOWN, "TEARDOWN", "TEARDOWN"),
	/*RTSP_MTHD_NULL Should be the last Element of the table */
	RTSP_SET_MTHD_MAP(RTSP_MTHD_NULL, "", ""),
};

#define RTSP_SET_HDR_MAP(id, type_mask, name, char_map) \
	{\
		.header_enable = RTSP_HDR_ENABLE,\
		.header_id = id,\
		.header_type_mask = type_mask,\
		.header_name = name,\
		.header_name_len = sizeof(name) - 1 ,\
		{\
			.char_map_8 = char_map,\
		},\
		.compose_header = NULL,\
		.parse_header = NULL,\
	}\

#define RTSP_HDR_INDEX_START ((uint32_t)(RTSP_HDR_ACCEPT-1))
#define RTSP_HDR_INDEX_END ((uint32_t)(RTSP_HDR_WWW_AUTHENTICATE-1))
#define RTSP_HDR_INDEX(header) ((uint32_t)(header-1))

/** 
 * @brief Header Map for Parser
 * Header Comparisions are case-insensitive and hence int-char map(key) is given in lower case
 * All Headers Enabled by default
 */
static rtsp_header_map_t header_map_table[] = {
	RTSP_SET_HDR_MAP(RTSP_HDR_ACCEPT, RTSP_TYPE_REQUEST, "Accept", "accept  "),
    RTSP_SET_HDR_MAP(RTSP_HDR_ACCEPT_ENCODING, RTSP_TYPE_REQUEST, "Accept-Encoding", "accept-e"),
    RTSP_SET_HDR_MAP(RTSP_HDR_ACCEPT_LANGUAGE, RTSP_TYPE_REQUEST, "Accept-Language", "accept-l"),
    RTSP_SET_HDR_MAP(RTSP_HDR_ALLOW, RTSP_TYPE_RESPONSE, "Allow", "allow   "),
    RTSP_SET_HDR_MAP(RTSP_HDR_AUTHORIZATION, RTSP_TYPE_REQUEST, "Authorization", "authoriz"),
    RTSP_SET_HDR_MAP(RTSP_HDR_BANDWIDTH, RTSP_TYPE_REQUEST, "Bandwidth", "bandwidt"),
    RTSP_SET_HDR_MAP(RTSP_HDR_BLOCKSIZE, RTSP_TYPE_REQUEST, "Blocksize", "blocksiz"),
    RTSP_SET_HDR_MAP(RTSP_HDR_CACHE_CONTROL, RTSP_TYPE_GENERAL, "Cache-Control", "cache-co"),
    RTSP_SET_HDR_MAP(RTSP_HDR_CONFERENCE, RTSP_TYPE_REQUEST, "Conference", "conferen"),
    RTSP_SET_HDR_MAP(RTSP_HDR_CONNECTION, RTSP_TYPE_GENERAL, "Connection", "connecti"),
		/*
		 * Key collisions present here for 'content-' 
		 * Should be handled on code using this table
		 */
    RTSP_SET_HDR_MAP(RTSP_HDR_CONTENT_BASE, RTSP_TYPE_ENTITY, "Content-Base", "content-"), 
    RTSP_SET_HDR_MAP(RTSP_HDR_CONTENT_ENCODING, RTSP_TYPE_ENTITY, "Content-Encoding", "content-"),
    RTSP_SET_HDR_MAP(RTSP_HDR_CONTENT_LANGUAGE, RTSP_TYPE_ENTITY, "Content-Language", "content-"),
    RTSP_SET_HDR_MAP(RTSP_HDR_CONTENT_LENGTH, RTSP_TYPE_ENTITY, "Content-Length", "content-"),
    RTSP_SET_HDR_MAP(RTSP_HDR_CONTENT_LOCATION, RTSP_TYPE_ENTITY,"Content-Location", "content-"),
    RTSP_SET_HDR_MAP(RTSP_HDR_CONTENT_TYPE, RTSP_TYPE_RESPONSE | RTSP_TYPE_ENTITY,"Content-Type", "content-"),

    RTSP_SET_HDR_MAP(RTSP_HDR_CSEQ, RTSP_TYPE_GENERAL, "Cseq", "cseq    "),
    RTSP_SET_HDR_MAP(RTSP_HDR_DATE, RTSP_TYPE_GENERAL, "Date", "date    "),
    RTSP_SET_HDR_MAP(RTSP_HDR_EXPIRES, RTSP_TYPE_ENTITY, "Expires", "expires "),
    RTSP_SET_HDR_MAP(RTSP_HDR_FROM, RTSP_TYPE_REQUEST, "From", "from    "),
    RTSP_SET_HDR_MAP(RTSP_HDR_IF_MODIFIED_SINCE, RTSP_TYPE_REQUEST, "If-Modified-Since", "if-modif"),
    RTSP_SET_HDR_MAP(RTSP_HDR_LAST_MODIFIED, RTSP_TYPE_ENTITY, "Last-Modified", "last-mod"),
    RTSP_SET_HDR_MAP(RTSP_HDR_PROXY_AUTHENTICATE, RTSP_TYPE_REQUEST, "Proxy-Authenticate", "proxy-au"),
    RTSP_SET_HDR_MAP(RTSP_HDR_PROXY_REQUIRE, RTSP_TYPE_REQUEST, "Proxy-Require", "proxy-re"),
    RTSP_SET_HDR_MAP(RTSP_HDR_PUBLIC, RTSP_TYPE_RESPONSE,"Public", "public  "),
    RTSP_SET_HDR_MAP(RTSP_HDR_RANGE, RTSP_TYPE_REQUEST | RTSP_TYPE_RESPONSE, "Range", "range   "),
    RTSP_SET_HDR_MAP(RTSP_HDR_REFERER, RTSP_TYPE_REQUEST, "Referer", "referer "),
    RTSP_SET_HDR_MAP(RTSP_HDR_REQUIRE, RTSP_TYPE_REQUEST, "Require", "require "),
    RTSP_SET_HDR_MAP(RTSP_HDR_RETRY_AFTER,RTSP_TYPE_RESPONSE, "Retry-After", "retry   "),
    RTSP_SET_HDR_MAP(RTSP_HDR_RTP_INFO, RTSP_TYPE_RESPONSE, "RTP-Info", "rtp-info"),
    RTSP_SET_HDR_MAP(RTSP_HDR_SCALE, RTSP_TYPE_REQUEST | RTSP_TYPE_RESPONSE, "Scale", "scale   "), 
    RTSP_SET_HDR_MAP(RTSP_HDR_SESSION, RTSP_TYPE_REQUEST | RTSP_TYPE_RESPONSE,"Session", "session "),
    RTSP_SET_HDR_MAP(RTSP_HDR_SERVER, RTSP_TYPE_RESPONSE, "Server", "server  "),
    RTSP_SET_HDR_MAP(RTSP_HDR_SPEED, RTSP_TYPE_REQUEST | RTSP_TYPE_RESPONSE, "Speed", "speed   "),
    RTSP_SET_HDR_MAP(RTSP_HDR_TRANSPORT, RTSP_TYPE_REQUEST | RTSP_TYPE_RESPONSE, "Transport", "transpor"),
    RTSP_SET_HDR_MAP(RTSP_HDR_UNSUPPORTED, RTSP_TYPE_RESPONSE, "Unsupported", "unsuppor"),
    RTSP_SET_HDR_MAP(RTSP_HDR_USER_AGENT, RTSP_TYPE_REQUEST, "User-Agent", "user-age"),
    RTSP_SET_HDR_MAP(RTSP_HDR_VIA, RTSP_TYPE_GENERAL, "Via", "via     "),
    RTSP_SET_HDR_MAP(RTSP_HDR_WWW_AUTHENTICATE, RTSP_TYPE_RESPONSE,"WWW-Authenticate", "www-auth"),

	/* RTSP_HDR_NULL Should be the last Element of the table */
    RTSP_SET_HDR_MAP(RTSP_HDR_NULL, RTSP_TYPE_NULL,"", ""),
};

/** 
 * @brief 	Reports collision on 8Byte key
 * 			Manual Check only
 * 			TODO - Auto Verification of Collision Keys
 * 
 * @param header
 * 
 * @return Collision status
 * 			1 - On Collision
 * 			0 - No Collision
 */
#define IS_COLLISION_8_ON(header) (((header >= RTSP_HDR_CONTENT_BASE) && (header <= RTSP_HDR_CONTENT_TYPE)) ? TRUE : FALSE)

/** 
 * @brief Variables to be initialized after Init is called
 */
static rtsp_method_map_t *p_method_map;
static rtsp_header_map_t *p_header_map;

/** 
 * @brief Register compose_header call back
 * 
 * @param id Header Id
 * @param chdr_cb call back function to register
 * 
 * @return Error code
 * Failure if there is a call back already registered
 */
rtsp_error_t rtsp_table_plugin_chdr_cb(rtsp_header_id_t id,rtsp_chdr_cb_t chdr_cb){

	if(!chdr_cb || !IS_VALID_HEADER(id) || !p_header_map){
		return (RTSP_E_INVALID_ARG);
	}

	if(NULL == p_header_map[RTSP_HDR_INDEX(id)].compose_header){
		p_header_map[RTSP_HDR_INDEX(id)].compose_header = chdr_cb;
		return (RTSP_E_SUCCESS);
	} else {
		ERR_LOG("'compose_header' function already plugged in for header %s", p_header_map[RTSP_HDR_INDEX(id)].header_name);
	}	
	return (RTSP_E_FAILURE);
}

/** 
 * @brief Get compose_header call back
 * 
 * @param id header id
 * 
 * @return function pointer to call back
 */
rtsp_chdr_cb_t rtsp_table_get_chdr_cb(rtsp_header_id_t id){

	if(IS_VALID_HEADER(id) && p_header_map){
		return (p_header_map[RTSP_HDR_INDEX(id)].compose_header);
	} 
	return (NULL);

}

/** 
 * @brief Register parse_header call back
 * 
 * @param id Header Id
 * @param phdr_cb call back function to register
 * 
 * @return Error code
 * Failure if there is a call back already registered
 */
rtsp_error_t rtsp_table_plugin_phdr_cb(rtsp_header_id_t id,rtsp_phdr_cb_t phdr_cb){

	if(!phdr_cb || !IS_VALID_HEADER(id) || !p_header_map){
		return (RTSP_E_INVALID_ARG);
	}

	if(NULL == p_header_map[RTSP_HDR_INDEX(id)].parse_header){
		p_header_map[RTSP_HDR_INDEX(id)].parse_header = phdr_cb;
		return (RTSP_E_SUCCESS);
	} else {
		ERR_LOG("'parse_header' function already plugged in for header %s", p_header_map[RTSP_HDR_INDEX(id)].header_name);
	}	
	return (RTSP_E_FAILURE);
}

/** 
 * @brief Get parse_header call back
 * 
 * @param id header id
 * 
 * @return function pointer to call back
 */
rtsp_phdr_cb_t rtsp_table_get_phdr_cb(rtsp_header_id_t id){

	if(IS_VALID_HEADER(id) && p_header_map){
		return (p_header_map[RTSP_HDR_INDEX(id)].parse_header);
	} 
	return (NULL);
}

/** 
 * @brief Initialize rtsp lookup tables 
 * 
 * @return rtsp_error_t - Error codes
 */
rtsp_error_t rtsp_table_init(void ){

	if(p_method_map){
		fprintf(stderr, "Method Map Already Initialized \n");
	} else {
		fprintf(stderr, "Initializing Method Map Table ...");
			/* 
			 * TODO - Validate method map
			 * Print Collision details 
			 */
		p_method_map = method_map_table;
		fprintf(stderr, "Done\n");
	}

	if(p_header_map){
		fprintf(stderr, "Header Map Already Initialized \n");
	} else {
		fprintf(stderr, "Initializing Header Map Table ...");
			/* 
			 * TODO - Validate header map
			 * Print Collision details 
			 */
		p_header_map = header_map_table;
		fprintf(stderr, "Done\n");
	}
		
	/*
	 * TODO - Initialize other tables here 
	 */

	return (RTSP_E_SUCCESS);
}

#if 0
/** 
 * @brief Api to get Method Map Base
 * 
 * @return Pointer to Method Map Table or NULL on failure
 */
rtsp_method_map_t *rtsp_table_get_hdr_map(void ){
	return (p_method_map); 
}

/** 
 * @brief Api to get Header Map Base
 * 
 * @return Pointer to Header Map Table or NULL on failure
 */
rtsp_header_map_t *rtsp_table_get_mthd_map(void){
	return (p_header_map);
}
#endif 

/**
 * Following APIs that actually do the mapping
 * enum -> string 
 * string -> enum 
 */

/** 
 * @brief Get Method Type from string
 * 
 * @param p_method_ret Returning the method type
 * @param method_string String which will be looked for METHOD in the begining 
 * @param string_len Length of method_string
 * 
 * @return rtsp_error_t
 */
rtsp_error_t rtsp_table_get_method_type(rtsp_method_t *p_method_ret, const char *method_string, int32_t string_len){
	
	uint32_t *p_int_map_4;
	uint32_t method_index = 0;

	if (!p_method_ret || !method_string || !p_method_map){
		return (RTSP_E_INVALID_ARG);
	}

	/* 
	 * Using 4 Character Map for Method Lookup
	 */
	if (string_len < RTSP_CHAR_INT_MAP4_LEN){
		return (RTSP_E_INCOMPLETE);
	}

	p_int_map_4 = (uint32_t *) (&method_string[0]);

	for(method_index = RTSP_MTHD_INDEX_START;  
		p_method_map[method_index].rtsp_method != RTSP_MTHD_NULL; method_index++){
		if(p_method_map[method_index].int_map_4 == *p_int_map_4){
			/* 
			 * There is no Collision in this 4 byte key for Method 
			 * (Manually Verified - TODO Auto verification may be added in init function)
			 * Hence we have reached the desired entry
			 */
			break;
		}
	}

	if(p_method_map[method_index].rtsp_method != RTSP_MTHD_NULL){
		/* 
		 * Valid Entry 
		 * Now validate the whole string 
		 */

		if(string_len >=  p_method_map[method_index].method_name_len){
			if(0 == strncmp(p_method_map[method_index].method_name, 
					method_string, p_method_map[method_index].method_name_len)){
				/* 
			 	* Match Exists 
			 	*/
				*p_method_ret = p_method_map[method_index].rtsp_method;
				return (RTSP_E_SUCCESS);
			}
		} else {
			return (RTSP_E_INCOMPLETE);
		}
	}
	*p_method_ret = RTSP_MTHD_NULL;
	return (RTSP_E_FAILURE);
}


/** 
 * @brief Get pointer to the method_str in method table
 * 
 * @param method Method Type
 * 
 * @return Pointer to the string, Null on failure
 */
const char /*@null@*/*method_str(rtsp_method_t method){
	if((p_method_map != NULL) && IS_VALID_METHOD(method)){
		return ((p_method_map[method-1].method_name));
	}
	return (NULL);
}

/** 
 * @brief Get length of the method_str in the method table
 * 
 * @param method Method Type
 * 
 * @return Length of string. 
 */
int32_t get_method_str_len(rtsp_method_t method){
	if((p_method_map != NULL) && IS_VALID_METHOD(method)){
		return ((p_method_map[method-1].method_name_len));
	}
	return 0;
}

/** 
 * @brief Get pointer to the header_str in header table
 * 
 * @param header Header ID
 * 
 * @return Pointer to the string, Null on failure
 */
const char /*@null@*/*header_str(rtsp_header_id_t header){
	if((p_header_map != NULL) && IS_VALID_HEADER(header)){
		return ((p_header_map[header-1].header_name));
	}
	return (NULL);
}

/** 
 * @brief Get length of the header_str in the header table
 * 
 * @param header Header ID
 * 
 * @return Length of string. 
 */
int32_t get_header_str_len(rtsp_method_t header){
	if((p_header_map != NULL) && IS_VALID_HEADER(header)){
		return ((p_header_map[header-1].header_name_len));
	}
	return 0;
}

/** 
 * @brief Get string from the requested method type. 
 * The string is written in the destination buffer 
 * of given length. If the method string is bigger than 
 * the destination the function returns failure
 * 
 * @param method_dst_str Pointer to the destination string 
 * @param dst_len Length available at the destination string
 * @param method Method to be converted to string 
 * @param p_mthd_str_len Length of the method written to the string
 * 
 * @return Error number
 */
rtsp_error_t rtsp_table_get_method_str(char *method_dst_str, 
		int32_t dst_len, 
		rtsp_method_t method, 
		int32_t *pp_mthd_str_len){

	rtsp_error_t ret_val = RTSP_E_FAILURE;
	uint32_t method_index;

	/* 
	 * pp_mthd_str_len is not validated, it will be returned 
	 * with a value if it is set with a valid pointer
	 */
	if(!p_method_map || !method_dst_str || dst_len == 0 || !IS_VALID_METHOD(method)){
		return (RTSP_E_INVALID_ARG);
	}

	method_index = RTSP_MTHD_INDEX(method);

	if(p_method_map[method_index].method_name_len > dst_len){
		ret_val = RTSP_E_NO_SPACE;
 	} else {
		strncpy(method_dst_str, p_method_map[method_index].method_name,
					p_method_map[method_index].method_name_len);
		if(pp_mthd_str_len){
			*pp_mthd_str_len = p_method_map[method_index].method_name_len;
		}
		ret_val = RTSP_E_SUCCESS;
	}
	return (ret_val);						
}



/** 
 * @brief Get header type from header string
 * case insensitive
 * 
 * @param p_header_ret header id returned
 * @param header_string
 * @param string_len
 * 
 * @return 
 */
rtsp_error_t rtsp_table_get_header_type(rtsp_header_id_t *p_header_ret, 
		const char *header_string, int32_t string_len){

	uint64_t *p_int_map_8;
	char char_map[RTSP_CHAR_INT_MAP8_LEN+1];
	uint32_t header_index = 0;

	if (!p_header_ret || !header_string || !p_header_map){
		return (RTSP_E_INVALID_ARG);
	}

	memset (char_map,0,RTSP_CHAR_INT_MAP8_LEN+1);

	/* 
	 * Using 8 Character Map for Method Lookup
	 * Using different
	 */
	if (string_len < RTSP_CHAR_INT_MAP8_LEN){
		return (RTSP_E_INCOMPLETE);
	}
	int32_t hdr_len = 0;
	const char *cs = header_string, *ce= header_string + string_len;
	while ((cs < ce) && *cs++ != ':')
		;
	hdr_len = (int32_t)(cs-header_string - 1);

	strncpy(char_map, header_string, 
			( RTSP_CHAR_INT_MAP8_LEN > hdr_len ? hdr_len : RTSP_CHAR_INT_MAP8_LEN));
	p_int_map_8 = (uint64_t *) (&char_map[0]);
	/* 
	 * Convert to lower case and compare 
	 * */
	*p_int_map_8 |= (uint64_t)0x2020202020202020ll;

	for(header_index = RTSP_HDR_INDEX_START;  
		p_header_map[header_index].header_id != RTSP_HDR_NULL; header_index++){
		if(p_header_map[header_index].int_map_8 == *p_int_map_8){
			/* 
			 * There is Collision in this 8 byte key for Header
			 */
			if(IS_COLLISION_8_ON(p_header_map[header_index].header_id)){
				/* 
				 * Collision Header encountered
				 * Do strcmp now, (TODO - can be optimized)
				 */
				if(string_len >=  p_header_map[header_index].header_name_len){
					if (0 == strncasecmp(p_header_map[header_index].header_name, 
						header_string, p_header_map[header_index].header_name_len)){
						/* 
					 	* Match found on collision set 
					 	*/
						break;
					} 
				} else {
					return (RTSP_E_INCOMPLETE);
				}

			} else {
				break;
			}
		}
	}

	if(p_header_map[header_index].header_id != RTSP_HDR_NULL){

		if(IS_COLLISION_8_ON(p_header_map[header_index].header_id)){
			/* 
			 * Since Full check is done on Collision Strings 
			 */
			*p_header_ret = p_header_map[header_index].header_id;
			return (RTSP_E_SUCCESS);
		} else {
			if(string_len >=  p_header_map[header_index].header_name_len){
				if(0 == strncasecmp(p_header_map[header_index].header_name, 
					header_string,p_header_map[header_index].header_name_len)){
					*p_header_ret = p_header_map[header_index].header_id;
					return (RTSP_E_SUCCESS);
				}
			} else {
				return (RTSP_E_INCOMPLETE);
			}
		}
	}

	*p_header_ret = RTSP_HDR_NULL;
	return (RTSP_E_FAILURE);
}

/** 
 * @brief Get string from the requested header type. 
 * The string is written in the destination buffer 
 * of given length. If the header string is longer than 
 * the destination the function returns failure
 * 
 * @param header_dst_str Pointer to the destination string 
 * @param dst_len Length available at the destination string
 * @param header Header to be converted to string 
 * @param p_mthd_str_len Length of the header written to the string
 * 
 * @return Error number
 */
rtsp_error_t rtsp_table_get_header_str(char *header_dst_str, 
		int32_t dst_len, 
		rtsp_header_id_t header, 
		int32_t *pp_hdr_str_len){

	rtsp_error_t ret_val = RTSP_E_FAILURE;
	uint32_t header_index;

	/* 
	 * pp_hdr_str_len is not validated, it will be returned 
	 * with a value if it is set with a valid pointer
	 */
	if(!p_header_map || !header_dst_str || dst_len == 0 || !IS_VALID_HEADER(header)){
		return (RTSP_E_INVALID_ARG);
	}

	header_index = RTSP_HDR_INDEX(header);

	if(p_header_map[header_index].header_name_len > dst_len){
		ret_val = RTSP_E_NO_SPACE;
 	} else {
		strncpy(header_dst_str, p_header_map[header_index].header_name,
					p_header_map[header_index].header_name_len);
		if(pp_hdr_str_len){
			*pp_hdr_str_len = p_header_map[header_index].header_name_len;
		}
		ret_val = RTSP_E_SUCCESS;
	}
	return (ret_val);						
}




