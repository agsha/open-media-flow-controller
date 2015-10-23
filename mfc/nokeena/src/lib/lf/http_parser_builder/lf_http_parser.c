#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lf_http_parser.h"


static void init(parsed_http_msg_t* msg);
static void delete(parsed_http_msg_t* msg);
static int32_t parseRequestLine(char const* msg, http_request_t* request);
static int32_t parseResponseLine(char const* msg, http_response_t* response);

parsed_http_msg_t* parseHTTP_Message(uint8_t const* msg) {

	parsed_http_msg_t* parsed_out = calloc(1, sizeof(parsed_http_msg_t));
	if (parsed_out == NULL)
		return NULL;
	init(parsed_out);
	if (strncmp((char*)msg, "HTTP", 4) == 0) {
		parsed_out->type = HTTP_RESPONSE;
		if (parseResponseLine((char*)msg, &parsed_out->msg_ctx.response) < 0)
			goto error_return;
	} else {
		parsed_out->type = HTTP_REQUEST;
		if (parseRequestLine((char*)msg,
					&parsed_out->msg_ctx.request) < 0)
			goto error_return;
	}
	char* first_line_end = strstr((char*)msg, "\r\n");
	if (first_line_end == NULL) {
		goto error_return;
	}
	first_line_end += 2;
	char* start_pos = first_line_end;
	uint32_t hdr_count = 0;
	while (strncmp(start_pos, "\r\n", 2) != 0) {

		char* tmp = strstr(start_pos, "\r\n");
		if (tmp == NULL)
			goto error_return;
		start_pos = tmp + 2;
		hdr_count++;
	}
	if (hdr_count > 0) {

		parsed_out->header_count = hdr_count;
		parsed_out->headers = calloc(hdr_count, sizeof(char*));
		if (parsed_out->headers == NULL)
			goto error_return;
		parsed_out->values = calloc(hdr_count, sizeof(char*));
		if (parsed_out->values == NULL)
			goto error_return;

		start_pos = first_line_end;
		uint32_t i = 0;
		while (hdr_count > 0) {

			char* tmp1 = strchr(start_pos, ':');
			if (tmp1 == NULL)
				goto error_return;
			uint32_t name_len = tmp1 - start_pos;
			parsed_out->headers[i] = calloc(name_len + 1, sizeof(char));
			if (parsed_out->headers[i] == NULL)
				goto error_return;
			strncpy(parsed_out->headers[i], start_pos, name_len);
			parsed_out->headers[i][name_len] = '\0';

			tmp1 += 1;
			char* tmp2 = strstr(tmp1, "\r\n");
			if (tmp2 == NULL)
				goto error_return;
			uint32_t value_len = tmp2 - tmp1;

			parsed_out->values[i] = calloc(value_len + 1, sizeof(char));
			if (parsed_out->values[i] == NULL)
				goto error_return;
			strncpy(parsed_out->values[i], tmp1, value_len);
			parsed_out->values[i][value_len] = '\0';

			if (strncasecmp(parsed_out->headers[i], "Content-Length", 14) == 0)
					parsed_out->content_len = atol(parsed_out->values[i]);

			start_pos = tmp2 + 2;
			hdr_count--;
			i++;
		}
		if (parsed_out->content_len > 0)
			parsed_out->content = (uint8_t*)(start_pos + 2);
	}
	parsed_out->delete_hdlr = delete;
	return parsed_out;

error_return:
	delete(parsed_out);
	return NULL;
}


static void init(parsed_http_msg_t* msg) {

	msg->header_count = 0;
	msg->headers = NULL;
	msg->values = NULL;
	msg->content = NULL;
	msg->content_len = 0;
}


static void delete(parsed_http_msg_t* msg) {

	uint32_t i = 0;
	for (; i < msg->header_count; i++) {

		if (msg->headers[i] != NULL)
			free(msg->headers[i]);
		if (msg->values[i] != NULL)
			free(msg->values[i]);
	}
	free(msg->headers);
	free(msg->values);
	free(msg);
}


static int32_t parseRequestLine(char const* msg, http_request_t* request) {

	char* tmp0 = strstr(msg, "\r\n");
	if (tmp0 == NULL)
		return -1;;
	uint32_t fl_len = tmp0 - msg;
	char fl[fl_len + 1];
	strncpy(&fl[0], msg, fl_len);
	fl[fl_len] = '\0';

	char* tmp1 = strchr(&fl[0], ' ');
	if (tmp1 == NULL)
		return -2;
	uint32_t len = tmp1 - &fl[0];
	if (len > 19)
		return -3;
	strncpy(&request->method[0], &fl[0], len);
	request->method[len] = '\0';

	char* tmp2 = strrchr(&fl[0], ' ');
	if (tmp2 == NULL)
		return -4;
	if (strncasecmp(tmp2 + 1, "HTTP/", 5) == 0) {
		strncpy(&request->version[0], tmp2 + 6, 4);//"HTTP/"
		request->version[4] = '\0';
	} else
		return -5;

	len = tmp2 - tmp1 - 1;
	if (len > 1023)
		return -6;
	strncpy(&request->path[0], tmp1 + 1, len);
	request->path[len] = '\0';

	return 0;

}


static int32_t parseResponseLine(char const* msg, http_response_t* response) {

	char* tmp0 = strchr(msg, '/');
	if (tmp0 == NULL)
		return -1;
	tmp0 += 1;
	char* tmp1 = strchr(tmp0, ' ');
	if (tmp1 == NULL)
		return -2;
	uint32_t len = tmp1 - tmp0;
	strncpy(&response->version[0], tmp0, len);
	response->version[len] = '\0';
	tmp1 += 1;

	char* tmp2 = strchr(tmp1, ' ');
	if (tmp2 == NULL)
		return -3;
	len = tmp2 - tmp1;
	char code[5];
    strncpy(&code[0], tmp1, len);
	code[len] = '\0';
	response->code = atoi(&code[0]);
	tmp2 += 1;

	char* tmp3 = strstr(tmp2, "\r\n");
	if (tmp3 == NULL)
		return -4;
	len = tmp3 - tmp2;
	if (len > 1023)
		return -5;
    strncpy(&response->msg[0], tmp2, len);
	response->msg[len] = '\0';

	return 0;
}


void printParsedHttpMessage(parsed_http_msg_t const* msg) {

	if (msg->type == HTTP_REQUEST) {
		printf("Message type: REQUEST\n");
		printf("%s %s HTTP/%s\n", &msg->msg_ctx.request.method[0],
				&msg->msg_ctx.request.path[0],
				&msg->msg_ctx.request.version[0]);
	} else {
		printf("Message type: RESPONSE\n");
		printf("%u %s HTTP/%s\n", msg->msg_ctx.response.code,
				&msg->msg_ctx.response.msg[0],
				&msg->msg_ctx.response.version[0]);
	}
	uint32_t i = 0;
	for (; i < msg->header_count; i++)
		printf("%s:%s\n", msg->headers[i], msg->values[i]);
	printf("\n");
	if (msg->content_len > 0)
		for (i = 0; i < msg->content_len; i++)
			printf("%c", msg->content[i]);
	printf("\n");
}

