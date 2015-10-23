#include <stdio.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <tcl.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include "stdint.h"
#include "nkn_debug.h"
#include "nkn_defs.h"
#include "server.h"
#include "network.h"
#include "nkn_om.h"
#include "pe_defs.h"
#include "parser_utils.h"

NKNCNT_DEF(pe_parse_err_tcl_script, AO_t, "", "")

NKNCNT_DEF(pe_eval_http_recv_req, AO_t, "", "")
NKNCNT_DEF(pe_eval_http_send_resp, AO_t, "", "")
NKNCNT_DEF(pe_eval_cl_http_recv_req, AO_t, "", "")
NKNCNT_DEF(pe_eval_http_recv_req_co, AO_t, "", "")
NKNCNT_DEF(pe_eval_err_http_recv_req, AO_t, "", "")
NKNCNT_DEF(pe_eval_err_http_send_resp, AO_t, "", "")
NKNCNT_DEF(pe_eval_err_cl_http_recv_req, AO_t, "", "")
NKNCNT_DEF(pe_eval_err_http_recv_req_co, AO_t, "", "")

NKNCNT_DEF(pe_act_http_req_reject_request, AO_t, "", "")
NKNCNT_DEF(pe_act_http_req_url_rewrite, AO_t, "", "")
NKNCNT_DEF(pe_act_http_req_redirect, AO_t, "", "")
NKNCNT_DEF(pe_act_http_req_set_origin_server, AO_t, "", "")
NKNCNT_DEF(pe_act_http_req_set_cache_name, AO_t, "", "")
NKNCNT_DEF(pe_act_http_req_cache_object, AO_t, "", "")
NKNCNT_DEF(pe_act_http_req_no_cache_object, AO_t, "", "")
//NKNCNT_DEF(pe_act_http_req_callout, AO_t, "", "")
NKNCNT_DEF(pe_act_http_req_insert_header, AO_t, "", "")
NKNCNT_DEF(pe_act_http_req_remove_header, AO_t, "", "")
NKNCNT_DEF(pe_act_http_req_modify_header, AO_t, "", "")
NKNCNT_DEF(pe_act_http_req_cache_index_append, AO_t, "", "")
NKNCNT_DEF(pe_act_http_req_transmit_rate, AO_t, "", "")

NKNCNT_DEF(pe_act_http_rsp_insert_header, AO_t, "", "")
NKNCNT_DEF(pe_act_http_rsp_remove_header, AO_t, "", "")
NKNCNT_DEF(pe_act_http_rsp_modify_header, AO_t, "", "")
NKNCNT_DEF(pe_act_http_rsp_set_ip_tos, AO_t, "", "")
NKNCNT_DEF(pe_act_http_rsp_modify_resp_code, AO_t, "", "")

NKNCNT_DEF(pe_err_http_req_get_no_token, AO_t, "", "")
NKNCNT_DEF(pe_err_http_req_get_no_host, AO_t, "", "")
NKNCNT_DEF(pe_err_http_req_get_no_uri, AO_t, "", "")
NKNCNT_DEF(pe_err_http_req_get_no_method, AO_t, "", "")
NKNCNT_DEF(pe_err_http_req_get_unknown_token, AO_t, "", "")

NKNCNT_DEF(pe_wrn_http_req_get_no_cookie, AO_t, "", "")
NKNCNT_DEF(pe_wrn_http_req_get_no_header, AO_t, "", "")
NKNCNT_DEF(pe_wrn_http_req_get_no_query, AO_t, "", "")

NKNCNT_DEF(pe_err_http_req_set_no_action, AO_t, "", "")
NKNCNT_DEF(pe_err_http_req_set_redirect, AO_t, "", "")
NKNCNT_DEF(pe_err_http_req_set_no_cache_object, AO_t, "", "")
NKNCNT_DEF(pe_err_http_req_set_cache_object, AO_t, "", "")
NKNCNT_DEF(pe_err_http_req_set_origin_server, AO_t, "", "")
NKNCNT_DEF(pe_err_http_req_set_url_rewrite, AO_t, "", "")
//NKNCNT_DEF(pe_err_http_req_set_callout, AO_t, "", "")
NKNCNT_DEF(pe_err_http_req_set_ins_hdr_param, AO_t, "", "")
NKNCNT_DEF(pe_err_http_req_set_ins_hdr, AO_t, "", "")
NKNCNT_DEF(pe_err_http_req_set_rmv_hdr_param, AO_t, "", "")
NKNCNT_DEF(pe_err_http_req_set_rmv_hdr, AO_t, "", "")
NKNCNT_DEF(pe_err_http_req_set_mod_hdr_param, AO_t, "", "")
NKNCNT_DEF(pe_err_http_req_set_mod_hdr, AO_t, "", "")
NKNCNT_DEF(pe_err_http_req_set_unknown_action, AO_t, "", "")
NKNCNT_DEF(pe_err_http_req_set_cache_index_append_1, AO_t, "", "")
NKNCNT_DEF(pe_err_http_req_set_cache_index_append_2, AO_t, "", "")
NKNCNT_DEF(pe_err_http_req_set_cache_index_append_3, AO_t, "", "")
NKNCNT_DEF(pe_err_http_req_set_cache_index_append_4, AO_t, "", "")
NKNCNT_DEF(pe_err_http_req_set_transmit_rate_1, AO_t, "", "")
NKNCNT_DEF(pe_err_http_req_set_transmit_rate_2, AO_t, "", "")

NKNCNT_DEF(pe_act_cl_http_req_cluster_redirect, AO_t, "", "")
NKNCNT_DEF(pe_act_cl_http_req_cluster_proxy, AO_t, "", "")

NKNCNT_DEF(pe_err_cl_http_req_get_no_token, AO_t, "", "")
NKNCNT_DEF(pe_err_cl_http_req_get_no_host, AO_t, "", "")
NKNCNT_DEF(pe_err_cl_http_req_get_no_uri, AO_t, "", "")
NKNCNT_DEF(pe_err_cl_http_req_get_no_method, AO_t, "", "")
NKNCNT_DEF(pe_err_cl_http_req_get_unknown_token, AO_t, "", "")
NKNCNT_DEF(pe_err_cl_http_req_set_no_action, AO_t, "", "")
NKNCNT_DEF(pe_err_cl_http_req_set_cluster_redriect, AO_t, "", "")
NKNCNT_DEF(pe_err_cl_http_req_set_cluster_proxy, AO_t, "", "")
NKNCNT_DEF(pe_err_cl_http_req_set_unknown_action, AO_t, "", "")

NKNCNT_DEF(pe_err_http_rsp_get_no_token, AO_t, "", "")
NKNCNT_DEF(pe_err_http_rsp_get_no_host, AO_t, "", "")
NKNCNT_DEF(pe_err_http_rsp_get_no_uri, AO_t, "", "")
NKNCNT_DEF(pe_err_http_rsp_get_no_method, AO_t, "", "")
NKNCNT_DEF(pe_err_http_rsp_get_unknown_token, AO_t, "", "")

NKNCNT_DEF(pe_wrn_http_rsp_get_no_rsp_header, AO_t, "", "")
NKNCNT_DEF(pe_wrn_http_rsp_get_no_cookie, AO_t, "", "")
NKNCNT_DEF(pe_wrn_http_rsp_get_no_header, AO_t, "", "")
NKNCNT_DEF(pe_wrn_http_rsp_get_no_query, AO_t, "", "")

NKNCNT_DEF(pe_err_http_rsp_set_no_action, AO_t, "", "")
NKNCNT_DEF(pe_err_http_rsp_set_ins_hdr_param, AO_t, "", "")
NKNCNT_DEF(pe_err_http_rsp_set_ins_hdr, AO_t, "", "")
NKNCNT_DEF(pe_err_http_rsp_set_rmv_hdr_param, AO_t, "", "")
NKNCNT_DEF(pe_err_http_rsp_set_rmv_hdr, AO_t, "", "")
NKNCNT_DEF(pe_err_http_rsp_set_mod_hdr_param, AO_t, "", "")
NKNCNT_DEF(pe_err_http_rsp_set_mod_hdr, AO_t, "", "")
NKNCNT_DEF(pe_err_http_rsp_set_set_ip_tos, AO_t, "", "")
NKNCNT_DEF(pe_err_http_rsp_set_unknown_action, AO_t, "", "")
NKNCNT_DEF(pe_err_http_rsp_set_mod_resp_code_param, AO_t, "", "")
NKNCNT_DEF(pe_err_http_rsp_set_mod_resp_code, AO_t, "", "")


#if 0
NKNCNT_DEF(pe_err_http_req_co_get_no_token, AO_t, "", "")
NKNCNT_DEF(pe_err_http_req_co_get_no_host, AO_t, "", "")
NKNCNT_DEF(pe_err_http_req_co_get_no_uri, AO_t, "", "")
NKNCNT_DEF(pe_err_http_req_co_get_no_method, AO_t, "", "")
NKNCNT_DEF(pe_err_http_req_co_get_unknown_token, AO_t, "", "")

NKNCNT_DEF(pe_act_http_req_co_ignore_resp, AO_t, "", "")
NKNCNT_DEF(pe_act_http_req_co_reject_request, AO_t, "", "")
NKNCNT_DEF(pe_act_http_req_co_redirect, AO_t, "", "")
NKNCNT_DEF(pe_act_http_req_co_url_rewrite, AO_t, "", "")

NKNCNT_DEF(pe_err_http_req_co_set_no_action, AO_t, "", "")
NKNCNT_DEF(pe_err_http_req_co_set_redirect, AO_t, "", "")
NKNCNT_DEF(pe_err_http_req_co_set_url_rewrite, AO_t, "", "")
NKNCNT_DEF(pe_err_http_req_co_set_unknown_action, AO_t, "", "");
#endif

typedef struct cl_pe_param {
	con_t * pcon;
	int def_proxy;
} cl_pe_param_t;

typedef struct pe_resp_param {
	http_cb_t *phttp;
	mime_header_t *presp_hdr;
	uint64_t pe_action;
} pe_resp_param_t;

/*
 * Local functions
 */
static int pe_http_recv_req_gettokens(ClientData clientData, Tcl_Interp *interp, 
			int objc, Tcl_Obj *CONST objv[]);
int get_cookie_name_value(const char *data, int *datalen, const char **name, int *namelen, int *totlen);
static int parse_range(http_cb_t *phttp, char *data);

