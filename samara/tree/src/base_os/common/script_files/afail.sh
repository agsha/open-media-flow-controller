#!/bin/sh

#
# (C) Copyright 2013-2015 Juniper Networks, Inc. All Rights reserved.
# (C) Copyright 2002-2013 Tall Maple Systems, Inc.
# All rights reserved.
# The contents of this file are subject to the terms of the MIT License.
# You may not use this file except in compliance with the License.
# See the file License.txt at the top of this source tree.
# You can obtain a copy of the license at https://opensource.org/licenses/MIT
#

#
# This script is used to analyze a failure, which may or may not have caused
# a core dump.  It is designed to be run as a crash handler from pm.
#

PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

umask 0022


usage()
{
    echo "usage: $0 -n process_name -b binary_path [-l launch_path] [-p process_pid] [-c core_path] [-x exit_code] [-s term_signal] [-a staging_area] [-u process_uptime]"
    echo ""
    exit 1
}

PARSE=`/usr/bin/getopt 'n:p:b:l:c:x:s:a:u:' "$@"`

if [ $? != 0 ] ; then
    usage
fi

eval set -- "$PARSE"

# If we should only handle failures that have coredumps, or all failures
CORE_CRITICAL_ONLY=1

PROCESS_NAME=
PROCESS_PID=-1
BINARY_PATH=
LAUNCH_PATH=
CORE_PATH=
EXIT_CODE=
TERM_SIGNAL=
STAGING_AREA=
HAS_COREDUMP=0
PROCESS_UPTIME=

while true ; do
    case "$1" in
        -n) PROCESS_NAME=$2; shift 2 ;;
        -b) BINARY_PATH=$2; shift 2 ;;
        -l) LAUNCH_PATH=$2; shift 2 ;;
        -p) PROCESS_PID=$2; shift 2 ;;
        -c) HAS_COREDUMP=1; CORE_PATH=$2; shift 2 ;;
        -x) EXIT_CODE=$2; shift 2 ;;
        -s) TERM_SIGNAL=$2; shift 2 ;;
        -a) STAGING_AREA=$2; shift 2 ;;
        -u) PROCESS_UPTIME=$2; shift 2 ;;
        --) shift ; break ;;
        *) echo "afail.sh: parse failure" >&2 ; usage ;;
    esac
done

if [ ! -z "$*" ]; then
    usage
fi

if [ -z "${PROCESS_NAME}" -o -z "${BINARY_PATH}" ]; then
    usage
fi

if [ -f /etc/customer.sh ]; then
    . /etc/customer.sh
fi

# See if we want to do our thing
if [ ${CORE_CRITICAL_ONLY} ]; then
    if [ ${HAS_COREDUMP} -eq 0 ]; then
        exit 0
    fi
fi

OUTPUT_FILE=/tmp/afail-out-tmp-fsum.$$
rm -f ${OUTPUT_FILE}
touch ${OUTPUT_FILE}
BT_FULL_TMP=/tmp/afail-out-tmp-btfull.$$
rm -f ${BT_FULL_TMP}
touch ${BT_FULL_TMP}
REGS_TMP=/tmp/afail-out-tmp-regs.$$
rm -f ${REGS_TMP}
touch ${REGS_TMP}
MAIL_FILE=/tmp/afail-out-tmp-mail.$$
rm -f ${MAIL_FILE}
touch ${MAIL_FILE}

echo "==================================================" >> ${OUTPUT_FILE}
echo "Event information:" >> ${OUTPUT_FILE}
echo ""  >> ${OUTPUT_FILE}

if [ ! -z "${CORE_PATH}" ]; then
    core_size=`stat -Lc %s ${CORE_PATH}`
    core_time=`date '+%Y-%m-%d %H:%M:%S %z' -r ${CORE_PATH}`
fi

if [ ! -z "${BINARY_PATH}" ]; then
    binary_size=`stat -Lc %s ${BINARY_PATH}`
    binary_time=`date '+%Y-%m-%d %H:%M:%S %z' -r ${BINARY_PATH}`
fi

