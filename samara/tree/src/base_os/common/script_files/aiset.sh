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

# 
# This script sets the active image on a disk.  See writeimage.sh for a
# discussion of disk layouts.  Note that this does not set which partition
# is marked bootable, which is always left as the bootmgr partition.
# It generates a /bootmgr/boot/grub/grub.conf which references
# the correct partitions. 
# 
# XXX In the runtime case, the script assumes that the partition to write
# grub.conf to, and the partition containing 'etc' are currently mounted on
# /bootmgr and / (respectively).  This means that in a running system you
# cannot set the active image of a disk you have not booted from.
#
# In the manufacturing case, the script assumes that the partition
# need for the bootmgr is not currently mounted.
#

PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

umask 0022


usage()
{
    echo "usage: $0 -i [-l NEXT_BOOT_ID] [-p MD5_PASSWORD] [-r] [-f {true,false}]"
    echo "usage: $0 -m -d BOOT_DISK [-L LAYOUT] [-l NEXT_BOOT_ID]"
    echo "          [-p MD5_PASSWORD]"
    echo ""
    echo "-i: not running at manufacture time (generally image install)"
    echo "-m: running at manufacture time"
    echo ""
    echo "-l NEXT_BOOT_ID: image location to boot from: 1 or 2"
    echo "-d BOOT_DISK: (mfg only) /dev/sda or /dev/hda"
    echo "-L LAYOUT: (mfg only) image layout, like STD"
    echo "-w HWNAME: (mfg only) hardware name (usually optional on x86)"
    echo "-p MD5_PASSWORD: MD5 encrypted password"
    echo "-r: (install only) re-install the bootmgr itself (GRUB or u-boot)"
    echo "-f {true,false}: enable or disable fallback reboot behavior for next boot"
    echo "-I IMAGE_LOCATION_ID -s IMAGE_LOCATION_STATE : exclusive with -l"
    echo "        States are: 0=invalid; 1=active; 2=fallback; 3=manual"
    echo ""
    echo "Writes a grub.conf which use the selected next boot location,"
    echo "   and which contains the installed image version strings."
    echo ""
    exit 1
}

grub_cleanup()
{
    sync

    if [ ${STARTED_BOOTMGR_MOUNT} -eq 1 ]; then
        if [ ${DO_MOUNT_BOOTMGR} -eq 1 ]; then
            umount ${TMP_MNT_IMAGE}/BOOTMGR/bootmgr
        else
            mount -o remount,ro ${PART_BOOTMGR}
        fi
    fi

    if [ ! -z "${ROOT1_LOOP_DEV}" ]; then
        losetup -d ${ROOT1_LOOP_DEV}
    fi

    if [ ! -z "${ROOT2_LOOP_DEV}" ]; then
        losetup -d ${ROOT2_LOOP_DEV}
    fi

    if [ ${STARTED_TMP_MNTS} -eq 1 ]; then
        umount ${TMP_MNT_IMAGE}/ROOT_1 2> /dev/null
        umount ${TMP_MNT_IMAGE}/ROOT_2 2> /dev/null
        umount ${TMP_MNT_IMAGE}/BOOT_1 2> /dev/null
        umount ${TMP_MNT_IMAGE}/BOOT_2 2> /dev/null
        umount ${TMP_MNT_IMAGE}/BK_ROOT_1 2> /dev/null
        umount ${TMP_MNT_IMAGE}/BK_ROOT_2 2> /dev/null
    fi

}

grub_cleanup_exit()
{
    grub_cleanup
    exit 1
}

