/*
 * get_manager.c -- fqueue consumer which loads the given object into
 *	the cache via GET and TFM put tasks.
 *	Requests consist of an URL and mime_header_t.
 *	The get_manager is an in process version of the offline origin manager.
 *
 * Basic flow:
 * 1) Dequeue request from fqueue /tmp/GMGR.queue
 * 2) Create a GET task to sequentially retrieve the given object.
 * 3) At each GET task callback, push the given data to TFM by creating
 *    an asynchronous TFM put task.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <poll.h>
#include <unistd.h>
#include <libgen.h>
#include <malloc.h>
#include <semaphore.h>
#include <errno.h>
#include <assert.h>
#include <sys/prctl.h>
#include <strings.h>

#include "nkn_mgmt_agent.h"
#include "nkn_log_strings.h"

#include "nkn_defs.h"
#include "nkn_cfg_params.h"
#include "nkn_stat.h"
#include "nkn_attr.h"
#include "nkn_debug.h"
#include "nkn_mediamgr_api.h"

#include "server.h"
#include "fqueue.h"
#include "http_header.h"
#include "om_defs.h"
#include "get_manager.h"
#include "nkn_cod.h"
#include "nkn_pseudo_con.h"

// #define SEND_UOL_URI 1

/*
 *******************************************************************************
 * Macro definitions
 *******************************************************************************
 */
#if 1
#define STATIC static
#else
#define STATIC
#endif

#define DBG(_fmt, ...) DBG_LOG(MSG, MOD_GM, _fmt, __VA_ARGS__)

#define TSTR(_p, _l, _r)  char *(_r); ( 1 ? ( (_r) = alloca((_l)+1), \
        memcpy((_r), (_p), (_l)), (_r)[(_l)] = '\0' ) : 0 )

#define GM_TASK_SEM_WAIT() { \
    int _rv; \
    int _my_errno; \
    do { \
    	_rv = sem_wait(&gm_task_sem); \
    } while ((_rv != 0) && (errno == EINTR)); \
    _my_errno = errno; \
    assert(_rv == 0); \
}

#define GM_TASK_SEM_POST() { \
    int _rv; \
    _rv = sem_post(&gm_task_sem); \
    assert(_rv == 0); \
}

#define PCON_LINK_TAIL(_pcon) { \
	(_pcon)->next = &PCON_HEAD; \
	(_pcon)->prev = PCON_HEAD.prev; \
	PCON_HEAD.prev->next = (_pcon); \
	PCON_HEAD.prev = (_pcon); \
	PCON_HEAD_ELEMENTS++; \
}

#define PCON_LINK_HEAD(_pcon) { \
	(_pcon)->next = PCON_HEAD.next; \
	(_pcon)->prev = &PCON_HEAD; \
	PCON_HEAD.next->prev = (_pcon); \
	PCON_HEAD.next = (_pcon); \
	PCON_HEAD_ELEMENTS++; \
}

#define PCON_UNLINK(_pcon) { \
	(_pcon)->prev->next = (_pcon)->next; \
	(_pcon)->next->prev = (_pcon)->prev; \
	(_pcon)->next = 0; \
	(_pcon)->prev = 0; \
	PCON_HEAD_ELEMENTS--; \
}

/*
 *******************************************************************************
 * Declarations
 *******************************************************************************
 */
NKNCNT_DEF(gm_get_tasks, int32_t, "", "Pending get manager tasks")
NKNCNT_DEF(gm_tfm_put_tasks, int32_t, "", "Pending tfm put tasks")

NKNCNT_DEF(gm_total_get_tasks, int64_t, "", "Total get tasks created")
NKNCNT_DEF(gm_total_tfm_put_tasks, int64_t, "", "Total tfm put tasks created")

NKNCNT_DEF(gm_total_get_task_successes, int64_t, "", "Total get task successes")
NKNCNT_DEF(gm_total_get_task_fails, int64_t, "", "Total get task failures")
NKNCNT_DEF(gm_total_get_task_err_rets, int64_t, "", "Total get task err rets")

NKNCNT_DEF(gm_total_tfm_put_successes, int64_t, "", "Total tfm put successes")
NKNCNT_DEF(gm_total_tfm_put_fails, int64_t, "", "Total tfm put failures")

NKNCNT_DEF(gm_total_bytes_read, int64_t, "", "Total bytes read")


