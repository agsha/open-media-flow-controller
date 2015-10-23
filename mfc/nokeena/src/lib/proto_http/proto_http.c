/*
 * proto_http.c -- External interface
 */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/param.h>
#include "memalloc.h"
#include "proto_http_int.h"
#include "proto_http/proto_http.h"

extern int proto_http_build_res_common(http_cb_t *phttp, int 
				       status_code, int errcode, 
				       mime_header_t *presponse_hdr);

extern uint64_t glob_tot_get_in_this_second;

extern void *__libc_malloc(size_t size);
extern void *__libc_realloc(void *ptr, size_t size);
extern void *__libc_calloc(size_t n, size_t size);
extern void *__libc_memalign(size_t align, size_t s);
extern void *__libc_valloc(size_t size);
extern void *__libc_pvalloc(size_t size);
extern void  __libc_free(void *ptr);

int *phttp_log_level;
int (*phttp_dbg_log)(int, long, const char *, ...)
		    __attribute__ ((format (printf, 3, 4)));

void *(*phttp_malloc_type)(size_t size, nkn_obj_type_t type);
void *(*phttp_realloc_type)(void *ptr, size_t size, nkn_obj_type_t type);
void *(*phttp_calloc_type)(size_t n, size_t size, nkn_obj_type_t type);
void *(*phttp_memalign_type)(size_t align, size_t s, nkn_obj_type_t type);
void *(*phttp_valloc_type)(size_t size, nkn_obj_type_t type);
void *(*phttp_pvalloc_type)(size_t size, nkn_obj_type_t type);
char *(*phttp_strdup_type)(const char *s, nkn_obj_type_t type);
void (*phttp_free)(void *ptr);

typedef struct date_str {
    int length;
    char date[128];
} date_str_t;

static time_t nkn_cur_ts;
static date_str_t cur_datestr_array[2];
static volatile int cur_datestr_idx = 1;
static pthread_t timer_1sec_thread_id;
static int phttp_default_log_level = PH_MSG3;

// Global proto HTTP data
char myhostname[1024];
int myhostname_len;
int max_get_rate_limit;

const char *response_200_OK_body = 
	"<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">"
	"<HTML>"
	    "<HEAD>"
	        "<TITLE>200 OK</TITLE>"
		"<BODY>200 OK</BODY>"
	    "</HEAD>"
	"</HTML>";
int sizeof_response_200_OK_body;

int proto_http_dbg_log(int level, long module, const char *fmt, ...)
		    __attribute__ ((format (printf, 3, 4)));
int proto_http_dbg_log(int level, long module, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    printf("[Level=%d Module=0x%lx]:", level, module);
    vprintf(fmt, ap);
    printf("\n");
    return 0;
}

static
void *proto_http_malloc_type(size_t size, nkn_obj_type_t type)
{
    UNUSED_ARGUMENT(type);
    return __libc_malloc(size);
}

static
void *proto_http_realloc_type(void *ptr, size_t size, nkn_obj_type_t type)
{
    UNUSED_ARGUMENT(type);
    return __libc_realloc(ptr, size);
}

static
void *proto_http_calloc_type(size_t nmemb, size_t size, nkn_obj_type_t type)
{
    UNUSED_ARGUMENT(type);
    return __libc_calloc(nmemb, size);
}

static
void *proto_http_memalign_type(size_t align, size_t s, nkn_obj_type_t type)
{
    UNUSED_ARGUMENT(type);
    return __libc_memalign(align, s);
}

static
void *proto_http_valloc_type(size_t size, nkn_obj_type_t type)
{
    UNUSED_ARGUMENT(type);
    return __libc_valloc(size);
}

static
void *proto_http_pvalloc_type(size_t size, nkn_obj_type_t type)
{
    UNUSED_ARGUMENT(type);
    return __libc_pvalloc(size);
}

static
char *proto_http_strdup_type(const char *s, nkn_obj_type_t type)
{
    size_t len;
    char *new_str;

    len = strlen(s) + 1;
    new_str = proto_http_malloc_type(len, type);
    memcpy(new_str, s, len);

    return new_str;
}

static
void proto_http_free(void *ptr)
{
    return __libc_free(ptr);
}


