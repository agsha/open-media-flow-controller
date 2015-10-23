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
#include "om_defs.h"
#include "pe_defs.h"

NKNCNT_EXTERN(pe_parse_err_tcl_script, AO_t)

NKNCNT_DEF(pe_eval_om_recv_resp, AO_t, "", "")
NKNCNT_DEF(pe_eval_om_send_req, AO_t, "", "")
NKNCNT_DEF(pe_eval_err_om_recv_resp, AO_t, "", "")
NKNCNT_DEF(pe_eval_err_om_send_req, AO_t, "", "")

NKNCNT_DEF(pe_act_om_rsp_cache_object, AO_t, "", "")
NKNCNT_DEF(pe_act_om_rsp_redirect, AO_t, "", "")
NKNCNT_DEF(pe_act_om_rsp_no_cache_object, AO_t, "", "")
NKNCNT_DEF(pe_act_om_rsp_insert_header, AO_t, "", "")
NKNCNT_DEF(pe_act_om_rsp_remove_header, AO_t, "", "")
NKNCNT_DEF(pe_act_om_rsp_modify_header, AO_t, "", "")

NKNCNT_DEF(pe_act_om_req_insert_header, AO_t, "", "")
NKNCNT_DEF(pe_act_om_req_remove_header, AO_t, "", "")
NKNCNT_DEF(pe_act_om_req_modify_header, AO_t, "", "")
NKNCNT_DEF(pe_act_om_req_set_ip_tos, AO_t, "", "")

NKNCNT_DEF(pe_err_om_rsp_get_no_token, AO_t, "", "")
NKNCNT_DEF(pe_err_om_rsp_get_no_host, AO_t, "", "")
NKNCNT_DEF(pe_err_om_rsp_get_no_uri, AO_t, "", "")
NKNCNT_DEF(pe_err_om_rsp_get_no_method, AO_t, "", "")
NKNCNT_DEF(pe_err_om_rsp_get_unknown_token, AO_t, "", "")

NKNCNT_DEF(pe_wrn_om_req_get_no_cookie, AO_t, "", "")
NKNCNT_DEF(pe_wrn_om_req_get_no_header, AO_t, "", "")
NKNCNT_DEF(pe_wrn_om_req_get_no_query, AO_t, "", "")

NKNCNT_DEF(pe_err_om_rsp_set_no_action, AO_t, "", "")
NKNCNT_DEF(pe_err_om_rsp_set_redirect, AO_t, "", "")
NKNCNT_DEF(pe_err_om_rsp_set_ins_hdr_param, AO_t, "", "")
NKNCNT_DEF(pe_err_om_rsp_set_ins_hdr, AO_t, "", "")
NKNCNT_DEF(pe_err_om_rsp_set_rmv_hdr_param, AO_t, "", "")
NKNCNT_DEF(pe_err_om_rsp_set_rmv_hdr, AO_t, "", "")
NKNCNT_DEF(pe_err_om_rsp_set_mod_hdr_param, AO_t, "", "")
NKNCNT_DEF(pe_err_om_rsp_set_mod_hdr, AO_t, "", "")
NKNCNT_DEF(pe_err_om_rsp_set_unknown_action, AO_t, "", "")

NKNCNT_DEF(pe_err_om_req_get_no_token, AO_t, "", "")
NKNCNT_DEF(pe_err_om_req_get_no_host, AO_t, "", "")
NKNCNT_DEF(pe_err_om_req_get_no_uri, AO_t, "", "")
NKNCNT_DEF(pe_err_om_req_get_no_method, AO_t, "", "")
NKNCNT_DEF(pe_err_om_req_get_unknown_token, AO_t, "", "")

NKNCNT_DEF(pe_wrn_om_rsp_get_no_rsp_header, AO_t, "", "")
NKNCNT_DEF(pe_wrn_om_rsp_get_no_cookie, AO_t, "", "")
NKNCNT_DEF(pe_wrn_om_rsp_get_no_header, AO_t, "", "")
NKNCNT_DEF(pe_wrn_om_rsp_get_no_query, AO_t, "", "")

NKNCNT_DEF(pe_err_om_req_set_no_action, AO_t, "", "")
NKNCNT_DEF(pe_err_om_req_set_ins_hdr_param, AO_t, "", "")
NKNCNT_DEF(pe_err_om_req_set_ins_hdr, AO_t, "", "")
NKNCNT_DEF(pe_err_om_req_set_rmv_hdr_param, AO_t, "", "")
NKNCNT_DEF(pe_err_om_req_set_rmv_hdr, AO_t, "", "")
NKNCNT_DEF(pe_err_om_req_set_mod_hdr_param, AO_t, "", "")
NKNCNT_DEF(pe_err_om_req_set_mod_hdr, AO_t, "", "")
NKNCNT_DEF(pe_err_om_req_set_set_ip_tos, AO_t, "", "")
NKNCNT_DEF(pe_err_om_req_set_unknown_action, AO_t, "", "");

