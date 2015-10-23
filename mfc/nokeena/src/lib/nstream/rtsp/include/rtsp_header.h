/* File : rtsp_header.h Author : Patrick Mishel R
 * Copyright (C) 2008 - 2009 Nokeena Networks, Inc. 
 * All rights reserved.
 */
#ifndef _RTSP_HEADER_H_
#define _RTSP_HEADER_H_
/** 
 * @file rtsp_header.h
 * @brief  Defining header structures 
 * Unsupported Headers may be parsed to Hdr ID and Contents
 * @author Patrick Mishel R
 * @version 1.00
 * @date 2009-07-15
 */

/**
 * @brief RTSP_HDR_ACCEPT - Request
 * Supports application/sdp only
 * TODO - Extend Data structure to hold all possible values 
 * as per protocol Grammar
 */
struct rtsp_hdr_accept { 
	/*
	 * Bit mask of requested application types 
	 */
	rtsp_app_type_t app_type; 
};

typedef struct rtsp_hdr_accept rtsp_hdr_accept_t;

/** 
 * @brief RTSP_HDR_ALLOW - Response
 * Supports all methods
 */
struct rtsp_hdr_allow {
	/*
	 * Bit mask of methods
	 */
	rtsp_method_bit_t method_mask; 
};

typedef struct rtsp_hdr_allow rtsp_hdr_allow_t;

/**
 * @brief RTSP_HDR_BANDWIDTH - Request
 * Positive Integer - Bits per Second
 */
struct rtsp_hdr_bandwidth {
	uint32_t bits_per_second;
};

typedef struct rtsp_hdr_bandwidth rtsp_hdr_bandwidth_t;

/**  
 * @brief RTSP_HDR_BLOCKSIZE - Request
 * Positive Integer - octets
 */
struct rtsp_hdr_blocksize {
	uint32_t bytes;
};

typedef struct rtsp_hdr_blocksize rtsp_hdr_blocksize_t;

/** 
 * @brief RTSP_HDR_CACHE_CONTROL - General
 * TODO - Define Types for different cache directives and add a bit mask.
 */
struct rtsp_hdr_cache_control {

};

typedef struct rtsp_hdr_cache_control rtsp_hdr_cache_control_t;

/** 
 * @brief RTSP_HDR_CONNECTION - General
 */
struct rtsp_hdr_connection {
	/* TODO - Use bit defines for known connection-tokens */
	char *token;
};

typedef struct rtsp_hdr_connection rtsp_hdr_connection_t;

/**
 * @brief RTSP_HDR_CONTENT_ENCODING - Entity
 */
struct rtsp_hdr_cont_encoding {
	/* TODO - Use bit defines for known content-codings */
	char *token;
};

typedef struct rtsp_hdr_cont_encoding rtsp_hdr_cont_encoding_t;

/**
 * @brief RTSP_HDR_CONTENT_LANGUAGE - Entity
 */
struct rtsp_hdr_cont_lang {
	char * language_tag;
};

typedef struct rtsp_hdr_cont_lang rtsp_hdr_content_lang_t;

/** 
 * @brief RTSP_HDR_CONTENT_LENGTH - Entity
 */
struct rtsp_hdr_cont_len {
	uint32_t bytes;
};

typedef struct rtsp_hdr_cont_len rtsp_hdr_cont_len_t;

/** 
 * @brief RTSP_HDR_CONTENT_TYPE - Entity - Response
 */
struct rtsp_hdr_cont_type {
	rtsp_app_type_t app_type; 
	char *undef_app_types;
};

typedef struct rtsp_hdr_cont_type rtsp_hdr_cont_type_t;

/** 
 * @brief RTSP_HDR_CSEQ - General
 */
struct rtsp_hdr_cseq { 
	uint32_t seq_num;
}; 

typedef struct rtsp_hdr_cseq rtsp_hdr_cseq_t; 

/** 
 * @brief RTSP_HDR_RTP_INFO - Response
 */
struct rtsp_hdr_RTP_info { 
	char *url;
	int32_t seq;
	int64_t rtptime;
};

typedef struct rtsp_hdr_RTP_info rstp_hdr_RTP_info_t;

/**
 * @brief RTSP_HDR_SESSION - Request, Response
 */
struct rtsp_hdr_session { 
	const char * id;
	int32_t id_len; 
	int32_t timeout; 
};

typedef struct rtsp_hdr_session rtsp_hdr_session_t;

/** 
 * @brief RTSP_HDR_TRANSPORT - Request, Response
 */
struct rtsp_hdr_transport {

	rtsp_tspec_t spec;
	int32_t ttl;
	int32_t layers;
	int32_t append;
	

	/* Port Numbers
	 * valid only if >= 0
	 */
	int32_t ileaved_ch_fr;
	int32_t ileaved_ch_to;
	int32_t port_fr;
	int32_t port_to;
	int32_t client_port_fr;
	int32_t client_port_to;
	int32_t server_port_fr;
	int32_t server_port_to;
	rtsp_method_t mode;
	char destination[RTSP_MAX_HOST_LEN + 1];
	uint64_t ssrc;

 /*	TODO */
 	char *next_transport;
	int32_t transport_len;
};

typedef struct rtsp_hdr_transport rtsp_hdr_transport_t;

/** 
 * @brief RTSP_HDR_RAW 
 */
struct rtsp_hdr_raw{
 /*	TODO */
 	const char *data;
	int32_t len;
};
typedef struct rtsp_hdr_raw rtsp_hdr_raw_t;
#endif