if [ ${HAS_COREDUMP} -eq 1 ]; then
    echo "Description:    Unexpected failure of process ${PROCESS_NAME}"  >> ${OUTPUT_FILE}

else
##################################################
    echo "Description:    Unexpected exit of process ${PROCESS_NAME}"  >> ${OUTPUT_FILE}
fi

if [ ! -z ${LAUNCH_PATH} ]; then
    echo "Launch path:    ${LAUNCH_PATH}"  >> ${OUTPUT_FILE}
fi
echo "Binary path:    ${BINARY_PATH}"  >> ${OUTPUT_FILE}
echo "Binary size:    ${binary_size}" >> ${OUTPUT_FILE}
echo "Binary time:    ${binary_time}" >> ${OUTPUT_FILE}
if [ ! -z "${CORE_PATH}" ]; then
    core_name=`basename ${CORE_PATH}`
    echo "Core name:      ${core_name}" >> ${OUTPUT_FILE}
    echo "Core size:      ${core_size}" >> ${OUTPUT_FILE}
    echo "Core time:      ${core_time}" >> ${OUTPUT_FILE}
fi
if [ ! -z ${PROCESS_PID} -a ${PROCESS_PID} != 0 -a ${PROCESS_PID} != -1 ]; then
    echo "Process ID:     ${PROCESS_PID}"  >> ${OUTPUT_FILE}
fi
if [ ! -z ${EXIT_CODE} ]; then
    echo "Exit code:      ${EXIT_CODE}"  >> ${OUTPUT_FILE}
fi
if [ ! -z ${TERM_SIGNAL} ]; then
    echo "Fatal signal:   ${TERM_SIGNAL}"  >> ${OUTPUT_FILE}
fi
if [ ! -z "${PROCESS_UPTIME}" ]; then
    echo "Process uptime: ${PROCESS_UPTIME}"  >> ${OUTPUT_FILE}
fi


# Do coredump
if [ ${HAS_COREDUMP} -eq 1 ]; then

    cp ${OUTPUT_FILE} ${BT_FULL_TMP}

    # Normal (brief) backtrace

    GDB_COMMANDS_TMP=/tmp/afail-tmp-bt.$$
    rm -f ${GDB_COMMANDS_TMP}
    touch ${GDB_COMMANDS_TMP}
    cat >> ${GDB_COMMANDS_TMP} <<EOF
set height 0
set width 0
thread apply all backtrace
EOF
    echo "" >> ${OUTPUT_FILE}
    echo "Backtrace:" >> ${OUTPUT_FILE}
    echo "" >> ${OUTPUT_FILE}
    gdb -nx --batch --command ${GDB_COMMANDS_TMP} --se ${BINARY_PATH} --core ${CORE_PATH} >> ${OUTPUT_FILE}
    echo "" >> ${OUTPUT_FILE}
    rm -f ${GDB_COMMANDS_TMP}

    # Full (detailed) backtrace

    GDB_COMMANDS_TMP=/tmp/afail-tmp-btf.$$
    rm -f ${GDB_COMMANDS_TMP}
    touch ${GDB_COMMANDS_TMP}
    cat >> ${GDB_COMMANDS_TMP} <<EOF
set height 0
set width 0
thread apply all backtrace full
EOF
    echo "" >> ${BT_FULL_TMP}
    echo "Backtrace:" >> ${BT_FULL_TMP}
    echo "" >> ${BT_FULL_TMP}
    gdb -nx --batch --command ${GDB_COMMANDS_TMP} --se ${BINARY_PATH} --core ${CORE_PATH} >> ${BT_FULL_TMP}
    echo "" >> ${BT_FULL_TMP}
    rm -f ${GDB_COMMANDS_TMP}

    # Register values

    GDB_COMMANDS_TMP=/tmp/afail-tmp-reg.$$
    rm -f ${GDB_COMMANDS_TMP}
    touch ${GDB_COMMANDS_TMP}
    cat >> ${GDB_COMMANDS_TMP} <<EOF
