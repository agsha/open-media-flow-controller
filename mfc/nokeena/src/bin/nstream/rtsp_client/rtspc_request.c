/* File : rtspc_request.c Author : Patrick Mishel R
 * Copyright (C) 2008 - 2009 Nokeena Networks, Inc. 
 * All rights reserved.
 */

/** 
 * @file rtspc_request.c
 * @brief Client Request messages
 * 		In summary forming messages for the following requests
 * 		DESCRIBE
 * 		SETUP
 * 		PLAY
 * 		PAUSE
 * 		TEARDOWN
 * @author Patrick Mishel R
 * @version 1.00
 * @date 2009-08-07
 */

#include <rtsp_session.h>
#include <rtspc_conf.h>
#include <string.h>
#include <ctype.h>

static rtsp_error_t init_req_line(rtsp_session_t *sess){
	rtsp_msg_t *msg = NULL;
	if(!sess) {
		return (RTSP_E_INVALID_ARG);
	}

	msg = &sess->request;
	memset(msg, 0 , sizeof(rtsp_msg_t)); 
	msg->start_line.version = RTSP_VER_1_00;
	msg->start_line.msg_type = RTSP_MSG_REQUEST;
	return (RTSP_E_SUCCESS);
}

static inline int init_req_hdr(rtsp_session_t *sess){
	if(sess){
		sess->request.header_list = NULL;
		return (RTSP_E_SUCCESS);
	}
	return(RTSP_E_FAILURE);
}

static inline int link_header(rtsp_hdr_list_t *head, rtsp_hdr_list_t *next){
	if(head && next){
		head->next = next;
		return (RTSP_E_SUCCESS);
	}
	ERR_LOG("Append hdr failure");
	return(RTSP_E_FAILURE);
}

static inline int init_req_hdr_cseq(rtsp_session_t *sess){
	if(sess && sess->limits){
		if(REQ_HDR_IDX_CSEQ < sess->limits->req_hdr_count){
			sess->req_hdrs[REQ_HDR_IDX_CSEQ].header.type = RTSP_TYPE_REQUEST;
			sess->req_hdrs[REQ_HDR_IDX_CSEQ].header.id = RTSP_HDR_CSEQ;
			sess->req_hdrs[REQ_HDR_IDX_CSEQ].header.cseq.seq_num = 0;
			sess->request.header_list = &sess->req_hdrs[REQ_HDR_IDX_CSEQ];
			return (RTSP_E_SUCCESS);
		}
	}
	return(RTSP_E_FAILURE);
}

static inline int init_req_hdr_accept(rtsp_session_t *sess){
	if(sess && sess->limits){
		if(REQ_HDR_IDX_ACCEPT < sess->limits->req_hdr_count){
			sess->req_hdrs[REQ_HDR_IDX_ACCEPT].header.type = RTSP_TYPE_REQUEST;
			sess->req_hdrs[REQ_HDR_IDX_ACCEPT].header.id = RTSP_HDR_ACCEPT;
			sess->req_hdrs[REQ_HDR_IDX_ACCEPT].header.accept.app_type = RTSP_APP_TYPE_SDP;
			return (RTSP_E_SUCCESS);
		}
	}
	return(RTSP_E_FAILURE);
}

static inline int init_req_hdr_session(rtsp_session_t *sess){
	if(sess && sess->limits){
		if(REQ_HDR_IDX_ACCEPT < sess->limits->req_hdr_count){
			sess->req_hdrs[REQ_HDR_IDX_ACCEPT].header.type = RTSP_TYPE_REQUEST;
			sess->req_hdrs[REQ_HDR_IDX_ACCEPT].header.id = RTSP_HDR_SESSION;
			sess->req_hdrs[REQ_HDR_IDX_ACCEPT].header.session.id = NULL;
			sess->req_hdrs[REQ_HDR_IDX_ACCEPT].header.session.id_len = 0;
			sess->req_hdrs[REQ_HDR_IDX_ACCEPT].header.session.timeout = 0;
			return (RTSP_E_SUCCESS);
		}
	}
	return(RTSP_E_FAILURE);
}
static inline rtsp_error_t rtsp_req_compose(rtsp_session_t *sess){

	if(!sess) {
		return (RTSP_E_INVALID_ARG);
	}

	return( rtsp_compose_message(sess->request_buffer,
			sess->req_buf_size, &sess->used_req_buf, &sess->request));

}

