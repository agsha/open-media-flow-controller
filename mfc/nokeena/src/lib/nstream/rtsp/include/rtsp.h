/* File : rtsp.h Author : Patrick Mishel R
 * Copyright (C) 2008 - 2009 Nokeena Networks, Inc. 
 * All rights reserved.
 */
#ifndef _RTSP_H_
#define _RTSP_H_

#include <inttypes.h>
#include <rtsp_def.h>
#include <rtsp_error.h>

/** 
 * @brief Structure Definitions for Individual Headers Included
 */
#include "rtsp_header.h" 

/** 
 * @brief RTSP Header Elements
 * Each Header ID Will map to a structure in the union
 * Header type is used to validate headers according to the Grammar
 */
typedef struct rtsp_header_t{ 
	rtsp_hdr_type_t type;
	rtsp_header_id_t id;
	union {
		rtsp_hdr_accept_t accept;
		rtsp_hdr_allow_t allow;
		rtsp_hdr_bandwidth_t bandwidth;
		rtsp_hdr_blocksize_t blocksize;
		rtsp_hdr_cache_control_t cache_control;
		rtsp_hdr_connection_t connection;
		rtsp_hdr_cont_encoding_t content_encoding;
		rtsp_hdr_content_lang_t content_language;
		rtsp_hdr_cont_len_t content_len;
		rtsp_hdr_cont_type_t content_type;
		rtsp_hdr_cseq_t cseq;
		rstp_hdr_RTP_info_t RTP_info;
		rtsp_hdr_session_t session;
		rtsp_hdr_transport_t transport;
		rtsp_hdr_raw_t raw;
	};
}rtsp_header_t;

/** 
 * @brief RTSP Header List 
 */
typedef struct rtsp_hdr_list_t{
	rtsp_header_t header;
	struct rtsp_hdr_list_t *next;
}rtsp_hdr_list_t;

/** 
 * @brief  RTSP Entity Info
 */
typedef struct rtsp_entity{
	/* 
	 * Specify
	 * 1. Application protocol type
	 * 2. Application protocol Data & Size
	 */
	rtsp_app_type_t content_type;
	int32_t len;
	const char *data;

}rtsp_entity_t;

/** 
 * @brief RTSP URL
 */
typedef struct rtsp_url_t {
	rtsp_url_type_t type;
	char host[RTSP_MAX_HOST_LEN + 1];
	uint32_t port;
	char abs_path[RTSP_MAX_ABS_PATH_LEN + 1];
}rtsp_url_t;

/** 
 * @brief RTSP Request
 */
typedef struct rtsp_request_t { /* Request -line */
	rtsp_method_t method;
	rtsp_url_t url;
	/* TODO - Check if we need to support other URI */
}rtsp_request_t;

/** 
 * @brief RTSP Response
 */
typedef struct rtsp_response_t { /* Status-line */
	uint32_t status_code;
	int32_t reason_len;
	const char *reason_phrase;
}rtsp_response_t;

/** 
 * @brief RTSP Start line 
 */
typedef struct rtsp_start_line_t{
	rtsp_version_t version;	
	rtsp_msg_type_t msg_type; 
	union {
		rtsp_request_t request;
		rtsp_response_t response;
	};
}rtsp_start_line_t;

/** 
 * @brief  RTSP Protocol - The MSG - RFC 2326
 */
typedef struct rtsp_msg_t{
	/* Static Quantites in the message */
	rtsp_start_line_t start_line;
	/* Dynamic Quantites in the Message */
	rtsp_hdr_list_t *header_list;
	rtsp_entity_t *entity;
}rtsp_msg_t;


/** 
 * @brief Composer API 
 * 
 * @param rtsp_send_buffer
 * @param rtsp_send_buf_size
 * @param rtsp_used_buf_size
 * @param rtsp_msg
 * 
 * @return 
 */
extern rtsp_error_t rtsp_compose_message (char *rtsp_send_buffer, 
		int32_t rtsp_send_buf_size, 
		int32_t *rtsp_used_buf_size,
		rtsp_msg_t *rtsp_msg);

/** 
 * @brief 
 * 
 * @param p_url Pointer to rtsp_url_t data structure
 * @param str Url string (Null Terminated )
 * @param string_len (Length of url string )
 * 
 * @return 
 */
extern rtsp_error_t get_rtsp_url(rtsp_url_t *p_url, 
		const char *str, 
		int32_t string_len);


/** 
 * @brief Parse start_line
 * 
 * @param p_start_line pointer to start line structure
 * @param rtsp_msg_buffer Message buffer pointer
 * @param rtsp_msg_len Buffer length
 * @param endptr Parsed end
 * 
 * @return  error
 */
rtsp_error_t rtsp_parse_start_line(rtsp_start_line_t *p_start_line, 
		const char *rtsp_msg_buffer, int32_t rtsp_msg_len, const char **endptr);


/** 
 * @brief Parse an RTSP Header and create data structure
 * 
 * @param header allocated header Data struct
 * @param rtsp_msg_buffer RTSP Message buffer
 * @param rtsp_msg_len Length of the buffer
 * @param endptr Buffer pointer returned after successful parsing
 * 
 * @return 
 */
rtsp_error_t rtsp_parse_header(rtsp_header_t *header, 
		const char * rtsp_msg_buffer, int32_t rtsp_msg_len, const char **endptr);

#endif
