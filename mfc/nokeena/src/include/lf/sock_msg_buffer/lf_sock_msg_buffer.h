#ifndef LFD_SOCK_MSG_BUFFER_H
#define LFD_SOCK_MSG_BUFFER_H

#include <stdint.h> 
#include <pthread.h> 

struct msg_buffer;
typedef int32_t (*msg_handler_fptr)(uint8_t* buf, uint32_t len,void const* ctx);
typedef void (*registered_msg_ctx_delete_fptr)(void* ctx);

struct msg_buffer* createMsgBuffer(uint32_t read_buf_max_len,
		uint32_t write_buf_max_len, msg_handler_fptr registered_msg_hdlr,
		void* registered_msg_hdlr_ctx,
		registered_msg_ctx_delete_fptr registered_msg_ctx_delete_hdlr);

typedef int32_t (*add_to_read_buf_fptr)(struct msg_buffer*, uint8_t const* buf,
		uint32_t len);

typedef int32_t (*add_to_write_buf_fptr)(struct msg_buffer*, uint8_t const* buf,
		uint32_t len);

typedef int32_t (*get_write_buf_fptr)(struct msg_buffer*, uint8_t* buf,
		uint32_t len);

typedef void (*delete_msg_buffer_fptr)(struct msg_buffer*);

typedef struct msg_buffer {

	uint8_t* read_buf;
	uint32_t read_buf_max_len;
	uint32_t read_buf_filled_len;

	uint8_t* write_buf;
	uint32_t write_buf_max_len;
	uint32_t write_buf_filled_len;

	struct timeval last_activity;
	pthread_mutex_t read_buf_lock;
	pthread_mutex_t write_buf_lock;

	msg_handler_fptr registered_msg_hdlr;
	void* registered_msg_ctx;
	registered_msg_ctx_delete_fptr registered_msg_ctx_delete_hdlr;

	add_to_read_buf_fptr add_read_msg_hdlr;
	add_to_write_buf_fptr add_write_msg_hdlr;
	get_write_buf_fptr get_write_msg_hdlr;
	delete_msg_buffer_fptr delete_hdlr;

} msg_buffer_t;

#endif
