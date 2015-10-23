:
# Start the installation.  This can be run from root .profile
# Exit with 22 when this script was run from another TTY.
#
# NOTE: /tmp/img.filename
# The script /sbin/overlay-manuf.sh checks for installation media.
# If found, it creates /tmp/img.filename and installs all the files
# from the manufacturing tgz file (such as /sbin/pre-install.sh)
#
# NOTE: /tmp/img.url
# The file /tmp/img.url is created by the pxe-install script.
# The pxe-install script also installs all the files from the
# manufacturing tgz file (such as /sbin/pre-install.sh)

MY_TTY=`tty`
if [ -f /tmp/install-running-on ] ; then
  RUNNING_ON=`cat /tmp/install-running-on`
  if [ "${MY_TTY}" != "${RUNNING_ON}" ] ; then
    echo The installation is running on another terminal: `cat /tmp/install-running-on`
    exit 22
  fi
fi
# Note: We only create the flag file /tmp/install-running-on
# when the user has entered a response to a prompt.
# This is because this script can be run twice, potentially
# at the same time, because this is run automatically on the
# vga console which is automatically logged in, and on
# the serial console when the user logs in as root.
# So until that point, be careful because it can be
# being run at the same time from two consoles.


EXITING=no
STOP_FILE=/tmp/stop.$$.tmp
trap_int_handler()
{
  [ "${EXITING}" = "yes" ] && exit 1
  echo
  echo "(Interrupt)"
  echo
  touch ${STOP_FILE}
}
trap "trap_int_handler" INT

Exit_Logout()
{
  EXITING=yes
  rm -f ${STOP_FILE}
  trap - INT
  # Exit with 0 so the user gets a login prompt.
  # If there have been too many logins we might be having a problem,
  # so in that case exit with 10 so it goes to the shell prompt.
  [ ${LCNT} -gt 4 ] && echo ${LCNT}
  [ ${LCNT} -lt 10 ] && exit 0
  exit 10
}

Exit_To_Shell()
{
  EXITING=yes
  rm -f ${STOP_FILE}
  trap - INT
  # Exit with 10 so it goes to the shell prompt.
  exit 10
}

READ_COUNT=0
Reboot_or_Halt()
{
  while true ; do
    [ -f ${STOP_FILE} ] && Exit_To_Shell
    # Just in case the stdin somehow is spewing, limit the number of loops.
    READ_COUNT=$(( ${READ_COUNT} + 1 ))
    [ ${READ_COUNT} -gt 100 ] && Exit_To_Shell
    # 1 is used for the most likely choice.
    # 8 is reserved from reboot and 9 is reserved for halt.
    echo Enter 0 to go to a shell prompt.
    echo Enter 1 to Reboot without installing or restoring.
    echo Enter 9 to Halt without installing or restoring.
    echo -n "Enter 0, 1, 9 : "
    read ANS
    [ -f ${STOP_FILE} ] && continue
    [ -z "${ANS}" ] && continue
    case ${ANS} in
    0|exit|quit|q)
      # Break out into the shell
      Exit_To_Shell
      ;;
    1|8|reboot)
      echo "Rebooting ..."
      # Suppress all kernel messages to the console
      killall klogd
      # Kill syslogd because it can get a segfault when system shutting down.
      killall syslogd
      sleep 2
      reboot
      exit 0
      ;;
    9|halt)
      echo "Halting the system ..."
      # Suppress all kernel messages to the console
      killall klogd
      # Kill syslogd because it can get a segfault when system shutting down.
      killall syslogd
      sleep 2
      halt
      exit 0
      ;;
    esac
  done
}

[ ! -f /tmp/login-counter.txt ] && echo 0 > /tmp/login-counter.txt
LCNT=`cat /tmp/login-counter.txt`
LCNT=$(( ${LCNT} + 1))
echo ${LCNT} > /tmp/login-counter.txt

# The install-options.txt file sets the following from the boot command line:
IOP_INSTALL_FROM=
IOP_INSTALL_OPT=
IOP_MAC_ADDR=
IOP_ROOT_IMAGE=
IOP_MY_IP=
IOP_SERVER_IP=
IOP_GW_IP=
IOP_NET_MASK=
IOP_DEFAULT_URL=
# See S30_mfc_install for format and explanations.

