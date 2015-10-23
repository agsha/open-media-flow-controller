#! /bin/bash

SM=/nkn/tmp/show_media.tmp
AC=/usr/StorMan/arcconf

function cat_media()
{
/opt/tms/bin/cli << EOF
  en
  show media-cache disk list
EOF
} # cat_media

function get_media()
{
  cat_media > $SM
}

# Taken from MFG script
function get_model()
{
  SERIAL_NUMBER=`/usr/sbin/dmidecode -s system-serial-number`
  MODEL_NUMBER=`echo "${SERIAL_NUMBER}" | cut -c1-4`
  case ${MODEL_NUMBER} in
    0277|0278|0279|0280|0304|0305|0306) IS_VXA=yes ;;
  esac
}  # get_model


# Taken from MFG script
function get_drives()
{
  case ${MODEL_NUMBER} in
    0279)
      # Model Number = 0279
      # VXA1001 :
      # 1U, Minimum RAM 4GB,
      # Network interfaces allowed 2x1gbe,
      # Storage: 1xSATA500GB (Total 500GB),
      MODEL_NAME=VXA1001
      MIN_RAMG=4
      DRIVES_MAX=1
      ;;
    0280)
      # Model Number = 0280
      # VXA1002:
      # 1U, Minimum RAM 8GB,
      # Network interfaces allowed: 4x1gbe,
      # Storage: 4xSATA500GB (Total 2TB),
      MODEL_NAME=VXA1002
      MIN_RAMG=8
      DRIVES_MAX=4
      ;;
    0304)
      # Model Number = 0304
      # VXA1100:
      # 1U, Minimum RAM 48GB, Max 144GB
      # Network interfaces allowed: 4x1gbe, 2x10Gbe,
      # Storage: max 8 drives, any combo
      MODEL_NAME=VXA1100
      MIN_RAMG=48
      DRIVES_MAX=8
      ;;
    0277)
      # Model Number = 0277
      # VXA2002:
      # 2U, Minimum RAM 36GB,
      # Network interfaces allowed 4x1gbe, 2x10Gbe,
      # Storage: 4xSAS500GB (Total 2 TB),
      MODEL_NAME=VXA2002
      MIN_RAMG=36
      DRIVES_MAX=4
      ;;
    0278)
      # Model Number: 0278
      # VXA2010:
      # 2U,Minimum RAM 36GB,
      # Network interfaces allowed 4x1gbe, 2x10Gbe,
      # Storage: 14xSAS500GB + 2xSSD64GB (Total of 7.128 TB),
      MODEL_NAME=VXA2010
      MIN_RAMG=36
      DRIVES_MAX=16
      ;;
    0305)
      # Model Number = 0305
      # VXA2100:
      # 2U, Minimum RAM 48GB, Max 144GB
      # Network interfaces allowed: 4x1gbe, 2x10Gbe,
      # Storage: max 16 drives, any combo, plus mini-SAS port
      MODEL_NAME=VXA2100
      MIN_RAMG=48
      DRIVES_MAX=16
      ;;
    0306)
      # Model Number = 0306
      # VXA2200:
      # 2U, Minimum RAM 96GB, Max 144GB
      # Network interfaces allowed: 4x1gbe, 2x10Gbe,
      # Storage: max 16 drives, any combo, plus mini-SAS port
      MODEL_NAME=VXA2200
      MIN_RAMG=96
      DRIVES_MAX=16
      ;;
    *)
      echo Installation is not supported on this model number: ${MODEL_NUMBER}
      echo Installation is only supported on the following:
      echo VXA1001 - model number 0279
      echo VXA1002 - model number 0280
      echo VXA1100 - model number 0304
      echo VXA2002 - model number 0277
      echo VXA2010 - model number 0278
      echo VXA2100 - model number 0305
      echo VXA2200 - model number 0306
  esac
}  # get_drives


