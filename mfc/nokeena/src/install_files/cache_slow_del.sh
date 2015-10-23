#! /bin/bash
#
#	File : cache_slow_del.sh
#
#	Modified by : Ramanand Narayanan 
#	Date : April 30th, 2009
#
MAX_PER_ITER=500
PAUSE_TIME=20
NAMESPACE="";
if [ $# -lt 3 ]; then
  echo "Usage: cache_slow_del <domain> <UID> <list file> "
  echo 
  exit 0
fi

# Get the domain from the parameter
NS_DOMAIN=$1;
shift;

# Get the UID from the parameter
NS_UID=$1;
shift;

# Get the filename
LOG_FILE=$1;
shift;
# Get the namespace if provided
#BUG:7154:It is mainly used for logging the cache deletion start
#and completion for a given namespace.
NAMESPACE=$1;
shift;

# Get the number of entries
NUM_ENTRIES=`wc -l $LOG_FILE | cut -f1 -d' '`

if [ "$NS_DOMAIN" = "*" ]; then
	NS_DOMAIN=any
fi

# Sanity check 
if [ ! -f $LOG_FILE ]; then 
	logger "[Cache Deletion] Error: List file missing for Domain $NS_DOMAIN"
	exit 1;
fi
if [ "$NAMESPACE" != "" ]; then
	logger "[Cache Deletion] Starting for Namespace: $NAMESPACE";
fi
logger "[Cache Deletion] Starting for Domain $NS_DOMAIN";
logger "[Cache Deletion] Number of entries : $NUM_ENTRIES";

COUNT=0;
# Based on the domain call cache_del.sh with suitable parameters
IFS=$'\t\n'
while read object
do
	#Queue the delete request
	RETVAL=1
	while [ $RETVAL -ne 0 ]; do
		/opt/nkn/bin/nknfqueue -f -r "${NS_UID}_$object" -q /tmp/FMGR.queue
		RETVAL=$?

		# If queue is full wait and retry
		if [ $RETVAL -ne 0 ]; then
			sleep 10;
		fi
	done
	COUNT=$(($COUNT+1))

	if [ $COUNT -ge $MAX_PER_ITER ]; then
		logger "[Cache Deletion] $MAX_PER_ITER entries queued for deletion";
		COUNT=0;
		sleep $PAUSE_TIME;
	fi
done < $LOG_FILE

# Remove the delete list file
rm -f $LOG_FILE ;

IFS=$' \t\n'
logger "[Cache Deletion] Completed for Domain: $NS_DOMAIN";
if [ "$NAMESPACE" != "" ]; then
	logger "[Cache Deletion] Completed for Namespace: $NAMESPACE";
fi

exit 0;
