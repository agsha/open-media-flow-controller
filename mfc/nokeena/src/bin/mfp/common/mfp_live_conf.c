#include "mfp_live_conf.h"


void* mfp_live_malloc(uint32_t size) {

#ifdef MFP_LIVE_MALLOC_FN
	return mfp_live_malloc_custom(size);
#else
	return malloc(size);
#endif
}


void* mfp_live_calloc(uint32_t num, uint32_t size) {

#ifdef MFP_LIVE_CALLOC_FN
	return mfp_live_calloc_custom(num, size);
#else
	return calloc(num, size);
#endif
}

