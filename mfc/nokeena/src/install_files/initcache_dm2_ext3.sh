#! /bin/bash
#
#   Copyright (c) Juniper Networks., 2010-2013
#

PATH=$PATH:/opt/nkn/bin:/sbin
TESTING=0

# This file holds common script functions which perform disk identification
# operations for each type of controller.
if [ $TESTING -eq 0 ]; then
  source /opt/nkn/bin/disks_common.sh
elif [ -e /tmp/disks_common.sh ]; then
  source /tmp/disks_common.sh
else
  source /opt/nkn/bin/disks_common.sh
fi

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
  echo -n "Usage: initcache_dm2_ext3.sh <device> <block size in KiB> " 
  echo "<page size in KiB> <data integer percentage 1-100>"
  echo "Example: initcache_dm2_ext3.sh /dev/sdb 256 4 95"
  echo "  <device> should be full device name, not a partition"
  echo "  <block size in KiB> size of a data block in KiB"
  echo "  <page size in KiB> size of a disk page in KiB"
  echo "  <percentage> should be 95 <= X <= 98"
  exit 0
fi
DR=$1
BlkSizeinKiB=$2
PageSizeinKiB=$3
PERC=$4
ERR=/nkn/tmp/err.initcache_dm2_ext3.$$

if [ $PERC -lt 0 -o $PERC -gt 100 ]; then
  # 0 indicates default sizing
  echo "Illegal data percentage value.  Must be 0-100, 0 for default"
  exit 1;
fi
# Identify whether it is an Intel SSD drive. If so, perform secure erase
# it.
  if is_intel_ssd_drive "$DR"; then
    perform_secure_erase "$DR"
  fi
MKFS=""
if [ -x /sbin/mkfs.xfs ]; then
  #
  # XFS will not automatically overwrite other filesystems without the -f option
  #
  # mkfs parameters:
  # Log:
  # - length = 96MB (96*1024*1024/4096 = 24576 blocks)
  # - version = 2
  # - stripe unit = 16KB (16*1024/512=32 sectors)
  #   - align log writes; must be multiple of 4096
  # - lazy-count = 1 : spray superblock updates
  #
  # Data:
  # - block size = 4096 (default)
  # - stripe unit = 128K (256 sectors)
  #   - For large files (> 512 KiB), align end of file to this amount
  # - stripe width = 128K (256 sectors) : stripe width of RAID device
  #     - must be multiple of stripe unit
  #
  # Mount parameters:
  # Log:
  # - logbufs=8: default
  # - logbsize=131072: Allocate 128KiB internal log buffers
  #
  # Use the -K option in mkfs.xfs for Viking drives to avoid using the TRIM
  # command.
  ARG="-f -l size=24576b,version=2,sunit=32,lazy-count=1 -d sunit=256,swidth=256"
  if is_Viking_SSD_with_SF1500 "${DR}"; then
    MKFS="/sbin/mkfs.xfs -K ${ARG}"
  else
    MKFS="/sbin/mkfs.xfs ${ARG}"
  fi
  MNT_OPTS="-o logbsize=131072"
fi
if [ "${MKFS}" = "" -a -x /sbin/mkfs.ext3 ]; then
  MKFS=/sbin/mkfs.ext3
fi
if [ "${MKFS}" = "" ]; then
  echo "Can not find mkfs.xfs or mkfs.ext3 binaries"
  exit 1;
fi
echo $DR | grep /dev/ > /dev/null 2>&1
if [ $? -ne 0 ]; then
  echo "<device> argument must start with /dev/"
  exit 1;
fi
check_is_mounted ${DR}

# Wipe out all previous stuff.  If the device is not completely unmounted,
# this command will fail.
parted -s $DR mklabel msdos >> $ERR 2>&1
if [ $? -ne 0 ]; then
  echo "ERROR: creating label"
  cat $ERR
  exit 1
fi
parted -s $DR print >> $ERR 2>&1
if [ $? -ne 0 ]; then
  echo "ERROR: Cannot read partition information";
  cat $ERR
  exit 1;
fi

# Typical fdisk output
#
#Disk /dev/sdf: 64.0 GB, 64023257088 bytes
#255 heads, 63 sectors/track, 7783 cylinders
#Units = cylinders of 16065 * 512 = 8225280 bytes
#
#   Device Boot      Start         End      Blocks   Id  System
#/dev/sdf1               1        6224    49994152   83  Linux
#/dev/sdf2            6225        7777    12466440+  83  Linux

SZ_NUM=$(fdisk -l $DR | head -2 | tail -1 | sed -e 's@.*, @@g' -e 's@ bytes@@')
SZ=$(echo "scale=0; $SZ_NUM / 4096 * 4096" | bc)
#
# If the drive size is greater than 130GB, the meta partition is 10% the size.
# If the drive is smaller than 130GB, the meta partition is 20% of the size.
#
#SZ_NUM_INT=$(echo $SZ_NUM/1 | bc);
#if [ $SZ_NUM_INT -ge 130000000000 ]; then
#    FS_PERC=90
#else
#    FS_PERC=80
#fi

#
# If the blocksize=256K, this cache disk may be asked to hold smaller objects,
# so we need more metadata space.
# If the blocksize is not 256K[i.e.SAS/SATA] then xfs meta data partition is 
# reduced from 10% to 5%
#
if [ "$BlkSizeinKiB" = "256" ]; then
  FS_PERC=80
else
  FS_PERC=95
fi
if [ $PERC -eq 0 ]; then
  PERC=$FS_PERC
