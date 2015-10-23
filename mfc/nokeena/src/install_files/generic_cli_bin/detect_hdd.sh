#!/bin/bash
#       Description: Script print out the info from sas2ircu
#          in a concise form.  Also determine the device name
#
#       Based on findslottodcno.sh
#       Copyright (c) Juniper Networks Inc., 2012
#
# Note that this script is also used in the manufacturing environment
# which is much more limited than the full MFC environment.
# There is no /opt/nkn.

# Set the Variables
SAS2IRCU=/sbin/sas2ircu
SMARTCTL=/opt/nkn/bin/smartctl
if [ ! -x ${SMARTCTL} ] ; then
  SMARTCTL=/usr/sbin/smartctl
fi
PROC_MNTINFO=/proc/mounts
TMP_DIR=/tmp/diskinfo.$$
TMP_DIR=/tmp/diskinfo
SMART_BASE=${TMP_DIR}/smart.info
TMP_FILE_BASE=${TMP_DIR}/tmp
SPLIT_FILENAME_BASE_C=${TMP_DIR}/cont_c
SPLIT_FILENAME_BASE_S=${TMP_DIR}/slot_c

# set the exit codes
# 1 = internal error
# 2 = not supported model
# 3 = sas2ircu returned error code

function is_vxa_2100()
{
  [ ! -e /etc/vxa_model ] && return 1
  # The this file sets VXA_MODEL
  . /etc/vxa_model
  [ "${VXA_MODEL}" -eq 0305 ] && return 0
  return 1
} # is_vxa_2100


# Clean up the old temp files
function Cleanup_tmpfiles()
{
  /bin/rm -rf ${TMP_DIR}
} # Cleanup_tmpfiles


function dump_smart_info()
{
  local base
  local fname

  rm -f ${SMART_BASE}.sd*
  for d in /dev/sd[a-z] /dev/sda[a-z]; do
    [ ! -b ${d} ] && continue
    base=`basename ${d}`
    fname="${SMART_BASE}.${base}"
    ${SMARTCTL} -i ${d} > ${fname} &
  done
  wait
} # dump_smart_info


function get_devname_from_smart_info()
{
  local ser_num=${1}
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
      base=`echo $files | sed "s@${SMART_BASE}.@@"`
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
        base=`echo $f | sed "s@${SMARTBASE}.@@"`
        echo "$base"
        return 0
      fi
      # If no match, then continue looking through other files
    done
    # No match found
    return 1
  fi
}        # get_devname_from_smart_info

