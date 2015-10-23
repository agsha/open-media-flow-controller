/*
 * proto_http.h - External interface
 */

#ifndef _PROTO_HTTP_H
#define _PROTO_HTTP_H

typedef enum {
    PH_ALARM = 0,
    PH_SEVERE = 1,
    PH_ERROR = 2,
    PH_WARNING = 3,
    PH_MSG = 4,
    PH_MSG1 = 4,
    PH_MSG2 = 5,
    PH_MSG3 = 6,
} proto_http_log_level_t;

/*
 *****************************
 * Known header definitions  *
 *****************************
 */
typedef enum {
    H_ACCEPT = 0,
    H_ACCEPT_CHARSET,
    H_ACCEPT_ENCODING,
    H_ACCEPT_LANGUAGE,
    H_ACCEPT_RANGES,
    H_AGE,
    H_ALLOW,
    H_AUTHORIZATION,
    H_CACHE_CONTROL,
    H_CONNECTION,
    H_CONTENT_BASE,
    H_CONTENT_DISPOSITION,
    H_CONTENT_ENCODING,
    H_CONTENT_LANGUAGE,
    H_CONTENT_LENGTH,
    H_CONTENT_LOCATION,
    H_CONTENT_MD5,
    H_CONTENT_RANGE,
    H_CONTENT_TYPE,
    H_TE,
    H_TRANSFER_ENCODING,
    H_TRAILER,
    H_COOKIE,
    H_COOKIE2,
    H_DATE,
    H_ETAG,
    H_EXPIRES,
    H_FROM,
    H_HOST,
    H_IF_MATCH,
    H_IF_MODIFIED_SINCE,
    H_IF_NONE_MATCH,
    H_IF_RANGE,
    H_IF_UNMODIFIED_SINCE,
    H_LAST_MODIFIED,
    H_LINK,
    H_LOCATION,
    H_MAX_FORWARDS,
    H_MIME_VERSION,
    H_PRAGMA,
    H_PROXY_AUTHENTICATE,
    H_PROXY_AUTHENTICATION_INFO,
    H_PROXY_AUTHORIZATION,
    H_PROXY_CONNECTION,
    H_PUBLIC,
    H_RANGE,
    H_REQUEST_RANGE,
    H_REFERER,
    H_RETRY_AFTER,
    H_SERVER,
    H_SET_COOKIE,
    H_SET_COOKIE2,
    H_UPGRADE,
    H_USER_AGENT,
    H_VARY,
    H_VIA,
    H_EXPECT,
    H_WARNING,
    H_WWW_AUTHENTICATE,
    H_AUTHENTICATION_INFO,
    H_KEEP_ALIVE,
    /*
     **********************
     * Nokeena extensions *
     **********************
     */
    H_X_NKN_URI,		/* URI as received OTW */
    H_X_NKN_METHOD,	/* HTTP method as received OTW */
    H_X_NKN_QUERY,	/* HTTP query string as received OTW */
    H_X_NKN_RESPONSE_STR,/* HTTP response string */
    H_X_NKN_REMAPPED_URI,/* Target URI to use, remapped via SSP */
    H_X_NKN_DECODED_URI,	/* Decoded URI */
    H_X_NKN_SEEK_URI,	/* HDR_X_NKN_URI less the query param  */
    				/*   for seek */
    H_X_NKN_FP_SVRHOST,	/* Forward Proxy Server IP  */
    H_X_NKN_FP_SVRPORT,	/* Forward Proxy Server Port  */
    H_X_NKN_CLIENT_HOST,	/* Client Host header (used by OM) */
    H_X_NKN_REQ_DEST_IP,	/* Request dest IP (network byte order) */
    H_X_NKN_REQ_DEST_PORT,/* Request dest Port (network byte order) */
    H_X_NKN_ABS_URL_HOST,/* Host from absolute URL */
    H_X_NKN_REQ_LINE,	/* HTTP request line  (used by access log) */
    H_X_NKN_REQ_REAL_DEST_IP, /* Request dest IP picked from packet */
    H_X_NKN_REQ_REAL_DEST_PORT, /* Request dest port picked from packet*/
    H_X_NKN_REQ_REAL_SRC_IP,/* Requset src port picked from packet */
    H_X_NKN_REQ_REAL_SRC_PORT, /* Request src IP pciked from packet */
    H_X_NKN_ORIGIN_SVR,	/* Origin server set by PE */
    H_X_NKN_CACHE_POLICY,	/* Cache yes/no set by PE */
    H_X_NKN_CACHE_NAME,	/* Cache name set by PE for caching uri's with query */
    /*
     *********************************
     * Nokeena - Customer Extensions *
     *********************************
     */
    H_X_ACCEL_CACHE_CONTROL,
    H_X_LOCATION,
    H_X_REDIRECT_HOST,
    H_X_REDIRECT_PORT,
    H_X_REDIRECT_URI,

    /*
     **********************
     * Nokeena extensions *
     **********************
     */
    H_X_NKN_CL7_CACHEKEY_HOST, /* Added to request hdrs (host:port),
 				       * used by OSS_HTTP_PROXY_CONFIG */
    H_X_NKN_CL7_ORIGIN_HOST, /* Added to request hdrs (host:port),
    				     * used by OSS_HTTP_PROXY_CONFIG */
    H_X_NKN_CL7_PROXY, /* Added to request hdrs, pass Real ClientIP 
    			       * and DestIP:Port when proxying TProxy reqs */
    H_X_NKN_CL7_STATUS, /* Added to response hdrs,
    				* Cluster L7 request status for access.log */
    H_X_INTERNAL,
    H_X_NKN_PE_HOST_HDR,	/* Host header set by PE */
    H_X_NKN_INCLUDE_ORIG, /* Include original ip and port */
    H_X_NKN_ORIGIN_IP,    /* Origin Server IP */
    H_X_NKN_MD5_CHECKSUM, /* X-NKN-MD5-Checksum header */
    H_MAX_DEFS
    /*
     ***************************************************************************
     * Note: Always add to the end of list to prevent invalidating persistent
     *       cache attribute data.
     ***************************************************************************
     */
} ProtoHTTPHeaderId;

