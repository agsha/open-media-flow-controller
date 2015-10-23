/**
 * @file   dns_tcp_handler.h
 * @author Sunil Mukundan <smukundan@cmbu-build03>
 * @date   Fri Mar  2 01:32:45 2012
 * 
 * @brief  
 * 
 * 
 */
#ifndef _CR_DNS_TCP_NET_HANDLER_
#define _CR_DNS_TCP_NET_HANDLER_

#include <stdio.h>
#include <sys/types.h>

#include "entity_data.h"

#ifdef __cpluplus
extern "C" {
#endif

#if 0 //keep emacs happy
}
#endif

int8_t cr_dns_tcp_listen_handler(entity_data_t* ctx, void* caller_id,
		        delete_caller_ctx_fptr caller_id_delete_hdlr); 

int8_t cr_dns_tcp_epollin_handler(entity_data_t* ctx, void* caller_id,
		        delete_caller_ctx_fptr caller_id_delete_hdlr);

int8_t cr_dns_tcp_epollerr_handler(entity_data_t* ctx, void* caller_id,
		        delete_caller_ctx_fptr caller_id_delete_hdlr);

int8_t cr_dns_tcp_epollhup_handler(entity_data_t* ctx, void* caller_id,
		        delete_caller_ctx_fptr caller_id_delete_hdlr);

int8_t cr_dns_tcp_timeout_handler(entity_data_t* ctx, void* caller_id,
		        delete_caller_ctx_fptr caller_id_delete_hdlr);

int8_t cr_dns_tcp_epollout_handler(entity_data_t *ctx, void *caller_id, 
		    delete_caller_ctx_fptr caller_id_delete_hdlr);

void freeConnCtxHdlr(void* ctx);

#ifdef __cpluplus
}
#endif

#endif //_CR_DNS_TCP_NET_HANDLER_
