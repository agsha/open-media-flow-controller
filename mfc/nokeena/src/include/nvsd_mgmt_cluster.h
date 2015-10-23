/*
 * Filename :   nvsd_mgmt_cluster.h
 * Date     :   2010/09/03
 * Author   :   Manikandan`Vengatachalam
 *
 * (C) Copyright 2010 Juniper Networks, Inc.
 * All rights reserved.
 *
 */

#ifndef __NVSD_MGMT_CLUSTER__H
#define __NVSD_MGMT_CLUSTER__H


#define MAX_SUFFIX_NUM 1024
/* one extra for '\0' */
#define MAX_SUFFIX_NAME_LEN 17


/* Redirect-method data structure, members as an enum for indicating the method, string to indicate address & port
 * to redirect, string hold url  to redirect path, and a Boolean to hold the local redirect allow/deny status.
 */
typedef enum
{
	REM_NONE = 0,
	CONSISTENT_HASH_REDIRECT = 1,
	CHASH_REDIRECT_REPLICATE = 2
} redirect_method_en;

typedef enum
{
	PROXY_REM_NONE = 0,
	CONSISTENT_HASH_PROXY = 1,
	CHASH_PROXY_REPLICATE = 2
} proxy_method_en;

/*For load-balance structure, an integer to hold the threshold, An enum for indicate whether it is
 * replicate-least-loaded or replicate-random.
 */

typedef enum
{
	REPLICATE_NONE = 0,
	REPLICATE_LEAST_LOADED= 1,
	REPLICATE_RANDOM= 2
} load_replicate_en;


typedef enum
{
   CL_TYPE_NONE = 0,
   CL_TYPE_REDIRECT =1,
   CL_TYPE_PROXY =2,
   CL_TYPE_CACHE_CLUSTER =3
} cl_type_en;

typedef enum
{
	CL_NONE = 0,
	CL_BASE_URL = 1,
	CL_COMPLETE_URL = 2
} cluster_hash_en;

/* Moved from nvsd_mgmt_namespace.h*/

typedef enum
{
	SM_FT_NONE = 0,
 	SM_FT_HOST_ORIGIN_MAP = 1,
	SM_FT_CLUSTER_MAP = 2,
	SM_FT_ORIG_ESC_MAP = 3,
	SM_FT_NFS_MAP = 4,
	SM_FT_ORIG_RR_MAP = 5,
} server_map_format_en;

typedef struct
{
	int heartbeat_interval;
	int connect_timeout;
	int allowed_fails;
	int read_timeout;
}cluster_monitor_params_t;

typedef struct server_map_node_data_st
{
	char			*name;
	server_map_format_en	format_type;
	char			*file_url;
	int			refresh;
	char			*binary_server_map;
	int			binary_size;
        int                     state;
	cluster_monitor_params_t monitor_values;
} server_map_node_data_t;
/*************************************************/

typedef struct redirect_method_data_st
{
	char * addr_port;
	char * baseRedirectPath;
	boolean local_redirect;
} redirect_method_data_t  ;

typedef struct proxy_method_data_st
{
	char * addr_port;
	char * baseRedirectPath;
	boolean local_redirect;
} proxy_method_data_t  ;

typedef struct lb_data_st
{
	load_replicate_en replicate;
	uint32_t threshold;
} lb_data_t  ;

typedef char suffix_name_t[MAX_SUFFIX_NAME_LEN];

typedef struct cl7_suffix_map {  
    suffix_name_t suffix_name;
    unsigned int action; // 1=Proxy; 2=Redirect
    int valid;
} cl7_suffix_map_t;


typedef struct cluster_node_data_st
{
    char *cluster_name;
    char *server_map_name;
    server_map_node_data_t *serverMap;
    cl_type_en cl_type;
    cluster_hash_en hash_url;

    // XXX: Add suffix map data declarations here
    int suffix_map_entries;
    cl7_suffix_map_t suffix_map[MAX_SUFFIX_NUM];


    union {
        struct cl_redirect {
            redirect_method_en method;

            union {
                struct rd_redirect_only {
                	redirect_method_data_t rd_data;
                } redirect_only;
                struct rd_redirect_replicate {
                	redirect_method_data_t rd_data;
                	lb_data_t lb_data;
                } redirect_replicate;
            } u;
        } redirect_cluster;

        struct cl_proxy {
            proxy_method_en method;

            union {
                struct rd_proxy_only {
                	proxy_method_data_t pd_data;
                } proxy_only;
                struct rd_proxy_replicate {
                	proxy_method_data_t pd_data;
                	lb_data_t lb_data;
                } proxy_replicate;
            } u;
        } proxy_cluster;

        struct cl_cache_cluster {

        } cache_cluster;
    } u;
} cluster_node_data_t;

#define cnd_redir 	u.redirect_cluster.u.redirect_only
#define cnd_redir_repl 	u.redirect_cluster.u.redirect_replicate
#define cnd_proxy 	u.proxy_cluster.u.proxy_only
#define cnd_proxy_repl 	u.proxy_cluster.u.proxy_replicate


#endif // __NVSD_MGMT_CLUSTER__H
