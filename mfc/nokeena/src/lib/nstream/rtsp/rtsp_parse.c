/* File : rtsp_parse.c Author : Patrick Mishel R
 * Copyright (C) 2008 - 2009 Nokeena Networks, Inc. 
 * All rights reserved.
 */

/** 
 * @file rtsp_parse.c
 * @brief Parse and decode RTSP Message
 * @author Patrick Mishel R
 * @version 1.00
 * @date 2009-07-16
 */


#include <ctype.h>
#include <string.h>
#include <strings.h>

#include <rtsp.h>
#include <rtsp_table.h>
#include <rtsp_local.h>

#include "rtsp_parse_fns.c"
#include "rtsp_parse_header.c"

#define RTSP_V_STR "RTSP/"
#define RTSP_V_STR_LEN STATIC_STR_LEN("RTSP/")

#define RTSP_V_1_00_STR "RTSP/1.0"
#define RTSP_V_1_00_STR_LEN STATIC_STR_LEN(RTSP_V_1_00_STR)

#define NUM(num_char) ((uint32_t)(num_char - '0'))
#define RTSP_STATUS_CODE(x) ((isdigit(x[0]) && isdigit(x[1]) && isdigit(x[2])) ? ((NUM(x[0])*100) + (NUM(x[1])*10) + (NUM(x[2]))) : -1)

/* All memory passed to functions, 
 * even if length is given, should be '\0' terminated 
 * (may extend beyond the given length */


static inline const char *reason_phrase_end (const char *start, const char *end){
	if(start){
		while((start<end) && (is_text(*start))){
			start++;
		}
		if((start+2 <= end) && (*start++ == RTSP_CR) && (*start++ == RTSP_LF)){
			return (start);
		} else {
			return (NULL);
		}
	}
	return (NULL);
}

/** 
 * @brief  Gets rtsp url from the string passed
 * 	- $UT - Unit test this function before release
 * 
 * @param p_url - pointer to the url structure to be filled by this function
 * @param str - Input string
 * @param string_len - Length of the string (Should be null terminated even if length is given )
 * @param next_ptr - return offset 
 * 
 * @return rtsp_error_t
 */
