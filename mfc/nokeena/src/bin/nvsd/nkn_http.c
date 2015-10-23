#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <sys/types.h>
#include <sys/timeb.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <limits.h>
#include <netinet/in.h>
#include <linux/netfilter_ipv4.h>
#include <arpa/inet.h>
#include <atomic_ops.h>
#include <ctype.h>
#include <openssl/md5.h>
#include <openssl/hmac.h>

#include "server.h"
#include "network.h"
#include "nkn_stat.h"
#include "nkn_http.h"
#include "nkn_cfg_params.h"
#include "nkn_debug.h"
#include "ssp.h"
#include "nkn_namespace.h"
#include "nvsd_resource_mgr.h"
#include "nkn_cod.h"
#include "nkn_om.h"
#include "pe_defs.h"
#include "om_defs.h"
#include "om_fp_defs.h"
#include "nvsd_resource_mgr.h"
#include "nkn_assert.h"
#include "nkn_tunnel_defs.h"
#include "pe_geodb.h"
#include "pe_ucflt.h"
#include "nkn_urlfilter.h"

extern struct con_sess_bw_queue sbq_head;
extern struct con_active_queue caq_head;
extern int nkn_timer_interval;
int nkn_resource_pool_enable = 1;
int nkn_http_trace_enable = 1;
int dynamic_uri_enable = 1;
int nkn_http_pers_conn_nums = 1;
int pe_url_category_lookup = 0;
int pe_ucflt_failover_bypass_enable = 0;
extern int glob_cachemgr_init_done;

int validate_client_request(con_t *con);
int validate_auth_header(con_t *con);
int get_aws_authorization_hdr_value(con_t *con, char *hdr_value);

extern void nkn_cleanup_a_task(con_t * con);
static intercept_type_t apply_CL7_intercept_policies(con_t *con, 
						cluster_descriptor_t *cldesc,
			     			intercept_type_t action);
static void handle_no_cache_request(con_t *con, 
				const namespace_config_t *nsconf, 
				int consider_no_cache);
static void handle_if_directives(con_t *con);
static void handle_via(con_t *con, const namespace_config_t *nsconf);
static int CL7_proxy_request(con_t *con, namespace_token_t ns_token, 
				namespace_token_t node_ns_token);
int pe_helper_get_geostat(geo_ip_req_t *data, void *pvtdata);
geo_info_t* get_geo_info_t(uint32_t ip, geo_info_t* p_geoinf);
int pe_helper_get_url_cat(void *data, void *pvtdata);
ucflt_info_t* get_ucflt_info_t(char *url, ucflt_info_t* p_ucflt_inf);

NKNCNT_EXTERN(tot_http_sockets, AO_t);

// List all counters here
NKNCNT_DEF(http_tot_transactions, AO_t , "", "Total HTTP transactions")
NKNCNT_DEF(https_tot_transactions, AO_t , "", "Total HTTPS transactions")
NKNCNT_DEF(http_err_len_not_match, int, "", "Total Length not matched")
NKNCNT_DEF(tot_bytes_sent, unsigned long, "", "Total Byte Sent")
NKNCNT_DEF(socket_err_sent, int, "", "Total Socket Error")
extern AO_t glob_tot_bytes_tobesent;
extern AO_t glob_tot_size_from_tunnel;
NKNCNT_DEF(http_tot_well_finished, AO_t, "", "Total Completed HTTP Trans")
NKNCNT_DEF(afr_miss_due_network, uint64_t, "", "AFR miss due to network problem")
NKNCNT_DEF(socket_bug993, uint32_t, "", "bug993")
NKNCNT_DEF(tunnel_req_http_abs_url, uint64_t, "", "ABS url in forward proxy")
NKNCNT_DEF(tunnel_req_http_get_req_cod_err, uint64_t, "", "http_get_req_cod returned error")
NKNCNT_DEF(tp_req_to_ifip, uint64_t, "", "Tproxy interface gets req")
NKNCNT_DEF(http_req_parse_err, uint64_t, "", "Total HTTP request parse error")
NKNCNT_DEF(tunnel_req_http_parse_err, uint64_t, "", "HTTP request parse failure")
NKNCNT_DEF(tunnel_req_http_check_req_err, uint64_t, "", "HTTP check request failure")
NKNCNT_DEF(http_delayed_req_scheds, uint64_t, "", "HTTP delayed requests due to namespace init");
NKNCNT_DEF(http_delayed_req2_scheds, uint64_t, "", "HTTP delayed type 2 requests due to namespace init");
NKNCNT_DEF(http_delayed_req_fails, uint64_t, "", "HTTP delayed requests initiation fails");
NKNCNT_DEF(http_delayed_req2_fails, uint64_t, "", "HTTP delayed type 2 requests initiation fails");
NKNCNT_DEF(http_ns_bind_ip_nomatch, uint64_t, "", "HTTP Namespace bind IP no match found");
NKNCNT_DEF(http_ns_bind_ip_match, uint64_t, "", "HTTP Namespace bind IP match found");
NKNCNT_DEF( tunnel_req_cachepath_notready, uint64_t, "", "cache path not ready")

NKNCNT_DEF(cll7_intercept_badmethod, uint64_t, "", "HTTP Cluster L7 Bad HTTP method");
NKNCNT_DEF(cll7redir_intercept_success, uint64_t, "", "HTTP Cluster L7 redirect success");
NKNCNT_DEF(cll7redir_intercept_success_local, uint64_t, "", "HTTP Cluster L7 redirect processed locally");
NKNCNT_DEF(cll7redir_intercept_retries, uint64_t, "", "HTTP Cluster L7 redirect request retries");
NKNCNT_DEF(cll7redir_intercept_errs, uint64_t, "", "HTTP Cluster L7 redirect request errors");
NKNCNT_DEF(cll7redir_int_err_1, uint64_t, "", "HTTP Cluster L7 redirect internal error 1");
NKNCNT_DEF(cll7redir_int_err_2, uint64_t, "", "HTTP Cluster L7 redirect internal error 2");
NKNCNT_DEF(cll7proxy_req_int_err1, uint64_t, "", "HTTP Cluster L7 proxy request internal error 1");
NKNCNT_DEF(cll7proxy_req_int_err2, uint64_t, "", "HTTP Cluster L7 proxy request internal error 2");
NKNCNT_DEF(cll7proxy_req_int_err3, uint64_t, "", "HTTP Cluster L7 proxy request internal error 3");
NKNCNT_DEF(cll7proxy_req_int_err4, uint64_t, "", "HTTP Cluster L7 proxy request internal error 4");
NKNCNT_DEF(cll7proxy_req_int_err5, uint64_t, "", "HTTP Cluster L7 proxy request internal error 5");
NKNCNT_DEF(cll7proxy_req_tunnel_err, uint64_t, "", "HTTP Cluster L7 proxy request tunnel error");
NKNCNT_DEF(cll7proxy_req_tunneled, uint64_t, "", "HTTP Cluster L7 proxy request tunneled due to limitations");
NKNCNT_DEF(cll7proxy_int_err_1, uint64_t, "", "HTTP Cluster L7 proxy internal error 1");
NKNCNT_DEF(cll7proxy_intercept_success_local, uint64_t, "", "HTTP Cluster L7 proxy processed locally");
NKNCNT_DEF(cll7proxy_intercept_success, uint64_t, "", "HTTP Cluster L7 proxy processed remote");

NKNCNT_DEF(http_404_52016_resp, uint64_t, "", "HTTP Connection close response 404");
NKNCNT_DEF(http_501_52016_resp, uint64_t, "", "HTTP Connection close response 501");
NKNCNT_DEF(dynamic_uri_err_no_uri, uint64_t, "", "dyn_uri no uri")
NKNCNT_DEF(dynamic_uri_err_max_uri_size, uint64_t, "", "dyn_uri max uri size")
NKNCNT_DEF(dynamic_uri_err_large_cnt, uint64_t, "", "dyn_uri large cnt")
NKNCNT_DEF(dynamic_uri_err_regexec_match, uint64_t, "", "dyn_uri regexec_match")
NKNCNT_DEF(dynamic_uri_tokenize_memalloc_fail,  uint64_t, "", "dyn_uri tokenize memory allocation failure")
NKNCNT_DEF(dynamic_uri_tokenize_err, uint64_t, "", "dyn_uri tokenize error")
NKNCNT_DEF(seek_uri_err_no_uri, uint64_t, "", "dyn_uri no uri")
NKNCNT_DEF(seek_uri_err_max_uri_size, uint64_t, "", "dyn_uri max uri size")
NKNCNT_DEF(seek_uri_err_large_cnt, uint64_t, "", "dyn_uri large cnt")
NKNCNT_DEF(seek_uri_err_regexec_match, uint64_t, "", "dyn_uri regexec_match")
NKNCNT_DEF(client_send_tot_bytes, AO_t, "", "total bytes sent")
NKNCNT_DEF(http_cache_miss_cnt, uint64_t, "", "total http cache miss")
NKNCNT_DEF(http_cache_hit_cnt, uint64_t, "", "total http cache miss")
NKNCNT_DEF(dbg_http_handle_request1, AO_t , "", "Total HTTP transactions")
NKNCNT_DEF(dbg_http_handle_request2, AO_t , "", "Total HTTP transactions")
NKNCNT_DEF(normalize_pe_set_origin_server, uint64_t , "", "Normalized con because of PE")
NKNCNT_DEF(normalize_remapped_uri, uint64_t , "", "Normalized con because of URI remap")
NKNCNT_DEF(normalize_decoded_uri, uint64_t , "", "Normalized con because of URI decode")
NKNCNT_DEF(normalize_host_hdr_inherit, uint64_t , "", "Normalized con because of Host hdr inherit")
NKNCNT_DEF(set_dscp_responses, AO_t, "", "Number of DSCP set responses")
NKNCNT_DEF(reset_dscp_responses, AO_t, "", "Number of DSCP reset responses")
NKNCNT_DEF(aws_auth_hdr_mismatch, uint64_t, "", "aws authorization header mismatch")

#define TSTR(_p, _l, _r)  char *(_r); ( 1 ? ( (_r) = alloca((_l)+1), \
        memcpy((_r), (_p), (_l)), (_r)[(_l)] = '\0' ) : 0 )

static const char header_cluster_ver[] = "v002";

static const char *response_200_OK_body = 
	"<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">"
	"<HTML>"
	    "<HEAD>"
	        "<TITLE>200 OK</TITLE>"
		"<BODY>200 OK</BODY>"
	    "</HEAD>"
	"</HTML>";
int sizeof_response_200_OK_body;

static const char predef_token_names[][40] = { 
	"http.req.custom.flvseek_offset",
	"http.req.custom.range_end",
	"http.req.custom.range_start"
};

// Parallel struct to match (N,V) pairs, which is originally in string format,
// using enum for quickly identifying name and value in (N,V) pair.
struct param_tok {
	uri_token_name_t name_type;
	int match_str_idx; // The value 'x' in $x match string.
};


int is_hex_encoded_uri(char *puri, int len);
static int tokenize(const char *str, struct param_tok *token);
static int is_valid_number(char *str);
URI_REMAP_STATUS nkn_tokenize_uri(char *uri, nkn_regmatch_t *regex_match, 
					const char str[], nkn_uri_token_t *uri_token);
int nkn_tokenize_gen_seek_uri(con_t *con);

/* caller has to ensure http_config_p is not NULL */
inline int nkn_http_is_tproxy_cluster(http_config_t *http_config_p)
{
    return ((http_config_p->origin.select_method == OSS_HTTP_NODE_MAP) && 
        http_config_p->origin.u.http.include_orig_ip_port &&
        (http_config_p->origin.u.http.node_map_entries == 1));
}

int nkn_http_init(void)
{
	sizeof_response_200_OK_body = strlen(response_200_OK_body);
	return 0;
}

////////////////////////////////////////////////////////////////////////
// HTTP request
////////////////////////////////////////////////////////////////////////
	
nkn_cod_t http_get_req_cod(con_t *con);
nkn_cod_t http_get_req_cod(con_t *con)
{
	nkn_cod_t cod;
	const char *data;
	int len, hostlen;
	char cache_uri [MAX_URI_HDR_HOST_SIZE + MAX_URI_SIZE + 1024];
	u_int32_t attrs;
	int hdrcnt;
	const namespace_config_t * ns_cfg;
	char *p;
	int rv;
	char host[1024];
	namespace_token_t token;
	char *p_host = host;
	uint16_t port;
	size_t cachelen;

        if (mime_hdr_get_cache_index(&con->http.hdr,
                &data, &len, &attrs, &hdrcnt)) {
		return NKN_COD_NULL;
	}

	ns_cfg = con->http.nsconf;
	if (ns_cfg == NULL) {
	    	DBG_LOG(MSG, MOD_HTTP, "namespace_to_config() ret=NULL");
		return NKN_COD_NULL;
	}
	/* Default to strip query if ssp config is present 
	 * (or) Check strip_query knob B1750
	 * */
	if ((ns_cfg->ssp_config && ns_cfg->ssp_config->player_type != -1) ||
		(ns_cfg->http_config && ns_cfg->http_config->request_policies.strip_query == NKN_TRUE)) {
	    if (!CHECK_CON_FLAG(con, CONF_REFETCH_REQ) &&
		is_known_header_present(&con->http.hdr, MIME_HDR_X_NKN_QUERY)) {
		p = memchr(data, '?', len);
		if (p) {
		   len = p-data;
                   add_known_header(&con->http.hdr, MIME_HDR_X_NKN_REMAPPED_URI, 
                                    data, len);     
                  if (!is_hex_encoded_uri((char *)data, len)) {
                       SET_HTTP_FLAG2(&con->http, HRF2_QUERY_STRING_REMOVED_URI);
                  }
		}
	    } else {
		DBG_LOG(MSG, MOD_HTTP, "RBCI Probe request and/or no query header present."
			" Strip query string from URI action skipped, con=%p", con);
	    }
	}

	rv = request_to_origin_server(REQ2OS_CACHE_KEY, 0, &con->http.hdr, 
				      ns_cfg, 0, 0, &p_host, sizeof(host),
				      &hostlen, &port, 0, 1);
	if (rv) {
	    DBG_LOG(MSG, MOD_HTTP, "request_to_origin_server() failed, rv=%d",
		    rv);
	    return NKN_COD_NULL;
	}

	token.u.token = 0;
	/*get_known_header and add_known_header for the same heap varaible makes that variable unusable
	so getting 'data' again here*/
        if (mime_hdr_get_cache_index(&con->http.hdr,
                &data, &len, &attrs, &hdrcnt)) {
                return NKN_COD_NULL;
        }
	rv = nstoken_to_uol_uri_stack(token, data,
				      len, host, hostlen,
				      cache_uri, sizeof(cache_uri),
				      ns_cfg, &cachelen);
	if (!rv) {
		DBG_LOG(MSG, MOD_HTTP, "nstoken_to_uol_uri(token=0x%lx) failed",
			con->http.ns_token.u.token);
		return NKN_COD_NULL;
	}

	cod = nkn_cod_open(cache_uri, cachelen, NKN_COD_NW_MGR);
	return cod;
}

int is_hex_encoded_uri(char *puri, int len)
{
	char *p = puri;
#define CHECK_HEX_ENC(p, a, b)	((*(((char *)(p)) + 1)) == ((char) (a)) && \
				(*(((char *)(p)) + 2)) == ((char) (b)))
	while (len) {
		if (*p == '%' &&
			!(CHECK_HEX_ENC(p, '2', '0') ||	// space
			CHECK_HEX_ENC(p, '2', '1') ||	// !
			CHECK_HEX_ENC(p, '2', '3') ||   // #
			CHECK_HEX_ENC(p, '2', '4') ||   // $
			CHECK_HEX_ENC(p, '2', '5') ||   // %
			CHECK_HEX_ENC(p, '2', '6') ||   // &
			CHECK_HEX_ENC(p, '2', '8') ||   // (
			CHECK_HEX_ENC(p, '2', '9') ||   // )
			CHECK_HEX_ENC(p, '2', 'A') || CHECK_HEX_ENC(p, '2', 'a') || // *
			CHECK_HEX_ENC(p, '2', 'B') || CHECK_HEX_ENC(p, '2', 'B') || // +
			CHECK_HEX_ENC(p, '2', 'D') || CHECK_HEX_ENC(p, '2', 'd') || // -
			CHECK_HEX_ENC(p, '2', 'E') || CHECK_HEX_ENC(p, '2', 'e') || // .
			CHECK_HEX_ENC(p, '3', 'A') || CHECK_HEX_ENC(p, '3', 'a') || // :
			CHECK_HEX_ENC(p, '3', 'B') || CHECK_HEX_ENC(p, '3', 'b') || // ;
			CHECK_HEX_ENC(p, '3', 'C') || CHECK_HEX_ENC(p, '3', 'c') || // <
			CHECK_HEX_ENC(p, '3', 'D') || CHECK_HEX_ENC(p, '3', 'd') || // =
			CHECK_HEX_ENC(p, '3', 'E') || CHECK_HEX_ENC(p, '3', 'e') || // >
			CHECK_HEX_ENC(p, '3', 'F') || CHECK_HEX_ENC(p, '3', 'f') || // ?
			CHECK_HEX_ENC(p, '4', '0') ||   // @
			CHECK_HEX_ENC(p, '5', 'B') || CHECK_HEX_ENC(p, '5', 'b') || // [
			CHECK_HEX_ENC(p, '5', 'D') || CHECK_HEX_ENC(p, '5', 'd') || // ]
			CHECK_HEX_ENC(p, '5', 'E') || CHECK_HEX_ENC(p, '5', 'e') || // ^
			CHECK_HEX_ENC(p, '5', 'F') || CHECK_HEX_ENC(p, '5', 'f') || // _
			CHECK_HEX_ENC(p, '6', '0') ||   // `
			CHECK_HEX_ENC(p, '7', 'B') || CHECK_HEX_ENC(p, '7', 'b') || // {
			CHECK_HEX_ENC(p, '7', 'C') || CHECK_HEX_ENC(p, '7', 'c') || // |
			CHECK_HEX_ENC(p, '7', 'D') || CHECK_HEX_ENC(p, '7', 'd') || // }
			CHECK_HEX_ENC(p, '7', 'E') ||  CHECK_HEX_ENC(p, '7', 'e')) ) // ~
		{
			return 1;
		}
		p++;
		len--;
	}
#undef CHECK_HEX_ENC

	return 0;
}

URI_REMAP_STATUS nkn_remap_uri(con_t *con, int *resp_idx_offset);
URI_REMAP_STATUS nkn_remap_uri(con_t *con, int *resp_idx_offset)
{
	char *p;
	char *ptmp;
	char *pnew_uri;
	char *uri;
	char errbuf[32*1024];
	char new_uri[32*1024];
	char tokenize_string[256];

	int rv = -1;
	int len = 0;
	int bytes = 0;
	int cnt = 0;
	URI_REMAP_STATUS ret = 0;
	unsigned int uriattrs = 0;
	int resp_cache_index = 0;
	int remap_tokenize_enable = 0;

	nkn_regex_ctx_t *regctx = 0;
	nkn_regmatch_t match[128];

	const namespace_config_t *nsconf;

	nsconf = con->http.nsconf;

	if (!nsconf->http_config->request_policies.dynamic_uri.regctx) {
		return URI_REMAP_OK;
	}
	regctx = nsconf->http_config->request_policies.dynamic_uri.regctx;

	if (get_known_header(&con->http.hdr, MIME_HDR_X_NKN_DECODED_URI,
			(const char **)&uri, &len, &uriattrs, &cnt)) {
		if (get_known_header(&con->http.hdr, MIME_HDR_X_NKN_URI,
			(const char **)&uri, &len, &uriattrs, &cnt)) {
			glob_dynamic_uri_err_no_uri++;
			return URI_REMAP_ERR;
		}
	}

	memcpy(new_uri, uri, len);
	new_uri[len] = 0;

	rv = nkn_regexec_match(regctx, new_uri, sizeof(match)/sizeof(nkn_regmatch_t),
		match, errbuf, sizeof(errbuf)); 
	if (rv) {
		glob_dynamic_uri_err_regexec_match++;
		return URI_REMAP_ERR;
	}

	if (resp_idx_offset) {
		if ((nsconf->http_config->cache_index.include_header ||
			nsconf->http_config->cache_index.include_object) &&
			(is_known_header_present(&con->http.hdr, MIME_HDR_X_NKN_CLUSTER_TPROXY)==0))  {
			resp_cache_index = 1;
		}
	}

	if (nsconf->http_config->request_policies.dynamic_uri.tokenize_string &&
		(nsconf->http_config->request_policies.dynamic_uri.tokenize_string[0] != 0)) {
		remap_tokenize_enable = 1;
		memset(tokenize_string, 0, sizeof(tokenize_string));
		strncpy(&tokenize_string[0], 
				nsconf->http_config->request_policies.dynamic_uri.tokenize_string,
				strlen(nsconf->http_config->request_policies.dynamic_uri.tokenize_string));
	}
	// Perform URI tokenization
	if(remap_tokenize_enable) {
		con->http.uri_token = nkn_calloc_type (1, sizeof(nkn_uri_token_t), 
								mod_http_uri_token_t);

		if (!con->http.uri_token) {
			DBG_ERR(MSG, 
				"Memory allocation failed for uri token uri:%s",
				new_uri);
			con->http.tunnel_reason_code = NKN_TR_RES_DYN_URI_TOKENIZE_ERR;
			glob_dynamic_uri_tokenize_memalloc_fail++;
			return URI_REMAP_TOKENIZE_ERR;
		}

		ret = nkn_tokenize_uri(new_uri, match, &tokenize_string[0], con->http.uri_token);
		if (ret == URI_REMAP_TOKENIZE_ERR) {
			DBG_LOG(MSG, MOD_HTTP, 
				"URL tokenization error ret=%d. Setting up tunnel path for uri=%s", 
				ret, new_uri);
			con->http.tunnel_reason_code = NKN_TR_RES_DYN_URI_TOKENIZE_ERR;
			return ret;
		}
		if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
			DBG_TRACE("Dynamic URI Tokenization for Uri: %s", uri);
		}
	}

	len = strlen(nsconf->http_config->request_policies.dynamic_uri.mapping_string);
	ptmp = p = nsconf->http_config->request_policies.dynamic_uri.mapping_string;
	pnew_uri = new_uri;
	while (len > 0) {
		while (len && *p != '$') {
			*pnew_uri = *p;
			pnew_uri++, p++; len--;
		}
		if (len == 0) break;
		if (*p != '$') return URI_REMAP_ERR;
		p++, len--;

		if (resp_cache_index) {
			if (*p == 'r') {
				*resp_idx_offset = pnew_uri - new_uri;
				p++, len--;
				DBG_LOG(MSG, MOD_HTTP, 
					"Dynamic URI positional response cache-index: %s offset %d", 
					new_uri, *resp_idx_offset);
				continue;
			}
		}
		// support $1 to $9 only.
		cnt = atoi(p);
		p++, len--;

		if (cnt<1 || cnt > 9) {
			glob_dynamic_uri_err_large_cnt++;
			return URI_REMAP_ERR;
		}

		// copy data
		bytes = match[cnt].rm_eo - match[cnt].rm_so;
		memcpy(pnew_uri, &uri[match[cnt].rm_so], bytes);
		pnew_uri += bytes;
	}
	*pnew_uri = 0;
	pnew_uri++;

	if (pnew_uri - new_uri > MAX_URI_SIZE) {
		glob_dynamic_uri_err_max_uri_size++;
		return URI_REMAP_HEX_DECODE_ERR;
	}

	len = (pnew_uri - new_uri) -1;
	if (is_hex_encoded_uri(new_uri, len)) {
		return URI_REMAP_HEX_DECODE_ERR;
	}

	add_known_header(&con->http.hdr, MIME_HDR_X_NKN_REMAPPED_URI, new_uri, len);
	SET_HTTP_FLAG(&con->http, HRF_DYNAMIC_URI);
	con->module_type = NORMALIZER;
	glob_normalize_remapped_uri++;

	if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
		DBG_TRACE("Dynamic Uri: %s remapped to %s", uri, new_uri);
	}

	DBG_LOG(MSG, MOD_HTTP, "Dynamic Uri: %s is remapped to %s", uri, new_uri);

	return URI_REMAP_OK;
}

/* 
 * Tokenize the URI based on specification mentioned in str
 *
 * IN  uri          original URI string
 * IN  regex_match	Regex match value
 * IN  str			Input specification for tokenization
 * OUT uri_token	Internal struct of parsed uri token values
 *
 * returns  		  URI_REMAP_OK: success
 *			URI_REMAP_TOKENIZE_ERR: error
 */
URI_REMAP_STATUS nkn_tokenize_uri(char *uri, nkn_regmatch_t *regex_match, const char str[], nkn_uri_token_t *uri_token)
{
	int ret, i, bytes, idx;
	char value[20];
	uint64_t ul_val;
	nkn_regmatch_t match;
	uri_token_name_t ntype;
	struct param_tok param_tokens[10];

	memset(param_tokens, 0, sizeof(param_tokens));

	ret = tokenize(str, param_tokens);

	DBG_LOG(MSG, MOD_HTTP,
		"URL tokenize: Incoming URI:%s", uri);
	for (i=0; i<ret; i++) {
		idx = param_tokens[i].match_str_idx;
		ntype = param_tokens[i].name_type;
		match = regex_match[idx];
		bytes = (match.rm_eo - match.rm_so);
		// Reset value array for each iteration.
		memset(value, 0, 20);
		strncpy(value, &uri[match.rm_so], bytes);
		value[bytes] = 0;

		// Check if the match string contains a valid number.
		if (is_valid_number(value) != 0) {
			DBG_LOG(MSG, MOD_HTTP, 
				"URL tokenize: The match string is not a valid number. "
				"Possible bad regex match [$%d] specified for token %s",
				idx, predef_token_names[ntype]);
			glob_dynamic_uri_tokenize_err++;
			return URI_REMAP_TOKENIZE_ERR;
		}

		ul_val = strtoul(value, NULL, 10);

		switch (ntype) {
			case TOKEN_FLVSEEK_OFFSET:
			{
				uri_token->flvseek_byte_start = ul_val;
				DBG_LOG(MSG, MOD_HTTP, 
					"URL tokenize: flvseek_byte_start=%ld", 
					uri_token->flvseek_byte_start);
				SET_UTF_FLAG(uri_token, UTF_FLVSEEK_BYTE_START);
				break;
            }
			case TOKEN_HTTP_RANGE_END:
			{
				uri_token->http_range_end = ul_val;
				DBG_LOG(MSG, MOD_HTTP, 
					"URL tokenize: http_range_end=%ld", 
					uri_token->http_range_end);
				SET_UTF_FLAG(uri_token, UTF_HTTP_RANGE_END);
				break;
			}
			case TOKEN_HTTP_RANGE_START:
			{
				uri_token->http_range_start = ul_val;
				DBG_LOG(MSG, MOD_HTTP, 
					"URL tokenize: http_range_start=%ld", 
					uri_token->http_range_start);
				SET_UTF_FLAG(uri_token, UTF_HTTP_RANGE_START);
				break;
			}
			default:
				DBG_LOG(MSG, MOD_HTTP,
					"URL tokenize: unknown token");
				break;
		}
		DBG_LOG(MSG, MOD_HTTP,
			"URL tokenize: token flags set: %d",
			uri_token->token_flag);
	}

	return URI_REMAP_OK;
}

/* Export this function.
 * Construct X_NKN_SEEK_URI header based on regex configuration.
 *
 * IN  con	con_t struct
 *
 * returns  0:  success
 * returns -1:  error
 */
