/*
 * Copyright 1997-2000 by Pawel Krawczyk <kravietz@ceti.pl>
 *
 * See http://www.ceti.com.pl/~kravietz/progs/tacacs.html
 * for details.
 *
 * hdr_check.c  Perform basic sanity checks on received packet.
 */

#include <stdlib.h>
#include <syslog.h>
#include <netinet/in.h>

#include "tacplus.h"
#include "messages.h"
#include "libtac.h"

/*
 * Checks given reply header for possible inconsistencies: 1. reply type
 * other than expected 2. sequence number other than 2 3. session_id
 * different from one sent in request Returns pointer to error message or
 * NULL when the header seems to be correct 
 */
int
_tac_check_header(HDR *th, int type, tacacs_server_t *server,
		  const char **ret_err_msg)
{
    int err = 0;

    if (ret_err_msg == NULL) {
	err = -1;
	goto bail;
    }
    *ret_err_msg = NULL;

    if (th->type != type) {
	syslog(LOG_ERR, "%s: unrelated reply, type %d, expected %d",
	       __FUNCTION__, th->type, type);
	*ret_err_msg = protocol_err_msg;
	err = -1;
    }
    else if (th->seq_no != ++server->seq_no) {
	syslog(LOG_ERR, "%s: Invalid sequence number (got %d, expected %d)",
	       __FUNCTION__, th->seq_no, server->seq_no);
	*ret_err_msg = protocol_err_msg;
	err = -1;
    }
    else if (ntohl(th->session_id) != session_id) {
	syslog(LOG_ERR,
	       "%s: unrelated reply, received session_id %d != sent %d",
	       __FUNCTION__, ntohl(th->session_id), session_id);
	*ret_err_msg = protocol_err_msg;
	err = -1;
    }

 bail:
    return(err);
}
