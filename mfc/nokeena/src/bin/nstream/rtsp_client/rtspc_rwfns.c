/* File : rtspc_rwfns.c Author : Patrick Mishel R
 * Copyright (C) 2008 - 2009 Nokeena Networks, Inc. 
 * All rights reserved.
 */

/** 
 * @file rtspc_rwfns.c
 * @brief Read Write functions for RTSP
 * @author Patrick Mishel R
 * @version 1.00
 * @date 2009-08-12
 */


/* READ 
 * 1. Peek and Read Socket Till header End
 * 2. Parse Header Till Header End
 * 3. If Entity present - Read Entity till length required
 * 4. Parse Entity
 * Message Read and parse Done
 * Limitations - Hard limit on the size of RTSP message recvd.
 */

const char hdr_end[] = {RTSP_CR,RTSP_LF,RTSP_CR,RTSP_LF,0};

rtsp_error_t respc_rd_res_headers(rtsp_session_t *sess){

	if(!sess){
		return (RTSP_E_INVALID_ARG);
	}

	sess->used_res_buf = recv(sess->sfd, 
			sess->response_buffer, sess->res_buf_size , MSG_PEEK);
	rtsp_error_t err = RTSP_E_FAILURE;
	if(sess->used_res_buf < 0){
		if (errno == EAGAIN){
			/* Device not ready/data not available */
			err =  (RTSP_E_RETRY);
		} else {
			ERR_LOG("recv from socket failed");
			perror("recv");
			err =  (RTSP_E_FAILURE);
		}
	} else if (sess->used_res_buf < 0){
		err = (RTSP_E_RETRY);
	} else { 
		/* Read Some Data 
		 * Check if header End is reached */
		const char *end = NULL;
		sess->response_buffer[sess->used_res_buf] = '\0';
		end = strstr(sess->response_buffer, hdr_end);
		if(end == NULL) {
			if(sess->used_res_buf >= sess->res_buf_size){
				/* No memory to hold one message */
				ERR_LOG("Insufficient Memory to Hold RTSP Message");
				err =  (RTSP_E_FAILURE);
			} else {
				/* Partial Message recvd retry after some time */
				err =  (RTSP_E_RETRY);
			}
		} else {
			/* Message present */
			end+=4;
			sess->used_res_buf = recv(sess->sfd, 
					sess->response_buffer, end - sess->response_buffer , MSG_WAITALL);
			err = (RTSP_E_SUCCESS);
		}
	}
	return (err);
}

rtsp_error_t rtspc_rd_res_entity(rtsp_session_t *sess){
	int32_t recv_len = 0;
	if(!sess && !sess->response.entity){
		return (RTSP_E_INVALID_ARG);
	}

	if(sess->res_entity_len <=0 ){
		/* No Data to Read */
		return (RTSP_E_SUCCESS);
	}

	if((sess->used_res_buf + sess->res_entity_len) > sess->res_buf_size){
		ERR_LOG("NO Space for Entity");
		return(RTSP_E_FAILURE);
	}

	
	recv_len = recv(sess->sfd, &sess->response_buffer[sess->used_res_buf], 
			sess->res_entity_len , MSG_WAITALL);

	if(recv_len < 0){
		if(EAGAIN == errno) {
			ERR_LOG("EAGAIN");
			return (RTSP_E_RETRY);
		}
		perror("recv");
		return(RTSP_E_FAILURE);
	} 

	sess->res_entity_len -= recv_len;
	sess->used_res_buf += recv_len;

	if(sess->response.entity){
		/* Update entity data */
		sess->response.entity->len += recv_len;	
	} else {
		/* Allocate Entity space */
		sess->response.entity = &sess->res_entity;
		sess->response.entity->data = &sess->response_buffer[sess->used_res_buf-recv_len];
		sess->response.entity->len = recv_len;
	}

	if(sess->res_entity_len > 0){
		return(RTSP_E_RETRY);
	}

	/* Recv all reqd data */
	return(RTSP_E_SUCCESS);
}

rtsp_error_t rtspc_wr_rqst_msg(rtsp_session_t *sess){

	if(!sess){
		return (RTSP_E_INVALID_ARG);
	}

	/*  
	 *  Send Data to socket from request buffer
	 */

	int32_t bytes_to_send = sess->used_req_buf - sess->sent_bytes;
	int32_t bytes_sent = 0;

	if(bytes_to_send < 0){
		ERR_LOG("Invalid 'Bytes' to send");
		return (RTSP_E_FAILURE);
	}

	bytes_sent = send(sess->sfd, &sess->request_buffer[sess->sent_bytes], 
			(sess->used_req_buf - sess->sent_bytes) ,MSG_DONTWAIT);

	if(bytes_sent < 0){
		if(EAGAIN == errno) {
			ERR_LOG("EAGAIN");
			return (RTSP_E_RETRY);
		}
		perror("recv");
		return (RTSP_E_FAILURE);
	}

	sess->sent_bytes += bytes_sent;

	if(sess->sent_bytes < sess->used_req_buf){
		return (RTSP_E_RETRY);
	}
	sess->sent_bytes = 0;
	return (RTSP_E_SUCCESS);
}

