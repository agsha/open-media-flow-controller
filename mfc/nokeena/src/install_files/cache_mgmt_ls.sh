#! /bin/bash 
#
#	File : cache_mgmt_ls.sh
#
#	Modified by : Ramanand Narayanan 
#	Date : April 21st, 2009
#
CACHE_DISK_LIB=/opt/nkn/bin/cache_disk_lib.sh
MAX_DISPLAY=50
HEADER_LINES=6
LOG_DIR=/var/log/nkn/cache
BUILD_OBJECT_LIST_SCRIPT=/opt/nkn/bin/mgmt_build_ns_object_list.py

# Echos the head of the list file 
function echo_list_head()
{
	local LIST_FILE=$1;

	# Now cat the log file out for mgmt interface
	NUM_ENTRIES=`grep "^ " $LIST_FILE | wc -l | cut -f1 -d' '`
	NUM_LINES=`cat $LIST_FILE | wc -l | cut -f1 -d' '`

	if [ "$NUM_LINES" -gt "$MAX_DISPLAY" ]; then
		echo "Possible number of objects: $NUM_ENTRIES (approx) "
		echo "Note:";
		echo "  Displaying the first $MAX_DISPLAY lines";
		echo "";
		echo "  Background task is in progress for collecting the full object list"
		echo "  Please check system log for the task completion"
		echo "  For exact number of objects please download the full object list file"
		echo ""
		echo "  Download the full object list file using the command: ";
		echo "\"upload object list <namespace> scp://<username>@<hostname>/<path>/<filename>\"";
		echo 
	else
		echo "Total number of objects: $NUM_ENTRIES "
	fi


	# fix for 1642 skipping three header lines [ if any header line is added please increment the header lines
	MAX_DISPLAY=`expr $MAX_DISPLAY + $HEADER_LINES`
	# Print the top MAX_DISPLAY entries if there are objects
	if [ "$NUM_ENTRIES" -gt "0" ]; then
		# First print the header portion from the list file
		head -$HEADER_LINES $LIST_FILE

		# Now print the first MAX_DISPLAY lines
		grep -e "RAM" -e "dc_*" $LIST_FILE | head -n50
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

# Sanity check on the parameters
if [ $# -lt 3 ]; then
  echo "Usage: cache_mgmt_ls <namespace> <all|<pattern>> <all|domain>"
  echo 
  exit 0
fi

# Get the UID from the parameter
NAMESPACE=$1;
shift;

# Get the pattern
OBJ_PATTERN=$1;
shift;

# Get the domain to search 
SEARCH_DOMAIN=$1;
shift;

# Get the UID as well
NS_UID=$1;
shift;

# Get the Port
SEARCH_PORT=$1;

#Search for single URI
SINGLE_OBJECT_LIST_FILE=${LOG_DIR}/ns_single_objects_${NAMESPACE}.lst
GREPED_SINGLE_OBJECT_LIST_FILE=${LOG_DIR}/ns_single_objects_${NAMESPACE}_grep.lst
GREPED_SINGLE_OBJECT_LIST_FILE_TMP=${LOG_DIR}/ns_single_objects_${NAMESPACE}_grep.lst.tmp

TM_RAMCACHE_ACTION=/nkn/nvsd/namespace/actions/get_ramcache_list

if [ "$SEARCH_DOMAIN" != "all" ]; then
	MOUNT_POINT=${SEARCH_DOMAIN}:${SEARCH_PORT}
else
	MOUNT_POINT=*
fi

if [ "$OBJ_PATTERN" != "all" ]; then
	URI_PATTERN=`echo "\"$OBJ_PATTERN\"" | sed '/*/!d'`
	if [ -z $URI_PATTERN ]; then

		#Handle special characters such as ^,[
		OBJ_PATTERN=`echo $OBJ_PATTERN | awk '{gsub(/\[/,"\\\[")}1'`
		OBJ_PATTERN=`echo $OBJ_PATTERN | awk '{gsub(/\^/,"\\\^")}1'`

		#Load the CACHE_DISK_LIB
		if [ -f $CACHE_DISK_LIB ]; then
			. $CACHE_DISK_LIB;
		else
			echo "Unable to load the Disk lib".
			exit 2;
		fi

		# Get the active disk list
		get_active_disks;

		#Get the object path alone
		OBJ_DIR=`dirname "$OBJ_PATTERN"`
		#Canonicalize the path
		OBJ_DIR=`readlink -m "$OBJ_DIR"`

		#Get ramcache list and search
		/opt/tms/bin/mdreq action ${TM_RAMCACHE_ACTION} namespace string ${NAMESPACE} uid string ${NS_UID} filename string ${SINGLE_OBJECT_LIST_FILE}
		cat "${SINGLE_OBJECT_LIST_FILE}" | awk 'NR < 7' > "${GREPED_SINGLE_OBJECT_LIST_FILE}"

		echo "*  Loc   Size (KB)          Expiry                             URL" >> "${GREPED_SINGLE_OBJECT_LIST_FILE}";
		echo "*---------------------------------------------------------------------------------------" >>"${GREPED_SINGLE_OBJECT_LIST_FILE}";

		for i in $ACTIVE_DISKS
		do
			for j in /nkn/mnt/${i}${NS_UID}_${MOUNT_POINT}
			do
				CONTAINER=${j}${OBJ_DIR}
				if [ -d ${CONTAINER} ]; then
					/opt/nkn/bin/container_read -m -R "${CONTAINER}" >> "${SINGLE_OBJECT_LIST_FILE}"
				fi
			done
		done

		#Now grep our wanted items
		grep "${OBJ_PATTERN}$" "${SINGLE_OBJECT_LIST_FILE}" >> "${GREPED_SINGLE_OBJECT_LIST_FILE}"

		#If we have origin-server and port info grep for them in the list
		if [ "$MOUNT_POINT" != "*" ]; then
			cat "${GREPED_SINGLE_OBJECT_LIST_FILE}" | awk 'NR < 7' > "${GREPED_SINGLE_OBJECT_LIST_FILE_TMP}"
			grep -e "${MOUNT_POINT}/" "${GREPED_SINGLE_OBJECT_LIST_FILE}" >> "${GREPED_SINGLE_OBJECT_LIST_FILE_TMP}"
			mv "${GREPED_SINGLE_OBJECT_LIST_FILE_TMP}" "${GREPED_SINGLE_OBJECT_LIST_FILE}"
		fi

		#echo the greped output
		echo_list_head $GREPED_SINGLE_OBJECT_LIST_FILE;
		rm -f "${GREPED_SINGLE_OBJECT_LIST_FILE}"
		rm -f "${SINGLE_OBJECT_LIST_FILE}"
		exit 0;
	fi
fi

# Object list file name 
OBJECT_LIST_FILE=${LOG_DIR}/ns_objects_${NAMESPACE}.lst
GREPED_OBJECT_LIST_FILE=${LOG_DIR}/ns_objects_${NAMESPACE}.grep.lst

# Convert the PATTERN that is grep friendly 
# first made . as literal and then made the normal grep friendly pattern  [!!fix for bug 1737]
OBJ_PATTERN=`echo "$OBJ_PATTERN" | sed -e s'/\./\\\./'g | sed s@\*@\.\*@g`"$" 

# Now run the script that builds the whole object list 
nice ${BUILD_OBJECT_LIST_SCRIPT} ${NAMESPACE} &

# Just give a short pause and check for the log file
sleep 2;

# Now check if the list file exists if not just print "no objects"
if [ ! -f $OBJECT_LIST_FILE ]; then
	echo "${NAMESPACE} : No objects for this namespace"
	exit 1;
fi

# First check if request is for all objects and all domains as this is easy
if [ "$OBJ_PATTERN" = "all$" ]; then
	if [ "$SEARCH_DOMAIN" = "all" ]; then
		# We have to echo the head of the object list
		echo_list_head $OBJECT_LIST_FILE ;

		# Done 
		exit 0;
	else
		# Sleep an additional 4 sec if is a pattern search
		sleep 4;

		# Get the head part from the full list as is
		head -$HEADER_LINES $OBJECT_LIST_FILE > $GREPED_OBJECT_LIST_FILE ;
		grep -e "$MOUNT_POINT/" $OBJECT_LIST_FILE >> $GREPED_OBJECT_LIST_FILE ;

		# We have to echo the head of the object list
		echo_list_head $GREPED_OBJECT_LIST_FILE ;
		rm -f $GREPED_OBJECT_LIST_FILE ;

		if [ "$NUM_ENTRIES" -eq "0" ]; then
			echo "No match found"
			echo "Note : If there are a lot of objects in the cache then"
			echo "       the background task to get the object list might"
			echo "       still be running, hence check again in about 15 seconds"
		fi
	fi
else
	# Sleep an additional 4 sec if is a pattern search
	sleep 4;

	# Get the head part from the full list as is
	head -$HEADER_LINES $OBJECT_LIST_FILE > $GREPED_OBJECT_LIST_FILE ;
	if [ "$SEARCH_DOMAIN" != "all" ]; then
	    # If domain pattern is given, then grep on domain and then the object pattern.
               grep  -e "$MOUNT_POINT/" $OBJECT_LIST_FILE | grep "$OBJ_PATTERN" >> $GREPED_OBJECT_LIST_FILE ;
	else
	       grep "$OBJ_PATTERN" $OBJECT_LIST_FILE  >> $GREPED_OBJECT_LIST_FILE ;
	fi


	# We have to echo the head of the object list
	echo_list_head $GREPED_OBJECT_LIST_FILE ;
	rm -f $GREPED_OBJECT_LIST_FILE ;

	if [ "$NUM_ENTRIES" -eq "0" ]; then
		echo "No match found"
		echo "Note : If there are a lot of objects in the cache then"
		echo "       the background task to get the object list might"
		echo "       still be running, hence check again in about 10 seconds"
	fi
fi

#
# End of cache_mgmt_ls.sh
#
