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
    echo "usage: $0 -t -f imagefile.img"
    echo "usage: $0 -i -f imagefile.img"
    echo "usage: $0 -i -d -l [1|2]"

    exit 1
}


INFO_SH=build_version.sh
LOCAL_INFO_SH=/opt/tms/release/build_version.sh
MD5SUMS=md5sums
TMP_MNT_IMAGE=/tmp/mnt_image_imgq.$$
TPKG_QUERY=/sbin/tpkg_query.sh
LAST_DOBINCP_FILE=/opt/tms/release/last_dobincp
if [ ! -z "${PROD_TREE_ROOT}" -a -x "${PROD_TREE_ROOT}/src/base_os/common/script_files/tpkg_query.sh" ]; then
    TPKG_QUERY=${PROD_TREE_ROOT}/src/base_os/common/script_files/tpkg_query.sh
fi

PARSE=`/usr/bin/getopt 'tif:dl:' "$@"`

if [ $? != 0 ] ; then
    usage
fi

eval set -- "$PARSE"

DO_TEST=0
DO_INFO=0
DO_DEV=0
AUTO_DEV=1
HAVE_LOC=0
HAVE_FILE=0
LOC_ID=0

while true ; do
    case "$1" in
        -t) DO_TEST=1; shift ;;
        -i) DO_INFO=1; shift ;;
        -f) HAVE_FILE=1; IMAGE_FILE=$2; shift 2 ;;
        -d) DO_DEV=1; shift ;;
        -l) HAVE_LOC=1; LOC_ID=$2; shift 2 ;;
        --) shift ; break ;;
        *) echo "imgq.sh: parse failure" >&2 ; usage ;;
    esac
done

if [ $DO_TEST -eq $DO_INFO ]; then
    usage
fi

if [ $DO_TEST -eq 1 -a $HAVE_FILE -eq 0 ]; then
    usage
fi

if [ $DO_INFO -eq 1 -a \( $DO_DEV -eq 0 -a $HAVE_FILE -eq 0 \) ]; then
    usage
fi

if [ ${DO_DEV} -eq 1 ]; then
    if [ ${HAVE_LOC} -eq 0 -o \( "${LOC_ID}" != "1" -a "${LOC_ID}" != "2" \) ]; then
        usage
    fi

    # Get the active image location if required
    AIG_THIS_BOOT_ID=
    AIG_NEXT_BOOT_ID=

    eval `/sbin/aiget.sh`

    if [ -z "${AIG_THIS_BOOT_ID}" ]; then
        AIG_THIS_BOOT_ID=
        AIG_NEXT_BOOT_ID=
    fi

    DEV_IS_LOCAL=0
    if [ "${LOC_ID}" = "${AIG_THIS_BOOT_ID}" ]; then
        DEV_IS_LOCAL=1
    else

        # Now we have to figure out the location of the other root partition 
        . /etc/image_layout.sh
        . /etc/layout_settings.sh
        . /etc/image_layout.sh

        eval 'name_other_root="${IL_LO_'${IL_LAYOUT}'_LOC_ROOT_'${LOC_ID}'_PART}"'
        eval 'part_other_root="${IL_LO_'${IL_LAYOUT}'_PART_'${name_other_root}'_DEV}"'
        eval 'dir_other_root="${IL_LO_'${IL_LAYOUT}'_LOC_ROOT_'${LOC_ID}'_DIR}"'
        eval 'fstype_other_root="${IL_LO_'${IL_LAYOUT}'_PART_'${name_other_root}'_FSTYPE}"'
        if [ -z "${name_other_root}" ]; then
            # If not set, this root must be a virtual partition
            eval 'name_other_root="${IL_LO_'${IL_LAYOUT}'_LOC_ROOT_'${LOC_ID}'_VPART}"'
            eval 'part_name_other_vroot="${IL_LO_'${IL_LAYOUT}'_VPART_'${name_other_root}'_PART_NAME}"'
            eval 'fstype_other_vroot="${IL_LO_'${IL_LAYOUT}'_VPART_'${name_other_root}'_FSTYPE}"'
            eval 'file_name_other_vroot="${IL_LO_'${IL_LAYOUT}'_VPART_'${name_other_root}'_FILE_NAME}"'
            eval 'part_other_vroot_mount="${IL_LO_'${IL_LAYOUT}'_PART_'${part_name_other_vroot}'_MOUNT}"'
        fi
    fi

    # Make sure the location isn't marked invalid
    if [ "${DEV_IS_LOCAL}" != "1" ]; then
        eval 'loc_state="${AIG_LOC_ID_'${LOC_ID}'_STATE}"'
        if [ ! -z "${loc_state}" -a "${loc_state}" = "0" ]; then
            exit 0
        fi
    fi