/* int	 strcasecmp(const char *, const char *); */
/* int	 strncasecmp(const char *, const char *, size_t); */

int get_cookie_name_value(const char *data, int *datalen, const char **name, int *namelen, int *totlen);

/*
 * Local functions
 */
static int pe_om_recv_resp_gettokens(ClientData clientData, Tcl_Interp *interp, 
			int objc, Tcl_Obj *CONST objv[]);
static int pe_om_send_request_gettokens(ClientData clientData, Tcl_Interp *interp, 
			int objc, Tcl_Obj *CONST objv[]);

static int pe_om_recv_resp_gettokens(ClientData clientData, Tcl_Interp *interp, 
			int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj	*res;
	char * token;
	omcon_t * ocon;
	con_t * httpreqcon;
	con_t *org_httpreqcon;
	int rv;
	u_int32_t attrs;
	int hdrcnt;
	const char *data;
	int datalen;

	if (objc < 2) {
		//AO_fetch_and_add1(&glob_pe_parse_err_tcl_script);
		AO_fetch_and_add1(&glob_pe_err_om_rsp_get_no_token);
		return TCL_ERROR;
	}
	token = Tcl_GetString(objv[1]);

	res = Tcl_GetObjResult(interp);
	ocon = (omcon_t *)clientData;
	httpreqcon = ocon->httpreqcon;
	if (ocon->p_validate) {
		org_httpreqcon = (con_t *)(ocon->p_validate->in_proto_data);
	}
	else {
		org_httpreqcon = NULL;
	}

	/*
	 * Tokens for Origin Server response object
	 */
	if(strcasecmp(token, TOKEN_OM_RES_STATUS_CODE) == 0) {
		char respcode[16];
		snprintf(respcode, 16, "%d", ocon->phttp->respcode);
		Tcl_SetStringObj(res, respcode, strlen(respcode));
		return TCL_OK;
	}
	else if((strcasecmp(token, TOKEN_OM_RES_HEADER) == 0) && (objc==3)) {
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
			rv = get_known_header(&ocon->phttp->hdr, enum_token,
				&data, &datalen, &attrs, &hdrcnt);
			if (!rv && hdrcnt > 1 && datalen < 4096) {
				memcpy(out_buf, data, datalen);
				len = datalen;
				for (i = 1; i < hdrcnt; i++) {
					rv1 = get_nth_known_header(&ocon->phttp->hdr, enum_token, 
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
			rv = get_unknown_header(&ocon->phttp->hdr, header_name,
				strlen(header_name), &data, &datalen, &attrs);
		}
		if (!rv) {
			Tcl_SetStringObj(res, data, datalen);
		}
		else {
			Tcl_SetStringObj(res, "", 0);
			AO_fetch_and_add1(&glob_pe_wrn_om_rsp_get_no_rsp_header);
		}
		return TCL_OK;
	}

	/*
	 * Tokens for client request object
	 */
	else if(strcasecmp(token, TOKEN_HTTP_REQ_HOST) == 0) {
		rv = get_known_header(&httpreqcon->http.hdr, MIME_HDR_HOST, 
				&data, &datalen, &attrs, &hdrcnt);
		if (!rv) {
			Tcl_SetStringObj(res, data, datalen);
			return TCL_OK;
		}
		Tcl_SetStringObj(res, "", 0);
		AO_fetch_and_add1(&glob_pe_err_om_rsp_get_no_host);
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_REQ_URI) == 0) {
		rv = get_known_header(&httpreqcon->http.hdr, MIME_HDR_X_NKN_URI, 
				&data, &datalen, &attrs, &hdrcnt);
		if (!rv) {
			Tcl_SetStringObj(res, data, datalen);
			return TCL_OK;
		}
		Tcl_SetStringObj(res, "", 0);
		AO_fetch_and_add1(&glob_pe_err_om_rsp_get_no_uri);
		return TCL_OK;
	}
	else if((strcasecmp(token, TOKEN_HTTP_REQ_COOKIE) == 0) && (objc==3)) {
		char * cookie_name;
		int nth;
		mime_header_t *hdr;
		const char *pos_s;
		const char *name;
		int namelen;
		int totlen;
		int len;

		cookie_name = Tcl_GetString(objv[2]);
		len = strlen(cookie_name);
		if (org_httpreqcon) {
			hdr = &org_httpreqcon->http.hdr;
		}
		else {
			hdr = &httpreqcon->http.hdr;
		}
		rv = get_known_header(hdr, MIME_HDR_COOKIE,
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
				rv = get_nth_known_header(hdr, MIME_HDR_COOKIE,
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
		AO_fetch_and_add1(&glob_pe_wrn_om_rsp_get_no_cookie);
		return TCL_OK;
	}
	else if((strcasecmp(token, TOKEN_HTTP_REQ_HEADER) == 0) && (objc==3)) {
		char * header_name;
		mime_header_t *hdr;
		int enum_token;
		int ret;
		char out_buf[4096];
		int len = 0;
		int i;
		int rv1;

		if (org_httpreqcon) {
			hdr = &org_httpreqcon->http.hdr;
		}
		else {
			hdr = &httpreqcon->http.hdr;
		}

		header_name = Tcl_GetString(objv[2]);
		ret = string_to_known_header_enum(header_name, strlen(header_name), &enum_token);
		if(ret == 0) {
			rv = get_known_header(hdr, enum_token,
				&data, &datalen, &attrs, &hdrcnt);
			if (!rv && hdrcnt > 1 && datalen < 4096) {
				memcpy(out_buf, data, datalen);
				len = datalen;
				for (i = 1; i < hdrcnt; i++) {
					rv1 = get_nth_known_header(hdr, enum_token, 
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
			rv = get_unknown_header(hdr, header_name,
				strlen(header_name), &data, &datalen, &attrs);
		}

		if (!rv) {
			Tcl_SetStringObj(res, data, datalen);
		}
		else {
			Tcl_SetStringObj(res, "", 0);
			AO_fetch_and_add1(&glob_pe_wrn_om_rsp_get_no_header);
		}
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_REQ_METHOD) == 0) {
		rv = get_known_header(&httpreqcon->http.hdr, MIME_HDR_X_NKN_METHOD, 
				&data, &datalen, &attrs, &hdrcnt);
		if (!rv) {
			Tcl_SetStringObj(res, data, datalen);
			return TCL_OK;
		}
		Tcl_SetStringObj(res, "", 0);
		AO_fetch_and_add1(&glob_pe_err_om_rsp_get_no_method);
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_REQ_QUERY) == 0) {
		rv = get_known_header(&httpreqcon->http.hdr, MIME_HDR_X_NKN_QUERY, 
				&data, &datalen, &attrs, &hdrcnt);
		if (!rv) {
			Tcl_SetStringObj(res, data, datalen);
		}
		else {
			Tcl_SetStringObj(res, "", 0);
			AO_fetch_and_add1(&glob_pe_wrn_om_rsp_get_no_query);
		}
		return TCL_OK;
	}

	/*
	 * Tokens for network socket
	 */
	else if(strcasecmp(token, TOKEN_SOCKET_SOURCE_IP) == 0) {
		char ip[INET6_ADDRSTRLEN];

		if (httpreqcon->src_ipv4v6.family == AF_INET) {
			inet_ntop(AF_INET, &IPv4(httpreqcon->src_ipv4v6), ip, INET6_ADDRSTRLEN);
		}
		else {
			inet_ntop(AF_INET6, &IPv6(httpreqcon->src_ipv4v6), ip, INET6_ADDRSTRLEN);
		}
		Tcl_SetStringObj(res, ip, strlen(ip));
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_SOCKET_SOURCE_PORT) == 0) {
		char port[16];
		snprintf(port, 16, "%d", ntohs(httpreqcon->src_port));
		Tcl_SetStringObj(res, port, strlen(port));
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_SOCKET_DEST_IP) == 0) {
		char ip[INET6_ADDRSTRLEN];

		if (httpreqcon->http.remote_ipv4v6.family == AF_INET) {
			inet_ntop(AF_INET, &IPv4(httpreqcon->http.remote_ipv4v6), ip, INET6_ADDRSTRLEN);
		}
		else {
			inet_ntop(AF_INET6, &IPv6(httpreqcon->http.remote_ipv4v6), ip, INET6_ADDRSTRLEN);
		}
		Tcl_SetStringObj(res, ip, strlen(ip));
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_SOCKET_DEST_PORT) == 0) {
		char port[16];
		snprintf(port, 16, "%d", ntohs(httpreqcon->dest_port));
		Tcl_SetStringObj(res, port, strlen(port));
		return TCL_OK;
	}
	else {
		//AO_fetch_and_add1(&glob_pe_parse_err_tcl_script);
		AO_fetch_and_add1(&glob_pe_err_om_rsp_get_unknown_token);
	}
	/* Not defined */
	return TCL_ERROR;
}

static int pe_om_recv_resp_setaction(ClientData clientData, Tcl_Interp *interp, 
			int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj	*res;
	char * token;
	omcon_t * ocon;
	con_t * httpreqcon;

	if (objc < 2) {
		AO_fetch_and_add1(&glob_pe_err_om_rsp_set_no_action);
		return TCL_ERROR;
	}
	token = Tcl_GetString(objv[1]);

	res = Tcl_GetObjResult(interp);
	ocon = (omcon_t *)clientData;
	httpreqcon = ocon->httpreqcon;
	Tcl_SetStringObj(res, "true", 4);

	/*
	 * Tokens for HTTP structure
	 */
	if(strcasecmp(token, ACTION_CACHE_OBJECT) == 0) {
		SET_ACTION(ocon, PE_ACTION_RES_CACHE_OBJECT);
		CLEAR_ACTION(ocon, PE_ACTION_RES_NO_CACHE_OBJECT);
		add_known_header(&httpreqcon->http.hdr, MIME_HDR_X_NKN_CACHE_POLICY, "cache", 5);
		add_known_header(&ocon->phttp->hdr, MIME_HDR_X_NKN_CACHE_POLICY, "cache", 5);
		AO_fetch_and_add1(&glob_pe_act_om_rsp_cache_object);
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_REDIRECT) == 0) {
		char * uri;

		if (objc != 3) {
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_om_rsp_set_redirect);
			return TCL_ERROR;
		}

		uri = Tcl_GetString(objv[2]);
		SET_ACTION(ocon, PE_ACTION_REDIRECT);
		add_known_header(&httpreqcon->http.hdr, MIME_HDR_LOCATION, uri, strlen(uri));
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
			httpreqcon->http.subcode = code;
		}
		else {
			httpreqcon->http.subcode = NKN_PE_REDIRECT;
		}
		AO_fetch_and_add1(&glob_pe_act_om_rsp_redirect);
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_NO_CACHE_OBJECT) == 0) {
		SET_ACTION(ocon, PE_ACTION_RES_NO_CACHE_OBJECT);
		CLEAR_ACTION(ocon, PE_ACTION_RES_CACHE_OBJECT);
		add_known_header(&httpreqcon->http.hdr, MIME_HDR_X_NKN_CACHE_POLICY, "no_cache", 8);
		add_known_header(&ocon->phttp->hdr, MIME_HDR_X_NKN_CACHE_POLICY, "no_cache", 8);
		AO_fetch_and_add1(&glob_pe_act_om_rsp_no_cache_object);
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_INSERT_HEADER) == 0) {
		char * header_name;
		char * data;
		int enum_token;
		int ret;
		int rv;

		if (objc != 4) {
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_om_rsp_set_ins_hdr_param);
			return TCL_ERROR;
		}

		header_name = Tcl_GetString(objv[2]);
		data = Tcl_GetString(objv[3]);
		//SET_ACTION(ocon, PE_ACTION_INSERT_HEADER);

		ret = string_to_known_header_enum(header_name, strlen(header_name), &enum_token);
		rv = 1;
		if (ret == 0) {
			if (enum_token == MIME_HDR_X_ACCEL_CACHE_CONTROL) {
				int hlen = ocon->oscfg.nsc->http_config->policies.cache_redirect.hdr_len;
				char *hname = ocon->oscfg.nsc->http_config->policies.cache_redirect.hdr_name;
				if (hlen && (strncasecmp(hname, header_name, hlen) == 0) &&
						(is_known_header_present(&ocon->phttp->hdr, enum_token) == 0)) {
					rv = add_known_header(&ocon->phttp->hdr, enum_token, 
						data, strlen(data));
				}
				else {
					if ((is_known_header_present(&ocon->phttp->hdr, enum_token) == 0) &&
							(is_unknown_header_present(&ocon->phttp->hdr, header_name,
							strlen(header_name)) == 0)) {
						rv = add_unknown_header(&ocon->phttp->hdr, header_name,
							strlen(header_name), data, strlen(data));
					}
				}
			}
			else {
				if (is_known_header_present(&ocon->phttp->hdr, enum_token) == 0) {
					rv = add_known_header(&ocon->phttp->hdr, enum_token, 
						data, strlen(data));
				}
			}
		}
		else {
			if (is_unknown_header_present(&ocon->phttp->hdr, header_name,
					strlen(header_name)) == 0) {
				rv = add_unknown_header(&ocon->phttp->hdr, header_name,
					strlen(header_name), data, strlen(data));
			}
		}
		if (rv) {
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_om_rsp_set_ins_hdr);
			return TCL_OK;
		}

		AO_fetch_and_add1(&glob_pe_act_om_rsp_insert_header); 
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_REMOVE_HEADER) == 0) {
		char * header_name;
		int enum_token;
		int ret;
		int rv;

		if (objc != 3) {
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_om_rsp_set_rmv_hdr_param);
			return TCL_ERROR;
		}

		header_name = Tcl_GetString(objv[2]);

		ret = string_to_known_header_enum(header_name, strlen(header_name), &enum_token);
		rv = 0;
		if (ret == 0) {
			rv = delete_known_header(&ocon->phttp->hdr, enum_token);
		}
		else {
			rv = delete_unknown_header(&ocon->phttp->hdr, header_name,
					strlen(header_name));
		}
		if (rv) {
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_om_rsp_set_rmv_hdr);
			return TCL_OK;
		}

		AO_fetch_and_add1(&glob_pe_act_om_rsp_remove_header); 
		return TCL_OK;
	} 
	else if(strcasecmp(token, ACTION_MODIFY_HEADER) == 0) {
		char * header_name;
		char * data;
		int enum_token;
		int ret;
		int rv;

		if (objc != 4) {
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_om_rsp_set_mod_hdr_param);
			return TCL_ERROR;
		}

		header_name = Tcl_GetString(objv[2]);
		data = Tcl_GetString(objv[3]);

		ret = string_to_known_header_enum(header_name, strlen(header_name), &enum_token);
		rv = 0;
		if (ret == 0) {
			if (is_known_header_present(&ocon->phttp->hdr, enum_token)) {
				rv = delete_known_header(&ocon->phttp->hdr, enum_token);
			}
			rv = add_known_header(&ocon->phttp->hdr, enum_token, 
				data, strlen(data));
		}
		else {
			if (is_unknown_header_present(&ocon->phttp->hdr, header_name,
					strlen(header_name))) {
				rv = delete_unknown_header(&ocon->phttp->hdr, header_name,
						strlen(header_name));
			}
			rv = add_unknown_header(&ocon->phttp->hdr, header_name,
				strlen(header_name), data, strlen(data));
		}
		if (rv) {
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_om_rsp_set_mod_hdr);
			return TCL_OK;
		}

		AO_fetch_and_add1(&glob_pe_act_om_rsp_modify_header); 
		return TCL_OK;
	}
	else {
		Tcl_SetStringObj(res, "false", 5);
		AO_fetch_and_add1(&glob_pe_err_om_rsp_set_unknown_action);
	}
	return TCL_ERROR;
}

