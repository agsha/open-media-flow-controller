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
#include <sys/time.h>
#ifdef WITH_SELINUX
#include <selinux/selinux.h>
#define SELINUX_ENABLED (selinux_enabled!=-1 ? selinux_enabled : (selinux_enabled=is_selinux_enabled()>0))
static security_context_t prev_context=NULL;
static int selinux_enabled=-1;
#else
#define SELINUX_ENABLED 0
#endif

#define MAXPASS		200	/* the maximum length of a password */

#include <security/_pam_types.h>
#include <security/_pam_macros.h>

#include "md5.h"
#include "bigcrypt.h"
#include "passverify.h"

#define _pam_delete(xx)         \
{                               \
	_pam_overwrite(xx);     \
        _pam_drop(xx);          \
}

static int
_unix_shadowed(const struct passwd *pwd)
{
        if (pwd != NULL) {
                if (strcmp(pwd->pw_passwd, "x") == 0) {
                        return 1;
                }
                if ((pwd->pw_passwd[0] == '#') &&
                    (pwd->pw_passwd[1] == '#') &&
                    (strcmp(pwd->pw_name, pwd->pw_passwd + 2) == 0)) {
                        return 1;
                }
        }
        return 0;
}

#define HELPER_COMPILE
#include "passupdate.c"

static int
verify_account(const char * const uname)
{
	struct spwd *spent;
	struct passwd *pwent;

	pwent = getpwnam(uname);
	if (!pwent) {
		_log_err(LOG_ALERT, "could not identify user (from getpwnam(%s))", uname);
		return PAM_USER_UNKNOWN;
	}

	spent = getspnam( uname );
	if (!spent) {
		_log_err(LOG_ALERT, "could not get username from shadow (%s))", uname);
		return PAM_AUTHINFO_UNAVAIL;	/* Couldn't get username from shadow */
	}
	printf("%ld:%ld:%ld:%ld:%ld:%ld",
		 spent->sp_lstchg, /* last password change */
                 spent->sp_min, /* days until change allowed. */
                 spent->sp_max, /* days before change required */
                 spent->sp_warn, /* days warning for expiration */
                 spent->sp_inact, /* days before account inactive */
                 spent->sp_expire); /* date when account expires */

	return PAM_SUCCESS;
}

static int
set_password(const char *forwho, const char *shadow, const char *remember)
{
    struct passwd *pwd = NULL;
    int retval;
    char pass[MAXPASS + 1];
    char towhat[MAXPASS + 1];
    int npass = 0;
    /* we don't care about number format errors because the helper
       should be called internally only */
    int doshadow = atoi(shadow);
    int nremember = atoi(remember);
    char *passwords[] = { pass, towhat };

    /* read the password from stdin (a pipe from the pam_unix module) */

    npass = read_passwords(STDIN_FILENO, 2, passwords);

    if (npass != 2) {	/* is it a valid password? */
      if (npass == 1) {
        _log_err(LOG_DEBUG, "no new password supplied");
	memset(pass, '\0', MAXPASS);
      } else {
        _log_err(LOG_DEBUG, "no valid passwords supplied");
      }
      return PAM_AUTHTOK_ERR;
    }

#ifdef USE_LCKPWDF
    if (lock_pwdf() != PAM_SUCCESS)
    	return PAM_AUTHTOK_LOCK_BUSY;
#endif

    pwd = getpwnam(forwho);
    
    if (pwd == NULL) {
        retval = PAM_USER_UNKNOWN;
        goto done;
    }

    /* If real caller uid is not root we must verify that
       received old pass agrees with the current one.
       We always allow change from null pass. */
    if (getuid()) {
	retval = _unix_verify_password(forwho, pass, 1);
	if (retval != PAM_SUCCESS) {
	    goto done;
	}
    }

    /* first, save old password */
    if (save_old_password(forwho, pass, nremember)) {
	retval = PAM_AUTHTOK_ERR;
	goto done;
    }

    if (doshadow || _unix_shadowed(pwd)) {
	retval = _update_shadow(forwho, towhat);
	if (retval == PAM_SUCCESS)
	    if (!_unix_shadowed(pwd))
		retval = _update_passwd(forwho, "x");
    } else {
	retval = _update_passwd(forwho, towhat);
    }

done:
    memset(pass, '\0', MAXPASS);
    memset(towhat, '\0', MAXPASS);

#ifdef USE_LCKPWDF
    unlock_pwdf();
#endif

    if (retval == PAM_SUCCESS) {
	return PAM_SUCCESS;
    } else {
	return PAM_AUTHTOK_ERR;
    }
}

int main(int argc, char *argv[])
{
	char *option;

	/*
	 * Catch or ignore as many signal as possible.
	 */
	setup_signals();

	/*
	 * we establish that this program is running with non-tty stdin.
	 * this is to discourage casual use. It does *NOT* prevent an
	 * intruder from repeatadly running this program to determine the
	 * password of the current user (brute force attack, but one for
	 * which the attacker must already have gained access to the user's
	 * account).
	 */

	if (isatty(STDIN_FILENO) || argc < 3 ) {
		_log_err(LOG_NOTICE
		      ,"inappropriate use of Unix helper binary [UID=%d]"
			 ,getuid());
		fprintf(stderr
		 ,"This binary is not designed for running in this way\n"
		      "-- the system administrator has been informed\n");
		sleep(10);	/* this should discourage/annoy the user */
		return PAM_SYSTEM_ERR;
	}

	/* We must be root to read/update shadow.
	 */
	if (geteuid() != 0) {
	    return PAM_AUTH_ERR;
	}
	
	option = argv[2];

	if (strncmp(option, "verify", 8) == 0) {
	  /* Get the account information from the shadow file */
	  return verify_account(argv[1]);
	}

	if (strncmp(option, "update", 8) == 0) {
          if (argc == 5) 
	      /* Attempting to change the password */
	      return set_password(argv[1], argv[3], argv[4]);
	}

	return PAM_SYSTEM_ERR;
}

/*
 * Copyright (c) Andrew G. Morgan, 1996. All rights reserved
 * Copyright (c) Red Hat, Inc., 2007. All rights reserved
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