get_loop_dev()
{
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
        grub_cleanup_exit
    else
        vecho 1 "Use loop device /dev/loop$loop_devnum"
    fi

    LOOP_DEV=/dev/loop$loop_devnum
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


uboot_cleanup()
{
    vecho 0 "== Cleanup"

    if [ ! -z "${TMP_EE_OUT}" ]; then
        rm -f ${TMP_EE_OUT}
    fi

    if [ ! -z "${TMP_NVS_OUT}" ]; then
        rm -f ${TMP_NVS_OUT}
    fi

    grub_cleanup
}

uboot_cleanup_exit()
{
    uboot_cleanup

    exit 1
}

do_uboot_reinstall()
{
    loc_list="UBOOT_ENV_1 UBOOT_1"

    for location in ${loc_list}; do
        eval 'curr_part="${IL_LO_'${IL_LAYOUT}'_LOC_'${location}'_PART}"'
        if [ ! -z "${curr_part}" ]; then
            eval 'curr_fstype="${IL_LO_'${IL_LAYOUT}'_PART_'${curr_part}'_FSTYPE}"'
            eval 'curr_part_id="${IL_LO_'${IL_LAYOUT}'_PART_'${curr_part}'_PART_ID}"'
            eval 'curr_cdev="${IL_LO_'${IL_LAYOUT}'_PART_'${curr_part}'_DEV_CHAR}"'

            if [ "${curr_part_id}" != "d9" -o ! -z "${curr_fstype}" ]; then
                vecho 0 "== Nothing matching to extract for MTD location ${location} on ${curr_cdev}"
                continue
            fi

            curr_extract_mtd=
            if [ "${curr_part_id}" = "d9" ]; then
                eval 'curr_extract_mtd="${IL_LO_'${IL_LAYOUT}'_LOC_'${location}'_IMAGE_EXTRACT_MTD}"'
            fi

            vecho 0 "== Erasing non-fs MTD partition on ${curr_cdev} for ${curr_part}"

            FAILURE=0
            do_verbose ${FLASH_ERASEALL} ${curr_cdev} || FAILURE=1
            if [ ${FAILURE} -eq 1 ]; then
                vecho -1 "*** Could not zero non-fs MTD partition ${curr_cdev}"
                uboot_cleanup_exit
            fi

            if [ -z "${curr_extract_mtd}" ]; then
                vecho 0 "== Nothing to extract for MTD location ${location} on ${curr_cdev}"
                continue
            fi
            
            vecho 0 "== Extracting by copying to MTD for location ${location} on ${curr_cdev}"
            
            eval 'xmtd_part="${IL_LO_'${IL_LAYOUT}'_LOC_'${location}'_SRC_PART}"'
            eval 'xmtd_part_mount="${IL_LO_'${IL_LAYOUT}'_PART_'${xmtd_part}'_MOUNT}"'
            
            # We assume that the source partition is already mounted, and
            # mounted as expected
            amp=${xmtd_part_mount}
            
            xmtd_from_path=${amp}/${curr_extract_mtd}

            FAILURE=0
            ${CP} ${xmtd_from_path} ${curr_cdev} || FAILURE=1
            if [ ${FAILURE} -eq 1 ]; then
                vecho -1 "*** Could not copy for ${location} to ${curr_cdev} from ${xmtd_from_path}"
                uboot_cleanup_exit
            fi
            
            # The MTD may be bigger than the source file, so deal with this
            # XXX #dep/parse: ls
            VMC_ORIG_SIZE=$(ls -lLn ${xmtd_from_path} | awk '{print $5}')
            VMC_BLOCKS=$(echo ${VMC_ORIG_SIZE} | awk '{s=4096; b= int($1 / s); x=$1 % s; k=b * s; if (k + x == $1) {print b}}')
            VMC_EXTRA=$(echo  ${VMC_ORIG_SIZE} | awk '{s=4096; b= int($1 / s); x=$1 % s; k=b * s; if (k + x == $1) {print x}}')
            VMC_SKIP=$(echo   ${VMC_ORIG_SIZE} | awk '{s=4096; b= int($1 / s); x=$1 % s; k=b * s; if (k + x == $1) {print k}}')
            if [ -z "${VMC_BLOCKS}" -o -z "${VMC_EXTRA}" -o -z "${VMC_SKIP}" ]; then
                vecho -1 "*** Could not verify copy for ${location} to ${curr_cdev} from ${xmtd_from_path}"
                uboot_cleanup_exit
            fi

            devsum=$( (dd if=${curr_cdev} bs=4096 count=${VMC_BLOCKS} conv=notrunc; dd if=${curr_cdev} bs=1 count=${VMC_EXTRA} skip=${VMC_SKIP} ) 2>/dev/null | md5sum | awk '{print $1}')
            filesum=$(md5sum ${xmtd_from_path} | awk '{print $1}')
            
            if [ "${devsum}" != "${filesum}" -o -z "${filesum}" ]; then
                vecho -1 "*** Could not verify copy for ${location} to ${curr_cdev} from ${xmtd_from_path}"
                vecho -1 "*** Failed sums: ${devsum} and ${filesum}"
                uboot_cleanup_exit
            fi
            vecho 0 "-- Verified copy to MTD matches source file"
        fi
    done

}

do_uboot_settings()
{
    trap "uboot_cleanup_exit" TERM INT

    # Optionally, re-install u-boot
    if [ ${REINSTALL_BOOTMGR} -eq 1 ]; then
        FAILURE=0
        do_uboot_reinstall || FAILURE=1
        if [ ${FAILURE} -eq 1 ]; then
            echo "*** Could not install u-boot."
            echo "*** WARNING: do not reboot until addressing this issue."
            uboot_cleanup_exit
        fi
    fi

    if [ "${uboot_image_config}" = "eeprom" ]; then

        TMP_EE_OUT=/tmp/ee.conf.$$

        rm -f ${TMP_EE_OUT}
        touch ${TMP_EE_OUT}
        chmod 600 ${TMP_EE_OUT}

        cat ${template_root}/${EE_TEMPLATE} | \
            sed \
            -e "s,@IMAGE_ID_ACTIVE@,${IMAGE_ID_ACTIVE},g" \
            -e "s,@IMAGE_ID_FALLBACK@,${IMAGE_ID_FALLBACK},g" \
            -e "s,@IMAGE_STATE_1@,${IMAGE_LOC_ID_1_STATE},g" \
            -e "s,@IMAGE_STATE_2@,${IMAGE_LOC_ID_2_STATE},g" \
            -e "s,@INIT_OPTION_1@,${INIT_OPTION_1},g" \
            -e "s,@EXTRA_KERNEL_PARAMS_1@,${EXTRA_KERNEL_PARAMS_1},g" \
            -e "s,@INITRD_OPTION_2@,${INITRD_OPTION_2},g" \
            -e "s,@INIT_OPTION_2@,${INIT_OPTION_2},g" \
            -e "s,@EXTRA_KERNEL_PARAMS_2@,${EXTRA_KERNEL_PARAMS_2},g" \
            | \
            sed -e "s,@IMAGE_NAME_1@,${VER1},g" \
            -e "s,@IMAGE_NAME_2@,${VER2},g" \
            -e "s,@BOOTMGR_PASSWORD@,${BOOTMGR_PASSWORD},g" \
            -e "s/@[^@]*@//g" > ${TMP_EE_OUT}

        FAILURE=0
        ${EETOOL} -a burn -f ${TMP_EE_OUT} || FAILURE=1

    elif [ "${uboot_image_config}" = "nvs_file" ]; then

        TMP_NVS_OUT=/tmp/nvs.conf.$$

        rm -f ${TMP_NVS_OUT}
        touch ${TMP_NVS_OUT}
        chmod 600 ${TMP_NVS_OUT}

        cat ${template_root}/${NVS_TEMPLATE} | \
            sed \
            -e "s,@IMAGE_ID_ACTIVE@,${IMAGE_ID_ACTIVE},g" \
            -e "s,@IMAGE_ID_FALLBACK@,${IMAGE_ID_FALLBACK},g" \
            -e "s,@IMAGE_STATE_1@,${IMAGE_LOC_ID_1_STATE},g" \
            -e "s,@IMAGE_STATE_2@,${IMAGE_LOC_ID_2_STATE},g" \
            -e "s,@INIT_OPTION_1@,${INIT_OPTION_1},g" \
            -e "s,@EXTRA_KERNEL_PARAMS_1@,${EXTRA_KERNEL_PARAMS_1},g" \
            -e "s,@INITRD_OPTION_2@,${INITRD_OPTION_2},g" \
            -e "s,@INIT_OPTION_2@,${INIT_OPTION_2},g" \
            -e "s,@EXTRA_KERNEL_PARAMS_2@,${EXTRA_KERNEL_PARAMS_2},g" \
            | \
            sed -e "s,@IMAGE_NAME_1@,${VER1},g" \
            -e "s,@IMAGE_NAME_2@,${VER2},g" \
            -e "s,@BOOTMGR_PASSWORD@,${BOOTMGR_PASSWORD},g" \
            -e "s/@[^@]*@//g" > ${TMP_NVS_OUT}

        PREFIX_ARG=
        if [ ! -z "${uboot_prefix_dir}" ]; then
            PREFIX_ARG="-d ${uboot_prefix_dir}"
        fi
        FAILURE=0
        ${NVSTOOL} ${PREFIX_ARG} -w -f ${TMP_NVS_OUT} || FAILURE=1
    fi
    uboot_cleanup
}

EETOOL=/opt/tms/bin/eetool
EE_TEMPLATE=/etc/ee.conf.template
NVSTOOL=/opt/tms/bin/nvstool
NVS_TEMPLATE=/etc/nvs.conf.template
CP=/bin/cp
FLASH_ERASEALL=/usr/sbin/flash_eraseall

PARSE=`/usr/bin/getopt 'mid:l:I:s:L:w:vp:rf:' "$@"`

if [ $? != 0 ] ; then
    usage
fi

eval set -- "$PARSE"

DO_MANUFACTURE=-1
USER_TARGET_DEVS=
USER_TARGET_DEV_COUNT=0
NEXT_BOOT_ID=
MD5_PASSWORD=
IMAGE_LAYOUT=STD
HWNAME=
VERBOSE=0
ROOT1_LOOP_DEV=
ROOT2_LOOP_DEV=
LOOP_DEV=/dev/loop0
DEVICE_MAP_FILE="boot/grub/device.map"
REINSTALL_BOOTMGR=0
FALLBACK_REBOOT=
LAST_LOC_ID=
LAST_LOC_ID_LIST=

while true ; do
    case "$1" in
        -m) DO_MANUFACTURE=1; shift ;;
        -i) DO_MANUFACTURE=0; shift ;;
        -d) 
            new_disk=$2; shift 2
            echo $new_disk | grep -q "^/dev"
            if [ $? -eq 1 ]; then
                usage
            fi
            USER_TARGET_DEV_COUNT=$((${USER_TARGET_DEV_COUNT} + 1))
            USER_TARGET_DEVS="${USER_TARGET_DEVS} ${new_disk}"
            eval 'USER_TARGET_DEV_'${USER_TARGET_DEV_COUNT}'="${new_disk}"'
            ;;
        -l) NEXT_BOOT_ID=$2; shift 2 
            if [ ! -z "${LAST_LOC_ID}" ]; then
                usage
            fi
            ;;
        -I) LAST_LOC_ID=$2; shift 2 
            if [ ! -z "${NEXT_BOOT_ID}" ]; then
                usage
            fi
            if [ -z "${LAST_LOC_ID}" ]; then
                usage
            fi
            if [ "${LAST_LOC_ID}" != "1" -a "${LAST_LOC_ID}" != "2" ]; then
                usage
            fi
            LAST_LOC_ID_LIST="${LAST_LOC_ID_LIST} ${LAST_LOC_ID}"
            ;;
        -s) 
            LAST_LOC_STATE=$2; shift 2
            if [ -z "${LAST_LOC_ID}" ]; then
                usage
            fi
            if [ -z "${LAST_LOC_STATE}" ]; then
                usage
            fi
            if [ "${LAST_LOC_STATE}" -lt 0 -o "${LAST_LOC_STATE}" -gt 3 ]; then
                usage
            fi
            eval 'USER_LOC_ID_'${LAST_LOC_ID}'_STATE="${LAST_LOC_STATE}"'
            ;;
        -p) MD5_PASSWORD=$2; shift 2 ;;
        -L) IMAGE_LAYOUT=$2; shift 2 ;;
        -w) HWNAME=$2; shift 2 ;;
        -f) FALLBACK_REBOOT=$2; shift 2 ;;
        -r) REINSTALL_BOOTMGR=1; shift ;;
        -v) VERBOSE=$((${VERBOSE}+1)); shift ;;
        --) shift ; break ;;
        *) echo "aiset.sh: parse failure" >&2 ; usage ;;
    esac
