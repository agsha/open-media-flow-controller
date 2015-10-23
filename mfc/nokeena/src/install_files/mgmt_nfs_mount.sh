#! /bin/bash
#
#	File : mgmt_nfs_mount.sh
#
#	Description : This script is for doing an NFS mount. This script
#		will be invoked from the management module. 
#
#	Created By : Ramanand Narayanan (ramanandn@juniper.net)
#
#	Created On : 14 October, 2010
#
#	Copyright (c) Juniper Networks Inc., 2008-10
#


# Local Environment Variables
PATH=$PATH:/sbin
MOUNT_PATH=/nkn/nfs


function usage()
{
  echo
  echo "Usage: $0 <remote path> <local name> <mount options>"
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
if [ $# -lt 2 ]; then
	usage $*;
fi

# Now get the parameters
REMOTE_PATH=$1
LOCAL_NAME=$2
MOUNT_OPTS=$3

# Create the local mount path
mkdir -p ${MOUNT_PATH}/$LOCAL_NAME

# Check the return code to make sure directory is there
if [ $? != 0 ]; then 
	logger -s "${ME}: Failed to create ${MOUNT_PATH}/${LOCAL_NAME}"
	exit 1;
fi

# Now try to do the mount
mount $MOUNT_OPTS ${REMOTE_PATH} ${MOUNT_PATH}/${LOCAL_NAME}

# Check the return code to make sure the mount was successful 
if [ $? != 0 ]; then 
	logger -s "${ME}: Failed to mount ${REMOTE_PATH} on ${MOUNT_PATH}/${LOCAL_NAME}"
	exit 1;
else
	logger -s "${ME}: Successfully mounted ${REMOTE_PATH} on ${LOCAL_NAME}"

fi

exit 0;

#
# End of mgmt_nfs_mount.sh
#
