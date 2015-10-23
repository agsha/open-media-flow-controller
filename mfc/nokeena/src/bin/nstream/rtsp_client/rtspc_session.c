/* File : rtspc_session.c Author : Patrick Mishel R
 * Copyright (C) 2008 - 2009 Nokeena Networks, Inc. 
 * All rights reserved.
 */
/** 
 * @file rtspc_session.c
 * @brief RTSP Client Session handling functions
 * @author Patrick Mishel R
 * @version 1.00
 * @date 2009-08-06
 */

#include <errno.h>
#include <rtsp_session.h>

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <rtspc_if.h>

#include "rtspc_conf_private.h"
#include "rtspc_rwfns.c"

/** 
 * @brief - Macro does error print - Useful for debug and 
 * 			jumps to the cleanup label
 * 
 * @param cleanup_label - Label to go to 
 * @param str - String/fmtstring to print
 * @param x... - Variable Arguments
 * 
 */
#define ERR_GOTO_EXIT(cleanup_label, str,x...) {\
	ERR_LOG(str, ##x);\
	goto cleanup_label;\
}

typedef enum session_type_t { 
	SESS_TYPE_RTSP = 0x01,
	SESS_TYPE_SUB = 0x02,
}session_type_t;

/* 
 * Session ID - Limitation
 * Max Sessions - 64K Sessions
 */
typedef struct session_info_t{
	session_type_t type;
	union {
		rtsp_session_t *rtsp_sinfo;
	};
}session_info_t;

int set_non_blocking_socket (int fd){
	if(fd > 0){
		int flag = fcntl(fd, F_GETFD);
		if (flag < 0)
			return -1;
		flag |= O_NONBLOCK;
		if(fcntl(fd, F_SETFD, flag) < 0){
			perror("fcntl");
			return (-1);
		}
		return 0;
	}
	return -1;
}

/** 
 * @brief Connects to the host
 * 
 * @param sess Session Context
 * 		TODO - Move allocation part to separate function
 * 		To enable one time allocation of server context
 * 
 * @return Error Code
 */
static inline rtsp_error_t rtspc_connect(rtsp_session_t *sess){

	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int const service_len = 32;
	char service[service_len];
	int ret = -1;

	if(!sess){
		return (RTSP_E_INVALID_ARG);
	}

	/* 
	 * Function will be called only valid url structure is present
	 */
	service[service_len-1]= '\0';
	strcpy(service, "rtsp");

	memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;     /* IPv4 or IPv6 */
	hints.ai_flags = 0;
	hints.ai_protocol = 0;
	switch(sess->rtsp_url.type){
		case RTSP_NW_TCP:
			hints.ai_socktype = SOCK_STREAM;
			break;
		case RTSP_NW_STAR:
		case RTSP_NW_UDP:
		default:
			ERR_LOG("URI Not supported");
			return (RTSP_E_UNSUPPORTED);
			break;
	}

	if(sess->rtsp_url.port > 0){
		snprintf(service, service_len,"%u", sess->rtsp_url.port);
		hints.ai_flags = AI_NUMERICSERV;
	} else {
		/* Default rtsp service name will pick default port 
		 * configured in the system /etc/services
		 */
	}

	ret = getaddrinfo(sess->rtsp_url.host,service, &hints, &result);
	if (ret != 0) {
		ERR_LOG("getaddrinfo: %s\n", gai_strerror(ret));
		return (RTSP_E_FAILURE);
	}

	/*
	 * Connect to address
	 * get Socket fd
	 */
	for (rp = result; rp != NULL; rp = rp->ai_next){
		sess->sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sess->sfd == -1)
			continue;

		if (connect(sess->sfd, rp->ai_addr, rp->ai_addrlen) != -1)
			break;  
		/* Success */
		close(sess->sfd);
	}
	
	freeaddrinfo(result);

	if(rp == NULL){
		ERR_LOG("Could not connect to host '%s'", sess->rtsp_url.host);
		return (RTSP_E_FAILURE);
	}

	if(0 != set_non_blocking_socket (sess->sfd)){
		ERR_LOG("Cannot Set Socket as Non-Blocking");
		return (RTSP_E_FAILURE);
	}

	STAT_LOG("Connected to host '%s'", sess->rtsp_url.host);
	return (RTSP_E_SUCCESS);
}


/** 
 * @brief Initialize Session context
 * 
 * @param url - rtsp url in the format rtsp://host:port/abs_url
 * @param pp_sess - Returned rtsp session struct
 * 
 * @return 
 */
