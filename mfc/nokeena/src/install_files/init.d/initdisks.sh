#! /bin/bash
#
#	File : initdisks.sh
#
#	Description : This script sets up Disk caches
#		This is run from init.d at run level 3, after mdinit 
#		(which starts mgmtd)
#
#	Created By : Michael Nishimoto (miken@nokeena.com)
#
#	Created On : 11 November, 2008
#
#	Copyright (c) Nokeena Inc., 2008
#

TESTING=0
PATH=$PATH:/sbin
if [ $TESTING -eq 0 ]; then
  CACHE_CONFIG=/config/nkn/diskcache-startup.info
else
  CACHE_CONFIG=/config/nkn/diskcache-startup.info.miken
fi

# This file holds common script functions which perform disk identification
# operations for each type of controller.
if [ $TESTING -eq 0 ]; then
  source /opt/nkn/bin/disks_common.sh
elif [ -e /tmp/disks_common.sh ]; then
  source /tmp/disks_common.sh
else
  source /opt/nkn/bin/disks_common.sh
fi

ROOT_RAWFILE=/config/nkn/root_cache_raw
ROOT_FSFILE=/config/nkn/root_cache_fs
FIRSTCACHE_RAWFILE=/config/nkn/first_cache_raw
FIRSTCACHE_FSFILE=/config/nkn/first_cache_fs
ROOT_RAW=""
ROOT_FS=""
INIT_CACHE_PS_DISKS_FILE=/config/nkn/ps-disks-list.txt
INIT_CACHE_PS_PDISKS_FILE=/config/nkn/ps-pdisks-list.txt
INIT_CACHE_OPT_FILE=/config/nkn/diskcache-init.opt
EUSB_DISK_DEV_INFO_FILE=/config/nkn/eusb-disk.info
INITCACHE=/opt/nkn/bin/initcache_dm2_ext3.sh
INITROOTCACHE=/opt/nkn/bin/initrootcache_dm2_ext3.sh
INIT_PS_DISK=/opt/nkn/bin/init_ps_disk.sh
REINIT_CONFIG_DISKS=/opt/nkn/bin/reinit_config_disks.sh
WRITE_DISKINFO_TO_DB=/opt/nkn/bin/write_diskinfo_to_db.sh

MFG_DB_PATH=/config/mfg/mfdb
MFG_ROOT_DEV=/mfg/mfdb/disk_caches/root_disk_dev
MFG_ROOT_CACHE_RAW=/mfg/mfdb/disk_caches/root_cache_raw
MFG_ROOT_CACHE_FS=/mfg/mfdb/disk_caches/root_cache_fs
MFG_DISK_REINIT=/mfg/mfdb/disk_caches/reinit
SYSDATA_DISK_DEV_NAME=
EUSB_DISK_DEV_NAME=
#default sizing
PERCENT=0
TMPMNT=/nkn/tmp_mnt
DM_MAPFILE=nkn_map_file.txt
DM2_FREEBLKS=freeblks
ROOT_CACHE_FOUND=0

# Local functions

#
# For a DM style cache, we only look for the existence of the map file.
#
function is_DM_cache()
{
  local MNT=$1

  if [ -e ${MNT}/${DM_MAPFILE} ]; then
    return 0;
  else
    return 1;
  fi
}	# is_DM_cache


#
# For a DM2 style cache, we look for the free space bitmap.
#
function is_DM2_cache()
{
  local MNT=$1

  if [ -e ${MNT}/${DM2_FREEBLKS} ]; then
    return 0;
  else
    return 1;
  fi
}	# is_DM2_cache


