#ifndef MFP_LIVE_ACCUM_CREATE_H
#define MFP_LIVE_ACCUM_CREATE_H

#include <stdint.h>
#include <stdlib.h>
#define MFP_LIVE_ACCUMV2
#ifdef MFP_LIVE_ACCUMV2
 #include "mfp_live_accum_ts_v2.h"
#else
 #include "mfp_live_accum_ts.h"
#endif

#include "mfp_accum_intf.h"
#include "mfp_live_global.h"
#include "nkn_vpe_mfu_defs.h"
#include "mfp_safe_io.h"
#include "mfp_sl_fmtr_conf.h"
#include "mfp_ref_count_mem.h"
#include "mfp_publ_context.h"

accum_ts_t* newAccumulatorTS(sess_stream_id_t*, SOURCE_TYPE stype);
void deleteAccumulator(void* accum_ts);
int32_t accum_validate_msfmtr_taskcnt(
	sess_stream_id_t* id,
	int8_t *name,
	uint32_t key_frame_interval,
	int32_t task_counter);

int32_t slow_strm_HA(sess_stream_id_t *id, mfp_publ_t *pub_ctx);


#endif

