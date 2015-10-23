#!/bin/sh
#
# (C) Copyright 2013-2015 Juniper Networks, Inc. All Rights reserved.
# (C) Copyright 2002-2011 Tall Maple Systems, Inc.
# All rights reserved.
# The contents of this file are subject to the terms of the MIT License.
# You may not use this file except in compliance with the License.
# See the file License.txt at the top of this source tree.
# You can obtain a copy of the license at https://opensource.org/licenses/MIT
#

# This file is responsible for starting the crypto IKE daemon, racoon.
# 
# We wrap the execution of racoon in order to ensure that the policy and SA
# databases have been flushed of any stale data and saved configuration has
# been restored configuration prior to racoon startup.

PROG_DESCR="racoon startup"
VERBOSE=3
VERBOSE_ARGS=

LOG_COMPONENT="racoon_startup"
LOG_TAG="${LOG_COMPONENT}[$$]"
# LOG_LEVEL is uppercase, facility lower
LOG_FACILITY=user
LOG_LEVEL=NOTICE
LOG_OUTPUT=0
LOG_FILE=

PLUTO_WAIT_FILE='/var/run/pluto/ipsec.plutowait'

# =======================================================
# Utility Functions
# =======================================================

# =====================================================
# Based on verboseness setting suppress command output
# =====================================================
do_verbose()
{
    if [ ${VERBOSE} -gt 0 ]; then
        $*
    else
        $* > /dev/null 2>&1
    fi
}

# ==================================================
# Echo or not based on VERBOSE setting
# ==================================================
vecho()
{
    level=$1
    shift

    if [ ${VERBOSE} -gt ${level} ]; then
        echo "$*"
    fi
}

# ==================================================
# Echo and log or not based on VERBOSE setting
# ==================================================
vlecho()
{
    level=$1
    shift
    local log_level=${LOG_LEVEL:-NOTICE}

    if [ ${VERBOSE:-0} -gt ${level} ]; then
        echo "$*"
        logger -p ${LOG_FACILITY:-user}.${log_level} -t "${LOG_TAG:-${0}[$$]}" "[${LOG_COMPONENT:-${0}}.${log_level}]: $*"
        if [ ${LOG_OUTPUT} -ne 0 -a ! -z "${LOG_FILE}" ]; then
            echo "$*" >> ${LOG_FILE}
        fi
    fi
}

# =========================================================
# Log or not at the stated level, based on VERBOSE setting
# =========================================================
vlog()
{
    level=$1
    shift
    local log_level=${1:-${LOG_LEVEL}}
    shift

    if [ ${VERBOSE:-0} -gt ${level} ]; then
        logger -p ${LOG_FACILITY:-user}.${log_level} -t "${LOG_TAG:-${0}[$$]}" "[${LOG_COMPONENT:-${0}}.${log_level}]: $*"
        if [ ${LOG_OUTPUT} -ne 0 -a ! -z "${LOG_FILE}" ]; then
            echo "$*" >> ${LOG_FILE}
        fi
    fi
}

# ======================================================
# Echo and log an Error or not based on VERBOSE setting
# ======================================================
vlechoerr()
{
    level=$1
    shift
    local log_level=ERR

    if [ ${VERBOSE:-0} -gt ${level} ]; then
        echo "$*"
        logger -p ${LOG_FACILITY:-user}.${log_level} -t "${LOG_TAG:-${0}[$$]}" "[${LOG_COMPONENT:-${0}}.${log_level}]: $*"
        if [ ${LOG_OUTPUT} -ne 0 -a ! -z "${LOG_FILE}" ]; then
            echo "$*" >> ${LOG_FILE}
        fi
    fi
}

# ==================================================
# Cleanup when we are finished or interrupted
# ==================================================
cleanup()
{
    # Nothing to do    
    echo "" >>/dev/null
}

# ==================================================
# Cleanup and exit on fatal error
# ==================================================
cleanup_exit_failure()
{
    cleanup
    
    local msg=$1 END_TIME=$(date "+%Y%m%d-%H%M%S")

    vlechoerr -1 "====== Aborting $PROG_DESCR with error at ${END_TIME}: $msg"

    exit 1
}

# ==================================================
# Cleanup when called from 'trap' for ^C or signal
# ==================================================

trap_cleanup_exit()
{
    cleanup_exit_failure  "$PROG_DESCR interrupted:  exiting"

    exit 1
}

