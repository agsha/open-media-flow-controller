#!/bin/bash

#       File: mirror_status.sh
#
#       Description: The script provides the status of mirrored disks.
#	This script is run when cli command "show system mirror boot-device 
# 	status" or "show system mirror boot-device status detail" is issued.
#
#       Created On : 25 June, 2012
#
#       Copyright (c) Juniper Networks Inc., 2012
#

# The logic is to determine the status of all the mirrors and
# provide single update to end user.

# Set the Variables
MDADM=/sbin/mdadm
MDSTAT=/proc/mdstat
FINDSLOT=/opt/nkn/generic_cli_bin/findslottodcno.sh
SFDISK=/sbin/sfdisk
FDISK=/sbin/fdisk
RAID_DEV=/dev/md
MDDETAIL=/nkn/tmp/mddetail.txt
# System info file that is used for "detail" option.
MDINFO=/nkn/tmp/mdinfo.txt
TMP=/nkn/tmp

if [ ! -e /opt/nkn/bin/disks_common.sh ]; then
  logger -s "mirror_status.sh: Missing library file"
  exit 1;
fi
source /opt/nkn/bin/disks_common.sh

# Check if system is VXA 2100 or not
if ! is_vxa_2100 ; then
  echo "Mirroring is not supported on this platform"
  echo "Mirroring is only supported on VXA 2100"
  exit 2
fi

# check if mirroring is used or not

grep -q "/dev/md" /config/nkn/boot-disk.info  > /dev/null 2>&1
RETCODE=$?
if [ $RETCODE -eq 1 ]; then
  echo "Mirroring is not configured on this system"
  exit 0
fi

# Check if the script is called in verbose mode ("detail" in CLI)
#echo "Argument 1 is $1"
SHIFTCNT=0
while getopts "v" opt; do
  case $opt in
    v ) ISVERBOSE="yes"
        (( SHIFTCNT++ ))
        ;;
  esac
  (( SHIFTCNT++ ))
done

# Gather the details for all the partitions in case user wants to
# get verbose details.
/bin/rm -f $MDDETAIL*
/bin/rm -f $MDINFO*
echo "Mirrored: Yes" >>$MDINFO
echo "Partition TotSize UsedSize AvailSize State ReSync" >>$MDINFO
TOTALSIZE="0"
# Loop for all boot device mirrored partitions and gather the required info.
# If the boot device mirror configuration changes, this needs to be modified
# as well.
for PNUM in 1 2 3 5 6 7 8 9 10 12; do
  R=${RAID_DEV}${PNUM}
  $MDADM --detail $R >> $MDDETAIL.${PNUM}
  RETCODE=$?
  if [ $RETCODE -eq 0 ]; then
    RESYNC=""
    SIZE=`grep " Array " $MDDETAIL.${PNUM} | awk '{print $7,$8}' | cut -f1 -d")"`
    BYTES=`$FDISK -l $R 2>/dev/null | grep "Disk " | awk '{print $5}'`
    TOTALSIZE=$(($TOTALSIZE+$BYTES))
    USEDSIZE=`df -kh | grep "$R " |  awk '{print $3}'`
    AVAILSIZE=`df -kh | grep "$R " |  awk '{print $4}'`
    ISMOUNTED=`grep -q "$R " /proc/mounts > /dev/null 2>&1`
    RETCODE=$?
    if [ $RETCODE -eq 1 ]; then
      USEDSIZE="N/A"
      AVAILSIZE="N/A"
    fi
    MDSTATE=`grep "State :" $MDDETAIL.${PNUM} | awk '{print $3,$4,$5,$6}'`
    csplit -s -z -k -f $TMP/md_ $MDSTAT %"md"% /"md"/ {10}   > /dev/null 2>&1
    case $PNUM in 
    1|2|3) 
      MDNO=$(($PNUM-1))
      grep -q "re" $TMP/md_0$MDNO > /dev/null 2>&1
      RETCODE=$?
      if [ $RETCODE -eq 0 ]; then
        grep -q finish $TMP/md_0$MDNO > /dev/null 2>&1
        RETCODE=$?
        if [ $RETCODE -eq 0 ]; then
          RESYNC=`grep -i "re" $TMP/md_0$MDNO | awk '{print $4,$5,$6,$7}'`
        else
	  RESYNC="Delayed"
        fi
      else
	  RESYNC="N/A"
      fi 
      ;;
    5|6|7|8|9|10)
      MDNO=$(($PNUM-2))
      grep -q "re" $TMP/md_0$MDNO > /dev/null 2>&1
      RETCODE=$?
      if [ $RETCODE -eq 0 ]; then
        grep -q finish $TMP/md_0$MDNO > /dev/null 2>&1
        RETCODE=$?
        if [ $RETCODE -eq 0 ]; then
          RESYNC=`grep -i "re" $TMP/md_0$MDNO | awk '{print $4,$5,$6,$7}'`
        else
	  RESYNC="Delayed"
        fi
      else
	  RESYNC="N/A"
      fi
      ;;
    12)
      grep -q "re" $TMP/md_09 > /dev/null 2>&1
      RETCODE=$?
      if [ $RETCODE -eq 0 ]; then
        grep -q finish $TMP/md_09 > /dev/null 2>&1
        RETCODE=$?
        if [ $RETCODE -eq 0 ]; then
          RESYNC=`grep -i "re" $TMP/md_09  | awk '{print $4,$5,$6,$7}'`
        else
	  RESYNC="Delayed"
        fi
      else
	  RESYNC="N/A"
      fi
      ;;
    esac
    echo "Partition"$PNUM $SIZE $USEDSIZE $AVAILSIZE $MDSTATE $RESYNC >> $MDINFO
  fi
