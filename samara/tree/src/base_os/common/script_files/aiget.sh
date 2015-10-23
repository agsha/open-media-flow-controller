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
# This script gets which image was booted from a disk, and which will boot
# next time the machine is restarted.  See writeimage.sh for more on disk
# layouts.  
#
# This script can only be run on a running system NOT during manufacturing
#
# XXX The script currently only works if you're booted from the disk you
#     want to know the active image of, and the bootmgr partition is mounted
#     on /bootmgr !

PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

umask 0022


usage()
{
    echo "usage: $0"
    echo ""
    exit 1
}

# returns OK if $1 contains $2
strstr() {
    case "${1}" in 
        *${2}*) return 0;;
    esac
    return 1
}

# returns OK if $1 starts with $2
strprefix() {
    case "${1}" in 
        ${2}*) return 0;;
    esac
    return 1
}

EETOOL=/opt/tms/bin/eetool
NVSTOOL=/opt/tms/bin/nvstool

if [ $# != 0 ]; then
    usage
fi

if [ ! -f /etc/layout_settings.sh ]; then
    echo "Error: No valid layout settings information found."
    exit 1
fi

if [ ! -f /etc/image_layout.sh ]; then
    echo "Error: No valid image layout information found."
    exit 1
fi


# Make sure we listen to anything image_layout.sh may have to say
. /etc/image_layout.sh
. /etc/layout_settings.sh
. /etc/image_layout.sh


##################################################
# Try to find the current booted image id by looking at the
# kernel arguments, if not fall back to old way of looking at
# the root mount point (which does not work if root fs is in a
# tmpfs).
##################################################

cmdline=`cat /proc/cmdline`

FOUND_BOOT_ID=0

if strstr "$cmdline" img_id= ; then
    for arg in $cmdline ; do
        if [ "${arg##img_id=}" != "${arg}" ]; then
            THIS_BOOT_ID="${arg##img_id=}"
            FOUND_BOOT_ID=1
            break
        fi
    done
fi

# See what root device they started from
# XXX #dep/parse: mount
BOOTED_ROOT_PART=`/bin/mount | grep '^.* on / type .*$' | awk '{print $1}'`
BOOTED_ROOT_DEV=`echo ${BOOTED_ROOT_PART} | sed 's/^\(.*[^0-9]*\)[0-9][0-9]*$/\1/'`

# XXX #dep/parse: mount
BOOTED_BOOT_PART=`/bin/mount | grep '^.* on /boot type .*$' | awk '{print $1}'`
BOOTED_BOOT_DEV=`echo ${BOOTED_BOOT_PART} | sed 's/^\(.*[^0-9]*\)[0-9][0-9]*$/\1/'`

if [ ${FOUND_BOOT_ID} -ne 1 ]; then
    # Find the PART that goes with the booted root part
    eval 'part_list="${IL_LO_'${IL_LAYOUT}'_PARTS}"'

    curr_root_part=
    for part in ${part_list}; do
        eval 'this_part="${IL_LO_'${IL_LAYOUT}'_PART_'${part}'_DEV}"'
        if [ "${this_part}" = "${BOOTED_ROOT_PART}" ]; then
            curr_root_part=${part}
        fi
    done

    # See if LOC ROOT_1 or ROOT_2 is on this PART
    eval 'root_1_part="${IL_LO_'${IL_LAYOUT}'_LOC_ROOT_1_PART}"'
    eval 'root_2_part="${IL_LO_'${IL_LAYOUT}'_LOC_ROOT_2_PART}"'

    THIS_BOOT_ID=-1
    case "${curr_root_part}" in 
        ${root_1_part})
            THIS_BOOT_ID=1
            ;;
        ${root_2_part})
            THIS_BOOT_ID=2
            ;;
        *)
            ;;
    esac
fi

if [ "${BOOTED_ROOT_DEV}" = "tmpfs" ]; then
    # Find the disk the virtual partition image is stored on
    eval 'root_vpart="${IL_LO_'${IL_LAYOUT}'_LOC_ROOT_'${THIS_BOOT_ID}'_VPART}"'
    eval 'root_part="${IL_LO_'${IL_LAYOUT}'_VPART_'${root_vpart}'_PART_NAME}"'
    eval 'images_mount="${IL_LO_'${IL_LAYOUT}'_PART_'${root_part}'_MOUNT}"'

    # XXX #dep/parse: mount
    BOOTED_ROOT_PART=`/bin/mount | grep "^.* on ${images_mount} type .*\$" | awk '{print $1}'`
    BOOTED_ROOT_DEV=`echo ${BOOTED_ROOT_PART} | sed 's/^\(.*[^0-9]*\)[0-9][0-9]*$/\1/'`
fi

