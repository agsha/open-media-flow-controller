/* File : rtsp_compose_header.c Author : Patrick Mishel R
 * Copyright (C) 2008 - 2009 Nokeena Networks, Inc. 
 * All rights reserved.
 */

/** 
 * @file rtsp_compose_header.c
 * @brief Header Composing functions
 * @author Patrick Mishel R
 * @version 1.00
 * @date 2009-07-30
 */

/*
 * chdr  == compose_header
 */

static inline rtsp_error_t rtsp_chdr_connection (char *send_buf, int32_t send_buf_size, int32_t *used_buf_size, rtsp_header_t *rtsp_hdr){

	int32_t len = 0;

	if(!send_buf || send_buf_size<=0 || !used_buf_size || !rtsp_hdr || rtsp_hdr->id != RTSP_HDR_CONNECTION){
		return (RTSP_E_INVALID_ARG);
	}

	/* Fills only the header details and not the header name */
	if(rtsp_hdr->connection.token){
		/*
		 * Token present
		 */
		len = snprintf(send_buf,send_buf_size,"%s", rtsp_hdr->connection.token);
		if(len <=0 )
			return (RTSP_E_FAILURE);

		if(len >= send_buf_size)
			return (RTSP_E_NO_SPACE); 
		*used_buf_size = len;
		return (RTSP_E_SUCCESS);
	}
	return (RTSP_E_INVALID_ARG);
}

static inline rtsp_error_t rtsp_chdr_cseq(char *send_buf, int32_t send_buf_size, int32_t *used_buf_size, rtsp_header_t *rtsp_hdr){

	int32_t len = 0;

	if(!send_buf || send_buf_size<=0 || !used_buf_size || !rtsp_hdr || rtsp_hdr->id != RTSP_HDR_CSEQ){
		return (RTSP_E_INVALID_ARG);
	}
	
	len = snprintf(send_buf, send_buf_size,"%u", rtsp_hdr->cseq.seq_num);
	if(len <=0 )
		return (RTSP_E_FAILURE);

	if(len >= send_buf_size)
		return (RTSP_E_NO_SPACE);

	*used_buf_size = len;
	return (RTSP_E_SUCCESS);
}


static inline rtsp_error_t rtsp_chdr_accept(char *send_buf, int32_t send_buf_size, int32_t *used_buf_size, rtsp_header_t *rtsp_hdr){

	int32_t len = 0;

	if(!send_buf || send_buf_size<=0 || !used_buf_size || !rtsp_hdr || rtsp_hdr->id != RTSP_HDR_ACCEPT){
		return (RTSP_E_INVALID_ARG);
	}
	
	/* 
	 * TODO (when needed)- 
	 * Detail implementation like 'type/subtype' generic and other options
	 *
	 */
	if(rtsp_hdr->accept.app_type & RTSP_APP_TYPE_SDP){
		len = snprintf(send_buf, send_buf_size,"application/sdp");
	}

	if(len <=0 )
		return (RTSP_E_FAILURE);

	if(len >= send_buf_size)
		return (RTSP_E_NO_SPACE);

	*used_buf_size = len;
	return (RTSP_E_SUCCESS);
}


