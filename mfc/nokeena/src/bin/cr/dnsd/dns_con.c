/*
 * @file   dns_con.c
 * @author Sunil Mukundan <smukundan@cmbu-build03>
 * @date   Thu Mar  1 21:46:53 2012
 * 
 * @brief  implements all functionality relevant to the dns_con_ctx_t
 * obj. the object is a container for all the module ojects that will
 * be used during the life of a connection.
 * Semantics:  
 * 1. only the reference to each member module is stored; hence if
 * this has to be thread safe the each member object SHOULD implement
 * a lock (pthread_mutex).
 * 2. member objects SHOULD only be accessed via GET, SET API's
 * provided.
 * 3. GET & SET API's will allow the caller to choose whether the
 * module object's refrence should be returned with or without a
 * lock. this is provided as an optimization to avoid multiple locks
 * and unlocks in the case a set of operations need to be done under
 * the dns_con_ctx_t objects lock
 *  
 */
#include "dns_con.h"
#include "dns_tcp_handler.h"

/* config defaults */
extern cr_dns_cfg_t g_cfg;

/* counters */
extern AO_t glob_dns_tot_transactions;
uint64_t glob_dns_tcp_recv_err = 0;
uint64_t glob_dns_parse_err = 0;

/* static */
static inline int32_t proto_data_get_curr_buf(
	      struct tag_proto_data *pd, 
	      const uint8_t **buf, uint32_t *rw_pos,
	      uint32_t *buf_len);
static inline void proto_data_mark_curr_buf_done(
			 struct tag_proto_data *pd);
static inline void proto_data_set_curr_buf_type(
			struct tag_proto_data *pd,
			proto_data_buf_type_t type);
static inline int32_t proto_data_get_buf(
		      struct tag_proto_data *pd,
		      uint32_t buf_id, const uint8_t **buf,
		      uint32_t *rw_pos, uint32_t *buf_len);
static inline proto_data_buf_type_t proto_data_get_buf_type(
				    struct tag_proto_data *pd,
				    uint32_t buf_id);
static inline int32_t proto_data_update_curr_buf_rw_pos(
				struct tag_proto_data *pd,
				uint32_t len);

void *
dns_con_create(void)
{
    dns_con_ctx_t *dns_con = NULL;
    void *out = NULL;
    int32_t err = 0;
    
    dns_con = nkn_calloc_type(1, sizeof(dns_con_ctx_t), 100);
    if (!dns_con) {
	DBG_LOG(ERROR, MOD_NETWORK, "out of memory when allocating "
		"connection context");
	goto error;
    }

    err = dns_nw_ctx_create(&dns_con->nw_ctx);
    if (err) {
	out = NULL;
	DBG_LOG(ERROR, MOD_NETWORK, "error creating "
		"network context, err code %d", err);
	goto error;
    }
    
    err = dns_proto_handler_create(dns_con, 
			   &dns_con->proto_obj);
    if (err) {
	out = NULL;
	DBG_LOG(ERROR, MOD_NETWORK, "error creating "
		"protocol object, err code %d", err);
	goto error;
    }
    
    err = dns_proto_data_create(g_cfg.dns_qr_buf_size, 
				1 + MAX_QUERY_COUNT, 
				&dns_con->qr_buf);
    if (err) {
	out = NULL;
	DBG_LOG(ERROR, MOD_NETWORK, "error creating "
		"query/response buffers, err code %d", err);
	goto error;
    }
    dns_con->last_activity.tv_sec = 0; 
    dns_con->last_activity.tv_usec = 0; 
	
    out = dns_con;
 error:
    return out;
}

int32_t
dns_nw_ctx_create(nw_ctx_t **out)
{
    nw_ctx_t *ctx = NULL;
    int32_t err = 0;
    
    assert(out);
    *out = NULL;

    ctx = (nw_ctx_t *)
	nkn_calloc_type(1, sizeof(nw_ctx_t), 100);
    if (!ctx) {
	err = -ENOMEM;
	goto error;
    }
    ctx->ptype= DNS_UDP;
    ctx->fd = -1;
    NW_CTX_SET_STATE(ctx, DNS_NW_RECV);
    pthread_mutex_init(&ctx->lock, NULL);

    *out = ctx;
    return err;

 error:
    return err;
}

