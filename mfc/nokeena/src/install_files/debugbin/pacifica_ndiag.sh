#! /bin/bash
NDIAG=/opt/nkn/debugbin/ndiag
#NDIAG=./ndiag
TABLE_16=/opt/nkn/debugbin/Pacifica_SATADIMM_table_16
TABLE_16_TMP=/nkn/tmp/Pacifica_SATADIMM_table_16
LOG_DIR=/nkn/tmp/ndiag.logs
BLOCKSIZE=$[256*1024]
PAGESIZE=$[4*1024]
TOTAL_DEVICES=0
VERBOSE=0
LOOP=0

# DEBUG=0: normal execution
# DEBUG=1: print what would happen and assume success
# DEBUG=2: normal execution but only test with 100 blocks
# DEBUG=3: print what would happen and assume failures
DEBUG=0

function usage()
{
  printf "Usage: $0 [dhLsTr]\n"
  printf "  d diskname: Test this disk name (sda) [alternative to slot]\n"
  printf "  h: help\n"
  printf "  s slot #: Test this slot number [required]\n"
  printf "  T #: Total number of SATADIMMs [required]\n"
  printf "  L: loop forever\n"
  printf "  r: Remove partition on the drive. Prerequisite: -d option\n"
  exit 0
}	# usage

function validate_16_satadimms()
{
  local devices=`cat $TABLE_16_TMP | awk '{print $1}'`
  local slots=`cat $TABLE_16_TMP | awk '{print $2}'`
  DEVICES=( ${devices} )
  SLOTS=( ${slots} )
  local i
  local invalid=1

  for i in `seq 0 1 15`; do
    if ( ! ls -l /dev/${DEVICES[i]} > /dev/null ); then
      echo "Device not found in slot: " ${SLOTS[i]}
      invalid=0
    fi
  done
  return $invalid
}	# validate_16_satadimms

function system_drive()
{
  local drive=$1

  num=`grep "^/dev/${drive}[0-9]" /proc/mounts | egrep -v "${drive}1 |${drive}2 " | wc -l`
  if [ $num -gt 0 ]; then
    return 0
  else
    return 1
  fi
}       # system_drive

function run_threads_drives()
{
  local all_drives=$1
  local size
  local blocks
  local cmd

  # Do not delete $LOG_DIR because this script may have multiple instances running
  mkdir $LOG_DIR >& /dev/null
  for drive in $all_drives; do
    if ( system_drive $drive ); then
      logger -s "Invalid system drive given: $drive"
      continue
    fi
    size=`cat /sys/block/${drive}/size`
    if [ $DEBUG -eq 2 ]; then
      blocks=100
    else
      blocks=$[$size/$BLOCKSIZE*512]
    fi
    if [ $VERBOSE -eq 1 ]; then
      logger -s "Start test: drive=$drive"
    fi
    cmd="$NDIAG -d /dev/${drive} -b $BLOCKSIZE -m 1 -n $blocks -p $PAGESIZE -s 0 -f $LOG_DIR/${drive}.log > $LOG_DIR/${drive}.err 2>&1 &"
    if [ $DEBUG -eq 0 -o $DEBUG -eq 2 ]; then
      eval $cmd
    elif [ $DEBUG -eq 1 ]; then
      echo $cmd
      echo "STATUS: success" > $LOG_DIR/${drive}.log
    else
      echo $cmd
      echo "STATUS: failed" > $LOG_DIR/${drive}.log
    fi
  done
  wait
}	# run_threads_drives

