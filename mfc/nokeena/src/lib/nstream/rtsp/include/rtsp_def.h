/* File : rtsp_def.h Author : Patrick Mishel R
 * Copyright (C) 2008 - 2009 Nokeena Networks, Inc. 
 * All rights reserved.
 */
#ifndef _RTSP_DEF_H_
#define _RTSP_DEF_H_

/** 
 * @file rtsp_def.h
 * @brief Defines for RTSP (RFC - 2326)
 * @author Patrick Mishel R
 * @version 1.00
 * @date 2009-07-15
 */

typedef uint32_t bool_t;

#define TRUE (1)
#define FALSE (0)

#define RTSP_CR ((char ) 0x0D)
#define RTSP_LF ((char ) 0x0A)

#define RTSP_STATUS_CODE_LEN 3
#define RTSP_MAX_HOST_LEN 256
#define RTSP_MAX_ABS_PATH_LEN 256

#define STATIC_STR_LEN(x) (sizeof(x) - 1)


//#define GET_HEX(c)  (isxdigit(c) ? (isdigit(c)? (c-'0') : (tolower(c) - 'a' + 0x0A)): -1)
	

/** 
 * @brief Supported RTSP version numbers
 */
typedef enum rtsp_version_t{
	RTSP_VER_1_00 = 1,
}rtsp_version_t;

typedef enum rtsp_msg_type_t {
	RTSP_MSG_INVALID = 0x00,
	RTSP_MSG_REQUEST = 0x01,
	RTSP_MSG_RESPONSE = 0x02,
}rtsp_msg_type_t;


/*
 * RTSP Protocol Headers - RFC [Page 45]
 */
typedef enum rtsp_header_id_t {
	RTSP_HDR_NULL = 0x00,
	RTSP_HDR_ACCEPT = 0x01,
    RTSP_HDR_ACCEPT_ENCODING, 
    RTSP_HDR_ACCEPT_LANGUAGE, 
    RTSP_HDR_ALLOW, 
    RTSP_HDR_AUTHORIZATION, 
    RTSP_HDR_BANDWIDTH, 
    RTSP_HDR_BLOCKSIZE, 
    RTSP_HDR_CACHE_CONTROL, 
    RTSP_HDR_CONFERENCE, 
    RTSP_HDR_CONNECTION, 
    RTSP_HDR_CONTENT_BASE, 
    RTSP_HDR_CONTENT_ENCODING, 
    RTSP_HDR_CONTENT_LANGUAGE, 
    RTSP_HDR_CONTENT_LENGTH, 
    RTSP_HDR_CONTENT_LOCATION, 
    RTSP_HDR_CONTENT_TYPE, 
    RTSP_HDR_CSEQ, 
    RTSP_HDR_DATE, 
    RTSP_HDR_EXPIRES, 
    RTSP_HDR_FROM, 
    RTSP_HDR_IF_MODIFIED_SINCE, 
    RTSP_HDR_LAST_MODIFIED, 
    RTSP_HDR_PROXY_AUTHENTICATE,
    RTSP_HDR_PROXY_REQUIRE,
    RTSP_HDR_PUBLIC,
    RTSP_HDR_RANGE,
    RTSP_HDR_REFERER,
    RTSP_HDR_REQUIRE,
    RTSP_HDR_RETRY_AFTER,
    RTSP_HDR_RTP_INFO,
    RTSP_HDR_SCALE,
    RTSP_HDR_SESSION,
    RTSP_HDR_SERVER,
    RTSP_HDR_SPEED,
    RTSP_HDR_TRANSPORT,
    RTSP_HDR_UNSUPPORTED,
    RTSP_HDR_USER_AGENT,
    RTSP_HDR_VIA,
    RTSP_HDR_WWW_AUTHENTICATE,
    RTSP_HDR_RAW,
}rtsp_header_id_t;

#define IS_VALID_HEADER(header) (((header>=RTSP_HDR_ACCEPT) && (header<=RTSP_HDR_WWW_AUTHENTICATE))? TRUE : FALSE)

/*
 * Classification of headers by type
 */
