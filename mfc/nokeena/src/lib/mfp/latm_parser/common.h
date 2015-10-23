#ifndef _LATM_COMMON_H_

#define _LATM_COMMON_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>


typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef signed int int32_t;
typedef unsigned short int uint16_t;
typedef unsigned long int uint64_t;
#if !defined(UNIT_TEST)
#include "nkn_debug.h"
#include "nkn_assert.h"
#include "nkn_memalloc.h"
#endif

#include "loas_parser.h"
#include "latm_parser.h"

#if !defined(CHECK)
#include "ts_parser.h"
#define VPE_ERROR (-1)
#define VPE_SUCCESS (0)
#define TS_PKT_SIZE (188)
#endif

#define LOAS_SYNC_WORD   0x2b7       ///< 11 bits LOAS sync word



#define MAJOR_VERSION (0)
#define MINOR_VERSION (1)

#define MAX_LATMCHUNKS (1000)
#ifdef __cplusplus
}
#endif

#endif

