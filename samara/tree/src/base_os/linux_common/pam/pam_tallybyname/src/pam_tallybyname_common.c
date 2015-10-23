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

/* ------------------------------------------------------------------------- */
/* NOTE: this file should not include any TMS headers (outside its own
 * directory), as it needs to be able to build within a PAM module,
 * and does not link with libcommon, etc.
 *
 * For booleans, we use ints instead, with 0 for false and 1 for true.
 *
 * XXX/EMT: we call syslog() instead of pam_syslog() because we are also
 * called from the pam_tallybyname binary, which does not include the PAM
 * headers or link with libpam.  This means that our messages are not
 * formatted the same as the module's (nor the same as the binary's, which
 * uses lc_log_basic()).  However, the majority of our messages are only
 * in error cases, or for keeping the tally log trimmed (which is not
 * part of the module's main control flow anyway).
 */

/* ------------------------------------------------------------------------- */
/* Implementation notes: Locking
 * -----------------------------
 *
 * We use transactions to ensure correctness in the face of concurrent
 * access.  Of course, it would be simplest to put everything inside
 * exclusive transactions, but there are two problems with this...
 *
 * First, performance penalty from reduced concurrency.  We're not all
 * that concerned with this, except to the extent that slow performance
 * could cause denial of service attacks -- or security holes, where
 * we fail to enforce or track failures, in the case of timeouts
 * caused by slow performance.
 *
 * Secondly, we want to avoid taking the write lock when we don't need
 * it, to limit the damage caused by possible bugs e.g. where we end
 * up stuck in a system call.  We're not worried about crashing, since
 * that breaks any lock we were holding (try 'kill -9' on a sqlite3
 * process that has run 'begin exclusive transaction;').  But if we
 * hang, e.g. in a syscall, while holding an exclusive write lock,
 * this would prevent anyone else from accessing the database, and
 * logins would be either delayed or untracked.
 *
 * These are the operations we do, and what kind of locks we need for
 * each.  With each we show the function name that implements this
 * operation, and whether it happens in the PAM module only, the
 * binary only, or both.
 *
 *   1. Opening the database, and verifying tables and indices.
 *      tbn_open_db_ensure_tables(): module, binary
 *
 *      (a) SELECT FROM sqlite_master                         (read)
 *      (b) CREATE TABLE                                      (write)
 *      (c) CREATE INDEX                                      (write)
 *
 *      Start with DEFERRED transaction, and leave it open for our
 *      caller to continue with.  Note that we don't need anything
 *      better for ourselves because the "IF NOT EXISTS" option to
 *      "CREATE" lets us atomically check and create the tables
 *      and indices.
 *
 *   2. Checking if the user account is locked:
 *      pam_tbn_check_ok(): module
 *
 *      (a) SELECT locked FROM users WHERE username [plain]   (read)
 *      (b) SELECT locked FROM users WHERE username [mapped]  (read)
 *
 *      Continue with the DEFERRED transaction inherited from #1.
 *      We won't need to write anything, so this just keeps a SHARED
 *      lock all the way through.
 *
 *   3. Recording a login success:
 *      pam_tbn_reset_user(): module
 *
 *      (a) SELECT FROM users WHERE username [plain]           (read)
 *      (b) DELETE FROM users WHERE username [plain]           (write)
 *      (c) SELECT FROM users WHERE username [mapped]          (read)
 *      (d) DELETE FROM users WHERE username [mapped]          (write)
 *
 *      Continue with the DEFERRED transaction inherited from #1.
 *      This is two indepdendent operations, (a) then maybe (b);
 *      and (c) then maybe (d).  For each, we start with the query,
 *      and if the username does not exist, we are satisfied that
 *      success is recorded, and finish.  If the username does exist,
 *      proceed to the next step, which will take a write lock when
 *      needed.  Of course the user could end up being deleted (and
 *      even then recreated) after our check, but the fact is that we
 *      still set out to delete this user's record, so there is no
 *      real surprise in the outcome.  No one could have strongly
 *      expected some different ordering.
 *
 *   4. Resetting an account (like recording login success, only with
 *      more options):
 *      tbn_bin_reset(): binary
 *
 *      (a) DELETE FROM users / UPDATE users                   (write)
 *
 *      Continue with the DEFERRED transaction inherited from #1.
 *
 *   5. Recording a login failure:
 *      tbn_record_failure(), tbn_maybe_trim_db(): module, binary
 *
 *      (a) SELECT num_failures FROM users [plain]             (read)
 *      (b) SELECT num_failures FROM users [mapped]            (read)
 *      (c1) UPDATE OR REPLACE                                 (write)
 *      (c2) INSERT INTO users                                 (write)
 *      (d) INSERT OR REPLACE INTO aux_data                    (write)
 *      (e) SELECT FROM aux_data                               (read)
 *          ...and possibly branch to (5) based on result.
 *
 *      Upgrade to an EXCLUSIVE transaction.  We'll unavoidably have
 *      to take the write lock anyway, and there's not much advantage
 *      in trying to put that off slightly longer.  We also need this
 *      to make sure that nothing changes while we're in the middle
 *      of our multiple statements, needed due to hashing.
 *
 *      Steps (a) and (b) are to find the existing record, if any,
 *      as well as the old number of failures.  We then choose step
 *      (c1) if there was an existing record, or (c2) if there was not.
 *      We then proceed to steps (d) and (e) to update the number of
 *      failures, for determining if we need to do database cleanup.
 *
 *   6. Cleaning up the database:
 *      tbn_trim_db(): module, binary
 *
 *      (a) SELECT last_fail_time FROM users                   (read)
 *      (b) DELETE FROM users WHERE last_fail_time             (write)
 *      (c) UPDATE aux_data                                    (write)
 *
 *      Continue with the EXCLUSIVE transaction inherited from #5.
 *      In theory we could have restarted with a DEFERRED transaction
 *      since that's all that is required for (a).  However, this is 
 *      our potentially longest-running query, and we wanted to have
 *      it be exclusive to simplify some of our failure logic, where
 *      we can assume that we won't get timeouts except at the
 *      beginning (where you can only time out on long writes).
 *      Also, particularly with TBN_IDX_CLEANUP_1, we expect the query
 *      to go much faster than the delete operation.
 *
 *   7. Locking the user if they require locking:
 *      tbn_maybe_lock_user(): module, binary
 *
 *      (a) SELECT locked FROM users WHERE username            (read)
 *      (b) UPDATE users                                       (write)
 *
 *      Continue with the EXCLUSIVE transaction inherited from #5.
 *      Note that we do the pre-query partly so we can report if we
 *      were the ones to have just locked the user, and so we can
 *      create the user record if it didn't already exist.  Given
 *      this pre-query, the logic is simpler and more obviously correct
 *      if done within an EXCLUSIVE transaction.
 *
 *      Note also that we don't do the double query here that we do
 *      while checking or recording failure.  We expect to be told
 *      explicitly where to find the user.  This is how the binary
 *      works anyway (you can tell it whether or not to hash the
 *      username); and it works fine for the module too, since it
 *      has just recorded a failure and fixed the record there
 *      (as an earlier part of this same tranaction).
 *
 *   8. Listing all user records:
 *      tbn_bin_list(): binary
 *
 *      (a) SELECT FROM users                                  (read)
 *
 *      Continue with the DEFERRED transaction inherited from #1.
 *      As with #2, we won't need to write anything, so this just
 *      keeps a SHARED lock all the way through.
 *
 *
 * So the bottom line is that we only need the write lock:
 *   - 5, 6, 7: when recording a login failure.
 *   - 3b, 3d, 4: when recording a login success, if it is the first one
 *     after a failure (or where it is explicitly ordered).
 *   - 1b, 1c: the first time we run, to set up the database.
 *
 * This should allow for a reasonable amount of concurrency, and do as
 * much to avert write-lock disasters as we can reasonably do.
 *
 *
 * Implementation notes: Hashing
 * -----------------------------
 *
 * One decision involved here was whether to clear out old unknown
 * records when the hashing policy was changed.  This would have
 * simplified implementation, but it was decided that we didn't want
 * to lose old authentication failure data when the configuration was
 * changed.  Consequently, we cannot assume that all the records in
 * the database were created under the same policy that is currently
 * configured.  So every time we want to look up a username, or
 * operate on an account, we have to look in both places.  This is the
 * case even for local user accounts, since an account that is currently
 * local might have been unknown the last time we looked it up.
 *
 * This adds some complexity, but it not expected to be a performance
 * problem.  The cost to hash a single username was measured on two
 * systems using the perf.h API:
 *   tb1   (2.33 GHz Xeon E5410)  average time 0.000027 sec
 *   mako  (1.00 GHz PPC 460EX)   average time 0.000157 sec
 * The cost of the lookups is also low, as it is done by username,
 * which is automatically indexed by SQLite.
 *
 * Another decision was whether to keep the username as the sole key,
 * or if we should make username_mapped part of the key.  This would
 * be required to accomodate as separate records the unlikely case of
 * two users, one of which hashes to 'X', and the other of which is
 * 'X' in plaintext.  However, this is so incredibly unlikely (unless
 * an admin actually sought to do this by creating an account after
 * seeing the hash of another account), it was decided not to be a 
 * concern.  The worst that would happen (again, probably due to a
 * purposeful move on the part of an admin) is that failures for the
 * two users would be tracked together in a single record (which would
 * oscillate between being named 'X' and the hash of 'X').
 *
 * This also means we don't have to worry about the user telling us,
 * when they specify a username (to look up, or to reset), whether
 * the username is intended to be a plaintext one or a hashed one.
 * We just look in both places automatically.
 */


