#! /bin/bash
#
#       File : initrootcache_dm2_ext3.sh
#
#       Description : This script initializes the root disk cache
#
#       Created By : Michael Nishimoto (miken@nokeena.com)
#
#       Created On : 2 December, 2008
#
#       Modified On :
#
#       Copyright (c) Nokeena Inc., 2008
#	Copyright (c) Juniper Networks., 2010-2013

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
}

if [ $# -lt 1 ]; then
  echo -n "Usage: initrootcache_dm2_ext3.sh <raw partition> <fs partition> "
  echo "<block size in KiB> <page size in KiB> <data integer percentage 1-100>"
  echo "Example: initrootcache_dm2_ext3.sh /dev/sda14 /dev/sda13 2048 32 95"
  echo "  <raw partition> should be a partition"
  echo "  <fs partition> should be a partition"
  echo "  <block size in KiB> size of a data block in KiB"
  echo "  <page size in KiB> size of a disk page in KiB"
  echo "  <percentage> should be 95 <= X <= 98"
  exit 0
fi
RAWDEV=$1
FSDEV=$2
BlkSizeinKiB=$3
PageSizeinKiB=$4
PERC=$5
ERR=/nkn/tmp/err.initrootcache_dm2_ext3.$$

if [ $PERC -lt 0 -o $PERC -gt 100 ]; then
  # 0 indicates default sizing
  echo "Illegal data percentage value.  Must be 0-100"
  exit 1;
fi
# Identify whether it is an Intel SSD drive. If so, perform secure erase
# it.
  if is_intel_ssd_drive "$FSDEV"; then
    perform_secure_erase "$FSDEV"
  fi

MKFS=""
if [ -x /sbin/mkfs.xfs ]; then
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
  ARG="-f -l size=24576b,version=2,sunit=32,lazy-count=1 -d sunit=256,swidth=256"
  if is_Viking_SSD_with_SF1500 "${FSDEV}"; then
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
echo $FSDEV | grep /dev/ > /dev/null 2>&1
if [ $? -ne 0 ]; then
  echo "<fs device> argument must start with /dev/"
  exit 1;
fi
check_is_mounted ${FSDEV}
echo $RAWDEV | grep /dev/ > /dev/null 2>&1
if [ $? -ne 0 ]; then
  echo "<raw device> argument must start with /dev/"
  exit 1;
fi

# Remove partition number at end
if is_cciss_smart_disk; then
  DISK=`echo $FSDEV | sed 's/p[0-9]*$//g'`
else
  DISK=`echo $FSDEV | sed 's/[0-9]*$//g'`
fi

PART_BASENAME=$(echo $FSDEV | sed -e 's@/dev/@@' -e 's@/@-@' )
MKFS_ERR=/nkn/tmp/error.mkfs.ext3.${PART_BASENAME}
${MKFS} ${FSDEV} > $MKFS_ERR 2>&1
if [ $? -ne 0 ]; then
  echo "ERROR: ${MKFS} error"
  cat $MKFS_ERR
  exit 1;
fi
# Put the label back
e2label ${FSDEV} DMFS >> $ERR 2>&1

TMPMNT=/tmp/ext3/root
mkdir -p $TMPMNT >> $ERR 2>&1
mount ${MNT_OPTS} ${FSDEV} $TMPMNT >> $ERR 2>&1
if [ $? -ne 0 ]; then
  echo "ERROR: mount ${FSDEV} ${TMPMNT} error"
  cat $ERR
  exit 1
fi
oneKBlks=$(fdisk -l ${DISK} | grep ${RAWDEV})
if [ $? -ne 0 ]; then
  echo "ERROR: Cannot find ${RAWDEV} in fdisk -l output"
  fdisk -l ${DISK} >> $ERR 2>&1
  cat $ERR
  umount $TMPMNT
  exit 1
fi
oneKBlks=$(fdisk -l ${DISK} | grep ${RAWDEV} | awk '{print $4}' | sed 's/+//g')
#
# Disk blocks will be a multiple of 1KiB which means that we can read any
# power of two up to 2MiB.  If I only wrote 1MiB blocks, then reading
# 2MiB might return unrelated data.
# 
noBlks=$[ $oneKBlks / $BlkSizeinKiB ]
# This is the number of 32KiB disk pages which can fit into some number
# of '$BlkSizeinKiB' disk blocks
noPages=$[ $noBlks * $BlkSizeinKiB / $PageSizeinKiB ];
# One bit for each 32KiB page
bytesFreeBitMap=$[ $noPages / 8 ];

cache2_create $TMPMNT $[$BlkSizeinKiB*1024] $[$PageSizeinKiB*1024] $noPages >> $ERR 2>&1
if [ $? -ne 0 ]; then
  echo "ERROR: cache2_create failed"
  cat $ERR
  umount $TMPMNT
fi
umount $TMPMNT

# Clear out beginning of partition (needed for testing)
dd if=/dev/zero of=${RAWDEV} bs=1M count=10 > /dev/null 2>&1
if [ $? -ne 0 ]; then
  echo "ERROR: Could not clear raw partition"
  exit 1
fi

rm -f ${ERR}