done

if [ ! -z "$*" ] ; then
    usage
fi

# Get the operational mode
if [ "${DO_MANUFACTURE}" -eq -1 ]; then
    usage
fi

if [ "${DO_MANUFACTURE}" -eq 1 -a -z "${USER_TARGET_DEVS}" ]; then
    usage
fi

# Only 1 and 2 are valid boot ids
if [ ! -z "${NEXT_BOOT_ID}" ]; then
    if [ "${NEXT_BOOT_ID}" != "1" -a "${NEXT_BOOT_ID}" != "2" ]; then
        usage
    fi
fi

# If we're manufacturing, the default location is 1
if [ ${DO_MANUFACTURE} -eq 1 -a -z "${NEXT_BOOT_ID}" -a -z "${LAST_LOC_ID_LIST}" ]; then
    NEXT_BOOT_ID=1
fi

# Get the active image location if required
AIG_THIS_BOOT_ID=
AIG_NEXT_BOOT_ID=
if [ ${DO_MANUFACTURE} -eq 0 ]; then
    # 
    eval `/sbin/aiget.sh`

    if [ -z "${AIG_THIS_BOOT_ID}" ]; then
        AIG_THIS_BOOT_ID=
        AIG_NEXT_BOOT_ID=
    fi
fi

# If no location was specified, default it to the active one, or else to
# 1.  Also, set FALLBACK_BOOT_ID .  Note that this is from -l , and
# means that there were always be both active and fallback partitions
# specified, regardless if they are invalid locations.

# XXXX-mtd!: the described behavior is WRONG: we should normally not
# allow the active or fallback to be set to an invalid location, unless
# some new parameter to aiset tells us to force things (like during mfg
# or install).

if [ -z "${LAST_LOC_ID_LIST}" ]; then
    if [ -z "${NEXT_BOOT_ID}" -a ! -z "${AIG_NEXT_BOOT_ID}" ]; then
        NEXT_BOOT_ID=${AIG_NEXT_BOOT_ID}
    fi
    if [ -z "${NEXT_BOOT_ID}" ]; then
        NEXT_BOOT_ID=1
    fi
    if [ "${NEXT_BOOT_ID}" != "1" -a "${NEXT_BOOT_ID}" != "2" ]; then
        NEXT_BOOT_ID=1
    fi

    if [ "${NEXT_BOOT_ID}" = "1" ]; then
        FALLBACK_BOOT_ID=2
    fi
    if [ "${NEXT_BOOT_ID}" = "2" ]; then
        FALLBACK_BOOT_ID=1
    fi
    if [ -z "${FALLBACK_BOOT_ID}" ]; then
        FALLBACK_BOOT_ID=1
    fi

    # XXXX-mtd: these are also wrong in the case of failures, etc.
    if [ "${NEXT_BOOT_ID}" = "1" ]; then
        IMAGE_LOC_ID_1_STATE=1
        IMAGE_LOC_ID_2_STATE=2
    else
        IMAGE_LOC_ID_1_STATE=2
        IMAGE_LOC_ID_2_STATE=1
    fi
fi

# We've finished all the 'normal' (-l or no image id's specified) cases.
# Now we're left with cases where some exact image location states have
# been specified.  NOTE: We're only paying attention to the loc 1 and 2
# settings.  We may end up with _no_ active images out of this

if [ ! -z "${LAST_LOC_ID_LIST}" ]; then
    IMAGE_LOC_ID_1_STATE="${USER_LOC_ID_1_STATE}"
    IMAGE_LOC_ID_2_STATE="${USER_LOC_ID_2_STATE}"

    if [ -z "${IMAGE_LOC_ID_1_STATE}" ]; then
        if [ ! -z "${AIG_LOC_ID_1_STATE}" ]; then
            IMAGE_LOC_ID_1_STATE=${AIG_LOC_ID_1_STATE}
        else
            IMAGE_LOC_ID_1_STATE=0
        fi
    fi
    if [ -z "${IMAGE_LOC_ID_2_STATE}" ]; then
        if [ ! -z "${AIG_LOC_ID_2_STATE}" ]; then
            IMAGE_LOC_ID_2_STATE=${AIG_LOC_ID_2_STATE}
        else
            IMAGE_LOC_ID_2_STATE=0
        fi
    fi

    if [ "${IMAGE_LOC_ID_1_STATE}" -lt 0 -o "${IMAGE_LOC_ID_1_STATE}" -gt 3 ]; then
        IMAGE_LOC_ID_1_STATE=0
    fi

    if [ "${IMAGE_LOC_ID_2_STATE}" -lt 0 -o "${IMAGE_LOC_ID_2_STATE}" -gt 3 ]; then
        IMAGE_LOC_ID_2_STATE=0
    fi

    # Now both states are in 0..3 , and from this we need to set 
    # NEXT_BOOT_ID and FALLBACK_BOOT_ID

    NEXT_BOOT_ID=-1
    FALLBACK_BOOT_ID=-1

    if [ "${IMAGE_LOC_ID_1_STATE}" = "1" ]; then
        NEXT_BOOT_ID=1
    fi
    if [ "${IMAGE_LOC_ID_2_STATE}" = "1" -a "${NEXT_BOOT_ID}" = "-1" ]; then
        NEXT_BOOT_ID=2
    fi

    if [ "${IMAGE_LOC_ID_1_STATE}" = "2" ]; then
        FALLBACK_BOOT_ID=1
    fi
    if [ "${IMAGE_LOC_ID_2_STATE}" = "2" -a "${FALLBACK_BOOT_ID}" = "-1" ]; then
        FALLBACK_BOOT_ID=2
    fi

