:
# Do needed fixup adjustments to the system, before the Samara PM is run.
PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH
umask 0022

export LOGF=/log/init/fixups.log
VXA_MODEL_FILE=/etc/vxa_model
VXA_NAME_FILE=/etc/vxa_name
PACIFICA_HW_FILE=/etc/pacifica_hw

[ -f /etc/build_version.sh ] && . /etc/build_version.sh

# ===========================================================================
# Catch signal and exit. Currently nothing to cleanup
# ===========================================================================
function trap_cleanup_exit()
{
    echo "*** Interrupted by signal"
    exit 1
}
trap "trap_cleanup_exit" HUP INT QUIT PIPE TERM

# ===========================================================================
# Functions
# ===========================================================================

DID_DATE=no
function Log_Date()
{
  [ "${DID_DATE}" = "no" ] && date >> ${LOGF}
  DID_DATE=yes
}

function Make_Dir()
{
  CHMOD=${2}
  CHOWN=${3}
  [ -z "${CHMOD}" ] && CHMOD=755
  [ -z "${CHOWN}" ] && CHOWN=admin:root
  mkdir -p ${1}
  chmod ${CHMOD} ${1}
  chown ${CHOWN} ${1}
  Log_Date
  echo Make_Dir ${1} >> ${LOGF}
}

function Make_Sure_Directory_Exists()
{
  if [ ! -d ${1} ] ; then
    Make_Dir ${1} ${2} ${3}
  else
    [ ! -z "${2}" ] && chmod ${2} ${1}
    [ ! -z "${3}" ] && chown ${3} ${1}
  fi
}

function Move_It()
{
  Log_Date
  echo Move_It ${*} >> ${LOGF}
  mv ${@} 2>> ${LOGF}
}

