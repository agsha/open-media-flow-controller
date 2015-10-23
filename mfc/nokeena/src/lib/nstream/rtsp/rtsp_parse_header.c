/* File : rtsp_parse_header.c Author : Patrick Mishel R
 * Copyright (C) 2008 - 2009 Nokeena Networks, Inc. 
 * All rights reserved.
 */
#ifndef _PARSE_HEADER_C_
#define _PARSE_HEADER_C_ 
/** 
 * @file rtsp_parse_header.c
 * @brief Parsing functions for individual headers
 * @author Patrick Mishel R
 * @version 1.00
 * @date 2009-08-09
 */

/*
 * phdr = parse header
 * rtsp_error_t rtsp_phdr_<header name>(rtsp_header_t *header, const char *msg, int32_t msg_len, const char **endptr)
 */

#include <stdlib.h> 
#include <limits.h>
#include "rtsp_parse_fns.c"

rtsp_error_t rtsp_phdr_cseq(rtsp_header_t *header, const char *msg, int32_t msg_len, const char **endptr){

	const char *end = msg + msg_len;
	unsigned long long ull_cseq = 0;

	if(!header || !msg || msg_len <= 0){
		return (RTSP_E_INVALID_ARG);
	}

	ull_cseq = strtoull(msg,(char **) endptr, 10);
	/* TODO - Find strntoull and put it here 
	 * Simple way is to swap last char with null char and then restore after computation*/
	if(*endptr >= end){
		ERR_LOG("Invalid Memory Access");
		return (RTSP_E_INCOMPLETE);
	}

	if(ull_cseq > UINT_MAX){
		ERR_LOG ("CSeq not supported beyond %u - occured (%llu)", UINT_MAX, ull_cseq);
		return (RTSP_E_UNSUPPORTED);
	}

	header->cseq.seq_num = ull_cseq;
	return (RTSP_E_SUCCESS);
}

rtsp_error_t rtsp_phdr_content_len(rtsp_header_t *header, const char *msg, int32_t msg_len, const char **endptr){

	const char *end = msg + msg_len;
	unsigned long long ull_cseq = 0;

	if(!header || !msg || msg_len <= 0){
		return (RTSP_E_INVALID_ARG);
	}

	ull_cseq = strtoull(msg,(char **) endptr, 10);
	/* TODO - Find strntoull and put it here 
	 * Simple way is to swap last char with null char and then restore after computation*/
	if(*endptr >= end){
		ERR_LOG("Invalid Memory Access");
		return (RTSP_E_INCOMPLETE);
	}

	if(ull_cseq > UINT_MAX){
		ERR_LOG ("CSeq not supported beyond %u - occured (%llu)", UINT_MAX, ull_cseq);
		return (RTSP_E_UNSUPPORTED);
	}

	header->content_len.bytes = ull_cseq;
	return (RTSP_E_SUCCESS);
}

static inline const char *find_char(const char *scan, const char *end, char c){
	if(scan && end){
		while((scan < end) && *scan && (*scan != c)){
			scan++;
		}
	}
	return(scan);
}

static inline  rtsp_error_t is_strncasematch(const char *scan, const char * end, const char *string, int str_len){
	if(scan && end && string && (str_len>0) && ((scan + str_len) <= end) && (0 == strncasecmp(scan, string, str_len)))
		return (RTSP_E_SUCCESS);
	return (RTSP_E_FAILURE); 
}

