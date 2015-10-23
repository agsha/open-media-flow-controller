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

#
# Usage: stats_defrag.sh [-n] [-v | -vv]
#
#        -n   Print the number of files improved
#        -v   Verbose: list paths of all files inspected
#        -vv  Very verbose: also tell what happened with each file
#

#
# This script makes a simple attempt at defragmenting stats files.
# For each one:
#
#   1. Check how many extents it has, and what the ideal would be.
#      If it is already ideal, we're done.
#
#   2. Make a copy of the file.  The hope is that a newly-written
#      file, allocated all at once, will be less fragmented.
#
#   3. Check how many extents the new file has.  If it is fewer than the
#      old one, replace the old one with it.
#
#   4. If the new file does NOT have fewer extents, we're going to delete
#      it, but don't do so until the end.  If we delete it now, the next
#      file we copy may very well fill in the same holes and not get 
#      defragged either.  We want to temporarily keep those holes plugged,
#      so subsequent files will have a better chance of being contiguous.
#

usage()
{
    echo "usage: $0 [-n] [-v | -vv]"
    exit 1
}

#
# Make sure statsd is not running.  We don't want to delete any files
# out from under it!
#
ST_PID=`pidof /opt/tms/bin/statsd`
if [ ! -z "${ST_PID}" ]; then
    echo "Cannot degragment while statsd is running!"
    exit 1
fi

PARSE=`/usr/bin/getopt 'nv' "$@"`

if [ $? != 0 ] ; then
    usage
fi

eval set -- "$PARSE"

VERBOSE=0
PRINT_NUM=0

while true ; do
    case "$1" in
        -n) PRINT_NUM=1
            shift ;;
        -v) VERBOSE=$((${VERBOSE}+1))
            shift ;;
        --) shift ; break ;;
        *)  echo "$0: parse failure" >&2 ; usage ;;
    esac
done

vecho()
{
    level=$1
    shift

    if [ ${VERBOSE} -gt ${level} ]; then
        echo "$*"
    fi
}

STATS_DIR=/var/opt/tms/stats
CP=/bin/cp

FILES_NOFIX=
FILES_FIXED=
FILES_TO_DELETE=
NUM_IMPROVED=0

STATS_FILES=`find ${STATS_DIR} -type f`
for STATS_FILE in ${STATS_FILES} ; do
    # XXX #dep/parse: filefrag
    FF_OUTPUT=`filefrag ${STATS_FILE}`

    # Extract info from 'filefrag' output, assuming format like:
    #    snmpd: 149 extents found, perfection would be 1 extent
    # -OR-
    #    foo: 1 extent found
    EXT_ACTUAL=`echo ${FF_OUTPUT} | awk '{print $2}'`
    EXT_IDEAL=`echo ${FF_OUTPUT} | awk '{print $8}'`

    vecho 0 "${STATS_FILE}"

    # If we're already at the ideal, it just doesn't print anything
    # about the ideal.
    if [ -z "${EXT_IDEAL}" ]; then
        vecho 1 "   Has ${EXT_ACTUAL} extent(s), already ideal; no defrag"
        FILES_NOFIX+="${STATS_FILE} "
    else
        vecho 1 "   Has ${EXT_ACTUAL} extents (${EXT_IDEAL} ideal); attempting defrag"

        # All stats files end in .dat, so this won't overwrite anything.
        STATS_FILE_COPY=`mktemp ${STATS_FILE}.XXXXXX`
        ${CP} -f -p ${STATS_FILE} ${STATS_FILE_COPY}

        # XXX #dep/parse: filefrag
        NEW_FF_OUTPUT=`filefrag ${STATS_FILE_COPY}`
        NEW_EXT_ACTUAL=`echo ${NEW_FF_OUTPUT} | awk '{print $2}'`
        if [ ${NEW_EXT_ACTUAL} -lt ${EXT_ACTUAL} ]; then
            vecho 1 "   Copy reduced to ${NEW_EXT_ACTUAL} extent(s), so replacing original"
            mv -f ${STATS_FILE_COPY} ${STATS_FILE}
            FILES_FIXED+="${STATS_FILE} "
            NUM_IMPROVED=$((${NUM_IMPROVED}+1))
        else
            vecho 1 "   Copy has ${NEW_EXT_ACTUAL} extents, which is no better; will discard later"
            FILES_TO_DELETE+="${STATS_FILE_COPY} "
        fi
    fi
done

vecho 1 ""
vecho 1 "Files already ideal: ${FILES_NOFIX}"
vecho 1 ""
vecho 1 "Files improved:      ${FILES_FIXED}"         
vecho 1 ""
vecho 1 "Rejected copies:     ${FILES_TO_DELETE}"

if [ ${PRINT_NUM} -eq 1 ]; then
    echo "${NUM_IMPROVED}"
fi

if [ ! -z "${FILES_TO_DELETE}" ]; then
    rm -f ${FILES_TO_DELETE}
fi
