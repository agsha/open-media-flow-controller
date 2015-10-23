#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "crst_nw_handler.h"
#include "crst_con_ctx.h"
#include "crst_request_proc.h"
#include "nkn_ref_count_mem.h"
#include "event_dispatcher.h"

#include "nkn_debug.h"

#define STORE_CON_MAX_IDLE_TIME 60

/* COUNTERS */
uint64_t glob_crst_dbg_sock_accept_err = 0;
uint64_t glob_crst_dbg_nw_oom_err = 0;
uint64_t glob_crst_dbg_nw_recv_err = 0;
uint64_t glob_crst_dbg_nw_send_err = 0;
uint64_t glob_crst_dbg_nw_epollerr = 0;
uint64_t glob_crst_dbg_nw_epollhup = 0;
uint64_t glob_crst_dbg_nw_timeo = 0;

/* STATIC */
static void freeConnCtxHdlr(void* ctx); 
static uint32_t diffTimevalToMs(struct timeval const* from,
	struct timeval const* val);
static void releaseRefConnCtxHdlr(void* ctx); 
static void msgHandlerCtxDelete(void* ctx); 


int8_t crst_listen_handler(entity_data_t* ctx, void* caller_id,
	delete_caller_ctx_fptr caller_id_delete_hdlr) {

    int32_t fd = ctx->fd;
    crst_con_ctx_t* cl_con_ctx = NULL;
    event_disp_t* disp = (event_disp_t*)caller_id;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(struct sockaddr_un));
    socklen_t addrlen = sizeof(struct sockaddr_un);
    int32_t client_fd = accept(fd, &addr, &addrlen);
    if (client_fd < 0) {
	glob_crst_dbg_sock_accept_err++;
	DBG_LOG(ERROR, MOD_CRST, "Error accepting connection "
		"[err=%d]", -errno);
	perror("Accept failed: ");
	return -errno;
    }
    msg_hdlr_ctx_t* msg_ctx = malloc(sizeof(msg_hdlr_ctx_t));
    if (msg_ctx == NULL) {
	glob_crst_dbg_nw_oom_err++;
	DBG_LOG(ERROR, MOD_CRST, "Error creating message context "
		"[err=%d]", -ENOMEM);
	close(client_fd);
	return -ENOMEM;
    }
    cl_con_ctx = createStoreConnectionContext(crstMsgHandler, msg_ctx,
	    msgHandlerCtxDelete);
    if (ctx == NULL) {
	glob_crst_dbg_nw_oom_err++;
	DBG_LOG(ERROR, MOD_CRST, "Error creating ref context "
		"[err=%d]", -ENOMEM);
	close(client_fd);
	return -ENOMEM;
    }
    ref_count_mem_t* ref_con = createRefCountedContainer(cl_con_ctx,
	    freeConnCtxHdlr);
    ref_con->hold_ref_cont(ref_con);

    entity_data_t* entity_ctx = newEntityData(client_fd, ref_con, 
	    releaseRefConnCtxHdlr, crst_epollin_handler, 
	    crst_epollout_handler, crst_epollerr_handler, 
	    crst_epollhup_handler, NULL);
    if (entity_ctx == NULL) {
	glob_crst_dbg_nw_oom_err++;
	DBG_LOG(ERROR, MOD_CRST, "Error creating entity context "
		"[err=%d]", -ENOMEM);
	close(client_fd);
	return -ENOMEM;
    }
    msg_ctx->disp = disp;
    msg_ctx->entity_ctx = entity_ctx;
    entity_ctx->event_flags |= EPOLLIN;
    gettimeofday(&(entity_ctx->to_be_scheduled), NULL);
    entity_ctx->to_be_scheduled.tv_sec += STORE_CON_MAX_IDLE_TIME;
    disp->reg_entity_hdlr(disp, entity_ctx);
    return 0;
}


int8_t crst_epollin_handler(entity_data_t* ctx, void* caller_id,
	delete_caller_ctx_fptr caller_id_delete_hdlr) {

    int32_t fd = ctx->fd;
    int32_t client_fd = -1;
    crst_con_ctx_t* cl_con_ctx = NULL;
    event_disp_t* disp = (event_disp_t*)caller_id;
    ref_count_mem_t* ref_conn = (ref_count_mem_t*)ctx->context;
    crst_con_ctx_t* con = (crst_con_ctx_t*)ref_conn->mem;


    uint8_t tmp_buf[1500];
    int32_t rc = recv(fd, &tmp_buf[0], 1500, 0);
    if (rc <= 0) {

	if (rc < 0) {
	    glob_crst_dbg_nw_recv_err++;
	    DBG_LOG(ERROR, MOD_CRST, "Error reading from socket "
		"[err=%d]", -errno);
	    rc = -errno;
	} else if (rc == 0) {
	    DBG_LOG(MSG, MOD_CRST, "Reset from client");
	}
	goto error_return;
    }
    if (con->pr->parse_hdlr(con->pr, &tmp_buf[0], rc) < 0) {
	DBG_LOG(ERROR, MOD_CRST, "Error parsing IPC request");
	goto error_return;
    }
    return 0;

error_return:
    disp->unreg_entity_hdlr(disp, ctx);
    close(fd);
    return -1;
}


