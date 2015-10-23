/*
 * nkn_pseudo_con.c
 *	Interfaces to create a psuedo con_t for use in OM requests
 *	which do _not_ originate from a client.
 *	The created object is passed in the 'in_proto_data' field of
 *	the MM_stat_resp_t and MM_get_resp_t structures.
 */
#include <sys/types.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "nkn_defs.h"
#include "nkn_pseudo_con.h"
#include "mime_header.h"
#include "nkn_debug.h"

static int32_t nkn_pseudo_fd = PSEUDO_FD_BASE;
static pthread_mutex_t pseudo_fd_mutex = PTHREAD_MUTEX_INITIALIZER;

int nkn_copy_orp_data(void *in_proto_data, void **out_proto_data);
void nkn_free_orp_data(void *proto_data);
extern char *get_ip_str(ip_addr_t *ip_ptr);

static int32_t nkn_get_pseudo_fd(void)
{
    int32_t fd;

    pthread_mutex_lock(&pseudo_fd_mutex);
    fd = nkn_pseudo_fd++;
    if (nkn_pseudo_fd < 0) {
	nkn_pseudo_fd = PSEUDO_FD_BASE;
    }
    pthread_mutex_unlock(&pseudo_fd_mutex);

    return fd;
}

static nkn_pseudo_con_t *int_nkn_alloc_pseudo_con_t(int size)
{
    nkn_pseudo_con_t *pcon;

    if (size && (size >= (int)sizeof(nkn_pseudo_con_t))) {
	pcon = nkn_calloc_type(1, size, mod_pcon_pcon_t);
    } else {
	pcon = nkn_calloc_type(1, sizeof(nkn_pseudo_con_t), mod_pcon_pcon_t);
    }

    if (pcon) {
	pcon->con.magic = CON_MAGIC_USED;
	pcon->con.fd = nkn_get_pseudo_fd();
	pcon->con.nkn_taskid = -1;
	pcon->con.http.flag = HRF_HTTP_11 | HRF_METHOD_GET;
	pcon->con.http.cb_totlen = 1;
	pcon->con.http.cb_max_buf_size = sizeof(pcon->dummy_buf);
	pcon->con.http.cb_buf = pcon->dummy_buf;
	pcon->con.pns = interface_find_by_dstip(inet_addr("127.0.0.1"));

	return pcon;
    } else {
	return NULL;
    }
}

nkn_pseudo_con_t *nkn_alloc_pseudo_con_t(void)
{
    return int_nkn_alloc_pseudo_con_t(0);
}

nkn_pseudo_con_t *nkn_alloc_pseudo_con_t_custom(int size)
{
    return int_nkn_alloc_pseudo_con_t(size);
}

int nkn_pseudo_con_t_init(nkn_pseudo_con_t *pcon, const nkn_attr_t *attr,
			  int in_method, char *uri, namespace_token_t token)
{
    int rv = 0;
    mime_header_t hdr;
    const char *data;
    int datalen;
    uint32_t hattr;
    int hdrcnt;
    const char *method;
    static const char *localhost = "localhost";

    mime_hdr_init(&hdr, MIME_PROT_HTTP, 0, 0);
    if (attr) {
	rv = nkn_attr2mime_header(attr, 0, &hdr);
	if (!rv) {
	    // Copy required context from attributes to pcon mime_header_t
	    rv = get_known_header(&hdr, MIME_HDR_X_NKN_CLIENT_HOST, &data, &datalen,
				  &hattr, &hdrcnt);
	    if (!rv) {
		add_known_header(&pcon->con.http.hdr, MIME_HDR_HOST, data, datalen);
	    } else {
		add_known_header(&pcon->con.http.hdr, MIME_HDR_HOST, localhost,
				 strlen(localhost));
	    }
	} else {
	    DBG_LOG(MSG, MOD_OM, "nkn_attr2mime_header() failed, rv=%d", rv);
	    rv = 1;
	    goto exit;
	}
    } else {
	/* This will be filled in MM */
    }

    if (in_method & HRF_METHOD_GET) {
	method = "GET";
    } else if (in_method & HRF_METHOD_POST) {
	method = "POST";
    } else if (in_method & HRF_METHOD_HEAD) {
	method = "HEAD";
    } else {
	DBG_LOG(MSG, MOD_OM, "No HTTP method");
	rv = 2;
	goto exit;
    }
    rv = add_known_header(&pcon->con.http.hdr, MIME_HDR_X_NKN_METHOD,
			    method, strlen(method));
    if (rv) {
	DBG_LOG(MSG, MOD_OM, "add_known_header() failed, rv = %d", rv);
	rv = 3;
	goto exit;
    }
    rv = add_known_header(&pcon->con.http.hdr, MIME_HDR_X_NKN_URI,
			    uri, strlen(uri));
    if (rv) {
	DBG_LOG(MSG, MOD_OM, "add_known_header() failed, rv = %d", rv);
	rv = 4;
	goto exit;
    }
    pcon->con.http.ns_token = token;
    pcon->con.http.nsconf = namespace_to_config(pcon->con.http.ns_token);
    if (pcon->con.http.nsconf == NULL) {
	DBG_LOG(MSG, MOD_OM, "namespace_to_config() failed");
	rv = 5;
	goto exit;
    }

    if (attr) {
	    pcon->con.http.cache_encoding_type = attr->encoding_type;
    } else {
	    pcon->con.http.cache_encoding_type = -1;
    }

exit:
    mime_hdr_shutdown(&hdr);
    return rv;
}

