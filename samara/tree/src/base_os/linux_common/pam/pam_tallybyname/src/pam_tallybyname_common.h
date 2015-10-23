/*
 *
 * (C) Copyright 2013-2015 Juniper Networks, Inc. All Rights reserved.
 * (C) Copyright 2002-2013 Tall Maple Systems, Inc.
 * All rights reserved.
 * The contents of this file are subject to the terms of the MIT License.
 * You may not use this file except in compliance with the License.
 * See the file License.txt at the top of this source tree.
 * You can obtain a copy of the license at https://opensource.org/licenses/MIT
 *
 */

#ifndef __PAM_TALLYBYNAME_COMMON_H_
#define __PAM_TALLYBYNAME_COMMON_H_

#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif


/* ------------------------------------------------------------------------- */
/* NOTE: this file should not include any TMS headers (outside its own
 * directory), as it needs to be able to build within a PAM module,
 * and does not link with libcommon, etc.
 *
 * For booleans, we use ints instead, with 0 for false and 1 for true.
 */

#include "pam_common.h"
#include "sqlite_utils.h"

#define TBN_MODULE_NAME         "pam_tallybyname"
#define TBN_BINARY_NAME         TBN_MODULE_NAME

/*
 * This is repeated in our image_files Makefile, as well as the /var
 * upgrade script.
 */
#define TBN_DEFAULT_LOGFILE     "/var/log/tallynamelog"
#define TBN_TABLE_NAME          "users"

#define TBN_ERR_GENERIC  PAM_ERR_GENERIC

#ifndef aaa_tbn_db_busy_timeout_ms
#define aaa_tbn_db_busy_timeout_ms 7500
#endif

/* ----------------------------------------------------------------------------
 * Options for mapping a username.
 */
typedef enum {
    /*
     * No mapping: keep username as plaintext.
     */
    tum_none = 0,

    /*
     * 1: Hashed using HMAC-SHA-256-192, using the hash key defined in
     *    tbn_hash_key ("pam_tallybyname_hash_key_abcdefg"), then rendered
     *    to ASCII as Base32, using the same mapping as we do for licenses
     *    ("0123456789ABCDEFGHJKLMNPQRTUVWXY").
     */
    tum_hash = 1,

    /*
     * NOTE: there are some places in the code which assume there is
     * only one mapping besides "none".  If we add more, the code must
     * be adapted, e.g. to use a loop.
     */

    tum_LAST = tum_hash

} tbn_username_mapping;


/* ----------------------------------------------------------------------------
 * Database fields:
 *
 *   1. username: username of user.  Primary key.  Complete and plaintext
 *      by default, but see username_mapped and username_truncated for
 *      modifiers.
 *
 *   2. username_mapped: was this username mapped to another one, and if
 *      so, what method was used?  This is a value from tbn_username_mapping.
 *
 *   3. username_truncated: was the username truncated?  Boolean: 0/1.
 *      (Generally we will only truncate plaintext usernames, since mapping
 *      them will produce a fixed-length result.)  The database can handle
 *      arbitrarily long usernames, but we may truncate them to keep the
 *      database size in check.
 *
 *   4. known: was this user recognized as a known user account on the last
 *      login failure?  Boolean: 0/1.  (Currently just means it was a
 *      local user account, but later we might recognize some remote 
 *      usernames as "known" too.)
 *
 *   5. locked: is this account locked?  Boolean: 0/1.  Note that this does
 *      not reflect the influence of the lock_time or unlock_time settings,
 *      which are essentially overrides to the 'locked' flag, enforced at
 *      check time.
 *
 *   6. num_failures: number of consecutive authentication failures since
 *      the last success.  This (and the next three fields) do NOT consider
 *      failures caused by pam_tallybyname itself: the user must have been
 *      given a full chance to authenticate himself for it to be considered
 *      a failure.
 *
 *   7. last_failure_time: a time_t (lt_time_sec) value representing a
 *      timestamp for the last recorded authentication failure.
 *
 *   8. last_failure_host: a string containing whatever we got from
 *      PAM_RHOST during the last authentication failure.
 *
 *   9. last_failure_host_truncated: was the last_failure_host truncated?
 *      Boolean: 0/1.  As with username_truncated, we may truncate the
 *      last_failure_host strings to keep the db from getting too large.
 *
 *  10. last_failure_tty: a string containing whatever we got from
 *      PAM_TTY during the last authentication failure.
 *
 * XXX/EMT: because we do not have username_mapped and
 * username_truncated as primary keys, they could change for a given
 * username.  That is highly unlikely in the case of the mapping, but the
 * most common scenario here is that a username of exactly the max
 * length could get overwritten by one that's too long but got
 * truncated to the same thing, or vice-versa.  That's not a big
 * problem, since different too-long usernames that all truncate to
 * the same thing can overwrite each other anyway -- though
 * potentially confusing to an observant user.
 */

