#! /bin/bash
#
#	File : cache_disk_lib.sh
#
#	Description : This file has a bunch of disk cache utility shell 
#		functions that are used by different scripts
#
#	Created By : Michael Nishimoto (miken@nokeena.com)
#
#	Created On : 25 February, 2009
#
#	Copyright (c) Nokeena Inc., 2009
#

PATH=$PATH:/sbin
MDREQ=/opt/tms/bin/mdreq
CACHE_CONFIG=/config/nkn/diskcache-startup.info
NKNCNT=/opt/nkn/bin/nkncnt

# TallMaple Nodes
TM_DISKID=/nkn/nvsd/diskcache/config/disk_id
MFG_DB_PATH=/config/mfg/mfdb
MFG_ROOT_DEV=/mfg/mfdb/disk_caches/root_disk_dev
TM_ACTIVATED=activated
TM_CACHE_ENABLED=cache_enabled
TM_COMMENTED_OUT=commented_out
TM_FS_PART=fs_partition
TM_MISSING_AFTER_BOOT=missing_after_boot
TM_RAW_PART=raw_partition
TM_SET_SEC_CNT=set_sector_count
TM_SUBPROV=sub_provider

function get_options()
{
  while getopts ":D:" options; do
    case $options in
      D ) DOMAIN=$OPTARG
	;;
      h ) usage
	;;
      \? ) usage
	;;
      * ) usage
	;;
    esac
  done

  if [ "${DOMAIN}" != "" ]; then
    shift;shift;
  fi
}	# get_options


function list_disk_caches()
{
  ${MDREQ} -v query iterate - ${TM_DISKID}
}	# list_disk_caches


function map_prov_id_to_prov_string()
{
  case _${1} in
    _1) echo "SSD" ;;
    _5) echo "SAS" ;;
    _6) echo "SATA" ;;
    *) echo "ERROR" ;;
  esac
}

function disk_get()
{
  local serial_num=$1
  local node=$2
  local val=`${MDREQ} -v query get - ${TM_DISKID}/${serial_num}/${node}`
  echo ${val}
}


function disk_missing_after_boot()
{
  local serial_num=$1
  local val=`disk_get ${serial_num} ${TM_MISSING_AFTER_BOOT}`
  if [ "${val}" = "true" ]; then
    return 0
  else
    return 1
  fi
}	# disk_missing_after_boot


function disk_commented_out()
{
  local serial_num=$1
  local val=`disk_get ${serial_num} ${TM_COMMENTED_OUT}`
  if [ "${val}" = "true" ]; then
    return 0
  else
    return 1
  fi
}	# disk_commented_out


function disk_activated()
{
  local serial_num=$1
  local val=`disk_get ${serial_num} ${TM_ACTIVATED}`
  if [ "${val}" = "true" ]; then
    return 0
  else
    return 1
  fi
}	# disk_commented_out


function disk_enabled()
{
  local serial_num=$1
  local val=`disk_get ${serial_num} ${TM_CACHE_ENABLED}`

  if [ "${val}" = "true" ]; then
    return 0
  else
    return 1
  fi
}	# disk_commented_out


function disk_prov_match()
{
  local serial_num=$1
  local need_prov_str=$2
  local got_prov_str=`disk_get ${serial_num} ${TM_SUBPROV}`

  if [ "${need_prov_str}" = "${got_prov_str}" ]; then
    return 0
  else
    return 1
  fi
}	# disk_prov_match


function disk_running()
{
  local disk_name=$1
  local state=`${NKNCNT} -t 0 -s ${disk_name}.dm2_state | grep dm2_state | awk '{print $3}'`

  if [ "${state}" = "10" ]; then
    return 0
  else
    return 1
  fi
}	# disk_running


function get_mountpt()
{
  local need_fs_part=$1
  local fs_part
  local mountpt

  cat /proc/mounts | \
  while read line; do
    set $line
    fs_part=$1
    mountpt=$2
    if [ "${fs_part}" = "${need_fs_part}" ]; then
      echo ${mountpt}
      return
    fi
  done
}	# get_mountpt


function get_diskname()
{
  local need_fs_part=$1
  local fs_part
  local mountpt
  local dn=foo

  cat /proc/mounts | \
  while read line; do
    set $line
    fs_part=$1
    mountpt=$2
    if [ "${fs_part}" = "${need_fs_part}" ]; then
      dn=`echo ${mountpt} | sed 's@/nkn/mnt/@@g'`
      echo ${dn}
      # This is not a mistake.  A single 'return' or 'break' doesnot get me
      # out of the loop.  I think that it's the pipe that is causing the
      # problem.  'break 2' seems to work.
      break
    fi
  done
}	# get_diskname


#
# End of cache_disk_lib.sh
#
