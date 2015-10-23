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
# This script is used to verify a .img file , or to query information about
# the image.  Internally these files are currently zips.  
#

PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

umask 0022


usage()
{
    echo "usage: $0 [-m] [-L LAYOUT] [-d /DEV/N1]"
    echo ""
    echo "-m: running at manufacture time, ignore any image_layout.sh"
    echo ""
    echo "-L LAYOUT_TYPE: STD or other defined layout"
    echo ""
    echo "-w HWNAME: (mfg only) hardware name (usually optional on x86)"
    echo ""
    echo "-d DEVICE : device to put image on (i.e. /dev/hda); repeatable"
    echo ""
    echo "-F: one of the filesystems is FAT, so times may be +/- 1 second"
    echo ""
    echo "Note that in the manufacturing environment, you should specify"
    echo "    options that match how you manufactured the image, and should "
    echo "    specify nothing if in a running system."

    exit 1
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
# Cleanup when we are finished or interrupted
# ==================================================
cleanup()
{
    # Get out of any dirs we might remove
    cd /

    for ump in ${UNMOUNT_LIST}; do
        umount ${ump} > /dev/null 2>&1
    done

    # Unmount all the backing partitions
    for ump in ${UNMOUNT_BKP_LIST}; do
        umount ${ump} > /dev/null 2>&1
    done
}


# ==================================================
# Cleanup when called from 'trap' for ^C, signal, or fatal error
# ==================================================
cleanup_exit()
{
    cleanup
    exit 1
}


do_mount_vpart()
{
    vpart=$1
    tmount_dir=$2

    eval 'bkpart_name="${IL_LO_'${IL_LAYOUT}'_VPART_'${vpart}'_PART_NAME}"'
    eval 'vp_file_name="${IL_LO_'${IL_LAYOUT}'_VPART_'${vpart}'_FILE_NAME}"'
    eval 'fstype="${IL_LO_'${IL_LAYOUT}'_VPART_'${vpart}'_FSTYPE}"'

    if [ -z "${bkpart_name}" -o -z "${vp_file_name}" ]; then
        echo "*** Configuration error for virtual ${vpart}"
        continue
    fi

    eval 'part_mount="${IL_LO_'${IL_LAYOUT}'_PART_'${bkpart_name}'_MOUNT}"'

    phys_part_mount=${TMP_VMOUNT}/${part_mount}

    # Assume the backing partition has already been mounted

    vp_path=${phys_part_mount}/${vp_file_name}

    mkdir -p ${tmount_dir}

    FAILURE=0
    mount -o loop,noatime,ro -t ${fstype} ${vp_path} ${tmount_dir} || FAILURE=1
    if [ ${FAILURE} -eq 1 ]; then
        echo "*** Could not mount vpartition ${vp_path} on ${tmount_dir}"
        exit
        cleanup_exit
    fi
    UNMOUNT_LIST="${tmount_dir} ${UNMOUNT_LIST}"
}

# returns OK if $1 contains $2
strstr() {
    case "${1}" in 
        *${2}*) return 0;;
    esac
    return 1
}

PARSE=`/usr/bin/getopt 'mL:w:d:Fv' "$@"`

if [ $? != 0 ] ; then
    usage
fi

eval set -- "$PARSE"

IMAGE_LAYOUT=STD
HAVE_LAYOUT=0
HAVE_HWNAME=0
VERBOSE=0
HAVE_DEV_LIST=0
DEV_LIST=
DEV_COUNT=0
HAS_FAT=0
MANIFEST_EXTRA_EXCLUDES=
IGNORE_IMAGE_LAYOUT=0

# Define customer-specific graft functions
if [ -f /etc/customer_rootflop.sh ]; then
    . /etc/customer_rootflop.sh
fi

if [ "$HAVE_IMGVERIFY_GRAFT_1" = "y" ]; then
    imgverify_graft_1
fi

while true ; do
    case "$1" in
        -m) IGNORE_IMAGE_LAYOUT=1; shift ;;
        -L) HAVE_LAYOUT=1
            IMAGE_LAYOUT=$2; shift 2 ;;
        -w) HAVE_HWNAME=1
            HWNAME=$2; shift 2 ;;
        -v) VERBOSE=$((${VERBOSE}+1)); shift ;;
        -F) HAS_FAT=1; shift ;;
        -d) HAVE_DEV_LIST=1
            new_disk=$2; shift 2
            echo $new_disk | grep -q "^/dev"
            if [ $? -eq 1 ]; then
                vecho -1 "Problem with -d option"
                usage
            fi
            DEV_LIST="${DEV_LIST} ${new_disk}"
            DEV_COUNT=$((${DEV_COUNT} + 1))
            eval 'DEV_'${DEV_COUNT}'="${new_disk}"'
            ;;
        --) shift ; break ;;
        *) echo "imgverify.sh: parse failure" >&2 ; usage ;;
    esac