function Symlink_File()
{
  SRC_FULLPATH=${1}
  TARGET_FULLPATH=${2}

  if [ ! -L ${TARGET_FULLPATH} ]; then
      ln -s ${SRC_FULLPATH} ${TARGET_FULLPATH}
  fi

}
# Replace a regular directory with a symlink to another location.
function Symlink_Dir()
{
  A_FULLPATH=${1}
  B_FULLPATH=${2}
  A_PARENT=`dirname ${A_FULLPATH}`
  B_PARENT=`dirname ${B_FULLPATH}`
  Make_Sure_Directory_Exists ${A_PARENT}
  Make_Sure_Directory_Exists ${B_PARENT}

  if [ -h ${A_FULLPATH} ] ; then
    # The symlink exists.  Assume that it is correct.
    # Make sure the target exists.
    Make_Sure_Directory_Exists ${B_FULLPATH}
  else
    # Either it is a directory (expected) or it does not exist (improbable).
    # When it does not exist, then make it so that the logic works.
    Make_Sure_Directory_Exists ${A_FULLPATH}
    # When the target does not exists, move the dir from the first location.
    if [ ! -d ${B_FULLPATH} ] ; then
        Move_It ${A_FULLPATH} ${B_FULLPATH}
        # Just in case that failed, make sure directory exists.
        Make_Sure_Directory_Exists ${B_FULLPATH}
    else
        # Both directories exist, so move the contents.
        Move_It ${A_FULLPATH}/*    ${B_FULLPATH}
        Move_It ${A_FULLPATH}/.??* ${B_FULLPATH}
    fi
    cd ${A_PARENT}
    AA=`basename ${A_FULLPATH}`
    rm -rf ${AA}
    ln -s ${B_FULLPATH} ${AA}
    Log_Date
    echo In:`pwd` >> ${LOGF}
    echo ln -s ${B_FULLPATH} ${AA} >> ${LOGF}
  fi
  # Make sure the direcories have proper mode and ownership.
  chmod 755        ${B_FULLPATH}
  chown admin:root ${B_FULLPATH}
} # Symlink_Dir


function Cleanup_lockfiles()
{
    rm -rf /config/nkn/.ooriginmgr_pid
    rm -rf /config/nkn/.vpemgr_pid

    # Remove all the namespace object list and lock files
    rm -f /var/log/nkn/cache/*.lck
    rm -f /var/log/nkn/cache/*.lst
}

# Make sure the root filesystem is writeable.
# Save the previous state, so can put it back.
SAVE_STATE=unknown
Root_RW()
{
  touch /test_for_read_only_root 2> /dev/null
  if [ ${?} -ne 0 ] ; then
    SAVE_STATE=ro
    echo Remount root rw
    echo Remount root rw >> ${LOGF}
    mount -o remount,rw /
  else
    SAVE_STATE=rw
  fi
}

Root_Restore()
{
  if [ "${SAVE_STATE}" = "ro" ] ; then
    rm -f /test_for_read_only_root 2> /dev/null
    echo Remount root readonly
    echo Remount root readonly >> ${LOGF}
    mount -o remount,ro /
  fi
}

# When the /dev/cciss directory exists, then we need to create new
# node names for the "whole drive" device names.
# This needs to be done is because (1) the cciss devices do not use the same
# naming convention as the # /dev/sda, /dev/sdb, ... device names, where
# the partition device names just have the partition number appended.
# (e.g. /dev/sda1), and (2) there is code that depends on this naming
# convention.
# So by creating new matching node names for all "c<N>d<N>" names by appending
# a "p" to the name, the new node name can be used to fit this convention.
# E.g.:   ln c0d0 c0d0p
# This needs to be done every time the system is started up because the
# device names in /dev/cciss seem to be automatically remade when the system
# is rebooted.
Create_cciss_dev()
{
  [ ! -d /dev/cciss ] && return
  cd /dev/cciss
  for DEV in `ls c*d* | grep -v p`
  do
    [ -b ${DEV}p ] && continue
    echo ln ${DEV} ${DEV}p
    echo ln ${DEV} ${DEV}p >> ${LOGF}
    ln ${DEV} ${DEV}p
  done
}

Update_HW_Info()
{
  if [ "${PACIFICA_HW}" = "yes" -a ! -f ${PACIFICA_HW_FILE} ] ; then
    echo PACIFICA_HW=${PACIFICA_HW} > ${PACIFICA_HW_FILE}
  fi
  if [ "${IS_VXA}" = "yes" -a ! -f ${VXA_MODEL_FILE} ] ; then
    echo VXA_MODEL="${VXA_MODEL}" > ${VXA_MODEL_FILE}
    echo VXA_NAME="${VXA_NAME}"   > ${VXA_NAME_FILE}
  fi
}

Fix_StorMan()
{
  # Fix up the file modes in /usr/StorMan
  ls -l /usr/StorMan/* >> ${LOGF}
  chmod ugo-w /usr/StorMan/*
  chmod ugo+r /usr/StorMan/*
  chmod 555   /usr/StorMan/arcconf /usr/StorMan/hrconf
  # Symlink the cli log file to a file in /log
  cd /usr/StorMan
  A=UcliEvt.log
  if [ ! -h ${A} ] ; then
    [ -f ${A} ] && Move_It ${A} /log
    ln -s /log/${A} ${A}
    ls -l >> ${LOGF}
  fi
}

Create_Cron_Link()
{
  # Create a symlink to the cron entry for sys monitor
  if [ ! -L /var/opt/tms/output/cron.d/nkn_sys_mon.cron ]; then
    ln -s /opt/nkn/bin/nkn_sys_mon.cron /var/opt/tms/output/cron.d
  fi
}

Remove_Cron_Link()
{
  # Since we are removing the drop_cache cron job need to 
  # remove the soft link if it exists
  if [ -L /var/opt/tms/output/cron.d/drop_cache.cron ]; then
    rm /var/opt/tms/output/cron.d/drop_cache.cron
  fi
}

HW_Check()
{
  # Note that this is run very early before we make the root fs writeable.
  SMFG=`/usr/sbin/dmidecode -s system-manufacturer`
  SPNAME=`/usr/sbin/dmidecode -s system-product-name`
  SERIAL_NUMBER=`/usr/sbin/dmidecode -s system-serial-number`
  SUUID=`/usr/sbin/dmidecode -s system-uuid`

  case _${SMFG} in
  _Not*Specified*|_To*be*filled*by*|_) SMFG=NA ;;
  esac
  case _${SPNAME} in
  _Not*Specified*|_To*be*filled*by*|_) SPNAME=NA ;;
  esac

  IS_VXA=no
  IS_ROOT_DEVICE_CHANGEABLE=no
  USE_EUSB=no
  VXA_MODEL=
  VXA_NAME=

  PACIFICA_HW=no
  if [ -f ${PACIFICA_HW_FILE} ] ; then
    PACIFICA_HW=yes
    return
  else
    cmdline=`cat /proc/cmdline`
    if strstr "${cmdline}" model=pacifica ; then
      PACIFICA_HW=yes
      return
    fi
  fi
  # Not pacifica at this point.

  if [ -f ${VXA_MODEL_FILE} ] ; then
    eval `grep ^VXA_MODEL= ${VXA_MODEL_FILE}`
    eval `grep ^VXA_NAME= ${VXA_NAME_FILE}`
    IS_VXA=yes
  else
    case ${SMFG} in
    Juniper\ *)
      VXA_MODEL=`echo "${SERIAL_NUMBER}" | cut -c1-4`
      case ${VXA_MODEL} in
      0277) VXA_NAME=VXA2002 ; IS_VXA=yes ;;
      0278) VXA_NAME=VXA2010 ; IS_VXA=yes ;;
      0279) VXA_NAME=VXA1001 ; IS_VXA=yes ;;
      0280) VXA_NAME=VXA1002 ; IS_VXA=yes ;;
      0304) VXA_NAME=VXA1100 ; IS_VXA=yes ;;
      0305) VXA_NAME=VXA2100 ; IS_VXA=yes ;;
      0306) VXA_NAME=VXA2200 ; IS_VXA=yes ;;
      *)    VXA_MODEL= ;;
      esac
      ;;
    esac
  fi
  if [ "${IS_VXA}" = "yes" ] ; then
    # See if the disk layout model is a type that uses the eUSB as a boot drive 
    . /etc/image_layout.sh
    case ${IL_LAYOUT} in
    FLASH*) USE_EUSB=boot ;;
    esac
    if [ "${VXA_MODEL}" = "0305" ] ; then
      IS_ROOT_DEVICE_CHANGEABLE=yes
      Update_SystemData_Disk_Info
    fi
    return
  fi
  # Not VXA and Not pacifica at this point.
}

function Update_SystemData_Disk_Info()
{
  local CNT_FOUND
  local CNT_SLOTS
  local THIS_SLOT
  local NKN_DEV
  local SD_DEV
  local SLOT_TO_DEV_FILE
  local PREV_SYSDATA_DEV
  local SYSDATA_DEV_NAME
  local SERIAL_FILE
  local SERIAL_STRING
  # On systems where the root drive device name could change
  # when rebooted, such as when SSD drive are added or removed,
  # update the /config/nkn/system-data-disk.info file.
  # Currently this only happens on model 0305 (VXA2100=VXA2000) systems.
  [ "${IS_ROOT_DEVICE_CHANGEABLE}" != "yes" ] && return
  # If we get here we are on a VXA model 0305.
  if [ -f /config/nkn/2-disk.info ] ; then
    # Rename these two files so that code in Create_eUSB_File()
    # does not overwrite what we are putting into system-data-*disk.info
    mv /config/nkn/2-disk.info  /config/nkn/2-disk.info.orig
    mv /config/nkn/2-pdisk.info /config/nkn/2-pdisk.info.orig
  fi

  # Note that the drive currently that has /nkn mounted might
  # not be the correct one.  So we have to look for the proper one.

  # We need to do special processing to get the slot ordering.
  # Generate the file that lets us find out the physical location of a HDD.
  # This currently only works with model 0305 because we have a utiltiy that
  # can give this info for the controller that it uses.
  echo "VXA model 0305 detect_hdd" | tee -a ${LOGF}
  SLOT_TO_DEV_FILE=/config/nkn/slot_to_dev.txt
  bash /opt/nkn/generic_cli_bin/detect_hdd.sh -a -q > ${SLOT_TO_DEV_FILE}

  # See if the device specified in /config/nkn/system-data-disk.info is correct.
  PREV_SYSDATA_DEV=`cat /config/nkn/system-data-disk.info`
  # See if the file that keeps the serial number of the system disk exists.
  # If so, find the device with that serial number.
  SYSDATA_DEV_NAME=

  SERIAL_FILE=/config/nkn/system-data-serial.info
  [ ! -s ${SERIAL_FILE} ] && rm -f ${SERIAL_FILE} 
  if [ -f ${SERIAL_FILE} ] ; then
    SERIAL_STRING=`cat ${SERIAL_FILE}`
    # First clear all the varibles that will be set.
    eval `grep 'D_CONTROLLER=;' ${SLOT_TO_DEV_FILE}`
    eval `cat ${SLOT_TO_DEV_FILE} \
      | grep "D_SER_NUM=\"${SERIAL_STRING}\";" \
      | grep "D_CONTROLLER=0;"`
    SYSDATA_DEV_NAME=${D_DEVNAME}
    if [ ! -z "${SYSDATA_DEV_NAME}" ] ; then
      if [ "${SYSDATA_DEV_NAME}" = "${PREV_SYSDATA_DEV}" ]  ; then
        # All ok as is
        return
      fi
      echo ${SYSDATA_DEV_NAME} > /config/nkn/system-data-disk.info
      echo ${SYSDATA_DEV_NAME} > /config/nkn/system-data-pdisk.info
      return
    fi
    # It is not expected to get here.
    logger -s "Error, did not find disk with serial ${SERIAL_STRING}"
  fi
  # No serial number was stored, so figure out what the proper
  # system-data disk is using the info in the slot-to-dev.txt file.
  # The default is the first device by slot number that is not an SSD.
    
  # Get details of the first enclosure.
  E_NUMSLOTS=
  E_STARTSLOT=
  # Now find and set the E_* variables from the found line.
  eval `cat ${SLOT_TO_DEV_FILE} | grep 'E_ENCLOSURE_NUM=1;' | head -1`
  if [ -z "${E_NUMSLOTS}" ] ; then
    # This should never happen, but just in case use something.
    E_NUMSLOTS=40
    E_STARTSLOT=0
  fi

  # Find the first slot with a non-SSD drive, in the first enclosure and
  # see if it has the proper labels.
  rm -f /tmp/ssd_drvs.list 
  THIS_SLOT=$(( E_STARTSLOT - 1 ))
  CNT_SLOTS=1
  while [ ${CNT_SLOTS} -le ${E_NUMSLOTS} ] 
  do
    (( THIS_SLOT++ ))
    (( CNT_SLOTS++ ))
    # First clear all the varibles that will be set.
    eval `grep 'D_CONTROLLER=;' ${SLOT_TO_DEV_FILE}`
    # Now find and set the D_* variables from the found line.
    eval `cat ${SLOT_TO_DEV_FILE} \
      | grep 'D_ENCLOSURE=1;' \
      | grep "D_SLOT=${THIS_SLOT};"`
    case "${D_DRV_TYPE}" in
    SAS_SSD|SATA_SSD)
      echo ${D_DEVNAME} >> /tmp/ssd_drvs.list 
      continue
      ;;
    esac
    CNT_FOUND=0
    for i in ${D_DEVNAME}[1-7]
    do 
      [ ! -b ${i} ] && continue
      L=`e2label ${i} 2> /dev/null`
      case ${L} in
      COREDUMP|NKN|LOG) (( CNT_FOUND++ )) ;;
      esac
    done
    if [ ${CNT_FOUND} -eq 3 ] ; then
      # Found it
      echo ${D_DEVNAME} > /config/nkn/system-data-disk.info
      echo ${D_DEVNAME} > /config/nkn/system-data-pdisk.info
      echo ${D_SER_NUM} > ${SERIAL_FILE}
      return
    fi
  done
  # The only reason we get here is if somehow the sysdata disk is on an SSD
  # which is not a good idea, or was not found in the first enclosure, which
  # should not be because the system will not boot if the COREDUMP, NKN and
  # LOG labeled filesystems are not found.  So consider the SSDs we skipped.
  if [ -s /tmp/ssd_drvs.list ] ; then
    cat /tmp/ssd_drvs.list | while read ITEM
    do
      CNT_FOUND=0
      for i in ${ITEM}[1-7]
      do 
        [ ! -b ${i} ] && continue
        L=`e2label ${i} 2> /dev/null`
        case ${L} in
        COREDUMP|NKN|LOG) (( CNT_FOUND++ )) ;;
        esac
      done
      if [ ${CNT_FOUND} -eq 3 ] ; then
        # Found it
        echo ${D_DEVNAME} > /config/nkn/system-data-disk.info
        echo ${D_DEVNAME} > /config/nkn/system-data-pdisk.info
        echo ${D_SER_NUM} > ${SERIAL_FILE}
        return
      fi
   done
  fi
  # Not found.  This should never happen.
  # Use the disk that has /nkn currently mounted.
  NKN_DEV=`df /nkn | grep /nkn | awk '{print $1}'`
  SD_DEV=`echo ${NKN_DEV%%[0-9]}`
  echo ${SD_DEV} > /config/nkn/system-data-disk.info
  echo ${SD_DEV} > /config/nkn/system-data-pdisk.info
  D_SER_NUM=
  eval `cat ${SLOT_TO_DEV_FILE} \
      | grep "D_DEVNAME=${SD_DEV};"`
  echo ${D_SER_NUM} > ${SERIAL_FILE}
}

#
# In the case of VXA with eUSB device the root is on this device
# and eUSB always shows up as the last device. The file /etc/image_layout.sh
# has a variable that stores the device of the root. Typically we install root
# the first drive (sda) hence there are no issues. In the case of eUSB, since 
# it shows as the last device, if new drives are added or drives are removed
# the device name of eUSB would change. This file needs to be updated in such 
# cases to ensure the variable always has the correct root device. 
#
Update_ImageLayout_File()
{
  # This is only for VXA that boots from eUSB.
  [ "${IS_VXA}" = "no" ] && return
  [ "${USE_EUSB}" != "boot" ] && return

  # Using a disk layout that boots from eUSB.
  # First get the current eUSB device name.
  local BOOT_DISK_DEV=""
  for disk in /sys/block/sd*
  do
    grep eUSB $disk/device/model > /dev/null 2> /dev/null
    if [ $? -eq 0 ]; then
      BOOT_DISK_DEV=/dev/`basename $disk`
      break
    fi
    grep PSH16H0S2  $disk/device/model > /dev/null 2> /dev/null
    if [ $? -eq 0 ]; then
      BOOT_DISK_DEV=/dev/`basename $disk`
      break
    fi
  done
  if [ ! -z "${BOOT_DISK_DEV}" ] ; then
    echo "Found eUSB ($BOOT_DISK_DEV)"
    echo "Found eUSB ($BOOT_DISK_DEV)" >> ${LOGF}
  fi

  # Confirm we found the device
  if [ "_$BOOT_DISK_DEV" = "_" ]; then
    echo "VXA model but no eUSB found"
    echo "VXA model but no eUSB found" >> ${LOGF}
    return
  fi

  # Verify if it the same as the one in image layout file
  IMAGELAYOUT_FILE=/etc/image_layout.sh
  grep =${BOOT_DISK_DEV} ${IMAGELAYOUT_FILE} > /dev/null
  if [ ${?} -eq 0 ] ; then
    # It is the same.
    return
  fi
  # Modify image layout file with the correct root device name
  cat ${IMAGELAYOUT_FILE} | sed -e s@TARGET_DISK1_DEV=.*@TARGET_DISK1_DEV=${BOOT_DISK_DEV}@g > ${IMAGELAYOUT_FILE}.bak
  mv ${IMAGELAYOUT_FILE}.bak ${IMAGELAYOUT_FILE}

  # Also update /tmp/eusb-root-dev.txt file
  echo ${BOOT_DISK_DEV} > /tmp/eusb-root-dev.txt

  echo "eUSB root device changed to ${BOOT_DISK_DEV} since last boot, modifying ${IMAGELAYOUT_FILE}"
  echo "eUSB root device changed to ${BOOT_DISK_DEV} since last boot, modifying ${IMAGELAYOUT_FILE}" >> ${LOGF}
}	# Update_ImageLayout_File

#
# With every boot, the eUSB device can change as drives are added or
# subtracted.  This happens because the eUSB device is the last device
# in the /dev/sd* naming scheme.  The identity of the eUSB file is written
# to a file, so other pieces of code don't need to understand how to identify
# it.
#
# This file is not normally run by hand and initdisks.sh is run by hand.
# However, creating the eUSB file in this script is OK because the eUSB
# device can not change between reboots.
#
Create_eUSB_File()
{
  # This is only for VXA.
  [ "${IS_VXA}" = "no" ] && return
  
  # Check if any of the device is an eUSB device
  local FOUND_EUSB=0
  local EUSB_DISK_DEV

  for disk in /sys/block/sd*; do
    grep eUSB ${disk}/device/model > /dev/null 2> /dev/null
    if [ $? -eq 0 ]; then
      EUSB_DISK_DEV=/dev/`basename $disk`
      break
    fi
    grep PSH16H0S2  $disk/device/model > /dev/null 2> /dev/null
    if [ $? -eq 0 ]; then
      EUSB_DISK_DEV=/dev/`basename $disk`
      break
    fi
  done
  if [ ! -z "${EUSB_DISK_DEV}" ] ; then
    FOUND_EUSB=1
    logger "Found eUSB ($EUSB_DISK_DEV) during boot"
    echo ${EUSB_DISK_DEV} > /config/nkn/eusb-disk.info
    echo ${EUSB_DISK_DEV} > /config/nkn/eusb-pdisk.info
    if [ "${USE_EUSB}" = "boot" ] ; then
      # Using a disk layout model that uses eUSB as boot drive, so update
      # /config/nkn/boot-disk.info and boot-pdisk.info.
      echo ${EUSB_DISK_DEV} > /config/nkn/boot-disk.info
      echo ${EUSB_DISK_DEV} > /config/nkn/boot-pdisk.info
    fi
  fi
  if [ ${FOUND_EUSB} -eq 0 ]; then
    logger -s "No eUSB device found for VXA system"
  elif [ "${USE_EUSB}" = "boot" ] ; then
    # Before final release version MFC 12.2.0, the system-data-disk.info
    # was not correctly set from the 2-disk.info file.
    # So the first time here, fix it up.
    if [ -f /config/nkn/2-disk.info ] ; then
      cp /config/nkn/2-disk.info  /config/nkn/system-data-disk.info
      cp /config/nkn/2-pdisk.info /config/nkn/system-data-pdisk.info
      mv /config/nkn/2-disk.info  /config/nkn/2-disk.info.orig
      mv /config/nkn/2-pdisk.info /config/nkn/2-pdisk.info.orig
    fi
  fi
}	# Create_eUSB_File

# When on a VXA that boots from the eUSB we MUST clean up the labels on all
# the disks so that labels used on eUSB and the root drive are not used on
# any other disk.
# This is because in order to boot from the eUSB and have it find the
# proper filesystems it has to find them using filesystem labels.
#
# If we change any labels here, then we need to reboot immediately.
# If we do not do this cleanup then we may have gotten partitions mounted
# from non-eUSB drives that must be mounted from the eUSB.
Cleanup_Labels_reboot()
{
  # This is only for VXA that boots from eUSB.
  [ "${IS_VXA}" = "no" ] && return
  [ "${USE_EUSB}" != "boot" ] && return
  FOUND_CONFLICT=0
  #
  SYSDATA_DEV=`cat /config/nkn/system-data-pdisk.info`
  # Fix system-data partition conflicts.
  for SYSDATA_DEV_PART in ${SYSDATA_DEV}[1-9] ${SYSDATA_DEV}[1-9][0-9]
  do
    [ ! -b ${SYSDATA_DEV_PART} ] && continue
    ONE_SYSDATA_LABEL=`e2label ${SYSDATA_DEV_PART} 2>/dev/null`
    [ -z "${ONE_SYSDATA_LABEL}" ] && continue
    for HDD_DEV in /dev/sd*[a-z] /dev/c0d[0-9] /dev/c0d[1-9][0-9]
    do
      [ ! -b ${HDD_DEV} ] && continue
      [ "${HDD_DEV}" = "${EUSB_DEV}" ]    && continue
      [ "${HDD_DEV}" = "${SYSDATA_DEV}" ] && continue
      for HDD_PART in ${HDD_DEV}[1-9] ${HDD_DEV}[1-9][0-9]
      do
        HDD_LABEL=`e2label ${HDD_PART} 2>/dev/null`
        [ -z "${HDD_LABEL}" ] && continue
        [ "${ONE_SYSDATA_LABEL}" != "${HDD_LABEL}" ] && continue
        logger -s Cleanup label conflict with ${SYSDATA_DEV}: ${ONE_SYSDATA_LABEL} ${HDD_PART}
        FOUND_CONFLICT=1
        e2label ${HDD_PART} X_${HDD_LABEL} 2> /dev/null
      done
    done
  done
  # Fix eUSB partition conflicts.
  if [ -f /config/nkn/eusb-disk.info ] ; then
    EUSB_DEV=`cat /config/nkn/eusb-disk.info`
    for EUSB_PART in ${EUSB_DEV}[1-9] ${EUSB_DEV}[1-9][0-9]
    do
      [ ! -b ${EUSB_PART} ] && continue
      EUSB_LABEL=`e2label ${EUSB_PART} 2>/dev/null`
      [ -z "${EUSB_LABEL}" ] && continue
      for HDD_DEV in /dev/sd*[a-z] /dev/c0d[0-9] /dev/c0d[1-9][0-9]
      do
        [ "${HDD_DEV}" = "${EUSB_DEV}" ] && continue
        [ "${HDD_DEV}" = "${SYSDATA_DEV}"    ] && continue
        for HDD_PART in ${HDD_DEV}[1-9] ${HDD_DEV}[1-9][0-9]
        do
          HDD_LABEL=`e2label ${HDD_PART} 2>/dev/null`
          [ -z "${HDD_LABEL}" ] && continue
          [ "${EUSB_LABEL}" != "${HDD_LABEL}" ] && continue
          logger -s Cleanup label conflict with eUSB: ${EUSB_LABEL} ${HDD_PART}
          FOUND_CONFLICT=1
          e2label ${HDD_PART} X_${HDD_LABEL} 2> /dev/null
        done
      done
    done
  fi
  [ ${FOUND_CONFLICT} -eq 0 ] && return
  logger -s Rebooting after cleaning up labels
  shutdown -r -f now
  sleep 10
  sleep 10
  sleep 10
  exit
}

# Update the files in /config/nkn that tell which devices are used by RAID.
# When RAID is not used then remove the files.
#   raid-parts-list.txt  = list of all the partition drive names used by RAID.
#   raid-disks-list.txt  = list of the drive device names for the base devices.
#   raid-pdisks-list.txt = list of the drive device names for the base devices
#                          that you append the partition number to to form
#                          the device name of the parition.
Determine_Raid_Drives()
{
  TPF=/tmp/raid-parts-list.tmp
  TF_SD=/tmp/raid-sd-disk-list.tmp
  TF_CCISS=/tmp/raid-cciss-disk-list.tmp
  TF_CCISS_P=/tmp/raid-cciss-pdisk-list.tmp
  TMP_F=/tmp/f-list.tmp
  TMP_D=/tmp/d-list.tmp
  TMP_P=/tmp/p-list.tmp
  rm -f ${TPF} ${TF_SD} ${TF_CCISS} ${TF_CCISS_P}
  rm -f ${TMP_F} ${TMP_D} ${TMP_P}
  for i in `cat /proc/mdstat | grep ^md | cut -f 5- -d' '`
  do
     [ -z "${i}" ] && continue
     V=`echo "${i}" | cut -f1 -d'['`
     echo "/dev/${V}" >> ${TPF}
     case "${V}" in
     sd*|hd*)
        W=`echo ${V} | sed "s/[1-9][0-9]*//"`
        echo /dev/${W} >> ${TF_SD}
        ;;
     cciss/c*d*p*)
        W=`echo ${V} | sed "s/p[0-9]*//"`
        echo /dev/${W} >> ${TF_CCISS}
        W=`echo ${V} | sed "s/p[0-9]*/p/"`
        echo /dev/${W} >> ${TF_CCISS_P}
        ;;
     esac
  done
  if [ ! -f ${TPF} ] ; then
    rm -f /config/nkn/raid-parts-list.txt
    rm -f /config/nkn/raid-disks-list.txt
    rm -f /config/nkn/raid-pdisks-list.txt
    return
  fi
  cat ${TPF} | sort | uniq > ${TMP_F}
  [ ! -f /config/nkn/raid-parts-list.txt ] && \
    echo Unknown > /config/nkn/raid-parts-list.txt
  cmp -s ${TMP_F} /config/nkn/raid-parts-list.txt
  if [ ${?} -ne 0 ] ; then
    cat ${TMP_F} > /config/nkn/raid-parts-list.txt
    chmod 644 /config/nkn/raid-parts-list.txt
  fi
  # ------------------------------------------------
  if [ -f ${TF_SD} ] ; then
    cat ${TF_SD} >> ${TMP_D}
    cat ${TF_SD} >> ${TMP_P}
  fi
  if [ -f ${TF_CCISS} ] ; then
    cat ${TF_CCISS} >> ${TMP_D}
  fi
  if [ -f ${TF_CCISS_P} ] ; then
    cat ${TF_CCISS_P} >> ${TMP_P}
  fi
  cat ${TMP_D} | sort | uniq > /tmp/fixups.sort.tmp
  cat /tmp/fixups.sort.tmp > ${TMP_D}
  cat ${TMP_P} | sort | uniq > /tmp/fixups.sort.tmp
  cat /tmp/fixups.sort.tmp > ${TMP_P}
  rm -f /tmp/fixups.sort.tmp
  DL=/config/nkn/raid-disks-list.txt
  [ ! -f ${DL} ] && echo "NA" > ${DL}
  cmp -s ${TMP_D} ${DL}
  if [ ${?} -ne 0 ] ; then
    cat ${TMP_D} > ${DL}
    chmod 644 ${DL}
  fi
  DL=/config/nkn/raid-pdisks-list.txt
  [ ! -f ${DL} ] && echo "NA" > ${DL}
  cmp -s ${TMP_P} ${DL}
  if [ ${?} -ne 0 ] ; then
    cat ${TMP_P} > ${DL}
    chmod 644 ${DL}
  fi
  rm -f ${TPF} ${TF_SD} ${TF_CCISS} ${TF_CCISS_P}
  rm -f ${TMP_F} ${TMP_D} ${TMP_P}
}

