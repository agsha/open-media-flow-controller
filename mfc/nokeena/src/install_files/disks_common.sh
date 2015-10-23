#! /bin/bash
#
#	File : disks_common.sh
#
#	Description : This file holds common routines for:
#		initdisks.sh
#		write_diskinfo_to_db.sh
#
#	Created By : Michael Nishimoto (miken@nokeena.com)
#
#	Created On : 8 Auguest, 2009
#
#	Copyright (c) Nokeena Inc., 2009
#	Copyright (c) Juniper Networks., 2010-2013
#

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/StorMan
SYSDATA_DISK_DEV_INFO_FILE=/config/nkn/system-data-disk.info
BOOT_DISK_DEV_INFO_FILE=/config/nkn/boot-disk.info
if [ -x /opt/nkn/bin/smartctl ]; then
  SMARTCTL=/opt/nkn/bin/smartctl
else
  SMARTCTL=/usr/sbin/smartctl
fi

if [ -x /opt/nkn/bin/hdparm ]; then
  HDPARM=/opt/nkn/bin/hdparm
else
  HDPARM=
fi

VMWARE_SENTINEL_FILE=/config/nkn/allow_vmware
VXA_MODEL_FILE=/etc/vxa_model
PACIFICA_MODEL_FILE=/etc/pacifica_hw

LASTFINDSLOTFILE=/nkn/tmp/findslot.last

HPCLI=/usr/sbin/hpacucli
HPCLI_SHOW_ALL="${HPCLI} ctrl all show"
HPCLI_SLOT=
HPCLI_OUT=/nkn/tmp/smart_array.config
HPCLI_CTRL_ALL_OUT=/nkn/tmp/smart_array.ctrl
HPCLI_CTRL_LOG_OUT=/nkn/tmp/smart_array.logical
HPCLI_CTRL_PHYS_OUT=/nkn/tmp/smart_array.physical

ADPCLI=/usr/StorMan/arcconf
ADPCLI_GETVERS="${ADPCLI} getversion"
ADPCLI_GETVERS_OUT=/nkn/tmp/adaptec.vers
ADPCLI_GETVERS_ERR=/nkn/tmp/adaptec.vers.err
ADPCLI_GETCONF_ALL="${ADPCLI} getconfig"
ADPCLI_OUT=/nkn/tmp/adaptec.config
ADPCLI_ERR=/nkn/tmp/adaptec.config.err
ADPCLI_CONF_DISK=/nkn/tmp/arcconf.getconfig.pd.

TYPE_UNKNOWN_STRING="TYPE_UNKNOWN"

##########################################################
#
# Helper code
#
##########################################################

#
# This routine won't work correctly if used with a disk controller which allows
# drives to show up in a state which will not cause a /dev/* node to be created.
#
# Controllers in this list need this function & don't support > 16 drives
#	3ware
#	megaraid
#
function find_slot()
{
  local SLOT=-1

  if [ "$1" = "${SLOT_0}" ]; then
    SLOT=0
  elif [ "$1" = "${SLOT_1}" ]; then
    SLOT=1
  elif [ "$1" = "${SLOT_2}" ]; then
    SLOT=2
  elif [ "$1" = "${SLOT_3}" ]; then
    SLOT=3
  elif [ "$1" = "${SLOT_4}" ]; then
    SLOT=4
  elif [ "$1" = "${SLOT_5}" ]; then
    SLOT=5
  elif [ "$1" = "${SLOT_6}" ]; then
    SLOT=6
  elif [ "$1" = "${SLOT_7}" ]; then
    SLOT=7
  elif [ "$1" = "${SLOT_8}" ]; then
    SLOT=8
  elif [ "$1" = "${SLOT_9}" ]; then
    SLOT=9
  elif [ "$1" = "${SLOT_10}" ]; then
    SLOT=10
  elif [ "$1" = "${SLOT_11}" ]; then
    SLOT=11
  elif [ "$1" = "${SLOT_12}" ]; then
    SLOT=12
  elif [ "$1" = "${SLOT_13}" ]; then
    SLOT=13
  elif [ "$1" = "${SLOT_14}" ]; then
    SLOT=14
  elif [ "$1" = "${SLOT_15}" ]; then
    SLOT=15
  fi
  echo ${SLOT}
}	# find_slot


