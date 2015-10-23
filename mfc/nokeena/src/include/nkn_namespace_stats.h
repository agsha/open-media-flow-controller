#ifndef NKN_NAMESPACE_STATS_H
#define NKN_NAMESPACE_STATS_H

#include <string.h>

#include "nkn_defs.h"
#include "nvsd_mgmt_namespace.h"

/*
 * Should probably change this from enum to a parameterized value, proportional
 * to the # of supported drives.
 */
typedef enum {
    NS_CACHE_PIN_SPACE_RESV_TOTAL = 1,
    NS_CACHE_PIN_SPACE_USED_TOTAL,
    NS_CACHE_PIN_SPACE_ENABLED,
    NS_CACHE_PIN_SPACE_OBJ_LIMIT,
    NS_CACHE_PIN_OBJECT_COUNT,

    NS_CACHE_PIN_SPACE_D1,
    NS_CACHE_PIN_SPACE_D2,
    NS_CACHE_PIN_SPACE_D3,
    NS_CACHE_PIN_SPACE_D4,
    NS_CACHE_PIN_SPACE_D5,
    NS_CACHE_PIN_SPACE_D6,
    NS_CACHE_PIN_SPACE_D7,
    NS_CACHE_PIN_SPACE_D8,
    NS_CACHE_PIN_SPACE_D9,
    NS_CACHE_PIN_SPACE_D10,
    NS_CACHE_PIN_SPACE_D11,
    NS_CACHE_PIN_SPACE_D12,
    NS_CACHE_PIN_SPACE_D13,
    NS_CACHE_PIN_SPACE_D14,
    NS_CACHE_PIN_SPACE_D15,
    NS_CACHE_PIN_SPACE_D16,
    NS_CACHE_PIN_SPACE_D17,
    NS_CACHE_PIN_SPACE_D18,
    NS_CACHE_PIN_SPACE_D19,
    NS_CACHE_PIN_SPACE_D20,
    NS_CACHE_PIN_SPACE_D21,
    NS_CACHE_PIN_SPACE_D22,
    NS_CACHE_PIN_SPACE_D23,
    NS_CACHE_PIN_SPACE_D24,
    NS_CACHE_PIN_SPACE_D25,
    NS_CACHE_PIN_SPACE_D26,
    NS_CACHE_PIN_SPACE_D27,
    NS_CACHE_PIN_SPACE_D28,
    NS_CACHE_PIN_SPACE_D29,
    NS_CACHE_PIN_SPACE_D30,
    NS_CACHE_PIN_SPACE_D31,
    NS_CACHE_PIN_SPACE_D32,
    NS_CACHE_PIN_SPACE_D33,
    NS_CACHE_PIN_SPACE_D34,
    NS_CACHE_PIN_SPACE_D35,
    NS_CACHE_PIN_SPACE_D36,
    NS_CACHE_PIN_SPACE_D37,
    NS_CACHE_PIN_SPACE_D38,
    NS_CACHE_PIN_SPACE_D39,
    NS_CACHE_PIN_SPACE_D40,
    NS_CACHE_PIN_SPACE_D41,	// extra slot
    NS_CACHE_PIN_SPACE_D42,	// extra slot
    NS_CACHE_PIN_SPACE_D43,	// extra slot
    NS_CACHE_PIN_SPACE_D44,	// extra slot
    NS_CACHE_PIN_SPACE_D45,	// extra slot
    NS_CACHE_PIN_SPACE_D46,	// extra slot
    NS_CACHE_PIN_SPACE_D47,	// extra slot
    NS_CACHE_PIN_SPACE_D48,	// extra slot
    NS_CACHE_PIN_SPACE_D49,	// extra slot
    NS_CACHE_PIN_SPACE_D50,	// extra slot
    NS_CACHE_PIN_SPACE_MAX,

    NS_USED_SPACE_SSD,
    NS_USED_SPACE_SAS,
    NS_USED_SPACE_SATA,

    NS_USED_SPACE_D1,
    NS_USED_SPACE_D2,
    NS_USED_SPACE_D3,
    NS_USED_SPACE_D4,
    NS_USED_SPACE_D5,
    NS_USED_SPACE_D6,
    NS_USED_SPACE_D7,
    NS_USED_SPACE_D8,
    NS_USED_SPACE_D9,
    NS_USED_SPACE_D10,
    NS_USED_SPACE_D11,
    NS_USED_SPACE_D12,
    NS_USED_SPACE_D13,
    NS_USED_SPACE_D14,
    NS_USED_SPACE_D15,
    NS_USED_SPACE_D16,
    NS_USED_SPACE_D17,
    NS_USED_SPACE_D18,
    NS_USED_SPACE_D19,
    NS_USED_SPACE_D20,
    NS_USED_SPACE_D21,
    NS_USED_SPACE_D22,
    NS_USED_SPACE_D23,
    NS_USED_SPACE_D24,
    NS_USED_SPACE_D25,
    NS_USED_SPACE_D26,
    NS_USED_SPACE_D27,
    NS_USED_SPACE_D28,
    NS_USED_SPACE_D29,
    NS_USED_SPACE_D30,
    NS_USED_SPACE_D31,
    NS_USED_SPACE_D32,
    NS_USED_SPACE_D33,
    NS_USED_SPACE_D34,
    NS_USED_SPACE_D35,
    NS_USED_SPACE_D36,
    NS_USED_SPACE_D37,
    NS_USED_SPACE_D38,
    NS_USED_SPACE_D39,
    NS_USED_SPACE_D40,
    NS_USED_SPACE_D41,
    NS_USED_SPACE_D42,
    NS_USED_SPACE_D43,
    NS_USED_SPACE_D44,
    NS_USED_SPACE_D45,
    NS_USED_SPACE_D46,
    NS_USED_SPACE_D47,
    NS_USED_SPACE_D48,
    NS_USED_SPACE_D49,
    NS_USED_SPACE_D50,
    NS_USED_SPACE_MAX,

    NS_TOTAL_OBJECTS_D1,
    NS_TOTAL_OBJECTS_D2,
    NS_TOTAL_OBJECTS_D3,
    NS_TOTAL_OBJECTS_D4,
    NS_TOTAL_OBJECTS_D5,
    NS_TOTAL_OBJECTS_D6,
    NS_TOTAL_OBJECTS_D7,
    NS_TOTAL_OBJECTS_D8,
    NS_TOTAL_OBJECTS_D9,
    NS_TOTAL_OBJECTS_D10,
    NS_TOTAL_OBJECTS_D11,
    NS_TOTAL_OBJECTS_D12,
    NS_TOTAL_OBJECTS_D13,
    NS_TOTAL_OBJECTS_D14,
    NS_TOTAL_OBJECTS_D15,
    NS_TOTAL_OBJECTS_D16,
    NS_TOTAL_OBJECTS_D17,
    NS_TOTAL_OBJECTS_D18,
    NS_TOTAL_OBJECTS_D19,
    NS_TOTAL_OBJECTS_D20,
    NS_TOTAL_OBJECTS_D21,
    NS_TOTAL_OBJECTS_D22,
    NS_TOTAL_OBJECTS_D23,
    NS_TOTAL_OBJECTS_D24,
    NS_TOTAL_OBJECTS_D25,
    NS_TOTAL_OBJECTS_D26,
    NS_TOTAL_OBJECTS_D27,
    NS_TOTAL_OBJECTS_D28,
    NS_TOTAL_OBJECTS_D29,
    NS_TOTAL_OBJECTS_D30,
    NS_TOTAL_OBJECTS_D31,
    NS_TOTAL_OBJECTS_D32,
    NS_TOTAL_OBJECTS_D33,
    NS_TOTAL_OBJECTS_D34,
    NS_TOTAL_OBJECTS_D35,
    NS_TOTAL_OBJECTS_D36,
    NS_TOTAL_OBJECTS_D37,
    NS_TOTAL_OBJECTS_D38,
    NS_TOTAL_OBJECTS_D39,
    NS_TOTAL_OBJECTS_D40,
    NS_TOTAL_OBJECTS_D41,
    NS_TOTAL_OBJECTS_D42,
    NS_TOTAL_OBJECTS_D43,
    NS_TOTAL_OBJECTS_D44,
    NS_TOTAL_OBJECTS_D45,
    NS_TOTAL_OBJECTS_D46,
    NS_TOTAL_OBJECTS_D47,
    NS_TOTAL_OBJECTS_D48,
    NS_TOTAL_OBJECTS_D49,
    NS_TOTAL_OBJECTS_D50,
    NS_TOTAL_OBJECTS_MAX,

    NS_INGEST_OBJECTS_SATA,
    NS_INGEST_OBJECTS_SAS,
    NS_INGEST_OBJECTS_SSD,

    NS_DELETE_OBJECTS_SATA,
    NS_DELETE_OBJECTS_SAS,
    NS_DELETE_OBJECTS_SSD,

    NS_SATA_MAX_USAGE,
    NS_SATA_RESV_USAGE,
    NS_SATA_ACT_USAGE,

    NS_SAS_MAX_USAGE,
    NS_SAS_RESV_USAGE,
    NS_SAS_ACT_USAGE,

    NS_SSD_MAX_USAGE,
    NS_SSD_RESV_USAGE,
    NS_SSD_ACT_USAGE,

    NS_STAT_MAX,		/* The largest value */
} ns_stat_category_t;