uint64_t pe_eval_om_recv_response(pe_rules_t * p_perule, void * pcon)
{
	int ret;
	Tcl_Command tok;
	omcon_t * ocon = (omcon_t *)pcon;

	ocon->pe_action = PE_ACTION_NO_ACTION;
	if (!PE_CHECK_FLAG(p_perule, PE_EXEC_OM_RECV_RESP)) {
		return PE_ACTION_NO_ACTION;
	}
	AO_fetch_and_add1(&glob_pe_eval_om_recv_resp);

	// Supported variables
	tok = Tcl_CreateObjCommand(p_perule->tip, 
			"getparam", pe_om_recv_resp_gettokens,
		        (ClientData) ocon, (Tcl_CmdDeleteProc *) NULL);

	// Supported actions
	tok = Tcl_CreateObjCommand(p_perule->tip, 
			"setaction", pe_om_recv_resp_setaction,
		        (ClientData) ocon, (Tcl_CmdDeleteProc *) NULL);
	ret = Tcl_Eval(p_perule->tip, "pe_om_recv_response");
	if ( ret != TCL_OK) {
		DBG_LOG(ERROR, MOD_HTTP, "pe_om_recv_response returns %d\n", ret);
		AO_fetch_and_add1(&glob_pe_eval_err_om_recv_resp);
		return PE_ACTION_ERROR;
	}
	return ocon->pe_action;
}