int nkn_tokenize_gen_seek_uri(con_t *con)
{
	char *p;
	char *uri;
	char errbuf[32*1024];
	char new_uri[32*1024];
	char seek_uri[32*1024];

	int rv = -1;
	int len = 0;
	int bytes = 0;
	int cnt = 0;
	int skip_flag = 0;
	int i = 0;
	unsigned int uriattrs = 0;

	nkn_regex_ctx_t *regctx = 0;
	nkn_regmatch_t match[128];

	const namespace_config_t *nsconf;

	nsconf = con->http.nsconf;

	if (!nsconf->http_config->request_policies.seek_uri.regctx) {
		return 0;
	}
	regctx = nsconf->http_config->request_policies.seek_uri.regctx;

	if (get_known_header(&con->http.hdr, MIME_HDR_X_NKN_DECODED_URI,
			(const char **)&uri, &len, &uriattrs, &cnt)) {
		if (get_known_header(&con->http.hdr, MIME_HDR_X_NKN_URI,
			(const char **)&uri, &len, &uriattrs, &cnt)) {
			glob_seek_uri_err_no_uri++;
			return -1;
		}
	}

	memset(seek_uri, 0, 32*1024);
	memcpy(new_uri, uri, len);
	new_uri[len] = 0;

	rv = nkn_regexec_match(regctx, new_uri, sizeof(match)/sizeof(nkn_regmatch_t),
			match, errbuf, sizeof(errbuf));
	if (rv) {
		glob_seek_uri_err_regexec_match++;
		return -1;
	}

	len = strlen(nsconf->http_config->request_policies.seek_uri.mapping_string);
	p = nsconf->http_config->request_policies.seek_uri.mapping_string;
	while (len > 0) {
		while (len && *p != '$') {
			p++; len--;
		}
		if (len == 0) break;
		if (*p != '$') return -1;
		p++, len--;

		// support $1 to $9 only.
		cnt = atoi(p);
		p++, len--;

		if (cnt<1 || cnt > 9) {
			glob_seek_uri_err_large_cnt++;
			return -1;
		}

		// Strip tokens from the URI.
		new_uri[match[cnt].rm_so] = '|'; new_uri[match[cnt].rm_eo - 1] = '|';
		bytes = match[cnt].rm_eo - match[cnt].rm_so;
	}

	DBG_LOG(MSG, MOD_HTTP, "Filter new_uri=%s", new_uri);
	p = new_uri;
	while (*p) {
		if (*p == '|') {
			if (skip_flag) {
				skip_flag = 0;
			} else {
				skip_flag = 1;
			}
		} else if (!skip_flag) {
			seek_uri[i] = *p;
			i++; 
		}
		p++;
	}

	len = strlen(seek_uri);
	DBG_LOG(MSG, MOD_HTTP, "Stripped seek_uri=%s", seek_uri);

	add_known_header(&con->http.hdr, MIME_HDR_X_NKN_SEEK_URI, seek_uri, len);

	if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
		DBG_TRACE("Generate Seek Uri: %s remapped to %s", uri, seek_uri);
	}

	DBG_LOG(MSG, MOD_HTTP, "Generate Seek Uri: %s is remapped to %s", uri, seek_uri);

	return 0;
}

/* 
 * Tokenize the given specification into param tokens
 *
 * IN  str			specification of (N,V) pairs for tokenization
 * OUT param_token	param token struct holding post tokenization result
 *
 * returns  <num>:  Number of (N,V) pair(s) successfully parsed
 */
static int tokenize(const char *str, struct param_tok *param_token) {
#define MAX_TOKEN_NAME_SIZE		(40)
#define MAX_TOKEN_NUMS			(10)
#define MAX_TOKEN_MATCH_SIZE	(3)

	char post_str[256];
	int i, idx = 0, pos = 0, nv_pair_nums = 0;
	char nv_name[MAX_TOKEN_NUMS][MAX_TOKEN_NAME_SIZE];
	char nv_match[MAX_TOKEN_NUMS][MAX_TOKEN_MATCH_SIZE];
	const char *p;
	const char *p_str;
	int offset=0;

	p_str = &str[0];

	// Count the number of (N,V) pairs in the input.
	p = strpbrk(p_str, "(");
	while (p != NULL) {
		nv_pair_nums++;
		p = strpbrk(p+1, "(");
	}

	// Strip the delimiter chars and setup the post_str for quick
	// parsing of (N,V) pairs using sscanf further down
	p = p_str = &str[0];
	while (*p) {
		if((*p == '(') ||
			(*p == ')') ||
			(*p == ' ')) {
			/* Skip */
		} else if (*p == ',') {
			post_str[pos] = ' ';
			pos++;
		} else {
			post_str[pos] = *p;
			pos++;
		}
		idx++; p++;
	}
	post_str[pos] = 0;

	DBG_LOG(MSG, MOD_HTTP, 
		"Tokenize: str=%s\tpost_str:%s\tnv_pair_nums=%d", 
		str, post_str, nv_pair_nums);

	// Use the post_str and perform parsing and fill up param_token structure.
	// This would be used by the caller to setup required internal URI token 
	// parameters
	for(i=0; i<nv_pair_nums; i++) {
		char *ptr, *c;
		sscanf(&post_str[offset], "%s %s", &nv_name[i][0], &nv_match[i][0]);
		ptr = (char*) bsearch (&nv_name[i][0], 
					predef_token_names, 3, 
					MAX_TOKEN_NAME_SIZE, 
					(int(*)(const void*,const void*)) strcmp);
		c = &nv_match[i][1];
		DBG_LOG(MSG, MOD_HTTP,
			"Tokenize: nv_name[%d]=%s, nv_match[%d]=%s",
			i, nv_name[i], i, nv_match[i]);
		if (ptr != NULL) {
			param_token[i].name_type = (ptr-&predef_token_names[0][0])/MAX_TOKEN_NAME_SIZE;
			param_token[i].match_str_idx = atoi(c);
			DBG_LOG(MSG, MOD_HTTP, 
				"Tokenize: param_token[%d].name_type=%d, param_token[%d].match_str_idx=%d", 
				i, param_token[i].name_type , i, param_token[i].match_str_idx);
			offset += strlen(nv_name[i])+strlen(nv_match[i])+2;
		}
	}
	return nv_pair_nums;
}

/*
 * Utility function to validate if given string has only digits in it.
 *
 * IN  str  Input string
 *  Returns : 0 Success
 *			  1 Failure
 */
static int is_valid_number(char *str) {
	char *p, c;
	int ret = 0;
	p = str;

	while (*p) {
		c = *p;
		if (isspace(c) ||
			!isdigit(c)) {
			ret = 1;
			break;
		}
		p++;
	}
	return ret;
}

/*
 * return TRUE: if it is cacheable request.
 * return FALSE: if it is non-cacheable request.
 */
int is_cacheable_request(con_t *con, int head_to_get_enable);
int is_cacheable_request(con_t *con, int head_to_get_enable)
{
	const char *data;
	int datalen;
	uint32_t attr;
	int header_cnt;

	/* if it is not forward proxy request */
	if(!CHECK_HTTP_FLAG(&con->http, HRF_FORWARD_PROXY) ) {
		return TRUE;
	}

	/* all HEAD requests are regarded as cacheable */
	if(head_to_get_enable && CHECK_HTTP_FLAG(&con->http, HRF_METHOD_HEAD)) {
		return TRUE;
	}

	/* Check URI size */
	get_known_header(&con->http.hdr, MIME_HDR_X_NKN_URI, &data, &datalen,
			&attr, &header_cnt);
	if(datalen >= (MAX_URI_SIZE - MAX_URI_HDR_SIZE)) {
		return FALSE;
	}

	/* All GET requests except Uri has ? inside */
	if (CHECK_HTTP_FLAG(&con->http, HRF_METHOD_GET)) {
		return TRUE;
	}

	/* All others */
	return FALSE;
}

static int is_this_bad_req_tunnel_able(con_t *con)
{
        int parsing_done = 0;
	const namespace_config_t *nsconf = NULL;
        http_cb_t *phttp = &con->http;
	char req_dest_ip_str[64];
	char req_dest_port_str[64];
	int ret = 0;
        namespace_select_method_t select_method;

again:
        phttp->ns_token = http_request_to_namespace(&phttp->hdr, &nsconf);
        if (!VALID_NS_TOKEN(phttp->ns_token)) {
                if (parsing_done == 0 && 
			http_get_host_and_uri_from_bad_req(phttp) == TRUE) {
                        parsing_done = 1;
                        goto again;
                } else {
                        return FALSE;
                }
        }

	/* BZ 5263:
	 * In some cases, we can directly tunnel the request if 
	 * we know the destination IP address on the packet. For 
	 * this to work, we get the DIP from the packet so that
	 * the tunnel code can look up this number as the origin
	 * address.
	 *
	 * Note: if we get a namespace, handle origin select 
	 * type and set up some basic headers so that a tunnel 
	 * can be established. For now, only HTTP_DEST_IP is 
	 * handled.
	 */
	con->http.nsconf = nsconf;
        select_method = nsconf->http_config->origin.select_method;

	if (select_method == OSS_HTTP_DEST_IP ||
            nkn_http_is_tproxy_cluster(nsconf->http_config)) {
		/*
		 * Add REAL (packet level) destination IP/Port header (network
		 * byte order) for T-proxy
		 */
		char ip[INET6_ADDRSTRLEN];
		if (unlikely(con->ip_family == AF_INET6)) {
			if (memcmp(IPv6(con->http.remote_ipv4v6),
				IPv6(con->dest_ipv4v6), sizeof(struct in6_addr)) == 0) {
				glob_tp_req_to_ifip ++;
				return FALSE;
			}
			/* good valid destination IP address.  */
			if (inet_ntop(AF_INET6, &IPv4(con->http.remote_ipv4v6),
						&ip[0], INET6_ADDRSTRLEN) == NULL) {
				glob_tp_req_to_ifip ++;
				return FALSE;
			}
			ret = snprintf(req_dest_ip_str, sizeof(req_dest_ip_str), "%s", ip);
			add_known_header(&con->http.hdr, MIME_HDR_X_NKN_REQ_REAL_DEST_IP,
				(const char *)req_dest_ip_str, ret);
			ret = snprintf(req_dest_port_str, sizeof(req_dest_port_str), "%hu",
				ntohs(con->http.remote_sock_port));
			add_known_header(&con->http.hdr, MIME_HDR_X_NKN_REQ_REAL_DEST_PORT,
				(const char *)req_dest_port_str, ret);
		} else {
			if (IPv4(con->http.remote_ipv4v6) == IPv4(con->dest_ipv4v6)) {
				glob_tp_req_to_ifip ++;
				return FALSE;
			}
			/* good valid destination IP address.  */
			if (inet_ntop(AF_INET, &IPv4(con->http.remote_ipv4v6),
					    &ip[0], INET6_ADDRSTRLEN) == NULL) {
				glob_tp_req_to_ifip ++;
				return FALSE;
			}
			ret = snprintf(req_dest_ip_str, sizeof(req_dest_ip_str), "%s", ip);
			add_known_header(&con->http.hdr, MIME_HDR_X_NKN_REQ_REAL_DEST_IP,
				(const char *)req_dest_ip_str, ret);
			ret = snprintf(req_dest_port_str, sizeof(req_dest_port_str), "%hu",
				ntohs(con->http.remote_sock_port));
			add_known_header(&con->http.hdr, MIME_HDR_X_NKN_REQ_REAL_DEST_PORT,
				(const char *)req_dest_port_str, ret);
		}
		SET_HTTP_FLAG(&con->http, HRF_TPROXY_MODE);
        } else if (select_method == OSS_HTTP_FOLLOW_HEADER) {
		char ip[INET6_ADDRSTRLEN];
		ret = TRUE;
		if (!is_known_header_present(&con->http.hdr, MIME_HDR_HOST)) {
			ret = http_get_host_and_uri_from_bad_req(phttp);
			if (ret == FALSE) {
				glob_tp_req_to_ifip ++;
				return FALSE;
			}
		}
		if (unlikely(con->ip_family == AF_INET6)) {
			/* good valid destination IP address.  */
			if (inet_ntop(AF_INET6, &IPv4(con->http.remote_ipv4v6),
						&ip[0], INET6_ADDRSTRLEN) == NULL) {
				glob_tp_req_to_ifip ++;
				return FALSE;
			}
			ret = snprintf(req_dest_ip_str, sizeof(req_dest_ip_str), "%s", ip);
			add_known_header(&con->http.hdr, MIME_HDR_X_NKN_REQ_REAL_DEST_IP,
				(const char *)req_dest_ip_str, ret);
			ret = snprintf(req_dest_port_str, sizeof(req_dest_port_str), "%hu",
				ntohs(con->http.remote_sock_port));
			add_known_header(&con->http.hdr, MIME_HDR_X_NKN_REQ_REAL_DEST_PORT,
				(const char *)req_dest_port_str, ret);
		} else {
			/* good valid destination IP address.  */
			if (inet_ntop(AF_INET, &IPv4(con->http.remote_ipv4v6),
						&ip[0], INET6_ADDRSTRLEN) == NULL) {
				glob_tp_req_to_ifip ++;
				return FALSE;
			}
			ret = snprintf(req_dest_ip_str, sizeof(req_dest_ip_str), "%s", ip);
			add_known_header(&con->http.hdr, MIME_HDR_X_NKN_REQ_REAL_DEST_IP,
				(const char *)req_dest_ip_str, ret);
			ret = snprintf(req_dest_port_str, sizeof(req_dest_port_str), "%hu",
				ntohs(con->http.remote_sock_port));
			add_known_header(&con->http.hdr, MIME_HDR_X_NKN_REQ_REAL_DEST_PORT,
				(const char *)req_dest_port_str, ret);
                }
		SET_HTTP_FLAG(&con->http, HRF_TPROXY_MODE);
        }

        return TRUE;
}


static void setup_dest_ip_port_hdrs(mime_header_t *hdr, uint32_t dest_ip, 
				    uint16_t dest_port, int overwrite)
{
	/* Note: dest_ip and dest_port are in network byte order */

	int ret;
	char req_dest_ip_str[64];
	char req_dest_port_str[64];

	if (overwrite || 
	    !is_known_header_present(hdr, MIME_HDR_X_NKN_REQ_DEST_IP)) {
		ret = snprintf(req_dest_ip_str, sizeof(req_dest_ip_str), 
			       "%d", dest_ip);
		add_known_header(hdr, MIME_HDR_X_NKN_REQ_DEST_IP,
			 	 (const char *)req_dest_ip_str, ret);
	}

	if (overwrite || 
	    !is_known_header_present(hdr, MIME_HDR_X_NKN_REQ_DEST_PORT)) {

		ret = snprintf(req_dest_port_str, sizeof(req_dest_port_str), 
			       "%hu", dest_port);
		add_known_header(hdr, MIME_HDR_X_NKN_REQ_DEST_PORT,
			 	 (const char *)req_dest_port_str, ret);
	}
}

static int build_l7redirect_response(const mime_header_t *req,
				const char *host, int host_strlen,
				uint32_t port,
				const char *prefix_uri, int prefix_uri_strlen,
				mime_header_t *resp)

{
	int rv;
	const char *uri = 0;
	int urilen;
	u_int32_t attrs;
	int hdrcnt;
	char port_str[64];
	int port_strlen;
	int bufsize;
	char *buf;
	int bytes = 0;;

	init_http_header(resp, 0, 0);
	rv = get_known_header(req, MIME_HDR_X_NKN_URI, &uri, &urilen, &attrs, 
			    &hdrcnt);
	if (rv) {
		DBG_LOG(MSG, MOD_HTTP, 
			"get_known_header(MIME_HDR_X_NKN_URI) failed, rv=%d",
			rv);
		return 1;
	}

	port_strlen = snprintf(port_str, sizeof(port_str), "%d", port);
	bufsize = 7 /* http:// */ + host_strlen + 1 /* : */ + port_strlen +
		prefix_uri_strlen + urilen;
	if (bufsize > MAX_HTTP_HEADER_SIZE) {
		DBG_LOG(MSG, MOD_HTTP, 
			"Buffer size exceeded bufsize=%d max=%d", 
			bufsize, MAX_HTTP_HEADER_SIZE);
		return 2;
	}
	buf = alloca(bufsize + 1);

	memcpy(buf, "http://", 7);
	bytes += 7;

	memcpy(&buf[bytes], host, host_strlen);
	bytes += host_strlen;

	buf[bytes] = ':';
	bytes += 1;

	memcpy(&buf[bytes], port_str, port_strlen);
	bytes += port_strlen;

	memcpy(&buf[bytes], prefix_uri, prefix_uri_strlen);
	bytes += prefix_uri_strlen;

	memcpy(&buf[bytes], uri, urilen);
	bytes += urilen;

	buf[bytes] = '\0';

	rv = add_known_header(resp, MIME_HDR_LOCATION,
			      (const char *)buf, bufsize);
	if (rv) {
		DBG_LOG(MSG, MOD_HTTP, 
			"add_known_header(MIME_HDR_CONTENT_LOCATION) "
			"failed, rv=%d", rv);
		return 3;
	}
	return 0;
}

static int adjust_request_for_l7redir(mime_header_t *req, 
				      const cl_redir_attrs_t *predir_attr)
{
	int rv;
	const char *uri;
	int urilen;
	u_int32_t attrs;
	int hdrcnt;
	char *pbuf;
	int pbuf_strlen;

	// Adjust request URI for redirect prefix

	if (predir_attr->path_prefix_strlen) {
		rv = get_known_header(req, MIME_HDR_X_NKN_URI, 
				      &uri, &urilen, &attrs, &hdrcnt);
		if (!rv) {
			pbuf_strlen = predir_attr->path_prefix_strlen + urilen;
			if (pbuf_strlen > MAX_HTTP_HEADER_SIZE) {
				DBG_LOG(MSG, MOD_HTTP, 
					"MIME_HDR_X_NKN_URI header size "
					"exceeded size=%d max=%d",
					pbuf_strlen, MAX_HTTP_HEADER_SIZE);
				return 1;
			}
			pbuf = alloca(pbuf_strlen + 1);
			memcpy(pbuf, predir_attr->path_prefix, 
			       predir_attr->path_prefix_strlen);
			memcpy(&pbuf[predir_attr->path_prefix_strlen], 
			       uri, urilen);
			rv = add_known_header(req, MIME_HDR_X_NKN_URI, 
					      (const char *)pbuf, pbuf_strlen);
			if (rv) {
				DBG_LOG(MSG, MOD_HTTP, 
					"add_known_header(MIME_HDR_X_NKN_URI) "
					"failed, rv=%d", rv);
				return 2;
			}
						
		}
		rv = get_known_header(req, MIME_HDR_X_NKN_DECODED_URI, 
				      &uri, &urilen, &attrs, &hdrcnt);
		if (!rv) {
			pbuf_strlen = predir_attr->path_prefix_strlen + urilen;
			if (pbuf_strlen > MAX_HTTP_HEADER_SIZE) {
				DBG_LOG(MSG, MOD_HTTP, 
					"MIME_HDR_X_NKN_DECODED_URI header "
					"size exceeded size=%d max=%d",
					pbuf_strlen, MAX_HTTP_HEADER_SIZE);
				return 3;
			}
			pbuf = alloca(pbuf_strlen + 1);
			memcpy(pbuf, predir_attr->path_prefix, 
			       predir_attr->path_prefix_strlen);
			memcpy(&pbuf[predir_attr->path_prefix_strlen], 
			       uri, urilen);
			rv = add_known_header(req, MIME_HDR_X_NKN_DECODED_URI, 
					      (const char *)pbuf, pbuf_strlen);
			if (rv) {
				DBG_LOG(MSG, MOD_HTTP, 
					"add_known_header"
					"(MIME_HDR_X_NKN_DECODED_URI) "
					"failed, rv=%d", rv);
				return 4;
			}
		}

	}
	return 0;
}

/*
 * hash the header for tproxy clustering
 * 
 * Arguments:
 *   orig_md5_str: an empty buffer provided by the caller to store hash result
 *   header_cluster/str_len: string buffer/length provided by the caller, 
 */
static void http_create_header_cluster_hash(char *orig_md5_str, 
        char *header_cluster, int str_len);
static void http_create_header_cluster_hash(char *orig_md5_str, 
        char *header_cluster, int str_len)
{
    MD5_CTX ctx;
    unsigned char MD5hash[MD5_DIGEST_LENGTH];
    int i;

    MD5_Init(&ctx);
    MD5_Update(&ctx, header_cluster, str_len);
    MD5_Final(MD5hash, &ctx);

    for (i = 0; i < MD5_DIGEST_LENGTH; i++) {
        snprintf(orig_md5_str + i + i, 3, "%02x", MD5hash[i]); 
    }
}

/*
 * Include custom header which carries request's IP, port and cache index 
 * information.
 * 
 * header_cluster is a buffer with exact length MAX_HTTP_HEADER_SIZE + 1 
 * supplied by the caller 
 */
int http_create_header_cluster(con_t *con, int hdr_token, char *header_cluster, 
        int *header_cluster_len_p);
int http_create_header_cluster(con_t *con, int hdr_token, char *header_cluster, 
        int *header_cluster_len_p)
{
#define HEADER_CLUSTER_ADD(_current_p, _data, _datalen, _separator)     \
        memcpy((_current_p), (_data), (_datalen));                      \
        (_current_p) += (_datalen);                                     \
        *(_current_p) = (_separator);                                   \
        (_current_p)++;                

    char orig_md5_str[MD5_DIGEST_STR_LENGTH + 1];
    int header_len, ret_len;
    char *current_p;
    gchar *encoded_str;
    const char *data_cache_index, *data_src_ip, *data_dest_ip, *data_dest_port;
    int datalen_cache_index, datalen_src_ip, datalen_dest_ip, datalen_dest_port;
    u_int32_t attrs;
    int hdrcnt;

    (void)hdr_token;

    /* original source IP */
    if (get_known_header(&con->http.hdr, MIME_HDR_X_NKN_REQ_REAL_SRC_IP,
                         &data_src_ip, &datalen_src_ip, &attrs, &hdrcnt)) {
        DBG_LOG(MSG, MOD_HTTP, "MIME_HDR_X_NKN_REQ_REAL_SRC_IP not present!\n");
        return FALSE;
    }

    /* original destination IP */
    if (get_known_header(&con->http.hdr, MIME_HDR_X_NKN_REQ_REAL_DEST_IP,
                         &data_dest_ip, &datalen_dest_ip, &attrs, &hdrcnt)) {
        DBG_LOG(MSG, MOD_HTTP, 
                "MIME_HDR_X_NKN_REQ_REAL_DEST_IP not present!\n");
        return FALSE;
    } 

    /* original destination port */
    if (get_known_header(&con->http.hdr, MIME_HDR_X_NKN_REQ_REAL_DEST_PORT,
                  &data_dest_port, &datalen_dest_port, &attrs, &hdrcnt)) {
        DBG_LOG(MSG, MOD_HTTP, 
                "MIME_HDR_X_NKN_REQ_REAL_DEST_PORT not present!\n");
        return FALSE;
    }     

    /* cache index */    
    if (mime_hdr_get_cache_index(&con->http.hdr, &data_cache_index, 
                &datalen_cache_index, &attrs, &hdrcnt)) {
        DBG_LOG(MSG, MOD_HTTP, "cache index not present!\n");
        return FALSE;
    } else if (datalen_cache_index > MAX_URI_SIZE) {
        DBG_LOG(MSG, MOD_HTTP, "cache_index length > MAX_URI_SIZE!\n");
        return FALSE;
    }

    /* include spaces between fields */    
    header_len = 
        strlen(header_cluster_ver) + datalen_src_ip + datalen_dest_ip + 
        datalen_dest_port + datalen_cache_index + HEADER_CLUSTER_SPACES;

    if (header_len > MAX_HTTP_HEADER_SIZE) {
        DBG_LOG(MSG, MOD_HTTP, 
                "total length exceeds maximum: version %ld, src-ip %d, "
                "dest-ip %d, dest-port %s cache index %d!",
                strlen(header_cluster_ver), datalen_src_ip, 
                datalen_dest_ip, data_dest_port, datalen_cache_index);
        return FALSE;
    }

    orig_md5_str[MD5_DIGEST_STR_LENGTH] = 0;
    header_cluster[MAX_HTTP_HEADER_SIZE] = 0;

    current_p = header_cluster;    
    HEADER_CLUSTER_ADD(current_p, header_cluster_ver, 
                       strlen(header_cluster_ver), ' ');
    HEADER_CLUSTER_ADD(current_p, data_src_ip, datalen_src_ip, ' '); 
    HEADER_CLUSTER_ADD(current_p, data_dest_ip, datalen_dest_ip, ' ');
    HEADER_CLUSTER_ADD(current_p, data_dest_port, datalen_dest_port, ' ');
    HEADER_CLUSTER_ADD(current_p, data_cache_index, datalen_cache_index, 0);    

    DBG_LOG(MSG, MOD_HTTP,
            "header_cluster: %s, header_len %d", header_cluster, header_len);

    /* Base64(SrcIP,DestIP,DestPort)MD5(SrcIP,DestIP,DestPort) */
    http_create_header_cluster_hash(orig_md5_str, header_cluster, header_len);

    encoded_str = g_base64_encode((unsigned char *)header_cluster, header_len);
    if (!encoded_str) {
    	// Should never happen
        DBG_LOG(MSG, MOD_HTTP, "g_base64_encode error\n");
        return FALSE;
    }

    ret_len = snprintf(header_cluster, MAX_HTTP_HEADER_SIZE, "%s%s",
                       encoded_str, orig_md5_str);
    g_free(encoded_str);

    if (ret_len > MAX_HTTP_HEADER_SIZE) {
        DBG_LOG(MSG, MOD_HTTP,
                "cluster header exceeds maximum size %d", MAX_HTTP_HEADER_SIZE);
        return FALSE;

    }    

    *header_cluster_len_p = ret_len;

    return TRUE;
}

/*
 * Handle custom header which carries request's IP, port and cache index 
 * information
 */
int http_decode_header_cluster(con_t *con, char *orig_str, 
                               int orig_len, int hdr_token);
