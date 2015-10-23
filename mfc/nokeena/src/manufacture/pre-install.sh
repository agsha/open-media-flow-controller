#! /bin/bash
# Do the pre-install steps.

echo "pre-install ${*}" >> /tmp/pre-install.log
if [ "_${1}" = "_--automatic" ] ; then
  # Do no prompts, take the defaults.
  AUTO_INST=auto
  echo "automatic" >> /tmp/pre-install.log
  # The second param can be 1 thru f, or "10".
  # When 1 thru f, it is automatic VM installation.
  # When 10, then normal automatic install.
  AUTO_MODE=${2}
  [ -z "${AUTO_MODE}" ] && AUTO_MODE=1
else
  AUTO_INST=no
fi

if [ -f /tmp/step-post-inst.txt ] ; then
  if [ "${AUTO_INST}" = "auto" ] ; then
    # This should not happen.
    exit 1
  fi
  # The post install script has already been run.
  # If we broke out of that, we want to run it again
  # because it has the prompts for rebooting, halting
  # and setting up the ethernet naming.
  bash /sbin/post-install.sh
  RV=${?}
  if [ ${RV} -gt 0 -a ${RV} -lt 9 ] ; then
     echo Installation failure
     sleep 3
  fi
  exit ${RV}
fi

EXITING=no
STOP_FILE=/tmp/stop.$$.tmp
trap_int_handler()
{
  [ "${EXITING}" = "yes" ] && exit 8
  echo
  echo "(Interrupt)"
  echo
  touch ${STOP_FILE}
}
trap "trap_int_handler" INT

# Dot in the functions for VXA checking
# VXA_Check()
# EUSB_Check()
# VXA_Hardware_Check()
. /sbin/vxa_functions.dot

Exit_Logout()
{
  EXITING=yes
  rm -f ${STOP_FILE}
  echo ..
  echo "Login as root to install MFC"
  sleep 2
  # This script is run by root.profile, which does the following
  # with the return code:
  # 0-8 prints out "Error, Install returned <number>." and then goes
  # to the login prompt.
  # 9 just goes to the login prompt without printing anything.
  # 10 and higher makes it go to the shell.
  exit 9
}

Reboot()
{
  rm -f ${STOP_FILE}
  trap - INT
  echo "Rebooting ..."
  # Suppress all kernel messages to the console
  killall klogd
  # Kill syslogd because it can get a segfault when system shutting down.
  killall syslogd
  sleep 2
  reboot
  # Exiting with 11 makes root.profile go to the shell prompt.
  exit 11
}

Halt()
{
  rm -f ${STOP_FILE}
  trap - INT
  echo "Halting the system ..."
  # Suppress all kernel messages to the console
  killall klogd
  # Kill syslogd because it can get a segfault when system shutting down.
  killall syslogd
  sleep 2
  halt
  # Exiting with 11 makes root.profile go to the shell prompt.
  exit 11
}

Return_To_Continue()
{
  [ "${AUTO_INST}" = "auto" ] && return
  echo -n Press return to continue
  ANS=
  read ANS
}

EULA_Step()
{
  EULA_FILE=/etc/eula.txt
  if [ "${AUTO_INST}" = "auto" ] ; then
    echo accepted-eula > /tmp/eula_response.txt
    return
  fi
  READ_FILE=no
  READ_COUNT=0
  if [ ! -f /tmp/eula_response.txt ] ; then
    while true ; do
      # Just in case the stdin somehow is spewing, limit the number of loops.
      READ_COUNT=$(( ${READ_COUNT} + 1 ))
      [ ${READ_COUNT} -gt 200 ] && Exit_Logout
      echo
      if [ "${READ_FILE}" != "yes" ] ; then
        echo "Enter 1 to Read the EULA agreement and continue with installation."
      else
        echo "Enter 1 to Read the EULA agreement again."
        echo "Enter 'accept' to accept the EULA agreement and continue with installation."
      fi
      echo Enter 8 to Reboot without installing or restoring.
      echo Enter 9 to Halt without installing or restoring.
      #echo Enter 0 to Exit without installing or restoring.
      echo
      if [ "${READ_FILE}" != "yes" ] ; then
        echo -n "Enter 1, 2, or 3 : "
      else
        echo -n "Enter 1, 2, 3, or 'accept' : "
      fi
      ANS=
      read ANS
      [ -z "${ANS}" ] && continue
      case "${ANS}" in
      0|exit|quit|q)
        # Exiting with 10 makes root.profile go to the shell prompt.
        # It also prints a message before that about using exit and reboot.
        exit 10
        ;;
      1|read)
        cat ${EULA_FILE} | more
        READ_FILE=yes
        continue
        ;;
      8|reboot)  Reboot ;;
      9|halt)    Halt ;;
      accept|accept-eula)
        [ "${READ_FILE}" != "yes" -a "${ANS}" = "accept" ] && continue
        echo accepted-eula > /tmp/eula_response.txt
        break
        ;;
      esac
    done
  fi
  echo
  echo
  if [ ! -f /tmp/eula_response.txt ] ; then
    echo You need to agree to the EULA before installing.
    echo Start over.
    Exit_Logout
  fi
}

VM_Check()
{
  echo "VM_Check()" >> /tmp/pre-install.log
  if [ "${IS_VXA}" = "yes" ] ; then
    IS_VM=no
    IS_KVM=no
    IS_VMWARE=no
    rm -f /tmp/vm_info.txt
    return
  fi
  if [ "_${IS_VM}" = "_yes" ] ; then
    [ -f /tmp/vm_info.txt ] && return
  fi

  IS_VM=no
  IS_KVM=no
  IS_VMWARE=no
  if [ "_${SPRD}" = "_KVM" ] ; then
    IS_VM=yes
    IS_KVM=yes
    echo IS_VM=${IS_VM}         >  /tmp/vm_info.txt
    echo IS_KVM=${IS_KVM}       >> /tmp/vm_info.txt
    echo IS_VMWARE=${IS_VMWARE} >> /tmp/vm_info.txt
  else
    # Test for VMware
    case ${SMFG} in
    VMware*) IS_VMWARE=yes ;;
    esac
    case ${SPRD} in
    VMware*) IS_VMWARE=yes ;;
    esac
    case ${SSER} in
    VMware*) IS_VMWARE=yes ;;
    esac
    if [ "${IS_VMWARE}" = "yes" ] ; then
      IS_VM=yes
      echo IS_VM=${IS_VM}         >  /tmp/vm_info.txt
      echo IS_KVM=${IS_KVM}       >> /tmp/vm_info.txt
      echo IS_VMWARE=${IS_VMWARE} >> /tmp/vm_info.txt
    fi
  fi
}

