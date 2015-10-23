/* 
 * main.c -- Content Broker main
 */
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <signal.h>
#include <netdb.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <pthread.h>
#include <pthread.h>
#include <limits.h>
#include <stdint.h>

#define _GNU_SOURCE
#include <getopt.h>

#include "libnknalog.h"
#include "proto_http/proto_http.h"
#include "libnet/libnet.h"
#include "cb/content_broker.h"
#include "cb_log.h"
#include "cb_malloc.h"

extern void config_and_run_counter_update(void);

#define UNUSED_ARGUMENT(x) (void)x
#define TSTR(_p, _l, _r) char *(_r); \
	( 1 ? ( (_r) = alloca((_l)+1), \
		memcpy((_r), (_p), (_l)), (_r)[(_l)] = '\0' ) : 0 )

/*
 * getopt static data
 */
const char *option_str = "a:dehl:p:vV";

static struct option long_options[] = {
    {"accesslog", 0, 0, 'a'},
    {"daemonize", 0, 0, 'd'},
    {"errlog", 0, 0, 'e'},
    {"help", 0, 0, 'h'},
    {"loglevel", 1, 0, 'l'},
    {"port", 1, 0, 'p'},
    {"verbose", 0, 0, 'v'},
    {"valgrind", 0, 0, 'V'},
    {0, 0, 0, 0}
};

const char *helpstr = (
"Content Broker 0.1\n\n"
"Options:\n"
"\t-a --accesslog (profile name default: \"cb_accesslog\")\n"
"\t-d --daemonize\n"
"\t-e --errlog (enable error/access log default: disable)\n"
"\t-h --help\n"
"\t-l --loglevel [0-6] (default: 6)\n"
"\t-p --port [listen port]\n"
"\t-v --verbose\n"
"\t-V --valgrind\n"
);

const char *opt_accesslog = "cb_accesslog";
int opt_daemonize = 0; // disable
int opt_verbose = 0; // disable
int opt_valgrind_mode = 0; // disable
int opt_enable_errlog = 0; // disable

static GHashTable *HT_hdl2token;
static GMutex *HT_hdl2token_mutex;
static alog_conn_data_t NULL_alog_cdata;

static 
int hash_init(GHashTable **hashtbl, GMutex **mutex, 
	      void (*key_free)(gpointer), void (*value_free)(gpointer))
{
    *hashtbl = g_hash_table_new_full(g_str_hash, g_str_equal,
				     key_free, value_free);
    if (!*hashtbl) {
    	return 1;
    }

    *mutex = g_mutex_new();
    if (!*mutex) {
    	return 2;
    }
    return 0;
}

static
int hash_add(GHashTable *hashtbl, GMutex *mutex, char *key, void *value)
{
    g_mutex_lock(mutex);
    g_hash_table_replace(hashtbl, key, value);
    g_mutex_unlock(mutex);

    return 0;
}

static
void *hash_lookup(GHashTable *hashtbl, GMutex *mutex, char *key)
{
    void *ret;

    g_mutex_lock(mutex);
    ret = g_hash_table_lookup(hashtbl, key);
    g_mutex_unlock(mutex);

    return ret;
}

static
int hash_delete(GHashTable *hashtbl, GMutex *mutex, const char *key)
{
    gboolean res;

    g_mutex_lock(mutex);
    res = g_hash_table_remove(hashtbl, key);
    g_mutex_unlock(mutex);

    return res;
}

static
void HT_hdl2token_key_free(gpointer key)
{
    if (key) {
    	cb_free(key);
    }
}

static
void HT_hdl2token_val_free(gpointer val)
{
    if (val) {
    	deleteHTTPtoken_data((token_data_t) val);
    }
}

#define MK_HDL2TOKEN_HASH_KEY(_hdl, _key, _sizeof_key) { \
    int _ret = snprintf((_key), (_sizeof_key), "%lx%lx", \
    			(_hdl)->opaque[0], (_hdl)->opaque[1]); \
    if (_ret >= (int)(_sizeof_key)) { \
    	(_key)[(_sizeof_key)-1] = '\0'; \
    } \
}