static inline rtsp_error_t rtsp_get_url(rtsp_url_t *p_url, const char *str, int32_t string_len, const char **next_ptr){
	
	const char *scan=NULL, *end = NULL, *host=NULL, *abs_path=NULL;
	int32_t host_len=0, abs_path_len=0;
	int32_t sym_state = 0x00;
	uint32_t port = 0;

	if(!p_url || !str || string_len<=0){
		return (RTSP_E_INVALID_ARG);
	}

	/** 
	 * rtsp_URL  =   ( "rtsp:" | "rtspu:" ) "//" host [":" port][abs_path]
	 */

	/* Minimum length the URL can have is
	 * sizeof("rtsp://a") 
	 * since host can have a minimum len of 1 
	 * rtsp://a is a valid URL
	 */

	if(string_len <= (int32_t )STATIC_STR_LEN("rtsp://a")){
		return(RTSP_E_INCOMPLETE);
	}


	scan = str;
	end = str + string_len;
	if(('u' == tolower(scan[STATIC_STR_LEN("rtspu")-1]))
			&& (0 == strncasecmp(str, "rtspu://",STATIC_STR_LEN("rtspu://")))){
		p_url->type = RTSP_NW_UDP;
		scan += STATIC_STR_LEN("rtspu://");
	} else if(0 == strncasecmp(str, "rtsp://",STATIC_STR_LEN("rtsp://"))){
		p_url->type = RTSP_NW_TCP;
		scan += STATIC_STR_LEN("rtsp://");
	} else {
		return(RTSP_E_FAILURE);
	}

	/* host */
	host = scan;
	if(*scan == '-')
		return (RTSP_E_FAILURE);

	/* TODO  need to validate with proper spec
	 * Numbers allowed in start of name
	 * hname :: <name>*["." name]
	 * name :: <let or digit>[*[<let or digit or hyphen]<let or digit>]
	 * */
	#define STATE_DOT 0x01
	#define STATE_DASH 0x02 

	while((scan < end) && (*scan)){
		if(isalnum(*scan)){
			sym_state = 0x00;
		} else if (*scan == '-') {
			if(sym_state & STATE_DOT){
				break;
			}
			sym_state = STATE_DASH;
		} else if (*scan == '.') {
			if(sym_state & (STATE_DOT|STATE_DASH)){
				break;
			}
			sym_state = STATE_DOT;
		} else {
			/* Any other char End mark */
			break;
		}
		scan++;
	}

	if (sym_state){
		/* Host cannot be in this states below
		 * a .-
		 * b -.
		 * c ..
		 * */
		return (RTSP_E_FAILURE);
	} 
	host_len = scan - host;

	/* [":" port]*/
	if((scan < end) && *scan == ':'){
		scan++;
		while ((scan < end) && *scan && isdigit(*scan)){
			port = port*10 + (uint32_t)(*scan - '0');
			scan++;
		}
		if( (scan < end) && (port == 0)){
			return (RTSP_E_FAILURE);
		}
	}

	/* abs_path */
	/*
	 * TODO - validation according to grammar */

	abs_path = scan;
	while ((scan < end) &&  (*scan ) && (*scan != RTSP_CR) && (*scan != RTSP_LF) && (*scan != ' ')){
		scan++;
	}

	abs_path_len = scan - abs_path;

	if(scan > end) {
		return (RTSP_E_INCOMPLETE);
	}

	/* Copy Host and abs path to data structures */
	if(host_len >= RTSP_MAX_HOST_LEN || abs_path_len >= RTSP_MAX_ABS_PATH_LEN) {
		return (RTSP_E_URI_TOO_LARGE);
	}

	strncpy(p_url->host, host, host_len);
	p_url->host[host_len] = '\0';
	p_url->port = port;
	strncpy(p_url->abs_path, abs_path, abs_path_len);
	p_url->abs_path[abs_path_len] = '\0';

	if(next_ptr)
	    *next_ptr = scan;
	return (RTSP_E_SUCCESS);
}


/** 
 * @brief Get token in a string separated by a delimiter (Single Char)
 * 	Spec ref R[15.1]
 * 
 * @param pp_token 
 * @param token_len
 * @param input_string
 * @param input_str_len
 * @param delim_char
 * @param next_ptr
 * 
 * @return 
 */
static inline rtsp_error_t rtsp_get_token(const char **pp_token, int32_t *token_len, const char *input_string, int32_t input_str_len,  char delim_char, const char **next_ptr){
	/* 
	 * TODO - $EN - delim_char can be changed to an array of characters which may be delimiters for a token
	 */

	int32_t scan_index = 0;
	rtsp_error_t e_ret_val = RTSP_E_FAILURE;
	if(!pp_token || !token_len || !input_string || input_str_len<=0){
		return (RTSP_E_INVALID_ARG);
	}

	while(scan_index < input_str_len){
		if(input_string[scan_index] == delim_char){
			/* Token end reached 
			 * Calling function has to ensure reliable delim_char as per spec
			 * */
			*pp_token = input_string;
			*token_len = scan_index+1;
			if (next_ptr)
				*next_ptr = &input_string[scan_index+1];
			e_ret_val = RTSP_E_SUCCESS;
			break;
		} else if(iscntrl(input_string[scan_index]) || is_tspecial(input_string[scan_index])){
			/* 
	 		 * Validates Token as per R[15.1]
	 		 */
			e_ret_val = RTSP_E_FAILURE;
			break;
		} else {
			scan_index++;
		}
	}

	return (e_ret_val);
}

