:
# Start the automatic installation.
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


# If the image file or url is not there, then it might be pxe install.
# This situation also happens when booted from install CD but
# the CD device is not avaliable immediately. (Some VMs and shared
# CD devices might operate like this.)
if [ ! -f /tmp/img.url -a ! -f /tmp/img.filename ] ; then
  echo See if pxe by doing: pxe-install --automatic
  /sbin/pxe-install --automatic 2>&1
  if [ ! -f /tmp/img.url ] ; then
    # Not pxe, so do a delay and retry the overlay-manuf.sh for
    # the situation where the mounting of the CD is delayed.
    sleep 30
    echo "doing: /sbin/overlay-manuf.sh"
    /sbin/overlay-manuf.sh 2>&1
  fi
  date
fi

if [ ! -f /tmp/img.url -a ! -f /tmp/img.filename ] ; then
  echo Error, no img.url or img.filename
  exit 2
fi
echo "doing: pre-install --automatic ${*}"
/bin/bash /sbin/pre-install.sh --automatic ${*}
# When installation success normally should not return.