#define HDL2TOKEN_LOOKUP(_hdl) handle2token_lookup((_hdl))
#define HDL2TOKEN_ADD(_hdl, _token) handle2token_add((_hdl), (_token))
#define HDL2TOKEN_DELETE(_hdl) handle2token_delete((_hdl))

static
token_data_t *handle2token_lookup(const libnet_conn_handle_t *hdl)
{
    char key[64];

    MK_HDL2TOKEN_HASH_KEY(hdl, key, sizeof(key));
    return (token_data_t *)hash_lookup(HT_hdl2token, HT_hdl2token_mutex, key);
}

static
int handle2token_add(const libnet_conn_handle_t *hdl, token_data_t *token)
{
    char *key = cb_malloc(64);

    if (!key) {
    	return 100;
    }

    MK_HDL2TOKEN_HASH_KEY(hdl, key, 64);
    return hash_add(HT_hdl2token, HT_hdl2token_mutex, key, (void *)token);
}

static
int handle2token_delete(const libnet_conn_handle_t *hdl)
{
    char key[64];

#if 0
    if (!key) {
    	return 100;
    }
#endif

    MK_HDL2TOKEN_HASH_KEY(hdl, key, sizeof(key));
    return hash_delete(HT_hdl2token, HT_hdl2token_mutex, key);
}

/*
 *******************************************************************************
 * Proto HTTP callback functions
 *******************************************************************************
 */
static
int http_log(proto_http_log_level_t level, long module,
	     const char *fmt, ...)
{
    UNUSED_ARGUMENT(module);
    va_list ap;

    va_start(ap, fmt);
    return cb_log_var(SS_HTTP, (cb_log_level)level, fmt, ap);
}

static
void *http_malloc_type(size_t size, int type)
{
    UNUSED_ARGUMENT(type);
    return cb_malloc(size);
}

static
void *http_realloc_type(void *ptr, size_t size, int type)
{
    UNUSED_ARGUMENT(type);
    return cb_realloc(ptr, size);
}

static
void *http_calloc_type(size_t nmemb, size_t size, int type)
{
    UNUSED_ARGUMENT(type);
    return cb_calloc(nmemb, size);
}

static
void *http_memalign_type(size_t align, size_t size, int type)
{
    UNUSED_ARGUMENT(type);
    return cb_memalign(align, size);
}

static
void *http_valloc_type(size_t size, int type)
{
    UNUSED_ARGUMENT(type);
    return cb_valloc(size);
}

static
void *http_pvalloc_type(size_t size, int type)
{
    UNUSED_ARGUMENT(type);
    return cb_pvalloc(size);
}

static
char *http_strdup_type(const char *s, int type)
{
    UNUSED_ARGUMENT(type);
    return cb_strdup(s);
}

static
void http_free(void *ptr)
{
    return cb_free(ptr);
}

/*
 *******************************************************************************
 * libnet callback functions
 *******************************************************************************
 */
static
int libnet_dbg_log(int level, long module, const char *fmt, ...)
{
    UNUSED_ARGUMENT(module);
    va_list ap;

    va_start(ap, fmt);
    return cb_log_var(SS_NET, (cb_log_level)level, fmt, ap);
}

static
void *libnet_malloc_type(size_t size, int type)
{
    UNUSED_ARGUMENT(type);
    return cb_malloc(size);
}

static
void *libnet_realloc_type(void *ptr, size_t size, int type)
{
    UNUSED_ARGUMENT(type);
    return cb_realloc(ptr, size);
}

static
void *libnet_calloc_type(size_t nmemb, size_t size, int type)
{
    UNUSED_ARGUMENT(type);
    return cb_calloc(nmemb, size);
}

static
void libnet_free(void *ptr)
{
    return cb_free(ptr);
}

static int
close_conn(libnet_conn_handle_t *handle, int consider_keepalive, 
	   int delete_hdl2token_mapping)
{
    int rv;
    int rv2;

    rv = libnet_close_conn(handle, consider_keepalive);
    if (rv) {
	CB_LOG(CB_ERROR, 
	       "Handle=(0x%lx 0x%lx) libnet_close_conn() failed, ka=%d rv=%d",
	       handle->opaque[0], handle->opaque[1], consider_keepalive, rv);
    }

    if (delete_hdl2token_mapping) {
    	// Remove mapping if it exists.
    	rv2 = HDL2TOKEN_DELETE(handle);
    }

    return rv;
}

