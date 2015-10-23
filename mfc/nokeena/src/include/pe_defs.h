#ifndef __PE_HTTP__H
#define __PE_HTTP__H

#include "tcl.h"

#define TOKEN_SOCKET_SOURCE_IP		"socket.source_ip"
#define TOKEN_SOCKET_SOURCE_PORT	"socket.source_port"
#define TOKEN_SOCKET_DEST_IP		"socket.dest_ip"
#define TOKEN_SOCKET_DEST_PORT		"socket.dest_port"

#define TOKEN_HTTP_REQ_HOST		"http.req.host"
#define TOKEN_HTTP_REQ_URI		"http.req.uri"
#define TOKEN_HTTP_REQ_COOKIE		"http.req.cookie"
#define TOKEN_HTTP_REQ_HEADER		"http.req.header"
#define TOKEN_HTTP_REQ_METHOD		"http.req.method"
#define TOKEN_HTTP_REQ_QUERY		"http.req.query"
#define TOKEN_HTTP_REQ_URL_CATEGORY	"http.req.url_category"

#define TOKEN_OM_RES_STATUS_CODE	"om.res.status_code"
#define TOKEN_OM_RES_HEADER		"om.res.header"

#define TOKEN_HTTP_RES_STATUS_CODE	"http.res.status_code"
#define TOKEN_HTTP_RES_HEADER		"http.res.header"

#define TOKEN_CALLOUT_STATUS_CODE	"callout.status_code"
#define TOKEN_CALLOUT_RESPONSE		"callout.response"
#define TOKEN_CALLOUT_SERVER		"callout.server"
#define TOKEN_GEO_STATUS_CODE		"socket.geodata.status_code"
#define TOKEN_GEO_CONTINENT_CODE	"socket.geodata.continent_code"
#define TOKEN_GEO_COUNTRY_CODE		"socket.geodata.country_code"
#define TOKEN_GEO_COUNTRY		"socket.geodata.country"
#define TOKEN_GEO_STATE			"socket.geodata.state"
#define TOKEN_GEO_CITY			"socket.geodata.city"
#define TOKEN_GEO_POSTAL_CODE		"socket.geodata.postal_code"
#define TOKEN_GEO_ISP			"socket.geodata.isp"


#define ACTION_CL_DEFALUT_ACTION	"cl.default_action"

#define ACTION_SET_IP_TOS		"set_ip_tos"
#define ACTION_REJECT_REQUEST		"reject_request"
#define ACTION_URL_REWRITE		"url_rewrite"
#define ACTION_REDIRECT			"redirect"
#define ACTION_CACHE_OBJECT		"cache_object"
#define ACTION_NO_CACHE_OBJECT		"no_cache_object"
#define ACTION_SET_ORIGIN_SERVER	"set_origin_server"
#define ACTION_INSERT_HEADER		"insert_header"
#define ACTION_REMOVE_HEADER		"remove_header"
#define ACTION_MODIFY_HEADER		"modify_header"
#define ACTION_SET_CACHE_NAME		"set_cache_name"
#define ACTION_CLUSTER_REDIRECT		"cluster_redirect"
#define ACTION_CLUSTER_PROXY		"cluster_proxy"
#define ACTION_CALLOUT			"callout"
#define ACTION_IGNORE_CALLOUT_RESP	"ignore_callout_response"
#define ACTION_CACHE_INDEX_APPEND	"cache_index_append"
#define ACTION_TRANSMIT_RATE		"transmit_rate"
#define ACTION_MODIFY_RESPONSE_CODE	"modify_response_code"