#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <pwd.h>
#include <errno.h>
#include <openssl/sha.h>
#include "sqlite_utils.h"
#include "pam_tallybyname_common.h"
#include "customer.h"


#ifndef NULL
#define NULL ((void *)0)
#endif

static const int tbn_bits_per_b32_char = 5;
static const int tbn_bits_per_byte = 8;
static const uint8_t tbn_bit_masks[] = {0x00, 0x01, 0x03, 0x07,
                                        0x0f, 0x1f, 0x3f, 0x7f, 0xff};

/*
 * We have two conflicting goals:
 *   - Truncate so don't end up with such an unwieldy long string.
 *   - Don't truncate so much that we end up with a string short enough
 *     to be confused with a local username.
 *
 * We compromise on 192 bits.  Used with Base32 encoding, this yields
 * a 39-character username, which is longer than Samara's maximum
 * local username of 31 characters.
 */
static const int tbn_hash_trunc_bits = 192;

/* Chosen to be tbn_hmac_digest_sha256_length chars exactly */
static const uint8_t tbn_hash_key[] = "pam_tallybyname_hash_key_abcdefg";

/*
 * Copied from b32_num_to_char in encoding.c.  We could have used
 * base64 to save 4 characters (128 bits maps to 22 or 26 characters,
 * for base64 or base32 respectively) but we prefer base32 because it
 * doesn't require any punctuation to be put into the result.
 */
static const char tbn_b32_num_to_char[] =
    "0123456789ABCDEFGHJKLMNPQRTUVWXY";

#ifndef min
#define min(x,y) ( (x)<(y)?(x):(y) )
#endif


/* ------------------------------------------------------------------------- */
int
tbn_curr_time(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (tv.tv_sec);
}


