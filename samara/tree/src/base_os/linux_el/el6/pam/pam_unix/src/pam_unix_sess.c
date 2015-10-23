/*
 * $Id: pam_unix_sess.c,v 1.5 2013/03/04 02:44:42 et Exp $
 *
 * Copyright Alexander O. Yuriev, 1996.  All rights reserved.
 * Copyright Jan Rêkorajski, 1999.  All rights reserved.
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

/* indicate the following groups are defined */

#define PAM_SM_SESSION

#include <security/_pam_macros.h>
#include <security/pam_modules.h>
#include <security/pam_ext.h>
#include <security/pam_modutil.h>

#include "support.h"

#include "pam_common.h"

/*
 * NOTE: this is shared with 
 * src/base_os/common/openssh/src/auth-pam.c
 * (with whom we share no header files)
 */
#define PAM_TRUSTED_AUTH_INFO_SET_STR "set"

/*
 * PAM framework looks for these entry-points to pass control to the
 * session module.
 */

/*
 * Defined in pam_unix_auth.c (we have no header files!)
 */
void pam_unix_set_trusted_auth_info(pam_handle_t *pamh,
                                    lpc_auth_method auth_method,
                                    const char *remote_name,
                                    const char *local_name);

PAM_EXTERN int pam_sm_open_session(pam_handle_t * pamh, int flags,
				   int argc, const char **argv)
{
	char *user_name = NULL, *service = NULL;
	unsigned int ctrl = 0;
	int retval = 0;
	const char *login_name = NULL;
	const char *login_username = NULL;
	const char *old_tai = NULL;
	int names_different = 0;
        lpc_auth_method auth_method = lpcam_none;
        const char *user_remote = NULL, *user_local = NULL;

        /* pam_syslog(pamh, LOG_DEBUG, "pam_sm_open_session()"); */

	D(("called."));

	ctrl = _set_ctrl(pamh, flags, NULL, NULL, argc, argv);

	retval = pam_get_item(pamh, PAM_USER, (void *) &user_name);
	if (user_name == NULL || *user_name == '\0' || retval != PAM_SUCCESS) {
		pam_syslog(pamh, LOG_CRIT,
		         "open_session - error recovering username");
		return PAM_SESSION_ERR;		/* How did we get authenticated with
						   no username?! */
	}
	retval = pam_get_item(pamh, PAM_SERVICE, (void *) &service);
	if (service == NULL || *service == '\0' || retval != PAM_SUCCESS) {
		pam_syslog(pamh, LOG_CRIT,
		         "open_session - error recovering service");
		return PAM_SESSION_ERR;
	}
	login_name = pam_modutil_getlogin(pamh);
	if (login_name == NULL) {
	    login_name = "";
	}

	login_username = pam_getenv(pamh, "LOGIN_USER");
	if (login_username && (!user_name || 
                               (strcmp(login_username, user_name) != 0))) {
            names_different = 1;
	    pam_syslog(pamh, LOG_INFO, "session opened for user %s (%s) by "
                       "%s(uid=%lu)"
                       ,login_username, user_name
                       ,login_name, (unsigned long) getuid());
	}
	else {
	    names_different = 0;
	    pam_syslog(pamh, LOG_INFO, "session opened for user %s by %s(uid=%lu)",
		       user_name, login_name, (unsigned long)getuid());
	}

	/*
	 * Make sure TRUSTED_AUTH_INFO is set.  The cases are:
	 * 
	 *   1. Set to the magic string PAM_TRUSTED_AUTH_INFO_SET_STR.
         *      This is how sshd signals us that it wants us to set it
         *      ourselves; probably it means the user authenticated in some
         *      non-PAM way, like with an ssh authorized key.
	 *
	 *      (a) If user_name and login_name match, we'll assume that
	 *	    their authentication was "local", and report it that way.
	 *	    (This could be remote authentication if someone
         *          introduced a new PAM module, but we'll stick with 
         *          Occam's Razor...)
	 *
	 *      (b) If user_name and login_name do not match, this is
	 *	    unexpected.  This suggests it was a remote authentication,
	 *	    but by a module that's not one we have modified to support
	 *	    TRUSTED_AUTH_INFO.  Since we don't know the situation,
         *          leave it alone.
         *
         *   2. Set to something else.  We'll presume it's set to some
         *      real value here, and leave it alone.
         *
         *   3. Not set at all.  We're not sure how this happened, but
         *      leave it alone.
	 */
	old_tai = pam_getenv(pamh, "TRUSTED_AUTH_INFO");

	if (old_tai == NULL) { /* Case 3 */
            /*
             * Leave it unset.
             */
	    pam_syslog(pamh, LOG_DEBUG, "TRUSTED_AUTH_INFO: not set, "
                       "leaving it alone");
        }
        else if (!strcmp(old_tai, PAM_TRUSTED_AUTH_INFO_SET_STR)) { /*Case 1*/
            if (login_username && user_name) {
                user_remote = login_username;
                user_local = user_name;
            }
            else {
                /* We know from above test that user_name is non-NULL */
                user_remote = user_name;
                user_local = user_name;
            }

	    if (names_different) { /* Case 1b */
		pam_syslog(pamh, LOG_WARNING, "TRUSTED_AUTH_INFO: set "
                           "requested, but two different usernames?  "
                           "(LOGIN_USER = '%s', AUTH_USER = '%s')  "
                           "Leaving it alone", user_remote, user_local);
	    }
	    else { /* Case 1a */
		pam_syslog(pamh, LOG_DEBUG, "TRUSTED_AUTH_INFO: set "
                           "requested, names match, presuming non-PAM local "
			   "authentication, e.g. ssh authorized key");
                auth_method = lpcam_local;
	    }

            if (auth_method != lpcam_none) {
                pam_unix_set_trusted_auth_info(pamh, auth_method,
                                               user_remote, user_local);
            }
	}
        else { /* Case 2 */
            /*
             * Leave it unset.
             */
	    pam_syslog(pamh, LOG_DEBUG, "TRUSTED_AUTH_INFO: already set, "
                       "leaving it alone");
        }

	return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_close_session(pam_handle_t * pamh, int flags,
				    int argc, const char **argv)
{
	char *user_name, *service;
	const char *login_username;
	unsigned int ctrl;
	int retval;

        /* pam_syslog(pamh, LOG_DEBUG, "pam_sm_close_session()"); */

	D(("called."));

	ctrl = _set_ctrl(pamh, flags, NULL, NULL, argc, argv);

	retval = pam_get_item(pamh, PAM_USER, (void *) &user_name);
	if (user_name == NULL || *user_name == '\0' || retval != PAM_SUCCESS) {
		pam_syslog(pamh, LOG_CRIT,
		         "close_session - error recovering username");
		return PAM_SESSION_ERR;		/* How did we get authenticated with
						   no username?! */
	}
	retval = pam_get_item(pamh, PAM_SERVICE, (void *) &service);
	if (service == NULL || *service == '\0' || retval != PAM_SUCCESS) {
		pam_syslog(pamh, LOG_CRIT,
		         "close_session - error recovering service");
		return PAM_SESSION_ERR;
	}

	login_username = pam_getenv(pamh, "LOGIN_USER");
	if (login_username && (!user_name ||
                               (strcmp(login_username, user_name) != 0))) {
	    pam_syslog(pamh, LOG_INFO, "session closed for user %s (%s)"
                       ,login_username ,user_name);
	}
        else {
            pam_syslog(pamh, LOG_INFO, "session closed for user %s",
                       user_name);
        }

	return PAM_SUCCESS;
}

/* static module data */
#ifdef PAM_STATIC
struct pam_module _pam_unix_session_modstruct = {
    "pam_unix_session",
    NULL,
    NULL,
    NULL,
    pam_sm_open_session,
    pam_sm_close_session,
    NULL,
};
#endif

