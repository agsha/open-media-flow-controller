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
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <syslog.h>
#include "tms-ssh-utils.h"
#include "openbsd-compat.h"
#include "customer.h"
#include "ttime.h"
#include "tpaths.h"

#ifdef PROD_FEATURE_RESTRICT_CMDS
#ifndef ssh_restrict_rules_NONE

/* ------------------------------------------------------------------------- */
/** Select how a path is to be interpreted, in terms of restricting
 * access to files or directories.
 */
typedef enum {
    tamt_none = 0,

    /** 
     * The filename for this rule is the exact name of a single file.
     */
    tamt_file = 1 << 0,

    /**
     * The filename for this rule is the exact name of a single
     * directory, and matches the directory itself as well as all of
     * its contents (recursively).
     */
    tamt_dir = 1 << 1,

    /**
     * The filename for this rule is the exact name of a single
     * directory, and matches only the directory itself and the
     * files which it directly contains.  It does NOT match any
     * directories underneath.
     *
     * NOTE: the path string used with this directive MUST end in a
     * '/' character.  e.g. the root filesystem would be "/", while
     * /usr would be "/usr/".
     */
    tamt_dir_single = 1 << 2,

    /**
     * The filename for this rule is a prefix, and matches any full
     * paths which have this string as a prefix.  Note that this is
     * not quite the same as tamt_dir; e.g. a rule with "/opt/foo"
     * will match a request for "/opt/foobar" if tamt_prefix is used,
     * but not with tamt_dir.
     */
    tamt_prefix = 1 << 3,

    /*
     * XXX/EMT: could support regexes, etc.
     */

} tms_access_match_type;

#if 0
static const lc_enum_string_map tms_access_match_type_map[] = {
    { tamt_none, "none" },
    { tamt_file, "file" },
    { tamt_dir, "dir" },
    { tamt_dir_single, "dir_single" },
    { tamt_prefix, "prefix" },
    { 0, NULL }
};
#endif


/* ------------------------------------------------------------------------- */
/** Flags which can be applied to a single rule.
 *
 * (No flags are defined currently; this is defined to provide for
 * future extensibility.)
 */
typedef enum {
    tamf_none = 0,
} tms_access_match_flags;

/* Bit field of tms_access_match_flags ORed together */
typedef unsigned int tms_access_match_flags_bf;


/* ------------------------------------------------------------------------- */
/** Specify an action to take if a rule matches the path in the access
 * request.
 */
typedef enum {
    tama_none = 0,
    tama_allow,
    tama_deny,
} tms_access_match_action;

#if 0
static const lc_enum_string_map tms_access_match_action_map[] = {
    { tama_none, "none" },
    { tama_allow, "allow" },
    { tama_deny, "deny" },
    { 0, NULL }
};
#endif


/* ------------------------------------------------------------------------- */
/** Structure to express one rule about what files we permit to be
 * accessed via scp and sftp.
 */
typedef struct {
    /**
     * Which operation(s) does this rule apply to? 
     *
     * NOTE: for now, this MUST be set to tfot_all, because more
     * specific restrictions cannot be reliably enforced.  (Bug 12347)
     * Rules set to anything else will generate a logged warning, and
     * will be ignored.
     */
    tms_file_oper_type_bf tar_opers;

    /**
     * How do we interpret tar_path for purposes of matching against 
     * files we are considering operating on?
     */
    tms_access_match_type tar_match_type;

    /**
     * Option flags to modify our matching.
     */ 
    tms_access_match_flags_bf tar_match_flags;

    /**
     * What should we do with the access request, if this rule matches?
     */
    tms_access_match_action tar_match_action;

    /**
     * Absolute path of file(s) or directory(ies) to which this rule
     * applies.  See tar_match_type for how to interpret this.
     */
    const char *tar_path;

} tms_access_rule;


