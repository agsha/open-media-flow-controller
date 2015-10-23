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

# This script takes a .img file which has the manufacturing kernel and
# rootflop in it, and makes a bootable iso image that will automatically
# manufacture a system it is booted on.  The S34automfg script
# implements the automanufacture process itself.

# The cd has the following components:
# 
# 1) isolinux:  the boot loader
# 2) kernel: the mfg kernel (vmlinuz)
# 3) root filesystem file: the mfg rootfs (initrd)

PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

umask 0022

usage()
{
    echo "Usage: $0 -i IMAGE_FILENAME -o OUTPUT_ISO "
    echo "         [-g] [-G] [-c] [-C] [-u URL] [-d] [-D]"
    echo "         [-m model] [-z] [-Z] [-F] [-E ARG_STRING]"
    echo "         [-I] [-r] [-v] [-q]"
    echo ""
    echo "-i IMAGE_FILENAME: source image file.  Must have been built with "
    echo "     option IMAGE_INCLUDE_MFG , or conversion will fail."
    echo ""
    echo "-o OUTPUT_ISO_FILENAME: generated iso image."
    echo ""
    echo "-g : use video (VGA) console."
    echo ""
    echo "-G : use serial (text) console.  This is the default."
    echo ""
    echo "-c : do not copy image file to ISO"
    echo ""
    echo "-C : copy image file to ISO.  This is the default."
    echo ""
    echo "-d : halt when manufacture is done"
    echo ""
    echo "-D : reboot when manufacture is done.  This is the default."
    echo ""
    echo "-m model : specify which model to manufacture .  This is not "
    echo "     required.  The default as read from the image will be used"
    echo "     otherwise.  [xopt]"
    echo ""
    echo "-u : specify the URL from which to download the image to install.  [xopt]"
    echo "     This is not needed unless '-C' is in effect, as the image will"
    echo "     already be present on the ISO by default."
    echo ""
    echo "-t : do not use tmpfs for workspace, instead use target media.  [xopt]"
    echo "     By default, tmpfs is used."
    echo ""
    echo "-z : do not manufacture if the target disks are not empty.  Default. [xopt]"
    echo ""
    echo "-Z : manufacture even if the target disks are not empty.  [xopt]"
    echo ""
    echo "-y : mark targets empty on manufacturing failure.  [xopt]"
    echo ""
    echo "-Y : do not mark targets empty on manufacturing failure.  Default.  [xopt]"
    echo ""
    echo "-F : do not enable automfg in the automfg config file.  This"
    echo "    will mean that the generated ISO will never automatically start"
    echo "    automfg.  This may be useful if you are wrapping automfg in"
    echo "    another script, which itself could then call '...automfg -f' "
    echo ""
    echo "-E ARG_STRING: append these xopts to the automfg config file. "
    echo "    Note that this is done after all of the above make [xopt] are"
    echo "    put into xopts in the same config file."
    echo ""
    echo "-I: Ignore any image signature when verifying the input image."
    echo ""
    echo "-r: Require image signature which verifying the input image."
    echo ""
    echo "-v: verbose."
    echo ""
    echo "-q: quiet."
    echo ""
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

# ==================================================
# Cleanup when we are finished or interrupted
# ==================================================
cleanup()
{
    # Get out of any dirs we might remove
    cd /

    vecho 0 "Cleanup"

    sync

    if [ ! -z "${NEW_ROOTFS_MOUNT}" ]; then
        umount ${NEW_ROOTFS_MOUNT} > /dev/null 2>&1
        rmdir ${NEW_ROOTFS_MOUNT}
    fi
    if [ ! -z "${NEW_ROOTFS_LOOPDEV}" ]; then
        losetup -d ${NEW_ROOTFS_LOOPDEV} > /dev/null 2>&1
    fi

    if [ ! -z "${II_STAGE_DIR}" -a -d "${II_STAGE_DIR}" ]; then
        rm -rf "${II_STAGE_DIR}"
    fi

    if [ ! -z "${II_WORK_DIR}" -a -d "${II_WORK_DIR}" ]; then
        rm -rf "${II_WORK_DIR}"
    fi


}

# ==================================================
# Cleanup on fatal error
# ==================================================
cleanup_exit()
{
    cleanup

    vecho 0 "Ending with error"

    exit 1
}


# ==================================================
# Cleanup on fatal error
# ==================================================
die_vecho()
{
    level=$1
    shift

    if [ ${VERBOSE} -gt ${level} ]; then
        echo "$*"
    fi

    cleanup_exit
}

# Takes the filename to associate with a loopback device.  Prints the
# name of the loopback device on success (return 0), otherwise prints
# nothing (return 1).
# 
# This is all required because if we have a read-only "/" , loopback
# associations are not automatically cleaned up by umount.  Further, we
# have to do retry due to bad race behavior in "losetup": it checks to
# see if a /dev/loopX file is in use, does some other stuff, and then
# tries the requried ioctl(), which may fail if someone else has used
# /dev/loopX in the meantime.

do_losetup()
{
    losetup_file=$1
    shift

    if [ -z "${losetup_file}" ]; then
        return 1
    fi

    losetup_pass=0
    while true ; do
        losetup_pass=$((${losetup_pass}+1))
        if [ $losetup_pass -gt 3 ]; then
            return 1
        fi
        FAILURE=0
        # XXX #dep/parse: losetup
        losetup_output=$(losetup -v -f ${losetup_file} 2>&1) || FAILURE=1
        losetup_race_output=$(echo "${losetup_output}" | grep 'ioctl: LOOP_SET_FD: Device or resource busy')
        losetup_non_race_output=$(echo "${losetup_output}" | grep -v 'ioctl: LOOP_SET_FD: Device or resource busy')
        losetup_failure_output=$(echo "${losetup_non_race_output}" | grep -v 'Loop device is')
        if [ ${FAILURE} -eq 1 -a -z "${losetup_race_output}" ]; then
            return 1
        elif [ ${FAILURE} -eq 1 -a ! -z "${losetup_race_output}" ]; then
            usleep 50000
        else
            break
        fi

    done

    # XXX #dep/parse: losetup
    echo "${losetup_output}" | grep '^Loop device is ' | awk '{print $4}'
    return 0
}

# ==================================================
# Ensure that a given filesystem is supported by the kernel.
# Returns 0 on success, 1 otherwise.
# ==================================================
ensure_fstype()
{
    enfstype=$1

    if ! grep -qw "${enfstype}" /proc/filesystems ; then
        /sbin/modprobe -q "${enfstype}" || return 1
        if ! grep -qw "${enfstype}" /proc/filesystems ; then
            return 1
        fi
    fi
    return 0
}

TPKG_QUERY=/sbin/tpkg_query.sh
TPKG_EXTRACT=/sbin/tpkg_extract.sh

CONSOLE_serial_PARAMS="console=ttyS0,9600n8"
CONSOLE_video_PARAMS="console=tty0"
VERBOSE=0
TPKG_ARGS=
SIG_IGNORE=0
SIG_REQUIRE=0

GETOPT_OPTS='i:o:gGcCdDu:m:tzZyYFE:Irvq'
PARSE=`/usr/bin/getopt -s sh $GETOPT_OPTS "$@"`
if [ $? != 0 ] ; then
    vecho -1 "Error from getopt"
    usage
fi

eval set -- "$PARSE"

AUTOMFG_ENABLE=1
IMAGE_FILENAME=
ISO_FILENAME=
MFG_MODEL=
MFG_REQUIRE_EMPTY_TARGETS=1
MFG_MARK_TARGETS_EMPTY_ON_FAILURE=0
HAVE_OPT_XOPTS_STR=0
OPT_XOPTS_STR=
MFG_USE_TMPFS=0
MFG_VERBOSE=-1
MFG_IMAGE_URL=
CONSOLE_1=serial
CONSOLE_2=video
ACTION_DONE_ARG=D
COPY_IMAGE=1

# This cannot be changed.  Also, it depends on what "mkbootfl.sh" does.
# Note that it is used for both the incoming and generated fstype.
NEW_ROOTFS_FSTYPE=ext2


while true ; do
    case "$1" in
        -i) IMAGE_FILENAME=$2; shift 2 ;;
        -o) ISO_FILENAME=$2; shift 2 ;;
        -g) CONSOLE_1=video; CONSOLE_2=serial; shift ;;
        -G) CONSOLE_1=serial; CONSOLE_2=video; shift ;;
        -c) COPY_IMAGE=0; shift ;;
        -C) COPY_IMAGE=1; shift ;;
        -d) ACTION_DONE_ARG=d; shift ;;
        -D) ACTION_DONE_ARG=D; shift ;;
        -u) MFG_IMAGE_URL=$2; shift 2 ;;
        -m) MFG_MODEL=$2; shift 2 ;;
        -t) MFG_USE_TMPFS=0; shift ;;
        -z) MFG_REQUIRE_EMPTY_TARGETS=1 ; shift ;;
        -Z) MFG_REQUIRE_EMPTY_TARGETS=0 ; shift ;;
        -y) MFG_MARK_TARGETS_EMPTY_ON_FAILURE=1 ; shift ;;
        -Y) MFG_MARK_TARGETS_EMPTY_ON_FAILURE=0 ; shift ;;
        -F) AUTOMFG_ENABLE=0; shift ;;
        -E) HAVE_OPT_XOPTS_STR=1; 
            OPT_XOPTS_STR=$2; shift 2 ;;
        -I) SIG_IGNORE=1
            TPKG_ARGS="${TPKG_ARGS} -S"; shift ;;
        -r) SIG_REQUIRE=1
            TPKG_ARGS="${TPKG_ARGS} -s"; shift ;;
        -v) VERBOSE=$((${VERBOSE}+1)); shift ;;
        -q) VERBOSE=-1; shift ;;
        --) shift ; break ;;

        *) echo "$0: parse failure" >&2 ; usage ;;
    esac
