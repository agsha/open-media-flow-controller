/*
 * Copyright 1997-2000 by Pawel Krawczyk <kravietz@ceti.pl>
 *
 * See http://www.ceti.com.pl/~kravietz/progs/tacacs.html
 * for details.
 *
 * tac_account_read  Read accounting reply from server.
 */

#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>

#include "tacplus.h"
#include "libtac.h"
#include "messages.h"
#include "xalloc.h"

int
tac_account_read(tacacs_conf_t *conf, tacacs_server_t *server,
		 const char **ret_err_msg)
{
        int err = 0;
	HDR th;
	struct acct_reply *tb = NULL;
	int len_from_header, r, len_from_body;
	const char *msg = NULL;
	int msg_len, data_len;

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

	reset_server(server, FALSE);

	r=read(conf->sockfd, &th, TAC_PLUS_HDR_SIZE);
	if(r < TAC_PLUS_HDR_SIZE) {
  		syslog(LOG_ERR,
 			"%s: short acct header, %d of %d: %m", __FUNCTION__,
		 	r, TAC_PLUS_HDR_SIZE);
		err = -1;
		*ret_err_msg = r == 0 ? "No response from server" :
                                        "Truncated response from server";
		goto bail;
 	}

 	/* check the reply fields in header */
	err = _tac_check_header(&th, TAC_PLUS_ACCT, server, &msg);
	if (err) {
            *ret_err_msg = msg;
	    goto bail;
	}

 	len_from_header=ntohl(th.datalength);
 	tb=(struct acct_reply *) xcalloc(1, len_from_header);

 	/* read reply packet body */
 	r=read(conf->sockfd, tb, len_from_header);
 	if(r < len_from_header) {
  		syslog(LOG_ERR,
			 "%s: incomplete message body, %d bytes, expected %d: %m",
			 __FUNCTION__,
			 r, len_from_header);
		err = -1;
		*ret_err_msg = r == 0 ? "Empty response message from server" :
                                        "Incomplete response message from server";
		goto bail;
 	}

 	/* decrypt the body */
 	_tac_crypt(server->secret, (u_char *) tb, &th, len_from_header);

	msg_len = tb->msg_len;
	data_len = tb->data_len;

 	/* check the length fields */
 	len_from_body=sizeof(tb->msg_len) + sizeof(tb->data_len) +
            sizeof(tb->status) + msg_len + data_len;

 	if(len_from_header != len_from_body) {
  		syslog(LOG_ERR,
			"%s: invalid reply content, incorrect key?",
			__FUNCTION__);
		err = -1;
		*ret_err_msg = system_err_msg;
		goto bail;
 	}

 	/* save status and clean up */
 	r=tb->status;
	if(msg_len) {
	    server->server_msg=(char *) xcalloc(1, msg_len);
	    bcopy(tb+TAC_ACCT_REPLY_FIXED_FIELDS_SIZE, server->server_msg,
		  msg_len); 
	    *ret_err_msg = server->server_msg;
	}

 	/* server logged our request successfully */
	if(r == TAC_PLUS_ACCT_STATUS_SUCCESS) {
	    TACDEBUG((LOG_DEBUG, "%s: accounted ok", __FUNCTION__));
	    *ret_err_msg = NULL;
	}
	else {
	    /* return pointer to server message */
	    syslog(LOG_ERR, "%s: accounting failed, server reply was %d "
		   "(%s)", __FUNCTION__, r, server->server_msg);
	    if (*ret_err_msg == NULL) {
		*ret_err_msg = "Accounting failed";
	    }
	    err = -1;
	    goto bail;
	}

 bail:
	if (tb) {
	    free(tb);
	}
	return(err);
}
