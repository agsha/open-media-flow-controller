#ifndef _CR_DNS_NET_HANDLER_
#define _CR_DNS_NET_HANDLER_

#include <stdio.h>
#include <sys/types.h>

#include "entity_data.h"

#ifdef __cpluplus
extern "C" {
#endif

#if 0 //keep emacs happy
}
#endif

int8_t cr_dns_udp_epollin_handler(entity_data_t *ed, void *priv, 
			  delete_caller_ctx_fptr priv_destructor);
int8_t cr_dns_udp_epollerr_handler(entity_data_t *ctx, void *priv, 
			   delete_caller_ctx_fptr priv_destructor);
int8_t cr_dns_udp_epollhup_handler(entity_data_t *ctx, void *priv, 
			   delete_caller_ctx_fptr priv_destructor);

#ifdef __cpluplus
}
#endif

#endif //_CR_DNS_NET_HANDLER_
