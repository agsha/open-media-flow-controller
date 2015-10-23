/* File : http_data.c Author : 
 * Copyright (C) 2008 - 2009 Nokeena Networks, Inc. 
 * All rights reserved.
 */


#ifdef PROTO_HTTP_LIB
#include "proto_http_int.h"
#include "http_def.h"
#include "mime_header.h"
#include "http_header.h"
#else 
#include <http_def.h>
#include <mime_header.h>
#include <http_header.h>
#endif

mime_header_descriptor_t http_known_headers[MIME_HDR_MAX_DEFS] = {
    {"Accept", 6, MIME_HDR_ACCEPT, DT_STRING, 1},
    {"Accept-Charset", 14, MIME_HDR_ACCEPT_CHARSET, DT_STRING, 1},
    {"Accept-Encoding", 15, MIME_HDR_ACCEPT_ENCODING, DT_STRING, 1},
    {"Accept-Language", 15, MIME_HDR_ACCEPT_LANGUAGE, DT_STRING, 1},
    {"Accept-Ranges", 13, MIME_HDR_ACCEPT_RANGES, DT_STRING, 1},
    {"Age", 3, MIME_HDR_AGE, DT_INT, 0},
    {"Allow", 5, MIME_HDR_ALLOW, DT_STRING, 1},
    {"Authorization", 13, MIME_HDR_AUTHORIZATION, DT_STRING, 0},
    {"Cache-Control", 13, MIME_HDR_CACHE_CONTROL, DT_CACHECONTROL_ENUM, 1},
    {"Connection", 10, MIME_HDR_CONNECTION, DT_STRING, 0},
    {"Content-Base", 12, MIME_HDR_CONTENT_BASE, DT_STRING, 0},
    {"Content-Disposition", 19, MIME_HDR_CONTENT_DISPOSITION, DT_STRING, 0},
    {"Content-Encoding", 16, MIME_HDR_CONTENT_ENCODING, DT_STRING, 1},
    {"Content-Language", 16, MIME_HDR_CONTENT_LANGUAGE, DT_STRING, 1},
    {"Content-Length", 14, MIME_HDR_CONTENT_LENGTH, DT_SIZE, 0},
    {"Content-Location", 16, MIME_HDR_CONTENT_LOCATION, DT_STRING, 0},
    {"Content-MD5", 11, MIME_HDR_CONTENT_MD5, DT_STRING, 0},
    {"Content-Range", 13, MIME_HDR_CONTENT_RANGE, DT_CONTENTRANGE, 0},
    {"Content-Type", 12, MIME_HDR_CONTENT_TYPE, DT_STRING, 0},
    {"Te", 2, MIME_HDR_TE, DT_STRING, 1},
    {"Transfer-Encoding", 17, MIME_HDR_TRANSFER_ENCODING, DT_STRING, 1},
    {"Trailer", 7, MIME_HDR_TRAILER, DT_STRING, 1},
    {"Cookie", 6, MIME_HDR_COOKIE, DT_STRING, 1},
    {"Cookie2", 7, MIME_HDR_COOKIE2, DT_STRING, 1},
    {"Date", 4, MIME_HDR_DATE, DT_DATE_1123, 0},
    {"ETag", 4, MIME_HDR_ETAG, DT_STRING, 0},
    {"Expires", 7, MIME_HDR_EXPIRES, DT_DATE_1123, 0},
    {"From", 4, MIME_HDR_FROM, DT_STRING, 0},
    {"Host", 4, MIME_HDR_HOST, DT_STRING, 0},
    {"If-Match", 8, MIME_HDR_IF_MATCH, DT_STRING, 1},
    {"If-Modified-Since", 17, MIME_HDR_IF_MODIFIED_SINCE, DT_DATE_1123, 0},
    {"If-None-Match", 13, MIME_HDR_IF_NONE_MATCH, DT_STRING, 1},
    {"If-Range", 8, MIME_HDR_IF_RANGE, DT_DATE_1123_OR_ETAG, 0},
    {"If-Unmodfied-Since", 18, MIME_HDR_IF_UNMODIFIED_SINCE, DT_DATE_1123, 0},
    {"Last-Modified", 13, MIME_HDR_LAST_MODIFIED, DT_DATE_1123, 0},
    {"Link", 4, MIME_HDR_LINK, DT_STRING, 1},
    {"Location", 8, MIME_HDR_LOCATION, DT_STRING, 0},
    {"Max-Forwards", 12, MIME_HDR_MAX_FORWARDS, DT_INT, 0},
    {"Mime-Version", 12, MIME_HDR_MIME_VERSION, DT_STRING, 0},
    {"Pragma", 6, MIME_HDR_PRAGMA, DT_STRING, 1},
    {"Proxy-Authenticate", 18, MIME_HDR_PROXY_AUTHENTICATE, DT_STRING, 0},
    {"Proxy-Authentication-Info", 25, MIME_HDR_PROXY_AUTHENTICATION_INFO, DT_STRING, 1},
    {"Proxy-Authorization", 19, MIME_HDR_PROXY_AUTHORIZATION, DT_STRING, 0},
    {"Proxy-Connection", 16, MIME_HDR_PROXY_CONNECTION, DT_STRING, 1},
    {"Public", 6, MIME_HDR_PUBLIC, DT_STRING, 0},
    {"Range", 5, MIME_HDR_RANGE, DT_RANGE, 0},
    {"Request-Range", 13, MIME_HDR_REQUEST_RANGE, DT_RANGE, 0},
    {"Referer", 7, MIME_HDR_REFERER, DT_STRING, 0},
    {"Retry-After", 11, MIME_HDR_RETRY_AFTER, DT_STRING, 0},
    {"Server", 6, MIME_HDR_SERVER, DT_STRING, 0},
    {"Set-Cookie", 10, MIME_HDR_SET_COOKIE, DT_STRING, 1},
    {"Set-Cookie2", 11, MIME_HDR_SET_COOKIE2, DT_STRING, 1},
    {"Upgrade", 7, MIME_HDR_UPGRADE, DT_STRING, 1},
    {"User-Agent", 10, MIME_HDR_USER_AGENT, DT_STRING, 0},
    {"Vary", 4, MIME_HDR_VARY, DT_STRING, 1},
    {"Via", 3, MIME_HDR_VIA, DT_STRING, 1},
    {"Expect", 6, MIME_HDR_EXPECT, DT_STRING, 1},
    {"Warning", 7, MIME_HDR_WARNING, DT_STRING, 1},
    {"WWW-Authenticate", 16, MIME_HDR_WWW_AUTHENTICATE, DT_STRING, 1},
    {"Authentication-Info", 19, MIME_HDR_AUTHENTICATION_INFO, DT_STRING, 1},
    {"Keep-Alive", 10, MIME_HDR_KEEP_ALIVE, DT_STRING, 0},
    /*
     **********************
     * Nokeena extensions *
     **********************
     */
    {"X-NKN-Uri", 9, MIME_HDR_X_NKN_URI, DT_STRING, 0},
    {"X-NKN-Method", 12, MIME_HDR_X_NKN_METHOD, DT_STRING, 0},
    {"X-NKN-Query", 11, MIME_HDR_X_NKN_QUERY, DT_STRING, 0},
    {"X-NKN-Response-String", 21, MIME_HDR_X_NKN_RESPONSE_STR, DT_STRING, 0},
    {"X-NKN-Remapped-Uri", 18, MIME_HDR_X_NKN_REMAPPED_URI, DT_STRING, 0},
    {"X-NKN-Decoded-Uri", 17, MIME_HDR_X_NKN_DECODED_URI, DT_STRING, 0},
    {"X-NKN-Seek-Uri", 14, MIME_HDR_X_NKN_SEEK_URI, DT_STRING, 0},
    {"X-NKN-FP-svrhost", 16, MIME_HDR_X_NKN_FP_SVRHOST, DT_STRING, 0},
    {"X-NKN-FP-svrport", 16, MIME_HDR_X_NKN_FP_SVRPORT, DT_STRING, 0},
    {"X-NKN-Client-Host", 17, MIME_HDR_X_NKN_CLIENT_HOST, DT_STRING, 0},
    {"X-NKN-Req-Dest-IP", 17, MIME_HDR_X_NKN_REQ_DEST_IP, DT_STRING, 0},
    {"X-NKN-Req-Dest-Port", 19, MIME_HDR_X_NKN_REQ_DEST_PORT, DT_STRING, 0},
    {"X-NKN-Abs-Url-Host", 18, MIME_HDR_X_NKN_ABS_URL_HOST, DT_STRING, 0},
    {"X-NKN-Request-Line", 18, MIME_HDR_X_NKN_REQ_LINE, DT_STRING, 0},
    {"X-NKN-Req-Real-Dest-IP", 22, MIME_HDR_X_NKN_REQ_REAL_DEST_IP, DT_STRING, 0},
    {"X-NKN-Req-Real-Dest-Port", 24, MIME_HDR_X_NKN_REQ_REAL_DEST_PORT, DT_STRING, 0},
    {"X-NKN-Req-Real-Src-IP", 21, MIME_HDR_X_NKN_REQ_REAL_SRC_IP, DT_STRING, 0},
    {"X-NKN-Req-Real-Src-Port", 23, MIME_HDR_X_NKN_REQ_REAL_SRC_PORT, DT_STRING, 0},
    {"X-NKN-Origin-Svr", 16, MIME_HDR_X_NKN_ORIGIN_SVR, DT_STRING, 0},
    {"X-NKN-Cache-Policy", 18, MIME_HDR_X_NKN_CACHE_POLICY, DT_STRING, 0},
    {"X-NKN-Cache-Name", 16, MIME_HDR_X_NKN_CACHE_NAME, DT_STRING, 0},
    /*
     *********************************
     * Nokeena - Customer Extensions *
     *********************************
     */
    {"X-Accel-Cache-Control", 21, MIME_HDR_X_ACCEL_CACHE_CONTROL, DT_CACHECONTROL_ENUM, 1},
    {"X-Location", 10, MIME_HDR_X_LOCATION, DT_STRING, 0},
    {"X-Redirect-Host", 15, MIME_HDR_X_REDIRECT_HOST, DT_STRING, 0},
    {"X-Redirect-Port", 15, MIME_HDR_X_REDIRECT_PORT, DT_STRING, 0},
    {"X-Redirect-Uri", 14, MIME_HDR_X_REDIRECT_URI, DT_STRING, 0},
    /*
     **********************
     * Nokeena extensions *
     **********************
     */
    {"X-NKN-CL7-Cachekey-Host", 23, MIME_HDR_X_NKN_CL7_CACHEKEY_HOST, DT_STRING, 0},
    {"X-NKN-CL7-Origin-Host", 21, MIME_HDR_X_NKN_CL7_ORIGIN_HOST, DT_STRING, 0},
    {"X-NKN-CL7-Proxy", 15, MIME_HDR_X_NKN_CL7_PROXY, DT_STRING, 0},
    {"X-NKN-CL7-Status", 16, MIME_HDR_X_NKN_CL7_STATUS, DT_STRING, 1},
    {"X-NKN-Internal", 14, MIME_HDR_X_INTERNAL, DT_STRING, 0},
    {"X-NKN-PE-Host", 13, MIME_HDR_X_NKN_PE_HOST_HDR, DT_STRING, 0},
    {"X-NKN-Include-Orig", 18, MIME_HDR_X_NKN_INCLUDE_ORIG, DT_STRING, 0},
    {"X-NKN-Origin-IP", 15, MIME_HDR_X_NKN_ORIGIN_IP, DT_STRING, 0},
    {"X-NKN-MD5-Checksum", 18, MIME_HDR_X_NKN_MD5_CHECKSUM, DT_STRING, 0},
};