Find_Disks()
{
  echo "Find_Disks()" >> /tmp/pre-install.log
  # First make all the device nodes for all the disks we can find.
  # Filter out the device that has the cdrom or usb drive mounted.
  # Save a list of all the device names found or created, to use
  # in the next step.
  rm -f /tmp/disks-list.txt
  rm -f /tmp/disks-found*
  SKIP1=`df /mnt/cdrom 2> /dev/null | grep /mnt/cdrom | cut -f1 -d' ' | cut -f3 -d/`
  SKIP2=`df / | grep '/$' | cut -f1 -d' ' | cut -f3 -d/`
  echo "${SKIP1},${SKIP2}" > /tmp/disks-skip.txt
  [ -z "${SKIP1}" ] && SKIP1=zzzz
  [ -z "${SKIP2}" ] && SKIP2=zzzz
  COUNT=0
  cat /proc/diskstats | grep -v " ram" | grep -v " loop" | grep -v " md" \
     | while read LINE
  do
    [ -z "${LINE}" ] && return
    MAJOR=`echo $LINE | cut -f1 -d' '`
    MINOR=`echo $LINE | cut -f2 -d' '`
    DNAME=`echo $LINE | cut -f3 -d' '`
    VAL1=`echo $LINE | cut -f4 -d' '`
    # If the first value is 0 then there is no device connected.
    if [ "${DNAME}" = "${SKIP1}" -o "${DNAME}" = "${SKIP2}" -o ${VAL1} -eq 0 ] ; then 
      echo ${MAJOR},${MINOR},${DNAME},${VAL1} >> /tmp/disks-skip.txt
      continue
    fi
    # The minor number of the device for the whole drive is a multiple of 16.
    case ${MINOR} in
    0|16|32|48|64|80|96|112|128|144|160|176|192|208|224|240)
      COUNT=$(( ${COUNT} + 1 ))
      if [ ! -b /dev/${DNAME} ] ; then
        # Need to create the block device node
        D=`dirname /dev/${DNAME}`
        [ ! -d ${D} ] && mkdir -p ${D}
        mknod /dev/${DNAME} b ${MAJOR} ${MINOR}
        if [ ${?} -ne 0 ] ; then
          echo Error, failed to create device node /dev/${DNAME}
          echo No more inodes?
        fi
      fi
      echo ${COUNT},${DNAME}, >> /tmp/disks-list.txt
    esac
  done

  if [ "${IS_2100_VXA}" = "yes" ] ; then
    # We need to do special processing on the 2100 to get the slot ordering.
    # We had to make all the device nodes above before doing this step.
    bash /sbin/detect_hdd.sh -a -q > /tmp/slot_to_dev.txt
  fi
  [ ! -s /tmp/slot_to_dev.txt ] && rm -f /tmp/slot_to_dev.txt

  cat /tmp/disks-list.txt | while read LINE
  do
      COUNT=`echo ${LINE} | cut -f1 -d,`
      DNAME=`echo ${LINE} | cut -f2 -d,`
      DEV_SIZE_K=`sfdisk -q -s /dev/${DNAME}`
      if [ -z "${DEV_SIZE_K}" ] ; then
        DEV_SIZE_M=unknown
        DEV_SIZE_G=unknown
      else
        # Use awk because busybox expr does not work above 2^32
        DEV_SIZE_M=`echo ${DEV_SIZE_K} | awk '{a= $1 + 999; a= a / 1000; print int(a)}'`
        DEV_SIZE_G=`echo ${DEV_SIZE_M} | awk '{a= $1 + 999; a= a / 1024; print int(a)}'`
      fi
      if [ ${COUNT} -lt 10 ] ; then
        SLOT="${COUNT}: "
      else
        SLOT=${COUNT}:
      fi
      DT=
      if [ "${EUSB_DEV_NAME}" = "/dev/${DNAME}" ] ; then
        SLOT=eUSB:
      elif [ -f /tmp/slot_to_dev.txt ] ; then
        # First clear all the varibles that will be set.
        eval `grep 'D_CONTROLLER=;' /tmp/slot_to_dev.txt`
        # Now find and set the D_* variables from the found line.
        eval `grep "D_DEVNAME=/dev/${DNAME};" /tmp/slot_to_dev.txt`
        if [ -z "${D_SLOT}" ] ; then
          SLOT=%:
        else
          if [ ${D_SLOT} -lt 10 ] ; then
            SLOT="${D_SLOT}: "
          else
            SLOT=${D_SLOT}:
          fi
          DT="${D_DRV_TYPE} ${D_MFG} ${D_MODEL_NUM} serial:${D_LSI_SER}"
        fi
      elif [ "${IS_VM}" = "no" ] ; then
        # Get the model and serial from smartctl
        smartctl -i /dev/${DNAME} > /tmp/devinfo.${DNAME}.txt 2> /dev/null
        DM=`grep -i '^Device Model:' /tmp/devinfo.${DNAME}.txt | cut -c14-`
        [ -z "${DM}" ] && \
          DM=`grep -i '^Product:' /tmp/devinfo.${DNAME}.txt | cut -c9-`
        [ -z "${DM}" ] && \
          DM=`grep -i '^Vendor:' /tmp/devinfo.${DNAME}.txt | cut -c9-`
        DM=`echo ${DM}`
        SN=`grep -i '^Serial Number:' /tmp/devinfo.${DNAME}.txt | cut -c15-`
        [ -z "${SN}" ] && \
          SN=`grep -i '^Serial' /tmp/devinfo.${DNAME}.txt | cut -c7-`
        SN=`echo ${SN}`
        if [ -z "${SN}" ] ; then
          DT="${DM}"
        else
          DT="${DM} serial: ${SN}"
        fi
      fi
      if [ "${SLOT}" = "eUSB:" ] ; then
        echo "eUSB: ${DNAME} ${DEV_SIZE_G} GB"          >  /tmp/disks-foundeusb.txt
      elif [ -f /tmp/slot_to_dev.txt ] ; then
        echo "${SLOT} ${DNAME} ${DEV_SIZE_G} GB ${DT}" >> /tmp/disks-found${D_CONTROLLER}.txt
      else
        echo "${SLOT} ${DNAME} ${DEV_SIZE_G} GB ${DT}" >> /tmp/disks-found.log
      fi
  done
  if [ -f /tmp/slot_to_dev.txt ] ; then
    if [ -f /tmp/disks-found1.txt ] ; then
      echo Enclosure 1 >> /tmp/disks-found.log
    fi
    sort -n /tmp/disks-found0.txt >> /tmp/disks-found.log
    if [ -f /tmp/disks-found1.txt ] ; then
      echo Enclosure 2 >> /tmp/disks-found.log
      sort -n /tmp/disks-found1.txt >> /tmp/disks-found.log
    fi
  fi
  [ -f /tmp/disks-foundeusb.txt ] && cat /tmp/disks-foundeusb.txt >> /tmp/disks-found.log
   
  # Now see which type of HDD controllers there are.
  # Filter out the device that has the cdrom or external usb drive mounted.
  SKIP1=`df /mnt/cdrom | grep /mnt/cdrom | cut -f1 -d' ' | cut -f3 -d/`
  SKIP2=`df / | grep '/$' | cut -f1 -d' ' | cut -f3 -d/`
  [ -z "${SKIP1}" ] && SKIP1=zzzzzzzzzzzzzzzzzzzz
  [ -z "${SKIP2}" ] && SKIP2=zzzzzzzzzzzzzzzzzzzz
  FOUND_SDA=0
  FOUND_SDA=`grep " sda " /proc/diskstats | grep -v " ${SKIP1} " | grep -v " ${SKIP2} " | grep -v " sda 0 " | wc -l`
  FOUND_SDA=`echo ${FOUND_SDA}`
  FOUND_HDA=0
  FOUND_HDA=`grep " hda " /proc/diskstats | grep -v " ${SKIP1} " | grep -v " ${SKIP2} " | grep -v " hda 0 " | wc -l`
  FOUND_HDA=`echo ${FOUND_HDA}`
  FOUND_CCISS=0
  FOUND_CCISS=`grep " cciss/c0d0 " /proc/diskstats | grep -v " cciss/c0d0 0 " | wc -l`
  FOUND_CCISS=`echo ${FOUND_CCISS}`
  FOUND_VDA=0
  FOUND_VDA=`grep " vda " /proc/diskstats | grep -v " vda 0 " | wc -l`
  FOUND_VDA=`echo ${FOUND_VDA}`

  # Now determine the default root drive to use.  (Not the eUSB boot drive)
  # Set ROOT_DISK_DEV to the device name for the whole drive.
  # Set ROOT_DISK_PDEV to the device name that use to create partition device
  # names by appending a partition number.  For some devices this is the same
  # as the ROOT_DISK_DEV name.
  FIRST_SLOT_DEV=
  SECOND_SLOT_DEV=na
  if [ -f /tmp/slot_to_dev.txt ] ; then
    E_NUMSLOTS=
    E_STARTSLOT=
    # Now find and set the E_* variables from the found line.
    eval `cat /tmp/slot_to_dev.txt | grep 'E_ENCLOSURE_NUM=1;' | head -1`
    if [ -z "${E_NUMSLOTS}" ] ; then
      # This should never happen, but just in case use something.
      E_NUMSLOTS=40
      E_STARTSLOT=0
    fi
    THIS_SLOT=$(( ${E_STARTSLOT} - 1 ))
    CNT_SLOTS=1
    # Find the first and second slot with a non-SSD drive.
    while [ ${CNT_SLOTS} -le ${E_NUMSLOTS} ] 
    do
      (( THIS_SLOT++ ))
      (( CNT_SLOTS++ ))
      # First clear all the varibles that will be set.
      eval `grep 'D_CONTROLLER=;' /tmp/slot_to_dev.txt`
      # Now find and set the D_* variables from the found line.
      eval `cat /tmp/slot_to_dev.txt | grep 'D_CONTROLLER=0;' | grep "D_SLOT=${THIS_SLOT};"`
      # Do not use a SSD.
      [ "${D_DRV_TYPE}" = "SAS_SSD" ] && continue 
      [ "${D_DRV_TYPE}" = "SATA_SSD" ] && continue 
      if [ -z "${FIRST_SLOT_DEV}" ] ; then
        FIRST_SLOT_DEV=${D_DEVNAME}
      else
        SECOND_SLOT_DEV=${D_DEVNAME}
        # Break out of the while loop.
        break
      fi
    done
  fi
  DEFAULT_ROOT_DISK_DEV=unknown
  DEFAULT_ROOT_DISK_PDEV=unknown
  DEFAULT_HDD2_DISK_DEV=na
  DEFAULT_HDD2_DISK_PDEV=na
  if [ ! -z "${FIRST_SLOT_DEV}" ] ; then
    DEFAULT_ROOT_DISK_DEV=${FIRST_SLOT_DEV}
    DEFAULT_ROOT_DISK_PDEV=${DEFAULT_ROOT_DISK_DEV}
    DEFAULT_HDD2_DISK_DEV=${SECOND_SLOT_DEV}
    DEFAULT_HDD2_DISK_PDEV=${DEFAULT_HDD2_DISK_DEV}
  elif [ ${FOUND_SDA} -ne 0 -a ${FOUND_HDA} -eq 0 -a ${FOUND_CCISS} -eq 0 -a ${FOUND_VDA} -eq 0 ] ; then
    DEFAULT_ROOT_DISK_DEV=/dev/sda
    DEFAULT_ROOT_DISK_PDEV=${DEFAULT_ROOT_DISK_DEV}
    grep -q " sdb " /proc/diskstats
    if [ ${?} -eq 0 ] ; then
      DEFAULT_HDD2_DISK_DEV=/dev/sdb
      DEFAULT_HDD2_DISK_PDEV=${DEFAULT_HDD2_DISK_DEV}
    fi
  elif [ ${FOUND_SDA} -eq 0 -a ${FOUND_HDA} -ne 0 -a ${FOUND_CCISS} -eq 0 -a ${FOUND_VDA} -eq 0 ] ; then
    DEFAULT_ROOT_DISK_DEV=/dev/hda
    DEFAULT_ROOT_DISK_PDEV=${DEFAULT_ROOT_DISK_DEV}
    grep -q " hdb " /proc/diskstats
    if [ ${?} -eq 0 ] ; then
      DEFAULT_HDD2_DISK_DEV=/dev/hdb
      DEFAULT_HDD2_DISK_PDEV=${DEFAULT_HDD2_DISK_DEV}
    fi
  elif [ ${FOUND_SDA} -eq 0 -a ${FOUND_HDA} -eq 0 -a ${FOUND_CCISS} -ne 0 -a ${FOUND_VDA} -eq 0 ] ; then
    DEFAULT_ROOT_DISK_DEV=/dev/cciss/c0d0
    DEFAULT_ROOT_DISK_PDEV=${DEFAULT_ROOT_DISK_DEV}p
    grep -q " cciss/c0d1 " /proc/diskstats
    if [ ${?} -eq 0 ] ; then
      DEFAULT_HDD2_DISK_DEV=/dev/cciss/c0d1
      DEFAULT_HDD2_DISK_PDEV=${DEFAULT_HDD2_DISK_DEV}p
    fi
  elif [ ${FOUND_SDA} -eq 0 -a ${FOUND_HDA} -eq 0 -a ${FOUND_CCISS} -eq 0 -a ${FOUND_VDA} -ne 0 ] ; then
    DEFAULT_ROOT_DISK_DEV=/dev/vda
    DEFAULT_ROOT_DISK_PDEV=${DEFAULT_ROOT_DISK_DEV}
    grep -q " vdb " /proc/diskstats
    if [ ${?} -eq 0 ] ; then
      DEFAULT_HDD2_DISK_DEV=/dev/vdb
      DEFAULT_HDD2_DISK_PDEV=${DEFAULT_HDD2_DISK_DEV}
    fi
  fi
}