static
void *timer_1sec(void *arg)
{
    UNUSED_ARGUMENT(arg);

    prctl(PR_SET_NAME, "phttp-1sec", 0, 0, 0);

    int next_datestr_idx;
    date_str_t *next;

    while (1) { // Begin while

    if (cur_datestr_idx) {
	next_datestr_idx = 0;
    } else {
	next_datestr_idx = 1;
    }
    next = &cur_datestr_array[next_datestr_idx];
    time(&nkn_cur_ts);
    mk_rfc1123_time(&nkn_cur_ts, next->date, sizeof(cur_datestr_array[0].date),
		    &next->length);
    cur_datestr_idx = next_datestr_idx;

    // For HTTP GET attack counters
    glob_tot_get_in_this_second = 0;

    sleep(1);

    } // End while
    return NULL;
}

char *nkn_get_datestr(int *datestr_len)
{
    date_str_t *cur = &cur_datestr_array[cur_datestr_idx];
    if (datestr_len) {
	*datestr_len = cur->length;
    }
    return cur->date;
}

int proto_http_init(const proto_http_config_t *cfg)
{
    int rv;

    if (!cfg) {
    	return 1;
    }
    if (cfg->interface_version != PROTO_HTTP_INTF_VERSION) {
    	return 2;
    }
    if (cfg->log_level) {
    	phttp_log_level = cfg->log_level;
    } else {
    	phttp_log_level = &phttp_default_log_level;
    }

    if (cfg->logfunc) {
    	phttp_dbg_log = (int (*)(int, long, const char *, ...))cfg->logfunc;
    } else {
    	phttp_dbg_log = proto_http_dbg_log;
    }

    if (cfg->proc_malloc_type) {
    	phttp_malloc_type = 
	    (void *(*)(size_t, nkn_obj_type_t))cfg->proc_malloc_type;
    } else {
    	phttp_malloc_type = proto_http_malloc_type;
    }

    if (cfg->proc_realloc_type) {
    	phttp_realloc_type = 
	    (void *(*)(void *, size_t, nkn_obj_type_t))cfg->proc_realloc_type;
    } else {
    	phttp_realloc_type = proto_http_realloc_type;
    }

    if (cfg->proc_calloc_type) {
    	phttp_calloc_type = 
	    (void *(*)(size_t, size_t, nkn_obj_type_t))cfg->proc_calloc_type;
    } else {
    	phttp_calloc_type = proto_http_calloc_type;
    }

    if (cfg->proc_memalign_type) {
    	phttp_memalign_type = 
	    (void *(*)(size_t, size_t, nkn_obj_type_t))cfg->proc_memalign_type;
    } else {
    	phttp_memalign_type = proto_http_memalign_type;
    }

    if (cfg->proc_valloc_type) {
    	phttp_valloc_type = 
	    (void *(*)(size_t, nkn_obj_type_t))cfg->proc_valloc_type;
    } else {
    	phttp_valloc_type = proto_http_valloc_type;
    }

    if (cfg->proc_pvalloc_type) {
    	phttp_pvalloc_type = 
	    (void *(*)(size_t, nkn_obj_type_t))cfg->proc_pvalloc_type;
    } else {
    	phttp_pvalloc_type = proto_http_pvalloc_type;
    }

    if (cfg->proc_strdup_type) {
    	phttp_strdup_type = 
	    (char *(*)(const char *, nkn_obj_type_t))cfg->proc_strdup_type;
    } else {
    	phttp_strdup_type = proto_http_strdup_type;
    }

    if (cfg->proc_free) {
    	phttp_free = cfg->proc_free;
    } else {
    	phttp_free = proto_http_free;
    }

    mime_hdr_startup();
    gethostname(myhostname, sizeof(myhostname)-1);
    myhostname_len = strlen(myhostname);
    sizeof_response_200_OK_body = strlen(response_200_OK_body);

    if (cfg->options & OPT_DETECT_GET_ATTACK) {
    	max_get_rate_limit = 300000;
    } else {
    	max_get_rate_limit = INT_MAX;
    }

    rv = pthread_create(&timer_1sec_thread_id, 0, timer_1sec, 0);
    if (rv) {
        return 3;
    }

    return 0;
}

