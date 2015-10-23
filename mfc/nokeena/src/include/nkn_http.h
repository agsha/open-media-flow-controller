
#ifndef __NKN_HTTP__H__
#define __NKN_HTTP__H__

#include <stdint.h>
#include <arpa/inet.h>

#include "nkn_sched_api.h"
#include "cache_mgr.h"
#include "http_header.h"
#include "nkn_namespace.h"
#include "nkn_lockstat.h"
#include "nkn_ssp_cb.h"


#define OM_STAT_SIG_MEDIA_BLK_LEN	1048586
#define OM_STAT_SIG_TOT_CONTENT_LEN	0x7fffffffffffffff

//////////////////////////////////////////////////////////////////////////
// HTTP parser
//////////////////////////////////////////////////////////////////////////

typedef enum {
	HPS_OK=0,
	HPS_ERROR=1,
	HPS_NEED_MORE_DATA=2,
} HTTP_PARSE_STATUS ;
typedef enum {
	URI_REMAP_OK = 0,
	URI_REMAP_ERR = -1,
	URI_REMAP_HEX_DECODE_ERR = -2,
	URI_REMAP_TOKENIZE_ERR = -3,
} URI_REMAP_STATUS;

#define HEADER_CLUSTER_SPACES  4        // spaces before cache-index

#define TYPICAL_HTTP_CB_BUF 16384 
#define TYPICAL_HTTP_CB_BUF_OM 16384
#define TYPICAL_HTTP_CB_BUF_OM_TPROXY 8192

#define MAX_ONE_PACKET_SIZE 8192
#define MAX_HTTP_HEADER_SIZE 8192 

//
// All these flags should be the same as Accesslog flag which is defined in accesslog_pub.h
//
#define HRF_HTTP_10			0x0000000000000001
#define HRF_HTTP_11			0x0000000000000002
#define HRF_MFC_PROBE_REQ		0x0000000000000004
#define HRF_CONNECTION_KEEP_ALIVE	0x0000000000000008
#define HRF_FORWARD_PROXY           	0x0000000000000010
#define HRF_BYTE_RANGE			0x0000000000000020
//#define HRF_HTTP_RESP_200		0x0000000000000040
//#define HRF_HTTP_RESP_206		0x0000000000000080
#define HRF_HTTP_SET_IP_TOS		0x0000000000000040
#define HRF_TIER2_MODE			0x0000000000000080
#define HRF_TRANSCODE_CHUNKED		0x0000000000000100
#define HRF_RES_HEADER_SENT		0x0000000000000200
#define HRF_RES_SEND_TO_SSP		0x0000000000000400
#define HRF_TPROXY_MODE			0x0000000000000800
#define HRF_BYTE_SEEK			0x0000000000001000
#define HRF_NO_CACHE			0x0000000000002000
#define HRF_PREPEND_IOV_DATA		0x0000000000004000
#define HRF_APPEND_IOV_DATA		0x0000000000008000
#define HRF_MULTIPART_RESPONSE		0x0000000000010000
#define HRF_ZERO_DATA_ERR_RETURN	0x0000000000020000
#define HRF_TRACE_REQUEST		0x0000000000040000
#define HRF_LOCAL_GET_REQUEST           0x0000000000080000
#define HRF_METHOD_GET			0x0000000000100000
#define HRF_METHOD_POST			0x0000000000200000
#define HRF_METHOD_HEAD           	0x0000000000400000
#define HRF_METHOD_CONNECT           	0x0000000000800000
#define HRF_METHOD_OPTIONS           	0x0000000001000000
#define HRF_SUPPRESS_SEND_DATA          0x0000000002000000
#define HRF_CONTENT_LENGTH          	0x0000000004000000
#define HRF_HAS_QUERY			0x0000000008000000
#define HRF_MULTI_BYTE_RANGE		0x0000000010000000
#define HRF_REVALIDATE_CACHE		0x0000000020000000
#define HRF_HAS_HEX_ENCODE		0x0000000040000000
#define HRF_METHOD_PUT           	0x0000000080000000
#define HRF_METHOD_DELETE           	0x0000000100000000
#define HRF_METHOD_TRACE           	0x0000000200000000
#define HRF_EXPIRE_RESTART_DONE		0x0000000400000000
#define HRF_EXPIRE_RESTART_2_DONE	0x0000000800000000
#define HRF_SSP_CONFIGURED		0x0000001000000000
#define HRF_ONE_PACKET_SENT		0x0000002000000000
#define HRF_SSP_SF_RESPONSE		0x0000004000000000
#define HRF_SSP_CACHE_ONLY_FETCH	0x0000008000000000
#define HRF_STREAMING_DATA		0x0000010000000000
#define HRF_BYTE_RANGE_BM		0x0000020000000000
#define HRF_BYTE_RANGE_HM		0x0000040000000000
#define HRF_302_REDIRECT		0x0000080000000000
#define HRF_RETURN_REVAL_CACHE          0x0000100000000000
#define HRF_RP_SESSION_ALLOCATED	0x0000200000000000
#define HRF_SSP_NO_AM_HITS		0x0000400000000000
#define HRF_BYTE_RANGE_START_STOP	0x0000800000000000
#define HRF_PARSE_DONE			0x0001000000000000
#define HRF_SEP_REQUEST			0x0002000000000000
#define HRF_100_CONTINUE		0x0004000000000000
#define HRF_CL_L7_PROXY_REQUEST		0x0008000000000000
#define HRF_CL_L7_PROXY_LOCAL		0x0010000000000000
#define HRF_DYNAMIC_URI			0x0020000000000000
#define HRF_CACHE_COOKIE		0x0040000000000000
#define HRF_SSP_SEEK_TUNNEL		0x0080000000000000
#define HRF_CLIENT_INTERNAL		0x0100000000000000
#define HRF_RESP_INDEX_CHUNK_REFETCH	0x0200000000000000
#define HRF_PE_SET_IP_TOS		0x0400000000000000
#define HRF_HTTP_09                     0x0800000000000000
#define HRF_HTTP_RESPONSE		0x1000000000000000
#define HRF_SSP_INTERNAL_REQ		0x2000000000000000
#define HRF_HTTPS_REQ			0x4000000000000000
#define HRF_WWW_AUTHENTICATE_NTLM	0x8000000000000000