typedef struct pseudo_con { // derived class
    nkn_pseudo_con_t con; // Always first
    int64_t flags;
    off_t bytes_read;
    nkn_uol_t uol;
    struct pseudo_con *next;
    struct pseudo_con *prev;
    /* Last segment to retrieve */
    off_t last_chunk_offset;
    off_t last_chunk_size;
} pseudo_con_t;

/* 
 * pseudo_con_t.flags definitions
 */
#define PC_FL_VIRTUAL_DOMAIN_REVERSE_PROXY	(1 << 0)
#define PC_FL_TRANSPARENT_REVERSE_PROXY		(1 << 1)
#define PC_FL_SEND_LAST_CHUNK			(1 << 2)

typedef struct TFM_put_privatedata {
    nkn_task_id_t buf_task_id; // Task holding buffers
    nkn_task_id_t my_task_id;
    nkn_buffer_t *attrbuf;
    int num_iovecs;
    nkn_iovec_t content[MAX_CM_IOVECS];
} TFM_put_privatedata_t;

static unsigned int max_pending_gm_tasks = 8; // Concurrent GET tasks
static sem_t gm_task_sem;

static pthread_t gmgr_req_thread_id;

static pseudo_con_t PCON_HEAD;
static int PCON_HEAD_ELEMENTS = -1;
static pthread_mutex_t pcon_head_lock = PTHREAD_MUTEX_INITIALIZER;

STATIC pseudo_con_t *alloc_pseudo_con_t(void);
STATIC void free_pseudo_con_t(pseudo_con_t *pcon);

STATIC cache_response_t *alloc_cache_response_t(pseudo_con_t *pcon);
STATIC void free_cache_response_t(cache_response_t *cr);

STATIC TFM_put_privatedata_t *alloc_TFM_put_privatedata_t(nkn_task_id_t id, 
						   cache_response_t *cptr);
STATIC void free_TFM_put_privatedata_t(TFM_put_privatedata_t *tfm_priv);

STATIC MM_put_data_t *alloc_MM_put_data_t(TFM_put_privatedata_t *tfm_priv, 
				   nkn_cod_t cod, off_t offset, off_t len,
				   off_t total_length);
STATIC void free_MM_put_data_t(MM_put_data_t *MM_put_data);

STATIC int sched_get_task(pseudo_con_t *pcon);
STATIC int get_task_response_handler(nkn_task_id_t id, cache_response_t *cptr, 
				     pseudo_con_t *pcon);

STATIC int process_request(fqueue_element_t *qe);
STATIC void *gmgr_req_func(void *arg);
STATIC int start_http_get_task(const char *uri, int uri_len, 
			       pseudo_con_t *pcon);

STATIC int sched_tfm_task(TFM_put_privatedata_t *tfm_priv, 
		   		nkn_cod_t cod, off_t offset, off_t len, 
				off_t total_length);

STATIC void get_mgr_input(nkn_task_id_t id);
STATIC void get_mgr_output(nkn_task_id_t id);
STATIC void get_mgr_cleanup(nkn_task_id_t id);

STATIC void get_mgr_tfm_input(nkn_task_id_t id);
STATIC void get_mgr_tfm_output(nkn_task_id_t id);
STATIC void get_mgr_tfm_cleanup(nkn_task_id_t id);

/*
 *******************************************************************************
 * 		P R I V A T E  F U N C T I O N S
 *******************************************************************************
 */

/* Private structure associated with the GET task */
STATIC
pseudo_con_t *alloc_pseudo_con_t(void)
{
    return (pseudo_con_t *)nkn_alloc_pseudo_con_t_custom(sizeof(pseudo_con_t));
}

STATIC
void free_pseudo_con_t(pseudo_con_t *pcon)
{
    if (pcon) {
    	pthread_mutex_lock(&pcon_head_lock);
	if (pcon->next) {
	    PCON_UNLINK(pcon);
	}
    	pthread_mutex_unlock(&pcon_head_lock);

    	if (pcon->uol.cod != NKN_COD_NULL) {
      	    nkn_cod_close(pcon->uol.cod, NKN_COD_GET_MGR);
    	}
	nkn_free_pseudo_con_t((nkn_pseudo_con_t *)pcon);
	glob_gm_get_tasks--;
    }
}

