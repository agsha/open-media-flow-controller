/* File : rtspc_conf_private.h Author : Patrick Mishel R
 * Copyright (C) 2008 - 2009 Nokeena Networks, Inc. 
 * All rights reserved.
 */
#ifndef _SESS_CONF_PRIVATE_H_
/** 
 * @file rtspc_conf_private.h
 * @brief Configuration/Settings Pvt Data exported through rtsp_conf.h
 * @author Patrick Mishel R
 * @version 1.00
 * @date 2009-08-08
 */

#define _SESS_CONF_PRIVATE_H_

#include <rtsp_session.h>
#include <rtspc_conf.h>


#define SET_SESS_CONF(CONF_ID, URL_LEN, REQ_BUF_SIZE, RES_BUF_SIZE, REQ_HDR_COUNT, RES_HDR_COUNT, SCR_PAD_SIZE) \
{\
	.conf_id = CONF_ID,\
	.url_len = URL_LEN,\
	.sdp_size = RES_BUF_SIZE,\
	.req_buf_size = REQ_BUF_SIZE,\
	.res_buf_size = RES_BUF_SIZE,\
	.req_hdr_count = REQ_HDR_COUNT,\
	.res_hdr_count = RES_HDR_COUNT,\
	.scr_pad_size = SCR_PAD_SIZE,\
}

const rtsp_ses_conf_t conf_limits[] =
{
	SET_SESS_CONF(1, 0, (64*1024), (64*1024), 32, 32, (4*1024)),
	SET_SESS_CONF(2, 0, (32*1024), (32*1024), 8, 8, (4*1024)),
	SET_SESS_CONF(3, 0, (16*1024), (16*1024), 8, 8, (4*1024)),
	SET_SESS_CONF(4, 0, (8*1024), (8*1024), 8, 8, (4*1024)),
	SET_SESS_CONF(5, 0, (4*1024), (4*1024), 5, 5, (4*1024)),
	SET_SESS_CONF(0, 0, 0, 0, 0, 0, 0),
};

/* 
 * Selected conf
 */
const uint32_t SESS_CONF_INDEX = 0 ;

/* Hard Request Header Indices */
const uint32_t REQ_HDR_IDX_CSEQ = 0;
const uint32_t REQ_HDR_IDX_ACCEPT = 1;
const uint32_t REQ_HDR_IDX_TRANSPORT = 2;
const uint32_t REQ_HDR_IDX_SESSION = 3;
const uint32_t REQ_HDR_IDX_TEMP2 = 4;

const uint32_t MAX_SESS_CONF = ((sizeof(conf_limits)/sizeof(rtsp_ses_conf_t)) - 1 );

#endif 
