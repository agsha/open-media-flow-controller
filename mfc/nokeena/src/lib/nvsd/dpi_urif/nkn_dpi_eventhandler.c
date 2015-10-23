/*
 * File: nkn_dpi_eventhandler.c
 *   
 * Author: Jeya ganesh babu
 *
 * (C) Copyright 2014 Juniper Networks, Inc.
 * All rights reserved.
 */

#ifndef _STUB_
unsigned int glob_dpi_uri_filter_size;

#else /* _STUB_ */

/* Include Files */
#include <stdio.h>
#include <string.h>
#include <libwrapper/uwrapper.h>

#include "nkn_dpi_netfilter.h"
#include "nkn_dpi_priv.h"
#include "nkn_dpi_eventhandler.h"
#include "dpi_urlfilter.h"
#include "nkn_http.h"

uint32_t glob_dpi_uri_filter_size;

/* 
 * Handler for the start of a request 
 * sesn_data initialization done here
 */
static void
http_request_handler(uapp_cnx_t *uapp_cnx,
				const uevent_t *ev __attribute((unused)),
				void *param __attribute((unused)))
{
    http_dpi_sesn_data_t *sesn_data;

    if (uapp_cnx->ucnx_type == CLEP_CNX_SERVER) {
	/* HTTP Response */
    } else {
	/* HTTP Request */
	if (!uapp_cnx->user_handle) {
	    sesn_data = nkn_calloc_type(1, sizeof(http_dpi_sesn_data_t),
					mod_dpi_sesn_data_t);
	    uapp_cnx->user_handle = sesn_data;
	} else {
	    sesn_data = uapp_cnx->user_handle;
	}
	/* If its a pipeline request and next request is in the same
	 * packet and if the existing action is reject, dont reset
	 * the status
	 */
	if (!((sesn_data->status & DPI_SESN_HTTP_PIPECHK) &&
		    (sesn_data->status & DPI_SESN_HTTP_LS_REJ)))
	    SET_HTTP_SESN_START(sesn_data);
    }

    return;
}

/* 
 * Handler for the uri
 */
static void
http_uri_handler(uapp_cnx_t *uapp_cnx,
				const uevent_t *ev __attribute((unused)),
				void *param __attribute((unused)))
{
    http_dpi_sesn_data_t *sesn_data;
    namespace_uf_reject_t fil_status;

    if (!uapp_cnx->user_handle) {
	sesn_data = nkn_calloc_type(1, sizeof(http_dpi_sesn_data_t),
				    mod_dpi_sesn_data_t);
	uapp_cnx->user_handle = sesn_data;
	SET_HTTP_SESN_START(sesn_data);
    } else {
	sesn_data = uapp_cnx->user_handle;
    }

    if (unlikely(!(sesn_data->status &
		    (DPI_SESN_HTTP_LS_REJ | DPI_SESN_HTTP_LS_ACP)))) {
	sesn_data->uri = nkn_calloc_type(1, (ev->len + 1), mod_dpi_sesn_data_t);
	memcpy(sesn_data->uri, ev->data, ev->len);
	sesn_data->ulen = ev->len;
	if (unlikely(glob_dpi_uri_filter_size &&
		    ev->len > glob_dpi_uri_filter_size)) {
	    SET_HTTP_SESN_REJ(sesn_data);
	} else if (unlikely(sesn_data->uri &&
			    sesn_data->method && sesn_data->host)) {
	    fil_status = DPI_urlfilter_lookup(sesn_data->method,
					      sesn_data->host, sesn_data->hlen,
					      sesn_data->uri, sesn_data->ulen);
	    if (fil_status == NS_UF_REJECT_NOACTION) {
		SET_HTTP_SESN_ACP(sesn_data);
	    } else {
		SET_HTTP_SESN_REJ(sesn_data);
	    }
	}
    }
    return;
}

/* 
 * Handler for the method
 */
static void
http_method_handler(uapp_cnx_t *uapp_cnx,
				const uevent_t *ev __attribute((unused)),
				void *param __attribute((unused)))
{
    http_dpi_sesn_data_t *sesn_data;
    namespace_uf_reject_t fil_status;
    uint32_t i;

    if (!uapp_cnx->user_handle) {
	sesn_data = nkn_calloc_type(1, sizeof(http_dpi_sesn_data_t),
				    mod_dpi_sesn_data_t);
	uapp_cnx->user_handle = sesn_data;
	SET_HTTP_SESN_START(sesn_data);
    } else {
	sesn_data = uapp_cnx->user_handle;
    }

    switch((*(unsigned int *)ev->data) | 0x20202020) {
	case sig_get:
	    sesn_data->method = HRF_METHOD_GET;
	    break;

	case sig_head:
	    sesn_data->method = HRF_METHOD_HEAD;
	    break;

	case sig_post:
	    sesn_data->method = HRF_METHOD_POST;
	    break;

	case sig_options:
	    sesn_data->method = HRF_METHOD_OPTIONS;
	    break;

	case sig_put:
	    sesn_data->method = HRF_METHOD_PUT;
	    break;

	case sig_delete:
	    sesn_data->method = HRF_METHOD_DELETE;
	    break;

	case sig_trace:
	    sesn_data->method = HRF_METHOD_TRACE;
	    break;

	case sig_connect:
	    sesn_data->method = HRF_METHOD_CONNECT;
	    break;

	default:
	    SET_HTTP_SESN_REJ(sesn_data);
    }

    if (unlikely(!(sesn_data->status &
		    (DPI_SESN_HTTP_LS_REJ | DPI_SESN_HTTP_LS_ACP)))) {
	if (unlikely(sesn_data->uri && sesn_data->method && sesn_data->host)) {
	    fil_status = DPI_urlfilter_lookup(sesn_data->method,
					      sesn_data->host, sesn_data->hlen,
					      sesn_data->uri, sesn_data->ulen);
	    if (fil_status == NS_UF_REJECT_NOACTION) {
		SET_HTTP_SESN_ACP(sesn_data);
	    } else {
		SET_HTTP_SESN_REJ(sesn_data);
	    }
	}
    }
    return;
}

