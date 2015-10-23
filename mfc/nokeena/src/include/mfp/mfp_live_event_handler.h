#ifndef MFP_LIVE_EVENT_HANDLER_H
#define MFP_LIVE_EVENT_HANDLER_H

#include <sys/socket.h>

#include "mfp_live_global.h"
#include "disp/dispatcher_manager.h"
#include "mfp_publ_context.h"
#include "nkn_debug.h"


//#define STREAM_TIMEOUT 30 // In seconds

int8_t mfpLiveEpollinHandler(entity_context_t* ctx);

int8_t mfpLiveEpolloutHandler(entity_context_t* ctx);

int8_t mfpLiveEpollerrHandler(entity_context_t* ctx);

int8_t mfpLiveEpollhupHandler(entity_context_t* ctx);

int8_t mfpLiveTimeoutHandler(entity_context_t* ctx);

#ifdef STREAM_CONT_CHECK
int32_t checkTS_StreamContinuity(int8_t const *data,
	int32_t len, stream_param_t *stream_parm, int8_t const * sess_name);
#endif


#endif
