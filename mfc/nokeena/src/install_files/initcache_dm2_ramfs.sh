#! /bin/bash

PATH=$PATH:/opt/nkn/bin:/sbin

check_one_mounted()
{
  if grep -q "${1} " /proc/mounts; then
    return 0;
  else
    return 1;
  fi
}

#
# Check to see /dev/sdX, /dev/sdX1, or /dev/sdX2 is mounted.
#
check_is_mounted()
{
  local DR=$1

  check_one_mounted ${DR}
  if [ $? -eq 0 ]; then
    echo "${DR} is mounted.  Unmount before running this script"
    exit 1;
  fi
  check_one_mounted ${DR}1
  if [ $? -eq 0 ]; then
    echo "${DR}1 is mounted.  Unmount before running this script"
    exit 1;
  fi
  check_one_mounted ${DR}2
  if [ $? -eq 0 ]; then
    echo "${DR}2 is mounted.  Unmount before running this script"
    exit 1;
  fi
}

if [ $# -lt 1 ]; then
  echo -n "Usage: initcache_dm2_ramfs.sh <device> <block size in KiB> <ram disk> <mgmt name> "
  echo "<page size in KiB> <data integer percentage 1-100>"
  echo "Example: initcache_dm2_ramfs.sh /dev/ram0 256 4 95 dc_1"
  echo "  <device> should be full device name, not a partition"
  echo "  <block size in KiB> size of a data block in KiB"
  echo "  <page size in KiB> size of a disk page in KiB"
  echo "  <percentage> should be 95 <= X <= 98"
  echo "  <ram disk> ram disk that should be used"
  echo "  <disk name> manangement name of the disk"
  exit 0
fi
DR=$1
BlkSizeinKiB=$2
PageSizeinKiB=$3
PERC=$4
RAMDISK=$5
MGMT_NAME=$6
ERR=/nkn/tmp/err.initcache_dm2_ramfs.$$

if [ $PERC -lt 0 -o $PERC -gt 100 ]; then
  # 0 indicates default sizing
  echo "Illegal data percentage value.  Must be 0-100, 0 for default"
  exit 1;
fi

echo $DR | grep /dev/ > /dev/null 2>&1
if [ $? -ne 0 ]; then
  echo "<device> argument must start with /dev/"
  exit 1;
fi
check_is_mounted ${DR}

SZ_NUM=$(fdisk -l $DR | head -2 | tail -1 | sed -e 's@.*, @@g' -e 's@ bytes@@')
SZ=$(echo "scale=0; $SZ_NUM / 4096 * 4096" | bc)
# If the drive size is greater than 130GB, the meta partition is 10% the size
# If the drive is smaller than 130GB, the meta partirion is 20% of the size
SZ_NUM_INT=$(echo $SZ_NUM/1 | bc);
if [ $SZ_NUM_INT -ge 130000000000 ]; then
    FS_PERC=90
else
    FS_PERC=80
fi
if [ $PERC -eq 0 ]; then
    PERC=$FS_PERC
fi
META_SZ=$(echo "scale=0; $SZ_NUM * (100 -$FS_PERC) / 100" | bc)
#echo $META_SZ $SZ $FS_PERC

# Create an ramfs filesystem to hold the meta data
DR_NAME=$(echo $DR | sed -e 's@/dev/@@' -e 's@/@-@' )
MKFS_ERR=/nkn/tmp/error.mkfs.ramfs.${DR_NAME}
RAW_PART_NAME=${DR}0
TMPMNT=/nkn/mnt/$MGMT_NAME
#exit
mount -t ramfs -o size=$META_SZ $RAMDISK $TMPMNT

oneKBlks=$(echo "scale=0; $SZ_NUM / 1024" | bc)
noBlks=$[ $oneKBlks / $BlkSizeinKiB ]
# This is the number of 32KiB disk pages which can fit into some number
# of '$BlkSizeinKiB' disk blocks
noPages=$[ $noBlks * $BlkSizeinKiB /$PageSizeinKiB ];
# One bit for each page
bytesFreeBitMap=$[ $noPages / 8 ];

cache2_create $TMPMNT $[$BlkSizeinKiB*1024] $[$PageSizeinKiB*1024] $noPages 1 >> $ERR 2>&1
if [ $? -ne 0 ]; then
  echo "ERROR: cache2_create failed"
  cat $ERR
  umount $TMPMNT
fi
#umount $TMPMNT

# Clear out beginning of partition (needed for testing)
dd if=/dev/zero of=${RAW_PART_NAME} bs=1M count=10 > /dev/null 2>&1
if [ $? -ne 0 ]; then
  echo "ERROR: Could not clear raw partition"
  exit 1
fi

rm -f ${ERR}