function check_for_success_drives()
{
  local all_drives=$1
  local failed=0
  local num_fail=0
  local num_pass=0

  for drive in ${all_drives}; do
    if ( grep -q STATUS $LOG_DIR/${drive}.log ); then
      if ! ( grep STATUS $LOG_DIR/${drive}.log | grep -q success ); then
	logger -s "Diagnostic for drive ${drive} failed"
	failed=1
	(( num_fail++ ))
      else
	(( num_pass++ ))
      fi
    else
      logger -s "Diagnostic for drive ${drive} failed to run correctly"
      failed=1
      (( num_fail++ ))
    fi
  done
  if [ $VERBOSE -eq 1 ]; then
    logger -s "Tests completed: $num_pass passed, $num_fail failed"
  fi
  if [ $DEBUG -eq 2 -a $index -eq 3 ]; then
    failed=1
  fi
  return $failed
}	# check_for_success_drives

function map_slot_to_drive()
{
  local slot=$1

  if [ $TOTAL_DEVICES -eq 16 ]; then
    if ( grep -qP "\t${slot}$" $TABLE_16_TMP ); then
      local drive=`grep -P "\t${slot}$" $TABLE_16_TMP | awk '{print $1}'`
      echo "${drive}"
      return 0
    else
      echo "Invalid slot: $slot"
      return 1
    fi
  fi
}	# map_slot_to_drive

function run_threads_slots()
{
  local all_slots=$1
  local drive
  local size
  local blocks
  local cmd

  # Do not delete $LOG_DIR because this script may have multiple instances running
  mkdir $LOG_DIR >& /dev/null
  for slot in $all_slots; do
    drive=`map_slot_to_drive $slot`
    if [ $? -ne 0 ]; then
      # print error
      echo "${drive}"
      return
    fi
    if ( system_drive $drive ); then
      echo "Invalid system drive found in slot: $slot"
      continue
    fi
    size=`cat /sys/block/${drive}/size`
    if [ $DEBUG -eq 2 ]; then
      blocks=100
    else
      blocks=$[$size/$BLOCKSIZE*512]
    fi
    if [ $VERBOSE -eq 1 ]; then
      echo "Start test: slot=$slot drive=$drive"
    fi
    cmd="$NDIAG -d /dev/${drive} -b $BLOCKSIZE -m 1 -n $blocks -p $PAGESIZE -s 0 -f $LOG_DIR/${drive}.log > $LOG_DIR/${drive}.err 2>&1 &"
    if [ $DEBUG -eq 0 -o $DEBUG -eq 2 ]; then
      eval $cmd
    elif [ $DEBUG -eq 1 ]; then
      echo $cmd
      echo "STATUS: success" > $LOG_DIR/${drive}.log
    else
      echo $cmd
      echo "STATUS: failed" > $LOG_DIR/${drive}.log
    fi
  done
  wait
}	# run_threads_slots

function check_for_success_slots()
{
  local all_slots=$1
  local failed=0
  local drive
  local num_pass=0
  local num_fail=0

  for slot in ${all_slots}; do
    drive=`map_slot_to_drive $slot`
    if ( grep -q STATUS $LOG_DIR/${drive}.log ); then
      if ! ( grep STATUS $LOG_DIR/${drive}.log | grep -q success ); then
	logger -s "Diagnostic for drive in slot ${slot} failed"
	failed=1
	(( num_fail++ ))
      else
	(( num_pass++ ))
      fi
    else
      logger -s "Diagnostic for slot ${slot} failed to run correctly"
      failed=1
      (( num_fail++ ))
    fi
  done
  if [ $VERBOSE -eq 1 ]; then
    logger -s "Tests completed: $num_pass passed, $num_fail failed"
  fi
  # failed=1 mean failure
  return $failed
}	# check_for_success_slots

##################################################################
#
# MAIN
#
##################################################################

