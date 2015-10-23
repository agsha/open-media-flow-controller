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

PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

umask 0022


usage()
{
    echo "usage: $0 {-r|-w}"
    exit 1
}

# returns OK if $1 contains $2
strstr() {
    case "${1}" in 
        *${2}*) return 0;;
    esac
    return 1
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

trap "cleanup_exit" HUP INT QUIT PIPE TERM

# ==================================================
# Cleanup when we are finished or interrupted
# ==================================================
cleanup()
{
    FAILURE=0
    if [ ! -z "${UNMOUNT_VPART}" ]; then
        umount ${MOUNT_EARGS} ${UNMOUNT_VPART} || FAILURE=1
        if [ ${FAILURE} -eq 1 ]; then
            echo "*** Could not umount ${UNMOUNT_VPART}"
        fi
        if [ ${DO_LOOP_DEV_DEL} -eq 1 ]; then
            losetup -d ${LOOP_DEV} > /dev/null 2>&1
        fi
    fi

    FAILURE=0
    # Remount the backing partition ro
    if [ ! -z "${UNMOUNT_VPART_PHYS}" ]; then
        if [ ${DO_REMOUNT} -ne 1 ]; then
            umount ${MOUNT_EARGS} ${UNMOUNT_VPART_PHYS} || FAILURE=1
            if [ ${FAILURE} -eq 1 ]; then
                echo "*** Could not unmount partition ${UNMOUNT_VPART_PHYS}"
            fi
        else
            mount -o remount,ro ${UNMOUNT_VPART_PHYS} || FAILURE=1
            if [ ${FAILURE} -eq 1 ]; then
                echo "*** Could not remount partition ${UNMOUNT_VPART_PHYS} ro"
            fi
        fi
    fi

    vecho 0 "$0: done"
}


# ==================================================
# Cleanup when called from 'trap' for ^C, signal, or fatal error
# ==================================================
cleanup_exit()
{
    cleanup
    exit 1
}

