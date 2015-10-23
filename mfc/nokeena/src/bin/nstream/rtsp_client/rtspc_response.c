/* File : rtspc_response.c Author : Patrick Mishel R
 * Copyright (C) 2008 - 2009 Nokeena Networks, Inc. 
 * All rights reserved.
 */

/** 
 * @file rtspc_response.c
 * @brief Response handlers 
 * @author Patrick Mishel R
 * @version 1.00
 * @date 2009-08-12
 */

#include <rtsp_session.h>
#include <rtspc_conf.h>
#include <string.h>
#include <stdlib.h>


/** 
 * @brief Parses startlines(mostly status line) and headers read at the response buffer
 * 
 * @param sess - Session Context
 * 
 * @return 
 */
rtsp_error_t rtsp_res_parse_hdrs(rtsp_session_t *sess){

	const char *end = NULL;
	const char *scan = NULL;
	int hidx = 0;
	rtsp_error_t err = RTSP_E_FAILURE;

	if(!sess){
		return (RTSP_E_INVALID_ARG);
	}

	scan = sess->response_buffer;
	end = sess->response_buffer + sess->used_res_buf;
	/*
	 * Initialize Response
	 */
	sess->response.header_list = NULL;
	sess->response.entity = NULL;
	sess->res_entity_len = 0;

	err = rtsp_parse_start_line(&sess->response.start_line, scan, (end-scan), &scan);
	if(RTSP_E_SUCCESS != err){
		ERR_LOG("Parsing Start-Line Failed");
		return (err);
	}

	for(hidx= 0; (scan < end); hidx++){
		if(hidx >= sess->res_hdr_count){
			ERR_LOG("Insufficient Header Memory");
			return (RTSP_E_FAILURE);
		}

		err = rtsp_parse_header( &sess->res_hdrs[hidx].header, scan, (end-scan), &scan);
		if(RTSP_E_SUCCESS != err){
			ERR_LOG("Parsing Header {%d} Failed", hidx+1);
			return(err);
		}

		/* Terminating Header List */
		sess->res_hdrs[hidx].next = NULL;

		if(hidx > 0){
			/* Linking Headers */
			sess->res_hdrs[hidx-1].next = &sess->res_hdrs[hidx];
		} else {
			sess->response.header_list = &sess->res_hdrs[hidx];
		}

		if(sess->res_hdrs[hidx].header.id == RTSP_HDR_CONTENT_LENGTH){
			/* Content present after header data
			 * TODO - Validation As per RFC
			 */
			sess->res_entity_len = sess->res_hdrs[hidx].header.content_len.bytes;
		}
	}
	return (RTSP_E_SUCCESS);
}

/* Response Handling functions */

rtsp_error_t res_DESCRIBE(rtsp_session_t *sess){
	if(!sess){
		return (RTSP_E_INVALID_ARG);
	}
	/* Copy SDP Data */
	if(sess->sdp){
		if(sess->response.entity->len < sess->sdp_size){
			memcpy(sess->sdp, sess->response.entity->data, sess->response.entity->len);
			sess->used_sdp_size = sess->response.entity->len;
			return (RTSP_E_SUCCESS);
		} else {
			ERR_LOG("Memory not available to copy SDP Data ");
		}
	}
	ERR_LOG("Memory not allocated to copy SDP Data ");
	return (RTSP_E_FAILURE);
}

rtsp_error_t res_SETUP_audio(rtsp_session_t *sess){
	rtsp_hdr_list_t *hdr = NULL;
	if(!sess){
		return (RTSP_E_INVALID_ARG);
	}
	if(!sess->sess_id){
		hdr = sess->response.header_list;
		while(hdr && (hdr->header.id != RTSP_HDR_SESSION)){
			hdr = hdr->next;
		}
		if(hdr){
			/* Got Session id */
			if(hdr->header.session.id_len > 0){
				sess->sess_id = (char *)malloc(hdr->header.session.id_len +1);
				if(!sess->sess_id){
					ERR_LOG("Cannot Allocate Memory");
					return (RTSP_E_FAILURE);
				}
				memcpy(sess->sess_id, hdr->header.session.id, hdr->header.session.id_len);
				sess->sess_id_len = hdr->header.session.id_len;
				sess->sess_id[sess->sess_id_len] = '\0';
			}
		}
	}

	return (RTSP_E_SUCCESS);
	
}

rtsp_error_t res_SETUP_video(rtsp_session_t *sess){
	rtsp_hdr_list_t *hdr = NULL;
	if(!sess){
		return (RTSP_E_INVALID_ARG);
	}

	if(!sess->sess_id){
		hdr = sess->response.header_list;
		while(hdr && (hdr->header.id != RTSP_HDR_SESSION)){
			hdr = hdr->next;
		}
		if(hdr){
			/* Got Session id */
			if(hdr->header.session.id_len > 0){
				sess->sess_id = (char *)malloc(hdr->header.session.id_len);
				if(!sess->sess_id){
					ERR_LOG("Cannot Allocate Memory");
					return (RTSP_E_FAILURE);
				}
				memcpy(sess->sess_id, hdr->header.session.id, hdr->header.session.id_len);
			}
		}
	}
	return (RTSP_E_SUCCESS);
}

rtsp_error_t res_PLAY(rtsp_session_t *sess){
	if(!sess){
		return (RTSP_E_INVALID_ARG);
	}
	return (RTSP_E_SUCCESS);
}

rtsp_error_t res_TEARDOWN(rtsp_session_t *sess){
	if(!sess){
		return (RTSP_E_INVALID_ARG);
	}
	return (RTSP_E_SUCCESS);
}

rtsp_error_t res_handle(rtsp_session_t *sess){

	rtsp_error_t err = RTSP_E_FAILURE;

	if(!sess){
		return(RTSP_E_INVALID_ARG);
	}

	if(sess->msg_num > 5){
		return (RTSP_E_FAILURE);
	}
	switch(sess->msg_num){
		case 1:
			err = res_DESCRIBE(sess);
			break;
		case 2:
			err = res_SETUP_audio(sess);
			break;
		case 3:
			err = res_SETUP_video(sess);
			break;
		case 4:
			err = res_PLAY(sess);
			break;
		case 5:
			err = res_TEARDOWN(sess);
		default:
			break;
	}

	if(err != RTSP_E_SUCCESS)
		return (err);
	return(err);
}
