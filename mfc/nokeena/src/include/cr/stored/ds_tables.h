
#ifndef _DS_TABLES_
#define _DS_TABLES_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 //keeps emacs happy
}
#endif

#include <glib.h>
#include <sys/queue.h>

#include "ds_hash_table_mgmt.h"
#include "lfc/nkn_lf_client.h"
#include "cr/common/cr_utils.h"
//#include "cr/de_intf.h"
#include "cr/stored/crst_common_defs.h"

#define SET_DS_OBJ_STATE_SAFE(_p,_x)\
    pthread_mutex_lock(&_p->lock);\
    _p->state = _x;\
    pthread_mutex_unlock(&_p->lock);

#define SET_DS_OBJ_STATE(_p,_x)\
    _p->state = _x;\

#define HOLD_DS_OBJ(_p) \
    pthread_mutex_lock(&_p->lock); \

#define REL_DS_OBJ(_p) \
    pthread_mutex_unlock(&_p->lock); \

/* need to get these from mgmt */
#define NAME_STR_LEN 64 
#define IP_STR_LEN 128
#define PATH_STR_LEN 256

#define HOLD_CRST_CFG(_p) \
    pthread_mutex_lock(&(_p)->lock);\

#define REL_CRST_CFG(_p) \
    pthread_mutex_unlock(&(_p)->lock);\

typedef struct tag_cr_dns_store_cfg{
    pthread_mutex_t lock;
    unsigned int max_domains;
    unsigned int max_cache_entities;
    char listen_path[PATH_STR_LEN];
    unsigned int listen_port;
    unsigned char debug_assert;
    char *geodb_file_name;
    uint8_t geodb_installed;
} cr_dns_store_cfg_t;

typedef struct tag_domain_stats {
    uint64_t num_hits;
    uint64_t num_err;
} domain_stats_t;

typedef struct domain_list{
    pthread_mutex_t lock;
    char* name;
    uint32_t name_len;
    uint8_t wildcard;
    hash_tbl_props_t* attributes;
    routing_policy_t rp;
    char cname_last_resort[256];
    uint32_t cname_last_resort_len;
    GList* cg_ptr_list;
    void *de_state;
    const de_intf_t *de_cb;
    uint32_t ttl;
    domain_stats_t stats;
}domain_t;

typedef struct cache_group{
    pthread_mutex_t lock; 
    char* name;
    hash_tbl_props_t* attributes;
    GList* ce_ptr_list;
    GList* domain_list; 
}cache_group_t;

typedef enum tag_pop_state {
    POP_ACTIVE,
    POP_DEAD
} pop_state_t;

//struct Pop;
typedef struct Pop{
    pthread_mutex_t lock;
    pop_state_t state;    
    char* name;
    conjugate_graticule_t location;
    hash_tbl_props_t* attributes;
    GList* ce_ptr_list;
}pop_t;

typedef enum tag_cahe_entity_state {
    CE_ACTIVE,
    CE_DEAD
} cache_entity_state_t;

typedef struct tag_lfc_attr_t {
    uint16_t port;
    uint32_t poll_freq;
} lfc_attr_t;

typedef struct cache_entity{
    pthread_mutex_t lock;
    cache_entity_state_t state;
    char name[NAME_STR_LEN];
    char addr[ce_addr_type_max][IP_STR_LEN];
    uint32_t addr_len[ce_addr_type_max];
    cache_entity_addr_type_t addr_type;
    uint32_t load_watermark;
    hash_tbl_props_t* attributes; 
    GList* cg_ptr_list;
    const pop_t *pop;
    lfc_attr_t lf_attr;
    obj_lf_client_t *lfc;
    lf_stats_t fb;
} cache_entity_t; 


#ifdef __cplusplus
}
#endif

#endif //_DS_TABLES_