static int pe_http_recv_req_gettokens(ClientData clientData, Tcl_Interp *interp, 
			int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj	*res;
	char * token;
	con_t *con;
	int rv;
	u_int32_t attrs;
	int hdrcnt;
	const char *data;
	int datalen;
	int trace_flag = 0;

	if (objc < 2) {
		AO_fetch_and_add1(&glob_pe_err_http_req_get_no_token);
		return TCL_ERROR;
	}
	token = Tcl_GetString(objv[1]);

	res = Tcl_GetObjResult(interp);
	con = (con_t *)clientData;
	trace_flag = CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST);

	/*
	 * Tokens for HTTP structure
	 */
	if(strcasecmp(token, TOKEN_HTTP_REQ_HOST) == 0) {
		rv = get_known_header(&con->http.hdr, MIME_HDR_HOST, 
				&data, &datalen, &attrs, &hdrcnt);
		if (!rv) {
			if (trace_flag) {
				DBG_TRACE("PE getparam %s : %s", token, data);
			}
			Tcl_SetStringObj(res, data, datalen);
			return TCL_OK;
		}
		if (trace_flag) {
			DBG_TRACE("PE getparam %s : Error - Host header not found", token);
		}
		Tcl_SetStringObj(res, "", 0);
		AO_fetch_and_add1(&glob_pe_err_http_req_get_no_host);
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_REQ_URI) == 0) {
		rv = get_known_header(&con->http.hdr, MIME_HDR_X_NKN_URI, 
				&data, &datalen, &attrs, &hdrcnt);
		if (!rv) {
			if (trace_flag) {
				DBG_TRACE("PE getparam %s : %s", token, data);
			}
			Tcl_SetStringObj(res, data, datalen);
			return TCL_OK;
		}
		if (trace_flag) {
			DBG_TRACE("PE getparam %s : Error - URI not found", token);
		}
		Tcl_SetStringObj(res, "", 0);
		AO_fetch_and_add1(&glob_pe_err_http_req_get_no_uri);
		return TCL_OK;
	}
	else if((strcasecmp(token, TOKEN_HTTP_REQ_COOKIE) == 0) && (objc==3)) {
		char * cookie_name;
		int nth;
		const char *pos_s;
		const char *name;
		int namelen;
		int totlen;
		int len;
		
		cookie_name = Tcl_GetString(objv[2]);
		len = strlen(cookie_name);
		rv = get_known_header(&con->http.hdr, MIME_HDR_COOKIE, 
				&data, &datalen, &attrs, &hdrcnt);
		if (!rv) {
			get_cookie_name_value(data, &datalen, &name, &namelen, &totlen);
			if (len == namelen && strncasecmp(name, cookie_name, len)==0) {
				if (trace_flag) {
					DBG_TRACE("PE getparam %s %s : %s", token, cookie_name, name);
				}
				Tcl_SetStringObj(res, name, totlen);
				return TCL_OK;
			}
			/* Extract Cookie name value after ;
			 */
			pos_s = name + totlen + 1;
			while (datalen > 0) {
				get_cookie_name_value(pos_s, &datalen, &name, &namelen, &totlen);
				if (len == namelen && strncasecmp(name, cookie_name, len)==0) {
					if (trace_flag) {
						DBG_TRACE("PE getparam %s %s : %s", token, cookie_name, name);
					}
					Tcl_SetStringObj(res, name, totlen);
					return TCL_OK;
				}
				pos_s = name + totlen + 1;
			}
			for(nth=1; nth<hdrcnt; nth++) {
				rv = get_nth_known_header(&con->http.hdr, MIME_HDR_COOKIE,
						nth, &data, &datalen, &attrs);
				get_cookie_name_value(data, &datalen, &name, &namelen, &totlen);
				if (len == namelen && strncasecmp(name, cookie_name, len)==0) {
					if (trace_flag) {
						DBG_TRACE("PE getparam %s %s : %s", token, cookie_name, name);
					}
					Tcl_SetStringObj(res, name, totlen);
					return TCL_OK;
				}
				pos_s = name + totlen + 1;
				while (datalen > 0) {
					get_cookie_name_value(pos_s, &datalen, &name, &namelen, &totlen);
					if (len == namelen && strncasecmp(name, cookie_name, len)==0) {
						if (trace_flag) {
							DBG_TRACE("PE getparam %s %s : %s", token, cookie_name, name);
						}
						Tcl_SetStringObj(res, name, totlen);
						return TCL_OK;
					}
					pos_s = name + totlen + 1;
				}
			}
		}
		if (trace_flag) {
			DBG_TRACE("PE getparam %s %s : Warning - Cookie not found", token, cookie_name);
		}
		Tcl_SetStringObj(res, "", 0);
		AO_fetch_and_add1(&glob_pe_wrn_http_req_get_no_cookie);
		return TCL_OK;
	}
	else if((strcasecmp(token, TOKEN_HTTP_REQ_HEADER) == 0) && (objc==3)) {
		char * header_name;
		int enum_token;
		int ret;
		char out_buf[4096];
		int len = 0;
		int i;
		int rv1;

		header_name = Tcl_GetString(objv[2]);
		ret = string_to_known_header_enum(header_name, strlen(header_name), &enum_token);
		if(ret == 0) {
			/*
			 * It is a known header.
			 */
			rv = get_known_header(&con->http.hdr, enum_token,
				&data, &datalen, &attrs, &hdrcnt);
			if (!rv && hdrcnt > 1 && datalen < 4096) {
				memcpy(out_buf, data, datalen);
				len = datalen;
				for (i = 1; i < hdrcnt; i++) {
					rv1 = get_nth_known_header(&con->http.hdr, enum_token, 
							i, &data, &datalen, &attrs);
					if (!rv1 && ((len+datalen+2) < 4096)) {
						memcpy(out_buf+len, ", ", 2);
						len += 2;
						memcpy(out_buf+len, data, datalen);
						len += datalen;
					}
				}
				data = out_buf;
				datalen = len;
			}
		}
		else {
			/* 
			 * It is a customer header.
			 */
			rv = get_unknown_header(&con->http.hdr, header_name,
				strlen(header_name), &data, &datalen, &attrs);
		}
		if (!rv) {
			if (trace_flag) {
				DBG_TRACE("PE getparam %s %s : %s", token, header_name, data);
			}
			Tcl_SetStringObj(res, data, datalen);
		}
		else {
			if (trace_flag) {
				DBG_TRACE("PE getparam %s %s : Warning - header not found", token, header_name);
			}
			Tcl_SetStringObj(res, "", 0);
			AO_fetch_and_add1(&glob_pe_wrn_http_req_get_no_header);
		}
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_REQ_METHOD) == 0) {
		rv = get_known_header(&con->http.hdr, MIME_HDR_X_NKN_METHOD, 
				&data, &datalen, &attrs, &hdrcnt);
		if (!rv) {
			if (trace_flag) {
				DBG_TRACE("PE getparam %s : %s", token, data);
			}
			Tcl_SetStringObj(res, data, datalen);
			return TCL_OK;
		}
		if (trace_flag) {
			DBG_TRACE("PE getparam %s : Error - Method not found", token);
		}
		Tcl_SetStringObj(res, "", 0);
		AO_fetch_and_add1(&glob_pe_err_http_req_get_no_method);
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_REQ_QUERY) == 0) {
		rv = get_known_header(&con->http.hdr, MIME_HDR_X_NKN_QUERY, 
				&data, &datalen, &attrs, &hdrcnt);
		if (!rv) {
			if (trace_flag) {
				DBG_TRACE("PE getparam %s : %s", token, data);
			}
			Tcl_SetStringObj(res, data, datalen);
		}
		else {
			if (trace_flag) {
				DBG_TRACE("PE getparam %s : Warning - Query not found", token);
			}
			AO_fetch_and_add1(&glob_pe_wrn_http_req_get_no_query);
			Tcl_SetStringObj(res, "", 0);
		}
		return TCL_OK;
	}


	/*
	 * Tokens for network socket
	 */
	else if(strcasecmp(token, TOKEN_SOCKET_SOURCE_IP) == 0) {
		char ip[INET6_ADDRSTRLEN];

		if (con->src_ipv4v6.family == AF_INET) {
			inet_ntop(AF_INET, &IPv4(con->src_ipv4v6), ip, INET6_ADDRSTRLEN);
		}
		else {
			inet_ntop(AF_INET6, &IPv6(con->src_ipv4v6), ip, INET6_ADDRSTRLEN);
		}
		if (trace_flag) {
			DBG_TRACE("PE getparam %s : %s", token, ip);
		}
		Tcl_SetStringObj(res, ip, strlen(ip));
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_SOCKET_SOURCE_PORT) == 0) {
		char port[16];
		snprintf(port, 16, "%d", ntohs(con->src_port));
		if (trace_flag) {
			DBG_TRACE("PE getparam %s : %s", token, port);
		}
		Tcl_SetStringObj(res, port, strlen(port));
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_SOCKET_DEST_IP) == 0) {
		char ip[INET6_ADDRSTRLEN];

		if (con->http.remote_ipv4v6.family == AF_INET) {
			inet_ntop(AF_INET, &IPv4(con->http.remote_ipv4v6), ip, INET6_ADDRSTRLEN);
		}
		else {
			inet_ntop(AF_INET6, &IPv6(con->http.remote_ipv4v6), ip, INET6_ADDRSTRLEN);
		}
		if (trace_flag) {
			DBG_TRACE("PE getparam %s : %s", token, ip);
		}
		Tcl_SetStringObj(res, ip, strlen(ip));
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_SOCKET_DEST_PORT) == 0) {
		char port[16];
		snprintf(port, 16, "%d", ntohs(con->dest_port));
		if (trace_flag) {
			DBG_TRACE("PE getparam %s : %s", token, port);
		}
		Tcl_SetStringObj(res, port, strlen(port));
		return TCL_OK;
	}
	/* URL Category */
	else if(strcasecmp(token, TOKEN_HTTP_REQ_URL_CATEGORY) == 0) {
		char *str = con->uc_inf.category;
		if (trace_flag) {
			DBG_TRACE("PE getparam %s : %s", token, str);
		}
		Tcl_SetStringObj(res, str, strlen(str));
		return TCL_OK;
	}
	/* GeoIP tokens */
	else if (strcasecmp(token, TOKEN_GEO_STATUS_CODE) == 0) {
		char respcode[16];
		snprintf(respcode, 16, "%d", con->ginf.status_code);
		if (trace_flag) {
			DBG_TRACE("PE getparam %s : %s", token, respcode);
		}
		Tcl_SetStringObj(res, respcode, strlen(respcode));
		return TCL_OK;
	}
	else if (strcasecmp(token, TOKEN_GEO_CONTINENT_CODE) == 0) {
		char *str = con->ginf.continent_code;
		if (trace_flag) {
			DBG_TRACE("PE getparam %s : %s", token, str);
		}
		Tcl_SetStringObj(res, str, strlen(str));
		return TCL_OK;
	}
	else if (strcasecmp(token, TOKEN_GEO_COUNTRY_CODE) == 0) {
		char *str = con->ginf.country_code;
		if (trace_flag) {
			DBG_TRACE("PE getparam %s : %s", token, str);
		}
		Tcl_SetStringObj(res, str, strlen(str));
		return TCL_OK;
	}
	else if (strcasecmp(token, TOKEN_GEO_COUNTRY) == 0) {
		char *str = con->ginf.country;
		if (trace_flag) {
			DBG_TRACE("PE getparam %s : %s", token, str);
		}
		Tcl_SetStringObj(res, str, strlen(str));
		return TCL_OK;
	}
	else if (strcasecmp(token, TOKEN_GEO_STATE) == 0) {
		char *str = con->ginf.state;
		if (trace_flag) {
			DBG_TRACE("PE getparam %s : %s", token, str);
		}
		Tcl_SetStringObj(res, str, strlen(str));
		return TCL_OK;
	}
	else if (strcasecmp(token, TOKEN_GEO_CITY) == 0) {
		char *str = con->ginf.city;
		if (trace_flag) {
			DBG_TRACE("PE getparam %s : %s", token, str);
		}
		Tcl_SetStringObj(res, str, strlen(str));
		return TCL_OK;
	}
	else if (strcasecmp(token, TOKEN_GEO_POSTAL_CODE) == 0) {
		char *str = con->ginf.postal_code;
		if (trace_flag) {
			DBG_TRACE("PE getparam %s : %s", token, str);
		}
		Tcl_SetStringObj(res, str, strlen(str));
		return TCL_OK;
	}
	else if (strcasecmp(token, TOKEN_GEO_ISP) == 0) {
		char *str = con->ginf.isp;
		if (trace_flag) {
			DBG_TRACE("PE getparam %s : %s", token, str);
		}
		Tcl_SetStringObj(res, str, strlen(str));
		return TCL_OK;
	}
	else {
		if (trace_flag) {
			DBG_TRACE("PE getparam %s : Error - Unknown token", token);
		}
		AO_fetch_and_add1(&glob_pe_err_http_req_get_unknown_token);
	}

	/* Not defined */
	return TCL_ERROR;
}