token_data_t NewTokenData()
{
    http_cb_t *cb = 0;
    HTTPProtoStatus ret = HP_SUCCESS;
    int rv;

    ////////////////////////////////////////////////////////////////////////////
    while (1) { // Begin while
    ////////////////////////////////////////////////////////////////////////////

    cb = nkn_calloc_type(1, sizeof(*cb), mod_http_cb);
    if (!cb) {
    	ret = HP_MEMALLOC_ERR;
	break;
    }

    rv = mime_hdr_init(&cb->hdr, MIME_PROT_HTTP, 0, 0); // Copy data
    if (rv) {
    	ret = HP_INIT_ERR;
	break;
    }

    rv = mime_hdr_init(&cb->resphdr, MIME_PROT_HTTP, 0, 0); // Copy data
    if (rv) {
    	ret = HP_INIT2_ERR;
	break;
    }
    break;

    ////////////////////////////////////////////////////////////////////////////
    } // End while
    ////////////////////////////////////////////////////////////////////////////

    if (ret != HP_SUCCESS) {
    	if (cb) {
	    deleteHTTPtoken_data((token_data_t)cb);
	    cb = 0;
	}
    }
    return (token_data_t)cb;
}

static
token_data_t ProtoData2TokenData(void *state_data, const proto_data_t *pd,
			     	 int http_request, HTTPProtoStatus *status,
				 int *respcode)
{
    UNUSED_ARGUMENT(state_data);
    http_cb_t *cb = 0;
    HTTPProtoStatus ret = HP_SUCCESS;
    int rv;
    HTTP_PARSE_STATUS parse_status;

    ////////////////////////////////////////////////////////////////////////////
    while (1) { // Begin while
    ////////////////////////////////////////////////////////////////////////////

    if (!pd || !pd->u.HTTP.data || !pd->u.HTTP.datalen) {
    	ret = HP_INVALID_INPUT;
	break;
    }

    cb = nkn_calloc_type(1, sizeof(*cb), mod_http_cb);
    if (!cb) {
    	ret = HP_MEMALLOC_ERR;
	break;
    }

    if (!http_request) {
    	SET_HTTP_FLAG(cb, HRF_HTTP_RESPONSE);
    }

    rv = mime_hdr_init(&cb->hdr, MIME_PROT_HTTP, 0, 0); // Copy data
    if (rv) {
    	ret = HP_INIT_ERR;
	break;
    }

    rv = mime_hdr_init(&cb->resphdr, MIME_PROT_HTTP, 0, 0); // Copy data
    if (rv) {
    	ret = HP_INIT2_ERR;
	break;
    }

    cb->remote_port = pd->u.HTTP.destPort;
    cb->cb_buf = pd->u.HTTP.data;
    cb->cb_max_buf_size = pd->u.HTTP.datalen;
    cb->cb_totlen = pd->u.HTTP.datalen;

    parse_status = http_parse_header(cb);
    switch (parse_status) {
    case HPS_OK:
    	break;
    case HPS_ERROR:
    	ret = HP_PARSE_ERR;
	break;

    case HPS_NEED_MORE_DATA:
    	ret = HP_MORE_DATA;
	break;

    default:
    	ret = HP_INTERNAL_ERR;
	break;
    }
    break;
    ////////////////////////////////////////////////////////////////////////////
    } // End while
    ////////////////////////////////////////////////////////////////////////////

    if (status) {
    	*status = ret;
	if (!http_request) {
	    if (respcode && cb) {
	    	*respcode = cb->respcode;
	    }
	}
    }
    if (ret != HP_SUCCESS) {
    	if (cb) {
	    deleteHTTPtoken_data((token_data_t)cb);
	    cb = 0;
	}
    }
    return (token_data_t)cb;
}

token_data_t HTTPProtoData2TokenData(void *state_data, const proto_data_t *pd,
				     HTTPProtoStatus *status)
{
    return ProtoData2TokenData(state_data, pd, 1, status, 0);
}

token_data_t HTTPProtoRespData2TokenData(void *state_data, 
					 const proto_data_t *pd,
				     	 HTTPProtoStatus *status,
					 int *http_respcode)
{
    return ProtoData2TokenData(state_data, pd, 0, status, http_respcode);
}