/* Structure associated with the GET task */
STATIC
cache_response_t *alloc_cache_response_t(pseudo_con_t *pcon)
{
    cache_response_t *cr = nkn_calloc_type(1, sizeof(cache_response_t),
    					mod_fq_cache_response_t);
    if (cr) {
    	cr->magic = CACHE_RESP_REQ_MAGIC;
    	cr->uol = pcon->uol;
#ifdef SEND_UOL_URI
    	cr->uol.uri = (char *)nkn_cod_get_cnp(cr->uol.cod);
#endif
	cr->in_client_data.proto = NKN_PROTO_HTTP;
	cr->in_client_data.ip_address = pcon->con.con.src_ip;
	cr->in_client_data.port = pcon->con.con.src_port;
	cr->in_client_data.client_ip_family = pcon->con.con.ip_family;
	cr->in_proto_data = &pcon->con;
	if (!cr->uol.offset) {
	    cr->in_rflags |= (CR_RFLAG_GETATTR | CR_RFLAG_FIRST_REQUEST);
	    SET_CON_FLAG(&pcon->con.con, CONF_FIRST_TASK);
	} else {
	    UNSET_CON_FLAG(&pcon->con.con, CONF_FIRST_TASK);
	}
	cr->in_rflags |= CR_RFLAG_INTERNAL;         // do not count as hit
	cr->ns_token = pcon->con.con.http.ns_token;
    }
    return cr;
}

STATIC
void free_cache_response_t(cache_response_t *cr) 
{
    free(cr);
}

/* Private structure associated with the TFM put task */
STATIC
TFM_put_privatedata_t *alloc_TFM_put_privatedata_t(nkn_task_id_t id, 
						   cache_response_t *cptr)
{
    TFM_put_privatedata_t *tfm_priv = nkn_calloc_type(1, sizeof(TFM_put_privatedata_t),
    						mod_fq_TFM_put_privatedata_t);
    if (tfm_priv) {
    	tfm_priv->buf_task_id = id;
    	tfm_priv->attrbuf = cptr->attr;
    	tfm_priv->num_iovecs = cptr->num_nkn_iovecs;
	glob_gm_tfm_put_tasks++;
    }
    return tfm_priv;
}

void free_TFM_put_privatedata_t(TFM_put_privatedata_t *tfm_priv)
{
    free(tfm_priv);
    glob_gm_tfm_put_tasks--;
}

/* Structure associated with the TFM put task */
STATIC
MM_put_data_t *alloc_MM_put_data_t(TFM_put_privatedata_t *tfm_priv, 
				   nkn_cod_t cod, off_t offset, off_t len,
				   off_t total_length)
{
    MM_put_data_t *MM_put_data = nkn_calloc_type(1, sizeof(MM_put_data_t),
    						mod_fq_MM_put_data_t);
    if (MM_put_data) {
#ifdef SEND_UOL_URI
    	MM_put_data->uol.uri = (char *)nkn_cod_get_cnp(cod);
#endif
    	MM_put_data->uol.cod = nkn_cod_dup(cod, NKN_COD_GET_MGR);
    	MM_put_data->uol.offset = offset;
    	MM_put_data->uol.length = len;

	MM_put_data->iov_data_len = tfm_priv->num_iovecs;
	MM_put_data->attr = tfm_priv->attrbuf;
	MM_put_data->iov_data = tfm_priv->content;
	MM_put_data->total_length = total_length;
	MM_put_data->ptype = TFMMgr_provider;
    }
    return MM_put_data;
}

STATIC
void free_MM_put_data_t(MM_put_data_t *MM_put_data)
{
    if (MM_put_data) {
    	if (MM_put_data->uol.cod != NKN_COD_NULL) {
	    nkn_cod_close(MM_put_data->uol.cod, NKN_COD_GET_MGR);
	}
    }
    free(MM_put_data);
}

STATIC
int sched_get_task(pseudo_con_t *pcon)
{
    cache_response_t *cr;
    cr = alloc_cache_response_t(pcon);
    if (!cr) {
    	return 1;
    }
    pcon->con.con.nkn_taskid = nkn_task_create_new_task(
    						TASK_TYPE_CHUNK_MANAGER,
    						TASK_TYPE_GET_MANAGER,
						TASK_ACTION_INPUT,
						0,
						cr,
						sizeof(cache_response_t),
						0 /* No deadline */);
    assert(pcon->con.con.nkn_taskid != -1);
    nkn_task_set_private(TASK_TYPE_GET_MANAGER, pcon->con.con.nkn_taskid, 
    			 (void *)pcon);
    nkn_task_set_state(pcon->con.con.nkn_taskid, TASK_STATE_RUNNABLE);
    glob_gm_total_get_tasks++;

    return 0;
}