/*
 * Convert the the input args in different byte units to 'bytes'.
 * Return 0 for a wrong input,
 *        1 for success
 *        2 for a input in seconds unit 's'
 */
static int
pe_convert_2bytes(char *value, uint64_t *out_bytes)
{
	uint64_t digits = 0;
        char    *units = NULL;
        char    *units_end = NULL;

        if ((value == NULL) || (value[0] == '\0')
            || !(digits = strtoul(value, &units, 10))) {
        	return 1; /* Do nothing */
        }

        while (isspace(units[0])) units++ ; /* Skip any whitespaces */
        units_end = units ;
        /* Find the end of units string and terminate with a '\0'*/
        while (units_end[0] && !isspace(units_end[0])) units_end++ ;
        units_end[0] = '\0' ;

        if (units[0] == 's') { // seconds
		*out_bytes = digits ;
                return 2;
        }

        if (units[0] == 'b') {
		*out_bytes = digits / 8 ;
                return 1;
        } else if (units[0] == 'B') {
		*out_bytes = digits;
                return 1;
        } 
        if (units[1] == '\0') return 0;

    	if (units[0] == 'k' || units[0] == 'K' ) {
		digits *= 0x400 ;
    	} else if (units[0] == 'm' || units[0] == 'M' ) {
		digits *= 0x100000 ;
    	} else if (units[0] == 'g' || units[0] == 'G' ) {
		digits *= 0x40000000 ;
    	} else if (units[0] == 't' || units[0] == 'T' ) {
		digits *= 0x10000000000 ;
        } else {
		return 0;
        }

        if (units[1] == 'b') {
      		digits /= 8 ;
        } else if (units[1] != 'B') {
          	return 0;
        }


        *out_bytes = digits ;
        return 1;
}

/*
 * Convert the the input args in different byte units to 'bytes'.
 * This will be set to Virtual player fields in con_t structure.
 * 
 *  Return 1 on success.
 */
static int 
pe_set_transmit_rate_args (con_t *con, 
                           pe_transmit_rate_args_t *tcl_args)
{
        int ret = 1;

        // Check if con already has an active set of PE-SSP args set.
        if (con->pe_ssp_args == NULL) {
		// Allocate pe_transmit_rate_args_u64_t in con.
		con->pe_ssp_args = nkn_calloc_type(1, sizeof(pe_transmit_rate_args_u64_t), 
					           mod_pe_transmit_rate_t);
                if (con->pe_ssp_args == NULL) {
			return 0;
                }
        }

        // Save the current value of con-args to be used in the case
        //      where the PE argument value is NULL.
        con->pe_ssp_args->max_bw 	= 0;
        con->pe_ssp_args->bit_rate   	= 0;
        con->pe_ssp_args->fast_start_sz = 0;
        con->pe_ssp_args->fast_start_unit_sec = 0;

        if(!pe_convert_2bytes(tcl_args->pe_max_bw, &con->pe_ssp_args->max_bw)) {
        	return 0;
        }
        if(!pe_convert_2bytes(tcl_args->pe_bit_rate, &con->pe_ssp_args->bit_rate)) {
        	return 0;
        }
        if(!(ret = pe_convert_2bytes(tcl_args->pe_fast_start, 
				     &con->pe_ssp_args->fast_start_sz))) {
        	return 0;
        } else if (ret == 2) {
		con->pe_ssp_args->fast_start_unit_sec = 1;
	}

        return 1;
}

static int pe_http_recv_setaction(ClientData clientData, Tcl_Interp *interp, 
			int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj	*res;
	char * token;
	con_t *con;
	int trace_flag = 0;
	u_int32_t attrs = 0;
	int hdrcnt = 0;

	if (objc < 2) {
		AO_fetch_and_add1(&glob_pe_err_http_req_set_no_action);
		return TCL_ERROR;
	}
	token = Tcl_GetString(objv[1]);

	res = Tcl_GetObjResult(interp);
	con = (con_t *)clientData;
	trace_flag = CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST);
	Tcl_SetStringObj(res, "true", 4);
	
	/*
	 * Tokens for HTTP structure
	 */
	if(strcasecmp(token, ACTION_REJECT_REQUEST) == 0) {
		SET_ACTION(con, PE_ACTION_REJECT_REQUEST);
		if (objc >= 3) {
			int code;
			char *code_str;

			code_str = Tcl_GetString(objv[2]);
			if (strncasecmp(code_str, "geoip", 5) == 0) {
				code = NKN_PE_GEOIP_REJECT;
			}
			else if (strncasecmp(code_str, "ucflt", 5) == 0) {
				code = NKN_PE_UCFLT_REJECT;
			}
			else {
				code = NKN_PE_REJECT;
			}
			if (trace_flag) {
				DBG_TRACE("PE setaction %s, Sub error code %s (%d)", 
					token, code_str, code);
			}
			con->http.subcode = code;
		}
		else {
			if (trace_flag) {
				DBG_TRACE("PE setaction %s", token);
			}
			con->http.subcode = NKN_PE_REJECT;
		}
		AO_fetch_and_add1(&glob_pe_act_http_req_reject_request);
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_REDIRECT) == 0) {
		char * uri;

		if (objc < 3) {
			if (trace_flag) {
				DBG_TRACE("PE setaction %s : Error - No URI", token);
			}
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_http_req_set_redirect);
			return TCL_ERROR;
		}

		uri = Tcl_GetString(objv[2]);

		SET_ACTION(con, PE_ACTION_REDIRECT);
		add_known_header(&con->http.hdr, MIME_HDR_LOCATION, uri, strlen(uri));

		if (objc >= 4) {
			int code;
			char *code_str;

			code_str = Tcl_GetString(objv[3]);
			if (strncasecmp(code_str, "geoip", 5) == 0) {
				code = NKN_PE_GEOIP_REDIRECT;
			}
			else if (strncasecmp(code_str, "ucflt", 5) == 0) {
				code = NKN_PE_UCFLT_REDIRECT;
			}
			else {
				code = NKN_PE_REDIRECT;
			}
			if (trace_flag) {
				DBG_TRACE("PE setaction %s : %s, Sub error code %s (%d)", 
					token, uri, code_str, code);
			}
			con->http.subcode = code;
		}
		else {
			if (trace_flag) {
				DBG_TRACE("PE setaction %s : %s", token, uri);
			}
			con->http.subcode = NKN_PE_REDIRECT;
		}
		AO_fetch_and_add1(&glob_pe_act_http_req_redirect);
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_NO_CACHE_OBJECT) == 0) {
		if (CHECK_HTTP_FLAG(&con->http, HRF_METHOD_GET)) {
			SET_ACTION(con, PE_ACTION_REQ_NO_CACHE_OBJECT);
			CLEAR_ACTION(con, PE_ACTION_REQ_CACHE_OBJECT);
			add_known_header(&con->http.hdr, MIME_HDR_X_NKN_CACHE_POLICY, "no_cache", 8);

			if (trace_flag) {
				DBG_TRACE("PE setaction %s", token);
			}
			AO_fetch_and_add1(&glob_pe_act_http_req_no_cache_object);
			return TCL_OK;
		}
		if (trace_flag) {
			DBG_TRACE("PE setaction %s : Error - Non GET method", token);
		}
		Tcl_SetStringObj(res, "false", 5);
		AO_fetch_and_add1(&glob_pe_err_http_req_set_no_cache_object);
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_CACHE_OBJECT) == 0) {
		if (CHECK_HTTP_FLAG(&con->http, HRF_METHOD_GET)) {
			SET_ACTION(con, PE_ACTION_REQ_CACHE_OBJECT);
			CLEAR_ACTION(con, PE_ACTION_REQ_NO_CACHE_OBJECT);
			add_known_header(&con->http.hdr, MIME_HDR_X_NKN_CACHE_POLICY, "cache", 5);

			if (trace_flag) {
				DBG_TRACE("PE setaction %s", token);
			}
			AO_fetch_and_add1(&glob_pe_act_http_req_cache_object);
			return TCL_OK;
		}
		if (trace_flag) {
			DBG_TRACE("PE setaction %s : Error - Non GET method", token);
		}
		Tcl_SetStringObj(res, "false", 5);
		AO_fetch_and_add1(&glob_pe_err_http_req_set_cache_object);
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_SET_ORIGIN_SERVER) == 0) {
		char * uri;

		if (objc != 3) {
			if (trace_flag) {
				DBG_TRACE("PE setaction %s : Error - No server", token);
			}
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_http_req_set_origin_server);
			return TCL_ERROR;
		}

		uri = Tcl_GetString(objv[2]);

		SET_ACTION(con, PE_ACTION_SET_ORIGIN_SERVER);
		add_known_header(&con->http.hdr, MIME_HDR_X_NKN_ORIGIN_SVR, uri, strlen(uri));

		if (trace_flag) {
			DBG_TRACE("PE setaction %s %s", token, uri);
		}
		AO_fetch_and_add1(&glob_pe_act_http_req_set_origin_server);
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_URL_REWRITE) == 0) {
		char * domain;
		char * uri;
		int hit = 0;
		
		if (objc != 4) {
			if (trace_flag) {
				DBG_TRACE("PE setaction %s : Error - Required parameters missing", token);
			}
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_http_req_set_url_rewrite);
			return TCL_ERROR;
		}

		domain = Tcl_GetString(objv[2]);
		uri = Tcl_GetString(objv[3]);

		SET_ACTION(con, PE_ACTION_URL_REWRITE);
		if (strlen(domain) > 4) {
			delete_known_header(&con->http.hdr, MIME_HDR_HOST);
			add_known_header(&con->http.hdr, MIME_HDR_HOST, domain, strlen(domain));
			SET_CON_FLAG(con, CONF_PE_HOST_HDR);
			hit = 1;
		}
		if (strlen(uri) > 0) {
			if (uri[0] != '/') {
				if (trace_flag) {
					DBG_TRACE("PE setaction %s %s %s : Error - URL should start with /", token, domain, uri);
				}
				Tcl_SetStringObj(res, "false", 5);
				AO_fetch_and_add1(&glob_pe_err_http_req_set_url_rewrite);
				return TCL_OK;
			}
			delete_known_header(&con->http.hdr, MIME_HDR_X_NKN_URI);
			delete_known_header(&con->http.hdr, MIME_HDR_X_NKN_DECODED_URI);
			delete_known_header(&con->http.hdr, MIME_HDR_X_NKN_QUERY);
			CLEAR_HTTP_FLAG(&con->http, HRF_HAS_HEX_ENCODE);
			CLEAR_HTTP_FLAG(&con->http, HRF_HAS_QUERY);
			if (http_parse_uri_internal(&con->http, uri, CHECK_ACTION(con, PE_ACTION_URL_REWRITE))) {
				if (trace_flag) {
					DBG_TRACE("PE setaction %s %s %s : Error - Parse error in URL", token, domain, uri);
				}
				Tcl_SetStringObj(res, "false", 5);
				AO_fetch_and_add1(&glob_pe_err_http_req_set_url_rewrite);
				return TCL_OK;
			}
			hit = 1;
		}
		if (hit) {
			if (trace_flag) {
				DBG_TRACE("PE setaction %s %s %s", token, domain, uri);
			}
			AO_fetch_and_add1(&glob_pe_act_http_req_url_rewrite);
		}
		else {
			if (trace_flag) {
				DBG_TRACE("PE setaction %s %s %s : Error - In parameter", token, domain, uri);
			}
			Tcl_SetStringObj(res, "false", 5);
		}
		return TCL_OK;
	} else if(strcasecmp(token, ACTION_CACHE_INDEX_APPEND) == 0) {
		char * index_2 = NULL;
		const char * uri = NULL;
    		int    uri_len = 0 ;
   		char   new_uri[2 * MAX_URI_SIZE] = {0,} ;
    		int    new_uri_len = 0 ;
		int    rv = 0;
		
		if (objc != 3) {
			if (trace_flag) {
				DBG_TRACE("PE setaction %s : Error - Required parameters missing", token);
			}
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_http_req_set_cache_index_append_1);
			return TCL_ERROR;
		}

		index_2 = Tcl_GetString(objv[2]);

		SET_ACTION(con, PE_ACTION_CACHE_INDEX_APPEND);

                rv = get_known_header(&con->http.hdr, MIME_HDR_X_NKN_URI,
                                      &uri, &uri_len, &attrs, &hdrcnt);

		if (!rv && (uri_len > 0)) {
			if (uri[0] != '/') {
				if (trace_flag) {
					DBG_TRACE("PE setaction %s %s : Error - URL should start with /", 
                                                  token, uri);
				}
				Tcl_SetStringObj(res, "false", 5);
				AO_fetch_and_add1(&glob_pe_err_http_req_set_cache_index_append_2);
				return TCL_OK;
			}

                        // Append the new string to the current URI.
                        new_uri_len = snprintf(new_uri, sizeof(new_uri), "%s.%s", uri, index_2) ;
                   
                        if (new_uri_len > 0) {
		                add_known_header(&con->http.hdr, MIME_HDR_X_NKN_REMAPPED_URI,
                                                 new_uri, new_uri_len);
                        } else { 
				if (trace_flag) {
					DBG_TRACE("PE setaction %s %s", token, uri);
				}
				Tcl_SetStringObj(res, "false", 5);
				AO_fetch_and_add1(&glob_pe_err_http_req_set_cache_index_append_3);
                        }
		}
		else {
			if (trace_flag) {
				DBG_TRACE("PE setaction %s %s : Error - In parameter", token, uri);
			}
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_http_req_set_cache_index_append_4);
		}

		AO_fetch_and_add1(&glob_pe_act_http_req_cache_index_append);
		return TCL_OK;

	} else if(strcasecmp(token, ACTION_TRANSMIT_RATE) == 0) {
                pe_transmit_rate_args_t tcl_args = {NULL, NULL, NULL} ;
		
	        // Avoid the case where there are no virtual-player associated
	        //     with the namespace or an empty virtual-player config is attached.
	        if ((con->http.nsconf == NULL)
        	   || (con->http.nsconf->ssp_config == NULL)
	           || (con->http.nsconf->ssp_config->player_name == NULL)) {
			if (trace_flag) {
				DBG_TRACE("PE setaction %s : Virtual-player not configured..", token) ;
			}
			return TCL_OK; /* Do nothing */
		}

		if (objc > 5 || objc < 3) {
			if (trace_flag) {
				DBG_TRACE("PE setaction %s : Error - Invalid number of parameters(%d) given", 
                                          token, objc-2);
			}
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_http_req_set_transmit_rate_1);
			return TCL_ERROR;
		}

                tcl_args.pe_max_bw     = Tcl_GetString(objv[2]);
                tcl_args.pe_bit_rate   = Tcl_GetString(objv[3]);
                tcl_args.pe_fast_start = Tcl_GetString(objv[4]);

		SET_ACTION(con, PE_ACTION_TRANSMIT_RATE);

                // Return 0 when the input when input argument is specified incorrectly.
 		if (!pe_set_transmit_rate_args(con, &tcl_args)) {
			if (trace_flag) {
				DBG_TRACE("PE setaction %s %s %s %s, con->pe_ssp:%s : Error - In parameter", token, 
                                                ((tcl_args.pe_max_bw) ? tcl_args.pe_max_bw: "NULL"),
                                                ((tcl_args.pe_bit_rate) ? tcl_args.pe_bit_rate: "NULL"),
                                                ((tcl_args.pe_fast_start) ? tcl_args.pe_fast_start: "NULL"),
                                                (con->pe_ssp_args ? "PRESENT": "NULL"));
			}
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_http_req_set_transmit_rate_2);
		        CLEAR_ACTION(con, PE_ACTION_TRANSMIT_RATE);
			return TCL_ERROR;
		}
		if (trace_flag) {
			DBG_TRACE("PE setaction %s success: %s %s %s : u64: bw:%lu, bitrate:%lu, faststart:%lu", 
                             token, 
                             ((tcl_args.pe_max_bw) ? tcl_args.pe_max_bw: "NULL"),
                             ((tcl_args.pe_bit_rate) ? tcl_args.pe_bit_rate: "NULL"),
                             ((tcl_args.pe_fast_start) ? tcl_args.pe_fast_start: "NULL"),
                             (con->pe_ssp_args ? con->pe_ssp_args->max_bw : 0),
                             (con->pe_ssp_args ? con->pe_ssp_args->bit_rate : 0),
                             (con->pe_ssp_args ? con->pe_ssp_args->fast_start_sz : 0)
                             );
		}

		AO_fetch_and_add1(&glob_pe_act_http_req_transmit_rate);
		return TCL_OK;
	}
