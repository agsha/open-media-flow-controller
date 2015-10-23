#ifndef _MFP_FILE_CONVERT_
#define _MFP_FILE_CONVERT_

#include <inttypes.h>

#include "mfp_publ_context.h"
#include "mfp_file_converter_intf.h"
#include "mfp_live_sess_id.h"

#ifdef __cplusplus
extern "C" {
#endif


    typedef struct tag_mfp_fconv_tpool_obj {
	file_conv_intf_t *intf;
	int32_t sess_id;
	void *input;
	void *output;
    } mfp_fconv_tpool_obj_t;
    
    /** 
     * converts a publisher context to the respective publish schemes
     * @param pub - the publisher context
     * @return returns 0 on success and negative integer on error
     */
    int32_t mfp_convert_pmf(int32_t sess_id);

#ifdef __cplusplus
}
#endif

#endif //_MFP_FILE_CONVERT_
