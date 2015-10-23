#ifndef MFP_SL_FMTR_CONF_H 
#define MFP_SL_FMTR_CONF_H 

#include <stdint.h>

int32_t SL_IOW_Handler(void* ctx, uint8_t const* file_path,
			uint8_t const* buf, uint32_t len);

int32_t SL_IOD_Handler(void* ctx, uint8_t const* file_path); 

#endif
