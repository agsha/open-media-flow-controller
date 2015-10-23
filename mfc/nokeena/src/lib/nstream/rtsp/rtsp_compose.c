/* File : rtsp_compose.c Author : Patrick Mishel R
 * Copyright (C) 2008 - 2009 Nokeena Networks, Inc. 
 * All rights reserved.
 */
/** 
 * @file rtsp_compose.c
 * @brief Compose/Generate RTSP Message
 * 		rtsp datastructure -> rtsp Message
 * @author Patrick Mishel R
 * @version 1.00
 * @date 2009-07-16
 */

#include <string.h>

#include <rtsp.h>
#include <rtsp_table.h>
#include <rtsp_local.h>

#include "rtsp_compose_header.c"

static inline rtsp_error_t ADD_CRLF(char *buf, int32_t buf_len){
	if(buf_len >= 2){
		*buf++ = '\r';
		*buf++ = '\n';
		return RTSP_E_SUCCESS;
	}
	return RTSP_E_NO_SPACE;
}

static inline rtsp_error_t rtsp_compose_version(char *rtsp_send_buffer, 
		int32_t rtsp_send_buf_size, 
		int32_t *rtsp_used_buf_size, 
		rtsp_version_t rtsp_ver){
	rtsp_error_t rtsp_err = RTSP_E_VER_NOT_SUP;
	switch(rtsp_ver){
		case RTSP_VER_1_00:
			if(rtsp_send_buf_size >= (sizeof("RTSP/1.1")-1)){
				strncpy(rtsp_send_buffer, "RTSP/1.1", (sizeof("RTSP/1.1")-1));
				*rtsp_used_buf_size = (sizeof("RTSP/1.1")-1);
				rtsp_err =  RTSP_E_SUCCESS;
			}	
			break;
		default:
			break;
	}
	return (rtsp_err);
}

static inline rtsp_error_t rtsp_compose_url (char *rtsp_send_buffer, 
		int32_t rtsp_send_buf_size, 
		int32_t *rtsp_used_buf_size,
		rtsp_url_t *rtsp_url){

	rtsp_error_t rtsp_err =  RTSP_E_NO_SPACE;
	int32_t len = 0;

	if ( !rtsp_send_buffer || rtsp_send_buf_size<=0 || !rtsp_used_buf_size || !rtsp_url){
		return (RTSP_E_INVALID_ARG);
	}

	switch (rtsp_url->type){

		case RTSP_NW_STAR:
			if(rtsp_send_buf_size >= (sizeof("*")-1)){
				*rtsp_send_buffer = '*';	
				*rtsp_used_buf_size = 1;
				rtsp_err =  RTSP_E_SUCCESS;
				return (rtsp_err);
			} 
			break;
		case RTSP_NW_TCP:
			if(rtsp_send_buf_size >= (sizeof("rtsp://") - 1)){
				strncpy(rtsp_send_buffer, "rtsp://", (sizeof("rtsp://") -1));
				*rtsp_used_buf_size = (sizeof("rtsp://") - 1);
				rtsp_err =  RTSP_E_SUCCESS;
			}	
			break;
		case RTSP_NW_UDP:
			if(rtsp_send_buf_size >= (sizeof("rtspu://") - 1)){
				strncpy(rtsp_send_buffer, "rtspu://", (sizeof("rtspu://") - 1));
				*rtsp_used_buf_size = (sizeof("rtspu://") - 1);
				rtsp_err =  RTSP_E_SUCCESS;
			}	
			break;
		default:
			rtsp_err = RTSP_E_INVALID_URI;
			break;
	}

	if(rtsp_err != RTSP_E_SUCCESS){
		return (rtsp_err);
	}

	len = strnlen(rtsp_url->host, (RTSP_MAX_HOST_LEN+1));

	if(len == (RTSP_MAX_HOST_LEN+1)){
		/* 
		 * Non null terminated host name
		 * May be junk present
		 */
		ERR_LOG("Incomplete host name");
		return (RTSP_E_INVALID_HOST);
	}

	if((rtsp_send_buf_size - *rtsp_used_buf_size) < len){
		return (RTSP_E_NO_SPACE);
	}

	/* [host] */
	if(RTSP_E_SUCCESS == rtsp_validate_host(rtsp_url->host, len)){
		strncpy(&rtsp_send_buffer[*rtsp_used_buf_size], rtsp_url->host, len);
		(*rtsp_used_buf_size) += len;
	} else {
		return (RTSP_E_INVALID_HOST);
	}

	/* [:port] */
	if(rtsp_url->port){
		len = snprintf(&rtsp_send_buffer[*rtsp_used_buf_size], (rtsp_send_buf_size - *rtsp_used_buf_size), 
			":%u", rtsp_url->port);
		if((len < 0 ) || ((len == (rtsp_send_buf_size - *rtsp_used_buf_size)) && (rtsp_send_buffer[len]!='\0'))){
			return (RTSP_E_NO_SPACE);
		}
		(*rtsp_used_buf_size) += len;
		
	}

	/* [abs_path] */
	if(rtsp_url->abs_path[0]!='\0'){

		len = strnlen(rtsp_url->abs_path, (RTSP_MAX_ABS_PATH_LEN + 1));
		if(len == (RTSP_MAX_HOST_LEN + 1)){
			ERR_LOG("Incomplete abs_path");
			return (RTSP_E_INVALID_URI);
		}

		if((rtsp_send_buf_size - *rtsp_used_buf_size) < len){
			return (RTSP_E_NO_SPACE);
		}

		/* [abs_path] */
		strncpy(&rtsp_send_buffer[*rtsp_used_buf_size], rtsp_url->abs_path, len);
		(*rtsp_used_buf_size) += len;
	}
	return (RTSP_E_SUCCESS);
}