# ========================================================================
# Function to mount a virtual file system image onto a specified
# directory. The physical partition which is the backing store location
# should already be mounted, but may need to be remounted if a write
# is required.
# ========================================================================
do_mount_vpart()
{
    vpart=$1
    tmount_dir=$2
    do_rw_mount=$3

    lcl_do_remount=${DO_REMOUNT}

    eval 'part_name="${IL_LO_'${IL_LAYOUT}'_VPART_'${vpart}'_PART_NAME}"'
    vecho 0 "vpart= ${vpart} backing part=${part_name}"
        
    eval 'vp_file_name="${IL_LO_'${IL_LAYOUT}'_VPART_'${vpart}'_FILE_NAME}"'
    eval 'dev="${IL_LO_'${IL_LAYOUT}'_PART_'${part_name}'_DEV}"'
    eval 'bk_part_mount="${IL_LO_'${IL_LAYOUT}'_PART_'${part_name}'_MOUNT}"'

    if [ -z "${part_name}" -o -z "${vp_file_name}" ]; then
        echo "*** Configuration error for virtual ${vpart}"
        cleanup_exit
    fi

    if [ -z "${dev}" -o -z "${bk_part_mount}" ]; then
        echo "*** No physical partition for virtual ${vpart}"
        cleanup_exit
    fi

    if [ ! -d "${bk_part_mount}" ]; then
        # XXX should this have been ensured by manufacture?
        mkdir -p ${bk_part_mount}

        # If the mount didn't exist, a good bet the partition wasn't mounted
        lcl_do_remount=0
    fi

    # XXX #dep/parse: tune2fs
    mount_count=`tune2fs -l ${dev} | grep 'Mount.*count' | awk '{print $3}'`
    max_mount_count=`tune2fs -l ${dev} | grep 'Maximum.*mount.*count' | awk '{print $4}'`

    if [ ! -z "${mount_count}" -a ! -z "${max_mount_count}" ]; then
        mounts_left=`expr ${max_mount_count} - ${mount_count}`
    else
        mounts_left=0
    fi

    if [ ${mounts_left} -le 1 ]; then
        vecho 0 "-- Force fsck on ${dev}"
        if [ ${lcl_do_remount} -eq 1 ]; then
            FAILURE=0
            umount ${bk_part_mount} || FAILURE=1
            if [ ${FAILURE} -eq 1 ]; then
                echo "*** Could not umount partition ${bk_part_mount} for fsck"
                cleanup_exit
            fi
        fi
        fsck -T -a ${dev} > /dev/null 2>&1
        if [ ${lcl_do_remount} -eq 1 ]; then
            # Must force a mount now
            vecho 0 "== Mount backing partition ${dev} on ${bk_part_mount} after fsck"
            FAILURE=0
            mount ${MOUNT_EARGS} ${dev} ${bk_part_mount} || FAILURE=1
            if [ ${FAILURE} -eq 1 ]; then
                echo "*** Could not mount partition ${dev} on ${bk_part_mount} after fsck"
                cleanup_exit
            fi
        fi
    fi

    if [ ${do_rw_mount} -eq 1 ]; then
        # Remount the backing partition
        FAILURE=0

        if [ ${lcl_do_remount} -ne 1 ]; then
            vecho 0 "== Mount backing partition ${dev} on ${bk_part_mount}"

            mount ${MOUNT_EARGS} ${dev} ${bk_part_mount} || FAILURE=1
            if [ ${FAILURE} -eq 1 ]; then
                echo "*** Could not mount partition ${dev} on ${bk_part_mount} for writing"
            fi
        else
            vecho 0 "== Remount backing partition rw ${dev} on ${bk_part_mount}"

            mount -o remount,rw ${bk_part_mount} || FAILURE=1
            if [ ${FAILURE} -eq 1 ]; then
                echo "*** Could not remount partition ${dev} on ${bk_part_mount} for writing"
            fi

        fi
        if [ ${FAILURE} -eq 1 ]; then
            cleanup_exit
        fi
        UNMOUNT_VPART_PHYS=${bk_part_mount}
    fi

    vp_path=${bk_part_mount}/${vp_file_name}

    if [ ${DO_ROOTFS_HALT} -ne 1 ]; then
        mkdir -p ${tmount_dir}
    fi

    # XXX #dep/parse: tune2fs
    mount_count=`tune2fs -l ${vp_path} | grep 'Mount.*count' | awk '{print $3}'`
    max_mount_count=`tune2fs -l ${vp_path} | grep 'Maximum.*mount.*count' | awk '{print $4}'`

    if [ ! -z "${mount_count}" -a ! -z "${max_mount_count}" ]; then
        mounts_left=`expr ${max_mount_count} - ${mount_count}`
    else
        mounts_left=0
    fi

    if [ ${mounts_left} -le 1 -o ${DO_FSCK} -eq 1 ]; then
        vecho 0 "-- Running fsck on ${vp_path}"
        fsck -T -a ${vp_path} > /dev/null 2>&1
    fi

    FAILURE=0
    mount ${MOUNT_EARGS} -o loop=${LOOP_DEV},noatime -t ext2 ${vp_path} ${tmount_dir} || FAILURE=1
    if [ ${FAILURE} -eq 1 ]; then
        echo "*** Could not mount partition ${vp_path} on ${tmount_dir}"
        cleanup_exit
    fi

    vecho 0 "== Mounted virtual fs image in ${vp_path} for ${vpart} on ${tmount_dir}"

    UNMOUNT_VPART=${tmount_dir}
}