int8_t crst_epollout_handler(entity_data_t* ctx, void* caller_id,
	delete_caller_ctx_fptr caller_id_delete_hdlr) {

    int32_t fd = ctx->fd;
    crst_con_ctx_t* cl_con_ctx = NULL;
    event_disp_t* disp = (event_disp_t*)caller_id;
    ref_count_mem_t* ref_conn = (ref_count_mem_t*)ctx->context;
    crst_con_ctx_t* con = (crst_con_ctx_t*)ref_conn->mem;

    uint8_t tmp_buf[1500];
    int32_t bytes_avl = con->bldr->get_prot_data_hdlr(con->bldr, &tmp_buf[0],
	    1500);
    if (bytes_avl <= 0) {

	disp->unset_write_hdlr(disp, ctx);
    } else {
	int32_t rc = send(fd, &tmp_buf[0], bytes_avl, 0);
	if (rc <= 0) {
	    glob_crst_dbg_nw_send_err++;
	    DBG_LOG(ERROR, MOD_CRST, "Error writing in socket "
		    "[err=%d]", -errno);
	    goto error_return;
	}
    }
    return 0;

error_return:
    disp->unreg_entity_hdlr(disp, ctx);
    close(fd);
    return -1;
}


int8_t crst_epollerr_handler(entity_data_t* ctx, void* caller_id,
	delete_caller_ctx_fptr caller_id_delete_hdlr) {

    event_disp_t* disp = (event_disp_t*)caller_id;
    int32_t fd = ctx->fd;

    glob_crst_dbg_nw_epollerr++;
    DBG_LOG(ERROR, MOD_CRST, "EPOLLERR on socket "
	    "[err=%d]", -errno);

    disp->unreg_entity_hdlr(disp, ctx);
    close(fd);
    return 0;
}


int8_t crst_epollhup_handler(entity_data_t* ctx, void* caller_id,
	delete_caller_ctx_fptr caller_id_delete_hdlr) {

    event_disp_t* disp = (event_disp_t*)caller_id;
    int32_t fd = ctx->fd;

    glob_crst_dbg_nw_epollhup++;
    DBG_LOG(ERROR, MOD_CRST, "EPOLLHUP on socket "
	    "[err=%d]", -errno);
    disp->unreg_entity_hdlr(disp, ctx);
    close(fd);
    return 0;
}


int8_t crst_timeout_handler(entity_data_t* ctx, void* caller_id,
	delete_caller_ctx_fptr caller_id_delete_hdlr) {

    int32_t fd = ctx->fd;
    ref_count_mem_t* ref_conn = (ref_count_mem_t*)ctx->context;
    crst_con_ctx_t* con = (crst_con_ctx_t*)ref_conn->mem;
    event_disp_t* disp = (event_disp_t*)caller_id;
    struct timeval now;
    gettimeofday(&now, NULL);
    uint32_t idle_time_ms = diffTimevalToMs(&now, &(con->last_activity));
    if (idle_time_ms < (STORE_CON_MAX_IDLE_TIME * 1000)) {

	gettimeofday(&(ctx->to_be_scheduled), NULL);
	ctx->to_be_scheduled.tv_sec += STORE_CON_MAX_IDLE_TIME
	    - (idle_time_ms/1000);
	disp->sched_timeout_hdlr(disp, ctx);
    } else {
	if (ref_conn->get_ref_count(ref_conn) > 1) {
	    gettimeofday(&(ctx->to_be_scheduled), NULL);
	    ctx->to_be_scheduled.tv_sec += STORE_CON_MAX_IDLE_TIME
		- (idle_time_ms/1000);
	    disp->sched_timeout_hdlr(disp, ctx);
	} else {
	    glob_crst_dbg_nw_timeo++;
	    DBG_LOG(MSG, MOD_CRST, "Connection Timeout [fd=%d]", fd);
	    goto error_return;
	}
    }
    return 1;

error_return:
    disp->unreg_entity_hdlr(disp, ctx);
    close(fd);
    return -1;
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


static void freeConnCtxHdlr(void* ctx) {

    crst_con_ctx_t* con = (crst_con_ctx_t*)ctx;
    con->delete_hdlr(con);
}


static void releaseRefConnCtxHdlr(void* ctx) {

    ref_count_mem_t* ref_con = (ref_count_mem_t*)ctx;
    ref_con->release_ref_cont(ref_con);
}


static void msgHandlerCtxDelete(void* ctx) {

    msg_hdlr_ctx_t* hdlr_ctx = (msg_hdlr_ctx_t*)ctx;
    free(hdlr_ctx);
}