fi

vecho 1 "NBI: ${NEXT_BOOT_ID}"
vecho 1 "FBI: ${FALLBACK_BOOT_ID}"
vecho 1 "IL1S: ${IMAGE_LOC_ID_1_STATE}"
vecho 1 "IL2S: ${IMAGE_LOC_ID_2_STATE}"

# Handle fallback reboot option
if [ ! -z "${FALLBACK_REBOOT}" ]; then
    case "${FALLBACK_REBOOT}" in
        true) rm -f /var/opt/tms/.skip_fallback_reboot ;;
        false) touch /var/opt/tms/.skip_fallback_reboot ;;
        *) echo "$0: bad value given for -f option" >&2 ; usage ;;
    esac
fi

# Set vars for using layout stuff
IL_LAYOUT="${IMAGE_LAYOUT}"
if [ ! -z "${HWNAME}" ]; then
    IL_HWNAME="${HWNAME}"
fi

if [ ${DO_MANUFACTURE} -eq 0 ]; then
    . /etc/image_layout.sh
    . /etc/layout_settings.sh
    . /etc/image_layout.sh

    # Set TARGET_NAMES, which are things like 'DISK1'
    eval 'TARGET_NAMES="${IL_LO_'${IL_LAYOUT}'_TARGETS}"'

    for first_target in ${TARGET_NAMES}; do 
        break
    done
else
    . /etc/layout_settings.sh

    # Set TARGET_NAMES, which are things like 'DISK1'
    eval 'TARGET_NAMES="${IL_LO_'${IL_LAYOUT}'_TARGETS}"'

    for first_target in ${TARGET_NAMES}; do 
        break
    done

    # Set the target devices based on the command line
    dev_num=1
    for tn in ${TARGET_NAMES}; do
        if [ "${dev_num}" -le "${USER_TARGET_DEV_COUNT}" ]; then
            eval 'curr_dev="${USER_TARGET_DEV_'${dev_num}'}"'
            eval 'IL_LO_'${IL_LAYOUT}'_TARGET_'${tn}'_DEV="${curr_dev}"'
        else
            eval 'curr_dev="${IL_LO_'${IL_LAYOUT}'_TARGET_'${tn}'_DEV}"'
            if [ -z "${curr_dev}" ]; then
                echo "*** Not enough device targets specified."
                exit 1
            fi
        fi
        dev_num=$((${dev_num} + 1))
    done

    . /etc/layout_settings.sh
fi

if [ -z "${first_target}" ]; then
    echo "*** Could not find first target for layout ${IL_LAYOUT}"
    exit 1
fi

eval 'FIRST_TARGET_DEV="${IL_LO_'${IL_LAYOUT}'_TARGET_'${first_target}'_DEV}"'
BOOT_DISK_DEV_NAME=`echo $FIRST_TARGET_DEV |sed 's/^\/dev\/\(.*\)$/\1/'`

# ==================================================
# Set up all the template variables

# State settings: 0=invalid; 1=active; 2=fallback; 3=manual

# The following are defined as @var@ for use in the templates
#
# IMAGE_ID_ACTIVE
# IMAGE_ID_FALLBACK
# IMAGE_NAME_1
# IMAGE_NAME_2
# IMAGE_STATE_1
# IMAGE_STATE_2
#
# GRUB_IMAGE_ID_ACTIVE
# GRUB_IMAGE_ID_FALLBACK
# GRUB_MD5_PASSWORD
# GRUB_BOOTMGR
# GRUB_BOOT_1
# GRUB_BOOT_2
#
# PART_BOOTMGR
# PART_BOOT_1
# PART_BOOT_2
# PART_ROOT_1
# PART_ROOT_2
# PART_SWAP
# PART_VAR
# PART_CONFIG
#
# ROOT_DEV_1
# ROOT_DEV_2
#
# DIR_BOOTMGR
# DIR_BOOT_1
# DIR_BOOT_2
# DIR_ROOT_1
# DIR_ROOT_2
# DIR_VAR
# DIR_CONFIG

IMAGE_ID_ACTIVE=${NEXT_BOOT_ID}
IMAGE_ID_FALLBACK=${FALLBACK_BOOT_ID}
GRUB_IMAGE_ID_ACTIVE=
if [ "${IMAGE_ID_ACTIVE}" = "1" ]; then
    GRUB_IMAGE_ID_ACTIVE=0
elif [ "${IMAGE_ID_ACTIVE}" = "2" ]; then
    GRUB_IMAGE_ID_ACTIVE=1
fi
GRUB_IMAGE_ID_FALLBACK=
if [ "${IMAGE_ID_FALLBACK}" = "1" ]; then
    GRUB_IMAGE_ID_FALLBACK=0
elif [ "${IMAGE_ID_FALLBACK}" = "2" ]; then
    GRUB_IMAGE_ID_FALLBACK=1
fi

TMP_MNT_IMAGE=/tmp/mnt_image_ais
GRUB_CONF=/boot/grub/grub.conf
GRUB_CONF_TEMP=/boot/grub/grub.conf.$$

VPART_NAME_ROOT_1=
VPART_NAME_ROOT_2=
LOOPBACK_ROOT_1=0
LOOPBACK_ROOT_2=0
USE_INITRD_1=0
USE_INITRD_2=0

# Deal with ROOTs possibly being a virtual fs image
eval 'NAME_ROOT_1="${IL_LO_'${IL_LAYOUT}'_LOC_ROOT_1_PART}"'
if [ -z "${NAME_ROOT_1}" ]; then
    eval 'NAME_ROOT_1="${IL_LO_'${IL_LAYOUT}'_LOC_ROOT_1_VPART}"'
    # Find the partition that is the backing store for this VFS
    eval 'PART_NAME_ROOT_1="${IL_LO_'${IL_LAYOUT}'_VPART_'${NAME_ROOT_1}'_PART_NAME}"'
    eval 'PART_ROOT_1="${IL_LO_'${IL_LAYOUT}'_PART_'${PART_NAME_ROOT_1}'_DEV}"'

    eval 'VPART_NAME_ROOT_1="${IL_LO_'${IL_LAYOUT}'_VPART_'${NAME_ROOT_1}'_FILE_NAME}"'
    eval 'ROOT_1_FSTYPE="${IL_LO_'${IL_LAYOUT}'_VPART_'${NAME_ROOT_1}'_FSTYPE}"'
    eval 'LOOPBACK_ROOT_1="${IL_LO_'${IL_LAYOUT}'_VPART_'${NAME_ROOT_1}'_ISROOTFS}"'

    if [ -z "${LOOPBACK_ROOT_1}" ]; then
        LOOPBACK_ROOT_1=0
    fi
    # Integrity check
    if [ "${LOOPBACK_ROOT_1}" -ne 1  ]; then
        echo "*** virtual partition root_1 not the rootfs"
        exit 1
    fi
    USE_INITRD_1=1

else
    eval 'PART_ROOT_1="${IL_LO_'${IL_LAYOUT}'_PART_'${NAME_ROOT_1}'_DEV}"'
fi

