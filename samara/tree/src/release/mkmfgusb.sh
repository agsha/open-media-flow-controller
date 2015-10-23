#!/bin/sh

#
#
#
#

# This script makes a ZIP, which once it is properly unzip'd onto a USB
# flash device, and a script has been run, can be boot from to
# manufacture an appliance.
#

# On x86, the x86 PC BIOS loads syslinux off USB, which can load the
# manufacturing kernel and root filesystem off the USB, and boot it. 
#
# On PowerPC, u-boot (as modified by us) has a "run mfg_usb" command
# that will load the manufacturing kernel and root filesystem off the
# USB, and boot it.

# The USB image has the following components:
# 
# 1) windows syslinux tools: syslinux.exe and a batch scripts (with variants
#    based on the mounted drive letter) [x86 only]
# 2) kernel: the system kernel (as vmlinuz)
# 3) root filesystem file: a 'rootflop' image made in 'make release' ,
#    and potentially modified to include the system image
#
# A "readme" file placed in the generated ZIP file gives usage
# instructions.

if [ -z "${CROSS_PATH_PREADD}" ]; then
    PATH=/usr/bin:/bin:/usr/sbin:/sbin
else
    PATH=${CROSS_PATH_PREADD}:/usr/bin:/bin:/usr/sbin:/sbin
fi
export PATH

# ==================================================
# Cleanup when we are finished or interrupted
# ==================================================
cleanup()
{
    # Get out of any dirs we might remove
    cd /

    if [ ! -z "${UNMOUNT_ROOTFS}" ]; then
        ${SUDO} umount ${UNMOUNT_ROOTFS}
        rmdir ${UNMOUNT_ROOTFS}
    fi

    if [ ! -z "${UNMOUNT_ROOTFLOP}" ]; then
        ${SUDO} umount ${UNMOUNT_ROOTFLOP}
        rmdir ${UNMOUNT_ROOTFLOP}
    fi

    if [ ! -z "${CLEANUP_FILES}" ]; then
        rm -f ${CLEANUP_FILES}
    fi

    if [ ! -z "${MFGUSB_STAGE_ROOT}" ]; then
        rm -rf ${MFGUSB_STAGE_ROOT}
    fi

}


# ==================================================
# Cleanup when called from 'trap' for ^C, signal, or fatal error
# ==================================================
cleanup_exit()
{
    cleanup
    exit 1
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


if [ -z "${PROD_RELEASE_OUTPUT_DIR}" ]; then
    if [ -z "${PROD_OUTPUT_ROOT}" ] ; then
        echo "ERROR: must be called from build or set PROD_OUTPUT_ROOT plus PROD_PRODUCT and PROD_TARGET_ARCH"
        exit 1
    fi
    if [ -z "${PROD_PRODUCT}" ] ; then
        echo "ERROR: must be called from build or set PROD_PRODUCT plus PROD_OUTPUT_ROOT and PROD_TARGET_ARCH"
        exit 1
    fi
    if [ -z "${PROD_TARGET_ARCH}" ] ; then
        echo "ERROR: must be called from build or set PROD_TARGET_ARCH plus PROD_OUTPUT_ROOT and PROD_PRODUCT"
        exit 1
    fi
    PROD_PRODUCT_LC=`echo ${PROD_PRODUCT} | tr '[A-Z]' '[a-z]'`
    PROD_TARGET_ARCH_LC=`echo ${PROD_TARGET_ARCH} | tr '[A-Z]' '[a-z]'`
    export PROD_RELEASE_OUTPUT_DIR=${PROD_OUTPUT_ROOT}/product-${PROD_PRODUCT_LC}-${PROD_TARGET_ARCH_LC}/release
    if [ ! -d ${PROD_RELEASE_OUTPUT_DIR} ]; then
        echo "Must do 'make release' before building mfgcd."
        echo "No directory ${PROD_RELEASE_OUTPUT_DIR}"
        exit 1
    fi
fi
if [ -z "${PROD_INSTALL_OUTPUT_DIR}" ]; then
    PROD_INSTALL_OUTPUT_DIR=`dirname ${PROD_RELEASE_OUTPUT_DIR}`/install
fi
[ ! -d ${PROD_RELEASE_OUTPUT_DIR}/mfgusb ] && mkdir ${PROD_RELEASE_OUTPUT_DIR}/mfgusb
if [ ! -d ${PROD_RELEASE_OUTPUT_DIR}/mfgusb ] ; then
    echo Cannot create directory ${PROD_RELEASE_OUTPUT_DIR}/mfgusb
    exit 1
fi

PARSE=`/usr/bin/getopt 'd:a:ev' "$@"`

if [ $? != 0 ] ; then
    usage
fi

eval set -- "$PARSE"

VERBOSE=1
TARCH_FAMILY=x86
MFGUSB_SUBDIR=
EMBED_IMAGE=1

while true ; do
    case "$1" in
        -d) MFGUSB_SUBDIR=$2; shift 2 ;;
        -a) TARCH_FAMILY=$2; shift 2 ;;
        -e) EMBED_IMAGE=0; shift ;;
        -v) VERBOSE=$((${VERBOSE}+1)); shift ;;
        --) shift ; break ;;
        *) echo "mkmfgusb.sh: parse failure" >&2 ; usage ;;
    esac
