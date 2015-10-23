/*
 * support.c support functions for pam_tacplus.c Copyright 1998,1999,2000 
 * by Pawel Krawczyk <kravietz@ceti.pl> See `CHANGES' file for revision
 * history. 
 */
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <stdarg.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>

#define PAM_SM_AUTH
#define PAM_SM_ACCOUNT
#define PAM_SM_SESSION
/*
 * #define PAM_SM_PASSWORD 
 */

#include <security/pam_modules.h>
#include <security/pam_ext.h>

#include "pam_tacplus.h"
#include "tacplus.h"
#include "libtac.h"
#include "xalloc.h"
#include "support.h"

static char     conf_file[BUFFER_SIZE];	/* configuration file */

char           *tac_service = NULL;
char           *tac_protocol = NULL;


const char           *
_pam_get_terminal(pam_handle_t * pamh)
{
    int             retval;
    const char      *tty;

    retval = pam_get_item(pamh, PAM_TTY, (const void **) &tty);
    if (retval != PAM_SUCCESS || tty == NULL || *tty == '\0') {
	tty = ttyname(STDIN_FILENO);
	if (tty == NULL || *tty == '\0') {
	    tty = "unknown";
	}
    }
    return (tty);
}

const char           *
_pam_get_rhost(pam_handle_t * pamh)
{
    int             retval;
    const char      *rhost;

    retval = pam_get_item(pamh, PAM_RHOST, (const void **) &rhost);
    if (retval != PAM_SUCCESS || rhost == NULL || *rhost == '\0') {
        rhost = "unknown";
    }
    return (rhost);
}

void
_pam_log(pam_handle_t *pamh, int err, const char *format, ...)
{
    va_list         args;

    va_start(args, format);
    /*
     * Upstream does this with openlog(), and a straight syslog(), but
     * we want all our PAM syslog messages to look and act the same.
     */
    pam_vsyslog(pamh, err, format, args);
    va_end(args);
}


/*
 * stolen from pam_stress 
 */
int
converse(pam_handle_t * pamh, int nargs, struct pam_message **message,
	 struct pam_response **response)
{
    int             retval;
    struct pam_conv *conv;

    if ((retval = pam_get_item(pamh, PAM_CONV, (const void **) &conv))
	== PAM_SUCCESS) {
	retval =
	    conv->conv(nargs, (const struct pam_message **) message,
		       response, conv->appdata_ptr);
	if (retval != PAM_SUCCESS) {
	    _pam_log(pamh, LOG_ERR, "(pam_tacplus) converse returned %d",
		     retval);
	    _pam_log(pamh, LOG_ERR, "that is: %s", pam_strerror(pamh, retval));
	}
    } else {
	_pam_log(pamh, LOG_ERR, "(pam_tacplus) converse failed to get pam_conv");
    }

    return retval;
}

/*
 * stolen from pam_stress 
 */
int
tacacs_get_password(pam_handle_t *pamh, const char *prompt_msg, int flags, 
                    int ctrl, char **ret_password)
{
    int retval;

    if (ctrl & PAM_TAC_DEBUG) {
	syslog(LOG_DEBUG, "%s: called", __FUNCTION__);
    }

    /*
     * grab the password (if any) from the previous authentication layer 
     */
    retval = pam_get_item(pamh, PAM_AUTHTOK, (const void **) ret_password);
    if (retval != PAM_SUCCESS) {
	return (retval);
    }
    if (*ret_password) {
	*ret_password = strdup(*ret_password);
	return (PAM_SUCCESS);
    }
    else if (ctrl & PAM_TAC_USE_FIRST_PASS) {
	/*
	 * use one pass only, stopping if it fails 
	 */
	return (PAM_AUTH_ERR);
    }

    retval = pam_get_user_data(pamh, prompt_msg, flags, ctrl, ret_password);
    return(retval);
}