if [ -f /tmp/install-options.txt ] ; then
  . /tmp/install-options.txt
  echo ; echo ; echo ; echo ; echo ; echo ; echo ; echo ; echo ; echo ; echo
  cat /tmp/install-options.txt
fi
SHOW_PXE_MENU=no
if [ ! -z "${IOP_INSTALL_OPT}" ] ; then
  for ITEM in `echo "${IOP_INSTALL_OPT}" | sed 's/,/ /g'`
  do
    [ "${ITEM}" = "show-pxe-menu" ] &&  SHOW_PXE_MENU=yes
  done
  if [ ! -z "${IOP_DEFAULT_URL}" ] ; then
    # SHOW_PXE_MENU == no  means to do the normal PXE install. (default)
    # SHOW_PXE_MENU == yes means to show the menu that allows
    #                      picking another URL to install from.
    [ "${SHOW_PXE_MENU}" = "yes" ] && echo SHOW_PXE_MENU=yes
  fi
fi

echo ; echo ; echo ; echo ; echo ; echo ; echo ; echo ; echo ; echo ; echo
stty onlcr
clear
echo "        Welcome to Juniper Networks Media Flow Controller Installation"
echo
if [ ! -f /tmp/img.url -a ! -f /tmp/img.filename ] ; then
  /sbin/overlay-manuf.sh
  RV=${?}
  if [ ${RV} -eq 2 ] ; then
    Reboot_or_Halt
    #NOTREACHED
  fi
fi
# The file /tmp/img.url is created by the pxe-install script.
[ -f /tmp/img.url ] && echo img.url `cat /tmp/img.url`
[ -f /tmp/img.filename ] && echo img.filename `cat /tmp/img.filename`

# Do the first prompt when doing PXE installation.
# This is also used in the situation where booted from install CD but
# the CD device is not avaliable immediately. (Some VMs and shared
# CD devices might operate like this.)
# The reason we need a prompt this is that this script might be run
# automatically on a terminal that no user is actually using.
# So this prevents such situations from going any further.
#
# SKIP the first prompt when any one of these situations:
# 1: Doing 'show-pxe-menu' PXE install, because we want to first use the
#    prompt that allows choosing a different URL to install from.
# 2: PXE URL is already determined (/tmp/img.url exists).
# 3. Booted from CD or USB and the install media was found (/tmp/img.filename exists).

SKIP_FIRST=no
rm -f /tmp/no-eth-device.txt
if [ ! -z "${IOP_DEFAULT_URL}" -a "${SHOW_PXE_MENU}" = "no" -a ! -f /tmp/img.url ] ; then
  # PXE install menu 1
  READ_COUNT=0
  while true ; do
    [ -f ${STOP_FILE} ] && Exit_To_Shell
    # Just in case the stdin somehow is spewing, limit the number of loops.
    READ_COUNT=$(( ${READ_COUNT} + 1 ))
    [ ${READ_COUNT} -gt 100 ] && Exit_To_Shell
    if [ -f /tmp/no-eth-device.txt ] ; then
      Reboot_or_Halt
      #NOTREACHED
    fi
    echo Enter 0 to go to a shell prompt.
    echo Enter 1 to Start the installation from PXE.
    # Note, in the past '3' meant the same as '1' is now, so later
    # treat both the same.
    echo Enter 8 to Reboot without installing or restoring.
    echo Enter 9 to Halt without installing or restoring.
    echo -n "Enter 0, 1, 8, 9 : "
    read ANS
    if [ -f /tmp/install-running-on ] ; then
      RUNNING_ON=`cat /tmp/install-running-on`
      if [ "${MY_TTY}" != "${RUNNING_ON}" ] ; then
        echo
        echo The installation is running on another terminal: `cat /tmp/install-running-on`
        echo
        sleep 5
        EXITING=yes
        rm -f ${STOP_FILE}
        trap - INT
        exit 22
      fi
    fi
    [ -f ${STOP_FILE} ] && continue
    [ -z "${ANS}" ] && continue
    case ${ANS} in
    0|exit|quit|q)
      # Break out into the shell
      Exit_To_Shell
      ;;
    8|reboot)
      echo "Rebooting ..."
      # Suppress all kernel messages to the console
      killall klogd
      # Kill syslogd because it can get a segfault when system shutting down.
      killall syslogd
      sleep 2
      reboot
      exit 0
      ;;
    9|halt)
      echo "Halting the system ..."
      # Suppress all kernel messages to the console
      killall klogd
      # Kill syslogd because it can get a segfault when system shutting down.
      killall syslogd
      sleep 2
      halt
      exit 0
      ;;
    1|3)
        if [ "${SHOW_PXE_MENU}" = "no" ] ; then
          echo ${MY_TTY} > /tmp/install-running-on
          /sbin/pxe-install --automatic
          RV=${?}
          if [ ${RV} -eq 2 ] ; then
            # Hard failure, no point to continue.
            Reboot_or_Halt
            #NOTREACHED
          fi
          # If pxe-install was successful then /tmp/img.url now exists. 
          break
        fi
      ;;
  esac
  done
