PR: 
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
# The plan: daemonize, shut down all of user land.  Make a tmpfs, copy root,
# /config, /bootmgr, parts of /var and /opt into it.  unmount everything
# (except root).  pivot_root and chroot into the tmpfs, and unmount all
# that's left, including the old root.  We then restart syslogd and start
# ourself again to finally call manufacture.sh.  Afterward we get the chance 
# to copy over things like the old config.
#

# If you change where this program is installed, you must update this!
SELF=/sbin/remanufacture.sh

usage()
{
    echo "usage: $0 [-v] [-F LOGFILE] [-c] [-l] [-e] -f full_path_to_image"
    echo ""
    echo "-c: copy configuration from old system to new"
    echo ""
    echo "-f IMAGE: this must be an absolute path to a .img file"
    echo ""
    echo "-l: log to /var/tmp/remfg/rl.#.txt where # is the pass number"
    echo ""
    echo "-e: exit instead of rebooting, on success or failure"
    echo ""
    echo "-F LOGFILE: make manufacture.sh log some verbose output to the file."
    echo "   Use '-l' to cause remanufacture.sh itself to log to a file."
    echo ""
    echo "-I: Ignore any image signature"
    echo ""
    echo "-r: Require image signature"
    echo ""
    echo "-v: verbose mode.  Specify multiple times for more output"
    echo ""
    echo ""
    echo ""
    echo "usage [external forms, for programmatic or test use only]:"
    echo "       $0 [-i] -x external_program"
    echo "       $0 [-i] -x -"
    echo ""
    echo "-x EXTERNAL_PROGRAM: instead of calling manufacture.sh, call this "
    echo "                     script.  If '-', don't call anything, but do not"
    echo "                     restart the system either; used for debugging"
    echo ""
    echo "-i: external program will be run with stdin as the console."
    exit 1
}

# ==================================================
# Based on verboseness setting suppress command output
# ==================================================
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

    if [ ${VERBOSE} -gt ${level} ]; then
        echo "$*"
        echo "$*" | xargs -i logger -p ${LOG_FACILITY:-user}.${log_level} -t "${LOG_TAG:-${0}[$$]}" "[${LOG_COMPONENT:-${0}}.${log_level}]: {}"
        if [ ${LOG_OUTPUT} -ne 0 -a ! -z "${LOG_FILE}" ]; then
            echo "$*" >> ${LOG_FILE}
        fi
    fi
}

# ==================================================
# Echo and log an Error or not based on VERBOSE setting
# ==================================================
vlechoerr()
{
    level=$1
    shift
    local log_level=ERR

    if [ ${VERBOSE:-0} -gt ${level} ]; then
        echo "$*"
        echo "$*" | xargs -i logger -p ${LOG_FACILITY:-user}.${log_level} -t "${LOG_TAG:-${0}[$$]}" "[${LOG_COMPONENT:-${0}}.${log_level}]: {}"
        if [ ${LOG_OUTPUT} -ne 0 -a ! -z "${LOG_FILE}" ]; then
            echo "$*" >> ${LOG_FILE}
        fi
    fi
}

# ==================================================
# Called on errors after pass 0 
# ==================================================
die_reboot() {
    vlechoerr -1 "*** Remanufacture process failed."

    if [ ${EXIT_WHEN_DONE} -ne 1 ]; then
        if [ "${NEED_REBOOT}" != 0 ]; then
            vlechoerr -1 "*** Rebooting..."
            sync
            # Juniper added the sleep 5
            sleep 5
            /sbin/reboot -f
        fi
    else
        if [ "${DO_TELINIT}" = "1" -a "${DID_TELINIT}" = "1" ]; then
            /sbin/telinit 4
        fi
    fi

    vlechoerr -1 "*** Exiting"
    sync

    exit 1
}

# ==================================================
# Cleanup when called from 'trap' for ^C or signal
# ==================================================
trap_cleanup_exit()
{
    vlechoerr -1 "*** Interrupted by signal"
    die_reboot
}

# returns OK if $1 contains $2
strstr() {
    case "${1}" in
        *${2}*) return 0;;
    esac
    return 1
}