/* ------------------------------------------------------------------------- */
int
tbn_open_db_ensure_tables(const char *path, int create,
                          sqlite_utils_transaction_mode min_mode,
                          sqlite3 **ret_db)
{
    int err = 0;
    struct stat stats;
    sqlite3 *db = NULL;
    const char *sql = NULL;
    sqlite3_stmt *stmt = NULL;
    int exists = 0;
    int tried_recover = 0;
    sqlite_utils_transaction_mode xaction_mode = sutm_none;

    if (ret_db == NULL) {
        err = TBN_ERR_GENERIC;
        goto bail;
    }

    if (path == NULL) {
        path = TBN_DEFAULT_LOGFILE;
    }

    /* ........................................................................
     * Prepare to open the database.
     */
    if (create) {
        /*
         * sqlite3_open() will automatically create the database file
         * if it doesn't exist.
         */
        umask(077);
    }
    else {
        /*
         * We do NOT want to create the database file if it doesn't
         * already exist.  This is for consistency with other similar
         * components (though not pam_tally2!), and to leave it to
         * someone else to determine the right owner/group/mode.
         *
         * Sqlite3_open() would do that for us, and the fancier version of
         * sqlite3_open() which takes flags to tell it not to is not
         * available in the EL5 version.  So we have to pre-check.  This
         * sounds a little like a TOCTOU problem, but it's not really,
         * because the worst that would happen is:
         *   - The file doesn't exist, and then shows up after we check:
         *     we skip doing anything this time, but will do so next time.
         *   - The file does exist, and then disappears after we check:
         *     sqlite3_open() (re-)creates it for us.
         */
        errno = 0;
        err = lstat(path, &stats);
        if (err && errno == ENOENT) {
            syslog(LOG_ERR, "ERROR: tally database at '%s' did not "
                   "exist, and we will not create it.  Aborting.", path);
            err = TBN_ERR_GENERIC;
            goto bail;
        }
        if (err || errno) {
            syslog(LOG_ERR, "ERROR: unable to check existence of database "
                   "at '%s'.  Aborting.", path);
            err = TBN_ERR_GENERIC;
            goto bail;
        }
        /*
         * If we get here, the db exists, so we can proceed.
         */
    }

    /* ........................................................................
     * Open the database.
     *
     * If the database file is present but empty, sqlite3_open() deals
     * gracefully with it.  It treats it just as an empty database
     * with no tables, and will allow us to create some.
     *
     * We never expect to get SQLITE_BUSY or SQLITE_LOCKED here, but
     * if we do, they would be fatal errors rather than one we'd try
     * to recover from by blowing away the db.  And SQLITE_NOMEM is
     * possible and also unrecoverable.
     */
    err = sqlite3_open(path, &db);
    if (err == SQLITE_BUSY ||
        err == SQLITE_LOCKED ||
        err == SQLITE_NOMEM) {
        bail_error_sqlite(err, db, sql);
    }
    if (err) {

        syslog(LOG_WARNING, "WARNING: could not open tally database at '%s', "
               "attempting recovery...", path);

    /* ........................................................................
     * Some basic attempt at error recovery here, if needed...
     */
    try_recover:
        errno = 0;
        err = truncate(path, 0);
        if (err || errno) {
            syslog(LOG_ERR, "ERROR: tally database at '%s' was corrupted, "
                   "and was unable to be truncated!  Aborting.", path);
            err = TBN_ERR_GENERIC;
            goto bail;
        }
        err = sqlite3_open(path, &db);
        if (err) {
            syslog(LOG_ERR, "ERROR: tally database at '%s' was corrupted, "
                   "and we truncated it, but were still unable to open it!  "
                   "Aborting.", path);
            err = TBN_ERR_GENERIC;
            goto bail;
        }
        syslog(LOG_WARNING, "WARNING: tally database at '%s' was corrupted.  "
               "It has been truncated and is now starting at empty.", path);
        tried_recover = 1;
    }

    if (db == NULL) {
        err = TBN_ERR_GENERIC;
        goto bail;
    }

    /* ........................................................................
     * Set a busy timeout.  This is the maximum time a SQLite call
     * will block (well, actually it busy-waits) on a locked database
     * before giving up.
     *
     * See comments above aaa_tbn_db_busy_timeout_ms in customer.h for
     * more on what goes into choosing this value.
     *
     * If the timer expires, something like SQLITE_BUSY or
     * SQLITE_LOCKED will be returned from the call, and we'll bail
     * out and permit ourselves to be skipped.  i.e. a locked user
     * will be permitted to attempt login, and neither failed nor
     * successful logins will be recorded.
     *
     * We could have installed our own busy handler here.  We might
     * have used it to (a) log something about the status of our
     * waiting, and/or (b) place an upper limit on the total amount of
     * time a login can take.  The timeout itself does not place such
     * a limit because each login entails multiple database
     * transactions.  If the database is completely busy the whole
     * time, the delay is bounded at 2x the timeout: one for 'check',
     * and then one for 'success' or 'failure'.  If the database frees
     * up just before the timeout expires, and then becomes busy again
     * later, and this happens repeatedly, the user could be made to
     * wait for more than 2x as long overall -- but this is very
     * unlikely, since the only long-running operation is one which
     * we do not perform often.
     */
    err = sqlite3_busy_timeout(db, aaa_tbn_db_busy_timeout_ms);
    bail_error_sqlite(err, db, sql);

    /* ........................................................................
     * Begin a DEFERRED transaction, so we'll take and hold a SHARED
     * lock as soon as we make our first query.
     *
     * Any errors we get here are unexpected and fatal.  If someone
     * else has a lock, even an EXCLUSIVE one, that should not cause
     * an error, since we are deferring taking even a SHARED lock
     * until our first query.  And if the database is corrupt, that
     * won't cause problems until later either.
     */
    err = sqlite_utils_transaction_begin(db, sutm_deferred);
    bail_error_sqlite(err, db, sql);
    xaction_mode = sutm_deferred;

    /* ........................................................................
     * Do a dummy query here.  This forces the taking of a SHARED
     * lock, and also should allow us to detect most problems that 
     * are going to occur.  Here we can deal with:
     *   - SQLITE_BUSY/SQLITE_LOCKED: fatal, but exit silently.
     *     Our caller will deal with this.
     *   - SQLITE_NOMEM: fatal, exit noisily.
     *   - Anything else: the first time, go back to 'try_recover'.
     *     If we're already tried that, fatal, and exit noisily.
     *
     * After the first check, any error is fatal, with a noisy exit.
     * We don't expect to get SQLITE_BUSY anymore, since our first
     * query took a SHARED lock and did not release it.  And we
     * don't expect any other errors to be recoverable if the first
     * query succeeded.
     *
     * XXX/EMT: would be nice to verify schema.  But if we change that
     * we should be fixing it up in /var upgrade.  The more common
     * expected failure is file corruption, which we already tried to
     * deal with above.
     */
    err = sqlite_utils_table_exists(db, TBN_TABLE_NAME, &exists);
    if (err == SQLITE_BUSY || err == SQLITE_LOCKED) {
        goto bail;
    }
    else if (err == SQLITE_NOMEM) {
        bail_error_sqlite(err, db, sql);
    }
    else if (err) {
        if (tried_recover) {
            bail_error_sqlite(err, db, sql);
        }
        else {
            /*
             * In some cases the database will appear to open
             * successfully, but it will balk at the first query we
             * try to do on it.  We'll humor it, and try a recovery
             * from here.  But after we successfully confirm that our
             * first table exists, all bets are off for future errors.
             */
            sqlite_utils_close(&db, 0);
            db = NULL;
            syslog(LOG_WARNING, "WARNING: could not read from tally database "
                   "at '%s', attempting recovery...", path);
            goto try_recover;
        }
    }

    /*
     * We don't actually care about 'exists', this was just a dummy
     * query.  We're going to use "IF NOT EXISTS" to atomically make
     * sure the table is there.
     */

    /* ........................................................................
     * Make sure we have the main user table.
     */
    sql = "CREATE TABLE IF NOT EXISTS " TBN_TABLE_NAME
        " (" TBN_FIELD_USERNAME " TEXT PRIMARY KEY "
        ", " TBN_FIELD_USERNAME_MAPPED " INTEGER "
        ", " TBN_FIELD_USERNAME_TRUNCATED " INTEGER "
        ", " TBN_FIELD_KNOWN " INTEGER "
        ", " TBN_FIELD_LOCKED " INTEGER "
        ", " TBN_FIELD_NUM_FAILURES " INTEGER "
        ", " TBN_FIELD_LAST_FAILURE_TIME " INTEGER "
        ", " TBN_FIELD_LAST_FAILURE_HOST " TEXT "
        ", " TBN_FIELD_LAST_FAILURE_HOST_TRUNCATED " INTEGER "
        ", " TBN_FIELD_LAST_FAILURE_TTY " TEXT "
        " )";
    
    sqlite_utils_finalize(&stmt);
    err = sqlite3_prepare(db, sql, -1, &stmt, NULL);
    bail_error_sqlite(err, db, sql);
    
    err = sqlite_utils_step(stmt);
    bail_error_sqlite(err, db, sql);

    /* ........................................................................
     * Make sure we have the auxiliary data table
     */
    sql = "CREATE TABLE IF NOT EXISTS " TBN_AUX_TABLE_NAME
        " (" TBN_AUX_FIELD_NAME " TEXT PRIMARY KEY "
        ", " TBN_AUX_FIELD_VALUE_INT " INTEGER "
        ", " TBN_AUX_FIELD_VALUE_STR " TEXT "
        " )";
    
    sqlite_utils_finalize(&stmt);
    err = sqlite3_prepare(db, sql, -1, &stmt, NULL);
    bail_error_sqlite(err, db, sql);
    
    err = sqlite_utils_step(stmt);
    bail_error_sqlite(err, db, sql);

    /* ........................................................................
     * Make sure we have our index, to speed up certain queries.
     *
     * TBN_IDX_CLEANUP_1 is meant to speed up the SELECT at the
     * beginning of tbn_trim_db().
     */
    sql = "CREATE INDEX IF NOT EXISTS " TBN_IDX_CLEANUP_1
        " ON " TBN_TABLE_NAME
        " ( " TBN_FIELD_KNOWN
        " , " TBN_FIELD_LAST_FAILURE_TIME
        " ) ";

    sqlite_utils_finalize(&stmt);
    err = sqlite3_prepare(db, sql, -1, &stmt, NULL);
    bail_error_sqlite(err, db, sql);
    
    err = sqlite_utils_step(stmt);
    bail_error_sqlite(err, db, sql);

 done:
    /* ........................................................................
     * If the caller requested a higher transaction mode than we ended
     * up in, move to it.  Note that this again may encounter a busy
     * database, which we deal with in the same manner as above.
     */
    if (xaction_mode < min_mode) {
        if (xaction_mode != sutm_none) {
            err = sqlite_utils_transaction_end(db);
            bail_error_sqlite(err, db, "END TRANSACTION");
            xaction_mode = sutm_none;
        }

        err = sqlite_utils_transaction_begin(db, min_mode);
        if (err == SQLITE_BUSY || err == SQLITE_LOCKED) {
            goto bail;
        }
        bail_error_sqlite(err, db, "BEGIN EXCLUSIVE TRANSACTION");
        xaction_mode = sutm_exclusive;
    }

 bail:
    /*
     * Intentionally do NOT end our transaction.  The transaction will
     * be ended when our caller closes the database with
     * sqlite_utils_close().
     *
     * However, do verify that we at least have some transaction going,
     * as long as we are claiming to return with success.
     */
    if (err == 0 && xaction_mode == sutm_none) {
        syslog(LOG_ERR, "ERROR: returning from opening database with "
               "no transaction open, but no error either!");
    }

    sqlite_utils_finalize(&stmt);
    if (ret_db) {
        *ret_db = db;
    }
    return(err);
}