# XXX note, we do not support /boot backed on a VPART tmpfs
if [ "${BOOTED_BOOT_DEV}" = "tmpfs" ]; then
    # Find the disk the virtual partition image is stored on
    eval 'root_vpart="${IL_LO_'${IL_LAYOUT}'_LOC_BOOT_'${THIS_BOOT_ID}'_VPART}"'
    eval 'root_part="${IL_LO_'${IL_LAYOUT}'_VPART_'${root_vpart}'_PART_NAME}"'
    eval 'images_mount="${IL_LO_'${IL_LAYOUT}'_PART_'${root_part}'_MOUNT}"'

    # XXX #dep/parse: mount
    BOOTED_BOOT_PART=`/bin/mount | grep "^.* on ${images_mount} type .*\$" | awk '{print $1}'`
    BOOTED_BOOT_DEV=`echo ${BOOTED_BOOT_PART} | sed 's/^\(.*[^0-9]*\)[0-9][0-9]*$/\1/'`
fi

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

NEXT_BOOT_ID=-1
FALLBACK_BOOT_ID=-1

# State settings: 0=invalid; 1=active; 2=fallback; 3=manual
IMAGE_LOC_ID_1_STATE=0
IMAGE_LOC_ID_2_STATE=0

# As we must always have both 'titles' in the grub, we need a way to
# mark one invalid.
IMAGE_INVALID_TITLE="Unknown image"

##################################################
# Parse grub.conf to look for selected and fallback locations
##################################################
if [ "${bootmgr_type}" = "grub" ]; then
    GRUB=/bootmgr/boot/grub/grub.conf

    if [ -f ${GRUB} ]; then
        grub_default=`grep '^default=' ${GRUB} | sed 's/^.*=\([0-9]*\)[ \t]*$/\1/'`
        if [ -z "$grub_default" -o \( "$grub_default" != "0" -a "$grub_default" != "1" \) ]; then
            NEXT_BOOT_ID=-1
        else
            NEXT_BOOT_ID=`expr $grub_default + 1`
        fi

        grub_fallback=`grep '^fallback=' ${GRUB} | sed 's/^.*=\([0-9]*\)[ \t]*$/\1/'`
        if [ -z "$grub_fallback" -o \( "$grub_fallback" != "0" -a "$grub_fallback" != "1" \) ]; then
            FALLBACK_BOOT_ID=-1
        else
            FALLBACK_BOOT_ID=`expr $grub_fallback + 1`
        fi

        titles=`egrep '^title( |$)' ${GRUB} | sed 's/^\(title\)\(.*\)$/\2/' | sed 's/^ \(.*\)$/\1/'`
        title_1=
        title_2=
        for lc in `seq 1 $(echo "${titles}" | wc -l)`; do
            tl=$(echo "${titles}" | sed -n "${lc}p")
            eval 'title_'${lc}'="${tl}"'
        done
        if [ -z "${title_1}" ]; then
            title_1="${IMAGE_INVALID_TITLE}"
        fi
        if [ -z "${title_2}" ]; then
            title_2="${IMAGE_INVALID_TITLE}"
        fi

        # Start both states as manual
        IMAGE_LOC_ID_1_STATE=3
        IMAGE_LOC_ID_2_STATE=3

        # Possibly mark states as invalid
        if strprefix "${title_1}" "${IMAGE_INVALID_TITLE}" ; then
            IMAGE_LOC_ID_1_STATE=0
        fi
        if strprefix "${title_2}" "${IMAGE_INVALID_TITLE}" ; then
            IMAGE_LOC_ID_2_STATE=0
        fi

        # Finally, see if any 'manuals' want to be active (preferred) or fallback
        if [ "${NEXT_BOOT_ID}" = "1" -a "${IMAGE_LOC_ID_1_STATE}" = "3" ]; then
            IMAGE_LOC_ID_1_STATE=1
        fi
        if [ "${NEXT_BOOT_ID}" = "2" -a "${IMAGE_LOC_ID_2_STATE}" = "3" ]; then
            IMAGE_LOC_ID_2_STATE=1
        fi

        if [ "${FALLBACK_BOOT_ID}" = "1" -a "${IMAGE_LOC_ID_1_STATE}" = "3" ]; then
            IMAGE_LOC_ID_1_STATE=2
        fi
        if [ "${FALLBACK_BOOT_ID}" = "2" -a "${IMAGE_LOC_ID_2_STATE}" = "3" ]; then
            IMAGE_LOC_ID_2_STATE=2
        fi

    fi  