void deleteHTTPtoken_data(token_data_t td)
{
    http_cb_t *cb;

    if (!td) {
    	return;
    }

    cb = (http_cb_t *)td;
    if (cb->resp_buf) {
	free(cb->resp_buf);
	cb->resp_buf = 0;
	cb->res_hdlen = 0;
    }

    if (cb->req_cb_buf) {
    	free(cb->req_cb_buf);
    	cb->req_cb_buf = 0;
	cb->req_cb_totlen = 0;
    }
    mime_hdr_shutdown(&cb->hdr);
    mime_hdr_shutdown(&cb->resphdr);
    free(cb);
}

int HTTPHeaderBytes(token_data_t td)
{
    http_cb_t *cb;

    if (!td) {
    	return 0;
    }
    cb = (http_cb_t *)td;

    return cb->http_hdr_len;
}

#define MIMEHDR(_cb, _request_or_response) \
    ((_request_or_response) ? &(_cb)->resphdr :  &(_cb)->hdr)
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
ProtoHTTPHeaderId HTTPHdrStrToHdrToken(const char *str, int len)
{
    int rv;
    int enum_header;

    rv = mime_hdr_strtohdr(MIME_PROT_HTTP, str, len, &enum_header);
    if (!rv) {
    	return (ProtoHTTPHeaderId)enum_header;
    } else {
    	return (ProtoHTTPHeaderId)-1;
    }
}

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
		       ProtoHTTPHeaderId token, const char *val, int len)
{
    http_cb_t *cb;
    mime_header_t *hdr;

    if (!td) {
    	return 1000;
    }
    cb = (http_cb_t *)td;
    hdr = MIMEHDR(cb, request_or_response);

    return mime_hdr_add_known(hdr, (int)token, val, len);
}

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
		       int *header_cnt)
{
    http_cb_t *cb;
    mime_header_t *hdr;
    u_int32_t attr;

    if (!td) {
    	return 1000;
    }
    cb = (http_cb_t *)td;
    hdr = MIMEHDR(cb, request_or_response);

    return mime_hdr_get_known(hdr, (int)token, val, vallen, &attr, header_cnt);
}

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
			  const char **val, int *vallen)
{
    http_cb_t *cb;
    mime_header_t *hdr;
    u_int32_t attr;

    if (!td) {
    	return 1000;
    }
    cb = (http_cb_t *)td;
    hdr = MIMEHDR(cb, request_or_response);

    return mime_hdr_get_nth_known(hdr, (int)token, header_num, val, vallen, 
				  &attr);
}

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
		       	  ProtoHTTPHeaderId token)
{
    http_cb_t *cb;
    mime_header_t *hdr;

    if (!td) {
    	return 1000;
    }
    cb = (http_cb_t *)td;
    hdr = MIMEHDR(cb, request_or_response);

    return mime_hdr_del_known(hdr, (int)token);
}

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
			 const char *val, int vallen)
{
    http_cb_t *cb;
    mime_header_t *hdr;

    if (!td) {
    	return 1000;
    }
    cb = (http_cb_t *)td;
    hdr = MIMEHDR(cb, request_or_response);

    return mime_hdr_add_unknown(hdr, name, namelen, val, vallen);
}

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
			 const char **val, int *vallen)
{
    http_cb_t *cb;
    mime_header_t *hdr;
    u_int32_t attr;

    if (!td) {
    	return 1000;
    }
    cb = (http_cb_t *)td;
    hdr = MIMEHDR(cb, request_or_response);

    return mime_hdr_get_unknown(hdr, name, namelen, val, vallen, &attr);
}

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
			    const char **val, int *vallen)
{
    http_cb_t *cb;
    mime_header_t *hdr;
    u_int32_t attr;

    if (!td) {
    	return 1000;
    }
    cb = (http_cb_t *)td;
    hdr = MIMEHDR(cb, request_or_response);

    return mime_hdr_get_nth_unknown(hdr, header_num, name, namelen, 
				    val, vallen, &attr);
}

/*
 * HTTPGetQueryArg() -- Get nth query string arg
 *
 *  Return:
 *   ==0, Success
 *   !=0, Error
 */