/* ACTIONS */
#define PE_ACTION_ERROR			0x80000000
#define PE_ACTION_NO_ACTION		0x00000000
#define PE_ACTION_REJECT_REQUEST	0x00000001
#define PE_ACTION_URL_REWRITE		0x00000002
#define PE_ACTION_REDIRECT		0x00000004
#define PE_ACTION_SET_IP_TOS		0x00000008
#define PE_ACTION_REQ_CACHE_OBJECT	0x00000010
#define PE_ACTION_RES_CACHE_OBJECT	0x00000020
#define PE_ACTION_REQ_NO_CACHE_OBJECT	0x00000040
#define PE_ACTION_RES_NO_CACHE_OBJECT	0x00000080
#define PE_ACTION_SET_ORIGIN_SERVER	0x00000100
#define PE_ACTION_INSERT_HEADER		0x00000200
#define PE_ACTION_REMOVE_HEADER		0x00000400
#define PE_ACTION_MODIFY_HEADER		0x00000800
#define PE_ACTION_SET_CACHE_NAME	0x00001000
#define PE_ACTION_CLUSTER_REDIRECT	0x00002000
#define PE_ACTION_CLUSTER_PROXY		0x00004000
#define PE_ACTION_CALLOUT		0x00008000
#define PE_ACTION_IGNORE_CALLOUT_RESP	0x00010000
/* Append the given string to cache name */
#define PE_ACTION_CACHE_INDEX_APPEND	0x00020000
/* Specify the bandwidth settings for this object. 
 * It would accept three parameters, maximum bandwidth, 
 * maximum bitrate for the video, and fast start time/size
 */
#define PE_ACTION_TRANSMIT_RATE		0x00040000
#define PE_ACTION_MODIFY_RESPONSE_CODE	0x00080000

#define PE_ACTION_CACHE_OBJECT		(PE_ACTION_REQ_CACHE_OBJECT | PE_ACTION_RES_CACHE_OBJECT)
#define PE_ACTION_NO_CACHE_OBJECT	(PE_ACTION_REQ_NO_CACHE_OBJECT | PE_ACTION_RES_NO_CACHE_OBJECT)

#define SET_ACTION(x, f) (x)->pe_action |= (f)
#define CLEAR_ACTION(x, f) (x)->pe_action &= ~(f)
#define CHECK_ACTION(x, f) ((x)->pe_action & (f))

#define PE_EXEC_HTTP_RECV_REQ		0x00000002
#define PE_EXEC_HTTP_SEND_RESP		0x00000004
#define PE_EXEC_OM_SEND_REQ		0x00000008
#define PE_EXEC_OM_RECV_RESP		0x00000010

#define PE_SET_FLAG(x, f) (x)->pe_flag |= (f)
#define PE_CLEAR_FLAG(x, f) (x)->pe_flag &= ~(f)
#define PE_CHECK_FLAG(x, f) ((x)->pe_flag & (f))


typedef struct pe_rules {
	Tcl_Obj * ruleobj; // pointing to pre-compiled rule script.
	Tcl_Interp *tip;
	struct pe_rules *next;
	uint32_t file_id;
	uint32_t pe_flag;
} pe_rules_t;

typedef struct pe_ilist {
	int pe_num_interp;
	int pe_num_free;
	pe_rules_t *p_rules_free_list;
	pthread_mutex_t pe_list_mutex;
	char *policy_buf;
	uint32_t file_id;
	uint32_t pe_flag;
} pe_ilist_t;

typedef struct pe_transmit_rate_args {
	char * pe_max_bw     ;
        char * pe_bit_rate    ;
        char * pe_fast_start ;
} pe_transmit_rate_args_t ;

typedef struct pe_transmit_rate_args_u64 {
	uint64_t max_bw        ;
        uint64_t bit_rate      ;
        uint64_t fast_start_sz ;
        int fast_start_unit_sec;
} pe_transmit_rate_args_u64_t ;

#ifndef PE_CHECK
pe_rules_t * pe_create_rule(policy_engine_config_t *pe_config, pe_ilist_t **pp_list);
#endif
void pe_free_rule(pe_rules_t * p_perule, pe_ilist_t *p_list);
pe_ilist_t * pe_create_ilist(void);
void pe_cleanup(void * plist);


uint64_t pe_eval_http_recv_request(pe_rules_t * p_perule, void * con);
uint64_t pe_eval_cl_http_recv_request(pe_rules_t * p_perule, void * con, int def_proxy);
uint64_t pe_eval_om_recv_response(pe_rules_t * p_perule, void * ocon);
uint64_t pe_eval_om_send_request(pe_rules_t * p_perule, void * ocon);
uint64_t pe_eval_http_send_response(pe_rules_t * p_perule, void *p_http, void *p_resp_hdr);
uint64_t pe_eval_http_recv_request_callout(pe_rules_t * p_perule, void * pcon);

#endif // __PE_HTTP__H
