#ifndef _NKN_VPE_M3U8__
#define _NKN_VPE_M3U8__


#include <stdlib.h>
#include <alloca.h>
#include <string.h>
#include <strings.h>
//#include "nkn_vpe_types.h"
#include "nkn_vpe_ism_read_api.h"

#define MAX_PATH 256
//#define MAX_PROFILES 10

typedef struct _tag_m3u8_data{
    uint32_t n_chunks;
    uint32_t smooth_flow_duration;
    int bitrate;

}m3u8_data;


int32_t write_m3u8(m3u8_data **, int32_t , char *, char *, char *);

#endif
