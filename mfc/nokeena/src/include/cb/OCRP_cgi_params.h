/*
 * OCRP_cgi_params.h -- OCRP cgi properties
 */
#ifndef _OCRP_CGI_PARAMS_H_
#define _OCRP_CGI_PARAMS_H_

#define OCRP_BASE_DIR "/nkn/tmp/ocrp"
#define OCRP_DIR_MAX_STORAGE_BYTES (20 * 1024 * 1024)

#define POST_TEMPLATE_DATAFILE "/nkn/tmp/ocrp/TMP_POST_XXXXXX"

#define POST_BASEDIR_DATAFILE "/nkn/tmp/ocrp/"
#define POST_DATAFILE_COMPONENT "POST-"
#define POST_BASE_DATAFILE POST_BASEDIR_DATAFILE POST_DATAFILE_COMPONENT

#define POST_BASE_DEBUG_DATAFILE "/nkn/tmp/ocrp/DBG-POST-"
#define POST_SEQNUM_FILE "/nkn/tmp/ocrp/.post_seqnum"
#define POST_CONTENT_TYPE "application/octet-stream"
#define POST_DEBUG_CONTENT_TYPE "application/octet-stream-debug"

#define POST_SCRIPT_NAME_LOAD "load"
#define POST_SCRIPT_NAME_UPDATE "update"

#define POST_MAX_ELEMENTS (64 * 1024)

#define GET_BASE_DEBUG_DATAFILE "/nkn/tmp/ocrp/DBG-GET-"
#define GET_DEBUG_CONTENT_TYPE "text/plain-debug"
#define GET_DBG_SEQNUM_FILE "/nkn/tmp/ocrp/.get_dbg_seqnum"
#define GET_STATUS_CONTENT_FILE "/nkn/tmp/ocrp/GET_STATUS_CONTENT"

#define GET_SCRIPT_NAME_STATUS "get-status"
#define GET_SCRIPT_NAME_ASSET_MAP "get-asset-map"
#define GET_ASSET_MAP_QS_KEY "key="
#define GET_ASSET_MAP_QS_KEY_STRLEN 4

#define HTTP_GET_ASSET_SERVER_IP "127.0.0.1"
#define HTTP_GET_ASSET_SERVER_PORT 8888

#endif /* _OCRP_CGI_PARAMS_H_ */
/*
 * End of OCRP_cgi_params.h 
 */
