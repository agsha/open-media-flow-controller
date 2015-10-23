#ifndef LF_NW_EVENT_HANDLER_H
#define LF_NW_EVENT_HANDLER_H

#include "entity_context.h"

typedef struct fd_context {

	int32_t fd;
	entity_context_t* entity_ctx;
} fd_context_t;


int8_t lfdListenHandler(entity_context_t* ctx); 


int8_t lfEpollinHandler(entity_context_t* ctx); 


int8_t lfEpolloutHandler(entity_context_t* ctx); 


int8_t lfEpollerrHandler(entity_context_t* ctx); 


int8_t lfEpollhupHandler(entity_context_t* ctx); 


int8_t lfTimeoutHandler(entity_context_t* ctx); 


#endif
