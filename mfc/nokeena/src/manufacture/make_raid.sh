#! /bin/bash
# This is a script to create the RAID from two real drives.
# This script is run from a writeimage hook function in customer_rootflop.sh.
#
# We are called after the two real drives have the partitions created
# but before the filesystems are created, so that the filesystems
# can be created on the RAIDed partition.
#
# The command line parameter to this function is the full path filename
# of the raid_disk_info.txt file.  This file was created by install-mfc
# when it was instructed to install on a RAID root device.
#
# These are the settings in raid_disk_info.txt with example settings:
# MD_DEV=/dev/md
# MD_PDEV=/dev/md
# RAID1_DEV=/dev/sda
# RAID1_PDEV=/dev/sdb
# RAID2_DEV=/dev/sda
# RAID2_PDEV=/dev/sdb

# The *_DEV settings have the device name for the whole device.
# The *_PDEV settings have the device name that we append a partition number to.

DEV_NAME_FILE=${1}
PART_INFO_FILE=${2}

if [ -z "${PART_INFO_FILE}" ] ; then
  echo Error, script ${0} was called without required parameters.
  exit 1
fi
if [ ! -f "${DEV_NAME_FILE}" -o ! -f "${PART_INFO_FILE}" ] ; then
  echo Error, script ${0} was called with an invalid parameter.
  exit 1
fi
for ITEM in RAID_DEV D1_DEV D2_PDEV D2_DEV D2_PDEV
do
  grep -q ^${ITEM}= ${DEV_NAME_FILE}
  [ ${?} -eq 0 ] && continue
  echo Error, script ${0} missing setting ${ITEM} in ${DEV_NAME_FILE}
  exit 1
done
# All OK so dot it in.
. ${DEV_NAME_FILE}

##############################################################################
#### About the --metadata parameter (from http://linux.die.net/man/8/mdadm)
# -e, --metadata=
#    Declare the style of RAID metadata (superblock) to be used.
#    The default is 1.2 for --create, and to guess for other operations.
#    The default can be overridden by setting the metadata value for the
#    CREATE keyword in mdadm.conf.
#
# Options are: 
#    0, 0.90 
#
# Use the original 0.90 format superblock. This format limits arrays to 28
# component devices and limits component devices of levels 1 and greater to 2
# terabytes. It is also possible for there to be confusion about whether the
# superblock applies to a whole device or just the last partition, if that
# partition starts on a 64K boundary.
#
#    1, 1.0, 1.1, 1.2 default 
#
# Use the new version-1 format superblock. This has fewer restrictions.
# It can easily be moved between hosts with different endian-ness, and a
# recovery operation can be checkpointed and restarted.
# The different sub-versions store the superblock at different locations on
# the device, either at the end (for 1.0), at the start (for 1.1) or 4K from
# the start (for 1.2). "1" is equivalent to "1.2" (the commonly preferred 1.x
# format). "default" is equivalent to "1.2".
#
#    ddf
#
# Use the "Industry Standard" DDF (Disk Data Format) format defined by SNIA.
# When creating a DDF array a CONTAINER will be created, and normal arrays
# can be created in that container.
#
#    imsm
#
# Use the Intel(R) Matrix Storage Manager metadata format. This creates a
# CONTAINER which is managed in a similar manner to DDF, and is supported by
# an option-rom on some platforms: 
#   http://www.intel.com/design/chipsets/matrixstorage_sb.htm
#
##############################################################################

MDADM_EXE=/sbin/mdadm
MDADM_PARAMS="--metadata=0.90"
if [ -f /tmp/mdadm.params ] ; then
  MDADM_PARAMS=`cat /tmp/mdadm.params`
  echo MDADM_PARAMS=${MDADM_PARAMS}
fi

# Make sure the raid is stopped and devices not in use.
for PNUM in 1 2 3 5 6 7 8 9 10 11 12
do
  R=${RAID_DEV}${PNUM}
  D1=${D1_PDEV}${PNUM}
  D2=${D2_PDEV}${PNUM}
  ${MDADM_EXE} --stop ${R} > /dev/null 2>&1
  umount ${D1} > /dev/null 2>&1
  umount ${D2} > /dev/null 2>&1
done

echo +++ Initializing second RAID drive
echo +++ Zeroing existing partition table on ${D2_DEV}
dd if=/dev/zero of=${D2_DEV} bs=512 count=256 > /dev/null 2>&1 || FAILURE=1
# Zero the end of the disk as well
SIZE_K=`sfdisk -q -s ${D2_DEV} 2> /dev/null` || FAILURE=1
if [ -z "${SIZE_K}" ] ; then
  echo Failed to initialize drive ${D2_DEV}
  exit 1
fi
# We use 'awk' as busybox expr does not work above 2^32
SIZE_M=`echo ${SIZE_K} | awk '{f= $1 / 1024; print int(f)}'`
SEEK_M=`echo ${SIZE_M} | awk '{f= $1 - 10; if (f > 0) {print int(f)} else {print 0}}'`
if [ ! -z "${SEEK_M}" ]; then
  dd if=/dev/zero of=${D2_DEV} bs=1M seek=${SEEK_M} > /dev/null 2>&1
fi

Chop_Ends()
{
  F=${1}
  B=${2}
  L=`wc -l ${3} | awk '{ print $1 }' | cut -f1 -d' '`
  cat ${3} | tail -$((L - F)) | head -$((L - F - B))
}

echo +++ Create partitions
RV=0
cat ${PART_INFO_FILE}
grep -q "script " ${PART_INFO_FILE}
if [ ${?} -ne 0 ] ; then
  cat ${PART_INFO_FILE} | sfdisk -uM ${D2_DEV} > /tmp/sfdisk.out 2>&1
  RV=${?}
  if [ ${RV} -eq 0 ] ; then
    # Only print out the useful info
    grep ^Disk /tmp/sfdisk.out 
    Chop_Ends 10 5 /tmp/sfdisk.out 
  else
    cat /tmp/sfdisk.out
  fi
else
  # This case is not expected.
  cat ${PART_INFO_FILE} | sed "s#${D1_DEV}#${D2_DEV}#" > /tmp/d2_part_script.sh
  sh /tmp/d2_part_script.sh 2>&1 > /tmp/d2_part_disk.out
  parted --script -- ${D2_DEV} unit MiB 2>&1 print >> /tmp/d2_part_disk.out
  RV=${?}
  cat /tmp/d2_part_disk.out
fi
if [ ${RV} -ne 0 ]; then
  echo Failed to create partition table
  exit 1
fi

# At this point the second disk has the needed partitions.
# Now create the RAID for each
ls -l ${RAID_DEV}*

echo +++ Create Mirror
for PNUM in 1 2 3 5 6 7 8 9 10 11 12
do
  R=${RAID_DEV}${PNUM}
  D1=${D1_PDEV}${PNUM}
  D2=${D2_PDEV}${PNUM}
  echo +++ ${R} ${D1} ${D2}
  ${MDADM_EXE} --stop ${R} > /dev/null 2>&1
  ${MDADM_EXE} --zero-superblock ${D1} ${D2} > /dev/null 2>&1
  dd if=/dev/zero of=${D1} bs=512 count=256 > /dev/null 2>&1
  dd if=/dev/zero of=${D2} bs=512 count=256 > /dev/null 2>&1
  ${MDADM_EXE} --create --assume-clean --force --verbose ${MDADM_PARAMS} ${R} --level=1 --raid-disks=2 ${D1} ${D2}
done
exit 0