done

if [ ! -z "$*" ] ; then
    usage
fi


if [ -z "${IMAGE_FILENAME}" -o -z "${ISO_FILENAME}" ]; then
    usage
fi

II_WORK_DIR=/tmp/itoi_wd.$$
II_STAGE_DIR=${II_WORK_DIR}/stage

if [ -f /usr/lib/syslinux/isolinux.bin ]; then
    ISOLINUX_BIN=/usr/lib/syslinux/isolinux.bin
else
    ISOLINUX_BIN=/usr/share/syslinux/isolinux.bin
fi

trap "cleanup_exit" HUP INT QUIT PIPE TERM

mkdir -p ${II_WORK_DIR}
mkdir -p ${II_STAGE_DIR}

# First, make sure the provided image exists, isn't corrupt, and has what
# we need in it

# Exists
if [ ! -r ${IMAGE_FILENAME} ]; then
    die_vecho -1 "Error: could not open image in ${IMAGE_FILENAME}"
fi

vecho 0 "Copying image to working directory"
COPIED_IMAGE_FILE=${II_WORK_DIR}/`basename ${IMAGE_FILENAME}`

cp ${IMAGE_FILENAME} ${COPIED_IMAGE_FILE} || \
    die_vecho -1 "Error: could not copy image file ${IMAGE_FILENAME}"