typedef enum {
    NS_STATE_ACTIVE = 1,
    NS_STATE_INACTIVE = 2,
    NS_STATE_DELETED = 3,
    NS_STATE_DELETED_BUT_FILLED = 4,
    NS_STATE_TO_BE_DELETED  = 5,
} ns_state_t;
#define IS_NS_STATE_DELETING(ns) \
   ((ns)->state == NS_STATE_DELETED || (ns)->state == NS_STATE_TO_BE_DELETED)



typedef struct ns_stat_token_s {
    union {
        struct ns_stat_token_data_s {
	    uint32_t gen;
	    uint32_t val;
	} stat_token_data;
	uint64_t stat_token;
    } u;
} ns_stat_token_t;
#define NS_STAT_NULL ((ns_stat_token_t)0)
#define NS_STAT_TOKEN_IDX(s) (s.u.stat_token_data.val)
#define NS_STAT_TOKEN_GEN(s) (s.u.stat_token_data.gen)

/* Tier specifc namespace configs */
typedef struct ns_tier_entry {
    uint32_t	read_size;
    uint32_t	block_free_threshold;
    size_t	max_disk_usage;
    int		group_read;
} ns_tier_entry_t;

typedef struct ns_stat_entry {
    char	    ns_key[NKN_MAX_UID_LENGTH+1]; /* namespace:uuid */
    uint64_t	    index;
    uint32_t	    token_curr_gen;
    int		    rp_index;		/* pass to resource pool code */
    ns_tier_entry_t ssd;
    ns_tier_entry_t sas;
    ns_tier_entry_t sata;
    uint64_t	    cache_pin_resv_bytes;	/* byte reservation */
    uint64_t	    cache_pin_max_obj_bytes;
    int		    cache_pin_enabled;
    ns_state_t	    state;
    uint32_t        num_dup_ns;         /* Number of duplicate namespaces the
					   stat is pointing to  */
} ns_stat_entry_t;