done
#Convert total bytes into GB
TOTALSIZE=$(($TOTALSIZE/1000000000))
echo "Disk Capacity: $TOTALSIZE GB" >>$MDINFO

# check in /proc/mdstat for the old device
# check for it's slot # in /nkn/tmp/findslot.last file.  Now the new device is
# in other slot. Find it's device id using findslottodcno.sh -c 0 -s $SLOTNO

# DEVICE_1 - The first device in mirror
# DEVICE_2 - The second device in mirror
# SLOTNO_1 - The slot# for first device
# SLOTNO - The slot no that needs the new drive
# RESYNCTIME - The amount of time needed for resync to complete
DEVICE_1=`grep -i 'md5 :' $MDSTAT | awk '{print $5}' |  cut -f1 -d"[" |  sed 's/[0-9]//g'`
SLOTNO_1=`grep -i $DEVICE_1 $LASTFINDSLOTFILE | awk '{print $1}'`
DEVICE_2=`grep -i 'md5 :' $MDSTAT | awk '{print $6}' |  cut -f1 -d"[" |  sed 's/[0-9]//g'`

if [ "${SLOTNO_1}" != "0"  ]; then
    SLOTNO="0"
  else
    SLOTNO="1"
fi

# Check for string "resync" or "rebuild" to see if the mirror is
# Resyncing or Rebuilding. Or Just look for "re" to check for both
# at the same time.
grep -i re $MDSTAT > /dev/null 2>&1
RETCODE=$?
if [ $RETCODE -eq 0 ]; then
  echo "Resync is in progress for boot drive mirror. No administrative action"
  echo "required at this point of time."
  if [ "${ISVERBOSE}" == "yes" ] ; then
    /bin/cat $MDINFO
  fi
  exit 0
fi

# When Boot drive mirror needs replacement
# Check if there is any failure
egrep -q "2/1" $MDSTAT > /dev/null 2>&1
RETCODE=$?
if [ $RETCODE -eq 1 ]; then
  # This means Mirror is in clean state
  echo "Boot drive mirror is in clean state. No administrative action required"
  echo "at this point of time."
  if [ "${ISVERBOSE}" == "yes" ] ; then
    /bin/cat $MDINFO
  fi
  exit 0
else
  # Determine which disk needs replacement
  # We will see something like this in /proc/mdstat for a failed disk
  # md1 : active raid1 sdc1[2](F) sdd1[0]
  # Now figure out which disk has failed and find it's slot. if
  # LASTFINDSLOTFILE changed after the disk is removed, the device
  # may not exist in the list and hence we need to use other disks
  # slotno to determine the correct slotno.
  D1=`cat $MDSTAT | grep md | grep F| head -1 | awk '{print $5}'`
  D2=`cat $MDSTAT | grep md | grep F| head -1 | awk '{print $6}'`
  if [[ "$D1" == *F* ]] ; then
    D1=`echo $D1  |  cut -f1 -d'[' |  sed 's/[0-9]//g'`
    SLOTNO=`grep -i $D1 $LASTFINDSLOTFILE | awk '{print $1}'`
    if [ "${SLOTNO}" == "" ] ; then
      D2=`echo $D2 | cut -f1 -d'[' | sed 's/[0-9]//g'`
      SLOTNO=`grep -i $D2 $LASTFINDSLOTFILE | awk '{print $1}'`
      if [ "${SLOTNO}" != "0" ]; then
	SLOTNO="0"
      else
	SLOTNO="1"
      fi
    fi 
  else
    if [[ "$D2" == *F* ]] ; then
      D2=`echo $D2 | cut -f1 -d'[' | sed 's/[0-9]//g'`
      SLOTNO=`grep -i $D2 $LASTFINDSLOTFILE | awk '{print $1}'`
      if [ "${SLOTNO}" == "" ] ; then
        D1=`echo $D1 | cut -f1 -d'[' | sed 's/[0-9]//g'`
        SLOTNO=`grep -i $D1 $LASTFINDSLOTFILE | awk '{print $1}'`
        if [ "${SLOTNO}" != "0" ]; then
	  SLOTNO="0"
        else
	  SLOTNO="1"
        fi
      fi
   fi
  fi
  echo "Boot drive mirror is in degraded state. Disk in slot $SLOTNO is bad"
  echo "and needs replacement."
  echo "Administrative action: Replace bad drive in slot $SLOTNO and start "
  echo "resync by issuing 'system mirror boot-device resync' "
  if [ "${ISVERBOSE}" == "yes" ] ; then
    /bin/cat $MDINFO
  fi
  exit 0
fi
