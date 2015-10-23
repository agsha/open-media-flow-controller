/*
 * cl_cmm.h - Push requests to Cluster Membership Manager (CMM) FQueue
 */
#ifndef CL_CMM_H_
#define CL_CMM_H_
#include "nkn_namespace.h"

/*
 * cl_cmm_init() - system initialization
 *	Return: 0 => Success
 */
int cl_cmm_init(void);

/*
 * send_cmm_monitor() - Push monitor nodes to CMM Fqueue
 *
 *	Input:
 *	  nodes - Nodes in configuration
 *	  host_port - host:port sring
 *	  host_port_strlen - strlen
 *	  heartbeat_path - path portion of the heartbeat URL
 *	  heartbeat_path_strlen - strlen
 *	  hdr - Headers to send in heartbeat request
 *	  token - Associated token
 *	  instance - Associated instance
 *
 * 	Returns: 0 => Success
 */
int send_cmm_monitor(int nodes, const char **host_port,
                     const int **host_port_strlen, 
		     const char **heartbeat_path,
		     const int **heartbeat_path_strlen, 
		     namespace_token_t token, int instance);

/*
 * send_cmm_cancel_monitor() - Push cancel monitor nodes to CMM Fqueue
 *
 *	Input:
 *	  nodes - Nodes in configuration
 *	  host_port - host:port sring
 *	  host_port_strlen - strlen
 *	  token - Associated token
 *	  instance - Associated instance
 *
 * 	Returns: 0 => Success
 */
int send_cmm_cancel_monitor(int nodes, const char **host_port,
			    const int **host_port_strlen, 
			    namespace_token_t token, int instance);

#endif /* CL_CMM_H_ */
/*
 * End of cl_cmm.h
 */
