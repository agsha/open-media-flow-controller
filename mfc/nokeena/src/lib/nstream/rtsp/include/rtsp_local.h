/* File : rtsp_local.h Author : Patrick Mishel R
 * Copyright (C) 2008 - 2009 Nokeena Networks, Inc. 
 * All rights reserved.
 */
#ifndef _RTSP_LOCAL_H_
#define _RTSP_LOCAL_H_

/** 
 * @file rtsp_local.h
 * @brief Locol prototypes Included here
 * @author Patrick Mishel R
 * @version 1.00
 * @date 2009-08-03
 */


/* Parser */
extern rtsp_error_t rtsp_phdr_cb_init( void );

/* Composer */
extern rtsp_error_t rtsp_chdr_cb_init( void );

/* Validator */
extern int32_t rtsp_get_valid_host_len(const char *host, int32_t host_len);
extern rtsp_error_t rtsp_validate_host(const char *host, int32_t host_len);

#endif 