# Verify
vecho 0 "Verifying image"
${TPKG_QUERY} ${VERBOSE_ARGS} -t -f ${COPIED_IMAGE_FILE} ${TPKG_ARGS} || \
    die_vecho -1 "Error: Invalid image: could not verify ${IMAGE_FILENAME}"

# Has mfg kernel and initrd
vecho 0 "Getting meta-information from image"
eval `${TPKG_QUERY} ${VERBOSE_ARGS} -i -x -f ${COPIED_IMAGE_FILE} | egrep '^MFG_|^export MFG_' | sed -e 's,^MFG_\([A-Za-z0-9_]*\)=,II_MFG_\1=,' -e 's,^export MFG_\([A-Za-z0-9_]*\)$,export II_MFG_\1,'`

if [ -z "${II_MFG_KERNEL_NAME}" -o -z "${II_MFG_INITRD_NAME}" ]; then
    die_vecho -1 "Error: image does not contain required manufacturing components"
fi

# If they did not specify a model, get them the default model
if [ -z "${MFG_MODEL}" ]; then
    if [ ! -z "${II_MFG_MODELS_DEFAULT}" ]; then
        MFG_MODEL=${II_MFG_MODELS_DEFAULT}
    else
        die_vecho -1 "Error: image does not contain default model and no model specified"
    fi