STATIC 
int insert_if_unique(pseudo_con_t *pcon)
{
    int rv = 0;
    pseudo_con_t *cur_pcon;

    pthread_mutex_lock(&pcon_head_lock);
    if (PCON_HEAD_ELEMENTS < 0) {
        PCON_HEAD.next = &PCON_HEAD;
        PCON_HEAD.prev = &PCON_HEAD;
	PCON_HEAD_ELEMENTS = 0;
    }

    cur_pcon = PCON_HEAD.next;
    while (cur_pcon != &PCON_HEAD) {
        if (cur_pcon->uol.cod == pcon->uol.cod) {
	    rv = 1;
	    break;
	}
	cur_pcon = cur_pcon->next;
    }

    if (!rv) {
    	PCON_LINK_HEAD(pcon);
    }
    pthread_mutex_unlock(&pcon_head_lock);

    return rv;
}

STATIC
int start_http_get_task(const char *uri, int uri_len, pseudo_con_t *pcon)
{
    char *p;
    char *p2;
    int len;
    int rv;
    const char *host;
    int hostlen;
    u_int32_t attrs;
    int hdrcnt;

    if (!uri || !uri_len || (uri_len <= 7)) {
    	DBG("Invalid args, uri=%p uri_len=%d", uri, uri_len);
    	return 1;
    }

    // uri = http://{host}:{port}/{uri}
    p = memchr(uri+7, '/', uri_len-7);
    if (!p) {
        TSTR(uri, uri_len, p_uri);
    	DBG("Invalid(1) URI=%s", p_uri);
	return 2;
    }
    len = uri_len - (p - uri);

    p2 = memchr(p, '?', len);
    if (p2) {
    	len = p2 - p;
    }

    hostlen=0;
    if (CHECK_HTTP_FLAG(&pcon->con.con.http, HRF_FORWARD_PROXY) ||
        (pcon->flags & PC_FL_VIRTUAL_DOMAIN_REVERSE_PROXY) ||
	(pcon->flags & PC_FL_TRANSPARENT_REVERSE_PROXY)) {
        get_known_header(&pcon->con.con.http.hdr, MIME_HDR_HOST, 
			     &host, &hostlen, &attrs, &hdrcnt);
    }
    if (pcon->uol.cod == NKN_COD_NULL) {
    	char *cache_uri;
	size_t cachelen;
    	cache_uri = nstoken_to_uol_uri(pcon->con.con.http.ns_token, p, len, 
					host, hostlen, &cachelen);
    	if (!cache_uri) {
	    TSTR(uri, uri_len, p_uri);
	    DBG("malloc() failed for uri=%p", p_uri);
	    return 3;
	}
	pcon->uol.cod = nkn_cod_open(cache_uri, cachelen, NKN_COD_GET_MGR);
	if (pcon->uol.cod == NKN_COD_NULL) {
	    DBG("nkn_cod_open(%s) failed", cache_uri);
	    free(cache_uri);
	    return 4;
	}
	free(cache_uri);
    }

    rv = insert_if_unique(pcon);
    if (rv) {
        TSTR(uri, uri_len, p_uri);
    	DBG("Request ignored since GET already pending for uri=%p", p_uri);
	return 5;
    }

    rv = sched_get_task(pcon);
    if (rv) {
    	DBG("sched_get_task() failed, rv=%d", rv);
        return rv+10;
    }
    return 0;
}

