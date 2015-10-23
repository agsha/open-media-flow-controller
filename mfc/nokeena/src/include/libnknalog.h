/*
 * libnknalog.h -- Generic access log client interface 
 */
#ifndef _LIBNKNALOG_H
#define _LIBNKNALOG_H
#include "proto_http/proto_http.h"
#include "accesslog_pub.h"

typedef void * alog_token_t;

typedef struct alog_conn_data {
    /*
     * Socket information
     */
    uint16_t	reserved;
    uint16_t	dst_port;
    ip_addr_t	src_ipv4v6;
    ip_addr_t	dst_ipv4v6;

    /*
     * HTTP transaction information
     */
    struct timeval start_ts;	// HTTP begin timestamp 
    struct timeval end_ts;	// HTTP end timestamp
    struct timeval ttfb_ts;	// Time to first byte
    uint64_t	flag; // See ALF_XXX defines accesslog_pub.h
    uint64_t	in_bytes;
    uint64_t	out_bytes;
    uint16_t	status;
    uint16_t	reserved1;
    uint32_t	subcode;
    uint32_t	resp_hdr_size;
} alog_conn_data_t;

/*
 * alog_write() -- Write access log entry
 *
 *  Return:
 *   ==0 Success
 *   !=0 Error
 */
int alog_write(alog_token_t alog_token, alog_conn_data_t *cdata, 
	       token_data_t td);

/*
 * alog_init() -- Initialize alog client
 *
 *  alog_pname -- Access log profile name
 *
 *  Return:
 *   !=0 Success
 *   ==0 Error
 */
alog_token_t alog_init(const char *alog_pname);

#endif /* _LIBNKNALOG_H */

/*
 * End of libnknalog.h
 */
