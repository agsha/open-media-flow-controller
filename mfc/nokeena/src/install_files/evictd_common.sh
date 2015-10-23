#! /bin/bash
#
#	File : evictd_common.sh
#
#	Description : This script evicts objects from the disk cache
#
#	Originally Written By : Michael Nishimoto (miken@nokeena.com)
#
#	Originally Created On : 25 February, 2009 (cache_evictd.sh)
#
#       Code split/renamed On : 28 January, 2011
#
#	Copyright (c) Nokeena Inc., 2009
#

MDREQ=/opt/tms/bin/mdreq
CACHE_CONFIG=/config/nkn/diskcache-startup.info
AM_PROV_THRESH=glob_dm2_prov_hot_threshold
NKNCNT=/opt/nkn/bin/nkncnt
EVICT_TIER=/opt/nkn/bin/cache_provider_evict
CACHE_EVICT=/opt/nkn/bin/cache_evict.sh
EVICT_CYCLE=$[60]

# TallMaple Nodes
TM_DISKID=/nkn/nvsd/diskcache/config/disk_id
TM_SYNC_INTR=/nkn/nvsd/buffermgr/config/attr_sync_interval
TM_EXT_WMRK_SATA=/nkn/nvsd/diskcache/config/watermark/external/sata
TM_EXT_WMRK_SAS=/nkn/nvsd/diskcache/config/watermark/external/sas
TM_EXT_WMRK_SSD=/nkn/nvsd/diskcache/config/watermark/external/ssd
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


function get_seed()
{
  local prov=$1
  local hotness_seed

  $NKNCNT -s ${AM_PROV_THRESH}_${prov} -t 0 | grep -q ${AM_PROV_THRESH}
  if [ $? -eq 1 ]; then
    logger -s "ERROR: Can not evict from Provider=${prov}: Missing ${AM_PROV_THRESH}_${prov}"
    echo "0"
  fi

  hotness_seed=`$NKNCNT -s ${AM_PROV_THRESH}_${prov} -t 0 | grep ${AM_PROV_THRESH} | awk '{print $3}'`
  echo "${hotness_seed}"
}	# get_seed

function sync_interval_get()
{
  local val=`${MDREQ} -v query get - ${TM_SYNC_INTR}`
  echo ${val}
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


function skip_disk()
{
  local serial_num=$1
  local prov_str=$2
  local disk_name=$3

  if ! disk_prov_match ${serial_num} ${prov_str}; then
    return 0
  fi
  if disk_missing_after_boot ${serial_num}; then
    return 0
  fi
  if disk_commented_out ${serial_num}; then
    return 0
  fi
  if ! disk_activated ${serial_num}; then
    return 0
  fi
  if ! disk_enabled ${serial_num}; then
    return 0
  fi
  if ! disk_running $disk_name; then
    return 0
  fi
  return 1
}	# skip_disk


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


function get_high_water_mark()
{
  local prov=$1
  local tm_node
  local	val

  case _${prov} in
    _1)
    tm_node=$TM_EXT_WMRK_SSD/high
    val=`${MDREQ} -v query get - ${tm_node}`
    if (( "$val" < 0 || "$val" > 100 )); then
	echo "90"
    else
	echo ${val}
    fi
    ;;
    _5)
    tm_node=$TM_EXT_WMRK_SAS/high
    val=`${MDREQ} -v query get - ${tm_node}`
    if (( "$val" < 0 || "$val" > 100 )); then
	echo "90"
    else
	echo ${val}
    fi
    ;;
    _6)
    tm_node=$TM_EXT_WMRK_SATA/high
    val=`${MDREQ} -v query get - ${tm_node}`
    if (( "$val" < 0 || "$val" > 100 )); then
	echo "90"
    else
	echo ${val}
    fi
    ;;
    *)
	echo "90"
	logger "get_high_water_mark: Illegal provider: ${prov}"
	;;
  esac
}	# get_high_water_mark