rtsp_error_t rtsp_phdr_transport(rtsp_header_t *header, const char *msg, int32_t msg_len, const char **endptr){

	const char *scan = NULL;
	const char *temp = NULL;
	const char *scan_start = NULL;
	const char *scan_end = NULL;
	const char *end = msg + msg_len;
	rtsp_error_t err = RTSP_E_FAILURE;

	if(!header || !msg || msg_len <= 0){
		return (RTSP_E_INVALID_ARG);
	}
	/* TODO - Multiple transport headers separated by comma.
	 */

	/* 
	 * transport-protocol/profile
	 */
	scan = msg;
	if(msg_len < STATIC_STR_LEN("RTP/AVP"))
		return (RTSP_E_FAILURE);

	if(0 != strncmp(scan, "RTP/AVP", STATIC_STR_LEN("RTP/AVP"))){
		return (RTSP_E_FAILURE);
	} 

	scan += STATIC_STR_LEN("RTP/AVP");
	header->transport.spec = 0;
	if(*scan == ';') {
		header->transport.spec = TSPEC_RTP_AVP_UDP;
	} else if(*scan == '/'){
		if(((scan+STATIC_STR_LEN("/TCP"))<end )
				&& (0 == strncasecmp(scan,"/TCP",STATIC_STR_LEN("/TCP")))){
			header->transport.spec = TSPEC_RTP_AVP_TCP;
			scan+=STATIC_STR_LEN("/TCP");
		} else if(((scan+STATIC_STR_LEN("/UDP"))<end) 
				&& (0 == strncasecmp(scan,"/UDP",STATIC_STR_LEN("/UDP")))){
			header->transport.spec = TSPEC_RTP_AVP_UDP;
			scan+=STATIC_STR_LEN("/UDP");
		}
	}

	while( (scan< end) && (*scan == ';')){
		scan++;
		scan_end = find_char(scan, end, ';');
		if(*scan_end != ';')
			scan_end = find_char(scan, end, '\r');

	/* 
	 * Quite a long function - TODO splitup
	 * Logic
	 * First char is the Key
	 * 1. Switch on key
	 * 2. Do strcmp and find match
	 * 3. Extract parameter values
	 */
		scan_start = scan;
		switch (*scan|0x20){
			case 'u':
				err = is_strncasematch(scan, scan_end, "unicast", STATIC_STR_LEN("unicast"));
				if(err == RTSP_E_SUCCESS){
					scan+=STATIC_STR_LEN("unicast");
					header->transport.spec |= TSPEC_RTP_AVP_UNICAST;
					break;
				}
				break;
			case 'm':
				err = is_strncasematch(scan, scan_end, "multicast", STATIC_STR_LEN("multicast"));
				if(err == RTSP_E_SUCCESS){
					scan+=STATIC_STR_LEN("multicast");
					header->transport.spec |= TSPEC_RTP_AVP_MULTICAST;
					break;
				}
				err = is_strncasematch(scan, scan_end, "mode", STATIC_STR_LEN("mode"));
				if(err == RTSP_E_SUCCESS){
					scan+=STATIC_STR_LEN("mode");
					if(*scan != '='){
						err = (RTSP_E_FAILURE);
						break;
					}
					scan++;
					/*
					 * TODO - Mode parsing - Now we bypass and mark it as success
					 * 1\#mode
					 */
					scan = scan_end;
					break;
				}
				break;
			case 'd':
				err = is_strncasematch(scan, scan_end, "destination", STATIC_STR_LEN("destination"));
				if(err == RTSP_E_SUCCESS){
					scan+=STATIC_STR_LEN("destination");
					if(*scan != '='){
						err = (RTSP_E_FAILURE);
						break;
					}
					scan++;
					{
						int host_len = 0;
						host_len = rtsp_get_valid_host_len(scan, (scan_end-scan));
						if(host_len < RTSP_MAX_HOST_LEN){
							strncpy(header->transport.destination, scan, host_len);
							scan+=host_len;
							break;
						} else {
							ERR_LOG("Invalid Destination host");
							err = RTSP_E_FAILURE;
							break;
						}

					}
				}
				break;
			case 'i':
				err = is_strncasematch(scan, scan_end, "interleaved", STATIC_STR_LEN("interleaved"));
				if(err == RTSP_E_SUCCESS){
					header->transport.spec |= TSPEC_RTP_AVP_INTERLEAVED;
					scan+=STATIC_STR_LEN("interleaved");
					if(*scan != '='){
						err = (RTSP_E_FAILURE);
						break;
					}
					scan++;

					header->transport.ileaved_ch_fr = strtoul(scan,(char **)&temp,10);
					if(temp > (scan_end+1)){
						ERR_LOG("Parsing Transport Error - Check Logic");
						err = RTSP_E_FAILURE;
						break;
					}
					if(temp-scan > 3){
						ERR_LOG("1*3DIGIT  spec not met");
						err = RTSP_E_FAILURE;
						break;
					}

					if(*temp == '-'){
						scan = ++temp;
						header->transport.ileaved_ch_to = strtoul(scan,(char **)&temp,10);
						if(temp > (scan_end+1)){
							ERR_LOG("Parsing Transport Error - Check Logic");
							err = RTSP_E_FAILURE;
							break;
						}
						if(temp-scan > 3){
							ERR_LOG("1*3DIGIT  spec not met");
							err = RTSP_E_FAILURE;
							break;
						}
					} else {
						header->transport.ileaved_ch_to = -1;
					}
				}
				break;
			case 'a':
				err = is_strncasematch(scan, scan_end, "append", STATIC_STR_LEN("append"));
				if(err == RTSP_E_SUCCESS){
					scan+=STATIC_STR_LEN("append");
					header->transport.append = 1;
					break;
				}
				break;
			case 't':
				err = is_strncasematch(scan, scan_end, "ttl", STATIC_STR_LEN("ttl"));
				if(err == RTSP_E_SUCCESS){
					scan+=STATIC_STR_LEN("ttl");
					if(*scan!='='){
						err = (RTSP_E_FAILURE);
						break;
					}
					temp = scan;
					header->transport.ttl = strtoul(scan, (char **)&temp,10);
					if(temp > (scan_end+1)){
						ERR_LOG("Parsing Transport Error - Check Logic");
						err = RTSP_E_FAILURE;
						break;
					}
					scan = temp;
				}
				break;
			case 'l':
				err = is_strncasematch(scan, scan_end, "layers", STATIC_STR_LEN("layers"));
				if(err == RTSP_E_SUCCESS){
					scan+=STATIC_STR_LEN("layers");
					if(*scan != '='){
						err = (RTSP_E_FAILURE);
						break;
					}
					scan++;
					temp = scan;
					header->transport.layers = strtoull(scan,(char **)&temp,10);
					if(temp > (scan_end+1)){
						ERR_LOG("Parsing Transport Error - Check Logic");
						err = RTSP_E_FAILURE;
						break;
					}
				}
				break;
			case 'p':
				err = is_strncasematch(scan, scan_end, "port", STATIC_STR_LEN("port"));
				if(err == RTSP_E_SUCCESS){
					scan+=STATIC_STR_LEN("port");
					if(*scan != '='){
						err = (RTSP_E_FAILURE);
						break;
					}
					scan++;
					temp = scan;
					header->transport.port_fr = strtoul(scan,(char **)&temp,10);
					if(temp > (scan_end+1)){
						ERR_LOG("Parsing Transport Error - Check Logic");
						err = RTSP_E_FAILURE;
						break;
					}
					if(temp-scan > 5){
						ERR_LOG("1*5DIGIT  spec not met");
						err = RTSP_E_FAILURE;
						break;
					}

					if(*temp == '-'){
						scan = ++temp;
						header->transport.port_to = strtoul(scan,(char **)&temp,10);
						if(temp > (scan_end+1)){
							ERR_LOG("Parsing Transport Error - Check Logic");
							err = RTSP_E_FAILURE;
							break;
						}
						if(temp-scan > 5){
							ERR_LOG("1*5DIGIT  spec not met");
							err = RTSP_E_FAILURE;
							break;
						}
					} else {
						header->transport.port_to = -1;
					}
					scan = temp;
				}
				break;
			case 'c':
				err = is_strncasematch(scan, scan_end, "client_port", STATIC_STR_LEN("client_port"));
				if(err == RTSP_E_SUCCESS){
					scan+=STATIC_STR_LEN("client_port");
					if(*scan != '='){
						err = (RTSP_E_FAILURE);
						break;
					}
					scan++;
					temp = scan;
					header->transport.client_port_fr = strtoul(scan,(char **)&temp,10);
					if(temp > (scan_end+1)){
						ERR_LOG("Parsing Transport Error - Check Logic");
						err = RTSP_E_FAILURE;
						break;
					}

					if(temp-scan > 5){
						ERR_LOG("1*5DIGIT  spec not met");
						err = RTSP_E_FAILURE;
						break;
					}
					
					if(*temp == '-'){
						scan = ++temp;
						header->transport.client_port_to = strtoul(scan,(char **)&temp,10);
						if(temp > (scan_end+1)){
							ERR_LOG("Parsing Transport Error - Check Logic");
							err = RTSP_E_FAILURE;
							break;
						}
						if(temp-scan > 5){
							ERR_LOG("1*5DIGIT  spec not met");
							err = RTSP_E_FAILURE;
							break;
						}
					} else {
						header->transport.client_port_to = -1;
					}
					scan = temp;
				}
				break;
			case 's':
				err = is_strncasematch(scan, scan_end, "server_port", STATIC_STR_LEN("server_port"));
				if(err == RTSP_E_SUCCESS){
					scan+=STATIC_STR_LEN("server_port");
					if(*scan != '='){
						err = (RTSP_E_FAILURE);
						break;
					}
					scan++;
					temp = scan;
					header->transport.server_port_fr = strtoul(scan,(char **)&temp,10);
					if(temp > (scan_end+1)){
						ERR_LOG("Parsing Transport Error - Check Logic");
						err = RTSP_E_FAILURE;
						break;
					}
					if(temp-scan > 5){
						ERR_LOG("1*5DIGIT  spec not met");
						err = RTSP_E_FAILURE;
					}

					if(*temp == '-'){
						scan = ++temp;
						header->transport.server_port_to = strtoul(scan,(char **)&temp,10);
						if(temp > (scan_end+1)){
							ERR_LOG("Parsing Transport Error - Check Logic");
							err = RTSP_E_FAILURE;
							break;
						}
						if(temp-scan > 5){
							ERR_LOG("1*5DIGIT  spec not met");
							err = RTSP_E_FAILURE;
							break;
						}
					} else {
						header->transport.server_port_to = -1;
					}
					scan = temp;
				} else if (RTSP_E_SUCCESS == (err = is_strncasematch(scan, scan_end, "ssrc", STATIC_STR_LEN("ssrc")))){
					scan+=STATIC_STR_LEN("ssrc");
					if(*scan != '='){
						err = (RTSP_E_FAILURE);
						break;
					}
					scan++;
					/* 
					 * 8*8(HEX)
					 */
					if((scan+8) > scan_end){
						err = (RTSP_E_FAILURE);
						break;
					}
					temp = scan;
					header->transport.ssrc = strtoull(scan, (char **)&temp, 16);
					if((temp-scan) != 8){
						ERR_LOG("8*8(HEX) Spec not met for ssrc");
						err = (RTSP_E_FAILURE);
						break;
					}
					scan = temp;

				} else {
					err = RTSP_E_UNSUPPORTED;
				} 
				break;
			default:
				err = RTSP_E_UNSUPPORTED;
				break;
		}

		if((err == RTSP_E_UNSUPPORTED) || (scan != scan_end)){
			ERR_LOG("Parsing failed for Transport Parameter"\
					"\"%.*s",(int )(scan_end - scan_start), scan_start);
		} 

		if (err == RTSP_E_FAILURE){
			break;
		}
		scan = scan_end;
		/* Scan the next Parameter */
	}
	if(endptr)
		*endptr = scan;
	return (err);
}

