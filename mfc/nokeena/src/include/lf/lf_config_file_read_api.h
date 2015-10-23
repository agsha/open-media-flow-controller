#ifndef _LF_READ_CONFIG_
#define _LF_READ_CONFIG_

#include <stdio.h>
#include <stdlib.h>

#define LF_UNKNOWN_TYPE    0
#define LF_INT_TYPE        1
#define LF_FLOAT_TYPE      2
#define LF_STR_VAL_TYPE    3
#define LF_STR_PTR_TYPE    4
#define LF_STR_CONST_TYPE  5
#define LF_DOUBLE_TYPE     6
#define LF_LONG_TYPE       7
#define LF_STR_LIST_TYPE   8
#define LF_FUNC_TYPE       9
#define LF_CB_FUNC_TYPE    10

#ifdef __cplusplus 
extern "C" {
#endif

#if 0
}
#endif

typedef struct tag_lf_cfg_param {
    const char *name;
    char type;
} lf_cfg_param_t;
    
typedef struct tag_lf_cfg_param_val{
    lf_cfg_param_t defn;
    void *p_val;
} lf_cfg_param_val_t;
    
int32_t lf_read_cfg_file(const char *cfg_file,
			 lf_cfg_param_val_t *cfg_param);
	
#ifdef __cplusplus
}
#endif

#endif //_LF_READ_CONFIG_