find_output_dev() {
    # Detect the output device, to avoid problems with writing to
    # /dev/console when /dev/console is a serial console which does not
    # exist.  Otherwise we could end up blocking.  If OUTPUT_DEV is "",
    # we do not change our output, which (if we're coming from pass 1)
    # means we'll be using /dev/null .

    OUTPUT_DEV=/dev/tty0
    consoles=$(cat /proc/cmdline | tr ' ' '\n' | grep console= | sed 's/^console=//' | sed 's/\(^[^,]*\).*$/\1/')
    for console in ${consoles}; do
        case "${console}" in
             tty0) 
                 sdev=/dev/${console}
                 if [ -e "${sdev}" ]; then
                     OUTPUT_DEV=${sdev}
                 fi
                 ;;
             ttyS*)
                 sdev=/dev/${console}
                 # Last digit is port number
                 sernum=$(echo "${sdev}" | sed 's/^.*\(.\)$/\1/')

                 # Make sure this device exists, and if so that it has DSR or CD
                 if [ -e "${sdev}" ]; then
                     # Get just the signal names as a list
                     sersigs=$(cat /proc/tty/driver/serial  | grep '^'${sernum}': ' | tr ' ' '\n' | grep -v : | tr '|' ' ')
                     present=0
                     echo "${sersigs}" | egrep -q '(DSR|CD)' && present=1
                     if [ ${present} -eq 1 ]; then
                         OUTPUT_DEV=${sdev}
                     fi
                 fi
                 ;;
            *) ;;
        esac
    done
}

# Get us into / as we'll be unmounting partitions
cd /

TPKG_QUERY=/sbin/tpkg_query.sh

PASS_VAL=0

LOG_COMPONENT="remanufacture"
LOG_TAG="${LOG_COMPONENT}[$$]"
# LOG_LEVEL is uppercase, facility lower
LOG_FACILITY=user
LOG_LEVEL=NOTICE
LOG_OUTPUT=0
LOG_FILE=

VERBOSE=0
VERBOSE_ARGS=
SYSIMAGE_FILE=
SELF_ARGS=
COPY_CONFIG=0
EXIT_WHEN_DONE=0
EXTERNAL_REMANUFACTURE=
EXTERNAL_INTERACTIVE=0
DO_TELINIT=1
DID_TELINIT=
OUTPUT_DEV=/dev/tty0
NEED_REBOOT=0
MFG_BASE_ARGS=

MANUFACTURE=/sbin/manufacture.sh
CONFIG_DB_DIR=/config/db
MFG_DB_DIR=/config/mfg
MFG_DB_PATH=${MFG_DB_DIR}/mfdb
MFG_INC_DB_PATH=${MFG_DB_DIR}/mfincdb
MDDBREQ=/opt/tms/bin/mddbreq

# XXXX If MFG_MOUNT, etc. are changed in manufacture.sh, update here too.
NEW_MFG_MOUNT=/tmp/mfg_mount
NEW_MFG_VMOUNT=/tmp/mfg_vmount
NEW_MFG_DB_DIR=${NEW_MFG_MOUNT}/config/mfg
NEW_MFG_DB_PATH=${NEW_MFG_DB_DIR}/mfdb
NEW_MFG_INC_DB_PATH=${NEW_MFG_DB_DIR}/mfincdb

ALL_ARGS="$@"
PARSE=`/usr/bin/getopt 'z:cf:x:leivF:Irq' "$@"`

if [ $? != 0 ] ; then
    usage
fi

eval set -- "$PARSE"

while true ; do
    case "$1" in
        -z) PASS_VAL=$2; shift 2 ;;
        -c) COPY_CONFIG=1
            SELF_ARGS="${SELF_ARGS} $1"
            shift ;;
        -f) SYSIMAGE_FILE=$2; 
            SELF_ARGS="${SELF_ARGS} $1 $2"
            shift 2 ;;
        -x) EXTERNAL_REMANUFACTURE=$2; 
            SELF_ARGS="${SELF_ARGS} $1 $2"
            shift 2 ;;
        -i) EXTERNAL_INTERACTIVE=1; 
            SELF_ARGS="${SELF_ARGS} $1"
            shift ;;
        -l) LOG_OUTPUT=1;
            SELF_ARGS="${SELF_ARGS} $1"
            shift ;;
        -e) EXIT_WHEN_DONE=1;
            SELF_ARGS="${SELF_ARGS} $1"
            shift ;;
        -v) VERBOSE=$((${VERBOSE}+1))
            SELF_ARGS="${SELF_ARGS} $1"
            VERBOSE_ARGS="${VERBOSE_ARGS} $1"
            shift ;;
        -F) SELF_ARGS="${SELF_ARGS} $1 $2"
            VERBOSE_ARGS="${VERBOSE_ARGS} $1 $2"
            shift 2 ;;
        -I) SELF_ARGS="${SELF_ARGS} $1"
            MFG_BASE_ARGS="${MFG_BASE_ARGS} $1"
            shift ;;
        -r) SELF_ARGS="${SELF_ARGS} $1"
            MFG_BASE_ARGS="${MFG_BASE_ARGS} $1"
            shift ;;
        -q) VERBOSE=0
            SELF_ARGS="${SELF_ARGS} $1"
            VERBOSE_ARGS="$1"
            shift ;;
        --) shift ; break ;;
        *) echo "$0: parse failure" >&2 ; usage ;;
    esac