/* 2nd set of flags for http parsing. 
 * NOTE: Add all the new flags with name starting HRF2_* 
 */
#define HRF2_TRANSCODE_CHUNKED_WITH_SUB	0x0000000000000001
#define HRF2_EXPIRED_OBJ_DEL		0x0000000000000002
#define HRF2_EXPIRED_OBJ_ENT		0x0000000000000004
#define HRF2_VARY_REQUEST		0x0000000000000008
#define HRF2_HAS_VARY			0x0000000000000010
#define HRF2_HAS_VARY_USR_AGNT		0x0000000000000020
#define HRF2_HAS_VARY_ACCPT_ENCD	0x0000000000000040
#define HRF2_HAS_VARY_OTHERS		0x0000000000000080
#define HRF2_CONNECTION_UPGRADE		0x0000000000000100
#define HRF2_UPGRADE_WEBSOCKET		0x0000000000000200
#define HRF2_QUERY_STRING_REMOVED_URI	0x0000000000000040
#define HRF2_RBCI_URI			0x0000000000000080



#define HRF_HTTP_METHODS 		(HRF_METHOD_GET|HRF_METHOD_POST|\
					 HRF_METHOD_HEAD|HRF_METHOD_CONNECT|\
					 HRF_METHOD_OPTIONS|HRF_METHOD_PUT|\
					 HRF_METHOD_DELETE|HRF_METHOD_TRACE)

#define HRF_ENCODING_IDENTITY		0x0000
#define HRF_ENCODING_GZIP		0x0001
#define HRF_ENCODING_DEFLATE		0x0002
#define HRF_ENCODING_COMPRESS		0x0004
#define HRF_ENCODING_EXI		0x0008
#define HRF_ENCODING_PACK200_GZIP	0x0010
#define HRF_ENCODING_SDCH		0x0020
#define HRF_ENCODING_BZIP2		0x0040
#define HRF_ENCODING_PEERDIST		0x0080
#define HRF_ENCODING_ALL		0x7fff
#define HRF_ENCODING_UNKNOWN		0x8000

#define SET_HTTP_FLAG(x, f)  (x)->flag |= (f)
#define CLEAR_HTTP_FLAG(x, f) (x)->flag &= ~(f)
#define CHECK_HTTP_FLAG(x, f) ( ((x)->flag & (f)) == (f) )

#define SET_HTTP_FLAG2(x, f)  (x)->flags2 |= (f)
#define CLEAR_HTTP_FLAG2(x, f) (x)->flags2 &= ~(f)
#define CHECK_HTTP_FLAG2(x, f) ( ((x)->flags2 & (f)) == (f) )

#define CHECK_HTTP_RESPONSE_FLAG(x, f) ((x)->response_flags & (f))