int http_decode_header_cluster(con_t *con, char *orig_str, 
                               int orig_len, int hdr_token)
{
    char orig_md5_str_receive[MD5_DIGEST_STR_LENGTH + 1];
    char orig_md5_str[MD5_DIGEST_STR_LENGTH + 1];
    int  header_len;
    int  rv, space_counter;
    unsigned int i;    
    char *strbuf, *orig_ver_str, *orig_src_ip_str, *orig_dest_ip_str, 
         *orig_dest_port_str, *cache_index, *header_cluster;
    guchar *decoded_str;
    gsize decoded_len;
    ip_addr_t src_ip, dest_ip;

    /* sanity checks */    
    if (orig_str == NULL) {
        DBG_LOG(MSG, MOD_HTTP, "orig_str is NULL");
        return FALSE;    
    }

    if (orig_len > MAX_HTTP_HEADER_SIZE) {
        DBG_LOG(MSG, MOD_HTTP,
                "%s length %d exceeds maximum %d",
		http_known_headers[hdr_token].name,
                orig_len, MAX_HTTP_HEADER_SIZE);
        return FALSE;
    }

    /* Remove Base64 from 
       Base64(SrcIP,DestIP,DestPort)MD5(SrcIP,DestIP,DestPort) */
    if (orig_len < (MD5_DIGEST_STR_LENGTH + 1)) {
        DBG_LOG(MSG, MOD_HTTP,
                "%s length %d less than MD5_DIGEST_STR_LENGTH + 1 bytes",
		http_known_headers[hdr_token].name, orig_len);
    	return FALSE;
    }


    header_len = orig_len - MD5_DIGEST_STR_LENGTH;
    strbuf = alloca(header_len + 1);
    memcpy(strbuf, orig_str, header_len);
    strbuf[header_len] = '\0';

    /* get MD5 value calculated by the sender */
    memcpy(orig_md5_str_receive, &orig_str[header_len], 
           MD5_DIGEST_STR_LENGTH);
    orig_md5_str_receive[MD5_DIGEST_STR_LENGTH] = '\0';

    /* decode the string. return may not be null terminated! */
    decoded_str = g_base64_decode(strbuf, &decoded_len);
    if (!decoded_str) {
        DBG_LOG(MSG, MOD_HTTP, "g_base64_decode() failed");
	return FALSE;
    }

    if (decoded_len > MAX_HTTP_HEADER_SIZE) {
        DBG_LOG(MSG, MOD_HTTP, "decoded_len too long");
        return FALSE;
    }
 
    header_cluster = alloca(decoded_len + 1);
    header_cluster[decoded_len] = 0;    

    memcpy(header_cluster, decoded_str, decoded_len);
    g_free(decoded_str);

    DBG_LOG(MSG, MOD_HTTP, "header_cluster: %s, len %ld", 
            header_cluster, decoded_len);     

    /* hash decoded string, then compare the MD5 value with the sender 
       calculated one */
    http_create_header_cluster_hash(orig_md5_str, header_cluster, decoded_len);
    orig_md5_str[MD5_DIGEST_STR_LENGTH] = 0;

    if (strncmp(orig_md5_str_receive, orig_md5_str,
                MD5_DIGEST_LENGTH + MD5_DIGEST_LENGTH) != 0) { 
        DBG_LOG(MSG, MOD_HTTP, 
                "sender/receiver hash value don't match: orig_md5_str_receive %s orig_md5_str %s",
                orig_md5_str_receive, orig_md5_str);
        return FALSE;
    }

    /* find start of cache-index */    
    space_counter = 0;
    for (i = 0; i < decoded_len; i++) {
        if (header_cluster[i] == ' ') {
            space_counter++;
 
            if (space_counter == HEADER_CLUSTER_SPACES) {
                cache_index = header_cluster + i + 1;
                break;
            }
        }
    }    
    
    if (space_counter != HEADER_CLUSTER_SPACES) {
        DBG_LOG(ERROR, MOD_HTTP, "not enough spaces in the header");
        return FALSE;
    }

    /* the first 4 fields cannot be longer than the offset of last space (i) */
    orig_ver_str = alloca(i + 1);
    orig_src_ip_str = alloca(i + 1);
    orig_dest_ip_str = alloca(i + 1);
    orig_dest_port_str = alloca(i + 1);

    orig_ver_str[i] = 0;
    orig_src_ip_str[i] = 0;
    orig_dest_ip_str[i] = 0;
    orig_dest_port_str[i] = 0;

    if (sscanf(header_cluster, "%s %s %s %s", orig_ver_str, orig_src_ip_str, 
                orig_dest_ip_str, orig_dest_port_str) != 4) {
        DBG_LOG(MSG, MOD_HTTP, 
                "wrong content in %s %s",
		http_known_headers[hdr_token].name,
                header_cluster);
        return FALSE;
    }

    if (strncmp(orig_ver_str, header_cluster_ver, 
                strlen(header_cluster_ver)) != 0) {
        DBG_LOG(MSG, MOD_HTTP, "version mismatch");
        return FALSE;
    }                

    if (inet_pton(con->src_ipv4v6.family, orig_src_ip_str, 
                  &src_ip.addr) <= 0) {
        DBG_LOG(MSG, MOD_HTTP, "invalid src_ip %s in addr family %d", 
                orig_src_ip_str, con->src_ipv4v6.family);
        return FALSE;
    }

    if (inet_pton(con->src_ipv4v6.family, orig_dest_ip_str,
                  &dest_ip.addr) <= 0) {
        DBG_LOG(MSG, MOD_HTTP, "invalid dest_ip %s in addr family %d", 
                orig_dest_ip_str, con->src_ipv4v6.family);
        return FALSE;
    } 

    if (strlen(orig_dest_port_str) > MAX_PORT_STR_LEN) {
        DBG_LOG(MSG, MOD_HTTP, "invalid dest_port %s", orig_dest_port_str);
        return FALSE;
    }

    /* override src_ipv4v6 in con */
    con->src_ipv4v6.addr = src_ip.addr;

    rv = add_known_header(&con->http.hdr, MIME_HDR_X_NKN_REQ_REAL_DEST_IP,
                     (const char *)orig_dest_ip_str, strlen(orig_dest_ip_str));
    if (rv) {
        DBG_LOG(MSG, MOD_HTTP, 
              "Error in add_known_header for MIME_HDR_X_NKN_REQ_REAL_DEST_IP");
        return FALSE;
    }

    rv = add_known_header(&con->http.hdr, MIME_HDR_X_NKN_REQ_REAL_DEST_PORT,
                     (const char *)orig_dest_port_str, 
                     strlen(orig_dest_port_str));
    if (rv) {
        DBG_LOG(MSG, MOD_HTTP, "Error in add_known_header for MIME_HDR_X_NKN_REQ_REAL_DEST_PORT");
        return FALSE;
    }

    if (hdr_token != MIME_HDR_X_NKN_CL7_PROXY) {
        rv = add_known_header(&con->http.hdr, MIME_HDR_X_NKN_CACHE_INDEX,
                             (const char *)cache_index,
                             strlen(cache_index));
        if (rv) {
            DBG_LOG(MSG, MOD_HTTP, 
                "Error in add_known_header for MIME_HDR_X_NKN_CACHE_INDEX");
            return FALSE;
        }
    }

    SET_HTTP_FLAG(&con->http, HRF_TPROXY_MODE);

    return TRUE; 
}

http_orig_header_ret_t http_decode_header_cluster_headers(con_t *con);
http_orig_header_ret_t http_decode_header_cluster_headers(con_t *con)
{
    int orig_hdr_token = -1;
    const char *orig_data = NULL;
    int orig_len = 0;
    u_int32_t attrs;
    int hdrcnt;
    int rv;    

    if ((rv = get_known_header(&con->http.hdr, MIME_HDR_X_NKN_CLUSTER_TPROXY,
                               &orig_data, &orig_len, &attrs, &hdrcnt)) == 0) {
        orig_hdr_token = MIME_HDR_X_NKN_CLUSTER_TPROXY;

    } else if ((rv = get_known_header(&con->http.hdr, MIME_HDR_X_NKN_CL7_PROXY,
                             &orig_data, &orig_len, &attrs, &hdrcnt)) == 0) {
        orig_hdr_token = MIME_HDR_X_NKN_CL7_PROXY;
    } else {
        return HTTP_ORIG_HEADER_NOT_PRESENT;
    }

    TSTR(orig_data, orig_len, orig_str);
    if (http_decode_header_cluster(con, orig_str, orig_len, orig_hdr_token) 
		         == FALSE) {
	CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
	http_close_conn(con, NKN_HTTP_PARSER);
	return HTTP_ORIG_HEADER_ERR;                
    }
    SET_HTTP_FLAG(&con->http, HRF_TIER2_MODE);
    return HTTP_ORIG_HEADER_PRESENT;
}


int http_handle_request_post_pe(con_t *con);
int http_handle_request_url_cat_lookup(con_t *con);

int http_add_real_dest_ip_port_header(con_t *con);
int http_add_real_dest_ip_port_header(con_t *con)
{
    /* TODO: need to support IPv6 */
    int ret;
    char ip[INET6_ADDRSTRLEN];
    char req_dest_ip_str[64];
    char req_dest_port_str[64];

    /*
     * Add REAL (packet level) destination IP/Port header (network
     * byte order) for T-proxy
     */
    if (unlikely(con->ip_family == AF_INET6)) {
	if (memcmp(IPv6(con->http.remote_ipv4v6),
		IPv6(con->dest_ipv4v6), sizeof(struct in6_addr)) == 0) {
	    glob_tp_req_to_ifip ++;
	    CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
	    http_close_conn(con, NKN_HTTP_TPROXY_OUR_IFIP);
	    return FALSE;
	}
	/* good valid destination IP address.  */
	if (inet_ntop(AF_INET6, &IPv4(con->http.remote_ipv4v6),
		    &ip[0], INET6_ADDRSTRLEN) == NULL) {
	    glob_tp_req_to_ifip ++;
	    CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
	    http_close_conn(con, NKN_HTTP_TPROXY_OUR_IFIP);
	    return FALSE;
	}
	ret = snprintf(req_dest_ip_str, sizeof(req_dest_ip_str), "%s", ip);
	add_known_header(&con->http.hdr, MIME_HDR_X_NKN_REQ_REAL_DEST_IP,
			(const char *)req_dest_ip_str, ret);
	ret = snprintf(req_dest_port_str, sizeof(req_dest_port_str), "%hu",
			ntohs(con->http.remote_sock_port));
	add_known_header(&con->http.hdr, MIME_HDR_X_NKN_REQ_REAL_DEST_PORT,
			(const char *)req_dest_port_str, ret);
    } else {
	if (IPv4(con->http.remote_ipv4v6) == IPv4(con->dest_ipv4v6)) {
	    glob_tp_req_to_ifip ++;
	    CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
	    http_close_conn(con, NKN_HTTP_TPROXY_OUR_IFIP);
	    return FALSE;
	}
	/* good valid destination IP address.  */
	if (inet_ntop(AF_INET, &IPv4(con->http.remote_ipv4v6),
				    &ip[0], INET6_ADDRSTRLEN) == NULL) {
	    glob_tp_req_to_ifip ++;
	    CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
	    http_close_conn(con, NKN_HTTP_TPROXY_OUR_IFIP);
	    return FALSE;
	}
	ret = snprintf(req_dest_ip_str, sizeof(req_dest_ip_str), "%s", ip);
	add_known_header(&con->http.hdr, MIME_HDR_X_NKN_REQ_REAL_DEST_IP,
			(const char *)req_dest_ip_str, ret);
	ret = snprintf(req_dest_port_str, sizeof(req_dest_port_str), "%hu",
			ntohs(con->http.remote_sock_port));
	add_known_header(&con->http.hdr, MIME_HDR_X_NKN_REQ_REAL_DEST_PORT,
			(const char *)req_dest_port_str, ret);
    }
    SET_HTTP_FLAG(&con->http, HRF_TPROXY_MODE);

    return TRUE;
}

int http_add_real_ip_port_header(con_t *con);
int http_add_real_ip_port_header(con_t *con)
{
    char data_src_ip[INET6_ADDRSTRLEN + 1];

    /* add real src IP header */    
    data_src_ip[INET6_ADDRSTRLEN] = 0;
    if (inet_ntop(con->src_ipv4v6.family, &con->src_ipv4v6.addr,
                  data_src_ip, INET6_ADDRSTRLEN) == NULL) {
        CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
        http_close_conn(con, NKN_HTTP_PARSER);
        return FALSE;
    }

    add_known_header(&con->http.hdr, MIME_HDR_X_NKN_REQ_REAL_SRC_IP,
                     (const char *)data_src_ip, strlen(data_src_ip));
   
    if (is_known_header_present(&con->http.hdr, 
                MIME_HDR_X_NKN_REQ_REAL_DEST_IP) && 
        is_known_header_present(&con->http.hdr, 
                MIME_HDR_X_NKN_REQ_REAL_DEST_PORT)) {
        return TRUE;
    }  
 
    return http_add_real_dest_ip_port_header(con);

}

static int is_bind_ip_nomatch(con_t *con, const namespace_config_t *nsconf) {
    int i32index;

    if (!nsconf->ip_bind_list.n_ips) {
        return 0;
    }

    for (i32index = 0; i32index < nsconf->ip_bind_list.n_ips; i32index++) {
        if ((AF_INET == con->dest_ipv4v6.family) && (AF_INET == nsconf->ip_bind_list.ip_addr[i32index].ip_type)) {
            if (IPv4(con->dest_ipv4v6) == nsconf->ip_bind_list.ip_addr[i32index].ipaddr[0]) {
                glob_http_ns_bind_ip_match++;
                return 0;
            }
        }
    }

    glob_http_ns_bind_ip_nomatch++;

    return 1;
}

/*
 * apply_url_filter() -- Apply namespace specific URL filter
 *	Return:
 *	  ==0 => Deny Request
 *	  !=0 => Allow Request
 */
static int apply_url_filter(const namespace_config_t *nsconf, con_t *con)
{
    int ret;
    int rv;
    mime_header_t resp_hdr;
    char content_length_str[128];
    namespace_uf_reject_t reject_action;

    reject_action = url_filter_lookup(nsconf->uf_config, &con->http.hdr);

    switch (reject_action) {
    case NS_UF_REJECT_NOACTION:
    	ret = 1; // allow
	break;

    case NS_UF_REJECT_RESET:
    case NS_UF_URI_SIZE_REJECT_RESET:
	if (reject_action == NS_UF_REJECT_RESET)
	    con->http.subcode = NKN_UF_CONN_RESET;
	else
	    con->http.subcode = NKN_UF_URI_SIZE_CONN_RESET;
	reset_close_conn(con);
	ret = 0; // deny
	break;

    case NS_UF_REJECT_CLOSE:
    case NS_UF_URI_SIZE_REJECT_CLOSE:
	if (reject_action == NS_UF_REJECT_CLOSE)
	    con->http.subcode = NKN_UF_CONN_CLOSE;
	else
	    con->http.subcode = NKN_UF_URI_SIZE_CONN_CLOSE;
	close_conn(con);
	ret = 0; // deny
	break;

    case NS_UF_REJECT_HTTP_403:
    case NS_UF_URI_SIZE_REJECT_HTTP_403:
	if (reject_action ==  NS_UF_REJECT_HTTP_403) {
	    http_build_res_403(&con->http, NKN_UF_HTTP_403_FORBIDDEN); 
	    con->http.subcode = NKN_UF_HTTP_403_FORBIDDEN;
	    http_close_conn(con, NKN_UF_HTTP_403_FORBIDDEN);
	} else {
	    http_build_res_403(&con->http, NKN_UF_URI_SIZE_HTTP_403_FORBIDDEN); 
	    con->http.subcode = NKN_UF_URI_SIZE_HTTP_403_FORBIDDEN;
	    http_close_conn(con, NKN_UF_URI_SIZE_HTTP_403_FORBIDDEN);
	}
	ret = 0; // deny
	break;

    case NS_UF_REJECT_HTTP_404:
    case NS_UF_URI_SIZE_REJECT_HTTP_404:
	if (reject_action ==  NS_UF_REJECT_HTTP_404) {
	    http_build_res_404(&con->http, NKN_UF_HTTP_404_NOTFOUND); 
	    con->http.subcode = NKN_UF_HTTP_404_NOTFOUND;
	    http_close_conn(con, NKN_UF_HTTP_404_NOTFOUND);
	} else {
	    http_build_res_404(&con->http, NKN_UF_URI_SIZE_HTTP_404_NOTFOUND); 
	    con->http.subcode = NKN_UF_URI_SIZE_HTTP_404_NOTFOUND;
	    http_close_conn(con, NKN_UF_URI_SIZE_HTTP_404_NOTFOUND);
	}
	ret = 0; // deny
	break;

    case NS_UF_REJECT_HTTP_301:
    case NS_UF_REJECT_HTTP_302:
    case NS_UF_URI_SIZE_REJECT_HTTP_301:
    case NS_UF_URI_SIZE_REJECT_HTTP_302:
    {
	const char *redir_host;
	int redir_host_strlen;
	const char *redir_url;
	int redir_url_strlen;

	const char *req_uri;
	int req_urilen;
	unsigned int req_uriattrs;
	int req_uri_cnt;
	char loc_url[MAX_URI_HDR_HOST_SIZE + MAX_URI_SIZE + 1024 /*pad*/];

	if (reject_action == NS_UF_REJECT_HTTP_301) {
	    redir_host = nsconf->uf_config->uf_reject.http_301.redir_host;
	    redir_host_strlen = 
	    	nsconf->uf_config->uf_reject.http_301.redir_host_strlen;
	    redir_url = nsconf->uf_config->uf_reject.http_301.redir_url;
	    redir_url_strlen = 
	    	nsconf->uf_config->uf_reject.http_301.redir_url_strlen;
	} else if (reject_action == NS_UF_REJECT_HTTP_302) {
	    redir_host = nsconf->uf_config->uf_reject.http_302.redir_host;
	    redir_host_strlen = 
	    	nsconf->uf_config->uf_reject.http_302.redir_host_strlen;
	    redir_url = nsconf->uf_config->uf_reject.http_302.redir_url;
	    redir_url_strlen = 
	    	nsconf->uf_config->uf_reject.http_302.redir_url_strlen;
	} else if (reject_action == NS_UF_URI_SIZE_REJECT_HTTP_301) {
	    redir_host =
		nsconf->uf_config->uf_uri_size_reject.http_301.redir_host;
	    redir_host_strlen = 
	       nsconf->uf_config->uf_uri_size_reject.http_301.redir_host_strlen;
	    redir_url =
		nsconf->uf_config->uf_uri_size_reject.http_301.redir_url;
	    redir_url_strlen = 
		nsconf->uf_config->uf_uri_size_reject.http_301.redir_url_strlen;
	} else {
	    redir_host =
		nsconf->uf_config->uf_uri_size_reject.http_302.redir_host;
	    redir_host_strlen = 
	       nsconf->uf_config->uf_uri_size_reject.http_302.redir_host_strlen;
	    redir_url =
		nsconf->uf_config->uf_uri_size_reject.http_302.redir_url;
	    redir_url_strlen = 
	    	nsconf->uf_config->uf_uri_size_reject.http_302.redir_url_strlen;
	}

	if (!redir_url) { // use request URL
	    rv = get_known_header(&con->http.hdr, MIME_HDR_X_NKN_URI,
	    			  (const char **)&req_uri, &req_urilen, 
				  &req_uriattrs, &req_uri_cnt);
	    if (!rv) {
	    	redir_url = req_uri;
	    	redir_url_strlen = req_urilen;
	    }
	}

	if ((7 /* http:// */ + 
	    redir_host_strlen + redir_url_strlen) < (int) sizeof(loc_url)) {
	    strcpy(loc_url, "http://");
	    memcpy(&loc_url[7], redir_host, redir_host_strlen);
	    memcpy(&loc_url[7 + redir_host_strlen], redir_url, 
	    	   redir_url_strlen);
	    loc_url[7 + redir_host_strlen + redir_url_strlen] = '\0';
	} else {
	    // Should never happen due to host and url length checks
	    loc_url[0] = '\0';
	}

	init_http_header(&resp_hdr, 0, 0);
	rv = add_known_header(&resp_hdr, MIME_HDR_LOCATION,
			      loc_url, 
			      (7 + redir_host_strlen + redir_url_strlen));
	if (rv) {
	    DBG_LOG(MSG, MOD_HTTP, 
		    "add_known_header() failed, rv=%d", rv);
	}

	if (reject_action == NS_UF_REJECT_HTTP_301) {
	    http_build_res_301(&con->http, NKN_UF_HTTP_301_REDIRECT_PERM, 
			       &resp_hdr);
	    con->http.subcode = NKN_UF_HTTP_301_REDIRECT_PERM;
	    http_close_conn(con, NKN_UF_HTTP_301_REDIRECT_PERM);
	} else if (reject_action == NS_UF_REJECT_HTTP_302) {
	    http_build_res_302(&con->http, NKN_UF_HTTP_302_REDIRECT, &resp_hdr);
	    con->http.subcode = NKN_UF_HTTP_302_REDIRECT;
	    http_close_conn(con, NKN_UF_HTTP_302_REDIRECT);
	} else if (reject_action == NS_UF_URI_SIZE_REJECT_HTTP_301) {
	    http_build_res_301(&con->http,
				NKN_UF_URI_SIZE_HTTP_301_REDIRECT_PERM, 
			       &resp_hdr);
	    con->http.subcode = NKN_UF_URI_SIZE_HTTP_301_REDIRECT_PERM;
	    http_close_conn(con, NKN_UF_URI_SIZE_HTTP_301_REDIRECT_PERM);
	} else {
	    http_build_res_302(&con->http, NKN_UF_URI_SIZE_HTTP_302_REDIRECT,
				&resp_hdr);
	    con->http.subcode = NKN_UF_URI_SIZE_HTTP_302_REDIRECT;
	    http_close_conn(con, NKN_UF_URI_SIZE_HTTP_302_REDIRECT);
	}
	shutdown_http_header(&resp_hdr);
	ret = 0; // deny
	break;
    }

    case NS_UF_REJECT_HTTP_200:
    case NS_UF_URI_SIZE_REJECT_HTTP_200:
    {
	if (reject_action == NS_UF_REJECT_HTTP_200) {
	    rv = snprintf(content_length_str, sizeof(content_length_str), "%d",
		      nsconf->uf_config->
		      	uf_reject.http_200.response_body_strlen);
	} else {
	    rv = snprintf(content_length_str, sizeof(content_length_str), "%d",
		      nsconf->uf_config->
		      	uf_uri_size_reject.http_200.response_body_strlen);
	}

	init_http_header(&resp_hdr, 0, 0);
	rv = add_known_header(&resp_hdr, MIME_HDR_CONTENT_LENGTH,
			      content_length_str, rv);
	if (rv) {
	    DBG_LOG(MSG, MOD_HTTP, 
		    "add_known_header() failed, rv=%d", rv);
	}

	rv = add_known_header(&resp_hdr, MIME_HDR_CONTENT_TYPE, "text/html", 9);
	if (rv) {
	    DBG_LOG(MSG, MOD_HTTP, 
		    "add_known_header() failed, rv=%d", rv);
	}
	http_build_res_200_ext(&con->http, &resp_hdr);

	if (reject_action == NS_UF_REJECT_HTTP_200) {
	    // Add inline response body
	    con->http.resp_body_buf = 
		nsconf->uf_config->uf_reject.http_200.response_body;

	    // Note: -body_len denotes not malloc'ed
	    con->http.resp_body_buflen =
		-nsconf->uf_config->uf_reject.http_200.response_body_strlen;

	    con->http.subcode = NKN_UF_HTTP_200_WITH_BODY;
	    http_close_conn(con, NKN_UF_HTTP_200_WITH_BODY);
	} else {
	    // Add inline response body
	    con->http.resp_body_buf = 
		nsconf->uf_config->uf_uri_size_reject.http_200.response_body;

	    // Note: -body_len denotes not malloc'ed
	    con->http.resp_body_buflen =
		-nsconf->uf_config->uf_uri_size_reject.http_200.response_body_strlen;
	    con->http.subcode = NKN_UF_URI_SIZE_HTTP_200_WITH_BODY;
	    http_close_conn(con, NKN_UF_URI_SIZE_HTTP_200_WITH_BODY);
	}

	shutdown_http_header(&resp_hdr);
	ret = 0; // deny
	break;
    }
    case NS_UF_REJECT_HTTP_CUSTOM:
	// Not yet implemented.
	// Fall through

    default:
	DBG_LOG(MSG, MOD_HTTP, 
		"Invalid URL filter reject action (%d)", 
		reject_action);
	con->http.subcode = NKN_HTTP_NAMESPACE;
	reset_close_conn(con);
	ret = 0; //deny
	break;
    }
    return ret;
}

int http_handle_request(con_t *con)
{
	int ret;
	char outdumpbuf[4096];
	const namespace_config_t *nsconf;
	u_int32_t attrs;
	int hdrcnt;
	char req_dest_ip_str[64];
	char req_dest_port_str[64];
	int rv = 0;
        http_orig_header_ret_t orig_header_ret;
	intercept_type_t intercept_action;
	const cl_redir_attrs_t *predir_attr;
	char *ns_name;
	namespace_token_t ns_token;
	namespace_token_t node_ns_token;
	char *target_host;
	int target_hostlen = 0 ;
	uint16_t target_port;
	mime_header_t redir_resphdr;
	int valid_http_request;
	cluster_descriptor_t *cldesc;

	if (CHECK_CON_FLAG(con, CONF_RETRY_HDL_REQ)) {
		UNSET_CON_FLAG(con, CONF_RETRY_HDL_REQ);
		goto retry_request;
	}

        // Parsing the HTTP request
	ret = http_parse_header(&con->http);
        if ( ret == HPS_NEED_MORE_DATA ) {
		UNSET_CON_FLAG(con, CONF_BLOCK_RECV);
                return TRUE;
        } else if ( ret == HPS_ERROR ) {
		int subcode = NKN_HTTP_PARSER;
		SET_CON_FLAG(con, CONF_BLOCK_RECV);
		DBG_LOG(MSG, MOD_HTTP, "http request parse error");

		if(!CHECK_HTTP_FLAG(&con->http, HRF_HTTP_09))
			glob_http_req_parse_err++;

	       /*
		* If it is not supported method, we return;
		*/
                if (con->http.respcode == 405) {
		    subcode = NKN_HTTP_METHOD_NOT_SUPPORTED;
		    CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
                    http_build_res_405(&con->http, subcode);
                    http_close_conn(con, subcode);
                    return FALSE;
                }

               /*
                * There is no need to tunnel this request further,
                * since this there is an invalid expectation specified
                * as part of this request. Just return status 
                * 417(Expectation Failed).
                */
                if (con->http.respcode == 417) {
		    CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
                    http_build_res_417(&con->http, subcode);
                    http_close_conn(con, subcode);
                    return FALSE;
                }

               /*
                * Possibly tunnel the request to origin so that it could be
                * handled by the origin server.
                */
		if (is_this_bad_req_tunnel_able(con) == TRUE) {

            if (is_bind_ip_nomatch(con, con->http.nsconf)) {
                CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
                http_build_res_403(&con->http, NKN_HTTP_NS_BIND_IP_NOMATCH);
                http_close_conn(con, NKN_HTTP_NS_BIND_IP_NOMATCH);
                return FALSE;
            }

            // refcnt=1, set via is_this_bad_req_tunnel_able()
			DBG_LOG(MSG, MOD_HTTP, "forwarding to tunnel path");
			/*
			 * if tunnel fails, resposnse 400 will be sent
			 * with sub error code for tunnel failure not parse
			 * error sub code. It will help to understand the 
			 * tunnel failures.
			 */
			if (con->http.tunnel_reason_code == NKN_TR_UNKNOWN) {
				con->http.tunnel_reason_code = NKN_TR_REQ_BAD_REQUEST;
			}
			ret = fp_make_tunnel(con, -1, 0, 0);
			if(ret == 0) {
				if (con->http.tunnel_reason_code == NKN_TR_REQ_MAX_SIZE_LIMIT) {
					// don't receive any event from client. BUG 8210
					NM_del_event_epoll(con->fd);
				}
				// HTTP 0.9 doesn't support persistent connection.
				if (CHECK_HTTP_FLAG(&con->http, HRF_HTTP_09)) {
					CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
				} else {
					glob_tunnel_req_http_parse_err++;
				}
				return TRUE;
			}
			DBG_LOG(MSG, MOD_HTTP, "failed to tunnel");
			subcode = ret;
                }
		CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
                http_build_res_400(&con->http, subcode);
                http_close_conn(con, subcode);
                return FALSE;
	}
	SET_CON_FLAG(con, CONF_BLOCK_RECV);
        if(CHECK_CON_FLAG(con, CONF_HTTPS_HDR_RECVD)) {
            AO_fetch_and_add1(&glob_https_tot_transactions);
        } else { 
	    AO_fetch_and_add1(&glob_http_tot_transactions);
	}

	if (DO_DBG_LOG(MSG, MOD_HTTP)) {
		DBG_LOG(MSG, MOD_HTTP, "con=%p fd=%d\n%s", con, con->fd,
			dump_http_header(&con->http.hdr, outdumpbuf,
					 sizeof(outdumpbuf), 1));
	}

	/*
	 * Check if a trace is requested and is enabled
 	 */
	if (nkn_http_trace_enable &&
	    is_unknown_header_present(&con->http.hdr, 
	    	MIME_HDR_X_NKN_TRACE_STR, MIME_HDR_X_NKN_TRACE_LEN)) {
		SET_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST);
	}