int nkn_copy_host_hdr(char *host_hdr,
			     void **out_host_hdr)
{
    int hostlen = strlen(host_hdr);

    *out_host_hdr = (void *)nkn_calloc_type(1, hostlen+1,
						mod_pseudo_con_host_hdr);
    if(*out_host_hdr) {
	memcpy(*out_host_hdr, host_hdr, hostlen);
	((char *)(*out_host_hdr))[hostlen] = '\0';
	return 0;
    }
    return -1;
}

/*
 * Copy the host header string to AM object data
 */
int nkn_copy_con_host_hdr(void *in_proto_data,
			     void **out_host_hdr)
{
    const char *host;
    int hostlen;
    u_int32_t attr;
    int header_cnt;
    int rv = 0;

    rv = get_known_header(&((nkn_pseudo_con_t *)in_proto_data)->con.http.hdr,
			    MIME_HDR_HOST, &host, &hostlen, &attr, &header_cnt);
    if (!rv) {
	*out_host_hdr = (void *)nkn_calloc_type(1, hostlen+1,
						mod_pseudo_con_host_hdr);
	if(*out_host_hdr) {
	    memcpy(*out_host_hdr, host, hostlen);
	    ((char *)(*out_host_hdr))[hostlen] = '\0';
	} else
	    rv = -1;
    }
    return rv;
}

/*
 * Free the host header string
 */
void nkn_free_con_host_hdr(void *out_host_hdr)
{
    if(out_host_hdr)
	free(out_host_hdr);
}

int nkn_copy_con_client_data(int nomodule_check, void *in_proto_data,
			     void **out_proto_data)
{
    nkn_pseudo_con_t *con_in_data, *con_out_data;
    if (!in_proto_data || !out_proto_data) {
        return -1;
    }
    con_in_data = (nkn_pseudo_con_t *)in_proto_data;
    if (nomodule_check) {
        *out_proto_data = (void *)nkn_calloc_type(1,
                         sizeof(nkn_pseudo_con_t),
                         mod_default_pcon_pcon_t);
        if(*out_proto_data != NULL) {
            con_out_data = (nkn_pseudo_con_t *)*out_proto_data;
	    mime_hdr_init(&(((nkn_pseudo_con_t *)*out_proto_data)->con.http.hdr),
		    MIME_PROT_HTTP, 0, 0);
            mime_hdr_copy_headers(&con_out_data->con.http.hdr,
                &con_in_data->con.http.hdr);
            con_out_data->con.module_type =
                con_in_data->con.module_type;
	    if (CHECK_HTTP_FLAG(&con_in_data->con.http, HRF_TPROXY_MODE)) {
		SET_HTTP_FLAG(&con_out_data->con.http, HRF_TPROXY_MODE);
	    }
	    return 0;
	}
	return -1;
    } else {
    	switch (con_in_data->con.module_type) {
            case SSP:
	    case NORMALIZER:
            	*out_proto_data = (void *)nkn_calloc_type(1,
				  sizeof(nkn_pseudo_con_t),
                                  mod_normalize_pcon_pcon_t);
            	if(*out_proto_data != NULL) {
                    con_out_data = (nkn_pseudo_con_t *)*out_proto_data;
		    mime_hdr_init(
			    &(((nkn_pseudo_con_t *)*out_proto_data)->con.http.hdr),
			    MIME_PROT_HTTP, 0, 0);
                    mime_hdr_copy_headers(&con_out_data->con.http.hdr,
                        &con_in_data->con.http.hdr);
		    con_out_data->con.module_type =
			con_in_data->con.module_type;
		    if (CHECK_HTTP_FLAG(&con_in_data->con.http, HRF_TPROXY_MODE)) {
			SET_HTTP_FLAG(&con_out_data->con.http, HRF_TPROXY_MODE);
		    }
            	}
            return 0;
        default:
            *out_proto_data = NULL;
            return 0;
    	}
    }	
    return -1;
}

