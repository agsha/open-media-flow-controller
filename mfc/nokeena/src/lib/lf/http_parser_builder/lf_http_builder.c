#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lf_http_builder.h"

#define DEFAULT_MSG_BUF_LEN 4096

#define DEFAULT_MSG_BUF_INCR 1024 

static void init(http_msg_builder_t* http_bldr);
static void delete(http_msg_builder_t* http_bldr); 
static int32_t addHdr(http_msg_builder_t* http_bldr, char const* hdr,
		char const* value); 
static int32_t addContent(http_msg_builder_t* http_bldr, uint8_t const* content,
		uint32_t content_len); 
static void getMsg(http_msg_builder_t* http_bldr, uint8_t const** msg,
		uint32_t* msg_len);
static int32_t addToBuffer(http_msg_builder_t* http_bldr, uint8_t const* data,
		uint32_t data_len); 

http_msg_builder_t* createHTTP_Response(char const* version,
		uint32_t status_code, char const* status_msg,
		uint32_t approx_buf_size) {

	if ((version == NULL) || (status_code == 0) || (status_msg == NULL))
		return NULL;
	http_msg_builder_t* http_bldr = calloc(1, sizeof(http_msg_builder_t));
	if (http_bldr == NULL)
		return NULL;
	init(http_bldr);

	strncpy(&http_bldr->msg_ctx.response.version[0], version, 5);
	http_bldr->msg_ctx.response.version[4] = '\0';
	strncpy(&http_bldr->msg_ctx.response.msg[0], status_msg, 1024);
	http_bldr->msg_ctx.response.msg[1023] = '\0';
	http_bldr->msg_ctx.response.code = status_code;

	uint32_t buf_len;
	if (approx_buf_size == 0)
		buf_len = DEFAULT_MSG_BUF_LEN;
	else
		buf_len = approx_buf_size;
	http_bldr->buf = malloc(buf_len * sizeof(uint8_t));
	if (http_bldr->buf == NULL)
		goto error_return;
	http_bldr->total_len = buf_len;

	char tmp_msg[1200];
	int32_t len = snprintf(&tmp_msg[0], 1200, "HTTP/%s %d %s\r\n",
			&http_bldr->msg_ctx.response.version[0],
			http_bldr->msg_ctx.response.code,
			&http_bldr->msg_ctx.response.msg[0]);
	if (addToBuffer(http_bldr, (uint8_t*)&tmp_msg[0], len) < 0)
		goto error_return;
	return http_bldr;

error_return:
	delete(http_bldr);
	return NULL;
}


http_msg_builder_t* createHTTP_Request(char const* version,
		char const* method, char const* path,
		uint32_t approx_buf_size) {

	if ((version == NULL) || (method == NULL) || (path == NULL))
		return NULL;
	http_msg_builder_t* http_bldr = calloc(1, sizeof(http_msg_builder_t));
	if (http_bldr == NULL)
		return NULL;
	init(http_bldr);

	strncpy(&http_bldr->msg_ctx.request.version[0], version, 5);
	http_bldr->msg_ctx.request.version[4] = '\0';
	strncpy(&http_bldr->msg_ctx.request.path[0], path, 1024);
	http_bldr->msg_ctx.request.path[1023] = '\0';
	strncpy(&http_bldr->msg_ctx.request.method[0], method, 20);
	http_bldr->msg_ctx.request.method[19] = '\0';

	uint32_t buf_len;
	if (approx_buf_size == 0)
		buf_len = DEFAULT_MSG_BUF_LEN;
	else
		buf_len = approx_buf_size;
	http_bldr->buf = malloc(buf_len * sizeof(uint8_t));
	if (http_bldr->buf == NULL)
		goto error_return;
	http_bldr->total_len = buf_len;

	char tmp_msg[1200];
	int32_t len = snprintf(&tmp_msg[0], 1200, "%s %s HTTP/%s\r\n",
			&http_bldr->msg_ctx.request.method[0],
			&http_bldr->msg_ctx.request.path[0],
			&http_bldr->msg_ctx.request.version[0]);
	if (addToBuffer(http_bldr, (uint8_t*)&tmp_msg[0], len) < 0)
		goto error_return;
	return http_bldr;

error_return:
	delete(http_bldr);
	return NULL;
}


static void init(http_msg_builder_t* http_bldr) {

	http_bldr->buf = NULL;
	http_bldr->filled_len = 0;
	http_bldr->total_len = 0;
	http_bldr->added_content_flag = 0;

	http_bldr->add_hdr_hdlr = addHdr;
	http_bldr->add_content_hdlr = addContent;
	http_bldr->get_msg_hdlr = getMsg; 

	http_bldr->delete_hdlr = delete;
}


static void delete(http_msg_builder_t* http_bldr) {

	if (http_bldr->buf != NULL)
		free(http_bldr->buf);
	free(http_bldr);
}


static int32_t addHdr(http_msg_builder_t* http_bldr, char const* hdr,
		char const* value) {

	if (http_bldr->added_content_flag > 0)//Content written. No more write
		return -1;
	int32_t rc = 0;
	char tmp_buf[1024];
	int32_t len = snprintf(&tmp_buf[0], 1024, "%s: %s\r\n", hdr, value);
	if (len < 1024) {
		if (addToBuffer(http_bldr, (uint8_t*)&tmp_buf[0], len) < 0)
			rc = -2;
	} else
		rc = -3;
	return rc;
}


static int32_t addContent(http_msg_builder_t* http_bldr, uint8_t const* content,
		uint32_t content_len) {

	if (http_bldr->added_content_flag > 0)//Content written. No more write
		return -1;
	if (content_len == 0)
		return 0;
	if (addToBuffer(http_bldr, (uint8_t*)"\r\n", 2) < 0)
		return -2;
	if (addToBuffer(http_bldr, content, content_len) < 0)
		return -3;
	http_bldr->added_content_flag = content_len;
	return 0;
}


static void getMsg(http_msg_builder_t* http_bldr, uint8_t const** msg,
		uint32_t* msg_len) {

	if (http_bldr->added_content_flag == 0) {
		addToBuffer(http_bldr, (uint8_t*)"\r\n", 2);
		http_bldr->added_content_flag = 1;
	}
	*msg = http_bldr->buf;
	*msg_len = http_bldr->filled_len;
}


static int32_t addToBuffer(http_msg_builder_t* http_bldr, uint8_t const* data,
		uint32_t data_len) {

	uint32_t avl_len = http_bldr->total_len - http_bldr->filled_len;
	if (data_len > avl_len) {
		uint32_t new_len = http_bldr->total_len + data_len
			+ DEFAULT_MSG_BUF_INCR; 
		http_bldr->buf = realloc(http_bldr->buf, new_len);
		if (http_bldr->buf == NULL)
			return -1;
	}
	memcpy(http_bldr->buf + http_bldr->filled_len, data, data_len);
	http_bldr->filled_len += data_len;
	return 0;
}

