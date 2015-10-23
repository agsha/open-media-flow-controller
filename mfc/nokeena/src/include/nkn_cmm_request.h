/*
 * nkn_cmm_request.h - NVSD/CMM request definitions
 */

#ifndef NKN_CMM_REQUEST_H
#define NKN_CMM_REQUEST_H

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_CMM_DATA_ELEMENTS 256 // always equal to MAX_CLUSTER_MAP_NODES

#define MAX_CMM_NODE_HANDLE_LEN 1024 // Include NULL
/*
 * cmm_node_handle() - Construct node handle used between NVSD and CMM
 *      Returns: 0 => Success
 */
int cmm_node_handle(uint64_t namespace_token, int instance,
                    const char *host_port, char *buf, int buflen);

/*
 * cmm_node_handle_deserialize() - Break node_handle into defined components
 *
 *	Returns: 0 => Success
 *	Caller allocates sufficient buffer for (char *) element.
 */
int cmm_node_handle_deserialize(const char *buf, int buflen,
				uint64_t *ns_token, int *instance, 
				char *host_port);
/*
 * FQueue name element definitions
 */

/*
 *******************************************************************************
 * Monitor node request (NVSD => CMM)
 *******************************************************************************
 */
#define REQ_CMM_MONITOR "Monitor"
#define REQ_CMM_MONITOR_STRLEN 7

#define REQ_CMM_NAMESPACE "Namespace" // Associated namespace name used in nvsd
#define REQ_CMM_NAMESPACE_STRLEN 9

#define REQ_CMM_HTTP_HEADER "http_header" // FQueue known value
#define REQ_CMM_HEADER_STRLEN 11
/*
 * The REQ_HEADER field is a serialized mime_header_t
 */

#define REQ_CMM_DATA "Data"
#define REQ_CMM_DATA_STRLEN 4
/*
 * The REQ_CMM_DATA field consists of the following entry for each node:
 *
 *     	{NameSpaceToken}:{Instance}:{Host:Port}{Space}
 *	{Heartbeat HTTP URL}{Space}
 *     	{Heart Beat Interval}{Space}
 *	{Connect Timeout}{Space}
 *	{Allowed Connect Failures}{Space}
 *     	{Read-timeout};
 */

typedef struct cmm_node_config {
    char *node_handle;
    char *heartbeaturl;
    int heartbeat_interval;
    int connect_timeout;
    int allowed_connect_failures;
    int read_timeout;
} cmm_node_config_t;

/*
 * cmm_request_monitor_data_serialize() - serialize cmm_node_config_t
 *	Returns: Output bytes consumed
 */
int cmm_request_monitor_data_serialize(cmm_node_config_t *cmm_config,
			       	       char *data, int datalen);
/*
 * cmm_request_monitor_data_deserialize() - deserialize cmm_node_config_t
 *	If no malloc/free proc provided, all returned strings are 
 *	malloc'ed memory.
 *
 *	Returns: Input bytes consumed => Success
 *		 <0 => Error
 */
int cmm_request_monitor_data_deserialize(const char *data, int datalen,
				 	 cmm_node_config_t *cmm_config,
					 void *(*malloc_proc)(size_t sz),
					 void (*free_proc)(void *p));
/*
 *******************************************************************************
 * Cancel monitor node request (NVSD => CMM)
 *******************************************************************************
 */
#define REQ_CMM_CANCEL "Cancel"
#define REQ_CMM_CANCEL_STRLEN 6

/*
 * The REQ_CMM_DATA field consists of the following entry for each node:
 *
 *     	{NameSpaceToken}:{Instance}:{Host:Port};
 */

/*
 * cmm_request_cancel_data_serialize() - serialize cancel node handles
 *	Returns: Output bytes consumed
 */
int cmm_request_cancel_data_serialize(int nodes,
				const char *node_handle[MAX_CMM_DATA_ELEMENTS],
				char *data, int datalen);

/*
 * nvsd_request_data_deserialize() deserialize node_handle entries
 *	Returns: 0 => Success
 */
int cmm_request_cancel_data_deserialize(const char *data, int datalen, 
			      int *nodes,
			      const char *node_handle[MAX_CMM_DATA_ELEMENTS], 
			      int node_handle_strlen[MAX_CMM_DATA_ELEMENTS]);

/*
 *******************************************************************************
 * Resend monitor node requests (CMM => NVSD)
 *******************************************************************************
 */
#define REQ_NVSD_RESEND "Resend"
#define REQ_NVSD_RESEND_STRLEN 6

/*
 *******************************************************************************
 * Node(s) online/offline (CMM => NVSD)
 *******************************************************************************
 */
#define REQ_NVSD_ONLINE "Online"
#define REQ_NVSD_ONLINE_STRLEN 6

#define REQ_NVSD_OFFLINE "Offline"
#define REQ_NVSD_OFFLINE_STRLEN 7

#define REQ_NVSD_DATA "Data"
#define REQ_NVSD_DATA_STRLEN 4
/*
 * The REQ_NVSD_DATA field consists of the following entry for each node:
 *
 *     	{NameSpaceToken}:{Instance}:{Host:Port};
 *
 */

/*
 * nvsd_request_data_serialize() serialize node_handle(s) 
 *	Returns: Output bytes consumed
 */
int nvsd_request_data_serialize(int nodes, 
				const char *node_handle[MAX_CMM_DATA_ELEMENTS],
				char *data, int datalen);
/*
 * nvsd_request_data_deserialize() deserialize node_handle entries
 *	Returns: 0 => Success
 */
int nvsd_request_data_deserialize(const char *data, int datalen, int *nodes,
			      const char *node_handle[MAX_CMM_DATA_ELEMENTS], 
			      int node_handle_strlen[MAX_CMM_DATA_ELEMENTS]);

#ifdef __cplusplus
}
#endif
#endif /* NKN_CMM_REQUEST_H */
/*
 * End of nkn_cmm_request.h
 */