eval 'NAME_ROOT_2="${IL_LO_'${IL_LAYOUT}'_LOC_ROOT_2_PART}"'
if [ -z "${NAME_ROOT_2}" ]; then
    eval 'NAME_ROOT_2="${IL_LO_'${IL_LAYOUT}'_LOC_ROOT_2_VPART}"'
    # Find the partition that is the backing store for this VFS
    eval 'PART_NAME_ROOT_2="${IL_LO_'${IL_LAYOUT}'_VPART_'${NAME_ROOT_2}'_PART_NAME}"'
    eval 'PART_ROOT_2="${IL_LO_'${IL_LAYOUT}'_PART_'${PART_NAME_ROOT_2}'_DEV}"'

    eval 'VPART_NAME_ROOT_2="${IL_LO_'${IL_LAYOUT}'_VPART_'${NAME_ROOT_2}'_FILE_NAME}"'
    eval 'ROOT_2_FSTYPE="${IL_LO_'${IL_LAYOUT}'_VPART_'${NAME_ROOT_2}'_FSTYPE}"'
    eval 'LOOPBACK_ROOT_2="${IL_LO_'${IL_LAYOUT}'_VPART_'${NAME_ROOT_2}'_ISROOTFS}"'

    # Deal with the (non-standard) case of only a root 1
    if [ -z "${LOOPBACK_ROOT_2}" -a -z "${NAME_ROOT_2}" ]; then
        NAME_ROOT_2=
        PART_NAME_ROOT_2=
        PART_ROOT_2=
        VPART_NAME_ROOT_2=
        LOOPBACK_ROOT_2=0
    fi

    if [ -z "${LOOPBACK_ROOT_2}" ]; then
        LOOPBACK_ROOT_2=0
    fi
    # Integrity check
    if [ ! -z "${NAME_ROOT_2}" -a "${LOOPBACK_ROOT_2}" -ne 1 ]; then
        echo "*** virtual partition root_2 not the rootfs"
        exit 1
    fi
    USE_INITRD_2=1

else
    eval 'PART_ROOT_2="${IL_LO_'${IL_LAYOUT}'_PART_'${NAME_ROOT_2}'_DEV}"'
fi

eval 'NAME_BOOTMGR_1="${IL_LO_'${IL_LAYOUT}'_LOC_BOOTMGR_1_PART}"'
eval 'NAME_BOOT_1="${IL_LO_'${IL_LAYOUT}'_LOC_BOOT_1_PART}"'
eval 'NAME_BOOT_2="${IL_LO_'${IL_LAYOUT}'_LOC_BOOT_2_PART}"'
eval 'NAME_SWAP_1="${IL_LO_'${IL_LAYOUT}'_LOC_SWAP_1_PART}"'
eval 'NAME_VAR_1="${IL_LO_'${IL_LAYOUT}'_LOC_VAR_1_PART}"'
eval 'NAME_CONFIG_1="${IL_LO_'${IL_LAYOUT}'_LOC_CONFIG_1_PART}"'
eval 'PART_BOOTMGR="${IL_LO_'${IL_LAYOUT}'_PART_'${NAME_BOOTMGR_1}'_DEV}"'
eval 'PART_BOOT_1="${IL_LO_'${IL_LAYOUT}'_PART_'${NAME_BOOT_1}'_DEV}"'
eval 'PART_BOOT_2="${IL_LO_'${IL_LAYOUT}'_PART_'${NAME_BOOT_2}'_DEV}"'


eval 'PART_SWAP="${IL_LO_'${IL_LAYOUT}'_PART_'${NAME_SWAP_1}'_DEV}"'
eval 'PART_VAR="${IL_LO_'${IL_LAYOUT}'_PART_'${NAME_VAR_1}'_DEV}"'
eval 'PART_CONFIG="${IL_LO_'${IL_LAYOUT}'_PART_'${NAME_CONFIG_1}'_DEV}"'

eval 'OS_PART_NUM_BOOTMGR="${IL_LO_'${IL_LAYOUT}'_PART_'${NAME_BOOTMGR_1}'_PART_NUM}"'
eval 'OS_PART_NUM_BOOT_1="${IL_LO_'${IL_LAYOUT}'_PART_'${NAME_BOOT_1}'_PART_NUM}"'
eval 'OS_PART_NUM_BOOT_2="${IL_LO_'${IL_LAYOUT}'_PART_'${NAME_BOOT_2}'_PART_NUM}"'

eval 'DIR_BOOTMGR="${IL_LO_'${IL_LAYOUT}'_LOC_BOOTMGR_1_DIR}"'
eval 'DIR_BOOT_1="${IL_LO_'${IL_LAYOUT}'_LOC_BOOT_1_DIR}"'
eval 'DIR_BOOT_2="${IL_LO_'${IL_LAYOUT}'_LOC_BOOT_2_DIR}"'
eval 'DIR_ROOT_1="${IL_LO_'${IL_LAYOUT}'_LOC_ROOT_1_DIR}"'
eval 'DIR_ROOT_2="${IL_LO_'${IL_LAYOUT}'_LOC_ROOT_2_DIR}"'
eval 'DIR_SWAP="${IL_LO_'${IL_LAYOUT}'_LOC_SWAP_1_DIR}"'
eval 'DIR_VAR="${IL_LO_'${IL_LAYOUT}'_LOC_VAR_1_DIR}"'
eval 'DIR_CONFIG="${IL_LO_'${IL_LAYOUT}'_LOC_CONFIG_1_DIR}"'

eval 'EXTRA_KERNEL_PARAMS="${IL_LO_'${IL_LAYOUT}'_EXTRA_KERNEL_PARAMS}"'
if [ ! -z "${EXTRA_KERNEL_PARAMS}" ]; then
    EXTRA_KERNEL_PARAMS_1="${EXTRA_KERNEL_PARAMS}"
    EXTRA_KERNEL_PARAMS_2="${EXTRA_KERNEL_PARAMS}"
fi

# Note that if you have an initrd, the kernel automatically tries to run
# "/init" from it.  In our special loopback root/VPART setup, we force
# "/linuxrc" to be run instead.

# If we don't have a loopback root or are mounting by label,
# ROOT_DEV_{1,2} will be the raw partition

PATH_INITRD_1=
INITRD_OPTION_1=
INIT_OPTION_1=
ROOT_DEV_1=${PART_ROOT_1}
PATH_INITRD_2=
INITRD_OPTION_2=
INIT_OPTION_2=
ROOT_DEV_2=${PART_ROOT_2}

eval 'ROOT_MOUNT_BY_LABEL="${IL_LO_'${IL_LAYOUT}'_ROOT_MOUNT_BY_LABEL}"'
if [ -z "${ROOT_MOUNT_BY_LABEL}" ]; then
    ROOT_MOUNT_BY_LABEL=0
fi

# Location of where we place the created initrd (if needed)

if [ ${LOOPBACK_ROOT_1} -eq 1 ]; then
    # This is where writeimage.sh will put the created initrd
    INITRD_OPTION_1="initrd"
    PATH_INITRD_1=${DIR_BOOT_1}initrd
    INIT_OPTION_1="init=/linuxrc"

    # The initrd option will use a two stage boot process by first
    # loading the initrd into RAM. The initrd in turn loads the
    # real root fs.
    ROOT_DEV_1="/dev/ram"