static inline rtsp_error_t rtsp_get_method(rtsp_method_t *p_method, const char *request_string, int32_t request_str_len, const char **next_ptr){
	rtsp_error_t e_ret_val = RTSP_E_FAILURE;
	uint32_t mthd_len = 0;

	if(!p_method && !request_string && request_str_len <=0){
		return (RTSP_E_INVALID_ARG);
	}

	e_ret_val = rtsp_table_get_method_type(p_method, request_string, request_str_len);
	if(e_ret_val == RTSP_E_SUCCESS){
		mthd_len = get_method_str_len( *p_method );
		/* Check this - Redundant Condition */
		if(mthd_len &&  (mthd_len <= request_str_len)){
			if(next_ptr){
				*next_ptr = (char *)(request_string + mthd_len);
			}
		}
	} else if (e_ret_val == RTSP_E_FAILURE){
		/* No Defined methods 
		 * Only Extension Methods possible 
		 * Validating token
		 */
		const char *p_token = NULL;
		int32_t token_len = 0;
		e_ret_val = rtsp_get_token(&p_token, &token_len, request_string, request_str_len, ' ', next_ptr);
		if(e_ret_val == RTSP_E_SUCCESS){
			/* 
			 * Valid extension-method [R6.1] found
			 * But we do not support any extensions now.
			 * TODO - Supported extension-methods decoding 
			 */
			e_ret_val = RTSP_E_UNSUPPORTED;
		}
	} else {
		/* Return error occured */
	}
	return (e_ret_val);
}

/** 
 * @brief Validate version string and get version
 * 
 * @param version returned version value
 * @param ver_string Input string
 * @param ver_str_len Input string length
 * @param next_ptr next pointer after version has been decoded
 * 
 * @return error state
 */
static inline rtsp_error_t rtsp_get_version(rtsp_version_t *version, const char *ver_string, int32_t ver_str_len, const char **next_ptr){

	if(!version && !ver_string && ver_str_len <= 0){
		return (RTSP_E_INVALID_ARG);
	}

	if(0 == strncasecmp(ver_string, RTSP_V_1_00_STR, RTSP_V_1_00_STR_LEN)){
		/* Found Known Version String */
		*version = RTSP_VER_1_00;
		if(next_ptr && (ver_str_len > RTSP_V_1_00_STR_LEN)){
			*next_ptr = (char *)(ver_string + RTSP_V_1_00_STR_LEN);
		} else {
			next_ptr = NULL;
		}
		return (RTSP_E_SUCCESS);
	} else if (0 == strncasecmp(ver_string, RTSP_V_STR, RTSP_V_STR_LEN)){
		/* Found Un - Known Version String */
		/* TODO */
		return (RTSP_E_UNSUPPORTED);
	} else if (0 == strncasecmp(ver_string, RTSP_V_STR, ver_str_len)){
		return (RTSP_E_INCOMPLETE);
	} else {
		/* Not Matching a version string */
		return (RTSP_E_FAILURE);
	}
}

/** 
 * @brief - Decoding string line as a status line
 * 
 * @param p_start_line
 * @param rtsp_msg_buffer
 * @param rtsp_msg_len
 * 
 * @return 
 */