#define TBN_FIELD_USERNAME                     "username"
#define TBN_FIELD_USERNAME_MAPPED              "username_mapped"
#define TBN_FIELD_USERNAME_TRUNCATED           "username_truncated"
#define TBN_FIELD_KNOWN                        "known"
#define TBN_FIELD_LOCKED                       "locked"
#define TBN_FIELD_NUM_FAILURES                 "num_failures"
#define TBN_FIELD_LAST_FAILURE_TIME            "last_fail_time"
#define TBN_FIELD_LAST_FAILURE_HOST            "last_fail_host"
#define TBN_FIELD_LAST_FAILURE_HOST_TRUNCATED  "last_fail_host_truncated"
#define TBN_FIELD_LAST_FAILURE_TTY             "last_fail_tty"

/* ----------------------------------------------------------------------------
 * The order of the columns in the table.  When reading query results,
 * these are the indices each field will have -- ONLY IF we have not
 * restricted the set of fields to be returned.
 */
#define TBN_COLUMN_USERNAME                          0
#define TBN_COLUMN_USERNAME_MAPPED                   1
#define TBN_COLUMN_USERNAME_TRUNCATED                2
#define TBN_COLUMN_KNOWN                             3
#define TBN_COLUMN_LOCKED                            4
#define TBN_COLUMN_NUM_FAILURES                      5
#define TBN_COLUMN_LAST_FAILURE_TIME                 6
#define TBN_COLUMN_LAST_FAILURE_HOST                 7
#define TBN_COLUMN_LAST_FAILURE_HOST_TRUNCATED       8
#define TBN_COLUMN_LAST_FAILURE_TTY                  9

/* ----------------------------------------------------------------------------
 * These are arbitrary numbers that can be used in a prepared SQL
 * statement to refer to values for each of these fields.  Note that
 * they cannot share the values of the column indices because those
 * must start from 0, while these must start from 1.
 *
 * Note also that this assumes we'll only have a single one of each to
 * refer to from our SQL (except for the username, for which we have
 * made provisions) -- this is true so far, though not inherent.
 *
 * "BPARAM" stands for "Bound PARAMeter".
 */
#define TBN_BPARAM_USERNAME                          1
#define TBN_BPARAM_USERNAME_STR                    "?1"
#define TBN_BPARAM_USERNAME_MAPPED                   2
#define TBN_BPARAM_USERNAME_MAPPED_STR             "?2"
#define TBN_BPARAM_USERNAME_TRUNCATED                3
#define TBN_BPARAM_USERNAME_TRUNCATED_STR          "?3"
#define TBN_BPARAM_KNOWN                             4
#define TBN_BPARAM_KNOWN_STR                       "?4"
#define TBN_BPARAM_LOCKED                            5
#define TBN_BPARAM_LOCKED_STR                      "?5"
#define TBN_BPARAM_NUM_FAILURES                      6
#define TBN_BPARAM_NUM_FAILURES_STR                "?6"
#define TBN_BPARAM_LAST_FAILURE_TIME                 7
#define TBN_BPARAM_LAST_FAILURE_TIME_STR           "?7"
#define TBN_BPARAM_LAST_FAILURE_HOST                 8
#define TBN_BPARAM_LAST_FAILURE_HOST_STR           "?8"
#define TBN_BPARAM_LAST_FAILURE_HOST_TRUNCATED       9
#define TBN_BPARAM_LAST_FAILURE_HOST_TRUNCATED_STR "?9"
#define TBN_BPARAM_LAST_FAILURE_TTY                 10
#define TBN_BPARAM_LAST_FAILURE_TTY_STR           "?10"

