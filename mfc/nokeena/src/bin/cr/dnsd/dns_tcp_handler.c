/**
 * @file   dns_tcp_handler.c
 * @author Sunil Mukundan <smukundan@cmbu-build03>
 * @date   Fri Mar  2 01:28:32 2012
 * 
 * @brief  implements the network event handlers for DNS queries
 * received as TCP packets. Supports multi part parsing of DNS
 * requests 
 * 
 * 
 */
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h> //inet_ntop()
#include <errno.h>

//nkn includes
#include "nkn_debug.h"
#include "dns_tcp_handler.h"
#include "dns_server.h"
#include "dns_common_defs.h"
#include "dns_con.h"
#include "dns_proto_handler.h"
#include "cr_common_intf.h"
#include "common/nkn_free_list.h"
#include "common/nkn_ref_count_mem.h"

#define DNS_TCP_MAX_IDLE_TIME 60

// externs
extern cr_dns_cfg_t g_cfg;
extern obj_list_ctx_t *dns_con_ctx_list;
extern dns_disp_mgr_t gl_disp_mgr;

// counters
extern AO_t glob_dns_max_con_limit_err;
extern uint64_t glob_dns_parse_err;

static uint32_t diffTimevalToMs(struct timeval const* from,
				struct timeval const * val); 
static int32_t cr_dns_tcp_cleanup(event_disp_t *disp, 
				  entity_data_t *ent_ctx); 
static void releaseRefConnCtxHdlr(void* ctx);

int8_t
cr_dns_tcp_listen_handler(entity_data_t* ent_ctx, void* caller_id, 
			  delete_caller_ctx_fptr caller_id_delete_hdlr) {

    int32_t err = 0;
    int32_t fd = ent_ctx->fd;
    dns_con_ctx_t* client_con = NULL;
    event_disp_t* disp = NULL;// = (event_disp_t*)caller_id;
    struct sockaddr_in addr;
    socklen_t addrlen;
    ref_count_mem_t *ref_con = NULL;
    uint32_t timeo = 0;

    int32_t client_fd = accept(fd, (struct sockaddr_in*)&addr, &addrlen);
    if (client_fd < 0)
	goto error_return;
    DBG_LOG(MSG, MOD_NETWORK, "accepted client socket [fd=%d] ",
	    client_fd);
    err = dns_con_ctx_list->pop_free(dns_con_ctx_list, 
				     (void **)&client_con);
    if (err) {
	DBG_LOG(ERROR, MOD_NETWORK, "error finding a free connection "
		"context [fd = %d], [err = %d]", client_fd, err);
	AO_fetch_and_add1(&glob_dns_max_con_limit_err);
	goto error_return;
    }

    HOLD_CFG_OBJ(&g_cfg);
    disp = gl_disp_mgr.disp[gl_disp_mgr.curr_idx];
    gl_disp_mgr.curr_idx++;
    if (gl_disp_mgr.curr_idx >= gl_disp_mgr.num_disp) {
	gl_disp_mgr.curr_idx = 0;
    }
    timeo = g_cfg.tcp_timeout;
    REL_CFG_OBJ(&g_cfg);
    nw_ctx_t* nw_client = dns_con_get_nw_ctx(client_con);
    assert(NW_CTX_GET_STATE(nw_client) == DNS_NW_RECV);
    nw_client->fd = client_fd;
    nw_client->ptype = DNS_TCP;
    nw_client->disp = disp;
    nw_client->timeo = timeo;
    dns_set_socket_non_blocking(client_fd);

    ref_con = createRefCountedContainer(client_con,
					freeConnCtxHdlr);
    if (!ref_con) {
	DBG_LOG(ERROR, MOD_NETWORK, "error creating "
		"query/response buffers, err code %d", err);
	goto error_return;
    }
    ref_con->hold_ref_cont(ref_con);
    entity_data_t* entity_ctx = newEntityData(client_fd, ref_con,
	      releaseRefConnCtxHdlr, cr_dns_tcp_epollin_handler, 
	      cr_dns_tcp_epollout_handler, cr_dns_tcp_epollerr_handler,
	      cr_dns_tcp_epollhup_handler, cr_dns_tcp_timeout_handler);
    if (entity_ctx == NULL) {
	err = -ENOMEM;
	DBG_LOG(SEVERE, MOD_NETWORK, "error adding socket to "
		"event dispatcher [fd = %d], [err = %d]",
		fd, err);
	close(client_fd);
	if (dns_con_ctx_list->push_free(dns_con_ctx_list, 
					(void**)&client_con) < 0)
	    assert(0);
	goto error_return;
    }
    entity_ctx->event_flags |= EPOLLIN;
    gettimeofday(&(entity_ctx->to_be_scheduled), NULL);
    entity_ctx->to_be_scheduled.tv_sec += nw_client->timeo;
    nw_client->ed = entity_ctx;
    disp->reg_entity_hdlr(disp, entity_ctx);

 error_return:
    return err;
}