function verify_disk()
{
  local slot=$1
  local drive_type=$2

  local line=`grep "dc_${slot} " $SM`
  local ret=0
  local sm_drive_type=`echo $line | awk '{print $2}'`
  local sm_state=`echo $line | awk '{print $8" "$9}'`

  if [ "${drive_type}" != "${sm_drive_type}" ]; then
    echo "DISK ${slot} should have ${drive_type} drive"
    ret=1
  fi
  if [ "${sm_state}" != "cache running" ]; then
    echo "DISK ${slot} should have 'cache running' state"
    ret=1
  fi
  return $ret
}  # verify_disk


function verify_disks_1001()
{
  verify_disk 1 SATA
  return $?
}  # verify_1001


function verify_disks_1002()
{
  local ret=0
  local ret2

  for i in `seq 1 1 4`; do
    verify_disk $i SATA
    ret2=$?
    if [ $ret2 != 0 ]; then
      ret=$ret2
    fi
  done
  return $ret
}  # verify_disks_1002

function verify_disks_1100()
{
  echo TBD: verify_disks_1100
  return 0
}  # verify_disks_1100

function verify_disks_2002()
{
  local ret=0
  local ret2

  for i in `seq 1 1 4`; do
    verify_disk $i SAS
    ret2=$?
    if [ $ret2 != 0 ]; then
      ret=$ret2
    fi
  done
  return $ret
  return 0
}  # verify_disks_2002


function verify_disks_2010()
{
  local ret=0
  local ret2

  for i in `seq 1 1 14`; do
    verify_disk $i SAS
    ret2=$?
    if [ $ret2 != 0 ]; then
      ret=$ret2
    fi
  done
  verify_disk 15 SSD
  ret2=$?
  if [ $ret2 != 0 ]; then
    ret=$ret2
  fi
  
  verify_disk 16 SSD
  ret2=$?
  if [ $ret2 != 0 ]; then
    ret=$ret2
  fi
  return $ret
}  # verify_disks_2010

function verify_disks_2100()
{
  echo TBD: verify_disks_2100
  return 0
}  # verify_disks_2100

function verify_disks_2200()
{
  echo TBD: verify_disks_2200
  return 0
}  # verify_disks_2200

function verify_no_logical_volumes()
{
  $AC getconfig 1 ld | grep -q "No logical devices configured"
  if [ $? -eq 0 ]; then
    return;
  else
    echo "FAIL: No logical devices should be configured"
    exit 1
  fi
}  # verify_no_logical_volumes

function verify_vxa_write_through()
{
  $AC getconfig 1 pd | grep -q "write-back"
  if [ $? -eq 0 ]; then
    echo "FAIL: At least one device is configured as write-back instead of write-through"
    exit 1
  fi
}  # verify_vxa_write_through

function verify_model()
{
  case ${MODEL_NUMBER} in
    0279)
      verify_disks_1001
      ret=$?
      ;;
    0280)
      verify_disks_1002
      ret=$?
    0304)
      verify_no_logical_volumes
      # TBD: Any other tests?
      verify_disks_1100
      ret=$?
      ;;
    0277)
      verify_no_logical_volumes
      verify_vxa_write_through 4
      verify_disks_2002
      ret=$?
      ;;
    0278)
      verify_no_logical_volumes
      verify_vxa_write_through 16
      verify_disks_2010
      ret=$?
      ;;
    0305)
      verify_no_logical_volumes
      # TBD: Any other tests?
      verify_disks_2100
      ret=$?
    0306)
      verify_no_logical_volumes
      # TBD: Any other tests?
      verify_disks_2100
      ret=$?
    *)
      echo "FAIL: Unknown VXA hardware model ${MODEL_NUMBER}"
      exit 1
      ;;
  esac
  return $ret
}  # verify_model

########################################################################
#
#  Main program
#
########################################################################
get_media
get_model
get_drives
if [ "$IS_VXA" = "yes" ]; then
  verify_model
  if [ $? -ne 0 ]; then
    echo "FAIL: Some part of this system is not configured correctly"
    exit 1
  fi
else
  echo "FAIL: This system is not Juniper approved hardware"
  exit 1;
fi
echo "PASS: "
echo "  - Correct number of drives are installed"
echo "  - Correct drive types are in each slot"
echo "  - No logical volumes are created"
echo "  - Correct JBOD devices are created"
echo "  - Write through mode enabled for each drive"
