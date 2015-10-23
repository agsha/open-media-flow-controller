#ifndef _MFP_READ_CONFIG_
#define _MFP_READ_CONFIG_

#include <stdio.h>
#include <stdlib.h>

#define MFP_UNKNOWN_TYPE    0
#define MFP_INT_TYPE        1
#define MFP_FLOAT_TYPE      2
#define MFP_STR_VAL_TYPE    3
#define MFP_STR_PTR_TYPE    4
#define MFP_STR_CONST_TYPE  5
#define MFP_DOUBLE_TYPE     6
#define MFP_LONG_TYPE       7
#define MFP_STR_LIST_TYPE   8
#define MFP_FUNC_TYPE       9
#define MFP_CB_FUNC_TYPE    10

#ifdef __cplusplus 
extern "C" {
#endif

    typedef struct tag_mfp_cfg_param {
	const char *name;
	char type;
    } mfp_cfg_param_t;
    
    typedef struct tag_mfp_cfg_param_val{
	mfp_cfg_param_t defn;
	void *p_val;
    } mfp_cfg_param_val_t;
    
    int32_t mfp_read_cfg_file(const char *cfg_file,
			      mfp_cfg_param_val_t *cfg_param);
	
#ifdef __cplusplus
}
#endif

#endif //_MFP_READ_CONFIG_
