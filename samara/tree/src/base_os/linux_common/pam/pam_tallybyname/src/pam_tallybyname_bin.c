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

#include <unistd.h>
#include "common.h"
#include "logging.h"
#include "ttime.h"
#include "escape_utils.h"
#include "file_utils.h"
#include "type_conv.h"
#include "pam_tallybyname_common.h"

typedef enum {
    tot_none = 0,
    tot_list,
    tot_reset,
    tot_purge,
    tot_failure,
    tot_lock,
    tot_map,
} tbn_oper_type;

tbn_oper_type tbn_oper = tot_none;
char *tbn_username = NULL;
uint32 tbn_max_recs = 0;
char *tbn_hostname = NULL;
char *tbn_tty = NULL;
char *tbn_path_override = NULL;
tbool tbn_all = false;
tbool tbn_maybe_capital = false;
tbool tbn_machread = false;
tbool tbn_user_unknown = false;
tbool tbn_clear_only = false, tbn_unlock_only = false;
tbn_username_mapping tbn_mapping = tum_none;


/* ------------------------------------------------------------------------- */
static void
tbn_bin_usage(void)
{
    int err = 1; /* On purpose, to return an error */

    printf
        ("Usage:\n"
         "   %s -l [-F <path>] [-u <username> | -a] [-m]\n"
         "   %s -r [-F <path>] {-u <username> | -a} [-c | -U]\n"
         "   %s -p [-F <path>] {-u <username> | -a | -C | -M <max recs>}\n"
         "   %s -f [-F <path>]  -u <username> [-h <hostname>] [-t <TTY>]\n"
         "             "
         "                                    [-i <mapping ID>] [-n]\n"
         "   %s -k [-F <path>]  -u <username> [-i <mapping ID>]\n"
         "   %s -o              -u <username>  -i <mapping ID>\n"
         "\n"
         "   -l: list user status (defaults to all users, '-a' ignored)\n"
         "   -r: reset user (no default, requires username or '-a' to reset "
         "all)\n"
         "   -p: purge user (like -r, except also deletes whole user record)\n"
         "   -f: record login failure for specified user (with no automatic "
         "purging)\n"
         "   -k: lock out a specified user\n"
         "   -o: map a specified username, and print the result\n"
         "\n"
         "   -F <path>: path of file to operate on (default %s)\n"
         "   -u <username>: username to be viewed or operated upon\n"
         "   -M <max recs>: maximum number of records to keep; delete oldest "
         "unknowns\n"
         "   -h <hostname>: specify remote hostname for failure "
         "(only with '-f')\n"
         "   -t <TTY>: specify TTY for failure (only with '-f')\n"
         "   -i <mapping ID>: specify particular mapping to perform:\n"
         "      0  no mapping\n"
         "      1  HMAC-SHA-256-192 rendered as Base32\n"
         "\n"
         "   -a: view or operate on all users (only with '-l', '-r', or "
         "'-p')\n"
         "   -C: operate on all usernames which might contain capital "
         "letters: either\n"
         "       do contain capital letters, or are hashed (only with '-p')\n"
         "   -m: machine-readable format (only with '-l')\n"
         "   -c: clear history only, do not unlock user (only with '-r')\n"
         "   -U: unlock user only, do not clear history (only with '-r')\n"
         "   -n: mark user as \"unknown\" (only with '-f')\n"
         "\n"
         "   Machine-readable format means:\n"
         "     - Streamlined column headings\n"
         "     - Comma-separated fields, not column-aligned with whitespace\n"
         "     - Escape username and remote host\n"
         "     - Render boolean flags as integers, instead of 'yes'/'no'\n"
         "     - Render last failure time as integer (time_t)\n"
         "     - More fields included.  The total set is:\n"
         "       {" TBN_FIELD_USERNAME ", " TBN_FIELD_USERNAME_MAPPED ", "
         TBN_FIELD_USERNAME_TRUNCATED ", " TBN_FIELD_KNOWN ", "
         TBN_FIELD_LOCKED ",\n"
         "        " TBN_FIELD_NUM_FAILURES ", "
         TBN_FIELD_LAST_FAILURE_TIME ", " TBN_FIELD_LAST_FAILURE_HOST ", "
         TBN_FIELD_LAST_FAILURE_HOST_TRUNCATED ",\n"
         "        " TBN_FIELD_LAST_FAILURE_TTY "}\n"
         ,
         TBN_BINARY_NAME, TBN_BINARY_NAME, TBN_BINARY_NAME, TBN_BINARY_NAME,
         TBN_BINARY_NAME, TBN_BINARY_NAME, TBN_DEFAULT_LOGFILE);

    lc_log_basic(LOG_INFO, "Bad parameters, usage message printed");
    lc_log_basic(LOG_INFO, "%s finishing (code %d)", TBN_BINARY_NAME, err);
    lc_log_close();
    exit(err);
}


