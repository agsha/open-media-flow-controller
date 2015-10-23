#!/bin/sh

#
# (C) Copyright 2013-2015 Juniper Networks, Inc. All Rights reserved.
# (C) Copyright 2002-2012 Tall Maple Systems, Inc.
# All rights reserved.
# The contents of this file are subject to the terms of the MIT License.
# You may not use this file except in compliance with the License.
# See the file License.txt at the top of this source tree.
# You can obtain a copy of the license at https://opensource.org/licenses/MIT
#

# .............................................................................
# Usage: logrotate_wrapper.sh <logrotate parameters>
#
# This script also looks for "-z <n> <pid>" as its first two parameters.
# 'n' selects which "pass" to run as (0 or 1).  'pid' simply passes the
# pid of the originating logrotate_wrapper.sh shell process, for logging
# purposes.  However, this is only for internal use of the logrotate 
# wrapper script, and should not be used by callers.
#

# .............................................................................
# Bug 14711 notes: logrotate doesn't like finding dates with a year before
# MINYEAR (1996, taken directly from code in logrotate.c) in its status file,
# though it will happily write such a status file if that's how the clock is
# set.  This will keep it from effectively doing automatic rotation as long
# as the date is thus set (except possibly the first time it runs, if there
# wasn't already a "bad" status file to discourage it), and the best we can
# do about that is warn about it.  But if the date was once set that way, 
# and is no longer, we can automatically recover by deleting the old status
# file.
#
# So our plan is:
#
#   * Run logrotate, and check if it failed with an error reading the status
#     file.  If it did not fail, stay silent, even if the year is bad.
#     (We could detect trouble brewing by checking the year here, but it's
#     tricky to phrase the warning at that point, since it may have just
#     automatically rotated the logs this time.  Better to wait until
#     something actually bad happens, and then step in to explain it.)
#
#   * If it DID fail with trouble reading the log file, then:
#      - If the system clock currently has a bad year:
#         - If we haven't already done so, warn them about the bad year,
#           and say the logs will not be automatically rotated as long as
#           this is the case.  Make a note that we have warned them, so we
#           don't repeat the same thing every 10 minutes.
#         - Do NOT remove the logrotate.status file.  This won't help us,
#           and in fact could make it rotate the logs every 10 minutes,
#           which would be bad.
#      - If the system clock currently has a good year:
#         - Remove the marker file for the warning, if it exists.  So if we
#           ever end up later with a bad year again, we'll warn them again.
#         - Remove the logrotate.status file, and log saying that we have
#           done so, just so they know something has happened.
#         - XXX/EMT: we could rerun logrotate at this point.  For now,
#           though, just wait for it to run again naturally.
#

# .............................................................................
# Constants
#
PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

LOGIDENT=logrotate
LOGROTATE_WRAPPER=/usr/sbin/logrotate_wrapper.sh
STATUSFILE=/var/lib/logrotate.status
WARNFILE=/var/lib/logrotate.warned
MINYEAR=1996

#
# Should be set to DEBUG usually, but can change to INFO to get more
# insight into what's going on.
#
DEBUG=DEBUG

# .............................................................................
# strstr(): return 0 iff $1 contains $2
#
strstr() {
    case "${1}" in 
        *${2}*) return 0;;
    esac
    return 1
}

# .............................................................................
# logit(): log a string nicely (using facility 'local6').
#
# Usage: logit <level> <message>
#
# Preset ${LOGIDENT} to be the identifying string to include with each
# message, often the name of the binary or script.
#
# e.g.:
# LOGIDENT=logrotate
# logit INFO "Starting pass ${PASS_VAL}"
#
logit() {
    level=$1
    msg=$2
    LOGGER=/usr/bin/logger
    ${LOGGER} -t "${LOGIDENT}[$$]" -p local6.${level} "[${LOGIDENT}.${level}]: ${msg}"
    return 0
}

# .............................................................................
# Global init
#
umask 0022

PASS_VAL=0

if [ "$1" = "-z" ]; then
    PASS_VAL=$2
    PARENT_PID=$3
    shift 3
fi

# .............................................................................
# Pass 0: take the lock
#
if [ $PASS_VAL -eq 0 ]; then
    logit ${DEBUG} "Outer pass starting, taking file lock"
    /usr/bin/flock /var/lock/logrotate -c "${LOGROTATE_WRAPPER} -z 1 $$ $*"
    logit ${DEBUG} "Outer pass finishing, released file lock"
    logit INFO "Log rotation check completed"
fi

# .............................................................................
# Pass 1: we have the lock, so actually run logrotate
#
if [ $PASS_VAL -eq 1 ]; then
    logit ${DEBUG} "Inner pass starting, running: logrotate $*"

    #
    # Capture both the output and return code of logrotate.  The return
    # code is how we know something went wrong, but the output can be used
    # in limited circumstances to help distinguish different failure cases
    # (which all typically use the same return code).
    #
    # Our handling of failures can go one of two ways:
    #
    #   * Bug 11316: if we find a 'messages.1', that means it failed to
    #     compress it, even though we told it to.  We infer this probably
    #     means it ran out of space, and take steps to deal with that:
    #     make sure we won't delete this "orphaned" log file on the next
    #     rotation.
    #
    #   * Bug 14711 et al.: if we find the string "could not read state file"
    #     in the output, that means it had trouble reading 
    #     /var/lib/logrotate.status, and typically that it aborted at that
    #     point.  So follow our notes under "bug 14711" above.
    #
    output=`/usr/sbin/logrotate $* 2>&1`
    lret=$?

    if [ "${lret}" != "0" ]; then
        echo "Logrotate returned code $lret, output: $output"
        logit INFO "Logrotate returned code $lret, output: $output"

        #
        # Bug 11316 handling.
        #
        if [ -f /var/log/messages.1 ]; then
            mv -f /var/log/messages.1 /var/log/messages
            killall -HUP syslogd
            logit WARNING "Log rotation failed, continuing with old messages file"
        fi

        #
        # Bug 14711 handling.
        #
        # XXX #dep/parse: logrotate
        #
        if strstr "$output" "could not read state file" ; then
            YEAR=`date '+%Y'`
            if [ "${YEAR}" -lt ${MINYEAR} ]; then
                BAD_YEAR=1
                if [ -f ${WARNFILE} ]; then
                    LOGLEVEL=${DEBUG}
                else
                    LOGLEVEL=WARNING
                    touch ${WARNFILE}
                fi
                logit ${LOGLEVEL} "System clock set to unsupported year (${YEAR}), logs will not be automatically rotated"
            else
                BAD_YEAR=0
                rm -f ${WARNFILE}
                rm -f ${STATUSFILE}
                logit WARNING "Unable to read status for automatic log rotation.  Possibly system clock was set wrong (prior to ${MINYEAR}) the last time we checked?  Anyway, year now (${YEAR}) is fine.  Removing status file, hopefully it will work next time."
            fi
        fi
    else
        logit ${DEBUG} "Logrotate succeeded"
    fi
    logit ${DEBUG} "Inner pass finishing"
fi