int32_t
dns_proto_data_create(int32_t buf_size, int32_t num_bufs, 
		      proto_data_t **out)
{
    proto_data_t *pd = NULL;
    int32_t i = 0, err = 0;
    
    pd = (proto_data_t*)
	nkn_calloc_type(1, sizeof(proto_data_t), 100);
    if (!pd) {
	err = -ENOMEM;
	goto error;
    }

    pthread_mutex_init(&pd->lock, NULL);
    pd->alloc_size_per_vec = buf_size;
    pd->num_iovecs = num_bufs;
    
    pd->otw_data = (struct iovec *)
	nkn_calloc_type(sizeof(struct iovec), pd->num_iovecs,
			100);
    if (!pd->otw_data) {
	err = -ENOMEM;
	goto error;
    }
    pd->buf_type = (proto_data_buf_type_t *)
	nkn_calloc_type(sizeof(proto_data_buf_type_t), pd->num_iovecs,
			100);
    if (!pd->buf_type) {
	err = -ENOMEM;
	goto error;
    }
    for (i = 0; i < num_bufs; i++) {
	pd->otw_data[i].iov_base = 
	    nkn_malloc_type(buf_size, mod_http_respbuf);
	if (!pd->otw_data[i].iov_base) {
	    err = -ENOMEM;
	    goto error;
	}
	pd->otw_data[i].iov_len = 0;
    }
    pd->get_curr_buf = proto_data_get_curr_buf;
    pd->mark_curr_buf = proto_data_mark_curr_buf_done;
    pd->set_curr_buf_type = proto_data_set_curr_buf_type;
    pd->upd_curr_buf_rw_pos = proto_data_update_curr_buf_rw_pos;

    pd->get_buf = proto_data_get_buf;
    pd->get_buf_type = proto_data_get_buf_type;

    *out = pd;
    return err;

 error:
    if (pd) {
	for (i = 0; i < num_bufs; i++) {
	    if (pd->otw_data[i].iov_base)
		free(pd->otw_data[i].iov_base);
	}
	free(pd);
    }
    
    return err;
}

void
dns_con_reset_for_reuse(void *ref)
{
    nw_ctx_t *nw = NULL;
    proto_data_t *pd = NULL;
    dns_con_ctx_t *con = NULL;
    
    con = (dns_con_ctx_t*)ref;
    
    if (con) {
	/* reset nw context */
	nw = dns_con_get_nw_ctx(con);
	assert(nw->fd != -1);
	if (nw->ptype == DNS_TCP) {
	    DBG_LOG(MSG, MOD_NETWORK, "closing connection [fd=%d]", nw->fd);
	    close(nw->fd);
	}
	nw->fd = -1;
	NW_CTX_CLEAR_STATE(nw);
	NW_CTX_SET_STATE(nw, DNS_NW_RECV);

	{
	    uint32_t i = 0;
	    /* reset proto obj */
	    /* reset qr proto data */
	    pd = dns_con_get_proto_data(con);
	    pd->curr_vec_id = 0;
	    pd->exp_data_size = 0;
	    for(i = 0; i < pd->num_iovecs; i++) {
		pd->otw_data[i].iov_len = 0;
	    }
	}

	/* add back to connection context pool */
	//obj_list_return_free(dns_con_ctx_list
    }
    
    return;
}