typedef void * token_data_t;

typedef struct proto_data {
    union {
	/*
    	 * Note: For the HTTP response case, 
	 *	 only "data" and "datalen" are required
	 */
        struct http {
	    const char *destIP;
	    uint16_t destPort; // Host byte order
	    const char *clientIP;
	    uint16_t clientPort; // Host byte
	    char *data;  // NULL terminated data
	    int datalen; // strlen(data), where (data[datalen]=='\0')
	} HTTP;
    } u;
} proto_data_t;

#define PROTO_HTTP_INTF_VERSION 1

/*
 * proto_http_init() options
 */
#define OPT_NONE		0
#define OPT_DETECT_GET_ATTACK	(1 << 0)

typedef struct proto_http_config {
    int interface_version; // Set to PROTO_HTTP_INTF_VERSION
    /*
     * All (*proc)() and value pointers args are optional, 
     * specify 0 to select the default.
     */
    uint64_t options;
    /*
     * Logging interface
     */
    int *log_level;
    int (*logfunc)(proto_http_log_level_t level, long module,
		   const char *fmt, ...);
    /*
     * Memory allocation interfaces
     */
    void *(*proc_malloc_type)(size_t size, int type);
    void *(*proc_realloc_type)(void *ptr, size_t size, int type);
    void *(*proc_calloc_type)(size_t nmemb, size_t size, int type);
    void *(*proc_memalign_type)(size_t align, size_t s, int type);
    void *(*proc_valloc_type)(size_t size, int type);
    void *(*proc_pvalloc_type)(size_t size, int type);
    char *(*proc_strdup_type)(const char *s, int type);
    void (*proc_free)(void *ptr);
    char pad[128];
} proto_http_config_t;

/*
 * proto_http_init() -- Subsystem initialization
 * 
 * Return:
 *  ==0, Success 
 *  !=0, Error
 */
int proto_http_init(const proto_http_config_t *cfg);

typedef enum {
    HP_SUCCESS = 0,
    HP_MORE_DATA = 1,
    HP_INVALID_INPUT,
    HP_MEMALLOC_ERR,
    HP_INIT_ERR,
    HP_INIT2_ERR,
    HP_PARSE_ERR,
    HP_INTERNAL_ERR
} HTTPProtoStatus;

/*
 * NewTokenData() -- Allocate initialized token_data_t
 *
 * Return:
 *  Success => token_data_t != 0
 *  Error   => token_data_t == 0
 */
token_data_t NewTokenData(void);

/*
 * HTTPProtoData2TokenData() -- Translate OTW HTTP request byte stream to 
 *				tokenized format.
 * 
 * Return:
 *  Success => (status == HP_SUCCESS) && (token_data_t != NULL)
 *  Error   => (status != HP_SUCCESS)
 */
token_data_t HTTPProtoData2TokenData(void *state_data, const proto_data_t *pd,
				     HTTPProtoStatus *status);

/*
 * HTTPProtoRespData2TokenData() -- Translate OTW HTTP response byte stream to 
 *				    tokenized format.
 * 
 * Return:
 *  Success => (status == HP_SUCCESS) && (token_data_t != NULL)
 *  Error   => (status != HP_SUCCESS)
 *
 *  http_respcode - HTTP response code
 */
token_data_t HTTPProtoRespData2TokenData(void *state_data, 
					 const proto_data_t *pd,
				     	 HTTPProtoStatus *status,
					 int *http_respcode);