rtsp_error_t rtspc_req_int(rtsp_session_t *sess){

	rtsp_error_t err = RTSP_E_FAILURE;

	err = init_req_line(sess);
	if(err != RTSP_E_SUCCESS)
		return (err);

	err = init_req_hdr(sess);
	if(err != RTSP_E_SUCCESS)
		return (err);

	err = init_req_hdr_cseq(sess);
	if(err != RTSP_E_SUCCESS)
		return (err);

	err = init_req_hdr_accept(sess);
	if(err != RTSP_E_SUCCESS)
		return (err);

	return(err);
}

rtsp_error_t req_DESCRIBE(rtsp_session_t *sess){
	rtsp_msg_t *msg = NULL;
	rtsp_hdr_list_t **hdr_list;

	if(!sess) {
		return (RTSP_E_INVALID_ARG);
	}
	msg = &sess->request;
	msg->start_line.request.method = RTSP_MTHD_DESCRIBE;
	msg->start_line.request.url = sess->rtsp_url;
	sess->req_hdrs[REQ_HDR_IDX_CSEQ].header.cseq.seq_num++;
	/* Adding CSEQ to request */
	hdr_list = &msg->header_list;
	*hdr_list = &sess->req_hdrs[REQ_HDR_IDX_CSEQ];
	hdr_list = &((*hdr_list)->next);
	/* Adding Accept to Request */
	*hdr_list = &sess->req_hdrs[REQ_HDR_IDX_ACCEPT];
	hdr_list = &((*hdr_list)->next);
	/* Ending List of headers */
	*hdr_list = NULL;
	return(RTSP_E_SUCCESS);	
}


rtsp_error_t req_SETUP_audio(rtsp_session_t *sess){

	rtsp_msg_t *msg = NULL;
	rtsp_hdr_list_t **hdr_list;
	rtsp_error_t rtsp_err = RTSP_E_FAILURE;
	const char *audio = NULL;
	const char *control = NULL;
	const char *end = NULL;
	int32_t len = 0;

	if(!sess || !sess->sdp || (sess->used_sdp_size <= 0)) { 
		return (RTSP_E_INVALID_ARG);
	}

	msg = &sess->request;
	msg->start_line.request.method = RTSP_MTHD_SETUP;
	audio = strcasestr(sess->sdp, "m=audio ");

	if(audio == NULL){
		ERR_LOG("AUDIO Track Not available");
		return(RTSP_E_FAILURE);
	}

	control = strcasestr(audio, "a=control:");
	if(control == NULL){
		ERR_LOG("AUDIO Track  control attribute Not available");
		return(RTSP_E_FAILURE);
	}

	end = strchr(control, '\r');
	if(end == NULL){
		ERR_LOG("Cannot find End of Control");
		return(RTSP_E_FAILURE);
	}
	control+=(sizeof("m=control:")-1);
	while(isspace(*control))control++;
	if( (end-control > (sizeof("rtsp://")-1))
			&& (0 == strncmp("rtsp://", control, sizeof("rtsp://")-1))){
		if((end-control) < (sess->scr_pad_size-sess->scr_used_len)){
			strncpy(&sess->scratch_pad[sess->scr_used_len], control, end-control);
			len = end-control;
		}
		else{
			ERR_LOG("Memory not enough to handle url %.*s",( int)(end-control), control);
			return (RTSP_E_FAILURE);
		}
	} else {

		len = snprintf(&sess->scratch_pad[sess->scr_used_len],
				(sess->scr_pad_size-sess->scr_used_len),"%s/%.*s", sess->url,(int)(end-control), control);
		if(len == sess->scr_pad_size-sess->scr_used_len){
			ERR_LOG("Memory not enough to handle url %s/%.*s", sess->url,(int)(end-control), control);
			return (RTSP_E_FAILURE);
		}
	}

	rtsp_err = get_rtsp_url(&msg->start_line.request.url, &sess->scratch_pad[sess->scr_used_len] ,len);
	if(rtsp_err != RTSP_E_SUCCESS){
		return(RTSP_E_FAILURE);
	}
	/* 
	 * If URL points to different host 
	 * we dont support now
	 */

	sess->req_hdrs[REQ_HDR_IDX_CSEQ].header.cseq.seq_num++;
	/* Adding CSEQ to request */
	hdr_list = &msg->header_list;
	*hdr_list = &sess->req_hdrs[REQ_HDR_IDX_CSEQ];
	hdr_list = &((*hdr_list)->next);

	/* Adding Transport header 
	 */
	*hdr_list = &sess->req_hdrs[REQ_HDR_IDX_TRANSPORT];
	(*hdr_list)->header.id = RTSP_HDR_TRANSPORT;
	(*hdr_list)->header.type = RTSP_TYPE_REQUEST;
	(*hdr_list)->header.transport.spec = TSPEC_RTP_AVP_UC_UDP;
	/* Initialize RTP Session and get binded port */

	(*hdr_list)->header.transport.client_port_fr = 4000;
	/* Initialize RTCP Session and get binded port */

	(*hdr_list)->header.transport.client_port_to = 0;
	hdr_list = &((*hdr_list)->next);
	
	if(sess->sess_id){
		*hdr_list = &sess->req_hdrs[REQ_HDR_IDX_SESSION];
		(*hdr_list)->header.id = RTSP_HDR_SESSION;
		(*hdr_list)->header.type = RTSP_TYPE_REQUEST;
		(*hdr_list)->header.session.id = sess->sess_id;
		(*hdr_list)->header.session.id_len = sess->sess_id_len;
		hdr_list = &((*hdr_list)->next);
	}
	/* Ending List of headers */
	*hdr_list = NULL;
	
	return(RTSP_E_SUCCESS);
}

