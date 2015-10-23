#! /bin/bash
#
#	File : re-init-config-disks.sh
#
#	Description : This script re-initializes all the disk caches in
#		/config/nkn/diskcache-startup.info
#
#	Created By : Michael Nishimoto (miken@nokeena.com)
#
#	Created On : 27 January, 2009
#
#	Copyright (c) Nokeena Inc., 2009
#

# source the common disk functions
source /opt/nkn/bin/disks_common.sh

TESTING=0
PATH=$PATH:/sbin
CACHE_CONFIG=/config/nkn/diskcache-startup.info
INITCACHE=/opt/nkn/bin/initcache_dm2_ext3.sh
INITROOTCACHE=/opt/nkn/bin/initrootcache_dm2_ext3.sh
PERCENT=0 #default sizing
if [ -x /opt/nkn/bin/smartctl ]; then
  SMARTCTL=/opt/nkn/bin/smartctl
else
  SMARTCTL=/usr/sbin/smartctl
fi
MDREQ=/opt/tms/bin/mdreq
TMPSHELL=/nkn/tmp/reinit.$$

# This file holds common script functions which perform disk identification
# operations for each type of controller.
if [ $TESTING -eq 0 ]; then
  source /opt/nkn/bin/disks_common.sh
elif [ -e /tmp/disks_common.sh ]; then
  source /tmp/disks_common.sh
else
  source /opt/nkn/bin/disks_common.sh
fi

# TallMaple Nodes
TM_DISKID=/nkn/nvsd/diskcache/config/disk_id
TM_ACTIVATED=activated
TM_CACHE_ENABLED=cache_enabled
TM_COMMENTED_OUT=commented_out
TM_FS_PART=fs_partition
TM_MISSING_AFTER_BOOT=missing_after_boot
TM_RAW_PART=raw_partition
TM_SET_SEC_CNT=set_sector_count
TM_SUBPROV=sub_provider

#NKN_MDREQ_LOG=/nkn/tmp/write_diskinfo_to_db.log

#
# Skip this disk if the first character is a comment character
#
function skip_this_disk()
{
  local serial_num=$1
  local filter_serial_num=`echo $serial_num | sed 's/^#//g'`

  if [ "$serial_num" = "$filter_serial_num" ]; then
    return 1;
  else
    return 0;
  fi
}	# skip_this_disk


function reinit()
{
  local line
  local serial_num
  local raw_part
  local fs_part
  local set_sector_cnt
  local ptype
  local misc1
  local comment_out
  local disk_name
  local stat_name
  local block_size
  local page_size

  if [ ! -e ${CACHE_CONFIG} ]; then
    return;
  fi
  cat ${CACHE_CONFIG} | \
  while read line; do
    set $line
    if [ $# -ne 9 ]; then
      logger -s "Illegal disk cache line: $line"
      continue;
    fi
    serial_num=$1
    raw_part=$2
    fs_part=$3
    disk_name=$4
    stat_name=$5
    set_sector_cnt=$6
    ptype=$7
    misc1=$8

    comment_out=0
    if (skip_this_disk $serial_num); then
      comment_out=1
      serial_num=`echo $serial_num | sed 's/^#*//g'`
      continue;
    fi

    ret=`get_disk_block_page_sizes $ptype`
    block_size=`echo "$ret" | awk '{print $1}'`
    page_size=`echo "$ret" | awk '{print $2}'`

    if (is_system_disk $raw_part); then
      echo "${INITROOTCACHE} ${raw_part} ${fs_part} ${block_size} ${page_size} ${PERCENT}"
      echo "${INITROOTCACHE} ${raw_part} ${fs_part} ${block_size} ${page_size} ${PERCENT} &" >> ${TMPSHELL}
    else
      echo "${INITCACHE} ${disk_name} ${block_size} ${page_size} ${PERCENT}"
      echo "${INITCACHE} ${disk_name} ${block_size} ${page_size} ${PERCENT} &" >> ${TMPSHELL}
    fi
  done
  wait
}	# reinit


function umount_all_disk_caches()
{
  local i

  for i in `seq 20`; do
    umount /nkn/mnt/dc_${i} > /dev/null 2>&1
  done
}	# umount_all_disk_caches


#--------------------------------------------------------------
#
#  MAIN LOGIC BEGINS HERE 
#
#--------------------------------------------------------------
umount_all_disk_caches
rm -f ${TMPSHELL}
reinit
echo "wait" >> ${TMPSHELL}
sh ${TMPSHELL}
rm -f ${TMPSHELL}