char http_end2end_header[MIME_HDR_MAX_DEFS] = 
{
    1, /* MIME_HDR_ACCEPT */
    1, /* MIME_HDR_ACCEPT_CHARSET */
    1, /* MIME_HDR_ACCEPT_ENCODING */
    1, /* MIME_HDR_ACCEPT_LANGUAGE */
    0, /* MIME_HDR_ACCEPT_RANGES */
    1, /* MIME_HDR_AGE */
    1, /* MIME_HDR_ALLOW */
    1, /* MIME_HDR_AUTHORIZATION */
    1, /* MIME_HDR_CACHE_CONTROL */
    0, /* MIME_HDR_CONNECTION */
    1, /* MIME_HDR_CONTENT_BASE */
    1, /* MIME_HDR_CONTENT_DISPOSITION */
    1, /* MIME_HDR_CONTENT_ENCODING */
    1, /* MIME_HDR_CONTENT_LANGUAGE */
    1, /* MIME_HDR_CONTENT_LENGTH */
    1, /* MIME_HDR_CONTENT_LOCATION */
    1, /* MIME_HDR_CONTENT_MD5 */
    1, /* MIME_HDR_CONTENT_RANGE */
    1, /* MIME_HDR_CONTENT_TYPE */
    0, /* MIME_HDR_TE */
    0, /* MIME_HDR_TRANSFER_ENCODING */
    0, /* MIME_HDR_TRAILER */
    1, /* MIME_HDR_COOKIE */
    1, /* MIME_HDR_COOKIE2 */
    1, /* MIME_HDR_DATE */
    1, /* MIME_HDR_ETAG */
    1, /* MIME_HDR_EXPIRES */
    1, /* MIME_HDR_FROM */
    1, /* MIME_HDR_HOST */
    1, /* MIME_HDR_IF_MATCH */
    1, /* MIME_HDR_IF_MODIFIED_SINCE */
    1, /* MIME_HDR_IF_NONE_MATCH */
    1, /* MIME_HDR_IF_RANGE */
    1, /* MIME_HDR_IF_UNMODIFIED_SINCE */
    1, /* MIME_HDR_LAST_MODIFIED */
    1, /* MIME_HDR_LINK */
    1, /* MIME_HDR_LOCATION */
    1, /* MIME_HDR_MAX_FORWARDS */
    1, /* MIME_HDR_MIME_VERSION */
    1, /* MIME_HDR_PRAGMA */
    0, /* MIME_HDR_PROXY_AUTHENTICATE */
    1, /* MIME_HDR_PROXY_AUTHENTICATION_INFO */
    0, /* MIME_HDR_PROXY_AUTHORIZATION */
    1, /* MIME_HDR_PROXY_CONNECTION */
    0, /* MIME_HDR_PUBLIC */
    1, /* MIME_HDR_RANGE */
    1, /* MIME_HDR_REQUEST_RANGE */
    1, /* MIME_HDR_REFERER */
    1, /* MIME_HDR_RETRY_AFTER */
    1, /* MIME_HDR_SERVER */
    1, /* MIME_HDR_SET_COOKIE */
    1, /* MIME_HDR_SET_COOKIE2 */
    0, /* MIME_HDR_UPGRADE */
    1, /* MIME_HDR_USER_AGENT */
    1, /* MIME_HDR_VARY */
    1, /* MIME_HDR_VIA */
    1, /* MIME_HDR_EXPECT */
    1, /* MIME_HDR_WARNING */
    1, /* MIME_HDR_WWW_AUTHENTICATE */
    1, /* MIME_HDR_AUTHENTICATION_INFO */
    0, /* MIME_HDR_KEEP_ALIVE */
    /*
     **********************
     * Nokeena extensions *
     **********************
     */
    0, /* MIME_HDR_X_NKN_URI */
    0, /* MIME_HDR_X_NKN_METHOD */
    0, /* MIME_HDR_X_NKN_QUERY */
    0, /* MIME_HDR_X_NKN_RESPONSE_STR */
    0, /* MIME_HDR_X_NKN_REMAPPED_URI */
    0, /* MIME_HDR_X_NKN_DECODED_URI */
    0, /* MIME_HDR_X_NKN_SEEK_URI */
    0, /* MIME_HDR_X_NKN_FP_SVRHOST */
    0, /* MIME_HDR_X_NKN_FP_SVRPORT */
    0, /* MIME_HDR_X_NKN_CLIENT_HOST */
    0, /* MIME_HDR_X_NKN_REQ_DEST_IP */
    0, /* MIME_HDR_X_NKN_REQ_DEST_PORT */
    0, /* MIME_HDR_X_NKN_ABS_URL_HOST */
    0, /* MIME_HDR_X_NKN_REQ_LINE */
    0, /* MIME_HDR_X_NKN_REQ_REAL_DEST_IP */
    0, /* MIME_HDR_X_NKN_REQ_REAL_DEST_PORT */
    0, /* MIME_HDR_X_NKN_REQ_REAL_SRC_IP */
    0, /* MIME_HDR_X_NKN_REQ_REAL_SRC_PORT */
    0, /* MIME_HDR_X_NKN_ORIGIN_SVR */
    0, /* MIME_HDR_X_NKN_CACHE_POLICY */
    0, /* MIME_HDR_X_NKN_CACHE_NAME */
    /*
     *********************************
     * Nokeena - Customer Extensions *
     *********************************
     */
    1, /* MIME_HDR_X_ACCEL_CACHE_CONTROL */
    0, /* MIME_HDR_X_LOCATION */
    0, /* MIME_HDR_X_REDIRECT_HOST */
    0, /* MIME_HDR_X_REDIRECT_PORT */
    0, /* MIME_HDR_X_REDIRECT_URI */
    /*
     **********************
     * Nokeena extensions *
     **********************
     */
    0, /* MIME_HDR_X_NKN_CL7_CACHEKEY_HOST */
    0, /* MIME_HDR_X_NKN_CL7_ORIGIN_HOST */
    1, /* MIME_HDR_X_NKN_CL7_PROXY */
    0, /* MIME_HDR_X_NKN_CL7_STATUS */
    0,
    0, /* MIME_HDR_X_NKN_PE_HOST_HDR */
    1, /* MIME_HDR_X_NKN_INCLUDE_ORIG */
    0, /* MIME_HDR_X_NKN_ORIGIN_IP */
    0, /* MIME_HDR_X_NKN_MD5_CHECKSUM */
};