int
pam_get_user_data(pam_handle_t *pamh, const char *prompt_msg, int flags, 
                  int ctrl, char **ret_data)
{
    char *data = NULL;
    struct pam_message msg[1], *pmsg[1];
    struct pam_response *resp;
    int retval;

    if (ctrl & PAM_TAC_DEBUG) {
	syslog(LOG_DEBUG, "%s: called", __FUNCTION__);
    }

    /*
     * set up conversation call 
     */
    pmsg[0] = &msg[0];
    msg[0].msg_style = PAM_PROMPT_ECHO_OFF; /* XXX should be an option */
    msg[0].msg = prompt_msg;
    resp = NULL;

    if ((retval = converse(pamh, 1, pmsg, &resp)) != PAM_SUCCESS) {
	return(retval);
    }

    if (resp) {
	if ((resp[0].resp == NULL) && (ctrl & PAM_TAC_DEBUG)) {
	    _pam_log(pamh, LOG_DEBUG, "pam_get_user_data: NULL data received");
	}
	data = resp[0].resp;	/* remember this! */

	/* Clear so we don't free */
	resp[0].resp = NULL;
    }
    else if (ctrl & PAM_TAC_DEBUG) {
	_pam_log(pamh, LOG_DEBUG, "pam_get_user_data: no error reported"
		 "getting data, but NULL returned!?");
	return(PAM_CONV_ERR);
    }

    free(resp);
    resp = NULL;

    *ret_data = data;		/* this *MUST* be free()'d by this module */

    if (ctrl & PAM_TAC_DEBUG) {
	syslog(LOG_DEBUG, "%s: obtained data [%s]", __FUNCTION__,
	       data);
    }

    return(PAM_SUCCESS);
}

int
_pam_parse(pam_handle_t *pamh, int argc, const char **argv, tacacs_conf_t *conf)
{

    int             ctrl = 0;

    memset(conf, 0, sizeof(tacacs_conf_t));	/* ensure it's initialized 
						 */
    conf->initialized = 1;

    strcpy(conf_file, CONF_FILE);

    for (ctrl = 0; argc-- > 0; ++argv) {

	if (!strcmp(*argv, "debug")) {	/* all */
	    ctrl |= PAM_TAC_DEBUG;
	} else if (!strcmp(*argv, "first_hit")) {	/* authentication */
	    ctrl |= PAM_TAC_FIRSTHIT;
	} else if (!strcmp(*argv, "use_first_pass")) {
	    ctrl |= PAM_TAC_USE_FIRST_PASS;
	} else if (!strcmp(*argv, "try_first_pass")) {
	    ctrl |= PAM_TAC_TRY_FIRST_PASS;
	} else if (!strncmp(*argv, "service=", 8)) {	/* author & acct */
	    tac_service = (char *) xcalloc(1, strlen(*argv + 8) + 1);
	    strcpy(tac_service, *argv + 8);
	} else if (!strncmp(*argv, "protocol=", 9)) {	/* author & acct */
	    tac_protocol = (char *) xcalloc(1, strlen(*argv + 9) + 1);
	    strcpy(tac_protocol, *argv + 9);
	} else if (!strcmp(*argv, "acct_all")) {
	    ctrl |= PAM_TAC_ACCT;
	} else if (!strncmp(*argv, "template_user=", 14)) {
	    conf->template_user = (const char *) (*argv + 14);
	} else if (!strcmp(*argv, "use_vendor_attr")) {
	    conf->use_vendor_attr = 1;
	} else if (!strncmp(*argv, "conf=", 5)) {
	    strcpy(conf_file, *argv + 5);
	} else {
	    _pam_log(pamh, LOG_WARNING, "unrecognized option: %s", *argv);
	}
    }

    return ctrl;

}				/* _pam_parse */


/*
 * Read in the server configuration file
 */
