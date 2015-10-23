/*
 * Copyright 1997-2000 by Pawel Krawczyk <kravietz@ceti.pl>
 *
 * See http://www.ceti.com.pl/~kravietz/progs/tacacs.html
 * for details.
 *
 * tac_account_send  Send accounting event information to server.
 *
 */

/*
 * NOTE: This file has been modified by Tall Maple Systems, Inc. 
 *
 * The changes are:
 *
 *  - Support for the standard rem-addr header field was added
 *
 */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <syslog.h>
#include <sys/uio.h>

#include "tacplus.h"
#include "libtac.h"
#include "xalloc.h"

int
tac_account_send(tacacs_conf_t * conf, tacacs_server_t * server,
		 int type, char *user, const char *tty, const char *rem_addr,
                 struct tac_attrib *attr)
{
    HDR            *th;
    struct acct     tb;
    u_char          user_len,
                    port_len,
                    rem_addr_len;

    struct tac_attrib *a;
    int             i = 0;	/* arg count */
    int             pkt_len = 0;
    int             pktl = 0;
    int             w;		/* write count */
    u_char         *pkt;
    u_char         *pktp;
    int             ret = 0;
    struct iovec pkt_vecs[2];

    server->seq_no++;
    th = _tac_req_header(TAC_PLUS_ACCT, server->seq_no);

    /*
     * set header options 
     */
    th->version = protocol_version(TAC_PLUS_ACCT, AUTHEN_METH_TACACSPLUS,
				   server->auth_type);

    th->encryption = server->secret ? TAC_PLUS_ENCRYPTED : TAC_PLUS_CLEAR;

    TACDEBUG((LOG_DEBUG, "%s: user '%s', tty '%s', encrypt: %s, type: %s",
	      __FUNCTION__, user, tty,
	      (server->secret) ? "yes" : "no",
	      (type == TAC_PLUS_ACCT_FLAG_START) ? "START" : "STOP"))

	user_len = (u_char) strlen(user);
    port_len = (u_char) strlen(tty);
    rem_addr_len = rem_addr ? ((u_char) strlen(rem_addr)) : 0;

    tb.flags = (u_char) type;
    tb.authen_method = AUTHEN_METH_TACACSPLUS;
    tb.priv_lvl = TAC_PLUS_PRIV_LVL_MIN;
    tb.authen_type = server->auth_type;
    tb.authen_service = server->service;
    tb.user_len = user_len;
    tb.port_len = port_len;
    tb.rem_addr_len = rem_addr_len;

    /*
     * allocate packet 
     */
    pkt = (u_char *) xcalloc(1, TAC_ACCT_REQ_FIXED_FIELDS_SIZE);
    pkt_len = sizeof(tb);

    /*
     * fill attribute length fields 
     */
    a = attr;
    while (a) {

	pktl = pkt_len;
	pkt_len += sizeof(a->attr_len);
	pkt = xrealloc(pkt, pkt_len);

	/*
	 * see comments in author_s.c pktp=pkt + pkt_len; pkt_len +=
	 * sizeof(a->attr_len); pkt = xrealloc(pkt, pkt_len); 
	 */

	bcopy(&a->attr_len, pkt + pktl, sizeof(a->attr_len));
	i++;

	a = a->next;
    }

    /*
     * fill the arg count field and add the fixed fields to packet 
     */
    tb.arg_cnt = i;
    bcopy(&tb, pkt, TAC_ACCT_REQ_FIXED_FIELDS_SIZE);

    /*
     * #define PUTATTR(data, len) \ pktp = pkt + pkt_len; \ pkt_len +=
     * len; \ pkt = xrealloc(pkt, pkt_len); \ bcopy(data, pktp, len); 
     */
#define PUTATTR(data, len) \
	pktl = pkt_len; \
	pkt_len += len; \
	pkt = xrealloc(pkt, pkt_len); \
	memmove(pkt + pktl, data, len);

    /*
     * fill user and port fields 
     */
    PUTATTR(user, user_len)
	PUTATTR(tty, port_len)
    if (rem_addr_len) {
	    PUTATTR(rem_addr, rem_addr_len)
    }

	/*
	 * fill attributes 
	 */
	a = attr;
    while (a) {
	PUTATTR(a->attr, a->attr_len)

	    a = a->next;
    }

    /*
     * finished building packet, fill len_from_header in header 
     */
    th->datalength = htonl(pkt_len);

    /*
     * encrypt packet body 
     */
    _tac_crypt(server->secret, pkt, th, pkt_len);

    /*
     * Make one call to writev to at least attempt to get everything
     * in one packet, which makes network traces easier.
     */
    pkt_vecs[0].iov_base = th;
    pkt_vecs[0].iov_len = TAC_PLUS_HDR_SIZE;
    pkt_vecs[1].iov_base = pkt;
    pkt_vecs[1].iov_len = pkt_len;
    w = writev(conf->sockfd, pkt_vecs, 2);
    if (w < 0 || w < (pkt_len + TAC_PLUS_HDR_SIZE)) {
	syslog(LOG_ERR, "%s: short write on acct packet: wrote %d of %d: %m",
	       __FUNCTION__, w, (pkt_len + TAC_PLUS_HDR_SIZE));
	ret = -1;
    }

    free(pkt);
    free(th);

    return (ret);
}
