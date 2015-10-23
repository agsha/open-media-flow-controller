# -----------------------------------------------------------------------------
# Graft functions for these two scripts:
#   ${PROD_TREE_ROOT}/src/release/mkmfgcd.sh
#   ${PROD_TREE_ROOT}/src/release/mkmfgusb.sh
# These script creates the MFC install DVD iso file and USB zip file.

# The mkmfgcd_graft_begin function bypasses the requirement to be run as root.
HAVE_MKMFGCD_GRAFT_BEGIN=y
mkmfgcd_graft_begin()
{
  echo PROD_INSTALL_OUTPUT_DIR=${PROD_INSTALL_OUTPUT_DIR}
  echo PROD_RELEASE_OUTPUT_DIR=${PROD_RELEASE_OUTPUT_DIR}
  chown ${REAL_USER} ${PROD_RELEASE_OUTPUT_DIR}
  # Now create the freedist archive to put into the installation CD.
  sh ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT}/src/release/mkfreedist.sh
}

# The mkmfgcd_graft_pre_iso function is for creating our customized
# isolinux/isolinux.cfg and isolinux/boot.mg files.
HAVE_MKMFGCD_GRAFT_PRE_ISO=y
mkmfgcd_graft_pre_iso()
{
  echo MFGCD_ISO_NAME=${MFGCD_ISO_NAME}
  echo BOOTFLOP_NAME=${BOOTFLOP_NAME}
  echo ROOTFLOP_NAME=${ROOTFLOP_NAME}
  echo BOOTFLOP_LINUX_NAME=${BOOTFLOP_LINUX_NAME}
  echo IMAGE_NAME=${IMAGE_NAME}
  echo MFGCD_STAGE_DIR=${MFGCD_STAGE_DIR}
  MKISOFS=/usr/bin/mkisofs
  ROOTFLOP_OUTPUT_NAME=rootflop.img

  # Create the isolinux config file.
  # Differences between the default Samara file and this:
  # For Samara Fir (CentOS 5.7) the ramdisk_size must be at least 61440
  # or it will not boot.
  # PR: 761359 (from BZ 4266):
  # Modified the isolinux.cfg file being created in this script to
  # hide all the kernel debug messages and also created a bootmsg
  # to be displayed to make it cleaner:
  # 1 - Changed "PROMPT 1" to "PROMPT 0"
  # 2 - Added "DISPLAY boot.msg"
  # 3 - Removed "console=ttyS0,9600n8" from the APPEND line
  # 4 - Added "quiet loglevel=4" to the APPEND line
  # PR: 794633
  # - Added "nodmraid".  This prevents the dmraid-driver from trying to use
  #   a RAID on disks that have been previously set up.  Our manufacturing
  #   environment can have problems creating or deleting raid arrays if old
  #   ones get activated automatically.
  # PR: 783846
  # - Added "pci=noaer". This disables AER for PCI devices.  Without this
  #   pre-1.1 PCIe device raises looping AERs which causes hang on bootup.
  # PR: 961913 
  # - Add ""console=ttyS0,9600n8" to the APPEND line so systems that are
  #   easier to use with a VGA console than with a serial console (e.g.
  #   Contrail KVM) have boot messages on the VGA console.

  KERNEL_ROOTFS_SIZE=61440
  cat > ${MFGCD_STAGE_DIR}/isolinux/isolinux.cfg <<EOF
SERIAL 0 9600 0x3
DEFAULT linux
TIMEOUT 10
PROMPT 0
DISPLAY boot.msg
LABEL linux
    KERNEL linux
    APPEND load_ramdisk=1 ramdisk_size=${KERNEL_ROOTFS_SIZE} quiet loglevel=4 console=tty0 console=ttyS0,9600n8 initrd=${ROOTFLOP_OUTPUT_NAME} rw noexec=off panic=10 nodmraid pci=noaer
EOF

cat > ${MFGCD_STAGE_DIR}/isolinux/boot.msg <<EOF
^L 
          Welcome to Juniper's Media Flow Controller Installation

                  Install system is loading ...
                  (could take up to 30 seconds)

EOF

  echo "Media Flow Controller" > ${MFGCD_STAGE_DIR}/copyright.txt
  echo "Copyright 2008-`date +%Y` Juniper Networks, Inc." >> ${MFGCD_STAGE_DIR}/copyright.txt
  echo "Juniper Networks, 1194 North Mathilda Avenue, CA 94089, Phone 408-745-2000" > ${MFGCD_STAGE_DIR}/contact.txt
  touch ${MFGCD_STAGE_DIR}/${PROD_ID}_${BUILD_PROD_RELEASE}_${BUILD_NUMBER}_${BUILD_ID}
  echo "${PROD_ID}_${BUILD_PROD_RELEASE}_${BUILD_NUMBER}_${BUILD_ID}" \
        > ${MFGCD_STAGE_DIR}/about.txt
  date >> ${MFGCD_STAGE_DIR}/about.txt
  if [ -f ${PROD_INSTALL_OUTPUT_DIR}/rootflop/etc/eula.txt ] ; then
       cp ${PROD_INSTALL_OUTPUT_DIR}/rootflop/etc/eula.txt \
         ${MFGCD_STAGE_DIR}
  elif [ -f ${PROD_RELEASE_OUTPUT_DIR}/etc/eula.txt ] ; then
         cp ${PROD_RELEASE_OUTPUT_DIR}/etc/eula.txt \
           ${MFGCD_STAGE_DIR}
  fi
  if [ -f ${PROD_INSTALL_OUTPUT_DIR}/rootflop/etc/eula.pdf ] ; then
       cp ${PROD_INSTALL_OUTPUT_DIR}/rootflop/etc/eula.pdf \
         ${MFGCD_STAGE_DIR}
  elif [ -f ${PROD_RELEASE_OUTPUT_DIR}/etc/eula.pdf ] ; then
         cp ${PROD_RELEASE_OUTPUT_DIR}/etc/eula.pdf \
           ${MFGCD_STAGE_DIR}
  fi
  KD=`pwd`

  # Put the manufacture files in the manufacture subdir
  mkdir ${MFGCD_STAGE_DIR}/manufacture
  cd ${MFGCD_STAGE_DIR}/manufacture
  tar zxf ${PROD_RELEASE_OUTPUT_DIR}/manufacture/manufacture*.tgz

  # Put the DMI (vjx) image file in the dmi subdir
  THIS=dmi
  TO=${MFGCD_STAGE_DIR}/${THIS}
  VF=${PROD_RELEASE_OUTPUT_DIR}/${THIS}/version.txt
  PKG_FILE=
  if [ -f ${VF} ] ; then
    . ${VF}
  fi
  if [ ! -z "${PKG_FILE}" ] ; then
    if [ -f "${PKG_FILE}" ] ; then
      mkdir ${TO}
      cp "${PKG_FILE}" ${TO}
      cp ${PROD_RELEASE_OUTPUT_DIR}/${THIS}/* ${TO}
    else
      echo
      echo Warning: No such file "${PKG_FILE}"
      echo Warning: Not inclding ${THIS} package in CD image.
      echo
    fi
  fi

  # Put the freedist archive in the freedist subdir of the CD image.
  if [ -f ${PROD_OUTPUT_ROOT}/product-nokeena-x86_64/release/freedist/freedist*.tgz ] ; then
    # This sets FREEDIST_FILENAME=
    . ${PROD_OUTPUT_ROOT}/product-nokeena-x86_64/freedist/freedistinfo.sh
    [ ! -d ${MFGCD_STAGE_DIR}/freedist ] && mkdir ${MFGCD_STAGE_DIR}/freedist
    rm -f  ${MFGCD_STAGE_DIR}/freedist/freedist*.tgz
    cp ${PROD_OUTPUT_ROOT}/product-nokeena-x86_64/release/freedist/${FREEDIST_FILENAME} ${MFGCD_STAGE_DIR}/freedist/
  else
    echo ============================================================================================
    echo No file ${PROD_OUTPUT_ROOT}/product-nokeena-x86_64/release/freedist/freedist*.tgz
    echo ============================================================================================
  fi

  cd ${KD}
  cat >> ${MFGCD_STAGE_DIR}/.mkisofsrc <<EOF
APPI=Media Flow Controller ${BUILD_PROD_RELEASE}
ABST=about.txt
COPY=copyright.txt
PUBL=Juniper Networks, 1194 North Mathilda Avenue, CA 94089, Phone 408-745-2000
VOLI=${PROD_ID}_${BUILD_PROD_RELEASE}_${BUILD_NUMBER}
VOLS=${BUILD_ID}
EOF
  ls -la ${MFGCD_STAGE_DIR}/*
  echo ${MFGCD_STAGE_DIR}/.mkisofsrc
  cat ${MFGCD_STAGE_DIR}/.mkisofsrc

# mkisofs looks for the .mkisofsrc file, first in the current working directory,
# then in the user’s home directory, and then in  the  directory  in  which  the
# mkisofs  binary  is stored.  This file is assumed to contain a series of lines
# of the form TAG=value , and in this way you can specify certain options.   The
# case  of the tag is not significant.  Some fields in the volume header are not
# settable on the command line, but can be altered through this facility.   Com-
# ments  may  be  placed  in  this file, using lines which start with a hash (#)
# character.

}

HAVE_MKMFGCD_GRAFT_DONE=y
mkmfgcd_graft_done()
{
  cp ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT}/src/release/create-cloudvm-kvm.sh ${PROD_RELEASE_OUTPUT_DIR}/mfgcd/
  cd ${PROD_RELEASE_OUTPUT_DIR}/mfgcd/
  if [ -f mfgcd-*.iso ] ; then
    ./create-cloudvm-kvm.sh --a1
  else
    echo Error, no mfgcd-*.iso file in ${PROD_RELEASE_OUTPUT_DIR}/mfgcd
    echo Cannot create qcow2 KVM image file
  fi
  chown -R ${REAL_USER} ${PROD_RELEASE_OUTPUT_DIR}/mfgcd
}

# -----------------------------------------------------------------------------
# -----------------------------------------------------------------------------
# Graft functions for src/release/mkmfgusb.sh
# This script creates the MFC install USB drive zip file.
# These graft functions are here because we want to be able to
# create the file without having the full source tree.

# The mkmfgusb_graft_begin function bypasses the requirement to be run as root.
HAVE_MKMFGUSB_GRAFT_BEGIN=y
mkmfgusb_graft_begin()
{
  echo PROD_INSTALL_OUTPUT_DIR=${PROD_INSTALL_OUTPUT_DIR}
  echo PROD_RELEASE_OUTPUT_DIR=${PROD_RELEASE_OUTPUT_DIR}
  chown ${REAL_USER} ${PROD_RELEASE_OUTPUT_DIR}
  sh ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT}/src/release/mkfreedist.sh
}

# The mkmfgusb_graft_rootfs function is for updating files that go into the ROOTFS.
# Note that this is run BEFORE mkmfgusb_graft_pre_zip.
HAVE_MKMFGUSB_GRAFT_ROOTFS=y
mkmfgusb_graft_rootfs()
{
  echo MFGUSB_ZIP_NAME=${MFGUSB_ZIP_NAME}
  echo BOOTFLOP_NAME=${BOOTFLOP_NAME}
  echo ROOTFLOP_NAME=${ROOTFLOP_NAME}
  echo BOOTFLOP_LINUX_NAME=${BOOTFLOP_LINUX_NAME}
  echo IMAGE_NAME=${IMAGE_NAME}
  echo MFGUSB_STAGE_DIR=${MFGUSB_STAGE_DIR}

  TMP_FILE=/tmp/mfgusb.${$}tmp
  cat ${MFGUSB_STAGE_DIR}/readme.txt \
    | sed -e "s/Run manufacture.sh/Run install/" \
    | sed -e "s/manufacture.sh .*/install/" \
    | sed -e "s/example manufacture /example install /" \
    > ${TMP_FILE}
  diff ${MFGUSB_STAGE_DIR}/readme.txt ${TMP_FILE}
  cp ${TMP_FILE} ${MFGUSB_STAGE_DIR}/readme.txt
  rm -f ${TMP_FILE}

  # First copy the files that need to be in the root of the zip file.
  echo "Media Flow Controller" > ${MFGUSB_STAGE_DIR}/copyright.txt
  echo "Copyright 2008-`date +%Y` Juniper Networks, Inc." >> ${MFGUSB_STAGE_DIR}/copyright.txt
  echo "Juniper Networks, 1194 North Mathilda Avenue, CA 94089, Phone 408-745-2000" > ${MFGUSB_STAGE_DIR}/contact.txt
  touch ${MFGUSB_STAGE_DIR}/${PROD_ID}_${BUILD_PROD_RELEASE}_${BUILD_NUMBER}_${BUILD_ID}
  echo "${PROD_ID}_${BUILD_PROD_RELEASE}_${BUILD_NUMBER}_${BUILD_ID}" \
        > ${MFGUSB_STAGE_DIR}/about.txt
  date >> ${MFGUSB_STAGE_DIR}/about.txt
  if [ -f ${PROD_RELEASE_OUTPUT_DIR}/etc/eula.txt ] ; then
       cp ${PROD_RELEASE_OUTPUT_DIR}/etc/eula.txt \
         ${MFGUSB_STAGE_DIR}
  elif [ -f ${PROD_INSTALL_OUTPUT_DIR}/rootflop/etc/eula.txt ] ; then
         cp ${PROD_INSTALL_OUTPUT_DIR}/rootflop/etc/eula.txt \
           ${MFGUSB_STAGE_DIR}
  fi
  if [ -f ${PROD_RELEASE_OUTPUT_DIR}/etc/eula.pdf ] ; then
       cp ${PROD_RELEASE_OUTPUT_DIR}/etc/eula.pdf \
         ${MFGUSB_STAGE_DIR}
  elif [ -f ${PROD_INSTALL_OUTPUT_DIR}/rootflop/etc/eula.pdf ] ; then
         cp ${PROD_INSTALL_OUTPUT_DIR}/rootflop/etc/eula.pdf \
           ${MFGUSB_STAGE_DIR}
  fi

  # Now copy the files that need to be on in the root of the
  # filesystem booted up from the USB.  Copy to ${ROOTFS_MOUNT}
  cp ${MFGUSB_STAGE_DIR}/copyright.txt ${ROOTFS_MOUNT}
  cp ${MFGUSB_STAGE_DIR}/contact.txt ${ROOTFS_MOUNT}
  cp ${MFGUSB_STAGE_DIR}/about.txt ${ROOTFS_MOUNT}
  cp ${MFGUSB_STAGE_DIR}/eula.txt ${ROOTFS_MOUNT}
  if [ -f ${MFGUSB_STAGE_DIR}/eula.pdf ] ; then
       cp ${MFGUSB_STAGE_DIR}/eula.pdf ${ROOTFS_MOUNT}
  fi
  touch ${ROOTFS_MOUNT}/${PROD_ID}_${BUILD_PROD_RELEASE}_${BUILD_NUMBER}_${BUILD_ID}
  KD=`pwd`

  # Put the manufacture files in the manufacture subdir
  mkdir ${ROOTFS_MOUNT}/manufacture
  cd ${ROOTFS_MOUNT}/manufacture
  tar zxvf ${PROD_RELEASE_OUTPUT_DIR}/manufacture/manufacture*.tgz
  cd ${KD}
}