/*
 * This is not one of the fields, but rather is to allow a reference
 * to an alternate form of the username to TBN_BPARAM_USERNAME.
 * The intention is that this is used for a mapped username.
 * Note that it differs from TBN_BPARAM_USERNAME_MAPPED, which refers
 * to the integer enum field which says whether or not the username in
 * the record is mapped.
 */
#define TBN_BPARAM_ALT_USERNAME                  11
#define TBN_BPARAM_ALT_USERNAME_STR            "?11"


/* ----------------------------------------------------------------------------
 * Name of the index we use to speed up certain queries.
 */
#define TBN_IDX_CLEANUP_1       "idx_cleanup_1"


/* ----------------------------------------------------------------------------
 * Auxiliary data table.
 *
 * This is where we toss various auxiliary values.  To avoid having to
 * change the schema if we think of new things to store here, we try
 * to use a generic structure.  Each row has a name key, as well as
 * int and string value fields. Hopefully each thing we want can be
 * stored as either an int or a value (or in rare cases, both).
 *
 * Note that we have no BPARAM constants here because substitution
 * (mainly a guard against escaping-related isses) is not necessary
 * with names that we have chosen ourselves.  For convenience,
 * we just have constants for our names and then just use them inline.
 *
 * The names we have so far are:
 *
 *   - failures_since_last_cleanup: uses value_int to hold the number of
 *     failures that have been registered in the users table since the 
 *     last time we did a "cleanup", which is deleting some older records
 *     to limit us to a certain number of records.
 */
#define TBN_AUX_TABLE_NAME          "aux_data"
#define TBN_AUX_FIELD_NAME          "name"
#define TBN_AUX_FIELD_VALUE_INT     "value_int"
#define TBN_AUX_FIELD_VALUE_STR     "value_str"
#define TBN_AUX_NAME_FAILURE_COUNT  "'failures_since_last_cleanup'"


/* ----------------------------------------------------------------------------
 * Maximum length we'll permit for a username or remote host before
 * truncating it.  XXX/EMT: this could be configurable.
 */
#define PAM_TBN_STRING_MAX_LEN 63


/* ----------------------------------------------------------------------------
 * Get current time in number of seconds since the Epoch.
 * Equivalent to libcommon's lt_curr_time().
 */
int tbn_curr_time(void);


/* ----------------------------------------------------------------------------
 * Open our database of user login records, and make sure the database
 * has our tables and indices defined.
 *
 * We begin a DEFERRED transaction for our own purposes, and may
 * upgrade to an EXCLUSIVE one if we need to make any updates.  When
 * we return, we will have a transaction open, and the caller may
 * specify (using 'min_mode') the minimum strength transaction they
 * expect.  Note that we will never downgrade our transaction (close
 * an EXCLUSIVE one and reopen a DEFERRED one); this opens new
 * concurrency issues (albeit unlikely ones), and staying EXCLUSIVE
 * has no significant other implications since we expect to have
 * upgraded to EXCLUSIVE here very seldom indeed.
 *
 * Note that if we're going to end up timing out due to a long-running
 * operation in another process/thread, it's almost certain to happen
 * here (during our first query, which attempts to take a SHARED lock,
 * or during our attempt to upgrade to an EXCLUSIVE transaction).
 * This would cause us to return SQLITE_BUSY or SQLITE_LOCKED.  It
 * should not happen later because (a) no write operations can begin
 * while we hold a SHARED lock, and (b) we never do any long-running
 * read operations without holding an EXCLUSIVE lock, see item #6
 * under "Implementation notes: Locking" in pam_tallybyname_common.c.
 *
 * 'create' tells us what we should do if the data file doesn't exist.
 * 1 means to create it, with umask 077.  0 means to fail with an error.
 * Note that we're OK starting with a zero-length file.  And in either
 * case, if we encounter a problem opening the file or performing our
 * first query, we'll truncate the file to length 0 and try again.
 *
 * The path is optional; if NULL is provided, we use the default,
 * TBN_DEFAULT_LOGFILE.
 */
int tbn_open_db_ensure_tables(const char *path, int create, 
                              sqlite_utils_transaction_mode min_mode,
                              sqlite3 **ret_db);


/* ----------------------------------------------------------------------------
 * Check a username to see if it is "known".  (Currently, this just means
 * whether it is a local user account.)  Returns 1 for yes, and 0 for no.
 */