rtsp_error_t req_SETUP_video(rtsp_session_t *sess){

	rtsp_msg_t *msg = NULL;
	rtsp_hdr_list_t **hdr_list;
	rtsp_error_t rtsp_err = RTSP_E_FAILURE;
	const char *video = NULL;
	const char *control = NULL;
	const char *end = NULL;
	int32_t len = 0;

	if(!sess || !sess->sdp || (sess->used_sdp_size <= 0)) { 
		return (RTSP_E_INVALID_ARG);
	}

	msg = &sess->request;
	msg->start_line.request.method = RTSP_MTHD_SETUP;
	video = strcasestr(sess->sdp, "m=video ");

	if(video == NULL){
		ERR_LOG("AUDIO Track Not available");
		return(RTSP_E_FAILURE);
	}

	control = strcasestr(video, "a=control:");
	if(control == NULL){
		ERR_LOG("AUDIO Track  control attribute Not available");
		return(RTSP_E_FAILURE);
	}

	end = strchr(control, '\r');
	if(end == NULL){
		ERR_LOG("Cannot find End of Control");
		return(RTSP_E_FAILURE);
	}
	control+=(sizeof("m=control:")-1);
	while(isspace(*control))control++;
	if( (end-control > (sizeof("rtsp://")-1))
			&& (0 == strncmp("rtsp://", control, sizeof("rtsp://")-1))){
		if((end-control) < (sess->scr_pad_size-sess->scr_used_len)){
			strncpy(&sess->scratch_pad[sess->scr_used_len], control, end-control);
			len = end-control;
		}
		else{
			ERR_LOG("Memory not enough to handle url %.*s", (int)(end-control), control);
			return (RTSP_E_FAILURE);
		}
	} else {

		len = snprintf(&sess->scratch_pad[sess->scr_used_len],
				(sess->scr_pad_size-sess->scr_used_len),"%s/%.*s", sess->url,(int)(end-control), control);
		if(len == sess->scr_pad_size-sess->scr_used_len){
			ERR_LOG("Memory not enough to handle url %s/%.*s", sess->url,(int)(end-control), control);
			return (RTSP_E_FAILURE);
		}
	}

	rtsp_err = get_rtsp_url(&msg->start_line.request.url, &sess->scratch_pad[sess->scr_used_len] ,len);
	if(rtsp_err != RTSP_E_SUCCESS){
		return(RTSP_E_FAILURE);
	}

	sess->req_hdrs[REQ_HDR_IDX_CSEQ].header.cseq.seq_num++;
	/* Adding CSEQ to request */
	hdr_list = &msg->header_list;
	*hdr_list = &sess->req_hdrs[REQ_HDR_IDX_CSEQ];
	hdr_list = &((*hdr_list)->next);

	/* Adding Transport header 
	 */
	*hdr_list = &sess->req_hdrs[REQ_HDR_IDX_TRANSPORT];
	(*hdr_list)->header.id = RTSP_HDR_TRANSPORT;
	(*hdr_list)->header.type = RTSP_TYPE_REQUEST;
	(*hdr_list)->header.transport.spec = TSPEC_RTP_AVP_UC_UDP;
	(*hdr_list)->header.transport.client_port_fr = 4000;
	(*hdr_list)->header.transport.client_port_to = 0;
	hdr_list = &((*hdr_list)->next);

	if(sess->sess_id){
		*hdr_list = &sess->req_hdrs[REQ_HDR_IDX_SESSION];
		(*hdr_list)->header.id = RTSP_HDR_SESSION;
		(*hdr_list)->header.type = RTSP_TYPE_REQUEST;
		(*hdr_list)->header.session.id = sess->sess_id;
		(*hdr_list)->header.session.id_len = sess->sess_id_len;
		hdr_list = &((*hdr_list)->next);
	}
	/* Ending List of headers */
	*hdr_list = NULL;
	
	return(RTSP_E_SUCCESS);
}