char http_allowed_304_header[MIME_HDR_MAX_DEFS] = 
{
    1, /* MIME_HDR_ACCEPT */
    1, /* MIME_HDR_ACCEPT_CHARSET */
    1, /* MIME_HDR_ACCEPT_ENCODING */
    1, /* MIME_HDR_ACCEPT_LANGUAGE */
    1, /* MIME_HDR_ACCEPT_RANGES */
    1, /* MIME_HDR_AGE */
    0, /* MIME_HDR_ALLOW */
    1, /* MIME_HDR_AUTHORIZATION */
    1, /* MIME_HDR_CACHE_CONTROL */
    0, /* MIME_HDR_CONNECTION */
    1, /* MIME_HDR_CONTENT_BASE */
    1, /* MIME_HDR_CONTENT_DISPOSITION */
    0, /* MIME_HDR_CONTENT_ENCODING */
    0, /* MIME_HDR_CONTENT_LANGUAGE */
    0, /* MIME_HDR_CONTENT_LENGTH */
    1, /* MIME_HDR_CONTENT_LOCATION */
    0, /* MIME_HDR_CONTENT_MD5 */
    0, /* MIME_HDR_CONTENT_RANGE */
    0, /* MIME_HDR_CONTENT_TYPE */
    0, /* MIME_HDR_TE */
    1, /* MIME_HDR_TRANSFER_ENCODING */
    0, /* MIME_HDR_TRAILER */
    0, /* MIME_HDR_COOKIE */
    0, /* MIME_HDR_COOKIE2 */
    1, /* MIME_HDR_DATE */
    1, /* MIME_HDR_ETAG */
    1, /* MIME_HDR_EXPIRES */
    1, /* MIME_HDR_FROM */
    1, /* MIME_HDR_HOST */
    0, /* MIME_HDR_IF_MATCH */
    0, /* MIME_HDR_IF_MODIFIED_SINCE */
    0, /* MIME_HDR_IF_NONE_MATCH */
    0, /* MIME_HDR_IF_RANGE */
    0, /* MIME_HDR_IF_UNMODIFIED_SINCE */
    0, /* MIME_HDR_LAST_MODIFIED */
    1, /* MIME_HDR_LINK */
    1, /* MIME_HDR_LOCATION */
    1, /* MIME_HDR_MAX_FORWARDS */
    1, /* MIME_HDR_MIME_VERSION */
    1, /* MIME_HDR_PRAGMA */
    0, /* MIME_HDR_PROXY_AUTHENTICATE */
    1, /* MIME_HDR_PROXY_AUTHENTICATION_INFO */
    0, /* MIME_HDR_PROXY_AUTHORIZATION */
    1, /* MIME_HDR_PROXY_CONNECTION */
    0, /* MIME_HDR_PUBLIC */
    1, /* MIME_HDR_RANGE */
    1, /* MIME_HDR_REQUEST_RANGE */
    1, /* MIME_HDR_REFERER */
    1, /* MIME_HDR_RETRY_AFTER */
    1, /* MIME_HDR_SERVER */
    1, /* MIME_HDR_SET_COOKIE */
    1, /* MIME_HDR_SET_COOKIE2 */
    0, /* MIME_HDR_UPGRADE */
    1, /* MIME_HDR_USER_AGENT */
    1, /* MIME_HDR_VARY */
    1, /* MIME_HDR_VIA */
    1, /* MIME_HDR_EXPECT */
    1, /* MIME_HDR_WARNING */
    1, /* MIME_HDR_WWW_AUTHENTICATE */
    1, /* MIME_HDR_AUTHENTICATION_INFO */
    0, /* MIME_HDR_KEEP_ALIVE */
    /*
     **********************
     * Nokeena extensions *
     **********************
     */
    0, /* MIME_HDR_X_NKN_URI */
    0, /* MIME_HDR_X_NKN_METHOD */
    0, /* MIME_HDR_X_NKN_QUERY */
    0, /* MIME_HDR_X_NKN_RESPONSE_STR */
    0, /* MIME_HDR_X_NKN_REMAPPED_URI */
    0, /* MIME_HDR_X_NKN_DECODED_URI */
    0, /* MIME_HDR_X_NKN_SEEK_URI */
    0, /* MIME_HDR_X_NKN_FP_SVRHOST */
    0, /* MIME_HDR_X_NKN_FP_SVRPORT */
    0, /* MIME_HDR_X_NKN_CLIENT_HOST */
    0, /* MIME_HDR_X_NKN_REQ_DEST_IP */
    0, /* MIME_HDR_X_NKN_REQ_DEST_PORT */
    0, /* MIME_HDR_X_NKN_ABS_URL_HOST */
    0, /* MIME_HDR_X_NKN_REQ_LINE */
    0, /* MIME_HDR_X_NKN_REQ_REAL_DEST_IP */
    0, /* MIME_HDR_X_NKN_REQ_REAL_DEST_PORT */
    0, /* MIME_HDR_X_NKN_REQ_REAL_SRC_IP */
    0, /* MIME_HDR_X_NKN_REQ_REAL_SRC_PORT */
    0, /* MIME_HDR_X_NKN_ORIGIN_SVR */
    0, /* MIME_HDR_X_NKN_CACHE_POLICY */
    0, /* MIME_HDR_X_NKN_CACHE_NAME */
    /*
     *********************************
     * Nokeena - Customer Extensions *
     *********************************
     */
    1, /* MIME_HDR_X_ACCEL_CACHE_CONTROL */
    0, /* MIME_HDR_X_LOCATION */
    0, /* MIME_HDR_X_REDIRECT_HOST */
    0, /* MIME_HDR_X_REDIRECT_PORT */
    0, /* MIME_HDR_X_REDIRECT_URI */
    /*
     **********************
     * Nokeena extensions *
     **********************
     */
    0, /* MIME_HDR_X_NKN_CL7_CACHEKEY_HOST */
    0, /* MIME_HDR_X_NKN_CL7_ORIGIN_HOST */
    0, /* MIME_HDR_X_NKN_CL7_PROXY */
    0, /* MIME_HDR_X_NKN_CL7_STATUS */
    0,
    0, /* MIME_HDR_X_NKN_PE_HOST_HDR */
    0, /* MIME_HDR_X_NKN_INCLUDE_ORIG */
    0, /* MIME_HDR_X_NKN_ORIGIN_IP */
    0, /* MIME_HDR_X_NKN_MD5_CHECKSUM */
};

