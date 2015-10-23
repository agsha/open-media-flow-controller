//#define MS_DEV
#ifdef MS_DEV
#include "inttypeswin.h"
#else
#include "inttypes.h"
#endif

#define MPEG2_TS_PKT_SIZE 188
#define PICTURE_START_CODE 00

uint32_t handle_mpeg2(uint8_t*,uint32_t*);
