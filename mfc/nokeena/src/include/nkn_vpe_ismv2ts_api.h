#ifndef _ISMV2TS_API_
#define _ISMV2TS_API_

#include <inttypes.h>
#include <stdlib.h>

#include "mfp/mfp_file_converter_intf.h"
#include "nkn_vpe_ismv_mfu.h"
#include "nkn_vpe_ismv_reader.h"
#include "nkn_vpe_types.h"

#ifdef _cplusplus
extern "C" {
#endif

    typedef struct tag_ismv2ts_converter {
	ismv2ts_builder_t *bldr;
	ismv_parser_ctx_t *parser;
	ismv_reader_t *reader;
	int32_t sess_id;
    } ismv2ts_converter_ctx_t;
    
    int32_t init_ismv2ts_converter(file_conv_ip_params_t *fconv_params, 
				   void **conv_ctx);
    int32_t ismv2ts_process_frags(void *private_data, void **out);


#ifdef _cplusplus
}
#endif

#endif //_ISMV2TS_API_
