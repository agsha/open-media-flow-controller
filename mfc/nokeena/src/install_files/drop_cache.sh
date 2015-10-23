#! /bin/bash
#
#	File : drop_cache.sh
#
#	Description : This script runs the drop_cache_awk in a while loop
#
#	Created By : Ramanand Narayanan (ramanandn@juniper.net)
#
#	Created On : 13 April, 2011
#
#	Copyright (c) Juniper Networks Inc., 2011
#


# Set the Variables
DROP_CACHE_AWK_SCRIPT=/opt/nkn/bin/drop_cache.awk
PROC_MEMINFO=/proc/meminfo
SLEEP_TIME=10
DROP_THRESHOLD_PREREAD_VAL="Threshold: 20"
DROP_THRESHOLD_PREREAD=/config/nkn/drop_cache_threshold_preread

# Check if pre-read done
check_if_pre_read_done()
{
  # get sata.dm2_preread_done
  SATA_PREREAD_DONE=`/opt/nkn/bin/nkncnt -s SATA.dm2_preread_done -t 0 | grep dm2 | awk '{print $3;}'`

  # get sas.dm2_preread_done
  SAS_PREREAD_DONE=`/opt/nkn/bin/nkncnt -s SAS.dm2_preread_done -t 0 | grep dm2 | awk '{print $3;}'`

  # get ssd.dm2_preread_done
  SSD_PREREAD_DONE=`/opt/nkn/bin/nkncnt -s SSD.dm2_preread_done -t 0 | grep dm2 | awk '{print $3;}'`

  #logger -s "sata,ssd,sas ${SATA_PREREAD_DONE}, ${SSD_PREREAD_DONE}, ${SAS_PREREAD_DONE}"

  if [ "${SATA_PREREAD_DONE}" != "" ]
  then
    # SATA present
    if [ ${SATA_PREREAD_DONE} -eq 0 ]
    then
	return 1
    fi
  fi
  if [ "${SAS_PREREAD_DONE}" != "" ]
  then
    # SAS present
    if [ ${SAS_PREREAD_DONE} -eq 0 ]
    then
	return 1
    fi
  fi
  if [ "${SSD_PREREAD_DONE}" != "" ]
  then
    # SSD present
    if [ ${SSD_PREREAD_DONE} -eq 0 ]
    then
    	return 1
    fi
   fi
  # No disk tiers.. bail
  return 0
}


#--------------------------------------------------------------
#
#  MAIN LOGIC BEGINS HERE
#
#--------------------------------------------------------------
echo $DROP_THRESHOLD_PREREAD_VAL > $DROP_THRESHOLD_PREREAD
while [ 1 ]; do

	DROP_THRESHOLD=/config/nkn/drop_cache_threshold
	#check if pread is in progress,if set DROP_THRSHOLD to 20
	check_if_pre_read_done;
    	if [ $? -eq 1 ] ; then
	   DROP_THRESHOLD=$DROP_THRESHOLD_PREREAD
	fi
	# Run the awk script 
	$DROP_CACHE_AWK_SCRIPT $PROC_MEMINFO $DROP_THRESHOLD

	# Sleep for SLEEP_TIME seconds before invoking the awk script again
	sleep $SLEEP_TIME;
done

#
# End of drop_cache.sh
#