# =====================================================
# Crypto database restore and racoon exec
# =====================================================

# We don't currently support wrapper-specific command line options.  All
# command line parameters are passed directly through to racoon on exec.

# We're ready to do real work, so setup environment
trap "trap_cleanup_exit" HUP INT QUIT PIPE TERM
shopt -s execfail

RACOON_PATH='/usr/sbin/racoon'
RACOON_CONF='/var/opt/tms/output/racoon/racoon.conf'
SETKEY_PATH='/sbin/setkey'
SETKEY_CONF='/var/opt/tms/output/setkey.conf'

check_setkey()
{

    if [ ! -x "$SETKEY_PATH" ]; then
        cleanup_exit_failure "Crypto configuration tool '$SETKEY_PATH' missing or not executable!"
    fi

    if [ ! -f "$SETKEY_CONF" ];  then
        cleanup_exit_failure "Crypto policy and SA config file '$SETKEY_CONF' is missing!"
    fi
}

check_racoon()
{
    if [ ! -x "$RACOON_PATH" ]; then
        cleanup_exit_failure "Crypto IKE daemon '$SETKEY_PATH' missing or not executable!"
    fi

    if [ ! -f "$RACOON_CONF" ];  then
        cleanup_exit_failure "Crypto IKE config file '$RACOON_CONF' is missing!"
    fi
}

setkey_restore()
{
    local setkey_err_msg="" errnum=0

    check_setkey

    setkey_err_msg=$($SETKEY_PATH -f $SETKEY_CONF 2>&1)
    errnum=$?

    if [ $errnum -ne 0 ];  then
        if [ $errnum -eq 1 ];  then
            # setkey error, most likely an illegal configuration entry
            vlechoerr -1 "Crypto policy / SA DB restore error: '$setkey_err_msg'"
        else
            vlechoerr -1 "Unexpected error code $errnum executing '$SETKEY_PATH -f $SETKEY_CONF', message: '$setkey_err_msg'"
        fi
    fi

    return $errnum
}

check_wait_pluto()
{
    local errnum=0 waitfile='' pluto_pid='' wait_ticks=0 max_ticks=0

    pluto_pid=`pidof -xs _plutorun`
    # /pm/process/ipsec/kill_timeout == 10
    max_ticks=`expr 4 \* 32`

    while
        kill -0 $pluto_pid >/dev/null 2>&1
    do
        if [ "$wait_ticks" -eq 1 ];  then
            vlecho 1 "Waiting for pluto to terminate..."
        fi
        usleep 250000
        wait_ticks=`expr $wait_ticks + 1`
        if [ $wait_ticks -gt $max_ticks ];  then
            errnum=1
            break;
        fi
    done

    while
        [ -f $PLUTO_WAIT_FILE ]
    do
        if [ "$wait_ticks" -eq 1 ];  then
            vlecho 1 "Waiting for ipsec_startup.sh to pluto clean up..."
        fi
        usleep 250000
        wait_ticks=`expr $wait_ticks + 1`
        if [ $wait_ticks -gt $max_ticks ];  then
            errnum=1
            break;
        fi
    done

    return $errnum
}

# Flush and reload the policy and SA database from setkey.conf
# Note that setkey.conf may have it's own flush directives that are redundant,
# but we need to be sure the database is clean.

check_wait_pluto || cleanup_exit_failure "Timed out waiting for pluto daemon to die"
check_racoon
check_setkey

vlecho 1 "Restoring crypto policy and SA database"

do_verbose $SETKEY_PATH -FP || cleanup_exit_failure "Failure flushing crypto policy database"
vlecho 2 "Flushed crypto policy database"

do_verbose $SETKEY_PATH -F || cleanup_exit_failure "Failure flushing crypto SA database"
vlecho 2 "Flushed crypto SA database"

do_verbose setkey_restore || cleanup_exit_failure "Failure restoring policy and SA databases"
vlecho 2 "Restored crypto policy and SA databases"

# Exec racoon in the foreground (-F)
vlecho 1 "Starting racoon"
vlecho 2 "exec $RACOON_PATH $@"
exec $RACOON_PATH "$@"

# We shouldn't be here unless exec failed
END_TIME=$(date "+%Y%m%d-%H%M%S")
vlecho -1 "====== Ending $PROG_DESCR due to racoon exec failure at ${END_TIME}"

exit 1
