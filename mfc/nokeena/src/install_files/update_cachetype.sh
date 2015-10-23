#!/bin/sh

#
#	File : update_cachetype.sh
#
#	Purpose : This script takes in 3 params and updates the file
#		/config/nkn/diskcache-startup.info
#		This is called from the CLI and is to change the cache type
#		of a disk manually.The CLI takes care of updating the node 
#		information. This script it only to change the info file.
#		NOTE : Today, the updated info file will get overwritten with
#			a reboot. Michael will fix this later as it does not
#			affect functionality at this time. 
#	
#	Created By : Ramanand Narayanan (miken@nokeena.com)
#
#	Created On : 09 November, 2009
#
#	Copyright (c) Ankeena Inc., 2009
#
#

CACHE_CONFIG=/config/nkn/diskcache-startup.info


function print_usage()
{
	echo;
	echo "Usage: $ME <disk name> <old cache type> <new cache type>";
	echo;
}


# --------------------------------------------------------------------------- #
#
#  MAIN LOGIC BEGINS HERE 
#
# --------------------------------------------------------------------------- #

ME=`basename "${0}"`

# First check if there are 3 parameters
if [ $# -ne 3 ]; then
	print_usage $*;
	exit 1;
fi

DISK_NAME=$1;
shift;

OLD_CACHE_TYPE=$1;
shift;

NEW_CACHE_TYPE=$1;
shift;

echo
logger "${ME}: Changing the cache-type of disk ${DISK_NAME} to ${CACHE_TYPE}"

# Now backup the current cache config file
cp ${CACHE_CONFIG} ${CACHE_CONFIG}.orig
for i in `seq 5 -1 2`; do
  if [ -e ${CACHE_CONFIG}.bak$[ $i-1 ] -a -e ${CACHE_CONFIG}.bak${i} ]; then
    mv -f ${CACHE_CONFIG}.bak${i} ${CACHE_CONFIG}.bak$[ $i+1 ]
  fi
done
if [ -e ${CACHE_CONFIG}.bak1 ]; then
  mv -f ${CACHE_CONFIG}.bak1 ${CACHE_CONFIG}.bak2
fi
if [ -e ${CACHE_CONFIG} ]; then
  mv -f ${CACHE_CONFIG} ${CACHE_CONFIG}.bak1
fi

# Now do the change 
sed -e 's/\(^'${DISK_NAME}' .*\)'${OLD_CACHE_TYPE}'/\1'${NEW_CACHE_TYPE}'/g' ${CACHE_CONFIG}.orig > ${CACHE_CONFIG}

# Get rid of the temporary file
rm -f ${CACHE_CONFIG}.orig

exit 0;

#
# End of update_cachetype.sh
#
