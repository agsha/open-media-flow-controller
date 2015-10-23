/*
 *
 * Filename:  pe_mgmt.h
 * Date:      2011/06/`6
 * Author:    Thilak Raj S
 *
 * (C) Copyright 2009-10 Juniper  Networks, Inc.
 * All rights reserved.
 *
 *
 */
#ifndef NVSD_MGMT_PE_H
#define NVSD_MGMT_PE_H

#define NKN_MAX_PE_SRVR       256
#define NKN_MAX_PE_SRVR_HST     8
#define NKN_MAX_PE_SRVR_BIND    8
#define NKN_MAX_PE_SRVR_STR    16
#define NKN_MAX_PE_HOST_STR    16
#define NKN_PE_ALLOWED_CHARS   "/\\*:|`\"?"


typedef enum policy_srvr_proto {
	ICAP,

	OUT_OF_BOUNDS,
}policy_srvr_proto_t;

typedef enum policy_srvr_callouts {
	CLIENT_REQ,
	CLIENT_RSP,
	ORIGIN_REQ,
	ORIGIN_RSP,

	MAX_CALLOUTS,
}policy_srvr_callouts_t;


typedef enum policy_failover_type {
	BYPASS,
	REJECT,

}policy_failover_type_t;

typedef struct policy_ep_st {
	char *domain;
	uint16_t port;
} policy_ep_t;


typedef struct policy_srvr_st {
	char *name;
	char *uri;
	boolean enable;
	policy_srvr_proto_t proto;
	policy_ep_t endpoint[NKN_MAX_PE_SRVR_HST];
} policy_srvr_t;

/*
typedef policy_srvr_obj_st {
	policy_srvr_t   *policy_srvr;
	tbool callouts[MAX_CALLOUTS];
	policy_failover_type fail_over;
	uint16_t precedence;
} policy_srvr_obj_t
*/

#endif