# raw_part = Any full partition name which is not the full disk name.
function is_system_disk()
{
  local raw_part=$1
  local sysdata_disk=`cat ${SYSDATA_DISK_DEV_INFO_FILE}`
  local boot_disk=`cat ${BOOT_DISK_DEV_INFO_FILE}`

  # Will work even when SmartArray uses different device names
  if (echo ${sysdata_disk} | grep -q cciss); then
    # On Smart Array the root drive is /dev/cciss/c0d0.
    # The system data disk name is a subset of the raw partition.
    if (echo ${raw_part} | grep -q ${sysdata_disk}); then
      return 0;
    else
      return 1;
    fi
  else
    local disk=`echo ${raw_part} | sed 's/[0-9]$//'`

    if [ "${disk}" = "${sysdata_disk}" ]; then
      return 0;
    elif [ "${disk}" = "${boot_disk}" ]; then
      return 0;
    else
      return 1;
    fi
  fi
}       # is_system_disk


# Return whether the system is a vxa 2100 or not
function is_vxa_2100()
{
  [ ! -e /etc/vxa_model ] && return 1
  # The this file sets VXA_MODEL
  . /etc/vxa_model
  [ "${VXA_MODEL}" -eq 0305 ] && return 0
  return 1
} # is_vxa_2100


function is_eUSB_device()
{
  local eusb=`cat /config/nkn/eusb-disk.info`

  if [ "${eusb}" = "$1" ]; then
    return 0
  else
    return 1
  fi
}	# is_eUSB_device


function is_megaraid()
{
  if lspci | egrep -q -i "megaraid|Expandable RAID"; then
    return 0
  else
    return 1
  fi
}	# is_megaraid


function is_smart_array()
{
  if lspci | grep -q -i "smart array"; then
    return 0
  else
    return 1
  fi
}	# is_smart_array


function is_3ware()
{
  if lspci | grep -q -i "3ware"; then
    return 0
  else
    return 1
  fi
}	# is_3ware


function is_cciss_smart_disk()
{
  echo ${1} | grep -q cciss
}	# is_cciss_smart_disk


function is_adaptec_ibm()
{
  local adaptec=`lspci | grep -i aac-raid`

  if [ $? -eq 0 ]; then
    if echo $adaptec | grep -q -i rocket; then
      return 0
    else
      return 1
    fi
  else
    return 1
  fi
}


function is_adaptec_sun()
{
  local adaptec=`lspci | grep -i aac-raid`

  if [ $? -eq 0 ]; then
    $ADPCLI_GETCONF_ALL 1 AD | grep "Controller Model" | grep -q -i sun
    if [ $? -eq 0 ]; then
      return 0
    else
      return 1
    fi
  else
    return 1
  fi
}	# is_adaptec_sun


function is_production_vxa()
{
  # This file was added for all VXA chassis and should give enough info
  # to figure out if a system is a production VXA system.
  if [ -e ${VXA_MODEL_FILE} ]; then
    return 0;
  else
    return 1;
  fi
}	# is_production_vxa

function is_pacifica_hw()
{
  # This file was added for pacifica hw and should give enough info
  # to figure out if a system is a pacifica blade.
  if [ -e ${PACIFICA_MODEL_FILE} ]; then
    return 0;
  else
    return 1;
  fi
}	# is_pacifica_hw


#
# Return 0 if this is a VXA system that uses Adaptec controller.
#
function is_adaptec_vxa()
{
  # All VXA models should have /etc/vxa_model file
  if [ -e /etc/vxa_model ]; then
    lspci | grep -q -i aac-raid
    if [ $? -eq 0 ]; then
      return 0
    else
      # Some VXAs don't have Adaptec controller.  They can use standard calls
      return 1
    fi
  else
    return 1
  fi
}	# is_adaptec_vxa