static inline rtsp_error_t rtsp_parse_status_line(rtsp_start_line_t *p_start_line,const char *rtsp_msg_buffer, int32_t rtsp_msg_len , const char **endptr){

	const char *parse_string = NULL, *end = NULL, *temp = NULL;
	rtsp_error_t e_ret_val = RTSP_E_FAILURE;

	if(!p_start_line || !rtsp_msg_buffer || rtsp_msg_len <= 0){
		return (RTSP_E_INVALID_ARG);
	}
	/* R[7.1]
	 * Status-Line = RTSP-Version SP Status-Code SP Reason-Phrase CRLF
	 * Allowing multiple SP since RFC Text R[7.1] suggests
	 */

	parse_string = rtsp_msg_buffer;
	end = rtsp_msg_buffer + rtsp_msg_len;
	/* RTSP-Version */
	e_ret_val = rtsp_get_version(&p_start_line->version, parse_string, rtsp_msg_len, &parse_string);

	switch (e_ret_val){
	case RTSP_E_SUCCESS:
		/* Found Version String  Start Hence it is status line */ 
		p_start_line->msg_type = RTSP_MSG_RESPONSE;

		/* SP */
		temp = skip_SP(parse_string, end);
		if((NULL == temp) || (temp==parse_string)){
			/* No spaces */
			e_ret_val = RTSP_E_FAILURE;
		}else if(temp == end){
			/* Only spaces till end */
			e_ret_val = RTSP_E_INCOMPLETE;
			break;
		}
		parse_string = temp;

		/* Status-Code */
		if((parse_string + RTSP_STATUS_CODE_LEN) <= end){
			if(0 == (p_start_line->response.status_code = RTSP_STATUS_CODE(parse_string))){
				e_ret_val = RTSP_E_FAILURE;
				break;
			}
		} else {
			e_ret_val = RTSP_E_INCOMPLETE;
			break;
		}
		parse_string += RTSP_STATUS_CODE_LEN;

		/* SP */
		temp = skip_SP(parse_string, end);
		if((NULL == temp) || (temp==parse_string)){
			/* No spaces */
			e_ret_val = RTSP_E_FAILURE;
		}else if(temp==end){
			/* Only spaces till end */
			e_ret_val = RTSP_E_INCOMPLETE;
			break;
		}
		parse_string = temp;

		/* Reason-Phrase */

		p_start_line->response.reason_phrase = parse_string;
		while((parse_string <= end) && is_text(*parse_string))
			parse_string++;
			;
		/* CRLF */
		if((parse_string+1) <= end){
			if(RTSP_CR == *parse_string++ && RTSP_LF ==*parse_string++){
				p_start_line->response.reason_len = (int32_t)((rtsp_msg_buffer - parse_string)+ rtsp_msg_len);
			} else {
				e_ret_val = RTSP_E_FAILURE;
				break;
			}
		} else {
			e_ret_val = RTSP_E_INCOMPLETE;
			break;
		}

		if(endptr)
			*endptr = parse_string;
		e_ret_val = RTSP_E_SUCCESS;
		break;
	case RTSP_E_UNSUPPORTED:
		/* Version Not supported */
	case RTSP_E_FAILURE:
	default:
		/* Must Be Request-Line */
		break;
	}
	return (e_ret_val);
}

