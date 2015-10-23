/*
 * proto_http_int.h -- Internal library definitions
 */
#ifndef _PROTO_HTTP_INT_H
#define _PROTO_HTTP_INT_H

#include <sys/types.h>
#include <stdint.h>
#include "http_header.h"
#include "proto_http/proto_http.h"

#define TRUE 1
#define FALSE 0

typedef enum boolean {
    NKN_TRUE = 1,
    NKN_FALSE = 0
} boolean;

enum dlevel_enum {
    ALARM = 0,
    SEVERE = 1,
    ERROR = 2,
    WARNING = 3,
    MSG = 4,
    MSG1 = 4,
    MSG2 = 5,
    MSG3 = 6,
};

#define http_build_res_common proto_http_build_res_common

#define myhostname proto_http_myhostname
extern char proto_http_myhostname[1024];

#define myhostname_len proto_http_myhostname_len
extern int proto_http_myhostname_len;

#define max_get_rate_limit proto_http_max_get_rate_limit
extern int proto_http_max_get_rate_limit;

#define response_200_OK_body proto_http_response_200_OK_body
extern const char *proto_http_response_200_OK_body;

#define sizeof_response_200_OK_body proto_http_sizeof_response_200_OK_body
extern int proto_http_sizeof_response_200_OK_body;

extern int *phttp_log_level;
extern int (*phttp_dbg_log)(int, long, const char *, ...);

#define MOD_HTTP     0x0000000000000002
#define MOD_HTTPHDRS 0x0000000000000100
#define MOD_OM       0x0000000000001000

#define DBG_LOG(_dlevel, _dmodule, _fmt, ...) { \
	if ((_dlevel) <= *phttp_log_level) { \
	    (*phttp_dbg_log)((_dlevel), (_dmodule), (_fmt), ##__VA_ARGS__); \
	} \
}
#define DBG_TRACE(_fmt, ...)

#define UNUSED_ARGUMENT(x) (void)x

#define NKNCNT_DEF( _name, _type, _unit, _comment) \
	_type glob_##_name = 0; \
	const char * gunit_##_name = _unit; \
	const char * gcomment_##_name = _comment;

#define NS_INCR_STATS(_nsc, _proto, _type, _name)

#define NKN_EXPIRY_FOREVER (-1)

#define MAX_URI_HDR_HOST_SIZE 128
// /{name max 16}:XXXXXXXX (max 8) _YYYYYYYY (host max 128):XXXXX (port max 5)
#define MAX_URI_HDR_SIZE (1+16+1+8+1+MAX_URI_HDR_HOST_SIZE+1+5)
#define MAX_URI_SIZE (MAX_URI_HDR_SIZE+512)
#define MAX_HTTP_HEADER_SIZE 8192

// http_cb_t.flag definitions

#define HRF_HTTP_10			0x0000000000000001
#define HRF_HTTP_11			0x0000000000000002
#define HRF_MFC_PROBE_REQ		0x0000000000000004
#define HRF_CONNECTION_KEEP_ALIVE	0x0000000000000008
#define HRF_FORWARD_PROXY           	0x0000000000000010
#define HRF_BYTE_RANGE			0x0000000000000020
//#define HRF_HTTP_RESP_200		0x0000000000000040
//#define HRF_HTTP_RESP_206		0x0000000000000080
#define HRF_HTTP_SET_IP_TOS		0x0000000000000040
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
#define HRF_WWW_AUTHENTICATE		0x8000000000000000

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
#define CHECK_HTTP_RESPONSE_FLAG(x, f) ((x)->response_flags & (f))

#define SET_CRAWL_REQ(x) (x)->crawl_req = 1
#define CHECK_CRAWL_REQ(x) ((x)->crawl_req == 1)

typedef enum {
    Unknown_provider = 0,
    SolidStateMgr_provider = 1,
    FlashMgr_provider = 2,
    BufferCache_provider = 3,
    SASDiskMgr_provider = 4,
    SAS2DiskMgr_provider = 5,
    SATADiskMgr_provider = 6,
    NKN_MM_max_pci_providers = 7,
    Peer_provider = 10,
    TFMMgr_provider = 11,
    Tunnel_provider = 12, /* not registered in MM, used for accesslog only */
    RTSPMgr_provider = 17,
    NKN_MM_origin_providers = 18,
    NFSMgr_provider = 19,
    OriginMgr_provider = 20, /* Origin Manager should always be LAST */
    NKN_MM_MAX_CACHE_PROVIDERS = 21,
} nkn_provider_type_t;

#define NKN_SSP_SMOOTHSTREAM_STREAMTYPE_MANIFEST (1)
#define NKN_SSP_SMOOTHSTREAM_STREAMTYPE_VIDEO    (2) //video/mp4
#define NKN_SSP_SMOOTHSTREAM_STREAMTYPE_AUDIO    (3) //audio/mp4
#define NKN_SSP_FLASHSTREAM_STREAMTYPE_FRAGMENT  (4) //video/f4f


typedef struct policy_engine_config {
    char                *policy_file;
    int                 update;
    void                *policy_data;
} policy_engine_config_t;

typedef struct nkn_ssp_params {
    uint16_t ssp_client_id;
    uint8_t  ssp_streamtype;
} nkn_ssp_params_t;

