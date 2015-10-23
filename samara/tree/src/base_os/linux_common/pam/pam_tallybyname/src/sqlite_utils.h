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

#ifndef __SQLITE_UTILS_H_
#define __SQLITE_UTILS_H_

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

#include <sqlite3.h>
#include <syslog.h>


void bail_debug_sqlite(void);

/* ------------------------------------------------------------------------- */
/** Bail macro tailored for use with SQLite calls.
 *
 * Note that we have to treat SQLITE_DONE as a success case to deal
 * properly with results from sqlite3_step() / sqlite_utils_step().
 * Note that we do NOT treat SQLITE_ROW as success.  The expected usage
 * is one of the following:
 *   (a) sqlite3_step() expecting multiple rows: you looped as long as the
 *       result was SQLITE_ROW, and then exited and came here (expecting
 *       SQLITE_DONE).
 *   (b) sqlite3_step() excepting a single row: you stepped once, dealt 
 *       with the results, then stepped again, and came here (expecting
 *       SQLITE_DONE).
 *   (c) sqlite3_step() with something not a query, so not expecting 
 *       any rows.  Come here directly (expecting SQLITE_DONE).
 *   (d) Some other sqlite call.  Come here directly (expecting SQLITE_OK).
 *
 * In case (a), you should not actually break out before processing
 * all rows, even if you manually set your return value to SQLITE_DONE.
 * It may not allow you to finalize your step in this case!
 *
 * Note that in most cases, if we got an error code back from an API,
 * we're likely to get the same error code back from sqlite3_errcode().
 * But we do query separately just in case, and so we can get the error
 * message too.  (There doesn't seem to be a function to get a message
 * just from a raw code.)
 */
#define bail_error_sqlite(errcode, db, msg)                                   \
    do {                                                                      \
        if (errcode != SQLITE_OK && errcode != SQLITE_DONE) {                 \
            int __do_goto = 1;                                                \
            if (db) {                                                         \
                syslog(LOG_ERR, "ERROR: %s(), %s:%u.  "                       \
                       "Sqlite error %d.  "                                   \
                       "Database says: %s (code %d).  Msg: %s",               \
                       __FUNCTION__, __FILE__, __LINE__,                      \
                       errcode, sqlite3_errmsg(db), sqlite3_errcode(db),      \
                       msg ? msg : "");                                       \
            }                                                                 \
            else {                                                            \
                syslog(LOG_ERR, "ERROR: %s(), %s:%u.  "                       \
                       "Sqlite error %d.  "                                   \
                       "NULL database.  Msg: %s",                             \
                       __FUNCTION__, __FILE__, __LINE__,                      \
                       errcode, msg ? msg : "");                              \
            }                                                                 \
            bail_debug_sqlite();                                              \
            if (__do_goto) {                                                  \
                goto bail;                                                    \
            }                                                                 \
        }                                                                     \
        else {                                                                \
            /* If it was SQLITE_DONE, don't treat as error later */           \
            errcode = SQLITE_OK;                                              \
        }                                                                     \
    } while (0)


/* ------------------------------------------------------------------------- */
/** Variant on bail_error_sqlite() which never logs anything.  This is
 * mainly for use within sqlite_utils itself, which wants to leave all
 * the logging to its caller.  The caller may decide what is
 * log-worthy and what isn't and may also have different ways of
 * logging, e.g. syslog() vs. lc_log_basic() vs. pam_syslog(), etc.
 */
#define bail_error_sqlite_quiet(errcode)                                      \
    do {                                                                      \
        if (errcode != SQLITE_OK && errcode != SQLITE_DONE) {                 \
            int __do_goto = 1;                                                \
            bail_debug_sqlite();                                              \
            if (__do_goto) {                                                  \
                goto bail;                                                    \
            }                                                                 \
        }                                                                     \
        else {                                                                \
            /* If it was SQLITE_DONE, don't treat as error later */           \
            errcode = SQLITE_OK;                                              \
        }                                                                     \
    } while (0)


/* ------------------------------------------------------------------------- */
/** Transaction mode, mirroring the set of options offered by SQLite itself.
 * See descriptions at: http://sqlite.org/lang_transaction.html
 */
typedef enum {
    sutm_none = 0,
    sutm_deferred,
    sutm_immediate,
    sutm_exclusive
} sqlite_utils_transaction_mode;


/* ------------------------------------------------------------------------- */
/** Begin a transaction.  This controls locking behavior of a SQLite
 * database.  Critical sections in a set of multiple SQL statements
 * may want to be bracketed by this and sqlite_utils_transaction_end().
 *
 * NOTE: every call to this MUST be balanced by a call to
 * sqlite_utils_transaction_end(), or by passing 1 to 
 * sqlite_utils_close().
 */
int sqlite_utils_transaction_begin(sqlite3 *db,
                                   sqlite_utils_transaction_mode mode);


/* ------------------------------------------------------------------------- */
/** End a transaction, presumably begun earlier with
 * sqlite_utils_transaction_begin().
 *
 * NOTE: an error will result if this is done without a previous 
 * sqlite_utils_transaction_begin().
 */
int sqlite_utils_transaction_end(sqlite3 *db);


/* ------------------------------------------------------------------------- */
/** Like sqlite3_step(), except we pave over an idiosyncracy in its
 * error reporting.  If something goes wrong with sqlite3_step(), it
 * returns the generic SQLITE_ERROR, which doesn't really tell you
 * what went wrong.  In this case, it expects you to call
 * sqlite3_reset() on the step, and it may return a more specific
 * error code.  So we do that for you.
 *
 * (The docs say that if the step is constructed with the more recent
 * sqlite3_prepare_v2(), it doesn't do this, and just gives you the
 * real error right off.  However, EL5's sqlite does not have that new
 * interface, so we are stuck with the old one for now.)
 */
int sqlite_utils_step(sqlite3_stmt *stmt);


/* ------------------------------------------------------------------------- */
/** Like sqlite3_finalize(), except we (a) check the return code and log
 * something on error, and (b) NULL out the step pointer to make sure you
 * don't try to use it again.
 */
void sqlite_utils_finalize(sqlite3_stmt **inout_stmt);


/* ------------------------------------------------------------------------- */
/** Tell if a table exists in the provided db.
 */
int sqlite_utils_table_exists(sqlite3 *db, const char *table_name,
                              int *ret_exists);


/* ------------------------------------------------------------------------- */
/** Tell if an index exists for the specified table in the provided db.
 */
int sqlite_utils_index_exists(sqlite3 *db, const char *table_name,
                              const char *index_name, int *ret_exists);


/* ------------------------------------------------------------------------- */
/** Close a database file.  This is a wrapper for sqlite3_close(), which
 * offers two improvements:
 *   - Let the caller specify if there is an open transaction which
 *     should be closed.
 *   - NULL out the database pointer, to avoid accidental dangling
 *     references.
 */
int sqlite_utils_close(sqlite3 **inout_db, int end_transaction);


#ifdef __cplusplus
}
#endif

#endif /* __SQLITE_UTILS_H_ */