fi


if [ ${DO_TEST} -eq 1 ]; then
    # Now fully handled by tpkg_query.sh .  Use of tpkg_query.sh is
    # preferred, as it can specify handling of signatures.

    if [ ! -r ${IMAGE_FILE} ]; then
        echo "*** Could not read file ${IMAGE_FILE}"
        exit 1
    fi

    ${TPKG_QUERY} -t -f ${IMAGE_FILE}
    exit $?
fi


if [ ${DO_INFO} -eq 1 ]; then
    if [ ${DO_DEV} -eq 0 ]; then
        # The plan is to extract the build_info.sh info file, grep'ing out the 
        # lines the caller won't care about.
    
        FAILURE=0
        # XXX #dep/parse: unzip
        INFO="`/usr/bin/unzip -qqp ${IMAGE_FILE} ${INFO_SH} | grep '='`" || FAILURE=1
        
        if [ ${FAILURE} -eq 1 ]; then
            echo "*** Could not extract image info file"
            exit 1
        fi
    else
        # The plan is to just grep the build_info.sh file if we're asked
        # about the booted image, and otherwise to temporarily mount the
        # other root partition, and grep it from there.

        if [ ${DEV_IS_LOCAL} -eq 1 ]; then
            FAILURE=0
            if [ ! -e "${LOCAL_INFO_SH}" ]; then
                FAILURE=1
            else
                INFO="`cat $LOCAL_INFO_SH | grep '='`" || FAILURE=1
            fi
            if [ ${FAILURE} -eq 1 ]; then
                echo "*** Could not extract image info file"
                exit 1
            fi
            if [ -e ${LAST_DOBINCP_FILE} ]; then
                LAST_DOBINCP=`stat -c '%y' ${LAST_DOBINCP_FILE}`
            fi
        else
            mount_options="-o ro -t ${fstype_other_root}"

            if [ ! -z "${part_name_other_vroot}" ]; then
                # If set, this is a virtual root partition and further
                # we assume the backing store is already mounted
                mount_options="${mount_options} -o loop,noatime -t ${fstype_other_vroot}"

                # Change part_other_root to be the loopback filesystem name
                part_other_root=${part_other_vroot_mount}/${file_name_other_vroot}
            fi

            other_root_mount="${TMP_MNT_IMAGE}/ROOT_${LOC_ID}"
            other_root_dir=${other_root_mount}/${dir_other_root}
            mkdir -p ${other_root_dir}
            FAILURE=0
            mount ${mount_options} ${part_other_root} ${other_root_mount} || FAILURE=1

            if [ ${FAILURE} -eq 1 ]; then
                echo "*** Could not mount device ${part_other_root} to extract image info"
                exit 1
            fi


            FAILURE=0
            if [ ! -e "${other_root_dir}/${LOCAL_INFO_SH}" ]; then
                FAILURE=1
            else
                INFO="`cat ${other_root_dir}/${LOCAL_INFO_SH} | grep '='`" || FAILURE=1
            fi

            if [ -e ${other_root_dir}/${LAST_DOBINCP_FILE} ]; then
                LAST_DOBINCP=`stat -c '%y' ${other_root_dir}/${LAST_DOBINCP_FILE}`
            fi

            umount ${other_root_mount}
            rm -rf ${TMP_MNT_IMAGE}
            if [ ${FAILURE} -eq 1 ]; then
                echo "*** Could not extract image info file"
                exit 1
            fi
        fi


    fi

    echo "$INFO"
    if [ ! -z "${LAST_DOBINCP}" ]; then
        LAST_DOBINCP_FMT=`echo ${LAST_DOBINCP} | sed 's/^\(.* .*\)\.[0-9]*.*$/\1/' | sed 's/-/\//g'`
        echo "LAST_DOBINCP=${LAST_DOBINCP_FMT}"
    fi
fi
