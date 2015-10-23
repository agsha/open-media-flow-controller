/* File : rtsp_error.h Author : Patrick Mishel R
 * Copyright (C) 2008 - 2009 Nokeena Networks, Inc. 
 * All rights reserved.
 */
#ifndef _RTSP_ERROR_H_
#define _RTSP_ERROR_H_
/** 
 * @file rtsp_error.h
 * @brief Error Defines 
 * @author Patrick Mishel R
 * @version 1.00
 * @date 2009-07-15
 */

#include <stdio.h>

/** 
 * @brief Error Values used in rtsp parser library
 * TODO - Improve clean interface for Errors
 */
enum rtsp_error {
	RTSP_E_SUCCESS = 0,
	RTSP_E_FAILURE = 1,
	RTSP_E_INVALID_ARG = 2,
	RTSP_E_MEM_ALLOC_FAIL = 3,
	RTSP_E_URI_TOO_LARGE = 4,
	RTSP_E_UNSUPPORTED = 5,
	RTSP_E_VER_NOT_SUP = 6,
	RTSP_E_NO_SPACE = 7,
	RTSP_E_NOT_IMPLEMENTED = 8,
	RTSP_E_INVALID_HOST = 9,
	RTSP_E_INVALID_URI = 10,
	RTSP_E_INCOMPLETE = 11,
	RTSP_E_RETRY = 12,
};

typedef enum rtsp_error rtsp_error_t;

#define ERR_LOG(str, x...) fprintf(stderr,"E:%s:%d: "str"\n", __FILE__, __LINE__, ##x)

#define STAT_LOG(str, x...) fprintf(stderr,"S:%s:%d: "str"\n", __FILE__, __LINE__, ##x)
#endif