rtsp_error_t rtspc_create_session(char *url, rtsp_session_t **pp_sess){

	/* 
	 * Got an RTSP URL 
	 * Create Network Socket and Connect
	 */
	rtsp_error_t rtsp_err = RTSP_E_FAILURE;
	rtsp_session_t *sess = NULL;
	if(!url || !pp_sess){
		return(RTSP_E_INVALID_ARG);
	}

	sess = calloc(1, sizeof(rtsp_session_t));
	if(NULL == sess){
		return (RTSP_E_MEM_ALLOC_FAIL);
	}

	sess->url = strdup(url);
	sess->url_len = strlen(sess->url);
	rtsp_err = get_rtsp_url(&sess->rtsp_url, sess->url,sess->url_len);
	if(rtsp_err != RTSP_E_SUCCESS){
		ERR_GOTO_EXIT(free_and_exit, "RTSP Client URL '%s' Parsing Failed", sess->url);
	}

	/* Connect to Server */
	rtsp_err = rtspc_connect(sess);
	if(rtsp_err != RTSP_E_SUCCESS){
		ERR_GOTO_EXIT(free_and_exit, "RTSP Client Connect Failed");
	}

	/* 
	 * Allocate Context Memory 
	 * Using configuration index SESS_CONF_INDEX 
	 * */
	rtsp_err = RTSP_E_MEM_ALLOC_FAIL;

	sess->sdp = (char *)calloc(
			conf_limits[SESS_CONF_INDEX].sdp_size,sizeof(char));
	if(!sess->sdp){
		ERR_GOTO_EXIT(free_and_exit, "RTSP SDP Mem Alloc Failed");
	}

	sess->request_buffer = (char *)calloc(
			conf_limits[SESS_CONF_INDEX].req_buf_size,sizeof(char));
	if(!sess->request_buffer){
		ERR_GOTO_EXIT(free_and_exit, "RTSP Client Memory Allocation Failed");
	}

	sess->response_buffer = (char *)calloc(
			conf_limits[SESS_CONF_INDEX].res_buf_size, sizeof(char));
	if(!sess->response_buffer){
		ERR_GOTO_EXIT(free_and_exit, "RTSP Client Memory Allocation Failed");
	}

	sess->req_hdrs = (rtsp_hdr_list_t *)calloc(
			conf_limits[SESS_CONF_INDEX].req_hdr_count, sizeof(rtsp_hdr_list_t));
	if(!sess->req_hdrs){
		ERR_GOTO_EXIT(free_and_exit, "RTSP Client Memory Allocation Failed");
	}

	sess->res_hdrs = (rtsp_hdr_list_t *)calloc(
			conf_limits[SESS_CONF_INDEX].res_hdr_count, sizeof(rtsp_hdr_list_t));
	if(!sess->res_hdrs){
		ERR_GOTO_EXIT(free_and_exit, "RTSP Client Memory Allocation Failed");
	}

	sess->scratch_pad = (char *) calloc(
			conf_limits[SESS_CONF_INDEX].scr_pad_size, sizeof(char));
	if(!sess->scratch_pad){
		ERR_GOTO_EXIT(free_and_exit, "RTSP Client Memory Allocation Failed");
	}
	
	sess->sdp_size = conf_limits[SESS_CONF_INDEX].sdp_size;
	sess->req_buf_size = conf_limits[SESS_CONF_INDEX].req_buf_size;
	sess->res_buf_size = conf_limits[SESS_CONF_INDEX].res_buf_size;
	sess->req_hdr_count = conf_limits[SESS_CONF_INDEX].req_hdr_count;
	sess->res_hdr_count = conf_limits[SESS_CONF_INDEX].res_hdr_count;
	sess->scr_pad_size = conf_limits[SESS_CONF_INDEX].scr_pad_size;

	sess->limits = &conf_limits[SESS_CONF_INDEX];

	/* Returning Session Pointer */
	*pp_sess = sess;
	return(rtsp_err);
free_and_exit:
	rtspc_destroy_session(sess);
	return(rtsp_err);
}

rtsp_error_t rtspc_init_session(rtsp_session_t *sess){

	rtsp_error_t err = RTSP_E_FAILURE;

	if(!sess){
		return (RTSP_E_INVALID_ARG);
	}

 	err = rtspc_req_int(sess);
	if (RTSP_E_SUCCESS != err)
		return (err);

	/* Add other init functions */
	return (err);
}
	
/** 
 * @brief free Session context
 * 
 * @param sess
 */
void rtspc_destroy_session(rtsp_session_t *sess){

	if(NULL == sess) {
		return;
	}

	if(sess->url){
		free(sess->url);
		sess->url = NULL;
		sess->url_len = 0;
	}

	if(sess->request_buffer){
		free(sess->request_buffer);
		sess->request_buffer = NULL;
		sess->req_buf_size = 0;
	}

	if(sess->response_buffer){
		free(sess->response_buffer);
		sess->response_buffer = NULL;
		sess->res_buf_size = 0;
	}

	if(sess->req_hdrs){
		free(sess->req_hdrs);
		sess->req_hdrs = NULL;
		sess->req_hdr_count = 0;
	}

	if(sess->res_hdrs){
		free(sess->res_hdrs);
		sess->res_hdrs = NULL;
		sess->res_hdr_count = 0;
	}

	if(sess->scratch_pad){
		free(sess->scratch_pad);
		sess->scratch_pad = NULL;
		sess->scr_pad_size = 0;
	}

	if(sess->sfd){
		close(sess->sfd);
		sess->sfd = -1;
	}

	free(sess);
	return;
}