Hardware_Snoop()
{
  echo "Hardware_Snoop()" >> /tmp/pre-install.log
  /sbin/dmidecode-info.sh > /tmp/dmidecode-info.txt
  export SMFG=`dmidecode -s system-manufacturer`
  export SPRD=`dmidecode -s system-product-name`
  export SVER=`dmidecode -s system-version`
  export SSER=`dmidecode -s system-serial-number`
  export SUUID=`dmidecode -s system-uuid`
  echo "SMFG=${SMFG}"   >  /tmp/systeminfo.txt
  echo "SPRD=${SPRD}"   >  /tmp/systeminfo.txt
  echo "SVER=${SVER}"   >> /tmp/systeminfo.txt
  echo "SSER=${SSER}"   >> /tmp/systeminfo.txt
  echo "SUUID=${SUUID}" >> /tmp/systeminfo.txt
  /sbin/eth-info.sh       > /tmp/eth-info.txt
  TMP_KB=`grep ^MemTotal: /proc/meminfo | cut -f2 -d:`
  RAM_KB=`echo ${TMP_KB} | cut -f1 -d' '`
  TMP_KB=`expr ${RAM_KB} + 1023`
  RAM_MB=`expr ${TMP_KB} / 1024`
  TMP_MB=`expr ${RAM_MB} + 1023`
  RAM_GB=`expr ${TMP_MB} / 1024`
  echo RAM_KB=${RAM_KB} >  /tmp/ram-size.txt
  echo RAM_MB=${RAM_MB} >> /tmp/ram-size.txt
  echo RAM_GB=${RAM_GB} >> /tmp/ram-size.txt
}

KVM_vda_Setup()
{
  [ "${IS_KVM}" = "no" ] && return
  echo "KVM_vda_Setup()" >> /tmp/pre-install.log
  # KVM uses /dev/vda and one some systems needs to be major 253
  # and on other systems nees to be 254.
  # So get the needed major number from /proc/diskstats
  FOUND_VDA=`grep " vda " /proc/diskstats | grep -v " vda 0 " | wc -l`
  VDA_MAJOR=253
  if [ ${FOUND_VDA} -eq 1 ] ; then
    VDA_MAJOR=`grep " vda " /proc/diskstats | awk '{print $1}'`
  fi   
  rm -f /dev/1:0:0:0
  rm -f /dev/vda*
  mknod /dev/vda b ${VDA_MAJOR} 0
  for i in 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
  do
    mknod /dev/vda$i b ${VDA_MAJOR} $i
  done
}

VM_Choice_Auto()
{
  [ "${IS_VM}" = "no" ] && return
  echo "VM_Choice_Auto()"       >> /tmp/pre-install.log
  date                          >> /tmp/pre-install.log
  echo "AUTO_INST=${AUTO_INST}" >> /tmp/pre-install.log
  if [ "${AUTO_INST}" = "auto" ] ; then
    [ -z "${CACHE_CMDLINE}" ] && CACHE_CMDLINE=init-cache
    case ${AUTO_MODE} in
    1|2|3|4|5|6|7)
      DISK_LAYOUT_CMDLINE=disk-layout=cloudvm
      PMODEL=CLOUDVM
      ;;
    *)
      DISK_LAYOUT_CMDLINE=disk-layout=cloudrc
      PMODEL=CLOUDRC
      ;;
    esac
    FIRST_DRIVE_CACHE=no
    grep -q ^IL_LO_${PMODEL}_PART_DMRAW_TARGET=DISK1 /etc/customer_rootflop.sh
    [ ${?} -eq 0 ] && FIRST_DRIVE_CACHE=yes
    echo auto_reboot > /tmp/auto_reboot
    echo AUTO_MODE=${AUTO_MODE} > /etc/vm_cloudvm
    echo PMODEL=${PMODEL}      >> /etc/vm_cloudvm
    echo FIRST_DRIVE_CACHE=${FIRST_DRIVE_CACHE} >> /etc/vm_cloudvm
    echo vga         > /etc/console_set
    echo poweroff    > /etc/firstboot_action
    echo force,nogroup > /etc/ethreorder_options
    rm -f /etc/ethreorder_eth0 
    rm -f /etc/ethreorder_eth1 
    KVM_vda_Setup
    return
  fi
  while true ; do
    # Just in case the stdin somehow is spewing, limit the number of loops.
    READ_COUNT=$(( ${READ_COUNT} + 1 ))
    [ ${READ_COUNT} -gt 200 ] && Exit_Logout
    echo MFC on VM setup.
    echo Enter 1 for CloudVM installation.
    echo Enter 2 for CloudVM installation "(serial console)".
    echo Enter 3 for CloudRC installation.
    echo Enter 4 for CloudRC installation "(serial console)".
    echo Enter 5 for VM with no persistent eth device across reboot.
    echo Enter 6 to not make these choices.
    echo Enter 0 to Exit without installing or restoring.
    echo
    echo -n "Enter 0, 1, 2 or 3: "
    ANS=
    read ANS
    [ -z "${ANS}" ] && continue
    case "${ANS}" in
    0|exit|quit|q)
      # Exiting with 10 makes root.profile go to the shell prompt.
      # It also prints a message before that about using exit and reboot.
      exit 10
      ;;
    1*|2*)
      A1=`echo "${ANS}" | cut -c1`
      A2=`echo "${ANS}" | cut -c2-`
      FIRST_DRIVE_CACHE=no
      DISK_LAYOUT_CMDLINE=disk-layout=cloudvm
      grep -q ^IL_LO_CLOUDVM_PART_DMRAW_TARGET=DISK1 /etc/customer_rootflop.sh
      [ ${?} -eq 0 ] && FIRST_DRIVE_CACHE=yes
      if [ "${A2}" != "keep" ] ; then
        echo auto_reboot > /tmp/auto_reboot
        echo AUTO_MODE=0    > /etc/vm_cloudvm
        echo PMODEL=CLOUDVM >> /etc/vm_cloudvm
        echo FIRST_DRIVE_CACHE=${FIRST_DRIVE_CACHE} >> /etc/vm_cloudvm
        if [ ${A1} -eq 1 ] ; then
          echo vga       > /etc/console_set
        else
          echo serial    > /etc/console_set
        fi
        echo poweroff    > /etc/firstboot_action
        [ "${A2}" = "reboot"   ] && echo reboot   > /etc/firstboot_action
        [ "${A2}" = "halt"     ] && echo halt     > /etc/firstboot_action
        [ "${A2}" = "poweroff" ] && echo poweroff > /etc/firstboot_action
        [ "${A2}" = "none"     ] && rm -f /etc/firstboot_action
        [ "${A2}" = "auto"     ] && AUTO_INST=auto
        echo force,nogroup > /etc/ethreorder_options
        rm -f /etc/ethreorder_eth0 
        rm -f /etc/ethreorder_eth1 
      fi
      KVM_vda_Setup
      break
      ;;
    3*|4*)
      A1=`echo "${ANS}" | cut -c1`
      A2=`echo "${ANS}" | cut -c2-`
      FIRST_DRIVE_CACHE=no
      DISK_LAYOUT_CMDLINE=disk-layout=cloudrc
      grep -q ^IL_LO_CLOUDRC_PART_DMRAW_TARGET=DISK1 /etc/customer_rootflop.sh
      [ ${?} -eq 0 ] && FIRST_DRIVE_CACHE=yes
      if [ "${A2}" != "keep" ] ; then
        echo auto_reboot > /tmp/auto_reboot
        echo vm_cloudvm  > /etc/vm_cloudvm
        echo AUTO_MODE=0    > /etc/vm_cloudvm
        echo PMODEL=CLOUDRC >> /etc/vm_cloudvm
        echo FIRST_DRIVE_CACHE=${FIRST_DRIVE_CACHE} >> /etc/vm_cloudvm
        if [ ${A1} -eq 1 ] ; then
          echo vga       > /etc/console_set
        else
          echo serial    > /etc/console_set
        fi
        echo poweroff    > /etc/firstboot_action
        [ "${A2}" = "reboot"   ] && echo reboot   > /etc/firstboot_action
        [ "${A2}" = "halt"     ] && echo halt     > /etc/firstboot_action
        [ "${A2}" = "poweroff" ] && echo poweroff > /etc/firstboot_action
        [ "${A2}" = "none"     ] && rm -f /etc/firstboot_action
        [ "${A2}" = "auto"     ] && AUTO_INST=auto
        echo force,nogroup > /etc/ethreorder_options
        rm -f /etc/ethreorder_eth0 
        rm -f /etc/ethreorder_eth1 
      fi
      KVM_vda_Setup
      break
      ;;
    5)
      echo force,nogroup > /etc/ethreorder_options
      rm -f /etc/ethreorder_eth0 
      rm -f /etc/ethreorder_eth1 
      break
      ;;
    6) # Do nothing
      break
      ;;
    *)
      continue
      ;;
    esac
  done
  echo
}