Get_LSI_Controller_Info()
{
  local RETCODE
  local NUM
  local F
  local DEVNAME
  local MY_SPLIT_BASE
  local MY_TMPFILE

  local THIS_CONTROLLER=${1}
  local ONLY_SLOT=${2}

  MY_TMPFILE=${TMP_FILE_BASE}${THIS_CONTROLLER}.out
  MY_SPLIT_BASE_C=${SPLIT_FILENAME_BASE_C}${THIS_CONTROLLER}_E
  MY_SPLIT_BASE_S=${SPLIT_FILENAME_BASE_S}${THIS_CONTROLLER}_S
  rm -f ${MY_SPLIT_BASE_C}*
  rm -f ${MY_SPLIT_BASE_S}*

  $SAS2IRCU ${THIS_CONTROLLER} display > ${MY_TMPFILE}
  RETCODE=$?
  if [ ${RETCODE} -ne 0 ]; then
    echo "sasicru returned with error code:" ${RETCODE} >&2
    return 3
  fi
  NUM=`grep -i "Device is a hard disk" ${MY_TMPFILE} | wc -l`
  if [ ${NUM} -eq 0 ] ; then
    USE_THIS=${MY_TMPFILE}
  else
    csplit -s -z -k -f ${MY_SPLIT_BASE_C}_ ${MY_TMPFILE} \
       %"Device is a Hard disk"% /"Enclosure information"/ "1" \
       > /dev/null 2>&1
    USE_THIS=${MY_SPLIT_BASE_C}_01
  fi
  E_ENCLOSURE_NUM=`grep "^  Enclosure#" ${USE_THIS} | tail -1 | awk '{print $3}'`
  E_LOGICAL_ID=`grep    "^  Logical ID" ${USE_THIS} | tail -1 | awk '{print $4}'`
  E_NUMSLOTS=`grep      "^  Numslots"   ${USE_THIS} | tail -1 | awk '{print $3}'`
  E_STARTSLOT=`grep     "^  StartSlot"  ${USE_THIS} | tail -1 | awk '{print $3}'`

  echo \
    E_ENCLOSURE_NUM=${E_ENCLOSURE_NUM}';'  \
    E_LOGICAL_ID=\"${E_LOGICAL_ID}\"';' \
    E_NUMSLOTS=\"${E_NUMSLOTS}\"';'  \
    E_STARTSLOT=\"${E_STARTSLOT}\"';'

  echo \
    E_ENCLOSURE_NUM=';' \
    E_LOGICAL_ID=';' \
    E_NUMSLOTS=';' \
    E_STARTSLOT=';' \

  if [ ${NUM} -eq 0 ] ; then
   return
  fi

  csplit -s -z -k -f ${MY_SPLIT_BASE_S} ${MY_SPLIT_BASE_C}_00 \
     %"Device is a "% /"Device is a"/ "{$NUM}" \
     > /dev/null 2>&1
  for N in `seq 0 32`; do
    F=`printf "${MY_SPLIT_BASE_S}%02d\n" ${N}`
    if [ ! -f ${F} ]; then
      break
    fi
    D_SLOT=`grep "Slot #" ${F} | awk '{print $4}'`
    if [ "${ONLY_SLOT}" != "" ]; then
      if [ ${ONLY_SLOT} -ne ${D_SLOT} ]; then
        continue
      fi
    fi
    grep -q "Device is a Hard disk" ${F}
    [ ${?} -ne 0 ] && continue
    D_ENCLOSURE=`grep "^  Enclosure" ${F} | head -1 | awk '{print $4}'`
    D_SAS_ADDR=` grep "^  SAS Addre" ${F} | awk '{print $4}'`
    D_STATE=`    grep "^  State "    ${F} | awk '{print $3 '_' $4}'`
    D_SIZE_VAL=` grep "^  Size "     ${F} | awk '{print $6}'`
    D_SIZEUNITS=`grep "^  Size "     ${F} | awk '{print $2 $3 $4}'`
    D_MFG=`      grep "^  Manufactu" ${F} | awk '{print $3}'`
    D_MODEL_NUM=`grep "^  Model Num" ${F} | awk '{print $4}'`
    D_FIRMW_REV=`grep "^  Firmware " ${F} | awk '{print $4}'`
    D_LSI_SER=`  grep "^  Serial No" ${F} | awk '{print $4}'`
    D_GUID=`     grep "^  GUID "     ${F} | awk '{print $3}'`
    D_PROTOCOL=` grep "^  Protocol " ${F} | awk '{print $3}'`
    D_DRV_TYPE=` grep "^  Drive Typ" ${F} | awk '{print $4}'`

    D_SER_NUM=
    DEVNAME=`get_devname_from_smart_info ${D_LSI_SER}`
    RV=${?}
    if [ $? -eq 0 -a "${DEVNAME}" != "" ] ; then
      # There is a match.
      # The serial number in sas2icru doesn't look complete, so
      # Get that info from smartctl.
      D_SER_NUM=`${SMARTCTL} -i /dev/${DEVNAME} | grep -i "serial number:" | \
        awk '{print $3}'`
    fi
    echo \
        D_CONTROLLER=${THIS_CONTROLLER}';' \
        D_SLOT=${D_SLOT}';' \
        D_DEVNAME=/dev/${DEVNAME}';' \
        D_DRV_TYPE=\"${D_DRV_TYPE}\"';' \
        D_SIZE_VAL=\"${D_SIZE_VAL}\"';' \
        D_SIZEUNITS=\"${D_SIZEUNITS}\"';' \
        D_ENCLOSURE=${D_ENCLOSURE}';' \
        D_MFG=\"${D_MFG}\"';' \
        D_MODEL_NUM=\"${D_MODEL_NUM}\"';' \
        D_FIRMW_REV=\"${D_FIRMW_REV}\"';' \
        D_LSI_SER=\"${D_LSI_SER}\"';' \
        D_SER_NUM=\"${D_SER_NUM}\"';' \
        D_GUID=\"${D_GUID}\"';' \
        D_SAS_ADDR=${D_SAS_ADDR}';' \
        D_PROTOCOL=\"${D_PROTOCOL}\"';' \
        D_STATE=\"${D_STATE}\"';' \
      
  done
  echo \
        D_CONTROLLER=';' \
        D_SLOT=';' \
        D_DEVNAME=';' \
        D_DRV_TYPE=';' \
        D_SIZE_VAL=';' \
        D_SIZEUNITS=';' \
        D_ENCLOSURE=';' \
        D_MFG=';' \
        D_MODEL_NUM=';' \
        D_FIRMW_REV=';' \
        D_LSI_SER=';' \
        D_SER_NUM=';' \
        D_GUID=';' \
        D_SAS_ADDR=';' \
        D_PROTOCOL=';' \
        D_STATE=';' \

}

Cleanup_tmpfiles
[ ! -d ${TMP_DIR} ] && mkdir -p ${TMP_DIR}
[ ! -d ${TMP_FILE_BASE} ] && mkdir -p ${TMP_FILE_BASE}


is_vxa_2100
if [ ${?} -eq 0 ] ; then
  dump_smart_info
  Get_LSI_Controller_Info 0
  Get_LSI_Controller_Info 1
  #Cleanup_tmpfiles
fi

# Example 1, the content of one of the split files (minus the leading # here)
#Device is a Hard disk
#  Enclosure #                             : 1
#  Slot #                                  : 9
#  SAS Address                             : 5000c50-0-429b-eb8d
#  State                                   : Ready (RDY)
#  Size (in MB)/(in sectors)               : 140014/286749487
#  Manufacturer                            : SEAGATE 
#  Model Number                            : ST9146803SS     
#  Firmware Revision                       : 0006
#  Serial No                               : 6SD42QMS
#  GUID                                    : 5000c500429beb8f
#  Protocol                                : SAS
#  Drive Type                              : SAS_HDD

#Example 2, at the end of the output from sas2ircu (minus the leading # here)

#------------------------------------------------------------------------
#Enclosure information
#------------------------------------------------------------------------
#  Enclosure#                              : 1
#  Logical ID                              : 500062b2:000c6ec0
#  Numslots                                : 16
#  StartSlot                               : 0
#------------------------------------------------------------------------
#SAS2IRCU: Command DISPLAY Completed Successfully.
#SAS2IRCU: Utility Completed Successfully.
