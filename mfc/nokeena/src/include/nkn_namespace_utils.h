/*
 *******************************************************************************
 * nkn_namespace_utils.h -- Public object constructor and destructors
 *******************************************************************************
 */

#ifndef _NKN_NAMESPACE_UTILS_H
#define _NKN_NAMESPACE_UTILS_H

#include "nkn_namespace.h"
#include "nvsd_mgmt_namespace.h"

ssp_config_t *new_ssp_config_t(namespace_node_data_t *nd);
void delete_ssp_config_t(ssp_config_t *ssp);

http_config_t *new_http_config_t(namespace_node_data_t *nd, 
				 namespace_config_t *nsc,
				 const namespace_config_t *od_nsc);
void delete_http_config_t(http_config_t *http);

pub_point_config_t *new_pub_point_config_t(namespace_node_data_t *nd);
void delete_pub_point_config_t(pub_point_config_t *pp);

rtsp_config_t *new_rtsp_config_t(namespace_node_data_t *nd);
void delete_rtsp_config_t(rtsp_config_t *pp);

cluster_config_t *new_cluster_config_t(namespace_node_data_t *nd,
				       namespace_config_t *nsc,
				       const namespace_config_t *od_nsc);
void delete_cluster_config_t(cluster_config_t *cl_config);

/* Create a cache pinning config instance. Populate the config
 * from the namespace nodes received from the management config
 * database.
 */
cache_pin_config_t *new_cache_pin_config_t(namespace_node_data_t *nd);

/* Delete a cache pinning config instance. 
 * Called when namespace config is being deleted.
 */
void delete_cache_pin_config_t(cache_pin_config_t *cpcfg);

/* Create a namespace specific access log config instance. The config
 * instance is owned by the namesapce */
struct ns_accesslog_config *new_accesslog_config_t(namespace_node_data_t *nd);

/* Delete a namespace specific accesslog config instance. */
void delete_accesslog_config_t(struct ns_accesslog_config *aclogcfg);

compress_config_t *new_compress_config_t(namespace_node_data_t *nd);

/* Delete a compress config instance. 
 * Called when namespace config is being deleted.
 */
void delete_compress_config_t(compress_config_t *cpcfg);

unsuccess_response_cache_cfg_t *new_non2xx_cache_config_t(namespace_node_data_t *nd);

/* Delete a non2xx_cache config instance. 
 * Called when namespace config is being deleted.
 */
void delete_non2xx_cache_config_t(unsuccess_response_cache_cfg_t *cpcfg);

/*
 * Create URL Filter Trie instance
 */
void *new_url_filter_trie(const char *fname, 
				       const char *namespace,int *err);

/*
 * Duplicate (bump reference count) URL Filter Trie instance
 */
void *dup_url_filter_trie(void *uf_trie);

/*
 * Delete URL Filter Trie instance
 */
void delete_url_filter_trie(void *uf_trie);

/*
 * Create a namespace specific URL filter instance.
 * The config instance is owned by the namespace.
 */
url_filter_config_t *new_url_filter_config_t(namespace_node_data_t *nd);

/*
 * Delete a URL filter instance.
 * Called when namespace config is being deleted.
 */
void delete_url_filter_config_t(url_filter_config_t *ufc);

#endif  /* _NKN_NAMESPACE_UTILS_H */

/*
 * End of nkn_namespace_utils.h
 */
