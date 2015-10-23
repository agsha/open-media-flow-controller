/* File : rtsp_header.h
 * Copyright (C) 2008 - 2009 Nokeena Networks, Inc. 
 * All rights reserved.
 */
#ifndef _RTSP_HEADER_H
/** 
 * @file rtsp_header.h
 * @brief RTSP Header file
 * @author Patrick Mishel R
 * @version 1.00
 * @date 2009-09-24
 */

#define _RTSP_HEADER_H

#include "rtsp_def.h"
#include "rtsp_server.h"
#include "mime_header.h"
#include "alloca.h"

/**
 * @brief Gets a string allocated using nkn_malloc_type
 *
 * @param mime_hdr_ptr - pointer to the mime header structure
 *
 * (Return values )
 * @param dst_ptr - pointer to the allocated string filled with abs_url
 *
 * Pointer allocated in dst_ptr will free on the calling function returning
 *
 * @return
 */
#define RTSP_GET_ALLOC_URL(mime_hdr_ptr, dst_ptr){\
    unsigned int hdr_attr = 0;\
    int hdr_cnt = 0;\
    const char *scan = NULL;\
    char *url = NULL;\
    int len = 0;\
    if ( 0 == mime_hdr_get_known (mime_hdr_ptr, RTSP_HDR_X_URL, &scan, \
				  &len, &hdr_attr, &hdr_cnt)) {\
	url =  alloca(len+1);\
	assert(url);\
	memcpy(url, scan, len);\
	url[len] = '\0';\
	dst_ptr = url;\
    }\
}

/**
 * @brief Gets a string allocated using nkn_malloc_type
 *
 * @param mime_hdr_ptr - pointer to the mime header structure
 *
 * (Return values )
 * @param dst_ptr - pointer to the allocated string filled with abs_url
 *
 * Pointer allocated in dst_ptr will free on the calling function returning
 *
 * @return
 */
#define RTSP_GET_ALLOC_ABS_URL(mime_hdr_ptr, dst_ptr){\
    unsigned int hdr_attr = 0;\
    int hdr_cnt = 0;\
    const char *scan = NULL;\
    char *url = NULL;\
    int len = 0;\
    if ( 0 == mime_hdr_get_known (mime_hdr_ptr, RTSP_HDR_X_ABS_PATH, &scan, \
				  &len, &hdr_attr, &hdr_cnt)) {\
	url =  alloca(len+1);\
	assert(url);\
	memcpy(url, scan, len);\
	url[len] = '\0';\
	dst_ptr = url;\
    }\
}

/**
 * @brief Gets abs_path of RTSP URL Split into two components
 * abs_path = /level1/level2/level3/.../leveln
 * abs_path = [(pre_suffix)/level1/level2/level3/...]/[(suffix)leveln]
 *
 * @param mime_hdr_ptr
 *
 * (Return values )
 * @param pre_suffix
 * @param suffix
 *
 * @return
 */
#define RTSP_GET_ALLOC_URL_COMPONENT(mime_hdr_ptr, pre_suffix, suffix){\
    unsigned int hdr_attr = 0;\
    int hdr_cnt = 0;\
    char *scan = NULL;\
    int len = 0;\
    if ( 0 == mime_hdr_get_known ( mime_hdr_ptr, RTSP_HDR_X_ABS_PATH, \
				   (const char **)&scan, &len, \
				   &hdr_attr, &hdr_cnt) ) {\
	pre_suffix =  alloca(len+1);\
	assert(pre_suffix);\
	memcpy(pre_suffix, scan, len);\
	pre_suffix[len] = '\0';\
	suffix = &pre_suffix[len];\
 	scan = strrchr(pre_suffix, '/');\
	if(scan){\
	    *scan = '\0';\
	    scan++;\
	    suffix = scan;\
	}\
    }\
}

/**
 * @brief Known Header Descriptor
 */
extern mime_header_descriptor_t rtsp_known_headers[RTSP_HDR_MAX_DEFS];