static
int libnet_event_recv_data(libnet_conn_handle_t *handle, 
			   char *data, int datalen, 
			   const libnet_conn_attr_t *attr)
{
    int rv;
    int ret = 0;
    proto_data_t pd;
    token_data_t token;
    HTTPProtoStatus status;

    const char *uri;
    int urilen;
    int hdrcnt;

    char *location;
    int strlen_location;
    int max_method_host_port_len;

    char *respbuf;
    int respbuflen;

    cb_route_data_t cb_rd;
    alog_conn_data_t alog_cdata;
    uint64_t http_opts;
    int http_resp_code = 0;

    alog_cdata = NULL_alog_cdata;
    alog_cdata.dst_port = *attr->DestPort;
    alog_cdata.src_ipv4v6 = *((ip_addr_t *)attr->clientip);
    alog_cdata.dst_ipv4v6 = *((ip_addr_t *)attr->destip);
    gettimeofday(&alog_cdata.start_ts, 0);
    alog_cdata.flag = ALF_METHOD_GET;

    pd.u.HTTP.destIP = attr->DestIP;
    pd.u.HTTP.destPort = *attr->DestPort;
    pd.u.HTTP.clientIP = attr->ClientIP;
    pd.u.HTTP.clientPort = *attr->ClientPort;
    pd.u.HTTP.data = data;
    pd.u.HTTP.datalen = datalen;

    token = HTTPProtoData2TokenData(0, &pd, &status);
    if (status != HP_SUCCESS) {
    	if (status == HP_MORE_DATA) {
	    return 1; // Callback when more data available
	}
    }

    while (1) {

    http_opts = HTTPProtoReqOptions(token);
    if (!(http_opts & PH_ROPT_METHOD_GET)) {
    	http_resp_code = 400;
    	HTTPAddKnownHeader(token, 1, H_CONTENT_LENGTH, "0", 1);
	rv = HTTPResponse(token, http_resp_code, (const char **)&respbuf,
			  &respbuflen, 0);
	if (rv) {
	    CB_LOG(CB_ERROR, 
	    	   "Handle=(0x%lx 0x%lx) HTTPResponse() failed, rv=%d", 
		   handle->opaque[0], handle->opaque[1], rv);
	    ret = 1;
	    break;
	}
	goto send_response;
    }

    rv = HTTPGetKnownHeader(token, 0, H_X_NKN_URI, &uri, &urilen, &hdrcnt);
    if (rv) {
    	http_resp_code = 400;
    	HTTPAddKnownHeader(token, 1, H_CONTENT_LENGTH, "0", 1);
	rv = HTTPResponse(token, http_resp_code, (const char **)&respbuf,
			  &respbuflen, 0);
	if (rv) {
	    CB_LOG(CB_ERROR, 
	    	   "Handle=(0x%lx 0x%lx) HTTPResponse() failed, rv=%d", 
		   handle->opaque[0], handle->opaque[1], rv);
	    ret = 2;
	    break;
	}
	goto send_response;
    }

    TSTR(uri, urilen, uristr);
    CB_LOG(CB_MSG, "Handle=(0x%lx 0x%lx) Token=%p Request URL abs_path=[%s]",
	   handle->opaque[0], handle->opaque[1], token, uristr);

    rv = cb_route(uristr, 0 /* Select best loc */, &cb_rd);
    if (!rv) {
	max_method_host_port_len = sizeof(cb_rd.loc[0].fqdn) + 4096;
	strlen_location = urilen + max_method_host_port_len;
	location = alloca(strlen_location + 1);

	rv = snprintf(location, strlen_location, "http://%s:%hu", 
		      cb_rd.loc[0].fqdn, cb_rd.loc[0].port);
	if (rv >= max_method_host_port_len) {
	    CB_LOG(CB_ERROR, 
	    	   "Handle=(0x%lx 0x%lx) Location max length exceeded "
		   "(%d >= %d)", 
		   handle->opaque[0], handle->opaque[1],
		   rv, max_method_host_port_len);
	    ret = 3;
	    break;
	}
	strncat(location, uri, urilen);

	rv = HTTPAddKnownHeader(token, 1, H_LOCATION, 
				location, strlen(location));
	if (rv) {
	    CB_LOG(CB_ERROR, 
	    	   "Handle=(0x%lx 0x%lx) HTTPAddKnownHeader(H_LOCATION) "
		   "failed, rv=%d", handle->opaque[0], handle->opaque[1], rv);
	    ret = 4;
	    break;
	}

	http_resp_code = 302;
	rv = HTTPResponse(token, http_resp_code, (const char **)&respbuf,
			  &respbuflen, 0);
	if (rv) {
	    CB_LOG(CB_ERROR, 
	    	   "Handle=(0x%lx 0x%lx) HTTPResponse() failed, rv=%d", 
		   handle->opaque[0], handle->opaque[1], rv);
	    ret = 5;
	    break;
	}
	CB_LOG(CB_MSG, "Handle=(0x%lx 0x%lx) Send HTTP 302, Location:[%s]", 
	       handle->opaque[0], handle->opaque[1], location);
    } else {
    	HTTPAddKnownHeader(token, 1, H_CONTENT_LENGTH, "0", 1);
	http_resp_code = 404;
    	rv = HTTPResponse(token, http_resp_code, (const char **)&respbuf,
			  &respbuflen, 0);
	if (rv) {
	    CB_LOG(CB_ERROR, 
	    	   "Handle=(0x%lx 0x%lx) HTTPResponse() failed, rv=%d",
		   handle->opaque[0], handle->opaque[1], rv);
	    ret = 6;
	    break;
	}
	CB_LOG(CB_MSG, 
	       "Handle=(0x%lx 0x%lx) Send HTTP 404, URL abs_path:[%s]", 
	       handle->opaque[0], handle->opaque[1], uristr);
    }

send_response:

    if (http_opts & PH_ROPT_HTTP_11) {
    	alog_cdata.flag |= ALF_HTTP_11;
    } else if (http_opts & PH_ROPT_HTTP_10) {
    	alog_cdata.flag |= ALF_HTTP_10;
    } else if (http_opts & PH_ROPT_HTTP_09) {
    	alog_cdata.flag |= ALF_HTTP_09;
    }
    alog_cdata.in_bytes = HTTPHeaderBytes(token);
    alog_cdata.status = http_resp_code;
    alog_cdata.resp_hdr_size = respbuflen;

    rv = libnet_send_data(handle, respbuf, respbuflen);
    if (rv > 0) {
    	gettimeofday(&alog_cdata.end_ts, 0);
    	rv = cb_access_log(&alog_cdata, token);
	if (rv) {
	    CB_LOG(CB_ERROR, "cb_access_log() failed, rv=%d", rv);
	}
    	break;
    } else {
	rv = HTTPWriteUDA(token, (char *)&alog_cdata, sizeof(alog_cdata));
	if (rv != sizeof(alog_cdata)) {
	    CB_LOG(CB_ERROR, "HTTPWriteUDA() failed, rv=%d expected=%d",
	    	   rv, sizeof(alog_cdata));
	}

    	// Write failed, retry later
	rv = HDL2TOKEN_ADD(handle, token);
	if (rv) {
	    CB_LOG(CB_ERROR, 
	    	   "Handle=(0x%lx 0x%lx) HDL2TOKEN_ADD() failed, rv=%d",
		   handle->opaque[0], handle->opaque[1], rv);
	    ret = 7;
	    break;
	}
    	return 2;
    }

    } // End while

    rv = close_conn(handle, (!ret ? HTTPisKeepAliveReq(token) : 0), 0);
    if (rv) {
	CB_LOG(CB_ERROR, 
	       "close_conn() Handle=(0x%lx 0x%lx) failed, rv=%d", 
	       handle->opaque[0], handle->opaque[1], rv);
    }
    deleteHTTPtoken_data(token);
    return 0;
}