else
    eval 'initrd_path="${IL_LO_'${IL_LAYOUT}'_INITRD_PATH}"'
    if [ ! -z "${initrd_path}" ]; then
        INITRD_OPTION_1="initrd"
        PATH_INITRD_1=${DIR_BOOT_1}${initrd_path}
        # We don't change the INIT_OPTION_1 here; we just make our
        # initrd use /init
    fi

    if [ ${ROOT_MOUNT_BY_LABEL} -eq 1 ]; then
        if [ -z "${initrd_path}" ]; then
            echo "*** Error: initrd path required when mounting by label"
            exit 1
        fi
        eval 'part_label="${IL_LO_'${IL_LAYOUT}'_PART_'${NAME_ROOT_1}'_LABEL}"'
        if [ -z "${part_label}" ]; then
            echo "*** Error: no label for root 1 ${NAME_ROOT_1}"
            exit 1
        fi
        ROOT_DEV_1="LABEL=${part_label}"
    fi
fi

if [ ${LOOPBACK_ROOT_2} -eq 1 ]; then
    # This is where writeimage.sh will put the created initrd
    INITRD_OPTION_2="initrd"
    PATH_INITRD_2=${DIR_BOOT_2}initrd
    INIT_OPTION_2="init=/linuxrc"

    # The initrd option will use a two stage boot process by first
    # loading the initrd into RAM. The initrd in turn loads the
    # real root fs.
    ROOT_DEV_2="/dev/ram"
else
    eval 'initrd_path="${IL_LO_'${IL_LAYOUT}'_INITRD_PATH}"'
    if [ ! -z "${initrd_path}" ]; then
        INITRD_OPTION_2="initrd"
        PATH_INITRD_2=${DIR_BOOT_2}${initrd_path}
        # We don't change the INIT_OPTION_2 here; we just make our
        # initrd use /init
    fi

    if [ ${ROOT_MOUNT_BY_LABEL} -eq 1 ]; then
        if [ -z "${initrd_path}" ]; then
            echo "*** Error: initrd path required when mounting by label"
            exit 1
        fi
        eval 'part_label="${IL_LO_'${IL_LAYOUT}'_PART_'${NAME_ROOT_2}'_LABEL}"'
        if [ -z "${part_label}" ]; then
            echo "*** Error: no label for root 1 ${NAME_ROOT_2}"
            exit 1
        fi
        ROOT_DEV_2="LABEL=${part_label}"
    fi
fi

if [ ! -z "${MD5_PASSWORD}" ]; then
    GRUB_MD5_PASSWORD="password --md5 ${MD5_PASSWORD}"
    BOOTMGR_PASSWORD="${MD5_PASSWORD}"
else
    if [ -f /var/opt/tms/output/bootmgr_pass ]; then
        MD5_PASSWORD=`cat /var/opt/tms/output/bootmgr_pass`
        if [ ! -z "${MD5_PASSWORD}" ]; then
            GRUB_MD5_PASSWORD="password --md5 ${MD5_PASSWORD}"
            BOOTMGR_PASSWORD="${MD5_PASSWORD}"
        fi
    fi
fi



# ==================================================
# Output phase
# ==================================================

# @PART_ROOT_1@ and @PART_ROOT_2@ are for backwards compatibility.

STARTED_TMP_MNTS=0
STARTED_BOOTMGR_MOUNT=0
REQUIRE_MOUNTED_BOOTMGR=1

trap "grub_cleanup_exit" TERM INT

# ==================================================
# Get the versioning info from each image

# If we are a running system, some partitions may already be mounted
DO_MOUNT_ROOT_1=1
DO_MOUNT_ROOT_2=1
IMAGE_INVALID_TITLE="Unknown image"
VER1="${IMAGE_INVALID_TITLE} 1"
VER2="${IMAGE_INVALID_TITLE} 2"
template_root=

if [ ${DO_MANUFACTURE} -eq 0 ]; then
    if [ ! -z "${AIG_THIS_BOOT_ID}" ]; then
        # XXX assume things are mounted correctly, should test
        if [ ${AIG_THIS_BOOT_ID} -eq 1 ]; then
            DO_MOUNT_ROOT_1=0
            . /etc/build_version.sh
            VER1="${BUILD_PROD_VERSION}"
        else
            DO_MOUNT_ROOT_2=0
            . /etc/build_version.sh
            VER2="${BUILD_PROD_VERSION}"
        fi
    fi
fi

root_1_bk_mount=
root_2_bk_mount=

mount_options="-o ro"

STARTED_TMP_MNTS=1

# Image 1 version
if [ ${DO_MOUNT_ROOT_1} -eq 1 -a ! -z "${PART_ROOT_1}" ]; then
    if [ ! -z "${VPART_NAME_ROOT_1}" ]; then
        if [ ${DO_MANUFACTURE} -ne 1 ]; then
            get_loop_dev
            ROOT1_LOOP_DEV=$LOOP_DEV
            mount_options="-o loop=${ROOT1_LOOP_DEV},noatime,ro -t ${ROOT_1_FSTYPE}"
        else
            mount_options="-o loop,noatime,ro -t ${ROOT_1_FSTYPE}"
        fi

        if [ ${DO_MANUFACTURE} -eq 1 ]; then
            # Must mount the backing store
            root_1_bk_mount=${TMP_MNT_IMAGE}/BK_ROOT_1
            mkdir -p ${root_1_bk_mount}
            mount -o ro ${PART_ROOT_1} ${root_1_bk_mount}
        else
            # On a running system, the backing store is alreay mounted
            eval 'root_1_bk_mount="${IL_LO_'${IL_LAYOUT}'_PART_'${PART_NAME_ROOT_1}'_MOUNT}"'
        fi

        # Change PART_ROOT_1 to be the loopback filesystem name
        vecho 1 "Switching to VFS root file ${root_1_bk_mount}/${VPART_NAME_ROOT_1} on ${PART_ROOT_1}"
        PART_ROOT_1=${root_1_bk_mount}/${VPART_NAME_ROOT_1}
    else
        eval 'fstype="${IL_LO_'${IL_LAYOUT}'_PART_'${NAME_ROOT_1}'_FSTYPE}"'
        mount_options="-o ro -t ${fstype}"
    fi

    root_1_mount=${TMP_MNT_IMAGE}/ROOT_1
    root_1_dir=${root_1_mount}/${DIR_ROOT_1}
    mkdir -p ${root_1_mount}
    mount ${mount_options} ${PART_ROOT_1} ${root_1_mount}

    if [ -f ${root_1_dir}/etc/build_version.sh ]; then
        . ${root_1_dir}/etc/build_version.sh
        VER1="${BUILD_PROD_VERSION}"
    fi
    if [ ${DO_MANUFACTURE} -ne 0 ]; then
        template_root=${root_1_dir}
        # Do not unmount root1, as we need to get the template from it
    else
        umount ${root_1_mount}
        root_1_mount=
    fi
fi

mount_options="-o ro"