/* ------------------------------------------------------------------------- */
/* We provide three default sets of matching rules:
 *   1. tms_access_rules_whitelist: extensible list of dirs/files to permit.
 *   2. tms_access_rules_blacklist: extensible list of dirs/files to deny.
 *   3. tms_access_rules_custom: fully customer-defined rule set.
 *
 * The customer selects which one of these to use by #defining
 * ssh_restrict_rules in customer.h.  Or, they can opt out of applying
 * any rules by #defining ssh_restrict_rules_NONE.
 * 
 * To determine what to do with a file, we scan through the list in
 * order.  If a rule matches, change the action to what that rule
 * says.  Scan the entire list, even if we get a match partway
 * through.  Thus a rule can override the decision made by one that
 * came before it.  So usually more general rules come at the top,
 * followed by more specific rules that partially override them.
 *
 * Every rule set must explicitly make a decision on every file given
 * to it.  If a rule set does not decide, the default action is to
 * allow access, but a warning will be logged to encourage the
 * developer to explicitly declare a default (a rule at the top that
 * will match everything).
 */


/* ------------------------------------------------------------------------- */
/* Rule set for a "whitelist" approach, where the default action is to
 * deny access, unless the file or directory is explicitly called out
 * as allowed.
 *
 * NOTE: do not allow access to /var/opt/tms/output, because there are
 * ways a user could bypass our security if they could write to it, 
 * e.g. setting the restricted_cmds_license symlink to point to "1".
 */
tms_access_rule tms_access_rules_whitelist[] = {
    {tfot_all, tamt_prefix, 0, tama_deny,  ""},
    {tfot_all, tamt_dir,    0, tama_allow, "/config/db"},
    {tfot_all, tamt_dir,    0, tama_allow, "/config/text"},
    {tfot_all, tamt_dir,    0, tama_allow, "/var/home"},
    {tfot_all, tamt_dir,    0, tama_allow, "/var/log"},
    {tfot_all, tamt_file,   0, tama_deny,  "/var/log/systemlog"},
    {tfot_all, tamt_dir,    0, tama_allow, "/var/opt/tms/images"},
    {tfot_all, tamt_dir,    0, tama_allow, "/var/opt/tms/snapshots"},
    {tfot_all, tamt_dir,    0, tama_allow, "/var/opt/tms/stats"},
    {tfot_all, tamt_dir,    0, tama_allow, "/var/opt/tms/sysdumps"},
    {tfot_all, tamt_dir,    0, tama_allow, "/var/opt/tms/tcpdumps"},
    {tfot_all, tamt_dir,    0, tama_allow, "/var/opt/tms/text_configs"},
    {tfot_all, tamt_dir,    0, tama_allow, "/var/opt/tms/virt/pools"},
    {tfot_all, tamt_dir,    0, tama_allow, "/var/opt/tms/virt/vm_save"},
    {tfot_all, tamt_file,   0, tama_allow, "/etc/build_version.txt"},
    {tfot_all, tamt_dir,    0, tama_allow, "/usr/share/snmp/mibs"},

/* ========================================================================= */
/* Customer-specific graft point 1: whitelist additions.  Use this
 * graft point to add rules to this rule set.  You can specify more
 * whitelisted directories and files, and/or override some of our
 * rules above.
 * =========================================================================
 */
#ifdef INC_OPENSSH_TMS_SSH_UTILS_GRAFT_POINT
#undef OPENSSH_TMS_SSH_UTILS_GRAFT_POINT
#define OPENSSH_TMS_SSH_UTILS_GRAFT_POINT 1
#include "../base_os/common/openssh/src/tms-ssh-utils.inc.c"
#endif /* INC_OPENSSH_TMS_SSH_UTILS_GRAFT_POINT */

    {0, 0, 0, 0, NULL}
};


/* ------------------------------------------------------------------------- */
/* Rule set for a "blacklist" approach, where the default action is to 
 * allow access, unless the file or directory is explicitly called out
 * as denied.
 *
 * NOTE: do not allow access to /var/opt/tms/output, because there are
 * ways a user could bypass our security if they could write to it, 
 * e.g. setting the restricted_cmds_license symlink to point to "1".
 */