int HTTPGetQueryArg(token_data_t td, int arg_num,
		    const char **name, int *name_len,
		    const char **val, int *val_len)
{
    http_cb_t *cb;
    mime_header_t *hdr;

    if (!td) {
    	return 1000;
    }

    cb = (http_cb_t *)td;
    hdr = MIMEHDR(cb, 0);

    return mime_hdr_get_query_arg(hdr, arg_num, name, name_len, val, val_len);
}

/*
 * HTTPGetQueryArgbyName() -- Get nth query string arg by name
 *
 *  Return:
 *   ==0, Success
 *   !=0, Error
 */
int HTTPGetQueryArgbyName(token_data_t td, const char *name, int name_len,
			  const char **val, int *val_len)
{
    http_cb_t *cb;
    mime_header_t *hdr;

    if (!td) {
    	return 1000;
    }

    cb = (http_cb_t *)td;
    hdr = MIMEHDR(cb, 0);

    return mime_hdr_get_query_arg_by_name(hdr, name, name_len, val, val_len);
}

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
			    const char *name, int namelen)
{
    http_cb_t *cb;
    mime_header_t *hdr;

    if (!td) {
    	return 1000;
    }
    cb = (http_cb_t *)td;
    hdr = MIMEHDR(cb, request_or_response);

    return mime_hdr_del_unknown(hdr, name, namelen);
}

/*
 * HTTPisKeepAliveReq() -- Is a keepalive connection request?
 *
 *  Return:
 *   ==0, False
 *   !=0, True
 */
int HTTPisKeepAliveReq(token_data_t td)
{
    http_cb_t *cb;

    if (!td) {
    	return 1000;
    }
    cb = (http_cb_t *)td;

    return CHECK_HTTP_FLAG(cb, HRF_CONNECTION_KEEP_ALIVE);
}

/*
 * HTTPProtoReqOptions() -- Return HTTP protocol request options.
 *
 *  Return:
 *       Defined options bit mask.
 */

#define ALLOWED_OPTIONS \
	(HRF_HTTP_09|HRF_HTTP_10|HRF_HTTP_11| \
         HRF_METHOD_GET|HRF_METHOD_POST|HRF_METHOD_HEAD|HRF_METHOD_CONNECT| \
         HRF_METHOD_PUT|HRF_METHOD_DELETE|HRF_METHOD_TRACE| \
         HRF_HTTPS_REQ)

uint64_t HTTPProtoReqOptions(token_data_t td)
{
    http_cb_t *cb;

    if (!td) {
    	return 0;
    }
    cb = (http_cb_t *)td;
    return cb->flag & ALLOWED_OPTIONS;
}

/*
 * HTTPProtoSetReqOptions() -- Set HTTP protocol request options.
 */
void HTTPProtoSetReqOptions(token_data_t td, uint64_t options)
{
    http_cb_t *cb;

    if (!td) {
    	return;
    }
    cb = (http_cb_t *)td;
    cb->flag &= ~ALLOWED_OPTIONS;
    cb->flag |= (options & ALLOWED_OPTIONS);
}

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
int HTTPRequest(token_data_t td, const char **buf, int *buflen)
{
    http_cb_t *cb;
    mime_header_t *hdr;
    int rv;

    if (!td) {
    	return 1000;
    }

    cb = (http_cb_t *)td;
    rv = build_http_request(cb);
    if (!rv) {
    	*buf = cb->req_cb_buf;
	*buflen = cb->req_cb_totlen;
    }
    return rv;
}

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
 */
int HTTPResponse(token_data_t td, int http_response_code, 
		 const char **buf, int *buflen, int *cur_http_response_code)
{
    http_cb_t *cb;
    int rv;

    if (!td) {
    	return 1000;
    }
    cb = (http_cb_t *)td;

    if (http_response_code > 0) {
    	if (cb->resp_buf) {
	    free(cb->resp_buf);
	    cb->resp_buf = 0;
	    cb->res_hdlen = 0;
	}
    	rv = http_build_res_common(cb, http_response_code, 0, &cb->resphdr);
    }
    if (cur_http_response_code) {
    	*cur_http_response_code = cb->respcode;
    }
    if (cb->resp_buf) {
    	*buf = cb->resp_buf;
    	*buflen = cb->res_hdlen;

	return 0;
    } else {
    	return 2;
    }
}