typedef enum rtsp_hdr_type_t {
	RTSP_TYPE_NULL = 0x00,
	RTSP_TYPE_REQUEST = 0x01,
	RTSP_TYPE_RESPONSE = 0x02,
	RTSP_TYPE_ENTITY = 0x04,
	RTSP_TYPE_GENERAL = 0x08
}rtsp_hdr_type_t;


/* 
 * Flag define indication of support
 */
typedef enum rtsp_support_t {
	RTSP_NOT_SUPPORTED = 0x00,
	RTSP_SUPPORTED = 0x01,
}rtsp_support_t;


/* 
 * Representing Methods - Number Defined
 */
typedef enum rtsp_method_t {
	RTSP_MTHD_NULL = 0x00,
	RTSP_MTHD_DESCRIBE = 0x01,
	RTSP_MTHD_ANNOUNCE,
	RTSP_MTHD_GET_PARAMETER,
	RTSP_MTHD_OPTIONS,
	RTSP_MTHD_PAUSE,
	RTSP_MTHD_PLAY,
	RTSP_MTHD_RECORD,
	RTSP_MTHD_REDIRECT,
	RTSP_MTHD_SETUP,
	RTSP_MTHD_SET_PARAMETER,
	RTSP_MTHD_TEARDOWN,
}rtsp_method_t;


/* 
 * Representing Methods - Bit Defined
 */
typedef enum rtsp_method_bit_t {
	RTSP_MTHD_BIT_NULL = 0x0000,
	RTSP_MTHD_BIT_DESCRIBE = 0x0001,
	RTSP_MTHD_BIT_ANNOUNCE = 0x0002,
	RTSP_MTHD_BIT_GET_PARAMETER = 0x0004,
	RTSP_MTHD_BIT_OPTIONS = 0x0008,
	RTSP_MTHD_BIT_PAUSE = 0x0010,
	RTSP_MTHD_BIT_PLAY = 0x0020,
	RTSP_MTHD_BIT_RECORD = 0x0040,
	RTSP_MTHD_BIT_REDIRECT = 0x0080,
	RTSP_MTHD_BIT_SETUP = 0x0100,
	RTSP_MTHD_BIT_SET_PARAMETER = 0x0200,
	RTSP_MTHD_BIT_TEARDOWN = 0x0400,
}rtsp_method_bit_t;

#define IS_VALID_METHOD(method) (((method>=RTSP_MTHD_DESCRIBE) && (method<=RTSP_MTHD_TEARDOWN))? TRUE : FALSE)

/*
 * Representing application protocol types
 */
typedef enum rtsp_app_type_t {
	RTSP_APP_TYPE_NULL = 0x00,
	RTSP_APP_TYPE_SDP = 0x01,
}rtsp_app_type_t;

typedef enum rtsp_url_type_t {
	RTSP_NW_STAR = 0x01,/* "*" */
	RTSP_NW_TCP = 0x02, /* "rtsp:// */
	RTSP_NW_UDP = 0x03, /* rtspu:// */
}rtsp_url_type_t;

typedef enum rtsp_state_t {
	RTSP_STATE_INIT =  0x00,
	RTSP_STATE_READY ,
	RTSP_STATE_PLAYING ,
	RTSP_STATE_RECORDING ,
} rtsp_state_t;

/* Transistion
 */
typedef enum rtsp_trans_t {
	RTSP_NO_CHANGE = 0x00,
	RTSP_ADVANCE,
	RTSP_RESET,
} rtsp_trans_t;


/** 
 * @brief Transport Spec
 * Transport Protocol, Profile and Lower
 */
typedef enum rtsp_tspec_t {
	TSPEC_RTP_AVP_UNICAST = ( 1 << 0 ),
	TSPEC_RTP_AVP_MULTICAST = ( 1 << 1 ),
	TSPEC_RTP_AVP_INTERLEAVED = ( 1 << 2 ),

	TSPEC_RTP_AVP_UDP = ( 1 << 3),
	TSPEC_RTP_AVP_TCP = ( 1 << 4),

	TSPEC_RTP_AVP_UC_UDP = (TSPEC_RTP_AVP_UNICAST | TSPEC_RTP_AVP_UDP),
	TSPEC_RTP_AVP_MC_UDP = (TSPEC_RTP_AVP_MULTICAST | TSPEC_RTP_AVP_UDP), 
} rtsp_tspec_t;


#endif