done

if [ ! -z "$*" ] ; then
    usage
fi

# See if we're in a running image or a bootfloppy
kern_cmdline="`cat /proc/cmdline`"
IS_BOOTFLOP=1
THIS_BOOT_ID=
if strstr "$kern_cmdline" img_id= ; then
    IS_BOOTFLOP=0

    for arg in $kern_cmdline ; do
        if [ "${arg##img_id=}" != "${arg}" ]; then
            THIS_BOOT_ID="${arg##img_id=}"
            break
        fi
    done

else
    IS_BOOTFLOP=1
fi

if [ ${HAVE_DEV_LIST} -eq 0 -a ${IS_BOOTFLOP} -eq 1 ]; then
    echo "Error: device list required in bootfloppy environment"
    usage
fi

if [ ! -f /etc/layout_settings.sh ]; then
    echo "Error: No valid layout settings information found."
    exit 1
fi

if [ ${IGNORE_IMAGE_LAYOUT} -eq 1 -o ! -f /etc/image_layout.sh ]; then
    # Set vars for using layout stuff
    IL_LAYOUT=${IMAGE_LAYOUT}
    export IL_LAYOUT
    if [ ! -z "${HWNAME}" ]; then
        IL_HWNAME=${HWNAME}
        export IL_HWNAME
    fi
fi

# Make sure we listen to anything image_layout.sh may have to say
# This should only be present in a running system.
if [ ${IGNORE_IMAGE_LAYOUT} -eq 0 -a -f /etc/image_layout.sh ]; then
    . /etc/image_layout.sh
    if [ ${HAVE_LAYOUT} -ne 0 ]; then
        vecho -1 "= Warning: ignoring command line layout"
    fi
    if [ ${HAVE_HWNAME} -ne 0 ]; then
        vecho -1 "= Warning: ignoring command line HWNAME"
    fi
    if [ ${HAVE_DEV_LIST} -ne 0 ]; then
        vecho -1 "= Warning: ignoring command line device list"
    fi
fi

. /etc/layout_settings.sh

vecho 0 "== Using layout: ${IL_LAYOUT}"

eval 'targets="${IL_LO_'${IL_LAYOUT}'_TARGETS}"'
eval 'parts="${IL_LO_'${IL_LAYOUT}'_PARTS}"'
if [ -z "${targets}" -o -z "${parts}" ]; then
    echo "Error: Invalid layout ${IL_LAYOUT} specified."
    exit 1
fi

if [ ${IGNORE_IMAGE_LAYOUT} -eq 1 -o ! -f /etc/image_layout.sh ]; then

    # Now associate the targets (devices) from the layout file with the devices
    # the user specified on the command line

    # Set TARGET_NAMES, which are things like 'DISK1'
    eval 'TARGET_NAMES="${IL_LO_'${IL_LAYOUT}'_TARGETS}"'
    dev_num=1
    for tn in ${TARGET_NAMES}; do
        if [ "${dev_num}" -le "${DEV_COUNT}" ]; then
            eval 'user_dev="${DEV_'${dev_num}'}"'
            eval 'IL_LO_'${IL_LAYOUT}'_TARGET_'${tn}'_DEV="${user_dev}"'
        else
            eval 'curr_dev="${IL_LO_'${IL_LAYOUT}'_TARGET_'${tn}'_DEV}"'
            if [ -z "${curr_dev}" ]; then
                echo "Error: not enough device targets specified."
                cleanup_exit
            fi
        fi
        dev_num=$((${dev_num} + 1))
    done
    vecho 0 "== Using dev list: ${DEV_LIST}"
else
    . /etc/image_layout.sh
    vecho 0 "== Using dev list from /etc/image_layout.sh"
fi


# Reread layout settings file, as we can only now reference target device
# names to get partition device name (now that we have the devices from
# the user).
. /etc/layout_settings.sh



### set -x

