/*
 * Copyright 1997-2000 by Pawel Krawczyk <kravietz@ceti.pl>
 *
 * See http://www.ceti.com.pl/~kravietz/progs/tacacs.html
 * for details.
 *
 * authen_r.c  Read authentication reply from server.
 */

#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>

#include "tacplus.h"
#include "libtac.h"
#include "messages.h"
#include "xalloc.h"

/*
 * reads packet from TACACS+ server; returns: NULL if the authentication
 * succeded string pointer if it failed 
 */
int
tac_authen_read(tacacs_conf_t *conf, tacacs_server_t *server, 
		const char **ret_err_msg, int *ret_status)
{
    HDR th;
    struct authen_reply *tb = NULL;
    int len_from_header, r, len_from_body;
    int serv_msg_len, data_len;
    char *msg = NULL;
    int err = 0;

    if (ret_status) {
        *ret_status = TAC_PLUS_AUTHEN_STATUS_ERROR;
    }
    if (ret_err_msg == NULL) {
	syslog(LOG_ERR, "%s: error no err msg buffer provided",
	       __FUNCTION__);
	err = -1;
	goto bail;
    }
    *ret_err_msg = NULL;

    if (server == NULL) {
	syslog(LOG_ERR, "%s: error no TACACS+ server specified",
	       __FUNCTION__);
	err = -1;
	*ret_err_msg = no_server_err_msg;
	goto bail;
    }

    /* Free from any previous server messages */
    reset_server(server, FALSE);

    /*
     * read the reply header 
     * XXX it would appear that the reads are being done on non-blocking
     * sockets. This should be fixed to deal with partial reads.
     */
    r = read(conf->sockfd, &th, TAC_PLUS_HDR_SIZE);
    if (r < TAC_PLUS_HDR_SIZE) {
	syslog(LOG_ERR,
	       "%s: error reading authen header, read %d of %d: %m",
	       __FUNCTION__, r, TAC_PLUS_HDR_SIZE);
	*ret_err_msg = system_err_msg;
	err = -1;
	goto bail;
    }

    /* check the reply fields in header */
    err = _tac_check_header(&th, TAC_PLUS_AUTHEN, server, ret_err_msg);
    if (err) {
	goto bail;
    }

    len_from_header = ntohl(th.datalength);
    tb = (struct authen_reply *) xcalloc(1, len_from_header);

    /* read reply packet body */
    r = read(conf->sockfd, tb, len_from_header);
    if (r < len_from_header) {
	syslog(LOG_ERR,
	       "%s: incomplete message body, %d bytes, expected %d: %m",
	       __FUNCTION__, r, len_from_header);
	*ret_err_msg = system_err_msg;
	err = -1;
	goto bail;
    }

    /* decrypt the body */
    _tac_crypt(server->secret, (u_char *) tb, &th, len_from_header);

    serv_msg_len = ntohs(tb->msg_len);
    data_len = ntohs(tb->data_len);

    /* check the length fields */
    len_from_body = sizeof(tb->status) + sizeof(tb->flags) +
	sizeof(tb->msg_len) + sizeof(tb->data_len) +
	serv_msg_len + data_len;

    if (len_from_header != len_from_body) {
	syslog(LOG_ERR,
	       "%s: invalid reply content, incorrect key? (calc %d, hdr %d)",
	       __FUNCTION__, len_from_body, len_from_header);
	*ret_err_msg = system_err_msg;
	err = -1;
	goto bail;
    }

    /* Save off any message or data returned by the server */
    if (serv_msg_len) {
	server->server_msg = (char *) xcalloc(1, serv_msg_len);
	bcopy(tb + TAC_AUTHEN_REPLY_FIXED_FIELDS_SIZE, server->server_msg,
	      serv_msg_len);
	/* Show user what the server said */
	*ret_err_msg = server->server_msg;
    }
    if (data_len) {
	server->data = (char *) xcalloc(1, data_len);
	bcopy(tb + TAC_AUTHEN_REPLY_FIXED_FIELDS_SIZE + serv_msg_len,
	      server->data, data_len);
    }

    /* save status and clean up */
    r = tb->status;
    if (ret_status) {
	*ret_status = r;
    }

    switch (r) {
    case TAC_PLUS_AUTHEN_STATUS_PASS:
	/* server authenticated username and password successfully */
	break;

    case TAC_PLUS_AUTHEN_STATUS_FAIL:
	if (*ret_err_msg == NULL) {
	    *ret_err_msg = auth_fail_msg;
	}
	err = -1;
	break;

    case TAC_PLUS_AUTHEN_STATUS_GETDATA:
    case TAC_PLUS_AUTHEN_STATUS_GETUSER:
    case TAC_PLUS_AUTHEN_STATUS_GETPASS:
	break;

    case TAC_PLUS_AUTHEN_STATUS_ERROR:
	syslog(LOG_ERR,
	       "%s: TACACS+ authentication failed, server detected error",
	   __FUNCTION__);
	err = -1;
	break;

    case TAC_PLUS_AUTHEN_STATUS_RESTART:
    case TAC_PLUS_AUTHEN_STATUS_FOLLOW:
    default:
	syslog(LOG_ERR,
	       "%s: TACACS+ authentication failed, unexpected status %#x",
	   __FUNCTION__, r);
	if (*ret_err_msg == NULL) {
	    *ret_err_msg = unexpected_status_msg;
	}
	err = -1;
	break;
    }

 bail:
    if (err && (*ret_err_msg == NULL)) {
	/* At least say something bad */
	*ret_err_msg = bad_login_msg;
    }
    if (tb) {
	free(tb);
	tb = NULL;
    }
    return(err);
}
