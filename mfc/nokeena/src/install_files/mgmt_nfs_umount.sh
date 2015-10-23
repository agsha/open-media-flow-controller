#! /bin/bash
#
#	File : mgmt_nfs_umount.sh
#
#	Description : This script is for doing an NFS umount. This script
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


# Now try to do the umount
umount ${MOUNT_PATH}/${LOCAL_NAME}

# Check the return code to make sure the umount was successful
if [ $? != 0 ]; then 
	logger -s "${ME}: Failed to umount ${MOUNT_PATH}/${LOCAL_NAME}"
	exit 1;
else
	logger -s "${ME}: Successfully unmounted ${LOCAL_NAME}"

	# Now remove the mount point
	rmdir ${MOUNT_PATH}/${LOCAL_NAME}
fi

exit 0;

#
# End of mgmt_nfs_umount.sh
#
