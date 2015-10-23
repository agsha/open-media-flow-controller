/* File : nkn_dpi_priv.h
 * Copyright (C) 2014 Juniper Networks, Inc. 
 * All rights reserved.
 */

/**
 * @file nkn_dpi_priv.h
 * @brief Utility functions for netfilter related functions
 * @author
 * @version 1.00
 * @date 2014-04-10
 */
#ifndef _NKN_DPI_PRIV_H
#define _NKN_DPI_PRIV_H

#include <atomic_ops.h>
#ifdef NFQUEUE_URI_FILTER
#include <linux/netfilter.h>
#include <libnetfilter_queue/libnetfilter_queue.h>
#endif
#include "accesslog_pub.h"
#include "nkn_defs.h"
#include "nkn_errno.h"
#include "nkn_debug.h"
#include "nkn_lockstat.h"
#include "queue.h"
#include "nkn_pfbridge.h"

/* HTTP METHODS */
#define sig_get	    0x20746567 // "get "
#define sig_head    0x64616568 // "head"
#define sig_post    0x74736f70 // "post"
#define sig_connect 0x6e6e6f63 // "conn"
#define sig_options 0x6974706f // "opti"
#define sig_put	    0x20747570 // "put "
#define sig_delete  0x656c6564 // "dele"
#define sig_trace   0x63617274 // "trac"

/* DPI Session Data Status */
#define DPI_SESN_HTTP_START    0x0001
#define DPI_SESN_HTTP_END      0x0002
#define DPI_SESN_HTTP_LS_REJ   0x0004
#define DPI_SESN_HTTP_LS_ACP   0x0008
#define DPI_SESN_HTTP_UNK_ACP  0x0010
#define DPI_SESN_HTTP_PIPECHK  0x0020

#ifdef NFQUEUE_URI_FILTER
#define SET_HTTP_SESN_END(x)   {		    \
    x->status &= ~DPI_SESN_HTTP_START;		    \
    x->status |= DPI_SESN_HTTP_END;		    \
    if (!(x->status & (DPI_SESN_HTTP_LS_REJ |	    \
		    DPI_SESN_HTTP_LS_ACP)))	    \
	x->status |= DPI_SESN_HTTP_LS_REJ;	    \
    if (x->nf_data) {				    \
	struct dpi_sesn_nf_data *p;		    \
	struct dpi_sesn_nf_data *op;		    \
	p = x->nf_data;				    \
	if (x->status & DPI_SESN_HTTP_LS_ACP)	    \
	    dpi_accesslog_write(x->uri,		    \
				x->ulen,	    \
				x->host,	    \
				x->hlen,	    \
				x->method,	    \
				x->ip_src,	    \
				x->ip_dst,	    \
				0);		    \
	else					    \
	    dpi_accesslog_write(x->uri,		    \
				x->ulen,	    \
				x->host,	    \
				x->hlen,	    \
				x->method,	    \
				x->ip_src,	    \
				x->ip_dst,	    \
				NKN_UF_DROP_PKTS);  \
	while(p) {				    \
	    if (x->status & DPI_SESN_HTTP_LS_REJ){  \
		if (p->buffer) {		    \
		    nkn_pf_reject(p->buffer,	    \
				  p->pkt_id,	    \
				  p->queue);	    \
		    free(p->buffer);		    \
		} else	{			    \
		    nfq_set_verdict2(p->queue,	    \
				    p->pkt_id,	    \
				    NF_ACCEPT,	    \
				    0xFFFFFFFF,	    \
				    0, NULL);	    \
		}				    \
		AO_fetch_and_add1(&nkn_dpi_filter_rej_cnt[x->tid]); \
	    } else {				    \
		if (p->buffer) {		    \
		    nkn_pf_accept(p->buffer,	    \
				  p->pkt_id,	    \
				  p->queue);	    \
		    free(p->buffer);		    \
		} else	{			    \
		    nfq_set_verdict(p->queue,	    \
				    p->pkt_id,	    \
				    NF_ACCEPT,	    \
				    0, NULL);	    \
		}				    \
		AO_fetch_and_add1(&nkn_dpi_filter_acp_cnt[x->tid]); \
	    }					    \
	    op = p;				    \
	    p = p->next;			    \
	    free(op);				    \
	}					    \
	x->nf_data = NULL;			    \
	x->nf_last = NULL;			    \
    }						    \
}
#else
/* Session handler Macros */
#define SET_HTTP_SESN_END(x)   {		    \
    x->status &= ~DPI_SESN_HTTP_START;		    \
    x->status |= DPI_SESN_HTTP_END;		    \
    if (!(x->status & (DPI_SESN_HTTP_LS_REJ |	    \
		    DPI_SESN_HTTP_LS_ACP)))	    \
	x->status |= DPI_SESN_HTTP_LS_REJ;	    \
    if (x->nf_data) {				    \
	struct dpi_sesn_nf_data *p;		    \
	struct dpi_sesn_nf_data *op;		    \
	p = x->nf_data;				    \
	while(p) {				    \
	    if (x->status & DPI_SESN_HTTP_LS_REJ){  \
		if (p->buffer) {		    \
		    nkn_pf_reject(p->buffer,	    \
				  p->pkt_id,	    \
				  p->queue);	    \
		    free(p->buffer);		    \
		}				    \
		AO_fetch_and_add1(&nkn_dpi_filter_rej_cnt[x->tid]); \
	    } else {				    \
		if (p->buffer) {		    \
		    nkn_pf_accept(p->buffer,	    \
				  p->pkt_id,	    \
				  p->queue);	    \
		    free(p->buffer);		    \
		}				    \
		AO_fetch_and_add1(&nkn_dpi_filter_acp_cnt[x->tid]); \
	    }					    \
	    op = p;				    \
	    p = p->next;			    \
	    free(op);				    \
	}					    \
	x->nf_data = NULL;			    \
	x->nf_last = NULL;			    \
    }						    \
}
#endif

