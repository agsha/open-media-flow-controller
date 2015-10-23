#! /bin/bash

LOG_FILE=/nkn/tmp/cache_evict.log.$$

usage() {
  echo "Usage: cache_evict.sh -e <cache-types: eg:1,5,6> -f <transaction-flag> -t <hotness> URI_ARG"
  echo "where, URI_ARG is 'A' || 'B' with" 
  echo "        A: '-F <URI list file>'"
  echo "        B: <full URI name>"
  echo "cache types: 1: SSD, 5: SAS, 6: SATA"
  echo "transaction flag: 1: start, 2: stop"
  echo "temperature: <uint64_t>"
  echo "URI list file: file that contains the URIs to be evicted"
  exit 1
}

if [ $# -lt 1 ]; then
  usage
fi

while getopts ":e:f:F:t:" options; do
  case $options in
    e ) EVICT_CACHE=$OPTARG
	;;
    f ) TRANS=$OPTARG
	;;
    t ) HOTNESS=$OPTARG
	;;
    F )	DEL_LIST=1
	;;
    h ) usage
	;;
    \? ) usage
	;;
    * ) usage
	;;
  esac
done

if [ "${EVICT_CACHE}" != "" ]; then
  shift;shift;
fi
if [ "$EVICT_CACHE" == "" ]; then
  EVICT_CACHE=0
fi

if [ "${TRANS}" != "" ]; then
  shift;shift;
fi
if [ "$TRANS" == "" ]; then
  TRANS=1
fi

if [ "${HOTNESS}" != "" ]; then
  shift;shift;
fi
if [ "$HOTNESS" == "" ]; then
  HOTNESS=0
fi

if [ "${DEL_LIST}" != "" ]; then
  shift;
fi

DOMAIN="any"
URI=$1

# if eviction is disabled, exit here
if [ "$DEL_LIST" != "1" ]; then
    if [ "${TRANS}" = "1" ]; then
	rm -f $LOG_FILE
    fi

    if [ -e /config/nkn/disk_cache_eviction_disable ]; then
	DATE=`date`
	echo "${DATE}: ${URI} ${HOTNESS}" >> $LOG_FILE
	exit
    fi
else
    if [ -e /config/nkn/disk_cache_eviction_disable ]; then
	exit
    fi
fi

# if $DEL_LIST is 1, $URI will point a file that lists the objects to be deleted
# the object list file is passed to nknfqueue as an attribute using the
# 'del_uri_file' token. the arg to 'u' option is a special object "/tmp/ignore"
# which nvsd will ignore.
# if $DEL_LIST is 0, $URI will be the actual object to be deleted.

ARGLIST="-i ttt1 -N evict_cache:${EVICT_CACHE} -N remove_object:1"

if [ "$DEL_LIST" != "1" ]; then
  # object to be evicted is passed as an argument
  ARGLIST="${ARGLIST} -N transflag:${TRANS} -N hotness:${HOTNESS} -u $URI"

else
  # objects to be evicted are in a file
  ARGLIST="${ARGLIST} -N del_uri_file:$URI -u /tmp/ignore"
fi

# the 'n' option instructs nknfqueue to not delete the data file
# and the 'p' option provides the full path of the data file to nkfqueue.
# both these options are not useful for this script, but nknfqueue would 
# complain if they aren't provided.
ARGLIST="${ARGLIST} -n -p /etc/termcap"

# send it to the fqueue
/opt/nkn/bin/nknfqueue ${ARGLIST} 

ret=1
while [ $ret -ne 0 ]; do
  /opt/nkn/bin/nknfqueue -e ttt1 -q /tmp/FMGR.queue
  ret=$?
  if [ $ret -ne 0 ]; then
    sleep 15;
  fi
done
