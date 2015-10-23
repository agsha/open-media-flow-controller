#ifndef MFP_LIVE_CONF_H
#define MFP_LIVE_CONF_H

#include <stdint.h>
#include <stdlib.h>


extern void* mfp_live_malloc_custom(uint32_t size);

extern void* mfp_live_calloc_custom(uint32_t num, uint32_t size);

void* mfp_live_malloc(uint32_t size);

void* mfp_live_calloc(uint32_t num, uint32_t size);

#endif