retry_request:

	orig_header_ret = http_decode_header_cluster_headers(con);
	if (orig_header_ret == HTTP_ORIG_HEADER_ERR) {
		return FALSE;
	} /* else  if (orig_header_ret == HTTP_ORIG_HEADER_NOT_PRESENT) {
		// fall through: right case 
	} */    

        valid_http_request = http_check_request(&con->http);

	/*
	 * Apply request intercept action
	 */
	intercept_action = namespace_request_intercept(con->fd, &con->http.hdr,
						    ntohl(IPv4(con->dest_ipv4v6)),
						    ntohs(con->dest_port),
						    &target_host,
						    &target_hostlen,
						    &target_port,
						    &predir_attr, 
						    &ns_name, &ns_token, 
						    &node_ns_token,
						    &cldesc);
	if (valid_http_request && (intercept_action != INTERCEPT_NONE)) {
		con->http.nsconf = namespace_to_config(ns_token); // 2nd ref
		release_namespace_token_t(ns_token); // release 1st reference

        if (is_bind_ip_nomatch(con, con->http.nsconf)) {
            CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
            http_build_res_403(&con->http, NKN_HTTP_NS_BIND_IP_NOMATCH);
            http_close_conn(con, NKN_HTTP_NS_BIND_IP_NOMATCH);
            release_namespace_token_t(node_ns_token);
            release_namespace_token_t(ns_token);
            return FALSE;
        }

		if ((intercept_action == INTERCEPT_REDIRECT) ||
		    (intercept_action == INTERCEPT_PROXY)) {

		    // Only allow intercept action on defined HTTP methods
		    if (!(con->http.flag & HRF_HTTP_METHODS)) {
		    	// Undefined HTTP method
		    	CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
		    	http_build_res_501(&con->http, 
					   NKN_HTTP_METHOD_NOT_SUPPORTED);
		    	http_close_conn(con, NKN_HTTP_METHOD_NOT_SUPPORTED);
			release_namespace_token_t(node_ns_token);
		    	release_namespace_token_t(ns_token);
		    	glob_cll7_intercept_badmethod++;
		    	return FALSE;
		    }
		    intercept_action = 
			apply_CL7_intercept_policies(con, cldesc, 
							intercept_action);
		}

	    	if (intercept_action == INTERCEPT_REDIRECT) {
			release_namespace_token_t(node_ns_token);
	    		if (predir_attr->allow_redir_local || 
			    !is_local_ip(target_host, target_hostlen)) {

				ret = build_l7redirect_response(&con->http.hdr,
					target_host, target_hostlen,
					target_port,
					predir_attr->path_prefix,
					predir_attr->path_prefix_strlen,
					&redir_resphdr);

				if (!ret) {
					int bytes;
					char tbuf[1024];

					TSTR(target_host, target_hostlen, 
					     thost);
					DBG_LOG(MSG, MOD_L7REDIR,
						"INTERCEPT_REDIRECT: "
						"con=%p fd=%d req=%p "
						"ns=[%s] "
						"302 redirect => %s:%hu",
						con, con->fd, &con->http.hdr,
						ns_name ? ns_name : "",
						thost, target_port);

					// Access log status header
					bytes = snprintf(tbuf, sizeof(tbuf), 
						"Use=%s:%hu", thost, 
						target_port);
					update_known_header(&con->http.hdr, 
						MIME_HDR_X_NKN_CL7_STATUS,
						tbuf, bytes, 0);
						
					http_build_res_302(&con->http, 0, 
						   	&redir_resphdr);
					shutdown_http_header(&redir_resphdr);
					CLEAR_HTTP_FLAG(&con->http, 
						HRF_CONNECTION_KEEP_ALIVE);
					http_close_conn(con, NKN_HTTP_REQUEST);
	    				release_namespace_token_t(ns_token);
					glob_cll7redir_intercept_success++;
					return FALSE;
				} else {
					// Error in building redirect response
					DBG_LOG(MSG, MOD_HTTP, 
						"build_redirect_response() "
						"error, con=%p fd=%d req=%p "
						"rv=%d", 
						con, con->fd, &con->http.hdr, 
						ret);
					CLEAR_HTTP_FLAG(&con->http, 
						HRF_CONNECTION_KEEP_ALIVE);
					http_build_res_501(&con->http, 
						       NKN_CL7_RESPONSE_ERR);
					shutdown_http_header(&redir_resphdr);
					http_close_conn(con, 
							NKN_CL7_RESPONSE_ERR);
	    				release_namespace_token_t(ns_token);
					glob_cll7redir_int_err_1++;
					return FALSE;
				}
			} else {
				// Process request locally

				struct in_addr saddr;
				TSTR(target_host, target_hostlen, thost);
				DBG_LOG(MSG, MOD_L7REDIR,
					"INTERCEPT_REDIRECT: "
					"con=%p fd=%d req=%p "
					"ns=[%s] "
					"process locally => %s:%hu",
					con, con->fd, &con->http.hdr,
					ns_name ? ns_name : "",
					thost, target_port);

				inet_pton(AF_INET, (const char *)thost,
					  &saddr);
				setup_dest_ip_port_hdrs(&con->http.hdr, 
							saddr.s_addr,
							htons(target_port),
							1 /* overwrite */);
				rv = adjust_request_for_l7redir(&con->http.hdr, 
							      	predir_attr);
				if (rv) {
					DBG_LOG(MSG, MOD_HTTP, 
						"adjust_request_for_redir() "
						"failed, con=%p fd=%d req=%p "
						"rv=%d", 
						con, con->fd, &con->http.hdr, 
						rv);
					CLEAR_HTTP_FLAG(&con->http, 
						HRF_CONNECTION_KEEP_ALIVE);
					http_build_res_501(&con->http, 
						       NKN_CL7_LOCAL_PROC_ERR);
					http_close_conn(con, 
							NKN_CL7_LOCAL_PROC_ERR);
	    				release_namespace_token_t(ns_token);
					glob_cll7redir_int_err_2++;
					return FALSE;
				}
				// Access log status header
				update_known_header(&con->http.hdr, 
					MIME_HDR_X_NKN_CL7_STATUS,
					"Use=local", 9, 0);

	    			release_namespace_token_t(ns_token);
				con->http.nsconf = NULL;
				glob_cll7redir_intercept_success_local++;
			}

	    	} else if (intercept_action == INTERCEPT_PROXY) {
			rv = adjust_request_for_l7redir(&con->http.hdr, 
							predir_attr);
			if (rv) {
				DBG_LOG(MSG, MOD_HTTP, 
					"adjust_request_for_redir() "
					"failed, con=%p fd=%d req=%p "
					"rv=%d", 
					con, con->fd, &con->http.hdr, 
					rv);
				CLEAR_HTTP_FLAG(&con->http, 
					HRF_CONNECTION_KEEP_ALIVE);
				http_build_res_501(&con->http, 
					       NKN_CL7_LOCAL_PROC_ERR);
				http_close_conn(con, 
						NKN_CL7_LOCAL_PROC_ERR);
				release_namespace_token_t(ns_token);
				release_namespace_token_t(node_ns_token);
				glob_cll7proxy_int_err_1++;
				return FALSE;
			}
			if (!is_local_ip(target_host, target_hostlen)) {
				rv = CL7_proxy_request(con, ns_token, 
							node_ns_token);
				if (rv) {
					glob_cll7proxy_intercept_success++;
				}
				return rv;
			} else {
				// Process request locally
				struct in_addr saddr;
				TSTR(target_host, target_hostlen, thost);
				DBG_LOG(MSG, MOD_L7REDIR,
					"INTERCEPT_PROXY: "
					"con=%p fd=%d req=%p "
					"ns=[%s] "
					"process locally => %s:%hu",
					con, con->fd, &con->http.hdr,
					ns_name ? ns_name : "",
					thost, target_port);

				inet_pton(AF_INET, (const char *)thost,
					  &saddr);
				setup_dest_ip_port_hdrs(&con->http.hdr, 
							saddr.s_addr,
							htons(target_port),
							1 /* overwrite */);
				// Access log status header
				update_known_header(&con->http.hdr, 
					MIME_HDR_X_NKN_CL7_STATUS,
					"Use=local", 9, 0);

	    			release_namespace_token_t(ns_token);
				con->http.nsconf = NULL;
				release_namespace_token_t(node_ns_token);
				glob_cll7proxy_intercept_success_local++;
			}

	    	} else if (intercept_action == INTERCEPT_RETRY) {
			glob_cll7redir_intercept_retries++;
			TSTR(target_host, target_hostlen, thost);
			DBG_LOG(MSG, MOD_L7REDIR,
				"INTERCEPT_RETRY: "
				"con=%p fd=%d req=%p "
				"ns=[%s] ",
				con, con->fd, &con->http.hdr, 
				ns_name ? ns_name : "");

			/* Delay and retry until init is complete */
			net_fd_handle_t fhd;
			int retries;

			con->timer_event = TMRQ_EVENT_NAMESPACE_WAIT_2;
			fhd = NM_fd_to_fhandle(con->fd);

			retries = 3;
			while ((ret = net_set_timer(fhd, NS_INIT_WAIT_INTERVAL, 
					    	TT_ONESHOT)) < 0) {
				if ((retries--) == 0) {
					break;
				}
				sleep(0); // yield to HRT thread
			}
			if (!ret) {
				con->http.ns_token = ns_token;
				con->http.nsconf = NULL;
				glob_http_delayed_req2_scheds++;
				return TRUE;
			} else {
				glob_http_delayed_req2_fails++;
				con->timer_event = TMRQ_EVENT_UNDEFINED;

				CLEAR_HTTP_FLAG(&con->http, 
					HRF_CONNECTION_KEEP_ALIVE);
				http_build_res_500(&con->http, 
						   NKN_HTTP_REQUEST);
				http_close_conn(con, NKN_HTTP_REQUEST);
				release_namespace_token_t(ns_token);
				return FALSE;
			}

	    	} else if (intercept_action == INTERCEPT_ERR) {
			DBG_LOG(MSG, MOD_HTTP, 
				"L7 request intercept failed, "
				"con=%p fd=%d req=%p", 
				con, con->fd, &con->http.hdr);
			CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
			http_build_res_501(&con->http, NKN_CL7_NO_NODES);
			http_close_conn(con, NKN_CL7_NO_NODES);
			release_namespace_token_t(ns_token);
			glob_cll7redir_intercept_errs++;
			return FALSE;
		}
	} else {
		if (intercept_action != INTERCEPT_NONE) {
			release_namespace_token_t(node_ns_token);
			release_namespace_token_t(ns_token);
		}
	}


	/*
	 * Checking the HTTP request integration
	 * This check is to make sure that raw HTTP request header is legal.
	 * this check should happen before any other module gets the data.
	 */
        if (valid_http_request != TRUE) {
		/* Before tunnelling flag, respcode and subcode's copy 
		will be taken . if the request is not served via tunnel
		flag, respcode and subcode will be copied back and 
		error will be sent to client */
		int flag = con->http.flag;
		int respcode = con->http.respcode;
		int subcode = con->http.subcode;
		ret = NKN_HTTP_REQUEST;
		con->http.flag = 0;
		con->http.respcode = 0;
		con->http.subcode = 0;
		if ((intercept_action == INTERCEPT_NONE) && 
		    is_this_bad_req_tunnel_able(con) == TRUE) {

            if (is_bind_ip_nomatch(con, con->http.nsconf)) {
                CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
                http_build_res_403(&con->http, NKN_HTTP_NS_BIND_IP_NOMATCH);
                http_close_conn(con, NKN_HTTP_NS_BIND_IP_NOMATCH);
                return FALSE;
            }
		    	// refcnt=1, set via is_this_bad_req_tunnel_able()
			DBG_LOG(MSG, MOD_HTTP, "forwarding to tunnel path");
			ret = fp_make_tunnel(con, -1, 0, 0);
			/* BZ 5263:
			 * Rewrite this piece of logic to propogate real reason
			 * of failure. In this case, HTTP/1.1 request with 
			 * no Host header was seen. This subcode will have been
			 * set in the con_t struct, but fp_make_tunnel()
			 * also fails, because we try and get the dest ip from the
			 * token - MIME_HDR_X_NKN_REQ_REAL_DEST_IP - which doesnt
			 * happen at all (Scroll down below). Since the IP is not
			 * available, we cant tunnel.  
			 * A special case is made in is_this_bad_req_tunnel_able()
			 * to read up the destination IP so that the tunnel can be 
			 * made.
			 */
			if (ret == 0) {
				glob_tunnel_req_http_check_req_err++;
				return TRUE;
			}
			/* reset the subcode, so that fp_make_tunnel error
			 * code is given back.
			 */
			if (ret)
				subcode = ret;
			DBG_LOG(MSG, MOD_HTTP, "failed to tunnel - errcode (%d)", ret);
			if (CHECK_HTTP_FLAG(&con->http, HRF_RES_HEADER_SENT)) {
				// Error response already generated
				respcode = 0;
				flag |= HRF_RES_HEADER_SENT;
			}
		}
		con->http.flag = flag;
		con->http.respcode = respcode;
		con->http.subcode = subcode;
		CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
		switch(respcode) {
		case 400:
			http_build_res_400(&con->http, subcode);
			break;
		case 416:
			http_build_res_416(&con->http, subcode);
			break;
		case 505:
			http_build_res_505(&con->http, subcode);
			break;
		default:
			break;
		}
		// If there is no other HTTP code, 
		// We will return 500 Internal server error
                http_close_conn(con, ret);
                return FALSE;
        }

	/*
	 * nkn_http_pers_conn_nums:
	 * case 0: follow browser requirement
	 * case 1: close the socket after each transaction.
	 * case n: close the socket after "n" transactions.
	 */
	con->num_of_trans ++;
	if(nkn_http_pers_conn_nums && (nkn_http_pers_conn_nums == con->num_of_trans) ) {
		CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
	}

	/*
	 *  Add request destination IP/Port header (network byte order)
	 */
	setup_dest_ip_port_hdrs(&con->http.hdr, IPv4(con->dest_ipv4v6), 
				con->dest_port, 0 /* !overwrite */);

	/* Set the Module type as NORMALIZER, so that the con structure gets
	 * passed to AM if the uri is normalized (Hex encoded chars are present)
	 * This will be required when MM fetches the object from the Origin
	 */
	ret =  is_known_header_present(&con->http.hdr, MIME_HDR_X_NKN_DECODED_URI);
	if(ret) {
		con->module_type = NORMALIZER;
		glob_normalize_decoded_uri++;
	}
        /*
         * fill up the ns_token
         */
	con->http.ns_token = http_request_to_namespace(&con->http.hdr, &nsconf);
	if (!VALID_NS_TOKEN(con->http.ns_token)) {
		const char *host = 0;
		int hostlen = 0;
		const char *uri = 0;
		int urilen = 0;

		get_known_header(&con->http.hdr, MIME_HDR_HOST, &host, &hostlen,
			&attrs, &hdrcnt);
		TSTR(host, hostlen, hoststr);
		get_known_header(&con->http.hdr, MIME_HDR_X_NKN_URI, &uri, &urilen,
			&attrs, &hdrcnt);
		TSTR(uri, urilen, uristr);
		DBG_LOG(MSG, MOD_HTTP, 
			"No namespace for hostname=%s uri=%s", hoststr, uristr);
		if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
			DBG_TRACE("URL %s%s did not match any namespace", hoststr, uristr);
		}

		CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
		http_build_res_403(&con->http, NKN_HTTP_NAMESPACE); 
                http_close_conn(con, NKN_HTTP_NAMESPACE); // return 500 internal server error
		return FALSE;
	}

	/*
	 * Apply URL filter if applicable
	 */
	if (nsconf->uf_config->uf_trie ||
		nsconf->uf_config->uf_max_uri_size) {
		rv = apply_url_filter(nsconf, con);
		if (!rv) { // Deny request
		    AO_fetch_and_add1(&(nsconf->stat->urif_stats.rej_cnt));
		    return FALSE;
		} else {
		    AO_fetch_and_add1(&(nsconf->stat->urif_stats.acp_cnt));
		}
	}

        /*
         * handling special cases for forward proxy and t-proxy.
         */
	if (strcmp(nsconf->namespace, "mfc_probe") == 0) {
		SET_HTTP_FLAG(&con->http, HRF_MFC_PROBE_REQ);
              	AO_fetch_and_sub1(&glob_tot_http_sockets);
	        AO_fetch_and_sub1(&glob_http_tot_transactions);
	}

	if(CHECK_CON_FLAG(con, CONF_HTTPS_HDR_RECVD)) {
		if( !(nsconf->http_config->client_delivery_type & DP_CLIENT_RES_SECURE)) {
			CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
			http_build_res_403(&con->http, NKN_HTTPS_NAMESPACE);
                        http_close_conn(con, NKN_HTTPS_NAMESPACE);
			return FALSE;
			}
	} else {
		if(!(nsconf->http_config->client_delivery_type & DP_CLIENT_RES_NON_SECURE)) {
			CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
			http_build_res_403(&con->http, NKN_HTTP_NAMESPACE);
                        http_close_conn(con, NKN_HTTP_NAMESPACE);
			return FALSE;
		}
	}
	if (!(nsconf->http_config->response_protocol & DP_HTTP)) {
		DBG_LOG(MSG, MOD_HTTP, 
			"Bad protocol (fd = %d) - http:// - for a namespace that doesnt allow http.",
			con->fd);
		/* This namespace cannot allow a HTTP request */
		CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
		http_build_res_403(&con->http, NKN_HTTP_NAMESPACE); 
		http_close_conn(con, NKN_HTTP_NAMESPACE);
		return FALSE;
	}

    if (is_bind_ip_nomatch(con, nsconf)) {
        CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
        http_build_res_403(&con->http, NKN_HTTP_NS_BIND_IP_NOMATCH);
        http_close_conn(con, NKN_HTTP_NS_BIND_IP_NOMATCH);
        return FALSE;
    }


	DBG_LOG(MSG, MOD_HTTP, 
			"Associating request with namespace gen=%u token=%u", 
			con->http.ns_token.u.token_data.gen,
			con->http.ns_token.u.token_data.val);

	con->http.nsconf = nsconf;

       /* Set MD5 context for request tunnel case. */
        if (con->http.nsconf
           && con->http.nsconf->acclog_config->al_resp_hdr_md5_chksum_configured
           && con->http.nsconf->md5_checksum_len
           && !con->md5) {
            con->md5 = (http_resp_md5_checksum_t *)nkn_malloc_type(sizeof(http_resp_md5_checksum_t), 
                                                                   mod_om_md5_context);
            if (con->md5) {
                MD5_Init(&con->md5->ctxt) ;
                con->md5->temp_seq = 0;
                con->md5->in_strlen = 0;
                DBG_LOG(MSG, MOD_HTTP, "MD5_CHECKSUM init. con:%p, md5_ctxt:%p", con, con->md5);
            }
        }

	/* update the bytes received count for this namespace */
	NS_UPDATE_STATS((con->http.nsconf), PROTO_HTTP, client, in_bytes, (con->http.cb_offsetlen));
	/* Update the request count for this namespace */
	NS_INCR_STATS((con->http.nsconf), PROTO_HTTP, client, requests);

	/* Requirement 2.1 - 34.
	 * Check for maximum session count reached in namespace.
	 */
	if (nkn_resource_pool_enable &&
	    !(nvsd_rp_alloc_resource(RESOURCE_POOL_TYPE_CLIENT_SESSION,
			nsconf->rp_index, 1))) {
		/* make sure we dont hold this connection, even if it
		 * was marked for a keep-alive.
		 */
		CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
		/* Build the response message */
		http_build_res_503(&con->http, NKN_HTTP_NS_MAX_CONNS);

		http_close_conn(con, NKN_HTTP_NS_MAX_CONNS);
		return FALSE;
	} else {
		SET_HTTP_FLAG(&con->http, HRF_RP_SESSION_ALLOCATED);
		AO_fetch_and_add1((volatile AO_t *) &(nsconf->stat->g_http_sessions));
		NS_INCR_STATS(nsconf, PROTO_HTTP, client, active_conns);
		NS_INCR_STATS(nsconf, PROTO_HTTP, client, conns);
	}


        switch (nsconf->http_config->origin.select_method) {
	case OSS_HTTP_ABS_URL:
                if( forwardproxy_enable && 
		    (is_cacheable_request(con, nsconf->http_config->policies.convert_head)==FALSE) ) {
			con->http.tunnel_reason_code = NKN_TR_REQ_ABS_URL_NOT_CACHEABLE;
                        ret = fp_make_tunnel(con, -1, 0, 0);
                        if(ret != 0) {
				CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
                		http_close_conn(con, ret);
				return FALSE;
			}
                        else {
                        	glob_tunnel_req_http_abs_url++;
                        }
                        return TRUE;
                }
		/* for other cases */
		break;

	case OSS_HTTP_DEST_IP:
                if (orig_header_ret == HTTP_ORIG_HEADER_NOT_PRESENT) {
                        /* normal flow if include-orig head is not found */
                        if (http_add_real_dest_ip_port_header(con) == FALSE) {
                                return FALSE;
                        }   
                }     

		break;

	case OSS_HTTP_NODE_MAP:
                if (nsconf->http_config->origin.u.http.include_orig_ip_port &&
                    http_add_real_ip_port_header(con) == FALSE) {
                        return FALSE;
                }
		break;

	case OSS_NFS_CONFIG:
	case OSS_NFS_SERVER_MAP:
		if(!CHECK_HTTP_FLAG(&con->http, HRF_METHOD_HEAD) &&
			!CHECK_HTTP_FLAG(&con->http, HRF_METHOD_GET)) {
		    CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
		    http_build_res_501(&con->http, NKN_NFS_METHOD_NOT_IMPLEMENTED);
	            http_close_conn(con, NKN_NFS_METHOD_NOT_IMPLEMENTED);
		    return FALSE;
		}
		break;

	case OSS_HTTP_FOLLOW_HEADER:

		/* BZ 3899: check if convert HEAD to GET flag is not set.
		 * If so, then tunnel the request
		 */
		/* BZ 6840: For this proxy mode, original code was to
		 * tunnel HEAD requests to origin, even if we had the
		 * object in cache. Removed/deleted this piece of logic
		 * since it is inconsistent with other types of proxy
		 * modes that we support.
		 * All HEAD requests MUST atempt to respond from cache
		 * if the object is in cache. If the object isnt in cache
		 * then we allow OM to treat it as a cacheable miss request
		 * and tunnel the response to the client.
		 */
                if (orig_header_ret == HTTP_ORIG_HEADER_NOT_PRESENT) {
			char ip[INET6_ADDRSTRLEN];
                        /* normal flow if include-orig head is not found */
			if (unlikely(con->ip_family == AF_INET6)) {
				if (!is_known_header_present(&con->http.hdr, MIME_HDR_HOST) &&
				    (memcmp(IPv6(con->http.remote_ipv4v6),
					    IPv6(con->dest_ipv4v6), sizeof(struct in6_addr)) == 0)) {
					CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
					glob_tp_req_to_ifip ++;
					http_close_conn(con, ret);
					return FALSE;
				}
				/* good valid destination IP address.  */
				if (inet_ntop(AF_INET6, &IPv4(con->http.remote_ipv4v6),
							&ip[0], INET6_ADDRSTRLEN) == NULL) {
					CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
					glob_tp_req_to_ifip ++;
					http_close_conn(con, ret);
					return FALSE;
				}
				ret = snprintf(req_dest_ip_str, sizeof(req_dest_ip_str), "%s", ip);
				add_known_header(&con->http.hdr, MIME_HDR_X_NKN_REQ_REAL_DEST_IP,
						(const char *)req_dest_ip_str, ret);
				ret = snprintf(req_dest_port_str, sizeof(req_dest_port_str), "%hu",
						ntohs(con->http.remote_sock_port));
				add_known_header(&con->http.hdr, MIME_HDR_X_NKN_REQ_REAL_DEST_PORT,
						(const char *)req_dest_port_str, ret);
			} else {
				if (!is_known_header_present(&con->http.hdr, MIME_HDR_HOST) &&
					    IPv4(con->http.remote_ipv4v6) == IPv4(con->dest_ipv4v6)) {
					CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
					glob_tp_req_to_ifip ++;
					http_close_conn(con, ret);
					return FALSE;
				}
				/* good valid destination IP address.  */
				if (inet_ntop(AF_INET, &IPv4(con->http.remote_ipv4v6),
							&ip[0], INET6_ADDRSTRLEN) == NULL) {
					CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
					glob_tp_req_to_ifip ++;
					http_close_conn(con, ret);
					return FALSE;
				}
				ret = snprintf(req_dest_ip_str, sizeof(req_dest_ip_str), "%s", ip);
				add_known_header(&con->http.hdr, MIME_HDR_X_NKN_REQ_REAL_DEST_IP,
						(const char *)req_dest_ip_str, ret);
				ret = snprintf(req_dest_port_str, sizeof(req_dest_port_str), "%hu",
						ntohs(con->http.remote_sock_port));
				add_known_header(&con->http.hdr, MIME_HDR_X_NKN_REQ_REAL_DEST_PORT,
						(const char *)req_dest_port_str, ret);
			}
                }

		if (!is_known_header_present(&con->http.hdr, MIME_HDR_HOST)) {
			con->http.tunnel_reason_code = NKN_TR_REQ_NO_HOST;
	    		ret = fp_make_tunnel(con, -1, 0, 0);
			if (ret != 0) {
				CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
				http_close_conn(con, ret);
				return FALSE;
	    		}
            		return TRUE;
		}
		SET_HTTP_FLAG(&con->http, HRF_TPROXY_MODE);
		break;

	default:
		break;
	}

#if 0
	if (action & PE_ACTION_CALLOUT) {
		policy_srvr_list_t *p_srlist;

		p_srlist = con->http.nsconf->http_config->policy_engine_config.policy_server_list;
		if (con->pe_co && con->pe_co->co_list &&
			(strcasecmp(p_srlist->policy_srvr->name, con->pe_co->co_list->server) == 0)) {
			if (p_srlist->policy_srvr->proto == GEO_LOCALDB) {
				geo_ip_req_t *req;
				geo_ip_t *resp;

				req = nkn_malloc_type(sizeof(geo_ip_req_t), mod_pe_geo_ip_req_t);
				resp = nkn_malloc_type(sizeof(geo_ip_t), mod_pe_geo_ip_t);
				req->magic = GEODB_MAGIC;
				req->ip = con->src_ip;
				memset(resp, 0, sizeof(geo_ip_t));
				resp->magic = GEODB_MAGIC;
				con->pe_co->co_list->co_resp = resp;
				pe_helper_get_geostat(req, con);
				return TRUE;
			}
		}
	}
#endif

	if (nsconf->http_config->geo_ip_cfg.server == GEOIP_MAXMIND) {
		geo_ip_req_t *req;
		geo_info_t* p_geoinfo = NULL;

		if (con->src_ipv4v6.family == AF_INET) {
			p_geoinfo = get_geo_info_t(IPv4(con->src_ipv4v6), &con->ginf);
			if (p_geoinfo == NULL) {
				req = nkn_malloc_type(sizeof(geo_ip_req_t), mod_pe_geo_ip_req_t);
				if (req != NULL) {
					req->magic = GEODB_MAGIC;
					req->ip = ntohl(IPv4(con->src_ipv4v6)); // Convert to host byte order
					req->lookup_timeout = nsconf->http_config->geo_ip_cfg.lookup_timeout;

					AO_fetch_and_add1(&glob_dbg_http_handle_request1);
					if (pe_helper_get_geostat(req, con) > 0) {
						ret = suspend_NM_socket(con->fd);
						return TRUE;
					}
				}
				con->ginf.status_code = 500;
			}
		}
		else {
			con->ginf.status_code = 500;
		}
	}

	return http_handle_request_url_cat_lookup(con);
}

