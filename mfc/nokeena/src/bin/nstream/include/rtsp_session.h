/* File : rtsp_session.h Author : Patrick Mishel R
 * Copyright (C) 2008 - 2009 Nokeena Networks, Inc. 
 * All rights reserved.
 */

#ifndef _RTSP_SESSION_H_
/** 
 * @file rtsp_session.h
 * @brief 
 * @author Patrick Mishel R
 * @version 1.00
 * @date 2009-08-06
 */
#define _RTSP_SESSION_H_


#include <rtsp.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

/** 
 * @brief 
 *  Configuration for session on init
 *  Array of const configuration allows
 *  different profile to be selected
 *  Any hard limits for rtsp session 
 *  should fall into this structure
 */
typedef struct rtsp_ses_conf_t{
	uint32_t conf_id;
	uint32_t url_len; 
	uint32_t sdp_size;
	uint32_t req_buf_size;
	uint32_t res_buf_size;
	uint32_t req_hdr_count;
	uint32_t res_hdr_count;
	uint32_t scr_pad_size;
}rtsp_ses_conf_t;

typedef enum rdwr_state_t {
	RTSP_INVALID = 0x00,
	RTSP_INIT,
	RTSP_WAIT_CMD,
	RTSP_WRITE_REQ,
	RTSP_READ_HDRS,
	RTSP_READ_ENTITY,
	RTSP_PROCESS_DATA,
	RTSP_ERR_EXIT,
	RTSP_EXIT
}rdwr_state_t;

/** 
 * @brief rtsp session context
 */
typedef struct rtsp_session_t {
	/* FIXME
	 * Consider locking if context is going to 
	 * be serviced in different threads
	 */
	
	/* 
	 * rtsp://url
	 * TODO : Remove *url if rtsp_url suffices
	 */

	char *url;
	int32_t url_len;
	rtsp_url_t rtsp_url;
	 
	char *sdp;
	int32_t sdp_size;
	int32_t used_sdp_size;

	/* RTSP Session ID */
	char *sess_id;
	int32_t sess_id_len;

	/*
	 * Request Msg and associated data
	 */

	rtsp_msg_t request;
	char *request_buffer;
	int32_t req_buf_size;

	int32_t used_req_buf;
	int32_t sent_bytes;

	int32_t msg_num;
	/* 
	 * Response Msg and associated data
	 */
		
	rtsp_msg_t response;
	char *response_buffer; 
	int32_t res_buf_size;
	int32_t used_res_buf;
	int32_t res_entity_len;

	/* 
	 * Request Header memory 
	 */
	rtsp_hdr_list_t *req_hdrs;
	uint32_t req_hdr_count;
	rtsp_entity_t req_entity;

	/* 
	 * Response Header memory 
	 */
	rtsp_hdr_list_t *res_hdrs;
	uint32_t res_hdr_count;
	rtsp_entity_t res_entity;

	/* 
	 * Scratch pad to avoid temporary memory allocations
	 */
	char *scratch_pad; 
	int32_t scr_pad_size;
	/* Identify memory courrption if 
	 * scratch_pad is used 
	 * reset length to 0 if finished using
	 */
	uint32_t scr_used_len; 
	
	/* 
	 * Network Parameters 
	 * sfd  - Socket File Desc
	 */
	
	int sfd;
	rdwr_state_t rw_state;
	/* Limits configuration */
	const rtsp_ses_conf_t *limits;

}rtsp_session_t;


#endif 