fi

# Extract the kernel and initrd from the image into our work dir
vecho 0 "Extracting kernel and initrd"
${TPKG_EXTRACT} ${TPKG_ARGS} ${VERBOSE_ARGS} -f ${COPIED_IMAGE_FILE} -x ${II_WORK_DIR} -c "${II_MFG_KERNEL_NAME}|${II_MFG_INITRD_NAME}" || \
    die_vecho -1 "Error: could not extract manufacturing components from image"

mkdir -p ${II_STAGE_DIR}/isolinux

KERNEL_FILE=${II_WORK_DIR}/$(basename ${II_MFG_KERNEL_NAME})
INITRD_FILE=${II_WORK_DIR}/$(basename ${II_MFG_INITRD_NAME})

if [ ! -f "${KERNEL_FILE}" -o ! -f "${INITRD_FILE=}" ]; then
    die_vecho -1 "Error: could not find extracted manufacturing components"
fi

vecho 0 "Copying files into stage"

mv ${KERNEL_FILE} ${II_STAGE_DIR}/isolinux/linux || \
    die_vecho -1 "Error: could not move manufacturing kernel into our stage"

# Put isolinux into the stage
cp -p "${ISOLINUX_BIN}" ${II_STAGE_DIR}/isolinux || \
    die_vecho -1 "Error: could not add isolinux into our stage"

# If the primary console is serial or video
eval 'CONSOLE_1_PARAMS="${CONSOLE_'${CONSOLE_1}'_PARAMS}"'
eval 'CONSOLE_2_PARAMS="${CONSOLE_'${CONSOLE_2}'_PARAMS}"'

KERN_VERBOSE="quiet loglevel=4"

# Figure out what 'ramdisk_size' to use, in case the target kernel has a
# too low default, because it is older.  By default, we will not set a
# RAMDISK_SIZE_PARAM for initrd >= 16MB , as we would rather rely on the
# newer kernel's default, in case it increases again later.  No newer
# kernel is allowed to have a default ramdisk too small to hold the
# default mfg rootfs (which is now > 16MB).

RAMDISK_SIZE_PARAM=
RAMDISK_SIZE=`zcat ${INITRD_FILE} | wc -c | awk '{f= $1 / 1024; print int(f)}'`
if [ ! -z "${RAMDISK_SIZE}" -a "${RAMDISK_SIZE}" -lt "16384" ]; then
    RAMDISK_SIZE_PARAM="ramdisk_size=16384"
fi

# The isolinux config file
cat > ${II_STAGE_DIR}/isolinux/isolinux.cfg <<EOF
SERIAL 0 9600 0x3
DEFAULT linux
TIMEOUT 10
PROMPT 1
LABEL linux
    KERNEL linux
    APPEND load_ramdisk=1 ${RAMDISK_SIZE_PARAM} ${CONSOLE_2_PARAMS} ${CONSOLE_1_PARAMS} initrd=initrd rw noexec=off ${KERN_VERBOSE} panic=10
EOF
if [ $? -eq 1 ]; then
    die_vecho -1 "Error: could not add isolinux.cfg into our stage"
fi

if [ "${COPY_IMAGE}" = "1" ]; then
    vecho 0 "Copying image into stage"
    mv ${COPIED_IMAGE_FILE} ${II_STAGE_DIR}/image.img || \
        die_vecho -1 "Error: could not move image file into our stage"
fi
rm -f ${COPIED_IMAGE_FILE}

# Copy the root filesystem to the stage dir
NEW_ROOTFS_MOUNT=/tmp/iiorf_mount.$$
NEW_ROOTFS_FILE=${II_STAGE_DIR}/isolinux/initrd.raw
NEW_ROOTFS_LOOPDEV=
FINAL_ROOTFS_FILE=${II_STAGE_DIR}/isolinux/initrd

