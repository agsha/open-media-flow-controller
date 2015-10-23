#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "lf_sock_msg_buffer.h"

static int32_t addReadMsg(msg_buffer_t* msg_buf, uint8_t const* buf,
		uint32_t len);
static int32_t addWriteMsg(msg_buffer_t* msg_buf, uint8_t const* buf,
		uint32_t len);
static int32_t getWriteMsg(msg_buffer_t* msg_buf, uint8_t* buf, uint32_t len);
static void delete(msg_buffer_t* msg_buf);
static int32_t checkReadMsg(msg_buffer_t* msg_buf);

static char * strnstr(const char *s, const char *find, size_t slen); 
static char * strncasestr(const char *s, const char *find, size_t slen); 

msg_buffer_t* createMsgBuffer(uint32_t read_buf_max_len,
		uint32_t write_buf_max_len, msg_handler_fptr registered_msg_hdlr,
		void* registered_msg_ctx,
		registered_msg_ctx_delete_fptr registered_msg_ctx_delete_hdlr) {

	if (read_buf_max_len == 0)
		return NULL;
	msg_buffer_t* msg_buf = calloc(1, sizeof(msg_buffer_t));
	if (msg_buf == NULL)
		return NULL;
	msg_buf->read_buf = malloc(read_buf_max_len * sizeof(uint8_t));
	if (msg_buf->read_buf == NULL) {
		free(msg_buf);
		return NULL;
	}
	msg_buf->write_buf = malloc(write_buf_max_len * sizeof(uint8_t));
	if (msg_buf->write_buf == NULL) {
		free(msg_buf->read_buf);
		free(msg_buf);
		return NULL;
	}
	msg_buf->read_buf_max_len = read_buf_max_len;
	msg_buf->read_buf_filled_len = 0;
	msg_buf->write_buf_max_len = write_buf_max_len;
	msg_buf->write_buf_filled_len = 0;
	msg_buf->last_activity.tv_sec = 0;
	msg_buf->last_activity.tv_usec = 0;
	pthread_mutex_init(&msg_buf->read_buf_lock, NULL);
	pthread_mutex_init(&msg_buf->write_buf_lock, NULL);

	msg_buf->registered_msg_hdlr = registered_msg_hdlr;
	msg_buf->registered_msg_ctx = registered_msg_ctx;
	msg_buf->registered_msg_ctx_delete_hdlr = registered_msg_ctx_delete_hdlr;

	msg_buf->add_read_msg_hdlr = addReadMsg;
	msg_buf->add_write_msg_hdlr = addWriteMsg;
	msg_buf->get_write_msg_hdlr = getWriteMsg;
	msg_buf->delete_hdlr = delete;

	return msg_buf;
}


static int32_t addReadMsg(msg_buffer_t* msg_buf, uint8_t const* buf,
		uint32_t len) {

	pthread_mutex_lock(&msg_buf->read_buf_lock);
	uint32_t avl_len = msg_buf->read_buf_max_len - msg_buf->read_buf_filled_len;
	if (avl_len == 0) {
		printf("Client socke buffer full\n");
		pthread_mutex_unlock(&msg_buf->read_buf_lock);
		return -1;
	}
	if (avl_len < len)
		len = avl_len;
	memcpy(msg_buf->read_buf + msg_buf->read_buf_filled_len, buf, len);
	msg_buf->read_buf_filled_len += len;
	if (checkReadMsg(msg_buf) < 0) {
		pthread_mutex_unlock(&msg_buf->read_buf_lock);
		return -1;
	}
	pthread_mutex_unlock(&msg_buf->read_buf_lock);
	return len;
}


static int32_t addWriteMsg(msg_buffer_t* msg_buf, uint8_t const* buf,
		uint32_t len) {

	pthread_mutex_lock(&msg_buf->write_buf_lock);
	uint32_t avl_len = msg_buf->write_buf_max_len -
		msg_buf->write_buf_filled_len;
	if (avl_len < len)
		len = avl_len;
	memcpy(msg_buf->write_buf + msg_buf->write_buf_filled_len, buf, len);
	msg_buf->write_buf_filled_len += len;
	pthread_mutex_unlock(&msg_buf->write_buf_lock);
	return len;
}