set height 0
set width 0
thread apply all info all-registers
EOF
    echo "" >> ${REGS_TMP}
    echo "Registers:" >> ${REGS_TMP}
    echo "" >> ${REGS_TMP}
    gdb -nx --batch --command ${GDB_COMMANDS_TMP} --se ${BINARY_PATH} --core ${CORE_PATH} >> ${REGS_TMP}
    echo "" >> ${REGS_TMP}
    rm -f ${GDB_COMMANDS_TMP}

fi

# Now copy our output file into the pm snapshot directory
if [ ! -z "${STAGING_AREA}" ]; then
    cp ${OUTPUT_FILE} ${STAGING_AREA}/fsummary.txt
    cp ${BT_FULL_TMP} ${STAGING_AREA}/backtrace_full.txt
    cp ${REGS_TMP} ${STAGING_AREA}/backtrace_registers.txt
fi

# We now want to take a system dump
SYSINFO=
SYSDUMP=

WANT_SYSDUMP=1
#
# Graft point to say if we want a sysdump
# Available variables:
#   - ${WANT_SYSDUMP}: set to 0 to disable sysdump
#
if [ "$HAVE_AFAIL_GRAFT_2" = "y" ]; then
    afail_graft_2
fi

if [ "${WANT_SYSDUMP}" = "1" ]; then
    FAILURE=0
    OPS=`/sbin/sysdump.sh ${STAGING_AREA}/fsummary.txt ${STAGING_AREA}/backtrace_full.txt ${STAGING_AREA}/backtrace_registers.txt` || FAILURE=1
    if [ ${FAILURE} -eq 1 ]; then
        logger -p user.err "Could not generate dump"
        exit 1
    fi

    # Set SYSINFO and SYSDUMP
    eval ${OPS}
fi

# Copy the sysinfo and sysdump files into the staging area.
# The sysdump contains the sysinfo file as well as fsummary.txt;
# they are just included separately as a convenience.
if [ ! -z "${SYSINFO}" -a ! -z "${STAGING_AREA}" ]; then
    cp ${SYSINFO} ${STAGING_AREA}
fi
if [ ! -z "${SYSDUMP}" -a ! -z "${STAGING_AREA}" ]; then
    cp ${SYSDUMP} ${STAGING_AREA}
fi

# If there's a Java "HotSpot" file, which contains additional information
# in the case of a JVM crash (presumably due to a native code bug), include
# its contents in the email
HS_NAME=hs_err_pid${PROCESS_PID}.log
HS_PATH=${STAGING_AREA}/${HS_NAME}
if [ -f "${HS_PATH}" ]; then
    echo "" >> ${OUTPUT_FILE}
    echo "==================================================" >> ${OUTPUT_FILE}
    echo "Contents of ${HS_NAME}:" >> ${OUTPUT_FILE}
    echo "" >> ${OUTPUT_FILE}
    cat ${HS_PATH} >> ${OUTPUT_FILE}
    echo "" >> ${OUTPUT_FILE}
fi

# Finally, tack the summary info from the dump onto our summary, and send the 
# combined summary with the dump off to the critical failure email address

echo "" >> ${OUTPUT_FILE}
if [ ! -z "${SYSINFO}" -a -f "${SYSINFO}" ]; then
    cat ${SYSINFO} >> ${OUTPUT_FILE}
fi

HOSTNAME=`uname -n`
FAILURE=0

# NOTE: notifies.txt is a path defined in tpaths.h, but we can't use it
# here because we are a shell script.
RECIPS_FILE=`cat /var/opt/tms/output/notifies.txt | grep RECIPIENTS=`
WEBPOST_FILE=`cat /var/opt/tms/output/notifies.txt | grep AUTOSUPPORT_WEBPOST_`

# Set FAILURE_RECIPIENTS and AUTOSUPPORT_RECIPIENTS
eval ${RECIPS_FILE}

# Set AUTOSUPPORT_WEBPOST_ENABLED and AUTOSUPPORT_WEBPOST_URL.
eval ${WEBPOST_FILE}