/* ------------------------------------------------------------------------- */
static int
tbn_bin_list(sqlite3 *db, const char *seluser)
{
    int err = 0;
    const char *sql = NULL;
    sqlite3_stmt *stmt = NULL;
    char *username_trunc = NULL, *username_mapped = NULL;
    const unsigned char *username = NULL;
    const unsigned char *last_fail_host = NULL;
    const unsigned char *last_fail_tty = NULL;
    const unsigned char *last_fail_from = NULL;
    int num_failures = 0, known = 0, locked = 0;
    int user_mapped = 0, user_truncated = 0, host_truncated = 0;

    lt_time_sec last_fail_time = 0;
    char *last_fail_time_str = NULL;
    char *username_esc = NULL, *last_fail_host_esc = NULL;

    if (seluser) {
        err = tbn_string_fixup(seluser, &username_trunc, NULL,
                               tum_hash, &username_mapped);
        bail_error(err);

        /*
         * XXX/EMT: unlike other places, we do this in one statement,
         * and so it won't get to use the index.  It makes our coding
         * job easier, but is less efficient.  This code path is not
         * used in normal operation, only by an admin fiddling directly
         * on the command line.
         */
        sql = "SELECT * FROM " TBN_TABLE_NAME
            " WHERE " TBN_FIELD_USERNAME " = " TBN_BPARAM_USERNAME_STR
            " OR " TBN_FIELD_USERNAME " = " TBN_BPARAM_ALT_USERNAME_STR;

        err = sqlite3_prepare(db, sql, -1, &stmt, NULL);
        bail_error_sqlite(err, db, sql);

        err = sqlite3_bind_text(stmt, TBN_BPARAM_USERNAME, username_trunc,
                                -1, SQLITE_STATIC);
        bail_error_sqlite(err, db, sql);

        err = sqlite3_bind_text(stmt, TBN_BPARAM_ALT_USERNAME,
                                username_mapped, -1, SQLITE_STATIC);
        bail_error_sqlite(err, db, sql);
    }
    else {
        /* XXXX/EMT: allow sorting: bind_text w/ field name? */
        sql = "SELECT * FROM " TBN_TABLE_NAME
            " ORDER BY " TBN_FIELD_KNOWN " DESC , " TBN_FIELD_USERNAME;

        err = sqlite3_prepare(db, sql, -1, &stmt, NULL);
        bail_error_sqlite(err, db, sql);
    }

    if (tbn_machread) {
        printf(TBN_FIELD_USERNAME ","
               TBN_FIELD_USERNAME_MAPPED ","
               TBN_FIELD_USERNAME_TRUNCATED ","
               TBN_FIELD_KNOWN ","
               TBN_FIELD_LOCKED ","
               TBN_FIELD_NUM_FAILURES ","
               TBN_FIELD_LAST_FAILURE_TIME ","
               TBN_FIELD_LAST_FAILURE_HOST ","
               TBN_FIELD_LAST_FAILURE_HOST_TRUNCATED ","
               TBN_FIELD_LAST_FAILURE_TTY "\n");
    }
    else {
        printf("%-15s  %-5s  %-6s  %-8s  %-19s  %s\n",
               "Username", "Known", "Locked", "Failures",
               "Last fail time", "Last fail from");
        printf("%-15s  %-5s  %-6s  %-8s  %-19s  %s\n",
               "--------", "-----", "------", "--------",
               "--------------", "--------------");
    }

    while ((err = sqlite_utils_step(stmt)) == SQLITE_ROW) {
        /*
         * XXXX/EMT: deal with mapped usernames -- use a (*) to indicate,
         * here and in CLI.  (Q: when they specify username in a query,
         * do we assume it's already mapped, or ever map it for them?
         * Do they tell us explicitly?  Or do we just try it one way,
         * then the other way?)
         */
        username = sqlite3_column_text(stmt, TBN_COLUMN_USERNAME);
        complain_null(username);
        user_mapped = sqlite3_column_int(stmt, TBN_COLUMN_USERNAME_MAPPED);
        user_truncated = sqlite3_column_int(stmt,
                                            TBN_COLUMN_USERNAME_TRUNCATED);
        known = sqlite3_column_int(stmt, TBN_COLUMN_KNOWN);
        locked = sqlite3_column_int(stmt, TBN_COLUMN_LOCKED);
        num_failures = sqlite3_column_int(stmt, TBN_COLUMN_NUM_FAILURES);
        last_fail_time = sqlite3_column_int(stmt,
                                            TBN_COLUMN_LAST_FAILURE_TIME);
        last_fail_host = sqlite3_column_text(stmt,
                                             TBN_COLUMN_LAST_FAILURE_HOST);
        complain_null(last_fail_host);
        host_truncated = sqlite3_column_int
            (stmt, TBN_COLUMN_LAST_FAILURE_HOST_TRUNCATED);
        last_fail_tty = sqlite3_column_text(stmt,
                                            TBN_COLUMN_LAST_FAILURE_TTY);
        complain_null(last_fail_tty);

        if (tbn_machread) {
            /*
             * Note: it's important that ',' NOT be in the non-escaped
             * char set, since that's our field delimiter.
             */
            err = lc_escape_str((const char *)username, '\\',
                                lecf_non_escaped_char_set,
                                ESC_CLIST_ALPHANUM "-_.", &username_esc);
            bail_error(err);

            err = lc_escape_str((const char *)last_fail_host, '\\',
                                lecf_non_escaped_char_set,
                                ESC_CLIST_ALPHANUM "-_.", &last_fail_host_esc);
            bail_error(err);

            printf("%s,%d,%d,%d,%d,%d,%d,%s,%d,%s\n",
                   username_esc, user_mapped, user_truncated, 
                   known, locked, num_failures, last_fail_time,
                   last_fail_host_esc, host_truncated, last_fail_tty);
        }
        else {
            safe_free(last_fail_time_str);
            if (last_fail_time > 0) {
                err = lt_datetime_sec_to_str(last_fail_time, false,
                                             &last_fail_time_str);
                bail_error(err);
            }
            else {
                last_fail_time_str = strdup("");
                bail_null(last_fail_time_str);
            }

            /*
             * XXX/EMT: are there any legitimate cases where we'd have
             * both, and want to print both?  Note that in the ssh case,
             * it installs the placeholder string "ssh" into PAM_TTY
             * and we don't want to bother displaying that.
             */
            last_fail_from = safe_strlen((const char *)last_fail_host) ?
                last_fail_host : last_fail_tty;

            printf("%-15s  %-5s  %-6s  %-8d  %-19s  %s\n",
                   username, lc_bool_to_friendly_str(known),
                   lc_bool_to_friendly_str(locked), num_failures,
                   last_fail_time_str, last_fail_from);
        }
    }
    bail_error_sqlite(err, db, sql);

 bail:
    safe_free(username_trunc);
    safe_free(username_mapped);
    safe_free(last_fail_time_str);
    safe_free(username_esc);
    safe_free(last_fail_host_esc);
    sqlite_utils_finalize(&stmt);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
tbn_bin_read_args(int argc, char *argv[])
{
    int err = 0;
    int c = 0;
    tbool is_abs = false;

    if (argc < 2) {
        tbn_bin_usage();
    }

    /* ........................................................................
     * Read the arguments
     */
    while ((c = getopt(argc, argv, "F:u:M:h:t:i:lrpfkoamcCUn")) != -1) {
        if ((c == 'l' || c == 'r' || c == 'p' || c == 'f' || c == 'k') &&
            tbn_oper != tot_none) {
            printf("%% Can only specify a single operation (-l, -r, -p, "
                   "-f, or -k)\n");
            tbn_bin_usage();
        }

        switch (c) {
        case 'l':
            tbn_oper = tot_list;
            break;

        case 'r':
            tbn_oper = tot_reset;
            break;

        case 'p':
            tbn_oper = tot_purge;
            break;

        case 'f':
            tbn_oper = tot_failure;
            break;

        case 'k':
            tbn_oper = tot_lock;
            break;

        case 'o':
            tbn_oper = tot_map;
            break;

        case 'F':
            if (!optarg || !*optarg) {
                printf("%% Must specify file path with '-F'.\n");
                tbn_bin_usage();
            }
            tbn_path_override = strdup(optarg);
            bail_null(tbn_path_override);
            break;

        case 'u':
            if (!optarg || !*optarg) {
                printf("%% Must specify username with '-u'.\n");
                tbn_bin_usage();
            }
            tbn_username = strdup(optarg);
            bail_null(tbn_username);
            break;

        case 'M':
            if (!optarg || !*optarg) {
                printf("%% Must specify maximum number of records with "
                       "'-M'.\n");
                tbn_bin_usage();
            }
            err = lc_str_to_uint32(optarg, &tbn_max_recs);
            if (err) {
                printf("%% Invalid number of records: %s.\n", optarg);
                tbn_bin_usage();
            }
            if (tbn_max_recs == 0) {
                printf("%% Use '-a' to purge all users.\n");
                tbn_bin_usage();
            }
            break;

        case 'h':
            if (!optarg || !*optarg) {
                printf("%% Must specify hostname with '-h'.\n");
                tbn_bin_usage();
            }
            tbn_hostname = strdup(optarg);
            bail_null(tbn_hostname);
            break;

        case 't':
            if (!optarg || !*optarg) {
                printf("%% Must specify TTY with '-t'.\n");
                tbn_bin_usage();
            }
            tbn_tty = strdup(optarg);
            bail_null(tbn_tty);
            break;

        case 'i':
            if (!optarg || !*optarg) {
                printf("%% Must specify mapping ID with '-i'.\n");
                tbn_bin_usage();
            }
            if (!strcmp(optarg, "0")) {
                tbn_mapping = tum_none;
            }
            else if (!strcmp(optarg, "1")) {
                tbn_mapping = tum_hash;
            }
            else {
                printf("%% Unrecognized mapping ID: %s\n", optarg);
                tbn_bin_usage();
            }
            break;

        case 'a':
            tbn_all = true;
            break;

        case 'm':
            tbn_machread = true;
            break;

        case 'c':
            tbn_clear_only = true;
            break;

        case 'C':
            tbn_maybe_capital = true;
            break;

        case 'U':
            tbn_unlock_only = true;
            break;

        case 'n':
            tbn_user_unknown = true;
            break;

        default:
            /* getopt() already printed something */
            tbn_bin_usage();
            break;
        }
    }

    /* ........................................................................
     * Validate the arguments
     */
    if (optind < argc) {
        printf("%% Unrecognized parameter: %s\n", argv[optind]);
        tbn_bin_usage();
    }

    if (tbn_oper == tot_none) {
        printf("%% Must specify operation: -l, -r, -p, -f, -k, or -o\n");
        tbn_bin_usage();
    }

    if (tbn_hostname && tbn_oper != tot_failure) {
        printf("%% '-h' can only be used with '-f'.\n");
        tbn_bin_usage();
    }

    if (tbn_tty && tbn_oper != tot_failure) {
        printf("%% '-t' can only be used with '-f'.\n");
        tbn_bin_usage();
    }

    if (tbn_maybe_capital && tbn_oper != tot_purge) {
        printf("%% '-C' can only be used with '-p'.\n");
        tbn_bin_usage();
    }

    if (tbn_path_override) {
        err = lf_path_is_absolute_str(tbn_path_override, &is_abs);
        bail_error(err);
        if (!is_abs) {
            printf("%% If path is specified with '-F', it must be "
                   "absolute.\n");
            tbn_bin_usage();
        }
    }

    if (tbn_username && tbn_all) {
        printf("%% Cannot use '-a' with '-u'.\n");
        tbn_bin_usage();
    }

    if (tbn_all && tbn_oper != tot_list && tbn_oper != tot_reset &&
        tbn_oper != tot_purge) {
        printf("%% '-a' can only be used with '-l', '-r', or '-p'.\n");
        tbn_bin_usage();
    }

    if (tbn_machread && tbn_oper != tot_list) {
        printf("%% '-m' can only be used with '-l'.\n");
        tbn_bin_usage();
    }

    if (tbn_clear_only && tbn_oper != tot_reset) {
        printf("%% '-c' can only be used with '-r'.\n");
        tbn_bin_usage();
    }

    if (tbn_unlock_only && tbn_oper != tot_reset) {
        printf("%% '-U' can only be used with '-r'.\n");
        tbn_bin_usage();
    }

    if (tbn_user_unknown && tbn_oper != tot_failure) {
        printf("%% '-n' can only be used with '-f'.\n");
        tbn_bin_usage();
    }

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
/* Reset our login records for one or all users.  This can mean doing one
 * or both of (a) unlocking the account, and/or (b) clearing the history
 * of login failures for the account.
 *
 * It would have been nice for this to go into pam_tallybyname_common.c,
 * so pam_tbn_reset_user() could call us too.  But we use tstring, so
 * we can't go there.  The module's needs are much simpler (always just
 * delete a single user), so it has its own simplified implementation.
 *
 * \param db Database on which to operate.
 * \param username Username of user to reset, or NULL to reset all users.
 * \param max_recs If no username specified, and if we're doing a purge,
 * this is the maximum number of records to leave in the database.  0 just
 * purges all of them; otherwise, we will delete all but this number.
 * \param purge Completely delete the user's record from the database?
 * If this is true, then 'unlock' and 'clear_history' must both also be true,
 * or it's an error.  If this is false, the user record will be left in the
 * database even if both 'unlock' and 'clear_history' are true.
 * \param unlock Unlock the account?
 * \param clear_history Clear login failure history for this account?
 */
static int
tbn_bin_reset(sqlite3 *db, const char *username, tbool if_maybe_capital,
              uint32 max_recs, tbool purge, tbool unlock, tbool clear_history)
{
    int err = 0, err2 = 0;
    tstring *sql_ts = NULL;
    const char *sql = NULL;
    sqlite3_stmt *stmt = NULL;
    tbool need_comma = false;
    char *username_trunc = NULL, *username_mapped = NULL;

    bail_null(db);
    bail_require(!purge || (unlock && clear_history));

    if (!unlock && !clear_history) {
        /*
         * Not technically an error, but it is a no-op.
         */
        goto bail;
    }

    err = ts_new(&sql_ts);
    bail_error(err);

    if (max_recs > 0) {
        bail_require(purge);
        bail_require(username == NULL);
        err = tbn_trim_db(db, max_recs);
        bail_error(err);
        goto bail;
    }
    else if (purge) {
        err = ts_append_str(sql_ts, "DELETE FROM " TBN_TABLE_NAME);
        bail_error(err);
    }
    else {
        err = ts_append_str(sql_ts, "UPDATE " TBN_TABLE_NAME " SET ");
        bail_error(err);
        if (unlock) {
            err = ts_append_str(sql_ts, TBN_FIELD_LOCKED " = 0");
            bail_error(err);
            need_comma = true;
        }
        if (clear_history) {
            if (need_comma) {
                err = ts_append_str(sql_ts, ", ");
                bail_error(err);
            }
            err = ts_append_str
                (sql_ts,
                 TBN_FIELD_NUM_FAILURES " = 0, "
                 TBN_FIELD_LAST_FAILURE_TIME " = 0, "
                 TBN_FIELD_LAST_FAILURE_HOST " = '', "
                 TBN_FIELD_LAST_FAILURE_HOST_TRUNCATED " = 0, "
                 TBN_FIELD_LAST_FAILURE_TTY " = ''");
            bail_error(err);
        }
    }

    if (username) {
        err = tbn_string_fixup(username, &username_trunc, NULL,
                               tum_hash, &username_mapped);
        bail_error(err);
        err = ts_append_str
            (sql_ts,
             " WHERE " TBN_FIELD_USERNAME " = " TBN_BPARAM_USERNAME_STR
             " OR " TBN_FIELD_USERNAME " = " TBN_BPARAM_ALT_USERNAME_STR);
        bail_error(err);
    }
    else if (if_maybe_capital) {
        err = ts_append_str
            (sql_ts,
             " WHERE " TBN_FIELD_USERNAME " != lower(" TBN_FIELD_USERNAME ")"
             " OR " TBN_FIELD_USERNAME_MAPPED " != 0");
        bail_error(err);
    }
    else {
        /*
         * No need to add anything here, without a WHERE clause it
         * will match everything.
         */
    }

    sql = ts_str(sql_ts);

    sqlite_utils_finalize(&stmt);
    err = sqlite3_prepare(db, sql, -1, &stmt, NULL);
    bail_error_sqlite(err, db, sql);

    if (username) {
        err = sqlite3_bind_text(stmt, TBN_BPARAM_USERNAME,
                                username_trunc, -1, SQLITE_STATIC);
        bail_error_sqlite(err, db, sql);
        err = sqlite3_bind_text(stmt, TBN_BPARAM_ALT_USERNAME,
                                username_mapped, -1, SQLITE_STATIC);
        bail_error_sqlite(err, db, sql);
    }

    err = sqlite_utils_step(stmt);
    bail_error_sqlite(err, db, sql);

 bail:
    safe_free(username_trunc);
    safe_free(username_mapped);
    sqlite_utils_finalize(&stmt);
    ts_free(&sql_ts);
    return(err);
}


/* ------------------------------------------------------------------------- */
static int
tbn_bin_exec_args(sqlite3 *db)
{
    int err = 0;
    int user_existed = 0, locked_user = 0;
    char *username_mapped = NULL;

    bail_null(db);

    switch (tbn_oper) {
    case tot_list:
        if (tbn_username) {
            lc_log_basic(LOG_DEBUG, "List status of user: %s", tbn_username);
        }
        else {
            lc_log_basic(LOG_DEBUG, "List status of all users");
        }
        err = tbn_bin_list(db, tbn_username);
        bail_error(err);
        break;

    case tot_reset:
        if (tbn_username == NULL && !tbn_all) {
            printf("%% Must specify username to reset, or '-a' to reset all "
                   "users.\n");
            tbn_bin_usage();
        }

        if (tbn_username) {
            lc_log_basic(LOG_DEBUG, "Reset user: %s", tbn_username);
        }
        else {
            lc_log_basic(LOG_DEBUG, "Reset all users");
        }
        err = tbn_bin_reset(db, tbn_username, false, 0, false,
                            !tbn_clear_only, !tbn_unlock_only);
        bail_error(err);
        break;

    case tot_purge:
        if (tbn_username == NULL && !tbn_all && tbn_max_recs == 0 &&
            !tbn_maybe_capital) {
            printf("%% Must specify username to purge, or '-a', or '-C', "
                   "or maximum number of records.\n");
            tbn_bin_usage();
        }
        if (tbn_max_recs > 0 && (tbn_username || tbn_all ||
                                 tbn_maybe_capital)) {
            printf("%% Cannot specify '-M' with '-u', '-a', or '-C'.\n");
            tbn_bin_usage();
        }

        if (tbn_username) {
            lc_log_basic(LOG_DEBUG, "Purge user: %s", tbn_username);
        }
        else if (tbn_all) {
            lc_log_basic(LOG_DEBUG, "Purge all users");
        }
        else if (tbn_maybe_capital) {
            lc_log_basic(LOG_DEBUG, "Purge all users who might contain "
                         "capital letters");
        }
        else {
            lc_log_basic(LOG_DEBUG, "Purge oldest unknown user records to "
                         "leave no more than %" PRIu32 " remaining",
                         tbn_max_recs);
        }
        err = tbn_bin_reset(db, tbn_username, tbn_maybe_capital, tbn_max_recs,
                            true, true, true);
        bail_error(err);
        break;

    case tot_failure:
        if (tbn_username == NULL) {
            printf("%% Must specify username to record login failure.\n");
            tbn_bin_usage();
        }

        if (tbn_hostname) {
            lc_log_basic(LOG_DEBUG, "Record login failure for user '%s' "
                         "from host '%s'", tbn_username, tbn_hostname);
        }
        else if (tbn_tty) {
            /*
             * XXX/EMT: are there any legitimate cases where we'd have
             * both, and want to print both?  Note that in the ssh case,
             * it installs the placeholder string "ssh" into PAM_TTY
             * and we don't want to bother displaying that.
             */
            lc_log_basic(LOG_DEBUG, "Record login failure for user '%s' "
                         "from TTY '%s'", tbn_username, tbn_tty);
        }
        else {
            lc_log_basic(LOG_DEBUG, "Record login failure for user '%s'",
                         tbn_username);
        }
        err = tbn_record_failure(db, tbn_username, tbn_mapping,
                                 !tbn_user_unknown, tbn_hostname, tbn_tty,
                                 0, 0);
        bail_error(err);
        break;

    case tot_lock:
        if (tbn_username == NULL) {
            printf("%% Must specify username to lock out.\n");
            tbn_bin_usage();
        }
        lc_log_basic(LOG_DEBUG, "Lock out user '%s'", tbn_username);

        err = tbn_maybe_lock_user(db, tbn_username, tbn_mapping, 0,
                                  &user_existed, &locked_user);
        bail_error(err);

        if (!user_existed) {
            printf("%% User account did not exist.  Create account first "
                   "with '-f'.\n");
        }
        else if (!locked_user) {
            printf("%% User account was already locked.  No change made.\n");
        }
        break;

    case tot_map:
        if (tbn_username == NULL) {
            printf("%% Must specify username to map.\n");
            tbn_bin_usage();
        }
        err = tbn_string_fixup(tbn_username, NULL, NULL, tbn_mapping,
                               &username_mapped);
        bail_error(err);
        printf("%s\n", username_mapped ? username_mapped : tbn_username);
        break;

    default:
        bail_force(lc_err_unexpected_case);
        break;
    }

 bail:
    safe_free(username_mapped);
    return(err);
}


/* ------------------------------------------------------------------------- */
int
main(int argc, char *argv[])
{
    int err = 0, err2 = 0;
    const char *db_path = NULL;
    tbool exists = false;
    sqlite3 *db = NULL;
    sqlite_utils_transaction_mode xaction_mode = sutm_none;

    err = lc_log_init(TBN_BINARY_NAME, "", LCO_none,
                      LC_COMPONENT_ID(LCI_PAM_TALLYBYNAME),
                      LOG_DEBUG, LOG_LOCAL7, LCT_SYSLOG);
    bail_error(err);

    lc_log_basic(LOG_DEBUG, "%s starting", TBN_BINARY_NAME);

    err = tbn_bin_read_args(argc, argv);
    bail_error(err);

    db_path = tbn_path_override ? tbn_path_override : TBN_DEFAULT_LOGFILE;

    /* XXX/EMT: could offer option to create file */
    err = lf_test_exists(db_path, &exists);
    bail_error_errno(err, "Testing file existence: %s", db_path);
    if (!exists) {
        printf("%% File '%s' does not exist.\n", db_path);
        goto bail;
    }

    switch (tbn_oper) {
    case tot_list:
    case tot_reset:
    case tot_purge:
    case tot_map:
        xaction_mode = sutm_deferred;
        break;

    case tot_failure:
    case tot_lock:
        xaction_mode = sutm_exclusive;
        break;

    default:
        bail_force(lc_err_unexpected_case);
        break;
    }

    err = tbn_open_db_ensure_tables(db_path, 0, xaction_mode, &db);
    if (err == SQLITE_BUSY || err == SQLITE_LOCKED) {
        printf("%% Database was locked, try again later.\n");
        goto bail_no_message;
    }

    err = tbn_bin_exec_args(db);
    bail_error(err);

 bail:
    if (err) {
        printf("%% An error occurred: %s (%d)\n"
               "See logs for details.\n",
               db ? sqlite3_errmsg(db) : "(NULL database)",
               db ? sqlite3_errcode(db) : -1);
    }

 bail_no_message:
    if (db) {
        err2 = sqlite_utils_close(&db, 1);
        complain_error(err2);
    }

    lc_log_basic(err ? LOG_INFO : LOG_DEBUG, "%s finishing (code %d)",
                 TBN_BINARY_NAME, err);
    lc_log_close();
    exit(err);
}