# Determine which drives are to be NOT used for Cache or Persistent Store.
# This includes boot, sysdata, eusb, and drives in RAID.
# Put the drive info in the files
#   /config/nkn/reserved-disks-list.txt
#   /config/nkn/reserved-pdisks-list.txt
# The -disk- list has the device names for the entire drive.
# The -pdisk- list has the device name that you append a
#             partition number to form the device name of a partition.
# Note:
#   Persistant data drives can be added and removed during runtime.
#   Thus any code that looks at reserved-disks-list.txt will also
#   have to deal with the disk device names listed in 
#      /config/nkn/ps-disks-list.txt
#      /config/nkn/ps-pdisks-list.txt
Id_Non_Cache_Drives()
{
  TMP_D=/tmp/fixups.1.tmp
  TMP_P=/tmp/fixups.2.tmp
  rm -f ${TMP_D} ${TMP_P}
  for i in boot system-data eusb
  do
    [ -f  /config/nkn/${i}-disk.info ] && \
      cat /config/nkn/${i}-disk.info  >> ${TMP_D}
    [ -f  /config/nkn/${i}-pdisk.info ] && \
      cat /config/nkn/${i}-pdisk.info >> ${TMP_P}
  done
  #
  [ -f  /config/nkn/raid-disks-list.txt ] && \
    cat /config/nkn/raid-disks-list.txt >> ${TMP_D}

  [ -f  /config/nkn/raid-pdisks-list.txt ] && \
    cat /config/nkn/raid-pdisks-list.txt >> ${TMP_P}
  #
  cat ${TMP_D} | sort | uniq > /tmp/fixups.sort.tmp
  cat /tmp/fixups.sort.tmp > ${TMP_D}
  cat ${TMP_P} | sort | uniq > /tmp/fixups.sort.tmp
  cat /tmp/fixups.sort.tmp > ${TMP_P}
  rm -f /tmp/fixups.sort.tmp
  #
  REAL=/config/nkn/reserved-disks-list.txt
  NEW=${TMP_D}
  [ ! -f ${REAL} ] && echo "NA" > ${REAL}
  cmp -s ${NEW} ${REAL}
  if [ ${?} -ne 0 ] ; then
    cat ${NEW} > ${REAL}
    chmod 644 ${REAL}
  fi
  REAL_P=/config/nkn/reserved-pdisks-list.txt
  NEW=${TMP_P}
  [ ! -f ${REAL_P} ] && echo "NA" > ${REAL_P}
  cmp -s ${NEW} ${REAL_P}
  if [ ${?} -ne 0 ] ; then
    cat ${NEW} > ${REAL_P}
    chmod 644 ${REAL_P}
  fi
  # Check if mirroring is configured. If yes, add disks in slot 0
  # and 1 to reserved-disks-list.txt irrespective of whether they are
  # currently part of mirror or not. If we don't add them to
  # reserved disks list, initdisks.sh will use that disk for
  # cache purposes and mirror resync will fail.
  grep -q "/dev/md" /config/nkn/boot-disk.info  > /dev/null 2>&1
  if [ ${?} -eq 0 ]; then
    # Get the device names for drives in slot 0 & 1. We can get this
    # info from /config/nkn/slot_to_dev.txt  An example line in this
    # file for slot 0 is:
    # D_CONTROLLER=0; D_SLOT=0; D_DEVNAME=/dev/sdd; D_DRV_TYPE="SAS_HDD"; D_SIZE_VAL="140014/286749487"; D_SIZEUNITS="(inMB)/(insectors)"; D_ENCLOSURE=1; D_MFG="SEAGATE"; D_MODEL_NUM="ST9146803SS"; D_FIRMW_REV="0006"; D_LSI_SER="6SD42W5D"; D_SER_NUM="6SD42W5D0000B20616EN"; D_GUID="5000c5004295a733"; D_SAS_ADDR=5000c50-0-4295-a731; D_PROTOCOL="SAS"; D_STATE="Ready(RDY)";
    SLOT_TO_DEV_FILE=/config/nkn/slot_to_dev.txt
    DEV_0=`grep "D_CONTROLLER=0; D_SLOT=0;" ${SLOT_TO_DEV_FILE}`
    if [ ${?} -eq 0 ]; then
      DEV_0=`echo $DEV_0 | awk '{print $3}' | cut -f2 -d"=" | cut -f1 -d";"`
      grep -q $DEV_0 ${REAL}
      if [ ${?} -eq 1 ]; then
        echo $DEV_0 >> ${REAL}
        echo $DEV_0 >> ${REAL_P}
      fi
    fi
    DEV_1=`grep "D_CONTROLLER=0; D_SLOT=1;" ${SLOT_TO_DEV_FILE}`
    if [ ${?} -eq 0 ]; then
      DEV_1=`echo $DEV_1 | awk '{print $3}' | cut -f2 -d"=" | cut -f1 -d";"`
      grep -q $DEV_1 ${REAL}
      if [ ${?} -eq 1 ]; then
        echo $DEV_1 >> ${REAL}
        echo $DEV_1 >> ${REAL_P}
      fi
    fi
  fi
  rm -f ${TMP_D} ${TMP_P}
}

