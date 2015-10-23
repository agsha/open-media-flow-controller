#ifndef DNS_REQUEST_PROC_H
#define DNS_REQUEST_PROC_H

#include <stdint.h>
#include "dns_parser.h"

int32_t parseCompleteHandler(parsed_dns_msg_t* pr_ctx, void* ctx);

#endif
