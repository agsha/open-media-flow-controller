:
# This is a script to update the console setting in /bootmgr/boot/grub/grub.conf 

# Notes about grub.conf command lines:

# On the "terminal" command line the parameters "serial" and/or "console"
# can be specified.
# When both are specified there will be a prompt on the serial and on the vga
# console asking for a key to be pressed until the timeout expires.
# If a key is pressed before the timeout then the boot menu is displayed on
# that device, but not the other.
# If a key is not pressed before the timeout then the boot menu is displayed
# on which ever of "serial" or "console" is first on the line.
# After the boot menu is displayed the command "timeout" specifies how long
# to wait before before automatically booting, and the command "default"
# specifies which entry to use to boot.  (0 is the first)
# The entries are indicated by the "title" command and the lines following
# the "title" command line specifies which filesystem to boot, and the kernel
# command line parameters.


# You can specify multiple console= options on the kernel command line.
# The order is very important.
# See https://www.kernel.org/doc/Documentation/serial-console.txt
# Output will appear on all devices specified by the console= option.
# The *last* device will be used when you open /dev/console. So, for example:
# 
# 	console=ttyS0,9600n8 console=tty0
# 
# specifies that opening /dev/console will get you the current foreground virtual
# console (on the VGA console), and kernel messages will appear on both the VGA
# console and the 1st serial port (ttyS0 or COM1) at 9600 baud using 8 bits.
# If you want the serial line to be used as the console device then use:
#
#       console=tty0 console=ttyS0,9600n8
#

# This script changes the grub settings related the serial/vga console.
# The single command line parameter must be either "vga" or "serial".

case "_${1}" in
_vga|_serial) OPT=${1}; break ;;
_*) echo "Specify either vga or serial" ; exit 1 ;;
esac

if [ "${OPT}" = "vga" ] ; then
  T_FR="serial console"
  T_TO="console serial"
  K_FR="console=tty0 console=ttyS0,9600n8"
  K_TO="console=ttyS0,9600n8 console=tty0"
else
  T_FR="console serial"
  T_TO="serial console"
  K_FR="console=ttyS0,9600n8 console=tty0"
  K_TO="console=tty0 console=ttyS0,9600n8"
fi

date
COUNT=0
for ORIG_FILE in /bootmgr/boot/grub/grub.conf /etc/grub.conf.template /etc/grub.conf.template.new
do
  echo ${ORIG_FILE}
  COUNT=`expr ${COUNT} + 1`
  U_FILE=/tmp/grub.${COUNT}.updated
  rm -f ${U_FILE}
  #O_FILE=/tmp/grub.${COUNT}.orig
  cat ${ORIG_FILE} \
    | sed -e "s/${T_FR}/${T_TO}/" \
    | sed -e "s/${K_FR}/${K_TO}/" \
    > ${U_FILE}

  cmp -s ${U_FILE} ${ORIG_FILE}
  if [ ${?} -eq 0 ] ; then
    # No change.
    echo No change needed to ${ORIG_FILE}
    grep console ${ORIG_FILE}
    continue
  fi
  echo ${ORIG_FILE} updated
  echo "# Updated ${ORIG_FILE} `date`" >> ${U_FILE}
  ls -l ${ORIG_FILE} ${U_FILE}
  sum ${ORIG_FILE}   ${U_FILE}
  diff ${ORIG_FILE}  ${U_FILE}

  D1=`echo ${ORIG_FILE} | cut -f1-2 -d/`
  case ${D1} in
  /bootmgr) MP=/bootmgr ;;
  /etc)     MP=/        ;;
  esac
  # First make filesystem writeable
  mount -o remount,rw ${MP}
  RV=${?}
  if [ ${RV} -ne 0 ] ; then
    echo Error: Failed to make ${MP} writable, not updating grub.conf
    continue
  fi
  #rm -f ${O_FILE}
  #cp -a ${ORIG_FILE} ${O_FILE}
  chmod 666 ${ORIG_FILE}
  cat ${U_FILE} > ${ORIG_FILE}
  rm -f ${U_FILE}
  chmod 600 ${ORIG_FILE}
  ls -l ${ORIG_FILE}
  sum ${ORIG_FILE}
  # Make filesystem read-only again.
  mount -o remount,ro ${MP}
  RV=${?}
  if [ ${RV} -ne 0 ] ; then
    echo Warning: Failed to make ${MP} readonly
    continue
  fi
done