# Customer.sh should set the SUBJECT_PREFIX variable for us.
# But set it ourselves first in case it doesn't.
SUBJECT_PREFIX=System

# XXX/jcho: this is a temporary implementation.
# Check if webpost is enabled. If so, we should only send the autosupport
# message by webpost. If not, we should continue sending it via email.
# XXX/EMT: the AUTOSUPPORT_WEBPOST_ENABLED and AUTOSUPPORT_WEBPOST_URL
# were previously set by mdreq, but now they are set from evaluating
# notifies.txt above.  This change was to avoid deadlocks if mgmtd was
# waiting on PM as in bug 10364.

MAKEMAIL_OPTS=
if [ ${AUTOSUPPORT_WEBPOST_ENABLED} = true ]; then
    # Tell makemail to not mail, just to format a message into a file.
    MAKEMAIL_OPTS="-o ${MAIL_FILE}"
fi

WANT_EMAIL=1
#
# Graft point to modify or append to the body of the email to be sent.
# Available variables:
#   - ${OUTPUT_FILE}: path to the file containing email body
#
if [ "$HAVE_AFAIL_GRAFT_1" = "y" ]; then
    afail_graft_1
fi

if [ "${WANT_EMAIL}" -eq 0 ]; then

    # If you change this list, change copy below too
    rm -f ${OUTPUT_FILE}
    rm -f ${BT_FULL_TMP}
    rm -f ${REGS_TMP}
    rm -f ${MAIL_FILE}
    if [ ! -z "${SYSINFO}" ]; then
        rm -f ${SYSINFO}
    fi
    exit 0
fi

# At this point, all we have left to do is send email and cleanup after
# ourselves.  We do this in the background so we can return immediately.

(

    # Send the autosupport and failure emails separately to avoid relay issues

    AUTOSUPPORT_RECIPIENTS="`echo ${AUTOSUPPORT_RECIPIENTS} | sed 's/ /,/'`"
    if [ ! -z "${AUTOSUPPORT_RECIPIENTS}" ]; then
        makemail.sh -s "$SUBJECT_PREFIX failure on $HOSTNAME: Process failure: ${PROCESS_NAME}" -t "${AUTOSUPPORT_RECIPIENTS}" -i ${OUTPUT_FILE} ${MAKEMAIL_OPTS} -S "-C /etc/ssmtp-auto.conf" -m application/x-gzip ${SYSDUMP} || FAILURE=1
        if [ ${FAILURE} -eq 1 ]; then
            logger -p user.err "Could not send autosupport failure notification mail"
        fi
        if [ ${AUTOSUPPORT_WEBPOST_ENABLED} = true ]; then
            # URL-escape the '@' in the email address
            AUTOSUPPORT_RECIPIENTS="`echo ${AUTOSUPPORT_RECIPIENTS} | sed 's/@/%40/'`"
            URL="${AUTOSUPPORT_WEBPOST_URL}?to=${AUTOSUPPORT_RECIPIENTS}"
            if ! /sbin/webpost.sh ${URL} ${MAIL_FILE}; then
                logger -p user.err "Could not post failure notification mail"
            fi
        fi
    fi

    FAILURE_RECIPIENTS="`echo ${FAILURE_RECIPIENTS} | sed 's/ /,/'`"
    if [ ! -z "${FAILURE_RECIPIENTS}" ]; then
        makemail.sh -s "$SUBJECT_PREFIX failure on $HOSTNAME: Process failure: ${PROCESS_NAME}" -t "${FAILURE_RECIPIENTS}" -i ${OUTPUT_FILE} -m application/x-gzip ${SYSDUMP} || FAILURE=1
        if [ ${FAILURE} -eq 1 ]; then
            logger -p user.err "Could not send failure notification mail"
        fi
    fi

    # If you change this list, change copy above too
    rm -f ${OUTPUT_FILE}
    rm -f ${BT_FULL_TMP}
    rm -f ${REGS_TMP}
    rm -f ${MAIL_FILE}
    if [ ! -z "${SYSINFO}" ]; then
        rm -f ${SYSINFO}
    fi

) &

exit 0
