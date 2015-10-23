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

/* ------------------------------------------------------------------------- */
/* NOTE: this file should not include any TMS headers (outside its own
 * directory), as it needs to be able to build within a PAM module,
 * and does not link with libcommon, etc.
 *
 * For booleans, we use ints instead, with 0 for false and 1 for true.
 */

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "sqlite_utils.h"


/*
 * Keep track of how long a SQLite call takes, and possibly log something
 * about it.  We log if it takes longer than an arbitrary threshold amount
 * (SQLUTILS_CALL_LOG_THRESHOLD_MS), or if it was a SQLITE_BUSY or
 * SQLITE_LOCKED error.
 *
 * These utilities are currently only used by pam_tallybyname, which
 * calls sqlite3_busy_timeout(aaa_tbn_db_busy_timeout_ms) during init,
 * and that constant defaults to 7.5 seconds, though it can be overridden
 * by a customer.  Our own threshold is considerably shorter than that.
 * So if we get SQLITE_BUSY or SQLITE_LOCKED, we probably would have
 * passed our logging threshold anyway.  We include that as a safeguard
 * to make sure that something still gets logged if (a) someone lowers
 * aaa_tbn_db_busy_timeout_ms for some reason; (b) we somehow manage
 * to get SQLITE_BUSY or SQLITE_LOCKED without waiting the timeout;
 * or (c) these utilities are used from some other context which does
 * not install a timeout.
 *
 * These are patterned after VIW_START and VIW_END from tvirtd.
 *
 * XXXX/EMT: we should be reading from /proc/uptime instead, so we 
 * won't get confused if the clock changes.  We can't use
 * lt_sys_uptime_ms() since we're not linking with libcommon.
 */

#define SQLUTILS_CALL_LOG_THRESHOLD_MS 1000

#define SQLUTILS_START                                                        \
    long long int start = 0, finish = 0;                                      \
    start = sqlite_utils_curr_time_ms();

#define SQLUTILS_END(err)                                                     \
    finish = sqlite_utils_curr_time_ms();                                     \
    if (err == SQLITE_BUSY || err == SQLITE_LOCKED) {                         \
        syslog(LOG_WARNING,                                                   \
               "WARNING: SQLite call failed because database "                \
               "was busy or locked (%s).  Call took %lld ms.",                \
               (err == SQLITE_BUSY) ? "SQLITE_BUSY" : "SQLITE_LOCKED",        \
               (finish - start));                                             \
    }                                                                         \
    else if (finish - start >= SQLUTILS_CALL_LOG_THRESHOLD_MS) {              \
        syslog(LOG_INFO,                                                      \
               "INFO: SQLite call (%s) took longer than expected: %lld ms.",  \
               __FUNCTION__, (finish - start));                               \
    }


/* ------------------------------------------------------------------------- */
void
bail_debug_sqlite(void)
{
    volatile int x __attribute__((unused)) = 0;
}


/* ------------------------------------------------------------------------- */
/* Local variant of lt_curr_time_ms(), since we can't use libcommon calls.
 */
static long long int
sqlite_utils_curr_time_ms(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (tv.tv_sec * (long long int) 1000 + tv.tv_usec / 1000);
}


/* ------------------------------------------------------------------------- */
int
sqlite_utils_transaction_begin(sqlite3 *db,
                               sqlite_utils_transaction_mode xaction_mode)
{
    int err = 0;
    const char *sql = NULL;
    sqlite3_stmt *stmt = NULL;

    SQLUTILS_START;

    if (db == NULL) {
        err = -1;
        goto bail;
    }

    switch (xaction_mode) {
    case sutm_deferred:
        sql = "BEGIN DEFERRED TRANSACTION";
        break;

    case sutm_immediate:
        sql = "BEGIN IMMEDIATE TRANSACTION";
        break;

    case sutm_exclusive:
        sql = "BEGIN EXCLUSIVE TRANSACTION";
        break;

    default:
        err = -1;
        goto bail;
    }

    err = sqlite3_prepare(db, sql, -1, &stmt, NULL);
    bail_error_sqlite_quiet(err);
    
    err = sqlite_utils_step(stmt);
    bail_error_sqlite_quiet(err);

 bail:
    SQLUTILS_END(err);
    sqlite_utils_finalize(&stmt);
    return(err);
}


/* ------------------------------------------------------------------------- */
int
sqlite_utils_transaction_end(sqlite3 *db)
{
    int err = 0;
    const char *sql = NULL;
    sqlite3_stmt *stmt = NULL;

    SQLUTILS_START;

    if (db == NULL) {
        err = -1;
        goto bail;
    }

    sql = "END TRANSACTION";

    err = sqlite3_prepare(db, sql, -1, &stmt, NULL);
    bail_error_sqlite_quiet(err);
    
    err = sqlite_utils_step(stmt);
    bail_error_sqlite_quiet(err);

 bail:
    SQLUTILS_END(err);
    sqlite_utils_finalize(&stmt);
    return(err);
}


