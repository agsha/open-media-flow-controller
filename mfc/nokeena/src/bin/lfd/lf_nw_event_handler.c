#include <sys/socket.h>
#include <stdint.h>
#include <errno.h>

#include "lf_nw_event_handler.h"
#include "lf_msg_handler.h"
#include "entity_context.h"
#include "dispatcher_manager.h"
#include "lf_uint_ctx_cont.h"
#include "lf_sock_msg_buffer.h"

#include "lf_dbg.h"

#define SOCK_MAX_IDLE_TIME 60

extern dispatcher_mngr_t* disp_mngr;
extern uint_ctx_cont_t* uint_ctx_cont;

static void sockMsgBufferDelete(void* ctx); 

static void lfMsgHandlerContextDelete(void* ctx);

static uint32_t diffTimevalToMs(struct timeval const* from,
		struct timeval const * val);

static int32_t nWriteBytes(int32_t fd, uint8_t const* buf, uint32_t len);

int8_t lfdListenHandler(entity_context_t* ctx) {

	entity_context_t* entity_ctx = NULL;
	struct sockaddr_in addr;
	uint32_t addr_len = sizeof(struct sockaddr_in);
	int32_t new_fd = accept(ctx->fd, &addr, &addr_len); 
	if (new_fd < 0) {

		perror("accept failed: ");
		LF_LOG(LOG_ERR, LFD_NW_HANDLER, "Accept failed.Error number: %d\n",
				errno);
		return 0;
	}
	entity_ctx = newEntityContext(new_fd,
			NULL, NULL,lfEpollinHandler, lfEpolloutHandler,
			lfEpollerrHandler, lfEpollhupHandler,
			lfTimeoutHandler, disp_mngr);
	if (entity_ctx == NULL)
		goto error_return;

	entity_ctx->event_flags |= EPOLLIN;
	gettimeofday(&(entity_ctx->to_be_scheduled), NULL);
	entity_ctx->to_be_scheduled.tv_sec += SOCK_MAX_IDLE_TIME; 

	fd_context_t* fd_ctx = malloc(sizeof(fd_context_t));
	if (fd_ctx == NULL)
		goto error_return;
	fd_ctx->fd = new_fd;
	fd_ctx->entity_ctx = entity_ctx;
	msg_buffer_t* msg_buf = createMsgBuffer(1024*64, 256*1024, lfMsgHandler,
			(void*)fd_ctx, lfMsgHandlerContextDelete); 
	if (msg_buf == NULL)
		goto error_return;
	if (uint_ctx_cont->add_element_hdlr(uint_ctx_cont, new_fd, msg_buf,
				sockMsgBufferDelete) < 0)
		goto error_return;

	gettimeofday(&msg_buf->last_activity, NULL);
	disp_mngr->register_entity(entity_ctx);
	return 1;

error_return:
	if (new_fd >=0)
		close(new_fd);
	if (entity_ctx != NULL)
		entity_ctx->delete_entity(entity_ctx);
	disp_mngr->self_unregister(ctx);
	close(ctx->fd);
	return -1;

}


int8_t lfEpollinHandler(entity_context_t* ctx) {

	msg_buffer_t* buf_ctx = uint_ctx_cont->acquire_element_hdlr(
			uint_ctx_cont, ctx->fd);
	if (buf_ctx == NULL) 
		goto error_return;
	buf_ctx->last_activity.tv_sec = ctx->event_time.tv_sec;
	buf_ctx->last_activity.tv_usec = ctx->event_time.tv_usec;
	uint8_t tmp_buf[1500];
	int32_t rc = read(ctx->fd, &tmp_buf[0], 1500);
	if (rc < 0) {
		perror("recv failed: ");
		LF_LOG(LOG_ERR, LFD_NW_HANDLER, "Receive failed.Error number: %d\n",
				errno);
		goto error_return;
	}
	if (rc == 0) {
		LF_LOG(LOG_NOTICE, LFD_NW_HANDLER, "Client connection closed.\n");
		goto error_return;
	}
	rc = buf_ctx->add_read_msg_hdlr(buf_ctx, &tmp_buf[0], rc);
	if (rc < 0)
		goto error_return;
	uint_ctx_cont->release_element_hdlr(uint_ctx_cont, ctx->fd);
	return 1;

error_return:
	uint_ctx_cont->release_element_hdlr(uint_ctx_cont, ctx->fd);
	if (buf_ctx != NULL)
		uint_ctx_cont->remove_element_hdlr(uint_ctx_cont, ctx->fd);
	int32_t tmp_fd = ctx->fd;
	disp_mngr->self_unregister(ctx);
	close(tmp_fd);
	return -1;
}