done

if [ ! -z "$*" ] ; then
    usage
fi

if [ -z "${SYSIMAGE_FILE}" -a -z "${EXTERNAL_REMANUFACTURE}" ]; then
    usage
fi

# Define graft functions
if [ -f /etc/customer_rootflop.sh ]; then
    . /etc/customer_rootflop.sh
fi

# Make sure we have some terminal type set
if [ -z "${TERM}" ]; then
    TERM=vt102
    export TERM
fi

# This is used to signal manufacture.sh that we are actually in progress
# of remanufacturing the box.  NOTE: if you change this, update manufacture.sh
REMANUFACTURE=1
export REMANUFACTURE

# Setup logging
if [ ${LOG_OUTPUT} -ne 0 ]; then
    LOG_DIR=/var/tmp/remfg
    LOG_FILE=${LOG_DIR}/rl.${PASS_VAL}.txt
    if [ ${PASS_VAL} -eq 0 ]; then
        rm -rf ${LOG_DIR}
    fi
    mkdir -m 755 -p ${LOG_DIR}
fi

trap "trap_cleanup_exit" HUP INT QUIT PIPE TERM

vlecho 1 "Starting remanufacture, pass ${PASS_VAL} with args: ${ALL_ARGS}"
vlecho 1 "Self args: ${SELF_ARGS}"

# "daemonize"
if [ ${PASS_VAL} -eq 0 ]; then
    sync
 # Juniper added remanufacture_graft_5
    if [ "$HAVE_REMANUFACTURE_GRAFT_5" = "y" ]; then
        remanufacture_graft_5
    fi

    if [ ${LOG_OUTPUT} -ne 0 ]; then
        rm -rf ${LOG_DIR}
        mkdir -m 755 -p ${LOG_DIR}
    fi

    vlecho 1 "Starting ${SELF} pass ${PASS_VAL} ..."

    # First make sure that the image file exists and is a valid image
    if [ ! -z "${SYSIMAGE_FILE}" ]; then

        # Note: we're passing -m to tpkg_query, which means we'll be
        # using the (quicker) but less secure image hash verification
        # here.  That's okay, as we're about to do it all again anyway,
        # once writeimage starts.  We just want to avoid a reboot on a
        # clearly corrupted image.

        FAILURE=0
        ${TPKG_QUERY} -t -m -f ${SYSIMAGE_FILE} || FAILURE=1
        if [ ${FAILURE} -ne 0 ]; then
            vlechoerr -1 "*** Invalid image: ${SYSIMAGE_FILE}"
            exit 1
        fi
    fi

    # Make sure we're not being run from a remanufacture environment
    CMDLINE_DEV=`cat /proc/cmdline | grep 'root=' | sed 's/^.*root=\([^ ]*\).*$/\1/'`
    # XXX #dep/parse: mount
    BOOTED_ROOT_DEV=`/bin/mount | grep '^.* on / type .*$' | awk '{print $1}'`

    # The root dev may need to be mapped to a real device name (LABEL= or UUID= )
    if strstr "${CMDLINE_DEV}" "=" ; then
        CMDLINE_DEV=`blkid -l -o device -t "${CMDLINE_DEV}"`
    fi

    # See if using device number for root
    if [ ! -z "${CMDLINE_DEV}" -a ! -z "${BOOTED_ROOT_DEV}" -a "${CMDLINE_DEV}" != "${BOOTED_ROOT_DEV}" ]; then
        echo "${CMDLINE_DEV}" | grep -q '^[0-9][0-9]*:[0-9][0-9]*$'
        if [ $? -eq 0 ]; then
            alt_booted=$(printf "%d:%d\n" `stat --format='0x%t 0x%T' "${BOOTED_ROOT_DEV}"`)

            if [ "${CMDLINE_DEV}" = "${alt_booted}" ]; then
                CMDLINE_DEV="${BOOTED_ROOT_DEV}"
            fi
        fi
    fi

    if [ "x${CMDLINE_DEV}" != "x${BOOTED_ROOT_DEV}" ]; then
        vlechoerr -1 "*** Invalid root devices: booted ${CMDLINE_DEV} now ${BOOTED_ROOT_DEV}"
        exit 1
    fi

    vlecho -1 "Remanufacture starting in background.  You will be logged off,"
    vlecho -1 "and the system will reboot when the remanufacture is complete."

    /bin/sh ${SELF} ${SELF_ARGS} -z 1 > /dev/null 2>&1 < /dev/null &
    exit 0