/* ------------------------------------------------------------------------- */
int
tbn_username_is_known(const char *username, int *ret_known)
{
    int err = 0;
    int known = 0;
    struct passwd local_user_buf;
    struct passwd *local_user = NULL;
    long bufsize = 0;
    char *buf = NULL;

    memset(&local_user_buf, 0, sizeof(local_user_buf));

    if (username == NULL) {
        err = TBN_ERR_GENERIC;
        goto bail;
    }

    /*
     * Use threadsafe alternative to getpwnam() in case we're in a
     * multithreaded environment.  Too bad we can't put 'buf' on the
     * stack, since we have to choose its size based on a function
     * call.
     */
    bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (bufsize < 0) {
        err = TBN_ERR_GENERIC;
        goto bail;
    }
    buf = malloc(bufsize);
    err = getpwnam_r(username, &local_user_buf, buf, bufsize, &local_user);
    /*
     * The getpwnam_r man page is vague about what error codes might
     * be used to say the user just didn't exist.  So don't try to 
     * distinguish this from a real error.
     */
    known = (err == 0 && local_user != NULL);

 bail:
    if (buf) {
        free(buf);
    }
    if (ret_known) {
        *ret_known = known;
    }
    return(err);
}


/* ------------------------------------------------------------------------- */
int
tbn_string_fixup(const char *string, char **ret_truncated_string,
                 int *ret_was_truncated, tbn_username_mapping mapping,
                 char **ret_mapped_string)
{
    int err = 0;
    int string_len = 0;
    char *truncated_string = NULL, *mapped_string = NULL;
    int was_truncated = 0;
    uint8_t *digest = NULL;
    uint32_t digest_len = 0;
    int hash_trunc_len = 0;

    if (string == NULL) {
        err = TBN_ERR_GENERIC;
        goto bail;
    }

    /* ........................................................................
     * First, do the truncation.
     */
    string_len = strlen(string);
    was_truncated = 0;
    if (ret_truncated_string) {
        if (string_len <= PAM_TBN_STRING_MAX_LEN) {
            truncated_string = strdup(string);
        }
        else {
            truncated_string = malloc(PAM_TBN_STRING_MAX_LEN + 1);
            if (truncated_string) {
                strncpy(truncated_string, string, PAM_TBN_STRING_MAX_LEN);
                truncated_string[PAM_TBN_STRING_MAX_LEN] = '\0';
                was_truncated = 1;
            }
        }
        if (truncated_string == NULL) {
            err = TBN_ERR_GENERIC;
            goto bail;
        }
    }

    /* ........................................................................
     * Then do the mapping.
     */
    if (ret_mapped_string && mapping != tum_none) {
        switch (mapping) {

        case tum_hash:
            err = lpc_hmac_sha256(tbn_hash_key,
                                  strlen((const char *)tbn_hash_key),
                                  (const uint8_t *)string, strlen(string),
                                  &digest, &digest_len);
            if (err) {
                if (aaa_log_unknown_usernames_flag) {
                    syslog(LOG_WARNING, "WARNING: failed to map unknown "
                           "username '%s'", string);
                }
                else {
                    syslog(LOG_DEBUG, "Failed to map unknown "
                           "username '%s'", string);
                    syslog(LOG_WARNING, "WARNING: failed to map unknown "
                           "username");
                }
                goto bail;
            }

            hash_trunc_len = min((int)digest_len, 
                                 tbn_hash_trunc_bits / tbn_bits_per_byte);

            err = lpc_encode_b32(digest, hash_trunc_len, &mapped_string);
            /* Don't bail, in error case we're still getting part-hashed */
            if (err || mapped_string == NULL) {
                if (aaa_log_unknown_usernames_flag) {
                    syslog(LOG_WARNING, "WARNING: failed to render hashed "
                           "unknown username '%s' as base32", string);
                }
                else {
                    syslog(LOG_DEBUG, "Failed to render hashed "
                           "unknown username '%s' as base32", string);
                    syslog(LOG_WARNING, "WARNING: failed to render hashed "
                           "unknown username as base32");
                }
                err = TBN_ERR_GENERIC;
            }
            break;

        default:
            err = TBN_ERR_GENERIC;
            goto bail;
            break;
        }
    }

 bail:
    if (digest) {
        free(digest);
    }
    if (ret_truncated_string) {
        *ret_truncated_string = truncated_string;
    }
    else if (truncated_string) {
        free(truncated_string);
    }
    if (ret_was_truncated) {
        *ret_was_truncated = was_truncated;
    }
    if (ret_mapped_string) {
        *ret_mapped_string = mapped_string;
    }
    else if (mapped_string) {
        free(mapped_string);
    }
    return(err);
}