static inline rtsp_error_t rtsp_chdr_transport(char *send_buf, int32_t send_buf_size, int32_t *used_buf_size, rtsp_header_t *rtsp_hdr){

	int32_t len = 0;
	int32_t total_used_len = 0;
	rtsp_hdr_transport_t *transport = NULL;
	/* TODO
	 * layers
	 * destination
	 * append
	 * ...
	 */

	if(!send_buf || send_buf_size<=0 || !used_buf_size || !rtsp_hdr || rtsp_hdr->id != RTSP_HDR_TRANSPORT){
		return (RTSP_E_INVALID_ARG);
	}

	transport = &rtsp_hdr->transport;

	switch(transport->spec){
		case TSPEC_RTP_AVP_UC_UDP:
			if((transport->client_port_fr > 0 ) && (transport->client_port_to > 0 )){
				len = snprintf(send_buf, send_buf_size,
						"RTP/AVP/UDP;unicast;client_port=%u-%u", transport->client_port_fr,transport->client_port_to);
			} else if (transport->client_port_fr > 0 ) {
				len = snprintf(send_buf, send_buf_size ,
						"RTP/AVP/UDP;unicast;client_port=%u",transport->client_port_fr);
			} else {
				len = snprintf(send_buf, send_buf_size,"RTP/AVP/UDP;unicast");
			}
			if((transport->server_port_fr > 0 ) && (transport->server_port_to > 0 )){
				total_used_len+=len;
				if((send_buf_size - total_used_len) <= 0 ){
					return (RTSP_E_NO_SPACE);
				}
				len = snprintf(&send_buf[total_used_len], send_buf_size - total_used_len 
						,";server_port=%u-%u",transport->server_port_fr,transport->server_port_to);
			} else if (transport->server_port_fr > 0 ) {
				total_used_len+=len;
				if((send_buf_size - total_used_len) <= 0 ){
					return (RTSP_E_NO_SPACE);
				}
				len = snprintf(&send_buf[total_used_len], send_buf_size - total_used_len 
						,";server_port=%u",transport->server_port_fr);
			}
			break;
		case TSPEC_RTP_AVP_MC_UDP:
			if((transport->port_fr > 0 ) && (transport->port_to > 0 )){
				len = snprintf(send_buf, send_buf_size,
						"RTP/AVP/UDP;multicast;port=%u-%u", transport->port_fr,transport->port_to);
			} else if (transport->port_fr > 0 ) {
				len = snprintf(send_buf, send_buf_size ,
						"RTP/AVP/UDP;multicast;port=%u",transport->port_fr);
			} else {
				len = snprintf(send_buf, send_buf_size,"RTP/AVP/UDP;multicast");
			}
			if(transport->ttl > 0){
				total_used_len+=len;
				if((send_buf_size - total_used_len) <= 0 ){
					return (RTSP_E_NO_SPACE);
				}
				len = snprintf(&send_buf[total_used_len], send_buf_size - total_used_len ,
						";ttl=%u", transport->ttl);
			}
			break;
		case TSPEC_RTP_AVP_TCP:
			break;
		case TSPEC_RTP_AVP_UNICAST:
		case TSPEC_RTP_AVP_MULTICAST:
		case TSPEC_RTP_AVP_INTERLEAVED:
		case TSPEC_RTP_AVP_UDP:
			ERR_LOG("More Specific Definitions required to compose Transport header:");
		default:
			return(RTSP_E_FAILURE);
			break;
	}

	if(len<0)
		return(RTSP_E_FAILURE);


	if((transport->ileaved_ch_fr > 0 ) && (transport->ileaved_ch_to > 0 )){
		total_used_len+=len;
		if((send_buf_size - total_used_len) <= 0 ){
			return (RTSP_E_NO_SPACE);
		}
		len = snprintf(&send_buf[total_used_len], (send_buf_size - total_used_len),
				";interleaved=%u-%u", transport->ileaved_ch_fr, transport->ileaved_ch_to);
	} else if (transport->ileaved_ch_fr > 0 ) {
		total_used_len+=len;
		if((send_buf_size - total_used_len) <= 0 ){
			return (RTSP_E_NO_SPACE);
		}
		len = snprintf(&send_buf[total_used_len], (send_buf_size - total_used_len),
				";interleaved=%u",transport->ileaved_ch_fr);
	} 

	if(len<0)
		return(RTSP_E_FAILURE);
	

	if((len >= (send_buf_size - total_used_len)) && (send_buf[send_buf_size] != '\0')){
		return(RTSP_E_NO_SPACE);
	}

	/* TODO - 
	 * All other parameters *
	 */
	total_used_len += len;
	*used_buf_size = total_used_len;
	return (RTSP_E_SUCCESS);

}

static inline rtsp_error_t rtsp_chdr_session(char *send_buf, int32_t send_buf_size, int32_t *used_buf_size, rtsp_header_t *rtsp_hdr){

	if(!send_buf || send_buf_size<=0 || !used_buf_size || !rtsp_hdr || rtsp_hdr->id != RTSP_HDR_SESSION){
		return (RTSP_E_INVALID_ARG);
	}

	int32_t len = 0;

	if(rtsp_hdr->session.timeout > 0){
		len = snprintf(send_buf, send_buf_size, "%.*s;timeout=%u", rtsp_hdr->session.id_len, rtsp_hdr->session.id, rtsp_hdr->session.timeout);
	} else {
		len = snprintf(send_buf, send_buf_size, "%.*s", rtsp_hdr->session.id_len, rtsp_hdr->session.id);
	}

	if(len<0)
		return (RTSP_E_FAILURE);

	if(len >= send_buf_size)
		return (RTSP_E_NO_SPACE);

	*used_buf_size = len;
	return (RTSP_E_SUCCESS);
}