void nkn_free_con_client_data(void *proto_data)
{
    if(proto_data) {
	mime_hdr_shutdown(&((nkn_pseudo_con_t *)proto_data)->con.http.hdr);
	free(proto_data);
    }
}

nkn_pseudo_con_t *nkn_create_pcon()
{
    nkn_pseudo_con_t *con_out_data;

    con_out_data = (nkn_pseudo_con_t *)nkn_calloc_type(1,
                         sizeof(nkn_pseudo_con_t),
                         mod_default_pcon_pcon_t);
    if(con_out_data)
	mime_hdr_init(&con_out_data->con.http.hdr,
			MIME_PROT_HTTP, 0, 0);
    return (void*)con_out_data;
}

void nkn_free_pseudo_con_t(nkn_pseudo_con_t *pcon)
{
    if (pcon) {
	if(pcon->con.http.nsconf)
	    release_namespace_token_t(pcon->con.http.ns_token);
	mime_hdr_shutdown(&pcon->con.http.hdr);
	free(pcon);
    }
}

void nkn_populate_encoding_hdr(uint16_t encoding_type, nkn_pseudo_con_t *pseudo_con)
{
    mime_header_t *hdr;
    uint16_t umask;
    char buf[4096];
    char *p, *pstart;

    hdr = &(pseudo_con->con.http.hdr);

    if (is_known_header_present(hdr, MIME_HDR_ACCEPT_ENCODING)) {
	return;
    }

    if (encoding_type == HRF_ENCODING_ALL) {
	add_known_header(hdr, MIME_HDR_ACCEPT_ENCODING, "*", 1);
	return;
    }

    p = pstart = buf;
    for (umask = 1; umask < HRF_ENCODING_UNKNOWN; umask <<= 1) {
	if (umask > encoding_type) break;
	if ((p - pstart) > 4080) break;
	switch (encoding_type & umask) {
	case HRF_ENCODING_GZIP:
	    p += snprintf(p, sizeof(p), "%s", "gzip, ");
	    break;
	case HRF_ENCODING_DEFLATE:
	    p += snprintf(p, sizeof(p), "%s", "deflate, ");
	    break;
	case HRF_ENCODING_COMPRESS:
	    p += snprintf(p, sizeof(p), "%s", "compress, ");
	    break;
	case HRF_ENCODING_EXI:
	    p += snprintf(p, sizeof(p), "%s", "exi, ");
	    break;
	case HRF_ENCODING_PACK200_GZIP:
	    p += snprintf(p, sizeof(p), "%s", "pack200_gzip, ");
	    break;
	case HRF_ENCODING_SDCH:
	    p += snprintf(p, sizeof(p), "%s", "sdch, ");
	    break;
	case HRF_ENCODING_BZIP2:
	    p += snprintf(p, sizeof(p), "%s", "bzip2, ");
	    break;
	case HRF_ENCODING_PEERDIST:
	    p += snprintf(p, sizeof(p), "%s", "peerdist, ");
	    break;
	default:
	    break;
	}
    }
    p = p - 2;
    add_known_header(hdr, MIME_HDR_ACCEPT_ENCODING, pstart, p - pstart);

    return;
}