/*
 *******************************************************************************
 * HTTP User Data Area (UDA) API
 *******************************************************************************
 */

 /*
  * HTTPWriteUDA() -- Write given bytes to UDA.
  *
  * Return:
  *   >=0, Success (bytes written to UDA)
  *   < 0, Error
  */
int HTTPWriteUDA(token_data_t td, char *buf, int buflen)
{
    http_cb_t *cb;
    int bytes_to_write;

    cb = (http_cb_t *)td;
    bytes_to_write = MIN(PH_MAX_UDA_SZ, buflen);

    memcpy(cb->uda, buf, bytes_to_write);
    cb->valid_uda_bytes = bytes_to_write;

    return bytes_to_write;
}

 /*
  * HTTPReadUDA() -- Read UDA bytes.
  *
  * Return:
  *   >=0, Success (bytes read from UDA)
  *   < 0, Error
  */
int HTTPReadUDA(token_data_t td, char *buf, int buflen)
{
    http_cb_t *cb;
    int bytes_to_read;

    cb = (http_cb_t *)td;
    bytes_to_read = MIN(buflen, cb->valid_uda_bytes);

    memcpy(buf, cb->uda, bytes_to_read);

    return bytes_to_read;
}

/*
 *******************************************************************************
 * Conversion functions
 *******************************************************************************
 */

typedef struct colon_header_name {
    const char *name;
    int namelen;
} colon_header_name_t;

#define COLON_H_HOST	0
#define COLON_H_METHOD	1
#define COLON_H_SCHEME	2
#define COLON_H_PATH	3
#define COLON_H_VERSION	4
#define COLON_H_STATUS	5
#define COLON_H_AUTH	6

#define COLON_MAXHDRS	7

static colon_header_name_t colon_headers[7] = {
    {":host", 5},
    {":method", 7},
    {":scheme", 7},
    {":path", 5},
    {":version", 8},
    {":status", 7},
    {":authority", 10},
    {0, 0}
};

static int
add_nv(char *name, int namelen, char *val, int vallen,
       NV_desc_t **nvdata, int *nv_index, int *nv_index_max)
{
#define NV_INITIAL_SIZE 1024
    char *p;

    if (*nv_index >= *nv_index_max) {
    	if (*nv_index_max == 0) {
	    *nv_index_max = NV_INITIAL_SIZE;
	} else {
	    *nv_index_max *= 2;
	}
    	p = nkn_realloc_type(*nvdata, *nv_index_max * sizeof(NV_desc_t),
			     mod_http_nv_buf);
	if (p) {
	    *nvdata = (NV_desc_t *)p;
	} else {
	    free(*nvdata);
	    return 1; // realloc failed
	}
    }
    (*nvdata)[*nv_index].name = name;
    (*nvdata)[*nv_index].namelen = namelen;
    (*nvdata)[*nv_index].val = val;
    (*nvdata)[*nv_index].vallen = vallen;
    (*nv_index)++;

    return 0; // Success
}

static int 
int_HTTPHdr2NV(token_data_t td, int request_or_response, uint64_t flags,
	       char **buf, int *buflen, int *nv_count, int ret_nv_vector)
{

#define INT_ADD_NV(_data, _datalen, _val, _vallen, _func, _line) { \
    int _status; \
    _status = add_nv((char *)(_data), (_datalen), (char *)(_val), (_vallen), \
		     &NV, &NV_index, &NV_index_max); \
    if (_status) { \
    	return _status; \
    } \
}

