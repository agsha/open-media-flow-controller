#! /bin/bash

usage() {
  echo "cache_add.sh [-ehl:m:o:p:rD:H:PS:] <full path to file> <URI name>"
  echo "  -e: option to end the PUTs for an URI"
  echo "  -h: help"
  echo "  -l <length>: byte len of segment: 32KB aligned"
  echo "  -m <max-age>: (seconds from now)"
  echo "  -o <offset>: byte off into file: 32KB aligned"
  echo "  -p <provider>: Put object in specific tier"
  echo "  -r delete the original file specified in <full path to file> after successful ingest; default is to preserve" 
  echo "  -D <namespace domain name>"
  echo "  -E <expiry time (sec): offset from NOW>"
  echo "  -H <Host string>"
  echo "  -P: Pin object"
  echo "  -S <start time (sec): offset from NOW>"
  echo "Notes:"
  echo "  * Options must be passed one letter per '-'"
  echo "  * If length option used, then offset option must also be used"
  echo "  * If option 'e' is used, offset and length wil be ignored" 
  echo "Providers:"
  echo "  1: SSD"
  echo "  5: SAS"
  echo "  6: SATA"
  exit 1
}

if [ $# -lt 2 ]; then
  usage;
fi

SHIFTCNT=0
while getopts "eh:l:m:o:p:rD:E:H:PS:" options; do
  case $options in
    e ) END_PUT=1
	(( SHIFTCNT++ ))
	;;
    h ) usage
	;;
    l ) LENGTH=$OPTARG
	(( SHIFTCNT += 2 ))
	;;
    m ) MAXAGE=$OPTARG
	(( SHIFTCNT += 2 ))
	;;
    o ) OFFSET=$OPTARG
	(( SHIFTCNT += 2 ))
	;;
    p ) PROVIDER=$OPTARG
	(( SHIFTCNT += 2 ))
	;;
    r ) REMOVE_ORIG_FILE=1
	(( SHIFTCNT++ ))
	;;
    D ) DOMAIN=$OPTARG
	(( SHIFTCNT += 2 ))
	;;
    E ) EXPIRY_OFFSET=$OPTARG
	(( SHIFTCNT += 2 ))
	;;
    H ) HOST=$OPTARG
	(( SHIFTCNT += 2 ))
	;;
    P ) CACHE_PIN=1
	(( SHIFTCNT++ ))
	;;
    S ) START_TIME_OFFSET=$OPTARG
	(( SHIFTCNT += 2 ))
	;;
    \? ) usage
	;;
    * ) usage
	;;
  esac
done

if [ "${PROVIDER}" != "1" -a "${PROVIDER}" != "5" -a "${PROVIDER}" != "6" \
    -a "${PROVIDER}" != "" ]; then
  echo "Provider must be 1, 5, or 6"
  usage
fi
if [ "${LENGTH}" != "" -a "${OFFSET}" == "" ]; then
  echo "If length given, then offset must also be given"
  usage
fi
# SSD, SAS, or SATA
if [ "${PROVIDER}" == "1" -o "${PROVIDER}" == "5" -o \
	"${PROVIDER}" == "6" -o "${PROVIDER}" == "" ]; then
  if [ "${OFFSET}" != "" ]; then
    if [ $[${OFFSET} % (32*1024) ] != 0 ]; then
      echo "offset must be multiple of 32KB"
      usage
    fi
  fi
  if [ "${LENGTH}" != "" ]; then
    if [ $[${LENGTH} % (32*1024) ] != 0 ]; then
      echo "length must be multiple of 32KB"
      usage
    fi
  fi
fi
if [ "${EXPIRY_OFFSET}" != "" ]; then
  if [ "${EXPIRY_OFFSET}" -lt -1 ]; then
    echo "Offset in seconds from now must be >= -1"
    echo "-1 means no end time"
    usage
  fi
fi
if [ "${START_TIME_OFFSET}" != "" ]; then
  if [ "${START_TIME_OFFSET}" -lt 0 ]; then
    echo "Offset in seconds from NOW must be >= 0"
    usage
  fi
fi

while [ $SHIFTCNT -gt 0 ]; do
  shift;
  (( SHIFTCNT-- ))
done


FILE=$1
URL=$2
CL=`wc -m $FILE | awk '{print $1}'`
QF=/nkn/tmp/tmp.q
mkdir -p /nkn/tmp
touch $QF
ret=1

ARGLIST="-i ${QF} -p $FILE -u $URL -H Content-Length:${CL}"
if [ "${REMOVE_ORIG_FILE}" == "" ]; then
   ARGLIST="${ARGLIST} -n"
fi
if [ "${DOMAIN}" != "" ]; then
   ARGLIST="${ARGLIST} -N namespace_domain:${DOMAIN}"
fi
if [ "${HOST}" != "" ]; then
   ARGLIST="${ARGLIST} -H Host:${HOST}"
fi
if [ "${MAXAGE}" != "" ]; then
   ARGLIST="${ARGLIST} -H Cache-Control:max-age=${MAXAGE}"
fi
if [ "${LENGTH}" != "" ]; then
   ARGLIST="${ARGLIST} -N segment_length:${LENGTH}"
fi
if [ "${OFFSET}" != "" ]; then
   ARGLIST="${ARGLIST} -N segment_offset:${OFFSET}"
fi
if [ "${PROVIDER}" != "" ]; then
   ARGLIST="${ARGLIST} -N provider_type:${PROVIDER}"
fi
if [ "${END_PUT}" != "" ]; then
   ARGLIST="${ARGLIST} -N end_put:${END_PUT}"
fi
if [ "${CACHE_PIN}" != "" ]; then
   ARGLIST="${ARGLIST} -N cache_pin:${CACHE_PIN}"
fi
if [ "${EXPIRY_OFFSET}" != "" ]; then
   ARGLIST="${ARGLIST} -N expiry_offset:${EXPIRY_OFFSET}"
fi
if [ "${START_TIME_OFFSET}" != "" ]; then
   ARGLIST="${ARGLIST} -N start_time_offset:${START_TIME_OFFSET}"
fi

while [ $ret -ne 0 ]; do
  /opt/nkn/bin/nknfqueue ${ARGLIST}
  ret=$?
  if [ $ret -ne 0 ]; then
    sleep 1
  fi
done
ret=1
while [ $ret -ne 0 ]; do
  /opt/nkn/bin/nknfqueue -e $QF -q /tmp/FMGR.queue
  ret=$?
  if [ $ret -ne 0 ]; then
    sleep 1;
  fi
done
echo $URL
rm -f $QF