STATIC
int sched_tfm_task(TFM_put_privatedata_t *tfm_priv, 
		   nkn_cod_t cod, off_t offset, off_t len, off_t total_length)
{
    MM_put_data_t *MM_put_data;

    MM_put_data = alloc_MM_put_data_t(tfm_priv, cod, offset, len, total_length);
    if (!MM_put_data) {
    	DBG("alloc_MM_put_data_t() failed for uri=%p", nkn_cod_get_cnp(cod));
    	return 1;
    }
    MM_put_data->src_manager = TASK_TYPE_GET_MANAGER_TFM;
    tfm_priv->my_task_id = nkn_task_create_new_task(
    					TASK_TYPE_MEDIA_MANAGER,
					TASK_TYPE_GET_MANAGER_TFM,
					TASK_ACTION_INPUT,
					MM_OP_WRITE,
					MM_put_data,
					sizeof(*MM_put_data),
					0 /* No deadline */ );
    assert(tfm_priv->my_task_id != -1);
    nkn_task_set_private(TASK_TYPE_GET_MANAGER_TFM, tfm_priv->my_task_id, 
    			 (void *)tfm_priv);
    nkn_task_set_state(tfm_priv->my_task_id, TASK_STATE_RUNNABLE);
    glob_gm_total_tfm_put_tasks++;

    return 0;
}

/*
 * GET task completion handler
 * Return:
 *   ret == 0 -> EOF
 *   ret < 0  -> Resume
 *   ret > 0  -> Error
 */
STATIC
int get_task_response_handler(nkn_task_id_t id, cache_response_t *cptr, 
			      pseudo_con_t *pcon)
{
    int n;
    off_t bytes;
    TFM_put_privatedata_t *tfm_priv;
    int rv;

    if (!cptr->uol.offset) {
    	if (!cptr->attr) {
	    DBG("No attributes for uri=%s", nkn_cod_get_cnp(pcon->uol.cod));
	    return 1;
	}
    	/* Get "Content-Length:" from attributes */
	pcon->con.con.http.content_length = cptr->attr->content_length;
	if (!pcon->con.con.http.content_length) {
	    DBG("Zero Content-Length attribute for uri=%s", 
	    	nkn_cod_get_cnp(pcon->uol.cod));
	    return 3;
	}
    }

    /*
     * Setup TFM data and schedule TFM data push
     */
    tfm_priv = alloc_TFM_put_privatedata_t(id, cptr);
    if (!tfm_priv) {
    	DBG("alloc_TFM_put_privatedata_t() failed for uri=%p", 
	    nkn_cod_get_cnp(pcon->uol.cod));
	return 4;
    }

    for (n = 0, bytes = 0; n < cptr->num_nkn_iovecs; n++) {
    	tfm_priv->content[n] = cptr->content[n];
    	bytes += cptr->content[n].iov_len;
    }
    pcon->bytes_read += bytes;
    glob_gm_total_bytes_read += bytes;

    rv = sched_tfm_task(tfm_priv, pcon->uol.cod, pcon->uol.offset, bytes,
    			pcon->con.con.http.content_length);
    if (rv) {
	DBG("sched_tfm_task() failed, rv=%d", rv);
	free_TFM_put_privatedata_t(tfm_priv);
	return 5;
    }

    /*
     * After the initial offset=0 GET, start the next request at
     * 6MB to allow the get manager to prefetch ahead of a sequential client
     * Note the skipped segement in [last_chunk_offset, last_chunk_size]
     */
    if (!cptr->uol.offset && (pcon->con.con.http.content_length >= (6*MiBYTES))) {
	pcon->last_chunk_offset = cptr->uol.offset + bytes;
	pcon->last_chunk_size = (6*MiBYTES) - pcon->last_chunk_offset;

	pcon->uol.offset = (6*MiBYTES) - bytes; // note -bytes, see below
    }

    if (pcon->bytes_read < pcon->con.con.http.content_length) {
	if (!(pcon->flags & PC_FL_SEND_LAST_CHUNK)) {
	    if ((pcon->bytes_read + pcon->last_chunk_size) < 
		pcon->con.con.http.content_length) {
	    	pcon->uol.offset += bytes;
	    	pcon->uol.length = pcon->con.con.http.content_length - 
	    		(pcon->bytes_read + pcon->last_chunk_size);
	    	return -1; // Resume task
	    } else {
	    	pcon->flags |= PC_FL_SEND_LAST_CHUNK;
	    	pcon->uol.offset = pcon->last_chunk_offset;
	    	pcon->uol.length = pcon->last_chunk_size;
	    	return -1; // Resume task
	    }
	} else {
	    pcon->uol.offset += bytes;
	    pcon->uol.length = pcon->last_chunk_size - 
		    (pcon->uol.offset - pcon->last_chunk_offset);
	    return -1; // Resume task
	}
    } else {
    	return 0; // EOF
    }
    return 6; // Should never get here
}