int32_t
dns_con_tcp_data_recv_handler(dns_con_ctx_t *con)
{
    proto_data_t *otw_req = NULL;
    obj_proto_desc_t *ph = NULL;
    proto_token_t *parsed_req = NULL;
    uint8_t *buf = NULL;
    int32_t err = 0, rv = 0, buf_len = 0, dns_msg_size = 0;
    int32_t buf_id = 0;
    nw_ctx_t *nw = NULL;
    int fd = -1;

    /* acquire a safe NW context, since NW context state change should
     * happen only on one thread. the following scenarios are likely
     * a. we may have scehduled a lookup and this may be reset, we
     * need to mark an ERR so that the lookup will refrain from sending
     * date on a stale FD. This is especially important since this FD
     * can be re-assigned to another connection when the lookup is being
     * processed.
     */
    nw = dns_con_get_nw_ctx_safe(con);
    otw_req = dns_con_get_proto_data(con);
    fd = nw->fd;

    buf_id = otw_req->curr_vec_id;
    buf = otw_req->otw_data[buf_id].iov_base;
    buf += otw_req->otw_data[buf_id].iov_len;
    buf_len = otw_req->alloc_size_per_vec - 
	otw_req->otw_data[buf_id].iov_len;
    assert(buf_len > 0);
    
    /* check current NW state before reading  */
    if (!(NW_CTX_GET_STATE(nw) & DNS_NW_RECV)) {
	DBG_LOG(ERROR, MOD_NETWORK, "NW state machine error "
		"[fd=%d] reading on a FD with state %d",
		NW_CTX_GET_STATE(nw));
	goto error;
    }
    rv = recv(fd, buf, buf_len, MSG_DONTWAIT);
    if (rv < 0) {
	err = -errno;
	DBG_LOG(ERROR, MOD_NETWORK, "error receiving data on "
		"socket, [fd = %d], [err = %d]", fd, err);
	glob_dns_tcp_recv_err++;
	NW_CTX_SET_STATE(nw, DNS_NW_ERR);
	goto error;
    } else if (rv == 0) {
	err = -ECONNRESET;
	DBG_LOG(MSG, MOD_NETWORK, "reset from client "
		"[fd = %d], [err = %d]", fd, err);
	NW_CTX_SET_STATE(nw, DNS_NW_DONE);
	goto error;
    }
    if (rv == buf_len) { 
	otw_req->otw_data[buf_id].iov_len += rv;
	otw_req->curr_vec_id++;
	otw_req->set_curr_buf_type(otw_req, 
		   PROTO_DATA_BUF_TYPE_RECV);
	/* move on to next state; dont break away */
    } else {
	otw_req->otw_data[buf_id].iov_len += rv;
    }

 error:
    dns_con_rel_nw_ctx(con);
    return err;
}

int32_t 
dns_con_send_data(dns_con_ctx_t *con)
{
    int32_t i, err = 0, rc = 0;
    uint32_t len = 0, rw_pos = 0, to_send  = 0;
    const uint8_t *data;
    proto_data_t *pd = dns_con_get_proto_data(con);
    /* acquire a safe NW context, since NW context state change should
     * happen only on one thread. the following scenarios are likely
     * a. we may have scehduled a lookup and this may be reset, we
     * need to mark an ERR so that the lookup will refrain from sending
     * date on a stale FD. This is especially important since this FD
     * can be re-assigned to another connection when the lookup is being
     * processed.
     */
    nw_ctx_t* nw = dns_con_get_nw_ctx_safe(con);

    /* check current NW state before sending */
    if (((NW_CTX_GET_STATE(nw) & DNS_NW_ERR)) ||
	((NW_CTX_GET_STATE(nw) & DNS_NW_DONE))) {
	DBG_LOG(ERROR, MOD_NETWORK, "NW state machine error "
		"[fd=%d] writing on a FD with state %d",
		nw->fd, NW_CTX_GET_STATE(nw));
	err = -EBADF;
	goto error;
    }
    for (i = 0; i < (int32_t)pd->num_iovecs; i++) {
	if (pd->get_buf_type(pd, i) == PROTO_DATA_BUF_TYPE_SEND) {

	    err = pd->get_buf(pd, i, &data, &rw_pos, &len);
	    if (err) {
		goto error;
	    }
	    to_send = rw_pos;
	    if (nw->ptype == DNS_TCP) {
		rc = send(nw->fd, data, to_send, 0);
		if (rc < 0) {
		    err = -errno;
		    DBG_LOG(ERROR, MOD_NETWORK, "error sending data "
			    "[fd=%d], [err=%d]", nw->fd, err);
		    goto error;
		}
	    } else {
		rc = sendto(nw->fd, data, to_send, 0, &nw->r_addr,
			    sizeof(struct sockaddr));
		if (rc < 0) {
		    err = -errno;
		    DBG_LOG(ERROR, MOD_NETWORK, "error sending data "
			    "[fd=%d], [err=%d]", nw->fd, err);
		    goto error;
		}
	    }
	}
    }
    
    /* needs to be send if there are no outstanding queries to to be
     * processed since this will tell the cleanup to close the
     * connection
     */
    NW_CTX_SET_STATE(nw, DNS_NW_DONE);
    AO_fetch_and_add1(&glob_dns_tot_transactions);
    DBG_LOG(MSG, MOD_NETWORK, "sent data on the wire [fd=%d], "
	    "[bytes=%d]", nw->fd, rc);
    dns_con_rel_nw_ctx(con);
    return err;
 error:
    DBG_LOG(ERROR, MOD_NETWORK, "error sending data "
	    "[fd=%d], [type=%], [err=%d]", nw->fd, 
	    nw->ptype, errno);
    NW_CTX_SET_STATE(nw, DNS_NW_ERR);
    dns_con_rel_nw_ctx(con);
    return err;
}

