/*
 * This program is designed to run setuid(root) or with sufficient
 * privilege to read all of the unix password databases. It is designed
 * to provide a mechanism for the current user (defined by this
 * process' uid) to verify their own password.
 *
 * The password is read from the standard input. The exit status of
 * this program indicates whether the user is authenticated or not.
 *
 * Copyright information is located at the end of the file.
 *
 */

#include "config.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <shadow.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

#include <security/_pam_types.h>
#include <security/_pam_macros.h>

#include "md5.h"
#include "bigcrypt.h"

#include "passverify.h"

/* syslogging function for errors and other information */

void
_log_err(int err, const char *format,...)
{
	va_list args;

	va_start(args, format);
	openlog("unix_chkpwd", LOG_CONS | LOG_PID, LOG_AUTHPRIV);
	vsyslog(err, format, args);
	va_end(args);
	closelog();
}

static int
_unix_shadowed(const struct passwd *pwd)
{
	char hashpass[1024];
	if (pwd != NULL) {
		if (strcmp(pwd->pw_passwd, "x") == 0) {
			return 1;
		}
		if (strlen(pwd->pw_name) < sizeof(hashpass) - 2) {
			strcpy(hashpass, "##");
			strcpy(hashpass + 2, pwd->pw_name);
			if (strcmp(pwd->pw_passwd, hashpass) == 0) {
				return 1;
			}
		}
	}
	return 0;
}

static void
su_sighandler(int sig)
{
#ifndef SA_RESETHAND
	/* emulate the behaviour of the SA_RESETHAND flag */
	if ( sig == SIGILL || sig == SIGTRAP || sig == SIGBUS || sig = SIGSERV )
		signal(sig, SIG_DFL);
#endif
	if (sig > 0) {
		_exit(sig);
	}
}

void
setup_signals(void)
{
	struct sigaction action;	/* posix signal structure */

	/*
	 * Setup signal handlers
	 */
	(void) memset((void *) &action, 0, sizeof(action));
	action.sa_handler = su_sighandler;
#ifdef SA_RESETHAND
	action.sa_flags = SA_RESETHAND;
#endif
	(void) sigaction(SIGILL, &action, NULL);
	(void) sigaction(SIGTRAP, &action, NULL);
	(void) sigaction(SIGBUS, &action, NULL);
	(void) sigaction(SIGSEGV, &action, NULL);
	action.sa_handler = SIG_IGN;
	action.sa_flags = 0;
	(void) sigaction(SIGTERM, &action, NULL);
	(void) sigaction(SIGHUP, &action, NULL);
	(void) sigaction(SIGINT, &action, NULL);
	(void) sigaction(SIGQUIT, &action, NULL);
}

int
read_passwords(int fd, int npass, char **passwords)
{
	int rbytes = 0;
	int offset = 0;
	int i = 0;
	char *pptr;
	while (npass > 0) {
        	rbytes = read(fd, passwords[i]+offset, MAXPASS-offset);

        	if (rbytes < 0) {
                	if (errno == EINTR) continue;
                	break;
		}
		if (rbytes == 0)
			break;

		while (npass > 0 && (pptr=memchr(passwords[i]+offset, '\0', rbytes))
			!= NULL) {
			rbytes -= pptr - (passwords[i]+offset) + 1;
			i++;
			offset = 0;
			npass--;
			if (rbytes > 0) {
				if (npass > 0)
					memcpy(passwords[i], pptr+1, rbytes);
				memset(pptr+1, '\0', rbytes);
			}
		}
		offset += rbytes;
	}               

	/* clear up */
	if (offset > 0 && npass > 0) {
		memset(passwords[i], '\0', offset);
	}

	return i;
}

static void
strip_hpux_aging(char *p)
{
	const char *valid = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			    "abcdefghijklmnopqrstuvwxyz"
			    "0123456789./";
	if ((*p != '$') && (strlen(p) > 13)) {
		for (p += 13; *p != '\0'; p++) {
			if (strchr(valid, *p) == NULL) {
				*p = '\0';
				break;
			}
		}
	}
}

