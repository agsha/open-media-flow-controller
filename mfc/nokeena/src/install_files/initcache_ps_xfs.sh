#! /bin/bash

PATH=$PATH:/opt/nkn/bin:/sbin
INIT_CACHE_PS_DISKS_FILE=/config/nkn/ps-disks-list.txt

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
  local DR=${1}

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

#--------------------------------------------------------------
#
#  MAIN LOGIC BEGINS HERE 
#
#--------------------------------------------------------------

if [ $# -lt 1 ]; then
  echo "Usage: initcache_ps_xfs.sh <device> [<dry_run>]"
  echo "Example: initcache_ps_xfs.sh /dev/sdb"
  echo "  <device>  should be full device name, not a partition"
  echo "  <dry_run> any value, run without changing anything on the disk (optional)"
  exit 0
fi
DR=${1}
if [ "${2}" = "" ]; then
    DRY_RUN=0
else
    DRY_RUN=1
fi
ERR=/nkn/tmp/err.initcache_ps_xfs.$$

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
  MKFS="/sbin/mkfs.xfs -f -l size=24576b,version=2,sunit=32,lazy-count=1 -d sunit=256,swidth=256"
  MNT_OPTS="-o logbsize=131072"
fi
if [ "${MKFS}" = "" -a -x /sbin/mkfs.ext3 ]; then
  MKFS=/sbin/mkfs.ext3
fi
if [ "${MKFS}" = "" ]; then
  echo "Can not find mkfs.xfs or mkfs.ext3 binaries"
  exit 1;
fi
echo ${DR} | grep /dev/ > /dev/null 2>&1
if [ ${?} -ne 0 ]; then
  echo "<device> argument must start with /dev/"
  exit 1;
fi

# Make sure not a drive used by the system.
grep "^${DR}\$" /config/nkn/reserved-disks-list.txt
if [ ${?} -eq 0 ]; then
  echo "System disk cannot be used as a persistent store"
  exit 1
fi

# The disk should not be mounted while we do the formatting
check_is_mounted ${DR}

if [ ${DRY_RUN} -eq 1 ]; then
    exit 1
fi

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

# parted likes to align partitions to cylinder groups; however, the default
# cylinder group size is not a multiple of 4096.  Hence, to guarantee that
# we align partitions to 4096, we take the cylinder group size which must be
# a multiple of a sector (512 bytes) and multiply by 8 to get a super-cylinder.
SZ_CYL=$(fdisk -l $DR | grep "cylinders of" | awk '{print $9}')
SZ_CYL=$[${SZ_CYL} * 8]
SZ_DATA=$(echo "scale=0; $SZ_NUM / $SZ_CYL * $SZ_CYL" | bc)
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

# Sync out the parted changes
sync

# Create an xfs filesystem on the partition
DR_NAME=$(echo $DR | sed -e 's@/dev/@@' -e 's@/@-@' )
MKFS_ERR=/nkn/tmp/error.mkfs.ps.xfs.${DR_NAME}
PART_NAME=${DR}1
for i in `seq 10`; do
  if [ -e ${PART_NAME} ]; then
    break
  fi
  sync
  sleep 1
done
if [ ! -e ${PART_NAME} ]; then
  echo "ERROR: ${PART_NAME} not found after running parted"
  exit 1
fi
$MKFS ${PART_NAME} > $MKFS_ERR 2>&1
if [ $? -ne 0 ]; then
  echo "ERROR: $MKFS error"
  cat $MKFS_ERR
  exit 1;
fi

RAW_BASENAME=`basename ${PART_NAME}`
TMPMNT=/tmp/xfs/ps/${RAW_BASENAME}
mkdir -p $TMPMNT >> $ERR 2>&1
mount ${MNT_OPTS} ${PART_NAME} $TMPMNT >> $ERR 2>&1
if [ $? -ne 0 ]; then
  echo "ERROR: mount ${PART_NAME} ${TMPMNT} error"
  cat $ERR
  exit 1
fi

touch $TMPMNT/.ps

umount $TMPMNT

## Add the device to the sentinel file
if [ -e ${INIT_CACHE_PS_DISKS_FILE} ]; then
  OUT=`cat ${INIT_CACHE_PS_DISKS_FILE} | grep ${DR}`
  if [ "${OUT}" == "" ]; then
      echo ${DR} >> ${INIT_CACHE_PS_DISKS_FILE}
  fi
else
  echo ${DR} > ${INIT_CACHE_PS_DISKS_FILE}
fi

rm -f ${ERR}