VM_Choice_Step()
{
  [ "${IS_VM}" = "no" ] && return
  while true ; do
    # Just in case the stdin somehow is spewing, limit the number of loops.
    READ_COUNT=$(( ${READ_COUNT} + 1 ))
    [ ${READ_COUNT} -gt 200 ] && Exit_Logout
    echo Enter 1 for VM with persistent eth devices across reboot.
    echo Enter 2 for VM with no persistent eth devices across reboot.
    echo Enter 3 KVM /dev/vda setup.
    echo Enter 4 to not make any of these choices.
    echo Enter 0 to Exit without installing or restoring.
    echo
    echo -n "Enter 0, 1, 2, or 3 : "
    ANS=
    read ANS
    [ -z "${ANS}" ] && continue
    case "${ANS}" in
    0|exit|quit|q)
      # Exiting with 10 makes root.profile go to the shell prompt.
      # It also prints a message before that about using exit and reboot.
      exit 10
      ;;
    1)
      echo nogroup > /etc/ethreorder_options
      #rm -f /etc/ethreorder_eth0 
      #rm -f /etc/ethreorder_eth1 
      break
      ;;
    2)
      echo force,nogroup > /etc/ethreorder_options
      #rm -f /etc/ethreorder_eth0 
      #rm -f /etc/ethreorder_eth1 
      break
      ;;
    3)
      if [ "${IS_KVM}" = "yes" ] ; then
        # KVM uses /dev/vda and needs to be major 253
        rm -f /dev/1:0:0:0
        rm -f /dev/vda*
        mknod /dev/vda b 253 0
        for i in 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
        do
           mknod /dev/vda$i b 253 $i
        done
      else
        echo Not on KVM, nothing to do.
      fi
      break
      ;;
    4) # Do nothing
      break
      ;;
    revert)
      echo Revert previous VM related choices.
      FIRST_DRIVE_CACHE=
      DISK_LAYOUT_CMDLINE=
      rm -f /tmp/auto_reboot 
      rm -f /etc/vm_cloudvm
      rm -f /etc/firstboot_action
      rm -f /etc/ethreorder_options
      break
      ;;
    *)
      continue
      ;;
    esac
  done
  echo
}

Cache_Choice_Step()
{
  while true ; do
    # Just in case the stdin somehow is spewing, limit the number of loops.
    READ_COUNT=$(( ${READ_COUNT} + 1 ))
    [ ${READ_COUNT} -gt 200 ] && Exit_Logout
    echo
    echo Enter 1 to NOT use part of the first disk for cache.
    echo Enter 2 to use part of the first disk for cache.
    echo Enter 3 for an explanation of this choice.
    echo Enter 0 to Exit without installing or restoring.
    echo
    echo -n "Enter 0, 1, 2 or 3 : "
    ANS=
    read ANS
    [ -z "${ANS}" ] && continue
    case "${ANS}" in
    0|exit|quit|q)
      # Exiting with 10 makes root.profile go to the shell prompt.
      # It also prints a message before that about using exit and reboot.
      exit 10
      ;;
    1|no*|default)
      FIRST_DRIVE_CACHE=no
      if [ "${FOUND_VXA_EUSB}" = "yes" -a "${BOOT_CHOICE}" = "eusb" ] ; then
        DISK_LAYOUT_CMDLINE=disk-layout=vxa2
      else
        if [ "${BOOT_CHOICE}" = "mirror" ] ; then
          DISK_LAYOUT_CMDLINE=disk-layout=mirror
        else
          DISK_LAYOUT_CMDLINE=disk-layout=normal
        fi
      fi
      break
      ;;
    2|cache)
      FIRST_DRIVE_CACHE=yes
      if [ "${FOUND_VXA_EUSB}" = "yes" -a "${BOOT_CHOICE}" = "eusb" ] ; then
        DISK_LAYOUT_CMDLINE=disk-layout=vxa1
      else
        DISK_LAYOUT_CMDLINE=disk-layout=cache
      fi
      break
      ;;
    3|help)
      echo ' '
      echo 'If you choose to not have cache on the first drive, then --'
      echo 'For drives up to 244GB:'
      echo '  Between 30% and 40% of the drive will be used for the log partition.'
      echo '  Precisely: (drive size - 38GB)/2'
      echo 'For drives larger than 244GB:'
      echo '  The log partition will be about 103GB + the size of the drive over 244GB.'
      echo ' '
      echo 'If you choose to have cache on the first drive, then --'
      echo 'For drives up to 347GB:'
      echo '  Between 20% and 30% of the drive will be used for the log partition.'
      echo '  Between 20% and 30% the drive will be used for cache.'
      echo '  Precisely: (drive size - 38GB)/3'
      echo 'For drives larger than 347:'
      echo '  The log partition will be about 103GB.'
      echo '  The cache will use about 103GB + the size of the drive over 347GB.'
      Return_To_Continue
      continue
      ;;
    *)
      continue
      ;;
    esac
  done
  echo
  SHOW_LAYOUT_CMDLINE=
}

Boot_Choice_Step()
{
  while true ; do
    # Just in case the stdin somehow is spewing, limit the number of loops.
    READ_COUNT=$(( ${READ_COUNT} + 1 ))
    [ ${READ_COUNT} -gt 200 ] && Exit_Logout
    echo
    if [ "${MIRR_BOOT_OK}" = "yes" ] ; then
      echo Enter 1 to use the first two HDDs for Mirror Boot drive.
      USE_1=", 1"
    else
      USE_1=
    fi
    if [ "${ROOT_BOOT_OK}" = "yes" ] ; then
      echo Enter 2 to use the first HDD for boot drive.
      USE_2=", 2"
    else
      USE_2=
    fi
    if [ "${EUSB_BOOT_OK}" = "yes" ] ; then
      echo Enter 3 to use the internal Flash drive for boot drive.
      USE_3=", 3"
    else
      USE_3=
    fi
    echo Enter 0 to Exit without installing or restoring.
    echo
    echo -n "Enter 0${USE_1}${USE_2}${USE_3} : "
    ANS=
    read ANS
    [ -z "${ANS}" ] && continue
    case "${ANS}" in
    0|exit|quit|q)
      # Exiting with 10 makes root.profile go to the shell prompt.
      # It also prints a message before that about using exit and reboot.
      exit 10
      ;;
    1|mirror)
      if [ -z "${USE_1}" ] ; then
        echo
        echo Invalid input
        echo
        continue
      fi
      BOOT_CHOICE=mirror
      break
      ;;
    2|root)
      if [ -z "${USE_2}" ] ; then
        echo
        echo Invalid input
        echo
        continue
      fi
      BOOT_CHOICE=root
      break
      ;;
    3|flash|eusb)
      if [ -z "${USE_3}" ] ; then
        echo
        echo Invalid input
        echo
        continue
      fi
      BOOT_CHOICE=eusb
      break
      ;;
    *)
      echo
      echo Invalid input
      echo
      continue
      ;;
    esac
  done
  echo
}

Update_Model_VXA()
{
  if [ "${FOUND_VXA_EUSB}" = "yes" -a "${BOOT_CHOICE}" = "eusb" ] ; then
    if [ "${FIRST_DRIVE_CACHE}" = "yes" ] ; then
      DISK_LAYOUT_CMDLINE=disk-layout=vxa1
    else
      DISK_LAYOUT_CMDLINE=disk-layout=vxa2
    fi
  else
    if [ "${BOOT_CHOICE}" = "mirror" ] ; then
      DISK_LAYOUT_CMDLINE=disk-layout=mirror
    else
      DISK_LAYOUT_CMDLINE=disk-layout=normal
    fi
  fi
  SHOW_LAYOUT_CMDLINE=
}

# The install-options.txt file the following (and others) from the boot command line:
IOP_INSTALL_OPT=
# See S30_mfc_install for format and explanations.
if [ -f /tmp/install-options.txt ] ; then
  . /tmp/install-options.txt
fi
# Dot in the file that has the functions Parse_Manuf_Opt() and Help_Manuf_Opt()
. /sbin/parse_manuf_opt.sh