done

if [ ! -z "$*" ] ; then
    usage
fi

# Dot in the file with the graft functions.
# The source tree location is:
#   ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT}/src/release/mkmfg_graft.sh
# In order to be able to an iso from just the release directory,
# the makefile there needs to copy mkmfg_graft.sh to the directory:
#   ${PROD_RELEASE_OUTPUT_DIR}/mkmfg/
# For backwards compatibility this graft file can be customer.sh.
# Normally this customer.sh file can be found in the output tree as:
#   ${PROD_INSTALL_OUTPUT_DIR}/image/etc/customer.sh
if [ -f ${PROD_RELEASE_OUTPUT_DIR}/mkmfg/mkmfg_graft.sh ] ; then
      . ${PROD_RELEASE_OUTPUT_DIR}/mkmfg/mkmfg_graft.sh
elif [ -f ${PROD_INSTALL_OUTPUT_DIR}/image/etc/customer.sh ]; then
        . ${PROD_INSTALL_OUTPUT_DIR}/image/etc/customer.sh
else
    echo WARNING: Missing ${PROD_RELEASE_OUTPUT_DIR}/mkmfg/mkmfg_graft.sh
    echo WARNING: or missing ${PROD_INSTALL_OUTPUT_DIR}/image/etc/customer.sh
fi

# Graft function to have alternative validation tests and to be able
# to set special filenames in the *_NAME variables.
if [ "${HAVE_MKMFGUSB_GRAFT_BEGIN}" = "y" ]; then
    mkmfgusb_graft_begin
else
    if [ `id -u` != 0 ]; then
        echo "ERROR: must be called as root"
        exit 1
    fi
fi

if [ -z "${TARCH_FAMILY}" ]; then
    TARCH_FAMILY=x86
fi

if [ -z "${ROOTFLOP_NAME}" ]; then
    ROOTFLOP_NAME=`basename ${PROD_RELEASE_OUTPUT_DIR}/rootflop/rootflop-*`
    [ ! -f  ${PROD_RELEASE_OUTPUT_DIR}/rootflop/${ROOTFLOP_NAME} ] && ROOTFLOP_NAME=rootflop.img
fi

if [ -z "${BOOTFLOP_LINUX_NAME}" ]; then
    BOOTFLOP_LINUX_NAME=`basename ${PROD_RELEASE_OUTPUT_DIR}/bootflop/vmlinuz-bootflop-*`
    [ ! -f ${PROD_RELEASE_OUTPUT_DIR}/bootflop/${BOOTFLOP_LINUX_NAME} ] && BOOTFLOP_LINUX_NAME=vmlinuz
fi

if [ "${TARCH_FAMILY}" = "ppc" ]; then
    if [ -z "${BOOTFLOP_FDT_NAME}" ]; then
        BOOTFLOP_FDT_NAME=fdt
    fi
fi

if [ -z "${IMAGE_NAME}" ]; then
    IMAGE_NAME=`basename ${PROD_RELEASE_OUTPUT_DIR}/image/image-*`
    [ ! -f ${PROD_RELEASE_OUTPUT_DIR}/image/${IMAGE_NAME} ] && IMAGE_NAME=image.img
fi

if [ -z "${MFGUSB_ZIP_NAME}" ]; then
    MFGUSB_ZIP_NAME=`echo ${IMAGE_NAME} | sed "s/image/mfgusb/" | sed "s/\.img/.zip/"`
fi

if [ -z "${ROOTFLOP_OUTPUT_NAME}" ]; then
    if [ "${TARCH_FAMILY}" = "x86" ]; then
        ROOTFLOP_OUTPUT_NAME=rootflop.img
    else
        ROOTFLOP_OUTPUT_NAME=rootfs
    fi