static inline rtsp_error_t rtsp_compose_start_line(char *rtsp_send_buffer, 
		int32_t rtsp_send_buf_size,
		int32_t *rtsp_used_buf_size, 
		rtsp_start_line_t *rtsp_start_line){

	rtsp_error_t rtsp_err = RTSP_E_FAILURE;
	int32_t buf_used_len = 0;
	int32_t total_used_buf_len = 0;

	if ( !rtsp_send_buffer || rtsp_send_buf_size<=0 || !rtsp_used_buf_size || !rtsp_start_line){
		return (RTSP_E_INVALID_ARG);
	}

	switch(rtsp_start_line->msg_type){
		case RTSP_MSG_REQUEST:
				/* Method */
			rtsp_err = rtsp_table_get_method_str(rtsp_send_buffer, 
				rtsp_send_buf_size, rtsp_start_line->request.method, &buf_used_len);
			if (rtsp_err != RTSP_E_SUCCESS){
				break;
			}
			total_used_buf_len += buf_used_len;
				/* SP */
			if((total_used_buf_len + 1) <= rtsp_send_buf_size){
				rtsp_send_buffer[total_used_buf_len++] = ' ';
			} else {
				rtsp_err = RTSP_E_NO_SPACE;
				break;
			}
				/* rtsp_URL */
			rtsp_err = rtsp_compose_url (&rtsp_send_buffer[total_used_buf_len], 
					(rtsp_send_buf_size - total_used_buf_len), &buf_used_len, &rtsp_start_line->request.url);
			if (rtsp_err != RTSP_E_SUCCESS){
				break;
			}
			total_used_buf_len += buf_used_len;

				/* SP */
			if((total_used_buf_len + 1) <= rtsp_send_buf_size){
				rtsp_send_buffer[total_used_buf_len++] = ' ';
			} else {
				rtsp_err = RTSP_E_NO_SPACE;
				break;
			}
			
				/* Version */
			rtsp_err = rtsp_compose_version (&rtsp_send_buffer[total_used_buf_len], 
					(rtsp_send_buf_size - total_used_buf_len), &buf_used_len, rtsp_start_line->version);
			if (rtsp_err != RTSP_E_SUCCESS){
				break;
			}
			total_used_buf_len += buf_used_len;
			break;
		case RTSP_MSG_RESPONSE:
			rtsp_err = RTSP_E_NOT_IMPLEMENTED;
			break;
		case RTSP_MSG_INVALID:
		default:
			break;
	}

	if(rtsp_err == RTSP_E_SUCCESS){
		*rtsp_used_buf_size = total_used_buf_len;
	}

	return (rtsp_err);
}