TMP_MNT_IMAGE=/tmp/mnt_image_iv
TMP_VMOUNT=/tmp/mnt_vmount
UNMOUNT_LIST=
UNMOUNT_BKP_LIST=

mkdir -p ${TMP_MNT_IMAGE}

trap "cleanup_exit" HUP INT QUIT PIPE TERM

# Build up the list of partitions in IMAGE_[1|2]_PART_LIST . These are
# suitably ordered for mounting because the layout LOCS are suitably ordered
# for mounting.
for inum in 1 2; do
    loc_list=""
    eval 'loc_list="${IL_LO_'${IL_LAYOUT}'_IMAGE_'${inum}'_LOCS}"'

    for loc in ${loc_list}; do
        add_part=""
        eval 'add_part="${IL_LO_'${IL_LAYOUT}'_LOC_'${loc}'_PART}"'

        if [ ! -z "${add_part}" ]; then
            # Only add it on if it is unique
            eval 'curr_list="${IMAGE_'${inum}'_PART_LIST}"'

            present=0
            echo "${curr_list}" | grep -q " ${add_part} " - && present=1
            if [ ${present} -eq 0 ]; then
                eval 'IMAGE_'${inum}'_PART_LIST="${IMAGE_'${inum}'_PART_LIST} ${add_part} "'
                eval 'IMAGE_'${inum}'_PART_TYPE_'${add_part}'="p"'
            fi
        fi

        add_vpart=""
        eval 'add_vpart="${IL_LO_'${IL_LAYOUT}'_LOC_'${loc}'_VPART}"'

        if [ ! -z "${add_vpart}" ]; then
            # Only add it on if it is unique
            eval 'curr_list="${IMAGE_'${inum}'_PART_LIST}"'

            present=0
            echo "${curr_list}" | grep -q " ${add_vpart} " - && present=1
            if [ ${present} -eq 0 ]; then
                eval 'IMAGE_'${inum}'_PART_LIST="${IMAGE_'${inum}'_PART_LIST} ${add_vpart} "'
                eval 'IMAGE_'${inum}'_PART_TYPE_'${add_vpart}'="v"'

                eval 'bk_part_name="${IL_LO_'${IL_LAYOUT}'_VPART_'${add_vpart}'_PART_NAME}"'
                if [ ! -z "${bk_part_name}" ]; then
                    # Only add it on if it is unique
                    eval 'curr_bkpart_list="${IMAGE_VPART_BACK_LIST}"'

                    present=0
                    echo "${curr_bkpart_list}" | grep -q " ${bk_part_name} " - && present=1
                    if [ ${present} -eq 0 ]; then
                        eval 'IMAGE_VPART_BACK_LIST="${IMAGE_VPART_BACK_LIST} ${bk_part_name} "'
                    fi
                fi
            fi
        fi

    done
done

bkpart_list="${IMAGE_VPART_BACK_LIST}"
for bkpart in ${bkpart_list}; do
    # Must get the backing partition mounted
    eval 'part_dev="${IL_LO_'${IL_LAYOUT}'_PART_'${bkpart}'_DEV}"'
    eval 'part_mount="${IL_LO_'${IL_LAYOUT}'_PART_'${bkpart}'_MOUNT}"'
    eval 'part_fstype="${IL_LO_'${IL_LAYOUT}'_PART_'${bkpart}'_FSTYPE}"'

    if [ -z "${bkpart}" -o -z "${part_dev}" \
        -o -z "${part_mount}" -o -z "${part_fstype}" \
        -o "${part_fstype}" = "swap" ]; then
        continue
    fi

    mount_point="${TMP_VMOUNT}/${part_mount}"

    mkdir -p ${mount_point}
    umount ${mount_point} > /dev/null 2>&1
    FAILURE=0

    mount -o defaults,noatime -t ${part_fstype} ${part_dev} ${mount_point} || FAILURE=1
    if [ ${FAILURE} -eq 1 ]; then
        echo "*** Could not mount partition ${part_dev} on ${mount_point}"
        cleanup_exit
    fi
    UNMOUNT_BKP_LIST="${mount_point} ${UNMOUNT_BKP_LIST}"
done