zcat ${INITRD_FILE} > ${NEW_ROOTFS_FILE} || \
    die_vecho -1 "Error: could not add manufacturing initrd into our work area"

ensure_fstype ${NEW_ROOTFS_FSTYPE} || cleanup_exit

mkdir -p ${NEW_ROOTFS_MOUNT}

# We explicitly set up a loopback device, so we can clean it up
# when / is mounted read-only.
FAILURE=0
NEW_ROOTFS_LOOPDEV=$(do_losetup ${NEW_ROOTFS_FILE}) || FAILURE=1
if [ -z "${NEW_ROOTFS_FILE}" -o "${FAILURE}" = "1" ]; then
    NEW_ROOTFS_MOUNT=
    die_vecho -1 "Error: could not setup loop device for rootfs"
fi
mount -o noatime -t ${NEW_ROOTFS_FSTYPE} ${NEW_ROOTFS_LOOPDEV} ${NEW_ROOTFS_MOUNT} || \
    die_vecho -1 "Could not mount rootfs"

# Emit the automfg file.  This is used by S34automfg as a config file.
# By default, S34automfg always runs, and uses this file if present.
AUTOMFG=${NEW_ROOTFS_MOUNT}/etc/automfg.txt
touch ${AUTOMFG} || die_vecho -1 "Could not create automfg config file"

MKISOFS_EXTRA_OPTS=

# XXXX add graft point for customers to add files, set xopts, etc

if [ ${AUTOMFG_ENABLE} = 1 ]; then
    echo "xopt_automfg_enable" >> ${AUTOMFG}
fi

if [ ! -z "${ACTION_DONE_ARG}" ]; then
    echo "xopt_automfg_${ACTION_DONE_ARG}" >> ${AUTOMFG}
fi

if [ ! -z "${MFG_MODEL}" ]; then
    echo "xopt_mfg_m=${MFG_MODEL}" >> ${AUTOMFG}
fi

if [ ! -z "${MFG_VERBOSE}" ]; then
    if [ ${MFG_VERBOSE} -lt 0 ]; then
        echo "xopt_mfg_q" >> ${AUTOMFG}
    else
        echo "xopt_mfg_v" >> ${AUTOMFG}
    fi
fi

if [ ${MFG_USE_TMPFS} = 0 ]; then
    echo "xopt_mfg_t" >> ${AUTOMFG}
fi

if [ ! -z "${MFG_IMAGE_URL}" ]; then
    echo "xopt_mfg_u=${MFG_IMAGE_URL}" >> ${AUTOMFG}
fi

if [ ${MFG_REQUIRE_EMPTY_TARGETS} = 1 ]; then
    echo "xopt_mfg_z" >> ${AUTOMFG}
fi

if [ ${MFG_MARK_TARGETS_EMPTY_ON_FAILURE} = 1 ]; then
    echo "xopt_mfg_y" >> ${AUTOMFG}
fi

if [ "${HAVE_OPT_XOPTS_STR}" = "1" -a ! -z "${OPT_XOPTS_STR}" ]; then
    echo "${OPTX_XOPTS_STR}" >> ${AUTOMFG}
fi

sync
umount ${NEW_ROOTFS_MOUNT}
losetup -d ${NEW_ROOTFS_LOOPDEV} > /dev/null 2>&1
rmdir ${NEW_ROOTFS_MOUNT}
NEW_ROOTFS_MOUNT=
NEW_ROOTFS_LOOPDEV=

gzip -9c ${NEW_ROOTFS_FILE} > ${FINAL_ROOTFS_FILE} || \
    die_vecho -1 "Could not compress final initrd"
rm -f ${NEW_ROOTFS_FILE}

# Build the iso image
vecho 0  "Building the iso image..."

rm -f "${ISO_FILENAME}"

cd ${II_STAGE_DIR}

/usr/bin/mkisofs -quiet -J -R -o ${ISO_FILENAME} \
       ${MKISOFS_EXTRA_OPTS} \
       -b isolinux/isolinux.bin -c isolinux/boot.cat \
       -no-emul-boot -boot-load-size 4 -boot-info-table . || \
    die_vecho -1 "*** Could not build ISO image"

cleanup

vecho 0 "Done."

exit 0
