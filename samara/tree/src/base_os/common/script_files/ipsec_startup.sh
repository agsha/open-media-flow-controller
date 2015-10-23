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

# This file is responsible for starting the crypto IKE daemon (pluto).
# 
# We wrap the execution of ipsec in order to ensure that the policy and SA
# databases have been flushed of any stale data and saved configuration has
# been restored configuration prior to ipsec startup.
#
# This script replaces the Openswan distribution script /etc/init.d/ipsec
# We don't currently support wrapper-specific command line options.

test $IPSEC_STARTUP_DEBUG && set -v -x
#Should we chain the script debug flag?
#export IPSEC_INIT_SCRIPT_DEBUG=IPSEC_STARTUP_DEBUG

PROG_DESCR="Crypto IPsec"
VERBOSE=3
VERBOSE_ARGS=

LOG_COMPONENT="ipsec_startup"
LOG_TAG="${LOG_COMPONENT}[$$]"
# LOG_LEVEL is uppercase, facility lower
LOG_FACILITY=user
LOG_LEVEL=NOTICE
LOG_OUTPUT=0
LOG_FILE=

IPSEC_CONF='/var/opt/tms/output/pluto/ipsec.conf'
SETKEY_PATH='/sbin/setkey'
SETKEY_CONF='/var/opt/tms/output/setkey.conf'
PLUTO_WAIT_FILE='/var/run/pluto/ipsec.plutowait'

# =======================================================
# Stuff adapted from /etc/init.d/ipsec source
# =======================================================
IPSEC_EXECDIR="${IPSEC_EXECDIR-/usr/libexec/ipsec}"
IPSEC_LIBDIR="${IPSEC_LIBDIR-/usr/libexec/ipsec}"
IPSEC_SBINDIR="${IPSEC_SBINDIR-/usr/sbin}"
IPSEC_CONFS="${IPSEC_CONFS-/etc}"

PATH="${IPSEC_SBINDIR}":/sbin:/usr/sbin:/usr/local/bin:/bin:/usr/bin
export PATH

IPSEC_DIR="$IPSEC_LIBDIR"
export IPSEC_DIR IPSEC_CONFS IPSEC_LIBDIR IPSEC_EXECDIR

# =======================================================
# END: Stuff adapted from /etc/init.d/ipsec source
# =======================================================

# This is file is defined in ipesec _realsetup (lock=...)
runlock="/var/run/pluto/ipsec_setup.pid"

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

# ==================================================
# Echo and log or not based on VERBOSE setting
# but at the specified log level
# ==================================================
vlecho_log_level()
{
    local level=$1 log_level=$2
    shift; shift

    if [ ${VERBOSE:-0} -gt ${level} ]; then
        echo "$*"
        logger -p ${LOG_FACILITY:user}.${log_level} -t "${LOG_TAG:-${0}[$$]}" "[${LOG_COMPONENT:-${0}}.${log_level}]: $*"
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
    rm -f $PLUTO_WAIT_FILE
}

# ==================================================
# Cleanup and exit on fatal error
# ==================================================
cleanup_exit_failure()
{
    local msg=$1 errnum=$2 END_TIME=$(date "+%Y%m%d-%H%M%S")

    cleanup

    if [ -z "$errnum" ];  then
        errnum=1
    fi

    vlechoerr -1 "====== Aborting $PROG_DESCR with error at ${END_TIME}: $msg"

    exit $errnum
}

# ==================================================
# Cleanup and exit on termination signal
# ==================================================
cleanup_exit_terminate()
{
    local msg=$1 errnum=$2 END_TIME=$(date "+%Y%m%d-%H%M%S")

    cleanup
    
    if [ -z "$errnum" ];  then
        errnum=0
    fi

    vlecho_log_level -1 INFO "====== $PROG_DESCR stopped ipsec at ${END_TIME} with status $errnum: $msg"

    exit $errnum
}

# ==================================================
# Cleanup when called from 'trap' for ^C or signal
# ==================================================