#if 0
	else if(strcasecmp(token, ACTION_CALLOUT) == 0) {
		char * server;
		char * param = NULL;
		policy_srvr_list_t *p_srlist = NULL;
		
		if (objc < 3) {
			AO_fetch_and_add1(&glob_pe_err_http_req_set_callout);
			return TCL_ERROR;
		}

		server = Tcl_GetString(objv[2]);
		if (objc > 3) {
			param = Tcl_GetString(objv[3]);
		}

		/* Check if the given external server name matches
		 * TODO: scan the list of server for the match
		 */
		p_srlist = con->http.nsconf->http_config->policy_engine_config.policy_server_list;
		if (!p_srlist || !p_srlist->policy_srvr || strcasecmp(p_srlist->policy_srvr->name, server) != 0 ) {
			AO_fetch_and_add1(&glob_pe_err_http_req_set_callout);
			return TCL_ERROR;
		}

		SET_ACTION(con, PE_ACTION_CALLOUT);

		/* 
		 * Validate server name with that configured in namespace and 
		 * the param type supported for the protocol.
		 */
		/* 
		 * store server name and param type
		 */
		// TODO
		pe_co_list_t *p_co, *cur;
		
		if (con->pe_co == NULL) {
			con->pe_co = nkn_malloc_type(sizeof(pe_co_t), mod_pe_co_t);
			con->pe_co->num_co = 0;
			con->pe_co->cur_co = NULL;
			con->pe_co->co_list = NULL;
		}
		p_co = nkn_malloc_type(sizeof(pe_co_list_t), mod_pe_co_list_t);
		p_co->server = nkn_strdup_type(server, mod_pe_co_str);
		p_co->param_type = nkn_strdup_type(param, mod_pe_co_str);
		p_co->next = NULL;
		p_co->co_resp = NULL;
		if (con->pe_co->co_list == NULL) {
			con->pe_co->co_list = p_co;
		}
		else {
			/* For multiple callouts
			 */
			cur = con->pe_co->co_list;
			while (cur->next) {
				cur = cur->next;
			}
			cur->next = p_co;
		}
		con->pe_co->num_co++;
		
		AO_fetch_and_add1(&glob_pe_act_http_req_callout);
		return TCL_OK;
	}
#endif
#if 0
	/* WIP */
	else if(strcasecmp(token, ACTION_SET_CACHE_NAME) == 0) {
		char * uri;

		if (objc != 3) {
			glob_pe_parse_err_tcl_script ++;
			return TCL_ERROR;
		}

		uri = Tcl_GetString(objv[2]);

		SET_ACTION(con, PE_ACTION_SET_CACHE_NAME);
		add_known_header(&con->http.hdr, MIME_HDR_X_NKN_CACHE_NAME, uri, strlen(uri));

		AO_fetch_and_add1(&glob_pe_act_set_cache_name);
		return TCL_OK;
	}