/* ------------------------------------------------------------------------- */
int
sqlite_utils_step(sqlite3_stmt *stmt)
{
    int err = 0;

    SQLUTILS_START;

    if (stmt == NULL) {
        err = -1;
        goto bail;
    }

    err = sqlite3_step(stmt);
    if (err == SQLITE_ERROR) {
        err = sqlite3_reset(stmt);
    }

    /* Let 'err' fall through in both cases */

 bail:
    SQLUTILS_END(err);
    return(err);
}


/* ------------------------------------------------------------------------- */
void
sqlite_utils_finalize(sqlite3_stmt **inout_stmt)
{
    int err = 0;

    if (inout_stmt && *inout_stmt) {
        err = sqlite3_finalize(*inout_stmt);
        if (err) {
            syslog(LOG_WARNING, "WARNING: unable to finalize sqlite3_stmt %p",
                   *inout_stmt);
        }
        else {
            *inout_stmt = NULL;
        }
    }

 bail:
    return;
}


/* ------------------------------------------------------------------------- */
int
sqlite_utils_table_exists(sqlite3 *db, const char *table_name,
                          int *ret_exists)
{
    int err = 0;
    int exists = 0;
    const char *sql = NULL;
    sqlite3_stmt *stmt = NULL;

    SQLUTILS_START;

    if (db == NULL || table_name == NULL) {
        err = -1;
        goto bail;
    }

    sql = "SELECT name FROM sqlite_master WHERE "
        "type = 'table' AND name = :table";

    err = sqlite3_prepare(db, sql, -1, &stmt, NULL);
    bail_error_sqlite_quiet(err);

    err = sqlite3_bind_text(stmt, 1, table_name, -1, SQLITE_STATIC);
    bail_error_sqlite_quiet(err);

    err = sqlite_utils_step(stmt);
    if (err == SQLITE_DONE) {
        exists = 0;
        err = 0;
    }
    else if (err == SQLITE_ROW) {
        exists = 1;
        err = 0;
    }
    bail_error_sqlite_quiet(err);

 bail:
    SQLUTILS_END(err);
    sqlite_utils_finalize(&stmt);
    if (ret_exists) {
        *ret_exists = exists;
    }
    return(err);
}


/* ------------------------------------------------------------------------- */
int
sqlite_utils_index_exists(sqlite3 *db, const char *table_name,
                          const char *index_name, int *ret_exists)
{
    int err = 0;
    int exists = 0;
    const char *sql = NULL;
    sqlite3_stmt *stmt = NULL;

    SQLUTILS_START;

    if (db == NULL || table_name == NULL) {
        err = -1;
        goto bail;
    }

    sql = "SELECT name FROM sqlite_master WHERE "
        "type = 'index' AND name = :index AND tbl_name = :table";

    err = sqlite3_prepare(db, sql, -1, &stmt, NULL);
    bail_error_sqlite_quiet(err);

    err = sqlite3_bind_text(stmt, 1, index_name, -1, SQLITE_STATIC);
    bail_error_sqlite_quiet(err);

    err = sqlite3_bind_text(stmt, 2, table_name, -1, SQLITE_STATIC);
    bail_error_sqlite_quiet(err);

    err = sqlite_utils_step(stmt);
    if (err == SQLITE_DONE) {
        exists = 0;
        err = 0;
    }
    else if (err == SQLITE_ROW) {
        exists = 1;
        err = 0;
    }
    bail_error_sqlite_quiet(err);

 bail:
    SQLUTILS_END(err);
    sqlite_utils_finalize(&stmt);
    if (ret_exists) {
        *ret_exists = exists;
    }
    return(err);
}


/* ------------------------------------------------------------------------- */
int
sqlite_utils_close(sqlite3 **inout_db, int end_transaction)
{
    int err = 0;
    
    if (inout_db == NULL) {
        err = -1;
        goto bail;
    }

    if (*inout_db) {
        if (end_transaction) {
            err = sqlite_utils_transaction_end(*inout_db);
            if (err) {
                syslog(LOG_INFO, "SQLite_utils: unable to end transaction");
            }
            /* Stifle this error and move on to closing the db anyway */
            err = 0;
        }
        err = sqlite3_close(*inout_db);
        if (err == 0) {
            *inout_db = NULL;
        }
        else {
            syslog(LOG_INFO, "SQLite_utils: unable to close database");
            /* Pass this error through to the caller */
        }

    }

 bail:
    return(err);
}