fi
# --------------------------------------------------------
if [ ! -z "${IOP_DEFAULT_URL}" -a ! -f /tmp/img.url ] ; then
  # PXE install menu 2 -- Allow user to specify alternate URL, and then
  #                       run pxe-install without the "--automatic" option.
  #
  # We get here if the previous "/sbin/pxe-install --automatic" to create /tmp/img.url.
  # We also get here when 'SHOW_PXE_MENU=yes' because then pxe-install was not run yet.
  # Thus this code is not normally used, because show-pxe-menu is not normally used
  # and "pxe-install --automatic" should not fail.
  READ_COUNT=0
  while true ; do
    [ -f ${STOP_FILE} ] && break
    # Just in case the stdin somehow is spewing, limit the number of loops.
    READ_COUNT=$(( ${READ_COUNT} + 1 ))
    [ ${READ_COUNT} -gt 100 ] && break
    if [ -f /tmp/no-eth-device.txt ] ; then
      Reboot_or_Halt
      #NOTREACHED
    fi
    if [ ! -z "${IOP_DEFAULT_URL}" ] ; then
      echo "Default URL: ${IOP_DEFAULT_URL}"
      echo Specify the URL to install from, or
      echo 0 to go to a shell prompt
      echo 1 to Install from the default URL
      echo 8 to Reboot without installing or restoring
      echo 9 to Halt without installing or restoring
      echo
      echo -n "Enter 0, 1, 8, 9 or the URL and press return : "
    else
      echo Installation media not found.
      echo Specify the URL to install from, or
      echo 0 to go to a shell prompt
      echo 8 to Reboot without installing or restoring
      echo 9 to Halt without installing or restoring
      echo
      echo -n "Enter 0, 8, 9 or the URL and press return : "
    fi
    read ANS
    [ -f ${STOP_FILE} ] && continue
    [ -z "${ANS}" ] && continue
    case ${ANS} in
    0|exit|quit|q)
      # Break out into the shell
      Exit_To_Shell
      ;;
    8|reboot)
      echo "Rebooting ..."
      # Suppress all kernel messages to the console
      killall klogd
      # Kill syslogd because it can get a segfault when system shutting down.
      killall syslogd
      sleep 2
      reboot
      exit 0
      ;;
    9|halt)
      echo "Halting the system ..."
      # Suppress all kernel messages to the console
      killall klogd
      # Kill syslogd because it can get a segfault when system shutting down.
      killall syslogd
      sleep 2
      halt
      exit 0
      ;;
    1|3)
      # NOTE: '3' used to be the choice for Interactive installing from default URL,
      # so still accept '3' as '1'.
      ANS="${IOP_DEFAULT_URL}"
      ;;
    esac
    # The user specified a URL to install from.
    # Now starting the first step in the install process.
    echo ${MY_TTY} > /tmp/install-running-on

    # When the pxe-install script is successful it returns 0 and
    # created /tmp/img.url (both or neither).
    echo pxe-install "${ANS}"
    pxe-install --quiet "${ANS}"
    RV=${?}
    if [ ${RV} -eq 0 ] ; then
      echo
     echo Ready to install from ${ANS}
      echo
      break
    fi
    if [ ${RV} -eq 2 ] ; then
      Reboot_or_Halt
      #NOTREACHED
    fi
    echo
    echo ================================================
    echo Try again
    echo ================================================
    echo
  done
  [ -f ${STOP_FILE} ] && Exit_Logout
  [ ${READ_COUNT} -gt 100 ] && Exit_Logout
