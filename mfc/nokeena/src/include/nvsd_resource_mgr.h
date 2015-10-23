/*
 *
 * Filename:  nvsd_resource_mgr.h
 * Date:      2010/09/23
 * Author:    Thilak Raj S
 *
 * (C) Copyright 2009-10 Juniper  Networks, Inc.
 * All rights reserved.
 *
 *
 */
#ifndef NVSD_RESOURCE_MGR__H
#define NVSD_RESOURCE_MGR__H
#include <atomic_ops.h>

#define NKN_MAX_RESOURCES	6
#define NKN_MAX_RESOURCE_POOL   1500
typedef struct resource_st {
	AO_t max;
	AO_t used;
	uint64_t configmax;
} resource_t;

typedef struct resource_pool_node_data_st {
	char					*name;
	uint32_t				index;
	uint32_t				num_ns_bound;
	struct resource_pool_node_data_st	*parent;
	resource_t				resources[NKN_MAX_RESOURCES];
} resource_pool_t;


typedef enum {
	RESOURCE_POOL_TYPE_CLIENT_SESSION = 1,
	RESOURCE_POOL_TYPE_ORIGIN_SESSION = 2,
	RESOURCE_POOL_TYPE_BW = 3,
	RESOURCE_POOL_TYPE_UTIL_TIER1 = 4,
	RESOURCE_POOL_TYPE_UTIL_SSD   = 4,
	RESOURCE_POOL_TYPE_UTIL_TIER2 = 5,
	RESOURCE_POOL_TYPE_UTIL_SAS   = 5,
	RESOURCE_POOL_TYPE_UTIL_TIER3 = 6,
	RESOURCE_POOL_TYPE_UTIL_SATA  = 6,

	RESOURCE_POOL_MAX        = 7,
} rp_type_en;

// Pointer to global resource pool
//resource_pool_t *p_global_resource_pool = NULL;

resource_pool_t *
nvsd_mgmt_get_resource_mgr(const char *cpResourcePool);

/*
 *      function : nvsd_rp_alloc_resource_no_check()
 *      purpose : Allocate the specifed amount of resource from the resource pool.
 *		  This version of alloc doesn't do a bounds check on the max resources.
 *      returns : 1 on success and 0 on failure
 */
int
nvsd_rp_alloc_resource_no_check(rp_type_en resource_type,
		       uint32_t rp_index,
		       uint64_t resource_count);


/*
 *      function : nvsd_rp_alloc_resource()
 *      purpose : Allocate the specifed amount of resource from the resource pool.
 *      returns : 1 on success and 0 on failure
 */
int
nvsd_rp_alloc_resource(rp_type_en resource_pool_type,
		       uint32_t rp_index,
		       uint64_t resource_count);

/*
 *      function : nvsd_rp_free_resource()
 *      purpose : free the specifed amount of resource to the resource pool.
 *      returns : 1 on success and 0 on failure
 */
int
nvsd_rp_free_resource(rp_type_en resource_pool_type,
		      uint32_t rp_index,
		      uint64_t resource_count);

/*
 *      function : nvsd_rp_get_resource()
 *      purpose : get the resource count from the resource pool.
 *      returns : (uint64)-1 on a error,count on success.
 */
uint64_t
nvsd_rp_get_resource(rp_type_en resource_type,
		     uint32_t rp_index);

/*
 *      function : nvsd_rp_set_resource()
 *      purpose : set the resource count to the resource pool.
 *      returns : 1 on success and 0 on failure
 */
int
nvsd_rp_set_resource(rp_type_en resource_type,
		     uint32_t rp_index,
		     uint64_t resource_count);


/*
 *      function : nvsd_rp_bw_timer_cleanup_1sec(void)
 *      purpose : clean up the resource used count.
 *      returns : Nothing.
 */
void
nvsd_rp_bw_timer_cleanup_1sec(void);


/*
 *      function : nvsd_rp_bw_update_send()
 *      purpose : update the used count.
 *      returns : Nothing.
 */
void
nvsd_rp_bw_update_send(int rp_index, uint64_t byte_sent);

/*
 *      function : nvsd_rp_bw_update_send()
 *      purpose : update the available bw.
 *      returns : 0 on no more BW,-1 for infinite.
 */
uint64_t
nvsd_rp_bw_query_available_bw(int rp_index);

//Generic ones
void
nvsd_rp_cleanup_used(rp_type_en resource_type);

uint64_t
nvsd_rp_query_available(rp_type_en resource_type,
			uint32_t rp_index);

int nvsd_rp_set_total(rp_type_en resource_type,
		       uint64_t resource);

uint64_t nvsd_rp_get_intf_total_bw(void);

#endif // NVSD_RESOURCE_MGR__H