int http_handle_request_url_cat_lookup(con_t *con) 
{
	// For POC it is only enabled form config file. 
	// Todo: Read from namespace configuration.
	//
	DBG_LOG(MSG, MOD_HTTP, "Entering %s\n", __FUNCTION__) ;
	if (pe_url_category_lookup) {
		ucflt_req_t *req;
		ucflt_info_t* p_ucflt_info = NULL;
		const char *host = 0;
		int hostlen = 0;
		const char *uri = 0;
		int urilen = 0;
		u_int32_t attrs;
		int hdrcnt;
		char url[512];
		int ret;

		get_known_header(&con->http.hdr, MIME_HDR_HOST, &host, &hostlen,
			&attrs, &hdrcnt);
		get_known_header(&con->http.hdr, MIME_HDR_X_NKN_URI, &uri, &urilen,
			&attrs, &hdrcnt);
		snprintf(url, 512, "http://%s%s", host, uri);

		p_ucflt_info = get_ucflt_info_t(url, &con->uc_inf);
		if (p_ucflt_info == NULL) {
			req = nkn_malloc_type(sizeof(ucflt_req_t), mod_pe_ucflt_req_t);
			if (req != NULL) {
				strncpy(req->url, url, 512);
				req->lookup_timeout = 60;

				//AO_fetch_and_add1(&glob_dbg_http_handle_request1);
				if (pe_helper_get_url_cat(req, con) > 0) {
					ret = suspend_NM_socket(con->fd);
					return TRUE;
				}
			}
			con->uc_inf.status_code = 500;
		}
	}
	
	return http_handle_request_post_pe(con);
}

int http_handle_request_post_pe(con_t *con)
{
	int ret;
	const namespace_config_t *nsconf;
	origin_server_select_method_t os_select_method;
	u_int32_t attrs;
	int hdrcnt;
	int rv = 0;
	int cache_path = 0;
	AO_t init_in_progress;

	nsconf = con->http.nsconf;
	os_select_method = nsconf->http_config->origin.select_method;
	init_in_progress = AO_load((volatile AO_t*)&nsconf->init_in_progress);
	AO_fetch_and_add1(&glob_dbg_http_handle_request2);

	/*
	 * Check if a trace is requested and is enabled
	 */
	if (nkn_http_trace_enable &&
	    is_unknown_header_present(&con->http.hdr, MIME_HDR_X_NKN_TRACE_STR, MIME_HDR_X_NKN_TRACE_LEN)) {
		SET_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST);
	}

	if (nsconf->http_config->geo_ip_cfg.server == GEOIP_MAXMIND && con->ginf.status_code != 0) {
		if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
			DBG_TRACE("GeoIP Status code : %d\nCountry : %s, %s, %s\nCity: %s, %s, %s\nISP: %s\n",
				  con->ginf.status_code, con->ginf.country, 
				  con->ginf.country_code, con->ginf.continent_code,
				  con->ginf.city, con->ginf.state, 
				  con->ginf.postal_code, con->ginf.isp);
		}
		
		/* 
		 * If GeoIP lookup fails and failover bypass is not enabled reject the request
		 */
		if (con->ginf.status_code == 500 && nsconf->http_config->geo_ip_cfg.failover_bypass == FALSE) {
			CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
			http_build_res_403(&con->http, NKN_PE_REJECT_ON_GEOIP_FAIL);
			http_close_conn(con, NKN_PE_REJECT_ON_GEOIP_FAIL);
			return FALSE;
		}
	}

	if (pe_url_category_lookup && con->uc_inf.status_code != 0) {
		if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
			DBG_TRACE("URL Category lookup Status code : %d\nNum Category : %d\nCategory : %s\n",
				  con->uc_inf.status_code, 
				  con->uc_inf.category_count,
				  con->uc_inf.category);
		}
		
		/* 
		 * If URL category lookup fails and failover bypass is not enabled reject the request
		 */
		if (con->uc_inf.status_code == 500 && !pe_ucflt_failover_bypass_enable) {
			CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
			http_build_res_403(&con->http, NKN_PE_REJECT_ON_UCFLT_FAIL);
			http_close_conn(con, NKN_PE_REJECT_ON_UCFLT_FAIL);
			return FALSE;
		}
	}

	
	/*
	 * Apply Policy Engine
	 */
	pe_ilist_t * p_list = NULL;
	pe_rules_t * p_perule = NULL;

	if (nsconf->http_config->policy_engine_config.policy_file) {
		p_perule = pe_create_rule(&nsconf->http_config->policy_engine_config, &p_list);
		if (p_perule) {
			uint64_t action;
			//con->p_perule = p_perule;
			action = pe_eval_http_recv_request(p_perule, con);
			pe_free_rule(p_perule, p_list);
			if (action & PE_ACTION_REJECT_REQUEST) {
				CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
				http_build_res_403(&con->http, 0);
				http_close_conn(con, 0);
				return FALSE;
			}
			if (action & PE_ACTION_REDIRECT) {
				mime_header_t response_hdr;
				const char *data1;
				int datalen1;
				int hdrcnt1;
				u_int32_t attr1;

				/* Create a new header and add loction header only.
				* This avoids function http_build_res_302 from returning
				* all headers in the request.
				*/
				init_http_header(&response_hdr, 0, 0);
				rv = get_known_header(&con->http.hdr, MIME_HDR_LOCATION,
						    &data1, &datalen1, &attr1, &hdrcnt1);
				if (!rv) {
					rv = add_known_header(&response_hdr,
						    MIME_HDR_LOCATION, data1, datalen1);
				}

				if (rv) {
					DBG_LOG(MSG, MOD_HTTP,
					"add_known_header(MIME_HDR_CONTENT_LOCATION) "
					"failed, rv=%d", rv);
					CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
					http_close_conn(con, 0);
					shutdown_http_header(&response_hdr);
					return FALSE; 
				}

				http_build_res_302(&con->http, 0, &response_hdr);
				http_close_conn(con, 0);
				shutdown_http_header(&response_hdr);
				return TRUE;
			}
#if 0
			if (action & PE_ACTION_CALLOUT) {
				policy_srvr_list_t *p_srlist;

				p_srlist = con->http.nsconf->http_config->policy_engine_config.policy_server_list;
				if (con->pe_co && con->pe_co->co_list &&
					(strcasecmp(p_srlist->policy_srvr->name, con->pe_co->co_list->server) == 0)) {
					if (p_srlist->policy_srvr->proto == GEO_LOCALDB) {
						geo_ip_req_t *req;
						geo_ip_t *resp;

						req = nkn_malloc_type(sizeof(geo_ip_req_t), mod_pe_geo_ip_req_t);
						resp = nkn_malloc_type(sizeof(geo_ip_t), mod_pe_geo_ip_t);
						req->magic = GEODB_MAGIC;
						req->ip = con->src_ip;
						memset(resp, 0, sizeof(geo_ip_t));
						resp->magic = GEODB_MAGIC;
						con->pe_co->co_list->co_resp = resp;
						pe_helper_get_geostat(req, con);
						return TRUE;
					}
				}
			}
#endif

			if (action & PE_ACTION_REQ_NO_CACHE_OBJECT) {
				con->http.tunnel_reason_code = NKN_TR_REQ_POLICY_NO_CACHE;
				ret = fp_make_tunnel(con, -1, 0, 0);
				if (ret != 0) {
					CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
					http_close_conn(con, ret);
					return FALSE;
				}
				return TRUE;
			}
			if (action & PE_ACTION_SET_ORIGIN_SERVER) {
				/* save full headers for use by AM/BM
				*/
				con->module_type = NORMALIZER;
				glob_normalize_pe_set_origin_server++;
			}
#if 0
			case PE_ACTION_URL_REWRITE:
				// Does nothing here.
				// It will be handled later on.
				break;
			case PE_ACTION_SET_IP_TOS:
				// Already handled in action function 
				break;
			case PE_ACTION_SET_ORIGIN_SERVER:
				// Already handled in action function 
				break;
			default:
				break;
#endif
		}

	}
#if 0
	/*
	 * If external policy callout was done, invoke TCL interpreter to
	 * process the response.
	 */
	if (con->pe_action & PE_ACTION_CALLOUT) { 
		policy_srvr_list_t *p_srlist;
		
		p_srlist = con->http.nsconf->http_config->policy_engine_config.policy_server_list;
		if (con->pe_co && con->pe_co->co_list &&
				(strcasecmp(p_srlist->policy_srvr->name, con->pe_co->co_list->server) == 0) &&
				(p_srlist->policy_srvr->proto == GEO_LOCALDB)) {
			pe_ilist_t * p_list = NULL;
			pe_rules_t * p_perule = NULL;

			if (nsconf->http_config->policy_engine_config.policy_file) {
				p_perule = pe_create_rule(&nsconf->http_config->policy_engine_config, &p_list);
			}
			if (p_perule) {
				uint64_t action;
				
				//con->p_perule = p_perule;
				action = pe_eval_http_recv_request_callout(p_perule, con);
				pe_free_rule(p_perule, p_list);
				if (action & PE_ACTION_REJECT_REQUEST) {
					CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
					http_build_res_403(&con->http, NKN_HTTP_PE_REJECT);
					http_close_conn(con, NKN_HTTP_PE_REJECT);
					return FALSE;
				}
				if (action & PE_ACTION_REDIRECT) {
					mime_header_t response_hdr;
					const char *data1;
					int datalen1;
					int hdrcnt1;
					u_int32_t attr1;

					/* Create a new header and add loction header only.
					 * This avoids function http_build_res_302 from returning
					 * all headers in the request.
					 */
					init_http_header(&response_hdr, 0, 0);
					rv = get_known_header(&con->http.hdr, MIME_HDR_LOCATION, &data1, &datalen1, &attr1, &hdrcnt1);
					if (!rv) {
						rv = add_known_header(&response_hdr, MIME_HDR_LOCATION, data1, datalen1);
					}

					if (rv) {
						DBG_LOG(MSG, MOD_HTTP, 
							"add_known_header(MIME_HDR_CONTENT_LOCATION) "
							"failed, rv=%d", rv);
						CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
						http_close_conn(con, 0);
						shutdown_http_header(&response_hdr);
						return FALSE; 
					}

					http_build_res_302(&con->http, 0, &response_hdr);
					http_close_conn(con, 0);
					shutdown_http_header(&response_hdr);
					return TRUE;
				}
			}
		}
	}

	if (con->pe_action & PE_ACTION_REQ_NO_CACHE_OBJECT) {
		con->http.tunnel_reason_code = NKN_TR_REQ_POLICY_NO_CACHE;
		ret = fp_make_tunnel(con, -1, 0, 0);
		if (ret != 0) {
			CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
			http_close_conn(con, ret);
			return FALSE;
		}
		return TRUE;
	}
	if (con->pe_action & PE_ACTION_SET_ORIGIN_SERVER) {
		/* save full headers for use by AM/BM
		 */
		con->module_type = NORMALIZER;
	}
#endif

	/*
	 * We do have some limitations in today's reverse/virtual proxy cases.
	 */


	if (http_check_mfd_limitation(&con->http, con->pe_action) != TRUE) {
		if (con->http.tunnel_reason_code == NKN_TR_REQ_BIG_URI) {
			if (CHECK_CON_FLAG(con, CONF_REFETCH_REQ) ||
			    nsconf->http_config->request_policies.dynamic_uri.regctx ||
				CHECK_HTTP_FLAG(&con->http, HRF_TIER2_MODE)) {
				con->http.tunnel_reason_code = 0;
				cache_path = 1;
			}
		}
		if (cache_path == 0) {
			ret = fp_make_tunnel(con, -1, 0, 0);
			if (ret != 0) {
				CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
				http_close_conn(con, ret);
				return FALSE;
			}
			return TRUE;
		}
	}
	// finally check whether cache path is up
	if (glob_cachemgr_init_done == 0) {
		con->http.tunnel_reason_code = NKN_TR_REQ_CACHEPATH_NOTREADY;
		glob_tunnel_req_cachepath_notready++;
		ret = fp_make_tunnel(con, -1, 0, 0);
		if (ret != 0) {
			CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
			http_close_conn(con, ret);
			return FALSE;
		}
		return TRUE;
	}

	/*
	 * Handle If-Modified-Since and If-None-Match directives
	 */
	handle_if_directives(con);

	/*
	 * Handle via header
	 */
	handle_via(con, nsconf);

	/*
	 *  Determine the namespace associated with the request
	 */
	if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
		const char *host = 0;
		int hostlen = 0;
		const char *uri = 0;
		int urilen = 0;

		get_known_header(&con->http.hdr, MIME_HDR_HOST, &host, &hostlen,
			&attrs, &hdrcnt);
		TSTR(host, hostlen, hoststr);
		get_known_header(&con->http.hdr, MIME_HDR_X_NKN_URI, &uri, &urilen,
			&attrs, &hdrcnt);
		TSTR(uri, urilen, uristr);
		nsconf = con->http.nsconf;
		DBG_TRACE("URL %s%s matched namespace %s", hoststr, uristr, 
			  nsconf ? nsconf->namespace : "");
	}

	// Set CONF_REFETCH_REQ flag to indicate response cache index based
	// request. CLI backend integration TBD here.
	if ((nsconf->http_config->cache_index.include_header || 
		nsconf->http_config->cache_index.include_object) &&
		(is_known_header_present(&con->http.hdr, MIME_HDR_X_NKN_CLUSTER_TPROXY)==0)) {
		if (nsconf->http_config->cache_index.checksum_from_end) {
			SET_CON_FLAG(con, CONF_CI_FETCH_END_OFF);
		}
		SET_CON_FLAG(con, CONF_REFETCH_REQ);
		if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
			DBG_TRACE("Response cache-index set - first probe request");
		}
	}

	// Skip SSP and Dynamic URI translations if response based cache index is set
	if (!CHECK_CON_FLAG(con, CONF_REFETCH_REQ)) {
		int ssp_ret_val;
		//BZ: 8355 Dyn url cfg is not effective if SSP is associated to namespace
		if (dynamic_uri_enable &&
				(is_known_header_present(&con->http.hdr, MIME_HDR_X_NKN_CLUSTER_TPROXY)==0)) {
			ret = nkn_remap_uri(con, NULL);
			nsconf = con->http.nsconf;
			if ((ret && nsconf->http_config->request_policies.dynamic_uri.tunnel_no_match) ||
			    (ret == URI_REMAP_TOKENIZE_ERR)  || (ret == URI_REMAP_HEX_DECODE_ERR)) {
				con->http.tunnel_reason_code = NKN_TR_REQ_DYNAMIC_URI_REGEX_FAIL;
				ret = fp_make_tunnel(con, -1, 0, 0);
				if (ret != 0) {
					CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
					http_close_conn(con, ret);
					return FALSE;
				}
				return TRUE;
			}
		}

		if (nsconf->http_config->request_policies.seek_uri.seek_uri_enable) {
			ret = nkn_tokenize_gen_seek_uri(con);
		}

		// Apply SSP 
		ret = http_apply_ssp(con, &ssp_ret_val);
		if ((ssp_ret_val < 0) || (3 == ssp_ret_val)) {
			return ret;
		}
	} else {
		DBG_LOG(MSG, MOD_HTTP, "Skip dynamic URI + SSP for response based "
				"cache-index request(probe)");		
	}

	// BZ 8355: Remove this #if 0 code after 2.0.8 release
#if 0
	/*
	if (!CHECK_HTTP_FLAG(&con->http, HRF_SSP_CONFIGURED)) {
		// Virtual Player has high precedence over dynamic uri
		if (dynamic_uri_enable) {
			ret = nkn_remap_uri(con);
			nsconf = con->http.nsconf;
			if (ret && nsconf->http_config->request_policies.dynamic_uri.tunnel_no_match) {
				con->http.tunnel_reason_code = NKN_TR_REQ_DYNAMIC_URI_REGEX_FAIL;
				ret = fp_make_tunnel(con, -1, 0, 0);
				if (ret != 0) {
					CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
					http_close_conn(con, ret);
					return FALSE;
				}
				return TRUE;
			}
		}
	}
	*/
#endif

	/*
	 * Get COD token. If return NKN_COD_NULL, we will tunnel the request.
	 */
	con->http.req_cod = http_get_req_cod(con);
	if (con->http.req_cod == NKN_COD_NULL) {
	    if ((os_select_method == OSS_NFS_CONFIG) ||
	    	(os_select_method == OSS_NFS_SERVER_MAP)) {
		// Note: Cannot tunnel NFS requests
		CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
		http_build_res_404(&con->http, NKN_HTTP_CLOSE_CONN_NFS_CONFIG);
		http_close_conn(con, NKN_HTTP_CLOSE_CONN_NFS_CONFIG);
	    } else {
		con->http.tunnel_reason_code = NKN_TR_REQ_GET_REQ_COD_ERR;
	    	ret = fp_make_tunnel(con, -1, 0, 0);
	    	if (ret != 0) {
		    CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
		    http_build_res_404(&con->http, ret);
		    http_close_conn(con, ret);
		    return FALSE;
	    	} else {
	    		glob_tunnel_req_http_get_req_cod_err++;
	    	}
	    }
            return TRUE;
	}

	/* If HDR_HOST is present and we have the host_header_inherit config
	 * this has to be passed to AM for internal fetch requests
	 */
	ret =  is_known_header_present(&con->http.hdr, MIME_HDR_HOST);
	if(ret) {
		if (nsconf->http_config->policies.is_host_header_inheritable) {
		    con->module_type = NORMALIZER;
		    glob_normalize_host_hdr_inherit++;
		}
	}

	// Handle Client Cache-Control: no-cache request
	handle_no_cache_request(con, nsconf, 0);

	if (nsconf->http_config->request_policies.num_accept_headers) {
		ret = validate_client_request(con);
		if (ret) {
			CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
			http_build_res_403(&con->http, ret);
			http_close_conn(con, ret);
			return FALSE;
		}
	}


	if (!init_in_progress) {
        	return nkn_post_sched_a_task(con);
	} else {
		/* Delay nkn_post_sched_a_task() call until init is complete */
		net_fd_handle_t fhd;
		int retries;

		con->timer_event = TMRQ_EVENT_NAMESPACE_WAIT;
		fhd = NM_fd_to_fhandle(con->fd);

		retries = 1;
		while ((ret = net_set_timer(fhd, NS_INIT_WAIT_INTERVAL, 
					    TT_ONESHOT)) < 0) {
			if ((retries--) == 0) {
				break;
			}
			sleep(0); // yield to HRT thread
		}
		if (!ret) {
			glob_http_delayed_req_scheds++;
		} else {
			glob_http_delayed_req_fails++;
			con->timer_event = TMRQ_EVENT_UNDEFINED;
        		return nkn_post_sched_a_task(con);
		}
	}

	return TRUE;
}

static void handle_if_directives(con_t *con)
{
#define DEFAULT_OUTBUF_BUFSIZE 4096
#define OUTBUF(_data, _datalen, _bytesused) { \
    if (((_datalen)+(_bytesused)) >= outbufsize) { \
	tmp_outbuf = outbuf; \
    	tmp_outbufsize = outbufsize; \
	if (outbufsize == DEFAULT_OUTBUF_BUFSIZE) { \
	    outbuf = 0; \
	} \
	outbufsize = MAX(outbufsize * 2, (_datalen)+(_bytesused)+1); \
	outbuf = nkn_realloc_type(outbuf, outbufsize, mod_http_hdrbuf); \
	if (outbuf) { \
	    if (tmp_outbufsize == DEFAULT_OUTBUF_BUFSIZE) { \
	    	memcpy(outbuf, tmp_outbuf, (_bytesused)); \
	    } \
	    memcpy((void *)&outbuf[(_bytesused)], (_data), (_datalen)); \
	    (_bytesused) += (_datalen); \
	} else { \
	    outbuf = tmp_outbuf; \
	    outbufsize = tmp_outbufsize; \
	} \
    } else { \
       	memcpy((void *)&outbuf[(_bytesused)], (_data), (_datalen)); \
    	(_bytesused) += (_datalen); \
    } \
}
	int rv;
	int bytesused = 0;
	int nth;
	const char *if_mod_since_val;
	int if_mod_since_vallen;
	const char *if_none_match_val;
	int if_none_match_vallen;
	u_int32_t attrs;
	int hdrcnt;
	u_int8_t user_attr;

	char default_buf[DEFAULT_OUTBUF_BUFSIZE];
	char *outbuf = default_buf;
	int outbufsize = DEFAULT_OUTBUF_BUFSIZE;
	int tmp_outbufsize;
	char *tmp_outbuf;
	int cl7_proxy = CHECK_HTTP_FLAG(&con->http, HRF_CL_L7_PROXY_REQUEST);

	/*
	 *  Note if "If-Modified-Since" request
	 */
	if_mod_since_val = 0;
	get_known_header(&con->http.hdr, MIME_HDR_IF_MODIFIED_SINCE, 
			 &if_mod_since_val, &if_mod_since_vallen, 
			 &attrs, &hdrcnt);
	if (if_mod_since_val) {
		user_attr = HTTP_PAIR_REQ;
		add_namevalue_header(&con->http.hdr, 
			http_known_headers[MIME_HDR_IF_MODIFIED_SINCE].name,
			http_known_headers[MIME_HDR_IF_MODIFIED_SINCE].namelen,
			if_mod_since_val, if_mod_since_vallen, user_attr);
                /*Do not delete this header for HEAD methods, as these will get
                tunnelled. And in the tunnel build requests these headers
                will be sent only if present in the request. For GET method
                MFC does the 200/304 handling so we can delete. Bug 6441*/
                if(!cl7_proxy && !CHECK_HTTP_FLAG(&con->http, HRF_METHOD_HEAD))
			delete_known_header(&con->http.hdr, MIME_HDR_IF_MODIFIED_SINCE);
	}

	/*
	 *  Note if "If-None-Match" request
	 */
	if_none_match_val = 0;
	get_known_header(&con->http.hdr, MIME_HDR_IF_NONE_MATCH, 
			 &if_none_match_val, &if_none_match_vallen, 
			 &attrs, &hdrcnt);
	if (if_none_match_val) {

		user_attr = HTTP_PAIR_REQ;
		if (hdrcnt == 1) {
			add_namevalue_header(&con->http.hdr, 
				http_known_headers[MIME_HDR_IF_NONE_MATCH].name,
				http_known_headers[MIME_HDR_IF_NONE_MATCH].namelen,
				if_none_match_val, if_none_match_vallen, user_attr);
		} else {
			OUTBUF(if_none_match_val, if_none_match_vallen, bytesused);
			for (nth = 1; nth < hdrcnt; nth++) {
				rv = get_nth_known_header(&con->http.hdr, 
						MIME_HDR_IF_NONE_MATCH,
						nth, &if_none_match_val,
						&if_none_match_vallen,
						&attrs);
				if (rv) {
					DBG_LOG(MSG, MOD_HTTP, 
						"get_nth_known_header(n=%d) failed, "
						"nth=%d rv=%d",
						MIME_HDR_IF_NONE_MATCH, nth, rv);
					continue;

				}
				OUTBUF(",", 1, bytesused);
				OUTBUF(if_none_match_val, if_none_match_vallen, bytesused);
			}
			add_namevalue_header(&con->http.hdr, 
				http_known_headers[MIME_HDR_IF_NONE_MATCH].name,
				http_known_headers[MIME_HDR_IF_NONE_MATCH].namelen,
				outbuf, bytesused, user_attr);
			if (outbuf && (outbufsize != DEFAULT_OUTBUF_BUFSIZE)) {
			        free(outbuf);
			}

		}
		/*Do not delete this header for HEAD methods, same as above. bug 6441*/ 
		if(!cl7_proxy && !CHECK_HTTP_FLAG(&con->http, HRF_METHOD_HEAD))
			delete_known_header(&con->http.hdr, MIME_HDR_IF_NONE_MATCH);
	}
#undef DEFAULT_OUTBUF_BUFSIZE
#undef OUTBUF
}

static void handle_via(con_t *con, const namespace_config_t *nsconf)
{
	int ret;
	const char *via_val = 0;
	int via_vallen = 0;
	char *via_hdr;
	u_int32_t attrs;
	int hdrcnt;

	/*
	 *  Add "Via" header
	 *
	 * Add the port from which the request came to Via header.
	 * BZ 3681.
	 * Use dest_port, this seems to have the correct port value.
	 * BZ 3811.
	 */
	if (!nkn_http_is_transparent_proxy(nsconf)) { 
		via_hdr = (char *)alloca(myhostname_len + 1024);
		ret = snprintf(via_hdr, myhostname_len + 1024, "%s %s:%hu", 
			(CHECK_HTTP_FLAG(&con->http, HRF_HTTP_10) ? "1.0" : "1.1"),
			myhostname, ntohs(con->dest_port));
		if (is_known_header_present(&con->http.hdr, MIME_HDR_VIA)) {
			get_known_header(&con->http.hdr, MIME_HDR_VIA, &via_val, &via_vallen,
				&attrs, &hdrcnt);
			TSTR(via_val, via_vallen, viastr);
			if ((strstr(viastr, myhostname) == NULL) && (ret > 0)) {
				add_known_header(&con->http.hdr, MIME_HDR_VIA, via_hdr, ret);
			}
		}
		else {
			if (ret > 0) {
				add_known_header(&con->http.hdr, MIME_HDR_VIA, via_hdr, ret);
			}
		}
	}
}

static 
void handle_no_cache_request(con_t *con, const namespace_config_t *nsconf,
			     int consider_no_cache)
{
	// Pragma: no-cache request
	if (consider_no_cache && 
	    is_known_header_present(&con->http.hdr, MIME_HDR_PRAGMA)) {
		const char *data;
		int datalen;
		u_int32_t attr;
		int header_cnt;

		if (!get_known_header(&con->http.hdr, MIME_HDR_PRAGMA,
				      &data, &datalen, &attr, &header_cnt)) {
			if ((header_cnt == 1) && 
			    (strncasecmp(data, "no-cache", datalen) == 0)) {
				http_set_no_cache_request(&con->http);
				return;
			}
		}
	}
	// Client Cache-Control: no-cache request
	if (is_known_header_present(&con->http.hdr, MIME_HDR_CACHE_CONTROL)) {
		int ret;
		const cooked_data_types_t *ckdp;
		int ckd_len;
		mime_hdr_datatype_t dtype;
		ret = get_cooked_known_header_data(&con->http.hdr, 
					MIME_HDR_CACHE_CONTROL, &ckdp, &ckd_len,
		                        &dtype);
		/* BZ 4664
		 *  Case 1: Cache-Control: max-age = 0
		 * 	Check whether the CLI also specifies that
		 * 	we can serve this object from cache (override mode).
		 * 	If the CLI says to serve from origin, then set
		 * 	revalidate for the object
		 *
		 *  Case 2: Cache-Control: max-age = NN
		 * 	Check the age value given by the client against the
		 * 	CLI configured value. If the age exceeds,
		 * 		a. Serve from Origin; OR
		 * 		b. Serve from Cache
		 *
		 *  Case 3: cache-Control: <XXXXX>
		 * 	We dont handle this yet!!
		 * 
		 *  TODO: we need to check the actual age of the object
		 *  	before we make these checks and take a
		 * 	decision purely based on the client-request.
		 *
		 * */

		if (!ret && (dtype == DT_CACHECONTROL_ENUM) ) {
		    if (consider_no_cache &&
		    	(ckdp->u.dt_cachecontrol_enum.mask & 
				(CT_MASK_NO_CACHE|CT_MASK_NO_STORE))) {
		    	http_set_no_cache_request(&con->http);
		    } else if ((ckdp->u.dt_cachecontrol_enum.mask & CT_MASK_MAX_AGE) &&
		    	(ckdp->u.dt_cachecontrol_enum.max_age >= 
			 nsconf->http_config->policies.client_req_max_age) &&
			(nsconf->http_config->policies.client_req_serve_from_origin)) {
		    	/* For now, treat this case also as no cache request
		    	 * Will do actual revalidation in Release 1.1
		    	 * BZ 3073.
		    	 */
			con->http.req_max_age = ckdp->u.dt_cachecontrol_enum.max_age;
			http_set_revalidate_cache_request(&con->http);
		    } 
		}
	} 
}