rtsp_error_t process_req_res(rtsp_session_t *sess){

	rtsp_error_t err = RTSP_E_FAILURE;
	rtsp_trans_t action = RTSP_NO_CHANGE;

	if(!sess){
		return (RTSP_E_INVALID_ARG);
	}
	
	switch(sess->response.start_line.response.status_code/100){
		case 1:
			/* TODO - Check this action */
			action = RTSP_NO_CHANGE;
			break;
		case 2:
			action = RTSP_ADVANCE;
			break;
		case 3:
			action = RTSP_RESET;
			break;
		case 4:
		case 5:
			action = RTSP_NO_CHANGE;
			break;
		default:
			ERR_LOG("Unknown Status Code %d", sess->response.start_line.response.status_code);
			return (RTSP_E_FAILURE);
			break;
	}
	
	switch (action){
		case RTSP_NO_CHANGE:
			err = res_handle(sess);
			if(err != RTSP_E_SUCCESS){
				sess->rw_state = RTSP_ERR_EXIT;
				break;
			}
			sess->rw_state = RTSP_ERR_EXIT;
			break;
		case RTSP_ADVANCE:
			err = res_handle(sess);
			if(err != RTSP_E_SUCCESS){
				sess->rw_state = RTSP_ERR_EXIT;
				break;
			}
			sess->msg_num++;
			err = req_compose(sess);
			if(err != RTSP_E_SUCCESS){
				sess->rw_state = RTSP_ERR_EXIT;
			}
			break;
		case RTSP_RESET:
			sess->msg_num = 0;
			/* CHECKME
			 * - Might cause infinite loop
			 */
			err = req_compose(sess);
			if(err != RTSP_E_SUCCESS){
				sess->rw_state = RTSP_ERR_EXIT;
			}
			break;
	}
	return (err);
}


rtsp_error_t process_rtsp_loop(rtsp_session_t *sess){
	rtsp_error_t err = RTSP_E_FAILURE;

	static int init = 0;
	if(!sess){
		return(RTSP_E_INVALID_ARG);
	}
	
	if(init){
		fprintf(stdout, "in>>>\n%.*s", sess->used_res_buf, sess->response_buffer);
		err = process_req_res(sess);
	} else {
		sess->msg_num++;
		err = req_compose(sess);
		init = 1;
	}

	if(err != RTSP_E_SUCCESS){
		sess->rw_state = RTSP_ERR_EXIT;
	} else {
		fprintf(stdout, "out<<<\n%.*s", sess->used_req_buf, sess->request_buffer);
	}
	return (RTSP_E_SUCCESS);
}

	/* Fall back if the next state matches\
	* the next case\
	* CAUTION if new states are added\
	*/\
#define RdWr_ERR(err, next_state) {\
	if(RTSP_E_RETRY == err){\
		/* Try on next call back */\
		break;\
	} else if (RTSP_E_SUCCESS == err){\
		sess->rw_state = next_state;\
	} else {\
		sess->rw_state = RTSP_ERR_EXIT;\
	}\
}\

rtsp_error_t rdwr_loop(rtsp_session_t *sess){
	rtsp_error_t err = RTSP_E_FAILURE;

	if(!sess){
		return(RTSP_E_INVALID_ARG);
	}

	switch(sess->rw_state){	
		case RTSP_INIT:
			sess->rw_state = RTSP_WRITE_REQ;
			if(sess->rw_state != RTSP_WRITE_REQ)
				break;
		case RTSP_WAIT_CMD:
			/* Process commands 
			 * This is a manual command
			 */
			sleep(30);
			sess->msg_num = 5;
			err = req_compose(sess);
			fprintf(stdout, "out<<<\n%.*s", sess->used_req_buf, sess->request_buffer);
			if(sess->rw_state != RTSP_WRITE_REQ)
				break;
		case RTSP_WRITE_REQ:
			err = rtspc_wr_rqst_msg(sess);
			RdWr_ERR(err, RTSP_READ_HDRS);
			if(sess->rw_state != RTSP_READ_HDRS)
				break;
		case RTSP_READ_HDRS:
			err = respc_rd_res_headers(sess);
			RdWr_ERR(err, RTSP_READ_ENTITY);
			err = rtsp_res_parse_hdrs(sess);
			RdWr_ERR(err, RTSP_READ_ENTITY);
			if(sess->rw_state != RTSP_READ_ENTITY)
				break;
		case RTSP_READ_ENTITY:
			err = rtspc_rd_res_entity(sess);
			RdWr_ERR(err, RTSP_PROCESS_DATA);
			if(sess->rw_state != RTSP_PROCESS_DATA)
				break;
		case RTSP_PROCESS_DATA:
			/* State functions only when Request (R) and (r) 
			 * are present
			 * rtsp_process (R, r)
			 * 		Eval(R,r)
			 * 		  Success
			 * 			Compose next message
			 * 			rw_state -> WRITE_REQ
			 * 		  Failure
			 * 			Exit
			 */
			err = process_rtsp_loop(sess);
			if(sess->rw_state != RTSP_EXIT && sess->rw_state != RTSP_ERR_EXIT)
				break;
		case RTSP_ERR_EXIT:
		case RTSP_EXIT:
		case RTSP_INVALID:
		default:
			/* Do Cleanup 
			 * unregister callbacks to this function 
			 */
			return (RTSP_E_FAILURE);
			break;
	}
	return (err);
}