EA_OPT=no
EUSB_BOOT_OK=unset
MIRR_BOOT_OK=unset
ROOT_BOOT_OK=unset
CACHE_CMDLINE=
DISK_LAYOUT_CMDLINE=
ROOT_DEV_CMDLINE=
HDD2_DEV_CMDLINE=
EUSB_DEV_CMDLINE=
BOOT_FROM_CMDLINE=
MAX_FS_CMDLINE=
BUF_PART_CMDLINE=
CACHE_PART_CMDLINE=
LOG_PART_CMDLINE=
COREDUMP_PART_CMDLINE=
ROOT_PART_CMDLINE=
CONFIG_PART_CMDLINE=
VAR_PART_CMDLINE=
XOPT_CMDLINE=
DRYRUN_CMDLINE=
ERR=0
for ITEM in `echo ${IOP_INSTALL_OPT} | sed 's/,/ /g'`
do
  INVALID_FLAG=
  ERR_MSG=
  ERR2_MSG=
  case ${ITEM} in
  bashx)       XOPT_CMDLINE="-x" ;;
  dryrun)      DRYRUN_CMDLINE="--dryrun" ;;
  accept-eula) EA_OPT=yes ;; # installopt=accept-eula
  init-cache)  CACHE_CMDLINE=init-cache ;; # installopt=init-cache
  keep-cache)  CACHE_CMDLINE=keep-cache ;; # installopt=keep-cache
  eusb-boot-ok) EUSB_BOOT_OK=yes        ;; # installopt=eusb-boot-ok
  mirror-boot-ok) MIRR_BOOT_OK=yes      ;; # installopt=mirror-boot-ok
  root-boot-ok) ROOT_BOOT_OK=yes        ;; # installopt=root-boot-ok
  disk-layout=*|root-dev=*|hdd2-dev=*|eusb-dev=*|boot-from=*|max-fs=*|buf-part=*|cache-part=*|log-part=*|coredump-part=*|root-part=*|config-part=*|var-part=*)
               Parse_Manuf_Opt "${ITEM}" ;;
  esac
  if [ -z ${INVALID_FLAG} ] ; then
    case ${ITEM} in
    disk-layout=*) DISK_LAYOUT_CMDLINE=${PARSE_OUT} ;;
    eusb-dev=*)    EUSB_DEV_CMDLINE=${PARSE_OUT} ;;
    root-dev=*)    ROOT_DEV_CMDLINE=${PARSE_OUT}
                   if [ ! -z "${OPT_RDISK_DEV}" ] ; then
                     SIZE=`sfdisk -q -s ${OPT_RDISK_DEV} 2> /dev/null`
                     if [ -z "${SIZE}" ] ; then
                         # Invalid, so do not use
                         ROOT_DEV_CMDLINE=
                     fi
                   fi
                   OPT_RDISK_DEV=
                   ;;
    hdd2-dev=*)    HDD2_DEV_CMDLINE=${PARSE_OUT}
                   if [ ! -z "${OPT_HDD2_DEV}" ] ; then
                     SIZE=`sfdisk -q -s ${OPT_HDD2_DEV} 2> /dev/null`
                     if [ -z "${SIZE}" ] ; then
                         # Invalid, so do not use
                         HDD2_DEV_CMDLINE=
                     fi
                   fi
                   OPT_HDD2_DEV=
                   ;;
    boot-from=*)   BOOT_FROM_CMDLINE=${PARSE_OUT} ;;
    max-fs=*)      MAX_FS_CMDLINE=${PARSE_OUT} ;;
    buf-part=*)    BUF_PART_CMDLINE=${PARSE_OUT} ;;
    cache-part=*)  CACHE_PART_CMDLINE=${PARSE_OUT} ;;
    log-part=*)    LOG_PART_CMDLINE=${PARSE_OUT} ;;
    coredump-part=*) COREDUMP_PART_CMDLINE=${PARSE_OUT} ;;
    root-part=*)   ROOT_PART_CMDLINE=${PARSE_OUT} ;;
    config-part=*) CONFIG_PART_CMDLINE=${PARSE_OUT} ;;
    var-part=*)    VAR_PART_CMDLINE=${PARSE_OUT} ;;
    esac
  else
    ERR=1
    echo
    echo "Invalid pxe-boot manufacturing option: ${ITEM}"
    echo "${ERR_MSG}"
    echo "${ERR2_MSG}"
  fi
done
if [ ${ERR} -ne 0 ] ; then
  echo
  echo Installation failure
  sleep 3
  exit 10
fi
if [ -f /tmp/auto-installed.txt ] ; then
  # When previously tried an automatic install, then
  # do not do the automatic install again.
  # Was that installation successful?  If so, run post-install.sh
  if [ -f /etc/build_version.sh ] ; then
    trap - INT
    rm -f ${STOP_FILE}
    bash ${XOPT_CMDLINE} /sbin/post-install.sh
    RV=${?}
    if [ ${RV} -gt 0 -a ${RV} -lt 9 ] ; then
       echo Installation failure
       sleep 3
    fi
    exit ${RV}
  fi
  CACHE_CMDLINE=
fi
cat /proc/diskstats > /tmp/diskstats.1.txt
# The sas2ircu utility requires that this device node be made.
[ ! -c /dev/mpt2ctl ] && mknod /dev/mpt2ctl c 10 221
echo Detecting hardware.  Please wait...
dmesg -c > /tmp/dmesg-before-hw-check.txt
FIRST_DRIVE_CACHE=NA
VXA_Check
EUSB_Check
Hardware_Snoop
VM_Check
VM_Choice_Auto
# Get disk info
Find_Disks
dmesg -c > /tmp/dmesg-after-hw-check.txt
cat /proc/diskstats > /tmp/diskstats.2.txt

if [ "${IS_VXA}" = "no" ] ; then
  if [ "${EA_OPT}" = "yes" ] ; then
    echo accepted-eula > /tmp/eula_response.txt
  else
    EULA_Step
  fi
  W=`dmidecode -s system-product-name`
  V=`dmidecode -s system-version`
  U=`dmidecode -s system-uuid`
  rm -f /tmp/mfg-test.txt
  echo "    Manufacturer: ${SMFG}"                            >> /tmp/mfg-test.txt
  [ "_${W}" != "_Not Specified" ] && echo "    Product: ${W}" >> /tmp/mfg-test.txt
  [ "_${V}" != "_Not Specified" ] && echo "    Version: ${V}" >> /tmp/mfg-test.txt
  echo "    Serial Number: ${SERIAL_NUMBER}"                  >> /tmp/mfg-test.txt
  [ "_${U}" != "_Not Specified" ] && echo "    UUID: ${U}"    >> /tmp/mfg-test.txt
  echo "    RAM: ${RAM_GB} GB"                                >> /tmp/mfg-test.txt
  echo
  echo "Hardware detected:"
  cat /tmp/mfg-test.txt
fi

echo

# The funcion Parse_Manuf_Opt sets OPT_BOOT_FROM when boot-from= was specified.
# Configure the boot defaults.
BOOT_CHOICE=${OPT_BOOT_FROM}
if [ "${IS_VXA}" = "no" ] ; then
  ROOT_BOOT_OK=yes
  EUSB_BOOT_OK=no
  if [ -z "${BOOT_CHOICE}" ] ; then
    # Default: boot from root drive
    BOOT_CHOICE=root
  elif [ "${BOOT_CHOICE}" = "eusb" ] ; then
    echo eUSB choice is only for VXA machines.
    BOOT_CHOICE=root
  fi
else
  # Is VXA.
  echo accepted-eula > /tmp/eula_response.txt
  # Note: VXA_Hardware_Check sets FIRST_DRIVE_CACHE
  # Note: VXA_Hardware_Check sets MODEL_NAME
  VXA_Hardware_Check
  # Set boot defaults.
  if [ "${FOUND_VXA_EUSB}" = "yes" ] ; then
     # On some future VXA hardware we might not want to allow booting from eUSB
     # unless a command line override is done.
     # For now if eUSB exists, allow booting from it.
     # so the command line override currently has no effect.
     EUSB_BOOT_OK=yes
  else
     EUSB_BOOT_OK=no
  fi
  if [ -z "${BOOT_CHOICE}" ] ; then
    # Default: boot from eUSB on VXA when allowed.
    if [ "${EUSB_BOOT_OK}" = "yes" ] ; then
      BOOT_CHOICE=eusb
    else
      BOOT_CHOICE=root
    fi
  fi
  if [ "${BOOT_CHOICE}" = "eusb" -a "${EUSB_BOOT_OK}" != "yes" ] ; then
    echo Cannot boot from eUSB drive
    BOOT_CHOICE=root
  fi
  # The default is to not allow the choice of RAID boot except on VXA2100.
  if [ "${MODEL_NAME}" = "VXA2100" ] ; then
    # For now only on VXA2100 default to allow RAID boot.
    MIRR_BOOT_OK=yes
  fi
fi

# Validate the default or command line specified root drive.
if [ ! -z "${OPT_RDISK_DEV}" ] ; then
  SIZE=`sfdisk -q -s ${OPT_RDISK_DEV} 2> /dev/null`
  if [ -z "${SIZE}" ] ; then
    # Invalid, so do not use
    echo "Specified drive does not exist: ${OPT_RDISK_DEV}"
    DEFAULT_ROOT_DISK_DEV=unknown
    DEFAULT_ROOT_DISK_PDEV=unknown
  else
    DEFAULT_ROOT_DISK_DEV=${OPT_RDISK_DEV}
    DEFAULT_ROOT_DISK_PDEV=${OPT_RDISK_PDEV}
  fi
elif [ "${DEFAULT_ROOT_DISK_DEV}" != "na" ] ; then
  SIZE=`sfdisk -q -s ${DEFAULT_ROOT_DISK_DEV} 2> /dev/null`
  if [ -z "${SIZE}" ] ; then
    # Invalid, so do not use
    echo "Default 2nd drive does not exist: ${DEFAULT_ROOT_DISK_DEV}"
    DEFAULT_ROOT_DISK_DEV=unknown
    DEFAULT_ROOT_DISK_PDEV=unknown
  fi
fi

# Validate the default or command line specified 2nd drive.
if [ ! -z "${OPT_HDD2_DEV}" ] ; then
  SIZE=`sfdisk -q -s ${OPT_HDD2_DEV} 2> /dev/null`
  if [ -z "${SIZE}" ] ; then
    # Invalid, so do not use
    echo "Specified drive does not exist: ${OPT_HDD2_DEV}"
    DEFAULT_HDD2_DISK_DEV=na
    DEFAULT_HDD2_DISK_PDEV=na
  else
    DEFAULT_HDD2_DISK_DEV=${OPT_HDD2_DEV}
    DEFAULT_HDD2_DISK_PDEV=${OPT_HDD2_PDEV}
  fi
elif [ "${DEFAULT_HDD2_DISK_DEV}" != "na" ] ; then
  SIZE=`sfdisk -q -s ${DEFAULT_HDD2_DISK_DEV} 2> /dev/null`
  if [ -z "${SIZE}" ] ; then
    # Invalid, so do not use
    echo "Default 2nd drive does not exist: ${DEFAULT_HDD2_DISK_DEV}"
    DEFAULT_HDD2_DISK_DEV=na
    DEFAULT_HDD2_DISK_PDEV=na
  fi
fi
if [ "${BOOT_CHOICE}" = "mirror" ] ; then
  # Disallow RAID boot if no drive specified or found.
  if [ "${DEFAULT_HDD2_DISK_DEV}" = "na" ] ; then
    echo Cannot boot from Mirror root because suitable drives not found.
    MIRR_BOOT_OK=no
    BOOT_CHOICE=root
  fi