trap_cleanup_exit()
{
    local signal=$1 retval=0 how="cleanly"
    vlecho 1 "caught signal $signal"
    stop_ipsec
    retval=$?
    if [ $retval -ne 0 ];  then
        how="with pluto shutdown errors"
    fi
    # XXX/SML: Pluto's stop script logic in _realsetup goes to great length to
    # try to ensure death of pluto and proper cleanup, including a last resort
    # kill of the known pluto pids from the last run, but we'll do a catchall
    # last ditch attempt here in case there's some extraneous pluto process
    # hanging around.
    killall -s KILL pluto 2>/dev/null && cleanup_exit_failure "$PROG_DESCR failed attempt to killall ipsec"
    cleanup_exit_terminate "$PROG_DESCR terminated $how on signal $signal" $retval
    exit $retval
}

trap_sigquit()
{
    trap_cleanup_exit SIGQUIT
}

trap_sigterm()
{
    trap_cleanup_exit SIGTERM
}

trap_sigint()
{
    trap_cleanup_exit SIGINT
}

trap_sigpipe()
{
    trap_cleanup_exit SIGPIPE
}

trap_sighup()
{
    # XXX/SML: maybe reread config?  Who would send this if not a shell user?
    vlecho -1 "Ignoring SIGHUP"
}

# =======================================================
# IPsec Functions
# =======================================================

# =====================================================
# Crypto database restore and ipsec exec
# =====================================================

check_setkey_sanity()
{

    if [ ! -x "$SETKEY_PATH" ]; then
        cleanup_exit_failure "Crypto configuration tool '$SETKEY_PATH' missing or not executable!"
    fi

    if [ ! -f "$SETKEY_CONF" ];  then
        cleanup_exit_failure "Crypto policy and SA config file '$SETKEY_CONF' is missing!"
    fi
}