#
# When Virtualization is enabled, Samara logic adds new service 
# called iptables primarily to insmod ip_conntrack for its use in  NAT. 
# In MFC, we don't plan to use NAT and more so we cannot use ip_conntrack
# as that hinders our performance. Hence, adding the logic to switch off
# the iptables service
#
Disable_ip_conntrack()
{
  # Using chkconfig switch off the iptables service
  chkconfig iptables off
}

#
# Need to load the kernel module for the Myricom's 10GigE. 
# The driver name is myri10ge.
#
Load_myri10ge_ko()
{
  modprobe myri10ge
}

Link_Kdump_image()
{
    if [ -f /etc/sysconfig/kdump ]
    then
	. /etc/sysconfig/kdump
    fi

    KVER=`uname -r | sed -e "s/smp//g"`
    if [ ! -f /boot/vmlinuz-${KVER} ]
    then
	logger -t kdump "Mounting /boot to fixup kexec-kdump image"
	/bin/mount -o remount,rw /boot
	logger -t kdump "Linking /boot/vmlinuz-smp"
	[ ! -f /boot/vmlinuz-${KVER} ] && ln -s /boot/vmlinuz-smp /boot/vmlinuz-${KVER}
	/bin/mount -o remount,ro /boot
    fi
    logger -t kdump "Linking /lib/modules"
    /bin/mount -o remount,rw /
    [ ! -f /lib/modules/${KVER} ] && ln -s /lib/modules/`uname -r` /lib/modules/${KVER}
    if [ ! -f /sbin/mkdumprd.real ] ; then
      # Create the wrapper script for mkdumprd that makes sure
      # the /boot and root filesystems are writeable.
      mv /sbin/mkdumprd /sbin/mkdumprd.orig
      # Change - to avoid errors displayed during kdump init
      #    1. add kernel version to depmod as the kdump kernel doesnot
      #       have the "smp" string.
      #    2. The /etc/localtime will not be present upon first boot. This 
      #       will be created when initial configuration is applied via tms
      cat /sbin/mkdumprd.orig  | sed -e "s/depmod -b \$MNTIMAGE\//depmod -b \$MNTIMAGE\/ \$kernel/g" | grep -v "cp \/etc\/localtime"  > /sbin/mkdumprd.real
      chmod 755 /sbin/mkdumprd.real
      echo '/bin/mount -o remount,rw /boot' >  /sbin/mkdumprd 
      echo '/bin/mount -o remount,rw /'     >> /sbin/mkdumprd 
      echo '/sbin/mkdumprd.real ${@}'       >> /sbin/mkdumprd 
      echo 'RV=${?}'                        >> /sbin/mkdumprd 
      echo '/bin/mount -o remount,ro /boot' >> /sbin/mkdumprd 
      echo '/bin/mount -o remount,ro /'     >> /sbin/mkdumprd 
      echo 'exit ${RV}'                     >> /sbin/mkdumprd 
      chmod 555 /sbin/mkdumprd 
    fi
    # Update the timestamps on these files:
    #   /etc/kdump.conf /boot/vmlinuz-${KVER}* /boot/initrd-${KVER}kdump.img
    # It is important that kdump.conf and the others be newer than initrd.img
    # otherwise kdump will try to rebuild /boot/initrd-${KVER}kdump.img
    # which will fail and kdump then does a panic.
    touch -m /etc/kdump.conf
    /bin/mount -o remount,ro /
    /bin/mount -o remount,rw /boot
    touch -h -m /boot/vmlinuz-${KVER}
    touch -h -m /boot/vmlinuz-${KVER}-kdump
    touch -h -m /boot/initrd-${KVER}kdump.img
    /bin/mount -o remount,ro /boot
}

