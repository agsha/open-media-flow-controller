#!/bin/bash

#       File: mirror_resync.sh
#
#       Description: The script starts the resync after adding good disk.
#	This script is run when cli command "system mirror boot-device resync"
#	is issued.
#
#       Created On : 25 June, 2012
#
#       Copyright (c) Juniper Networks Inc., 2012
#

# The logic is to determine the current good disk in mirror and
# start resyncing the mirrors for the new drive added.

# Set the Variables
MDADM=/sbin/mdadm
FINDSLOT=/opt/nkn/generic_cli_bin/findslottodcno.sh
SFDISK=/sbin/sfdisk
FDISK=/sbin/fdisk
RAID_DEV=/dev/md
MROUT=/nkn/tmp/mirror_resync.out
MDSTAT=/proc/mdstat

#set the variables for re-partitioning the newdisk
D1OUT=/nkn/tmp/disk1_d.out
NEWD1OUT=/nkn/tmp/newdisk1_d.out
D2OUT=/nkn/tmp/disk2_d.out
DROUT=/nkn/tmp/disk_resize.out

if [ ! -e /opt/nkn/bin/disks_common.sh ]; then
  logger -s "mirror_resync.sh: Missing library file"
  exit 1;
fi
source /opt/nkn/bin/disks_common.sh


# Clean up old temp files
function cleanup_tmpfiles()
{
  /bin/rm $MROUT >/dev/null 2>&1
  /bin/rm $D1OUT >/dev/null 2>&1
  /bin/rm $D2OUT >/dev/null 2>&1
  /bin/rm $NEWD1OUT >/dev/null 2>&1
  /bin/rm $DROUT >/dev/null 2>&1
}

# Check if system is VXA 2100 or not
function Check_vxa2100()
{
  echo "In Check_vxa2100 ..." >> $MROUT 2>&1
  if ! is_vxa_2100 ; then
    echo "Mirroring is not supported on this platform"
    echo "Mirroring is only supported on VXA 2100"
    exit 2
  fi
}

#Check if mirroring is configured
function Check_mirroring()
{
  echo "In Check_mirroring ..." >> $MROUT 2>&1
  grep -q "/dev/md" /config/nkn/boot-disk.info > /dev/null 2>&1
  RETCODE=$?
  if [ $RETCODE -eq 1 ]; then
    echo "Mirroring is not configured on this system"
    exit 0
  fi
}