int
initialize(pam_handle_t *pamh, tacacs_conf_t *conf, int accounting)
{
    char            hostname[BUFFER_SIZE];
    char            auth_type[BUFFER_SIZE];
    char            secret[BUFFER_SIZE];
    char            buffer[BUFFER_SIZE];
    char           *p;
    FILE           *fserver;
    tacacs_server_t *server = NULL;
    int             timeout;
    int             line = 0;
    int             num_items = 0;

    /*
     * the first time around, read the configuration file 
     */
    if ((fserver = fopen(conf_file, "r")) == (FILE *) NULL) {
	_pam_log(pamh, LOG_ERR, "Could not open configuration file %s: %s\n",
		 conf_file, strerror(errno));
	return PAM_ABORT;
    }

    while (!feof(fserver) &&
	   (fgets(buffer, sizeof(buffer), fserver) != (char *) NULL) &&
	   (!ferror(fserver))) {
	line++;
	p = buffer;

	/*
	 *  Skip blank lines and whitespace
	 */
	while (*p &&
	       ((*p == ' ') || (*p == '\t') ||
		(*p == '\r') || (*p == '\n')))
	    p++;

	/*
	 *  Nothing, or just a comment.  Ignore the line.
	 */
	if ((!*p) || (*p == '#')) {
	    continue;
	}

	timeout = 3;
	num_items = sscanf(p, "%255s %d %255s %255s", hostname, &timeout, auth_type,
			   secret);
	if (num_items < 3) {
	    _pam_log(pamh, LOG_ERR,
		     "ERROR reading %s, line %d: Could not read hostname, "
		     "timeout, or auth_type\n", conf_file, line);
	    continue;		/* invalid line */
	} else {		/* read it in and save the data */
	    tacacs_server_t *tmp;

	    tmp = calloc(1, sizeof(tacacs_server_t));
	    if (!tmp) {
		/*
		 * Seems pretty grim 
		 */
		_pam_log(pamh, LOG_ERR, "Could not allocate space for server %s\n",
			 hostname);
		return PAM_AUTHINFO_UNAVAIL;
	    }

	    if (server) {
		server->next = tmp;
		server = server->next;
	    } else {
		conf->server = tmp;
		server = tmp;	/* first time */
	    }

	    /*
	     * sometime later do memory checks here 
	     */
	    server->hostname = strdup(hostname);
	    if (num_items > 3) {
		server->secret = strdup(secret);
	    }
	    server->port = 0;

	    if ((timeout < 1) || (timeout > 60)) {
		server->timeout = 3;
	    } else {
		server->timeout = timeout;
	    }
	    server->next = NULL;
	    if (strcmp(auth_type, "pap") == 0) {
		server->auth_type = TAC_PLUS_AUTHEN_TYPE_PAP;
		server->service = TAC_PLUS_AUTHEN_SVC_PPP;
	    }
	    else {
		server->auth_type = TAC_PLUS_AUTHEN_TYPE_ASCII;
		server->service = TAC_PLUS_AUTHEN_SVC_LOGIN;
	    }
	    server->seq_no = TAC_PLUS_INIT_SEQNO;
	}
    }
    fclose(fserver);

    if (!server) {		/* no server found, die a horrible death */
	_pam_log(pamh, LOG_ERR,
		 "No TACACS+ server found in configuration file %s\n",
		 conf_file);
	return PAM_AUTHINFO_UNAVAIL;
    }

    return PAM_SUCCESS;
}

void
release_config(tacacs_conf_t *conf)
{
    tacacs_server_t *server = NULL, *next;

    if (conf == NULL || conf->initialized == 0) {
        return;
    }

    /*
     * XXX/EMT: why isn't this part of the config structure?
     */
    if (tac_service) {
	free(tac_service);
    }
    if (tac_protocol) {
	free(tac_protocol);
    }
    
    server = conf->server;
    while (server) {
	next = server->next;
        if (server->hostname) {
            free(server->hostname);
            server->hostname = NULL;
        }
        if (server->secret) {
            free(server->secret);
            server->secret = NULL;
        }
	free(server);
	server = next;
    }

    memset(conf, 0, sizeof(tacacs_conf_t));
}

/*
 * Copyright (c) Cristian Gafton, 1996, <gafton@redhat.com>
 *                                              All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * ALTERNATIVELY, this product may be distributed under the terms of
 * the GNU Public License, in which case the provisions of the GPL are
 * required INSTEAD OF the above restrictions.  (This clause is
 * necessary due to a potential bad interaction between the GPL and
 * the restrictions contained in a BSD-style copyright.)
 *
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */
