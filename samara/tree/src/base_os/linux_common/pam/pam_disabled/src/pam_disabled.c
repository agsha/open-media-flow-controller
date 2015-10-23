/*
 *
 * (C) Copyright 2013-2015 Juniper Networks, Inc. All Rights reserved.
 * (C) Copyright 2002-2012 Tall Maple Systems, Inc.
 * All rights reserved.
 * The contents of this file are subject to the terms of the MIT License.
 * You may not use this file except in compliance with the License.
 * See the file License.txt at the top of this source tree.
 * You can obtain a copy of the license at https://opensource.org/licenses/MIT
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <pwd.h>
#include <errno.h>
#include <shadow.h>

#include <security/_pam_macros.h>
/*
 * here, we make a definition for the externally accessible function
 * in this file (this definition is required for static a module
 * but strongly encouraged generally) it is used to instruct the
 * modules include file to define the function prototypes.
 */

#define PAM_SM_AUTH
#define PAM_SM_ACCOUNT

#include <security/pam_modules.h>
#include <security/pam_modutil.h>
#include <security/pam_ext.h>

#ifndef UNUSED
#define UNUSED __attribute__ ((unused))
#endif

static const char pam_disabled_passwd_prefix_char = '!';

/* From pam_unix/support.c */
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

static int pam_disabled_perform_check(pam_handle_t *pamh)
{
    int err = 0;
    const char *username = NULL;
    int retval = PAM_IGNORE;
    char buf[16384];
    struct passwd pwbuf;
    struct passwd *pwbuf_p = NULL;
    struct spwd spbuf;
    struct spwd *spbuf_p = NULL;
    const char *passwd = NULL;

    memset(&pwbuf, 0, sizeof(pwbuf));
    memset(&spbuf, 0, sizeof(spbuf));

    /*
     * Pam_nologin returns various other return values such as
     * PAM_USER_UNKNOWN, and PAM_SYSTEM_ERR, if things go wrong while
     * trying to make the determination.  But we want to take the
     * lowest-risk path, where we do not interfere unless we're sure
     * we need to prevent the user from logging in.  So we return
     * PAM_IGNORE in all error cases; but we do still log at WARNING
     * so these errors can be detected.
     */

    if ((pam_get_user(pamh, &username, NULL) != PAM_SUCCESS) || !username) {
        pam_syslog(pamh, LOG_WARNING, "Cannot determine username");
        retval = PAM_IGNORE;
        goto bail;
    }

    /* ........................................................................
     * Try the main password file first, in case they aren't using
     * shadow passwords.
     */
    errno = 0;
    err = getpwnam_r(username, &pwbuf, buf, sizeof(buf), &pwbuf_p);
    if (err != 0) {
        pam_syslog(pamh, LOG_WARNING, "Error looking up user %s: %s",
                   username, strerror(errno));
        retval = PAM_IGNORE;
        goto bail;
    }

    if (pwbuf_p == NULL) {
        pam_syslog(pamh, LOG_WARNING, "Could not find user %s", username);
        retval = PAM_IGNORE;
        goto bail;
    }

    pam_syslog(pamh, LOG_DEBUG, "Got info for username %s", username);

    if (pwbuf.pw_passwd == NULL) {
        retval = PAM_IGNORE;
        goto bail;
    }

    passwd = pwbuf.pw_passwd;

    /* ........................................................................
     * If they are using shadow passwords, try there next.
     */
    if (_unix_shadowed(&pwbuf)) {
        err = getspnam_r(username, &spbuf, buf, sizeof(buf), &spbuf_p);
        if (err != 0) {
            pam_syslog(pamh, LOG_WARNING, "Error looking up shadow password "
                       "for user %s: %s", username, strerror(errno));
            retval = PAM_IGNORE;
            goto bail;
        }
        
        if (spbuf_p == NULL) {
            pam_syslog(pamh, LOG_WARNING, "Could not find shadow password "
                       "for user %s", username);
            retval = PAM_IGNORE;
            goto bail;
        }

        passwd = spbuf.sp_pwdp;
    }
    
    /* ........................................................................
     * If the password can be gotten at all, we now have it.
     */
    if (passwd != NULL && passwd[0] == pam_disabled_passwd_prefix_char) {
        pam_syslog(pamh, LOG_NOTICE, "Denying access to user %s, whose "
                   "account is disabled", username);
        retval = PAM_AUTH_ERR;
        goto bail;
    }

    retval = PAM_IGNORE;

 bail:    
    return(retval);
}


/* --- authentication management functions --- */

PAM_EXTERN int
pam_sm_authenticate (pam_handle_t *pamh, int flags UNUSED,
                     int argc, const char **argv)
{
    return pam_disabled_perform_check(pamh);
}

PAM_EXTERN int
pam_sm_setcred (pam_handle_t *pamh UNUSED, int flags UNUSED,
                int argc, const char **argv)
{
    return(PAM_IGNORE);
}

/* --- account management function --- */

PAM_EXTERN int
pam_sm_acct_mgmt(pam_handle_t *pamh, int flags UNUSED,
                 int argc, const char **argv)
{
    return pam_disabled_perform_check(pamh);
}


#ifdef PAM_STATIC

/* static module data */

struct pam_module _pam_disabled_modstruct = {
     "pam_disabled",
     pam_sm_authenticate,
     pam_sm_setcred,
     pam_sm_acct_mgmt,
     NULL,
     NULL,
     NULL,
};

#endif /* PAM_STATIC */

/* end of module definition */
