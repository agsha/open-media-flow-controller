#ifndef NKN_TUNDEFS_H
#define NKN_TUNDEFS_H

// TR: Tunnel Reason
#define NKN_TR_UNKNOWN			    0  //Unknown reasons, validate failed or other complicated paths

//Reason code (1-63) for taking tunnel in request path
#define NKN_TR_REQ_BAD_REQUEST              1  //It's a bad request from client which can be tunneled
#define NKN_TR_REQ_ABS_URL_NOT_CACHEABLE    2  //Origin Selection method is Abs URL, but request is not cacheable
#define NKN_TR_REQ_NO_HOST                  3  //There is no Host header
#define NKN_TR_REQ_POLICY_NO_CACHE          4  //PE set to don't cache
#define NKN_TR_REQ_SSP_FORCE_TUNNEL         5  //SSP set to force tunnel
#define NKN_TR_REQ_GET_REQ_COD_ERR          6  //Request cod returned NULL(hostname>128,port_len>5,err creating COD)
#define NKN_TR_REQ_BIG_URI                  7  //URI size greater than 512 bytes
#define NKN_TR_REQ_NOT_GET                  8  //Neither GET, nor HEAD requests
#define NKN_TR_REQ_UNSUPP_CONT_TYPE         9  //Accept content-type does not match the request
#define NKN_TR_REQ_TUNNEL_ONLY              10 //Force tunnel for all requests
#define NKN_TR_REQ_CONT_LEN                 11 //request has content length and not zero
#define NKN_TR_REQ_MULT_BYTE_RANGE          12 //multi-byte mime header exists in request
#define NKN_TR_REQ_HDR_LEN                  13 //Some data exists in addition to http header
#define NKN_TR_REQ_AUTH_HDR                 14 //Authorization header present
#define NKN_TR_REQ_COOKIE                   15 //cookie header present
#define NKN_TR_REQ_QUERY_STR                16 //request has query parameters
#define NKN_TR_REQ_CC_NO_CACHE              17 //request has cache-control set to no-cache
#define NKN_TR_REQ_PRAGMA_NO_CACHE          18 //request has pragma set to no-cache
#define NKN_TR_REQ_CHUNK_ENCODED            19 //request has transfer encoding chunked
#define NKN_TR_REQ_HTTP_BAD_HOST_HEADER     20 //Host exists but its not valid FQDN
#define NKN_TR_REQ_HTTP_BAD_URL_HOST_HEADER 21 //Host in absolute URI is not valid FQDN
#define NKN_TR_REQ_HTTP_URI                 22 //URI does not exist in the request
#define NKN_TR_REQ_HTTP_REQ_RANGE1          23 //Byte range request, negative
#define NKN_TR_REQ_HTTP_UNSUPP_VER          24 //HTTP version not 1.0 or 1.1
#define NKN_TR_REQ_HTTP_NO_HOST_HTTP11      25 //HTTP version 1.1 without Host header
#define NKN_TR_REQ_HTTP_ERR_CHUNKED_REQ     26 //Transfer type encoding request
#define NKN_TR_REQ_HTTP_REQ_RANGE2          27 //Byte range request, stop range < start range
#define NKN_TR_REQ_DYNAMIC_URI_REGEX_FAIL   28 //Dynamic uri regex match fail
#define NKN_TR_REQ_MAX_SIZE_LIMIT	    29 // Client request size is more than the configured one
#define NKN_TR_REQ_NOT_SUPPORTED_ENCODING   30 //request encoding type is not supported
#define NKN_TR_REQ_INCAPABLE_BR_ORIGIN	    31 //request tproxy incapable byte range origin tunnel 
#define NKN_TR_REQ_CACHEPATH_NOTREADY	    32 //cachepath is not ready to accept incoming requests
#define NKN_TR_REQ_VARY_USER_AGENT_MISMATCH 33 //cachepath is not ready to accept incoming requests