elif [ "${bootmgr_type}" = "uboot" ]; then

    if [ "${uboot_image_config}" = "eeprom" ]; then
        # Get the EEPROM values into variables, with EE_ in front of
        # them, and exported.

        eval `${EETOOL} | grep = | sed -e 's,^\([^=]*\)=,EE_\1=,p' -e 's,^\([^=]*\)=.*$,export \1,'`

        IMAGE_LOC_ID_1_STATE="${EE_S1STATE}"
        IMAGE_LOC_ID_2_STATE="${EE_S2STATE}"
    elif [ "${uboot_image_config}" = "nvs_file" ]; then
        # Get the values from the U-boot image config file into
        # variables, with NVS_ in front of them, and exported.

        eval "$(${NVSTOOL} | grep = | sed -e 's,^\([^=]*\)=,NVS_\1=,p' -e 's,^\([^=]*\)=.*$,export \1,')"

        IMAGE_LOC_ID_1_STATE="${NVS_IMAGE_1_STATE}"
        IMAGE_LOC_ID_2_STATE="${NVS_IMAGE_2_STATE}"
    fi

    if [ -z "${IMAGE_LOC_ID_1_STATE}" ]; then
        IMAGE_LOC_ID_1_STATE=0
    fi
    if [ "${IMAGE_LOC_ID_1_STATE}" -lt 0 -o "${IMAGE_LOC_ID_1_STATE}" -gt 3 ]; then
        IMAGE_LOC_ID_1_STATE=0
    fi
    if [ -z "${IMAGE_LOC_ID_2_STATE}" ]; then
        IMAGE_LOC_ID_2_STATE=0
    fi
    if [ "${IMAGE_LOC_ID_2_STATE}" -lt 0 -o "${IMAGE_LOC_ID_2_STATE}" -gt 3 ]; then
        IMAGE_LOC_ID_2_STATE=0
    fi


    # Now, set NEXT_BOOT_ID .  We'll give the fallback if nothing's marked active
    if [ "${IMAGE_LOC_ID_1_STATE}" = "1" ]; then
        NEXT_BOOT_ID=1
    fi
    if [ "${IMAGE_LOC_ID_2_STATE}" = "1" -a ${NEXT_BOOT_ID} = "-1" ]; then
        NEXT_BOOT_ID=2
    fi
    if [ "${IMAGE_LOC_ID_1_STATE}" = "2" -a ${NEXT_BOOT_ID} = "-1" ]; then
        NEXT_BOOT_ID=1
    fi
    if [ "${IMAGE_LOC_ID_2_STATE}" = "2" -a ${NEXT_BOOT_ID} = "-1" ]; then
        NEXT_BOOT_ID=2
    fi

    # Set FALLBACK_BOOT_ID.  Will be -1 if there's no loc marked '2' .
    if [ "${IMAGE_LOC_ID_1_STATE}" = "2" ]; then
        FALLBACK_BOOT_ID=1
    fi
    if [ "${IMAGE_LOC_ID_2_STATE}" = "2" -a ${FALLBACK_BOOT_ID} = "-1" ]; then
        FALLBACK_BOOT_ID=2
    fi
fi


##################################################
# Build the target device list
##################################################
TARGET_NAMES=""
TARGET_DEVS=""
eval 'TARGET_NAMES="${IL_LO_'${IL_LAYOUT}'_TARGETS}"'
for tn in ${TARGET_NAMES}; do
    new_dev=""
    eval 'new_dev="${IL_LO_'${IL_LAYOUT}'_TARGET_'${tn}'_DEV}"'

    if [ -z "${new_dev}" ]; then
        echo "Error: could not build device list"
        exit 1
    fi

    TARGET_DEVS="${TARGET_DEVS}${TARGET_DEVS:+ }${new_dev}"
done


##################################################
# Figure out if fallback-reboot is enabled, or not
##################################################
FALLBACK_REBOOT_ENABLE="true"
if [ -f /var/opt/tms/.skip_fallback_reboot ]; then
    FALLBACK_REBOOT_ENABLE="false"
fi


##################################################
# Print out all the output variables, suitable for 'eval' use
##################################################

echo "AIG_BOOTED_DEV=\"${BOOTED_ROOT_DEV}\""
##echo "AIG_BOOTED_DEV_BOOT=${BOOTED_BOOT_DEV}"
echo "AIG_THIS_BOOT_ID=${THIS_BOOT_ID}"
echo "AIG_NEXT_BOOT_ID=${NEXT_BOOT_ID}"
echo "AIG_FALLBACK_BOOT_ID=${FALLBACK_BOOT_ID}"
echo "AIG_LAYOUT=${IL_LAYOUT}"
echo "AIG_TARGET_DEVS=\"${TARGET_DEVS}\""
echo "AIG_FALLBACK_REBOOT_ENABLE=\"${FALLBACK_REBOOT_ENABLE}\""
for loc in 1 2; do
    eval 'tls="${IMAGE_LOC_ID_'${loc}'_STATE}"'
    echo "AIG_LOC_ID_${loc}_STATE=${tls}"
done