char http_removable_304_header[MIME_HDR_MAX_DEFS] = 
{
    0, /* MIME_HDR_ACCEPT */
    0, /* MIME_HDR_ACCEPT_CHARSET */
    0, /* MIME_HDR_ACCEPT_ENCODING */
    0, /* MIME_HDR_ACCEPT_LANGUAGE */
    0, /* MIME_HDR_ACCEPT_RANGES */
    0, /* MIME_HDR_AGE */
    0, /* MIME_HDR_ALLOW */
    0, /* MIME_HDR_AUTHORIZATION */
    0, /* MIME_HDR_CACHE_CONTROL */
    0, /* MIME_HDR_CONNECTION */
    0, /* MIME_HDR_CONTENT_BASE */
    0, /* MIME_HDR_CONTENT_DISPOSITION */
    0, /* MIME_HDR_CONTENT_ENCODING */
    0, /* MIME_HDR_CONTENT_LANGUAGE */
    0, /* MIME_HDR_CONTENT_LENGTH */
    0, /* MIME_HDR_CONTENT_LOCATION */
    0, /* MIME_HDR_CONTENT_MD5 */
    0, /* MIME_HDR_CONTENT_RANGE */
    0, /* MIME_HDR_CONTENT_TYPE */
    0, /* MIME_HDR_TE */
    0, /* MIME_HDR_TRANSFER_ENCODING */
    0, /* MIME_HDR_TRAILER */
    1, /* MIME_HDR_COOKIE */
    1, /* MIME_HDR_COOKIE2 */
    0, /* MIME_HDR_DATE */
    0, /* MIME_HDR_ETAG */
    0, /* MIME_HDR_EXPIRES */
    0, /* MIME_HDR_FROM */
    0, /* MIME_HDR_HOST */
    0, /* MIME_HDR_IF_MATCH */
    0, /* MIME_HDR_IF_MODIFIED_SINCE */
    0, /* MIME_HDR_IF_NONE_MATCH */
    0, /* MIME_HDR_IF_RANGE */
    0, /* MIME_HDR_IF_UNMODIFIED_SINCE */
    0, /* MIME_HDR_LAST_MODIFIED */
    0, /* MIME_HDR_LINK */
    0, /* MIME_HDR_LOCATION */
    0, /* MIME_HDR_MAX_FORWARDS */
    0, /* MIME_HDR_MIME_VERSION */
    0, /* MIME_HDR_PRAGMA */
    0, /* MIME_HDR_PROXY_AUTHENTICATE */
    0, /* MIME_HDR_PROXY_AUTHENTICATION_INFO */
    0, /* MIME_HDR_PROXY_AUTHORIZATION */
    0, /* MIME_HDR_PROXY_CONNECTION */
    0, /* MIME_HDR_PUBLIC */
    0, /* MIME_HDR_RANGE */
    0, /* MIME_HDR_REQUEST_RANGE */
    0, /* MIME_HDR_REFERER */
    0, /* MIME_HDR_RETRY_AFTER */
    0, /* MIME_HDR_SERVER */
    0, /* MIME_HDR_SET_COOKIE */
    0, /* MIME_HDR_SET_COOKIE2 */
    0, /* MIME_HDR_UPGRADE */
    0, /* MIME_HDR_USER_AGENT */
    0, /* MIME_HDR_VARY */
    0, /* MIME_HDR_VIA */
    0, /* MIME_HDR_EXPECT */
    0, /* MIME_HDR_WARNING */
    0, /* MIME_HDR_WWW_AUTHENTICATE */
    0, /* MIME_HDR_AUTHENTICATION_INFO */
    0, /* MIME_HDR_KEEP_ALIVE */
    /*
     **********************
     * Nokeena extensions *
     **********************
     */
    0, /* MIME_HDR_X_NKN_URI */
    0, /* MIME_HDR_X_NKN_METHOD */
    0, /* MIME_HDR_X_NKN_QUERY */
    0, /* MIME_HDR_X_NKN_RESPONSE_STR */
    0, /* MIME_HDR_X_NKN_REMAPPED_URI */
    0, /* MIME_HDR_X_NKN_DECODED_URI */
    0, /* MIME_HDR_X_NKN_SEEK_URI */
    0, /* MIME_HDR_X_NKN_FP_SVRHOST */
    0, /* MIME_HDR_X_NKN_FP_SVRPORT */
    0, /* MIME_HDR_X_NKN_CLIENT_HOST */
    0, /* MIME_HDR_X_NKN_REQ_DEST_IP */
    0, /* MIME_HDR_X_NKN_REQ_DEST_PORT */
    0, /* MIME_HDR_X_NKN_ABS_URL_HOST */
    0, /* MIME_HDR_X_NKN_REQ_LINE */
    0, /* MIME_HDR_X_NKN_REQ_REAL_DEST_IP */
    0, /* MIME_HDR_X_NKN_REQ_REAL_DEST_PORT */
    0, /* MIME_HDR_X_NKN_REQ_REAL_SRC_IP */
    0, /* MIME_HDR_X_NKN_REQ_REAL_SRC_PORT */
    0, /* MIME_HDR_X_NKN_ORIGIN_SVR */
    0, /* MIME_HDR_X_NKN_CACHE_POLICY */
    0, /* MIME_HDR_X_NKN_CACHE_NAME */
    /*
     *********************************
     * Nokeena - Customer Extensions *
     *********************************
     */
    1, /* MIME_HDR_X_ACCEL_CACHE_CONTROL */
    0, /* MIME_HDR_X_LOCATION */
    0, /* MIME_HDR_X_REDIRECT_HOST */
    0, /* MIME_HDR_X_REDIRECT_PORT */
    0, /* MIME_HDR_X_REDIRECT_URI */
    /*
     **********************
     * Nokeena extensions *
     **********************
     */
    0, /* MIME_HDR_X_NKN_CL7_CACHEKEY_HOST */
    0, /* MIME_HDR_X_NKN_CL7_ORIGIN_HOST */
    0, /* MIME_HDR_X_NKN_CL7_PROXY */
    0, /* MIME_HDR_X_NKN_CL7_STATUS */
    0,
    0, /* MIME_HDR_X_NKN_PE_HOST_HDR */
    0, /* MIME_HDR_X_NKN_INCLUDE_ORIG */
    0, /* MIME_HDR_X_NKN_ORIGIN_IP */
    0, /* MIME_HDR_X_NKN_MD5_CHECKSUM */
};