# Identify the devices that are part of Mirror and their slots.
#
# OLDDEVICE - The device which is already in mirror, ex: /dev/sdd
# NEWDEVICE - The device which needs to be added to mirror, ex: /dev/sdc
# SLOTNO - The slot# in which new drive is added. ex: 1
function Get_Mirroring_devices()
{
  echo "In Get_Mirroring_devices ..." >> $MROUT 2>&1
  # check in /proc/mdstat for the old device
  # check for it's slot # in /nkn/tmp/findslot.last file.  Now the new device is
  # in other slot. Find it's device id using findslottodcno.sh -c 0 -s $SLOTNO
  #
  # Format of $MDSTAT when a drive(sdc in this case) has failed
  # md5 : active raid1 sdd5[0] sdc5[2](F)
  #       4096448 blocks [2/1] [U_]
  #
  # md6 : active raid1 sdd3[0] sdc3[1]
  #       136448 blocks [2/2] [UU]
  #------------------OR-------------------------
  # md5 : active raid1 sdc5[2](F) sdd5[0]
  #	  4096448 blocks [2/1] [U_]
  #
  # md6 : active raid1 sdc6[2](F) sdd6[0]
  #       4096448 blocks [2/1] [U_]
  # md5 is the root and hence we check for drive which has not failed in
  # md5 row. That becomes the old device which will be used for copying
  # data on new device
  string=`cat $MDSTAT | grep md5`
  # CNT is used to omit the first four words and point to the devices
  CNT=0
  for word in $string; do
    if [ "$CNT" -le 3 ]; then
	CNT=$[CNT + 1]
	    continue
    else
	OLDDEVICE=`echo $word | grep F`
	if [ $? == 0 ]; then
	    continue
        else
	    OLDDEVICE=$word
            break
        fi
    fi
  done

# There is no md device which has failed, so just look for the only device
  # that is currently in the mdstat. This happens only when the system is booted
  # with only one mirrored disk
  if [ "${OLDDEVICE}" == "" ]; then
    OLDDEVICE=`grep -i 'md5 :' $MDSTAT | awk '{print $5}'`
  fi
  OLDDEVICE=`echo $OLDDEVICE | cut -f1 -d'[' | sed 's/[0-9]//g'`
  OLDSLOTNO=`grep -i $OLDDEVICE $LASTFINDSLOTFILE | awk '{print $1}'`

  if [ "${OLDSLOTNO}" != "0"  ]; then
      SLOTNO="0"
    else
      SLOTNO="1"
  fi

  NEWDEVICE=`$FINDSLOT -q -c 0 -s $SLOTNO | awk '{print $2}'`
  OLDDEVICE=`echo "/dev/$OLDDEVICE"`

  echo "The OLDDEVICE is $OLDDEVICE" >> $MROUT
  echo "The $NEWDEVICE is inserted in slot $SLOTNO" >> $MROUT

  if [ "${NEWDEVICE}" == "" ]; then
    echo "Failed to resync, no spare disc in slot $SLOTNO"
    if [ "${ISVERBOSE}" == "yes" ] ; then
      /bin/cat $MROUT
    fi
    exit 1
  fi
}

# Check if the new device is already part of mirror
function Is_mirroring_required()
{
  echo "In Is_mirroring_required ..." >> $MROUT 2>&1
  NEWDEVICE_1=`echo $NEWDEVICE | cut -f3 -d"/"`
  grep -i $NEWDEVICE_1 /proc/mdstat  > /dev/null 2>&1
  RETCODE=$?
  if [ $RETCODE -eq 0 ]; then
    echo "Device $NEWDEVICE in slot $SLOTNO is already part of mirror" >>$MROUT
    echo "Mirror resync is not required at this time."
    if [ "${ISVERBOSE}" == "yes" ] ; then
      /bin/cat $MROUT
    fi
    exit 1
  fi
}

# Check the newly inserted disk size and see if we can use it for mirroring
function CheckDiskSize()
{
  echo "In CheckDiskSize ..." >> $MROUT 2>&1
  # Do error checking to see if we can use this device for mirroring or not.
  # We check for sizes for the two disks and if the difference is more then
  # 10 gb, we fail the sync
  olddevicesize=`$FDISK -l $OLDDEVICE | grep Disk | head -1 | awk '{print $3}'`
  if [ ${?} -ne 0 ] ; then
    echo "Error while getting device $OLDDEVICE size " >>$MROUT
    exit 1
  fi
  newdevicesize=`$FDISK -l $NEWDEVICE | grep Disk | head -1 | awk '{print $3}'`
  if [ ${?} -ne 0 ] ; then
    echo "Error while getting device $NEWDEVICE size " >>$MROUT
    exit 1
  fi

  echo "newdevice $NEWDEVICE size is $newdevicesize" >>$MROUT
  echo "old device $OLDDEVICE size is $olddevicesize" >>$MROUT

  # Using this type of expressions to handle floationg point arithmetic.
  sizedifference=`echo "$olddevicesize-$newdevicesize" | bc`

  echo "size difference of two disks is " $sizedifference >>$MROUT

  cmpres1=`echo "$sizedifference >= 0" | bc`
  cmpres2=`echo "$sizedifference < 10" | bc`
  if [[ $cmpres1 == 1 && $cmpres2 == 1 ]];  then
    # We can continue to partition the disk
    echo "Size difference is in accepatable range " >>$MROUT
    is_resizedisk_reqd="yes"
  else
    echo "Resync failed, size of disk in $SLOTNO is invalid"
    echo "Size of inserted device in slot $SLOTNO  is $newdevicesize"
    echo "The difference between two disks mirrored is more than 10gb"
    echo "and hence can't be used  for mirroring. Pl. insert the disk"
    echo "with proper size and start the resync again."
    if [ "${ISVERBOSE}" == "yes" ] ; then
      /bin/cat $MROUT
    fi
    exit 1
  fi
}