int8_t
cr_dns_tcp_epollin_handler(entity_data_t* ent_ctx, void* caller_id,
			   delete_caller_ctx_fptr caller_id_delete_hdlr) {

    int32_t err = 0;
    int32_t fd = ent_ctx->fd;
    ref_count_mem_t* ref_conn = (ref_count_mem_t*)ent_ctx->context;
    dns_con_ctx_t* con = (dns_con_ctx_t*)ref_conn->mem;
    event_disp_t* disp = (event_disp_t*)caller_id;
    obj_proto_desc_t *ph = NULL;
    proto_data_t *otw_req = NULL;
    proto_token_t *parsed_req = NULL;

    err = dns_con_tcp_data_recv_handler(con);
    if (err < 0)
	goto error_return;
    otw_req = dns_con_get_proto_data(con);
    ph = dns_con_get_proto_desc(con);
    err = ph->init(ref_conn);
    err = ph->proto_data_to_token_data(ref_conn, otw_req, 
				       (void **)&parsed_req);
    if (err) {
	glob_dns_parse_err++;
	DBG_LOG(ERROR, MOD_NETWORK, "error parsing DNS query "
		"[fd = %d], [err = %d]", fd, err);
	goto error_return;
    }


    con->last_activity.tv_sec = ent_ctx->event_time.tv_sec;
    con->last_activity.tv_usec = ent_ctx->event_time.tv_usec;
    return 1;

 error_return:
    cr_dns_tcp_cleanup(disp, ent_ctx);
    return -1;
}

int8_t
cr_dns_tcp_epollerr_handler(entity_data_t *ent_ctx,
			    void *caller_id,
			    delete_caller_ctx_fptr caller_id_delete_hdlr)
{
    event_disp_t* disp = (event_disp_t*)caller_id;
    int32_t fd = ent_ctx->fd;
    cr_dns_tcp_cleanup(disp, ent_ctx);
    return 0;
}

int8_t
cr_dns_tcp_epollhup_handler(entity_data_t* ent_ctx,
			    void *caller_id,
			    delete_caller_ctx_fptr caller_id_delete_hdlr)
{
    event_disp_t* disp = (event_disp_t*)caller_id;
    cr_dns_tcp_cleanup(disp, ent_ctx);
    return 0;
}

int8_t
cr_dns_tcp_epollout_handler(entity_data_t *ent_ctx, void *caller_id, 
		    delete_caller_ctx_fptr caller_id_delete_hdlr)
{
    ref_count_mem_t* ref_conn = (ref_count_mem_t*)ent_ctx->context;
    dns_con_ctx_t* con = (dns_con_ctx_t*)ref_conn->mem;
    event_disp_t *disp = (event_disp_t*)caller_id;
    int8_t err = 0;
    
    err = dns_con_send_data(con);
    if (err) {
	if (err == -EAGAIN) {
	    err = 0;
	    goto done;
	}
	goto error;
    }
    disp->unset_write_hdlr(disp, ent_ctx);

 done:
    ref_conn->release_ref_cont(ref_conn);
    return err;
    
 error:
    ref_conn->release_ref_cont(ref_conn);
    cr_dns_tcp_cleanup(disp, ent_ctx);
    return err;
}

