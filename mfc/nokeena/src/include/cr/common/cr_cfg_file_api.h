#ifndef _CR_READ_CONFIG_
#define _CR_READ_CONFIG_

#include <stdio.h>
#include <stdlib.h>

#define CR_UNKNOWN_TYPE    0
#define CR_INT_TYPE        1
#define CR_FLOAT_TYPE      2
#define CR_STR_VAL_TYPE    3
#define CR_STR_PTR_TYPE    4
#define CR_STR_CONST_TYPE  5
#define CR_DOUBLE_TYPE     6
#define CR_LONG_TYPE       7
#define CR_STR_LIST_TYPE   8
#define CR_FUNC_TYPE       9
#define CR_CB_FUNC_TYPE    10

#ifdef __cplusplus 
extern "C" {
#endif

#if 0
}
#endif

typedef struct tag_cr_cfg_param {
    const char *name;
    char type;
} cr_cfg_param_t;
    
typedef struct tag_cr_cfg_param_val{
    cr_cfg_param_t defn;
    void *p_val;
} cr_cfg_param_val_t;
    
int32_t cr_read_cfg_file(const char *cfg_file,
			 cr_cfg_param_val_t *cfg_param);
	
#ifdef __cplusplus
}
#endif

#endif //_CR_READ_CONFIG_
