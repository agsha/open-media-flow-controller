#ifndef _HTTP_DEF_H
#define _HTTP_DEF_H
/*
 *****************************
 * Known header definitions  *
 *****************************
 */
typedef enum {
    MIME_HDR_ACCEPT = 0,
    MIME_HDR_ACCEPT_CHARSET,
    MIME_HDR_ACCEPT_ENCODING,
    MIME_HDR_ACCEPT_LANGUAGE,
    MIME_HDR_ACCEPT_RANGES,
    MIME_HDR_AGE,
    MIME_HDR_ALLOW,
    MIME_HDR_AUTHORIZATION,
    MIME_HDR_CACHE_CONTROL,
    MIME_HDR_CONNECTION,
    MIME_HDR_CONTENT_BASE,
    MIME_HDR_CONTENT_DISPOSITION,
    MIME_HDR_CONTENT_ENCODING,
    MIME_HDR_CONTENT_LANGUAGE,
    MIME_HDR_CONTENT_LENGTH,
    MIME_HDR_CONTENT_LOCATION,
    MIME_HDR_CONTENT_MD5,
    MIME_HDR_CONTENT_RANGE,
    MIME_HDR_CONTENT_TYPE,
    MIME_HDR_TE,
    MIME_HDR_TRANSFER_ENCODING,
    MIME_HDR_TRAILER,
    MIME_HDR_COOKIE,
    MIME_HDR_COOKIE2,
    MIME_HDR_DATE,
    MIME_HDR_ETAG,
    MIME_HDR_EXPIRES,
    MIME_HDR_FROM,
    MIME_HDR_HOST,
    MIME_HDR_IF_MATCH,
    MIME_HDR_IF_MODIFIED_SINCE,
    MIME_HDR_IF_NONE_MATCH,
    MIME_HDR_IF_RANGE,
    MIME_HDR_IF_UNMODIFIED_SINCE,
    MIME_HDR_LAST_MODIFIED,
    MIME_HDR_LINK,
    MIME_HDR_LOCATION,
    MIME_HDR_MAX_FORWARDS,
    MIME_HDR_MIME_VERSION,
    MIME_HDR_PRAGMA,
    MIME_HDR_PROXY_AUTHENTICATE,
    MIME_HDR_PROXY_AUTHENTICATION_INFO,
    MIME_HDR_PROXY_AUTHORIZATION,
    MIME_HDR_PROXY_CONNECTION,
    MIME_HDR_PUBLIC,
    MIME_HDR_RANGE,
    MIME_HDR_REQUEST_RANGE,
    MIME_HDR_REFERER,
    MIME_HDR_RETRY_AFTER,
    MIME_HDR_SERVER,
    MIME_HDR_SET_COOKIE,
    MIME_HDR_SET_COOKIE2,
    MIME_HDR_UPGRADE,
    MIME_HDR_USER_AGENT,
    MIME_HDR_VARY,
    MIME_HDR_VIA,
    MIME_HDR_EXPECT,
    MIME_HDR_WARNING,
    MIME_HDR_WWW_AUTHENTICATE,
    MIME_HDR_AUTHENTICATION_INFO,
    MIME_HDR_KEEP_ALIVE,
    /*
     **********************
     * Nokeena extensions *
     **********************
     */
    MIME_HDR_X_NKN_URI,		/* URI as received OTW */
    MIME_HDR_X_NKN_METHOD,	/* HTTP method as received OTW */
    MIME_HDR_X_NKN_QUERY,	/* HTTP query string as received OTW */
    MIME_HDR_X_NKN_RESPONSE_STR,/* HTTP response string */
    MIME_HDR_X_NKN_REMAPPED_URI,/* Target URI to use, remapped via SSP */
    MIME_HDR_X_NKN_DECODED_URI,	/* Decoded URI */
    MIME_HDR_X_NKN_SEEK_URI,	/* HDR_X_NKN_URI less the query param  */
    				/*   for seek */
    MIME_HDR_X_NKN_FP_SVRHOST,	/* Forward Proxy Server IP  */
    MIME_HDR_X_NKN_FP_SVRPORT,	/* Forward Proxy Server Port  */
    MIME_HDR_X_NKN_CLIENT_HOST,	/* Client Host header (used by OM) */
    MIME_HDR_X_NKN_REQ_DEST_IP,	/* Request dest IP (network byte order) */
    MIME_HDR_X_NKN_REQ_DEST_PORT,/* Request dest Port (network byte order) */
    MIME_HDR_X_NKN_ABS_URL_HOST,/* Host from absolute URL */
    MIME_HDR_X_NKN_REQ_LINE,	/* HTTP request line  (used by access log) */
    MIME_HDR_X_NKN_REQ_REAL_DEST_IP, /* Request dest IP picked from packet */
    MIME_HDR_X_NKN_REQ_REAL_DEST_PORT, /* Request dest port picked from packet*/
    MIME_HDR_X_NKN_REQ_REAL_SRC_IP,/* Requset src port picked from packet */
    MIME_HDR_X_NKN_REQ_REAL_SRC_PORT, /* Request src IP pciked from packet */
    MIME_HDR_X_NKN_ORIGIN_SVR,	/* Origin server set by PE */
    MIME_HDR_X_NKN_CACHE_POLICY,	/* Cache yes/no set by PE */
    MIME_HDR_X_NKN_CACHE_NAME,	/* Cache name set by PE for caching uri's with query */
    /*
     *********************************
     * Nokeena - Customer Extensions *
     *********************************
     */
    MIME_HDR_X_ACCEL_CACHE_CONTROL,
    MIME_HDR_X_LOCATION,
    MIME_HDR_X_REDIRECT_HOST,
    MIME_HDR_X_REDIRECT_PORT,
    MIME_HDR_X_REDIRECT_URI,

    /*
     **********************
     * Nokeena extensions *
     **********************
     */
    MIME_HDR_X_NKN_CL7_CACHEKEY_HOST, /* Added to request hdrs (host:port),
 				       * used by OSS_HTTP_PROXY_CONFIG */
    MIME_HDR_X_NKN_CL7_ORIGIN_HOST, /* Added to request hdrs (host:port),
    				     * used by OSS_HTTP_PROXY_CONFIG */
    MIME_HDR_X_NKN_CL7_PROXY, /* Added to request hdrs, pass Real ClientIP 
    			       * and DestIP:Port when proxying TProxy reqs */
    MIME_HDR_X_NKN_CL7_STATUS, /* Added to response hdrs,
    				* Cluster L7 request status for access.log */
    MIME_HDR_X_INTERNAL,
    MIME_HDR_X_NKN_PE_HOST_HDR,	/* Host header set by PE */
    MIME_HDR_X_NKN_INCLUDE_ORIG, /* Include original ip and port */
    MIME_HDR_X_NKN_ORIGIN_IP,    /* Origin Server IP */
    MIME_HDR_X_NKN_MD5_CHECKSUM, /* X-NKN-MD5-Checksum header */
    MIME_HDR_MAX_DEFS
    /*
     ***************************************************************************
     * Note: Always add to the end of list to prevent invalidating persistent
     *       cache attribute data.
     ***************************************************************************
     */
} http_header_id;

#endif