//Reason code (65-127) for taking tunnel in response code path
#define NKN_TR_RES_POLICY_NO_CACHE      65 //PE set to don't cache
#define NKN_TR_RES_QUERY_STR            66 //URI with query string and disable cache on query is configured
#define NKN_TR_RES_RANGE_CHUNKED        67 //both transfer-encoding: byte-range and transfer_encoding: chunked exist
#define NKN_TR_RES_NO_CONTLEN_NO_CHUNK  68 //Neither content_length nor transfer_encoding: chunked exist
#define NKN_TR_RES_CONTLEN_ZERO         69 //content_length exists but it is zero
#define NKN_TR_RES_HEX_ENCODED          70 //uri has unprintable characters and virtual player is not configured
#define NKN_TR_RES_SSP_CONFIG_ERR       71 //server returns 404 and SSP is not configured
#define NKN_TR_RES_UNSUPP_RESP          72 //unsupported response from the server(not 200, 404, 100 etc.)
#define NKN_TR_RES_NO_302_NOT_RPROXY    73 //handle 302 is not configured and its not reverse proxy
#define NKN_TR_RES_NO_LOC_HDR           74 //302 response without location header
#define NKN_TR_RES_LOC_HDR_ERR          75 //302 response and location header has errors
#define NKN_TR_RES_COD_EXPIRED          76 //COD verification failed
#define NKN_TR_RES_CC_NO_TRANSFORM      77 //Cache control set to no transform
#define NKN_TR_RES_SAVE_VAL_ST_ERR      78 //Failed to save request state for validate
#define NKN_TR_RES_SAVE_MIME_HDR_ERR    79 //Failed to save mime header
#define NKN_TR_RES_MIME_HDR_TO_BUF_ERR  80 //Failed to serialize ocon mime_header_t into attribute buffer
#define NKN_TR_RES_CONT_RANGE_ERR       81 //error in getting content range from response
#define NKN_TR_RES_INVALID_CONT_RANGE   82 //Response has invalid content range data
#define NKN_TR_RES_NO_CONT_LEN          83 //No content length
#define NKN_TR_RES_RANGE_OFFSET_ERR     84 //received OS data offset not same as request offset
#define NKN_TR_RES_CC_PRIVATE           85 //response has Cache control set to private
#define NKN_TR_RES_SET_COOKIE           86 //response has set-cookie
#define NKN_TR_RES_OBJ_EXPIRED          87 //Object already expired, caching disabled
#define NKN_TR_RES_ADD_NKN_QUERY_ERR    88 //Adding query to attributes failed
#define NKN_TR_RES_CACHE_SIZE_LIMIT     89 //Cap of Cache Object Size
#define NKN_TR_RES_NON_CACHEABLE	90 //Non cacheable, because it has some ache control directive
#define NKN_TR_RES_NOT_IN_CACHE		91 //Object not found in cache
#define NKN_TR_RES_CHUNKED_EXP		92 //Chunked response, but cache-age is less than what's configured 
#define NKN_TR_RES_NOT_SUPPORTED_ENCODING       93 //response encoding type is not supported
#define NKN_TR_RES_CACHE_IDX_NO_HEADER  94 //Cache index based response has no configured header value in reply
#define NKN_TR_RES_CACHE_IDX_URI_MAX_SIZE_LIMIT 95 //Cache index based response uri exceeds maximum uri size
#define NKN_TR_RES_CACHE_IDX_OUT_OF_RANGE_DATA	96 //Cache index based response has out of range data bytes
#define NKN_TR_RES_DYN_URI_TOKENIZE_ERR	97 // Dynamic URI tokenization error.
#define NKN_TR_RES_CACHE_IDX_OBJ_CKSUM_CHUNKED 98 //Cache index response data configured for chunked object
#define NKN_TR_REQ_HTTP_09_REQUEST	99 // HTTP 0.9 requests
#define NKN_TR_RES_WWW_AUTHENTICATE_NTLM	100 // WWW-Authentication: NTLM needs connection level tunnel
#define NKN_TR_REQ_INT_CRAWL_REQ	101 // Internal crawl request
#define NKN_TR_RES_MAX_SIZE_LIMIT	102 // Server response size is more than the configured one
#define NKN_TR_RES_SUB_ENCODED_CHUNKED  103 // Chunked request which is sub-encoded with some other format.
#define NKN_TR_RES_NO_ETAG_LMH		104 // If etag/lmh not there and cod version ignore not set.
#define NKN_TR_RES_INCAPABLE_BR_ORIGIN	105 //response rproxy incapable byte range origin tunnel 
#define NKN_TR_RES_VARY_DISABLE		106 // Response has Vary header and Vary header handling not enabled 
#define NKN_TR_RES_NOT_SUPPORTED_VARY	107 // Response has unsupported Vary header
#define NKN_TR_RES_CONNECTION_UPGRADE	108 //Connection: upgrade needs connection level tunnel

#define NKN_TR_MASK(x) ((uint64_t)1 << x)
#define SET_TR_REQ_OVERRIDE(x,y) (x |= (NKN_TR_MASK(y)))
#define CHECK_TR_REQ_OVERRIDE(x,y) (x & (NKN_TR_MASK(y)))

#define SET_TR_RES_OVERRIDE(x,y) (x |= (NKN_TR_MASK((y-64))))
#define CHECK_TR_RES_OVERRIDE(x,y) (x & (NKN_TR_MASK((y-64))))

#endif

