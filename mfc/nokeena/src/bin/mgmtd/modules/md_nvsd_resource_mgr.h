/*
 *
 * Filename:  md_nvsd_resource_manager.h
 * Date:      2010/09/12
 * Author:    Thilak Raj S
 *
 * (C) Copyright 2010 Juniper Networks, Inc.
 * All rights reserved.
 *
 *
 */


#define sp "/stat/nkn"
struct sample_config
{
	/*! Sample node specific details */
	char *name;             /* string */
	char *enable;           /* bool */
	char *node;             /* name */
	uint32 interval;        /* duration */
	uint32 num_to_keep;     /* uint32 */
	char * sample_method;   /* string {direect,delta,aggregate}    */
	char * delta_constraint;/* string {none,increasing,decreasing} */
	uint32 sync_interval;   /* uint32 */
	char *label;            /* string - description*/
	char *unit;             /* string - unit*/
};

#define define_sample(n,nde,metd,lbl,ut) \
{ \
	.name = (char *)n , \
	.enable = (char *)"true" , \
	.node =  (char *)nde  , \
	.interval = 5 , \
	.num_to_keep = 5 , \
	.sample_method =  (char *)metd  , \
	.delta_constraint = (char *)"increasing", \
	.sync_interval = 10 , \
	.label = (char *)lbl , \
	.unit = (char *)ut , \
},

#define define_sample_end define_sample

#define rm_ns  "/nkn/nvsd/resource_mgr/namespace/*/"

struct sample_config sample_cfg_entries[] = {
	define_sample("ns_served_bytes", rm_ns "*/served_bytes","delta",
			"ns_current_bw","BITS")
	define_sample("ns_transactions", rm_ns"*/transactions","delta",
			"ns_current_transactions","Number of transactions per sec")

	define_sample_end(NULL,NULL,NULL,NULL,NULL) // { NULL, NULL, NULL, NULL }
};

#define MAX_RSRC_POOL_NAME 16

struct resource_pool_s {
    char name[MAX_RSRC_POOL_NAME];
    uint64_t config_bw;	/* same as mdb config node */
    uint64_t bandwidth; /* calculated bandwidth, */
    uint64_t config_conn;
    uint64_t connection;
};

enum resource_type {
    e_rp_client_sessions,
    e_rp_client_bandwidth,
};