#endif
	if(strcasecmp(token, ACTION_INSERT_HEADER) == 0) {
		char * header_name;
		char * data;
		int enum_token;
		int ret;
		int rv;

		if (objc != 4) {
			if (trace_flag) {
				DBG_TRACE("PE setaction %s : Error - Required parameters missing", token);
			}
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_http_req_set_ins_hdr_param);
			return TCL_ERROR;
		}

		header_name = Tcl_GetString(objv[2]);
		data = Tcl_GetString(objv[3]);

		ret = string_to_known_header_enum(header_name, strlen(header_name), 
				&enum_token);
		rv = 0;
		if (ret == 0) {
			if ((rv = is_known_header_present(&con->http.hdr, enum_token)) == 0) {
				switch (enum_token) {
					case MIME_HDR_HOST:
						SET_CON_FLAG(con, CONF_PE_HOST_HDR);
						rv = add_known_header(&con->http.hdr, 
							MIME_HDR_X_NKN_PE_HOST_HDR, 
							data, strlen(data));
						break;
					case MIME_HDR_RANGE:
						rv = parse_range(&con->http, data);
						break;
					default:
						break;
				}
				if (rv == 0) {
					rv = add_known_header(&con->http.hdr, enum_token, 
						data, strlen(data));
				}
			}
		}
		else {
			if ((rv = is_unknown_header_present(&con->http.hdr, header_name,
					strlen(header_name))) == 0) {
				rv = add_unknown_header(&con->http.hdr, header_name,
					strlen(header_name), data, strlen(data));
			}
		}
		if (rv) {
			if (trace_flag) {
				DBG_TRACE("PE setaction %s %s %s : Error - Unable to insert header", 
					token, header_name, data);
			}
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_http_req_set_ins_hdr);
			return TCL_OK;
		}

		if (trace_flag) {
			DBG_TRACE("PE setaction %s %s %s", token, header_name, data);
		}
		AO_fetch_and_add1(&glob_pe_act_http_req_insert_header); 
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_REMOVE_HEADER) == 0) {
		char * header_name;
		int enum_token;
		int ret;
		int rv;

		if (objc != 3) {
			if (trace_flag) {
				DBG_TRACE("PE setaction %s : Error - Required parameters missing", token);
			}
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_http_req_set_rmv_hdr_param);
			return TCL_ERROR;
		}

		header_name = Tcl_GetString(objv[2]);

		ret = string_to_known_header_enum(header_name, strlen(header_name), 
				&enum_token);
		rv = 0;
		if (ret == 0) {
			rv = delete_known_header(&con->http.hdr, enum_token);
		}
		else {
			rv = delete_unknown_header(&con->http.hdr, header_name,
					strlen(header_name));
		}
		if (rv) {
			if (trace_flag) {
				DBG_TRACE("PE setaction %s %s : Error - Unable to remove header", 
					token, header_name);
			}
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_http_req_set_rmv_hdr);
			return TCL_OK;
		}

		if (trace_flag) {
			DBG_TRACE("PE setaction %s %s", token, header_name);
		}
		AO_fetch_and_add1(&glob_pe_act_http_req_remove_header); 
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_MODIFY_HEADER) == 0) {
		char * header_name;
		char * data;
		int enum_token;
		int ret;
		int rv;

		if (objc != 4) {
			if (trace_flag) {
				DBG_TRACE("PE setaction %s : Error - Required parameters missing", token);
			}
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_http_req_set_mod_hdr_param);
			return TCL_ERROR;
		}

		header_name = Tcl_GetString(objv[2]);
		data = Tcl_GetString(objv[3]);

		ret = string_to_known_header_enum(header_name, strlen(header_name), 
				&enum_token);
		rv = 0;
		if (ret == 0) {
			if (is_known_header_present(&con->http.hdr, enum_token)) {
				rv = delete_known_header(&con->http.hdr, enum_token);
			}
			switch (enum_token) {
				case MIME_HDR_HOST:
					SET_CON_FLAG(con, CONF_PE_HOST_HDR);
					rv = add_known_header(&con->http.hdr, 
						MIME_HDR_X_NKN_PE_HOST_HDR, 
						data, strlen(data));
					break;
				case MIME_HDR_RANGE:
					rv = parse_range(&con->http, data);
					break;
				default:
					break;
			}
			if (rv == 0) {
				rv = add_known_header(&con->http.hdr, enum_token, 
					data, strlen(data));
			}
		}
		else {
			if (is_unknown_header_present(&con->http.hdr, header_name,
					strlen(header_name))) {
				rv = delete_unknown_header(&con->http.hdr, header_name,
						strlen(header_name));
			}
			rv = add_unknown_header(&con->http.hdr, header_name,
				strlen(header_name), data, strlen(data));
		}
		if (rv) {
			if (trace_flag) {
				DBG_TRACE("PE setaction %s %s %s : Error - Unable to modify header", 
					token, header_name, data);
			}
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_http_req_set_mod_hdr);
			return TCL_OK;
		}

		if (trace_flag) {
			DBG_TRACE("PE setaction %s %s %s", token, header_name, data);
		}
		AO_fetch_and_add1(&glob_pe_act_http_req_modify_header); 
		return TCL_OK;
	}
	else {
		if (trace_flag) {
			DBG_TRACE("PE setaction %s : Error - Unknown action", token);
		}
		Tcl_SetStringObj(res, "false", 5);
		AO_fetch_and_add1(&glob_pe_err_http_req_set_unknown_action);
	}

	return TCL_ERROR;
}

uint64_t pe_eval_http_recv_request(pe_rules_t * p_perule, void * pcon)
{
	int ret;
	Tcl_Command tok;
	con_t * con = (con_t *)pcon;

	con->pe_action = PE_ACTION_NO_ACTION;
	if (!PE_CHECK_FLAG(p_perule, PE_EXEC_HTTP_RECV_REQ)) {
		return PE_ACTION_NO_ACTION;
	}
	AO_fetch_and_add1(&glob_pe_eval_http_recv_req);
	
	// Supported variables
	tok = Tcl_CreateObjCommand(p_perule->tip, 
			"getparam", pe_http_recv_req_gettokens,
			(ClientData) con, (Tcl_CmdDeleteProc *) NULL);

	// Supported actions
	tok = Tcl_CreateObjCommand(p_perule->tip, 
			"setaction", pe_http_recv_setaction,
			(ClientData) con, (Tcl_CmdDeleteProc *) NULL);

	if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
		DBG_TRACE("PE pe_http_recv_request");
	}
	
	ret = Tcl_Eval(p_perule->tip, "pe_http_recv_request");
	if ( ret != TCL_OK) {
		DBG_LOG(ERROR, MOD_HTTP, "pe_http_recv_request returns %d\n", ret);
		if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
			DBG_TRACE("PE pe_http_recv_request Error");
		}
		AO_fetch_and_add1(&glob_pe_eval_err_http_recv_req);
		return PE_ACTION_ERROR;
	}
	return con->pe_action;
}

int get_cookie_name_value(const char *data, int *datalen, const char **name, int *namelen, int *totlen) {
	const char *p;
	int i;
	int found_name = 0;
	int qoute = 0;

	p = data;
	while (*datalen && (*p == ' ' || *p =='\t')) {
		*datalen -= 1;
		p++;
	}
	*name = p;
	*namelen = 0;
	*totlen = 0;
	for (i=0; i < *datalen; i++, p++) {
		/* Check for qouted string 
		 */
		if (qoute && *p != '\"' ) {
			continue;
		}
		if (*p == '\"' ) {
			if (qoute) {
				qoute = 0;
			}
			else {
				qoute = 1;
			}
			continue;
		}

		/* Check for name end
		 */
		if (!found_name && (*p == ' ' || *p == '\t' || *p == '=') ) {
			*namelen = i;
			found_name = 1;
		}
		
		/* Check for cookie end
		 */
		if (*p == ';' || *p == ',') {
			*totlen = i;
			*datalen -= 1;
			break;
		}
	}
	*totlen = i;
	*datalen -= i;
	return *namelen ? 0 : 1;
}

static int pe_cl_http_recv_req_gettokens(ClientData clientData, Tcl_Interp *interp, 
			int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj *res;
	char * token;
	con_t *con;
	int rv;
	u_int32_t attrs;
	int hdrcnt;
	const char *data;
	int datalen;
	cl_pe_param_t *param;

	if (objc < 2) {
		AO_fetch_and_add1(&glob_pe_err_cl_http_req_get_no_token);
		return TCL_ERROR;
	}
	token = Tcl_GetString(objv[1]);

	res = Tcl_GetObjResult(interp);
	param = (cl_pe_param_t *)clientData;
	con = param->pcon;

	if(strcasecmp(token, ACTION_CL_DEFALUT_ACTION) == 0) {
		if (param->def_proxy)
			Tcl_SetStringObj(res, ACTION_CLUSTER_PROXY, strlen(ACTION_CLUSTER_PROXY));
		else
			Tcl_SetStringObj(res, ACTION_CLUSTER_REDIRECT, strlen(ACTION_CLUSTER_REDIRECT));
		return TCL_OK;
	}

	/*
	 * Tokens for HTTP structure
	 */
	else if(strcasecmp(token, TOKEN_HTTP_REQ_HOST) == 0) {
		rv = get_known_header(&con->http.hdr, MIME_HDR_HOST, 
				&data, &datalen, &attrs, &hdrcnt);
		if (!rv) {
			Tcl_SetStringObj(res, data, datalen);
			return TCL_OK;
		}
		Tcl_SetStringObj(res, "", 0);
		AO_fetch_and_add1(&glob_pe_err_cl_http_req_get_no_host);
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_REQ_URI) == 0) {
		rv = get_known_header(&con->http.hdr, MIME_HDR_X_NKN_URI, 
				&data, &datalen, &attrs, &hdrcnt);
		if (!rv) {
			Tcl_SetStringObj(res, data, datalen);
			return TCL_OK;
		}
		Tcl_SetStringObj(res, "", 0);
		AO_fetch_and_add1(&glob_pe_err_cl_http_req_get_no_uri);
		return TCL_OK;
	}
	else if((strcasecmp(token, TOKEN_HTTP_REQ_COOKIE) == 0) && (objc==3)) {
		char * cookie_name;
		int nth;
		const char *pos_s;
		const char *name;
		int namelen;
		int totlen;
		int len;
		
		cookie_name = Tcl_GetString(objv[2]);
		len = strlen(cookie_name);
		rv = get_known_header(&con->http.hdr, MIME_HDR_COOKIE, 
				&data, &datalen, &attrs, &hdrcnt);
		if (!rv) {
			get_cookie_name_value(data, &datalen, &name, &namelen, &totlen);
			if (len == namelen && strncasecmp(name, cookie_name, len)==0) {
				Tcl_SetStringObj(res, name, totlen);
				return TCL_OK;
			}
			/* Extract Cookie name value after ;
			 */
			pos_s = name + totlen + 1;
			while (datalen > 0) {
				get_cookie_name_value(pos_s, &datalen, &name, &namelen, &totlen);
				if (len == namelen && strncasecmp(name, cookie_name, len)==0) {
					Tcl_SetStringObj(res, name, totlen);
					return TCL_OK;
				}
				pos_s = name + totlen + 1;
			}
			for(nth=1; nth<hdrcnt; nth++) {
				rv = get_nth_known_header(&con->http.hdr, MIME_HDR_COOKIE,
						nth, &data, &datalen, &attrs);
				get_cookie_name_value(data, &datalen, &name, &namelen, &totlen);
				if (len == namelen && strncasecmp(name, cookie_name, len)==0) {
					Tcl_SetStringObj(res, name, totlen);
					return TCL_OK;
				}
				pos_s = name + totlen + 1;
				while (datalen > 0) {
					get_cookie_name_value(pos_s, &datalen, &name, &namelen, &totlen);
					if (len == namelen && strncasecmp(name, cookie_name, len)==0) {
						Tcl_SetStringObj(res, name, totlen);
						return TCL_OK;
					}
					pos_s = name + totlen + 1;
				}
			}
		}
		Tcl_SetStringObj(res, "", 0);
		return TCL_OK;
	}
	else if((strcasecmp(token, TOKEN_HTTP_REQ_HEADER) == 0) && (objc==3)) {
		char * header_name;
		int enum_token;
		int ret;
		char out_buf[4096];
		int len = 0;
		int i;
		int rv1;

		header_name = Tcl_GetString(objv[2]);
		ret = string_to_known_header_enum(header_name, strlen(header_name), &enum_token);
		if(ret == 0) {
			/*
			 * It is a known header.
			 */
			rv = get_known_header(&con->http.hdr, enum_token,
				&data, &datalen, &attrs, &hdrcnt);
			if (!rv && hdrcnt > 1 && datalen < 4096) {
				memcpy(out_buf, data, datalen);
				len = datalen;
				for (i = 1; i < hdrcnt; i++) {
					rv1 = get_nth_known_header(&con->http.hdr, enum_token, 
							i, &data, &datalen, &attrs);
					if (!rv1 && ((len+datalen+2) < 4096)) {
						memcpy(out_buf+len, ", ", 2);
						len += 2;
						memcpy(out_buf+len, data, datalen);
						len += datalen;
					}
				}
				data = out_buf;
				datalen = len;
			}
		}
		else {
			/* 
			 * It is a customer header.
			 */
			rv = get_unknown_header(&con->http.hdr, header_name,
				strlen(header_name), &data, &datalen, &attrs);
		}
		if (!rv) {
			Tcl_SetStringObj(res, data, datalen);
		}
		else {
			Tcl_SetStringObj(res, "", 0);
		}
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_REQ_METHOD) == 0) {
		rv = get_known_header(&con->http.hdr, MIME_HDR_X_NKN_METHOD, 
				&data, &datalen, &attrs, &hdrcnt);
		if (!rv) {
			Tcl_SetStringObj(res, data, datalen);
			return TCL_OK;
		}
		Tcl_SetStringObj(res, "", 0);
		AO_fetch_and_add1(&glob_pe_err_cl_http_req_get_no_method);
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_REQ_QUERY) == 0) {
		rv = get_known_header(&con->http.hdr, MIME_HDR_X_NKN_QUERY, 
				&data, &datalen, &attrs, &hdrcnt);
		if (!rv) {
			Tcl_SetStringObj(res, data, datalen);
		}
		else {
			Tcl_SetStringObj(res, "", 0);
		}
		return TCL_OK;
	}


	/*
	 * Tokens for network socket
	 */
	else if(strcasecmp(token, TOKEN_SOCKET_SOURCE_IP) == 0) {
		char ip[INET6_ADDRSTRLEN];

		if (con->src_ipv4v6.family == AF_INET) {
			inet_ntop(AF_INET, &IPv4(con->src_ipv4v6), ip, INET6_ADDRSTRLEN);
		}
		else {
			inet_ntop(AF_INET6, &IPv6(con->src_ipv4v6), ip, INET6_ADDRSTRLEN);
		}
		Tcl_SetStringObj(res, ip, strlen(ip));
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_SOCKET_SOURCE_PORT) == 0) {
		char port[16];
		snprintf(port, 16, "%d", ntohs(con->src_port));
		Tcl_SetStringObj(res, port, strlen(port));
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_SOCKET_DEST_IP) == 0) {
		char ip[INET6_ADDRSTRLEN];

		if (con->http.remote_ipv4v6.family == AF_INET) {
			inet_ntop(AF_INET, &IPv4(con->http.remote_ipv4v6), ip, INET6_ADDRSTRLEN);
		}
		else {
			inet_ntop(AF_INET6, &IPv6(con->http.remote_ipv4v6), ip, INET6_ADDRSTRLEN);
		}
		Tcl_SetStringObj(res, ip, strlen(ip));
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_SOCKET_DEST_PORT) == 0) {
		char port[16];
		snprintf(port, 16, "%d", ntohs(con->dest_port));
		Tcl_SetStringObj(res, port, strlen(port));
		return TCL_OK;
	}
	else {
		AO_fetch_and_add1(&glob_pe_err_cl_http_req_get_unknown_token);
	}

	/* Not defined */
	return TCL_ERROR;
}