fi

[ "${EUSB_BOOT_OK}" = "unset" ] && EUSB_BOOT_OK=no
[ "${MIRR_BOOT_OK}" = "unset" ] && MIRR_BOOT_OK=no
[ "${ROOT_BOOT_OK}" = "unset" ] && ROOT_BOOT_OK=no

echo Disks found
cat /tmp/disks-found.log

if [ "${AUTO_INST}" = "auto" ] ; then
  [ -z "${CACHE_CMDLINE}" ] && CACHE_CMDLINE=init-cache
fi
SHOW_LAYOUT_CMDLINE=
if [ -z ${DISK_LAYOUT_CMDLINE} ] ; then
  if [ "${IS_VXA}" = "yes" ] ; then
    # Update_Model_VXA sets  DISK_LAYOUT_CMDLINE
    Update_Model_VXA
  else
    if [ "${AUTO_INST}" = "auto" ] ; then
      FIRST_DRIVE_CACHE=no
      DISK_LAYOUT_CMDLINE=disk-layout=normal
    else
      # Ask the user what to do.
      # Note: Cache_Choice_Step sets FIRST_DRIVE_CACHE and DISK_LAYOUT_CMDLINE
      Cache_Choice_Step
    fi
  fi
else
  # Normalize the disk layout cmdline parameter.
  case ${DISK_LAYOUT_CMDLINE} in
  *cache|*vxa1)
    FIRST_DRIVE_CACHE=yes
    if [ "${FOUND_VXA_EUSB}" = "yes" -a "${BOOT_CHOICE}" = "eusb" ] ; then
      DISK_LAYOUT_CMDLINE=disk-layout=vxa1
    else
      DISK_LAYOUT_CMDLINE=disk-layout=cache
    fi
    ;;
  *nocache|*vxa2|*normal)
    # Note: change "nocache" to "normal" or "vxa2".
    FIRST_DRIVE_CACHE=no
    if [ "${FOUND_VXA_EUSB}" = "yes" -a "${BOOT_CHOICE}" = "eusb" ] ; then
      DISK_LAYOUT_CMDLINE=disk-layout=vxa2
    else
      DISK_LAYOUT_CMDLINE=disk-layout=normal
    fi
    ;;
  *mirror)
    FIRST_DRIVE_CACHE=no
    DISK_LAYOUT_CMDLINE=disk-layout=mirror
    ;;
  *demo*1)
    # The demo layout names that end in "1" are the ones that include cache. (demo32g1)
    FIRST_DRIVE_CACHE=yes
    SHOW_LAYOUT_CMDLINE=${DISK_LAYOUT_CMDLINE}
    ;;
  *demo*2)
    # The demo layout names that end in "2" are the ones that do not have cache. (demo8g2)
    FIRST_DRIVE_CACHE=no
    SHOW_LAYOUT_CMDLINE=${DISK_LAYOUT_CMDLINE}
    ;;
  *cloudvm)
    FIRST_DRIVE_CACHE=no
    grep -q ^IL_LO_CLOUDVM_PART_DMRAW_TARGET=DISK1 /etc/customer_rootflop.sh
    [ ${?} -eq 0 ] && FIRST_DRIVE_CACHE=yes
    ;;
  *cloudrc)
    FIRST_DRIVE_CACHE=no
    grep -q ^IL_LO_CLOUDRC_PART_DMRAW_TARGET=DISK1 /etc/customer_rootflop.sh
    [ ${?} -eq 0 ] && FIRST_DRIVE_CACHE=yes
    ;;
  *)
    FIRST_DRIVE_CACHE=NA
    SHOW_LAYOUT_CMDLINE=${DISK_LAYOUT_CMDLINE}
    ;;
  esac
fi
#DEFAULT_COREDUMP_SIZE_MB=`expr ${RAM_MB} + 5`
DEFAULT_COREDUMP_SIZE_MB=not_calculated
if [ -z "${COREDUMP_PART_CMDLINE}" -a "${DEFAULT_COREDUMP_SIZE_MB}" != "not_calculated" ] ; then
  COREDUMP_PART_CMDLINE=coredump-part=${DEFAULT_COREDUMP_SIZE_MB}
fi