#
# Do everything for both images 1 and 2.  This may / does end up checking
# files on shared partitions twice.  
#
# The plan is to mount all the partitions for a given image, check them against
# the manifest, and then unmount them all.
#
for inum in 1 2; do
    eval 'part_list="${IMAGE_'${inum}'_PART_LIST}"'

    if [ -z "${part_list}" ]; then
        vecho 0 "== Skipping verify of empty image location ${inum}"
        continue
    fi

    vecho 0 "== Verifying image location ${inum}"

    vecho 0 "=== Mounting partitions"

    for part in ${part_list}; do
        # Either a virtual or physical partition
        eval 'part_vtype="${IMAGE_'${inum}'_PART_TYPE_'${part}'}"'

        if [ "${part_vtype}" = "v" ]; then
            eval 'part_mount="${IL_LO_'${IL_LAYOUT}'_VPART_'${part}'_MOUNT}"'
            mount_point="${TMP_MNT_IMAGE}/${part_mount}"

            mkdir -p ${mount_point}

            do_mount_vpart ${part} ${mount_point}
            UNMOUNT_LIST="${mount_point} ${UNMOUNT_LIST}"
            continue
        fi

        eval 'part_dev="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_DEV}"'
        eval 'part_mount="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_MOUNT}"'
        eval 'part_fstype="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_FSTYPE}"'

        # Note this is IL_LO_*_PART_*_OPTIONS_IMG , which is only used
        # by imaging, not for the running system's fstab
        eval 'part_options="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_OPTIONS_IMG}"'

        if [ -z "${part_options}" ]; then
            part_options="defaults,noatime,ro"
        fi

        if [ -z "${part}" -o -z "${part_dev}" \
            -o -z "${part_mount}" -o -z "${part_fstype}" \
            -o "${part_fstype}" = "swap" ]; then
            continue
        fi

        # XXX #dep/parse: mount
        curr_part_options=`mount | grep '^'${part_dev} | head -1 | awk '{print $6}' | tr -d '()'`
        if [ -z "${curr_part_options}" ]; then
            # This should only be if the partition isn't mounted already
            curr_part_options="${part_options}"
        fi

        mount_options=
        if [ ! -z "${curr_part_options}" ]; then
            mount_options="-o ${curr_part_options}"
        fi
        mount_options="${mount_options} -t ${part_fstype}"

        mount_point="${TMP_MNT_IMAGE}/${part_mount}"

        mkdir -p ${mount_point}
        umount ${mount_point} > /dev/null 2>&1
        FAILURE=0
        mount ${mount_options} ${part_dev} ${mount_point} || FAILURE=1
        if [ ${FAILURE} -eq 1 ]; then
            echo "*** Could not mount partition ${part_dev} on ${mount_point}"
            cleanup_exit
        fi
        UNMOUNT_LIST="${mount_point} ${UNMOUNT_LIST}"
    done

    FAT_ARG=
    if [ ${HAS_FAT} -eq 1 ]; then
        FAT_ARG="-F"
    fi

    vecho 0 "=== Checking manifest"
    FAILURE=0
    cat ${TMP_MNT_IMAGE}/etc/MANIFEST | egrep -v ' \.(/dev/shm|/var/log/messages|/var/log/lastlog|/var/log/web_access_log|/var/log/web_error_log|/var/log/systemlog|/var/lib/logrotate.status|/lib/modules/[^/]*/modules.[^/]*|/var/opt/tms/output/.*|/etc/fstab|/etc/mtab|/etc/.firstboot|/proc|/bootmgr/boot/grub/grub.conf|/bootmgr/boot/grub/menu.lst|/etc/adjtime|/boot/config|/boot/fdt|/boot/System.map|/boot/vmlinuz|'${MANIFEST_EXTRA_EXCLUDES}')$' | /bin/lfi -TNGUM ${FAT_ARG} -r ${TMP_MNT_IMAGE} -c - 2>&1 | grep -v '^VERIFIED' && FAILURE=1

    # XXXX-mtd: currently files written to raw MTD partitions are not
    # verified here!

    vecho 0 "=== Unmounting partitions"
    # Unmount all the partitions
    for ump in ${UNMOUNT_LIST}; do
        umount ${ump} > /dev/null 2>&1
    done

    if [ ${FAILURE} -eq 1 ]; then
        vecho -1 "*** Image location ${inum} verification failed."
        exit 1 
    else
        vecho 0 "=== Image location ${inum} verified successfully."
    fi

done

# Unmount all the backing partitions
for ump in ${UNMOUNT_BKP_LIST}; do
    umount ${ump} > /dev/null 2>&1
done

vecho 0 "== Done"

exit 0
