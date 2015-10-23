#! /bin/bash
#
#	File : mgmt_build_ns_object_list.sh
#
#	Description : This script  builds a file with the  list of objects in 
#			the cache for a given namespace 
#
#	Created By : Ramanand Narayanan (ramanand@ankeena.com)
#
#	Created On : 17 November, 2009
#
#	Copyright (c) Ankeena Inc., 2009
#


# Local Environment Variables
PATH=$PATH:/sbin
LOG_DIR=/var/log/nkn/cache
CACHE_DISK_LIB=/opt/nkn/bin/cache_disk_lib.sh
MDREQ=/opt/tms/bin/mdreq
TM_NAMESPACE=/nkn/nvsd/namespace
LINES_TO_DISPLAY=50

TM_RAMCACHE_ACTION=/nkn/nvsd/namespace/actions/get_ramcache_list


function usage()
{
  echo
  echo "Usage: mgmt_build_ns_object_list.sh  <namespace>"
  echo 
  exit 1
}


# Get the parameters into readable variable names
function parse_params()
{
	NAMESPACE=$1;
	shift;
}

# Check if the namespace is active
function namespace_activated()
{
  local namespace=$1
  local val=`${MDREQ} -v query get - ${TM_NAMESPACE}/${namespace}/status/active`
  if [ "${val}" = "true" ]; then
    return 0
  else
    return 1
  fi
}

# Get the list of active disks
function get_active_disks()
{
  local disks=`list_disk_caches`
  local disk

  ACTIVE_DISKS="";
  for disk in ${disks}; do
    serial_num=$disk
    fs_part=`disk_get ${disk} ${TM_FS_PART}`
    diskname=`get_diskname ${fs_part}`

    # Need to check that the disk exists in the system because it can be
    # pulled while in a 'cache enabled' 'status active' state.
    if disk_missing_after_boot ${serial_num}; then
      continue;
    fi
    if disk_commented_out ${serial_num}; then
      continue;
    fi
    if ! disk_activated ${serial_num}; then
      continue; 
    fi
    if ! disk_enabled ${serial_num}; then
      continue;
    fi
    if ! disk_running $diskname; then
      continue;
    fi

    # It is a disk of interest
    ACTIVE_DISKS="$ACTIVE_DISKS $diskname"
  done
}

#--------------------------------------------------------------
#
#  MAIN LOGIC BEGINS HERE 
#
#--------------------------------------------------------------

# Include the disk utility functions library
if [ -f $CACHE_DISK_LIB ]; then 
	. $CACHE_DISK_LIB;
else
	echo "Failed to include $CACHE_DISK_LIB, unable to continue";
	exit 2;
fi

# Check if min number of parametes are passed
if [ $# -lt 1 ]; then
	usage;
fi

# Now get the parameters
parse_params $*;

# Create the name of the LOG file and the Lock file
OBJECT_LIST_FILE=${LOG_DIR}/ns_objects_${NAMESPACE}.lst
LOCK_FILE=${LOG_DIR}/ns_objects_${NAMESPACE}.lst.lck

# Check if namespace is active
if ! namespace_activated ${NAMESPACE}; then
	echo "${NAMESPACE} : namespace inactive";
fi

# Get the active disk list
get_active_disks;

# Get the UID for the namespace
NS_UID=`${MDREQ} -v query get - ${TM_NAMESPACE}/${NAMESPACE}/uid`

# If lock file exists then it means the task is already in progress
# Hence, nothing to do
if [ -f ${LOCK_FILE} ]; then
	exit 0;
else
	# Check if the list file exists
	if [ -f ${OBJECT_LIST_FILE} ]; then 
		# If the number of objects in the old list is above 10K 
		# we would like to create a 2 min pause between subsequent
		# requests

		old_list_count=`wc -l ${OBJECT_LIST_FILE} | cut -d' ' -f1`;
		if [ "$old_list_count" -gt "10000" ]; then 
			# Now find the time since that last update 
			# to the object list file
			file_change_time=`stat -c"%Z" ${OBJECT_LIST_FILE}`;
			curr_time=$(date +%s);
			time_diff=$((curr_time - $file_change_time));

			# Check if time difference is less than 120 sec 
			# then sleep for the remaining seconds
			if [ "$time_diff" -lt "120" ]; then
				# Create the lock file
				touch $LOCK_FILE;

				sleep_time=$((120 - $time_diff));
				logger "Object List Creation for Namespace '${NAMESPACE}': Pausing $sleep_time seconds"
				sleep $sleep_time;
			fi
		fi
	fi
	
	# Create the lock file
	touch $LOCK_FILE;

	# Remove the old list file
	rm -f  $OBJECT_LIST_FILE;
fi

# Now get the ramcache object list first
${MDREQ} action ${TM_RAMCACHE_ACTION} namespace string ${NAMESPACE} uid string ${NS_UID} filename string ${OBJECT_LIST_FILE}

logger "Object List Creation for Namespace '${NAMESPACE}': STARTED"
# Now if the file got created then append to it 
if [ ! -f ${OBJECT_LIST_FILE} ]; then 
	echo "Generated on `date`" > $OBJECT_LIST_FILE;
	echo >> $OBJECT_LIST_FILE;
fi

# Now get the directory list 
# Find the directories that match the prefixed UID
for  i in $ACTIVE_DISKS
do
	for j in /nkn/mnt/$i/${NS_UID}_*
	do
		#Get the container read done here itself,Avoiding an extra loop
		#DIR_LIST="$DIR_LIST $j";
		# First check if the path exists
		if [ ! -d $j ]; then
		    continue;
		fi

		# Get the domain name from directory name 
		DOMAIN_NAME=`echo $j | sed -e s@^.*${NS_UID}_@@g`;

		echo "*  Loc   Size (KB)          Expiry                             URL" >> $OBJECT_LIST_FILE;
		echo "*---------------------------------------------------------------------------------------" >> $OBJECT_LIST_FILE;

		# Get the dc_? from the directory name
		DC_NAME=`echo $j | cut -d'/' -f4`;	# Use / as delimiter and get the 3rd col

		# Since there could be spaces in the directory name set the IFS to not take
		# space as delimiter
		IFS=$'\t\n'
		EMPTY=`find $j -name container | wc -l`
		if [ "$EMPTY" = "0" ]; then
		continue;
		fi

		/opt/nkn/bin/container_read -m -R $j >> $OBJECT_LIST_FILE;
		IFS=$' \t\n'

		echo "--------------------------------------------------------" >> $OBJECT_LIST_FILE;
	done
done




echo "End of cache objects for namespace ${NAMESPACE}" >> $OBJECT_LIST_FILE;
echo "--------------------------------------------------------" >> $OBJECT_LIST_FILE;
logger "Object List Creation for Namespace '${NAMESPACE}': COMPLETED"

# Now that we are done remove the locak file and  cat the top N file and leave 
rm -f $LOCK_FILE;
exit 0;


#
# mgmt_build_ns_object_list.sh
#