# Image 2 version
if [ ${DO_MOUNT_ROOT_2} -eq 1 -a ! -z "${PART_ROOT_2}" ]; then
    if [ ! -z "${VPART_NAME_ROOT_2}" ]; then
        if [ ${DO_MANUFACTURE} -ne 1 ]; then
            get_loop_dev
            ROOT2_LOOP_DEV=$LOOP_DEV
            mount_options="-o loop=${ROOT2_LOOP_DEV},noatime,ro -t ${ROOT_2_FSTYPE}"
        else
            mount_options="-o loop,noatime,ro -t ${ROOT_2_FSTYPE}"
        fi

        if [ ${DO_MANUFACTURE} -eq 1 ]; then
            # Must mount the backing store
            root_2_bk_mount=${TMP_MNT_IMAGE}/BK_ROOT_2
            mkdir -p ${root_2_bk_mount}
            mount -o ro ${PART_ROOT_2} ${root_2_bk_mount}
        else
            # On a running system, the backing store is alreay mounted
            eval 'root_2_bk_mount="${IL_LO_'${IL_LAYOUT}'_PART_'${PART_NAME_ROOT_2}'_MOUNT}"'
        fi

        # Change PART_ROOT_2 to be the loopback filesystem name
        vecho 1 "Switching to VFS root file ${root_2_bk_mount}/${VPART_NAME_ROOT_2} on ${PART_ROOT_2}"
        PART_ROOT_2=${root_2_bk_mount}/${VPART_NAME_ROOT_2}
    else
        eval 'fstype="${IL_LO_'${IL_LAYOUT}'_PART_'${NAME_ROOT_2}'_FSTYPE}"'
        mount_options="-o ro -t ${fstype}"
    fi

    root_2_mount=${TMP_MNT_IMAGE}/ROOT_2
    root_2_dir=${root_2_mount}/${DIR_ROOT_2}
    mkdir -p ${root_2_mount}
    mount ${mount_options} ${PART_ROOT_2} ${root_2_mount}
    
    if [ -f ${root_2_dir}/etc/build_version.sh ]; then
        . ${root_2_dir}/etc/build_version.sh
        VER2="${BUILD_PROD_VERSION}"
    fi
    umount ${root_2_mount}
    root_2_mount=
fi

# Version fixup, in case of empty, or in case state says the location is
# bad
if [ -z "${VER1}" -o "${IMAGE_LOC_ID_1_STATE}" = "0" ]; then
    VER1="${IMAGE_INVALID_TITLE} 1"
fi
if [ -z "${VER2}" -o "${IMAGE_LOC_ID_2_STATE}" = "0" ]; then
    VER2="${IMAGE_INVALID_TITLE} 2"
fi

if [ ${DO_MANUFACTURE} -eq 0 ]; then
    if [ ! -z "${AIG_THIS_BOOT_ID}" ]; then
        template_root=/
    fi
fi

if [ -z "${template_root}" ]; then
    echo "*** Error could not find image settings template."
    grub_cleanup_exit
fi

# ==================================================
# Break out to U-boot image settings here
# ==================================================

eval 'bootmgr_type="${IL_LO_'${IL_LAYOUT}'_BOOTMGR}"'
# Right now, enforce only "grub" and "uboot" supported
if [ -z "${bootmgr_type}" -o "${bootmgr_type}" != "uboot" ]; then
    bootmgr_type=grub
fi

if [ "${bootmgr_type}" = "uboot" ]; then
    eval 'uboot_image_config="${IL_LO_'${IL_LAYOUT}'_UBOOT_IMAGE_CONFIG_TYPE}"'

    # For u-boot, enforce only "eeprom" and "nvs_file" image config
    # types supported.
    if [ -z "${uboot_image_config}" -o "${uboot_image_config}" != "nvs_file" ]; then
        uboot_image_config=eeprom
    fi
fi

# See if we'll want to do any bootmgr mount / remount (which we do not
# do with uboot/eeprom).

if [ "${bootmgr_type}" = "uboot" -a "${uboot_image_config}" = "eeprom" ]; then
    REQUIRE_MOUNTED_BOOTMGR=0
else
    REQUIRE_MOUNTED_BOOTMGR=1

    if [ -z "${PART_BOOTMGR}" ]; then
        echo "*** Error: could not locate required BOOTMGR partition"
        exit 1
    fi
fi

if [ ${REQUIRE_MOUNTED_BOOTMGR} -eq 1 ]; then

    DO_MOUNT_BOOTMGR=1

    if [ ${DO_MANUFACTURE} -eq 0 ]; then
        if [ ! -z "${AIG_THIS_BOOT_ID}" ]; then
            # Make sure if bootmgr is not mounted, we try to do so.
            # XXX if BOOTMGR isn't mounted on /bootmgr , it'll be trouble
            # XXX #dep/parse: mount
            bp=`/bin/mount | grep '^.* on /bootmgr type .*$' | awk '{print $1}'`
            if [ ! -z "${bp}" ]; then
                DO_MOUNT_BOOTMGR=0
            fi
        fi
    fi

    STARTED_BOOTMGR_MOUNT=1
    
    if [ ${DO_MOUNT_BOOTMGR} -eq 1 ]; then

        # XXX? does this mean that at least in the u-boot case that
        # BOOTMGR cannot really share a PART with anything else, since
        # then it would have a non-empty DIR_BOOTMGR, and the
        # uboot_prefix_dir would be incorrect?

        eval 'bootmgr_fstype="${IL_LO_'${IL_LAYOUT}'_PART_'${NAME_BOOTMGR_1}'_FSTYPE}"'
        bootmgr_root_dir=${TMP_MNT_IMAGE}/BOOTMGR
        bootmgr_mount=${TMP_MNT_IMAGE}/BOOTMGR/bootmgr
        bootmgr_mount_options="-o noatime -t ${bootmgr_fstype}"
        bootmgr_dir=${bootmgr_mount}/${DIR_BOOTMGR}
        mkdir -p ${bootmgr_mount}
        mount ${bootmgr_mount_options} ${PART_BOOTMGR} ${bootmgr_mount}

        grub_out_root=${bootmgr_dir}
        uboot_prefix_dir=${bootmgr_root_dir}
    else
        mount -o remount,rw ${PART_BOOTMGR}

        grub_out_root=/bootmgr
        uboot_prefix_dir=
    fi
fi

# ===========================================================================
# aiset.sh graft point #2.  In this graft point, you may reference any of
# the following variables:
#
#   - ${NEXT_BOOT_ID}: either "1" or "2", indicating the boot location
#     which is now being selected as the new next boot location.
#
#   - ${AIG_THIS_BOOT_ID}: either "1" or "2", indicating the boot location
#     from which we are currently booted.  Note that although this may in
#     some cases be the same as ${NEXT_BOOT_ID}, that does NOT mean no 
#     change is being made -- the next boot ID may have been different
#     before this run of aiset.sh.
#
if [ "$HAVE_AISET_GRAFT_2" = "y" ]; then
    aiset_graft_2
fi
# ===========================================================================

if [ "${bootmgr_type}" = "uboot" ]; then
    do_uboot_settings
    exit 0
fi

# ==================================================
# Continue on with grub, get the right grub.conf template

# Can't do this test until we're determined the root
if [ -f ${template_root}/etc/grub.conf.template.new ]; then
    GRUB_TEMPLATE=/etc/grub.conf.template.new
else
    GRUB_TEMPLATE=/etc/grub.conf.template
fi