#
# Disble write caching for all disks, to be the safest.
#
function disable_write_caching()
{
  local DISK=
  local RV=
  local LOG_SAVE=/log/init/disable_write_caching.log
  local CMD=false
  local CMD2=
  local T=${SLOT_0}
  local SLOT
  local N

  if is_pacifica_hw; then
    T=pacifica
  elif is_megaraid; then
    T=megaraid
  elif is_adaptec_vxa; then
    T=vxa
  elif is_smart_array; then
    T=smart
  fi
 
  case ${T} in

  pacifica)
    # Added case for pacifica since sdparm for disabling write cache is not
    # working but hdparm for the same works
    logger -s "[`date`] ${ME}: Disable write cache for /dev/sd devices on" \
      " pacifica blade"  2>&1 | tee -a ${LOG_SAVE}
    CMD="hdparm -W0"
    ;;

  /dev/hd*)
    logger -s "[`date`] ${ME}: Disable write cache for /dev/hd devices" \
      2>&1 | tee -a ${LOG_SAVE}
    CMD="hdparm -W0"
    ;;

  /dev/sd*)
    logger -s "[`date`] ${ME}: Disable write cache for /dev/sd devices" \
      2>&1 | tee -a ${LOG_SAVE}
    CMD="/opt/nkn/bin/sdparm --set WCE=0"
    ;;

  smart | /dev/cciss/*)
    # Smart Array
    logger -s "[`date`] ${ME}: Disable write cache on SmartArray controller" \
      2>&1 | tee -a ${LOG_SAVE}
    # Note: hpacucli needs to have /opt/compaq/cpqacuxe/ on a writeable
    # filesystem. Fixups.sh creates a symlink for this.
    # If we were on a shared system, we could only disable write caches
    # for drives that we own.

    CMD="/usr/sbin/hpacucli ctrl slot="
    CMD2=" modify dwc=disable"
    /usr/sbin/hpacucli ctrl all show  | grep Slot > /var/tmp/cciss-detect.txt
    SLOT=0
    while [ ${SLOT} -le 16 ]; do
      grep "Slot ${SLOT}" /var/tmp/cciss-detect.txt
      if [ ${?} -ne 0 ]; then
	SLOT=$[$SLOT + 1]
        continue
      fi
      logger -s "${ME}: ${CMD}${SLOT}${CMD2}" 2>&1 | tee -a ${LOG_SAVE}
      if [ $TESTING -eq 0 ]; then
	${CMD}${SLOT}${CMD2} >> ${LOG_SAVE} 2>&1
	RV=${?}
	if [ ${RV} -eq 0 ]; then
	  OUT=`${CMD}${SLOT} modify dwc=? | grep current`
	  logger -s "${ME}: ${OUT}" 2>&1 | tee -a ${LOG_SAVE}
	fi
      fi
      SLOT=$[$SLOT + 1]
    done
    return
    ;;

  megaraid)
    # There does not seem to be a MegaCLi option to control enable/disable write cache.
    # All megaRAID devices have write cache off by default.
    logger -s "${ME}: WARNING: unable to detect write cache settings for MegaRAID devices" \
    logger -s "${ME}: WARNING: unable to automatically disable write cache for MegaRAID devices" \
      2>&1 | tee -a ${LOG_SAVE}
    return
    ;;

  vxa)
    # Juniper hardware
    # If we were on a shared system, we could only disable write caches
    # for drives that we own.
    logger -s "[`date`] ${ME}: Disable write cache for VXA devices" 2>&1 \
      | tee -a ${LOG_SAVE}
    # Only issue commands for "hard drives".
    # Assume that we own all "hard drives".
    # Assume that we only have one controller.
    # csplit will create separate files with each physical device
    OUT="${ADPCLI_CONF_DISK}"
    /usr/StorMan/arcconf getconfig 1 pd | csplit -s -f ${OUT} -k -z - "/Device #/" {20} > /dev/null 2>&1
    for N in `seq 16`; do
      local F=`printf "${OUT}%02d\n" ${N}`
      if [ ! -f ${F} ]; then
	continue;
      fi
      if (grep -i -q "hard drive" $F); then
	local NUM=`grep -i "Channel,Device" $F | awk '{print $4}' | sed -e 's/(.*)//g' -e 's/0,//'`
	if (grep -i "Write Cache" $F | grep -q -i disabled); then
	  logger -s "${ME}: Device 0 ID ${NUM} is already WRITE-THROUGH" 2>&1 | tee -a ${LOG_SAVE}
	else
	  CMD="/usr/StorMan/arcconf setcache 1 device 0 $NUM wt"
	  ${CMD} >> ${LOG_SAVE} 2>&1
	  RV=$?
	  if [ ${RV} -ne 0 ]; then
	    logger -s "${ME}: ${CMD} failed (${RV})" 2>&1 | tee -a ${LOG_SAVE}
	  else
	    logger -s "${ME}: ${CMD}" 2>&1 | tee -a ${LOG_SAVE}
	  fi
        fi
      fi
    done
    return
    ;;

  *)
    logger -s "${ME}: WARNING: unable to disable write cache for ${SLOT_0} devices" \
      2>&1 | tee -a ${LOG_SAVE}
    return
    ;;

  esac
  for DISK in ${DEVICES}; do
    if ls -l ${DISK} > /dev/null 2>&1; then
      logger -s "${ME}: ${CMD} ${DISK}" 2>&1 | tee -a ${LOG_SAVE}
      ${CMD} ${DISK} >> ${LOG_SAVE} 2>&1
      RV=${?}
      if [ ${RV} -ne 0 ]; then
        logger -s "${ME}: ERROR: Disable write cache command failed ${DISK} (${RV})" \
          2>&1 | tee -a ${LOG_SAVE}
      fi
    fi
  done
}	# disable_write_caching


#
# Find the root cache and set it in ROOT_RAW if found.  Also setup ROOT_FS.
#
function find_root_cache()
{
  local DISK
  local PARTS
  local LABEL
  local PART

  # For now a MFG script is creating this file.  This script can no longer
  # calculate this value from the root disk value.
  if [ -e ${ROOT_RAWFILE} ]; then
    ROOT_RAW=`cat ${ROOT_RAWFILE}`
  fi
  if [ -e ${ROOT_FSFILE} ]; then
    ROOT_FS=`cat ${ROOT_FSFILE}`
  fi
  if [ "${ROOT_RAW}" != "" -a "${ROOT_FS}" != "" ]; then
    logger -s "${ME}:     Sysdata cache raw partition: ${ROOT_RAW}"
    logger -s "${ME}:     Sysdata cache filesystem partition: ${ROOT_FS}"
    return 1
  fi
  if [ "${ROOT_RAW}" = "" -a "${ROOT_FS}" = "" ]; then
    if [ -e ${FIRSTCACHE_RAWFILE} ]; then
      ROOT_RAW=`cat ${FIRSTCACHE_RAWFILE}`
    fi
    if [ -e ${FIRSTCACHE_FSFILE} ]; then
      ROOT_FS=`cat ${FIRSTCACHE_FSFILE}`
    fi
  fi
  if [ "${ROOT_RAW}" != "" -a "${ROOT_FS}" != "" ]; then
    logger -s "${ME}:     First cache raw partition: ${ROOT_RAW}"
    logger -s "${ME}:     First cache filesystem partition: ${ROOT_FS}"
    return 1
  fi

  if [ "${ROOT_RAW}" = "" -a "${ROOT_FS}" = "" ]; then
    logger -s "${ME}:     No cache on system drive"
    return 0
  fi

  if [ "${ROOT_RAW}" == "" ]; then
    logger -s "${ME}: ERROR: Sysdata cache raw partition: NOT FOUND."
    logger -s "${ME}: Sysdata cache filesystem: ${ROOT_FS}"
  fi
  if [ "${ROOT_FS}" == "" ]; then
    logger -s "${ME}: ERROR: Sysdata cache filesystem: NOT FOUND."
    logger -s "${ME}: Sysdata cache raw partition: ${ROOT_RAW}"
  fi

  # Make 2 root variables consistent, so we don't create a root cache
  ROOT_RAW=
  ROOT_FS=
  return 0
}	# find_root_cache

function setup_ps_devices()
{
  local PS_DISK
  local TYPE
  local PART1
  local PART2
  local SERIAL
  local STATNM
  local SZ
  local FWV

  [ ! -s ${INIT_CACHE_PS_DISKS_FILE} ] && return
  for PS_DISK in `cat ${INIT_CACHE_PS_DISKS_FILE}`; do
    case ${PS_DISK} in
    /dev/cciss/c*) PS_PDISK=${PS_DISK}p ;;
    *)             PS_PDISK=${PS_DISK} ;;
    esac
    PART1=${PS_PDISK}1
    PART2=${PS_PDISK}2
    TYPE=`get_drive_type ${PS_DISK}`
    logger -s "${ME}:	Initializing Persistent Store device: ${PS_DISK}."
    SERIAL=`get_serial ${PS_DISK}`
    FWV=`get_firmware_version ${PS_DISK}`
    STATNM=`get_block_disk_name ${PS_DISK} ${PART1}`
    SZ=`get_part_size ${PS_DISK} 1`
    if [ $TESTING -eq 0 ]; then
	${INIT_PS_DISK} ${PS_DISK}
    else
	echo ${INIT_PS_DISK} ${PS_DISK}
    fi
    echo "${SERIAL}  ${PART1}  ${PART2}  ${PS_DISK}  ${STATNM}  ${SZ}  ${TYPE}  PS  ${FWV}" >> ${CACHE_CONFIG}
    return
  done
}	# setup_ps_devices

function setup_sysdata_DM_partitions()
{
  local TYPE
  local RETVAL
  local BLKSZ
  local PAGESZ
  local SERIAL
  local STATNM
  local PARTNUM
  local SZ
  local FWV

  # This global should aways be set at this point, but just be safe.
  [ -z "${SYSDATA_DISK_DEV_NAME}" ] && return
  DISK="${SYSDATA_DISK_DEV_NAME}"

  # See if the sysdata disk has the DM partitions.
  if [ "${ROOT_FS}" = "" ]; then
    # Valid root disk not found
    logger -s "${ME}:     No cache on system drive: ${DISK}."
    return
  fi
  if [ ${ROOT_CACHE_FOUND} -ne 0 ]; then
    logger -s "${ME}:     No cache on system drive: ${DISK}."
    return
  fi

  TYPE=`get_drive_type ${DISK}`
  RETVAL=`get_disk_block_page_sizes $TYPE`
  BLKSZ=`echo "$RETVAL" | awk '{print $1}'`
  PAGESZ=`echo "$RETVAL" | awk '{print $2}'`

  #
  # Check to see if the DM2 cache is already setup and if not, then create it.
  #
  if mount ${ROOT_FS} ${TMPMNT} > /dev/null 2>&1; then
    if ! is_DM2_cache ${TMPMNT}; then
      # If we are booting off this drive and we find that we need to
      # initialize this disk, we do not care if the TM DB has any
      # other entries.
      umount ${TMPMNT}
      logger -s "${ME}:     Initializing system drive cache: ${ROOT_FS}"

      if [ $TESTING -eq 0 ]; then
        ${INITROOTCACHE} ${ROOT_RAW} ${ROOT_FS} ${BLKSZ} ${PAGESZ} ${PERCENT}
      else
        echo ${INITROOTCACHE} ${ROOT_RAW} ${ROOT_FS} ${BLKSZ} ${PAGESZ} ${PERCENT}
      fi
      if [ $? -ne 0 ]; then
        return
      fi
    else
      umount ${TMPMNT}
    fi
  fi
  SERIAL=`get_serial ${DISK}`
  FWV=`get_firmware_version ${DISK}`
  STATNM=`get_block_disk_name ${DISK} ${ROOT_RAW}`
  PARTNUM=`echo "${ROOT_RAW}" | sed s@${DISK}@@g`
  SZ=`get_part_size ${DISK} ${PARTNUM}`
  echo "${SERIAL}  ${ROOT_RAW}  ${ROOT_FS}  ${DISK}  ${STATNM}  ${SZ}  ${TYPE}  DM2  ${FWV}" >> ${CACHE_CONFIG}
  logger -s "${ME}:     Preserving system drive cache on drive: ${DISK}."
  ROOT_CACHE_FOUND=1
}

function setup_one_DM_disk()
{
  local DISK=$1
  local PDISK=
  local PART1
  local PART2
  local TYPE
  local RETVAL
  local BLKSZ
  local PAGESZ
  local SERIAL
  local STATNM
  local SZ
  local PROCESSED
  local INIT_DM2_CACHE
  local PARTS
  local PART
  local FWV

  case ${DISK} in
  /dev/cciss/c*) PDISK=${DISK}p ;;
  *)             PDISK=${DISK} ;;
  esac
  PART1=${PDISK}1
  PART2=${PDISK}2

  TYPE=`get_drive_type ${DISK}`

  grep -q "^${DISK}\$" /config/nkn/reserved-disks-list.txt
  if [ ${?} -eq 0 ] ; then
    # This drive is not for DM cache use.
    logger -s "${ME}:     Skipping reserved drive ${DISK}"
    return
  fi
  logger -s "${ME}:     Setting up cache on ${DISK}"

  RETVAL=`get_disk_block_page_sizes ${TYPE}`
  BLKSZ=`echo "$RETVAL" | awk '{print $1}'`
  PAGESZ=`echo "$RETVAL" | awk '{print $2}'`
  parted --script ${DISK} print > /dev/null 2>&1
  if [ $? -eq 1 ]; then
    #
    # No partition table present; parted will return 0 if it finds a
    # ext3 filesystem on the entire partition.
    #
    logger -s "${ME}:     Initializing drive cache: ${DISK}."
    if [ $TESTING -eq 0 ]; then
      ${INITCACHE} ${DISK} ${BLKSZ} ${PAGESZ} ${PERCENT}
    else
      echo ${INITCACHE} ${DISK} ${BLKSZ} ${PAGESZ} ${PERCENT}
    fi
    if [ $? -ne 0 ]; then
      logger -s "${ME}:        Cache initialization failed: ${DISK}"
      return
    fi
    SERIAL=`get_serial ${DISK}`
    FWV=`get_firmware_version ${DISK}`
    STATNM=`get_block_disk_name ${DISK} ${PART1}`
    SZ=`get_part_size ${DISK} 1`
    echo "${SERIAL}  ${PART1}  ${PART2}  ${DISK}  ${STATNM}  ${SZ}  ${TYPE}  DM2  ${FWV}" >> ${CACHE_CONFIG}
    return
  fi


  #
  # There may not be a partition found if an ext3 filesystem spans the
  # entire disk.  However, parted returns 0 in this case.
  #
  # Partition found: exit code=0
  PROCESSED=0
  INIT_DM2_CACHE=1
  PARTS=`fdisk -l ${DISK} 2>/dev/null | egrep "^${DISK}"  | awk '{print $1}'`
  for PART in ${PARTS}; do
    if [ "${DISK}1" = "${PART}" ]; then
      if mount ${PART} ${TMPMNT} > /dev/null 2>&1; then
	if is_DM_cache "${TMPMNT}"; then
	  logger -s "${ME}: WARNING: Deleting unsupported DM cache: ${PART}."
	fi
	umount ${TMPMNT}
      fi
      INIT_DM2_CACHE=1
      continue
    fi

    #
    # This is standard DM2 cache partition.  Check to see if a DM2 cache
    # is already setup.
    #
    if [ "${PART2}" = "${PART}" ]; then
      if mount ${PART} ${TMPMNT} > /dev/null 2>&1; then
	if is_DM2_cache "${TMPMNT}"; then
	  SERIAL=`get_serial ${DISK}`
          FWV=`get_firmware_version ${DISK}`
	  STATNM=`get_block_disk_name ${DISK} ${PART1}`
	  SZ=`get_part_size ${DISK} 1`
	  TYPE=`get_drive_type ${DISK}`
	  echo "${SERIAL}  ${PART1}  ${PART2}  ${DISK}  ${STATNM}  ${SZ}  ${TYPE}  DM2  ${FWV}" >> ${CACHE_CONFIG}
	  logger -s "${ME}:     Preserving DM2 cache on drive: ${DISK}."
	  INIT_DM2_CACHE=0
	  PROCESSED=1
	  umount ${TMPMNT}
	  break
	fi
	umount ${TMPMNT}
      fi  # mount
    fi  # if DISK2 = PART
  done  # for PART

  #
  # Initialize a DM2 cache if we do not recognize this disk as being the
  # root drive or as having a DM2 cache already.
  #
  if [ ${INIT_DM2_CACHE} -eq 1 ]; then
    echo "    Initializing drive cache: ${DISK}."
    if [ $TESTING -eq 0 ]; then
      ${INITCACHE} ${DISK} ${BLKSZ} ${PAGESZ} ${PERCENT}
    else
      echo ${INITCACHE} ${DISK} ${BLKSZ} ${PAGESZ} ${PERCENT}
    fi
    if [ $? -eq 0 ]; then
      SERIAL=`get_serial ${DISK}`
      FWV=`get_firmware_version ${DISK}`
      STATNM=`get_block_disk_name ${DISK} ${PART1}`
      SZ=`get_part_size ${DISK} 1`
      echo "${SERIAL}  ${PART1}  ${PART2}  ${DISK}  ${STATNM}  ${SZ}  ${TYPE}  DM2  ${FWV}" >> ${CACHE_CONFIG}
      PROCESSED=1
    fi
  fi
  if [ $PROCESSED -eq 0 ]; then
    logger -s "${ME}: ERROR: Disk not processed: ${DISK}"
  fi
}	# setup_one_DM_disk

#
# Setup the required file system parameters
#
function setup_disk_caches()
{
  local DISK
  local PS_DISK

  logger -s "${ME}:     Candidate cache drives: ${DEVICES}."
  setup_sysdata_DM_partitions
  for DISK in ${DEVICES}; do
    if [ -e ${INIT_CACHE_PS_DISKS_FILE} ]; then
      grep -q "^${DISK}\$" ${INIT_CACHE_PS_DISKS_FILE}
      if [ ${?} -eq 0 ] ; then
	logger "${ME}:     Skipping persistent data drive ${DISK}"
	continue
      fi
    fi
    if ! ls -l ${DISK} > /dev/null 2>&1; then
      logger "${ME}: ${DISK} not present"
      continue
    fi
    setup_one_DM_disk ${DISK}
  done # for DISK in $DEVICES
}	# setup_disk_caches

function reinit_disk_caches()
{
  # Only re-init the drive caches if the contents of ${INIT_CACHE_OPT_FILE}
  # is "init-cache".
  local init_cache_flag=
  if [ -e ${INIT_CACHE_OPT_FILE} ]; then
    init_cache_flag=`cat ${INIT_CACHE_OPT_FILE}`
  fi
  logger -s "${ME}: Reinit: [${init_cache_flag}]"
  if [ "${init_cache_flag}" != "init-cache" ]; then
    return
  fi

  echo "keep-cache" > ${INIT_CACHE_OPT_FILE}
  # All ok, so run the re-init script
  ${REINIT_CONFIG_DISKS}
}	#  reinit_disk_caches

function set_root_dev()
{
  # Get the device name of the root drive
  if [ -e ${SYSDATA_DISK_DEV_INFO_FILE} ]; then
    SYSDATA_DISK_DEV_NAME=`cat ${SYSDATA_DISK_DEV_INFO_FILE}`
  fi
  # Get the device name of the eUSB drive
  if [ -e ${EUSB_DISK_DEV_INFO_FILE} ]; then
    EUSB_DISK_DEV_NAME=`cat ${EUSB_DISK_DEV_INFO_FILE}`
  fi
}	# set_root_dev

function setup_a()
{
  local CNT=1
  local ROOT_DEV=${1}

  DEVICES="${ROOT_DEV}a"
  SLOT_0="${ROOT_DEV}a"
  case ${ROOT_DEV} in
  /dev/hd*)
    PARM_CMD=hdparm
    ;;
  /dev/sd*)
    PARM_CMD=/opt/nkn/bin/sdparm
    ;;
  *)
    PARM_CMD=
    ;;
  esac
  #        1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 29 40 41 42 43 44 45 46 47 48 49 50 51
  for A in b c d e f g h i j  k  l  m  n  o  p  q  r  s  t  u  v  w  x  y  z aa ab ac ad ae af ag ah ai aj ak al am an ao ap aq ar as at au av aw ax ay az; do
    if [ "${EUSB_DISK_DEV_NAME}" == "${ROOT_DEV}${A}" ]; then
      logger -s "${ME}: Skipping eUSB device ${EUSB_DISK_DEV_NAME}"
      continue;
    fi
    if [ ! -z "${PARM_CMD}" ] ; then
      # If the device does not exist, then skip.
      [ ! -e ${ROOT_DEV}${A} ] && continue
      # On an IBM Adaptec card, the following command prints info on stdout
      # then returns 5.
#      ${PARM_CMD} -i ${ROOT_DEV}${A} > /dev/null 2>/dev/null
#      [ $? -ne 0 ] && continue
      # If the device's media is removable (a CD or DVD drive), then skip.
      # This test is needed not only because trying to use a CD or DVD
      # for cache does not make sense, also later using hdparm -W0
      # (disable write cache) on the device hangs.
      ${PARM_CMD} -i ${ROOT_DEV}${A} | grep Config= | grep Removeable > /dev/null
      [ $? -eq 0 ] && continue
    fi
    eval SLOT_${CNT}=${ROOT_DEV}${A}
    DEVICES="${DEVICES} ${ROOT_DEV}${A}"
    CNT=$[$CNT + 1]
  done
}	# setup_a

function setup_n()
{
  local ROOT_DEV=${1}
  local CNT=1

  DEVICES="${ROOT_DEV}0"
  SLOT_0="${ROOT_DEV}0"
  for N in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
    eval SLOT_${CNT}=${ROOT_DEV}${N}
    DEVICES="${DEVICES} ${ROOT_DEV}${N}"
    CNT=$[$CNT + 1]
  done
}	# setup_n

# Using the root disk dev name (from SYSDATA_DISK_DEV_NAME), set up the 
# device global variables, DEVICES and SLOT_*
function setup_rootdev_devices_and_slots()
{
  logger -s "${ME}: System drive:${SYSDATA_DISK_DEV_NAME}"
  # VXA2100 systems may not have /dev/sda as the root if SSDs are present.
  # In particular, the root should be /dev/sdc if SSDs are present in slots
  # 14 & 15.
  case _${SYSDATA_DISK_DEV_NAME} in
  _/dev/sd*)    setup_a /dev/sd ;;
  _/dev/hda)    setup_a /dev/hd ;;
  # Adding this case for virtio devices
  _/dev/vd*)	setup_a /dev/vd ;;
  _/dev/cciss/c0d0*)
    # HP SmartArray naming convention before Centos 6.0
    setup_n /dev/cciss/c0d
    SYSDATA_DISK_DEV_NAME=/dev/cciss/c0d0
    ;;
  _/dev/*)
    logger -s "${ME}: ERROR: Unsupported disk driver for cache filesystem."
    # Guess at the naming scheme of the device by looking at the last character.
    case ${SYSDATA_DISK_DEV_NAME} in
    *a)
      setup_a ${SYSDATA_DISK_DEV_NAME%a}
      ;;
    *0)
      # This could be dangerous
      setup_n ${SYSDATA_DISK_DEV_NAME%0}
      ;;
    *)
      # This could be a eUSB case, so check for it
      if [ ! -z "${EUSB_DISK_DEV_NAME}" ] ; then
        logger -s "${ME}: Sysdata disk is an eUSB"
        setup_a /dev/sd 
      else
        # Do not know the naming scheme, so just use the root drive.
        DEVICES="${SYSDATA_DISK_DEV_NAME}"
        SLOT_0="${SYSDATA_DISK_DEV_NAME}"
      fi
      ;;
    esac
    ;;
  *)  logger -s "${ME}: ERROR: Sysdata disk drive name not specified."
    # This should not happen, because install-mfg should specify the name
    # on the manufacture.sh command line.
    # Just in case, default to SDA naming scheme.
    SYSDATA_DISK_DEV_NAME=/dev/sda
    setup_a /dev/sd
    ;;
  esac
}	# setup_rootdev_devices_and_slots

#--------------------------------------------------------------
#
#  MAIN LOGIC BEGINS HERE 
#
#--------------------------------------------------------------
# We would like to get the /nkn/tmp/findslot.last file before the CLI
# appears as there is a small window where mirroring code depends on the
# availability of this file before DM2_init() runs this cmd to determine
# the correct slot no.
/opt/nkn/generic_cli_bin/findslottodcno.sh -a > /dev/null 2>&1

ME=`basename "${0}"`
# Create the cache partitions and filesystems
echo
logger -s "${ME}: Setting up all the drive caches."
for i in `seq 5 -1 2`; do
  if [ -e ${CACHE_CONFIG}.bak$[ $i-1 ] -a -e ${CACHE_CONFIG}.bak${i} ]; then
    mv -f ${CACHE_CONFIG}.bak${i} ${CACHE_CONFIG}.bak$[ $i+1 ]
  fi
done
if [ -e ${CACHE_CONFIG}.bak1 ]; then
  mv -f ${CACHE_CONFIG}.bak1 ${CACHE_CONFIG}.bak2
fi
if [ -e ${CACHE_CONFIG} ]; then
  mv -f ${CACHE_CONFIG} ${CACHE_CONFIG}.bak1
fi
mkdir $TMPMNT > /dev/null 2>&1

#
# This function populates a file which contains all controller info from
# the vendor specific binary when needed.
#
# - HP Smart Array controller
# - Adaptec IBM controller
# - The Adaptec/JFMD controller can be configured to work with the generic
#   code if all non-root drives have morphing disabled
#
if is_invalid_config; then
  exit 0;
fi

killall klogd
sleep 2
/sbin/klogd -c 4

# Get the device name of the root drive into the global SYSDATA_DISK_DEV_NAME
set_root_dev

# Set the global variables DEVICES and SLOT_* based on SYSDATA_DISK_DEV_NAME
setup_rootdev_devices_and_slots

disable_write_caching
setup_ps_devices
find_root_cache
setup_disk_caches

# Now write the info saved in the ${CACHE_CONFIG} to the config database.
if [ $TESTING -eq 0 ]; then
  ${WRITE_DISKINFO_TO_DB} initdata
else
  echo "Skipping DB update"
fi
logger -s "${ME}: Disk cache setup: DONE"

reinit_disk_caches

/etc/init.d/syslog restart > /dev/null 2>&1

echo

exit 0

#
# End of initdisks.sh
#
