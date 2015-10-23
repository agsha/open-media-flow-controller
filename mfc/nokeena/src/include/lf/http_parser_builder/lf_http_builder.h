#ifndef LFD_HTTP_BUILDER_H
#define LFD_HTTP_BUILDER_H

#include <stdint.h>

#include "lf_http_parser.h"

struct http_msg_builder;

typedef void (*delete_http_bld_fptr)(struct http_msg_builder*);

typedef int32_t (*http_bld_add_hdr_fptr)(struct http_msg_builder* http_bld, 
		char const* hdr, char const* value);

typedef int32_t (*http_bld_add_content_fptr)(struct http_msg_builder* http_bld, 
		uint8_t const* content, uint32_t content_len);

typedef void (*http_bld_get_msg_fptr)(struct http_msg_builder*, 
		uint8_t const** msg, uint32_t* msg_len);

typedef struct http_msg_builder {

	http_msg_ctx_t msg_ctx;
	uint8_t* buf;
	uint32_t filled_len;
	uint32_t total_len;
	uint32_t added_content_flag;

	http_bld_add_hdr_fptr add_hdr_hdlr;
	http_bld_add_content_fptr add_content_hdlr;
	http_bld_get_msg_fptr get_msg_hdlr;
	delete_http_bld_fptr delete_hdlr;
} http_msg_builder_t;


http_msg_builder_t* createHTTP_Response(char const* version,
		uint32_t status_code, char const* status_msg, uint32_t approx_buf_size);

http_msg_builder_t* createHTTP_Request(char const* version,
		char const* method, char const* path, uint32_t approx_buf_size);

#endif
