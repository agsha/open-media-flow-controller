/*
 * Copyright 1997-2000 by Pawel Krawczyk <kravietz@ceti.pl>
 *
 * See http://www.ceti.com.pl/~kravietz/progs/tacacs.html
 * for details.
 *
 * authen_s.c  Send authentication request to the server.
 */

#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/uio.h>

#include "tacplus.h"
#include "libtac.h"
#include "xalloc.h"

/*
 * this function sends a packet do TACACS+ server, asking for validation
 * of given username and password 
 */
int
tac_authen_send(tacacs_conf_t *conf, tacacs_server_t *server,
		const char *user, char *pass, const char *tty, const char *r_addr)
{
    HDR *th; /* TACACS+ packet header */
    struct authen_start as; /* message body */
    struct authen_cont ac;
    int user_len = 0, port_len = 0, pass_len = 0, bodylength, w;
    int r_addr_len = 0;
    int pkt_len = 0;
    u_char *pkt;
    int pkt_type_start;
    int ret = 0;
    struct iovec pkt_vecs[2];

    pkt_type_start = FALSE;
    if (server->seq_no == TAC_PLUS_INIT_SEQNO) {
	/* XXX the whole handling of start vs continue should be better */
	pkt_type_start = TRUE;
    }

    server->seq_no++;
    th = _tac_req_header(TAC_PLUS_AUTHEN, server->seq_no);

    /*
     * set some header options 
     */
    th->version = protocol_version(TAC_PLUS_AUTHEN, TAC_PLUS_AUTHEN_LOGIN,
				   server->auth_type);

    th->encryption = server->secret ? TAC_PLUS_ENCRYPTED : TAC_PLUS_CLEAR;

    TACDEBUG((LOG_DEBUG, "%s: user '%s', pass '%s', tty '%s', rem_addr '%s', encrypt: %s", \
	      __FUNCTION__, user, pass, tty, r_addr, (server->secret) ? "yes" : "no"));

    /* get size of submitted data */
    user_len = strlen(user);
    if (tty) {
	port_len = strlen(tty);
    }
    if (pass) {
	/* oddly enough, if a continue, the pass will be in user */
	pass_len = strlen(pass);
    }
    if (r_addr) {
        r_addr_len = strlen(r_addr);
    }

    if (pkt_type_start) {
	/*
	 * fill the body of message 
	 * port and data length should be 0 on a continue
	 */

	if (server->auth_type == TAC_PLUS_AUTHEN_TYPE_ASCII) {
	    /*
	     * Some Ciscos aren't too happy about receiving the
	     * password in the START packet. So don't send it.
	     */
	    pass_len = 0;
	}

	as.action = TAC_PLUS_AUTHEN_LOGIN;
	as.priv_lvl = TAC_PLUS_PRIV_LVL_MIN;
	as.authen_type = server->auth_type;
	as.service = server->service;
	as.user_len = user_len;
	as.port_len = port_len;
	as.rem_addr_len = r_addr_len;	/* may be e.g Caller-ID in future */
	as.data_len = pass_len;
	bodylength = sizeof(as);
    }
    else {
	/* a continue packet */
	ac.user_msg_len = htons(user_len);
	/* XXX don't support data yet, and ascii exchange doesn't
	 * want any thing in the data field.
	 */
	ac.user_data_len = 0; 
	ac.flags = 0;
	bodylength = TAC_AUTHEN_CONT_FIXED_FIELDS_SIZE;
    }

    /*
     * fill body length in header 
     */
    bodylength += user_len + port_len + pass_len + r_addr_len;

    th->datalength = htonl(bodylength);

    /*
     * build the packet 
     */
    pkt = (u_char *) xcalloc(1, bodylength + 10);

    if (pkt_type_start) {
	bcopy(&as, pkt, sizeof(as));	/* packet body beginning */
	pkt_len += sizeof(as);
    }
    else {
	bcopy(&ac, pkt, TAC_AUTHEN_CONT_FIXED_FIELDS_SIZE);
	pkt_len += TAC_AUTHEN_CONT_FIXED_FIELDS_SIZE;
    }
    bcopy(user, pkt + pkt_len, user_len);	/* user */
    pkt_len += user_len;
    if (tty) {
	bcopy(tty, pkt + pkt_len, port_len);	/* tty */
    }
    pkt_len += port_len;
    if (r_addr) {
        bcopy(r_addr, pkt+pkt_len, r_addr_len);   /* rem addr */
    }
    pkt_len += r_addr_len;
    if (pass && pass_len) {
	bcopy(pass, pkt + pkt_len, pass_len);	/* password */
    }
    pkt_len += pass_len;

    /*
     * pkt_len == bodylength ?XXX
     */
    if (pkt_len != bodylength) {
	TACDEBUG((LOG_DEBUG,
		  "tac_authen_send: bodylength %d != pkt_len %d",
		  bodylength, pkt_len));
    }

    /*
     * encrypt the body 
     */
    _tac_crypt(server->secret, pkt, th, bodylength);

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
	syslog(LOG_ERR, "%s: short write on packet: wrote %d of %d: %m",
	       __FUNCTION__, w, (pkt_len + TAC_PLUS_HDR_SIZE));
	ret = -1;
    }

    free(pkt);
    free(th);

    return (ret);
}				/* tac_authen_send */
