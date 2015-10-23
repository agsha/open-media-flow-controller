#! /bin/bash
# $1 == device name (/dev/sdb2) of file system
# $2 == mount directory

if [ $# -lt 2 ]; then
  echo "Usage: mount_cache.sh <dev_name> <mnt_dir>"
  exit 1
fi

dev_name=$1
mnt_dir=$2

#
# noatime: don't update inode when atime changes
# nobarrier: don't perform extra log flushes because we have disabled
#	controller and drive write caches
# logbsize=262144: allocate 256KiB log buffers to increase performance
#
EXT3_MOUNT_ARGS="noatime"
XFS_MOUNT_ARGS_V1="noatime,nobarrier"
XFS_MOUNT_ARGS_V2="noatime,nobarrier,logbsize=262144"

if [ ! -e $dev_name ]; then
  logger -s "ERROR: Can not find device name: $dev_name"
  exit 1;
fi

if [ ! -d $mnt_dir ]; then
  logger -s "ERROR: $mnt_dir is not a directory"
  exit 1;
fi

# Need to only look at a small # of bytes because we can't
# guarantee order of execution within the pipes.
/usr/bin/hexdump -n 200 -C $dev_name | head -1 | grep -q XFSB 
if [ $? -eq 0 ]; then
  # XFS file system
  out=`/bin/mount $dev_name $mnt_dir`
  ret=$?
  if [ $ret -ne 0 ]; then
    logger -s "ERROR: XFS mount $dev_name failed: $ret"
    logger -s "$out"
    exit 1;
  fi
  /usr/sbin/xfs_info $mnt_dir | grep '^log' | awk '{print $5}' | grep -q "version=2"
  ret=$?
  umount $mnt_dir
  if [ $ret -eq 0 ]; then
    # version 2 XFS log
    out=`/bin/mount -o $XFS_MOUNT_ARGS_V2 $dev_name $mnt_dir`
    ret=$?
  else
    # version 1 XFS log
    out=`/bin/mount -o $XFS_MOUNT_ARGS_V1 $dev_name $mnt_dir`
    ret=$?
  fi
  if [ $ret -ne 0 ]; then
    logger -s "ERROR: XFS mount $dev_name failed: $ret"
    logger -s "$out"
    exit 1;
  fi
else
  # ext3 file system
  out=`/bin/mount -o $EXT3_MOUNT_ARGS $dev_name $mnt_dir`
  ret=$?
  if [ $ret -ne 0 ]; then
    logger -s "ERROR: EXT3 mount $dev_name failed: $ret"
    logger -s "$out"
    exit 1;
  fi
fi
exit 0
