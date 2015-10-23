#! /bin/bash
#
#	File : write_disks_to_db.sh
#
#	Description : This script pushs information gathered from the
#		initdisks.sh script (/config/nkn/diskcache-startup.info)
#		to the TallMaple DB
#
#	Created By : Michael Nishimoto (miken@nokeena.com)
#
#	Created On : 15 January, 2009
#
#	Copyright (c) Nokeena Inc., 2009
#

PATH=$PATH:/sbin
CACHE_CONFIG=/config/nkn/diskcache-startup.info
MDREQ=/opt/tms/bin/mdreq
MDDBREQ=/opt/tms/bin/mddbreq
INITCACHE=/opt/nkn/bin/initcache_dm2_ext3.sh
PERCENT=0

if [ ! -e /opt/nkn/bin/disks_common.sh ]; then
  logger -s "write_diskinfo_to_db.sh: Missing library file"
  exit 1;
fi
source /opt/nkn/bin/disks_common.sh

DB_CHANGED_FLAG_FILE=/nkn/tmp/write_diskinfo_to_db.changed.tmp
TM_DB_IS_EMPTY=0
DB_CMD=
DB_ADD_CMD=
DB_GET_CMD=
DB_ITERATE_CMD=
ACTIVE_DB=
VMWARE_SENTINEL_FILE=/config/nkn/allow_vmware

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
TM_OWNER=owner
TM_DISK_NAME=block_disk_name
TM_STAT_NAME=stat_size_name
TM_FIRMWARE_VER=firmware_version

LOG_FILE=/nkn/tmp/write_diskinfo_to_db.log

# Local functions

function setup_db_cmd()
{
  ps -e | grep -v grep | grep -q mgmtd
  if [ ${?} -eq 0 ]; then
    if [ ! -x ${MDREQ} ]; then
      logger -s "${ME}: ERROR: Command to update DB is missing: ${MDREQ}"
      return 1
    fi
    ACTIVE_DB=
    DB_CMD="${MDREQ}"
    DB_ADD_CMD="${MDREQ} set create -"
    DB_GET_CMD="${MDREQ} query get -"
    DB_ITERATE_CMD="${MDREQ} -v query iterate -"
  else
    if [ ! -x ${MDDBREQ} ]; then
      logger -s "${ME}: ERROR: Command to update DB is missing: ${MDDBREQ}"
      return 2
    fi
    ACTIVE_DB="/config/db/`cat /config/db/active`"
    DB_CMD="${MDDBREQ}"
    DB_ADD_CMD="${MDDBREQ} ${ACTIVE_DB} set modify -"
    DB_GET_CMD="${MDDBREQ} ${ACTIVE_DB} query get -"
    DB_ITERATE_CMD="${MDDBREQ} -v ${ACTIVE_DB} query iterate -"
  fi
  # Verify that the command will work.
  echo "${DB_ITERATE_CMD} ${TM_DISKID}" >> ${LOG_FILE}
  ${DB_ITERATE_CMD} ${TM_DISKID} >> ${LOG_FILE} 2>&1
  if [ ${?} -ne 0 ]; then
    echo "ERROR: Failed: ${DB_ITERATE_CMD} ${TM_DISKID}" >> ${LOG_FILE}
    logger -s "${ME}: ERROR: Failed: ${DB_ITERATE_CMD} ${TM_DISKID}"
    return 3
  fi    
  return 0
}

function list_disk_caches()
{
  ${DB_ITERATE_CMD} ${TM_DISKID}
}	# list_disk_caches


function is_TM_db_empty()
{
  local dc=`list_disk_caches`
  if [ "${dc}" = "" ]; then
    TM_DB_IS_EMPTY=1
  else
    TM_DB_IS_EMPTY=0
  fi
}	# is_TM_db_empty


function mknod_diskid()
{
  local serial_num=${1}
  local base=${2}
  echo "${TM_DISKID}/${serial_num}/${base}"
}	# mknod_diskid