fi


# Verify they did a 'make release' first, as we depend on the output
if [ ! -f ${PROD_RELEASE_OUTPUT_DIR}/bootflop/${BOOTFLOP_LINUX_NAME} -o \
     ! -f ${PROD_RELEASE_OUTPUT_DIR}/rootflop/${ROOTFLOP_NAME} -o \
     ! -f ${PROD_RELEASE_OUTPUT_DIR}/image/${IMAGE_NAME} ]; then
    echo "Must do 'make release' before building mfgusb."
    exit 1
fi

UNMOUNT_ROOTFS=
UNMOUNT_ROOTFLOP=
CLEANUP_FILES=
MFGUSB_STAGE_ROOT=

# This cannot be changed.  Also, it depends on what "mkbootfl.sh" does.
# Note that it is used for both the incoming and generated fstype, and
# that it typically must be suitable for a kernel to use directly as an
# initrd.
ROOTFLOP_FSTYPE=ext2

trap "cleanup_exit" HUP INT QUIT PIPE TERM


# If we aren't told the answer via the environment, compute the size we
# need for the rootfs.  This takes into account where the image file
# will go.
if [ -z "${ROOTFS_SIZE}" ]; then
    BASE_SIZE=`zcat ${PROD_RELEASE_OUTPUT_DIR}/rootflop/${ROOTFLOP_NAME} | wc -c | awk '{f= $1 / 1024; print int(f)}'`
    if [ -z "${BASE_SIZE}" -o "${BASE_SIZE}" -lt "16000" ]; then
        BASE_SIZE=16000
    fi

    # 96 MB (a multiple of 16MB)
    IMG_SIZE=98304
    imgs=`/bin/ls -l ${PROD_RELEASE_OUTPUT_DIR}/image/${IMAGE_NAME} | awk '{f= $5 / 1024; print int(f)}'`
    if [ -z "${imgs}" ]; then
        imgs=0
    fi
    if [ "${imgs}" -gt "${IMG_SIZE}" ]; then
        IMG_SIZE=${imgs}
    fi
    if [ "${EMBED_IMAGE}" = "0" ]; then
        IMG_SIZE=0
    fi

    # Note that we leave some extra room for workspace
    ROOTFS_SIZE=`expr ${IMG_SIZE} + ${BASE_SIZE} + 8193`

    # Round to a multiple of 16MB so that we aren't likely to see
    # changes in size very often.  Note that for PowerPC, the U-boot
    # environment variable "mfg_ramdisk_size" must be sufficient.
    ISZ_MULT=16384
    ROOTFS_SIZE=`echo ${ROOTFS_SIZE} ${ISZ_MULT} 1 - + ${ISZ_MULT} / ${ISZ_MULT} \* p | dc -`
fi

# Leave some slack between the fs size and the kernel ramdisk size,
# which should not be needed.
KERNEL_ROOTFS_SIZE=$((ROOTFS_SIZE + 8192))

MFGUSB_STAGE_ROOT=/tmp/mfgusb_stage.$$
MFGUSB_STAGE_DIR=${MFGUSB_STAGE_ROOT}/${MFGUSB_SUBDIR}${MFGUSB_SUBDIR:+/}

rm -f ${MFGUSB_STAGE_ROOT}
mkdir -p ${MFGUSB_STAGE_DIR}

if [ "${TARCH_FAMILY}" = "x86" ]; then

    # Put syslinux.exe into the stage
    cp -p ${PROD_TREE_ROOT}/src/release/mfgusb/syslinux.exe ${MFGUSB_STAGE_DIR}/ || cleanup_exit
    cp -p ${PROD_TREE_ROOT}/src/release/mfgusb/mbr.bin ${MFGUSB_STAGE_DIR}/ || cleanup_exit 
    cp -p ${PROD_TREE_ROOT}/src/release/mfgusb/sysln211.exe ${MFGUSB_STAGE_DIR}/ || cleanup_exit 
    cp -p ${PROD_TREE_ROOT}/src/release/mfgusb/readme.txt ${MFGUSB_STAGE_DIR}/ || cleanup_exit
    
    # XXX this should come from the build tree instead of being inline

    # The syslinux config file
    cat > ${MFGUSB_STAGE_DIR}/syslinux.cfg <<EOF
