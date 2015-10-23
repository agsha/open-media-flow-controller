#! /bin/bash 
#
#	File : mgmt_nfs_mountstat.sh
#
#	Description : This script is for getting status on mount path.This script
#		will be invoked from the management module. 
#
#	Created By : Varsha Rajagopalan(varshar@juniper.net)
#
#	Created On : 22 October, 2010
#
#	Copyright (c) Juniper Networks Inc., 2008-10
#

# Local Environment Variables
MOUNT_PATH=/nkn/nfs
FILEPATH=/proc/mounts


function usage()
{
  echo
  echo "Usage: $0 <local name>"
  echo 
  exit 1
}


#--------------------------------------------------------------
#
#  MAIN LOGIC BEGINS HERE 
#
#--------------------------------------------------------------

ME=`basename "${0}"`

# Check if min number of parametes are passed
if [ $# -lt 1 ]; then
	usage $*;
fi

# Now get the parameters
LOCAL_NAME=$1


# Now try to do the mount
NUM_ENTRIES=` cat ${FILEPATH} | grep " ${MOUNT_PATH}/${LOCAL_NAME} "  | wc -l | cut -f1 -d' '`

# Check the return code to make sure the mount was successful 
if [ "$NUM_ENTRIES" != "0" ]; then 
	logger -s "${ME}: Found the mount point ${MOUNT_PATH}/${LOCAL_NAME}"
else
	logger -s "${ME}: No mount point on ${MOUNT_PATH}/${LOCAL_NAME}"
	exit 1;

fi

exit 0;

#
# End of mgmt_nfs_mount.sh
#