# The mkmfgusb_graft_pre_zip function is called just before zipping up the
# files from ${MFGUSB_STAGE_DIR}.
# This for creating syslinux.cfg and other files that do not have to go into
# the ROOTFS but we want in the zip file.
# Note that this is run AFTER mkmfgusb_graft_rootfs.
# (This is run AFTER the rootfs image file is all finished.)
HAVE_MKMFGUSB_GRAFT_PRE_ZIP=y
mkmfgusb_graft_pre_zip()
{
  ZIP=/usr/bin/zip
  ZIP_OPT=

  # The syslinux config file
  # Differences between the default Samara file and this:
  # For Samara Fir (CentOS 5.7) the ramdisk_size must be at least 61440
  # or it will not boot.
  # PR: 761359 (from BZ 4266):
  # Modified the isolinux.cfg file being created in this script to
  # hide all the kernel debug messages and also created a bootmsg
  # to be displayed to make it cleaner:
  # 1 - Changed "PROMPT 1" to "PROMPT 0"
  # 2 - Added "DISPLAY boot.msg"
  # 3 - Removed "console=ttyS0,9600n8" from the APPEND line
  # 4 - Added "quiet loglevel=4" to the APPEND line
  # PR: 794633
  # - Added "nodmraid".  This prevents the dmraid-driver from trying to use
  #   a RAID on disks that have been previously set up.  Our manufacturing
  #   environment can have problems creating or deleting raid arrays if old
  #   ones get activated automatically.
  # PR: 783846
  # - Added "pci=noaer". This disables AER for PCI devices.  Without this
  #   pre-1.1 PCIe device raises looping AERs which causes hang on bootup.
  # PR: 961913 
  # - Add ""console=ttyS0,9600n8" to the APPEND line so systems that are
  #   easier to use with a VGA console than with a serial console (e.g.
  #   Contrail KVM) have boot messages on the VGA console.
  
  [ ${KERNEL_ROOTFS_SIZE} -lt 61440 ] && KERNEL_ROOTFS_SIZE=61440
  cat > ${MFGUSB_STAGE_DIR}/syslinux.cfg <<EOF
SERIAL 0 9600 0x3
DEFAULT linux
TIMEOUT 10
PROMPT 0
DISPLAY boot.msg
LABEL linux
    KERNEL linux
    APPEND load_ramdisk=1 ramdisk_size=${KERNEL_ROOTFS_SIZE} quiet loglevel=4 console=tty0 console=ttyS0,9600n8 initrd=${ROOTFLOP_OUTPUT_NAME} rw noexec=off panic=10 nodmraid pci=noaer
EOF

cat > ${MFGCD_STAGE_DIR}/isolinux/boot.msg <<EOF
^L 
          Welcome to Juniper's Media Flow Controller Installation

                  Install system is loading ...
                  (could take up to 30 seconds)

EOF

  # Put the DMI (vjx) image file in the dmi subdir
  THIS=dmi
  VF=${PROD_RELEASE_OUTPUT_DIR}/${THIS}/version.txt
  if [ -f ${VF} ] ; then
    . ${VF}
    if [ -f "${PKG_FILE}" ] ; then
      cp "${PKG_FILE}" ${MFGUSB_STAGE_DIR}/${THIS}-`basename ${PKG_FILE}`
      cp ${PROD_RELEASE_OUTPUT_DIR}/${THIS}/version.txt ${MFGUSB_STAGE_DIR}/${THIS}-version.txt
    else
      echo
      echo Warning: No such file "${PKG_FILE}"
      echo Warning: Not inclding ${THIS} package in USB image.
      echo
    fi
  fi

  # Put the freedist archive in the freedist subdir of the USB image.
  if [ -f ${PROD_OUTPUT_ROOT}/product-nokeena-x86_64/release/freedist/freedist*.tgz ] ; then
    # This sets FREEDIST_FILENAME=
    . ${PROD_OUTPUT_ROOT}/product-nokeena-x86_64/freedist/freedistinfo.sh
    [ ! -d ${MFGUSB_STAGE_DIR}/freedist ] && mkdir ${MFGUSB_STAGE_DIR}/freedist
    rm -f  ${MFGUSB_STAGE_DIR}/freedist/freedist*.tgz
    cp ${PROD_OUTPUT_ROOT}/product-nokeena-x86_64/release/freedist/${FREEDIST_FILENAME} ${MFGUSB_STAGE_DIR}/freedist/
  else
    echo ============================================================================================
    echo No file ${PROD_OUTPUT_ROOT}/product-nokeena-x86_64/release/freedist/freedist*.tgz
    echo ============================================================================================
  fi

}

HAVE_MKMFGUSB_GRAFT_DONE=y
mkmfgusb_graft_done()
{
  echo Created ${PROD_RELEASE_OUTPUT_DIR}/mfgusb/${MFGUSB_ZIP_NAME}
  chown -R ${REAL_USER} ${PROD_RELEASE_OUTPUT_DIR}/mfgusb
   ls -l  ${PROD_RELEASE_OUTPUT_DIR}/mfgusb
   ls -lh ${PROD_RELEASE_OUTPUT_DIR}/mfgusb/${MFGUSB_ZIP_NAME}
}

