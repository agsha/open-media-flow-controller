#!/bin/sh

#
#
#
#

#
# This script makes a bootable iso image from which a box can be manufactured.

# The cd has the following components:
# 
# 1) isolinux:  the boot loader
# 2) kernel: the system kernel (as vmlinuz)
# 3) root filesystem file: a 'rootflop' image made in 'make release'

PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

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
if [ "${HAVE_MKMFGCD_GRAFT_BEGIN}" = "y" ]; then
    mkmfgcd_graft_begin
else
    if [ `id -u` != 0 ]; then
        echo "ERROR: must be called as root"
        exit 1
    fi
fi

if [ -z "${ROOTFLOP_NAME}" ]; then
    ROOTFLOP_NAME=`basename ${PROD_RELEASE_OUTPUT_DIR}/rootflop/rootflop-*`
    [ ! -f  ${PROD_RELEASE_OUTPUT_DIR}/rootflop/${ROOTFLOP_NAME} ] && ROOTFLOP_NAME=rootflop.img
fi

if [ -z "${BOOTFLOP_LINUX_NAME}" ]; then
    BOOTFLOP_LINUX_NAME=`basename ${PROD_RELEASE_OUTPUT_DIR}/bootflop/vmlinuz-bootflop-*`
    [ ! -f ${PROD_RELEASE_OUTPUT_DIR}/bootflop/${BOOTFLOP_LINUX_NAME} ] && BOOTFLOP_LINUX_NAME=vmlinuz
fi

if [ -z "${IMAGE_NAME}" ]; then
    IMAGE_NAME=`basename ${PROD_RELEASE_OUTPUT_DIR}/image/image-*`
    [ ! -f ${PROD_RELEASE_OUTPUT_DIR}/image/${IMAGE_NAME} ] && IMAGE_NAME=image.img
fi

if [ -z "${MFGCD_ISO_NAME}" ]; then
    MFGCD_ISO_NAME=`echo ${IMAGE_NAME} | sed "s/image/mfgcd/" | sed "s/\.img/.iso/"`
fi

# Verify they did a 'make release' first, as we depend on the output
if [ ! -f ${PROD_RELEASE_OUTPUT_DIR}/bootflop/${BOOTFLOP_LINUX_NAME} -o \
     ! -f ${PROD_RELEASE_OUTPUT_DIR}/rootflop/${ROOTFLOP_NAME} -o \
     ! -f ${PROD_RELEASE_OUTPUT_DIR}/image/${IMAGE_NAME} ]; then
    echo "Must do 'make release' before building mfgcd."
    ls -l  \
      ${PROD_RELEASE_OUTPUT_DIR}/bootflop/${BOOTFLOP_LINUX_NAME} \
      ${PROD_RELEASE_OUTPUT_DIR}/rootflop/${ROOTFLOP_NAME}  \
      ${PROD_RELEASE_OUTPUT_DIR}/image/${IMAGE_NAME}
    echo "Must do 'make release' before building mfgcd."
    exit 1
fi

if [ -z "${ISOLINUX_BIN}" ]; then
    if [ -f /usr/lib/syslinux/isolinux.bin ]; then
        ISOLINUX_BIN=/usr/lib/syslinux/isolinux.bin
    else
        ISOLINUX_BIN=/usr/share/syslinux/isolinux.bin
    fi
fi

MFGCD_STAGE_DIR=/tmp/mfgcd_stage.$$

mkdir -p ${MFGCD_STAGE_DIR}
mkdir -p ${MFGCD_STAGE_DIR}/isolinux

# Put isolinux into the stage
cp -p "${ISOLINUX_BIN}" ${MFGCD_STAGE_DIR}/isolinux || exit 1


# XXX this should come from the build tree instead of being inline

# Note that we're not setting 'ramdisk_size' below, so the kernel
# default must be sufficient.

# The isolinux config file
cat > ${MFGCD_STAGE_DIR}/isolinux/isolinux.cfg <<EOF
SERIAL 0 9600 0x3
DEFAULT linux
TIMEOUT 10
PROMPT 1
LABEL linux
    KERNEL linux
    APPEND load_ramdisk=1 console=ttyS0,9600n8 console=tty0 initrd=rootflop.img rw noexec=off panic=10
EOF

cp -p ${PROD_RELEASE_OUTPUT_DIR}/bootflop/${BOOTFLOP_LINUX_NAME} \
    ${MFGCD_STAGE_DIR}/isolinux/linux || exit 1

# Copy the root filesystem over to the stage
#
# NOTE: this is an implicit dependency on "mkbootfl.sh" , that the
# rootflop.img is a suitable format for a kernel to use directly as an
# initrd.

zcat ${PROD_RELEASE_OUTPUT_DIR}/rootflop/${ROOTFLOP_NAME} \
    > ${MFGCD_STAGE_DIR}/isolinux/rootflop.img || exit 1

# Copy the system image over to the stage

cp -p ${PROD_RELEASE_OUTPUT_DIR}/image/${IMAGE_NAME} \
    ${MFGCD_STAGE_DIR}/image.img || exit 1

#
# Have a graft point for customers who want to customize the CD:
#
#   1. To add or modify files we are putting onto the CD, the staging 
#      directory holding these files is ${MFGCD_STAGE_DIR}.
#
#   2. To add custom options to mkisofs, set MKISOFS_EXTRA_OPTS.
#
MKISOFS_EXTRA_OPTS=
if [ "$HAVE_MKMFGCD_GRAFT_1" = "y" ]; then
    mkmfgcd_graft_1
fi
# The above is for backwards compatibility when graft functions were in customer.sh
#
# Graft point to adjust what goes into the iso in  ${MFGCD_STAGE_DIR}.  E.g.:
# 1: Create customized isolinux.cfg and .mkisofsrc files.
# 2: Add custom options to mkisofs with ${MKISOFS_EXTRA_OPTS}.
# 3: Specify an alternate mkisofs binary.
if [ "${HAVE_MKMFGCD_GRAFT_PRE_ISO}" = "y" ]; then
    mkmfgcd_graft_pre_iso
else
    MKISOFS=/usr/bin/mkisofs
fi

# Build the iso image
echo "Building the iso image..."

rm -f ${PROD_RELEASE_OUTPUT_DIR}/${MFGCD_ISO_NAME}

cd ${MFGCD_STAGE_DIR}
FAILURE=0
/usr/bin/mkisofs -quiet -J -R -o ${PROD_RELEASE_OUTPUT_DIR}/mfgcd/${MFGCD_ISO_NAME} \
       ${MKISOFS_EXTRA_OPTS} \
       -b isolinux/isolinux.bin -c isolinux/boot.cat \
       -no-emul-boot -boot-load-size 4 -boot-info-table \
       . || FAILURE=1
if [ ${FAILURE} -eq 1 ]; then
    echo "*** Could not build ISO image for install"
    exit 1
fi

if [ "${HAVE_MKMFGCD_GRAFT_DONE}" = "y" ]; then
    mkmfgcd_graft_done
fi
cd /

rm -rf ${MFGCD_STAGE_DIR}

echo "Done."