##########################################################################
# If CACHE_CMDLINE is not set at this point, then we need to interactively
# select either "first drive with cache" or "first drive without cache".
# If BOOT_CHOICE is not set at this point, then we need to interactively
# select eusb, root or mirror.
# 
# Also in this interactive step, the user can enter hidden overrides for
# the device names, partition sizes, and so on.
CMDLINE_OPTS_LINE=
READ_COUNT=0
DO_INTERACTIVE=no
[ -z "${CACHE_CMDLINE}" ] && DO_INTERACTIVE=yes
[ -z "${BOOT_CHOICE}" ] && DO_INTERACTIVE=yes
if [ "${DO_INTERACTIVE}" = "yes"  ] ; then
  # INTERACTIVE ------------------------------------------- INTERACTIVE
  rm -f ${STOP_FILE}
  while true ; do
    # Just in case the stdin somehow is spewing, limit the number of loops.
    READ_COUNT=$(( ${READ_COUNT} + 1 ))
    [ ${READ_COUNT} -gt 200 ] && Exit_Logout

    # If more than one boot type allowed, then show choice.
    SHOW_BOOT_CHOICE=no
    if [ "${MIRR_BOOT_OK}" = "yes" ] ; then
       [ "${EUSB_BOOT_OK}" = "yes" ] && SHOW_BOOT_CHOICE=yes
       [ "${ROOT_BOOT_OK}" = "yes" ] && SHOW_BOOT_CHOICE=yes
    fi
    if [ "${EUSB_BOOT_OK}" = "yes" ] ; then
       [ "${MIRR_BOOT_OK}" = "yes" ] && SHOW_BOOT_CHOICE=yes
       [ "${ROOT_BOOT_OK}" = "yes" ] && SHOW_BOOT_CHOICE=yes
    fi

    if [ ! -z "${ROOT_DEV_CMDLINE}" ] ; then
      RD=`echo "${ROOT_DEV_CMDLINE}" | cut -c 10-`
      echo "Install on disk `basename ${RD}`"
    elif [ "${DEFAULT_ROOT_DISK_DEV}" != "unknown" ] ; then
      echo "Install on disk `basename ${DEFAULT_ROOT_DISK_DEV}`"
    fi
    echo
    echo Enter 1 to Install Media Flow Controller.
    if [ -f /etc/vm_cloudvm ] ; then
      USE_2=
    else
      USE_2=", 2"
      echo Enter 2 to Restore Media Flow Controller.
    fi
    USE_5=
    if [ ! -f /etc/vm_cloudvm ] ; then
      if [ "${BOOT_CHOICE}" != "mirror" ] ; then
        case ${FIRST_DRIVE_CACHE} in
        yes)
          echo "Enter 5 to Change the first drive cache choice. (currently with cache)"
          USE_5=", 5"
          ;;
        no)
          echo "Enter 5 to Change the first drive cache choice. (currently with no cache)"
          USE_5=", 5"
          ;;
        esac
     fi
    fi
    USE_6=
    if [ "${SHOW_BOOT_CHOICE}" = "yes" ] ; then
      USE_6=", 6"
      case "${BOOT_CHOICE}" in
      eusb)
        echo "Enter 6 to Change the boot drive choice. (currently: Internal Flash)"
        ;;
      root)
        echo "Enter 6 to Change the boot drive choice. (currently: HDD)"
        ;;
      mirror)
        echo "Enter 6 to Change the boot drive choice. (currently: HDD Mirror)"
        ;;
      esac
    fi
    USE_7=
    if [ "${IS_VM}" != "no" ] ; then
        echo "Enter 7 for VM related choices"
        USE_7=", 7"
    fi
    if [ -f /etc/vm_cloudvm ] ; then
      USE_8=
    else
      USE_8=", 8"
      echo Enter 8 to Reboot without installing or restoring.
    fi
    echo Enter 9 to Halt without installing or restoring.
    echo Enter 0 to Exit without installing or restoring.
    echo Or enter special options as instructed by support personnel.
    CMDLINE_OPTS_LINE=
    A="${SHOW_LAYOUT_CMDLINE}${ROOT_DEV_CMDLINE}${HDD2_DEV_CMDLINE}${EUSB_DEV_CMDLINE}${BOOT_FROM_CMDLINE}${MAX_FS_CMDLINE}${BUF_PART_CMDLINE}${CACHE_PART_CMDLINE}${LOG_PART_CMDLINE}${ROOT_PART_CMDLINE}${CONFIG_PART_CMDLINE}${VAR_PART_CMDLINE}"
    # At this point, COREDUMP_PART_CMDLINE might be set to calculated default.
    # Only show the COREDUMP_PART_CMDLINE setting when it is NOT the default.
    if [ "_${COREDUMP_PART_CMDLINE}" != "_coredump-part=${DEFAULT_COREDUMP_SIZE_MB}" ] ; then
      A=yes
    fi
    if [ ! -z "${A}" ] ; then
      echo '   Current special options:'
      for i in "${SHOW_LAYOUT_CMDLINE}" \
             "${ROOT_DEV_CMDLINE}" "${HDD2_DEV_CMDLINE}" "${EUSB_DEV_CMDLINE}" \
             "${BOOT_FROM_CMDLINE}" "${MAX_FS_CMDLINE}" \
             "${BUF_PART_CMDLINE}" "${CACHE_PART_CMDLINE}" "${LOG_PART_CMDLINE}" \
             "${COREDUMP_PART_CMDLINE}" \
             "${ROOT_PART_CMDLINE}" "${CONFIG_PART_CMDLINE}" "${VAR_PART_CMDLINE}" \
             ""
      do
        [ -z "${i}" ] && continue
        if [ "${i}" != "coredump-part=${DEFAULT_COREDUMP_SIZE_MB}" ] ; then
          CMDLINE_OPTS_LINE="${CMDLINE_OPTS_LINE} ${i}"
          echo "      ${i}"
        fi
      done
    fi
    if [ "_${COREDUMP_PART_CMDLINE}" = "_coredump-part=${DEFAULT_COREDUMP_SIZE_MB}" ] ; then
      CMDLINE_OPTS_LINE="${CMDLINE_OPTS_LINE} ${COREDUMP_PART_CMDLINE}"
    fi
    [ ! -z ${DRYRUN_CMDLINE} ] && echo "${DRYRUN_CMDLINE}"
    [ ! -z ${XOPT_CMDLINE} ] && echo "${XOPT_CMDLINE}"
    if [ "_${ROOT_DEV_CMDLINE}" = "_" -a "${DEFAULT_ROOT_DISK_DEV}" = "unknown" ] ; then
      if [ ${FOUND_SDA} -eq 0 -a ${FOUND_HDA} -eq 0 -a ${FOUND_CCISS} -eq 0 ] ; then
        echo "Known disk controller not found."
        echo "You can try installing on this unsupported hardware by specifying the"
        echo "root drive device name with the special option: root-dev=/dev/<devicename>"
      else
        echo "More than one disk controller type found, so you must specify the"
        echo "root drive device name with one of these special options:"
        [ ${FOUND_SDA}   -ne 0 ] && echo "   root-dev=sda"
        [ ${FOUND_HDA}   -ne 0 ] && echo "   root-dev=hda"
        [ ${FOUND_CCISS} -ne 0 ] && echo "   root-dev=cciss"
      fi
    fi
    if [ ! -z "${USE_2}" ] ; then
      echo
      echo "Note:"
      echo "    Install will delete all cached data."
      echo "    Restore attempts to keep cached data."
    fi
    echo
    echo -n "Enter 0, 1${USE_2}${USE_5}${USE_6}${USE_7}${USE_8}, 9 : "
    ITEM=
    read ITEM
    [ -z "${ITEM}" ] && continue
    # Note: Parse_Manuf_Opt() sets INVALID_FLAG when bad syntax.
    export INVALID_FLAG=
    case "${ITEM}" in
    1|install) CACHE_CMDLINE=init-cache ; break ;;
    2|restore) 
      if [ -z "${USE_2}" ] ; then
        echo
        echo Invalid input
        echo
        continue
      fi
      CACHE_CMDLINE=keep-cache ; break ;;
    8|reboot)
      if [ -z "${USE_8}" ] ; then
        echo
        echo Invalid input
        echo
        continue
      fi
      Reboot ;;
    9|halt)    Halt ;;
    5|cache-choice)
      if [ -z "${USE_5}" ] ; then
        echo
        echo Invalid input
        echo
        continue
      fi
      Cache_Choice_Step
      ;;
    6|boot-choice)
      if [ -z "${USE_6}" ] ; then
        echo
        echo Invalid input
        echo
        continue
      fi
      Boot_Choice_Step
      # The above call to Boot_Choice_Step set BOOT_CHOICE=
      # Zero BOOT_FROM_CMDLINE so it does not show in the special opts list.
      BOOT_FROM_CMDLINE=
      # Call Update_Model_VXA() to set DISK_LAYOUT_CMDLINE=
      Update_Model_VXA
      ;;
    7|vm-choice)
      if [ -z "${USE_7}" ] ; then
        echo
        echo Invalid input
        echo
        continue
      fi
      VM_Choice_Step
      ;;
    boot-from=*)
      Parse_Manuf_Opt "${ITEM}"
      # The above call to Parse_Manuf_Opt set OPT_BOOT_FROM=
      case ${OPT_BOOT_FROM} in
      eusb)
        FOUND_VXA_EUSB=
        EUSB_BOOT_OK=no
        EUSB_Check
        if [ "${FOUND_VXA_EUSB}" = "yes" ] ; then
          BOOT_CHOICE=eusb
          EUSB_BOOT_OK=yes
        else
          # ERROR!  Default to boot from root drive.
          BOOT_CHOICE=root
          ROOT_BOOT_OK=yes
          EUSB_BOOT_OK=no
          echo
          echo No eUSB device found!
          echo
        fi
        Update_Model_VXA
        ;;
      root)    
        BOOT_CHOICE=root
        ROOT_BOOT_OK=yes
        Update_Model_VXA
        ;;
      mirror)    
        BOOT_CHOICE=mirror
        MIRR_BOOT_OK=yes
        Update_Model_VXA
        ;;
      esac
      BOOT_FROM_CMDLINE=boot-from=${BOOT_CHOICE}
      ;;
    root-boot-ok)    
      ROOT_BOOT_OK=yes
      ;;
    mirror-boot-ok)    
      MIRR_BOOT_OK=yes
      ;;
    disk-layout=*|root-dev=*|hdd2-dev=*|max-fs=*|buf-part=*|cache-part=*|log-part=*|coredump-part=*|root-part=*|config-part=*|var-part=*)
      echo
      OPT_RDISK_DEV=
      OPT_HDD2_DEV=
      Parse_Manuf_Opt "${ITEM}"
      ;;
    eusb-dev=*)
      echo
      Parse_Manuf_Opt "${ITEM}"
      EUSB_Check
      if [ "${FOUND_VXA_EUSB}" = "yes" ] ; then
        BOOT_CHOICE=eusb
        EUSB_BOOT_OK=yes
      else
        # ERROR!  Default to boot from root drive.
        BOOT_CHOICE=root
        ROOT_BOOT_OK=yes
        EUSB_BOOT_OK=no
        echo
        echo No eUSB device found!
        echo
      fi
      ;;
    HELP)
      echo
      Help_Manuf_Opt all
      Return_To_Continue
      echo
      ;;
    0|exit|quit|q)
      # Exiting with 10 makes root.profile go to the shell prompt.
      # It also prints a message before that about using exit and reboot.
      exit 10
      ;;
    DEBUG*|*DEBUG)
      echo
      echo dryrun
      echo dryrunoff
      echo bash
      echo bashx
      if [ "_${IS_VXA}" = "_yes" ] ; then
        echo eusb
        echo noeusb
      fi
      Return_To_Continue
      echo
      ;;
    dryrun)    DRYRUN_CMDLINE="--dryrun" ;;
    dryrunoff) DRYRUN_CMDLINE= ;;
    bash)      XOPT_CMDLINE= ;;
    bashx)     XOPT_CMDLINE="-x" ;;
    *)
      echo
      echo Invalid input
      echo
      continue
      ;;
    esac

    if [ ! -z ${INVALID_FLAG} ] ; then
      A=`echo "${ITEM}" | cut -f1 -d=`
      B=`echo "${ITEM}" | cut -f2- -d=`
      echo
      if [ "${A}" = "${B}" ] ; then
        echo The ${A} setting is not valid
      else
        echo The ${A} setting is not valid: "${B}"
      fi
      echo "${ERR_MSG}"
      echo "${ERR2_MSG}"
      echo ""
      continue
    fi
    
    case ${ITEM} in
    disk-layout=*) DISK_LAYOUT_CMDLINE=${PARSE_OUT}
                   SHOW_LAYOUT_CMDLINE=${DISK_LAYOUT_CMDLINE}
                   FIRST_DRIVE_CACHE=NA
                   ;;
    root-dev=*)    ROOT_DEV_CMDLINE=${PARSE_OUT}
                   if [ ! -z "${OPT_RDISK_DEV}" ] ; then
                     SIZE=`sfdisk -q -s ${OPT_RDISK_DEV} 2> /dev/null`
                     if [ -z "${SIZE}" ] ; then
                       echo Error, no such device ${OPT_RDISK_DEV}
                       ROOT_DEV_CMDLINE=
                       OPT_RDISK_DEV=
                     fi
                   fi
                   if [ -z "${OPT_RDISK_DEV}" ] ; then
                     DEFAULT_ROOT_DISK_DEV=unknown
                     DEFAULT_ROOT_DISK_PDEV=unknown
                   else
                     DEFAULT_ROOT_DISK_DEV=${OPT_RDISK_DEV}
                     DEFAULT_ROOT_DISK_PDEV=${OPT_RDISK_PDEV}
                   fi
                   ;;
    hdd2-dev=*)    HDD2_DEV_CMDLINE=${PARSE_OUT}
                   if [ ! -z "${OPT_HDD2_DEV}" ] ; then
                     SIZE=`sfdisk -q -s ${OPT_HDD2_DEV} 2> /dev/null`
                     if [ -z "${SIZE}" ] ; then
                       echo Error, no such device ${OPT_RDISK_DEV}
                       HDD2_DEV_CMDLINE=
                       OPT_HDD2_DEV=
                     fi
                   fi
                   if [ -z "${OPT_HDD2_DEV}" ] ; then
                     DEFAULT_HDD2_DISK_DEV=na
                     DEFAULT_HDD2_DISK_PDEV=na
                   else
                     DEFAULT_HDD2_DISK_DEV=${OPT_HDD2_DEV}
                     DEFAULT_HDD2_DISK_PDEV=${OPT_HDD2_PDEV}
                   fi
                   ;;
    eusb-dev=*)    EUSB_DEV_CMDLINE=${PARSE_OUT} ;;
    max-fs=*)      MAX_FS_CMDLINE=${PARSE_OUT} ;;
    buf-part=*)    BUF_PART_CMDLINE=${PARSE_OUT} ;;
    cache-part=*)  CACHE_PART_CMDLINE=${PARSE_OUT} ;;
    log-part=*)    LOG_PART_CMDLINE=${PARSE_OUT} ;;
    coredump-part=*) COREDUMP_PART_CMDLINE=${PARSE_OUT} ;;
    root-part=*)   ROOT_PART_CMDLINE=${PARSE_OUT} ;;
    config-part=*) CONFIG_PART_CMDLINE=${PARSE_OUT} ;;
    var-part=*)    VAR_PART_CMDLINE=${PARSE_OUT} ;;
    esac

    # Validate drives for use with RAID.
    BAD_CHOICE=
    if [ "${BOOT_CHOICE}" = "mirror" ] ; then
      if [ "${DEFAULT_HDD2_DISK_DEV}" = "na" ] ; then
        echo Warning, Cannot boot from Mirror root because suitable
        echo second drive not found or specified yet.
        BAD_CHOICE=mirror
      elif [ ! -b "${DEFAULT_HDD2_DISK_DEV}" ] ; then
        echo Warning, Cannot boot from Mirror root because the device
        echo specified for the second drive is invalid. ${DEFAULT_HDD2_DISK_DEV}
        BAD_CHOICE=mirror
      elif [ ! -b "${DEFAULT_ROOT_DISK_DEV}" ] ; then
        echo Warning, Cannot boot from Mirror root because the device
        echo specified for the first drive is invalid. ${DEFAULT_ROOT_DISK_DEV}
        BAD_CHOICE=mirror
      else
        # Verify sizes for RAID disks.  Get the size in MB.
        S1=`sfdisk -q -uM -s ${DEFAULT_ROOT_DISK_DEV} 2> /dev/null`
        S2=`sfdisk -q -uM -s ${DEFAULT_HDD2_DISK_DEV} 2> /dev/null`
        if [ -z "${S1}" ] ; then
          echo Warning, Cannot boot from Mirror root because the device
          echo specified for the first drive is invalid. ${DEFAULT_ROOT_DISK_DEV}
          BAD_CHOICE=mirror
        elif [ -z "${S2}" ] ; then
          echo Warning, Cannot boot from Mirror root because the device
          echo specified for the second drive is invalid. ${DEFAULT_HDD2_DISK_DEV}
          BAD_CHOICE=mirror
        else
          SIZE_DIFF=$((S1 - S2))
          if [ ${SIZE_DIFF} -ge 10000 -o ${SIZE_DIFF} -le -10000 ] ; then
            echo Warning, Cannot boot from Mirror root because the drive
            echo sizes are different by 10GB or more.
            BAD_CHOICE=mirror
          elif [ ${S1} -gt ${S2} ] ; then
            # Until we are able to support the first disk being larger than
            # the second disk, disallow that situation.
            echo Warning, Cannot boot from Mirror root because the first drive
            echo is larger than the second.
            BAD_CHOICE=mirror
          fi
        fi
      fi
    fi # endif validate mirror
    if [ "_${BAD_CHOICE}" = "_mirror" ] ; then
      # Change the boot choice to something other than mirror.
      if [ "${EUSB_BOOT_OK}" = "yes" ] ; then
        BOOT_CHOICE=eusb
      else
        BOOT_CHOICE=root
        ROOT_BOOT_OK=yes
      fi
    fi
    # End of input loop  
  done
  echo Selected install opts: ${CACHE_CMDLINE} "${CMDLINE_OPTS_LINE}" \
    > /tmp/select-installed.txt
  BOOT_FROM_CMDLINE=boot-from=${BOOT_CHOICE}