void nkn_populate_proto_data(void **in_proto_data,
			nkn_client_data_t * out_client_data,
			am_object_data_t * in_client_data,
			char * uri,
			namespace_token_t token, nkn_provider_type_t src_ptype)
{
    char    *client_uri;
    char    *host_name = NULL;
    char    *port_num = NULL;
    char    server_addr[MAX_URI_SIZE];
    int	    ret;
    const char *data;
    int        datalen;
    uint32_t   hattr;
    int        hdrcnt;

    switch(src_ptype) {
	case RTSPMgr_provider:
	    out_client_data->proto = NKN_PROTO_RTSP;
	    //if(!*in_proto_data && in_client_data)
	    //    *in_proto_data = in_client_data->proto_data;
	    if(!*in_proto_data && in_client_data) {
		nkn_copy_orp_data(in_client_data->proto_data, in_proto_data);
	    }
	    break;
	case NFSMgr_provider:
	    out_client_data->proto = NKN_PROTO_HTTP;
	    *in_proto_data = NULL;
	    break;
	case OriginMgr_provider:
	default:
	    out_client_data->proto = NKN_PROTO_HTTP;
	    if(*in_proto_data && in_client_data) {
		memcpy(&out_client_data->ipv4v6_address, &in_client_data->client_ipv4v6,
			sizeof(ip_addr_t));
		out_client_data->port		= in_client_data->client_port;
		out_client_data->client_ip_family = in_client_data->client_ip_family;
		if(in_client_data->flags & AM_INGEST_BYTE_SEEK) {
		    SET_HTTP_FLAG(&((nkn_pseudo_con_t *)*in_proto_data)->con.http,
				    HRF_BYTE_SEEK);
		    ((nkn_pseudo_con_t *)*in_proto_data)->con.http.seekstop = 0;
		}
		break;
	    }
	    *in_proto_data = (void *)nkn_alloc_pseudo_con_t();
	    if(*in_proto_data == NULL) {
		break;
	    }
	    ret = mime_hdr_init(
			&(((nkn_pseudo_con_t *)*in_proto_data)->con.http.hdr),
			MIME_PROT_HTTP, 0, 0);
	    if(ret) {
		nkn_free_pseudo_con_t((nkn_pseudo_con_t *)*in_proto_data);
		break;
	    }
	    client_uri = ns_uri2uri(token, uri, 0);
	    ret = nkn_pseudo_con_t_init((nkn_pseudo_con_t *)*in_proto_data,
		    NULL, HRF_METHOD_GET, client_uri, token);
	    if(ret) {
		nkn_free_pseudo_con_t((nkn_pseudo_con_t *)*in_proto_data);
		break;
	    }

	    if(in_client_data->flags & AM_INGEST_BYTE_RANGE) {
		SET_HTTP_FLAG(&((nkn_pseudo_con_t *)*in_proto_data)->con.http,
				HRF_BYTE_RANGE);
		SET_HTTP_FLAG(&((nkn_pseudo_con_t *)*in_proto_data)->con.http,
				HRF_BYTE_RANGE_START_STOP);
		((nkn_pseudo_con_t *)*in_proto_data)->con.http.brstop = 1;
	    }

	    if(in_client_data->flags & AM_INGEST_BYTE_SEEK) {
		SET_HTTP_FLAG(&((nkn_pseudo_con_t *)*in_proto_data)->con.http,
				HRF_BYTE_SEEK);
		((nkn_pseudo_con_t *)*in_proto_data)->con.http.seekstop = 0;
	    }

	    /* Get the origin host name and port from the cache name
	    * and fill the http hdr in the MIME_HDR_HOST field. This needs
	    * to be set for the TProxy case, other wise we will not 
	    * know the origin server.
	    */
	    if(in_client_data->host_hdr) {
		    add_known_header(
			    &((nkn_pseudo_con_t *)*in_proto_data)->con.http.hdr,
			    MIME_HDR_HOST, in_client_data->host_hdr,
			    strlen((char *)in_client_data->host_hdr));
	    } else {
		host_name = get_part_from_cache_prefix(uri, CP_PART_SERVER,
						strlen(uri));
		if(host_name && host_name[0]) {
		    port_num = get_part_from_cache_prefix(uri, CP_PART_PORT,
						strlen(uri));
		    snprintf(server_addr, MAX_URI_SIZE, "%s:%d", host_name,
						atoi(port_num));
		    add_known_header(
			    &((nkn_pseudo_con_t *)*in_proto_data)->con.http.hdr,
			    MIME_HDR_HOST, server_addr, strlen(server_addr));
		    if(port_num)
			free(port_num);
		} else {
		    add_known_header(
			    &((nkn_pseudo_con_t *)*in_proto_data)->con.http.hdr,
			    MIME_HDR_HOST, "localhost", 9);
		}
	    }

	    if(host_name)
		free(host_name);

	    /* Copy the client data structure to the cache_request structure
	    */
	    if(in_client_data) {
		memcpy(&out_client_data->ipv4v6_address, &in_client_data->client_ipv4v6,
			sizeof(ip_addr_t));
		out_client_data->port		= in_client_data->client_port;
		out_client_data->client_ip_family = in_client_data->client_ip_family;
		
		memcpy(&(((nkn_pseudo_con_t *)*in_proto_data)->con.src_ipv4v6), &in_client_data->client_ipv4v6,
			sizeof(ip_addr_t));
		((nkn_pseudo_con_t *)*in_proto_data)->con.src_port
						= in_client_data->client_port;
		((nkn_pseudo_con_t *)*in_proto_data)->con.ip_family
						= in_client_data->client_ip_family;
		
		if((IPv4(in_client_data->origin_ipv4v6) !=0) || 
			 (IPv6(in_client_data->origin_ipv4v6)) !=0 ) {
		    snprintf(server_addr, MAX_URI_SIZE, "%s", get_ip_str(&in_client_data->origin_ipv4v6));
		    add_known_header(
			    &((nkn_pseudo_con_t *)*in_proto_data)->con.http.hdr,
			    MIME_HDR_X_NKN_REQ_REAL_DEST_IP, server_addr,
			    strlen(server_addr));
		    snprintf(server_addr, MAX_URI_SIZE, "%d",
			    in_client_data->origin_port);
		    add_known_header(
			    &((nkn_pseudo_con_t *)*in_proto_data)->con.http.hdr,
			    MIME_HDR_X_NKN_REQ_REAL_DEST_PORT, server_addr,
			    strlen(server_addr));
		}
		if(in_client_data->proto_data)
		    mime_hdr_copy_headers(
			    &((nkn_pseudo_con_t *)*in_proto_data)->con.http.hdr,
			    &((con_t *)in_client_data->proto_data)->http.hdr);
		add_known_header(
			    &((nkn_pseudo_con_t *)*in_proto_data)->con.http.hdr,
			    MIME_HDR_X_NKN_METHOD, "GET", 3);
		ret = get_known_header(&((nkn_pseudo_con_t *)*in_proto_data)->con.http.hdr,
			    MIME_HDR_X_LOCATION, &data, &datalen, &hattr, &hdrcnt);
		if(!ret)
		    SET_HTTP_FLAG(&((nkn_pseudo_con_t *)*in_proto_data)->con.http,
			    HRF_302_REDIRECT);
		if (in_client_data->encoding_type) {
			nkn_populate_encoding_hdr(in_client_data->encoding_type,
						    ((nkn_pseudo_con_t *)*in_proto_data));
		}
	    }
	    break;
    }
    if (*in_proto_data && in_client_data->flags & AM_NON_CHUNKED_STREAMING) {
	    SET_HTTP_FLAG(&((nkn_pseudo_con_t *)
			*in_proto_data)->con.http, HRF_STREAMING_DATA);
    }
}

