#ifndef _RTSPC_IF_
/* File : rtspc_if.h Author : Patrick Mishel R
 * Copyright (C) 2008 - 2009 Nokeena Networks, Inc. 
 * All rights reserved.
 */
#define _RTSPC_IF_
/** 
 * @file rtspc_if.h
 * @brief interface header file
 * 	TODO - Cleanup
 * @author Patrick Mishel R
 * @version 1.00
 * @date 2009-08-07
 */

#include <rtsp.h>
#include <rtsp_session.h>

/** 
 * @brief Initialize Session context
 * 
 * @param url - rtsp url in the format rtsp://host:port/abs_url
 * @param pp_sess - Returned rtsp session struct
 * 
 * @return 
 */
extern rtsp_error_t rtspc_create_session(char *url, rtsp_session_t **pp_sess);

/** 
 * @brief 
 * 
 * @param sess
 */
extern void rtspc_destroy_session(rtsp_session_t *sess);

#endif