int8_t
cr_dns_tcp_timeout_handler(entity_data_t* ent_ctx, void* caller_id,
		   delete_caller_ctx_fptr caller_id_delete_hdlr) 
{

    int32_t fd = ent_ctx->fd;
    ref_count_mem_t* ref_conn = (ref_count_mem_t*)ent_ctx->context;
    dns_con_ctx_t* con = (dns_con_ctx_t*)ref_conn->mem;
    event_disp_t* disp = (event_disp_t*)caller_id;
    nw_ctx_t *nw = dns_con_get_nw_ctx(con);
    uint32_t timeo = nw->timeo;

    struct timeval now;
    gettimeofday(&now, NULL);
    uint32_t idle_time_ms = diffTimevalToMs(&now,
			    &(con->last_activity));

    if (idle_time_ms < (timeo * 1000)) {
	gettimeofday(&(ent_ctx->to_be_scheduled), NULL);
	ent_ctx->to_be_scheduled.tv_sec += timeo
	    - (idle_time_ms/1000);
	disp->sched_timeout_hdlr(disp, ent_ctx);
    } else {
	DBG_LOG(WARNING, MOD_NETWORK, "Connection timed out [fd=%d]", 
		nw->fd);
	NW_CTX_SET_STATE(nw, DNS_NW_DONE);
	goto error_return;
    }
    return 1;

 error_return:
    cr_dns_tcp_cleanup(disp, ent_ctx);
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

void freeConnCtxHdlr(void* ctx) {

    assert(ctx);

    dns_con_ctx_t* con = (dns_con_ctx_t*)ctx;
    assert(con);

    nw_ctx_t *nw = dns_con_get_nw_ctx(con);
    assert(NW_CTX_GET_STATE(nw) & DNS_NW_DONE);
    
    dns_con_reset_for_reuse(con);
    
    if (dns_con_ctx_list->push_free(dns_con_ctx_list, (void*)con) < 0)
	assert(0);
}


static void releaseRefConnCtxHdlr(void* ctx) {

    ref_count_mem_t* ref_con = (ref_count_mem_t*)ctx;
    ref_con->release_ref_cont(ref_con);
}

/** 
 * cleanup the TCP connection. the connection can reach here in two
 * states namely,
 * 1. when request processing has not yet commenced (epollhup, parsing
 * error, tcp errors)
 * 2. when request processing has commenced (unexpected reset from
 * client, epollerr etc)
 * 
 * in the case of #1 we need to unregister the fd from the epoll set
 * and close the FD to release system resources
 *
 * in the case of #2 we need to unregister the fd from the epoll fd
 * set but we SHOULD not close the fd since the processing is in
 * progress and the processing to send data on a fd that has been
 * re-assgined to another connection by the kernel
 *
 * We determine if request processing is in progress or not by
 * examining the ref count. The design, mandates that the connection
 * context is ref counted by each thread that uses it. the ref count
 * library calls an associated cleanup function when the ref count is
 * zero. this will ensure that the FD will be closed in the case #2
 * and resources will be returned to the system
 *
 * @param disp - event dispatcher context
 * @param ent_ctx - entity context
 * 
 * @return unused
 */
static int32_t
cr_dns_tcp_cleanup(event_disp_t *disp, entity_data_t *ent_ctx)
{
    ref_count_mem_t* ref_conn =	(ref_count_mem_t*)ent_ctx->context;
    int32_t fd = ent_ctx->fd;
    int32_t ref_cnt = (int32_t)ref_conn->get_ref_count(ref_conn);
    dns_con_ctx_t *con = ref_conn->mem;
    nw_ctx_t *nw = dns_con_get_nw_ctx(con);

    assert(ref_cnt >= 0);
    NW_CTX_SET_STATE(nw, DNS_NW_DONE);
    if (ref_cnt > 1) {
	struct timeval now;
	gettimeofday(&now, NULL);
	uint32_t idle_time_ms = diffTimevalToMs(&now,
				&(con->last_activity));
	gettimeofday(&(ent_ctx->to_be_scheduled), NULL);
	ent_ctx->to_be_scheduled.tv_sec += 
	    nw->timeo - (idle_time_ms/1000);
	disp->sched_timeout_hdlr(disp, ent_ctx);
    } else {
	/* unresigter calls the associated delete callback for this entity
	 * context. which automatically releases the reference count
	 */
	disp->unreg_entity_hdlr(disp, ent_ctx);
	DBG_LOG(MSG, MOD_NETWORK, "complete cleanup, unregistering "
		"all events and close [fd=%d]", fd);
    } 
    return 0;
}