rtsp_error_t req_PLAY(rtsp_session_t *sess){
	rtsp_msg_t *msg = NULL;
	rtsp_hdr_list_t **hdr_list;

	if(!sess) {
		return (RTSP_E_INVALID_ARG);
	}
	msg = &sess->request;
	msg->start_line.request.method = RTSP_MTHD_PLAY;
	msg->start_line.request.url = sess->rtsp_url;
	sess->req_hdrs[REQ_HDR_IDX_CSEQ].header.cseq.seq_num++;
	/* Adding CSEQ to request */
	hdr_list = &msg->header_list;
	*hdr_list = &sess->req_hdrs[REQ_HDR_IDX_CSEQ];
	hdr_list = &((*hdr_list)->next);
	if(sess->sess_id){
		*hdr_list = &sess->req_hdrs[REQ_HDR_IDX_SESSION];
		(*hdr_list)->header.id = RTSP_HDR_SESSION;
		(*hdr_list)->header.type = RTSP_TYPE_REQUEST;
		(*hdr_list)->header.session.id = sess->sess_id;
		(*hdr_list)->header.session.id_len = sess->sess_id_len;
		hdr_list = &((*hdr_list)->next);
	} else {
		ERR_LOG ("Session-id not obtained to continue to play ");
		return(RTSP_E_FAILURE);
	}
	/* Ending List of headers */
	*hdr_list = NULL;
	return(RTSP_E_SUCCESS);
}

rtsp_error_t req_TEARDOWN(rtsp_session_t *sess){
	rtsp_msg_t *msg = NULL;
	rtsp_hdr_list_t **hdr_list;

	if(!sess) {
		return (RTSP_E_INVALID_ARG);
	}
	msg = &sess->request;
	msg->start_line.request.method = RTSP_MTHD_TEARDOWN;
	msg->start_line.request.url = sess->rtsp_url;
	sess->req_hdrs[REQ_HDR_IDX_CSEQ].header.cseq.seq_num++;
	/* Adding CSEQ to request */
	hdr_list = &msg->header_list;
	*hdr_list = &sess->req_hdrs[REQ_HDR_IDX_CSEQ];
	hdr_list = &((*hdr_list)->next);
	if(sess->sess_id){
		*hdr_list = &sess->req_hdrs[REQ_HDR_IDX_SESSION];
		(*hdr_list)->header.id = RTSP_HDR_SESSION;
		(*hdr_list)->header.type = RTSP_TYPE_REQUEST;
		(*hdr_list)->header.session.id = sess->sess_id;
		(*hdr_list)->header.session.id_len = sess->sess_id_len;
		hdr_list = &((*hdr_list)->next);
	} else {
		ERR_LOG ("Session-id not obtained to continue to play ");
		return(RTSP_E_FAILURE);
	}
	/* Ending List of headers */
	*hdr_list = NULL;
	return(RTSP_E_SUCCESS);
}

rtsp_error_t req_compose(rtsp_session_t *sess){
	rtsp_error_t err = RTSP_E_FAILURE;

	if(!sess){
		return(RTSP_E_INVALID_ARG);
	}

	if(sess->msg_num > 5){
		return (RTSP_E_FAILURE);
	}


	switch(sess->msg_num){
		case 1:
			err = req_DESCRIBE(sess);
			sess->rw_state = RTSP_WRITE_REQ;
			break;
		case 2:
			err = req_SETUP_audio(sess);
			sess->rw_state = RTSP_WRITE_REQ;
			break;
		case 3:
			err = req_SETUP_video(sess);
			sess->rw_state = RTSP_WRITE_REQ;
			break;
		case 4:
			err = req_PLAY(sess);
			sess->rw_state = RTSP_WAIT_CMD;
			break;
		case 5:
			err = req_TEARDOWN(sess);
			sess->rw_state = RTSP_WRITE_REQ;
			break;
		default:
			err = RTSP_E_FAILURE;
	}

	if(err != RTSP_E_SUCCESS)
		return (err);

	return rtsp_req_compose(sess);
}

rtsp_error_t req_compose_TEARDOWN(rtsp_session_t *sess){
	rtsp_error_t err = RTSP_E_FAILURE; 
	err = req_TEARDOWN(sess);
	if(err != RTSP_E_SUCCESS){
		ERR_LOG("Cannot construct TEARDOWN");
	} else {
		err = rtsp_req_compose(sess);
		if(err != RTSP_E_SUCCESS){
			ERR_LOG("Failed to compose TEARDOWN");
		}
	}
	return (err);
}