# strstr returns 0 (true) if string in $2 is a substring of $1
strstr() {
    [ "${1#*$2*}" = "$1" ] && return 1
    return 0
}

Setup_Pacifica_Resiliency()
{
    # Note that this is run very early before we make the root fs writeable.
    [ "${PACIFICA_HW}" = "no" ]  && return 0
    echo
    echo "Setting up shutdown/reboot system scripts"
    echo
    mv /sbin/reboot /sbin/reboot.orig
    mv /opt/nkn/bin/reboot.pacifica /sbin/reboot
    mv /sbin/shutdown /sbin/shutdown.orig
    mv /opt/nkn/bin/shutdown.pacifica /sbin/shutdown
    chmod a+x /sbin/reboot /sbin/shutdown
    /bin/ln -sf /sbin/reboot /sbin/halt
    /bin/ln -sf /sbin/reboot /usr/bin/reboot
    /bin/ln -sf /sbin/reboot /usr/bin/halt
    /bin/ln -sf /sbin/shutdown /usr/bin/poweroff
    modprobe i2c-dev
    modprobe coretemp
}


Setup_Pacifica()
{
  # Note that this is run very early before we make the root fs writeable.
  [ "${PACIFICA_HW}" = "no" ]  && return 0

  echo 
  echo "MFC running on Pacifica ..."
  echo 

  SYSTEM_DATA_DEVICE=/dev/sda
  ROOT_CACHE_FS_DEVICE=/dev/sda7
  ROOT_CACHE_RAW_DEVICE=/dev/sda8

  # Changing /dev/random to use /dev/urandom
  mv /dev/random /dev/random.original
  ln -s /dev/urandom /dev/random
  
  # Load Broadcom driver 
  echo "Loading Broadcom driver ..."
  if [ -f /config/nkn/packet_mode_filter ]; then
    # Load pf ring
    echo "Loading pf_ring"
    rmmod bnx2x
    modprobe pf_ring
    modprobe -C /etc/modprobe_urif.conf bnx2x
  else
    modprobe bnx2x
  fi

  # Load mxc led driver
  echo "Loading MXC Led driver"
  modprobe pacifica_gpio

  # Sync the clock using NTP Server on RE
  # Need to have a way to find RE IP instead of hardcoding it
  echo "Syncing clock with NTP server in RE ..."
  ntpdate 128.0.0.1

  # Setup the system, boot and root file info in /config/nkn/
  # Need to do it only if we are using /dev/sda 
  # Should typically run only once. The first time sda is being setup.
  if [ ! -f /etc/.pacifica.ramdisk ] &&
      [ ! -f  /config/nkn/system-data-disk.info ]; then
    echo "${SYSTEM_DATA_DEVICE}" > /config/nkn/system-data-disk.info;
    echo "${SYSTEM_DATA_DEVICE}" > /config/nkn/system-data-pdisk.info;
    echo "${SYSTEM_DATA_DEVICE}" > /config/nkn/boot-disk.info;
    echo "${SYSTEM_DATA_DEVICE}" > /config/nkn/boot-pdisk.info;
    echo "${ROOT_CACHE_FS_DEVICE}" > /config/nkn/root_cache_fs;
    echo "${ROOT_CACHE_RAW_DEVICE}" > /config/nkn/root_cache_raw;

    # Zero the the first 4K of the DM raw partition 
    dd if=/dev/zero of=${ROOT_CACHE_RAW_DEVICE} bs=512 count=8192 > /dev/null 2>&1

    # Now run the script that sets up the root disk cache
    Make_Sure_Directory_Exists /nkn/tmp
    /opt/nkn/bin/initrootcache_dm2_ext3.sh ${ROOT_CACHE_RAW_DEVICE} ${ROOT_CACHE_FS_DEVICE} 2048 32 90;
  fi

  # Kill the udhcpc client running as we will switch to the native dhclient 
  killall udhcpc
     
  # Find the hostid using dmidecode
  HOSTID=`/opt/nkn/bin/dmidecode-info.sh | grep UUID | cut -f2 -d':' | cut -c2-`;
  
  # Now run the manufacture.sh script to setup samara files
  if [ -z $HOSTID ]; then
    /sbin/manufacture.sh -a -m normal -P -i
   else
    /sbin/manufacture.sh -a -m normal -P -i -h "$HOSTID"
  fi

} # end of Setup_Pacifica()

