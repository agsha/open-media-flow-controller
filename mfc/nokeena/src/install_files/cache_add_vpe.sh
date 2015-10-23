age() {
  echo "addcache.sh [-h] [-l] [-m] [-o] [-D] <full path to file> <URI name>"
  echo "  -h: help"
  echo "  -l <length>: byte len of segment: 32KB aligned"
  echo "  -m <max-age>: (minutes from now)"
  echo "  -o <offset>: byte off into file: 32KB aligned"
  echo "  -D <domain name>"
  echo "Notes:"
  echo "  * Options must be passed one letter per '-'"
  echo "  * If length option used, then offset option must also be used"
  echo "Providers:"
  echo "  1: SSD"
  echo "  5: SAS"
  echo "  6: SATA"
  exit 1
}

if [ $# -lt 2 ]; then
  usage;
fi

while getopts "h:l:m:o:p:D:" options; do
  case $options in
    h ) usage
        ;;
    l ) LENGTH=$OPTARG
        ;;
    m ) MAXAGE=$OPTARG
        ;;
    o ) OFFSET=$OPTARG
        ;;
    p ) PROVIDER=$OPTARG
        ;;
    D ) DOMAIN=$OPTARG
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
if [ "${MAXAGE}" != "" ]; then
  shift;shift;
fi
if [ "${DOMAIN}" != "" ]; then
  shift;shift;
fi
if [ "${LENGTH}" != "" ]; then
  shift;shift;
fi
if [ "${OFFSET}" != "" ]; then
  shift;shift;
fi
if [ "${PROVIDER}" != "" ]; then
  shift;shift;
fi

FILE=$1
URL=$2
CL=`wc -m $FILE | awk '{print $1}'`
QF=/nkn/tmp/tmp.q
mkdir -p /nkn/tmp
touch $QF
ret=1
ARGLIST="-i ${QF} -p $FILE -u $URL -H Content-Length:${CL}"
if [ "${DOMAIN}" != "" ]; then
   ARGLIST="${ARGLIST} namespace_domain:${DOMAIN}"
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

