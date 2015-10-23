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
# Note this script is duplicated in both the 'tools' directory
# and in the 'src/base_os/common/script_files' directory.
# These two copies should be kept in sync.
#

PATH=/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin
export PATH

umask 0022

usage()
{
    echo "usage: $0 [-q] [-d <directory>] [-f <filename>] hostname [hostname ...]"
    echo "       $0 -l [-d <directory>] [-f <filename>]"
    echo ""
    echo "   -q: quiet"
    echo "   -d: specify home dir of user to operate on"
    echo "   -f: specify absolute path to known hosts file to operate on"
    echo "   -l: just list known hosts, don't change anything"
    echo ""
    echo "   -d and -f are mutually exclusive."
    exit 1
}


PARSE=`/usr/bin/getopt 'qld:f:' "$@"`

if [ $? != 0 ] ; then
    usage
fi

eval set -- "$PARSE"

VERBOSE=1
DO_LIST_ONLY=0
USER_DIR=${HOME}
USE_SSH_KEYGEN=0

while true ; do
    case "$1" in
        -q) VERBOSE=0; shift 1 ;;
        -l) DO_LIST_ONLY=1; shift 1 ;;
        -d) USER_DIR=$2; shift 2 ;;
        -f) KHF=$2; shift 2 ;;
        --) shift ; break ;;
        *) echo "rkn.sh: parse failure" >&2 ; usage ;;
    esac
done

if [ "x${KHF}" = "x" ]; then
    KHF=${USER_DIR}${DOT_SSH_DIR}/.ssh/known_hosts
fi

BACKUP_KHF=${KHF}.backup
TKHF=${KHF}.tmp.$$
TKHF_FINAL=${KHF}.tmpf.$$

# echo "KHF:        $KHF"
# echo "BACKUP_KHF: $BACKUP_KHF"
# echo "TKHF:       $TKHF"
# echo "TKHF_FINAL: $TKHF_FINAL"

HOSTN=`hostname`
DOMAIN=`echo ${HOSTN} | sed -e '/\./!d' -e 's/^[^\.]*\.\(.*\)$/\1/'`

# See if they're using any hashed hostnames, in which case we'll also
# use ssh-keygen to attempt removal
has_hashed=0
grep -q '^|' ${KHF} && has_hashed=1
if [ ${has_hashed} -ne 0 ]; then
    USE_SSH_KEYGEN=1
else
    USE_SSH_KEYGEN=0
fi

if [ ${DO_LIST_ONLY} -ne 0 ]; then
    if [ $# -ne 0 ]; then
            usage
    fi
    cat ${KHF} | awk '{print $1}' | grep -v '^|' | sed -e 's/,/\n/'

    exit 0
fi

if [ $# -lt 1 ]; then
    usage
fi

#
# If the known hosts file doesn't exist at all, there's no point in
# getting bent out of shape about it.
#
if [ ! -r "${KHF}" ]; then
    echo "Known hosts file not found."
    exit 0
fi

#
# Our plan is to make a temporary file that has all but the lines we want to
# delete, potential removing lines for each specified hostname.  At the end,
# if we detect any changes, we display the diff, make a backup copy of the
# known hosts file, and install the new file.
#
#
# As we do not know if the domain name will be present in the known hosts
# file, we append it to the name if it doesn't already have the domain on it.
#

rm -f ${TKHF} ${TKHF}.old ${TKHF_FINAL}

cp -p ${KHF} ${TKHF_FINAL}

# ${TKHF} will be the new file, and we want to preserve the owner, group,
# and mode of the old file.  This won't happen automatically if we just
# create it, since we may not be running as the user whose KHF we're 
# working with (see bug 13693).  So copy the original, then truncate it.
cp -p ${KHF} ${TKHF}
echo -n "" > ${TKHF}

while [ $# -gt 0 ]; do
    THIS_HOST=$(echo "$1" | sed 's/^.*@\(.*\)$/\1/')

    if [ ! -z "${DOMAIN}" -o "`echo ${THIS_HOST} | sed 's/.'${DOMAIN}'//'`" = "${THIS_HOST}" ]; then
        FULL_HOST="${THIS_HOST}.${DOMAIN}"
        egrep -v '^('${THIS_HOST}'|'${FULL_HOST}')[, ]|,('${THIS_HOST}'|'${FULL_HOST}')[, ]' ${TKHF_FINAL}  >> ${TKHF}
    else
        egrep -v '^'${THIS_HOST}'[, ]|,'${THIS_HOST}'[, ]' ${TKHF_FINAL}  >> ${TKHF}
    fi

    if [ ${USE_SSH_KEYGEN} -ne 0 ]; then
        ssh-keygen -R ${THIS_HOST} -f ${TKHF} > /dev/null 2>&1
        ssh-keygen -R ${FULL_HOST} -f ${TKHF} > /dev/null 2>&1
        rm -f ${TKHF}.old
    fi

    mv ${TKHF} ${TKHF_FINAL}

    shift
done

rm -f ${TKHF}

diff -q ${KHF} ${TKHF_FINAL} > /dev/null
if [ $? -eq 0 ]; then
    if [ ${VERBOSE} -ne 0 ]; then
        echo "No changes made."
    fi
    rm -f ${TKHF_FINAL}
    exit 0
fi

if [ ${VERBOSE} -ne 0 ]; then
    echo "Differences:"
    echo ""
    diff ${KHF} ${TKHF_FINAL}
fi

cp -p ${KHF} ${BACKUP_KHF}
if [ ${VERBOSE} -ne 0 ]; then
    echo ""
    echo "Backup saved as ${BACKUP_KHF}"
    echo ""
fi

mv ${TKHF_FINAL} ${KHF}

exit 0