static inline rtsp_error_t rtsp_compose_header(char *rtsp_send_buf,
		int32_t rtsp_send_buf_size,
		int32_t *rtsp_used_buf_size,
		rtsp_header_t *rtsp_header){

	rtsp_error_t rtsp_err = RTSP_E_FAILURE;
	int32_t used_buf_len = 0;
	int32_t total_used_buf_len = 0;
	rtsp_chdr_cb_t compose_hdr_cb = NULL;

	if ( !rtsp_send_buf || rtsp_send_buf_size<=0 || !rtsp_used_buf_size || !rtsp_header){
		return (RTSP_E_INVALID_ARG);
	}

	if(rtsp_header->id == RTSP_HDR_RAW){
		used_buf_len = snprintf(rtsp_send_buf, rtsp_send_buf_size,"%.*s",rtsp_header->raw.len,  rtsp_header->raw.data);
		if(used_buf_len == rtsp_send_buf_size && rtsp_send_buf[used_buf_len] == '\0'){
			return (RTSP_E_NO_SPACE);
		} else {
			*rtsp_used_buf_size = used_buf_len;
			return (RTSP_E_SUCCESS);
		}
	}
	
	rtsp_err = rtsp_table_get_header_str(rtsp_send_buf, rtsp_send_buf_size, rtsp_header->id, &used_buf_len);
	if(rtsp_err != RTSP_E_SUCCESS){
		return rtsp_err;
	}

	total_used_buf_len += used_buf_len;
	used_buf_len = 0;

	if((total_used_buf_len + 1) <= rtsp_send_buf_size){
		rtsp_send_buf[total_used_buf_len++] = ':';
	} else {
		return (RTSP_E_NO_SPACE);
	}

	compose_hdr_cb = rtsp_table_get_chdr_cb(rtsp_header->id);
	if(compose_hdr_cb == NULL){
		return (RTSP_E_NOT_IMPLEMENTED);
	}
	
	rtsp_err = (*compose_hdr_cb)(&rtsp_send_buf[total_used_buf_len],(rtsp_send_buf_size-total_used_buf_len), &used_buf_len,  rtsp_header);
	if(rtsp_err != RTSP_E_SUCCESS){
		return rtsp_err;
	}
	total_used_buf_len += used_buf_len;
	used_buf_len = 0;


	*rtsp_used_buf_size = total_used_buf_len;
	
	return (rtsp_err);
}
		
static inline rtsp_error_t rtsp_compose_entity(char *rtsp_send_buf,
		int32_t rtsp_send_buf_size,
		int32_t *rtsp_used_buf_size,
		rtsp_entity_t *rtsp_entity){
	rtsp_error_t rtsp_err = RTSP_E_FAILURE;
	return (rtsp_err);
}