##########################################################
#
# GET info functions
#
##########################################################

# See dev/eng/samples/smartarray for sample output
function get_serial_smart_array()
{
  local disk=$1
  local num_arrays=`grep -c array $HPCLI_CTRL_ALL_OUT`
  local array
  local serial

  for array in `seq $num_arrays`; do
    cat ${HPCLI_CTRL_LOG_OUT}.${array} | grep -q ${disk}
    if [ $? -eq 0 ]; then
      serial=`cat ${HPCLI_CTRL_PHYS_OUT}.${array} | grep "Serial Number" | awk '{print $3}'`
      echo "$serial"
      return
    fi
  done
  logger -s "${ME}: Can not find serial number for ${disk}"
  echo ""
}	# get_serial_smart_array


function get_serial_3ware()
{
  local disk=$1
  local slot
  local num
  local match
  local rootdev=""

  match=`/sbin/lspci | grep -i 3ware`
  if echo $match | egrep -q " 9[0-9][0-9[0-9]"; then
    # 9XXX controller
    rootdev="/dev/twa0"
  elif echo $match | egrep -q " [6-8][0-9][0-9[0-9]"; then
    # 6XXX or 7XXX or 8XXX controller
    rootdev="/dev/twe0"
  fi

  if [ "${rootdev}" != "" ]; then
    slot=`find_slot ${disk}`
    num=`${SMARTCTL} -d 3ware,${slot} -i ${rootdev} | grep -i "serial number:" | awk '{print $3}'`
    echo $num
  else
    echo ""
  fi
}	# get_serial_3ware


function get_serial_adaptec_ibm_sun()
{
  local disk=$1
  local dnum=`find_slot $disk`
  local serialnum

  # Find the disk # which is one more than the slot number which starts at 0
  (( dnum++ ))

  # 1. First find all serial numbers.
  # 2. Remove the controller serial number.
  # 3. We want the Nth entry, so let's remove everything after the Nth entry
  # 4. Take the last entry of what remains
  #
  # Get the Nth line with "Serial number" with no "Controller" string
  serialnum=`grep -i "Serial number" $ADPCLI_OUT | grep -v -i "Controller Serial Number" | head -${dnum} | tail -1 | awk '{print $4}'`
  echo "$serialnum"
}	# get_serial_adaptec_ibm_sun

#
# smartctl works for SAS and SATA drives so far
#
# This routine does not depend on an initialized drive.
# The format of $DISK should be "/dev/sdb".
#
function get_serial()
{
  local NUM;
  local DISK=$1
  local SLOT

  if is_adaptec_ibm || is_adaptec_sun; then
    NUM=`get_serial_adaptec_ibm_sun ${DISK}`
  elif is_megaraid; then
    SLOT=`find_slot ${DISK}`
    # Note: See the comment about using /dev/sda in function 
    #	get_drive_type_megaraid().
    NUM=`${SMARTCTL} -d megaraid,${SLOT} -i /dev/sda | grep -i "serial number:" | awk '{print $3}'`
  elif is_smart_array; then
    NUM=`get_serial_smart_array ${DISK}`
  elif is_3ware; then
    NUM=`get_serial_3ware ${DISK}`
  else
    NUM=`${SMARTCTL} -i ${DISK} | grep -i "serial number:" | awk '{print $3}'`
  fi
  if [ "${NUM}" = "" ]; then
    # ** indicates no serial number
    echo `basename ${DISK}*@`
  else
    echo "$NUM"
  fi
}	# get_serial

#
# smartctl works for SAS and SATA drives so far
#
# This routine does not depend on an initialized drive.
# The format of $DISK should be "/dev/sdb".
#
function get_firmware_version()
{
  local NUM;
  local DISK=$1
  local SLOT

  if is_production_vxa || is_pacifica_hw ; then
    NUM=`${SMARTCTL} -d sat -i ${DISK} |grep -i "Firmware Version:"|awk '{print $3}'`
    if [ "${NUM}" = "" ]; then
      echo "NA"
    else
      echo "$NUM"
    fi
  else
    echo "NA"
  fi
}	# get_firmware_version