inline obj_proto_desc_t *
dns_con_get_proto_desc_safe(dns_con_ctx_t *ctx)
{
    pthread_mutex_lock(&ctx->proto_obj->lock);
    return ctx->proto_obj;
}

inline void
dns_con_rel_proto_desc(dns_con_ctx_t *ctx)
{
    pthread_mutex_unlock(&ctx->proto_obj->lock);
}

inline proto_data_t *
dns_con_get_proto_data_safe(dns_con_ctx_t *ctx)
{
    pthread_mutex_lock(&ctx->qr_buf->lock);
    return ctx->qr_buf;
}

inline void
dns_con_rel_proto_data(dns_con_ctx_t *ctx)
{
    pthread_mutex_unlock(&ctx->qr_buf->lock);
}

inline obj_proto_desc_t *
dns_con_get_proto_desc(dns_con_ctx_t *ctx)
{
    return ctx->proto_obj;
}

inline proto_data_t *
dns_con_get_proto_data(dns_con_ctx_t *ctx)
{
    return ctx->qr_buf;
}

inline nw_ctx_t *
dns_con_get_nw_ctx_safe(dns_con_ctx_t *ctx)
{
    pthread_mutex_lock(&ctx->nw_ctx->lock);
    return ctx->nw_ctx;
}

inline nw_ctx_t *
dns_con_get_nw_ctx(dns_con_ctx_t *ctx)
{
    return ctx->nw_ctx;
}

inline void
dns_con_rel_nw_ctx(dns_con_ctx_t *ctx)
{
    pthread_mutex_unlock(&ctx->nw_ctx->lock);
}  

static inline int32_t
proto_data_get_curr_buf(struct tag_proto_data *pd, 
		 const uint8_t **buf, uint32_t *rw_pos,
		 uint32_t *buf_len)
{
    assert(pd);

    int32_t err = 0;
    uint32_t buf_id = pd->curr_vec_id;
    struct iovec *iov = &pd->otw_data[buf_id];

    if (buf_id < pd->num_iovecs) {
	*buf = (uint8_t *)iov->iov_base;
	*buf_len = pd->alloc_size_per_vec;
	*rw_pos = iov->iov_len;
    } else {
	*buf = NULL;
	*buf_len = 0;
	err = -ENOBUFS;
    }

    return err;
}

static inline void
proto_data_mark_curr_buf_done(struct tag_proto_data *pd)
{
    assert(pd);

    pd->curr_vec_id++;
}

static inline void
proto_data_set_curr_buf_type(struct tag_proto_data *pd,
			proto_data_buf_type_t type) 
{
    assert(pd);

    pd->buf_type[pd->curr_vec_id] = type;
}

static inline int32_t
proto_data_get_buf(struct tag_proto_data *pd, 
		   uint32_t buf_id, const uint8_t **buf, 
		   uint32_t *rw_pos, uint32_t *buf_len)
{
    assert(pd);

    int32_t err = 0;
    struct iovec *iov = &pd->otw_data[buf_id];

    if (buf_id < pd->num_iovecs) {
	*buf = (uint8_t *)iov->iov_base;
	*buf_len = pd->alloc_size_per_vec;
	*rw_pos = iov->iov_len;
    } else {
	*buf = NULL;
	*buf_len = 0;
	err = -ENOBUFS;
    }

    return err;
}

static inline int32_t
proto_data_update_curr_buf_rw_pos(struct tag_proto_data *pd,
				  uint32_t len)
{
    int32_t err = 0, rem_len = 0;
    uint32_t buf_id = pd->curr_vec_id;
    struct iovec *iov = &pd->otw_data[buf_id];
    
    rem_len = pd->alloc_size_per_vec - iov->iov_len;
    if (rem_len <= 0){ 
	err = -E_PROTO_DATA_BUF_OOB;
	goto error;
    }
    iov->iov_len += len;
    
 error:
    return err;
}
				 

static inline proto_data_buf_type_t
proto_data_get_buf_type(struct tag_proto_data *pd,
			uint32_t buf_id)
{
    assert(pd);

    return pd->buf_type[buf_id];
}