/*
 * deleteHTTPtoken_data() -- Free token_data_t
 */
void deleteHTTPtoken_data(token_data_t td);

/*
 * HTTPHeaderBytes() -- Return total header bytes, includes "\r\n" denoting end
 *			of headers.
 */
int HTTPHeaderBytes(token_data_t td);

/*
 * HTTPisKeepAliveReq() -- Is a keepalive connection request?
 *
 *  Return:
 *   ==0, False
 *   !=0, True
 */
int HTTPisKeepAliveReq(token_data_t td);

/*
 * HTTPProtoReqOptions() -- Return HTTP protocol request options.
 *
 *  Return:
 *       Defined options bit mask (see PH_ROPT_XXX defines).
 */
#define PH_ROPT_HTTP_09		0x0800000000000000
#define PH_ROPT_HTTP_10		0x0000000000000001
#define PH_ROPT_HTTP_11		0x0000000000000002
#define PH_ROPT_METHOD_GET	0x0000000000100000
#define PH_ROPT_METHOD_POST	0x0000000000200000
#define PH_ROPT_METHOD_HEAD	0x0000000000400000
#define PH_ROPT_METHOD_CONNECT	0x0000000000800000
#define PH_ROPT_METHOD_PUT	0x0000000080000000
#define PH_ROPT_METHOD_DELETE	0x0000000100000000
#define PH_ROPT_METHOD_TRACE	0x0000000200000000
#define PH_ROPT_SCHEME_HTTPS	0x4000000000000000

uint64_t HTTPProtoReqOptions(token_data_t td);

/*
 * HTTPProtoSetReqOptions() -- Set HTTP protocol request options.
 */
void HTTPProtoSetReqOptions(token_data_t td, uint64_t options);

/*
 *******************************************************************************
 * Known HTTP header API
 *******************************************************************************
 */

/*
 * HTTPHdrStrToHdrToken() -- Pointer/Length data to known header token
 *  Return:
 *    >=0, Success
 *    < 0, Error
 */
ProtoHTTPHeaderId HTTPHdrStrToHdrToken(const char *str, int len);

/* 
 * HTTPAddKnownHeader() -- Add known HTTP header
 *
 *  request_or_response - 0=Request headers; !=0 Response headers
 *
 *  Return:
 *   ==0, Success
 *   !=0, Error
 */
int HTTPAddKnownHeader(token_data_t td, int request_or_response, 
		       ProtoHTTPHeaderId token, const char *val, int len);

/*
 * HTTPGetKnownHeader() -- Get known HTTP header
 *
 *  request_or_response - 0=Request headers; !=0 Response headers
 *
 *  Return:
 *   ==0, Success
 *   !=0, Error
 *
 *  header_cnt - number of values associated with multi value header
 */
int HTTPGetKnownHeader(token_data_t td, int request_or_response,
		       ProtoHTTPHeaderId token, const char **val, int *vallen,
		       int *header_cnt);

/*
 * HTTPGetNthKnownHeader() -- Get Nth value associated with multi value known
 *			      HTTP header.
 *  request_or_response - 0=Request headers; !=0 Response headers
 *
 *  Return:
 *   ==0, Success
 *   !=0, Error
 */
int HTTPGetNthKnownHeader(token_data_t td, int request_or_response,
		          ProtoHTTPHeaderId token, 
			  int header_num,  /* zero based */
			  const char **val, int *vallen);
/*
 * HTTPGetQueryArg() -- Get nth query string arg
 *
 *  Return:
 *   ==0, Success
 *   !=0, Error
 */
int HTTPGetQueryArg(token_data_t td, int arg_num,
		    const char **name, int *name_len,
		    const char **val, int *val_len);

/*
 * HTTPGetQueryArgbyName() -- Get nth query string arg by name
 *
 *  Return:
 *   ==0, Success
 *   !=0, Error
 */
int HTTPGetQueryArgbyName(token_data_t td, const char *name, int namelen,
			  const char **val, int *val_len);

/*
 * HTTPDeleteKnownHeader() -- Delete known HTTP header
 *
 *  request_or_response - 0=Request headers; !=0 Response headers
 *
 *  Return:
 *   ==0, Success
 *   !=0, Error
 */
int HTTPDeleteKnownHeader(token_data_t td, int request_or_response, 
		       	  ProtoHTTPHeaderId token);
/*
 *******************************************************************************
 * Unknown HTTP header API
 *******************************************************************************
 */

/*
 * HTTPAddUnknownHeader() -- Add unknown HTTP header
 *
 *  request_or_response - 0=Request headers; !=0 Response headers
 *
 *  Return:
 *   ==0, Success
 *   !=0, Error
 */
int HTTPAddUnknownHeader(token_data_t td, int request_or_response,
			 const char *name, int namelen,
			 const char *val, int vallen);