// Following flags and macros SET/CHECK/CLEAR are related to URI 
// tokenization.
#define UTF_FLVSEEK_BYTE_START	0x0001
#define UTF_HTTP_RANGE_END		0x0002
#define UTF_HTTP_RANGE_START	0x0004

#define SET_UTF_FLAG(x, f)  (x)->token_flag |= (f)
#define CHECK_UTF_FLAG(x, f) ( ((x)->token_flag & (f)) == (f) )
#define CLEAR_UTF_FLAG(x, f) (x)->token_flag &= ~(f)

#define SET_CRAWL_REQ(x) (x)->crawl_req = 1
#define CHECK_CRAWL_REQ(x) ((x)->crawl_req == 1)

#define MAX_NAME_LENGTH		30

#define HTTP_PAIR_REQ	1
#define HTTP_PAIR_RES	2
#define MAX_PORT_STR_LEN 5      // maximum 65535

#define MD5_DIGEST_STR_LENGTH (MD5_DIGEST_LENGTH * 2)

#define MAX_PREPEND_CONTENT_IOVECS	1
#define MAX_APPEND_CONTENT_IOVECS	1
#define MAX_CONTENT_IOVECS		64
#define MAX_HTTP_METHODS_ALLOWED	16
#define CONTENT_IOVECS	(MAX_PREPEND_CONTENT_IOVECS +  \
			MAX_APPEND_CONTENT_IOVECS + \
			MAX_CONTENT_IOVECS)

#define HIT_HISTORY_SIZE	1024
typedef struct http_control_block {
	uint64_t flag;
	uint64_t flags2; /* 'flag' set is full. adding this for future HTTP flags */
	uint16_t http_encoding_type; //request or response encoding type
	uint16_t cache_encoding_type; //cached object encoding type
	nkn_cod_t req_cod; 	// COD associated with request URI


	// Buffer used to store request header, URI, responsei header, etc
        char * cb_buf;
	int32_t cb_max_buf_size; // cb_buf size
        int32_t cb_totlen;		// total this length data in cb_buf
        int32_t cb_offsetlen;	// so many offset data has been processed
	off_t cb_bodyoffset;	// offset to body start in initial cb
	int32_t chunklen;		// for check response.
	int32_t http_hdr_len;   // Total HTTP header length
	ip_addr_t remote_ipv4v6;   	// this server IP address returns the response.
	int16_t remote_port;   	// this server port returns the response.
	uint16_t remote_sock_port; //this is from getsockname()

	// HTTP header data
	mime_header_t hdr;
	mime_header_t * p_resp_hdr; // make a copy of response header for accesslog
	nkn_attr_t * attr;	// returned attribute from BM

	//
	// request information
	//
	int req_max_age;	// max_age value in req Cache-Control: max-age=xxx
	off_t brstart, brstop; 	// byte range:
	off_t seekstart, seekstop; // internal byte seek
	struct timeval start_ts;	// start time of the HTTP Transaction
	struct timeval end_ts;		// end time of the HTTP Transaction
	struct timeval ttfb_ts;		// time to first byte
	const namespace_config_t *nsconf;
	namespace_token_t ns_token;
	namespace_token_t cl_l7_ns_token; // CL L7 proxy req namespace
	namespace_token_t cl_l7_node_ns_token; // CL L7 proxy node namespace

	//
	// response information
	//
        int32_t res_hdlen;		// response header length
	char *resp_buf;
	int resp_buflen;
	const char *resp_body_buf;	// inline 200 response body 
	int resp_body_buflen;		// < 0 => non-malloc'ed data
        off_t content_length;	// The HTTP transaction content length

	nkn_iovec_t content[CONTENT_IOVECS]; // actual content, returned from CM
	int32_t num_nkn_iovecs;
	char *prepend_content_data; // != free() prepend data
	int prepend_content_size;
	char *append_content_data; // != free() append data
	int append_content_size; 
        off_t tot_reslen;	// Total response data size in the iovece
	off_t pseudo_content_length; // use this if HRF_MULTIPART_RESPONSE
	nkn_provider_type_t provider; // remember the provider.

	int32_t respcode, subcode, tunnel_reason_code;	// response HTTP code
	uint64_t response_flags;        // HTTP flags set in the response path.
	time_t obj_create;
	time_t obj_expiry;
	int timeout_called;		// For disappearing client timeout issue.
	int redirect_count;
	int nfs_tunnel;                  // Do extra check for NFS origin
	int crawl_req;

	// Status counter
	uint64_t size_from_origin;
	char * hit_history;
	int hit_history_used;
	uint64_t object_hotval;
	uint64_t first_provider;
	uint64_t last_provider;
	uint64_t last_provider_size_from;

	// SSP definition
	nkn_ssp_params_t *p_ssp_cb; // Back reference to ssp control block. This is only used by SSP
	nkn_uri_token_t *uri_token;
	int tos_opt;
} http_cb_t;


