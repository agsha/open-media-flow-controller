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


#ifndef TRUE
#define TRUE (1)
#endif

#ifndef TRUE
#define FALSE (0)
#endif

#define RTSP_CR ((char ) 0x0D)
#define RTSP_LF ((char ) 0x0A)

#define RTSP_STATUS_CODE_LEN 3

#define ST_STRLEN(x) (sizeof(x) - 1)

#define RTSP_V_STR "RTSP/"
#define RTSP_V_STR_LEN ST_STRLEN("RTSP/")

#define RTSP_MAX_HOST_LEN 256
#define RTSP_MAX_ABS_PATH_LEN 256

/** 
 * @brief FIXME - Max URL Length is not limited because of parser code
 * The buffer used internally in vfs_fopen limits this to 256
 */
#define RTSP_MAX_URL_LEN 256

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
    RTSP_HDR_ACCEPT = 0x0,		// 0x0000 0000 0000 0001
    RTSP_HDR_ACCEPT_ENCODING,		// 0x0000 0000 0000 0002
    RTSP_HDR_ACCEPT_LANGUAGE,		// 0x0000 0000 0000 0004
    RTSP_HDR_ALLOW,			// 0x0000 0000 0000 0008
    RTSP_HDR_AUTHORIZATION,		// 0x0000 0000 0000 0010
    RTSP_HDR_BANDWIDTH,			// 0x0000 0000 0000 0020
    RTSP_HDR_BLOCKSIZE,			// 0x0000 0000 0000 0040
    RTSP_HDR_CACHE_CONTROL,		// 0x0000 0000 0000 0080
    RTSP_HDR_CONFERENCE,		// 0x0000 0000 0000 0100
    RTSP_HDR_CONNECTION,		// 0x0000 0000 0000 0200
    RTSP_HDR_CONTENT_BASE,		// 0x0000 0000 0000 0400
    RTSP_HDR_CONTENT_ENCODING,		// 0x0000 0000 0000 0800
    RTSP_HDR_CONTENT_LANGUAGE,		// 0x0000 0000 0000 1000
    RTSP_HDR_CONTENT_LENGTH,		// 0x0000 0000 0000 2000
    RTSP_HDR_CONTENT_LOCATION,		// 0x0000 0000 0000 4000
    RTSP_HDR_CONTENT_TYPE,		// 0x0000 0000 0000 8000
    RTSP_HDR_CSEQ,			// 0x0000 0000 0001 0000
    RTSP_HDR_DATE,			// 0x0000 0000 0002 0000
    RTSP_HDR_EXPIRES,			// 0x0000 0000 0004 0000
    RTSP_HDR_FROM,			// 0x0000 0000 0008 0000
    RTSP_HDR_IF_MODIFIED_SINCE,		// 0x0000 0000 0010 0000
    RTSP_HDR_LAST_MODIFIED,		// 0x0000 0000 0020 0000
    RTSP_HDR_PROXY_AUTHENTICATE,	// 0x0000 0000 0040 0000
    RTSP_HDR_PROXY_REQUIRE,		// 0x0000 0000 0080 0000
    RTSP_HDR_PUBLIC,			// 0x0000 0000 0100 0000
    RTSP_HDR_RANGE,			// 0x0000 0000 0200 0000
    RTSP_HDR_REFERER,			// 0x0000 0000 0400 0000
    RTSP_HDR_REQUIRE,			// 0x0000 0000 0800 0000
    RTSP_HDR_RETRY_AFTER,		// 0x0000 0000 1000 0000
    RTSP_HDR_RTP_INFO,			// 0x0000 0000 2000 0000
    RTSP_HDR_SCALE,			// 0x0000 0000 4000 0000
    RTSP_HDR_SESSION,			// 0x0000 0000 8000 0000
    RTSP_HDR_SERVER,			// 0x0000 0001 0000 0000
    RTSP_HDR_SPEED,			// 0x0000 0002 0000 0000
    RTSP_HDR_TRANSPORT,			// 0x0000 0004 0000 0000
    RTSP_HDR_UNSUPPORTED,		// 0x0000 0008 0000 0000
    RTSP_HDR_USER_AGENT,		// 0x0000 0010 0000 0000
    RTSP_HDR_VIA,			// 0x0000 0020 0000 0000
    RTSP_HDR_WWW_AUTHENTICATE,		// 0x0000 0040 0000 0000

    /* 
     * Supported Extended headers 
     */
    RTSP_HDR_X_PLAY_NOW,						// 0x0000 0080 0000 0000

    RTSP_HDR_MAX_RFC_DEFS,						// 0x0000 0100 0000 0000
    /* 
     * Nokeena Headers		Ref: RFC 2326 section 3.2	    
     * These heaers are not part of the rfc header definition
     * but defined for use with the mime header library*/
    RTSP_HDR_X_METHOD,	    /* Method name for RTSP (OTW)	    */	// 0x0000 0200 0000 0000
    RTSP_HDR_X_PROTOCOL,    /* protocol (rtsp:// || rtspu://)(OTW)  */	// 0x0000 0400 0000 0000
    RTSP_HDR_X_HOST,	    /* host (OTW)			    */	// 0x0000 0800 0000 0000
    RTSP_HDR_X_PORT,	    /* [:port] (OTW)                        */	// 0x0000 1000 0000 0000
    RTSP_HDR_X_URL,	    /* rtsp_URL	full url    [R3.2](OTW)	    */	// 0x0000 2000 0000 0000
    RTSP_HDR_X_ABS_PATH,    /* abs_path following (OTW)		    */	// 0x0000 4000 0000 0000
    RTSP_HDR_X_VERSION,     /* RTSP-Version  (OTW)		    */	// 0x0000 8000 0000 0000
    RTSP_HDR_X_VERSION_NUM, /* RTSP-Version number part only eg 1.0 */	// 0x0001 0000 0000 0000
    RTSP_HDR_X_STATUS_CODE, /* Status-Code   (OTW)		    */	// 0x0002 0000 0000 0000
    RTSP_HDR_X_REASON,	    /* Reason-Phrase (OTW)		    */	// 0x0004 0000 0000 0000
    RTSP_HDR_X_QT_LATE_TOLERANCE,					// 0x0008 0000 0000 0000
    RTSP_HDR_X_NKN_REQ_REAL_DEST_IP,					// 0x0010 0000 0000 0000
    RTSP_HDR_X_QUERY,	    /* Query part			    */	// 0x0020 0000 0000 0000
    RTSP_HDR_MAX_DEFS,							// 0x0040 0000 0000 0000

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

