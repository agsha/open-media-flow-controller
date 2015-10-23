#ifndef CRST_REQUEST_PROC_H
#define CRST_REQUEST_PROC_H


#include <stdint.h>

#include "crst_prot_parser.h"

int32_t crstMsgHandler(crst_prot_msg_t* msg, void* con_ctx); 

#endif