#
# When building 2.6.32-220 (CentOS 6.2) kernel in Fir which is CentOS 5.7
# Samara installs kmod-kvm RPM which installs stock kvm.ko of 5.7 and 
# hence fails to load kvm and kvm-intel modules when running. Fix is to 
# remove the directory /lib/modules/2.6.18-274.el5/extra/kmod-kvm
# -- By Ramanand (ramanandn@juniper.net) on March 22nd, 2012
# -- Updated by Ramanand on September 20th, 2012 (kernel change)
# -- Updated by Ramanand on November 17, 2012 (kernel change)
#
Fix_kmod_kvm_If_2_6_32_Kernel()
{
    KERNEL_MINOR_VER="220.23.1"

    # Get the running kernel version
    KVER=`uname -r | sed -e "s/smp//g"`
    echo KVER=${KVER}

    # If the kernel is 2.6.32 then remove the 2.6.18 modules if it exists
    if [ $KVER == "2.6.32-${KERNEL_MINOR_VER}.el6JUNIPER" ] &&
           [ -d /lib/modules/2.6.32-${KERNEL_MINOR_VER}.el6JUNIPERsmp/extra/kmod-kvm ]; then
    	rm -rf /lib/modules/2.6.18-274.el5;
	rm /lib/modules/2.6.32-${KERNEL_MINOR_VER}.el6JUNIPERsmp/extra/kmod-kvm
	depmod -a
	modprobe kvm-intel 2> /dev/null
    fi

    if [ $KVER == "2.6.32-${KERNEL_MINOR_VER}.el6JUNIPER" ] &&
           [ -f /lib/modules/2.6.32-${KERNEL_MINOR_VER}.el6JUNIPERsmp/kernel/drivers/net/ixgbe/ixgbe.ko ]; then
        rm -rf /lib/modules/2.6.32-${KERNEL_MINOR_VER}.el6JUNIPERsmp/kernel/drivers/net/ixgbe/ixgbe.ko;
        rmmod ixgbe 2> /dev/null
    fi

    # Now update the modules.dep and load ixgbe again
    depmod -a
    modprobe ixgbe

} # end of Fix_kmod_kvm_If_2_6_32_Kernel()

Larger_coredump()
{
  # Normally we want to symlink /var/opt/tms/snapshots to /coredump/snapshots.
  # But if /coredump is smaller than 40GB (because originally manufactured that
  # way with an ancient version of MFC), then symlink to /nkn/snapshots
  # instead so there will be enough space for an nvsd coredump file on
  # machines with lots of RAM.  (Terminology: snapshot == coredump).
  # We also want sysdumps to got to the same filesystem as snapshots do,
  # because of logic the Samara system has for cleaning up old snapshots
  # and sysdumps.

  # If we upgraded from a system that did not symlink the sysdumps dir,
  # then clear it out before we create the symlinks because Symlink_Dir
  # copies the contents of the directory before creating the symlink.
  # We don't want to do such copying because 1) it can take a long time,
  # and 2) it might run the filesystem out of room.
  if [ ! -L /var/opt/tms/sysdumps ] ; then
    rm -f /var/opt/tms/sysdumps/sysdump-*.tgz
  fi

  COREDUMP_SIZE_K=`df -P /coredump | grep "/coredump" | awk '{print $2}'`
  echo "/coredump size = ${COREDUMP_SIZE_K} K" >> ${LOGF}
  if [ ${COREDUMP_SIZE_K} -gt 41943040 ] ; then
    # If snapshots symlink is not already a symlink to /coredump/snapshots
    # then remove old snapshot files and symlink before doing symlink.
    ls -ld /var/opt/tms/snapshots | grep -q " -> /coredump/snapshots"
    if [ ${?} -ne 0 ] ; then
      rm -rf /coredump/snapshots 2> /dev/null
      rm -f /var/opt/tms/snapshots 2> /dev/null
    fi
    if [ -L /coredump/snapshots ] ; then
      # Just in case /coredump/snapshots is itself a symlink
      # (like to nkn/snapshots) remove it before making the symlink.
      rm -f /coredump/snapshots
    fi
    Symlink_Dir /var/opt/tms/snapshots  /coredump/snapshots
    
    # If sysdumps symlink is not already a symlink to /coredump/sysdumps
    # then remove any old sysdump files and symlink before doing symlink.
    ls -ld /var/opt/tms/sysdumps | grep -q " -> /coredump/sysdumps"
    if [ ${?} -ne 0 ] ; then
      rm -f /var/opt/tms/sysdumps/sysdump-*.tgz 2> /dev/null
      rm -f /var/opt/tms/sysdumps 2> /dev/null
    fi
    Symlink_Dir /var/opt/tms/sysdumps   /coredump/sysdumps
  else
    # If snapshots symlink is not already a symlink to /nkn/snapshots
    # then remove old snapshot files and symlink before doing symlink.
    ls -ld /var/opt/tms/snapshots | grep -q " -> /nkn/snapshots"
    if [ ${?} -ne 0 ] ; then
      rm -rf /coredump/snapshots 2> /dev/null
      rm -f /var/opt/tms/snapshots 2> /dev/null
    fi
    Symlink_Dir /var/opt/tms/snapshots  /nkn/snapshots
    Symlink_Dir /coredump/snapshots     /nkn/snapshots

    # If sysdumps symlink is not already a symlink to /nkn/sysdumps
    # then remove any old sysdump files and symlink before doing symlink.
    ls -ld /var/opt/tms/sysdumps | grep -q " -> /nkn/sysdumps"
    if [ ${?} -ne 0 ] ; then
      rm -f /var/opt/tms/sysdumps/sysdump-*.tgz 2> /dev/null
      rm -f /var/opt/tms/sysdumps 2> /dev/null
    fi
    Symlink_Dir /var/opt/tms/sysdumps   /nkn/sysdumps
  fi
}