#define RTSP_STATUS_100 RTSP_STATIS_CONTINUE
#define RTSP_STATUS_200 RTSP_STATIS_OK

typedef enum rtsp_status_t {
	RTSP_STATUS_INV = -1,
	RTSP_STATUS_100 = 0,
	RTSP_STATUS_200,
	RTSP_STATUS_201,
	RTSP_STATUS_250,
	RTSP_STATUS_300,
	RTSP_STATUS_301,
	RTSP_STATUS_302,
	RTSP_STATUS_303,
	RTSP_STATUS_304,
	RTSP_STATUS_305,
	RTSP_STATUS_400,
	RTSP_STATUS_401,
	RTSP_STATUS_402,
	RTSP_STATUS_403,
	RTSP_STATUS_404,
	RTSP_STATUS_405,
	RTSP_STATUS_406,
	RTSP_STATUS_407,
	RTSP_STATUS_408,
	RTSP_STATUS_410,
	RTSP_STATUS_411,
	RTSP_STATUS_412,
	RTSP_STATUS_413,
	RTSP_STATUS_414,
	RTSP_STATUS_415,
	RTSP_STATUS_451,
	RTSP_STATUS_452,
	RTSP_STATUS_453,
	RTSP_STATUS_454,
	RTSP_STATUS_455,
	RTSP_STATUS_456,
	RTSP_STATUS_457,
	RTSP_STATUS_458,
	RTSP_STATUS_459,
	RTSP_STATUS_460,
	RTSP_STATUS_461,
	RTSP_STATUS_462,
	RTSP_STATUS_500,
	RTSP_STATUS_501,
	RTSP_STATUS_502,
	RTSP_STATUS_503,
	RTSP_STATUS_504,
	RTSP_STATUS_505,
	RTSP_STATUS_551,
	RTSP_STATUS_MAX,
}rtsp_status_t;

#define IS_RTSP_STATUS_VALID(status) ((status>=RTSP_STATUS_100 && status<RTSP_STATUS_MAX) ? 1 : 0)

#endif