extern ns_stat_token_t NS_NULL_STAT_TOKEN;

static inline int
ns_stat_token_equal(ns_stat_token_t st1,
		    ns_stat_token_t st2)
{
    return (st1.u.stat_token == st2.u.stat_token);
}	/* ns_stat_token_equal */

static inline int
ns_is_stat_token_null(ns_stat_token_t stoken)
{
    return ns_stat_token_equal(stoken, NS_NULL_STAT_TOKEN);
}	/* ns_is_stat_token_null */

/* Stat namespace routines */
int ns_stat_add_namespace(const int nth, const int state, const char *ns_uuid,
			  const uint64_t cache_pin_max_obj_size,
			  const uint64_t cache_pin_resv_bytes,
			  const int cache_pin_enabled,
			  const ns_tier_entry_t *ssd,
			  const ns_tier_entry_t *sas,
			  const ns_tier_entry_t *sata,
			  const int lock);
int ns_stat_del_namespace(const char *ns_uuid);
int ns_stat_add_new_namespace(namespace_node_data_t *new_ns);
int ns_stat_get_initial_namespaces(void);
int ns_stat_total_subcategory(ns_stat_category_t cat, int64_t *total);

/* Stat value routines */
int ns_stat_add(ns_stat_token_t token, ns_stat_category_t stat_type,
		int64_t val, int64_t *new_val);
int ns_stat_get(ns_stat_token_t token, ns_stat_category_t stat_type,
		int64_t *curr_val);
int ns_stat_set(ns_stat_token_t token, ns_stat_category_t stat_stype,
		int64_t val);
ns_stat_token_t ns_stat_get_stat_token(const char *namespace_uuid);
int ns_stat_free_stat_token(ns_stat_token_t ns_stoken);
ns_stat_token_t ns_stat_get_stat_token_cache_name_dir(const char *uri_dir);
int ns_stat_get_rpindex_from_nsuuid(const char *ns_uuid, int *rpindex);

int ns_stat_get_tier_max_disk_usage(const char *ns_uuid,
				    nkn_provider_type_t ptype,
				    size_t *size_in_mb);
int ns_stat_set_tier_max_disk_usage(const char *ns_uuid,
				    nkn_provider_type_t ptype,
				    size_t size_in_mb);
int ns_stat_get_tier_group_read_state(const char *ns_uuid,
				      nkn_provider_type_t ptype,
				      int *enabled);
int ns_stat_set_tier_group_read_state(const char *ns_uuid,
				      nkn_provider_type_t ptype,
				      int enabled);
int ns_stat_get_tier_block_free_threshold(const char *ns_uuid,
					  nkn_provider_type_t ptype,
					  uint32_t *prcnt);
int ns_stat_set_tier_block_free_threshold(const char *ns_uuid,
					  nkn_provider_type_t ptype,
					  uint32_t prcnt);
int ns_stat_get_tier_read_size(const char *ns_uuid,  nkn_provider_type_t ptype,
			       uint32_t *read_size);
int ns_stat_set_tier_read_size(const char *ns_uuid,  nkn_provider_type_t ptype,
			       uint32_t read_size);

int ns_stat_get_cache_pin_enabled_state(const char *namespace_uuid,
					int *enabled);
int ns_stat_set_cache_pin_enabled_state(const char *ns_uuid, int enabled);
int ns_stat_get_cache_pin_max_obj_size(const char *ns_uuid,uint64_t *max_bytes);
int ns_stat_set_cache_pin_max_obj_size(const char *ns_uuid, uint64_t max_bytes);
int ns_stat_set_cache_pin_diskspace(const char *ns_uuid,
				    uint64_t max_resv_bytes);

#endif /* NKN_NAMESPACE_STATS_H */