extern rtsp_error_t rtsp_compose_message (char *rtsp_send_buffer, 
		int32_t rtsp_send_buf_size, 
		int32_t *rtsp_used_buf_size,
		rtsp_msg_t *rtsp_msg){

	rtsp_error_t rtsp_err = RTSP_E_FAILURE;

	int32_t used_buf_size = 0;
	int32_t total_used_buf_size = 0;

	rtsp_hdr_list_t *hdr_list = NULL;

	/* Arg Validation */
	if ( !rtsp_send_buffer || rtsp_send_buf_size<=0 || !rtsp_used_buf_size || !rtsp_msg){
		return (RTSP_E_INVALID_ARG);
	}

	/* Compose StartLine */
	rtsp_err = rtsp_compose_start_line(rtsp_send_buffer, rtsp_send_buf_size, &used_buf_size, &rtsp_msg->start_line);


	/* Compose headers */
	hdr_list = rtsp_msg->header_list;
	while ((rtsp_err == RTSP_E_SUCCESS) && hdr_list) {
		total_used_buf_size += used_buf_size;
		used_buf_size = 0;
		/* Add CRLF */
		rtsp_err = ADD_CRLF(&rtsp_send_buffer[total_used_buf_size], rtsp_send_buf_size-total_used_buf_size);
		if(rtsp_err != RTSP_E_SUCCESS)
			break;
		total_used_buf_size += 2;
		rtsp_err = rtsp_compose_header(&rtsp_send_buffer[total_used_buf_size], 
				(rtsp_send_buf_size-total_used_buf_size), &used_buf_size, &hdr_list->header);
		hdr_list=hdr_list->next;
	}

	/* Compose entity */
	if((rtsp_err == RTSP_E_SUCCESS) && rtsp_msg->entity){
		/* Add CRLF */
		total_used_buf_size += used_buf_size;
		used_buf_size = 0;
		rtsp_err = ADD_CRLF(&rtsp_send_buffer[total_used_buf_size], rtsp_send_buf_size-total_used_buf_size);
		if(rtsp_err == RTSP_E_SUCCESS){
			total_used_buf_size += 2;
			rtsp_err = rtsp_compose_entity(&rtsp_send_buffer[total_used_buf_size], 
				(rtsp_send_buf_size-total_used_buf_size), &used_buf_size, rtsp_msg->entity);
		}
	}

	if(rtsp_err == RTSP_E_SUCCESS){
		total_used_buf_size += used_buf_size;
		used_buf_size = 0;
		/* Add CR-LF */
		rtsp_err = ADD_CRLF(&rtsp_send_buffer[total_used_buf_size], rtsp_send_buf_size-total_used_buf_size);
		if(rtsp_err == RTSP_E_SUCCESS)
			total_used_buf_size += 2;
		/* Add CR-LF */
		rtsp_err = ADD_CRLF(&rtsp_send_buffer[total_used_buf_size], rtsp_send_buf_size-total_used_buf_size);
		if(rtsp_err == RTSP_E_SUCCESS)
			total_used_buf_size += 2;
	}
	
	if(rtsp_err == RTSP_E_SUCCESS){
		*rtsp_used_buf_size = total_used_buf_size;
	}
	/* Handle Errors */
	return (rtsp_err);
}


/** 
 * @brief Registers defined Call backs for Composing Headers
 * 
 * @return error codes
 */
extern rtsp_error_t rtsp_chdr_cb_init( void ) {

	rtsp_error_t err = RTSP_E_FAILURE;

	err = rtsp_table_plugin_chdr_cb(RTSP_HDR_CONNECTION,rtsp_chdr_connection);
	if(err != RTSP_E_SUCCESS){
		return err;
	}

	err = rtsp_table_plugin_chdr_cb(RTSP_HDR_CSEQ,rtsp_chdr_cseq);
	if(err != RTSP_E_SUCCESS){
		return err;
	}

	err = rtsp_table_plugin_chdr_cb(RTSP_HDR_ACCEPT,rtsp_chdr_accept);
	if(err != RTSP_E_SUCCESS){
		return err;
	}

	err = rtsp_table_plugin_chdr_cb(RTSP_HDR_TRANSPORT,rtsp_chdr_transport);
	if(err != RTSP_E_SUCCESS){
		return err;
	}

	err = rtsp_table_plugin_chdr_cb(RTSP_HDR_SESSION,rtsp_chdr_session);
	if(err != RTSP_E_SUCCESS){
		return err;
	}
	/* Add header compose functions to plugin here */

	return err;
}

