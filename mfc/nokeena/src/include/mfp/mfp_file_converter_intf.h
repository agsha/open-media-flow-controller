#ifndef _MFP_FILE_CONV_INTF_
#define _MFP_FILE_CONV_INTF_

#include <inttypes.h>
#include <stdlib.h>
#include "mfp_publ_context.h"
#include "mfp_live_accum_ts.h"
#ifdef __cplusplus 
extern "C" {
#endif

    typedef struct tag_file_conv_ip_params {
		char* sess_name;
	int32_t n_input_files;
	int32_t profile_bitrate[MAX_STREAM_PER_ENCAP];
	char *input_path[MAX_STREAM_PER_ENCAP];
	char *output_path;
	char *delivery_url;
	int32_t ss_package_type; //MBR or SBR
	int32_t key_frame_interval;
        uint32_t segment_start_idx;
        uint8_t min_segment_child_plist;
        uint8_t segment_idx_precision;
	int32_t sess_id;
	int32_t streaming_type;
	int32_t num_pmf;
	int32_t ms_param_status;
	int32_t ss_param_status;
	uint32_t streams_per_encap;
	register_formatter_error_t error_callback_fn;
    } file_conv_ip_params_t;
    
    typedef struct tag_file_conv_intf {
	int32_t (*prepare_input)(void *out, void *private_data);
	int32_t (*init_file_conv)(file_conv_ip_params_t *,\
				  void  **private_data);
	int32_t (*read_and_process)(void *private_data, void **out);
	int32_t (*write_out_data)(void *out);
	void (*cleanup_file_conv)(void *private_data);
    } file_conv_intf_t;
    void register_formatter_error_file(
				       ref_count_mem_t *ref_cont);
    
#ifdef __cplusplus
}
#endif

#endif //_MFP_FILE_CONV_INTF_
