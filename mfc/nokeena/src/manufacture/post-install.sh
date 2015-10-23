#! /bin/bash
# Do the post-install steps.
#
#

EXITING=no
STOP_FILE=/tmp/stop.$$.tmp
trap_int_handler()
{
  if [ "${EXITING}" = "yes" ] ; then
    Finish
    exit 8
  fi
  echo
  echo "(Interrupt)"
  echo
  touch ${STOP_FILE}
}
trap "trap_int_handler" INT

Exit_Logout()
{
  EXITING=yes
  echo ...
  echo "Login as root to install MFC"
  sleep 2
  # This script is run by root.profile, which does the following
  # with the return code:
  # 0-8 prints out "Error, Install returned <number>." and then goes
  # to the login prompt.
  # 9 just goes to the login prompt without printing anything.
  # 10 and higher makes it go to the shell.
  Finish
  exit 9
}

Finish()
{
  EXITING=yes
  rm -f ${STOP_FILE}
  trap - INT
  cp /var/log/messages /tmp/kernel-messages.txt
  cp /tmp/*.* ${MFGLOG_DIR}
  umount ${LOG_MNT}
  umount ${CONFIG_MNT}
  umount ${ROOT1_MNT}
  umount ${ROOT2_MNT}
  sleep 2
}

Reboot()
{
  Finish
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
  Finish
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
# The file /tmp/mfg.env is created by install-mfc and sets
#   LOG_DEV    LOG_MNT
#   CONFIG_DEV CONFIG_MNT
#   ROOT1_DEV  ROOT1_MNT
#   ROOT2_DEV  ROOT2_MNT
# so that we can mount which parts of the installed filesystems we need.
if [ ! -f /tmp/mfg.env ] ; then
  echo Error installing.  Failed to create /tmp/mfg.env
  # Exiting with 11 makes root.profile go to the shell prompt.
  exit 11
fi
. /tmp/mfg.env
export LOG_DEV LOG_MNT CONFIG_DEV CONFIG_MNT ROOT1_DEV ROOT1_MNT ROOT2_DEV ROOT2_MNT
[ -d ${LOG_MNT} ]    && umount ${LOG_MNT} > /dev/null 2>&1
[ -d ${CONFIG_MNT} ] && umount ${CONFIG_MNT} > /dev/null 2>&1
[ -d ${ROOT1_MNT} ]  && umount ${ROOT1_MNT} > /dev/null 2>&1
[ -d ${ROOT2_MNT} ]  && umount ${ROOT2_MNT} > /dev/null 2>&1
[ ! -d ${LOG_MNT} ]    && mkdir -p ${LOG_MNT}
[ ! -d ${CONFIG_MNT} ] && mkdir -p ${CONFIG_MNT}
[ ! -d ${ROOT1_MNT} ]  && mkdir -p ${ROOT1_MNT}
[ ! -d ${ROOT2_MNT} ]  && mkdir -p ${ROOT2_MNT}
mount ${LOG_DEV}    ${LOG_MNT}
mount ${CONFIG_DEV} ${CONFIG_MNT}
mount ${ROOT1_DEV}  ${ROOT1_MNT}
mount ${ROOT2_DEV}  ${ROOT2_MNT}
if [ ! -f ${ROOT1_MNT}/etc/build_version.sh ] ; then
  echo Error installing.  Installation incomplete.
  echo Exiting to the shell to allow debugging.
  # Exiting with 11 makes root.profile go to the shell prompt.
  exit 11
fi
. ${ROOT1_MNT}/etc/build_version.sh

MFGLOG_DIR=${LOG_MNT}/mfglog
[ ! -d ${MFGLOG_DIR} ] && mkdir -p ${MFGLOG_DIR}

date > /tmp/step-post-inst.txt

if [ -f /tmp/mfg-fail.txt ] ; then
  cat /tmp/mfg-fail.txt
  Exit_Logout
fi

VXA_Eth_Setup()
{
  # Copy the ethlooptest binary from the installed location.
  # This is a 32bit binary so it can be run in this environment.
  F=${ROOT1_MNT}/opt/nkn/bin/ethlooptest
  if [ -f ${F} ] ; then
    rm -f /sbin/ethlooptest
    cp ${F} /sbin/
    chmod +x /sbin/ethlooptest
  else
    echo ERROR, no ${F}
  fi

  /sbin/vxa-eth-setup.sh > /tmp/vxa-eth-setup.log 2>&1
  RV=${?}
  if [ ${RV} -ne 0 ] ; then
    echo Error configuring ethernet ports. >> /tmp/mfg-fail.txt
    echo vxa-eth-setup.sh returned ${RV} >> ${MFGLOG_DIR}/vxa-eth-setup.log
    cat /tmp/vxa-eth-setup.log
    echo Installation failed.
  fi
  if [ -f /tmp/eth-order-eth.out ] ; then
    cp /tmp/eth-order* ${MFGLOG_DIR}/
  else
    echo Error, VXA ethernet ordering not configured. >> /tmp/mfg-fail.txt
  fi
  cp /tmp/vxa-eth-setup.log ${MFGLOG_DIR}/

  if [ ! -f /tmp/mfg-test.txt ] ; then
    echo Installation not done. >> /tmp/mfg-fail.txt
  fi
  if [ ! -f /tmp/imgverify.out ] ; then
    echo Installation failed. >> /tmp/mfg-fail.txt
  fi
  for i in disks-found.log mfg-fail.txt
  do
    [ ! -f ${i} ] && continue
    cp ${i} ${MFGLOG_DIR}/
  done

  if [ -f /tmp/mfg-fail.txt ] ; then
    cat /tmp/mfg-fail.txt
    Exit_Logout
  fi
}

# If root disk mirroring is used, then MBR needs to be same on both first and
# second drives. MBR is always written onto first drive, hence we copy the
# first 512 bytes of first drive onto 2nd drive using 'dd'.
Mirroring_MBR_Setup()
{
  [ ! -f /tmp/raid_disk_info.txt ] && return
  # Dot in /tmp/raid_disk_info.txt to get the D1_DEV and D2_DEV settings.
  . /tmp/raid_disk_info.txt
  # We don't have oflag=direct for dd in MFG environment, so we issue sync
  # just to be sure so that the contents are written back to drive.
  /bin/dd if=${D1_DEV} of=${D2_DEV} bs=512 count=1 > /dev/null 2>&1
  RV=${?}
  sync; sleep 2
  echo "Copied MBR from ${D1_DEV} to ${D2_DEV}"
  [ ${RV} -ne 0 ] && echo "return code is ${RV}"
  # Michael believes in 'sync;sync;sync' voodoo.
  sync;sync;sync; sleep 2
}

VXA_Done()
{
  echo ">>> Phase 1 Installation Done <<<"

  # Run the "Manufacturing Verification Test" printout script.
  /sbin/mvt-printout
  echo
  echo ">>>>> Install Process Complete <<<<<"
  echo
  READ_COUNT=0
  while true ; do
    # Just in case the stdin somehow is spewing, limit the number of loops.
    READ_COUNT=$(( ${READ_COUNT} + 1 ))
    [ ${READ_COUNT} -gt 200 ] && Exit_Logout
    echo Enter 1 to Reboot to complete the installation.
    echo Enter 2 to Halt.  Installation will continue when rebooted.
    echo Enter 3 to Flash ethernet port LEDs.
    echo Enter 4 to Print out the results of the manufacturing tests.
    echo
    echo -n "Enter 1, 2, 3, or 4 : "
    ANS=
    read ANS
    [ -z "${ANS}" ] && continue
    case "${ANS}" in
    0|exit|quit|q)
      # Exiting with 10 makes root.profile go to the shell prompt.
      # It also prints a message before that about using exit and reboot.
      Finish
      exit 10
      ;;
    1|reboot)
      Reboot
      ;;
    2|halt)
      Halt
      ;;
    3|eth*flash)
      # Use the "+" option do make it use the mappings that are going to be used.
      bash /sbin/ethflash 3 +
      ;;
    3\ *|eth*flash\ *)
      ITERATIONS=`echo ${ANS} | cut -f2 -d' '`
      case ${ITERATIONS} in
      [0-9]|[0-9][0-9]|[0-9][0-9][0-9]) ;;
      *) echo Invalid entry, not a valid iteration number.
        continue
        ;;
      esac
      # Use the "+" option do make it use the mappings that are going to be used.
      bash /sbin/ethflash ${ITERATIONS} +
      ;;
    4|mvt) /sbin/mvt-printout ;;
    eth*test*) /sbin/eth-test ;;
    esac
  done
  #NOTREACHED
  # Should never reach this point.  To be safe force an exit.
  Exit_Logout
}

No_Eth_Done()
{
  echo ">>> Phase 1 Installation Done <<<"
  echo
  READ_COUNT=0
  while true ; do
    # Just in case the stdin somehow is spewing, limit the number of loops.
    READ_COUNT=$(( ${READ_COUNT} + 1 ))
    [ ${READ_COUNT} -gt 200 ] && Exit_Logout
    echo
    echo Enter 0 to go to a shell prompt.
    echo Enter 1 to Reboot to complete the installation.
    echo Enter 2 to Halt.  Installation will continue when rebooted.
    echo
    echo -n "Enter 0, 1, or 2 : "
    ANS=
    read ANS
    [ -z "${ANS}" ] && continue
    case "${ANS}" in
    0|exit|quit|q)
      # Exiting with 10 makes root.profile go to the shell prompt.
      # It also prints a message before that about using exit and reboot.
      Finish
      exit 10
      ;;
    1|reboot)
      Reboot
      ;;
    2|halt)
      Halt
      ;;
    esac
  done
  #NOTREACHED
  # Should never reach this point.  To be safe force an exit.
  Exit_Logout
}

if [ -f /etc/vxa_model ] ; then
  VXA_Eth_Setup
fi

# Copy the flag files to /config/nkn/
for i in /etc/vm_cloudvm /etc/console_set /etc/firstboot_* /etc/ethreorder_*
do
  [ -f ${i} ] && cp ${i} ${CONFIG_MNT}/nkn
done

# Note: /tmp/grub_deferred.txt is set in writeimage_graft_5() in
#   nokeena/src/base_os/common/script_files/customer_rootflop.sh
if [ -f /tmp/grub_deferred.txt ] ; then
  /sbin/grub_setup_raid.sh
  RV=${?}
  if [ ${RV} -eq 0 ] ; then
    rm -f /tmp/grub_deferred.txt
  fi
fi
Mirroring_MBR_Setup
  
if [ -f /tmp/grub_install_failed.txt -o -f /tmp/grub_deferred.txt ] ; then
  echo
  if [ -f /tmp/grub_install_failed.txt ] ; then
    echo ERROR
    echo ERROR: `cat /tmp/grub_install_failed.txt`
    echo ERROR: The system will not boot.
    echo ERROR
  else
    echo Grub is NOT set up at this point.  That task was deferred.
    echo The system will not boot until grub is properly set up.
  fi
  echo
  while true ; do
    # Just in case the stdin somehow is spewing, limit the number of loops.
    READ_COUNT=$(( ${READ_COUNT} + 1 ))
    [ ${READ_COUNT} -gt 200 ] && Exit_Logout
    echo
    echo Enter 0 to Go to a shell prompt
    echo Enter 1 to Reboot to try to install again
    echo Enter 2 to Halt the machine
    echo Enter 3 to Contine with install sequence anyway.
    echo
    echo -n "Enter 0, 1, or 2 : "
    ANS=
    read ANS
    [ -z "${ANS}" ] && continue
    case "${ANS}" in
    0|exit|quit|q)
      # Exiting with 10 makes root.profile go to the shell prompt.
      # It also prints a message before that about using exit and reboot.
      Finish
      exit 10
      ;;
    1|reboot)
      Reboot
      ;;
    2|halt)
      Halt
      ;;
    3|cont)
      break
      ;;
    esac
  done
fi

if [ -f /etc/vxa_model ] ; then
  VXA_Done
  #NOTREACHED
fi

# The rest of the script is for NON-VXA.
echo ">>> Phase 1 Installation Done <<<"
echo

# The install-options.txt file the following (and others) from the boot command line:
IOP_INSTALL_OPT=
IOP_MAC_ADDR=
# See S30_mfc_install for format and explanations.
if [ -f /tmp/install-options.txt ] ; then
  . /tmp/install-options.txt
fi
ETHSETUP_OPT=
AUTO_ETH=no
AUTO_REBOOT=no
AUTO_HALT=no
for ITEM in `echo ${IOP_INSTALL_OPT} | sed 's/,/ /g'`
do
  case "${ITEM}" in
  ethsetup=*)
    ETHSETUP_OPT=`echo " ${ITEM}" | cut -f2 -d=` # installopt=ethsetup=...
    ;;
  auto-eth)
    AUTO_ETH=yes                                # installopt=auto-eth
    [ -z "${ETHSETUP_OPT}" ] && ETHSETUP_OPT=${IOP_MAC_ADDR}
    ;;
  auto-reboot)
    AUTO_REBOOT=yes                             # installopt=auto-reboot
    ;;
  auto-halt)
    AUTO_HALT=yes                               # installopt=auto-halt
    ;;
  esac
done
if [ -f /tmp/auto_reboot ] ; then
  AUTO_REBOOT=yes
elif [ -f /tmp/auto_halt ] ; then
  AUTO_HALT=yes
elif [ -f /tmp/no_auto_reboot ] ; then
  AUTO_REBOOT=no
elif [ -f /tmp/no_auto_halt ] ; then
  AUTO_HALT=no
fi

NEW_ORDER_FILE=/tmp/eth-order-new.out
if [ "${AUTO_ETH}" = "yes" -a ! -z "${ETHSETUP_OPT}" ] ; then
  bash /sbin/eth-setup-menu use ${ETHSETUP_OPT}
  # To make the new settings be used when the system is rebooted, the
  # settings need to be in /config/nkn/eth-order-use.list
  if [ -f ${NEW_ORDER_FILE} ] ; then
    cp ${NEW_ORDER_FILE} ${CONFIG_MNT}/nkn/eth-order-use.list
  fi
  if [ "${AUTO_REBOOT}" = "yes" ] ; then
    echo
    echo Reboot to complete the installation.
    sleep 5
    Reboot
  fi
  if [ "${AUTO_HALT}" = "yes" ] ; then
    echo
    echo Reboot to complete the installation.
    sleep 5
    Halt
  fi
fi

if [ "${AUTO_REBOOT}" = "yes" ] ; then
  echo
  if  [ -f /tmp/vm_info.txt ] ; then
    echo Reboot the VM to complete the installation.
  else
    echo Reboot the machine to complete the installation.
  fi
  sleep 5
  Reboot
fi
if [ "${AUTO_HALT}" = "yes" ] ; then
  echo
  if  [ -f /tmp/vm_info.txt ] ; then
    echo Reboot the VM to complete the installation.
  else
    echo Reboot the machine to complete the installation.
  fi
  sleep 5
  Halt
fi
if [ -f /etc/vm_cloudvm ] ; then
  # Normally with CloudVM installation auto-reboot already was done.
  # For debugging, when auto-reboot is disabled, do this here.
  No_Eth_Done
  #NOTREACHED
  # Should never reach this point.  To be safe force an exit.
  Exit_Logout
fi

READ_COUNT=0
while true ; do
  # Just in case the stdin somehow is spewing, limit the number of loops.
  READ_COUNT=$(( ${READ_COUNT} + 1 ))
  [ ${READ_COUNT} -gt 200 ] && Exit_Logout
  # To match the global usage, changed some of the numbers in this menu
  # so that we reserve '8' for reboot and '9' for halt,
  #   2 changed to 9   (halt)
  #   6 changed to 2   (reset)
  #   7 changed to 6   (PXE mac)
  #   8 changed to 7   (PXE spec)
  # This keeps the most likely used selections the same (1,3,4,5)
  echo Enter 1 to Reboot to complete the installation.
  N=2
  if [ "${AUTO_ETH}" = "no" ] ; then
    echo Enter 2 to Reset ethernet device naming back to default.
    echo Enter 3 to Flash ethernet port LEDs.
    echo Enter 4 to Configure ethernet device names interactively.
    echo Enter 5 to Specify devices to use for eth0 and eth1.
    N=6
    if [ ! -z "${IOP_MAC_ADDR}" ] ; then
      N=7
      echo Enter 6 to Use the PXE-boot interface for eth0.
      echo "        (${IOP_MAC_ADDR})"
      if [ ! -z "${ETHSETUP_OPT}" ] ; then
        N=8
        echo Enter 7 to Use the PXE-boot specification for eth0.
        echo "        (${ETHSETUP_OPT})"
      fi
    fi
  fi
  # 8 is reserved for Reboot (In this one case the same as #1)
  echo Enter 9 to Halt.  Installation will continue when rebooted.
  # echo Enter 0 to Exit to shell prompt.
  echo
  case ${N} in
  2) echo -n "Enter 1, 9 : " ;;
  6) echo -n "Enter 1, 2, 3, 4, 5, 9 : " ;;
  7) echo -n "Enter 1, 2, 3, 4, 5, 6, 9 : " ;;
  8) echo -n "Enter 1, 2, 3, 4, 5, 6, 7, 9 : " ;;
  esac
  ANS=
  read ANS
  [ -z "${ANS}" ] && continue
  case "${ANS}" in
  0|exit|quit|q)
    # Exiting with 10 makes root.profile go to the shell prompt.
    # It also prints a message before that about using exit and reboot.
    Finish
    exit 10
    ;;
  1|8|reboot)
    Reboot
    ;;
  9|halt)
    Halt
    ;;
  3|eth*flash)
    # Use the "+" option do make it use the mappings that are going to be used.
    bash /sbin/ethflash 3 +
    ;;
  3\ *|eth*flash\ *)
    ITERATIONS=`echo ${ANS} | cut -f2 -d' '`
    case ${ITERATIONS} in
    [0-9]|[0-9][0-9]|[0-9][0-9][0-9]) ;;
    *) echo Invalid entry, not a valid iteration number.
      continue
      ;;
    esac
    # Use the "+" option do make it use the mappings that are going to be used.
    bash /sbin/ethflash ${ITERATIONS} +
    ;;
  4|eth-setup-interactive)
    bash /sbin/eth-setup-interactive
    # To make the new settings be used when the system is rebooted, the
    # settings need to be in /config/nkn/eth-order-use.list
    if [ -f ${NEW_ORDER_FILE} ] ; then
      cp ${NEW_ORDER_FILE} ${CONFIG_MNT}/nkn/eth-order-use.list
    fi
    ;;
  5|eth-setup-menu)
    bash /sbin/eth-setup-menu menu
    # To make the new settings be used when the system is rebooted, the
    # settings need to be in /config/nkn/eth-order-use.list
    if [ -f ${NEW_ORDER_FILE} ] ; then
      cp ${NEW_ORDER_FILE} ${CONFIG_MNT}/nkn/eth-order-use.list
    fi
    ;;
  2|eth-setup-reset)
    rm -f ${NEW_ORDER_FILE}
    rm -f ${CONFIG_MNT}/nkn/eth-order-use.list
    echo
    echo Done
    echo
    ;;
  6|eth-setup-pxe-mac)
    bash /sbin/eth-setup-menu use ${IOP_MAC_ADDR}
    # To make the new settings be used when the system is rebooted, the
    # settings need to be in /config/nkn/eth-order-use.list
    if [ -f ${NEW_ORDER_FILE} ] ; then
      cp ${NEW_ORDER_FILE} ${CONFIG_MNT}/nkn/eth-order-use.list
    fi
    ;;
  7|eth-setup-pxe-spec)
    bash /sbin/eth-setup-menu use ${ETHSETUP_OPT}
    # To make the new settings be used when the system is rebooted, the
    # settings need to be in /config/nkn/eth-order-use.list
    if [ -f ${NEW_ORDER_FILE} ] ; then
      cp ${NEW_ORDER_FILE} ${CONFIG_MNT}/nkn/eth-order-use.list
    fi
    ;;
  esac
done
#NOTREACHED
# Should never reach this point.  To be safe force an exit.
Exit_Logout