fi

# Now take care of the non-PXE case.
if [ -z "${IOP_DEFAULT_URL}" -a ! -f /tmp/img.filename ] ; then
  # NON-PXE menu 1
  READ_COUNT=0
  while true ; do
    [ -f ${STOP_FILE} ] && Exit_To_Shell
    # Just in case the stdin somehow is spewing, limit the number of loops.
    READ_COUNT=$(( ${READ_COUNT} + 1 ))
    [ ${READ_COUNT} -gt 100 ] && Exit_To_Shell
    if [ -f /tmp/no-eth-device.txt ] ; then
      Reboot_or_Halt
      #NOTREACHED
    fi
    echo Enter 0 to go to a shell prompt.
    echo Enter 1 to Attempt to install from Installation Media.
    # Note, in the past '3' meant the same as '1' is now, so later
    # treat both the same.
    echo Enter 8 to Reboot without installing or restoring.
    echo Enter 9 to Halt without installing or restoring.
    echo -n "Enter 0, 1, 8, 9 : "
    read ANS
    if [ -f /tmp/install-running-on ] ; then
      RUNNING_ON=`cat /tmp/install-running-on`
      if [ "${MY_TTY}" != "${RUNNING_ON}" ] ; then
        echo
        echo The installation is running on another terminal: `cat /tmp/install-running-on`
        echo
        sleep 5
        EXITING=yes
        rm -f ${STOP_FILE}
        trap - INT
        exit 22
      fi
    fi
    [ -f ${STOP_FILE} ] && continue
    [ -z "${ANS}" ] && continue
    case ${ANS} in
    0|exit|quit|q)
      # Break out into the shell
      Exit_To_Shell
      ;;
    8|reboot)
      echo "Rebooting ..."
      # Suppress all kernel messages to the console
      killall klogd
      # Kill syslogd because it can get a segfault when system shutting down.
      killall syslogd
      sleep 2
      reboot
      exit 0
      ;;
    9|halt)
      echo "Halting the system ..."
      # Suppress all kernel messages to the console
      killall klogd
      # Kill syslogd because it can get a segfault when system shutting down.
      killall syslogd
      sleep 2
      halt
      exit 0
      ;;
    1|3|cdrom|pxe)
        echo "Checking for Installation Media ..."
        /sbin/overlay-manuf.sh
        RV=${?}
        if [ ${RV} -eq 2 ] ; then
          Reboot_or_Halt
          #NOTREACHED
        fi
        echo
        sleep 2
        if [ -f /tmp/img.filename ] ; then
          echo ${MY_TTY} > /tmp/install-running-on
          echo Found Installation Media
          echo
          sleep 3
          break
        fi
        echo Installation Media not found
        echo
        sleep 3
        continue
      ;;
    esac
  done
fi

# At this point there is always supposed to be a pre-install script
if [ ! -f /sbin/pre-install.sh ] ; then
  echo
  echo Some error occurred.  Not installing.
  echo Missing pre-install script.
  sleep 5
  Exit_To_Shell
fi
# The pre-install.sh script runs install-mfc which in turn runs post-install.sh
/bin/bash /sbin/pre-install.sh
RV=${?}

# Normally we never reach this point because the user is only presented
# with the choice of rebooting or halting.
# So something abnormal happened or a ^C or an undocumented option was
# used for the purpose of getting to this point.
# Exit codes:
# 0-8 prints out "Install returned <number>." and then go
# to the login prompt.
# 9 just goes to the login prompt without printing anything.
# 10 makes it go to the shell after printing helpful message.
# Above 10 just goes to the shell without printing anything.

if [ ${RV} -lt 10 ] ; then
  # Just in case the machine is shutting down and the install script was
  # killed as part of that and has an error code in this range, wait a bit
  # before printing the error message, to give time for this script to
  # be killed, so the user is not confused by an error message when
  # rebooting or halting.
  sleep 3
  [ ${RV} -ne 9 ] && echo Install returned ${RV}.
  # This exit will exit the login shell, so the user gets a login prompt.
  Exit_Logout
fi
# If we get here we are to start up the normal shell.
trap - INT
rm -f ${STOP_FILE}
exit ${RV}