/**
 * @brief Call back interface to Builds header on the allocated buffer passed
 *	  Functionality is defined on where the call back is registered
 *	  For Eg if registered in status table, it is linked with status code
 *
 * @param prtsp - RTSP Contect
 * @param buf - Buffer to be filled up
 * @param buf_len - Buffer length at hand
 * @param end - End of valid entries after building
 *
 * @return - Status
 */
typedef rtsp_parse_status_t (*rtsp_build_hdr_t)(rtsp_cb_t *prtsp, char *buf, int buf_len, char **end);

int rtsp_status_map_init (void);
int rtsp_status_map_exit (void);

/**
 * @brief table having the status description of rtsp status codes
 */
typedef struct rtsp_status_desc_t {
    rtsp_status_t id;
    int i_status_code;
    char c_status_code[RTSP_STATUS_CODE_LEN];
    const char *reason;
    int reason_len;

    /* Header building function to build
     * status code specific headers */
    rtsp_build_hdr_t build_hdr_fn;

    /* Counter Variable plugged in to indicate
     * number of status messages built */
    int *status_counter;
}rtsp_status_desc_t;

/**
 * @brief RTSP Status MAP
 */
extern rtsp_status_desc_t rtsp_status_map[RTSP_STATUS_MAX];

#if 0
/* 
 * We can use this after we are stable enough to avoid checks
 */
#define RTSP_STATUS_VALID_MAP(sid) (1)
#else
#define RTSP_STATUS_VALID_MAP(sid) ((IS_RTSP_STATUS_VALID(sid) && rtsp_status_map[sid].id == sid))
#endif

#define RTSP_STATUS_REASON_STR(sid) (RTSP_STATUS_VALID_MAP(sid) ? rtsp_status_map[sid].reason : NULL)
#define RTSP_STATUS_CODE_STR(sid) (RTSP_STATUS_VALID_MAP(sid) ? rtsp_status_map[sid].c_status_code : NULL)
#define RTSP_STATUS_CODE_NUM(sid) (RTSP_STATUS_VALID_MAP(sid) ? rtsp_status_map[sid].i_status_code : -1)
#define RTSP_STATUS_BUILD_HDR(sid) (RTSP_STATUS_VALID_MAP(sid) ? rtsp_status_map[sid].build_hdr_fn : NULL)

#define RTSP_STATUS_INCREMENT_COUNTER(sid) ((RTSP_STATUS_VALID_MAP(sid) && rtsp_status_map[sid].status_counter) \
						? rtsp_status_map[sid].status_counter++: ;)

#define RTSP_STATUS_REG_COUNTER(sid, counter_variable) (RTSP_STATUS_VALID_MAP(sid)\
                                                    ? rtsp_status_map[sid].status_counter = &counter_variable\
						    : assert(RTSP_STATUS_VALID_MAP(sid)));

#define RTSP_STATUS_REG_BUILD_HDR_FN(sid, function) (RTSP_STATUS_VALID_MAP(sid)\
                                                    ? rtsp_status_map[sid].build_hdr_fn = function\
						    : assert(RTSP_STATUS_VALID_MAP(sid)));

#define RTSP_REG_CNT_AND_FN(sid, counter, function) \
{\
    RTSP_STATUS_REG_COUNTER(sid, counter);\
    RTSP_STATUS_REG_BUILD_HDR_FN(sid, function);\
}

extern rtsp_parse_status_t
rtsp_build_response(rtsp_cb_t * prtsp, rtsp_status_t id, int sub_err_code);

/** 
 * @brief Validation function for RTSP
 *  Check RTSP Protocol limitations
 * 
 * @param prtsp
 * 
 * @return status
 */
extern rtsp_parse_status_t rtsp_validate_protocol(rtsp_cb_t *prtsp);


/** 
 * @brief Check MFD limitations
 * 
 * @param prtsp
 * 
 * @return protocol limitations
 */
extern rtsp_parse_status_t rtsp_check_mfd_limitation(rtsp_cb_t *prtsp);

#endif