STATIC
int process_request(fqueue_element_t *qe_l)
{
    int rv;

    const char *name;
    int name_len;

    const char *uri;
    int uri_len;

    const char *hdrdata;
    int hdrdata_len;

    const char *nstokendata;
    int nstokendata_len;
    namespace_token_t nstoken;

    const namespace_config_t *nsc = 0;
    pseudo_con_t *pcon = 0;

    const char *http_flag;
    int http_flag_len;

    pcon = alloc_pseudo_con_t();
    if (!pcon) {
    	DBG("alloc_pseudo_con_t() failed, rv=%d", rv);
	rv = 1;
	goto exit;
    }

    rv = get_attr_fqueue_element(qe_l, T_ATTR_URI, 0, &name, &name_len,
    			&uri, &uri_len);
    if (rv) {
    	DBG("get_attr_fqueue_element(T_ATTR_URI) failed, rv=%d", rv);
	rv = 2;
	goto exit;
    }

    if (uri_len >= (MAX_URI_SIZE - MAX_URI_HDR_SIZE)) {
    	DBG("uri_len(%d) >= (MAX_URI_SIZE-MAX_URI_HDR_SIZE)(%d)",
	    uri_len, MAX_URI_SIZE-MAX_URI_HDR_SIZE);
	rv = 3;
	goto exit;
    }

    rv = get_nvattr_fqueue_element_by_name(qe_l, "http_header", 11,
					   &hdrdata, &hdrdata_len);
    if (!rv) {
	rv = deserialize(hdrdata, hdrdata_len, &pcon->con.con.http.hdr, 0, 0);
	if (rv) {
	    DBG("deserialize() failed, rv=%d", rv);
	    rv = 4;
	    goto exit;
	}
    } else {
	DBG("get_nvattr_fqueue_element_by_name() "
	    "for \"http_header\" failed, rv=%d", rv);
    }

    nstokendata = 0;
    rv = get_nvattr_fqueue_element_by_name(qe_l, "namespace_token", 15,
                                    &nstokendata, &nstokendata_len);
    if (nstokendata) {
    	nstoken.u.token = (uint64_t)atol_len(nstokendata, nstokendata_len);
	nsc = namespace_to_config(nstoken);
    }

    if (!nsc) {
        TSTR(uri, uri_len, p_uri);
    	DBG("Invalid namespace token(gen=%d val=%d) "
	    "for URI=%s, request dropped", 
	    nstoken.u.token_data.gen, nstoken.u.token_data.val, p_uri);
	rv = 5;
	goto exit;
    }
    pcon->con.con.http.ns_token = nstoken;

    http_flag = 0;
    rv = get_nvattr_fqueue_element_by_name(qe_l, "http_flag", 9,
                                    &http_flag, &http_flag_len);
    if (http_flag) {
    	if (!strncasecmp(http_flag, "FORWARD_PROXY", http_flag_len)) {
    		pcon->con.con.http.flag |= HRF_FORWARD_PROXY;
	} else if (!strncasecmp(http_flag, "VIRTUAL_DOMAIN_REVERSE_PROXY", 
				http_flag_len)) {
		pcon->flags |= PC_FL_VIRTUAL_DOMAIN_REVERSE_PROXY;
	} else if (!strncasecmp(http_flag, "TRASPARENT_REVERSE_PROXY",
				http_flag_len)) {
		pcon->flags |= PC_FL_TRANSPARENT_REVERSE_PROXY;
	}
    }

    rv = start_http_get_task(uri, uri_len, pcon);
    if (rv) {
        TSTR(uri, uri_len, p_uri);
    	DBG("send_http_request() failed uri=%s, rv=%d", p_uri, rv);
	rv = 6;
	goto exit;
    }

    {
        TSTR(uri, uri_len, p_uri);
    	DBG("Scheduled GET for URI=%s", p_uri);
    }
    return 0;

exit:

    if (pcon) {
    	free_pseudo_con_t(pcon);
    }
    if (nsc) {
    	release_namespace_token_t(nstoken);
    }
    return rv;
}

/*
 ******************************************************************************
 * Get Manager task thread handler for fqueue message dispatch
 ******************************************************************************
 */