static inline rtsp_error_t rtsp_parse_request_line(rtsp_start_line_t *p_start_line, const char *rtsp_msg_buffer, int32_t rtsp_msg_len, const char **endptr){

	const char *parse_string = rtsp_msg_buffer;
	const char *end = rtsp_msg_buffer + rtsp_msg_len;
	const char *temp = NULL;
	rtsp_error_t e_ret_val = RTSP_E_FAILURE;

	if(!p_start_line || !rtsp_msg_buffer || rtsp_msg_len <= 0){
		return (RTSP_E_INVALID_ARG);
	}
	/* R[6.1]
	 * Request-Line = Method SP Request-URI SP RTSP-Version CRLF
	 * Allowing multiple SP since RFC Text H[5.1] suggests
	 */


	e_ret_val = rtsp_get_method(&p_start_line->request.method, parse_string, rtsp_msg_len, &parse_string);
	
	if(e_ret_val != RTSP_E_SUCCESS ){
		return (e_ret_val);
	}

	/* Found valid Method Hence it is request line */ 
	p_start_line->msg_type = RTSP_MSG_REQUEST;

    /* SP */
	temp = skip_SP(parse_string, end);
	if((NULL == temp) || (temp==parse_string)){
		/* No spaces */
		e_ret_val = RTSP_E_FAILURE;
		return(e_ret_val);
	}else if(temp==end){
		/* Only spaces till end */
		e_ret_val = RTSP_E_INCOMPLETE;
		return(e_ret_val);
	}
	parse_string = temp;

	/* Request-URI  */
	e_ret_val = rtsp_get_url(&p_start_line->request.url ,parse_string, (rtsp_msg_len - (parse_string - rtsp_msg_buffer)), &parse_string);
	if(e_ret_val != RTSP_E_SUCCESS){
		ERR_LOG("Failed to parse URI");
		return (e_ret_val);
	}

	/* SP */
	temp = skip_SP(parse_string, end);
	if((NULL == temp) || (temp==parse_string)){
		/* No spaces */
		e_ret_val = RTSP_E_FAILURE;
		return(e_ret_val);
	}else if(temp==end){
		/* Only spaces till end */
		e_ret_val = RTSP_E_INCOMPLETE;
		return(e_ret_val);
	}
	parse_string = temp;

	/* RTSP-Version */
	e_ret_val = rtsp_get_version(&p_start_line->version, parse_string, (rtsp_msg_len - (parse_string - rtsp_msg_buffer)) , &parse_string);
	
	if(e_ret_val != RTSP_E_SUCCESS){
		return (e_ret_val);
	}

	/* CRLF */
	if((parse_string+2)  < end){
		if((*parse_string++ == RTSP_CR) && (*parse_string++ == RTSP_LF)){
			e_ret_val = RTSP_E_SUCCESS;
		} else {
			e_ret_val = RTSP_E_FAILURE;
		}
	} else {
		e_ret_val = RTSP_E_INCOMPLETE;
	}

	if(endptr)
		*endptr = parse_string;
	return (e_ret_val);
}

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
rtsp_error_t rtsp_parse_start_line(rtsp_start_line_t *p_start_line, const char *rtsp_msg_buffer, int32_t rtsp_msg_len, const char **endptr){

	/* 
	 * 1. Check if Status-line
	 * 2. If (Status line)
	 * 	Parse (Response)
	 * 3. Else If (Request line)
	 *  Parse (Request)
	 * 4. Else 
	 *  Parse Extentions
	 */

	rtsp_error_t e_ret_val = RTSP_E_FAILURE;

	if(!p_start_line || !rtsp_msg_buffer || !rtsp_msg_len){
		return (RTSP_E_INVALID_ARG);
	}

	e_ret_val = rtsp_parse_status_line(p_start_line,rtsp_msg_buffer,rtsp_msg_len, endptr);

	if(e_ret_val == RTSP_E_FAILURE){
		/* Check if it is Request Line */
		e_ret_val = rtsp_parse_request_line(p_start_line, rtsp_msg_buffer, rtsp_msg_len, endptr);
	} 
	
	/* 
	 * Handle Error Cases for Request and Response
	 */
	return (e_ret_val);
}

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
rtsp_error_t rtsp_parse_header(rtsp_header_t *header, const char * rtsp_msg_buffer, int32_t rtsp_msg_len, const char **endptr){

	const char *scan = rtsp_msg_buffer;
	const char *end = rtsp_msg_buffer + rtsp_msg_len;

	rtsp_error_t e_ret_val = RTSP_E_FAILURE;
	int32_t hdr_len;
	rtsp_phdr_cb_t parse_hdr_cb = NULL;

	if(!header || !rtsp_msg_buffer || !rtsp_msg_len){
		return (RTSP_E_INVALID_ARG);
	}

	e_ret_val = rtsp_table_get_header_type(&header->id, rtsp_msg_buffer, rtsp_msg_len);
	if(e_ret_val == RTSP_E_SUCCESS){
		hdr_len = get_header_str_len(header->id);
		if(hdr_len &&  (hdr_len <= rtsp_msg_len)){
			scan += hdr_len;
		} else {
			e_ret_val = RTSP_E_INCOMPLETE;
		}
	} else {
		/* 
			* extention-header might be possible 
			* TODO - Validation and return value */
		e_ret_val = (e_ret_val == RTSP_E_INCOMPLETE)? e_ret_val : RTSP_E_UNSUPPORTED;
		/* Parse to a Raw header format 
		 * Do validation later */
		header->id = RTSP_HDR_RAW;
		header->type = 0;
		header->raw.data = rtsp_msg_buffer;
		while (((scan+1) < end) && !((*scan == RTSP_CR) && (*(scan+1) == RTSP_LF))){
			scan++;
		}
		if((scan+1) >= end){
			return (RTSP_E_INCOMPLETE);
		}
		scan +=2;
		if(endptr)
			*endptr = scan;
		return(RTSP_E_SUCCESS);
	}

	if(e_ret_val != RTSP_E_SUCCESS)
		return(e_ret_val);
	
	if(scan >= end) 
		return (RTSP_E_INCOMPLETE);

	if(*scan++ != ':')
		return (RTSP_E_FAILURE);
	
	scan = skip_WS(scan, end);
	if(scan >= end){
		/* Only spaces till end */
		return (RTSP_E_INCOMPLETE);
	}

	/* Callback for respective headers */

	parse_hdr_cb = rtsp_table_get_phdr_cb(header->id);
	if(parse_hdr_cb != NULL){
		e_ret_val = (*parse_hdr_cb)( header, scan, end-scan, &scan);
		if(e_ret_val != RTSP_E_SUCCESS){
			return (e_ret_val);
		}
	} 

	if(parse_hdr_cb == NULL || e_ret_val != RTSP_E_SUCCESS){
		/* Parsing into RAW header */
		header->id = RTSP_HDR_RAW;
		header->type = 0;
		header->raw.data = rtsp_msg_buffer;
		while (((scan+1) < end) && !((*scan == RTSP_CR) && (*(scan+1) == RTSP_LF))){
			scan++;
		}
		if((scan+1) >= end){
			return (RTSP_E_INCOMPLETE);
		}
		scan +=2;
		if(endptr)
			*endptr = scan;
		return(RTSP_E_SUCCESS);
	}

	scan = skip_WS(scan, end);
	if(scan >= end){
		/* Only spaces till end */
		return (RTSP_E_INCOMPLETE);
	}

	/* CRLF */ 
	if((scan +2)  < end){
		if((*scan ++ == RTSP_CR) && (*scan ++ == RTSP_LF)){
			e_ret_val = RTSP_E_SUCCESS;
		} else {
			e_ret_val = RTSP_E_FAILURE;
		}
	} else {
		e_ret_val = RTSP_E_INCOMPLETE;
	}

	if(endptr)
		*endptr = scan;

	return (e_ret_val);

}

rtsp_error_t get_rtsp_url(rtsp_url_t *p_url, const char *str, int32_t string_len){
	return rtsp_get_url(p_url, str, string_len, NULL);
}

/** 
 * @brief Registers defined Call backs for Parsing Headers
 * 
 * @return error codes
 */
extern rtsp_error_t rtsp_phdr_cb_init( void ) {

	rtsp_error_t err = RTSP_E_SUCCESS;

	err = rtsp_table_plugin_phdr_cb(RTSP_HDR_CSEQ,rtsp_phdr_cseq);
	if(err != RTSP_E_SUCCESS){
		return err;
	}

	err = rtsp_table_plugin_phdr_cb(RTSP_HDR_CONTENT_LENGTH,rtsp_phdr_content_len);
	if(err != RTSP_E_SUCCESS){
		return err;
	}

	err = rtsp_table_plugin_phdr_cb(RTSP_HDR_TRANSPORT,rtsp_phdr_transport);
	if(err != RTSP_E_SUCCESS){
		return err;
	}

	err = rtsp_table_plugin_phdr_cb(RTSP_HDR_SESSION,rtsp_phdr_session);
	if(err != RTSP_E_SUCCESS){
		return err;
	}

	/* Add header parse functions to plugin here */

	return (err);
}
