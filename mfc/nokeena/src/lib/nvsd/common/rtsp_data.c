#include <rtsp_def.h>
#include <rtsp_header.h>

#define RSET_HD(hdr_id, hdr_name, hdr_datatype, hdr_comma) \
	{\
		.id = hdr_id,\
		.name = hdr_name,\
		.namelen = sizeof(hdr_name) - 1 ,\
		.type = hdr_datatype,\
		.comma_hdr = hdr_comma\
	}


#define RTSP_HDR_INDEX_START ((uint32_t)(RTSP_HDR_ACCEPT-1))
#define RTSP_HDR_INDEX_END ((uint32_t)(RTSP_HDR_WWW_AUTHENTICATE-1))
#define RTSP_HDR_INDEX(header) ((uint32_t)(header-1))

/** 
 * @brief Header Map for Parser
 * Header Comparisions are case-insensitive and hence int-char map(key) is given in lower case
 * All Headers Enabled by default
 */
mime_header_descriptor_t rtsp_known_headers[RTSP_HDR_MAX_DEFS] = {
    RSET_HD(RTSP_HDR_ACCEPT, "Accept", DT_STRING, 1),
    RSET_HD(RTSP_HDR_ACCEPT_ENCODING, "Accept-Encoding", DT_STRING, 1),
    RSET_HD(RTSP_HDR_ACCEPT_LANGUAGE, "Accept-Language", DT_STRING, 1),
    RSET_HD(RTSP_HDR_ALLOW, "Allow", DT_STRING, 1),
    RSET_HD(RTSP_HDR_AUTHORIZATION, "Authorization", DT_STRING, 1),
    RSET_HD(RTSP_HDR_BANDWIDTH, "Bandwidth", DT_STRING, 1),
    RSET_HD(RTSP_HDR_BLOCKSIZE, "Blocksize", DT_STRING, 1),
    RSET_HD(RTSP_HDR_CACHE_CONTROL, "Cache-Control", DT_STRING, 1),
    RSET_HD(RTSP_HDR_CONFERENCE, "Conference", DT_STRING, 1),
    RSET_HD(RTSP_HDR_CONNECTION, "Connection", DT_STRING, 1),
    RSET_HD(RTSP_HDR_CONTENT_BASE, "Content-Base", DT_STRING, 0),
    RSET_HD(RTSP_HDR_CONTENT_ENCODING, "Content-Encoding", DT_STRING, 1),
    RSET_HD(RTSP_HDR_CONTENT_LANGUAGE, "Content-Language", DT_STRING, 1),
    RSET_HD(RTSP_HDR_CONTENT_LENGTH, "Content-Length", DT_STRING, 0),
    RSET_HD(RTSP_HDR_CONTENT_LOCATION, "Content-Location", DT_STRING, 0),
    RSET_HD(RTSP_HDR_CONTENT_TYPE, "Content-Type", DT_STRING, 0),
    RSET_HD(RTSP_HDR_CSEQ, "Cseq", DT_STRING, 0),
    RSET_HD(RTSP_HDR_DATE, "Date", DT_DATE_1123, 0),
    RSET_HD(RTSP_HDR_EXPIRES, "Expires", DT_DATE_1123, 0),
    RSET_HD(RTSP_HDR_FROM, "From", DT_STRING, 0),
    RSET_HD(RTSP_HDR_IF_MODIFIED_SINCE, "If-Modified-Since", DT_DATE_1123, 0),
    RSET_HD(RTSP_HDR_LAST_MODIFIED, "Last-Modified", DT_DATE_1123, 0),
    RSET_HD(RTSP_HDR_PROXY_AUTHENTICATE, "Proxy-Authenticate", DT_STRING, 0),
    RSET_HD(RTSP_HDR_PROXY_REQUIRE, "Proxy-Require", DT_STRING, 0),
    RSET_HD(RTSP_HDR_PUBLIC, "Public", DT_STRING, 0),
    RSET_HD(RTSP_HDR_RANGE, "Range", DT_STRING, 0),
    RSET_HD(RTSP_HDR_REFERER, "Referer", DT_STRING, 0),
    RSET_HD(RTSP_HDR_REQUIRE, "Require", DT_STRING, 1),
    RSET_HD(RTSP_HDR_RETRY_AFTER, "Retry-After", DT_STRING, 0),
    RSET_HD(RTSP_HDR_RTP_INFO, "RTP-Info", DT_STRING, 1),
    RSET_HD(RTSP_HDR_SCALE, "Scale", DT_STRING, 0),
    RSET_HD(RTSP_HDR_SESSION, "Session", DT_STRING, 0),
    RSET_HD(RTSP_HDR_SERVER, "Server", DT_STRING, 0),
    RSET_HD(RTSP_HDR_SPEED, "Speed", DT_STRING, 0),
    RSET_HD(RTSP_HDR_TRANSPORT, "Transport", DT_STRING, 1),
    RSET_HD(RTSP_HDR_UNSUPPORTED, "Unsupported", DT_STRING, 1),
    RSET_HD(RTSP_HDR_USER_AGENT, "User-Agent", DT_STRING, 0),
    RSET_HD(RTSP_HDR_VIA, "Via", DT_STRING, 1),
    RSET_HD(RTSP_HDR_WWW_AUTHENTICATE, "WWW-Authenticate", DT_STRING, 1),
    /*
     * Supported Extended headers
     */
    RSET_HD(RTSP_HDR_X_PLAY_NOW, "x-playNow", DT_STRING, 1),
    RSET_HD(RTSP_HDR_MAX_RFC_DEFS, "X-RTSP-MAX-RFC-DEFINES", DT_STRING, 1),

    /* The following headers are defined to represent values
     * the exact string specified will not match any where else
     */
    RSET_HD(RTSP_HDR_X_METHOD, "X-RTSP-Method", DT_STRING, 1),
    RSET_HD(RTSP_HDR_X_PROTOCOL, "X-RTSP-Protocol", DT_STRING, 1),
    RSET_HD(RTSP_HDR_X_HOST, "X-RTSP-Host", DT_STRING, 1),
    RSET_HD(RTSP_HDR_X_PORT, "X-RTSP-Port", DT_STRING, 1),
    RSET_HD(RTSP_HDR_X_URL, "X-RTSP-URL", DT_STRING, 1),
    RSET_HD(RTSP_HDR_X_ABS_PATH, "X-RTSP-Abs_Path", DT_STRING, 1),
    RSET_HD(RTSP_HDR_X_VERSION, "X-RTSP-Version", DT_STRING, 1),
    RSET_HD(RTSP_HDR_X_VERSION_NUM, "X-RTSP-Varsion_num", DT_STRING, 1),
    RSET_HD(RTSP_HDR_X_STATUS_CODE, "X-RTSP-Status_code", DT_STRING, 1),
    RSET_HD(RTSP_HDR_X_REASON, "X-RTSP-Reason", DT_STRING, 1),
    RSET_HD(RTSP_HDR_X_QT_LATE_TOLERANCE, "x-transport-options", DT_STRING, 1),
    RSET_HD(RTSP_HDR_X_NKN_REQ_REAL_DEST_IP, "X-NKN-Req-Real-Dest-IP", DT_STRING, 0),
    RSET_HD(RTSP_HDR_X_QUERY, "X-Query", DT_STRING, 0),
};

