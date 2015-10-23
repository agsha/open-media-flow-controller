#!/bin/bash

#       Description: Script to find mapping between slot# to dc_X
#               We use sas2ircu utility to gather info and do the
#               mapping.
#
#       Created By : Raja Gopal Andra (randra@juniper.net)
#
#       Created On : 17 Feb, 2012
#
#       Copyright (c) Juniper Networks Inc., 2012
#
# Note that this script is also used in the manufacturing environment
# which is much more limited than the full MFC environment.
# There is no /opt/nkn, and normally no /nkn.

# Set the Variables
SAS2IRCU=/sbin/sas2ircu
SMARTCTL=/opt/nkn/bin/smartctl
if [ ! -x ${SMARTCTL} ] ; then
  SMARTCTL=/usr/sbin/smartctl
fi
PROC_MNTINFO=/proc/mounts
TMP=/nkn/tmp
SMART_BASE=$TMP/smart.info

# set the exit codes
# 1 = internal error
# 2 = not supported model
# 3 = sas2ircu returned error code

if [ ! -e /opt/nkn/bin/disks_common.sh ]; then
  logger -s "findslottodcno.sh: Missing library file"
  exit 1;
fi
source /opt/nkn/bin/disks_common.sh


# Clean up the old temp files
function cleanup_tmpfiles()
{
  local CNT=0

  ls -lt $TMP/*sas2ircu*  2>/dev/null | awk '{print $9}' > $TMP/cleanupfilelist.txt
  for i in `cat $TMP/cleanupfilelist.txt`; do
    (( CNT++ )) ;
    if [ $CNT -gt 10 ]; then
      /bin/rm -f $i
    fi
  done

  /bin/rm -f $TMP/cleanupfilelist.txt
} # cleanup_tmpfiles


function dump_smart_info()
{
  local base
  local fname

  rm -f $SMART_BASE.sd*
  for d in /dev/sd[a-z] /dev/sda[a-z]; do
    base=`basename $d`
    fname="$SMART_BASE.$base"
    $SMARTCTL -i $d > $fname &
  done
  wait
}	# dump_smart_info


function get_devname_from_smart_info()
{
  local ser_num=$1
  local smart_ser_num
  local files
  local num_files
  local base
  local model

  files=`grep -l $ser_num $SMART_BASE.*`
  num_files=`echo $files | wc -w`
  if [ $num_files -eq 1 ]; then
    model=`egrep "Device Model:|Device:|Vendor:" $files`
    if ( echo $model | grep -q SEAGATE ); then
      # SEAGATE drive only have 8 character serial numbers
      smart_ser_num=`grep -i "serial number:" $files | awk '{print $3}' | \
			cut -c 1-8`
    else
      # Non-SEAGATE drive models
      smart_ser_num=`grep -i "serial number:" $files | awk '{print $3}'`
    fi
    if [ "$smart_ser_num" = "$ser_num" ]; then
      # Just make sure the serial numbers match exactly.
      # remove "${SMART_BASE}."
      base=`echo $files | sed 's@/nkn/tmp/smart.info.@@'`
      echo "$base"
      return 0
    else
      # Should never happen.
      # A non-SEAGATE drive did not have an exact model match.
      # Should we just allow this through?
      return 1
    fi
  else
    # More than 1 file matched, so we need to find the one with the exact match
    for f in $files; do
      model=`egrep "Device Model:|Device:|Vendor:" $f`
      if ( echo $model | grep -q SEAGATE ); then
	# SEAGATE drive only have 8 character serial numbers
	smart_ser_num=`grep -i "serial number:" $f | awk '{print $3}' | \
			cut -c 1-8`
      else
	# Non-SEAGATE drive models
	smart_ser_num=`grep -i "serial number:" $f | awk '{print $3}'`
      fi
      if [ "$smart_ser_num" = "$ser_num" ]; then
	# Just make sure the serial numbers match exactly.
	base=`echo $f | sed 's@/nkn/tmp/smart.info.@@'`
	echo "$base"
	return 0
      fi
      # If no match, then continue looking through other files
    done
    # No match found
    return 1
  fi
}	# get_devname_from_smart_info


# Function to display LSI controller information
function display_lsi_controller_info()
{
  local CNO=$1
  local SLOTBASE
  local VIR_SLOT
  local RETCODE
  local NUM
  local S2I_F
  local S2I_SLOTNO
  local S2I_ENCNO
  local S2I_SER_NUM
  local DEVNAME

  ( $SAS2IRCU $CNO display ) > $TMP/sas2ircu_$CNO.$$.out 2> $TMP/sas2ircu_$CNO.$$.time

  RETCODE=$?
  if [ $RETCODE -ne 0 ]; then
    Print "sas2ircu returned with error code:" $RETCODE
    if ( grep -q "No Controller Found at index 1" $TMP/sas2ircu_$CNO.$$.out ); then
      logger "findslottodcno.sh: LSI controller 1 not found"
      return 0
    fi
    exit 3
  fi

  NUM=`grep -i "Device is a hard disk" $TMP/sas2ircu_$CNO.$$.out | wc -l`
  csplit -s -z -k -f $TMP/sas2ircu_$CNO$$_ $TMP/sas2ircu_$CNO.$$.out %"Device is a Hard disk"% /"Device is a Hard disk"/ "{$NUM}" > /dev/null 2>&1
  # Sample file from the csplit is as below
  #
  # Device is a Hard disk
  #   Enclosure #                             : 1
  #   Slot #                                  : 0
  #   SAS Address                             : 5000c50-0-4092-6151
  #   State                                   : Ready (RDY)
  #   Size (in MB)/(in sectors)               : 476940/976773167
  #   Manufacturer                            : SEAGATE
  #   Model Number                            : ST9500620SS
  #   Firmware Revision                       : 0002
  #   Serial No                               : 9XF0NGL1
  #   GUID                                    : 5000c50040926153
  #   Protocol                                : SAS
  #   Drive Type                              : SAS_HDD
  # We need to extract the required data from this file.

  # Print the header
  Print "Slot#   DeviceName         Mount_Pt          Serial_No"
  if [ "${LIST_ALL}" == "yes"  ]; then
    if [ $CNO -ne 0 ]; then
      (( SLOTBASE=16 ));
    fi
  fi

  for N in `seq 0 64`; do
    S2I_F=`printf "$TMP/sas2ircu_$CNO$$_%02d\n" ${N}`
    if [ ! -f ${S2I_F} ]; then
      continue;
    fi
    S2I_SLOTNO=`grep -i  "Slot #" $S2I_F | head -1 | awk '{print $4}'`
    if [ "${SNO}" != "" ]; then
      if [ $SNO -ne $S2I_SLOTNO ]; then
        continue;
      fi
    fi

    # when shelves are dual attached (one to each port), we will have
    # two enclosures for single controller and hence need to take care
    # of the slotno. The Enclosure # will be "3" for the third shelf and
    # hence we need to add "24" to get the right slotno for disks in the
    # 3rd enclosure. The assumption here is that we have "16" slots in
    # internal shelf and "24" in each external JbodShelf.
    S2I_ENCNO=`grep -i  "Enclosure #" $S2I_F | head -1 | awk '{print $4}'`
    if [ "${S2I_ENCNO}" == "3" ]; then
      S2I_SLOTNO=$S2I_SLOTNO+24
    fi

    S2I_SER_NUM=`grep -i  "Serial No" $S2I_F | head -1 | awk '{print $4}'`
    DEVNAME=`get_devname_from_smart_info $S2I_SER_NUM`
    if [ $? -ne 0 ]; then
      # there is no match
      continue;
    fi

    # Serial number in sas2icru doesn't look complete
    GOOD_SER_NUM=`$SMARTCTL -i /dev/$DEVNAME | grep -i "serial number:" | \
			awk '{print $3}'`
    if  [ "${DEVNAME}" != "" ]; then
      grep -q "${DEVNAME}2" $PROC_MNTINFO
      if [ $? -eq 0 ]; then
	MNTPT=`grep "${DEVNAME}2" $PROC_MNTINFO | grep dc_ | \
			head -1 | awk '{print $2}'`
	# May be "" if this disk is really the system data disk
      else
        MNTPT="not_mounted                        "
      fi
      if [ "${MNTPT}" == "" ]; then
        MNTPT="not_mounted                        "
      fi
      (( VIR_SLOT=SLOTBASE+S2I_SLOTNO ))
      if [ "${INFO_ONLY}" != "yes" ]; then
        echo $VIR_SLOT "     " /dev/$DEVNAME "      " $MNTPT "       " $GOOD_SER_NUM
      else
        echo $VIR_SLOT /dev/$DEVNAME $MNTPT $GOOD_SER_NUM
      fi
    fi
  done
} # display_lsi_controller_info


# Method to check if previous findslot output can be reused. We check if the
# previous output(/nkn/tmp/findslot.last) is newer then write_diskinfo_to_db.log
# file. /nkn/tmp/findslot.last is updated every time "findslottodcno.sh -a" is
# run and contains the last run information.
function get_previous_findslot_output()
{
  local WRITEDBFILE

  # There is typically only 1 write_diskinfo_to_db.log file, but just to be
  # sure we look for the latest one.
  WRITEDBFILE=`ls -lt /nkn/tmp/write_diskinfo_to_db.log* | head -1 | awk '{print $9}'`
  if [ $LASTFINDSLOTFILE -nt $WRITEDBFILE ]; then
    cat $LASTFINDSLOTFILE
    return 0
  else
    return 1
  fi
  wait
} # get_previous_findslot_output

# Method which calls the appropriate disk controllers to retrieve the info
function get_controller_info()
{

  if [ "${LIST_ALL}" != "yes"  ]; then
    display_lsi_controller_info $CONT_NUM
  else
    if [ "${LIST_PREVINFO}" == "yes"  ]; then 
      if get_previous_findslot_output ; then
        # the findslot info is newer then mount-new
	return 0
      fi
    fi
    (display_lsi_controller_info 0 > $TMP/sas2ircu_0_0.out) &
    display_lsi_controller_info 1 > $TMP/sas2ircu_1_0.out
    wait
    cat $TMP/sas2ircu_0_0.out >  $TMP/sas2ircu_2_0.out
    cat $TMP/sas2ircu_1_0.out | grep -v Slot >> $TMP/sas2ircu_2_0.out
    cp $TMP/sas2ircu_2_0.out $LASTFINDSLOTFILE
    cat $TMP/sas2ircu_2_0.out
    cleanup_tmpfiles
  fi
} # get_controller_info


function Print()
{
  if [ "${INFO_ONLY}" != "yes" ]; then
    echo "${@}"
  fi
}


# usage info
function usage()
{
  Print "findslottodcno.sh [-c <controller#> [-s <slot#>] | -a] [-q] [-C]"
  Print "  -a: List all controllers info"
  Print "  -c #: Print info for specfic controller"
  Print "  -s #: Print info for specific slot (must be given with -c)"
  Print "  -q: Only print the info in a concise form; give with another option"
  Print "  -C: print the info from previous run if there are no changes; should"
  Print "    be given with option -a"
  Print "  -h: Help"
  exit 1
} # usage


#******************************************************
#
# Main Program
#
#******************************************************
# Main logic starts here
# step 1. Check if it's vxa 2100 or not
# step 2. Retrieve the requested information by calling appropriate
# disk controller specific routines

if [ $# -lt 1 ]; then
  usage;
fi

# process the command arguments
SHIFTCNT=0
while getopts ":c:s:aqC" opt; do
  case $opt in
    c ) CONT_NUM=$OPTARG
        (( SHIFTCNT++ ))
        ;;
    s ) SNO=$OPTARG
        (( SHIFTCNT++ ))
        ;;
    C ) LIST_PREVINFO=yes
        ;;
    a ) LIST_ALL=yes
        ;;
    q ) # Only print out the info. Must be given with another option.
        INFO_ONLY=yes
        ;;
    \? ) usage
        ;;
    * ) usage
        ;;
  esac
  (( SHIFTCNT++ ))
done

if [ "${LIST_ALL}" != "yes" ]; then
  if [ ! $(echo "$CONT_NUM" | grep -E "^[0-9]+$") ]; then
    Print "Integer expected for Controller"
    usage
  fi

  if [ $SHIFTCNT -gt 2 ]; then
    if [ ! $(echo "$SNO" | grep -E "^[0-9]+$") ]; then
      Print "Integer expected for slot number"
      usage
    fi
  fi
else
  SNO=""
fi

while [ $SHIFTCNT -gt 0 ]; do
  shift;
  (( SHIFTCNT-- ))
done

# Clean up old tmp files
cleanup_tmpfiles

# Check if system is VXA 2100 or not
if ! is_vxa_2100 ; then
  Print "findslottodcno.sh is not supported on this platform"
  Print "findslottodcno.sh is only supported on VXA 2100"
  exit 2
fi

#
# To debug this code, run dump_smart_info once and comment out.
# Then run get_devname_from_smart_info and pass various serial
# numbers, like what is shown below.
#
dump_smart_info
#get_devname_from_smart_info "6XS19RF4"
get_controller_info