static int pe_om_send_request_gettokens(ClientData clientData, Tcl_Interp *interp, 
			int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj	*res;
	char * token;
	omcon_t * ocon;
	con_t * httpreqcon;
	con_t *org_httpreqcon;
	int rv;
	u_int32_t attrs;
	int hdrcnt;
	const char *data;
	int datalen;

	if (objc < 2) {
		AO_fetch_and_add1(&glob_pe_err_om_req_get_no_token);
		return TCL_ERROR;
	}
	token = Tcl_GetString(objv[1]);

	res = Tcl_GetObjResult(interp);
	ocon = (omcon_t *)clientData;
	httpreqcon = ocon->httpreqcon;
	if (ocon->p_validate) {
		org_httpreqcon = (con_t *)(ocon->p_validate->in_proto_data);
	}
	else {
		org_httpreqcon = NULL;
	}

	/*
	 * Tokens for client request object
	 */
	if(strcasecmp(token, TOKEN_HTTP_REQ_HOST) == 0) {
		rv = get_known_header(&httpreqcon->http.hdr, MIME_HDR_HOST, 
				&data, &datalen, &attrs, &hdrcnt);
		if (!rv) {
			Tcl_SetStringObj(res, data, datalen);
			return TCL_OK;
		}
		Tcl_SetStringObj(res, "", 0);
		AO_fetch_and_add1(&glob_pe_err_om_req_get_no_host);
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_REQ_URI) == 0) {
		rv = get_known_header(&httpreqcon->http.hdr, MIME_HDR_X_NKN_URI, 
				&data, &datalen, &attrs, &hdrcnt);
		if (!rv) {
			Tcl_SetStringObj(res, data, datalen);
			return TCL_OK;
		}
		Tcl_SetStringObj(res, "", 0);
		AO_fetch_and_add1(&glob_pe_err_om_req_get_no_uri);
		return TCL_OK;
	}
	else if((strcasecmp(token, TOKEN_HTTP_REQ_COOKIE) == 0) && (objc==3)) {
		char * cookie_name;
		int nth;
		mime_header_t *hdr;
		const char *pos_s;
		const char *name;
		int namelen;
		int totlen;
		int len;

		cookie_name = Tcl_GetString(objv[2]);
		len = strlen(cookie_name);
		if (org_httpreqcon) {
			hdr = &org_httpreqcon->http.hdr;
		}
		else {
			hdr = &httpreqcon->http.hdr;
		}
		rv = get_known_header(hdr, MIME_HDR_COOKIE,
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
				rv = get_nth_known_header(hdr, MIME_HDR_COOKIE,
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
		AO_fetch_and_add1(&glob_pe_wrn_om_req_get_no_cookie);
		return TCL_OK;
	}
	else if((strcasecmp(token, TOKEN_HTTP_REQ_HEADER) == 0) && (objc==3)) {
		char * header_name;
		mime_header_t *hdr;
		int enum_token;
		int ret;
		char out_buf[4096];
		int len = 0;
		int i;
		int rv1;

		if (org_httpreqcon) {
			hdr = &org_httpreqcon->http.hdr;
		}
		else {
			hdr = &httpreqcon->http.hdr;
		}

		header_name = Tcl_GetString(objv[2]);
		ret = string_to_known_header_enum(header_name, strlen(header_name), &enum_token);
		if(ret == 0) {
			rv = get_known_header(hdr, enum_token,
				&data, &datalen, &attrs, &hdrcnt);
			if (!rv && hdrcnt > 1 && datalen < 4096) {
				memcpy(out_buf, data, datalen);
				len = datalen;
				for (i = 1; i < hdrcnt; i++) {
					rv1 = get_nth_known_header(hdr, enum_token, 
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
			rv = get_unknown_header(hdr, header_name,
				strlen(header_name), &data, &datalen, &attrs);
		}

		if (!rv) {
			Tcl_SetStringObj(res, data, datalen);
		}
		else {
			Tcl_SetStringObj(res, "", 0);
			AO_fetch_and_add1(&glob_pe_wrn_om_req_get_no_header);
		}
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_REQ_METHOD) == 0) {
		rv = get_known_header(&httpreqcon->http.hdr, MIME_HDR_X_NKN_METHOD, 
				&data, &datalen, &attrs, &hdrcnt);
		if (!rv) {
			Tcl_SetStringObj(res, data, datalen);
			return TCL_OK;
		}
		Tcl_SetStringObj(res, "", 0);
		AO_fetch_and_add1(&glob_pe_err_om_req_get_no_method);
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_REQ_QUERY) == 0) {
		rv = get_known_header(&httpreqcon->http.hdr, MIME_HDR_X_NKN_QUERY, 
				&data, &datalen, &attrs, &hdrcnt);
		if (!rv) {
			Tcl_SetStringObj(res, data, datalen);
		}
		else {
			Tcl_SetStringObj(res, "", 0);
			AO_fetch_and_add1(&glob_pe_wrn_om_req_get_no_query);
		}
		return TCL_OK;
	}

	/*
	 * Tokens for network socket
	 */
	else if(strcasecmp(token, TOKEN_SOCKET_SOURCE_IP) == 0) {
		char ip[INET6_ADDRSTRLEN];

		if (httpreqcon->src_ipv4v6.family == AF_INET) {
			inet_ntop(AF_INET, &IPv4(httpreqcon->src_ipv4v6), ip, INET6_ADDRSTRLEN);
		}
		else {
			inet_ntop(AF_INET6, &IPv6(httpreqcon->src_ipv4v6), ip, INET6_ADDRSTRLEN);
		}
		Tcl_SetStringObj(res, ip, strlen(ip));
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_SOCKET_SOURCE_PORT) == 0) {
		char port[16];
		snprintf(port, 16, "%d", ntohs(httpreqcon->src_port));
		Tcl_SetStringObj(res, port, strlen(port));
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_SOCKET_DEST_IP) == 0) {
		char ip[INET6_ADDRSTRLEN];

		if (httpreqcon->http.remote_ipv4v6.family == AF_INET) {
			inet_ntop(AF_INET, &IPv4(httpreqcon->http.remote_ipv4v6), ip, INET6_ADDRSTRLEN);
		}
		else {
			inet_ntop(AF_INET6, &IPv6(httpreqcon->http.remote_ipv4v6), ip, INET6_ADDRSTRLEN);
		}
		Tcl_SetStringObj(res, ip, strlen(ip));
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_SOCKET_DEST_PORT) == 0) {
		char port[16];
		snprintf(port, 16, "%d", ntohs(httpreqcon->dest_port));
		Tcl_SetStringObj(res, port, strlen(port));
		return TCL_OK;
	}
	else {
		AO_fetch_and_add1(&glob_pe_err_om_req_get_unknown_token);
	}
	/* Not defined */
	return TCL_ERROR;
}