typedef struct http_method_types {

	nkn_mutex_t cfg_mutex;
	int32_t  method_cnt;
	int32_t all_methods;
	struct {
		int32_t allowed;	// This Method Type is allowed
		int32_t method_len;		// Request Type Len
		char	*name;	// Request Type string
	} method[MAX_HTTP_METHODS_ALLOWED];

} http_method_types_t;

typedef struct nkn_custom_token {
	long flvseek_byte_start;
	long http_range_start;
	long http_range_end;
} nkn_custom_token_t;

typedef enum {
        HTTP_ORIG_HEADER_ERR = 0,
        HTTP_ORIG_HEADER_PRESENT,
        HTTP_ORIG_HEADER_NOT_PRESENT,
} http_orig_header_ret_t;

extern http_method_types_t http_allowed_methods;

//extern http_method_types_t http_allowed_methods[MAX_HTTP_METHODS_ALLOWED];
//extern int http_allow_all_methods;
//extern int http_allowed_method_cnt;
int nkn_check_http_cb_buf(http_cb_t * phttp);
int nkn_http_init(void);
HTTP_PARSE_STATUS http_parse_header(http_cb_t * phttp);
int http_parse_uri(http_cb_t * phttp, const char * p);
int http_parse_uri_internal(http_cb_t * phttp, const char * p, int pe_rewrite_uri);

int http_get_host_and_uri_from_bad_req(http_cb_t *phttp);
int http_check_request(http_cb_t * phttp);
int http_check_mfd_limitation(http_cb_t * phttp, uint64_t pe_action);

int http_build_res_200(http_cb_t * phttp);
int http_build_res_200_ext(http_cb_t * phttp, mime_header_t * presponse_hdr);
int http_build_res_206(http_cb_t * phttp);
int http_build_res_301(http_cb_t * phttp, int errcode, mime_header_t * presponse_hdr);
int http_build_res_302(http_cb_t * phttp, int errcode, mime_header_t * presponse_hdr);
int http_build_res_304(http_cb_t * phttp, int errcode, mime_header_t * presponse_hdr);
int http_build_res_400(http_cb_t * phttp, int errcode);
int http_build_res_403(http_cb_t * phttp, int errcode);
int http_build_res_404(http_cb_t * phttp, int errcode);
int http_build_res_405(http_cb_t * phttp, int errcode);
int http_build_res_413(http_cb_t * phttp, int errcode);
int http_build_res_414(http_cb_t * phttp, int errcode);
int http_build_res_416(http_cb_t * phttp, int errcode);
int http_build_res_417(http_cb_t * phttp, int errcode);
int http_build_res_500(http_cb_t * phttp, int errcode);
int http_build_res_501(http_cb_t * phttp, int errcode);
int http_build_res_502(http_cb_t * phttp, int errcode);
int http_build_res_503(http_cb_t * phttp, int errcode);
int http_build_res_504(http_cb_t * phttp, int errcode);
int http_build_res_505(http_cb_t * phttp, int errcode);
int http_build_res_unsuccessful(http_cb_t * phttp, int response);

void add_namevalue_pair(http_cb_t * phttp, unsigned char req_or_res, const char * name, const char * value);
void free_namevalue_pair(http_cb_t * phttp);

// Returns 0 on success
int http_set_seek_request(http_cb_t *cb, off_t seek_start, off_t seek_end);

// Returns 0 on success
int http_set_no_cache_request(http_cb_t *cb);

// Returns 0 on success
int http_set_revalidate_cache_request(http_cb_t *cb);

// Returns 0 on success
int http_prepend_response_data(http_cb_t *cb, const char *data, int datalen, 
			int free_malloced_data);
// Returns 0 on success
int http_append_response_data(http_cb_t *cb, const char *data, int datalen, 
			int free_malloced_data);
// Returns 0 on success
int http_set_multipart_response(http_cb_t *cb, off_t pseudo_content_length);

// Returns 0 on success
int http_set_local_get_request(http_cb_t *cb);

extern int nkn_http_is_tproxy_cluster(http_config_t *http_config_p);

#endif // __NKN_HTTP__H__

