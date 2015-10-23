/*
 * Copyright 1997-2000 by Pawel Krawczyk <kravietz@ceti.pl>
 *
 * See http://www.ceti.com.pl/~kravietz/progs/tacacs.html
 * for details.
 *
 * author_r.c  Reads authorization reply from the server.
 */

#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>

#include "tacplus.h"
#include "xalloc.h"
#include "libtac.h"
#include "messages.h"

/* This function returns structure containing 
    1. status (granted/denied)
    2. message for the user
    3. list of attributes returned by server
   The attributes should be applied to service authorization
   is requested for, but actually the aren't. Attributes are
   discarded. 
*/
void tac_author_read(tacacs_conf_t *conf, tacacs_server_t *server,
		     struct areply *re) {
	HDR th;
	int err = 0;
	struct author_reply *tb = NULL;
	int len_from_header, r, len_from_body;
	char *pktp;
	const char *msg = NULL;
	int msg_len, data_len;

	bzero(re, sizeof(struct areply));
	
	r=read(conf->sockfd, &th, TAC_PLUS_HDR_SIZE);
	if(r < TAC_PLUS_HDR_SIZE) {
  		syslog(LOG_ERR,
 			"%s: short author header, %d of %d: %m", __FUNCTION__,
		 	r, TAC_PLUS_HDR_SIZE);
		re->msg = system_err_msg;
		re->status = AUTHOR_STATUS_ERROR;
		goto AuthorExit;
 	}

	/* check header consistency */
	err = _tac_check_header(&th, TAC_PLUS_AUTHOR, server, &msg);
	if (err) {
		/* no need to process body if header is broken */
		re->msg = msg;
		re->status = AUTHOR_STATUS_ERROR;
		goto AuthorExit;
	}

 	len_from_header=ntohl(th.datalength);
 	tb=(struct author_reply *) xcalloc(1, len_from_header);

 	/* read reply packet body */
 	r=read(conf->sockfd, tb, len_from_header);
 	if(r < len_from_header) {
  		syslog(LOG_ERR,
			 "%s: short author body, %d of %d: %m", __FUNCTION__,
			 r, len_from_header);
		re->msg = system_err_msg;
		re->status = AUTHOR_STATUS_ERROR;
		goto AuthorExit;
 	}

 	/* decrypt the body */
 	_tac_crypt(server->secret, (u_char *) tb, &th, len_from_header);

	msg_len = ntohs(tb->msg_len);
	data_len = ntohs(tb->data_len);

 	/* check consistency of the reply body
	 * len_from_header = declared in header
	 * len_from_body = value computed from body fields
	 */
 	len_from_body = TAC_AUTHOR_REPLY_FIXED_FIELDS_SIZE +
	    		msg_len + data_len;
	    
	pktp = (char *) tb + TAC_AUTHOR_REPLY_FIXED_FIELDS_SIZE;
	
	for(r = 0; r < tb->arg_cnt; r++) {
	    len_from_body += sizeof(u_char); /* add arg length field's size*/
	    len_from_body += *pktp; /* add arg length itself */
		pktp++;
	}
	
 	if(len_from_header != len_from_body) {
  		syslog(LOG_ERR,
		       "%s: inconsistent author reply body, incorrect key?",
		       __FUNCTION__);
		re->msg = system_err_msg;
		re->status = AUTHOR_STATUS_ERROR;
		goto AuthorExit;
 	}

	/* packet seems to be consistent, prepare return messages */
	/* server message for user */
	if(msg_len) {
		char *msg1 = (char *) xcalloc(1, msg_len+1);
		bcopy((u_char *) tb+TAC_AUTHOR_REPLY_FIXED_FIELDS_SIZE
				+ (tb->arg_cnt)*sizeof(u_char),
				msg1, msg_len);
		syslog(LOG_ERR, "%s: author failed msg: %s", __FUNCTION__, msg1);
		re->msg = msg1;
	}

	/* server message to syslog */
	if(data_len) {
		char *smsg=(char *) xcalloc(1, data_len+1);
		bcopy((u_char *) tb + TAC_AUTHOR_REPLY_FIXED_FIELDS_SIZE
				+ (tb->arg_cnt)*sizeof(u_char)
				+ msg_len, smsg, 
				data_len);
		syslog(LOG_ERR, "%s: author failed data: %s", __FUNCTION__,smsg);
		free(smsg);
	}

	/* prepare status */
	switch(tb->status) {
		/* success conditions */
		/* XXX support optional vs mandatory arguments */
		case AUTHOR_STATUS_PASS_ADD:
		case AUTHOR_STATUS_PASS_REPL:
			{
				char *argp; 

				if(!re->msg) re->msg=author_ok_msg;
				re->status=tb->status;
			
				/* add attributes received to attribute list returned to
				   the client */
				pktp = (char *) tb + TAC_AUTHOR_REPLY_FIXED_FIELDS_SIZE;
				argp = pktp + (tb->arg_cnt * sizeof(u_char)) + msg_len +
						data_len;
				/* argp points to current argument string
				   pktp holds current argument length */
				for(r=0; r < tb->arg_cnt; r++) {
					char buff[256];
					char name[128];
					char content[128];
					char *sep;
					char *attrib;
					char *value;
					
					bcopy(argp, buff, *pktp);
					buff[(int) *pktp] = '\0';
					sep=index(buff, '=');
					if(sep == NULL)
							sep=index(buff, '*');
					if(sep == NULL)
							syslog(LOG_WARNING, "%s: attribute contains no separator: %s", __FUNCTION__, buff);
					*sep = '\0';
					value = ++sep;
					/* now buff points to attribute name,
					   value to the attribute value */
					tac_add_attrib(&re->attr, buff, value);
					
					argp += *pktp;
					pktp++; 
				}
			}
			
			break;

		/* authorization failure conditions */
		/* failing to follow is allowed by RFC, page 23  */
		case AUTHOR_STATUS_FOLLOW: 
		case AUTHOR_STATUS_FAIL:
			if(!re->msg) re->msg=author_fail_msg;
			re->status=AUTHOR_STATUS_FAIL;
			break;

		/* error conditions */	
		case AUTHOR_STATUS_ERROR:
		default:
			if(!re->msg) re->msg=author_err_msg;
			re->status=AUTHOR_STATUS_ERROR;
	}

AuthorExit:

	free(tb);	
	TACDEBUG((LOG_DEBUG, "%s: server replied '%s'", __FUNCTION__, \
			re->msg))
	
}
