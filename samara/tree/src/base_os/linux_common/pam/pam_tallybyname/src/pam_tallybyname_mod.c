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

/*
 * PAM "tally by name" module.  This is like pam_tally2, in that it
 * tracks the number of consecutive authentication failures for each
 * user account, and denies authentication when it exceeds a
 * configured number.  There are two main differences vs. pam_tally2:
 *
 *   1. pam_tally2 works based on uid, while we work based on login
 *      username.  That is the username first given by the user, as
 *      opposed to the local username (which may be different in the case
 *      of remote authentication).
 *
 *   2. pam_tally2 only needs to run once during the "auth" phase.
 *      It records failure as soon as authentication begins, which is
 *      technically not correct.  It operates just fine, since the
 *      count is reset on success.  The only functional impact is that
 *      it limits the number of simultaneous login attempts.  Also, if
 *      you use its command-line utility to view the login failure
 *      status, it will show these "pending failures" as part of the
 *      full failure count, which is misleading.
 *
 *      We, on the other hand, need to run potentially twice during
 *      the "auth" phase.  The first time, which generally goes before
 *      the other authentication modules, is to enforce existing
 *      lockouts.  The second time, which goes after all the other
 *      modules and should only be run in the case of failure, is to
 *      record failure.  This is to solve the aforementioned
 *      idiosyncracy of pam_tally2.  We care more about it, because
 *      (a) we're exposing the failure counts in the UI, and (b) we'll
 *      be using the failure counts to generate login failure events,
 *      and don't want to misreport them.
 *
 *      Running only in the case of failure imposes some requirements
 *      on the ordering and contents of system-auth, as described below.
 *
 * For this module to work, it must appear in three places in the PAM
 * config file for each service (or for all, via system-auth).
 * Please see the pam_tallybyname section in doc/design/aaa.txt for
 * requirements about how this module should be used.
 *
 * There is a binary by the same name (pam_tallybyname) which can be
 * used to view and operate on the database we use for tracking users.
 *
 * This module accepts various configuration options for use during the
 * authentication phase.  General option notes:
 *   - As is standard for PAM modules, the format of each option is
 *     "optionname=value".
 *   - Option names and values are case-sensitive.
 *
 * Option:  mode
 * Phases:  ALL
 * Values:  check, failure, success
 * Default: check
 *
 *     This is the main indicator of what operation the module should
 *     perform, as mentioned in the three steps #1, #2, and #3 above.
 *       - "check" means to check the user's entry in the database and
 *         enforce any lockout.
 *       - "failure" means the user has tried and failed to log in,
 *         and we should at least update their failure count.  We may
 *         also mark the account locked out, depending on other policy
 *         as configured by items below.
 *       - "success" means the user has successfully logged in, and we
 *         should purge their entry in the database (both unlocking the
 *         account, and clearing the history of login failures).
 *
 *     "check" and "failure" may only be used in the "auth" phase.
 *     "success" may only be used in the "account" phase.
 *
 * Option:  onerr
 * Phases:  ALL
 * Values:  fail, ignore
 * Default: fail
 *
 *     What should we do if we encounter an internal error?
 *
 *     'fail' means passing the failure through (or returning
 *     PAM_SYSTEM_ERR if there is no PAM error code yet).
 *
 *     'ignore' means stifling the error, and returning PAM_IGNORE,
 *     essentially bowing out of the auth process.  Warnings will still
 *     be logged, however.
 *
 *     Note that this does not apply to processing of options themselves.
 *     Regardless of any 'onerr' setting, bad options are logged at
 *     LOG_WARNING and then ignored.  The behavior of pam_tallybyname
 *     is only influenced by good options.
 *
 * Option:  deny
 * Phases:  auth (with mode "failure")
 * Value:   <n> (number of attempts)
 * Default: 0 (disabled)
 *
 *     While in "failure" mode, what is the maximum number of
 *     authentication failures permitted before we will lock the account?
 *     e.g. if this number is 3, we will lock the account after the third
 *     authentication failure.
 *
 *     0 means to never automatically lock the account.  Note that this
 *     setting does not affect enforcement (it is not even read in "check"
 *     mode); if an account is already locked, we will continue to enforce
 *     it regardless of how it got that way.
 *
 * Option:  unlock_time
 * Phases:  auth (with mode "check")
 * Value:   <n> (number of seconds)
 * Default: 0 (disabled)
 *
 *     While in "check" mode, should we allow a user to attempt login
 *     anyway, even if their account is locked, if it has been this many
 *     seconds since their last login failure?  0 means to never allow
 *     such exceptions.  Login failures we caused do not count (do not
 *     reset the timer).  Note that once the unlock_time timer has
 *     expired, this is not the same as the account becoming unlocked.
 *     The account will remain locked, but they will get a temporary
 *     exception and login will be permitted anyway.
 *
 * Option:  lock_time
 * Phases:  auth (with mode "check")
 * Value:   <n> (number of seconds)
 * Default: 0 (disabled)
 *
 *     While in "check" mode, should we enforce a minimum lockout
 *     period between each authentication failure?  If this is set to
 *     0, there is no such period.  Otherwise, for this number of
 *     seconds after EACH login failure (not counting ones we ourselves
 *     cause from "check" mode), a user will not be permitted to
 *     re-attempt login, even if the account is not locked.
 *
 * Options: override_admin, override_unknown
 * Phases:  ALL
 * Values:  none, no_lockout, no_track
 * Default: none
 *
 *     Do we exempt certain user classes from being locked out, or
 *     from being tracked at all?  This can be used to decrease what
 *     is done for a given user class, below the global settings.  No
 *     setting for a given user class, or "none", means to just honor
 *     the global settings.
 *
 *     Note that these overrides must be used in all phases and in all
 *     modes to be effective.  They potentially affect behavior for
 *     all modes ("check", "failure", and "success").
 *
 *     "no_lockout" means never to deny access to that user class, either
 *     based on a lockout in the database, or based on lock_time.  We also
 *     will not record any new lockouts based on auth failures.  However,
 *     we would still record auth failures.  "no_track" means the same,
 *     plus also do not record auth failures either.
 *
 *     The "admin" user class means just the single user account named
 *     "admin.  The "unknown" user class means just users whose usernames
 *     we do not recognize.  Currently this just means nonlocal users,
 *     though this could be refined later.  For now, these could be valid
 *     remote users, mistyped usernames, made-up usernames, etc. -- we 
 *     cannot distinguish.
 *
 * Option:  map_unknown
 * Phases:  ALL
 * Value:   {0,1}
 * Default: 0
 *
 *     Request that unknown usernames (the same ones which would be
 *     affected by the override_unknown option described above) be
 *     mapped to something other than their plaintext representation.
 *     The mapped version would be placed into the database in lieu
 *     of the plaintext username.
 *
 *     This is a security measure to address the problem caused by
 *     storing unrecognized usernames: often users accidentally type
 *     their password in lieu of their username, and it is undesirable
 *     to have this information stored and visible in plaintext.
 *
 *     See tbn_username_mapping enum in in pam_tallybyname_common.h
 *     for explanation of options.
 *
 * Option:  downcase
 * Phases:  ALL
 * Value:   {0,1}
 * Default: 0
 *
 *     Request that all usernames containing any capital letters be
 *     converted to all lowercase before doing anything else with it in
 *     the tallybyname module.  (e.g. this takes effect before the
 *     username is mapped by map_unknown) This lowercased username is used
 *     only within tallybyname, and is not set back into the environment,
 *     and therefore does not affect any other PAM operations.
 *
 *     0 (false) means not to do this; 1 (true) means to do this.
 *
 * Option:  max_recs
 * Phases:  auth (with mode "failure")
 * Value:   <x>,<y> (check every y failures; trim to no more than x records)
 * Default: 0,0 (no trimming)
 *
 *     When recording a failure, we can trim the database of login failures
 *     to have no more than a certain number of records.  We can do this
 *     in order to limit how large the database may get, particularly since
 *     unauthenticated users can add new records to it.  If this option is
 *     used, both x and y must be greater than zero.  x is the desired
 *     maximum number of records in the database.  y is the number of logins
 *     between each enforcement.  Setting y to 1 would mean maximum strictness:
 *     check after every login failure.  But that is likely expensive, so it
 *     will commonly be set higher.
 *
 * XXX/EMT: I18N: we don't use gettext in here.
 */