fi

vlecho 1 "remanufacture pass ${PASS_VAL}"

# Called for programmatic or debugging work using the -x flag
handle_external_remanufacture() {
    FAILURE=0
    if [ ${EXTERNAL_REMANUFACTURE} != "-" ]; then
        ${EXTERNAL_REMANUFACTURE} || FAILURE=1
    fi
    sync

    if [ ${FAILURE} -eq 1 ]; then
        vlechoerr -1 "*** External remanufacture failed"
        die_reboot
    else
        if [ ${EXTERNAL_INTERACTIVE} -ne 1 ]; then
            if [ ${EXIT_WHEN_DONE} -ne 1 ]; then
                vlecho -1 "Successfully remanufactured.  Rebooting..."
                sync
                # Juniper added the sleep 5
                sleep 5
                /sbin/reboot -f
            else
                vlecho -1 "Successfully remanufactured.  Exiting."
                sync
                exit 0
            fi
        else
            vlecho -1 "Successfully remanufactured.  Exiting."
            sync
        fi

        exit 0
    fi

    exit 1
}

#
# Make all the partitions we think we might need, since we're getting rid of
# udev.  We do this so it won't try to run programs while we're 0'ing and
# creating disk paritions, potentially making the disk busy.
#
make_disk_devices() {

    devdir=/dev

    # ide hda
    for i in `seq 1 15`; do
        [ -e ${devdir}/hda$i ] || mknod -m 640 ${devdir}/hda$i b 3 $i
    done
    [ -e ${devdir}/hda ] || mknod -m 640 ${devdir}/hda b 3 0

    # ide hdb
    for i in `seq 1 15`; do
        [ -e ${devdir}/hdb$i ] || mknod -m 640 ${devdir}/hdb$i b 3 `expr 64 + $i`
    done
    [ -e ${devdir}/hdb ] || mknod -m 640 ${devdir}/hdb b 3 64

    # ide hdc
    for i in `seq 1 15`; do
        [ -e ${devdir}/hdc$i ] || mknod -m 640 ${devdir}/hdc$i b 22 $i
    done
    [ -e ${devdir}/hdc ] || mknod -m 640 ${devdir}/hdc b 22 0

    # ide hdd
    for i in `seq 1 15`; do
        [ -e ${devdir}/hdd$i ] || mknod -m 640 ${devdir}/hdd$i b 22 `expr 64 + $i`
    done
    [ -e ${devdir}/hdd ] || mknod -m 640 ${devdir}/hdd b 22 64

    # scsi sda
    for i in `seq 0 15`; do
        [ -e ${devdir}/sda$i ] || mknod -m 640 ${devdir}/sda$i b 8 `expr 16 \* 0 + $i`
    done
    [ -e ${devdir}/sda ] || mknod -m 640 ${devdir}/sda b 8 `expr 16 \* 0 + 0`

    # scsi sdb
    for i in `seq 0 15`; do
        [ -e ${devdir}/sdb$i ] || mknod -m 640 ${devdir}/sdb$i b 8 `expr 16 \* 1 + $i`
    done
    [ -e ${devdir}/sdb ] || mknod -m 640 ${devdir}/sdb b 8 `expr 16 \* 1 + 0`

    # scsi sdc
    for i in `seq 0 15`; do
        [ -e ${devdir}/sdc$i ] || mknod -m 640 ${devdir}/sdc$i b 8 `expr 16 \* 2 + $i`
    done
    [ -e ${devdir}/sdc ] || mknod -m 640 ${devdir}/sdc b 8 `expr 16 \* 2 + 0`

    # scsi sdd
    for i in `seq 0 15`; do
        [ -e ${devdir}/sdd$i ] || mknod -m 640 ${devdir}/sdd$i b 8 `expr 16 \* 3 + $i`
    done
    [ -e ${devdir}/sdd ] || mknod -m 640 ${devdir}/sdd b 8 `expr 16 \* 3 + 0`
}