int
_unix_verify_password(const char *name, const char *p, int nullok)
{
	struct passwd *pwd = NULL;
	struct spwd *spwdent = NULL;
	char *salt = NULL;
	char *pp = NULL;
	int retval = PAM_AUTH_ERR;
	size_t salt_len;

	/* UNIX passwords area */
	setpwent();
	pwd = getpwnam(name);	/* Get password file entry... */
	endpwent();
	if (pwd != NULL) {
		if (_unix_shadowed(pwd)) {
			/*
			 * ...and shadow password file entry for this user,
			 * if shadowing is enabled
			 */
			setspent();
			spwdent = getspnam(name);
			endspent();
			if (spwdent != NULL)
				salt = x_strdup(spwdent->sp_pwdp);
			else
				pwd = NULL;
		} else {
			if (strcmp(pwd->pw_passwd, "*NP*") == 0) {	/* NIS+ */
				uid_t save_uid;

				save_uid = geteuid();
				seteuid(pwd->pw_uid);
				spwdent = getspnam(name);
				seteuid(save_uid);

				salt = x_strdup(spwdent->sp_pwdp);
			} else {
				salt = x_strdup(pwd->pw_passwd);
			}
		}
	}
	if (pwd == NULL || salt == NULL) {
		_log_err(LOG_ALERT, "check pass; user unknown");
		p = NULL;
		return PAM_USER_UNKNOWN;
	}

	strip_hpux_aging(salt);
	salt_len = strlen(salt);
	if (salt_len == 0) {
		return (nullok == 0) ? PAM_AUTH_ERR : PAM_SUCCESS;
	}
	if (p == NULL || strlen(p) == 0) {
		_pam_overwrite(salt);
		_pam_drop(salt);
		return PAM_AUTHTOK_ERR;
	}

	/* the moment of truth -- do we agree with the password? */
	retval = PAM_AUTH_ERR;
	if (!strncmp(salt, "$1$", 3)) {
		pp = Goodcrypt_md5(p, salt);
		if (pp && strcmp(pp, salt) == 0) {
			retval = PAM_SUCCESS;
		} else {
			_pam_overwrite(pp);
			_pam_drop(pp);
			pp = Brokencrypt_md5(p, salt);
			if (pp && strcmp(pp, salt) == 0)
				retval = PAM_SUCCESS;
		}
	} else if (*salt == '$') {
	        /*
		 * Ok, we don't know the crypt algorithm, but maybe
		 * libcrypt nows about it? We should try it.
		 */
	        pp = x_strdup (crypt(p, salt));
		if (pp && strcmp(pp, salt) == 0) {
			retval = PAM_SUCCESS;
		}
	} else if (*salt == '*' || *salt == '!' || salt_len < 13) {
	    retval = PAM_AUTH_ERR;
	} else {
		pp = bigcrypt(p, salt);
		/*
		 * Note, we are comparing the bigcrypt of the password with
		 * the contents of the password field. If the latter was
		 * encrypted with regular crypt (and not bigcrypt) it will
		 * have been truncated for storage relative to the output
		 * of bigcrypt here. As such we need to compare only the
		 * stored string with the subset of bigcrypt's result.
		 * Bug 521314.
		 */
		if (pp && salt_len == 13 && strlen(pp) > salt_len) {
		    _pam_overwrite(pp+salt_len);
		}
		
		if (pp && strcmp(pp, salt) == 0) {
			retval = PAM_SUCCESS;
		}
	}
	p = NULL;		/* no longer needed here */

	/* clean up */
	_pam_overwrite(pp);
	_pam_drop(pp);

	return retval;
}

char *
getuidname(uid_t uid)
{
	struct passwd *pw;
	static char username[256];

	pw = getpwuid(uid);
	if (pw == NULL)
		return NULL;

	strncpy(username, pw->pw_name, sizeof(username));
	username[sizeof(username) - 1] = '\0';

	return username;
}
/*
 * Copyright (c) Andrew G. Morgan, 1996. All rights reserved
 * Copyright (c) Red Hat, Inc. 2007. All rights reserved
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