#define SET_HTTP_SESN_START(x) {	\
    SET_HTTP_SESN_END(x);		\
    x->status = DPI_SESN_HTTP_START;	\
    x->status |= DPI_SESN_HTTP_PIPECHK;	\
}

#define RESET_HTTP_SESN_PIPECHK(x) {	\
    x->status &= ~DPI_SESN_HTTP_PIPECHK;\
}

#define SET_HTTP_SESN_ACP(x)   {	\
    x->status |= DPI_SESN_HTTP_LS_ACP;  \
    SET_HTTP_SESN_END(x);		\
}

#define SET_HTTP_SESN_REJ(x)   {	    \
    x->status |= DPI_SESN_HTTP_LS_REJ;	    \
    SET_HTTP_SESN_END(x);		    \
}

#if 0
#define SET_HTTP_SESN_ACP(x)   {	\
    x->status |= DPI_SESN_HTTP_LS_ACP;  \
    dpi_accesslog_write(x->uri,		\
			x->ulen,	\
			x->host,	\
			x->hlen,	\
			x->method,	\
			x->ip_src,	\
			x->ip_dst,	\
			0);		\
    SET_HTTP_SESN_END(x);		\
}

#define SET_HTTP_SESN_REJ(x)   {	    \
    x->status |= DPI_SESN_HTTP_LS_REJ;	    \
    dpi_accesslog_write(x->uri,		    \
			x->ulen,	    \
			x->host,	    \
			x->hlen,	    \
			x->method,	    \
			x->ip_src,	    \
			x->ip_dst,	    \
			NKN_UF_DROP_PKTS);  \
    SET_HTTP_SESN_END(x);		    \
}
#endif

/*
 * Session packet info list.
 * This will be used when the header is split into multiple packets
 */
struct dpi_sesn_nf_data {
    struct nfq_q_handle *queue;
    unsigned char *buffer;
    int pkt_id;
    struct dpi_sesn_nf_data *next;
};

/*
 * Session data structure
 */
typedef struct http_dpi_sesn_data_st {
    char *uri;
    char *host;
    uint64_t method;
    uint32_t ip_src; /* For log */
    uint32_t ip_dst; /* For log */
    int ulen;
    int hlen;
    int status;
    int tid;
    struct dpi_sesn_nf_data *nf_data;
    struct dpi_sesn_nf_data *nf_last;
}http_dpi_sesn_data_t;

extern uint64_t nkn_dpi_filter_rej_cnt[MAX_NFQUEUE_THREADS];
extern uint64_t nkn_dpi_filter_acp_cnt[MAX_NFQUEUE_THREADS];

#endif /* _NKN_DPI_PRIV_H */

