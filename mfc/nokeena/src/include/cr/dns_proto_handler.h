/**
 * @file   dns_proto_handler.h
 * @author Sunil Mukundan <smukundan@cmbu-build03>
 * @date   Tue Feb 28 04:37:19 2012
 * 
 * @brief  
 * 
 * 
 */
#ifndef _DNS_PROTO_HANDLER_
#define _DNS_PROTO_HANDLER_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>

#include "nkn_debug.h"
#include "nkn_memalloc.h"
#include "cr_common_intf.h"
#include "dns_pb/dns_parser.h"
#include "dns_common_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

#if 0
}
#endif

int32_t dns_proto_handler_create(void *const state_data, obj_proto_desc_t **out); 
int32_t dns_proto_handler_setup(obj_proto_desc_t *obj, 
			       void *state_data);

#ifdef __cplusplus
}
#endif

#endif //_DNS_PROTO_HANDLER_