/*
 * HTTPGetUnknownHeader() -- Get unknown HTTP header
 *
 *  request_or_response - 0=Request headers; !=0 Response headers
 *
 *  Return:
 *   ==0, Success
 *   !=0, Error
 */
int HTTPGetUnknownHeader(token_data_t td, int request_or_response,
			 const char *name, int namelen, 
			 const char **val, int *vallen);

/*
 * HTTPGetNthUnknownHeader() -- Get Nth unknown HTTP header
 *
 *  request_or_response - 0=Request headers; !=0 Response headers
 *
 *  Return:
 *   ==0, Success
 *   !=0, Error
 */
int HTTPGetNthUnknownHeader(token_data_t td, int request_or_response,
			    int header_num, /* zero based */
			    const char **name, int *namelen,
			    const char **val, int *vallen);

/*
 * HTTPDeleteUnknownHeader() -- Delete unknown HTTP header
 *
 *  request_or_response - 0=Request headers; !=0 Response headers
 *
 *  Return:
 *   ==0, Success
 *   !=0, Error
 */
int HTTPDeleteUnknownHeader(token_data_t td, int request_or_response,
			    const char *name, int namelen);

/*
 *******************************************************************************
 * HTTP Request API
 *******************************************************************************
 */

/*
 * HTTPRequest() -- Build OTW HTTP request from the request headers
 *
 * Return:
 *  ==0, Success
 *  !=0, Error
 *
 *  buf - OTW data start
 *  buflen - OTW data length
 */
int HTTPRequest(token_data_t td, const char **buf, int *buflen);

/*
 *******************************************************************************
 * HTTP Response API
 *******************************************************************************
 */

/*
 * HTTPResponse() -- Build OTW HTTP response from the response headers
 *   and the given HTTP response code
 *
 * Return:
 *  ==0, Success
 *  !=0, Error
 *
 *  http_response_code -- Valid HTTP response code (e.g. 200), 
 *			  if (http_response_code < 0), avoid response 
 *			  generation and only return [buf, buflen]
 *  buf - OTW data start
 *  buflen - OTW data length
 *  cur_http_response_code - currently defined HTTP response code (optional)
 */
int HTTPResponse(token_data_t td, int http_response_code, 
		 const char **buf, int *buflen, int *cur_http_response_code);

/*
 *******************************************************************************
 * HTTP User Data Area (UDA) API
 *******************************************************************************
 */
#define PH_MAX_UDA_SZ 256 // Max size of UDA

 /*
  * HTTPWriteUDA() -- Write given bytes to UDA.
  *
  * Return:
  *   >=0, Success (bytes written to UDA)
  *   < 0, Error
  */
int HTTPWriteUDA(token_data_t td, char *buf, int buflen);

 /*
  * HTTPReadUDA() -- Read UDA bytes.
  *
  * Return:
  *   >=0, Success (bytes read from UDA)
  *   < 0, Error
  */
int HTTPReadUDA(token_data_t td, char *buf, int buflen);

/*
 *******************************************************************************
 * Conversion functions
 *******************************************************************************
 */

#define FL_2NV_SPDY_PROTO (1 << 0)
#define FL_2NV_HTTP2_PROTO (1 << 1)

/*
 * HTTPHdr2NV() -- Build NV (name, value) representation of HTTP headers
 *
 * Return:
 *  ==0, Success
 *  !=0, Error
 *
 * request_or_response - request=1; response=0
 * flags - see FL_2NV flags
 * buf - allocated buffer containing NV
 * buflen - allocated buffer byte count
 * nv_count - number of NV pairs
 *
 * Note: 
 *   1) Returned NV listed terminated with null NV entry.
 *   2) Caller frees the returned "buf" data via free()
 */
int HTTPHdr2NV(token_data_t td, int request_or_response, uint64_t flags,
	       char **buf, int *buflen, int *nv_count);

/*
 * HTTPHdr2NVdesc() -- Build NV_desc_t representation of HTTP headers
 *
 * Return:
 *  ==0, Success
 *  !=0, Error
 *
 * request_or_response - request=1; response=0
 * flags - see FL_2NV flags
 * buf - allocated buffer containing NV_desc_t list
 * buflen - allocated buffer byte count
 * nv_count - number of NV_desc_t elements
 *
 * Note: 
 *   1) Caller frees the returned "buf" data via free()
 */
typedef struct NV_desc {
    char *name;
    int namelen;
    char *val;
    int vallen;
} NV_desc_t;

int HTTPHdr2NVdesc(token_data_t td, int request_or_response, uint64_t flags,
		   NV_desc_t **buf, int *buflen, int *nv_count);

#endif /* _PROTO_HTTP_H */

/*
 * End of proto_http.h
 */