function get_low_water_mark()
{
  local prov=$1
  local tm_node
  local	val

  case _${prov} in
    _1)
    tm_node=$TM_EXT_WMRK_SSD/low
    val=`${MDREQ} -v query get - ${tm_node}`
    if (( "$val" < 0 || "$val" > 100 )); then
	echo "85"
    else
	echo ${val}
    fi
    ;;
    _5)
    tm_node=$TM_EXT_WMRK_SAS/low
    val=`${MDREQ} -v query get - ${tm_node}`
    if (( "$val" < 0 || "$val" > 100 )); then
	echo "85"
    else
	echo ${val}
    fi
    ;;
    _6)
    tm_node=$TM_EXT_WMRK_SATA/low
    val=`${MDREQ} -v query get - ${tm_node}`
    if (( "$val" < 0 || "$val" > 100 )); then
	echo "85"
    else
	echo ${val}
    fi
    ;;
    *)  echo "85"
	logger "get_low_water_mark: Illegal provider: ${prov}"
	;;
  esac
}	# get_low_water_mark


function evict_provider()
{
  local prov=$1
  local temp_seed=`get_seed ${prov}`
  if [ "${temp_seed}" = "ERROR" ]; then
    return;
  fi
  local prov_str=`map_prov_id_to_prov_string ${prov}`
  local disks=`list_disk_caches`
  local diskname
  local fs_part
  local mountpt
  local free_resv_blocks
  local block_sz
  local disk_total_blocks=0
  local disk_free_resv_blocks=0
  local free_percent
  local del_blocks=0
  local curline
  local nlines
  local evict_high_water_mark
  local evict_low_water_mark
  local tmp_file
  local file_list=""
  local evict_file
  local sync_interval

  sync_interval=`sync_interval_get`; # in seconds
  for disk in ${disks}; do
    fs_part=`disk_get ${disk} ${TM_FS_PART}`
    diskname=`get_diskname ${fs_part}`
    if skip_disk ${disk} ${prov_str} ${diskname}; then
      continue;
    fi
    disk_total_blocks=`${NKNCNT} -t 0 -s ${diskname}.dm2_total_blocks | grep dm2_total_blocks | awk '{print $3}'`
    disk_free_resv_blocks=`${NKNCNT} -t 0 -s ${diskname}.dm2_free_resv_blocks | grep dm2_free_resv_blocks | awk '{print $3}'`
    block_sz=`${NKNCNT} -t 0 -s ${diskname}.dm2_block_size | grep dm2_block_size | awk '{print $3}'`

    mountpt=`get_mountpt ${fs_part}`

    if [ ${disk_total_blocks} -eq 0 ]; then
      free_percent=100
    else
      (( free_percent = 100*disk_free_resv_blocks / disk_total_blocks ))
    fi

    evict_high_water_mark=`get_high_water_mark $prov`
    evict_low_water_mark=`get_low_water_mark $prov`

    if (( 100 - free_percent >= evict_high_water_mark )); then
      (( del_blocks =
	    (100 - free_percent - evict_low_water_mark) * disk_total_blocks / 100 ))
# Generate the URI list that needs to be evicted
      tmp_file=/nkn/tmp/dm2_disk_evict.${prov}.${diskname}

      # Validate that we don't already have this disk eviction queued
      /opt/nkn/bin/nknfqueue -Q /tmp/FMGR.queue | grep -q "${tmp_file}"
      if [ $? -ne 0 ]; then
	file_list="${file_list} ${tmp_file}"
	mv -f $tmp_file ${tmp_file}.last
	$EVICT_TIER -s $temp_seed -d $del_blocks -p $prov -t $sync_interval ${mountpt} > $tmp_file &
      else
	logger "Discarding eviction for ${tmp_file}"
      fi
    fi
  done
  wait
# Attempt to evict the URIs with 'cache_evict.sh'. The -F option takes the
# URI delete list file as athe argument.
  for evict_file in ${file_list}; do
    ${CACHE_EVICT} -e ${prov} -F $evict_file
  done


}	# evict_provider