# ========================================================================
# Function to unmount a virtual file system image
# ========================================================================
do_umount_vpart()
{
    # Get to someplace presumably out of the way
    cd /

    # Must unmount loopback mount point first
    if [ ! -z "${UNMOUNT_VPART}" ]; then
        vecho 1 "-- unmount ${UNMOUNT_VPART}"
        umount ${MOUNT_EARGS} ${UNMOUNT_VPART}
        UNMOUNT_VPART=
        if [ ${DO_LOOP_DEV_DEL} -eq 1 ]; then
            losetup -d ${LOOP_DEV} > /dev/null 2>&1
        fi
    fi

    # Remount the backing partition ro
    if [ ! -z "${UNMOUNT_VPART_PHYS}" ]; then
        FAILURE=0
        if [ ${DO_REMOUNT} -ne 1 ]; then
            vecho 1 "-- unmount ${UNMOUNT_VPART_PHYS}"
            umount ${MOUNT_EARGS} ${UNMOUNT_VPART_PHYS} || FAILURE=1
            if [ ${FAILURE} -eq 1 ]; then
                echo "*** Could not unmount partition ${UNMOUNT_VPART_PHYS}"
            fi
        else
            vecho 1 "-- remount ${UNMOUNT_VPART_PHYS} ro"
            mount -o remount,ro ${UNMOUNT_VPART_PHYS} || FAILURE=1
            if [ ${FAILURE} -eq 1 ]; then
                echo "*** Could not remount partition ${UNMOUNT_VPART_PHYS} ro"
            fi
        fi
        UNMOUNT_VPART_PHYS=
        if [ ${FAILURE} -eq 1 ]; then
            cleanup_exit
        fi
    fi
}


# ==================================================
# Parse command line arguments, setup
# ==================================================

PARSE=`/usr/bin/getopt 'rwhfvl' "$@"`

if [ $? != 0 ] ; then
    usage
fi

eval set -- "$PARSE"


# Defaults
DO_READ=-1
DO_WRITE=-1
VERBOSE=0
DO_ROOTFS_HALT=0
DO_LOOP_DEV_DEL=1
DO_REMOUNT=1
DO_FSCK=0
RSYNC_VERBOSE=

MOUNT_EARGS=


# Don't make VPART_LOOP_MOUNT a directory that must be created
# as most likely at boot time this is run with root r/o and
# possibly /tmp not pointing to anything yet.
VPART_LOOP_MOUNT=/mnt

UNMOUNT_VPART_PHYS=
UNMOUNT_VPART=

while true ; do
    case "$1" in
        -r) DO_READ=1; shift ;;
        -w) DO_WRITE=1; shift ;;
        -h) DO_ROOTFS_HALT=1; shift;;
        -f) DO_FSCK=1; shift ;;
        -l) DO_LOOP_DEV_DEL=0; shift ;;
        -v) VERBOSE=$((${VERBOSE}+1)); shift ;;
        --) shift ; break ;;
        *) echo "vpart.sh: parse failure" >&2 ; usage ;;
    esac
done

# Test all the options that got set for correctness

if [ ${DO_READ} -eq -1 -a ${DO_WRITE} -eq -1 ]; then
    usage
fi
if [ ${DO_READ} -eq 1 -a ${DO_WRITE} -eq 1 ]; then
    usage
fi

if [ ${VERBOSE} -gt 1 ]; then
    RSYNC_VERBOSE=-v
fi

# Determine what virtual partitions need to be loaded in to
# the respective tmpfs's
. /etc/image_layout.sh
. /etc/layout_settings.sh
. /etc/image_layout.sh

vecho 0 "Running: $0"

eval 'all_vparts="${IL_LO_'${IL_LAYOUT}'_VPARTS}"'

vecho 0 "all_vparts= ${all_vparts}"

if [ -z "${all_vparts}" ]; then
    cleanup
    exit 0
fi

found_loopdev=0
        
for i in `seq 0 7`; do
    losetup /dev/loop$i > /dev/null 2>&1
    if [ $? -eq 1 ]; then
        loop_devnum=$i
        found_loopdev=1
        break
    fi
done

if [ $found_loopdev -eq 0 ]; then
    echo "*** Error no loop devices available"
    logger -p user.err "Error no loop devices available"
    cleanup_exit
