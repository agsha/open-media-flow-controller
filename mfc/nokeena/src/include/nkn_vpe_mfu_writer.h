#ifndef _NKN_VPE_MFU_WRITER__
#define _NKN_VPE_MFU_WRITER__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nkn_vpe_types.h"
#include "nkn_vpe_mfu_defs.h"

#define RAW_FG_FIXED_SIZE 16

#define MFU_FIXED_SIZE 16
#define FIXED_BOX_SIZE 8
#define _BIG_ENDIAN_
typedef struct mfu_desc_tt{

    uint32_t vdat_size;
    uint32_t adat_size;
    uint32_t sps_size;
    uint32_t pps_size;
    uint32_t mfub_box_size;
    uint32_t mfu_raw_box_size;
    mfub_t *mfub_box;
    mfu_rwfg_t  *mfu_raw_box;
    uint8_t *vdat;
    uint8_t *adat;
    uint32_t is_audio;
    uint32_t is_video;

}mfu_desc_t;
typedef mfu_desc_t moof2mfu_desc_t;

int32_t mfu_writer_mfub_box(uint32_t, mfub_t*, uint8_t*);
int32_t mfu_writer_vdat_box(uint32_t, uint8_t*, uint8_t*);
int32_t mfu_writer_adat_box(uint32_t, uint8_t*, uint8_t*);
int32_t mfu_writer_rwfg_box(uint32_t, mfu_rwfg_t*, uint8_t*,
			    mfu_desc_t*);


int32_t
mfu_writer_get_sample_duration(uint32_t *, uint32_t *, uint32_t );

#endif
