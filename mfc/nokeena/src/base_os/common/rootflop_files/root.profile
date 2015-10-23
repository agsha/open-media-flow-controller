# Root user .profile for the MFC installation environment.
# This is /root/.profile, used when root logs in on the serial console.
#

FIRST=no
echo ${$}:`tty`:`date` >> /tmp/rootprofile.log
if [ ! -f /tmp/first.detect ] ; then
  echo :${$}: > /tmp/first.detect
  sleep 1
  R=`echo ${RANDOM} | cut -c1`
  sleep ${R}
  grep :${$}: /tmp/first.detect > /dev/null
  if [ ${?} -eq 0 ] ; then
    echo First is ${$} >> /tmp/rootprofile.log
    FIRST=yes
  else
    echo Not first is ${$} >> /tmp/rootprofile.log
  fi
else
  echo Later is ${$} >> /tmp/rootprofile.log
fi
if [ "${FIRST}" = "yes" ] ; then
  echo ${$}:`tty`:`date` >> /tmp/first.log
  ps  >> /tmp/first.log
  set >> /tmp/first.log
  who >> /tmp/first.log
  ifconfig >> /tmp/first.log
else
  echo ${$}:`tty`:`date` >> /tmp/notfirst.log
  ps  >> /tmp/notfirst.log
  set >> /tmp/notfirst.log
  who >> /tmp/notfirst.log
  ifconfig >> /tmp/notfirst.log
fi

# First see if an unattended install is being done.
if [ -f /tmp/unattended-install ] ; then
  echo .
  echo "Automatic MFC installation in progress"
  echo
  if [ -x /sbin/show-install-progress.sh ] ; then
    # Run the program that shows the install progress.
    /sbin/show-install-progress.sh
    echo .
    echo Starting shell prompt.  Exit to again view installation progress.
  else
    echo Starting shell prompt.  For diagnostic use only.
  fi
  bash
  exit 9
else
  # Start or resume the interactive installation.
  echo ${$}:Running /sbin/interactive-install.sh >> /tmp/rootprofile.log
  /sbin/interactive-install.sh
  RV=${?}
  case ${RV} in
  [0-8])
    # This exit will exit the login shell so the user gets a login prompt.
    echo .
    echo "Login as root to install MFC"
    echo
    exit ${RV}
    ;;
  9)
    # This exit will exit the login shell so the user gets a login prompt.
    exit 9
    ;;
  10) # Go_To_Shell_Message
    echo
    echo Use the exit command to start the installation over.
    echo Use the reboot command to reboot the machine.
    echo Use the halt command to shut down the machine.
    echo
    ;;
  22)
    /sbin/show-install-progress.sh
    ;;
  esac
fi