else
  # NON-INTERACTIVE ------------------------------------------- NON-INTERACTIVE
  # If we are here then CACHE_CMDLINE was already set.
  if [ -z "${BOOT_FROM_CMDLINE}" ] ; then
    BOOT_FROM_CMDLINE=boot-from=${BOOT_CHOICE}
  fi
  if [ "${BOOT_CHOICE}" != "eusb" ] ; then
    EUSB_DEV_CMDLINE=
  fi
  CMDLINE_OPTS_LINE=
  for i in "${DISK_LAYOUT_CMDLINE}" \
           "${ROOT_DEV_CMDLINE}" "${HDD2_DEV_CMDLINE}" "${EUSB_DEV_CMDLINE}" \
           "${BOOT_FROM_CMDLINE}" "${MAX_FS_CMDLINE}" \
           "${BUF_PART_CMDLINE}" "${CACHE_PART_CMDLINE}" "${LOG_PART_CMDLINE}" \
           "${COREDUMP_PART_CMDLINE}" \
           "${ROOT_PART_CMDLINE}" "${CONFIG_PART_CMDLINE}" "${VAR_PART_CMDLINE}" \
           ""
  do
    [ -z "${i}" ] && continue
    CMDLINE_OPTS_LINE="${CMDLINE_OPTS_LINE} ${i}"
  done
  echo Automatic installed: ${CACHE_CMDLINE} "${CMDLINE_OPTS_LINE}" \
    > /tmp/auto-installed.txt
fi
echo
if [ -z "${CACHE_CMDLINE}" ] ; then
  Exit_Logout
fi
BOOT_FROM_CMDLINE="boot-from=${BOOT_CHOICE}"
if [ "_${IS_VXA}" = "_yes" ] ; then
  if [ "${BOOT_CHOICE}" = "eusb" ] ; then
    if [ -z "${EUSB_DEV_CMDLINE}" ] ; then
      EUSB_DEV_CMDLINE="eusb-dev=${EUSB_DEV_NAME}"
    fi
  else
    EUSB_DEV_CMDLINE=
    if [ -f /tmp/eusb-root-dev.txt ] ; then
      mv /tmp/eusb-root-dev.txt /tmp/unused-eusb-root-dev.txt
    fi
  fi
fi
if [ "_${ROOT_DEV_CMDLINE}" = "_" -a "${DEFAULT_ROOT_DISK_DEV}" = "unknown" ] ; then
  if [ ${FOUND_SDA} -eq 0 -a ${FOUND_HDA} -eq 0 -a ${FOUND_CCISS} -eq 0 -a ${FOUND_VDA} -eq 0 ] ; then
    echo "Known disk controller not found."
    echo "You can try installing on this unsupported hardware by specifying the"
    echo "root drive device name with the special option: root-dev=/dev/<devicename>"
  else
    echo "More than one disk controller type found, so you must specify the"
    echo "root drive device name."
    echo "Specify one of:"
    [ ${FOUND_SDA}   -ne 0 ] && echo "   root-dev=sda"
    [ ${FOUND_HDA}   -ne 0 ] && echo "   root-dev=hda"
    [ ${FOUND_CCISS} -ne 0 ] && echo "   root-dev=cciss"
    [ ${FOUND_VDA}   -ne 0 ] && echo "   root-dev=vda"
  fi
  echo
  Exit_Logout
fi
if [ -z "${ROOT_DEV_CMDLINE}" ] ; then
  if [ "${DEFAULT_ROOT_DISK_DEV}" = "${DEFAULT_ROOT_DISK_PDEV}" ] ; then
    ROOT_DEV_CMDLINE=root-dev=${DEFAULT_ROOT_DISK_DEV}
  else
    ROOT_DEV_CMDLINE="root-dev=${DEFAULT_ROOT_DISK_DEV};${DEFAULT_ROOT_DISK_PDEV}"
  fi
fi
if [ -z "${HDD2_DEV_CMDLINE}" ] ; then
  if [ "${DEFAULT_HDD2_DISK_DEV}" = "${DEFAULT_HDD2_DISK_PDEV}" ] ; then
    HDD2_DEV_CMDLINE=hdd2-dev=${DEFAULT_HDD2_DISK_DEV}
  else
    HDD2_DEV_CMDLINE="hdd2-dev=${DEFAULT_HDD2_DISK_DEV};${DEFAULT_HDD2_DISK_PDEV}"
  fi
fi

echo "Starting the install/restore process >>>"
echo
echo "Warning : Installation process could take anywhere from 10 minutes to "
echo "even more than an hour depending on the number and size of the disks. "
echo "In between you might not see any messages being printed and that is normal."
echo "Please expect long pauses and be patient."
echo
echo "Thanks for installing Media Flow Controller"
echo
sleep 5;

trap - INT
rm -f ${STOP_FILE}
# Note that install-mfc will run the post-install.sh script
echo bash ${XOPT_CMDLINE} /sbin/install-mfc \
  --nousage ${XOPT_CMDLINE} ${DRYRUN_CMDLINE} \
  "${CACHE_CMDLINE}" "${DISK_LAYOUT_CMDLINE}" \
  "${ROOT_DEV_CMDLINE}" "${HDD2_DEV_CMDLINE}" "${EUSB_DEV_CMDLINE}" \
  "${BOOT_FROM_CMDLINE}" "${MAX_FS_CMDLINE}" \
  "${BUF_PART_CMDLINE}" "${CACHE_PART_CMDLINE}" "${LOG_PART_CMDLINE}" \
  "${COREDUMP_PART_CMDLINE}" \
  "${ROOT_PART_CMDLINE}" "${CONFIG_PART_CMDLINE}" "${VAR_PART_CMDLINE}" \
  > /tmp/install-mfc-cmd.txt
bash ${XOPT_CMDLINE} /sbin/install-mfc \
  --nousage ${XOPT_CMDLINE} ${DRYRUN_CMDLINE} \
  "${CACHE_CMDLINE}" "${DISK_LAYOUT_CMDLINE}" \
  "${ROOT_DEV_CMDLINE}" "${HDD2_DEV_CMDLINE}" "${EUSB_DEV_CMDLINE}" \
  "${BOOT_FROM_CMDLINE}" "${MAX_FS_CMDLINE}" \
  "${BUF_PART_CMDLINE}" "${CACHE_PART_CMDLINE}" "${LOG_PART_CMDLINE}" \
  "${COREDUMP_PART_CMDLINE}" \
  "${ROOT_PART_CMDLINE}" "${CONFIG_PART_CMDLINE}" "${VAR_PART_CMDLINE}" \
  ""
exit ${?}