tms_access_rule tms_access_rules_blacklist[] = {
    {tfot_all, tamt_prefix,     0, tama_allow, ""},
    {tfot_all, tamt_dir_single, 0, tama_deny,  "/"},
    {tfot_all, tamt_dir,        0, tama_deny,  "/vtmp"},
    {tfot_all, tamt_dir,        0, tama_deny,  "/config/mfg"},
    {tfot_all, tamt_dir,        0, tama_deny,  "/var/cache"},
    {tfot_all, tamt_dir,        0, tama_deny,  "/var/empty"},
    {tfot_all, tamt_dir,        0, tama_deny,  "/var/lib"},
    {tfot_all, tamt_dir,        0, tama_deny,  "/var/lock"},
    {tfot_all, tamt_file,       0, tama_deny,  "/var/log/systemlog"},
    {tfot_all, tamt_dir,        0, tama_deny,  "/var/net-snmp"},
    {tfot_all, tamt_dir,        0, tama_deny,  "/var/opt/tms/cmc"},
    {tfot_all, tamt_dir,        0, tama_deny,  "/var/opt/tms/output"},
    {tfot_all, tamt_dir,        0, tama_deny,  "/var/opt/tms/sched"},
    {tfot_all, tamt_dir,        0, tama_deny,  "/var/opt/tms/web"},
    {tfot_all, tamt_dir,        0, tama_deny,  "/var/racoon"},
    {tfot_all, tamt_dir,        0, tama_deny,  "/var/root"},
    {tfot_all, tamt_dir,        0, tama_deny,  "/var/run"},
    {tfot_all, tamt_dir,        0, tama_deny,  "/var/spool"},
    {tfot_all, tamt_dir,        0, tama_deny,  "/var/stmp"},
    {tfot_all, tamt_dir,        0, tama_deny,  "/var/tmp"},
    {tfot_all, tamt_dir,        0, tama_deny,  "/var/var_version.sh"},
    {tfot_all, tamt_dir,        0, tama_deny,  "/bin"},
    {tfot_all, tamt_dir,        0, tama_deny,  "/boot"},
    {tfot_all, tamt_dir,        0, tama_deny,  "/bootmgr"},
    {tfot_all, tamt_dir,        0, tama_deny,  "/dev"},
    {tfot_all, tamt_dir,        0, tama_deny,  "/etc"},
    {tfot_all, tamt_file,       0, tama_allow, "/etc/build_version.txt"},
    {tfot_all, tamt_dir,        0, tama_deny,  "/initrd"},
    {tfot_all, tamt_dir,        0, tama_deny,  "/lib"},
    {tfot_all, tamt_dir,        0, tama_deny,  "/lib64"},
    {tfot_all, tamt_dir,        0, tama_deny,  "/mnt"},
    {tfot_all, tamt_dir,        0, tama_deny,  "/opt"},
    {tfot_all, tamt_dir,        0, tama_deny,  "/proc"},
    {tfot_all, tamt_dir,        0, tama_deny,  "/sbin"},
    {tfot_all, tamt_dir,        0, tama_deny,  "/sys"},
    {tfot_all, tamt_dir,        0, tama_deny,  "/usr"},
    {tfot_all, tamt_dir,        0, tama_allow, "/usr/share/snmp/mibs"},

/* ========================================================================= */
/* Customer-specific graft point 2: blacklist additions.  Use this
 * graft point to add rules to this rule set.  You can specify more
 * blacklisted directories and files, and/or override some of our
 * rules above.
 * =========================================================================
 */
#ifdef INC_OPENSSH_TMS_SSH_UTILS_GRAFT_POINT
#undef OPENSSH_TMS_SSH_UTILS_GRAFT_POINT
#define OPENSSH_TMS_SSH_UTILS_GRAFT_POINT 2
#include "../base_os/common/openssh/src/tms-ssh-utils.inc.c"
#endif /* INC_OPENSSH_TMS_SSH_UTILS_GRAFT_POINT */

    {0, 0, 0, 0, NULL}
};


/* ------------------------------------------------------------------------- */
/* Fully custom rule set.  Starts empty, for the customer who wants to
 * start with a "blank slate".
 *
 * Please see warnings under "System lockdown options" in
 * customer/demo/src/include/customer.h, since by writing your own
 * rule set, you may weaken the security of the system.
 *
 * NOTE: to use this, you MUST define at least one rule using the graft
 * point; see comments below.
 */