STATIC
void *gmgr_req_func(void *arg)
{
    int rv;
    fhandle_t fh;
    fqueue_element_t qe;
    struct pollfd pfd;

    UNUSED_ARGUMENT(arg);
    prctl(PR_SET_NAME, "nvsd-gmgr", 0, 0, 0);

    // Create input fqueue
    do {
    	rv = create_fqueue(gmgr_queuefile, gmgr_queue_maxsize);
	if (rv) {
	    DBG_LOG(SEVERE, MOD_GM,
	    	"Unable to create input queuefile [%s], rv=%d",
	   	gmgr_queuefile, rv);
	    DBG_ERR(SEVERE,
                "Unable to create input queuefile [%s], rv=%d",
                gmgr_queuefile, rv);
	    sleep(10);
	}
    } while (rv);

    // Open input fqueue
    do {
    	fh = open_queue_fqueue(gmgr_queuefile);
	if (fh < 0) {
	    DBG("Unable to open input queuefile [%s]", gmgr_queuefile);
	    sleep(10);
	}
    } while (fh < 0);

    while (1) {
    	rv = dequeue_fqueue_fh_el(fh, &qe, 1);
	if (rv == -1) {
	    continue;
	} else if (rv) {
	    DBG("dequeue_fqueue_fh_el(%s) failed, rv=%d", gmgr_queuefile, rv);
	    poll(&pfd, 0, 100);
	    continue;
	}

	GM_TASK_SEM_WAIT();
	rv = process_request(&qe);
	if (rv) {
	    GM_TASK_SEM_POST();
	    DBG("process_request() failed, rv=%d", rv);
	}
    }
    return 0;
}

/*
 *******************************************************************************
 * Task scheduler interfaces
 *******************************************************************************
 */
STATIC
void get_mgr_input(nkn_task_id_t id)
{
    UNUSED_ARGUMENT(id);
    return;
}

STATIC
void get_mgr_output(nkn_task_id_t id)
{
    cache_response_t *cptr = 0;
    pseudo_con_t *pcon = 0;
    int rv;
    int cleanup_task = 1;

    cptr = (cache_response_t *)nkn_task_get_data(id);
    if (!cptr) {
    	DBG("Null cache_response_t for taskid=0x%lx", id);
	goto term_exit;
    }
    assert(cptr->magic == (int)CACHE_RESP_RESP_MAGIC);

    pcon = (pseudo_con_t *)nkn_task_get_private(TASK_TYPE_GET_MANAGER, id);
    if (!pcon || pcon->con.con.nkn_taskid != id) {
    	DBG("Invalid private data taskid=0x%lx pcon=%p", id, pcon);
	goto term_exit;
    }

    if (cptr->errcode) {
    	DBG("GET task aborted for uri=%s off=%ld len=%ld errcode=%d", 
	    nkn_cod_get_cnp(pcon->uol.cod), pcon->uol.offset, 
	    pcon->uol.length, cptr->errcode);
	glob_gm_total_get_task_err_rets++;
	goto term_exit;
    }

    rv = get_task_response_handler(id, cptr, pcon);
    if (rv >= 0) {
    	if (rv == 0) {
    	    DBG("GET task complete for uri=%s", nkn_cod_get_cnp(pcon->uol.cod));
	    cleanup_task = 0;
	} else {
    	    DBG("GET task handler error for uri=%s off=%ld len=%ld rv=%d",
	    	nkn_cod_get_cnp(pcon->uol.cod), pcon->uol.offset, 
		pcon->uol.length, rv);
	}
	goto term_exit;
    } else {
	rv = sched_get_task(pcon);
	if (rv) {
    	    DBG("GET task continuation failed for uri=%s off=%ld len=%ld rv=%d",
	    	nkn_cod_get_cnp(pcon->uol.cod), pcon->uol.offset, 
		pcon->uol.length, rv);
		goto term_exit;
	}
    	DBG("GET task continuation for uri=%s off=%ld len=%ld", 
	    nkn_cod_get_cnp(pcon->uol.cod), pcon->uol.offset, pcon->uol.length);
    } 

    /* Resume GET task */
    free_cache_response_t(cptr);
    glob_gm_total_get_task_successes++;
    return;

term_exit:

    /* Terminate GET task */
    if (cptr) {
    	free_cache_response_t(cptr);
    }
    release_namespace_token_t(pcon->con.con.http.ns_token);
    free_pseudo_con_t(pcon);
    if (cleanup_task) {
    	nkn_task_set_action_and_state(id, TASK_ACTION_CLEANUP,
					TASK_STATE_RUNNABLE);
	glob_gm_total_get_task_fails++;
    } else {
    	glob_gm_total_get_task_successes++;
    }
    GM_TASK_SEM_POST();
    return;
}