# NOTE: /rootfs as our tmpfs mount point is hard-coded here
part_get_remaining() {
    awk '$2 ~ /^\/$|^\/proc|^\/sys|^\/dev|^\/rootfs/ {next}
    $3 == "tmpfs" || $3 == "proc" {print $2 ; next}
    /(^#|loopfs|autofs|devfs|^none|^\/dev\/ram)/ {next}
        {print $2}' /proc/mounts
}

part_get_swaps() {
    if [ -e /proc/swaps ]; then
        awk '! /^Filename/ { print $1 }' /proc/swaps
    fi
}


NEED_REBOOT=1

# Pass 1 is before and while doing the pivot_root / chroot
if [ ${PASS_VAL} -eq 1 ]; then
    ROOTFS_SIZE=400

    # This sets the global OUTPUT_DEV
    find_output_dev

    mount -oremount,rw /
# Juniper added remove of /oldroot
    rm -rf /oldroot
    mkdir -p /oldroot

    # NOTE: the /rootfs name is hard-coded in part_get_remaining() above

    umount /rootfs
    rm -rf /rootfs
    mkdir -p /rootfs

 # Juniper added remanufacture_graft_6
    # Customer-specific configuration, for changing ROOTFS_SIZE
    if [ "$HAVE_REMANUFACTURE_GRAFT_6" = "y" ]; then
        remanufacture_graft_6
    fi

    # Set up a tmpfs that will be the new root file system
    FAILURE=0
    mount -t tmpfs -o size=${ROOTFS_SIZE}M tmpfs /rootfs || FAILURE=1
    if [ ${FAILURE} -eq 1 ]; then
        vlechoerr -1 "*** could not make working space in /rootfs.  Remanufacture failed."
        exit 1
    fi
    
    cat /proc/mounts > /rootfs/prev_proc_mounts

    # Give mgmtd a chance to tell the user what's happening
    vlecho 1 "Delaying to allow for remanufacture notification"
    sleep 2


    # Shut down all the processes we can, including our parent.  
    /sbin/service pm stop
    /sbin/service smartd stop
    /sbin/service syslog stop
    
    # Get rid of udevd, to make sure it doesn't busy the disk
    if [ -f /proc/sys/kernel/hotplug ]; then
        echo "" > /proc/sys/kernel/hotplug
    fi
    killall udevd

    # Make sure all the device files for common disks exist
    make_disk_devices
    

    # We would have used 'telinit 1' instead, but it starts a shell we
    # do not want.  
    # 
    # Instead, in firstboot we enforce that we only run 'normal'
    # things at runlevel 3, and that runlevels 2, 4 and 5 are special,
    # and not for customer use.  Customers must not chkconfig normal
    # things to any level but 3, and must not change the default run level
    # to any level but 3!
    # 
    # At runlevel 2 we run nothing, including no getty's.  At runlevel 4
    # we just run getty's, but nothing else.

    if [ "${DO_TELINIT}" = "1" ]; then
        DID_TELINIT=1
        /sbin/telinit 2
    fi

    #
    # Customer-specific shutdown or cleanup.  Also, customer may set
    # REMFG_EXTRA_FILTER_ROOTFS to an egrep expression to filter.
    #
    if [ "$HAVE_REMANUFACTURE_GRAFT_4" = "y" ]; then
        remanufacture_graft_4
    fi

    # minilogd just keeps poping up when you least want it!
    killall -9 minilogd

    # Try to kill off all processes that are left, just using TERM
    killall5 -15

    sync

    # Copy over everything we need into the tmpfs

    # First /, but not all of /opt/ , and not a couple other big files
    FAILURE=0
    find / -xdev -print | egrep -v -e '^/opt/|/httpd$|^/usr/.*bin/snmp.*$|^/rootfs/|^/usr/include/|\.a$|^/usr/lib.*/(perl|python|gcc|valgrind).*/|^/src/|^/usr/local/|^/usr/share/emacs|^/usr/bin/emacs.*|^/usr/java/|^/usr/lib/jvm/|/core\.[0-9]*$|^/usr/libexec/qemu-kvm$|^/usr/share/zoneinfo/|^/usr/share/kvm/|^/usr/share/libvirt|^/usr/share/X11|^/etc/MANIFEST$|^/usr/libexec/ipsec/|^/usr/sbin/racoon.*|^/boot/' ${REMFG_EXTRA_FILTER_ROOTFS:+"-e"} ${REMFG_EXTRA_FILTER_ROOTFS:+"${REMFG_EXTRA_FILTER_ROOTFS}"} | cpio -paBdmu /rootfs || FAILURE=1
    find /opt/tms/bin | egrep '/mddbreq|/genlicense|/eetool|nvstool' | cpio -paBdmu /rootfs || FAILURE=1
    find /opt/tms/lib/security | cpio -paBdmu /rootfs || FAILURE=1
    find /opt/tms/release | cpio -paBdmu /rootfs || FAILURE=1
    find /var/opt/tms/output | cpio -paBdmu /rootfs || FAILURE=1
    find /var/lib | cpio -paBdmu /rootfs || FAILURE=1
    find /var/run | cpio -paBdmu /rootfs || FAILURE=1
    find /var/spool | cpio -paBdmu /rootfs || FAILURE=1
    if [ ${COPY_CONFIG} -eq 1 ]; then
        find /config | cpio -paBdmu /rootfs || FAILURE=1
    else
        find /config/mfg | cpio -paBdmu /rootfs || FAILURE=1
    fi
    find /bootmgr | cpio -paBdmu /rootfs || FAILURE=1
    mkdir -m  755 -p /rootfs/var/empty || FAILURE=1
    mkdir -m  111 -p /rootfs/var/empty/sshd || FAILURE=1
    mkdir -m  755 -p /rootfs/var/home || FAILURE=1
    mkdir -m  755 -p /rootfs/var/home/root || FAILURE=1
    mkdir -m  755 -p /rootfs/var/home/monitor || FAILURE=1
    mkdir -m 1777 -p /rootfs/var/lock || FAILURE=1
    mkdir -m  755 -p /rootfs/var/lock/subsys || FAILURE=1
    mkdir -m  755 -p /rootfs/var/log || FAILURE=1
    mkdir -m  755 -p /rootfs/var/root || FAILURE=1
    mkdir -m 1777 -p /rootfs/var/root/tmp || FAILURE=1
    mkdir -m 1777 -p /rootfs/var/tmp || FAILURE=1

    if [ ${FAILURE} -ne 0 ]; then
        vlechoerr -1 "*** could not copy files into /rootfs.  Remanufacture failed."
        die_reboot
    fi

    # Copy over the image file
    if [ ! -z "${SYSIMAGE_FILE}" ]; then
        FAILURE=0
        echo "${SYSIMAGE_FILE}" | cpio -paBdmu /rootfs || FAILURE=1
        if [ ${FAILURE} -ne 0 ]; then
            vlechoerr -1 "*** could not copy image file into /rootfs.  Remanufacture failed."
            die_reboot
        fi
    fi

    # We need to get init to re-exec itself, but how depends on init
    init_is_upstart=0
    /sbin/telinit --version 2>&1 | grep -q upstart && init_is_upstart=1

    if [ "$HAVE_REMANUFACTURE_GRAFT_1" = "y" ]; then
        remanufacture_graft_1
    fi

    # Try to kill off everything again with TERM, and then with KILL
    sleep 2
    killall5 -15
    sleep 3
    killall5 -9

    vlecho 0 "Doing unmount and pivot..."

    # Copy over our log files, if any
    if [ ${LOG_OUTPUT} -ne 0 -a -d ${LOG_DIR} ]; then
        find ${LOG_DIR} | cpio -paBdmu /rootfs
    fi

    # Unmount as much as we can of the current system.  / will be busy.
    swapoff -a
    swaps=`part_get_swaps`
    if [ ! -z "${swaps}" ]; then
        swapoff ${swaps}
    fi
    umount -a -t ext2,ext3,ext4,jffs2,vfat
    remaining=`part_get_remaining | sort -r`
    if [ ! -z "${remaining}" ]; then
        umount ${remaining}
    fi
    umount -a -t ext2,ext3,ext4,jffs2,vfat
    mount -oremount,rw /
    swapoff -a
    swaps=`part_get_swaps`
    if [ ! -z "${swaps}" ]; then
        swapoff ${swaps}
    fi

    if [ ! -z "${OUTPUT_DEV}" -a -e "${OUTPUT_DEV}" ]; then
        chroot_output_device=$(echo ${OUTPUT_DEV} | sed 's/^\///')
    else
        chroot_output_device=dev/null
    fi

    cd /rootfs
    /sbin/pivot_root . oldroot

    ##################################################
    # NOTE: the lines below until the EOF are run
    #       on their own, in the chroot.  The other
    #       functions in this file are NOT available.
    ##################################################
    exec /usr/sbin/chroot . /bin/sh <<-EOF > ${chroot_output_device} 2>&1
    mount --move /oldroot/proc /proc
    mount --move /oldroot/dev /dev
    mount --move /oldroot/sys /sys

    if [ "${init_is_upstart}" = "0" ]; then
        /sbin/telinit u
    else
        kill -TERM 1
    fi
    killall mingetty agetty
    rm -f /etc/mtab
    cat /proc/mounts > /etc/mtab

    if [ "${DO_TELINIT}" = "1" -a "${DID_TELINIT}" = "1" ]; then
        /sbin/telinit 4
    fi

    umount -a -t ext2,ext3,ext4,jffs2,vfat
    if [ "${init_is_upstart}" = "0" ]; then
        /sbin/telinit u
    else
        kill -TERM 1
    fi
    rm -f /etc/mtab
    cat /proc/mounts > /etc/mtab
    umount -a -t ext2,ext3,ext4,jffs2,vfat

    # We're done with the pivoting part, move on to pass 2
    (sleep 2;  /bin/sh ${SELF} ${SELF_ARGS} -z 2 > /dev/null 2>&1 < /dev/null ) &
    disown
    exit 0
EOF

fi

# Pass 2 is after the pivot_root/chroot.  We're only doing this for development
# reasons; it is not required, but otherwise it is too painful to test. 
#
# Mostly we'll just call the manufacture script.
#
#
if [ ${PASS_VAL} -eq 2 ]; then

    # Just in case, also for use in testing
    swapoff -a
    swaps=`part_get_swaps`
    if [ ! -z "${swaps}" ]; then
        swapoff ${swaps}
    fi
    umount -a -t ext2,ext3,ext4,jffs2,vfat
    killall -9 minilogd
    /sbin/service syslog restart
    killall -9 minilogd
    umount -a -t ext2,ext3,ext4,jffs2,vfat
    remaining=`part_get_remaining | sort -r`
    if [ ! -z "${remaining}" ]; then
        umount ${remaining}
    fi
    umount -a -t ext2,ext3,ext4,jffs2,vfat
    swapoff -a
    swaps=`part_get_swaps`
    if [ ! -z "${swaps}" ]; then
        swapoff ${swaps}
    fi

    # This sets the global OUTPUT_DEV
    find_output_dev

    if [ ! -z "${OUTPUT_DEV}" -a -e "${OUTPUT_DEV}" ]; then
        exec > ${OUTPUT_DEV} 2>&1

        # So we can read what is being echo'd out when not logged in
        clear
        stty -F ${OUTPUT_DEV} cooked -nl
        clear
    fi

    if [ ! -z "${OUTPUT_DEV}" ]; then
        vlecho -1 "Sending output to: ${OUTPUT_DEV}"
    else
        vlecho -1 "Could not determine valid console for output"
    fi

    # If there are any partitions left mounted, say so, and reboot
    remaining=`part_get_remaining | sort -r`
    if [ ! -z "${remaining}" ]; then
        vlechoerr -1 "*** Could not unmount remaining partitions: ${remaining}"

        PSO="$(ps -AwwL -o user,pid,lwp,ppid,nlwp,pcpu,pri,nice,vsize,rss,tty,stat,wchan:12,start,bsdtime,command)"

        vlechoerr -1 "*** Still open:"
        for rl in ${remaining}; do
            vlechoerr -1 "Remaining: ${rl}:"
            FUO="$(lsof ${rl})"
            vlechoerr -1 "${FUO}"
        done

        vlechoerr -1 "*** Still running:"
        vlechoerr -1 "${PSO}"

        die_reboot
    fi

    # Allow remanufacture (or other activities) to be done instead of
    # 'normal' manufacture
    if [ ! -z "${EXTERNAL_REMANUFACTURE}" ]; then
        vlecho -1 "=== Starting external remanufacture: ${EXTERNAL_REMANUFACTURE}"

        if [ ${EXTERNAL_INTERACTIVE} -eq 1 ]; then
            if [ ! -z "${OUTPUT_DEV}" ]; then
                exec /bin/sh ${SELF} ${SELF_ARGS} -z 4 > ${OUTPUT_DEV} 2>&1 < ${OUTPUT_DEV} &
            else
                exec /bin/sh ${SELF} ${SELF_ARGS} -z 4 &
            fi
        fi

        # Never returns, always exits or exec's
        handle_external_remanufacture

        die_reboot
    fi

    # XXX this could be in a different script from here on down.

    vlecho -1 "=================================================="
    vlecho -1 "Remanufacture: manufacturing the system"
    vlecho -1 "=================================================="

    # Make sure that the image file still exists
    if [ -z "${SYSIMAGE_FILE}" -o ! -f ${SYSIMAGE_FILE} ]; then
        vlechoerr -1 "*** No image file: ${SYSIMAGE_FILE}"
        die_reboot
    fi

    # Get model number from mfg db  (XXX could want command line override)
    model=
    FAILURE=0
    if [ -z "${model}" ]; then
        model=`${MDDBREQ} -v ${MFG_DB_PATH} query get "" /mfg/mfdb/system/model` || FAILURE=1
    fi
    if [ -z "${model}" -o ${FAILURE} -eq 1 ]; then
        vlechoerr -1 "*** Bad model information"
        die_reboot
    fi

    # Get hostid
    hostid=
    HOSTID_ARGS=
    if [ -z "${hostid}" ]; then
        FAILURE=0
        hostid=`${MDDBREQ} -v ${MFG_DB_PATH} query get "" /mfg/mfdb/system/hostid` || FAILURE=1
        if [ ${FAILURE} -eq 1 ]; then
            vlechoerr -1 "*** Bad hostid information"
            die_reboot
        fi
        HOSTID_ARGS="-h ${hostid}"
    fi

    DO_MFG=1
    FAILURE=0
    # By default, we specify -t , so we won't try to use tmpfs
    MFG_ARGS="-t"

    vlecho 0 "About to start manufacturing"

    # Graft point for customers to call manufacture.sh themselves (and
    # set DO_MFG=1 so we won't run it as well), or to add arguments to
    # our call by changing MFG_ARGS

    if [ "$HAVE_REMANUFACTURE_GRAFT_2" = "y" ]; then
        remanufacture_graft_2
    fi

    if [ ${DO_MFG} -eq 1 ]; then
        FAILURE=0
        ${MANUFACTURE} ${VERBOSE_ARGS} -a -m "${model}" ${HOSTID_ARGS} -f "${SYSIMAGE_FILE}" ${MFG_BASE_ARGS} ${MFG_ARGS} -x "${SELF} ${SELF_ARGS} -z 3" || FAILURE=1

    fi        
 # Juniper added remanufacture_graft_7
    if [ "$HAVE_REMANUFACTURE_GRAFT_7" = "y" ]; then
        remanufacture_graft_7
    fi

    if [ ${FAILURE} -eq 1 ]; then
        vlechoerr -1 "*** manufacture failed"
        die_reboot
    fi
    
    if [ ${EXIT_WHEN_DONE} -ne 1 ]; then
        vlecho -1 "Successfully remanufactured.  Rebooting..."
        sync
        # Juniper added the sleep 5
        sleep 5
        /sbin/reboot -f
    else
        vlecho -1 "Successfully remanufactured.  Exiting."
        sync
        exit 0
    fi

    exit 0
fi

# This pass is called from manufacture.sh with all the partitions mounted.
# It is our chance to do things like copy over config.  The new system is 
# mounted on ${MFG_MOUNT} .
if [ ${PASS_VAL} -eq 3 ]; then

    if [ ${COPY_CONFIG} -eq 1 ]; then
        vlecho 0 "== Copying config from previous system"

        cp -a ${CONFIG_DB_DIR}/* ${NEW_MFG_MOUNT}/${CONFIG_DB_DIR}/
    fi

    # Graft point for customers to update mfg db from previous mfg db, etc.
    if [ "$HAVE_REMANUFACTURE_GRAFT_3" = "y" ]; then
        remanufacture_graft_3
    fi

fi

# This pass is called when we have an external interactive script we are running
if [ ${PASS_VAL} -eq 4 ]; then
    handle_external_remanufacture
fi


exit 0