echo "Version 2.0"
SHIFTCNT=0
while getopts "d:hLs:T:vr" options; do
  case $options in
    d )
	if [ "$OPTARG" = "all" ]; then
	  if [ $TOTAL_DEVICES -eq 16 ]; then
	    # /dev/sda is slot 9
	    FINDDRIVES="sdb sdc sdd sde sdf sdg sdh sdi sdj sdk sdl sdm sdn sdo sdp"
	  else
	    echo "'all' option not supported for this count: $TOTAL_DEVICES"
	    usage
	  fi
	else
	  FINDDRIVES="${FINDDRIVES} $OPTARG"
	fi
	(( SHIFTCNT += 2 ))
	;;
    h ) usage
	;;
    L ) LOOP=1
	;;
    s )
	if [ "$OPTARG" = "all" ]; then
	  if [ $TOTAL_DEVICES -eq 16 ]; then
	    # /dev/sda is slot 9
	    FINDSLOT="1 2 3 4 5 6 7 8 10 11 12 13 14 15 16"
	  else
	    echo "'all' option not supported for this count: $TOTAL_DEVICES"
	    usage
	  fi
	else
	  FINDSLOT="${FINDSLOT} $OPTARG"
	fi
	(( SHIFTCNT += 2 ))
	;;
    T ) TOTAL_DEVICES=$OPTARG
	(( SHIFTCNT++ ))
	;;
    v ) VERBOSE=1
	(( SHIFTCNT++ ))
	;;
    r ) if [ -n "$FINDDRIVES" ]; then
	  drive=${FINDDRIVES// }
	  # Find the total no of partitions on the drive
	  tot_partition=`fdisk -l /dev/$drive | grep -c -q "$drive[0-9]"`
	  # For each partition, wipe out every partition using 'parted'
	  # If it fails I assume that the partition is mounted, so I
	  # unmount it. I do not think it should fail after that since
	  # the partition wont be used while diagnosis. (Asked Michael)
	  if [ -n "$tot_partition" ]; then
	    while [ "$tot_partition" -gt 0 ]; do
	      temp1=`parted -i /dev/$drive rm $tot_partition`
	      if [ $? -eq 1 ]; then
		temp2=`umount /dev/${drive}${tot_partition}`
		temp3=`parted -i /dev/$drive rm $tot_partition`
	      fi
	      (( tot_partiton-- ))
	    done
	  fi
	else
	  usage
	fi
        ;;
    \? ) usage
	;;
    * ) usage
	;;
  esac
done

while [ $SHIFTCNT -gt 0 ]; do
  shift;
  (( SHIFTCNT-- ))
done

if [ "$TOTAL_DEVICES" -eq 0 ]; then
  usage
fi
if [ $TOTAL_DEVICES -eq 16 ]; then
  if [ ! -e $TABLE_16_TMP ]; then
    cp $TABLE_16 $TABLE_16_TMP
  fi
  if validate_16_satadimms; then
    printf "Cannot recognize all 16 SATADIMMS\n"
    exit 1
  fi
fi
ACTUAL_DEV=`ls /sys/block | grep -c sd`
if [ "${TOTAL_DEVICES}" -ne "${ACTUAL_DEV}" ]; then
    echo "T entered is not equal to the number of devices in the system"
    usage
fi

if [ $LOOP -eq 1 ]; then
  index=1
  while true; do
    if [ "$FINDSLOT" != "" ]; then
      run_threads_slots "$FINDSLOT"
      check_for_success_slots "$FINDSLOT"
    else
      run_threads_drives "$FINDDRIVES"
      check_for_success_drives "$FINDDRIVES"
    fi
    if [ $? -eq 0 ]; then
      logger -s "All tests completed successfully pass: $index"
      (( index++ ))
    else
      # Stop looping
      logger -s "At least one test failed, so stopping infinite loop"
      break
    fi
  done
else
  index=1
  # Run once
  if [ "$FINDSLOT" != "" ]; then
    run_threads_slots "$FINDSLOT"
    check_for_success_slots "$FINDSLOT"
  else
    run_threads_drives "$FINDDRIVES"
    check_for_success_drives "$FINDDRIVES"
  fi
  if [ $? -eq 0 ]; then
    logger -s "All tests completed successfully: $index"
    (( index++ ))
  fi
fi