static int pe_cl_http_recv_setaction(ClientData clientData, Tcl_Interp *interp, 
			int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj	*res;
	char * token;
	con_t *con;

	if (objc < 2) {
		AO_fetch_and_add1(&glob_pe_err_cl_http_req_set_no_action);
		return TCL_ERROR;
	}
	token = Tcl_GetString(objv[1]);

	res = Tcl_GetObjResult(interp);
	con = (con_t *)clientData;
	Tcl_SetStringObj(res, "true", 4);

	if(strcasecmp(token, ACTION_CLUSTER_REDIRECT) == 0) {
		SET_ACTION(con, PE_ACTION_CLUSTER_REDIRECT);
		AO_fetch_and_add1(&glob_pe_act_cl_http_req_cluster_redirect);
		/* TBD Add any additional processing
		 */
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_CLUSTER_PROXY) == 0) {
		SET_ACTION(con, PE_ACTION_CLUSTER_PROXY);
		AO_fetch_and_add1(&glob_pe_act_cl_http_req_cluster_proxy);
		/* TBD Add any additional processing
		 */
		return TCL_OK;
	}
	else {
		Tcl_SetStringObj(res, "false", 5);
		AO_fetch_and_add1(&glob_pe_err_cl_http_req_set_unknown_action);
	}

	return TCL_ERROR;
}

uint64_t pe_eval_cl_http_recv_request(pe_rules_t * p_perule, void * pcon, int def_proxy)
{
	int ret;
	Tcl_Command tok;
	con_t * con = (con_t *)pcon;
	cl_pe_param_t param;

	con->pe_action = PE_ACTION_NO_ACTION;
	AO_fetch_and_add1(&glob_pe_eval_cl_http_recv_req);
	
	// Supported variables
	param.pcon = pcon;
	param.def_proxy = def_proxy;
	tok = Tcl_CreateObjCommand(p_perule->tip, 
			"getparam", pe_cl_http_recv_req_gettokens,
			(ClientData) &param, (Tcl_CmdDeleteProc *) NULL);

	// Supported actions
	tok = Tcl_CreateObjCommand(p_perule->tip, 
			"setaction", pe_cl_http_recv_setaction,
			(ClientData) con, (Tcl_CmdDeleteProc *) NULL);
	ret = Tcl_Eval(p_perule->tip, "pe_cl_http_recv_request");
	if ( ret != TCL_OK) {
		DBG_LOG(ERROR, MOD_HTTP, "pe_cl_http_recv_request returns %d\n", ret);
		AO_fetch_and_add1(&glob_pe_eval_err_cl_http_recv_req);
		return PE_ACTION_ERROR;
	}
	return con->pe_action;
}


static int pe_http_send_resp_gettokens(ClientData clientData, Tcl_Interp *interp, 
			int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj	*res;
	char * token;
	//con_t *con;
	int rv;
	u_int32_t attrs;
	int hdrcnt;
	const char *data;
	int datalen;
	pe_resp_param_t *param;
	mime_header_t *phdr;
	int trace_flag = 0;

	if (objc < 2) {
		AO_fetch_and_add1(&glob_pe_err_http_rsp_get_no_token);
		return TCL_ERROR;
	}
	token = Tcl_GetString(objv[1]);

	res = Tcl_GetObjResult(interp);
	param = (pe_resp_param_t *)clientData;
	phdr = &param->phttp->hdr;
	trace_flag = CHECK_HTTP_FLAG(param->phttp, HRF_TRACE_REQUEST);

	/*
	 * Tokens for response object
	 */
	if(strcasecmp(token, TOKEN_HTTP_RES_STATUS_CODE) == 0) {
		char respcode[16];
		snprintf(respcode, 16, "%d", param->phttp->respcode);
		if (trace_flag) {
			DBG_TRACE("PE getparam %s : %s", token, respcode);
		}
		Tcl_SetStringObj(res, respcode, strlen(respcode));
		return TCL_OK;
	}
	else if((strcasecmp(token, TOKEN_HTTP_RES_HEADER) == 0) && (objc==3)) {
		char * header_name;
		int enum_token;
		int ret;
		char out_buf[4096];
		int len = 0;
		int i;
		int rv1;

		header_name = Tcl_GetString(objv[2]);
		ret = string_to_known_header_enum(header_name, strlen(header_name), &enum_token);
		if(ret == 0) {
			rv = get_known_header(param->presp_hdr, enum_token,
				&data, &datalen, &attrs, &hdrcnt);
			if (!rv && hdrcnt > 1 && datalen < 4096) {
				memcpy(out_buf, data, datalen);
				len = datalen;
				for (i = 1; i < hdrcnt; i++) {
					rv1 = get_nth_known_header(param->presp_hdr, enum_token, 
							i, &data, &datalen, &attrs);
					if (!rv1 && ((len+datalen+2) < 4096)) {
						memcpy(out_buf+len, ", ", 2);
						len += 2;
						memcpy(out_buf+len, data, datalen);
						len += datalen;
					}
				}
				data = out_buf;
				datalen = len;
			}
                }
		else {
			rv = get_unknown_header(param->presp_hdr, header_name,
				strlen(header_name), &data, &datalen, &attrs);
		}
		if (!rv) {
			if (trace_flag) {
				DBG_TRACE("PE getparam %s %s : %s", token, header_name, data);
			}
			Tcl_SetStringObj(res, data, datalen);
		}
		else {
			if (trace_flag) {
				DBG_TRACE("PE getparam %s %s : Warning - Header not found", token, header_name);
			}
			Tcl_SetStringObj(res, "", 0);
			AO_fetch_and_add1(&glob_pe_wrn_http_rsp_get_no_rsp_header);
		}
		return TCL_OK;
	}
	
	/*
	 * Tokens for HTTP structure
	 */
	else if(strcasecmp(token, TOKEN_HTTP_REQ_HOST) == 0) {
		rv = get_known_header(phdr, MIME_HDR_HOST, 
				&data, &datalen, &attrs, &hdrcnt);
		if (!rv) {
			if (trace_flag) {
				DBG_TRACE("PE getparam %s : %s", token, data);
			}
			Tcl_SetStringObj(res, data, datalen);
			return TCL_OK;
		}
		if (trace_flag) {
			DBG_TRACE("PE getparam %s : Error - Host header not found", token);
		}
		Tcl_SetStringObj(res, "", 0);
		AO_fetch_and_add1(&glob_pe_err_http_rsp_get_no_host);
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_REQ_URI) == 0) {
		rv = get_known_header(phdr, MIME_HDR_X_NKN_URI, 
				&data, &datalen, &attrs, &hdrcnt);
		if (!rv) {
			if (trace_flag) {
				DBG_TRACE("PE getparam %s : %s", token, data);
			}
			Tcl_SetStringObj(res, data, datalen);
			return TCL_OK;
		}
		if (trace_flag) {
			DBG_TRACE("PE getparam %s : Error - URI not found", token);
		}
		Tcl_SetStringObj(res, "", 0);
		AO_fetch_and_add1(&glob_pe_err_http_rsp_get_no_uri);
		return TCL_OK;
	}
	else if((strcasecmp(token, TOKEN_HTTP_REQ_COOKIE) == 0) && (objc==3)) {
		char * cookie_name;
		int nth;
		const char *pos_s;
		const char *name;
		int namelen;
		int totlen;
		int len;
		
		cookie_name = Tcl_GetString(objv[2]);
		len = strlen(cookie_name);
		rv = get_known_header(phdr, MIME_HDR_COOKIE, 
				&data, &datalen, &attrs, &hdrcnt);
		if (!rv) {
			get_cookie_name_value(data, &datalen, &name, &namelen, &totlen);
			if (len == namelen && strncasecmp(name, cookie_name, len)==0) {
				if (trace_flag) {
					DBG_TRACE("PE getparam %s %s : %s", token, cookie_name, name);
				}
				Tcl_SetStringObj(res, name, totlen);
				return TCL_OK;
			}
			/* Extract Cookie name value after ;
			 */
			pos_s = name + totlen + 1;
			while (datalen > 0) {
				get_cookie_name_value(pos_s, &datalen, &name, &namelen, &totlen);
				if (len == namelen && strncasecmp(name, cookie_name, len)==0) {
					if (trace_flag) {
						DBG_TRACE("PE getparam %s %s : %s", token, cookie_name, name);
					}
					Tcl_SetStringObj(res, name, totlen);
					return TCL_OK;
				}
				pos_s = name + totlen + 1;
			}
			for(nth=1; nth<hdrcnt; nth++) {
				rv = get_nth_known_header(phdr, MIME_HDR_COOKIE,
						nth, &data, &datalen, &attrs);
				get_cookie_name_value(data, &datalen, &name, &namelen, &totlen);
				if (len == namelen && strncasecmp(name, cookie_name, len)==0) {
					if (trace_flag) {
						DBG_TRACE("PE getparam %s %s : %s", token, cookie_name, name);
					}
					Tcl_SetStringObj(res, name, totlen);
					return TCL_OK;
				}
				pos_s = name + totlen + 1;
				while (datalen > 0) {
					get_cookie_name_value(pos_s, &datalen, &name, &namelen, &totlen);
					if (len == namelen && strncasecmp(name, cookie_name, len)==0) {
						Tcl_SetStringObj(res, name, totlen);
						return TCL_OK;
					}
					pos_s = name + totlen + 1;
				}
			}
		}
		if (trace_flag) {
			DBG_TRACE("PE getparam %s %s : Warning - Cookie not found", token, cookie_name);
		}
		Tcl_SetStringObj(res, "", 0);
		AO_fetch_and_add1(&glob_pe_wrn_http_rsp_get_no_cookie);
		return TCL_OK;
	}
	else if((strcasecmp(token, TOKEN_HTTP_REQ_HEADER) == 0) && (objc==3)) {
		char * header_name;
		int enum_token;
		int ret;
		char out_buf[4096];
		int len = 0;
		int i;
		int rv1;

		header_name = Tcl_GetString(objv[2]);
		ret = string_to_known_header_enum(header_name, strlen(header_name), &enum_token);
		if(ret == 0) {
			/*
			 * It is a known header.
			 */
			rv = get_known_header(phdr, enum_token,
				&data, &datalen, &attrs, &hdrcnt);
			if (!rv && hdrcnt > 1) {
				memcpy(out_buf, data, datalen);
				len = datalen;
				for (i = 1; i < hdrcnt; i++) {
					rv1 = get_nth_known_header(phdr, enum_token, 
							i, &data, &datalen, &attrs);
					if (!rv1 && ((len+datalen+2) < 4096)) {
						memcpy(out_buf+len, ", ", 2);
						len += 2;
						memcpy(out_buf+len, data, datalen);
						len += datalen;
					}
				}
				data = out_buf;
				datalen = len;
			}
		}
		else {
			/* 
			 * It is a customer header.
			 */
			rv = get_unknown_header(phdr, header_name,
				strlen(header_name), &data, &datalen, &attrs);
		}
		if (!rv) {
			if (trace_flag) {
				DBG_TRACE("PE getparam %s %s : %s", token, header_name, data);
			}
			Tcl_SetStringObj(res, data, datalen);
		}
		else {
			if (trace_flag) {
				DBG_TRACE("PE getparam %s %s : Warning - Header not found", token, header_name);
			}
			Tcl_SetStringObj(res, "", 0);
			AO_fetch_and_add1(&glob_pe_wrn_http_rsp_get_no_header);
		}
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_REQ_METHOD) == 0) {
		rv = get_known_header(phdr, MIME_HDR_X_NKN_METHOD, 
				&data, &datalen, &attrs, &hdrcnt);
		if (!rv) {
			if (trace_flag) {
				DBG_TRACE("PE getparam %s : %s", token, data);
			}
			Tcl_SetStringObj(res, data, datalen);
			return TCL_OK;
		}
		if (trace_flag) {
			DBG_TRACE("PE getparam %s : Warning - Method not found", token);
		}
		Tcl_SetStringObj(res, "", 0);
		AO_fetch_and_add1(&glob_pe_err_http_rsp_get_no_method);
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_REQ_QUERY) == 0) {
		rv = get_known_header(phdr, MIME_HDR_X_NKN_QUERY, 
				&data, &datalen, &attrs, &hdrcnt);
		if (!rv) {
			if (trace_flag) {
				DBG_TRACE("PE getparam %s : %s", token, data);
			}
			Tcl_SetStringObj(res, data, datalen);
		}
		else {
			if (trace_flag) {
				DBG_TRACE("PE getparam %s : Warning - Query not found", token);
			}
			Tcl_SetStringObj(res, "", 0);
			AO_fetch_and_add1(&glob_pe_wrn_http_rsp_get_no_query);
		}
		return TCL_OK;
	}


	/*
	 * Tokens for network socket
	 */
