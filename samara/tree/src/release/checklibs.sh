#!/bin/sh

#
#
#
#

# This script can be used while creating or modifying the base packages
# that make up a platform.  It detects binaries or libraries that are
# dynamically linked against libraries that are missing from the system
# image.  It does this by calling 'ldd' repeatedly.

# To use it, either copy the script to / on a running system and run it or:
#
# 1) Do a full build:
#
#    sudo make
#    sudo make install
#
# 2) Enter your build output directory:
#
#    cd ${PROD_OUTPUT_ROOT}/product-demo-i386/install/image/
#
#    NOTE: you can also do this for .../install/rootflop/ , but you
#          must also copy in 'ldd' (and make it use /bin/sh), as:
#
#        cd ${PROD_OUTPUT_ROOT}/product-demo-i386/install/rootflop/
#        sudo cat /usr/bin/ldd  | sed 's,^#!.*/bin/bash$,#!/bin/sh,' > ./usr/bin/ldd
#        sudo chmod 755 ./usr/bin/ldd
#
# 3) Copy this script into the directory
#
#    sudo cp -p ${PROD_TREE_ROOT}/src/release/checklibs.sh .
#
# 4) Chroot into this directory, and run ldconfig:
#
#    sudo chroot . ./bin/sh
#    ldconfig
#
#    NOTE: you do not need to do 'ldconfig' for the rootflop.
#          Everything is required to work as-is, and 'ldconfig' is
#          not present.
#
# 5) If you have not already mounted /proc et al. in this directory,
#    do so now.  You'll only have to do this once for a given directory
#    until you unmount them.
#
#    mount -n -t proc /proc /proc
#    mount -n -t usbfs /proc/bus/usb /proc/bus/usb
#    mount -n -t sysfs /sys /sys
#    cat /proc/mounts > /etc/mtab
#
#    NOTE: do not do the "cat /proc/mounts" line for the rootflop, it uses
#          a symlink instead.
#
# 6) Run this script
#
#    ./checklibs.sh
#
#    Ignore any lines about find and /proc files (which disappear)

PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

usage()
{
    echo "usage: $0 [-s file] [-v [-v]]"
    exit 1
}

vecho()
{
    level=$1
    shift

    if [ ${VERBOSE} -gt ${level} ]; then
        echo $*
    fi
}

PARSE=`/usr/bin/getopt 's:v' "$@"`

if [ $? != 0 ] ; then
    usage
fi

eval set -- "$PARSE"

VERBOSE=0
VERBOSE_STRING=
SINGLE=0

while true ; do
    case "$1" in
        -s) SINGLE=1; SINGLE_FILE=$2; shift 2 ;;
        -v) VERBOSE=$((${VERBOSE}+1)); VERBOSE_STRING=" -v ${VERBOSE_STRING}"; shift ;;
        --) shift ; break ;;
        *) echo "$0: parse failure" >&2 ; usage ;;
    esac
done

if [ ! -z "$*" ] ; then
    usage
fi

if [ ${SINGLE} -eq 0 ]; then
    # Call ourself to deal with the single file.  We used to have:
    # "! -fstype proc" below, but busybox does not support that.  We are
    # using 'xargs' instead of "-exec" for the same reason.

    find . -type f -perm +111 -print | xargs -r -n 1 $0 ${VERBOSE_STRING} -s

else
    failure=0
    outp=`ldd $SINGLE_FILE 2>&1` || failure=1

    if [ ${failure} -ne 0 -o -z "${outp}" ]; then
        notdynamic=0
        if [ -z "${outp}" ]; then
            notdynamic=1
        fi
        echo "${outp}" | grep -q "not a dynamic executable" - && notdynamic=1
        if [ ${notdynamic} -eq 1 ]; then
            vecho 0 "NOTDYNAMIC: ${SINGLE_FILE}"
            exit 0
        fi

        echo "ERROR: ${SINGLE_FILE}: ${outp}"
        exit 1
    fi

    missing=0
    echo "${outp}" | grep -q "not found" - && missing=1
    if [ ${missing} -eq 1 ]; then
        mo=`echo "${outp}" | grep "not found" - | sed 's/ => not found//' | sort | uniq | awk '{print "    MISSING_LIB: " $1}'`
        echo "MISSING_FROM: ${SINGLE_FILE}"
        echo "${mo}"
    else
        vecho 0 "GOOD: ${SINGLE_FILE}"
	if [ ${VERBOSE} -gt 1 ]; then
            mo=`echo "${outp}" |  sed 's/ => .*$//' | sort | uniq | awk '{print "    LIB: " $1}'`
	    echo "${mo}"
	fi
        exit 0
    fi

fi

exit 0