fi
# parted likes to align partitions to cylinder groups; however, the default
# cylinder group size is not a multiple of 4096.  Hence, to guarantee that
# we align partitions to 4096, we take the cylinder group size which must be
# a multiple of a sector (512 bytes) and multiply by 8 to get a super-cylinder.
# SZ_DATA is calculated with the help of temporary variables and using scale
# setting so as to preserve decimal digits 
SZ_CYL=$(fdisk -l $DR | grep "cylinders of" | awk '{print $9}')
SZ_CYL=$[${SZ_CYL} * 8]
TMP_VAR1=$(echo "scale=3; $PERC / 100" | bc)
TMP_VAR2=$(echo "scale=6; $TMP_VAR1 * $SZ_NUM" | bc)
TMP_VAR3=$(echo "scale=10; $TMP_VAR2 / $SZ_CYL" | bc)
TMP_VAR4=$(echo "scale=10; $TMP_VAR3  * $SZ_CYL" | bc)
SZ_DATA=`printf "%.0f\n" $TMP_VAR4`
SZ_START=$[128*1024]

# The data partition would start at a 512 byte boundary with default(0)
# arguments to parted. In case of SSD's we would like to align the
# data partition to the Erase Block Size (EBS) so as optimize the writes on
# to the device. The EBS is supposedly 128*1024 bytes. To make things
# consistent all the data partitions would start at 128*1024 byte offset.
parted -s $DR mkpart primary ${SZ_START}B $[${SZ_DATA} - 1]B
if [ $? -ne 0 ]; then
  echo "ERROR: Creating data partition"
  exit 1;
fi
parted -s $DR mkpart primary ${SZ_DATA}B $[${SZ} - 1]B
if [ $? -ne 0 ]; then
  echo "ERROR: Creating file system partition"
  exit 1;
fi

# Sync out the parted changes
sync

# Create an xfs/ext3 filesystem on the 2nd partition
DR_NAME=$(echo $DR | sed -e 's@/dev/@@' -e 's@/@-@' )
MKFS_ERR=/nkn/tmp/error.mkfs.ext3.${DR_NAME}
if is_cciss_smart_disk ${DR}; then
  META_PART_NAME=${DR}p2
  RAW_PART_NAME=${DR}p1
else
  META_PART_NAME=${DR}2
  RAW_PART_NAME=${DR}1
fi
for i in `seq 10`; do
  if [ -e ${META_PART_NAME} ]; then
    break
  fi
  sync
  sleep 1
done
if [ ! -e ${META_PART_NAME} ]; then
  echo "ERROR: ${META_PART_NAME} not found after running parted"
  exit 1
fi

$MKFS ${META_PART_NAME} > $MKFS_ERR 2>&1
if [ $? -ne 0 ]; then
  echo "ERROR: $MKFS error"
  cat $MKFS_ERR
  exit 1;
fi

RAW_BASENAME=`basename ${RAW_PART_NAME}`
TMPMNT=/tmp/ext3/${RAW_BASENAME}
mkdir -p $TMPMNT >> $ERR 2>&1
mount ${MNT_OPTS} ${META_PART_NAME} $TMPMNT >> $ERR 2>&1
if [ $? -ne 0 ]; then
  echo "ERROR: mount ${META_PART_NAME} ${TMPMNT} error"
  cat $ERR
  exit 1
fi
oneKBlks=$(fdisk -l ${DR} | grep ${RAW_BASENAME})
if [ $? -ne 0 ]; then
  echo "ERROR: Cannot find ${DR_BASENAME}1 in fdisk -l output"
  fdisk -l ${DR} >> $ERR 2>&1
  cat $ERR
  umount $TMPMNT
  exit 1
fi
oneKBlks=$(fdisk -l ${DR} | grep ${RAW_BASENAME} | awk '{print $4}' | sed 's/+//g')
#
# Disk blocks will be a multiple of 1KiB which means that we can read any power of two
# up to 2MiB.  If I only wrote 1MiB blocks, then reading 2MiB might return
# unrelated data.
# 
noBlks=$[ $oneKBlks / $BlkSizeinKiB ]
# This is the number of 32KiB disk pages which can fit into some number
# of '$BlkSizeinKiB' disk blocks
noPages=$[ $noBlks * $BlkSizeinKiB /$PageSizeinKiB ];
# One bit for each page
bytesFreeBitMap=$[ $noPages / 8 ];

cache2_create $TMPMNT $[$BlkSizeinKiB*1024] $[$PageSizeinKiB*1024] $noPages >> $ERR 2>&1
if [ $? -ne 0 ]; then
  echo "ERROR: cache2_create failed"
  cat $ERR
  umount $TMPMNT
fi
umount $TMPMNT

#
# When formatting a drive, make sure there is a valid size for the raw
# partition.  If there is no valid size, then the drive was just added and
# is had no partitions until the formatting, so /config/nkn.diskinfo-startup
# will not have the size.
#
CFG=/config/nkn/diskcache-startup.info
CFG2=/tmp/diskcache-startup.info
if (grep -q $RAW_PART_NAME $CFG); then
  line=`grep $RAW_PART_NAME $CFG`
  size=`echo $line | awk '{print $6}'`
  # Round to 4KiB
  sectors=$[ $oneKBlks / 4 * 4 * 2 ];
  if [ "$size" = "SIZE_UNKNOWN" ]; then
    grep -v $RAW_PART_NAME $CFG > $CFG2
    f1=`echo $line | awk '{print $1" "$2" "$3" "$4" "$5}'`
    f2=`echo $line | awk '{print $7" "$8}'`
    echo "$f1 $sectors $f2" >> $CFG2
    cp $CFG2 $CFG
  fi
fi

# Clear out beginning of partition (needed for testing)
dd if=/dev/zero of=${RAW_PART_NAME} bs=1M count=10 > /dev/null 2>&1
if [ $? -ne 0 ]; then
  echo "ERROR: Could not clear raw partition"
  exit 1
fi

rm -f ${ERR}