#define RTSP_HD_MAX (sizeof(rtsp_known_headers)/sizeof(mime_header_descriptor_t))


#define RSET_SD(status_id, i_sc, c_sc, reason_str) \
{\
    .id = status_id,\
    .i_status_code = i_sc,\
    .c_status_code = c_sc,\
    .reason = reason_str,\
    .reason_len = sizeof(reason_str -1),\
    .build_hdr_fn = NULL,\
}

rtsp_status_desc_t rtsp_status_map[RTSP_STATUS_MAX] = {
    RSET_SD(RTSP_STATUS_100,100,"100","Continue"),
    RSET_SD(RTSP_STATUS_200,200,"200","OK"),
    RSET_SD(RTSP_STATUS_201,201,"201","Created"),
    RSET_SD(RTSP_STATUS_250,250,"250","Low on Storage Space"),
    RSET_SD(RTSP_STATUS_300,300,"300","Multiple Choices"),
    RSET_SD(RTSP_STATUS_301,301,"301","Moved Permanently"),
    RSET_SD(RTSP_STATUS_302,302,"302","Moved Temporarily"),
    RSET_SD(RTSP_STATUS_303,303,"303","See Other"),
    RSET_SD(RTSP_STATUS_304,304,"304","Not Modified"),
    RSET_SD(RTSP_STATUS_305,305,"305","Use Proxy"),
    RSET_SD(RTSP_STATUS_400,400,"400","Bad Request"),
    RSET_SD(RTSP_STATUS_401,401,"401","Unauthorized"),
    RSET_SD(RTSP_STATUS_402,402,"402","Payment Required"),
    RSET_SD(RTSP_STATUS_403,403,"403","Forbidden"),
    RSET_SD(RTSP_STATUS_404,404,"404","Not Found"),
    RSET_SD(RTSP_STATUS_405,405,"405","Method Not Allowed"),
    RSET_SD(RTSP_STATUS_406,406,"406","Not Acceptable"),
    RSET_SD(RTSP_STATUS_407,407,"407","Proxy Authentication Required"),
    RSET_SD(RTSP_STATUS_408,408,"408","Request Time-out"),
    RSET_SD(RTSP_STATUS_410,410,"410","Gone"),
    RSET_SD(RTSP_STATUS_411,411,"411","Length Required"),
    RSET_SD(RTSP_STATUS_412,412,"412","Precondition Failed"),
    RSET_SD(RTSP_STATUS_413,413,"413","Request Entity Too Large"),
    RSET_SD(RTSP_STATUS_414,414,"414","Request-URI Too Large"),
    RSET_SD(RTSP_STATUS_415,415,"415","Unsupported Media Type"),
    RSET_SD(RTSP_STATUS_451,451,"451","Parameter Not Understood"),
    RSET_SD(RTSP_STATUS_452,452,"452","Conference Not Found"),
    RSET_SD(RTSP_STATUS_453,453,"453","Not Enough Bandwidth"),
    RSET_SD(RTSP_STATUS_454,454,"454","Session Not Found"),
    RSET_SD(RTSP_STATUS_455,455,"455","Method Not Valid in This State"),
    RSET_SD(RTSP_STATUS_456,456,"456","Header Field Not Valid for Resource"),
    RSET_SD(RTSP_STATUS_457,457,"457","Invalid Range"),
    RSET_SD(RTSP_STATUS_458,458,"458","Parameter Is Read-Only"),
    RSET_SD(RTSP_STATUS_459,459,"459","Aggregate operation not allowed"),
    RSET_SD(RTSP_STATUS_460,460,"460","Only aggregate operation allowed"),
    RSET_SD(RTSP_STATUS_461,461,"461","Unsupported transport"),
    RSET_SD(RTSP_STATUS_462,462,"462","Destination unreachable"),
    RSET_SD(RTSP_STATUS_500,500,"500","Internal Server Error"),
    RSET_SD(RTSP_STATUS_501,501,"501","Not Implemented"),
    RSET_SD(RTSP_STATUS_502,502,"502","Bad Gateway"),
    RSET_SD(RTSP_STATUS_503,503,"503","Service Unavailable"),
    RSET_SD(RTSP_STATUS_504,504,"504","Gateway Time-out"),
    RSET_SD(RTSP_STATUS_505,505,"505","RTSP Version not supported"),
    RSET_SD(RTSP_STATUS_551,551,"551","Option not supported"),
};

#define RTSP_SD_MAX (sizeof(rtsp_status_map)/sizeof(rtsp_status_desc_t))