int tbn_username_is_known(const char *username, int *ret_known);


/* ----------------------------------------------------------------------------
 * Munge a string for use with our database.  This is used for both
 * usernames and remote hostnames, but some of the options are only
 * applicable to usernames.
 *
 * \param string String to be munged.
 *
 * \retval ret_truncated_string The string, truncated if necessary, but
 * still always in plaintext.
 *
 * \retval ret_was_truncated 0 if ret_truncated_string matches the input
 * string; 1 if it was truncated to make it fit.
 *
 * \param mapping Mapping operation to perform on the string, if any.
 * Generally only intended for usernames, though could be used for anything.
 * Note that the mapping is applied to the original (pre-truncation) string.
 *
 * \retval ret_mapped_string If 'mapping' was not tum_none, this returns the
 * result of mapping the string.  If 'mapping' was tum_none, then NULL is
 * returned here.
 */
int tbn_string_fixup(const char *string, char **ret_truncated_string,
                     int *ret_was_truncated, tbn_username_mapping mapping,
                     char **ret_mapped_string);


/* ----------------------------------------------------------------------------
 * Trim the database: delete as many unknown records as necessary
 * (maybe zero) to reduce it to no more than max_recs records.  Rows
 * with the oldest last-failure timestamp are deleted first.  Note
 * that we never delete known records.
 *
 * This also resets the counter for how many failures there have been
 * since our last trim.
 *
 * NOTE: this assumes the database given is already in an EXCLUSIVE
 * transaction.
 */
int tbn_trim_db(sqlite3 *db, int max_recs);


/* ----------------------------------------------------------------------------
 * Record a login failure for the specified user.  Note that the
 * timestamp is not taken as a parameter because we just use the
 * current system clock.  We have to take "known", rather than
 * figuring it out for the caller, because the pam_tallybyname
 * binary is one of our callers, and it does not want to assume it
 * is running on the same system as the file pertains to.
 *
 * If there is any truncation or mapping to be done, we take care of
 * it, so pass us a full, unadulterated username.
 *
 * We do not tamper with the lock on the account, though...
 * see tbn_maybe_lock_user().
 *
 * NOTE: this assumes the database given is already in an EXCLUSIVE
 * transaction.
 *
 * \param db Database on which to operate.
 *
 * \param username Username of user for whom to record failure (required).
 *
 * \param mapping Mapping to use for username when recording the failure.
 * Note that when looking for existing records, we will search the username
 * under ALL possible mappings.  But this is the one under which we will
 * record the failure, moving the old record if necessary.
 *
 * \param known Is this user to recorded as one known as a local account?
 *
 * \param host Hostname of remote host from which user tried to login
 * (optional).
 *
 * \param tty TTY from which user tried to login (optional).
 *
 * \param max_recs Maximum number of records to enforce, or 0 for no
 * enforcement.
 *
 * \param max_recs_check_freq How many calls of this function between
 * checks?  1 means to check on every call.
 */
int tbn_record_failure(sqlite3 *db, const char *username,
                       tbn_username_mapping mapping, int known,
                       const char *host, const char *tty,
                       int max_recs, int max_recs_check_freq);


/* ----------------------------------------------------------------------------
 * If this user already has a record, and has max_fail (or more)
 * consecutive authentication failures on record, lock the account.
 * Note that if max_fail is <=0, the account will always be locked if
 * it had a record before.  If the account did not have a record,
 * nothing will be changed.
 *
 * Again, we take care of any truncation or mapping.
 *
 * 'ret_user_existed' gets 1 if the user had a record, and 0 if they
 * did not.  Note that in the 0 case, this means nothing has been
 * changed in the db.
 *
 * 'ret_locked_user' gets 1 if the user was locked just now; or 0 if
 * it was not changed by this call.  0 could mean (a) the user didn't
 * have enough failures yet, (b) the user didn't have a record at all,
 * or (c) the user account was already locked.
 *
 * NOTE: this assumes the database given is already in an EXCLUSIVE
 * transaction.
 */
int tbn_maybe_lock_user(sqlite3 *db, const char *username,
                        tbn_username_mapping mapping, int max_fail,
                        int *ret_user_existed, int *ret_locked_user);


#ifdef __cplusplus
}
#endif

#endif /* __PAM_TALLYBYNAME_COMMON_H_ */
