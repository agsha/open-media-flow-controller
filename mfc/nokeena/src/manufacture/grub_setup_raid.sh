#! /bin/bash
# Install grub on an mirror drive.

X=

Install_Raid_Grub()
{
  DID_GRUB_INSTALL=1
  ERR=
  GRUB_INSTALL_SCRIPT=/tmp/grub-install
  cat /sbin/grub-install \
    | sed 's/ --batch/ --verbose --batch/' > ${GRUB_INSTALL_SCRIPT}

  [ "_${X}" != "_" ] && set +x

  echo      ${GRUB_INSTALL_SCRIPT} --no-floppy \
    --root-directory=${mount_point} ${RAID_P1_DEV} >> /tmp/grub.log  2>&1
  bash ${X} ${GRUB_INSTALL_SCRIPT} --no-floppy \
    --root-directory=${mount_point} ${RAID_P1_DEV} >> /tmp/grub.log  2>&1
  RV=${?}
  echo RV=${RV} >> /tmp/grub.log
  [ ${RV} -ne 0 ] && ERR="install-grub"

  # Install the grub.
  # Note: When running grub interactively, use --no-curses instead of --batch
  echo Do /sbin/grub --batch >> /tmp/grub.log
  /sbin/grub --batch >> /tmp/grub.log 2>&1 <<EOF
root (hd0,0)
setup (hd0)
root (hd1,0)
setup (hd1)
quit
EOF
  RV=${?}
  [ "_${X}" != "_" ] && set +x

  echo RV=${RV} >> /tmp/grub.log
  [ ${RV} -ne 0 ] && ERR="${ERR} grub"
}

rm -f /tmp/grub.log
RAID_DEV=/dev/md
grep -q ^RAID_DEV= /tmp/raid_disk_info.txt
if [ ${?} -eq 0 ] ; then
  . /tmp/raid_disk_info.txt
fi
RAID_P1_DEV=${RAID_DEV}1

DID_MOUNT=no
if [ -z "${mount_point}" ] ; then
  mount_point=/tmp/m1
  [ ! -d ${mount_point} ] && mkdir ${mount_point}
  mount ${RAID_P1_DEV} ${mount_point} >> /tmp/grub.log 2>&1
  DID_MOUNT=yes
else
  if [ ! -d ${mount_point}/boot ] ; then
    mount ${RAID_P1_DEV} ${mount_point} >> /tmp/grub.log 2>&1
    DID_MOUNT=yes
  fi
fi

Install_Raid_Grub

if [ "${DID_MOUNT}" = "yes" ] ; then
  umount ${RAID_P1_DEV}
fi
if [ ! -z "${ERR}" ] ; then
  echo "ERROR: ${ERR}"
  echo "Failed: ${ERR}" > /tmp/grub_install_failed.txt
  cat /tmp/grub.log
  exit 1
fi
exit 0