#include "pam_tallybyname_common.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <security/pam_modules.h>
#include <security/pam_modutil.h>
#include <security/pam_ext.h>

#include "customer.h"

typedef enum {
    ptco_none = 0,
    ptco_no_track,
    ptco_no_lockout,
} pam_tbn_class_override;

typedef enum {
    ptm_none = 0,
    ptm_check,
    ptm_failure,
    ptm_success,
} pam_tbn_mode_type;

typedef enum {
    ptp_none = 0,
    ptp_auth,
    ptp_account,
} pam_tbn_phase;

/*
 * We don't want any global variables, in case we are used in a
 * multithreaded context.  Instead, an options struct will be
 * allocated on the stack of whatever pam_sm_...() function is called,
 * then we'll pass it around to whoever needs it.
 */
typedef struct pam_tbn_options {
    pam_tbn_mode_type pto_mode;
    int pto_fail_on_error;
    long pto_deny;
    long pto_unlock_time;
    long pto_lock_time;
    pam_tbn_class_override pto_override_admin;
    pam_tbn_class_override pto_override_unknown;
    tbn_username_mapping pto_map_unknown;
    int pto_downcase;
    long pto_max_recs;
    long pto_max_recs_check_freq;
} pam_tbn_options;

static const pam_tbn_options PAM_TBN_OPTIONS_INIT = {
    .pto_mode = ptm_check,
    .pto_fail_on_error = 1,
    .pto_deny = 0,
    .pto_unlock_time = 0,
    .pto_lock_time = 0,
    .pto_override_admin = ptco_none,
    .pto_override_unknown = ptco_none,
    .pto_map_unknown = tum_none,
    .pto_downcase = 0,
    .pto_max_recs = 0,
    .pto_max_recs_check_freq = 0,
};

static const char *pam_tbn_username_admin = "admin";


/* ------------------------------------------------------------------------- */
static void
bail_debug_pam(void)
{
    volatile int x __attribute__((unused)) = 0;
}


/* ------------------------------------------------------------------------- */
/* Note: we specifically use and capitalize "ERROR" so this will come
 * up in searches looking for errors.
 */
#define bail_error_pam(_pamh, _pto, _err, _pamval)                            \
    do {                                                                      \
        int __do_goto = 0;                                                    \
        if (_err) {                                                           \
            __do_goto = 1;                                                    \
            if (_pto->pto_fail_on_error) {                                    \
                pam_syslog(_pamh, LOG_ERR, "%s(), %s:%u.  Internal ERROR "    \
                           "%d, failing (PAM_SYSTEM_ERR)",                    \
                           __FUNCTION__, __FILE__, __LINE__, _err);           \
                _pamval = PAM_SYSTEM_ERR;                                     \
                pam_info(_pamh, "An internal error occurred.");               \
            }                                                                 \
            else {                                                            \
                pam_syslog(_pamh, LOG_ERR, "%s(), %s:%u.  Internal ERROR "    \
                           "%d, bailing out (PAM_IGNORE)",                    \
                           __FUNCTION__, __FILE__, __LINE__, _err);           \
                _pamval = PAM_IGNORE;                                         \
            }                                                                 \
            bail_debug_pam();                                                 \
        }                                                                     \
        else if (_pamval) {                                                   \
            __do_goto = 1;                                                    \
            bail_debug_pam();                                                 \
        }                                                                     \
        if (__do_goto) {                                                      \
            goto bail;                                                        \
        }                                                                     \
    } while (0)


/* ------------------------------------------------------------------------- */
/* Return back to PAM from a function called directly by PAM.
 * 'err' is our own indicator for internal errors.  'pamval' is a PAM
 * return value made up by someone along the way.  Our general policy
 * is to break on internal errors, so if we have an internal error,
 * we should NOT have a PAM error too.  However, on the off chance that
 * this does happen, and they said "onerr=ignore", we don't want to end
 * up ignoring something where there was a PAM error waiting there!
 */
#define PAM_TBN_RETURN(_pamh, _pto, _err, _pamval)                            \
    do {                                                                      \
        const char *_log_str = NULL;                                          \
        int _log_level = 0;                                                   \
        if (_err == SQLITE_BUSY || _err == SQLITE_LOCKED) {                   \
            _log_str = "Database was busy";                                   \
            _log_level = LOG_INFO; /* Someone else already logged above */    \
        }                                                                     \
        else if (_err) {                                                      \
            _log_str = "Internal ERROR";                                      \
            _log_level = LOG_WARNING;                                         \
            pam_info(_pamh, "An internal error occurred.");                   \
        }                                                                     \
        if (_pto->pto_fail_on_error) {                                        \
            if (_err) {                                                       \
                _pamval = PAM_SYSTEM_ERR;                                     \
                pam_syslog(_pamh, LOG_WARNING, "%s; returning failure "       \
                           "(PAM_SYSTEM_ERR)", _log_str);                     \
            }                                                                 \
        }                                                                     \
        else {                                                                \
            /* Don't override non-success values */                           \
            if (_err && (_pamval == PAM_SUCCESS || _pamval == PAM_IGNORE)) {  \
                _pamval = PAM_IGNORE;                                         \
                pam_syslog(_pamh, _log_level, "%s; abstaining "               \
                           "(PAM_IGNORE)", _log_str);                         \
            }                                                                 \
        }                                                                     \
        pam_syslog(_pamh, LOG_DEBUG, "Returning PAM code %d", _pamval);       \
        return(_pamval);                                                      \
    } while(0)