static int CL7_proxy_request(con_t *con, namespace_token_t ns_token, 
				namespace_token_t node_ns_token)
{
	/* 
	 *  At this point, ns_token and node_ns_token are valid 
	 *  with refcnt=1.
	 */
	int ret;
	const namespace_config_t *nsconf;
	const namespace_config_t *node_nsconf;

	int hostlen;
	char host[2049];
	char *p_host;
	uint16_t port;

	int tracelog = 0;
	const char *trc_host_str = 0;
	const char *trc_uri_str = 0;
	const char *node_host_str = 0;
	char tbuf[1024];

	host[0] = '\0';
	p_host = host+1;
        char header_cluster[MAX_HTTP_HEADER_SIZE + 1];
        int header_cluster_len;

	/* Trace log support */
	if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
		int rv;
		const char *thost = 0;
		int thostlen;
		const char *uri = 0;
		int urilen;
		u_int32_t attrs;
		int hdrcnt;

		rv = get_known_header(&con->http.hdr, MIME_HDR_HOST, 
			&thost, &thostlen,
			&attrs, &hdrcnt);
		if (!rv) {
			trc_host_str = alloca(thostlen+1);
			memcpy((char *)trc_host_str, thost, thostlen);
			((char *)trc_host_str)[thostlen] = '\0';
		}
		rv = get_known_header(&con->http.hdr, MIME_HDR_X_NKN_URI, &uri, 
			&urilen, &attrs, &hdrcnt);
		if (!rv) {
			trc_uri_str = alloca(urilen+1);
			memcpy((char *)trc_uri_str, uri, urilen);
			((char *)trc_uri_str)[urilen] = '\0';
		}
		tracelog = 1;
	}

	/* Create cluster node IP:Port string */
	node_nsconf = namespace_to_config(node_ns_token);
	if (node_nsconf) {
		origin_server_HTTP_t *os_http;

		os_http = node_nsconf->http_config->origin.u.http.server[0];
		node_host_str = alloca(os_http->hostname_strlen+6+1);
		memcpy((char *)node_host_str, 
			os_http->hostname, os_http->hostname_strlen);
		snprintf((char *) &node_host_str[os_http->hostname_strlen],
			6+1, ":%hu", os_http->port);
		((char *)node_host_str) [os_http->hostname_strlen+6+1-1] = '\0';
	} else {
		// Should never happen
		DBG_LOG(MSG, MOD_HTTP, 
			"No CL7 node namespace defined (fd = %d)", con->fd);
		CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
		http_build_res_403(&con->http, NKN_HTTP_NAMESPACE); 
		http_close_conn(con, NKN_HTTP_NAMESPACE);
		return FALSE;
	}

	// Note: +1 refcnt on node_nsconf (node_ns_token) at this point

	con->http.cl_l7_ns_token = ns_token;
	con->http.cl_l7_node_ns_token = node_ns_token;
	SET_HTTP_FLAG(&con->http, HRF_CL_L7_PROXY_REQUEST);

	/*
	 * nkn_http_pers_conn_nums:
	 * case 0: follow browser requirement
	 * case 1: close the socket after each transaction.
	 * case n: close the socket after "n" transactions.
	 */
	con->num_of_trans++;
	if (nkn_http_pers_conn_nums && 
	    (nkn_http_pers_conn_nums == con->num_of_trans) ) {
		CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
	}

	/*
	 *  Add request destination IP/Port header (network byte order)
	 */
	setup_dest_ip_port_hdrs(&con->http.hdr, IPv4(con->dest_ipv4v6), 
				con->dest_port, 0 /* !overwrite */);
	/*
	 * Set the Module type as NORMALIZER, so that the con structure gets
	 * passed to AM if the uri is normalized (Hex encoded chars are present)
	 * This will be required when MM fetches the object from the Origin
	 */
	ret = is_known_header_present(&con->http.hdr, 
					MIME_HDR_X_NKN_DECODED_URI);
	if (ret) {
		con->module_type = NORMALIZER;
	}
	con->http.ns_token = ns_token;
	nsconf = namespace_to_config(ns_token);

	if (!(nsconf->http_config->response_protocol & DP_HTTP)) {
		DBG_LOG(MSG, MOD_HTTP, 
			"CL7 Bad protocol (fd = %d) - http:// - for a "
			"namespace that doesnt allow http.", con->fd);
		/* This namespace cannot allow a HTTP request */
		CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
		http_build_res_403(&con->http, NKN_HTTP_NAMESPACE); 
		http_close_conn(con, NKN_HTTP_NAMESPACE);
		release_namespace_token_t(node_ns_token);
		return FALSE;
	}

	DBG_LOG(MSG, MOD_HTTP, 
		"CL7 Associating request fd=%d with namespace [%s] "
		"gen=%u token=%u node namespace [%s]",
		con->fd, nsconf->namespace,
		con->http.ns_token.u.token_data.gen,
		con->http.ns_token.u.token_data.val,
		node_nsconf->namespace);

	if (tracelog) {
		DBG_TRACE("CL7 Proxy URL %s%s matched namespace %s, "
			"proxying [%s]",
			trc_host_str, trc_uri_str,
			nsconf ? nsconf->namespace : "", node_host_str);
	}

	con->http.nsconf = nsconf;

	/* HTTPS considerations */
	if (CHECK_CON_FLAG(con, CONF_HTTPS_HDR_RECVD)) {
		if (!(nsconf->http_config->client_delivery_type & 
		    		DP_CLIENT_RES_SECURE)) {
			CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
			http_build_res_403(&con->http, NKN_HTTPS_NAMESPACE);
                        http_close_conn(con, NKN_HTTPS_NAMESPACE);
			release_namespace_token_t(node_ns_token);
			return FALSE;
		}
	} else {
		if (!(nsconf->http_config->client_delivery_type & 
				DP_CLIENT_RES_NON_SECURE)) {
			CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
			http_build_res_403(&con->http, NKN_HTTP_NAMESPACE);
                        http_close_conn(con, NKN_HTTP_NAMESPACE);
			release_namespace_token_t(node_ns_token);
			return FALSE;
		}
	}

	/* update the bytes received count for this namespace */
	NS_UPDATE_STATS((con->http.nsconf), PROTO_HTTP, client, in_bytes, 
			(con->http.cb_offsetlen));
	/* Update the request count for this namespace */
	NS_INCR_STATS((con->http.nsconf), PROTO_HTTP, client, requests);

	/*
	 * Requirement 2.1 - 34.
	 * Check for maximum session count reached in namespace.
	 */
	if (nkn_resource_pool_enable &&
	    !(nvsd_rp_alloc_resource(RESOURCE_POOL_TYPE_CLIENT_SESSION,
					nsconf->rp_index, 1))) {
		/*
		 * make sure we dont hold this connection, even if it
		 * was marked for a keep-alive.
		 */
		CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
		/* Build the response message */
		http_build_res_503(&con->http, NKN_HTTP_NS_MAX_CONNS);

		http_close_conn(con, NKN_HTTP_NS_MAX_CONNS);
		release_namespace_token_t(node_ns_token);
		return FALSE;
	} else {
		SET_HTTP_FLAG(&con->http, HRF_RP_SESSION_ALLOCATED);
		AO_fetch_and_add1((volatile AO_t *)&(nsconf->stat->g_http_sessions));
		NS_INCR_STATS(nsconf, PROTO_HTTP, client, active_conns);
		NS_INCR_STATS(nsconf, PROTO_HTTP, client, conns);
	}


        if (nsconf->http_config->origin.select_method == OSS_HTTP_DEST_IP) {
            if (http_add_real_dest_ip_port_header(con) == FALSE) {
                release_namespace_token_t(node_ns_token);
                return FALSE;
            }
        }

	ret = request_to_origin_server(REQ2OS_CACHE_KEY, 0, &con->http.hdr, 
				       nsconf, 0, 0, &p_host, sizeof(host)-1,
				       &hostlen, &port, 0, 1);
	if (!ret) {
		/*
		 * Note the parent namespace cache key for use in
		 * the generation of the node namespace cache key.
		 */
		ret = add_known_header(&con->http.hdr, 
				 MIME_HDR_X_NKN_CL7_CACHEKEY_HOST, 
				 (const char *)p_host, hostlen);
		if (ret) {
			DBG_LOG(MSG, MOD_HTTP, 
				"add_known_header"
				"(MIME_HDR_X_NKN_CL7_CACHEKEY_HOST) "
				"failed, rv=%d", ret);
			glob_cll7proxy_req_int_err1++;
			goto err_exit;
		}
		/*
		 * Note the parent namespace origin host:port
		 * for use in request retry when the proxy node is unreachable.
		 */
		ret = request_to_origin_server(REQ2OS_ORIGIN_SERVER_LOOKUP, 
					0, &con->http.hdr, 
				       	nsconf, 0, 0, &p_host, sizeof(host)-1,
				       	&hostlen, &port, 0, 1);
		if (ret) {
			DBG_LOG(MSG, MOD_HTTP, 
				"request_to_origin_server() failed, rv=%d", 
				ret);
			glob_cll7proxy_req_int_err2++;
			goto err_exit;
		}
		/* Note: Prepend origin host:port with NULL */
		ret = add_known_header(&con->http.hdr, 
				 MIME_HDR_X_NKN_CL7_ORIGIN_HOST, 
				 (const char *)(p_host-1), hostlen+1);
		if (ret) {
			DBG_LOG(MSG, MOD_HTTP, 
				"add_known_header"
				"(MIME_HDR_X_NKN_CL7_ORIGIN_HOST) "
				"failed, rv=%d", ret);
			glob_cll7proxy_req_int_err3++;
			goto err_exit;
		}
	} else {
		DBG_LOG(MSG, MOD_HTTP, 
			"request_to_origin_server() failed, rv=%d", ret);
		glob_cll7proxy_req_int_err4++;
		goto err_exit;
	}

	/* Access log status header */
	ret = snprintf(tbuf, sizeof(tbuf), "Use=%s", node_host_str);
	update_known_header(&con->http.hdr, MIME_HDR_X_NKN_CL7_STATUS,
			tbuf, ret, 0);

	L7_SWT_NODE_NS(&con->http);
	con->http.req_cod = http_get_req_cod(con);
	if (con->http.req_cod == NKN_COD_NULL) {
		L7_SWT_PARENT_NS(&con->http);
		DBG_LOG(MSG, MOD_HTTP, "http_get_req_cod() failed");
		glob_cll7proxy_req_int_err5++;
		goto err_exit;
	}

	if (!http_check_mfd_limitation(&con->http, PE_ACTION_NO_ACTION)) {
		L7_SWT_PARENT_NS(&con->http); // Tunnel under parent NS config
		ret = fp_make_tunnel(con, -1, 0, 0);
		if (!ret) {
			release_namespace_token_t(node_ns_token);
			glob_cll7proxy_req_tunneled++;
   			return TRUE;
		} else {
			glob_cll7proxy_req_tunnel_err++;
			goto err_exit;
		}
	}

	/*
	 * Add the MIME_HDR_X_NKN_CL7_PROXY if applicable
	 */
	if (nsconf->http_config->origin.u.http.include_orig_ip_port) {
                if( http_add_real_ip_port_header(con) == FALSE) {
                        release_namespace_token_t(node_ns_token);
                        return FALSE;
                }

                if (http_create_header_cluster(con, MIME_HDR_X_NKN_CL7_PROXY, 
                        header_cluster, &header_cluster_len) == FALSE) {
                        goto err_exit;
                }

                ret = add_known_header(&con->http.hdr, MIME_HDR_X_NKN_CL7_PROXY,
                                      (const char *)header_cluster, header_cluster_len);
                if (ret) {
                        DBG_LOG(MSG, MOD_HTTP,
                                "Error in add_known_header, ret = %d", ret);
                        goto err_exit;
                }
	}

	/*
	 * Handle via header
	 */
	handle_via(con, con->http.nsconf);
	L7_SWT_PARENT_NS(&con->http);

	/*
	 * Handle If-Modified-Since and If-None-Match directives
	 */
	handle_if_directives(con);

	if (tracelog) {
		DBG_TRACE("CL7 Proxy URL %s%s proxy to [%s] initiated",
			trc_host_str, trc_uri_str, node_host_str);
	}

	// Handle Client Cache-Control: no-cache request
	handle_no_cache_request(con, node_nsconf, 1);
	release_namespace_token_t(node_ns_token);

	nkn_post_sched_a_task(con);
   	return TRUE;

err_exit:

	if (tracelog) {
		DBG_TRACE("CL7 Proxy URL %s%s proxy to [%s] "
			"request setup failed",
			trc_host_str, trc_uri_str, node_host_str);
	}
	CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
	http_build_res_500(&con->http, NKN_CL7_PROXY_ERR);
	http_close_conn(con, NKN_CL7_PROXY_ERR);
	release_namespace_token_t(node_ns_token);
	return FALSE;

}

////////////////////////////////////////////////////////////////////////
// HTTP response
////////////////////////////////////////////////////////////////////////

//
// For functions http_send_data_chuck_header() and http_send_data()
// return >0: means so much data has been sent out
// return -1: means EWOULDBLOCK
// return -2: unexpected error, socket should close.
//
static int http_send_data_chuck_header(con_t * con, int len)
{
	// send encoding header first.
       char linebuf[64];
       int sizebuf, ret;

	sizebuf=snprintf(linebuf, 64, "\r\n%x\r\n", len);
	ret = send(con->fd, linebuf, sizebuf, 0);
	if( ret == -1 )  {
		if( errno == EAGAIN ) return -1;
		return -2;
	}

       AO_fetch_and_add(&glob_client_send_tot_bytes, ret);
       return 0;
}

static void update_namespace_stats(con_t *con, int ret);
static void update_namespace_stats(con_t *con, int ret)
{
	switch(con->http.provider) {
	case BufferCache_provider:
		NS_UPDATE_STATS((con->http.nsconf), PROTO_HTTP, client, out_bytes_ram, ret);
		break;
	case SASDiskMgr_provider:
	case SAS2DiskMgr_provider:
	case SATADiskMgr_provider:
	case SolidStateMgr_provider:
		NS_UPDATE_STATS((con->http.nsconf), PROTO_HTTP, client, out_bytes_disk, ret);
		break;
	case NFSMgr_provider:
	case OriginMgr_provider:
		NS_UPDATE_STATS((con->http.nsconf), PROTO_HTTP, client, out_bytes_origin, ret);
		break;
	case Tunnel_provider:
		// Should never happen
		break;
	default:
		break;
	}
	NS_UPDATE_STATS((con->http.nsconf), PROTO_HTTP, client, out_bytes, ret);
	return;
}

int http_send_data(con_t * con, char * p, int len)
{
        int sockfd=con->fd;
        int ret;

	if( CHECK_HTTP_FLAG(&con->http, HRF_SUPPRESS_SEND_DATA) ) {
		ret = 0;
		glob_tot_bytes_sent += ret;
		return len;
	}

	if( CHECK_HTTP_FLAG(&con->http, HRF_TRANSCODE_CHUNKED) ) {
		ret = http_send_data_chuck_header(con, len);
		if(ret < 0) return ret;
	}

        ret = send(sockfd, p, len, 0);
	if( ret == -1 )  {
		if( errno == EAGAIN ) return -1;
	//
	// The most popular error are EPIPE and ECONNRESET
	// EPIPE: data sent when socket has been closed.
	// ECONNRESET: data sent wehn socket has been reset
	//
	// We have called signal(EPIPE, SIG_IGN) to ignore the EPIPE signal
	//
               	return -2;
	}

       /*
        * If the log format has %{X-NKN-MD5-Checksum}o & the MD5 checksum 
        * string length configured, calculate md5 chksum for 'ret' number of characters 
        * and update the md5-context in http connection 'con'.
        */
        len = ret ;
        if (con->http.nsconf && 
            con->http.nsconf->acclog_config->al_resp_hdr_md5_chksum_configured && 
            con->http.nsconf->md5_checksum_len &&
            con->md5 &&
            (con->http.respcode == 200 || con->http.respcode == 206)) {

            int md5_total_len = con->md5->in_strlen + len ;

            // Update the MD5 context with 'len' response data string.
            if(md5_total_len > con->http.nsconf->md5_checksum_len) {
                len = con->http.nsconf->md5_checksum_len - con->md5->in_strlen ;
            }  

            if (len > 0) {
                con->md5->temp_seq ++ ;
 
                // Do MD5 update only when the data length is less than the 
                // configured number of bytes.
                con->md5->in_strlen += len ;

                MD5_Update(&con->md5->ctxt, p, len);
                DBG_LOG(MSG, MOD_HTTP,  "MD5_CHECKSUM(Update[%d]):[Len:%d], con=%p fd=%d r=%d, s=%d, l=%ld",
                        con->md5->temp_seq,
                        len, con, con->fd,
                        con->http.respcode, con->http.subcode,
                        con->http.content_length);
            }
        }

	SET_CON_FLAG(con, CONF_SEND_DATA);

        if (!CHECK_HTTP_FLAG(&con->http, HRF_MFC_PROBE_REQ)) {	
		glob_tot_bytes_sent += ret;
		AO_fetch_and_add(&glob_client_send_tot_bytes, ret);
		update_namespace_stats(con, ret);
	}

        return ret;
}



static int value_in_list(const char *value, int value_len, 
			 const char *list, int list_len)
{
 	char *needle;
 	char *haystack;

 	needle = alloca(value_len+1);
	memcpy(needle, value, value_len);
	needle[value_len] = '\0';

 	haystack = alloca(list_len+1);
	memcpy(haystack, list, list_len);
	haystack[list_len] = '\0';

	char *p;
	p = strstr(haystack, needle);
	if (p && ((p == haystack) || (*(p-1) == ' ') || (*(p-1) == ',')) &&
	    (!p[value_len] ||
	     (p[value_len] == ' ') || (p[value_len] == ','))) {
	    return 1; // Match
	} else {
	    return 0; // No match
	}
}

static int handle_if_conditionals(con_t *con) 
{
	mime_header_t *req_hdr = &con->http.hdr;
	const char *if_mod_since = 0;
	int if_mod_since_len;
	const char *if_none_match = 0;
	int if_none_match_len;
	//int if_none_match_cnt = 0;
	int if_none_match_valid = 0;
	u_int32_t attrs;
	int rv;
	const char *response_modified_header;
	int response_modified_header_len;
	const char *response_etag_header;
	int response_etag_header_len;
	int n, hdrcnt;
	const char *name;
	int name_len;
	const char *val;
	int val_len;
	char *p_start ;
	const char delimiter[] = ",";
	char *tokenstr = NULL;

	typedef struct name_len {
		const char *name;
		int name_len;
	} name_len_t;

	name_len_t **hdr_to_delete;
	int hdrs_to_delete_cnt;
	int token;
	mime_header_t response_hdr;

	init_http_header(&response_hdr, 0, 0);
	rv = nkn_attr2http_header(con->http.attr, 1, &response_hdr);
	if (rv) {
		DBG_LOG(MSG, MOD_HTTP, "nkn_attr2http_header() failed,  rv=%d", rv);
		return 5;
	}


	rv = get_namevalue_header(req_hdr,
			http_known_headers[MIME_HDR_IF_MODIFIED_SINCE].name,
			http_known_headers[MIME_HDR_IF_MODIFIED_SINCE].namelen,
			&if_mod_since,
			&if_mod_since_len, &attrs);
	if (!rv) {

		rv = get_known_header(&response_hdr, MIME_HDR_LAST_MODIFIED, 
				&response_modified_header, 
				&response_modified_header_len, 
				&attrs, &hdrcnt);
		if (rv) {
			DBG_LOG(MSG, MOD_HTTP, 
				"get_namevalue_header(\"Last-Modified\") "
				"failed, rv=%d", rv);
			shutdown_http_header(&response_hdr);
			return 1;
		}

               /*
                *  If IMS time is valid and Last-Modified time > IMS, return 2.
                */
                if (if_mod_since_len != response_modified_header_len) {
			DBG_LOG(MSG, MOD_HTTP, "Object modified, IMS==true");
			shutdown_http_header(&response_hdr);
			return 2;
                } else {
                       /*
                        * Variables used for IMS(If-Modified-Since),
                        * LMH (Last Modified) timestamp comparison.
                        */
                        time_t  ims_time = 0, lmh_time = 0 ;
 
                        ims_time = parse_date(if_mod_since, if_mod_since
                                                          + if_mod_since_len - 1) ;
                        lmh_time = parse_date(response_modified_header,
                                              response_modified_header +
                                              response_modified_header_len - 1) ;
                        if (ims_time /* IMS Time valid */
                            && ims_time < lmh_time) {
                               DBG_LOG(MSG, MOD_HTTP, "Object modified, IMS==true");
                               shutdown_http_header(&response_hdr);
                               return 2;
                        }
            	}
	}
	rv = get_namevalue_header(req_hdr,
			http_known_headers[MIME_HDR_IF_NONE_MATCH].name,
			http_known_headers[MIME_HDR_IF_NONE_MATCH].namelen,
			&if_none_match,
			&if_none_match_len, &attrs);
	if (!rv) {

		rv = get_known_header(&response_hdr, MIME_HDR_ETAG, 
				&response_etag_header, 
				&response_etag_header_len, 
				&attrs, &hdrcnt);
		if (rv) {
			DBG_LOG(MSG, MOD_HTTP, 
				"get_namevalue_header(\"Etag\") failed, "
				"rv=%d", rv);
			shutdown_http_header(&response_hdr);
			return 3;
		}
		p_start = alloca(if_none_match_len + 1);
		strncpy(p_start, if_none_match, if_none_match_len);
		/*
		 * Bug 5186 - it should be terminated with null. 
		 * Otherwise value_in_list fucntion wont find the match 
		 * if p_start[if_none_match_len] != '\0' 
		 */
		p_start[if_none_match_len] = '\0';
		tokenstr = strtok(p_start, delimiter);
		while(tokenstr != NULL) {

			if (((strlen(tokenstr) == 1) && (*tokenstr == '*')) ||
				value_in_list(response_etag_header, 
		    		   response_etag_header_len,
				   tokenstr, strlen(tokenstr))) {
				if_none_match_valid = 1;
				break;
			}

			tokenstr = strtok(NULL, delimiter);
		}
		if (!if_none_match_valid) {
			DBG_LOG(MSG, MOD_HTTP, "Object modified, INM==true");
			shutdown_http_header(&response_hdr);
			return 4;
		}
	}

   	hdr_to_delete = alloca(req_hdr->cnt_namevalue_headers * 
				sizeof(name_len_t));
	hdrs_to_delete_cnt = 0;

	// Only return allowable 304 response headers
	for (n = 0; (get_nth_namevalue_header(req_hdr, n, &name, &name_len,
					 &val, &val_len, &attrs) == 0); n++) {
		if (ATTR2USERATTR(attrs) != HTTP_PAIR_RES) {
			continue;
		}
		rv = string_to_known_header_enum(name, name_len, &token);
		if (rv || !http_allowed_304_header[token]) {
			hdr_to_delete[hdrs_to_delete_cnt] = 
				alloca(sizeof(name_len_t));
			hdr_to_delete[hdrs_to_delete_cnt]->name = name;
			hdr_to_delete[hdrs_to_delete_cnt]->name_len = name_len;
			hdrs_to_delete_cnt++;
		}
	}

	for (n = 0; n < hdrs_to_delete_cnt; n++) {
		delete_namevalue_header(req_hdr, hdr_to_delete[n]->name,
					hdr_to_delete[n]->name_len);
	}
	http_build_res_304(&con->http, 0, &response_hdr);
	shutdown_http_header(&response_hdr);
	return 0;
}

static int http_send_response_header(con_t * con)
{
	http_cb_t * phttp;
	char outbuf[4096];
	int close_client_conn = 0;

        phttp = &con->http;

	DBG_LOG(MSG, MOD_HTTP, "con=%p fd=%d", con, con->fd);

	/* For persistent connection remote port is not updated.
	 * Copy it from con->dest_port.
	 * BZ 3814.
	 */
	if (phttp->remote_port == 0)
		phttp->remote_port = ntohs(con->dest_port);

	if (is_namevalue_header_present(&con->http.hdr, 
			http_known_headers[MIME_HDR_IF_MODIFIED_SINCE].name,
			http_known_headers[MIME_HDR_IF_MODIFIED_SINCE].namelen) ||
	    is_namevalue_header_present(&con->http.hdr, 
			http_known_headers[MIME_HDR_IF_NONE_MATCH].name,
			http_known_headers[MIME_HDR_IF_NONE_MATCH].namelen)) 
	{
		if (handle_if_conditionals(con) == 0) {
			SET_HTTP_FLAG(&con->http, HRF_SUPPRESS_SEND_DATA);
			goto send_response;
		}
	}

        // Case Range: bytes=101-
        if( CHECK_HTTP_FLAG(phttp, HRF_BYTE_RANGE) ) {
                // Some Sanity check
                if(phttp->brstart<0 || phttp->brstop<0) {
                        http_build_res_400(phttp, NKN_HTTP_RES_RANGE_1);
			close_client_conn = 1;
			goto send_response;
                }

		if (phttp->content_length == 0) {
                        http_build_res_500(phttp, NKN_HTTP_GET_CONT_LENGTH);
			close_client_conn = 1;
			goto send_response;
		}
                if(!CHECK_HTTP_FLAG(phttp,HRF_BYTE_RANGE_START_STOP) && phttp->brstop==0) {
                        phttp->brstop=phttp->content_length - 1;
                } else if(phttp->brstop>=phttp->content_length) {
                        phttp->brstop=phttp->content_length - 1;
                }

                if(CHECK_HTTP_FLAG(phttp,HRF_BYTE_RANGE_START_STOP) && (phttp->brstart > phttp->brstop)) {
                        http_build_res_400(phttp, NKN_HTTP_RES_RANGE_2);
			close_client_conn = 1;
			goto send_response;
                } else {
			/* If its NFS tunnel, its a 200 OK response */
			if(phttp->nfs_tunnel)
				http_build_res_200(&con->http);
			else
				http_build_res_206(&con->http);
		}

        } else if( CHECK_HTTP_FLAG(phttp, HRF_BYTE_SEEK) ) {
                if(phttp->seekstop==0) {
                        phttp->seekstop=phttp->content_length - 1;
                } else if(phttp->seekstop>=phttp->content_length) {
                        phttp->seekstop=phttp->content_length - 1;
                }

                if(phttp->seekstart > phttp->seekstop) {
                        http_build_res_400(phttp, NKN_HTTP_RES_SEEK);
			close_client_conn = 1;
			goto send_response;
                } else {
			/* If its NFS tunnel, its a 200 OK response */
			if(phttp->nfs_tunnel)
				http_build_res_200(&con->http);
			else
				http_build_res_206(&con->http);
		}

        } else {
                if(con->http.attr && (con->http.attr->na_flags2 & NKN_OBJ_NEGCACHE)&& 
			(con->http.attr->na_respcode > 0)) {
		    http_build_res_unsuccessful(&con->http, con->http.attr->na_respcode);

		    if(con->http.attr->content_length == 0) { 
			SET_HTTP_FLAG(&con->http, HRF_SUPPRESS_SEND_DATA);
		    }


		    goto send_response;
                }

                http_build_res_200(&con->http);
		goto send_response;
        }

send_response:
	/* For HEAD request data must not be sent to the client, though 
	request has byte range or byte seek or any option. so below check 
	must be done for all response messages - refer bug 4451 */
	if (CHECK_HTTP_FLAG(phttp, HRF_METHOD_HEAD)) {
		SET_HTTP_FLAG(&con->http, HRF_SUPPRESS_SEND_DATA);
	}

	if (!con->http.resp_buf) {
		// Response buffer size exceeded, drop connection
                return SOCKET_CLOSED;
	}

	if (DO_DBG_LOG(MSG, MOD_HTTP)) {
		DBG_LOG(MSG, MOD_HTTP, "fd=%d\n%s", con->fd,
			dump_http_header(&con->http.hdr, outbuf,
					 sizeof(outbuf), 1));
	}

	DBG_LOG(MSG, MOD_HTTP, "fd=%d raw response buffer [%s]", 
		con->fd, con->http.resp_buf);

	return close_client_conn ? SOCKET_CLOSED : SOCKET_DATA_SENTOUT;
}