tms_access_rule tms_access_rules_custom[] = {

/* ========================================================================= */
/* Customer-specific graft point 3: empty rule set additions.  Use this
 * graft point to add rules to this rule set.
 *
 * NOTE: a warning will be logged if your rule set does not produce a
 * decision for any file or directory the user tries to access.  The
 * default action in this case is to permit access, but we want this
 * decision to be made explicitly by the rule set.  The standard way
 * to do this is to have a catch-all rule at the top, which will match
 * any access request, even if none of the other rules below do.
 * Choose one of the two rules below, to do "default deny", or
 * "default allow", respectively:
 *
 *   {tfot_all, tamt_prefix, 0, tama_deny,  ""},
 *   {tfot_all, tamt_prefix, 0, tama_allow, ""},
 * =========================================================================
 */
#ifdef INC_OPENSSH_TMS_SSH_UTILS_GRAFT_POINT
#undef OPENSSH_TMS_SSH_UTILS_GRAFT_POINT
#define OPENSSH_TMS_SSH_UTILS_GRAFT_POINT 3
#include "../base_os/common/openssh/src/tms-ssh-utils.inc.c"
#endif /* INC_OPENSSH_TMS_SSH_UTILS_GRAFT_POINT */

    {0, 0, 0, 0, NULL}
};


/* ------------------------------------------------------------------------- */
static int
tms_path_check_ok_one(const char *abspath, tms_file_oper_type oper)
{
    int ok = 0;
    tms_access_rule *rules = NULL, *rule = NULL;
    int i = 0, match = 0;
    int ap_len = 0, rule_len = 0, path_len = 0;
    char *last_slash = NULL;

    if (abspath == NULL) {
        goto bail;
    }
    
    syslog(LOG_DEBUG, "Checking if oper %d OK for file %s", oper, abspath);

#ifndef ssh_restrict_rules
#error Must define ssh_restrict_rules in customer.h
#endif

    rules = ssh_restrict_rules;
    ok = -1; /* Undetermined */

    for (i = 0; rules[i].tar_path; ++i) {
        rule = &(rules[i]);

        if (rule == NULL) {
            continue; /* Should not happen */
        }

        /*
         * XXX/EMT: bug 12347.
         */
        if (rule->tar_opers != tfot_all) {
            syslog(LOG_ERR, "Rule for path %s does not have required "
                   "tar_opers == tfot_all: ignoring", rule->tar_path);
            continue;
        }

        if ((rule->tar_opers & oper) == 0) {
            continue; /* This rule does not apply to this operation */
        }

        match = 0;

        switch (rule->tar_match_type) {

        case tamt_file:
            if (!strcmp(abspath, rule->tar_path)) {
                match = 1;
            }
            break;

        case tamt_dir:
            if (!strcmp(abspath, rule->tar_path)) {
                match = 1;
            }
            ap_len = strlen(abspath);
            rule_len = strlen(rule->tar_path);
            if (ap_len > rule_len &&
                strstr(abspath, rule->tar_path) == abspath &&
                abspath[rule_len] == '/') {
                match = 1;
            }
            break;

        case tamt_dir_single:
            last_slash = strrchr(abspath, '/');
            if (last_slash) {
                rule_len = strlen(rule->tar_path);
                path_len = last_slash - abspath + 1;
                if (rule_len == path_len &&
                    !strncmp(abspath, rule->tar_path, rule_len)) {
                    match = 1;
                }
            }
            break;

        case tamt_prefix:
            ap_len = strlen(abspath);
            rule_len = strlen(rule->tar_path);
            if (ap_len >= rule_len &&
                strstr(abspath, rule->tar_path) == abspath) {
                match = 1;
            }
            break;
       
        default:
            syslog(LOG_ERR, "Invalid value for tar_match_type in rule for "
                   "%s: %d", rule->tar_path, rule->tar_match_type);
            break;
        }

        if (match) {
            syslog
                (LOG_DEBUG, 
                 "Rule %d matched: oper %d; match type %d; "
                 "match action %d; path \"%s\"", i, rule->tar_opers,
                 rule->tar_match_type, rule->tar_match_action, rule->tar_path);

            ok = (rule->tar_match_action == tama_allow) ? 1 : 0;
        }
    }

    if (ok < 0) {
        syslog(LOG_WARNING, "Nothing in rule set matched file %s.  "
               "Rule set should be able to match anything!  "
               "Allowing access.", abspath);
        ok = 1;
    }

    if (ok == 0) {
        syslog(LOG_INFO, "Denying %s access to %s", (oper == tfot_read) ?
               "read" : "write", abspath);
    }
    else {
        syslog(LOG_DEBUG, "Permitting %s access to %s", (oper == tfot_read) ?
               "read" : "write", abspath);
    }

 bail:
    return(ok);
}


