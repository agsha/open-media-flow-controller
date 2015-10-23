#ifndef LFD_HTTP_PARSER_H
#define LFD_HTTP_PARSER_H

#include <stdint.h>

typedef enum {

	HTTP_REQUEST,
	HTTP_RESPONSE
} http_msg_type_t;


typedef struct http_request {

	char method[20];
	char path[1024];
	char version[5];
} http_request_t;

typedef struct http_response {

	uint32_t code;
	char msg[1024];
	char version[5];
} http_response_t;


typedef union {
	http_request_t request;
	http_response_t response;
} http_msg_ctx_t;

struct parsed_http_msg;

typedef void (*delete_parsed_hhtp_msg_fptr)(struct parsed_http_msg*);

typedef struct parsed_http_msg {

	http_msg_type_t type;
	http_msg_ctx_t msg_ctx;
	uint32_t header_count;
	char** headers;
	char** values;
	uint8_t const* content;
	uint64_t content_len;

	delete_parsed_hhtp_msg_fptr delete_hdlr;
} parsed_http_msg_t;


parsed_http_msg_t* parseHTTP_Message(uint8_t const* msg);

void printParsedHttpMessage(parsed_http_msg_t const* msg);

#endif