static void
reset_con_for_receive(con_t *con)
{
        con->http.num_nkn_iovecs=0;
	if (con->http.prepend_content_data) {
		free(con->http.prepend_content_data);
		con->http.prepend_content_data = 0;
	}
	if (con->http.append_content_data) {
		free(con->http.append_content_data);
		con->http.append_content_data = 0;
	}
	CLEAR_HTTP_FLAG(&con->http, 
	  (HRF_ZERO_DATA_ERR_RETURN|HRF_PREPEND_IOV_DATA|HRF_APPEND_IOV_DATA));
}

void http_pe_overwrite_ssp_params(con_t *con)
{
        // Avoid the case where there are no virtual-player associated
        //     with the namespace or an empty virtual-player config is attached.
        if ((con->http.nsconf == NULL)
            || (con->http.nsconf->ssp_config == NULL)
            || (con->http.nsconf->ssp_config->player_name == NULL)) {
                DBG_TRACE("pe_overwrite : Virtual-player not configured..") ;
                return ;
        }

        // Override ssp set values with PE values for transmit rate.
        if (con->pe_ssp_args) {
                if (con->pe_ssp_args->max_bw) {
                        uint64_t bw = 0;
                        float burst_factor = con->http.nsconf->ssp_config->rate_control.burst_factor;
			uint64_t bit_rate = 0 ;

                        // Add burst factor and 20% TCP/IP overhead to bit-rate.
                        bit_rate = (uint64_t) (burst_factor * 1.2 * (float)con->pe_ssp_args->bit_rate) ;

                        // Min of max-bw and bit-rate values set by PE is taken as bandwidth.
                        bw = MIN(con->pe_ssp_args->max_bw, bit_rate) ;

                        AO_fetch_and_add(&con->pns->max_allowed_bw,
                                          (con->max_bandwidth - bw));
                        con->max_bandwidth = bw ;
                }
                if (con->pe_ssp_args->fast_start_sz) {
                        // If Fast start is given in seconds, buf size is secs * max-bw.
                        if (con->pe_ssp_args->fast_start_unit_sec) {
                                con->max_faststart_buf = con->pe_ssp_args->fast_start_sz * con->max_bandwidth ;
                        } else { // Else the value from PE script for fast-start-size.
                                con->max_faststart_buf = con->pe_ssp_args->fast_start_sz ;
                        }
                }
        }

	return ;
}

//
// send_to_ssp() -- Pass back the data contents to SSP instead of pushing
//    it OTW.
//
static
int send_to_ssp(con_t *con)
{
        int ret = 0;
        long len;
        char * p;
        int i;
	int total_sent = 0;
	int error_exit = 1;
	int expected_ret = -1;

	/* bz 6931: changing the condition check here to do the
	 * following. if the BM GET task returns data we pass it to
	 * the SSP vector by vector. If the BM GET returns error then
	 * we send the DATA NOT FOUND flag
	 * 'OM_STAT_SIG_TOT_CONTENT_LEN' to SSP which can then bail
	 * out and clear states
	 */
	if (!con->http.num_nkn_iovecs){// ||
	    //( (con->http.provider == OriginMgr_provider) &&
	    // (con->http.respcode != 200) && 
	    // (con->http.respcode != 206))) {
		// Not found
    		ret = startSSP(con, OM_STAT_SIG_TOT_CONTENT_LEN, 0, 0);
		if (ret == 0 || ret == 2) {
			// ret == 0: Fetch and send OTW
		    	// ret == 2: Fetch and send to SSP
		    	if (ret == 0){
		    		CLEAR_HTTP_FLAG(&con->http, (HRF_RES_SEND_TO_SSP|HRF_RES_HEADER_SENT));
		    	} else {
				CLEAR_HTTP_FLAG(&con->http, (HRF_RES_HEADER_SENT));
				con->max_bandwidth = 0;
		    	}
			UNSET_CON_FLAG(con, CONF_NO_CONTENT_LENGTH);
			if (con->http.brstart) {
				SET_HTTP_FLAG(&con->http, HRF_BYTE_RANGE);
			}
			con->http.content_length = 0;
			error_exit = 0;
		} else if (ret == -NKN_SSP_FORCE_TUNNEL_PATH) {
		    // Request needs to be tunneled at this point
		    CLEAR_HTTP_FLAG(&con->http, (HRF_RES_SEND_TO_SSP|HRF_RES_HEADER_SENT|HRF_BYTE_RANGE));
		    UNSET_CON_FLAG(con, CONF_NO_CONTENT_LENGTH);
		    con->http.content_length = 0;
		    error_exit = ret;
		} else {
			// Expect ret=0 or ret=2 
			expected_ret = 100+2;
			error_exit = 1;
		}
		goto exit;
	}

    	for(i=0;i<con->http.num_nkn_iovecs;i++) {
    		len=con->http.content[i].iov_len;
    		if(len==0) continue;
    		p=con->http.content[i].iov_base;

    		ret = startSSP(con, con->http.content_length, p, len);

		http_pe_overwrite_ssp_params(con) ;

                con->http.content[i].iov_base += len;
                con->http.content[i].iov_len -= len;
                total_sent += len;

		if (total_sent < con->http.content_length) {
		    	// Expect ret=1 awaiting data
			// Expect ret=0 reinitiate request with target URL
                        // specified in known header MIME_HDR_X_NKN_REMAPPED_URI
		    if(ret == 0) { // Override with SEND_OTW
			DBG_LOG(MSG, MOD_SSP, "Virtual player reset the internal file fetch to a byte/seek range and enforced data to be sent over the wire");
			CLEAR_HTTP_FLAG(&con->http, HRF_RES_SEND_TO_SSP);
			//				if (con->http.brstart) {
			//SET_HTTP_FLAG(&con->http, HRF_BYTE_RANGE);
			//}
			con->http.content_length = 0;
			error_exit = 0;
			goto exit;
		    }
		    if (ret == 2) { // Re-initiate new request
                        CLEAR_HTTP_FLAG(&con->http, (HRF_RES_HEADER_SENT));
                        UNSET_CON_FLAG(con, CONF_NO_CONTENT_LENGTH);
                        if (con->http.brstart) {
                            SET_HTTP_FLAG(&con->http, HRF_BYTE_RANGE);
                        }
                        con->http.content_length = 0;
			con->max_bandwidth = 0;
                        error_exit = 0;
                        goto exit;
                    }
		    if (ret == -NKN_SSP_FORCE_TUNNEL_PATH) {
			// Request needs to be tunneled at this point
			CLEAR_HTTP_FLAG(&con->http, (HRF_RES_SEND_TO_SSP|HRF_RES_HEADER_SENT|HRF_BYTE_RANGE));
			UNSET_CON_FLAG(con, CONF_NO_CONTENT_LENGTH);
			con->http.content_length = 0;
			error_exit = ret;
			goto exit;
		    }
		    if (ret != 1) {
			expected_ret = 1;
			error_exit = 1;
			goto exit;
		    }
		} else {
		    	// Expect ret=0 reinitiate request with target URL
			// specified in known header MIME_HDR_X_NKN_REMAPPED_URI
		    if (ret == 2) { // Re-initiate new request
			CLEAR_HTTP_FLAG(&con->http, (HRF_RES_HEADER_SENT));
			UNSET_CON_FLAG(con, CONF_NO_CONTENT_LENGTH);
			if (con->http.brstart) {
			    SET_HTTP_FLAG(&con->http, HRF_BYTE_RANGE);
			}
			con->http.content_length = 0;
			con->max_bandwidth = 0;
			error_exit = 0;
			goto exit;
		    }
                    if (ret == -NKN_SSP_FORCE_TUNNEL_PATH) {
                        // Request needs to be tunneled at this point
                        CLEAR_HTTP_FLAG(&con->http, (HRF_RES_SEND_TO_SSP|HRF_RES_HEADER_SENT|HRF_BYTE_RANGE));
                        UNSET_CON_FLAG(con, CONF_NO_CONTENT_LENGTH);
                        con->http.content_length = 0;
                        error_exit = ret;
                        goto exit;
                    }
		    if (ret != 0) {
			con->max_bandwidth = 0;
			expected_ret = 0;
			error_exit = 1;
			goto exit;
		    }
		    CLEAR_HTTP_FLAG(&con->http, HRF_RES_SEND_TO_SSP);
		    if (con->http.brstart) {
			SET_HTTP_FLAG(&con->http, HRF_BYTE_RANGE);
		    }
		    con->http.content_length = 0;
		    error_exit = 0;
		    goto exit;
		}
	}

	con->http.brstart = total_sent + (con->http.brstart>0?con->http.brstart:0);
	con->http.brstop = con->http.content_length - 1;
	SET_HTTP_FLAG(&con->http, HRF_BYTE_RANGE);
	SET_HTTP_FLAG(&con->http, HRF_RES_SEND_TO_SSP);
	DBG_LOG(MSG, MOD_HTTP, "Chunked internal read for ssp from next 2MB block at offset %ld", con->http.brstart);
	return SOCKET_WAIT_CM_DATA;

exit:
	if (error_exit == -NKN_SSP_FORCE_TUNNEL_PATH) {
	    //delete_all_namevalue_headers(&con->http.hdr);
	    delete_known_header(&con->http.hdr, MIME_HDR_X_NKN_SEEK_URI);
	    reset_con_for_receive(con);
	    return error_exit;
	}

	if (!error_exit) {
	    // BZ 2036 check-in, r3962 deleted all name value headers for
	    // smoothflow internal loopback fetches
	    // BZ 6606 & 6551 requires removal of this call. (Reg test pending)
	    if (con->ssp.ssp_client_id == 4) { // Type 4 Smoothflow player only
	    	delete_all_namevalue_headers(&con->http.hdr);
	    } //BZ 6606 & 6551
		reset_con_for_receive(con);
        	return SOCKET_WAIT_CM_DATA;
	} else {
		DBG_LOG(MSG, MOD_HTTP, 
			"startSSP() phase error, ret=%d expected_ret=%d "
	    		"total_sent=%d content_length=%ld num_nkn_iovecs=%d\n",
			ret, expected_ret,
			total_sent, con->http.content_length, 
			con->http.num_nkn_iovecs);

	    	// Force socket close
		CLEAR_HTTP_FLAG(&con->http,
				HRF_CONNECTION_KEEP_ALIVE);
		
		// clean up the task and support abortive close
		if (nkn_do_conn_preclose_routine(con) == TRUE) {
		    return SOCKET_CLOSED_DONE_TRUE;
		} else {
		    http_close_conn(con, -ret);
		    return SOCKET_CLOSED_DONE_FALSE;
		}

	    	return SOCKET_CLOSED_DONE_TRUE;
	}
}

int http_apply_ssp(con_t *con, int *val)
{
	int ret = 0;


	// Perform URL remap
	ret = startSSP(con, 0, 0, 0);
	*val = ret;
	if (ret < 0) {
		// Reject connection
		if (ret == -NKN_SSP_FORCE_TUNNEL_PATH) {
			con->http.tunnel_reason_code = NKN_TR_REQ_SSP_FORCE_TUNNEL;
			ret = fp_make_tunnel(con, -1, 0, 0);
			if (ret != 0) {
				CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
				http_build_res_404(&con->http, ret);
				http_close_conn(con, ret);
				return FALSE;
			}
		} else {
			CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
			http_close_conn(con, -ret);
			return FALSE;
		}
		return TRUE;
	} else if (ret == 2) {
		// Send response data back to SSP and disable byte range
		// handling until we have the target URL
		SET_HTTP_FLAG(&con->http, HRF_RES_SEND_TO_SSP);
		CLEAR_HTTP_FLAG(&con->http, HRF_BYTE_RANGE);
	} else if (ret == 3) {
		// Reply with 200 OK
		con->http.content_length = sizeof_response_200_OK_body;
		con->http.content[0].iov_base = (char *)response_200_OK_body;
		con->http.content[0].iov_len = sizeof_response_200_OK_body;
		con->http.num_nkn_iovecs = 1;
		add_namevalue_header(&con->http.hdr,
				http_known_headers[MIME_HDR_CONTENT_TYPE].name,
				http_known_headers[MIME_HDR_CONTENT_TYPE].namelen,
				"text/html", 9, 0);
				http_try_to_sendout_data(con);
		ret = TRUE;
	}

	return (ret);
}

int http_send_response(con_t *con)
{
        int ret, cplen;
	int update_nkn_cur_ts = 0;
        long len;
        char * p;
        int i;
	struct iovec iov[4];
	const namespace_config_t *nsc;

	nsc = con->http.nsconf;

	// Requested data available, notify listeners prior to processing data
	updateSSP(con);

	if( CHECK_HTTP_FLAG(&con->http, HRF_RES_SEND_TO_SSP) ) {
	    //		return send_to_ssp(con);
	    ret = send_to_ssp(con);
	    if (ret == -NKN_SSP_FORCE_TUNNEL_PATH) {
		con->http.tunnel_reason_code = NKN_TR_REQ_SSP_FORCE_TUNNEL;
                ret = fp_make_tunnel(con, -1, 0, 0); // Tunnel the seek request to origin
                if (ret != 0) {
		    CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);
                    http_build_res_404(&con->http, ret);
                    http_close_conn(con, ret);
                }
		ret = SOCKET_NO_ACTION; // request is already tunneled, no further action required
            }
	    return ret;
	}

	if(!con->http.num_nkn_iovecs &&
		CHECK_HTTP_FLAG(&con->http, HRF_MULTIPART_RESPONSE)) {
		/*
		 * Multipart data not found, close connection
		 */
		return SOCKET_CLOSED;
	}

	if( ! CHECK_HTTP_FLAG(&con->http, HRF_RES_HEADER_SENT) ) {

		ret = http_send_response_header(con);

		/* Set TOS if set by policy engine or NS config
		 * Should this be done only for 200 or 206 ?
		 */
		if ((nsc->http_config->response_policies.dscp >= 0) || 
		    CHECK_HTTP_FLAG(&con->http, HRF_PE_SET_IP_TOS)) {
			if (!CHECK_HTTP_FLAG(&con->http, HRF_PE_SET_IP_TOS)) {
				con->http.tos_opt = nsc->http_config->response_policies.dscp << 2;
			}

			if (set_ip_policies(con->fd, &con->ip_tos, con->http.tos_opt, con->src_ipv4v6.family)) {
				DBG_LOG(MSG, MOD_HTTP, 
					"IP TOS setting failed");
			} 
			CHECK_HTTP_FLAG(&con->http, HRF_PE_SET_IP_TOS) ? CLEAR_HTTP_FLAG(&con->http, HRF_PE_SET_IP_TOS):0;
		} else {
			//Restore back the old IP TOS setting if needed.
			if (con->ip_tos >= 0) {
				setsockopt(con->fd, SOL_IP, IP_TOS, &con->ip_tos,
					sizeof(con->ip_tos));
				DBG_LOG(MSG, MOD_HTTP,
					"Restored old IP TOS settings for client socket=%d, "
					"tos=%x and con=%p",
					con->fd, con->ip_tos, con);
				AO_fetch_and_add1(&glob_reset_dscp_responses);
			}
		}

		// Send out HTTP response header out and close the socket
		if (ret == SOCKET_CLOSED) {
			ret = send(con->fd, con->http.resp_buf, con->http.res_hdlen, 0);
			if (ret == -1) {
				// should never happen.
				// How can an empty socket cannot send a http response header?
				DBG_LOG(MSG, MOD_HTTP, "send error errno=%d", errno);
			}
			else {
 				AO_fetch_and_add(&glob_client_send_tot_bytes, ret);

				/*For NFS origin, for tunnel case
				* header size is not accounted previously.*/
				if (con->http.nfs_tunnel)
				    AO_fetch_and_add(&glob_tot_size_from_tunnel, ret);
			}
			return SOCKET_CLOSED;
		}


		if ( (con->http.content_length + con->http.res_hdlen >= MAX_ONE_PACKET_SIZE) ||
		     (CHECK_HTTP_FLAG(&con->http, HRF_TRANSCODE_CHUNKED)) ||
		     (CHECK_HTTP_FLAG(&con->http, HRF_SUPPRESS_SEND_DATA)) ) {
			// If content_length is too big, send out data now.
			ret = send(con->fd, con->http.resp_buf, con->http.res_hdlen, 0);
			if (ret == -1) {
				// should never happen.
				// How can an empty socket cannot send a http response header?
				DBG_LOG(MSG, MOD_HTTP, "send error errno=%d", errno);
				return SOCKET_CLOSED;
			}
			
			/*For NFS origin, for tunnel case
			* header size is not accounted previously.*/
			if (con->http.nfs_tunnel)
			    AO_fetch_and_add(&glob_tot_size_from_tunnel, ret);

			if (!CHECK_HTTP_FLAG(&con->http, HRF_MFC_PROBE_REQ)) {
				glob_tot_bytes_sent += ret;
				AO_fetch_and_add(&glob_client_send_tot_bytes, ret);
				update_namespace_stats(con, ret);
			}
			// Mark HTTP header has been sent out
			SET_HTTP_FLAG(&con->http, HRF_RES_HEADER_SENT);
		}
		else {
			// Optimization for content_length is small, 
			// delay to send this HTTP header out.
			SET_HTTP_FLAG(&con->http, HRF_ONE_PACKET_SENT);
		}

		/* By this time, we should either have sent a response
		 * for a request, or have build and are optimizing it
		 * to send it (when content_length is small)
		 */
		NS_INCR_STATS((con->http.nsconf), PROTO_HTTP, client, responses);
		if ((con->http.provider != OriginMgr_provider)) {
			if (!CHECK_HTTP_FLAG(&con->http, HRF_MFC_PROBE_REQ)) {
				glob_http_cache_hit_cnt++;
				NS_INCR_STATS(con->http.nsconf, PROTO_HTTP, client, cache_hits);
			}
		} else {
			AO_fetch_and_add1(&glob_http_cache_miss_cnt);
			NS_INCR_STATS(con->http.nsconf, PROTO_HTTP, client, cache_miss);
		}
	}

	/*
	 * The logic to calculate con->max_send_size is
	 * 1. if fast start is configured, we will do fast start logic.
	 * 2. otherwise calculate based on min_afr.
	 * 3. all result should not exceed MBR.
	 */
	if(con->max_send_size == 0) {
		uint64_t mbr_size;

		update_nkn_cur_ts = 1;
		// Mark the time of this max_send_size calculation
		con->nkn_cur_ts = nkn_cur_ts;

		/*
		 * When faststart buffer is configured in SSP or network.
		 */
		if(con->max_faststart_buf) {
			/* if fast start is configured, we set the fast start here. */
			if(con->max_client_bw) {
				mbr_size = con->max_client_bw/nkn_timer_interval;
				con->max_send_size = (con->max_faststart_buf > mbr_size) ?
							mbr_size : con->max_faststart_buf;
			}
			else {
				con->max_send_size = con->max_faststart_buf;
			}

			if (sess_bandwidth_limit) {
				con->bandwidth_send_size = con->max_send_size;
				goto out;
			}
			// When session max bandwidth is not configured.
			goto done_with_max_send_size_cal;
		}

		if( con->min_afr ) {
			/*
			 * Then when min_afr is configured in SSP or network,
			 * calculate con->max_send_size based on min_afr.
			 */
                        con->max_send_size = con->min_afr * 1.2 / nkn_timer_interval;
			con->bandwidth_send_size = con->max_send_size;
                }
		else  {
			/*
		  	 * If neither faststart nor AFR is configured in SSP and network,
		  	 * calculate con->max_send_size based current tot_sessions.
		 	 */
			if (con->pns->tot_sessions == 0) {
				/* BUG 993: unsolved issue */
				DBG_LOG(ERROR, MOD_HTTP, "hit bug 993");
				glob_socket_bug993 ++;
				return SOCKET_CLOSED;
			}
			con->bandwidth_send_size = (con->pns->if_bandwidth)/
					(nkn_timer_interval);// * con->pns->tot_sessions);
			con->max_send_size = (con->pns->if_bandwidth + con->pns->if_credit)/
					(nkn_timer_interval);// * con->pns->tot_sessions);
		}

out:
		/* 
		 * No matter what calculation it is, we already limit it by max_bandwidth.
		 * Limit session bandwidth only when it is configured.
		 */
		if(con->max_bandwidth) {
			mbr_size = con->max_bandwidth/nkn_timer_interval;
			con->max_send_size = (con->max_send_size > mbr_size) ?
				mbr_size : con->max_send_size;
		}

done_with_max_send_size_cal:
		/*
		 * If MBR is not configured in network level
		 * We skip about statement because con->max_bandwidth is always set to 1.2 * con->min_afr.
		 */
		if(con->max_faststart_buf) {
			if( con->max_faststart_buf > con->max_send_size ) {
				con->max_faststart_buf -= con->max_send_size;
			} else {
				con->max_faststart_buf = 0;
			}
		}
		if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
			DBG_TRACE("AFR: time=%s size=%ld", nkn_get_datestr(NULL), con->max_send_size);
		}
		DBG_LOG(MSG, MOD_NETWORK, "AFR: time=%s size=%ld", nkn_get_datestr(NULL), con->max_send_size);

	}
	
	if( CHECK_HTTP_FLAG(&con->http, HRF_METHOD_HEAD) &&
	    con->http.num_nkn_iovecs ==0 &&
	    con->http.content_length) {
		//HRF_SUPPRESS_SEND_DATA flag will be send when sending the response header
		ret = http_send_data(con, 0, 0);
		goto TransactionDone;
	}
		
	if (nkn_resource_pool_enable) {
		uint64_t rp_available;
		rp_available = nvsd_rp_bw_query_available_bw(con->http.nsconf->rp_index);
		if(rp_available != (uint64_t)-1) {
			con->max_send_size = (uint64_t)((con->max_send_size > rp_available) ?
				rp_available : con->max_send_size);
		}
	}


        for(i=0;i<con->http.num_nkn_iovecs;i++) {

                len=con->http.content[i].iov_len;
                if(len==0) continue;
                p=con->http.content[i].iov_base;

		len = ((uint64_t)len>con->max_send_size) ? con->max_send_size :(uint64_t) len;
		if (update_nkn_cur_ts) con->nkn_cur_ts = nkn_cur_ts;

		if (CHECK_HTTP_FLAG(&con->http, HRF_ONE_PACKET_SENT)) {
			int chunked = 0;
			int update_stats = 1;
			cplen = MAX_ONE_PACKET_SIZE - con->http.res_hdlen;
			cplen = (cplen>len) ? len : cplen;
			if (CHECK_HTTP_FLAG(&con->http, HRF_SUPPRESS_SEND_DATA) ||
			    (chunked = CHECK_HTTP_FLAG(&con->http, HRF_TRANSCODE_CHUNKED))) {
				if (chunked == 0) {
					ret = con->http.res_hdlen + cplen;
					update_stats = 0;
				} else {
					ret = http_send_data_chuck_header(con, con->http.res_hdlen + cplen);
					if (ret >= 0) {
						iov[0].iov_base = con->http.resp_buf;
						iov[0].iov_len = con->http.res_hdlen;
						iov[1].iov_base = p;
						iov[1].iov_len = cplen;
						ret = writev(con->fd, iov, 2);
					}
				}
			} else {
				iov[0].iov_base = con->http.resp_buf;
				iov[0].iov_len = con->http.res_hdlen;
				iov[1].iov_base = p;
				iov[1].iov_len = cplen;
				ret = writev(con->fd, iov, 2);
			}
			if(ret < con->http.res_hdlen) {
				// Even HTTP response header has not been sent out
				return SOCKET_CLOSED;
			}

                       /*
                        * If the log format has %{X-NKN-MD5-Checksum}o & the MD5 checksum
                        * string length configured, calculate md5 chksum for 'len' characters
                        * and update the md5-context in http connection 'con'.
                        */
                        if (con->http.nsconf &&
                            con->http.nsconf->acclog_config->al_resp_hdr_md5_chksum_configured &&
                            con->http.nsconf->md5_checksum_len &&
                            con->md5 &&
                            (con->http.respcode == 200 || con->http.respcode == 206)) {
                
                            // Add the HTTP body string length to md5_in_strlen to get the total.
                            int md5_cur_strlen = (ret - con->http.res_hdlen) ;
                            int md5_total_len = con->md5->in_strlen + md5_cur_strlen ;
                
                            // Update the MD5 context with 'len' response data string.
                            if(md5_total_len > con->http.nsconf->md5_checksum_len) {
                                md5_cur_strlen = con->http.nsconf->md5_checksum_len - con->md5->in_strlen ;
                            }
                
                            if (md5_cur_strlen > 0) {
                                con->md5->temp_seq ++ ;
                
                                // Do MD5 update only when the data length is less than the
                                // configured number of bytes.
                                con->md5->in_strlen += md5_cur_strlen ;
                
                                MD5_Update(&con->md5->ctxt, p, md5_cur_strlen);
                                DBG_LOG(MSG, MOD_HTTP,  "MD5_CHECKSUM(Update.2[%d]):[current strLen:%d], con=%p fd=%d r=%d, s=%d, l=%ld",
                                        con->md5->temp_seq,
                                        md5_cur_strlen, con, con->fd,
                                        con->http.respcode, con->http.subcode,
                                        con->http.content_length);
                            }
                        }

			if (update_stats) {
				SET_CON_FLAG(con, CONF_SEND_DATA);
				glob_tot_bytes_sent += ret;
				AO_fetch_and_add(&glob_client_send_tot_bytes, ret);
				update_namespace_stats(con, ret);
			}
			// Mark HTTP header has been sent out
			SET_HTTP_FLAG(&con->http, HRF_RES_HEADER_SENT);

			/*For NFS origin, for tunnel case
			* header size is not accounted previously.*/
			if (con->http.nfs_tunnel)
			    AO_fetch_and_add(&glob_tot_size_from_tunnel, con->http.res_hdlen);

			ret -= con->http.res_hdlen; // Only sent out so much data
			CLEAR_HTTP_FLAG(&con->http, HRF_ONE_PACKET_SENT);
		}
		else {
                	ret = http_send_data(con, p, len);
		}
                if (ret == -1) {
                        return SOCKET_WAIT_EPOLLOUT_EVENT;
                } else if (ret == -2) {
			glob_socket_err_sent ++;
                        return SOCKET_CLOSED;
                }

		AO_fetch_and_add(&glob_tot_bytes_tobesent, -ret);

                con->http.content[i].iov_base += ret;
                con->http.content[i].iov_len -= ret;

		con->max_send_size -= ret;
                con->sent_len += ret;

		/*
		 * The following logic is used to support credit algorithm.
		 * In AFR calculation, we allow to send out so much data.
		 * if an interface bandwidth allows sent out more data,
		 * we should not waste those bandwidth.
		 * the difference (if_bandwidth - if_totbytes_sent) is called credit.
		 * in next second, we can use this extra credit for active
		 * sessions to send out extra more data.
		 * but we should not count extra more data twice.
		 * Otherwise we will see oscillation in credit.
		 */
		if(con->bandwidth_send_size > 0) {
			long bandwidth_size;
			bandwidth_size = ((long)con->bandwidth_send_size > ret) ?
					ret : (long)con->bandwidth_send_size;
			con->bandwidth_send_size -= bandwidth_size;
			con->pns->if_totbytes_sent += bandwidth_size;

			nvsd_rp_bw_update_send(con->http.nsconf->rp_index, bandwidth_size);
		}

		if( CHECK_HTTP_FLAG(&con->http, HRF_BYTE_RANGE) ) {
                	if (con->sent_len == 
				con->http.brstop - con->http.brstart + 1) {
				goto TransactionDone;
			}
		} else if( CHECK_HTTP_FLAG(&con->http, HRF_BYTE_SEEK) ) {
                	if (con->sent_len == 
			    ((con->http.seekstop - con->http.seekstart + 1) +
			     con->http.prepend_content_size + 
			     con->http.append_content_size)) {
				goto TransactionDone;
			}
                } else if ((CHECK_HTTP_FLAG(&con->http, HRF_PREPEND_IOV_DATA) || 
			    CHECK_HTTP_FLAG(&con->http, HRF_APPEND_IOV_DATA)) && 
			   con->sent_len == (con->http.content_length + 
			   CHECK_HTTP_FLAG(&con->http,HRF_PREPEND_IOV_DATA) ? con->http.prepend_content_size : 0 + 
                           CHECK_HTTP_FLAG(&con->http,HRF_APPEND_IOV_DATA) ? con->http.append_content_size : 0)) {
		    
		                goto TransactionDone;

                } else if (CHECK_HTTP_FLAG(&con->http, 
					(HRF_METHOD_HEAD|HRF_NO_CACHE))) {
				// non-cacheable no content length HEAD request
		                goto TransactionDone;
		} else {
		    
                	if (con->sent_len >= con->http.content_length) {
				goto TransactionDone;
			}
		}

                /*
		 * The reason why we put this con into timer queue is because:
		 * for con-current 20K connections, if we didn't limit some connection's throughput,
		 * some other connections will eventually time out
		 * because the total bandwidth is limited to 1Gbytes per network port.
		 *
		 * In this math, assuming server_timer is in sec unit
		 */
                if (!CHECK_HTTP_FLAG(&con->http, HRF_SUPPRESS_SEND_DATA) &&
		    (con->max_send_size==0)) {
			if (con->nkn_cur_ts == nkn_cur_ts) {
                        	// Wait for next second to send out more data 
				if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
					DBG_TRACE("AFR: time=%s size=%ld", nkn_get_datestr(NULL), con->max_send_size);
				}
                        	return SOCKET_TIMER_EVENT;
			}
			// We cannot send out the whole max_send_size data within this second.
			// Update the counter
			glob_afr_miss_due_network++;
			// return here to calculate con->max_send_size again.
                        return SOCKET_WAIT_EPOLLOUT_EVENT;
                }	

		/* This code is left over here. Not sure why? */
		if(con->http.content[i].iov_len) {
                         return SOCKET_WAIT_EPOLLOUT_EVENT;
		}

		/* NFS Tunnel does not use BM buffers, no need to deref the
		 * buffers. The buffers will be freed during cleanup
		 */
		if(!con->http.nfs_tunnel) {
			/* 
			 * We are done with this iovec.  Mark it done with BM so that
			 * the underlying buffer can be released.  Otherwise, we 
			 * hold on to all the buffers for the duration it takes
			 * to send them all.
			 */
			if (CHECK_HTTP_FLAG(&con->http, HRF_APPEND_IOV_DATA) &&
				(i == (con->http.num_nkn_iovecs-1))) {
				// Not a buffer from cache
			} else if (CHECK_HTTP_FLAG(&con->http, HRF_PREPEND_IOV_DATA)) {
				if (i > 0) {
					nkn_cr_content_done(con->nkn_taskid, i-1);
				}
			} else {
				nkn_cr_content_done(con->nkn_taskid, i);
			}
		}
        }

        // We have sent out all available data so far
        // We need to ask CM for more data
	reset_con_for_receive(con);
	if (CHECK_HTTP_FLAG(&con->http, HRF_SUPPRESS_SEND_DATA)) {
		goto TransactionDone;
	}
        return SOCKET_WAIT_BUFFER_DATA;

