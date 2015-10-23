/**
 * @file   dns_udp_handler.c
 * @author Sunil Mukundan <smukundan@cmbu-build03>
 * @date   Mon Feb 24 15:59:15 2012
 * 
 * @brief  implements the network event handlers for DNS queries
 * received as UDP packets. Assumes that the entire query is contained
 * in one packet.
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
#include "dns_udp_handler.h"
#include "dns_server.h"
#include "dns_common_defs.h"
#include "dns_con.h"
#include "dns_proto_handler.h"
#include "cr_common_intf.h"
#include "common/nkn_free_list.h"

// externs
extern cr_dns_cfg_t g_cfg;
extern obj_list_ctx_t *dns_con_ctx_list;

// counters
extern AO_t glob_dns_max_con_limit_err;
extern AO_t glob_dns_tot_transactions;

// static definitions

/** 
 * EPOLLIN handler for DNS UDP messages
 * for every packet the following happens
 * read-->create parser context-->parse data-->create async lookup task
 *               |                   |
 *   exit <--- error<----------------
 * @param ctx [in] - entity context that describes the connection (fd etc)
 * @param priv [in] - private data specific to the connection
 * @param priv_delete [in] - delete handler for the connection
 * specific data
 * 
 * @return returns 1 on success and zero on error. if this function
 * returns an error then this connection described by the entity context
 * is removed from the event loop
 */
int8_t
cr_dns_udp_epollin_handler(entity_data_t *ctx,
	void *priv, 
	delete_caller_ctx_fptr priv_delete)
{
    struct sockaddr_in from_addr;
    int32_t rv = 0, err = 1, fd, buf_len = 1024;
    uint32_t from_addr_len = 0;
    uint8_t *buf = NULL;
    obj_proto_desc_t *ph = NULL;
    dns_con_ctx_t *con = NULL;
    nw_ctx_t* nw_ctx = NULL;
    proto_data_t *otw_req = NULL;
    proto_token_t *parsed_req = NULL;

    fd = ctx->fd;
    if (AO_load(&glob_dns_tot_transactions) == 99997) {
	int uu = 0;
    }
    err = dns_con_ctx_list->pop_free(dns_con_ctx_list, (void **)&con);
    if (err) {
	DBG_LOG(ERROR, MOD_NETWORK, "error finding a free connection "
		"context [fd = %d], [err = %d]", fd, err);
	AO_fetch_and_add1(&glob_dns_max_con_limit_err);
	goto error;
    }

    ref_count_mem_t* ref_con = createRefCountedContainer(con,
						 freeConnCtxHdlr);
    if (!ref_con) {
	DBG_LOG(ERROR, MOD_NETWORK, "error creating "
		"query/response buffers, err code %d", err);
	goto error;
    }

    ph = dns_con_get_proto_desc(con);
    nw_ctx = dns_con_get_nw_ctx(con);
    assert(NW_CTX_GET_STATE(nw_ctx) == DNS_NW_RECV);
    nw_ctx->fd = fd;
    nw_ctx->ptype = DNS_UDP;
    otw_req = dns_con_get_proto_data(con);
    buf = otw_req->otw_data[0].iov_base;
    buf_len = otw_req->alloc_size_per_vec; 

    from_addr_len = sizeof(struct sockaddr_in);
    rv = recvfrom(fd, buf, buf_len, 0,
	    (struct sockaddr_in*)&from_addr,
	    &from_addr_len);
    if (rv < 0) {
	err = -errno;
	DBG_LOG(ERROR, MOD_NETWORK, "error reading network data "
		"[fd = %d], [err = %d]", fd, err);
	goto error;
    }
    memcpy(&nw_ctx->r_addr, &from_addr, sizeof(struct sockaddr_in));
    otw_req->otw_data[0].iov_len = rv;

    err = ph->init(con);
    err = ph->proto_data_to_token_data(ref_con, otw_req, 
	    (void **)&parsed_req);
    if (err) {
	DBG_LOG(ERROR, MOD_NETWORK, "error parsing DNS query "
		"[fd = %d], [err = %d]", fd, err);
	goto error;
    }

clean:
    return err;

error:
    return err;
}

int8_t
cr_dns_udp_epollerr_handler(entity_data_t *ctx,
	void *priv,
	delete_caller_ctx_fptr priv_delete)
{
    return 1;
}

int8_t
cr_dns_udp_epollhup_handler(entity_data_t *ctx,
	void *priv,
	delete_caller_ctx_fptr priv_delete)

{
    return 1;
}

#if 0
int32_t
dns_domain_store_lookup_task_create()
{


}
#endif