#define ADD_NV(_data, _datalen, _val, _vallen) { \
    INT_ADD_NV(_data, _datalen, _val, _vallen, __FUNCTION__, __LINE__) \
}

    http_cb_t *cb;
    mime_header_t *hdr;

    int http_status_len;

    const char *data;
    int datalen;
    const char *name;
    int namelen;
    u_int32_t attr;
    int header_cnt;

    int rv;
    int n;
    int nth;

    NV_desc_t *NV = 0;
    int NV_index = 0;
    int NV_index_max = 0;

    int allocbytes;
    char *pdata;
    char **pp;

    cb = (http_cb_t *)td;
    hdr = MIMEHDR(cb, request_or_response);

    if (request_or_response) { // Only request
	/* :host */
	if (!mime_hdr_get_known(hdr, MIME_HDR_HOST, &data, &datalen, 
				&attr, &header_cnt)) {
	    if (flags & FL_2NV_HTTP2_PROTO) {
		/* :authority */
		ADD_NV(colon_headers[COLON_H_AUTH].name, 
		       colon_headers[COLON_H_AUTH].namelen, data, datalen);
	    } else {
		/* :host */
		ADD_NV(colon_headers[COLON_H_HOST].name, 
		       colon_headers[COLON_H_HOST].namelen, data, datalen);
	    }
	}
    }

    /* :method */
    if (request_or_response) { // Only request
	switch (cb->flag & HRF_HTTP_METHODS) {
	case HRF_METHOD_GET:
	    ADD_NV(colon_headers[COLON_H_METHOD].name, 
		   colon_headers[COLON_H_METHOD].namelen, "get", 3);
	    break;
	case HRF_METHOD_POST:
	    ADD_NV(colon_headers[COLON_H_METHOD].name, 
		   colon_headers[COLON_H_METHOD].namelen, "post", 4);
	    break;
	case HRF_METHOD_HEAD:
	    ADD_NV(colon_headers[COLON_H_METHOD].name, 
		   colon_headers[COLON_H_METHOD].namelen, "head", 4);
	    break;
	case HRF_METHOD_CONNECT:
	    ADD_NV(colon_headers[COLON_H_METHOD].name, 
		   colon_headers[COLON_H_METHOD].namelen, "connect", 7);
	    break;
	case HRF_METHOD_OPTIONS:
	    ADD_NV(colon_headers[COLON_H_METHOD].name, 
		   colon_headers[COLON_H_METHOD].namelen, "options", 7);
	    break;
	case HRF_METHOD_PUT:
	    ADD_NV(colon_headers[COLON_H_METHOD].name, 
		   colon_headers[COLON_H_METHOD].namelen, "put", 3);
	    break;
	case HRF_METHOD_DELETE:
	    ADD_NV(colon_headers[COLON_H_METHOD].name, 
		   colon_headers[COLON_H_METHOD].namelen, "delete", 6);
	    break;
	case HRF_METHOD_TRACE:
	    ADD_NV(colon_headers[COLON_H_METHOD].name, 
		   colon_headers[COLON_H_METHOD].namelen, "trace", 5);
	    break;
	default:
	    break;
	}
    }

    /* :scheme */
    if (request_or_response) { // Only request
	if (cb->flag & HRF_HTTPS_REQ) {
	    ADD_NV(colon_headers[COLON_H_SCHEME].name, 
		   colon_headers[COLON_H_SCHEME].namelen, "https", 5);
	} else {
	    ADD_NV(colon_headers[COLON_H_SCHEME].name, 
		   colon_headers[COLON_H_SCHEME].namelen, "http", 4);
	}
    }

    /* :path */
    if (request_or_response) { // Only request
	if (!mime_hdr_get_known(hdr, MIME_HDR_X_NKN_URI, &data, &datalen, 
				&attr, &header_cnt)) {
	    ADD_NV(colon_headers[COLON_H_PATH].name,
		   colon_headers[COLON_H_PATH].namelen, data, datalen);
	}
    }

    /* :version */
    if (request_or_response) { // Only request
	if (cb->flag & HRF_HTTP_11) {
	    ADD_NV(colon_headers[COLON_H_VERSION].name,
		   colon_headers[COLON_H_VERSION].namelen, "http/1.1", 8);
	} else if (cb->flag & HRF_HTTP_10) {
	    ADD_NV(colon_headers[COLON_H_VERSION].name,
		   colon_headers[COLON_H_VERSION].namelen, "http/1.0", 8);
	}
    }

    /* :status */
    if (request_or_response == 0) { // Only response
    	http_status_len = snprintf(cb->http_status_str, 
				   sizeof(cb->http_status_str), "%d", 
				   cb->respcode);
	ADD_NV(colon_headers[COLON_H_STATUS].name,
	       colon_headers[COLON_H_STATUS].namelen, 
	       cb->http_status_str, http_status_len);
    }

    /* Add known headers */
    for (n = 0; n < MIME_HDR_X_NKN_URI; n++) { // Skip all MIME_HDR_X headers
    	if (!hdr->known_header_map[n]) {
	    continue;
	}

    	if (!mime_hdr_get_known(hdr, n, &data, &datalen, &attr, &header_cnt)) {
	    ADD_NV(http_known_headers[n].name, 
		   http_known_headers[n].namelen, data, datalen);

	    for (nth = 1; nth < header_cnt; nth++) {
		rv = get_nth_known_header(hdr, n, nth, &data, &datalen, &attr);
		if (!rv) {
		    ADD_NV(http_known_headers[n].name, 
		   	   http_known_headers[n].namelen, data, datalen);
                }
	    }
	} 
    }

    /* Add unknown headers */
    nth = 0;
    while (!get_nth_unknown_header(hdr, nth, &name, &namelen, 
				   &data, &datalen, &attr)) {
	ADD_NV(name, namelen, data, datalen);
	nth++;
    }

    if (ret_nv_vector) {
    	// Return NV_desc_t list
    	*buf = (char *)NV;
	*buflen = NV_index_max * sizeof(NV_desc_t);
	*nv_count = NV_index;

	return 0;
    }

    /* Compute required memory for NV data */

    allocbytes = (NV_index + 1) * (sizeof(char *) * 2);
    for (n = 0; n < NV_index; n++) {
    	allocbytes += NV[n].namelen + 1;
    	allocbytes += NV[n].vallen + 1;
    }

    /* Pack data into memory block */

    *buflen = roundup(allocbytes, 8);
    pp = nkn_malloc_type(*buflen, mod_http_nv_data);
    if (!pp) {
    	*buflen = 0;
    	free(NV);
	return 2;
    }

    pdata = (char *)&pp[2*(NV_index+1)];
    for (n = 0; n < NV_index; n++) {
	pp[n*2] = pdata;
    	memcpy(pdata, NV[n].name, NV[n].namelen);
	pdata[NV[n].namelen] = '\0';
	pdata += (NV[n].namelen + 1);

	pp[(n*2)+1] = pdata;
    	memcpy(pdata, NV[n].val, NV[n].vallen);
	pdata[NV[n].vallen] = '\0';
	pdata += (NV[n].vallen + 1);
    }
    pp[n*2] = 0;
    pp[(n*2)+1] = 0;

    free(NV);
    *buf = (char *)pp;
    *nv_count = NV_index;

    return 0;
}