static
int libnet_event_recv_data_err(libnet_conn_handle_t *handle, 
			       char *data, int datalen, 
			       const libnet_conn_attr_t *attr, int reason)
{
    UNUSED_ARGUMENT(attr);
    int rv;

    CB_LOG(CB_MSG, "Handle=(0x%lx 0x%lx) data=%p datalen=%d reason=%d",
    	   handle->opaque[0], handle->opaque[1], data, datalen, reason);

    rv = close_conn(handle, 0, 1);
    if (rv) {
	CB_LOG(CB_ERROR, 
	       "close_conn() Handle=(0x%lx 0x%lx) failed, "
	       "rv=%d reason=%d",
	       handle->opaque[0], handle->opaque[1], rv, reason);
    }
    return 0;
}

static
int libnet_event_send_data(libnet_conn_handle_t *handle)
{
    int rv;
    int ret = 0;
    char *respbuf;
    int respbuflen;
    int respcode = -1;
    const char *val;
    int vallen;
    int hdrcnt;
    alog_conn_data_t alog_cdata;

    token_data_t token = HDL2TOKEN_LOOKUP(handle);

    CB_LOG(CB_MSG, "Handle=(0x%lx 0x%lx) Token=%p",
    	   handle->opaque[0], handle->opaque[1], token);
    
    if (token) {
	rv = HTTPReadUDA(token, (char *)&alog_cdata, sizeof(alog_cdata));
	if (rv != sizeof(alog_cdata)) {
	    CB_LOG(CB_ERROR, "HTTPReadUDA() failed, rv=%d expected=%d",
	    	   rv, sizeof(alog_cdata));
	    alog_cdata = NULL_alog_cdata;
	}

    	rv = HTTPResponse(token, -1, (const char **)&respbuf, &respbuflen, 
			  &respcode);
	if (!rv) {
	    rv = libnet_send_data(handle, respbuf, respbuflen);
	    if (rv > 0) {

	    	// Make access log entry
    		gettimeofday(&alog_cdata.end_ts, 0);
		rv = cb_access_log(&alog_cdata, token);
		if (rv) {
		    CB_LOG(CB_ERROR, "cb_access_log() failed, rv=%d", rv);
		}

		switch(respcode) {
		case 302:
		{
		    val = 0;
		    rv = HTTPGetKnownHeader(token, 1, H_LOCATION, 
					    &val, &vallen, &hdrcnt);
		    if (!val) {
		    	val = "";
		    	vallen = 1;
		    }
		    TSTR(val, vallen, locstr);
		    CB_LOG(CB_MSG, 
			   "Handle=(0x%lx 0x%lx) Send HTTP 302, Location:[%s]",
			   handle->opaque[0], handle->opaque[1], locstr);
		    break;
		}
		case 404:
		{
		    val = 0;
		    rv = HTTPGetKnownHeader(token, 1, H_X_NKN_URI, 
					    &val, &vallen, &hdrcnt);
		    if (!val) {
		    	val = "";
		    	vallen = 1;
		    }
		    TSTR(val, vallen, urlstr);
		    CB_LOG(CB_MSG, "Handle=(0x%lx 0x%lx) "
		    	   "Send HTTP 404, URL abs_path:[%s]", 
			   handle->opaque[0], handle->opaque[1], urlstr);
		    break;
		}
		default:
		{
		    TSTR(respbuf, respbuflen, respstr);
		    CB_LOG(CB_MSG, "Handle=(0x%lx 0x%lx) Send [%s]",
			   handle->opaque[0], handle->opaque[1], respstr);
		    break;
		}
		} // End of switch

    		rv = close_conn(handle, HTTPisKeepAliveReq(token), 1);
    		if (rv) {
		    CB_LOG(CB_ERROR, 
	       		   "close_conn() Handle=(0x%lx 0x%lx) failed, "
			   "rv=%d", 
			   handle->opaque[0], handle->opaque[1], rv);
    		}
    		return 0;

	    } else {
	    	// Write failed, retry request
	    	return 2;
	    }
	} else {
	    CB_LOG(CB_ERROR, 
		   "HTTPResponse(-1) failed, Handle=(%0xlx %0xlx) rv=%d",
		   handle->opaque[0], handle->opaque[1], rv);
	    ret = 1;
	}
    } else {
    	ret = 2;
    }

    rv = close_conn(handle, 0, 1);
    if (rv) {
	CB_LOG(CB_ERROR, 
	       "libnet_close_conn() Handle=(0x%lx 0x%lx) failed, rv=%d", 
	       handle->opaque[0], handle->opaque[1], rv);
    }
    return 0;
}

