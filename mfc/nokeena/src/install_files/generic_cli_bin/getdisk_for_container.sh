#! /bin/bash
#
#       File : getdisk_for_container.sh
#
#       Description : This script gets the disk info for the given container
#
#       Created By : Raja Gopal Andra (randra@juniper.net)
#
#       Created On : 08 Feb, 2012
#
#       Copyright (c) Juniper Networks Inc., 2012
#

# Set the Variables
PROC_MNTINFO=/proc/mounts
DISK_CONFIG_INFO=/config/nkn/diskcache-startup.info

#usage info
usage() {
  echo "getdisk_from_container.sh <contname>"
  echo "  -h: help"
  exit 1
}

if [ $# -lt 1 ]; then
  usage;
fi


#--------------------------------------------------------------
#
#  MAIN LOGIC BEGINS HERE
#
# We look up for given container name(expected in the format of 
# /nkn/mnt/dc_X) in /proc/mounts and then extract the associated
# disk for this mount. The we  look for actual disk information
# by looking at the configuration file.
#--------------------------------------------------------------
for i in `grep -w $1 $PROC_MNTINFO  | awk '{print $1}'` ;
do
  grep -w $i $DISK_CONFIG_INFO  | awk '{print $2}' ;
done

#
# End of getdisk_for_container.sh
#