/* 
 * Handler for the host header
 */
static void
http_host_handler(uapp_cnx_t *uapp_cnx,
				const uevent_t *ev __attribute((unused)),
				void *param __attribute((unused)))
{
    http_dpi_sesn_data_t *sesn_data;
    namespace_uf_reject_t fil_status;

    if (uapp_cnx->ucnx_type == CLEP_CNX_SERVER) {
	/* HTTP Response */
	return;
    }

    if (!uapp_cnx->user_handle) {
	sesn_data = nkn_calloc_type(1, sizeof(http_dpi_sesn_data_t),
				    mod_dpi_sesn_data_t);
	uapp_cnx->user_handle = sesn_data;
	SET_HTTP_SESN_START(sesn_data);
    } else {
	sesn_data = uapp_cnx->user_handle;
    }

    if (unlikely(!(sesn_data->status &
		    (DPI_SESN_HTTP_LS_REJ | DPI_SESN_HTTP_LS_ACP)))) {
	sesn_data->host = nkn_calloc_type(1, (ev->len + 1),
					  mod_dpi_sesn_data_t);
	memcpy(sesn_data->host, ev->data, ev->len);
	sesn_data->hlen = ev->len;
	if (likely(sesn_data->uri && sesn_data->method && sesn_data->host)) {
	    fil_status = DPI_urlfilter_lookup(sesn_data->method,
					      sesn_data->host, sesn_data->hlen,
					      sesn_data->uri, sesn_data->ulen);
	    if (fil_status == NS_UF_REJECT_NOACTION) {
		SET_HTTP_SESN_ACP(sesn_data);
	    } else {
		SET_HTTP_SESN_REJ(sesn_data);
	    }
	}
    }
    return;
}

/* 
 * Handler for the end of the header
 */
static void
http_request_end_handler(uapp_cnx_t *uapp_cnx,
				const uevent_t *ev __attribute((unused)),
				void *param __attribute((unused)))
{
    http_dpi_sesn_data_t *sesn_data;
    sesn_data = uapp_cnx->user_handle;

    if (sesn_data)
	SET_HTTP_SESN_END(sesn_data);
    return;
}

/*
 * Register all the intereste HTTP attributes with their callbak functions
 * Open the accesslog file
 */
void
event_handler_init(void)
{
    uevent_hooks_add_parm(Q_PROTO_HTTP, Q_HTTP_REQUEST,
                            http_request_handler, NULL);
    uevent_hooks_add_parm(Q_PROTO_HTTP, Q_HTTP_METHOD,
                            http_method_handler, NULL);
    uevent_hooks_add_parm(Q_PROTO_HTTP, Q_HTTP_URI_DECODED,
                            http_uri_handler, NULL);
    uevent_hooks_add_parm(Q_PROTO_HTTP, Q_HTTP_HOST,
                            http_host_handler, NULL);
    uevent_hooks_add_parm(Q_PROTO_HTTP, Q_HTTP_HEADER_END,
                            http_request_end_handler, NULL);
} /* end of event_handler_init() */


/*
 * Even handler cleanup
 * TODO: to be called during cleanup
 */
void
event_handler_cleanup(void)
{
    uevent_hooks_remove_parm(Q_PROTO_HTTP, Q_HTTP_REQUEST,
                            http_request_handler, NULL);
    uevent_hooks_remove_parm(Q_PROTO_HTTP, Q_HTTP_METHOD,
                            http_method_handler, NULL);
    uevent_hooks_remove_parm(Q_PROTO_HTTP, Q_HTTP_URI_DECODED,
                            http_uri_handler, NULL);
    uevent_hooks_remove_parm(Q_PROTO_HTTP, Q_HTTP_HOST,
                            http_host_handler, NULL);
    uevent_hooks_remove_parm(Q_PROTO_HTTP, Q_HTTP_HEADER_END,
                            http_request_end_handler, NULL);
} /* end of event_handler_cleanup() */

/*
 * Return session status to the NKN DPI module
 * NF verdict is set based on this result
 */