/* ------------------------------------------------------------------------- */
int
tbn_trim_db(sqlite3 *db, int max_recs)
{
    int err = 0;
    const char *sql = NULL;
    int time_cutoff = 0;
    sqlite3_stmt *stmt = NULL;

    /*.........................................................................
     * 1. Figure out the timestamp on the nth oldest record.
     *    Then we'll use that as our cutoff for deleting records.
     *
     *    We only want to delete unknown records.  That will require
     *    an extra deletion criterion below, but as for here, it affects
     *    how we order stuff.  The offset is our max # of records, so 
     *    everything before the offset is stuff we don't want to delete.
     *    Put the known stuff in there.  At the bottom will be the 
     *    unknown stuff with the lower last failure times, which is 
     *    what we want to trim.
     * 
     *    Note: TBN_IDX_CLEANUP_1 was chosen specifically to help
     *    speed up this query.
     */
    sql = "SELECT " TBN_FIELD_LAST_FAILURE_TIME " FROM " TBN_TABLE_NAME
        " ORDER BY "
        TBN_FIELD_KNOWN " DESC, "
        TBN_FIELD_LAST_FAILURE_TIME " DESC"
        " LIMIT 1 OFFSET ?1";
    
    sqlite_utils_finalize(&stmt);
    err = sqlite3_prepare(db, sql, -1, &stmt, NULL);
    bail_error_sqlite(err, db, sql);
    
    err = sqlite3_bind_int(stmt, 1, max_recs);
    bail_error_sqlite(err, db, sql);
    
    err = sqlite_utils_step(stmt);
    if (err == SQLITE_DONE) {
        /*
         * Didn't get any rows, so there were presumably no extra rows
         * in the database, so we're done!
         */
        err = 0;
        syslog(LOG_DEBUG, "Tally log trim: no trimmable rows found");
        time_cutoff = 0;
    }
    else if (err == SQLITE_ROW) {
        err = 0;
        time_cutoff = sqlite3_column_int(stmt, 0);

        if (time_cutoff <= 0) {
            syslog(LOG_WARNING, "WARNING: Tally log trim: got unexpected "
                   "time_cutoff %d, not trimming or resetting db",
                   time_cutoff);
            goto bail;
        }

        err = sqlite_utils_step(stmt); /* Finish, to avoid SQLITE_LOCKED */
        /* We now expect SQLITE_DONE */
    }
    bail_error_sqlite(err, db, sql);

    /*.........................................................................
     * 2. Trim all unknown records which are older than the cutoff
     *    we found.
     */
    if (time_cutoff > 0) {
        sql = "DELETE FROM " TBN_TABLE_NAME
            " WHERE " TBN_FIELD_LAST_FAILURE_TIME " <= "
            TBN_BPARAM_LAST_FAILURE_TIME_STR
            " AND " TBN_FIELD_KNOWN " = 0";
        
        sqlite_utils_finalize(&stmt);
        err = sqlite3_prepare(db, sql, -1, &stmt, NULL);
        bail_error_sqlite(err, db, sql);
        
        err = sqlite3_bind_int(stmt, TBN_BPARAM_LAST_FAILURE_TIME,
                               time_cutoff);
        bail_error_sqlite(err, db, sql);
        
        err = sqlite_utils_step(stmt);
        bail_error_sqlite(err, db, sql);

        syslog(LOG_INFO, "Tally log trim: trimmed all unknown records with "
               "last fail time <= %d", time_cutoff);
    }

    /*.........................................................................
     * 3. Reset the counter of how many failures there have been since
     *    the last trim.
     */
    sql = "UPDATE " TBN_AUX_TABLE_NAME
        " SET " TBN_AUX_FIELD_VALUE_INT " = 0"
        " WHERE " TBN_AUX_FIELD_NAME " = " TBN_AUX_NAME_FAILURE_COUNT;
    
    sqlite_utils_finalize(&stmt);
    err = sqlite3_prepare(db, sql, -1, &stmt, NULL);
    bail_error_sqlite(err, db, sql);

    err = sqlite_utils_step(stmt);
    bail_error_sqlite(err, db, sql);

    syslog(LOG_DEBUG, "Tally log trim: failure counter reset to zero");
    
 bail:
    sqlite_utils_finalize(&stmt);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
tbn_maybe_trim_db(sqlite3 *db, int max_recs, int max_recs_check_freq)
{
    int err = 0;
    const char *sql = NULL;
    sqlite3_stmt *stmt = NULL;
    long num_failures = 0;
    int do_trim = 0;

    syslog(LOG_DEBUG, "Tally log trim: maybe trim db, max_recs=%d, "
           "max_recs_check_freq=%d", max_recs, max_recs_check_freq);

    /* ........................................................................
     * We have three cases:
     *   (a) Check_freq == 1: don't bother incrementing counter, just go
     *       straight to the trim (which will reset the counter).
     *   (b) Max_recs == 0: increment the counter, but never do a trim.
     *   (c) Otherwise: increment the counter, then check to see if it's
     *       time for trim.  Again, a trim will reset the counter.
     *
     * There are a few different operations involved here, and we may
     * get interleaved with someone else doing the same thing.  We
     * don't worry too much, as the worst-case scenario is not all
     * that bad.  But if there are a lot of concurrent login failures,
     * we could end up allowing a few more than the check frequency
     * extra records get into the database.  (XXX/EMT: we could
     * (a) decrement the counter by check_freq, instead of resetting,
     * or (b) lock the database.)
     */

    /* ........................................................................
     * Case (a): mark that we want a trim
     */
    if (max_recs_check_freq == 1) {
        syslog(LOG_DEBUG, "Tally log trim: check frequency 1, so skip "
               "counter and always do trim");
        do_trim = 1;
    }

    /* ........................................................................
     * Cases (b) and (c): record the failure by incrementing the counter.
     */
    else {
        sql = "INSERT OR REPLACE INTO " TBN_AUX_TABLE_NAME
            " (" TBN_AUX_FIELD_NAME
            ", " TBN_AUX_FIELD_VALUE_INT
            ") "
            "VALUES "
            "( " TBN_AUX_NAME_FAILURE_COUNT
            ", (COALESCE((SELECT " TBN_AUX_FIELD_VALUE_INT " FROM "
            TBN_AUX_TABLE_NAME " WHERE " TBN_AUX_FIELD_NAME " = "
            TBN_AUX_NAME_FAILURE_COUNT "), 0) + 1)"
            ")";
        
        sqlite_utils_finalize(&stmt);
        err = sqlite3_prepare(db, sql, -1, &stmt, NULL);
        bail_error_sqlite(err, db, sql);
        
        err = sqlite_utils_step(stmt);
        bail_error_sqlite(err, db, sql);

        /*
         * Case (b): we're done
         */
        if (max_recs == 0) {
            syslog(LOG_DEBUG, "Tally log trim: incremented counter, but "
                   "max_recs 0, so skipping trim");
            goto bail;
        }

        /* ....................................................................
         * Case (c): figure out if it's time for a trim
         */
        sql = "SELECT " TBN_AUX_FIELD_VALUE_INT " FROM " TBN_AUX_TABLE_NAME
            " WHERE " TBN_AUX_FIELD_NAME " = " TBN_AUX_NAME_FAILURE_COUNT;

        sqlite_utils_finalize(&stmt);
        err = sqlite3_prepare(db, sql, -1, &stmt, NULL);
        bail_error_sqlite(err, db, sql);
        
        err = sqlite_utils_step(stmt);
        if (err == SQLITE_DONE) {
            /* Unexpected!  We just created the row above! */
            err = TBN_ERR_GENERIC;
            syslog(LOG_WARNING, "WARNING: Tally log trim: failed to query "
                   "field %s even though we just created it!  Forcing trim",
                   TBN_AUX_NAME_FAILURE_COUNT);
            num_failures = max_recs_check_freq;
        }
        else if (err == SQLITE_ROW) {
            err = 0;
            num_failures = sqlite3_column_int(stmt, 0);
            err = sqlite_utils_step(stmt); /* Finish, to avoid SQLITE_LOCKED */
            /* We now expect SQLITE_DONE */
        }
        
        bail_error_sqlite(err, db, sql);

        do_trim = (num_failures >= max_recs_check_freq);
        syslog(LOG_DEBUG, "Tally log trim: %ld failures since last trim, "
               "max %d, so %s", num_failures, max_recs_check_freq,
               do_trim ? "doing a trim now" : "not doing a trim yet");
    }

    /* ........................................................................
     * Cases (a) and (c): do the trim, if warranted.
     */
    if (do_trim) {
        err = tbn_trim_db(db, max_recs);
        if (err) {
            syslog(LOG_WARNING, "WARNING: Tally log trim: failed");
        }
    }

 bail:
    sqlite_utils_finalize(&stmt);
    return(err);
}


/* ------------------------------------------------------------------------- */
/* We have more than one SQL statement to perform, but note that we are
 * meant to be called while holding an EXCLUSIVE lock, so it's OK.
 */
int
tbn_record_failure(sqlite3 *db, const char *username,
                   tbn_username_mapping mapping_wanted, int known,
                   const char *host, const char *tty,
                   int max_recs, int max_recs_check_freq)
{
    int err = 0;
    const char *sql = NULL;
    sqlite3_stmt *stmt = NULL;
    char *host_trunc = NULL;
    int host_was_truncated = 0;
    tbn_username_mapping mapping_found = tum_none;
    int now = 0;
    int num_failures = 0;
    char *username_trunc = NULL, *username_mapped = NULL;
    const char *username_found = NULL;
    int username_was_truncated = 0, trunc1 = 0;
    int have_rec = 0;
    const char *username1 = NULL, *username2 = NULL;
    tbn_username_mapping mapping1 = tum_none, mapping2 = tum_none;

    if (db == NULL || username == NULL) {
        err = TBN_ERR_GENERIC;
        goto bail;
    }

    /* ........................................................................
     * Get straight the fields we'll be setting.
     */
    if (host == NULL) {
        host = "";
    }

    if (tty == NULL) {
        tty = "";
    }

    err = tbn_string_fixup(host, &host_trunc, &host_was_truncated,
                           tum_none, NULL);
    if (err) {
        goto bail;
    }

    now = tbn_curr_time();

    /*
     * Only one of these will end up in the database, according to
     * 'mapping_wanted'.  But we potentially need both of them, to 
     * find the existing record in the database for this user (if any).
     */
    err = tbn_string_fixup(username, &username_trunc, &username_was_truncated,
                           tum_hash, &username_mapped);
    if (err) {
        goto bail;
    }

    /* ........................................................................
     * Figure out what record already exists for this user, if any.
     *
     * As in various other places, we do this using potentially two
     * queries, rather than one with an 'OR', so we can use the
     * auto-index on username.  But if we find the record with the
     * first query, we don't need to do the second.
     *
     * We first try whichever one is judged more likely (based on what
     * mapping the caller says they want, assuming the config has been
     * that way for a while).  So shortly after a config change,
     * accounts with an existing record will tend to have to be
     * queried twice the first time, but the system will soon converge
     * on having to do only a single query for each.
     *
     * As long as we're doing a query anyway, get the num_failures
     * from any record we find.  We'll use this to help us update the
     * record.
     *
     * (We could alternately have done it as part of the update, using
     * an inline query and a SQL expression to increment it.  That's
     * what we did before, when it was a single "INSERT OR REPLACE
     * INTO" statement without an explicit transaction.  But since
     * we're inside an exclusive transaction here, it just risks
     * adding a needless extra query.
     */
    sql = "SELECT " TBN_FIELD_NUM_FAILURES " FROM " TBN_TABLE_NAME
        " WHERE " TBN_FIELD_USERNAME " = " TBN_BPARAM_USERNAME_STR
        " AND " TBN_FIELD_USERNAME_MAPPED " = " TBN_BPARAM_USERNAME_MAPPED_STR;

    if (mapping_wanted == tum_hash) {
        username1 = username_mapped;
        mapping1 = tum_hash;
        trunc1 = 0;
        username2 = username_trunc;
        mapping2 = tum_none;
    }
    else {
        username1 = username_trunc;
        mapping1 = tum_none;
        trunc1 = username_was_truncated;
        username2 = username_mapped;
        mapping2 = tum_hash;
    }

    /* ........................................................................
     * Step 1: check under the most likely username.
     */
    sqlite_utils_finalize(&stmt);
    err = sqlite3_prepare(db, sql, -1, &stmt, NULL);
    bail_error_sqlite(err, db, sql);

    err = sqlite3_bind_text(stmt, TBN_BPARAM_USERNAME, username1,
                            -1, SQLITE_STATIC);
    bail_error_sqlite(err, db, sql);

    err = sqlite3_bind_int(stmt, TBN_BPARAM_USERNAME_MAPPED, mapping1);
    bail_error_sqlite(err, db, sql);

    err = sqlite_utils_step(stmt);
    if (err == SQLITE_ROW) {
        have_rec = 1;
        username_found = username1;
        mapping_found = mapping1;
        num_failures = sqlite3_column_int(stmt, 0);
        err = sqlite_utils_step(stmt);
    }
    bail_error_sqlite(err, db, sql);

    /* ........................................................................
     * Step 2: check under whichever other username we didn't already
     * check above.  Note that the SQL we use is the same in both
     * cases; the only thing that changes is what we sub in for the
     * two parameters.
     */
    if (have_rec == 0 && username2) {
        sqlite_utils_finalize(&stmt);
        err = sqlite3_prepare(db, sql, -1, &stmt, NULL);
        bail_error_sqlite(err, db, sql);
        
        err = sqlite3_bind_text(stmt, TBN_BPARAM_USERNAME, username2,
                                -1, SQLITE_STATIC);
        bail_error_sqlite(err, db, sql);

        err = sqlite3_bind_int(stmt, TBN_BPARAM_USERNAME_MAPPED, mapping2);
        bail_error_sqlite(err, db, sql);
        
        err = sqlite_utils_step(stmt);
        if (err == SQLITE_ROW) {
            have_rec = 1;
            username_found = username2;
            mapping_found = mapping2;
            num_failures = sqlite3_column_int(stmt, 0);
            err = sqlite_utils_step(stmt);
        }
        bail_error_sqlite(err, db, sql);
    }

    /* ........................................................................
     * Record the failure in the db.
     *
     * We now have either:
     *   (a) have_rec set (with old num_failures set)
     *   (c) have_rec not set (with num_failures zero)
     *
     * It doesn't matter to us which kind of record we found above.
     * We'll do an UPDATE OR REPLACE which sets the username and
     * username_mapped fields; this will update them, or leave them
     * alone, as necessary.  Note that the only field we leave alone
     * during our update is 'locked'.
     *
     * The "OR REPLACE" tells it to stomp on any previously existing
     * row that would conflict with us.  This could only come up when
     * we are updating the username field, to change from mapped to
     * unmapped, or vice-versa.  That should never happen if we are the
     * only ones to update the database, because we would never end up
     * leaving the mapped and unmapped forms of a username both present.
     * We mainly have the "OR REPLACE" as a defensive measure, in case
     * someone else has messed with the database, since otherwise we'd
     * get a SQL error in that case.
     *
     * If we have neither kind of record, we INSERT a new one.  Again,
     * we don't need to set 'locked'.
     */
    if (have_rec) {
        sql = "UPDATE OR REPLACE " TBN_TABLE_NAME
            " SET " TBN_FIELD_USERNAME " = "
                    TBN_BPARAM_USERNAME_STR
            " , " TBN_FIELD_USERNAME_MAPPED " = "
                  TBN_BPARAM_USERNAME_MAPPED_STR
            " , " TBN_FIELD_USERNAME_TRUNCATED " = "
                  TBN_BPARAM_USERNAME_TRUNCATED_STR
            " , " TBN_FIELD_KNOWN " = "
                  TBN_BPARAM_KNOWN_STR
            " , " TBN_FIELD_NUM_FAILURES " = "
                  TBN_BPARAM_NUM_FAILURES_STR
            " , " TBN_FIELD_LAST_FAILURE_TIME " = "
                  TBN_BPARAM_LAST_FAILURE_TIME_STR
            " , " TBN_FIELD_LAST_FAILURE_HOST " = "
                  TBN_BPARAM_LAST_FAILURE_HOST_STR
            " , " TBN_FIELD_LAST_FAILURE_HOST_TRUNCATED " = "
                  TBN_BPARAM_LAST_FAILURE_HOST_TRUNCATED_STR
            " , " TBN_FIELD_LAST_FAILURE_TTY " = "
                  TBN_BPARAM_LAST_FAILURE_TTY_STR
            " WHERE " TBN_FIELD_USERNAME " = " TBN_BPARAM_ALT_USERNAME_STR;
    }
    else {
        num_failures = 0;
        sql = "INSERT INTO " TBN_TABLE_NAME
            " (" TBN_FIELD_USERNAME
            ", " TBN_FIELD_USERNAME_MAPPED
            ", " TBN_FIELD_USERNAME_TRUNCATED
            ", " TBN_FIELD_KNOWN
            ", " TBN_FIELD_NUM_FAILURES
            ", " TBN_FIELD_LAST_FAILURE_TIME
            ", " TBN_FIELD_LAST_FAILURE_HOST
            ", " TBN_FIELD_LAST_FAILURE_HOST_TRUNCATED
            ", " TBN_FIELD_LAST_FAILURE_TTY
            ") "
            "VALUES "
            "( " TBN_BPARAM_USERNAME_STR
            ", " TBN_BPARAM_USERNAME_MAPPED_STR
            ", " TBN_BPARAM_USERNAME_TRUNCATED_STR
            ", " TBN_BPARAM_KNOWN_STR
            ", " TBN_BPARAM_NUM_FAILURES_STR
            ", " TBN_BPARAM_LAST_FAILURE_TIME_STR
            ", " TBN_BPARAM_LAST_FAILURE_HOST_STR
            ", " TBN_BPARAM_LAST_FAILURE_HOST_TRUNCATED_STR
            ", " TBN_BPARAM_LAST_FAILURE_TTY_STR
            ")";
    }

    sqlite_utils_finalize(&stmt);
    err = sqlite3_prepare(db, sql, -1, &stmt, NULL);
    bail_error_sqlite(err, db, sql);

    err = sqlite3_bind_text(stmt, TBN_BPARAM_USERNAME, username1,
                            -1, SQLITE_STATIC);
    bail_error_sqlite(err, db, sql);
    
    err = sqlite3_bind_int(stmt, TBN_BPARAM_USERNAME_MAPPED, mapping1);
    bail_error_sqlite(err, db, sql);

    err = sqlite3_bind_int(stmt, TBN_BPARAM_USERNAME_TRUNCATED, trunc1);
    bail_error_sqlite(err, db, sql);

    err = sqlite3_bind_int(stmt, TBN_BPARAM_KNOWN, known);
    bail_error_sqlite(err, db, sql);

    ++num_failures;
    err = sqlite3_bind_int(stmt, TBN_BPARAM_NUM_FAILURES, num_failures);
    bail_error_sqlite(err, db, sql);

    err = sqlite3_bind_int(stmt, TBN_BPARAM_LAST_FAILURE_TIME, now);
    bail_error_sqlite(err, db, sql);

    err = sqlite3_bind_text(stmt, TBN_BPARAM_LAST_FAILURE_HOST, host_trunc,
                            -1, SQLITE_STATIC);
    bail_error_sqlite(err, db, sql);

    err = sqlite3_bind_int(stmt, TBN_BPARAM_LAST_FAILURE_HOST_TRUNCATED,
                           host_was_truncated);
    bail_error_sqlite(err, db, sql);

    err = sqlite3_bind_text(stmt, TBN_BPARAM_LAST_FAILURE_TTY, tty,
                            -1, SQLITE_STATIC);
    bail_error_sqlite(err, db, sql);

    if (have_rec) {
        err = sqlite3_bind_text(stmt, TBN_BPARAM_ALT_USERNAME, username_found,
                                -1, SQLITE_STATIC);
        bail_error_sqlite(err, db, sql);
    }

    err = sqlite_utils_step(stmt);
    bail_error_sqlite(err, db, sql);

    /* ........................................................................
     * Trim the database if needed.
     */
    err = tbn_maybe_trim_db(db, max_recs, max_recs_check_freq);
    /* Let 'err' fall thru */

 bail:
    if (username_trunc) {
        free(username_trunc);
    }
    if (username_mapped) {
        free(username_mapped);
    }
    if (host_trunc) {
        free(host_trunc);
    }
    sqlite_utils_finalize(&stmt);
    return(err);
}


/* ------------------------------------------------------------------------- */
int
tbn_maybe_lock_user(sqlite3 *db, const char *username,
                    tbn_username_mapping mapping, int max_fail,
                    int *ret_user_existed, int *ret_locked_user)
{
    int err = 0;
    char *username_trunc = NULL, *username_mapped = NULL;
    int username_was_truncated = 0;
    const char *username_chosen = NULL;
    const char *sql = NULL;
    sqlite3_stmt *stmt = NULL;
    int existed = 0;
    int num_changed = 0;
    int was_locked = 0;
    int locked_user = 0;

    if (db == NULL || username == NULL) {
        err = TBN_ERR_GENERIC;
        goto bail;
    }

    err = tbn_string_fixup(username, &username_trunc, &username_was_truncated,
                           mapping, &username_mapped);
    if (err) {
        goto bail;
    }

    username_chosen = username_mapped ? username_mapped : username_trunc;

    /* ........................................................................
     * 1. See if the user exists, and if so, if they are locked.
     *
     * Note: we only check in the place where this user is supposed to be.
     * Our assumption is that a failure was just recorded, so the user was
     * at that time moved to where they should be.
     */
    sql = "SELECT " TBN_FIELD_LOCKED " FROM " TBN_TABLE_NAME
        " WHERE " TBN_FIELD_USERNAME " = " TBN_BPARAM_USERNAME_STR;

    sqlite_utils_finalize(&stmt);
    err = sqlite3_prepare(db, sql, -1, &stmt, NULL);
    bail_error_sqlite(err, db, sql);

    err = sqlite3_bind_text(stmt, TBN_BPARAM_USERNAME, username_chosen,
                            -1, SQLITE_STATIC);
    bail_error_sqlite(err, db, sql);

    err = sqlite_utils_step(stmt);
    if (err == SQLITE_ROW) {
        err = 0;
        existed = 1;
        was_locked = sqlite3_column_int(stmt, 0);
        err = sqlite_utils_step(stmt);
        bail_error_sqlite(err, db, sql);
    }
    else if (err == SQLITE_DONE) {
        err = 0;
        existed = 0;
        locked_user = 0;
        goto bail;
    }
    else {
        bail_error_sqlite(err, db, sql);
    }

    /* ........................................................................
     * 2. If the user was already locked, we're done.
     */
    if (was_locked) {
        /* existed = 1 (already) */
        locked_user = 0;
        goto bail;
    }

    /* ........................................................................
     * 3. If we get here, the user exists and is not locked.  So here's our
     *    chance to lock him.
     */
    sql = "UPDATE " TBN_TABLE_NAME
        " SET " TBN_FIELD_LOCKED " = 1"
        " WHERE " TBN_FIELD_USERNAME " = " TBN_BPARAM_USERNAME_STR
        " AND " TBN_FIELD_NUM_FAILURES " >= " TBN_BPARAM_NUM_FAILURES_STR;

    sqlite_utils_finalize(&stmt);
    err = sqlite3_prepare(db, sql, -1, &stmt, NULL);
    bail_error_sqlite(err, db, sql);

    err = sqlite3_bind_text(stmt, TBN_BPARAM_USERNAME, username_chosen,
                            -1, SQLITE_STATIC);
    bail_error_sqlite(err, db, sql);

    err = sqlite3_bind_int(stmt, TBN_BPARAM_NUM_FAILURES, max_fail);
    bail_error_sqlite(err, db, sql);

    err = sqlite_utils_step(stmt);
    bail_error_sqlite(err, db, sql);

    num_changed = sqlite3_changes(db);

    /*
     * This means we matched a record, and therefore that we just locked
     * the user.  We know it was not already locked because we just checked
     * above, and we are inside an exclusive transaction here.
     */
    locked_user = (num_changed > 0);

 bail:
    if (username_trunc) {
        free(username_trunc);
    }
    if (username_mapped) {
        free(username_mapped);
    }
    if (ret_user_existed) {
        *ret_user_existed = existed;
    }
    if (ret_locked_user) {
        *ret_locked_user = locked_user;
    }
    sqlite_utils_finalize(&stmt);
    return(err);
}