TransactionDone:
 	if (CHECK_HTTP_FLAG(&con->http, HRF_MULTIPART_RESPONSE)) {
		ret = requestdoneSSP(con);
		if (ret) {
			// Initiate another request
			delete_all_namevalue_headers(&con->http.hdr);
			reset_con_for_receive(con);
			con->sent_len = 0;
			con->http.content_length = 0;
			con->http.num_nkn_iovecs = 0;
			return SOCKET_WAIT_CM_DATA;
		}
		CLEAR_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE);//Fix to close conn on completion of sending data
	}

	if (!(CHECK_HTTP_FLAG(&con->http, HRF_MFC_PROBE_REQ))) {
		AO_fetch_and_add1(&glob_http_tot_well_finished);
	}

	//
	// Should we keep the connection alive?
	// If yes, we only phttpet the http control block
	// Otherwise, we will close the connection
	//
	if( CHECK_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE) ) {
		nkn_cleanup_a_task(con);
		ret = reset_http_cb(con);
		if (ret == TRUE)
	        	return SOCKET_WAIT_EPOLLIN_EVENT;
		else
			return SOCKET_CLOSED_DONE_FALSE;
	}
	else {
		// Time to close the socket
        	return SOCKET_CLOSED;
	}
}

int http_set_seek_request(http_cb_t *cb, off_t seek_start, off_t seek_end) {
	assert(!CHECK_HTTP_FLAG(cb, HRF_BYTE_SEEK));
	if (CHECK_HTTP_FLAG(cb, HRF_BYTE_RANGE)) {
		// Change byte range to seek
		CLEAR_HTTP_FLAG(cb, HRF_BYTE_RANGE);
		cb->brstart = 0;
		cb->brstop = 0;
	}
	SET_HTTP_FLAG(cb, HRF_BYTE_SEEK);
	cb->seekstart = seek_start;
	cb->seekstop = seek_end;

	return 0;
}

int http_set_no_cache_request(http_cb_t *cb) {
	SET_HTTP_FLAG(cb, HRF_NO_CACHE);
	return 0;
}

int http_set_revalidate_cache_request(http_cb_t *cb) {
	SET_HTTP_FLAG(cb, HRF_REVALIDATE_CACHE);
	return 0;
}

int http_prepend_response_data(http_cb_t *cb, const char *data, int datalen,
                        int free_malloced_data)
{
	nkn_iovec_t content[CONTENT_IOVECS];

	if (!data || !datalen) {
		return 1; // Invalid data
	}
   	if (cb->prepend_content_size) {
		return 2; // Already exists
	}
	memcpy((void *)&content[0], &cb->content[0], 
		cb->num_nkn_iovecs * sizeof(nkn_iovec_t));
	cb->content[0].iov_base = (char *)data;
	cb->content[0].iov_len = datalen;
	memcpy((void *)&cb->content[1], (void *)&content[0],
		cb->num_nkn_iovecs * sizeof(nkn_iovec_t));
	cb->num_nkn_iovecs++;
	cb->prepend_content_data = free_malloced_data ? (char *)data : 0;
	cb->prepend_content_size = datalen;
	SET_HTTP_FLAG(cb, HRF_PREPEND_IOV_DATA);
	return 0;
}

int http_append_response_data(http_cb_t *cb, const char *data, int datalen,
                        int free_malloced_data)
{
	// Not allowed on byte range requests
	assert(!CHECK_HTTP_FLAG(cb, HRF_BYTE_RANGE));

	// Only allowed on seek requests
	assert(CHECK_HTTP_FLAG(cb, HRF_BYTE_SEEK));

	if (!data || !datalen) {
		return 1; // Invalid data
	}
   	if (cb->append_content_size) {
		return 2; // Already exists
	}
	cb->content[cb->num_nkn_iovecs].iov_base = (char *)data;
	cb->content[cb->num_nkn_iovecs].iov_len = datalen;
	cb->num_nkn_iovecs++;
	cb->append_content_data = free_malloced_data ? (char *)data : 0;
	cb->append_content_size = datalen;
	SET_HTTP_FLAG(cb, HRF_APPEND_IOV_DATA);
	return 0;
}

int http_set_multipart_response(http_cb_t *cb, off_t pseudo_content_length)
{
	// Only allow one call
	assert(!CHECK_HTTP_FLAG(cb, HRF_MULTIPART_RESPONSE));

	SET_HTTP_FLAG(cb, HRF_MULTIPART_RESPONSE);
	cb->pseudo_content_length = pseudo_content_length;
	return 0;
}

int http_set_local_get_request(http_cb_t *cb) {
    SET_HTTP_FLAG(cb, HRF_LOCAL_GET_REQUEST);
    return 0;
}


////////////////////////////////////////////////////////////////////////
// Socket support function
////////////////////////////////////////////////////////////////////////

/*
 * This function should be called at the end of HTTP transaction.
 *
 * The difference between http_close_conn() and close_conn() is
 * http_close_conn() keeps the connection alive while wiping out
 * all HTTP control block data.
 * close_conn() wipes out all data including socket data and HTTP
 * control block. Socket is also closed.
 */
void http_close_conn(con_t * con, int sub_errorcode)
{
	if (con->magic != CON_MAGIC_USED) return;
	DBG_LOG(MSG, MOD_HTTP, "fd=%d", con->fd);

	if (NM_CHECK_FLAG(&gnm[con->fd], NMF_RST_WAIT)) {
		return;
	}

	if (CHECK_HTTP_FLAG(&con->http, HRF_CL_L7_PROXY_REQUEST)) {
		L7_SWT_PARENT_NS(&con->http);
	}

	if( !CHECK_HTTP_FLAG(&con->http, HRF_RES_HEADER_SENT) ) {
	    if (sub_errorcode == NKN_FP_NOT_HTTP_ORIGIN) {
		http_build_res_501(&con->http, sub_errorcode ? sub_errorcode : NKN_HTTP_CLOSE_CONN);
		DBG_LOG(MSG, MOD_HTTP, "Client connection closed with 501 ,fd %d errorcode %d", con->fd,
		             sub_errorcode ? sub_errorcode : NKN_HTTP_CLOSE_CONN);
		if (!sub_errorcode || sub_errorcode == NKN_HTTP_CLOSE_CONN) {
			glob_http_501_52016_resp++;
	     		//print_trace();
		}

	    } else if (sub_errorcode == NKN_SSP_HASHCHECK_FAIL || sub_errorcode == NKN_SSP_HASHEXPIRY_FAILED) {
		http_build_res_403(&con->http, sub_errorcode ? sub_errorcode : NKN_HTTP_CLOSE_CONN);
		DBG_LOG(MSG, MOD_HTTP, "Client connection closed with 403 ,fd %d errorcode %d", con->fd,
				sub_errorcode ? sub_errorcode : NKN_HTTP_CLOSE_CONN);
	    } else {
		http_build_res_404(&con->http, sub_errorcode ? sub_errorcode : NKN_HTTP_CLOSE_CONN);
		DBG_LOG(MSG, MOD_HTTP, "Client connection closed with 404 ,fd %d errorcode %d", con->fd,
				sub_errorcode ? sub_errorcode : NKN_HTTP_CLOSE_CONN);
		if (!sub_errorcode || sub_errorcode == NKN_HTTP_CLOSE_CONN) {
			glob_http_404_52016_resp++;
			//print_trace();
		}
	    }
	}

        if( CHECK_HTTP_FLAG(&con->http, HRF_BYTE_RANGE) ) {
                if((con->http.brstop - con->http.brstart + 1) != 
			con->sent_len) {
			glob_http_err_len_not_match ++;
		}
        } else if( CHECK_HTTP_FLAG(&con->http, HRF_BYTE_SEEK) ) {
                if(((con->http.seekstop - con->http.seekstart + 1) +
		     con->http.prepend_content_size + 
		     con->http.append_content_size) != con->sent_len) {
			glob_http_err_len_not_match ++;
		}
        } else {
                if(con->http.content_length != con->sent_len)
			glob_http_err_len_not_match ++;
        }

        if(con->http.res_hdlen && !CHECK_HTTP_FLAG(&con->http, HRF_RES_HEADER_SENT)) {
                // It does not matter for return value at this case
		CLEAR_HTTP_FLAG(&con->http, HRF_TRANSCODE_CHUNKED);
                http_send_data(con, con->http.resp_buf, con->http.res_hdlen);

		if (con->http.resp_body_buf) { // inline 200 response
		    http_send_data(con, (char *)con->http.resp_body_buf,
				   con->http.resp_body_buflen < 0 ?
				       -con->http.resp_body_buflen :
				       con->http.resp_body_buflen);
		}
	}

	if( CHECK_HTTP_FLAG(&con->http, HRF_TRANSCODE_CHUNKED) ) {
		http_send_data_chuck_header(con, 0);
	}

	if (CHECK_HTTP_FLAG(&con->http, HRF_RP_SESSION_ALLOCATED) && con->http.nsconf != NULL) {
		nvsd_rp_free_resource(RESOURCE_POOL_TYPE_CLIENT_SESSION, con->http.nsconf->rp_index, 1);
		AO_fetch_and_sub1((volatile AO_t *) &(con->http.nsconf->stat->g_http_sessions));
		NS_DECR_STATS(con->http.nsconf, PROTO_HTTP, client, active_conns);
		NS_DECR_STATS(con->http.nsconf, PROTO_HTTP, client, conns);
		CLEAR_HTTP_FLAG(&con->http, HRF_RP_SESSION_ALLOCATED);
	}

	if( CHECK_HTTP_FLAG(&con->http, HRF_CONNECTION_KEEP_ALIVE) &&
	    (sub_errorcode == 0) ) {
		/*
		 * we only support pipeline requests when it is Keep-Alive and no internal error.
		 */
		reset_http_cb(con);
		return;
	}

        close_conn(con);
}

/*
 *  apply_CL7_intercept_policies() - Apply Cluster L7 action override 
 *	policies using suffix map followed by the Policy Engine.
 */
static intercept_type_t apply_CL7_intercept_policies(con_t *con, 
						cluster_descriptor_t *cldesc,
			     			intercept_type_t current_action)
{
	int tracelog;
	const char *trc_host_str;
	const char *trc_uri_str;
	const namespace_config_t *nsc = con->http.nsconf;

	/* Trace log support */
	if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
		int rv;
		const char *thost = 0;
		int thostlen;
		const char *uri = 0;
		int urilen;
		u_int32_t attrs;
		int hdrcnt;

		rv = get_known_header(&con->http.hdr, MIME_HDR_HOST, 
			&thost, &thostlen,
			&attrs, &hdrcnt);
		if (!rv) {
			trc_host_str = alloca(thostlen+1);
			memcpy((char *)trc_host_str, thost, thostlen);
			((char *)trc_host_str)[thostlen] = '\0';
		}
		rv = get_known_header(&con->http.hdr, MIME_HDR_X_NKN_URI, &uri, 
			&urilen, &attrs, &hdrcnt);
		if (!rv) {
			trc_uri_str = alloca(urilen+1);
			memcpy((char *)trc_uri_str, uri, urilen);
			((char *)trc_uri_str)[urilen] = '\0';
		}
		tracelog = 1;
	} else {
		trc_host_str = 0;
		trc_uri_str = 0;
		tracelog = 0;
	}

	/*
	 ***********************************************************************
	 * Apply CL7 Suffix Map
	 ***********************************************************************
	 */
	if (cldesc && cldesc->suffix_map) {
		int rv;
		const char *uri = 0;
		int urilen;
		u_int32_t attrs;
		int hdrcnt;
		char *suffix;
		suffix_map_trie_node_t *tnd;

		rv = get_known_header(&con->http.hdr, MIME_HDR_X_NKN_URI,
				&uri, &urilen, &attrs, &hdrcnt);
		if (!rv) {
		    	suffix = alloca(urilen+1);
		    	rv = extract_suffix(uri, urilen, suffix, 
					    urilen+1, '.', 1);
		    	if (!rv) {
				tnd = (suffix_map_trie_node_t *)
			    		nkn_trie_exact_match(cldesc->suffix_map,
							    suffix);
				if (tnd) {
			    		current_action = tnd->type;
			    		if (tracelog) {
					    DBG_TRACE(
					    "CL7 Suffix map action: "
					    "con=%p fd=%d URL %s%s => L7 %s", 
					    con, con->fd, 
					    trc_host_str, trc_uri_str,
					    (current_action == 
					    	INTERCEPT_REDIRECT) ?
					    	"Redirect" : "Proxy");
			    		}
				}
		    	}
		}
	}

	/***********************************************************************
	 * Apply CL7 Policy Engine
	 ***********************************************************************
	 */
	pe_ilist_t *p_list = NULL;
	pe_rules_t *p_perule;
	
	p_perule = pe_create_rule(&nsc->http_config->policy_engine_config, 
				  &p_list);
	if (p_perule) {
		uint64_t action;
		
		action = pe_eval_cl_http_recv_request(p_perule, con, 
					(current_action == INTERCEPT_PROXY));
		pe_free_rule(p_perule, p_list);

		if (action & PE_ACTION_CLUSTER_REDIRECT) {
			if (tracelog) {
				DBG_TRACE("CL7 PE action: con=%p fd=%d "
					"URL %s%s => L7 Redirect", 
					con, con->fd,
					trc_host_str, trc_uri_str);
			}
			current_action = INTERCEPT_REDIRECT;
		} else if (action & PE_ACTION_CLUSTER_PROXY) {
			if (tracelog) {
				DBG_TRACE("CL7 PE action: con=%p fd=%d "
					"URL %s%s => L7 Proxy", 
					con, con->fd,
					trc_host_str, trc_uri_str);
			}
			current_action = INTERCEPT_PROXY;
		}
	}

	if (tracelog) {
		DBG_TRACE("CL7 con=%p fd=%d URL %s%s => L7 %s",
			con, con->fd,
			trc_host_str, trc_uri_str, 
			current_action == INTERCEPT_REDIRECT ? 
				"Redirect" : "Proxy");
	}
	
	if (!mime_hdr_is_known_present(&con->http.hdr, 
					MIME_HDR_X_NKN_CL7_STATUS)) {
		const char *val;
		switch(current_action) {
		case INTERCEPT_REDIRECT:
			val = "Mode=Redirect";
			break;
		case INTERCEPT_PROXY:
			val = "Mode=Proxy";
			break;
		default:
			val = "Mode=?";
		}
		add_known_header(&con->http.hdr, 
				MIME_HDR_X_NKN_CL7_STATUS, 
				val, strlen(val));
	}
	return current_action;
}

/*
 * set_ip_policies() - Apply IP_TOS settings as configured through NS or PE.
 * IN:  sock_fd    : socket fd
 *      new_tos    : New IP TOS setting
 *      family     : AF_INET/AF_INET6 - socket address family.
 * IN/OUT: old_tos : input is tos init value and if so ouput is back up of old tos.
 * returns 0: success
 *	   1: Error
 */
int set_ip_policies(int sock_fd, int32_t *old_tos, int new_tos, uint8_t family) {
	int ret = 0;
	int tos = 0, tos_len;

	switch (*old_tos) {
	case -1: // In this persistent connection, this is the first request for IP TOS setting.
		{
			/* Backup the existing client socket DSCP setting into con */
			tos_len = sizeof(tos);

			if (AF_INET == family) {
				if (getsockopt(sock_fd, SOL_IP, IP_TOS, &tos, (socklen_t *)&tos_len) < 0) {
					DBG_ERR(MSG, "getsockopt() failed to retrieve IP_TOS setting for "
						"socket fd=%d, family=Ipv4",
						sock_fd);
					ret = 1;
					break;
				} else {
					DBG_LOG(MSG, MOD_HTTP, "Retrieved IP_TOS settings for socket fd=%d" 
						"tos=%x, family=Ipv4",
						sock_fd, tos);
				}
			} else {
				if (getsockopt(sock_fd, IPPROTO_IPV6, IPV6_TCLASS, &tos, 
						(socklen_t *)&tos_len) < 0) {
					DBG_ERR(MSG, "getsockopt() failed to retrieve IP_TOS setting for "
						"socket fd=%d, family=IPv6",
						sock_fd);
					ret = 1;
					break;
				} else {
					DBG_LOG(MSG, MOD_HTTP, "Retrieved IP_TOS settings for socket fd=%d"
						"tos=%x, family=IPv6", 
						sock_fd, tos);
				}
			}
			*old_tos = tos;
		}
	default: 
		// Fall through case. This is for first and/or followup requests over the same 
		// persistent connection.
		{
			if (AF_INET == family) {
				setsockopt(sock_fd, SOL_IP, IP_TOS, &new_tos,
					sizeof(new_tos));
			}
			else {
				setsockopt(sock_fd, IPPROTO_IPV6, IPV6_TCLASS, &new_tos,
					sizeof(new_tos));
			}
			DBG_LOG(MSG, MOD_HTTP, 
				"Setup IP_TOS settings for socket fd=%d, family=%s, "
				"old_tos=%d and new_tos=%d[TOS header:0x%x]",
				sock_fd, (family == AF_INET) ? "IPV4":"IPV6",
				*old_tos, new_tos, new_tos);
			AO_fetch_and_add1(&glob_set_dscp_responses);
			break;
		}
	}

	return ret;
}


int get_aws_authorization_hdr_value(con_t *con, char *hdr_value)
{
	int methodlen;
	int urilen;
	int hostlen;
	int datelen;
	unsigned int hmaclen;
	const char *method;
	const char *uri;
	const char *host;
	const char *date;
	u_int32_t attr;
	int hdrcnt;
	int rv;

	char *bucket_name;
	char *encoded_sign;
	char *p;
	char *signature;
	char final_uri[4096] = { 0 };
	unsigned char hmac[1024] = { 0 };
	char content_md5[4] = "";
	char content_type[4] = "";
	char cananionicalized_amz_hdrs[4] = "";
	char domain[1024];

	const char *aws_access_key;
	int aws_access_key_len;
	const char *aws_secret_key;
	int aws_secret_key_len;

	rv = get_known_header(&con->http.hdr, MIME_HDR_X_NKN_METHOD,
				&method, &methodlen, &attr, &hdrcnt);
	if (rv) {
		DBG_LOG(MSG, MOD_HTTP, "method is not found\n");
		return 1;
	}

	rv = get_known_header(&con->http.hdr, MIME_HDR_X_NKN_URI,
				&uri, &urilen, &attr, &hdrcnt);
	if (rv) {
		DBG_LOG(MSG, MOD_HTTP, "uri is not found\n");
		return 1;
	}

	rv = get_known_header(&con->http.hdr, MIME_HDR_DATE,
				&date, &datelen, &attr, &hdrcnt);
	if (rv) {
		DBG_LOG(MSG, MOD_HTTP, "date is not found\n");
		return 1;
	}

	if (con->http.nsconf->http_config->origin.select_method == OSS_HTTP_FOLLOW_HEADER) {
		rv = get_known_header(&con->http.hdr, MIME_HDR_HOST,
				 &host, &hostlen, &attr, &hdrcnt);
		if (rv) {
			DBG_LOG(MSG, MOD_HTTP, "host is not found\n");
			return 1;
		}
		memcpy(domain, host, hostlen);
		domain[hostlen] = '\0';
	} else if (con->http.nsconf->http_config->origin.select_method == OSS_HTTP_CONFIG) {
		memcpy(domain, con->http.nsconf->http_config->origin.u.http.server[0]->hostname,
			    con->http.nsconf->http_config->origin.u.http.server[0]->hostname_strlen);
		domain[con->http.nsconf->http_config->origin.u.http.server[0]->hostname_strlen] = '\0';
	} else {
		DBG_LOG(MSG, MOD_HTTP, "it not follow header host and it is not http host\n");
		return 1;
	}

	signature = hdr_value;
	aws_access_key = con->http.nsconf->http_config->origin.u.http.server[0]->aws.access_key;
	aws_access_key_len = con->http.nsconf->http_config->origin.u.http.server[0]->aws.access_key_len;
	aws_secret_key = con->http.nsconf->http_config->origin.u.http.server[0]->aws.secret_key;
	aws_secret_key_len = con->http.nsconf->http_config->origin.u.http.server[0]->aws.secret_key_len;

	bucket_name = strtok(domain, ".");
	strcat(final_uri, "/");
	strcat(final_uri, bucket_name);
	if (uri[1] == '?' && ((strncmp(uri+2, "prefix", 6) == 0) || (strncmp(uri+2, "delimiter", 9) == 0))) {
		strncat(final_uri, "/", 1);
	} else {
		strncat(final_uri, uri, urilen);
	}

	strncat(signature, method, methodlen);
	strcat(signature, "\n");

	strcat(signature, content_md5);
	strcat(signature, "\n");
	strcat(signature, content_type);
	strcat(signature, "\n");
	strncat(signature, date, datelen);
	strcat(signature, "\n");
	strcat(signature, cananionicalized_amz_hdrs);
	strcat(signature, final_uri);

	p = (char *)HMAC(EVP_sha1(), aws_secret_key, aws_secret_key_len,
		    (const unsigned char *)signature, strlen(signature), hmac, &hmaclen);
	if (hmaclen == 0) return 1;
	encoded_sign = g_base64_encode(hmac, hmaclen);
	if (encoded_sign == NULL) return 1;

	strcpy(signature, "AWS ");
	strncat(signature, aws_access_key, aws_access_key_len);
	strcat(signature, ":");
	strcat(signature, encoded_sign);
	g_free(encoded_sign);

	return 0;
}

int validate_auth_header(con_t *con)
{
	int ret;
	int datalen;
	const char *data;
	uint32_t attr;
	int header_cnt;
	char hdr_value[4096] = { 0 };
	int len;

	if (con->http.nsconf->http_config->origin.u.http.server[0]->aws.active) {
		ret = get_known_header(&con->http.hdr, MIME_HDR_AUTHORIZATION,
			    &data, &datalen, &attr, &header_cnt);
		if (ret) {
			DBG_LOG(MSG, MOD_HTTP, "Authorization header is not found\n")
			return NKN_HTTP_AUTHORIZATION_FAIL;
		}
		ret = get_aws_authorization_hdr_value(con, hdr_value);
		if (ret) {
			DBG_LOG(MSG, MOD_HTTP, "get_aws_authorization_hdr_value is failed\n")
			return NKN_HTTP_AUTHORIZATION_FAIL;
		}
		len = strlen(hdr_value);
		if ((len != datalen) || memcmp(hdr_value, data, datalen)) {
			DBG_LOG(MSG, MOD_HTTP, "mismatch in the aws authorization header value\n");
			glob_aws_auth_hdr_mismatch++;
			return NKN_HTTP_AUTHORIZATION_FAIL;
		}
	}

	return 0;
}

int validate_client_request(con_t *con)
{
	int n;
	const namespace_config_t *nsc;
	header_descriptor_t *hd;
	int ret;

	nsc = con->http.nsconf;
	for (n = 0; n <  nsc->http_config->request_policies.num_accept_headers; n++) {
		hd = &nsc->http_config->request_policies.accept_headers[n];
		if (hd && hd->name) {
			if (strcasecmp(hd->name, "Authorization") == 0) {
				ret = validate_auth_header(con);
				if (ret) return ret;
			}
		}
	}

	return 0;
}
