/* File : parser_utils.h
 * Copyright (C) 2008 - 2009 Nokeena Networks, Inc. 
 * All rights reserved.
 */
#ifndef _PARSER_UTILS_H
/** 
 * @file parser_utils.h
 * @brief Parser Utility functions. Used by RTSP/HTTP Parsers
 * @author
 * @version 1.00
 * @date 2009-09-15
 */
#define _PARSER_UTILS_H

#ifndef ST_STRLEN
#define ST_STRLEN(static_string) (sizeof(static_string) - 1)
#endif

#include <ctype.h>

// Just skip leading space
static inline const char *
nkn_skip_space(const char *p)
{
    while (*p == ' ' || *p == '\t')
	p++;
    return p;
}

/*
 * Skip space, tab and comma and move to next header
 * value in comma separated value list.
 */
static inline const char *
nkn_skip_comma_space(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == ',')
        p++;
    return p;
}

/* 
 * Get pointer to each token in a list separated with
 * comma and optional space.
 * p_end is used to find the boundary condition when
 * extracting each token from a comma separated list
 * of tokens.
 * For eg: In list 'abc, def, hij', p_end will point 
 *         to char 'j' always.
 */
static inline const char *
nkn_comma_delim_start(const char *p,
                      const char *p_end)
{
    while (*p != ' ' && *p != '\t' && *p != ',' && p != p_end)
        p++;
    
    return ((p == p_end) ? (p+1) : p) ;
}

// Skip name trailing sapce, colon and value leading space
static inline const char *
nkn_skip_colon(const char *p)
{
    while (*p == ' ' || *p == '\t')
	p++;
    if (*p != ':')
	return p;
    p++;
    while (*p == ' ' || *p == '\t')
	p++;
    return p;
}

static inline const char *
nkn_skip_colon_check(const char *p, int *colon_check)
{
    while (*p == ' ' || *p == '\t')
	p++;
    if (*p != ':'){
	if (colon_check) *colon_check = 0;
	return p;
    }
    p++;
    while (*p == ' ' || *p == '\t')
	p++;
    if (colon_check) *colon_check = 1;
    return p;
}

static inline const char *
nkn_skip_to_nextline(const char *p)
{
	/* Check for multi-line headers
	 * BZ 3354, 3360
	 */
	while (*p != '\n') {
		while (*p != '\n') {
			if (*p == 0)
				return NULL;	// should never happen
			p++;
		}
		if ( *(p+1) == ' ' || *(p+1) == '\t' )
			p++;
	}
	return p + 1;
}

static inline const char *
nkn_find_digit_end(const char *p)
{
    while (*p >= '0' && *p <= '9')
	p++;
    return p - 1;
}

// caller should skip name leading space.
// trailing space is NOT part of name
static inline const char *
nkn_find_hdrname_end(const char *p)
{
    const char *ps = p;
    while (*p != ':') {
	if (*p == ' ') {
	    return p - 1;
	}
	if (*p == '\n') {
	    return p;		// didn't find ':' before '\n'
	}
	p++;
    }
    p--;
    if (ps == p)
	return p;		// case: only :
    return p;
}

// caller should skip value leading space.
// trailing space IS part of value
static inline const char *
nkn_find_hdrvalue_end(const char *p)
{
	const char *ps = p;

	/* Check for multi-line headers
	 * BZ 3354, 3360
	 */
	while (*p != '\n') {
		while (*p != '\n') {
			p++;
		}
		if ( *(p+1) == ' ' || *(p+1) == '\t' )
			p++;
	}
	if (ps == p)
		return ps;		// case: only \n
	if (*(p - 1) == '\r') {
		if (ps == p - 1)
			return p - 1;	// case: only \r\n
		return p - 2;		// legal case: aaa\r\n 
		// including special cases: 1. aaa\r\r\n 
		//                          2. aaa\raaa\r\n
	}
	return p - 1;		// case: aaa\n
}

// Skips SP  0x20 ref:RFC (HTTP/RTSP)
static inline const char *
nkn_skip_SP(const char *p)
{
	while (*p == ' ')
		p++;
	return p;
}

/** 
 * @brief Finds if character c belongs to tspecial token group ref: RFC
 * 
 * @param c
 * 
 * @return 
 */
static inline int
is_tspecial(char c)
{
	switch (c) {
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
			return (1);
		default:
			return (0);
	}
}

/** 
 * @brief Skips valid token characters till the delimiter is reached
 * 
 * @param p
 * @param delimiter
 * 
 * @return Returns pointer to the char next to the token end 
 *	   NUll on Failure
 */
static inline const char *
nkn_skip_token(const char *p, char delim_char)
{
	while (*p) {
		if (*p == delim_char) {
			/* Token end reached 
			 * Calling function has to ensure reliable delim_char as per spec
			 * */
			return p;
		} else if (iscntrl(*p) || is_tspecial(*p)) {
			/* 
			 * Validates Token as per R[15.1]
			 */
			return (NULL);
		} else {
			p++;
		}
	}
	return (p);
}


static inline int
is_text(char c)
{
    if (((c >= 0) && (c <= 30)) || (c == 127))
	return (0);
    return (1);
}

#endif
