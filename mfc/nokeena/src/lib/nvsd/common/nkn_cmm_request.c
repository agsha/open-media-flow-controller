/*
 * nkn_cmm_request.c - Serialize/Deserialize functions for NVSD/CMM interface
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <alloca.h>
#include "nkn_defs.h"
#include "nkn_cmm_request.h"
#include "nkn_memalloc.h"

/*
 * cmm_node_handle() - Construct node handle used between NVSD and CMM
 *
 *	Returns: 0 => Success
 */
int cmm_node_handle(uint64_t namespace_token, int instance, 
		    const char *host_port, char *buf, int buflen)
{
    int rv;

    rv = snprintf(buf, buflen, "%lu:%d:%s", 
		  namespace_token, instance, host_port);
    if (rv < buflen) {
    	return 0; // Success
    } else {
    	return 1; // Failure
    }
}

/*
 * cmm_node_handle_deserialize() - Break node_handle into defined components
 *
 *	Returns: 0 => Success
 *	Caller allocates sufficient buffer for (char *) element.
 */
int cmm_node_handle_deserialize(const char *buf, int buflen,
				uint64_t *ns_token, int *instance, 
				char *host_port)
{
    int rv;
    int retval = 0;
    char *tbuf;

    tbuf = alloca(buflen+1);
    memcpy(tbuf, buf, buflen);
    tbuf[buflen] = '\0';

    rv = sscanf(tbuf, "%lu:%d:%s", ns_token, instance, host_port);
    if (rv != 3) {
    	retval = 1;
    }
    return retval;
}

/*
 * cmm_request_monitor_data_serialize() - serialize cmm_node_config_t
 *	Returns: Output bytes consumed
 */
int cmm_request_monitor_data_serialize(cmm_node_config_t *cmm_config,
			       char *data, int datalen)
{
    int rv;
    rv = snprintf(data, datalen, "%s %s %d %d %d %d;",
    		  cmm_config->node_handle, cmm_config->heartbeaturl,
		  cmm_config->heartbeat_interval, cmm_config->connect_timeout,
		  cmm_config->allowed_connect_failures, 
		  cmm_config->read_timeout);
    return rv;
}

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
					 void (*free_proc)(void *p))
{
    int rv;
    int retval;
    char *p_end;
    
    memset(cmm_config, 0, sizeof(*cmm_config));
    p_end = memchr(data, ';', datalen);
    if (!p_end) {
    	retval = -1;
	goto err_exit;
    }
    memset((char *)cmm_config, 0, sizeof(*cmm_config));

    if (malloc_proc) {
    	cmm_config->node_handle = (*malloc_proc)(1024);
    } else {
    	cmm_config->node_handle = nkn_malloc_type(1024, mod_cmm_req_str);
    }
    if (!cmm_config->node_handle) {
    	retval = -2;
	goto err_exit;
    }
    cmm_config->node_handle[1024-1] = '\0';

    if (malloc_proc) {
    	cmm_config->heartbeaturl = (*malloc_proc)(1024);
    } else {
    	cmm_config->heartbeaturl = nkn_malloc_type(1024, mod_cmm_req_str);
    }
    if (!cmm_config->heartbeaturl) {
    	retval = -3;
	goto err_exit;
    }
    cmm_config->heartbeaturl[1024-1] = '\0';

    rv = sscanf(data, "%1023s %1023s %d %d %d %d;", 
	        cmm_config->node_handle, cmm_config->heartbeaturl,
		&cmm_config->heartbeat_interval, &cmm_config->connect_timeout,
		&cmm_config->allowed_connect_failures, 
		&cmm_config->read_timeout);
    if (rv != 6) {
    	retval = -4;
	goto err_exit;
    }
    return (p_end - data) + 1;

err_exit:

    if (cmm_config->node_handle) {
    	if (free_proc) {
	    (*free_proc)(cmm_config->node_handle);
	} else {
	    free(cmm_config->node_handle);
	}
    	cmm_config->node_handle = NULL;
    }

    if (cmm_config->heartbeaturl) {
    	if (free_proc) {
	    (*free_proc)(cmm_config->heartbeaturl);
	} else {
	    free(cmm_config->heartbeaturl);
	}
    	cmm_config->heartbeaturl = NULL;
    }
    return retval;
}

/*
 * nvsd_request_data_serialize() - serialize node_handle(s) 
 *	Returns: Output bytes consumed
 */
int nvsd_request_data_serialize(int nodes, const char **node_handle, 
				char *data, int datalen)
{
    int rv;
    int n;
    char *buf = data;
    int buflen = datalen;
    int bytes_used = 0;

    for (n = 0; n < nodes; n++) {
    	rv = snprintf(&buf[bytes_used], buflen-bytes_used, 
		      "%s;", node_handle[n]);
	if (rv >= (buflen - bytes_used)) {
	    // Buffer space exceeded
	    return bytes_used;
	}
	bytes_used += rv;
    }
    return bytes_used;
}

int cmm_request_cancel_data_serialize(int nodes, const char **node_handle, 
				      char *data, int datalen)
{
    return nvsd_request_data_serialize(nodes, node_handle, data, datalen);
}

/*
 * nvsd_request_data_deserialize() - deserialize node_handle entries
 *	Returns: 0 => Success
 */
int nvsd_request_data_deserialize(const char *data, int datalen, int *nodes,
			      const char *node_handle[MAX_CMM_DATA_ELEMENTS],
			      int node_handle_strlen[MAX_CMM_DATA_ELEMENTS])
{
    int bytes_used = 0;
    const char *p_start = data;
    const char *p_end;
    int slen;

    *nodes = 0;
    while (bytes_used < datalen) {
	p_end = memchr(p_start, ';', datalen - bytes_used);
   	if (!p_end) {
	    break;
   	}
	slen = (p_end - p_start) + 1;
	node_handle[*nodes] = p_start;
	node_handle_strlen[*nodes] = slen-1;

   	bytes_used += slen;
	p_start += slen;
	(*nodes)++;
   }
   return 0;
}

int cmm_request_cancel_data_deserialize(const char *data, int datalen, 
				int *nodes,
			      	const char *node_handle[MAX_CMM_DATA_ELEMENTS],
			      	int node_handle_strlen[MAX_CMM_DATA_ELEMENTS])
{
    return nvsd_request_data_deserialize(data, datalen, nodes, node_handle,
    					 node_handle_strlen);
}

/*
 * End of nkn_cmm_request.c
 */