SERIAL 0 9600 0x3
DEFAULT linux
TIMEOUT 10
PROMPT 1
LABEL linux
    KERNEL linux
    APPEND load_ramdisk=1 ramdisk_size=${KERNEL_ROOTFS_SIZE} console=ttyS0,9600n8 console=tty0 initrd=${ROOTFLOP_OUTPUT_NAME} rw noexec=off panic=10
EOF

    echo "syslinux.exe -s d:" > ${MFGUSB_STAGE_DIR}/inst_d.bat
    echo "syslinux.exe -s e:" > ${MFGUSB_STAGE_DIR}/inst_e.bat
    echo "syslinux.exe -s f:" > ${MFGUSB_STAGE_DIR}/inst_f.bat
    echo "syslinux.exe -s g:" > ${MFGUSB_STAGE_DIR}/inst_g.bat
    echo "syslinux.exe -s h:" > ${MFGUSB_STAGE_DIR}/inst_h.bat
    echo "syslinux.exe -s i:" > ${MFGUSB_STAGE_DIR}/inst_i.bat
    echo "syslinux.exe -s j:" > ${MFGUSB_STAGE_DIR}/inst_j.bat
    chmod 755 ${MFGUSB_STAGE_DIR}/*.bat

elif [ "${TARCH_FAMILY}" = "ppc" ]; then

    cp -p ${PROD_TREE_ROOT}/src/release/mfgusb/readme-ppc.txt ${MFGUSB_STAGE_DIR}/readme.txt || cleanup_exit

fi

# Mount the root floppy filesystem from the base build
#==================================================
if [ "${TARCH_FAMILY}" = "x86" ]; then
    cp -p ${PROD_RELEASE_OUTPUT_DIR}/bootflop/${BOOTFLOP_LINUX_NAME} \
        ${MFGUSB_STAGE_DIR}/linux || cleanup_exit
else
    cp -p ${PROD_RELEASE_OUTPUT_DIR}/bootflop/${BOOTFLOP_LINUX_NAME} \
        ${MFGUSB_STAGE_DIR}/vmlinuz || cleanup_exit
    cp -p ${PROD_RELEASE_OUTPUT_DIR}/bootflop/${BOOTFLOP_FDT_NAME} \
        ${MFGUSB_STAGE_DIR}/fdt || cleanup_exit
fi

echo "Uncompressing and mounting floppy root file system from build"
ROOTFLOP_MOUNT=/tmp/mfgusb_rootflop_mount.$$
zcat ${PROD_RELEASE_OUTPUT_DIR}/rootflop/${ROOTFLOP_NAME} \
    > ${MFGUSB_STAGE_DIR}/${ROOTFLOP_OUTPUT_NAME} || cleanup_exit

ensure_fstype ${ROOTFLOP_FSTYPE} || cleanup_exit

mkdir -p ${ROOTFLOP_MOUNT}
${SUDO} mount -o loop,noatime,ro -t ${ROOTFLOP_FSTYPE} ${MFGUSB_STAGE_DIR}/${ROOTFLOP_OUTPUT_NAME} ${ROOTFLOP_MOUNT} || cleanup_exit
UNMOUNT_ROOTFLOP=${ROOTFLOP_MOUNT}

# Create a new root file system based on the root floppy plus the
# system image.
#==================================================
ROOTFS_FILE=/tmp/mfgusb_rootfs.$$
ROOTFS_COMP_FILE=/tmp/mfgusb_rootflop.$$
ROOTFS_MOUNT=/tmp/mfgusb_rootfs_mount.$$

${SUDO} umount ${ROOTFS_MOUNT} > /dev/null 2>&1 
rm -f ${ROOTFS_FILE}
echo "Creating new empty rootfloppy, size ${ROOTFS_SIZE}KB"
CLEANUP_FILES="${CLEANUP_FILES} ${ROOTFS_FILE}"
dd if=/dev/zero of=${ROOTFS_FILE} bs=1k count=${ROOTFS_SIZE} status=noxfer > /dev/null 2>&1 || cleanup_exit

echo "Making filesystem for rootfloppy"
FAILURE=0
mke2fs -q -m 0 -F ${ROOTFS_FILE} || FAILURE=1
if [ ${FAILURE} -eq 1 ]; then
    echo "*** Error: could not make filesystem on ${ROOTFS_FILE}"
    cleanup_exit
fi

mkdir -p ${ROOTFS_MOUNT}
${SUDO} mount -o loop,noatime -t ${ROOTFLOP_FSTYPE} ${ROOTFS_FILE} ${ROOTFS_MOUNT} || cleanup_exit
UNMOUNT_ROOTFS=${ROOTFS_MOUNT}

rmdir ${ROOTFS_MOUNT}/lost+found

echo "Copying rootfloppy file system from build"
(cd ${ROOTFLOP_MOUNT}; ${SUDO} tar -cpf - . ) | (cd ${ROOTFS_MOUNT} ; ${SUDO} tar -xpf -) || cleanup_exit

# Copy the system image over to the new rootfs
if [ "${EMBED_IMAGE}" != "0" ]; then
    echo "Copying system image from build into root filesystem"
    ls -l ${ROOTFS_MOUNT}
    ${SUDO} chmod 777 ${ROOTFS_MOUNT}
    cp -p ${PROD_RELEASE_OUTPUT_DIR}/image/${IMAGE_NAME} \
        ${ROOTFS_MOUNT}/image.img || cleanup_exit
fi

# Graft point for updating the rootfs before the compressed file is made.
if [ "${HAVE_MKMFGUSB_GRAFT_ROOTFS}" = "y" ]; then
    mkmfgusb_graft_rootfs
fi
${SUDO} umount ${ROOTFS_MOUNT}
rmdir ${ROOTFS_MOUNT}
UNMOUNT_ROOTFS=

${SUDO} umount ${ROOTFLOP_MOUNT}
rmdir ${ROOTFLOP_MOUNT}
UNMOUNT_ROOTFLOP=


# Copy (and compress) the new rootfs into the stage
echo "Compressing rootfs"
rm -f ${ROOTFS_COMP_FILE}
CLEANUP_FILES="${CLEANUP_FILES} ${ROOTFS_COMP_FILE}"
gzip -9c ${ROOTFS_FILE} > ${ROOTFS_COMP_FILE} || cleanup_exit
rm -f ${ROOTFS_FILE}

if [ "${TARCH_FAMILY}" = "x86" ]; then
    mv ${ROOTFS_COMP_FILE} ${MFGUSB_STAGE_DIR}/${ROOTFLOP_OUTPUT_NAME} || cleanup_exit
elif [ "${TARCH_FAMILY}" = "ppc" ]; then
    mkimage -A ppc -O linux -T ramdisk -C gzip -n "${ROOTFLOP_NAME}" -d "${ROOTFS_COMP_FILE}" "${MFGUSB_STAGE_DIR}/${ROOTFLOP_OUTPUT_NAME}" || cleanup_exit
fi

if [ "${EMBED_IMAGE}" = "0" ]; then
    echo "Copying system image from build into stage"
    cp -p ${PROD_RELEASE_OUTPUT_DIR}/image/${IMAGE_NAME} \
        ${MFGUSB_STAGE_DIR}/image.img || cleanup_exit
fi

#
# Have a graft point for customers who want to customize the USB image.
# To add or modify files we are putting onto the USB, the staging directory
# holding these files is ${MFGUSB_STAGE_DIR}.
#
if [ "$HAVE_MKMFGUSB_GRAFT_1" = "y" ]; then
    mkmfgusb_graft_1
fi
# The above is for backwards compatibility when graft functions were in customer.sh
#
# Graft point to adjust what goes into the zip in ${MFGUSB_STAGE_DIR}.
# This can be used to:
# 1: Create customized syslinux.cfg.
# 2: Specify an alternate zip binary.
if [ "${HAVE_MKMFGUSB_GRAFT_PRE_ZIP}" = "y" ]; then
    mkmfgusb_graft_pre_zip
else
    ZIP=/usr/bin/zip
    ZIP_OPT=-q
fi

# Build the ZIP image
echo "Building the ZIP image..."

rm -f ${PROD_RELEASE_OUTPUT_DIR}/mfgusb/${MFGUSB_ZIP_NAME}

cd ${MFGUSB_STAGE_ROOT}
FAILURE=0
${ZIP} ${ZIP_OPT} ${PROD_RELEASE_OUTPUT_DIR}/mfgusb/${MFGUSB_ZIP_NAME} `find ${MFGUSB_SUBDIR:-.} -maxdepth 1 -type f -print` \
        || FAILURE=1
if [ ${FAILURE} -eq 1 ]; then
    echo "*** Could not build ZIP image for install"
    cleanup_exit
fi

if [ "${HAVE_MKMFGUSB_GRAFT_DONE}" = "y" ]; then
    mkmfgusb_graft_done
fi
cleanup

echo "Done."