/* ------------------------------------------------------------------------- */
static int
tms_path_check_license(tbool *ret_licensed)
{
    int err = 0;
    int retv = 0;
    tbool licensed = false;
    char rlbuf[PATH_MAX];
    int new_value = 0;

    if (ret_licensed == NULL) {
        goto bail;
    }

    /*
     * This is not a long-lived process, and in most cases, we'll only
     * be called once during its short lifetime.  So optimize for
     * obvious correctness instead of efficiency (i.e. don't try to
     * avoid rereading the file too frequently, etc.)
     */

    rlbuf[0] = '\0';
    errno = 0;
    retv = readlink(FILE_RESTRICTED_CMDS_LICENSE_PATH, rlbuf, 
                    sizeof(rlbuf) - 1);
    if (retv >= 0) {
        rlbuf[retv] = '\0';
        new_value = atoi(rlbuf);
        licensed = (new_value != 0);
    }
    else {
        syslog(LOG_WARNING, "Error reading from %s: %m",
               FILE_RESTRICTED_CMDS_LICENSE_PATH);
        licensed = false;
    }

    syslog(LOG_DEBUG, "%s license present: %s",
           RESTRICTED_CMDS_LICENSED_FEATURE_NAME,
           licensed ? "yes" : "no");

 bail:
    if (ret_licensed) {
        *ret_licensed = licensed;
    }
    return(err);
}


/* ------------------------------------------------------------------------- */
int
tms_path_check_ok(const char *path, tms_file_oper_type_bf opers)
{
    int err = 0;
    int ok = 1;
    char abspath[PATH_MAX + 1];
    char *result = NULL;
    struct stat st_info;
    tbool licensed = false;

    memset(&st_info, 0, sizeof(st_info));

    if (path == NULL) {
        goto bail;
    }

    err = tms_path_check_license(&licensed);
    if (err) {
        syslog(LOG_WARNING, "Error checking for %s license; "
               "assuming no license present",
               RESTRICTED_CMDS_LICENSED_FEATURE_NAME);

        /* Default to not allowing access */
        licensed = false;
    }
   
    if (licensed) {
        syslog(LOG_DEBUG, "TMS_SSH: allowing access because license "
               "installed");
        goto bail;
    }

    errno = 0;
    abspath[0] = '\0';

    result = openbsd_realpath(path, abspath);
    if (result == NULL) {
        /*
         * This could happen because the user named a file in a directory
         * that doesn't exist.  Other standard mechanisms will catch this
         * and deny access, so we don't have to get involved.
         */
        syslog(LOG_DEBUG, "Unable to resolve file path %s.  Allowing access.",
               path);
        goto bail;
    }
    else {
        syslog(LOG_DEBUG, "Resolved path \"%s\" to \"%s\"", path, abspath);
    }

    err = stat(abspath, &st_info);
    if (err) {
        err = 0;
        /*
         * This could happen because a file is about to be created, or
         * because the user doesn't have filesystem permissions.
         * That's OK, we didn't really need the stats anyway, so we'll
         * still apply our normal rules.
         */
        syslog(LOG_DEBUG, "Unable to stat file path %s.  Continuing with "
               "access checks...", abspath);
    }
    else if (st_info.st_nlink > 1) {
        /*
         * Log this info, but not sure what we can do about it, so
         * essentially we're ignoring it.
         */
        syslog(LOG_DEBUG, "File %s has %d hard links", abspath,
               (int)(st_info.st_nlink));
    }

    if ((opers & tfot_read) != 0) {
        ok &= tms_path_check_ok_one(abspath, tfot_read);
    }

    if ((opers & tfot_write) != 0) {
        ok &= tms_path_check_ok_one(abspath, tfot_write);
    }

 bail:
    return(ok);
}

#endif /* ssh_restrict_rules_NONE */
#endif /* PROD_FEATURE_RESTRICT_CMDS */
