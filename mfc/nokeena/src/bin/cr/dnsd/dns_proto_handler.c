/**
 * @file   dns_prot_handler.c
 * @author Sunil Mukundan <smukundan@cmbu-build03>
 * @date   Tue Feb 28 03:27:50 2012
 * 
 * @brief  implements the protocol object interface defined in the
 * obj_proto_data_t object. see cr_common_intf.h for definitions and
 * the following link for the design
 * http://cmbu-sv01.englab.juniper.net/mediawiki/index.php/Core_Platform_Overview
 * 
 * 
 */
#include "dns_proto_handler.h"
#include "dns_con.h"
#include "dns_request_proc.h"

static int32_t dns_proto_parse_packet(void *ctx, proto_data_t *buf,  
				      void **token_data);
static int32_t dns_proto_handler_shutdown(void *ctx);
static int32_t dns_proto_handler_init(void *ctx);
static int32_t cr_dns_msg_hdlr(struct parsed_dns_msg* parsed_data, void* ctx); 

// externs
extern cr_dns_cfg_t g_cfg;

int32_t
dns_proto_handler_create(void *const state_data, obj_proto_desc_t **out)
{
    obj_proto_desc_t *obj = NULL;
    int32_t err = 0;
    dns_pr_t *prot = NULL;
    
    assert(out);

    *out = NULL;
    obj = (obj_proto_desc_t *)
	nkn_calloc_type(1, sizeof(obj_proto_desc_t), 100);
    if (!obj) {
	err = -ENOMEM;
	goto error;
    }
    pthread_mutex_init(&obj->lock, NULL);

    /* create the DNS parser context */
    prot = createDNS_Handler(g_cfg.dns_qr_buf_size);
    if (!prot) {
	err = -ENOMEM;
	goto error;
    }
    dns_proto_handler_setup(obj, (void *)prot);

    *out = obj;
    return err;
 error:
    if (obj) free(obj);
    return err;
	  
}

int32_t 
dns_proto_handler_setup(obj_proto_desc_t *obj, void *state_data)
{
 
    obj->init = dns_proto_handler_init;
    obj->shutdown = dns_proto_handler_shutdown;
    obj->proto_data_to_token_data = dns_proto_parse_packet;
    obj->token_data_to_proto_data = NULL; //not implemented
    obj->state_data = state_data;

    return 0;
}

static int32_t
dns_proto_handler_init(void *ctx)
{
    return 0;
}

static int32_t
dns_proto_handler_shutdown(void *ctx)
{
    return 0;
}

/** 
 * proto_data2token_data implementation; uses the underlying protocol
 * parser defined by dns_pr_t to parse the data defined in
 * proto_data_t.
 * Before calling the protocol parser, we need to set the transport
 * type to TCP/UDP to account for the DNS message preamble. The parser
 * transfer control to dnsRequestProcessor after a DNS query is parsed
 * completely. In case of
 * a. an incomplete packet, control is transferred to the network
 * handler to come back with more data
 * b. an invalid packet is handled with appropriate action
 * 
 * @param ctx [in] - connection context
 * @param buf [in] - query buffer
 * @param token_data [out] - tokenized output of the parser
 * 
 * @return returns 0 on success, 1 if the parser requires more data
 * and a non-zero negative number on error
 */
static int32_t
dns_proto_parse_packet(void *ctx, proto_data_t *buf, 
		       void **token_data)
{
    int32_t err = 0;
    dns_pr_t *prot = NULL;
    uint32_t transport_type = 1;
   
    assert(buf);
    assert(token_data);
    *token_data = NULL;

    ref_count_mem_t *ref_con = (ref_count_mem_t *)ctx;
    dns_con_ctx_t* con = (dns_con_ctx_t*)ref_con->mem;
    nw_ctx_t* nw_ctx = dns_con_get_nw_ctx(con);
    obj_proto_desc_t *pd = dns_con_get_proto_desc(con);
    
    /* parse the DNS query
     * 1. set the transport type
     * 2. call parser
     * 3. handle error if any
     */
    prot = (dns_pr_t *)pd->state_data;
    transport_type = (nw_ctx->ptype == DNS_TCP);
    prot->set_transport_hdlr(prot, transport_type);
    prot->set_parse_complete_hdlr(prot, parseCompleteHandler, ctx);
    err = prot->parse_hdlr(prot, buf->otw_data[0].iov_base, 
			   buf->otw_data[0].iov_len);
    if (err) {
	DBG_LOG(ERROR, MOD_NETWORK, "error parsing DNS query "
		"[fd = %d], [err = %d]", nw_ctx->fd, err);
	goto error;
    }
    
 error:
    return err;
}