else
    vecho 1 "Use loop device /dev/loop$loop_devnum"
    LOOP_DEV="/dev/loop$loop_devnum"
fi

if [ ${DO_ROOTFS_HALT} -eq 1 ]; then
    # Can not create a mount point, just use /mnt
    VPART_LOOP_MOUNT=/mnt

    # When called like this, assume everything is umounted
    DO_REMOUNT=0
fi

eval `/sbin/aiget.sh`
eval 'NAME_ROOT="${IL_LO_'${IL_LAYOUT}'_LOC_ROOT_'${AIG_THIS_BOOT_ID}'_VPART}"'

for vpart in ${all_vparts}; do
    if [ ${DO_READ} -eq 1 ]; then
        eval 'is_rootfs="${IL_LO_'${IL_LAYOUT}'_VPART_'${vpart}'_ISROOTFS}"'
        if [ -z "${is_rootfs}" ]; then
            is_rootfs=0
        fi

        if [ ${is_rootfs} -eq 1 ]; then
            vecho 0 "-- Skipping read for root fs ${vpart} "
            continue
        fi

        eval 'vpart_mount="${IL_LO_'${IL_LAYOUT}'_VPART_'${vpart}'_MOUNT}"'

        vecho 0 "Reading in virtual part ${vpart}"

        do_mount_vpart ${vpart} ${VPART_LOOP_MOUNT} 0

        # The mount points are always assumed to start from /
        cd ${vpart_mount}

        # REMXXX old (cd ${VPART_LOOP_MOUNT}; tar -cpf - .) | tar -xpf -
        (cd ${VPART_LOOP_MOUNT}; find . -print0 | cpio -p -0 -a -B -d -m -u ${vpart_mount})

        do_umount_vpart

        vecho 0 "== Done setting up ${vp_path} using ${tmount_dir}"
    else
        eval 'do_writeback="${IL_LO_'${IL_LAYOUT}'_VPART_'${vpart}'_DO_WRITEBACK}"'
        if [ -z "${do_writeback}" ]; then
            do_writeback=0
        fi

        eval 'is_rootfs="${IL_LO_'${IL_LAYOUT}'_VPART_'${vpart}'_ISROOTFS}"'
        if [ -z "${is_rootfs}" ]; then
            is_rootfs=0
        fi

        if [ ${do_writeback} -ne 1 ]; then
            vecho 0 "-- Skipping write back for ${vpart} "
            continue
        fi

        if strstr "${vpart}" ROOT ; then
            if [ "${vpart##ROOT}" != "${NAME_ROOT##ROOT}" ]; then
                # Only write back current root to booted root location
                vecho 0 "-- Skipping write for non active root fs ${vpart} "
                continue
            fi
        fi

        if [ ${DO_ROOTFS_HALT} -eq 1 -a ${is_rootfs} -ne 1 ]; then
            vecho 0 "-- Skipping write for non-root fs ${vpart} "
            continue
        fi

        do_mount_vpart ${vpart} ${VPART_LOOP_MOUNT} 1

        eval 'mount_name="${IL_LO_'${IL_LAYOUT}'_VPART_'${vpart}'_MOUNT}"'
        eval 'wb_excludes="${IL_LO_'${IL_LAYOUT}'_VPART_'${vpart}'_WRITEBACK_EXCLUDE}"'

        cd ${VPART_LOOP_MOUNT}

        rsync_ex_arg=""
        if [ ! -z "${wb_excludes}" ]; then
            for xa in ${wb_excludes}; do
                new_rsync_ex="--exclude=${xa}"
                rsync_ex_arg="${rsync_ex_arg} ${new_rsync_ex}"
            done
        fi

        vecho 1 "rsync_ex_arg= ${rsync_ex_arg}"

        # Write back what's changed

        # We'll try to do an rsync with the first list excluded, and see
        # if it works.  If it doesn't, we'll try again with the same
        # list, but having rsync delete the excluded files first.  If
        # this fails, we'll move to the next list, and start again.  We
        # put the things we're most willing to lose in the earlier
        # lists.

        EXCLUDE_LIST_NAMES="1 2 3 4"
        EXCLUDE_LIST_1='--exclude=tmp/ --exclude=root/tmp/ --exclude=opt/tms/web/graphs/'

        EXCLUDE_LIST_2="${EXCLUDE_LIST_1}"' --exclude=log/messages.* --exclude=log/web_*_log.* --exclude=log/sa/ --exclude=opt/tms/snapshots/ --exclude=opt/tms/sysdumps/ --exclude=opt/tms/images/'

        EXCLUDE_LIST_3="${EXCLUDE_LIST_2}"' --exclude=log/messages* --exclude=log/web_*_log* --exclude=opt/tms/tcpdumps/ --exclude=opt/tms/stats/reports/ '

        EXCLUDE_LIST_4="${EXCLUDE_LIST_3}"' --exclude=home/'

        MAX_SIZE=1M

        vecho 0 "== Deleting missing files from backing store"

        # First, delete missing files from the backing store
        FAILURE=0
        do_verbose rsync ${RSYNC_VERBOSE} -a -O --delete --ignore-existing --ignore-non-existing ${mount_name}/ . || FAILURE=1
        if [ ${FAILURE} -eq 1 ]; then
            echo "*** Could not write back changes for deletes."
            logger -p user.err "Could not write back changes for deletes"
        fi

        # Try to do excluding the "less important" parts
        for xln in ${EXCLUDE_LIST_NAMES}; do
            eval 'xl="${EXCLUDE_LIST_'${xln}'}"'

            ## vecho 1 == "XL[${xln}]: ${xl}"
            
            vecho 0 "== Writing back system files to the backing store, exclude group ${xln}"
            FAILURE=0
            do_verbose rsync ${RSYNC_VERBOSE} -a --delete ${rsync_ex_arg} ${xl} ${mount_name}/ . || FAILURE=1
            if [ ${FAILURE} -eq 1 ]; then
                vecho 0 "== Writing back system files, exclude group ${xln} to the backing store, with delete excludes"
                # Try again, deleting the excluded files from the backing
                FAILURE=0
                do_verbose rsync ${RSYNC_VERBOSE} -a --delete --delete-excluded ${rsync_ex_arg} ${xl} ${mount_name}/ . || FAILURE=1

            fi

            if [ ${FAILURE} -eq 0 ]; then
                break
            fi
        done

        if [ ${FAILURE} -eq 1 ]; then
            echo "*** Could not write back some system file changes."
            logger -p user.err "Could not write back some system file changes"
        fi


        # Try to do all small files.  We do this before the big ones
        # in hopes of losing fewer files if we run out of space
        vecho 0 "== Writing back all small changes to the backing store"
        FAILURE=0
        do_verbose rsync ${RSYNC_VERBOSE} -a --max-size ${MAX_SIZE} --delete ${rsync_ex_arg} ${mount_name}/ . || FAILURE=1
        if [ ${FAILURE} -eq 1 ]; then
            echo "*** Could not write back all small changes."
            logger -p user.err "Could not write back all small changes"
        fi

        # Try to do everything
        vecho 0 "== Writing back all changes to the backing store"
        FAILURE=0
        do_verbose rsync ${RSYNC_VERBOSE} -a --delete ${rsync_ex_arg} ${mount_name}/ . || FAILURE=1
        if [ ${FAILURE} -eq 1 ]; then
            echo "*** Could not write back all changes."
            logger -p user.err "Could not write back all changes"
        fi

        do_umount_vpart

        vecho 0 "== Done backing ${mount_name} to ${VPART_LOOP_MOUNT}"
    fi
done


# ==================================================
# Cleanup: delete temp files and potentially unmount
# ==================================================

cleanup

vecho 0 "==== vpart.sh finished."