# Optionally, re-install the grub onto the bootmgr partition
if [ ${REINSTALL_BOOTMGR} -eq 1 ]; then
    # Set up mount_point and curr_target_dev like in writeimage graft point 5.
    mount_point=${grub_out_root}
    curr_target_dev=${FIRST_TARGET_DEV}

    vecho 0 "Putting GRUB on ${curr_target_dev}"

    mkdir -p ${mount_point}/boot/grub

    # Allow customer to emit things into:
    #      ${mount_point}/boot/grub/device.map
    # This might be needed if you're changing the device map for some reason. 
    # You usually should not need to do this.
    #
    # Customer can set DID_GRUB_INSTALL=1 if they're install
    # the grub themselves in this graft point.
    #
    # NOTE: this graft point is similar to writeimage graft point 5,
    # which is used in writeimage to originally install the grub at
    # manufacture time.

    DID_GRUB_INSTALL=0
    if [ "$HAVE_AISET_GRAFT_1" = "y" ]; then
        aiset_graft_1
    fi

    if [ ${DID_GRUB_INSTALL} -ne 1 ]; then
        FAILURE=0
        do_verbose grub-install --no-floppy --root-directory=${mount_point} ${curr_target_dev} || FAILURE=1
        sync
        if [ ${FAILURE} -eq 1 ]; then
            echo "*** Could not install GRUB on ${curr_target_dev}"
            grub_cleanup_exit
        fi
    fi
fi

BOOT_DISK_GRUB_NAME=`grep $BOOT_DISK_DEV_NAME\$ ${grub_out_root}/$DEVICE_MAP_FILE |  sed 's/^(\(.*\)).*$/\1/'`

if [ -z "$BOOT_DISK_GRUB_NAME" ]; then
    BOOT_DISK_GRUB_NAME=hd0
    echo "*** Defaulting to $BOOT_DISK_GRUB_NAME for GRUB boot disk"
fi

# Grub hd numbers are one less than the os partition numbers
GRUB_BOOTMGR="(${BOOT_DISK_GRUB_NAME},$((${OS_PART_NUM_BOOTMGR} - 1)))"
GRUB_BOOT_1="(${BOOT_DISK_GRUB_NAME},$((${OS_PART_NUM_BOOT_1} - 1)))"
GRUB_BOOT_2="(${BOOT_DISK_GRUB_NAME},$((${OS_PART_NUM_BOOT_2} - 1)))"

rm -f ${grub_out_root}/${GRUB_CONF_TEMP}
touch ${grub_out_root}/${GRUB_CONF_TEMP}
chmod 600 ${grub_out_root}/${GRUB_CONF_TEMP}

# Last expression below makes any @...@ variables we don't
# understand be replaced to nothing.
cat ${template_root}/${GRUB_TEMPLATE} | \
    sed \
          -e "s,@IMAGE_ID_ACTIVE@,${IMAGE_ID_ACTIVE},g" \
          -e "s,@IMAGE_ID_FALLBACK@,${IMAGE_ID_FALLBACK},g" \
          -e "s,@GRUB_IMAGE_ID_ACTIVE@,${GRUB_IMAGE_ID_ACTIVE},g" \
          -e "s,@GRUB_IMAGE_ID_FALLBACK@,${GRUB_IMAGE_ID_FALLBACK},g" \
          -e "s,@PART_BOOTMGR@,${PART_BOOTMGR},g" \
          -e "s,@PART_BOOT_1@,${PART_BOOT_1},g" \
          -e "s,@PART_BOOT_2@,${PART_BOOT_2},g" \
          -e "s,@PART_ROOT_1@,${PART_ROOT_1},g" \
          -e "s,@PART_ROOT_2@,${PART_ROOT_2},g" \
          -e "s,@ROOT_DEV_1@,${ROOT_DEV_1},g" \
          -e "s,@ROOT_DEV_2@,${ROOT_DEV_2},g" \
          -e "s,@PART_SWAP@,${PART_SWAP},g" \
          -e "s,@PART_VAR@,${PART_VAR},g" \
          -e "s,@PART_CONFIG@,${PART_CONFIG},g" \
          -e "s,@DIR_BOOTMGR@,${DIR_BOOTMGR},g" \
          -e "s,@DIR_BOOT_1@,${DIR_BOOT_1},g" \
          -e "s,@DIR_BOOT_2@,${DIR_BOOT_2},g" \
          -e "s,@DIR_ROOT_1@,${DIR_ROOT_1},g" \
          -e "s,@DIR_ROOT_2@,${DIR_ROOT_2},g" \
          -e "s,@DIR_VAR@,${DIR_VAR},g" \
          -e "s,@DIR_CONFIG@,${DIR_CONFIG},g" \
          -e "s,@PATH_INITRD_1@,${PATH_INITRD_1},g" \
          -e "s,@PATH_INITRD_2@,${PATH_INITRD_2},g" \
          -e "s,@INITRD_OPTION_1@,${INITRD_OPTION_1},g" \
          -e "s,@INIT_OPTION_1@,${INIT_OPTION_1},g" \
          -e "s,@EXTRA_KERNEL_PARAMS_1@,${EXTRA_KERNEL_PARAMS_1},g" \
          -e "s,@INITRD_OPTION_2@,${INITRD_OPTION_2},g" \
          -e "s,@INIT_OPTION_2@,${INIT_OPTION_2},g" \
          -e "s,@EXTRA_KERNEL_PARAMS_2@,${EXTRA_KERNEL_PARAMS_2},g" \
    | \
      sed -e "s,@IMAGE_NAME_1@,${VER1},g" \
          -e "s,@IMAGE_NAME_2@,${VER2},g" \
          -e "s,@GRUB_MD5_PASSWORD@,${GRUB_MD5_PASSWORD},g" \
          -e "s,@BOOTMGR_PASSWORD@,${BOOTMGR_PASSWORD},g" \
          -e "s/@GRUB_BOOTMGR@/${GRUB_BOOTMGR}/g" \
          -e "s/@GRUB_BOOT_1@/${GRUB_BOOT_1}/g" \
          -e "s/@GRUB_BOOT_2@/${GRUB_BOOT_2}/g" \
          -e "s/@[^@]*@//g" > ${grub_out_root}/${GRUB_CONF_TEMP}

sync

if [ -s "${grub_out_root}/${GRUB_CONF_TEMP}" ]; then
    mv ${grub_out_root}/${GRUB_CONF_TEMP} ${grub_out_root}/${GRUB_CONF}
else
    echo "*** Error: empty GRUB config file"
    grub_cleanup_exit
fi

chmod 0600 ${grub_out_root}/${GRUB_CONF}

sync

if [ ${STARTED_BOOTMGR_MOUNT} -eq 1 ]; then
    if [ ${DO_MOUNT_BOOTMGR} -eq 1 ]; then
        umount ${bootmgr_mount}
    else
        mount -o remount,ro ${PART_BOOTMGR}
    fi
fi

if [ ${DO_MOUNT_ROOT_1} -eq 1 -a ! -z "${root_1_mount}" ]; then
    umount ${root_1_mount}
fi

if [ ${DO_MANUFACTURE} -eq 1 -a ! -z "${root_1_bk_mount}" ]; then
    umount ${root_1_bk_mount}
fi

if [ ${DO_MANUFACTURE} -eq 1 -a ! -z "${root_2_bk_mount}" ]; then
    umount ${root_2_bk_mount}
fi

if [ ! -z "${ROOT1_LOOP_DEV}" ]; then
    losetup -d ${ROOT1_LOOP_DEV}
fi

if [ ! -z "${ROOT2_LOOP_DEV}" ]; then
    losetup -d ${ROOT2_LOOP_DEV}
fi

exit 0