/* ------------------------------------------------------------------------- */
static int
pam_tbn_strtol(const char *str, long *ret_num)
{
    int err = 0;
    long num = 0;
    char *endp = NULL;

    errno = 0;
    num = strtol(str, &endp, 10);
    if (errno != 0 || endp == str || !endp || *endp) {
        err = TBN_ERR_GENERIC;
    }

 bail:
    if (ret_num) {
        *ret_num = num;
    }
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
pam_tbn_downcase(const char *username_const, char **ret_username)
{
    int err = 0;
    char *username = NULL;
    int i = 0, len = 0;

    /* XXXX/EMT: I18N: UTF-8 (bug 15003) */

    username = strdup(username_const);
    if (username == NULL) {
        err = 1;
        goto bail;
    }

    len = strlen(username);
    for (i = 0; i < len; ++i) {
        if ((unsigned char)(username[i]) < 0x80) {
            username[i] = tolower(username[i]);
        }
    }

 bail:
    if (ret_username) {
        *ret_username = username;
    }
    else if (username) {
        free(username);
        username = NULL;
    }
    return(err);
}


/* ------------------------------------------------------------------------- */
/* Get the username we should use.  The manner of this differs
 * depending on when we are called.  Too early and LOGIN_USER is not
 * set yet, and we have to rely on pam_get_user().  To late, and
 * LOGIN_USER is set, but pam_get_user() may no longer return what we
 * want, if the LOGIN_USER has gotten mapped to a different AUTH_USER.
 *
 * So we have to take LOGIN_USER if it is set; otherwise, use
 * pam_get_user().
 */
static int
pam_tbn_get_username(pam_handle_t *pamh, int downcase, char **ret_username,
                     int *ret_pamval)
{
    int err = 0;
    int pamval = 0;
    const char *username_const = NULL;
    char *username = NULL;
    int i = 0, len = 0;
    unsigned char ch = 0;
    int bad_char = 0;

    if (pamh == NULL) {
        err = TBN_ERR_GENERIC;
        goto bail;
    }

    /*
     * The man page for pam_getenv() says pretty clearly:
     *
     *    The return values are of the form: "name=value".
     *
     * However, this is just not true.  It gives you the value alone.
     * This is seen empirically and verified in the implementation of
     * pam_getenv().
     */
    username_const = pam_getenv(pamh, "LOGIN_USER");
    if (username_const == NULL) {
        err = pam_get_user(pamh, &username_const, NULL);
        if (err == PAM_CONV_ERR) {
            /*
             * Bug 14763: when there's a NULL username, under EL5 we'll
             * get back a NULL with no error -- (username_const == NULL)
             * check below catches this.  But under EL6, we get back
             * PAM_CONV_ERR instead.  Treat it the same way.
             */
            err = 0;
            pam_syslog(pamh, LOG_INFO, "Rejecting missing username");
            pamval = PAM_USER_UNKNOWN;
            username_const = NULL; /* Probably is already, but just be sure */
            goto bail;
        }
        else if (err) {
            goto bail;
        }
    }

    if (username_const == NULL) {
        pam_syslog(pamh, LOG_INFO, "Rejecting bad username (null string)");
        pamval = PAM_USER_UNKNOWN;
        goto bail;
    }

    if (downcase) {
        err = pam_tbn_downcase(username_const, &username);
        if (err) {
            pam_syslog(pamh, LOG_DEBUG, "Failed to downcase username '%s', "
                       "rejecting", username_const);
            pam_syslog(pamh, LOG_INFO, "Failed to downcase username, "
                       "rejecting");
            pamval = PAM_USER_UNKNOWN;
            err = 0;
            goto bail;
        }

        if (!strcmp(username_const, username)) {
            pam_syslog(pamh, LOG_DEBUG, "Username '%s': was already "
                       "lowercase", username);
        }
        else {
            pam_syslog(pamh, LOG_DEBUG, "Username '%s': downcased to '%s'",
                       username_const, username);
        }
    }
    else {
        username = strdup(username_const);
        if (username == NULL) {
            pam_syslog(pamh, LOG_WARNING, "Failed to allocate space for "
                       "username");
            err = TBN_ERR_GENERIC;
            goto bail;
        }

        if (!strcmp(username_const, username)) {
            pam_syslog(pamh, LOG_DEBUG, "Username '%s': not attempting "
                       "downcasing", username);
        }
    }
    
    len = strlen(username);
    if (len == 0) {
        pam_syslog(pamh, LOG_INFO, "Rejecting bad username (empty string)");
        pamval = PAM_USER_UNKNOWN;
        goto bail;
    }

    for (i = 0; i < len; ++i) {
        ch = (unsigned char)(username[i]);
        /*
         * Don't mess with anything >=128, since we could be stomping
         * on some UTF-8 stuff.
         */
        if ((ch < 32 && ch != '\t') || ch == 127) {
            bad_char = 1;
            break;
        }
    }

    if (bad_char) {
        if (aaa_log_unknown_usernames_flag) {
            pam_syslog(pamh, LOG_INFO, "Rejecting bad username (contains "
                       "byte %u): %s", (unsigned char)(username[i]), username);
        }
        else {
            pam_syslog(pamh, LOG_DEBUG, "Rejecting bad username (contains "
                       "byte %u): %s", (unsigned char)(username[i]), username);
            pam_syslog(pamh, LOG_INFO, "Rejecting bad username (contains "
                       "byte %u)", (unsigned char)(username[i]));
        }
        /*
         * XXX/EMT: we're doing this test because we don't want to
         * pollute our database with usernames with weird characters.
         * It's not technically our place to reject users, so it's
         * tempting to return PAM_IGNORE.  But we don't want to take
         * the chance that somebody could get a username with one of
         * these characters into a remote auth server, and then with
         * PAM_IGNORE we'd be abdicating our responsibility to enforce
         * for that user.  So be safe and just reject it.
         */
        pamval = PAM_USER_UNKNOWN;
        goto bail;
    }

 bail:
    if (ret_username) {
        *ret_username = username;
    }
    else if (username) {
        free(username);
        username = NULL;
    }
    if (ret_pamval) {
        *ret_pamval = pamval;
    }
    return(err);
}


/* ------------------------------------------------------------------------- */
/* Because of our policy of ignoring any bad options, there is no way this
 * can fail.  We complain about whatever we don't like, and ingest whatever
 * we do like.
 */
static void
pam_tbn_read_options(pam_handle_t *pamh, int argc, const char *argv[],
                     pam_tbn_phase phase, pam_tbn_options *ret_pto)
{
    int err = 0;
    int i = 0;
    unsigned int len = 0;
    char arg[1024];
    char *eq = NULL, *comma = NULL;
    const char *name = NULL;
    char *value = NULL;
    int badopt = 0;
    long num = 0;
    pam_tbn_class_override override = ptco_none;

    for (i = 0; i < argc; ++i) {
        if (argv[i] == NULL) {
            continue;
        }

#ifdef PAM_TBN_DEBUG
        pam_syslog(pamh, LOG_INFO, "Argv[%d] = %s", i, argv[i]);
#endif

        len = strlen(argv[i]);
        if (len >= sizeof(arg)) {
            pam_syslog(pamh, LOG_WARNING, "Ignoring option that is too long: "
                       "%s", argv[i]);
            continue;
        }

        strncpy(arg, argv[i], sizeof(arg));
        /* Due to above test, we know this will be NUL-terminated */

        name = arg;
        value = NULL;
        eq = strchr(arg, '=');
        if (eq) {
            *eq = '\0';
            value = eq + 1;
        }

        /*
         * In theory we could have options which did not take a value.
         * However, we currently do not, so centralize the check.
         */
        if (value == NULL) {
            pam_syslog(pamh, LOG_WARNING, "Ignoring option with no value: %s",
                       argv[i]);
            continue;
        }

        badopt = 0;

        if (!strcmp(name, "mode")) {
            if (!strcmp(value, "check")) {
                ret_pto->pto_mode = ptm_check;
                if (phase != ptp_auth) {
                    pam_syslog(pamh, LOG_WARNING, "Ignoring option only good "
                               "for 'auth' mode: %s", argv[i]);
                    continue;
                }
            }
            else if (!strcmp(value, "failure")) {
                ret_pto->pto_mode = ptm_failure;
                if (phase != ptp_auth) {
                    pam_syslog(pamh, LOG_WARNING, "Ignoring option only good "
                               "for 'auth' mode: %s", argv[i]);
                    continue;
                }
            }
            else if (!strcmp(value, "success")) {
                ret_pto->pto_mode = ptm_success;
                if (phase != ptp_account) {
                    pam_syslog(pamh, LOG_WARNING, "Ignoring option only good "
                               "for 'account' mode: %s", argv[i]);
                    continue;
                }
            }
            else {
                badopt = 1;
            }
        }

        else if (!strcmp(name, "onerr")) {
            if (!strcmp(value, "fail")) {
                ret_pto->pto_fail_on_error = 1;
            }
            else if (!strcmp(value, "ignore")) {
                ret_pto->pto_fail_on_error = 0;
            }
            else {
                badopt = 1;
            }
        }

        else if (!strcmp(name, "deny")) {
            err = pam_tbn_strtol(value, &num);
            if (err || num < 0) {
                badopt = 1;
            }
            else {
                ret_pto->pto_deny = num;
            }
        }

        else if (!strcmp(name, "unlock_time")) {
            err = pam_tbn_strtol(value, &num);
            if (err || num < 0) {
                badopt = 1;
            }
            else {
                ret_pto->pto_unlock_time = num;
            }
        }

        else if (!strcmp(name, "lock_time")) {
            err = pam_tbn_strtol(value, &num);
            if (err || num < 0) {
                badopt = 1;
            }
            else {
                ret_pto->pto_lock_time = num;
            }
        }

        else if (!strcmp(name, "override_admin") ||
                 !strcmp(name, "override_unknown")) {
            if (!strcmp(value, "none")) {
                override = ptco_none;
            }
            else if (!strcmp(value, "no_track")) {
                override = ptco_no_track;
            }
            else if (!strcmp(value, "no_lockout")) {
                override = ptco_no_lockout;
            }
            else {
                badopt = 1;
            }
            if (!badopt) {
                if (!strcmp(name, "override_admin")) {
                    ret_pto->pto_override_admin = override;
                }
                else if (!strcmp(name, "override_unknown")) {
                    ret_pto->pto_override_unknown = override;
                }
            }
        }

        else if (!strcmp(name, "map_unknown")) {
            err = pam_tbn_strtol(value, &num);
            if (err || num < tum_none || num > tum_LAST) {
                badopt = 1;
            }
            else {
                ret_pto->pto_map_unknown = (tbn_username_mapping)num;
            }
        }

        else if (!strcmp(name, "downcase")) {
            err = pam_tbn_strtol(value, &num);
            if (err || (num != 0 && num != 1)) {
                badopt = 1;
            }
            else {
                ret_pto->pto_downcase = (num == 1);
            }
        }

        else if (!strcmp(name, "max_recs")) {
            comma = strchr(value, ',');
            if (comma == NULL) {
                badopt = 1;
            }
            else {
                *comma = '\0';
                err = pam_tbn_strtol(value, &num);
                if (err || num < 0) {
                    badopt = 1;
                }
                else {
                    ret_pto->pto_max_recs = num;
                    err = pam_tbn_strtol(comma + 1, &num);
                    if (err || num < 0) {
                        badopt = 1;
                    }
                    else {
                        ret_pto->pto_max_recs_check_freq = num;
                    }
                }
            }
        }

        else {
            pam_syslog(pamh, LOG_WARNING, "Ignoring unrecognized option: "
                       "%s", argv[i]);
            continue;
        }

        if (badopt) {
            pam_syslog(pamh, LOG_WARNING, "Ignoring option with bad value: "
                       "%s", argv[i]);
            continue;
        }
    }

    /*
     * Warn them if they set any options that are not appropriate for
     * this mode.  We can't do that until the end, since they might not
     * have put the mode first.  We also check the mode here, since
     * setting no mode implicitly selects "check".
     */
    switch (ret_pto->pto_mode) {
    case ptm_check:
    case ptm_failure:
        if (phase != ptp_auth) {
            pam_syslog(pamh, LOG_WARNING, "Can only use mode '%s' in 'auth' "
                       "phase.  Changing mode to 'success'",
                       ret_pto->pto_mode == ptm_check ? "check" : "failure");
            ret_pto->pto_mode = ptm_success;
        }
        break;

    case ptm_success:
        if (phase != ptp_account) {
            pam_syslog(pamh, LOG_WARNING, "Can only use mode 'success' in "
                       "'account' phase.  Changing mode to 'check'");
            ret_pto->pto_mode = ptm_check;
        }
        break;

    default:
        /* ??? */
        break;
    }

    /* XXX/EMT: other sanity checks? */

 bail:
    return;
}


/* ------------------------------------------------------------------------- */
/* Try to look up a record for the specified user.  'mapping' tells us
 * what mapping has already been applied to the username given to us
 * in 'username', so we can include that in the match criteria for our
 * lookup.
 */
static int
pam_tbn_check_one_user(pam_handle_t *pamh, sqlite3 *db, pam_tbn_options *pto,
                       const char *username, tbn_username_mapping mapping,
                       int *ret_found, int *ret_locked,
                       int *ret_last_fail_time, int *ret_pamval)
{
    int err = 0;
    int pamval = 0;
    const char *sql = NULL;
    sqlite3_stmt *stmt = NULL;
    int found = 0, locked = 0, last_fail_time = 0;

    sql = "SELECT " TBN_FIELD_LOCKED " , " TBN_FIELD_LAST_FAILURE_TIME
        " FROM " TBN_TABLE_NAME
        " WHERE " TBN_FIELD_USERNAME " = " TBN_BPARAM_USERNAME_STR
        " AND " TBN_FIELD_USERNAME_MAPPED " = " TBN_BPARAM_USERNAME_MAPPED_STR;

    sqlite_utils_finalize(&stmt);
    err = sqlite3_prepare(db, sql, -1, &stmt, NULL);
    bail_error_sqlite(err, db, sql);

    err = sqlite3_bind_text(stmt, TBN_BPARAM_USERNAME, username,
                            -1, SQLITE_STATIC);
    bail_error_sqlite(err, db, sql);

    err = sqlite3_bind_int(stmt, TBN_BPARAM_USERNAME_MAPPED, mapping);
    bail_error_sqlite(err, db, sql);

    /*
     * Five things can happen here:
     *   1. Good: we get our answer, then finish (SQLITE_ROW, SQLITE_DONE).
     *   2. Good: we get no answer (SQLITE_DONE), meaning the user has no
     *      record, and therefore we default to zero.
     *   3. Bad: we get our answer, then another row (SQLITE_ROW, SQLITE_ROW).
     *      (This would be disturbing since we registered username as a
     *      primary key!)
     *   4. Bad: we get our answer, then an error (SQLITE_ROW, !=SQLITE_DONE).
     *   5. Bad: we get an error right off (!=SQLITE_ROW && !=SQLITE_DONE).
     *
     * We could pick ourselves up after all of the bad cases (3 and 4 could
     * be like 1; and 5 could be like 2); however, ultimately the 'onerr'
     * setting (pam_tbn_fail_on_error) will tell us what to do.
     *
     * XXX/EMT: when referencing 'username' below, should reflect any
     * transformations to 'fixed_username'.
     */
    err = sqlite_utils_step(stmt);
    if (err == SQLITE_ROW) {
        found = 1;
        locked = sqlite3_column_int(stmt, 0);
        last_fail_time = sqlite3_column_int(stmt, 1);
        err = sqlite_utils_step(stmt);
        if (err == SQLITE_DONE) {
            /* Case 1: Good */
            err = 0;
        }
        else if (err == SQLITE_ROW) {
            /* Case 3: Bad */
            pam_syslog(pamh, LOG_WARNING, "Got multiple rows fetching data "
                       "for user '%s'.", username);
            err = TBN_ERR_GENERIC;
        }
        else {
            /* Case 4: Bad */
            pam_syslog(pamh, LOG_WARNING, "Got error after fetching data "
                       "for user '%s'.  Ignoring.", username);
            err = TBN_ERR_GENERIC;
        }
    }
    else if (err == SQLITE_DONE) {
        /* Case 2: Fine, user had no record */
        err = 0;
        found = 0;
        locked = 0;
        last_fail_time = 0;
    }
    else {
        /* Case 5: Bad */
        pam_syslog(pamh, LOG_WARNING, "Error %d fetching data "
                   "for user '%s', bailing out.", err, username);
        err = TBN_ERR_GENERIC;
    }

    bail_error_pam(pamh, pto, err, pamval);

 bail:
    if (ret_found) {
        *ret_found = found;
    }
    if (ret_locked) {
        *ret_locked = locked;
    }
    if (ret_last_fail_time) {
        *ret_last_fail_time = last_fail_time;
    }
    if (ret_pamval) {
        *ret_pamval = pamval;
    }
    sqlite_utils_finalize(&stmt);
    return(err);
}


/* ------------------------------------------------------------------------- */
/* Check whether the specified user is OK to log in, given their history of
 * login failures, and our configuration.  ret_pamval will get PAM_SUCCESS
 * or PAM_MAXTRIES accordingly.
 *
 * XXXX/EMT (bug 14599): here and in pam_sm_authenticate(), we log the
 * username at LOG_NOTICE.  We have the hashing feature to try to keep
 * from recording accidental passwords and such entered as usernames,
 * and this would seem to somewhat defeat that.  It's not quite as
 * bad, since you have to do it enough times to get the account locked
 * to get anything logged.  But since such records may not get reset
 * for a long while, enough failed logins could build up slowly over
 * time.
 */
static int
pam_tbn_check_ok(pam_handle_t *pamh, sqlite3 *db, pam_tbn_options *pto,
                 const char *username, tbn_username_mapping expected_mapping,
                 int *ret_pamval)
{
    int err = 0, pamval = 0;
    int found = 0, locked = 0, clock_change = 0, allow = 0;
    intptr_t we_caused_failure = 0;
    int last_fail_time = 0, now = 0, since_last_fail = 0;
    char *username_trunc = NULL, *username_mapped = NULL;
    const char *username1 = NULL, *username2 = NULL;
    tbn_username_mapping mapping1 = tum_none, mapping2 = tum_none;
    int is_known = 0;

    if (pamh == NULL || db == NULL || username == NULL) {
        err = TBN_ERR_GENERIC;
        goto bail;
    }

    /*
     * We are purposefully using tum_hash here instead of
     * expected_mapping.  It doesn't matter what the current config
     * is; we might still find accounts recorded the other way.
     *
     * We will use expected_mapping to order our checks, though; this
     * is an optimization based on the assumption that the account is
     * probably already in the right place.
     */ 
    err = tbn_string_fixup(username, &username_trunc, NULL,
                           tum_hash, &username_mapped);
    bail_error_pam(pamh, pto, err, pamval);

    if (expected_mapping == tum_none) {
        username1 = username_trunc;
        mapping1 = tum_none;
        username2 = username_mapped;
        mapping2 = expected_mapping;
    }
    else {
        username1 = username_mapped;
        mapping1 = expected_mapping;
        username2 = username_trunc;
        mapping2 = tum_none;
    }

    /* ........................................................................
     * Fetch data: whether this account is locked, and the time of their
     * last failure.  The former is obviously our first stop in checking
     * if login is OK; the latter can help override the former in light
     * of lock_time or unlock_time.
     *
     * Note that we break this into two queries, instead of a single
     * query with an 'OR', because "explain query plan" only shows
     * "WITH INDEX sqlite_autoindex_users_1" with the single username.
     * We figure that two queries with the index are probably better
     * than one without.
     */
    err = pam_tbn_check_one_user(pamh, db, pto, username1, mapping1,
                                 &found, &locked, &last_fail_time,
                                 &pamval);
    bail_error_pam(pamh, pto, err, pamval);
    if (!found && username2) {
        err = pam_tbn_check_one_user(pamh, db, pto, username2, mapping2,
                                     &found, &locked, &last_fail_time,
                                     &pamval);
        bail_error_pam(pamh, pto, err, pamval);
    }

    /* ........................................................................
     * Compute times.  If the last fail time is in the future, this is
     * suggestive that someone changed the system clock.  In that
     * case, we will err on the side of permitting login.  That is,
     * assume it has been enough time since the last login, if that
     * matters at all.  In other words,
     *   - Ignore lock_time.
     *   - If unlock_time is set at all, permit login.
     *   - If unlock_time is not set, obey the lock.
     *
     * Being permissive like this will allow them to try again if at
     * all possible, and the last_fail_time will be set to something
     * consistent with the new clock.  So presumably we won't find
     * ourselves here again and again, unless the user is locked and
     * unlock_time is not set.  In that case, the clock didn't matter
     * anyway.
     */

    now = tbn_curr_time();
    if (now < last_fail_time) {
        clock_change = 1;
    }
    else {
        since_last_fail = now - last_fail_time;
    }

    /* ........................................................................
     * We have all the data, and now have only to make the decision.
     */

    err = tbn_username_is_known(username, &is_known);
    if (err) {
        goto bail;
    }

    if (!found) {
        pam_syslog(pamh, LOG_DEBUG, "Permitting access to user '%s': "
                   "account has no recorded authentication failures since "
                   "last success", username);
        allow = 1;
    }

    else if (clock_change) {
        if (pto->pto_unlock_time > 0) {
            pam_syslog(pamh, LOG_DEBUG, "Permitting access to user '%s': "
                       "clock changed, and unlock_time set, so giving them "
                       "the benefit of the doubt.", username);
            allow = 1;
        }
        else if (locked) {
            if (is_known || aaa_log_unknown_usernames_flag) {
                pam_syslog(pamh, LOG_NOTICE, "Denying access to user '%s': "
                           "Maximum number of failed logins reached, "
                           "account locked.  (Clock changed, and unlock_time "
                           "not set)", username);
            }
            else {
                pam_syslog(pamh, LOG_DEBUG, "Denying access to user '%s': "
                           "Maximum number of failed logins reached, "
                           "account locked.  (Clock changed, and unlock_time "
                           "not set)", username);
                pam_syslog(pamh, LOG_NOTICE, "Denying access to unknown user: "
                           "Maximum number of failed logins reached, "
                           "account locked.  (Clock changed, and unlock_time "
                           "not set)");
            }
            pam_info(pamh, "Maximum number of failed logins reached, "
                     "account locked.");
            allow = 0;
        }
        else {
            pam_syslog(pamh, LOG_DEBUG, "Permitting access to user '%s': "
                       "clock changed, unlock time not set, and account "
                       "not locked.", username);
            allow = 1;
        }
    }

    else if (locked) {
        if (pto->pto_unlock_time > 0 &&
            since_last_fail >= pto->pto_unlock_time) {
            pam_syslog(pamh, LOG_DEBUG, "Permitting access to user '%s': "
                       "account locked, but unlock_time has elapsed since "
                       "last failure", username);
            allow = 1;
        }
        else if (pto->pto_unlock_time > 0) {
            if (is_known || aaa_log_unknown_usernames_flag) {
                pam_syslog(pamh, LOG_NOTICE, "Denying access to user '%s': "
                           "Maximum number of failed logins reached, "
                           "account locked.  "
                           "You may try again in %ld second(s).", username,
                           pto->pto_unlock_time - since_last_fail);
            }
            else {
                pam_syslog(pamh, LOG_DEBUG, "Denying access to user '%s': "
                           "Maximum number of failed logins reached, "
                           "account locked.  "
                           "You may try again in %ld second(s).", username,
                           pto->pto_unlock_time - since_last_fail);
                pam_syslog(pamh, LOG_NOTICE, "Denying access to unknown user: "
                           "Maximum number of failed logins reached, "
                           "account locked.  "
                           "You may try again in %ld second(s).",
                           pto->pto_unlock_time - since_last_fail);
            }
            pam_info(pamh, "Maximum number of failed logins reached, "
                     "account locked.\n"
                     "You may try again in %ld second(s).",
                     pto->pto_unlock_time - since_last_fail);
            allow = 0;
        }
        else {
            if (is_known || aaa_log_unknown_usernames_flag) {
                pam_syslog(pamh, LOG_NOTICE, "Denying access to user '%s': "
                           "Maximum number of failed logins reached, "
                           "account locked.  Unlock_time is disabled.",
                           username);
            }
            else {
                pam_syslog(pamh, LOG_DEBUG, "Denying access to user '%s': "
                           "Maximum number of failed logins reached, "
                           "account locked.  Unlock_time is disabled.",
                           username);
                pam_syslog(pamh, LOG_NOTICE, "Denying access to unknown user: "
                           "Maximum number of failed logins reached, "
                           "account locked.  Unlock_time is disabled.");
            }
            pam_info(pamh, "Maximum number of failed logins reached, "
                     "account locked.");
            allow = 0;
        }
    }

    else { /* (!locked) */
        if (pto->pto_lock_time > 0 &&
            since_last_fail < pto->pto_lock_time) {
            if (is_known || aaa_log_unknown_usernames_flag) {
                pam_syslog(pamh, LOG_NOTICE, "Denying access to user '%s': "
                           "Account temporarily locked due to recent "
                           "authentication failure.  "
                           "You may try again in %ld second(s).",
                           username, pto->pto_lock_time - since_last_fail);
            }
            else {
                pam_syslog(pamh, LOG_DEBUG, "Denying access to user '%s': "
                           "Account temporarily locked due to recent "
                           "authentication failure.  "
                           "You may try again in %ld second(s).",
                           username, pto->pto_lock_time - since_last_fail);
                pam_syslog(pamh, LOG_NOTICE, "Denying access to unknown user: "
                           "Account temporarily locked due to recent "
                           "authentication failure.  "
                           "You may try again in %ld second(s).",
                           pto->pto_lock_time - since_last_fail);
            }
            pam_info(pamh, "Account temporarily locked due to recent "
                     "authentication failure.\n"
                     "You may try again in %ld second(s).",
                     pto->pto_lock_time - since_last_fail);
            allow = 0;
        }
        else if (pto->pto_lock_time > 0) {
            pam_syslog(pamh, LOG_DEBUG, "Permitting access to user '%s': "
                       "account is not locked, and lock_time has "
                       "already elapsed.", username);
            allow = 1;
        }
        else {
            pam_syslog(pamh, LOG_DEBUG, "Permitting access to user '%s': "
                       "account is not locked, and lock_time is disabled.",
                       username);
            allow = 1;
        }
    }

    /*
     * We need to remember that we were the one who caused the
     * failure, so our failure handler will know not to record it as a
     * real authentication failure.
     */
    we_caused_failure = !allow;
    err = pam_set_data(pamh, TBN_MODULE_NAME, (void *)we_caused_failure, NULL);
    bail_error_pam(pamh, pto, err, pamval);

    pamval = allow ? PAM_SUCCESS : PAM_MAXTRIES;

 bail:
    if (username_trunc) {
        free(username_trunc);
    }
    if (username_mapped) {
        free(username_mapped);
    }
    if (ret_pamval) {
        *ret_pamval = pamval;
    }
    return(err);
}


/* ------------------------------------------------------------------------- */
PAM_EXTERN int
pam_sm_authenticate(pam_handle_t *pamh, int flags,
                    int argc, const char *argv[])
{
    int err = 0, err2 = 0, pamval = 0;
    pam_tbn_options lpto = PAM_TBN_OPTIONS_INIT;
    pam_tbn_options *pto = NULL;
    struct stat stats;
    sqlite3 *db = NULL;
    char *username = NULL;
    int is_admin = 0, is_known = 0;
    int no_lockout = 0, no_track = 0;
    const char *remote_host = NULL;
    const char *tty = NULL;
    const void *we_caused_failure_v = NULL;
    intptr_t we_caused_failure = 0;
    int user_existed = 0, locked_user = 0;
    sqlite_utils_transaction_mode xaction_mode = sutm_none;
    tbn_username_mapping mapping = tum_none;

    pam_syslog(pamh, LOG_DEBUG, "pam_sm_authenticate()");
    pam_tbn_read_options(pamh, argc, argv, ptp_auth, &lpto);
    pto = &lpto;
    pam_syslog(pamh, LOG_DEBUG, "Mode of operation: %s",
               (pto->pto_mode == ptm_check) ? "check" :
               "failure");
    
    /*
     * tbn_open_db_ensure_tables() is about to do this same check, but
     * its logging is not as nice.  Pre-check so we can do a good log
     * message.  If it changes right after, we'll get a worse log
     * message, or one of the other minor TOCTOU results as described
     * in tbn_open_db_ensure_tables().
     */
    errno = 0;
    err = lstat(TBN_DEFAULT_LOGFILE, &stats);
    if (err && errno == ENOENT) {
        pam_syslog(pamh, LOG_INFO, "Tally log file '%s' does not exist; "
                   "abstaining (PAM_IGNORE)", TBN_DEFAULT_LOGFILE);
        err = 0;
        errno = 0;
        pamval = PAM_IGNORE;
        goto bail;
    }

    err = pam_tbn_get_username(pamh, pto->pto_downcase, &username, &pamval);
    bail_error_pam(pamh, pto, err, pamval);

    switch (pto->pto_mode) {
    case ptm_check:
        xaction_mode = sutm_deferred;
        break;

    case ptm_failure:
        xaction_mode = sutm_exclusive;
        break;

    default:
        err = TBN_ERR_GENERIC;
        goto bail;
    }

    err = tbn_open_db_ensure_tables(NULL, 0, xaction_mode, &db);
    if (err == SQLITE_BUSY || err == SQLITE_LOCKED) {
        goto bail;
    }
    bail_error_pam(pamh, pto, err, pamval);

    is_admin = !strcmp(username, pam_tbn_username_admin);
    err = tbn_username_is_known(username, &is_known);
    if (err) {
        goto bail;
    }

    err = pam_get_data(pamh, TBN_MODULE_NAME, &we_caused_failure_v);
    if (err == PAM_NO_MODULE_DATA) {
        err = 0;
    }
    else {
        bail_error_pam(pamh, pto, err, pamval);
        we_caused_failure = (intptr_t)we_caused_failure_v;
    }

    if (we_caused_failure) {
        pam_syslog(pamh, LOG_DEBUG, "Not recording authentication failure "
                   "because we were the ones to deny login");
    }

    no_track = (we_caused_failure ||
                (is_admin && pto->pto_override_admin == ptco_no_track) ||
                (!is_known && pto->pto_override_unknown == ptco_no_track));
    no_lockout = (no_track ||
                  (is_admin && pto->pto_override_admin != ptco_none) ||
                  (!is_known && pto->pto_override_unknown != ptco_none));

    mapping = is_known ? tum_none : pto->pto_map_unknown;

    switch (pto->pto_mode) {
    case ptm_check:
        if (no_lockout) {
            /* XXX/EMT: logging, especially for overrides */
        }
        else {
            /*
             * XXXX/EMT: bug 14541: this is where we'd do a sleep() if
             * we wanted to.  If !we_caused_failure AND time left is
             * less than a threshold.  See pto->pto_lock_time.
             */

            err = pam_tbn_check_ok(pamh, db, pto, username, mapping, &pamval);
            bail_error_pam(pamh, pto, err, pamval);
        }
        break;

    case ptm_failure:
        if (no_track) {
            /* XXX/EMT: logging, especially for overrides */
        }
        else {
            pam_get_item(pamh, PAM_RHOST, (const void **)&remote_host);
            if (!remote_host) {
                remote_host = "";
            }
            pam_get_item(pamh, PAM_TTY, (const void **)&tty);
            if (!tty) {
                tty = "";
            }
            err = tbn_record_failure(db, username, mapping,
                                     is_known, remote_host, tty,
                                     pto->pto_max_recs,
                                     pto->pto_max_recs_check_freq);
            bail_error_pam(pamh, pto, err, pamval);
        }

        if (no_lockout) {
            /* XXX/EMT: logging, especially for overrides */
        }
        else if (pto->pto_deny > 0) {
            err = tbn_maybe_lock_user(db, username, mapping,
                                      pto->pto_deny, &user_existed,
                                      &locked_user);
            bail_error_pam(pamh, pto, err, pamval);

            if (!user_existed) {
                /*
                 * This is a surprise, since we just recorded a
                 * failure for the user.  However, it could be a race,
                 * since we're not atomic with the previous operation;
                 * someone could have just reset the user.  Assume
                 * that's what happened, rather than something more
                 * sinister.
                 */
            }
            else if (locked_user) {
                if (is_known || aaa_log_unknown_usernames_flag) {
                    pam_syslog(pamh, LOG_NOTICE, "Too many login failures for "
                               "user '%s': account now locked.", username);
                }
                else {
                    pam_syslog(pamh, LOG_DEBUG, "Too many login failures for "
                               "user '%s': account now locked.", username);
                    pam_syslog(pamh, LOG_NOTICE, "Too many login failures for "
                               "unknown user: account now locked.");
                }
            }
        }

        break;

    default:
        err = TBN_ERR_GENERIC;
        break;
    }

 bail:
    if (err == SQLITE_BUSY || err == SQLITE_LOCKED) {
        pam_syslog(pamh, LOG_WARNING, "WARNING: database was busy: skipping "
                   "lockout check for user %s%s%s",
                   username ? "'" : "",
                   username ? username : "UNKNOWN",
                   username ? "'" : "");
    }
    err2 = sqlite_utils_close(&db, 1);
    if (err2) {
        pam_syslog(pamh, LOG_WARNING, "WARNING: could not close database "
                   "file from pam_sm_authenticate()");
    }
    if (username) {
        free(username);
        username = NULL;
    }
    PAM_TBN_RETURN(pamh, pto, err, pamval);
}


/* ------------------------------------------------------------------------- */
static int
pam_tbn_reset_user_one(pam_handle_t *pamh, sqlite3 *db, pam_tbn_options *pto,
                       const char *username_orig, const char *username_fixed,
                       tbn_username_mapping mapping)
{
    int err = 0, pamval = 0;
    const char *sql = NULL;
    sqlite3_stmt *stmt = NULL;
    int record_found = 0;
    int was_locked = 0;

    /* ........................................................................
     * First, find out if the user has a record at all, and if so, if
     * their account was locked.  This is for two purposes:
     *   - If they don't have a record, we can avoid issuing the DELETE
     *     statement, and therefore ever taking a write lock.
     *     (We assume that it will take a write lock for the 'DELETE'
     *     even if there is no matching record to delete.)
     *   - If their account was locked, we want to log at NOTICE that it
     *     is now being unlocked.
     */
    sql = "SELECT " TBN_FIELD_LOCKED " FROM " TBN_TABLE_NAME
        " WHERE " TBN_FIELD_USERNAME " = " TBN_BPARAM_USERNAME_STR
        " AND " TBN_FIELD_USERNAME_MAPPED " = " TBN_BPARAM_USERNAME_MAPPED_STR;
    
    sqlite_utils_finalize(&stmt);
    err = sqlite3_prepare(db, sql, -1, &stmt, NULL);
    bail_error_sqlite(err, db, sql);

    err = sqlite3_bind_text(stmt, TBN_BPARAM_USERNAME, username_fixed,
                            -1, SQLITE_STATIC);
    bail_error_sqlite(err, db, sql);

    err = sqlite3_bind_int(stmt, TBN_BPARAM_USERNAME_MAPPED, mapping);
    bail_error_sqlite(err, db, sql);

    err = sqlite_utils_step(stmt);
    if (err == SQLITE_ROW) {
        record_found = 1;
        was_locked = sqlite3_column_int(stmt, 0);
        err = sqlite_utils_step(stmt);
    }
    /*
     * There should never be more than one result, so we expect to be 
     * at SQLITE_DONE here.
     */
    bail_error_sqlite(err, db, sql);

    /* ........................................................................
     * If they had a record, delete it.
     */
    if (record_found) {
        /*
         * For now, we purge the whole record rather than just resetting it.
         * This is to help avoid the database from getting too large.
         */
        sql = "DELETE FROM " TBN_TABLE_NAME
            " WHERE " TBN_FIELD_USERNAME " = " TBN_BPARAM_USERNAME_STR
            " AND " TBN_FIELD_USERNAME_MAPPED " = "
            TBN_BPARAM_USERNAME_MAPPED_STR;

        sqlite_utils_finalize(&stmt);
        err = sqlite3_prepare(db, sql, -1, &stmt, NULL);
        bail_error_sqlite(err, db, sql);
        
        err = sqlite3_bind_text(stmt, TBN_BPARAM_USERNAME, username_fixed, -1,
                                SQLITE_STATIC);
        bail_error_sqlite(err, db, sql);

        err = sqlite3_bind_int(stmt, TBN_BPARAM_USERNAME_MAPPED, mapping);
        bail_error_sqlite(err, db, sql);
        
        err = sqlite_utils_step(stmt);
        bail_error_sqlite(err, db, sql);

        if (was_locked) {
            pam_syslog(pamh, LOG_NOTICE, "Successful login for user '%s': "
                       "account now unlocked, and authentication failure "
                       "history erased", username_orig);
        }
        else {
            pam_syslog(pamh, LOG_INFO, "Successful login for user '%s': "
                       "authentication failure history erased", username_orig);
        }
    }

 bail:
    sqlite_utils_finalize(&stmt);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
pam_tbn_reset_user(pam_handle_t *pamh, sqlite3 *db, pam_tbn_options *pto,
                   const char *username)
{
    int err = 0, pamval = 0;
    char *username_trunc = NULL, *username_mapped = NULL;
    tbn_username_mapping mapping_done = tum_none;

    if (pamh == NULL || db == NULL) {
        err = TBN_ERR_GENERIC;
        goto bail;
    }

    pam_syslog(pamh, LOG_DEBUG, "Resetting history of user '%s'",
               username);

    /*
     * We use tum_hash (instead of pto->pto_map_unknown) on purpose.
     * See pam_tbn_check_ok() comments.
     */
    err = tbn_string_fixup(username, &username_trunc, NULL,
                           tum_hash, &username_mapped);
    bail_error_pam(pamh, pto, err, pamval);

    /*
     * Since we're resetting both forms of the username anyway, we
     * don't need to do it in order like we do with check_ok and
     * record_failure.
     */
    err = pam_tbn_reset_user_one(pamh, db, pto, username, username_trunc,
                                 tum_none);
    bail_error_pam(pamh, pto, err, pamval);

    if (username_mapped) {
        err = pam_tbn_reset_user_one(pamh, db, pto, username, username_mapped,
                                     tum_hash);
        bail_error_pam(pamh, pto, err, pamval);
    }

 bail:
    if (username_trunc) {
        free(username_trunc);
    }
    if (username_mapped) {
        free(username_mapped);
    }
    return(err);
}


/* ------------------------------------------------------------------------- */
PAM_EXTERN int
pam_sm_setcred(pam_handle_t *pamh, int flags,
               int argc, const char **argv)
{
    return(PAM_IGNORE);
}


/* ------------------------------------------------------------------------- */
PAM_EXTERN int
pam_sm_acct_mgmt(pam_handle_t *pamh, int flags,
                 int argc, const char **argv)
{
    int err = 0, err2 = 0, pamval = 0;
    pam_tbn_options lpto = PAM_TBN_OPTIONS_INIT;
    pam_tbn_options *pto = NULL;
    char *username = NULL;
    struct stat stats;
    sqlite3 *db = NULL;

    pam_syslog(pamh, LOG_DEBUG, "pam_sm_acct_mgmt()");
    pam_tbn_read_options(pamh, argc, argv, ptp_account, &lpto);
    pto = &lpto;

    /*
     * Pre-check for better log message.
     */
    errno = 0;
    err = lstat(TBN_DEFAULT_LOGFILE, &stats);
    if (err && errno == ENOENT) {
        pam_syslog(pamh, LOG_INFO, "Tally log file '%s' does not exist; "
                   "abstaining (PAM_IGNORE)", TBN_DEFAULT_LOGFILE);
        err = 0;
        errno = 0;
        pamval = PAM_IGNORE;
        goto bail;
    }

    err = pam_tbn_get_username(pamh,  pto->pto_downcase, &username, &pamval);
    bail_error_pam(pamh, pto, err, pamval);

    err = tbn_open_db_ensure_tables(NULL, 0, sutm_deferred, &db);
    if (err == SQLITE_BUSY || err == SQLITE_LOCKED) {
        goto bail;
    }
    bail_error_pam(pamh, pto, err, pamval);

    err = pam_tbn_reset_user(pamh, db, pto, username);
    bail_error_pam(pamh, pto, err, pamval);

 bail:
    if (err == SQLITE_BUSY || err == SQLITE_LOCKED) {
        pam_syslog(pamh, LOG_WARNING, "WARNING: database was busy: skipping "
                   "recording of authentication result for user %s%s%s",
                   username ? "'" : "",
                   username ? username : "UNKNOWN",
                   username ? "'" : "");
    }
    err2 = sqlite_utils_close(&db, 1);
    if (err2) {
        pam_syslog(pamh, LOG_WARNING, "WARNING: could not close database "
                   "file from pam_sm_acct_mgmt()");
    }
    if (username) {
        free(username);
        username = NULL;
    }
    PAM_TBN_RETURN(pamh, pto, err, pamval);
}


/*-----------------------------------------------------------------------*/

#ifdef PAM_STATIC

/* static module data */

struct pam_module _pam_tally_modstruct = {
    TBN_MODULE_NAME,
    pam_sm_authenticate,
    pam_sm_setcred,
    pam_sm_acct_mgmt,
    NULL,
    NULL,
    NULL,
};

#endif   /* #ifdef PAM_STATIC */
