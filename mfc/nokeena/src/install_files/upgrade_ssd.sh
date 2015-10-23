#! /bin/bash
#
# Upgrade Intel SSD Drives with old firmware (before 8860)
#
# If any drives are actually upgraded, the system must be rebooted.
#
#
#

export LD_LIBRARY_PATH=/usr/StorMan

# Validate that this system is a VXA system.
if [ ! -e /etc/vxa_model ]; then
  logger -s "Command only relevant on VXA hardware." 2>&1 | sed -e 's/admin: //g' -e 's/logger: //g'
  exit
fi


OUT="/nkn/tmp/arcconf.getconfig.pd."
REBOOT=0
SHOULD_UPGRADE="NO"
rm -f ${OUT}*
/usr/StorMan/arcconf getconfig 1 pd | csplit -s -f ${OUT} -k -z - "/Device #/" {20} > /dev/null 2>&1

for DR in /sys/block/sd*; do
  base=`basename $DR`
  MODEL=`/opt/nkn/bin/smartctl -d sat -i /dev/${base} | grep "Device Model" | sed 's/Device Model://g'`

  VENDOR=`echo ${MODEL} | awk '{print $2}'`
  # Check that we see "INTEL" string
  if [ "$VENDOR" != "INTEL" ]; then
    continue
  fi

  # Check that first 8 characters match SSD 2.5" X25-E drive
  T=`echo ${MODEL} | awk '{print $1}' | cut -c -8`
  if [ "$T" != "SSDSA2SH" ]; then
    continue
  fi

  # Check that we have a 1st gen X25-E drive
  T=`echo ${MODEL} | awk '{print $1}' | cut -c 13-13`
  if [ "$T" != "1" ]; then
    continue
  fi

  # Check that we have downrev firmware
  FIRMWARE=`/opt/nkn/bin/smartctl -d sat -i /dev/${base} | grep "Firmware Version:" | sed 's/Firmware Version://g'`
  T=`echo ${FIRMWARE} | cut -c 5-8`
  if [ "$T" -ge "8860" ]; then
    continue
  fi

  if ( grep -q /dev/${base} /proc/mounts ); then
    # We don't want to upgrade drives if they are mounted
    DEV=`grep /dev/${base} /proc/mounts|awk '{print $2}'|sed 's@/nkn/mnt/@@g'`
    /usr/bin/logger -s "* Need to upgrade SSD X25-E drive (${base} ${SERIAL_NUM}) with downrev firmware (${T}) to more current firmware (8860), but the device ($DEV) is not in 'status inactive' state." 2>&1 | sed -e 's/admin: //g' -e 's/logger: //g'
    SHOULD_UPGRADE="YES"
    continue
  fi
  SERIAL_NUM=`/opt/nkn/bin/smartctl -d sat -i /dev/${base} | grep "Serial Number:" | sed 's/Serial Number://g' | awk '{print $1}'`
  /usr/bin/logger -s "* Attempting to upgrade SSD X25-E drive (${base} ${SERIAL_NUM}) with downrev firmware (${T}) to more current firmware (8860)" 2>&1 | sed -e 's/admin: //g' -e 's/logger: //g'

  F=`grep -l ${SERIAL_NUM} ${OUT}*`
  if [ $? -eq 0 ]; then

    CHANNEL=`grep -i "Channel,Device" $F | awk '{print $4}' | sed -e 's/(.*)//g' -e 's/,.*//'`
    ID=`grep -i "Channel,Device" $F | awk '{print $4}' | sed -e 's/(.*)//g' -e 's/.*,//'`

    # Only support 1 Adaptec controller for now
    /usr/StorMan/arcconf imageupdate 1 DEVICE ${CHANNEL} ${ID} 256000 /lib/firmware/rel8860.bin 7 noprompt >& /nkn/tmp/arcconf.imageupdate.${SERIAL_NUM}.${CHANNEL}.${ID}
    if [ $? -eq 0 ]; then
      REBOOT=1
    else
      logger -s "*   Upgrade SSD X25-E drive (${base} ${SERIAL_NUM}) to more current firmware (8860): Failed" 2>&1 | sed -e 's/admin: //g' -e 's/logger: //g'
      SHOULD_UPGRADE="YES"
    fi
  fi
done

if [ $REBOOT -eq 1 ]; then
  logger -s "This system needs to be rebooted before the upgraded SSD drives will be visible." 2>&1 | sed -e 's/admin: //g' -e 's/logger: //g'
elif [ "$SHOULD_UPGRADE" = "YES" ]; then
  logger -s "Some drives failed to upgrade." 2>&1 | sed -e 's/admin: //g' -e 's/logger: //g'
else
  logger -s "All discovered X-25E SSD drives have up-to-date firmware." 2>&1 | sed -e 's/admin: //g' -e 's/logger: //g'
fi