static int pe_om_send_request_setaction(ClientData clientData, Tcl_Interp *interp, 
			int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj	*res;
	char * token;
	omcon_t * ocon;
	con_t * httpreqcon;

	if (objc < 2) {
		AO_fetch_and_add1(&glob_pe_err_om_req_set_no_action);
		return TCL_ERROR;
	}
	token = Tcl_GetString(objv[1]);

	res = Tcl_GetObjResult(interp);
	ocon = (omcon_t *)clientData;
	httpreqcon = ocon->httpreqcon;
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
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_om_req_set_ins_hdr_param);
			return TCL_ERROR;
		}

		header_name = Tcl_GetString(objv[2]);
		data = Tcl_GetString(objv[3]);

		ret = string_to_known_header_enum(header_name, strlen(header_name), &enum_token);
		rv = 0;
		if (ret == 0) {
			if ((rv = is_known_header_present(&httpreqcon->http.hdr, enum_token)) == 0) {
				switch (enum_token) {
					case MIME_HDR_HOST:
						OM_SET_FLAG(ocon, OMF_PE_HOST_HDR);
						rv = add_known_header(&httpreqcon->http.hdr, 
							MIME_HDR_X_NKN_PE_HOST_HDR, 
							data, strlen(data));
						break;
					case MIME_HDR_RANGE:
						OM_SET_FLAG(ocon, OMF_PE_RANGE_HDR);
						break;
					default:
						break;
				}
				rv = add_known_header(&httpreqcon->http.hdr, enum_token, 
					data, strlen(data));
			}
		}
		else {
			if ((rv = is_unknown_header_present(&httpreqcon->http.hdr, header_name,
					strlen(header_name))) == 0) {
				rv = add_unknown_header(&httpreqcon->http.hdr, header_name,
					strlen(header_name), data, strlen(data));
			}
		}
		if (rv) {
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_om_req_set_ins_hdr);
			return TCL_OK;
		}

		AO_fetch_and_add1(&glob_pe_act_om_req_insert_header); 
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_REMOVE_HEADER) == 0) {
		char * header_name;
		int enum_token;
		int ret;
		int rv;

		if (objc != 3) {
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_om_req_set_rmv_hdr_param);
			return TCL_ERROR;
		}

		header_name = Tcl_GetString(objv[2]);

		ret = string_to_known_header_enum(header_name, strlen(header_name), &enum_token);
		rv = 0;
		if (ret == 0) {
			rv = delete_known_header(&httpreqcon->http.hdr, enum_token);
		}
		else {
			rv = delete_unknown_header(&httpreqcon->http.hdr, header_name,
					strlen(header_name));
		}
		if (rv) {
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_om_req_set_rmv_hdr);
			return TCL_OK;
		}

		AO_fetch_and_add1(&glob_pe_act_om_req_remove_header); 
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_MODIFY_HEADER) == 0) {
		char * header_name;
		char * data;
		int enum_token;
		int ret;
		int rv;

		if (objc != 4) {
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_om_req_set_mod_hdr_param);
			return TCL_ERROR;
		}

		header_name = Tcl_GetString(objv[2]);
		data = Tcl_GetString(objv[3]);

		ret = string_to_known_header_enum(header_name, strlen(header_name), &enum_token);
		rv = 0;
		if (ret == 0) {
			if (is_known_header_present(&httpreqcon->http.hdr, enum_token)) {
				rv = delete_known_header(&httpreqcon->http.hdr, enum_token);
			}
			switch (enum_token) {
				case MIME_HDR_HOST:
					OM_SET_FLAG(ocon, OMF_PE_HOST_HDR);
					rv = add_known_header(&httpreqcon->http.hdr, 
							MIME_HDR_X_NKN_PE_HOST_HDR, 
							data, strlen(data));
					break;
				case MIME_HDR_RANGE:
					OM_SET_FLAG(ocon, OMF_PE_RANGE_HDR);
					break;
				default:
					break;
			}
			rv = add_known_header(&httpreqcon->http.hdr, enum_token, 
				data, strlen(data));
		}
		else {
			if (is_unknown_header_present(&httpreqcon->http.hdr, header_name,
					strlen(header_name))) {
				rv = delete_unknown_header(&httpreqcon->http.hdr, header_name,
						strlen(header_name));
			}
			rv = add_unknown_header(&httpreqcon->http.hdr, header_name,
				strlen(header_name), data, strlen(data));
		}
		if (rv) {
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_om_req_set_mod_hdr);
			return TCL_OK;
		}

		AO_fetch_and_add1(&glob_pe_act_om_req_modify_header); 
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_SET_IP_TOS) == 0) {
		char * tos;
		int opt;

		if (objc != 3) {
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_om_req_set_set_ip_tos);
			return TCL_ERROR;
		}

		tos = Tcl_GetString(objv[2]);
		//con->pe_action = PE_ACTION_SET_IP_TOS;
		/* 
		 * tos byte PE configuration should like: 0b110011  configure 6 bits only
		 */
		AO_fetch_and_add1(&glob_pe_act_om_req_set_ip_tos);
		if((strlen(tos) != 8) || (tos[0]!='0') || (tos[1]!='b')) {
			Tcl_SetStringObj(res, "false", 5);
			AO_fetch_and_add1(&glob_pe_err_om_req_set_set_ip_tos);
			return TCL_OK;
		}

		/* performance: don't set again */
		if(OM_CHECK_FLAG(ocon, OMF_HAS_SET_TOS)) {
			return TCL_OK;
		}
		OM_SET_FLAG(ocon, OMF_HAS_SET_TOS);

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
		if (OM_CHECK_FLAG(ocon, OMF_IS_IPV6)) {
			setsockopt(ocon->fd, IPPROTO_IPV6, IPV6_TCLASS, &opt, sizeof(opt));
		}
		else {
			setsockopt(ocon->fd, SOL_IP, IP_TOS, &opt, sizeof(opt));
		}

		return TCL_OK;
	}
	else {
		Tcl_SetStringObj(res, "false", 5);
		AO_fetch_and_add1(&glob_pe_err_om_req_set_unknown_action);
	}
	return TCL_ERROR;
}