#if 0
	else if(strcasecmp(token, TOKEN_SOCKET_SOURCE_IP) == 0) {
		struct in_addr in;
		char ip[16];
		in.s_addr = con->src_ip;
		snprintf(ip, 16, "%s", inet_ntoa(in));
		Tcl_SetStringObj(res, ip, strlen(ip));
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_SOCKET_SOURCE_PORT) == 0) {
		char port[16];
		snprintf(port, 16, "%d", ntohs(con->src_port));
		Tcl_SetStringObj(res, port, strlen(port));
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_SOCKET_DEST_IP) == 0) {
		struct in_addr in;
		char ip[16];
		in.s_addr = con->dest_ip;
		snprintf(ip, 16, "%s", inet_ntoa(in));
		Tcl_SetStringObj(res, ip, strlen(ip));
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_SOCKET_DEST_PORT) == 0) {
		char port[16];
		snprintf(port, 16, "%d", ntohs(con->dest_port));
		Tcl_SetStringObj(res, port, strlen(port));
		return TCL_OK;
	}
#endif
	else {
		if (trace_flag) {
			DBG_TRACE("PE getparam %s : Error - Unknown token", token);
		}
		AO_fetch_and_add1(&glob_pe_err_http_rsp_get_unknown_token);
	}

	/* Not defined */
	return TCL_ERROR;
}

static int pe_http_send_resp_setaction(ClientData clientData, Tcl_Interp *interp, 
			int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj	*res;
	char * token;
	//con_t *con;
	pe_resp_param_t *param;
	int trace_flag = 0;

	if (objc < 2) {
		AO_fetch_and_add1(&glob_pe_err_http_rsp_set_no_action);
		return TCL_ERROR;
	}
	token = Tcl_GetString(objv[1]);

	res = Tcl_GetObjResult(interp);
	param = (pe_resp_param_t *)clientData;
	trace_flag = CHECK_HTTP_FLAG(param->phttp, HRF_TRACE_REQUEST);
	Tcl_SetStringObj(res, "true", 4);

	/*
	 * Actions
	 */
	if(strcasecmp(token, ACTION_INSERT_HEADER) == 0) {
		char * header_name;
		char * data;
		int enum_token;
		int ret;
		int rv;

		if (objc != 4) {
			if (trace_flag) {
				DBG_TRACE("PE setaction %s : Error - Required parameters missing", token);
			}
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_http_rsp_set_ins_hdr_param);
			return TCL_ERROR;
		}

		header_name = Tcl_GetString(objv[2]);
		data = Tcl_GetString(objv[3]);

		ret = string_to_known_header_enum(header_name, strlen(header_name), &enum_token);
		rv = 0;
		if (ret == 0) {
			if ((rv = is_known_header_present(param->presp_hdr, enum_token)) == 0) {
				rv = add_known_header(param->presp_hdr, enum_token, 
					data, strlen(data));
			}
		}
		else {
			if ((rv = is_unknown_header_present(param->presp_hdr, header_name,
					strlen(header_name))) == 0) {
				rv = add_unknown_header(param->presp_hdr, header_name,
					strlen(header_name), data, strlen(data));
			}
		}
		if (rv) {
			if (trace_flag) {
				DBG_TRACE("PE setaction %s %s %s : Error - Unable to insert", token, header_name, data);
			}
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_http_rsp_set_ins_hdr);
			return TCL_OK;
		}

		if (trace_flag) {
			DBG_TRACE("PE setaction %s %s %s", token, header_name, data);
		}
		AO_fetch_and_add1(&glob_pe_act_http_rsp_insert_header); 
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_REMOVE_HEADER) == 0) {
		char * header_name;
		int enum_token;
		int ret;
		int rv;

		if (objc != 3) {
			if (trace_flag) {
				DBG_TRACE("PE setaction %s : Error - Required parameters missing", token);
			}
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_http_rsp_set_rmv_hdr_param);
			return TCL_ERROR;
		}

		header_name = Tcl_GetString(objv[2]);

		ret = string_to_known_header_enum(header_name, strlen(header_name), &enum_token);
		rv = 0;
		if (ret == 0) {
			rv = delete_known_header(param->presp_hdr, enum_token);
		}
		else {
			rv = delete_unknown_header(param->presp_hdr, header_name,
					strlen(header_name));
		}
		if (rv) {
			if (trace_flag) {
				DBG_TRACE("PE setaction %s %s : Error - Unable to remove", token, header_name);
			}
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_http_rsp_set_rmv_hdr);
			return TCL_OK;
		}

		if (trace_flag) {
			DBG_TRACE("PE setaction %s %s", token, header_name);
		}
		AO_fetch_and_add1(&glob_pe_act_http_rsp_remove_header); 
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_MODIFY_HEADER) == 0) {
		char * header_name;
		char * data;
		int enum_token;
		int ret;
		int rv;

		if (objc != 4) {
			if (trace_flag) {
				DBG_TRACE("PE setaction %s : Error - Required parameters missing", token);
			}
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_http_rsp_set_mod_hdr_param);
			return TCL_ERROR;
		}

		header_name = Tcl_GetString(objv[2]);
		data = Tcl_GetString(objv[3]);

		ret = string_to_known_header_enum(header_name, strlen(header_name), &enum_token);
		rv = 0;
		if (ret == 0) {
			if (is_known_header_present(param->presp_hdr, enum_token)) {
				rv = delete_known_header(param->presp_hdr, enum_token);
			}
			rv = add_known_header(param->presp_hdr, enum_token, 
				data, strlen(data));
		}
		else {
			if (is_unknown_header_present(param->presp_hdr, header_name,
					strlen(header_name))) {
				rv = delete_unknown_header(param->presp_hdr, header_name,
						strlen(header_name));
			}
			rv = add_unknown_header(param->presp_hdr, header_name,
				strlen(header_name), data, strlen(data));
		}
		if (rv) {
			if (trace_flag) {
				DBG_TRACE("PE setaction %s %s %s : Error - Unable to modify", token, header_name, data);
			}
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_http_rsp_set_mod_hdr);
			return TCL_OK;
		}

		if (trace_flag) {
			DBG_TRACE("PE setaction %s %s %s", token, header_name, data);
		}
		AO_fetch_and_add1(&glob_pe_act_http_rsp_modify_header); 
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_MODIFY_RESPONSE_CODE) == 0) {
		char * data;
		int code = 0;

		if (objc < 3) {
			if (trace_flag) {
				DBG_TRACE("PE setaction %s : Error - Required parameters missing", token);
			}
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_http_rsp_set_mod_resp_code_param);
			return TCL_ERROR;
		}

		data = Tcl_GetString(objv[2]);
		code = atoi( data );

		if (code == 200 || code == 206) {
			param->phttp->respcode = code;
		}
		else {
			if (trace_flag) {
				DBG_TRACE("PE setaction %s %s : Error - Unable to modify", token, data);
			}
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_http_rsp_set_mod_resp_code);
			return TCL_OK;
		}

		if (trace_flag) {
			DBG_TRACE("PE setaction %s %s", token, data);
		}
		AO_fetch_and_add1(&glob_pe_act_http_rsp_modify_resp_code); 
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_SET_IP_TOS) == 0) {
		char * tos;
		int opt;

		if (objc != 3) {
			if (trace_flag) {
				DBG_TRACE("PE setaction %s : Error - Required parameters missing", token);
			}
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_http_rsp_set_set_ip_tos);
			return TCL_ERROR;
		}

		tos = Tcl_GetString(objv[2]);
		//con->pe_action = PE_ACTION_SET_IP_TOS;
		/* 
		 * tos byte PE configuration should like: 0b110011  configure 6 bits only
		 */
		AO_fetch_and_add1(&glob_pe_act_http_rsp_set_ip_tos);
		if((strlen(tos) != 8) || (tos[0]!='0') || (tos[1]!='b')) {
			if (trace_flag) {
				DBG_TRACE("PE setaction %s %s : Error - Wrong parameter", token, tos);
			}
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_http_rsp_set_set_ip_tos);
			return TCL_OK;
		}

		/*
		 * IP protocol DSCP/TOS byte:
		 * 0    1   2  3   4   5   6   7
		 * PRECEDENCE  D   T   R   UNUSED
		 *
		 * IPTOS_TOS_MASK 0x1E
		 * D: low delay. 	IPTOS_LOWDELAY
		 * T: high throughput.  IPTOS_THROUGHPUT
		 * R: high reliability. IPTOS_RELIABILITY
		 *
		 * IPTOS_PREC_MASK 0xE0
		 */

		opt = 0;
		if(tos[2]=='1') opt |= IPTOS_PREC_FLASHOVERRIDE;//0x80
		if(tos[3]=='1') opt |= IPTOS_PREC_IMMEDIATE; //0x40
		if(tos[4]=='1') opt |= IPTOS_PREC_PRIORITY;  //0x20
		if(tos[5]=='1') opt |= IPTOS_LOWDELAY;  //0x10
		if(tos[6]=='1') opt |= IPTOS_THROUGHPUT;  //0x08
		if(tos[7]=='1') opt |= IPTOS_RELIABILITY;  //0x04

		/* store the options in http structure.
		 * setsockopt is done before send as fd is not available here
		 */
		if (param->phttp->respcode >= 200 && param->phttp->respcode < 300) {
			param->phttp->tos_opt = opt;
			param->pe_action |= PE_ACTION_SET_IP_TOS;
			SET_HTTP_FLAG(param->phttp, HRF_PE_SET_IP_TOS);
		}

		if (trace_flag) {
			DBG_TRACE("PE setaction %s %s", token, tos);
		}
		return TCL_OK;
	}
	else {
		if (trace_flag) {
			DBG_TRACE("PE setaction %s : Error - Unknown action", token);
		}
		Tcl_SetStringObj(res, "false", 5);
		AO_fetch_and_add1(&glob_pe_err_http_rsp_set_unknown_action);
	}

	return TCL_ERROR;
}