STATIC
void get_mgr_cleanup(nkn_task_id_t id)
{
    UNUSED_ARGUMENT(id);
    return;
}

STATIC
void get_mgr_tfm_input(nkn_task_id_t id)
{
    UNUSED_ARGUMENT(id);
    return;
}

STATIC
void get_mgr_tfm_output(nkn_task_id_t id)
{
    MM_put_data_t *MM_put_data;
    TFM_put_privatedata_t *tfm_priv;
    nkn_task_id_t buf_task_id;

    MM_put_data = nkn_task_get_data(id);
    if (!MM_put_data) {
    	DBG("Null MM_put_data for taskid=0x%lx", id);
	glob_gm_total_tfm_put_fails++;
	return;
    }

    tfm_priv = (TFM_put_privatedata_t *)
    		nkn_task_get_private(TASK_TYPE_GET_MANAGER_TFM, id);
    if (!tfm_priv) {
    	DBG("Invalid private data taskid=0x%lx tfm_priv=%p", id, tfm_priv);
	glob_gm_total_tfm_put_fails++;
	return;
    }

    buf_task_id = tfm_priv->buf_task_id;
    free_MM_put_data_t(MM_put_data);
    free_TFM_put_privatedata_t(tfm_priv);

    // Resume task holding buffers
    nkn_task_set_action_and_state(buf_task_id, TASK_ACTION_CLEANUP,
			TASK_STATE_RUNNABLE);

    // Cleanup this task
    nkn_task_set_action_and_state(id, TASK_ACTION_CLEANUP,
				    TASK_STATE_RUNNABLE);
    glob_gm_total_tfm_put_successes++;
}

STATIC
void get_mgr_tfm_cleanup(nkn_task_id_t id)
{
    UNUSED_ARGUMENT(id);
    return;
}

/*
 *******************************************************************************
 * 		P U B L I C  F U N C T I O N S
 *******************************************************************************
 */
int get_manager_init()
{
    int rv;
    pthread_attr_t attr;
    int stacksize = 128 * KiBYTES;

    rv = sem_init(&gm_task_sem, 0, max_pending_gm_tasks);
    if (rv) {
    	DBG_LOG(SEVERE, MOD_GM, "sem_init() failed, rv=%d errno=%d",
		rv, errno);
	DBG_ERR(SEVERE, "sem_init() failed, rv=%d errno=%d",
                rv, errno);
    }

    nkn_task_register_task_type(TASK_TYPE_GET_MANAGER,
    				&get_mgr_input, 
    				&get_mgr_output, 
			 	&get_mgr_cleanup);

    nkn_task_register_task_type(TASK_TYPE_GET_MANAGER_TFM,
    				&get_mgr_tfm_input, 
    				&get_mgr_tfm_output, 
			 	&get_mgr_tfm_cleanup);

    rv = pthread_attr_init(&attr);
    if (rv) {
    	DBG_LOG(SEVERE, MOD_GM, "pthread_attr_init() failed, rv=%d", rv);
	DBG_ERR(SEVERE, "pthread_attr_init() failed, rv=%d", rv);
	return 0;
    }

    rv = pthread_attr_setstacksize(&attr, stacksize);
    if (rv) {
    	DBG_LOG(SEVERE, MOD_GM, "pthread_attr_setstacksize() failed, rv=%d", 
		rv);
	DBG_ERR(SEVERE, "pthread_attr_setstacksize() failed, rv=%d", rv);
	return 0;
    }

    if ((rv = pthread_create(&gmgr_req_thread_id, &attr, 
    			     gmgr_req_func, NULL))) {
	DBG_LOG(SEVERE, MOD_GM, "pthread_create() failed, rv=%d", rv);
	DBG_ERR(SEVERE, "pthread_create() failed, rv=%d", rv);
	return 0;
    }
    return 1;
}

/*
 * End of get_manager.c
 */