int8_t lfEpolloutHandler(entity_context_t* ctx) {

	msg_buffer_t* buf_ctx = uint_ctx_cont->acquire_element_hdlr(
			uint_ctx_cont, ctx->fd);
	if (buf_ctx == NULL) 
		goto error_return;
	buf_ctx->last_activity.tv_sec = ctx->event_time.tv_sec;
	buf_ctx->last_activity.tv_usec = ctx->event_time.tv_usec;
	uint8_t tmp_buf[1500];
	int32_t rc = buf_ctx->get_write_msg_hdlr(buf_ctx, &tmp_buf[0], 1500); 
	if (rc < 0)
		goto error_return;
	else if (rc > 0) {
		rc = nWriteBytes(ctx->fd, &tmp_buf[0], rc);
		if (rc < 0) {
			goto error_return;
		}
	} else {
		goto close_return;
	}

	uint_ctx_cont->release_element_hdlr(uint_ctx_cont, ctx->fd);
	return 1;

error_return:
	LF_LOG(LOG_ERR, LFD_NW_HANDLER, "Epollout Error\n");
close_return:
	uint_ctx_cont->release_element_hdlr(uint_ctx_cont, ctx->fd);
	if (buf_ctx != NULL)
		uint_ctx_cont->remove_element_hdlr(uint_ctx_cont, ctx->fd);
	int32_t tmp_fd = ctx->fd;
	disp_mngr->self_unregister(ctx);
	close(tmp_fd);
	return -1;
}


int8_t lfEpollerrHandler(entity_context_t* ctx) {

	LF_LOG(LOG_ERR, LFD_NW_HANDLER, "Epoll Error\n");
	uint_ctx_cont->remove_element_hdlr(uint_ctx_cont, ctx->fd);
	int32_t tmp_fd = ctx->fd;
	disp_mngr->self_unregister(ctx);
	close(tmp_fd);
	return 1;
}


int8_t lfEpollhupHandler(entity_context_t* ctx) {

	LF_LOG(LOG_ERR, LFD_NW_HANDLER, "Epoll Error\n");
	uint_ctx_cont->remove_element_hdlr(uint_ctx_cont, ctx->fd);
	int32_t tmp_fd = ctx->fd;
	disp_mngr->self_unregister(ctx);
	close(tmp_fd);
	return 1;
}


int8_t lfTimeoutHandler(entity_context_t* ctx) {

	msg_buffer_t* buf_ctx = uint_ctx_cont->acquire_element_hdlr(
			uint_ctx_cont, ctx->fd);
	if (buf_ctx == NULL) 
		goto error_return;
	struct timeval now;
	gettimeofday(&now, NULL);
	gettimeofday(&now, NULL);
	uint32_t idle_time_ms = diffTimevalToMs(&now, &(buf_ctx->last_activity));
	if (idle_time_ms < (SOCK_MAX_IDLE_TIME * 1000)) {

		gettimeofday(&(ctx->to_be_scheduled), NULL);
		ctx->to_be_scheduled.tv_sec += SOCK_MAX_IDLE_TIME
			- (idle_time_ms/1000);
		disp_mngr->self_schedule_timeout(ctx);
	} else {
		LF_LOG(LOG_NOTICE, LFD_NW_HANDLER, "Connection timed out\n");
		goto error_return;
	}

	uint_ctx_cont->release_element_hdlr(uint_ctx_cont, ctx->fd);
	return 1;

error_return:
	uint_ctx_cont->release_element_hdlr(uint_ctx_cont, ctx->fd);
	if (buf_ctx != NULL)
		uint_ctx_cont->remove_element_hdlr(uint_ctx_cont, ctx->fd);
	int32_t tmp_fd = ctx->fd;
	disp_mngr->self_unregister(ctx);
	close(tmp_fd);
	return -1;
}


static void sockMsgBufferDelete(void* ctx) {

	msg_buffer_t* msg_buf = (msg_buffer_t*)ctx;
	msg_buf->delete_hdlr(msg_buf);
}

static void lfMsgHandlerContextDelete(void* ctx) {

	fd_context_t* fd_ctx = (fd_context_t*)ctx;
	free(fd_ctx);
}

static uint32_t diffTimevalToMs(struct timeval const* from,
		struct timeval const * val) {
	//if -ve, return 0
	double d_from = from->tv_sec + ((double)from->tv_usec)/1000000;
	double d_val = val->tv_sec + ((double)val->tv_usec)/1000000;
	double diff = d_from - d_val;
	if (diff < 0)
		return 0;
	return (uint32_t)(diff * 1000);
}


static int32_t nWriteBytes(int32_t fd, uint8_t const* buf, uint32_t len) {

	uint32_t offset = 0;
	int32_t rv = 0;
	while (len > 0) {

		int32_t rc = write(fd, buf + offset, len);
		if (rc < 0) {
			LF_LOG(LOG_ERR, LFD_NW_HANDLER, "write failed %d \n", errno);
			if ((errno != EINTR) && (errno != EAGAIN)) {
				perror("Write failed: ");
				rv = -1;
				break;
			}
		} else {
			if ((uint32_t)rc < len) {
				LF_LOG(LOG_ERR, LFD_NW_HANDLER, "TCP Half sent: %u %u %d\n",
						len, offset, rc);
			}
			offset += rc;
			len -= rc;
		}
	}
	return rv;
}

