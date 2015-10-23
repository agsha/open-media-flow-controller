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

PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

umask 0022

#
# Usage: timeout.sh [-t <seconds>] -- <program path> <program arguments>
#
# Run a specified program with a timeout.  Specify the number of seconds 
# with -t, or use the default of 10 seconds.  If the program completes 
# within the allotted time, the output is printed normally.  Otherwise,
# we attempt to terminate it, and exit.
#
# NOTE: this does not work correctly with a program that requires the
# terminal.  The program specified is run in the background.
#
# (The "--" is not always technically required, but is advisable
# to avoid difficulties.  Because we are using getopt, if any of your 
# program arguments begin with '-', it might mistake them for one intended
# for timeout.sh.  Using the "--" before the program path prevents this.)
#

usage()
{
    echo "usage: $0 [-t <seconds>] -- <program path> <program arguments>"
    echo ""
    echo "Default timeout is 10 seconds if not specified."
}

PARSE=`/usr/bin/getopt -s sh 't:' "$@"`

if [ $? != 0 ] ; then
    usage
fi

eval set -- "$PARSE"

TIMEOUT=10

while true ; do
    case "$1" in
        -t) TIMEOUT=$2; shift 2 ;;
        --) shift ; break ;;
        *) echo "timeout.sh: parse failure" >&2 ; usage ;;
    esac
done

CMD="$*"

timeout_cmd() {
    had_timeout=1
}

cleanup_exit() {
    if [ "${cmd_pid}" -ne 0 ]; then
        kill "${cmd_pid}" > /dev/null 2>&1
    fi
    if [ "${timer_pid}" -ne 0 ]; then
        kill "${timer_pid}" > /dev/null 2>&1
    fi

    exit 1
}

had_timeout=0
cmd_pid=0
timer_pid=0
trap "timeout_cmd" ALRM
trap "cleanup_exit" HUP INT QUIT PIPE TERM

#
# Launch the command we want.
#
# Note: this exact invocation is necessary to deal correctly with quotation
# marks on the command line.  If you do the obvious...
#
#   ${CMD} &
#
# ...that does not work if there are double quotes in the command intended
# to bind words together.
#
sh -c "${CMD}" &
cmd_pid=$!

#
# Launch the the timer.
#
had_timeout=0
(sleep ${TIMEOUT}; kill -ALRM $$) &
timer_pid=$!

#
# Wait for our command to finish, killing timer when it does.
# If the timer goes off, a SIGALRM will break us out of the "wait" call.
#
wait ${cmd_pid}
retv=$?
kill ${timer_pid} > /dev/null 2>&1

if [ "$retv" -gt 128 -a "${had_timeout}" -eq 1 ]; then
    echo "'${CMD}' timed out after ${TIMEOUT} seconds"
    kill ${cmd_pid}
elif [ "$retv" -ne 0 ]; then
    echo "'${CMD}' returned failure code ${retv}"
fi

exit $retv
