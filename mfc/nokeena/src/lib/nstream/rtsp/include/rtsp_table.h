/* File : rtsp_table.h Author : Patrick Mishel R
 * Copyright (C) 2008 - 2009 Nokeena Networks, Inc. 
 * All rights reserved.
 */
#ifndef _RTSP_TABLE_H_
#define _RTSP_TABLE_H_
/** 
 * @file rtsp_table.h
 * @brief This file contains structure definitions of tables 
 * `used by RTSP Parser for Mapping Messages from bits to strings and vice versa
 * @author Patrick Mishel R
 * @version 1.00
 * @date 2009-07-15
 */

#include <inttypes.h>
#include "rtsp_def.h"

/*
 * The METHOD Table MAP
 */
#define RTSP_CHAR_INT_MAP4_LEN 4
#define RTSP_CHAR_INT_MAP8_LEN 8

/** 
 * @brief Header compose Call back type definition	
 * 
 * @param send_buf - Buffer to be filled and sent  
 * @param send_buf_size - Size of the buffer 
 * @param used_buf_size - Size of the buffer used after filling up the data
 * @param rtsp_hdr - Header Data to be filled up
 * 
 * @return 
 */
typedef rtsp_error_t (*rtsp_chdr_cb_t)(char *send_buf, int32_t send_buf_size, int32_t *used_buf_size, rtsp_header_t *rtsp_hdr);

typedef rtsp_error_t (*rtsp_phdr_cb_t)(rtsp_header_t *rtsp_hdr, const char *send_buf, int32_t send_buf_size, const char  **endptr);

struct rtsp_method_map{
	rtsp_method_t rtsp_method; 
	char *method_name;
	int32_t method_name_len;
	union {
		char char_map_8[RTSP_CHAR_INT_MAP8_LEN];
		uint32_t int_map_4;
		uint64_t int_map_8;
	};
};

typedef struct rtsp_method_map rtsp_method_map_t;

/* 
 * TODO - Come back and remove this if not necessary
 * This is intended to make header support configurable
 */
enum rtsp_hdr_enable {
	RTSP_HDR_DISABLE = 2,
	RTSP_HDR_ENABLE = 1,
};

typedef enum rtsp_hdr_enable rtsp_hdr_enable_t;

/* 
 * The Header Map
 */
struct rtsp_header_map{
	rtsp_hdr_enable_t header_enable;
	rtsp_header_id_t header_id;
	rtsp_hdr_type_t  header_type_mask;
	char *header_name;
	int32_t header_name_len;
	
	union {
		char char_map_8[RTSP_CHAR_INT_MAP8_LEN];
		uint32_t int_map_4;
		uint64_t int_map_8;
	};
	rtsp_chdr_cb_t compose_header;
	rtsp_phdr_cb_t parse_header;
};

typedef struct rtsp_header_map rtsp_header_map_t;

/* 
 * Exported functions
 */


extern rtsp_error_t rtsp_table_init	(void );

extern rtsp_error_t rtsp_table_get_method_type(rtsp_method_t *p_method_ret, const char *method_string, int32_t string_len);
extern rtsp_error_t rtsp_table_get_method_str(char *method_dst_str, int32_t dst_len, rtsp_method_t method, int32_t *pp_mthd_str_len);
extern int32_t 	get_method_str_len(rtsp_method_t method);

extern rtsp_error_t rtsp_table_get_header_type(rtsp_header_id_t *p_header_ret, const char *header_string, int32_t string_len);
extern rtsp_error_t rtsp_table_get_header_str(char *header_dst_str, int32_t dst_len, rtsp_header_id_t header, int32_t *pp_hdr_str_len);
extern int32_t 	get_header_str_len(rtsp_method_t method);

extern rtsp_error_t rtsp_table_plugin_chdr_cb(rtsp_header_id_t id,rtsp_chdr_cb_t chdr_cb);
extern rtsp_error_t rtsp_table_plugin_phdr_cb(rtsp_header_id_t id,rtsp_phdr_cb_t chdr_cb);

extern rtsp_chdr_cb_t rtsp_table_get_chdr_cb(rtsp_header_id_t id);
extern rtsp_phdr_cb_t rtsp_table_get_phdr_cb(rtsp_header_id_t id);

#endif
