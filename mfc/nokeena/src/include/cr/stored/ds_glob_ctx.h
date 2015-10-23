#ifndef _DS_GLOB_CTX_
#define _DS_GLOB_CTX_

#include <stdio.h>
#include <inttypes.h>
#include "ds_hash_table_mgmt.h"

#ifdef __cplusplus
extern "C" {
#endif


    typedef struct ds_glob_ctx{
	//	hash_tbl_props_t* dns_CG;
	hash_tbl_props_t*  domainname_domainptr;
	hash_tbl_props_t*  CGname_CGptr;
	hash_tbl_props_t* CEname_CEptr;
	hash_tbl_props_t*  POPname_POPptr;
    }ds_glob_ctx_t;


    int32_t init_dstore(void);
#ifdef __cplusplus
}
#endif

#endif //_DS_GLOB_CTX_
