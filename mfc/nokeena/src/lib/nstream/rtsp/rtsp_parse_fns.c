/* File : rtsp_parse_fns.c Author : Patrick Mishel R
 * Copyright (C) 2008 - 2009 Nokeena Networks, Inc. 
 * All rights reserved.
 */
#ifndef _PARSE_FNS_C_
/** 
 * @file rtsp_parse_fns.c
 * @brief 
 * @author Patrick Mishel R
 * @version 1.00
 * @date 2009-08-20
 */
#define _PARSE_FNS_C_
#include <rtsp_def.h>

static inline const char *skip_SP(const char *start, const char *end){
	if(start){
		while ((start <= end) && (' ' == *start))
			start++;
	}
	return (start);
}

static inline const char *skip_WS(const char *start, const char *end){
	if(start){
		while ((start <= end) && (*start!=RTSP_CR) && (*start!=RTSP_LF) && (isspace(*start)))
			start++;
	}
	return (start);
}

/** 
 * @brief Finds if character c belongs to tspecial token group
 * 
 * @param c
 * 
 * @return 
 */
static inline int32_t is_tspecial(char c){
	switch (c){
		case '(':
		case ')':
		case '<':
		case '>':
		case '@':
		case ',':
		case ';':
		case ':':
		case '\\':
		case '"':
		case '?':
		case '=':
		case '{':
		case '}':
		case ' ':
		case '\t':
			return (TRUE) ;
		default:
			return (FALSE);
	}
}

static inline int32_t is_safe(char c){
	switch (c){
		case '$':
		case '-':
		case '_':
		case '.':
		case '+':
			return (TRUE) ;
		default:
			return (FALSE);
	}
}

static inline int32_t is_text(char c){
	if(((c >= 0) && (c<=30)) ||  (c==127))
		return (FALSE);
	return (TRUE);
}

#endif
