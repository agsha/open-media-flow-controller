/**
 * @file   de_intf.h
 * @author Sunil Mukundan <smukundan@cmbu-build03>
 * @date   Tue May  1 23:19:50 2012
 * 
 * @brief  
 * 
 * 
 */
#ifndef _DE_INTF_
#define _DE_INTF_

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

#include "lfc/nkn_lf_client.h"
#include "common/cr_utils.h"
#include "common/rrecord_msg_utils.h"
#include "stored/crst_common_defs.h"
//#include "stored/ds_tables.h"

#ifdef __cplusplus
extern "C" {
#endif

#if 0 //keeps emacs happy
}
#endif

#define MAX_RESULT_REC_CNT 3

typedef struct crst_ce_attr {

    lf_stats_t stats;
    conjugate_graticule_t loc_attr;
    uint32_t load_watermark;

    const char *addr[ce_addr_type_max];
    const uint32_t *addr_len[ce_addr_type_max];
    uint32_t addr_type[ce_addr_type_max];
    uint32_t num_addr;
} de_cache_attr_t, crst_ce_attr_t;

typedef struct tag_de_input {
    char resolv_addr[32];
    uint32_t ce_count;
    uint32_t ttl;
    const routing_policy_t *rp;
    cache_entity_addr_type_t in_addr_type;
    de_cache_attr_t *ce_attr;
} de_input_t, crst_search_out_t;

typedef int32_t (*de_init_fnp)(void **state);
typedef int32_t (*de_decide_fnp)(void *state, 
				 crst_search_out_t *di, 
				 uint8_t **result, 
				 uint32_t* result_len);
typedef int32_t (*de_cleanup_fnp)(void *state);

typedef struct tag_de_intf_t {
    de_init_fnp init;
    de_decide_fnp decide;
    de_cleanup_fnp cleanup;
} de_intf_t;

#ifdef __cplusplus
}
#endif

#endif //_DE_INTF_