function get_block_disk_name()
{
  local DISK=$1
  local PART1=$2
  local DISK_BASE=`echo ${DISK} | sed 's@/.*/@@'`
  local PART_BASE=`echo ${PART1} | sed 's@/.*/@@'`

  if is_cciss_smart_disk ${DISK}; then
    echo "/sys/block/cciss!${DISK_BASE}/cciss!${PART_BASE}/size"
  else
    # megaraid and normal device names
    echo "/sys/block/${DISK_BASE}/${PART_BASE}/size"
  fi
}	# get_block_disk_name


#
# Parted prints out the size in MB or GB.  Most of our drives will display GB
# but some of the smaller drive may not have much space left over, so they
# display in MB.  Need to convert MB to GB before returning.
#
# This routine does not depend on an initialized cache
#
function get_part_size()
{
  local DISK=$1
  local PART=$2
  local DISK_BASE=`basename ${DISK}`
  local SZ
  local FN

  if [ $# -lt 2 ]; then
    echo "0"
  fi
  if is_cciss_smart_disk ${DISK}; then
    if echo ${PART} | grep -q p; then
      FN=/sys/block/cciss\!${DISK_BASE}/cciss\!${DISK_BASE}${PART}/size
    else
      FN=/sys/block/cciss\!${DISK_BASE}/cciss\!${DISK_BASE}p${PART}/size
    fi
  else
    FN=/sys/block/${DISK_BASE}/${DISK_BASE}${PART}/size
  fi
  if [ -e  ${FN} ]; then
    SZ=`cat ${FN}`
    if [ "$SZ" = "0" ]; then
      echo "SIZE_UNKNOWN"
    else
      # 4000 = # sectors in 2MiB
      SZ=`echo "${SZ}/4096*4096" | bc`
      echo "$SZ"
    fi
  else
    echo "SIZE_UNKNOWN"
  fi
  return
}	# get_part_size

##########################################################
#
# Get Drive Type
#
##########################################################
function is_ssd_drive()
{
  local model=$1

  #
  # - Original Intel SLC SSDs have string SSDSA in Device Model
  #     (example: SSDSA2SH064G1GC)
  # - Newer Intel MLC SSDs have string SSDSC in Device Model
  #     (example: INTEL SSDSC2BW240A3)
  # - Viking/Sandforce Pacifica SSD SATADIMM model # has
  #	base string: FSDC400GFCVMMA.
  # - Viking generic 400GB SSD model is VRFS21400GBCTMMA.
  # - Viking custom 400GB SSD model is VRFS21400GBCTMMAL1.
  # - Viking 200GB SSD model is VRFS21200GBCYMMA.
  # - One Samsung model is MCBQE25G5MPQ-0VAD3.
  # - One OCZ model is D2CSTK251M11-048.  Serial # has "OCZ-" in it.
  #
  if echo "${model}" | egrep -q "SSDSA|SSDSC|FSDC400GFCVMMA|VRFS21400GBCTMMA|VRFS21200GBCYMMA|D2CSTK251M11-048|MCBQE25G5MPQ-0VAD3"; then
    return 0
  else
    return 1
  fi
}	# is_ssd_drive


# Identify if it is a Viking drive
function is_Viking_SSD_with_SF1500()
{
  local DISK=$1
  local cmd="${SMARTCTL} -i ${DISK}"
  local model=`${cmd} | grep -i "Device Model"`

  if [ $? -eq 0 ]; then
    if echo "${model}" | egrep -q "VRFS21400GBCTMMA|VRSFVR21200GBCYMMA"; then
      return 0
    else
      return 1
    fi
  else
    return 1
  fi
}


# Identify if it is an Intel SSD drive
function is_intel_ssd_drive()
{
  #
  # - Intel SSDs have string SSDSA in Device Model
  # - This code assumes that all Intel drives behave the same, including
  # - ones which might have a SandForce controller. SandForce previously
  # - suggested that frequent secure erases were bad.
  #
  local drive=$1
  if echo "$drive" | grep -q "/dev/sd"; then
    true #continue
  else
    return 1
  fi
  local cmd="${SMARTCTL} -i ${drive}"
  local model=`${cmd} | grep -i "Device Model"`

  if [ $? -eq 0 ]; then
    if echo "$model" | egrep -q "SSDSA"; then
      return 0
    fi
  fi
  return 1
}	# is_intel_ssd_drive


# Function to perform secure erase on a drive
function perform_secure_erase()
{
  local dr=$1
  # Save the output of secure erase operation
  local output_file="/nkn/tmp/secure_erase.txt"
  # If dr is a valid drive continue
  if echo "$dr" | grep -q "/dev/sd"; then
    true #continue
  else
    echo -e "Error passing drive to secure_erase function\n" \
    >> "$output_file"
    return
  fi
  local cmd="${SMARTCTL} -i ${dr}"
  local serial_num=`$cmd | grep -i "Serial" | awk '{print $3}'`
  if [ $? -eq 1 ]; then
    return
  fi
  # Secure erase operation is performed differently on VXA2100 and VXA2010
  # On VXA2100, execute the hdparm command for secure erase
  # On any other system, secure erase is not supported
  #
  echo  SERIAL NO="$serial_num" >> "$output_file"
  if is_vxa_2100; then
    echo -e "Date: " `date` >> "$output_file"
    if [ "$HDPARM" != "" ]; then
      "$HDPARM" --security-set-pass PASSWD "$dr" >> "$output_file"
      if [ $? -eq 0 ]; then
        "$HDPARM" --security-erase PASSWD "$dr" >> "$output_file"
      fi
    fi
  else
    echo -e "Secure erase not supported on this system\n" >> "$output_file"
  fi
  echo -e "-----------------------------------------------------------\n" \
  >> "$output_file"
}	# perform_secure_erase


function get_drive_type_megaraid()
{
  local disk=$1
  local slot=`find_slot ${disk}`
  # Note that we only support megaraid when it is the only contoller.
  # For the smartctl command for megaraid you have to supply a device
  # name of a drive on the controller.  So we use "/dev/sda".
  local cmd="${SMARTCTL} -d megaraid,${slot} -i /dev/sda"
  local dtype=`${cmd} | grep -i "transport protocol:" | awk '{print $3}'`

  if [ "${dtype}" = "SAS" ]; then
    echo "SAS"
    return
  fi

  # This only works when SSD reports as a SATA device
  local model=`${cmd} | grep -i "Device Model"`
  if is_ssd_drive "${model}"; then
    echo "SSD"
    return
  fi

  # This allows me to see the Intel SSD behind the megaraid
  if ${cmd} | grep -q "ATA Version is"; then
    echo "SATA"
    return
  fi
  echo $TYPE_UNKNOWN_STRING
}	# get_drive_type_megaraid


#
# We need some sample output here
#
function get_drive_type_smart_array()
{
  local disk=$1
  local num_arrays=`grep -c array $HPCLI_CTRL_ALL_OUT`
  local disk_type
  local array

  for array in `seq $num_arrays`; do
    cat ${HPCLI_CTRL_LOG_OUT}.${array} | grep -q ${disk}
    if [ $? -eq 0 ]; then
      disk_type=`cat ${HPCLI_CTRL_PHYS_OUT}.${array} | grep "Interface Type" | awk '{print $3}'`

      if [ "${disk_type}" = "SAS" ]; then
	echo "SAS"
	return
      fi
      #
      # HP SmartArray in CentOS 6.2 can return "Solid State SATA" for an SSD
      # drive.  Not sure what algorithm it uses to determine this, so we will
      # not depend on its algorithm.  "Solid" = "Solid State SATA".  Use 
      # this check to avoid leading blank issues.
      #
      if [ "${disk_type}" = "Solid" ]; then
	echo "SSD"
	return
      fi
      if [ "${disk_type}" = "SATA" ]; then
        local model=`cat ${HPCLI_CTRL_PHYS_OUT}.${array} | grep "Model:"`
	if is_ssd_drive "${model}"; then
	  echo "SSD"
	else
	  echo "SATA"
	fi
	return
      fi
      # Should not fall through
      echo "$disk_type"
      return
    fi
  done
  logger -s "${ME}: Can not find interface type for ${disk}"
  echo "SATA"
}	# get_drive_type_smart_array


# This function must be changed to handle > 1 drive.
function get_drive_type_3ware()
{
  local disk=$1
  local type
  local slot
  local match
  local rootdev=""
  local cmd

  match=`/sbin/lspci | grep -i 3ware`
  if `echo $match | egrep -q " 9[0-9][0-9[0-9]"`; then
    # 9XXX controller
    rootdev="/dev/twa0"
  elif `echo $match | egrep -q " [6-8][0-9][0-9[0-9]"`; then
    # 6XXX or 7XXX or 8XXX controller
    rootdev="/dev/twe0"
  fi
  slot=`find_slot ${disk}`
  cmd="${SMARTCTL} -d 3ware,${slot} -i ${rootdev}"

  # This only works when SSD reports as a SATA device
  local model=`$cmd | grep -i "Device Model"`
  if is_ssd_drive "${model}"; then
    echo "SSD"
    return
  fi

  type=`${cmd} | grep -i "transport protocol:" | awk '{print $3}'`

  if [ "${type}" = "SAS" ]; then
    # This most likely won't work
    echo "SAS"
    return
  fi

  if ${cmd} | grep -q "ATA Version is"; then
    echo "SATA"
    return
  fi

  # With current s/w we can't recognize SAS drives on certain controllers
  echo "SAS"
}	# get_drive_type_3ware


function get_drive_type_adaptec_ibm_sun()
{
  local disk=$1
  local type
  local dnum=`find_slot $disk`
  local model
  local xferspeed

  (( dnum++ ))
  # Get the Nth line with "Model"
  model=`grep -i Model $ADPCLI_OUT | grep -v -i "Controller Model" | head -${dnum} | tail -1`
  if is_ssd_drive "${model}"; then
    echo "SSD"
    return
  fi

  # Get the Nth line with "Transfer Speed"
  xferspeed=`grep -i "Transfer Speed" $ADPCLI_OUT | head -${dnum} | tail -1`
  if echo "${xferspeed}" | grep -q SAS; then
    echo "SAS"
    return
  fi
  if echo "${xferspeed}" | grep -q SATA; then
    echo "SATA"
    return
  fi

  echo $TYPE_UNKNOWN_STRING
  return
}	# get_drive_type_adaptec_ibm_sun

function get_drive_type_adaptec_vxa()
{
  local disk=$1

  local serialnum=`${SMARTCTL} -i ${disk} | grep -i "serial number:" | awk '{print $3}'`
  if [ "$serialnum" = "" ]; then
    echo $TYPE_UNKNOWN_STRING
    return
  fi

  # Exact grep. Assume that only one file can return true
  grep -q " ${serialnum}$" ${ADPCLI_CONF_DISK}*
  if [ $? -ne 0 ]; then
    echo $TYPE_UNKNOWN_STRING
    return
  fi

  # Should only be one file here because serial number is unique
  local file=`grep -l " ${serialnum}$" ${ADPCLI_CONF_DISK}*`
  local model=`grep -i Model $file`
  if is_ssd_drive "${model}"; then
    echo "SSD"
    return
  fi

  # Get the Nth line with "Transfer Speed"
  local xferspeed=`grep -i "Transfer Speed" $file`
  if echo "${xferspeed}" | grep -q SAS; then
    echo "SAS"
    return
  fi
  if echo "${xferspeed}" | grep -q SATA; then
    echo "SATA"
    return
  fi

  echo $TYPE_UNKNOWN_STRING
  return
}	# get_drive_type_adaptec_vxa

function is_vm_device()
{
  local disk=$1

  # Grep for /dev/vd in the disk
  if echo "$disk" | grep -q "/dev/vd"; then
    return 0
  else
    return 1
  fi
}	# is_vm_device

#
# smartctl gives us the correct information but not in a consistent manner,
# so we need to make two different calls.
#
# This routine does not depend on an initialized drive.
#
function get_drive_type()
{
  local DISK=$1
  local TYPE

  # This is for virtual machine devices
  if is_vm_device "${DISK}"; then
    TYPE=SAS
    echo "${TYPE}"
    return
  elif is_adaptec_ibm || is_adaptec_sun; then
    TYPE=`get_drive_type_adaptec_vxa ${DISK}`
    echo "${TYPE}"
    return
  elif is_adaptec_vxa; then
    TYPE=`get_drive_type_adaptec_vxa ${DISK}`
    echo "${TYPE}"
    return
  elif is_megaraid; then
    TYPE=`get_drive_type_megaraid ${DISK}`
    echo "${TYPE}"
    return
  elif is_smart_array; then
    TYPE=`get_drive_type_smart_array ${DISK}`
    echo "${TYPE}"
    return
  elif is_3ware; then
    TYPE=`get_drive_type_3ware ${DISK}`
    echo "${TYPE}"
    return
  fi

  # Look for SSD device first based on device name since there is no other
  # standard tag for them (yet).
  #
  # "Device Model:" is present for SATA devices
  # "Device:" is present for SAS devices
  #
  # It appears that early JMFD systems reported SSD drives as SATA where later
  # Adapatec/JMFD controllers reported them as SAS.
  #
  NAME=`${SMARTCTL} -i ${DISK} | egrep -i "Device Model:|Device:"`
  if is_ssd_drive "${NAME}"; then
    echo "SSD"
    return
  fi

  TYPE=`${SMARTCTL} -i ${DISK} | grep -i "transport protocol:" | awk '{print $3}'`
  if [ "${TYPE}" = "SAS" ]; then
    echo "SAS"
    return
  fi
  if ${SMARTCTL} -i ${DISK} | grep -q "ATA Version is"; then
    echo "SATA"
    return
  fi

  TYPE=`${SMARTCTL} -i ${DISK} | grep -i "Device:" | awk '{print $2}'`
  if [ "${TYPE}" = "VMware" -o "${TYPE}" = "VMware," ]; then
    if [ -e $VMWARE_SENTINEL_FILE ]; then 
      echo "SATA"
      return
    fi
  fi
  echo $TYPE_UNKNOWN_STRING
  return
}	# get_drive_type


##########################################################
#
# Smart Array code
#
##########################################################
function get_smart_array_info()
{
  local num_arrays=`grep -c array $HPCLI_CTRL_ALL_OUT`
  local hpcli_slot="$HPCLI ctrl slot=$HPCLI_SLOT"
  local array
  local id

  for array in `seq $num_arrays`; do
    $hpcli_slot logicaldrive $array show > ${HPCLI_CTRL_LOG_OUT}.${array}
    drive=`grep physicaldrive $HPCLI_CTRL_ALL_OUT | grep -n physicaldrive | grep "^${array}:" | awk '{print $2" "$3}'`
    id=`echo $drive | awk '{print $2}'`
    $hpcli_slot $drive show > ${HPCLI_CTRL_PHYS_OUT}.${id}
    rm -f ${HPCLI_CTRL_PHYS_OUT}.${array}
    ln -s ${HPCLI_CTRL_PHYS_OUT}.${id} ${HPCLI_CTRL_PHYS_OUT}.${array}
  done
}	# get_smart_array_info


function is_invalid_smart_array_config()
{
  $HPCLI_SHOW_ALL > $HPCLI_OUT
  if [ $? -ne 0 ]; then
    logger -s "${ME}: HP CLI failed: ${HPCLI_SHOW_ALL}"
    return 0;
  fi

  local slot_num=`grep -c Slot $HPCLI_OUT`
  if [ $slot_num -ne 1 ]; then
    logger -s "${ME}: Only one Smart Array controller currently supported"
    return 0;
  fi

  HPCLI_SLOT=`grep Slot $HPCLI_OUT | awk '{print $6}'`
  $HPCLI ctrl slot=$HPCLI_SLOT show config > ${HPCLI_CTRL_ALL_OUT}
  if grep -q unassigned $HPCLI_CTRL_ALL_OUT; then
    local lineno=`grep -n unassigned $HPCLI_CTRL_ALL_OUT | sed 's/:.*//g'`
    lineno=$[$lineno - 1]
    head -${lineno} $HPCLI_CTRL_ALL_OUT > ${HPCLI_CTRL_ALL_OUT}2
    mv ${HPCLI_CTRL_ALL_OUT}2 ${HPCLI_CTRL_ALL_OUT}
  fi
  local num_arrays=`grep -c array $HPCLI_CTRL_ALL_OUT`
  local num_drives=`grep -c physicaldrive $HPCLI_CTRL_ALL_OUT`
  if [ $num_arrays -ne $num_drives ]; then
    logger -s "${ME}: At least one Array logical drive has multiple bound physical drives"
    return 0;
  fi

  get_smart_array_info
  return 1;
}	# is_invalid_smart_array_config

##########################################################
#
# Adaptec Controller code
#
##########################################################
function get_adaptec_info()
{
  $ADPCLI_GETVERS > $ADPCLI_GETVERS_OUT 2> $ADPCLI_GETVERS_ERR
  if [ $? -ne 0 ]; then
    logger -s "${ME}: Adaptec CLI failed: ${ADPCLI_GETVERS}"
    return 0;
  fi

  local cont_num=`grep "Controllers found" $ADPCLI_GETVERS_OUT | awk '{print $3}'`
  if [ $cont_num -ne 1 ]; then
    logger -s "${ME}: Only one Adaptec AAC-RAID controller currently supported"
    return 0;
  fi

  cont_num=`grep -v "Controllers found" $ADPCLI_GETVERS_OUT | grep Controller | awk '{print $2}' | sed 's/#//g'`
  $ADPCLI_GETCONF_ALL $cont_num > $ADPCLI_OUT 2> $ADPCLI_ERR
  if [ $? -ne 0 ]; then
    logger -s "${ME}: Adaptec CLI failed: ${ADPCLI_GETCONF_ALL}"
    return 0;
  fi
  return 1;
}

function is_invalid_adaptec_ibm_sun_config()
{
  # Retrive Adaptec controller info and store in files.
  if get_adaptec_info; then
    return 0;
  fi

  egrep -q "IBM ServeRAID|Sun STK RAID" $ADPCLI_OUT
  if [ $? -ne 0 ]; then
    logger -s "${ME}: Adaptec CLI output inconsistency: ${ADPCLI_GETCONF_ALL}"
    return 0;
  fi

  local num_logical=`grep -i -c "Logical device number" $ADPCLI_OUT`
  local num_drives=`grep -i -c "Device is a Hard drive" $ADPCLI_OUT`
  if [ $num_logical -ne $num_drives ]; then
    logger -s "${ME}: At least one logical device has multiple bound physical drives"
    return 0;
  fi
  return 1;
}	# is_invalid_adaptec_ibm_sun_config

# Return 1 for valid configurations, 0 for invalid configurations
function is_invalid_config()
{
  if is_smart_array; then
    is_invalid_smart_array_config
    return $?
  elif is_adaptec_ibm || is_adaptec_sun; then
    is_invalid_adaptec_ibm_sun_config
    return $?
  elif is_adaptec_vxa; then
    if get_adaptec_info; then
      return 0
    fi
  fi

  return 1;
}	# is_invalid_config

# Return the block and page sizes based on the disk type
# Note that they are expressed in KBs.
function get_disk_block_page_sizes()
{
  local ptype=$1

  if [ "$ptype" = "SSD" ]; then
    blocksz=256
    pagesz=4
  else
    blocksz=2048
    pagesz=32
  fi

  echo $blocksz $pagesz
}	# get_disk_block_page_sizes