static
int libnet_event_timeout(libnet_conn_handle_t *handle)
{
    int rv;

    CB_LOG(CB_MSG, "Handle=(0x%lx 0x%lx)",
    	   handle->opaque[0], handle->opaque[1]);

    rv = close_conn(handle, 0, 1);
    if (rv) {
	CB_LOG(CB_ERROR, 
	       "close_conn() Handle=(0x%lx 0x%lx) failed, rv=%d", 
	       handle->opaque[0], handle->opaque[1], rv);
    }
    return 0;
}

/*
 * main support functions
 */
static 
int getoptions(int argc, char **argv, libnet_config_t *net_cfg)
{
    int c;
    int option_index = 0;
    int num_listen_ports = 0;

    memset(net_cfg, 0, sizeof(*net_cfg));

    while (1) {
        c = getopt_long(argc, argv, option_str, long_options, &option_index);
        if (c == -1) {
            break;
        }
        switch (c) {
        case 'a':
	    opt_accesslog = optarg;
	    break;

        case 'e':
	    opt_enable_errlog = 1;
	    break;
		
        case 'h':
            printf("[%s]: \n%s\n", argv[0], helpstr);
            exit(110);
	    break;

        case 'l':
	    log_level = atoi(optarg);
	    if (log_level < CB_ALARM) {
	        log_level = CB_ALARM;
	    } else if (log_level > CB_MSG3) {
	        log_level = CB_MSG3;
	    }
	    break;

        case 'p':
	    if (num_listen_ports >= LIBNET_MAX_LISTEN_PORTS) {
		printf("Maximum listen ports exceeded (%d > %d)\n", 
		       num_listen_ports+1, LIBNET_MAX_LISTEN_PORTS);
		exit(111);
	    }
            net_cfg->libnet_listen_ports[num_listen_ports] = atoi(optarg);
	    if (!net_cfg->libnet_listen_ports[num_listen_ports] ||
		(net_cfg->libnet_listen_ports[num_listen_ports] > USHRT_MAX)) {
		printf("Invalid TCP Port (%d)\n", 
		       net_cfg->libnet_listen_ports[num_listen_ports]);
		exit(112);
	    }
	    num_listen_ports++;
            break;

        case 'v':
            opt_verbose = 1;
            break;

        case 'V':
            opt_valgrind_mode = 1;
            break;

        case '?':
            printf("[%s]: \n%s\n", argv[0], helpstr);
            exit(113);
	    break;
        }
    }
    if (!num_listen_ports) {
	printf("No listen ports specified\n");
	exit(114);
    }
    return 0;
}