void nkn_populate_attr_data(nkn_attr_t *nknattr, nkn_pseudo_con_t *pcon,
			    nkn_provider_type_t src_ptype)
{
    const char *data;
    int len;
    u_int32_t attrs;
    int hdrcnt;
    int rv = 0;
    mime_header_t hdr;
    if (nknattr == NULL || pcon == NULL) return;

    switch(src_ptype) {
	case OriginMgr_provider:
	    pcon->con.http.cache_encoding_type = nknattr->encoding_type;
	    mime_hdr_init(&hdr, MIME_PROT_HTTP, 0, 0);
	    rv = nkn_attr2mime_header(nknattr, 0, &hdr);
	    if (!rv) {
		if (!get_known_header(&hdr, MIME_HDR_X_NKN_REMAPPED_URI,
				    &data, &len, &attrs, &hdrcnt)) {
		    add_known_header(&pcon->con.http.hdr, MIME_HDR_X_NKN_REMAPPED_URI, data, len);
		}
		if (!get_known_header(&hdr, MIME_HDR_X_NKN_DECODED_URI,
					&data, &len, &attrs, &hdrcnt)) {
		    add_known_header(&pcon->con.http.hdr, MIME_HDR_X_NKN_DECODED_URI, data, len);
		}
		if (!get_known_header(&hdr, MIME_HDR_X_NKN_URI,
					&data, &len, &attrs, &hdrcnt)) {
		    add_known_header(&pcon->con.http.hdr, MIME_HDR_X_NKN_URI, data, len);
		}
		if (!get_known_header(&hdr, MIME_HDR_X_NKN_SEEK_URI,
					&data, &len, &attrs, &hdrcnt)) {
		    add_known_header(&pcon->con.http.hdr, MIME_HDR_X_NKN_SEEK_URI, data, len);
		}
		if (!get_known_header(&hdr, MIME_HDR_X_NKN_ABS_URL_HOST,
					&data, &len, &attrs, &hdrcnt)) {
		    add_known_header(&pcon->con.http.hdr, MIME_HDR_X_NKN_ABS_URL_HOST, data, len);
		}
	    }
	    mime_hdr_shutdown(&hdr);
	    return;
	default:
	    break;
    }
}

void nkn_free_proto_data(void *in_proto_data, nkn_provider_type_t src_ptype)
{
    switch(src_ptype) {
	case RTSPMgr_provider:
	    nkn_free_orp_data(in_proto_data);
	    break;
	case NFSMgr_provider:
	    break;
	case OriginMgr_provider:
	default:
	    nkn_free_pseudo_con_t((nkn_pseudo_con_t *)in_proto_data);
	    break;
    }
}
/*
 * End of nkn_pseudo_con.c
 */