uint64_t pe_eval_om_send_request(pe_rules_t * p_perule, void * pcon)
{
	int ret;
	Tcl_Command tok;
	omcon_t * ocon = (omcon_t *)pcon;

	ocon->pe_action = PE_ACTION_NO_ACTION;
	if (!PE_CHECK_FLAG(p_perule, PE_EXEC_OM_SEND_REQ)) {
		return PE_ACTION_NO_ACTION;
	}
	AO_fetch_and_add1(&glob_pe_eval_om_send_req);

	// Supported variables
	tok = Tcl_CreateObjCommand(p_perule->tip, 
			"getparam", pe_om_send_request_gettokens,
		        (ClientData) ocon, (Tcl_CmdDeleteProc *) NULL);

	// Supported actions
	tok = Tcl_CreateObjCommand(p_perule->tip, 
			"setaction", pe_om_send_request_setaction,
		        (ClientData) ocon, (Tcl_CmdDeleteProc *) NULL);
	ret = Tcl_Eval(p_perule->tip, "pe_om_send_request");
	if ( ret != TCL_OK) {
		DBG_LOG(ERROR, MOD_HTTP, "pe_om_send_request returns %d\n", ret);
		AO_fetch_and_add1(&glob_pe_eval_err_om_send_req);
		return PE_ACTION_ERROR;
	}
	return ocon->pe_action;
}


