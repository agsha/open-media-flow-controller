/* File : rtspc_conf.h Author : Patrick Mishel R
 * Copyright (C) 2008 - 2009 Nokeena Networks, Inc. 
 * All rights reserved.
 */
#ifndef _RTSPC_CONF_H
/** 
 * @file rtspc_conf.h
 * @brief Configuration Setting for Client
 * @author Patrick Mishel R
 * @version 1.00
 * @date 2009-08-08
 */
#define _RTSPC_CONF_H
/* 
 * Selected conf
 */
extern const uint32_t SESS_CONF_INDEX;

/* Hard Request Header Indices */
extern const uint32_t REQ_HDR_IDX_CSEQ;
extern const uint32_t REQ_HDR_IDX_ACCEPT;
extern const uint32_t REQ_HDR_IDX_TRANSPORT;
extern const uint32_t REQ_HDR_IDX_SESSION;
extern const uint32_t REQ_HDR_IDX_TEMP2;

extern const uint32_t MAX_SESS_CONF;
#endif