inline int
nkn_dpi_eventhandler_get_sess_status(void *ucnx, uint32_t src, uint32_t dst)
{
    uapp_cnx_t *uapp_cnx = (uapp_cnx_t *)ucnx;
    int ret_val = 0;
    http_dpi_sesn_data_t *sesn_data;

    src=src;
    dst=dst;
    if (uapp_cnx) {
	sesn_data = uapp_cnx->user_handle;
	if (sesn_data) {
	    /* Current packet parse complete.
	     * Reset pipeline check flag 
	     */
	    RESET_HTTP_SESN_PIPECHK(sesn_data);

	    /* Send Rej/Accept Status */
	    ret_val =  (sesn_data->status & (DPI_SESN_HTTP_LS_REJ |
			DPI_SESN_HTTP_LS_ACP));
	    /* Session is complete, the session data can be freed up */
	    if (sesn_data->status & DPI_SESN_HTTP_END) {
		if (sesn_data->status & DPI_SESN_HTTP_LS_REJ)
		    dpi_accesslog_write(sesn_data->uri, sesn_data->ulen,
					sesn_data->host, sesn_data->hlen,
					sesn_data->method, src,
					dst, NKN_UF_DROP_PKTS);
		else if (sesn_data->status & DPI_SESN_HTTP_LS_ACP)
		    dpi_accesslog_write(sesn_data->uri, sesn_data->ulen,
					sesn_data->host, sesn_data->hlen,
					sesn_data->method, src,
					dst, 0);
		if (sesn_data->uri)
		    free(sesn_data->uri);
		if (sesn_data->host)
		    free(sesn_data->host);
		free(sesn_data);
		uapp_cnx->user_handle = NULL;
	    }
	} else {
	    ret_val =  DPI_SESN_HTTP_UNK_ACP;
	}
    }
    return ret_val;
}

/*
 * Add qh, pkt_id to session data
 */
inline int
nkn_dpi_eventhandler_link_pfpkt_to_session(void *ucnx,
					   void *ring,
					   int len,
					   unsigned char *buffer,
					   uint32_t ip_src,
					   uint32_t ip_dst,
					   uint64_t thread_id)
{
    uapp_cnx_t *uapp_cnx = (uapp_cnx_t *)ucnx;
    int ret_val = -1;
    http_dpi_sesn_data_t *sesn_data;
    struct dpi_sesn_nf_data *nf_data;

    if (uapp_cnx) {
	sesn_data = uapp_cnx->user_handle;
	if (sesn_data) {
	    sesn_data->ip_src = ip_src;
	    sesn_data->ip_dst = ip_dst;
	    sesn_data->tid = (int)thread_id;
	    nf_data = nkn_calloc_type(1, sizeof(struct dpi_sesn_nf_data),
				      mod_dpi_sesn_data_t);
	    nf_data->queue  = ring;
	    nf_data->buffer = nkn_malloc_type(len, mod_dpi_sesn_data_t);
	    memcpy(nf_data->buffer, buffer, len);
	    nf_data->pkt_id = len;
	    nf_data->next   = NULL;
	    if (sesn_data->nf_data) {
		if (sesn_data->nf_last) {
		    sesn_data->nf_last->next = nf_data;
		    sesn_data->nf_last = nf_data;
		}
	    } else {
		sesn_data->nf_data = nf_data;
		sesn_data->nf_last = nf_data;
	    }
	    ret_val = 0;
	}
    }
    return ret_val;
}

/*
 * Add qh, pkt_id to session data
 */
inline int
nkn_dpi_eventhandler_link_pkt_to_session(void *ucnx,
					 struct nfq_q_handle *queue,
					 int pkt_id,
					 uint32_t ip_src,
					 uint32_t ip_dst,
					 uint64_t thread_id)
{
    uapp_cnx_t *uapp_cnx = (uapp_cnx_t *)ucnx;
    int ret_val = -1;
    http_dpi_sesn_data_t *sesn_data;
    struct dpi_sesn_nf_data *nf_data;

    if (uapp_cnx) {
	sesn_data = uapp_cnx->user_handle;
	if (sesn_data) {
	    sesn_data->ip_src = ip_src;
	    sesn_data->ip_dst = ip_dst;
	    sesn_data->tid = (int)thread_id;
	    nf_data = nkn_calloc_type(1, sizeof(struct dpi_sesn_nf_data),
				      mod_dpi_sesn_data_t);
	    nf_data->queue  = queue;
	    nf_data->buffer = NULL;
	    nf_data->pkt_id = pkt_id;
	    nf_data->next   = NULL;
	    if (sesn_data->nf_data) {
		if (sesn_data->nf_last) {
		    sesn_data->nf_last->next = nf_data;
		    sesn_data->nf_last = nf_data;
		}
	    } else {
		sesn_data->nf_data = nf_data;
		sesn_data->nf_last = nf_data;
	    }
	    ret_val = 0;
	}
    }
    return ret_val;
}

#endif /* _STUB_ */

/* End of event_handler.c */