Swap_Setup()
{
  # Not using swap on Pacifica.
  [ "${PACIFICA_HW}" = "yes" ]  && return 0

  # When there is a swap partition, then use it.
  # On systems without a swap partition then we need to enable swap to a file.

  # Search for a swap partition on the root drive.
  SWAP_DEV_REAL=
  SYS_DISK_DEV=`cat /config/nkn/system-data-disk.info`
  MINOR_NUM=`parted --script ${SYS_DISK_DEV} print | grep swap | awk '{ print $1 }'`
  if [ ! -z "${MINOR_NUM}" ] ; then
      SWAP_DEV_REAL=`cat /config/nkn/system-data-pdisk.info`${MINOR_NUM}
  fi
  SWAP_DEV_FSTAB=`cat /etc/fstab | awk '{ print $1 "," $2 "," $3 "," }' | grep ,swap,swap, | head -1 | cut -f1 -d,`
  if [ ! -z "${SWAP_DEV_REAL}" ] ; then
    rm -f /nkn/swapfile.swap
    if [ ! -z "${SWAP_DEV_FSTAB}" ] ; then
      # The swap device in /etc/fstab might not be correct.
      # On VXA model 0305 (VXA2100=VXA2000) systems, the swap device in
      # /etc/fstab needs to be checked for correctness on very reboot, because
      # the device will can change if SSD drive have been added or removed.
      if [ "${SWAP_DEV_FSTAB}" = "${SWAP_DEV_REAL}" ] ; then
        # It is correct, so return
        return
      fi
      # It is not the correct device, so update /etc/fstab and start swap.
      echo Fixing swap device in /etc/fstab >> ${LOGF}
      cp /etc/fstab /tmp/fstab.$$.tmp
      cat  /tmp/fstab.$$.tmp | sed "s%${SWAP_DEV_FSTAB}\t%${SWAP_DEV_REAL}\t%" > /etc/fstab
      diff /tmp/fstab.$$.tmp /etc/fstab >> ${LOGF}
      rm -f /tmp/fstab.$$.tmp
      swapon -a -e
      return
    fi
    echo There is a swap paritition but no swap in fstab. | tee -a ${LOGF}
    echo Start swap to swap partition | tee -a ${LOGF}
    echo "${SWAP_DEV_REAL} swap swap default 0 0" >> /etc/fstab
    swapon -a -e
    return
  fi
  # There does not seem to be a swap partition, so use a swap file
  NKN_FREE_K=`df -P /nkn/. | grep "/nkn" | awk '{print $4}'`
  if [ -z "${NKN_FREE_K}" ] ; then
    echo Error getting /nkn free space size | tee -a ${LOGF}
    echo Not using swap | tee -a ${LOGF}
    return
  fi
  echo "/nkn free size = ${NKN_FREE_K} K" >> ${LOGF}
  # Make sure there is enough space on the filesystem for a swap file.
  # We do not want to have it use up space when /nkn is low on free space
  # because we want to leave some head room.
  # So remove swap file if less than 5G free.
  # When the swap file will be 9G:
  #  - Do not create swap file when less than 16G free.
  # When the swap file will be 1G:
  #  - Do not create swap file when less than 8G free.

  # 5G is 5242880K ( 5 * 1024 * 1024)
  LEVEL1_K=5242880
  if [ -f /config/nkn/vm_cloudvm ] ; then
    # In a VM, so use minimal size swap file.
    SIZE_G=`grep ^SWAP_SIZE_G= /config/nkn/vm_cloudvm | cut -f2 -d=`
    SIZE_G=`expr "${SIZE_G}" + 0 2> /dev/null`
    # When not specified use 1G size
    [ -z ${SIZE_G} ] && SIZE_G=1
    LEVEL2_K=`grep ^SWAP_MIN_NKN_FREE_K= /config/nkn/vm_cloudvm | cut -f2 -d=`
    LEVEL2_K=`expr "${LEVEL2_K}" + 0 2> /dev/null`
    # When not specified use 8G minimum free space.  8G is 8388608K ( 8 * 1024 * 1024)
    [ -z ${LEVEL2_K} ] && LEVEL2_K=8388608
  else
    # Use a normal sized swap file.
    SIZE_G=9
    # 16G is 16777216K (16 * 1024 * 1024)
    LEVEL2_K=16777216
  fi

  if [ -f /nkn/swapfile.swap -a ${NKN_FREE_K} -lt ${LEVEL1_K} ] ; then
    echo Low free space on /nkn, ${NKN_FREE_K} | tee -a ${LOGF}
    echo Removing swap file | tee -a ${LOGF}
    swapoff /nkn/swapfile.swap 2> /dev/null
    rm -f   /nkn/swapfile.swap
    echo Not using swap | tee -a ${LOGF}
    return
  fi

  if [ ! -f /nkn/swapfile.swap ] ; then
    if [ ${NKN_FREE_K} -lt ${LEVEL2_K} ] ; then
      echo Low free space on /nkn, ${NKN_FREE_K} | tee -a ${LOGF}
      echo Not using swap | tee -a ${LOGF}
      return
    fi
    echo Creating swapfile ${SIZE_G} G   | tee -a ${LOGF}
    echo This could take several minutes | tee -a ${LOGF}
    date >> ${LOGF}
    echo dd if=/dev/zero of=/nkn/swapfile.swap bs=1G count=${SIZE_G} >> ${LOGF}
    dd if=/dev/zero of=/nkn/swapfile.swap bs=1G count=${SIZE_G} >> ${LOGF} 2>&1
    date >> ${LOGF}
    echo .
    echo mkswap -f /nkn/swapfile.swap >> ${LOGF}
    mkswap -f /nkn/swapfile.swap | tee -a ${LOGF}
  fi
  date  >> ${LOGF}
  echo Enabling swap file   | tee -a ${LOGF}
  swapon /nkn/swapfile.swap | tee -a ${LOGF}
  date  >> ${LOGF}
}

Log_Export_Sizing()
{
  # The file /config/nkn/log-export-size specifies the maximum /log
  # filesystem disk space the Log Export functionality is to use.
  # Use 50% of the /log filesystem up to 40GB maximum.
  # Create the file if it is not already made.
  [ -s /config/nkn/log-export-size ]  && return

  LOG_SIZE_K=`df -P /log | grep "/log" | awk '{print $2}'`
  # Use half the space and round up to to multiple of 1024.
  USE_SIZE_K=$(( ( (LOG_SIZE_K / 2) + 1023) / 1024 * 1024 ))
  # Max size is 40G which is 41943040K (40 * 1024 * 1024)
  MAX_SIZE_K=41943040
  [ ${USE_SIZE_K} -gt ${MAX_SIZE_K} ] && USE_SIZE_K=${MAX_SIZE_K}
  echo ${USE_SIZE_K} > /config/nkn/log-export-size
}

update_initrd_with_mdadm()
{
    mount -o remount,rw /boot
    cd /boot
    mv initrd.img initrd.img.gz
    gunzip initrd.img.gz
    # Create a temp directory to extract and update the initrd
    rm -rf  initrd
    mkdir   initrd
    cd initrd;
    cpio -i --make-directories < ../initrd.img;
    cp /etc/mdadm/mdadm.conf etc/mdadm/

    # Recreate the initrd
    find . | cpio --create --format='newc' > ../initrd.img;
    cd ..
    gzip initrd.img;
    mv initrd.img.gz initrd.img;

    # Remove the initrd directory
    rm -rf initrd;
    mount -o remount,ro /boot
}

Create_Mdadmconf()
{
  # If Mirroring is used, we need to create/update the mdadm.conf file.
  grep -q "/dev/md" /config/nkn/boot-disk.info  > /dev/null 2>&1
  if [ ${?} -eq 0 ]; then
    Make_Sure_Directory_Exists /etc/mdadm
    /sbin/mdadm -Es > /etc/mdadm/mdadm.conf
    update_initrd_with_mdadm
  fi
}

Set_Mfdb_jnprinfo()
{
  # The system/jnprinfo node is one that we create here for
  # display in the CLI "show version" command (via changes in
  # Samara tree/src/bin/cli/modules/cli_diag_cmds.c, and
  # in the GUI "Summary" page via our source file
  # nokeena/src/bin/web/templates/mon-summary-common.tem
  # Set this node if not already set.

  MFG_DB_PATH=/config/mfg/mfdb
  MDDBREQ=/opt/tms/bin/mddbreq
  ITEM=system/jnprinfo
  N=`${MDDBREQ} -v ${MFG_DB_PATH} query get - /mfg/mfdb/${ITEM}`
  if [ ! -z "${N}" ] ; then
    echo "Hardware: ${N}"
    return
  fi
  # Get strings from dmidecode system-manufacturer and system-product
 
  if [ "${SMFG}${SPNAME}" = "NANA" ] ; then
    N=NA
  elif [ "${SMFG}" = "NA" ] ; then
    N="${SPNAME}"
  elif [ "${SPNAME}" = "NA" ] ; then
    N="${SMFG}"
  else
    N="${SMFG} ${SPNAME}"
  fi
  if [ "${PACIFICA_HW}" = "yes" ] ; then
    [ "${SMFG}"   = "NA" ] && SMFG="Juniper"
    [ "${SPNAME}" = "NA" ] && SPNAME="AS-MLC"
    N="${SMFG} ${SPNAME}"
  fi
  ${MDDBREQ} -c ${MFG_DB_PATH} set modify "" /mfg/mfdb/${ITEM} string "${N}"
  echo "Hardware: ${N}"
}