uint64_t pe_eval_http_send_response(pe_rules_t * p_perule, void *p_http, void *p_resp_hdr)
{
	int ret;
	Tcl_Command tok;
	pe_resp_param_t param;

	if (!PE_CHECK_FLAG(p_perule, PE_EXEC_HTTP_SEND_RESP)) {
		return PE_ACTION_NO_ACTION;
	}
	AO_fetch_and_add1(&glob_pe_eval_http_send_resp);
	
	// Supported variables
	param.phttp = (http_cb_t *) p_http;
	param.presp_hdr = (mime_header_t *) p_resp_hdr;
	param.pe_action = PE_ACTION_NO_ACTION;
	tok = Tcl_CreateObjCommand(p_perule->tip, 
			"getparam", pe_http_send_resp_gettokens,
			(ClientData) &param, (Tcl_CmdDeleteProc *) NULL);

	// Supported actions
	tok = Tcl_CreateObjCommand(p_perule->tip, 
			"setaction", pe_http_send_resp_setaction,
			(ClientData) &param, (Tcl_CmdDeleteProc *) NULL);

	if (CHECK_HTTP_FLAG(param.phttp, HRF_TRACE_REQUEST)) {
		DBG_TRACE("PE pe_http_send_response");
	}
	
	ret = Tcl_Eval(p_perule->tip, "pe_http_send_response");
	if (ret != TCL_OK) {
		DBG_LOG(ERROR, MOD_HTTP, "pe_http_send_response returns %d\n", ret);
		if (CHECK_HTTP_FLAG(param.phttp, HRF_TRACE_REQUEST)) {
			DBG_TRACE("PE pe_http_send_response Error");
		}
		AO_fetch_and_add1(&glob_pe_eval_err_http_send_resp);
		return PE_ACTION_ERROR;
	}
	return param.pe_action;
}


#if 0
static int pe_http_recv_req_callout_gettokens(ClientData clientData, Tcl_Interp *interp, 
			int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj	*res;
	char * token;
	con_t *con;
	int rv;
	u_int32_t attrs;
	int hdrcnt;
	const char *data;
	int datalen;

	if (objc < 2) {
		//AO_fetch_and_add1(&glob_pe_parse_err_tcl_script);
		AO_fetch_and_add1(&glob_pe_err_http_req_co_get_no_token);
		return TCL_ERROR;
	}
	token = Tcl_GetString(objv[1]);

	res = Tcl_GetObjResult(interp);
	con = (con_t *)clientData;

	/*
	 * Tokens for HTTP structure
	 */
	if (strcasecmp(token, TOKEN_CALLOUT_STATUS_CODE) == 0) {
		char respcode[16];
		snprintf(respcode, 16, "%d", con->pe_co->co_list->status_code);
		Tcl_SetStringObj(res, respcode, strlen(respcode));
		return TCL_OK;
	}
	else if (strcasecmp(token, TOKEN_CALLOUT_RESPONSE) == 0) {
		Tcl_SetStringObj(res, "", 0);
		return TCL_OK;
	}
	else if (strcasecmp(token, TOKEN_CALLOUT_SERVER) == 0) {
		Tcl_SetStringObj(res, con->pe_co->co_list->server, 
				strlen(con->pe_co->co_list->server));
		return TCL_OK;
	}
	else {
		AO_fetch_and_add1(&glob_pe_err_http_req_co_get_unknown_token);
	}

	/* Not defined */
	return TCL_ERROR;
}


static int pe_http_recv_callout_setaction(ClientData clientData, Tcl_Interp *interp, 
			int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj	*res;
	char * token;
	con_t *con;

	if (objc < 2) {
		//glob_pe_parse_err_tcl_script ++;
		AO_fetch_and_add1(&glob_pe_err_http_req_co_set_no_action);
		return TCL_ERROR;
	}
	token = Tcl_GetString(objv[1]);

	res = Tcl_GetObjResult(interp);
	con = (con_t *)clientData;

	/*
	 * Tokens for HTTP structure
	 */
	if (strcasecmp(token, ACTION_IGNORE_CALLOUT_RESP) == 0) {
		SET_ACTION(con, PE_ACTION_IGNORE_CALLOUT_RESP);
		AO_fetch_and_add1(&glob_pe_act_http_req_co_ignore_resp);
		return TCL_OK;
	}
	else {
		AO_fetch_and_add1(&glob_pe_err_http_req_co_set_unknown_action);
	}

	return TCL_ERROR;
}



uint64_t pe_eval_http_recv_request_callout(pe_rules_t * p_perule, void * pcon)
{
	int ret;
	Tcl_Command tok;
	con_t * con = (con_t *)pcon;

	//con->pe_action = PE_ACTION_NO_ACTION;
	AO_fetch_and_add1(&glob_pe_eval_http_recv_req_co);
	
	// Supported variables
	tok = Tcl_CreateObjCommand(p_perule->tip, 
			"getparam", pe_http_recv_req_callout_gettokens,
			(ClientData) con, (Tcl_CmdDeleteProc *) NULL);

	// Supported actions
	tok = Tcl_CreateObjCommand(p_perule->tip, 
			"setaction", pe_http_recv_callout_setaction,
			(ClientData) con, (Tcl_CmdDeleteProc *) NULL);
	ret = Tcl_Eval(p_perule->tip, "pe_http_recv_request_callout");
	if ( ret != TCL_OK) {
		DBG_LOG(ERROR, MOD_HTTP, "pe_http_recv_request returns %d\n", ret);
		AO_fetch_and_add1(&glob_pe_eval_err_http_recv_req_co);
		return PE_ACTION_ERROR;
	}
	return con->pe_action;
}
#endif

int parse_range(http_cb_t *phttp, char *data)
{
// "Rang" Range: Bytes 21010-47021
	char *p, *p2, *p_st, *p_end;
	int range_start_flag, range_stop_flag;
	const char *ptmp = NULL;
	range_start_flag = range_stop_flag = 0;

	p = data;

	// Check for "Bytes="
	if (strncasecmp(p, "Bytes=", 6) != 0) {
		return HPS_ERROR;
	}

	// skip "Bytes="
	p_st = p;
	p += 6;
	p = (char *) nkn_skip_space(p);
	if((*p!='-') && (*p<'0' || *p>'9')) {
		return HPS_ERROR; // MUST BE number
	}
	p2 = (char *) nkn_find_digit_end(p) + 1;
	if (*p2 == '-' || *p2 == ' ') {
		phttp->brstart = strtol(p, (char **)&ptmp, 10);
		if (phttp->brstart < 0 || p == ptmp) return HPS_ERROR;
		if (phttp->brstart == LONG_MIN || phttp->brstart == LONG_MAX)
			return HPS_ERROR;
		range_start_flag = 1;
	}
	else
		phttp->brstart = -1;
	p_end = p;
	p2 = strchr(p, '-');
	if (p2) {
		p_end = p2+1;
		p_end = (char *) nkn_skip_space(p_end);
		if((*p_end!='\0') && (*p_end<'0' || *p_end>'9')) {
			return HPS_ERROR; // MUST BE number
		}
		p2 = (char *) nkn_find_digit_end(p_end) + 1;
		if (*p2 == '\0' || *p2 == ' ') {
			phttp->brstop = strtol(p_end, (char **)&ptmp, 10);
			if (phttp->brstop < 0 || p == ptmp) return HPS_ERROR;
			if (phttp->brstop == LONG_MIN || phttp->brstop == LONG_MAX)
				return HPS_ERROR;
			//Check to identify a valid stop range value in p_end variable
			if (*p_end != '\0' && *p_end != ' ')
				range_stop_flag = 1;
		}
		else if (*p2 == ',' ) {
			phttp->brstop = strtol(p_end, (char **)&ptmp, 10);
			if (phttp->brstop < 0 || ptmp == p_end) return HPS_ERROR;
			if (phttp->brstop == LONG_MIN || phttp->brstop == LONG_MAX)
				return HPS_ERROR;
			SET_HTTP_FLAG(phttp, HRF_MULTI_BYTE_RANGE);
		}
		else
			phttp->brstop = -1;
		/* three cases here: 
		 * Range: bytes=1-  ==> valid return 206 whole file - first byte
		 * Range: bytes=1-0 ==> invalid return 416
		 * Range: bytes=0-0 ==> valid return 206 with 1 byte.
		 */
		if ( (*p_end == '0') && 
		    (phttp->brstart > phttp->brstop) ) {
			phttp->brstop = -1;
			// hack set to -1. Then http_check_request() will fail.
		}
	} else {
		phttp->brstop = 0;
	}
	SET_HTTP_FLAG(phttp, HRF_BYTE_RANGE);
	if(CHECK_HTTP_FLAG(phttp, HRF_MULTI_BYTE_RANGE)) {
		p_end = p_st;
		while(*p_end!='\r' && *p_end!='\n' && *p_end!='\0') {
			p_end++;
		}
		p_end--; // because of add_known_header length.
	}
	else {
		p_end = (char *) nkn_find_digit_end(p_end);
	}
	if (range_start_flag && range_stop_flag)
		SET_HTTP_FLAG(phttp, HRF_BYTE_RANGE_START_STOP);
	return(HPS_OK);
}