function add_new_disk()
{
  local serial_num=${1}
  local ret

  echo "${DB_ADD_CMD} ${TM_DISKID}/${serial_num} string ${serial_num}" >> ${LOG_FILE}
  ${DB_ADD_CMD} ${TM_DISKID}/${serial_num} string ${serial_num} >> ${LOG_FILE} 2>&1
  ret=${?}
  if [ ${ret} -ne 0 ]; then
    echo "ERROR: add_new_disk failed: ${ret}" >> ${LOG_FILE}
    logger -s "${ME}: ERROR: add_new_disk failed: ${ret}"
  else
    date > ${DB_CHANGED_FLAG_FILE}
  fi
  return ${ret}
}	# add_new_disk

function set_diskid_basename()
{
  local serial_num=${2}
  local tm_type=${3}
  local value=${4}
  local node=`mknod_diskid ${serial_num} ${1}`
  local ret

  if [ ${#} -lt 4 ]; then
    echo "ERROR: set_diskid_basename() wrong number of args" >> ${LOG_FILE}
    logger -s "${ME}: ERROR: set_diskid_basename() wrong number of args"
    return 1;
  fi
  echo "${DB_ADD_CMD} ${node} ${tm_type} ${value}" >> ${LOG_FILE}
  ${DB_ADD_CMD} ${node} ${tm_type} ${value} >> ${LOG_FILE} 2>&1
  ret=${?}
  if [ ${ret} -ne 0 ]; then
    echo "ERROR: set_disk_basename ${1} $serial_num $tm_type $value" >> ${LOG_FILE}
    logger -s "${ME}: ERROR: set_disk_basename ${1} $serial_num $tm_type $value"
  else
    date > ${DB_CHANGED_FLAG_FILE}
  fi
  return ${ret}
}	# set_diskid_basename


#
# Skip this disk if the first character is a comment character
#
function skip_this_disk()
{
  local serial_num=${1}
  local filter_serial_num=`echo $serial_num | sed 's/^#//g'`

  if [ "$serial_num" = "$filter_serial_num" ]; then
    return 1;
  else
    return 0;
  fi
}	# skip_this_disk


#
# When pushing the contents of /config/nkn/diskcache-startup.info into the
# TM DB for the first time, we can assume that it is OK to activate and
# cache_enable all disks.
#
function push_disk_info_to_empty_db()
{
  local line
  local serial_num
  local raw_part
  local fs_part
  local set_sector_cnt
  local sub_ptype
  local owner
  local comment_out
  local disk_name
  local stat_name

  if [ ! -e ${CACHE_CONFIG} ]; then
    return;
  fi
  cat ${CACHE_CONFIG} | \
  while read line; do
    set $line
    serial_num=${1}
    raw_part=${2}
    fs_part=${3}
    disk_name=${4}
    stat_name=${5}
    set_sector_cnt=${6}
    sub_ptype=${7}
    owner=${8}
    fwv=${9}

    comment_out=false
    if (skip_this_disk $serial_num); then
      comment_out=true
      serial_num=`echo $serial_num | sed 's/^#*//g'`
    fi

    if (! add_new_disk $serial_num); then
      logger -s "${ME}: ERROR: add_new_disk failed"
      exit 1
    fi

    if (! set_diskid_basename $TM_MISSING_AFTER_BOOT $serial_num bool true); then
      logger -s "${ME}: ERROR: Setting $serial_num $TM_MISSING_AFTER_BOOT true"
      continue;
    fi

    if (! set_diskid_basename $TM_COMMENTED_OUT $serial_num bool $comment_out); then
      logger -s "${ME}: ERROR: Setting $serial_num $TM_COMMENTED_OUT $comment_out"
      continue;
    fi

    if (! set_diskid_basename $TM_RAW_PART $serial_num string ${2}); then
      logger -s "${ME}: ERROR: Setting $serial_num $TM_RAW_PART ${2}"
      continue;
    fi

    if (! set_diskid_basename $TM_FS_PART $serial_num string ${3}); then
      logger -s "${ME}: ERROR: Setting $serial_num $TM_FS_PART ${3}"
      continue;
    fi

    if (! set_diskid_basename $TM_DISK_NAME $serial_num string ${4}); then
      logger -s "${ME}: ERROR: Setting $serial_num $TM_DISK_NAME ${4}"
      continue;
    fi

    if (! set_diskid_basename $TM_STAT_NAME $serial_num string ${5}); then
      logger -s "${ME}: ERROR: Setting $serial_num $TM_STAT_NAME ${5}"
      continue;
    fi

    if (! set_diskid_basename $TM_SET_SEC_CNT $serial_num uint64 ${6}); then
      logger -s "${ME}: ERROR: Setting $serial_num $TM_SET_SEC_CNT ${6}"
      continue;
    fi

    if (! set_diskid_basename $TM_SUBPROV $serial_num string ${7}); then
      logger -s "${ME}: ERROR: Setting $serial_num $TM_SUBPROV ${7}"
      continue;
    fi

    if (! set_diskid_basename $TM_OWNER $serial_num string ${8}); then
      logger -s "${ME}: ERROR: Setting $serial_num $TM_OWNER ${8}"
      continue;
    fi

    if (! set_diskid_basename $TM_ACTIVATED $serial_num bool true); then
      logger -s "${ME}: ERROR: Setting $serial_num $TM_ACTIVATED true"
      continue;
    fi

    if (! set_diskid_basename $TM_CACHE_ENABLED $serial_num bool true); then
      logger -s "${ME}: ERROR: Setting $serial_num $TM_CACHE_ENABLED true"
      continue;
    fi

    if (! set_diskid_basename $TM_FIRMWARE_VER $serial_num string ${9}); then
      logger -s "${ME}: ERROR: Setting $serial_num $TM_FIRMWARE_VER ${9}"
      continue;
    fi

    #
    # Use the MISSING_AFTER_BOOT field as a consistency node.  This means
    # that the MISSING_AFTER_BOOT node will be the last node modified.  If
    # the node is not set to TRUE, then no other fields may be correct.
    #
    # The node was set to TRUE in mark_drives_as_missing
    #
    if (! set_diskid_basename $TM_MISSING_AFTER_BOOT $serial_num bool false); then
      logger -s "${ME}: ERROR: Setting $serial_num $TM_MISSING_AFTER_BOOT false"
      continue;
    fi

  done
}	# push_disk_info_to_empty_db


function stat_disk()
{
  local serial_num=${1}
  local out=`${DB_GET_CMD} ${TM_DISKID}/${serial_num}`
  echo "ret [${out}] from ${DB_GET_CMD} ${TM_DISKID}/${serial_num}" >> ${LOG_FILE}
  if [ "${out}" = "" ]; then
    return 1
  else
    return 0
  fi
}	# stat_disk

function mark_drives_as_missing()
{
  local previous_disks=`list_disk_caches`
  local disk

  for disk in ${previous_disks}; do
    if (! set_diskid_basename $TM_MISSING_AFTER_BOOT $disk bool true); then
      logger -s "${ME}: ERROR: Failed to reset $TM_MISSING_AFTER_BOOT field: $disk"
    fi
  done
}	# mark_drives_as_missing

# This function is called from a while loop in function
# push_disk_info_to_populated_db. The 'if' blocks used in this function
# make use of the 'continue' statement which refers to the while loop from
# where the function is called. And hence, the 'continue' works as it is
# expected to work if used inside a while loop.
function update_existing_disk()
{
    local serial_num=${1}
    local raw_part=${2}
    local fs_part=${3}
    local disk_name=${4}
    local stat_name=${5}
    local set_sector_cnt=${6}
    local sub_ptype=${7}
    local owner=${8}
    local comment_out=${9}
    local fwv=${10}

    if [ ${#} -lt 8 ]; then
      logger -s "${ME}: ERROR: update_existing_disk() wrong # of args: ${#}"
      return
    fi

    if [ $comment_out -eq 0 ]; then
      if (! set_diskid_basename $TM_COMMENTED_OUT $serial_num bool false); then
        logger -s "${ME}: ERROR: Setting $serial_num $TM_COMMENTED_OUT"
        continue; # See the use of 'continue' in the above comment
      fi
    else
      if (! set_diskid_basename $TM_COMMENTED_OUT $serial_num bool true); then
        logger -s "${ME}: ERROR: Setting $serial_num $TM_COMMENTED_OUT"
        continue; # Refer to the comment at the top of the function
      fi
    fi

    # The raw partition can change from boot to boot
    if (! set_diskid_basename $TM_RAW_PART $serial_num string $raw_part); then
      logger -s "${ME}: ERROR: Setting $serial_num $TM_RAW_PART $raw_part"
      continue;
    fi

    # The fs partition can change from boot to boot
    if (! set_diskid_basename $TM_FS_PART $serial_num string $fs_part); then
      logger -s "${ME}: ERROR: Setting $serial_num $TM_FS_PART $fs_part"
      continue;
    fi

    if (! set_diskid_basename $TM_DISK_NAME $serial_num string $disk_name); then
      logger -s "${ME}: ERROR: Setting $serial_num $TM_DISK_NAME $disk_name"
      continue;
    fi

    if (! set_diskid_basename $TM_STAT_NAME $serial_num string $stat_name); then
      logger -s "${ME}: ERROR: Setting $serial_num $TM_STAT_NAME $stat_name"
      continue;
    fi

    if (! set_diskid_basename $TM_OWNER $serial_num string $owner); then
      logger -s "${ME}: ERROR: Setting $serial_num $TM_OWNER $owner"
      continue;
    fi

    # The user may decide to change this value
    if (! set_diskid_basename $TM_SET_SEC_CNT $serial_num uint64 $set_sector_cnt); then
      logger -s "${ME}: ERROR: Setting $serial_num $TM_SET_SEC_CNT $set_sector_cnt"
      continue;
    fi

    if (! set_diskid_basename $TM_FIRMWARE_VER $serial_num string $fwv); then
      logger -s "${ME}: ERROR: Setting $serial_num $TM_FIRMWARE_VER $fwv"
      continue;
    fi

# -----------------------------------------------------------------------------
# COMMENTING THE BELOW 3 LINES - Ramanand 10th Nov 2009
# Since we have added a new CLI "media-cahce disk <disk name> cache-type <type>
# we do not want the sub provider node to be updated on reboots

   # if (! set_diskid_basename $TM_SUBPROV $serial_num string $sub_ptype); then
   #   logger -s "${ME}: ERROR: Setting $serial_num $TM_SUBPROV $sub_ptype"
   #   continue;
   # fi
# -----------------------------------------------------------------------------

    # We do not touch the 'activated' field

    # We do not touch the 'cache enabled' field

    #
    # Use the MISSING_AFTER_BOOT field as a consistency node.  This means
    # that the MISSING_AFTER_BOOT node will be the last node modified.  If
    # the node is not set to TRUE, then no other fields may be correct.
    #
    # The node was set to TRUE in mark_drives_as_missing
    #
    if (! set_diskid_basename $TM_MISSING_AFTER_BOOT $serial_num bool false); then
      logger -s "${ME}: ERROR: Setting $serial_num $TM_MISSING_AFTER_BOOT false"
      continue;
    fi
}	# update_existing_disk

#
# If the TM DB has already been provisioned and we now see a new drive,
# the algorithm is to add this drive as non-active and not-cache-enabled.
#
function add_new_disk_as_inactive()
{
    local serial_num=${1}
    local raw_part=${2}
    local fs_part=${3}
    local disk_name=${4}
    local stat_name=${5}
    local set_sector_cnt=${6}
    local sub_ptype=${7}
    local owner=${8}
    local comment_out=${9}
    local fwv=${10}

    if [ ${#} -lt 8 ]; then
      logger -s "${ME}: ERROR: add_new_disk_as_inactive() wrong # of args: ${#}"
      return
    fi

    if (! add_new_disk $serial_num) then
      logger -s "${ME}: ERROR: add_new_disk failed"
      exit 1
    fi

    if [ $comment_out -eq 0 ]; then
      if (! set_diskid_basename $TM_COMMENTED_OUT $serial_num bool false); then
        logger -s "${ME}: ERROR: Setting $serial_num $TM_COMMENTED_OUT"
      fi
    else
      if (! set_diskid_basename $TM_COMMENTED_OUT $serial_num bool true); then
        logger -s "${ME}: ERROR: Setting $serial_num $TM_COMMENTED_OUT"
      fi
    fi

    if (! set_diskid_basename $TM_RAW_PART $serial_num string $raw_part ); then
      logger -s "${ME}: ERROR: Setting $serial_num $TM_RAW_PART $raw_part"
    fi

    if (! set_diskid_basename $TM_FS_PART $serial_num string $fs_part ); then
      logger -s "${ME}: ERROR: Setting $serial_num $TM_FS_PART $fs_part"
    fi

    if (! set_diskid_basename $TM_DISK_NAME $serial_num string $disk_name ); then
      logger -s "${ME}: ERROR: Setting $serial_num $TM_DISK_NAME $disk_name"
    fi

    if (! set_diskid_basename $TM_STAT_NAME $serial_num string $stat_name ); then
      logger -s "${ME}: ERROR: Setting $serial_num $TM_STAT_NAME $stat_name"
    fi

    if (! set_diskid_basename $TM_SET_SEC_CNT $serial_num uint64 $set_sector_cnt ); then
      logger -s "${ME}: ERROR: Setting $serial_num $TM_SET_SEC_CNT $set_sector_cnt"
    fi

    if (! set_diskid_basename $TM_SUBPROV $serial_num string $sub_ptype); then
      logger -s "${ME}: ERROR: Setting $serial_num $TM_SUBPROV $sub_ptype"
    fi

    if (! set_diskid_basename $TM_OWNER $serial_num string $owner); then
      logger -s "${ME}: ERROR: Setting $serial_num $TM_OWNER $owner"
    fi

    if (! set_diskid_basename $TM_ACTIVATED $serial_num bool false); then
      logger -s "${ME}: ERROR: Setting $serial_num $TM_ACTIVATED true"
    fi

    if (! set_diskid_basename $TM_CACHE_ENABLED $serial_num bool false); then
      logger -s "${ME}: ERROR: Setting $serial_num $TM_CACHE_ENABLED true"
    fi

    if (! set_diskid_basename $TM_FIRMWARE_VER $serial_num string $fwv); then
      logger -s "${ME}: ERROR: Setting $serial_num $TM_FIRMWARE_VER $fwv"
      continue;
    fi

    #
    # Use the MISSING_AFTER_BOOT field as a consistency node.  This means
    # that the MISSING_AFTER_BOOT node will be the last node modified.  If
    # the node is not set to TRUE, then no other fields may be correct.
    #
    # The node was set to TRUE in mark_drives_as_missing
    #
    if (! set_diskid_basename $TM_MISSING_AFTER_BOOT $serial_num bool false); then
      logger -s "${ME}: ERROR: Setting $serial_num $TM_MISSING_AFTER_BOOT false"
    fi
}	# add_new_disk_as_inactive


#
# Take the information from /config/nkn/diskcache-startup.info and push
# it into the TM DB.  When doing this, we need to consider files which
# have been commented out.
#
# For disks which already exist in the DB, update everything except the
# activated and cache_enabled state.  These need to be persistent across
# reboots.
#
# For disks which do not exist in the DB, add them to the DB but do
# not activate or cache_enable them because they are new and could have
# come from another system.  Users must explicitly add them to the DB.
# If these disks have content on them already, users need to decide
# if they want to use the content or wipe it out.
#
function push_disk_info_to_populated_db()
{
  local line
  local serial_num
  local raw_part
  local fs_part
  local set_sector_cnt
  local sub_ptype
  local owner
  local comment_out
  local disk_name
  local stat_name

  if [ ! -e ${CACHE_CONFIG} ]; then
    return;
  fi

  mark_drives_as_missing

  cat ${CACHE_CONFIG} | \
  while read line; do
    set $line
    serial_num=${1}
    raw_part=${2}
    fs_part=${3}
    disk_name=${4}
    stat_name=${5}
    set_sector_cnt=${6}
    sub_ptype=${7}
    owner=${8}
    fwv=${9}

    comment_out=0
    if (skip_this_disk $serial_num); then
      comment_out=1
      serial_num=`echo $serial_num | sed 's/^#*//g'`
    fi

    if (stat_disk $serial_num); then
      update_existing_disk $serial_num $raw_part $fs_part $disk_name $stat_name $set_sector_cnt $sub_ptype $owner $comment_out $fwv
    else
      add_new_disk_as_inactive $serial_num $raw_part $fs_part $disk_name $stat_name $set_sector_cnt $sub_ptype $owner $comment_out $fwv
    fi

  done
}	# push_disk_info_to_populated_db


# Taken from initdisks.sh
Setup_A()
{
  DEVICES="${1}a"
  SLOT_0="${1}a"
  local CNT=1
  #        1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 51; do
  for A in b c d e f g h i j  k  l  m  n  o  p  q  r  s  t  u  v  w  x  y  z aa ab ac ad ae af ag ah ai aj ak al am an ao ap aq ar as at au av aw ax ay az; do
    eval SLOT_${CNT}=${1}${A}
    DEVICES="${DEVICES} ${1}${A}"
    local CNT=`expr ${CNT} + 1`
  done
}

# Taken from initdisks.sh
Setup_N()
{
  DEVICES="${1}0"
  SLOT_0="${1}0"
  local CNT=1

  for N in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
    eval SLOT_${CNT}=${1}${N}
    DEVICES="${DEVICES} ${1}${N}"
    local CNT=`expr ${CNT} + 1`
  done
}


#
# For a DM2 style cache, we look for the free space bitmap.
#
# Taken from initdisks.sh
#
function is_DM2_cache()
{
  local MNT=${1}

  if [ -e ${MNT}/${DM2_FREEBLKS} ]; then
    return 0;
  else
    return 1;
  fi
}	# is_DM2_cache


# Taken from initdisks.sh
function get_root_dev()
{
  # Get the device name of the root drive, to decide the device naming scheme.
  local SYSDATA_DISK_DEV_NAME=`cat ${SYSDATA_DISK_DEV_INFO_FILE}`

  # Now on VXA2100 systems, we are not guaranteed that the root is /dev/sda
  case _${SYSDATA_DISK_DEV_NAME} in
    _/dev/sd*)        Setup_A /dev/sd ;;
    _/dev/cciss/c0d0) Setup_N /dev/cciss/c0d ;;
    # Adding case to have mirrored disk name convention PR: 838382
    _/dev/md*) Setup_A /dev/sd ;;
    _/dev/*)  logger -s "${ME}: WARNING: Unsupported disk driver for cache filesystem: ${SYSDATA_DISK_DEV_NAME}"
      # Guess at naming scheme of the device by looking at the last character.
      case ${SYSDATA_DISK_DEV_NAME} in
	*a)
	  Setup_A ${SYSDATA_DISK_DEV_NAME%a}
	  ;;
	*0)
	  Setup_N ${SYSDATA_DISK_DEV_NAME%0}
	  ;;
	*)
        # Do not know the naming scheme, so just use the root drive.
	DEVICES="${SYSDATA_DISK_DEV_NAME}"
	SLOT_0="${SYSDATA_DISK_DEV_NAME}"
	;;
      esac
      ;;
    *)  logger -s "${ME}: ERROR: Sysdata disk drive name not specified."
      # This should not happen.
      # Just in case, default to SDA naming scheme.
      SYSDATA_DISK_DEV_NAME=/dev/sda
      Setup_A /dev/sd
      ;;
  esac
  echo ${SYSDATA_DISK_DEV_NAME}
}	# get_root_dev


function setup_one_non_root_disk()
{
  local DISK=${1}
  local PART1
  local PART2
  local OWNER="DM2"
  local FWV
  local TYPE
  local RETVAL
  local BLKSZ
  local PAGESZ

  if is_smart_array; then
    PART1=${DISK}p1
    PART2=${DISK}p2
  else
    PART1=${DISK}1
    PART2=${DISK}2
  fi

  # Create partitions for disks with no existing partitions
  parted --script ${DISK} print > /dev/null 2>&1
  if [ $? -eq 1 ]; then
    TYPE=`get_drive_type ${DISK}`
    RETVAL=`get_disk_block_page_sizes ${TYPE}`
    BLKSZ=`echo "$RETVAL" | awk '{print $1}'`
    PAGESZ=`echo "$RETVAL" | awk '{print $2}'`
    ${INITCACHE} ${DISK} ${BLKSZ} ${PAGESZ} ${PERCENT}
  fi

  logger -s "${ME}:    Adding disk cache: ${DISK}."
  local SERIAL=`get_serial ${DISK}`
  local FWV=`get_firmware_version ${DISK}`
  local STAT_NAME=`get_block_disk_name ${DISK} ${PART1}`
  local SZ=`get_part_size ${DISK} 1`
  local TYPE=`get_drive_type ${DISK}`
  echo "${SERIAL}  ${PART1}  ${PART2}  ${DISK}  ${STAT_NAME}  ${SZ}  ${TYPE}  DM2  ${FWV}" >> ${CACHE_CONFIG}
  if (! set_diskid_basename $TM_MISSING_AFTER_BOOT $serial_num bool true); then
    logger -s "${ME}: ERROR: Setting $serial_num $TM_MISSING_AFTER_BOOT true"
    continue;
  fi
  add_new_disk_as_inactive ${SERIAL} ${PART1}  ${PART2}  ${DISK}  ${STAT_NAME}  ${SZ} ${TYPE} ${OWNER} 0 ${FWV}
}	# setup_one_non_root_disk


#
# Push disk drives which are not found in the TM DB into the TM DB and
# /config/nkn/diskcache-startup.info.
#
# the basic algorithm:
#
#    Populate $DEVICES by finding root device (from initdisks.sh)
#    for all DISK in $DEVICES; do
#        if DISK is in TM DB, skip it
#        if DISK is not present in filesystem, skip it
#        Add disk to /config/nkn/diskcache-startup.info & TM DB
#           (Note: Adding code to TM DB uses code stolen from initdisks.sh)
#
# At some point, the common code used in this code path should be put into
# a shared library and shared with initdisks.sh
#
function push_new_disk_info_to_populated_db()
{
  get_root_dev > /dev/null 2>&1
  # $DEVICES now has the valid disk drives to search.

  for DISK in ${DEVICES}; do
    local serial_num=`get_serial ${DISK}`

    # Note that the root disk should always be found with stat_disk().
    if stat_disk ${serial_num}; then
      # If the serial number already exists in the TM DB, then skip this disk
      continue;
    fi
    # Linux should not have the device present if no drive exists
    if ! stat -t ${DISK} > /dev/null 2>&1; then
      continue;
    fi

      if is_system_disk ${DISK}; then
	# Skip system data devices on VXA systems
	continue;
      fi
    if is_production_vxa; then
      if is_eUSB_device ${DISK}; then
	# Skip eUSB devices on VXA systems
	continue;
      fi
    fi

    setup_one_non_root_disk ${DISK}
  done
}	# push_new_disk_info_to_populated_db


#--------------------------------------------------------------
#
#  MAIN LOGIC BEGINS HERE 
#
#--------------------------------------------------------------

ME=`basename "${0}"`

if is_invalid_config; then
  exit 0;
fi

# Create the cache partitions and filesystems
rm -f ${LOG_FILE}
setup_db_cmd
if [ ${?} -ne 0 ]; then
  logger -s "${ME}: ERROR: cannot write to configuration DB"
  exit 1
fi

if [ -z "${1}" ]; then
  ARG1=initdata
else
  ARG1=${1}
  shift
fi
ARG2=${1}

rm -f ${DB_CHANGED_FLAG_FILE}
case "${ARG1}" in
  initdata)
    if [ ! -e ${CACHE_CONFIG} ]; then
      logger -s "${ME}: ERROR: No ${CACHE_CONFIG} file to use."
      exit 1;
    fi
    logger -s "${ME}: Pushing disk cache information into DB."
    is_TM_db_empty
    if [ $TM_DB_IS_EMPTY -eq 1 ]; then
      push_disk_info_to_empty_db
    else
      push_disk_info_to_populated_db
    fi
    ;;
  newdisks)
    logger -s "${ME}: Put new disk cache information into DB."
    push_new_disk_info_to_populated_db
    ;;
  *)
    logger -s "${ME}: ERROR: Illegal input: ${ARG1}"
    logger -s "${ME}: Specify: [ initdata | newdisks ] [ no-save-config ]"
    exit 1
    ;;
esac

if [ -f ${DB_CHANGED_FLAG_FILE} ]; then
  if [ "${ARG2}" = "no-save-config" ]; then
    logger -s "${ME}: Not saving DB change to active configuration."
    rm -f ${DB_CHANGED_FLAG_FILE}
  else
    if [ "${DB_CMD}" = "${MDREQ}" ]; then
      # Make DB change persistent
      ${MDREQ} action /mgmtd/db/save > /nkn/tmp/write_diskinfo_to_db.save_db 2>&1
      RET=${?}
    else
      # When using mddbreq, each update is to the active configuration.
      RET=0
    fi
    if [ $RET -ne 0 ]; then
      logger -s "${ME}: ERROR: Failed saving DB change to active configuration: $RET"
    else
      logger -s "${ME}: Saved DB change to active configuration."
    fi
  fi
else
  logger -s "${ME}: No DB change."
fi

exit 0

#
# End of write_diskinfo_to_db.sh
#