static
void sig_handler(int signum)
{
    if (signum == SIGUSR1) {
    	/* Do nothing */
	return;
    } else if (signum == SIGUSR2) {
    	/* Force coredump via SEGV */
	CB_LOG(CB_SEVERE, "Creating coredump via SEGV, sig=%d", signum);
	*((char *)1) = 1;
    }
    exit(1);
}

static
void daemonize(void)
{
    if (fork() != 0) {
    	/* Terminate parent */
    	exit(0);
    }
    if (setsid() == -1) {
    	exit(0);
    }
    signal(SIGHUP, SIG_IGN);
    if (fork() != 0) {
    	/* Terminate parent */
    	exit(0);
    }

#if 0
    /*
     * Do not chdir when this process is managed by PM
     */
    if (chdir("/") != 0)  {
    	exit(0);
    }
#endif
}

int main(int argc, char **argv)
{
    int rv;
    proto_http_config_t http_cfg;
    libnet_config_t net_cfg;
    int libnet_err;
    int libnet_suberr;
    pthread_t net_parent_thread_id;
    void *value_ptr;

    assert(sizeof(alog_conn_data_t) <= PH_MAX_UDA_SZ);

    rv = getoptions(argc, argv, &net_cfg);
    if (rv) {
    	CB_LOG(CB_SEVERE, "getoptions() failed rv=%d", rv);
    	exit(10);
    }

    cb_log_init(opt_enable_errlog, opt_accesslog);

    if (signal(SIGINT, sig_handler) == SIG_IGN) {
    	signal(SIGINT, SIG_IGN);
    }

    if (signal(SIGHUP, sig_handler) == SIG_IGN) {
    	signal(SIGHUP, SIG_IGN);
    }

    if (signal(SIGTERM, sig_handler) == SIG_IGN) {
    	signal(SIGTERM, SIG_IGN);
    }

    if (signal(SIGQUIT, sig_handler) == SIG_IGN) {
    	signal(SIGQUIT, SIG_IGN);
    }
    signal(SIGUSR1, sig_handler);
    signal(SIGUSR2, sig_handler);

    if (opt_daemonize) {
    	daemonize();
    }

    /*
     * Glib initialization
     */
    g_thread_init(0);

    /*
     * Initialize global counter mgmt
     */
    config_and_run_counter_update();

    /*
     * Initialize Content Broker routing subsystem
     */
    rv = cb_init();
    if (rv) {
    	CB_LOG(CB_SEVERE, "cb_init() failed rv=%d", rv);
	exit(11);
    }

    /*
     * Initialize HTTP parser subsystem
     */
    memset(&http_cfg, 0, sizeof(http_cfg));
    http_cfg.interface_version = PROTO_HTTP_INTF_VERSION;
    http_cfg.options = OPT_NONE;
    http_cfg.log_level = &log_level;
    http_cfg.logfunc = http_log;
    http_cfg.proc_malloc_type = http_malloc_type;
    http_cfg.proc_realloc_type = http_realloc_type;
    http_cfg.proc_calloc_type = http_calloc_type;
    http_cfg.proc_memalign_type = http_memalign_type;
    http_cfg.proc_valloc_type = http_valloc_type;
    http_cfg.proc_pvalloc_type = http_pvalloc_type;
    http_cfg.proc_strdup_type = http_strdup_type;
    http_cfg.proc_free = http_free;

    rv = proto_http_init(&http_cfg);
    if (rv) {
    	CB_LOG(CB_SEVERE, "proto_http_init() failed rv=%d", rv);
	exit(12);
    }

    /*
     * Initialize HT_hdl2token hash table (used by libnet callouts)
     */
    rv = hash_init(&HT_hdl2token, &HT_hdl2token_mutex, 
    		   HT_hdl2token_key_free, HT_hdl2token_val_free);
    if (rv) {
    	CB_LOG(CB_SEVERE, "hdl2token hash_init() failed rv=%d", rv);
	exit(13);
    }

    /*
     * Initialize libnet subsystem
     */
    net_cfg.interface_version = LIBNET_INTF_VERSION;
    if (opt_valgrind_mode) {
    	net_cfg.options |= LIBNET_OPT_VALGRIND;
    }
    net_cfg.log_level = &log_level;
    net_cfg.libnet_dbg_log = libnet_dbg_log;
    net_cfg.libnet_malloc_type = libnet_malloc_type;
    net_cfg.libnet_realloc_type = libnet_realloc_type;
    net_cfg.libnet_calloc_type = libnet_calloc_type;
    net_cfg.libnet_free = libnet_free;

    net_cfg.libnet_recv_data_event = libnet_event_recv_data;
    net_cfg.libnet_recv_data_event_err = libnet_event_recv_data_err;
    net_cfg.libnet_send_data_event = libnet_event_send_data;
    net_cfg.libnet_timeout_event = libnet_event_timeout;

    rv = libnet_init(&net_cfg, &libnet_err, &libnet_suberr, 
		     &net_parent_thread_id);
    if (rv) {
    	CB_LOG(CB_SEVERE, "libnet_init() failed rv=%d err=%d suberr=%d",
	       rv, libnet_err, libnet_suberr);
	exit(14);
    }
    CB_LOG(CB_MSG, "Content Broker starting...");

    rv = pthread_join(net_parent_thread_id, &value_ptr);
    if (rv) {
    	CB_LOG(CB_SEVERE, "pthread_join() failed rv=%d", rv);
	exit(15);
    }
    exit(0);
}

/*
 * End of main.c
 */