# Re-partition logic is as follows
# Currently the root disks are partitioned as below:
# $ sfdisk -d /dev/sdd
# partition table of /dev/sdd
# unit: sectors
#
# /dev/sdd1 : start=        1, size=   273104, Id=fd, bootable
# /dev/sdd2 : start=   273105, size=   273105, Id=fd
# /dev/sdd3 : start=   546210, size=   273105, Id=fd
# /dev/sdd4 : start=   819315, size=975948750, Id= f
# /dev/sdd5 : start=   819316, size=  8193149, Id=fd
# /dev/sdd6 : start=  9012466, size=  8193149, Id=fd
# /dev/sdd7 : start= 17205616, size=   530144, Id=fd
# /dev/sdd8 : start= 17735761, size=  6297479, Id=fd
# /dev/sdd9 : start= 24033241, size=151010999, Id=fd
# /dev/sdd10: start=175044241, size=209728574, Id=fd
# /dev/sdd11: start=384772816, size= 20482874, Id=83
# /dev/sdd12: start=405255691, size=571512374, Id=fd
# $
# Here's the typicall mirrored disk layout:
# Partition 1 - BOOTMGR
# Partition 2 - BOOT_1
# Partition 3 - BOOT_2
# Partition 4 - Extended partition
# Partition 5 - ROOT_1
# Partition 6 - ROOT_2
# Partition 7 - CONFIG
# Partition 8 - VAR
# Partition 9 - COREDUMP
# Partition 10 - NKN
# Partition 11 - /spare Spare partition
# Partition 12 - LOG

# As you can see, we use Patition 11 for /spare. If the new disk inserted is
# of small size, we see if we can still use it for partitioning by reducing the
# /spare partition size on new disk. The new partition table will look exactly
# similar except for partition 11 size and partition 12 start location as well
# as the extended partition 4 size
#
# 1. Get the total size of new disk first. This would be start+size
#    of last valid partition in the sfdisk output.
# Ex: This is new disk output:
# /dev/sdb2 : start=862369200, size= 95923984, Id=83
# /dev/sdb3 : start=        0, size=        0, Id= 0
# /dev/sdb4 : start=        0, size=        0, Id= 0
# Now NEWTOTSIZE=862369200+95923984=958293184
#
# 2. Now find the size of last partition of old disk and start sector#
#    for 11th partition
# Ex: These are the last two partitions of mirrored root
# /dev/sdd11: start=384772816, size= 20482874, Id=83
# /dev/sdd12: start=405255691, size=571512374, Id=fd
# LASTPARTSIZE=571512374 and
# PART11START=384772816
#
# 3. Now find the start of 12th partition and size of 11th partition
#    NEW12PARTSTART = $NEWTOTSIZE-$PART11START
#    NEW11PARTSIZE=$NEW12PARTSTART-$PART11START-1
#
# 4. Also modify the part 4 size which will be $NEWTOTSIZE
# Example new partition table  will be:
#