rtsp_error_t rtsp_phdr_session(rtsp_header_t *header, const char *msg, int32_t msg_len, const char **endptr){
	const char *scan = NULL;
	const char *scan_end = NULL;
	const char *temp = NULL;
	const char *end = msg + msg_len;
	rtsp_error_t err = RTSP_E_FAILURE;

	if(!header || !msg || msg_len <= 0){
		return (RTSP_E_INVALID_ARG);
	}

	scan = msg;
	header->session.id = scan;
	while (scan < end && (isalnum(*scan) || is_safe(*scan)))
		scan++;

	header->session.id_len = scan-msg;
	if(header->session.id_len > 0)
		err = RTSP_E_SUCCESS;
	if(*scan == ';'){
		scan++;	
		scan_end = find_char(scan, end, '\r');
		err = is_strncasematch(scan, scan_end, "timeout", STATIC_STR_LEN("timeout"));
		if(err==RTSP_E_SUCCESS){
			scan+=STATIC_STR_LEN("timeout");
			if(*scan == '='){
				scan++;
				header->session.timeout = strtoul(scan,(char **)&temp,10);
				if(temp > (scan_end+1)){
					ERR_LOG("Parsing Session timeout Error - Check Logic");
					err = RTSP_E_FAILURE;
				} else {
					scan = temp;
				}
			}
		}
	} 
	if(endptr)
		*endptr = scan;
	return (err);

}

#endif
