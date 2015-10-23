#
# This script is to be run before any user action, to make sure
# the install media is mounted, we actually booted from CD or USB drive.
#
# When there is install media, then it installed the manufacture overlay
# which provides the files needed to do the installation.
#
# When booted from PXE, there is no media to mount, so then this does
# nothing, and "pxe-install" must be run, which does what needs to be
# done before doing the real software installation.
#
# Exiting with 0 means indicates success.
# Exiting with 1 means faiure, perhaps temporary.
# Exiting with 2 means failure, no hope of trying again.
#
IMG_FILE=none
MANUF_DIR=none
DMI_DIR=none
AUTORUN="autorun"
IMAGE_LOCATION_1=`pwd`/
IMAGE_LOCATION_2=/
IMAGE_LOCATION_3=/mnt/cdrom/
LOGF=/tmp/overlay-manuf.log


date > ${LOGF}
rm -f /tmp/img.filename
rm -f /tmp/failure.txt

Fail_Print()
{
  echo "${@}"
  echo "${@}" >> /tmp/failure.txt
  echo "${@}" >> ${LOGF}
}
Log()
{
  echo "${@}" >> ${LOGF}
}
Log_Print()
{
  echo "${@}"
  echo "${@}" >> ${LOGF}
}

for i in ${IMAGE_LOCATION_1} ${IMAGE_LOCATION_2} ${IMAGE_LOCAIONT_3}
do
  [ ! -f ${i}image.img ] && continue
  IMG_FILE=${i}image.img
  MANUF_DIR=${i}manufacture
  DMI_DIR=${i}dmi
done
if [ "${IMG_FILE}" = "none" ] ; then
  # Perhaps we booted from a CDROM, but it did not get automatically mounted.
  # Some USB CDROM drives do not automatically have their device nodes,
  # and thus cannot be automatically mounted when we boot from it.
  # So make the node just in case we booted from an CD-ROM, so we can mount it.
  if [ ! -d /dev/sr0 ] ; then
    Log \
    mknod /dev/sr0 b 11 0
    mknod /dev/sr0 b 11 0 >> ${LOGF} 2>&1
  fi
  # Perhaps this is a second CDROM driver.
  if [ ! -d /dev/scd1 ] ; then
    Log \
    mknod /dev/scd1 b 11 1
    mknod /dev/scd1 b 11 1 >> ${LOGF} 2>&1
  fi

  CDROM_DEVICES="/dev/sr0 /dev/scd1 /dev/hdd /dev/hdc /dev/hdb /dev/sdb /dev/hda /dev/sda"
  CDROM_MNT_POINT="/mnt/cdrom"
  umount ${CDROM_MNT_POINT} > /dev/null 2> /dev/null
  for DEVICE in ${CDROM_DEVICES}; do
    if mount -t iso9660 -o ro,exec,nosuid ${DEVICE} ${CDROM_MNT_POINT} > /dev/null 2>&1; then
      if [ -f ${CDROM_MNT_POINT}/image.img ] ; then
        Log_Print "Automatically mounted cdrom ${DEVICE} to ${CDROM_MNT_POINT}"
        IMG_FILE=${CDROM_MNT_POINT}/image.img
        MANUF_DIR=${CDROM_MNT_POINT}/manufacture
        DMI_DIR=${CDROM_MNT_POINT}/dmi
        break   
      fi
      umount ${CDROM_MNT_POINT} > /dev/null 2> /dev/null
    fi
  done
fi
if [ "${IMG_FILE}" = "none" ] ; then
  exit 0
fi
Log Found IMG_FILE=${IMG_FILE}
if [ ! -d ${MANUF_DIR} ] ; then
   Fail_Print Invalid installation media.
   exit 1
fi
echo ${IMG_FILE} > /tmp/img.filename
Log Found MANUF_DIR=${MANUF_DIR}

cd ${MANUF_DIR}
for i in `find * -type f`
do
  if [ -f /${i} -o -L /${i} ] ; then
    if [ ! -f /${i}-orig -a ! -L /${i}-orig ] ; then
      Log mv /${i} /${i}-orig
      mv /${i} /${i}-orig
    fi
  fi
  rm -f /${i}
  D=`dirname /${i}`
  [ ! -d ${D} ] && mkdir -p ${D}
  Log ln -s `pwd`/${i} /${i}
  ln -s `pwd`/${i} /${i}
  chmod +x `pwd`/${i} /${i} > /dev/null 2> /dev/null
done

# Now copy symlinks
for i in `find * -type l`
do
  if [ -f /${i} -o -L /${i} ] ; then
    if [ ! -f /${i}-orig -a ! -L /${i}-orig ] ; then
      Log mv /${i} /${i}-orig
      mv /${i} /${i}-orig
    fi
  fi
  rm -f /${i}
  D=`dirname /${i}`
  [ ! -d ${D} ] && mkdir -p ${D}
  Log cp -d `pwd`/${i} /${i}
  cp -d `pwd`/${i} /${i}
done

# Patch using patch files just installed.
if [ -f /sbin/grub-install.rej ] ; then
    echo
    Fail_Print !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    Fail_Print Cannot install because patch previously failed: /sbin/grub-install
    Fail_Print !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    echo
    exit 2
fi
if [ -f /sbin/grub-install.patch ] ; then
  if [ -f /sbin/grub-install.orig ]  ; then
    echo Already Patched: /sbin/grub-install
  else
    cd /sbin
    Log Patch /sbin/grub-install
    rm -f grub-install.rej
    patch < grub-install.patch >> ${LOGF} 2>&1
    if [ -f grub-install.rej ] ; then
      echo
      Fail_Print !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
      Fail_Print Cannot install because patch failed: /sbin/grub-install
      Fail_Print !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
      echo
      exit 2
    fi     
  fi
fi

if [ ! -d ${DMI_DIR} ] ; then
  Log Not Found DMI_DIR=${DMI_DIR}
else
  Log Found DMI_DIR=${DMI_DIR}
  rm -rf /tmp/dmi
  cd ${DMI_DIR}
  for i in `find * -type f`
  do
    D=`dirname /tmp/dmi/${i}`
    [ ! -d ${D} ] && mkdir -p ${D}
    Log \
    ln -s `pwd`/${i} /tmp/dmi/${i}
    ln -s `pwd`/${i} /tmp/dmi/${i}
  done
  ls -l /tmp/dmi >> ${LOGF}
fi

if [ -f ${CDROM_MNT_POINT}/${AUTORUN} ]; then
  Log_Print "Running cdrom autorun script: ${CDROM_MNT_POINT}/${AUTORUN}"
  sh ${CDROM_MNT_POINT}/${AUTORUN}
fi
exit 0