function resize_disk()
{
  echo "In resize_disk ..." >> $MROUT 2>&1
  local OLDDEV=$1
  local NEWDEV=$2

  echo "In resize script" > $DROUT

  # 1st argument to script is the device that is already part of boot mirror
  # 2nd argument is the device that needs to be added to boot mirror
  # For now assuming args are OLDDEV & NEWDEV
  echo "The device that is being resized is $NEWDEV from device $OLDDEV" >> $DROUT 2>&1

  $SFDISK -d $OLDDEV > $D1OUT
  if [ ${?} -ne 0 ] ; then
    echo "Error while getting device $OLDDEV partition table " >>$MROUT
    exit 1
  fi
  parted --script ${NEWDEV} print > /dev/null 2>&1
  if [ $? -eq 1 ]; then
    #
    # No partition table present; parted will return 0 if it finds a
    # ext3 filesystem on the entire partition.
    #
    echo "Error while getting device $NEWDEV partition table " >>$MROUT
    SIZE=`$SFDISK -s $NEWDEV`
    NEWTOTSIZE=$(($SIZE+$SIZE))
    echo "Size is $SIZE, Newtotalsize is $NEWTOTSIZE" >> $DROUT 2>&1
  else
    $SFDISK -d $NEWDEV > $D2OUT
    if [ ${?} -ne 0 ] ; then
      echo "Error while getting device $NEWDEV partition table " >>$MROUT
      exit 1
    fi
    START=`cat $D2OUT | grep -v " 0,"  | tail -1 | cut -f1 -d',' | cut -f2 -d'='`
    SIZE=`cat $D2OUT | grep -v " 0,"  | tail -1 | cut -f2 -d',' | cut -f2 -d'='`
    NEWTOTSIZE=$(($START+$SIZE))
    echo "START is $START, Size is $SIZE, Newtotalsize is $NEWTOTSIZE" >> $DROUT 2>&1
  fi
  echo "The sfdisk output of $OLDDEV is "  >> $DROUT 2>&1
  cat $D1OUT >> $DROUT 2>&1
  echo "The sfdisk output of $NEWDEV is "  >> $DROUT 2>&1
  cat $D2OUT >> $DROUT 2>&1

  LASTPARTSIZE=`cat $D1OUT | tail -1 |  awk '{print $3}' | cut -f2 -d'='| cut -f1 -d','`
  PART11START=`cat $D1OUT | tail -2 | head -1 |  awk '{print $2}' | cut -f2 -d'='| cut -f1 -d','`

  echo "partition 11 start is $PART11START" >> $DROUT 2>&1
  echo "last partition size is $LASTPARTSIZE" >> $DROUT 2>&1

  NEWPART12START=$(($NEWTOTSIZE-$LASTPARTSIZE))
  echo "new last partition start is $NEWPART12START" >> $DROUT 2>&1
  NEW11PARTSIZE=$(($NEWPART12START-$PART11START-1))
  echo "new 11th partition size is $NEW11PARTSIZE" >> $DROUT 2>&1

  if [[ $NEW11PARTSIZE -lt 0 ]]; then
    echo "$NEW11PARTSIZE is negative, something is wrong with sfdisk calculations" >> $DROUT 2>&1
    return 1
  fi

  # Now we need to replace patition 4 size(NEWTOTSIZE), Partition 11 size
  # (NEW11PARTSIZE) and new partition 12 start(NEWPART12START) in D1OUT
  # Finally display the output file
  echo "# " > $NEWD1OUT
  cat $D1OUT | while read line
  do
    if [[ "$line" == /dev/* ]]; then
      PARTNO=`echo $line |  awk '{print $1}' | cut -f3 -d'/' | sed 's/[^0-9]//g'`
      # Check if it's partition 4 or 11 or 12
      # Partition 4 is an extended partition and it's size is equivalent to
      # disk size
      # Partition 11 size is adjusted spare size
      # Partition 12 size has to be same as partition 12 size on the previous
      # mirrored disk
      if [[ "$PARTNO" == "4" ]]; then
        STR1=`echo $line | cut -f1 -d','`
        STR3=`echo $line | cut -f3 -d','`
        echo "$STR1, size=$NEWTOTSIZE, $STR3" >> $NEWD1OUT
      elif [[ "$PARTNO" == "11" ]]; then
          STR1=`echo $line | cut -f1 -d','`
          STR3=`echo $line | cut -f3 -d','`
          echo "$STR1, size=$NEW11PARTSIZE, $STR3" >> $NEWD1OUT
      elif [[ "$PARTNO" == "12" ]]; then
            STR1=`echo $line | cut -f1 -d'='`
            STR3=`echo $line | cut -f2-3 -d','`
            echo "$STR1=$NEWPART12START, $STR3" >> $NEWD1OUT
      else
            echo "$line" >> $NEWD1OUT
      fi
    else
      echo "$line" >> $NEWD1OUT
    fi
  done

  echo "NEW sfdisk Input file is" >> $DROUT 2>&1
  cat $NEWD1OUT >> $DROUT 2>&1
  cat $NEWD1OUT | $SFDISK --force $NEWDEV >> $DROUT 2>&1
  retval=$?
  cat $DROUT >> $MROUT 2>&1
  return $retval
}

function Partition_disk()
{
  echo "In Partition_disk ..." >> $MROUT 2>&1
  # Copy the MBR from bootable old disk onto new one
  echo "Copying MBR from $OLDDEVICE onto $NEWDEVICE"  >> $MROUT 2>&1
  dd if=$OLDDEVICE of=$NEWDEVICE oflag=direct bs=512 count=1 >> $MROUT 2>&1

  #Partiton the new device
  if [ "${is_resizedisk_reqd}" == "yes" ] ; then
    resize_disk $OLDDEVICE $NEWDEVICE
    RETCODE=$?
  else
    # This means both disks are of same size
    $SFDISK -d $OLDDEVICE | $SFDISK --force $NEWDEVICE >> $MROUT 2>&1
    RETCODE=$?
  fi
  echo "sfdisk error code is " $RETCODE >>$MROUT
  if [ $RETCODE -eq 1 ]; then
    echo "Device $NEWDEVICE in Slot $SLOTNO is currently in use"
    echo "Unmount all filesystems using this device and resync again"
    if [ "${ISVERBOSE}" == "yes" ] ; then
      /bin/cat $MROUT
    fi
    exit 1
  fi
}

# Start the mirror resync
function Start_Resync()
{
  echo "In Start_Resync ..." >> $MROUT 2>&1
  for PNUM in 1 2 3 5 6 7 8 9 10 12
  do
    R=${RAID_DEV}${PNUM}
    D1=${NEWDEVICE}${PNUM}
    echo +++ ${R} ${D1} >>$MROUT
    $MDADM --zero-superblock ${D1} >> $MROUT 2>&1
    $MDADM ${R} --add ${D1} >> $MROUT 2>&1
    RETCODE=$?
    if [ $RETCODE -eq 1 ]; then
      # Try one more time before moving onto next partition.
      # Some times this might fail when the sfdisk partition table
      # update is slow. Noticed only with 1st partition during tests.
      echo "Sleeping 2 seconds before trying one more time" >> $MROUT
      sleep 2
      $MDADM ${R} --add ${D1} >> $MROUT 2>&1
    fi
  done
  echo "Mirror resync started"
  if [ "${ISVERBOSE}" == "yes" ] ; then
    /bin/cat $MROUT
  fi
  exit 0
}

# Check if the script is called in verbose mode ("detail" in CLI)
SHIFTCNT=0
while getopts "v" opt; do
  case $opt in
    v ) ISVERBOSE="yes"
        (( SHIFTCNT++ ))
        ;;
  esac
  (( SHIFTCNT++ ))
done

#
# Main Logic begins here
#

# Clean up old temporary files
cleanup_tmpfiles

echo "The mirror resync debug output is ..." > $MROUT

# Check if the system is vxa 2100.
Check_vxa2100
# Check if mirroring is used
Check_mirroring
# Get the mirroring devices
Get_Mirroring_devices
# Check if mirroring is required at this time
Is_mirroring_required
# Check if the newly inserted disk can be added to the mirror.
CheckDiskSize
# Partition the new disk
Partition_disk
# Now the new disk is ready, add it to the mirror
Start_Resync