typedef enum {
    ACT_ADD=1,
    ACT_DELETE,
    ACT_REPLACE
} header_action_t;

typedef struct header_descriptor {
    header_action_t action;
    char *name;
    int name_strlen;
    char *value;
    int value_strlen;
} header_descriptor_t;

typedef struct cache_redirect {
    int hdr_len;
    char *hdr_name;
} cache_redirect_t;

typedef enum {
    OSS_UNDEFINED = 0,
    OSS_HTTP_CONFIG,
    OSS_HTTP_ABS_URL,
    OSS_HTTP_FOLLOW_HEADER,
    OSS_HTTP_DEST_IP,
    OSS_HTTP_SERVER_MAP,
    OSS_NFS_CONFIG,
    OSS_NFS_SERVER_MAP,
    OSS_RTSP_CONFIG,
    OSS_RTSP_DEST_IP,
    OSS_HTTP_NODE_MAP,
    OSS_HTTP_PROXY_CONFIG, // Origin for Cluster L7 Proxy node
    OSS_MAX_ENTRIES // Always last
} origin_server_select_method_t;

typedef struct origin_server {
    origin_server_select_method_t select_method;
    union {
        struct OS_http {
	    boolean use_client_ip;
	} http;
    } u;
} origin_server_t;

typedef struct origin_server_policies {
    int modify_date_header;
    cache_redirect_t cache_redirect;
} origin_server_policies_t;

typedef struct http_config {
    origin_server_t origin;
    origin_server_policies_t policies;
    int num_delete_response_headers;
    header_descriptor_t *delete_response_headers;
    int num_add_response_headers;
    header_descriptor_t *add_response_headers;
    uint32_t retry_after_time;
    policy_engine_config_t policy_engine_config;
} http_config_t;

struct ns_accesslog_config {
    int32_t al_resp_header_configured;
};

typedef struct namespace_config {
    http_config_t *http_config;
    struct ns_accesslog_config *acclog_config;
} namespace_config_t;

typedef struct http_control_block {
    uint64_t flag;
    const namespace_config_t *nsconf;
    uint16_t http_encoding_type;

    char *cb_buf;
    int32_t cb_max_buf_size; // cb_buf size
    int32_t cb_totlen;       // total bytes in cb_buf
    int32_t cb_offsetlen;    // offset data processed
    off_t cb_bodyoffset;     // offset to body start in initial cb
    int32_t http_hdr_len;    // total HTTP header length
    int16_t remote_port;     // request server port

    mime_header_t hdr;
    mime_header_t resphdr;
    mime_header_t *p_resp_hdr;
    int *attr;

    off_t brstart, brstop;  // byte range
    off_t seekstart, seekstop; // internal byte seek

    int32_t res_hdlen;	    // response header length
    char *resp_buf;
    int resp_buflen;
    off_t content_length;   // The HTTP transaction content length
    int prepend_content_size;
    int append_content_size;
    off_t tot_reslen;       // Total response data size in the iovece
    off_t pseudo_content_length; // use this if HRF_MULTIPART_RESPONSE
    nkn_provider_type_t provider; // remember the provider
    int32_t respcode, subcode, tunnel_reason_code;  // response HTTP code
    uint64_t response_flags;
    time_t obj_create;

    int crawl_req;

    // SSP definition
    nkn_ssp_params_t *p_ssp_cb;

    // UDA bytes
    int valid_uda_bytes;
    char uda[PH_MAX_UDA_SZ];

    // Request OTW generation
    char *req_cb_buf;
    int32_t req_cb_max_buf_size;
    int32_t req_cb_totlen;
    char http_status_str[32];
} http_cb_t;

typedef enum {
    HPS_OK=0,
    HPS_ERROR=1,
    HPS_NEED_MORE_DATA=2,
} HTTP_PARSE_STATUS;

char *nkn_get_datestr(int *datestr_len);
HTTP_PARSE_STATUS http_parse_header(http_cb_t *phttp);
int http_build_res_200(http_cb_t *phttp);
int http_build_res_206(http_cb_t *phttp);
int http_build_res_302(http_cb_t *phttp, int errcode, 
		       mime_header_t *presponse_hdr);
int http_build_res_304(http_cb_t *phttp, int errcode,
		       mime_header_t *presponse_hdr);
int http_build_res_400(http_cb_t *phttp, int errcode);
int http_build_res_403(http_cb_t *phttp, int errcode);
int http_build_res_404(http_cb_t *phttp, int errcode);
int http_build_res_405(http_cb_t *phttp, int errcode);
int http_build_res_413(http_cb_t *phttp, int errcode);
int http_build_res_414(http_cb_t *phttp, int errcode);
int http_build_res_416(http_cb_t *phttp, int errcode);
int http_build_res_417(http_cb_t *phttp, int errcode);
int http_build_res_500(http_cb_t *phttp, int errcode);
int http_build_res_501(http_cb_t *phttp, int errcode);
int http_build_res_502(http_cb_t *phttp, int errcode);
int http_build_res_503(http_cb_t *phttp, int errcode);
int http_build_res_504(http_cb_t *phttp, int errcode);
int http_build_res_505(http_cb_t *phttp, int errcode);

int build_http_request(http_cb_t *cb);

#endif /* _PROTO_HTTP_INT_H */

/*
 * End of proto_http_int.h
 */