/*
 * HTTPHdr2NV() -- Build NV (name, value) representation of HTTP headers
 *
 * Return:
 *  ==0, Success
 *  !=0, Error
 *
 * buf - allocated buffer containing NV
 * buflen - allocated buffer byte count
 * nv_count - number of NV pairs
 *
 * Note: 
 *   1) Returned NV listed terminated with null NV entry.
 *   2) Caller frees the returned "buf" data via free()
 */
int HTTPHdr2NV(token_data_t td, int request_or_response, uint64_t flags,
	       char **buf, int *buflen, int *nv_count)
{
    return int_HTTPHdr2NV(td, request_or_response, flags, buf, buflen,
			  nv_count, 0);
}

/*
 * HTTPHdr2NVdesc() -- Build NV_desc_t representation of HTTP headers
 *
 * Return:
 *  ==0, Success
 *  !=0, Error
 *
 * buf - allocated buffer containing NV_desc_t list
 * buflen - allocated buffer byte count
 * nv_count - number of NV_desc_t elements
 *
 * Note: 
 *   1) Caller frees the returned "buf" data via free()
 */
int HTTPHdr2NVdesc(token_data_t td, int request_or_response, uint64_t flags,
		   NV_desc_t **buf, int *buflen, int *nv_count)
{
    return int_HTTPHdr2NV(td, request_or_response, flags, (char **)buf, buflen, 
			  nv_count, 1);
}

/*
 * End of proto_http.c
 */
