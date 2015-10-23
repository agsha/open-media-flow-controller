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
. /etc/image_layout.sh
. /etc/layout_settings.sh
. /etc/image_layout.sh

VERBOSE=1

usage()
{
    echo "usage: $(basename $0) [-p][-v][-q]"
    echo ""
    echo "Return a list of disk devices that are part of the system layout."
    echo "By default, only parent disk names are returned, e.g. 'sda' 'sdb'"
    echo "Also sets the variables TARGET_DISKNAMES (disk device names only) "
    echo "and TARGET_DEVS (full device paths, e.g. '/dev/sda')"
    echo ""
    echo "-p: Also include disk partitions, e.g. 'sda1', 'sda2', ..."
    echo "-v: Verbose (default, prints device names)"
    echo "-q: Quiet (do not print device names or exit error message)"

    exit 1
}

vecho()
{
    level=$1
    shift

    if [ ${VERBOSE} -gt ${level} ]; then
        echo "$*"
    fi
}

PARSE=`/usr/bin/getopt -s sh 'pqv' "$@"`
if [ $? != 0 ]; then
    usage
fi

eval set -- "$PARSE"

while true ; do
    case "$1" in
        -p) INCLUDE_PARTS=1;  shift ;;
        -q) VERBOSE=-1;  shift ;;
        -v) VERBOSE=$((${VERBOSE}+1)); shift ;;
        --) shift ; break ;;
        *) usage ;;
    esac
done

TARGET_NAMES=""
export TARGET_DEVS=""
export TARGET_DISKNAMES=""

# Get system layout disk parent disk device names
eval 'TARGET_NAMES="${IL_LO_'${IL_LAYOUT}'_TARGETS}"'
for tn in ${TARGET_NAMES}; do
    new_dev=""
    eval 'new_dev="${IL_LO_'${IL_LAYOUT}'_TARGET_'${tn}'_DEV}"'

    if [ -z "${new_dev}" ]; then
        vecho -1 "Error: could not build device list for target $tn"
        exit 1
    fi

    diskname=${new_dev:5:${#new_dev}}
    diskname=`echo ${new_dev} | sed 's/^\/dev\///'`
    TARGET_DEVS="${TARGET_DEVS}${TARGET_DEVS:+ }${new_dev}"
    TARGET_DISKNAMES="${TARGET_DISKNAMES}${TARGET_DISKNAMES:+ }${diskname}"
    vecho 0 $diskname
done

if [ ${INCLUDE_PARTS} ]; then
    # Get system layout disk partition device names
    eval 'TARGET_PARTS="${IL_LO_'${IL_LAYOUT}'_PARTS}"'
    for tn in ${TARGET_PARTS}; do
        new_dev=""
        dev=""
        cdev=""
        eval 'dev="${IL_LO_'${IL_LAYOUT}'_PART_'${tn}'_DEV}"'
        eval 'cdev="${IL_LO_'${IL_LAYOUT}'_PART_'${tn}'_DEV_CHAR}"'

        # If the partition has a block device, use that, else it should have a
        # character device.
        if [ -n "${dev}" ]; then
            new_dev=$dev
        elif [ -n "${cdev}" ]; then
            new_dev=$cdev
        else
            vecho -1 "Error: could not build device list for target ${tn}"
            exit 1
        fi

        partname=`echo ${new_dev} | sed 's/^\/dev\///'`
        TARGET_DEVS="${TARGET_DEVS}${TARGET_DEVS:+ }${new_dev}"
        TARGET_DISKNAMES="${TARGET_DISKNAMES}${TARGET_DISKNAMES:+ }${partname}"
        vecho 0 $partname
    done
fi