setkey_restore()
{
    local setkey_err_msg="" errnum=0

    check_setkey_sanity

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

# =======================================================
# Stuff adapted from /etc/init.d/ipsec source
# =======================================================

check_ipsec_sanity()
{
    if [ `id -u` -ne 0 ]
    then
        cleanup_exit_failure "permission denied (must be superuser)"
    fi

    # Does not make any sense at all to continue without the main binary
    test -x $IPSEC_SBINDIR/ipsec || cleanup_exit_failure "Crypto IPsec main ipsec binary missing or not executable!"
}

verify_ipsec_config()
{
    local retval=0 config_error=""
    test -f $IPSEC_CONFS/ipsec.conf || cleanup_exit_failure "Crypto IPsec main config file missing"

    config_error=`ipsec addconn --checkconfig 2>&1`
    retval=$?
    if [ $retval != 0 ]
    then
        cleanup_exit_failure "Openswan IKE daemon configuration error: $config_error" $retval
    fi
}

check_wait_racoon()
{
    local errnum=0 waitfile='' pluto_pid='' wait_ticks=0 max_ticks=0

    pluto_pid=`pidof racoon`
    # /pm/process/racoon/kill_timeout == 1, always with SIGKILL
    # since racoon resists shutdown.
    max_ticks=`expr 4 \* 3`

    while
        kill -0 $pluto_pid >/dev/null 2>&1
    do
        if [ "$wait_ticks" -eq 1 ];  then
            vlecho_log_level 1 INFO "Waiting for racoon to terminate..."
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

start_ipsec()
{
    local retval=0
    verify_ipsec_config

    # misc setup
    umask 022
    mkdir -p /var/run/pluto || cleanup_exit_failure "Failure creating directory /var/run/pluto"

    # Pick up IPsec configuration (until we have done this, successfully, we
    # do not know where errors should go, hence the explicit "daemon.error"s.)
    # Note the "--export", which exports the variables created.
    # XXX #dep/parse: ipsec  
    variables=`ipsec addconn $IPSEC_CONFS/ipsec.conf --varprefix IPSEC --configsetup`
    if [ $? != 0 ]
        then
        cleanup_exit_failure "Failure parsing config setup portion of ipsec.conf" $?
    fi
    eval $variables
    
    export IPSEC_confreadsection=${IPSEC_confreadsection:-setup}
    export IPSECsyslog=${IPSECsyslog:-daemon.error}
    export IPSECplutowait="yes"
    export IPSECplutorestartoncrash="no"

    vlecho_log_level 1 NOTICE "Starting ipsec"
    vlecho_log_level 2 INFO "Calling ipsec _realsetup start"
    # XXX/SML: ipsec script exec's the command, but _plutorun forks off into
    # the background anyway. Why does /etc/init.d/ipsec run this in a subshell?
    ipsec _realsetup start
    retval=$? 
# XXX/SML: test note:  when invoked like this (without wait), as
# /etc/init.d/ipsec start does, this fails to do its stuff-- it only prints
# "ipsec: startup" and returns 0 (don't get debug output). Why??
#    (
#    ipsec _realsetup start
#    retval=$? 
#    ) 2>&1 | logger -s -p $IPSECsyslog -t $PROG_DESCR 2>&1  

    # XXXX/SML: We need to restore after pluto has started, as it silently
    # flushes the IPsec netkey databases.  It would appear that this that this
    # occurs within pluto itself, rather than any of the wrapper scripts,
    # although note that the _realsetup wrapper flushes everything on pluto
    # failure / exit (see 'KILLKIPS').  We should be more surgical-- but this
    # is a workaround until we can determine a way of knowing when pluto has
    # finished its flush.
    #
    # We'll restore the manual config even if pluto startup has failed, as
    # there's no need for manual peerings to be dependent upon pluto.  Note,
    # however, that pm ipsec restarts could create brief delays or disruptions.
    do_verbose setkey_restore || cleanup_exit_failure "Failure restoring policy and SA databases"
    vlecho 2 "Restored crypto policy and SA databases"
    return $retval
}

stop_ipsec()
{
    local retval=0

    export IPSECsyslog=${IPSECsyslog:-daemon.error}
    ipsec _realsetup stop
    retval=$? 
    return $retval
}

check_stop_ipsec()
{
    local retval=0
    if [ -f $runlock ];  then 
        vlecho -1 "Crypto IPsec not properly shut down..."
        stop_ipsec
        retval=$?
        # Make sure it's really dead (see also trap_sigquit())
        killall -s KILL pluto 2>/dev/null && cleanup_exit_failure "$PROG_DESCR failed attempt to killall ipsec"
    fi
    return $retval
}

# =======================================================
# END: Stuff adapted from /etc/init.d/ipsec source
# =======================================================

shopt -s execfail # perfunctory, though probably N/A
trap "trap_sigquit" QUIT
trap "trap_sigterm" TERM
trap "trap_sighup" HUP
trap "trap_sigpipe" PIPE

# trap -p

# =======================================================
# We're ready to do real work now, so kick things off
# =======================================================

# Flush and reload the policy and SA database from setkey.conf
# Note that setkey.conf may have it's own flush directives that are redundant,
# but we need to be sure the database is clean.

check_setkey_sanity

check_wait_racoon || cleanup_exit_failure "Timed out waiting for racoon daemon to die"

vlecho 1 "Restoring crypto policy and SA database"

do_verbose $SETKEY_PATH -FP || cleanup_exit_failure "Failure flushing crypto policy database"
vlecho 2 "Flushed crypto policy database"

do_verbose $SETKEY_PATH -F || cleanup_exit_failure "Failure flushing crypto SA database"
vlecho 2 "Flushed crypto SA database"

# =======================================================
# Start up IPsec
# =======================================================

check_ipsec_sanity
check_stop_ipsec || cleanup_exit_failure "Can't restart IPsec due to stop failure"
start_ipsec || cleanup_exit_failure "Crypto IPsec start failure"
vlecho_log_level -1 INFO "====== $PROG_DESCR started ipsec at $(date "+%Y%m%d-%H%M%S")"

# XXX/SML:  there are two instances of _plutorun, since it forks a subshell
# that waits on for pluto to exit. We'll take whichever instance we get. This
# appears to be the child subshell, but either way we should be fine, since
# upon the child's return, the parent simply reports pluto's exit status and
# exits.  Note that we disable IPSECplutorestartoncrash, otherwise the parent
# would restart pluto in a crash or unknown error case rather than exit.
PLUTORUN_PID=`pidof -xs _plutorun`

# XXX/SML:  One would think there were a non-busy way to wait on a non-child
# process, but so far I can't find one...
echo $$ > $PLUTO_WAIT_FILE || cleanup_exit_terminate "Can't write my pid ($$) into file ${PLUTO_WAIT_FILE}"
while
kill -0 $PLUTORUN_PID >/dev/null 2>&1
do
    usleep 1500000
done

END_TIME=$(date "+%Y%m%d-%H%M%S")
cleanup_exit_terminate "====== Crypto IPsec (pluto) died unexpectedly" $?

exit $RETVAL