#------------------------------------------------------------------------------
#
# This function does fixes related to VJX
#
VJX_Fix()
{
	# Create a version file in /config/db
	# write current release to mfc-version.txt. This file is fetched by VJX/mfcd
	# at start-up, so that VJX may advertise this version as its own version.
	# At MFC, there is sym-link to this file in /opt/tms/lib/web/contents/
	# XXX - commenting out following code for now, as this opens-up another servere issue
	#grep BUILD_PROD_RELEASE /etc/build_version.txt | cut -f2 -d- > /config/nkn/mfc-version.txt;
	grep BUILD_PROD_RELEASE /etc/build_version.txt | cut -f2 -d':' | sed -e 's/^ *//g' > /config/nkn/mfc-version.txt;

	# Now symlink this to the httpd documents directory
	if [ ! -s /opt/tms/lib/web/content/mfc-version.txt ]; then
	    ln -s /config/nkn/mfc-version.txt /opt/tms/lib/web/content/
	fi

} # end of VJX_Fix

Console_Change_Check()
{
   [ ! -f /config/nkn/console_set ] && return
   VAL=`head /config/nkn/console_set | grep -v '#' | cut -f1 -d' '`
   echo "Console Set to ${VAL}"
   case "_${VAL}" in
   _serial) /opt/nkn/bin/update_grub_console.sh serial 2>&1 | tee -a /log/install/grub-update.log ;;
   _vga)    /opt/nkn/bin/update_grub_console.sh vga    2>&1 | tee -a /log/install/grub-update.log  ;;
   esac
   mv /config/nkn/console_set /config/nkn/console_set.done
}

#--------------------------------------------------------------
#
#  MAIN LOGIC BEGINS HERE
#
#--------------------------------------------------------------

Make_Sure_Directory_Exists /log/init

HW_Check
# Let us setup Pacifica if we are running on it
Setup_Pacifica

Root_RW
# Note: In MFC-12.1.0 and later, first time install create the files
# system-data-disk.info and boot-disk.info in /config/nkn/.
# In earlier MFC versions, this info was put differently named files.
# Deal with filename change from versions before MFC12.
if [ ! -f /config/nkn/system-data-disk.info ] ; then
    # This code here is only run once after upgrade from pre-MFC12 to
    # create system-data-*disk.info and boot-*disk.info
    # These cp commands here create the files as needed for machines that
    # do not use eUSB as a boot drive.  For machines that use eUSB,
    # the files will be fixed up later (see notes below).
    cp /config/nkn/root-disk.info /config/nkn/system-data-disk.info
    cp /config/nkn/root-disk.info /config/nkn/boot-disk.info
    cp /config/nkn/root-pdisk.info /config/nkn/system-data-pdisk.info
    cp /config/nkn/root-pdisk.info /config/nkn/boot-pdisk.info
    # Note 1: On VXA model 0305 (VXA2100=VXA2000), the function VXA_Check
    # called below updates the info in /config/nkn/system-data-*disk.info
    # on every reboot.
    # Note 2: On VXA the function "Create_eUSB_File" called below updates the
    # info in /config/nkn/boot-*disk.info on every reboot.
    # Note 3: On VXA other than model 0305 the function "Create_eUSB_File"
    # on first boot after upgrade from 12.1.0 which was an upgrade from
    # 11.x or earlier, will also fix up /config/nkn/system-data-*disk.info
fi

Update_HW_Info
Create_cciss_dev
Create_eUSB_File
Cleanup_Labels_reboot
Determine_Raid_Drives
Id_Non_Cache_Drives
Swap_Setup

Larger_coredump
# following directory is need for MFA-2.2
Symlink_Dir /opt/tms/lib/web/content/smap /nkn/smap
Symlink_Dir /var/opt/tms/stats  /nkn/stats
Symlink_Dir /var/log  /log/varlog
Symlink_Dir /var/nkn/dashboard  /nkn/dashboard
Symlink_Dir /var/home /nkn/ftphome
Symlink_Dir /var/root /nkn/varroot
Symlink_Dir /var/tmp  /nkn/vartmp
Symlink_Dir /var/opt/tms/virt  /nkn/virt
Symlink_Dir /var/opt/tms/images /nkn/varopttmsimages
Symlink_Dir /usr/local/etc /config/usrlocaletc
Symlink_Dir /etc/libvirt /var/libvirt
Make_Sure_Directory_Exists /log/varlog/nkn
Make_Sure_Directory_Exists /log/varlog/nkncnt
Make_Sure_Directory_Exists /log/varlog/nkn/cache
Make_Sure_Directory_Exists /log/varlog/nkn/startup
Make_Sure_Directory_Exists /nkn/mnt
Make_Sure_Directory_Exists /nkn/smap
Make_Sure_Directory_Exists /nkn/mnt/fuse
Make_Sure_Directory_Exists /nkn/mnt/virt_cache
Make_Sure_Directory_Exists /nkn/mnt/virt_cache/virt_cache
Make_Sure_Directory_Exists /nkn/tmp
Make_Sure_Directory_Exists /nkn/tmp/cluster_hash
Make_Sure_Directory_Exists /config/mnts
Make_Sure_Directory_Exists /nkn/vpe
Make_Sure_Directory_Exists /nkn/vpe/file 777
Make_Sure_Directory_Exists /nkn/vpe/live 777
Make_Sure_Directory_Exists /nkn/policy_engine
Make_Sure_Directory_Exists /nkn/prefetch
Make_Sure_Directory_Exists /nkn/prefetch/jobs
Make_Sure_Directory_Exists /nkn/prefetch/lockfile
Make_Sure_Directory_Exists /nkn/ftphome/_ftpuser
Make_Sure_Directory_Exists /nkn/maxmind
Make_Sure_Directory_Exists /nkn/maxmind/db
Make_Sure_Directory_Exists /nkn/maxmind/downloads
Make_Sure_Directory_Exists /nkn/ucflt

if [ ! -p /var/log/local4.pipe ] ; then
  mkfifo /var/log/local4.pipe
  if [ $? -ne 0 ]; then
    logger -t fixup "Failed creating local4.pipe"
  fi
fi
#
# CMM node status related file 
Make_Sure_Directory_Exists /vtmp/www
echo "" > /vtmp/www/cmm-node-status.html
echo "bw:000,nvsd:0;" >> /vtmp/www/cmm-node-status.html
if [ ! -L /opt/tms/lib/web/content/cmm-node-status.html ]; then
    ln -s /vtmp/www/cmm-node-status.html /opt/tms/lib/web/content/cmm-node-status.html
fi
chmod 4755 /opt/tms/lib/web/cgi-bin/td
if [ ! -L /opt/tms/lib/web/content/id_dsa.pub ]; then
    ln -s /config/nkn/id_dsa.pub /opt/tms/lib/web/content/id_dsa.pub
fi
#SSL cert Dir
Make_Sure_Directory_Exists /config/nkn/cert 0700
Make_Sure_Directory_Exists /config/nkn/key 0700
Make_Sure_Directory_Exists /config/nkn/csr 0700
Make_Sure_Directory_Exists /config/nkn/ca 0700

Symlink_File /lib/nkn/libmediacodecs_raw_udp.so.1 /lib/nkn/libraw_udp.so.1.0.1
Symlink_File /lib/nkn/libmediacodecs_ankeena_fmt.so.1 /lib/nkn/libankeena_fmt.so.1.0.1
Symlink_File /lib/nkn/libmediacodecs_mp4.so.1 /lib/nkn/libmp4.so.1.0.1
Symlink_File /lib/nkn/libmediacodecs_mpeg2ts_h221.so.1 /lib/nkn/libmpeg2ts_h221.so.1.0.1
Symlink_File /lib/nkn/libmediacodecs_sample.so.1 /lib/nkn/libsample.so.1.0.1

Symlink_Dir /opt/compaq/cpqacuxe /var/compaq/cpqacuxe
Fix_StorMan
Update_ImageLayout_File
Disable_ip_conntrack

Fix_kmod_kvm_If_2_6_32_Kernel
Create_Mdadmconf
Set_Mfdb_jnprinfo

# VJX related fixes
VJX_Fix

# Pacifica Resiliency Related changes
Setup_Pacifica_Resiliency

Root_Restore

Create_Cron_Link
Remove_Cron_Link

Cleanup_lockfiles

Load_myri10ge_ko

/opt/nkn/startup/dmi_adapter_install.sh
/opt/nkn/startup/cpuperf.sh
/opt/dpi-analyzer/defaultconfig/installconf.sh 

Link_Kdump_image
# reserve space for logs
Log_Export_Sizing

# Create a DIR for object listing in nkn partition
Make_Sure_Directory_Exists /nkn/ns_objects 0700

# Change console serial->vga or vga->serial on next boot when indicated.
Console_Change_Check

echo End fixups.sh `date`  >> ${LOGF}
exit 0
