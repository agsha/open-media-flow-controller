#! /bin/bash
#
#	File : cache_mgmt_del.sh
#
#	Modified by : Ramanand Narayanan 
#	Created Date : April 22st, 2009
#
#	Last Modified : Dec 1st, 2009
#
LOG_DIR=/var/log/nkn/cache
BUILD_OBJECT_LIST_SCRIPT=/opt/nkn/bin/mgmt_build_ns_object_list.py

#--------------------------------------------------------------
#
#  MAIN LOGIC BEGINS HERE 
#
#--------------------------------------------------------------

# Sanity test 
mkdir -p $LOG_DIR

if [ $# -lt 4 ]; then
  echo "Usage: cache_mgmt_del <namespace> <uid> <domain> <all|<pattern>> <all|<search domain>> [pin]"
  echo 
  exit 0
fi

# Get the NAMESPACE name 
NAMESPACE=$1;
shift;

# Get the UID for the namespace 
NS_UID=$1;
shift;

# Get the domain from the parameter
NS_DOMAIN=$1;
shift;

# Get the pattern
OBJ_PATTERN=$1;
shift;

# Get the domain to search 
SEARCH_DOMAIN=$1;
shift;

# Get if its to purge pinned objects
PINNED=$1;
shift;

# Object list file name 
OBJECT_LIST_FILE=${LOG_DIR}/ns_objects_${NAMESPACE}.lst
OBJECT_PIN_LIST_FILE=${LOG_DIR}/ns_objects_${NAMESPACE}.lst.pin
DELETE_OBJECT_LIST_FILE=${LOG_DIR}/ns_objects_${NAMESPACE}_delete.lst
DELETE_OBJECT_LIST_FILE_TMP=${LOG_DIR}/ns_objects_${NAMESPACE}_delete.lst.tmp

# Check if a delete is already in progress
if [ -f $DELETE_OBJECT_LIST_FILE ]; then
	echo "$NAMESPACE : Deletion of objects for this namespace already in progress"
	echo "Issue this command only after the earlier one has completed"
	echo "Check log for status of deletion"
	
	exit 1;
fi

# Single URI Delete
# Go through the disks based on UID passed and get the originserver:port
# Append uri with originserver:port and call nknfqueue

if [ "$SEARCH_DOMAIN" != "all" ]; then
        MOUNT_POINT="/nkn/mnt/dc_*/${NS_UID}_${SEARCH_DOMAIN}:*"
else
        MOUNT_POINT="/nkn/mnt/dc_*/${NS_UID}_*"
fi

if [ "$OBJ_PATTERN" != "all" ]; then
	URI_PATTERN=`echo "\"$OBJ_PATTERN\"" | sed '/*/!d'`
	if [ -z $URI_PATTERN ]; then
		for i in $MOUNT_POINT
		do
			OBJ_DIR=`dirname "$OBJ_PATTERN"`
			CONTAINER=`echo $i | sed 's/^.*dc_.*\//\//'`
			if [ -d ${i}${OBJ_DIR} ]; then
				CONTAINER=${CONTAINER}${OBJ_PATTERN}
				/opt/nkn/bin/nknfqueue -f -r "${CONTAINER}" -q /tmp/FMGR.queue
				FOUND="true"
			fi
		done
		if [ "$FOUND" == "true" ]; then
			echo "Object queued for deletion"
			echo "Please run the object list command for the URI"
		else
			echo "Object not deleted"
		fi
		exit 0;
	fi
fi

# Convert the PATTERN that is grep friendly 
# first made . as literal and then made the normal grep friendly pattern  [!!fix for bug 1737]
OBJ_PATTERN=`echo "$OBJ_PATTERN" | sed -e s'/\./\\\./'g | sed s@\*@\.\*@g`"$" 

# Now run the script that builds the whole object list 
nice ${BUILD_OBJECT_LIST_SCRIPT} ${NAMESPACE} &

# Just give a short pause and check for the log file
sleep 2;

# Now check if the list file exists if not just print "no objects"
if [ ! -f $OBJECT_LIST_FILE ]; then
	echo "${NAMESPACE} : No objects to delete for this namespace"
	exit 1;
fi

# Psuedo lock so that multiple deletes are not kicked in
touch $DELETE_OBJECT_LIST_FILE ;

#if its a purge all,get from the pin file lst
if [ "$PINNED" = "pin" ]; then

	grep -e "  dc_[0-9]* *Y" ${OBJECT_LIST_FILE}  > ${OBJECT_PIN_LIST_FILE}
	grep "^ " $OBJECT_PIN_LIST_FILE  | cut -c54- > $DELETE_OBJECT_LIST_FILE
	rm -f $OBJECT_PIN_LIST_FILE	
else	
	# Skip the unwanted lines and get only the URI part from the object list 
	#Reverting the cut number from c54 to c47 since cache pinning is not supported in R2.1
#	grep "^ " $OBJECT_LIST_FILE  | cut -c47- > $DELETE_OBJECT_LIST_FILE_TMP
	#Increasing the cut value Refer bug: 8344
	grep "^ " $OBJECT_LIST_FILE  | cut -c54- > $DELETE_OBJECT_LIST_FILE_TMP
fi

#Pattern search all the stuff only applicable only for normal objet delete
if [ "$PINNED" != "pin" ]; then
	# First check if request is for all objects and all domains as this is easy
	if [ "$OBJ_PATTERN" = "all$" ]; then
		if [ "$SEARCH_DOMAIN" = "all" ]; then
			# Grep for only the object entries and create the list
			sort $DELETE_OBJECT_LIST_FILE_TMP | uniq > $DELETE_OBJECT_LIST_FILE;
		else
			sort $DELETE_OBJECT_LIST_FILE_TMP | uniq | grep $SEARCH_DOMAIN > $DELETE_OBJECT_LIST_FILE;
		fi
	else
		sort $DELETE_OBJECT_LIST_FILE_TMP | uniq | grep "$OBJ_PATTERN" > $DELETE_OBJECT_LIST_FILE;
	fi
fi

# Remove the temporary file
rm -f $DELETE_OBJECT_LIST_FILE_TMP;

# Now for each entry in the file call the cache_slow_del.sh
NUM_ENTRIES=`wc -l $DELETE_OBJECT_LIST_FILE | cut -f1 -d' '`

if [ "0" -eq "$NUM_ENTRIES" ]; then
	echo "$NAMESPACE : No objects found, hence nothing to delete";
	rm -f  $DELETE_OBJECT_LIST_FILE ;
else
	echo "$NAMESPACE : $NUM_ENTRIES object(s) being deleted "
	echo 
	echo "Check log for status of deletion"
	echo "Note : Same object in multiple tiers are treated as one object for deletion"
	echo "Warning : Deletion happens by blocks hence neighbouring objects could also get deleted"

	# Call the slow delete script
	nohup /opt/nkn/bin/cache_slow_del.sh "$SEARCH_DOMAIN" $NS_UID $DELETE_OBJECT_LIST_FILE "$NAMESPACE" 2> /dev/null &
fi

exit 0;

#
# End of cache_mgmt_del.sh
#