static int32_t getWriteMsg(msg_buffer_t* msg_buf, uint8_t* buf, uint32_t len) {

	pthread_mutex_lock(&msg_buf->write_buf_lock);
	if (len > msg_buf->write_buf_filled_len)
		len = msg_buf->write_buf_filled_len;
	memcpy(buf, msg_buf->write_buf, len);
	memcpy(msg_buf->write_buf,
			msg_buf->write_buf + len, msg_buf->write_buf_filled_len - len);
	msg_buf->write_buf_filled_len -= len;
	pthread_mutex_unlock(&msg_buf->write_buf_lock);
	return len;

}


static void delete(msg_buffer_t* msg_buf) {

	if (msg_buf->read_buf != NULL)
		free(msg_buf->read_buf);
	if (msg_buf->write_buf != NULL)
		free(msg_buf->write_buf);
	if (msg_buf->registered_msg_ctx != NULL)
		if (msg_buf->registered_msg_ctx_delete_hdlr != NULL)
			msg_buf->registered_msg_ctx_delete_hdlr(
					msg_buf->registered_msg_ctx);
	pthread_mutex_destroy(&msg_buf->read_buf_lock);
	pthread_mutex_destroy(&msg_buf->write_buf_lock);
	free(msg_buf);
}


static int32_t checkReadMsg(msg_buffer_t* msg_buf) {

	uint32_t msg_len = 0;
	char* tmp_ptr1 = strnstr((char*)msg_buf->read_buf, "\r\n\r\n",
			msg_buf->read_buf_filled_len);
	if (tmp_ptr1 != NULL) {

		tmp_ptr1 += 4;
		uint32_t len = (uint8_t*)tmp_ptr1 - msg_buf->read_buf;
		char* tmp_ptr2 = strncasestr((char*)msg_buf->read_buf,
				(char*)"Content-Length:", len);
		if (tmp_ptr2 != NULL) {

			char* tmp_ptr3 = strnstr(tmp_ptr2, "\r\n", tmp_ptr1 - tmp_ptr2);
			if (tmp_ptr3 != NULL) {

				uint32_t clen_str_len = tmp_ptr3 - tmp_ptr2 
					- strlen("Content-Length:");

				char clen_str[clen_str_len + 1];
				strncpy(&clen_str[0], tmp_ptr2 + strlen("Content-Length:"),
						clen_str_len);
				clen_str[clen_str_len] = '\0';
				uint32_t clen = atoi(&clen_str[0]);
				if (msg_buf->read_buf_filled_len >= clen + len)
					msg_len = clen + len;
			}
		} else {
			msg_len = len;
		}
		if (msg_len > 0) {
			uint8_t* buf = malloc(msg_len * sizeof(uint8_t));
			if (buf == NULL)
				return -1;
			memcpy(buf, msg_buf->read_buf, msg_len);
			if (msg_buf->registered_msg_hdlr(buf, msg_len,
					msg_buf->registered_msg_ctx) < 0) {

				printf("Invalid HTTP input.\n");
				return -1;
			}
			msg_buf->read_buf_filled_len -= msg_len;
			memcpy(msg_buf->read_buf, msg_buf->read_buf + msg_len,
					msg_buf->read_buf_filled_len);
		}
	}
	return 0;
}


// BSD implementation
static char * strnstr(const char *s, const char *find, size_t slen) {

	char c, sc;
	size_t len;

	if ((c = *find++) != '\0') {
		len = strlen(find);
		do {
			do {
				if (slen-- < 1 || (sc = *s++) == '\0')
					return (NULL);
			} while (sc != c);
			if (len > slen)
				return (NULL);
		} while (strncmp(s, find, len) != 0);
		s--;
	}
	return ((char *)s);
}


static char* strncasestr(const char *s, const char *find, size_t slen) {

	char c, sc;
	size_t len;

	if ((c = *find++) != '\0') {
		len = strlen(find);
		do {
			do {
				if (slen-- < 1 || (sc = *s++) == '\0')
					return (NULL);
			} while (tolower(sc) != tolower(c));
			if (len > slen)
				return (NULL);
		} while (strncasecmp(s, find, len) != 0);
		s--;
	}
	return ((char *)s);
}
